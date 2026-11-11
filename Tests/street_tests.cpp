// Street life (Docs/Design/79-a-staged-economy.md, sections 6.5-6.7, 7.7, 10 and 16.8; Phase 6): the day's thieves by
// market (within the cap, more as the slider falls, a hamlet's every few weeks), the hungry taking food from a till, the
// takes by band and what the Watch counts them worth; beggars (only destitute adults without a post, at most two a cell),
// alms only to residents, the Restday dole and on_dole, rough sleepers by a warm wall; a market whose staple players
// empty leaves its poor hungrier and its comfortable fed; help lifts a beggar (no begging the next day, a debt of thanks),
// within its market's cap, fed_household fame once a week; the shops' business; the short trouble by band; the
// standing-still rule (no stock moves in a watched shop without a buyer at its counter: an errand-runner); and the new
// state through a crash, by the journal (file mode here; through PostgreSQL by Tests/street_pg.py: --write-crashes and
// --check-crashes). Seeded and offline; the catalogue is the real Data/Items, the town the real Greyfen.
#include "test_purse.h"
#include "RatwCalendar.h"
#include "RatwGame.h"
#include "RatwJsonDoc.h"
#include "RatwSociety.h"
#include "RatwStatus.h"
#include "RatwTroubles.h"
#include "RatwWire.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
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
#include "town_fixture.h"

