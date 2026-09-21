#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <queue>
#include <set>
#include <sstream>

namespace ratw
{
namespace
{
constexpr double Radius = 0.065;
constexpr double Speed = 2.6;
constexpr int NavScale = 4;
constexpr double Epsilon = 1e-7;
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
double clarity(double d, double clear)
{
    if (clear <= Epsilon || d >= clear * 2.0)
        return 0.0;
    return d <= clear ? 1.0 : clamp01(2.0 - d / clear);
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
    exterior.description =
        "Rain stipples the packed earth. A broad path winds between juniper thickets and a stone-lined spring.";
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
        e.description = r.description;
        e.activity = r.activity;
        e.speakingColor = r.color;
        entities_[e.id] = e;
    }
}

Entity& World::addPlayer(const std::string& id, const std::string& name)
{
    auto existing = entities_.find(id);
    if (existing != entities_.end())
        return existing->second;
    Entity e;
    e.id = id;
    e.name = name;
    e.cellId = "tavern";
    e.position = {16.5, 12.5};
    e.description = "A wolf whose story is still being written.";
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
        for (const auto& entry : doors_)
            if (entry.second.cellId == cellId && !entry.second.open && doorCovers(entry.second, s))
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
        for (const auto& entry : doors_)
            if (entry.second.cellId == cellId && !entry.second.open && doorCovers(entry.second, p))
                return false;
    }
    return true;
}

double World::sightRange(const Entity& o) const
{
    const auto* c = cell(o.cellId);
    double weather = 1.0;
    if (c && c->outdoors)
        switch (c->weather)
        {
        case Weather::Rain:
            weather = .78;
            break;
        case Weather::Fog:
            weather = .40;
            break;
        case Weather::Snow:
            weather = .65;
            break;
        default:
            break;
        }
    return 27.0 * std::max(0.0, o.vision) * clamp01(o.eyeHealth) * weather;
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
    a->path.clear();
    a->input = length({dx, dy}) > 1.0 ? normalized({dx, dy}) : Vec2{dx, dy};
    a->transitioned = false;
    return {true, "Movement accepted.", {}};
}
Result World::stop(const std::string& id)
{
    auto* a = entity(id);
    if (!a)
        return {false, "Unknown actor.", {}};
    a->path.clear();
    a->input = {};
    a->velocity = {};
    return {true, "Stopped.", {}};
}
Result World::face(const std::string& id, double x, double y)
{
    auto* a = entity(id);
    if (!a || !finite({x, y}))
        return {false, "Invalid facing target.", {}};
    if (length(a->input) > Epsilon || !a->path.empty() || length(a->velocity) > Epsilon)
        return {false, "Stop moving to face a point.", {}};
    if (distance(a->position, {x, y}) > Epsilon)
        a->facing = std::atan2(y - a->position.y, x - a->position.x);
    return {true, "Facing changed.", {}};
}
Result World::moveTo(const std::string& id, double x, double y)
{
    auto* a = entity(id);
    if (!a || !finite({x, y}))
        return {false, "Invalid destination.", {}};
    const auto* c = cell(a->cellId);
    if (!c)
        return {false, "Missing cell.", {}};
    a->input = {};
    a->path.clear();
    a->velocity = {};
    a->transitioned = false;
    Vec2 requested{x, y};
    Vec2 goal = requested;
    const Door* exit = nullptr;
    // A click on, or just beyond, a boundary portal expresses a crossing.
    for (const auto& entry : doors_)
    {
        const auto& d = entry.second;
        if (d.cellId != a->cellId || !d.boundary)
            continue;
        const bool along = (d.position.y < 1 || d.position.y > c->height - 1) ? std::abs(x - d.position.x) < .48
                                                                              : std::abs(y - d.position.y) < .48;
        const bool edge = (d.position.y < 1 && y <= 1) || (d.position.y > c->height - 1 && y >= c->height - 1) ||
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
        if (exit->position.y < 1)
            beyond.y = -.15;
        else if (exit->position.y > c->height - 1)
            beyond.y = c->height + .15;
        else if (exit->position.x < 1)
            beyond.x = -.15;
        else
            beyond.x = c->width + .15;
        path.push_back(beyond);
    }
    a->path = std::move(path);
    return {true, "Following a route.", {}};
}

