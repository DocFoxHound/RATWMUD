// Circles (Docs/Design/50-player-card-friends-safety.md, Phase 5; agreed, doc 48 §3.4): out-of-character groups of
// accounts (a roleplay group, friends, a team), apart from Chapters, which stay the in-character organisation. A keeper
// and officers invite (by handle) and remove; anyone may leave; the last to leave ends it. Its chat reaches every member
// who is in the world, whatever wolf they're on, named by handles; its roster shows a member's wolf only where they
// share it with the circle; officers plan nights. Blocks hold: no invitation between a blocked pair, and a muted or
// blocked member's lines never reach the one who muted or blocked them. The limits are in Data/Social/profile.json.
#include "RatwGame.h"

#include <algorithm>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr double NightKeptSeconds = 86400;           // A night drops off a day after it was.
} // namespace

std::vector<std::string> Game::circlesOf(const std::string& account) const
{
    std::vector<std::string> ids;
    for (const auto& [id, c] : circles_)
        if (c.members.count(account))
            ids.push_back(id);
    return ids;
}

void Game::sendCircles(const std::string& account)
{
    // One account's circles, as it may see them: each with its roster (a member's wolf only where they share it with
    // the circle), nights in order, the invitations still waiting (for officers), and invitations to this account.
    auto* c = onlineClient(account);
    if (!c)
        return;
    auto e = Value::object();
    e.add("type", "circles");
    auto list = Value::array();
    auto invites = Value::array();
    for (const auto& [id, circle] : circles_)
    {
        if (const auto inv = circle.invited.find(account); inv != circle.invited.end())
        {
            auto row = Value::object();
            row.add("circle", id);
            row.add("name", circle.name);
            row.add("from", handleOf(inv->second.first));
            row.add("at", inv->second.second);
            invites.push(row);
        }
        const auto mine = circle.members.find(account);
        if (mine == circle.members.end())
            continue;
        auto o = Value::object();
        o.add("id", id);
        o.add("name", circle.name);
        o.add("role", mine->second.role);
        o.add("shares", mine->second.shares);
        auto roster = Value::array();
        std::vector<std::pair<std::string, const people::CircleMember*>> sorted;
        for (const auto& [who, m] : circle.members)
            sorted.push_back({who, &m});
        const auto rank = [](const std::string& role) { return role == "keeper" ? 0 : role == "officer" ? 1 : 2; };
        std::sort(sorted.begin(), sorted.end(), [&](const auto& a, const auto& b) {
            const bool ah = onlineClient(a.first), bh = onlineClient(b.first);
            return ah != bh ? ah : rank(a.second->role) != rank(b.second->role) ? rank(a.second->role) < rank(b.second->role)
                                                                             : people::handleKey(handleOf(a.first)) < people::handleKey(handleOf(b.first));
        });
        for (const auto& [who, m] : sorted)
        {
            auto row = Value::object();
            row.add("handle", handleOf(who));
            row.add("role", m->role);
            const auto* there = onlineClient(who);
            row.add("online", there != nullptr);
            if (there && m->shares)
                if (const auto* wolf = world_.entity(there->entityId))
                    row.add("character", wolf->name);     // (Shared with this circle: their true name.)
            if (who == account)
                row.add("you", true);
            roster.push(row);
        }
        o.add("members", roster);
        if (mine->second.role != "member")
        {
            auto waiting = Value::array();
            for (const auto& [who, by] : circle.invited)
                waiting.push(handleOf(who));
            o.add("invited", waiting);
        }
        auto nights = Value::array();
        for (const auto& n : circle.nights)
        {
            auto row = Value::object();
            row.add("id", n.id);
            row.add("at", n.at);
            row.add("place", n.place);
            row.add("line", n.line);
            row.add("by", handleOf(n.by));
            nights.push(row);
        }
        o.add("nights", nights);
        list.push(o);
    }
    e.add("circles", list);
    e.add("invites", invites);
    send(c, e);
}