json::Value parse(const std::string& text)
{
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error))
        throw std::runtime_error("bad test json: " + error);
    return v;
}
std::string slurp(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

// ------------------------------------------------------------------ A society alone: households in rows of homes

ResidentSpec person(const std::string& id, const std::string& role, const std::string& work, int age, Spot home, Spot job)
{
    ResidentSpec r;
    r.id = id;
    r.name = id;
    r.role = role;
    r.workLabel = work;
    r.age = age;
    r.appearance.sex = id.size() % 2 ? "male" : "female";
    r.home = home;
    r.work = job;
    r.evening = home;
    r.serve = job;
    r.purse = 40;
    return r;
}
CareerWorld careers(const Society& s)
{
    CareerWorld w;
    w.alive = [](const std::string&) { return true; };
    w.age = [&s](const std::string& id) { const auto* r = s.spec(id); return r ? r->age : 30; };
    w.regard = [](const std::string&, const std::string&) { return 0.0; };
    return w;
}
// `n` households of one or two, a third of them with no earner (no reason, no destitute: 6.3), and a stall keeper selling
// meals: one market (a society alone has one).
Society rows(int n)
{
    AuthoredRoster roster;
    roster.residents.push_back(person("moss", "merchant", "keeping the stall", 45, {"stall_up", 2.5, 2.5}, {"stall", 2.5, 4.5}));
    for (int i = 0; i < n; ++i)
    {
        const auto home = "row_" + std::to_string(i);
        const auto a = "h" + std::to_string(i) + "a";
        const bool idle = i % 3 == 0;
        roster.residents.push_back(person(a, "civilian", idle ? "" : "hauling crates", 30 + i % 20, {home, 2.5, 2.5}, {"yard", 3.5, 3.5}));
        if (i % 2)
        {
            const auto b = "h" + std::to_string(i) + "b";
            roster.residents.push_back(person(b, "civilian", "mending nets", 28 + i % 15, {home, 3.5, 2.5}, {"yard", 4.5, 3.5}));
            roster.kin.push_back({a, b, "spouse"});
        }
    }
    Society s(Roster::None);
    s.configure(roster);
    s.tendCareers(1.3, careers(s));
    return s;
}

double thefts(Society& s, int slider, int days, int* most = nullptr, int* daysWith = nullptr)
{
    s.setProsperity(slider, true);
    double sum = 0;
    for (int d = 10; d < 10 + days; ++d)
    {
        s.stagedBands(d, true);
        const auto plan = s.theftDay(d, [](const std::string&) { return 1.; });
        int n = 0;
        for (const auto& [m, c] : plan.count)
            n += c;
        sum += n;
        if (most)
            *most = std::max(*most, n);
        if (daysWith)
            *daysWith += n > 0;
        for (const auto& [m, c] : plan.count)
            expect(c <= s.rule("theftCap"), "a market's thefts a day within the cap: " + std::to_string(c));
        for (const auto& t : plan.thieves)
        {
            const auto* r = s.spec(t.id);
            expect(r && r->age >= 16 && t.hour >= 8 && t.hour < 20, "a thief is an adult, at an hour of the day: " + t.id);
        }
    }
    return sum / days;
}

void theftsByMarket()
{
    // 10.1 / 16.8: within the cap, and rising as the slider falls.
    auto s = rows(160);
    int most = 0;
    const double hard = thefts(s, -100, 120, &most), ordinary = thefts(s, 0, 120, &most), boom = thefts(s, 100, 120, &most);
    expect(hard > ordinary && ordinary > boom, "thefts rise as the slider falls: " + std::to_string(hard) + " " + std::to_string(ordinary) +
                                                   " " + std::to_string(boom));
    expect(most <= 4 && hard > .5, "a few a day in hard times, never past the cap: " + std::to_string(hard) + ", most " + std::to_string(most));
    if (std::getenv("STREET_EACH"))
        std::cout << "  thefts a day in 160 households: " << hard << " at -100, " << ordinary << " at 0, " << boom << " at +100\n";
    // The cap is a rule (the DM's theftCap).
    auto spec = s.authored().economy;
    spec.rules["theftCap"] = 1;
    s.adoptEconomy(spec);
    int capped = 0;
    thefts(s, -100, 30, &capped);
    expect(capped <= 1, "theftCap 1: at most one a day");
    spec.rules["theftCap"] = 0;
    s.adoptEconomy(spec);
    int none = 0;
    expect(thefts(s, -100, 30, &none) == 0 && none == 0, "theftCap 0: none");
    // A hamlet of four households sees a theft every few weeks, not daily.
    auto hamlet = rows(4);
    int with = 0, hamletMost = 0;
    const double rate = thefts(hamlet, 0, 365, &hamletMost, &with);
    if (std::getenv("STREET_EACH"))
        std::cout << "  a hamlet of four: thefts on " << with << " days of 365\n";
    expect(with >= 2 && with <= 365 / 7 && hamletMost <= 1, "a hamlet's thefts every few weeks: " + std::to_string(with) + " days of 365 (" +
                                                               std::to_string(rate) + " a day)");
    // The same day's lots: the same thieves on one thread or four (the plan reads nothing but the day and the bands).
    const auto a = s.theftDay(40, nullptr), b = s.theftDay(40, nullptr);
    expect(a.thieves.size() == b.thieves.size() &&
               std::equal(a.thieves.begin(), a.thieves.end(), b.thieves.begin(),
                          [](const Society::Thief& x, const Society::Thief& y) { return x.id == y.id && x.hour == y.hour; }),
           "the day's thieves are the same each time they are worked out");
}

// The docks with the stall (as life_status_tests'): two dockers and their pup in the Sump's rents, the stall keeper, a
// docker couple, the Heights' lady.
Society docks()
{
    AuthoredRoster roster;
    roster.residents = {person("ivar", "civilian", "unloading ships at the piers", 40, {"sump_rents", 2.5, 2.5}, {"piers", 3.5, 3.5}),
                        person("una", "civilian", "unloading ships at the piers", 38, {"sump_rents", 3.5, 2.5}, {"piers", 4.5, 3.5}),
                        person("tam", "civilian", "at lessons", 12, {"sump_rents", 4.5, 2.5}, {"school", 2.5, 2.5}),
                        person("moss", "merchant", "keeping the stall", 45, {"stall_up", 2.5, 2.5}, {"stall", 2.5, 4.5}),
                        person("birch", "civilian", "unloading ships at the piers", 32, {"ash_house", 2.5, 2.5}, {"piers", 5.5, 3.5}),
                        person("wren", "civilian", "unloading ships at the piers", 30, {"ash_house", 3.5, 2.5}, {"piers", 6.5, 3.5}),
                        person("lady", "civilian", "at home in the Heights", 50, {"vance_house", 2.5, 2.5}, {"vance_house", 2.5, 2.5})};
    roster.kin = {{"ivar", "una", "spouse"}, {"ivar", "tam", "parent"}, {"una", "tam", "parent"}, {"birch", "wren", "spouse"}};
    Society s(Roster::None);
    s.configure(roster);
    s.tendCareers(1.3, careers(s));
    return s;
}
double food(const Society& s, const std::string& id)
{
    double n = 0;
    for (const auto& [item, count] : s.account(id)->stock)
        n += double(Society::nourishment(item)) * count;
    return n;
}
void emptyLarder(Society& s, const std::string& id)
{
    for (const auto& [item, c] : std::map<std::string, int>(s.account(id)->stock))
        if (c > 0 && Society::nourishment(item) > 0)
            s.consume(id, item, c, "test: emptied");
}
// The stall's meals set to `meals` (its other food cleared), a till the households of the docks buy their food from.
std::string stallWith(Society& s, int meals)
{
    const auto till = s.tillOf("moss");
    for (const auto& [item, n] : std::map<std::string, int>(s.account(till)->stock))
        if (n > 0 && Society::nourishment(item) > 0)
            s.consume(till, item, n, "test: the shelf cleared");
    if (meals > 0)
        expect(s.create(till, "meal", meals, "test: the shelf"), "the stall's shelf set");
    return till;
}

void playersEmptyTheStaple()
{
    // 16.8: players empty a market's staple; that market's poor show more hunger the next day, the comfortable never do.
    // The same day twice: the stall stocked, and the stall bare (players bought it out before the households' hours).
    const auto day = [](bool bare) {
        auto s = docks();
        s.setStatusOverride(std::make_unique<json::Value>(parse(R"({"rows":{"poor":{"short":0}}})")).get());
        s.setStagedHooks({nullptr, nullptr, &Society::tillShelf});
        s.stagedBands(2, true);
        s.overrideBand(s.householdOf("ivar"), status::Poor, 100);
        s.overrideBand(s.householdOf("lady"), status::Comfortable, 100);
        for (const auto& id : {"ivar", "una", "tam", "lady"})
            emptyLarder(s, id);
        stallWith(s, bare ? 0 : 40);
        for (int h = 0; h < 24; ++h)
            s.tick(1., 3 + h / 24. + 1e-4, 0, {});
        return std::pair{food(s, "ivar") + food(s, "una") + food(s, "tam"), food(s, "lady")};
    };
    const auto stocked = day(false), bare = day(true);
    expect(bare.first < stocked.first, "the poor fed less the day their staple was bought out: " + std::to_string(bare.first) + " for " +
                                           std::to_string(stocked.first));
    expect(bare.second >= Society::NourishADay && std::abs(bare.second - stocked.second) < 1e-9,
           "the comfortable fed the same either way, from elsewhere: " + std::to_string(bare.second));
}

void standingStillInTheSociety()
{
    // 7.7 / 6.6, the society's side: a watched till's stock never moves at a household's hour; a runner sent buys the real
    // pieces at the counter; with none to send, the food waits, and at the shutting the household is fed from elsewhere
    // with the shelf left as it stands (a bare one still gives the poor their short day).
    for (const bool runner : {true, false})
    {
        auto s = docks();
        s.setStatusOverride(std::make_unique<json::Value>(parse(R"({"rows":{"poor":{"short":0}}})")).get());
        s.setStagedHooks({nullptr, nullptr, &Society::tillShelf});
        std::set<std::string> asked;
        bool watched = true;
        s.setStreetHooks({[&](const Society&, const std::string&) { return watched; },
                          [&](Society&, const std::string& household, const std::string&) {
                              asked.insert(household);
                              return runner;
                          }});
        s.stagedBands(2, true);
        s.overrideBand(s.householdOf("ivar"), status::Poor, 100);
        for (const auto& id : {"ivar", "una", "tam"})
            emptyLarder(s, id);
        const auto till = stallWith(s, 30);
        const auto household = s.householdOf("ivar");
        int before = Society::stock(*s.account(till), "meal");
        for (int h = 0; h < 24; ++h)
        {
            s.tick(1., 3 + h / 24. + 1e-4, 0, {});
            if (!runner || !s.runnerWaiting(household))
                expect(Society::stock(*s.account(till), "meal") == before || !watched,
                       "a watched till's stock never moves without a buyer at its counter (hour " + std::to_string(h) + ")");
            if (runner && s.runnerWaiting(household))
            {
                expect(Society::stock(*s.account(till), "meal") == before, "nothing leaves the shelf while the runner walks");
                expect(s.runnerBought(household), "the runner at the counter buys from the shelf");
                const int after = Society::stock(*s.account(till), "meal");
                expect(after < before, "the real pieces leave the shelf: " + std::to_string(after) + " of " + std::to_string(before));
                expect(food(s, "ivar") + food(s, "una") + food(s, "tam") > 0, "and reach the household");
                before = after;
            }
        }
        expect(asked.count(household), "a runner asked for the household (D21)");
        if (!runner)
            expect(Society::stock(*s.account(till), "meal") == 30 && food(s, "ivar") + food(s, "una") + food(s, "tam") > 0,
                   "with no runner: the shelf untouched all day, the household fed from elsewhere at the shutting");
    }
}

void businessFromSales()
{
    // 10.3: a shop whose shelf sells down is thriving; one whose shelf doesn't move is failing, and three days failing is
    // a status event for its keeper's household; a shop that sells some is steady. "none" before its first day's end.
    auto s = docks();
    s.setStagedHooks({nullptr, nullptr, &Society::tillShelf});
    s.stagedBands(2, true);
    const auto till = s.tillOf("moss");
    expect(s.businessOf("moss") == "none" && s.businessOf("ivar") == "none", "no business before a day's end; none for a non-keeper");
    // A day of nothing sold, three times over.
    for (int d = 3; d <= 5; ++d)
        s.businessDay(d);
    expect(s.businessOf("moss") == "failing", "a shelf that never moves: failing: " + s.businessOf("moss"));
    bool event = false;
    for (const auto& e : s.state().staged.events.count("moss") ? s.state().staged.events.at("moss") : std::vector<StagedState::Event>{})
        event |= e.kind == "failing";
    expect(event, "three days failing: a status event for the keeper");
    // A day that sells its shelves down (half its targets or more): thriving.
    int target = 0;
    for (const auto& ware : s.stagedShelfGoods(s.jobOf("moss")->founder))
        target += s.shelfTarget(till, ware);
    s.resumeAt(6.4);
    s.noteSale(till, "meal", std::max(1, target), false);
    s.businessDay(7);
    expect(s.businessOf("moss") == "thriving", "a shelf sold down: thriving: " + s.businessOf("moss") + " (target " + std::to_string(target) + ")");
    expect(s.state().staged.business.at(till).failing == 0, "the failing run broken");
    // Players' buying counts apart (the market's middle: here the only shop's own, so it isn't more than itself).
    s.resumeAt(7.4);
    s.noteSale(till, "meal", 1, true);
    expect(s.state().staged.business.at(till).players == 1, "a player's purchase counted as a player's");
}

void theDole()
{
    // 10.2, the society's side: bread for each head of a destitute or poor household, once a Restday, on_dole for a week;
    // nothing for a comfortable one.
    auto s = docks();
    s.stagedBands(2, true);
    s.overrideBand(s.householdOf("ivar"), status::Poor, 100);
    s.overrideBand(s.householdOf("lady"), status::Comfortable, 100);
    for (const auto& id : {"ivar", "una", "tam"})
        emptyLarder(s, id);
    const int given = s.giveDole(s.householdOf("ivar"), 2);
    expect(given > 0 && Society::stock(*s.account("ivar"), "bread") > 0 && Society::stock(*s.account("tam"), "bread") > 0,
           "bread for each head: " + std::to_string(given));
    expect(s.giveDole(s.householdOf("ivar"), 2) == 0, "once a Restday");
    expect(s.giveDole(s.householdOf("lady"), 2) == 0, "never a comfortable household");
    s.tick(1., 2.5, 0, {});
    expect(s.onDole("una") && !s.onDole("lady"), "on_dole for the household fed");
    s.tick(1., 9.5, 0, {});
    expect(!s.onDole("una"), "and a week after, not");
}

void liftsAndTheirCap()
{
    // 6.7: help worth a week's band food lifts a poor or destitute household a week, a month's a month; a comfortable one
    // never; at most half the market's destitute share at once (at least one); fed_household fame once a week a holder.
    auto s = rows(60);
    s.setProsperity(-100, true);
    s.stagedBands(12, true);
    std::vector<std::string> poor;
    for (const auto& [id, life] : s.state().residents)
        if (const auto* st = s.statusOf(id); st && st->band <= status::Poor && id.back() == 'a')
            poor.push_back(id);
    expect(poor.size() >= 6, "poor and destitute households to help: " + std::to_string(poor.size()));
    const auto week = s.bandFoodCost(poor[0], 7), month = s.bandFoodCost(poor[0], 30);
    expect(week > 0 && month > week, "a week's band food and a month's priced: " + std::to_string(week) + " " + std::to_string(month));
    expect(s.liftByHelp(poor[0], week - 1) == 0 && !s.liftedNow(poor[0]), "less than a week's food: no lift");
    expect(s.liftByHelp(poor[0], week) == 7 && s.liftedNow(poor[0]), "a week's: lifted a week");
    expect(s.liftByHelp(poor[1], s.bandFoodCost(poor[1], 30)) == 30, "a month's: a month");
    int lifted = 2;
    for (std::size_t i = 2; i < poor.size(); ++i)
        lifted += s.liftByHelp(poor[i], s.bandFoodCost(poor[i], 30)) > 0;
    const double share = status::sharesAt(s.statusTable(), s.prosperity())[status::Destitute];
    int households = 0;
    std::set<std::string> seen;
    for (const auto& [id, life] : s.state().residents)
        if (const auto* st = s.statusOf(id); st && seen.insert(st->household).second)
            ++households;
    const int room = std::max(1, int(std::ceil(.5 * share / 100 * households - 1e-9)));
    expect(lifted == room && lifted < int(poor.size()), "at most half the destitute share lifted at once: " + std::to_string(lifted) +
                                                            " of " + std::to_string(poor.size()) + ", room " + std::to_string(room));
    std::string comfortable;
    for (const auto& [id, life] : s.state().residents)
        if (const auto* st = s.statusOf(id); st && st->band >= status::Comfortable)
            comfortable = id;
    expect(!comfortable.empty() && s.liftByHelp(comfortable, 100000) == 0, "a comfortable household is never lifted");
    // A lifted household is out of the day's thieves.
    for (int d = 12; d < 60; ++d)
        for (const auto& t : s.theftDay(d, nullptr).thieves)
            expect(!s.liftedNow(t.id) || d > 12, "no lifted thief");
    // Fame: once a week per holder and household; a second character of the same account shares it.
    s.setHolder("player-ada", "acct:ada");
    s.setHolder("player-ada2", "acct:ada");
    const auto h = s.householdOf(poor[0]);
    expect(s.fedFameRoom("player-ada", h) && !s.fedFameRoom("player-ada2", h) && !s.fedFameRoom("player-ada", h),
           "fed_household fame once a week, by account holder");
    expect(s.fedFameRoom("player-bo", h), "another holder's own");
    s.tick(1., double(s.state().budgetDay) + 7.5, 0, {});
    expect(s.fedFameRoom("player-ada", h), "and again a week on");
}

void shortByBand()
{
    // 6.7: the short trouble is the head of a poor or destitute household's, its coins a fortnight of its band's food at
    // its market's price (at least 10p); lifted, it isn't short.
    auto s = docks();
    s.stagedBands(2, true);
    s.overrideBand(s.householdOf("ivar"), status::Destitute, 100);
    troubles::Reads reads;
    reads.society = &s;
    reads.age = [&s](const std::string& id) { const auto* r = s.spec(id); return r ? r->age : 30; };
    reads.alive = [](const std::string&) { return true; };
    const auto t = troubles::kindOf("short", "ivar", reads);
    const auto* kind = troubles::rules().kind("short");
    expect(t.kind == "short" && kind, "a destitute head is short of coin");
    expect(t.coins == std::max<std::int64_t>(10, s.bandFoodCost("ivar", kind->refillDays)) && t.coins >= 10,
           "its coins a fortnight of its band's food: " + std::to_string(t.coins));
    expect(t.days < kind->underDays, "its days fewer than the trouble's mark");
    expect(s.liftByHelp("ivar", t.coins, 14) == 14 && troubles::kindOf("short", "ivar", reads).kind.empty(), "lifted: not short");
}

// ------------------------------------------------------------------ Greyfen, with a priest at the Long House

// Greyfen's own world, copied, with a priest keeping a chapel in the Long House (indoors; its door gives on the square).
fs::path greyfen(const std::string& name)
{
    const auto root = fs::temp_directory_path() / ("ratw-street-" + name + "-" + std::to_string(::getpid()));
    fs::remove_all(root);
    fs::copy(RATW_SOURCE_DIR "/Data/Worlds/Greyfen", root, fs::copy_options::recursive);
    std::ofstream(root / "world.ratw", std::ios::app)
        << "resident \"vicar\" \"Vicar Holm\" \"civilian\" \"the priest of the chapel\" \"A grey priest.\" \"Peace.\" 52 \"timber\" "
           "\"male\" \"average\" \"saddle\" 2 1 5 11 1 8 17 \"-\" 30 0 1 \"longhouse\" 3.5 2.5 \"longhouse\" 12.5 3.5 \"longhouse\" "
           "12.5 6.5\n";
    return root;
}
World town(const fs::path& root)
{
    World world;
    const auto loaded = world.loadWorldFile((root / "world.ratw").string());
    expect(loaded.ok, "the town loads: " + loaded.message);
    world.setTimeOfDay(11);
    for (int i = 0; i < 120; ++i)
        world.tick(1);
    return world;
}
// On to the next such weekday, at an hour.
void onTo(World& w, int weekday, double hour)
{
    for (int i = 0; i < 8 && calendar::weekdayOf(w.calendarDays()) != weekday; ++i)
        w.advanceCalendar(1);
    expect(calendar::weekdayOf(w.calendarDays()) == weekday, "the weekday reached");
    w.setTimeOfDay(hour);
    for (int i = 0; i < 30; ++i)
        w.tick(1);
}
// Without a post, destitute: a resident made a beggar (its post emptied, its household's band set by the DM).
void destitute(World& w, const std::string& id)
{
    auto st = w.society().state();
    for (auto& [pid, p] : st.careers.positions)
        if (p.holder == id)
            p.holder.clear();
    expect(w.society().restore(st), "the post emptied");
    for (int i = 0; i < 4; ++i)
        w.tick(1);
    w.society().stagedBands(std::int64_t(std::floor(w.calendarDays())), true);
    expect(w.society().overrideBand(w.society().householdOf(id), status::Destitute, 100000), "the household destitute: " + id);
}
std::int64_t cash(const World& w, const std::string& id)
{
    const auto* a = w.society().account(id);
    return a ? a->cash : 0;
}

void hungryThievesAtTheTill()
{
    // 10.1: a hungry thief goes for a food shop and takes food from its till's stock (a new path in steal()), not the
    // keeper's own purse. (Greyfen's theft base is made high so its day has thieves; every resident starving.)
    const auto root = greyfen("till");
    auto w = town(root);
    auto often = parse(R"({"street": {"theftBase": 5}})");
    w.society().setStatusOverride(&often);
    w.advanceCalendar(1);
    w.setTimeOfDay(9);
    const auto starve = [&] {
        auto st = w.society().state();
        for (auto& [id, life] : st.residents)
            if (life.role != "guard" && !w.society().merchant(id))
            {
                life.hunger = 90;
                for (const auto& [item, n] : std::map<std::string, int>(st.accounts[id].stock))
                    if (Society::edible(item))
                        st.accounts[id].stock.erase(item);
            }
        expect(w.society().restore(st), "the town starves");
    };
    starve();
    std::map<std::string, std::map<std::string, int>> tills;
    for (const auto& p : w.society().positions())
        if (p.role == "merchant")
            if (const auto* a = w.society().account("till:" + p.id))
                tills["till:" + p.id] = a->stock;
    const auto plan = w.theftPlan();
    expect(!plan.thieves.empty(), "the day has thieves");
    for (const auto& t : plan.thieves)
        w.entity(t.id)->sneakSkill = 300;           // (Deft paws: where they go and what they take is what this checks.)
    bool fromTill = false;
    for (int i = 0; i < 7200 && !fromTill; ++i)
    {
        w.tick(1);
        if (i % 600 == 0)
            starve();
        for (const auto& inc : w.crime().incidents)
            if (inc.kind == "theft" && !inc.item.empty() && Society::edible(inc.item) && w.society().merchant(inc.victim))
                fromTill = true;
    }
    if (std::getenv("STREET_DEBUG"))
    {
        for (const auto& t : plan.thieves)
            std::cerr << "thief " << t.id << " h " << t.hour << " " << w.entity(t.id)->activity << " hunger " << w.society().resident(t.id)->hunger << "\n";
        for (const auto& inc : w.crime().incidents)
            std::cerr << "inc " << inc.kind << " " << inc.offender << " " << inc.victim << " " << inc.item << " " << inc.quantity << " " << inc.coins << "\n";
        for (const auto& [till, stock] : tills)
        {
            std::cerr << till << ":";
            for (const auto& [i, n] : stock)
                std::cerr << " " << i << "=" << n;
            std::cerr << "\n";
        }
    }
    expect(fromTill, "a hungry thief took food from a food shop");
    for (const auto& inc : w.crime().incidents)
        if (inc.kind == "theft" && !inc.item.empty() && w.society().merchant(inc.victim))
        {
            const auto till = w.society().tillOf(inc.victim);
            expect(tills.count(till), "the shop's till is known");
            expect(inc.coins == 0 && inc.quantity >= 1 && inc.quantity <= 2, "a piece or two of food, no coin");
        }
    // Never a player (D11): a player beside every thief all day is never robbed.
    expect(w.society().realBalanced(), "real money balanced");
    fs::remove_all(root);
}

void takesByBand()
{
    // 6.5 / 10.1: what a pickpocket finds goes with the victim's band (a rich one 5-25p), within a player's theft cap; and
    // the Watch counts stolen goods at their quoted price, not the old fixed few.
    const auto root = greyfen("take");
    auto w = town(root);
    w.addPlayer("player-ada", "Ada");
    w.society().setHolder("player-ada", "acct:ada");
    auto& s = w.society();
    expect(s.overrideBand(s.householdOf("wren"), status::Rich, 100000), "Wren's household rich");
    testPurse(s, "wren", 200, "test");
    std::int64_t got = 0;
    int tries = 0;
    for (; tries < 60 && got == 0; ++tries)
    {
        auto& e = *w.entity("player-ada");
        const auto& o = *w.entity("wren");
        e.cellId = o.cellId;
        e.position = {o.position.x + 1, o.position.y};
        e.sneakSkill = 300;                         // (A deft paw: the take, not the odds, is what this checks.)
        const auto before = cash(w, "player-ada");
        const auto done = w.steal("player-ada", "wren");
        if (std::getenv("STREET_DEBUG"))
            std::cerr << done.message << "\n";
        got = cash(w, "player-ada") - before;
        w.tick(4.1);
    }
    const auto& row = s.statusTable().rows[status::Rich];
    expect(got >= row.pocketLow && got <= std::min<std::int64_t>(row.pocketHigh, s.statusTable().cap("theftADay")),
           "a rich pocket's take, within the day's cap: " + std::to_string(got));
    expect(s.realBalanced(), "real money balanced: the take a registered faucet");
    // Worth: a meal at its quoted price where Wren keeps shop (her market's), never 0 for a good the old table didn't name.
    const auto meal = s.marketLot(s.statusOf("wren") ? s.statusOf("wren")->market : std::string(), "meal", 1);
    expect(meal >= 1 && meal <= 2 * Society::catalogPrice("meal"), "a meal's worth by the Board: " + std::to_string(meal));
    expect(s.marketLot("", "rope", 2) == 2 * Society::catalogPrice("rope"), "a good the old table didn't know is worth its price");
    fs::remove_all(root);
}

void beggarsAndAlms()
{
    // 10.2 / 16.8: on Marketday by day, the destitute adults without a post beg at the market (no chapel steps here: the
    // Long House is a lodging, its priest added for the dole), at most two a cell; nobody else does. Alms from the
    // comfortable and rich only to residents: a player standing among the beggars gets none.
    const auto root = greyfen("beg");
    auto w = town(root);
    for (const auto* id : {"linden", "rook", "maple"})
        destitute(w, id);
    w.addPlayer("player-ada", "Ada");
    onTo(w, calendar::Marketday, 10);
    const auto beggars = w.beggars();
    expect(!beggars.empty(), "beggars on Marketday");
    std::map<std::string, int> inCell;
    for (const auto& [id, at] : beggars)
    {
        const auto* st = w.society().statusOf(id);
        expect(st && st->band == status::Destitute && !w.society().jobOf(id) && w.society().spec(id)->age >= 16,
               "a beggar is a destitute adult without a post: " + id);
        ++inCell[at.cell];
    }
    for (const auto& [cell, n] : inCell)
        expect(n <= 2, "at most two a cell: " + cell + " " + std::to_string(n));
    for (const auto& [id, life] : w.society().state().residents)
        if (!beggars.count(id))
            if (const auto* st = w.society().statusOf(id); st && st->band == status::Destitute && !w.society().jobOf(id))
                expect(inCell.size() * 2 <= beggars.size(), "another destitute adult left out only when the spots are full: " + id);
    // They go there, and say they are begging.
    for (int i = 0; i < 400; ++i)
        w.tick(1);
    bool seen = false;
    for (const auto& [id, at] : beggars)
        seen |= w.entity(id)->activity.rfind("begging", 0) == 0;
    expect(seen, "a beggar at it: " + w.entity(beggars.begin()->first)->activity);
    // Alms: the comfortable and the rich standing by give; a player stood among them is never given anything.
    const auto beggar = beggars.begin()->first;
    const auto beggarBefore = cash(w, beggar);
    const auto adaBefore = cash(w, "player-ada");
    int givers = 0;
    for (const auto* id : {"wren", "fennel", "sorrel", "brann"})
    {
        auto& s = w.society();
        s.overrideBand(s.householdOf(id), status::Rich, 100000);
        testPurse(s, id, 20, "test");
        ++givers;
    }
    for (int i = 0; i < 200; ++i)
    {
        for (const auto* id : {"wren", "fennel", "sorrel", "brann", "player-ada"})
        {
            auto* e = w.entity(id);
            const auto* b = w.entity(beggar);
            e->cellId = b->cellId;
            e->position = {b->position.x + 1, b->position.y + (id[0] == 'p' ? -1 : 1)};
            e->path.clear();
            e->velocity = {};
        }
        w.tick(1);
    }
    expect(cash(w, "player-ada") == adaBefore, "no alms to a player");
    expect(cash(w, beggar) > beggarBefore, "the beggar's purse: " + std::to_string(cash(w, beggar)) + " from " + std::to_string(beggarBefore));
    expect(w.society().realBalanced(), "real money balanced");
    // On a working day, nobody begs.
    onTo(w, calendar::weekdayOf(w.calendarDays() + 1), 10);
    if (const int wd = calendar::weekdayOf(w.calendarDays()); wd != calendar::Marketday && wd != calendar::Restday)
        expect(w.beggars().empty(), "no beggars on a working day");
    fs::remove_all(root);
}

void restdayDole()
{
    // 10.2: on Restday after the service the chapel's priest hands bread to the destitute and poor households of its
    // town (a raw add, no church account), and they are on the dole; a player standing by is given nothing.
    const auto root = greyfen("dole");
    auto w = town(root);
    destitute(w, "linden");
    w.addPlayer("player-ada", "Ada");
    const auto adaBread = Society::stock(*w.society().account("player-ada"), "bread");
    onTo(w, calendar::Restday, Society::ServiceEnd + .2);
    expect(w.society().onDole("linden"), "Linden's household on the dole");
    expect(Society::stock(*w.society().account("linden"), "bread") > 0, "with bread in hand");
    expect(w.dolePriests().count("vicar"), "the priest handing it out");
    for (int i = 0; i < 300; ++i)
        w.tick(1);
    expect(w.entity("vicar")->activity.rfind("handing out the dole", 0) == 0, "seen at it: " + w.entity("vicar")->activity);
    expect(Society::stock(*w.society().account("player-ada"), "bread") == adaBread, "nothing for a player");
    expect(!w.society().onDole("wren"), "a comfortable household isn't on the dole");
    expect(w.society().realBalanced(), "real money balanced");
    fs::remove_all(root);
}

void roughSleepers()
{
    // 10.2 / D12: some winter nights the destitute with nowhere of their own sleep by a warm wall (a forge's) in their
    // market; never on a summer night.
    const auto root = greyfen("cold");
    auto w = town(root);
    for (const auto* id : {"linden", "rook", "maple", "sorrel"})
    {
        destitute(w, id);
        auto st = w.society().state();
        st.residents[id].homeCell.clear();          // (Homeless, as authored.)
        expect(w.society().restore(st), "homeless");
    }
    int nights = 0, wintry = 0;
    for (int d = 0; d < 400 && wintry < 30; ++d)
    {
        w.advanceCalendar(1);
        w.setTimeOfDay(23);
        w.tick(1);
        w.tick(1);
        const bool winter = calendar::calendarAt(w.calendarDays()).season == calendar::Season::Winter;
        if (!winter)
            expect(w.roughSleepers().empty(), "nobody by the warm wall out of winter");
        else
        {
            ++wintry;
            nights += !w.roughSleepers().empty();
            for (const auto& [id, at] : w.roughSleepers())
                expect(w.society().statusOf(id) && w.society().statusOf(id)->band == status::Destitute, "a sleeper is destitute");
        }
    }
    expect(wintry > 0 && nights > 0 && nights < wintry * 4, "by the warm wall some winter nights: " + std::to_string(nights) + " of " +
                                                                std::to_string(wintry));
    fs::remove_all(root);
}

void householdsBuyTheirWares()
{
    // 6.5 / 7.7 (and Phase 5's leftover: shelves fall only when someone buys): with no player near, households buy their
    // market's everyday wares from the shops' shelves by their bands' chances (here every household, every day), so a
    // shop's shelves thin between its restocks (an import's between its wagons or ships) and its business counts them.
    const auto root = greyfen("wares");
    auto w = town(root);
    auto every = parse(R"({"rows": {"poor": {"shops": 1}, "comfortable": {"shops": 1}, "rich": {"shops": 1}}})");
    w.society().setStatusOverride(&every);
    w.advanceCalendar(1);
    w.setTimeOfDay(7.9);
    for (int i = 0; i < 5; ++i)
        w.tick(1);
    auto& s = w.society();
    std::map<std::string, std::map<std::string, int>> before;
    for (const auto* k : {"wren", "brann"})
        before[k] = s.account(s.tillOf(k))->stock;
    w.advanceCalendar(10.5 / 24);
    for (int i = 0; i < 5; ++i)
        w.tick(1);
    int fell = 0, sold = 0;
    for (const auto* k : {"wren", "brann"})
    {
        for (const auto& [item, n] : before[k])
            fell += Society::nourishment(item) <= 0 && Society::stock(*s.account(s.tillOf(k)), item) < n;
        if (const auto b = s.state().staged.business.find(s.tillOf(k)); b != s.state().staged.business.end())
            sold += b->second.sold;
    }
    expect(fell > 0, "the shops' everyday wares bought down: " + std::to_string(fell));
    expect(sold > 0, "and counted toward their business: " + std::to_string(sold));
    fs::remove_all(root);
}

void standingStillInTheShop()
{
    // 7.7 / 16.8: a player standing in Wren's shop all day sees its stock change only with a buyer in the room: each drop
    // in a staged sale comes as an errand-runner stands at the counter (6.6), at most two runners in the cell.
    const auto root = greyfen("still");
    auto w = town(root);
    w.addPlayer("player-ada", "Ada");
    auto& s = w.society();
    const auto till = s.tillOf("wren");
    expect(till != "wren", "Wren's shop has a till");
    w.advanceCalendar(1);
    w.setTimeOfDay(7.5);
    const auto stand = [&] {
        auto* me = w.entity("player-ada");
        const auto* p = s.position(till.substr(5));
        me->cellId = p->work.cell;
        me->position = {p->work.x + 2, p->work.y + 2};
        me->velocity = {};
    };
    stand();
    for (int i = 0; i < 60; ++i)
        w.tick(1);
    std::map<std::string, int> before = s.account(till)->stock;
    int drops = 0, runnersSeen = 0;
    for (int i = 0; i < 6000; ++i)
    {
        stand();
        bool atCounter = false;
        int inCell = 0;
        for (const auto& [id, r] : w.runners())
        {
            const auto* e = w.entity(id);
            inCell += r.at.cell == s.position(till.substr(5))->work.cell;
            atCounter |= e && r.atCounter >= 0 && e->cellId == r.at.cell;
        }
        runnersSeen += !w.runners().empty();
        expect(inCell <= World::RunnersACell, "at most two runners in the player's cell");
        // (The morning restock is the keeper's own, setting out the wares: a buyer is not needed for it.)
        const bool settingOut = s.settingOut("wren");
        w.tick(1);
        const auto& now = s.account(till)->stock;
        for (const auto& [item, n] : before)
            if (Society::stock(s.account(till) ? *s.account(till) : EconomyAccount{}, item) < n)
            {
                ++drops;
                expect(atCounter || settingOut, "the shelf's " + item + " fell with no buyer at the counter, at tick " + std::to_string(i));
            }
        before = now;
    }
    expect(runnersSeen > 0, "errand-runners came to the counter");
    expect(drops > 0, "the shelf fell as they bought");
    fs::remove_all(root);
}

// ------------------------------------------------------------------ Through a crash, by the journal

game::Options optionsOn(const fs::path& root, const std::string& save)
{
    game::Options o;
    o.forkSnapshots = false;
    o.tiesOptional = true;
    o.hiddenNames = false;
    o.devIdentity = true;
    o.worldFile = (root / "world.ratw").string();
    o.savePath = save;
    return o;
}
std::string crashSave(const std::string& name)
{
    return (fs::temp_directory_path() / ("ratw-street-crash-" + name + "-" + std::to_string(::getpid()) + ".json")).string();
}
void cleanSave(const std::string& path)
{
    for (const auto* end : {"", ".journal", ".tmp", ".bak"})
        fs::remove(path + end);
}
// What street life holds, in words a file keeps: the dole, the business, the fed fame, the bands' lifts, Wren's shelves.
json::Value streetAsItStands(const World& w)
{
    const auto& s = w.society();
    const auto& st = s.state().staged;
    auto out = json::Value::object();
    std::ostringstream dole, business, fed, lifts, shelf;
    for (const auto& [h, d] : st.dole)
        dole << h << ' ' << d << ' ';
    for (const auto& [t, b] : st.business)
        business << t << ' ' << b.day << ' ' << b.sold << ' ' << b.players << ' ' << b.word << ' ' << b.failing << ' ';
    for (const auto& [k, d] : st.fed)
        fed << k << ' ' << d << ' ';
    for (const auto& [h, b] : st.bands)
        if (b.liftedUntil >= 0)
            lifts << h << ' ' << b.liftedUntil << ' ';
    if (const auto* a = s.account(s.tillOf("wren")))
        for (const auto& [item, n] : a->stock)
            shelf << item << ' ' << n << ' ';
    out.add("dole", dole.str());
    out.add("business", business.str());
    out.add("fed", fed.str());
    out.add("lifts", lifts.str());
    out.add("shelf", shelf.str());
    return out;
}
struct StreetCrash
{
    std::string name;
    std::function<void(game::Game&, Client&)> step;   // After the checkpoint: the street's step, and a player's record.
    std::function<void(game::Game&, const json::Value&)> check;   // After the restart (what was so at the crash given).
};
void enter(game::Game& g, Client& c, int n, const char* who)
{
    c.id = std::uint64_t(n);
    g.connect(&c);
    g.command(&c, cmd({{"type", "hello"}, {"id", who}, {"name", who}}));
    testPurse(g.world().society(), c.entityId, 400, "test");
}
void beside(game::Game& g, Client& c, const std::string& other)
{
    auto* me = g.world().entity(c.entityId);
    const auto* o = g.world().entity(other);
    me->cellId = o->cellId;
    me->position = {o->position.x + 1, o->position.y};
}
// A penny to Wren: an economy record (the player's side journalled, with what the world's ticks moved beside it).
void aPenny(game::Game& g, Client& c)
{
    beside(g, c, "wren");
    g.command(&c, cmd({{"type", "give"}, {"target", "wren"}, {"item", ""}, {"quantity", 0}, {"coins", 1.}}));
    g.settle();
}
void helpABeggar()
{
    // 6.7 / T15 / 16.8: a player's help (a week's food in coin) lifts a beggar's household: the next Marketday it doesn't
    // beg, has no hunger tags, and owes the player its thanks (the pair.owed lines can fire).
    const auto root = greyfen("help");
    game::Game g(optionsOn(root, {}));
    std::string problem;
    expect(g.start(problem), "a game starts on Greyfen: " + problem);
    auto& w = g.world();
    for (int i = 0; i < 30; ++i)
        w.tick(1);
    destitute(w, "linden");
    Client ash;
    enter(g, ash, 1, "ash");
    const auto week = w.society().bandFoodCost("linden", 7);
    expect(week > 0, "a week's food priced: " + std::to_string(week));
    beside(g, ash, "linden");
    g.command(&ash, cmd({{"type", "give"}, {"target", "linden"}, {"item", ""}, {"quantity", 0}, {"coins", double(week)}}));
    for (int i = 0; i < 4; ++i)
        g.tick(.25);
    expect(w.society().liftedNow("linden"), "the household lifted: " + ash.said());
    const auto* bond = w.bonds().find("linden", ash.entityId);
    expect(bond && bond->owed < 0, "Linden owes Ash his thanks (pair.owed)");
    onTo(w, calendar::Marketday, 10);
    expect(!w.beggars().count("linden"), "the next Marketday he doesn't beg");
    expect(w.society().realBalanced(), "real money balanced");
    fs::remove_all(root);
}

std::vector<StreetCrash> streetRuns()
{
    std::vector<StreetCrash> runs;
    // The Restday dole given on a tick: after the crash the household is on the dole as it was, and the priest gives no
    // second dole that Restday.
    runs.push_back({"dole",
                    [](game::Game& g, Client& c) {
                        auto& w = g.world();
                        onTo(w, calendar::Restday, Society::ServiceEnd + .2);
                        expect(w.society().onDole("linden"), "dole: given");
                        aPenny(g, c);
                    },
                    [](game::Game& g, const json::Value& was) {
                        auto& w = g.world();
                        expect(w.society().onDole("linden"), "dole: on the dole after the crash");
                        const auto bread = Society::stock(*w.society().account("linden"), "bread");
                        for (int i = 0; i < 40; ++i)
                            w.tick(1);
                        expect(Society::stock(*w.society().account("linden"), "bread") <= bread, "dole: no second dole that Restday");
                        (void)was;
                    }});
    // A player's help lifts a beggar's household (its band's lift and the fame's week journalled with the gift).
    runs.push_back({"lift",
                    [](game::Game& g, Client& c) {
                        auto& w = g.world();
                        beside(g, c, "linden");
                        const auto week = w.society().bandFoodCost("linden", 7);
                        g.command(&c, cmd({{"type", "give"}, {"target", "linden"}, {"item", ""}, {"quantity", 0}, {"coins", double(week)}}));
                        g.settle();
                        expect(w.society().liftedNow("linden"), "lift: lifted");
                    },
                    [](game::Game& g, const json::Value&) { expect(g.world().society().liftedNow("linden"), "lift: still lifted after the crash"); }});
    // A day's business turned on a tick and a player's purchase counted: both as at the crash.
    runs.push_back({"business",
                    [](game::Game& g, Client& c) {
                        auto& w = g.world();
                        w.advanceCalendar(1);
                        w.setTimeOfDay(9);
                        for (int i = 0; i < 30; ++i)
                            w.tick(1);
                        beside(g, c, "wren");
                        const auto wares = w.society().wares("wren");
                        expect(!wares.empty(), "business: Wren has wares");
                        const auto till = w.society().tillOf("wren");
                        const auto bought = [&] {
                            const auto& all = w.society().state().staged.business;
                            const auto b = all.find(till);
                            return b == all.end() ? 0 : b->second.players;
                        };
                        const int before = bought();
                        g.command(&c, cmd({{"type", "trade"}, {"target", "wren"}, {"item", wares.front()}, {"quantity", 1}, {"buy", true}}));
                        g.settle();
                        expect(bought() == before + 1, "business: the player's purchase counted: " + c.said());
                        aPenny(g, c);
                    },
                    [](game::Game&, const json::Value&) {}});
    // The households' staged buying on the world's ticks (no player near): Wren's shelves fall, and go whole into the
    // next record with the Board's (the trade-moved shelves: Phase 6), so after the crash they stand as they did.
    runs.push_back({"sales",
                    [](game::Game& g, Client& c) {
                        auto& w = g.world();
                        auto* me = w.entity(c.entityId);
                        me->cellId = "barracks";            // (Out of the way: no till is watched.)
                        me->position = {5.5, 5.5};
                        const auto till = w.society().tillOf("wren");
                        const auto before = w.society().account(till)->stock;
                        for (int i = 0; i < 1800 && w.society().account(till)->stock == before; ++i)
                            w.tick(1);
                        expect(w.society().account(till)->stock != before, "sales: the households bought from Wren's shelves");
                        aPenny(g, c);
                    },
                    [](game::Game&, const json::Value&) {}});
    return runs;
}
json::Value toTheStreetCrash(const StreetCrash& r, const fs::path& root, const std::string& path)
{
    json::Value was = json::Value::object();
    game::Game g(optionsOn(root, path));
    std::string problem;
    expect(g.start(problem), r.name + ": a game starts on Greyfen: " + problem);
    auto& w = g.world();
    for (int i = 0; i < 30; ++i)
        w.tick(1);
    destitute(w, "linden");
    Client ash;
    enter(g, ash, 71, "ash");
    g.save();
    g.settle();                                     // (The checkpoint.)
    r.step(g, ash);
    was.add("atCrash", streetAsItStands(w));
    return was;                                     // (The game gone without saving.)
}
void afterTheStreetCrash(const StreetCrash& r, const fs::path& root, const std::string& path, const json::Value& was)
{
    game::Game g(optionsOn(root, path));
    std::string problem;
    expect(g.start(problem), r.name + ": the crash's save starts: " + problem);
    const auto now = streetAsItStands(g.world());
    const auto& stood = was["atCrash"];
    for (const char* key : {"dole", "business", "fed", "lifts", "shelf"})
        expect(now.string(key) == stood.string(key),
               r.name + ": the " + key + " of the crash after the restart:\n  " + now.string(key) + "\n  for\n  " + stood.string(key));
    r.check(g, was);
    expect(g.world().society().realBalanced(), r.name + ": real money balanced");
}
void streetThroughACrash()
{
    for (const auto& r : streetRuns())
    {
        const auto root = greyfen("crash-" + r.name);
        const auto path = crashSave(r.name);
        cleanSave(path);
        const auto was = toTheStreetCrash(r, root, path);
        afterTheStreetCrash(r, root, path, was);
        cleanSave(path);
        fs::remove_all(root);
    }
}
// --write-crashes DIR / --check-crashes BACK DIR (Tests/street_pg.py): each case's checkpoint, journal and what was so.
int writeCrashes(const fs::path& dir)
{
    fs::create_directories(dir);
    for (const auto& r : streetRuns())
    {
        const auto root = greyfen("crash-" + r.name);
        const auto path = crashSave(r.name);
        cleanSave(path);
        const auto was = toTheStreetCrash(r, root, path);
        expect(fs::exists(path) && fs::exists(path + ".journal"), r.name + ": a checkpoint and a journal written");
        fs::copy_file(path, dir / (r.name + ".json"), fs::copy_options::overwrite_existing);
        fs::copy_file(path + ".journal", dir / (r.name + ".journal"), fs::copy_options::overwrite_existing);
        std::ofstream(dir / (r.name + ".was.json"), std::ios::binary) << json::dump(was);
        cleanSave(path);
        fs::remove_all(root);
    }
    return 0;
}
int checkCrashes(const fs::path& back, const fs::path& dir)
{
    for (const auto& r : streetRuns())
    {
        const auto root = greyfen("crash-" + r.name);
        const auto path = crashSave(r.name);
        cleanSave(path);
        fs::copy_file(back / (r.name + ".json"), path);
        fs::copy_file(back / (r.name + ".journal"), path + ".journal");
        json::Value was;
        std::string error;
        expect(json::parse(slurp(dir / (r.name + ".was.json")), was, error), r.name + ": what was so reads: " + error);
        afterTheStreetCrash(r, root, path, was);
        cleanSave(path);
        fs::remove_all(root);
    }
    std::cout << "street_tests --check-crashes: " << streetRuns().size() << " street crashes replayed the same through Postgres (" << checks
              << " checks)\n";
    return 0;
}

void streetStateRoundTrips()
{
    // The new state through the wire (lenient on read): an older save without it reads as none; a bad entry is dropped.
    StagedState st;
    st.dole["home|a"] = 12;
    st.fed["acct:ada|home|a"] = 11;
    auto& b = st.business["till:job:moss"];
    b.day = 12, b.sold = 7, b.players = 2, b.soldBefore = {3, 4}, b.playersBefore = {1, 0}, b.outEarly = true, b.failing = 2, b.word = "failing";
    const auto back = wire::readStaged(wire::stagedValue(st));
    expect(back.dole == st.dole && back.fed == st.fed && back.business == st.business, "the street's state round-trips");
    auto v = wire::stagedValue(st);
    const auto older = wire::readStaged(parse(R"({"bands": {}, "hour": 3, "day": 1})"));
    expect(older.dole.empty() && older.business.empty() && older.fed.empty(), "a save from before Phase 6 reads as none");
    const auto bad = wire::readStaged(parse(R"({"dole": {"h": -4, "": 2}, "business": {"till:x": {"day": "x"}, "till:y": {"day": 3, "word": "booming", "sold": -2}}})"));
    expect(bad.dole.empty() && bad.business.size() == 1 && bad.business.at("till:y").word.empty() && bad.business.at("till:y").sold == 0,
           "a bad entry dropped or clamped, never refused");
    // The diff the journal writes: each map's change, and nothing when nothing moved.
    expect(wire::stagedDiff(st, st).items().empty(), "no change, no diff");
    auto moved = st;
    moved.business["till:job:moss"].sold = 8;
    moved.dole.erase("home|a");
    expect(wire::stagedDiff(st, moved).items().size() == 2, "a business moved and a dole gone: two changes");
    (void)v;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--write-crashes")
            return writeCrashes(argv[2]);
        if (argc == 4 && std::string(argv[1]) == "--check-crashes")
            return checkCrashes(argv[2], argv[3]);
        const std::vector<std::pair<const char*, void (*)()>> tests{
            {"streetStateRoundTrips", streetStateRoundTrips},
            {"theftsByMarket", theftsByMarket},
            {"playersEmptyTheStaple", playersEmptyTheStaple},
            {"standingStillInTheSociety", standingStillInTheSociety},
            {"businessFromSales", businessFromSales},
            {"theDole", theDole},
            {"liftsAndTheirCap", liftsAndTheirCap},
            {"shortByBand", shortByBand},
            {"hungryThievesAtTheTill", hungryThievesAtTheTill},
            {"takesByBand", takesByBand},
            {"beggarsAndAlms", beggarsAndAlms},
            {"helpABeggar", helpABeggar},
            {"restdayDole", restdayDole},
            {"roughSleepers", roughSleepers},
            {"householdsBuyTheirWares", householdsBuyTheirWares},
            {"standingStillInTheShop", standingStillInTheShop},
            {"streetThroughACrash", streetThroughACrash},
        };
        const std::string only = argc == 3 && std::string(argv[1]) == "--only" ? argv[2] : "";
        for (const auto& [name, test] : tests)
            if (only.empty() || only == name)
            {
                const int before = checks;
                test();
                if (!only.empty() || std::getenv("STREET_EACH"))
                    std::cout << name << ": " << checks - before << " checks\n";
            }
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "street tests passed (" << checks << " checks)\n";
    return 0;
}
