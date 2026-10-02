#include "RatwWorld.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <thread>

namespace ratw
{
namespace
{
constexpr double Radius = 0.065;
constexpr double WalkSpeed = 2.6;
constexpr double StaminaRecovery = 5.0;
constexpr double SprintDrain = 15.0;
constexpr double ExhaustionRecovery = 20.0;
constexpr double DaySeconds = calendar::SecondsPerDay;
constexpr int NavScale = 4;
// Weighted A*: trusting the straight-line estimate this much more explores a small fraction of a large cell's
// million-node grid, for routes at most this much longer than the shortest (in practice barely longer at all).
constexpr double SearchGreed = 2.0;
constexpr std::size_t SmoothLookahead = 40; // Waypoints a smoothed path may skip in one straight line.
// Residents plan their routes a few a tick (planWantedRoutes): at most this many searches in one tick, and no new
// one once this much search work (nodes expanded) has been done in it.
constexpr int RouteSearchesPerTick = 1;
constexpr std::size_t RouteNodesPerTick = 30000;
constexpr double Epsilon = 1e-7;
constexpr double Pi = 3.14159265358979323846;
constexpr double TurnSpeed = Pi; // Radians per second: 180 degrees.
calendar::Weather skyWeather(Weather value)
{
    // The sky model only knows how much moonlight each kind lets through; cloud cover passes like rain, dust like fog.
    switch (value) { case Weather::Rain: case Weather::Storm: case Weather::Overcast: return calendar::Weather::Rain;
        case Weather::Snow: return calendar::Weather::Snow;
        case Weather::Fog: case Weather::Sandstorm: return calendar::Weather::Fog; default: return calendar::Weather::Clear; }
}
double length(Vec2 a)
{
    return std::hypot(a.x, a.y);
}
double distance(Vec2 a, Vec2 b)
{
    return length({a.x - b.x, a.y - b.y});
}
Vec2 normalized(Vec2 v)
{
    const auto n = length(v);
    return n > Epsilon ? Vec2{v.x / n, v.y / n} : Vec2{};
}
bool finite(Vec2 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y);
}
double clamp01(double x)
{
    return std::max(0.0, std::min(1.0, x));
}
bool validLighting(const Lighting& light)
{
    return std::isfinite(light.artificial) && light.artificial >= 0 && light.artificial <= 1 &&
           std::isfinite(light.daylightAccess) && light.daylightAccess >= 0 && light.daylightAccess <= 1 &&
           (light.tone == "warm" || light.tone == "neutral" || light.tone == "cool");
}
double clarity(double d, double clear)
{
    if (clear <= Epsilon || d >= clear * 2.0)
        return 0.0;
    return d <= clear ? 1.0 : clamp01(2.0 - d / clear);
}
bool stablePosture(const std::string& posture)
{
    return posture == "standing" || posture == "sitting" || posture == "lying" || posture == "crouching";
}
void clearTransientMotion(Entity& e)
{
    e.path.clear();
    e.input = {};
    e.velocity = {};
    e.turning = false;
    e.turnTarget = e.facing;
    // A reload can finish a physical posture change, never a travel intention.
    if (e.posture == "rising")
        e.posture = e.postureTarget;
    e.postureRemaining = 0.0;
    e.postureTarget.clear();
    e.transitioned = false;
    e.staminaRate = 0.0;
}
Tile fromGlyph(char g)
{
    Tile t;
    t.glyph = g;
    if (const auto* info = terrainInfo(g))
    {
        t.terrain = info->kind;
        t.solid = info->solid;
        t.opaque = info->opaque;
        t.height = info->height;
        t.stature = info->stature;
        t.movementCost = info->cost;
    }
    return t;
}
void fillRect(Cell& c, int x0, int y0, int x1, int y1, char glyph)
{
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            if (auto* t = c.tile(x, y))
                *t = fromGlyph(glyph);
}
Cell room(std::string id, std::string name, int w, int h, bool outside = false)
{
    Cell c;
    c.id = std::move(id);
    c.name = std::move(name);
    c.width = w;
    c.height = h;
    c.outdoors = outside;
    if (outside)
        c.wind = {0.0, .5, true};
    c.tiles.resize(static_cast<std::size_t>(w * h), fromGlyph(outside ? ',' : '.'));
    fillRect(c, 0, 0, w - 1, 0, '#');
    fillRect(c, 0, h - 1, w - 1, h - 1, '#');
    fillRect(c, 0, 0, 0, h - 1, '#');
    fillRect(c, w - 1, 0, w - 1, h - 1, '#');
    return c;
}
bool doorCovers(const Door& door, Vec2 p)
{
    return int(std::floor(p.x)) == int(std::floor(door.position.x)) &&
           int(std::floor(p.y)) == int(std::floor(door.position.y));
}

constexpr double MaxScentDistance = 64.0;
struct AirRoute
{
    double distance = -1.0;
    Vec2 bearing;
};

// Air ignores low furniture, but cannot cross opaque walls or closed doors.
// The route field is lazy and shared across all scents in one observer query.
// It is a bounded air-path approximation, not a fluid/plume simulation.
class AirRoutes
{
  public:
    // `doors`: the cell's own doors (World::doorsIn), not the world's.
    AirRoutes(const Cell& c, const std::vector<Door*>& doors, Vec2 origin)
        : c_(c), origin_(origin), open_(c.tiles.size(), false)
    {
        for (std::size_t i = 0; i < c.tiles.size(); ++i)
            open_[i] = !c.tiles[i].opaque && c.tiles[i].terrain != Terrain::Wall;
        for (const Door* door : doors)
        {
            const auto& d = *door;
            if (d.cellId == c.id && !d.open && inside(d.position))
                open_[index(int(d.position.x), int(d.position.y))] = false;
        }
    }

    AirRoute route(Vec2 target)
    {
        if (!inside(origin_) || !inside(target) || distance(origin_, target) > MaxScentDistance ||
            !open(int(origin_.x), int(origin_.y)) || !open(int(target.x), int(target.y)))
            return {};
        if (clear(target))
            return {distance(origin_, target), {target.x - origin_.x, target.y - origin_.y}};
        if (distances_.empty())
            build();
        const auto i = index(int(target.x), int(target.y));
        const double last = distance({std::floor(target.x) + .5, std::floor(target.y) + .5}, target);
        return std::isfinite(distances_[i]) && distances_[i] + last <= MaxScentDistance
                   ? AirRoute{distances_[i] + last, bearings_[i]}
                   : AirRoute{};
    }

  private:
    const Cell& c_;
    Vec2 origin_;
    std::vector<bool> open_;
    std::vector<double> distances_;
    std::vector<Vec2> bearings_;

    std::size_t index(int x, int y) const
    {
        return static_cast<std::size_t>(y * c_.width + x);
    }
    bool inside(Vec2 p) const
    {
        return finite(p) && p.x >= 0 && p.y >= 0 && p.x < c_.width && p.y < c_.height;
    }
    bool open(int x, int y) const
    {
        return x >= 0 && y >= 0 && x < c_.width && y < c_.height && index(x, y) < open_.size() && open_[index(x, y)];
    }
    bool clear(Vec2 target) const
    {
        const int samples = std::max(1, int(std::ceil(distance(origin_, target) / .12)));
        int lastX = int(origin_.x), lastY = int(origin_.y);
        for (int n = 1; n <= samples; ++n)
        {
            const double t = double(n) / samples;
            const int x = int(origin_.x + (target.x - origin_.x) * t);
            const int y = int(origin_.y + (target.y - origin_.y) * t);
            if (!open(x, y) || (x != lastX && y != lastY && (!open(x, lastY) || !open(lastX, y))))
                return false;
            lastX = x;
            lastY = y;
        }
        return true;
    }
    void build()
    {
        distances_.assign(c_.tiles.size(), std::numeric_limits<double>::infinity());
        bearings_.resize(c_.tiles.size());
        using Entry = std::pair<double, std::size_t>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pending;
        const auto start = index(int(origin_.x), int(origin_.y));
        distances_[start] = distance(origin_, {std::floor(origin_.x) + .5, std::floor(origin_.y) + .5});
        pending.push({distances_[start], start});
        while (!pending.empty())
        {
            const auto current = pending.top();
            pending.pop();
            if (current.first > distances_[current.second])
                continue;
            const int x = int(current.second % static_cast<std::size_t>(c_.width));
            const int y = int(current.second / static_cast<std::size_t>(c_.width));
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if ((!dx && !dy) || !open(x + dx, y + dy) || (dx && dy && (!open(x + dx, y) || !open(x, y + dy))))
                        continue;
                    const double nextDistance = current.first + (dx && dy ? std::sqrt(2.0) : 1.0);
                    const auto next = index(x + dx, y + dy);
                    if (nextDistance >= distances_[next] || nextDistance > MaxScentDistance)
                        continue;
                    distances_[next] = nextDistance;
                    bearings_[next] = current.second == start ? Vec2{x + dx + .5 - origin_.x, y + dy + .5 - origin_.y}
                                                              : bearings_[current.second];
                    pending.push({nextDistance, next});
                }
        }
    }
};

struct ScentDetection
{
    double clarity = 0;
    Vec2 bearing;
    bool windborne = false;
};
ScentDetection detectScent(const Entity& observer, const Entity& source, Wind wind, double scentFactor, AirRoutes& air)
{
    if (observer.id == source.id || observer.cellId != source.cellId || !std::isfinite(observer.smell) ||
        !std::isfinite(observer.noseHealth) || !std::isfinite(observer.scentSkill))
        return {};
    const double sensitivity = std::max(0.0, observer.smell) * clamp01(observer.noseHealth) *
                               (1.0 + .75 * clamp01(observer.scentSkill / 100.0));
    if (sensitivity <= Epsilon)
        return {};
    const Vec2 sourceToObserver =
        normalized({observer.position.x - source.position.x, observer.position.y - source.position.y});
    const double alignment =
        std::max(0.0, sourceToObserver.x * std::cos(wind.direction) + sourceToObserver.y * std::sin(wind.direction));
    const double carry = wind.strength * alignment * alignment;
    const double full = std::min(MaxScentDistance, (1.0 + 10.0 * carry) * sensitivity * scentFactor);
    const double limit = std::min(MaxScentDistance, (3.0 + 30.0 * carry) * sensitivity * scentFactor);
    if (distance(observer.position, source.position) >= limit)
        return {};
    const auto route = air.route(source.position);
    if (route.distance < 0 || route.distance >= limit)
        return {};
    const double strength = route.distance <= full ? 1.0 : clamp01((limit - route.distance) / (limit - full));
    return {strength, route.bearing, wind.strength > .05 && alignment > .35};
}
} // namespace

const Tile* Cell::tile(int x, int y) const
{
    if (x < 0 || y < 0 || x >= width || y >= height)
        return nullptr;
    const auto index = static_cast<std::size_t>(y * width + x);
    return index < tiles.size() ? &tiles[index] : nullptr;
}
Tile* Cell::tile(int x, int y)
{
    return const_cast<Tile*>(static_cast<const Cell&>(*this).tile(x, y));
}

Tile tileFromGlyph(char glyph)
{
    return fromGlyph(glyph);
}

const std::vector<TerrainInfo>& terrainCatalog()
{
    static const std::vector<TerrainInfo> catalog = {
#define RATW_TERRAIN(code, kind, solid, opaque, height, stature, cost, ramp, glyph, ascii, fg, bg, name)                 \
    {char(code), Terrain::kind, solid, opaque, height, stature, cost, ramp, char16_t(glyph), char(ascii), fg, bg, name},
#include "RatwTerrainCatalog.inc"
#undef RATW_TERRAIN
    };
    return catalog;
}

const TerrainInfo* terrainInfo(char code)
{
    static const auto index = [] {
        std::array<const TerrainInfo*, 128> table{};
        for (const auto& info : terrainCatalog())
            table[static_cast<unsigned char>(info.code)] = &info;
        return table;
    }();
    const auto slot = static_cast<unsigned char>(code);
    return slot < index.size() ? index[slot] : nullptr;
}

const char* weatherName(Weather w)
{
    switch (w)
    {
    case Weather::Rain:
        return "rain";
    case Weather::Fog:
        return "fog";
    case Weather::Snow:
        return "snow";
    case Weather::Overcast:
        return "overcast";
    case Weather::Storm:
        return "storm";
    case Weather::Sandstorm:
        return "sandstorm";
    default:
        return "clear";
    }
}
bool parseWeather(const std::string& name, Weather& out)
{
    for (int i = 0; i < WeatherKinds; ++i)
        if (name == weatherName(static_cast<Weather>(i)))
        {
            out = static_cast<Weather>(i);
            return true;
        }
    return false;
}
const char* knowledgeName(Knowledge k)
{
    switch (k)
    {
    case Knowledge::Visited:
        return "visited";
    case Knowledge::Glimpsed:
        return "glimpsed";
    default:
        return "unknown";
    }
}
const char* voiceName(Voice v)
{
    switch (v)
    {
    case Voice::Whisper:
        return "whisper";
    case Voice::Yell:
        return "yell";
    default:
        return "speak";
    }
}

const char* paceName(int pace)
{
    if (pace <= 0)
        return "walk";
    if (pace <= 5)
        return "trot";
    if (pace <= 8)
        return "run";
    return "sprint";
}

int effectivePace(const Entity& actor)
{
    // Lying movement first becomes a crouch. A requested sprint never
    // overrides that posture, nor can it circumvent exhaustion recovery.
    if (actor.posture != "standing" || actor.exhausted || actor.stamina <= 0.0)
        return 0;
    return std::clamp(actor.pace, 0, 10);
}

// A posture taken at once, without the timed rise only full simulation advances (for offstage NPCs).
void settle(Entity& e, const char* posture)
{
    e.posture = posture;
    e.postureRemaining = 0;
    e.postureTarget.clear();
}

double paceSpeed(const Entity& actor)
{
    const double dexterity = clamp01(effectiveDexterity(actor) / 100.0);
    const double sprintSpeed = WalkSpeed * (2.0 + 2.0 * dexterity);
    return WalkSpeed + (sprintSpeed - WalkSpeed) * (effectivePace(actor) / 10.0);
}

World::World()
{
    createDemo();
}
void World::createDemo()
{
    Cell tavern = room("tavern", "The Bent Bough", 32, 24);
    tavern.description = "Warm ash and steeping juniper cling to the rafters. Low tables and broad resting mats gather "
                         "around the hearth.";
    fillRect(tavern, 5, 4, 13, 4, '=');
    fillRect(tavern, 5, 9, 7, 10, 'T');
    fillRect(tavern, 11, 16, 13, 17, 'T');
    fillRect(tavern, 22, 16, 24, 17, 'T');
    fillRect(tavern, 6, 18, 7, 19, 'T');
    fillRect(tavern, 23, 1, 23, 9, '#');
    fillRect(tavern, 23, 9, 30, 9, '#');
    *tavern.tile(23, 6) = fromGlyph('+');
    *tavern.tile(16, 23) = fromGlyph('+');
    *tavern.tile(28, 4) = fromGlyph('^');
    Cell exterior = room("exterior", "Juniper Yard", 40, 28, true);
    exterior.worldY = 24;
    exterior.weather = Weather::Rain;
    exterior.description = "A broad packed-earth path winds between juniper thickets and a stone-lined spring.";
    fillRect(exterior, 14, 1, 18, 26, '.');
    fillRect(exterior, 1, 12, 38, 15, '.');
    fillRect(exterior, 26, 15, 29, 18, '~');
    fillRect(exterior, 7, 5, 10, 6, '#');
    fillRect(exterior, 25, 7, 26, 9, '#');
    fillRect(exterior, 14, 18, 18, 18, ':');
    fillRect(exterior, 14, 19, 18, 20, '^');
    *exterior.tile(16, 0) = fromGlyph('+');
    Cell loft = room("loft", "The Quiet Loft", 20, 14);
    loft.worldZ = 1;
    loft.description =
        "Below the sloped roof, dry herbs and old paper scent a quiet resting place above the common room.";
    fillRect(loft, 4, 4, 6, 4, 'T');
    fillRect(loft, 12, 7, 14, 8, 'T');
    *loft.tile(3, 10) = fromGlyph('^');
    cells_.emplace(tavern.id, tavern);
    cells_.emplace(exterior.id, exterior);
    cells_.emplace(loft.id, loft);
    for (auto& entry : cells_) entry.second.region = "demo_reach";
    Door main;
    main.id = "door_main";
    main.name = "Yard door";
    main.cellId = "tavern";
    main.position = {16.5, 23.5};
    main.portal = main.boundary = true;
    main.targetCell = "exterior";
    main.arrival = {16.5, 1.5};
    main.linkedDoor = "door_yard";
    doors_[main.id] = main;
    Door yard = main;
    yard.id = "door_yard";
    yard.name = "Hearth door";
    yard.cellId = "exterior";
    yard.position = {16.5, 0.5};
    yard.targetCell = "tavern";
    yard.arrival = {16.5, 22.5};
    yard.linkedDoor = "door_main";
    doors_[yard.id] = yard;
    Door pantry;
    pantry.id = "door_pantry";
    pantry.name = "Pantry door";
    pantry.cellId = "tavern";
    pantry.position = {23.5, 6.5};
    doors_[pantry.id] = pantry;
    Door upper;
    upper.id = "stairs_up";
    upper.name = "Loft steps";
    upper.cellId = "tavern";
    upper.position = {28.5, 4.5};
    upper.open = true;
    upper.portal = true;
    upper.targetCell = "loft";
    upper.arrival = {3.5, 9.5};
    upper.linkedDoor = "stairs_down";
    doors_[upper.id] = upper;
    Door down = upper;
    down.id = "stairs_down";
    down.name = "Common-room steps";
    down.cellId = "loft";
    down.position = {3.5, 10.5};
    down.targetCell = "tavern";
    down.arrival = {28.5, 5.5};
    down.linkedDoor = "stairs_up";
    doors_[down.id] = down;
    struct Resident
    {
        const char* id;
        const char* name;
        const char* cell;
        Vec2 p;
        const char* description;
        const char* activity;
        int color;
    };
    const Resident residents[] = {
        {"npc_keeper",
         "Rowan",
         "tavern",
         {9.5, 6.5},
         "A broad, ash-gray wolf, patient and attentive, with a worn herb pouch at the shoulder.",
         "tending the hearth",
         3},
        {"npc_scout",
         "Bracken",
         "tavern",
         {19.5, 12.5},
         "A lean russet wolf with alert ears, a rain-dark harness, and an irrepressible curiosity.",
         "watching the room",
         9},
        {"npc_cook",
         "Moss",
         "tavern",
         {26.5, 6.5},
         "A stocky cream-coated wolf who treats food and hospitality with equal seriousness.",
         "sorting stores",
         12},
        {"npc_porter",
         "Flint",
         "exterior",
         {17.5, 7.5},
         "A charcoal wolf in a sturdy pack harness, practical and quietly sociable.",
         "checking the path",
         20},
        {"npc_smith",
         "Ash",
         "exterior",
         {11.5, 14.5},
         "A scarred silver wolf carrying a neatly balanced roll of tools.",
         "mending a harness clasp",
         23},
        {"npc_scribe",
         "Vale",
         "loft",
         {8.5, 6.5},
         "A pale, soft-spoken wolf with ink-stained writing tools in a fitted side pouch.",
         "studying old accounts",
         28}};
    for (const auto& r : residents)
    {
        Entity e;
        e.id = r.id;
        e.name = r.name;
        e.cellId = r.cell;
        e.position = r.p;
        e.npc = true;
        e.age = std::string(r.id) == "npc_scribe" ? 71 : std::string(r.id) == "npc_smith" ? 57 : 32;
        e.lastBirthdayDay = calendarDays_;
        e.description = r.description;
        e.activity = r.activity;
        e.speakingColor = r.color;
        entities_[e.id] = e;
    }
    rebuildFixtureIndex();
}

void World::rebuildFixtureIndex()
{
    blockingFixtures_.clear();
    doorsIn_.clear();
    portalsIn_.clear();
    portalGrid_.clear();
    neighborCache_.clear();
    stepsCache_.clear();
    for (auto& entry : doors_)
        doorsIn_[entry.second.cellId].push_back(&entry.second);
    for (const auto& entry : doors_)
    {
        const auto& d = entry.second;
        if (!d.passage && d.id.rfind("stairs_", 0) != 0)
            blockingFixtures_[d.cellId][{int(std::floor(d.position.x)), int(std::floor(d.position.y))}].push_back(d.id);
    }
}