void Game::circleChanged(const people::Circle& circle)
{
    // Every member in the world sees the change (and anyone with an invitation waiting).
    for (const auto& [who, m] : circle.members)
        sendCircles(who);
    for (const auto& [who, by] : circle.invited)
        sendCircles(who);
}

bool Game::circleCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"type": "circle", "verb": "create" | "invite" | "accept" | "decline" | "leave" | "remove" | "officer" | "night" |
    //  "unnight" | "share" | "disband" | "list", "circle": id, "name", "handle", "on", "at", "place", "line", "night"}.
    const auto mine = accountKey(c);
    const auto verb = j.string("verb");
    if (c->entityId.empty() || mine.empty())
        return false;
    const auto& r = people::rules();
    if (verb == "list")
    {
        sendCircles(mine);
        result = {true, {}, {}};
        return true;
    }
    if (verb == "create")
    {
        std::string name, why;
        if (handleOf(mine).empty())
            result = {false, "Choose a handle first: circles know you by it.", {}};
        else if (!people::validCircleName(j.string("name"), name, why))
            result = {false, why, {}};
        else if (int(circlesOf(mine).size()) >= r.circlesPerAccount)
            result = {false, "You are in " + std::to_string(r.circlesPerAccount) + " circles already.", {}};
        else if (std::any_of(circles_.begin(), circles_.end(), [&](const auto& kv) { return people::handleKey(kv.second.name) == people::handleKey(name); }))
            result = {false, "There is a circle by that name already.", {}};
        else
        {
            people::Circle circle;
            circle.id = "circle-" + guid().substr(0, 16);
            circle.name = name;
            circle.created = now();
            circle.members[mine] = {"keeper", r.circleShareByDefault, now()};
            circles_[circle.id] = circle;
            sendCircles(mine);
            result = {true, "The circle " + name + " is made. Invite others by handle.", {}};
        }
        if (result.ok)
            saveSoon();
        return true;
    }
    const auto it = circles_.find(j.string("circle"));
    if (it == circles_.end())
    {
        result = {false, "No such circle.", {}};
        return true;
    }
    auto& circle = it->second;
    const auto me = circle.members.find(mine);
    const auto role = me == circle.members.end() ? std::string() : me->second.role;
    const bool officer = role == "keeper" || role == "officer";
    const auto other = accountByHandle(j.string("handle"));
    const auto theirHandle = handleOf(other);
    if (verb == "accept" || verb == "decline")
    {
        const auto inv = circle.invited.find(mine);
        if (inv == circle.invited.end())
            result = {false, "No invitation to that circle is waiting.", {}};
        else if (verb == "decline")
        {
            circle.invited.erase(inv);                   // (The officer who asked isn't told.)
            result = {true, "Declined.", {}};
        }
        else if (int(circlesOf(mine).size()) >= r.circlesPerAccount)
            result = {false, "You are in " + std::to_string(r.circlesPerAccount) + " circles already.", {}};
        else if (int(circle.members.size()) >= r.circleMembers)
            result = {false, circle.name + " is full.", {}};
        else
        {
            circle.invited.erase(inv);
            circle.members[mine] = {"member", r.circleShareByDefault, now()};
            result = {true, "You join " + circle.name + ".", {}};
        }
        sendCircles(mine);
    }
    else if (me == circle.members.end())
    {
        result = {false, "You aren't in that circle.", {}};
        return true;
    }
    else if (verb == "invite")
    {
        // (Someone blocked either way gets the refusal an unknown handle gets: doc 50, 7.)
        if (!officer)
            result = {false, "Only its keeper and officers invite.", {}};
        else if (other.empty() || theirHandle.empty() || blockedAccounts(mine, other))
            result = {false, "No one by that handle can be invited.", {}};
        else if (circle.members.count(other))
            result = {false, theirHandle + " is in the circle already.", {}};
        else if (circle.invited.count(other))
            result = {true, theirHandle + " has an invitation waiting.", {}};
        else if (int(circle.members.size() + circle.invited.size()) >= r.circleMembers)
            result = {false, circle.name + " is full.", {}};
        else
        {
            circle.invited[other] = {mine, now()};
            sendCircles(other);
            if (auto* there = onlineClient(other))
                system(there, handleOf(mine) + " invites you to the circle " + circle.name + ": see FRIENDS, CIRCLES.");
            result = {true, "Invited " + theirHandle + ".", {}};
        }
    }
    else if (verb == "remove")
    {
        const auto them = circle.members.find(other);
        if (!officer)
            result = {false, "Only its keeper and officers remove members.", {}};
        else if (them == circle.members.end())
        {
            // An invitation still waiting can be withdrawn.
            result = circle.invited.erase(other) ? Result{true, "Invitation withdrawn.", {}} : Result{false, "They aren't in the circle.", {}};
            sendCircles(other);
        }
        else if (other == mine || them->second.role == "keeper" || (them->second.role == "officer" && role != "keeper"))
            result = {false, "You can't remove them.", {}};
        else
        {
            circle.members.erase(them);
            sendCircles(other);
            result = {true, "Removed " + theirHandle + " from " + circle.name + ".", {}};
        }
    }
    else if (verb == "officer")
    {
        const auto them = circle.members.find(other);
        if (role != "keeper")
            result = {false, "Only its keeper names officers.", {}};
        else if (them == circle.members.end() || other == mine)
            result = {false, "They aren't a member.", {}};
        else
        {
            them->second.role = j.boolean("on") ? "officer" : "member";
            result = {true, theirHandle + (j.boolean("on") ? " is an officer now." : " is a member again."), {}};
        }
    }
    else if (verb == "share")
    {
        me->second.shares = j.boolean("on");
        result = {true, me->second.shares ? circle.name + " can see which wolf you're playing." : circle.name + " no longer sees which wolf you're playing.", {}};
    }
    else if (verb == "night")
    {
        const double at = j.number("at");
        const auto place = people::clean(j.string("place"), std::size_t(r.nightPlace));
        const auto line = people::clean(j.string("line"), std::size_t(r.nightLine));
        if (!officer)
            result = {false, "Only its keeper and officers plan nights.", {}};
        else if (at < now() - 60 || at > now() + 366 * 86400.0)
            result = {false, "Choose a time within the coming year.", {}};
        else if (place.empty())
            result = {false, "Say where.", {}};
        else if (int(circle.nights.size()) >= r.circleNights)
            result = {false, "The circle has " + std::to_string(r.circleNights) + " nights planned already.", {}};
        else
        {
            circle.nights.push_back({"night-" + guid().substr(0, 12), place, line, mine, at});
            std::sort(circle.nights.begin(), circle.nights.end(), [](const auto& a, const auto& b) { return a.at < b.at; });
            result = {true, "A night is planned at " + place + ".", {}};
        }
    }
    else if (verb == "unnight")
    {
        const auto before = circle.nights.size();
        if (officer)
            circle.nights.erase(std::remove_if(circle.nights.begin(), circle.nights.end(), [&](const auto& n) { return n.id == j.string("night"); }),
                                circle.nights.end());
        result = !officer ? Result{false, "Only its keeper and officers plan nights.", {}}
                 : circle.nights.size() < before ? Result{true, "The night is taken off.", {}}
                                                 : Result{false, "No such night.", {}};
    }
    else if (verb == "leave" || verb == "disband")
    {
        if (verb == "disband" && role != "keeper")
        {
            result = {false, "Only its keeper ends a circle.", {}};
            return true;
        }
        const auto name = circle.name;
        auto everyone = circle;                          // (To tell them, after.)
        if (verb == "disband")
            circle.members.clear();
        else
        {
            circle.members.erase(me);
            // A keeper who leaves hands the circle on: to the longest-standing officer, else the longest-standing member.
            if (role == "keeper" && !circle.members.empty())
            {
                auto next = circle.members.end();
                for (auto m = circle.members.begin(); m != circle.members.end(); ++m)
                    if (next == circle.members.end() || (m->second.role == "officer") > (next->second.role == "officer") ||
                        ((m->second.role == "officer") == (next->second.role == "officer") && m->second.joined < next->second.joined))
                        next = m;
                next->second.role = "keeper";
            }
        }
        result = {true, verb == "disband" ? "The circle " + name + " is ended." : "You leave " + name + ".", {}};
        if (circle.members.empty())
            circles_.erase(it);                          // (The last to leave ends it.)
        circleChanged(everyone);
        saveSoon();
        return true;
    }
    else
        return false;
    if (result.ok)
    {
        saveSoon();
        circleChanged(circle);
    }
    else
        sendCircles(mine);
    return true;
}

