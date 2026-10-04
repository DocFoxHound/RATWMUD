// Parties as players make and see them (Docs/Design/32-parties-chapters-factions.md, Part 2; the rules are
// RatwParty.cpp): the party commands, the party's chat out of character, members pulled into a party mate's fight
// (Docs/Design/33-combat.md, "Party auto-join"), and who is a party mate or hostile in each player's view.
#include "RatwGame.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

std::string Game::nameOf(const std::string& id) const
{
    if (const auto* e = world_.entity(id))
        return e->name;
    const auto found = characters_.find(id);
    return found == characters_.end() ? std::string("someone") : found->second.name;
}

void Game::tellParty(const std::vector<std::string>& members, const std::string& words, const std::string& except)
{
    for (const auto& m : members)
        if (m != except)
            if (auto* c = clientOf(m))
                system(c, words);
}

void Game::tellParty(const std::string& partyId, const std::string& words, const std::string& except)
{
    if (const auto* p = parties_.byId(partyId))
        tellParty(p->members, words, except);
}

Result Game::partyInvite(const std::string& from, const std::string& to)
{
    const auto* them = world_.entity(to);
    // Only a wolf you can see, and only a player: companions who are NPCs come with Phase 3 (doc 32).
    if (!them || them->npc || them->dead || (to != from && world_.visionClarity(from, to) <= 0))
        return {false, "You can only invite a wolf you can see.", to};
    if (!clientOf(to))
        return {false, "They are not here to answer.", to};
    const auto outcome = parties_.invite(from, to, now());
    if (!outcome.ok)
        return {false, outcome.message, to};
    if (auto* c = clientOf(to))
        system(c, nameOf(from) + " invites you to join their party. Accept or decline in the Party panel.");
    return {true, "You invite " + nameOf(to) + " to your party.", to};
}

bool Game::partyCommand(Connection* c, const Value& j, Result& result)
{
    const std::string id = c->entityId, verb = j.string("verb"), target = j.string("target");
    // Who was in it before: a party of two that loses one is gone, but the one left is still told.
    const auto* before = parties_.of(id);
    const std::vector<std::string> partyBefore = before ? before->members : std::vector<std::string>{};
    if (verb == "invite")
        result = partyInvite(id, target);
    else if (verb == "accept")
    {
        const auto* waiting = parties_.inviteFor(id, now());
        const std::string from = waiting ? waiting->from : std::string();
        const auto outcome = parties_.accept(id, now());
        result = {outcome.ok, outcome.ok ? "You join " + nameOf(from) + "'s party." : outcome.message, {}};
        if (outcome.ok)
            tellParty(outcome.message, nameOf(id) + " joins the party.", id);
    }
    else if (verb == "decline")
    {
        const auto* waiting = parties_.inviteFor(id, now());
        const std::string from = waiting ? waiting->from : std::string();
        const auto outcome = parties_.decline(id);
        result = {outcome.ok, outcome.ok ? "You decline the invitation." : outcome.message, {}};
        if (outcome.ok)
            if (auto* inviter = clientOf(from))
                system(inviter, nameOf(id) + " declines your invitation.");
    }
    else if (verb == "leave")
    {
        const auto outcome = parties_.leave(id);
        result = {outcome.ok, outcome.ok ? "You leave the party." : outcome.message, {}};
        if (outcome.ok)
            tellParty(partyBefore, nameOf(id) + " leaves the party.", id);
    }
    else if (verb == "remove")
    {
        const auto outcome = parties_.remove(id, target);
        result = {outcome.ok, outcome.ok ? "You send " + nameOf(target) + " from the party." : outcome.message, target};
        if (outcome.ok)
        {
            if (auto* gone = clientOf(target))
                system(gone, nameOf(id) + " sends you from the party.");
            for (const auto& m : partyBefore)
                if (m != id && m != target)
                    if (auto* other = clientOf(m))
                        system(other, nameOf(target) + " is no longer in the party.");
        }
    }
    else if (verb == "lead")
    {
        const auto outcome = parties_.lead(id, target);
        result = {outcome.ok, outcome.ok ? nameOf(target) + " leads the party now." : outcome.message, target};
        if (outcome.ok)
            tellParty(partyBefore, nameOf(target) + " leads the party now.", id);
    }
    else if (verb == "disband")
    {
        const auto outcome = parties_.disband(id);
        result = {outcome.ok, outcome.ok ? "You disband the party." : outcome.message, {}};
        if (outcome.ok)
            tellParty(partyBefore, nameOf(id) + " disbands the party.", id);
    }
    else if (verb == "goal")
    {
        const auto goal = mind::trim(j.string("goal"));
        const auto outcome = parties_.setGoal(id, goal);
        result = {outcome.ok, outcome.ok ? (goal.empty() ? "The party has no goal now." : "The party is bound for: " + goal + ".") : outcome.message, {}};
        if (outcome.ok)
            tellParty(partyBefore, goal.empty() ? "The party has no goal now." : "The party is bound for: " + goal + ".", id);
    }
    else if (verb == "stayout")
    {
        const auto pull = pulls_.find(id);
        if (pull == pulls_.end())
            result = {false, "Nobody's fight is calling you in.", {}};
        else
        {
            stayedOut_[id].insert(pull->second.battle);
            pulls_.erase(pull);
            result = {true, "You stay out of the fight.", {}};
        }
    }
    else if (verb == "autojoin")
    {
        const bool on = j.boolean("on", true);
        parties_.setAutoJoin(id, on);
        if (!on)
            pulls_.erase(id);
        result = {true, on ? "You will join your party's fights when you see them." : "You will no longer be pulled into your party's fights.", {}};
    }
    else
        return false;
    if (result.ok)
        saveSoon();
    return true;
}

