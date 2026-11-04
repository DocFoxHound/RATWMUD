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
// gone from Ridgemere 2.0's economy and the inns' stew of meat and bones. The works (Docs/Design/72-pulling-the-country-
// into-the-world.md, Phases 0 and 2): the woods' and the peat's, paying their crews and their rent to their House's estate
// (the Country's purse, booked by House), buying no basket, the Country supplying their goods again when they are off;
// and the households' fuel, firewood or peat, whichever their shops hold. Phase 3: the stone's works (the granite quarry
// and the two clay pits, House Greatly's; a pit with one hand and no keeper), and the Country's table less the works'
// crews. Phase 3's follow-up (open question 2): the food farms' land rent to their House, from their basket's share, and
// the Country's posts paid with it.
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
#include <set>
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
    expect(f.areas.size() == 8, "six food areas, the fuel's works and the stone's (doc 72): " + std::to_string(f.areas.size()));
    int farms = 0, keepers = 0, hands = 0;
    std::map<std::string, int> kinds;
    for (const auto& a : f.areas)
        for (const auto& x : a.works ? std::vector<items::FarmDef>{} : a.farms)
        {
            ++farms;
            keepers += x.keepers;
            hands += x.hands;
            ++kinds[x.kind];
            expect(x.reserve > 0 && x.purse > 0 && !x.yield.empty(), x.id + " has a float, a crew's purse and yields");
        }
    expect(farms == 31 && keepers == 31 && hands == 76,
           "31 farms, 31 keepers and 76 hands (a fibre hand at each grain farm, a vine hand fewer at each vineyard, doc 72): " +
               std::to_string(farms) + ", " + std::to_string(keepers) + ", " + std::to_string(hands));
    expect(kinds["vineyard"] == 2 && kinds["salt works"] == 2 && kinds["grain farm"] == 2,
           "two vineyards and two salt works (the user's decision), two grain farms");
    expect(f.keeper == 16 && f.hand == 10 && f.floatDays == 2 && f.openingStockDays == 7, "a keeper 16p and a hand 10p, two days' float");
    expect(f.start == -1 && f.every == 28 && f.order.size() == 8 && f.order.front() == "grain" && f.order[6] == "fuel" &&
               f.order.back() == "stone",
           "no switch until a world sets one; an area every 28 days, grain first, the fuel's works, then the stone's last");
    expect(f.estatePeople == 24 && f.otherWorkers == 68, "the Country's own people at its table: 24 of the estates, 68 workers");
    expect(!f.basket.empty() && !f.countryBasket.empty() && !f.switchTrades.empty(), "the farms' basket, the Country's after, the losers");
    // Its re-weighted basket buys nothing the farms' does but the building goods (doc 72, after country3b: the land rent
    // moved ~340p a day from the farms' baskets to the Country's, which buys back the farms' repairs, at 0.53 of the
    // farms' shares: the builders' ~71p a day the rent took).
    const std::set<std::string> building = {"cut_stone", "mortar", "bricks", "lime", "roof_tiles", "rope", "whitewash", "plaster"};
    int repairs = 0;
    for (const auto& [item, share] : f.countryBasket)
    {
        const auto farms = std::find_if(f.basket.begin(), f.basket.end(), [&](const auto& b) { return b.first == item; });
        expect(farms == f.basket.end() || building.count(item), "the Country's re-weighted basket buys nothing the farms' does: " + item);
        if (farms != f.basket.end())
        {
            expect(std::abs(share - .53 * farms->second) <= .5, "the farms' repairs at 0.53 of the farms' share: " + item);
            repairs += share;
        }
        expect(Society::nourishment(item) == 0 && item != "aqua_vitae", "and no food or drink: " + item);
    }
    // (Phase 4: cut stone 80 of the farms' shares and 42 of the Country's, for the quarry's summer: doc 72.)
    expect(repairs == 181, "the building goods 181 of the Country's shares: " + std::to_string(repairs));
    // The yields: the season means of the blend (a grain farm's flour in summer: the farms' people's confirming run's
    // yields, the flour cut to what was taken and the oats raised, doc 72, Phase 1; country1's patch half way, Phase 2;
    // country3's half way, Phase 3's follow-up; country3b's and country3c's half way; country4's, Phase 5; country5b's, its carry-overs).
    const auto& flour = f.areas[areaIndex("grain")].farms.front().yield.at("flour");
    double summer = 0;
    for (int d = 92; d < 184; ++d)
        summer += Society::seasonalValue(flour, d + .5);
    expect(std::abs(summer / 92 - 55.11) < .2, "a grain farm's flour in summer, the blend's mean: " + std::to_string(summer / 92));
    // The Country's herbs and raw fibre on the farms that already exist (doc 72, Phase 1): the eel marsh's herbs (the fish
    // area's goods; resin stays the Country's, with the woods), the grain farms' hemp and flax with a hand more each.
    const auto& fish = f.areas[areaIndex("fish")];
    const auto& grain = f.areas[areaIndex("grain")];
    for (const auto* good : {"herbs", "wormwood", "willow_bark", "thyme", "garlic", "comfrey"})
    {
        expect(std::find(fish.goods.begin(), fish.goods.end(), good) != fish.goods.end(), std::string("the fish area's: ") + good);
        for (const auto& x : fish.farms)
            expect(x.yield.count(good) == (x.id == "eel_and_fowl_marsh_1"), x.id + (x.id == "eel_and_fowl_marsh_1" ? " grows " : " doesn't grow ") + good);
    }
    expect(std::find(f.areas[areaIndex("fuel")].goods.begin(), f.areas[areaIndex("fuel")].goods.end(), "resin") !=
               f.areas[areaIndex("fuel")].goods.end(),
           "resin the woods' (the fuel area's, doc 72, Phase 2)");
    for (const auto* good : {"hemp", "flax"})
        expect(std::find(grain.goods.begin(), grain.goods.end(), good) != grain.goods.end(), std::string("the grain area's: ") + good);
    for (const auto& x : grain.farms)
    {
        expect(x.yield.count("hemp") && x.yield.count("flax") && x.keepers == 1 && x.hands == 5,
               x.id + " grows hemp and flax, with a hand more (1 + 5)");
        expect(x.reserve == 2 * (16 + 5 * 10), x.id + "'s float two days of its wages: " + std::to_string(x.reserve));
    }
    expect(f.areas[areaIndex("salt")].yieldScale == 88, "salt's yield scale 88 (doc 72, Phase 1)");
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
    expect(one.hands == 8 && two.hands == 4, "5 hands at 150%: 8; at 150% of 50%: 4 (rounded half up): " + std::to_string(one.hands) + ", " +
                                                 std::to_string(two.hands));
    expect(one.wages() == 18 + 8 * 12, "its wages a day: " + std::to_string(one.wages()));
    expect(one.reserve == 2 * one.wages(), "its float: two days of its wages");
    expect(one.purse == std::llround(double(one.def->purse) * 9 / 6), "its crew's purse by its size");
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
    turn(s, 10 + 7 * 28);
    for (std::size_t a = 0; a < items::farms().areas.size(); ++a)
        expect(s.farmAreaOn(a), "every area on by the eighth month (the stone's works last)");
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

