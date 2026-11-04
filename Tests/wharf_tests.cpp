// The Wharf and the city's loop (Docs/Design/73-the-wharf.md; the Country before it, Docs/Design/68-the-country-and-the-
// city-loop.md): every flow conserved (store draws split between the Wharf and the treasury's gate toll, carriage to the
// carters with the treasury's subsidy, the estates' purchases and their carriage, households' food, needs and comforts,
// wages from tills, the treasury and the estates, a till's profit); who pays which post; the estates spreading their
// buying over the makers; the subsidy's cap and its giving way before public wages; residents fed; the per-good switch;
// the loop's records; an older save's fund:store and outside:country; the Wharf's goods landed beyond the realm, an order
// from it by wagon, its credit moved by the DM. (The farms' side of the Wharf, its imports after the farms and its takes
// of their piles, is farms_tests' theWharf.)
//
// The catalogue is Data/Items with a carters' business added (the other session adds the real one: "carters"), copied
// to a scratch directory read through RATW_DATA_DIR, so a carters' yard can take carriage.
#include "RatwCalendar.h"
#include "RatwGame.h"
#include "RatwItems.h"
#include "RatwJsonDoc.h"
#include "RatwSociety.h"
#include "RatwWire.h"
#include "RatwWorld.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

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

// Data/Items and Data/Economy, with a "carters" business first in businesses.json.
void catalogueWithCarters()
{
    const fs::path source = fs::path(RATW_SOURCE_DIR) / "Data";
    const fs::path dir = fs::temp_directory_path() / ("ratw_country_tests_" + std::to_string(::getpid()));
    fs::remove_all(dir);
    fs::create_directories(dir / "Items");
    fs::create_directories(dir / "Economy");
    for (const auto& e : fs::directory_iterator(source / "Items"))
        if (e.is_regular_file())
            fs::copy_file(e.path(), dir / "Items" / e.path().filename());
    fs::copy_file(source / "Economy" / "ladder.json", dir / "Economy" / "ladder.json");
    std::ifstream in(dir / "Items" / "businesses.json");
    std::stringstream text;
    text << in.rdbuf();
    auto body = text.str();
    const auto at = body.find("\"businesses\"");
    const auto open = body.find('[', at);
    body.insert(open + 1, "\n    {\"id\": \"carters\", \"label\": \"Carters' yard\", \"trades\": [], \"stations\": [], \"kind\": \"yard\", "
                          "\"tier\": \"city\", \"sells\": [], \"match\": [\"carters' yard\"]},");
    std::ofstream(dir / "Items" / "businesses.json") << body;
    setenv("RATW_DATA_DIR", dir.string().c_str(), 1);
    std::string error;
    expect(items::load(&error), "the catalogue loads: " + error);
    expect(items::businessFor("running the Carters' Yard") && items::businessFor("running the Carters' Yard")->id == "carters",
           "the carters' business is known");
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

// A small city: two smiths (each with a hand), a baker, a fishmonger, the carters' yard and a driver, the watch, a clerk,
// a docker, a child, and someone retired.
AuthoredRoster roster(bool carters = true)
{
    AuthoredRoster r;
    r.residents = {person("smith", "merchant", "the smith at the forge", {"forge", 2.5, 2.5}, "home_smith"),
                   person("hand", "civilian", "helping at the forge", {"forge", 4.5, 2.5}, "home_smith"),
                   person("smith2", "merchant", "the smith at the anchor forge", {"forge2", 2.5, 2.5}, "home_two"),
                   person("hand2", "civilian", "helping at the anchor forge", {"forge2", 4.5, 2.5}, "home_two"),
                   person("baker", "merchant", "baker at The Loaf", {"bakery", 2.5, 2.5}, "home_baker"),
                   person("fish", "merchant", "fishmonger at the Fish Hall", {"fishhall", 2.5, 2.5}, "home_fish"),
                   person("guard", "guard", "keeping the watch", {"gate", 2.5, 2.5}, "watch_house_barracks"),
                   person("clerk", "civilian", "clerking at the City Office", {"office", 2.5, 2.5}, "home_clerk"),
                   person("child", "civilian", "at lessons", {"school", 2.5, 2.5}, "home_clerk", 8, 0),
                   person("upstairs", "civilian", "at lessons", {"school", 4.5, 2.5}, "home_clerk_up", 12, 0),
                   person("orphan", "civilian", "at lessons", {"school", 6.5, 2.5}, "home_orphan", 10, 0),
                   person("docker", "civilian", "unloading ships at the piers", {"piers", 2.5, 2.5}, "home_docker"),
                   person("elder", "civilian", "sits and watches the world", {"square", 2.5, 2.5}, "home_docker", 70, 10)};
    if (carters)
    {
        r.residents.push_back(person("carter", "merchant", "running the Carters' Yard", {"yard", 2.5, 2.5}, "home_carter"));
        r.residents.push_back(person("driver", "civilian", "driving for the Carters' Yard", {"yard", 4.5, 2.5}, "home_carter"));
    }
    r.kin = {{"clerk", "child", "parent"}, {"clerk", "upstairs", "parent"}};   // (One child in a room of its own upstairs.)
    r.economy.treasury = 10000;
    r.economy.storeRule = true;
    // The loop as doc 68 first built it: the later rules as they were (trades_tests, treasury_tests and selfcorrect_tests
    // have theirs; doc 68, "Trades", "The treasury's surplus", "Self-correcting rules").
    // (And the Wharf's valve shut, doc 73: the Country's flows as they were; the Wharf's own tests open it.)
    r.economy.rules = {{"subsidyToNeed", 0}, {"floatDrawDays", 1}, {"buyAheadDays", 0}, {"patrons", 0}, {"suppliers", 0}, {"surplus", 0},
                       {"stockPrices", 0}, {"shelfFollow", 0}, {"wageDrift", 0}, {"treasuryDrift", 0}, {"wharfValve", 0}};
    return r;
}

Society city(const std::function<void(AuthoredRoster&)>& change = {}, bool carters = true)
{
    auto r = roster(carters);
    if (change)
        change(r);
    Society s(Roster::None);
    s.configure(r);
    LifeDay day;
    day.communityOf = [](const std::string&) { return std::string("city"); };
    s.setDay(day);
    // The families as authored, taken in at the day's turn as the world does it.
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

const Position& post(const Society& s, const std::string& founder)
{
    const auto* p = s.position("job:" + founder);
    if (!p)
        throw std::runtime_error("no post for " + founder);
    return *p;
}

void whoPays()
{
    auto s = city();
    const auto pay = [&](const std::string& id) { return s.payOf(post(s, id)); };
    expect(pay("smith").kind == "keeper" && pay("smith").payer == "till:job:smith" && pay("smith").wage == 14, "a keeper: its till, 14p");
    expect(pay("hand").kind == "hand" && pay("hand").payer == "till:job:smith" && pay("hand").wage == 8, "a hand in its shop's room: the till");
    expect(pay("driver").kind == "hand" && pay("driver").payer == "till:job:carter", "a driver: the carters' till");
    expect(pay("guard").kind == "watch" && pay("guard").payer == "treasury", "the watch: the treasury");
    expect(pay("clerk").kind == "public" && pay("clerk").payer == "treasury", "a clerk: the treasury (city_loop.json's rules)");
    expect(pay("docker").kind == "estate" && pay("docker").payer == Society::WharfFund, "a docker: the estates' (the Wharf's purse)");
    expect(pay("child").kind.empty(), "a child at lessons: nobody");
    expect(pay("elder").kind.empty(), "the old who sit and watch: nobody (the alms keep them if their household runs short)");
    expect(s.crewOf("till:job:smith") == 2 && s.crewOf("till:job:baker") == 1, "a crew: its keeper and hands");
    expect(s.carters() == "till:job:carter", "carriage goes to the carters' yard");
    expect(city({}, false).carters() == Society::CarriageFund, "and without one, to fund:carriage");
}

void aDrawSplit()
{
    auto s = city();
    const auto till = s.tillOf("smith");
    s.shift("treasury", till, "", 0, 5000, "test: working money");
    const auto t0 = s.cash(till), c0 = s.cash(Society::WharfFund), r0 = s.cash("treasury"), k0 = s.cash("till:job:carter");
    // 400 iron bars: 400 × 9p at 100%; carriage on 2,000 weight at 6p a load, 12p, of which the treasury pays a quarter.
    const auto cost = s.storeCost("smith", "iron_bar", 400);
    const auto drawn = s.drawFromStore("smith", "iron_bar", 400);
    expect(drawn.ok, "the draw: " + drawn.message);
    const auto toll = cost * 30 / 100;
    expect(s.cash(Society::WharfFund) - c0 == cost - toll, "the Country takes the cost less the toll");
    expect(s.cash("treasury") - r0 == toll - 3, "the treasury the toll, less its 3p of the carriage");
    expect(s.cash("till:job:carter") - k0 == 12, "the carters the carriage, 12p");
    expect(t0 - s.cash(till) == cost + 9, "the till the cost and 9p of the carriage");
    expect(s.conserved(), "conserved");
    // A good the Wharf no longer lands is refused, and nothing moves.
    auto off = city([](AuthoredRoster& r) { r.economy.wharfOff = {"iron_bar"}; });
    const auto offTill = off.tillOf("smith");
    const auto cash = off.cash(offTill);
    const auto refused = off.drawFromStore("smith", "iron_bar", 1);
    expect(!refused.ok && refused.message.find("no longer lands") != std::string::npos && off.cash(offTill) == cash,
           "a good the Wharf no longer lands is refused: " + refused.message);
    expect(off.drawFromStore("smith", "charcoal", 1).ok, "others still come");
    // Without a carters' yard, carriage goes to fund:carriage: still conserved.
    auto none = city({}, false);
    none.shift("treasury", none.tillOf("smith"), "", 0, 5000, "test: working money");
    expect(none.drawFromStore("smith", "iron_bar", 400).ok && none.cash(Society::CarriageFund) == 12 && none.conserved(),
           "carriage to fund:carriage where there is no yard");
    // Small draws: the toll's and carriage's fractions are kept until they make a penny, so many small draws pay what
    // one large one does.
    auto small = city();
    const auto smallTill = small.tillOf("smith");
    small.shift("treasury", smallTill, "", 0, 5000, "test: working money");
    const auto r1 = small.cash("treasury");
    for (int i = 0; i < 100; ++i)
        expect(small.drawFromStore("smith", "iron_bar", 4).ok, "a small draw");
    expect(small.cash("treasury") - r1 >= toll - 4 && small.conserved(), "small draws pay the toll in full, near enough");
}

void theSubsidyGivesWayFirst()
{
    // The treasury's room: at most a twentieth of its money a day, and never into its float and tonight's public wages.
    auto poor = city([](AuthoredRoster& r) { r.economy.treasury = 0; });
    const auto till = poor.tillOf("smith");
    poor.shift(till, "treasury", "", 0, 0, "test");
    auto& state = const_cast<SocietyState&>(poor.state());
    (void)state;
    expect(poor.subsidyRoom() == 0, "a treasury with nothing over its float gives no subsidy");
    poor.shift(poor.tillOf("baker"), till, "", 0, 0, "test");
    // The till pays all the carriage then (12p on 400 bars).
    auto& accounts = const_cast<SocietyState&>(poor.state()).accounts;
    accounts.at(till).cash += 5000;                          // (Working money: made, as a mint would; minted kept equal.)
    const_cast<SocietyState&>(poor.state()).minted += 5000;
    const auto t0 = poor.cash(till);
    const auto cost = poor.storeCost("smith", "iron_bar", 400);
    expect(poor.drawFromStore("smith", "iron_bar", 400).ok, "a draw with no subsidy");
    expect(t0 - poor.cash(till) == cost + 12, "the till pays the whole carriage");
    expect(poor.conserved(), "conserved");
    // A rich treasury: a twentieth of it a day, then no more.
    auto rich = city([](AuthoredRoster& r) {
        r.economy.treasury = 1000;
        r.economy.carriage = 1000;                           // (Dear carriage, so the cap is reached quickly.)
    });
    const auto room = rich.subsidyRoom();
    expect(room == 50, "a twentieth of 1,000p: " + std::to_string(room));
    const auto richTill = rich.tillOf("smith");
    const_cast<SocietyState&>(rich.state()).accounts.at(richTill).cash += 100000;
    const_cast<SocietyState&>(rich.state()).minted += 100000;
    std::int64_t given = 0;
    for (int i = 0; i < 20; ++i)
    {
        const auto r0 = rich.cash("treasury"), k0 = rich.cash("till:job:carter");
        expect(rich.drawFromStore("smith", "iron_bar", 40).ok, "a dear draw");
        // (The toll's coming in: what the treasury gave is the carters' takings less what the till paid.)
        given += (rich.cash("till:job:carter") - k0) - 0;
        (void)r0;
    }
    expect(rich.subsidyRoom() <= 50 && rich.subsidyRoom() >= 0, "the room never grows past its cap in a day");
    expect(rich.loopToday().subsidy <= 50, "at most a twentieth of the treasury given in a day: " + std::to_string(rich.loopToday().subsidy));
    expect(rich.conserved(), "conserved");
    // Public wages are owed first: a treasury just over its float and its wages gives nothing.
    auto tight = city([](AuthoredRoster& r) { r.economy.treasury = Society::TreasuryFloat + 16; });   // (Two public posts at 8p.)
    expect(tight.subsidyRoom() == 0, "nothing over the float and tonight's wages: no subsidy");
}

// Runs the society from `from` to `to` (absolute days), a second at a time, everyone at work.
void run(Society& s, double from, double to)
{
    const auto bodies = atWork(s);
    for (double now = from; now < to; now += 1. / calendar::SecondsPerDay)
        s.tick(1, now, 0, bodies);
}

void aDayOfTheLoop()
{
    auto s = city([](AuthoredRoster& r) { r.economy.wharfReserve = 0; });   // (A small city's Country takes in little.)
    const auto money = s.moneySupply();
    std::map<std::string, std::int64_t> before;
    for (const auto& r : s.authored().residents)
        before[r.id] = s.cash(r.id);
    run(s, 1 + 7.9 / 24, 2 + 6. / 24);                       // From before the shops open to the next morning.
    expect(s.conserved() && s.moneySupply() == money, "a day of the loop: money conserved");
    const auto& d = s.loopYesterday();
    expect(d.foodBought > 0 && d.residentsSpent > 0, "households bought food: " + std::to_string(d.foodBought));
    expect(d.wagesPaid > 0 && d.wagesPaid <= d.wagesOwed, "wages were paid: " + std::to_string(d.wagesPaid) + " of " + std::to_string(d.wagesOwed));
    expect(d.drawn > 0 && d.toll > 0, "shops restocked from the Country and paid the toll");
    // Everyone has eaten or holds food: the child is fed by the household.
    for (const auto& r : s.authored().residents)
    {
        const auto* life = s.resident(r.id);
        expect(life && (life->hunger < 80 || Society::hasFood(*s.account(r.id))), r.id + " is fed (hunger " + std::to_string(life->hunger) + ")");
    }
    expect(Society::hasFood(*s.account("child")) || s.resident("child")->hunger < 60, "the child was fed though it holds no money");
    // Wages: the clerk from the treasury, the docker from the Country, the hand from the smith's till.
    const auto journal = s.takeJournal();
    // (The children's pennies, if any, are the household's alms, paid to its poorest member: neither paid for anything.)
    const auto spent = [&](const std::string& who) {
        for (const auto& e : journal)
            if (e.from == who && e.coins > 0)
                return true;
        return false;
    };
    expect(!spent("child"), "and paid nothing");
    expect((Society::hasFood(*s.account("upstairs")) || s.resident("upstairs")->hunger < 60) && !spent("upstairs"),
           "a child living upstairs, apart from its parent, is fed by the family too");
    const auto paid = [&](const std::string& from, const std::string& to) {
        for (const auto& e : journal)
            if (e.kind == "wages" && e.from == from && e.to == to && e.coins > 0)
                return true;
        return false;
    };
    expect(paid("treasury", "clerk") && paid("treasury", "guard"), "the treasury paid the clerk and the watch");
    expect(paid(Society::WharfFund, "docker"), "the Country paid the docker");
    bool alms = false;
    for (const auto& e : journal)
        alms = alms || (e.kind == "alms" && e.from == "treasury" && e.to == "orphan" && e.coins > 0);
    expect(alms, "the city's alms keep a child whose household runs short");
    expect(Society::hasFood(*s.account("orphan")) || s.resident("orphan")->hunger < 60 || s.cash("orphan") >= s.rule("almsADay"),
           "and it eats, or holds a day's food money for tomorrow");
    expect(paid("till:job:smith", "hand") && paid("till:job:smith", "smith"), "the smithy paid its hand and keeper");
    // The Country spent on the city's goods, spread over both smithies (never the first in a list).
    std::map<std::string, int> sold;
    for (const auto& e : journal)
        if (e.kind == "to the Wharf")
            sold[e.to] += e.quantity;
    expect(s.loopYesterday().basketSpent > 0, "the Country bought the city's goods");
    expect(sold["till:job:smith"] > 0 && sold["till:job:smith2"] > 0,
           "both smithies sold to it: " + std::to_string(sold["till:job:smith"]) + " and " + std::to_string(sold["till:job:smith2"]));
    for (const auto& e : journal)
        expect(e.kind != "to the Wharf" || e.to != "till:job:fish" || e.item == "salt_fish" || e.item == "smoked_fish",
               "it buys the fishmonger's own make (salt and smoked fish), never its resale: " + e.item);
    // The Country's purse stays bounded: it keeps its reserve and spends the rest.
    expect(s.cash(Society::WharfFund) <= 8 + 2 * (s.loopYesterday().drawn + 100), "the Country's purse is bounded");
}

void itsPostsFirst()
{
    // Doc 72, Phase 3 (country2: the Country missed its posts on 6 days, its purse down to 16p after two lean days): it
    // keeps estatePostsDays (2) nights of its posts' wages in its purse, over its reserve, and its basket buys only with
    // what is over; so its posts are paid every night, a lean day or two included.
    const auto spentWith = [](int nights, std::int64_t& left, std::int64_t& owed) {
        auto s = city([&](AuthoredRoster& r) {
            r.economy.wharfReserve = 0;
            r.economy.rules["estatePostsDays"] = nights;
        });
        owed = s.payOf(post(s, "docker")).wage;
        // Its purse set to three nights of its posts and 60p more.
        expect((s.cash(Society::WharfFund) == 0 || s.operatorTransfer(Society::WharfFund, "treasury", "", 0, s.cash(Society::WharfFund)).ok) &&
                   s.operatorTransfer("treasury", Society::WharfFund, "", 0, 3 * owed + 60).ok,
               "the Country's purse set");
        run(s, 1 + 7.9 / 24, 1 + 17.95 / 24);       // (The shop hours to just before the wages.)
        left = s.cash(Society::WharfFund);
        return s.loopToday().basketSpent;
    };
    expect(city().rule("estatePostsDays") == 2, "two nights by default (city_loop.json)");
    std::int64_t left1 = 0, left2 = 0, owed = 0;
    const auto spent1 = spentWith(1, left1, owed), spent2 = spentWith(2, left2, owed);
    expect(owed > 0 && left2 >= 2 * owed, "two nights kept before the wages: " + std::to_string(left2) + " of " + std::to_string(2 * owed));
    expect(left1 >= owed && spent1 > spent2, "one night kept (as it was): it spends more: " + std::to_string(spent1) + " and " +
                                                  std::to_string(spent2));
    // Tonight's wages paid in full, and tomorrow's still in its purse after them: a day that takes in nothing can't leave
    // its posts unpaid.
    auto s = city([&](AuthoredRoster& r) { r.economy.wharfReserve = 0; });
    expect((s.cash(Society::WharfFund) == 0 || s.operatorTransfer(Society::WharfFund, "treasury", "", 0, s.cash(Society::WharfFund)).ok) &&
               s.operatorTransfer("treasury", Society::WharfFund, "", 0, 3 * owed + 60).ok,
           "the Country's purse set");
    run(s, 1 + 7.9 / 24, 1 + 18.5 / 24);
    expect(s.loopToday().estatePostsOwed == owed && s.loopToday().estatePostsPaid == owed, "tonight's posts paid in full");
    expect(s.cash(Society::WharfFund) >= owed, "and the next night's kept: " + std::to_string(s.cash(Society::WharfFund)));
    expect(s.conserved(), "conserved");
}

// A purse set to exactly `coins`, to or from the treasury (nothing made).
void setPurse(Society& s, const std::string& fund, std::int64_t coins)
{
    const auto now = s.cash(fund);
    const bool ok = now == coins || (now > coins ? s.operatorTransfer(fund, "treasury", "", 0, now - coins).ok
                                                 : s.operatorTransfer("treasury", fund, "", 0, coins - now).ok);
    expect(ok && s.cash(fund) == coins, fund + " set to " + std::to_string(coins));
}

// The city with a post of each House's people beside the docker (the wharf's): Greatly's steward, a servant in Fellhall's
// hall, a Minor House's valet (no land: the Crown's), and a tally in the Iron Store (Timberland's, its cell named for
// Grayrock as it was). Each a Country post at its wage (8p).
void withHouses(AuthoredRoster& r)
{
    r.economy.wharfReserve = 0;
    for (auto p : {person("steward", "civilian", "serving at Greatly House", {"rm_greatly_house", 2.5, 2.5}, "home_steward"),
                   person("maid", "civilian", "serving in the hall", {"rm_fellhall", 2.5, 2.5}, "home_maid"),
                   person("valet", "civilian", "in service to the Carlisles", {"rm_carlisle_house", 2.5, 2.5}, "home_valet"),
                   person("tally", "civilian", "tallying in the Iron Store", {"rm_the_grayrock_iron_store", 2.5, 2.5}, "home_tally")})
        r.residents.push_back(p);
}

std::vector<EconomyEntry> wagesTonight(Society& s, std::int64_t day)
{
    run(s, double(day) + 17.99 / 24, double(day) + 18.5 / 24);
    return s.takeJournal();
}

bool paidBy(const std::vector<EconomyEntry>& journal, const std::string& from, const std::string& to, std::int64_t coins)
{
    for (const auto& e : journal)
        if (e.kind == "wages" && e.from == from && e.to == to && e.coins == coins)
            return true;
    return false;
}

void theHousesEstates()
{
    // Doc 72, Phase 6: each House its own estate purse, paying its own people; the Country's (Grayrock's: the wharf)
    // keeps the sea trade's.
    auto s = city(withHouses);
    const auto estateOf = [&](const std::string& id) {
        const auto* e = s.estateOfPost(post(s, id));
        return e ? e->id : std::string("-");
    };
    expect(estateOf("docker") == "grayrock" && estateOf("steward") == "greatly" && estateOf("maid") == "fell" &&
               estateOf("valet") == "crown" && estateOf("tally") == "timberland",
           "each post its House's estate: by its label, then its workplace; the Minor Houses' the Crown's");
    expect(Society::estateOfHouse("the Crown")->id == "crown" && Society::estateOfHouse("none")->grants &&
               Society::estateFund(*Society::estateOfHouse("Grayrock")) == Society::WharfFund &&
               Society::estateFund(*Society::estateOfHouse("Greatly")) == "estate:greatly",
           "the Houses' land: Grayrock's estate is the Country's purse (the wharf), land no estate names the Crown's");
    expect(s.estatesOn() && s.payOf(post(s, "steward")).payer == Society::WharfFund && s.houseFund("Greatly") == Society::WharfFund &&
               !s.account("estate:greatly"),
           "on by default; until the first night every post and rent the Country's purse");
    // The first night: the purses opened, the Country's money shared by the posts' wages (five posts at 8p: a fifth each).
    run(s, 1 + 7.9 / 24, 1 + 17.99 / 24);
    s.takeJournal();
    setPurse(s, Society::WharfFund, 1000);
    const auto money = s.moneySupply();
    auto journal = wagesTonight(s, 1);
    for (const auto* id : {"estate:greatly", "estate:fell", "estate:timberland", "estate:crown"})
        expect(s.account(id) && s.cash(id) == 200 - 8, std::string(id) + ": a fifth of the Country's money, less its post: " + std::to_string(s.cash(id)));
    expect(s.cash(Society::WharfFund) >= 200 - 8, "the Country (the wharf) keeps its fifth (and its sales since): " + std::to_string(s.cash(Society::WharfFund)));
    expect(paidBy(journal, Society::WharfFund, "docker", 8) && paidBy(journal, "estate:greatly", "steward", 8) &&
               paidBy(journal, "estate:fell", "maid", 8) && paidBy(journal, "estate:crown", "valet", 8) &&
               paidBy(journal, "estate:timberland", "tally", 8),
           "each estate paid its own people");
    const auto& night = s.loopToday();
    expect(night.estates.at("greatly").postsPaid == 8 && night.estates.at("greatly").moved == 200 && night.estates.at("grayrock").moved == -800 &&
               night.estatePostsOwed == 40 && night.estatePostsPaid == 40,
           "booked by estate, and the Country's posts in all");
    expect(s.payOf(post(s, "steward")).payer == "estate:greatly" && s.houseFund("Greatly") == "estate:greatly" &&
               s.houseFund("Grayrock") == Society::WharfFund && s.houseFund("none") == "estate:crown",
           "now the House's own purse");
    expect(s.conserved() && s.moneySupply() == money, "conserved");

    // The Crown's grants: Greatly 3p (5p short), the wharf none (8p short), Fell its 8p, the Crown 100p: it gives each what
    // it lacks of tonight's posts, recorded; every post paid.
    run(s, 2 + 7.9 / 24, 2 + 17.99 / 24);
    s.takeJournal();
    for (const auto& [fund, coins] : std::map<std::string, std::int64_t>{
             {"estate:greatly", 3}, {Society::WharfFund, 0}, {"estate:fell", 8}, {"estate:timberland", 100}, {"estate:crown", 100}})
        setPurse(s, fund, coins);
    journal = wagesTonight(s, 2);
    const auto& two = s.loopToday();
    expect(two.estates.at("greatly").grantsIn == 5 && two.estates.at("grayrock").grantsIn == 8 && two.estates.at("crown").grantsOut == 13 &&
               two.estates.at("fell").grantsIn == 0,
           "the Crown granted Greatly 5p and the wharf 8p, Fell nothing");
    expect(two.estatePostsPaid == 40 && paidBy(journal, "estate:greatly", "steward", 8) && paidBy(journal, Society::WharfFund, "docker", 8),
           "every post paid in full");
    expect(s.cash("estate:crown") == 100 - 13 - 8 && s.cash("estate:timberland") == 92, "the Crown's own post first; Timberland's own");
    bool recorded = false;
    for (const auto& e : journal)
        recorded = recorded || (e.from == "estate:crown" && e.to == "estate:greatly" && e.coins == 5 &&
                                e.kind.rfind("a grant from the Crown's levy", 0) == 0);
    expect(recorded && s.conserved(), "the grant in the journal; conserved");

    // The Crown short too: 10p, its own 8p first, the 2p over shared by what each lacks (8p each: 1p each).
    run(s, 3 + 7.9 / 24, 3 + 17.99 / 24);
    s.takeJournal();
    for (const auto& [fund, coins] : std::map<std::string, std::int64_t>{
             {"estate:greatly", 0}, {Society::WharfFund, 0}, {"estate:fell", 8}, {"estate:timberland", 8}, {"estate:crown", 10}})
        setPurse(s, fund, coins);
    journal = wagesTonight(s, 3);
    expect(paidBy(journal, "estate:crown", "valet", 8) && s.loopToday().estates.at("greatly").grantsIn == 1 &&
               s.loopToday().estates.at("grayrock").grantsIn == 1 && s.loopToday().estatePostsPaid == 8 * 3 + 1 + 1,
           "the Crown's own post paid; the rest shared; Greatly and the wharf paid a penny each");
    expect(s.conserved(), "conserved");

    // Its posts first: an estate buys (the basket, its tables) only with what is over estatePostsDays nights of its posts.
    const auto bought = [&](std::int64_t purse) {
        run(s, 4 + 7.9 / 24, 4 + 7.99 / 24);
        setPurse(s, "estate:greatly", purse);
        run(s, 4 + 7.99 / 24, 4 + 17.99 / 24);
        const auto found = s.loopToday().estates.find("greatly");
        return std::pair{found == s.loopToday().estates.end() ? std::int64_t(0) : found->second.spent, s.cash("estate:greatly")};
    };
    const auto [none, kept] = bought(16);
    expect(none == 0 && kept == 16, "two nights of its post: it buys nothing");
    auto t = city(withHouses);
    run(t, 1 + 7.9 / 24, 1 + 18.5 / 24);            // (The purses opened.)
    run(t, 1 + 18.5 / 24, 2 + 7.99 / 24);
    setPurse(t, "estate:greatly", 400);
    run(t, 2 + 7.99 / 24, 2 + 17.99 / 24);
    expect(t.loopToday().estates.count("greatly") && t.loopToday().estates.at("greatly").spent > 0 && t.cash("estate:greatly") >= 16,
           "with more, it buys, keeping its two nights: " + std::to_string(t.cash("estate:greatly")));
    expect(t.conserved(), "conserved");
}

void theEstatesOff()
{
    // The rule off: today's single purse (no estate opened, every post the Country's); switched off after a split, every
    // estate's money back in the Country's purse that night.
    auto s = city([](AuthoredRoster& r) {
        withHouses(r);
        r.economy.rules["estates"] = 0;
    });
    run(s, 1 + 7.9 / 24, 1 + 17.99 / 24);
    s.takeJournal();
    setPurse(s, Society::WharfFund, 1000);
    auto journal = wagesTonight(s, 1);
    expect(!s.estatesOn() && !s.account("estate:greatly") && !s.account("estate:crown") && s.loopToday().estates.empty(), "no estate opened");
    for (const auto* who : {"docker", "steward", "maid", "valet", "tally"})
        expect(paidBy(journal, Society::WharfFund, who, 8), std::string("the Country paid ") + who);
    expect(s.cash(Society::WharfFund) >= 1000 - 40 && s.loopToday().estatePostsPaid == 40 && s.conserved(), "from its one purse; conserved");

    auto t = city(withHouses);
    run(t, 1 + 7.9 / 24, 1 + 17.99 / 24);
    setPurse(t, Society::WharfFund, 1000);
    run(t, 1 + 17.99 / 24, 1 + 18.5 / 24);          // (Split: a fifth each.)
    const auto money = t.moneySupply();
    auto economy = t.authored().economy;
    economy.rules["estates"] = 0;
    t.adoptEconomy(economy);
    expect(!t.estatesOn() && t.payOf(post(t, "steward")).payer == Society::WharfFund && t.houseFund("Greatly") == Society::WharfFund,
           "off: the Country's purse again");
    expect(t.cash("estate:greatly") > 0, "the estates still hold theirs until the night");
    run(t, 1 + 18.5 / 24, 2 + 17.99 / 24);
    t.takeJournal();
    journal = wagesTonight(t, 2);
    for (const auto* id : {"estate:greatly", "estate:fell", "estate:timberland", "estate:crown"})
        expect(t.cash(id) == 0, std::string(id) + " emptied into the Country's purse");
    expect(t.loopToday().estates.at("greatly").moved < 0, "booked");
    expect(paidBy(journal, Society::WharfFund, "steward", 8) && paidBy(journal, Society::WharfFund, "valet", 8), "which pays every post");
    expect(t.conserved() && t.moneySupply() == money, "conserved");
    expect(!t.account("estate:greatly") && !t.account("estate:crown"), "the emptied purses closed");

    // Switched on again: the purses open anew and share the Country's money, so every estate pays its own people that
    // night (left open and empty they started with nothing, the Crown with nothing to grant).
    economy.rules["estates"] = 1;
    t.adoptEconomy(economy);
    run(t, 2 + 18.5 / 24, 3 + 17.99 / 24);
    t.takeJournal();
    journal = wagesTonight(t, 3);
    for (const auto* id : {"estate:greatly", "estate:fell", "estate:timberland", "estate:crown"})
        expect(t.account(id) && t.cash(id) > 0 && t.loopToday().estates.at(std::string(id).substr(7)).moved > 0, std::string(id) + ": its share again");
    expect(paidBy(journal, "estate:greatly", "steward", 8) && paidBy(journal, "estate:crown", "valet", 8) &&
               paidBy(journal, "estate:fell", "maid", 8) && paidBy(journal, "estate:timberland", "tally", 8) &&
               t.loopToday().estatePostsPaid == 40,
           "on again: each estate pays its own people");
    expect(t.conserved() && t.moneySupply() == money, "conserved");
}

void theCrownsLevy()
{
    // After estates6 (55 nights short: the Crown granted only what it held over its own posts while the wharf and
    // Timberland held more than theirs): the Crown levies the others' spare over their keep up to what it lacks of its own
    // (its posts' nights and the others' lacks), then, still short of tonight's posts and grants, what they hold over
    // tonight's posts; every post paid whenever the estates together hold tonight's; and together they spend no more
    // than the one purse would.
    auto s = city(withHouses);
    run(s, 1 + 7.9 / 24, 1 + 18.5 / 24);            // (The purses opened.)
    const auto night = [&](std::int64_t day, const std::map<std::string, std::int64_t>& purses) {
        run(s, double(day) + 7.9 / 24, double(day) + 17.99 / 24);
        s.takeJournal();
        for (const auto& [fund, coins] : purses)
            setPurse(s, fund, coins);
        return wagesTonight(s, day);
    };
    // Timberland 100p (its keep two nights of its post, 16p): the Crown, with nothing, lacks 56p of its keep (16p of its
    // own, 16p each for Greatly and the wharf, 8p for Fell), levied from Timberland's 84p over its keep; then its grants.
    auto money = s.moneySupply();
    auto journal = night(2, {{"estate:greatly", 0}, {Society::WharfFund, 0}, {"estate:fell", 8}, {"estate:timberland", 100}, {"estate:crown", 0}});
    auto two = s.loopToday();
    expect(two.estates.at("timberland").levyOut == 56 && two.estates.at("crown").levyIn == 56,
           "the Crown levied 56p of Timberland's spare: " + std::to_string(two.estates["timberland"].levyOut));
    expect(two.estates.at("greatly").grantsIn == 8 && two.estates.at("grayrock").grantsIn == 8 && two.estatePostsPaid == 40,
           "and granted Greatly and the wharf their posts: every post paid");
    expect(s.cash("estate:timberland") == 100 - 56 - 8 && s.cash("estate:crown") == 56 - 16 - 8, "Timberland keeps its keep and pays its own");
    bool recorded = false;
    for (const auto& e : journal)
        recorded = recorded || (e.from == "estate:timberland" && e.to == "estate:crown" && e.coins == 56 &&
                                e.kind.rfind("the Crown's levy on an estate's spare", 0) == 0);
    expect(recorded && s.conserved() && s.moneySupply() == money, "the levy in the journal; conserved");

    // Timberland 34p: 18p over its keep, levied; the Crown still 6p short of tonight's 24p (its post, Greatly's and the
    // wharf's), so 6p more of the 26p Timberland holds over tonight's post. Every post paid.
    money = s.moneySupply();
    journal = night(3, {{"estate:greatly", 0}, {Society::WharfFund, 0}, {"estate:fell", 8}, {"estate:timberland", 34}, {"estate:crown", 0}});
    const auto& three = s.loopToday();
    expect(three.estates.at("timberland").levyOut == 18 + 6 && three.estates.at("crown").levyIn == 24 && three.estatePostsPaid == 40,
           "levied over its keep, then over tonight's posts: every post paid");
    expect(s.cash("estate:timberland") == 34 - 24 - 8 && s.cash("estate:crown") == 0 && s.conserved() && s.moneySupply() == money,
           "Timberland's own post paid; conserved");

    // Together short (Timberland 12p, Fell 8p: 20p of the 40p): the levy takes no estate under tonight's post.
    journal = night(4, {{"estate:greatly", 0}, {Society::WharfFund, 0}, {"estate:fell", 8}, {"estate:timberland", 12}, {"estate:crown", 0}});
    expect(s.loopToday().estates.at("timberland").levyOut == 4 && (!s.loopToday().estates.count("fell") || s.loopToday().estates.at("fell").levyOut == 0) &&
               paidBy(journal, "estate:timberland", "tally", 8) && paidBy(journal, "estate:fell", "maid", 8) &&
               s.loopToday().estatePostsPaid == 8 + 8 + 4,
           "the 4p Timberland holds over its post levied, its own and Fell's paid");

    // Its spare: Greatly 100p over its own keep (16p) 84p, but the estates together hold 100p, 20p over the one purse's
    // keep (no reserve here, two nights of the five posts, 80p): 20p.
    run(s, 5 + 7.9 / 24, 5 + 7.95 / 24);
    for (const auto& [fund, coins] : std::map<std::string, std::int64_t>{
             {"estate:greatly", 100}, {Society::WharfFund, 0}, {"estate:fell", 0}, {"estate:timberland", 0}, {"estate:crown", 0}})
        setPurse(s, fund, coins);
    const auto owed = s.estatePostsOwed();
    expect(s.estateSpare("estate:greatly", owed) == 20 && s.estateSpare("estate:fell", owed) < 0,
           "an estate's spare never past the estates' together: " + std::to_string(s.estateSpare("estate:greatly", owed)));

    // The rule off: the Country's own spare over its keep, as before.
    auto t = city([](AuthoredRoster& r) {
        withHouses(r);
        r.economy.rules["estates"] = 0;
    });
    run(t, 1 + 7.9 / 24, 1 + 7.95 / 24);
    setPurse(t, Society::WharfFund, 100);
    expect(t.estateSpare(Society::WharfFund, t.estatePostsOwed()) == 100 - 80, "off: the one purse over its keep");
}

void pausedNeedsBuyNothing()
{
    auto s = city([](AuthoredRoster& r) { r.economy.needsPaused = true; });
    const auto hunger = s.resident("clerk")->hunger;
    run(s, 1 + 7.9 / 24, 1 + 20. / 24);
    expect(s.loopToday().foodBought == 0 && s.loopYesterday().foodBought == 0 && s.loopToday().wagesPaid == 0, "paused: nobody buys or is paid");
    expect(s.resident("clerk")->hunger == hunger, "and hunger is held");
    expect(s.conserved(), "conserved");
}

void theCountryOffSpends()
{
    // The Country switched off only takes in, as fund:store did.
    auto s = city([](AuthoredRoster& r) { r.economy.wharf = false; });
    run(s, 1 + 7.9 / 24, 1 + 17. / 24);
    expect(s.loopToday().basketSpent == 0, "the Country off: it spends nothing");
    expect(s.cash(Society::WharfFund) > 0 && s.conserved(), "but still takes in, conserved");
}

void anOlderSavesFund()
{
    // A save from before doc 68 holds fund:store: it loads with those coins the Country's.
    auto s = city();
    auto state = s.state();
    state.accounts[Society::StoreFund] = {};
    state.accounts.at(Society::StoreFund).cash = 70;
    state.accounts.at("treasury").cash -= 70;
    state.accounts.erase(Society::WharfFund);
    auto again = city();
    expect(again.restore(state), "an older save loads");
    expect(!again.account(Society::StoreFund) && again.cash(Society::WharfFund) == 70 && again.conserved(),
           "its fund:store is the Country's now");
}

void anOlderSavesCountry()
{
    // A save from before doc 73 holds outside:country (coins and goods): it loads with them the Wharf's, the old purse
    // gone, nothing made; and a save of the Wharf's book comes back as it was.
    auto s = city();
    auto state = s.state();
    state.accounts[Society::OldCountryFund] = state.accounts.at(Society::WharfFund);
    state.accounts.at(Society::OldCountryFund).cash = 90;
    state.accounts.at(Society::OldCountryFund).stock["iron_bar"] = 3;
    state.accounts.at("treasury").cash -= 90;
    state.accounts.erase(Society::WharfFund);
    state.loop.wharf = {120, 80, 9, 50, 330};
    auto again = city();
    expect(again.restore(state), "an older save loads");
    expect(!again.account(Society::OldCountryFund) && again.cash(Society::WharfFund) == 90 &&
               Society::stock(*again.account(Society::WharfFund), "iron_bar") == 3 && again.conserved(),
           "its outside:country is the Wharf's now, coins and goods");
    // Through the wire as a checkpoint saves it.
    const auto back = wire::readSociety(wire::society(again.state()));
    expect(back.loop.wharf.in == 120 && back.loop.wharf.out == 80 && back.loop.wharf.fee == 9 && back.loop.wharf.moved == 50 &&
               back.loop.wharf.credit == 330 && back.minted >= 0,
           "the Wharf's book saved and read back");
}

void theWharfLands()
{
    // A good no area grows (this catalogue has no farms) is the Wharf's lot whole: with the valve on, the realm pays the
    // beyond for it (sunk), the Wharf keeps its fee, the credit rises by what left; the toll and carriage as before.
    expect(items::cityLoop().rules.at("wharfValve") == 1, "the valve on in the data file");
    const auto open = [](AuthoredRoster& r) { r.economy.rules["wharfValve"] = 1; };
    auto s = city(open);
    expect(s.wharfValveOn() && s.rule("wharfFee") == 10, "the valve on, a tenth's fee");
    const auto money = s.moneySupply(), sunk = s.state().sunk, wharf = s.cash(Society::WharfFund), treasury = s.cash("treasury");
    const auto credit = s.state().loop.wharf.credit;
    const auto r = s.drawFromStore("smith", "iron_bar", 5, "drawn for its crafts");
    expect(r.ok, "the smith's iron drawn: " + r.message);
    const auto toll = s.cash("treasury") - treasury;
    const auto due = r.total - toll, fee = due / 10;
    expect(s.cash(Society::WharfFund) - wharf == fee, "the Wharf keeps its fee: " + std::to_string(s.cash(Society::WharfFund) - wharf));
    expect(s.state().sunk - sunk == due - fee && s.state().loop.wharf.credit - credit == due - fee, "the rest paid beyond, credited");
    expect(s.conserved() && s.moneySupply() == money - (due - fee), "conserved: the money's fall is what left the realm");
    expect(s.loopToday().wharfImported.at("iron_bar").pieces == 5 && s.loopToday().wharfImported.at("iron_bar").pennies == due,
           "booked as landed");
    // Never past the credit's room (wharfCreditMost, 2,000 in the data file): the realm pays the beyond no more than it
    // may have paid and not had back; the rest kept in the Wharf's purse.
    expect(s.rule("wharfCreditMost") == 2000, "the credit's cap, 2,000p");
    expect(s.moveWharfCredit(2000 - s.state().loop.wharf.credit - 3, "test", "near its cap").ok, "the credit 3p under its cap");
    expect(s.shift("treasury", s.tillOf("smith"), "", 0, 200, "test purse"), "the smith's till topped up");
    const auto nearSunk = s.state().sunk, nearWharf = s.cash(Society::WharfFund), nearTreasury = s.cash("treasury");
    const auto near = s.drawFromStore("smith", "iron_bar", 5, "drawn for its crafts");
    const auto nearDue = near.total - (s.cash("treasury") - nearTreasury);
    expect(near.ok && s.state().sunk - nearSunk == 3 && s.state().loop.wharf.credit == 2000 &&
               s.cash(Society::WharfFund) - nearWharf == nearDue - 3 && s.conserved(),
           "3p paid beyond, the rest kept: " + std::to_string(s.state().sunk - nearSunk) + ": " + near.message);
    // The valve off: the Wharf keeps it all, as the Country did.
    auto off = city([](AuthoredRoster& r) { r.economy.rules["wharfValve"] = 0; });
    const auto offWharf = off.cash(Society::WharfFund), offSunk = off.state().sunk, offTreasury = off.cash("treasury");
    const auto o = off.drawFromStore("smith", "iron_bar", 5, "drawn for its crafts");
    expect(o.ok && off.cash(Society::WharfFund) - offWharf == o.total - (off.cash("treasury") - offTreasury) && off.state().sunk == offSunk,
           "the valve off: all to the Wharf's purse, nothing sunk");
    // The stable price: the store room's at markup 100, the season's factor; never a shop's.
    expect(s.stablePrice("iron_bar", 4) == Society::catalogPrice("iron_bar") * 4, "the stable price, out of season groups");
    // Never: cheese, refused on both lists; no good on both.
    const auto& lists = items::cityLoop().wharf;
    expect(lists.never.count("cheese") && !lists.imports.count("cheese") && !lists.takes.count("cheese") && !s.wharfImports("cheese") &&
               !s.wharfTakes("cheese"),
           "cheese is never the Wharf's");
    for (const auto& [good, g] : lists.imports)
        expect(!lists.takes.count(good), good + ": not imported and taken both");
    expect(lists.imports.count("aqua_vitae") && lists.imports.at("aqua_vitae").on && lists.imports.count("offal") &&
               lists.imports.at("offal").on && lists.imports.count("butter") && !lists.imports.at("eggs").on,
           "aqua vitae, offal and butter imported; the new producers' goods listed off");
}

void anOrderByWagon()
{
    // A far community's store orders from the Wharf (doc 73, 5): the stable price, the toll, carriage by weight to the
    // carters (the treasury's share as any draw's), the beyond's share sunk; whole or nothing.
    auto s = city([](AuthoredRoster& r) { r.economy.rules["wharfValve"] = 1; });
    const std::string store = "stores:frosthollow";
    expect(s.openAccount(store) && s.shift("treasury", store, "", 0, 500, "test purse"), "the community's store, a purse");
    const auto carter = s.carters();
    const auto money = s.moneySupply(), carters = s.cash(carter), treasury = s.cash("treasury"), sunk = s.state().sunk;
    const auto r = s.orderFromWharf(store, "timber", 50, "an order from the Wharf");
    expect(r.ok && Society::stock(*s.account(store), "timber") == 50, "landed and carted to the store: " + r.message);
    expect(r.total == s.stablePrice("timber", 50), "at the stable price");
    const auto carriage = s.cash(carter) - carters;
    expect(carriage > 0 && r.message.find("carriage") != std::string::npos, "carriage to the carters: " + std::to_string(carriage));
    expect(s.cash("treasury") < treasury + r.total, "the toll to the treasury (its carriage share out)");
    expect(s.state().sunk > sunk && s.conserved() && s.moneySupply() == money - (s.state().sunk - sunk), "the beyond's share sunk, conserved");
    expect(!s.orderFromWharf(store, "iron_bar", 0, "x").ok && !s.orderFromWharf(store, "iron_bar", 1001, "x").ok, "whole quantities only");
    const auto poor = s.orderFromWharf(store, "steel_bar", 999, "too dear");
    expect(!poor.ok && Society::stock(*s.account(store), "steel_bar") == 0, "unpaid, nothing landed: " + poor.message);
}

void theCreditMoved()
{
    // The DM's lever (doc 73, 4.6): the credit moved within wharfActionMost, refused past it or with the rule 0; the
    // standing rules move it each night at 18:00; no coin moves.
    auto s = city([](AuthoredRoster& r) { r.economy.rules["wharfValve"] = 1; });
    const auto money = s.moneySupply();
    expect(s.moveWharfCredit(500, "dm", "a test").ok && s.state().loop.wharf.credit == 500 && s.state().loop.wharf.moved == 500,
           "moved by 500");
    expect(!s.moveWharfCredit(2001, "dm", "").ok && !s.moveWharfCredit(-2001, "dm", "").ok && !s.moveWharfCredit(0, "dm", "").ok,
           "past wharfActionMost (2,000) or nothing: refused");
    expect(s.moveWharfCredit(-800, "dm", "").ok && s.state().loop.wharf.credit == -300, "a take may go below 0");
    expect(s.wharfExportRoom() == 0, "below 0, the beyond buys nothing");
    expect(s.moneySupply() == money && s.conserved(), "no coin moved");
    auto none = city([](AuthoredRoster& r) { r.economy.rules["wharfActionMost"] = 0; });
    expect(!none.moveWharfCredit(10, "dm", "").ok, "with the rule 0, refused");
    // The standing lever: 100p a day in at 18:00, capped at wharfCreditMost.
    auto rule = city([](AuthoredRoster& r) {
        r.economy.rules["wharfValve"] = 1;
        r.economy.rules["wharfInjectADay"] = 100;
        r.economy.rules["wharfCreditMost"] = 150;
    });
    run(rule, 10 + 17.5 / 24, 10 + 18.5 / 24);
    expect(rule.state().loop.wharf.credit >= 100 && rule.state().loop.wharf.credit <= 150, "a night's inject: " +
           std::to_string(rule.state().loop.wharf.credit));
    run(rule, 11 + 17.5 / 24, 11 + 18.5 / 24);
    expect(rule.state().loop.wharf.credit <= 150, "never over its cap");
    expect(rule.conserved(), "conserved");
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
        expect(w.loadWorldFiles(files(""), "test").ok, "a world without loop records loads");
        const auto& e = w.society().authored().economy;
        expect(!e.needsPaused && e.wharf && e.wharfReserve == 200 && e.gateToll == 30 && e.carriage == 6 && e.wageKeeper == 14 &&
                   e.wageHand == 8 && e.wagePublic == 8 && e.wageWharf == 8 && e.wharfOff.empty() && e.subsidies.at("carriage") == 25,
               "with the defaults");
    }
    {
        World w;
        const auto r = w.loadWorldFiles(files("loop needs off country off reserve 50 toll 30 carriage 9 wages 20 9 7 6\n"
                                              "countryoff \"fish\"\ncountryoff \"flour\"\nsubsidy \"carriage\" 40\n"), "test");
        expect(r.ok, "the loop's records load: " + r.message);
        const auto& e = w.society().authored().economy;
        expect(e.needsPaused && !e.wharf && e.wharfReserve == 50 && e.gateToll == 30 && e.carriage == 9 && e.wageKeeper == 20 &&
                   e.wageHand == 9 && e.wagePublic == 7 && e.wageWharf == 6, "the loop record is read");
        expect(e.wharfOff == std::set<std::string>{"fish", "flour"} && e.subsidies.at("carriage") == 40, "and the goods and the subsidy");
    }
    for (const auto* bad : {"loop needs on country on reserve 200 toll 91 carriage 6 wages 14 8 8 8\n",
                            "loop needs maybe country on reserve 200 toll 25 carriage 6 wages 14 8 8 8\n",
                            "loop needs on country on reserve 200 toll 25 carriage 6 wages 14 8 8\n",
                            "loop needs on country on reserve 200 toll 25 carriage 6 wages 14 8 8 8 9\n",
                            "loop needs on country on reserve 200 toll 25 carriage 6 wages 14 8 8 8\nloop needs on country on reserve 200 toll 25 carriage 6 wages 14 8 8 8\n",
                            "countryoff \"no_such_good\"\n", "countryoff \"fish\"\ncountryoff \"fish\"\n",
                            "subsidy \"roads\" 10\n", "subsidy \"carriage\" 101\n", "subsidy \"carriage\" 10\nsubsidy \"carriage\" 20\n"})
    {
        World w;
        expect(!w.loadWorldFiles(files(bad), "test").ok, std::string("refused: ") + bad);
    }
    const auto build = files("loop needs on country on reserve 200 toll 30 carriage 6 wages 14 8 8 8\ncountryoff \"fish\"\nsubsidy \"carriage\" 5\n").at("world.ratw");
    const auto stripped = game::Game::withoutPeople(build);
    expect(stripped.find("loop ") == std::string::npos && stripped.find("countryoff") == std::string::npos &&
               stripped.find("subsidy") == std::string::npos, "a streamed build's loop records are stripped");
    // A rule under its old name (doc 73: countryPostsDays) reads as the new.
    {
        World w;
        expect(w.loadWorldFiles(files("rule \"countryPostsDays\" 3\n"), "test").ok, "an old rule's name loads");
        const auto& rules = w.society().authored().economy.rules;
        expect(rules.count("estatePostsDays") && rules.at("estatePostsDays") == 3 && !rules.count("countryPostsDays") &&
                   w.society().rule("estatePostsDays") == 3,
               "as estatePostsDays");
    }
}
} // namespace