void World::indexSeams(const std::string& cellId, const std::vector<std::string>& seamIds)
{
    // What rebuildFixtureIndex() would give, for the one cell whose seams just arrived: its door list in ID order.
    // Seams are open passages, so they never block sight or movement (blockingFixtures_), and every one leads where
    // the cell's exits already said, so neither its neighbours nor any route changes (see firstSteps()). A seam
    // that leads somewhere else means the manifest is out of step: then everything is rebuilt, as it used to be.
    const auto exits = exits_.find(cellId);
    portalGrid_.erase(cellId);
    auto& list = doorsIn_[cellId];
    for (const auto& id : seamIds)
    {
        auto& d = doors_.at(id);
        if (exits == exits_.end() || !exits->second.count(d.targetCell))
        {
            rebuildFixtureIndex();
            return;
        }
        list.push_back(&d);
    }
    std::sort(list.begin(), list.end(), [](const Door* a, const Door* b) { return a->id < b->id; });
}

bool World::blockedByDoor(const std::string& cellId, Vec2 point) const
{
    const auto blockers = blockingFixtures_.find(cellId);
    if (blockers == blockingFixtures_.end())
        return false;
    const auto tile = blockers->second.find({int(std::floor(point.x)), int(std::floor(point.y))});
    if (tile == blockers->second.end())
        return false;
    for (const auto& id : tile->second)
    {
        const auto* d = door(id);
        if (d && !d->open)
            return true;
    }
    return false;
}

Entity& World::addPlayer(const std::string& id, const std::string& name)
{
    auto existing = entities_.find(id);
    if (existing != entities_.end())
        return existing->second;
    ensureLoaded(spawnCell_);
    Entity e;
    e.id = id;
    e.name = name;
    e.cellId = spawnCell_;
    e.position = spawnPosition_;
    e.description = "A wolf whose story is still being written.";
    e.lastBirthdayDay = calendarDays_;
    society_.addPlayer(id);
    auto& stored = entities_.emplace(id, e).first->second;
    observe(id);
    return stored;
}
bool World::removePlayer(const std::string& id)
{
    const auto it = entities_.find(id);
    if (it == entities_.end() || it->second.npc)
        return false;
    entities_.erase(it);
    pendingPortals_.erase(id);
    travels_.erase(id);
    lastObserved_.erase(id);
    travelLegCells_.erase(id);
    travelRetryAt_.erase(id);
    travelProgress_.erase(id);
    return true;
}
Entity* World::entity(const std::string& id)
{
    const auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : &it->second;
}
const Entity* World::entity(const std::string& id) const
{
    const auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : &it->second;
}
Cell* World::cell(const std::string& id)
{
    const auto it = cells_.find(id);
    return it == cells_.end() ? nullptr : &it->second;
}
const Cell* World::cell(const std::string& id) const
{
    const auto it = cells_.find(id);
    return it == cells_.end() ? nullptr : &it->second;
}
const std::vector<Door*>& World::doorsIn(const std::string& cellId) const
{
    static const std::vector<Door*> none;
    const auto found = doorsIn_.find(cellId);
    return found == doorsIn_.end() ? none : found->second;
}

const std::vector<std::string>& World::neighborList(const std::string& cellId) const
{
    auto found = neighborCache_.find(cellId);
    if (found == neighborCache_.end())
    {
        const auto all = neighbors(cellId);
        found = neighborCache_.emplace(cellId, std::vector<std::string>(all.begin(), all.end())).first;
    }
    return found->second;
}

std::set<std::string> World::neighbors(const std::string& cellId) const
{
    std::set<std::string> out;
    for (const auto* d : doorsIn(cellId))
        if (d->portal)
            out.insert(d->targetCell);
    const auto seams = exits_.find(cellId);
    if (seams != exits_.end())
        out.insert(seams->second.begin(), seams->second.end());
    return out;
}

const std::map<std::string, std::string>& World::cachedSteps(const std::string& from) const
{
    auto found = stepsCache_.find(from);
    if (found == stepsCache_.end())
        found = stepsCache_.emplace(from, firstSteps(from)).first;
    return found->second;
}

std::map<std::string, std::string> World::firstSteps(const std::string& from) const
{
    std::map<std::string, std::string> first{{from, ""}};
    std::queue<std::string> pending;
    pending.push(from);
    while (!pending.empty())
    {
        const auto current = pending.front(); pending.pop();
        const auto visit = [&](const std::string& next) {
            if (first.count(next) || deadEnds_.count({current, next}))
                return;
            first[next] = current == from ? next : first[current];
            pending.push(next);
        };
        // A streamed cell's seams are in its exits whether or not it is loaded; its loaded seam records are left
        // to them, so a route never depends on which cells happen to be in memory (and loading one changes none).
        const auto seams = exits_.find(current);          // Seams (always open).
        for (const auto* d : doorsIn(current))
            if (d->portal && !d->locked &&
                !(d->passage && d->boundary && seams != exits_.end() && seams->second.count(d->targetCell)))
                visit(d->targetCell);
        if (seams != exits_.end())
            for (const auto& next : seams->second)
                visit(next);
    }
    return first;
}

const Door* World::door(const std::string& id) const
{
    const auto it = doors_.find(id);
    return it == doors_.end() ? nullptr : &it->second;
}

namespace
{
constexpr double FreeStep = 0.5, RampStep = 1.0, EyeHeight = 0.8, SightTarget = 0.5;
bool ramp(const Tile* t)
{
    return t && (t->terrain == Terrain::Slope || t->terrain == Terrain::Stairs);
}
bool stepAllowed(const Tile* from, const Tile& to)
{
    const double rise = std::abs(to.height - (from ? from->height : 0.0));
    return rise <= FreeStep + 1e-6 || (rise <= RampStep + 1e-6 && (ramp(from) || ramp(&to)));
}
} // namespace

bool World::passable(const std::string& cellId, Vec2 p, const Tile* from) const
{
    const auto* c = cell(cellId);
    if (!c || !finite(p))
        return false;
    for (Vec2 s :
         {p, Vec2{p.x - Radius, p.y}, Vec2{p.x + Radius, p.y}, Vec2{p.x, p.y - Radius}, Vec2{p.x, p.y + Radius}})
    {
        const auto* t = c->tile(int(std::floor(s.x)), int(std::floor(s.y)));
        if (!t || t->solid || !stepAllowed(from, *t))
            return false;
        if (blockedByDoor(cellId, s))
            return false;
    }
    return true;
}

int World::regionAt(const Cell& c, Vec2 point) const
{
    const int x = int(std::floor(point.x)), y = int(std::floor(point.y));
    const auto* map = regionMap(c);
    return !map || x < 0 || y < 0 || x >= c.width || y >= c.height ? -1 : (*map)[std::size_t(y * c.width + x)];
}

const std::vector<int>* World::regionMap(const Cell& c) const
{
    if (!c.loaded || c.tiles.size() != std::size_t(c.width * c.height))
        return nullptr;
    if (const auto kept = regions_.find(c.id); ticking_ && kept != regions_.end() && kept->second.checkedTick == ticks_ &&
                                               kept->second.region.size() == c.tiles.size())
        return &kept->second.region;
    std::uint64_t checksum = 1469598103934665603ULL;
    for (const auto& t : c.tiles)
    {
        checksum = (checksum ^ std::uint64_t(static_cast<unsigned char>(t.glyph))) * 1099511628211ULL;
        checksum = (checksum ^ std::uint64_t(std::int64_t(std::llround(t.height * 2)) + 64 + (t.solid ? 1024 : 0))) *
                   1099511628211ULL;
    }
    auto& r = regions_[c.id];
    r.checkedTick = ticking_ ? ticks_ : 0;
    if (r.region.size() != c.tiles.size() || r.checksum != checksum)
    {
        // New ground: what was known of its connections may no longer hold.
        bool forgot = false;
        for (auto it = deadEnds_.begin(); it != deadEnds_.end();)
            if (it->first == c.id || it->second == c.id)
                it = deadEnds_.erase(it), forgot = true;
            else
                ++it;
        if (forgot)
            stepsCache_.clear();
        r.checksum = checksum;
        r.region.assign(c.tiles.size(), -1);
        int next = 0;
        std::vector<int> frontier;
        for (int start = 0; start < int(c.tiles.size()); ++start)
        {
            if (r.region[start] >= 0 || c.tiles[start].solid)
                continue;
            r.region[start] = next;
            frontier.assign(1, start);
            while (!frontier.empty())
            {
                const int i = frontier.back();
                frontier.pop_back();
                const int ix = i % c.width, iy = i / c.width;
                for (const auto [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}})
                {
                    const int nx = ix + dx, ny = iy + dy, n = ny * c.width + nx;
                    if (nx < 0 || ny < 0 || nx >= c.width || ny >= c.height || r.region[n] >= 0 || c.tiles[n].solid ||
                        !stepAllowed(&c.tiles[i], c.tiles[n]))
                        continue;
                    r.region[n] = next;
                    frontier.push_back(n);
                }
            }
            ++next;
        }
        std::vector<std::size_t> sizes(std::size_t(next), 0);
        for (const int k : r.region)
            if (k >= 0)
                ++sizes[std::size_t(k)];
        r.main = sizes.empty() ? -1 : int(std::max_element(sizes.begin(), sizes.end()) - sizes.begin());
    }
    return &r.region;
}

int World::mainRegion(const Cell& c) const
{
    return regionMap(c) ? regions_.at(c.id).main : -1;
}

bool World::lineOfSight(const std::string& cellId, Vec2 from, Vec2 to) const
{
    const auto* c = cell(cellId);
    if (!c)
        return false;
    const auto fixtures = blockingFixtures_.find(cellId);
    return lineOfSight(*c, fixtures == blockingFixtures_.end() ? nullptr : &fixtures->second, nullptr, from, to);
}
bool World::lineOfSight(const Cell& cell, const FixtureTiles* fixtures, const std::vector<char>* fixtureMask, Vec2 from,
                        Vec2 to) const
{
    const auto* c = &cell;
    if (!finite(from) || !finite(to))
        return false;
    if (from.x < 0 || from.y < 0 || from.x >= c->width || from.y >= c->height || to.x < 0 || to.y < 0 ||
        to.x >= c->width || to.y >= c->height)
        return false;
    // Sight runs from the observer's eye to the middle of the target's tile. Ground, and whatever stands on it,
    // rising above that line hides what lies beyond: the far side of a hill, a plateau above a cliff, a thicket.
    const auto* fromTile = c->tile(int(std::floor(from.x)), int(std::floor(from.y)));
    const auto* toTile = c->tile(int(std::floor(to.x)), int(std::floor(to.y)));
    const double eye = (fromTile ? fromTile->height : 0.0) + EyeHeight;
    // A tall target (a tree, a statue) can show its top over ground that hides its foot.
    const double target = (toTile ? toTile->height + std::max(SightTarget, toTile->stature) : SightTarget);
    const int toX = int(std::floor(to.x)), toY = int(std::floor(to.y));
    const int count = std::max(1, int(std::ceil(distance(from, to) / 0.12)));
    // The line is sampled every 0.12 tiles: sample i of count lies at fraction i / count of the way. Samples a
    // fraction of a tile apart mostly share a tile, and along a straight line the samples in one tile are
    // consecutive, so each tile crossed is checked once for the whole run of samples in it. Within a run only the
    // height of the sight line changes, and it changes steadily with f, so the lowest point of the line over that
    // tile is at the run's first sample when the line climbs and its last when it falls: ground that rises above
    // the line at any sample in the tile rises above it there. The result is exactly that of checking every sample.
    const double dx = to.x - from.x, dy = to.y - from.y;
    const auto at = [&](int i, bool y) {
        const double f = double(i) / count;
        return y ? from.y + dy * f : from.x + dx * f;
    };
    // The first sample after `i` that lies beyond tile coordinate `tileAt` along one axis (count if none does).
    const auto leaves = [&](int i, int tileAt, double delta, bool y) {
        if (delta == 0)
            return count;
        const double origin = y ? from.y : from.x;
        const double edge = delta > 0 ? tileAt + 1.0 : double(tileAt);
        const double guess = std::ceil((edge - origin) / delta * count);
        int next = !(guess < count) ? count : guess <= i ? i + 1 : int(guess);
        const auto beyond = [&](int k) { return delta > 0 ? at(k, y) >= edge : at(k, y) < edge; };
        while (next > i + 1 && beyond(next - 1))
            --next;
        while (next < count && !beyond(next))
            ++next;
        return next;
    };
    for (int i = 1; i < count;)
    {
        const double px = at(i, false), py = at(i, true);
        const int x = int(std::floor(px)), y = int(std::floor(py));
        const int end = std::min(leaves(i, x, dx, false), leaves(i, y, dy, true));  // One past this tile's run.
        // The occluding destination itself is visible, without revealing beyond it.
        if (x != toX || y != toY)
        {
            const auto* t = c->tile(x, y);
            if (!t || t->opaque)
                return false;
            const bool fixture = fixtureMask ? (*fixtureMask)[std::size_t(y * c->width + x)] != 0
                                             : fixtures && fixtures->count({x, y});
            if (fixture && blockedByDoor(c->id, {px, py}))
                return false;
            if (t != fromTile)
            {
                const double f = double(target - eye >= 0 ? i : end - 1) / count;
                if (t->height + t->stature > eye + (target - eye) * f + 1e-6)
                    return false;
            }
        }
        i = end;
    }
    return true;
}

double World::sightRange(const Entity& o) const
{
    return 27.0 * std::max(0.0, o.vision) * ageVisionFactor(o) * clamp01(o.eyeHealth) * environmentAt(o.cellId, o.position).sight;
}
bool World::visiblePoint(const Entity& o, Vec2 p) const
{
    return distance(o.position, p) <= sightRange(o) && lineOfSight(o.cellId, o.position, p);
}
std::vector<char> World::visibleTileMask(const Entity& o, const Cell& c, double range) const
{
    std::vector<char> out(c.tiles.size(), 0);
    if (!(range > 0))
        return out;
    const int x0 = std::max(0, int(std::floor(o.position.x - range))), x1 = std::min(c.width - 1, int(std::ceil(o.position.x + range)));
    const int y0 = std::max(0, int(std::floor(o.position.y - range))), y1 = std::min(c.height - 1, int(std::ceil(o.position.y + range)));
    const Cell* sightCell = o.cellId == c.id ? &c : cell(o.cellId);
    if (!sightCell)
        return out;
    // Which tiles hold a closable fixture, as a flat mask: the rays below ask about thousands of tiles.
    std::vector<char> fixtureTiles;
    if (const auto fixtures = blockingFixtures_.find(o.cellId); fixtures != blockingFixtures_.end())
    {
        fixtureTiles.assign(sightCell->tiles.size(), 0);
        for (const auto& [at, ids] : fixtures->second)
            if (at.first >= 0 && at.second >= 0 && at.first < sightCell->width && at.second < sightCell->height)
                fixtureTiles[std::size_t(at.second * sightCell->width + at.first)] = 1;
    }
    const auto* mask = fixtureTiles.empty() ? nullptr : &fixtureTiles;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
        {
            const Vec2 p{x + .5, y + .5};
            if (distance(o.position, p) <= range && lineOfSight(*sightCell, nullptr, mask, o.position, p))
                out[std::size_t(y * c.width + x)] = 1;
        }
    return out;
}
World::View World::viewKey(const Entity& o, const Cell& c, double range) const
{
    std::uint64_t doors = 1469598103934665603ULL;
    for (const Door* d : doorsIn(c.id))
        if (!d->passage)
            doors = (doors ^ std::uint64_t(d->open)) * 1099511628211ULL;
    return {c.id, std::llround(o.position.x * 50), std::llround(o.position.y * 50), std::llround(range * 100), doors,
            std::uint64_t(c.tiles.size()), {}};
}
const std::vector<char>& World::viewOf(const Entity& o, const Cell& c, bool* fresh) const
{
    const double range = sightRange(o);
    const View key = viewKey(o, c, range);
    auto& v = views_[o.id];
    const bool same = v.cellId == key.cellId && v.x == key.x && v.y == key.y && v.range == key.range &&
                      v.doors == key.doors && v.tiles == key.tiles && v.visible.size() == c.tiles.size();
    if (!same)
    {
        v = key;
        v.visible = visibleTileMask(o, c, range);
    }
    if (fresh)
        *fresh = !same;
    return v.visible;
}
void World::prepareViews(const std::vector<std::string>& observerIds) const
{
    struct Job
    {
        const Entity* observer;
        const Cell* cell;
        double range;
        View key;
    };
    std::vector<Job> jobs;
    for (const auto& id : observerIds)
    {
        const auto* o = entity(id);
        const auto* c = o ? cell(o->cellId) : nullptr;
        if (!o || !c)
            continue;
        const double range = sightRange(*o);
        View key = viewKey(*o, *c, range);
        const auto cached = views_.find(id);
        if (cached != views_.end() && cached->second.cellId == key.cellId && cached->second.x == key.x &&
            cached->second.y == key.y && cached->second.range == key.range && cached->second.doors == key.doors &&
            cached->second.tiles == key.tiles && cached->second.visible.size() == c->tiles.size())
            continue;
        jobs.push_back({o, c, range, std::move(key)});
    }
    if (jobs.empty())
        return;
    // Only reads happen on the helpers (tiles, doors, fixtures); each writes its own result.
    const std::size_t threads = std::min<std::size_t>(jobs.size(), std::max(1u, std::min(8u, std::thread::hardware_concurrency())));
    if (threads > 1)
    {
        std::vector<std::thread> helpers;
        for (std::size_t t = 1; t < threads; ++t)
            helpers.emplace_back([&, t] {
                for (std::size_t j = t; j < jobs.size(); j += threads)
                    jobs[j].key.visible = visibleTileMask(*jobs[j].observer, *jobs[j].cell, jobs[j].range);
            });
        for (std::size_t j = 0; j < jobs.size(); j += threads)
            jobs[j].key.visible = visibleTileMask(*jobs[j].observer, *jobs[j].cell, jobs[j].range);
        for (auto& helper : helpers)
            helper.join();
    }
    else
        jobs[0].key.visible = visibleTileMask(*jobs[0].observer, *jobs[0].cell, jobs[0].range);
    for (auto& job : jobs)
        views_[job.observer->id] = std::move(job.key);
}
bool World::visiblePortal(const Entity& o, const Door& d) const
{
    return d.cellId == o.cellId && d.portal && d.open && visiblePoint(o, d.position);
}

std::vector<Vec2> World::findPath(const Entity& a, Vec2 goal, bool allowClosed) const
{
    const auto* c = cell(a.cellId);
    const auto* regions = c && finite(goal) && finite(a.position) ? regionMap(*c) : nullptr;
    if (!regions)
        return searchPath(a, goal, allowClosed);
    std::uint64_t closed = 1469598103934665603ULL;
    if (const auto fixtures = blockingFixtures_.find(a.cellId); !allowClosed && fixtures != blockingFixtures_.end())
        for (const auto& [at, ids] : fixtures->second)
            for (const auto& id : ids)
                if (const auto* d = door(id); d && !d->open)
                    for (const unsigned char ch : id + '\n')
                        closed = (closed ^ ch) * 1099511628211ULL;
    PathKey key{a.cellId, a.position.x, a.position.y, goal.x, goal.y, allowClosed, regions_.at(c->id).checksum, closed};
    if (const auto found = pathCache_.find(key); found != pathCache_.end())
    {
        ++pathHits_;
        return found->second;
    }
    ++pathMisses_;
    auto path = searchPath(a, goal, allowClosed);
    if (pathCache_.size() >= PathsKept)
        pathCache_.clear();                         // Simple and rare: the everyday ways are soon found again.
    pathCache_.emplace(std::move(key), path);
    return path;
}

