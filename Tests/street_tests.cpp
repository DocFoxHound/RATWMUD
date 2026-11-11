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

// ------------------------------------------------------------------ A market of shops: the business words, the wares

// `n` households (as rows) and a market's shops of DEV's kinds: two butchers, a greens stall, a provisioner (cheese, the
// luxury), a preserver, a smith, an armourer, an apothecary, a barber, a moneychanger, a baker. One market.
const std::vector<std::pair<std::string, std::string>>& bazaarShops()
{
    static const std::vector<std::pair<std::string, std::string>> shops{
        {"k_butcher", "butcher at Moss & Marrow"},       {"k_butcher2", "sausage butcher on Market Square"},
        {"k_greens", "greens stall on Market Square"},   {"k_prov", "provisioner at Bridge Stores"},
        {"k_preserve", "preserver at The Apple Loft"},   {"k_smith", "smith at Eastgate Ironmongers"},
        {"k_armour", "armourer at The Rivet and Rain"},  {"k_apoth", "apothecary at Hollis Physic"},
        {"k_barber", "barber at The Barber's Pole"},     {"k_money", "moneychanger at The Greatly Bank"},
        {"k_baker", "baker at The Wet Loaf"}};
    return shops;
}
Society bazaar(int n)
{
    AuthoredRoster roster;
    int i = 0;
    for (const auto& [id, work] : bazaarShops())
    {
        const auto shop = "shop_" + std::to_string(i++);
        roster.residents.push_back(person(id, "merchant", work, 40 + i, {shop + "_up", 2.5, 2.5}, {shop, 2.5, 4.5}));
    }
    for (int k = 0; k < n; ++k)
    {
        const auto home = "row_" + std::to_string(k);
        const auto a = "h" + std::to_string(k) + "a";
        roster.residents.push_back(person(a, "civilian", k % 3 == 0 ? "" : "hauling crates", 30 + k % 20, {home, 2.5, 2.5}, {"yard", 3.5, 3.5}));
        if (k % 2)
        {
            const auto b = "h" + std::to_string(k) + "b";
            roster.residents.push_back(person(b, "civilian", "mending nets", 28 + k % 15, {home, 3.5, 2.5}, {"yard", 4.5, 3.5}));
            roster.kin.push_back({a, b, "spouse"});
        }
    }
    Society s(Roster::None);
    s.configure(roster);
    s.tendCareers(1.3, careers(s));
    s.setStagedHooks({nullptr, nullptr, &Society::tillShelf});
    return s;
}
// Days of the bazaar at a slider: each morning its keepers set out their wares (as the World's do), each day's hours
// ticked; the business words of every shop from day `from` on, counted.
std::map<std::string, int> bazaarDays(Society& s, int slider, int days, int from = 6,
                                      std::map<std::string, std::map<std::string, int>>* byShop = nullptr)
{
    s.setProsperity(slider, true);
    std::map<std::string, int> words;
    for (int d = 2; d < 2 + days; ++d)
        for (int h = 0; h < 24; ++h)
        {
            if (h == 7)
                for (const auto& [id, work] : bazaarShops())
                    s.stagedRestock(id);
            s.tick(1., d + h / 24. + 1e-4, 0, {});
            if (h == 1 && d >= from)
                for (const auto& [id, work] : bazaarShops())
                {
                    const auto word = s.businessOf(id);
                    ++words[word];
                    if (byShop)
                        ++(*byShop)[id][word];
                }
        }
    return words;
}
std::string wordsText(const std::map<std::string, int>& w)
{
    std::string out;
    for (const auto& [k, n] : w)
        out += k + " " + std::to_string(n) + " ";
    return out;
}

void businessInOrdinaryTimes()
{
    // 10.3 / T9: at ordinary times most shops are steady, a few thriving or failing (never every butcher and greengrocer
    // failing because nothing buys their goods); hard times bring more failing, a boom more thriving.
    std::map<std::string, std::map<std::string, int>> ordinaryByShop;
    auto at0 = bazaar(120);
    const auto ordinary = bazaarDays(at0, 0, 16, 6, &ordinaryByShop);
    auto atHard = bazaar(120);
    const auto hard = bazaarDays(atHard, -100, 16);
    auto atBoom = bazaar(120);
    const auto boom = bazaarDays(atBoom, 100, 16);
    if (std::getenv("STREET_EACH"))
    {
        std::cout << "  business at 0: " << wordsText(ordinary) << "\n  at -100: " << wordsText(hard) << "\n  at +100: " << wordsText(boom) << "\n";
        for (const auto& [id, w] : ordinaryByShop)
            std::cout << "    " << id << ": " << wordsText(w) << "\n";
    }
    const auto share = [](const std::map<std::string, int>& w, const char* word) {
        int all = 0;
        for (const auto& [k, n] : w)
            all += n;
        const auto found = w.find(word);
        return all ? double(found == w.end() ? 0 : found->second) / all : 0.;
    };
    expect(share(ordinary, "steady") >= .6, "ordinary times: most shops steady: " + wordsText(ordinary));
    expect(share(ordinary, "failing") <= .12, "ordinary times: few shops failing: " + wordsText(ordinary));
    for (const char* id : {"k_butcher", "k_butcher2", "k_greens", "k_preserve", "k_smith", "k_apoth"})
        expect(ordinaryByShop[id]["failing"] <= 2, std::string("an ordinary butcher, greengrocer, preserver, smith or apothecary isn't failing day after day: ") +
                                                       id + " " + wordsText(ordinaryByShop[id]));
    expect(ordinaryByShop["k_barber"]["thriving"] <= 3 && ordinaryByShop["k_money"]["thriving"] <= 3,
           "services don't make a barber or a moneychanger thrive every day: " + wordsText(ordinaryByShop["k_barber"]) + "/ " +
               wordsText(ordinaryByShop["k_money"]));
    expect(share(hard, "failing") > share(ordinary, "failing"), "hard times: more failing: " + wordsText(hard));
    expect(share(boom, "thriving") > share(ordinary, "thriving"), "a boom: more thriving: " + wordsText(boom));
    expect(share(boom, "thriving") < .95, "a boom doesn't make every shop thriving: " + wordsText(boom));
}

void wareBuyingByBand()
{
    // 6.5: what a household buys besides its food fits its band: no poor or destitute wolf takes a steel sword, armour or
    // theriac off the shelves (a piece at most the top of its purse); the comfortable and rich buy luxuries (cheese, T10);
    // services go by band, the poor's only a barber's; a service puts nothing on a shelf, never sells out a "ware" before
    // noon, and moves no till for the journal.
    auto s = bazaar(240);
    bazaarDays(s, -100, 6);
    const auto& t = s.statusTable();
    int cheese = 0;
    for (const auto& [id, life] : s.state().residents)
    {
        const auto* st = s.statusOf(id);
        const auto* a = s.account(id);
        if (!st || !a)
            continue;
        const auto most = t.rows[std::size_t(std::clamp(st->band, 0, 3))].purseHigh;
        for (const auto& [item, n] : a->stock)
        {
            if (n > 0 && st->band <= status::Poor && Society::nourishment(item) <= 0)
                expect(Society::catalogPrice(item) <= most, "a poor or destitute wolf holds no ware past its purse: " + id + " " + item +
                                                              " " + std::to_string(Society::catalogPrice(item)) + "p");
            if (n > 0 && st->band == status::Comfortable && items::baseOf(item) == "cheese")
                ++cheese;
        }
    }
    expect(cheese > 0, "the comfortable bought the luxury (cheese is a luxury before it is the rich's food)");
    // Services: a barber's and a moneychanger's trade; the poor's only the barber's.
    auto poorOnly = parse(R"({"rows": {"destitute": {"services": 0}, "poor": {"services": 1}, "comfortable": {"services": 0}, "rich": {"services": 0}}})");
    s.setStatusOverride(&poorOnly);
    const auto trade = s.ordinaryTrade();
    const auto of = [&](const char* keeper) {
        const auto found = trade.find(s.tillOf(keeper));
        return found == trade.end() ? 0. : found->second;
    };
    expect(of("k_barber") > 0 && of("k_money") == 0, "the poor have only a barber's services: barber " + std::to_string(of("k_barber")) +
                                                         ", moneychanger " + std::to_string(of("k_money")));
    s.setStatusOverride(nullptr);
    // A service before noon: no "sold out before noon", no till moved for the journal; it counts as the business's trade.
    const auto till = s.tillOf("k_money");
    s.resumeAt(20.4);                               // (9:36.)
    s.noteSale(till, "", 1, false);                 // (The day's first: its record rolled to today.)
    (void)s.takeTillsMoved();
    const int before = s.state().staged.business.at(till).sold;
    s.noteSale(till, "", 1, false);
    const auto& b = s.state().staged.business.at(till);
    expect(!b.outEarly && b.sold == before + 1, "a service done: counted, no ware out before noon");
    expect(!s.takeTillsMoved().count(till), "and no shelf moved for the journal");
}

