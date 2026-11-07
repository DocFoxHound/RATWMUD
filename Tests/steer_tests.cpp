// Steering the economy (Docs/Design/74-steering-the-economy.md; Docs/Design/75-the-economy-controller.md): the rules and
// the lever table (its blended half marks); the steering at the centre leaving today's economy as it is, and off
// restoring the world's own rules; the skim (the richest first, its lines and caps, fresh each time, money conserved);
// Never Broke and Never Hungry; the levers at the slider's ends; the Levelling tithe (the Gini identity, its pennies, its
// remainders, conserved, deterministic); the Crown's charters (granted on the ramp, best first, upper keepers skipped,
// their profit to the House, handed back newest first, saved, deterministic); the slider followed and saved.
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
    expect(s.rule("steerMeanDays") == 56 && s.rule("skimMost") == 50 && s.rule("skimKeepDays") == 3, "the steering's own rules");
    for (const char* gone : {"steerJudgeDays", "steerStep", "steerStepMost", "steerWidthMid", "steerWidthEnd"})
        expect(items::ruleInfo(gone) == nullptr, std::string("the push's rules are gone (doc 75): ") + gone);
    const auto& t = items::cityLoop().steering;
    for (const char* gone : {"shareOut", "tollLean", "rentScale"})
        expect(!t.levers.count(gone), std::string("gone from the table: ") + gone);
    expect(items::steerAt(t.levers.at("levelling"), -1, 0) == 40 && items::steerAt(t.levers.at("levelling"), -.5, 0) == 20 &&
               items::steerAt(t.levers.at("levelling"), -.25, 0) == 10 && items::steerAt(t.levers.at("levelling"), 0, 0) == 0,
           "the tithe 40% at the far left, blended to 0 at the centre (its half mark null: halfway)");
    expect(items::steerAt(t.levers.at("surplusKeep"), 0, 4000) == 4000 && items::steerAt(t.levers.at("surplusKeep"), -.25, 4000) == 2500 &&
               items::steerAt(t.levers.at("surplusKeep"), -1, 4000) == 0,
           "W is the world's own: the keep 4,000 at the centre, 2,500 at -0.25, 0 at the far left");
    expect(items::steerAt(t.levers.at("treasuryCap"), -.5, 2000) == 1750 && items::steerAt(t.levers.at("treasuryCap"), .5, 2000) == 2000,
           "a null half mark blends its neighbours: the cap 1,750 at -0.5; the world's 2,000 right of the centre");
    expect(items::steerAt(t.levers.at("treasuryTarget"), 0, 5500) == 5500 && items::steerAt(t.levers.at("treasuryTarget"), -1, 5500) == 1500 &&
               items::steerAt(t.levers.at("treasuryTarget"), 1, 5500) == 850 && items::steerAt(t.levers.at("treasuryTarget"), .5, 5500) == 3175,
           "the toll drift's target: the world's 5,500p at the centre, reachable at the ends (1,500p, 850p), blended between");
    expect(items::steerAt(t.levers.at("richAbove"), 1, 40) == 150 && items::steerAt(t.levers.at("richAbove"), .75, 40) == 125,
           "the rich hold 150p a head at the far right");
    expect(items::steerAt(t.levers.at("spendAbove"), -1, 40) == 25 && items::steerAt(t.levers.at("spendAbove"), 0, 40) == 40,
           "the spending keep 25p a head at the far left");
    expect(items::steerAt(t.levers.at("charters"), 1, 0) == 33 && items::steerAt(t.levers.at("charters"), .5, 0) == 16.5 &&
               items::steerAt(t.levers.at("charters"), 0, 0) == 0 && t.chartersRampDays == 14 && std::abs(t.dividendPerCharter - .45) < 1e-12,
           "the charters 33% of the middle keepers' tills at the far right, over a 14-night ramp; the dividend 0.45 a point");
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
    auto centre = city(town(), 9000, {{"steer", 1}});
    run(centre, 1 + 17.9 / 24, 1 + 18.5 / 24);
    auto left = city(town(), 9000, {{"steer", 1}, {"steerLean", 0}});
    run(left, 1 + 17.9 / 24, 1 + 18.5 / 24);
    expect(left.state().loop.steer.push == -1 && left.rule("treasuryCap") == 1500 && left.rule("surplusKeep") == 0 &&
               left.rule("surplusAlmsTo") == 80 && left.steerLever("levelling") == 40 && left.steerLever("postsShare") == 50 &&
               left.rule("treasuryTarget") == 1500 && left.steerLever("charters") == 0,
           "at the far left: the cap 1,500, the keep 0, the poorest to 80p, the tithe 40%, the hands' share 50%, the target 1,500p");
    expect(left.conserved(), "conserved");
    auto right = city(town(), 9000, {{"steer", 1}, {"steerLean", 200}});
    run(right, 1 + 17.9 / 24, 1 + 18.5 / 24);
    expect(right.state().loop.steer.push == 1 && right.steerLever("groundRent") == 30 && right.steerLever("charters") == 33 &&
               std::abs(right.steerLever("dividend") - (15 + .45 * 33)) < 1e-9 && right.steerLever("richAbove") == 150 &&
               right.rule("treasuryCap") == 2000 && right.rule("treasuryTarget") == 850 && right.steerLever("levelling") == 0,
           "at the far right: the ground rent 30%, the charters 33%, the dividend 15% and 0.45 a point of them, the rich hold "
           "150p, the target 850p; the cap the world's: " + n(right.steerLever("dividend")));
    expect(right.gateTollNow() == centre.gateTollNow() && left.gateTollNow() == centre.gateTollNow(),
           "no toll lean at either end: the toll the drift's alone");
    // Off again: the world's own rules at once.
    setRules(left, {{"steer", 0}});
    expect(left.rule("treasuryCap") == 2000 && left.rule("surplusKeep") == 4000 && left.rule("treasuryTarget") == 5500 &&
               left.steerLever("levelling") == 0,
           "off: the world's own");
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
    expect(left.loopToday().postsShare > 0, "the left: the hand's share: " + n(left.loopToday().postsShare));
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

