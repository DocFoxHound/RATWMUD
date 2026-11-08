// NPC economic status, the bands (Docs/Design/79-a-staged-economy.md, sections 6 and 16.1; Phase 2): the slider's shares
// on worlds of any size, the realm's minimum, no destitute household without a reason, the guarantees in their order,
// the band near its home, Never Broke and Never Hungry, at most one band a week, the slider smoothed, the same bands on 1
// and 4 threads and across a save, the fortune's noise bounded, status.json read leniently, every account sorted.
// Seeded and offline; the catalogue is the real Data/Items (RATW_SOURCE_DIR).
#include "RatwItems.h"
#include "RatwJsonDoc.h"
#include "RatwSociety.h"
#include "RatwStatus.h"
#include "RatwWire.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

using namespace ratw;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

// N households in one market, each with a reason (so any may be destitute), near a comfortable home, scored by a lot.
std::vector<status::Candidate> pool(int n, const std::string& seed = "street")
{
    std::vector<status::Candidate> out;
    for (int i = 0; i < n; ++i)
    {
        status::Candidate c;
        c.key = seed + std::to_string(i);
        c.score = status::lot(c.key) * 4 - 2;
        c.reason = true;
        out.push_back(c);
    }
    return out;
}
std::array<int, status::Bands> counted(const std::vector<int>& bands)
{
    std::array<int, status::Bands> n{};
    for (const int b : bands)
        ++n[std::size_t(b)];
    return n;
}

void theTable()
{
    const auto& t = status::defaults();
    const auto built = status::builtIn();
    expect(t.shares.size() == built.shares.size(), "status.json's share rows read");
    for (std::size_t i = 0; i < t.shares.size(); ++i)
        expect(t.shares[i].at == built.shares[i].at && t.shares[i].shares == built.shares[i].shares,
               "status.json's shares are doc 79's (6.4)");
    expect(t.rows[status::Rich].purseHigh == 300 && t.rows[status::Destitute].meals == 1 && t.cap("welcomeDays") == 7,
           "status.json's rows and caps read");
    expect(status::sharesValid(t.shares), "the default table passes its own checks");
    for (int b = 0; b < status::Bands; ++b)
        expect(status::bandOf(status::bandName(b)) == b, "the band words round-trip");
}

void sharesOnAnyWorld()
{
    const auto& t = status::defaults();
    for (const int n : {1, 4, 9, 14, 120, 800})
        for (const double slider : {-100., -50., 0., 50., 100.})
        {
            status::CutOptions o;
            o.slider = slider;
            const auto bands = status::assign(t, pool(n), o);
            const auto c = counted(bands);
            const auto want = status::sharesAt(t, slider);
            for (int b = 0; b < status::Bands; ++b)
                expect(std::abs(c[std::size_t(b)] - want[std::size_t(b)] * n / 100.) <= 1 + 1e-9,
                       std::to_string(n) + " households at " + std::to_string(int(slider)) + ": " + status::bandName(b) + " " +
                           std::to_string(c[std::size_t(b)]) + " for " + std::to_string(want[std::size_t(b)] * n / 100.));
            if (n >= 10)
                expect(c[status::Destitute] >= 1 && c[status::Rich] >= 1,
                       "the realm's minimum: a destitute and a rich household in " + std::to_string(n));
        }
    // A 9-household hamlet at +100 isn't forced to keep a beggar.
    status::CutOptions boom;
    boom.slider = 100;
    expect(counted(status::assign(t, pool(9), boom))[status::Destitute] == 0, "no beggar forced on nine households in a boom");
    // No destitute household without a reason: its place goes to poor.
    auto some = pool(200, "reasonless");
    for (std::size_t i = 0; i < some.size(); i += 2)
        some[i].reason = false;
    status::CutOptions hard;
    hard.slider = -100;
    const auto bands = status::assign(t, some, hard);
    for (std::size_t i = 0; i < some.size(); ++i)
        expect(some[i].reason || bands[i] != status::Destitute, "no destitute household without a reason: " + some[i].key);
    expect(counted(bands)[status::Destitute] > 0, "those with a reason still destitute in hard times");
    // Never Broke empties destitute.
    hard.neverBroke = true;
    expect(counted(status::assign(t, pool(200), hard))[status::Destitute] == 0, "Never Broke: nobody destitute");
}