std::vector<Vec2> World::searchPath(const Entity& a, Vec2 goal, bool allowClosed) const
{
    const auto* c = cell(a.cellId);
    if (!c || !finite(goal))
        return {};
    if (goal.x < 0 || goal.y < 0 || goal.x >= c->width || goal.y >= c->height || a.position.x < 0 || a.position.y < 0 ||
        a.position.x >= c->width || a.position.y >= c->height)
        return {};
    const int w = c->width * NavScale, h = c->height * NavScale;
    auto pos = [w](int i) { return Vec2{(i % w + .5) / NavScale, (i / w + .5) / NavScale}; };
    auto index = [w, h](Vec2 p) {
        const int x = int(std::floor(p.x * NavScale)), y = int(std::floor(p.y * NavScale));
        return x < 0 || y < 0 || x >= w || y >= h ? -1 : y * w + x;
    };
    const int start = index(a.position), end = index(goal);
    if (start < 0 || end < 0)
        return {};
    // A goal in another region can never be reached: say so at once instead of searching everything reachable.
    if (const int from = regionAt(*c, a.position), to = regionAt(*c, goal); from >= 0 && to >= 0 && from != to)
        return {};
    // passable() for this one cell, with its lookups done once: a search asks millions of times on a large cell.
    // The tiles a closed door stands in, as a flat mask, rather than looking every sample up among the fixtures.
    std::vector<char> closedTiles;
    if (const auto fixtures = blockingFixtures_.find(a.cellId); !allowClosed && fixtures != blockingFixtures_.end())
        for (const auto& [at, ids] : fixtures->second)
            if (at.first >= 0 && at.second >= 0 && at.first < c->width && at.second < c->height)
                for (const auto& id : ids)
                    if (const auto* d = door(id); d && !d->open)
                    {
                        closedTiles.resize(c->tiles.size(), 0);
                        closedTiles[std::size_t(at.second * c->width + at.first)] = 1;
                    }
    const bool anyClosed = !closedTiles.empty();
    auto allowed = [&](Vec2 p, const Tile* from) {
        // The footprint is tiny (Radius): nearly always all five points are on one tile, and one look will do.
        const int cx = int(std::floor(p.x)), cy = int(std::floor(p.y));
        if (int(std::floor(p.x - Radius)) == cx && int(std::floor(p.x + Radius)) == cx &&
            int(std::floor(p.y - Radius)) == cy && int(std::floor(p.y + Radius)) == cy)
        {
            const auto* t = c->tile(cx, cy);
            return t && !t->solid && stepAllowed(from, *t) && !(anyClosed && closedTiles[std::size_t(cy * c->width + cx)]);
        }
        for (Vec2 s :
             {p, Vec2{p.x - Radius, p.y}, Vec2{p.x + Radius, p.y}, Vec2{p.x, p.y - Radius}, Vec2{p.x, p.y + Radius}})
        {
            const int tx = int(std::floor(s.x)), ty = int(std::floor(s.y));
            const auto* t = c->tile(tx, ty);
            if (!t || t->solid || !stepAllowed(from, *t))
                return false;
            if (anyClosed && closedTiles[std::size_t(ty * c->width + tx)])
                return false;
        }
        return true;
    };
    // allowed() for a node's centre, which the search asks of each node many times over (once for every neighbour
    // it is reached from, and again for diagonal corners). A node's footprint is five points on at most five tiles;
    // what they are is found once per search. Off ramps, a step is allowed when no footprint tile rises or falls
    // more than FreeStep from where the step starts, which is so exactly when the lowest and highest of them don't
    // (the rise is steady in the height on either side of it); with a ramp in play it is asked in full as before.
    enum : std::uint8_t { FootprintBlocked = 1, FootprintRamp = 2 };
    auto allowedNode = [&](int node, Vec2 p, const Tile* from) {
        if (nav_.footprintSearch[std::size_t(node)] != nav_.search)
        {
            std::uint8_t flags = 0;
            double low = std::numeric_limits<double>::infinity(), high = -low;
            for (Vec2 s :
                 {p, Vec2{p.x - Radius, p.y}, Vec2{p.x + Radius, p.y}, Vec2{p.x, p.y - Radius}, Vec2{p.x, p.y + Radius}})
            {
                const int tx = int(std::floor(s.x)), ty = int(std::floor(s.y));
                const auto* t = c->tile(tx, ty);
                if (!t || t->solid || (anyClosed && closedTiles[std::size_t(ty * c->width + tx)]))
                {
                    flags |= FootprintBlocked;
                    break;
                }
                if (ramp(t))
                    flags |= FootprintRamp;
                low = std::min(low, t->height);
                high = std::max(high, t->height);
            }
            nav_.footprintSearch[std::size_t(node)] = nav_.search;
            nav_.footprintFlags[std::size_t(node)] = flags;
            nav_.footprintLow[std::size_t(node)] = low;
            nav_.footprintHigh[std::size_t(node)] = high;
        }
        const auto flags = nav_.footprintFlags[std::size_t(node)];
        if (flags & FootprintBlocked)
            return false;
        if ((flags & FootprintRamp) || !from || ramp(from))
            return allowed(p, from);
        const double base = from->height;
        return std::abs(nav_.footprintHigh[std::size_t(node)] - base) <= FreeStep + 1e-6 &&
               std::abs(nav_.footprintLow[std::size_t(node)] - base) <= FreeStep + 1e-6;
    };
    const auto* dest = c->tile(int(goal.x), int(goal.y));
    if (!dest || !allowed(goal, dest))
        return {};
    using QueueItem = std::pair<double, int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> open;
    const auto nodes = static_cast<std::size_t>(w * h);
    if (nav_.seen.size() < nodes)
    {
        nav_.g.resize(nodes);
        nav_.previous.resize(nodes);
        nav_.seen.assign(nodes, 0);
        nav_.closed.assign(nodes, 0);
        nav_.footprintSearch.assign(nodes, 0);
        nav_.footprintFlags.resize(nodes);
        nav_.footprintLow.resize(nodes);
        nav_.footprintHigh.resize(nodes);
        nav_.search = 0;
    }
    if (++nav_.search == 0)
    {
        std::fill(nav_.seen.begin(), nav_.seen.end(), 0);
        std::fill(nav_.closed.begin(), nav_.closed.end(), 0);
        std::fill(nav_.footprintSearch.begin(), nav_.footprintSearch.end(), 0);
        nav_.search = 1;
    }
    const std::uint32_t search = nav_.search;
    auto gOf = [&](int i) { return nav_.seen[i] == search ? nav_.g[i] : std::numeric_limits<double>::infinity(); };
    auto previousOf = [&](int i) { return nav_.seen[i] == search ? nav_.previous[i] : -1; };
    auto isClosed = [&](int i) { return nav_.closed[i] == search; };
    nav_.seen[start] = search;
    nav_.g[start] = 0;
    nav_.previous[start] = -1;
    open.push({SearchGreed * distance(pos(start), goal), start});
    while (!open.empty())
    {
        const int current = open.top().second;
        open.pop();
        if (isClosed(current))
            continue;
        if (current == end)
            break;
        nav_.closed[current] = search;
        ++searchExpanded_;
        const Vec2 p = pos(current);
        const auto* tile = c->tile(int(p.x), int(p.y));
        if (!tile)
            continue;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (!dx && !dy)
                    continue;
                const int nx = current % w + dx, ny = current / w + dy;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                    continue;
                const int next = ny * w + nx;
                const Vec2 q = pos(next);
                if (isClosed(next) || !allowedNode(next, q, tile))
                    continue;
                // The diagonal's corners: {p.x, q.y} is the node in this column and the next row, and so on.
                if (dx && dy &&
                    (!allowedNode(ny * w + current % w, {p.x, q.y}, tile) || !allowedNode(current / w * w + nx, {q.x, p.y}, tile)))
                    continue;
                const auto* nt = c->tile(int(q.x), int(q.y));
                const double cost = distance(p, q) * nt->movementCost + std::abs(nt->height - tile->height) * .3;
                const double through = gOf(current) + cost;
                if (through + Epsilon < gOf(next))
                {
                    nav_.seen[next] = search;
                    nav_.g[next] = through;
                    nav_.previous[next] = current;
                    open.push({through + SearchGreed * distance(q, goal), next});
                }
            }
    }
    if (start != end && previousOf(end) < 0)
        return {};
    std::vector<Vec2> path;
    for (int i = end; i != start; i = previousOf(i))
    {
        if (i < 0)
            return {};
        path.push_back(pos(i));
    }
    std::reverse(path.begin(), path.end());
    if (path.empty() || distance(path.back(), goal) > .01)
        path.push_back(goal);
    if (!allowClosed && path.size() > 1)
    {
        // Remove the visible quarter-grid zigzag, preserving static clearance
        // and approximately preserving the terrain cost chosen by A*. Looking a bounded way ahead and stopping at
        // the first blocked line keeps this linear in the path's length; long city walks made it cubic.
        std::vector<Vec2> smooth;
        Vec2 anchor = a.position;
        std::size_t first = 0;
        while (first < path.size())
        {
            std::size_t chosen = first;
            double routeCost = 0;
            Vec2 priorWaypoint = anchor;
            for (std::size_t candidate = first; candidate < path.size() && candidate < first + SmoothLookahead;
                 ++candidate)
            {
                const auto* rt = c->tile(int(path[candidate].x), int(path[candidate].y));
                routeCost += distance(priorWaypoint, path[candidate]) * rt->movementCost;
                priorWaypoint = path[candidate];
                const int samples = std::max(1, int(std::ceil(distance(anchor, path[candidate]) / .1)));
                Vec2 prior = anchor;
                double directCost = 0;
                bool clear = true;
                for (int sample = 1; sample <= samples; ++sample)
                {
                    const double f = double(sample) / samples;
                    const Vec2 p{anchor.x + (path[candidate].x - anchor.x) * f,
                                 anchor.y + (path[candidate].y - anchor.y) * f};
                    const auto* pt = c->tile(int(prior.x), int(prior.y));
                    if (!pt || !allowed(p, pt))
                    {
                        clear = false;
                        break;
                    }
                    const auto* nt = c->tile(int(p.x), int(p.y));
                    directCost += distance(prior, p) * nt->movementCost;
                    prior = p;
                }
                if (!clear)
                    break;
                if (directCost <= routeCost + .025)
                    chosen = candidate;
            }
            smooth.push_back(path[chosen]);
            anchor = path[chosen];
            first = chosen + 1;
        }
        path = std::move(smooth);
    }
    return path;
}

Result World::move(const std::string& id, double dx, double dy)
{
    auto* a = entity(id);
    if (!a)
        return {false, "Unknown actor.", {}};
    if (a->dead)
        return {false, "You are dead.", {}};
    if (!finite({dx, dy}))
        return {false, "Movement must be finite.", {}};
    if (!issuingTravel_ && length({dx, dy}) > Epsilon)
        cancelTravel(id);
    else if (!issuingTravel_ && travelState(id).active)
        return {true, "Travel continues.", {}};
    a->path.clear();
    pendingPortals_.erase(id);
    a->input = length({dx, dy}) > 1.0 ? normalized({dx, dy}) : Vec2{dx, dy};
    if (length(a->input) > Epsilon)
    {
        a->turning = false;
        prepareMovement(*a);
    }
    else
        a->velocity = {};
    a->transitioned = false;
    return {true, "Movement accepted.", {}};
}
Result World::setDead(const std::string& id, bool dead)
{
    auto* a = entity(id);
    if (!a)
        return {false, "Unknown actor.", {}};
    if (a->dead == dead)
        return {false, dead ? a->name + " is already dead." : a->name + " is not dead.", {}};
    stop(id);
    a->dead = dead;
    a->posture = dead ? "lying" : "standing";
    a->postureTarget.clear();
    a->postureRemaining = 0;
    a->state = dead ? "dead" : "";
    a->activity = dead ? "dead" : "";
    a->typing = false;
    recordEvent({dead ? "death" : "revival", id, {}, {}, 0, 0, {}, 0, 0, {}});
    if (a->npc || society_.account(id))
    {
        careerNotes(dead ? society_.died(id, calendarDays_) : society_.revived(id));
        if (dead)
            // Those who were close grieve: the family for days, friends a little while.
            for (const auto& [otherId, other] : entities_)
            {
                if (otherId == id || other.dead || !other.npc)
                    continue;
                const auto* b = bonds_.find(otherId, id);
                const bool kin = society_.family(otherId, id);
                if (kin || (b && (b->affinity >= 20 || b->familiarity >= 40)))
                {
                    society_.mourn(otherId, id, calendarDays_ + (kin ? 5 : 2));
                    recordEvent({"mourning", otherId, id, {}, 0, 0, {}, 0, 0, kin ? "family" : "friend"});
                }
            }
    }
    return {true, dead ? a->name + " is dead." : a->name + " lives again.", {}};
}

Result World::adoptResident(const World& candidate, const std::string& id)
{
    const auto* spec = candidate.society_.spec(id);
    const auto it = entities_.find(id);
    if (it != entities_.end() && !it->second.npc)
        return {false, "That is a player character, not an NPC.", {}};
    if (!society_.adoptResident(candidate.society_, id))
        return {false, "No such NPC.", {}};
    if (!spec)
    {
        const std::string name = it != entities_.end() ? it->second.name : id;
        if (it != entities_.end())
            entities_.erase(it);
        pendingPortals_.erase(id);
        travels_.erase(id);
        bonds_.forget(id);
        return {true, name + " has left the world.", {}};
    }
    const auto* fresh = candidate.entity(id);
    if (!fresh)
        return {false, "The candidate world has no body for " + id + ".", {}};
    if (it == entities_.end())
    {
        entities_[id] = *fresh;
        return {true, spec->name + " has joined the world.", {}};
    }
    auto& e = it->second;
    e.name = fresh->name;
    e.description = fresh->description;
    e.appearance = fresh->appearance;
    e.speakingColor = fresh->speakingColor;
    if (!e.dead)
    {
        e.activity = fresh->activity;
        stop(id);                                   // Re-plan with the new schedule and places.
    }
    return {true, spec->name + " is updated.", {}};
}

void World::adoptLayers(const World& candidate)
{
    society_.adoptLayers(candidate.society_);
}

void World::adoptFactions(const World& candidate)
{
    factions_ = candidate.factions_;
    for (auto& [id, place] : cells_)
    {
        const auto* other = candidate.cell(id);
        place.factionClaims = other ? other->factionClaims : std::vector<std::string>{};
    }
}

Result World::stop(const std::string& id)
{
    auto* a = entity(id);
    if (!a)
        return {false, "Unknown actor.", {}};
    if (!issuingTravel_)
        cancelTravel(id);
    a->path.clear();
    a->input = {};
    a->velocity = {};
    pendingPortals_.erase(id);
    return {true, "Stopped.", {}};
}
Result World::setPace(const std::string& id, int pace)
{
    auto* a = entity(id);
    if (!a)
        return {false, "Unknown actor.", {}};
    if (pace < 0 || pace > 10)
        return {false, "Pace must be a notch from 0 to 10.", {}};
    a->pace = pace;
    return {true, std::string("Travel pace: ") + paceName(pace) + " (" + std::to_string(pace) + "/10).", {}};
}
Result World::face(const std::string& id, double x, double y)
{
    auto* a = entity(id);
    if (!a || !finite({x, y}))
        return {false, "Invalid facing target.", {}};
    if (length(a->input) > Epsilon || !a->path.empty() || length(a->velocity) > Epsilon || pendingPortals_.count(id))
        return {false, "Stop moving to face a point.", {}};
    if (!issuingTravel_)
        cancelTravel(id);
    if (distance(a->position, {x, y}) > Epsilon)
    {
        a->turnTarget = std::atan2(y - a->position.y, x - a->position.x);
        a->turning = std::abs(std::remainder(a->turnTarget - a->facing, 2.0 * Pi)) > Epsilon;
    }
    return {true, "Turning toward that direction.", {}};
}
void World::prepareMovement(Entity& a)
{
    // Repeated held input must not restart an in-progress rise.
    if (a.posture == "sitting" || a.posture == "lying")
    {
        a.postureRemaining = a.posture == "sitting" ? .65 : .45;
        a.postureTarget = a.posture == "sitting" ? "standing" : "crouching";
        a.posture = "rising";
        a.velocity = {};
    }
}
Result World::setPosture(const std::string& id, const std::string& posture)
{
    auto* a = entity(id);
    if (!a || !stablePosture(posture))
        return {false, "Invalid posture.", {}};
    stop(id);
    if ((a->posture == "rising" && a->postureTarget == posture) || a->posture == posture)
        return {true, a->posture == "rising" ? "Already changing posture." : "Posture unchanged.", {}};
    double duration = 0.0;
    if (posture == "standing")
        duration = a->posture == "lying" ? 1.0 : a->posture == "crouching" ? .5 : .65;
    else if (posture == "crouching")
        duration = a->posture == "lying" ? .45 : a->posture == "sitting" ? .65 : 0.0;
    a->postureRemaining = duration;
    a->postureTarget = duration > 0.0 ? posture : std::string{};
    a->posture = duration > 0.0 ? "rising" : posture;
    return {true, duration > 0.0 ? "Changing posture." : "Posture changed.", {}};
}
Result World::moveTo(const std::string& id, double x, double y)
{
    auto* a = entity(id);
    if (!a || !finite({x, y}))
        return {false, "Invalid destination.", {}};
    if (a->dead)
        return {false, "You are dead.", {}};
    const auto* c = cell(a->cellId);
    if (!c)
        return {false, "Missing cell.", {}};
    if (!issuingTravel_)
        cancelTravel(id);
    a->input = {};
    a->path.clear();
    a->velocity = {};
    a->transitioned = false;
    a->turning = false;
    pendingPortals_.erase(id);
    Vec2 requested{x, y};
    Vec2 goal = requested;
    const Door* exit = nullptr;
    // A click on, or just beyond, a boundary portal expresses a crossing.
    for (const Door* door : doorsIn(a->cellId))
    {
        const auto& d = *door;
        if (!d.boundary)
            continue;
        const bool horizontal =
            d.edge == 'N' || d.edge == 'S' || (d.edge == '-' && (d.position.y < 1 || d.position.y > c->height - 1));
        const double lateral = horizontal ? x : y;
        const double anchor = horizontal ? d.position.x : d.position.y;
        const bool along = d.passage ? std::floor(lateral) == std::floor(anchor) : std::abs(lateral - anchor) < .48;
        const bool edge = d.edge == 'N'   ? y <= 1
                          : d.edge == 'S' ? y >= c->height - 1
                          : d.edge == 'W' ? x <= 1
                          : d.edge == 'E'
                              ? x >= c->width - 1
                              : (d.position.y < 1 && y <= 1) || (d.position.y > c->height - 1 && y >= c->height - 1) ||
                                    (d.position.x < 1 && x <= 1) || (d.position.x > c->width - 1 && x >= c->width - 1);
        if (along && edge)
        {
            exit = &d;
            goal = d.position;
            break;
        }
    }
    auto path = findPath(*a, goal);
    if (path.empty())
    {
        auto permissive = findPath(*a, goal, true);
        Vec2 from = a->position;
        for (const auto& p : permissive)
        {
            // Each step is judged from the one before it: on slopes and stairs the ground changes along the way.
            const auto* t = c->tile(int(from.x), int(from.y));
            from = p;
            if (!passable(a->cellId, p, t))
            {
                for (const Door* d : doorsIn(a->cellId))
                    if (!d->open && distance(d->position, p) < 1.1)
                    {
                        a->path = path;
                        if (!a->path.empty())
                            prepareMovement(*a);
                        return {true, "Approaching the closed " + d->name + ". Choose Open to continue.", d->id};
                    }
                break;
            }
            path.push_back(p);
        }
        return {false, "No reachable route to that point.", {}};
    }
    if (exit && exit->open)
    {
        Vec2 beyond = exit->position;
        if (exit->edge == 'N' || (exit->edge == '-' && exit->position.y < 1))
            beyond.y = -.15;
        else if (exit->edge == 'S' || (exit->edge == '-' && exit->position.y > c->height - 1))
            beyond.y = c->height + .15;
        else if (exit->edge == 'W' || (exit->edge == '-' && exit->position.x < 1))
            beyond.x = -.15;
        else
            beyond.x = c->width + .15;
        path.push_back(beyond);
    }
    a->path = std::move(path);
    prepareMovement(*a);
    return {true, "Following a route.", {}};
}

void World::transition(Entity& a, const Door& d)
{
    if (const auto* held = custodyOf(a.id); held && d.targetCell != held->cell)
    {
        if (!a.npc)
            notice(a.id, "The door is barred. The watch holds you here a while yet.");
        stop(a.id);
        return;
    }
    if (!ensureLoaded(d.targetCell).ok)
        return;
    const auto* destination = cell(d.targetCell);
    if (!destination)
        return;
    Vec2 arrival = d.arrival;
    if (d.passage && d.boundary && d.edge != '-')
    {
        // Retain the lateral crossing coordinate, not the center of each
        // authoring tile. Never copy an unsafe offset into the destination.
        if (d.edge == 'N' || d.edge == 'S')
            arrival.x += a.position.x - d.position.x;
        else
            arrival.y += a.position.y - d.position.y;
        const auto* anchor = destination->tile(int(d.arrival.x), int(d.arrival.y));
        if (!anchor || !passable(d.targetCell, arrival, anchor))
            arrival = d.arrival;
    }
    a.cellId = d.targetCell;
    a.position = arrival;
    a.velocity = {};
    a.input = {};
    a.path.clear();
    a.turning = false;
    pendingPortals_.erase(a.id);
    a.transitioned = true;
    if (!a.npc)
        observe(a.id);
}

