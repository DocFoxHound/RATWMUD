// The Country's food farms (Docs/Design/71-the-country-s-industries.md, 4, Phases A and B; the user, 2026-11-01: "Yes to
// all your suggestions"): country_farms.json's defaults (two vineyards and two salt works, the crews, the wages, the
// schedule, the baskets); the yields blended by season and scaled by area and farm; crews scaled and wages falling
// through farm, area and defaults; the phased switch (an area at a time from its start, a month apart) and the founding
// (floats and settled purses made and counted as minted: the money conserved exactly); store draws from the farms' stock,
// spread by stock (their pennies evenly), past the season's cap and no further than the stock, the granary's price a
// year, the Country no longer sending an area's goods and sending them again when it is off; the farms' crews shopping as
// households, counted apart; the wages and the treasury's support for a short farm; the farms' basket; spoiling; the
// Country's basket moving to its re-weighted one; the records and their refusals, applied live; a save, and a restart's
// areas on from its first second; the preserver selling on the drying houses' stores before making its own; vegetables
// gone from Ridgemere 2.0's economy and the inns' stew of meat and bones.
//
// The catalogue is the real Data (RATW_SOURCE_DIR), country_farms.json with it.
#include "RatwCalendar.h"
#include "RatwGame.h"
#include "RatwItems.h"
#include "RatwJsonDoc.h"
#include "RatwSociety.h"
#include "RatwWire.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ratw;
namespace fs = std::filesystem;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

void catalogue()
{
    setenv("RATW_DATA_DIR", (fs::path(RATW_SOURCE_DIR) / "Data").string().c_str(), 1);
    std::string error;
    expect(items::load(&error), "the catalogue loads: " + error);
}

ResidentSpec person(const std::string& id, const std::string& role, const std::string& work, Spot job, const std::string& home,
                    int age = 30, std::int64_t purse = 60)
{
    ResidentSpec r;
    r.id = id;
    r.name = id;
    r.role = role;
    r.workLabel = work;
    r.home = {home, 2.5, 2.5};
    r.work = job;
    r.evening = r.home;
    r.serve = job;
    r.purse = purse;
    r.age = age;
    r.meals = 0;
    r.startHour = 7;
    r.endHour = 19;
    return r;
}

// A small city: a stall, an inn, a baker, a butcher, a smith (the farms' basket buys its make) and a clerk the treasury
// pays, and any `extra`; a rich treasury; the later rules that would move prices and wages off, so the farms' flows show
// plainly.
Society city(const std::function<void(EconomySpec&)>& change = {}, const std::vector<ResidentSpec>& extra = {})
{
    AuthoredRoster r;
    r.residents = {person("stall", "merchant", "keeping the stall in the square", {"square", 2.5, 2.5}, "home_stall", 40, 400),
                   person("inn", "merchant", "keeping the inn", {"inn", 2.5, 2.5}, "home_inn"),
                   person("baker", "merchant", "baker at The Loaf", {"bakery", 2.5, 2.5}, "home_baker"),
                   person("butcher", "merchant", "butcher at the Shambles", {"shambles", 2.5, 2.5}, "home_butcher", 40, 400),
                   person("smith", "merchant", "the smith at the forge", {"forge", 2.5, 2.5}, "home_smith", 40, 400),
                   person("clerk", "civilian", "clerking at the City Office", {"office", 2.5, 2.5}, "home_clerk")};
    r.residents.insert(r.residents.end(), extra.begin(), extra.end());
    r.economy.treasury = 20000;
    r.economy.storeRule = true;
    r.economy.rules = {{"stockPrices", 0}, {"shelfFollow", 0}, {"wageDrift", 0}, {"treasuryDrift", 0}, {"surplus", 0}, {"patrons", 0}};
    if (change)
        change(r.economy);
    Society s(Roster::None);
    s.configure(r);
    LifeDay day;
    day.communityOf = [](const std::string&) { return std::string("city"); };
    s.setDay(day);
    CareerWorld world;
    world.alive = [](const std::string&) { return true; };
    world.age = [&s](const std::string& id) { return s.spec(id) ? s.spec(id)->age : 30; };
    world.regard = [](const std::string&, const std::string&) { return 50.; };
    s.tendCareers(1, world);
    return s;
}

std::map<std::string, LifeBody> atWork(const Society& s)
{
    std::map<std::string, LifeBody> bodies;
    for (const auto& r : s.authored().residents)
    {
        LifeBody b{r.work.cell, r.work.x, r.work.y};
        b.age = r.age;
        bodies[r.id] = b;
    }
    return bodies;
}

int seasonOf(double day)
{
    const double d = std::fmod(day, double(calendar::DaysPerYear));
    return d < 92 ? 0 : d < 184 ? 1 : d < 275 ? 2 : 3;
}

// The society second by second from `from` to `to` (absolute days).
void run(Society& s, double from, double to)
{
    const auto bodies = atWork(s);
    for (double now = from; now < to; now += 1. / calendar::SecondsPerDay)
        s.tick(1, now, seasonOf(now), bodies);
}

// Over the turn of a day (its first loop hour): the farms tended on `day`.
void turn(Society& s, double day)
{
    run(s, day - .5 / 24, day + .5 / 24);
}

const Society::FarmNow& farm(const Society& s, const std::string& id)
{
    for (const auto& f : s.farms())
        if (f.def->id == id)
            return f;
    throw std::runtime_error("no farm " + id);
}