void monotoneInTheSlider()
{
    const auto& t = status::defaults();
    auto before = status::sharesAt(t, -100);
    for (int s = -95; s <= 100; s += 5)
    {
        const auto now = status::sharesAt(t, s);
        expect(now[status::Destitute] <= before[status::Destitute] + 1e-9 && now[status::Rich] >= before[status::Rich] - 1e-9,
               "destitute never rises and rich never falls as the slider rises: " + std::to_string(s));
        for (int b = 0; b < status::Bands; ++b)
            expect(status::shortChance(t, b, s) <= status::shortChance(t, b, s - 5) + 1e-12, "short days never more as the slider rises");
        before = now;
    }
    expect(status::shortChance(t, status::Destitute, -100) > status::shortChance(t, status::Destitute, 0) &&
               status::shortChance(t, status::Destitute, 100) < status::shortChance(t, status::Destitute, 0),
           "short days x1.5 at -100 and x0.5 at +100");
    for (int b = 1; b < status::Bands; ++b)
        expect(t.rows[std::size_t(b)].theft <= t.rows[std::size_t(b - 1)].theft &&
                   t.rows[std::size_t(b)].shortChance <= t.rows[std::size_t(b - 1)].shortChance,
               "theft odds and short days fall band by band");
    // More hard times, more destitute households in the cut.
    int last = 1 << 30;
    for (const double s : {-100., -50., 0., 50., 100.})
    {
        status::CutOptions o;
        o.slider = s;
        const int d = counted(status::assign(t, pool(800), o))[status::Destitute];
        expect(d <= last, "fewer destitute as the slider rises: " + std::to_string(d));
        last = d;
    }
}

void nearItsHomeAndTheWeek()
{
    const auto& t = status::defaults();
    // A poor home's band never two above it without an event; with one it may be.
    auto list = pool(100, "rows");
    for (auto& c : list)
        c.implied = status::Poor, c.reason = false;
    status::CutOptions boom;
    boom.slider = 100;
    for (const int b : status::assign(t, list, boom))
        expect(b <= status::Comfortable && b >= status::Destitute + 1, "a band at most one from its home, without an event");
    for (auto& c : list)
        c.recent = true;
    expect(counted(status::assign(t, list, boom))[status::Rich] > 0, "an event lets a band stray further");
    // At most one band a week, and only past an edge; "now" skips it.
    auto week = pool(50, "week");
    auto& top = week[0];
    top.score = 99;                                 // (The best in its market: the cut makes it rich.)
    top.previous = status::Poor;
    top.since = 0;
    status::CutOptions o;
    o.day = 3;
    expect(status::assign(t, week, o)[0] == status::Poor, "no change inside a week");
    o.day = 7;
    expect(status::assign(t, week, o)[0] == status::Comfortable, "a week on: one band, not two");
    o.now = true;
    expect(status::assign(t, week, o)[0] == status::Rich, "the DM's now: straight to the cut's band");
    // At restore each keeps its saved band.
    status::CutOptions restore;
    restore.keepPrevious = true;
    expect(status::assign(t, week, restore)[0] == status::Poor, "at restore: the saved band");
    // The realm's minimum keeps the hysteresis too: never two bands at once, never inside a week, never at restore.
    auto street = pool(20, "least");
    for (auto& c : street)
        c.previous = status::Comfortable, c.since = 5;
    status::CutOptions dayOn;
    dayOn.slider = 100;
    dayOn.day = 7;
    for (const int b : status::assign(t, street, dayOn))
        expect(b == status::Comfortable, "the minimum moves nobody comfortable to destitute, nor inside its week");
    for (auto& c : street)
        c.previous = status::Poor, c.since = 0;
    const auto moved = status::assign(t, street, dayOn);
    expect(counted(moved)[status::Destitute] == 1, "past the week, the minimum takes one poor household down one band");
    dayOn.keepPrevious = true;
    expect(counted(status::assign(t, street, dayOn))[status::Destitute] == 0, "at restore the minimum moves nobody");
}

