// The ambient director (RatwAmbient.h; Docs/Design/26-living-npcs.md, Phase 10). World members, kept here.
#include "RatwAmbient.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>

namespace ratw
{
namespace
{
// Placeholder numbers, to be tuned with play.
constexpr double PlaceEvery = 180;                  // World seconds between exchanges in one place...
constexpr double EachEvery = 600;                   // ...and for any one resident.
constexpr double Together = 3;                      // Tiles apart, at most, to talk.
constexpr double Earshot = .35;                     // How clearly a player must hear them.
constexpr std::size_t Nearest = 12;                 // Of those standing about, the nearest to a player are considered.
constexpr double NewsDays = 2, GossipDays = 4;
constexpr std::size_t NewsKept = 4;

double apart(Vec2 a, Vec2 b) { return std::hypot(a.x - b.x, a.y - b.y); }
} // namespace

std::string World::describeRegard(const std::string& holder, const std::string& other) const
{
    const auto* o = entity(other);
    return bonds_.describe(holder, other, o ? o->name : other);
}

void World::noteNews(const WorldEvent& e)
{
    const auto name = [&](const std::string& id) {
        const auto* who = entity(id);
        return who ? who->name : id;
    };
    const auto tell = [&](const std::string& who, const std::string& kind, const std::string& text) {
        const auto* person = entity(who);
        if (!person || !person->npc || person->transient)
            return;
        auto& mine = news_[who];
        mine.push_back({calendarDays_, kind, text});
        if (mine.size() > NewsKept)
            mine.erase(mine.begin());
    };
    const auto& a = e.actor;
    const auto& t = e.target;
    if (e.kind == "marriage")
    {
        tell(a, "marriage", name(a) + " married " + name(t));
        tell(t, "marriage", name(t) + " married " + name(a));
    }
    else if (e.kind == "mourning")
        tell(a, "loss", name(t) + " died, and " + name(a) + " mourns them (" + e.detail + ")");
    else if (e.kind == "apprenticeship")
    {
        tell(a, "work", name(a) + " became apprentice to " + name(t) + (e.detail.empty() ? "" : ", " + e.detail));
        tell(t, "work", name(t) + " took " + name(a) + " on as an apprentice");
    }
    else if (e.kind == "succession")
        tell(a, "work", name(a) + " took over the work of " + e.detail.substr(0, e.detail.find(':')));
    else if (e.kind == "theft")
        tell(t, "crime", name(a) + " robbed " + name(t));
    else if (e.kind == "assault")
        tell(t, "crime", name(a) + " attacked " + name(t));
    else if (e.kind == "arrest")
        tell(a, "crime", name(a) + " of the watch took " + name(t) + " to the gaol");
}

AmbientTopic World::ambientTopic(const std::string& a, const std::string& b)
{
    const auto* ea = entity(a);
    const auto* eb = entity(b);
    AmbientTopic best;
    if (!ea || !eb)
        return best;
    const auto name = [&](const std::string& id) {
        const auto* who = entity(id);
        return who ? who->name : id;
    };
    const auto bond = [&](const std::string& h, const std::string& o) {
        const auto* found = bonds_.find(h, o);
        return found ? *found : Bond{};
    };
    const Bond ab = bond(a, b), ba = bond(b, a);
    // The day, and the place, as either would see them.
    std::vector<std::string> day;
    const auto plan = dayPlan(communityOf(ea->cellId));
    day.push_back("It is " + calendar::weekdayName(calendar::weekdayOf(calendarDays_)) +
                  (plan.kind == "festival" ? ", the day of " + plan.name + " (a festival)"
                   : plan.kind == "market" ? ", market day"
                   : plan.kind == "rest"   ? ", the day of rest"
                                           : "") + ".");
    if (const auto* c = cell(ea->cellId))
    {
        day.push_back("They are in " + c->name + ".");
        if (c->outdoors && skyOf(c->id) >= 1)
            day.push_back(c->weather == Weather::Rain ? "It is raining." : "The weather is foul.");
    }
    // `source` names who opens: the one who knows (gossip, news) or bears the grudge.
    const auto consider = [&](AmbientTopic topic) {
        topic.facts.insert(topic.facts.end(), day.begin(), day.end());
        if (topic.score > best.score)
            best = std::move(topic);
    };
    // A rumour one has heard lately and the other hasn't: about anyone but the two of them, players included.
    for (const auto& [teller, listener] : {std::pair{a, b}, std::pair{b, a}})
        if (const auto* heard = beliefsOf(teller))
            for (const auto& belief : *heard)
            {
                if (belief.subject == teller || belief.subject == listener || belief.confidence < .45 ||
                    calendarDays_ - belief.day > GossipDays)
                    continue;
                bool known = false;
                if (const auto* theirs = beliefsOf(listener))
                    for (const auto& other : *theirs)
                        known |= other.subject == belief.subject && other.claim == belief.claim;
                if (known)
                    continue;
                const auto* about = entity(belief.subject);
                AmbientTopic t;
                t.kind = "gossip";
                t.subject = belief.subject;
                t.claim = belief.claim;
                t.source = teller;
                t.incident = belief.incident;
                t.confidence = belief.confidence;
                t.score = 3 + belief.confidence + (about && !about->npc ? .5 : 0);
                t.facts.push_back(name(teller) + " has heard that " + name(belief.subject) + " " + belief.claim + " (" +
                                  (belief.source == "saw it" ? name(teller) + " saw it"
                                                             : "from " + name(belief.source)) + "), and tells " +
                                  name(listener) + ".");
                if (about)
                    t.facts.push_back(name(belief.subject) + " is " + (about->npc ? "someone they know of" : "a traveller") + ".");
                consider(std::move(t));
            }
    // News from one's own life, told to the other.
    for (const auto& [teller, listener] : {std::pair{a, b}, std::pair{b, a}})
        if (const auto found = news_.find(teller); found != news_.end())
            for (const auto& n : found->second)
            {
                if (calendarDays_ - n.day > NewsDays)
                    continue;
                AmbientTopic t;
                t.kind = "news";
                t.source = teller;
                t.claim = n.kind;
                t.score = 2.8 + (n.kind == "loss" || n.kind == "crime" ? .2 : 0);
                t.facts.push_back(n.text + ".");
                t.facts.push_back(name(teller) + " tells " + name(listener) + " about it.");
                consider(std::move(t));
            }
    // An old grudge between them.
    if (std::min(ab.affinity, ba.affinity) <= -25)
    {
        AmbientTopic t;
        t.kind = "quarrel";
        t.source = ab.affinity <= ba.affinity ? a : b;
        t.score = 2.5 + std::abs(std::min(ab.affinity, ba.affinity)) / 100;
        t.facts.push_back(name(a) + " and " + name(b) + " do not get on.");
        for (const auto& [h, o] : {std::pair{a, b}, std::pair{b, a}})
        {
            if (const auto words = describeRegard(h, o); !words.empty())
                t.facts.push_back("How " + name(h) + " sees it: " + words);
            if (const auto* heard = beliefsOf(h))
                for (const auto& belief : *heard)
                    if (belief.subject == o && belief.confidence >= .35)
                        t.facts.push_back(name(h) + " believes " + name(o) + " " + belief.claim + ".");
        }
        consider(std::move(t));
    }
    // Friends, passing the time.
    if (ab.familiarity >= 40 && ab.affinity >= 20 && ba.affinity >= 10)
    {
        AmbientTopic t;
        t.kind = "friends";
        t.source = a;
        t.score = 1.5;
        t.facts.push_back(name(a) + " and " + name(b) + " are friends.");
        consider(std::move(t));
    }
    // Acquaintances, and a day worth remarking on.
    if (ab.familiarity >= 10 && (plan.kind != "work" || (cell(ea->cellId) && skyOf(ea->cellId) >= 1)))
    {
        AmbientTopic t;
        t.kind = "day";
        t.source = a;
        t.score = 1 + (plan.kind == "festival" ? .3 : 0);
        t.facts.push_back(name(a) + " and " + name(b) + " know each other.");
        consider(std::move(t));
    }
    return best;
}

std::vector<AmbientPick> World::ambientPicks(const std::vector<std::string>& listeners, const std::set<std::string>& busy)
{
    std::vector<AmbientPick> picks;
    std::set<std::string> cellsDone;
    const auto recent = [&](const std::string& key, double every) {
        const auto found = ambientLast_.find(key);
        return found != ambientLast_.end() && time_ - found->second < every;
    };
    const auto free = [&](const Entity& e) {
        if (!e.npc || e.dead || e.transient || e.offstage || e.state == "beaten down" || !e.leaderId.empty() ||
            custodyOf(e.id) || busy.count(e.id) || e.speakingUntil > time_ || recent(e.id, EachEvery) ||
            !e.path.empty() || std::hypot(e.velocity.x, e.velocity.y) > .05)
            return false;
        const auto* life = society_.resident(e.id);
        if (!life)
            return false;
        static const std::set<std::string> occupied{"sleep", "patrol", "companion", "relocate", "held in the gaol",
                                                    "looking for a chance", "guarding a caravan", "carrying a letter"};
        return !occupied.count(life->task) && life->task.rfind("stopping ", 0) != 0;
    };
    for (const auto& listenerId : listeners)
    {
        const auto* listener = entity(listenerId);
        if (!listener || listener->npc || listener->dead || cellsDone.count(listener->cellId) ||
            recent("cell:" + listener->cellId, PlaceEvery))
            continue;
        std::vector<const Entity*> near;
        for (const auto& [id, e] : entities_)
            if (e.cellId == listener->cellId && free(e) && hearingClarity(listenerId, id, Voice::Speak) >= Earshot)
                near.push_back(&e);
        std::sort(near.begin(), near.end(), [&](const Entity* x, const Entity* y) {
            return apart(x->position, listener->position) < apart(y->position, listener->position) ||
                   (apart(x->position, listener->position) == apart(y->position, listener->position) && x->id < y->id);
        });
        if (near.size() > Nearest)
            near.resize(Nearest);
        AmbientPick best;
        for (std::size_t i = 0; i < near.size(); ++i)
            for (std::size_t j = i + 1; j < near.size(); ++j)
            {
                if (apart(near[i]->position, near[j]->position) > Together)
                    continue;
                auto topic = ambientTopic(near[i]->id, near[j]->id);
                if (topic.score <= best.topic.score)
                    continue;
                const bool first = topic.source.empty() || topic.source == near[i]->id;
                best = {first ? near[i]->id : near[j]->id, first ? near[j]->id : near[i]->id, listener->cellId, std::move(topic)};
            }
        if (best.topic.score > 0)
        {
            cellsDone.insert(best.cell);
            picks.push_back(std::move(best));
        }
    }
    std::sort(picks.begin(), picks.end(), [](const AmbientPick& x, const AmbientPick& y) { return x.topic.score > y.topic.score; });
    return picks;
}

void World::ambientSpoken(const AmbientPick& pick)
{
    ambientLast_[pick.teller] = ambientLast_[pick.listener] = ambientLast_["cell:" + pick.cell] = time_;
    for (auto it = ambientLast_.begin(); it != ambientLast_.end();)
        it = time_ - it->second > 3600 ? ambientLast_.erase(it) : std::next(it);
    // What talk does (the words only perform it): gossip passed on is believed, a grudge aired sours, friends warm.
    const auto& t = pick.topic;
    if (t.kind == "gossip")
    {
        believe(pick.listener, t.subject, t.claim, pick.teller, t.confidence * .7, t.incident);
        bonds_.change(pick.listener, pick.teller, {.3, 0, 1, 0, 0}, calendarDays_);
    }
    else if (t.kind == "quarrel")
        for (const auto& [h, o] : {std::pair{pick.teller, pick.listener}, std::pair{pick.listener, pick.teller}})
            bonds_.change(h, o, {-1.5, -.5, .5, 0, 0}, calendarDays_);
    else
        for (const auto& [h, o] : {std::pair{pick.teller, pick.listener}, std::pair{pick.listener, pick.teller}})
            bonds_.change(h, o, {.5, .2, 1, 0, 0}, calendarDays_);
    recordEvent({t.kind == "quarrel" ? "quarrel" : "conversation", pick.teller, pick.listener, pick.cell, 0, 0, {}, 0, 0, t.kind});
}
} // namespace ratw
