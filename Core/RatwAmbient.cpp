// The ambient director (RatwAmbient.h; Docs/Design/26-living-npcs.md, Phase 10). World members, kept here.
#include "RatwAmbient.h"
#include "RatwWorld.h"

#include "RatwItems.h"

#include <algorithm>
#include <cmath>

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
    else if (e.kind == "relocation")
        talkOf("newcomer", a, {});
    if (e.kind == "marriage")
    {
        tell(a, "marriage", name(a) + " married " + name(t));
        tell(t, "marriage", name(t) + " married " + name(a));
    }
    else if (e.kind == "mourning")
        tell(a, "loss", name(a) + " lost " + name(t));     // (The teller first, as it's said: "I lost Bran".)
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
        tell(a, "crime", name(a) + " took " + name(t) + " to the gaol for the Watch");
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
        if (c->outdoors && skyAt(c->id, ea->position) >= 1)
            day.push_back(weatherAt(c->id, ea->position).kind == Weather::Rain ? "It is raining." : "The weather is foul.");
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
                    calendarDays_ - belief.day > GossipDays || belief.claim.rfind("deed:", 0) == 0)
                    continue;                       // (Deeds are talked of in their own words: doc 56.)
                bool known = false;
                if (const auto* theirs = beliefsOf(listener))
                    for (const auto& other : *theirs)
                        known |= other.subject == belief.subject && other.claim == belief.claim;
                if (known)
                    continue;
                const auto* about = entity(belief.subject);
                // A player is named only as the teller knows them, else by look (doc 56, 6).
                const auto subjectName = [&]() -> std::string {
                    if (!about || about->npc)
                        return name(belief.subject);
                    const auto known = namer_ ? namer_(teller, belief.subject) : about->name;
                    return known.empty() ? strangerWords(about->appearance) : known;
                }();
                AmbientTopic t;
                t.kind = "gossip";
                t.subject = belief.subject;
                t.claim = belief.claim;
                t.source = teller;
                t.incident = belief.incident;
                t.confidence = belief.confidence;
                t.score = 3 + belief.confidence + (about && !about->npc ? .5 : 0);
                t.talk["topic.subject"] = subjectName;
                // What kind of thing it is, for the lines to answer in kind (doc 62, phase 7): the claim is free words.
                const auto& c = belief.claim;
                const auto starts = [&](const char* w) { return c.rfind(w, 0) == 0; };
                t.talk["topic.claim_kind"] = c == "died" ? "death" : c == "married" ? "marriage"
                                           : starts("attacked") || c == "is violent" ? "violence"
                                           : starts("stole") || starts("tried to steal") ? "theft"
                                           : starts("was attacked") || starts("was robbed") ? "victim"
                                           : starts("is wanted") ? "wanted"
                                           : starts("raids the road") || starts("robs travellers") ? "bandit"
                                           : c == "keeps promises" ? "promise_kept" : c == "breaks promises" ? "promise_broken"
                                           : starts("was prowling") ? "prowling" : "other";
                t.facts.push_back(name(teller) + " has heard that " + subjectName + " " + belief.claim + " (" +
                                  (belief.source == "saw it" ? name(teller) + " saw it"
                                                             : "from " + name(belief.source)) + "), and tells " +
                                  name(listener) + ".");
                if (about)
                    t.facts.push_back(subjectName + " is " + (about->npc ? "someone they know of" : "a traveller") + ".");
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
                    if (known)
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
                    t.score = 3.2 + belief.confidence * .3;
                    t.talk["topic.known"] = words.byName ? "yes" : "no";
                    t.talk["topic.has_nickname"] = words.nickname.empty() ? "no" : "yes";
                    if (words.byName)
                        t.as = words.subject;
                    t.talk["topic.subject"] = words.subject;
                    t.talk["topic.deed"] = words.phrase;
                    if (!words.nickname.empty())
                        t.talk["topic.nickname"] = words.nickname;
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
                if (calendarDays_ - n.day > NewsDays)
                    continue;
                AmbientTopic t;
                t.kind = "news";
                t.source = teller;
                t.claim = n.kind;
                t.talk["topic.kind"] = n.kind;           // marriage, loss, work, crime: for the scenes.
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
        t.score = 1.5;
        t.facts.push_back(name(a) + " and " + name(b) + " are friends.");
        consider(std::move(t));
    }
    // Acquaintances, and a day worth remarking on.
    if (ab.familiarity >= 10 && (plan.kind != "work" || (cell(ea->cellId) && skyAt(ea->cellId, ea->position) >= 1)))
    {
        AmbientTopic t;
        t.kind = "day";
        t.source = a;
        t.score = 1 + (plan.kind == "festival" ? .3 : 0);
        t.facts.push_back(name(a) + " and " + name(b) + " know each other.");
        consider(std::move(t));
    }
    sceneTopics(a, b, consider);
    // What the written scenes need of any topic: who the two are to each other, the place, the hour, the town.
    if (!best.kind.empty())
    {
        best.talk["pair.band"] = pairBand(a, b);
        talkMoment(a, best.talk);
    }
    return best;
}

