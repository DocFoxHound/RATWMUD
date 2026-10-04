// The individual social game as players see it (Docs/Design/32-parties-chapters-factions.md, Part 1; the ledger's rules
// are RatwSocialCore.cpp): the scene a player is in and how it settled, Gold Stars and Stories, a title for their
// social level, how a wolf regards them in words, their name about a town (worked out when asked, never stored), and
// private notes on wolves they know.
#include "RatwGame.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
const char* degree(double v, const char* low, const char* mid, const char* high)
{
    return v >= 60 ? high : v >= 30 ? mid : low;
}
} // namespace

std::string Game::regardWords(const std::string& holder, const std::string& other) const
{
    const auto* b = world_.bonds().find(holder, other);
    if (!b || b->familiarity < 3)
        return "don't know you";
    std::vector<std::string> parts;
    parts.push_back(b->familiarity >= 50 ? "know you well" : b->familiarity >= 15 ? "know you" : "know you a little");
    if (b->affinity >= 10)
        parts.push_back(std::string("like you") + degree(b->affinity, " a little", "", " a great deal"));
    else if (b->affinity <= -10)
        parts.push_back(std::string("dislike you") + degree(-b->affinity, " a little", "", " intensely"));
    if (b->trust >= 10)
        parts.push_back(std::string("trust you") + degree(b->trust, " a little", "", " completely"));
    else if (b->trust <= -10)
        parts.push_back(std::string("distrust you") + degree(-b->trust, " a little", "", " entirely"));
    if (b->fear >= 20)
        parts.push_back("are afraid of you");
    if (b->respect >= 30)
        parts.push_back("respect you");
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i)
        out += (i == 0 ? "" : i + 1 == parts.size() ? " and " : ", ") + parts[i];
    if (b->owed > 0)
        out += "; you owe them " + std::to_string(b->owed) + (b->owed == 1 ? " penny" : " pennies");
    else if (b->owed < 0)
        out += "; they owe you " + std::to_string(-b->owed) + (b->owed == -1 ? " penny" : " pennies");
    return out;
}

std::vector<std::string> Game::reputationLines(const std::string& playerId) const
{
    // Read from the residents who know them, by community: never stored, never a faction's score (doc 32, 1.4).
    struct Tally
    {
        int known = 0, liked = 0, disliked = 0, trusted = 0, distrusted = 0, guards = 0, guardsWary = 0, talk = 0;
    };
    std::map<std::string, Tally> by;
    for (const auto& [id, e] : world_.entities())
    {
        if (!e.npc || e.transient || e.dead)
            continue;
        const auto community = world_.communityOf(e.cellId);
        if (community.empty())
            continue;
        if (const auto* beliefs = world_.beliefsOf(id))
            for (const auto& b : *beliefs)
                if (b.subject == playerId)
                {
                    ++by[community].talk;
                    break;
                }
        const auto* b = world_.bonds().find(id, playerId);
        if (!b || b->familiarity < 10)
            continue;
        auto& t = by[community];
        ++t.known;
        t.liked += b->affinity >= 20;
        t.disliked += b->affinity <= -20;
        t.trusted += b->trust >= 20;
        t.distrusted += b->trust <= -20;
        if (const auto* job = world_.society().jobOf(id); job && job->role == "guard")
        {
            ++t.guards;
            t.guardsWary += b->trust <= -10 || b->affinity <= -10;
        }
    }
    std::vector<std::string> out;
    for (const auto& [community, t] : by)
    {
        std::string place = community;
        if (const auto* c = world_.cell(community))
            place = c->name;
        std::vector<std::string> parts;
        if (t.known)
            parts.push_back("known to " + std::to_string(t.known) + (t.known == 1 ? " resident" : " residents"));
        if (t.liked * 2 > t.known && t.known >= 2)
            parts.push_back("well liked");
        else if (t.disliked * 2 > t.known && t.known >= 2)
            parts.push_back("disliked");
        if (t.trusted * 2 > t.known && t.known >= 2)
            parts.push_back("trusted");
        else if (t.distrusted * 2 > t.known && t.known >= 2)
            parts.push_back("distrusted");
        if (t.guardsWary > 0 && t.guardsWary * 2 >= t.guards)
            parts.push_back("the Watch is wary of you");
        if (t.talk)
            parts.push_back(std::to_string(t.talk) + " have heard talk of you");
        if (!parts.empty())
        {
            std::string line = place + ": ";
            for (std::size_t i = 0; i < parts.size(); ++i)
                line += (i ? "; " : "") + parts[i];
            out.push_back(line);
        }
    }
    if (out.empty())
        out.push_back("Nobody in any town knows you yet.");
    return out;
}

