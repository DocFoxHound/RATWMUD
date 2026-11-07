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
    expect(items::steerAt(t.levers.at("richAbove"), 1, 40) == 150 && items::steerAt(t.levers.at("richAbove"), .75, 40) == 125,
           "the rich hold 150p a head at the far right");
    expect(items::steerAt(t.levers.at("spendAbove"), -1, 40) == 25 && items::steerAt(t.levers.at("spendAbove"), 0, 40) == 40 &&
               items::steerAt(t.levers.at("tollLean"), 1, 0) == 0 && items::steerAt(t.levers.at("tollLean"), -1, 0) == 1,
           "the spending keep 25p a head at the far left; the toll lean +1 at the far left, none right of the centre");
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
               std::abs(items::steerAt(t.targets.at("upper"), .25, std::nan("")) - 15.75) < 1e-9,
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
    const auto line = w.comfortableAHead();
    const auto days = std::int64_t(items::cityLoop().steering.comfortable);   // (2 since doc 75's D1; 4 before.)
    expect(line == std::max<std::int64_t>(w.comfortsAbove(), days * w.necessitiesAHead()), "the comfortable line: the table's days of necessities a head: " + n(line));
    const auto got = w.skim(300, "test");
    const auto fromA = 600 - w.cash("aa"), fromB = 300 - w.cash("bb");
    expect(got == 300 && fromA + fromB == 300 && fromA > fromB && fromB > 0 && fromA <= (600 - line) / 2 && fromB <= (300 - line) / 2,
           "both pay, the richer more, neither over half its spare: " + n(fromA) + " and " + n(fromB));
    std::int64_t byGroup = 0;
    for (const auto& [group, coins] : w.loopToday().skimBy)
        byGroup += group.rfind("households: ", 0) == 0 ? coins : 0;
    expect(byGroup == 300 && w.loopToday().skimPayers.size() == 2 && w.loopToday().skimFor.at("test") == 300,
           "logged by payer and purpose");
    // Asked for more than anyone has spare: the second pass to the whole of each spare (never under the comfortable
    // line: the second pass no longer drops a household's line), then short, logged.
    auto all = city({at("aa", 600), at("bb", 300), at("cc", 5)}, 2000, {{"treasuryCap", 2000}});
    const auto raised = all.skim(1000000, "test");
    expect(raised < 1000000 && all.loopToday().skimShort == 1000000 - raised, "short, logged: " + n(raised));
    expect(all.cash("aa") == all.comfortableAHead() && all.cash("bb") == all.comfortableAHead() && all.cash("cc") == 5 &&
               all.cash("treasury") >= 2000,
           "nobody under its line: the comfortable line kept, the treasury its cap: " + n(all.cash("aa")));
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
    // The skim reworked: Never Broke after the alms, as credit toward necessities, not cash.
    for (const bool on : {false, true})
    {
        auto s = city(family(0), 3000, {{"neverBroke", on ? 1 : 0}, {"treasuryCap", 2000}}, FamilyKin);
        s.create(s.tillOf("fish"), "fish", 200, "test: a full fish hall");
        const auto money = s.moneySupply();
        run(s, 1 + 17.9 / 24, 1 + 18.5 / 24);
        const auto held = s.cash("aa_mother") + s.cash("bb_kid") + s.cash("cc_kid");
        const auto& d = s.loopToday();
        expect(held == 30, std::string(on ? "on" : "off") + ": the purses hold the day's alms only, 10p a head (no loose cash): " + n(held));
        if (!on)
        {
            expect(d.neverBrokePennies == 0 && d.neverBrokeCredit == 0 && s.state().loop.steer.credit.empty(), "off: nothing");
            continue;
        }
        const auto& credit = s.state().loop.steer.credit;
        const auto want = 3 * s.necessitiesAHead() - 30;
        expect(credit.size() == 1 && credit.begin()->second == want && d.neverBrokeCredit == want && d.neverBrokeHomes == 1,
               "after the alms, credit for just what they left short: " + n(d.neverBrokeCredit) + " for " + n(want));
        expect(d.neverBrokePennies == 0 && !d.relief.count("necessities"), "nothing spent yet: the shops are shut");
        expect(s.cash(Society::ReliefFund) == want && s.neverBrokeOwed() == want && d.skimFor.at("never broke") == want,
               "the skim raised the credit; it waits in fund:relief");
        expect(s.cash("zz_rich") >= 3000 - d.skim && s.cash("zz_rich") >= s.comfortableAHead(), "the rich household over the line: paid, not given");
        expect(s.conserved() && s.moneySupply() == money, "conserved");
        // The next day: the credit spent at the shops on the household's food and needs, never into a purse.
        run(s, 1 + 18.5 / 24, 2 + 17.9 / 24);
        const auto& e = s.loopToday();
        const auto left = s.neverBrokeOwed();
        const auto spent = e.neverBrokePennies;
        expect(spent > 0 && spent == e.relief.at("necessities") && spent + left == want,
               "spent at the shops, as relief for necessities: " + n(spent) + " of " + n(want) + ", " + n(left) + " left");
        expect(s.cash("aa_mother") + s.cash("bb_kid") + s.cash("cc_kid") <= 30, "the purses never hold the credit");
        expect(s.cash(Society::ReliefFund) == left, "the fund holds what is left of it");
        // The next night: the unused credit lapses and is not drawn again; the skim raises only the rest.
        const auto fund = s.cash(Society::ReliefFund);
        run(s, 2 + 17.9 / 24, 2 + 18.5 / 24);
        const auto& f = s.loopToday();
        const auto raised = f.skimFor.count("never broke") ? f.skimFor.at("never broke") : 0;
        expect(f.neverBrokeCredit > 0 && raised == std::max<std::int64_t>(0, f.neverBrokeCredit - fund),
               "unused credit not drawn again: tonight's credit " + n(f.neverBrokeCredit) + ", raised " + n(raised) + ", " + n(fund) + " already held");
        expect(s.cash(Society::ReliefFund) == s.neverBrokeOwed() && s.conserved() && s.moneySupply() == money, "conserved");
        // Turned off: the credit lapses at once.
        setRules(s, {{"neverBroke", 0}});
        run(s, 2 + 18.5 / 24, 3 + 18.5 / 24);
        expect(s.state().loop.steer.credit.empty() && s.conserved(), "off: its credit lapses; conserved");
    }
}

