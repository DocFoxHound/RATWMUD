// Map travel (the user, 2026-11-20: "Players should be able to click to a location on the minimap and the characters
// should travel all the way there. The path there should follow roads as closely as possible."): a spot clicked on the
// minimap or the World Map, in world tiles, walked to across as many cells as it takes.
//
// Planned coarse first, over the realm in blocks of TravelBlock tiles (travelGrid: a block a road crosses costs a road's
// step, open country its ground's cost times MapTravelOffRoad, water and rock nothing), once, when the spot is chosen;
// then, in each cell as the wolf comes into it, its own leg on that cell's tiles: to the seam nearest where the coarse
// way leaves the cell, or in the last cell to the spot itself (or as near it as the ground allows), off the roads only
// where no road goes. The coarse plan is a few milliseconds over the whole realm; a leg is one search of one cell.
#include "RatwWorld.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>
#include <queue>
#include <set>

namespace ratw
{
namespace
{
constexpr double Never = std::numeric_limits<double>::infinity();
double span(Vec2 a, Vec2 b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}
bool inside(const Cell& c, Vec2 world)
{
    return world.x >= c.worldX && world.y >= c.worldY && world.x < c.worldX + c.width && world.y < c.worldY + c.height;
}
Vec2 worldOf(const Cell& c, Vec2 local)
{
    return {c.worldX + local.x, c.worldY + local.y};
}
// The coarse way's points worth drawing (Ramer-Douglas-Peucker, `tolerance` tiles), by index, the first and last kept.
void simplify(const std::vector<Vec2>& line, std::size_t from, std::size_t to, double tolerance, std::vector<std::size_t>& out)
{
    if (to <= from + 1)
        return;
    const Vec2 a = line[from], b = line[to];
    const double length = span(a, b);
    double worst = -1;
    std::size_t at = from;
    for (std::size_t i = from + 1; i < to; ++i)
    {
        const Vec2 p = line[i];
        const double d = length < 1e-9 ? span(p, a)
                                       : std::abs((b.x - a.x) * (a.y - p.y) - (a.x - p.x) * (b.y - a.y)) / length;
        if (d > worst)
            worst = d, at = i;
    }
    if (worst <= tolerance)
        return;
    simplify(line, from, at, tolerance, out);
    out.push_back(at);
    simplify(line, at, to, tolerance, out);
}
} // namespace

const World::TravelGrid& World::travelGrid(double z) const
{
    const long key = std::lround(z * 100);
    if (const auto found = travelGrids_.find(key); found != travelGrids_.end())
        return found->second;
    TravelGrid grid;
    constexpr long B = TravelBlock;
    long x0 = LONG_MAX, y0 = LONG_MAX, x1 = LONG_MIN, y1 = LONG_MIN;
    const auto blocksOf = [&](const Cell& c, long& bx, long& by, int& bw, int& bh) {
        bx = long(std::floor(c.worldX / B));
        by = long(std::floor(c.worldY / B));
        bw = (c.width + int(B) - 1) / int(B);
        bh = (c.height + int(B) - 1) / int(B);
    };
    for (const auto& [id, blocks] : travelBlocks_)
        if (const auto* c = cell(id); c && c->outdoors && std::abs(c->worldZ - z) < .01)
        {
            long bx, by;
            int bw, bh;
            blocksOf(*c, bx, by, bw, bh);
            x0 = std::min(x0, bx), y0 = std::min(y0, by), x1 = std::max(x1, bx + bw), y1 = std::max(y1, by + bh);
        }
    // (A realm of 4096 tiles a side is 512 blocks a side; anything past 4096 blocks a side is no realm to plan over.)
    if (x0 < x1 && y0 < y1 && x1 - x0 <= 4096 && y1 - y0 <= 4096)
    {
        grid.x0 = x0, grid.y0 = y0, grid.cols = int(x1 - x0), grid.rows = int(y1 - y0);
        grid.cost.assign(std::size_t(grid.cols) * std::size_t(grid.rows), 0);
        for (const auto& [id, blocks] : travelBlocks_)
            if (const auto* c = cell(id); c && c->outdoors && std::abs(c->worldZ - z) < .01)
            {
                long bx, by;
                int bw, bh;
                blocksOf(*c, bx, by, bw, bh);
                for (int y = 0; y < bh; ++y)
                    for (int x = 0; x < bw; ++x)
                        if (const auto v = blocks[std::size_t(y * bw + x)])
                            grid.cost[std::size_t((by - y0 + y) * grid.cols + (bx - x0 + x))] = v;
            }
    }
    return travelGrids_.emplace(key, std::move(grid)).first->second;
}

std::vector<Vec2> World::coarseRoute(double z, Vec2 from, Vec2 to, std::string& problem) const
{
    const auto& grid = travelGrid(z);
    constexpr double B = TravelBlock;
    if (grid.cost.empty())
    {
        problem = "There is no map of the land here to travel by.";
        return {};
    }
    const int cols = grid.cols, rows = grid.rows;
    const auto blockOf = [&](Vec2 p, int& x, int& y) {
        x = int(long(std::floor(p.x / B)) - grid.x0);
        y = int(long(std::floor(p.y / B)) - grid.y0);
        return x >= 0 && y >= 0 && x < cols && y < rows;
    };
    int sx, sy, gx, gy;
    if (!blockOf(from, sx, sy) || !blockOf(to, gx, gy))
    {
        problem = "There is no land there to travel to.";
        return {};
    }
    const auto at = [&](int x, int y) { return std::size_t(y * cols + x); };
    // The spot's own block may be water or rock (a click on the bay): the nearest block with ground to stand on, a few
    // blocks off at most.
    if (!grid.cost[at(gx, gy)])
    {
        int best = -1;
        double nearest = Never;
        for (int dy = -4; dy <= 4; ++dy)
            for (int dx = -4; dx <= 4; ++dx)
            {
                const int x = gx + dx, y = gy + dy;
                if (x >= 0 && y >= 0 && x < cols && y < rows && grid.cost[at(x, y)] && std::hypot(dx, dy) < nearest)
                    nearest = std::hypot(dx, dy), best = int(at(x, y));
            }
        if (best < 0)
        {
            problem = "There is no ground to stand on there: open water or bare rock.";
            return {};
        }
        gx = best % cols, gy = best / cols;
    }
    // A* over the blocks: a step costs its length in tiles times the block's cost; the estimate is the straight way at a
    // road's cost, a little greedy (a fifth), so a long way is planned in a few milliseconds, not the whole realm.
    constexpr double Greed = 1.2;
    constexpr std::size_t MostExpanded = 600000;
    const std::size_t nodes = std::size_t(cols) * std::size_t(rows);
    std::vector<float> g(nodes, std::numeric_limits<float>::infinity());
    std::vector<int> previous(nodes, -1);
    std::vector<std::uint8_t> closed(nodes, 0);
    const auto cost = [&](int x, int y) -> double {
        if ((x == sx && y == sy) || (x == gx && y == gy))
            return std::max<double>(grid.cost[at(x, y)], 30) / 10.0;   // (Where it sets out and ends always count.)
        return grid.cost[at(x, y)] / 10.0;
    };
    const auto estimate = [&](int x, int y) { return Greed * B * std::hypot(x - gx, y - gy); };
    using Entry = std::pair<double, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
    const int start = int(at(sx, sy)), goal = int(at(gx, gy));
    g[std::size_t(start)] = 0;
    open.push({estimate(sx, sy), start});
    std::size_t expanded = 0;
    bool found = false;
    while (!open.empty() && expanded < MostExpanded)
    {
        const int current = open.top().second;
        open.pop();
        if (closed[std::size_t(current)])
            continue;
        if (current == goal)
        {
            found = true;
            break;
        }
        closed[std::size_t(current)] = 1;
        ++expanded;
        const int x = current % cols, y = current / cols;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int nx = x + dx, ny = y + dy;
                if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= cols || ny >= rows)
                    continue;
                const double step = cost(nx, ny);
                if (step <= 0 || closed[at(nx, ny)])
                    continue;
                if (dx && dy && cost(x, ny) <= 0 && cost(nx, y) <= 0)
                    continue;                       // (Never between two blocks nothing can cross.)
                const double through = g[std::size_t(current)] + (dx && dy ? std::sqrt(2.0) : 1.0) * B * step;
                const int next = int(at(nx, ny));
                if (through < g[std::size_t(next)])
                {
                    g[std::size_t(next)] = float(through);
                    previous[std::size_t(next)] = current;
                    open.push({through + estimate(nx, ny), next});
                }
            }
    }
    if (!found)
    {
        problem = "No way across the land leads there from here.";
        return {};
    }
    std::vector<Vec2> way;
    for (int i = goal; i >= 0; i = previous[std::size_t(i)])
        way.push_back({(grid.x0 + i % cols + .5) * B, (grid.y0 + i / cols + .5) * B});
    std::reverse(way.begin(), way.end());
    way.front() = from;
    way.back() = to;
    if (way.size() == 1)
        way.push_back(to);
    return way;
}