void theFortuneIsBounded()
{
    const auto& t = status::defaults();
    for (const char* key : {"rm_greatly_house_up", "sump_rents", "|homeless"})
    {
        const double stable = t.lot * (2 * status::lot(std::string(key) + "|fortune") - 1);
        expect(std::abs(stable) <= t.lot + 1e-9, "the family's lot within its bound");
        for (std::int64_t day = 0; day < 500 * 7; ++day)
        {
            const double noise = status::fortune(t, key, day) - stable;
            expect(std::abs(noise) <= t.noise + 1e-9, "the fortune's noise within 0.3 over 500 weeks: " + std::to_string(noise));
            if (day > 0)
                expect(std::abs(status::fortune(t, key, day) - status::fortune(t, key, day - 1)) <= 2 * t.noise / 7 + 1e-9,
                       "eased between weeks");
        }
    }
}

void statusJsonReadLeniently()
{
    const auto& base = status::defaults();
    const auto parse = [](const std::string& text) {
        json::Value v;
        std::string error;
        if (!json::parse(text, v, error))
            throw std::runtime_error("bad test json: " + error);
        return v;
    };
    std::vector<std::string> warnings;
    // A row summing to 140 is normalised (6.4), and the table it makes passes.
    auto t = status::read(parse(R"({"shares":[{"at":-100,"shares":[25.2,63,44.8,7]},{"at":0,"shares":[7,35,84,14]},{"at":100,"shares":[2.8,16.8,98,22.4]}]})"),
                          base, &warnings);
    expect(status::sharesValid(t.shares) && std::abs(t.shares[1].shares[status::Comfortable] - 60) < 1e-9 && !warnings.empty(),
           "a table summing to 140 normalised, with a warning");
    // Not monotone, a share under 2%, the wrong ends, or nonsense: the default whole.
    for (const char* bad : {R"({"shares":[{"at":-100,"shares":[5,25,60,10]},{"at":100,"shares":[18,45,32,5]}]})",
                            R"({"shares":[{"at":-100,"shares":[1,45,49,5]},{"at":100,"shares":[1,12,70,17]}]})",
                            R"({"shares":[{"at":-50,"shares":[18,45,32,5]},{"at":100,"shares":[2,12,70,16]}]})",
                            R"({"shares":"abc"})", R"({"shares":[{"at":"x"}]})"})
    {
        warnings.clear();
        t = status::read(parse(bad), base, &warnings);
        expect(t.shares.size() == base.shares.size() && t.shares[0].shares == base.shares[0].shares && !warnings.empty(),
               std::string("a bad share table falls back to the default: ") + bad);
    }
    // Clamps: a purse of 10^9, a meal count of -5, an unknown field; what's missing is the base's.
    t = status::read(parse(R"({"rows":{"rich":{"purse":[60,1000000000]},"poor":{"meals":-5}},"nonsense":1,"leanDays":"x"})"), base);
    expect(t.rows[status::Rich].purseHigh == 500 && t.rows[status::Poor].meals == 0 && t.leanDays == base.leanDays &&
               t.rows[status::Comfortable].purseHigh == base.rows[status::Comfortable].purseHigh,
           "clamped: a purse at most 500p, meals at least 0; the rest the defaults'");
    expect(status::read(parse("[1,2]"), base).shares[1].shares == base.shares[1].shares, "not an object: the defaults");
}