// Rich households of each kind: an upper-class one, a keeper's (the barber's), a middle-class one and a lower-class one
// (the hand, in someone else's shop), each alone and each holding `purse`.
std::vector<ResidentSpec> classes(std::int64_t purse)
{
    return {person("barber", "merchant", "barber at The Close Trim", {"shop", 2.5, 2.5}, "home_barber", 40, purse),
            person("hand", "civilian", "assistant at The Close Trim", {"shop", 3.5, 2.5}, "home_hand", 30, purse),
            person("lord", "civilian", "at home in the heights", {"home_lord", 2.5, 2.5}, "home_lord", 50, purse),
            at("mid", purse)};
}

void theSkimOrder()
{
    auto s = city(classes(900), 9000, {{"treasuryCap", 2000}, {"estates", 0}});
    expect(s.classOf("lord") == "upper" && s.classOf("hand") == "lower" && s.classOf("mid") == "middle" && s.classOf("barber") == "middle",
           "the classes: " + s.classOf("lord") + ", " + s.classOf("hand") + ", " + s.classOf("mid") + ", " + s.classOf("barber"));
    const auto ids = [&](int step, bool treasury) {
        std::string out;
        for (const auto& h : s.skimStep(step, treasury))
            out += (out.empty() ? "" : ",") + h.id;
        return out;
    };
    expect(ids(2, true) == "treasury" && ids(2, false).empty(), "step 2 the treasury (never at 17:00)");
    expect(ids(3, true) == "home home_lord" && ids(4, true) == "home home_barber" && ids(5, true) == "home home_mid",
           "steps 3-5: upper, the keeper's, the middle: " + ids(3, true) + " / " + ids(4, true) + " / " + ids(5, true));
    for (int step = 1; step <= Society::SkimSteps; ++step)
        expect(ids(step, true).find("home_hand") == std::string::npos && ids(step, true).find("till:") == std::string::npos,
               "a lower-class household never pays, nor a till");
    const auto money = s.moneySupply();
    // At 18:00 the treasury's spare first, with the households untouched.
    const auto treasury = s.cash("treasury");
    s.skim(100, "test");
    expect(treasury - s.cash("treasury") == 100 && s.cash("lord") == 900 && s.cash("barber") == 900 && s.cash("mid") == 900,
           "the treasury (step 2) before any household");
    // At 17:00 (no treasury): the upper first, up to half its idle spare; then the keeper's; then the middle, last.
    // (The cap is each skim's: half of each holder's idle spare at that moment.)
    const auto line = s.comfortableAHead(), half = (900 - line) / 2;
    s.skim(half, "test", false);
    expect(900 - s.cash("lord") == half && s.cash("barber") == 900 && s.cash("mid") == 900, "the upper first: " + n(900 - s.cash("lord")));
    auto o = city(classes(900), 9000, {{"treasuryCap", 2000}, {"estates", 0}});
    o.skim(half + 20, "test", false);
    expect(900 - o.cash("lord") == half && 900 - o.cash("barber") == 20 && o.cash("mid") == 900,
           "then the keeper's savings: " + n(900 - o.cash("barber")));
    auto l = city(classes(900), 9000, {{"treasuryCap", 2000}, {"estates", 0}});
    l.skim(2 * half + 10, "test", false);
    expect(900 - l.cash("lord") == half && 900 - l.cash("barber") == half && 900 - l.cash("mid") == 10 && l.cash("hand") == 900,
           "the middle last; the lower never: " + n(900 - l.cash("mid")));
    // Short of the first pass (every cap reached): the second takes the rest of each spare, in the same order, down to
    // the comfortable line and never under it; the lower household still pays nothing.
    s.skim(1000000, "test", false);
    expect(s.cash("lord") == line && s.cash("barber") == line && s.cash("mid") == line && s.cash("hand") == 900,
           "the second pass to the comfortable line: " + n(s.cash("lord")) + ", " + n(s.cash("mid")) + "; the hand " + n(s.cash("hand")));
    std::int64_t groups = 0;
    for (const auto& [group, coins] : s.loopToday().skimBy)
        groups += coins;
    expect(s.loopToday().skimBy.count("households: upper") && s.loopToday().skimBy.count("keepers") &&
               s.loopToday().skimBy.count("households: middle") && !s.loopToday().skimBy.count("households: lower") &&
               groups == s.loopToday().skim,
           "logged by the new groups");
    expect(s.cash(Society::ReliefFund) == s.loopToday().skim && s.conserved() && s.moneySupply() == money, "conserved");
    // Middle-class savings pay only over the comfortable line: 30p over, 30p taken, however much is asked.
    auto m = city({at("mm", 0)}, 2000, {{"treasuryCap", 2000}});
    m.shift("treasury", "mm", "", 0, m.comfortableAHead() + 30, "test: just over the line");
    expect(m.skim(500, "test") == 30 && m.cash("mm") == m.comfortableAHead(), "a middle household pays only over its line: 30p");
    auto under = city({at("mm", 0)}, 2000, {{"treasuryCap", 2000}});
    under.shift("treasury", "mm", "", 0, under.comfortableAHead(), "test: at the line");
    expect(under.skim(500, "test") == 0 && under.cash("mm") == under.comfortableAHead(), "at the line: nothing");
    // The estates (step 1) last of all since doc 75's D1 (the table's skimOrder: the treasury, upper, keepers, middle,
    // estates): with the treasury's spare, the treasury pays and the estate keeps its money.
    const auto& order = items::cityLoop().steering.skimOrder;
    expect(order == std::array<int, 5>{2, 3, 4, 5, 1}, "the table's order: the treasury, upper, keepers, middle, the estates last");
    auto e = city(town(), 1000000, {{"treasuryCap", 2000}});
    const auto f = Society::estateFund(items::cityLoop().estates.front());
    expect(e.account(f) && e.shift("treasury", f, "", 0, 500000, "test: a rich estate"), "an estate given money: " + f);
    const auto steps = e.skimStep(1, true);
    expect(!steps.empty() && steps.front().group.rfind("estates", 0) == 0, "step 1 the estates");
    const auto t = e.cash("treasury"), held = e.cash(f);
    e.skim(50, "test");
    expect(t - e.cash("treasury") == 50 && e.cash(f) == held, "the treasury before the estates: " + e.loopToday().skimBy.begin()->first);
    // With the treasury at its cap, a skim larger than every household's spare reaches the estates, last.
    auto g = city(town(), 2000, {{"treasuryCap", 2000}});
    const auto gf = Society::estateFund(items::cityLoop().estates.front());
    const bool spare = g.account(gf) && !g.skimStep(1, true).empty();
    g.skim(100000000, "test");
    std::int64_t estates = 0;
    for (const auto& [group, coins] : g.loopToday().skimBy)
        estates += group.rfind("estates", 0) == 0 ? coins : 0;
    expect(!spare || estates > 0, "the estates pay last, when everyone before them is spent: " + n(estates));
    expect(g.conserved(), "conserved");
    expect(e.conserved(), "conserved");
}