Result World::travelToPoint(const std::string& id, double worldX, double worldY)
{
    auto* actor = entity(id);
    if (!actor || actor->npc)
        return {false, "Travel is unavailable.", {}};
    if (actor->dead)
        return {false, "You are dead.", {}};
    if (actor->downedLeft > 0)
        return {false, "You are down: you can't set off anywhere.", {}};
    const Vec2 goal{worldX, worldY};
    if (!std::isfinite(goal.x) || !std::isfinite(goal.y))
        return {false, "Choose a spot on the map.", {}};
    const auto* here = cell(actor->cellId);
    if (!here || !here->outdoors)
        return {false, "Map travel sets out from open ground: step outside first.", {}};
    const Cell* target = nullptr;
    for (const auto& [cellId, c] : cells_)
        if (c.outdoors && std::abs(c.worldZ - here->worldZ) < .01 && inside(c, goal))
        {
            target = &c;
            break;
        }
    if (!target)
        return {false, "There is no land there to travel to.", {}};
    const Vec2 from = worldOf(*here, actor->position);
    if (span(from, goal) < 1)
        return {false, "You are already there.", {}};
    std::string problem;
    auto coarse = coarseRoute(here->worldZ, from, goal, problem);
    if (coarse.empty())
        return {false, problem, {}};
    // A refused spot leaves a journey under way, and the wolf's own walking, as they were.
    cancelTravel(id);
    actor->path.clear();
    actor->input = {};
    actor->velocity = {};
    actor->turning = false;
    pendingPortals_.erase(id);
    auto& state = travels_[id];
    state = {};
    state.toPoint = true;
    state.goal = goal;
    state.destination = target->id;
    state.active = true;
    state.status = "Travelling to " + target->name + ", by the roads where they go.";
    auto& travel = mapTravels_[id];
    travel = {};
    travel.targetCell = target->id;
    travel.target = {goal.x - target->worldX, goal.y - target->worldY};
    travel.coarse = std::move(coarse);
    travel.shown = {0};
    simplify(travel.coarse, 0, travel.coarse.size() - 1, 2.5, travel.shown);
    travel.shown.push_back(travel.coarse.size() - 1);
    // The cells the way goes through, in order (the travel bar's route).
    const Cell* last = here;
    for (const Vec2 p : travel.coarse)
    {
        if (!inside(*last, p))
            for (const auto& [cellId, c] : cells_)
                if (c.outdoors && std::abs(c.worldZ - here->worldZ) < .01 && inside(c, p))
                {
                    last = &c;
                    break;
                }
        if (inside(*last, p) && (state.route.empty() || state.route.back() != last->id))
            state.route.push_back(last->id);
    }
    travelLegCells_[id] = actor->cellId;
    travelProgress_[id] = {actor->position, time_, 0, 0};
    if (!planMapLeg(*actor, state, travel, problem))
    {
        cancelTravel(id);
        state = {};
        return {false, problem, {}};
    }
    showMapLine(*actor, state, travel);
    return {true, state.status, {}};
}

