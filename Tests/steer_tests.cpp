// Steering the economy (Docs/Design/74-steering-the-economy.md): the rules and the lever table; the steering at the
// centre leaving today's economy as it is, and off restoring the world's own rules; the skim (the richest first, its
// lines and caps, fresh each time, money conserved); Never Broke and Never Hungry; the levers at the slider's ends; the
// controller (its step, band, rate, dead band, feed-forward, the toll lean's guard, deterministic) and its save.
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
#include <fstream>
#include <sstream>
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

ResidentSpec at(const std::string& id, std::int64_t purse)
{
    return person(id, "civilian", "keeps the house", {"home_" + id, 2.5, 2.5}, "home_" + id, 35, purse);
}

Society city(std::vector<ResidentSpec> residents, std::int64_t treasury, const std::map<std::string, int>& rules, std::vector<Kin> kin = {})
{
    AuthoredRoster r;
    r.residents = std::move(residents);
    r.kin = std::move(kin);
    r.economy.treasury = treasury;
    r.economy.storeRule = true;
    r.economy.rules = rules;
    r.economy.rules.emplace("services", 0);
    r.economy.rules.emplace("almsNoEarner", 100);
    r.economy.rules.emplace("wharfValve", 0);
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

void run(Society& s, double from, double to)
{
    std::map<std::string, LifeBody> bodies;
    for (const auto& r : s.authored().residents)
    {
        LifeBody b{r.work.cell, r.work.x, r.work.y};
        b.age = r.age;
        bodies[r.id] = b;
    }
    for (double now = from; now < to; now += 1. / calendar::SecondsPerDay)
        s.tick(1, now, 0, bodies);
}

void setRules(Society& s, const std::map<std::string, int>& rules)
{
    auto next = s.authored().economy;
    for (const auto& [k, v] : rules)
        next.rules[k] = v;
    s.adoptEconomy(next);
}

std::string n(double v)
{
    std::ostringstream o;
    o << v;
    return o.str();
}

void theRulesAndTheTable()
{
    const Society s(Roster::None);
    expect(s.rule("steer") == 0 && s.rule("steerLean") == 100 && s.rule("neverHungry") == 0 && s.rule("neverBroke") == 0,
           "off by default: the slider at the centre, both floors off");
    expect(s.rule("steerJudgeDays") == 28 && s.rule("steerMeanDays") == 56 && s.rule("steerStep") == 5 && s.rule("steerStepMost") == 10 &&
               s.rule("steerWidthMid") == 15 && s.rule("steerWidthEnd") == 30 && s.rule("skimMost") == 50 && s.rule("skimKeepDays") == 3,
           "the steering's own rules");
    const auto& t = items::cityLoop().steering;
    expect(items::steerAt(t.levers.at("shareOut"), -.25, 0) == 6 && items::steerAt(t.levers.at("shareOut"), .5, 0) == 0,
           "the table interpolates: the share-out 6% at -0.25");
    expect(items::steerAt(t.levers.at("surplusKeep"), 0, 4000) == 4000 && items::steerAt(t.levers.at("surplusKeep"), -.25, 4000) == 2500 &&
               items::steerAt(t.levers.at("surplusKeep"), -1, 4000) == 0,
           "W is the world's own: the keep 4,000 at the centre, 2,500 at -0.25, 0 at the far left");
    expect(items::steerAt(t.levers.at("richAbove"), 1, 40) == 600 && items::steerAt(t.levers.at("richAbove"), .75, 40) == 375,
           "the rich hold 600p a head at the far right");
    // The shared cases (Tests/fixtures/steer_table.json), which the studio's tests read too.
    {
        std::ifstream in(fs::path(RATW_SOURCE_DIR) / "Tests/fixtures/steer_table.json");
        std::stringstream text;
        text << in.rdbuf();
        json::Value doc;
        std::string error;
        expect(json::parse(text.str(), doc, error), "the fixture reads: " + error);
        for (const auto& c : doc.array("cases"))
        {
            const auto& table = c.string("kind") == "levers" ? t.levers : t.targets;
            const double own = c["own"].isNull() ? std::nan("") : c["own"].asNumber();
            const double got = items::steerAt(table.at(c.string("name")), c.number("at"), own);
            const bool ok = c["want"].isNull() ? std::isnan(got) : std::abs(got - c.number("want")) < 1e-9;
            expect(ok, "the shared case " + c.string("name") + " at " + n(c.number("at")) + ": " + n(got));
        }
    }
    expect(std::isnan(items::steerAt(t.targets.at("gini"), .25, std::nan(""))) && items::steerAt(t.targets.at("gini"), 0, std::nan("")) == .487 &&
               std::abs(items::steerAt(t.targets.at("upper"), .25, std::nan("")) - 19) < 1e-9,
           "a target is none where a neighbouring mark has none: the Gini only watched right of the centre");
}

// Three households, a barber's till, a clerk the treasury pays: enough of the loop to move money at 18:00.
std::vector<ResidentSpec> town()
{
    return {person("barber", "merchant", "barber at The Close Trim", {"shop", 2.5, 2.5}, "home_barber", 40, 25),
            person("hand", "civilian", "assistant at The Close Trim", {"shop", 3.5, 2.5}, "home_hand", 30, 5),
            at("aa_rich", 900), at("bb_mid", 120), at("cc_poor", 0)};
}

void theCentreLeavesTodayAlone()
{
    // At the centre (the push 0) every lever is the world's own: the money moves exactly as with the steering off.
    auto off = city(town(), 9000, {});
    auto on = city(town(), 9000, {{"steer", 1}});
    run(off, 1 + 7.9 / 24, 2 + 18.5 / 24);
    run(on, 1 + 7.9 / 24, 2 + 18.5 / 24);
    expect(on.state().loop.steer.on && on.state().loop.steer.push == 0 && on.steering(), "on, at the centre: the push 0");
    for (const auto& [id, a] : off.state().accounts)
        expect(on.cash(id) == a.cash, "at the centre, " + id + " holds what it does with the steering off: " + n(on.cash(id)) + " against " + n(a.cash));
    expect(on.rule("surplusKeep") == off.rule("surplusKeep") && on.rule("treasuryCap") == off.rule("treasuryCap") &&
               on.rule("surplusAlmsTo") == off.rule("surplusAlmsTo") && on.gateTollNow() == off.gateTollNow(),
           "today's rules and toll");
    expect(on.conserved() && on.loopToday().skim == 0, "conserved; nothing skimmed");
    expect(off.state().loop.steer.empty(), "off: nothing of the steering in the save");
}

void theSkim()
{
    auto s = city({at("aa", 600), at("bb", 300), at("cc", 5)}, 2000, {{"treasuryCap", 2000}});
    const auto money = s.moneySupply();
    const auto nec = s.necessitiesAHead();
    expect(nec >= 16 && nec <= 40, "a day's necessities a head: the alms, the reserve and the needs: " + n(nec));
    const auto aa = s.cash("aa"), bb = s.cash("bb");
    expect(s.skim(100, "test") == 100, "the skim raises what it is asked");
    expect(aa - s.cash("aa") == 100 && s.cash("bb") == bb && s.cash("cc") == 5, "the richest first: aa gives all 100");
    expect(s.cash(Society::ReliefFund) == 100 && s.conserved() && s.moneySupply() == money, "into fund:relief; conserved");
    // Fresh: bb is now the richest, and pays.
    s.shift("aa", "bb", "", 0, 400, "test: bb richer now");
    const auto bb2 = s.cash("bb"), aa2 = s.cash("aa");
    s.skim(50, "test");
    expect(bb2 - s.cash("bb") == 50 && s.cash("aa") == aa2, "fresh each time: the richest now is bb, and bb pays");
    // Water-filling from the top: the two richest brought toward one level, each within half its spare.
    auto w = city({at("aa", 600), at("bb", 300), at("cc", 5)}, 2000, {{"treasuryCap", 2000}});
    const auto line = std::max<std::int64_t>(w.comfortsAbove(), 3 * w.necessitiesAHead());
    const auto got = w.skim(300, "test");
    const auto fromA = 600 - w.cash("aa"), fromB = 300 - w.cash("bb");
    expect(got == 300 && fromA + fromB == 300 && fromA > fromB && fromB > 0 && fromA <= (600 - line) / 2 && fromB <= (300 - line) / 2,
           "both pay, the richer more, neither over half its spare: " + n(fromA) + " and " + n(fromB));
    std::int64_t byGroup = 0;
    for (const auto& [group, coins] : w.loopToday().skimBy)
        byGroup += group.rfind("households: ", 0) == 0 ? coins : 0;
    expect(byGroup == 300 && w.loopToday().skimPayers.size() == 2 && w.loopToday().skimFor.at("test") == 300,
           "logged by payer and purpose");
    // Asked for more than anyone has spare: the second pass to a day's necessities, then short, logged.
    auto all = city({at("aa", 600), at("bb", 300), at("cc", 5)}, 2000, {{"treasuryCap", 2000}});
    const auto raised = all.skim(1000000, "test");
    expect(raised < 1000000 && all.loopToday().skimShort == 1000000 - raised, "short, logged: " + n(raised));
    expect(all.cash("aa") >= all.necessitiesAHead() && all.cash("bb") >= all.necessitiesAHead() && all.cash("cc") == 5 &&
               all.cash("treasury") >= 2000,
           "nobody under its line: a day's necessities kept, the treasury its cap");
    // Never fund:relief itself, and deterministic.
    const auto again = all.skim(10, "test");
    expect(!all.loopToday().skimPayers.count(Society::ReliefFund) && again == 0, "never fund:relief; nothing left to give");
    auto x = city({at("aa", 600), at("bb", 600), at("cc", 600)}, 2000, {{"treasuryCap", 2000}});
    auto y = city({at("aa", 600), at("bb", 600), at("cc", 600)}, 2000, {{"treasuryCap", 2000}});
    x.skim(301, "test");
    y.skim(301, "test");
    expect(x.cash("aa") == y.cash("aa") && x.cash("bb") == y.cash("bb") && x.cash("cc") == y.cash("cc") &&
               x.cash("aa") + x.cash("bb") + x.cash("cc") == 1800 - 301,
           "ties broken the same way every time");
    // Never Hungry's 17:00 skim leaves the treasury out (tonight's wage support not yet known); 18:00's may take its spare.
    auto t = city({at("aa", 600)}, 9000, {{"treasuryCap", 2000}});
    const auto treasury = t.cash("treasury");
    t.skim(200, "test", false);
    expect(t.cash("treasury") == treasury && !t.loopToday().skimPayers.count("treasury"), "before 18:00, never the treasury");
    t.skim(200, "test");
    expect(t.cash("treasury") < treasury && t.cash("treasury") >= 2000, "at 18:00, the treasury's spare over its cap: " + n(t.cash("treasury")));
}

const std::vector<Kin> FamilyKin{{"aa_mother", "bb_kid", "parent"}, {"aa_mother", "cc_kid", "parent"}};
std::vector<ResidentSpec> family(std::int64_t purse)
{
    return {person("fish", "merchant", "fishmonger at the Fish Hall", {"hall", 2.5, 2.5}, "home_fish"),
            person("aa_mother", "civilian", "keeps the house", {"home_fam", 2.5, 2.5}, "home_fam", 35, purse),
            person("bb_kid", "civilian", "at lessons", {"school", 2.5, 2.5}, "home_fam", 9, 0),
            person("cc_kid", "civilian", "at lessons", {"school", 4.5, 2.5}, "home_fam", 7, 0), at("zz_rich", 3000)};
}

void neverBroke()
{
    for (const bool on : {false, true})
    {
        auto s = city(family(0), 3000, {{"neverBroke", on ? 1 : 0}, {"treasuryCap", 2000}}, FamilyKin);
        const auto money = s.moneySupply();
        run(s, 1 + 17.9 / 24, 1 + 18.5 / 24);
        const auto held = s.cash("aa_mother") + s.cash("bb_kid") + s.cash("cc_kid");
        const auto& d = s.loopToday();
        if (!on)
        {
            expect(held == 30 && d.neverBrokePennies == 0, "off: the day's alms only, 10p a head: " + n(held));
            continue;
        }
        expect(held == 3 * s.necessitiesAHead(), "on: topped up to exactly the next day's necessities: " + n(held) + " for " +
                                                     n(3 * s.necessitiesAHead()));
        expect(d.neverBrokeHomes == 1 && d.neverBrokePennies == held - 30, "the alms first, then just enough: " + n(d.neverBrokePennies));
        expect(d.skimFor.at("never broke") == d.relief.at("necessities") + s.cash(Society::ReliefFund),
               "the skim raised exactly what was paid out");
        expect(s.cash(Society::ReliefFund) == 0 && s.conserved() && s.moneySupply() == money, "nothing left in the fund; conserved");
        expect(s.cash("zz_rich") >= 3000 - d.skim && s.cash("zz_rich") > 3 * s.necessitiesAHead(), "the rich household over the line: paid, not given");
    }
}

void neverHungry()
{
    constexpr double NourishADay = .0035 * 14400 / 1.1;
    for (const bool on : {false, true})
    {
        auto s = city(family(0), 3000, {{"neverHungry", on ? 1 : 0}, {"treasuryCap", 2000}}, FamilyKin);
        const auto hall = s.tillOf("fish");
        s.create(hall, "fish", 200, "test: a full fish hall");
        const auto money = s.moneySupply();
        run(s, 1 + 16.9 / 24, 1 + 17.2 / 24);
        const auto& d = s.loopToday();
        if (!on)
        {
            expect(d.neverHungryPieces == 0 && d.skim == 0, "off: nothing");
            continue;
        }
        const auto line = items::cityLoop().shortDays * NourishADay;
        for (const char* m : {"aa_mother", "bb_kid", "cc_kid"})
        {
            double have = 0;
            for (const auto& [item, count] : s.account(m)->stock)
                have += Society::edible(item) ? double(Society::nourishment(item)) * count : 0;
            expect(have >= line - 1e-9 && have < line + Society::nourishment("fish"), std::string(m) + " given food up to a day's and no more: " + n(have));
        }
        expect(d.neverHungryPieces > 0 && d.householdsShortAfter == 0 && d.neverHungryHomes == 1, "the homes short: none after");
        expect(d.neverHungryPennies > 0 && d.relief.at("food") == d.neverHungryPennies,
               "bought from the hall at the stable price, and the hall paid: " + n(d.neverHungryPennies) + "p for " + n(d.neverHungryPieces) +
                   " at " + n(s.stablePrice("fish", 1)) + ", the hall's fish " + n(s.account(hall)->stock.at("fish")));
        expect(d.skimFor.at("never hungry") == d.relief.at("food") + s.cash(Society::ReliefFund) && s.conserved() && s.moneySupply() == money,
               "the skim raised exactly the bill; conserved");
    }
}

void theLeversAtTheEnds()
{
    auto left = city(town(), 9000, {{"steer", 1}, {"steerLean", 0}});
    run(left, 1 + 17.9 / 24, 1 + 18.5 / 24);
    expect(left.state().loop.steer.push == -1 && left.rule("treasuryCap") == 1500 && left.rule("surplusKeep") == 0 &&
               left.rule("surplusAlmsTo") == 80 && left.steerLever("shareOut") == 30 && left.steerLever("postsShare") == 50,
           "at the far left: the cap 1,500, the keep 0, the poorest to 80p, the share-out 30%, the hands' share 50%");
    expect(left.conserved(), "conserved");
    auto right = city(town(), 9000, {{"steer", 1}, {"steerLean", 200}});
    run(right, 1 + 17.9 / 24, 1 + 18.5 / 24);
    expect(right.state().loop.steer.push == 1 && right.steerLever("groundRent") == 45 && right.steerLever("rentScale") == 140 &&
               right.steerLever("dividend") == 40 && right.steerLever("richAbove") == 600 && right.rule("treasuryCap") == 2000,
           "at the far right: the ground rent 45%, the rents 140%, the dividend 40%, the rich hold 600p; the cap the world's");
    expect(right.state().loop.steer.tollLean == -2 && right.gateTollNow() == left.gateTollNow() - 5, "the toll lean -2 at the right, +3 at the left");
    // Off again: the world's own rules at once.
    setRules(left, {{"steer", 0}});
    expect(left.rule("treasuryCap") == 2000 && left.rule("surplusKeep") == 4000 && left.steerLever("shareOut") == 0, "off: the world's own");
    // The toll lean's guard: a treasury under its floor keeps the toll from falling.
    auto poor = city(town(), 1000, {{"steer", 1}, {"steerLean", 200}});
    run(poor, 1 + 17.9 / 24, 1 + 18.5 / 24);
    expect(poor.state().loop.steer.tollLean == 0, "a treasury under 1,400p: the lean not under 0: " + n(poor.state().loop.steer.tollLean));
    // Judged each night, not only when the push moves (the runs of 2026-11-06): the slider set far right while the treasury
    // was over its floor, which the night's money then took under it. The lean goes back half a point a night to 0, long
    // before the first judging (day 56), and the money is conserved.
    auto fell = city(town(), 1500, {});
    run(fell, 1 + 7.9 / 24, 1 + 17.5 / 24);
    setRules(fell, {{"steer", 1}, {"steerLean", 200}});
    expect(fell.state().loop.steer.tollLean == -2, "over its floor: the lean -2 at once: " + n(fell.state().loop.steer.tollLean));
    fell.shift("treasury", "aa_rich", "", 0, fell.cash("treasury") - 1100, "test: the treasury drawn down");
    run(fell, 1 + 17.5 / 24, 1 + 18.5 / 24);
    expect(fell.state().loop.steer.tollLean == -1.5, "under its floor that night: the lean half a point back: " + n(fell.state().loop.steer.tollLean));
    run(fell, 1 + 18.5 / 24, 5 + 18.5 / 24);
    expect(fell.state().loop.steer.tollLean == 0 && fell.state().loop.steer.push == 1,
           "four nights on: the lean 0, the push still at the slider: " + n(fell.state().loop.steer.tollLean));
    expect(fell.conserved(), "conserved");
}

void theHandsShareAndTheGroundRent()
{
    // A barber's till well over its cap: at the left part of the keeper's profit goes to its hand; at the right part to
    // the Crown (the treasury, with the estates off). The till keeps its cap either way.
    const auto day = [](int lean) {
        auto s = city(town(), 9000, {{"steer", 1}, {"steerLean", lean}, {"estates", 0}, {"wageDrift", 0}});
        s.shift("treasury", s.tillOf("barber"), "", 0, 1000, "test: a good week");
        run(s, 1 + 17.9 / 24, 1 + 18.5 / 24);
        return s;
    };
    auto centre = day(100), left = day(0), right = day(200);
    expect(centre.loopToday().postsShare == 0 && centre.loopToday().groundRent == 0, "at the centre: neither");
    expect(left.loopToday().postsShare > 0 && left.cash("hand") > centre.cash("hand") && left.cash("barber") < centre.cash("barber"),
           "the left: the hand's share: " + n(left.loopToday().postsShare));
    expect(right.loopToday().groundRent > 0 && right.cash("barber") < centre.cash("barber"), "the right: the ground rent: " + n(right.loopToday().groundRent));
    expect(left.cash(left.tillOf("barber")) == centre.cash(centre.tillOf("barber")) &&
               right.cash(right.tillOf("barber")) == centre.cash(centre.tillOf("barber")),
           "the till keeps its cap");
    expect(left.conserved() && right.conserved(), "conserved");
}

void theShareOut()
{
    auto s = city({at("aa", 900), at("bb", 100), at("cc", 0), at("dd", 0)}, 2000, {{"steer", 1}, {"steerLean", 0}, {"treasuryCap", 2000}});
    const auto money = s.moneySupply();
    run(s, 1 + 17.9 / 24, 1 + 18.5 / 24);
    const auto& d = s.loopToday();
    expect(d.skimFor.count("share-out") && d.shareOut > 0 && d.shareOutHomes >= 2, "the share-out levels the poorest: " + n(d.shareOut));
    expect(s.cash("aa") < 900 && s.cash("cc") > 10 && std::abs(s.cash("cc") - s.cash("dd")) <= 1,
           "the rich give, the poorest raised to one level: " + n(s.cash("aa")) + ", " + n(s.cash("bb")) + ", " + n(s.cash("cc")) + ", " +
               n(s.cash("dd")));
    expect(d.skimFor.at("share-out") == d.relief.at("share-out") + s.cash(Society::ReliefFund) && s.conserved() && s.moneySupply() == money,
           "its books balance; conserved");
}

// The controller on a still world (no ticks): judged on its means once it has had steerMeanDays nights.
void theController()
{
    // Unequal households, no earners: the earners' Gini is 0, under any target left of the centre (too equal: right).
    auto make = [] { return city({at("aa", 900), at("bb", 100), at("cc", 0)}, 9000, {{"steer", 1}, {"steerLean", 50}}); };
    auto s = make();
    s.steerEconomy(1);
    const auto& st = s.state().loop.steer;
    expect(st.on && std::abs(st.push + .5) < 1e-12, "feed-forward: the push at the slider, -0.5");
    expect(s.rule("surplusKeep") == 1000 && s.rule("surplusAlmsTo") == 60 && s.steerLever("shareOut") == 12,
           "the levers at -0.5: the keep 1,000, the poorest to 60p, the share-out 12%");
    const auto [lo, hi] = s.steerBand();
    expect(std::abs(lo + .725) < 1e-12 && std::abs(hi + .275) < 1e-12, "its band: 0.225 either side at -0.5");
    std::vector<double> pushes;
    for (int day = 2; day <= 200; ++day)
    {
        s.steerEconomy(day);
        pushes.push_back(st.push);
    }
    expect(std::abs(pushes[54 - 1] + .5) < 1e-12, "no judging before its 56 nights");
    expect(std::abs(pushes[56 - 2] + .4) < 1e-12, "the first judging, day 56: the Gini over two bands off, the larger step, 0.10: " + n(pushes[54]));
    expect(std::abs(pushes[84 - 2] + .3) < 1e-12 && std::abs(pushes[112 - 2] - hi) < 1e-12 && std::abs(pushes.back() - hi) < 1e-12,
           "every 28 days a step, at most 0.10, then held at its band's edge");
    expect(!st.history.empty() && st.history.back().why.find("band's edge") != std::string::npos, "the reason: " + st.history.back().why);
    // Deterministic.
    auto t = make();
    for (int day = 1; day <= 200; ++day)
        t.steerEconomy(day);
    expect(t.state().loop.steer.history.size() == st.history.size() && t.state().loop.steer.history.back().why == st.history.back().why &&
               t.state().loop.steer.push == st.push,
           "two identical worlds, the same judgings");
    // The slider moved: the push to it at once, the judging restarting its 28 days.
    setRules(s, {{"steerLean", 150}});
    s.steerEconomy(201);
    expect(std::abs(st.push - .5) < 1e-12 && st.judged == 201 && s.steerLever("groundRent") == 20, "a slider change: feed-forward");
    // Saved and read again.
    const auto saved = wire::society(s.state());
    const auto back = wire::readSociety(saved).loop.steer;
    expect(back.on && back.push == st.push && back.lean == 150 && back.history.size() == st.history.size() && back.gini == st.gini &&
               back.history.back().why == st.history.back().why,
           "the steering's state saved and read again");
    // Off: the push 0, the world's rules.
    setRules(s, {{"steer", 0}});
    s.steerEconomy(202);
    expect(!st.on && st.push == 0 && s.rule("surplusKeep") == 4000, "off: the push 0, the world's own rules");
}

void theDeadBand()
{
    // At the centre, both measures in their bands: nothing moves (the targets set here to this world's own).
    auto& t = const_cast<items::CityLoop&>(items::cityLoop()).steering.targets;
    const auto saved = t;
    auto s = city({at("aa", 100), at("bb", 100)}, 9000, {{"steer", 1}});
    t["gini"][2] = s.giniEarners();
    t["upper"][2] = s.upperShare();
    for (int day = 1; day <= 120; ++day)
        s.steerEconomy(day);
    const auto& st = s.state().loop.steer;
    t = saved;
    expect(st.push == 0 && st.history.size() >= 3 && st.history.back().step == 0 &&
               st.history.back().why.find("in their bands") != std::string::npos,
           "the dead band: no move: " + st.history.back().why);
}
void theHungerGuard()
{
    // The hunger guard's weekly pull (the runs of 2026-11-06): with the far right's treasury, which pays the alms, far under
    // its floor (1,400p) on its mean and tonight, every seventh night the push goes back 0.10 toward the centre, before any
    // judging, never past it; with Never Hungry on, the floor is that toggle's and nothing is pulled.
    const auto hungry = [](bool fed) {
        std::map<std::string, int> rules{{"steer", 1}, {"steerLean", 200}};
        if (fed)
            rules["neverHungry"] = 1;
        auto s = city(town(), 9000, rules);
        run(s, 1 + 7.9 / 24, 1 + 18.5 / 24);
        auto state = s.state();
        state.loop.steer.days = 3650;               // (The means' 28-night rate: they stay far over for weeks.)
        state.loop.steer.treasury = 300;            // (The alms' purse far under its low, its mean and tonight.)
        expect(s.restore(state), "the state read back");
        s.shift("treasury", "aa_rich", "", 0, s.cash("treasury") - 300, "test: the treasury drawn down");
        run(s, 1 + 18.5 / 24, 14 + 18.5 / 24);      // (Days 7 and 14 are the guard's nights.)
        return s;
    };
    auto right = hungry(false);
    const auto& st = right.state().loop.steer;
    expect(std::abs(st.push - .8) < 1e-9, "two weeks far over: the push 1 -> 0.8, under its band (0.7..1 is not a floor): " + n(st.push));
    expect(!st.history.empty() && st.history.back().why.find("weekly pull") != std::string::npos, "the reason logged: " +
           (st.history.empty() ? std::string() : st.history.back().why));
    expect(right.steerLever("dividend") < 40 && right.steerLever("richAbove") < 600, "the right's levers with it");
    expect(right.conserved(), "conserved");
    auto fed = hungry(true);
    expect(fed.state().loop.steer.push == 1, "Never Hungry on: no pull: " + n(fed.state().loop.steer.push));
}
} // namespace

int main()
{
    try
    {
        catalogue();
        theRulesAndTheTable();
        theCentreLeavesTodayAlone();
        theSkim();
        neverBroke();
        neverHungry();
        theLeversAtTheEnds();
        theHandsShareAndTheGroundRent();
        theShareOut();
        theController();
        theDeadBand();
        theHungerGuard();
    }
    catch (const std::exception& error)
    {
        std::cerr << "steer_tests FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "steer_tests: " << checks << " checks passed\n";
    return 0;
}
