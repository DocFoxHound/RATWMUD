// talk_bench (Docs/Design/84-living-conversations.md §16, budget B1; Phase 4): how long the ambient director takes, one
// beat at a time, on its own (no other runs beside it). It builds a scratch game as talk_sim does (nothing is written),
// puts listeners where the most residents stand free to talk, freezes the world there, and runs the game's own director
// (Game::ambient) for the hours asked, timing each beat (its facts, an interruption looked for, a joiner, the engine's
// choice and rendering) and each look for a new talk (the pick, its query, its first line). Then it says the mean and
// the 99th percentile and checks them against B1: a beat's mean 0.6 ms or less and p99 3 ms, a look's p99 5 ms.
//
//   talk_bench [--world KEY|EXPORT_DIR] [--checkpoint FILE] [--warm GAME_HOURS] [--hours H] [--listeners N] [--no-check]
//
// The default world is the synthetic talk town (Tests/fixtures/talk_town, warmed two game hours): the ctest `talk_bench`
// (label perf, added with -DRATW_PERF_TESTS=ON). Ridgemere 3 (--world ridgemere3, RATW_DATABASE_URL or the world's
// game role) is the budget's real measure: run it by hand at a phase's end.
#include "RatwCalendar.h"
#include "RatwGame.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <unistd.h>
#include <vector>

using namespace ratw;
namespace fs = std::filesystem;