void idleBeforeBusy()
{
    // Two middle households: aa's savings have sat; bb's came today, more than aa's. The first pass takes from aa.
    auto s = city({at("aa", 600), at("bb", 0)}, 9000, {{"treasuryCap", 2000}, {"estates", 0}});
    s.skimIdleNight(1);                             // (The first night: each at its spare; bb has none.)
    expect(s.state().loop.steer.idle.at("home home_aa") == 600 - s.comfortableAHead() && !s.state().loop.steer.idle.count("home home_bb"),
           "seeded at the spare");
    s.shift("treasury", "bb", "", 0, 900, "test: bb's takings today");
    s.skim(40, "test", false);
    expect(600 - s.cash("aa") == 40 && s.cash("bb") == 900, "the idle purse before the busy one, though the busy one is richer");
    // Its idle spare rises a 28th of the gap a night: after a few weeks bb's money has sat, and bb (the richer) pays first.
    for (int day = 2; day <= 90; ++day)
        s.skimIdleNight(day);
    const auto bbIdle = s.state().loop.steer.idle.at("home home_bb");
    const auto bbSpare = 900 - s.comfortableAHead();
    expect(bbIdle > bbSpare * 9 / 10 && bbIdle < bbSpare && bbIdle > s.state().loop.steer.idle.at("home home_aa"),
           "after 89 nights most of bb's spare counts as idle (a 28th of the gap a night): " + n(bbIdle) + " of " + n(bbSpare));
    auto one = city({at("aa", 0)}, 9000, {{"treasuryCap", 2000}, {"estates", 0}});
    one.skimIdleNight(1);
    one.shift("treasury", "aa", "", 0, one.comfortableAHead() + 280, "test: 280p over the line");
    one.skimIdleNight(2);
    expect(one.state().loop.steer.idle.at("home home_aa") == 10, "a night: a 28th of 280p, 10p: " + n(one.state().loop.steer.idle.at("home home_aa")));
    one.skimIdleNight(2);
    expect(one.state().loop.steer.idle.at("home home_aa") == 10, "once a night only");
    const auto bb = s.cash("bb"), aa = s.cash("aa");
    s.skim(40, "test", false);
    expect(bb - s.cash("bb") == 40 && s.cash("aa") == aa, "now the richer pays");
    // Spent, its idle spare falls at once.
    s.shift("bb", "treasury", "", 0, 500, "test: bb spends");
    s.skimIdleNight(91);
    expect(s.state().loop.steer.idle.at("home home_bb") == std::max<std::int64_t>(0, s.cash("bb") - s.comfortableAHead()), "falls at once");
    // Busy money still pays in the second pass, when the idle money is not enough.
    auto b = city({at("aa", 0)}, 9000, {{"treasuryCap", 2000}, {"estates", 0}});
    b.skimIdleNight(1);
    b.shift("treasury", "aa", "", 0, 500, "test: fresh money");
    expect(b.skim(20, "test", false) == 20 && b.cash("aa") == 480, "the second pass: fresh money pays when nothing idle can");
    expect(s.conserved() && b.conserved(), "conserved");
}

