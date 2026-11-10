// Loads a streamed world export (python3 tools/world_build.py export DIR writes DEV's newest build) the way the game
// server does, then brings every cell into memory so the server's own checks run on all of it.
//
//   world_check EXPORT_DIR [--simulate FROM_HOUR TO_HOUR [--players N [--at-seams]] [--no-check] [--full] [--deterministic] [--events FILE]
//                                                       [--threads N] [--day N] [--sample] [--no-profile] [--no-births]
//                                                       [--fast] [--unseen-step N] [--straight] [--coarse-ticks] [--ship-day]]
//
// --simulate then runs the world the way the game server does (20 ticks a second) between two hours of the day and
// reports how long ticks take, so a region's population can be checked against the server's 50 ms tick budget.
// --players N (default 1) adds walking players: the first at the spawn, the rest beside residents
// spread over the world, each observed every tick and sent a view five times a second as the server does (--at-seams:
// the rest by the seams between outdoor cells instead, walking over them and back, for sight and the local map across
// seams: Docs/Design/70-the-realm.md). --no-check
// skips loading every place first (the check), which is slow and not needed to measure ticks. --deterministic does the
// work by counts, never the clock (World::setDeterministic), so a run repeats exactly. --full turns the simulation tiers
// off, simulating every NPC in full wherever it is (as before tiers), for comparison. --threads N (default 8; 1: the main
// thread alone) spreads the world's work over threads as the server's pool does (Docs/Design/31, "Fast-forward"); a
// deterministic run ends the same on any number.
//
// A run with no player (--players 0; Docs/Design/69-simulation-speed.md) samples nothing tick by tick (the held
// residents, the road tally, the crowds, each tick's time): --sample keeps them. --no-profile turns World::tick's clocks
// off (World::setProfiling). Each game day's wall time is printed as it ends. Births and arrivals are made as the game
// server makes them for a world from files (World::makeRequestedResidents, every tick); --no-births only asks for them.
// Deaths are the world's own (old age, World::tendOldAge), the same here as on the server. --day N starts the run on day
// N of the year (1-365: Spring 1-92, Summer 93-184, Autumn 185-275, Winter 276-365). (The economy's reports, --economy
// and the rest, went with the city's loop: Docs/Design/79-a-staged-economy.md, 17; --stage comes in its Phase 11.)
// --fast (opt-in: it changes results) is --unseen-step 30 --straight --coarse-ticks: the unseen decide every N seconds,
// each decision N seconds' worth (Society::setUnseenStep; the game's is 5); offstage legs are timed by their straight
// line (World::setStraightOffstage); and while nobody is onstage the harness ticks half a second at a time, not 0.05 s.
// --ship-day (doc 79, Phase 5's repair) brings each port's next ship in five seconds into the window (World::dockShip,
// as the DM would), so a window that no ship's own hour falls in is measured with a ship in: its goods on the shelves,
// the Wharf's hands at its cargo, the Wharf a crowd.
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwJsonDoc.h"
#include "RatwPool.h"
#include "RatwWire.h"
#include "RatwWorld.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <tuple>
#include <sstream>
#include <set>
#include <string>
#include <unordered_map>

using namespace ratw;
namespace fs = std::filesystem;

namespace
{
std::string slurp(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}
// Keeping to the roads: the outdoor world as one grid of whether a road is within a tile (read from the export's cells,
// by the game's own rule, roadTerrain: a tile the catalog marks a way), and how far residents walked on it
// and off it, sampled every second along the straight line between where they were and where they are.
struct RoadTally
{
    int size = 0;
    std::vector<std::uint8_t> near;                 // 1: a road within a tile; 2: open ground; 0: no outdoor cell.
    std::map<std::string, std::pair<double, double>> offsets;   // Outdoor cell -> its world position.
    std::map<std::string, Vec2> last;               // Resident -> where it was last (world tiles); NaN when indoors.
    double on[2] = {0, 0}, off[2] = {0, 0};         // By [onstage, offstage].
    std::map<std::string, double> offBy;            // Off-road distance by what the walker was doing,
    std::map<std::string, double> offIn;            // and by where (the cell; for travellers, those "on the road").
    double travel[2] = {0, 0};                      // Travellers' ("on the road", "going ...") [on, off] the road.