Result World::interact(const std::string& id, const std::string& target, const std::string& verb)
{
    auto* a = entity(id);
    if (!a)
        return {false, "Unknown actor.", target};
    const auto di = doors_.find(target);
    if (di != doors_.end())
    {
        auto& d = di->second;
        if (d.passage && d.boundary)
            return {false, "That seam is crossed by movement, not interaction.", target};
        if (d.cellId != a->cellId || !visiblePoint(*a, d.position))
            return {false, "You cannot see that here.", target};
        if (verb == "inspect")
            return {true, d.name + (d.open ? " is open." : " is closed."), target};
        if (distance(a->position, d.position) > d.reach)
            return {false, "Move closer to " + d.name + ".", target};
        if (verb == "knock")
            return {true, "You knock against " + d.name + ".", target};
        if (verb == "listen")
            return {true,
                    d.open ? "Sounds drift through the opening." : "Muted sounds pass through the closed barrier.",
                    target};
        if (verb == "open" || (verb == "enter" && d.portal && d.open))
        {
            if (d.locked)
                return {false, "It is locked.", target};
            d.open = true;
            if (!d.linkedDoor.empty() && doors_.count(d.linkedDoor))
                doors_.at(d.linkedDoor).open = true;
            // Opening the barrier named by a journey is the requested manual
            // intervention, not a replacement movement command. The ordinary
            // local-click rule still stops and requires a new destination.
            const auto travel = travels_.find(id);
            const bool continueTravel =
                travel != travels_.end() && travel->second.active && travel->second.nextDoor == target;
            const bool wasIssuingTravel = issuingTravel_;
            issuingTravel_ = issuingTravel_ || continueTravel;
            stop(id);
            issuingTravel_ = wasIssuingTravel;
            if (d.portal)
            {
                prepareMovement(*a);
                if (a->postureRemaining > Epsilon)
                {
                    pendingPortals_[id] = target;
                    return {true, "Changing posture before entering " + cells_.at(d.targetCell).name + ".", target};
                }
                transition(*a, d);
                return {true, "You enter " + cells_.at(d.targetCell).name + ".", target};
            }
            return {true,
                    "You open " + d.name +
                        (continueTravel ? ". Your journey can continue." : ". Choose a new destination to continue."),
                    target};
        }
        if (verb == "close")
        {
            if (d.passage || d.id.find("stairs_") == 0)
                return {false, "The steps have no door to close.", target};
            for (const auto& e : entities_)
                if (e.second.cellId == d.cellId && doorCovers(d, e.second.position))
                    return {false, "Someone is crossing the threshold.", target};
            if (!d.linkedDoor.empty())
            {
                const auto& other = doors_.at(d.linkedDoor);
                for (const auto& e : entities_)
                    if (e.second.cellId == other.cellId && doorCovers(other, e.second.position))
                        return {false, "Someone is crossing the far threshold.", target};
            }
            d.open = false;
            if (!d.linkedDoor.empty() && doors_.count(d.linkedDoor))
                doors_.at(d.linkedDoor).open = false;
            stop(id);
            return {true, "You close " + d.name + ".", target};
        }
        return {false, "That action is unavailable.", target};
    }
    const auto* other = entity(target);
    if (other && visionClarity(id, target) > 0.0)
    {
        if (verb == "inspect")
            return {true,
                    other->name + ": " + other->description + " Currently " + other->posture +
                        (other->state.empty() ? "." : "; " + other->state),
                    target};
        if (verb == "speak")
            return {true, "Address " + other->name + " in your roleplay post.", target};
    }
    return {false, "No available interaction with that target.", target};
}

std::vector<std::string> World::actions(const std::string& id, const std::string& target) const
{
    const auto* a = entity(id);
    if (!a)
        return {};
    const auto* d = door(target);
    if (d && !(d->passage && d->boundary) && d->cellId == a->cellId && visiblePoint(*a, d->position))
    {
        std::vector<std::string> out{"inspect"};
        if (distance(a->position, d->position) <= d->reach)
        {
            out.push_back("listen");
            if (!d->passage && d->id.find("stairs_") != 0)
                out.push_back("knock");
            if (!d->open)
                out.push_back("open");
            else
            {
                if (d->portal)
                    out.push_back("enter");
                if (!d->passage && d->id.find("stairs_") != 0)
                    out.push_back("close");
            }
        }
        return out;
    }
    const auto* e = entity(target);
    if (e && visionClarity(id, target) > 0.0)
        return {"inspect", "speak"};
    return {};
}

void World::integrate(Entity& a, double dt)
{
    if (a.stamina <= Epsilon)
        a.exhausted = true;
    const double elapsed = dt;
    double movedTime = 0.0;
    const Vec2 origin = a.position;
    const auto originCell = a.cellId;
    a.velocity = {};
    // A navigation step can consume several short waypoints. Account for
    // elapsed recovery exactly once, and charge only the time corresponding
    // to accepted translation, not a blocked input or a portal teleport.
    const auto advanceMotion = [&]() {
        const auto* c = cell(a.cellId);
        if (!c)
            return;
        if (a.turning)
        {
            const double difference = std::remainder(a.turnTarget - a.facing, 2.0 * Pi);
            const double turn = TurnSpeed * dt;
            if (std::abs(difference) <= turn + Epsilon)
            {
                a.facing = a.turnTarget;
                a.turning = false;
            }
            else
                a.facing = std::remainder(a.facing + std::copysign(turn, difference), 2.0 * Pi);
        }
        // Transition time consumes simulation time before any movement is allowed.
        if (a.postureRemaining > 0.0)
        {
            const double consumed = std::min(dt, a.postureRemaining);
            a.postureRemaining -= consumed;
            dt -= consumed;
            if (a.postureRemaining <= Epsilon)
            {
                a.postureRemaining = 0.0;
                a.posture = a.postureTarget;
                a.postureTarget.clear();
            }
            if (a.postureRemaining > 0.0)
                return;
        }
        const auto pending = pendingPortals_.find(a.id);
        if (pending != pendingPortals_.end())
        {
            const auto* d = door(pending->second);
            pendingPortals_.erase(pending);
            if (d && d->portal && d->open && d->cellId == a.cellId && distance(a.position, d->position) <= d->reach)
                transition(a, *d);
            return;
        }
        while (dt > Epsilon)
        {
            Vec2 direction = a.input;
            while (!a.path.empty() && distance(a.position, a.path.front()) < Epsilon)
                a.path.erase(a.path.begin());
            if (!a.path.empty())
                direction = normalized({a.path.front().x - a.position.x, a.path.front().y - a.position.y});
            if (length(direction) < Epsilon)
                return;
            a.turning = false;
            // Also cover authoritative scripts that assign an NPC path directly.
            prepareMovement(a);
            if (a.postureRemaining > 0.0)
                return;
            const auto* startTile = c->tile(int(a.position.x), int(a.position.y));
            if (!startTile)
                return;
            double speed = paceSpeed(a) / std::max(.1, startTile->movementCost);
            if (a.posture == "crouching")
                speed *= .30;
            if (a.npc)
                speed *= .57;
            speed *= environmentAt(c->id, a.position).movement;
            double travel = speed * dt;
            if (!a.path.empty())
                travel = std::min(travel, distance(a.position, a.path.front()));
            const Vec2 delta{direction.x * travel, direction.y * travel};
            const Vec2 proposed{a.position.x + delta.x, a.position.y + delta.y};
            // An NPC crosses an open boundary only where its route leads out through that edge: walking along an
            // edge it has just arrived by must not tip it back into the cell it came from.
            const auto leaving = [&](char edge) {
                if (!a.npc || edge == '-')
                    return true;
                if (a.path.empty())
                    return false;
                const auto end = a.path.back();
                return edge == 'N' ? end.y < 0 : edge == 'S' ? end.y > c->height : edge == 'W' ? end.x < 0 : end.x > c->width;
            };
            for (const Door* door : doorsIn(a.cellId))
            {
                const auto& d = *door;
                if (!d.portal || !d.boundary || !d.open || !leaving(d.edge))
                    continue;
                const bool crossed = d.edge == 'N'   ? proposed.y < Radius
                                     : d.edge == 'S' ? proposed.y >= c->height - Radius
                                     : d.edge == 'W' ? proposed.x < Radius
                                     : d.edge == 'E'
                                         ? proposed.x >= c->width - Radius
                                         : (d.position.y < 1 && proposed.y < Radius) ||
                                               (d.position.y > c->height - 1 && proposed.y >= c->height - Radius) ||
                                               (d.position.x < 1 && proposed.x < Radius) ||
                                               (d.position.x > c->width - 1 && proposed.x >= c->width - Radius);
                const bool horizontal = d.edge == 'N' || d.edge == 'S' ||
                                        (d.edge == '-' && (d.position.y < 1 || d.position.y > c->height - 1));
                const double lateral = horizontal ? proposed.x : proposed.y;
                const double anchor = horizontal ? d.position.x : d.position.y;
                const bool aligned =
                    d.passage ? std::floor(lateral) == std::floor(anchor) : std::abs(lateral - anchor) < .43;
                if (crossed && aligned)
                {
                    const double component = horizontal ? direction.y : direction.x;
                    const double coordinate = horizontal ? a.position.y : a.position.x;
                    const double limit = component < 0 ? Radius : (horizontal ? c->height : c->width) - Radius;
                    if (std::abs(component) > Epsilon)
                        movedTime += std::clamp((limit - coordinate) / component, 0.0, travel) / speed;
                    a.facing = std::atan2(direction.y, direction.x);
                    transition(a, d);
                    return;
                }
            }
            // Walking into any door takes the walker through it: stepping onto an
            // open doorway or stair, or pushing into a closed, unlocked door.
            // Locked doors still block; the arrival tile sits beside the far door.
            const Vec2 lead{proposed.x + direction.x * Radius, proposed.y + direction.y * Radius};
            for (Door* door : doorsIn(a.cellId))
            {
                auto& d = *door;
                if (!d.portal || d.locked || (d.passage && d.boundary) ||
                    !doorCovers(d, lead) || doorCovers(d, a.position) || !cell(d.targetCell))
                    continue;
                if (!d.open)
                {
                    d.open = true;
                    if (!d.linkedDoor.empty() && doors_.count(d.linkedDoor))
                        doors_.at(d.linkedDoor).open = true;
                }
                movedTime += travel / speed;
                a.facing = std::atan2(direction.y, direction.x);
                transition(a, d);
                return;
            }
            Vec2 accepted = a.position;
            if (passable(a.cellId, proposed, startTile))
                accepted = proposed;
            else
            {
                const Vec2 slideX{proposed.x, a.position.y};
                const Vec2 slideY{a.position.x, proposed.y};
                if (std::abs(delta.x) > Epsilon && passable(a.cellId, slideX, startTile))
                    accepted = slideX;
                if (std::abs(delta.y) > Epsilon && passable(a.cellId, {accepted.x, slideY.y}, startTile))
                    accepted.y = slideY.y;
                // A route invalidated by a newly closed door must not resume by itself.
                if (!a.path.empty())
                    a.path.clear();
            }
            const Vec2 actual{accepted.x - a.position.x, accepted.y - a.position.y};
            if (length(actual) > Epsilon)
            {
                a.facing = std::atan2(actual.y, actual.x);
                a.position = accepted;
                movedTime += length(actual) / speed;
            }
            if (a.path.empty() || distance(a.position, a.path.front()) >= Epsilon)
                return;
            a.path.erase(a.path.begin());
            dt -= travel / speed;
            if (a.path.empty())
                return;
        }
    };
    advanceMotion();
    if (a.cellId == originCell && elapsed > Epsilon)
        a.velocity = {(a.position.x - origin.x) / elapsed, (a.position.y - origin.y) / elapsed};
    updateStamina(a, elapsed, movedTime);
}

void World::updateStamina(Entity& a, double dt, double movedTime)
{
    if (dt <= 0.0)
        return;
    const double pace = effectivePace(a) / 10.0;
    const double grossDrain = SprintDrain * pace * pace;
    const double before = a.stamina;
    a.stamina = std::clamp(before + StaminaRecovery * dt - grossDrain * std::clamp(movedTime, 0.0, dt), 0.0, 100.0);
    a.staminaRate = (a.stamina - before) / dt;
    if (a.stamina <= Epsilon)
    {
        a.stamina = 0.0;
        a.exhausted = true;
    }
    else if (a.exhausted && a.stamina >= ExhaustionRecovery - Epsilon)
        a.exhausted = false;
}

namespace
{
// `text` as a JSON string of at most `limit` characters: valid UTF-8 only, control characters escaped.
void jsonText(std::string& out, const std::string& text, std::size_t limit)
{
    out += '"';
    std::size_t characters = 0;
    for (std::size_t i = 0; i < text.size() && characters < limit;)
    {
        const auto byte = static_cast<unsigned char>(text[i]);
        const std::size_t length = byte < 0x80 ? 1 : (byte >> 5) == 0x6 ? 2 : (byte >> 4) == 0xE ? 3 : (byte >> 3) == 0x1E ? 4 : 0;
        bool valid = length && i + length <= text.size();
        for (std::size_t k = 1; valid && k < length; ++k)
            valid = (static_cast<unsigned char>(text[i + k]) & 0xC0) == 0x80;
        if (!valid)
        {
            ++i;                                    // Not UTF-8: left out.
            continue;
        }
        if (byte == '"' || byte == '\\')
            out += '\\', out += char(byte);
        else if (byte < 0x20)
        {
            static const char* hex = "0123456789abcdef";
            out += "\\u00";
            out += hex[byte >> 4];
            out += hex[byte & 15];
        }
        else
            out.append(text, i, length);
        i += length;
        ++characters;
    }
    out += '"';
}
} // namespace

std::string eventsJson(const std::vector<WorldEvent>& events)
{
    std::string out = "[";
    for (const auto& e : events)
    {
        if (out.size() > 1)
            out += ',';
        out += "{\"kind\":";
        jsonText(out, e.kind.empty() ? std::string("event") : e.kind, 40);
        out += ",\"actor\":";
        jsonText(out, e.actor, 80);
        out += ",\"target\":";
        jsonText(out, e.target, 80);
        out += ",\"cell\":";
        jsonText(out, e.cell, 80);
        out += ",\"item\":";
        jsonText(out, e.item, 40);
        out += ",\"detail\":";
        jsonText(out, e.detail, 400);
        std::ostringstream numbers;
        numbers.precision(17);
        numbers << ",\"time\":" << (std::isfinite(e.time) ? e.time : 0.0) << ",\"day\":" << (std::isfinite(e.day) ? e.day : 0.0)
                << ",\"quantity\":" << e.quantity << ",\"coins\":" << e.coins << '}';
        out += numbers.str();
    }
    return out + "]";
}

void World::recordEvent(WorldEvent event)
{
    event.time = time_;
    event.day = calendarDays_;
    if (event.cell.empty())
        if (const auto* who = entity(event.actor))
            event.cell = who->cellId;
    bondsFromEvent(event);
    rumoursFromEvent(event);
    noteNews(event);
    contractsFromEvent(event);
    events_.push_back(std::move(event));
    if (events_.size() > EventsKept + EventsKept / 4)   // Trimmed in batches, not one at a time off the front.
    {
        const auto excess = events_.size() - EventsKept;
        events_.erase(events_.begin(), events_.begin() + std::ptrdiff_t(excess));
        droppedEvents_ += excess;
    }
}

void World::bondsFromEvent(const WorldEvent& e)
{
    // Only between living characters: the treasury, "outside", the herb patch and the dead have no feelings.
    const auto* actor = entity(e.actor);
    const auto* target = entity(e.target);
    if (e.actor == e.target || !actor || !target || actor->dead || target->dead || actor->transient || target->transient)
        return;
    const double day = calendarDays_;
    // A promise between these two is kept by dealing honestly with the other: trade, payment, a gift, help.
    if (e.kind == "economy" || e.kind == "gift" || e.kind == "help")
        for (auto& p : promises_)
            if (p.status == "open" && ((p.by == e.actor && p.to == e.target) || (p.by == e.target && p.to == e.actor)))
            {
                p.status = "kept";
                bonds_.change(p.to, p.by, {2, 6, 1, 0, 1}, day);
                events_.push_back({"promise kept", p.by, p.to, e.cell, time_, day, {}, 0, 0, p.what});
            }
    if (e.kind == "economy")
    {
        // A sale or a wage honestly paid: they know each other a little better and trust a little more.
        bonds_.mutual(e.actor, e.target, {.5, .5, 1, 0, 0}, day);
    }
    else if (e.kind == "conversation")
        bonds_.mutual(e.actor, e.target, {.2, 0, .5, 0, 0}, day);
    else if (e.kind == "help" || e.kind == "gift")
        bonds_.change(e.target, e.actor, {5, 3, 2, 0, 1}, day);        // The one helped warms to the helper.
    else if (e.kind == "harm")
    {
        bonds_.change(e.target, e.actor, {-20, -20, 2, 15, 0}, day);   // The one harmed.
        bonds_.change(e.actor, e.target, {0, 0, 2, 0, 0}, day);
    }
}

void World::tendBonds()
{
    const auto hour = std::int64_t(std::floor(calendarDays_ * 24));
    if (hour == bondHour_)
        return;
    bondHour_ = hour;
    // An hour spent at home with the household, or at work with the others working there: familiarity grows, and a
    // little liking. A crowded bunkhouse doesn't make everyone close to everyone: each knows only a few nearby.
    std::map<std::string, std::vector<const Entity*>> together;
    for (const auto& [id, e] : entities_)
    {
        if (!e.npc || e.dead)
            continue;
        const auto* life = society_.resident(id);
        const auto* spec = society_.spec(id);
        if (life && (life->task == "festival" || life->task == "at the market" || life->task == "resting"))
            together["out " + e.cellId].push_back(&e);         // A crowd, or friends on a day off (Phase 9).
        else if (life && e.cellId == life->homeCell)
            together["home " + life->homeCell].push_back(&e);
        else if (spec && e.cellId == spec->work.cell)
            together["work " + spec->work.cell].push_back(&e);
    }
    constexpr std::size_t Close = 3;
    for (const auto& [place, people] : together)
        for (std::size_t i = 0; i < people.size(); ++i)
            for (std::size_t k = 1; k <= std::min(Close, people.size() - 1); ++k)
            {
                const auto* other = people[(i + k + std::size_t(hour)) % people.size()];
                if (other != people[i])
                    bonds_.change(people[i]->id, other->id, place.rfind("out ", 0) == 0 ? BondChange{.5, .15, 1.5, 0, 0} : BondChange{.3, .1, 1, 0, 0},
                                  calendarDays_);
            }
    const auto day = std::int64_t(std::floor(calendarDays_));
    if (day != bondDay_)
    {
        if (bondDay_ >= 0)
            bonds_.fade(calendarDays_);
        bondDay_ = day;
        careerNotes(society_.tendCareers(calendarDays_, careerWorld()));
        tendPromises();
        tendMarriages();
    }
}

void World::promise(const std::string& by, const std::string& to, const std::string& what, double days)
{
    if (by.empty() || to.empty() || by == to || what.empty() || !std::isfinite(days) || days <= 0)
        return;
    promises_.push_back({by, to, what.substr(0, 200), calendarDays_, calendarDays_ + std::min(days, 30.0), "open"});
    // Settled promises are history (the event log keeps them); a few hundred open ones are plenty.
    promises_.erase(std::remove_if(promises_.begin(), promises_.end(), [&](const Promise& p) {
                        return p.status != "open" && calendarDays_ - p.due > 30;
                    }),
                    promises_.end());
    if (promises_.size() > 2000)
        promises_.erase(promises_.begin(), promises_.begin() + std::ptrdiff_t(promises_.size() - 2000));
}

void World::tendPromises()
{
    for (auto& p : promises_)
        if (p.status == "open" && calendarDays_ >= p.due)
        {
            p.status = "broken";
            bonds_.change(p.to, p.by, {-4, -10, 0, 0, -2}, calendarDays_);
            recordEvent({"promise broken", p.by, p.to, {}, 0, 0, {}, 0, 0, p.what});
        }
}

std::string World::promisesBetween(const std::string& npc, const std::string& other, const std::string& otherName) const
{
    std::string out;
    for (const auto& p : promises_)
    {
        if (p.status != "open" || !((p.by == npc && p.to == other) || (p.by == other && p.to == npc)))
            continue;
        const int days = int(std::ceil(p.due - calendarDays_));
        out += (out.empty() ? "" : " ") + std::string(p.by == npc ? "You promised " + otherName : otherName + " promised you") +
               ": " + p.what + " (due in " + std::to_string(std::max(0, days)) + " day" + (days == 1 ? "" : "s") + ").";
    }
    return out;
}