void theTillsGuard()
{
    // With Never Broke on, while the tills short's 28-night mean is well over the centre's level, the first pass takes the
    // estates' and the treasury's whole spare before any household's.
    // (The trip from the table: 25 x 1.15 = 28.75 before doc 75's D1 turned the guard off, tillsShortOver 1000: 275.)
    const auto& table = items::cityLoop().steering;
    const double trip = table.tillsShortNormal * (1 + table.tillsShortOver / 100.);
    const auto make = [trip](bool high) {
        auto s = city({at("aa", 2000)}, 4000, {{"treasuryCap", 2000}, {"estates", 0}, {"neverBroke", 1}});
        auto state = s.state();
        state.loop.steer.tillsDays = 28;
        state.loop.steer.tillsShort = high ? trip + 10 : trip - 1;
        expect(s.restore(state), "the state read back");
        return s;
    };
    auto plain = make(false), guarded = make(true);
    expect(!plain.skimGuardOn() && guarded.skimGuardOn(), "the guard on over its trip, off under: " + n(trip));
    const auto spare = guarded.cash("treasury") - 2000;
    plain.skim(spare, "test");
    guarded.skim(spare, "test");
    expect(plain.cash("aa") < 2000 && plain.cash("treasury") > 2000, "unguarded: half the treasury's spare, then the household");
    expect(guarded.cash("aa") == 2000 && guarded.cash("treasury") == 2000 && guarded.loopToday().skimGuard,
           "guarded: the treasury's whole spare first, the household untouched");
    // Off with Never Broke off, and before its mean has had 28 nights.
    setRules(guarded, {{"neverBroke", 0}});
    expect(!guarded.skimGuardOn(), "only while Never Broke is on");
    auto young = make(true);
    auto state = young.state();
    state.loop.steer.tillsDays = 27;
    young.restore(state);
    expect(!young.skimGuardOn(), "not before 28 nights");
    // Followed each night Never Broke runs.
    auto s = city(family(0), 3000, {{"neverBroke", 1}, {"treasuryCap", 2000}}, FamilyKin);
    run(s, 1 + 17.9 / 24, 2 + 18.5 / 24);
    expect(s.state().loop.steer.tillsDays == 2 && s.state().loop.steer.tillsShort >= 0, "the mean followed nightly: " + n(s.state().loop.steer.tillsDays));
    // Turned off, the mean starts again: turned on weeks later, the guard is never judged on nights from before.
    {
        auto st = s.state();
        st.loop.steer.tillsDays = 40;
        st.loop.steer.tillsShort = trip + 60;
        expect(s.restore(st), "the state read back");
    }
    expect(s.skimGuardOn(), "a high mean: the guard on");
    setRules(s, {{"neverBroke", 0}, {"neverHungry", 0}});
    run(s, 2 + 18.5 / 24, 3 + 18.5 / 24);
    expect(s.state().loop.steer.tillsDays == 0 && s.state().loop.steer.tillsShort < 0, "off: the tills short's mean cleared");
    expect(s.state().loop.steer.idle.empty() && s.state().loop.steer.idleDay < 0, "both floors off: the idle spares cleared");
    setRules(s, {{"neverBroke", 1}});
    expect(!s.skimGuardOn(), "on again: no guard until its mean has had 28 nights again");
    expect(plain.conserved() && guarded.conserved(), "conserved");
}

