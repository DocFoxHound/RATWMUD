// The regional weather field (Docs/Design/29-client-polish.md, phase 7). World members, kept here.
//
// The world's own weather is a pure function of the world seed and the calendar: every six game hours each outdoor
// cell may give birth to a weather system, by its climate and the season. A system drifts east on the prevailing wind
// (with a little north or south), grows for its first hours, holds, and fades away; its strength falls smoothly from its
// middle to its edge. So the same moment always has the same weather, after a restart too, and nothing needs saving
// but the fronts a DM or developer calls up. A point's weather is the strongest system over it (and the next strongest,
// where two meet). Weather set on a cell by hand pins that whole cell, as it always did.
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>

namespace ratw
{
namespace
{
constexpr std::uint64_t Seed = 0x52415457;          // "RATW", as the old per-cell forecast used.
constexpr double SlotDays = .25;                    // A chance of a new system every six game hours...
constexpr double LongestLife = 1.25;                // ...each living at most this many days.
constexpr double FieldStep = 1.0 / 96;              // The systems are worked out again every 15 game minutes.

enum class Climate { Temperate, Wet, Cold, Arid, Marsh };

std::uint64_t mix(std::uint64_t x)
{
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

std::uint64_t hashText(const std::string& s)
{
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s)
        h = (h ^ c) * 1099511628211ULL;
    return h;
}

double unit(std::uint64_t& h)
{
    h = mix(h);
    return double(h % 1000000) / 1000000.0;
}

// A cell's climate, from what its region is called (the generators name regions for their country).
Climate climateOf(const Cell& c)
{
    std::string r = c.region + " " + c.id + " " + c.name;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
    const auto has = [&](std::initializer_list<const char*> words) {
        return std::any_of(words.begin(), words.end(), [&](const char* w) { return r.find(w) != std::string::npos; });
    };
    if (has({"fen", "mire", "marsh", "bog", "drowned", "swamp", "mere"}))
        return Climate::Marsh;
    if (has({"coast", "isle", "ridgemere", "saltreach", "rain", "harbour", "sound", "grey horn"}))
        return Climate::Wet;
    if (has({"moor", "north", "bleak", "frost", "peak", "summit", "upper_accord", "ridge", "glacier", "snow", "warden"}))
        return Climate::Cold;
    if (has({"steppe", "amber", "barren", "ghost", "dune", "desert", "dust", "waste"}))
        return Climate::Arid;
    return Climate::Temperate;
}

// How likely a system is in each six hours from one cell, and what kind, by climate and season (0 spring .. 3 winter).
double chanceOf(Climate c)
{
    switch (c)
    {
    case Climate::Wet: return .10;
    case Climate::Marsh: return .08;
    case Climate::Cold: return .07;
    case Climate::Arid: return .03;
    default: return .045;
    }
}

Weather kindOf(Climate c, int season, double roll)
{
    const bool winter = season == 3, summer = season == 1;
    struct Odds
    {
        Weather kind;
        double weight;
    };
    std::vector<Odds> odds;
    switch (c)
    {
    case Climate::Wet:
        odds = {{winter ? Weather::Snow : Weather::Rain, 55}, {Weather::Storm, 15}, {Weather::Fog, 15}, {Weather::Overcast, 15}};
        break;
    case Climate::Marsh:
        odds = {{Weather::Fog, 45}, {Weather::Rain, 35}, {Weather::Overcast, 20}};
        break;
    case Climate::Cold:
        odds = {{summer ? Weather::Rain : Weather::Snow, winter ? 75 : 45}, {Weather::Fog, 15}, {Weather::Overcast, 25}, {Weather::Storm, 8}};
        break;
    case Climate::Arid:
        odds = {{Weather::Sandstorm, 45}, {Weather::Overcast, 35}, {Weather::Rain, summer ? 5 : 15}};
        break;
    default:
        odds = {{winter ? Weather::Snow : Weather::Rain, 45}, {Weather::Overcast, 30}, {Weather::Fog, 15}, {Weather::Storm, summer ? 12 : 6}};
        break;
    }
    double total = 0;
    for (const auto& o : odds)
        total += o.weight;
    roll *= total;
    for (const auto& o : odds)
        if ((roll -= o.weight) <= 0)
            return o.kind;
    return odds.back().kind;
}

double smooth(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3 - 2 * t);
}

// How strong a system is at a point and a moment: it grows, holds and fades, and weakens toward its edge.
double strengthOf(const WeatherSystem& s, double x, double y, double day)
{
    const double age = (day - s.born) / s.life;
    if (age < 0 || age > 1)
        return 0;
    const double time = smooth(age / .15) * (1 - smooth((age - .7) / .3));
    const double cx = s.x + s.vx * (day - s.born), cy = s.y + s.vy * (day - s.born);
    const double d = std::hypot(x - cx, y - cy);
    if (d >= s.radius)
        return 0;
    return s.peak * time * smooth(1 - d / s.radius);
}
} // namespace

double weatherStrengthAt(const WeatherSystem& system, double x, double y, double day) { return strengthOf(system, x, y, day); }