void Game::partyChat(Connection* c, const Entity& speaker, const std::string& text, const std::string& channel)
{
    // Out of character, to the party (or the Chapter) wherever they are; each reader sees the speaker as they know them.
    auto e = Value::object();
    e.add("type", "ooc");
    e.add("channel", channel);
    e.add("sequence", sequence_++);
    e.add("text", text);
    e.add("color", speaker.speakingColor);
    std::vector<std::string> readers{speaker.id};
    if (channel == "chapterooc")
    {
        if (const auto* ch = chapters_.of(speaker.id))
            for (const auto& [m, member] : ch->members)
                if (m != speaker.id)
                    readers.push_back(m);
    }
    else
        for (const auto& m : parties_.mates(speaker.id))
            readers.push_back(m);
    (void)c;
    for (const auto& m : readers)
        if (auto* other = clientOf(m))
        {
            e.set("speaker", names::capitalised(labelFor(m, speaker.id)));
            send(other, e);
        }
}

void Game::partyTick(double dt)
{
    partyAccumulator_ += dt;
    if (partyAccumulator_ < 0.25)
        return;
    partyAccumulator_ = 0;
    const double t = world_.time();
    // Places kept and lost: a member out of the world too long loses theirs.
    for (const auto& [who, partyId] : parties_.tick(now(), [&](const std::string& m) { return clientOf(m) != nullptr; }))
        tellParty(partyId, nameOf(who) + " has been away too long and is no longer in the party.");
    // Whoever is on the other side of a fight stays hostile a while after it (players only: they are the viewers).
    std::set<std::string> live;
    for (const auto& b : world_.battles())
    {
        if (b.over && (b.truced || (b.pvp && b.terms != "death")))
            for (const auto& f : b.fighters)        // Peace made, or a duel fairly settled: they are not foes any more.
                if (auto found = foes_.find(f.id); found != foes_.end())
                    for (const auto& g : b.fighters)
                        if (g.side != f.side)
                            found->second.erase(g.id);
        if (b.over)
            continue;
        live.insert(b.id);
        for (const auto& f : b.fighters)
        {
            const auto* fe = world_.entity(f.id);
            if (!fe || fe->npc || f.status == "fled")
                continue;
            for (const auto& g : b.fighters)
                if (g.side != f.side && g.status != "fled")
                    foes_[f.id][g.id] = t + party::AggroSeconds;
        }
    }
    for (auto it = foes_.begin(); it != foes_.end();)
    {
        for (auto k = it->second.begin(); k != it->second.end();)
            k = k->second < t ? it->second.erase(k) : std::next(k);
        it = it->second.empty() ? foes_.erase(it) : std::next(it);
    }
    for (auto it = stayedOut_.begin(); it != stayedOut_.end();)
    {
        for (auto k = it->second.begin(); k != it->second.end();)
            k = live.count(*k) ? std::next(k) : it->second.erase(k);
        it = it->second.empty() ? stayedOut_.erase(it) : std::next(it);
    }
    // A party mate's fight in sight calls the others in (doc 33): after PullSeconds, unless they stay out.
    for (const auto& b : world_.battles())
    {
        if (b.over)
            continue;
        for (const auto& f : b.fighters)
        {
            if (f.status == "fled")
                continue;
            for (const auto& mate : parties_.mates(f.id))
            {
                if (pulls_.count(mate) || !clientOf(mate) || !parties_.autoJoin(mate) || world_.inBattle(mate) ||
                    b.fled.count(mate) || b.observed.count(mate))
                    continue;
                if (const auto out = stayedOut_.find(mate); out != stayedOut_.end() && out->second.count(b.id))
                    continue;
                const auto* me = world_.entity(mate);
                if (!me || me->dead || me->downedLeft > 0 || me->age < battle::YoungestFighter || me->cellId != b.cellId ||
                    world_.custodyOf(mate) || world_.visionClarity(mate, f.id) <= 0)
                    continue;
                // Party mates on both sides (sparring): nobody else is pulled in.
                bool split = false;
                for (const auto& g : b.fighters)
                    if (g.side != f.side && g.status != "fled" && parties_.together(g.id, f.id))
                        split = true;
                if (split)
                    continue;
                pulls_[mate] = {b.id, f.side, t + party::PullSeconds, f.id};
                if (auto* c = clientOf(mate))
                    system(c, "Joining " + nameOf(f.id) + "'s fight\xe2\x80\xa6 Stay out in the Party panel if you'd rather not.");
            }
        }
    }
    for (auto it = pulls_.begin(); it != pulls_.end();)
    {
        const auto& [who, pull] = *it;
        const auto* b = world_.battle(pull.battle);
        if (!b || b->over || world_.inBattle(who) || !clientOf(who) || !parties_.together(who, pull.mate))
        {
            it = pulls_.erase(it);
            continue;
        }
        if (t < pull.at)
        {
            ++it;
            continue;
        }
        const auto joined = world_.joinBattle(who, pull.battle, pull.side);
        stayedOut_[who].insert(pull.battle);           // Once called, never again for this fight.
        if (auto* c = clientOf(who))
            system(c, joined.ok ? "You join " + nameOf(pull.mate) + "'s fight." : joined.message);
        if (joined.ok)
            record(Character, who);
        it = pulls_.erase(it);
    }
}