void theSkimSavedAndDeterministic()
{
    const auto make = [] {
        auto s = city(family(0), 3000, {{"neverBroke", 1}, {"neverHungry", 1}, {"treasuryCap", 2000}}, FamilyKin);
        s.create(s.tillOf("fish"), "fish", 200, "test: a full fish hall");
        run(s, 1 + 7.9 / 24, 3 + 18.5 / 24);
        return s;
    };
    auto a = make(), b = make();
    for (const auto& [id, acc] : a.state().accounts)
        expect(b.cash(id) == acc.cash, "two identical worlds, the same money: " + id);
    const auto& sa = a.state().loop.steer;
    const auto& sb = b.state().loop.steer;
    expect(sa.idle == sb.idle && sa.credit == sb.credit && sa.tillsShort == sb.tillsShort && sa.idleDay == sb.idleDay,
           "the same idle spares, credit and mean");
    expect(sa.idleDay == 3 && !sa.idle.empty() && !sa.credit.empty() && sa.tillsDays == 3, "the new state there: " + n(double(sa.idle.size())));
    const auto back = wire::readSociety(wire::society(a.state())).loop.steer;
    expect(back.idle == sa.idle && back.credit == sa.credit && back.idleDay == sa.idleDay && back.tillsShort == sa.tillsShort &&
               back.tillsDays == sa.tillsDays,
           "saved and read again");
    expect(a.conserved() && a.cash(Society::ReliefFund) >= a.neverBrokeOwed(), "conserved; the fund holds the credit");
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
    expect(right.state().loop.steer.push == 1 && right.steerLever("groundRent") == 30 && right.steerLever("rentScale") == 125 &&
               right.steerLever("dividend") == 30 && right.steerLever("richAbove") == 150 && right.rule("treasuryCap") == 2000,
           "at the far right: the ground rent 30%, the rents 125%, the dividend 30%, the rich hold 150p; the cap the world's");
    expect(right.state().loop.steer.tollLean == 0 && right.gateTollNow() == left.gateTollNow() - 1, "the toll lean 0 at the right, +1 at the left");
    // Off again: the world's own rules at once.
    setRules(left, {{"steer", 0}});
    expect(left.rule("treasuryCap") == 2000 && left.rule("surplusKeep") == 4000 && left.steerLever("shareOut") == 0, "off: the world's own");
    // The toll lean's guard: a treasury under its floor keeps the toll from falling. (Since Tune round 1 the table leans
    // the toll only up; the guard is kept for a table that leans it down, set here as the first build had it.)
    auto& levers = const_cast<items::CityLoop&>(items::cityLoop()).steering.levers;
    const auto table = levers;
    levers["tollLean"] = {3, 1.5, 0, -1, -2};
    auto poor = city(town(), 600, {{"steer", 1}, {"steerLean", 200}});
    run(poor, 1 + 17.9 / 24, 1 + 18.5 / 24);
    expect(poor.state().loop.steer.tollLean == 0, "a treasury under 800p: the lean not under 0: " + n(poor.state().loop.steer.tollLean));
    // Judged each night, not only when the push moves (the runs of 2026-11-06): the slider set far right while the treasury
    // was over its floor, which the night's money then took under it. The lean goes back half a point a night to 0, long
    // before the first judging (day 56), and the money is conserved.
    auto fell = city(town(), 1500, {});
    run(fell, 1 + 7.9 / 24, 1 + 17.5 / 24);
    setRules(fell, {{"steer", 1}, {"steerLean", 200}});
    expect(fell.state().loop.steer.tollLean == -2, "over its floor: the lean -2 at once: " + n(fell.state().loop.steer.tollLean));
    fell.shift("treasury", "aa_rich", "", 0, fell.cash("treasury") - 600, "test: the treasury drawn down");
    run(fell, 1 + 17.5 / 24, 1 + 18.5 / 24);
    expect(fell.state().loop.steer.tollLean == -1.5, "under its floor that night: the lean half a point back: " + n(fell.state().loop.steer.tollLean));
    run(fell, 1 + 18.5 / 24, 5 + 18.5 / 24);
    expect(fell.state().loop.steer.tollLean == 0 && fell.state().loop.steer.push == 1,
           "four nights on: the lean 0, the push still at the slider: " + n(fell.state().loop.steer.tollLean));
    expect(fell.conserved(), "conserved");
    levers = table;
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
    // With the estates on, the Crown's estate has it; with the treasury under its floor (800p), the treasury does (Tune
    // round 1), booked apart.
    const auto estates = [](std::int64_t treasury) {
        auto s = city(town(), treasury, {{"steer", 1}, {"steerLean", 200}, {"wageDrift", 0}});
        s.shift("treasury", s.tillOf("barber"), "", 0, std::min<std::int64_t>(1000, treasury / 2), "test: a good week");
        run(s, 1 + 17.9 / 24, 1 + 18.5 / 24);
        return s;
    };
    auto rich = estates(9000), poor = estates(1200);
    expect(rich.loopToday().groundRent > 0 && rich.loopToday().groundRentTreasury == 0, "the treasury over its floor: the ground rent the Crown's estate's: " +
           n(rich.loopToday().groundRent));
    expect(poor.loopToday().groundRent > 0 && poor.loopToday().groundRentTreasury == poor.loopToday().groundRent,
           "the treasury under its floor: the ground rent the treasury's: " + n(poor.loopToday().groundRentTreasury) + " of " + n(poor.loopToday().groundRent));
    expect(rich.conserved() && poor.conserved(), "conserved");
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
    expect(std::abs(st.push - .5) < 1e-12 && st.judged == 201 && s.steerLever("groundRent") == 15, "a slider change: feed-forward");
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
    // its low (500p) on its mean and tonight, every seventh night the push goes back 0.10 toward the centre, before any
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
    expect(right.steerLever("dividend") < 30 && right.steerLever("richAbove") < 150, "the right's levers with it");
    expect(right.conserved(), "conserved");
    auto fed = hungry(true);
    expect(fed.state().loop.steer.push == 1, "Never Hungry on: no pull: " + n(fed.state().loop.steer.push));
}

