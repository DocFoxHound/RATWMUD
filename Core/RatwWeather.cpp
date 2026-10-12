// The regional weather field (Docs/Design/29-client-polish.md, phase 7). World members, kept here.
//
// The world's own weather is a pure function of the world seed and the calendar: every six game hours each outdoor
// cell may give birth to a weather system, by its climate and the season. A system drifts east on the prevailing wind
// (with a little north or south), grows for its first hours, holds, and fades away; its strength falls smoothly from its
// middle to its edge. So the same moment always has the same weather, after a restart too, and nothing needs saving
// but the fronts a DM or developer calls up. A point's weather is the strongest system over it (and the next strongest,
// where two meet). Weather set on a cell by hand pins that whole cell, as it always did.
//
// Climates (the user, 2026-10-12): a cell's climate is what its author made it (its usual weather, and its ground), not
// what it is called; and how warm the land is changes gradually across the map (warmthAt), the cold country chilling
// the land near it. A system's rain falls as snow wherever the land is cold enough in the season, so the snow line
// lies across the land where it should, and moves with the year, rather than at the edges of cells.
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

// A cell's climate (the user, 2026-10-12: weather fit for each region): the weather its author gave it says what its
// country is (the generator writes a snowy country's cells "snow", a rainy one's "rain", a fen's "fog" or "overcast"),
// and where that was plain "clear", its ground does: a cell much under snow and ice is cold, one much of marsh is a
// marsh, one much of sand is arid.
Climate climateOf(const Cell& c, const CellGround* ground)
{
    switch (c.usual)
    {
    case Weather::Rain:
    case Weather::Storm:
        return Climate::Wet;
    case Weather::Snow:
        return Climate::Cold;
    case Weather::Fog:
    case Weather::Overcast:
        return Climate::Marsh;
    case Weather::Sandstorm:
        return Climate::Arid;
    default:
        break;
    }
    if (ground && ground->snow >= .25)
        return Climate::Cold;
    if (ground && ground->marsh >= .25)
        return Climate::Marsh;
    if (ground && ground->sand >= .4)
        return Climate::Arid;
    return Climate::Temperate;
}

