#include "RatwWorld.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    expect(!hasCell(world.snapshot("wolf").worldMap, "c"), "Existing nearby map remains first-degree only");
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
        std::cout << checks << " travel checks passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Travel check " << checks << " failed: " << error.what() << '\n';
        return 1;
    }
}
