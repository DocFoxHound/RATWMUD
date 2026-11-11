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
    // The market's news (prices, events, ships, wagons, the Times, the harvest, the town's lives) drawn once an hour for
    // the whole place, not a pair: whichever pair of a cell talks, the hour's draw decides which of what is live is the
    // talk of it, so each is heard while it lasts (Phase 8 repair; a pair's own draw would let the highest ceiling win).
    const auto hourly = [&](int salt) {
        std::uint64_t h = std::hash<std::string>{}("hour|" + townId) ^ (std::uint64_t(calendarDays_ * 24) * 0x9E3779B97F4A7C15ULL) ^
                          std::uint64_t(salt) * 1099511628211ULL;
        h ^= h >> 29;
        h *= 0xBF58476D1CE4E5B9ULL;
        h ^= h >> 32;
        return double(h % 1000) / 1000.0 * .4;
    };
    const auto topic = [&](const char* kind, double score) {
        AmbientTopic t;
        t.kind = kind;
        t.source = a;
        t.score = score;
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
        auto t = topic(kind == "raided" ? "bandits" : "caravan", kind == "raided" ? 2.4 : kind == "late" ? 1.75 + hourly(13)
                                                                  : kind == "due" ? 1.5 + hourly(13) : 1.7 + hourly(13));
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
            auto road = topic("caravan", 2.1);
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
        auto t = topic("caravan", 1.45 + hourly(14));
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
            auto t = topic("caravan", 1.65 + hourly(15));
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
        double score = 1.65 + hourly(16);
        for (const auto& move : talkPrices(a))
        {
            auto t = topic("prices", score);
            score -= .3;
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
                    auto c = topic("price_cause", scored + .05);   // (The same draw: the traveller named in the price's place.)
                    c.subject = id;
                    c.talk = price;
                    c.talk.erase("topic.cause");
                    c.talk.erase("topic.cause_words");
                    c.talk["topic.known"] = known.byName ? "yes" : "no";
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
            auto t = topic("ships", kind == "lost" ? 2.3 : kind == "late" ? 1.75 + hourly(18) : kind == "in" ? 1.7 + hourly(18)
                                    : kind == "due" ? 1.5 + hourly(18) : 1.3 + hourly(18));
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
            auto t = topic("event", (when == "now" ? 1.7 : when == "soon" ? 1.55 : 1.35) + hourly(19));
            t.facts.push_back("At the market: " + f["world.event_words"] + " (" + when + ").");
            consider(std::move(t));
        }
        if (f["world.times"] != "ordinary")         // (A cause is talked of only with Times it made: never in ordinary ones.)
        {
            auto t = topic("times", (f["world.times"] == "hard" || f["world.times"] == "boom" ? 1.65 : 1.45) + hourly(11));
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
                consider(topic("harvest", (word == 1 ? 1.1 : 1.5) + hourly(12)));
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
            auto t = topic(fresh ? "newcomer" : n.kind == "arrest" ? "crime" : "life", fresh ? 1.8 : n.kind == "death" ? 2.3
                                                                                        : n.kind == "arrest" ? 2.0 : 1.7 + hourly(21));
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