Result Game::circleLine(Connection* c, const std::string& circleId, const std::string& written)
{
    // Out of character, to every member in the world (doc 50, 6): named by handle; never to one who muted or blocked
    // the speaker; no scene credit.
    const auto mine = accountKey(c);
    const auto it = circles_.find(circleId);
    if (it == circles_.end() || !it->second.members.count(mine))
        return {false, "You aren't in that circle.", {}};
    const auto text = people::clean(written, std::size_t(people::rules().messageMost) + 1, true);
    if (text.empty())
        return {false, "Write something first.", {}};
    if (people::clean(written, std::size_t(people::rules().messageMost), true).size() < text.size())
        return {false, "A line is at most " + std::to_string(people::rules().messageMost) + " characters.", {}};
    auto e = Value::object();
    e.add("type", "ooc");
    e.add("channel", "circle");
    e.add("circle", circleId);
    e.add("circleName", it->second.name);
    e.add("sequence", sequence_++);
    e.add("speaker", handleOf(mine));
    e.add("text", text);
    e.add("at", now());
    for (const auto& [who, m] : it->second.members)
        if (auto* there = onlineClient(who))
        {
            if (there != c && hides(there->entityId, c->entityId))
                continue;                                // (Muted or blocked by them: never delivered.)
            e.set("outgoing", there == c);
            send(there, e);
            if (there != c)
                heardLine(there->entityId, std::uint64_t(e.number("sequence")), c->entityId, "circle", text);
        }
    return {true, {}, {}};
}

