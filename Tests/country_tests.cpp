// The Country and the city's loop (Docs/Design/68-the-country-and-the-city-loop.md): every flow conserved (store draws
// split between the Country and the treasury's gate toll, carriage to the carters with the treasury's subsidy, the
// Country's purchases and their carriage, households' food, needs and comforts, wages from tills, the treasury and the
// Country, a till's profit); who pays which post; the Country spreading its buying over the makers; the subsidy's cap and
// its giving way before public wages; residents fed; the per-good switch; the loop's records; an older save's fund:store.
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
    r.economy.rules = {{"subsidyToNeed", 0}, {"floatDrawDays", 1}, {"buyAheadDays", 0}, {"patrons", 0}, {"suppliers", 0}, {"surplus", 0},
                       {"stockPrices", 0}, {"shelfFollow", 0}, {"wageDrift", 0}, {"treasuryDrift", 0}};
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
    expect(pay("clerk").kind == "public" && pay("clerk").payer == "treasury", "a clerk: the treasury (country.json's rules)");
    expect(pay("docker").kind == "country" && pay("docker").payer == Society::CountryFund, "a docker: the Country");
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
    const auto t0 = s.cash(till), c0 = s.cash(Society::CountryFund), r0 = s.cash("treasury"), k0 = s.cash("till:job:carter");
    // 400 iron bars: 400 × 9p at 100%; carriage on 2,000 weight at 6p a load, 12p, of which the treasury pays a quarter.
    const auto cost = s.storeCost("smith", "iron_bar", 400);
    const auto drawn = s.drawFromStore("smith", "iron_bar", 400);
    expect(drawn.ok, "the draw: " + drawn.message);
    const auto toll = cost * 30 / 100;
    expect(s.cash(Society::CountryFund) - c0 == cost - toll, "the Country takes the cost less the toll");
    expect(s.cash("treasury") - r0 == toll - 3, "the treasury the toll, less its 3p of the carriage");
    expect(s.cash("till:job:carter") - k0 == 12, "the carters the carriage, 12p");
    expect(t0 - s.cash(till) == cost + 9, "the till the cost and 9p of the carriage");
    expect(s.conserved(), "conserved");
    // A good the Country no longer supplies is refused, and nothing moves.
    auto off = city([](AuthoredRoster& r) { r.economy.countryOff = {"iron_bar"}; });
    const auto offTill = off.tillOf("smith");
    const auto cash = off.cash(offTill);
    const auto refused = off.drawFromStore("smith", "iron_bar", 1);
    expect(!refused.ok && refused.message.find("no longer supplies") != std::string::npos && off.cash(offTill) == cash,
           "a good the Country no longer supplies is refused: " + refused.message);
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
    auto s = city([](AuthoredRoster& r) { r.economy.countryReserve = 0; });   // (A small city's Country takes in little.)
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
    expect(paid(Society::CountryFund, "docker"), "the Country paid the docker");
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
        if (e.kind == "to the Country")
            sold[e.to] += e.quantity;
    expect(s.loopYesterday().countrySpent > 0, "the Country bought the city's goods");
    expect(sold["till:job:smith"] > 0 && sold["till:job:smith2"] > 0,
           "both smithies sold to it: " + std::to_string(sold["till:job:smith"]) + " and " + std::to_string(sold["till:job:smith2"]));
    for (const auto& e : journal)
        expect(e.kind != "to the Country" || e.to != "till:job:fish" || e.item == "salt_fish" || e.item == "smoked_fish",
               "it buys the fishmonger's own make (salt and smoked fish), never its resale: " + e.item);
    // The Country's purse stays bounded: it keeps its reserve and spends the rest.
    expect(s.cash(Society::CountryFund) <= 8 + 2 * (s.loopYesterday().drawn + 100), "the Country's purse is bounded");
}