std::size_t areaIndex(const std::string& id)
{
    const auto& areas = items::farms().areas;
    for (std::size_t i = 0; i < areas.size(); ++i)
        if (areas[i].id == id)
            return i;
    throw std::runtime_error("no area " + id);
}

int held(const Society& s, const std::string& account, const std::string& item)
{
    const auto* a = s.account(account);
    return a ? Society::stock(*a, item) : 0;
}

void theDefaults()
{
    const auto& f = items::farms();
    expect(f.areas.size() == 6, "six food areas: " + std::to_string(f.areas.size()));
    int farms = 0, keepers = 0, hands = 0;
    std::map<std::string, int> kinds;
    for (const auto& a : f.areas)
        for (const auto& x : a.farms)
        {
            ++farms;
            keepers += x.keepers;
            hands += x.hands;
            ++kinds[x.kind];
            expect(x.reserve > 0 && x.purse > 0 && !x.yield.empty(), x.id + " has a float, a crew's purse and yields");
        }
    expect(farms == 31 && keepers == 31 && hands == 76, "31 farms, 31 keepers and 76 hands: " + std::to_string(farms) + ", " +
                                                            std::to_string(keepers) + ", " + std::to_string(hands));
    expect(kinds["vineyard"] == 2 && kinds["salt works"] == 2 && kinds["grain farm"] == 2,
           "two vineyards and two salt works (the user's decision), two grain farms");
    expect(f.keeper == 16 && f.hand == 10 && f.floatDays == 2 && f.openingStockDays == 7, "a keeper 16p and a hand 10p, two days' float");
    expect(f.start == -1 && f.every == 28 && f.order.size() == 6 && f.order.front() == "grain",
           "no switch until a world sets one; an area every 28 days, grain first");
    expect(!f.basket.empty() && !f.countryBasket.empty() && !f.switchTrades.empty(), "the farms' basket, the Country's after, the losers");
    for (const auto& [item, share] : f.countryBasket)
    {
        expect(std::none_of(f.basket.begin(), f.basket.end(), [&](const auto& b) { return b.first == item; }),
               "the Country's re-weighted basket buys nothing the farms' does: " + item);
        expect(Society::nourishment(item) == 0 && item != "aqua_vitae", "and no food or drink: " + item);
    }
    // The yields: the season means of the blend are doc 71's (a grain farm's flour in summer, 2.4).
    const auto& flour = f.areas[areaIndex("grain")].farms.front().yield.at("flour");
    double summer = 0;
    for (int d = 92; d < 184; ++d)
        summer += Society::seasonalValue(flour, d + .5);
    expect(std::abs(summer / 92 - 59.6) < .2, "a grain farm's flour in summer, the blend's mean: " + std::to_string(summer / 92));
}

void yieldsBySeason()
{
    const std::array<double, 4> v{10, 20, 30, 40};
    expect(std::abs(Society::seasonalValue(v, 46) - 10) < 1e-9 && std::abs(Society::seasonalValue(v, 138) - 20) < 1e-9 &&
               std::abs(Society::seasonalValue(v, 229.5) - 30) < 1e-9 && std::abs(Society::seasonalValue(v, 320) - 40) < 1e-9,
           "at the seasons' middles, their values");
    expect(std::abs(Society::seasonalValue(v, 92) - 15) < 1e-9, "between two middles, a straight line");
    expect(std::abs(Society::seasonalValue(v, 0) - (40 + (10 - 40) * (45. / 91))) < 1e-9, "Winter's runs on into Spring's");
    expect(std::abs(Society::seasonalValue(v, -1) - 25) < 1e-9, "before the date is known, the mean");
    expect(Society::seasonal({100, 50, 150, 200}, 138) == .5, "the seasons' factors as before");
    // A grain farm grows each day what its yields say for the day, times its area's yield scale and its own.
    for (const double day : {46., 229.})
    {
        auto s = city([](EconomySpec& e) {
            e.farms.areas["grain"].on = 1;
            e.farms.areas["grain"].yieldScale = 200;
            e.farms.farms["grain_farm_2"].yieldScale = 50;
        });
        s.takeJournal();
        turn(s, day);
        const auto& one = farm(s, "grain_farm_1");
        const auto& two = farm(s, "grain_farm_2");
        expect(std::abs(one.yieldScale - 2) < 1e-9 && std::abs(two.yieldScale - 1) < 1e-9, "the yield scales: the area's times the farm's");
        turn(s, day + 1);
        const auto& g1 = s.loopToday().farms.at("grain_farm_1");
        const double expected = Society::seasonalValue(one.def->yield.at("flour"), day + 1 + .5 / 24) * 2;
        // (Its day's flour, the oats, the straw; less the Country's table: here the flour alone, of what the day grew.)
        const int flour = held(s, one.till, "flour");
        expect(g1.grown > 0, "it grew today: " + std::to_string(g1.grown));
        // Founded on `day`: a week of its yields, then that day's and the next day's, less two days of the Country's table.
        const auto& f1 = one.def->yield.at("flour");
        const double week = 7 * Society::seasonalValue(f1, day + .5 / 24) * 2, first = Society::seasonalValue(f1, day + .5 / 24) * 2;
        const double table = Society::seasonalValue(one.def->table.at("flour"), day + .5 / 24) * 2;
        int drawn = 0;                              // (What the stall and the baker drew of it meanwhile.)
        for (const auto& e : s.takeJournal())
            drawn += e.from == one.till && e.item == "flour" && e.to != "consumed" ? e.quantity : 0;
        expect(std::abs(flour + drawn - (week + first + expected - table)) <= 3,
               "its store: the opening week and two days' flour, less the table's: " + std::to_string(flour) + " (" + std::to_string(week) +
                   " + " + std::to_string(first) + " + " + std::to_string(expected) + " - " + std::to_string(table) + ")");
        expect(held(s, two.till, "flour") < flour * .6, "the second farm at half the area's scale grows half as much");
    }
    // More flour in autumn than in winter: the season's shape.
    auto autumn = city([](EconomySpec& e) { e.farms.areas["grain"].on = 1; });
    turn(autumn, 229);
    auto winter = city([](EconomySpec& e) { e.farms.areas["grain"].on = 1; });
    turn(winter, 320);
    expect(held(autumn, farm(autumn, "grain_farm_1").till, "flour") > held(winter, farm(winter, "grain_farm_1").till, "flour"),
           "more flour in store in autumn than in winter");
}

