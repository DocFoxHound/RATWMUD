// Loads a streamed world export (python3 tools/world_build.py export DIR writes DEV's newest build) the way the game
// server does, then brings every cell into memory so the server's own checks run on all of it.
//
//   world_check EXPORT_DIR [--simulate FROM_HOUR TO_HOUR [--players N] [--no-check] [--full] [--events FILE] [--reckon]]
//
// --reckon ends the run with the month's reckoning (doc 42), as if the month were up, and says where the money went.
// --simulate then runs the world the way the game server does (20 ticks a second) between two hours of the day and
// reports how long ticks take, so a region's population can be checked against the server's 50 ms tick budget.
// --players N (default 1) adds walking players: the first at the spawn, the rest beside residents spread over the
// world, each observed every tick and sent a view five times a second as the server does. --no-check skips loading
// every place first (the check), which is slow and not needed to measure ticks. --full turns the simulation tiers
// off, simulating every NPC in full wherever it is (as before tiers), for comparison.
#include "RatwWorld.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <tuple>
#include <sstream>
#include <string>

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
// Where the money is (doc 42): each kind of holder's total, how many hold it, and the median of the people among them.
void moneyReport(const World& server, const char* when)
{
    struct Pile
    {
        std::int64_t total = 0;
        std::vector<std::int64_t> each;
    };
    std::map<std::string, Pile> piles;
    const auto& society = server.society();
    for (const auto& [id, account] : society.state().accounts)
    {
        std::string kind;
        if (id == "treasury")
            kind = "capital's treasury";
        else if (id.rfind("stores:", 0) == 0)
            kind = "town treasuries";
        else if (id.rfind("town:", 0) == 0)
            kind = "town " + id.substr(id.rfind(':') + 1);
        else if (id.rfind("home:", 0) == 0)
            kind = "home stores";
        else if (playerAccountId(id))
            kind = "players";
        else if (facilityAccount(id))
            kind = id.substr(0, id.find(':'));
        else if (const auto* spec = society.spec(id))
        {
            const auto* job = society.jobOf(id);
            const auto* e = server.entity(id);
            kind = job ? (job->role == "merchant" ? "merchants" : job->role == "guard" ? "guards"
                          : job->paid ? "paid civilians" : "unpaid civilians")
                 : society.apprenticedTo(id) ? "apprentices"
                 : e && e->age < 16          ? "children"
                                             : "adults without work";
            (void)spec;
        }
        else
            kind = "other";
        auto& pile = piles[kind];
        pile.total += account.cash;
        pile.each.push_back(account.cash);
    }
    int short_ = 0;
    for (const auto& [id, life] : society.state().residents)
        if (const auto* a = society.account(id); a && a->cash < 6)
            ++short_;
    std::cout << "  money " << when << " (" << society.moneySupply() << "p in all; " << short_
              << " residents short of a day's food money): holder, total, how many, median\n";
    for (auto& [kind, pile] : piles)
    {
        std::sort(pile.each.begin(), pile.each.end());
        std::cout << "    " << kind << ": " << pile.total << "p, " << pile.each.size() << ", " << pile.each[pile.each.size() / 2]
                  << "p\n";
    }
}
} // namespace