void theDriftAndTheFloors()
{
    // Tune round 1: the toll drift's target with the steering on is never over the steered ceiling (the cap and its keep),
    // so at the far left the drift judges the treasury where the surplus holds it; at the centre it is the world's own.
    {
        auto left = city(town(), 9000, {{"steer", 1}, {"steerLean", 0}});
        left.steerEconomy(1);
        auto centre = city(town(), 9000, {{"steer", 1}});
        centre.steerEconomy(1);
        const Society off(Roster::None);
        expect(left.rule("treasuryTarget") == 1500 && left.rule("treasuryCap") + left.rule("surplusKeep") == 1500,
               "the far left: the drift's target 1,500p, the cap and its keep: " + n(left.rule("treasuryTarget")));
        expect(centre.rule("treasuryTarget") == 5500 && off.rule("treasuryTarget") == 5500,
               "the centre and the steering off: the world's 5,500p");
        setRules(left, {{"steer", 0}});
        left.steerEconomy(2);
        expect(left.rule("treasuryTarget") == 5500, "turned off: the world's own again");
    }
    // With Never Broke on, a Gini under its band is the floor's levelling: no lean right against it. Off, the same world
    // leans right.
    auto& t = const_cast<items::CityLoop&>(items::cityLoop()).steering.targets;
    const auto saved = t;
    const auto judged = [&](bool broke) {
        std::map<std::string, int> rules{{"steer", 1}};
        if (broke)
            rules["neverBroke"] = 1;
        auto s = city({at("aa", 100), at("bb", 100)}, 9000, rules);
        t["gini"][2] = s.giniEarners() + .2;        // (Far too equal for the centre's band.)
        t["upper"][2] = s.upperShare();
        for (int day = 1; day <= 120; ++day)
            s.steerEconomy(day);
        t = saved;
        return s.state().loop.steer;
    };
    const auto broke = judged(true), plain = judged(false);
    expect(broke.push == 0 && !broke.history.empty() && broke.history.back().why.find("Never Broke") != std::string::npos,
           "Never Broke on: the Gini under its band not steered against: " + (broke.history.empty() ? std::string() : broke.history.back().why));
    expect(plain.push > 0, "Never Broke off: the same world leans right: " + n(plain.push));
}
} // namespace

