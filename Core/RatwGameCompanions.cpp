// Residents travelling with a party (Docs/Design/32-parties-chapters-factions.md, 2.3; the party rules are
// RatwParty.cpp): who may be asked along and why they would come (friendship or a wage), their orders, following the
// party, wages paid each game dawn, leaving of their own accord, fighting beside the party (never against the Watch),
// growing closer on the road, and a few words of their own as things happen.
#include "RatwGame.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
std::string lowered(std::string s)
{
    for (auto& ch : s)
        ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

// Trades that wander for a living: they come along at any hour, and ask more for it.
bool roving(const std::string& trade)
{
    const auto t = lowered(trade);
    for (const char* word : {"scout", "guide", "hunter", "travel", "pilgrim", "sellsword", "mercenary", "courier", "escort", "tracker"})
        if (t.find(word) != std::string::npos)
            return true;
    return false;
}

// A few words a companion says as things happen (written lines: no model, no cost). {place} is where they arrive.
const std::vector<std::string>& linesFor(const std::string& moment)
{
    static const std::map<std::string, std::vector<std::string>> lines{
        {"arrive", {"So this is {place}.", "{place}. I'd not thought to see it today.", "Mind your step in {place}.",
                    "{place}, then. Where to now?"}},
        {"danger", {"Careful. I don't like the look of them.", "Trouble ahead. Stay close.", "Keep your eyes open; we're watched."}},
        {"hurt", {"You're hurt. Let me look at that when we stop.", "That looks bad. Take it slower.", "Easy now. You're bleeding."}},
        {"quiet", {"Quiet road.", "Are we waiting on something?", "I could do with a meal before long.", "It's good to be out walking."}},
    };
    static const std::vector<std::string> none;
    const auto found = lines.find(moment);
    return found == lines.end() ? none : found->second;
}
} // namespace

std::string Game::whyNotJoin(const std::string& npcId) const
{
    const auto* e = world_.entity(npcId);
    const auto* life = world_.society().resident(npcId);
    if (!e || !e->npc || e->transient || !life)
        return "They aren't one to travel with anyone.";
    if (e->dead || e->downedLeft > 0 || e->state == "beaten down")
        return "They are in no state to travel.";
    if (parties_.of(npcId))
        return "They are already travelling with someone.";
    // Those who keep the town running stay at it: traders, the Watch, and the cook, keeper and forager.
    if (life->role != "resident" && life->role != "civilian")
        return "Their work keeps them here.";
    if (e->quickened || e->age < battle::YoungestFighter || world_.warrantFor(npcId) || world_.custodyOf(npcId))
        return "They can't come.";
    if (life->task == "sleep")
        return "They are asleep.";
    const auto* job = world_.society().jobOf(npcId);
    const auto* spec = world_.society().spec(npcId);
    const bool roams = roving(job ? job->title : spec ? spec->workLabel : std::string());
    if (job && !roams)
    {
        const double hour = world_.environmentAt(e->cellId).hour;
        const bool wraps = job->endHour < job->startHour;
        const bool working = wraps ? hour >= job->startHour || hour < job->endHour : hour >= job->startHour && hour < job->endHour;
        if (working)
            return "They have work to do until " + std::to_string(int(job->endHour) % 24) + ":00.";
    }
    std::size_t online = 0;
    for (const auto* c : clients_)
        online += !c->entityId.empty();
    if (parties_.companionCount() >= 4 + online / 4)
        return "No one else can be spared to travel just now.";
    return {};
}

std::int64_t Game::wageFor(const std::string& npcId) const
{
    const auto* job = world_.society().jobOf(npcId);
    const auto* spec = world_.society().spec(npcId);
    return roving(job ? job->title : spec ? spec->workLabel : std::string()) ? 10 : 6;
}

void Game::companionSays(const std::string& npcId, const std::string& words)
{
    ParsedPost post;
    post.ok = true;
    post.speech = true;
    post.segments.push_back({"speech", words});
    publish(npcId, post, Voice::Speak, {}, "party");
    npcLastSpeech_[npcId] = world_.time();
}

