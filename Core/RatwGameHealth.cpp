// Server health (Docs/Design/31-responsiveness.md, the health tracker): each minute's window and each long tick,
// as records for the recorder (Core/RatwHealth.h), read back by tools/perf_report.py and the DM's Health tab.
#include "RatwGame.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
double rounded(double v, double places = 2)
{
    const double scale = std::pow(10.0, places);
    return std::isfinite(v) ? std::round(v * scale) / scale : 0;
}

Value partsOf(const std::array<double, perf::PartCount>& parts, double divisor = 1)
{
    auto o = Value::object();
    for (std::size_t p = 0; p < perf::PartCount; ++p)
        o.add(perf::name(perf::Part(p)), rounded(parts[p] / divisor));
    return o;
}
} // namespace

void Game::healthWindow(const perf::Meter::Window& w, std::size_t clients, const Backlog& backlog)
{
    if (!health_)
        return;
    const double seconds = std::max(1e-9, std::chrono::duration<double>(perf::Meter::Clock::now() - w.began).count());
    const double ticks = double(std::max<std::uint64_t>(1, w.ticks));
    double sum = 0;
    for (const double p : w.passes)
        sum += p;
    auto body = Value::object();
    body.add("seconds", rounded(seconds, 1));
    body.add("clients", double(clients));
    body.add("ticks", double(w.ticks));
    auto tick = Value::object();
    tick.add("mean", rounded(w.passes.empty() ? 0 : sum / double(w.passes.size())));
    tick.add("p99", rounded(perf::percentile(w.passes, .99)));
    tick.add("max", rounded(perf::percentile(w.passes, 1.0)));
    tick.add("over50", double(std::count_if(w.passes.begin(), w.passes.end(), [](double p) { return p > 50; })));
    body.add("tick", tick);
    std::array<double, perf::PartCount> means{}, worst{};
    for (std::size_t p = 0; p < perf::PartCount; ++p)
    {
        means[p] = w.parts[p].ms;
        worst[p] = w.parts[p].worst;
    }
    body.add("parts", partsOf(means, ticks));
    body.add("worst", partsOf(worst));
    // The world's own parts, mean per tick, and its route searches.
    const auto& profile = world_.tickProfile();
    const double worldTicks = double(std::max<std::size_t>(1, profile.ticks));
    auto world = Value::object();
    world.add("streaming", rounded(profile.streaming.total / worldTicks));
    world.add("schedules", rounded(profile.schedules.total / worldTicks));
    world.add("movement", rounded(profile.movement.total / worldTicks));
    world.add("separation", rounded(profile.separation.total / worldTicks));
    world.add("views", rounded(profile.views.total / worldTicks));
    world.add("routes", double(profile.routeSearches));
    world.add("slowestRoute", rounded(profile.slowestRoute));
    body.add("world", world);
    auto traffic = Value::object();
    traffic.add("outMbps", rounded(double(w.bytesOut) * 8 / seconds / 1e6));
    traffic.add("messagesPerSecond", rounded(double(w.messagesOut) / seconds, 0));
    traffic.add("inKbps", rounded(double(w.bytesIn) * 8 / seconds / 1e3, 1));
    body.add("traffic", traffic);
    // Players' pings, as their pages told it in the last minute and a half: the middle player's middle ping, the
    // 95th percentile of their 95th percentiles, the worst ping, and whose; and corrections since the last window.
    std::vector<double> middles, highs;
    double worstPing = -1;
    std::string worstWho;
    int corrections = 0;
    for (auto* c : clients_)
    {
        corrections += std::max(0, c->netCorrections - c->netCorrectionsCounted);
        c->netCorrectionsCounted = c->netCorrections;
        if (c->netAt < 0 || world_.time() - c->netAt > 90)
            continue;
        middles.push_back(c->netP50);
        highs.push_back(c->netP95);
        if (c->netMax > worstPing)
        {
            worstPing = c->netMax;
            const auto* e = world_.entity(c->entityId);
            worstWho = e ? e->name : c->entityId;
        }
    }
    auto ping = Value::object();
    ping.add("reporting", double(middles.size()));
    ping.add("p50", rounded(middles.empty() ? 0 : perf::percentile(middles, .5), 1));
    ping.add("p95", rounded(highs.empty() ? 0 : perf::percentile(highs, .95), 1));
    ping.add("worst", rounded(std::max(0.0, worstPing), 1));
    ping.add("worstWho", worstWho);
    body.add("ping", ping);
    body.add("corrections", double(corrections));
    auto queue = Value::object();
    queue.add("largestKB", rounded(double(backlog.largest) / 1024, 1));
    queue.add("slow", double(backlog.slow));
    queue.add("dropped", double(backlog.dropped));
    body.add("backlog", queue);
    auto slowest = Value::array();
    for (const auto& s : w.slowest)
    {
        auto o = Value::object();
        o.add("ms", rounded(s.ms));
        o.add("parts", partsOf(s.parts));
        o.add("note", s.note);
        slowest.push(o);
    }
    body.add("slowest", slowest);
    health_->record("window", json::dump(body));
}

void Game::healthSpike(const perf::Meter::Window::Slow& pass)
{
    if (!health_)
        return;
    auto body = Value::object();
    body.add("ms", rounded(pass.ms));
    body.add("parts", partsOf(pass.parts));
    body.add("note", pass.note);
    body.add("clients", double(clients_.size()));
    health_->record("spike", json::dump(body));
}
} // namespace ratw::game
