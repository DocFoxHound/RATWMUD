// The economy orchestrator's plan (Core/RatwOrchestrator.h; Docs/Design/46-economy-orchestrator.md), on made-up snapshots:
// how it reads a town's distress and its kind, the bands, the week's decisions (only on a decision's day, never below a
// holder's need, every penny of the pot placed), the Dungeon Master's steers, prices, the saved state, and that the same
// snapshot always gives the same brief, on its own thread or not.
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
// shops. A rich great house in Wellby, a lean till in Hungerford, the land's church.
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
    s.goods.push_back({"wellby", "bread", 60, 40, 10, 2, 2, 25, true});
    s.goods.push_back({"hungerford", "bread", 120, 40, 10, 2, 2, 25, true});
    s.goods.push_back({"wellby", "ring", 2, 4, 1, 40, 40, 0, false});
    s.towns.push_back({"wellby", 300, 200, 0, 0});
    s.towns.push_back({"hungerford", 50, 300, 0, 0});
    s.holders.push_back({"house:gold", "wellby", HolderKind::House, 20000, 50, 100});
    s.holders.push_back({"till:stall", "hungerford", HolderKind::Till, 10, 10, 60});
    s.holders.push_back({"town:all:church", "", HolderKind::Church, 500, 20, 400});
    s.holders.push_back({"stores:wellby", "wellby", HolderKind::Treasury, 1500, 40, 600});
    return s;
}

const HolderBand& holder(const Brief& b, const std::string& id)
{
    for (const auto& h : b.holders)
        if (h.id == id)
            return h;
    throw std::runtime_error("no holder " + id);
}
const TownReading& town(const Brief& b, const std::string& id)
{
    for (const auto& t : b.towns)
        if (t.id == id)
            return t;
    throw std::runtime_error("no town " + id);
}
std::int64_t ordered(const Brief& b)
{
    std::int64_t n = 0;
    for (const auto& o : b.orders)
        n += o.coins;
    return n;
}
Steer steer(const std::string& kind, const std::string& target, double strength, const std::string& item = {})
{
    Steer s;
    s.id = "steer-" + kind;
    s.kind = kind;
    s.target = target;
    s.item = item;
    s.strength = strength;
    s.from = 0;
    s.until = 30;
    return s;
}

void readsDistress()
{
    Memory m;
    const auto b = plan(land(), m);
    const auto& well = town(b, "wellby");
    const auto& hurt = town(b, "hungerford");
    expect(well.distress < .1 && well.kind.empty(), "Wellby is well (" + std::to_string(well.distress) + ")");
    expect(hurt.distress > .8, "Hungerford is in distress (" + std::to_string(hurt.distress) + ")");
    expect(hurt.kind == "empty purses", "with food in its shop but none in its purses: " + hurt.kind);
    expect(hurt.hungry == .5 && hurt.starving == .2 && hurt.idle == 12, "Its sensors: half hungry, a fifth starving, twelve idle");
    expect(b.towns.front().id == "hungerford", "The towns come most in distress first");
    expect(std::abs(well.foodCost - 4) < 1e-9 && well.wageFloor == 8, "A day's bread costs 4p in Wellby; the living wage, a day's food for two, 8p");
}

void bandsAndTheWeek()
{
    Memory m;
    const auto b = plan(land(), m);
    const auto& gold = holder(b, "house:gold");
    expect(gold.band == "cap" && gold.need == 700, "The house holds far over its need (14 days of 50p): " + gold.band);
    expect(holder(b, "till:stall").band == "lean" && holder(b, "till:stall").toSpend == 0, "A lean till spends nothing");
    expect(gold.toSpend > 15000, "Over the cap, the house sends nearly all its excess out (" + std::to_string(gold.toSpend) + "p)");
    expect(gold.cash - gold.toSpend >= std::int64_t(std::ceil(1.5 * gold.need)), "and never below its band's top");
    expect(b.pot == ordered(b), "Every penny of the pot is ordered somewhere (" + std::to_string(b.pot) + "p)");
    std::int64_t toHungerford = 0;
    for (const auto& o : b.orders)
        if (o.town == "hungerford")
            toHungerford += o.coins;
    expect(toHungerford > b.pot / 2, "Most of it goes to the town in distress");
    for (const auto& o : b.orders)
        expect(o.channel != "price support" || o.town == "hungerford", "A well town needs no price support");
    // A day that isn't a decision's measures, and sends nothing.
    Memory m2;
    const auto day = plan(land(false), m2);
    expect(!day.decided && day.pot == 0 && day.orders.empty(), "Other days only measure");
    expect(day.prices.size() == 3, "but a good it sees for the first time is priced at once");
    expect(plan(land(false), m2).prices.empty(), "and not again until the week's decision");
    expect(holder(day, "house:gold").band == "cap", "though the bands show where each stands");
    // Before a day is counted, no bands at all.
    auto first = land(false);
    first.counted = 0;
    Memory m3;
    expect(holder(plan(first, m3), "house:gold").band == "warming", "A new world waits a day before its bands");
}