Game::Relations Game::relationsFor(const std::string& viewer) const
{
    Relations r;
    for (const auto& m : parties_.mates(viewer))
        r.mates.insert(m);
    // Their Chapter: its members in its colour, and whom it marks hostile (doc 32, 2.4 and Part 3).
    if (const auto* ch = chapters_.of(viewer))
    {
        r.colour = ch->colour;
        for (const auto& [m, member] : ch->members)
            if (m != viewer)
                r.chapterMates.insert(m);
        r.chapterMates.insert(ch->sworn.begin(), ch->sworn.end());   // Sworn residents wear its colours too.
        for (const auto& h : ch->hostiles)
        {
            const std::string why = "hostile to your Chapter" + (h.reason.empty() ? std::string() : ": " + h.reason);
            if (h.kind == "wolf")
                r.hostile.emplace(h.target, why);
            else if (h.kind == "chapter")
                if (const auto* other = chapters_.byId(h.target))
                    for (const auto& [m, member] : other->members)
                        r.hostile.emplace(m, why);
        }
    }
    // In a fight, the other side; and the other side of each party mate's fight.
    const auto against = [&](const std::string& who, const char* why) {
        if (const auto* b = world_.battleOf(who))
            if (const auto* f = b->fighter(who))
                for (const auto& g : b->fighters)
                    if (g.side != f->side && g.status != "fled" && !r.hostile.count(g.id))
                        r.hostile[g.id] = why;
    };
    against(viewer, "fighting you");
    for (const auto& m : r.mates)
        against(m, "fighting your party");
    const auto fought = [&](const std::string& who, const char* why) {
        if (const auto found = foes_.find(who); found != foes_.end())
            for (const auto& [foe, until] : found->second)
                if (!r.hostile.count(foe))
                    r.hostile[foe] = why;
    };
    fought(viewer, "fought you");
    for (const auto& m : r.mates)
        fought(m, "fought your party");
    r.hostile.erase(viewer);
    for (const auto& m : r.mates)
        r.hostile.erase(m);
    for (const auto& m : r.chapterMates)
        r.hostile.erase(m);
    return r;
}