    static bool road(char code) { return roadTerrain(code); }
    void load(const fs::path& root)
    {
        std::vector<std::tuple<int, int, std::vector<std::string>>> grids;
        for (const auto& file : fs::directory_iterator(root / "cells"))
        {
            const std::string text = slurp(file.path());
            if (text.find("\noutdoors: true") == std::string::npos)
                continue;
            std::istringstream lines(text);
            std::string line, id;
            int wx = 0, wy = 0;
            std::vector<std::string> rows;
            bool grid = false;
            while (std::getline(lines, line))
            {
                if (grid)
                    rows.push_back(line);
                else if (line.rfind("id: ", 0) == 0)
                    id = line.substr(4);
                else if (line.rfind("world: ", 0) == 0)
                    std::istringstream(line.substr(7)) >> wx >> wy;
                else if (line == "grid:")
                    grid = true;
            }
            offsets[id] = {wx, wy};
            for (std::size_t y = 0; y < rows.size(); ++y)
                size = std::max({size, wx + int(rows[y].size()), wy + int(y) + 1});
            grids.emplace_back(wx, wy, std::move(rows));
        }
        std::vector<std::uint8_t> isRoad(std::size_t(size) * std::size_t(size), 0);
        near.assign(isRoad.size(), 0);
        for (const auto& [wx, wy, rows] : grids)
            for (std::size_t y = 0; y < rows.size(); ++y)
                for (std::size_t x = 0; x < rows[y].size(); ++x)
                {
                    const auto at = std::size_t(wy + int(y)) * std::size_t(size) + std::size_t(wx + int(x));
                    near[at] = 2;
                    isRoad[at] = road(rows[y][x]);
                }
        auto spread = near;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                if (isRoad[std::size_t(y) * std::size_t(size) + std::size_t(x)])
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            if (x + dx >= 0 && y + dy >= 0 && x + dx < size && y + dy < size &&
                                near[std::size_t(y + dy) * std::size_t(size) + std::size_t(x + dx)])
                                spread[std::size_t(y + dy) * std::size_t(size) + std::size_t(x + dx)] = 1;
        near = std::move(spread);
    }
    void sample(const World& world)
    {
        for (const auto& [id, e] : world.entities())
        {
            if (!e.npc || e.dead)
                continue;
            const auto cell = offsets.find(e.cellId);
            const Vec2 now = cell == offsets.end() ? Vec2{NAN, NAN}
                                                   : Vec2{cell->second.first + e.position.x, cell->second.second + e.position.y};
            auto& before = last[id];
            const Vec2 then = before;
            before = now;
            if (!std::isfinite(now.x) || !std::isfinite(then.x))
                continue;
            const double length = std::hypot(now.x - then.x, now.y - then.y);
            if (length < .25 || length > 400)
                continue;
            const int steps = std::max(1, int(std::ceil(length)));
            double onRoad = 0, offRoad = 0;
            for (int s = 0; s < steps; ++s)
            {
                const double f = (s + .5) / steps;
                const int x = int(std::floor(then.x + (now.x - then.x) * f)), y = int(std::floor(then.y + (now.y - then.y) * f));
                if (x < 0 || y < 0 || x >= size || y >= size)
                    continue;
                const auto n = near[std::size_t(y) * std::size_t(size) + std::size_t(x)];
                (n == 1 ? onRoad : offRoad) += length / steps;
            }
            on[e.offstage] += onRoad;
            off[e.offstage] += offRoad;
            if (e.activity.rfind("on the road", 0) == 0 || e.activity.rfind("going ", 0) == 0)
                travel[0] += onRoad, travel[1] += offRoad;
            if (offRoad > 0)
            {
                offBy[e.activity.substr(0, e.activity.find(" — "))] += offRoad;
                if (e.activity.rfind("on the road", 0) == 0)
                    offIn[e.cellId + (e.offstage ? " (offstage)" : "")] += offRoad;
            }
        }
    }
    void report() const
    {
        const auto share = [](double a, double b) { return a + b > 0 ? 100.0 * a / (a + b) : 0.0; };
        std::cout << "  keeping to the roads, outdoors: onstage " << long(on[0] + off[0]) << " tiles walked, " << share(on[0], off[0])
                  << "% within a tile of a road; offstage " << long(on[1] + off[1]) << " tiles, " << share(on[1], off[1]) << "%\n";
        std::vector<std::pair<double, std::string>> most;
        for (const auto& [what, d] : offBy)
            most.emplace_back(d, what);
        std::sort(most.rbegin(), most.rend());
        std::cout << "    off the road most, by what they were doing (tiles):";
        for (std::size_t i = 0; i < most.size() && i < 8; ++i)
            std::cout << " " << most[i].second << " " << long(most[i].first) << ";";
        std::cout << "\n";
        most.clear();
        for (const auto& [where, d] : offIn)
            most.emplace_back(d, where);
        std::sort(most.rbegin(), most.rend());
        std::cout << "    travellers (on the road to somewhere, going somewhere): " << long(travel[0] + travel[1]) << " tiles, "
                  << share(travel[0], travel[1]) << "% within a tile of a road\n";
        std::cout << "    travellers off the road, by where (tiles):";
        for (std::size_t i = 0; i < most.size() && i < 8; ++i)
            std::cout << " " << most[i].second << " " << long(most[i].first) << ";";
        std::cout << "\n";
    }
};

} // namespace