int main(int argc, char** argv)
{
    const bool simulate = argc >= 5 && std::string(argv[2]) == "--simulate";
    int playerCount = 1;
    bool check = true;
    bool full = false;                              // --full: every NPC in full simulation (no tiers).
    bool reckon = false;                            // --reckon: the month's reckoning at the end.
    std::string eventsFile;                         // --events FILE: what happened, and everyone's names, as JSON.
    bool usage = argc != 2 && !simulate;
    for (int i = 5; simulate && i < argc; ++i)
    {
        const std::string flag = argv[i];
        if (flag == "--players" && i + 1 < argc)
            playerCount = std::max(1, std::atoi(argv[++i]));
        else if (flag == "--no-check")
            check = false;
        else if (flag == "--reckon")
            reckon = true;
        else if (flag == "--full")
            full = true;
        else if (flag == "--events" && i + 1 < argc)
            eventsFile = argv[++i];
        else
            usage = true;
    }
    if (usage)
    {
        std::cerr << "usage: world_check EXPORT_DIR [--simulate FROM_HOUR TO_HOUR [--players N] [--no-check] [--full] [--events FILE] [--reckon]]\n";
        return 2;
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
    moneyReport(server, "at the start");
    if (full)
        server.setTiered(false);
    const double from = std::atof(argv[3]), to = std::atof(argv[4]);
    server.setTimeOfDay(from);
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
    std::vector<double> views, motions;
    const auto npcs = [&] {
        std::size_t n = 0;
        for (const auto& [id, e] : server.entities())
            n += e.npc;
        return n;
    };
    std::map<std::string, std::string> start;
    for (const auto& [id, e] : server.entities())
        if (e.npc)
            start[id] = e.cellId + " " + std::to_string(int(e.position.x)) + "," + std::to_string(int(e.position.y));
    const int ticks = int((to - from) * 600.0 / 0.05);      // A game hour is 600 real seconds; the server ticks at 20 Hz.
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
    times.reserve(std::size_t(ticks));
    std::size_t mostLoaded = 0;
    for (int i = 0; i < ticks; ++i)
    {
        const auto begin = std::chrono::steady_clock::now();
        const auto loadedBefore = server.loadedCells();
        if (i % 300 == 0)                          // Walk east and west, as a player would.
            for (const auto& p : players)
                server.move(p, (i / 300) % 2 ? -1 : 1, 0);
        const auto profileBefore = server.tickProfile();
        server.tick(0.05);
        const auto& profileAfter = server.tickProfile();
        if (i % 100 == 0 && !std::getenv("RATW_NO_AMBIENT_LOOK"))
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
        auto& day = byDay[int(std::floor((from + i * 0.05 / 600.0) / 24.0))];
        day[0] += tickOnly;
        day[1] += profileAfter.schedules.total - profileBefore.schedules.total;
        day[2] += profileAfter.movement.total - profileBefore.movement.total;
        day[3] += profileAfter.separation.total - profileBefore.separation.total;
        day[4] += 1;
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
        if (std::getenv("RATW_STREAM_TRACE") && i % 1200 == 0)
            std::cerr << "STREAM t=" << i * 0.05 << " loaded=" << server.loadedCells() << " offstage="
                      << server.offstageCount() << "\n";
        if (const char* trace = std::getenv("RATW_TRACE"); trace && i % 200 == 0)
            if (const auto* e = server.entity(trace))
                std::cerr << "TRACE t=" << i * 0.05 << " " << e->cellId << " (" << e->position.x << "," << e->position.y
                          << ") path=" << e->path.size() << " state=" << e->state << " activity=" << e->activity << "\n";
        times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count());
        if (server.loadedCells() <= loadedBefore)
            steady.push_back(times.back());
        mostLoaded = std::max(mostLoaded, server.loadedCells());
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
    std::cout << npcs() << " residents, " << players.size() << " player" << (players.size() == 1 ? "" : "s") << ", " << ticks << " ticks from " << from << ":00 to " << to << ":00 ("
              << total / 1000.0 << " s of work): mean " << total / ticks << " ms, p99 " << pct(.99) << " ms, p99.9 "
              << pct(.999) << " ms, worst " << sorted.back() << " ms; " << moved << " residents moved; at most "
              << mostLoaded << " places in memory\n";
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
    std::cout << "  " << server.offstageCount() << " residents offstage at the end; money "
              << (server.society().conserved() ? "conserved" : "NOT CONSERVED") << "; " << server.loadedCells()
              << " places in memory at the end; " << server.bonds().count() << " bonds between them\n";
    moneyReport(server, "at the end");
    if (reckon)
    {
        std::cout << "  " << server.reckonNow() << "\n";
        moneyReport(server, "after the reckoning");
    }
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
    std::cout << "  at their scheduled place: " << arrived << ", on their way: " << underway << ", without a route: "
              << lost.size() << '\n';
    for (const auto& who : lost)
        std::cout << "    " << who << '\n';
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
    std::cout << "  digest: people " << std::hex << people << ", map memory " << digest << std::dec << " ("
              << remembered << " tiles remembered)\n";
    std::sort(steady.begin(), steady.end());
    if (!steady.empty())
        std::cout << "  without loading a place: p99 " << steady[std::min(steady.size() - 1, std::size_t(.99 * steady.size()))]
                  << " ms, p99.9 " << steady[std::min(steady.size() - 1, std::size_t(.999 * steady.size()))]
                  << " ms, worst " << steady.back() << " ms (" << times.size() - steady.size()
                  << " ticks loaded places)\n";
    return 0;
}
