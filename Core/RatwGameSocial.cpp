// The individual social game as players see it (Docs/Design/32-parties-chapters-factions.md, Part 1; the ledger's rules
// are RatwSocialCore.cpp): the scene a player is in and how it settled, Gold Stars and Stories, a title for their
// social level, how a wolf regards them in words, their name about a town (worked out when asked, never stored), and
// private notes on wolves they know.
#include "RatwWire.h"
#include "RatwStanding.h"
#include "RatwPractice.h"
#include "RatwGame.h"

#include <functional>
#include <set>
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
    std::set<std::string> touched;                  // Accounts whose earned Gift tiers to look at again (doc 49).
    for (; socialSeen_ < entries.size(); ++socialSeen_)
    {
        const auto& e = entries[socialSeen_];
        if (practice::socialReason(e.reason))
            touched.insert(accounts_.ownerOf(e.actor));
        if (e.reason != "qualified_session_settlement")
            continue;
        // The star rate's chances (doc 51, §3): one for each other who qualified with them, who could have starred them.
        if (!e.partner.empty())
            starBook_.chances(accountKey(e.actor), int(std::count(e.partner.begin(), e.partner.end(), ',')) + 1);
        const bool fight = e.session.rfind("fight-", 0) == 0;
        if (auto* c = clientOf(e.actor))
            system(c, fight ? "The fight is over" + (e.amount > 0 ? ": +" + std::to_string(e.amount) + " social" : std::string()) +
                                  ". Give a Gold Star to each who roleplayed it well (above where you write)."
                            : e.amount > 0 ? "The scene ends. +" + std::to_string(e.amount) + " social." : "The scene ends.");
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
    for (const auto& account : touched)
        checkUnlocks(account);
    tendScenes();                                   // Known wolves and recaps for scenes over (doc 50, 5).
    socialViewsDirty_ = true;
}