void World::tendMarriages()
{
    // Once a game week, two unmarried adults who like each other a great deal and know each other well may marry;
    // one moves in with the other. Not every such pair, and not all at once.
    const auto week = std::int64_t(std::floor(calendarDays_ / 7));
    if (week == marriageWeek_)
        return;
    marriageWeek_ = week;
    for (const auto& [id, e] : entities_)
    {
        if (!e.npc || e.dead || e.age < 18 || e.age > 60 || society_.spouse(id) || !society_.resident(id))
            continue;
        const auto* mine = bonds_.of(id);
        if (!mine)
            continue;
        for (const auto& [other, b] : *mine)
        {
            const auto* o = entity(other);
            const auto* back = bonds_.find(other, id);
            if (!(id < other) || !o || !o->npc || o->dead || o->age < 18 || o->age > 60 || society_.spouse(other) ||
                !society_.resident(other) || !back || b.affinity < 50 || back->affinity < 50 || b.familiarity < 60 ||
                back->familiarity < 60 || society_.family(id, other) ||
                (std::hash<std::string>{}(id + other) + std::uint64_t(week)) % 3 != 0)
                continue;
            // The one from the smaller household moves (the younger, if they are alike).
            int here = 0, there = 0;
            for (const auto& [someone, life] : society_.state().residents)
                here += society_.household(someone, id), there += society_.household(someone, other);
            const bool iMove = here < there || (here == there && e.age < o->age);
            const auto& mover = iMove ? id : other;
            const auto* stay = society_.resident(iMove ? other : id);
            double x = stay->homeX, y = stay->homeY;
            if (!society_.marry(id, other))
                continue;
            if (freeSpotNear(stay->homeCell, x, y))
                society_.moveHome(mover, stay->homeCell, x, y);
            recordEvent({"marriage", id, other, {}, 0, 0, {}, 0, 0, entity(mover)->name + " moves in"});
            break;
        }
    }
}

bool World::freeSpotNear(const std::string& cellId, double& x, double& y)
{
    if (!ensureLoaded(cellId).ok)
        return false;
    const auto* c = cell(cellId);
    if (!c)
        return false;
    std::set<std::pair<int, int>> homes;
    for (const auto& [id, life] : society_.state().residents)
        if (life.homeCell == cellId)
            homes.insert({int(std::floor(life.homeX)), int(std::floor(life.homeY))});
    const int cx = int(std::floor(x)), cy = int(std::floor(y));
    for (int ring = 1; ring <= 4; ++ring)
        for (int dy = -ring; dy <= ring; ++dy)
            for (int dx = -ring; dx <= ring; ++dx)
            {
                if (std::max(std::abs(dx), std::abs(dy)) != ring)
                    continue;
                const int tx = cx + dx, ty = cy + dy;
                const auto* t = c->tile(tx, ty);
                if (!t || t->solid || homes.count({tx, ty}) || blockedByDoor(cellId, {tx + .5, ty + .5}))
                    continue;
                x = tx + .5;
                y = ty + .5;
                return true;
            }
    return false;
}

std::vector<ResidentRequest> World::takeResidentRequests()
{
    auto requests = society_.takeRequests();
    for (auto& r : requests)
        if (!freeSpotNear(r.home.cell, r.home.x, r.home.y))
            r.home = {};                             // The host will find them somewhere to stay.
    return requests;
}

Result World::welcomeResident(const ResidentRequest& request, const std::string& id)
{
    if (!entity(id))
        return {false, "No such resident.", id};
    const auto note = society_.welcome(request, id);
    careerNotes({note});
    return {note.kind != "refused", note.detail, id};
}

CareerWorld World::careerWorld() const
{
    CareerWorld w;
    // Offline player characters are alive; so is anyone whose body isn't dead.
    w.alive = [this](const std::string& id) {
        const auto* e = entity(id);
        return e ? !e->dead : playerAccountId(id);
    };
    w.age = [this](const std::string& id) {
        const auto* e = entity(id);
        return e ? e->age : 30;
    };
    w.regard = [this](const std::string& who, const std::string& ofWhom) {
        const auto* b = bonds_.find(who, ofWhom);
        return b ? std::clamp(b->familiarity * .4 + b->affinity * .4 + b->trust * .2, 0.0, 100.0) : 0.0;
    };
    w.near = [this](const std::string& workCell, const std::string& homeCell) {
        const auto* work = cell(workCell);
        const auto* home = cell(homeCell);
        const auto known = [](const Cell* c) { return c && !c->region.empty() && c->region != "unassigned"; };
        return !known(work) || !known(home) || work->region == home->region;
    };
    return w;
}

void World::careerNotes(const std::vector<CareerNote>& notes)
{
    for (const auto& n : notes)
        recordEvent({n.kind, n.actor, n.target, {}, 0, 0, {}, 0, 0, n.detail});
    absorbJournal();                                // Inheritances are ledger entries.
}

Result World::apprentice(const std::string& player, const std::string& master)
{
    const auto* p = entity(player);
    const auto* m = entity(master);
    const auto* job = society_.jobOf(master);
    if (!p || p->npc || !m || !m->npc || m->dead || !job)
        return {false, "They have no trade to teach.", master};
    const auto note = society_.apprentice(player, job->id, careerWorld(), calendarDays_);
    if (note.kind != "apprenticeship")
        return {false, note.detail, master};
    careerNotes({note});
    return {true, m->name + " takes you on as an apprentice: " + job->title + ".", master};
}

std::vector<WorldEvent> World::takeEvents()
{
    absorbJournal();
    std::vector<WorldEvent> out;
    out.swap(events_);
    return out;
}

void World::absorbJournal()
{
    for (auto& entry : society_.takeJournal())
    {
        WorldEvent e;
        e.kind = "economy";
        e.detail = entry.kind;
        e.actor = entry.from;
        e.target = entry.to;
        e.item = entry.item;
        e.quantity = entry.quantity;
        e.coins = entry.coins;
        recordEvent(std::move(e));
    }
}

std::size_t World::offstageCount() const
{
    std::size_t n = 0;
    for (const auto& entry : entities_)
        n += entry.second.offstage;
    return n;
}

void World::nearCells(const Entity& e, std::set<std::string>& out) const
{
    // A door further off than this is a building the entity can neither see into nor reach within a few seconds.
    constexpr double NearDoor = 24.0;
    out.insert(e.cellId);
    const auto seams = exits_.find(e.cellId);
    if (seams != exits_.end())
        out.insert(seams->second.begin(), seams->second.end());
    auto portals = portalsIn_.find(e.cellId);
    if (portals == portalsIn_.end())
    {
        std::vector<const Door*> list;
        for (const Door* d : doorsIn(e.cellId))
            if (d->portal && !(d->passage && d->boundary))
                list.push_back(d);
        portals = portalsIn_.emplace(e.cellId, std::move(list)).first;
    }
    for (const Door* d : portals->second)
        if (distance(e.position, d->position) <= NearDoor)
            out.insert(d->targetCell);
    if (seams == exits_.end())                      // A world wholly in memory: its seams are ordinary records.
        for (const Door* d : doorsIn(e.cellId))
            if (d->portal && d->passage && d->boundary)
                out.insert(d->targetCell);
}

std::set<std::string> World::stageCells() const
{
    std::set<std::string> stage;
    for (const auto& entry : entities_)
        if (!entry.second.npc)
            nearCells(entry.second, stage);
    return stage;
}

void World::placeOnStage(Entity& e, const std::set<std::string>& stage)
{
    // Companions go where their leader goes, always in full.
    const bool off = e.leaderId.empty() && e.state != "following" && !stage.count(e.cellId);
    if (off == e.offstage)
        return;
    // Either way the NPC stands where it is, which is a place it could stand: walking only ever reaches such
    // places, and offstage hops only ever end on them. Whatever it was doing, it plans again from there.
    e.offstage = off;
    legs_.erase(e.id);
    pathRetryAt_.erase(e.id);
    pendingPortals_.erase(e.id);
    e.path.clear();
    e.input = {};
    e.velocity = {};
    e.turning = false;
    if (e.posture == "rising")
        settle(e, e.postureTarget.empty() ? "standing" : e.postureTarget.c_str());
    if (!off)
        ensureLoaded(e.cellId);
}

void World::moveOffstage(Entity& e, const std::string& task, const std::string& goalCell, Vec2 goal)
{
    // How much longer walking a route takes than a straight line, on average.
    constexpr double Detour = 1.3;
    auto& leg = legs_[e.id];
    if (!leg.goalCell.empty() && (leg.goalCell != goalCell || distance(leg.goal, goal) > 1e-9))
        leg = {};
    if (!leg.goalCell.empty())
    {
        if (time_ < leg.arriveAt)
            return;
        if (leg.crossing)
        {
            e.cellId = leg.intoCell;
            e.position = leg.arrival;
        }
        else
            e.position = leg.to;
        leg = {};
        return;
    }
    const double speed = std::max(.5, paceSpeed(e));
    if (e.cellId == goalCell)
    {
        leg = {goalCell, goal, false, {}, goal, {}, time_ + distance(e.position, goal) * Detour / speed};
        return;
    }
    const auto& steps = cachedSteps(e.cellId);
    const auto step = steps.find(goalCell);
    if (step == steps.end())
    {
        e.activity = task + " — route unavailable";
        return;
    }
    // The nearest way into the next cell: a door, or a crossing of the shared edge (known from the first time this
    // cell was in memory; if it never has been, it is loaded once to learn them).
    const auto nearest = [&]() -> const Door* {
        const Door* best = nullptr;
        const auto consider = [&](const Door& way) {
            if (way.portal && !way.locked && way.targetCell == step->second &&
                (!best || distance(e.position, way.position) < distance(e.position, best->position) - 1e-9))
                best = &way;
        };
        for (const Door* way : doorsIn(e.cellId))
            consider(*way);
        const auto* here = cell(e.cellId);
        if (here && !here->loaded)
            if (const auto known = seamAnchors_.find(e.cellId); known != seamAnchors_.end())
                for (const auto& way : known->second)
                    consider(way);
        return best;
    };
    const Door* way = nearest();
    if (!way && !seamAnchors_.count(e.cellId) && ensureLoaded(e.cellId).ok)
        way = nearest();
    if (!way)
    {
        e.activity = task + " — route unavailable";
        return;
    }
    leg = {goalCell, goal, true, way->targetCell, way->position, way->arrival,
           time_ + distance(e.position, way->position) * Detour / speed};
}

bool World::nearPortal(const std::string& cellId, Vec2 p, double within) const
{
    constexpr double Square = 2.0;                  // No wider than `within` is ever asked to be.
    auto grid = portalGrid_.find(cellId);
    if (grid == portalGrid_.end())
    {
        std::map<std::pair<long, long>, std::vector<const Door*>> squares;
        for (const Door* d : doorsIn(cellId))
            if (d->portal)
                squares[{long(std::floor(d->position.x / Square)), long(std::floor(d->position.y / Square))}].push_back(d);
        grid = portalGrid_.emplace(cellId, std::move(squares)).first;
    }
    const long x = long(std::floor(p.x / Square)), y = long(std::floor(p.y / Square));
    for (long dy = -1; dy <= 1; ++dy)
        for (long dx = -1; dx <= 1; ++dx)
            if (const auto found = grid->second.find({x + dx, y + dy}); found != grid->second.end())
                for (const Door* d : found->second)
                    if (distance(p, d->position) < within)
                        return true;
    return false;
}

void World::separate(double dt)
{
    // Only characters in the same cell push each other apart. Grouping them by cell first, keeping ID order within
    // each group, visits the same pairs in the same order as comparing everyone with everyone (pairs in different
    // cells share no one, so the groups don't affect each other), without the whole world's worth of pairs.
    auto& byCell = separateScratch_;
    byCell.clear();
    for (auto& entry : entities_)
        if (!entry.second.offstage)
            byCell.push_back(&entry.second);
    std::stable_sort(byCell.begin(), byCell.end(), [](const Entity* x, const Entity* y) { return x->cellId < y->cellId; });
    for (std::size_t first = 0, end = 0; first < byCell.size(); first = end)
    {
        end = first + 1;
        while (end < byCell.size() && byCell[end]->cellId == byCell[first]->cellId)
            ++end;
        // A crowd is bucketed by where each stood as this step began, and only pairs in touching buckets are
        // compared, still in the same order. Each push is at most .12 * dt, so across one step nobody moves anywhere
        // near Slack; a pair further apart than a bucket at the start can't touch before the step ends.
        constexpr double Slack = .25, Bucket = Radius * 2 + Slack;
        constexpr std::size_t Crowd = 24;
        std::map<std::pair<long, long>, std::vector<std::size_t>> buckets;
        std::vector<std::size_t> later;
        const bool crowded = end - first > Crowd;
        const auto bucketOf = [](Vec2 p) { return std::pair<long, long>{long(std::floor(p.x / Bucket)), long(std::floor(p.y / Bucket))}; };
        if (crowded)
            for (std::size_t k = first; k < end; ++k)
                buckets[bucketOf(byCell[k]->position)].push_back(k);
        std::vector<std::pair<long, long>> startBucket;
        if (crowded)
            for (std::size_t k = first; k < end; ++k)
                startBucket.push_back(bucketOf(byCell[k]->position));
        for (std::size_t i = first; i < end; ++i)
        {
            later.clear();
            if (crowded)
            {
                const auto [bx, by] = startBucket[i - first];
                for (long dy = -1; dy <= 1; ++dy)
                    for (long dx = -1; dx <= 1; ++dx)
                        if (const auto found = buckets.find({bx + dx, by + dy}); found != buckets.end())
                            for (const auto k : found->second)
                                if (k > i)
                                    later.push_back(k);
                std::sort(later.begin(), later.end());
            }
            else
                for (std::size_t j = i + 1; j < end; ++j)
                    later.push_back(j);
            for (const auto j : later)
            {
                auto& a = *byCell[i];
                auto& b = *byCell[j];
                const double d = distance(a.position, b.position);
                if (d >= Radius * 2)
                    continue;
                const bool atPortal = nearPortal(a.cellId, a.position, 1.7) || nearPortal(a.cellId, b.position, 1.7);
                if (atPortal || a.transitioned || b.transitioned)
                    continue;
                Vec2 n =
                    d > Epsilon ? Vec2{(a.position.x - b.position.x) / d, (a.position.y - b.position.y) / d} : Vec2{1, 0};
                const double push = std::min((Radius * 2 - d) * .15, .12 * dt);
                const auto* c = cell(a.cellId);
                const auto* ta = c->tile(int(a.position.x), int(a.position.y));
                const auto* tb = c->tile(int(b.position.x), int(b.position.y));
                const Vec2 pa{a.position.x + n.x * push, a.position.y + n.y * push},
                    pb{b.position.x - n.x * push, b.position.y - n.y * push};
                if (ta && passable(a.cellId, pa, ta))
                    a.position = pa;
                if (tb && passable(b.cellId, pb, tb))
                    b.position = pb;
            }
        }
    }
}

Result World::relocateResident(const std::string& id, const std::string& destination, double x, double y)
{
    ensureLoaded(destination);
    auto* actor = entity(id);
    const auto* targetCell = cell(destination);
    const auto* life = society_.resident(id);
    if (!actor || !actor->npc || !life || life->role != "resident" || !actor->leaderId.empty() ||
        actor->state == "following" || !targetCell || !std::isfinite(x) || !std::isfinite(y) ||
        x < .5 || y < .5 || x > targetCell->width - .5 || y > targetCell->height - .5)
        return {false, "Choose an existing, non-recruited resident and a traversable home; essential jobs are protected.", id};
    const auto* tile = targetCell->tile(int(x), int(y));
    if (!tile || !passable(destination, {x, y}, tile))
        return {false, "The proposed home is blocked.", id};
    if (!cachedSteps(actor->cellId).count(destination) || (actor->cellId == destination &&
        distance(actor->position, {x, y}) > .35 && findPath(*actor, {x, y}, true).empty()))
        return {false, "No authored route reaches the proposed home.", id};
    if (!society_.relocate(id, destination, x, y)) return {false, "Resident already relocating or invalid home.", id};
    stop(id);
    recordEvent({"relocation", id, {}, {}, 0, 0, {}, 0, 0, "new home in " + destination});
    return {true, "Relocation accepted; the resident must physically arrive before the home changes.", id};
}

void World::updateSchedules()
{
    std::map<std::string, LifeBody> bodies;
    for (auto& pair : entities_)
    {
        advanceAge(pair.second, calendarDays_);
        const auto& e = pair.second;
        if (e.dead)
            continue;                               // The dead keep no schedule: no work, no hunger, no walking.
        bodies[pair.first] = {e.cellId, e.position.x, e.position.y, !e.leaderId.empty() || e.state == "following", e.age};
    }
    planDays();
    society_.tick(.5, calendarDays_, int(calendar::calendarAt(calendarDays_).season), bodies);
    absorbJournal();
    tendBonds();
    tendRoads();
    tendCrime();
    // When a shift changes, a whole town sets off at once: plan a bounded number of routes per update and let the
    // rest set off on the next, rather than stalling the server for all of them in one tick.
    // Routes are not planned here: whoever needs one waits for planWantedRoutes, a few a tick.
    RouteBudget budget{0, searchExpanded_, 0, 0, true};
    const auto stage = tiered() ? stageCells() : std::set<std::string>{};
    for (auto& pair : entities_)
    {
        auto& e = pair.second;
        if (e.npc && tiered())
            placeOnStage(e, stage);
        if (e.transient)
            continue;                               // The road folk go their own ways (tendRoadFolk).
        const auto* life = society_.resident(pair.first);
        if (!e.npc || !life || life->goalCell.empty() || !e.leaderId.empty() || e.state == "following") continue;
        if (e.state == "beaten down") continue;     // Lying where they fell until they can get up (tendCrime).
        std::string task = life->task, reason = life->reason, goalCell = life->goalCell;
        Vec2 target{life->goalX, life->goalY};
        // The watch and the gaol, then work on the road, come before the day's plan.
        if (!crimeErrand(pair.first, task, reason, goalCell, target))
            errand(pair.first, *life, task, reason, goalCell, target);
        const std::string activity = task + " — " + reason;
        if (e.activity != activity) { stop(e.id); e.activity = activity; }
        if (e.cellId == goalCell && distance(e.position, target) <= .35)
        {
            if (task == "sleep" && e.posture != "lying")
            {
                if (e.offstage)
                    settle(e, "lying");
                else
                    setPosture(e.id, "lying");
            }
            continue;
        }
        headFor(e, task, goalCell, target, budget);
    }
    if (tiered())
    {
        // What streaming keeps in memory until the next update: the surroundings of everyone onstage.
        tierWanted_.clear();
        for (const auto& entry : entities_)
            if (!entry.second.offstage)
                nearCells(entry.second, tierWanted_);
    }
}

void World::planWantedRoutes()
{
    // In turn from where the last tick stopped, so no one waits behind the same few.
    RouteBudget budget{0, searchExpanded_, RouteSearchesPerTick, RouteNodesPerTick, true};
    auto it = routeWanted_.upper_bound(routeCursor_);
    for (std::size_t looked = 0, total = routeWanted_.size(); looked < total && !routeWanted_.empty(); ++looked)
    {
        if (budget.searches >= budget.maxSearches || searchExpanded_ - budget.expandedBefore >= budget.maxNodes)
            break;
        if (it == routeWanted_.end())
            it = routeWanted_.begin();
        const std::string id = it->first;
        const RouteWant want = it->second;
        it = routeWanted_.erase(it);
        routeCursor_ = id;
        auto* e = entity(id);
        if (!e || e->dead || e->offstage || !e->path.empty() || !e->leaderId.empty())
            continue;
        headFor(*e, want.task, want.goalCell, want.target, budget);
        it = routeWanted_.upper_bound(id);
    }
}