// The Levelling tithe (doc 75, 4.1).
void theTithe()
{
    // The Gini identity, in doubles: each value moved r of the way to the weighted mean, every gap scales by (1 - r), the
    // mean is the same, so G after is (1 - r) G before.
    const std::vector<std::pair<double, double>> purses = {{900, 1}, {300, 2}, {120, 3}, {40, 1}, {0, 2}, {2500, 1}};
    double w = 0, sum = 0;
    for (const auto& [x, k] : purses)
    {
        w += k;
        sum += x * k;
    }
    const double mu = sum / w, before = Society::giniOf(purses);
    for (const double r : {.1, .3, .4, .5, .9})
    {
        auto moved = purses;
        for (auto& [x, k] : moved)
            x = (1 - r) * x + r * mu;
        expect(std::abs(Society::giniOf(moved) - (1 - r) * before) < 1e-12,
               "G after the tithe is (1 - r) G before, at r " + n(r) + ": " + n(Society::giniOf(moved)) + " against " + n((1 - r) * before));
    }
    // In pennies: the total exactly kept, each household's new purse the floor of its want or one more, the pennies left
    // over to the largest remainders, ties in the order given.
    {
        const auto t = Society::tithed({10, 0, 0}, {1, 1, 1}, .5);   // (Wants 6.67, 1.67, 1.67: 6, 1, 1 and two pennies.)
        expect(t == std::vector<std::int64_t>{7, 2, 1}, "the remainders in order: " + n(t[0]) + ", " + n(t[1]) + ", " + n(t[2]));
        const auto u = Society::tithed({0, 0, 10}, {1, 1, 1}, .5);   // (1.67, 1.67, 6.67: the same remainders, the first two.)
        expect(u == std::vector<std::int64_t>{2, 2, 6}, "ties by the order given: " + n(u[0]) + ", " + n(u[1]) + ", " + n(u[2]));
        const auto v = Society::tithed({901, 7, 0, 33}, {2, 1, 3, 1}, .4);   // (Mean 941/7 = 134.43 a head.)
        std::int64_t total = 0;
        for (const auto c : v)
            total += c;
        const double wants[] = {901 + .4 * (2 * 941. / 7 - 901), 7 + .4 * (941. / 7 - 7), .4 * 3 * 941. / 7, 33 + .4 * (941. / 7 - 33)};
        bool near = true;
        for (std::size_t i = 0; i < 4; ++i)
            near = near && double(v[i]) >= std::floor(wants[i]) && double(v[i]) <= std::floor(wants[i]) + 1;
        expect(total == 941 && near, "the total kept, each the floor of its want or one more: " + n(v[0]) + ", " + n(v[1]) + ", " + n(v[2]) +
                                         ", " + n(v[3]));
        expect(Society::tithed({901, 7, 0, 33}, {2, 1, 3, 1}, .4) == v, "deterministic");
        expect(Society::tithed({901, 7, 0, 33}, {2, 1, 3, 1}, 0) == std::vector<std::int64_t>{901, 7, 0, 33}, "r 0 moves nothing");
        expect(Society::tithed({5, 5}, {1, 1}, .5) == std::vector<std::int64_t>{5, 5}, "all at the mean: nothing moves");
    }
    // In the city at the far left: the households over the mean pay, those under it are given, through fund:relief;
    // tills, the treasury and the estates untouched by it; G taken before it, and the money conserved.
    const auto night = [] {
        auto s = city({at("aa", 900), at("bb", 100), at("cc", 0), at("dd", 0)}, 2000, {{"steer", 1}, {"steerLean", 0}, {"treasuryCap", 2000}});
        run(s, 1 + 17.9 / 24, 1 + 18.5 / 24);
        return s;
    };
    auto s = night();
    const auto& d = s.loopToday();
    expect(d.tithe > 0 && d.titheGave == 1 && d.titheGot == 3 && s.cash("aa") < 900 && s.cash("cc") > 0 && std::abs(s.cash("cc") - s.cash("dd")) <= 1,
           "the one over the mean pays, the three under it are given, the poorest alike: " + n(d.tithe) + "; " + n(s.cash("aa")) + ", " + n(s.cash("bb")) + ", " + n(s.cash("cc")) +
               ", " + n(s.cash("dd")));
    expect(d.mGBefore > 0 && d.mGAll >= 0 && d.mGAll < d.mGBefore, "G before the tithe " + n(d.mGBefore) + ", after the night " + n(d.mGAll));
    expect(s.conserved(), "conserved");
    auto again = night();
    for (const auto& [id, a] : s.state().accounts)
        expect(again.cash(id) == a.cash, "deterministic: " + id);
    // At the centre (r 0) nothing is tithed.
    auto c = city({at("aa", 900), at("bb", 100), at("cc", 0), at("dd", 0)}, 2000, {{"steer", 1}, {"treasuryCap", 2000}});
    run(c, 1 + 17.9 / 24, 1 + 18.5 / 24);
    expect(c.loopToday().tithe == 0 && c.loopToday().mGBefore < 0 && c.cash("aa") == 900, "the centre: no tithe");
}