void World::transition(Entity& a, const Door& d)
{
    const auto* destination = cell(d.targetCell);
    if (!destination)
        return;
    a.cellId = d.targetCell;
    a.position = d.arrival;
    a.velocity = {};
    a.input = {};
    a.path.clear();
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
            stop(id);
            if (d.portal)
            {
                transition(*a, d);
                return {true, "You enter " + cells_.at(d.targetCell).name + ".", target};
            }
            return {true, "You open " + d.name + ". Choose a new destination to continue.", target};
        }
        if (verb == "close")
        {
            if (d.id.find("stairs_") == 0)
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
    if (other && other->cellId == a->cellId && visiblePoint(*a, other->position))
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
    if (d && d->cellId == a->cellId && visiblePoint(*a, d->position))
    {
        std::vector<std::string> out{"inspect"};
        if (distance(a->position, d->position) <= d->reach)
        {
            out.push_back("listen");
            if (d->id.find("stairs_") != 0)
                out.push_back("knock");
            if (!d->open)
                out.push_back("open");
            else
            {
                if (d->portal)
                    out.push_back("enter");
                if (d->id.find("stairs_") != 0)
                    out.push_back("close");
            }
        }
        return out;
    }
    const auto* e = entity(target);
    if (e && e->cellId == a->cellId && visiblePoint(*a, e->position))
        return {"inspect", "speak"};
    return {};
}

