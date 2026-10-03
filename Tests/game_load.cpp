// The load test (Docs/Design/31-responsiveness.md, Phase 1): the whole game, as the server runs it, with N players.
//
//   game_load EXPORT_DIR [--players N] [--layout cities|spread] [--seconds S] [--warmup S] [--walking server|client]
//             [--workers N]
//
// EXPORT_DIR is a world build written out as files (python3 tools/world_build.py export DIR). Each player is a fake
// client that does what the browser does: it enters with a development identity, walks (a new direction every two
// seconds), and acknowledges every snapshot, so snapshots are the deltas a real client gets. Every message it is sent
// is compressed exactly as the server compresses it. Nothing goes over a socket: the sockets are the server's own
// part (its RATW_PERF lines report them).
//
//   --layout cities   (the default, and the gate) players packed beside residents in the three most peopled regions
//   --layout spread   players beside residents taken evenly through the whole population
//   --workers N       threads finishing snapshots in parallel, as the server has (default: the cores less two; 0: none)
//   --walking client  each player walks its own wolf (doc 31, Phase 3), sending poses twenty times a second as the page
//                     does, and taking the server's correction when one is refused; "server" (the default) sends keys
//
// It prints the server's RATW_PERF lines for the measured window, then the cost of a full save. Not a ctest: a run
// takes minutes. The gates are in the design doc.
#include "RatwGame.h"
#include "RatwLink.h"
#include "RatwPack.h"
#include "RatwMotionCore.h"
#include "RatwPerf.h"
#include "RatwSystemLibs.h"

#include <algorithm>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