std::string World::pairBand(const std::string& a, const std::string& b) const
{
    // Who the two are to each other, nearest first (doc 30's bands; doc 62's pair.band).
    const Bond ab = bonds_.find(a, b) ? *bonds_.find(a, b) : Bond{};
    const Bond ba = bonds_.find(b, a) ? *bonds_.find(b, a) : Bond{};
    const auto* spouse = society_.spouse(a);
    if (spouse && *spouse == b)
        return "spouses";
    if (society_.family(a, b) || society_.household(a, b))
        return "family";
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

void World::talkTown(const std::string& who, std::map<std::string, std::string>& facts)
{
    // What the town has been talking of (doc 62's world.* facts): the same news the topics are made of, as facts any
    // line may test, whatever it's about.
    const auto* e = entity(who);
    if (!e)
        return;
    const auto townId = townIdOf(e->cellId);
    if (const auto seen = priceSeen_.find(townId); seen != priceSeen_.end())
        for (const auto& [item, p] : seen->second)
            if (p.dir != 0 && calendarDays_ - p.day <= 1.5 && (item == "herbs" || item == "meal"))
                facts["world." + item + "_price"] = p.dir > 0 ? "up" : "down";
    bool bandits = false;
    for (const auto& c : roads_.caravans)
        bandits |= c.status == "raided" && (c.from == townId || c.to == townId);
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
    facts["world.wedding"] = lately.count("marriage") ? "recent" : "no";
    facts["world.birth"] = lately.count("birth") ? "recent" : "no";
    // The place itself: how many are about, and whether a traveller (a player) is near.
    int people = 0;
    bool traveller = false;
    for (const auto* o : entitiesIn(e->cellId))
        if (o && !o->dead && o->id != who)
        {
            people += o->npc;
            traveller |= !o->npc && apart(o->position, e->position) <= 12;
        }
    facts["here.crowd"] = people >= 6 ? "crowd" : people >= 3 ? "busy" : "quiet";
    facts["here.player_near"] = traveller ? "yes" : "no";
}

void World::talkOrigin(const std::string& id, std::string& from, std::string& accent) const
{
    // Where a resident grew up, and how strongly they still speak like it (doc 62, phase 3). Worked out from who they
    // are, never stored, so the same resident always has the same: most grew up where they live; of each place's grown
    // folk about one in seven came from another (one in four in Ridgemere, the city everyone comes to). The old and
    // the never-moved speak broadest; those settled elsewhere fade.
    static const std::vector<std::string> Regions{"ridgemere", "frosthollow", "coldwater", "oakhaven", "mirebridge",
                                                  "larkfield", "southmeadow"};
    from.clear();
    accent.clear();
    const auto* e = entity(id);
    const auto* life = society_.resident(id);
    if (!e || !life)
        return;
    const auto home = townIdOf(life->homeCell.empty() ? e->cellId : life->homeCell);
    if (std::find(Regions.begin(), Regions.end(), home) == Regions.end())
        return;                                     // (Somewhere that is none of the realm's: no dialect of its own.)
    std::uint64_t h = 1469598103934665603ULL;
    for (const unsigned char c : "origin|" + id)
        h = (h ^ c) * 1099511628211ULL;
    h ^= h >> 29;
    const auto stage = lifeStage(e->age);
    const bool grown = stage == LifeStage::Adult || stage == LifeStage::Old;
    const bool moved = grown && int(h % 100) < (home == "ridgemere" ? 25 : 15);
    from = home;
    if (moved)
    {
        std::vector<std::string> others;
        for (const auto& r : Regions)
            if (r != home)
                others.push_back(r);
        from = others[(h >> 8) % others.size()];
    }
    const int roll = int((h >> 20) % 100);
    if (stage == LifeStage::Old)
        accent = moved ? "light" : "broad";
    else if (!moved)
        accent = roll < 20 ? "broad" : roll < 85 ? "light" : "none";
    else
        accent = roll < 10 ? "broad" : roll < 60 ? "light" : "none";
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
    facts["here.weather"] = c && c->outdoors ? weatherName(c->weather) : "indoors";
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
    // A little chance in the everyday topics, so the same two don't always talk of the same thing.
    const auto jitter = [&](int salt) {
        std::uint64_t h = std::hash<std::string>{}(a + "|" + b) ^ (std::uint64_t(calendarDays_ * 24) * 0x9E3779B97F4A7C15ULL) ^
                          std::uint64_t(salt) * 1099511628211ULL;
        h ^= h >> 31;
        return double(h % 1000) / 1000.0 * .7;
    };
    const auto topic = [&](const char* kind, double score) {
        AmbientTopic t;
        t.kind = kind;
        t.source = a;
        t.score = score;
        return t;
    };
    // Prices that moved lately in this town.
    if (const auto seen = priceSeen_.find(townId); seen != priceSeen_.end())
        for (const auto& [item, p] : seen->second)
            if (p.dir != 0 && calendarDays_ - p.day <= 1.5)
            {
                auto t = topic("prices", 2.2);
                t.talk["topic.item"] = item;
                t.talk["topic.dir"] = p.dir > 0 ? "up" : "down";
                t.talk["topic.what"] = item == "meal" ? "a meal" : "herbs";
                t.talk["topic.price"] = std::to_string(std::max(1, int(std::lround(p.factor * (item == "meal" ? 6 : 2))))) + " pennies";
                t.facts.push_back(std::string(item == "meal" ? "Meals" : "Herbs") + " cost " + (p.dir > 0 ? "more" : "less") + " in town lately.");
                consider(std::move(t));
            }
    // A price a wolf's trade lowered lately (doc 57, 5): said with its cause, the wolf named as the teller knows them
    // (a name it was given, or how they look), never by a name it wasn't.
    if (const auto causes = priceCauses_.find(townId); causes != priceCauses_.end() && knower_)
        for (const auto& cause : causes->second)
            if (calendarDays_ - cause.day <= 1.5)
            {
                const auto known = knower_(a, cause.who);
                const auto* good = items::good(cause.item);
                auto t = topic("price_cause", 2.6);
                t.subject = cause.who;
                t.talk["topic.known"] = known.byName ? "yes" : "no";
                t.talk["topic.who"] = known.words;
                t.talk["topic.what"] = cause.item == "meal" ? "meals" : good ? lowered(good->name.substr(0, good->name.find(" ("))) : cause.item;
                t.facts.push_back(name(a) + " says " + t.talk["topic.what"] + " got cheaper in town since " + known.words + " brought some in.");
                consider(std::move(t));
            }
    // Caravans to and from this town, and the bandits on the roads.
    for (const auto& c : roads_.caravans)
    {
        const bool ours = c.from == townId || c.to == townId;
        if (!ours)
            continue;
        const auto other = c.from == townId ? c.to : c.from;
        std::string kind = c.status == "raided" ? "raided" : c.status == "arrived" && c.to == townId ? "arrived"
                           : c.status == "travelling" && c.from == townId && c.leg > 0 ? "left" : "";
        if (kind.empty())
            continue;
        auto t = topic(kind == "raided" ? "bandits" : "caravan", kind == "raided" ? 2.4 : 1.9);
        t.talk["topic.kind"] = kind;
        t.talk["topic.where"] = townWords(other);
        t.facts.push_back("A caravan between here and " + townWords(other) + " " + kind + ".");
        consider(std::move(t));
        if (kind == "raided")
        {
            auto road = topic("caravan", 2.1);
            road.talk["topic.kind"] = "raided";
            road.talk["topic.where"] = townWords(other);
            consider(std::move(road));
        }
    }
    for (const auto& camp : roads_.camps)
        if (camp.active && calendarDays_ - camp.lastRaid <= 3)
        {
            auto t = topic("bandits", 2.0);
            t.talk["topic.kind"] = "camp";
            if (const auto* where = cell(camp.cell))
                t.talk["topic.where"] = where->name;
            consider(std::move(t));
            break;
        }
    // Crime in town, not between the two talking.
    for (const auto& i : crime_.incidents)
        if (i.town == townId && calendarDays_ - i.day <= 2 && i.victim != a && i.victim != b && i.offender != a && i.offender != b)
        {
            auto t = topic("crime", 2.4);
            t.talk["topic.kind"] = i.kind == "assault" ? "assault" : "theft";
            t.talk["topic.victim"] = name(i.victim);
            t.talk["topic.subject"] = i.witnesses.empty() ? std::string("someone") : name(i.offender);
            t.facts.push_back(name(i.victim) + " was " + (i.kind == "assault" ? "attacked" : "robbed") + " in town.");
            consider(std::move(t));
        }
    // The town's news: weddings, deaths, apprenticeships, arrests, newcomers.
    if (const auto found = townNews_.find(townId); found != townNews_.end())
        for (const auto& n : found->second)
        {
            if (calendarDays_ - n.day > TownNewsDays || n.subject == a || n.subject == b || n.other == a || n.other == b)
                continue;
            const bool fresh = n.kind == "newcomer";
            auto t = topic(fresh ? "newcomer" : n.kind == "arrest" ? "crime" : "life", fresh ? 1.8 : n.kind == "death" ? 2.3 : 2.0);
            t.talk["topic.kind"] = n.kind;
            t.talk["topic.subject"] = name(n.subject);
            if (!n.other.empty())
                t.talk["topic.where"] = name(n.other);
            consider(std::move(t));
        }
    // A festival today, or one called for the days ahead.
    const auto community = communityOf(ea->cellId);
    const auto plan = dayPlan(community);
    if (plan.kind == "festival")
    {
        auto t = topic("festival", 2.0);
        t.talk["topic.when"] = "today";
        t.talk["here.festival"] = plan.name;
        consider(std::move(t));
    }
    else
        for (const auto& f : festivals_)
            if (f.community == community && f.day > std::int64_t(calendarDays_) && f.day - calendarDays_ <= 3)
            {
                auto t = topic("festival", 1.9);
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
            auto t = topic("player", 1.6 + jitter(7) * .5);
            const auto* bond = bonds_.find(a, id);
            const bool knows = bond && bond->familiarity >= 5;
            // (Named only if the speaker was given the name: doc 56, 6.)
            const auto given = knows ? (namer_ ? namer_(a, id) : e.name) : std::string();
            t.talk["topic.known"] = !given.empty() ? "yes" : "no";
            t.talk["topic.subject"] = !given.empty() ? given : strangerWords(e.appearance);
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
            auto t = topic("accent", 1.3 + jitter(9) * .6 + (accentB == "broad" ? .3 : 0));
            t.talk["topic.accent_of"] = fromB;
            t.talk["topic.accent_town"] = townWords(fromB);
            consider(std::move(t));
        }
    }
    // The everyday: family, work, the weather, the town's own lore, and talk for talk's sake.
    if (const auto* spouse = society_.spouse(a); (spouse && *spouse == b) || society_.family(a, b) || society_.household(a, b))
        consider(topic("family", 1.4 + jitter(1)));
    const auto* ja = society_.jobOf(a);
    const auto* jb = society_.jobOf(b);
    if ((ja && jb && (ja->work.cell == jb->work.cell || ja->title == jb->title)) || (ja && ja->work.cell == ea->cellId))
        consider(topic("work", 1.1 + jitter(2)));
    if (const auto* c = cell(ea->cellId); c && c->outdoors)
        if (const auto here = weatherAt(c->id, ea->position); skyAt(c->id, ea->position) >= 1 || (here.kind == Weather::Fog && here.intensity >= .3))
            consider(topic("weather", 1.0 + jitter(3)));
        else
            consider(topic("weather", .6 + jitter(3)));     // (A fair sky is remarked on too, less often: doc 62, phase 7.)
    consider(topic("lore", .7 + jitter(4)));
    consider(topic("smalltalk", .8 + jitter(5)));
    // More of everyday life (doc 62, phase 7: no conversation heard twice in a hundred hours): each its own topic,
    // so the two wander from one subject to another hour by hour.
    static const char* const Everyday[] = {"food", "home", "pastimes", "animals", "health", "memories",
                                           "dreams", "faith", "travel", "land", "season"};
    int salt = 20;
    for (const char* kind : Everyday)
        consider(topic(kind, .7 + jitter(salt++)));
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
    for (const auto& listenerId : listeners)
    {
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
    if (t.kind == "gossip" || t.kind == "deed")
    {
        believe(pick.listener, t.subject, t.claim, pick.teller, t.confidence * .7, t.incident, t.as);   // (t.as: doc 56.)
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