void crewsAndWages()
{
    // Wages fall through the farm, its area and the defaults; hands scale with the area and the farm, rounded half up.
    auto s = city([](EconomySpec& e) {
        e.farms.keeper = 15;
        e.farms.areas["grain"].keeper = 18;
        e.farms.areas["grain"].crewScale = 150;
        e.farms.farms["grain_farm_1"].hand = 12;
        e.farms.farms["grain_farm_2"].crewScale = 50;
    });
    const auto& one = farm(s, "grain_farm_1");
    const auto& two = farm(s, "grain_farm_2");
    const auto& salt = farm(s, "salt_works_1");
    expect(one.keeper == 18 && one.hand == 12, "a farm's own hand's wage, its area's keeper's: " + std::to_string(one.keeper) + "/" + std::to_string(one.hand));
    expect(two.keeper == 18 && two.hand == 10, "the area's keeper's, the defaults' hand's");
    expect(salt.keeper == 15 && salt.hand == 10, "another area: the world's keeper's wage, the file's hand's");
    expect(one.hands == 6 && two.hands == 3, "4 hands at 150%: 6; at 150% of 50%: 3 (rounded half up): " + std::to_string(one.hands) + ", " +
                                                 std::to_string(two.hands));
    expect(one.wages() == 18 + 6 * 12, "its wages a day: " + std::to_string(one.wages()));
    expect(one.reserve == 2 * one.wages(), "its float: two days of its wages");
    expect(one.purse == std::llround(double(one.def->purse) * 7 / 5), "its crew's purse by its size");
}

void thePhasedSwitch()
{
    auto s = city([](EconomySpec& e) { e.farms.start = 10; });
    const auto money = s.moneySupply();
    const auto minted = s.state().minted;
    turn(s, 9);
    for (std::size_t a = 0; a < items::farms().areas.size(); ++a)
        expect(!s.farmAreaOn(a), "before the start, no area is on");
    expect(!s.account(Society::farmTill("grain_farm_1")), "and no farm is founded");
    turn(s, 10);
    expect(s.farmAreaOn(areaIndex("grain")) && !s.farmAreaOn(areaIndex("fruit")), "on the start, grain first");
    std::int64_t made = 0;
    for (const auto& f : s.farms())
        if (f.def->area == "grain")
        {
            expect(s.account(f.till) && s.account(f.crew), f.def->id + " founded");
            made += f.reserve + f.purse;
        }
    expect(s.loopToday().minted == made && s.farmsMinted() == made, "the founding: the floats and the purses, made: " + std::to_string(made));
    expect(s.state().minted == minted + made && s.conserved(), "counted as minted: the money conserved exactly");
    expect(s.moneySupply() >= money + made - 1000, "the money supply grew by the founding");   // (The day's flows aside.)
    turn(s, 37);
    expect(!s.farmAreaOn(areaIndex("fruit")), "the next area a month (28 days) later, not before");
    turn(s, 38);
    expect(s.farmAreaOn(areaIndex("fruit")) && s.farmAreaOn(areaIndex("grain")) && !s.farmAreaOn(areaIndex("meat")),
           "fruit on day 38, grain still on, meat not yet");
    turn(s, 10 + 5 * 28);
    for (std::size_t a = 0; a < items::farms().areas.size(); ++a)
        expect(s.farmAreaOn(a), "every area on by the sixth month");
    expect(std::abs(s.countryBasketShift() - 1) < 1e-9, "and the Country's basket wholly re-weighted");
    expect(s.conserved(), "conserved throughout");
    // An area's own switch over the schedule: off whatever the day.
    auto off = city([](EconomySpec& e) {
        e.farms.start = 0;
        e.farms.areas["grain"].on = 0;
    });
    turn(off, 100);
    expect(!off.farmAreaOn(areaIndex("grain")) && off.farmAreaOn(areaIndex("fruit")), "an area switched off stays off");
}

void conservationWithMinting()
{
    auto s = city([](EconomySpec& e) {
        e.farms.areas["grain"].on = 1;
        e.farms.areas["meat"].on = 1;
    });
    const auto money = s.moneySupply();
    run(s, 46 - .5 / 24, 47 + 6. / 24);                     // A day of the loop and the next morning.
    expect(s.conserved(), "a day with two areas on: conserved");
    expect(s.moneySupply() == money + s.farmsMinted(), "the money supply is the start's and the founding's: " + std::to_string(s.moneySupply()) +
                                                            " against " + std::to_string(money) + " + " + std::to_string(s.farmsMinted()));
    const auto after = s.moneySupply();
    run(s, 47 + 6. / 24, 48 + 6. / 24);
    expect(s.conserved() && s.moneySupply() == after, "the next day: nothing made, nothing lost");
    // A farm's wages were paid to its crew, who shopped in the city, counted apart from the residents.
    const auto& d = s.loopYesterday();
    const auto& g = d.farms.at("grain_farm_1");
    expect(g.wagesOwed == farm(s, "grain_farm_1").wages() && g.wagesPaid + g.support == g.wagesOwed,
           "the grain farm owed and paid its crew's wages: " + std::to_string(g.wagesPaid) + " of " + std::to_string(g.wagesOwed));
    std::int64_t crewSpent = 0;
    int crewFood = 0;
    for (const auto& [id, x] : d.farms)
    {
        crewSpent += x.crewSpent;
        crewFood += x.crewFood;
    }
    expect(crewSpent > 0 && crewFood > 0, "the crews bought food in the city's shops: " + std::to_string(crewFood));
    expect(d.residentsSpent >= 0 && d.householdsShort <= 6, "the residents' measures are the residents' (the crews apart)");
}

