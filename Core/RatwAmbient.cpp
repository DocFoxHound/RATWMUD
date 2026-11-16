// The ambient director (RatwAmbient.h; Docs/Design/26-living-npcs.md, Phase 10). World members, kept here.
#include "RatwAmbient.h"
#include "RatwWorld.h"

#include "RatwClaims.h"
#include "RatwTopics.h"
#include "RatwItems.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace ratw
{
namespace
{
// Placeholder numbers, to be tuned with play.
// World seconds between exchanges in one place: busier where more people stand about (doc 30)...
constexpr double PlaceEveryQuiet = 180, PlaceEveryBusy = 100, PlaceEveryCrowd = 60;
constexpr double EachEvery = 300;                   // ...and for any one resident.
constexpr double Together = 3;                      // Tiles apart, at most, to talk.
constexpr double Earshot = .35;                     // How clearly a player must hear them.
constexpr std::size_t Nearest = 12;                 // Of those standing about, the nearest to a player are considered.
constexpr double NewsDays = 2, GossipDays = 4, TownNewsDays = 4;
constexpr std::size_t NewsKept = 4, TownNewsKept = 24;

double apart(Vec2 a, Vec2 b) { return std::hypot(a.x - b.x, a.y - b.y); }

std::string lowered(std::string s)
{
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// A place in the talk library's words (here.place, Data/Talk/facts.json), from what it is called and whether it is
// outdoors.
std::string placeKind(const Cell& c, bool town)
{
    const auto name = lowered(c.name + " " + c.id);
    const auto has = [&](std::initializer_list<const char*> words) {
        for (const char* w : words)
            if (name.find(w) != std::string::npos)
                return true;
        return false;
    };
    if (c.outdoors)
        return has({"market", "plaza", "square"}) ? "market" : has({"gate"}) ? "gate" : has({"field", "farm"}) ? "field"
               : has({"quay", "wharf", "harbour", "shore", "pier", "dock"}) ? "shore" : town ? "street" : "wild";
    if (has({"inn", "tavern", "anvil", "cup", "lantern", "barrel", "fire", "oar", "ford house", "still water", "alehouse"}))
        return "tavern";
    if (has({"chapel", "shrine", "cathedral", "temple", "church"}))
        return "chapel";
    if (has({"barracks", "watch", "guard", "garrison", "toll post", "gaol"}))
        return "barracks";
    if (has({"hall", "court", "council", "palace", "refectory"}))
        return "hall";
    if (has({" house", "home", "tenement", "farm", "shack", "row", "court", "yard", "stair", "rookery", "warrens", "sinks"}))
        return "home";
    return "work";
}

// What a speaker would call a stranger they see: by their coat, never their name.
std::string strangerWords(const Appearance& a)
{
    const std::string coat = a.species == "arctic" ? "pale" : a.species == "red" ? "red" : a.species == "maned" ? "long-legged"
                             : a.species == "ethiopian" ? "russet" : "grey";
    return "that " + coat + " stranger";
}

std::string townWords(const std::string& id)
{
    std::string out;
    bool up = true;
    for (char c : id)
    {
        if (c == '_')
        {
            out += ' ';
            up = true;
            continue;
        }
        out += up ? char(std::toupper(static_cast<unsigned char>(c))) : c;
        up = false;
    }
    if (out.rfind("Northern ", 0) == 0 || out.rfind("Isle ", 0) == 0 || out.rfind("Dark ", 0) == 0)
        out = "the " + out;
    return out;
}
} // namespace

std::string World::placeKindOf(const std::string& cellId) const
{
    const auto* c = cell(cellId);
    return c ? placeKind(*c, townOf(cellId) != nullptr) : std::string();
}

std::string World::townIdOf(const std::string& cellId) const
{
    if (const auto* t = townOf(cellId))
        return t->id;
    const auto* c = cell(cellId);
    return c ? c->region : std::string();
}

std::string World::describeRegard(const std::string& holder, const std::string& other) const
{
    const auto* o = entity(other);
    return bonds_.describe(holder, other, o ? o->name : other);
}

void World::tellNews(const std::string& who, const std::string& kind, const std::string& text, const std::string& about)
{
    const auto* person = entity(who);
    if (!person || !person->npc || person->transient || claims::rawId(text))
        return;
    auto& mine = news_[who];
    mine.push_back({calendarDays_, kind, text, about});
    if (mine.size() > NewsKept)
        mine.erase(mine.begin());
}

void World::victimNews(const Incident& inc, bool identified)
{
    if (inc.kind != "theft" && inc.kind != "assault")
        return;                                     // (A try that failed is the town's talk, not the victim's news.)
    const auto* doer = entity(inc.offender);
    const auto* victim = entity(inc.victim);
    if (!victim)
        return;
    // Named only if the victim saw who it was (witness(): identified), a traveller only as the victim knows them (doc 56).
    std::string by = "someone";
    if (identified && doer)
    {
        by = doer->npc ? doer->name : namer_ ? namer_(inc.victim, inc.offender) : doer->name;
        if (by.empty())
            by = "a stranger";
    }
    tellNews(inc.victim, "crime", by + (inc.kind == "theft" ? " robbed " : " attacked ") + victim->name,
             identified ? inc.offender : std::string());
}

void World::noteNews(const WorldEvent& e)
{
    const auto name = [&](const std::string& id) {
        const auto* who = entity(id);
        return who ? who->name : std::string("someone");   // (Never an id: doc 84, defect 1.)
    };
    const auto tell = [&](const std::string& who, const std::string& kind, const std::string& text, const std::string& about) {
        tellNews(who, kind, text, about);
    };
    const auto& a = e.actor;
    const auto& t = e.target;
    // What the town talks of (doc 30): kept by town, a few days, whoever was there.
    const auto town = [&](const std::string& who) {
        const auto* p = entity(who);
        return !e.cell.empty() ? townIdOf(e.cell) : p ? townIdOf(p->cellId) : std::string();
    };
    const auto talkOf = [&](const std::string& kind, const std::string& subject, const std::string& other) {
        const auto where = town(subject);
        if (where.empty())
            return;
        auto& list = townNews_[where];
        list.push_back({calendarDays_, kind, subject, other});
        if (list.size() > TownNewsKept)
            list.erase(list.begin());
    };
    if (e.kind == "marriage")
        talkOf("marriage", a, t);
    else if (e.kind == "mourning")
        talkOf("death", t, a);
    else if (e.kind == "apprenticeship")
        talkOf("apprentice", a, t);
    else if (e.kind == "succession")
        talkOf("succession", a, {});
    else if (e.kind == "theft" || e.kind == "assault")
        talkOf(e.kind, a, t);
    else if (e.kind == "arrest")
        talkOf("arrest", t, a);
    else if (e.kind == "relocation" || e.kind == "newcomer")
        talkOf("newcomer", a, {});                  // (One come in from the Country is news too: doc 84, defect 10.)
    else if (e.kind == "birth" && !t.empty())
        talkOf("birth", t, a);                      // (The parent's pup: the scenes say "#topic.subject#'s pup".)
    // (Each told never to the other it is about: "I married Orm" isn't news to Orm.)
    if (e.kind == "marriage")
    {
        tell(a, "marriage", name(a) + " married " + name(t), t);
        tell(t, "marriage", name(t) + " married " + name(a), a);
    }
    else if (e.kind == "mourning")
        tell(a, "loss", name(a) + " lost " + name(t), t);  // (The teller first, as it's said: "I lost Bran".)
    else if (e.kind == "apprenticeship")
    {
        tell(a, "work", name(a) + " became apprentice to " + name(t) + (e.detail.empty() ? "" : ", " + e.detail), t);
        tell(t, "work", name(t) + " took " + name(a) + " on as an apprentice", a);
    }
    else if (e.kind == "succession")
        tell(a, "work", name(a) + " took over the work of " + e.detail.substr(0, e.detail.find(':')), {});
    else if (e.kind == "arrest")
        tell(a, "crime", name(a) + " took " + name(t) + " to the gaol for the Watch", t);
    // (A theft or an attack is the victim's own news only once they have noticed it, and names the one who did it only if
    // they saw who: victimNews, from witness(). It is recorded before anyone has looked.)
}

AmbientTopic World::ambientTopic(const std::string& a, const std::string& b)
{
    // The best of what the two might talk of (the first of the best, in the order weighed): the director's pick.
    AmbientTopic best;
    weighTopics(a, b, [&](AmbientTopic t) {
        if (t.score > best.score)
            best = std::move(t);
    });
    // What the written scenes need of any topic: who the two are to each other, the place, the hour, the town.
    if (!best.kind.empty())
    {
        best.talk["pair.band"] = pairBand(a, b);
        talkMoment(a, best.talk);
    }
    return best;
}

std::vector<AmbientTopic> World::agendaFor(const std::string& a, const std::string& b)
{
    // Doc 84 §8.5 (Phase 5): everything the two might talk of, the likeliest first (ties as weighed), each owned by the
    // one who'd raise it (`source`).
    std::vector<AmbientTopic> all;
    weighTopics(a, b, [&](AmbientTopic t) {
        if (t.score > 0)
            all.push_back(std::move(t));
    });
    std::stable_sort(all.begin(), all.end(), [](const AmbientTopic& x, const AmbientTopic& y) { return x.score > y.score; });
    return all;
}

void World::weighTopics(const std::string& a, const std::string& b, const std::function<void(AmbientTopic)>& take)
{
    const auto* ea = entity(a);
    const auto* eb = entity(b);
    if (!ea || !eb)
        return;
    const auto name = [&](const std::string& id) {
        const auto* who = entity(id);
        return who ? who->name : std::string();     // (Never the id: doc 84, defect 1.)
    };
    const auto bond = [&](const std::string& h, const std::string& o) {
        const auto* found = bonds_.find(h, o);
        return found ? *found : Bond{};
    };
    const Bond ab = bond(a, b), ba = bond(b, a);
    // The scores are topics.json's (doc 84 §7.3, Phase 5; today's numbers for a library without one).
    const auto& book = topics::book();
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
        if (c->outdoors && skyAt(c->id, ea->position) >= 1)
            day.push_back(weatherAt(c->id, ea->position).kind == Weather::Rain ? "It is raining." : "The weather is foul.");
    }
    // What cools a topic or warms it (doc 84 §8.5, Phase 5; topics.json, off in today's numbers): the same two on the
    // same thing lately, the same incident talked of all day in this place, the everyday floor where a place's talk has
    // been news, a speaker's favourites; and nothing told twice to one listener (the ledger's told set).
    const auto& ledger = talkLedger_;
    const auto here = ea->cellId;
    const double share = book.floorBoost != 0 && book.floorWindow > 0 ? ledger.newsShare(here, book.floorWindow) : -1;
    const bool floor = share > book.floorOver;
    std::vector<std::string> favA, favB;
    if (book.favourite != 0 && talkFavourites_)
        favA = talkFavourites_(a), favB = talkFavourites_(b);
    const auto told = [&](const std::string& listener, const std::string& key) { return book.toldOnce && ledger.wasTold(listener, key); };
    double closeness = 0;
    if (!book.pairBonus.empty())
        if (const auto found = book.pairBonus.find(pairBand(a, b)); found != book.pairBonus.end())
            closeness = found->second;
    // `source` names who opens: the one who knows (gossip, news) or bears the grudge.
    const auto consider = [&](AmbientTopic topic) {
        topic.facts.insert(topic.facts.end(), day.begin(), day.end());
        if (topic.key.empty())
            topic.key = topic.kind;
        const bool everyday = book.everyday(topic.kind);
        topic.score = topic.score * book.weight(topic.kind);
        if (book.noveltyFactor != 1 && book.noveltyDays > 0 &&
            ledger.talkedOf(a, b, topic.kind, everyday ? std::string() : topic.key, calendarDays_ - book.noveltyDays))
            topic.score *= book.noveltyFactor;
        if (book.saturationPerDay > 0 && !everyday && ledger.todayCount(here, topic.key, calendarDays_) >= book.saturationPerDay)
            topic.score *= book.saturationFactor;
        if (floor && everyday)
            topic.score += book.floorBoost;
        topic.score += closeness;
        if (book.favourite != 0)
        {
            const bool fa = std::find(favA.begin(), favA.end(), topic.kind) != favA.end();
            const bool fb = std::find(favB.begin(), favB.end(), topic.kind) != favB.end();
            if (fa || fb)
            {
                topic.score += book.favourite;
                if (!fa && everyday)
                    topic.source = b;               // (The one whose favourite it is brings it up.)
            }
        }
        if (onTopic)
            onTopic(a, b, topic);
        take(std::move(topic));
    };
    // A rumour one has heard lately and the other hasn't: about anyone but the two of them, players included. Said in
    // claims.json's words from the teller's point of view (doc 84 §7.7): never a raw id (defect 1), never to or by its
    // victim as gossip (defect 2: the one robbed tells it as their own news), never of the teller's own kin.
    const auto whoIs = [&](const std::string& teller, const std::string& id) -> std::string {
        const auto* e = entity(id);
        if (!e)
            return {};
        if (e->npc)
            return e->name;
        const auto known = namer_ ? namer_(teller, id) : e->name;   // (A player as the teller knows them: doc 56, 6.)
        return known.empty() ? strangerWords(e->appearance) : known;
    };
    for (const auto& [teller, listener] : {std::pair{a, b}, std::pair{b, a}})
        if (const auto* heard = beliefsOf(teller))
            for (const auto& held : *heard)
            {
                if (held.subject == teller || held.subject == listener || held.confidence < .45 ||
                    calendarDays_ - held.day > GossipDays || held.claim.rfind("deed:", 0) == 0)
                    continue;                       // (Deeds are talked of in their own words: doc 56.)
                auto belief = held;
                if (belief.kind.empty())
                    claimOf(belief);                // (Not yet filled: a table set after the load.)
                if (!belief.object.empty() && (belief.object == teller || belief.object == listener))
                    continue;
                if (const auto* spouse = society_.spouse(teller); (spouse && *spouse == belief.subject) || society_.family(teller, belief.subject))
                    continue;
                // A camp heard of is talked of as there now: only while it is (one cleared stays in the carters' talk,
                // and the lines say "they're camped out by..."; the slice-end review).
                if (belief.kind.rfind("bandits.", 0) == 0 &&
                    std::none_of(roads_.camps.begin(), roads_.camps.end(),
                                 [&](const BanditCamp& c) { return c.id == belief.subject && c.active; }))
                    continue;
                bool known = false;
                if (const auto* theirs = beliefsOf(listener))
                    for (const auto& other : *theirs)
                        known |= other.subject == belief.subject && other.claim == belief.claim;
                // (With the tuned book, a listener who holds it in other words, or by its incident or its kind about the
                // same victim, knows it too: never "Never!" to one who'd heard: T5.)
                if (known || told(listener, belief.subject + "|" + belief.claim) ||
                    (book.toldOnce && holding(listener, belief.subject, belief.claim, belief.incident, belief.kind, belief.object)))
                    continue;
                const auto* about = entity(belief.subject);
                claims::Words w;
                w.xName = whoIs(teller, belief.subject);
                w.xFirst = about && about->npc ? w.xName.substr(0, w.xName.find(' ')) : w.xName;
                if (!belief.object.empty())
                {
                    w.yName = whoIs(teller, belief.object);
                    const auto* object = entity(belief.object);
                    w.yFirst = object && object->npc ? w.yName.substr(0, w.yName.find(' ')) : w.yName;
                }
                w.place = placeWords(belief.place);
                if (const auto at = belief.claim.find(" for "); belief.claim.rfind("is wanted", 0) == 0 && at != std::string::npos)
                    w.charge = belief.claim.substr(at + 5);
                w.text = belief.claim;
                std::string said, predicate, topicKind = "gossip", sceneKind;
                if (claims::loaded())
                {
                    const auto* k = claims::find(belief.kind);
                    if (!k)
                        continue;                   // (A claim with no kind in claims.json is never spoken.)
                    said = claims::said(belief.kind, "they", w);
                    predicate = claims::predicate(belief.kind, w);
                    topicKind = k->topic;
                    sceneKind = k->topicKind;
                    if (said.empty() || (topicKind == "gossip" && (predicate.empty() || w.xName.empty())))
                        continue;
                }
                else
                {
                    // (No table: a claim about someone named, in its own free words after the name, as before.)
                    if (w.xName.empty() || claims::rawId(belief.claim))
                        continue;
                    predicate = belief.claim;
                    said = w.xName + " " + belief.claim;
                }
                AmbientTopic t;
                t.kind = topicKind;
                t.subject = belief.subject;
                t.claim = belief.claim;
                t.source = teller;
                t.incident = belief.incident;
                t.confidence = belief.confidence;
                const auto& sc = book.topics.count(topicKind) && book.topics.at(topicKind).kinds.count("claim")
                                     ? book.score(topicKind, "claim") : book.score("gossip", "claim");
                t.score = sc.base + belief.confidence + (about && !about->npc ? sc.add : 0);
                t.key = belief.subject + "|" + belief.claim;
                t.said = said;
                t.passes = true;
                t.claimKind = belief.kind;
                t.object = belief.object;
                t.place = belief.place;
                t.seen = belief.source == "saw it" ? "yes" : "no";
                t.talk["topic.said"] = said;
                t.talk["topic.claim_kind"] = claims::family(belief.kind, belief.claim);
                if (topicKind == "gossip")
                {
                    t.talk["topic.subject"] = w.xName;
                    t.talk["topic.claim"] = predicate;   // (For the lines written "#topic.subject# #topic.claim#".)
                    if (!w.yName.empty())
                        t.talk["topic.victim"] = w.yName;
                    talkSexes(belief.subject, belief.object, t.talk);   // (#topic.they#: Phase 6.)
                }
                else
                {
                    if (!sceneKind.empty())
                        t.talk["topic.kind"] = sceneKind;
                    if (const auto near = placeNear(belief.place); !near.empty())
                        t.talk["topic.where"] = near;
                }
                const auto source = belief.source == "saw it" ? name(teller) + " saw it"
                                    : entity(belief.source)   ? "from " + name(belief.source)
                                                              : belief.source;
                t.facts.push_back(name(teller) + " has heard that " + said + " (" + source + "), and tells " + name(listener) + ".");
                if (about && topicKind == "gossip")
                    t.facts.push_back(w.xName + " is " + (about->npc ? "someone they know of" : "a traveller") + ".");
                consider(std::move(t));
            }
    // A deed one has heard of and the other hasn't (doc 56, 5): told in its own words, the doer named as the teller
    // knows them.
    if (deedWords_)
        for (const auto& [teller, listener] : {std::pair{a, b}, std::pair{b, a}})
            if (const auto* heard = beliefsOf(teller))
                for (const auto& belief : *heard)
                {
                    if (belief.claim.rfind("deed:", 0) != 0 || belief.subject == listener || belief.confidence < .45 ||
                        calendarDays_ - belief.day > GossipDays)
                        continue;
                    bool known = false;
                    if (const auto* theirs = beliefsOf(listener))
                        for (const auto& other : *theirs)
                            known |= other.claim == belief.claim;
                    if (known || told(listener, belief.subject + "|" + belief.claim))
                        continue;
                    const auto words = deedWords_(teller, belief.claim, belief.subject);
                    if (words.phrase.empty())
                        continue;
                    AmbientTopic t;
                    t.kind = "deed";
                    t.subject = belief.subject;
                    t.claim = belief.claim;
                    t.source = teller;
                    t.confidence = belief.confidence;
                    const auto& sc = book.score("deed");
                    t.score = sc.base + belief.confidence * sc.scale;
                    t.key = belief.subject + "|" + belief.claim;
                    t.talk["topic.known"] = words.byName ? "yes" : "no";
                    t.talk["topic.has_nickname"] = words.nickname.empty() ? "no" : "yes";
                    if (words.byName)
                        t.as = words.subject;
                    t.talk["topic.subject"] = words.subject;
                    talkSexes(belief.subject, {}, t.talk);
                    t.talk["topic.deed"] = words.phrase;
                    if (!words.nickname.empty())
                        t.talk["topic.nickname"] = words.nickname;
                    t.said = words.subject + " " + words.phrase;
                    t.passes = true;
                    t.facts.push_back(name(teller) + " has heard that " + words.subject + " " + words.phrase +
                                      (words.nickname.empty() ? "" : ", and folk call them " + words.nickname) + "; " + name(teller) +
                                      " tells " + name(listener) + ".");
                    consider(std::move(t));
                }
    // News from one's own life, told to the other.
    for (const auto& [teller, listener] : {std::pair{a, b}, std::pair{b, a}})
        if (const auto found = news_.find(teller); found != news_.end())
            for (const auto& n : found->second)
            {
                if (calendarDays_ - n.day > NewsDays || (!n.about.empty() && n.about == listener) ||
                    told(listener, teller + "|" + n.text))
                    continue;                       // (Not told to the one it is about: "I married Orm" to Orm.)
                AmbientTopic t;
                t.kind = "news";
                t.seen = "yes";                     // (One's own life.)
                t.source = teller;
                t.claim = n.kind;
                t.talk["topic.kind"] = n.kind;           // marriage, loss, work, crime: for the scenes.
                const auto& sc = book.score("news");
                t.score = sc.base + (n.kind == "loss" || n.kind == "crime" ? sc.add : 0);
                t.key = teller + "|" + n.text;
                t.said = n.text;                         // (Put in the teller's own person when it is said: topic.news.)
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
        const auto& sc = book.score("quarrel");
        t.score = sc.base + std::abs(std::min(ab.affinity, ba.affinity)) / sc.per;
        t.key = a < b ? a + "|" + b : b + "|" + a;
        t.facts.push_back(name(a) + " and " + name(b) + " do not get on.");
        for (const auto& [h, o] : {std::pair{a, b}, std::pair{b, a}})
        {
            if (const auto words = describeRegard(h, o); !words.empty())
                t.facts.push_back("How " + name(h) + " sees it: " + words);
            if (const auto* heard = beliefsOf(h))
                for (const auto& belief : *heard)
                    if (belief.subject == o && belief.confidence >= .35 && belief.claim.rfind("deed:", 0) != 0)
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
        t.score = book.score("friends").base;
        t.facts.push_back(name(a) + " and " + name(b) + " are friends.");
        consider(std::move(t));
    }
    // Acquaintances, and a day worth remarking on.
    if (ab.familiarity >= 10 && (plan.kind != "work" || (cell(ea->cellId) && skyAt(ea->cellId, ea->position) >= 1)))
    {
        AmbientTopic t;
        t.kind = "day";
        t.source = a;
        const auto& sc = book.score("day");
        t.score = sc.base + (plan.kind == "festival" ? sc.add : 0);
        t.facts.push_back(name(a) + " and " + name(b) + " know each other.");
        consider(std::move(t));
    }
    sceneTopics(a, b, consider);
}

std::string World::pairBand(const std::string& a, const std::string& b) const
{
    // Who the two are to each other, nearest first (doc 30's bands; doc 62's pair.band).
    const Bond ab = bonds_.find(a, b) ? *bonds_.find(a, b) : Bond{};
    const Bond ba = bonds_.find(b, a) ? *bonds_.find(b, a) : Bond{};
    const auto* spouse = society_.spouse(a);
    if (spouse && *spouse == b)
        return "spouses";
    if (society_.family(a, b))
        return "family";
    if (society_.household(a, b))
        return "housemates";                        // (A shared roof, no kin tie: the watch house, lodgers. Doc 84, defect 3.)
    const auto* p = society_.apprenticedTo(a);
    const auto* q = society_.apprenticedTo(b);
    const auto* ja = society_.jobOf(a);
    const auto* jb = society_.jobOf(b);
    if ((p && jb && p->id == jb->id) || (q && ja && q->id == ja->id))
        return "apprentice";
    if (std::min(ab.affinity, ba.affinity) <= -25)
        return "rivals";
    if (ab.familiarity >= 40 && ab.affinity >= 20 && ba.affinity >= 10)
        return "friends";
    if (ab.familiarity >= 10 || ba.familiarity >= 10)
        return "acquaintances";
    return "strangers";
}

std::vector<Society::PriceMove> World::talkPrices(const std::string& who) const
{
    const auto* e = entity(who);
    const auto* m = e ? roadMarketOf(e->cellId) : nullptr;
    if (!m)
        return {};
    // (The speaker's own shop first, then a shop in the cell they stand in: the price they'd name is the one beside them.
    // The same for every speaker of a cell with no shop of their own, so a look works it out once a cell: Phase 8 repair.)
    const auto own = society_.tillOf(who);
    const auto key = m->id + "|" + (society_.account(own) ? own : std::string()) + "|" + e->cellId + "|" + talkHolder_;
    if (talkPricesMemo_)
        if (const auto found = talkPricesMemo_->find(key); found != talkPricesMemo_->end())
            return found->second;
    auto moves = society_.priceMoves(m->id, talkHolder_, 3, [&](const std::string& till) {
        if (till == own)
            return 0;
        const auto* post = till.size() > 5 ? society_.position(till.substr(5)) : nullptr;
        return post && post->work.cell == e->cellId ? 1 : 2;
    });
    if (talkPricesMemo_)
        (*talkPricesMemo_)[key] = moves;
    return moves;
}

namespace
{
// A good in talk's own words, where the catalogue's read stiffly in a sentence ("trencher bread", "fresh fish").
std::string talkWords(const std::string& base)
{
    static const std::map<std::string, std::string> Own{{"bread", "bread"}, {"fine_bread", "white bread"}, {"fish", "fish"},
                                                        {"porridge", "porridge"}, {"raw_meat", "meat"}, {"candle_tallow", "candles"},
                                                        {"meal", "meal"}, {"herbs", "herbs"}};
    const auto found = Own.find(base);
    return found != Own.end() ? found->second : World::cargoWords(base);
}
} // namespace

void World::talkGood(const std::string& base, std::map<std::string, std::string>& facts)
{
    // The two goods the old lines test by name keep their words ("herbs", "a meal"); every other its plain words, a
    // thing counted by the piece in the plural ("hides", "nails", "salt", "iron ore": the ships' cargo words). Its id,
    // and its sort (Phase 8 repair: a line about eating is said of food only, one about drinking of drink only).
    const auto words = talkWords(base);
    facts["topic.item"] = words;
    facts["topic.what"] = base == "meal" ? "a meal" : words;
    facts["topic.is"] = base == "meal" || !pluralGood(words) ? "is" : "are";
    facts["topic.good"] = base;
    const auto* item = items::good(base);
    static const std::set<std::string> Kitchen{"flour", "milk", "honey", "apples", "berries", "grapes", "eggs", "oats", "grain", "barley"};
    facts["topic.sort"] = item && item->drink ? "drink" : base == "meal" || Kitchen.count(base) || (item && item->category == "food") ? "food" : "goods";
}

void World::talkSexes(const std::string& subject, const std::string& victim, std::map<std::string, std::string>& facts) const
{
    // Doc 84 (Phase 6): a topic's subject and victim, male or female, for #topic.they# and #topic.victim_their#: never
    // "they" for a known single wolf.
    for (const auto& [id, key] : {std::pair{subject, "topic.subject_sex"}, std::pair{victim, "topic.victim_sex"}})
        if (const auto* e = id.empty() ? nullptr : entity(id))
            facts[key] = e->appearance->sex == "female" ? "female" : "male";
}

void World::talkTown(const std::string& who, std::map<std::string, std::string>& facts)
{
    // What the town has been talking of (doc 62's world.* facts): the same news the topics are made of, as facts any
    // line may test, whatever it's about.
    const auto* e = entity(who);
    if (!e)
        return;
    const auto townId = townIdOf(e->cellId);
    bool bandits = false;
    const auto* hereMarket = roadMarketOf(e->cellId);
    for (const auto& c : roads_.caravans)
        bandits |= c.status == "raided" && hereMarket && (c.from == hereMarket->id || c.to == hereMarket->id);
    for (const auto& camp : roads_.camps)
        bandits |= camp.active && calendarDays_ - camp.lastRaid <= 3;
    facts["world.bandits"] = bandits ? "near" : "no";
    bool crime = false;
    for (const auto& i : crime_.incidents)
        crime |= i.town == townId && calendarDays_ - i.day <= 2;
    facts["world.crime"] = crime ? "recent" : "no";
    const auto community = communityOf(e->cellId);
    std::string festival = dayPlan(community).kind == "festival" ? "today" : "no";
    if (festival == "no")
        for (const auto& f : festivals_)
            if (f.community == community && f.day > std::int64_t(calendarDays_) && f.day - calendarDays_ <= 3)
                festival = "soon";
    facts["world.festival"] = festival;
    std::set<std::string> lately;
    if (const auto news = townNews_.find(townId); news != townNews_.end())
        for (const auto& n : news->second)
            if (calendarDays_ - n.day <= 4)
                lately.insert(n.kind);
    facts["world.death"] = lately.count("death") ? "recent" : "no";
    // An arrest in the town within a day (doc 84 §7.4, Phase 6: the town's mood "edgy").
    bool arrest = false;
    if (const auto news = townNews_.find(townId); news != townNews_.end())
        for (const auto& n : news->second)
            arrest |= n.kind == "arrest" && calendarDays_ - n.day <= 1;
    facts["world.arrest"] = arrest ? "recent" : "no";
    facts["world.wedding"] = lately.count("marriage") ? "recent" : "no";
    facts["world.birth"] = lately.count("birth") ? "recent" : "no";
    // The news the last wagon in brought (doc 79, 8.1: `world.news`, the source market's top cause), a few days.
    if (const auto* m = roadMarketOf(e->cellId))
    {
        if (const auto news = society_.board().news.find(m->id); news != society_.board().news.end() && calendarDays_ - news->second.day <= 4)
        {
            facts["world.news"] = news->second.words;
            facts["world.news_from"] = marketWords(news->second.from);
        }
        // The season's harvest of the farms whose carts come here (8.2, T11: the same word that loads the carts), where any
        // staffed farm sends one.
        int sum = 0, carts = 0;
        const auto& farms = society_.farms();
        for (const auto& end : cartEnds_)
            if (end.market == m->id && end.farm < farms.size() && farms[end.farm].heldKeepers + farms[end.farm].heldHands > 0)
                sum += harvestOf(end.area), ++carts;
        if (carts > 0)
        {
            const int word = int(std::lround(double(sum) / carts));
            facts["world.harvest"] = word <= 0 ? "poor" : word >= 2 ? "good" : "fair";
        }
        // The roads' trade here (11's world.trade, called world.wagons: `trade` is a person's fact): a wagon to or from here
        // robbed, else one late, else one in lately.
        std::string trade = "no";
        for (const auto& c : roads_.caravans)
            if (c.kind != "cart" && (c.from == m->id || c.to == m->id))
            {
                const std::string kind = c.status == "raided" ? "raided"
                                         : c.status == "travelling" && c.to == m->id && c.overdue ? "late"
                                         : c.status == "returning" && c.to == m->id && c.leg <= 1 ? "in" : "";
                if (kind == "raided" || (kind == "late" && trade != "raided") || (kind == "in" && trade == "no"))
                    trade = kind;
            }
        facts["world.wagons"] = trade;
        // The port's ships (8.3; Phase 5: `world.ships`, `world.ship_name`), only where this market has a port.
        shipFacts(m->id, facts);
        // The market's Times and their cause, and its staged event (8.4, 8.5; Phase 7: world.times, world.times_cause,
        // world.event, world.event_words, world.event_when).
        eventFacts(m->id, facts);
        // The market's three largest moves in shown price (doc 79, 11; Phase 8): world.<good>_price up or down only where
        // the price on the list moved by a penny or more from its price at rest; meals and herbs steady where their list
        // is (the Game keeps only the goods the library lists).
        std::set<std::string> told;
        for (const auto& move : talkPrices(who))
        {
            facts["world." + move.base + "_price"] = move.trend > 0 ? "up" : "down";
            told.insert(move.base);
        }
        for (const char* base : {"meal", "herbs"})
            if (!told.count(base) && std::abs(society_.pressure(m->id, base)) > 1e-3)
                if (const auto* tp = society_.tradePoints().find(m->id))
                    for (const auto& till : tp->tills)
                        if (const auto* s = society_.account(till); s && s->stock.count(base))
                        {
                            const auto move = society_.priceMove(till, base, talkHolder_);
                            facts[std::string("world.") + base + "_price"] = move.trend > 0 ? "up" : move.trend < 0 ? "down" : "steady";
                            break;
                        }
    }
    // The place itself: how many are about, and whether a traveller (a player) is near.
    int people = 0;
    bool traveller = false;
    for (const auto* o : entitiesIn(e->cellId))
        if (o && !o->dead && o->id != who)
        {
            people += o->npc;
            traveller |= !o->npc && apart(o->position, e->position) <= 12;
        }
    facts["here.crowd"] = people >= 6 || wharfCrowd(e->cellId) || eventCrowd(e->cellId) ? "crowd" : people >= 3 ? "busy" : "quiet";   // (A ship in: 8.3; an event: 8.4.)
    facts["here.player_near"] = traveller ? "yes" : "no";
}

void World::setTalkDialects(std::map<std::string, std::string> dialectOf)
{
    talkDialectOf_ = std::move(dialectOf);
    talkHomes_.clear();
}

void World::talkOrigin(const std::string& id, std::string& from, std::string& accent) const
{
    // Where a resident grew up, and how strongly they still speak like it (doc 62, phase 3; doc 83, Phase 4). Worked
    // out from who they are, never stored, so the same resident always has the same: most grew up where they live; of
    // each place's grown folk about one in seven came from another (one in four in Ridgemere, the city everyone comes
    // to). The old and the never-moved speak broadest; those settled elsewhere fade. The places are the realm's seven
    // dialect regions, the Country's towns with a dialect of their own (Saltwell's, the oasis: doc 83) and the towns
    // that speak another's (talkDialectOf_: Fenshore), of those where somebody lives in this world: a town erased from
    // the map (Mirebridge on Ridgemere 3) is nobody's birthplace. (The Country's towns come after the seven in their
    // names' order, as they did when both borrowed a dialect, so nobody else's birthplace moves.)
    static const std::vector<std::string> Regions{"ridgemere", "frosthollow", "coldwater", "oakhaven", "mirebridge",
                                                  "larkfield", "southmeadow"};
    static const std::set<std::string> OwnTowns{"saltwell"};
    from.clear();
    accent.clear();
    const auto* e = entity(id);
    const auto* life = society_.resident(id);
    if (!e || !life)
        return;
    if (talkHomes_.empty())
    {
        std::map<std::string, int> lived;
        for (const auto& [rid, r] : society_.state().residents)
            if (!r.homeCell.empty())
                ++lived[townIdOf(r.homeCell)];
        for (const auto& r : Regions)
            if (lived.count(r))
                talkHomes_.push_back({r, lived[r]});
        auto towns = OwnTowns;
        for (const auto& [town, dialect] : talkDialectOf_)
            towns.insert(town);
        for (const auto& town : towns)
            if (lived.count(town) && std::none_of(talkHomes_.begin(), talkHomes_.end(), [&](const auto& p) { return p.first == town; }))
                talkHomes_.push_back({town, lived[town]});
    }
    const auto home = townIdOf(life->homeCell.empty() ? e->cellId : life->homeCell);
    if (std::find(Regions.begin(), Regions.end(), home) == Regions.end() && !OwnTowns.count(home) && !talkDialectOf_.count(home))
        return;                                     // (Somewhere that is none of the realm's: no dialect of its own.)
    std::uint64_t h = 1469598103934665603ULL;
    for (const unsigned char c : "origin|" + id)
        h = (h ^ c) * 1099511628211ULL;
    h ^= h >> 29;
    const auto stage = lifeStage(e->age);
    const bool grown = stage == LifeStage::Adult || stage == LifeStage::Old;
    // One who moved came from another place, the more likely the more folk live there (the city's sends most).
    std::uint64_t others = 0;
    for (const auto& [place, folk] : talkHomes_)
        others += place != home ? std::uint64_t(folk) : 0;
    const bool moved = grown && others > 0 && int(h % 100) < (home == "ridgemere" ? 25 : 15);
    from = home;
    if (moved)
    {
        auto pick = (h >> 8) % others;
        for (const auto& [place, folk] : talkHomes_)
            if (place != home)
            {
                if (pick < std::uint64_t(folk))
                {
                    from = place;
                    break;
                }
                pick -= std::uint64_t(folk);
            }
    }
    const int roll = int((h >> 20) % 100);
    if (stage == LifeStage::Old)
        accent = moved ? "light" : "broad";
    else if (!moved)
        accent = roll < 20 ? "broad" : roll < 85 ? "light" : "none";
    else
        accent = roll < 10 ? "broad" : roll < 60 ? "light" : "none";
}

std::string World::outsideOf(const std::string& cellId) const
{
    if (doorsKey() != outsideDoors_)                // (Not the doors' count: seams stream in and out: doorsKey.)
    {
        outsideOf_.clear();
        outsideDoors_ = doorsKey();
    }
    if (const auto found = outsideOf_.find(cellId); found != outsideOf_.end())
        return found->second;
    // Out through the doors that lead in, the nearest first, to the first cell under the sky.
    std::vector<std::string> queue{cellId};
    std::set<std::string> seen{cellId};
    std::string out;
    for (std::size_t at = 0; at < queue.size() && at < 16 && out.empty(); ++at)
    {
        const auto* c = cell(queue[at]);
        if (!c)
            continue;
        if (c->outdoors)
        {
            out = c->id;
            break;
        }
        for (const auto& [id, d] : doors_)
            if (d.targetCell == queue[at] && d.cellId != queue[at] && seen.insert(d.cellId).second)
                queue.push_back(d.cellId);
    }
    return outsideOf_[cellId] = out;
}

void World::talkMoment(const std::string& who, std::map<std::string, std::string>& facts)
{
    // Where and when, in the talk library's facts (doc 62): the place, the town, the hour, the sky, the day.
    const auto* e = entity(who);
    if (!e)
        return;
    const auto* c = cell(e->cellId);
    auto townId = townIdOf(e->cellId);
    const auto plan = dayPlan(communityOf(e->cellId));
    const auto cal = calendar::calendarAt(calendarDays_);
    static const char* seasons[] = {"spring", "summer", "autumn", "winter"};
    const double hour = cal.hour;
    facts["here.place"] = c ? placeKind(*c, townOf(e->cellId) != nullptr) : "street";
    facts["here.day"] = plan.kind;
    facts["here.region"] = townId;
    facts["here.season"] = seasons[std::clamp(int(cal.season), 0, 3)];
    facts["here.time"] = hour < 5 || hour >= 20 ? "night" : hour < 7.5 ? "dawn" : hour < 18 ? "day" : "dusk";
    facts["here.hour"] = std::to_string(std::clamp(int(std::floor(hour)), 0, 23));   // ("Good morning" before noon: doc 84 §7.8.)
    facts["here.weather"] = c && c->outdoors ? weatherName(c->weather) : "indoors";
    // The weather outside, from behind a counter too (doc 84 §7.8): a keeper may speak of the mist truthfully.
    if (const auto* out = c ? cell(c->outdoors ? c->id : outsideOf(c->id)) : nullptr)
        facts["here.outdoors"] = weatherName(out->weather);
    if (c)
        facts["here.place_name"] = c->name;
    facts["here.town"] = townWords(townId);
    facts["here.weekday"] = calendar::weekdayName(calendar::weekdayOf(calendarDays_));
    if (plan.kind == "festival")
        facts.emplace("here.festival", plan.name);
}

void World::sceneTopics(const std::string& a, const std::string& b, const std::function<void(AmbientTopic)>& consider)
{
    // The topics only the written scenes voice (doc 30): what has been happening in town, and the everyday.
    const auto* ea = entity(a);
    const auto* eb = entity(b);
    const auto name = [&](const std::string& id) {
        const auto* who = entity(id);
        return who ? who->name : std::string("someone");
    };
    const auto townId = townIdOf(ea->cellId);
    const auto& book = topics::book();              // (topics.json's numbers: doc 84 §7.3, Phase 5.)
    // A little chance in the everyday topics, so the same two don't always talk of the same thing.
    const auto jitter = [&](int salt) {
        std::uint64_t h = std::hash<std::string>{}(a + "|" + b) ^ (std::uint64_t(calendarDays_ * 24) * 0x9E3779B97F4A7C15ULL) ^
                          std::uint64_t(salt) * 1099511628211ULL;
        h ^= h >> 31;
        return double(h % 1000) / 1000.0 * book.jitter;
    };
    // The market's news (prices, events, ships, wagons, the Times, the harvest, the town's lives) drawn once an hour for
    // the whole place, not a pair: whichever pair of a cell talks, the hour's draw decides which of what is live is the
    // talk of it, so each is heard while it lasts (Phase 8 repair; a pair's own draw would let the highest ceiling win).
    const auto hourly = [&](int salt) {
        std::uint64_t h = std::hash<std::string>{}("hour|" + townId) ^ (std::uint64_t(calendarDays_ * 24) * 0x9E3779B97F4A7C15ULL) ^
                          std::uint64_t(salt) * 1099511628211ULL;
        h ^= h >> 29;
        h *= 0xBF58476D1CE4E5B9ULL;
        h ^= h >> 32;
        return double(h % 1000) / 1000.0 * book.hourly;
    };
    // A source's score: its base, and the place's draw for the hour or the pair's draw where it has one.
    const auto H = [&](const topics::Score& sc) { return sc.hourly >= 0 ? sc.base + hourly(sc.hourly) : sc.base; };
    const auto J = [&](const topics::Score& sc) { return sc.jitter >= 0 ? sc.base + jitter(sc.jitter) * sc.jitterScale : sc.base; };
    const auto topic = [&](const std::string& kind, double score, std::string key = {}) {
        AmbientTopic t;
        t.kind = kind;
        t.source = a;
        t.score = score;
        t.key = key.empty() ? kind : std::move(key);
        return t;
    };
    // Caravans to and from this town, and the bandits on the roads.
    // (The road layer's market here since doc 79's Phase 4: wagons run between markets; a wagon's first good and its
    // carter go with the topic, topic.what and topic.who.)
    const auto* hereMarket = roadMarketOf(ea->cellId);
    const auto marketId = hereMarket ? hereMarket->id : townId;
    for (const auto& c : roads_.caravans)
    {
        if (c.kind == "cart")
            continue;
        const bool ours = c.from == marketId || c.to == marketId;
        if (!ours)
            continue;
        const auto other = c.from == marketId ? c.to : c.from;
        std::string kind = c.status == "raided" ? "raided"
                           : c.status == "returning" && c.to == marketId && c.leg <= 1 ? "arrived"
                           : c.status == "travelling" && c.to == marketId && c.overdue ? "late"
                           : c.status == "travelling" && c.from == marketId && c.leg > 0 ? "left"
                           : c.status == "travelling" && c.to == marketId ? "due" : "";   // (On its way here: Phase 8.)
        if (kind.empty())
            continue;
        auto t = topic(kind == "raided" ? "bandits" : "caravan",
                       kind == "raided" ? H(book.score("bandits", "raided")) : H(book.score("caravan", kind)), "wagon|" + c.id + "|" + kind);
        t.talk["topic.kind"] = kind;
        t.talk["topic.where"] = townWords(other);
        if (!c.manifest.empty())
            talkGood(c.manifest.front().first, t.talk);   // ("nails", "hides", as a line says it mid-sentence; topic.is.)
        if (const auto* driver = entity(c.driver))
            t.talk["topic.who"] = driver->name;
        if (kind == "late" && !c.lateCause.empty())
            t.talk["topic.why"] = c.lateCause;                            // ("a broken axle", "the weather": Phase 8.)
        if (kind == "due")
            t.talk["topic.when"] = c.due > 0 && c.due - calendarDays_ <= 1 ? "today" : "soon";
        t.facts.push_back("A wagon between here and " + townWords(other) + " " + kind +
                          (kind == "late" && !c.lateCause.empty() ? ", held up by " + c.lateCause : std::string()) + ".");
        consider(std::move(t));
        if (kind == "raided")
        {
            auto road = topic("caravan", H(book.score("caravan", "raided")), "wagon|" + c.id + "|road");
            road.talk["topic.kind"] = "raided";
            road.talk["topic.where"] = townWords(other);
            consider(std::move(road));
        }
    }
    // A wagon posted to come here, not yet set out (8.6: "caravan due"; Phase 8), and the news the last one brought
    // (`world.news`, 8.1): what the carters' notices and the market already know.
    // (The soonest posted: Phase 8 repair.)
    const market::Departure* posted = nullptr;
    for (const auto& d : society_.board().committed)
        if (d.kind == "wagon" && d.to == marketId && (!posted || d.day < posted->day || (d.day == posted->day && d.hour < posted->hour)))
            posted = &d;
    if (posted)
    {
        const auto& d = *posted;
        auto t = topic("caravan", H(book.score("caravan", "posted")), "posted|" + d.from);
        t.talk["topic.kind"] = "due";
        t.talk["topic.when"] = d.day <= std::int64_t(std::floor(calendarDays_)) ? "today" : "soon";
        t.talk["topic.where"] = townWords(d.from);
        if (!d.manifest.empty())
            talkGood(d.manifest.front().first, t.talk);
        if (const auto* driver = entity(d.driver))
            t.talk["topic.who"] = driver->name;
        t.facts.push_back("A wagon from " + townWords(d.from) + " is posted to come here.");
        consider(std::move(t));
    }
    if (hereMarket)
        if (const auto news = society_.board().news.find(hereMarket->id);
            news != society_.board().news.end() && calendarDays_ - news->second.day <= 4 && !news->second.words.empty())
        {
            auto t = topic("caravan", H(book.score("caravan", "news")), "wagon_news|" + news->second.words);
            t.talk["topic.kind"] = "news";
            t.talk["topic.news"] = news->second.words;
            t.talk["topic.where"] = marketWords(news->second.from);
            t.facts.push_back("The last wagon in brought word: " + news->second.words + ".");
            consider(std::move(t));
        }
    // Prices that moved (doc 79, 11; Phase 8, the price topics back): the market's three largest moves in shown price,
    // said at the price the list shows the traveller listening (the shown-price gate: T1, T17), with the cause that moved
    // it; and where a traveller's own trade moved it, that traveller, as the teller knows them (a name given, or a look).
    // (Scored with the other live topics of the market, the events, ships, wagons, Times and harvest, each with a little
    // chance an hour, so each is heard while it lasts: Phase 8 repair. The largest move first; the others below it.)
    {
        const auto& sc = book.score("prices");
        double score = H(sc);
        for (const auto& move : talkPrices(a))
        {
            auto t = topic("prices", score, "price|" + move.base);
            score -= sc.step;
            talkGood(move.base, t.talk);
            t.talk["topic.dir"] = move.trend > 0 ? "up" : "down";
            t.talk["topic.price"] = Society::priceWords(move.shown, move.listCount);
            t.talk["topic.cause"] = move.cause;
            t.talk["topic.cause_words"] = move.causeWords;
            if (!move.ship.empty())
                t.talk["topic.ship"] = move.ship;
            t.facts.push_back(cargoWords(move.base) + ": " + (move.trend > 0 ? "dearer" : "cheaper") + " here lately, " +
                              Society::priceWords(move.shown, move.listCount) + " (" + move.causeWords + ").");
            const auto price = t.talk;
            const double scored = t.score;
            consider(std::move(t));
            if (move.pusher.empty() || !knower_)
                continue;
            for (const auto& [id, pe] : entities_)
                if (!pe.npc && !pe.dead && (holderOf_ ? holderOf_(id) : society_.holderOf(id)) == move.pusher)
                {
                    const auto known = knower_(a, id);
                    if (known.words.empty())
                        break;
                    auto c = topic("price_cause", scored + book.score("price_cause").add, "price|" + move.base);   // (The same draw: the traveller named in the price's place.)
                    c.subject = id;
                    c.talk = price;
                    c.talk.erase("topic.cause");
                    c.talk.erase("topic.cause_words");
                    c.talk["topic.known"] = known.byName ? "yes" : "no";
                    // (Whether that traveller's buying took a shop's last of it: "bought the shelf bare" is said only then.)
                    c.talk["topic.bare"] = move.bare ? "yes" : "no";
                    c.talk["topic.who"] = known.words;
                    c.facts.push_back(name(a) + " says " + cargoWords(move.base) + " got " + (move.trend > 0 ? "dearer" : "cheaper") +
                                      " since " + known.words + (move.trend > 0 ? " bought it up." : " sold a load in."));
                    consider(std::move(c));
                    break;
                }
        }
    }
    // The port's ships (8.3, 8.6; Phase 5's facts, Phase 8's topic): one in, due, kept out by a storm, lost, or sailed,
    // each its own topic with its own ship (topic.ship), so a ship lost is talked of while another is in.
    if (hereMarket && port(hereMarket->id))
    {
        const auto today = std::int64_t(std::floor(calendarDays_));
        std::map<std::string, const market::Ship*> told;
        for (const auto& s : society_.board().ships)
        {
            if (s.port != hereMarket->id)
                continue;
            const std::string kind = s.status == "in" ? "in"
                                     : s.status == "storm" && s.lost && today - s.day <= 4 ? "lost"
                                     : s.status == "storm" && today - s.day <= 2 ? "late"
                                     : s.status == "due" && s.day - today <= 2 ? "due"
                                     : s.status == "sailed" && today - s.day <= 1 ? "sailed" : "";
            if (kind.empty())
                continue;
            auto& one = told[kind];
            if (!one || (kind == "due" ? s.day < one->day : s.day > one->day))
                one = &s;
        }
        for (const auto& [kind, s] : told)
        {
            // (Phase 10's first pass: a lost ship was a fixed 2.3, over everything for its four days; in the hour's draw
            // now, still the likeliest, so the Times and an event are heard beside it.)
            auto t = topic("ships", H(book.score("ships", kind)), "ship|" + s->name + "|" + kind);
            t.talk["topic.kind"] = kind;
            t.talk["topic.ship"] = s->name;
            // (Her rich cargo's goods first, while it is on: the brandy she brought in, not her everyday cargo. The merge
            // of Phase 7's and Phase 8's repairs.)
            std::string rich;
            if (kind == "in")
                for (const auto& e : society_.board().events)
                    if (e.status == "on" && e.subject == "ship:" + s->id)
                        for (const auto& [g, target] : e.goods)
                            if (rich.empty() && target < 0)
                                rich = g;
            if (!rich.empty())
                talkGood(rich, t.talk);
            else if (!s->brings.empty())
                talkGood(s->brings.front().first, t.talk);
            if (kind == "due")
                t.talk["topic.when"] = s->day <= today ? "today" : "soon";
            t.facts.push_back(s->name + (kind == "in" ? " is in at the Wharf." : kind == "due" ? " is due at the Wharf."
                                         : kind == "late" ? " is stormbound." : kind == "lost" ? " was lost at sea." : " has sailed."));
            consider(std::move(t));
        }
    }
    // The market's staged event, its Times and the harvest of the farms that send it carts (8.2-8.5; Phase 7's facts,
    // Phase 8's topics): what the notices, the criers and the carts already tell.
    if (hereMarket)
    {
        std::map<std::string, std::string> f;
        eventFacts(hereMarket->id, f);
        if (f.count("world.event"))
        {
            const auto& when = f["world.event_when"];
            auto t = topic("event", H(book.score("event", when)), "event|" + f["world.event"] + "|" + when);
            t.facts.push_back("At the market: " + f["world.event_words"] + " (" + when + ").");
            consider(std::move(t));
        }
        if (f["world.times"] != "ordinary")         // (A cause is talked of only with Times it made: never in ordinary ones.)
        {
            // (Phase 10's first pass: 1.65 and 1.45 before, so an event on (1.7) took most hours from hard Times and
            // nearly all from lean ones; now hard or boom Times draw level with an event on, lean or good a step under.)
            auto t = topic("times", H(book.score("times", f["world.times"])), "times|" + f["world.times"]);
            t.facts.push_back("Times here are " + f["world.times"] + (f.count("world.times_cause") ? ", since " + f["world.times_cause"] : std::string()) + ".");
            consider(std::move(t));
        }
        int sum = 0, carts = 0;
        const auto& farms = society_.farms();
        for (const auto& end : cartEnds_)
            if (end.market == hereMarket->id && end.farm < farms.size() && farms[end.farm].heldKeepers + farms[end.farm].heldHands > 0)
                sum += harvestOf(end.area), ++carts;
        if (carts > 0)
        {
            const int word = int(std::lround(double(sum) / carts));
            const int season = int(calendar::calendarAt(calendarDays_).season);
            // (Talked of while it comes in, summer and autumn, most when it's poor or good; a poor one into the winter.)
            if (season == 1 || season == 2 || (season == 3 && word <= 0))
                consider(topic("harvest", H(book.score("harvest", word == 1 ? "fair" : "")), "harvest|" + std::to_string(word)));
        }
    }
    for (const auto& camp : roads_.camps)
        if (camp.active && calendarDays_ - camp.lastRaid <= 3)
        {
            auto t = topic("bandits", H(book.score("bandits", "camp")), "camp|" + camp.id);
            t.talk["topic.kind"] = "camp";
            if (const auto near = placeNear(camp.cell); !near.empty())
                t.talk["topic.where"] = near;   // ("Glasswater", never "The Greenholt, Glasswater" nor the cell's id: doc 84.)
            consider(std::move(t));
            break;
        }
    // Crime in town, not between the two talking.
    for (const auto& i : crime_.incidents)
        if (i.town == townId && calendarDays_ - i.day <= 2 && i.victim != a && i.victim != b && i.offender != a && i.offender != b)
        {
            auto t = topic("crime", H(book.score("crime")), "incident|" + i.id);
            t.talk["topic.kind"] = i.kind == "assault" ? "assault" : "theft";
            t.talk["topic.victim"] = name(i.victim);
            // The one who did it is named only if a witness saw who (one who heard a struggle, or glimpsed someone they
            // couldn't make out, names nobody), and a traveller only as the teller knows them (doc 56; the slice-end
            // review): else "someone".
            std::string who = "someone";
            if (std::any_of(i.witnesses.begin(), i.witnesses.end(), [](const Witness& w) { return w.identified; }))
                if (const auto* doer = entity(i.offender))
                {
                    who = doer->npc ? doer->name : namer_ ? namer_(a, i.offender) : std::string();
                    if (who.empty())
                        who = strangerWords(doer->appearance);
                }
            t.talk["topic.subject"] = who.empty() ? std::string("someone") : who;
            talkSexes(who == "someone" || who.empty() ? std::string() : i.offender, i.victim, t.talk);
            t.seen = std::any_of(i.witnesses.begin(), i.witnesses.end(), [&](const Witness& w) { return w.id == a; }) ? "yes" : "no";
            t.facts.push_back(name(i.victim) + " was " + (i.kind == "assault" ? "attacked" : "robbed") + " in town.");
            consider(std::move(t));
        }
    // The town's news: weddings, deaths, apprenticeships, arrests, newcomers.
    if (const auto found = townNews_.find(townId); found != townNews_.end())
        for (const auto& n : found->second)
        {
            if (calendarDays_ - n.day > TownNewsDays || n.subject == a || n.subject == b || n.other == a || n.other == b)
                continue;
            // Nor by or to their own (a spouse, kin, one under the same roof): a father isn't told his own pup's birth as
            // the town's news, nor tells it so; a family come in from the Country don't call each other newcomers.
            const auto close = [&](const std::string& x, const std::string& who) {
                if (who.empty())
                    return false;
                const auto* spouse = society_.spouse(x);
                return (spouse && *spouse == who) || society_.family(x, who) || society_.household(x, who);
            };
            if (close(a, n.subject) || close(b, n.subject) || close(a, n.other) || close(b, n.other))
                continue;
            const bool fresh = n.kind == "newcomer";
            auto t = topic(fresh ? "newcomer" : n.kind == "arrest" ? "crime" : "life",
                           fresh ? H(book.score("newcomer")) : n.kind == "arrest" ? H(book.score("crime", "arrest")) : H(book.score("life", n.kind)),
                           "town|" + n.kind + "|" + n.subject);
            t.talk["topic.kind"] = n.kind;
            t.talk["topic.subject"] = name(n.subject);
            talkSexes(n.subject, {}, t.talk);
            if (!n.other.empty())
                t.talk["topic.where"] = name(n.other);
            consider(std::move(t));
        }
    // A festival today, or one called for the days ahead.
    const auto community = communityOf(ea->cellId);
    const auto plan = dayPlan(community);
    if (plan.kind == "festival")
    {
        auto t = topic("festival", H(book.score("festival", "today")), "festival|" + plan.name);
        t.talk["topic.when"] = "today";
        t.talk["here.festival"] = plan.name;
        consider(std::move(t));
    }
    else
        for (const auto& f : festivals_)
            if (f.community == community && f.day > std::int64_t(calendarDays_) && f.day - calendarDays_ <= 3)
            {
                auto t = topic("festival", H(book.score("festival", "soon")), "festival|" + f.name);
                t.talk["topic.when"] = "soon";
                t.talk["here.festival"] = f.name;
                consider(std::move(t));
                break;
            }
    // A traveller standing near the two of them.
    for (const auto& [id, e] : entities_)
        if (!e.npc && !e.dead && e.cellId == ea->cellId && apart(e.position, ea->position) <= 12 &&
            visionClarity(a, id) > 0 && visionClarity(b, id) > 0)
        {
            auto t = topic("player", J(book.score("player")), "player|" + id);
            const auto* bond = bonds_.find(a, id);
            const bool knows = bond && bond->familiarity >= 5;
            // (Named only if the speaker was given the name: doc 56, 6.)
            const auto given = knows ? (namer_ ? namer_(a, id) : e.name) : std::string();
            t.talk["topic.known"] = !given.empty() ? "yes" : "no";
            t.talk["topic.subject"] = !given.empty() ? given : strangerWords(e.appearance);
            talkSexes(id, {}, t.talk);
            consider(std::move(t));
            break;
        }
    // Speech from elsewhere (doc 62, phase 3): a stranger, or one barely known, who doesn't sound like them.
    {
        std::string fromA, accentA, fromB, accentB;
        talkOrigin(a, fromA, accentA);
        talkOrigin(b, fromB, accentB);
        const auto band = pairBand(a, b);
        if (!fromA.empty() && !fromB.empty() && fromA != fromB && accentB != "none" &&
            (band == "strangers" || band == "acquaintances"))
        {
            const auto& sc = book.score("accent");
            auto t = topic("accent", J(sc) + (accentB == "broad" ? sc.add : 0));
            t.talk["topic.accent_of"] = fromB;
            t.talk["topic.accent_town"] = townWords(fromB);
            consider(std::move(t));
        }
    }
    // The everyday: family, work, the weather, the town's own lore, and talk for talk's sake.
    if (const auto* spouse = society_.spouse(a); (spouse && *spouse == b) || society_.family(a, b))
        consider(topic("family", J(book.score("family"))));   // (Kin only: housemates share a roof, not a father's tools.)
    else if (society_.household(a, b))
        consider(topic("home", J(book.score("home", "housemates"))));   // (What housemates have in common is the house: doc 84, defect 3.)
    const auto* ja = society_.jobOf(a);
    const auto* jb = society_.jobOf(b);
    if ((ja && jb && (ja->work.cell == jb->work.cell || ja->title == jb->title)) || (ja && ja->work.cell == ea->cellId))
        consider(topic("work", J(book.score("work"))));
    if (const auto* c = cell(ea->cellId); c && c->outdoors)
        if (const auto here = weatherAt(c->id, ea->position); skyAt(c->id, ea->position) >= 1 || (here.kind == Weather::Fog && here.intensity >= .3))
            consider(topic("weather", J(book.score("weather"))));
        else
            consider(topic("weather", J(book.score("weather", "fair"))));     // (A fair sky is remarked on too, less often: doc 62, phase 7.)
    consider(topic("lore", J(book.score("lore"))));
    consider(topic("smalltalk", J(book.score("smalltalk"))));
    // More of everyday life (doc 62, phase 7: no conversation heard twice in a hundred hours): each its own topic,
    // so the two wander from one subject to another hour by hour.
    static const char* const Everyday[] = {"food", "home", "pastimes", "animals", "health", "memories",
                                           "dreams", "faith", "travel", "land", "season"};
    for (const char* kind : Everyday)
        consider(topic(kind, J(book.score(kind))));
    (void)eb;
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
        if (!e.npc || e.dead || e.transient || e.offstage || e.state == "beaten down" || e.downedLeft > 0 || !e.leaderId.empty() ||
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
    // Talk's prices for each listener's own list (doc 79, 11, T17: talkHolder_), worked out once a look for a shop and a
    // cell (talkPricesMemo_), and the holder put back after.
    std::map<std::string, std::vector<Society::PriceMove>> memo;
    talkPricesMemo_ = &memo;
    const auto heldBefore = talkHolder_;
    for (const auto& listenerId : listeners)
    {
        talkFor(listenerId);
        const auto* listener = entity(listenerId);
        if (!listener || listener->npc || listener->dead || cellsDone.count(listener->cellId) ||
            recent("cell:" + listener->cellId, PlaceEveryCrowd))
            continue;
        std::vector<const Entity*> near;
        for (const auto& [id, e] : entities_)
            if (e.cellId == listener->cellId && free(e) && hearingClarity(listenerId, id, Voice::Speak) >= Earshot)
                near.push_back(&e);
        std::sort(near.begin(), near.end(), [&](const Entity* x, const Entity* y) {
            return apart(x->position, listener->position) < apart(y->position, listener->position) ||
                   (apart(x->position, listener->position) == apart(y->position, listener->position) && x->id < y->id);
        });
        const double every = near.size() >= 6 ? PlaceEveryCrowd : near.size() >= 3 ? PlaceEveryBusy : PlaceEveryQuiet;
        if (recent("cell:" + listener->cellId, every))
            continue;
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
                best = {first ? near[i]->id : near[j]->id, first ? near[j]->id : near[i]->id, listener->cellId, listenerId, std::move(topic)};
            }
        if (best.topic.score > 0)
        {
            cellsDone.insert(best.cell);
            picks.push_back(std::move(best));
        }
    }
    talkPricesMemo_ = nullptr;
    talkHolder_ = heldBefore;
    std::sort(picks.begin(), picks.end(), [](const AmbientPick& x, const AmbientPick& y) { return x.topic.score > y.topic.score; });
    return picks;
}

void World::ambientBegun(const AmbientPick& pick)
{
    ambientLast_[pick.teller] = ambientLast_[pick.listener] = ambientLast_["cell:" + pick.cell] = time_;
    for (auto it = ambientLast_.begin(); it != ambientLast_.end();)
        it = time_ - it->second > 3600 ? ambientLast_.erase(it) : std::next(it);
}

void World::ambientEnded(const AmbientPick& pick, const std::vector<std::string>& people)
{
    // Doc 84 §8.14 (Phase 4): the blanket warmth of a talk is gone; each line's moves carry their own (moves.json's
    // effects, applied by the game as they are said). Having talked, everyone in it knows the others a little better;
    // a quarrel's grudge, aired, sours (a topic's, until Phase 6's stances).
    const auto& t = pick.topic;
    for (const auto& h : people)
        for (const auto& o : people)
            if (h != o && !h.empty() && !o.empty())
                bonds_.change(h, o, t.kind == "quarrel" ? BondChange{-1.5, -.5, .5, 0, 0} : BondChange{0, 0, 1, 0, 0}, calendarDays_);
    recordEvent({t.kind == "quarrel" ? "quarrel" : "conversation", pick.teller, pick.listener, pick.cell, 0, 0, {}, 0, 0, t.kind});
}

const Belief* World::holding(const std::string& holder, const std::string& subject, const std::string& claim,
                             const std::string& incident, const std::string& kind, const std::string& object) const
{
    const auto found = beliefs_.find(holder);
    if (found == beliefs_.end() || subject.empty())
        return nullptr;
    for (const auto& b : found->second)
        if (b.subject == subject && (b.claim == claim || (!incident.empty() && b.incident == incident) ||
                                     (!kind.empty() && b.kind == kind && b.object == object)))
            return &b;
    return nullptr;
}

void World::told(const Belief& given, const std::string& via)
{
    // Doc 84 §8.6: corroboration by a second, independent source; otherwise as believe (the surer kept).
    // (Once a source: the ledger's told set remembers who told them what: doc 84 §8.7, Phase 5.)
    const auto key = given.subject + "|" + given.claim;
    const bool before = talkLedger_.counted(given.holder, given.source, key);
    talkLedger_.told(given.holder, given.source, key, given.kind, calendarDays_, true);
    const auto mine = beliefs_.find(given.holder);
    if (mine != beliefs_.end() && !given.source.empty())
        for (auto& old : mine->second)
            if (old.subject == given.subject && old.claim == given.claim)
            {
                const bool independent = given.source != old.source && given.source != given.holder && via != old.source &&
                                         via != given.holder && !before;
                if (!independent)
                    break;
                old.confidence = std::min(1.0, old.confidence + (1 - old.confidence) * .4);
                old.day = calendarDays_;
                return;
            }
    believe(given);
}

void World::doubted(const std::string& holder, const std::string& subject, const std::string& claim, double factor)
{
    if (const auto mine = beliefs_.find(holder); mine != beliefs_.end())
        for (auto& b : mine->second)
            if (b.subject == subject && b.claim == claim)
                b.confidence = std::clamp(b.confidence * std::clamp(factor, 0.0, 1.0), 0.0, 1.0);
}

void World::ambientSpoken(const AmbientPick& pick)
{
    ambientLast_[pick.teller] = ambientLast_[pick.listener] = ambientLast_["cell:" + pick.cell] = time_;
    for (auto it = ambientLast_.begin(); it != ambientLast_.end();)
        it = time_ - it->second > 3600 ? ambientLast_.erase(it) : std::next(it);
    // What talk does (the words only perform it): gossip passed on is believed, a grudge aired sours, friends warm.
    const auto& t = pick.topic;
    if (t.passes)
    {
        Belief told;
        told.holder = pick.listener;
        told.subject = t.subject;
        told.claim = t.claim;
        told.source = pick.teller;
        told.confidence = t.confidence * .7;
        told.incident = t.incident;
        told.as = t.as;                             // (Doc 56.)
        told.kind = t.claimKind;                    // (Its kind, victim and place go with it: doc 84 §7.7.)
        told.object = t.object;
        told.place = t.place;
        believe(told);
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