namespace
{
constexpr double HourSeconds = calendar::SecondsPerDay / 24;

std::string run(const std::string& command)
{
    std::string out;
    if (FILE* p = ::popen(command.c_str(), "r"))
    {
        char buffer[4096];
        for (std::size_t n; (n = std::fread(buffer, 1, sizeof buffer, p)) > 0;)
            out.append(buffer, n);
        if (::pclose(p) != 0)
            return "";
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out;
}

struct Stats
{
    std::size_t n = 0;
    double mean = 0, p50 = 0, p99 = 0, max = 0;
};
Stats stats(std::vector<double> xs)
{
    Stats s;
    s.n = xs.size();
    if (xs.empty())
        return s;
    std::sort(xs.begin(), xs.end());
    double sum = 0;
    for (const double x : xs)
        sum += x;
    s.mean = sum / double(xs.size());
    s.p50 = xs[xs.size() / 2];
    s.p99 = xs[std::min(xs.size() - 1, std::size_t(double(xs.size()) * .99))];
    s.max = xs.back();
    return s;
}

int usage()
{
    std::cerr << "usage: talk_bench [--world KEY|EXPORT_DIR] [--checkpoint FILE] [--warm GAME_HOURS] [--hours H] [--listeners N]\n"
                 "                  [--no-check]\n";
    return 2;
}
} // namespace

int main(int argc, char** argv)
{
    std::string world = RATW_SOURCE_DIR "/Tests/fixtures/talk_town", checkpoint;
    double warm = -1, hours = 6;
    int listeners = 4;
    bool check = true;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        const auto value = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--world")
            world = value();
        else if (a == "--checkpoint")
            checkpoint = value();
        else if (a == "--warm")
            warm = std::atof(value().c_str());
        else if (a == "--hours")
            hours = std::atof(value().c_str());
        else if (a == "--listeners")
            listeners = std::max(1, std::atoi(value().c_str()));
        else if (a == "--no-check")
            check = false;
        else
            return usage();
    }
    game::Options o;
    o.talkData = RATW_SOURCE_DIR "/Data/Talk";
    o.voiceData = RATW_SOURCE_DIR "/Data/Voice";
    o.scratch = true;
    o.forkSnapshots = false;
    o.savePath = (fs::temp_directory_path() / ("ratw-talk-bench-" + std::to_string(::getpid()) + ".json")).string();
    o.checkpointFile = checkpoint;
    const bool exported = fs::is_directory(world);
    if (exported)
        o.worldExport = world;
    else
    {
        const auto key = run("python3 '" RATW_SOURCE_DIR "/tools/world_db.py' resolve '" + world + "'");
        if (key.empty())
        {
            std::cerr << "talk_bench: '" << world << "' is neither an export directory nor a registered world\n";
            return 1;
        }
        o.database = key;
        if (const char* url = std::getenv("RATW_DATABASE_URL"))
            o.conninfo = url;
        else
            o.conninfo = run("python3 '" RATW_SOURCE_DIR "/tools/world_db.py' conninfo " + key + " --role game");
    }
    game::Game g(o);
    g.log = [](const char* level, const std::string& text) {
        if (std::string(level) != "info")
            std::cerr << "[" << level << "] " << text << "\n";
    };
    std::string problem;
    if (!g.start(problem))
    {
        std::cerr << "talk_bench: " << problem << "\n";
        return 1;
    }
    std::error_code ec;
    fs::remove(o.savePath, ec);
    auto& w = g.world();
    // A synthetic town wakes at home: run it a little first (the ctest's two game hours), then freeze it where it stands.
    if (warm < 0)
        warm = exported ? 2 : 0;
    for (long i = 0, n = long(warm * HourSeconds / .05); i < n; ++i)
        g.tick(.05);
    // Where the most residents stand free to talk (on stage or not: a listener brings them on), not in anyone's home.
    static const std::set<std::string> Occupied{"sleep", "patrol", "companion", "relocate", "held in the gaol",
                                                "looking for a chance", "guarding a caravan", "carrying a letter"};
    std::map<std::string, std::pair<int, Vec2>> crowds;
    for (const auto& [id, e] : w.entities())
    {
        if (!e.npc || e.dead || e.transient || !e.path.empty() || std::hypot(e.velocity.x, e.velocity.y) > .05)
            continue;
        const auto* life = w.society().resident(id);
        if (!life || Occupied.count(life->task) || w.placeKindOf(e.cellId) == "home" || e.cellId == life->homeCell)
            continue;
        auto& c = crowds[e.cellId];
        ++c.first;
        c.second.x += e.position.x;
        c.second.y += e.position.y;
    }
    std::vector<std::pair<int, std::string>> order;
    for (const auto& [cell, c] : crowds)
        if (c.first >= 2)
            order.push_back({-c.first, cell});
    std::sort(order.begin(), order.end());
    std::vector<std::string> ids;
    for (std::size_t i = 0; i < order.size() && int(ids.size()) < listeners; ++i)
    {
        const auto& cell = order[i].second;
        const auto& c = crowds[cell];
        const Vec2 at{c.second.x / c.first, c.second.y / c.first};
        const auto id = "bench_" + std::to_string(i + 1);
        w.addPlayer(id, "Listener " + std::to_string(i + 1), cell, at);
        ids.push_back(id);
    }
    if (ids.empty())
    {
        std::cerr << "talk_bench: nowhere two or more residents stand free to talk\n";
        return 1;
    }
    // Who is near a listener is on stage; everyone stands where they are (frozen).
    w.restage();
    std::vector<std::tuple<std::string, std::string, Vec2, bool>> all;
    for (const auto& [id, e] : w.entities())
        if (e.npc && !e.transient)
            all.push_back({id, e.cellId, e.position, e.offstage});
    for (const auto& [id, cell, at, off] : all)
        w.replayPlace(id, cell, at, false, "", off);
    g.setTalkListeners(ids);
    auto timing = std::make_shared<game::Game::TalkTiming>();
    g.setTalkTiming(timing);
    long exchanges = 0;
    g.onAmbientLine = [&](const game::Game::AmbientLine& l) { exchanges += l.beat == 0; };
    const double until = w.time() + hours * 3600;
    const auto wall = std::chrono::steady_clock::now();
    while (w.time() < until)
    {
        w.advanceClock(.25);
        g.ambient(.25);
    }
    const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall).count();
    const auto beat = stats(timing->beats), look = stats(timing->looks);
    std::printf("talk_bench: %s, %zu listeners, %.1f world hours, %ld talks, %.1f s wall\n", world.c_str(), ids.size(), hours,
                exchanges, took);
    std::printf("  beat (B1: mean 0.6 ms, p99 3 ms):  n %zu, mean %.3f ms, p50 %.3f, p99 %.3f, max %.3f\n", beat.n, beat.mean,
                beat.p50, beat.p99, beat.max);
    std::printf("  look (B1: p99 5 ms):               n %zu, mean %.3f ms, p50 %.3f, p99 %.3f, max %.3f\n", look.n, look.mean,
                look.p50, look.p99, look.max);
    if (beat.n == 0)
    {
        std::cerr << "talk_bench: no beat was timed (nobody talked)\n";
        return 1;
    }
    const bool within = beat.mean <= .6 && beat.p99 <= 3 && look.p99 <= 5;
    std::printf("  B1 %s\n", within ? "within budget" : "OVER BUDGET");
    return check && !within ? 1 : 0;
}