void drawsFromTheFarms()
{
    // The stall resells flour: its draws come from the grain farms' stores, past the Country's season cap.
    auto s = city([](EconomySpec& e) {
        e.farms.areas["grain"].on = 1;
        EconomySpec::Season tiny;
        tiny.normal = 1;                            // (The Country could send one grain piece a day.)
        e.seasons["grain"] = tiny;
    });
    turn(s, 46);
    const auto t1 = Society::farmTill("grain_farm_1"), t2 = Society::farmTill("grain_farm_2");
    const int before1 = held(s, t1, "flour"), before2 = held(s, t2, "flour");
    expect(before1 > 100 && before2 > 100, "the farms' stores hold flour");
    expect(s.countryLeft("flour") == before1 + before2, "what the city may draw is the farms' stock, not the season's cap");
    const auto country = s.cash(Society::CountryFund), treasury = s.cash("treasury"), farms = s.cash(t1) + s.cash(t2);
    const auto money = s.moneySupply();
    const auto r = s.drawFromStore("stall", "flour", 50, "restocked from the store room");
    expect(r.ok, "fifty flour drawn: " + r.message);
    expect(r.message.find("from the farms") != std::string::npos, "from the farms: " + r.message);
    expect(held(s, t1, "flour") + held(s, t2, "flour") == before1 + before2 - 50, "out of their stores");
    expect(std::abs((before1 - held(s, t1, "flour")) - (before2 - held(s, t2, "flour"))) <= 1, "spread over them by stock");
    expect(s.cash(Society::CountryFund) == country, "nothing to the Country");
    const auto toll = s.cash("treasury") - treasury;
    expect(toll > 0 && s.cash(t1) + s.cash(t2) - farms == r.total - toll, "the cost less the toll to the farms: " + std::to_string(r.total));
    expect(s.conserved() && s.moneySupply() == money, "conserved");
    // At a price a year (101% of the catalogue: the granary), whatever the season's.
    expect(r.total == std::int64_t(std::ceil(2. * 50 * .75 * 1.01 - 1e-9)), "the granary's price a year: " + std::to_string(r.total));
    // Limited by their stock: more than they hold is refused, and counted (a smaller harvest's stores, here).
    {
        auto lean = city([](EconomySpec& e) {
            e.farms.areas["grain"].on = 1;
            e.farms.areas["grain"].yieldScale = 10;
        });
        turn(lean, 46);
        const int left = held(lean, t1, "flour") + held(lean, t2, "flour");
        expect(left > 0 && left < 999, "a small store: " + std::to_string(left));
        const auto refused = lean.drawFromStore("stall", "flour", left + 1, "restocked from the store room");
        expect(!refused.ok && refused.message.find("The farms have only") != std::string::npos, "past their stock, refused: " + refused.message);
        expect(lean.loopToday().areasOut.at("grain") > 0, "and counted as the area running out");
        expect(held(lean, t1, "flour") + held(lean, t2, "flour") == left, "nothing moved");
    }
    // The Country no longer sends an area's goods, and sends them again when it is off.
    auto e = s.authored().economy;
    e.farms.areas["grain"].on = 0;
    s.adoptEconomy(e);
    const auto country2 = s.cash(Society::CountryFund);
    const auto back = s.drawFromStore("stall", "oats", 1, "restocked from the store room");
    expect(back.ok && s.cash(Society::CountryFund) > country2 - 1, "off again: the Country supplies them: " + back.message);
    expect(s.countryLeft("flour") <= 1, "and its season's cap is back");
}

void anEvenSplit()
{
    // Two like farms with like stores share many small draws: each takes half the pennies, within a penny (cut in the
    // list's order, the first lost the odd penny of every draw), and every penny of the cost less the toll is theirs.
    auto s = city([](EconomySpec& e) { e.farms.areas["grain"].on = 1; });
    turn(s, 46);
    const auto t1 = Society::farmTill("grain_farm_1"), t2 = Society::farmTill("grain_farm_2");
    expect(held(s, t1, "flour") == held(s, t2, "flour"), "like stores");
    const auto cash1 = s.cash(t1), cash2 = s.cash(t2), treasury = s.cash("treasury");
    std::int64_t paid = 0;
    for (int i = 0; i < 40; ++i)
    {
        const auto r = s.drawFromStore("stall", "flour", 2, "restocked from the store room");
        expect(r.ok, "two flour drawn: " + r.message);
        paid += r.total;
    }
    const auto got1 = s.cash(t1) - cash1, got2 = s.cash(t2) - cash2;
    expect(got1 + got2 == paid - (s.cash("treasury") - treasury), "the cost less the toll to the farms");
    expect(std::abs(got1 - got2) <= 1, "an even split: " + std::to_string(got1) + "p and " + std::to_string(got2) + "p");
    expect(held(s, t1, "flour") == held(s, t2, "flour"), "and like pieces from each");
}