void everyAccountSorted()
{
    using K = Society::AccountKind;
    const std::vector<std::pair<std::string, K>> table{
        {"wolf-0123456789abcdef0123456789abcdef", K::Real}, {"player-ada", K::Real}, {"chapter:c1", K::Real},
        {"contract:k1", K::Escrow},  {"letter:l1@x", K::Escrow}, {"let:l1", K::Escrow},     {"stall:s1", K::Escrow},
        {"table:t1", K::Escrow},     {"fest:x:1:race", K::Escrow}, {"project:p1", K::Escrow}, {"ground:g1", K::Escrow},
        {"bandits:camp1", K::Escrow}, {"treasury", K::Staged},   {"stores:coldwater", K::Staged}, {"till:job:x", K::Staged},
        {"town:works", K::Staged},   {"home:h1", K::Staged},     {"ivar", K::Staged},       {"house:fell", K::Dead},
        {"caravan:c1", K::Dead},     {"fund:relief", K::Dead},   {"bank:b", K::Dead},       {"outside:wharf", K::Dead},
        {"farm:f1", K::Dead},        {"crew:f1", K::Dead},       {"estate:fell", K::Dead}};
    for (const auto& [id, kind] : table)
        expect(Society::classify(id) == kind, "classified as doc 79's 9.1: " + id);
    // Every prefix facilityAccount knows (read from it: facilityPrefixes) is in the table, and classify sorts it so.
    expect(facilityPrefixes().size() >= 22, "facilityAccount's prefixes");
    for (const auto& prefix : facilityPrefixes())
    {
        expect(facilityAccount(prefix + "x"), "a facility prefix: " + prefix);
        bool listed = false;
        for (const auto& [id, kind] : table)
            if (id.rfind(prefix, 0) == 0)
                listed = true, expect(Society::classify(prefix + "x") == kind, "classify sorts the prefix as 9.1 does: " + prefix);
        expect(listed, "every facility prefix sorted in doc 79's 9.1 table: " + prefix);
    }
}

// A street of households, a House's head living in the Sump, a Heights family and a sleeper in the street.
ResidentSpec person(const std::string& id, const std::string& work, Spot home, int age = 35)
{
    ResidentSpec r;
    r.id = id;
    r.name = id;
    r.role = "civilian";
    r.workLabel = work;
    r.age = age;
    r.appearance.sex = "male";
    r.home = home;
    r.work = {"yard", 2.5, 2.5};
    r.evening = home;
    r.serve = r.work;
    r.purse = 30;
    return r;
}
AuthoredRoster street(int households, bool staged = true)
{
    AuthoredRoster roster;
    for (int i = 0; i < households; ++i)
        roster.residents.push_back(person("w" + std::to_string(i), i % 5 == 0 ? "at lessons" : "unloading ships at the piers",
                                          {"street_" + std::to_string(i), 2.5, 2.5}));
    roster.residents.push_back(person("fell", "ruling House Fell", {"sump_rents_9", 2.5, 2.5}, 50));
    roster.residents.push_back(person("sumpman", "unloading ships at the piers", {"sump_rents_10", 2.5, 2.5}));
    roster.residents.push_back(person("heightsman", "at lessons", {"vance_grounds", 2.5, 2.5}));
    roster.residents.push_back(person("sleeper", "unloading ships at the piers", {}));
    if (staged)
        roster.economy.rules["staged"] = 1;
    return roster;
}
struct World
{
    CareerWorld world(const Society& s)
    {
        CareerWorld w;
        w.alive = [](const std::string&) { return true; };
        w.age = [&s](const std::string& id) { const auto* r = s.spec(id); return r ? r->age : 30; };
        w.regard = [](const std::string&, const std::string&) { return 0.0; };
        return w;
    }
};