int main(int argc, char** argv)
{
    const bool simulate = argc >= 5 && std::string(argv[2]) == "--simulate";
    int playerCount = 1;
    bool sample = false;                            // --sample: the per-tick sampling even with no player.
    bool profile = true;                            // --no-profile: World::tick's clocks off.
    bool births = true;                             // --no-births: births and arrivals only asked for.
    bool fast = false, straight = false, coarse = false;   // --fast, --straight, --coarse-ticks.
    int unseenStep = 0;                             // --unseen-step N (0: the game's, Society::UnseenStep).
    bool unseenStepSet = false;
    bool check = true;
    bool full = false;                              // --full: every NPC in full simulation (no tiers).
    bool deterministic = false;                     // --deterministic: work by counts, never the clock (World::setDeterministic).
    std::string eventsFile;                         // --events FILE: what happened, and everyone's names, as JSON.
    unsigned threads = 8;                           // --threads N.
    int startDay = 0;                               // --day N: start on day N of the year (1-365; 0: the world's own).
    bool atSeams = false;                           // --at-seams: the players after the first by seams (doc 70).
    bool shipDay = false;                           // --ship-day: each port's next ship in, early in the window.
    bool usage = argc != 2 && !simulate;
    for (int i = 5; simulate && i < argc; ++i)
    {
        const std::string flag = argv[i];
        if (flag == "--players" && i + 1 < argc)
            playerCount = std::max(0, std::atoi(argv[++i]));
        else if (flag == "--at-seams")
            atSeams = true;
        else if (flag == "--ship-day")
            shipDay = true;
        else if (flag == "--sample")
            sample = true;
        else if (flag == "--no-profile")
            profile = false;
        else if (flag == "--no-births")
            births = false;
        else if (flag == "--fast")
            fast = true;
        else if (flag == "--straight")
            straight = true;
        else if (flag == "--coarse-ticks")
            coarse = true;
        else if (flag == "--unseen-step" && i + 1 < argc)
        {
            unseenStep = std::clamp(std::atoi(argv[++i]), 1, 600);
            unseenStepSet = true;
        }
        else if (flag == "--no-check")
            check = false;
        else if (flag == "--full")
            full = true;
        else if (flag == "--deterministic")
            deterministic = true;
        else if (flag == "--events" && i + 1 < argc)
            eventsFile = argv[++i];
        else if (flag == "--threads" && i + 1 < argc)
            threads = unsigned(std::max(1, std::atoi(argv[++i])));
        else if (flag == "--day" && i + 1 < argc)
        {
            startDay = std::atoi(argv[++i]);
            usage = usage || startDay < 1 || startDay > calendar::DaysPerYear;
        }
        else
            usage = true;
    }
    if (usage)
    {
        std::cerr << "usage: world_check EXPORT_DIR [--simulate FROM_HOUR TO_HOUR [--players N [--at-seams]] [--no-check] [--full] [--deterministic]"
                     " [--events FILE] [--threads N] [--day N] [--sample] [--no-profile] [--no-births] [--fast]"
                     " [--unseen-step N] [--straight] [--coarse-ticks] [--ship-day]]\n";
        return 2;
    }
    if (fast)
    {
        unseenStep = unseenStepSet ? unseenStep : 30;
        straight = coarse = true;
    }
    // Per-tick sampling (held residents, the road tally, the crowds, each tick's time) when someone plays, or asked.
    const bool sampling = playerCount > 0 || sample;
    if (coarse && sampling)
    {
        std::cerr << "--coarse-ticks (and --fast) need no player and no --sample: ticking at 0.05 s.\n";
        coarse = false;
    }
    const fs::path root = argv[1];
    std::map<std::string, std::string> files;
    for (const auto& entry : fs::recursive_directory_iterator(root))
        if (entry.is_regular_file())
            files[fs::relative(entry.path(), root).generic_string()] = slurp(entry.path());
    auto find = [&](const std::string& path) -> const std::string* {
        const auto it = files.find(path);
        return it == files.end() ? nullptr : &it->second;
    };
    World world;
    world.setCellSource({
        [&](const std::string& id, std::string& header) {
            const auto* cell = find("cells/" + id + ".cell");
            if (!cell)
                return std::string("no cell file for ") + id;
            header = cell->substr(0, cell->find("grid:") + 5);
            return std::string();
        },
        [&](const std::string& id, std::string& cell, std::string& seams) {
            const auto* text = find("cells/" + id + ".cell");
            if (!text)
                return std::string("no cell file for ") + id;
            cell = *text;
            const auto* sides = find("seams/" + id);
            seams = sides ? *sides : std::string();
            return std::string();
        }});
    std::map<std::string, std::string> manifest;
    for (const auto& [path, text] : files)
        if (path.rfind("cells/", 0) != 0 && path.rfind("seams/", 0) != 0)
            manifest[path] = text;
    const auto loaded = world.loadWorldFiles(manifest, root.string());
    if (!loaded.ok)
    {
        std::cerr << "World failed to load: " << loaded.message << '\n';
        return 1;
    }
    std::size_t tiles = 0, cells = 0, failures = 0;
    double loadTotal = 0, loadWorst = 0;
    std::string slowest;
    for (const auto& [id, cell] : world.cells())
    {
        if (!check)
            break;
        const auto loadBegin = std::chrono::steady_clock::now();
        const auto result = world.ensureLoaded(id);
        const double took = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - loadBegin).count();
        loadTotal += took;
        if (took > loadWorst)
        {
            loadWorst = took;
            slowest = id;
        }
        if (!result.ok)
        {
            std::cerr << id << ": " << result.message << '\n';
            ++failures;
            continue;
        }
        ++cells;
        tiles += world.cell(id)->tiles.size();
    }
    if (check && cells)
        std::cout << "loading a place: mean " << loadTotal / double(cells) << " ms, worst " << loadWorst << " ms ("
                  << slowest << ")\n";
    if (check)
        std::cout << cells << " places loaded (" << tiles << " tiles), " << world.doors().size() << " doors and seams, "
              << failures << " failures\n";
    if (failures || !simulate)
        return failures ? 1 : 0;

    // A fresh world, as a server starts: only the cells someone is in or beside get loaded.
    World server;
    Pool pool(threads - 1);                         // (The main thread takes a share too.)
    if (threads > 1)
        server.setParallel([&](std::size_t count, const std::function<void(std::size_t)>& job) { pool.run(count, job); });
    server.setCellSource({
        [&](const std::string& id, std::string& header) {
            const auto* cell = find("cells/" + id + ".cell");
            if (!cell)
                return std::string("no cell file for ") + id;
            header = cell->substr(0, cell->find("grid:") + 5);
            return std::string();
        },
        [&](const std::string& id, std::string& cell, std::string& seams) {
            const auto* text = find("cells/" + id + ".cell");
            if (!text)
                return std::string("no cell file for ") + id;
            cell = *text;
            const auto* sides = find("seams/" + id);
            seams = sides ? *sides : std::string();
            return std::string();
        }});
    if (!server.loadWorldFiles(manifest, root.string()).ok)
        return 1;
    if (full)
        server.setTiered(false);
    server.setDeterministic(deterministic);
    server.setProfiling(profile);
    if (unseenStep > 0)
        Society::setUnseenStep(unseenStep);
    World::setStraightOffstage(straight);
    if (unseenStep > 0 || straight || coarse)
        std::cout << "  fast steps (they change results): unseen step " << Society::unseenStep() << " s" << (straight ? ", straight offstage legs" : "")
                  << (coarse ? ", 0.5 s ticks while nobody is onstage" : "") << "\n";
    const double from = std::atof(argv[3]), to = std::atof(argv[4]);
    server.setTimeOfDay(from);
    if (startDay > 0)
    {
        // --day N: the run starts on that day of the year (the seasons on the Board: doc 79, 7.1). Nobody ages: an
        // entity's first birthday is reckoned from the day it is first seen.
        server.advanceCalendar(double(startDay - 1) - std::floor(server.calendarDays()));
        const auto date = calendar::calendarAt(server.calendarDays());
        std::cout << "  starting on day " << date.dayOfYear << " of the year (" << calendar::seasonName(date.season) << ", day "
                  << date.dayOfSeason << ")\n";
    }
    // Players walking about, each of whose views is built five times a second as the server does for each client:
    // the first at the spawn, the others beside residents taken evenly through the population.
    std::vector<std::string> players;
    std::vector<std::string> residents;
    for (const auto& [id, e] : server.entities())
        if (e.npc)
            residents.push_back(id);
    for (int p = 0; p < playerCount; ++p)
    {
        const std::string id = p ? "player-bench-" + std::to_string(p) : "player-bench";
        server.addPlayer(id, "Bench " + std::to_string(p));
        players.push_back(id);
        if (p && !residents.empty())
            if (const auto* beside = server.entity(residents[std::size_t(p - 1) * residents.size() / std::size_t(playerCount)]))
            {
                auto* me = server.entity(id);
                me->cellId = beside->cellId;
                me->position = beside->position;
            }
    }
    if (atSeams)
    {
        // Each player after the first a little way in from the east edge of an outdoor cell with another beyond it,
        // taken evenly through them (the players walk east and west, over the seam and back).
        std::vector<std::string> edges;
        for (const auto& [id, c] : server.cells())
            for (const auto& [other, d] : server.cells())
                if (c.outdoors && d.outdoors && std::abs(d.worldX - (c.worldX + c.width)) < .5 && std::abs(d.worldY - c.worldY) < .5 &&
                    std::abs(d.worldZ - c.worldZ) < .01)
                    edges.push_back(id);
        int placed = 0;
        for (int p = 1; p < playerCount && !edges.empty(); ++p)
        {
            const auto& at = edges[std::size_t(p - 1) * edges.size() / std::size_t(playerCount - 1)];
            if (!server.ensureLoaded(at).ok)
                continue;
            const auto* c = server.cell(at);
            bool stood = false;
            for (int r = 0; r < 40 && !stood; ++r)
                for (int dy = -r; dy <= r && !stood; ++dy)
                    stood = server.teleport(players[std::size_t(p)], at, c->width - 12.5, c->height / 2 + dy + .5).ok ||
                            server.teleport(players[std::size_t(p)], at, c->width - 12.5 - r, c->height / 2 + dy + .5).ok;
            placed += stood;
        }
        std::cout << "  " << placed << " players by seams (of " << edges.size() << " cells with one east of them)\n";
    }
    std::vector<double> views, motions;
    const auto npcs = [&] {
        std::size_t n = 0;
        for (const auto& [id, e] : server.entities())
            n += e.npc;
        return n;
    };
    std::set<std::string> deadAtStart;
    for (const auto& [id, e] : server.entities())
        if (e.npc && e.dead)
            deadAtStart.insert(id);
    std::map<std::string, std::string> start;
    for (const auto& [id, e] : server.entities())
        if (e.npc)
            start[id] = e.cellId + " " + std::to_string(int(e.position.x)) + "," + std::to_string(int(e.position.y));
    // In 0.05 s steps of world time: a game hour is 600 real seconds; the server ticks at 20 Hz.
    const int ticks = int((to - from) * 600.0 / 0.05);
    std::vector<ratw::WorldEvent> events;
    struct WorstTick
    {
        double total = 0, streaming = 0, schedules = 0, movement = 0, separation = 0, views = 0;
        std::size_t searches = 0;
        int at = 0;
    } worstSteady;
    std::vector<double> times, steady;           // `steady`: ticks in which no place had to be loaded.
    // Each game day's share, to see whether ticks grow dearer as the world ages: total, schedules, movement,
    // separation (ms), and ticks.
    std::map<int, std::array<double, 5>> byDay;
    // And what grows: the schedules' stages (ms), route searches, and (sampled every 30 s, the most in the day) the
    // wolves in the world, those onstage, the road folk, and the most standing on one tile.
    std::map<int, std::array<double, 7>> stagesByDay;
    std::map<int, std::size_t> searchesByDay;
    std::map<int, std::array<std::size_t, 4>> crowdByDay;
    std::map<int, std::string> biggestCrowd;        // Where each day's biggest crowd stood, and what it was doing.
    std::vector<double> picking;                    // The ambient director's look every five seconds (Phase 10).
    std::size_t picked = 0;
    if (sampling)
        times.reserve(std::size_t(ticks));
    std::size_t mostLoaded = 0;
    // Residents held more than ten game minutes on one tile while bound elsewhere (a game minute is 10 s, 200 ticks).
    struct Held
    {
        std::tuple<std::string, int, int> at;
        int since = 0;
        bool counted = false;
    };
    constexpr int HeldTicks = 10 * 200;
    std::map<std::string, Held> held;
    std::vector<std::string> heldOnce;
    RoadTally roads;
    if (sampling)
        roads.load(root);
    // Each game day's wall time (and, with the profile on, its stages), printed as it ends: the cost of a long run by
    // day, so any growth shows.
    constexpr int DayTicks = 24 * 12000;            // (A run's days are 24 game hours from its start.)
    const auto dayOf = [](int at) { return at / DayTicks; };
    struct DayCost
    {
        double seconds = 0, stages[7] = {};
        int ticks = 0;
    };
    std::map<int, DayCost> costByDay;
    const auto runBegun = std::chrono::steady_clock::now();
    auto dayBegun = runBegun;
    double stagesAtDay[7];
    std::copy(std::begin(server.tickProfile().stages), std::end(server.tickProfile().stages), stagesAtDay);
    int day = dayOf(0), ticksToday = 0, ticksRun = 0, madeBirths = 0, madeArrivals = 0;
    std::vector<ResidentRequest> made;
    const char* streamTrace = std::getenv("RATW_STREAM_TRACE");
    const char* trace = std::getenv("RATW_TRACE");
    const bool ambientLook = !std::getenv("RATW_NO_AMBIENT_LOOK");
    constexpr int CoarseSteps = 10;                 // (--coarse-ticks: half a second, in 0.05 s steps.)
    // Births and arrivals made as the game server makes them for a world from files (Game::makeResidents), after
    // each tick; --no-births leaves them asked for.
    const auto welcome = [&] {
        if (!births)
            return;
        made.clear();
        server.makeRequestedResidents(&made);
        for (const auto& r : made)
        {
            (r.kind == "birth" ? madeBirths : madeArrivals) += 1;
        }
    };
    for (int i = 0; i < ticks;)
    {
        // Coarser ticks while nobody is onstage, on the half seconds, so the hours fall where they fall in 0.05 s ticks.
        const int stride = coarse && !server.anyoneOnstage() ? std::min(CoarseSteps - i % CoarseSteps, ticks - i) : 1;
        if (shipDay && i <= 100 && i + stride > 100)
        {
            // Each port's ship of today, else its first still due, brought in now.
            const auto today = std::int64_t(std::floor(server.calendarDays()));
            for (const auto& port : server.ports())
            {
                const market::Ship* pick = nullptr;
                for (const auto& s : server.society().board().ships)
                    if (s.port == port.market && s.status == "due" && (!pick || (s.day == today && pick->day != today) || (pick->day != today && s.day < pick->day)))
                        pick = &s;
                const std::string id = pick ? pick->id : std::string(), name = pick ? pick->name : std::string();
                const bool docked = pick && server.dockShip(id);
                std::cout << "ship day: " << port.market << " (" << port.posts << " posts, " << port.imports.size() << " goods waiting for ships): "
                          << (docked ? name + " brought in" : pick ? name + " not brought in" : std::string("no ship due")) << '\n';
                // (Which goods: the list a Progress entry quotes is this line's, of the world it was run on.)
                std::cout << "ship goods: " << port.market << ":";
                for (const auto& g : port.imports)
                    std::cout << ' ' << g;
                std::cout << '\n';
            }
            if (server.ports().empty())
                std::cout << "ship day: the world has no port\n";
        }
        if (!sampling)
        {
            server.tick(0.05 * stride);
            ++ticksRun;
            ++ticksToday;
            welcome();
            if (!eventsFile.empty() && (i % 20 == 0 || stride > 1))
                for (auto& e : server.takeEvents())
                    events.push_back(std::move(e));
            if (i % 200 == 0)                       // Once a game minute (10 s).
                mostLoaded = std::max(mostLoaded, server.loadedCells());
        }
        else
        {
            const auto begin = std::chrono::steady_clock::now();
            const auto loadedBefore = server.loadedCells();
            if (i % 300 == 0)                          // Walk east and west, as a player would.
                for (const auto& p : players)
                    server.move(p, (i / 300) % 2 ? -1 : 1, 0);
            const auto profileBefore = server.tickProfile();
            server.tick(0.05);
            ++ticksRun;
            ++ticksToday;
            welcome();
            const auto& profileAfter = server.tickProfile();
            if (i % 20 == 0)
                roads.sample(server);
            if (i % 20 == 0)
            {
                // Held (Docs/Design/67-ridgemere2-city.md, 8: the Sump's narrow passages): a resident bound somewhere else
                // that stays on one tile, sampled every second.
                for (const auto& [id, e] : server.entities())
                {
                    const auto* life = e.npc && !e.dead ? server.society().resident(id) : nullptr;
                    auto& h = held[id];
                    const std::tuple<std::string, int, int> at{e.cellId, int(std::floor(e.position.x)), int(std::floor(e.position.y))};
                    const bool bound = life && !life->goalCell.empty() &&
                                       (e.cellId != life->goalCell || std::hypot(e.position.x - life->goalX, e.position.y - life->goalY) > 1.5) &&
                                       e.activity.find("route unavailable") == std::string::npos;
                    if (!bound || at != h.at)
                    {
                        h.at = at;
                        h.since = i;
                        continue;
                    }
                    if (i - h.since > HeldTicks && !h.counted)
                    {
                        h.counted = true;
                        std::ostringstream where;
                        where << id << " (in " << e.cellId << " " << std::get<1>(at) << "," << std::get<2>(at) << ", bound for " << life->goalCell
                              << ", at " << int(std::fmod(from + i * 0.05 / 600.0, 24.0)) << ":00)";
                        heldOnce.push_back(where.str());
                    }
                }
            }
            if (i % 100 == 0 && ambientLook)
            {
                // Only looked at, never spoken: the simulation goes on exactly as without it.
                const auto lookBegin = std::chrono::steady_clock::now();
                picked += server.ambientPicks(players).size();
                picking.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - lookBegin).count());
            }
            if (!eventsFile.empty() && i % 20 == 0)
                for (auto& e : server.takeEvents())
                    events.push_back(std::move(e));
            const double tickOnly = (profileAfter.streaming.total - profileBefore.streaming.total) +
                                    (profileAfter.schedules.total - profileBefore.schedules.total) +
                                    (profileAfter.movement.total - profileBefore.movement.total) +
                                    (profileAfter.separation.total - profileBefore.separation.total) +
                                    (profileAfter.views.total - profileBefore.views.total);
            auto& dayTotals = byDay[int(std::floor((from + i * 0.05 / 600.0) / 24.0))];
            dayTotals[0] += tickOnly;
            dayTotals[1] += profileAfter.schedules.total - profileBefore.schedules.total;
            dayTotals[2] += profileAfter.movement.total - profileBefore.movement.total;
            dayTotals[3] += profileAfter.separation.total - profileBefore.separation.total;
            dayTotals[4] += 1;
            {
                const int d = int(std::floor((from + i * 0.05 / 600.0) / 24.0));
                for (int k = 0; k < 7; ++k)
                    stagesByDay[d][std::size_t(k)] += profileAfter.stages[k] - profileBefore.stages[k];
                searchesByDay[d] += profileAfter.routeSearches - profileBefore.routeSearches;
                if (i % 600 == 0)
                {
                    std::size_t all = 0, onstage = 0, transient = 0, pile = 0;
                    std::map<std::tuple<std::string, int, int>, std::size_t> tiles;
                    std::tuple<std::string, int, int> pileAt;
                    for (const auto& [id, e] : server.entities())
                    {
                        ++all;
                        transient += e.transient;
                        if (e.offstage || e.dead)
                            continue;
                        ++onstage;
                        const std::tuple<std::string, int, int> at{e.cellId, int(std::floor(e.position.x)), int(std::floor(e.position.y))};
                        if (++tiles[at] > pile)
                        {
                            pile = tiles[at];
                            pileAt = at;
                        }
                    }
                    auto& c = crowdByDay[d];
                    if (pile > c[3])
                    {
                        // What they were about: each activity (its task, before the reason) and how many.
                        std::map<std::string, int> doing;
                        std::size_t moving = 0, routed = 0;
                        for (const auto& [id, e] : server.entities())
                            if (!e.offstage && !e.dead && e.cellId == std::get<0>(pileAt) && int(std::floor(e.position.x)) == std::get<1>(pileAt) &&
                                int(std::floor(e.position.y)) == std::get<2>(pileAt))
                            {
                                ++doing[e.activity.substr(0, e.activity.find(" — "))];
                                moving += std::hypot(e.velocity.x, e.velocity.y) > 1e-6;
                                routed += !e.path.empty();
                            }
                        std::ostringstream where;
                        where << std::get<0>(pileAt) << " " << std::get<1>(pileAt) << "," << std::get<2>(pileAt) << " at "
                              << int(std::fmod(from + i * 0.05 / 600.0, 24.0)) << ":00, " << moving << " moving, " << routed << " with a route:";
                        for (const auto& [what, n] : doing)
                            where << " " << n << "× " << what << ";";
                        biggestCrowd[d] = where.str();
                    }
                    c = {std::max(c[0], all), std::max(c[1], onstage), std::max(c[2], transient), std::max(c[3], pile)};
                }
            }
            if (i >= int(60 / 0.05) && tickOnly > worstSteady.total)
                worstSteady = {tickOnly,
                               profileAfter.streaming.total - profileBefore.streaming.total,
                               profileAfter.schedules.total - profileBefore.schedules.total,
                               profileAfter.movement.total - profileBefore.movement.total,
                               profileAfter.separation.total - profileBefore.separation.total,
                               profileAfter.views.total - profileBefore.views.total,
                               profileAfter.routeSearches - profileBefore.routeSearches, i};
            for (const auto& p : players)
            {
                // The motion frame the server sends each client every tick: who in the cell it can see.
                const auto motionBegin = std::chrono::steady_clock::now();
                const auto* me = server.entity(p);
                std::size_t seen = 0;
                for (const auto& [id, e] : server.entities())
                    if (me && e.cellId == me->cellId && id != me->id && server.visionClarity(me->id, id) > 0)
                        ++seen;
                motions.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - motionBegin).count());
                (void)seen;
            }
            // Five views a second for each player, a quarter of the players each tick, as the server staggers them;
            // their sight is worked out together first, as the server does.
            std::vector<std::string> due;
            for (std::size_t p = 0; p < players.size(); ++p)
                if ((std::size_t(i) + p) % 4 == 0)
                    due.push_back(players[p]);
            {
                const auto viewBegin = std::chrono::steady_clock::now();
                server.prepareViews(due);
                const double shared = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - viewBegin).count();
                if (!due.empty())
                    views.push_back(shared);
            }
            for (std::size_t p = 0; p < players.size(); ++p)
                if ((std::size_t(i) + p) % 4 == 0)
                {
                    const auto viewBegin = std::chrono::steady_clock::now();
                    const auto view = server.snapshot(players[p]);
                    views.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - viewBegin).count());
                    (void)view;
                }
            if (streamTrace && i % 1200 == 0)
                std::cerr << "STREAM t=" << i * 0.05 << " loaded=" << server.loadedCells() << " offstage="
                          << server.offstageCount() << "\n";
            if (trace && i % 200 == 0)
                if (const auto* e = server.entity(trace))
                    std::cerr << "TRACE t=" << i * 0.05 << " " << e->cellId << " (" << e->position.x << "," << e->position.y
                              << ") path=" << e->path.size() << " state=" << e->state << " activity=" << e->activity << "\n";
            times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count());
            if (server.loadedCells() <= loadedBefore)
                steady.push_back(times.back());
            mostLoaded = std::max(mostLoaded, server.loadedCells());
        }
        i += stride;
        if (const int now = dayOf(i); now != day || i >= ticks)
        {
            // A game day done (or the run): its wall time, and its stages where the profile is kept.
            const auto at = std::chrono::steady_clock::now();
            auto& cost = costByDay[day];
            cost.seconds = std::chrono::duration<double>(at - dayBegun).count();
            cost.ticks = ticksToday;
            for (int k = 0; k < 7; ++k)
                cost.stages[k] = (server.tickProfile().stages[k] - stagesAtDay[k]) / 1000.0;
            std::copy(std::begin(server.tickProfile().stages), std::end(server.tickProfile().stages), stagesAtDay);
            std::cout << "  run day " << day + 1 << (i % DayTicks ? " (part)" : "") << ": " << cost.seconds << " s wall, " << ticksToday
                      << " ticks; " << std::chrono::duration<double>(at - runBegun).count() << " s so far" << std::endl;
            dayBegun = at;
            day = now;
            ticksToday = 0;
        }
    }
    {
        const auto& profile = server.tickProfile();
        std::cout << "  inside World::tick, total ms (worst tick):";
        for (const auto& [name, part] : std::vector<std::pair<const char*, World::TickProfile::Part>>{
                 {"streaming", profile.streaming}, {"schedules", profile.schedules}, {"movement", profile.movement},
                 {"separation", profile.separation}, {"views", profile.views}})
            std::cout << ' ' << name << ' ' << long(part.total) << " (" << part.worst << ')';
        std::cout << "; " << profile.routeSearches << " route searches expanding " << profile.routeNodes
                  << " nodes (at most " << profile.largestRoute << " in one)\n";
        std::cout << "  path cache: " << server.pathCacheStats().first << " found, " << server.pathCacheStats().second
                  << " searched\n";
        std::cout << "  slowest route search: " << profile.slowestRoute << " ms in " << profile.slowestRouteCell << ", "
                  << profile.slowestRouteNodes << " nodes, " << profile.slowestRouteWaypoints << " waypoints\n";
        std::cout << "  slowest World::tick after the first minute (tick " << worstSteady.at << "): " << worstSteady.total
                  << " ms = streaming " << worstSteady.streaming << ", schedules " << worstSteady.schedules << " ("
                  << worstSteady.searches << " route searches), movement " << worstSteady.movement << ", separation "
                  << worstSteady.separation << ", views " << worstSteady.views << "\n";
    }
    std::size_t moved = 0;
    for (const auto& [id, e] : server.entities())
        if (e.npc && start.count(id) &&
            start[id] != e.cellId + " " + std::to_string(int(e.position.x)) + "," + std::to_string(int(e.position.y)))
            ++moved;
    double total = 0;
    for (double t : times)
        total += t;
    auto sorted = times;
    std::sort(sorted.begin(), sorted.end());
    const auto pct = [&](double p) { return sorted[std::min(sorted.size() - 1, std::size_t(p * sorted.size()))]; };
    // The first minute includes warming up: every cell someone stands in or beside loads, and every route and
    // region cache is built, at once. Report the steady state after it separately.
    const std::size_t warm = std::min(times.size(), std::size_t(60 / 0.05));
    const auto worstAt = std::size_t(std::max_element(times.begin(), times.end()) - times.begin());
    double warmWorst = 0, afterWorst = 0;
    for (std::size_t t = 0; t < times.size(); ++t)
        (t < warm ? warmWorst : afterWorst) = std::max(t < warm ? warmWorst : afterWorst, times[t]);
    std::vector<double> after(times.begin() + std::ptrdiff_t(warm), times.end());
    std::sort(after.begin(), after.end());
    if (!after.empty())
        std::cout << "  after the first minute: p99 " << after[std::min(after.size() - 1, std::size_t(.99 * after.size()))]
                  << " ms, p99.9 " << after[std::min(after.size() - 1, std::size_t(.999 * after.size()))]
                  << " ms, worst " << afterWorst << " ms (first minute worst " << warmWorst << " ms; worst tick "
                  << worstAt << ")\n";
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - runBegun).count();
    if (sampling)
        std::cout << npcs() << " residents, " << players.size() << " player" << (players.size() == 1 ? "" : "s") << ", " << ticks << " ticks from " << from << ":00 to " << to << ":00 ("
                  << total / 1000.0 << " s of work): mean " << total / ticks << " ms, p99 " << pct(.99) << " ms, p99.9 "
                  << pct(.999) << " ms, worst " << sorted.back() << " ms; " << moved << " residents moved; at most "
                  << mostLoaded << " places in memory; " << threads << " thread" << (threads == 1 ? "" : "s") << "\n";
    else
        std::cout << npcs() << " residents, no player, " << ticksRun << " ticks (" << ticks << " of 0.05 s) from " << from << ":00 to " << to
                  << ":00: " << wall << " s wall, " << wall / std::max(1e-9, (to - from) / 24.0) << " s a game day, mean "
                  << 1000.0 * wall / std::max(1, ticksRun) << " ms a tick; " << moved << " residents moved; at most " << mostLoaded
                  << " places in memory; " << threads << " thread" << (threads == 1 ? "" : "s") << "\n";
    if (costByDay.size() > 1 || !sampling)
    {
        std::cout << "  by run day (wall s; ticks; with the profile on, the schedules' stages in s: society bonds roads crime errands"
                     " streaming routes):\n";
        for (const auto& [d, c] : costByDay)
        {
            std::cout << "    day " << d + 1 << ": " << c.seconds << "; " << c.ticks << ";";
            for (const double sec : c.stages)
                std::cout << " " << sec;
            std::cout << "\n";
        }
    }
    // Who is where their day says they should be, and who could find no way there.
    std::size_t arrived = 0, underway = 0;
    std::vector<std::string> lost;
    for (const auto& [id, e] : server.entities())
    {
        const auto* life = server.society().resident(id);
        if (!e.npc || !life || life->goalCell.empty())
            continue;
        if (e.cellId == life->goalCell && std::hypot(e.position.x - life->goalX, e.position.y - life->goalY) <= 1.5)
            ++arrived;
        else if (e.activity.find("route unavailable") != std::string::npos)
            lost.push_back(id + " (in " + e.cellId + ", bound for " + life->goalCell + ")");
        else
            ++underway;
    }
    std::cout << "  " << server.offstageCount() << " residents offstage at the end; real money "
              << (server.society().realBalanced() ? "balanced" : "NOT BALANCED") << "; " << server.loadedCells()
              << " places in memory at the end; " << server.bonds().count() << " bonds between them\n";
    {
        const auto& roads = server.roads();
        std::size_t travelling = 0, raided = 0, camps = 0, beliefs = 0, open = 0;
        for (const auto& c : roads.caravans)
            (c.status == "raided" ? raided : travelling) += 1;
        for (const auto& b : roads.camps)
            camps += b.active;
        for (const auto& k : roads.contracts)
            open += k.status == "open" || k.status == "taken";
        for (const auto& [id, e] : server.entities())
            if (const auto* mine = server.beliefsOf(id))
                beliefs += mine->size();
        std::cout << "  roads: " << server.towns().size() << " towns (";
        for (const auto& t : server.towns())
            std::cout << t.id << (&t == &server.towns().front() ? "*" : "") << (&t == &server.towns().back() ? "" : ", ");
        std::cout << "); " << travelling << " caravans on the road, " << raided << " robbed; " << camps << " bandit camps; "
                  << open << " open contracts; " << beliefs << " rumours held\n";
    }
    {
        std::map<std::string, int> kinds;
        for (const auto& i : server.crime().incidents)
            ++kinds[i.kind];
        std::cout << "  crime: " << server.crime().incidents.size() << " incidents (";
        for (const auto& [kind, n] : kinds)
            std::cout << n << " " << kind << (kind == kinds.rbegin()->first ? "" : ", ");
        std::cout << "); " << server.crime().warrants.size() << " wanted, " << server.crime().custody.size() << " held\n";
    }
    if (!picking.empty())
    {
        double total = 0;
        for (const double t : picking)
            total += t;
        std::cout << "  ambient director: a look every 5 s takes mean " << total / double(picking.size()) << " ms, worst "
                  << *std::max_element(picking.begin(), picking.end()) << " ms; " << picked << " exchanges it would voice\n";
    }
    if (byDay.size() > 1)
    {
        std::cout << "  by game day (mean ms a tick: all = schedules + movement + separation + ...):\n";
        for (const auto& [d, t] : byDay)
            std::cout << "    day " << d + 1 << ": " << t[0] / t[4] << " = " << t[1] / t[4] << " + " << t[2] / t[4] << " + "
                      << t[3] / t[4] << " + ...\n";
        std::cout << "  by game day (schedules by stage, mean ms a tick: society bonds roads crime errands streaming routes; route"
                     " searches a tick; most wolves, onstage, road folk, on one tile):\n";
        for (const auto& [d, t] : byDay)
        {
            std::cout << "    day " << d + 1 << ":";
            for (const double ms : stagesByDay[d])
                std::cout << " " << ms / t[4];
            const auto& c = crowdByDay[d];
            std::cout << "; " << double(searchesByDay[d]) / t[4] << "; " << c[0] << " " << c[1] << " " << c[2] << " " << c[3] << "\n";
        }
        std::cout << "  each day's biggest crowd on one tile:\n";
        for (const auto& [d, where] : biggestCrowd)
            std::cout << "    day " << d + 1 << ": " << where << "\n";
    }
    if (!eventsFile.empty())
    {
        // For reading chronicles offline (tools/chronicle.py --events): the events as the database would get them.
        for (auto& e : server.takeEvents())
            events.push_back(std::move(e));
        std::ofstream out(eventsFile);
        out << "{\"events\": " << ratw::eventsJson(events) << ", \"names\": {";
        bool first = true;
        for (const auto& [id, e] : server.entities())
        {
            std::string name;
            for (const char ch : e.name)
                if (ch == '"' || ch == '\\') name += std::string("\\") + ch;
                else if (static_cast<unsigned char>(ch) >= 0x20) name += ch;
            out << (first ? "" : ", ") << '"' << id << "\": \"" << name << '"';
            first = false;
        }
        out << "}}\n";
        std::cout << "  events: " << events.size() << " written to " << eventsFile << '\n';
    }
    {
        // The people (births follow deaths: doc 68, Progress): who lives, who married, children born and arrivals asked
        // for. (With --no-births nobody makes them: they wait as requests.)
        int living = 0, oldest = 0;
        for (const auto& [id, e] : server.entities())
            if (e.npc && !e.dead && server.society().resident(id))
                ++living, oldest = std::max(oldest, e.age);
        int asked = 0, askedArrivals = 0, died = 0;
        for (const auto& r : server.takeResidentRequests())
            (r.kind == "birth" ? asked : askedArrivals) += 1;
        for (const auto& [id, e] : server.entities())
            died += e.npc && e.dead && server.society().resident(id) && !deadAtStart.count(id);
        std::cout << "  people: " << living << " residents living (the oldest " << oldest << "), "
                  << server.society().state().careers.spouses.size() / 2 << " married couples; " << madeBirths << " born, "
                  << madeArrivals << " arrived from the Country and " << died << " died in the run" << (births ? "" : " (--no-births)") << "; "
                  << asked << " children and " << askedArrivals << " arrivals asked for and not made; "
                  << server.society().state().careers.birthsOwed.size() << " children still owed for the dead; " << server.society().unplacedCount()
                  << " unplaced NPCs left in the world's list\n";
        // (By their old trade, of working age: doc 72, Phase 5's pool.)
        std::map<std::string, int> pool;
        for (const auto& u : server.society().authored().unplaced)
            if (u.age >= 16 && u.age < 65)
                ++pool[items::farmPeople().tradeOf(u.workLabel).empty() ? std::string("other") : items::farmPeople().tradeOf(u.workLabel)];
        std::cout << "  the unplaced of working age by old trade:";
        for (const auto& [trade, n] : pool)
            std::cout << ' ' << trade << ' ' << n << ';';
        std::cout << '\n';
    }
    if (const auto& placed = server.society().farmPlaced(); !placed.empty())
    {
        // The farms' people (doc 71, Phase C): who was placed, the farms' posts, and the households in the realm's homes.
        const auto& society = server.society();
        std::cout << "  the farms' people placed:";
        for (const auto& [what, n] : placed)
            std::cout << ' ' << what << ' ' << n << ';';
        int posts = 0, held = 0;
        for (const auto& f : society.farms())
            posts += f.postKeepers + f.postHands, held += f.heldKeepers + f.heldHands;
        std::map<std::string, int> homes;
        int hungry = 0, living = 0;
        double hungriest = 0;
        for (const auto& [id, life] : society.state().residents)
            if (const auto* e = server.entity(id); e && !e->dead && (life.homeCell.rfind("fs_", 0) == 0 || life.homeCell.rfind("cn_", 0) == 0))
            {
                ++homes[life.homeCell], ++living;
                hungry += life.hunger >= 60;
                hungriest = std::max(hungriest, life.hunger);
            }
        std::map<int, int> sizes;
        for (const auto& [home, n] : homes)
            ++sizes[n];
        std::cout << " posts " << posts << " (" << held << " held); " << living
                  << " living in the realm's homes, " << homes.size() << " households (by size:";
        for (const auto& [n, count] : sizes)
            std::cout << ' ' << n << 'x' << count;
        std::cout << "), " << hungry << " hungry (60+) at the end, the hungriest " << hungriest << '\n';
        // (RATW_FARM_PEOPLE=1: each of them, its home, its work and its post.)
        if (const char* each = std::getenv("RATW_FARM_PEOPLE"); each && *each == '1')
            for (const auto& [id, life] : society.state().residents)
                if (const auto* r = society.spec(id); r && (life.homeCell.rfind("fs_", 0) == 0 || life.homeCell.rfind("cn_", 0) == 0 ||
                                                            r->work.cell.rfind("fs_", 0) == 0 || r->work.cell.rfind("cn_", 0) == 0))
                    std::cout << "    farm person " << id << " | " << r->age << " | " << life.homeCell << " | " << r->work.cell << " | "
                              << r->workLabel << " | " << society.cash(id) << "p | hunger " << life.hunger << " | now " << (server.entity(id) ? server.entity(id)->cellId : std::string("-")) << " " << life.task << '\n';
        for (const auto& p : society.positions())
            if (p.role == "merchant" && p.work.cell.rfind("cn_", 0) == 0)
            {
                const auto till = "till:" + p.id;
                int stock = 0;
                if (const auto* a = society.account(till))
                    for (const auto& [item, n] : a->stock)
                        stock += n;
                const auto& ps = society.state().careers.positions.at(p.id);
                std::cout << "    " << p.title << " (" << ps.holder << "): till " << society.cash(till) << "p, " << stock << " pieces\n";
            }
    }
    if (sampling)
        roads.report();
    std::cout << "  at their scheduled place: " << arrived << ", on their way: " << underway << ", without a route: "
              << lost.size() << '\n';
    for (const auto& who : lost)
        std::cout << "    " << who << '\n';
    if (sampling)
    {
        std::size_t placed = 0;
        for (const auto& [id, e] : server.entities())
            placed += e.npc && server.society().resident(id) != nullptr;
        std::cout << "  held more than 10 game minutes on one tile while bound elsewhere: " << heldOnce.size() << " of " << placed
                  << " residents (" << (placed ? 100.0 * double(heldOnce.size()) / double(placed) : 0.0) << "%)\n";
        for (std::size_t k = 0; k < heldOnce.size() && k < 40; ++k)
            std::cout << "    " << heldOnce[k] << '\n';
        if (heldOnce.size() > 40)
            std::cout << "    ... and " << heldOnce.size() - 40 << " more\n";
    }
    std::sort(motions.begin(), motions.end());
    double motionTotal = 0;
    for (double m : motions)
        motionTotal += m;
    if (!motions.empty())
        std::cout << "  a player's motion frame (every tick): mean " << motionTotal / motions.size() << " ms, worst "
                  << motions.back() << " ms\n";
    std::sort(views.begin(), views.end());
    double viewTotal = 0;
    for (double v : views)
        viewTotal += v;
    if (!views.empty())
        std::cout << "  a player's view (5 a second): mean " << viewTotal / views.size() << " ms, worst " << views.back()
                  << " ms\n";
    // A fingerprint of where everyone ended up, what they were doing, and what each player has seen of the world: an
    // optimisation that changes no behaviour leaves it the same, so compare it before and after.
    std::uint64_t digest = 1469598103934665603ULL;
    const auto mix = [&](const void* data, std::size_t size) {
        for (std::size_t b = 0; b < size; ++b)
            digest = (digest ^ static_cast<const unsigned char*>(data)[b]) * 1099511628211ULL;
    };
    const auto mixText = [&](const std::string& text) { mix(text.data(), text.size() + 1); };
    for (const auto& [id, e] : server.entities())
    {
        mixText(id);
        mixText(e.cellId);
        mix(&e.position.x, sizeof e.position.x);
        mix(&e.position.y, sizeof e.position.y);
        mixText(e.activity);
        mixText(e.posture);
        if (const auto* life = server.society().resident(id))
            mixText(life->task);
    }
    const auto people = digest;
    // And the society's state (doc 69: the people digest above can't tell apart runs whose economies differ): every
    // account's cash and goods, every life's needs, work and goal, the posts' holders, and the legs under way.
    digest = 1469598103934665603ULL;
    for (const auto& [id, account] : server.society().state().accounts)
    {
        mixText(id);
        mix(&account.cash, sizeof account.cash);
        for (const auto& [item, n] : account.stock)
        {
            mixText(item);
            mix(&n, sizeof n);
        }
    }
    for (const auto& [id, life] : server.society().state().residents)
    {
        mixText(id);
        for (const double v : {life.hunger, life.fatigue, life.progress, life.goalX, life.goalY, life.homeX, life.homeY})
            mix(&v, sizeof v);
        mixText(life.task);
        mixText(life.reason);
        mixText(life.goalCell);
        mixText(life.homeCell);
    }
    for (const auto& [id, post] : server.society().state().careers.positions)
    {
        mixText(id);
        mixText(post.holder);
        mixText(post.apprentice);
    }
    const std::uint64_t legs = server.legsDigest();
    mix(&legs, sizeof legs);
    const auto societyDigest = digest;
    digest = 1469598103934665603ULL;
    std::size_t remembered = 0;
    for (const auto& p : players)
        for (const auto& [cellId, memory] : server.memories(p))
        {
            mixText(cellId);
            mix(memory.glyphs.data(), memory.glyphs.size());
            for (const bool seen : memory.observed)
            {
                mix(&seen, 1);
                remembered += seen;
            }
        }
    std::cout << "  digest: people " << std::hex << people << ", society " << societyDigest << ", map memory " << digest << std::dec
              << " (" << remembered << " tiles remembered)\n";
    std::sort(steady.begin(), steady.end());
    if (!steady.empty())
        std::cout << "  without loading a place: p99 " << steady[std::min(steady.size() - 1, std::size_t(.99 * steady.size()))]
                  << " ms, p99.9 " << steady[std::min(steady.size() - 1, std::size_t(.999 * steady.size()))]
                  << " ms, worst " << steady.back() << " ms (" << times.size() - steady.size()
                  << " ticks loaded places)\n";
    return 0;
}