void theCountryNoLongerSends()
{
    // Meat on: the butcher's raw meat comes from the cattle and pig farms; a good of the area no farm grows is had from nobody.
    auto s = city([](EconomySpec& e) { e.farms.areas["meat"].on = 1; });
    turn(s, 120);
    const auto r = s.drawFromStore("butcher", "raw_meat", 10, "restocked from the store room");
    expect(r.ok && r.message.find("from the farms") != std::string::npos, "raw meat from the farms: " + r.message);
    expect(s.farmAreaOf("raw_meat") == int(areaIndex("meat")) && s.farmAreaOf("flour") < 0, "the meat area's, not grain's");
}

void spoiling()
{
    auto s = city([](EconomySpec& e) { e.farms.areas["meat"].on = 1; });
    turn(s, 46);
    turn(s, 47);
    const auto& cattle = farm(s, "cattle_and_dairy_farm_1");
    const auto& milk = cattle.def->yield.at("milk");
    const double mean = (milk[0] + milk[1] + milk[2] + milk[3]) / 4;
    expect(held(s, cattle.till, "milk") <= int(std::ceil(mean)), "milk keeps a day: no more than a day's in store: " +
                                                                      std::to_string(held(s, cattle.till, "milk")));
    expect(s.loopToday().farms.at(cattle.def->id).spoiled > 0, "the rest spoiled, and counted");
}

void theFarmsBasket()
{
    // A farm with more than its float and tonight's wages spends the rest on the city's goods from their makers.
    auto s = city([](EconomySpec& e) { e.farms.areas["salt"].on = 1; });
    turn(s, 46);
    const auto till = Society::farmTill("salt_works_1");
    expect(s.operatorTransfer("treasury", till, "", 0, 3000).ok, "the salt works given 3,000p (moved, not made)");
    for (const char* good : {"nails", "fittings", "lock", "chain", "cookpot"})
        expect(s.create("till:job:smith", good, 99, "test stock"), std::string("the smith holds ") + good);
    const auto money = s.moneySupply();
    s.takeJournal();
    run(s, 46 + 8.5 / 24, 46 + 17.9 / 24);
    expect(s.conserved() && s.moneySupply() == money, "the shop hours: conserved");
    int bought = 0;
    bool carriage = false;
    for (const auto& e : s.takeJournal())
    {
        // (A sale's record runs as its money does: from the buyer to the seller.)
        bought += e.kind == "to a farm" && e.from == till && e.to == "till:job:smith" ? e.quantity : 0;
        carriage = carriage || (e.kind == "carriage out" && e.from == till);
    }
    expect(bought > 0 && s.loopToday().farms.at("salt_works_1").basket > 0, "it bought the smith's goods: " + std::to_string(bought));
    expect(carriage, "and paid their carriage out");
    expect(s.cash(till) >= farm(s, "salt_works_1").reserve, "never into its float");
}

void aShortFarm()
{
    // A farm that can't pay its crew (its till emptied): the treasury's wage support makes it up, counted as the switch's
    // when it can't.
    auto s = city([](EconomySpec& e) { e.farms.areas["honey"].on = 1; });
    turn(s, 46);
    const auto till = Society::farmTill("beekeeper_1");
    expect(s.operatorTransfer(till, "treasury", "", 0, s.cash(till)).ok, "the beekeeper's till emptied");
    run(s, 46 + 17.9 / 24, 46 + 18.5 / 24);
    const auto& d = s.loopToday().farms.at("beekeeper_1");
    expect(d.wagesPaid < d.wagesOwed && d.support > 0, "short of its wages, the treasury's support paid its crew: " + std::to_string(d.support));
    expect(s.loopToday().supportWanted > 0, "and the support wanted was counted");
    // Under the treasury's cap, nothing is paid: unpaid, the switch's.
    auto poor = city([](EconomySpec& e) {
        e.treasury = 500;
        e.farms.areas["honey"].on = 1;
    });
    turn(poor, 46);
    const auto t2 = Society::farmTill("beekeeper_2");
    expect(poor.operatorTransfer(t2, "treasury", "", 0, poor.cash(t2)).ok, "another emptied");
    run(poor, 46 + 17.9 / 24, 46 + 18.5 / 24);
    expect(poor.loopToday().supportUnpaidSwitch > 0, "under the cap its support goes unpaid, counted as the switch's: " +
                                                          std::to_string(poor.loopToday().supportUnpaidSwitch));
    expect(poor.conserved(), "conserved");
}

void theCountrysBasket()
{
    auto s = city([](EconomySpec& e) { e.farms.areas["grain"].on = 1; });
    turn(s, 46);
    const auto& areas = items::farms().areas;
    double all = 0;
    for (const auto& a : areas)
        all += a.takings;
    expect(std::abs(s.countryBasketShift() - areas[areaIndex("grain")].takings / all) < 1e-9,
           "the Country's basket moved by grain's share of the food areas' takings: " + std::to_string(s.countryBasketShift()));
    const auto none = city();
    expect(none.countryBasketShift() == 0, "with none on, the basket as it is");
}

void nothingUnlessSwitched()
{
    // A world that sets no farm: nothing opened, nothing made, the loop as it was.
    auto s = city();
    const auto minted = s.state().minted;
    run(s, 46 - .5 / 24, 46 + 12. / 24);
    for (const auto& [id, a] : s.state().accounts)
        expect(id.rfind("farm:", 0) != 0 && id.rfind("crew:", 0) != 0, "no farm's account: " + id);
    expect(s.state().minted == minted && s.farmsMinted() == 0 && s.loopToday().farms.empty(), "nothing made");
}