void Game::tendCircles()
{
    // Invitations past 14 days, and nights a day past, go.
    const double invites = now() - people::rules().circleInviteDays * 86400.0, nights = now() - NightKeptSeconds;
    for (auto& [id, circle] : circles_)
    {
        bool changed = false;
        for (auto inv = circle.invited.begin(); inv != circle.invited.end();)
            if (inv->second.second < invites)
            {
                inv = circle.invited.erase(inv);
                changed = true;
            }
            else
                ++inv;
        const auto before = circle.nights.size();
        circle.nights.erase(std::remove_if(circle.nights.begin(), circle.nights.end(), [&](const auto& n) { return n.at < nights; }),
                            circle.nights.end());
        if (changed || circle.nights.size() != before)
            circleChanged(circle);
    }
}

void Game::circlesSave(json::Value& root) const
{
    auto list = Value::array();
    for (const auto& [id, circle] : circles_)
        list.push(people::saveCircle(circle));
    root.add("circles", list);
}

void Game::circlesLoad(const json::Value& saved)
{
    // Only members and invitations of accounts the game knows; a circle with no one left isn't kept.
    circles_.clear();
    const auto known = [&](const std::string& account) {
        return !account.empty() && (accounts_.exists(account) || account.rfind("dev:", 0) == 0);
    };
    for (const auto& e : saved.array("circles"))
    {
        auto circle = people::loadCircle(e);
        for (auto m = circle.members.begin(); m != circle.members.end();)
            m = known(m->first) ? std::next(m) : circle.members.erase(m);
        for (auto inv = circle.invited.begin(); inv != circle.invited.end();)
            inv = known(inv->first) ? std::next(inv) : circle.invited.erase(inv);
        if (circle.id.empty() || circle.name.empty() || circle.members.empty())
            continue;
        if (std::none_of(circle.members.begin(), circle.members.end(), [](const auto& m) { return m.second.role == "keeper"; }))
            circle.members.begin()->second.role = "keeper";
        circles_[circle.id] = std::move(circle);
    }
}
} // namespace ratw::game
