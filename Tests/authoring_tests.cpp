#include "RatwWorld.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

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
bool near(double a, double b)
{
    return std::abs(a - b) < 1e-6;
}
void write(const fs::path& path, const std::string& contents)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << contents;
    if (!file)
        throw std::runtime_error("Could not write test fixture");
}
std::string replace(std::string text, const std::string& from, const std::string& to)
{
    const auto offset = text.find(from);
    if (offset == std::string::npos)
        throw std::runtime_error("Test replacement did not match: " + from);
    text.replace(offset, from.size(), to);
    return text;
}
std::string cellText(const std::string& id, int worldX, int worldY, bool interior = false,
                     const std::string& extra = "", int width = 8, int height = 8)
{
    std::ostringstream cell;
    cell << "id: " << id << "\nname: " << id << "\ndescription: Authored test terrain.\nworld: " << worldX << ' '
         << worldY << " 0\noutdoors: " << (interior ? "false" : "true") << "\nweather: clear\n";
    cell << extra << "grid:\n";
    for (int y = 0; y < height; ++y)
    {
        std::string row(static_cast<std::size_t>(width), '.');
        if (id == "west" && y == 5 && width > 3)
            row[3] = '+';
        if (id == "room" && y == 1 && width > 1)
            row[1] = '+';
        cell << row << '\n';
    }
    return cell.str();
}
const std::string BaseManifest =
    "RATW_WORLD 1\n"
    "cell \"west\" \"cells/west.cell\"\n"
    "cell \"east\" \"cells/east.cell\"\n"
    "cell \"room\" \"cells/room.cell\"\n"
    "spawn \"west\" 2.5 3.5\n"
    "door \"seam_w\" \"Unseen seam\" \"west\" 7.5 3.5 \"east\" 0.5 3.5 \"seam_e\" 1 0 1 1 \"E\"\n"
    "door \"seam_e\" \"Unseen seam\" \"east\" 0.5 3.5 \"west\" 7.5 3.5 \"seam_w\" 1 0 1 1 \"W\"\n"
    "door \"link_a\" \"Resting room\" \"west\" 3.5 5.5 \"room\" 1.5 2.5 \"link_b\" 0 0 0 0 \"-\"\n"
    "door \"link_b\" \"Return outside\" \"room\" 1.5 1.5 \"west\" 3.5 4.5 \"link_a\" 0 0 0 0 \"-\"\n";