void World::endMapTravel(Entity& actor, TravelState& state, const std::string& message)
{
    actor.path.clear();
    actor.input = {};
    actor.velocity = {};
    pendingPortals_.erase(actor.id);
    state.active = state.paused = false;
    state.nextDoor.clear();
    state.line.clear();
    state.route = {actor.cellId};
    state.status = message;
    mapTravels_.erase(actor.id);
    travelLegCells_.erase(actor.id);
    travelRetryAt_.erase(actor.id);
    travelProgress_.erase(actor.id);
    notice(actor.id, message);
}

void World::updateMapTravel(Entity& actor, TravelState& state)
{
    const auto found = mapTravels_.find(actor.id);
    if (found == mapTravels_.end())
    {
        endMapTravel(actor, state, "Travel stopped.");
        return;
    }
    auto& travel = found->second;
    if (actor.dead || actor.downedLeft > 0)
    {
        endMapTravel(actor, state, actor.dead ? "Travel stopped." : "Travel stopped: you are down.");
        return;
    }
    auto& progress = travelProgress_[actor.id];
    auto& legCell = travelLegCells_[actor.id];
    if (legCell != actor.cellId)
    {
        // Over a seam (or through a door): the next leg is planned here, after the crossing's moment.
        travel.lastCell = legCell;
        legCell = actor.cellId;
        state.paused = false;
        state.nextDoor.clear();
        travel.doorAhead.clear();
        travel.replans = 0;
        travel.finalLeg = travel.nearOnly = false;
        travel.pathShown = std::size_t(-1);
        progress = {actor.position, time_, 0, progress.transitions + 1};
        travelRetryAt_[actor.id] = time_ + .25;
        const auto* now = cell(actor.cellId);
        if (!now || !now->outdoors)
        {
            endMapTravel(actor, state, "Travel stopped: you went indoors.");
            return;
        }
        // However long the way, it crosses each of its cells once or twice: far more often, it is going round in circles.
        if (progress.transitions > int(state.route.size()) * 3 + 8)
        {
            endMapTravel(actor, state, "Travel stopped: the way there keeps turning back on itself. Choose the spot again.");
            return;
        }
        return;
    }
    if (state.paused)
    {
        const auto* gate = door(state.nextDoor);
        if (gate && !gate->open && !gate->locked)
            return;                                 // Waiting for the player to open it.
        state.paused = false;
        state.nextDoor.clear();
        travel.doorAhead.clear();
        state.status = "Travelling on.";
        actor.path.clear();
    }
    if (time_ < travelRetryAt_[actor.id] || actor.postureRemaining > 0 || pendingPortals_.count(actor.id))
        return;
    if (!actor.path.empty())
    {
        if (time_ - progress.checkedAt >= 1.0)
        {
            progress.stagnant = span(progress.position, actor.position) < .015 ? progress.stagnant + 1 : 0;
            progress.position = actor.position;
            progress.checkedAt = time_;
        }
        if (progress.stagnant < 3)
        {
            showMapLine(actor, state, travel);
            return;
        }
        // Held up three seconds (someone in the way, a cart, a gate shut since): the leg is found again from here.
        progress.stagnant = 0;
        actor.path.clear();
        if (++travel.replans > 3)
        {
            endMapTravel(actor, state, "Travel stopped: something blocks the way. Choose the spot again when it is clear.");
            return;
        }
    }
    else if (travel.finalLeg && actor.cellId == travel.targetCell)
    {
        endMapTravel(actor, state, travel.nearOnly ? "Arrived, as near that spot as the ground allows." : "Arrived.");
        return;
    }
    else if (!travel.doorAhead.empty())
    {
        if (const auto* gate = door(travel.doorAhead); gate && !gate->open && !gate->locked)
        {
            actor.input = {};
            actor.velocity = {};
            state.paused = true;
            state.nextDoor = gate->id;
            state.status = "Travel paused at the closed " + gate->name + ". Choose Open to continue.";
            return;
        }
        travel.doorAhead.clear();
    }
    else if (travel.aim > 0 && ++travel.replans > 3)
    {
        // A leg walked to its end without crossing where it led: the way on isn't there.
        endMapTravel(actor, state, "Travel stopped: the way on from here is blocked.");
        return;
    }
    std::string problem;
    if (!planMapLeg(actor, state, travel, problem))
    {
        endMapTravel(actor, state, problem);
        return;
    }
    progress.position = actor.position;
    progress.checkedAt = time_;
    progress.stagnant = 0;
    travelRetryAt_[actor.id] = time_ + .1;
    showMapLine(actor, state, travel);
}