void businessRecordNeverStepsBack()
{
    // 10.3: a sale between the day's turn at midnight and the market's dawn is today's; the record never rolls back a day.
    auto s = bazaar(20);
    const auto till = s.tillOf("k_smith");
    s.resumeAt(6.4);
    s.noteSale(till, "nails", 20, false);
    s.businessDay(7);                               // (Midnight: day 7.)
    s.resumeAt(7.1);                                // (2:24 on day 7: the market's day is still 6 till dawn.)
    s.noteSale(till, "nails", 1, true);
    const auto& b = s.state().staged.business.at(till);
    expect(b.day == 7 && b.soldBefore[0] == 20 && b.players == 1, "the night's purchase counted in today's, yesterday's 20 kept: day " +
                                                                      std::to_string(b.day) + ", before " + std::to_string(b.soldBefore[0]));
    s.businessDay(8);
    const auto& c = s.state().staged.business.at(till);
    expect(c.soldBefore[1] == 20 && c.playersBefore[0] == 1, "and the next night the days move on as they should");
}

void failingOnceARun()
{
    // 10.3: three days failing add one -0.3 status event to the keeper's household, not one every night after. (A smith
    // whose shelf stands bare, day after day, in ordinary times: nothing to buy, nothing sold.)
    auto s = bazaar(240);
    s.setProsperity(0, true);
    const auto till = s.tillOf("k_smith");
    const auto bare = [&] {
        for (const auto& [item, n] : std::map<std::string, int>(s.account(till)->stock))
            if (n > 0)
                s.consume(till, item, n, "test: the shelf bare");
    };
    for (int d = 2; d < 16; ++d)
        for (int h = 0; h < 24; ++h)
        {
            if (h == 7)
                bare();
            s.tick(1., d + h / 24. + 1e-4, 0, {});
        }
    expect(s.businessOf("k_smith") == "failing", "a smith that sells nothing in ordinary times: failing: " + s.businessOf("k_smith"));
    expect(s.state().staged.business.at(till).failing >= 8, "failing night after night: " + std::to_string(s.state().staged.business.at(till).failing));
    int events = 0;
    if (s.state().staged.events.count("k_smith"))
        for (const auto& e : s.state().staged.events.at("k_smith"))
            events += e.kind == "failing";
    expect(events == 1, "nights failing running: one event, not one a night: " + std::to_string(events));
}

void runnerGivingUpLate()
{
    // 6.6 / 7.7: a runner sent at the shops' last hour who gives up after it: the household is fed from elsewhere at once
    // (the comfortable always), not left without its day's food.
    auto s = docks();
    s.setStagedHooks({nullptr, nullptr, &Society::tillShelf});
    bool sendNow = false;
    s.setStreetHooks({[&](const Society&, const std::string&) { return true; },
                      [&](Society&, const std::string&, const std::string&) { return sendNow; }});
    s.stagedBands(2, true);
    s.overrideBand(s.householdOf("lady"), status::Comfortable, 100);
    emptyLarder(s, "lady");
    stallWith(s, 30);
    const auto household = s.householdOf("lady");
    for (int h = 0; h < 17; ++h)
        s.tick(1., 3 + h / 24. + 1e-4, 0, {});
    sendNow = true;
    s.tick(1., 3 + 17 / 24. + 1e-4, 0, {});         // (17:00: the waiting food tried again, a runner sent.)
    expect(s.runnerWaiting(household), "a runner sent at the last hour");
    s.tick(1., 3 + 17.6 / 24., 0, {});
    s.runnerFailed(household);                      // (17:36: it gives up.)
    expect(food(s, "lady") >= Society::NourishADay, "the comfortable fed from elsewhere when the late runner gives up: " +
                                                        std::to_string(food(s, "lady")));
}

void guardsNeverThieve()
{
    // 10.1: never a guard among the day's thieves, posted or not (DEV's postless Watch guards).
    AuthoredRoster roster;
    roster.residents.push_back(person("moss", "merchant", "keeping the stall", 45, {"stall_up", 2.5, 2.5}, {"stall", 2.5, 4.5}));
    for (int i = 0; i < 12; ++i)
        roster.residents.push_back(person("g" + std::to_string(i), "guard", "", 30, {"watch_house", 2.5 + i, 2.5}, {"watch_house", 3.5, 3.5}));
    roster.residents.push_back(person("tom", "civilian", "", 30, {"row_t", 2.5, 2.5}, {"yard", 3.5, 3.5}));
    auto placed = person("g_placed", "civilian", "-", 30, {"watch_house", 15.5, 2.5}, {"watch_house", 15.5, 2.5});
    placed.watch = true;                            // (Authored a guard, placed with no work spot: DEV's.)
    roster.residents.push_back(placed);
    Society s(Roster::None);
    s.configure(roster);
    s.tendCareers(1.3, careers(s));
    auto often = parse(R"({"street": {"theftBase": 5}})");
    s.setStatusOverride(&often);
    std::map<std::string, std::string> sly;
    for (int i = 0; i < 12; ++i)
        sly["g" + std::to_string(i)] = "sly";
    sly["g_placed"] = "sly";
    s.setHonesty(sly);
    s.setProsperity(-100, true);
    s.stagedBands(10, true);
    for (int i = 0; i < 12; ++i)
        s.overrideBand(s.householdOf("g" + std::to_string(i)), status::Destitute, 1000);
    int thieves = 0;
    for (int d = 10; d < 40; ++d)
        for (const auto& t : s.theftDay(d, nullptr).thieves)
        {
            expect(!s.spec(t.id)->ofTheWatch(), "a guard among the day's thieves: " + t.id);
            ++thieves;
        }
    expect(thieves > 0, "the day's thieves drawn, from those not of the Watch");
}