void herbsAndFibre()
{
    // Doc 72, Phase 1: with the fish area on, the apothecary's comfrey comes from the eel marsh's store and the Country
    // sends none; with grain on, the ropewalk's hemp from the grain farms'. Off, the Country supplies them again.
    const std::vector<ResidentSpec> makers = {
        person("apothecary", "merchant", "apothecary at the Mortar", {"mortar", 2.5, 2.5}, "home_apothecary", 40, 400),
        person("ropewalk", "merchant", "works the ropewalk by the wharf", {"ropewalk", 2.5, 2.5}, "home_ropewalk", 40, 400)};
    auto s = city([](EconomySpec& e) { e.farms.areas["fish"].on = 1, e.farms.areas["grain"].on = 1; }, makers);
    turn(s, 46);
    const auto marsh = Society::farmTill("eel_and_fowl_marsh_1");
    expect(held(s, marsh, "comfrey") > 0 && held(s, marsh, "willow_bark") > 0, "the marsh's store holds its herbs");
    expect(s.farmAreaOf("comfrey") == int(areaIndex("fish")) && s.farmAreaOf("hemp") == int(areaIndex("grain")) &&
               s.farmAreaOf("resin") < 0,
           "the herbs the fish area's, hemp the grain area's, resin nobody's");
    const auto country = s.cash(Society::CountryFund), till = s.cash(marsh);
    const int comfrey = held(s, marsh, "comfrey");
    const auto r = s.drawFromStore("apothecary", "comfrey", 2, "restocked from the store room");
    expect(r.ok && r.message.find("from the farms") != std::string::npos, "comfrey from the marsh: " + r.message);
    expect(held(s, marsh, "comfrey") == comfrey - 2 && s.cash(marsh) > till && s.cash(Society::CountryFund) == country,
           "out of its store, paid to its till, nothing to the Country");
    const auto t1 = Society::farmTill("grain_farm_1"), t2 = Society::farmTill("grain_farm_2");
    const int hemp = held(s, t1, "hemp") + held(s, t2, "hemp");
    expect(hemp > 100, "the grain farms' stores hold hemp: " + std::to_string(hemp));
    const auto rope = s.drawFromStore("ropewalk", "hemp", 10, "restocked from the store room");
    expect(rope.ok && rope.message.find("from the farms") != std::string::npos && held(s, t1, "hemp") + held(s, t2, "hemp") == hemp - 10,
           "hemp from the grain farms: " + rope.message);
    expect(s.conserved(), "conserved");
    auto e = s.authored().economy;
    e.farms.areas["fish"].on = 0;
    s.adoptEconomy(e);
    const auto back = s.drawFromStore("apothecary", "comfrey", 2, "restocked from the store room");
    expect(back.ok && back.message.find("from the farms") == std::string::npos, "fish off: the Country's comfrey again: " + back.message);
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
    const auto& today = s.loopToday().farms.at(cattle.def->id);
    expect(today.spoiled > 0 || today.goods.at("milk").used > 0, "the rest spoiled, or made into cheese and butter, and counted");
}

