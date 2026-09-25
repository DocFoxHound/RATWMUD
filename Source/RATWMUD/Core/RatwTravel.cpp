#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <set>

namespace ratw
{
namespace
{
constexpr double Infinity = std::numeric_limits<double>::infinity();
double distance(Vec2 a, Vec2 b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}
struct TravelGuard
{
    bool& flag;
    bool prior;
    explicit TravelGuard(bool& value) : flag(value), prior(value)
    {
        flag = true;
    }
    ~TravelGuard()
    {
        flag = prior;
    }
};
using Graph = std::map<std::string, std::map<std::string, double>>;
bool observed(const CellMemory& memory, Vec2 point)
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || point.x < 0 || point.y < 0 || point.x >= memory.width ||
        point.y >= memory.height)
        return false;
    const auto index = static_cast<std::size_t>(int(point.y) * memory.width + int(point.x));
    return index < memory.observed.size() && memory.observed[index];
}
bool knownConnection(const Door& door, const std::map<std::string, CellMemory>& book,
                     const std::map<std::string, Door>& doors)
{
    const auto from = book.find(door.cellId), to = book.find(door.targetCell);
    if (!door.portal || from == book.end() || to == book.end() || from->second.knowledge != Knowledge::Visited ||
        to->second.knowledge != Knowledge::Visited || !observed(from->second, door.position))
        return false;
    // A visited destination alone does not establish knowledge of an unseen
    // shortcut. Both ends of the connection must have actually been observed.
    const auto pair = doors.find(door.linkedDoor);
    return pair != doors.end() && pair->second.targetCell == door.cellId && pair->second.cellId == door.targetCell &&
           observed(to->second, pair->second.position);
}
Graph knownGraph(const std::map<std::string, CellMemory>& book, const std::map<std::string, Door>& doors)
{
    Graph graph;
    for (const auto& entry : book)
        if (entry.second.knowledge == Knowledge::Visited)
            graph[entry.first];
    for (const auto& entry : doors)
    {
        const auto& door = entry.second;
        if (!knownConnection(door, book, doors))
            continue;
        const auto& from = book.at(door.cellId);
        const auto& to = book.at(door.targetCell);
        // Estimated walking effort from remembered dimensions only. Remote
        // weather, doors, creatures and changed terrain never affect this graph.
        const double cost = 1.0 + (from.width + from.height + to.width + to.height) / 8.0;
        graph[door.cellId][door.targetCell] = cost;
    }
    return graph;
}
struct Routes
{
    std::map<std::string, double> costs;
    std::map<std::string, std::string> next;
};
Routes routesTo(const Graph& graph, const std::string& destination, const std::string& excluded = {})
{
    Graph reverse;
    for (const auto& from : graph)
        if (from.first != excluded)
            for (const auto& to : from.second)
                if (to.first != excluded)
                    reverse[to.first][from.first] = to.second;
    Routes routes;
    using Entry = std::pair<double, std::string>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    routes.costs[destination] = 0;
    queue.push({0, destination});
    while (!queue.empty())
    {
        const auto current = queue.top();
        queue.pop();
        if (current.first > routes.costs[current.second])
            continue;
        for (const auto& edge : reverse[current.second])
        {
            const double cost = current.first + edge.second;
            if (!routes.costs.count(edge.first) || cost < routes.costs[edge.first] - 1e-8)
            {
                routes.costs[edge.first] = cost;
                routes.next[edge.first] = current.second;
                queue.push({cost, edge.first});
            }
        }
    }
    return routes;
}
std::vector<std::string> routeFrom(const Routes& routes, std::string current, const std::string& destination)
{
    std::vector<std::string> route{current};
    for (std::size_t count = 0; current != destination && count <= routes.next.size(); ++count)
    {
        const auto next = routes.next.find(current);
        if (next == routes.next.end())
            return {};
        current = next->second;
        route.push_back(current);
    }
    return current == destination ? route : std::vector<std::string>{};
}
} // namespace

TravelState World::travelState(const std::string& id) const
{
    const auto it = travels_.find(id);
    return it == travels_.end() ? TravelState{} : it->second;
}

