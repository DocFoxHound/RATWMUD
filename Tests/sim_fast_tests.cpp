// The fast economy run (Docs/Design/69-simulation-speed.md): on a small streamed town (an export, as world_check loads
// one), the harness's path makes the births and arrivals the society asks for, as the game server does for a world from
// files (World::makeRequestedResidents after each tick), and the old die of old age (World::tendOldAge) as everywhere;
// the same world run twice, on one thread and on four, with the profile's clocks on and off, ends the same; the unseen
// step and straight legs are knobs a harness turns (and turns back). Given world_check's path (the first argument, as
// ctest passes it), it also runs the harness itself with no player: an economy file a day, the same on 1 and 4 threads,
// births and deaths in it, and no player.
#include "RatwJsonDoc.h"
#include "RatwPool.h"
#include "RatwWorld.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <functional>
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
std::string slurp(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

// A town of two cells side by side (its homes, its market), "brook" by its territory. Two of 100 die of old age the
// first day: old Hob, retired, and old Tamsin, who keeps the smithy (a hand's post of the market's: nobody in the town is
// free, so after eight days Fen Saltdale, unplaced, a fisher of the Country, comes in for it; births follow deaths, so a
// child is owed for each). Wren and Birch are married and live together, of an age for children: the first child is
// theirs, the other the next couple's that can (a couple has one a year or so apart).
constexpr int Side = 16;
std::string cellId(int i) { return "b_" + std::to_string(i) + "_0"; }
std::string resident(const std::string& who, const std::string& name, const std::string& role, const std::string& work, int age,
                     const std::string& sex, int paid, double hx, double wx)
{
    std::ostringstream r;
    r << "resident \"" << who << "\" \"" << name << "\" \"" << role << "\" \"" << work << "\" \"Someone.\" \"Hello.\" " << age
      << " \"timber\" \"" << sex << "\" \"average\" \"saddle\" 3 1 5 1 " << paid << " 8 17 \"-\" 40 0 1 \"" << cellId(0) << "\" " << hx
      << " 4.5 \"" << cellId(1) << "\" " << wx << " 8.5 \"" << cellId(0) << "\" " << hx << " 5.5\n";
    return r.str();
}
fs::path writeTown(const fs::path& root)
{
    fs::remove_all(root);
    fs::create_directories(root / "cells");
    fs::create_directories(root / "seams");
    std::ostringstream m, seamsA, seamsB;
    m << "RATW_WORLD 3\n";
    for (int i = 0; i < 2; ++i)
    {
        std::ofstream cell(root / "cells" / (cellId(i) + ".cell"));
        cell << "id: " << cellId(i) << "\nname: Brook " << i << "\ndescription: Open ground.\nworld: " << i * Side
             << " 0 0\noutdoors: true\nweather: clear\nsize: " << Side << ' ' << Side << "\ngrid:\n";
        for (int y = 0; y < Side; ++y)
            cell << std::string(Side, '.') << '\n';
        m << "area \"" << cellId(i) << "\"\n";
    }
    for (int k = 0; k < Side; ++k)
    {
        const std::string a = "seam_" + std::to_string(k) + "_a", b = "seam_" + std::to_string(k) + "_b";
        seamsA << "door \"" << a << "\" \"Open boundary\" \"" << cellId(0) << "\" " << Side - .5 << ' ' << k + .5 << " \"" << cellId(1)
               << "\" .5 " << k + .5 << " \"" << b << "\" 1 0 1 1 \"E\"\n";
        seamsB << "door \"" << b << "\" \"Open boundary\" \"" << cellId(1) << "\" .5 " << k + .5 << " \"" << cellId(0) << "\" "
               << Side - .5 << ' ' << k + .5 << " \"" << a << "\" 1 0 1 1 \"W\"\n";
    }
    std::ofstream(root / "seams" / cellId(0)) << seamsA.str();
    std::ofstream(root / "seams" / cellId(1)) << seamsB.str();
    m << "exits \"" << cellId(0) << "\" 1 \"" << cellId(1) << "\"\n";
    m << "exits \"" << cellId(1) << "\" 1 \"" << cellId(0) << "\"\n";
    for (int i = 0; i < 2; ++i)
        m << "territory \"" << cellId(i) << "\" \"brook\" \"-\" 0\n";
    m << "spawn \"" << cellId(0) << "\" 8.5 8.5\n";
    m << "economy 20000 100 50 10 12\n";
    m << resident("hob", "Hob Reed", "civilian", "-", 100, "male", 0, 2.5, 2.5);
    m << resident("tamsin", "Tamsin Reed", "civilian", "smithing", 100, "female", 1, 3.5, 4.5);
    m << resident("wren", "Wren Ash", "civilian", "weaving", 30, "female", 1, 5.5, 6.5);
    m << resident("birch", "Birch Ash", "civilian", "hauling", 32, "male", 1, 5.5, 7.5);
    m << resident("moss", "Moss Fern", "merchant", "keeping the stall", 40, "male", 1, 9.5, 9.5);
    m << resident("sedge", "Sedge Fern", "civilian", "farming", 35, "female", 1, 11.5, 11.5);
    // Unplaced (doc 66, 2): no home, no work, no evening place.
    m << "resident \"fen\" \"Fen Saltdale\" \"civilian\" \"fishes the lake\" \"A tall grey wolf who fishes the lake.\" \"Hello.\" 30 "
         "\"timber\" \"male\" \"average\" \"saddle\" 3 1 5 1 1 8 17 \"-\" 49 0 1 \"-\" 0 0 \"-\" 0 0 \"-\" 0 0\n";
    m << "kin \"wren\" \"birch\" spouse\n";
    std::ofstream(root / "world.ratw") << m.str();
    return root;
}

// The export loaded the way world_check (and the server) loads it: cells streamed from the files as wanted.
struct Export
{
    std::map<std::string, std::string> files, manifest;
    explicit Export(const fs::path& root)
    {
        for (const auto& entry : fs::recursive_directory_iterator(root))
            if (entry.is_regular_file())
                files[fs::relative(entry.path(), root).generic_string()] = slurp(entry.path());
        for (const auto& [path, text] : files)
            if (path.rfind("cells/", 0) != 0 && path.rfind("seams/", 0) != 0)
                manifest[path] = text;
    }
    void into(World& w, const fs::path& root)
    {
        w.setCellSource({[this](const std::string& id, std::string& header) {
                             const auto f = files.find("cells/" + id + ".cell");
                             if (f == files.end())
                                 return std::string("no cell file for ") + id;
                             header = f->second.substr(0, f->second.find("grid:") + 5);
                             return std::string();
                         },
                         [this](const std::string& id, std::string& cell, std::string& seams) {
                             const auto f = files.find("cells/" + id + ".cell");
                             if (f == files.end())
                                 return std::string("no cell file for ") + id;
                             cell = f->second;
                             const auto s = files.find("seams/" + id);
                             seams = s == files.end() ? std::string() : s->second;
                             return std::string();
                         }});
        const auto loaded = w.loadWorldFiles(manifest, root.string());
        expect(loaded.ok, "the town loads: " + loaded.message);
    }
};

// What a run ends with: everyone's state and every account, as world_check's digest mixes them.
std::string stateOf(const World& w)
{
    std::ostringstream out;
    for (const auto& [id, e] : w.entities())
        out << id << ' ' << e.cellId << ' ' << e.position.x << ',' << e.position.y << ' ' << e.activity << ' ' << e.dead << ' ' << e.age
            << '\n';
    for (const auto& [id, a] : w.society().state().accounts)
    {
        out << id << ' ' << a.cash;
        for (const auto& [item, n] : a.stock)
            out << ' ' << item << '=' << n;
        out << '\n';
    }
    for (const auto& [id, life] : w.society().state().residents)
        out << id << ' ' << life.hunger << ' ' << life.fatigue << ' ' << life.task << ' ' << life.goalCell << ' ' << life.goalX << ','
            << life.goalY << ' ' << life.homeCell << '\n';
    for (const auto& [id, post] : w.society().state().careers.positions)
        out << id << ' ' << post.holder << '\n';
    out << std::hex << w.legsDigest() << '\n';
    return out.str();
}

struct Tally
{
    int births = 0, arrivals = 0, deaths = 0, owed = 0;
    std::vector<std::string> made;                  // In the order they came: "birth wren" / "arrival fen".
};

// The harness's day loop, as world_check runs it with no player (each tick, then the residents asked for made), for
// `days` game days in half-second ticks.
Tally run(const fs::path& root, int days, unsigned threads, bool profile, std::string* state = nullptr)
{
    Export files(root);
    World w;
    Pool pool(threads > 1 ? threads - 1 : 0);
    if (threads > 1)
        w.setParallel([&](std::size_t count, const std::function<void(std::size_t)>& job) { pool.run(count, job); });
    files.into(w, root);
    w.setDeterministic(true);
    w.setProfiling(profile);
    w.setTimeOfDay(6);
    Tally t;
    std::set<std::string> dead;
    std::vector<ResidentRequest> made;
    const int ticks = days * 24 * 1200;             // (Half a second each.)
    for (int i = 0; i < ticks; ++i)
    {
        w.tick(.5);
        made.clear();
        w.makeRequestedResidents(&made);
        for (const auto& r : made)
        {
            (r.kind == "birth" ? t.births : t.arrivals) += 1;
            t.made.push_back(r.kind + " " + r.templateId);
        }
    }
    for (const auto& [id, e] : w.entities())
        t.deaths += e.npc && e.dead;
    if (state)
        *state = stateOf(w);
    // Everyone born is in the world and the society, alive, at home, of the right age and sex; the only others are the
    // world's own (the five written and Fen, come in from the Country): nobody else is ever made.
    for (const auto& [id, e] : w.entities())
    {
        if (id.rfind("born_", 0) != 0)
        {
            expect(std::set<std::string>{"hob", "tamsin", "wren", "birch", "moss", "sedge", "fen"}.count(id), id + " is one of the world's own");
            continue;
        }
        const auto* life = w.society().resident(id);
        const auto* spec = w.society().spec(id);
        expect(life && spec && e.npc, id + " is a resident");
        expect(e.age == 0 || e.age == 1, id + ", born here, is a child: " + std::to_string(e.age));
        expect(spec->appearance.sex == "male" || spec->appearance.sex == "female", id + " is male or female");
        expect(e.appearance->sex == spec->appearance.sex, id + "'s body is the sex its record says");
        expect(life->homeCell == cellId(0), id + " lives at its mother's home: " + life->homeCell);
    }
    if (const auto* fen = w.entity("fen"))
    {
        // Fen lodges with the household of his class (the homes are all in the first cell), never alone at the market; his
        // purse is the Country's, the money conserved.
        const auto* life = w.society().resident("fen");
        expect(life && life->homeCell == cellId(0) && !fen->dead, "Fen lives with a household of the town: " + (life ? life->homeCell : ""));
        expect(w.society().jobOf("fen") && w.society().jobOf("fen")->id == "job:tamsin", "and keeps the smithy");
        expect(!w.society().unplaced("fen"), "no longer unplaced");
    }
    expect(w.society().conserved(), "the money conserved");
    t.owed = int(w.society().state().careers.birthsOwed.size());
    return t;
}

void harnessPath(const fs::path& root)
{
    std::string one, four, unprofiled;
    const auto a = run(root, 120, 1, true, &one);
    expect(a.deaths >= 2, "the two of 100 die of old age: " + std::to_string(a.deaths));
    expect(a.arrivals == 1, "Fen comes in from the Country to keep the smithy: " + std::to_string(a.arrivals));
    // (Wren and Birch's first; the other to the next couple able, Sedge's: married in the town since.)
    expect(a.births >= 1 && a.births + a.owed == a.deaths, "births follow deaths, one for one: " + std::to_string(a.births) +
                                                              " born and " + std::to_string(a.owed) + " owed for " + std::to_string(a.deaths));
    bool fenCame = false, childOfWren = false;
    for (const auto& m : a.made)
    {
        fenCame = fenCame || m == "arrival fen";
        childOfWren = childOfWren || m == "birth wren";
    }
    expect(fenCame, "the arrival is Fen, of the world's own list");
    expect(childOfWren, "the child is Wren's");
    // Deterministic: the same world, on four threads, and with the profile's clocks off, ends exactly the same.
    const auto b = run(root, 120, 4, true, &four);
    expect(b.made == a.made && four == one, "the same run on four threads ends the same");
    const auto c = run(root, 120, 1, false, &unprofiled);
    expect(c.made == a.made && unprofiled == one, "and with the profile's clocks off");
}

void knobs()
{
    expect(Society::unseenStep() == Society::UnseenStep, "the game's unseen step is 5 s");
    Society::setUnseenStep(30);
    expect(Society::unseenStep() == 30, "a harness may lengthen it");
    Society::setUnseenStep(0);
    expect(Society::unseenStep() == 1, "never under a second");
    Society::setUnseenStep(Society::UnseenStep);
    World w;
    expect(w.profiling(), "the profile's clocks are on by default");
}

// world_check itself, with no player: an economy run's line a day, the same on one thread and four.
void theHarness(const std::string& binary, const fs::path& root)
{
    const auto out = fs::temp_directory_path() / ("ratw_sim_fast_" + std::to_string(::getpid()));
    fs::create_directories(out);
    const auto runIt = [&](const std::string& tag, const std::string& more) {
        const auto cmd = "\"" + binary + "\" \"" + root.string() + "\" --simulate 6 78 --no-check --deterministic --economy \"" +
                         (out / (tag + ".jsonl")).string() + "\" " + more + " > \"" + (out / (tag + ".out")).string() + "\" 2>&1";
        expect(std::system(cmd.c_str()) == 0, "world_check runs: " + slurp(out / (tag + ".out")));
        return std::make_pair(slurp(out / (tag + ".jsonl")), slurp(out / (tag + ".out")));
    };
    const auto [one, oneOut] = runIt("one", "--threads 1");
    const auto [four, fourOut] = runIt("four", "--threads 4 --no-profile");
    expect(oneOut.find("no player") != std::string::npos, "an economy run has no player: " + oneOut.substr(0, 400));
    expect(oneOut.find("run day 3") != std::string::npos, "and says what each day cost");
    int lines = 0;
    for (const char ch : one)
        lines += ch == '\n';
    expect(lines == 3, "a line a day: " + std::to_string(lines));
    expect(one.find("\"deaths\": 2") != std::string::npos, "the two of 100 die the first day: " + one.substr(0, 300));
    expect(one.find("\"births\": ") != std::string::npos && one.find("\"arrivals\": ") != std::string::npos &&
               one.find("\"births_owed\": ") != std::string::npos && one.find("\"unplaced\": 1") != std::string::npos,
           "births, arrivals, the children owed and the unplaced counted");
    expect(one.find("\"players\": 0") != std::string::npos, "and no player, of any kind");
    expect(one == four, "the economy file is the same on four threads, with the profile off");
    const auto [player, playerOut] = runIt("player", "--threads 1 --players 1");
    expect(playerOut.find("1 player") != std::string::npos, "--players 1 keeps a player for a city check");
    // The Country's food farms (doc 71, 4): switched on an area a day from day 0; each line is JSON, the farms' fields in
    // it, the founding counted as minted and the money conserved.
    const auto [farms, farmsOut] = runIt("farms", "--threads 1 --farms 0 --farms-every 1");
    expect(farmsOut.find("the farms' switch: the first area on day 0") != std::string::npos, "--farms: " + farmsOut.substr(0, 400));
    std::istringstream days(farms);
    int day = 0;
    for (std::string line; std::getline(days, line); ++day)
    {
        json::Value v;
        std::string error;
        expect(json::parse(line, v, error), "the farms' line is JSON: " + error);
        expect(v.number("farms_on", -1) == day + 1, "an area more each day: " + std::to_string(v.number("farms_on", -1)));
        expect(v["conserved"].asBool() && v.number("farms_minted") > 0 && v.number("minted") > 0, "the founding minted, conserved");
        expect(v.has("farm_grain_stock") && v.has("treasury_low") && v.has("support_unpaid_switch") && v["farms"].isObject() &&
                   v["farms"].has("grain_farm_1"),
               "the farms' and the treasury's fields");
    }
    expect(day == 3, "a line a day");
    // The economy's levels (doc 69, 10): off and full leave the economy file as it was; off writes no detail; full's
    // tables (gzipped JSON lines, names numbered) add up to the economy line's totals, day by day.
    const auto [off, offOut] = runIt("off", "--threads 1 --economy-level off --economy-detail \"" + (out / "off_detail.jsonl").string() + "\"");
    expect(off == one && !fs::exists(out / "off_detail.jsonl"), "off: the same economy file, no detail");
    const auto [full, fullOut] = runIt("full", "--threads 1 --farms 0 --farms-every 1 --economy-level full");
    expect(full == farms, "full: the same economy file as without it");
    expect(fullOut.find("economy level full: ") != std::string::npos, "full: says where its tables go");
    const auto unzipped = out / "full_tables.jsonl";
    const auto unzip = "gzip -dc \"" + (out / "full.jsonl.full.jsonl.gz").string() + "\" > \"" + unzipped.string() + "\"";
    expect(std::system(unzip.c_str()) == 0, "full: its tables are gzipped");
    std::istringstream tables(slurp(unzipped)), economy(full);
    std::vector<std::string> names;
    int tableDays = 0;
    bool metaSeen = false;
    for (std::string line; std::getline(tables, line);)
    {
        json::Value v;
        std::string error;
        expect(json::parse(line, v, error), "a table line is JSON: " + error);
        if (v.has("meta"))
        {
            metaSeen = v["meta"]["columns"].has("trade") && v["meta"]["farms"].has("grain_farm_1") &&
                       v["meta"]["columns"].has("work") && v["meta"]["columns"].has("hh_short");   // (Round 3, doc 73.)
            continue;
        }
        for (const auto& n : v.array("names"))
            names.push_back(n.asString());
        std::string econLine;
        expect(bool(std::getline(economy, econLine)), "a table line for each economy line");
        json::Value e;
        expect(json::parse(econLine, e, error) && e.number("day", -1) == v.number("day", -2), "the same day");
        double households = 0, country = 0, crews = 0, draws = 0, flowsIn = 0, grown = 0;
        for (const auto& r : v.array("trade"))
        {
            const auto& buyer = names.at(std::size_t(r.items()[1].asNumber()));
            const double pennies = r.items()[4].asNumber();
            households += buyer.rfind("hh:", 0) == 0 ? pennies : 0;
            country += buyer == "country" ? pennies : 0;
            crews += buyer == "crew" ? pennies : 0;
        }
        for (const auto& r : v.array("till"))
            draws += r.items()[6].asNumber();
        for (const auto& r : v.array("flow"))
            flowsIn += names.at(std::size_t(r.items()[1].asNumber())) == "treasury" ? r.items()[2].asNumber() : 0;
        for (const auto& r : v.array("farm_good"))
            grown += r.items()[2].asNumber();
        expect(households == e.number("residents_spent") + e.number("services_spent"),
               "households' purchases are the residents' spending and services: " + std::to_string(households) + " " +
                   std::to_string(e.number("residents_spent")));
        expect(country == e.number("basket_spent") + e.number("patrons_estates"), "the Country's purchases and patrons");
        expect(crews == e.number("farms_crew_spent"), "the crews' spending");
        expect(draws == e.number("drawn"), "the store draws: " + std::to_string(draws) + " " + std::to_string(e.number("drawn")));
        expect(grown == e.number("farms_grown"), "the farms' goods grown, by good, add up");
        expect(flowsIn >= e.number("toll"), "the treasury's inflows hold the toll");
        expect(v.array("class").size() >= 1 && v.array("farm").size() >= 1, "the classes and the farms, a row each");
        // Round 3 (doc 73): each maker's crew-hours, in tenths: what it used and lost are within what it put in.
        expect(v.has("work") && v.has("hh_short"), "the work and short households' tables");
        for (const auto& r : v.array("work"))
        {
            const double hours = r.items()[1].asNumber(), used = r.items()[2].asNumber(), lost = r.items()[3].asNumber();
            expect(names.at(std::size_t(r.items()[0].asNumber())).rfind("till:", 0) == 0 && hours >= 0 && used >= 0 && lost >= 0 &&
                       hours + used + lost > 0 &&   // (No empty rows: a till whose post stood empty all day has none.)
                       used + lost <= hours + 1 + 10 * 40,   // (Used may spend hours banked the day before: at most a bank.)
                   "a till's crew-hours");
        }
        for (const auto& r : v.array("hh_short"))
            expect(r.items().size() == 11 && r.items()[2].asNumber() >= 1, "a short household: its members, payers and the hour's purse");
        ++tableDays;
    }
    expect(metaSeen && tableDays == 3, "the meta line, then a table line a day: " + std::to_string(tableDays));
    // --rule-at DAY NAME VALUE (doc 74's sweep): the steering turned on at day 1's start and the slider moved to the far left at
    // day 2's, as the DM's economy.sync would: day 0's line as the run without them, day 1 steered at the centre, day 2 with
    // the push at once at the slider (feed-forward) and the levers with it.
    const auto [ruled, ruledOut] = runIt("ruled", "--threads 1 --rule-at 1 steer 1 --rule-at 2 steerLean 0");
    expect(ruledOut.find("day 1: the rule steer set to 1") != std::string::npos && ruledOut.find("day 2: the rule steerLean set to 0") != std::string::npos,
           "--rule-at: says what it set: " + ruledOut.substr(0, 600));
    {
        std::istringstream lines(ruled), plain(one);
        std::string l0, p0, l1, l2;
        std::getline(lines, l0);
        std::getline(plain, p0);
        std::getline(lines, l1);
        std::getline(lines, l2);
        expect(l0 == p0, "day 0 is as the run without --rule-at");
        json::Value v1, v2;
        std::string error;
        expect(json::parse(l1, v1, error) && json::parse(l2, v2, error), "the ruled lines are JSON: " + error);
        expect(v1.number("steer", -1) == 1 && v1.number("steer_lean", -1) == 0 && v1.number("steer_push", -9) == 0,
               "day 1: the steering on, at the centre: " + l1.substr(0, 200));
        expect(v2.number("steer", -1) == 1 && v2.number("steer_lean", 0) == -100 && v2.number("steer_push", 0) == -1 &&
                   v2.number("lev_levelling", 0) == 40 && v2.number("tithe", -1) >= 0,
               "day 2: the slider far left, the levers there (the Levelling tithe 40%) at once: " +
                   std::to_string(v2.number("lev_levelling", 0)) + "%");
        expect(v2["conserved"].asBool(), "and the money conserved");
    }
    const auto badRule = "\"" + binary + "\" \"" + root.string() + "\" --simulate 6 30 --rule-at 1 noSuchRule 1 > /dev/null 2>&1";
    expect(std::system(badRule.c_str()) != 0, "--rule-at refuses a rule that isn't one");
    const auto [fast, fastOut] = runIt("fast", "--threads 1 --fast");
    expect(fastOut.find("unseen step 30 s") != std::string::npos && fastOut.find("straight") != std::string::npos &&
               fastOut.find("0.5 s ticks") != std::string::npos,
           "--fast: the unseen step 30, straight legs, coarser ticks");
    fs::remove_all(out);
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const auto root = writeTown(fs::temp_directory_path() / ("ratw_sim_town_" + std::to_string(::getpid())));
        knobs();
        harnessPath(root);
        if (argc > 1)
            theHarness(argv[1], root);
        fs::remove_all(root);
    }
    catch (const std::exception& error)
    {
        std::cerr << "sim_fast_tests failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "sim_fast_tests: " << checks << " checks passed\n";
    return 0;
}