// Doc 75 (3.4): G, the Gini of cash a head over all households weighted by heads, and F, of pantry days a head.
void theMeasures()
{
    auto s = city({at("aa", 600), at("bb", 300), at("cc", 0)}, 2000, {});
    // Three households of one: the pairs' differences 300 + 600 + 300 = 1200, over 3 heads x 900p: 0.444.
    expect(std::abs(s.giniHeads() - 1200. / 2700.) < 1e-9, "G over three single households: " + n(s.giniHeads()));
    auto e = city({at("aa", 100), at("bb", 100)}, 2000, {});
    expect(std::abs(e.giniHeads()) < 1e-12, "G is 0 when every household holds the same a head");
    expect(e.foodGini() == 0, "F is 0 when no household holds food");
    // A household of two with 600p between them is 300p a head, weighted twice: against one alone at 300p, G is 0.
    auto k = city({person("aa", "civilian", "keeps the house", {"home_ab", 2.5, 2.5}, "home_ab", 35, 600),
                   person("bb", "civilian", "keeps the house", {"home_ab", 2.5, 2.5}, "home_ab", 35, 0), at("cc", 300)},
                  2000, {});
    expect(k.households().size() == 2 && std::abs(k.giniHeads()) < 1e-9,
           "weighted by heads: two sharing a home at 300p a head and one alone at 300p: " + n(k.giniHeads()));
    auto u = city({person("aa", "civilian", "keeps the house", {"home_ab", 2.5, 2.5}, "home_ab", 35, 600),
                   person("bb", "civilian", "keeps the house", {"home_ab", 2.5, 2.5}, "home_ab", 35, 0), at("cc", 0)},
                  2000, {});
    // Heads at 300, 300 and 0: pairs (2 x 1) x 300 = 600, over 3 heads x 600p: 1/3.
    expect(std::abs(u.giniHeads() - 1. / 3) < 1e-9, "a couple at 300p a head and one alone with nothing: " + n(u.giniHeads()));
}

int main()
{
    try
    {
        catalogue();
        theRulesAndTheTable();
        theCentreLeavesTodayAlone();
        theSkim();
        neverBroke();
        theSkimOrder();
        idleBeforeBusy();
        theTillsGuard();
        theSkimSavedAndDeterministic();
        neverHungry();
        theLeversAtTheEnds();
        theHandsShareAndTheGroundRent();
        theShareOut();
        theController();
        theDeadBand();
        theHungerGuard();
        theDriftAndTheFloors();
        theMeasures();
    }
    catch (const std::exception& error)
    {
        std::cerr << "steer_tests FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "steer_tests: " << checks << " checks passed\n";
    return 0;
}
