// The economy's monitor (Docs/Design/46-economy-orchestrator.md; doc 64, Phase 12): the day's measures and its saved
// state.
#include "RatwOrchestratorJson.h"

#include <algorithm>
#include <cmath>

namespace ratw::orchestra
{
using json::Value;

namespace
{
// Distress smoothed over this many days.
constexpr double SmoothDays = 3;
// Each sensor's weight in a town's distress. Hardship (hungry, starving, short, poor) weighs most; the signs of trouble
// coming (idle, bare shelves, failing trade, money draining away) less.
constexpr double WHungry = 3, WStarving = 6, WShort = 1, WPoor = 1, WIdle = .5, WShelves = .25, WTrade = .25, WDrain = .25;

double clamp01(double x) { return std::clamp(x, 0.0, 1.0); }
} // namespace

Brief measure(const Snapshot& s, Memory& memory)
{
    Brief brief;
    brief.day = s.day;
    brief.moneySupply = s.moneySupply;

    // --- Reading the towns ---------------------------------------------------------------------------------------------
    std::map<std::string, TownReading> towns;
    std::map<std::string, std::vector<const ResidentSnap*>> people;
    for (const auto& r : s.residents)
        if (!r.town.empty())
            people[r.town].push_back(&r);
    // Food cost: the cheapest food a town's shops have in, a day of it; a town with none pays the land's cheapest, carted in.
    std::map<std::string, double> cheapest;           // Town -> pennies a nourishment.
    double landCheapest = 0;
    std::map<std::string, double> shopFood;           // Town -> nourishment on its shops' shelves.
    for (const auto& g : s.goods)
        if (g.nourish > 0 && g.stock > 0 && g.price > 0)
        {
            const double per = g.price / g.nourish;
            auto& c = cheapest[g.town];
            if (c <= 0 || per < c)
                c = per;
            if (landCheapest <= 0 || per < landCheapest)
                landCheapest = per;
            shopFood[g.town] += double(g.stock) * g.nourish;
        }
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> takings;   // Town -> takings, running.
    for (const auto& sh : s.shops)
    {
        auto& t = takings[sh.town];
        t.first += sh.takings;
        t.second += sh.running;
    }
    std::map<std::string, const TownSnap*> townSnaps;
    for (const auto& t : s.towns)
        townSnaps[t.id] = &t;
    std::map<std::string, std::pair<int, int>> homesShort;   // Town -> short households, households.
    for (const auto& [id, residents] : people)
    {
        auto& t = towns[id];
        t.id = id;
        t.people = int(residents.size());
        const auto c = cheapest.find(id);
        t.foodCost = (c != cheapest.end() ? c->second : landCheapest > 0 ? landCheapest * 1.5 : 5. / NourishADay) * NourishADay;
    }
    for (const auto& h : s.homes)
    {
        auto it = towns.find(h.town);
        if (it == towns.end() || h.members <= 0)
            continue;
        const double days = double(h.nourishment) / (NourishADay * h.members) + double(h.cash) / (it->second.foodCost * h.members);
        auto& hs = homesShort[h.town];
        hs.first += days < 7;
        ++hs.second;
    }
    for (auto& [id, t] : towns)
    {
        const auto& residents = people[id];
        int hungry = 0, starving = 0, poor = 0, grown = 0, workers = 0, idle = 0;
        for (const auto* r : residents)
        {
            hungry += r->hunger >= 70;
            starving += r->hunger >= 90;
            if (r->age >= 16)
            {
                ++grown;
                poor += double(r->cash) < 7 * t.foodCost;
            }
            if (r->age >= 16 && r->age < 65 && !r->dependent)
            {
                ++workers;
                idle += r->earned3 <= 0;
            }
        }
        const double n = std::max(1, t.people);
        t.hungry = hungry / n;
        t.starving = starving / n;
        t.poor = grown ? double(poor) / grown : 0;
        t.idle = s.counted >= 3 ? idle : -1;        // (-1: not known yet.)
        const auto hs = homesShort[id];
        t.short_ = hs.second ? double(hs.first) / hs.second : 0;
        t.shopFoodDays = shopFood[id] / (NourishADay * n);
        const auto tk = takings[id];
        t.takingsRatio = tk.second > 0 ? double(tk.first) / double(tk.second) : 1;
        if (const auto ts = townSnaps.find(id); ts != townSnaps.end())
        {
            t.netInflow = ts->second->inflow - ts->second->outflow;
            t.playerIn = ts->second->playerIn, t.playerOut = ts->second->playerOut, t.playerGoods = ts->second->playerGoods;
        }
        // (Draining is judged on a week's average: a reckoning's day, or a caravan's, is no trend.)
        const auto knownNet = memory.net.find(id);
        const double net = knownNet == memory.net.end() ? double(t.netInflow) : knownNet->second + (double(t.netInflow) - knownNet->second) / 7;
        memory.net[id] = net;

        // Distress: each sensor as a part of a crisis (0..1), weighed, and saturating (1 - e^-sum).
        const double cHungry = clamp01(t.hungry / .2), cStarving = clamp01(t.starving / .05), cShort = clamp01(t.short_ / .5),
                     cPoor = clamp01(t.poor / .5), cIdle = clamp01(workers && s.counted >= 3 ? double(idle) / workers / .5 : 0),
                     cShelves = clamp01((2 - t.shopFoodDays) / 2), cTrade = clamp01(1 - t.takingsRatio),
                     cDrain = clamp01(-net / (n * t.foodCost));
        const double sum = WHungry * cHungry + WStarving * cStarving + WShort * cShort + WPoor * cPoor + WIdle * cIdle +
                           WShelves * cShelves * (cHungry + cShort > 0 ? 1 : .1) + WTrade * cTrade + WDrain * cDrain;
        const double raw = 1 - std::exp(-sum);
        t.raw = raw;
        const auto known = memory.distress.find(id);
        t.distress = memory.day < 0 || known == memory.distress.end() ? raw : known->second + (raw - known->second) / SmoothDays;
        memory.distress[id] = t.distress;
        auto& week = memory.week[id];
        week.first += raw;
        ++week.second;
        // Its kind: what its trouble mostly is.
        const double need = cHungry + cStarving + cShort;
        const std::pair<const char*, double> kinds[] = {{"empty shelves", (need + .25 * cPoor) * cShelves},
                                                        {"empty purses", (need + cPoor) * (1 - cShelves)},
                                                        {"no work", cIdle},
                                                        {"failing trade", cTrade},
                                                        {"draining", cDrain}};
        double best = 0;
        t.kind.clear();
        if (t.distress >= .1)
            for (const auto& [name, score] : kinds)
                if (score > best + 1e-9)
                    best = score, t.kind = name;
        for (const auto& [name, score] : kinds)
            memory.weekKinds[id + "|" + name] += score;
        // When the week is weighed: its distress, and the kind of trouble it mostly had.
        if (s.decide)
        {
            t.week = week.first / std::max(1, week.second);
            best = 0;
            t.kind.clear();
            if (t.week >= .1)
                for (const auto& [name, score] : kinds)
                    if (const double w = memory.weekKinds[id + "|" + name]; w > best + 1e-9)
                        best = w, t.kind = name;
        }
    }
    double landPeople = 0, landWeighted = 0;
    for (const auto& [id, t] : towns)
        landPeople += t.people, landWeighted += (s.decide ? t.week : t.distress) * t.people;
    brief.landDistress = landPeople > 0 ? landWeighted / landPeople : 0;
    brief.decided = s.decide;

    // --- How the land's money is spread ----------------------------------------------------------------------------------
    double held = 0;
    for (const auto& r : s.residents)
        held += double(std::max<std::int64_t>(0, r.cash));
    brief.residentShare = s.moneySupply > 0 ? held / double(s.moneySupply) : 0;
    // The poorer half's share of the households' money, by household: the households holding least a head, a half of
    // everyone living in them (a child's own purse is small; its household's is what feeds it).
    {
        std::vector<std::pair<double, const HomeSnap*>> homes;
        double everyone = 0, homesHeld = 0;
        for (const auto& h : s.homes)
            if (h.members > 0)
            {
                homes.push_back({double(std::max<std::int64_t>(0, h.cash)) / h.members, &h});
                everyone += h.members;
                homesHeld += double(std::max<std::int64_t>(0, h.cash));
            }
        std::sort(homes.begin(), homes.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        double bottom = 0, counted = 0;
        for (const auto& [perHead, h] : homes)
        {
            if (counted >= everyone / 2)
                break;
            bottom += double(std::max<std::int64_t>(0, h->cash));
            counted += h->members;
        }
        brief.bottomShare = homesHeld > 0 ? bottom / homesHeld : 0;
    }
    // The residents' median purse and Gini.
    {
        std::vector<std::int64_t> purses;
        for (const auto& r : s.residents)
            purses.push_back(std::max<std::int64_t>(0, r.cash));
        std::sort(purses.begin(), purses.end());
        if (!purses.empty())
        {
            brief.median = purses[purses.size() / 2];
            double total = 0, weighted = 0;
            for (std::size_t i = 0; i < purses.size(); ++i)
                total += double(purses[i]), weighted += double(i + 1) * double(purses[i]);
            const double n = double(purses.size());
            brief.gini = total > 0 ? (2 * weighted) / (n * total) - (n + 1) / n : 0;
        }
    }
    for (auto& [id, t] : towns)
        brief.towns.push_back(t);
    std::sort(brief.towns.begin(), brief.towns.end(), [](const TownReading& a, const TownReading& b) {
        return a.distress != b.distress ? a.distress > b.distress : a.id < b.id;
    });
    memory.day = s.day;
    if (s.decide)
    {
        memory.week.clear();
        memory.weekKinds.clear();
        memory.decided = s.day;
    }
    return brief;
}

// --- Saving ------------------------------------------------------------------------------------------------------------
// (A save from before Phase 12 holds much more: its steers, bands, orders, prices, wages, tax levels and banks. Whatever
// isn't read here is left behind.)
namespace
{
double round3(double x) { return std::round(x * 1000) / 1000; }

Brief readBrief(const Value& o)
{
    Brief b;
    if (!o.isObject())
        return b;
    b.day = std::int64_t(o.number("day", -1));
    b.decided = o.boolean("decided");
    b.landDistress = o.number("landDistress");
    b.moneySupply = std::int64_t(o.number("moneySupply"));
    b.median = std::int64_t(o.number("median"));
    b.gini = o.number("gini");
    b.residentShare = o.number("residentShare");
    b.bottomShare = o.number("bottomShare");
    for (const auto& x : o.array("towns"))
    {
        TownReading t;
        t.id = x.string("id");
        t.people = int(x.number("people"));
        t.distress = x.number("distress");
        t.raw = x.number("raw");
        t.week = x.number("week", -1);
        t.kind = x.string("kind");
        t.foodCost = x.number("foodCost");
        t.hungry = x.number("hungry");
        t.starving = x.number("starving");
        t.short_ = x.number("short");
        t.poor = x.number("poor");
        t.idle = int(x.number("idle"));
        t.shopFoodDays = x.number("shopFoodDays");
        t.takingsRatio = x.number("takingsRatio");
        t.netInflow = std::int64_t(x.number("netInflow"));
        t.playerIn = std::int64_t(x.number("playerIn"));
        t.playerOut = std::int64_t(x.number("playerOut"));
        t.playerGoods = std::int64_t(x.number("playerGoods"));
        b.towns.push_back(t);
    }
    return b;
}
} // namespace

Value briefJson(const Brief& b)
{
    auto o = Value::object();
    o.add("day", double(b.day));
    o.add("decided", b.decided);
    o.add("landDistress", round3(b.landDistress));
    o.add("moneySupply", double(b.moneySupply));
    o.add("median", double(b.median));
    o.add("gini", round3(b.gini));
    o.add("residentShare", round3(b.residentShare));
    o.add("bottomShare", round3(b.bottomShare));
    auto towns = Value::array();
    for (const auto& t : b.towns)
    {
        auto x = Value::object();
        x.add("id", t.id);
        x.add("people", t.people);
        x.add("distress", round3(t.distress));
        x.add("raw", round3(t.raw));
        if (t.week >= 0)
            x.add("week", round3(t.week));
        x.add("kind", t.kind);
        x.add("foodCost", round3(t.foodCost));
        x.add("hungry", round3(t.hungry));
        x.add("starving", round3(t.starving));
        x.add("short", round3(t.short_));
        x.add("poor", round3(t.poor));
        x.add("idle", t.idle);
        x.add("shopFoodDays", round3(t.shopFoodDays));
        x.add("takingsRatio", round3(t.takingsRatio));
        x.add("netInflow", double(t.netInflow));
        if (t.playerIn || t.playerOut || t.playerGoods)
        {
            x.add("playerIn", double(t.playerIn));
            x.add("playerOut", double(t.playerOut));
            x.add("playerGoods", double(t.playerGoods));
        }
        towns.push(x);
    }
    o.add("towns", towns);
    return o;
}

std::string briefText(const Brief& brief)
{
    return json::dump(briefJson(brief));
}

Value stateJson(const State& st)
{
    auto o = Value::object();
    o.add("day", double(st.memory.day));
    auto memory = Value::object(), distress = Value::object(), net = Value::object();
    for (const auto& [k, v] : st.memory.distress)
        distress.add(k, v);
    for (const auto& [k, v] : st.memory.net)
        net.add(k, v);
    memory.add("distress", distress);
    memory.add("net", net);
    auto week = Value::object(), weekKinds = Value::object();
    for (const auto& [k, v] : st.memory.week)
    {
        auto pair = Value::array();
        pair.push(v.first);
        pair.push(v.second);
        week.add(k, pair);
    }
    for (const auto& [k, v] : st.memory.weekKinds)
        weekKinds.add(k, v);
    memory.add("week", week);
    memory.add("weekKinds", weekKinds);
    memory.add("day", double(st.memory.day));
    memory.add("decided", double(st.memory.decided));
    o.add("memory", memory);
    o.add("brief", briefJson(st.last));
    o.add("decision", briefJson(st.decision));
    return o;
}

State readState(const Value& o)
{
    State st;
    if (!o.isObject())
        return st;
    const auto& m = o.object("memory");
    for (const auto& [k, v] : m.object("distress").fields())
        if (v.isNumber() && v.asNumber() >= 0 && v.asNumber() <= 1)
            st.memory.distress[k] = v.asNumber();
    for (const auto& [k, v] : m.object("net").fields())
        if (v.isNumber() && std::abs(v.asNumber()) < 1e12)
            st.memory.net[k] = v.asNumber();
    for (const auto& [k, v] : m.object("week").fields())
        if (v.isArray() && v.items().size() == 2 && v.items()[0].isNumber() && v.items()[1].isNumber())
            st.memory.week[k] = {v.items()[0].asNumber(), int(v.items()[1].asNumber())};
    for (const auto& [k, v] : m.object("weekKinds").fields())
        if (v.isNumber())
            st.memory.weekKinds[k] = v.asNumber();
    st.memory.day = std::int64_t(m.number("day", -1));
    st.memory.decided = std::int64_t(m.number("decided", -1));
    st.last = readBrief(o.object("brief"));
    st.decision = readBrief(o.object("decision"));
    return st;
}
} // namespace ratw::orchestra