Result Game::askAlong(const std::string& playerId, const std::string& npcId, bool hire)
{
    auto* npc = world_.entity(npcId);
    const auto* player = world_.entity(playerId);
    if (!npc || !player || world_.visionClarity(playerId, npcId) <= 0 ||
        std::hypot(npc->position.x - player->position.x, npc->position.y - player->position.y) > 3)
        return {false, "Move closer to ask them.", npcId};
    if (const auto* mine = parties_.of(playerId); mine && mine->leader != playerId)
        return {false, "Only the party's leader can ask someone along.", npcId};
    if (const auto* mine = parties_.of(playerId); mine && mine->companions.size() >= party::MaxCompanions)
        return {false, "Your party already has as many travelling with it as it can look after.", npcId};
    if (const auto why = whyNotJoin(npcId); !why.empty())
        return {false, why, npcId};
    const auto* bond = world_.bonds().find(npcId, playerId);
    party::Companion c;
    c.id = npcId;
    c.joined = world_.calendarDays();
    if (hire)
    {
        // Paid a day ahead, purse to purse; again each dawn after.
        c.reason = "hired";
        c.wage = wageFor(npcId);
        if (bond && bond->trust < -20)
        {
            companionSays(npcId, "Not for any money. Not with you.");
            return {false, "They won't work for you.", npcId};
        }
        const auto* purse = world_.society().account(playerId);
        if (!purse || purse->cash < c.wage)
            return {false, "You can't pay the first day's " + std::to_string(c.wage) + " pennies.", npcId};
        if (!world_.society().shift(playerId, npcId, "", 0, c.wage, "companion wage"))
            return {false, "The coins wouldn't change hands.", npcId};
        c.paidThrough = std::floor(world_.calendarDays()) + 1;
        record(Economy, playerId);
    }
    else
    {
        // Out of friendship: they must like and trust you, and know you for someone worth following.
        c.reason = "friend";
        if (social_.level(playerId) < 3 || !bond || bond->affinity < 40 || bond->trust < 30)
        {
            companionSays(npcId, !bond || bond->familiarity < 10 ? "I hardly know you." : "I don't know you well enough for that.");
            return {false, "They won't come along as a friend. Not yet.", npcId};
        }
    }
    const auto outcome = parties_.addCompanion(playerId, c);
    if (!outcome.ok)
        return {false, outcome.message, npcId};
    npc->leaderId = "party:" + outcome.message;       // Off their schedule; the party leads (doc 32).
    world_.stop(npcId);
    companionSays(npcId, hire ? "Done. I'll walk with you." : "Lead on. I'll come.");
    tellParty(parties_.of(playerId)->members, names::capitalised(labelFor(playerId, npcId)) + " travels with the party now.", playerId);
    saveSoon();
    return {true, hire ? "They take your coin and come along." : "They agree to come along.", npcId};
}

Result Game::orderCompanion(const std::string& playerId, const std::string& npcId, const std::string& order)
{
    auto* c = parties_.companion(npcId);
    const auto* p = parties_.of(playerId);
    if (!c || !p || !parties_.together(playerId, npcId))
        return {false, "They aren't travelling with you.", npcId};
    if (c->by != playerId && p->leader != playerId)
        return {false, "They take that from whoever asked them along, or the leader.", npcId};
    if (order == "wait here")
    {
        c->waiting = true;
        world_.stop(npcId);
        companionSays(npcId, "I'll wait here.");
    }
    else if (order == "follow me")
    {
        c->waiting = false;
        companionSays(npcId, "Right behind you.");
    }
    else if (order == "go home" || order == "dismiss")
    {
        companionSays(npcId, order == "go home" ? "Home, then. Safe roads." : "Then this is where I leave you. Fare well.");
        parties_.releaseCompanion(npcId);
        releaseCompanions();
    }
    else
        return {false, "They don't follow that.", npcId};
    saveSoon();
    return {true, {}, npcId};
}

