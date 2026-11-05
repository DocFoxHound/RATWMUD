// The first economy fixes of Ridgemere 2.0 (Docs/Design/68-the-country-and-the-city-loop.md, Progress, "First fixes"):
// the loop's rules as settings (city_loop.json's `rules`, a world's `rule` records), and what each does. Comforts that
// spend savings (no 20-a-day limit); fair food within a household (the least fed first, everyone to a day before anyone
// to two, only food someone can pay for); the alms as a setting; the treasury over its cap paying the short tills'
// posts; the work bank holding a business's longest craft; the wage rules' heads' posts; and the city's services,
// bought only from what is left after food, needs and the reserve, split between the shop's till and the treasury, never
// kept. Money is conserved in every one.
//
// The catalogue is the real Data/Items (RATW_SOURCE_DIR).
#include "RatwCalendar.h"
#include "RatwGame.h"
#include "RatwItems.h"
#include "RatwSociety.h"
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

Society city(std::vector<ResidentSpec> residents, std::vector<Kin> kin, const std::function<void(AuthoredRoster&)>& change = {})
{
    AuthoredRoster r;
    r.residents = std::move(residents);
    r.kin = std::move(kin);
    r.economy.treasury = 10000;
    r.economy.storeRule = true;
    // The first fixes' loop: the later rules as they were (trades_tests, treasury_tests and selfcorrect_tests have theirs;
    // doc 68, "Trades", "The treasury's surplus", "Self-correcting rules").
    r.economy.rules = {{"subsidyToNeed", 0}, {"floatDrawDays", 1}, {"buyAheadDays", 0}, {"patrons", 0}, {"suppliers", 0}, {"surplus", 0},
                       {"stockPrices", 0}, {"shelfFollow", 0}, {"wageDrift", 0}, {"treasuryDrift", 0}, {"wharfValve", 0}};
    if (change)
        change(r);
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

void run(Society& s, double from, double to)
{
    const auto bodies = atWork(s);
    for (double now = from; now < to; now += 1. / calendar::SecondsPerDay)
        s.tick(1, now, 0, bodies);
}

// The food each resident took in for the household (bought itself, or handed by the member who paid), in days of food
// (NourishADay: about 46 nourishment).
std::map<std::string, double> received(const std::vector<EconomyEntry>& journal)
{
    constexpr double NourishADay = .0035 * 14400 / 1.1;
    std::map<std::string, double> got;
    for (const auto& e : journal)
    {
        const double n = Society::edible(e.item) ? double(Society::nourishment(e.item)) * e.quantity / NourishADay : 0;
        if (e.kind == "market")
            got[e.from] += n;                       // (A sale is recorded buyer first.)
        else if (e.kind == "for the household")
        {
            got[e.from] -= n;
            got[e.to] += n;
        }
    }
    return got;
}
std::string days(double d)
{
    std::ostringstream o;
    o.precision(2);
    o << d;
    return o.str();
}

void theRulesAndTheirRecords()
{
    auto s = city({person("fish", "merchant", "fishmonger at the Fish Hall", {"hall", 2.5, 2.5}, "home_fish")}, {});
    expect(s.rule("comfortsMost") == 200 && s.rule("almsADay") == 10 && s.rule("treasuryCap") == 2000 && s.rule("wageSupport") == 100 &&
               s.rule("workBankHours") == 0 && s.rule("fairFood") == 1 && s.rule("services") == 1 && s.rule("servicesCityShare") == 30,
           "the rules as city_loop.json has them");
    expect(s.rule("nonsense") == -1, "a name that isn't a rule");
    expect(s.comfortsAbove() == 40 && std::abs(s.comfortsShare() - .2) < 1e-9, "comforts over 40p a head, a fifth a day");
    expect(std::count_if(s.services().begin(), s.services().end(), [](const auto& v) { return !v.patron; }) == 12,
           "twelve services for households (and the patrons' work, doc 68, \"Trades\"): " + std::to_string(s.services().size()));
    for (const auto& v : s.services())
    {
        const auto* item = items::good(v.id);
        expect(item && item->category == "service" && v.price == item->price && v.on && v.every >= 1 && items::business(v.business),
               "a service is a catalogue item of a real business, at its price: " + v.id);
    }
    // The world's own, in records (migration 0058).
    const auto files = [](const std::string& extra) {
        std::ostringstream cell;
        cell << "id: town\nname: Town\ndescription: A town.\nworld: 0 0 0\nsize: 24 24\noutdoors: true\nweather: clear\ngrid:\n";
        for (int y = 0; y < 24; ++y)
            cell << std::string(24, '.') << '\n';
        std::string manifest = "RATW_WORLD 2\ncell \"town\" \"cells/town.cell\"\nspawn \"town\" 12.5 12.5\neconomy 1000 100 50 10 12\n" + extra +
                               "resident \"baker\" \"baker\" \"merchant\" \"baker at The Loaf\" \"Someone.\" \"Hello.\" 38 \"timber\" "
                               "\"female\" \"average\" \"piebald\" 4 3 0 8 1 8 17 \"-\" 30 0 1 \"town\" 3.5 5.5 \"town\" 3.5 8.5 \"town\" 3.5 11.5\n";
        return std::map<std::string, std::string>{{"world.ratw", manifest}, {"cells/town.cell", cell.str()}};
    };
    {
        World w;
        const auto r = w.loadWorldFiles(files("rule \"almsADay\" 12\nrule \"treasuryCap\" 500\nrule \"fairFood\" 0\n"
                                              "service \"groom_and_trim\" off price 2 every 9\nservice \"gone_away\" on price 1 every 1\n"),
                                        "test");
        expect(r.ok, "the rule and service records load: " + r.message);
        const auto& so = w.society();
        expect(so.rule("almsADay") == 12 && so.rule("treasuryCap") == 500 && so.rule("fairFood") == 0 && so.rule("comfortsMost") == 200,
               "the world's rules, the rest city_loop.json's");
        const auto& groom = so.services().front();
        expect(groom.id == "groom_and_trim" && !groom.on && groom.price == 2 && groom.every == 9, "the world's groom and trim");
        expect(so.services()[1].on && so.services()[1].price == items::good("tooth_drawn")->price, "the others as city_loop.json has them");
        expect(so.authored().economy.services.count("gone_away"), "a service city_loop.json no longer lists is kept, and does nothing");
    }
    for (const auto* bad : {"rule \"almsADay\" 1001\n", "rule \"nonsense\" 1\n", "rule \"fairFood\" 2\n", "rule almsADay 10\n",
                            "rule \"almsADay\" 10\nrule \"almsADay\" 11\n", "rule \"almsADay\" x\n", "rule \"almsADay\" 10 11\n",
                            "service \"groom_and_trim\" maybe price 2 every 9\n", "service \"groom_and_trim\" on price 0 every 9\n",
                            "service \"groom_and_trim\" on price 2 every 0\n", "service \"groom_and_trim\" on price 2\n",
                            "service \"groom_and_trim\" on price 2 every 9\nservice \"groom_and_trim\" on price 2 every 9\n"})
    {
        World w;
        expect(!w.loadWorldFiles(files(bad), "test").ok, std::string("refused: ") + bad);
    }
    const auto stripped = game::Game::withoutPeople(files("rule \"almsADay\" 12\nservice \"groom_and_trim\" off price 2 every 9\n").at("world.ratw"));
    expect(stripped.find("rule ") == std::string::npos && stripped.find("service ") == std::string::npos,
           "a streamed build's rule and service records are stripped");
    // The DM's economy.sync: the rules and services change at once.
    auto next = s.authored().economy;
    next.rules["almsADay"] = 3;
    next.rules["services"] = 0;
    next.services["tooth_drawn"] = {true, 9, 2};
    s.adoptEconomy(next);
    expect(s.rule("almsADay") == 3 && s.rule("services") == 0 && s.services()[1].price == 9 && s.services()[1].every == 2,
           "adoptEconomy: the new rules and services");
}

// A family of three with money for everyone's first day of food but not all their second, and a fishmonger.
std::vector<ResidentSpec> family(std::int64_t purse)
{
    return {person("fish", "merchant", "fishmonger at the Fish Hall", {"hall", 2.5, 2.5}, "home_fish"),
            person("aa_mother", "civilian", "keeps the house", {"home_fam", 2.5, 2.5}, "home_fam", 35, purse),
            person("bb_kid", "civilian", "at lessons", {"school", 2.5, 2.5}, "home_fam", 9, 0),
            person("cc_kid", "civilian", "at lessons", {"school", 4.5, 2.5}, "home_fam", 7, 0)};
}
const std::vector<Kin> FamilyKin{{"aa_mother", "bb_kid", "parent"}, {"aa_mother", "cc_kid", "parent"}};

void fairFoodInAHousehold()
{
    // A day's food is about 46 nourishment (4 fish of 15, 2p each); two days' 7. 30p: 12 fish in all.
    const auto day = [](bool fair) {
        auto s = city(family(30), FamilyKin, [&](AuthoredRoster& r) { r.economy.rules["fairFood"] = fair ? 1 : 0; });
        s.create(s.tillOf("fish"), "fish", 200, "test: a full fish hall");
        const auto money = s.moneySupply();
        run(s, 1 + 7.9 / 24, 1 + 17.5 / 24);       // (Before the wages and the alms.)
        expect(s.conserved() && s.moneySupply() == money, "conserved");
        return received(s.takeJournal());
    };
    auto fair = day(true), old = day(false);
    expect(fair["aa_mother"] >= 1 && fair["bb_kid"] >= 1 && fair["cc_kid"] >= 1,
           "fair: every member a day's food first (" + days(fair["aa_mother"]) + ", " + days(fair["bb_kid"]) + ", " + days(fair["cc_kid"]) + ")");
    expect(old["aa_mother"] >= 1.9 && old["cc_kid"] < 1, "the old order: the first members two days', the last short (" +
                                                            days(old["aa_mother"]) + ", " + days(old["bb_kid"]) + ", " + days(old["cc_kid"]) + ")");
    // Only food someone can pay for is picked: a hall of roasts (12p) and a little fish, the family holding 11p.
    auto s = city(family(11), FamilyKin, [](AuthoredRoster& r) { r.economy.storeRule = false; });   // (Nothing restocked.)
    const auto hall = s.tillOf("fish");
    for (const auto& [item, n] : std::map<std::string, int>(s.account(hall)->stock))
        if (Society::edible(item))
            s.consume(hall, item, n, "test: nothing else");
    s.create(hall, "roast", 200, "test: roasts");
    s.create(hall, "fish", 20, "test: and a little fish");
    run(s, 1 + 7.9 / 24, 1 + 17.5 / 24);
    auto got = received(s.takeJournal());
    const double fed = got["aa_mother"] + got["bb_kid"] + got["cc_kid"];
    expect(fed >= 1.2, "the family bought the fish it could pay for, not roasts it couldn't: " + days(fed) + " days");
    expect(s.conserved(), "conserved");
}

void almsAsASetting()
{
    for (const int level : {10, 13})
    {
        auto s = city(family(0), FamilyKin, [&](AuthoredRoster& r) {
            if (level != 10)
                r.economy.rules["almsADay"] = level;
        });
        run(s, 1 + 7.9 / 24, 1 + 18.5 / 24);
        const auto held = s.cash("aa_mother") + s.cash("bb_kid") + s.cash("cc_kid");
        expect(held == 3 * level, "alms make a household of three up to " + std::to_string(level) + "p a head: " + std::to_string(held));
        expect(s.loopYesterday().alms + s.loopToday().alms >= 3 * level && s.conserved(), "counted, and conserved");
    }
}

// A barber's shop that takes nothing: its till short of its keeper's and hand's wages.
std::vector<ResidentSpec> barbers(std::int64_t purse)
{
    return {person("barber", "merchant", "barber at The Close Trim", {"shop", 2.5, 2.5}, "home_barber", 40, purse),
            person("helper", "civilian", "helping at the barber's", {"shop", 4.5, 2.5}, "home_helper", 30, 30)};
}

void theTreasuryCapPaysShortTills()
{
    const auto evening = [](int cap, int support, std::int64_t treasury) {
        auto s = city(barbers(25), {}, [&](AuthoredRoster& r) {
            r.economy.rules["services"] = 0;
            r.economy.rules["treasuryCap"] = cap;
            r.economy.rules["wageSupport"] = support;
            r.economy.treasury = treasury;
        });
        const auto money = s.moneySupply();
        run(s, 1 + 17.9 / 24, 1 + 18.5 / 24);
        expect(s.conserved() && s.moneySupply() == money, "conserved");
        return s;
    };
    // The till holds 25p over its 20p float: 5p for 22p of wages.
    const auto none = evening(100000, 100, 10000);
    expect(none.loopToday().wageSupport == 0 && none.loopToday().wagesPaid < 8, "under the cap: no support, the posts short");
    auto full = evening(2000, 100, 10000);
    const auto& d = full.loopToday();
    std::map<std::string, std::int64_t> pay;
    for (const auto& e : full.takeJournal())
        if (e.kind == "wages" || e.kind == "wage support")
            pay[e.to] += e.coins;
    expect(d.wageSupport >= 15 && d.wagesPaid >= d.wagesOwed - 2 && d.tillsShort == 1,
           "over the cap the treasury pays what the till could not: " + std::to_string(d.wagesPaid) + " of " + std::to_string(d.wagesOwed) +
               ", " + std::to_string(d.wageSupport) + " of it support");
    expect(pay["barber"] >= 13 && pay["helper"] >= 7, "each post the same share of its wage: " + std::to_string(pay["barber"]) + " and " +
                                                          std::to_string(pay["helper"]));
    const auto half = evening(2000, 50, 10000);
    expect(half.loopToday().wageSupport >= 7 && half.loopToday().wageSupport <= 9, "wageSupport 50: half the shortfall: " +
                                                                                  std::to_string(half.loopToday().wageSupport));
    // Only what is over the cap: a treasury 6p over it gives at most 6p.
    const auto tight = evening(2000, 100, 2000 + 6);
    expect(tight.loopToday().wageSupport <= 6 && tight.cash("treasury") >= 2000, "never into the cap: " +
                                                                                  std::to_string(tight.loopToday().wageSupport));
    // A till that pays in full is given nothing.
    auto paying = city(barbers(500), {}, [](AuthoredRoster& r) { r.economy.rules["services"] = 0; });
    run(paying, 1 + 17.9 / 24, 1 + 18.5 / 24);
    expect(paying.loopToday().wageSupport == 0 && paying.loopToday().wagesPaid == paying.loopToday().wagesOwed, "a till that can pay: none");
}

void theWorkBankHoldsTheLongestCraft()
{
    // A lone cartographer: a map is 13.9 hours of work, more than the two hours' worth a crew of one used to bank.
    const auto maps = [](int bankHours) {
        auto s = city({person("carto", "merchant", "cartographer at The Soundings", {"shop", 2.5, 2.5}, "home_carto")}, {},
                      [&](AuthoredRoster& r) { r.economy.rules["workBankHours"] = bankHours; });
        const auto till = s.tillOf("carto");
        s.shift("treasury", till, "", 0, 2000, "test: working money");
        const auto* business = items::businessFor("cartographer at The Soundings");
        const double most = s.workBankMost(till, business);
        if (const int start = Society::stock(*s.account(till), "map"); start > 0)
            s.consume(till, "map", start, "test: the starting maps sold");
        for (int h = 0; h < 40; ++h)
            s.restockFromStore("carto", 1 + (8 + h) / 24.);
        expect(s.conserved(), "conserved");
        // The crew's hours, logged (round 3, doc 73): put in an hour each open hour, what the maps used, the rest lost past
        // the bank's cap or still banked.
        const int made = Society::stock(*s.account(till), "map");
        const auto found = s.loopToday().work.find(till);
        expect(found != s.loopToday().work.end(), "the cartographer's hours logged");
        const auto& w = found->second;
        double mapHours = 0;
        for (const auto* k : items::craftsFor(business->id))
            if (std::any_of(k->out.begin(), k->out.end(), [](const auto& o) { return o.first == "map"; }))
                mapHours = k->hours;
        expect(w.hours >= 1 && w.hours <= 40 && std::abs(w.used - made * mapHours) < 1e-6 && w.lost >= 0 &&
                   w.used + w.lost <= w.hours + 1e-6 && w.hours - w.used - w.lost <= most + 1e-6,
               "its hours: " + std::to_string(w.hours) + " put in, " + std::to_string(w.used) + " used, " + std::to_string(w.lost) +
                   " lost, " + std::to_string(made) + " maps");
        return std::pair<double, int>{most, made};
    };
    const auto [longest, made] = maps(0);
    expect(longest >= 14.8 && longest < 15, "the bank holds the longest craft and an hour: " + std::to_string(longest));
    expect(made >= 2, "maps are made over two days: " + std::to_string(made));
    const auto [two, none] = maps(2);
    expect(two == 2 && none == 0, "two hours' worth (the old rule): none ever made (" + std::to_string(none) + ")");
}

void theHeadsPostsArePaid()
{
    auto s = city({person("queen", "civilian", "the Queen, at the King's Residence", {"residence", 2.5, 2.5}, "home_q"),
                   person("office", "civilian", "running the City Office", {"office", 2.5, 2.5}, "home_o"),
                   person("clerk", "civilian", "clerking at the City Office", {"office", 4.5, 2.5}, "home_c"),
                   person("lender", "civilian", "lending", {"vance", 2.5, 2.5}, "home_v"),
                   person("aldric", "civilian", "overseeing the Grayrock wharf", {"harbour", 2.5, 2.5}, "home_a")},
                  {});
    const auto pay = [&](const std::string& id) { return s.payOf(*s.position("job:" + id)); };
    expect(pay("queen").payer == "treasury" && pay("queen").wage == 14, "the Queen: the treasury, a head's wage");
    expect(pay("office").payer == "treasury" && pay("office").wage == 14, "the City Office's head: the treasury, a head's wage");
    expect(pay("clerk").payer == "treasury" && pay("clerk").wage == 8, "its clerks as before");
    expect(pay("lender").payer == Society::WharfFund && pay("lender").wage == 14, "a House's lending: its estates (the Country)");
    expect(pay("aldric").payer == Society::WharfFund && pay("aldric").wage == 14, "the Grayrock wharf: the Country");
}

// The barbers, a household with savings and one without.
std::vector<ResidentSpec> customers()
{
    auto r = barbers(60);
    r.push_back(person("rich_a", "civilian", "keeps the house", {"home_rich", 2.5, 2.5}, "home_rich", 40, 1000));
    r.push_back(person("rich_b", "civilian", "at leisure", {"home_rich", 3.5, 2.5}, "home_rich", 40, 0));
    r.push_back(person("poor", "civilian", "keeps the house", {"home_poor", 2.5, 2.5}, "home_poor", 40, 30));
    r.push_back(person("fish", "merchant", "fishmonger at the Fish Hall", {"hall", 2.5, 2.5}, "home_fish", 30, 30));
    return r;
}

void servicesFromWhatIsLeftOver()
{
    const auto days = [](const std::function<void(AuthoredRoster&)>& change) {
        auto s = city(customers(), {{"rich_a", "rich_b", "spouse"}}, change);
        s.create(s.tillOf("fish"), "fish", 400, "test: a full fish hall");
        const auto money = s.moneySupply();
        const auto t0 = s.cash("treasury");
        run(s, 1 + 7.9 / 24, 3 + 17.5 / 24);
        expect(s.conserved() && s.moneySupply() == money, "conserved");
        (void)t0;
        auto journal = s.takeJournal();
        return std::pair<Society, std::vector<EconomyEntry>>{std::move(s), std::move(journal)};
    };
    // Groom and trim every day at 3p, so three days show it.
    auto [s, journal] = days([](AuthoredRoster& r) { r.economy.services["groom_and_trim"] = {true, 3, 1}; });
    std::map<std::string, int> done;
    std::int64_t paid = 0, city = 0, grooms = 0;
    for (const auto& e : journal)
    {
        if (e.kind == "a service")
        {
            ++done[e.from];
            paid += e.coins;
            expect(e.to == s.tillOf("barber"), "sold by the barber's till");
            if (e.item == "groom_and_trim")
                expect(e.coins == 3, "at the world's price"), ++grooms;
        }
        if (e.kind == "the city's share of a service")
        {
            expect(e.from == s.tillOf("barber") && e.to == "treasury", "the city's share from the till to the treasury");
            city += e.coins;
        }
    }
    expect(done["rich_a"] + done["rich_b"] >= 3 && grooms >= 3, "the household with savings had a groom and trim each day: " +
                                                                      std::to_string(grooms));
    expect(!done.count("poor") && !done.count("fish"), "the households with nothing over their reserve bought none");
    expect(city >= paid * 30 / 100 - 1 && city <= paid * 30 / 100 + 1, "the city's 30% of " + std::to_string(paid) + "p: " + std::to_string(city));
    for (const auto& [id, a] : s.state().accounts)
        for (const auto& v : s.services())
            expect(Society::stock(a, v.id) == 0, "a service is never kept: " + v.id + " in " + id);
    expect(s.loopYesterday().servicesSold > 0 || s.loopToday().servicesSold > 0, "counted");
    // Off, all of them or one; the city's share a setting.
    auto [off, offJournal] = days([](AuthoredRoster& r) {
        r.economy.rules["services"] = 0;
        r.economy.services["groom_and_trim"] = {true, 3, 1};
    });
    for (const auto& e : offJournal)
        expect(e.kind != "a service", "services off: none done");
    auto [one, oneJournal] = days([](AuthoredRoster& r) { r.economy.services["groom_and_trim"] = {false, 3, 1}; });
    for (const auto& e : oneJournal)
        expect(!(e.kind == "a service" && e.item == "groom_and_trim"), "one service off: never done");
    auto [all, allJournal] = days([](AuthoredRoster& r) {
        r.economy.services["groom_and_trim"] = {true, 3, 1};
        r.economy.rules["servicesCityShare"] = 100;
    });
    std::int64_t allPaid = 0, allCity = 0;
    for (const auto& e : allJournal)
    {
        allPaid += e.kind == "a service" ? e.coins : 0;
        allCity += e.kind == "the city's share of a service" ? e.coins : 0;
    }
    expect(allPaid > 0 && allCity == allPaid, "a city share of 100%: all of it to the treasury");
}

void comfortsSpendSavings()
{
    // 1,000p over 40p a head: a fifth of the excess a day, no longer stopped at 20 purchases.
    const auto spent = [](int most) {
        auto s = city(customers(), {{"rich_a", "rich_b", "spouse"}}, [&](AuthoredRoster& r) {
            r.economy.rules["services"] = 0;
            r.economy.rules["comfortsMost"] = most;
        });
        // A shop of comforts: candles at 2p, plenty.
        s.create(s.tillOf("fish"), "candle_tallow", 400, "test: candles");
        s.create(s.tillOf("fish"), "fish", 100, "test: fish");
        run(s, 1 + 7.9 / 24, 1 + 17.5 / 24);
        expect(s.conserved(), "conserved");
        return s.loopToday().comfortsBought;
    };
    const int limited = spent(20), unlimited = spent(200);
    expect(limited <= 20 && unlimited > 40, "comforts: " + std::to_string(limited) + " with the old limit, " + std::to_string(unlimited) +
                                                " without");
}
} // namespace

int main()
{
    try
    {
        catalogue();
        theRulesAndTheirRecords();
        fairFoodInAHousehold();
        almsAsASetting();
        theTreasuryCapPaysShortTills();
        theWorkBankHoldsTheLongestCraft();
        theHeadsPostsArePaid();
        servicesFromWhatIsLeftOver();
        comfortsSpendSavings();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Economy fix tests passed: " << checks << " checks.\n";
    return 0;
}