void vegetablesGone()
{
    // Dropped from Ridgemere 2.0's economy (the user: "These are wolves after all"); the item stays in the catalogue.
    expect(items::good("vegetables") != nullptr, "the item stays in the catalogue (the old map's)");
    for (const auto& g : items::country().seasons)
        expect(std::find(g.goods.begin(), g.goods.end(), "vegetables") == g.goods.end() && g.id != "vegetables",
               "no season group of vegetables: " + g.id);
    for (const auto& b : items::businesses())
    {
        for (const auto* k : items::craftsFor(b.id))
            expect(std::none_of(k->in.begin(), k->in.end(), [](const auto& i) { return i.first == "vegetables"; }),
                   "no craft uses vegetables: " + k->id);
        const auto& sup = items::suppliesFor(b.id);
        expect(std::find(sup.begin(), sup.end(), "vegetables") == sup.end(), "nobody supplies vegetables: " + b.id);
        const auto& buys = items::buysFor(b.id);
        expect(std::find(buys.begin(), buys.end(), "vegetables") == buys.end(), "nobody buys them from players: " + b.id);
    }
    for (const auto& a : items::farms().areas)
        expect(std::find(a.goods.begin(), a.goods.end(), "vegetables") == a.goods.end(), "no farm grows them: " + a.id);
    auto s = city();
    const auto& stall = s.storeGoods("stall");
    expect(std::find(stall.begin(), stall.end(), "vegetables") == stall.end(), "the stall draws none");
}

void theStew()
{
    // The inns' stew: raw meat and bones (the user's decision, doc 71's open question 7), the meal's nourishment as it was.
    const items::Craft* stew = nullptr;
    for (const auto* k : items::craftsFor("inn"))
        if (k->id == "stew")
            stew = k;
    using In = std::vector<std::pair<std::string, int>>;
    expect(stew && stew->in == In{{"raw_meat", 1}, {"bones", 1}} && stew->out == In{{"meal", 1}}, "the stew: a raw meat and a bone make a meal");
    expect(Society::nourishment("meal") == 50, "the meal feeds as it did");
    auto s = city();
    expect(s.storeUse("inn", "bones") == Society::StoreUse::Make && s.storeUse("inn", "vegetables") == Society::StoreUse::None,
           "the inn draws bones for it, and no vegetables");
    s.takeJournal();
    run(s, 46 + 7.9 / 24, 46 + 12. / 24);
    int bones = 0;
    for (const auto& e : s.takeJournal())
        bones += e.kind == "made into stew" && e.item == "bones" ? e.quantity : 0;
    expect(bones > 0, "the inn made stew of its bones: " + std::to_string(bones));
}

void aSave()
{
    auto s = city([](EconomySpec& e) { e.farms.areas["fish"].on = 1; });
    turn(s, 46);
    const auto minted = s.state().minted;
    const auto saved = wire::society(s.state());
    const auto text = json::dump(saved);
    json::Value back;
    std::string error;
    expect(json::parse(text, back, error), "the save parses: " + error);
    auto again = city([](EconomySpec& e) { e.farms.areas["fish"].on = 1; });
    expect(again.restore(wire::readSociety(back)), "and restores");
    expect(again.account(Society::farmTill("sea_fishing_boat_1")) && again.account(Society::farmCrew("sea_fishing_boat_1")),
           "with the farms' tills and crews");
    turn(again, 47);
    expect(again.state().minted == minted && again.conserved(), "not founded again: nothing more made");
}

void aRestart()
{
    // After a restart (a restore) an area the schedule has switched on is on from the first second the world decides,
    // before its first loop hour runs: the shops' draws of its goods come from the farms' stores, never the Country's.
    const auto scheduled = [](EconomySpec& e) { e.farms.start = 0; };
    auto s = city(scheduled);
    turn(s, 46);
    expect(s.farmAreaOn(areaIndex("grain")) && s.farmsHold("flour") > 0, "grain on by the schedule, flour in the farms' stores");
    const auto text = json::dump(wire::society(s.state()));
    const auto saved = [&]() {
        json::Value back;
        std::string error;
        expect(json::parse(text, back, error), "the save parses: " + error);
        return wire::readSociety(back);
    };
    auto again = city(scheduled);
    expect(again.restore(saved()), "it restores");
    const double now = 46 + 10.5 / 24;
    again.tick(1, now, seasonOf(now), atWork(again));
    const int flour = again.farmsHold("flour");
    expect(again.farmAreaOn(areaIndex("grain")) && again.farmAreaOn(areaIndex("fruit")) && !again.farmAreaOn(areaIndex("meat")),
           "the areas on at its first second, as the schedule has them on day 46");
    expect(flour > 0 && again.countryLeft("flour") == flour, "what the shops may draw is the farms' stock: " + std::to_string(flour));
    const auto country = again.cash(Society::CountryFund);
    const auto r = again.drawFromStore("stall", "flour", 5, "restocked from the store room");
    expect(r.ok && r.message.find("from the farms") != std::string::npos && again.farmsHold("flour") == flour - 5,
           "drawn from the farms' stores: " + r.message);
    expect(again.cash(Society::CountryFund) == country, "nothing to the Country");
    // An area switched on by its own switch while the world was down: founded at that first second.
    auto honey = city([](EconomySpec& e) {
        e.farms.start = 0;
        e.farms.areas["honey"].on = 1;
    });
    expect(honey.restore(saved()), "the same save restores with honey switched on");
    const auto minted = honey.state().minted;
    expect(!honey.account(Society::farmTill("beekeeper_1")), "the save has no beekeepers");
    honey.tick(1, now, seasonOf(now), atWork(honey));
    expect(honey.account(Society::farmTill("beekeeper_1")) && honey.farmsHold("honey") > 0, "founded at its first second");
    expect(honey.state().minted > minted && honey.conserved(), "counted as minted: conserved");
}