void theExportBasket()
{
    // The export basket (doc 73, 4.5): the credit buys the city's exports from the makers' open shops, minted as spent,
    // the credit down by exactly what the beyond paid in; its carriage the beyond's alone (no treasury share); never a
    // good the Wharf landed today (the beyond never buys back what it sold the realm).
    const auto open = [](AuthoredRoster& r) {
        r.economy.rules["wharfValve"] = 1;
        r.economy.wharfReserve = 0;
    };
    const auto exported = [](const std::vector<EconomyEntry>& journal) {
        std::map<std::string, int> goods;
        for (const auto& e : journal)
            if (e.kind == "for beyond the realm" && e.quantity > 0)
                goods[e.item] += e.quantity;
        return goods;
    };
    auto s = city(open);
    expect(s.moveWharfCredit(2000, "test", "the export basket's credit").ok, "credit to spend");
    s.takeJournal();
    const auto& book = s.state().loop.wharf;
    const auto credit = book.credit, in = book.in, out = book.out, moved = book.moved;
    run(s, 1 + 9.9 / 24, 1 + 11.1 / 24);
    const auto journal = s.takeJournal();
    const auto goods = exported(journal);
    expect(!goods.empty() && s.loopToday().wharfBasket > 0, "the beyond bought the city's goods: " + std::to_string(s.loopToday().wharfBasket) + "p");
    expect(book.credit - credit == (book.out - out) - (book.in - in) + (book.moved - moved),
           "the credit down by what the beyond paid in, up by what it was paid");
    expect(s.conserved(), "conserved");
    // A good landed today is not bought back: the same city, the same hours, one of those goods first landed by the Wharf.
    const auto good = goods.begin()->first;
    auto again = city(open);
    expect(again.moveWharfCredit(2000, "test", "").ok && again.openAccount("stores:test") &&
               again.shift("treasury", "stores:test", "", 0, 2000, "test purse"),
           "the same credit, and a store to order for");
    const auto landed = again.orderFromWharf("stores:test", good, 1, "an order from the Wharf");
    expect(landed.ok && again.loopToday().wharfImported.count(good), good + " landed today: " + landed.message);
    again.takeJournal();
    run(again, 1 + 9.9 / 24, 1 + 11.1 / 24);
    const auto after = exported(again.takeJournal());
    expect(!after.count(good), good + " not bought for beyond the realm the day it was landed");
    expect(again.conserved(), "conserved");
}

int main()
{
    try
    {
        catalogueWithCarters();
        whoPays();
        aDrawSplit();
        theSubsidyGivesWayFirst();
        aDayOfTheLoop();
        itsPostsFirst();
        theHousesEstates();
        theEstatesOff();
        theCrownsLevy();
        pausedNeedsBuyNothing();
        theCountryOffSpends();
        anOlderSavesFund();
        anOlderSavesCountry();
        theWharfLands();
        anOrderByWagon();
        theCreditMoved();
        theExportBasket();
        theRecords();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Wharf tests passed: " << checks << " checks.\n";
    return 0;
}