void theGuaranteesInOrder()
{
    Society s(Roster::None);
    s.configure(street(30));
    World w;
    s.tendCareers(1.3, w.world(s));
    s.setQuarters({{"fell", "sump"}, {"sumpman", "sump"}, {"heightsman", "heights"}});
    s.setProsperity(-100);
    s.stagedBands(2, true);
    expect(s.statusOf("fell") && s.statusOf("fell")->band == status::Rich, "a House's head in the Sump is rich (3 before 4)");
    expect(s.statusOf("sumpman")->band <= status::Poor, "the Sump is at most poor");
    expect(s.statusOf("heightsman")->band >= status::Comfortable, "the Heights at least comfortable, in hard times too");
    expect(s.statusOf("sleeper")->band == status::Destitute && s.statusOf("sleeper")->household == "|sleeper",
           "the homeless destitute, a household of one");
    // A DM's override first of all, then cleared.
    s.overrideBand(s.householdOf("fell"), status::Poor, 30);
    expect(s.statusOf("fell")->band == status::Poor && s.statusOf("fell")->overridden, "a DM's override before a House");
    s.overrideBand(s.householdOf("fell"), -1, 0);
    expect(s.statusOf("fell")->band == status::Rich, "the override cleared");
    // The Sump's label removed by a host: no error, the Sump household just ranks.
    s.setQuarters({});
    s.stagedBands(3, true);
    expect(s.statusOf("sumpman") != nullptr && s.statusOf("heightsman") != nullptr, "with the quarters gone, still banded");
    // Every member of a household shares its band; no destitute household without a reason.
    for (const auto& [id, life] : s.state().residents)
    {
        const auto* st = s.statusOf(id);
        expect(st != nullptr, id + " banded");
        expect(st->band != status::Destitute || st->reason, id + ": destitute only with a reason");
    }
    // Off, nothing is banded. (Not where RATW_STAGED_ECONOMY forces it on.)
    if (Society::stagedForced())
        return;
    Society off(Roster::None);
    off.configure(street(5, false));
    off.tendCareers(1.3, w.world(off));
    expect(off.statusOf("w1") == nullptr, "the staged economy off: no bands");
}

void theSliderSmoothed()
{
    Society s(Roster::None);
    s.configure(street(20));
    World w;
    s.tendCareers(1.3, w.world(s));
    s.stagedBands(1, true);
    expect(s.prosperity() == 0, "ordinary times to begin");
    s.setProsperity(100);
    for (int day = 2; day <= 6; ++day)
    {
        s.stagedBands(day);
        expect(std::abs(s.prosperity() - 10. * (day - 1)) < 1e-9, "the slider moves 10 a day: " + std::to_string(s.prosperity()));
    }
    s.stagedBands(7, true);
    expect(s.prosperity() == 100, "the DM's now: at once");
    s.setProsperity(-9999);
    expect(s.prosperitySetting() == -100, "a slider of -9999 clamped");
}

std::map<int, int> census(const Society& s)
{
    std::map<int, int> n;
    for (const auto& [id, life] : s.state().residents)
        if (const auto* st = s.statusOf(id))
            ++n[st->band];
    return n;
}