// The Crown's charters (doc 75, 4.2): four middle keepers' shops and a large one's, whose keeper is upper class.
std::vector<ResidentSpec> shops()
{
    std::vector<ResidentSpec> out;
    for (const char* k : {"aa_keep", "bb_keep", "cc_keep", "dd_keep"})
        out.push_back(person(k, "merchant", std::string("barber at The ") + k, {std::string("shop_") + k, 2.5, 2.5}, std::string("home_") + k, 40, 40));
    out.push_back(person("gull", "merchant", "innkeeper at the Grey Gull", {"rm_the_grey_gull_inn", 2.5, 2.5}, "home_gull", 50, 40));
    out.push_back(at("zz_mid", 100));
    return out;
}

void theCharters()
{
    auto& table = const_cast<items::CityLoop&>(items::cityLoop()).steering;
    const auto saved = table;
    table.levers["charters"] = {0, 0, 0, 50, 100};  // (All the middle keepers' tills at the far right.)
    table.chartersRampDays = 4;
    const auto make = [] {
        auto s = city(shops(), 9000, {{"steer", 1}, {"steerLean", 200}, {"wageDrift", 0}});
        LifeDay day = s.day();                      // (aa's shop in Greatly's district; bb's in a House's with no estate.)
        day.houseOf = [](const std::string& cell) {
            return cell == "shop_aa_keep" ? std::string("Greatly") : cell == "shop_bb_keep" ? std::string("Nowhere") : std::string();
        };
        s.setDay(day);
        expect(s.classOf("gull") == "upper" && s.classOf("aa_keep") == "middle", "the classes: " + s.classOf("gull") + ", " + s.classOf("aa_keep"));
        run(s, 1 + 17.9 / 24, 1 + 18.5 / 24);      // (Turned on: the ramp to 4, waiting for the keepers' profit's month.)
        auto state = s.state();
        expect(state.loop.steer.charters.empty() && state.loop.steer.chartersTo == 4 && state.loop.steer.profitNights == 1,
               "the first night: none yet, the ramp to the four middle keepers' tills: " + n(state.loop.steer.chartersTo) + ", charters " + n(double(state.loop.steer.charters.size())) + ", nights " + n(state.loop.steer.profitNights));
        state.loop.steer.profitNights = 28;         // (A month of profit, best first: cc, aa, dd, bb.)
        state.loop.steer.profit = {{s.tillOf("aa_keep"), 40}, {s.tillOf("bb_keep"), 10}, {s.tillOf("cc_keep"), 90}, {s.tillOf("dd_keep"), 20},
                                   {s.tillOf("gull"), 500}};
        expect(s.restore(state), "the state read back");
        return s;
    };
    auto s = make();
    const auto& st = s.state().loop.steer;
    s.shift("treasury", s.tillOf("cc_keep"), "", 0, 1000, "test: a good week");
    const auto keeper = s.cash("cc_keep");
    run(s, 1 + 18.5 / 24, 2 + 18.5 / 24);           // (The ramp starts over tonight: a quarter, one charter.)
    expect(st.charters.size() == 1 && st.charters[0].first == s.tillOf("cc_keep") && s.loopToday().chartersGranted == 1,
           "the first night of the ramp: one charter, the best till first: " + (st.charters.empty() ? std::string("none") : st.charters[0].first));
    expect(st.charters[0].second == "the Crown", "a district no House claims: the Crown's: " + st.charters[0].second);
    expect(s.loopToday().charterProfit > 0 && s.cash("cc_keep") <= keeper + 40,
           "its profit to the House, its keeper paid a wage: " + n(s.loopToday().charterProfit) + "; the keeper " + n(keeper) + " -> " + n(s.cash("cc_keep")));
    const auto* post = s.position(s.tillOf("cc_keep").substr(5));
    expect(post && s.tillHouse(post) == "the Crown" && s.charterOf(s.tillOf("cc_keep")) == "the Crown", "the overlay: the House's till");
    for (const auto& h : s.skimStep(4, true))
        expect(h.id != "home home_cc_keep", "a chartered shop's keeper is no longer a keeper the skim takes profit from");
    run(s, 2 + 18.5 / 24, 5 + 18.5 / 24);
    std::string order;
    for (const auto& [till, house] : st.charters)
        order += (order.empty() ? "" : ",") + till.substr(9);
    expect(st.charters.size() == 4 && order.find("gull") == std::string::npos,
           "four nights: all four middle keepers' tills, the upper keeper's never: " + order);
    expect(s.charterOf(s.tillOf("aa_keep")) == "Greatly" && s.charterOf(s.tillOf("bb_keep")) == "the Crown" &&
               s.charterOf(s.tillOf("dd_keep")) == "the Crown",
           "each to the House whose district it is in, else the Crown: " + s.charterOf(s.tillOf("aa_keep")) + ", " +
               s.charterOf(s.tillOf("bb_keep")));
    expect(s.conserved(), "conserved");
    // Saved and read again.
    const auto back = wire::readSociety(wire::society(s.state())).loop.steer;
    expect(back.charters == st.charters && back.chartersTo == st.chartersTo && back.chartersFrom == st.chartersFrom &&
               back.chartersNights == st.chartersNights && back.profit == st.profit && back.profitNights == st.profitNights,
           "the charters saved and read again");
    // Deterministic.
    auto t = make();
    t.shift("treasury", t.tillOf("cc_keep"), "", 0, 1000, "test: a good week");
    run(t, 1 + 18.5 / 24, 5 + 18.5 / 24);
    expect(t.state().loop.steer.charters == st.charters, "two identical worlds, the same charters");
    for (const auto& [id, a] : s.state().accounts)
        expect(t.cash(id) == a.cash, "deterministic: " + id);
    // The slider back to the centre: handed back over the ramp, newest first.
    const auto last = st.charters.back().first;
    setRules(s, {{"steerLean", 100}});
    run(s, 5 + 18.5 / 24, 6 + 18.5 / 24);
    expect(st.charters.size() == 3 && std::none_of(st.charters.begin(), st.charters.end(), [&](const auto& c) { return c.first == last; }) &&
               s.loopToday().chartersBack == 1,
           "the first night back: the newest handed back: " + n(double(st.charters.size())) + " from " + n(st.chartersFrom) + " to " + n(st.chartersTo) + " since " + n(double(st.chartersNights)) + " back " + n(s.loopToday().chartersBack) + " day " + n(double(s.loopToday().day)));
    // Turned off: all handed back at once.
    setRules(s, {{"steer", 0}});
    run(s, 6 + 18.5 / 24, 7 + 18.5 / 24);
    expect(st.charters.empty() && s.tillHouse(post).empty() && !s.steering(), "off: every charter handed back");
    expect(s.conserved(), "conserved");
    table = saved;
}