void itsPostsFirst()
{
    // Doc 72, Phase 3 (country2: the Country missed its posts on 6 days, its purse down to 16p after two lean days): it
    // keeps countryPostsDays (2) nights of its posts' wages in its purse, over its reserve, and its basket buys only with
    // what is over; so its posts are paid every night, a lean day or two included.
    const auto spentWith = [](int nights, std::int64_t& left, std::int64_t& owed) {
        auto s = city([&](AuthoredRoster& r) {
            r.economy.countryReserve = 0;
            r.economy.rules["countryPostsDays"] = nights;
        });
        owed = s.payOf(post(s, "docker")).wage;
        // Its purse set to three nights of its posts and 60p more.
        expect((s.cash(Society::CountryFund) == 0 || s.operatorTransfer(Society::CountryFund, "treasury", "", 0, s.cash(Society::CountryFund)).ok) &&
                   s.operatorTransfer("treasury", Society::CountryFund, "", 0, 3 * owed + 60).ok,
               "the Country's purse set");
        run(s, 1 + 7.9 / 24, 1 + 17.95 / 24);       // (The shop hours to just before the wages.)
        left = s.cash(Society::CountryFund);
        return s.loopToday().countrySpent;
    };
    expect(city().rule("countryPostsDays") == 2, "two nights by default (country.json)");
    std::int64_t left1 = 0, left2 = 0, owed = 0;
    const auto spent1 = spentWith(1, left1, owed), spent2 = spentWith(2, left2, owed);
    expect(owed > 0 && left2 >= 2 * owed, "two nights kept before the wages: " + std::to_string(left2) + " of " + std::to_string(2 * owed));
    expect(left1 >= owed && spent1 > spent2, "one night kept (as it was): it spends more: " + std::to_string(spent1) + " and " +
                                                  std::to_string(spent2));
    // Tonight's wages paid in full, and tomorrow's still in its purse after them: a day that takes in nothing can't leave
    // its posts unpaid.
    auto s = city([&](AuthoredRoster& r) { r.economy.countryReserve = 0; });
    expect((s.cash(Society::CountryFund) == 0 || s.operatorTransfer(Society::CountryFund, "treasury", "", 0, s.cash(Society::CountryFund)).ok) &&
               s.operatorTransfer("treasury", Society::CountryFund, "", 0, 3 * owed + 60).ok,
           "the Country's purse set");
    run(s, 1 + 7.9 / 24, 1 + 18.5 / 24);
    expect(s.loopToday().countryPostsOwed == owed && s.loopToday().countryPostsPaid == owed, "tonight's posts paid in full");
    expect(s.cash(Society::CountryFund) >= owed, "and the next night's kept: " + std::to_string(s.cash(Society::CountryFund)));
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
    r.economy.countryReserve = 0;
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
               Society::estateFund(*Society::estateOfHouse("Grayrock")) == Society::CountryFund &&
               Society::estateFund(*Society::estateOfHouse("Greatly")) == "estate:greatly",
           "the Houses' land: Grayrock's estate is the Country's purse (the wharf), land no estate names the Crown's");
    expect(s.estatesOn() && s.payOf(post(s, "steward")).payer == Society::CountryFund && s.houseFund("Greatly") == Society::CountryFund &&
               !s.account("estate:greatly"),
           "on by default; until the first night every post and rent the Country's purse");
    // The first night: the purses opened, the Country's money shared by the posts' wages (five posts at 8p: a fifth each).
    run(s, 1 + 7.9 / 24, 1 + 17.99 / 24);
    s.takeJournal();
    setPurse(s, Society::CountryFund, 1000);
    const auto money = s.moneySupply();
    auto journal = wagesTonight(s, 1);
    for (const auto* id : {"estate:greatly", "estate:fell", "estate:timberland", "estate:crown"})
        expect(s.account(id) && s.cash(id) == 200 - 8, std::string(id) + ": a fifth of the Country's money, less its post: " + std::to_string(s.cash(id)));
    expect(s.cash(Society::CountryFund) >= 200 - 8, "the Country (the wharf) keeps its fifth (and its sales since): " + std::to_string(s.cash(Society::CountryFund)));
    expect(paidBy(journal, Society::CountryFund, "docker", 8) && paidBy(journal, "estate:greatly", "steward", 8) &&
               paidBy(journal, "estate:fell", "maid", 8) && paidBy(journal, "estate:crown", "valet", 8) &&
               paidBy(journal, "estate:timberland", "tally", 8),
           "each estate paid its own people");
    const auto& night = s.loopToday();
    expect(night.estates.at("greatly").postsPaid == 8 && night.estates.at("greatly").moved == 200 && night.estates.at("grayrock").moved == -800 &&
               night.countryPostsOwed == 40 && night.countryPostsPaid == 40,
           "booked by estate, and the Country's posts in all");
    expect(s.payOf(post(s, "steward")).payer == "estate:greatly" && s.houseFund("Greatly") == "estate:greatly" &&
               s.houseFund("Grayrock") == Society::CountryFund && s.houseFund("none") == "estate:crown",
           "now the House's own purse");
    expect(s.conserved() && s.moneySupply() == money, "conserved");

    // The Crown's grants: Greatly 3p (5p short), the wharf none (8p short), Fell its 8p, the Crown 100p: it gives each what
    // it lacks of tonight's posts, recorded; every post paid.
    run(s, 2 + 7.9 / 24, 2 + 17.99 / 24);
    s.takeJournal();
    for (const auto& [fund, coins] : std::map<std::string, std::int64_t>{
             {"estate:greatly", 3}, {Society::CountryFund, 0}, {"estate:fell", 8}, {"estate:timberland", 100}, {"estate:crown", 100}})
        setPurse(s, fund, coins);
    journal = wagesTonight(s, 2);
    const auto& two = s.loopToday();
    expect(two.estates.at("greatly").grantsIn == 5 && two.estates.at("grayrock").grantsIn == 8 && two.estates.at("crown").grantsOut == 13 &&
               two.estates.at("fell").grantsIn == 0,
           "the Crown granted Greatly 5p and the wharf 8p, Fell nothing");
    expect(two.countryPostsPaid == 40 && paidBy(journal, "estate:greatly", "steward", 8) && paidBy(journal, Society::CountryFund, "docker", 8),
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
             {"estate:greatly", 0}, {Society::CountryFund, 0}, {"estate:fell", 8}, {"estate:timberland", 8}, {"estate:crown", 10}})
        setPurse(s, fund, coins);
    journal = wagesTonight(s, 3);
    expect(paidBy(journal, "estate:crown", "valet", 8) && s.loopToday().estates.at("greatly").grantsIn == 1 &&
               s.loopToday().estates.at("grayrock").grantsIn == 1 && s.loopToday().countryPostsPaid == 8 * 3 + 1 + 1,
           "the Crown's own post paid; the rest shared; Greatly and the wharf paid a penny each");
    expect(s.conserved(), "conserved");

    // Its posts first: an estate buys (the basket, its tables) only with what is over countryPostsDays nights of its posts.
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
    setPurse(s, Society::CountryFund, 1000);
    auto journal = wagesTonight(s, 1);
    expect(!s.estatesOn() && !s.account("estate:greatly") && !s.account("estate:crown") && s.loopToday().estates.empty(), "no estate opened");
    for (const auto* who : {"docker", "steward", "maid", "valet", "tally"})
        expect(paidBy(journal, Society::CountryFund, who, 8), std::string("the Country paid ") + who);
    expect(s.cash(Society::CountryFund) >= 1000 - 40 && s.loopToday().countryPostsPaid == 40 && s.conserved(), "from its one purse; conserved");

    auto t = city(withHouses);
    run(t, 1 + 7.9 / 24, 1 + 17.99 / 24);
    setPurse(t, Society::CountryFund, 1000);
    run(t, 1 + 17.99 / 24, 1 + 18.5 / 24);          // (Split: a fifth each.)
    const auto money = t.moneySupply();
    auto economy = t.authored().economy;
    economy.rules["estates"] = 0;
    t.adoptEconomy(economy);
    expect(!t.estatesOn() && t.payOf(post(t, "steward")).payer == Society::CountryFund && t.houseFund("Greatly") == Society::CountryFund,
           "off: the Country's purse again");
    expect(t.cash("estate:greatly") > 0, "the estates still hold theirs until the night");
    run(t, 1 + 18.5 / 24, 2 + 17.99 / 24);
    t.takeJournal();
    journal = wagesTonight(t, 2);
    for (const auto* id : {"estate:greatly", "estate:fell", "estate:timberland", "estate:crown"})
        expect(t.cash(id) == 0, std::string(id) + " emptied into the Country's purse");
    expect(t.loopToday().estates.at("greatly").moved < 0, "booked");
    expect(paidBy(journal, Society::CountryFund, "steward", 8) && paidBy(journal, Society::CountryFund, "valet", 8), "which pays every post");
    expect(t.conserved() && t.moneySupply() == money, "conserved");
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
    auto s = city([](AuthoredRoster& r) { r.economy.country = false; });
    run(s, 1 + 7.9 / 24, 1 + 17. / 24);
    expect(s.loopToday().countrySpent == 0, "the Country off: it spends nothing");
    expect(s.cash(Society::CountryFund) > 0 && s.conserved(), "but still takes in, conserved");
}