void threadsAndSaves()
{
    // The same bands on one thread and four: the day's pass is single-threaded; the deciding only reads it.
    const auto run = [](bool threaded) {
        Society s(Roster::None);
        s.configure(street(40));
        if (threaded)
            s.setParallel([](std::size_t count, const std::function<void(std::size_t)>& job) {
                std::vector<std::thread> ts;
                for (std::size_t k = 0; k < 4; ++k)
                    ts.emplace_back([&, k] {
                        for (std::size_t i = k; i < count; i += 4)
                            job(i);
                    });
                for (auto& t : ts)
                    t.join();
            });
        World w;
        s.tendCareers(1.3, w.world(s));
        s.setProsperity(-60);
        for (int h = 0; h <= 72; ++h)
            s.tick(1., 2 + h / 24. + 1e-4, 0, {});
        return s;
    };
    const auto one = run(false);
    const auto four = run(true);
    for (const auto& [id, life] : one.state().residents)
        expect(one.statusOf(id) && four.statusOf(id) && one.statusOf(id)->band == four.statusOf(id)->band &&
                   one.cash(id) == four.cash(id),
               "the same band and purse on 1 and 4 threads: " + id);
    // Saved and restored: the same bands, the staged hour kept; a restart makes nothing jump.
    const auto saved = wire::society(one.state());
    const auto text = json::dump(saved);
    json::Value back;
    std::string error;
    expect(json::parse(text, back, error), "the save parses");
    expect(back.has("staged"), "the save holds the staged state");
    const auto state = wire::readSociety(back);
    expect(state.minted >= 0, "the save reads back valid");
    expect(state.staged.bands.size() == one.state().staged.bands.size() && state.staged.hour == one.state().staged.hour &&
               state.staged.slider == one.state().staged.slider,
           "the staged state round-trips");
    Society again(Roster::None);
    again.configure(street(40));
    expect(again.restore(state), "restored");
    for (const auto& [id, life] : one.state().residents)
        expect(again.statusOf(id) && again.statusOf(id)->band == one.statusOf(id)->band, "no band jumps at a restart: " + id);
    expect(json::dump(wire::society(again.state())) == text, "a restore writes back the same save");
    // Off: no staged field at all (save_compat's purses and escrow untouched).
    if (Society::stagedForced())
        return;
    Society off(Roster::None);
    off.configure(street(5, false));
    expect(!wire::society(off.state()).has("staged"), "the staged economy off: nothing staged saved");
}

void neverHungryAndShortDays()
{
    // The destitute go short on their short days; with Never Hungry, never.
    const auto days = [](bool neverHungry) {
        auto roster = street(12);
        roster.economy.rules["neverHungry"] = neverHungry ? 1 : 0;
        Society s(Roster::None);
        s.configure(roster);
        World w;
        s.tendCareers(1.3, w.world(s));
        json::Value always;
        std::string error;
        json::parse(R"({"rows":{"destitute":{"short":100}}})", always, error);
        s.setStatusOverride(&always);
        int shortDays = 0;
        for (int day = 2; day <= 8; ++day)
        {
            const double before = [&] {
                double n = 0;
                for (const auto& [item, c] : s.account("sleeper")->stock)
                    n += double(Society::nourishment(item)) * c;
                return n;
            }();
            for (int h = 0; h < 24; ++h)
                s.tick(1., day + h / 24. + 1e-4, 0, {});
            double after = 0;
            for (const auto& [item, c] : s.account("sleeper")->stock)
                after += double(Society::nourishment(item)) * c;
            shortDays += after - before < Society::NourishADay - 1e-9 && before < Society::NourishADay ? 1 : 0;
            // (Nobody eats here: empty the wallet so the next day's provision shows.)
            auto copy = s.account("sleeper")->stock;
            for (const auto& [item, c] : copy)
                if (c > 0 && Society::nourishment(item) > 0)
                    s.consume("sleeper", item, c, "test: eaten");
        }
        return shortDays;
    };
    expect(days(false) >= 5, "the destitute short most days at a 100% chance");
    expect(days(true) == 0, "Never Hungry: no short days");
}
} // namespace

int main()
{
    try
    {
        if (!items::load())
            throw std::runtime_error("the catalogue didn't load");
        theTable();
        sharesOnAnyWorld();
        monotoneInTheSlider();
        nearItsHomeAndTheWeek();
        theFortuneIsBounded();
        statusJsonReadLeniently();
        everyAccountSorted();
        theGuaranteesInOrder();
        theSliderSmoothed();
        threadsAndSaves();
        neverHungryAndShortDays();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "Status tests passed: " << checks << " checks.\n";
    return 0;
}