void thePreserversStores()
{
    // Fruit and honey on in winter: the preserver sells on the drying houses' dried apples and preserves from their
    // stores before it makes its own, though the orchards hold apples enough (they keep) and honey with them.
    auto s = city(
        [](EconomySpec& e) {
            e.farms.areas["fruit"].on = 1;
            e.farms.areas["honey"].on = 1;
        },
        {person("preserver", "merchant", "keeping the preserving house", {"jars", 2.5, 2.5}, "home_jars", 40, 2000)});
    turn(s, 320);
    const auto till = s.tillOf("preserver");
    const int dried = s.farmsHold("dried_apples"), preserves = s.farmsHold("fruit_preserves");
    expect(dried > 100 && preserves > 100 && s.farmsHold("apples") > 1000 && s.farmsHold("honey") > 0,
           "the drying houses hold dried apples and preserves, the orchards apples: " + std::to_string(dried) + ", " +
               std::to_string(preserves) + ", " + std::to_string(s.farmsHold("apples")));
    s.takeJournal();
    run(s, 320 + .5 / 24, 320 + 20. / 24);               // (A winter day's shop hours.)
    int resold = 0, made = 0, apples = 0;
    for (const auto& e : s.takeJournal())
    {
        const bool preserved = e.item == "dried_apples" || e.item == "fruit_preserves";
        resold += preserved && e.to == till && e.from.rfind("farm:", 0) == 0 ? e.quantity : 0;
        made += preserved && e.to == till && e.kind == "made from the store room" ? e.quantity : 0;
        apples += e.item == "apples" && e.to == till && e.from.rfind("farm:", 0) == 0 ? e.quantity : 0;
    }
    expect(resold > 0, "the preserver sold on the drying houses' goods: " + std::to_string(resold));
    expect(s.farmsHold("dried_apples") > 0 && s.farmsHold("fruit_preserves") > 0, "which held out the day");
    expect(made == 0 && apples == 0, "so it made none of its own, nor drew the orchards' apples for them: " + std::to_string(made) +
                                         " made, " + std::to_string(apples) + " apples");
    expect(s.conserved(), "conserved");
}

void theShortagesWatched()
{
    // world_check's full level records a draw the supply cut short (Society::setShortageWatcher): the farms' stock, or a
    // season's cap, for a good a shop sells on and for a craft's input alike; never a draw the till itself refused.
    auto s = city([](EconomySpec& e) { e.farms.areas["grain"].on = 1; });
    turn(s, 46);
    std::vector<Society::Shortage> seen;
    s.setShortageWatcher([&seen](const Society::Shortage& x) { seen.push_back(x); });
    const auto stall = s.tillOf("stall"), baker = s.tillOf("baker");
    const auto shortOf = [&](const std::string& till, const std::string& item) {
        int n = 0;
        for (const auto& x : seen)
            n += x.buyer == till && x.item == item ? x.pieces : 0;
        return n;
    };
    // A till that can't pay: its draw is refused, and no shortage of the farms' flour (they hold plenty) is seen.
    s.consume(stall, "flour", held(s, stall, "flour"), "eaten");
    const auto cash = s.cash(stall);
    expect(s.shift(stall, "treasury", "", 0, cash, "emptied"), "the stall's till emptied");
    s.restockFromStore("stall", 46 + 10. / 24);
    expect(held(s, stall, "flour") == 0, "a till without money draws nothing");
    expect(shortOf(stall, "flour") == 0, "and its refused draw is not the supply's shortage: " + std::to_string(shortOf(stall, "flour")));
    expect(s.shift("treasury", stall, "", 0, cash, "returned"), "the stall's money back");
    // The farms' flour gone: the stall's resale draw and the baker's input for its bread are both recorded short.
    for (const auto& f : s.farms())
        s.consume(f.till, "flour", held(s, f.till, "flour"), "eaten");
    s.consume(baker, "flour", held(s, baker, "flour"), "eaten");
    s.consume(baker, "bread", held(s, baker, "bread"), "eaten");
    for (int hour = 11; hour < 14; ++hour)
    {
        s.restockFromStore("stall", 46 + hour / 24.);
        s.restockFromStore("baker", 46 + hour / 24.);
    }
    expect(shortOf(stall, "flour") > 0, "the stall's flour short of the farms' stock");
    expect(shortOf(baker, "flour") > 0, "the baker's flour, an input, short too");
    expect(s.conserved(), "conserved");
}

std::map<std::string, std::string> files(const std::string& extra)
{
    std::ostringstream cell;
    cell << "id: town\nname: Town\ndescription: A town.\nworld: 0 0 0\nsize: 24 24\noutdoors: true\nweather: clear\ngrid:\n";
    for (int y = 0; y < 24; ++y)
        cell << std::string(24, '.') << '\n';
    std::string manifest = "RATW_WORLD 2\ncell \"town\" \"cells/town.cell\"\nspawn \"town\" 12.5 12.5\neconomy 1000 100 50 10 12\n" + extra +
                           "resident \"smith\" \"smith\" \"merchant\" \"the smith at the forge\" \"Someone.\" \"Hello.\" 38 \"timber\" "
                           "\"female\" \"average\" \"piebald\" 4 3 0 8 1 8 17 \"-\" 30 0 1 \"town\" 3.5 5.5 \"town\" 3.5 8.5 \"town\" 3.5 11.5\n";
    return {{"world.ratw", manifest}, {"cells/town.cell", cell.str()}};
}