void Game::releaseCompanions()
{
    for (const auto& npcId : parties_.takeReleased())
        if (auto* npc = world_.entity(npcId))
        {
            // Back to their own life: their schedule walks them home or to work from where they stand.
            npc->leaderId.clear();
            npc->activity.clear();
            world_.stop(npcId);
            companionMoments_.erase(npcId);
            sharedDanger_.erase(npcId);
        }
}

void Game::companionLeaves(const std::string& npcId, const std::string& words, const std::string& why)
{
    const auto* p = parties_.of(npcId);
    if (!p)
        return;
    const auto members = p->members;
    companionSays(npcId, words);
    parties_.releaseCompanion(npcId);
    for (const auto& m : members)
        if (auto* c = clientOf(m))
            system(c, names::capitalised(labelFor(m, npcId)) + " leaves the party: " + why);
    releaseCompanions();
    saveSoon();
}

void Game::companionTick(double dt)
{
    releaseCompanions();
    companionAccumulator_ += dt;
    if (companionAccumulator_ < 0.25)
        return;
    const double step = companionAccumulator_;
    companionAccumulator_ = 0;
    const double day = world_.calendarDays();
    const double t = world_.time();
    std::vector<std::tuple<std::string, std::string, std::string>> leaving;   // npc, words, why
    // The journal and older saves know companions as resident → the one they follow.
    companionOwner_.clear();
    for (const auto& [partyId, p] : parties_.all())
    {
        // Whom they follow: the leader, if in the world; else whoever is.
        const Entity* leader = nullptr;
        if (clientOf(p.leader))
            leader = world_.entity(p.leader);
        for (const auto& m : p.members)
            if (!leader && clientOf(m))
                leader = world_.entity(m);
        int place = 0;
        for (const auto& c : p.companions)
        {
            companionOwner_[c.id] = c.by;
            auto* npc = world_.entity(c.id);
            if (!npc)
                continue;
            if (npc->dead)
            {
                leaving.emplace_back(c.id, "", "they have died.");
                continue;
            }
            npc->leaderId = "party:" + partyId;
            npc->activity = c.waiting ? "waiting for their party" : "travelling with a party";
            // Following, a little apart from each other.
            const double ox = place == 0 ? -0.9 : 0.9;
            ++place;
            if (leader && !c.waiting && !world_.inBattle(c.id) && npc->downedLeft <= 0 && npc->cellId == leader->cellId)
            {
                const double d = std::hypot(npc->position.x - leader->position.x, npc->position.y - leader->position.y);
                if (d > 2.7 && npc->path.empty())
                    world_.moveTo(npc->id, leader->position.x + ox, leader->position.y + 0.7);
                if (d < 1.4)
                    world_.stop(npc->id);
            }
            // Wages, each game dawn, from whoever hired them.
            if (c.reason == "hired" && std::floor(day) >= c.paidThrough)
            {
                if (world_.society().shift(c.by, c.id, "", 0, c.wage, "companion wage"))
                {
                    if (auto* mine = parties_.companion(c.id))
                        mine->paidThrough = std::floor(day) + 1;
                    record(Economy, c.by);
                    if (auto* cl = clientOf(c.by))
                        system(cl, "You pay " + labelFor(c.by, c.id) + " " + std::to_string(c.wage) + " pennies for another day.");
                }
                else
                    leaving.emplace_back(c.id, "No pay, no company. I'm going home.", "you couldn't pay them.");
                continue;
            }
            const auto* bond = world_.bonds().find(c.id, c.by);
            // A friend goes back to their own life after a day or two, unless the bond is very strong.
            if (c.reason == "friend" && day - c.joined > 2 && (!bond || bond->affinity < 70))
            {
                leaving.emplace_back(c.id, "I should get back to my own life. It's been good travelling with you.",
                                     "they have their own life to get back to.");
                continue;
            }
            // Falling out: a friend who no longer likes or trusts them, a hireling who no longer trusts them.
            if ((c.reason == "friend" && (!bond || bond->affinity < 20 || bond->trust < 10)) ||
                (c.reason == "hired" && bond && bond->trust < -20))
            {
                leaving.emplace_back(c.id, "I've had enough of this. I'm going.", "they've had enough.");
                continue;
            }
        }
    }
    // A crime by the party in front of them: they won't be part of it.
    const auto& incidents = world_.crime().incidents;
    for (const auto& inc : incidents)
    {
        if (seenIncidents_.count(inc.id))
            continue;
        seenIncidents_.insert(inc.id);
        if (const auto* p = parties_.of(inc.offender))
            for (const auto& c : p->companions)
                for (const auto& w : inc.witnesses)
                    if (w.id == c.id && w.identified)
                        leaving.emplace_back(c.id, "I won't be part of that. I'm done with you.", "they saw what was done.");
    }
    if (seenIncidents_.size() > 2000)
        seenIncidents_.clear();
    // Fighting beside the party: a party player's fight in sight, unless it is against the Watch.
    for (const auto& b : world_.battles())
    {
        if (b.over)
            continue;
        for (const auto& f : b.fighters)
        {
            if (f.status == "fled" || !clientOf(f.id))
                continue;
            const auto* p = parties_.of(f.id);
            if (!p)
                continue;
            bool watch = false;
            for (const auto& g : b.fighters)
                watch |= g.side != f.side && world_.guardOnDuty(g.id);
            for (const auto& c : p->companions)
            {
                const auto* npc = world_.entity(c.id);
                if (!npc || world_.inBattle(c.id) || b.fled.count(c.id) || npc->downedLeft > 0 || npc->cellId != b.cellId ||
                    world_.visionClarity(c.id, f.id) <= 0)
                    continue;
                if (watch)
                {
                    if (!warnedWatch_.count(c.id + "|" + b.id))
                    {
                        warnedWatch_.insert(c.id + "|" + b.id);
                        companionSays(c.id, "Not against the Watch. I won't.");
                    }
                    continue;
                }
                if (world_.joinBattle(c.id, b.id, f.side).ok)
                    sharedDanger_[c.id].insert(f.id);
            }
        }
    }
    // Danger shared: once their fight is over, they trust each other more.
    for (auto it = sharedDanger_.begin(); it != sharedDanger_.end();)
    {
        if (world_.inBattle(it->first))
        {
            ++it;
            continue;
        }
        for (const auto& player : it->second)
            world_.bonds().mutual(it->first, player, {1, 3, 1, 0, 1}, day);
        it = sharedDanger_.erase(it);
    }
    // Time on the road together: each game hour, they come to know each other.
    companionHours_ += step / calendar::SecondsPerDay * 24;
    if (companionHours_ >= 1)
    {
        companionHours_ = 0;
        for (const auto& [partyId, p] : parties_.all())
            for (const auto& c : p.companions)
                for (const auto& m : p.members)
                    if (const auto *a = world_.entity(c.id), *b = world_.entity(m); a && b && a->cellId == b->cellId && clientOf(m))
                        world_.bonds().mutual(c.id, m, {0.3, 0.2, 1, 0, 0}, day);
    }
    for (const auto& [npcId, words, why] : leaving)
        companionLeaves(npcId, words, why);
    companionRemarks(t);
}

