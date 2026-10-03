#pragma once
// Where the game thread's time goes (Docs/Design/31-responsiveness.md, Phase 1). The game, the server and the load
// test (Tests/game_load.cpp) all fill one Meter: each piece of work is timed by a Scope, exclusive of any Scope inside
// it (compressing a snapshot counts as compression, not as the view that sent it). The server reports a window of it
// every minute; the load test, once at the end.
//
// Timing belongs to the thread that made the meter (the game's): a Scope on any other thread (the worker pool's)
// counts nothing, and the pool's work is timed as a whole by the game thread around it. Traffic (sent) may be counted
// from any thread. Timing costs two clock reads per Scope.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace ratw::perf
{
enum Part : std::uint8_t
{
    Commands,       // A client's commands and acknowledgements, as they arrive.
    World,          // World::tick: streaming, schedules, movement, separation, views' sight.
    Views,          // Building and sending players' snapshots.
    Sight,          // What the players due a snapshot can see, and their memories of it (before their views).
    Motion,         // Building and sending players' motion frames.
    TickOther,      // The rest of Game::tick: the Mind, ambient talk, barks, residents, the DM, the director.
    Saves,          // Capturing the world for a save, and waiting for one (Game::save).
    Database,       // Statements on the game thread's own database connection.
    Compression,    // zlib, as each message is queued for a client.
    Sockets,        // Accepting, reading, parsing and writing (the server).
    PartCount
};

inline const char* name(Part p)
{
    static const char* names[] = {"commands", "world", "views", "sight", "motion", "tick-other", "saves", "database", "compression", "sockets"};
    return p < PartCount ? names[p] : "?";
}

// Upper bounds (ms) of the histogram's buckets; the last is everything slower.
constexpr std::array<double, 9> BucketLimits = {1, 2, 5, 10, 20, 50, 100, 200, 500};
constexpr std::size_t Buckets = BucketLimits.size() + 1;

inline std::size_t bucket(double ms)
{
    std::size_t b = 0;
    while (b < BucketLimits.size() && ms >= BucketLimits[b])
        ++b;
    return b;
}

class Meter
{
  public:
    using Clock = std::chrono::steady_clock;

    struct Totals
    {
        double ms = 0, worst = 0;
        std::uint64_t count = 0;
        std::array<std::uint32_t, Buckets> histogram{};
    };
    struct Window
    {
        std::array<Totals, PartCount> parts{};
        std::vector<double> passes;                 // Busy time of each loop pass that ran a tick (ms).
        std::uint64_t ticks = 0;
        std::uint64_t bytesOut = 0, messagesOut = 0, bytesIn = 0;
        Clock::time_point began = Clock::now();
    };

    // Timed work: inclusive time less the time of any Scope that ran inside it.
    bool own() const { return std::this_thread::get_id() == owner_; }
    void begin(Part p)
    {
        if (!own())
            return;
        if (depth_ < Stack)
            stack_[depth_] = {p, Clock::now(), 0};
        ++depth_;
    }
    void end()
    {
        if (!own() || depth_ == 0)
            return;
        --depth_;
        if (depth_ >= Stack)
            return;
        const auto& open = stack_[depth_];
        const double inclusive = std::chrono::duration<double, std::milli>(Clock::now() - open.at).count();
        add(open.part, std::max(0.0, inclusive - open.inside));
        if (depth_ > 0 && depth_ - 1 < Stack)
            stack_[depth_ - 1].inside += inclusive;
    }
    void add(Part p, double ms)
    {
        if (!own())
            return;
        auto& t = window_.parts[p];
        t.ms += ms;
        t.worst = std::max(t.worst, ms);
        ++t.count;
        ++t.histogram[bucket(ms)];
    }
    void pass(double busyMs, bool ticked)
    {
        if (ticked)
        {
            window_.passes.push_back(busyMs);
            ++window_.ticks;
        }
    }
    void sent(std::size_t bytes)
    {
        bytesOut_.fetch_add(bytes, std::memory_order_relaxed);
        messagesOut_.fetch_add(1, std::memory_order_relaxed);
    }
    void received(std::size_t bytes) { window_.bytesIn += bytes; }

    const Window& window() const { return window_; }
    // The window so far, and a new one begun.
    Window take()
    {
        Window out = std::move(window_);
        out.bytesOut = bytesOut_.exchange(0);
        out.messagesOut = messagesOut_.exchange(0);
        window_ = Window{};
        return out;
    }

  private:
    static constexpr std::size_t Stack = 8;
    struct Open
    {
        Part part;
        Clock::time_point at;
        double inside;
    };
    std::array<Open, Stack> stack_{};
    std::size_t depth_ = 0;
    Window window_;
    std::atomic<std::uint64_t> bytesOut_{0}, messagesOut_{0};
    std::thread::id owner_ = std::this_thread::get_id();
};

// Times the enclosing block as `p`; nothing at all without a meter.
class Scope
{
  public:
    Scope(Meter* m, Part p) : meter_(m)
    {
        if (meter_)
            meter_->begin(p);
    }
    ~Scope()
    {
        if (meter_)
            meter_->end();
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

  private:
    Meter* meter_;
};

inline double percentile(std::vector<double> values, double p)
{
    if (values.empty())
        return 0;
    std::sort(values.begin(), values.end());
    return values[std::min(values.size() - 1, std::size_t(p * double(values.size())))];
}

inline std::string fixed(double v, int places = 1)
{
    char text[32];
    std::snprintf(text, sizeof text, "%.*f", places, v);
    return text;
}

// The window as log lines: "RATW_PERF" (the loop: passes, each part's mean per tick, clients and traffic) and
// "RATW_PERF_BLOCKED" (for each part with any call of 5 ms or more, how many calls fell in each bucket).
inline std::vector<std::string> report(const Meter::Window& w, std::size_t clients)
{
    const double seconds = std::max(1e-9, std::chrono::duration<double>(Meter::Clock::now() - w.began).count());
    const double ticks = double(std::max<std::uint64_t>(1, w.ticks));
    std::vector<std::string> lines;
    double sum = 0;
    for (double p : w.passes)
        sum += p;
    std::string line = "RATW_PERF " + fixed(seconds, 0) + "s ticks=" + std::to_string(w.ticks) + " clients=" + std::to_string(clients) +
                       " pass mean=" + fixed(w.passes.empty() ? 0 : sum / double(w.passes.size())) +
                       " p99=" + fixed(percentile(w.passes, .99)) + " max=" + fixed(percentile(w.passes, 1.0)) + " ms; per tick:";
    for (std::size_t p = 0; p < PartCount; ++p)
        line += std::string(" ") + name(Part(p)) + "=" + fixed(w.parts[p].ms / ticks, 2);
    line += " ms; out=" + fixed(double(w.bytesOut) * 8 / seconds / 1e6, 2) + " Mbit/s (" + fixed(double(w.messagesOut) / seconds, 0) +
            " msg/s) in=" + fixed(double(w.bytesIn) * 8 / seconds / 1e3, 1) + " kbit/s";
    lines.push_back(line);
    std::string blocked;
    for (std::size_t p = 0; p < PartCount; ++p)
    {
        const auto& t = w.parts[p];
        if (t.worst < 5)
            continue;
        blocked += std::string(" ") + name(Part(p)) + "[worst " + fixed(t.worst) + "ms:";
        for (std::size_t b = bucket(5); b < Buckets; ++b)
            if (t.histogram[b])
                blocked += " " + (b < BucketLimits.size() ? "<" + fixed(BucketLimits[b], 0) : ">=" + fixed(BucketLimits.back(), 0)) + "ms×" +
                           std::to_string(t.histogram[b]);
        blocked += "]";
    }
    if (!blocked.empty())
        lines.push_back("RATW_PERF_BLOCKED" + blocked);
    return lines;
}
} // namespace ratw::perf

namespace ratw::perf
{
// World::TickProfile (RatwWorld.h) over a window, as a log line: "RATW_PERF_WORLD", each part's mean per tick and
// worst, and the route searches.
template <class Profile> std::string worldLine(const Profile& p)
{
    const double ticks = double(std::max<std::size_t>(1, p.ticks));
    std::string line = "RATW_PERF_WORLD";
    const std::pair<const char*, const typename Profile::Part*> parts[] = {
        {"streaming", &p.streaming}, {"schedules", &p.schedules}, {"movement", &p.movement}, {"separation", &p.separation}, {"views", &p.views}};
    for (const auto& [label, part] : parts)
        line += std::string(" ") + label + "=" + fixed(part->total / ticks, 2) + "(worst " + fixed(part->worst) + ")";
    line += " ms; routes=" + std::to_string(p.routeSearches) + " nodes=" + std::to_string(p.routeNodes) + " largest=" +
            std::to_string(p.largestRoute) + " slowest=" + fixed(p.slowestRoute) + "ms";
    if (!p.slowestRouteCell.empty())
        line += " in " + p.slowestRouteCell;
    return line;
}
} // namespace ratw::perf