struct Fixture
{
    fs::path dir;
    Fixture()
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int suffix = 0; suffix < 100; ++suffix)
        {
            const auto candidate =
                fs::temp_directory_path() / ("ratw-authoring-" + std::to_string(stamp) + "-" + std::to_string(suffix));
            if (fs::create_directory(candidate))
            {
                dir = candidate;
                break;
            }
        }
        if (dir.empty())
            throw std::runtime_error("Could not create isolated test directory");
        fs::create_directory(dir / "cells");
        reset();
    }
    ~Fixture()
    {
        std::error_code ignored;
        // Only the exact unique directory created by this fixture is removed.
        fs::remove_all(dir, ignored);
    }
    void reset()
    {
        write(dir / "cells/west.cell", cellText("west", 0, 0));
        write(dir / "cells/east.cell", cellText("east", 8, 0));
        write(dir / "cells/room.cell", cellText("room", 0, 0, true));
        manifest(BaseManifest);
    }
    void manifest(const std::string& text)
    {
        write(dir / "world.ratw", text);
    }
    std::string path() const
    {
        return (dir / "world.ratw").string();
    }
};
void tick(World& world, double seconds)
{
    for (int step = 0; step < int(seconds * 60); ++step)
        world.tick(1.0 / 60.0);
}
void expectReject(World& world, const Fixture& fixture, const std::string& label)
{
    const auto beforeCells = world.cells().size(), beforeDoors = world.doors().size();
    const auto beforeActors = world.entities().size();
    const auto beforeTime = world.time();
    const auto actor = *world.entity("tester");
    const auto result = world.loadWorldFile(fixture.path());
    expect(!result.ok, label + " rejected");
    expect(world.cells().size() == beforeCells && world.doors().size() == beforeDoors &&
               world.entities().size() == beforeActors && world.time() == beforeTime &&
               world.entity("tester")->cellId == actor.cellId &&
               near(world.entity("tester")->position.x, actor.position.x),
           label + " preserves live state atomically");
}
void validImportAndMovement()
{
    Fixture fixture;
    World world;
    world.addPlayer("old", "Old");
    tick(world, 1);
    expect(world.loadWorldFile(fixture.path()).ok, "Valid manifest imports");
    expect(world.cells().size() == 3 && !world.cell("tavern"), "Custom cells replace the entire demo");
    expect(world.entities().empty(), "Custom content removes demo NPCs and prior players");
    expect(world.time() == 0, "Custom content starts a new clock");
    auto& player = world.addPlayer("tester", "Tester");
    expect(player.cellId == "west" && near(player.position.x, 2.5) && near(player.position.y, 3.5),
           "Authored spawn controls initial cell and position");
    tick(world, 61);
    expect(world.entities().size() == 1, "Demo schedules do not create or move custom NPCs");
    player.position = {6.5, 3.72};
    const auto snapshot = world.snapshot("tester");
    bool hasSeam = false, hasNeighbor = false;
    for (const auto& door : snapshot.doors)
        hasSeam = hasSeam || door.boundary;
    for (const auto& cell : snapshot.worldMap)
        hasNeighbor = hasNeighbor || cell.id == "east";
    expect(!hasSeam, "Automatic transport seam never appears as a visible door");
    expect(snapshot.cell.id == "west" && snapshot.cell.width == 8, "Local view remains current-cell-only");
    expect(hasNeighbor, "Automatic seam retains normal adjacent-world-map visibility");
    expect(world.actions("tester", "seam_w").empty(), "Seams expose no context-menu target");
    expect(!world.interact("tester", "seam_w", "close").ok, "Forged interaction cannot close a seam");
    expect(world.move("tester", 1, 0).ok, "Continuous movement begins");
    tick(world, 1);
    expect(player.cellId == "east" && player.transitioned, "WASD crosses seam automatically");
    expect(near(player.position.x, .5) && near(player.position.y, 3.72),
           "Seam preserves safe lateral connection point");
    expect(player.input.x == 0 && player.path.empty() && player.velocity.x == 0, "Transition stops all motion");
    tick(world, 1);
    expect(near(player.position.x, .5), "No automatic continuation after crossing");
    world.move("tester", -1, 0);
    tick(world, 1);
    expect(player.cellId == "west" && near(player.position.y, 3.72), "Reciprocal seam returns through the same point");
    player.position = {5.5, 3.5};
    expect(world.moveTo("tester", 8.1, 3.5).ok, "Click beyond edge accepts reachable seam");
    tick(world, 2);
    expect(player.cellId == "east" && player.path.empty(), "Click path crosses and stops");
    player.cellId = "west";
    player.position = {3.5, 4.5};
    expect(!world.door("link_a")->open, "Explicit door starts closed");
    expect(!world.lineOfSight("west", {3.5, 4.5}, {3.5, 6.5}), "Indexed closed door blocks sight");
    expect(world.interact("tester", "link_a", "open").ok, "Explicit door still requires Open action");
    expect(player.cellId == "room" && near(player.position.y, 2.5), "Opening linked door transitions to detached room");
    expect(world.door("link_a")->open && world.door("link_b")->open, "Door state synchronized reciprocally");
    expect(world.lineOfSight("west", {3.5, 4.5}, {3.5, 6.5}), "Indexed fixture immediately follows Open state");
    expect(world.interact("tester", "link_b", "close").ok, "Explicit doors remain closable");
    expect(!world.lineOfSight("west", {3.5, 4.5}, {3.5, 6.5}),
           "Indexed fixture immediately follows reciprocal Close state");
    auto saved = world.save();
    saved.doorStates["seam_w"] = saved.doorStates["seam_e"] = false;
    expect(!world.restore(saved).ok, "Persistence cannot close permanent seams");
    saved = world.save();
    World restored;
    expect(restored.loadWorldFile(fixture.path()).ok && restored.restore(saved).ok,
           "Custom state roundtrips against its world");
    expect(restored.entity("tester")->cellId == "room", "Custom persisted location is restored");
    expect(!restored.lineOfSight("west", {3.5, 4.5}, {3.5, 6.5}), "Fixture index remains valid after restore");
}
void invalidManifests()
{
    Fixture fixture;
    World world;
    world.addPlayer("tester", "Tester");
    tick(world, 2);
    auto reject = [&](const std::string& text, const std::string& label) {
        fixture.reset();
        fixture.manifest(text);
        expectReject(world, fixture, label);
    };
    reject(replace(BaseManifest, "RATW_WORLD 1", "RATW_WORLD 4"), "Unknown format version");
    reject("", "Empty manifest");
    reject(BaseManifest + "unknown 7\n", "Unknown record");
    reject(BaseManifest + "RATW_WORLD 1\n", "Duplicate format header");
    reject(replace(BaseManifest, "\"west\" \"cells/west.cell\"", "west \"cells/west.cell\""), "Unquoted ID");
    reject(BaseManifest + "spawn \"west\" 2.5 3.5\n", "Duplicate spawn");
    reject(replace(BaseManifest, "spawn \"west\" 2.5 3.5\n", ""), "Missing spawn");
    reject(replace(BaseManifest, "spawn \"west\" 2.5 3.5", "spawn \"absent\" 2.5 3.5"), "Dangling spawn");
    reject(replace(BaseManifest, "spawn \"west\" 2.5 3.5", "spawn \"west\" 2.6 3.5"), "Noncenter spawn");
    reject(replace(BaseManifest, "spawn \"west\" 2.5 3.5", "spawn \"west\" 1e300 3.5"), "Huge spawn coordinate");
    reject(replace(BaseManifest, "spawn \"west\" 2.5 3.5", "spawn \"west\" nan 3.5"), "Nonfinite spawn");
    reject(replace(BaseManifest, "spawn \"west\" 2.5 3.5", "spawn \"west\" 3.5 5.5"), "Spawn on explicit endpoint");
    reject(replace(BaseManifest, "spawn \"west\" 2.5 3.5", "spawn \"west\" 2.5 3.5 surplus"), "Trailing tokens");
    reject(BaseManifest + "cell \"west\" \"cells/west.cell\"\n", "Duplicate cell ID");
    reject(replace(BaseManifest, "\"cells/west.cell\"", "\"/etc/passwd\""), "Absolute cell path");
    reject(replace(BaseManifest, "\"cells/west.cell\"", "\"cells/../cells/west.cell\""), "Traversing cell path");
    reject(replace(BaseManifest, "\"cells/west.cell\"", "\"cells/absent.cell\""), "Missing cell file");
    reject(replace(BaseManifest, "cell \"west\"", "cell \"different\""), "Cell identity mismatch");
    reject(replace(BaseManifest, "\"cells/room.cell\"", "\"cells/west.cell\""), "Reused physical file");
    reject(BaseManifest + "door \"seam_w\" \"Duplicate\" \"west\" 7.5 3.5 \"east\" 0.5 3.5 \"seam_e\" 1 0 1 1 \"E\"\n",
           "Duplicate fixture ID");
    reject(replace(BaseManifest, "\"seam_e\" 1 0 1 1", "\"absent\" 1 0 1 1"), "Dangling reciprocal fixture");
    reject(replace(BaseManifest, "7.5 3.5 \"east\"", "7.5 3.5 \"absent\""), "Dangling fixture cell");
    reject(replace(BaseManifest, "7.5 3.5 \"east\"", "7.5 3.5 \"west\""), "Same-cell portal");
    reject(replace(BaseManifest, "\"west\" 7.5 3.5 \"east\"", "\"west\" 1e300 3.5 \"east\""), "Huge endpoint");
    reject(replace(BaseManifest, "\"east\" 0.5 3.5 \"west\"", "\"east\" 1e300 3.5 \"west\""), "Huge paired endpoint");
    reject(replace(BaseManifest, "\"east\" 0.5 3.5 \"seam_e\"", "\"east\" 0.5 9.5 \"seam_e\""),
           "Out-of-bounds arrival");
    reject(replace(BaseManifest, "\"seam_e\" 1 0 1 1", "\"seam_e\" 2 0 1 1"), "Nonboolean fixture flag");
    reject(replace(BaseManifest, "\"link_b\" 0 0 0 0", "\"link_b\" 1 0 0 0"), "Mismatched paired flags");
    reject(replace(BaseManifest, "\"E\"", "\"Q\""), "Unknown edge");
    reject(replace(BaseManifest, "\"E\"", "\"-\""), "Boundary missing edge");
    reject(replace(BaseManifest, "\"E\"", "\"N\""), "Incorrect boundary edge");
    reject(replace(BaseManifest, "\"east\" 0.5 3.5 \"seam_e\"", "\"east\" 1.5 3.5 \"seam_e\""),
           "Displaced seam arrival");
    reject(replace(BaseManifest, "\"room\" 1.5 2.5 \"link_b\"", "\"room\" 6.5 2.5 \"link_b\""),
           "Distant explicit arrival");
    reject(replace(BaseManifest, "\"room\" 1.5 2.5 \"link_b\"", "\"room\" 1.5 1.5 \"link_b\""),
           "Arrival on portal itself");
    auto shut = replace(BaseManifest, "\"seam_e\" 1 0 1 1", "\"seam_e\" 0 0 1 1");
    shut = replace(shut, "\"seam_w\" 1 0 1 1", "\"seam_w\" 0 0 1 1");
    reject(shut, "Closed permanent seam");
    fixture.reset();
    write(fixture.dir / "cells/east.cell", cellText("east", 9, 0));
    expectReject(world, fixture, "Seam between nonadjacent rectangles");
    fixture.reset();
    write(fixture.dir / "cells/east.cell", replace(cellText("east", 8, 0), "world: 8 0 0", "world: 8 0 1"));
    expectReject(world, fixture, "Automatic seam crosses Z levels");
    fixture.reset();
    write(fixture.dir / "cells/east.cell", cellText("east", 8, 0, false, "height: 0 3 1\n"));
    expectReject(world, fixture, "Seam height difference too large");
    fixture.reset();
    write(fixture.dir / "cells/room.cell", cellText("room", 0, 0, true, "height: 1 2 2\n"));
    expectReject(world, fixture, "Arrival stranded behind height step");

    Fixture outside;
    fixture.reset();
    fs::create_symlink(outside.dir / "cells/west.cell", fixture.dir / "cells/escape.cell");
    fixture.manifest(replace(BaseManifest, "cells/west.cell", "cells/escape.cell"));
    expectReject(world, fixture, "Symlink escaping world directory");
}
void cellValidation()
{
    Fixture fixture;
    World world;
    world.addPlayer("tester", "Tester");
    const auto original = cellText("west", 0, 0);
    auto reject = [&](const std::string& cell, const std::string& label) {
        fixture.reset();
        write(fixture.dir / "cells/west.cell", cell);
        expectReject(world, fixture, label);
    };
    reject(replace(original, "weather: clear", "weather: typhoon"), "Unknown weather");
    reject(replace(original, "outdoors: true", "outdoors: maybe"), "Invalid outdoors flag");
    reject(replace(original, "world: 0 0 0", "world: nan 0 0"), "Nonfinite world coordinate");
    reject(replace(original, "world: 0 0 0", "world: 0 0 0 extra"), "Extra metadata tokens");
    reject(replace(original, "name: west", "name: west\nname: duplicate"), "Duplicate cell header");
    reject(replace(original, "name: west", "future: unsupported"), "Unknown cell header");
    reject(replace(original, "........", "....?..."), "Unknown glyph");
    reject(replace(original, "........", "....\xc3\xa9.."), "Unicode is drawn, never stored");
    fixture.reset();
    write(fixture.dir / "cells/west.cell", replace(original, "........", ".kbP_mS."));
    World catalogWorld;
    expect(catalogWorld.loadWorldFile(fixture.path()).ok && catalogWorld.cell("west")->tile(1, 0)->glyph == 'k',
           "Catalog tiles load from a cell file");
    reject(replace(original, "........", "......."), "Ragged cell rows");
    reject(replace(original, "........", ""), "Empty grid row");
    reject(replace(original, "name: west", "name: bad\tname"), "Control character in name");
    reject(cellText("west", 0, 0, false, "height: 1 1 .3\n"), "Non-half height");
    reject(cellText("west", 0, 0, false, "height: 1 1 .25\n"), "Quarter steps are retired");
    reject(cellText("west", 0, 0, false, "height: 1 1 17\n"), "Height out of range");
    reject(cellText("west", 0, 0, false, "height: 8 1 1\n"), "Height outside grid");
    reject(cellText("west", 0, 0, false, "height: 1 1 1\nheight: 1 1 1\n"), "Duplicate height");
    reject(cellText("west", 0, 0, false, "height: 1 1 nan\n"), "Nonfinite height");
    reject(cellText("west", 0, 0, false, "wind: 0 2 1\n"), "Wind strength out of range");
    reject(cellText("west", 0, 0, false, "wind: 0 .5 3\n"), "Invalid wind variable flag");
    reject(cellText("west", 0, 0, true, "wind: 0 .5 1\n"), "Indoor wind");
    reject(cellText("west", 0, 0, false, "", 3, 8), "Cell smaller than minimum");
    reject(cellText("west", 0, 0, false, "", 257, 8), "Cell larger than maximum");
    reject(replace(original, "...+....", "...#...."), "Blocked door endpoint");
    auto blocked = original;
    const auto grid = blocked.find("grid:\n") + 6;
    blocked[grid + 3 * 9 + 2] = '#';
    reject(blocked, "Blocked spawn");
    fixture.reset();
    write(fixture.dir / "cells/west.cell", cellText("west", 0, 0, false, "height: 1 1 -1.5\nwind: 1 .25 0\n"));
    expect(world.loadWorldFile(fixture.path()).ok, "Valid height and wind metadata import");
    expect(near(world.cell("west")->tile(1, 1)->height, -1.5), "Height override survives import");
    expect(near(world.windAt("west").direction, 1) && near(world.windAt("west").strength, .25),
           "Wind override survives import");
    expect(near(world.windAt("room").strength, 0), "Indoor defaults remain calm");
    expect(world.cell("east")->wind.variable && near(world.cell("east")->wind.strength, .5),
           "Outdoor defaults retain live wind");
}
void cornersAndPassages()
{
    Fixture fixture;
    auto manifest =
        BaseManifest +
        "cell \"north\" \"cells/north.cell\"\n"
        "door \"corner_e\" \"Corner east\" \"west\" 7.5 0.5 \"east\" 0.5 0.5 \"corner_w\" 1 0 1 1 \"E\"\n"
        "door \"corner_w\" \"Corner west\" \"east\" 0.5 0.5 \"west\" 7.5 0.5 \"corner_e\" 1 0 1 1 \"W\"\n"
        "door \"corner_n\" \"Corner north\" \"west\" 7.5 0.5 \"north\" 7.5 7.5 \"corner_s\" 1 0 1 1 \"N\"\n"
        "door \"corner_s\" \"Corner south\" \"north\" 7.5 7.5 \"west\" 7.5 0.5 \"corner_n\" 1 0 1 1 \"S\"\n";
    fixture.manifest(manifest);
    write(fixture.dir / "cells/north.cell", cellText("north", 0, -8));
    World world;
    expect(world.loadWorldFile(fixture.path()).ok, "Corner tile supports two different seam edges");
    auto& player = world.addPlayer("tester", "Tester");
    player.position = {7.5, .5};
    world.move("tester", 1, 0);
    tick(world, .5);
    expect(player.cellId == "east", "East corner movement cannot accidentally trigger north exit");
    player.cellId = "west";
    player.position = {7.5, .5};
    world.move("tester", 0, -1);
    tick(world, .5);
    expect(player.cellId == "north", "North corner movement uses north exit");
    player.cellId = "west";
    player.position = {6.5, .5};
    world.moveTo("tester", 8.1, .5);
    tick(world, 1);
    expect(player.cellId == "east", "Corner click path chooses correct edge");

    fixture.reset();
    fixture.manifest(replace(BaseManifest, "spawn \"west\" 2.5 3.5", "spawn \"west\" 7.5 3.5"));
    expect(world.loadWorldFile(fixture.path()).ok, "Spawn on invisible open transport seam remains safe");
    expect(near(world.addPlayer("tester", "Tester").position.x, 7.5), "Border spawn anchor retained");

    fixture.reset();
    manifest = replace(BaseManifest, "\"link_b\" 0 0 0 0", "\"link_b\" 1 0 0 1");
    manifest = replace(manifest, "\"link_a\" 0 0 0 0", "\"link_a\" 1 0 0 1");
    fixture.manifest(manifest);
    expect(world.loadWorldFile(fixture.path()).ok, "Explicit permanent passage imports");
    auto& walker = world.addPlayer("tester", "Tester");
    walker.position = {3.5, 4.5};
    bool visible = false;
    for (const auto& door : world.snapshot("tester").doors)
        visible = visible || door.id == "link_a";
    expect(visible && !world.actions("tester", "link_a").empty(),
           "Explicit passage retains visible interactive fixture");
    expect(!world.interact("tester", "link_a", "close").ok, "Explicit passage cannot close");
    expect(world.interact("tester", "link_a", "enter").ok && walker.cellId == "room",
           "Explicit passage supports Enter");

    fixture.reset();
    manifest = replace(BaseManifest, "\"room\" 1.5 2.5 \"link_b\"", "\"east\" 0.5 3.5 \"link_b\"");
    manifest = replace(manifest, "\"room\" 1.5 1.5 \"west\"", "\"east\" 1.5 3.5 \"west\"");
    fixture.manifest(manifest);
    auto east = cellText("east", 8, 0);
    east[east.find("grid:\n") + 6 + 3 * 9 + 1] = '+';
    write(fixture.dir / "cells/east.cell", east);
    expect(world.loadWorldFile(fixture.path()).ok, "Explicit portal arrival may overlap an open automatic seam");

    fixture.reset();
    manifest = replace(BaseManifest, "\"link_b\" 0 0 0 0", "\"link_b\" 1 0 0 0");
    manifest = replace(manifest, "\"link_a\" 0 0 0 0", "\"link_a\" 1 0 0 0");
    for (const std::string id : {"link_a", "link_b"})
        for (int occurrence = 0; occurrence < 2; ++occurrence)
            manifest = replace(manifest, id, id == "link_a" ? "stairs_a" : "stairs_b");
    fixture.manifest(manifest);
    write(fixture.dir / "cells/west.cell", replace(cellText("west", 0, 0), "...+....", "...^...."));
    write(fixture.dir / "cells/room.cell", replace(cellText("room", 0, 0, true), ".+......", ".^......"));
    expect(world.loadWorldFile(fixture.path()).ok, "Paired stairs import with authored step glyphs");
    auto& climber = world.addPlayer("tester", "Tester");
    climber.position = {3.5, 4.5};
    expect(!world.interact("tester", "stairs_a", "close").ok, "Stairs cannot close");
    expect(world.interact("tester", "stairs_a", "enter").ok && climber.cellId == "room",
           "Authored stairs remain explicit cell-transition actions");
}
void boundsAndCollisionValidation()
{
    Fixture fixture;
    World world;
    world.addPlayer("tester", "Tester");
    fixture.manifest(
        BaseManifest +
        "door \"link_c\" \"Duplicate endpoint\" \"west\" 3.5 5.5 \"room\" 1.5 2.5 \"link_d\" 0 0 0 0 \"-\"\n"
        "door \"link_d\" \"Duplicate endpoint\" \"room\" 1.5 1.5 \"west\" 3.5 4.5 \"link_c\" 0 0 0 0 \"-\"\n");
    expectReject(world, fixture, "Reused explicit fixture endpoint");
    fixture.reset();
    fixture.manifest(BaseManifest + std::string(16385, 'x') + '\n');
    expectReject(world, fixture, "Oversized manifest line");
    fixture.reset();
    write(fixture.dir / "cells/west.cell", std::string(4 * 1024 * 1024 + 1, 'x'));
    expectReject(world, fixture, "Oversized cell file");
    fixture.reset();
    std::string manyCells = BaseManifest;
    for (int i = 0; i < 254; ++i)
    {
        const auto id = "small_" + std::to_string(i);
        write(fixture.dir / "cells" / (id + ".cell"), cellText(id, 100 + i * 4, 100, false, "", 4, 4));
        if (i < 253)
            manyCells += "cell \"" + id + "\" \"cells/" + id + ".cell\"\n";
    }
    fixture.manifest(manyCells);
    expect(world.loadWorldFile(fixture.path()).ok && world.cells().size() == 256, "Maximum 256 cells can import");
    world.addPlayer("tester", "Tester");
    fixture.manifest(manyCells + "cell \"small_253\" \"cells/small_253.cell\"\n");
    expectReject(world, fixture, "257th cell exceeds import bound");
    fixture.reset();
    std::string large = "RATW_WORLD 1\nspawn \"large_0\" 2.5 3.5\n";
    for (int i = 0; i < 4; ++i)
    {
        const auto id = "large_" + std::to_string(i);
        write(fixture.dir / "cells" / (id + ".cell"), cellText(id, i * 256, 0, false, "", 256, 256));
        large += "cell \"" + id + "\" \"cells/" + id + ".cell\"\n";
    }
    fixture.manifest(large);
    expect(world.loadWorldFile(fixture.path()).ok && world.cells().size() == 4, "Maximum 262144 terrain tiles import");
    world.addPlayer("tester", "Tester");
    fixture.manifest(large + "cell \"west\" \"cells/west.cell\"\n");
    expectReject(world, fixture, "Total terrain tile bound");
    fixture.reset();
    std::ostringstream excessive;
    excessive << BaseManifest;
    for (int i = 0; i < 65533; ++i)
        excessive << "door \"limit_" << i
                  << "\" \"Limit\" \"west\" 3.5 5.5 \"room\" 1.5 2.5 \"link_b\" 0 0 0 0 \"-\"\n";
    fixture.manifest(excessive.str());
    const auto rejected = world.loadWorldFile(fixture.path());
    expect(!rejected.ok && rejected.message.find("excessive fixture") != std::string::npos,
           "Fixture records are capped at 65536 before topology validation");
    expect(world.entity("tester") && world.cells().size() == 4, "Fixture-count rejection preserves prior large world");
}
int probe(const std::string& path)
{
    World world;
    const auto result = world.loadWorldFile(path);
    if (!result.ok)
    {
        std::cerr << result.message << '\n';
        return 2;
    }
    auto& player = world.addPlayer("probe", "Atlas probe");
    const auto initialCell = player.cellId;
    const auto initial = player.position;
    bool testedSeam = false, seamPassed = false;
    for (const auto& record : world.doors())
    {
        const auto& seam = record.second;
        if (!seam.passage || !seam.boundary)
            continue;
        testedSeam = true;
        player.cellId = seam.cellId;
        player.position = seam.position;
        const double dx = seam.edge == 'E' ? 1 : seam.edge == 'W' ? -1 : 0;
        const double dy = seam.edge == 'S' ? 1 : seam.edge == 'N' ? -1 : 0;
        world.move("probe", dx, dy);
        tick(world, 1);
        seamPassed =
            player.cellId == seam.targetCell && player.path.empty() && player.input.x == 0 && player.input.y == 0;
        break;
    }
    std::cout << "{\"cells\":" << world.cells().size() << ",\"doors\":" << world.doors().size()
              << ",\"spawn\":{\"cell\":\"" << initialCell << "\",\"x\":" << initial.x << ",\"y\":" << initial.y
              << "},\"seamTested\":" << (testedSeam ? "true" : "false")
              << ",\"seamPassed\":" << (seamPassed ? "true" : "false") << "}\n";
    return testedSeam && !seamPassed ? 3 : 0;
}
} // namespace
int main(int argc, char** argv)
{
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--probe")
            return probe(argv[2]);
        validImportAndMovement();
        invalidManifests();
        cellValidation();
        cornersAndPassages();
        boundsAndCollisionValidation();
        std::cout << checks << " authoring checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Authoring check failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
