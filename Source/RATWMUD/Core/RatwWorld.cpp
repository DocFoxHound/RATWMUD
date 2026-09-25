#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <functional>
#include <limits>
#include <queue>
#include <set>
#include <sstream>

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
constexpr double Epsilon = 1e-7;
constexpr double Pi = 3.14159265358979323846;
constexpr double TurnSpeed = Pi; // Radians per second: 180 degrees.
Weather worldWeather(calendar::Weather value)
{
    switch (value) { case calendar::Weather::Rain: return Weather::Rain; case calendar::Weather::Snow: return Weather::Snow;
        case calendar::Weather::Fog: return Weather::Fog; default: return Weather::Clear; }
}
calendar::Weather skyWeather(Weather value)
{
    switch (value) { case Weather::Rain: return calendar::Weather::Rain; case Weather::Snow: return calendar::Weather::Snow;
        case Weather::Fog: return calendar::Weather::Fog; default: return calendar::Weather::Clear; }
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
    switch (g)
    {
    case '#':
        t.terrain = Terrain::Wall;
        t.solid = t.opaque = true;
        break;
    case 'T':
        t.terrain = Terrain::Table;
        t.solid = true;
        break;
    case '=':
        t.terrain = Terrain::Counter;
        t.solid = true;
        break;
    case '~':
        t.terrain = Terrain::Water;
        t.movementCost = 2.4;
        break;
    case ',':
    case '"':
        t.terrain = Terrain::Grass;
        t.movementCost = 1.12;
        break;
    case '^':
        t.terrain = Terrain::Stairs;
        t.height = 0.5;
        t.movementCost = 1.25;
        break;
    case ':':
        t.height = 0.25;
        t.movementCost = 1.12;
        break;
    default:
        break;
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
    AirRoutes(const Cell& c, const std::map<std::string, Door>& doors, Vec2 origin)
        : c_(c), origin_(origin), open_(c.tiles.size(), false)
    {
        for (std::size_t i = 0; i < c.tiles.size(); ++i)
            open_[i] = !c.tiles[i].opaque && c.tiles[i].terrain != Terrain::Wall;
        for (const auto& entry : doors)
        {
            const auto& d = entry.second;
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
    default:
        return "clear";
    }
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
    for (const auto& entry : doors_)
    {
        const auto& d = entry.second;
        if (!d.passage && d.id.rfind("stairs_", 0) != 0)
            blockingFixtures_[d.cellId][{int(std::floor(d.position.x)), int(std::floor(d.position.y))}].push_back(d.id);
    }
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
const Door* World::door(const std::string& id) const
{
    const auto it = doors_.find(id);
    return it == doors_.end() ? nullptr : &it->second;
}

bool World::passable(const std::string& cellId, Vec2 p, double fromHeight) const
{
    const auto* c = cell(cellId);
    if (!c || !finite(p))
        return false;
    for (Vec2 s :
         {p, Vec2{p.x - Radius, p.y}, Vec2{p.x + Radius, p.y}, Vec2{p.x, p.y - Radius}, Vec2{p.x, p.y + Radius}})
    {
        const auto* t = c->tile(int(std::floor(s.x)), int(std::floor(s.y)));
        if (!t || t->solid || std::abs(t->height - fromHeight) > 0.75)
            return false;
        if (blockedByDoor(cellId, s))
            return false;
    }
    return true;
}

bool World::lineOfSight(const std::string& cellId, Vec2 from, Vec2 to) const
{
    const auto* c = cell(cellId);
    if (!c || !finite(from) || !finite(to))
        return false;
    if (from.x < 0 || from.y < 0 || from.x >= c->width || from.y >= c->height || to.x < 0 || to.y < 0 ||
        to.x >= c->width || to.y >= c->height)
        return false;
    const int count = std::max(1, int(std::ceil(distance(from, to) / 0.12)));
    for (int i = 1; i < count; ++i)
    {
        const double f = double(i) / count;
        const Vec2 p{from.x + (to.x - from.x) * f, from.y + (to.y - from.y) * f};
        // The occluding destination itself is visible, without revealing beyond it.
        if (int(std::floor(p.x)) == int(std::floor(to.x)) && int(std::floor(p.y)) == int(std::floor(to.y)))
            continue;
        const auto* t = c->tile(int(std::floor(p.x)), int(std::floor(p.y)));
        if (!t || t->opaque)
            return false;
        if (blockedByDoor(cellId, p))
            return false;
    }
    return true;
}

double World::sightRange(const Entity& o) const
{
    return 27.0 * std::max(0.0, o.vision) * ageVisionFactor(o) * clamp01(o.eyeHealth) * environmentAt(o.cellId).sight;
}
bool World::visiblePoint(const Entity& o, Vec2 p) const
{
    return distance(o.position, p) <= sightRange(o) && lineOfSight(o.cellId, o.position, p);
}
bool World::visiblePortal(const Entity& o, const Door& d) const
{
    return d.cellId == o.cellId && d.portal && d.open && visiblePoint(o, d.position);
}

std::vector<Vec2> World::findPath(const Entity& a, Vec2 goal, bool allowClosed) const
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
    auto allowed = [&](Vec2 p, double height) {
        if (!allowClosed)
            return passable(a.cellId, p, height);
        for (Vec2 s :
             {p, Vec2{p.x - Radius, p.y}, Vec2{p.x + Radius, p.y}, Vec2{p.x, p.y - Radius}, Vec2{p.x, p.y + Radius}})
        {
            const auto* t = c->tile(int(std::floor(s.x)), int(std::floor(s.y)));
            if (!t || t->solid || std::abs(t->height - height) > .75)
                return false;
        }
        return true;
    };
    const auto* dest = c->tile(int(goal.x), int(goal.y));
    if (!dest || !allowed(goal, dest->height))
        return {};
    using QueueItem = std::pair<double, int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> open;
    std::vector<double> g(static_cast<std::size_t>(w * h), std::numeric_limits<double>::infinity());
    std::vector<int> previous(static_cast<std::size_t>(w * h), -1);
    std::vector<bool> closed(static_cast<std::size_t>(w * h), false);
    g[start] = 0;
    open.push({distance(pos(start), goal), start});
    while (!open.empty())
    {
        const int current = open.top().second;
        open.pop();
        if (closed[current])
            continue;
        if (current == end)
            break;
        closed[current] = true;
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
                if (closed[next] || !allowed(q, tile->height))
                    continue;
                if (dx && dy && (!allowed({p.x, q.y}, tile->height) || !allowed({q.x, p.y}, tile->height)))
                    continue;
                const auto* nt = c->tile(int(q.x), int(q.y));
                const double cost = distance(p, q) * nt->movementCost + std::abs(nt->height - tile->height) * .3;
                if (g[current] + cost + Epsilon < g[next])
                {
                    g[next] = g[current] + cost;
                    previous[next] = current;
                    open.push({g[next] + distance(q, goal), next});
                }
            }
    }
    if (start != end && previous[end] < 0)
        return {};
    std::vector<Vec2> path;
    for (int i = end; i != start; i = previous[i])
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
        // and approximately preserving the terrain cost chosen by A*.
        std::vector<Vec2> smooth;
        Vec2 anchor = a.position;
        std::size_t first = 0;
        while (first < path.size())
        {
            std::size_t chosen = first;
            double routeCost = 0;
            Vec2 priorWaypoint = anchor;
            for (std::size_t candidate = first; candidate < path.size(); ++candidate)
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
                    if (!pt || !allowed(p, pt->height))
                    {
                        clear = false;
                        break;
                    }
                    const auto* nt = c->tile(int(p.x), int(p.y));
                    directCost += distance(prior, p) * nt->movementCost;
                    prior = p;
                }
                if (clear && directCost <= routeCost + .025)
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
    for (const auto& entry : doors_)
    {
        const auto& d = entry.second;
        if (d.cellId != a->cellId || !d.boundary)
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
        for (const auto& p : permissive)
        {
            const auto* t = c->tile(int(a->position.x), int(a->position.y));
            if (!passable(a->cellId, p, t ? t->height : 0))
            {
                for (const auto& entry : doors_)
                    if (entry.second.cellId == a->cellId && !entry.second.open &&
                        distance(entry.second.position, p) < 1.1)
                    {
                        a->path = path;
                        if (!a->path.empty())
                            prepareMovement(*a);
                        return {true, "Approaching the closed " + entry.second.name + ". Choose Open to continue.",
                                entry.first};
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
        if (!anchor || !passable(d.targetCell, arrival, anchor->height))
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
            speed *= environmentAt(c->id).movement;
            double travel = speed * dt;
            if (!a.path.empty())
                travel = std::min(travel, distance(a.position, a.path.front()));
            const Vec2 delta{direction.x * travel, direction.y * travel};
            const Vec2 proposed{a.position.x + delta.x, a.position.y + delta.y};
            for (const auto& entry : doors_)
            {
                const auto& d = entry.second;
                if (d.cellId != a.cellId || !d.portal || !d.boundary || !d.open)
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
            Vec2 accepted = a.position;
            if (passable(a.cellId, proposed, startTile->height))
                accepted = proposed;
            else
            {
                const Vec2 slideX{proposed.x, a.position.y};
                const Vec2 slideY{a.position.x, proposed.y};
                if (std::abs(delta.x) > Epsilon && passable(a.cellId, slideX, startTile->height))
                    accepted = slideX;
                if (std::abs(delta.y) > Epsilon && passable(a.cellId, {accepted.x, slideY.y}, startTile->height))
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

void World::separate(double dt)
{
    for (auto i = entities_.begin(); i != entities_.end(); ++i)
    {
        auto j = i;
        for (++j; j != entities_.end(); ++j)
        {
            auto& a = i->second;
            auto& b = j->second;
            if (a.cellId != b.cellId)
                continue;
            const double d = distance(a.position, b.position);
            if (d >= Radius * 2)
                continue;
            bool atPortal = false;
            for (const auto& entry : doors_)
                if (entry.second.cellId == a.cellId && entry.second.portal &&
                    (distance(a.position, entry.second.position) < 1.7 ||
                     distance(b.position, entry.second.position) < 1.7))
                {
                    atPortal = true;
                    break;
                }
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
            if (ta && passable(a.cellId, pa, ta->height))
                a.position = pa;
            if (tb && passable(b.cellId, pb, tb->height))
                b.position = pb;
        }
    }
}

Result World::relocateResident(const std::string& id, const std::string& destination, double x, double y)
{
    auto* actor = entity(id);
    const auto* targetCell = cell(destination);
    const auto* life = society_.resident(id);
    if (!actor || !actor->npc || !life || life->role != "resident" || !actor->leaderId.empty() ||
        actor->state == "following" || !targetCell || !std::isfinite(x) || !std::isfinite(y) ||
        x < .5 || y < .5 || x > targetCell->width - .5 || y > targetCell->height - .5)
        return {false, "Choose an existing, non-recruited resident and a traversable home; essential jobs are protected.", id};
    const auto* tile = targetCell->tile(int(x), int(y));
    if (!tile || !passable(destination, {x, y}, tile->height))
        return {false, "The proposed home is blocked.", id};
    std::queue<std::string> pending;
    std::set<std::string> reached{actor->cellId}; pending.push(actor->cellId);
    while (!pending.empty())
    {
        const auto current = pending.front(); pending.pop();
        for (const auto& entry : doors_)
            if (entry.second.portal && !entry.second.locked && entry.second.cellId == current &&
                reached.insert(entry.second.targetCell).second) pending.push(entry.second.targetCell);
    }
    if (!reached.count(destination) || (actor->cellId == destination &&
        distance(actor->position, {x, y}) > .35 && findPath(*actor, {x, y}, true).empty()))
        return {false, "No authored route reaches the proposed home.", id};
    if (!society_.relocate(id, destination, x, y)) return {false, "Resident already relocating or invalid home.", id};
    stop(id);
    return {true, "Relocation accepted; the resident must physically arrive before the home changes.", id};
}

void World::updateSchedules()
{
    std::map<std::string, LifeBody> bodies;
    for (auto& pair : entities_)
    {
        advanceAge(pair.second, calendarDays_);
        const auto& e = pair.second;
        bodies[pair.first] = {e.cellId, e.position.x, e.position.y, !e.leaderId.empty() || e.state == "following"};
    }
    society_.tick(.5, calendarDays_, int(calendar::calendarAt(calendarDays_).season), bodies);
    if (customWorld_) return;
    for (auto& pair : entities_)
    {
        auto& e = pair.second;
        const auto* life = society_.resident(pair.first);
        if (!e.npc || !life || life->goalCell.empty() || !e.leaderId.empty() || e.state == "following") continue;
        const std::string activity = life->task + " — " + life->reason;
        if (e.activity != activity) { stop(e.id); e.activity = activity; }
        const Vec2 target{life->goalX, life->goalY};
        if (e.cellId == life->goalCell && distance(e.position, target) <= .35)
        {
            if (life->task == "sleep" && e.posture != "lying") setPosture(e.id, "lying");
            continue;
        }
        if (e.posture == "lying" || e.posture == "sitting") setPosture(e.id, "standing");
        if (!e.path.empty()) continue;
        if (e.cellId == life->goalCell)
        {
            auto result = moveTo(e.id, target.x, target.y);
            if (const auto* barrier = door(result.targetId))
                if (!barrier->locked && distance(e.position, barrier->position) <= barrier->reach)
                    interact(e.id, barrier->id, "open");
            continue;
        }
        // Residents know their authored work/home routes, not player exploration.
        std::queue<std::string> pending;
        std::map<std::string, std::string> firstDoor;
        pending.push(e.cellId);
        firstDoor[e.cellId] = "";
        while (!pending.empty() && !firstDoor.count(life->goalCell))
        {
            const auto current = pending.front(); pending.pop();
            for (const auto& d : doors_)
                if (d.second.portal && !d.second.locked && d.second.cellId == current &&
                    !firstDoor.count(d.second.targetCell))
                {
                    firstDoor[d.second.targetCell] = current == e.cellId ? d.first : firstDoor[current];
                    pending.push(d.second.targetCell);
                }
        }
        if (!firstDoor.count(life->goalCell)) { e.activity = life->task + " — route unavailable"; continue; }
        const auto* d = door(firstDoor[life->goalCell]);
        if (!d) continue;
        if (distance(e.position, d->position) <= d->reach)
            interact(e.id, d->id, d->open ? "enter" : "open");
        else
        {
            auto result = moveTo(e.id, d->position.x, d->position.y);
            if (const auto* barrier = door(result.targetId))
                if (!barrier->locked && distance(e.position, barrier->position) <= barrier->reach)
                    interact(e.id, barrier->id, "open");
        }
    }
}

void World::tick(double dt)
{
    if (!std::isfinite(dt) || dt <= 0)
        return;
    // Bounded steps prevent tunneling. The hosting server should use 1/30 s;
    // even delayed input cannot tunnel through an entire terrain feature.
    dt = std::min(dt, 60.0);
    while (dt > Epsilon)
    {
        const double step = std::min(dt, 1.0 / 30.0);
        time_ += step;
        calendarDays_ = std::min(calendar::MaxGameDays, calendarDays_ + step / DaySeconds);
        const auto slot = std::int64_t(std::floor(calendarDays_ * 4));
        if (slot != climateSlot_)
        {
            climateSlot_ = slot;
            for (auto& cellPair : cells_)
                if (cellPair.second.outdoors && cellPair.second.seasonalWeather)
                    cellPair.second.weather = worldWeather(calendar::forecastAt(0x52415457, cellPair.first, calendarDays_).weather);
        }
        scheduleAccumulator_ += step;
        if (scheduleAccumulator_ + 1e-9 >= .5)
        {
            updateSchedules();
            scheduleAccumulator_ = std::max(0., scheduleAccumulator_ - .5);
        }
        for (auto& entry : entities_)
        {
            updateTravel(entry.second);
            integrate(entry.second, step);
        }
        separate(step);
        dt -= step;
    }
    for (const auto& entry : entities_)
        if (!entry.second.npc)
            observe(entry.first);
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
    range *= environmentAt(oc->id).hearing;
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
    for (const auto& entry : doors_)
    {
        const auto& d = entry.second;
        if (d.cellId != s->cellId || !d.portal || d.targetCell != o->cellId)
            continue;
        double route = distance(s->position, d.position) + distance(o->position, d.arrival) + 2.0;
        double transmission = d.open ? .80 : .24;
        if (!lineOfSight(s->cellId, s->position, d.position))
            transmission *= .45;
        if (!lineOfSight(o->cellId, d.arrival, o->position))
            transmission *= .45;
        best = std::max(best, clarity(route, range * transmission * environmentAt(sc->id).hearing));
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
    range *= environmentAt(c->id).hearing;
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
    out.illumination = calendar::skyAt(calendarDays_, skyWeather(c->weather)).outdoorIllumination;
    out.lightSource = out.daylight <= Epsilon ? "night sky" : "daylight";
    out.sight = out.illumination;
    switch (c->weather)
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
    default:
        break;
    }
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
    for (auto& c : cells_) if (c.second.outdoors && c.second.seasonalWeather) useSeasonalWeather(c.first);
    return {true, "Time of day updated.", {}};
}
Result World::advanceCalendar(double days)
{
    if (!std::isfinite(days) || days < 0 || days > 36500 || calendarDays_ + days > calendar::MaxGameDays)
        return {false, "Calendar advance must be finite, forward and no more than 100 years.", {}};
    calendarDays_ += days;
    climateSlot_ = -1;
    for (auto& e : entities_) advanceAge(e.second, calendarDays_);
    for (auto& c : cells_) if (c.second.outdoors && c.second.seasonalWeather) useSeasonalWeather(c.first);
    return {true, "Shared calendar advanced.", {}};
}
Result World::useSeasonalWeather(const std::string& id)
{
    auto* c = cell(id);
    if (!c || !c->outdoors) return {false, "Seasonal weather requires an outdoor cell.", id};
    c->seasonalWeather = true;
    c->weather = worldWeather(calendar::forecastAt(0x52415457, id, calendarDays_).weather);
    return {true, "Seasonal weather enabled.", id};
}
Result World::trade(const std::string& player, const std::string& trader, const std::string& item, int quantity, bool buy)
{
    const auto* p = entity(player);
    const auto* m = entity(trader);
    const auto* life = society_.resident(trader);
    if (!p || p->npc || !m || !Society::merchant(trader) || p->cellId != m->cellId ||
        distance(p->position, m->position) > 2. || visionClarity(player, trader) <= 0)
        return {false, "No accessible trader is within reach.", {}};
    if (m->posture == "lying" || (life && life->task == "sleep")) return {false, "The trader is sleeping.", trader};
    auto result = society_.trade(player, trader, item, quantity, buy);
    return {result.ok, result.message, trader};
}
Result World::gather(const std::string& player)
{
    const auto* p = entity(player);
    if (!p || p->npc || p->cellId != "exterior" || distance(p->position, {17.5, 7.5}) > 1.7 ||
        !visiblePoint(*p, {17.5, 7.5})) return {false, "Approach the visible herb patch in Juniper Yard to gather.", {}};
    auto result = society_.gather(player);
    return {result.ok, result.message, {}};
}
Result World::eat(const std::string& player)
{
    auto* p = entity(player);
    if (!p || p->npc) return {false, "No controlled character.", {}};
    auto result = society_.eat(player);
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
    AirRoutes air(*c, doors_, observer->position);
    return detectScent(*observer, *source, windAt(c->id), environmentAt(c->id).scent, air).clarity;
}
std::vector<ScentCue> World::scentCues(const std::string& observerId) const
{
    const auto* observer = entity(observerId);
    if (!observer)
        return {};
    const auto* c = cell(observer->cellId);
    if (!c)
        return {};
    AirRoutes air(*c, doors_, observer->position);
    const auto wind = windAt(c->id);
    const double scentFactor = environmentAt(c->id).scent;
    std::map<int, ScentCue> sectors;
    for (const auto& entry : entities_)
    {
        if (entry.first == observerId || entry.second.cellId != observer->cellId ||
            visionClarity(observerId, entry.first) > 0)
            continue;
        const auto scent = detectScent(*observer, entry.second, wind, scentFactor, air);
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
    if (static_cast<int>(weather) >= 0 && static_cast<int>(weather) <= 3)
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
    for (int y = 0; y < c->height; ++y)
        for (int x = 0; x < c->width; ++x)
        {
            const auto index = static_cast<std::size_t>(y * c->width + x);
            if (visiblePoint(*o, {x + .5, y + .5}))
            {
                memory.glyphs[index] = c->tiles[index].glyph;
                memory.observed[index] = true;
            }
        }
    for (const auto& entry : doors_)
    {
        const auto& d = entry.second;
        if (!visiblePortal(*o, d))
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
    out.environment = environmentAt(c->id);
    out.scentCues = scentCues(observerId);
    out.visibleTiles.resize(c->tiles.size());
    out.rememberedTiles.resize(c->tiles.size());
    const auto& book = memories(observerId);
    const auto remembered = book.find(c->id);
    const CellMemory emptyMemory;
    const auto& memory = remembered == book.end() ? emptyMemory : remembered->second;
    for (int y = 0; y < c->height; ++y)
        for (int x = 0; x < c->width; ++x)
        {
            const auto i = static_cast<std::size_t>(y * c->width + x);
            const bool visible = visiblePoint(*o, {x + .5, y + .5});
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
    for (const auto& entry : doors_)
        if (!(entry.second.passage && entry.second.boundary) && entry.second.cellId == o->cellId &&
            visiblePoint(*o, entry.second.position))
        {
            Door visible = entry.second;
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
    for (const auto& entry : doors_)
        if (entry.second.cellId == o->cellId && entry.second.portal)
        {
            adjacent.insert(entry.second.targetCell);
            if (visiblePortal(*o, entry.second))
                currentlyVisible.insert(entry.second.targetCell);
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
    for (const auto& entry : entities_)
    {
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
        out.doorStates[entry.first] = entry.second.open;
    for (const auto& entry : cells_)
    {
        out.weather[entry.first] = entry.second.weather;
        out.winds[entry.first] = entry.second.wind;
        out.lighting[entry.first] = entry.second.lighting;
        out.seasonalWeather[entry.first] = entry.second.seasonalWeather;
    }
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
    Society restoredSociety(!customWorld_);
    if (state.hasSociety && !restoredSociety.restore(state.society)) return {false, "Invalid saved society or money ledger.", {}};
    if (state.hasSociety && state.society.enabled == customWorld_) return {false, "Society does not match authored world.", {}};
    if (state.hasSociety && state.society.budgetDay > std::floor(restoredDays)) return {false, "Society budget is in the future.", {}};
    for (const auto& resident : restoredSociety.state().residents)
    {
        const auto& life = resident.second;
        const auto validHome = [&](const std::string& id, double x, double y) {
            const auto* c = cell(id);
            if (!c || !std::isfinite(x) || !std::isfinite(y) || x < .5 || y < .5 ||
                x > c->width - .5 || y > c->height - .5) return false;
            const auto* t = c->tile(int(x), int(y)); return t && !t->solid;
        };
        if (!validHome(life.homeCell, life.homeX, life.homeY) ||
            (!life.relocationCell.empty() && !validHome(life.relocationCell, life.relocationX, life.relocationY)))
            return {false, "Saved resident home or relocation is invalid.", resident.first};
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
            return {false, "Unknown saved fixture.", d.first};
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
            for (const auto& entry : doors_)
                if (entry.second.cellId == e.cellId && !effectiveDoors[entry.first] && doorCovers(entry.second, p))
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
    for (const auto& e : state.npcs)
    {
        if (!e.npc || !entity(e.id) || !entity(e.id)->npc || !ids.insert(e.id).second || !validActor(e))
            return {false, "Invalid saved NPC.", e.id};
    }
    if (state.hasSociety)
        for (const auto& life : state.society.residents)
            if (!ids.count(life.first)) return {false, "Saved resident has no physical character record.", life.first};
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
        if (!cell(w.first) || static_cast<int>(w.second) < 0 || static_cast<int>(w.second) > 3)
            return {false, "Invalid saved weather.", w.first};
    for (const auto& w : state.winds)
        if (!cell(w.first) || !std::isfinite(w.second.direction) || !std::isfinite(w.second.strength) ||
            w.second.strength < 0 || w.second.strength > 1 || (!cell(w.first)->outdoors && w.second.strength != 0))
            return {false, "Invalid saved wind.", w.first};
    for (const auto& light : state.lighting)
        if (!cell(light.first) || !validLighting(light.second))
            return {false, "Invalid saved lighting.", light.first};
    for (auto it = entities_.begin(); it != entities_.end();)
        if (!it->second.npc)
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
    for (auto e : state.npcs)
    {
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
    for (const auto& w : state.winds)
        cells_[w.first].wind = {std::remainder(w.second.direction, 2.0 * Pi), w.second.strength, w.second.variable};
    for (const auto& light : state.lighting)
        cells_[light.first].lighting = light.second;
    time_ = state.time;
    clockOffsetHours_ = state.clockOffsetHours;
    calendarDays_ = restoredDays;
    society_ = std::move(restoredSociety);
    climateSlot_ = std::int64_t(std::floor(calendarDays_ * 4));
    memories_ = state.memories;
    scheduleAccumulator_ = 0;
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
    Cell candidate;
    std::string line;
    std::vector<std::string> rows;
    std::set<std::string> headers;
    std::map<std::pair<int, int>, double> heights;
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
                std::abs(candidate.worldX) > 1e6 || std::abs(candidate.worldY) > 1e6 ||
                std::abs(candidate.worldZ) > 1e6)
                return {false, "Invalid world position.", path};
        }
        else if (key == "weather")
        {
            if (value != "clear" && value != "rain" && value != "fog" && value != "snow")
                return {false, "Unknown weather value.", path};
            candidate.weather = value == "rain"   ? Weather::Rain
                                : value == "fog"  ? Weather::Fog
                                : value == "snow" ? Weather::Snow
                                                  : Weather::Clear;
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
        else if (key == "height")
        {
            int x = -1, y = -1;
            double height = 0;
            std::istringstream numbers(value);
            numbers >> x >> y >> height;
            if (!numbers || !(numbers >> std::ws).eof() || x < 0 || y < 0 || x >= 512 || y >= 512 ||
                !std::isfinite(height) || height < -16 || height > 16 ||
                std::abs(height * 4 - std::round(height * 4)) > 1e-8 ||
                !heights.emplace(std::make_pair(x, y), height).second)
                return {false, "Invalid or duplicate height override.", path};
        }
        else
            return {false, "Unknown cell header.", path};
    }
    const bool validId = !candidate.id.empty() && candidate.id.size() <= 48 && candidate.id.front() >= 'a' &&
                         candidate.id.front() <= 'z' &&
                         std::all_of(candidate.id.begin(), candidate.id.end(), [](char ch) {
                             return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
                         });
    if (!file.eof() || !validId || !headers.count("name") || !headers.count("world") || !headers.count("outdoors") ||
        !headers.count("weather") || !inGrid || rows.empty() || rows.size() > 512 || rows.front().empty() ||
        rows.front().size() > 512)
        return {false, "Invalid cell dimensions.", path};
    candidate.width = int(rows.front().size());
    candidate.height = int(rows.size());
    for (const auto& row : rows)
    {
        if (row.size() != rows.front().size())
            return {false, "Cell rows have different widths.", path};
        for (char glyph : row)
        {
            if (std::string(".#,\"T=~:^+").find(glyph) == std::string::npos)
                return {false, "Unknown terrain glyph.", path};
            candidate.tiles.push_back(fromGlyph(glyph));
        }
    }
    for (const auto& height : heights)
    {
        auto* tile = candidate.tile(height.first.first, height.first.second);
        if (!tile)
            return {false, "Height override is outside its cell.", path};
        tile->height = height.second;
    }
    if (authoredWind && !candidate.outdoors && candidate.wind.strength != 0)
        return {false, "Indoor cells cannot have outdoor wind.", path};
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
    if (candidate.outdoors && !authoredWind)
        candidate.wind = {0.0, .5, true};
    cells_[candidate.id] = std::move(candidate);
    return {true, "Cell loaded.", path};
}

} // namespace ratw
