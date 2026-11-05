// The treasury's surplus (Docs/Design/68-the-country-and-the-city-loop.md, Progress, "The treasury's surplus: works and
// alms"): at 18:00, after the treasury's own posts, the short tills' wages over its cap and the alms, what it still holds
// over its cap and surplusKeep goes to the poorest households (per head, poorest first, up to surplusAlmsTo) and to public
// works orders from the city's trades (till to till, the tills with most room under their keeper's profit first). Its
// defaults and the works' list; nothing with the rule off or under the cap and reserve; the order it runs in; the alms'
// level and ceiling; the works' orders, their split by trade and their tills; money conserved in every one.
//
// The catalogue is the real Data/Items (RATW_SOURCE_DIR).
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
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

// A city of these residents, its treasury and these rules (the household services off: nothing bought but at 18:00).
Society city(std::vector<ResidentSpec> residents, std::int64_t treasury, const std::map<std::string, int>& rules)
{
    AuthoredRoster r;
    r.residents = std::move(residents);
    r.economy.treasury = treasury;
    r.economy.storeRule = true;
    r.economy.rules = rules;
    r.economy.rules["services"] = 0;
    r.economy.rules.emplace("almsNoEarner", 100);   // (The alms as any household's: econ_fix_tests has the no-earner's, doc 73, round 4.)
    r.economy.rules.emplace("wharfValve", 0);       // (The Wharf's valve shut, doc 73: the Country's flows as these tests have them; wharf_tests and farms_tests open it.)
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

// The evening: the society from 17:54 to 18:30 of day 1 (the wages, the wage support, the alms and the surplus).
void evening(Society& s)
{
    std::map<std::string, LifeBody> bodies;
    for (const auto& r : s.authored().residents)
    {
        LifeBody b{r.work.cell, r.work.x, r.work.y};
        b.age = r.age;
        bodies[r.id] = b;
    }
    for (double now = 1 + 17.9 / 24; now < 1 + 18.5 / 24; now += 1. / calendar::SecondsPerDay)
        s.tick(1, now, 0, bodies);
}

ResidentSpec poor(const std::string& id, std::int64_t purse)
{
    return person(id, "civilian", "keeps the house", {"home_" + id, 2.5, 2.5}, "home_" + id, 35, purse);
}

void theDefaults()
{
    const Society s(Roster::None);
    expect(s.rule("surplus") == 1 && s.rule("surplusKeep") == 4000 && s.rule("surplusWorks") == 20 && s.rule("surplusAlms") == 80 &&
               s.rule("surplusAlmsTo") == 40,
           "the defaults: on, 4,000p kept over the cap, a fifth to the works, four fifths to the poorest, up to 40p a head");
    const auto& works = items::cityLoop().works;
    expect(works.size() >= 8 && works.front().business == "builders_yard" && works.front().weight == 3,
           "city_loop.json's works: the builders' yard first, and the other trades");
    for (const char* trade : {"smithy", "chandlery", "ropewalk", "cooperage"})
        expect(std::any_of(works.begin(), works.end(), [&](const items::WorksOrder& w) { return w.business == trade && !w.goods.empty(); }),
               std::string("the works order from the ") + trade);
}

void nothingUnderTheReserveOrOff()
{
    // Over the cap (2,000) but not over the cap and reserve (6,000): it stays.
    auto under = city({poor("ann", 0)}, 5000, {{"treasuryCap", 2000}, {"surplusKeep", 4000}});
    const auto money = under.moneySupply();
    evening(under);
    expect(under.loopToday().surplus == 0 && under.loopToday().surplusAlms == 0 && under.loopToday().worksSpent == 0,
           "under the cap and reserve: nothing");
    expect(under.cash("ann") == 10, "ann has the day's alms only: " + std::to_string(under.cash("ann")));
    expect(under.conserved() && under.moneySupply() == money, "conserved");
    // Off: nothing, however much the treasury holds.
    auto off = city({poor("ann", 0)}, 50000, {{"surplus", 0}});
    evening(off);
    expect(off.loopToday().surplus == 0 && off.cash("ann") == 10, "the rule off: nothing");
}

void itRunsLast()
{
    // A short barber's till (5p over its float for 22p of wages), a clerk the treasury pays, and a household with nothing:
    // the clerk's wage, the wage support and the alms first, the surplus from what is left over the cap and reserve.
    const std::vector<ResidentSpec> folk = {
        person("barber", "merchant", "barber at The Close Trim", {"shop", 2.5, 2.5}, "home_barber", 40, 25),
        person("helper", "civilian", "helping at the barber's", {"shop", 4.5, 2.5}, "home_helper", 30, 30),
        person("clerk", "civilian", "clerking at the Records Office", {"rolls", 2.5, 2.5}, "home_clerk", 30, 30), poor("ann", 0)};
    const std::map<std::string, int> rules = {{"treasuryCap", 2000}, {"surplusKeep", 0}, {"surplusAlmsTo", 1000}, {"surplusAlms", 100},
                                              {"surplusWorks", 0}};
    // 20p over the cap: the clerk's 8p, the barber's shortfall (about 17p) and ann's 10p take it under the cap.
    auto tight = city(folk, 2020, rules);
    auto money = tight.moneySupply();
    evening(tight);
    expect(tight.loopToday().wageSupport > 0 && tight.loopToday().alms == 10 && tight.loopToday().surplus == 0,
           "the wage support and the alms first; nothing over for the surplus: support " +
               std::to_string(tight.loopToday().wageSupport) + ", surplus " + std::to_string(tight.loopToday().surplus));
    expect(tight.conserved() && tight.moneySupply() == money, "conserved");
    // 200p over: the surplus is what is over the cap after them all, every penny to the households (no ceiling to speak
    // of: all of them raised to one level), and in the ledger after the wages, the support and the alms.
    auto over = city(folk, 2200, rules);
    money = over.moneySupply();
    evening(over);
    const auto& d = over.loopToday();
    std::int64_t clerk = 0;
    std::vector<std::string> order;
    for (const auto& e : over.takeJournal())
    {
        if (e.kind == "wages" && e.to == "clerk")
            clerk += e.coins;
        if (e.from == "treasury" && (order.empty() || order.back() != e.kind))
            order.push_back(e.kind);
    }
    expect(clerk == 8, "the clerk paid: " + std::to_string(clerk));
    expect(order == std::vector<std::string>{"wages", "wage support", "alms", "alms from the surplus"},
           "in the ledger: the treasury's posts, the wage support, the alms, then the surplus");
    expect(d.surplus == 200 - clerk - d.wageSupport - d.alms && d.surplus > 150,
           "the surplus is what is over the cap after the posts, the support and the alms: " + std::to_string(d.surplus) + " (support " +
               std::to_string(d.wageSupport) + ", alms " + std::to_string(d.alms) + ")");
    expect(d.surplusAlms == d.surplus && over.cash("treasury") == 2000,
           "all of it given, the treasury left at its cap: " + std::to_string(over.cash("treasury")));
    expect(over.conserved() && over.moneySupply() == money, "conserved");
}

void thePoorestToALevel()
{
    // The day's alms (10p a head) first: ann 0 -> 10, bob 5 -> 10, cat holds 12, dan 30, a household of two (eve and
    // fay) 20 between them -> 20.
    const std::vector<ResidentSpec> folk = {poor("ann", 0), poor("bob", 5), poor("cat", 12), poor("dan", 30),
                                            person("eve", "civilian", "keeps the house", {"home_ef", 2.5, 2.5}, "home_ef", 35, 20),
                                            person("fay", "civilian", "keeps the house", {"home_ef", 4.5, 2.5}, "home_ef", 35, 0)};
    const std::map<std::string, int> rules = {{"treasuryCap", 0}, {"surplusKeep", 1000}, {"surplusAlms", 100}, {"surplusWorks", 0},
                                              {"surplusAlmsTo", 20}};
    // 5p of surplus (the base alms take 15p): everyone at 10 a head to 11 (4p: ann, bob, eve and fay a head each), then
    // the next penny to the poorest first (ann); cat (12) nothing.
    {
        auto s = city(folk, 1000 + 15 + 5, rules);
        const auto money = s.moneySupply();
        evening(s);
        expect(s.loopToday().alms == 15 && s.loopToday().surplus == 5, "the day's alms 15p, 5p over: " + std::to_string(s.loopToday().surplus));
        const auto ef = s.cash("eve") + s.cash("fay");
        expect(s.cash("ann") == 12 && s.cash("bob") == 11 && ef == 22 && s.cash("cat") == 12 && s.cash("dan") == 30,
               "raised to a level, the poorest first: ann " + std::to_string(s.cash("ann")) + ", bob " + std::to_string(s.cash("bob")) +
                   ", eve and fay " + std::to_string(ef) + ", cat " + std::to_string(s.cash("cat")));
        expect(s.loopToday().surplusAlms == 5 && s.cash("treasury") == 1000, "every penny of it given, the reserve kept");
        expect(s.conserved() && s.moneySupply() == money, "conserved");
    }
    // Plenty over: everyone under it up to the ceiling, 20p a head, no further; dan (30) nothing; the rest stays (no works
    // to order from).
    {
        auto s = city(folk, 5000, rules);
        const auto money = s.moneySupply();
        evening(s);
        const auto ef = s.cash("eve") + s.cash("fay");
        expect(s.cash("ann") == 20 && s.cash("bob") == 20 && s.cash("cat") == 20 && ef == 40 && s.cash("dan") == 30,
               "up to the ceiling and no further: ann " + std::to_string(s.cash("ann")) + ", eve and fay " + std::to_string(ef) +
                   ", dan " + std::to_string(s.cash("dan")));
        expect(s.loopToday().surplusAlms == 10 + 10 + 8 + 20 && s.loopToday().surplusHouseholds == 4, "48p to four households");
        expect(s.cash("treasury") == 5000 - 15 - 48, "what neither places stays");
        expect(s.conserved() && s.moneySupply() == money, "conserved");
    }
    // A ceiling at the day's alms: nothing more.
    {
        auto s = city(folk, 5000, {{"treasuryCap", 0}, {"surplusKeep", 1000}, {"surplusAlms", 100}, {"surplusWorks", 0}, {"surplusAlmsTo", 10}});
        evening(s);
        expect(s.loopToday().surplusAlms == 0 && s.cash("ann") == 10, "a ceiling of 10p a head: no one is under it after the alms");
    }
}

// A builders' yard (its work on the public buildings: services), two smithies (one rich, its till at its profit line
// after tonight's wages; one with an empty till), and a household with nothing. No wage support (the short tills' posts
// would take the treasury's money first).
Society works(std::int64_t treasury, std::map<std::string, int> rules, int fittings)
{
    rules["wageSupport"] = 0;
    auto s = city({person("yard", "merchant", "keeping the builders' yard", {"yard", 2.5, 2.5}, "home_yard", 40, 30),
                   person("rich", "merchant", "the smith at the forge", {"forge", 2.5, 2.5}, "home_rich", 40, 2000),
                   person("smith", "merchant", "smith at the Anvil", {"anvil", 2.5, 2.5}, "home_smith", 40, 60), poor("ann", 0)},
                  treasury, rules);
    const auto smith = s.tillOf("smith");
    s.shift(smith, "smith", "", 0, s.cash(smith), "test: an empty till");
    for (const auto& till : {s.tillOf("rich"), smith})
        s.create(till, "fittings", fittings, "test: fittings on the shelf");
    return s;
}

void thePublicWorks()
{
    auto s = works(1000 + 10 + 100, {{"treasuryCap", 0}, {"surplusKeep", 1000}, {"surplusAlms", 0}, {"surplusWorks", 100}}, 10);
    const auto yard = s.tillOf("yard"), rich = s.tillOf("rich"), smith = s.tillOf("smith");
    const auto money = s.moneySupply();
    const auto rich0 = s.cash(rich), smith0 = s.cash(smith), yard0 = s.cash(yard);
    evening(s);
    const auto& d = s.loopToday();
    expect(d.surplus == 100, "100p over the reserve after ann's alms: " + std::to_string(d.surplus));
    expect(d.worksSpent > 100 - 6 && d.worksSpent <= 100 && s.cash("treasury") >= 1000,
           "the works ordered all but the odd pennies, never into the reserve: " + std::to_string(d.worksSpent));
    // The two trades present: the builders' yard 3 shares, the smithy 2 (of what is left).
    const auto byYard = d.worksTrades.count("builders_yard") ? d.worksTrades.at("builders_yard").spent : 0;
    const auto bySmith = d.worksTrades.count("smithy") ? d.worksTrades.at("smithy").spent : 0;
    expect(byYard > 60 - 6 && byYard <= 60 && byYard + bySmith == d.worksSpent,
           "three fifths to the builders' yard, the rest to the smithy: " + std::to_string(byYard) + " and " + std::to_string(bySmith));
    // Till to till, wages first: the yard's work is a service, done at once; the smithy's fittings come off the shelf of
    // the till with room under its profit (the empty one), and the rich one is given nothing while it has room.
    expect(s.cash(yard) - yard0 >= byYard - 30 && Society::stock(*s.account(yard), "wall_repointed") == 0 &&
               Society::stock(*s.account(yard), "slates_relaid") == 0 && Society::stock(*s.account(yard), "timbers_mended") == 0,
           "the yard's work done at once, nothing kept");
    expect(s.cash(rich) <= rich0 && Society::stock(*s.account(rich), "fittings") >= 10, "the rich smithy (no room): none of the orders");
    expect(Society::stock(*s.account(smith), "fittings") < 10 && Society::stock(*s.account(smith), "fittings") >= 1,
           "the empty smithy's fittings ordered, one left on the shelf for its customers");
    for (const char* good : {"fittings", "wall_repointed", "slates_relaid", "timbers_mended"})
        expect(Society::stock(*s.account("treasury"), good) == 0, std::string("the treasury keeps nothing: ") + good);
    expect(s.cash(smith) > smith0 || bySmith == 0, "the empty smithy paid");
    expect(s.conserved() && s.moneySupply() == money, "conserved");
}

void theSplitAndWhatIsLeft()
{
    // Half to each; ann at the ceiling after a little: what the alms can't place goes to the works.
    auto s = works(1000 + 10 + 200, {{"treasuryCap", 0}, {"surplusKeep", 1000}, {"surplusAlms", 50}, {"surplusWorks", 50}, {"surplusAlmsTo", 20}},
                   30);
    const auto money = s.moneySupply();
    evening(s);
    const auto& d = s.loopToday();
    expect(d.surplus == 200 && d.surplusAlms == 10 && s.cash("ann") == 20, "ann to the ceiling, 10p: " + std::to_string(d.surplusAlms));
    expect(d.worksSpent > 190 - 8 && d.worksSpent <= 190, "the alms' share it could not place to the works: " + std::to_string(d.worksSpent));
    expect(s.conserved() && s.moneySupply() == money, "conserved");
    // Shares of more than the whole: the whole, in their proportion (never more than the surplus).
    auto t = works(1000 + 10 + 200, {{"treasuryCap", 0}, {"surplusKeep", 1000}, {"surplusAlms", 100}, {"surplusWorks", 100},
                                     {"surplusAlmsTo", 1000}},
                   30);
    const auto money2 = t.moneySupply();
    evening(t);
    expect(t.loopToday().surplusAlms + t.loopToday().worksSpent <= 200 && t.cash("treasury") >= 1000,
           "shares over 100%: never more than the surplus");
    // Half to the alms; the works' odd pennies to the alms after them.
    expect(t.loopToday().surplusAlms >= 100 && t.loopToday().surplusAlms < 100 + 8 && t.loopToday().worksSpent > 100 - 8,
           "half to each: " + std::to_string(t.loopToday().surplusAlms) + " and " + std::to_string(t.loopToday().worksSpent));
    expect(t.conserved() && t.moneySupply() == money2, "conserved");
}

void daysOfIt()
{
    // Five days of the whole loop (the households shopping, the shops restocking, the wages): each evening the surplus
    // leaves the treasury at its cap and reserve or over them; money conserved every hour.
    auto s = works(8000, {{"treasuryCap", 2000}, {"surplusKeep", 1000}}, 30);
    const auto money = s.moneySupply();
    std::map<std::string, LifeBody> bodies;
    for (const auto& r : s.authored().residents)
    {
        LifeBody b{r.work.cell, r.work.x, r.work.y};
        b.age = r.age;
        bodies[r.id] = b;
    }
    int evenings = 0;
    long step = 0;
    for (double now = 1; now < 6; now += 1. / calendar::SecondsPerDay)
    {
        s.tick(1, now, 0, bodies);
        if (++step % 600 == 0)                      // (A game hour.)
            expect(s.conserved(), "conserved");
        if (const double hour = (now - std::floor(now)) * 24;
            hour >= 18.6 && hour < 18.6 + 24. / calendar::SecondsPerDay && s.loopToday().surplus > 0)
        {
            ++evenings;
            expect(s.cash("treasury") >= 3000, "the cap and reserve kept: " + std::to_string(s.cash("treasury")));
        }
    }
    expect(evenings >= 1 && s.moneySupply() == money, "an evening with a surplus at least; conserved");
}
} // namespace

int main()
{
    try
    {
        catalogue();
        theDefaults();
        nothingUnderTheReserveOrOff();
        itRunsLast();
        thePoorestToALevel();
        thePublicWorks();
        theSplitAndWhatIsLeft();
        daysOfIt();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Treasury tests passed: " << checks << " checks.\n";
    return 0;
}
