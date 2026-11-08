// The fast economy run (Docs/Design/69-simulation-speed.md): on a small streamed town (an export, as world_check loads
// one), the harness's path makes the births and arrivals the society asks for, as the game server does for a world from
// files (World::makeRequestedResidents after each tick), and the old die of old age (World::tendOldAge) as everywhere;
// the same world run twice, on one thread and on four, with the profile's clocks on and off, ends the same; the unseen
// step and straight legs are knobs a harness turns (and turns back). Given world_check's path (the first argument, as
// ctest passes it), it also runs the harness itself with no player: the same on 1 and 4 threads, births and deaths.
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
        // purse is the Country's, real money balanced.
        const auto* life = w.society().resident("fen");
        expect(life && life->homeCell == cellId(0) && !fen->dead, "Fen lives with a household of the town: " + (life ? life->homeCell : ""));
        expect(w.society().jobOf("fen") && w.society().jobOf("fen")->id == "job:tamsin", "and keeps the smithy");
        expect(!w.society().unplaced("fen"), "no longer unplaced");
    }
    expect(w.society().realBalanced(), "real money balanced");
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

// world_check itself, with no player: the same run on one thread and four (its digest), its days, births and deaths.
// (Its economy file and reports went with the city's loop: Docs/Design/79-a-staged-economy.md, 17.)
void theHarness(const std::string& binary, const fs::path& root)
{
    const auto out = fs::temp_directory_path() / ("ratw_sim_fast_" + std::to_string(::getpid()));
    fs::create_directories(out);
    const auto runIt = [&](const std::string& tag, const std::string& more) {
        const auto cmd = "\"" + binary + "\" \"" + root.string() + "\" --simulate 6 78 --no-check --deterministic " + more + " > \"" +
                         (out / (tag + ".out")).string() + "\" 2>&1";
        expect(std::system(cmd.c_str()) == 0, "world_check runs: " + slurp(out / (tag + ".out")));
        return slurp(out / (tag + ".out"));
    };
    const auto lineOf = [](const std::string& text, const std::string& start) {
        const auto at = text.find(start);
        return at == std::string::npos ? std::string() : text.substr(at, text.find('\n', at) - at);
    };
    const auto oneOut = runIt("one", "--players 0 --threads 1");
    const auto fourOut = runIt("four", "--players 0 --threads 4 --no-profile");
    expect(oneOut.find("no player") != std::string::npos, "a run with no player: " + oneOut.substr(0, 400));
    expect(oneOut.find("run day 3") != std::string::npos, "and says what each day cost");
    const auto people = lineOf(oneOut, "  people: ");
    expect(people.find(" 2 died in the run") != std::string::npos, "the two of 100 die: " + people);
    expect(people.find(" born, ") != std::string::npos && people.find(" arrived from the Country") != std::string::npos &&
               people.find(" children still owed") != std::string::npos && people.find("1 unplaced NPCs left") != std::string::npos,
           "births, arrivals, the children owed and the unplaced counted: " + people);
    const auto digest = lineOf(oneOut, "  digest: ");
    expect(!digest.empty() && digest == lineOf(fourOut, "  digest: "), "the same run on four threads, with the profile off: " + digest);
    expect(people == lineOf(fourOut, "  people: "), "the same people on four threads");
    expect(oneOut.find("real money balanced") != std::string::npos, "real money balanced at the end");
    const auto playerOut = runIt("player", "--threads 1 --players 1");
    expect(playerOut.find("1 player") != std::string::npos, "--players 1 keeps a player for a city check");
    for (const char* gone : {"--economy x.jsonl", "--farms 0", "--rule-at 1 steer 1", "--steer-table x.json", "--economy-level full"})
    {
        const auto cmd = "\"" + binary + "\" \"" + root.string() + "\" --simulate 6 30 " + gone + " > /dev/null 2>&1";
        expect(std::system(cmd.c_str()) != 0, std::string("the economy's old flag is refused: ") + gone);
    }
    const auto fastOut = runIt("fast", "--players 0 --threads 1 --fast");
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