void theWeekIsWeighed()
{
    // Six good days and a bad one: the decision weighs the week, not the evening.
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
        plan(calm, m);
    }
    auto bad = land(true);
    bad.day = 7;
    const auto b = plan(bad, m);
    const auto& t = town(b, "hungerford");
    expect(b.decided && t.week > 0 && t.week < t.raw / 3, "The week's distress is its mean (" + std::to_string(t.week) + " against tonight's " +
                                                              std::to_string(t.raw) + ")");
    expect(m.week.empty() && m.decided == 7, "and the week's measures start afresh after a decision");
}

void steers()
{
    Memory m0;
    const auto plain = plan(land(), m0);
    {
        auto s = land();
        s.steers.push_back(steer("holder", "house:gold", 0));
        Memory m;
        const auto b = plan(s, m);
        expect(holder(b, "house:gold").band == "spared" && holder(b, "house:gold").toSpend == 0, "A spared house keeps what it has");
        expect(b.steers.size() == 1, "and the brief names the steer it weighed");
    }
    {
        auto s = land();
        s.steers.push_back(steer("holder", "stores:wellby", 3));
        Memory m;
        expect(holder(plan(s, m), "stores:wellby").toSpend > holder(plain, "stores:wellby").toSpend, "A squeezed treasury spends more");
    }
    {
        auto s = land();
        s.steers.push_back(steer("channel", "works", 0));
        Memory m;
        const auto b = plan(s, m);
        for (const auto& o : b.orders)
            expect(o.channel != "works", "A closed channel gets nothing");
        expect(b.pot == ordered(b), "and the pot still all goes somewhere");
    }
    {
        auto s = land();
        s.steers.push_back(steer("town", "hungerford", 0));
        Memory m;
        const auto b = plan(s, m);
        expect(town(b, "hungerford").distress == 0, "A town weighed at nothing reads as well");
        expect(town(b, "wellby").share > town(plain, "wellby").share, "and the pot goes elsewhere");
    }
    {
        auto s = land();
        s.steers.push_back(steer("price", "*", 3, "bread"));
        Memory m;
        const auto b = plan(s, m);
        for (const auto& p : b.prices)
            if (p.item == "bread")
                expect(std::abs(p.would - 2 * 1.25) < 1e-9 || p.would <= 2 * 1.25, "A price scare moves bread at most a quarter in a week");
    }
    {
        // Pressure on a land in no distress: more of the comfortable's excess goes out.
        auto s = land();
        s.holders[0].cash = 1300;                   // (The house a little over its band's top.)
        Memory a, b;
        const auto easy = plan(s, a);
        s.steers.push_back(steer("pressure", "", 3));
        expect(plan(s, b).pot > easy.pot, "Pressure sends more out");
    }
    {
        // Steers out of their days are not weighed.
        auto s = land();
        auto late = steer("holder", "house:gold", 0);
        late.from = 9;
        s.steers.push_back(late);
        Memory m;
        expect(holder(plan(s, m), "house:gold").band == "cap", "A steer that starts tomorrow isn't weighed today");
    }
    std::string problem;
    expect(validSteer(steer("pressure", "", 2), problem), "A pressure steer of 2 is fine");
    expect(!validSteer(steer("pressure", "", 9), problem), "of 9 is not");
    expect(!validSteer(steer("holder", "house:gold", 1), problem), "A holder is spared (0) or squeezed (1.5 to 4), not 1");
    expect(!validSteer(steer("channel", "bribes", 1), problem), "No such channel");
    expect(!validSteer(steer("price", "*", 2), problem), "A price steer names a good");
    auto long_ = steer("town", "wellby", 1);
    long_.until = long_.from + 57;
    expect(!validSteer(long_, problem), "A steer lasts at most 56 days");
    std::vector<ScriptedSteer> script;
    expect(readSteerScript(R"([{"day": 9, "kind": "town", "target": "wellby", "strength": 2, "days": 7}])", script, problem) &&
               script.size() == 1 && script[0].day == 9 && script[0].steer.target == "wellby" && script[0].days == 7,
           "A test's steer script reads");
    expect(!readSteerScript("{}", script, problem), "A script is an array");
}

void pricesAndMargin()
{
    auto s = land();
    s.goods[0].stock = 2;                           // (Wellby's bread nearly gone: dearer.)
    s.goods[2].stock = 200;                         // (A glut of rings: cheaper.)
    Memory m;
    const auto b = plan(s, m);
    for (const auto& p : b.prices)
    {
        if (p.town == "wellby" && p.item == "bread")
            expect(p.would > 2 && p.would <= 2.5, "Scarce bread costs more, a quarter at most in a week");
        if (p.item == "ring")
            expect(p.would < 40 && p.would >= 30, "A glut of rings costs less");
    }
    expect(m.price.count("wellby|bread") && b.margin > 0, "It remembers the prices it set, and keeps a margin");
}