std::vector<MapCell> World::travelMap(const std::string& id) const
{
    const auto* actor = entity(id);
    std::vector<MapCell> out;
    if (!actor || actor->npc)
        return out;
    for (const auto& entry : memories(id))
    {
        const auto& memory = entry.second;
        if (memory.knowledge != Knowledge::Visited)
            continue;
        MapCell cell;
        cell.id = entry.first;
        cell.name = memory.name;
        cell.width = memory.width;
        cell.height = memory.height;
        cell.worldX = memory.worldX;
        cell.worldY = memory.worldY;
        cell.worldZ = memory.worldZ;
        cell.knowledge = Knowledge::Visited;
        cell.current = actor->cellId == entry.first;
        cell.visible = cell.current;
        // This is a remembered travel index, not another live world snapshot.
        // No remote glyphs, entities, door state or weather are transmitted.
        out.push_back(std::move(cell));
    }
    return out;
}

Result World::cancelTravel(const std::string& id)
{
    auto* actor = entity(id);
    if (!actor)
        return {false, "Unknown actor.", {}};
    const auto it = travels_.find(id);
    const bool active = it != travels_.end() && it->second.active;
    if (active)
    {
        actor->path.clear();
        actor->input = {};
        actor->velocity = {};
        pendingPortals_.erase(id);
        it->second.active = false;
        it->second.paused = false;
        it->second.nextDoor.clear();
        it->second.route.clear();
        it->second.status = "Travel cancelled.";
    }
    travelLegCells_.erase(id);
    travelRetryAt_.erase(id);
    travelProgress_.erase(id);
    return {true, active ? "Travel cancelled." : "No active travel route.", {}};
}

Result World::travelTo(const std::string& id, const std::string& destination)
{
    auto* actor = entity(id);
    if (!actor || actor->npc)
        return {false, "Travel is unavailable.", {}};
    const auto& book = memories(id);
    const auto target = book.find(destination);
    // Identical rejection for nonexistent, unvisited and merely glimpsed IDs.
    if (destination.empty() || destination.size() > 48 || target == book.end() ||
        target->second.knowledge != Knowledge::Visited)
        return {false, "Choose a place you have visited before.", {}};
    const auto graph = knownGraph(book, doors_);
    const auto routes = routesTo(graph, destination);
    const auto route = routeFrom(routes, actor->cellId, destination);
    if (route.empty())
        return {false, "No remembered route connects you to that place.", {}};
    // A rejected request leaves an existing journey and local motion untouched.
    cancelTravel(id);
    actor->path.clear();
    actor->input = {};
    actor->velocity = {};
    actor->turning = false;
    pendingPortals_.erase(id);
    auto& state = travels_[id];
    state = {};
    state.destination = destination;
    state.route = route;
    state.active = actor->cellId != destination;
    state.status = state.active ? "Following a remembered route." : "Already at the destination.";
    if (state.active)
    {
        travelLegCells_[id] = actor->cellId;
        travelProgress_[id] = {actor->position, time_, 0, 0};
        updateTravel(*actor);
    }
    return {true, state.status, {}};
}

