// The economy's monitor (Core/RatwOrchestrator.h; Docs/Design/46-economy-orchestrator.md; doc 64, Phase 12), on made-up
// snapshots: how it reads a town's distress and its kind, how it weighs the week, how the land's money is spread, the
// saved state (and a save from before Phase 12), and that the same snapshot always gives the same measures.
#include "RatwOrchestratorJson.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ratw;
using namespace ratw::orchestra;

namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

// Two towns of twenty: Wellby, fed and earning; Hungerford, with hungry, starving and penniless folk. Bread in both
// shops.
Snapshot land(bool decide = true)
{
    Snapshot s;
    s.day = 8;
    s.counted = 7;
    s.decide = decide;
    s.moneySupply = 100000;
    for (int i = 0; i < 20; ++i)
    {
        ResidentSnap well;
        well.id = "w" + std::to_string(i);
        well.town = "wellby";
        well.home = "wh" + std::to_string(i / 2);
        well.age = 30;
        well.cash = 200;
        well.hunger = 20;
        well.earned3 = 30;
        well.earned7 = 70;
        s.residents.push_back(well);
        ResidentSnap poor = well;
        poor.id = "h" + std::to_string(i);
        poor.town = "hungerford";
        poor.home = "hh" + std::to_string(i / 2);
        poor.cash = i < 10 ? 0 : 20;
        poor.hunger = i < 4 ? 95 : i < 10 ? 75 : 30;
        poor.earned3 = i < 12 ? 0 : 10;
        poor.earned7 = i < 12 ? 0 : 20;
        s.residents.push_back(poor);
    }
    for (int i = 0; i < 10; ++i)
    {
        s.homes.push_back({"wh" + std::to_string(i), "wellby", 2, 400, 500});
        s.homes.push_back({"hh" + std::to_string(i), "hungerford", 2, i < 5 ? 0 : 40, 0});
    }
    s.shops.push_back({"till:bakery", "wellby", 40, 30});
    s.shops.push_back({"till:stall", "hungerford", 5, 30});
    s.goods.push_back({"wellby", "bread", 60, 2, 25});
    s.goods.push_back({"hungerford", "bread", 120, 2, 25});
    s.towns.push_back({"wellby", 300, 200, 0, 0, 0});
    s.towns.push_back({"hungerford", 50, 300, 0, 0, 0});
    return s;
}

const TownReading& town(const Brief& b, const std::string& id)
{
    for (const auto& t : b.towns)
        if (t.id == id)
            return t;
    throw std::runtime_error("no town " + id);
}

void readsDistress()
{
    Memory m;
    const auto b = measure(land(false), m);
    const auto& well = town(b, "wellby");
    const auto& hurt = town(b, "hungerford");
    expect(well.distress < .1 && well.kind.empty(), "Wellby is well (" + std::to_string(well.distress) + ")");
    expect(hurt.distress > .8, "Hungerford is in distress (" + std::to_string(hurt.distress) + ")");
    expect(hurt.kind == "empty purses", "with food in its shop but none in its purses: " + hurt.kind);
    expect(hurt.hungry == .5 && hurt.starving == .2 && hurt.idle == 12, "Its sensors: half hungry, a fifth starving, twelve idle");
    expect(hurt.short_ == 1 && hurt.poor == 1, "every household short of a week's food, every grown wolf poor");
    expect(hurt.netInflow == -250 && well.netInflow == 100, "and the coins between the towns");
    expect(b.towns.front().id == "hungerford", "The towns come most in distress first");
    expect(std::abs(well.foodCost - 4) < 1e-9, "A day's bread costs 4p in Wellby");
    expect(std::abs(well.shopFoodDays - 1.5) < 1e-9 && std::abs(well.takingsRatio - 40. / 30) < 1e-9, "and its shops' food and takings");
    expect(!b.decided && well.week < 0, "Not the evening of the reckoning: no week weighed");
    auto unknown = land(false);
    unknown.counted = 2;
    Memory n;
    expect(town(measure(unknown, n), "hungerford").idle == -1, "Idleness waits for three days counted");
}

