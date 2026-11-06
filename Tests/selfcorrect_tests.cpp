// The self-correcting rules (Docs/Design/68-the-country-and-the-city-loop.md, Progress, "Self-correcting rules"): a shop's
// prices follow its stock, its shelves its sales, a till's wages its cover, and the gate toll and the alms the treasury's
// level. For each: its bounds, its rate (how far it may move a day, a window or a fortnight), money conserved, what it
// has drifted to surviving a save and a restore, and its switch off giving the rule as it was (whatever a save holds).
//
// The catalogue is the real Data/Items (RATW_SOURCE_DIR).
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwJsonDoc.h"
#include "RatwSociety.h"
#include "RatwWire.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

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
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
std::string str(double x)
{
    return std::to_string(x);
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

// A city of these residents, its treasury and these rules: the Country, the household services, the patrons and the
// surplus off (nothing else buys from the shops but what a test does), and of the four rules only those it names on.
Society city(std::vector<ResidentSpec> residents, std::int64_t treasury, std::map<std::string, int> rules)
{
    AuthoredRoster r;
    r.residents = std::move(residents);
    r.economy.treasury = treasury;
    r.economy.storeRule = true;
    r.economy.wharf = false;
    std::map<std::string, int> base = {{"services", 0}, {"patrons", 0}, {"surplus", 0}, {"stockPrices", 0},
                                       {"shelfFollow", 0}, {"wageDrift", 0}, {"treasuryDrift", 0},
                                       {"wharfValve", 0}, {"almsNoEarner", 100}};   // (The alms as any household's: econ_fix_tests has the no-earner's, doc 73, round 4. The Wharf's valve shut, doc 73: the Country's flows as these tests have them; wharf_tests and farms_tests open it.)
    for (const auto& [k, v] : rules)
        base[k] = v;
    r.economy.rules = base;
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

// The society run in ten-second ticks, everyone at work; `until(day)` runs it to that time (day 1 is the first).
struct Town
{
    static constexpr double Step = 10;
    Society s;
    std::map<std::string, LifeBody> bodies;
    double now = 1;
    explicit Town(Society society) : s(std::move(society))
    {
        for (const auto& r : s.authored().residents)
        {
            LifeBody b{r.work.cell, r.work.x, r.work.y};
            b.age = r.age;
            bodies[r.id] = b;
        }
    }
    void until(double day)
    {
        for (; now < day; now += Step / calendar::SecondsPerDay)
            s.tick(Step, now, 0, bodies);
    }
};

bool restDay(int day)
{
    return calendar::weekdayOf(day + .5) == calendar::Restday || calendar::festivalDay(day + .5);
}

// The fishmonger's evening sale (20:00, the shop shut): so many fish put on the till's shelf and so many of them sold
// (to the treasury, which uses them up): `put` and `sell`.
void evening(Town& t, int day, int put, int sell)
{
    t.until(day + 20. / 24);
    const auto till = t.s.tillOf("fish");
    if (put > 0)
        expect(t.s.create(till, "fish", put, "test: fish in"), "fish put on the shelf");
    if (sell > 0)
    {
        expect(t.s.sale(till, "treasury", "fish", sell, Society::catalogPrice("fish"), "test: sold"), "fish sold");
        t.s.consume("treasury", "fish", sell, "test: eaten");
    }
}

// A fishmonger whose till is empty (it can draw nothing: only the test's fish are on its shelves), no wage support and
// no alms (nobody with money to buy them).
Town fishmonger(std::map<std::string, int> rules)
{
    rules["wageSupport"] = 0;
    rules["almsADay"] = 0;
    Town t(city({person("fish", "merchant", "fishmonger at the Fish Hall", {"hall", 2.5, 2.5}, "home_fish", 30, 0)}, 1000000, rules));
    const auto till = t.s.tillOf("fish");
    if (const auto cash = t.s.cash(till); cash > 0)
        t.s.shift(till, "treasury", "", 0, cash, "test: an empty till");
    const auto stock = t.s.account(till)->stock;    // (And bare shelves.)
    for (const auto& [item, n] : stock)
        if (n > 0)
            t.s.consume(till, item, n, "test: bare shelves");
    return t;
}

void theDefaults()
{
    const Society s(Roster::None);
    expect(s.rule("stockPrices") == 1 && s.rule("stockPriceBand") == 25 && s.rule("stockPriceStep") == 10,
           "prices follow stock: on, within 25%, a hundredth a day");
    expect(s.rule("shelfFollow") == 1 && s.rule("shelfDays") == 7 && s.rule("shelfCover") == 125 && s.rule("shelfGrow") == 25,
           "shelves follow sales: on, over a week, a quarter over them, a quarter more on selling out");
    expect(s.rule("wageDrift") == 1 && s.rule("wageDriftDays") == 28 && s.rule("wageDriftSpare") == 50 &&
               s.rule("wageDriftMost") == 12 && s.rule("wageDriftHandMost") == 12 && s.rule("wageDriftFloor") == 7,
           "wages follow cover: on, every 28 days, half to spare, at most 12p a keeper and 12p a hand, never under 7p");
    expect(s.rule("treasuryDrift") == 1 && s.rule("treasuryTarget") == 5500 && s.rule("treasuryBand") == 15 &&
               s.rule("treasuryDays") == 182 && s.rule("treasuryJudgeDays") == 28 && s.rule("tollStep") == 5 &&
               s.rule("tollDriftTenths") == 10 && s.rule("almsDriftMost") == 2,
           "the toll and the alms follow the treasury: on, 5,500p within 15%, over 182 days, judged every 28, half a point "
           "each time, 1 point (doc 73, round 10; 2 in round 9), 2p");
    expect(near(s.gateTollNow(), 30) && s.almsNow() == 10 && s.state().loop.empty(), "nothing drifted in a new society");
}

// The fishmonger's salt fish beside its fish: 30 sold of a shelf kept at 99 each evening (never sold out, nor a glut).
void saltFish(Town& t)
{
    const auto till = t.s.tillOf("fish");
    if (const int have = Society::stock(*t.s.account(till), "salt_fish"); have < 99)
        t.s.create(till, "salt_fish", 99 - have, "test: salt fish in");
    expect(t.s.sale(till, "treasury", "salt_fish", 30, Society::catalogPrice("salt_fish"), "test: sold"), "salt fish sold");
    t.s.consume("treasury", "salt_fish", 30, "test: eaten");
}

void pricesFollowStock()
{
    auto t = fishmonger({{"stockPrices", 1}});
    const auto till = t.s.tillOf("fish");
    const auto wares = t.s.wares("fish");
    expect(std::find(wares.begin(), wares.end(), "fish") != wares.end() && std::find(wares.begin(), wares.end(), "salt_fish") != wares.end(),
           "the fishmonger sells fish and salt fish");
    const auto money = t.s.moneySupply();
    const auto mean = [&](const std::string& ware) { return t.s.state().loop.sales.at(till).at(ware); };
    // The city's price level: its wares' factors by their sales (fish, salt fish, and the smoked fish its keeper makes and
    // eats once the till has money), 1 while none is at a bound.
    const auto level = [&]() {
        double sum = 0, weights = 0;
        for (const auto& [ware, m] : t.s.state().loop.sales.at(till))
        {
            sum += m * t.s.stockFactor(till, ware);
            weights += m;
        }
        return sum / weights;
    };
    // Fish sold out every evening: dearer each day (none over a Restday or festival) by about a hundredth, salt fish a
    // little cheaper against it, the two together at the catalogue's; to the band's top, 1.25, and no further.
    double f = 1;
    for (int day = 1; day <= 70; ++day)
    {
        evening(t, day, 3, 3);
        saltFish(t);
        t.until(day + 1 + 1. / 24);                 // (The day's turn.)
        const double now = t.s.stockFactor(till, "fish");
        if (restDay(day))
            expect(near(now, f), "day " + std::to_string(day) + ": a Restday or festival leaves it: " + str(now));
        else
            expect((now > f || near(now, 1.25)) && now - f <= .0105, "day " + std::to_string(day) + ": sold out, up at most a hundredth: " +
                                                                        str(f) + " to " + str(now));
        expect(now <= 1.25 + 1e-9 && t.s.stockFactor(till, "salt_fish") <= 1, "within the band, the plentiful cheaper");
        if (now < 1.25 - 1e-9 && !restDay(day))
            expect(near(level(), 1), "day " + std::to_string(day) + ": the price level held at 1: " + str(level()));
        f = now;
    }
    expect(near(f, 1.25), "at the band's top: " + str(f));
    // Quiet days (sold 1 of 3, nothing over its starting stock left): back toward 1.
    for (int day = 71; day <= 74; ++day)
    {
        evening(t, day, 3, 1);
        saltFish(t);
        t.until(day + 1 + 1. / 24);
        t.s.consume(till, "fish", Society::stock(*t.s.account(till), "fish"), "test: thrown out");
        const double now = t.s.stockFactor(till, "fish");
        expect(restDay(day) ? near(now, f) : now < f && f - now <= .0105, "a quiet day: back toward 1, a step at most: " + str(f) + " to " + str(now));
        f = now;
    }
    // Salt fish glutted (198 on the shelf, 1 a day sold) while the fish still sells out, applied live at a tenth a day:
    // the salt fish (its own make: nothing the store room charges for it) down to the band's foot, 0.75, and no further.
    auto economy = t.s.authored().economy;
    economy.rules["stockPriceStep"] = 100;
    t.s.adoptEconomy(economy);
    const auto sellSalt = [&](int n) {
        expect(t.s.sale(till, "treasury", "salt_fish", n, Society::catalogPrice("salt_fish"), "test: sold"), "salt fish sold");
        t.s.consume("treasury", "salt_fish", n, "test: eaten");
    };
    t.until(75);
    for (int lot = 0; lot < 2; ++lot)
        t.s.create(till, "salt_fish", 99, "test: salt fish in");
    for (int day = 75; day <= 110; ++day)
    {
        evening(t, day, 3, 3);
        sellSalt(1);
        t.until(day + 1 + 1. / 24);
        expect(t.s.stockFactor(till, "salt_fish") >= .75 - 1e-9, "within its floor: " + str(t.s.stockFactor(till, "salt_fish")));
    }
    expect(near(t.s.stockFactor(till, "salt_fish"), .75), "glutted, its own make: the band's foot: " + str(t.s.stockFactor(till, "salt_fish")));
    // The fish glutted (60 on the shelf, 1 a day) while the salt fish sells out: the fish (an input of its crafts it also
    // sells, drawn at the store room's making price, 100%) never under that and a little: as cheap as 1, no cheaper.
    t.s.consume(till, "salt_fish", Society::stock(*t.s.account(till), "salt_fish"), "test: thrown out");
    evening(t, 111, 60, 1);
    for (int day = 111; day <= 125; ++day)
    {
        if (day > 111)
            evening(t, day, 0, 1);
        t.s.create(till, "salt_fish", 5, "test: salt fish in");
        sellSalt(5);
        t.until(day + 1 + 1. / 24);
        expect(t.s.stockFactor(till, "fish") >= 1 - 1e-9, "within its floor: " + str(t.s.stockFactor(till, "fish")));
    }
    expect(near(t.s.stockFactor(till, "fish"), 1), "glutted, drawn at the making price: no cheaper than it: " + str(t.s.stockFactor(till, "fish")));
    expect(t.s.conserved() && t.s.moneySupply() == money, "conserved");
    // The switch off: the factor saved is kept but not used.
    economy.rules["stockPrices"] = 0;
    t.s.adoptEconomy(economy);
    expect(near(t.s.stockFactor(till, "fish"), 1) && t.s.shopCost(till, "fish", 10) == t.s.seasonalCost("fish", 10),
           "off: the season's price, whatever was saved");
    expect(!t.s.state().loop.prices.empty(), "(what drifted is kept for when it is on again)");
}

void residentsPayTheShopsPrice()
{
    // A fishmonger with fish at 1.2 times the price, and a resident with money and no food: what she pays is the season's
    // price times the shop's, over the day's buys (the fractions of a penny carried as the season's are).
    for (const bool on : {true, false})
    {
        auto t = Town(city({person("fish", "merchant", "fishmonger at the Fish Hall", {"hall", 2.5, 2.5}, "home_fish", 30, 0),
                            person("ann", "civilian", "keeps the house", {"home_ann", 2.5, 2.5}, "home_ann", 35, 200)},
                           10000, {{"stockPrices", on ? 1 : 0}, {"wageSupport", 0}}));
        const auto till = t.s.tillOf("fish");
        t.s.create(till, "fish", 99, "test: fish in");   // (A lot at most 99: the ledger's.)
        t.until(1 + 1. / 24);                   // (A society restores once it has begun its days.)
        auto saved = t.s.state();
        saved.loop.prices[till]["fish"] = 1.2;
        expect(t.s.restore(saved), "restored with a factor of 1.2");
        const auto money = t.s.moneySupply();
        t.s.takeJournal();
        t.until(1 + 17.5 / 24);                     // (Ann's market hour is before 17:00.)
        std::int64_t base = 0, paid = 0;
        double want = 0;
        expect(near(t.s.stockFactor(till, "fish"), on ? 1.2 : 1), "the shop's factor: " + str(t.s.stockFactor(till, "fish")));
        for (const auto& e : t.s.takeJournal())
        {
            if (e.kind == "market" && e.from == "ann")
            {
                base += e.coins;
                paid += e.coins;
                want += double(e.coins) * t.s.priceFactor(e.item) * t.s.stockFactor(till, e.item);
            }
            else if (e.kind == "market: the season's price" && e.from == "ann")
                paid += e.coins;
            else if (e.kind == "market: the season's price" && e.to == "ann")
                paid -= e.coins;
        }
        expect(base > 0, "ann bought fish");
        expect(std::abs(double(paid) - want) <= 1, std::string(on ? "on" : "off") + ": she paid " + std::to_string(paid) +
                                                       "p for " + std::to_string(base) + "p of fish at the catalogue's price, " +
                                                       "want " + str(want));
        expect(t.s.conserved() && t.s.moneySupply() == money, "conserved");
    }
}

void playersPayTheShopsPrice()
{
    // Players in the shifting market (doc 68, Progress, "Players in the shifting market; wider wages; the long runs"; the
    // user, 2026-11-01: "players should experience the economy shifts as well"): a fishmonger with fish at 1.2 times the
    // price and salt fish at 0.8. A player pays the season's price times the shop's for a lot, to the nearest penny and
    // never under a penny a piece, and the shop offers SellBackShare of that; what the quote says is what the trade
    // takes. Off, the season's price alone.
    for (const bool on : {true, false})
    {
        auto t = Town(city({person("fish", "merchant", "fishmonger at the Fish Hall", {"hall", 2.5, 2.5}, "home_fish", 30, 0)}, 10000,
                           {{"stockPrices", on ? 1 : 0}, {"wageSupport", 0}}));
        const auto till = t.s.tillOf("fish");
        const std::string ada = "player-ada";
        t.s.addPlayer(ada);
        t.s.shift("treasury", ada, "", 0, 500, "test: a purse");
        t.s.shift("treasury", till, "", 0, 500, "test: a till");
        t.s.create(till, "fish", 40, "test: fish in");
        t.s.create(ada, "salt_fish", 3, "test: her salt fish");
        t.until(1 + 1. / 24);                       // (A society restores once it has begun its days.)
        auto saved = t.s.state();
        saved.loop.prices[till]["fish"] = 1.2;
        saved.loop.prices[till]["salt_fish"] = .8;
        expect(t.s.restore(saved), "restored with factors");
        if (const int n = Society::stock(*t.s.account(till), "salt_fish"); n > 0)
            t.s.consume(till, "salt_fish", n, "test: room for hers");   // (A shop takes a few of a good: GoodsKept.)
        const auto money = t.s.moneySupply();
        const double fish = t.s.priceFactor("fish") * (on ? 1.2 : 1), salt = t.s.priceFactor("salt_fish") * (on ? .8 : 1);
        expect(near(t.s.shopFactor(till, "fish"), fish) && near(t.s.shopFactor(till, "salt_fish"), salt),
               "the shop's price: the season's times its own: " + str(t.s.shopFactor(till, "fish")));
        const auto lot = [](std::int64_t each, double f, int n) { return std::max<std::int64_t>(n, std::llround(double(each) * f * n)); };
        const auto one = t.s.quote(ada, "fish", "fish", 1, true), ten = t.s.quote(ada, "fish", "fish", 10, true);
        expect(one.ok && one.unitPrice == lot(Society::catalogPrice("fish"), fish, 1) && one.total == one.unitPrice,
               std::string(on ? "on" : "off") + ": a fish is " + std::to_string(one.unitPrice) + "p");
        expect(ten.ok && ten.total == lot(Society::catalogPrice("fish"), fish, 10) && ten.unitPrice == one.unitPrice,
               std::string(on ? "on" : "off") + ": ten fish are " + std::to_string(ten.total) + "p");
        if (on)
            expect(ten.total > Society::catalogPrice("fish") * 10 * t.s.priceFactor("fish") - 1, "scarce: dearer than the season's");
        const auto offer = t.s.quote(ada, "fish", "salt_fish", 3, false);
        expect(offer.ok && offer.total == lot(Society::catalogPrice("salt_fish"), salt * Society::SellBackShare, 3),
               std::string(on ? "on" : "off") + ": three salt fish fetch " + std::to_string(offer.total) + "p: " + offer.message);
        // The trade takes what the quote says, the odd pennies too.
        const auto before = t.s.cash(ada), shop = t.s.cash(till);
        const auto bought = t.s.trade(ada, "fish", "fish", 10, true);
        expect(bought.ok && t.s.cash(ada) == before - ten.total && t.s.cash(till) == shop + ten.total &&
                   Society::stock(*t.s.account(ada), "fish") == 10,
               "bought ten fish for the quote's " + std::to_string(ten.total) + "p: " + bought.message);
        expect(bought.message.find(std::to_string(ten.total) + " silver pennies") != std::string::npos, "and says so");
        const auto sold = t.s.trade(ada, "fish", "salt_fish", 3, false);
        expect(sold.ok && t.s.cash(ada) == before - ten.total + offer.total && Society::stock(*t.s.account(till), "salt_fish") >= 3,
               "sold three salt fish for the offer's " + std::to_string(offer.total) + "p: " + sold.message);
        expect(t.s.conserved() && t.s.moneySupply() == money, "conserved");
    }
    // A player's purchase is a sale like a resident's: buying the shelf out makes the ware dearer at the day's turn (and
    // the shop's shelf counts it).
    {
        auto t = fishmonger({{"stockPrices", 1}, {"shelfFollow", 1}});
        const auto till = t.s.tillOf("fish");
        const std::string ada = "player-ada";
        t.s.addPlayer(ada);
        t.s.shift("treasury", ada, "", 0, 500, "test: a purse");
        int day = 1;
        while (restDay(day) || restDay(day + 1))
            ++day;
        evening(t, day, 6, 0);
        saltFish(t);
        const auto bought = t.s.trade(ada, "fish", "fish", 6, true);
        expect(bought.ok, "she buys the shelf out: " + bought.message);
        t.until(day + 1 + 1. / 24);
        expect(t.s.stockFactor(till, "fish") > 1, "sold out to a player: dearer the next day: " + str(t.s.stockFactor(till, "fish")));
        expect(t.s.state().loop.sales.at(till).at("fish") >= 6, "her six counted in the shop's sales");
    }
}

void shelvesFollowSales()
{
    auto t = fishmonger({{"shelfFollow", 1}});
    const auto till = t.s.tillOf("fish");
    const auto money = t.s.moneySupply();
    const auto mean = [&]() {
        const auto& sales = t.s.state().loop.sales;
        const auto a = sales.find(till);
        if (a == sales.end())
            return -1.;
        const auto b = a->second.find("fish");
        return b == a->second.end() ? -1. : b->second;
    };
    // Ten a day, never selling out (what is left thrown out before the next): its first day's sales, then held.
    int day = 1;
    for (; day <= 3; ++day)
    {
        evening(t, day, 20, 10);
        t.until(day + 1 + 1. / 24);
        t.s.consume(till, "fish", Society::stock(*t.s.account(till), "fish"), "test: thrown out");
    }
    expect(near(mean(), 10) && t.s.shelfOf(till, "fish") == std::max(13, int(std::ceil(13 * t.s.shelfFactor("fish") - 1e-9))),
           "ten a day: a shelf of 13 (the season's shelf aside): " + str(mean()));
    // Twenty a day: a seventh of the way each day, never more.
    double m = 10;
    for (; day <= 10; ++day)
    {
        evening(t, day, 30, 20);
        t.until(day + 1 + 1. / 24);
        t.s.consume(till, "fish", Society::stock(*t.s.account(till), "fish"), "test: thrown out");
        m += (restDay(day) ? std::max(20., m) - m : 20 - m) / 7;
        expect(near(mean(), m), "day " + std::to_string(day) + ": a seventh of the way to 20: " + str(mean()) + ", not " + str(m));
    }
    expect(m > 15 && m < 20, "not there in a week: " + str(m));
    // A Restday's single sale never lowers it.
    for (; !restDay(day); ++day)
    {
        evening(t, day, 30, 20);
        t.until(day + 1 + 1. / 24);
        t.s.consume(till, "fish", Society::stock(*t.s.account(till), "fish"), "test: thrown out");
    }
    const double before = mean();
    evening(t, day, 5, 1);
    t.until(day + 1 + 1. / 24);
    expect(mean() >= before - 1e-9, "a Restday's or festival's few sales never lower it (the day before's count): " + str(mean()) +
                                        " against " + str(before));
    expect(t.s.conserved() && t.s.moneySupply() == money, "conserved");

    // A shelf too small to show its demand: a shelf's worth sold out each day (all it had) grows (the small shelf's spiral).
    auto u = fishmonger({{"shelfFollow", 1}});
    const auto till2 = u.s.tillOf("fish");
    int last = u.s.shelfOf(till2, "fish");
    for (int d = 1; d <= 12; ++d)
    {
        const int shelf = u.s.shelfOf(till2, "fish");
        evening(u, d, shelf, shelf);
        u.until(d + 1 + 1. / 24);
    }
    expect(u.s.shelfOf(till2, "fish") > 2 * last, "sold out each day, it grows: " + std::to_string(u.s.shelfOf(till2, "fish")) +
                                                      " from " + std::to_string(last));
    // One a day: never under its starting stock.
    auto v = fishmonger({{"shelfFollow", 1}});
    const auto till3 = v.s.tillOf("fish");
    for (int d = 1; d <= 4; ++d)
    {
        evening(v, d, 5, 1);
        v.until(d + 1 + 1. / 24);
        v.s.consume(till3, "fish", Society::stock(*v.s.account(till3), "fish"), "test: thrown out");
    }
    expect(v.s.shelfOf(till3, "fish") >= Society::GoodsKept, "one a day: its starting stock at least");
    // At most a thousand.
    auto w = fishmonger({{"shelfFollow", 1}});
    const auto till4 = w.s.tillOf("fish");
    w.until(1 + 1. / 24);
    auto saved = w.s.state();
    saved.loop.sales[till4]["fish"] = 5000;
    expect(w.s.restore(saved), "restored with 5,000 a day");
    expect(w.s.shelfOf(till4, "fish") <= Society::StoreDrawMost, "at most a draw's worth: " + std::to_string(w.s.shelfOf(till4, "fish")));
    // Off: a quarter over yesterday's sales, as it was, whatever the save holds.
    auto off = fishmonger({});
    const auto till5 = off.s.tillOf("fish");
    off.until(1 + 1. / 24);
    auto offSaved = off.s.state();
    offSaved.loop.sales[till5]["fish"] = 100;
    expect(off.s.restore(offSaved), "restored with 100 a day");
    evening(off, 1, 20, 10);
    off.until(2 + 1. / 24);
    expect(off.s.shelfOf(till5, "fish") == std::max(13, int(std::ceil(13 * off.s.shelfFactor("fish") - 1e-9))),
           "off: a quarter over yesterday's ten: " + std::to_string(off.s.shelfOf(till5, "fish")));
    expect(off.s.state().loop.sales.at(till5).at("fish") == 100, "and nothing tracked with both stock rules off");
}

// A smith and his hand at the forge, a barber and hers, and a clerk the treasury pays: each evening (17:30, before the
// wages) the forge takes in `takings` from the treasury and the barber's shop is emptied into it.
struct Wages
{
    Town t;
    std::int64_t takings;
    void day(int d)
    {
        t.until(d + 17.5 / 24);
        const auto forge = t.s.tillOf("smith"), shop = t.s.tillOf("barber");
        if (takings > 0)
            t.s.shift("treasury", forge, "", 0, takings, "test: takings");
        if (const auto cash = t.s.cash(shop); cash > 0)
            t.s.shift(shop, "treasury", "", 0, cash, "test: no takings");
        t.until(d + 19. / 24);
    }
    int wageOf(const std::string& who) const
    {
        for (const auto& p : t.s.positions())
            if (const auto held = t.s.state().careers.positions.find(p.id);
                held != t.s.state().careers.positions.end() && held->second.holder == who)
                return t.s.payOf(p).wage;
        return -1;
    }
};
Wages wages(std::map<std::string, int> rules, std::int64_t takings, int wageHand = 8)
{
    rules["wageDrift"] = rules.count("wageDrift") ? rules["wageDrift"] : 1;
    rules["wageSupport"] = 0;
    rules["almsADay"] = 0;
    auto s = city({person("smith", "merchant", "smith at the Anvil", {"forge", 2.5, 2.5}, "home_smith", 40, 100),
                   person("hand", "civilian", "helping at the forge", {"forge", 4.5, 2.5}, "home_hand", 30, 100),
                   person("barber", "merchant", "barber at The Close Trim", {"shop", 2.5, 2.5}, "home_barber", 40, 100),
                   person("helper", "civilian", "helping at the barber's", {"shop", 4.5, 2.5}, "home_helper", 30, 100),
                   person("clerk", "civilian", "clerking at the Records Office", {"rolls", 2.5, 2.5}, "home_clerk", 30, 100)},
                  100000, rules);
    auto economy = s.authored().economy;
    economy.wageHand = wageHand;
    s.adoptEconomy(economy);
    return Wages{Town(std::move(s)), takings};
}

void wagesFollowCover()
{
    // Every two days: the forge pays every wage with plenty of profit (a penny up a post), the barber's shop never pays
    // (a penny down), the clerk the treasury's 8p throughout. The bounds 12p either way (from 6p: doc 68, Progress, "the
    // long runs"): the forge to 26p and 20p, the barber's keeper to the floor, 7p (14 less 7: the floor before the bound).
    auto w = wages({{"wageDriftDays", 2}}, 200);
    const auto money = w.t.s.moneySupply();
    const auto forge = w.t.s.tillOf("smith"), barbers = w.t.s.tillOf("barber");
    expect(w.wageOf("smith") == 14 && w.wageOf("hand") == 8 && w.wageOf("barber") == 14 && w.wageOf("helper") == 8, "as the world sets them");
    expect(w.t.s.wageAtBound(forge) == 0 && w.t.s.wageAtBound(barbers) == 0, "neither at a bound");
    for (int d = 1; d <= 30; ++d)
    {
        w.day(d);
        const int windows = d / 2;
        expect(w.wageOf("smith") == 14 + std::min(12, windows) && w.wageOf("hand") == 8 + std::min(12, windows),
               "day " + std::to_string(d) + ": the forge's a penny a window, at most 12: " + std::to_string(w.wageOf("smith")) + ", " +
                   std::to_string(w.wageOf("hand")));
        expect(w.wageOf("barber") == std::max(7, 14 - windows) && w.wageOf("helper") == std::max(7, 8 - windows),
               "day " + std::to_string(d) + ": the short shop's a penny down a window, never under 7p: " +
                   std::to_string(w.wageOf("barber")) + ", " + std::to_string(w.wageOf("helper")));
        expect(w.t.s.wageAtBound(forge) == (windows >= 12 ? 1 : 0) && w.t.s.wageAtBound(barbers) == (windows >= 7 ? -1 : 0),
               "day " + std::to_string(d) + ": at the top once at 12p, at the foot once the keeper is at the floor");
        expect(w.wageOf("clerk") == 8, "the treasury's post keeps its wage");
        expect(w.t.s.conserved() && w.t.s.moneySupply() == money, "conserved");
    }
    expect(w.t.s.state().loop.wages.at(barbers).step == -7, "the step stops where it no longer lowers a post: " +
                                                                std::to_string(w.t.s.state().loop.wages.at(barbers).step));
    // What the wages are is what is paid: the forge's 26p and 20p tonight.
    w.t.until(31 + 17.9 / 24);
    w.t.s.shift("treasury", forge, "", 0, 200, "test: takings");
    w.t.s.takeJournal();
    w.t.until(31 + 18.5 / 24);
    std::map<std::string, std::int64_t> paid;
    for (const auto& e : w.t.s.takeJournal())
        if (e.kind == "wages")
            paid[e.to] += e.coins;
    expect(paid["smith"] == 26 && paid["hand"] == 20, "paid at the drifted wage: " + std::to_string(paid["smith"]) + ", " +
                                                          std::to_string(paid["hand"]));

    // The hands' bound apart from the keeper's (the DM's wageDriftHandMost): at 3p, the forge's hand stops at 11p while
    // its keeper goes on to 26p; at the top only when both are.
    auto apart = wages({{"wageDriftDays", 2}, {"wageDriftHandMost", 3}}, 200);
    for (int d = 1; d <= 26; ++d)
    {
        apart.day(d);
        const int windows = d / 2;
        expect(apart.wageOf("smith") == 14 + std::min(12, windows) && apart.wageOf("hand") == 8 + std::min(3, windows),
               "day " + std::to_string(d) + ": the keeper within 12p, the hand within 3p: " + std::to_string(apart.wageOf("smith")) +
                   ", " + std::to_string(apart.wageOf("hand")));
        expect(apart.t.s.wageAtBound(apart.t.s.tillOf("smith")) == (windows >= 12 ? 1 : 0), "at the top only with both at theirs");
    }

    // In the band (every wage paid, nothing to spare): it stays.
    auto even = wages({{"wageDriftDays", 2}}, 22);
    {
        const auto forge = even.t.s.tillOf("smith");
        even.t.s.shift(forge, "treasury", "", 0, std::max<std::int64_t>(0, even.t.s.cash(forge) - Society::TillFloat), "test: a float only");
    }
    for (int d = 1; d <= 8; ++d)
        even.day(d);
    expect(even.wageOf("smith") == 14 && even.wageOf("hand") == 8, "paid, nothing to spare: no change: " + std::to_string(even.wageOf("smith")));

    // A wage the world set under the floor is never raised to it; the DM's narrower bound holds at once; off, the base.
    auto low = wages({{"wageDriftDays", 2}}, 0, 5);
    low.t.until(1 + 1. / 24);                   // (A society restores once it has begun its days.)
    auto saved = low.t.s.state();
    saved.loop.wages[low.t.s.tillOf("barber")].step = -3;
    saved.loop.wages[low.t.s.tillOf("smith")].step = 6;
    expect(low.t.s.restore(saved), "restored with steps");
    expect(low.wageOf("helper") == 5 && low.wageOf("barber") == 11, "under the floor already: kept at 5p; the keeper 14 - 3");
    auto economy = low.t.s.authored().economy;
    economy.rules["wageDriftMost"] = 2;
    low.t.s.adoptEconomy(economy);
    expect(low.wageOf("smith") == 16 && low.wageOf("barber") == 12 && low.wageOf("hand") == 11,
           "the keeper's bound narrowed: at most 2p either way; the hand's still 12p");
    economy.rules["wageDriftHandMost"] = 1;
    low.t.s.adoptEconomy(economy);
    expect(low.wageOf("hand") == 6 && low.wageOf("smith") == 16, "the hand's narrowed too: at most 1p");
    economy.rules["wageDrift"] = 0;
    low.t.s.adoptEconomy(economy);
    expect(low.wageOf("smith") == 14 && low.wageOf("hand") == 5 && low.wageOf("barber") == 14, "off: the world's wages, whatever was saved");
}

// A household of one with money and nothing to buy (ann), another with nothing (bob, whose purse goes to ann at 17:00
// each day: the alms make him up at 18:00), and a fishmonger with a till to draw from.
// (Judged a fortnight apart here, as the rule was first built, unless the test says: the steps the tests count.)
constexpr int Fortnight = 14;
Town treasury(std::int64_t treasury, std::map<std::string, int> rules)
{
    rules["treasuryDrift"] = 1;
    rules["treasuryDays"] = rules.count("treasuryDays") ? rules["treasuryDays"] : 1;
    rules["treasuryJudgeDays"] = rules.count("treasuryJudgeDays") ? rules["treasuryJudgeDays"] : Fortnight;
    rules["wageSupport"] = 0;
    // (The drift's bounds as they were before round 9 (doc 73: 2 points since, 1 since round 10; in tenths since round 11),
    // so its steps show.)
    rules["tollDriftTenths"] = rules.count("tollDriftTenths") ? rules["tollDriftTenths"] : 50;
    return Town(city({person("ann", "civilian", "keeps the house", {"home_ann", 2.5, 2.5}, "home_ann", 35, 5000),
                      person("bob", "civilian", "keeps the house", {"home_bob", 2.5, 2.5}, "home_bob", 35, 0)},
                     treasury, rules));
}
// (`drain`: pennies from the treasury to ann at 17:00, or, under 0, from ann to the treasury: its takings.)
void poorBob(Town& t, int day, std::int64_t drain = 0)
{
    t.until(day + 17. / 24);
    if (const auto cash = t.s.cash("bob"); cash > 0)
        t.s.shift("bob", "ann", "", 0, cash, "test: bob's purse");
    if (drain > 0)
        t.s.shift("treasury", "ann", "", 0, drain, "test: the treasury drained");
    else if (drain < 0)
        t.s.shift("ann", "treasury", "", 0, -drain, "test: the treasury's takings");
    t.until(day + 18.5 / 24);
}

void tollAndAlmsFollowTheTreasury()
{
    // Far over its target (50,000p against 1,000p, and rising: 100p a day in, bob's alms out): each fortnight the toll
    // eases 3 points (to 5 at most) and the alms rise a penny (to 2 at most); bob is made up to the alms of the day.
    {
        auto t = treasury(50000, {{"treasuryTarget", 1000}, {"tollStep", 30}});
        const auto money = t.s.moneySupply();
        for (int day = 1; day <= 45; ++day)
        {
            poorBob(t, day, -100);
            const int fortnights = (day - 1) / Fortnight;
            const double toll = 30 - std::min(5., 3. * fortnights);
            const int alms = 10 + std::min(2, fortnights);
            expect(near(t.s.gateTollNow(), toll) && t.s.almsNow() == alms,
                   "day " + std::to_string(day) + ": the toll " + str(t.s.gateTollNow()) + " (" + str(toll) + "), the alms " +
                       std::to_string(t.s.almsNow()) + " (" + std::to_string(alms) + ")");
            const int given = 10 + std::min(2, (day - 2) / Fortnight);
            expect(day == 1 || t.s.cash("bob") == given, "day " + std::to_string(day) + ": bob made up to " + std::to_string(given) +
                                                             ": " + std::to_string(t.s.cash("bob")));
        }
        expect(t.s.conserved() && t.s.moneySupply() == money, "conserved");
        // The eased toll on a draw: 25% of its cost to the treasury.
        auto u = Town(city({person("fish", "merchant", "fishmonger at the Fish Hall", {"hall", 2.5, 2.5}, "home_fish", 30, 0)}, 50000,
                           {{"treasuryDrift", 1}, {"treasuryDays", 1}, {"treasuryJudgeDays", Fortnight}, {"treasuryTarget", 1000},
                            {"tollStep", 50}, {"tollDriftTenths", 50}, {"wageSupport", 0}, {"almsADay", 0}}));
        u.until(16);
        expect(near(u.s.gateTollNow(), 25), "eased to 25%: " + str(u.s.gateTollNow()));
        u.s.shift("treasury", u.s.tillOf("fish"), "", 0, 1000, "test: a till to draw with");
        const auto toll0 = u.s.loopToday().toll, drawn0 = u.s.loopToday().drawn;
        expect(u.s.drawFromStore("fish", "fish", 40, "test: a draw").ok, "a draw");
        const auto toll = u.s.loopToday().toll - toll0, cost = u.s.loopToday().drawn - drawn0;
        expect(std::abs(double(toll) - .25 * double(cost)) <= 1, "a quarter of its cost to the treasury: " + std::to_string(toll) +
                                                                      " of " + std::to_string(cost));
    }
    // Over its target but falling (drained 500p a day): nothing moves, so a change has time to tell.
    {
        auto t = treasury(50000, {{"treasuryTarget", 1000}, {"tollStep", 30}});
        for (int day = 1; day <= 30; ++day)
            poorBob(t, day, 500);
        expect(near(t.s.gateTollNow(), 30) && t.s.almsNow() == 10, "over but falling: no change");
    }
    // Under its target and not rising: the toll firms; the alms tighten, but never under the world's (the line under
    // hunger): raised to 12 they come back down to 10 and no further.
    {
        auto t = treasury(3000, {{"treasuryTarget", 50000}, {"tollStep", 30}});
        const auto money = t.s.moneySupply();
        for (int day = 1; day <= 31; ++day)
            poorBob(t, day);
        expect(near(t.s.gateTollNow(), 35) && t.s.almsNow() == 10, "under: the toll to 35, the alms the world's 10: " +
                                                                        str(t.s.gateTollNow()) + ", " + std::to_string(t.s.almsNow()));
        auto raised = t.s.state();
        raised.loop.alms = 2;
        expect(t.s.restore(raised), "restored with the alms 2p over");
        const int judged = int(raised.loop.judged);
        for (int day = 32; day <= judged + 3 * Fortnight + 1; ++day)
        {
            poorBob(t, day);
            const int fortnights = (day - judged) / Fortnight;
            expect(t.s.almsNow() == std::max(10, 12 - fortnights) && near(t.s.gateTollNow(), 35),
                   "day " + std::to_string(day) + ": the alms tighten a penny a fortnight, to the world's: " + std::to_string(t.s.almsNow()));
        }
        expect(t.s.cash("bob") == 10, "bob made up to 10: " + std::to_string(t.s.cash("bob")));
        expect(t.s.conserved() && t.s.moneySupply() == money, "conserved");
        // Inside the band: no change; the slow mean.
        auto inside = treasury(3000, {{"treasuryTarget", 3000}, {"tollStep", 30}, {"treasuryDays", 120}});
        for (int day = 1; day <= 31; ++day)
            poorBob(inside, day);
        expect(near(inside.s.gateTollNow(), 30) && inside.s.almsNow() == 10, "inside the band: no change");
        // Off: the world's, whatever was saved.
        auto economy = t.s.authored().economy;
        economy.rules["treasuryDrift"] = 0;
        t.s.adoptEconomy(economy);
        expect(near(t.s.gateTollNow(), 30) && t.s.almsNow() == 10, "off: the world's toll and alms");
        expect(near(t.s.state().loop.toll, 5), "(kept for when it is on again)");
    }
    // The defaults since doc 68's "Births follow deaths; arrivals from the Country; the treasury's target": judged every
    // 28 days, not 14 (so the half year's mean answers a step before the next), against 5,500p (about where the treasury
    // settles): a treasury at 6,000p is inside the band (4,675p to 6,325p) and nothing moves, where against 5,000p (its
    // top 5,750p) it eased; one at 7,000p eases half a point and raises the alms a penny each 28 days, and only then.
    {
        auto settled = treasury(6000, {{"treasuryJudgeDays", 28}, {"treasuryTarget", 5500}});
        for (int day = 1; day <= 60; ++day)
            poorBob(settled, day, -10);
        expect(near(settled.s.gateTollNow(), 30) && settled.s.almsNow() == 10, "6,000p against 5,500p: inside the band, no change");
        auto before = treasury(6000, {{"treasuryJudgeDays", 28}, {"treasuryTarget", 5000}});
        for (int day = 1; day <= 60; ++day)
            poorBob(before, day, -10);
        expect(before.s.gateTollNow() < 30 - .1, "(against the old 5,000p it eased: " + str(before.s.gateTollNow()) + ")");
        auto over = treasury(7000, {{"treasuryJudgeDays", 28}, {"treasuryTarget", 5500}});
        const auto money = over.s.moneySupply();
        for (int day = 1; day <= 90; ++day)
        {
            poorBob(over, day, -30);
            const int steps = (day - 1) / 28;
            expect(near(over.s.gateTollNow(), 30 - .5 * steps) && over.s.almsNow() == 10 + std::min(2, steps),
                   "day " + std::to_string(day) + ": a step each 28 days: the toll " + str(over.s.gateTollNow()) + ", the alms " +
                       std::to_string(over.s.almsNow()));
        }
        expect(over.s.conserved() && over.s.moneySupply() == money, "conserved");
        // The DM's: a world's own judging period, at once.
        auto economy = over.s.authored().economy;
        economy.rules["treasuryJudgeDays"] = 7;
        over.s.adoptEconomy(economy);
        expect(over.s.rule("treasuryJudgeDays") == 7, "the DM may judge it more often");
    }
}

void itSurvivesASave()
{
    // A society with all four on and something drifted in each: saved (the society's save, as the game's checkpoint
    // writes it), read back and restored into a new one, every value the same.
    auto w = wages({{"wageDriftDays", 2}, {"stockPrices", 1}, {"shelfFollow", 1}, {"treasuryDrift", 1}, {"treasuryTarget", 1000000},
                    {"treasuryDays", 1}, {"treasuryJudgeDays", Fortnight}, {"tollStep", 30}},
                   200);
    for (int d = 1; d <= 16; ++d)
        w.day(d);
    // (Some sales for the forge's shelves and prices.)
    const auto forge = w.t.s.tillOf("smith");
    w.t.until(17 + 20. / 24);
    w.t.s.create(forge, "nails", 30, "test: nails");
    expect(w.t.s.sale(forge, "treasury", "nails", 30, Society::catalogPrice("nails"), "test: sold"), "nails sold");
    w.t.until(18 + 1. / 24);
    const auto& loop = w.t.s.state().loop;
    expect(!loop.wages.empty() && !loop.sales.empty() && loop.level > 0 && loop.toll != 0,
           "something drifted in each");
    const auto saved = json::dump(wire::society(w.t.s.state()));
    json::Value parsed;
    std::string error;
    expect(json::parse(saved, parsed, error), "the save parses: " + error);
    const auto read = wire::readSociety(parsed);
    expect(read.minted >= 0, "the save reads");
    auto fresh = wages({{"wageDriftDays", 2}, {"stockPrices", 1}, {"shelfFollow", 1}, {"treasuryDrift", 1}, {"treasuryTarget", 1000000},
                        {"treasuryDays", 1}, {"treasuryJudgeDays", Fortnight}, {"tollStep", 30}},
                       200);
    expect(fresh.t.s.restore(read), "and restores");
    expect(json::dump(wire::society(fresh.t.s.state())) == saved, "the same save again");
    for (const char* who : {"smith", "hand", "barber", "helper", "clerk"})
        expect(fresh.wageOf(who) == w.wageOf(who), std::string("the same wage: ") + who);
    expect(near(fresh.t.s.gateTollNow(), w.t.s.gateTollNow()) && fresh.t.s.almsNow() == w.t.s.almsNow(), "the same toll and alms");
    expect(fresh.t.s.shelfOf(forge, "nails") == w.t.s.shelfOf(forge, "nails") && fresh.t.s.shelfOf(forge, "nails") > Society::GoodsKept,
           "the same shelf: " + std::to_string(fresh.t.s.shelfOf(forge, "nails")));
    // An older save (no "loop") reads as nothing drifted; a value out of its range is dropped, the rest kept.
    auto older = json::Value::object();
    for (const auto& [key, v] : parsed.fields())
        if (key != "loop")
            older.add(key, v);
    const auto old = wire::readSociety(older);
    expect(old.minted >= 0 && old.loop.empty(), "a save from before: nothing drifted");
    auto bad = json::Value::object();
    for (const auto& [key, v] : parsed.fields())
        if (key != "loop")
            bad.add(key, v);
    auto loopJson = json::Value::object();
    for (const auto& [key, v] : parsed["loop"].fields())
        loopJson.add(key, key == "toll" ? json::Value(500.) : v);
    bad.add("loop", loopJson);
    const auto dropped = wire::readSociety(bad);
    expect(dropped.minted >= 0 && dropped.loop.toll == 0 && dropped.loop.wages == read.loop.wages,
           "a toll out of range dropped, the wages kept");
    // All four off: nothing is kept at all (the save as before).
    auto none = wages({{"wageDrift", 0}}, 200);
    for (int d = 1; d <= 4; ++d)
        none.day(d);
    expect(none.t.s.state().loop.empty() && !wire::society(none.t.s.state()).has("loop"), "all four off: no loop in the save");
}
} // namespace

namespace ratw
{
bool operator==(const LoopState::Wages& a, const LoopState::Wages& b)
{
    return a.step == b.step && a.days == b.days && a.shortDays == b.shortDays && a.owed == b.owed && a.profit == b.profit;
}
} // namespace ratw

int main()
{
    try
    {
        catalogue();
        theDefaults();
        pricesFollowStock();
        residentsPayTheShopsPrice();
        playersPayTheShopsPrice();
        shelvesFollowSales();
        wagesFollowCover();
        tollAndAlmsFollowTheTreasury();
        itSurvivesASave();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Self-correcting rules tests passed: " << checks << " checks.\n";
    return 0;
}