void World::updateTravel(Entity& actor)
{
    auto it = travels_.find(actor.id);
    if (it == travels_.end() || !it->second.active || actor.npc)
        return;
    auto& state = it->second;
    auto& progress = travelProgress_[actor.id];
    auto pause = [&](const std::string& message, const std::string& blocker = std::string{}) {
        actor.path.clear();
        actor.input = {};
        actor.velocity = {};
        state.paused = true;
        state.status = message;
        state.nextDoor = blocker;
    };
    if (travelLegCells_[actor.id] != actor.cellId)
    {
        travelLegCells_[actor.id] = actor.cellId;
        state.nextDoor.clear();
        state.paused = false;
        progress.position = actor.position;
        progress.checkedAt = time_;
        progress.stagnant = 0;
        ++progress.transitions;
        if (progress.transitions > int(memories(actor.id).size() * 3 + 8))
        {
            pause("Travel paused: the remembered route no longer leads reliably onward. Choose a new destination.");
            return;
        }
        // Every actual crossing retains the normal arrival and visibly stops.
        // Auto-travel issues the next local command only after this short pause.
        travelRetryAt_[actor.id] = time_ + .25;
    }
    if (actor.cellId == state.destination)
    {
        actor.path.clear();
        actor.input = {};
        actor.velocity = {};
        state.active = state.paused = false;
        state.nextDoor.clear();
        state.route = {actor.cellId};
        state.status = "Arrived. Travel complete.";
        travelProgress_.erase(actor.id);
        travelLegCells_.erase(actor.id);
        travelRetryAt_.erase(actor.id);
        return;
    }
    if (state.paused)
    {
        const auto* blocker = door(state.nextDoor);
        if (!blocker || blocker->cellId != actor.cellId || !blocker->open)
            return;
        state.paused = false;
        state.nextDoor.clear();
        progress.stagnant = 0;
    }
    if (time_ < travelRetryAt_[actor.id] || actor.postureRemaining > 0 || pendingPortals_.count(actor.id))
        return;
    if (!actor.path.empty() || std::hypot(actor.input.x, actor.input.y) > 1e-7)
    {
        if (time_ - progress.checkedAt >= 1.0)
        {
            progress.stagnant = distance(progress.position, actor.position) < .015 ? progress.stagnant + 1 : 0;
            progress.position = actor.position;
            progress.checkedAt = time_;
            if (progress.stagnant >= 3)
                pause("Travel paused: movement is blocked. Choose a new destination when the way is clear.");
        }
        return;
    }
    if (!state.nextDoor.empty())
    {
        const auto* next = door(state.nextDoor);
        if (next && next->cellId == actor.cellId && distance(actor.position, next->position) <= next->reach)
        {
            if (!next->open)
            {
                pause("Travel paused at a closed door. Choose Open to continue.", next->id);
                return;
            }
            if (!next->portal)
            {
                // The player may open a local barrier while still approaching
                // it, before the journey has formally entered its paused state.
                // Opening it clears that leg; it is never an "enter" action.
                state.nextDoor.clear();
                progress.stagnant = 0;
            }
            else if (!next->boundary)
            {
                TravelGuard guard(issuingTravel_);
                const auto result = interact(actor.id, next->id, "enter");
                if (!result.ok)
                    pause("Travel paused: that entrance is not accessible here. Move closer or choose a new route.");
                return;
            }
        }
        if (!state.nextDoor.empty() && ++progress.stagnant >= 3)
        {
            pause("Travel paused: the local route changed. Choose a new destination.");
            return;
        }
    }

    const auto* current = cell(actor.cellId);
    if (!current)
    {
        pause("Travel paused: no local route is available.");
        return;
    }
    const auto& book = memories(actor.id);
    const auto graph = knownGraph(book, doors_);
    // Remaining routes cannot immediately double back through this cell.
    const auto routes = routesTo(graph, state.destination, actor.cellId);
    struct Candidate
    {
        const Door* door = nullptr;
        Vec2 point;
        double cost = Infinity;
    };
    std::vector<const Door*> exits;
    for (const auto& entry : doors_)
        if (entry.second.cellId == actor.cellId && knownConnection(entry.second, book, doors_) &&
            routes.costs.count(entry.second.targetCell))
            exits.push_back(&entry.second);
    if (exits.empty())
    {
        pause("Travel paused: no remembered onward connection is available. Choose a new destination.");
        return;
    }

    // One coarse weighted flood ranks every seam/portal in this cell. In an
    // exported atlas there can be hundreds of border fixtures; running A* once
    // per fixture would be needlessly expensive. The winning path still uses
    // the shared quarter-tile navigation and normal collision integration.
    const int width = current->width, height = current->height;
    const int start = int(actor.position.y) * width + int(actor.position.x);
    if (start < 0 || start >= width * height)
    {
        pause("Travel paused: no local route is available.");
        return;
    }
    std::vector<double> costs(static_cast<std::size_t>(width * height), Infinity);
    using Entry = std::pair<double, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    costs[start] = 0;
    queue.push({0, start});
    while (!queue.empty())
    {
        const auto item = queue.top();
        queue.pop();
        if (item.first > costs[item.second])
            continue;
        const int x = item.second % width, y = item.second / width;
        const auto* tile = current->tile(x, y);
        if (!tile)
            continue;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (!dx && !dy)
                    continue;
                const int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                    continue;
                const Vec2 point{nx + .5, ny + .5};
                if (!passable(actor.cellId, point, tile->height) ||
                    (dx && dy &&
                     (!passable(actor.cellId, {x + .5, ny + .5}, tile->height) ||
                      !passable(actor.cellId, {nx + .5, y + .5}, tile->height))))
                    continue;
                const auto* next = current->tile(nx, ny);
                const double cost = item.first + (dx && dy ? std::sqrt(2.0) : 1.0) * next->movementCost +
                                    std::abs(next->height - tile->height) * .3;
                const int index = ny * width + nx;
                if (cost < costs[index] - 1e-8)
                {
                    costs[index] = cost;
                    queue.push({cost, index});
                }
            }
    }
    std::vector<Candidate> candidates;
    for (const auto* exit : exits)
    {
        Candidate best;
        best.door = exit;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (exit->boundary && exit->open && (dx || dy))
                    continue;
                const Vec2 point{std::floor(exit->position.x) + .5 + dx, std::floor(exit->position.y) + .5 + dy};
                if (point.x < 0 || point.y < 0 || point.x >= width || point.y >= height ||
                    distance(point, exit->position) > exit->reach - .1)
                    continue;
                const auto* tile = current->tile(int(point.x), int(point.y));
                if (!tile || !passable(actor.cellId, point, tile->height))
                    continue;
                if (!exit->boundary && !lineOfSight(actor.cellId, point, exit->position))
                    continue;
                const double cost = costs[int(point.y) * width + int(point.x)] + routes.costs.at(exit->targetCell) +
                                    (exit->open ? 0 : 2.0);
                if (cost < best.cost - 1e-8)
                {
                    best.cost = cost;
                    best.point = point;
                }
            }
        if (std::isfinite(best.cost))
            candidates.push_back(best);
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.cost == b.cost ? a.door->id < b.door->id : a.cost < b.cost;
    });
    // At most four precise attempts cover a coarse/quarter-grid disagreement.
    // Normally the first candidate succeeds, regardless of seam count.
    for (std::size_t index = 0; index < candidates.size() && index < 4; ++index)
    {
        const auto& candidate = candidates[index];
        TravelGuard guard(issuingTravel_);
        Vec2 command = candidate.point;
        if (candidate.door->boundary && candidate.door->open)
        {
            // Request beyond the intended edge, disambiguating corner tiles
            // which may contain both a north/south and an east/west seam.
            const auto& exit = *candidate.door;
            if (exit.edge == 'N' || (exit.edge == '-' && exit.position.y < 1))
                command.y = -.15;
            else if (exit.edge == 'S' || (exit.edge == '-' && exit.position.y > height - 1))
                command.y = height + .15;
            else if (exit.edge == 'W' || (exit.edge == '-' && exit.position.x < 1))
                command.x = -.15;
            else
                command.x = width + .15;
        }
        Result result;
        if (candidate.door->boundary && candidate.door->open)
            result = moveTo(actor.id, command.x, command.y);
        else
        {
            // A portal's approach point can sit beside an unrelated automatic
            // seam. This is a local approach, not an edge-click instruction:
            // navigate exactly to it without moveTo's boundary-click expansion.
            auto path = findPath(actor, command);
            if (!path.empty())
            {
                actor.path = std::move(path);
                actor.input = {};
                actor.velocity = {};
                actor.turning = false;
                actor.transitioned = false;
                pendingPortals_.erase(actor.id);
                prepareMovement(actor);
                result = {true, "Following a local approach.", {}};
            }
        }
        if (!result.ok)
            continue;
        state.nextDoor = result.targetId.empty() ? candidate.door->id : result.targetId;
        state.route = routeFrom(routes, candidate.door->targetCell, state.destination);
        state.route.insert(state.route.begin(), actor.cellId);
        state.status = "Travelling toward " + book.at(state.destination).name + ".";
        progress.position = actor.position;
        progress.checkedAt = time_;
        travelRetryAt_[actor.id] = time_ + .1;
        return;
    }
    // A local, closable barrier may separate us from every known exit. Shared
    // navigation can approach it without opening it. Only this local state is
    // consulted; the planner has never peeked at a remote door's current state.
    std::sort(exits.begin(), exits.end(), [&](const Door* a, const Door* b) {
        const double ca = distance(actor.position, a->position) + routes.costs.at(a->targetCell);
        const double cb = distance(actor.position, b->position) + routes.costs.at(b->targetCell);
        return ca == cb ? a->id < b->id : ca < cb;
    });
    {
        TravelGuard guard(issuingTravel_);
        const auto result = moveTo(actor.id, exits.front()->position.x, exits.front()->position.y);
        if (result.ok && !result.targetId.empty())
        {
            state.nextDoor = result.targetId;
            state.status = "Approaching a local barrier. Choose Open when you reach it.";
            travelRetryAt_[actor.id] = time_ + .1;
            return;
        }
    }
    pause("Travel paused: the local path is blocked. Choose a new destination when the way is clear.");
}
} // namespace ratw
