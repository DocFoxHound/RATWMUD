#include "RatwWorld.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace ratw;
namespace fs = std::filesystem;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
void write(const fs::path& path, const std::string& text)
{
    std::ofstream file(path, std::ios::binary);
    file << text;
    if (!file)
        throw std::runtime_error("Could not write isolated travel fixture.");
}
struct Fixture
{
    fs::path dir;
    std::string manifest;
    Fixture(bool alternate = false, bool edgePortal = false)
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int suffix = 0; suffix < 100; ++suffix)
        {
            const auto candidate =
                fs::temp_directory_path() / ("ratw-travel-" + std::to_string(stamp) + "-" + std::to_string(suffix));
            if (fs::create_directory(candidate))
            {
                dir = candidate;
                break;
            }
        }
        if (dir.empty())
            throw std::runtime_error("Could not create isolated travel fixture.");
        manifest = "RATW_WORLD 1\n";
        for (int index = 0; index < 5; ++index)
        {
            const std::string id(1, char('a' + index));
            std::ostringstream cell;
            cell << "id: " << id << "\nname: Remembered " << id
                 << "\ndescription: Travel fixture.\nworld: " << index * 12
                 << " 0 0\noutdoors: true\nweather: clear\ngrid:\n";
            for (int y = 0; y < 8; ++y)
            {
                std::string row(12, '.');
                if (id == "b" && y == 3 && !edgePortal)
                    row[10] = '+';
                if (id == "b" && y == 4 && edgePortal)
                    row[0] = '+';
                if (id == "c" && y == 3)
                    row[1] = '+';
                cell << row << '\n';
            }
            write(dir / (id + ".cell"), cell.str());
            manifest += "cell \"" + id + "\" \"" + id + ".cell\"\n";
        }
        manifest += "spawn \"a\" 2.5 3.5\n"
                    "door \"seam_ab\" \"East seam\" \"a\" 11.5 3.5 \"b\" 0.5 3.5 \"seam_ba\" 1 0 1 1 \"E\"\n"
                    "door \"seam_ba\" \"West seam\" \"b\" 0.5 3.5 \"a\" 11.5 3.5 \"seam_ab\" 1 0 1 1 \"W\"\n"
                    "door \"link_bc\" \"Hearth door\" \"b\" 10.5 3.5 \"c\" 2.5 3.5 \"link_cb\" 0 0 0 0 \"-\"\n"
                    "door \"link_cb\" \"Return door\" \"c\" 1.5 3.5 \"b\" 9.5 3.5 \"link_bc\" 0 0 0 0 \"-\"\n";
        if (alternate)
            manifest +=
                "door \"seam_alt_ab\" \"Lower seam\" \"a\" 11.5 6.5 \"b\" 0.5 6.5 \"seam_alt_ba\" 1 0 1 1 \"E\"\n"
                "door \"seam_alt_ba\" \"Lower return\" \"b\" 0.5 6.5 \"a\" 11.5 6.5 \"seam_alt_ab\" 1 0 1 1 \"W\"\n";
        if (edgePortal)
        {
            const auto anchor = manifest.find("\"b\" 10.5 3.5");
            manifest.replace(anchor, std::string("\"b\" 10.5 3.5").size(), "\"b\" 0.5 4.5");
            const auto arrival = manifest.find("\"b\" 9.5 3.5");
            manifest.replace(arrival, std::string("\"b\" 9.5 3.5").size(), "\"b\" 1.5 4.5");
        }
        write(dir / "world.ratw", manifest);
    }
    ~Fixture()
    {
        std::error_code ignored;
        fs::remove_all(dir, ignored); // Exact unique fixture created above only.
    }
    void load(World& world)
    {
        const auto result = world.loadWorldFile((dir / "world.ratw").string());
        expect(result.ok, "Fixture loads: " + result.message);
        world.addPlayer("wolf", "Wolf");
    }
};
void tick(World& world, double duration)
{
    for (int step = 0; step < int(std::ceil(duration * 30)); ++step)
        world.tick(1.0 / 30.0);
}
void untilSettled(World& world, double limit)
{
    for (int step = 0; step < int(std::ceil(limit * 30)); ++step)
    {
        if (!world.travelState("wolf").active)
            return;
        world.tick(1.0 / 30.0);
        // A just-opened barrier clears its paused state on the next tick.
        if (world.travelState("wolf").paused)
            return;
    }
}
void visit(World& world, const std::string& cell)
{
    // Fixture setup represents previous exploration; the tests below exercise
    // actual movement and never reposition actors during an active journey.
    auto* actor = world.entity("wolf");
    actor->cellId = cell;
    actor->position = {5.5, 3.5};
    world.observe("wolf");
    actor->position = {11.5, 3.5};
    world.observe("wolf");
    actor->position = {1.5, 3.5};
    world.observe("wolf");
}
void knownRoute(World& world)
{
    visit(world, "b");
    visit(world, "c");
    visit(world, "a");
    world.entity("wolf")->position = {2.5, 3.5};
}
void openHearth(World& world)
{
    auto* actor = world.entity("wolf");
    actor->cellId = "b";
    actor->position = {9.5, 3.5};
    expect(world.interact("wolf", "link_bc", "open").ok, "Fixture opens connecting door explicitly");
    actor->cellId = "a";
    actor->position = {2.5, 3.5};
}
bool hasCell(const std::vector<MapCell>& cells, const std::string& id)
{
    return std::any_of(cells.begin(), cells.end(), [&](const MapCell& cell) { return cell.id == id; });
}
void visitedOnlyAndMemoryPrivacy()
{
    Fixture fixture;
    World world;
    fixture.load(world);
    expect(world.travelMap("wolf").size() == 1, "Only current visited cell is initially listed");
    expect(world.memories("wolf").at("b").knowledge == Knowledge::Glimpsed, "Adjacent cell is only glimpsed");
    const auto unknown = world.travelTo("wolf", "nonexistent");
    const auto hidden = world.travelTo("wolf", "e");
    const auto glimpse = world.travelTo("wolf", "b");
    expect(!unknown.ok && !hidden.ok && !glimpse.ok && unknown.message == hidden.message &&
               hidden.message == glimpse.message,
           "Unknown, unvisited and glimpsed travel requests reject identically");
    expect(!world.travelTo("wolf", std::string(4096, 'x')).ok, "Oversized travel identifier rejected");
    expect(!world.travelTo("missing", "a").ok && world.travelMap("missing").empty(), "Unknown actor rejected");
    knownRoute(world);
    const auto map = world.travelMap("wolf");
    expect(map.size() == 3 && hasCell(map, "c"), "Travel index includes nonadjacent previously visited cells");
    // The nearby map shows every known place outdoors within 512 tiles (doc 29, phase 8), not only the next cells.
    expect(hasCell(world.snapshot("wolf").worldMap, "c"), "The nearby map includes a visited place a few cells away");
    expect(std::all_of(map.begin(), map.end(), [](const MapCell& cell) { return cell.rememberedGlyphs.empty(); }),
           "Travel index transmits no remote tile payload");
    world.cell("c")->name = "Hidden rename";
    world.cell("c")->worldX = 999;
    world.cell("c")->weather = Weather::Snow;
    const auto staleMap = world.travelMap("wolf");
    const auto remembered =
        std::find_if(staleMap.begin(), staleMap.end(), [](const MapCell& c) { return c.id == "c"; });
    expect(remembered->name == "Remembered c" && remembered->worldX == 24 && !remembered->visible,
           "Remote runtime changes do not replace remembered map metadata");
    visit(world, "d");
    visit(world, "a");
    expect(!world.travelTo("wolf", "d").ok, "Visited but disconnected cell rejects without moving");
    auto state = world.save();
    auto& memory = state.memories["wolf"]["b"];
    memory.observed[3 * memory.width] = false;
    memory.glyphs[3 * memory.width] = ' ';
    expect(world.restore(state).ok, "Edited memory fixture restores");
    expect(!world.travelTo("wolf", "b").ok,
           "Visited cell with unobserved reciprocal connection has no remembered route");
}
void realMovementClosedDoorAndArrival()
{
    Fixture fixture;
    World world;
    fixture.load(world);
    knownRoute(world);
    expect(world.setPace("wolf", 10).ok, "Travel uses selected sprint pace");
    const auto result = world.travelTo("wolf", "c");
    expect(result.ok && world.travelState("wolf").active, "Three-cell journey accepted");
    expect(world.travelState("wolf").route == std::vector<std::string>({"a", "b", "c"}), "Known route has three cells");
    expect(world.entity("wolf")->cellId == "a" && world.entity("wolf")->position.x == 2.5,
           "Travel command never teleports the actor");
    tick(world, .2);
    expect(world.entity("wolf")->cellId == "a" && world.entity("wolf")->position.x > 2.5 &&
               world.entity("wolf")->position.x < 5,
           "Journey begins with ordinary continuous local movement");
    expect(world.entity("wolf")->stamina < 100, "Actual sprint travel consumes stamina");
    expect(world.move("wolf", 0, 0).ok && world.travelState("wolf").active && !world.entity("wolf")->path.empty(),
           "Neutral key-release/focus movement does not interrupt an explicit journey");
    const auto destinationBefore = world.travelState("wolf").destination;
    expect(!world.travelTo("wolf", "e").ok && world.travelState("wolf").destination == destinationBefore &&
               !world.entity("wolf")->path.empty(),
           "Invalid replacement request preserves current journey");
    tick(world, 20);
    const auto paused = world.travelState("wolf");
    expect(world.entity("wolf")->cellId == "b" && paused.active && paused.paused && paused.nextDoor == "link_bc",
           "Automatic seam crossing continues to closed explicit door and pauses");
    expect(!world.door("link_bc")->open, "Auto-travel never opens a closed door");
    const auto waiting = world.entity("wolf")->position;
    tick(world, 10);
    expect(std::hypot(waiting.x - world.entity("wolf")->position.x, waiting.y - world.entity("wolf")->position.y) <
               .001,
           "Paused journey does not retry movement or leak through barrier");
    expect(world.interact("wolf", "link_bc", "open").ok, "Explicit Open traverses the paused portal");
    tick(world, .1);
    expect(world.entity("wolf")->cellId == "c" && !world.travelState("wolf").active &&
               world.travelState("wolf").status.find("Arrived") != std::string::npos,
           "Journey completes after manual Open reaches destination");
    expect(std::abs(world.entity("wolf")->position.x - 2.5) < .001 && world.entity("wolf")->path.empty(),
           "Destination preserves safe arrival point and stops");
}
void openDoorAndManualInterruption()
{
    Fixture fixture;
    World world;
    fixture.load(world);
    knownRoute(world);
    openHearth(world);
    expect(world.travelTo("wolf", "c").ok, "Open multi-cell route starts");
    tick(world, 30);
    expect(world.entity("wolf")->cellId == "c" && !world.travelState("wolf").active,
           "Open explicit portal is entered after walking within reach");
    expect(world.travelTo("wolf", "a").ok, "Return route starts");
    tick(world, .1);
    expect(world.move("wolf", 0, 1).ok && !world.travelState("wolf").active, "Manual WASD cancels auto-travel");
    world.stop("wolf");
    expect(world.travelTo("wolf", "a").ok, "Journey restarts after manual movement");
    const auto position = world.entity("wolf")->position;
    expect(world.moveTo("wolf", position.x, position.y).ok && !world.travelState("wolf").active,
           "Manual local click cancels auto-travel");
    expect(world.travelTo("wolf", "a").ok && world.stop("wolf").ok && !world.travelState("wolf").active,
           "Stop cancels auto-travel");
    expect(world.travelTo("wolf", "a").ok && world.setPosture("wolf", "sitting").ok &&
               !world.travelState("wolf").active,
           "Manual posture change cancels auto-travel");
    expect(world.travelTo("wolf", "a").ok, "Seated actor may start a journey");
    const auto seated = world.entity("wolf")->position;
    tick(world, .2);
    expect(std::hypot(seated.x - world.entity("wolf")->position.x, seated.y - world.entity("wolf")->position.y) < .001,
           "Travel retains stand-up delay instead of instant seated movement");
    expect(world.cancelTravel("wolf").ok && world.entity("wolf")->path.empty(),
           "Explicit travel cancellation clears local path");
}
void blockedExitAlternativesAndInvalidation()
{
    Fixture fixture(true);
    World world;
    fixture.load(world);
    knownRoute(world);
    // A new obstacle closes the direct known exit after the player saw it.
    auto* blocked = world.cell("a")->tile(11, 3);
    blocked->solid = blocked->opaque = true;
    blocked->glyph = '#';
    expect(world.travelTo("wolf", "b").ok, "Route with an obstructed direct seam is accepted");
    expect(world.travelState("wolf").nextDoor == "seam_alt_ab",
           "Reachable alternate seam is preferred over blocked exit");
    tick(world, 20);
    expect(world.entity("wolf")->cellId == "b" && !world.travelState("wolf").active,
           "Alternate seam reaches destination through real terrain navigation");
    visit(world, "a");
    world.entity("wolf")->position = {2.5, 3.5};
    // Corridor wall forces the path through a lower opening.
    for (int y = 0; y < 6; ++y)
    {
        auto* tile = world.cell("a")->tile(7, y);
        tile->solid = tile->opaque = true;
        tile->glyph = '#';
    }
    expect(world.travelTo("wolf", "b").ok, "Corridor route starts");
    bool passedOpening = false;
    for (int step = 0; step < 900 && world.travelState("wolf").active; ++step)
    {
        world.tick(1.0 / 30.0);
        const auto* actor = world.entity("wolf");
        if (actor->cellId == "a" && actor->position.x > 7)
            passedOpening = passedOpening || actor->position.y >= 6;
    }
    expect(passedOpening && world.entity("wolf")->cellId == "b",
           "Intelligent pathing detours through corridor opening");
    visit(world, "a");
    world.entity("wolf")->position = {2.5, 3.5};
    expect(world.travelTo("wolf", "b").ok, "Journey starts before dynamic path invalidation");
    for (int y = 6; y < 8; ++y)
    {
        auto* tile = world.cell("a")->tile(7, y);
        tile->solid = tile->opaque = true;
        tile->glyph = '#';
    }
    tick(world, 20);
    expect(world.entity("wolf")->cellId == "a" && world.travelState("wolf").paused &&
               world.entity("wolf")->path.empty(),
           "Newly blocked route pauses with bounded attempts and no teleport");
    expect(!world.travelState("wolf").status.empty(), "Blocked journey explains its paused state");
}
void transientAcrossSaveAndDisconnect()
{
    Fixture fixture;
    World world;
    fixture.load(world);
    knownRoute(world);
    expect(world.travelTo("wolf", "c").ok, "Journey starts before save");
    tick(world, .2);
    const auto saved = world.save();
    auto invalid = saved;
    invalid.players.front().stamina = -1;
    expect(!world.restore(invalid).ok && world.travelState("wolf").active,
           "Rejected restore preserves an in-flight journey atomically");
    expect(world.restore(saved).ok, "In-flight world saves and restores");
    expect(!world.travelState("wolf").active && world.entity("wolf")->path.empty(),
           "Reload preserves physical location but never resumes travel intention");
    expect(world.travelMap("wolf").size() == 3, "Visited travel memory survives reload");
    expect(world.travelTo("wolf", "c").ok, "Fresh journey starts after reload");
    expect(world.removePlayer("wolf"), "Player disconnects");
    world.addPlayer("wolf", "Wolf");
    expect(!world.travelState("wolf").active, "Reconnect never inherits a stale journey");
    knownRoute(world);
    expect(world.travelTo("wolf", "c").ok, "Journey starts before content replacement");
    expect(world.loadWorldFile((fixture.dir / "world.ratw").string()).ok, "Authored world reloads");
    world.addPlayer("wolf", "Wolf");
    expect(!world.travelState("wolf").active && world.travelMap("wolf").size() == 1,
           "New world clears route state and old exploration");
}
void internalDoorAndRemoteState()
{
    World world;
    auto& actor = world.addPlayer("wolf", "Wolf");
    actor.cellId = "loft";
    actor.position = {3.5, 9.5};
    world.observe("wolf");
    actor.cellId = "tavern";
    actor.position = {28.5, 5.5};
    world.observe("wolf");
    actor.position = {20.5, 6.5};
    world.observe("wolf");
    expect(world.travelTo("wolf", "loft").ok, "Journey through a remembered pantry to loft starts");
    untilSettled(world, 4);
    expect(world.travelState("wolf").paused && world.travelState("wolf").nextDoor == "door_pantry" &&
               !world.door("door_pantry")->open,
           "Local non-portal blocker pauses the journey without automatically opening");
    expect(world.interact("wolf", "door_pantry", "open").ok && world.travelState("wolf").active,
           "Manual Open of a non-portal blocker preserves explicit world-travel intent");
    untilSettled(world, 12);
    expect(actor.cellId == "loft" && !world.travelState("wolf").active,
           "Journey resumes local navigation and enters known stairs after manual Open");
    actor.cellId = "tavern";
    actor.position = {22.0, 6.5};
    expect(world.interact("wolf", "door_pantry", "close").ok, "Fixture closes pantry for early intervention");
    expect(world.travelTo("wolf", "loft").ok && !world.travelState("wolf").paused &&
               world.interact("wolf", "door_pantry", "open").ok,
           "Player can explicitly Open a barrier while still approaching it");
    untilSettled(world, 12);
    expect(actor.cellId == "loft" && !world.travelState("wolf").active,
           "Early Open replans a local leg rather than trying to enter a non-portal door");
    Fixture fixture;
    World closed, open;
    fixture.load(closed);
    fixture.load(open);
    knownRoute(closed);
    knownRoute(open);
    openHearth(open);
    expect(closed.travelTo("wolf", "c").ok && open.travelTo("wolf", "c").ok,
           "Remote open and closed door worlds both accept remembered route");
    expect(closed.travelState("wolf").route == open.travelState("wolf").route &&
               closed.travelState("wolf").nextDoor == open.travelState("wolf").nextDoor &&
               closed.travelState("wolf").status == open.travelState("wolf").status,
           "Remote door state does not leak into initial route or status");
}
void portalApproachBesideUnrelatedSeam()
{
    Fixture fixture(false, true);
    World world;
    fixture.load(world);
    knownRoute(world);
    auto* actor = world.entity("wolf");
    actor->cellId = "b";
    actor->position = {.5, 2.5};
    expect(world.travelTo("wolf", "c").ok, "Door beside unrelated boundary seam is a valid travel target");
    bool wrongCell = false;
    for (int step = 0; step < 120; ++step)
    {
        world.tick(1.0 / 30.0);
        wrongCell = wrongCell || actor->cellId != "b";
    }
    expect(!wrongCell && world.travelState("wolf").paused && world.travelState("wolf").nextDoor == "link_bc",
           "Local portal approach stops beside seam without interpreting it as an edge crossing");
}