void Game::companionRemarks(double t)
{
    // A few words as things happen, at most one a party every RemarkSeconds.
    for (const auto& [partyId, p] : parties_.all())
    {
        if (p.companions.empty() || t < remarkAt_[partyId])
            continue;
        const Entity* leader = world_.entity(p.leader);
        if (!leader || !clientOf(p.leader))
            continue;
        std::string speaker, moment, place;
        for (const auto& c : p.companions)
        {
            const auto* npc = world_.entity(c.id);
            if (!npc || npc->cellId != leader->cellId || npc->dead || npc->downedLeft > 0 || world_.inBattle(c.id))
                continue;
            auto& seen = companionMoments_[c.id];
            // Arriving somewhere new together.
            if (seen.cell != npc->cellId)
            {
                const bool first = seen.cell.empty();
                seen.cell = npc->cellId;
                if (!first)
                    if (const auto* cell = world_.cell(npc->cellId))
                    {
                        speaker = c.id, moment = "arrive", place = cell->name;
                        break;
                    }
            }
            // Trouble in sight: bandits.
            bool trouble = false;
            for (const Entity* e : world_.entitiesIn(npc->cellId))
                trouble |= e->transient && world_.hostile(e->id) && world_.visionClarity(c.id, e->id) > 0;
            if (trouble && !seen.trouble)
            {
                seen.trouble = true;
                speaker = c.id, moment = "danger";
                break;
            }
            seen.trouble = trouble;
            // A party mate badly hurt.
            for (const auto& m : p.members)
                if (const auto* e = world_.entity(m); e && e->cellId == npc->cellId && e->hurt >= 40 && e->downedLeft <= 0 &&
                                                       !seen.hurt.count(m))
                {
                    seen.hurt.insert(m);
                    speaker = c.id, moment = "hurt";
                    break;
                }
            if (!speaker.empty())
                break;
            for (auto it = seen.hurt.begin(); it != seen.hurt.end();)
                if (const auto* e = world_.entity(*it); !e || e->hurt < 20)
                    it = seen.hurt.erase(it);
                else
                    ++it;
        }
        // A long quiet with everyone together.
        if (speaker.empty() && t - lastPartySpeech_[partyId] > QuietSeconds && t - remarkAt_[partyId] > QuietSeconds)
            for (const auto& c : p.companions)
                if (const auto* npc = world_.entity(c.id); npc && npc->cellId == leader->cellId && !npc->dead && !world_.inBattle(c.id))
                {
                    speaker = c.id, moment = "quiet";
                    break;
                }
        if (speaker.empty())
            continue;
        const auto& lines = linesFor(moment);
        if (lines.empty())
            continue;
        std::string line = lines[(std::hash<std::string>{}(speaker + moment + place) + std::uint64_t(t)) % lines.size()];
        if (const auto at = line.find("{place}"); at != std::string::npos)
            line.replace(at, 7, place);
        companionSays(speaker, line);
        remarkAt_[partyId] = t + RemarkSeconds;
        lastPartySpeech_[partyId] = t;
    }
}