void World::refreshWeatherField(bool force)
{
    const auto stamp = std::int64_t(std::floor(calendarDays_ / FieldStep));
    if (!force && stamp == fieldStamp_)
        return;
    fieldStamp_ = stamp;
    systems_.clear();
    const int season = int(calendar::calendarAt(calendarDays_).season);
    const auto lastSlot = std::int64_t(std::floor(calendarDays_ / SlotDays));
    const auto firstSlot = std::int64_t(std::floor((calendarDays_ - LongestLife) / SlotDays));
    for (const auto& [id, c] : cells_)
    {
        if (!c.outdoors || c.worldZ != 0)
            continue;
        const Climate climate = climateOf(c);
        const double chance = chanceOf(climate);
        const double ax = c.worldX + c.width * .5, ay = c.worldY + c.height * .5;
        const auto base = hashText(id) ^ Seed;
        for (auto slot = firstSlot; slot <= lastSlot; ++slot)
        {
            std::uint64_t h = mix(base ^ std::uint64_t(slot) * 0x9E3779B97F4A7C15ULL);
            if (unit(h) >= chance)
                continue;
            WeatherSystem s;
            s.id = id + ":" + std::to_string(slot);
            s.born = double(slot) * SlotDays + unit(h) * SlotDays;
            s.life = .3 + unit(h) * (LongestLife - .3);
            if (calendarDays_ < s.born || calendarDays_ > s.born + s.life)
                continue;
            s.kind = kindOf(climate, season, unit(h));
            s.radius = 140 + unit(h) * 380;
            s.peak = .55 + unit(h) * .45;
            s.x = ax + (unit(h) - .5) * c.width;
            s.y = ay + (unit(h) - .5) * c.height;
            // The prevailing wind: from the west, with a little north or south in it.
            s.vx = 90 + unit(h) * 180;
            s.vy = (unit(h) - .5) * 140;
            systems_.push_back(s);
        }
    }
    for (const auto& f : fronts_)
        if (calendarDays_ >= f.born && calendarDays_ <= f.born + f.life)
            systems_.push_back(f);
    // A cell left to the seasons shows the weather at its middle (what its name, the schedules and the old views use).
    for (auto& [id, c] : cells_)
        if (c.outdoors && c.seasonalWeather)
        {
            const auto s = sampleField(c.worldX + c.width * .5, c.worldY + c.height * .5);
            c.weather = s.intensity >= .3 ? s.kind : Weather::Clear;
        }
}

WeatherSample World::sampleField(double x, double y) const
{
    WeatherSample out;
    double strongest[WeatherKinds] = {};
    for (const auto& s : systems_)
    {
        const double v = strengthOf(s, x, y, calendarDays_);
        auto& slot = strongest[std::clamp(int(s.kind), 0, WeatherKinds - 1)];
        slot = std::max(slot, v);
    }
    for (int k = 0; k < WeatherKinds; ++k)
    {
        if (k == int(Weather::Clear) || strongest[k] <= 0)
            continue;
        if (strongest[k] > out.intensity)
        {
            out.second = out.kind;
            out.secondIntensity = out.intensity;
            out.kind = Weather(k);
            out.intensity = strongest[k];
        }
        else if (strongest[k] > out.secondIntensity)
        {
            out.second = Weather(k);
            out.secondIntensity = strongest[k];
        }
    }
    if (out.intensity < .05)
        out = {};
    return out;
}

WeatherSample World::weatherAt(const std::string& cellId, Vec2 at) const
{
    const auto* c = cell(cellId);
    if (!c || !c->outdoors)
        return {};
    if (!c->seasonalWeather)
        return {c->weather, c->weather == Weather::Clear ? 0.0 : 1.0, Weather::Clear, 0};
    auto s = sampleField(c->worldX + at.x, c->worldY + at.y);
    // Fog gathers in low ground and thins on the heights.
    if (s.kind == Weather::Fog)
        if (const auto* t = c->tile(int(std::floor(at.x)), int(std::floor(at.y))))
            s.intensity = std::clamp(s.intensity * (1 - t->height * .06), 0.0, 1.0);
    return s;
}

std::vector<WeatherSample> World::weatherGrid(const std::string& cellId, int step, int& cols, int& rows) const
{
    std::vector<WeatherSample> out;
    const auto* c = cell(cellId);
    cols = rows = 0;
    if (!c || !c->outdoors || step <= 0)
        return out;
    cols = (c->width + step - 1) / step + 1;
    rows = (c->height + step - 1) / step + 1;
    out.reserve(std::size_t(cols * rows));
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
            out.push_back(weatherAt(cellId, {double(std::min(x * step, c->width)), double(std::min(y * step, c->height))}));
    return out;
}

Result World::spawnFront(Weather kind, double x, double y, double radius, double heading, double hours, bool grown)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius) || !std::isfinite(heading) || !std::isfinite(hours) ||
        radius < 20 || radius > 2000 || hours <= 0 || hours > 72 || kind == Weather::Clear)
        return {false, "A front needs a kind other than clear, a reach of 20 to 2000 tiles and 1 to 72 hours.", {}};
    WeatherSystem f;
    f.id = "front-" + std::to_string(frontNext_++);
    f.kind = kind;
    f.x = x;
    f.y = y;
    f.radius = radius;
    f.peak = 1;
    const double speed = 160;                       // Tiles a day.
    f.vx = std::cos(heading) * speed;
    f.vy = std::sin(heading) * speed;
    f.life = hours / 24;
    f.born = calendarDays_ - (grown ? f.life * .2 : 0);
    if (grown)
    {
        f.x -= f.vx * (calendarDays_ - f.born);     // Its middle over the given point now, not where it was born.
        f.y -= f.vy * (calendarDays_ - f.born);
    }
    fronts_.push_back(f);
    // Fronts long gone are let go.
    fronts_.erase(std::remove_if(fronts_.begin(), fronts_.end(), [&](const WeatherSystem& s) { return s.born + s.life < calendarDays_ - 1; }),
                  fronts_.end());
    refreshWeatherField(true);
    return {true, "A front of " + std::string(weatherName(kind)) + " is on its way.", f.id};
}
} // namespace ratw