// How warm a climate's country is: with the season's warmth (seasonalWarmth), rain falls as snow where the sum is below
// 0. A cold country is colder the more of it is under snow. Elsewhere it is the cold country nearby that chills the land
// (`coldNear`: how much of the country around is cold, 0 to 1): a temperate plain or a fen far from the mountains has
// mild winters (rain, not snow), one beneath them snowy ones; the sea keeps a wet coast milder still.
double warmthOf(Climate c, const CellGround* ground, double coldNear)
{
    const double snow = ground ? ground->snow : 0;
    switch (c)
    {
    case Climate::Cold: return -.6 - .6 * snow;
    case Climate::Wet: return .5 - .6 * coldNear - .6 * snow;
    case Climate::Arid: return 1.0;
    default: return .4 - 1.2 * coldNear - .6 * snow;
    }
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
    if (climatesFor_ != cells_.size())
        mapClimates();
    for (const auto& [id, c] : cells_)
    {
        if (!c.outdoors || c.worldZ != 0)
            continue;
        const auto known = climates_.find(id);
        const Climate climate = known == climates_.end() ? Climate::Temperate : Climate(known->second);
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
            s.byClimate = true;
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
    // Rain or snow, by how warm the land is here in this season (the world's own systems only: a front called up
    // brings what it was called up as). So a rain coming in off the Sound turns to snow as it reaches the mountains,
    // where the land grows cold, not at a cell's edge.
    int falls = -1;
    for (const auto& s : systems_)
    {
        const double v = strengthOf(s, x, y, calendarDays_);
        if (v <= 0)
            continue;
        Weather kind = s.kind;
        if (s.byClimate && (kind == Weather::Rain || kind == Weather::Snow))
        {
            if (falls < 0)
                falls = warmthAt(x, y) + seasonalWarmth() < 0 ? int(Weather::Snow) : int(Weather::Rain);
            kind = Weather(falls);
        }
        auto& slot = strongest[std::clamp(int(kind), 0, WeatherKinds - 1)];
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

int World::skyAt(const std::string& cellId, Vec2 at) const
{
    const auto* c = cell(cellId);
    if (!c)
        return 0;
    if (!c->outdoors)
        return -1;
    const auto s = weatherAt(cellId, at);
    if (s.intensity < .3)
        return 0;
    switch (s.kind)
    {
    case Weather::Rain:
        return 1;
    case Weather::Snow:
    case Weather::Storm:
    case Weather::Sandstorm:
        return 2;
    default:
        return 0;
    }
}

// Each outdoor cell's climate, and the warmth grid: each cell's warmth (warmthOf) on a grid of WarmthStep tiles over
// the world, blended over about BlendTiles either side of the cells' edges (a few box blurs, which make a smooth
// bell; the sea and places off the map, where there is no cell, count for nothing either way).
void World::mapClimates()
{
    constexpr double WarmthStep = 32;
    constexpr int BlendTiles = 96;
    constexpr int ColdReach = 384;                  // How far off the cold country chills the land (box blurs).
    climatesFor_ = cells_.size();
    climates_.clear();
    warmth_ = {};
    double x0 = std::numeric_limits<double>::infinity(), y0 = x0, x1 = -x0, y1 = -x0;
    std::vector<std::tuple<const Cell*, Climate, const CellGround*>> outdoors;
    for (const auto& [id, c] : cells_)
    {
        if (!c.outdoors || c.worldZ != 0 || c.width <= 0 || c.height <= 0)
            continue;
        const auto ground = grounds_.find(id);
        const auto* g = ground == grounds_.end() ? nullptr : &ground->second;
        const Climate climate = climateOf(c, g);
        climates_[id] = int(climate);
        outdoors.emplace_back(&c, climate, g);
        x0 = std::min(x0, c.worldX);
        y0 = std::min(y0, c.worldY);
        x1 = std::max(x1, c.worldX + c.width);
        y1 = std::max(y1, c.worldY + c.height);
    }
    if (outdoors.empty())
        return;
    auto& w = warmth_;
    w.x0 = x0;
    w.y0 = y0;
    w.cols = int(std::ceil((x1 - x0) / WarmthStep)) + 1;
    w.rows = int(std::ceil((y1 - y0) / WarmthStep)) + 1;
    if (double(w.cols) * double(w.rows) > 4e6)          // (A world too wide to grid: every cell as its own.)
    {
        w.cols = w.rows = 0;
        return;
    }
    const auto n = std::size_t(w.cols) * std::size_t(w.rows);
    // Each grid point's cell, by index into `outdoors` (-1: no cell there, the sea or off the map).
    std::vector<int> owner(n, -1);
    for (std::size_t i = 0; i < outdoors.size(); ++i)
    {
        const auto* c = std::get<0>(outdoors[i]);
        for (int gy = int(std::ceil((c->worldY - y0) / WarmthStep)); gy * WarmthStep + y0 < c->worldY + c->height && gy < w.rows; ++gy)
            for (int gx = int(std::ceil((c->worldX - x0) / WarmthStep)); gx * WarmthStep + x0 < c->worldX + c->width && gx < w.cols; ++gx)
                owner[std::size_t(gy) * std::size_t(w.cols) + std::size_t(gx)] = int(i);
    }
    int r = 1;
    const auto blur = [&](std::vector<double>& v, bool across) {
        std::vector<double> out(v.size(), 0);
        for (int y = 0; y < w.rows; ++y)
            for (int x = 0; x < w.cols; ++x)
            {
                double total = 0;
                for (int k = -r; k <= r; ++k)
                {
                    const int sx = across ? x + k : x, sy = across ? y : y + k;
                    if (sx >= 0 && sy >= 0 && sx < w.cols && sy < w.rows)
                        total += v[std::size_t(sy) * std::size_t(w.cols) + std::size_t(sx)];
                }
                out[std::size_t(y) * std::size_t(w.cols) + std::size_t(x)] = total;
            }
        v = std::move(out);
    };
    // Two box blurs each way, of a value and of where there is one (so places with no cell count for nothing).
    const auto blend = [&](std::vector<double>& sum, std::vector<double>& weight, int reach) {
        r = std::max(1, int(std::lround(reach / WarmthStep / 2)));
        for (int pass = 0; pass < 2; ++pass)
            for (const bool across : {true, false})
            {
                blur(sum, across);
                blur(weight, across);
            }
    };
    // How much of the country around each point is cold; then each point's warmth by its cell's climate, blended.
    std::vector<double> cold(n, 0), near(n, 0);
    for (std::size_t i = 0; i < n; ++i)
        if (owner[i] >= 0)
        {
            cold[i] = std::get<1>(outdoors[std::size_t(owner[i])]) == Climate::Cold;
            near[i] = 1;
        }
    blend(cold, near, ColdReach);
    std::vector<double> sum(n, 0), weight(n, 0);
    for (std::size_t i = 0; i < n; ++i)
        if (owner[i] >= 0)
        {
            const auto& [c, climate, g] = outdoors[std::size_t(owner[i])];
            sum[i] = warmthOf(climate, g, near[i] > 1e-9 ? cold[i] / near[i] : 0);
            weight[i] = 1;
        }
    blend(sum, weight, BlendTiles);
    w.values.assign(n, 0.f);
    for (std::size_t i = 0; i < n; ++i)
        w.values[i] = weight[i] > 1e-9 ? float(sum[i] / weight[i]) : 0.f;
}

double World::warmthAt(double x, double y) const
{
    constexpr double WarmthStep = 32;
    const auto& w = warmth_;
    if (w.cols <= 0 || w.rows <= 0)
        return 0;
    const double fx = std::clamp((x - w.x0) / WarmthStep, 0.0, double(w.cols - 1));
    const double fy = std::clamp((y - w.y0) / WarmthStep, 0.0, double(w.rows - 1));
    const int ix = std::min(int(fx), std::max(0, w.cols - 2)), iy = std::min(int(fy), std::max(0, w.rows - 2));
    const auto at = [&](int gx, int gy) {
        gx = std::clamp(gx, 0, w.cols - 1);
        gy = std::clamp(gy, 0, w.rows - 1);
        return double(w.values[std::size_t(gy) * std::size_t(w.cols) + std::size_t(gx)]);
    };
    const double tx = fx - ix, ty = fy - iy;
    return (at(ix, iy) * (1 - tx) + at(ix + 1, iy) * tx) * (1 - ty) + (at(ix, iy + 1) * (1 - tx) + at(ix + 1, iy + 1) * tx) * ty;
}

double World::seasonalWarmth() const
{
    // Warmest at midsummer (the middle of summer, day 138 of the year), coldest at midwinter, smoothly between: so the
    // first snows come to the mountains well before the lowlands, and the snow line climbs again in spring.
    constexpr double Pi = 3.14159265358979323846, MidsummerDay = 138;
    const auto date = calendar::calendarAt(calendarDays_);
    const double day = date.valid ? date.dayOfYear - 1 + std::fmod(date.hour, 24.0) / 24 : MidsummerDay;
    return .5 + .8 * std::cos(2 * Pi * (day - MidsummerDay) / calendar::DaysPerYear);
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