// Map travel (the user, 2026-11-20: "Players should be able to click to a location on the minimap and the characters
// should travel all the way there. The path there should follow roads as closely as possible."). Four 40 by 24 cells,
// a, b and c in a row and d under c, every edge tile a seam; a dirt road runs up from near the wolf in a, east along
// y 4 through b and c, and down through c into d. Grass lies straight between the wolf and the end of the road.
struct RoadFixture
{
    fs::path dir;
    RoadFixture()
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        dir = fs::temp_directory_path() / ("ratw-maptravel-" + std::to_string(stamp));
        fs::create_directories(dir);
        constexpr int W = 40, H = 24;
        std::map<std::string, std::vector<std::string>> grids;
        for (const auto* id : {"a", "b", "c", "d"})
            grids[id] = std::vector<std::string>(H, std::string(W, ','));
        for (int y = 4; y <= 20; ++y)
            grids["a"][std::size_t(y)][4] = 'd';
        for (int x = 4; x < W; ++x)
            grids["a"][4][std::size_t(x)] = 'd';
        grids["b"][4] = std::string(W, 'd');
        for (int x = 0; x <= 35; ++x)
            grids["c"][4][std::size_t(x)] = 'd';
        for (int y = 4; y < H; ++y)
            grids["c"][std::size_t(y)][35] = 'd';
        for (int y = 0; y <= 12; ++y)
            grids["d"][std::size_t(y)][35] = 'd';
        for (int y = 16; y <= 22; ++y)
            for (int x = 20; x <= 26; ++x)
                grids["d"][std::size_t(y)][std::size_t(x)] = 'W';   // A pond.
        const std::map<std::string, std::pair<int, int>> at{{"a", {0, 0}}, {"b", {40, 0}}, {"c", {80, 0}}, {"d", {80, 24}}};
        std::string manifest = "RATW_WORLD 1\n";
        for (const auto& [id, rows] : grids)
        {
            std::ostringstream cell;
            cell << "id: " << id << "\nname: Field " << id << "\ndescription: Map travel fixture.\nworld: " << at.at(id).first
                 << ' ' << at.at(id).second << " 0\noutdoors: true\nweather: clear\ngrid:\n";
            for (const auto& row : rows)
                cell << row << '\n';
            write(dir / (id + ".cell"), cell.str());
            manifest += "cell \"" + id + "\" \"" + id + ".cell\"\n";
        }
        // An indoor room, for setting out from indoors.
        write(dir / "room.cell", "id: room\nname: A room\ndescription: Indoors.\nworld: 300 300 0\noutdoors: false\nweather: clear\n"
                                 "grid:\n......\n......\n......\n......\n");
        manifest += "cell \"room\" \"room.cell\"\nspawn \"a\" 2.5 20.5\n";
        std::ostringstream seams;
        const auto pair = [&](const std::string& a, double ax, double ay, char edge, const std::string& b, double bx, double by) {
            const char back = edge == 'N' ? 'S' : edge == 'S' ? 'N' : edge == 'W' ? 'E' : 'W';
            const std::string one = "s_" + a + b + "_" + std::to_string(int(ax)) + "_" + std::to_string(int(ay));
            const std::string two = "s_" + b + a + "_" + std::to_string(int(bx)) + "_" + std::to_string(int(by));
            seams << "door \"" << one << "\" \"Seam\" \"" << a << "\" " << ax << ' ' << ay << " \"" << b << "\" " << bx << ' '
                  << by << " \"" << two << "\" 1 0 1 1 \"" << edge << "\"\n";
            seams << "door \"" << two << "\" \"Seam\" \"" << b << "\" " << bx << ' ' << by << " \"" << a << "\" " << ax << ' '
                  << ay << " \"" << one << "\" 1 0 1 1 \"" << back << "\"\n";
        };
        for (int y = 0; y < H; ++y)
        {
            pair("a", W - .5, y + .5, 'E', "b", .5, y + .5);
            pair("b", W - .5, y + .5, 'E', "c", .5, y + .5);
        }
        for (int x = 0; x < W; ++x)
            pair("c", x + .5, H - .5, 'S', "d", x + .5, .5);
        manifest += seams.str();
        write(dir / "world.ratw", manifest);
    }
    ~RoadFixture()
    {
        std::error_code ignored;
        fs::remove_all(dir, ignored);
    }
};
void mapTravel()
{
    RoadFixture fixture;
    World world;
    const auto loaded = world.loadWorldFile((fixture.dir / "world.ratw").string());
    expect(loaded.ok, "Map travel fixture loads: " + loaded.message);
    auto& wolf = world.addPlayer("wolf", "Wolf");
    const auto setOff = [&](const std::string& cell, Vec2 at) {
        world.cancelTravel("wolf");
        wolf.cellId = cell;
        wolf.position = at;
        wolf.path.clear();
    };
    const auto where = [&] { return wolf.cellId + " " + std::to_string(wolf.position.x) + ", " + std::to_string(wolf.position.y); };
    // Walked until the journey ends, counting the tiles walked on and whether they were road.
    const auto walk = [&](double limit, int& onRoad, int& offRoad) {
        onRoad = offRoad = 0;
        std::string lastTile;
        for (int step = 0; step < int(limit * 30) && world.travelState("wolf").active; ++step)
        {
            world.tick(1.0 / 30.0);
            const auto* c = world.cell(wolf.cellId);
            const auto* t = c ? c->tile(int(wolf.position.x), int(wolf.position.y)) : nullptr;
            const std::string tile = wolf.cellId + std::to_string(int(wolf.position.x)) + "," + std::to_string(int(wolf.position.y));
            if (t && tile != lastTile)
                (t->glyph == 'd' ? onRoad : offRoad) += 1;
            lastTile = tile;
        }
    };

    // All the way along the road: through b and c and down into d, to the end of the road.
    setOff("a", {2.5, 20.5});
    auto result = world.travelToPoint("wolf", 80 + 35.5, 24 + 12.5);
    expect(result.ok && world.travelState("wolf").active && world.travelState("wolf").toPoint, "A spot three cells off is taken: " + result.message);
    const auto first = world.travelState("wolf");
    expect(first.destination == "d" && first.route.size() >= 4 && first.route.front() == "a" && first.route.back() == "d",
           "The way is planned through every cell between");
    expect(first.line.size() >= 3 && std::abs(first.line.back().x - 115.5) < .01 && std::abs(first.line.back().y - 36.5) < .01,
           "The maps are given the way, ending on the spot");
    int onRoad = 0, offRoad = 0;
    walk(240, onRoad, offRoad);
    expect(!world.travelState("wolf").active && world.travelState("wolf").status == "Arrived.", "The journey ends: " + world.travelState("wolf").status);
    expect(wolf.cellId == "d" && std::abs(wolf.position.x - 35.5) < .3 && std::abs(wolf.position.y - 12.5) < .3,
           "On the clicked spot, two seams and a corner later: " + where());
    expect(onRoad >= 150 && offRoad <= 8, "It keeps to the road, not straight over the grass: " + std::to_string(onRoad) +
                                              " road tiles, " + std::to_string(offRoad) + " off it");

    // Off the road at the end: along the road, then over the grass for the last stretch.
    setOff("a", {2.5, 20.5});
    expect(world.travelToPoint("wolf", 80 + 30.5, 24 + 8.5).ok, "A spot off the road is taken");
    walk(240, onRoad, offRoad);
    expect(wolf.cellId == "d" && std::abs(wolf.position.x - 30.5) < .3 && std::abs(wolf.position.y - 8.5) < .3,
           "It reaches a spot off the road: " + where());
    expect(onRoad >= 140, "The road takes it most of the way there: " + std::to_string(onRoad) + " road tiles");

    // A pond: as near as the ground allows, and said so.
    setOff("d", {35.5, 2.5});
    expect(world.travelToPoint("wolf", 80 + 23.5, 24 + 19.5).ok, "A spot in a pond is taken");
    walk(120, onRoad, offRoad);
    const auto pond = world.travelState("wolf");
    expect(!pond.active && pond.status.find("as near") != std::string::npos, "It stops by the water and says so: " + pond.status);
    expect(wolf.cellId == "d" && std::hypot(wolf.position.x - 23.5, wolf.position.y - 19.5) < 5, "Beside the pond: " + where());

    // Keys and a click on the ground take over; so does the journey being cancelled.
    setOff("a", {2.5, 20.5});
    expect(world.travelToPoint("wolf", 115.5, 36.5).ok, "Set off again");
    tick(world, 1);
    world.move("wolf", 1, 0);
    expect(!world.travelState("wolf").active && world.travelState("wolf").line.empty(), "WASD ends the journey and its line");
    world.move("wolf", 0, 0);
    expect(world.travelToPoint("wolf", 115.5, 36.5).ok, "Set off once more");
    tick(world, 1);
    world.moveTo("wolf", wolf.position.x, wolf.position.y - 2);
    expect(!world.travelState("wolf").active, "A click on the ground ends it too");
    expect(world.travelToPoint("wolf", 115.5, 36.5).ok, "And again");
    wolf.downedLeft = 30;
    tick(world, .2);
    expect(!world.travelState("wolf").active && world.travelState("wolf").status.find("down") != std::string::npos,
           "Downed, the wolf stops: " + world.travelState("wolf").status);
    wolf.downedLeft = 0;

    // Refused plainly, with nothing changed.
    setOff("a", {2.5, 20.5});
    result = world.travelToPoint("wolf", 300, 300);
    expect(!result.ok && result.message.find("no land") != std::string::npos && wolf.path.empty(), "Past the land: " + result.message);
    result = world.travelToPoint("wolf", 2.6, 20.6);
    expect(!result.ok && result.message.find("already") != std::string::npos, "Where it stands: " + result.message);
    setOff("room", {2.5, 1.5});
    result = world.travelToPoint("wolf", 115.5, 36.5);
    expect(!result.ok && result.message.find("outside") != std::string::npos, "Indoors: " + result.message);
}
} // namespace
int main()
{
    try
    {
        visitedOnlyAndMemoryPrivacy();
        realMovementClosedDoorAndArrival();
        openDoorAndManualInterruption();
        blockedExitAlternativesAndInvalidation();
        transientAcrossSaveAndDisconnect();
        internalDoorAndRemoteState();
        portalApproachBesideUnrelatedSeam();
        mapTravel();
        std::cout << checks << " travel checks passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Travel check " << checks << " failed: " << error.what() << '\n';
        return 1;
    }
}