Value Game::partyView(const std::string& id) const
{
    auto v = Value::object();
    bool any = false;
    if (const auto* p = parties_.of(id))
    {
        any = true;
        v.add("id", p->id);
        v.add("leader", p->leader);
        auto members = Value::array();
        for (const auto& m : p->members)
        {
            auto j = Value::object();
            j.add("id", m);
            j.add("name", names::capitalised(labelFor(id, m)));      // A party mate is a stranger until introduced.
            j.add("leader", m == p->leader);
            const auto* e = world_.entity(m);
            const bool online = e && clientOf(m);
            j.add("online", online);
            if (online)
            {
                j.add("cell", e->cellId);
                if (const auto* cell = world_.cell(e->cellId))
                    j.add("place", cell->name);
                j.add("x", std::round(e->position.x * 10) / 10);
                j.add("y", std::round(e->position.y * 10) / 10);
                j.add("health", std::round(100 - e->hurt));
                if (e->downedLeft > 0)
                    j.add("downed", true);
                j.add("fighting", world_.inBattle(m));
            }
            members.push(j);
        }
        // Residents travelling with it (Phase 3), after the players.
        for (const auto& c : p->companions)
        {
            auto j = Value::object();
            j.add("id", c.id);
            j.add("name", names::capitalised(labelFor(id, c.id)));
            j.add("npc", true);
            j.add("reason", c.reason);
            if (c.wage > 0)
                j.add("wage", double(c.wage));
            j.add("waiting", c.waiting);
            j.add("mine", c.by == id || p->leader == id);         // Whether this player may give them orders.
            if (const auto* e = world_.entity(c.id))
            {
                j.add("online", true);
                j.add("cell", e->cellId);
                if (const auto* cell = world_.cell(e->cellId))
                    j.add("place", cell->name);
                j.add("x", std::round(e->position.x * 10) / 10);
                j.add("y", std::round(e->position.y * 10) / 10);
                j.add("health", std::round(100 - e->hurt));
                if (e->downedLeft > 0)
                    j.add("downed", true);
                j.add("fighting", world_.inBattle(c.id));
            }
            members.push(j);
        }
        v.add("members", members);
        if (!p->goal.empty())
            v.add("goal", p->goal);
    }
    if (const auto* waiting = parties_.inviteFor(id, now()))
    {
        any = true;
        auto j = Value::object();
        j.add("from", waiting->from);
        j.add("name", names::capitalised(labelFor(id, waiting->from)));
        j.add("seconds", std::max(0.0, std::ceil(waiting->expires - now())));
        v.add("invite", j);
    }
    if (const auto pull = pulls_.find(id); pull != pulls_.end())
    {
        any = true;
        auto j = Value::object();
        j.add("name", names::capitalised(labelFor(id, pull->second.mate)));
        j.add("seconds", std::max(0.0, std::ceil(pull->second.at - world_.time())));
        v.add("pull", j);
    }
    if (!parties_.autoJoin(id))
    {
        any = true;
        v.add("autoJoin", false);
    }
    return any ? v : Value();
}
} // namespace ratw::game