void theWeekIsWeighed()
{
    // Six good days and a bad one: the week is weighed, not the evening.
    Memory m;
    auto calm = land(false);
    for (auto& r : calm.residents)
        if (r.town == "hungerford")
            r.hunger = 20, r.cash = 200, r.earned3 = 30, r.earned7 = 70;
    for (auto& h : calm.homes)
        if (h.town == "hungerford")
            h.cash = 400, h.nourishment = 500;
    calm.shops[1].takings = 40;                     // (Its stall doing well, and more coming in than going out.)
    calm.towns[1].inflow = 300, calm.towns[1].outflow = 50;
    for (int day = 1; day <= 6; ++day)
    {
        calm.day = day;
        measure(calm, m);
    }
    auto bad = land(true);
    bad.day = 7;
    const auto b = measure(bad, m);
    const auto& t = town(b, "hungerford");
    expect(b.decided && t.week > 0 && t.week < t.raw / 3, "The week's distress is its mean (" + std::to_string(t.week) + " against tonight's " +
                                                              std::to_string(t.raw) + ")");
    expect(t.distress < t.raw, "and the day's distress is smoothed");
    expect(m.week.empty() && m.decided == 7, "and the week's measures start afresh after it is weighed");
}

void howTheMoneyIsSpread()
{
    Memory m;
    const auto b = measure(land(false), m);
    expect(b.moneySupply == 100000 && std::abs(b.residentShare - .042) < 1e-9, "The residents hold 4.2% of the land's money");
    expect(std::abs(b.bottomShare - 200. / 4200) < 1e-9, "the poorer half of the households 200p of their 4,200p");
    expect(b.median == 200 && std::abs(b.gini - .4643) < 1e-3, "the middle purse 200p, the Gini 0.46 (" + std::to_string(b.gini) + ")");
}

void sameEveryTime()
{
    const auto s = land();
    Memory a, b;
    expect(briefText(measure(s, a)) == briefText(measure(s, b)), "The same snapshot gives the same measures");
}

void savedState()
{
    State st;
    Memory m;
    st.last = measure(land(false), m);
    st.decision = measure(land(), m);
    st.memory = m;
    const auto back = readState(stateJson(st));
    expect(back.memory.distress == st.memory.distress && back.memory.net == st.memory.net && back.memory.decided == st.memory.decided &&
               back.memory.day == st.memory.day,
           "Its memory is saved");
    expect(briefText(back.last) == briefText(st.last) && back.decision.decided && briefText(back.decision) == briefText(st.decision),
           "and the day's and the week's measures");
    // A save from before Phase 12: the steers, bands, orders, prices, wages, tax levels and banks it held are left behind.
    json::Value old;
    std::string problem;
    expect(json::parse(R"({"mode": "off", "nextSteer": 3, "steers": [{"id": "steer-1", "kind": "pressure", "strength": 2, "from": 1, "until": 9}],
                          "deposits": {"w1": ["wellby", 120]},
                          "memory": {"distress": {"wellby": 0.25}, "price": {"wellby|bread": 2.5}, "wage": {"wellby|labour": 14}, "ladder": 2,
                                     "taxLevel": {"stores:wellby": 1.3}, "day": 8, "decided": 8},
                          "brief": {"day": 8, "mode": "off", "pot": 900, "gini": 0.5, "holders": [{"id": "house:gold", "band": "over"}],
                                    "orders": [{"from": "land", "town": "wellby", "channel": "works", "coins": 900}],
                                    "towns": [{"id": "wellby", "people": 20, "distress": 0.25, "wageFloor": 9, "share": 900}]}})",
                       old, problem),
           "An old save parses: " + problem);
    const auto read = readState(old);
    expect(read.memory.distress.at("wellby") == .25 && read.memory.decided == 8 && read.last.day == 8 && read.last.gini == .5 &&
               read.last.towns.size() == 1 && read.last.towns[0].people == 20,
           "and loads its measures, the rest left behind");
}
} // namespace

int main()
{
    try
    {
        readsDistress();
        theWeekIsWeighed();
        howTheMoneyIsSpread();
        sameEveryTime();
        savedState();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
    std::cout << "orchestrator: " << checks << " checks passed\n";
    return 0;
}