void World::headFor(Entity& e, const std::string& task, const std::string& goalCell, Vec2 target, RouteBudget& budget)
{
    if (e.offstage)
    {
        if (e.posture == "lying" || e.posture == "sitting")
            settle(e, "standing");
        moveOffstage(e, task, goalCell, target);
        return;
    }
    if (e.posture == "lying" || e.posture == "sitting") setPosture(e.id, "standing");
    if (!e.path.empty()) return;
    // A failed search on a large cell explores everything reachable; don't repeat it every half second.
    if (const auto wait = pathRetryAt_.find(e.id); wait != pathRetryAt_.end() && time_ < wait->second) return;
    const auto seek = [&](Vec2 goal) {
        if (budget.searches >= budget.maxSearches || searchExpanded_ - budget.expandedBefore >= budget.maxNodes)
        {
            if (budget.defer)
                routeWanted_[e.id] = {task, goalCell, target};
            return Result{false, "Waiting to set off.", {}};
        }
        routeWanted_.erase(e.id);
        ++budget.searches;
        ++profile_.routeSearches;
        const auto expandedAt = searchExpanded_;
        const auto searchBegin = std::chrono::steady_clock::now();
        auto result = moveTo(e.id, goal.x, goal.y);
        const double took = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - searchBegin).count();
        profile_.routeNodes += searchExpanded_ - expandedAt;
        profile_.largestRoute = std::max(profile_.largestRoute, searchExpanded_ - expandedAt);
        if (took > profile_.slowestRoute)
        {
            profile_.slowestRoute = took;
            profile_.slowestRouteCell = e.cellId;
            profile_.slowestRouteNodes = searchExpanded_ - expandedAt;
            profile_.slowestRouteWaypoints = e.path.size();
        }
        if (e.path.empty() && !door(result.targetId))
            pathRetryAt_[e.id] = time_ + 5.0;
        else
            pathRetryAt_.erase(e.id);
        return result;
    };
    const auto* current = cell(e.cellId);
    const auto* regions = current ? regionMap(*current) : nullptr;
    const auto regionOf = [&](Vec2 p) {
        const int x = int(std::floor(p.x)), y = int(std::floor(p.y));
        return !regions || x < 0 || y < 0 || x >= current->width || y >= current->height
                   ? -1 : (*regions)[std::size_t(y * current->width + x)];
    };
    const int region = regionOf(e.position);
    const int goalRegion = e.cellId == goalCell ? regionOf(target) : -1;
    if (e.cellId == goalCell && (region < 0 || goalRegion < 0 || region == goalRegion))
    {
        auto result = seek(target);
        if (const auto* barrier = door(result.targetId))
            if (!barrier->locked && distance(e.position, barrier->position) <= barrier->reach)
                interact(e.id, barrier->id, "open");
        return;
    }
    // Residents know their authored work/home routes, not player exploration: the next cell toward the goal,
    // then the first way into it (in ID order, as ever). In the goal cell but cut off from the goal (below), the way
    // on is out and back in.
    auto step = cachedSteps(e.cellId).find(goalCell);
    if (step == cachedSteps(e.cellId).end()) { e.activity = task + " — route unavailable"; return; }
    std::string next = e.cellId == goalCell ? std::string() : step->second;
    // Where a way comes out matters as much as where it starts. A seam can land on a strip of the next cell that a
    // step or a wall cuts off from the rest, with no way on from it; residents were stranded there for good, and
    // piled up. So a way is judged by where it lands, when the cell beyond is in memory to tell: in the goal's own
    // region (the goal cell), or in the body of the cell (its largest region) on the way through.
    const auto landing = [&](const Door& way) {         // 2: lands well; 1: can't tell; 0: lands in a pocket.
        const auto* there = cell(way.targetCell);
        if (!there || !regionMap(*there))
            return 1;
        const int arrives = regionAt(*there, way.arrival);
        const int aim = way.targetCell == goalCell ? regionAt(*there, target) : -1;
        const int wanted = aim >= 0 ? aim : mainRegion(*there);
        return arrives < 0 || wanted < 0 ? 1 : arrives == wanted ? 2 : 0;
    };
    // The nearest way into the next cell (a street door, or the closest tile of a shared edge) of those landing best.
    // Only ways this resident can walk to count: behind a wall, the nearest point of an edge may be unreachable.
    // With no way into it from here (a pocket, or cut off from the goal), the nearest way anywhere that lands well.
    const auto usable = [&](const Door* way) {
        return way->portal && !way->locked && (region < 0 || regionOf(way->position) < 0 || regionOf(way->position) == region);
    };
    const auto nearer = [&](const Door* way, const Door* than) {
        return !than || distance(e.position, way->position) < distance(e.position, than->position) - 1e-9;
    };
    const Door* d = nullptr;
    int ranked = -1;
    const auto choose = [&] {
        d = nullptr;
        ranked = -1;
        for (const Door* way : doorsIn(e.cellId))
            if (usable(way) && way->targetCell == next && !next.empty())
                if (const int lands = landing(*way); lands > ranked || (lands == ranked && nearer(way, d)))
                    d = way, ranked = lands;
    };
    choose();
    if (d && ranked == 0 && region >= 0 && current && region == mainRegion(*current))
    {
        // From the body of this cell every way into the next lands in a pocket: that connection leads nowhere. Route
        // around it (once found, for everyone), and choose again.
        deadEnds_.insert({e.cellId, next});
        stepsCache_.clear();
        step = cachedSteps(e.cellId).find(goalCell);
        if (step == cachedSteps(e.cellId).end()) { e.activity = task + " — route unavailable"; return; }
        next = step->second;
        choose();
    }
    if (!d || ranked == 0)
    {
        const Door* out = nullptr;
        for (const Door* way : doorsIn(e.cellId))
            if (usable(way) && way->targetCell != e.cellId && nearer(way, out) && landing(*way) == 2)
                out = way;
        if (out)
            d = out;
    }
    if (!d || ranked == 0)
    {
        // No walking way out at all: pockets come in pairs across a seam (an edge tile on each side, cut off from its
        // own cell by a step), and someone set down in one (arriving from offstage, say) can only cross to the other.
        // They clamber over the step to the open ground of the cell's body beside them (never through a wall).
        if (current && region >= 0 && region != mainRegion(*current) && (e.cellId != goalCell || region != goalRegion))
        {
            const int body = mainRegion(*current);
            const int cx = int(std::floor(e.position.x)), cy = int(std::floor(e.position.y));
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (!dx && !dy)
                            continue;
                        const Vec2 p{cx + dx + .5, cy + dy + .5};
                        const auto* t = current->tile(cx + dx, cy + dy);
                        if (!t || t->solid || regionOf(p) != body || blockedByDoor(e.cellId, p))
                            continue;
                        e.position = p;
                        e.path.clear();
                        e.velocity = {};
                        pathRetryAt_.erase(e.id);
                        return;
                    }
        }
        if (!d)
            return;
    }
    if (d->boundary && d->open && d->edge != '-')
    {
        // Aim just past the shared edge: a corner tile belongs to two edges, and its centre alone is ambiguous.
        const auto* here = cell(e.cellId);
        Vec2 aim = d->position;
        if (d->edge == 'N') aim.y = -.15; else if (d->edge == 'S') aim.y = here->height + .15;
        else if (d->edge == 'W') aim.x = -.15; else aim.x = here->width + .15;
        seek(aim);
    }
    else if (distance(e.position, d->position) <= d->reach)
        interact(e.id, d->id, d->open ? "enter" : "open");
    else
    {
        // A closed door's own tile is blocked, so a search for it always fails; aim for the ground just in front
        // of it instead, and open it on arrival (above).
        Vec2 approach = d->position;
        if (!d->open)
        {
            const auto* here = cell(e.cellId);
            const auto* doorTile = here ? here->tile(int(std::floor(d->position.x)), int(std::floor(d->position.y))) : nullptr;
            double best = std::numeric_limits<double>::infinity();
            for (const Vec2 side : {Vec2{1, 0}, Vec2{-1, 0}, Vec2{0, 1}, Vec2{0, -1}})
            {
                // Right up against the door: at night a wolf may see little more than a tile.
                const Vec2 front{std::floor(d->position.x) + .5 + side.x * .6, std::floor(d->position.y) + .5 + side.y * .6};
                if (passable(e.cellId, front, doorTile) && distance(e.position, front) < best)
                {
                    best = distance(e.position, front);
                    approach = front;
                }
            }
        }
        auto result = seek(approach);
        if (const auto* barrier = door(result.targetId))
            if (!barrier->locked && distance(e.position, barrier->position) <= barrier->reach)
                interact(e.id, barrier->id, "open");
    }
}

void World::tick(double dt)
{
    if (!std::isfinite(dt) || dt <= 0)
        return;
    // Bounded steps prevent tunneling. The hosting server should use 1/30 s;
    // even delayed input cannot tunnel through an entire terrain feature.
    dt = std::min(dt, 60.0);
    ++ticks_;
    ticking_ = true;
    struct Done { bool& flag; ~Done() { flag = false; } } done{ticking_};
    using Clock = std::chrono::steady_clock;
    double tickStreaming = 0, tickSchedules = 0, tickMovement = 0, tickSeparation = 0, tickViews = 0;
    const auto since = [](Clock::time_point start) {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    };
    auto mark = Clock::now();
    stream();
    tickStreaming += since(mark);
    while (dt > Epsilon)
    {
        const double step = std::min(dt, 1.0 / 30.0);
        time_ += step;
        calendarDays_ = std::min(calendar::MaxGameDays, calendarDays_ + step / DaySeconds);
        const auto slot = std::int64_t(std::floor(calendarDays_ * 4));
        if (slot != climateSlot_)
            climateSlot_ = slot;
        refreshWeatherField();                     // The regional field, every 15 game minutes (RatwWeather.cpp).
        scheduleAccumulator_ += step;
        if (scheduleAccumulator_ + 1e-9 >= .5)
        {
            mark = Clock::now();
            updateSchedules();
            tickSchedules += since(mark);
            scheduleAccumulator_ = std::max(0., scheduleAccumulator_ - .5);
        }
        else if (!routeWanted_.empty())
        {
            mark = Clock::now();
            planWantedRoutes();
            tickSchedules += since(mark);
        }
        mark = Clock::now();
        for (auto& entry : entities_)
        {
            if (entry.second.dead)
            {
                entry.second.velocity = {};
                entry.second.input = {};
                entry.second.path.clear();
                continue;
            }
            if (entry.second.offstage)
                continue;                           // Offstage NPCs don't walk (see moveOffstage()).
            updateTravel(entry.second);
            integrate(entry.second, step);
        }
        tickMovement += since(mark);
        mark = Clock::now();
        separate(step);
        tickSeparation += since(mark);
        dt -= step;
    }
    mark = Clock::now();
    // A player's map memory takes in what they see as they go. A view is thousands of sight rays, so it is taken
    // on arriving in each tile rather than every tick; the server's snapshots (five a second) also observe from
    // exactly where the player stands, so everything a player is shown is remembered.
    std::vector<std::string> looking;
    for (const auto& entry : entities_)
        if (!entry.second.npc)
        {
            const auto& e = entry.second;
            const ObservedTile here{e.cellId, int(std::floor(e.position.x)), int(std::floor(e.position.y))};
            auto& last = lastObserved_[entry.first];
            if (last.cellId == here.cellId && last.x == here.x && last.y == here.y)
                continue;
            last = here;
            looking.push_back(entry.first);
        }
    prepareViews(looking);
    for (const auto& id : looking)
        observe(id);
    absorbJournal();                                // Whatever the host did to the society directly.
    tickViews += since(mark);
    const auto add = [](TickProfile::Part& part, double ms) {
        part.total += ms;
        part.worst = std::max(part.worst, ms);
    };
    add(profile_.streaming, tickStreaming);
    add(profile_.schedules, tickSchedules);
    add(profile_.movement, tickMovement);
    add(profile_.separation, tickSeparation);
    add(profile_.views, tickViews);
    ++profile_.ticks;
}

double World::visionClarity(const std::string& observerId, const std::string& sourceId) const
{
    const auto* o = entity(observerId);
    const auto* s = entity(sourceId);
    if (!o || !s)
        return 0;
    if (o->id == s->id)
        return 1;
    if (o->cellId != s->cellId || !visiblePoint(*o, s->position))
        return 0;
    if (s->posture == "crouching")
    {
        const double detectionRange = (7.0 - 4.0 * clamp01(s->sneakSkill / 100.0)) * (sightRange(*o) / 27.0);
        if (distance(o->position, s->position) >= detectionRange)
            return 0;
    }
    return clarity(distance(o->position, s->position), sightRange(*o) * .5);
}
double World::hearingClarity(const std::string& observerId, const std::string& sourceId, Voice voice) const
{
    const auto* o = entity(observerId);
    const auto* s = entity(sourceId);
    if (!o || !s)
        return 0;
    if (o->id == s->id)
        return 1;
    const double sensitivity =
        std::max(0.0, o->hearing) * ageHearingFactor(*o) * clamp01(o->earHealth) * (1.0 + .75 * clamp01(o->hearingSkill / 100.0));
    if (sensitivity <= Epsilon)
        return 0;
    double range = (voice == Voice::Whisper ? 2.0 : voice == Voice::Yell ? 32.0 : 16.0) * sensitivity;
    const auto* oc = cell(o->cellId);
    const auto* sc = cell(s->cellId);
    if (!oc || !sc)
        return 0;
    range *= environmentAt(oc->id, o->position).hearing;
    if (o->cellId == s->cellId)
    {
        if (!lineOfSight(o->cellId, o->position, s->position))
            range *= .38;
        const auto* ot = oc->tile(int(o->position.x), int(o->position.y));
        const auto* st = sc->tile(int(s->position.x), int(s->position.y));
        return clarity(distance(o->position, s->position) + (ot && st ? std::abs(ot->height - st->height) : 0.0),
                       range);
    }
    if (voice != Voice::Yell)
        return 0;
    double best = 0;
    for (const Door* door : doorsIn(s->cellId))
    {
        const auto& d = *door;
        if (!d.portal || d.targetCell != o->cellId)
            continue;
        double route = distance(s->position, d.position) + distance(o->position, d.arrival) + 2.0;
        double transmission = d.open ? .80 : .24;
        if (!lineOfSight(s->cellId, s->position, d.position))
            transmission *= .45;
        if (!lineOfSight(o->cellId, d.arrival, o->position))
            transmission *= .45;
        best = std::max(best, clarity(route, range * transmission * environmentAt(sc->id, s->position).hearing));
    }
    return best;
}
double World::movementAudibility(const std::string& observerId, const std::string& sourceId) const
{
    const auto* o = entity(observerId);
    const auto* s = entity(sourceId);
    if (!o || !s || o->cellId != s->cellId || length(s->velocity) <= Epsilon)
        return 0.0;
    if (o->id == s->id)
        return 1.0;
    const auto* c = cell(o->cellId);
    if (!c)
        return 0.0;
    const double sensitivity =
        std::max(0.0, o->hearing) * ageHearingFactor(*o) * clamp01(o->earHealth) * (1.0 + .75 * clamp01(o->hearingSkill / 100.0));
    const double sourceRange = s->posture == "crouching" ? 2.5 - 1.7 * clamp01(s->sneakSkill / 100.0) : 6.0;
    double range = sourceRange * sensitivity;
    range *= environmentAt(c->id, o->position).hearing;
    if (!lineOfSight(o->cellId, o->position, s->position))
        range *= .38;
    const auto* ot = c->tile(int(o->position.x), int(o->position.y));
    const auto* st = c->tile(int(s->position.x), int(s->position.y));
    const double separation = distance(o->position, s->position) + (ot && st ? std::abs(ot->height - st->height) : 0);
    return clarity(separation, range);
}
SensoryResult World::perceive(const std::string& observer, const std::string& source, Voice voice) const
{
    const double visual = visionClarity(observer, source);
    return {hearingClarity(observer, source, voice), visual, visual > 0.0, scentClarity(observer, source)};
}
Environment World::environmentAt(const std::string& cellId) const
{
    const auto* c = cell(cellId);
    return environmentAt(cellId, c ? Vec2{c->width * .5, c->height * .5} : Vec2{});
}

Environment World::environmentAt(const std::string& cellId, Vec2 at) const
{
    Environment out;
    out.date = calendar::calendarAt(calendarDays_);
    out.hour = out.date.hour;
    out.daylight = out.date.daylight;
    out.phase = out.date.period;
    const auto* c = cell(cellId);
    if (!c)
        return out;
    if (!c->outdoors)
    {
        // A roof blocks weather, not darkness. Daylight access represents
        // windows/openings at whole-cell scale, rather than rays through doors.
        out.artificialLight = c->lighting.artificial;
        out.daylightAccess = c->lighting.daylightAccess;
        out.lightingTone = c->lighting.tone;
        const double natural = out.daylight * out.daylightAccess;
        const double strongest = std::max(out.artificialLight, natural);
        out.illumination = std::max(.08, strongest);
        out.sight = out.illumination;
        // A bright day-lit tavern needs no amber overlay. As daylight fades,
        // its artificial light becomes atmospheric, without losing visibility.
        out.glowStrength = out.artificialLight * (1.0 - natural);
        out.lightSource = strongest <= Epsilon                                 ? "dark"
                          : out.artificialLight > Epsilon && natural > Epsilon ? "mixed"
                          : out.artificialLight > Epsilon                      ? "artificial"
                                                                               : "daylight";
        return out;
    }

    // These are legible game-balance factors, not a meteorological model.
    // Darkness changes sight only: it does not damage hearing or smell and
    // never secretly changes a selected gait or stamina recovery.
    // The weather where it was asked for (doc 29, phase 7): each effect as strong as the weather is there.
    const auto here = weatherAt(cellId, at);
    out.weather = here.kind;
    out.intensity = here.intensity;
    const double clearLight = calendar::skyAt(calendarDays_, calendar::Weather::Clear).outdoorIllumination;
    out.illumination = clearLight + (calendar::skyAt(calendarDays_, skyWeather(here.kind)).outdoorIllumination - clearLight) * here.intensity;
    out.lightSource = out.daylight <= Epsilon ? "night sky" : "daylight";
    out.sight = out.illumination;
    const double baseSight = out.sight;
    switch (here.kind)
    {
    case Weather::Rain:
        out.sight *= .78;
        out.hearing = .72;
        out.scent = .65;
        out.movement = .85;
        break;
    case Weather::Snow:
        out.sight *= .65;
        out.hearing = .85;
        out.scent = .80;
        out.movement = .70;
        break;
    case Weather::Fog:
        out.sight *= .40;
        out.scent = 1.05; // Small humidity bonus; no automatic hearing penalty.
        break;
    case Weather::Overcast:
        out.sight *= .92; // Flat grey light only; the senses are otherwise untouched.
        break;
    case Weather::Storm:
        out.sight *= .60;
        out.hearing = .50;
        out.scent = .55;
        out.movement = .75;
        break;
    case Weather::Sandstorm:
        out.sight *= .30;
        out.hearing = .60;
        out.scent = .35;
        out.movement = .65;
        break;
    default:
        break;
    }
    const auto scaled = [&](double full) { return 1.0 + (full - 1.0) * here.intensity; };
    out.sight = baseSight * scaled(baseSight > Epsilon ? out.sight / baseSight : 1.0);
    out.hearing = scaled(out.hearing);
    out.scent = scaled(out.scent);
    out.movement = scaled(out.movement);
    out.hearing *= 1.0 - .25 * windAt(cellId).strength;
    return out;
}
Result World::setTimeOfDay(double hour)
{
    if (!std::isfinite(hour) || hour < 0 || hour >= 24)
        return {false, "Time of day requires a finite hour from zero up to, but not including, 24.", {}};
    const double elapsedHours = std::fmod(time_, DaySeconds) * 24.0 / DaySeconds;
    clockOffsetHours_ = std::fmod(hour - elapsedHours + 24.0, 24.0);
    calendarDays_ = std::floor(calendarDays_) + hour / 24.;
    climateSlot_ = -1;
    refreshWeatherField(true);
    return {true, "Time of day updated.", {}};
}
Result World::advanceCalendar(double days)
{
    if (!std::isfinite(days) || days < 0 || days > 36500 || calendarDays_ + days > calendar::MaxGameDays)
        return {false, "Calendar advance must be finite, forward and no more than 100 years.", {}};
    calendarDays_ += days;
    climateSlot_ = -1;
    for (auto& e : entities_) advanceAge(e.second, calendarDays_);
    refreshWeatherField(true);
    return {true, "Shared calendar advanced.", {}};
}
Result World::useSeasonalWeather(const std::string& id)
{
    auto* c = cell(id);
    if (!c || !c->outdoors) return {false, "Seasonal weather requires an outdoor cell.", id};
    c->seasonalWeather = true;
    refreshWeatherField(true);
    return {true, "Seasonal weather enabled.", id};
}
Result World::trade(const std::string& player, const std::string& trader, const std::string& item, int quantity, bool buy)
{
    const auto* p = entity(player);
    const auto* m = entity(trader);
    const auto* life = society_.resident(trader);
    if (!p || p->npc || !m || !society_.merchant(trader) || p->cellId != m->cellId ||
        distance(p->position, m->position) > 2. || visionClarity(player, trader) <= 0)
        return {false, "No accessible trader is within reach.", {}};
    if (m->posture == "lying" || (life && life->task == "sleep")) return {false, "The trader is sleeping.", trader};
    auto result = society_.trade(player, trader, item, quantity, buy);
    absorbJournal();
    return {result.ok, result.message, trader};
}
Result World::gather(const std::string& player)
{
    const auto* p = entity(player);
    if (!p || p->npc || herbCell_.empty() || p->cellId != herbCell_ || distance(p->position, herbPatch_) > 1.7 ||
        !visiblePoint(*p, herbPatch_)) return {false, "Approach the visible herb patch to gather.", {}};
    auto result = society_.gather(player);
    absorbJournal();
    return {result.ok, result.message, {}};
}
Result World::eat(const std::string& player)
{
    auto* p = entity(player);
    if (!p || p->npc) return {false, "No controlled character.", {}};
    auto result = society_.eat(player);
    absorbJournal();
    if (result.ok)
    {
        p->stamina = std::min(100., p->stamina + 10.);
        if (p->stamina >= ExhaustionRecovery) p->exhausted = false;
    }
    return {result.ok, result.message, {}};
}
Result World::setLighting(const std::string& cellId, double artificial, double daylightAccess, const std::string& tone)
{
    auto* c = cell(cellId);
    const Lighting light{artificial, daylightAccess, tone};
    if (!c || !validLighting(light))
        return {false, "Lighting requires a known cell, levels from zero to one, and a warm, neutral, or cool tone.",
                cellId};
    c->lighting = light;
    return {true, "Cell lighting updated.", cellId};
}
Wind World::windAt(const std::string& cellId) const
{
    const auto* c = cell(cellId);
    if (!c || !c->outdoors)
        return {};
    Wind wind = c->wind;
    if (wind.variable && wind.strength > 0)
    {
        // A stable per-cell phase and the persisted clock avoid gust resets on
        // restart. No random state or platform-specific string hash is used.
        std::uint32_t hash = 2166136261u;
        for (unsigned char ch : c->id)
            hash = (hash ^ ch) * 16777619u;
        const double phase = double(hash % 10000) * (2.0 * Pi / 10000.0);
        wind.direction += .20 * std::sin(time_ / 43.0 + phase);
        wind.strength = clamp01(wind.strength * (1.0 + .18 * std::sin(time_ / 11.0 + phase)));
    }
    wind.direction = std::remainder(wind.direction, 2.0 * Pi);
    return wind;
}
Result World::setWind(const std::string& cellId, double direction, double strength, bool variable)
{
    auto* c = cell(cellId);
    if (!c || !std::isfinite(direction) || !std::isfinite(strength) || strength < 0 || strength > 1)
        return {false, "Wind requires a known cell, finite heading, and strength from zero to one.", cellId};
    if (!c->outdoors && strength > 0)
        return {false, "Indoor air is calm in this prototype.", cellId};
    c->wind = c->outdoors ? Wind{std::remainder(direction, 2.0 * Pi), strength, variable} : Wind{};
    return {true, "Wind updated.", cellId};
}
double World::scentClarity(const std::string& observerId, const std::string& sourceId) const
{
    const auto* observer = entity(observerId);
    const auto* source = entity(sourceId);
    if (!observer || !source || observer->cellId != source->cellId || observerId == sourceId)
        return 0;
    const auto* c = cell(observer->cellId);
    if (!c)
        return 0;
    AirRoutes air(*c, doorsIn(c->id), observer->position);
    return detectScent(*observer, *source, windAt(c->id), environmentAt(c->id, observer->position).scent, air).clarity;
}
std::vector<ScentCue> World::scentCues(const std::string& observerId) const
{
    const auto* observer = entity(observerId);
    if (!observer)
        return {};
    const auto* c = cell(observer->cellId);
    if (!c)
        return {};
    // The air map covers the whole cell: made only once someone unseen is here to be smelled.
    std::optional<AirRoutes> air;
    const auto wind = windAt(c->id);
    const double scentFactor = environmentAt(c->id, observer->position).scent;
    std::map<int, ScentCue> sectors;
    for (const auto& entry : entities_)
    {
        if (entry.first == observerId || entry.second.cellId != observer->cellId ||
            visionClarity(observerId, entry.first) > 0)
            continue;
        if (!air)
            air.emplace(*c, doorsIn(c->id), observer->position);
        const auto scent = detectScent(*observer, entry.second, wind, scentFactor, *air);
        if (scent.clarity <= 0)
            continue;
        const double angle = std::atan2(scent.bearing.y, scent.bearing.x);
        const int sector = (int(std::floor((angle + Pi / 8.0) / (Pi / 4.0))) + 8) % 8;
        auto& cue = sectors[sector];
        cue.sector = sector;
        cue.strength = std::max(cue.strength, scent.clarity > .66 ? 3 : scent.clarity > .33 ? 2 : 1);
        cue.windborne = cue.windborne || scent.windborne;
    }
    std::vector<ScentCue> out;
    for (const auto& entry : sectors)
        out.push_back(entry.second);
    return out;
}
void World::setWeather(const std::string& id, Weather weather)
{
    if (static_cast<int>(weather) >= 0 && static_cast<int>(weather) < WeatherKinds)
        if (auto* c = cell(id))
        {
            c->weather = weather;
            c->seasonalWeather = false;
        }
}