// The slider followed (doc 75, 3.3): the levers at its place the same night, nothing judged, the means re-seated on a
// move; saved and read again; off, the world's own.
void theSlider()
{
    auto s = city({at("aa", 900), at("bb", 100), at("cc", 0)}, 9000, {{"steer", 1}, {"steerLean", 50}});
    s.steerEconomy(1);
    const auto& st = s.state().loop.steer;
    expect(st.on && std::abs(st.push + .5) < 1e-12, "the levers read at the slider, -0.5");
    expect(s.rule("surplusKeep") == 1000 && s.rule("surplusAlmsTo") == 60 && s.steerLever("levelling") == 20 && s.rule("treasuryCap") == 1750,
           "the levers at -0.5: the keep 1,000, the poorest to 60p, the tithe 20%, the cap 1,750p");
    for (int day = 2; day <= 200; ++day)
        s.steerEconomy(day);
    expect(std::abs(st.push + .5) < 1e-12 && st.history.size() == 1 && st.days == 200, "200 nights: nothing trimmed, nothing judged");
    setRules(s, {{"steerLean", 150}});
    s.steerEconomy(201);
    expect(std::abs(st.push - .5) < 1e-12 && st.judged == 201 && s.steerLever("groundRent") == 15 && st.history.size() == 2 &&
               st.history.back().why.find("moved from -50 to 50") != std::string::npos,
           "a slider change: the levers there the same night: " + st.history.back().why);
    const auto back = wire::readSociety(wire::society(s.state())).loop.steer;
    expect(back.on && back.push == st.push && back.lean == 150 && back.history.size() == st.history.size() && back.gini == st.gini &&
               back.history.back().why == st.history.back().why,
           "the steering's state saved and read again");
    setRules(s, {{"steer", 0}});
    s.steerEconomy(202);
    expect(!st.on && st.push == 0 && s.rule("surplusKeep") == 4000, "off: the world's own rules");
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
        theTithe();
        theCharters();
        theSlider();
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