void theWatchInTheBands()
{
    // 6.3 / 10: one of the Watch with no post (a guard written with none, and DEV's patrol guards placed with no work spot,
    // each a household of one at the Watch House) is paid by its town: an earner, and its household never poor or
    // destitute at any slider, so never on the dole; even the DM's override never gives it the dole.
    AuthoredRoster roster;
    roster.residents.push_back(person("moss", "merchant", "keeping the stall", 45, {"stall_up", 2.5, 2.5}, {"stall", 2.5, 4.5}));
    std::vector<std::string> watch;
    for (int i = 0; i < 6; ++i)
    {
        roster.residents.push_back(person("g" + std::to_string(i), "guard", "", 30, {"watch_house", 2.5 + i, 2.5}, {"watch_house", 3.5, 3.5}));
        watch.push_back("g" + std::to_string(i));
        auto placed = person("p" + std::to_string(i), "civilian", "-", 30, {"watch_house", 10.5 + i, 2.5}, {"watch_house", 10.5 + i, 2.5});
        placed.watch = true;                        // (Authored a guard, placed with no work spot: DEV's.)
        roster.residents.push_back(placed);
        watch.push_back("p" + std::to_string(i));
    }
    for (int i = 0; i < 12; ++i)
        roster.residents.push_back(person("c" + std::to_string(i), "civilian", "", 40 + i, {"row_" + std::to_string(i), 2.5, 2.5}, {"yard", 3.5, 3.5}));
    Society s(Roster::None);
    s.configure(roster);
    s.tendCareers(1.3, careers(s));
    int destituteOthers = 0;
    for (const int slider : {-100, 0, 100})
    {
        s.setProsperity(slider, true);
        s.stagedBands(10, true);
        for (const auto& id : watch)
        {
            const auto* st = s.statusOf(id);
            expect(s.spec(id)->ofTheWatch() && (id[0] != 'p' || !s.jobOf(id)), "of the Watch (the placed with no post): " + id);
            expect(st && st->band >= status::Comfortable,
                   "one of the Watch never poor or destitute, at " + std::to_string(slider) + ": " + id + " " + std::to_string(st ? st->band : -1));
            expect(s.watchHousehold(st->household) && s.giveDole(st->household, 10) == 0, "no dole for the Watch: " + id);
        }
        for (int i = 0; i < 12; ++i)
            if (const auto* st = s.statusOf("c" + std::to_string(i)); st && st->band == status::Destitute)
                ++destituteOthers;
    }
    expect(destituteOthers > 0, "the destitute are others: the households without an earner");
    for (const auto& id : watch)
    {
        expect(s.overrideBand(s.householdOf(id), status::Destitute, 1000), "the DM's override: " + id);
        expect(s.statusOf(id)->band == status::Destitute && s.giveDole(s.householdOf(id), 10) == 0,
               "the DM's override holds, and still no dole for the Watch: " + id);
    }
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

void buyersAtAWatchedShop()
{
    // 7.7: near a player, a buy besides food needs a buyer at the counter (one of the household's own, the buyer hook):
    // the shelf of a watched shop moves only as each buyer there buys; with none to send, the buy waits, and goes undone
    // at the shutting.
    for (const bool send : {true, false})
    {
        auto s = bazaar(60);
        std::vector<std::tuple<std::string, std::string, std::string>> asked;
        s.setStreetHooks({[](const Society&, const std::string&) { return true; },
                          [](Society&, const std::string&, const std::string&) { return false; },
                          [&](Society&, const std::string& household, const std::string& till, const std::string& ware) {
                              asked.push_back({household, till, ware});
                              return send;
                          }});
        s.setProsperity(0, true);
        std::map<std::string, std::map<std::string, int>> before;
        for (const auto& [id, work] : bazaarShops())
            before[s.tillOf(id)] = s.account(s.tillOf(id))->stock;
        int bought = 0;
        for (int h = 0; h < 24; ++h)
        {
            s.tick(1., 2 + h / 24. + 1e-4, 0, {});
            if (send)
                for (const auto& [household, till, ware] : std::exchange(asked, {}))
                    bought += s.runnerBuy(household, till, ware);
        }
        int fell = 0;
        for (const auto& [till, stock] : before)
            for (const auto& [item, n] : stock)
                fell += std::max(0, n - Society::stock(*s.account(till), item));   // (The food waits too: no runner.)
        if (send)
            expect(bought > 0 && fell == bought, "each piece gone with a buyer at the counter: " + std::to_string(fell) + " for " + std::to_string(bought));
        else
            expect(fell == 0 && !asked.empty(), "with no buyer to send, nothing leaves a watched shelf: " + std::to_string(fell));
    }
}

void businessFromSales()
{
    // 10.3: a shop whose shelf sells down is thriving; one whose shelf doesn't move is failing (against its ordinary trade),
    // and three days failing is a status event for its keeper's household; "none" before its first day's end. (In the
    // bazaar: a lone stall's shelf in the docks has too little ordinary trade to tell.)
    auto s = bazaar(240);
    s.setProsperity(0, true);
    for (int h = 0; h < 30; ++h)
        s.tick(1., 2 + h / 24. + 1e-4, 0, {});
    const auto till = s.tillOf("k_smith");
    expect(s.businessOf("h1a") == "none", "none for a non-keeper");
    {
        auto fresh = bazaar(20);
        expect(fresh.businessOf("k_smith") == "none", "no business before a day's end");
    }
    const auto trade = s.ordinaryTrade();
    expect(trade.count(till) && trade.at(till) * 2 >= 3, "the smith's ordinary trade is enough to tell: " +
                                                           std::to_string(trade.count(till) ? trade.at(till) : 0.));
    // Days of nothing sold.
    for (int d = 4; d <= 8; ++d)
        s.businessDay(d);
    expect(s.businessOf("k_smith") == "failing", "a shelf that never moves: failing: " + s.businessOf("k_smith"));
    bool event = false;
    for (const auto& e : s.state().staged.events.count("k_smith") ? s.state().staged.events.at("k_smith") : std::vector<StagedState::Event>{})
        event |= e.kind == "failing";
    expect(event, "three days failing: a status event for the keeper");
    // A day that sells its shelves down (its whole shelves' targets): thriving.
    int target = 0;
    for (const auto& ware : s.stagedShelfGoods(s.jobOf("k_smith")->founder))
        target += s.shelfTarget(till, ware);
    s.resumeAt(8.4);
    s.noteSale(till, "nails", std::max(1, target), false);
    s.businessDay(9);
    expect(s.businessOf("k_smith") == "thriving", "a shelf sold down: thriving: " + s.businessOf("k_smith") + " (target " + std::to_string(target) + ")");
    expect(s.state().staged.business.at(till).failing == 0, "the failing run broken");
    // Players' buying counts apart.
    s.resumeAt(9.4);
    s.noteSale(till, "nails", 1, true);
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
    // A gift of a fortnight's food lifts a fortnight, as the short trouble's help of the same coins does.
    expect(s.liftByHelp(poor[2], s.bandFoodCost(poor[2], 14)) == 14, "a fortnight's: a fortnight");
    int lifted = 3;
    for (std::size_t i = 3; i < poor.size(); ++i)
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

// Greyfen's own world, copied, with a priest keeping a chapel in the Long House (indoors; its door gives on the square);
// `lane`: a second street (an outdoor cell of Greyfen's market beside the square) with two barrow-keepers in it, so the
// market has more than one cell to beg in.
std::vector<fs::path>& madeRoots()
{
    static std::vector<fs::path> roots;             // (Every copy and save made, removed at the end, passed or failed.)
    return roots;
}
fs::path greyfen(const std::string& name, bool lane = false)
{
    const auto root = fs::temp_directory_path() / ("ratw-street-" + name + "-" + std::to_string(::getpid()));
    fs::remove_all(root);
    madeRoots().push_back(root);
    fs::copy(RATW_SOURCE_DIR "/Data/Worlds/Greyfen", root, fs::copy_options::recursive);
    std::ofstream(root / "world.ratw", std::ios::app)
        << "resident \"vicar\" \"Vicar Holm\" \"civilian\" \"the priest of the chapel\" \"A grey priest.\" \"Peace.\" 52 \"timber\" "
           "\"male\" \"average\" \"saddle\" 2 1 5 11 1 8 17 \"-\" 30 0 1 \"longhouse\" 3.5 2.5 \"longhouse\" 12.5 3.5 \"longhouse\" "
           "12.5 6.5\n";
    if (lane)
    {
        auto town = slurp(root / "cells" / "town.cell");
        town.replace(town.find("id: town"), 8, "id: lane");
        town.replace(town.find("world: 0 0 0"), 12, "world: 0 40 0");
        std::ofstream(root / "cells" / "lane.cell", std::ios::binary) << town;
        std::ofstream(root / "world.ratw", std::ios::app)
            << "cell \"lane\" \"cells/lane.cell\"\nterritory \"lane\" \"greyfen\" \"-\" 0\n"
            << "resident \"cole\" \"Cole\" \"merchant\" \"keeping shop\" \"A lean wolf with a barrow.\" \"Odds and ends.\" 40 \"timber\" "
               "\"male\" \"average\" \"saddle\" 5 2 5 14 1 8 18 \"-\" 40 0 1 \"longhouse\" 22.5 9.5 \"lane\" 20.5 17.5 \"lane\" 22.5 20.5\n"
            << "resident \"dulse\" \"Dulse\" \"merchant\" \"keeping shop\" \"A quiet wolf with a barrow.\" \"Fresh today.\" 38 \"red\" "
               "\"female\" \"average\" \"saddle\" 5 2 5 14 1 8 18 \"-\" 40 0 1 \"longhouse\" 3.5 9.5 \"lane\" 40.5 17.5 \"lane\" 42.5 20.5\n";
        // Four wolves with no trade, each in a hovel of its own (its own household), and a guard of the Watch off duty with
        // no post (as DEV's many), in a fifth.
        const auto cottage = slurp(root / "cells" / "cottage.cell");
        for (int i = 1; i <= 5; ++i)
        {
            auto hovel = cottage;
            const auto id = "hovel" + std::to_string(i);
            hovel.replace(hovel.find("id: cottage"), 11, "id: " + id);
            std::ofstream(root / "cells" / (id + ".cell"), std::ios::binary) << hovel;
            std::ofstream(root / "world.ratw", std::ios::app)
                << "cell \"" << id << "\" \"cells/" << id << ".cell\"\nterritory \"" << id << "\" \"greyfen\" \"-\" 0\n";
        }
        for (int i = 1; i <= 4; ++i)
            std::ofstream(root / "world.ratw", std::ios::app)
                << "resident \"idle" << i << "\" \"Idle " << i << "\" \"civilian\" \"-\" \"A thin wolf.\" \"Spare a penny?\" " << 30 + i
                << " \"timber\" \"" << (i % 2 ? "female" : "male") << "\" \"average\" \"saddle\" 5 2 5 14 1 8 18 \"-\" 5 0 1 \"hovel" << i
                << "\" 2.5 7.5 \"town\" " << 30.5 + i << " 17.5 \"town\" " << 30.5 + i << " 20.5\n";
        std::ofstream(root / "world.ratw", std::ios::app)
            << "resident \"gorse\" \"Gorse\" \"guard\" \"-\" \"A Watch guard in a sodden cloak.\" \"Move along.\" 35 \"timber\" "
               "\"male\" \"average\" \"saddle\" 5 2 5 14 1 8 18 \"-\" 5 0 1 \"hovel5\" 2.5 7.5 \"town\" 36.5 17.5 \"town\" 36.5 20.5\n"
            // (And one written as DEV's are, on patrol with no work spot: the loader makes it a civilian with no post.)
            << "resident \"heath\" \"Heath\" \"guard\" \"on patrol\" \"A Watch guard in a sodden cloak.\" \"Keep moving.\" 41 \"timber\" "
               "\"female\" \"average\" \"saddle\" 5 2 5 14 1 6 18 \"-\" 5 0 1 \"hovel5\" 3.5 7.5 \"-\" 0 0 \"hovel5\" 3.5 7.5\n";
    }
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
    // keeper's own purse; hungry is hunger and less than a day's food in hand (a crust is no meal), and only at a shop its
    // keeper is keeping. The Watch's restitution of a till's food goes back on its shelf. (Greyfen's theft base is made
    // high so its day has thieves; every resident starving, a crust of bread each.)
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
                st.accounts[id].stock["bread"] = 1;     // (Less than a day's food: still hungry.)
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
    std::size_t seen = 0;
    for (int i = 0; i < 7200 && !fromTill; ++i)
    {
        w.tick(1);
        if (i % 600 == 0)
            starve();
        for (; seen < w.crime().incidents.size(); ++seen)
        {
            const auto& inc = w.crime().incidents[seen];
            if (inc.kind == "theft" && !inc.item.empty() && Society::edible(inc.item) && w.society().merchant(inc.victim))
            {
                fromTill = true;
                const auto* keeping = w.society().resident(inc.victim);
                expect(keeping && keeping->task == "trade", "a till robbed only while its keeper keeps the shop: " + inc.victim + " " +
                                                                 (keeping ? keeping->task : std::string()));
                expect(inc.from == w.society().tillOf(inc.victim), "the theft says the till the food came from");
            }
        }
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
    // Restitution: the Watch has the thief pay back; the food still in its paws goes back on the till's shelf, not into
    // the keeper's own wallet.
    const Incident* robbed = nullptr;
    for (const auto& inc : w.crime().incidents)
        if (inc.kind == "theft" && !inc.from.empty())
            robbed = &inc;
    const auto thief = robbed->offender, keeper = robbed->victim, till = robbed->from, item = robbed->item;
    const int quantity = robbed->quantity;
    expect(w.society().create(thief, item, quantity, "test: still in its paws"), "the stolen food in the thief's paws");
    testPurse(w.society(), thief, 50, "test");
    w.crime().warrants.push_back({thief, robbed->town, {robbed->id}, {{keeper, item, quantity, 0, till}}, 0, w.calendarDays()});
    const int onShelf = Society::stock(*w.society().account(till), item), inWallet = Society::stock(*w.society().account(keeper), item);
    w.setTimeOfDay(12);
    for (int i = 0; i < 1200 && std::any_of(w.crime().warrants.begin(), w.crime().warrants.end(), [&](const Warrant& x) { return x.person == thief; }); ++i)
    {
        auto* t = w.entity(thief);
        const auto* g = w.entity("harrow");
        t->cellId = g->cellId;
        t->position = {g->position.x + 1, g->position.y};
        t->path.clear();
        t->velocity = {};
        w.tick(1);
    }
    expect(std::none_of(w.crime().warrants.begin(), w.crime().warrants.end(), [&](const Warrant& x) { return x.person == thief; }),
           "the Watch settles the warrant");
    expect(Society::stock(*w.society().account(till), item) >= onShelf + quantity && Society::stock(*w.society().account(keeper), item) <= inWallet,
           "the food back on the till's shelf, not the keeper's wallet: shelf " + std::to_string(Society::stock(*w.society().account(till), item)) +
               " from " + std::to_string(onShelf));
    // Never a player (D11): a player beside every thief all day is never robbed.
    expect(w.society().realBalanced(), "real money balanced");
    fs::remove_all(root);
}

void liftedThievesStop()
{
    // 6.7 / 10.1: a household lifted by a player's help after the day's thieves are drawn steals no more that day.
    const auto root = greyfen("lifted");
    auto w = town(root);
    auto often = parse(R"({"street": {"theftBase": 5}})");
    w.society().setStatusOverride(&often);
    w.advanceCalendar(1);
    w.setTimeOfDay(8.5);
    for (int i = 0; i < 5; ++i)
        w.tick(1);
    const auto plan = w.theftPlan();
    expect(!plan.thieves.empty(), "the day has thieves");
    std::set<std::string> thieves;
    for (const auto& t : plan.thieves)
    {
        thieves.insert(t.id);
        w.entity(t.id)->sneakSkill = 300;
        expect(w.society().liftHousehold(w.society().householdOf(t.id), std::int64_t(std::floor(w.calendarDays())) + 7), "lifted: " + t.id);
    }
    const auto before = w.crime().incidents.size();
    for (int i = 0; i < 7200; ++i)
        w.tick(1);
    for (std::size_t k = before; k < w.crime().incidents.size(); ++k)
        expect(!thieves.count(w.crime().incidents[k].offender), "a lifted household's thief stole that day: " + w.crime().incidents[k].offender);
    fs::remove_all(root);
}

void aFreshWorldsFirstDay()
{
    // 10.1 / 10.2: on a fresh world's first day the day's thieves are drawn once the households are made (an empty plan
    // isn't kept for the day), and the beggars placed then, not the next hour.
    const auto root = greyfen("fresh");
    World w;
    const auto loaded = w.loadWorldFile((root / "world.ratw").string());
    expect(loaded.ok, "the town loads: " + loaded.message);
    // (Greyfen's households comfortable, the Watch's barracks too: their theft weight made high.)
    auto often = parse(R"({"street": {"theftBase": 1}, "rows": {"comfortable": {"theft": 1}}})");
    w.society().setStatusOverride(&often);
    w.setTimeOfDay(9);
    expect(w.theftPlan().thieves.empty(), "before the households are made: no thieves yet");
    for (int i = 0; i < 120; ++i)
        w.tick(1);
    expect(!w.theftPlan().thieves.empty(), "once they are: the first day's thieves");
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
    const auto root = greyfen("beg", true);
    auto w = town(root);
    for (const auto* id : {"idle1", "idle2"})
        destitute(w, id);
    for (const auto* id : {"idle3", "idle4"})
        expect(w.society().overrideBand(w.society().householdOf(id), status::Poor, 100000), "poor, not yet destitute: " + std::string(id));
    w.addPlayer("player-ada", "Ada");
    onTo(w, calendar::Marketday, 10);
    // Who may beg: destitute adults without a post or a master, never one of the Watch.
    const auto eligible = [&] {
        int n = 0;
        for (const auto& [id, life] : w.society().state().residents)
            if (const auto* st = w.society().statusOf(id); st && st->band == status::Destitute && !w.society().jobOf(id) &&
                                                            !w.society().apprenticedTo(id) && w.society().spec(id)->age >= 16 &&
                                                            !w.society().spec(id)->ofTheWatch())
                ++n;
        return n;
    };
    const int few = eligible();
    if (std::getenv("STREET_DEBUG"))
        for (const auto* id : {"idle1", "idle2", "idle3", "idle4", "gorse"})
        {
            const auto* st = w.society().statusOf(id);
            std::cerr << id << " household " << w.society().householdOf(id) << " band " << (st ? st->band : -9) << " job "
                      << (w.society().jobOf(id) != nullptr) << " weekday " << calendar::weekdayOf(w.calendarDays()) << " hour "
                      << (w.calendarDays() - std::floor(w.calendarDays())) * 24 << "\n";
        }
    const auto some = w.beggars().size();
    // T7: more destitute, more beggars (not the same few at any slider: the market's shops' doors and stalls are its
    // spots too, two a cell); a guard of the Watch without a post, destitute, never begs.
    expect(!w.society().jobOf("gorse") && w.society().spec("gorse")->role == "guard", "a guard without a post");
    expect(!w.society().jobOf("heath") && w.society().spec("heath")->ofTheWatch(), "a guard placed with no work spot: no post, still of the Watch");
    for (const auto* id : {"idle3", "idle4", "gorse"})
        destitute(w, id);
    w.setTimeOfDay(11.2);                           // (The next hour's places.)
    for (int i = 0; i < 30; ++i)
        w.tick(1);
    const auto beggars = w.beggars();
    if (std::getenv("STREET_EACH"))
        std::cout << "  beggars: " << some << " of " << few << " who may, then " << beggars.size() << " of " << eligible() << "\n";
    expect(int(some) == few, "every destitute adult who may beg has a place to: " + std::to_string(some) + " of " + std::to_string(few));
    expect(eligible() > few && int(beggars.size()) == eligible() && beggars.size() > some,
           "more destitute, more beggars (the old two spots held four): " + std::to_string(beggars.size()) + " of " + std::to_string(eligible()));
    expect(!beggars.count("gorse") && !beggars.count("heath") && !beggars.count("harrow"), "never a guard");
    for (const auto& [id, at] : beggars)
    {
        const auto* st = w.society().statusOf(id);
        expect(st && st->band == status::Destitute && !w.society().jobOf(id) && w.society().spec(id)->age >= 16 &&
                   !w.society().spec(id)->ofTheWatch(),
               "a beggar is a destitute adult without a post, not of the Watch: " + id);
    }
    // At most two at one place (a place, BeggarApart tiles from the next: 10.2's "two a cell").
    for (const auto& [id, at] : beggars)
    {
        int near = 0;
        for (const auto& [other, there] : beggars)
            near += there.cell == at.cell && std::hypot(there.x - at.x, there.y - at.y) < World::BeggarApart / 2;
        expect(near <= 2, "at most two at one place: " + id + " " + std::to_string(near));
    }
    for (const auto& [id, life] : w.society().state().residents)
        if (!beggars.count(id))
            if (const auto* st = w.society().statusOf(id); st && st->band == status::Destitute && !w.society().jobOf(id) &&
                                                            !w.society().apprenticedTo(id) && w.society().spec(id)->age >= 16 &&
                                                            !w.society().spec(id)->ofTheWatch())
                expect(false, "a destitute adult who may beg left without a place: " + id);
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
        {
            // (The beggar at its place: a hovel of this test's has no door to walk out of.)
            auto* b = w.entity(beggar);
            const auto& at = beggars.at(beggar);
            b->cellId = at.cell;
            b->position = {at.x, at.y};
            b->path.clear();
            b->velocity = {};
        }
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
    // A staged move between two residents' purses: no ledger entry, nothing journalled (9.2).
    for (const auto& e : w.society().state().ledger)
        expect(e.kind != "alms", "alms in the ledger: " + e.from + " to " + e.to);
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
    {
        auto* me = w.entity("player-ada");          // (Away in the barracks: nobody near the chapel's steps to see.)
        me->cellId = "barracks";
        me->position = {5.5, 5.5};
    }
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

void doleQueue()
{
    // 10.2: with a player at the chapel's steps the dole is seen being handed out: one of each poor or destitute household
    // comes and waits in a queue (at most DoleQueue at once) and is handed the bread at the front, never before; whoever is
    // left when the dole's hours end is given it then.
    const auto root = greyfen("queue");
    auto w = town(root);
    for (const auto* id : {"linden", "rook"})
        destitute(w, id);
    w.addPlayer("player-ada", "Ada");
    onTo(w, calendar::Restday, Society::ServiceEnd - .05);
    // The steps: where the Long House's door comes out on the square.
    Spot steps;
    for (const auto& [id, d] : w.doors())
        if (d.cellId == "longhouse" && w.cell(d.targetCell) && w.cell(d.targetCell)->outdoors)
            steps = {d.targetCell, d.arrival.x, d.arrival.y};
    expect(!steps.cell.empty(), "the chapel's steps found");
    const auto stand = [&] {
        auto* me = w.entity("player-ada");
        me->cellId = steps.cell;
        me->position = {steps.x - 2, steps.y + 1};
        me->velocity = {};
    };
    std::set<std::string> homes;
    for (const auto& [id, life] : w.society().state().residents)
        if (const auto* st = w.society().statusOf(id); st && st->band <= status::Poor)
            homes.insert(st->household);
    expect(!homes.empty(), "households for the dole");
    const auto today = std::int64_t(std::floor(w.calendarDays()));
    std::map<std::string, bool> atFront;            // (Household -> its taker was at the front for its seconds.)
    std::size_t mostInLine = 0;
    bool servedSeen = false;
    for (int i = 0; i < 1300; ++i)
    {
        stand();
        for (const auto& [id, t] : w.doleTakers())
            if (t.place == 0 && t.atFront >= 0)
                atFront[t.household] = true;
        mostInLine = std::max(mostInLine, w.doleTakers().size());
        w.tick(1);
        for (const auto& h : homes)
            if (w.society().doleGiven(h, today) && !atFront[h])
            {
                const double hod = (w.calendarDays() - std::floor(w.calendarDays())) * 24;
                expect(hod >= Society::ServiceEnd + 2 - 1e-6, "a household given its bread at the steps with nobody of it at the front: " + h);
            }
            else if (w.society().doleGiven(h, today))
                servedSeen = true;
    }
    expect(servedSeen, "a household served at the front of the queue");
    expect(mostInLine > 0 && mostInLine <= std::size_t(World::DoleQueue), "a queue, at most DoleQueue: " + std::to_string(mostInLine));
    // The dole's hours over: the rest given.
    onTo(w, calendar::Restday, Society::ServiceEnd + 2.1);
    for (const auto& h : homes)
        expect(w.society().doleGiven(h, today), "every poor and destitute household given its dole by the hours' end: " + h);
    expect(w.doleTakers().empty(), "and the queue gone home");
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
    std::map<std::string, int> slept;               // (Each of the four: the winter nights it slept by a warm wall.)
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
            for (const auto* id : {"linden", "rook", "maple", "sorrel"})
                slept[id] += w.roughSleepers().count(id) ? 1 : 0;
            std::set<std::string> tiles;
            for (const auto& [id, at] : w.roughSleepers())
            {
                expect(w.society().statusOf(id) && w.society().statusOf(id)->band == status::Destitute, "a sleeper is destitute");
                // (D12: in a sheltered spot by the wall, on the street: never inside the bakery or the forge.)
                expect(w.cell(at.cell) && w.cell(at.cell)->outdoors, "a sleeper by the wall outside, not within: " + id + " in " + at.cell);
                expect(tiles.insert(at.cell + " " + std::to_string(int(at.x)) + "," + std::to_string(int(at.y))).second,
                       "never two sleepers on one tile: " + id);
            }
        }
    }
    // D12's "a third of winter nights": each one's nights by its own seeded lot, so the four together sleep out about a
    // third of theirs (120 lots: 40 expected, 24-56 within three spreads), and none every night or never.
    int all = 0;
    for (const auto& [id, n] : slept)
    {
        all += n;
        expect(n > 0 && n < wintry, "by the warm wall some winter nights, not every one: " + id + " " + std::to_string(n) + " of " +
                                        std::to_string(wintry));
    }
    expect(wintry >= 30 && nights > 0, "winter nights seen: " + std::to_string(wintry));
    expect(all >= wintry * 4 / 5 && all <= wintry * 4 * 7 / 15,
           "about a third of winter nights: " + std::to_string(all) + " of " + std::to_string(wintry * 4));
    if (std::getenv("STREET_EACH"))
        std::cout << "  rough sleepers: " << all << " sleeper-nights of " << wintry * 4 << "\n";
    fs::remove_all(root);
}

// A cell's every tile raised to `height` (a `height:` override each), as DEV's city streets stand at 1.5-2.5.
void raise(const fs::path& cellFile, double height)
{
    auto text = slurp(cellFile);
    const auto size = text.find("size: ");
    int w = 0, h = 0;
    std::istringstream(text.substr(size + 6)) >> w >> h;
    std::ostringstream lines;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            lines << "height: " << x << ' ' << y << ' ' << height << '\n';
    text.insert(text.find("grid:"), lines.str());
    std::ofstream(cellFile, std::ios::binary) << text;
}
std::string tileOf(const Spot& at)
{
    return at.cell + " " + std::to_string(int(std::floor(at.x))) + "," + std::to_string(int(std::floor(at.y)));
}
// A tile someone could stand on, at the raised street's height (outdoors) or the floor's (indoors).
bool groundAt(const World& w, const Spot& at, double height)
{
    const auto* c = w.cell(at.cell);
    const auto* t = c ? c->tile(int(std::floor(at.x)), int(std::floor(at.y))) : nullptr;
    return t && !t->solid && (!c->outdoors || std::abs(t->height - height) < 1e-9);
}

void streetOnRaisedGround()
{
    // 10.2: on raised streets (DEV's city: 1.5 at the chapel steps, 2.5 in its streets) the beggars at a place, the rough
    // sleepers by a wall and the dole's queue still stand side by side, each on a tile of its own, none on the priest's
    // or a beggar's. (The tiles beside a place were asked about a step up from height 0, so above half a step every one
    // looked blocked and a whole row stood on the place's one tile.)
    const auto root = greyfen("raised", true);
    for (const char* c : {"town", "lane"})
        raise(root / "cells" / (std::string(c) + ".cell"), 2.5);
    auto w = town(root);
    expect(w.cell("town")->tile(20, 17)->height == 2.5 && w.cell("lane")->tile(20, 17)->height == 2.5, "the streets raised");
    for (const auto* id : {"idle1", "idle2", "idle3", "idle4", "linden", "rook", "maple", "sorrel"})
        destitute(w, id);
    w.addPlayer("player-ada", "Ada");
    const auto apartAll = [&](const std::map<std::string, Spot>& at, const std::string& what, std::set<std::string>& tiles) {
        for (const auto& [id, spot] : at)
        {
            expect(groundAt(w, spot, 2.5), what + " on ground to stand on: " + id + " " + tileOf(spot));
            expect(tiles.insert(tileOf(spot)).second, what + ": two on one tile, " + id + " at " + tileOf(spot));
        }
    };
    onTo(w, calendar::Marketday, 10);
    {
        std::set<std::string> tiles;
        apartAll(w.beggars(), "beggars", tiles);
        int paired = 0;                             // (Places holding two: the row was beside a place, not on it.)
        for (const auto& [id, at] : w.beggars())
            for (const auto& [other, there] : w.beggars())
                paired += id < other && at.cell == there.cell && std::hypot(at.x - there.x, at.y - there.y) < 3;
        expect(w.beggars().size() >= 4 && paired > 0, "beggars side by side at their places: " + std::to_string(w.beggars().size()) +
                                                          ", pairs " + std::to_string(paired));
    }
    // The Restday dole with Ada at the steps: the queue beside the priest, never on his tile or a beggar's.
    onTo(w, calendar::Restday, Society::ServiceEnd - .05);
    Spot steps;
    for (const auto& [id, d] : w.doors())
        if (d.cellId == "longhouse" && w.cell(d.targetCell) && w.cell(d.targetCell)->outdoors)
            steps = {d.targetCell, d.arrival.x, d.arrival.y};
    std::size_t most = 0;
    for (int i = 0; i < 900; ++i)
    {
        auto* me = w.entity("player-ada");
        me->cellId = steps.cell;
        me->position = {steps.x - 2, steps.y + 2};
        me->velocity = {};
        w.tick(1);
        std::set<std::string> tiles;
        apartAll(w.beggars(), "beggars on Restday", tiles);
        std::map<std::string, Spot> line;
        for (const auto& [id, t] : w.doleTakers())
            line[id] = t.at;
        for (const auto& [id, at] : w.dolePriests())
            line["priest " + id] = at;
        apartAll(line, "the dole's queue, the priest and the beggars", tiles);
        most = std::max(most, w.doleTakers().size());
    }
    expect(most >= 2, "a queue at the steps: " + std::to_string(most));
    // Winter nights: the homeless destitute two to a warm wall, side by side.
    {
        auto st = w.society().state();
        for (const auto* id : {"linden", "rook", "maple", "sorrel"})
            st.residents[id].homeCell.clear();
        expect(w.society().restore(st), "homeless");
    }
    int seen = 0, wintry = 0;
    for (int d = 0; d < 400 && wintry < 20; ++d)
    {
        w.advanceCalendar(1);
        w.setTimeOfDay(23);
        w.tick(1);
        w.tick(1);
        if (calendar::calendarAt(w.calendarDays()).season != calendar::Season::Winter)
            continue;
        ++wintry;
        std::set<std::string> tiles;
        apartAll(w.roughSleepers(), "rough sleepers", tiles);
        seen = std::max(seen, int(w.roughSleepers().size()));
    }
    expect(seen >= 2, "nights with two or more by the walls: " + std::to_string(seen));
    fs::remove_all(root);
}

void theWatchOffTheStreet()
{
    // 10.2: one of the Watch with no post (Gorse, a guard written with none; Heath, written on patrol with no work spot,
    // her bunk at the barracks, a communal home, as DEV's Watch House) never takes or queues for the dole and never sleeps
    // by a warm wall, even with the DM's override making it destitute and homeless.
    const auto root = greyfen("watch", true);
    auto w = town(root);
    auto& s = w.society();
    {
        auto st = s.state();
        st.residents["heath"].homeCell = "barracks";
        st.residents["linden"].homeCell.clear();    // (Someone to sleep out and be given the dole.)
        expect(s.restore(st), "Heath bunks at the barracks");
    }
    destitute(w, "linden");
    s.setProsperity(-100, true);
    w.advanceCalendar(1);
    w.setTimeOfDay(9);
    for (int i = 0; i < 10; ++i)
        w.tick(1);
    if (std::getenv("STREET_DEBUG"))
        for (const auto& [id, life] : s.state().residents)
            std::cerr << id << " home " << life.homeCell << " status " << (s.statusOf(id) ? s.statusOf(id)->household : "-") << "\n";
    for (const auto* id : {"gorse", "heath"})
    {
        const auto* st = s.statusOf(id);
        expect(st && st->band >= status::Comfortable, std::string("of the Watch: comfortable or better at -100: ") + id + " " +
                                                          (st ? st->household + " " + std::to_string(st->band) + (st->overridden ? " overridden" : "") : ""));
    }
    for (const auto* id : {"gorse", "heath"})
        expect(s.overrideBand(s.householdOf(id), status::Destitute, 100000), std::string("the DM's override: ") + id);
    {
        auto st = s.state();
        st.residents["gorse"].homeCell.clear();
        expect(s.restore(st), "Gorse homeless");
    }
    w.addPlayer("player-ada", "Ada");
    onTo(w, calendar::Restday, Society::ServiceEnd - .05);
    Spot steps;
    for (const auto& [id, d] : w.doors())
        if (d.cellId == "longhouse" && w.cell(d.targetCell) && w.cell(d.targetCell)->outdoors)
            steps = {d.targetCell, d.arrival.x, d.arrival.y};
    bool queued = false;
    for (int i = 0; i < 9000; ++i)
    {
        auto* me = w.entity("player-ada");
        me->cellId = steps.cell;
        me->position = {steps.x - 2, steps.y + 2};
        me->velocity = {};
        w.tick(1);
        for (const auto& [id, t] : w.doleTakers())
        {
            queued = true;
            expect(!s.spec(id)->ofTheWatch() && !s.watchHousehold(t.household), "one of the Watch in the dole's queue: " + id);
        }
    }
    expect(queued && s.onDole("linden"), "the dole given, by a queue");
    expect(!s.onDole("gorse") && !s.onDole("heath"), "never the Watch's");
    int nights = 0;
    for (int d = 0; d < 400 && nights < 20; ++d)
    {
        w.advanceCalendar(1);
        w.setTimeOfDay(23);
        w.tick(1);
        w.tick(1);
        if (calendar::calendarAt(w.calendarDays()).season != calendar::Season::Winter)
            continue;
        ++nights;
        expect(!w.roughSleepers().count("gorse") && !w.roughSleepers().count("heath"), "one of the Watch by a warm wall");
    }
    expect(nights >= 20, "winter nights seen");
    fs::remove_all(root);
}

void runnersToTheNearest()
{
    // 6.6: with its own food shop shut, a household's runner goes to the open shop of its market with its food on the
    // shelf nearest the household's door (World::runnerShop): Cole's barrow at the lane's near end, never Dulse's at its
    // far one (once picked by a hash of the household and the shop, anywhere in the market).
    const auto root = greyfen("nearest", true);
    {
        auto text = slurp(root / "world.ratw");
        const auto move = [&](const std::string& from, const std::string& to) {
            const auto at = text.find(from);
            expect(at != std::string::npos, "the stall found: " + from);
            text.replace(at, from.size(), to);
        };
        move("\"lane\" 20.5 17.5 \"lane\" 22.5 20.5", "\"lane\" 10.5 2.5 \"lane\" 12.5 3.5");
        move("\"lane\" 40.5 17.5 \"lane\" 42.5 20.5", "\"lane\" 60.5 36.5 \"lane\" 61.5 37.5");
        std::ofstream(root / "world.ratw", std::ios::binary) << text;
    }
    auto w = town(root);
    auto& s = w.society();
    w.addPlayer("player-ada", "Ada");
    w.advanceCalendar(1);
    w.setTimeOfDay(8.2);
    for (int i = 0; i < 30; ++i)
        w.tick(1);
    const auto wrenTill = s.tillOf("wren");
    const auto coleTill = s.tillOf("cole"), dulseTill = s.tillOf("dulse");
    expect(wrenTill != "wren" && coleTill != "cole" && dulseTill != "dulse", "three shops with tills");
    for (const auto& till : {coleTill, dulseTill})
        for (const auto* food : {"bread", "fish", "meal"})
            expect(s.create(till, food, 40, "test: a barrow of food"), "food on the barrows");
    {
        auto st = s.state();                        // (Wren's shop shut: nobody keeping it.)
        for (auto& [pid, p] : st.careers.positions)
            if (p.holder == "wren")
                p.holder.clear();
        expect(s.restore(st), "Wren's shop shut");
    }
    for (int i = 0; i < 5; ++i)
        w.tick(1);
    expect(s.resident("cole")->task == "trade" && s.resident("dulse")->task == "trade", "the barrows kept");
    // Every household's runner, sent for Wren's food with her shop shut: to the nearest barrow with it.
    std::set<std::string> households;
    for (const auto& [id, life] : s.state().residents)
        if (const auto* st = s.statusOf(id))
            households.insert(st->household);
    int sent = 0;
    for (const auto& h : households)
    {
        const auto* to = w.runnerShop(h, wrenTill);
        expect(to && "till:" + to->id == coleTill, "a runner to the nearest open shop with its food, not " +
                                                       (to ? "till:" + to->id : std::string("none")) + ": " + h);
        ++sent;
    }
    expect(sent >= 8, "households' runners: " + std::to_string(sent));
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
    auto busy = parse(R"({"rows": {"poor": {"shops": 3}, "comfortable": {"shops": 3}, "rich": {"shops": 3}}})");
    s.setStatusOverride(&busy);                     // (A busy day: wares wanted, as well as the food; with nobody free to go
                                                    // in Greyfen's working hours, they wait for the player to leave.)
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
    // (A file world's save and all it keeps beside it: the journal, the save's temporary and backup, its lock, and the
    // folders of its art and reports.)
    for (const auto* end : {"", ".journal", ".tmp", ".bak", ".lock", ".art", ".reports"})
        fs::remove_all(path + end);
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

void helpWhenTheCapIsFull()
{
    // 6.7: with its market's lifts full (half its destitute share, never fewer than one), a player's help for a short
    // household isn't taken (no coin moves, no fame, no "food enough for a fortnight"), and a gift, which is still given,
    // lifts nobody and solves no trouble.
    const auto root = greyfen("full");
    game::Game g(optionsOn(root, {}));
    std::string problem;
    expect(g.start(problem), "a game starts on Greyfen: " + problem);
    auto& w = g.world();
    auto& s = w.society();
    for (int i = 0; i < 30; ++i)
        w.tick(1);
    destitute(w, "linden");
    Client ash;
    enter(g, ash, 1, "ash");
    expect(s.liftHousehold(s.householdOf("wren"), std::int64_t(std::floor(w.calendarDays())) + 30), "another household of the market lifted");
    expect(!s.liftRoom("linden"), "the market's lifts full");
    g.forgetTroubles();
    const auto t = g.troubleNow("linden");
    expect(t.kind == "short", "Linden's household is short: " + t.kind);
    g.hearTroubleFor(ash.entityId, "linden");
    beside(g, ash, "linden");
    const auto cash = s.account(ash.entityId)->cash;
    const auto deeds = [&] {
        int n = 0;
        for (const auto* d : g.deeds().byDoer(ash.entityId))
            n += d->kind == "fed_household";
        return n;
    };
    ash.events.clear();
    g.command(&ash, cmd({{"type", "action"}, {"action", "trouble:help:linden"}, {"target", "linden"}}));
    g.settle();
    for (int i = 0; i < 4; ++i)
        g.tick(.25);
    expect(s.account(ash.entityId)->cash == cash && !s.liftedNow("linden") && deeds() == 0 &&
               ash.said().find("living on help already") != std::string::npos,
           "the help not taken, and why: " + ash.said());
    // A gift of the trouble's coins: given, but no lift and no trouble solved.
    beside(g, ash, "linden");
    g.command(&ash, cmd({{"type", "give"}, {"target", "linden"}, {"item", ""}, {"quantity", 0}, {"coins", double(t.coins)}}));
    g.settle();
    for (int i = 0; i < 4; ++i)
        g.tick(.25);
    expect(s.account(ash.entityId)->cash == cash - t.coins && !s.liftedNow("linden") && deeds() == 0,
           "a gift given, nobody lifted, no fame for it: " + ash.said());
    expect(s.realBalanced(), "real money balanced");
    fs::remove_all(root);
}

std::vector<StreetCrash> streetRuns()
{
    std::vector<StreetCrash> runs;
    // The Restday dole given on a tick (no player near the steps: given at once). Its bread is a raw add to staged wallets,
    // not journalled, and so is the dole's mark: after the crash the household is as at the checkpoint, and, the dole's
    // hours still running, the priest gives it its bread once (never twice, never on_dole with no bread).
    runs.push_back({"dole",
                    [](game::Game& g, Client& c) {
                        auto& w = g.world();
                        auto* me = w.entity(c.entityId);
                        me->cellId = "barracks";            // (Out of sight of the steps.)
                        me->position = {5.5, 5.5};
                        onTo(w, calendar::Restday, Society::ServiceEnd + .2);
                        expect(w.society().onDole("linden"), "dole: given");
                        aPenny(g, c);
                    },
                    [](game::Game& g, const json::Value& was) {
                        auto& w = g.world();
                        const auto bread = Society::stock(*w.society().account("linden"), "bread");
                        for (int i = 0; i < 40; ++i)
                            w.tick(1);
                        expect(w.society().onDole("linden"), "dole: on the dole again after the crash, within its hours");
                        const auto given = Society::stock(*w.society().account("linden"), "bread") - bread;
                        expect(given > 0 && given == int(was["doleBread"].asNumber(-1)),
                               "dole: its bread given once more, as much as before the crash: " + std::to_string(given));
                        for (int i = 0; i < 40; ++i)
                            w.tick(1);
                        expect(Society::stock(*w.society().account("linden"), "bread") <= bread + given, "dole: no second dole that Restday");
                    }});
    // The dole given, its hours over, then the crash before the next checkpoint: the bread and the mark are lost with it,
    // and on the restart (still that Restday, past the dole's hours) the household is given it then, once.
    runs.push_back({"dolelate",
                    [](game::Game& g, Client& c) {
                        auto& w = g.world();
                        auto* me = w.entity(c.entityId);
                        me->cellId = "barracks";
                        me->position = {5.5, 5.5};
                        onTo(w, calendar::Restday, Society::ServiceEnd + .2);
                        expect(w.society().onDole("linden"), "dolelate: given");
                        w.setTimeOfDay(Society::ServiceEnd + 2.5);
                        for (int i = 0; i < 30; ++i)
                            w.tick(1);
                        aPenny(g, c);
                    },
                    [](game::Game& g, const json::Value&) {
                        auto& w = g.world();
                        const double hod = (w.calendarDays() - std::floor(w.calendarDays())) * 24;
                        expect(calendar::weekdayOf(w.calendarDays()) == calendar::Restday && hod >= Society::ServiceEnd + 2,
                               "dolelate: restarted that Restday, past the dole's hours");
                        expect(!w.society().onDole("linden"), "dolelate: the crash took the dole");
                        const auto bread = Society::stock(*w.society().account("linden"), "bread");
                        for (int i = 0; i < 40; ++i)
                            w.tick(1);
                        expect(w.society().onDole("linden"), "dolelate: given the dole again that Restday");
                        const auto given = Society::stock(*w.society().account("linden"), "bread") - bread;
                        expect(given > 0, "dolelate: its bread: " + std::to_string(given));
                        for (int i = 0; i < 40; ++i)
                            w.tick(1);
                        expect(Society::stock(*w.society().account("linden"), "bread") <= bread + given, "dolelate: no second dole");
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
    was.add("atCheckpoint", streetAsItStands(w));
    const auto bread = Society::stock(*w.society().account("linden"), "bread");
    r.step(g, ash);
    was.add("atCrash", streetAsItStands(w));
    was.add("doleBread", double(Society::stock(*w.society().account("linden"), "bread") - bread));
    return was;                                     // (The game gone without saving.)
}
void afterTheStreetCrash(const StreetCrash& r, const fs::path& root, const std::string& path, const json::Value& was)
{
    game::Game g(optionsOn(root, path));
    std::string problem;
    expect(g.start(problem), r.name + ": the crash's save starts: " + problem);
    const auto now = streetAsItStands(g.world());
    // What is journalled stands as at the crash; the dole and fed fame, as at the checkpoint (with their bread and deeds).
    for (const char* key : {"business", "lifts", "shelf"})
        expect(now.string(key) == was["atCrash"].string(key),
               r.name + ": the " + key + " of the crash after the restart:\n  " + now.string(key) + "\n  for\n  " + was["atCrash"].string(key));
    for (const char* key : {"dole", "fed"})
        expect(now.string(key) == was["atCheckpoint"].string(key),
               r.name + ": the " + key + " of the checkpoint after the restart:\n  " + now.string(key) + "\n  for\n  " +
                   was["atCheckpoint"].string(key));
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
    // Nothing of these crashes left in the temporary folder.
    const auto mine = "-" + std::to_string(::getpid()) + ".json";
    for (const auto& entry : fs::directory_iterator(fs::temp_directory_path()))
    {
        const auto name = entry.path().filename().string();
        expect(!(name.rfind("ratw-street-crash-", 0) == 0 && name.find(mine) != std::string::npos), "a crash's file left behind: " + name);
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
    b.day = 12, b.sold = 7, b.players = 2, b.soldBefore = {3, 4}, b.playersBefore = {1, 0}, b.outEarly = true, b.failing = 2, b.judged = 5,
    b.word = "failing";
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
    moved.fed.clear();
    expect(wire::stagedDiff(st, moved).items().size() == 1,
           "a business moved: one change (the dole and fed fame go with the checkpoint, as their bread and their deed do)");
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
            {"buyersAtAWatchedShop", buyersAtAWatchedShop},
            {"businessInOrdinaryTimes", businessInOrdinaryTimes},
            {"wareBuyingByBand", wareBuyingByBand},
            {"businessRecordNeverStepsBack", businessRecordNeverStepsBack},
            {"failingOnceARun", failingOnceARun},
            {"runnerGivingUpLate", runnerGivingUpLate},
            {"guardsNeverThieve", guardsNeverThieve},
            {"theWatchInTheBands", theWatchInTheBands},
            {"theDole", theDole},
            {"liftsAndTheirCap", liftsAndTheirCap},
            {"shortByBand", shortByBand},
            {"hungryThievesAtTheTill", hungryThievesAtTheTill},
            {"liftedThievesStop", liftedThievesStop},
            {"aFreshWorldsFirstDay", aFreshWorldsFirstDay},
            {"takesByBand", takesByBand},
            {"beggarsAndAlms", beggarsAndAlms},
            {"helpABeggar", helpABeggar},
            {"helpWhenTheCapIsFull", helpWhenTheCapIsFull},
            {"restdayDole", restdayDole},
            {"doleQueue", doleQueue},
            {"roughSleepers", roughSleepers},
            {"streetOnRaisedGround", streetOnRaisedGround},
            {"theWatchOffTheStreet", theWatchOffTheStreet},
            {"runnersToTheNearest", runnersToTheNearest},
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
        for (const auto& root : madeRoots())
            fs::remove_all(root);
        for (const auto* name : {"dole", "dolelate", "lift", "business", "sales"})
            cleanSave(crashSave(name));
        return 1;
    }
    for (const auto& root : madeRoots())
        fs::remove_all(root);
    std::cout << "street tests passed (" << checks << " checks)\n";
    return 0;
}