void World::observe(const std::string& observerId)
{
    const auto* o = entity(observerId);
    if (!o || o->npc)
        return;
    const auto* c = cell(o->cellId);
    if (!c)
        return;
    auto& book = memories_[observerId];
    auto& memory = book[c->id];
    memory.knowledge = Knowledge::Visited;
    memory.cellId = c->id;
    memory.name = c->name;
    memory.width = c->width;
    memory.height = c->height;
    memory.worldX = c->worldX;
    memory.worldY = c->worldY;
    memory.worldZ = c->worldZ;
    memory.glyphs.resize(c->tiles.size(), ' ');
    memory.observed.resize(c->tiles.size(), false);
    bool fresh = false;
    const auto& visible = viewOf(*o, *c, &fresh);
    auto& view = views_[observerId];
    if (!fresh && view.remembered && memory.observed.size() == c->tiles.size())
        return;                                     // Nothing it sees has changed since the last look.
    view.remembered = true;                         // (A view made ahead by prepareViews() hasn't been taken in.)
    const double range = sightRange(*o);
    for (std::size_t index = 0; index < visible.size(); ++index)
        if (visible[index])
        {
            memory.glyphs[index] = c->tiles[index].glyph;
            memory.observed[index] = true;
        }
    for (const Door* door : doorsIn(o->cellId))
    {
        const auto& d = *door;
        // visiblePortal(), with the range worked out once: a large cell has a thousand and more edge crossings.
        if (!d.portal || !d.open || distance(o->position, d.position) > range ||
            !lineOfSight(o->cellId, o->position, d.position))
            continue;
        const auto* adjacent = cell(d.targetCell);
        if (!adjacent)
            continue;
        auto& glimpse = book[adjacent->id];
        if (glimpse.knowledge == Knowledge::Unknown)
            glimpse.knowledge = Knowledge::Glimpsed;
        glimpse.cellId = adjacent->id;
        glimpse.name = adjacent->name;
        glimpse.width = adjacent->width;
        glimpse.height = adjacent->height;
        glimpse.worldX = adjacent->worldX;
        glimpse.worldY = adjacent->worldY;
        glimpse.worldZ = adjacent->worldZ;
    }
}
const std::map<std::string, CellMemory>& World::memories(const std::string& id) const
{
    static const std::map<std::string, CellMemory> empty;
    const auto it = memories_.find(id);
    return it == memories_.end() ? empty : it->second;
}
Snapshot World::snapshot(const std::string& observerId)
{
    Snapshot out;
    out.time = time_;
    const auto* o = entity(observerId);
    if (!o)
        return out;
    observe(observerId);
    const auto* c = cell(o->cellId);
    if (!c)
        return out;
    out.self = *o;
    out.self.path.clear();
    out.self.input = {};
    out.cell = *c;
    out.cell.wind = windAt(c->id);
    out.environment = environmentAt(c->id, o->position);
    out.scentCues = scentCues(observerId);
    out.visibleTiles.resize(c->tiles.size());
    out.rememberedTiles.resize(c->tiles.size());
    const auto& book = memories(observerId);
    const auto remembered = book.find(c->id);
    const CellMemory emptyMemory;
    const auto& memory = remembered == book.end() ? emptyMemory : remembered->second;
    const auto& seen = viewOf(*o, *c);
    for (int y = 0; y < c->height; ++y)
        for (int x = 0; x < c->width; ++x)
        {
            const auto i = static_cast<std::size_t>(y * c->width + x);
            const bool visible = seen[i];
            out.visibleTiles[i] = visible;
            out.rememberedTiles[i] = !visible && i < memory.observed.size() && memory.observed[i];
            if (!visible)
            {
                out.cell.tiles[i] = Tile{};
                out.cell.tiles[i].glyph = out.rememberedTiles[i] ? memory.glyphs[i] : ' ';
            }
        }
    for (const auto& entry : entities_)
    {
        if (entry.first != observerId && !entry.second.npc && entry.second.cellId == o->cellId &&
            visionClarity(observerId, entry.first) <= 0 && movementAudibility(observerId, entry.first) > 0)
            out.movementHeard = true;
        if (entry.first == observerId ||
            (entry.second.cellId == o->cellId && visionClarity(observerId, entry.first) > 0))
        {
            Entity visible = entry.second;
            visible.path.clear();
            visible.input = {};
            out.entities.push_back(std::move(visible));
        }
    }
    for (const Door* door : doorsIn(o->cellId))
        if (!(door->passage && door->boundary) && visiblePoint(*o, door->position))
        {
            Door visible = *door;
            // Target anchors/paired fixture IDs are server topology, not UI data.
            visible.arrival = {};
            visible.linkedDoor.clear();
            const auto destinationMemory = book.find(visible.targetCell);
            if (visible.portal &&
                (destinationMemory == book.end() || destinationMemory->second.knowledge == Knowledge::Unknown))
                visible.targetCell.clear();
            out.doors.push_back(std::move(visible));
        }
    std::set<std::string> adjacent{c->id}, currentlyVisible{c->id};
    for (const Door* door : doorsIn(o->cellId))
        if (door->portal)
        {
            adjacent.insert(door->targetCell);
            if (visiblePortal(*o, *door))
                currentlyVisible.insert(door->targetCell);
        }
    for (const auto& id : adjacent)
    {
        const auto known = book.find(id);
        if (known == book.end() || known->second.knowledge == Knowledge::Unknown)
            continue;
        const auto& m = known->second;
        MapCell mc;
        mc.id = id;
        mc.name = m.name;
        mc.width = m.width;
        mc.height = m.height;
        mc.worldX = m.worldX;
        mc.worldY = m.worldY;
        mc.worldZ = m.worldZ;
        mc.knowledge = m.knowledge;
        mc.current = id == c->id;
        mc.visible = currentlyVisible.count(id) != 0;
        if (m.knowledge == Knowledge::Visited)
        {
            mc.rememberedGlyphs = m.glyphs;
            for (std::size_t index = 0; index < mc.rememberedGlyphs.size(); ++index)
                if (index >= m.observed.size() || !m.observed[index])
                    mc.rememberedGlyphs[index] = ' ';
        }
        out.worldMap.push_back(std::move(mc));
    }
    for (const auto& a : out.worldMap)
        for (const auto& b : out.worldMap)
            if (a.visible && b.visible && std::abs(a.worldX - b.worldX) < .01 && std::abs(a.worldY - b.worldY) < .01 &&
                std::abs(a.worldZ - b.worldZ) > .01)
                out.isometric = true;
    return out;
}

PersistedWorld World::save() const
{
    PersistedWorld out;
    out.time = time_;
    out.clockOffsetHours = clockOffsetHours_;
    out.calendarDays = calendarDays_;
    out.hasSociety = true;
    out.society = society_.state();
    out.memories = memories_;
    out.bonds = bonds_.save();
    out.promises = promises_;
    out.roads = roads_;
    out.crime = crime_;
    out.festivals = festivals_;
    out.roads.beliefs.clear();
    for (const auto& [holder, mine] : beliefs_)
        out.roads.beliefs.insert(out.roads.beliefs.end(), mine.begin(), mine.end());
    for (const auto& entry : entities_)
    {
        if (entry.second.transient)
            continue;                               // Road folk come back from the roads' own state.
        Entity e = entry.second;
        clearTransientMotion(e);
        e.typing = false;
        e.speakingUntil = 0;
        if (e.npc)
            out.npcs.push_back(std::move(e));
        else
            out.players.push_back(std::move(e));
    }
    for (const auto& entry : doors_)
        if (!(entry.second.passage && entry.second.boundary))
            out.doorStates[entry.first] = entry.second.open;
    for (const auto& entry : cells_)
    {
        out.weather[entry.first] = entry.second.weather;
        out.winds[entry.first] = entry.second.wind;
        out.lighting[entry.first] = entry.second.lighting;
        out.seasonalWeather[entry.first] = entry.second.seasonalWeather;
    }
    out.fronts = fronts_;
    return out;
}
Result World::restore(const PersistedWorld& state)
{
    // Beyond this bound double precision can no longer support useful frame
    // time and conversion of schedule epochs can overflow an integer.
    if (!std::isfinite(state.time) || state.time < 0 || state.time > 1e12)
        return {false, "Invalid saved clock.", {}};
    if (!std::isfinite(state.clockOffsetHours) || state.clockOffsetHours < 0 || state.clockOffsetHours >= 24)
        return {false, "Invalid saved time-of-day offset.", {}};
    if (!std::isfinite(state.calendarDays) || state.calendarDays < -1 || state.calendarDays > calendar::MaxGameDays ||
        (state.calendarDays < 0 && state.calendarDays != -1)) return {false, "Invalid saved calendar.", {}};
    const double restoredDays = state.calendarDays >= 0 ? state.calendarDays :
        std::fmod(state.clockOffsetHours + std::fmod(state.time, 7200.) * 24. / 7200., 24.) / 24.;
    if (streamed())
    {
        for (const auto* list : {&state.players, &state.npcs})
            for (const auto& e : *list)
                ensureLoaded(e.cellId);
        if (state.hasSociety)
            for (const auto& resident : state.society.residents)
            {
                ensureLoaded(resident.second.homeCell);
                if (!resident.second.relocationCell.empty())
                    ensureLoaded(resident.second.relocationCell);
            }
    }
    Society restoredSociety = society_; // Carries the authored population to validate against.
    restoredSociety.reset(society_.roster());
    if (state.hasSociety && !restoredSociety.restore(state.society)) return {false, "Invalid saved society or money ledger.", {}};
    Bonds restoredBonds;
    if (!restoredBonds.restore(state.bonds)) return {false, "Invalid saved bonds.", {}};
    if (state.hasSociety &&
        (state.society.enabled != society_.state().enabled || restoredSociety.roster() != society_.roster()))
        return {false, "Society does not match authored world.", {}};
    if (state.hasSociety && state.society.budgetDay > std::floor(restoredDays)) return {false, "Society budget is in the future.", {}};
    std::vector<std::string> residentIds;
    for (const auto& resident : restoredSociety.state().residents)
        residentIds.push_back(resident.first);
    for (const auto& id : residentIds)
    {
        const auto validHome = [&](const std::string& cellId, double x, double y) {
            const auto* c = cell(cellId);
            if (!c || !std::isfinite(x) || !std::isfinite(y) || x < .5 || y < .5 ||
                x > c->width - .5 || y > c->height - .5) return false;
            const auto* t = c->tile(int(x), int(y)); return t && !t->solid;
        };
        const auto usable = [&] {
            const auto& life = *restoredSociety.resident(id);
            return validHome(life.homeCell, life.homeX, life.homeY) &&
                   (life.relocationCell.empty() || validHome(life.relocationCell, life.relocationX, life.relocationY));
        };
        // A home the world no longer has (the building was rebuilt or moved): back to the authored bed.
        if (!usable() && !(restoredSociety.rehome(id) && usable()))
            return {false, "Saved resident home or relocation is invalid.", id};
    }
    for (const auto& e : state.players)
    {
        if (state.hasSociety && (e.id.rfind("player-", 0) == 0 || e.id.rfind("wolf-", 0) == 0) &&
            !restoredSociety.account(e.id))
            return {false, "Saved character has no economy account.", e.id};
        if (!state.hasSociety) restoredSociety.addPlayer(e.id);
    }
    for (const auto& mode : state.seasonalWeather) if (!cell(mode.first)) return {false, "Unknown weather mode cell.", mode.first};
    std::map<std::string, bool> effectiveDoors;
    for (const auto& d : doors_)
        effectiveDoors[d.first] = d.second.open;
    for (const auto& d : state.doorStates)
    {
        if (!doors_.count(d.first))
        {
            if (d.first.rfind("seam_", 0) == 0)
                continue;                  // Older saves kept seams (always open); streamed cells may not be loaded.
            return {false, "Unknown saved fixture.", d.first};
        }
        effectiveDoors[d.first] = d.second;
    }
    for (const auto& entry : doors_)
    {
        const auto& d = entry.second;
        if (!d.linkedDoor.empty() && effectiveDoors.count(d.linkedDoor) &&
            effectiveDoors[d.id] != effectiveDoors[d.linkedDoor])
            return {false, "Linked door states disagree.", d.id};
        if ((d.passage || d.id.find("stairs_") == 0) && !effectiveDoors[d.id])
            return {false, "Steps cannot be closed.", d.id};
    }
    auto validActor = [&](const Entity& e) {
        const auto* c = cell(e.cellId);
        if (!validAppearance(e.appearance)) return false;
        if (e.age < 0 || e.age > 10000 || !std::isfinite(e.lastBirthdayDay) || e.lastBirthdayDay < -1 ||
            e.lastBirthdayDay > calendar::MaxGameDays || e.lastBirthdayDay > restoredDays + 1 || (e.lastBirthdayDay < 0 && e.lastBirthdayDay != -1) ||
            !std::isfinite(e.strength) || e.strength < 0 || e.strength > 100 || !std::isfinite(e.wisdom) ||
            e.wisdom < 0 || e.wisdom > 100 || e.ageNoticePending < 0 || e.ageNoticePending > 10000) return false;
        if (!c || !finite(e.position) || e.position.x < 0 || e.position.y < 0 || e.position.x >= c->width ||
            e.position.y >= c->height || !std::isfinite(e.facing) || !std::isfinite(e.hearing) ||
            !std::isfinite(e.vision) || !std::isfinite(e.earHealth) || !std::isfinite(e.eyeHealth) || e.hearing < 0 ||
            e.vision < 0 || e.earHealth < 0 || e.earHealth > 1 || e.eyeHealth < 0 || e.eyeHealth > 1 ||
            !std::isfinite(e.turnTarget) || !std::isfinite(e.sneakSkill) || !std::isfinite(e.hearingSkill) ||
            e.sneakSkill < 0 || e.sneakSkill > 100 || e.hearingSkill < 0 || e.hearingSkill > 100 ||
            !std::isfinite(e.smell) || !std::isfinite(e.noseHealth) || !std::isfinite(e.scentSkill) || e.smell < 0 ||
            e.noseHealth < 0 || e.noseHealth > 1 || e.scentSkill < 0 || e.scentSkill > 100 ||
            !std::isfinite(e.dexterity) || e.dexterity < 0 || e.dexterity > 100 || !std::isfinite(e.stamina) ||
            e.stamina < 0 || e.stamina > 100 || e.pace < 0 || e.pace > 10 || !std::isfinite(e.staminaRate) ||
            e.staminaRate < -SprintDrain - Epsilon || e.staminaRate > StaminaRecovery + Epsilon ||
            (e.exhausted && e.stamina > ExhaustionRecovery) || !std::isfinite(e.postureRemaining) ||
            e.postureRemaining < 0 || e.postureRemaining > 1.0 ||
            (e.posture != "rising" && !stablePosture(e.posture)) ||
            (e.posture == "rising" &&
             (e.postureRemaining <= 0 || (e.postureTarget != "standing" && e.postureTarget != "crouching"))) ||
            (e.posture != "rising" && (e.postureRemaining != 0 || !e.postureTarget.empty())) || e.speakingColor < 0 ||
            e.speakingColor > 31)
            return false;
        for (Vec2 p : {e.position, Vec2{e.position.x - Radius, e.position.y}, Vec2{e.position.x + Radius, e.position.y},
                       Vec2{e.position.x, e.position.y - Radius}, Vec2{e.position.x, e.position.y + Radius}})
        {
            const auto* t = c->tile(int(std::floor(p.x)), int(std::floor(p.y)));
            if (!t || t->solid)
                return false;
            for (const Door* d : doorsIn(e.cellId))
                if (!effectiveDoors[d->id] && doorCovers(*d, p))
                    return false;
        }
        return true;
    };
    std::set<std::string> ids;
    for (const auto& e : state.players)
    {
        if (e.id.empty() || e.npc || !ids.insert(e.id).second || (entity(e.id) && entity(e.id)->npc) || !validActor(e))
            return {false, "Invalid saved player.", e.id};
    }
    // The world may have been rebuilt since the checkpoint: an NPC it no longer has is dropped, and one who stood
    // where a place no longer is (or is now built over) keeps the authored body. Anything malformed still rejects
    // the whole checkpoint.
    std::vector<const Entity*> keptNpcs;
    std::set<std::string> rebuilt;
    const auto placeable = [&](const Entity& e) {
        const auto* c = cell(e.cellId);
        if (!c || !finite(e.position) || e.position.x < 0 || e.position.y < 0 || e.position.x >= c->width ||
            e.position.y >= c->height)
            return false;
        // Its whole footprint, not only the middle: an NPC offstage is saved wherever its last hop ended.
        for (Vec2 p : {e.position, Vec2{e.position.x - Radius, e.position.y}, Vec2{e.position.x + Radius, e.position.y},
                       Vec2{e.position.x, e.position.y - Radius}, Vec2{e.position.x, e.position.y + Radius}})
        {
            const auto* t = c->tile(int(std::floor(p.x)), int(std::floor(p.y)));
            if (!t || t->solid)
                return false;
            for (const Door* d : doorsIn(e.cellId))
                if (!effectiveDoors[d->id] && doorCovers(*d, p))
                    return false;
        }
        return true;
    };
    for (const auto& e : state.npcs)
    {
        if (e.transient || e.id.rfind("road:", 0) == 0)
            continue;                                         // Road folk are never restored as they were.
        if (e.npc && !entity(e.id) && state.hasSociety && !restoredSociety.resident(e.id))
            continue;                                         // A resident the world no longer has.
        if (!e.npc || !entity(e.id) || !entity(e.id)->npc || !ids.insert(e.id).second)
            return {false, "Invalid saved NPC.", e.id};
        if (!placeable(e) && validAppearance(e.appearance))
        {
            rebuilt.insert(e.id);
            continue;
        }
        if (!validActor(e))
            return {false, "Invalid saved NPC.", e.id};
        keptNpcs.push_back(&e);
    }
    if (state.hasSociety)
        for (const auto& life : restoredSociety.state().residents)
        {
            const bool newcomer = !state.society.residents.count(life.first);
            if (!ids.count(life.first) && !rebuilt.count(life.first) &&
                !(newcomer && entity(life.first) && entity(life.first)->npc))
                return {false, "Saved resident has no physical character record.", life.first};
        }
    for (const auto& owner : state.memories)
        for (const auto& entry : owner.second)
        {
            const auto& m = entry.second;
            const auto* c = cell(entry.first);
            const auto tier = static_cast<int>(m.knowledge);
            if (owner.first.empty() || !c || m.cellId != entry.first || tier < 0 || tier > 2 || m.width <= 0 ||
                m.height <= 0 || m.width > 4096 || m.height > 4096 || !std::isfinite(m.worldX) ||
                !std::isfinite(m.worldY) || !std::isfinite(m.worldZ) || m.glyphs.size() != m.observed.size() ||
                (m.knowledge == Knowledge::Visited ? m.glyphs.size() != static_cast<std::size_t>(m.width * m.height)
                                                   : !m.glyphs.empty()))
                return {false, "Invalid saved map memory.", entry.first};
            for (std::size_t index = 0; index < m.glyphs.size(); ++index)
                if (!m.observed[index] && m.glyphs[index] != ' ')
                    return {false, "Unobserved map detail in saved memory.", entry.first};
        }
    for (const auto& w : state.weather)
        if (!cell(w.first) || static_cast<int>(w.second) < 0 || static_cast<int>(w.second) >= WeatherKinds)
            return {false, "Invalid saved weather.", w.first};
    for (const auto& w : state.winds)
        if (!cell(w.first) || !std::isfinite(w.second.direction) || !std::isfinite(w.second.strength) ||
            w.second.strength < 0 || w.second.strength > 1 || (!cell(w.first)->outdoors && w.second.strength != 0))
            return {false, "Invalid saved wind.", w.first};
    for (const auto& light : state.lighting)
        if (!cell(light.first) || !validLighting(light.second))
            return {false, "Invalid saved lighting.", light.first};
    for (auto it = entities_.begin(); it != entities_.end();)
        if (!it->second.npc || it->second.transient)
            it = entities_.erase(it);
        else
            ++it;
    for (auto e : state.players)
    {
        advanceAge(e, restoredDays);
        clearTransientMotion(e);
        e.typing = false;
        e.speakingUntil = 0;
        entities_[e.id] = std::move(e);
    }
    for (const auto* kept : keptNpcs)
    {
        auto e = *kept;
        advanceAge(e, restoredDays);
        clearTransientMotion(e);
        e.typing = false;
        e.speakingUntil = 0;
        entities_[e.id] = std::move(e);
    }
    for (const auto& d : state.doorStates)
        if (doors_.count(d.first))
            doors_[d.first].open = d.second;
    for (const auto& w : state.weather)
        if (cells_.count(w.first))
        {
            cells_[w.first].weather = w.second;
            cells_[w.first].seasonalWeather = false; // Safe legacy/manual mode.
        }
    for (const auto& mode : state.seasonalWeather) cells_[mode.first].seasonalWeather = mode.second;
    fieldStamp_ = std::numeric_limits<std::int64_t>::min();   // The field is worked out again for the restored moment.
    for (const auto& w : state.winds)
        cells_[w.first].wind = {std::remainder(w.second.direction, 2.0 * Pi), w.second.strength, w.second.variable};
    for (const auto& light : state.lighting)
        cells_[light.first].lighting = light.second;
    time_ = state.time;
    clockOffsetHours_ = state.clockOffsetHours;
    calendarDays_ = restoredDays;
    society_ = std::move(restoredSociety);
    climateSlot_ = std::int64_t(std::floor(calendarDays_ * 4));
    fronts_ = state.fronts;
    for (const auto& f : fronts_)
        if (f.id.rfind("front-", 0) == 0)
            frontNext_ = std::max(frontNext_, std::uint64_t(std::strtoull(f.id.c_str() + 6, nullptr, 10)) + 1);
    refreshWeatherField(true);
    memories_ = state.memories;
    lastObserved_.clear();
    bonds_ = std::move(restoredBonds);
    // The roads as saved; a caravan whose load is gone (an older save) is dropped. Towns are worked out again.
    roads_ = state.roads;
    crime_ = state.crime;
    festivals_ = state.festivals;
    ++festivalsChanged_;
    squaresDay_ = -1;
    pursuits_.clear();
    confrontations_.clear();
    marks_.clear();
    fights_.clear();
    crimeHour_ = -1;
    roads_.caravans.erase(std::remove_if(roads_.caravans.begin(), roads_.caravans.end(),
                                         [&](const Caravan& c) { return !society_.account(c.account); }),
                          roads_.caravans.end());
    beliefs_.clear();
    for (const auto& b : state.roads.beliefs)
        if (std::isfinite(b.confidence) && b.confidence > 0 && b.confidence <= 1)
            beliefs_[b.holder].push_back(b);
    roads_.beliefs.clear();
    townsReady_ = false;
    folk_.clear();
    encounters_.clear();
    spared_.clear();
    swingReady_.clear();
    priceHour_ = -1;
    promises_.clear();
    for (const auto& p : state.promises)
        if (!p.by.empty() && !p.to.empty() && p.by.size() <= 80 && p.to.size() <= 80 && p.what.size() <= 200 &&
            std::isfinite(p.made) && std::isfinite(p.due) && (p.status == "open" || p.status == "kept" || p.status == "broken"))
            promises_.push_back(p);
    scheduleAccumulator_ = 0;
    routeWanted_.clear();
    routeCursor_.clear();
    pendingPortals_.clear();
    travels_.clear();
    travelLegCells_.clear();
    travelRetryAt_.clear();
    travelProgress_.clear();
    return {true, "Saved world restored.", {}};
}