void Game::afterSocial()
{
    // Scenes settled since last time: those who took part are told, and grow closer (doc 32, 1.4).
    const auto& entries = social_.entries;
    for (; socialSeen_ < entries.size(); ++socialSeen_)
    {
        const auto& e = entries[socialSeen_];
        if (e.reason != "qualified_session_settlement")
            continue;
        if (auto* c = clientOf(e.actor))
            system(c, e.amount > 0 ? "The scene ends. +" + std::to_string(e.amount) + " social." : "The scene ends.");
        std::size_t at = 0;
        while (at <= e.partner.size())
        {
            const auto comma = e.partner.find(',', at);
            const auto partner = e.partner.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
            if (!partner.empty())
                world_.bonds().change(e.actor, partner, {1, 0.5, 2, 0, 0.5}, world_.calendarDays());
            if (comma == std::string::npos)
                break;
            at = comma + 1;
        }
        onSettled(e);                             // (Chapters take their share: doc 32, Part 3.)
    }
    socialViewsDirty_ = true;
}

void Game::refreshSocialViews(double dt)
{
    socialViewsAccumulator_ += dt;
    if (!socialViewsDirty_ && socialViewsAccumulator_ < 2)
        return;
    socialViewsAccumulator_ = 0;
    socialViewsDirty_ = false;
    socialViews_.clear();
    const double t = now();
    for (const auto* c : clients_)
    {
        const auto& id = c->entityId;
        if (id.empty())
            continue;
        auto v = Value::object();
        v.add("title", socialTitle(social_.level(id)));
        // The scene they are in now.
        for (const auto& [sid, s] : social_.sessions)
        {
            if (s.ended != 0 || !s.members.count(id))
                continue;
            auto scene = Value::object();
            scene.add("id", sid);
            scene.add("party", !s.party.empty());
            auto with = Value::array();
            for (const auto& [m, contribution] : s.members)
                if (m != id)
                    with.push(names::capitalised(labelFor(id, m)));
            scene.add("with", with);
            scene.add("turns", s.members.at(id).turns);
            scene.add("quiet", t - s.last >= 900);
            v.add("scene", scene);
            break;
        }
        // The last scene they finished, while it may still be starred (an hour).
        const SocialSession* last = nullptr;
        for (const auto& [sid, s] : social_.sessions)
            if (s.ended > 0 && t - s.ended < 3600 && s.members.count(id) && (!last || s.ended > last->ended))
                last = &s;
        if (last)
        {
            bool qualified = false;
            for (const auto& e : social_.entries)
                qualified |= e.actor == id && e.session == last->id && e.reason == "qualified_session_settlement";
            if (qualified)
            {
                auto ended = Value::object();
                ended.add("id", last->id);
                ended.add("xp", social_.paidFor(id, last->id));
                bool starred = false;
                for (const auto& st : social_.stars)
                    starred |= st.kind == "gold" && st.source == last->id && st.giver == id;
                auto targets = Value::array();
                if (!starred)
                    for (const auto& e : social_.entries)
                        if (e.session == last->id && e.reason == "qualified_session_settlement" && e.actor != id)
                        {
                            auto o = Value::object();
                            o.add("id", e.actor);
                            o.add("name", names::capitalised(labelFor(id, e.actor)));
                            targets.push(o);
                        }
                ended.add("starTargets", targets);
                // May it begin or carry on a Story?
                const auto paid = social_.paidIn(last->id);
                ended.add("storyable", !social_.storyOf(last->id) && paid.size() >= 2 && social_.paidFor(id, last->id) > 0);
                v.add("ended", ended);
            }
        }
        // Their Stories.
        auto stories = Value::array();
        for (const auto& [sid, st] : social_.stories)
        {
            if (!st.members.count(id) || st.state == "expired" || (st.state == "closed" && t - st.last > 86400 * 3))
                continue;
            auto o = Value::object();
            o.add("id", sid);
            o.add("name", st.name);
            o.add("state", st.state);
            o.add("mine", st.owner == id);
            o.add("scenes", double(st.scenes.size()));
            o.add("members", double(st.members.size()));
            o.add("approved", st.approvals.count(id) > 0);
            if (!st.chapter.empty())
                o.add("chapter", true);
            if (st.state == "closed" && !st.starred.count(id))
            {
                auto targets = Value::array();
                for (const auto& m : st.members)
                    if (m != id)
                    {
                        auto k = Value::object();
                        k.add("id", m);
                        k.add("name", names::capitalised(labelFor(id, m)));
                        targets.push(k);
                    }
                o.add("starTargets", targets);
            }
            stories.push(o);
        }
        v.add("stories", stories);
        socialViews_[id] = v;
    }
}