void Game::tendFightScenes()
{
    // Each fight is a scene of its own (doc 33): its players are in it from the start, talking or not; when it is over,
    // those who took their turns are paid for it, and a scene's ordinary pay for roleplaying it through (doc 51).
    bool settled = false;
    for (const auto& b : world_.battles())
    {
        if (!b.over)
        {
            for (const auto& f : b.fighters)
                if (const auto* e = world_.entity(f.id); e && !e->npc && f.status != "fled")
                    social_.joinFight(b.id, b.cellId, f.id, now());
            continue;
        }
        const auto scene = social_.sessions.find(SocialLedger::fightScene(b.id));
        if (scene == social_.sessions.end() || scene->second.ended > 0)
            continue;
        std::set<std::string> fought;
        for (const auto& f : b.fighters)
            if (f.turnsTaken >= 2)
                fought.insert(f.id);
        social_.settleFight(b.id, fought, now());
        settled = true;
    }
    // A fight gone without being seen over (cleared at once): its scene ends with what is known.
    for (auto& [sid, s] : social_.sessions)
        if (s.ended == 0 && SocialLedger::isFight(s))
        {
            const auto fight = s.party.substr(6);
            if (std::none_of(world_.battles().begin(), world_.battles().end(), [&](const Battle& b) { return b.id == fight; }))
            {
                std::set<std::string> members;
                for (const auto& [m, c] : s.members)
                    members.insert(m);
                social_.settleFight(fight, members, now());
                settled = true;
            }
        }
    if (settled)
    {
        afterSocial();
        saveSoon();
    }
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
        v.add("title", socialTitle(socialLevel(id)));
        v.add("stars", starsFor(id, id));              // (Their account's stars, exact: doc 51.)
        // Stars this wolf gave that it may still tag (doc 51, §2), each with whom it went to and the seconds left.
        auto given = Value::array();
        for (const auto* st : starBook_.openToTag(id, now()))
        {
            auto row = Value::object();
            row.add("id", st->id);
            row.add("to", names::capitalised(labelFor(id, st->recipientCharacter)));
            row.add("kind", st->kind);
            row.add("left", std::max(0.0, std::round(stars::rules().tagWindow - (now() - st->at))));
            auto tags = Value::array();                 // (The tags this wolf may give: Welcoming waits for doc 52.)
            for (const auto& tag : stars::rules().tags)
                if (std::find(stars::rules().newcomersOnly.begin(), stars::rules().newcomersOnly.end(), tag) == stars::rules().newcomersOnly.end())
                {
                    auto chip = Value::object();
                    chip.add("id", tag);
                    chip.add("name", stars::rules().tagNames.count(tag) ? stars::rules().tagNames.at(tag) : tag);
                    tags.push(chip);
                }
            row.add("tags", tags);
            given.push(row);
        }
        v.add("starsGiven", given);
        // The scenes they are in now (a party's or a fight's beside the room's), each with what they still need to be
        // paid, how long it has been quiet and when that ends it, and the one their next line counts toward (doc 08).
        const auto* me = world_.entity(id);
        const auto* fight = world_.battleOf(id);
        const bool fighting = fight && !fight->over && fight->fighter(id) && fight->fighter(id)->status != "fled";
        bool partyNear = false;
        if (const auto* mine = parties_.of(id); mine && me)
            for (const auto* other : clients_)
                if (other != c && !other->entityId.empty() && parties_.together(id, other->entityId))
                    if (const auto* e = world_.entity(other->entityId); e && e->cellId == me->cellId)
                        partyNear = true;
        auto scenes = Value::array();
        for (const auto& sid : social_.scenesOf(id))     // (Their own open scenes, from the ledger's index: doc 51.)
        {
            const auto found = social_.sessions.find(sid);
            if (found == social_.sessions.end())
                continue;
            const auto& s = found->second;
            const auto mine = s.members.find(id);
            if (s.ended != 0 || mine == s.members.end() || mine->second.left)
                continue;
            const auto& m = mine->second;
            const bool fightScene = SocialLedger::isFight(s);
            auto scene = Value::object();
            scene.add("id", sid);
            scene.add("party", !s.party.empty() && !fightScene);
            scene.add("fight", fightScene);
            auto with = Value::array();
            int othersShaped = 0;
            for (const auto& [other, contribution] : s.members)
                if (other != id && !contribution.left)
                {
                    with.push(names::capitalised(labelFor(id, other)));
                    if (SocialLedger::shaped(contribution))
                        ++othersShaped;
                }
                else if (other != id && SocialLedger::shaped(contribution))
                    ++othersShaped;          // (Stepped out, but their part still counts for the scene.)
            scene.add("with", with);
            scene.add("turns", m.turns);
            scene.add("words", m.words);
            scene.add("replies", m.replies);
            scene.add("needTurns", std::max(0, SocialLedger::ShapeTurns - m.turns));
            scene.add("needWords", std::max(0, SocialLedger::ShapeWords - m.words));
            scene.add("needReply", m.replies < SocialLedger::ShapeReplies);
            scene.add("othersShaped", othersShaped);
            const double quiet = std::max(0.0, t - s.last);
            scene.add("quiet", quiet >= SocialLedger::QuietSeconds);
            if (!fightScene)
                scene.add("endsIn", std::max(0.0, SocialLedger::EndSeconds - quiet));
            scene.add("next", fightScene ? fighting : !fighting && (s.party.empty() ? !partyNear : partyNear));
            // Who may come in (doc 51, §5), and who is knocking, as this wolf knows them.
            if (!fightScene)
            {
                scene.add("openness", s.openness);
                auto knocks = Value::array();
                for (const auto& [who, at] : s.knocks)
                    if (t - at < SocialLedger::KnockSeconds)
                    {
                        auto k = Value::object();
                        k.add("id", who);
                        k.add("name", names::capitalised(labelFor(id, who)));
                        knocks.push(k);
                    }
                scene.add("knocks", knocks);
            }
            scenes.push(scene);
        }
        if (!scenes.items().empty())
            v.add("scene", scenes.items().front());
        v.add("scenes", scenes);
        // Open and Knock scenes here that this wolf isn't in and could hear (doc 51, §6): how many wolves, never names.
        auto nearby = Value::array();
        if (me)
            for (const auto& sid : social_.openIn(me->cellId))
            {
                const auto found = social_.sessions.find(sid);
                if (found == social_.sessions.end())
                    continue;
                const auto& s = found->second;
                if (s.ended != 0 || SocialLedger::isFight(s) || s.openness == "private" || s.members.count(id))
                    continue;
                int wolves = 0;
                bool hears = false, blockedHere = false;
                for (const auto& [who, m] : s.members)
                    if (!m.left)
                    {
                        ++wolves;
                        blockedHere |= blocked(id, who);
                        hears = hears || (world_.entity(who) && world_.perceive(id, who, Voice::Speak).hearing > 0);
                    }
                if (!hears || blockedHere || wolves == 0)
                    continue;
                auto n = Value::object();
                n.add("id", sid);
                n.add("wolves", wolves);
                n.add("openness", s.openness);
                n.add("party", !s.party.empty());
                if (const auto k = s.knocks.find(id); k != s.knocks.end() && t - k->second < SocialLedger::KnockSeconds)
                    n.add("knocked", true);
                nearby.push(n);
            }
        v.add("nearby", nearby);
        // The last scene they finished, while it may still be starred (an hour).
        const SocialSession* last = nullptr;
        if (const auto ended = social_.sessions.find(social_.lastEnded(id)); ended != social_.sessions.end() && t - ended->second.ended < 3600)
            last = &ended->second;
        const auto settledIn = [&](const std::string& who, const std::string& session) {
            for (const auto i : social_.receiptsOf(who))
                if (social_.entries[i].session == session && social_.entries[i].reason == "qualified_session_settlement")
                    return true;
            return false;
        };
        if (last)
        {
            const bool qualified = settledIn(id, last->id);
            if (qualified)
            {
                auto ended = Value::object();
                ended.add("id", last->id);
                ended.add("xp", social_.paidFor(id, last->id));
                // A star for each other who took part, one each (doc 33's fight review; any scene the same).
                auto targets = Value::array(), starredNames = Value::array();
                std::set<std::string> listed;
                for (const auto& [other, m] : last->members)
                    if (other != id && settledIn(other, last->id) && listed.insert(other).second)
                    {
                        const bool starred = std::any_of(social_.stars.begin(), social_.stars.end(), [&](const SocialStar& st) {
                            return st.kind == "gold" && st.source == last->id && st.giver == id && st.recipient == other;
                        });
                        auto o = Value::object();
                        o.add("id", other);
                        o.add("name", names::capitalised(labelFor(id, other)));
                        if (starred)
                            starredNames.push(o);
                        else
                            targets.push(o);
                    }
                ended.add("starTargets", targets);
                ended.add("starred", starredNames);
                if (SocialLedger::isFight(*last))
                {
                    // A fight: paid for talking it through, as a scene is (doc 51).
                    const auto& me = last->members.at(id);
                    ended.add("fight", true);
                    ended.add("talked", me.turns >= 2 && me.words >= 35 && me.replies >= 1);
                }
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
    // A star between one account's own wolves is refused (doc 51): stars are thanks from another player.
    if ((verb == "star" || verb == "storystar") && !j.string("target").empty() && accountKey(id) == accountKey(j.string("target")))
    {
        result = {false, "Not one of your own wolves.", {}};
        return true;
    }
    if (verb == "star")
    {
        const auto target = j.string("target");
        const auto r = social_.star(id, target, j.string("session"), t);
        said(r, "You give " + labelFor(id, target) + " a Gold Star.");
        if (r.ok)
        {
            recordStar("gold", j.string("session"), id, target, r.amount);
            world_.bonds().change(target, id, {2, 1, 1, 0, 1}, world_.calendarDays());
            if (auto* other = clientOf(target))
                system(other, names::capitalised(labelFor(target, id)) + " gives you a Gold Star" +
                                  (r.amount > 0 ? " (+" + std::to_string(r.amount) + " social)." : "."));
        }
    }
    else if (verb == "leave")
    {
        // Stepping out of a scene (doc 08): settled for oneself if qualified; the others carry on.
        int paid = 0;
        if (!social_.leave(id, j.string("session"), t, &paid))
            result = {false, "You aren't in that scene.", {}};
        else
        {
            const bool receipt = std::any_of(social_.entries.begin(), social_.entries.end(), [&](const LedgerEntry& e) {
                return e.actor == id && e.session == j.string("session") && e.reason == "qualified_session_settlement";
            });
            result = {true, !receipt ? "You step out of the scene. You hadn't said enough with another to be paid for it."
                            : paid > 0 ? "You step out of the scene: +" + std::to_string(paid) + " social."
                                       : "You step out of the scene. You've had all the social pay there is today.", {}};
            tendScenes();                           // (Their recap, and those they were with: doc 50, 5.)
            socialViewsDirty_ = true;
            saveSoon();
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
    else if (verb == "openness" || verb == "join" || verb == "knock" || verb == "admit" || verb == "refuse")
    {
        // Openness, joining and knocking (doc 51, §5-6). The ledger keeps each scene's rules; here, who can hear whom,
        // blocks, and who is told.
        const auto sid = j.string("session");
        const auto it = social_.sessions.find(sid);
        const auto* me = world_.entity(id);
        if (it == social_.sessions.end() || !me)
        {
            result = {false, "No such scene.", {}};
            return true;
        }
        const auto& scene = it->second;
        const auto tellMembers = [&](const std::string& except, const std::function<std::string(const std::string&)> words) {
            for (const auto& [who, m] : scene.members)
                if (!m.left && who != except)
                    if (auto* other = clientOf(who))
                        system(other, words(who));
        };
        // One outside it: here, and able to hear one of its wolves speaking.
        const auto inEarshot = [&]() {
            if (scene.cell != me->cellId)
                return false;
            for (const auto& [who, m] : scene.members)
                if (!m.left && world_.entity(who) && world_.perceive(id, who, Voice::Speak).hearing > 0)
                    return true;
            return false;
        };
        const auto blockedByAny = [&]() {
            for (const auto& [who, m] : scene.members)
                if (blocked(id, who))
                    return true;
            return false;
        };
        if (verb == "openness")
        {
            const auto value = j.string("value");
            const bool changes = scene.openness != value;
            const auto r = social_.setOpenness(id, sid, value, t);
            result = {r.ok, r.ok ? std::string(value == "open" ? "The scene is open: anyone near may join in."
                                               : value == "knock" ? "The scene is knock to join: others ask, and one of you lets them in."
                                                                  : "The scene is private: only its wolves.") : r.message, {}};
            if (r.ok && changes)
                tellMembers(id, [&](const std::string& who) {
                    return names::capitalised(labelFor(who, id)) + (value == "open" ? " opened the scene." : value == "knock" ? " made the scene knock to join." : " made the scene private.");
                });
        }
        else if (verb == "join")
        {
            if (!inEarshot())
                result = {false, "You're too far from that scene to join it.", {}};
            else if (blockedByAny())
                result = {false, "You can't join that scene.", {}};      // (Never who: doc 50, 7.)
            else
            {
                const auto r = social_.join(id, sid, t);
                result = {r.ok, r.ok ? "You join the scene: your next words count in it." : r.message, {}};
            }
        }
        else if (verb == "knock")
        {
            if (!inEarshot())
                result = {false, "You're too far from that scene to knock.", {}};
            else if (blockedByAny())
                result = {false, "No answer.", {}};
            else
            {
                const auto r = social_.knock(id, sid, t);
                result = {r.ok, r.ok ? "You knock. One of the scene's wolves may let you in." : r.message, {}};
                if (r.ok)
                    for (const auto& [who, m] : scene.members)
                        if (!m.left)
                            if (auto* other = clientOf(who))
                            {
                                auto e = Value::object();
                                e.add("type", "knock");
                                e.add("session", sid);
                                e.add("from", names::capitalised(labelFor(who, id)));
                                if (const auto p = profiles_.find(id); p != profiles_.end())
                                    e.add("status", p->second.status);
                                send(other, e);
                                system(other, names::capitalised(labelFor(who, id)) + " is knocking: let them in, or not now, above where you write.");
                            }
            }
        }
        else
        {
            const auto who = j.string("who");
            const auto r = verb == "admit" ? social_.admit(id, sid, who, t) : social_.refuse(id, sid, who, t);
            result = {r.ok, r.ok ? std::string(verb == "admit" ? "Let in." : "Not now.") : r.message, {}};
            if (r.ok)
            {
                if (auto* them = clientOf(who))
                    system(them, verb == "admit" ? "You're let in: your next words count in the scene." : "No one lets you in just now.");
                if (verb == "admit")
                    tellMembers(id, [&](const std::string& m) {
                        return names::capitalised(labelFor(m, id)) + " let " + labelFor(m, who) + " in.";
                    });
            }
        }
        if (result.ok)
        {
            socialViewsDirty_ = true;
            saveSoon();
        }
    }
    else if (verb == "startag")
    {
        // Saying what a star was for (doc 51, §2): its giver, once, within ten minutes. (Welcoming waits for doc 52's
        // newcomers: until then no one is a newcomer.)
        std::string why;
        if (starBook_.tag(j.string("star"), accountKey(id), j.string("tag"), false, t, why))
        {
            const auto& tagNames = stars::rules().tagNames;
            result = {true, "Tagged: " + (tagNames.count(j.string("tag")) ? tagNames.at(j.string("tag")) : j.string("tag")) + ".", {}};
            socialViewsDirty_ = true;
            saveSoon();
        }
        else
            result = {false, why, {}};
    }
    else if (verb == "storystar")
    {
        const auto target = j.string("target");
        const auto r = social_.storyStar(id, target, j.string("story"), t);
        said(r, "You give " + labelFor(id, target) + " a Story Star.");
        if (r.ok)
            recordStar("story", j.string("story"), id, target, r.amount);
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
        // (The older way to note a wolf: kept in Known wolves now, doc 50, 5.)
        auto k = Value::object();
        k.add("verb", "note");
        k.add("target", j.string("target"));
        k.add("text", j.string("text"));
        knownCommand(c, k, result);
    }
    else
        return false;
    return true;
}

long long Game::socialXp(const std::string& characterId) const
{
    const auto account = accounts_.ownerOf(characterId);
    long long xp = 0;
    for (const auto& id : account.empty() ? std::vector<std::string>{characterId} : accounts_.characters(account))
        if (const auto it = social_.points.find(id); it != social_.points.end())
            xp += it->second;
    return xp;
}

int Game::socialLevel(const std::string& characterId) const
{
    return practice::levelFor(socialXp(characterId));
}
// ------------------------------------------------------------------ Earned Gift tiers (doc 49, Phase 5)

int Game::upheldReports(const std::string& account) const
{
    return upheldReportsWithin(account, standing::thresholds().reportDays);   // (Reports: doc 50, Phase 2.)
}

standing::Measures Game::measuresOf(const std::string& account) const
{
    // Counted from the ledger: the account's social level; scenes paid on its Normal wolves; stars its wolves received
    // from other accounts' wolves, and how many accounts gave them; Stories its wolves saw closed.
    standing::Measures m;
    const auto mine = accounts_.characters(account);
    const auto normal = [&](const std::string& id) {
        if (const auto* e = world_.entity(id))
            return e->gift.empty();
        const auto saved = characters_.find(id);
        return saved != characters_.end() && saved->second.gift.empty();
    };
    long long xp = 0;
    std::set<std::string> stories;
    for (const auto& id : mine)
    {
        if (const auto it = social_.points.find(id); it != social_.points.end())
            xp += it->second;
        const bool plain = normal(id);
        for (const auto i : social_.receiptsOf(id))
        {
            const auto& e = social_.entries[i];
            if (e.reason == "qualified_session_settlement" && e.amount > 0 && plain)
                ++m.normalScenes;
            else if (e.reason == "story_closure")
                stories.insert(e.session);
        }
    }
    // Stars from the star book (doc 51): counted stars, and how many accounts gave them.
    if (const auto* tally = starBook_.tally(account))
    {
        m.stars = tally->total;
        m.starGivers = int(tally->givers.size());
    }
    m.socialLevel = practice::levelFor(xp);
    m.closedStories = int(stories.size());
    m.upheldReports = upheldReports(account);
    return m;
}

void Game::checkUnlocks(const std::string& account)
{
    // A tier met opens and stays open (doc 49, 9), unless the Dungeon Master holds the account's unlocks.
    if (account.empty() || !accounts_.exists(account))
        return;
    auto& r = standing_[account];
    r.account = account;
    const auto m = measuresOf(account);
    measures_[account] = m;
    const auto& t = standing::thresholds();
    std::vector<std::string> told;
    if (!r.hold && r.giftedAt < 0 && standing::meetsGifted(m, t))
    {
        r.giftedAt = now();
        r.giftedBy = "earned";
        told.push_back("Wolves have noticed your roleplay. You may now create Gifted wolves.");
    }
    if (!r.hold && r.giftedAt >= 0 && r.quickenedAt < 0 && standing::meetsQuickened(m, t))
    {
        r.quickenedAt = now();
        r.quickenedBy = "earned";
        told.push_back("Your roleplay is spoken of far and wide. You may now create Quickened wolves.");
    }
    if (!told.empty())
    {
        for (const auto& id : accounts_.characters(account))
            if (auto* c = clientOf(id))
                for (const auto& line : told)
                    system(c, line);
        saveSoon();
    }
    tiersViews_[account] = tiersView(account);
}

Result Game::unlockTier(const std::string& account, const std::string& tier, const std::string& op, const std::string& by)
{
    // A Dungeon Master's word on an account's tiers: grant or revoke one, or hold and release new unlocks. Revoking is
    // the one exception to "kept once earned" (for abuse), and stands only while the account is held: a release lets
    // whatever is met open again.
    if (account.empty() || !accounts_.exists(account))
        return {false, "That character belongs to no account.", {}};
    if ((op == "grant" || op == "revoke") && tier != "gifted" && tier != "quickened")
        return {false, "Choose Gifted or Quickened.", {}};
    if (op != "grant" && op != "revoke" && op != "hold" && op != "release")
        return {false, "Grant, revoke, hold or release.", {}};
    auto& r = standing_[account];
    r.account = account;
    double& at = tier == "quickened" ? r.quickenedAt : r.giftedAt;
    std::string& how = tier == "quickened" ? r.quickenedBy : r.giftedBy;
    if (op == "grant")
    {
        at = now();
        how = by;
    }
    else if (op == "revoke")
    {
        at = -1;
        how.clear();
    }
    else
        r.hold = op == "hold";
    checkUnlocks(account);
    saveSoon();
    return {true, "Account " + account + ": " + op + (tier.empty() || op == "hold" || op == "release" ? std::string() : " " + tier) + ".", {}};
}

json::Value Game::tiersView(const std::string& account) const
{
    // For the lobby and the sheet: each tier open or not (and how), and what it still needs, line by line.
    auto view = Value::object();
    const auto record = standing_.find(account);
    const standing::Record r = record == standing_.end() ? standing::Record{} : record->second;
    const auto known = measures_.find(account);
    const auto m = known == measures_.end() ? measuresOf(account) : known->second;
    for (const char* tier : {"gifted", "quickened"})
    {
        auto t = Value::object();
        const bool open = options_.openTiers || standing::open(r, tier);
        t.add("open", open);
        if (standing::open(r, tier))
            t.add("by", std::string(tier) == "gifted" ? r.giftedBy : r.quickenedBy);
        auto lines = Value::array();
        for (const auto& p : standing::progress(tier, m, standing::thresholds()))
        {
            auto line = Value::object();
            line.add("measure", p.measure);
            line.add("have", p.have);
            line.add("need", p.need);
            line.add("label", p.label);
            lines.push(line);
        }
        t.add("progress", lines);
        if (!open)
            t.add("message", standing::lockedMessage(tier, m, standing::thresholds()));
        view.add(tier, t);
    }
    if (r.hold)
        view.add("hold", true);
    return view;
}

json::Value Game::standingSave() const
{
    // One entry per account (game.account_standing, readable by the tools and the DM: it holds no verifiers).
    auto list = Value::array();
    for (const auto& [account, r] : standing_)
    {
        auto e = Value::object();
        e.add("account", account);
        auto ids = Value::array();
        for (const auto& id : accounts_.characters(account))
            ids.push(id);
        e.add("characters", ids);
        if (r.giftedAt >= 0)
        {
            e.add("giftedAt", r.giftedAt);
            e.add("giftedBy", r.giftedBy);
        }
        if (r.quickenedAt >= 0)
        {
            e.add("quickenedAt", r.quickenedAt);
            e.add("quickenedBy", r.quickenedBy);
        }
        if (r.hold)
            e.add("hold", true);
        if (const auto m = measures_.find(account); m != measures_.end())
        {
            auto measures = Value::object();
            measures.add("socialLevel", m->second.socialLevel);
            measures.add("normalScenes", m->second.normalScenes);
            measures.add("stars", m->second.stars);
            measures.add("starGivers", m->second.starGivers);
            measures.add("closedStories", m->second.closedStories);
            e.add("measures", measures);
        }
        list.push(e);
    }
    auto root = Value::object();
    root.add("accounts", list);
    return root;
}

void Game::standingLoad(const json::Value& saved)
{
    // What was kept, then every account counted again from the ledger (tiers already met open as earned).
    standing_.clear();
    measures_.clear();
    tiersViews_.clear();
    for (const auto& e : saved.array("accounts"))
    {
        const auto account = e.string("account");
        if (account.empty() || !accounts_.exists(account))
            continue;
        auto& r = standing_[account];
        r.account = account;
        r.giftedAt = wire::strictNumber(e, "giftedAt", -1);
        r.giftedBy = e.string("giftedBy").substr(0, 80);
        r.quickenedAt = wire::strictNumber(e, "quickenedAt", -1);
        r.quickenedBy = e.string("quickenedBy").substr(0, 80);
        r.hold = e.boolean("hold");
    }
    for (const auto& account : accounts_.usernames())
        checkUnlocks(account);
}

void Game::recordStar(const std::string& kind, const std::string& source, const std::string& giver, const std::string& recipient, int xp)
{
    // Into the star book, against the receiving account (doc 51, §1); whether it counts is the book's.
    stars::Star star;
    star.id = "star-" + guid().substr(0, 16);
    star.kind = kind;
    star.source = source;
    star.giverAccount = accountKey(giver);
    star.giverCharacter = giver;
    star.recipientAccount = accountKey(recipient);
    star.recipientCharacter = recipient;
    star.at = now();
    star.xp = xp;
    starBook_.record(star);
    socialViewsDirty_ = true;                      // (The giver's tag chips: doc 51, §2.)
    if (accounts_.exists(star.recipientAccount))
        checkUnlocks(star.recipientAccount);       // (Stars open Quickened: doc 49.)
    saveSoon();
}

json::Value Game::starsFor(const std::string& viewer, const std::string& target) const
{
    // Exact for the player themselves and a friend who sees which wolf is theirs (doc 51, §1: an exact count on a
    // stranger's card would tie the wolf to its player); bands for everyone else.
    const auto mine = accountKey(viewer), theirs = accountKey(target);
    const bool exact = mine == theirs || !sharedHandle(mine, target).empty();
    return starBook_.view(theirs, exact);
}
} // namespace ratw::game