Result World::loadCellFile(const std::string& path)
{
    std::error_code error;
    const auto bytes = std::filesystem::file_size(path, error);
    if (error || bytes > 4 * 1024 * 1024)
        return {false, "Missing or oversized cell file.", path};
    std::ifstream file(path);
    if (!file)
        return {false, "Cannot open cell file.", path};
    return loadCell(file, path);
}

Result World::loadCellText(const std::string& text, const std::string& label)
{
    if (text.size() > 4 * 1024 * 1024)
        return {false, "Missing or oversized cell file.", label};
    std::istringstream file(text);
    return loadCell(file, label);
}

Result World::loadCell(std::istream& file, const std::string& path)
{
    Cell candidate;
    const auto parsed = parseCell(file, path, candidate, false);
    if (!parsed.ok)
        return parsed;
    for (const auto& entry : entities_)
        if (entry.second.cellId == candidate.id)
        {
            const auto p = entry.second.position;
            for (const Vec2 sample : {p, Vec2{p.x - Radius, p.y}, Vec2{p.x + Radius, p.y}, Vec2{p.x, p.y - Radius},
                                      Vec2{p.x, p.y + Radius}})
            {
                const auto* tile = candidate.tile(int(std::floor(sample.x)), int(std::floor(sample.y)));
                if (!tile || tile->solid)
                    return {false, "Cell replacement would strand an actor.", entry.first};
            }
        }
    for (const auto& entry : doors_)
    {
        const auto& d = entry.second;
        for (const auto& anchor : {std::make_pair(d.cellId, d.position), std::make_pair(d.targetCell, d.arrival)})
            if (anchor.first == candidate.id)
            {
                if (!finite(anchor.second) || anchor.second.x < 0 || anchor.second.y < 0 ||
                    anchor.second.x >= candidate.width || anchor.second.y >= candidate.height)
                    return {false, "Cell replacement would invalidate a portal anchor.", d.id};
                const auto* tile = candidate.tile(int(anchor.second.x), int(anchor.second.y));
                if (!tile || tile->solid)
                    return {false, "Cell replacement would invalidate a portal anchor.", d.id};
            }
    }
    cells_[candidate.id] = std::move(candidate);
    return {true, "Cell loaded.", path};
}

namespace
{
// "X Y H" as the exporter writes it: two small whole numbers and a height of whole or half units ("3", "-1.5",
// "2.0"). Those read exactly as a stream would read them; anything else is left to the stream (returns false).
bool fastHeight(const std::string& value, int& x, int& y, double& height)
{
    std::size_t at = 0;
    const auto whole = [&](int& out) {
        const std::size_t start = at;
        int number = 0;
        while (at < value.size() && at - start < 4 && value[at] >= '0' && value[at] <= '9')
            number = number * 10 + (value[at++] - '0');
        out = number;
        return at > start && (at == value.size() || value[at] < '0' || value[at] > '9');
    };
    if (!whole(x) || at >= value.size() || value[at++] != ' ' || !whole(y) || at >= value.size() || value[at++] != ' ')
        return false;
    const bool negative = at < value.size() && value[at] == '-';
    if (negative)
        ++at;
    int units = 0;
    if (!whole(units) || units > 16)
        return false;
    double half = 0;
    if (at < value.size())
    {
        if (value[at] != '.' || at + 2 != value.size() || (value[at + 1] != '0' && value[at + 1] != '5'))
            return false;
        half = value[at + 1] == '5' ? .5 : 0;
    }
    height = negative ? -(units + half) : units + half;
    return true;
}
} // namespace

Result World::parseCell(std::istream& file, const std::string& path, Cell& candidate, bool headerOnly) const
{
    std::string line;
    int sizeW = 0, sizeH = 0;
    std::vector<std::string> rows;
    std::set<std::string> headers;
    // Height overrides, and which tiles already have one (a large cell has tens of thousands of them).
    struct HeightOverride
    {
        int x, y;
        double height;
    };
    std::vector<HeightOverride> heights;
    std::vector<bool> heightSet;
    bool inGrid = false;
    bool authoredWind = false;
    while (std::getline(file, line))
    {
        if (line.size() > 32768)
            return {false, "Cell line exceeds the supported length.", path};
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        for (unsigned char ch : line)
            if (ch < 32 || ch == 127)
                return {false, "Control character in cell content.", path};
        if (inGrid)
        {
            if (line.empty() || line.size() > 512 || rows.size() >= 512)
                return {false, "Invalid cell grid dimensions.", path};
            rows.push_back(line);
            continue;
        }
        if (line == "grid:")
        {
            inGrid = true;
            if (headerOnly)
                break;
            continue;
        }
        const auto colon = line.find(':');
        if (colon == std::string::npos)
            return {false, "Unknown cell header syntax.", path};
        const std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        if (!value.empty() && value.front() == ' ')
            value.erase(value.begin());
        if (key != "height" && !headers.insert(key).second)
            return {false, "Duplicate cell header.", path};
        if (key == "id")
            candidate.id = value;
        else if (key == "name")
        {
            if (value.empty() || value.size() > 512)
                return {false, "Invalid cell name.", path};
            candidate.name = value;
        }
        else if (key == "description")
        {
            if (value.size() > 16384)
                return {false, "Cell description is too long.", path};
            candidate.description = value;
        }
        else if (key == "outdoors")
        {
            if (value != "true" && value != "false")
                return {false, "Invalid outdoors flag.", path};
            candidate.outdoors = value == "true";
        }
        else if (key == "world")
        {
            std::istringstream numbers(value);
            numbers >> candidate.worldX >> candidate.worldY >> candidate.worldZ;
            if (!numbers || !(numbers >> std::ws).eof() || !std::isfinite(candidate.worldX) ||
                !std::isfinite(candidate.worldY) || !std::isfinite(candidate.worldZ) ||
                std::abs(candidate.worldX) > 1e9 || std::abs(candidate.worldY) > 1e9 ||
                std::abs(candidate.worldZ) > 1e6)
                return {false, "Invalid world position.", path};
        }
        else if (key == "weather")
        {
            if (!parseWeather(value, candidate.weather))
                return {false, "Unknown weather value.", path};
        }
        else if (key == "wind")
        {
            int variable = -1;
            std::istringstream numbers(value);
            numbers >> candidate.wind.direction >> candidate.wind.strength >> variable;
            if (!numbers || !(numbers >> std::ws).eof() || !std::isfinite(candidate.wind.direction) ||
                !std::isfinite(candidate.wind.strength) || candidate.wind.strength < 0 || candidate.wind.strength > 1 ||
                (variable != 0 && variable != 1))
                return {false, "Invalid cell wind.", path};
            candidate.wind.direction = std::remainder(candidate.wind.direction, 2.0 * Pi);
            candidate.wind.variable = variable == 1;
            authoredWind = true;
        }
        else if (key == "lighting")
        {
            std::istringstream values(value);
            values >> candidate.lighting.artificial >> candidate.lighting.daylightAccess >> candidate.lighting.tone;
            if (!values || !(values >> std::ws).eof() || !validLighting(candidate.lighting))
                return {false, "Invalid cell lighting.", path};
        }
        else if (key == "size")
        {
            std::istringstream numbers(value);
            numbers >> sizeW >> sizeH;
            if (!numbers || !(numbers >> std::ws).eof() || sizeW < 4 || sizeH < 4 || sizeW > 256 || sizeH > 256)
                return {false, "Invalid cell size.", path};
        }
        else if (key == "height")
        {
            int x = -1, y = -1;
            double height = 0;
            if (!fastHeight(value, x, y, height))
            {
                std::istringstream numbers(value);
                numbers >> x >> y >> height;
                if (!numbers || !(numbers >> std::ws).eof())
                    return {false, "Invalid or duplicate height override.", path};
            }
            if (x < 0 || y < 0 || x >= 512 || y >= 512 || !std::isfinite(height) || height < -16 || height > 16 ||
                std::abs(height * 2 - std::round(height * 2)) > 1e-8)
                return {false, "Invalid or duplicate height override.", path};
            if (heightSet.empty())
                heightSet.assign(512 * 512, false);
            if (heightSet[std::size_t(y) * 512 + std::size_t(x)])
                return {false, "Invalid or duplicate height override.", path};
            heightSet[std::size_t(y) * 512 + std::size_t(x)] = true;
            heights.push_back({x, y, height});
        }
        else
            return {false, "Unknown cell header.", path};
    }
    const bool validId = !candidate.id.empty() && candidate.id.size() <= 48 && candidate.id.front() >= 'a' &&
                         candidate.id.front() <= 'z' &&
                         std::all_of(candidate.id.begin(), candidate.id.end(), [](char ch) {
                             return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
                         });
    if (headerOnly)
    {
        // A streamed cell's header: its size is given, its tiles come later.
        if (!validId || !headers.count("name") || !headers.count("world") || !headers.count("outdoors") ||
            !headers.count("weather") || !sizeW || !sizeH)
            return {false, "Invalid streamed cell header.", path};
        candidate.width = sizeW;
        candidate.height = sizeH;
        candidate.loaded = false;
    }
    else
    {
        if (!file.eof() || !validId || !headers.count("name") || !headers.count("world") || !headers.count("outdoors") ||
            !headers.count("weather") || !inGrid || rows.empty() || rows.size() > 512 || rows.front().empty() ||
            rows.front().size() > 512)
            return {false, "Invalid cell dimensions.", path};
        candidate.width = int(rows.front().size());
        candidate.height = int(rows.size());
        if (sizeW && (sizeW != candidate.width || sizeH != candidate.height))
            return {false, "Cell size header disagrees with its grid.", path};
        candidate.tiles.reserve(rows.size() * rows.front().size());
        for (const auto& row : rows)
        {
            if (row.size() != rows.front().size())
                return {false, "Cell rows have different widths.", path};
            for (char glyph : row)
            {
                if (!terrainInfo(glyph))
                    return {false, "Unknown terrain glyph.", path};
                candidate.tiles.push_back(fromGlyph(glyph));
            }
        }
        for (const auto& height : heights)
        {
            auto* tile = candidate.tile(height.x, height.y);
            if (!tile)
                return {false, "Height override is outside its cell.", path};
            tile->height = height.height;
        }
    }
    if (authoredWind && !candidate.outdoors && candidate.wind.strength != 0)
        return {false, "Indoor cells cannot have outdoor wind.", path};
    if (candidate.outdoors && !authoredWind)
        candidate.wind = {0.0, .5, true};
    return {true, "Cell read.", path};
}

std::size_t World::loadedCells() const
{
    return std::size_t(std::count_if(cells_.begin(), cells_.end(), [](const auto& c) { return c.second.loaded; }));
}

void World::stream(double idle)
{
    if (!streamed())
        return;
    // Runs every tick, so it only does real work when someone has changed cells or an unload check is due.
    // The cells characters are in; offstage NPCs need none (they travel without the ground in memory).
    std::set<std::string> occupied;
    for (const auto& entry : entities_)
        if (!entry.second.offstage)
            occupied.insert(entry.second.cellId);
    const bool due = time_ >= streamCheck_;
    if (!due && occupied == streamOccupied_)
        return;
    std::set<std::string> wanted = occupied;
    if (tiered())
        wanted.insert(tierWanted_.begin(), tierWanted_.end());   // Worked out each schedule update.
    else
        for (const auto& id : occupied)
        {
            const auto& next = neighborList(id);
            wanted.insert(next.begin(), next.end());
        }
    for (const auto& id : wanted)
    {
        needed_[id] = time_;
        ensureLoaded(id);
    }
    streamOccupied_ = std::move(occupied);
    if (!due)
        return;
    streamCheck_ = time_ + 10.0;
    std::vector<std::string> idle_;
    for (const auto& entry : cells_)
        if (entry.second.loaded && !wanted.count(entry.first) && time_ - needed_[entry.first] >= idle)
            idle_.push_back(entry.first);
    for (const auto& id : idle_)
        unload(id);
}

std::vector<std::string> World::cellsSoonNeeded() const
{
    std::set<std::string> out;
    if (!streamed())
        return {};
    for (const auto& [id, c] : cells_)
        if (c.loaded)
            for (const auto& next : neighborList(id))
                if (const auto* n = cell(next); n && !n->loaded)
                    out.insert(next);
    return {out.begin(), out.end()};
}

void World::unload(const std::string& cellId)
{
    auto* c = cell(cellId);
    if (!c || !c->loaded || !streamed())
        return;
    // This cell's seams leave doors_ and its door list; nothing else refers to them (see indexSeams()).
    const auto exits = exits_.find(cellId);
    portalGrid_.erase(cellId);
    bool unexpected = false;
    auto& here = doorsIn_[cellId];
    std::vector<Door*> kept;
    for (Door* d : here)
        if (d->passage && d->boundary)
        {
            unexpected |= exits == exits_.end() || !exits->second.count(d->targetCell);
            const std::string id = d->id;
            doors_.erase(id);
        }
        else
            kept.push_back(d);
    here = std::move(kept);
    if (unexpected)
        rebuildFixtureIndex();
    c->tiles.clear();
    c->tiles.shrink_to_fit();
    c->loaded = false;
    needed_.erase(cellId);
}

} // namespace ratw