bool Game::socialCommand(Connection* c, const Value& j, Result& result)
{
    const std::string id = c->entityId, verb = j.string("verb");
    const double t = now();
    const auto said = [&](const SocialResult& r, const std::string& done) {
        result = {r.ok, r.ok ? done : r.message, {}};
        if (r.ok)
        {
            socialViewsDirty_ = true;
            saveSoon();
        }
    };
    if (verb == "star")
    {
        const auto target = j.string("target");
        const auto r = social_.star(id, target, j.string("session"), t);
        said(r, "You give " + labelFor(id, target) + " a Gold Star.");
        if (r.ok)
        {
            world_.bonds().change(target, id, {2, 1, 1, 0, 1}, world_.calendarDays());
            if (auto* other = clientOf(target))
                system(other, names::capitalised(labelFor(target, id)) + " gives you a Gold Star" +
                                  (r.amount > 0 ? " (+" + std::to_string(r.amount) + " social)." : "."));
        }
    }
    else if (verb == "propose")
    {
        const auto r = social_.propose(id, j.string("session"), mind::trim(j.string("name")), t);
        said(r, "You begin a Story. The others who took part are asked to agree to it.");
        if (r.ok)
            if (const auto found = social_.stories.find(r.message); found != social_.stories.end())
            {
                markChapterStory(found->second);
                for (const auto& m : found->second.members)
                    if (m != id)
                        if (auto* other = clientOf(m))
                            system(other, names::capitalised(labelFor(m, id)) + " would make a Story of your scene: \"" +
                                              found->second.name + "\". Agree to it in your character sheet.");
            }
    }
    else if (verb == "approve")
        said(social_.approve(id, j.string("story"), t), "You agree to the Story.");
    else if (verb == "extend")
        said(social_.extend(id, j.string("story"), j.string("session"), t), "The Story goes on.");
    else if (verb == "close")
    {
        const auto r = social_.close(id, j.string("story"), t);
        said(r, "The Story is told.");
        if (r.ok)
            if (const auto found = social_.stories.find(j.string("story")); found != social_.stories.end())
            {
                for (const auto& m : found->second.members)
                    if (auto* other = clientOf(m))
                        system(other, "\"" + found->second.name + "\" is told. " + std::to_string(social_.paidFor(m, found->second.id)) +
                                          " social for seeing it through.");
                onStoryClosed(found->second);
            }
    }
    else if (verb == "storystar")
    {
        const auto target = j.string("target");
        const auto r = social_.storyStar(id, target, j.string("story"), t);
        said(r, "You give " + labelFor(id, target) + " a Story Star.");
        if (r.ok)
            if (auto* other = clientOf(target))
                system(other, names::capitalised(labelFor(target, id)) + " gives you a Story Star" +
                                  (r.amount > 0 ? " (+" + std::to_string(r.amount) + " social)." : "."));
    }
    else if (verb == "reputation")
    {
        auto e = Value::object();
        e.add("type", "reputation");
        auto lines = Value::array();
        for (const auto& line : reputationLines(id))
            lines.push(line);
        e.add("lines", lines);
        send(c, e);
        result = {true, {}, {}};
    }
    else if (verb == "note")
    {
        const auto target = j.string("target");
        const auto text = mind::trim(j.string("text"));
        if (text.size() > 500)
            result = {false, "A note is at most 500 letters.", target};
        else if (target.empty() || target == id || (!world_.entity(target) && !characters_.count(target)))
            result = {false, "There is no one to note.", target};
        else
        {
            if (text.empty())
                notes_[id].erase(target);
            else
                notes_[id][target] = text;
            if (notes_[id].size() > 300)
                result = {false, "You keep too many notes already.", target};
            else
            {
                result = {true, text.empty() ? "Note cleared." : "Noted.", target};
                saveSoon();
            }
        }
    }
    else
        return false;
    return true;
}

Value Game::notesSave() const
{
    auto root = Value::object();
    for (const auto& [owner, list] : notes_)
    {
        auto o = Value::object();
        for (const auto& [who, text] : list)
            o.add(who, text);
        root.add(owner, o);
    }
    return root;
}

void Game::notesLoad(const Value& saved)
{
    notes_.clear();
    for (const auto& [owner, list] : saved.fields())
        for (const auto& [who, text] : list.fields())
            if (text.isString() && notes_[owner].size() < 300)
                notes_[owner][who] = text.asString().substr(0, 500);
}
} // namespace ratw::game