void theDairy()
{
    // The user's decision after country3b (doc 72): the dairies' spare milk, what is past keeping (most the morning after a
    // Restday), made into cheese (crafts.json's dairy craft: 4 milk a cheese) and butter (2 milk), cheese keeping half a
    // year; the stall and the provisioners resell them, drawn from the farms; the Country sends none while meat is on.
    const auto* cheese = items::good("cheese");
    const auto* butter = items::good("butter");
    expect(cheese && cheese->keeps >= 180 && butter && butter->keeps == 14, "cheese keeps half a year, butter a fortnight");
    const auto& meat = items::farms().areas[areaIndex("meat")];
    expect(std::find(meat.goods.begin(), meat.goods.end(), "cheese") != meat.goods.end() &&
               std::find(meat.goods.begin(), meat.goods.end(), "butter") != meat.goods.end(),
           "butter and cheese the meat area's goods (the Country sends none while it is on)");
    int dairies = 0;
    for (const auto& x : meat.farms)
    {
        if (x.makes.empty())
            continue;
        ++dairies;
        expect(x.kind == "cattle and dairy farm" && x.makes.size() == 2 && x.makes[0].good == "cheese" && x.makes[0].from == "milk" &&
                   x.makes[0].in == 4 && x.makes[0].share == 0 && x.makes[0].store == 40 && x.makes[1].good == "butter" &&
                   x.makes[1].in == 2 && x.makes[1].store == 40,
               x.id + ": none of its spare milk to cheese (4 a piece, 40 in store at most), all of it to butter (2 a piece, 40)");
        // Doc 72, Phase 4's follow-up: milk of its own for each, all year; since country5b's carry-overs (cheese scarce,
        // the user's) 7 a day for ~1.75 cheese, and the 13 the cheese no longer takes to the butter's 8: 21 for 10.5; after
        // country6a (its price only 2-2.2 times the season's) 5 for 1.25, the 2 to the butter: 23 for 11.5.
        expect(x.makes[0].aDay == std::array<double, 4>{5, 5, 5, 5} && x.makes[1].aDay == std::array<double, 4>{23, 23, 23, 23},
               x.id + ": the cheese room's own milk, 5 a day, and the butter's 23");
    }
    expect(dairies == 3, "the three dairies make them, no other farm: " + std::to_string(dairies));

    auto s = city([](EconomySpec& e) { e.farms.areas["meat"].on = 1; });
    turn(s, 46);                                    // (Founded: a day's milk in store, what keeps.)
    // The founding morning's made sold to the stall first, so the next morning's making shows whole.
    for (const auto* g : {"cheese", "butter"})
        if (const int n = s.farmsHold(g); n > 0)
            expect(s.drawFromStore("stall", g, n, "restocked from the store room").ok, std::string("the founding's drawn: ") + g);
    const auto& cattle = farm(s, "cattle_and_dairy_farm_1");
    const auto& milkYield = cattle.def->yield.at("milk");
    const int keep = int(std::ceil((milkYield[0] + milkYield[1] + milkYield[2] + milkYield[3]) / 4 - 1e-9));
    // A day with no sales: the next morning a day's milk is past keeping, and is made into cheese and butter.
    const int milkBefore = held(s, cattle.till, "milk"), cheeseBefore = held(s, cattle.till, "cheese"),
              butterBefore = held(s, cattle.till, "butter");
    const auto money = s.moneySupply();
    s.takeJournal();
    turn(s, 47);
    const auto& day = s.loopToday().farms.at(cattle.def->id);
    const auto good = [&](const char* g) {
        const auto x = day.goods.find(g);
        return x == day.goods.end() ? Society::LoopDay::Farm::Good{} : x->second;
    };
    // What it made of its own milk (journalled apart), and of the spare.
    std::map<std::string, int> own;
    for (const auto& e : s.takeJournal())
        if (e.to == cattle.till && e.kind == "made at " + cattle.def->id + " of its own milk")
            own[e.item] += e.quantity;
    const int cheeseMade = held(s, cattle.till, "cheese") - cheeseBefore, butterMade = held(s, cattle.till, "butter") - butterBefore;
    expect(own["cheese"] >= 1 && own["cheese"] <= 2 && own["butter"] >= 11 && own["butter"] <= 12,
           "of its own milk 1-2 cheese and 11-12 butter: " + std::to_string(own["cheese"]) + " and " + std::to_string(own["butter"]));
    const int cheeseSpare = cheeseMade - own["cheese"], butterSpare = butterMade - own["butter"];
    expect(cheeseSpare == 0 && butterSpare > 0, "none of the spare milk to cheese, butter of it: " + std::to_string(cheeseSpare) + " and " +
                                                    std::to_string(butterSpare));
    expect(good("cheese").grown == cheeseMade && good("butter").grown == butterMade, "counted as the farm's made (its grown)");
    expect(good("milk").used == 4 * cheeseMade + 2 * butterMade, "4 milk a cheese and 2 a butter: " + std::to_string(good("milk").used));
    // Its own milk counted grown and used, never in the till: the shops' milk is the yield's alone.
    double yieldToday = Society::seasonalValue(milkYield, 47 + .5) * cattle.yieldScale * cattle.working();
    const int ownMilk = 4 * own["cheese"] + 2 * own["butter"];
    expect(good("milk").grown - ownMilk >= int(std::floor(yieldToday)) - 1 && good("milk").grown - ownMilk <= int(std::ceil(yieldToday)) + 1,
           "the milk grown: the yield's and its own: " + std::to_string(good("milk").grown));
    expect(held(s, cattle.till, "milk") == keep && good("milk").spoiled <= 1, "a day's milk kept, a piece spoiled at most: " +
                                                                                  std::to_string(good("milk").spoiled));
    // Conserved: no coin made or lost, and every piece of milk is accounted for (kept, made into the two, spoiled or eaten at the
    // Country's table; nothing sold in the night).
    expect(s.conserved() && s.moneySupply() == money, "no money made or lost (the making moves goods, not coins)");
    expect(milkBefore + good("milk").grown == held(s, cattle.till, "milk") + good("milk").used + good("milk").spoiled +
                                                  good("milk").eaten + good("milk").sold,
           "the milk accounted for: " + std::to_string(milkBefore) + " + " + std::to_string(good("milk").grown));
    // The shops draw them from the farms, the cost less the toll to the dairies; the Country sends none.
    const auto country = s.cash(Society::CountryFund);
    const auto before = s.moneySupply();
    const int held3 = held(s, Society::farmTill("cattle_and_dairy_farm_1"), "cheese") + held(s, Society::farmTill("cattle_and_dairy_farm_2"), "cheese") +
                      held(s, Society::farmTill("cattle_and_dairy_farm_3"), "cheese");
    const int shelf = held(s, s.tillOf("stall"), "cheese");
    const auto r = s.drawFromStore("stall", "cheese", 3, "restocked from the store room");   // (3: the dairies' ~1.25 a day each)
    expect(r.ok && r.message.find("from the farms") != std::string::npos, "the stall's cheese from the farms: " + r.message);
    expect(held(s, s.tillOf("stall"), "cheese") == shelf + 3, "on its shelves");
    const int held3After = held(s, Society::farmTill("cattle_and_dairy_farm_1"), "cheese") + held(s, Society::farmTill("cattle_and_dairy_farm_2"), "cheese") +
                           held(s, Society::farmTill("cattle_and_dairy_farm_3"), "cheese");
    expect(held3 - held3After == 3, "out of the dairies' stores");
    const auto b = s.drawFromStore("stall", "butter", 3, "restocked from the store room");
    expect(b.ok && b.message.find("from the farms") != std::string::npos, "and its butter: " + b.message);
    expect(s.cash(Society::CountryFund) == country && s.conserved() && s.moneySupply() == before, "nothing to the Country; conserved");
    // Many days with no sales: the store rooms fill to their most (cheese 40, butter 40) and no further; then the spare
    // spoils as before, and its own milk isn't milked. The cheese keeps (made goods don't spoil at the farm: only a yield
    // past its keeping does).
    for (int d = 48; d < 48 + 70; ++d)
        turn(s, d);
    expect(held(s, cattle.till, "cheese") == 40 && held(s, cattle.till, "butter") == 40, "the store rooms full and no fuller: " +
                                                                                              std::to_string(held(s, cattle.till, "cheese")) + ", " +
                                                                                              std::to_string(held(s, cattle.till, "butter")));
    const auto& full = s.loopToday().farms.at(cattle.def->id).goods.at("milk");
    expect(full.spoiled > 0 && full.used == 0, "full, the spare milk spoils again and none is made");
    // Meat off: the Country sends them again.
    auto e = s.authored().economy;
    e.farms.areas["meat"].on = 0;
    s.adoptEconomy(e);
    expect(s.farmAreaOf("cheese") < 0, "meat off: cheese the Country's again");
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
    // The country workshops first (doc 72, after country5: Larkfield's smithy, short of its one wage, sold the farms 13p a
    // day of the ~85p of iron goods they bought): its nails and cookpots, none of the city smith's while it holds them;
    // what it doesn't make (fittings, locks, chain) still the city smith's.
    auto both = city([](EconomySpec& e) { e.farms.areas["salt"].on = 1; },
                     {person("lark", "merchant", "village smith at Larkfield Smithy", {"cn_larkfield_smithy", 2.5, 2.5},
                             "cn_larkfield_smithy_flat", 40, 100)});
    turn(both, 46);
    expect(both.operatorTransfer("treasury", till, "", 0, 3000).ok, "the salt works given 3,000p again");
    const auto lark = both.tillOf("lark");
    expect(lark.rfind("till:", 0) == 0, "Larkfield's smithy its own till");
    for (const char* good : {"nails", "fittings", "lock", "chain", "cookpot"})
        expect(both.create("till:job:smith", good, 99, "test stock"), std::string("the smith holds ") + good);
    for (const char* good : {"nails", "cookpot"})
        expect(both.create(lark, good, 99, "test stock"), std::string("Larkfield's smithy holds ") + good);
    const auto money2 = both.moneySupply();
    both.takeJournal();
    run(both, 46 + 8.5 / 24, 46 + 17.9 / 24);
    expect(both.conserved() && both.moneySupply() == money2, "with the workshop: conserved");
    std::map<std::string, std::map<std::string, int>> from;
    for (const auto& e : both.takeJournal())
        if (e.kind == "to a farm" && e.from == till)
            from[e.to][e.item] += e.quantity;
    // (Its cookpots all the farm's, while it still holds some; its nails until it sold out, the 99 and what it made, then
    // the city smith's.)
    expect(from[lark]["cookpot"] > 0 && Society::stock(*both.account(lark), "cookpot") > 0 &&
               from["till:job:smith"]["cookpot"] == 0 && from[lark]["nails"] >= 99,
           "the farm's nails and cookpots Larkfield's: " + std::to_string(from[lark]["nails"]) + " and " +
               std::to_string(from[lark]["cookpot"]) + ", the city smith's " + std::to_string(from["till:job:smith"]["nails"]) +
               " and " + std::to_string(from["till:job:smith"]["cookpot"]) + "; Larkfield holds " +
               std::to_string(Society::stock(*both.account(lark), "nails")) + " and " +
               std::to_string(Society::stock(*both.account(lark), "cookpot")));
    expect(from["till:job:smith"]["fittings"] + from["till:job:smith"]["lock"] + from["till:job:smith"]["chain"] > 0,
           "what the workshop doesn't make, the city smith's");
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
        all += a.works ? 0 : a.takings;             // (The works' areas not: doc 72.)
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

// A general store for the works' goods (it resells firewood and peat), with the city.
std::vector<ResidentSpec> aStore()
{
    return {person("general", "merchant", "shopkeeper at the Corner Store", {"store", 2.5, 2.5}, "home_general", 40, 400)};
}

void theWorks()
{
    // Doc 72, Phases 0 and 2 (the user, 2026-11-03: "each works pays its own crew and sends its surplus to the noble House
    // whose land it is on"; House Timberland holds the woods): the fuel area's five works, a till each like a farm's.
    const auto& f = items::farms();
    const auto& fuel = f.areas[areaIndex("fuel")];
    expect(fuel.works && fuel.house == "Timberland", "the fuel area: works, on House Timberland's land");
    expect(fuel.goods == std::vector<std::string>({"timber", "firewood", "oak_bark", "resin", "charcoal", "peat"}),
           "its goods: the woods' and peat");
    int keepers = 0, hands = 0;
    std::int64_t wages = 0;
    std::set<std::string> ids;
    for (const auto& x : fuel.farms)
    {
        ids.insert(x.id);
        keepers += x.keepers, hands += x.hands;
        wages += 16 * x.keepers + 10 * x.hands;
        expect(x.table.empty() && x.basketADay == 0 && x.reserve == 2 * (16 * x.keepers + 10 * x.hands),
               x.id + ": no Country's table, no basket, a float of two days' wages");
    }
    expect(ids == std::set<std::string>({"logging_rainwood", "logging_oakhaven", "coppice_rainwood", "coppice_greenholt", "peat_fens"}) &&
               keepers == 5 && hands == 10 && wages == 180,
           "five works, 5 keepers and 10 hands, 180p a day");
    for (const auto& a : f.areas)
        expect(a.id == "fuel" || a.id == "stone" || (!a.works && !a.house.empty()), a.id + ": a food area, its land rent's House");
    // On: founded as farms are; the shops' fuel from the works' stores, nothing to the Country.
    auto s = city([](EconomySpec& e) { e.farms.areas["fuel"].on = 1; }, aStore());
    turn(s, 46);
    for (const auto& w : fuel.farms)
    {
        const auto& now = farm(s, w.id);
        expect(s.account(now.till) && s.account(now.crew) && now.house == "Timberland" && now.rents(), w.id + " founded, its rent Timberland's");
    }
    expect(s.farmAreaOf("firewood") == int(areaIndex("fuel")) && s.farmAreaOf("peat") == int(areaIndex("fuel")) &&
               s.farmAreaOf("charcoal") == int(areaIndex("fuel")),
           "firewood, peat and charcoal the fuel area's");
    const auto peat = Society::farmTill("peat_fens");
    const int peatHeld = held(s, peat, "peat");
    expect(peatHeld > 300 && s.farmsHold("peat") == peatHeld, "the peat cutting's store holds a week of peat: " + std::to_string(peatHeld));
    const auto country = s.cash(Society::CountryFund), money = s.moneySupply();
    const auto wood = s.drawFromStore("general", "firewood", 30, "restocked from the store room");
    const auto turf = s.drawFromStore("general", "peat", 20, "restocked from the store room");
    expect(wood.ok && turf.ok && wood.message.find("from the farms") != std::string::npos, "firewood and peat from the works: " + wood.message);
    expect(held(s, peat, "peat") == peatHeld - 20 && s.cash(Society::CountryFund) == country, "the peat out of its store, nothing to the Country");
    expect(s.conserved() && s.moneySupply() == money, "conserved");
    // At 18:00 each works pays its crew, then what its till holds over its float goes to Timberland's estate (its own
    // purse: doc 72, Phase 6), booked by House.
    const auto coppice = Society::farmTill("coppice_rainwood");
    expect(s.operatorTransfer("treasury", coppice, "", 0, 500).ok, "the coppice given 500p (moved, not made)");
    const auto before = s.moneySupply();
    s.takeJournal();
    run(s, 46 + 17.9 / 24, 46 + 18.5 / 24);
    const auto& d = s.loopToday();
    std::int64_t rents = 0, booked = 0;
    for (const auto& w : fuel.farms)
    {
        const auto& day = d.farms.at(w.id);
        const auto& now = farm(s, w.id);
        expect(day.wagesOwed == now.wages() && day.wagesPaid == day.wagesOwed, w.id + " paid its crew: " + std::to_string(day.wagesPaid));
        expect(s.cash(now.till) <= now.reserve, w.id + ": its till at its float or under after its rent: " + std::to_string(s.cash(now.till)));
        rents += day.rent;
    }
    expect(d.farms.at("coppice_rainwood").rent >= 500 - 36 && s.cash(coppice) == farm(s, "coppice_rainwood").reserve,
           "the coppice's rent: what was over its float and wages: " + std::to_string(d.farms.at("coppice_rainwood").rent));
    for (const auto& e : s.takeJournal())
        if (e.kind == "an estate's rent: House Timberland")
        {
            expect(e.to == "estate:timberland" && e.from.rfind("farm:", 0) == 0, "a works' rent to the estates' purse: " + e.from);
            booked += e.coins;
        }
    expect(d.rents.count("Timberland") && d.rents.at("Timberland") == rents && booked == rents, "booked by House: " + std::to_string(rents));
    expect(s.conserved() && s.moneySupply() == before, "conserved");
    // No basket: a works with money over spends none of it on the city's goods.
    for (const char* good : {"nails", "fittings", "lock"})
        expect(s.create("till:job:smith", good, 99, "test stock"), std::string("the smith holds ") + good);
    expect(s.operatorTransfer("treasury", coppice, "", 0, 800).ok, "the coppice given 800p again");
    s.takeJournal();
    run(s, 47 + 8.5 / 24, 47 + 17.9 / 24);
    for (const auto& e : s.takeJournal())
        expect(!(e.kind == "to a farm" && e.from == coppice), "a works buys no basket");
    expect(s.loopToday().farms.at("coppice_rainwood").basket == 0, "none counted");
    // Short of its wages, a works pays no rent (it keeps what it has; the treasury's support as a farm's).
    expect(s.operatorTransfer(peat, "treasury", "", 0, s.cash(peat)).ok, "the peat cutting's till emptied");
    run(s, 47 + 17.9 / 24, 47 + 18.5 / 24);
    expect(s.loopToday().farms.at("peat_fens").rent == 0 && s.loopToday().farms.at("peat_fens").support > 0,
           "short of its wages: no rent, the treasury's support");
    // Off: the Country supplies its goods again.
    auto e = s.authored().economy;
    e.farms.areas["fuel"].on = 0;
    s.adoptEconomy(e);
    const auto c2 = s.cash(Society::CountryFund);
    const auto back = s.drawFromStore("general", "peat", 5, "restocked from the store room");
    expect(back.ok && back.message.find("from the farms") == std::string::npos && s.cash(Society::CountryFund) > c2,
           "fuel off: the Country's peat again: " + back.message);
    expect(s.farmAreaOf("firewood") < 0, "and its firewood");
    // The Country's own sales (its report's line) count what was delivered: a draw paid back counts nothing.
    const auto sales = s.loopToday().countrySales;
    s.failNextStoreCreate();
    expect(!s.drawFromStore("general", "peat", 5, "restocked from the store room").ok && s.loopToday().countrySales == sales,
           "a draw paid back: none of the Country's sales");
    expect(s.drawFromStore("general", "peat", 5, "restocked from the store room").ok && s.loopToday().countrySales > sales,
           "a draw delivered: its sales");
    expect(s.conserved(), "conserved");
}

void theStoneWorks()
{
    // Doc 72, Phase 3 (the user, 2026-11-03: each works pays its own crew, 16p a keeper and 10p a hand, and sends what is
    // over to the House whose land it is on; stone and clay are House Greatly's; the lime quarry waits, the Country's): the
    // stone area's granite quarry and two clay pits, the pits one hand each and no keeper.
    const auto& f = items::farms();
    const auto& stone = f.areas[areaIndex("stone")];
    expect(stone.works && stone.house == "Greatly", "the stone area: works, on House Greatly's land");
    expect(stone.goods == std::vector<std::string>({"stone", "clay", "sand"}), "its goods: stone, clay and sand (limestone not)");
    std::map<std::string, std::pair<int, int>> crews;
    for (const auto& x : stone.farms)
    {
        crews[x.id] = {x.keepers, x.hands};
        expect(x.table.empty() && x.basketADay == 0 && x.reserve == 2 * (16 * x.keepers + 10 * x.hands),
               x.id + ": no Country's table, no basket, a float of two days' wages");
        for (const auto& [good, y] : x.yield)
            expect(y[0] > 0 && y[1] > 0 && y[2] > 0 && y[3] > 0, x.id + "'s " + good + ": grown in every season");
    }
    // The quarry 1 + 1 since country3 (its year 3 took 30-36p a day, short of a 1 + 2 crew's 36p on 19% of its days).
    expect(crews == std::map<std::string, std::pair<int, int>>({{"quarry_granite", {1, 1}}, {"clay_mirebridge", {0, 1}}, {"clay_fens", {0, 1}}}),
           "the quarry 1 + 1, each pit one hand and no keeper: 1 keeper and 3 hands, 46p a day");
    // Sized on country2's years 2-3 (the Country's sales to the shops) + 5%, flat (stone 26.92 a day, clay 58.45, sand
    // 25.71); then country3's patch half way (the three piling: grown 5-12% over sold), and country3b's half way for the
    // pits. The quarry's stone to the mason's 24.9 a day before the land rent (country3's year 3) + 5%, in country3's
    // patch's seasons: the Country's basket buys the builders' goods the rent took from the farms' (doc 72, after
    // country3b); then (Phase 4, doc 72) country3c's patch half way, times 1.1 for the mason's cut stone, its shares
    // raised (80 of the farms' basket, 42 of the Country's), so its summer pays its 1 + 1; the pits' clay its whole step in
    // spring (supply-bound there). After country5 (doc 72): its patch half way (the stone its whole step in spring), the
    // pits' clay to what was used (the patch's less its 5%: the potteries' 8.5 a day more had piled 9,041). After country5b
    // (doc 72): its patch half way (the stone its whole step in spring and summer), the pits' clay 7 a day more each for
    // the kiln hands. After country6a (doc 72): its patch half way, the pits' clay to what the potteries and the
    // brickworks drew (+5%: 8,367 had piled), half way (the early years drew more).
    const auto& quarry = stone.farms.front();
    expect(quarry.id == "quarry_granite" && quarry.yield.size() == 1 && std::abs(quarry.yield.at("stone")[1] - 26.09) < .01,
           "the quarry's stone, 26.09 a day at summer's middle (country6a's patch half way)");
    double stoneMean = 0;
    for (const double v : quarry.yield.at("stone"))
        stoneMean += v / 4;
    expect(std::abs(stoneMean - 29.24) < .05, "29.24 a day over the year: " + std::to_string(stoneMean));
    for (std::size_t i = 1; i < 3; ++i)
        expect(std::abs(stone.farms[i].yield.at("clay")[1] - 34.19) < .01 && std::abs(stone.farms[i].yield.at("sand")[1] - 9.02) < .01,
               stone.farms[i].id + ": half the clay and half the sand (the game's pit gives both; after country6a: the clay half way to what was drawn, the sand half way)");
    // On: founded; the mason's stone and sand from the works' stores, nothing to the Country; limestone still the Country's.
    auto s = city([](EconomySpec& e) { e.farms.areas["stone"].on = 1; },
                  {person("mason", "merchant", "stonemason in Chisel Row", {"masonry", 2.5, 2.5}, "home_mason", 40, 400)});
    turn(s, 46);
    for (const auto& w : stone.farms)
    {
        const auto& now = farm(s, w.id);
        expect(s.account(now.till) && s.account(now.crew) && now.house == "Greatly" && now.rents(), w.id + " founded, its rent Greatly's");
    }
    for (const char* good : {"stone", "clay", "sand"})
        expect(s.farmAreaOf(good) == int(areaIndex("stone")), std::string(good) + ": the stone area's");
    expect(s.farmAreaOf("limestone") < 0 && s.farmAreaOf("lime") < 0, "limestone and lime still the Country's (the lime quarry waits)");
    const auto quarryTill = Society::farmTill("quarry_granite");
    const int blocks = held(s, quarryTill, "stone");
    expect(blocks > 150 && s.farmsHold("stone") == blocks, "the quarry's store holds a week of stone: " + std::to_string(blocks));
    const auto country = s.cash(Society::CountryFund), money = s.moneySupply();
    const auto st = s.drawFromStore("mason", "stone", 10, "restocked from the store room");
    const auto sd = s.drawFromStore("mason", "sand", 6, "restocked from the store room");
    expect(st.ok && sd.ok && st.message.find("from the farms") != std::string::npos, "the mason's stone and sand from the works: " + st.message);
    expect(held(s, quarryTill, "stone") == blocks - 10 && s.cash(Society::CountryFund) == country, "the stone out of the quarry, nothing to the Country");
    const auto lime = s.drawFromStore("mason", "limestone", 4, "restocked from the store room");
    expect(lime.ok && lime.message.find("from the farms") == std::string::npos && s.cash(Society::CountryFund) > country,
           "the mason's limestone from the Country: " + lime.message);
    expect(s.conserved() && s.moneySupply() == money, "conserved");
    // At 18:00 a pit with no keeper pays its one hand (its crew's household, until a real one holds the post), and what is
    // over its float goes to House Greatly's estate, booked by House.
    const auto pit = Society::farmTill("clay_fens");
    expect(s.operatorTransfer("treasury", pit, "", 0, 200).ok, "the Sallow pits given 200p (moved, not made)");
    const auto before = s.moneySupply();
    s.takeJournal();
    run(s, 46 + 17.9 / 24, 46 + 18.5 / 24);
    const auto& d = s.loopToday();
    const auto& day = d.farms.at("clay_fens");
    expect(day.wagesOwed == 10 && day.wagesPaid == 10, "the pit paid its hand 10p: " + std::to_string(day.wagesPaid));
    expect(day.rent >= 200 - 10 && s.cash(pit) == farm(s, "clay_fens").reserve && farm(s, "clay_fens").reserve == 20,
           "its rent what was over its 20p float: " + std::to_string(day.rent));
    std::int64_t rents = 0, booked = 0;
    for (const auto& w : stone.farms)
    {
        const auto& today = d.farms.at(w.id);
        expect(today.wagesPaid == today.wagesOwed && today.wagesOwed == farm(s, w.id).wages(), w.id + " paid its crew");
        rents += today.rent;
    }
    for (const auto& e : s.takeJournal())
        if (e.kind == "an estate's rent: House Greatly")
        {
            expect(e.to == "estate:greatly", "a works' rent to the estates' purse");
            booked += e.coins;
        }
    expect(d.rents.count("Greatly") && d.rents.at("Greatly") == rents && booked == rents, "booked to House Greatly: " + std::to_string(rents));
    expect(s.conserved() && s.moneySupply() == before, "conserved");
    // Off: the Country sends stone, clay and sand again.
    auto e = s.authored().economy;
    e.farms.areas["stone"].on = 0;
    s.adoptEconomy(e);
    const auto c2 = s.cash(Society::CountryFund);
    const auto back = s.drawFromStore("mason", "stone", 2, "restocked from the store room");
    expect(back.ok && back.message.find("from the farms") == std::string::npos && s.cash(Society::CountryFund) > c2 &&
               s.farmAreaOf("clay") < 0,
           "stone off: the Country's stone and clay again: " + back.message);
    expect(s.conserved(), "conserved");
}

void theLandRent()
{
    // Doc 72, open question 2 (taken once country3 showed the Country's intake, 814p a day in year 3, under its 824p of
    // posts): each food farm pays the House whose land it is on its rule's percent of its takings each night, after its
    // crew's wages, out of what is over its float: the share its basket would have spent, never its wages. Into the
    // Country's purse ("the Houses' estates and the sea trade"), booked by House.
    const auto& f = items::farms();
    const std::map<std::string, std::string> houses{{"grain", "the Crown"}, {"fruit", "Fell"}, {"meat", "the Crown"},
                                                    {"fish", "Grayrock"}, {"honey", "Fell"}, {"salt", "the Crown"}};
    for (const auto& a : f.areas)
        expect(a.works || houses.at(a.id) == a.house, a.id + ": its farms on " + a.house + "'s land");
    // The percents: the rule's 16 but where an area has its own (country.json: fish 9, fruit 4, grain 28, salt 8).
    auto s = city([](EconomySpec& e) { e.farms.areas["grain"].on = 1; }, aStore());
    const std::map<std::string, int> percent{{"grain", 28}, {"fruit", 4}, {"meat", 16}, {"fish", 9}, {"honey", 16}, {"salt", 8},
                                             {"fuel", 0}, {"stone", 0}};
    // (Doc 72, Phase 4's payer: the three cattle and dairy farms their own rule, landRentDairy 32 (24 until country5), over the meat area's 16;
    // Phase 5, country4's carry-over: the thinnest farms theirs, landRentThin 0: the lake boats, the salt works, three
    // orchards.)
    int dairies = 0, thin = 0;
    for (const auto& now : s.farms())
    {
        const bool dairy = now.def->landRent == "dairy";
        dairies += dairy;
        thin += now.def->landRent == "thin";
        expect(s.landRentPercent(now) == (dairy ? 32 : now.def->landRent == "thin" ? 0 : percent.at(now.def->area)) &&
                   now.house == (now.works ? now.house : houses.at(now.def->area)),
               now.def->id + "'s land rent " + std::to_string(s.landRentPercent(now)) + "%, to " + now.house);
        expect(!dairy || (now.def->area == "meat" && now.def->kind == "cattle and dairy farm"), "a dairy's own rule: " + now.def->id);
    }
    expect(dairies == 3, "the three dairies their own rule: " + std::to_string(dairies));
    expect(thin == 7, "the seven thinnest farms theirs: " + std::to_string(thin));
    {
        auto tuned = city([](EconomySpec& e) {
            e.rules["landRent"] = 10;
            e.rules["landRentGrain"] = 0;
            e.rules["landRentDairy"] = 5;
        });
        expect(tuned.landRentPercent(farm(tuned, "grain_farm_1")) == 0 && tuned.landRentPercent(farm(tuned, "beekeeper_1")) == 10 &&
                   tuned.landRentPercent(farm(tuned, "salt_works_1")) == 0 && tuned.landRentPercent(farm(tuned, "salt_works_1")) == tuned.rule("landRentThin") &&
                   tuned.landRentPercent(farm(tuned, "cattle_and_dairy_farm_2")) == 5 && tuned.landRentPercent(farm(tuned, "pig_farm_1")) == 10,
               "the world's own: grain's none, the rule's 10 where an area has no own, the thin salt works' still country.json's 0, a dairy's "
               "its own 5 (the pig farm beside it the meat area's 10)");
    }
    turn(s, 46);
    const auto t1 = Society::farmTill("grain_farm_1"), t2 = Society::farmTill("grain_farm_2");
    // The day's takings: the stall's flour (the farms' till keeps the cost less the toll).
    const auto c1 = s.cash(t1);
    expect(s.drawFromStore("stall", "flour", 99, "restocked from the store room").ok &&
               s.drawFromStore("stall", "flour", 99, "restocked from the store room").ok,
           "the stall's flour from the farms");
    const auto took1 = s.cash(t1) - c1;
    expect(took1 > 0 && farm(s, "grain_farm_1").rentTakings >= took1 && s.landRentDue(farm(s, "grain_farm_1")) ==
                                                                          farm(s, "grain_farm_1").rentTakings * 28 / 100,
           "its rent so far: 28% of its takings: " + std::to_string(s.landRentDue(farm(s, "grain_farm_1"))));
    // Its basket keeps the rent back through the shop hours (given money over, which is not takings and pays no rent).
    for (const char* good : {"nails", "fittings", "lock", "chain", "cookpot"})
        expect(s.create("till:job:smith", good, 99, "test stock"), std::string("the smith holds ") + good);
    expect(s.operatorTransfer("treasury", t1, "", 0, 400).ok, "the grain farm given 400p (moved, not made)");
    run(s, 46 + 8.5 / 24, 46 + 17.9 / 24);
    const auto& g1 = farm(s, "grain_farm_1");
    const auto due1 = s.landRentDue(g1), due2 = s.landRentDue(farm(s, "grain_farm_2"));
    expect(s.loopToday().farms.at("grain_farm_1").basket > 0 && s.cash(t1) >= g1.reserve + g1.wages() + due1,
           "its basket spent, but never its float, tonight's wages or its rent: " + std::to_string(s.cash(t1)) + " against " +
               std::to_string(g1.reserve + g1.wages() + due1));
    // At 18:00: wages paid in full, then the rent, to the Crown's estate (its own purse: doc 72, Phase 6), booked by House.
    const auto money = s.moneySupply(), country = s.cash("estate:crown");
    const auto takenToday = g1.rentTakings;
    s.takeJournal();
    run(s, 46 + 17.9 / 24, 46 + 18.5 / 24);
    const auto& d = s.loopToday();
    std::int64_t booked = 0;
    for (const auto& e : s.takeJournal())
        if (e.kind == "a land rent: the Crown")
        {
            expect(e.to == "estate:crown" && (e.from == t1 || e.from == t2), "a land rent to the estates' purse: " + e.from);
            booked += e.coins;
        }
        else
            expect(e.kind.find("rent") == std::string::npos, "no other rent: " + e.kind);
    for (const char* id : {"grain_farm_1", "grain_farm_2"})
        expect(d.farms.at(id).wagesPaid == d.farms.at(id).wagesOwed && d.farms.at(id).wagesOwed > 0, std::string(id) + ": its crew paid in full");
    expect(d.farms.at("grain_farm_1").rent == due1 && d.farms.at("grain_farm_2").rent == due2 && due1 > 0,
           "each paid its rent: " + std::to_string(due1) + "p and " + std::to_string(due2) + "p");
    expect(d.landRents.count("the Crown") && d.landRents.at("the Crown") == due1 + due2 && booked == due1 + due2 && d.rents.empty(),
           "booked to the Crown, as land rents, not the works' rents");
    expect(s.cash("estate:crown") >= country + due1 + due2 - d.countryPostsPaid && s.cash(t1) >= g1.reserve,
           "the Crown's estate up by the rents (less tonight's posts), the farm's float left whole: " + std::to_string(country) + " -> " +
               std::to_string(s.cash("estate:crown")) + ", posts " + std::to_string(d.countryPostsPaid));
    // (Counted afresh after the rent: a shop open till 19:00 may draw a little more in the half hour after it.)
    expect(farm(s, "grain_farm_1").rentTakings < takenToday && farm(s, "grain_farm_1").rentTakings <= 10,
           "tomorrow's rent from tomorrow's takings: " + std::to_string(farm(s, "grain_farm_1").rentTakings));
    expect(s.conserved() && s.moneySupply() == money, "conserved");
    // Only from what is over its float: a farm left a few pennies over its float and wages pays those few.
    expect(s.operatorTransfer("treasury", "till:job:baker", "", 0, 400).ok, "the baker given 400p for flour");
    const auto next = s.drawFromStore("baker", "flour", 30, "restocked from the store room");
    expect(next.ok, "the next day's flour: " + next.message);
    const auto due = s.landRentDue(farm(s, "grain_farm_1"));
    const auto keepOnly = farm(s, "grain_farm_1").reserve + farm(s, "grain_farm_1").wages() + 3;
    const auto off = s.cash(t1) - keepOnly;
    expect(due > 3 && (off > 0 ? s.operatorTransfer(t1, "treasury", "", 0, off).ok : s.operatorTransfer("treasury", t1, "", 0, -off).ok) &&
               s.cash(t1) == keepOnly,
           "the grain farm's till set to 3p over its float and wages: due " + std::to_string(due));
    run(s, 47 + 17.9 / 24, 47 + 18.5 / 24);
    expect(s.loopToday().farms.at("grain_farm_1").rent == 3 && s.cash(t1) == farm(s, "grain_farm_1").reserve,
           "it paid the 3p over its float and wages, no more: " + std::to_string(s.loopToday().farms.at("grain_farm_1").rent));
    // Short of its wages, it pays none (its takings' rent let go; the treasury's support as before).
    const auto third = s.drawFromStore("baker", "flour", 30, "restocked from the store room");
    expect(third.ok, "another day's flour: " + third.message);
    expect(s.landRentDue(farm(s, "grain_farm_1")) > 0 && s.operatorTransfer(t1, "treasury", "", 0, s.cash(t1)).ok, "the grain farm's till emptied");
    const auto before = s.moneySupply();
    run(s, 48 + 17.9 / 24, 48 + 18.5 / 24);
    const auto& short1 = s.loopToday().farms.at("grain_farm_1");
    expect(short1.wagesPaid < short1.wagesOwed && short1.rent == 0 && farm(s, "grain_farm_1").rentTakings <= 10,
           "short of its wages: no rent, and none owed tomorrow (but a draw after 18:00): " +
               std::to_string(farm(s, "grain_farm_1").rentTakings));
    expect(s.conserved() && s.moneySupply() == before, "conserved");
    // A works pays its estates' rent as before, never a land rent.
    auto w = city([](EconomySpec& e) { e.farms.areas["stone"].on = 1; });
    turn(w, 46);
    for (const auto& x : w.farms())
        expect(!x.works || w.landRentDue(x) == 0, x.def->id + ": a works owes no land rent");
}

void theCountryPaysItsPosts()
{
    // The land rent's purpose (doc 72, open question 2): the Country's intake under its posts (country3's year 3) and its
    // posts paid again with the farms' rent. Six dockers of the sea trade, the Country's purse emptied; the
    // food areas on; only the nights run (no shop hours: nothing of its own sold). Without the rent its empty purse can't
    // pay its post; with it, each night's rent pays the next night's post.
    std::vector<ResidentSpec> dockers;
    for (int i = 0; i < 6; ++i)
        dockers.push_back(person("docker" + std::to_string(i), "civilian", "unloading ships at the wharf", {"wharf", 2.5, 2.5},
                                 "home_docker" + std::to_string(i)));
    const auto posts = [&](bool rent) {
        auto s = city(
            [&](EconomySpec& e) {
                e.countryReserve = 0;
                for (const auto& a : items::farms().areas)
                    e.farms.areas[a.id].on = a.works ? 0 : 1;
                for (const char* r : {"landRent", "landRentFish", "landRentFruit", "landRentGrain", "landRentSalt", "landRentDairy"})
                    if (!rent)
                        e.rules[r] = 0;
            },
            dockers);
        turn(s, 46);
        // Its purse emptied; the farms' takings: the stall's and the butcher's draws (the Country sells them none; the
        // makers' crafts at the day's end draw a little of its own, not six posts' worth).
        if (const auto c = s.cash(Society::CountryFund); c > 0)
            expect(s.operatorTransfer(Society::CountryFund, "treasury", "", 0, c).ok, "the Country's purse emptied");
        std::int64_t owed = 0, paid = 0, rents = 0;
        for (int day = 46; day < 49; ++day)
        {
            for (const char* shop : {"till:job:stall", "till:job:butcher", "till:job:baker"})
                expect(s.operatorTransfer("treasury", shop, "", 0, 400).ok, std::string(shop) + " given 400p");
            for (const auto& [shop, good] : {std::pair<const char*, const char*>{"stall", "flour"}, {"baker", "flour"}, {"butcher", "raw_meat"}})
                expect(s.drawFromStore(shop, good, 99, "restocked from the store room").ok, std::string(shop) + "'s " + good + " from the farms");
            for (const auto& f : s.farms())         // (Their wages topped up, moved not made: the takings all over them.)
                if (s.account(f.till))
                    expect(s.operatorTransfer("treasury", f.till, "", 0, f.wages()).ok, f.def->id + "'s wages topped up");
            run(s, day + 17.9 / 24, day + 18.5 / 24);
            if (day > 46)
                owed += s.loopToday().countryPostsOwed, paid += s.loopToday().countryPostsPaid;
            for (const auto& [house, n] : s.loopToday().landRents)
                rents += n;
        }
        expect(s.conserved(), "conserved");
        return std::make_tuple(owed, paid, rents);
    };
    const auto [owedWith, paidWith, rentsWith] = posts(true);
    const auto [owedWithout, paidWithout, rentsWithout] = posts(false);
    expect(owedWith == 2 * 6 * 8 && owedWith == owedWithout, "the dockers' posts owed each night: " + std::to_string(owedWith));
    expect(rentsWithout == 0 && paidWithout < owedWithout, "no rent: the Country's empty purse leaves its posts unpaid: " +
                                                               std::to_string(paidWithout) + " of " + std::to_string(owedWithout));
    expect(rentsWith > 0 && paidWith == owedWith, "the farms' land rent pays it: " + std::to_string(paidWith) + " of " + std::to_string(owedWith) +
                                                      " (rents " + std::to_string(rentsWith) + "p)");
}

void theTableShare()
{
    // The Country's table feeds its own people still in the Country (doc 71: 24 of the estates, 68 workers of its other
    // industries). A works' crew is of those workers: with its area on it buys its food in the shops, and eats at the
    // table no more (doc 72, Phase 3: the fuel's 15 and the stone's 4 of the 92, the quarry 1 + 1 since country3).
    auto s = city([](EconomySpec& e) {
        for (const auto& a : items::farms().areas)
            e.farms.areas[a.id].on = a.works ? 0 : 1;
    });
    turn(s, 46);
    expect(std::abs(s.tableShare() - 1) < 1e-9, "the works off: all 92 at the table");
    const auto eatenOver = [&](Society& w, int from) {
        std::int64_t eaten = 0;
        for (int day = from; day < from + 20; ++day)
        {
            turn(w, day);
            eaten += w.loopToday().farms.at("grain_farm_1").eaten;
        }
        return eaten;
    };
    const auto all = eatenOver(s, 47);
    auto e = s.authored().economy;
    e.farms.areas["fuel"].on = 1;
    e.farms.areas["stone"].on = 1;
    s.adoptEconomy(e);
    expect(std::abs(s.tableShare() - 73. / 92) < 1e-9, "the fuel's and the stone's 19 crews out: 73 of 92: " + std::to_string(s.tableShare()));
    const auto fewer = eatenOver(s, 67);
    expect(all > 0 && std::abs(double(fewer) / double(all) - 73. / 92) < .06,
           "a grain farm's table eats 73/92 as much: " + std::to_string(fewer) + " of " + std::to_string(all));
    expect(s.conserved(), "conserved");
}

void firewoodOrPeat()
{
    // A household's fuel is "any firewood or peat" (crafts.json): it takes whichever its shops hold, half and half when
    // they hold both (doc 72: peat a second fuel in place of part of the firewood); the general store supplies both.
    auto s = city({}, aStore());
    expect(s.storeUse("general", "peat") == Society::StoreUse::Resell && s.storeUse("general", "firewood") == Society::StoreUse::Resell,
           "the general store resells peat and firewood");
    const auto burned = [](Society& w, std::set<std::string> off) {
        auto e = w.authored().economy;
        e.countryOff = off;
        w.adoptEconomy(e);
        const auto till = w.tillOf("general");
        for (const char* fuel : {"firewood", "peat"})
            if (off.count(fuel))
                w.consume(till, fuel, held(w, till, fuel), "test: none");
        w.takeJournal();
        run(w, 46 + 7.5 / 24, 46 + 18. / 24);
        std::map<std::string, int> used;
        for (const auto& j : w.takeJournal())
            if (j.kind == "used at home")
                used[j.item] += j.quantity;
        return used;
    };
    {
        auto w = city({}, aStore());
        const auto used = burned(w, {"firewood"});
        expect(used.count("peat") && used.at("peat") > 0 && !used.count("firewood"), "no firewood to be had: the households burn peat");
    }
    {
        auto w = city({}, aStore());
        const auto used = burned(w, {"peat"});
        expect(used.count("firewood") && used.at("firewood") > 0 && !used.count("peat"), "no peat: firewood, as before");
    }
    {
        auto w = city({}, aStore());
        for (const char* fuel : {"firewood", "peat"})
            expect(w.create(w.tillOf("general"), fuel, 60, "test stock"), std::string("the store holds ") + fuel);
        int peat = 0, wood = 0;
        for (int day = 0; day < 6; ++day)
        {
            w.takeJournal();
            run(w, 46 + day + 7.5 / 24, 46 + day + 18. / 24);
            for (const auto& j : w.takeJournal())
                if (j.kind == "used at home" && (j.item == "peat" || j.item == "firewood"))
                    (j.item == "peat" ? peat : wood) += j.quantity;
        }
        expect(peat > 0 && wood > 0, "both held: some households burn one, some the other: " + std::to_string(peat) + " peat, " +
                                         std::to_string(wood) + " firewood");
    }
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
        expect(std::abs(now.yieldScale - 1.08) < 1e-9 && now.hands == 8 && now.keeper == 18 && now.hand == 12,
               "applied: 120% of 90%, 5 hands at 150%, the area's keeper, the farm's hand");
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
        herbsAndFibre();
        spoiling();
        theDairy();
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
        theWorks();
        theStoneWorks();
        theLandRent();
        theCountryPaysItsPosts();
        theTableShare();
        firewoodOrPeat();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Farms tests passed: " << checks << " checks.\n";
    return 0;
}