std::string Game::companionContext(const std::string& npcId) const
{
    const auto* c = parties_.companion(npcId);
    const auto* p = parties_.of(npcId);
    if (!c || !p)
        return {};
    std::string out = " You are travelling with a party";
    out += c->reason == "hired" ? " as hired help, at " + std::to_string(c->wage) + " pennies a day." : " as a friend.";
    std::string who;
    for (const auto& m : p->members)
    {
        who += (who.empty() ? "" : "; ") + labelFor(npcId, m);
        if (const auto regard = world_.bonds().describe(npcId, m, labelFor(npcId, m)); !regard.empty())
            who += " (" + regard + ")";
    }
    out += " With you: " + who + ".";
    if (!p->goal.empty())
        out += " The party is bound for: " + p->goal + ".";
    if (c->waiting)
        out += " You were asked to wait here.";
    return out;
}

void Game::adoptOldCompanions(const std::map<std::string, std::string>& owners)
{
    // Saves from before parties knew residents: each one following a player joins that player's party as a friend.
    for (const auto& [npcId, owner] : owners)
    {
        if (owner.empty() || parties_.of(npcId))
            continue;
        party::Companion c;
        c.id = npcId;
        c.reason = "friend";
        c.joined = world_.calendarDays();
        const auto* mine = parties_.of(owner);
        const auto outcome = parties_.addCompanion(mine && mine->leader != owner ? mine->leader : owner, c);
        if (outcome.ok)
            if (auto* npc = world_.entity(npcId))
                npc->leaderId = "party:" + outcome.message;
    }
}
} // namespace ratw::game