void sameEveryTime()
{
    auto s = land();
    s.steers.push_back(steer("pressure", "", 1.5));
    Memory a, b;
    const auto one = briefText(plan(s, a), true), two = briefText(plan(s, b), true);
    expect(one == two, "The same snapshot gives the same brief");
    Runner threaded(true), inline_(false);
    threaded.submit(s, Memory{});
    inline_.submit(s, Memory{});
    expect(threaded.pending() && threaded.pendingDay() == 8, "A plan is pending for day 8");
    const auto t = threaded.take(), i = inline_.take();
    expect(briefText(t.first, true) == one && briefText(i.first, true) == one, "on its own thread or not");
    expect(!threaded.pending(), "and once taken, nothing is pending");
}

void savedState()
{
    State st;
    Memory m;
    st.last = plan(land(false), m);
    st.decision = plan(land(), m);
    st.memory = m;
    st.steers.push_back(steer("town", "hungerford", 2));
    st.nextSteer = 4;
    const auto back = readState(stateJson(st));
    expect(back.steers.size() == 1 && back.steers[0].target == "hungerford" && back.nextSteer == 4, "Its steers are saved");
    expect(back.memory.price == st.memory.price && back.memory.decided == st.memory.decided && back.memory.margin == st.memory.margin,
           "and its memory");
    expect(back.decision.decided && back.decision.pot == st.decision.pot && back.decision.orders.size() == st.decision.orders.size(),
           "and the week's decision");
    std::int64_t savedOver = 0;
    for (const auto& h : back.decision.holders)
        savedOver += h.band == "over" || h.band == "cap";
    expect(savedOver == std::int64_t(back.decision.holders.size()), "The saved brief keeps only the holders over their band");
    Dials d;
    std::string problem;
    expect(readDialsText(R"({"mode": "off", "needDays": 10, "weights": {"hungry": 4}})", d, problem) && d.mode == "off" && d.needDays == 10 &&
               d.wHungry == 4 && d.cap == 4,
           "Dials read from JSON, the rest kept");
    expect(!readDialsText(R"({"mode": "loud"})", d, problem), "An unknown mode is refused");
    expect(!readDialsText(R"({"cap": 1})", d, problem), "and dials out of range");
}

void theWageTable()
{
    // Seeded at once from the dials' starts, never under the living floor.
    Memory m;
    auto s = land(false);
    const auto first = plan(s, m);
    expect(first.wages.at("wellby").at("help") == 16 && first.wages.at("wellby").at("guard") == 20, "Each town's table starts from the dials");
    expect(first.wages.at("wellby").at("keeper") == 8 && first.wages.at("wellby").at("odd job") == 4, "and a keeper's and an odd job's");
    // Posts going begging raise their pay at the week's decision; not on other days.
    s.towns[0].unfilled["help"] = 2;
    expect(plan(s, m).wages.at("wellby").at("help") == 16, "A day's measure moves no wage");
    auto week = land(true);
    week.towns[0].unfilled["help"] = 2;
    const auto raised = plan(week, m);
    expect(std::abs(raised.wages.at("wellby").at("help") - 17.6) < 1e-9, "Help going begging is paid a tenth more at the decision");
    // Many idle and nothing begging: it eases, but not below the living floor.
    expect(raised.wages.at("hungerford").at("labour") < 12 || town(raised, "hungerford").kind == "empty purses",
           "Many idle: the town's labour pays less, unless it is in want of work or money");
    Memory n;
    auto lean = land(true);
    lean.dials.wageStart["labour"] = 1;              // (A start under the floor.)
    expect(plan(lean, n).wages.at("wellby").at("labour") == 8, "A wage is never under the living floor");
    // Saved.
    State st;
    st.memory = m;
    expect(readState(stateJson(st)).memory.wage == m.wage, "The table is saved");
    Dials d;
    std::string problem;
    expect(readDialsText(R"({"wages": {"help": 20}, "wageRaise": 0.2})", d, problem) && d.wageStart.at("help") == 20 && d.wageRaise == .2 &&
               d.wageStart.at("guard") == 20,
           "Its starts and steps are dials");
}
} // namespace

int main()
{
    try
    {
        readsDistress();
        bandsAndTheWeek();
        theWeekIsWeighed();
        steers();
        pricesAndMargin();
        sameEveryTime();
        savedState();
        theWageTable();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
    std::cout << "orchestrator: " << checks << " checks passed\n";
    return 0;
}