bool World::planMapLeg(Entity& actor, TravelState& state, MapTravel& travel, std::string& problem)
{
    const auto* c = cell(actor.cellId);
    if (!c || !c->outdoors || c->tiles.empty())
    {
        problem = "Travel stopped: no way on from here.";
        return false;
    }
    const int w = c->width, h = c->height;
    const int sx = int(std::floor(actor.position.x)), sy = int(std::floor(actor.position.y));
    if (sx < 0 || sy < 0 || sx >= w || sy >= h)
    {
        problem = "Travel stopped: no way on from here.";
        return false;
    }
    const Vec2 here = worldOf(*c, actor.position);
    // Where along the coarse way the wolf is: its nearest point, from the last one on. Far off it (pushed aside, round
    // something the blocks didn't show), the way is planned again from here, a few times at most.
    const auto nearestFrom = [&](std::size_t from) {
        std::size_t best = from;
        double least = Never;
        for (std::size_t i = from; i < travel.coarse.size() && i < from + 600; ++i)
            if (const double d = span(travel.coarse[i], here); d < least)
                least = d, best = i;
        return std::pair{best, least};
    };
    auto [nearest, off] = nearestFrom(travel.progress);
    if (off > 6 * TravelBlock && travel.coarsePlans < 6)
    {
        const auto& goalCell = *cell(travel.targetCell);
        auto again = coarseRoute(c->worldZ, here, worldOf(goalCell, travel.target), problem);
        if (again.empty())
        {
            problem = "Travel stopped: " + problem;
            return false;
        }
        ++travel.coarsePlans;
        travel.coarse = std::move(again);
        travel.shown = {0};
        simplify(travel.coarse, 0, travel.coarse.size() - 1, 2.5, travel.shown);
        travel.shown.push_back(travel.coarse.size() - 1);
        nearest = 0;
    }
    travel.progress = nearest;
    // Where the coarse way leaves this cell (none: it ends here): well past its edge, not where it runs along the edge
    // (a road along a cell's border, in and out of the next cell by a few tiles, would have the wolf over the seam and
    // back at every bend).
    constexpr double Past = 1.5 * TravelBlock;
    const auto outside = [&](Vec2 p) {
        const double dx = std::max({c->worldX - p.x, p.x - (c->worldX + c->width), 0.0});
        const double dy = std::max({c->worldY - p.y, p.y - (c->worldY + c->height), 0.0});
        return std::hypot(dx, dy) > Past || (!inside(*c, p) && span(p, travel.coarse.back()) < 1e-9);
    };
    std::size_t exit = 0;
    for (std::size_t i = travel.progress; i < travel.coarse.size(); ++i)
        if (outside(travel.coarse[i]))
        {
            exit = i;
            break;
        }
    const bool inTarget = actor.cellId == travel.targetCell;

    // One search of this cell's tiles (A* on whole tiles, eight ways), off the roads MapTravelOffRoad times dearer, to
    // the cheapest of its ends: each counted with what it leaves to do (a seam, by how far it comes out from where the
    // coarse way crosses; the spot, by how far short of it).
    std::vector<char> shut(std::size_t(w) * std::size_t(h), 0);  // 1: a doorway to elsewhere; 2: a closed gate.
    std::vector<const Door*> gates(shut.size(), nullptr);
    for (const Door* d : portalsOf(actor.cellId))
        if (!d->boundary && d->targetCell != actor.cellId)
        {
            const int dx = int(std::floor(d->position.x)), dy = int(std::floor(d->position.y));
            if (dx >= 0 && dy >= 0 && dx < w && dy < h && !(dx == sx && dy == sy))
                shut[std::size_t(dy * w + dx)] = 1;
        }
    for (const Door* d : doorsIn(actor.cellId))
        if (!d->portal && !d->boundary && !d->open)
        {
            const int dx = int(std::floor(d->position.x)), dy = int(std::floor(d->position.y));
            if (dx >= 0 && dy >= 0 && dx < w && dy < h && !shut[std::size_t(dy * w + dx)])
            {
                shut[std::size_t(dy * w + dx)] = d->locked ? 1 : 2;
                gates[std::size_t(dy * w + dx)] = d;
            }
        }
    // Whether a wolf may stand on a tile's middle, come from ground of a given height: asked of the walking rules once a
    // tile and height (a search asks eight times a tile, from neighbours mostly of one height), not at every step.
    std::vector<float> askedFrom(shut.size(), std::numeric_limits<float>::quiet_NaN());
    std::vector<char> answer(shut.size(), 0);
    const auto standsOn = [&](int x, int y, const Tile* from) {
        const std::size_t i = std::size_t(y * w + x);
        const float key = from ? float(from->height) : -1e9f;
        if (askedFrom[i] != key)
        {
            askedFrom[i] = key;
            answer[i] = passable(actor.cellId, {x + .5, y + .5}, from);
        }
        return answer[i] != 0;
    };
    struct Exit
    {
        const Door* seam = nullptr;
        double cost = Never;
    };
    std::vector<Exit> seams;
    Vec2 aim = inTarget && !exit ? worldOf(*c, travel.target) : travel.coarse[exit ? exit : travel.coarse.size() - 1];
    std::map<const Cell*, std::pair<const std::vector<int>*, int>> regionsOf;
    const auto planSeams = [&] {
        seams.assign(shut.size(), {});
        bool any = false;
        for (const Door* d : doorsIn(actor.cellId))
        {
            if (!d->boundary || !d->passage || !d->open)
                continue;
            const auto* next = cell(d->targetCell);
            if (!next || !next->outdoors || std::abs(next->worldZ - c->worldZ) > .01)
                continue;
            const int dx = int(std::floor(d->position.x)), dy = int(std::floor(d->position.y));
            if (dx < 0 || dy < 0 || dx >= w || dy >= h)
                continue;
            double cost = MapTravelOffRoad * span(worldOf(*c, d->position), aim);
            if (d->targetCell == travel.lastCell)
                cost += 80;                         // (Not straight back where it came from, unless that is the way.)
            // (A strip of the next cell cut off from the rest of it.) Its region map and body looked up once a cell: outside
            // a tick a region map is checked against every tile each time it is asked for.
            auto [known, fresh] = regionsOf.try_emplace(next, nullptr, -1);
            if (fresh && (known->second.first = regionMap(*next)))
                known->second.second = mainRegion(*next);
            if (const auto* regions = known->second.first)
            {
                const int ax = int(std::floor(d->arrival.x)), ay = int(std::floor(d->arrival.y));
                const int lands = ax >= 0 && ay >= 0 && ax < next->width && ay < next->height
                                      ? (*regions)[std::size_t(ay * next->width + ax)] : -1;
                if (lands >= 0 && lands != known->second.second)
                    cost += 100;
            }
            auto& e = seams[std::size_t(dy * w + dx)];
            if (cost < e.cost)
                e = {d, cost};
            any = true;
        }
        return any;
    };
    struct Found
    {
        int end = -1;
        double total = Never;
        std::vector<int> previous;
    };
    constexpr double Near = 16;                     // How far short of the spot the last leg may end, at most.
    const Vec2 spot = travel.target;
    const auto search = [&](bool final) {
        Found f;
        const std::size_t nodes = shut.size();
        std::vector<double> g(nodes, Never);
        std::vector<char> done(nodes, 0);
        f.previous.assign(nodes, -1);
        const Vec2 aimLocal{aim.x - c->worldX, aim.y - c->worldY};
        const Vec2 toward = final ? spot : aimLocal;
        const auto estimate = [&](int x, int y) { return span({x + .5, y + .5}, toward); };
        const auto ending = [&](int i) -> double {
            const int x = i % w, y = i / w;
            if (final)
            {
                if (x == int(std::floor(spot.x)) && y == int(std::floor(spot.y)))
                    return 0;
                const double d = span({x + .5, y + .5}, spot);
                return d <= Near && shut[std::size_t(i)] == 0 ? 4 * d : Never;
            }
            return seams[std::size_t(i)].cost;
        };
        using Entry = std::pair<double, int>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
        const int start = sy * w + sx;
        g[std::size_t(start)] = 0;
        open.push({estimate(sx, sy), start});
        while (!open.empty())
        {
            const auto [score, current] = open.top();
            open.pop();
            if (done[std::size_t(current)])
                continue;
            if (score >= f.total)
                break;                              // (Nothing left can end cheaper than the best end found.)
            done[std::size_t(current)] = 1;
            if (const double end = ending(current); g[std::size_t(current)] + end < f.total)
                f.total = g[std::size_t(current)] + end, f.end = current;
            if (shut[std::size_t(current)] == 2)
                continue;                           // (A closed gate is walked to, and no further, till it is opened.)
            const int x = current % w, y = current / w;
            const auto* tile = c->tile(x, y);
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const int nx = x + dx, ny = y + dy;
                    if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= w || ny >= h)
                        continue;
                    const int next = ny * w + nx;
                    if (done[std::size_t(next)] || shut[std::size_t(next)] == 1)
                        continue;
                    const auto* nt = c->tile(nx, ny);
                    const bool gate = shut[std::size_t(next)] == 2;
                    if (!nt || (gate ? nt->solid : !standsOn(nx, ny, tile)))
                        continue;
                    if (dx && dy &&
                        (gate || shut[std::size_t(y * w + nx)] || shut[std::size_t(ny * w + x)] || !standsOn(x, ny, tile) ||
                         !standsOn(nx, y, tile)))
                        continue;
                    const double weight = roadTerrain(nt->glyph) ? nt->movementCost : nt->movementCost * MapTravelOffRoad;
                    const double through = g[std::size_t(current)] + std::hypot(dx, dy) * std::max(1.0, weight) +
                                           std::abs(nt->height - tile->height) * .3 + (gate ? 6 : 0);
                    if (through < g[std::size_t(next)])
                    {
                        g[std::size_t(next)] = through;
                        f.previous[std::size_t(next)] = current;
                        open.push({through + estimate(nx, ny), next});
                    }
                }
        }
        return f;
    };
    Found leg;
    bool final = false;
    travel.nearOnly = false;
    if (inTarget)
    {
        leg = search(true);
        const bool there = leg.end >= 0 && leg.end == int(std::floor(spot.y)) * w + int(std::floor(spot.x));
        if (there || !exit)
        {
            final = leg.end >= 0;
            travel.nearOnly = final && !there;
        }
    }
    if (!final)
    {
        if (!planSeams())
        {
            problem = "Travel stopped: there is no way across from this ground.";
            return false;
        }
        leg = search(false);
        if (leg.end < 0)
        {
            // In the target cell, cut off from the spot, and no way round by the next cell either.
            problem = inTarget ? "There is no way to that spot from here: walls, water or rock stand between."
                               : "Travel stopped: walls, water or rock stand between here and the way on.";
            return false;
        }
    }
    // The tiles of the leg, then straight across where nothing is in the way and the straight way costs no more than the
    // tiles it skips (by the same weights): a wolf walks a road's bends, not every tile of it, and never cuts a bend over
    // the grass.
    std::vector<int> tiles;
    for (int i = leg.end; i >= 0; i = leg.previous[std::size_t(i)])
        tiles.push_back(i);
    std::reverse(tiles.begin(), tiles.end());
    travel.doorAhead.clear();
    for (std::size_t k = 1; k < tiles.size(); ++k)
        if (shut[std::size_t(tiles[k])] == 2)
        {
            travel.doorAhead = gates[std::size_t(tiles[k])]->id;
            tiles.resize(k);                        // To the tile before the gate.
            final = false;
            break;
        }
    const auto weightAt = [&](const Tile& t) {
        return std::max(1.0, roadTerrain(t.glyph) ? t.movementCost : t.movementCost * MapTravelOffRoad);
    };
    const auto centre = [&](int i) { return Vec2{i % w + .5, double(i / w) + .5}; };
    std::vector<double> costTo(tiles.size(), 0);
    for (std::size_t k = 1; k < tiles.size(); ++k)
        costTo[k] = costTo[k - 1] + span(centre(tiles[k - 1]), centre(tiles[k])) * weightAt(*c->tile(tiles[k] % w, tiles[k] / w));
    // The straight way's cost from `from` to `to`, or infinity where something is in the way.
    const auto straight = [&](Vec2 from, Vec2 to, double most) {
        const int steps = std::max(1, int(std::ceil(span(from, to) / .25)));
        const double each = span(from, to) / steps;
        double cost = 0;
        for (int s = 1; s <= steps; ++s)
        {
            const Vec2 p{from.x + (to.x - from.x) * s / steps, from.y + (to.y - from.y) * s / steps};
            const int px = int(std::floor(p.x)), py = int(std::floor(p.y));
            const auto* t = c->tile(px, py);
            if (!t || t->solid || shut[std::size_t(py * w + px)])
                return Never;
            cost += each * weightAt(*t);
        }
        if (cost > most)
            return cost;                            // (Dearer: no need to look closer.)
        Vec2 at = from;
        for (int s = 1; s <= steps; ++s)
        {
            const Vec2 p{from.x + (to.x - from.x) * s / steps, from.y + (to.y - from.y) * s / steps};
            if (!passable(actor.cellId, p, c->tile(int(std::floor(at.x)), int(std::floor(at.y)))))
                return Never;
            at = p;
        }
        return cost;
    };
    std::vector<Vec2> path;
    {
        std::size_t anchor = 0;                     // Index into tiles of the last point kept (0: where it stands).
        Vec2 from = actor.position;
        while (anchor + 1 < tiles.size())
        {
            std::size_t best = anchor + 1;
            for (std::size_t k = anchor + 2; k < tiles.size() && k <= anchor + 16; ++k)
            {
                const double most = costTo[k] - costTo[anchor] + .3;
                const double cost = straight(from, centre(tiles[k]), most);
                if (cost == Never)
                    break;
                if (cost <= most)
                    best = k;
            }
            path.push_back(centre(tiles[best]));
            from = path.back();
            anchor = best;
        }
    }
    const Door* seam = final || !travel.doorAhead.empty() ? nullptr : seams[std::size_t(tiles.back())].seam;
    if (final && !travel.nearOnly)
    {
        if (path.empty() || span(path.back(), spot) < 1.5)
        {
            if (!path.empty())
                path.pop_back();
            path.push_back(spot);
        }
    }
    else if (seam)
    {
        if (!path.empty() && span(path.back(), seam->position) < .05)
            path.pop_back();
        path.push_back(seam->position);
        // Past the edge it crosses (as a click there would: moveTo), so the walk goes over it.
        Vec2 beyond = seam->position;
        if (seam->edge == 'N' || (seam->edge == '-' && seam->position.y < 1))
            beyond.y = -.15;
        else if (seam->edge == 'S' || (seam->edge == '-' && seam->position.y > h - 1))
            beyond.y = h + .15;
        else if (seam->edge == 'W' || (seam->edge == '-' && seam->position.x < 1))
            beyond.x = -.15;
        else
            beyond.x = w + .15;
        path.push_back(beyond);
    }
    travel.finalLeg = final;
    travel.aim = exit ? exit : travel.coarse.size() - 1;
    travel.pathShown = std::size_t(-1);
    actor.path = std::move(path);
    actor.input = {};
    actor.velocity = {};
    actor.turning = false;
    actor.transitioned = false;
    if (!actor.path.empty())
        prepareMovement(actor);
    if (!state.paused)
        state.status = "Travelling to " + cell(travel.targetCell)->name +
                       (roadTerrain(c->tile(sx, sy)->glyph) ? ", along the road." : ", by the roads where they go.");
    return true;
}

void World::showMapLine(const Entity& actor, TravelState& state, MapTravel& travel)
{
    // What the maps draw (the snapshot's travel line): the leg in hand as it will be walked, then the coarse way on
    // from where the leg ends, simplified, in world tiles. Made again only as the leg's turns are passed.
    if (travel.pathShown == actor.path.size())
        return;
    travel.pathShown = actor.path.size();
    const auto* c = cell(actor.cellId);
    if (!c)
        return;
    state.line.clear();
    state.line.push_back(worldOf(*c, actor.position));
    const std::size_t every = std::max<std::size_t>(1, actor.path.size() / 96);
    for (std::size_t i = 0; i < actor.path.size(); ++i)
        if (i % every == 0 || i + 1 == actor.path.size())
            state.line.push_back(worldOf(*c, actor.path[i]));
    if (!travel.finalLeg)
        for (const std::size_t i : travel.shown)
            if (i > travel.aim && state.line.size() < 240)
                state.line.push_back(travel.coarse[i]);
    if (state.line.empty() || span(state.line.back(), state.goal) > .5)
        state.line.push_back(state.goal);
}
} // namespace ratw