using namespace ratw;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace
{
perf::Meter meter;
// Counted from the snapshot workers' threads too.
std::atomic<std::size_t> snapshotBytes{0}, snapshots{0}, motionBytes{0}, motions{0};

// A browser, as far as the server's work for it goes.
struct Player final : game::Connection
{
    double revision = -1;
    std::uint64_t bytes = 0;
    std::size_t corrected = 0;
    bool wasCorrected = false;
    double cx = 0, cy = 0;

    void event(const std::string& json) override
    {
        pack(json.data(), json.size());
        if (json.find("\"type\":\"correction\"") == std::string::npos)
            return;
        json::Value v;
        std::string error;
        if (json::parse(json, v, error))
        {
            ++corrected;
            wasCorrected = true;
            cx = v.number("x");
            cy = v.number("y");
        }
    }
    void snapshotValue(const json::Value& root) override
    {
        // As the server does (doc 31, Phase 4.8).
        const auto packed = pack::encode(root);
        snapshotBytes += packed.size();
        ++snapshots;
        pack(packed.data(), packed.size());
        if (const auto* r = root.find("revision"))
            revision = r->asNumber();
    }
    void snapshot(const std::string& json) override
    {
        snapshotBytes += json.size();
        ++snapshots;
        pack(json.data(), json.size());
        // What the client acknowledges: the snapshot's revision (read without parsing the whole of it).
        if (const auto at = json.find("\"revision\":"); at != std::string::npos)
            revision = std::atof(json.c_str() + at + 11);
    }
    void motion(const json::Value& frame) override
    {
        const auto raw = motion::pack(frame);
        motionBytes += raw.size();
        ++motions;
        pack(reinterpret_cast<const char*>(raw.data()), raw.size());
    }
    bool allowsLocalCredentials() const override { return true; }

  private:
    void pack(const char* data, std::size_t size)
    {
        std::string payload;
        {
            perf::Scope timed(&meter, perf::Compression);
            link::encode(link::Event, std::string(data, size), payload);
        }
        bytes += payload.size();
        meter.sent(payload.size());
    }
};

std::string command(std::initializer_list<std::pair<const char*, json::Value>> fields)
{
    auto o = json::Value::object();
    for (const auto& [k, v] : fields)
        o.add(k, v);
    return json::dump(o);
}

int usage()
{
    std::cerr << "usage: game_load EXPORT_DIR [--players N] [--layout cities|spread] [--seconds S] [--warmup S] [--walking server|client]\n";
    return 2;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
        return usage();
    int players = 20;
    double seconds = 30, warmup = 20;
    std::string layout = "cities", walking = "server";
    int workers = -1;
    for (int i = 2; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (i + 1 >= argc)
            return usage();
        if (a == "--players")
            players = std::max(1, std::atoi(argv[++i]));
        else if (a == "--layout")
            layout = argv[++i];
        else if (a == "--seconds")
            seconds = std::max(1.0, std::atof(argv[++i]));
        else if (a == "--warmup")
            warmup = std::max(0.0, std::atof(argv[++i]));
        else if (a == "--walking")
            walking = argv[++i];
        else if (a == "--workers")
            workers = std::atoi(argv[++i]);
        else
            return usage();
    }
    if ((layout != "cities" && layout != "spread") || (walking != "server" && walking != "client"))
        return usage();
    std::string problem;
    if (!sys::zlibAvailable(problem))
    {
        std::cerr << problem << '\n';
        return 1;
    }

    // A private, disposable save (and its portraits folder), as the file worlds have.
    const fs::path save = fs::temp_directory_path() / ("game_load-" + std::to_string(::getpid()) + ".json");
    game::Options options;
    options.worldExport = argv[1];
    options.savePath = save.string();
    options.devIdentity = true;
    options.requireStorage = false;
    options.workerThreads = workers >= 0 ? unsigned(workers) : unsigned(std::clamp(int(std::thread::hardware_concurrency()) - 2, 0, 16));
    auto cleanUp = [&] {
        std::error_code ignored;
        fs::remove(save, ignored);
        fs::remove(save.string() + ".tmp", ignored);
        fs::remove_all(save.string() + ".art", ignored);
    };
    int status = 0;
    {
        game::Game g(options);
        g.log = [](const char* level, const std::string& text) {
            // (The fake players walk blindly into walls with --walking client: the server's refusals are expected.)
            if (std::string(level) != "info" && text.rfind("RATW_POSE ", 0) != 0)
                std::cerr << level << ": " << text << '\n';
        };
        if (!g.start(problem))
        {
            std::cerr << "The world did not load: " << problem << '\n';
            cleanUp();
            return 1;
        }
        g.setMeter(&meter);

        // Where the players stand: beside residents, in the busiest regions or all over.
        std::vector<std::string> residents;
        std::map<std::string, std::vector<std::string>> byRegion;
        for (const auto& [id, e] : g.world().entities())
            if (e.npc)
            {
                residents.push_back(id);
                if (const auto* cell = g.world().cell(e.cellId))
                    byRegion[cell->region].push_back(id);
            }
        std::vector<std::string> beside;
        if (layout == "spread")
            beside = residents;
        else
        {
            std::vector<std::pair<std::size_t, std::string>> regions;
            for (const auto& [region, people] : byRegion)
                regions.push_back({people.size(), region});
            std::sort(regions.rbegin(), regions.rend());
            std::cout << "cities:";
            for (std::size_t r = 0; r < std::min<std::size_t>(3, regions.size()); ++r)
            {
                std::cout << ' ' << regions[r].second << " (" << regions[r].first << " residents)";
                // The players are shared among the three in proportion to their people.
                const auto& people = byRegion[regions[r].second];
                beside.insert(beside.end(), people.begin(), people.end());
            }
            std::cout << '\n';
        }
        if (beside.empty())
        {
            std::cerr << "The world has no residents to stand beside.\n";
            cleanUp();
            return 1;
        }

        std::vector<std::unique_ptr<Player>> clients;
        for (int p = 0; p < players; ++p)
        {
            auto c = std::make_unique<Player>();
            c->id = std::uint64_t(p + 1);
            g.connect(c.get());
            const std::string id = "load-" + std::to_string(p);
            g.command(c.get(), command({{"type", "hello"}, {"id", id}, {"name", "Load " + std::to_string(p)}}));
            if (c->entityId.empty())
            {
                std::cerr << "Player " << id << " did not enter the world.\n";
                cleanUp();
                return 1;
            }
            const auto& near = beside[std::size_t(p) * beside.size() / std::size_t(players)];
            if (const auto* npc = g.world().entity(near))
                if (auto* me = g.world().entity(c->entityId))
                {
                    me->cellId = npc->cellId;
                    me->position = npc->position;
                }
            clients.push_back(std::move(c));
        }
        std::vector<std::uint32_t> poseSeq(clients.size(), 0);
        if (walking == "client")
            for (auto& c : clients)
                g.command(c.get(), command({{"type", "walking"}, {"mode", "client"}}));

        // Twenty ticks a second, as fast as they run: each tick, the commands due, the tick, and the acknowledgements.
        const auto run = [&](int ticks, bool measured) {
            for (int i = 0; i < ticks; ++i)
            {
                const auto begin = Clock::now();
                {
                    perf::Scope timed(&meter, perf::Commands);
                    for (std::size_t p = 0; p < clients.size(); ++p)
                    {
                        const int turn = (i / 40 + int(p)) % 4;
                        const double x = turn == 0 ? 1 : turn == 2 ? -1 : 0, y = turn == 1 ? 1 : turn == 3 ? -1 : 0;
                        auto& c = *clients[p];
                        if (walking == "server")
                        {
                            if (i % 40 == 0)
                                g.command(&c, command({{"type", "move"}, {"x", x}, {"y", y}}));
                            continue;
                        }
                        // The page's own walking, roughly: a walk's worth each tick in the same direction, back to the
                        // server's word when it refuses one.
                        const auto* me = g.world().entity(c.entityId);
                        if (!me)
                            continue;
                        double px = me->position.x, py = me->position.y;
                        if (c.wasCorrected)
                        {
                            px = c.cx;
                            py = c.cy;
                            c.wasCorrected = false;
                        }
                        g.pose(&c, ++poseSeq[p], float(px + x * .12), float(py + y * .12), 0, x, y);   // (link::Pose, as the server reads it.)
                    }
                }
                g.tick(0.05);
                {
                    perf::Scope timed(&meter, perf::Commands);
                    for (auto& c : clients)
                        if (c->revision >= 0)
                            g.acknowledge(c.get(), c->revision, false);
                }
                if (measured)
                    meter.pass(std::chrono::duration<double, std::milli>(Clock::now() - begin).count(), true);
            }
        };
        const auto start = Clock::now();
        run(int(warmup / 0.05), false);
        std::cout << "warmed up in " << std::chrono::duration<double>(Clock::now() - start).count() << " s ("
                  << g.world().loadedCells() << " places loaded)\n";
        meter.take();
        g.world().resetTickProfile();
        snapshotBytes = 0;
        snapshots = 0;
        motionBytes = 0;
        motions = 0;
        run(int(seconds / 0.05), true);

        // The window as the server would log it. Its rates are per second of game time, not of the run's own time.
        auto window = meter.take();
        window.began = Clock::now() - std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
        std::cout << players << " players (" << layout << "), " << window.ticks << " ticks measured\n";
        for (const auto& line : perf::report(window, clients.size()))
            std::cout << line << '\n';
        std::cout << perf::worldLine(g.world().tickProfile()) << '\n';
        std::uint64_t most = 0, all = 0;
        for (const auto& c : clients)
        {
            most = std::max(most, c->bytes);
            all += c->bytes;
        }
        if (walking == "client")
        {
            std::size_t corrections = 0;
            for (const auto& c : clients)
                corrections += c->corrected;
            std::cout << "poses refused: " << corrections << " of " << clients.size() * std::size_t(seconds / 0.05) << "\n";
        }
        std::cout << "per player: " << perf::fixed(double(all) * 8 / seconds / 1e3 / double(clients.size())) << " kbit/s (most "
                  << perf::fixed(double(most) * 8 / seconds / 1e3) << "); snapshot "
                  << (snapshots ? snapshotBytes.load() / snapshots.load() : 0) << " B raw, motion frame "
                  << (motions ? motionBytes.load() / motions.load() : 0)
                  << " B raw\n";

        // A full save, waited for, as the server makes one when it stops (file mode: the whole document on this thread).
        // Play itself is saved by forked snapshots and the journal (doc 31, Phase 2): their cost is under "saves" above.
        std::vector<double> saves;
        for (int i = 0; i < 3; ++i)
        {
            const auto begin = Clock::now();
            g.save();
            saves.push_back(std::chrono::duration<double, std::milli>(Clock::now() - begin).count());
        }
        std::error_code sizeError;
        const auto size = fs::file_size(save, sizeError);
        std::cout << "full save: " << perf::fixed(saves[0]) << ", " << perf::fixed(saves[1]) << ", " << perf::fixed(saves[2])
                  << " ms (" << (sizeError ? 0 : size / 1024) << " KB)\n";
        if (!g.storageReady())
            status = 1;
        for (auto& c : clients)
            g.disconnect(c.get());
    }
    cleanUp();
    return status;
}