void anOlderSavesFund()
{
    // A save from before doc 68 holds fund:store: it loads with those coins the Country's.
    auto s = city();
    auto state = s.state();
    state.accounts[Society::StoreFund] = {};
    state.accounts.at(Society::StoreFund).cash = 70;
    state.accounts.at("treasury").cash -= 70;
    state.accounts.erase(Society::CountryFund);
    auto again = city();
    expect(again.restore(state), "an older save loads");
    expect(!again.account(Society::StoreFund) && again.cash(Society::CountryFund) == 70 && again.conserved(),
           "its fund:store is the Country's now");
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
        expect(!e.needsPaused && e.country && e.countryReserve == 200 && e.gateToll == 30 && e.carriage == 6 && e.wageKeeper == 14 &&
                   e.wageHand == 8 && e.wagePublic == 8 && e.wageCountry == 8 && e.countryOff.empty() && e.subsidies.at("carriage") == 25,
               "with the defaults");
    }
    {
        World w;
        const auto r = w.loadWorldFiles(files("loop needs off country off reserve 50 toll 30 carriage 9 wages 20 9 7 6\n"
                                              "countryoff \"fish\"\ncountryoff \"flour\"\nsubsidy \"carriage\" 40\n"), "test");
        expect(r.ok, "the loop's records load: " + r.message);
        const auto& e = w.society().authored().economy;
        expect(e.needsPaused && !e.country && e.countryReserve == 50 && e.gateToll == 30 && e.carriage == 9 && e.wageKeeper == 20 &&
                   e.wageHand == 9 && e.wagePublic == 7 && e.wageCountry == 6, "the loop record is read");
        expect(e.countryOff == std::set<std::string>{"fish", "flour"} && e.subsidies.at("carriage") == 40, "and the goods and the subsidy");
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
}
} // namespace

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
        pausedNeedsBuyNothing();
        theCountryOffSpends();
        anOlderSavesFund();
        theRecords();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Country tests passed: " << checks << " checks.\n";
    return 0;
}
