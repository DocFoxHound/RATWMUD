// The fast economy run (Docs/Design/69-simulation-speed.md): on a small streamed town (an export, as world_check loads
// one), the harness's path makes the births and newcomers the society asks for, as the game server does for a world from
// files (World::makeRequestedResidents after each tick), and the old die of old age (World::tendOldAge) as everywhere;
// the same world run twice, on one thread and on four, with the profile's clocks on and off, ends the same; the unseen
// step and straight legs are knobs a harness turns (and turns back). Given world_check's path (the first argument, as
// ctest passes it), it also runs the harness itself with no player: an economy file a day, the same on 1 and 4 threads,
// births and deaths in it.
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
// first day: old Hob, retired (no post: only a child can take his place), and old Tamsin, who keeps the smithy (a post a
// stranger comes for after eight days). Wren and Birch are married and live together, of an age for children.
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
    int births = 0, newcomers = 0, deaths = 0;
    std::vector<std::string> made;                  // In the order they came: "birth wren" / "newcomer tamsin".
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
            (r.kind == "birth" ? t.births : t.newcomers) += 1;
            t.made.push_back(r.kind + " " + r.templateId);
        }
    }
    for (const auto& [id, e] : w.entities())
        t.deaths += e.npc && e.dead;
    if (state)
        *state = stateOf(w);
    // Everyone made is in the world and the society, alive, at home, of the right age and sex.
    for (const auto& [id, e] : w.entities())
    {
        if (id.rfind("born_", 0) != 0 && id.rfind("new_", 0) != 0)
            continue;
        const bool child = id.rfind("born_", 0) == 0;
        const auto* life = w.society().resident(id);
        const auto* spec = w.society().spec(id);
        expect(life && spec && e.npc, id + " is a resident");
        expect(!child || e.age == 0 || e.age == 1, id + ", born here, is a child: " + std::to_string(e.age));
        expect(spec->appearance.sex == "male" || spec->appearance.sex == "female", id + " is male or female");
        // (A child at its mother's home; a stranger where the last holder lived, else by the work: here, the market.)
        expect(life->homeCell == (child ? cellId(0) : cellId(1)), id + " lives in the town: " + life->homeCell);
    }
    return t;
}

void harnessPath(const fs::path& root)
{
    std::string one, four, unprofiled;
    const auto a = run(root, 120, 1, true, &one);
    expect(a.deaths >= 2, "the two of 100 die of old age: " + std::to_string(a.deaths));
    expect(a.newcomers >= 1, "a stranger comes to keep the smithy: " + std::to_string(a.newcomers));
    expect(a.births >= 1, "and a child is born to take old Hob's place: " + std::to_string(a.births));
    bool newcomerForTamsin = false, childOfWren = false;
    for (const auto& m : a.made)
    {
        newcomerForTamsin = newcomerForTamsin || m == "newcomer tamsin";
        childOfWren = childOfWren || m == "birth wren";
    }
    expect(newcomerForTamsin, "the stranger is like the last smith");
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
    expect(one.find("\"births\": ") != std::string::npos && one.find("\"newcomers\": ") != std::string::npos, "births and newcomers counted");
    expect(one == four, "the economy file is the same on four threads, with the profile off");
    const auto [player, playerOut] = runIt("player", "--threads 1 --players 1");
    expect(playerOut.find("1 player") != std::string::npos, "--players 1 keeps a player for a city check");
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