void theRecords()
{
    {
        World w;
        expect(w.loadWorldFiles(files(""), "test").ok, "a world without farm records loads");
        const auto& f = w.society().authored().economy.farms;
        expect(f.start == -2 && f.every == -1 && f.keeper == -1 && f.areas.empty() && f.farms.empty(), "with country_farms.json's");
    }
    {
        World w;
        const auto r = w.loadWorldFiles(files("farms start 30 every 20 order grain,meat wages 17 -\n"
                                              "farmarea \"grain\" on on yield 120 crew 150 wages 18 - factor 105\n"
                                              "farmarea \"salt\" on - yield - crew - wages - 9 factor -\n"
                                              "farm \"grain_farm_1\" yield 90 crew - wages - 12\n"), "test");
        expect(r.ok, "the farms' records load: " + r.message);
        const auto& f = w.society().authored().economy.farms;
        expect(f.start == 30 && f.every == 20 && f.order == std::vector<std::string>{"grain", "meat"} && f.keeper == 17 && f.hand == -1,
               "the farms record is read");
        const auto& g = f.areas.at("grain");
        expect(g.on == 1 && g.yieldScale == 120 && g.crewScale == 150 && g.keeper == 18 && g.hand == -1 && g.priceFactor == 105,
               "an area's");
        const auto& salt = f.areas.at("salt");
        expect(salt.on == -1 && salt.yieldScale == -1 && salt.hand == 9, "another's, its unset words country_farms.json's");
        const auto& one = f.farms.at("grain_farm_1");
        expect(one.yieldScale == 90 && one.crewScale == -1 && one.keeper == -1 && one.hand == 12, "a farm's");
        const auto& now = farm(w.society(), "grain_farm_1");
        expect(std::abs(now.yieldScale - 1.08) < 1e-9 && now.hands == 6 && now.keeper == 18 && now.hand == 12,
               "applied: 120% of 90%, 4 hands at 150%, the area's keeper, the farm's hand");
        expect(std::abs(w.society().farmPriceFactor("flour") - 1.05) < 1e-9 && w.society().farmPriceFactor("salt") == 0,
               "the area's price a year (switched on, it is on at once); salt at the season's");
    }
    for (const auto* bad : {"farms start 30 every 0 order - wages - -\n", "farms start -2 every 28 order - wages - -\n",
                            "farms start 30 every 28 order grain,grain wages - -\n", "farms start 30 every 28 wages - -\n",
                            "farms start - every - order - wages - -\nfarms start - every - order - wages - -\n",
                            "farmarea \"grain\" on maybe yield - crew - wages - - factor -\n",
                            "farmarea \"grain\" on on yield 1001 crew - wages - - factor -\n",
                            "farmarea \"grain\" on on yield - crew - wages - - factor 5\n",
                            "farmarea \"grain\" on on yield - crew - wages - - factor -\nfarmarea \"grain\" on off yield - crew - wages - - factor -\n",
                            "farm \"grain_farm_1\" yield - crew - wages 1001 -\n", "farm \"grain_farm_1\" yield - crew -\n"})
    {
        World w;
        expect(!w.loadWorldFiles(files(bad), "test").ok, std::string("refused: ") + bad);
    }
    const auto build = files("farms start 30 every 20 order - wages - -\nfarmarea \"grain\" on on yield - crew - wages - - factor -\n"
                             "farm \"grain_farm_1\" yield 90 crew - wages - -\n").at("world.ratw");
    const auto stripped = game::Game::withoutPeople(build);
    expect(stripped.find("farms ") == std::string::npos && stripped.find("farmarea") == std::string::npos &&
               stripped.find("farm \"") == std::string::npos, "a streamed build's farm records are stripped");
}

void appliedLive()
{
    // economy.sync: an area switched on while the world runs is founded at once; a yield scale changed grows so the next
    // day.
    auto s = city();
    turn(s, 46);
    auto e = s.authored().economy;
    e.farms.areas["honey"].on = 1;
    s.adoptEconomy(e);
    expect(s.farmAreaOn(areaIndex("honey")) && s.account(Society::farmTill("beekeeper_1")), "switched on live: founded at once");
    expect(s.conserved(), "conserved");
    e.farms.areas["honey"].yieldScale = 300;
    s.adoptEconomy(e);
    expect(std::abs(farm(s, "beekeeper_1").yieldScale - 3) < 1e-9, "a yield scale applied at once");
}
} // namespace

int main()
{
    try
    {
        catalogue();
        theDefaults();
        yieldsBySeason();
        crewsAndWages();
        thePhasedSwitch();
        conservationWithMinting();
        drawsFromTheFarms();
        anEvenSplit();
        theCountryNoLongerSends();
        spoiling();
        theFarmsBasket();
        aShortFarm();
        theCountrysBasket();
        nothingUnlessSwitched();
        vegetablesGone();
        theStew();
        aSave();
        aRestart();
        thePreserversStores();
        theShortagesWatched();
        theRecords();
        appliedLive();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Farms tests passed: " << checks << " checks.\n";
    return 0;
}