void World::integrate(Entity& a, double dt)
{
    a.velocity = {};
    const auto* c = cell(a.cellId);
    if (!c)
        return;
    Vec2 direction = a.input;
    while (!a.path.empty() && distance(a.position, a.path.front()) < Epsilon)
        a.path.erase(a.path.begin());
    if (!a.path.empty())
        direction = normalized({a.path.front().x - a.position.x, a.path.front().y - a.position.y});
    if (length(direction) < Epsilon)
        return;
    const auto* startTile = c->tile(int(a.position.x), int(a.position.y));
    if (!startTile)
        return;
    double speed = Speed / std::max(.1, startTile->movementCost);
    if (a.npc)
        speed *= .57;
    if (c->outdoors && (c->weather == Weather::Rain || c->weather == Weather::Snow))
        speed *= c->weather == Weather::Rain ? .85 : .70;
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
        const bool crossed = (d.position.y < 1 && proposed.y < Radius) ||
                             (d.position.y > c->height - 1 && proposed.y >= c->height - Radius) ||
                             (d.position.x < 1 && proposed.x < Radius) ||
                             (d.position.x > c->width - 1 && proposed.x >= c->width - Radius);
        const bool aligned = (d.position.y < 1 || d.position.y > c->height - 1)
                                 ? std::abs(proposed.x - d.position.x) < .43
                                 : std::abs(proposed.y - d.position.y) < .43;
        if (crossed && aligned)
        {
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
    const Vec2 initial = a.position;
    const Vec2 actual{accepted.x - a.position.x, accepted.y - a.position.y};
    if (length(actual) > Epsilon)
    {
        a.facing = std::atan2(actual.y, actual.x);
        a.velocity = {actual.x / dt, actual.y / dt};
        a.position = accepted;
    }
    if (!a.path.empty() && distance(a.position, a.path.front()) < Epsilon)
    {
        a.path.erase(a.path.begin());
        const double unused = dt - travel / speed;
        if (!a.path.empty() && unused > Epsilon)
        {
            const auto originalCell = a.cellId;
            integrate(a, unused);
            if (a.cellId == originalCell)
                a.velocity = {(a.position.x - initial.x) / dt, (a.position.y - initial.y) / dt};
        }
    }
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

void World::updateSchedules()
{
    struct Appointment
    {
        const char* cell;
        Vec2 pos;
        const char* activity;
    };
    const std::map<std::string, std::vector<Appointment>> schedules{
        {"npc_keeper",
         {{"tavern", {9.5, 6.5}, "tending the hearth"},
          {"tavern", {14.5, 13.5}, "checking on guests"},
          {"exterior", {17.5, 4.5}, "gathering fresh herbs"},
          {"tavern", {9.5, 6.5}, "preparing warm broth"}}},
        {"npc_scout",
         {{"tavern", {19.5, 12.5}, "watching the room"},
          {"exterior", {18.5, 13.5}, "checking the road"},
          {"exterior", {17.5, 20.5}, "reading tracks"},
          {"tavern", {19.5, 12.5}, "resting near the hearth"}}},
        {"npc_cook",
         {{"tavern", {26.5, 6.5}, "sorting stores"},
          {"tavern", {12.5, 6.5}, "preparing broth"},
          {"tavern", {14.5, 15.5}, "bringing out a meal"},
          {"tavern", {26.5, 6.5}, "counting provisions"}}},
        {"npc_porter",
         {{"exterior", {17.5, 7.5}, "checking the path"},
          {"exterior", {20.5, 14.5}, "stretching after a journey"},
          {"tavern", {19.5, 19.5}, "warming up indoors"},
          {"exterior", {17.5, 7.5}, "watching arrivals"}}},
        {"npc_smith",
         {{"exterior", {11.5, 14.5}, "mending a harness clasp"},
          {"exterior", {21.5, 14.5}, "visiting the spring"},
          {"tavern", {20.5, 12.5}, "sharing the afternoon meal"},
          {"exterior", {11.5, 14.5}, "sorting tools"}}},
        {"npc_scribe",
         {{"loft", {8.5, 6.5}, "studying old accounts"},
          {"loft", {10.5, 10.5}, "stretching by the steps"},
          {"tavern", {26.5, 6.5}, "asking after provisions"},
          {"loft", {8.5, 6.5}, "writing the day's record"}}}};
    for (auto& entry : entities_)
    {
        auto& e = entry.second;
        if (!e.npc || schedules.count(e.id) == 0 || e.state == "following" || !e.leaderId.empty())
            continue;
        const auto& list = schedules.at(e.id);
        const auto& target = list[static_cast<std::size_t>(time_ / 60.0) % list.size()];
        e.activity = target.activity;
        if (!e.path.empty())
            continue;
        if (e.cellId == target.cell)
        {
            if (distance(e.position, target.pos) < .3)
                continue;
            auto result = moveTo(e.id, target.pos.x, target.pos.y);
            if (!result.targetId.empty())
            {
                const auto* d = door(result.targetId);
                if (d && distance(e.position, d->position) <= d->reach)
                    interact(e.id, d->id, "open");
            }
        }
        else
        {
            for (const auto& doorEntry : doors_)
            {
                const auto& d = doorEntry.second;
                if (d.cellId != e.cellId || d.targetCell != target.cell)
                    continue;
                if (distance(e.position, d.position) <= d.reach)
                    interact(e.id, d.id, "open");
                else
                {
                    auto result = moveTo(e.id, d.position.x, d.position.y);
                    if (!result.targetId.empty())
                    {
                        const auto* barrier = door(result.targetId);
                        if (barrier && distance(e.position, barrier->position) <= barrier->reach)
                            interact(e.id, barrier->id, "open");
                    }
                }
                break;
            }
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
        scheduleAccumulator_ += step;
        if (scheduleAccumulator_ >= .5)
        {
            updateSchedules();
            scheduleAccumulator_ = 0;
        }
        for (auto& entry : entities_)
            integrate(entry.second, step);
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
    const double sensitivity = std::max(0.0, o->hearing) * clamp01(o->earHealth);
    if (sensitivity <= Epsilon)
        return 0;
    double range = (voice == Voice::Whisper ? 2.0 : voice == Voice::Yell ? 32.0 : 16.0) * sensitivity;
    const auto* oc = cell(o->cellId);
    const auto* sc = cell(s->cellId);
    if (!oc || !sc)
        return 0;
    auto weatherNoise = [](const Cell& c) {
        return c.outdoors && c.weather == Weather::Rain ? .72 : c.outdoors && c.weather == Weather::Snow ? .85 : 1.0;
    };
    range *= weatherNoise(*oc);
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
        best = std::max(best, clarity(route, range * transmission * weatherNoise(*sc)));
    }
    return best;
}
SensoryResult World::perceive(const std::string& observer, const std::string& source, Voice voice) const
{
    const double visual = visionClarity(observer, source);
    return {hearingClarity(observer, source, voice), visual, visual > 0.0};
}
void World::setWeather(const std::string& id, Weather weather)
{
    if (auto* c = cell(id))
        c->weather = weather;
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
        if (entry.first == observerId ||
            (entry.second.cellId == o->cellId && visionClarity(observerId, entry.first) > 0))
        {
            Entity visible = entry.second;
            visible.path.clear();
            visible.input = {};
            out.entities.push_back(std::move(visible));
        }
    for (const auto& entry : doors_)
        if (entry.second.cellId == o->cellId && visiblePoint(*o, entry.second.position))
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
    out.memories = memories_;
    for (const auto& entry : entities_)
    {
        Entity e = entry.second;
        e.path.clear();
        e.input = {};
        e.velocity = {};
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
        out.weather[entry.first] = entry.second.weather;
    return out;
}
Result World::restore(const PersistedWorld& state)
{
    // Beyond this bound double precision can no longer support useful frame
    // time and conversion of schedule epochs can overflow an integer.
    if (!std::isfinite(state.time) || state.time < 0 || state.time > 1e12)
        return {false, "Invalid saved clock.", {}};
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
        if (d.id.find("stairs_") == 0 && !effectiveDoors[d.id])
            return {false, "Steps cannot be closed.", d.id};
    }
    auto validActor = [&](const Entity& e) {
        const auto* c = cell(e.cellId);
        if (!c || !finite(e.position) || e.position.x < 0 || e.position.y < 0 || e.position.x >= c->width ||
            e.position.y >= c->height || !std::isfinite(e.facing) || !std::isfinite(e.hearing) ||
            !std::isfinite(e.vision) || !std::isfinite(e.earHealth) || !std::isfinite(e.eyeHealth) || e.hearing < 0 ||
            e.vision < 0 || e.earHealth < 0 || e.earHealth > 1 || e.eyeHealth < 0 || e.eyeHealth > 1 ||
            e.speakingColor < 0 || e.speakingColor > 31)
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
    for (auto it = entities_.begin(); it != entities_.end();)
        if (!it->second.npc)
            it = entities_.erase(it);
        else
            ++it;
    for (auto e : state.players)
    {
        e.input = {};
        e.path.clear();
        e.velocity = {};
        e.typing = false;
        e.speakingUntil = 0;
        e.transitioned = false;
        entities_[e.id] = std::move(e);
    }
    for (auto e : state.npcs)
    {
        e.input = {};
        e.path.clear();
        e.velocity = {};
        e.typing = false;
        e.speakingUntil = 0;
        e.transitioned = false;
        entities_[e.id] = std::move(e);
    }
    for (const auto& d : state.doorStates)
        if (doors_.count(d.first))
            doors_[d.first].open = d.second;
    for (const auto& w : state.weather)
        if (cells_.count(w.first))
            cells_[w.first].weather = w.second;
    time_ = state.time;
    memories_ = state.memories;
    scheduleAccumulator_ = 0;
    return {true, "Saved world restored.", {}};
}

Result World::loadCellFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
        return {false, "Cannot open cell file.", path};
    Cell candidate;
    std::string line;
    std::vector<std::string> rows;
    bool inGrid = false;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (inGrid)
        {
            if (!line.empty())
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
            continue;
        const std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        if (!value.empty() && value.front() == ' ')
            value.erase(value.begin());
        if (key == "id")
            candidate.id = value;
        else if (key == "name")
            candidate.name = value;
        else if (key == "description")
            candidate.description = value;
        else if (key == "outdoors")
            candidate.outdoors = value == "true";
        else if (key == "world")
        {
            std::istringstream numbers(value);
            numbers >> candidate.worldX >> candidate.worldY >> candidate.worldZ;
            if (!numbers || !std::isfinite(candidate.worldX) || !std::isfinite(candidate.worldY) ||
                !std::isfinite(candidate.worldZ))
                return {false, "Invalid world position.", path};
        }
        else if (key == "weather")
            candidate.weather = value == "rain"   ? Weather::Rain
                                : value == "fog"  ? Weather::Fog
                                : value == "snow" ? Weather::Snow
                                                  : Weather::Clear;
    }
    if (candidate.id.empty() || rows.empty() || rows.size() > 512 || rows.front().empty() || rows.front().size() > 512)
        return {false, "Invalid cell dimensions.", path};
    candidate.width = int(rows.front().size());
    candidate.height = int(rows.size());
    for (const auto& row : rows)
    {
        if (row.size() != rows.front().size())
            return {false, "Cell rows have different widths.", path};
        for (char glyph : row)
            candidate.tiles.push_back(fromGlyph(glyph));
    }
    for (const auto& entry : entities_)
        if (entry.second.cellId == candidate.id)
        {
            const auto* tile = candidate.tile(int(entry.second.position.x), int(entry.second.position.y));
            if (!tile || tile->solid)
                return {false, "Cell replacement would strand an actor.", entry.first};
        }
    cells_[candidate.id] = std::move(candidate);
    return {true, "Cell loaded.", path};
}

} // namespace ratw
