// Streamed worlds (RATW_WORLD 3): every cell's header is always known, but a cell's tiles and seams are in memory
// only while someone is in it or next to it. A generated 10x10 grid of outdoor cells; a resident commutes across it.
#include "RatwWorld.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ratw;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

constexpr int Grid = 10, Side = 16;
std::string id(int i, int j) { return "c_" + std::to_string(i) + "_" + std::to_string(j); }

struct Fixture
{
    std::string manifest;
    std::map<std::string, std::string> cells, seams;
    std::map<std::string, int> loads;           // How often each cell's tiles were fetched.
    World::CellSource source()
    {
        return {
            [this](const std::string& cell, std::string& header) {
                const auto found = cells.find(cell);
                if (found == cells.end()) return std::string("no such cell");
                header = found->second.substr(0, found->second.find("grid:") + 5);
                return std::string();
            },
            [this](const std::string& cell, std::string& text, std::string& sides) {
                const auto found = cells.find(cell);
                if (found == cells.end()) return std::string("no such cell");
                ++loads[cell];
                text = found->second;
                sides = seams[cell];
                return std::string();
            }};
    }
};

// The layout the build writes: a manifest of "area" and "exits" records, and each cell with its own seams.
Fixture grid(const std::string& residents)
{
    Fixture f;
    std::ostringstream manifest;
    manifest << "RATW_WORLD 3\n";
    std::map<std::string, std::set<std::string>> exits;
    int seam = 0;
    const auto side = [&](const std::string& a, double ax, double ay, const std::string& b, double bx, double by, char ea, char eb) {
        const std::string sa = "seam_" + std::to_string(seam) + "_a", sb = "seam_" + std::to_string(seam) + "_b";
        std::ostringstream one, two;
        one << "door \"" << sa << "\" \"Open boundary\" \"" << a << "\" " << ax << ' ' << ay << " \"" << b << "\" " << bx << ' ' << by
            << " \"" << sb << "\" 1 0 1 1 \"" << ea << "\"\n";
        two << "door \"" << sb << "\" \"Open boundary\" \"" << b << "\" " << bx << ' ' << by << " \"" << a << "\" " << ax << ' ' << ay
            << " \"" << sa << "\" 1 0 1 1 \"" << eb << "\"\n";
        f.seams[a] += one.str();
        f.seams[b] += two.str();
        exits[a].insert(b);
        exits[b].insert(a);
        ++seam;
    };
    for (int j = 0; j < Grid; ++j)
        for (int i = 0; i < Grid; ++i)
        {
            std::ostringstream cell;
            cell << "id: " << id(i, j) << "\nname: Field " << i << ' ' << j << "\ndescription: Open ground.\nworld: "
                 << i * Side << ' ' << j * Side << " 0\noutdoors: true\nweather: clear\nsize: " << Side << ' ' << Side << "\ngrid:\n";
            for (int y = 0; y < Side; ++y) cell << std::string(Side, '.') << '\n';
            f.cells[id(i, j)] = cell.str();
            manifest << "area \"" << id(i, j) << "\"\n";
            for (int k = 0; k < Side; ++k)
            {
                if (i + 1 < Grid) side(id(i, j), Side - .5, k + .5, id(i + 1, j), .5, k + .5, 'E', 'W');
                if (j + 1 < Grid) side(id(i, j), k + .5, Side - .5, id(i, j + 1), k + .5, .5, 'S', 'N');
            }
        }
    for (const auto& [from, to] : exits)
    {
        manifest << "exits \"" << from << "\" " << to.size();
        for (const auto& next : to) manifest << " \"" << next << '"';
        manifest << '\n';
    }
    manifest << "spawn \"c_0_0\" 8.5 8.5\n" << residents;
    f.manifest = manifest.str();
    return f;
}

const std::string Commuter =
    "economy 1000 100 50 10 12\n"
    "resident \"pip\" \"Pip\" \"civilian\" \"surveying\" \"A walker.\" \"Hello.\" 30 \"timber\" \"female\" \"average\" \"saddle\" "
    "3 1 5 1 1 6 18 \"-\" 10 0 1 \"c_0_0\" 8.5 8.5 \"c_9_9\" 8.5 8.5 \"c_0_0\" 9.5 8.5\n";

// Unless `tiers`, every NPC is simulated in full wherever it is (the streaming itself is what these check).
World load(Fixture& f, bool tiers = false)
{
    World world;
    world.setTiered(tiers);
    world.setCellSource(f.source());
    const auto loaded = world.loadWorldFiles({{"world.ratw", f.manifest}}, "grid");
    expect(loaded.ok, "A streamed world loads: " + loaded.message);
    return world;
}

void headersOnlyUntilNeeded()
{
    auto f = grid(Commuter);
    auto world = load(f);
    expect(world.cells().size() == Grid * Grid, "Every cell's header is known");
    expect(world.cell("c_5_5") && world.cell("c_5_5")->width == Side && !world.cell("c_5_5")->loaded, "A far cell has only its header");
    expect(world.cell("c_5_5")->name == "Field 5 5", "Headers carry names");
    expect(world.cell("c_0_0")->loaded && world.cell("c_9_9")->loaded, "The cells the loader checked (spawn, home, work) are loaded");
    expect(world.loadedCells() <= 3, "Nothing else is loaded yet");
    world.tick(1);
    // Pip is at work in c_9_9 at noon; the cells beside anyone are loaded too.
    expect(world.cell("c_8_9")->loaded && world.cell("c_9_8")->loaded, "Neighbours of an occupied cell load");
    expect(!world.cell("c_5_5")->loaded, "Cells nobody is near stay unloaded");
}

void aResidentWalksAcrossTheWorld()
{
    auto f = grid(Commuter);
    auto world = load(f);
    world.setTimeOfDay(18.5);                                   // Off work: walk home, 18 cells away.
    std::size_t most = 0;
    std::set<std::string> visited;
    int crossings = 0;
    std::string was = world.entity("pip")->cellId;
    for (int second = 0; second < 6000 && world.entity("pip")->cellId != "c_0_0"; ++second)
    {
        world.tick(1);
        if (world.entity("pip")->cellId != was) { ++crossings; was = world.entity("pip")->cellId; }
        visited.insert(world.entity("pip")->cellId);
        most = std::max(most, world.loadedCells());
    }
    expect(world.entity("pip")->cellId == "c_0_0", "Pip walks home across unloaded cells (visited " + std::to_string(visited.size()) + ")");
    expect(visited.size() >= 19, "Through every cell on the way");
    expect(crossings == int(visited.size()) - 1, "Never stepping back into a cell just left (crossings " + std::to_string(crossings) + ")");
    // Cells stay loaded two game minutes after anyone leaves them, so a walker leaves a short trail behind.
    expect(most <= 50, "Only the cells around Pip and a short trail behind are ever loaded, never all 100: " + std::to_string(most));
    for (int second = 0; second < 300; ++second) world.tick(1);
    expect(!world.cell("c_9_9")->loaded && !world.cell("c_5_5")->loaded, "Cells left behind unload once idle");
    expect(world.loadedCells() <= 3, "Afterwards only home and its neighbours stay: " + std::to_string(world.loadedCells()));
    expect(f.loads["c_9_9"] >= 1, "Cells were fetched from the source on demand");
}

void playersCrossAndReturn()
{
    auto f = grid("");
    auto world = load(f);
    world.addPlayer("player-ada", "Ada");
    world.tick(.1);
    expect(world.cell("c_1_0")->loaded, "The cell beside a player is ready before they reach it");
    auto* ada = world.entity("player-ada");
    const auto moved = world.moveTo("player-ada", Side + .15, 8.5);
    expect(moved.ok, "A click past the east edge heads for the seam: " + moved.message);
    for (int i = 0; i < 200 && ada->cellId == "c_0_0"; ++i) world.tick(.1);
    expect(ada->cellId == "c_1_0", "Ada crosses into the next cell");
    world.tick(.1);
    expect(world.cell("c_2_0")->loaded, "and the one beyond it loads");
    // On to c_2_0, then wait until c_0_0 unloads: travel back home still follows the remembered way.
    world.moveTo("player-ada", Side + .15, 8.5);
    for (int i = 0; i < 200 && ada->cellId == "c_1_0"; ++i) world.tick(.1);
    expect(ada->cellId == "c_2_0", "Ada walks on to c_2_0");
    for (int i = 0; i < 200; ++i) world.tick(1);
    expect(!world.cell("c_0_0")->loaded, "Home, two cells back, has unloaded");
    const auto travel = world.travelTo("player-ada", "c_0_0");
    expect(travel.ok, "A remembered route through an unloaded cell is still known: " + travel.message);
    for (int i = 0; i < 600 && ada->cellId != "c_0_0"; ++i) world.tick(.1);
    expect(ada->cellId == "c_0_0", "and followed home");
    world.moveTo("player-ada", Side + .15, 8.5);
    for (int i = 0; i < 200 && ada->cellId == "c_0_0"; ++i) world.tick(.1);
    // A save made now restores into a freshly loaded streamed world.
    const auto saved = world.save();
    for (const auto& [door, open] : saved.doorStates) expect(door.rfind("seam_", 0) != 0, "Seams are not saved: " + door);
    auto fresh = load(f);
    const auto restored = fresh.restore(saved);
    expect(restored.ok, "The save restores: " + restored.message);
    expect(fresh.entity("player-ada") && fresh.entity("player-ada")->cellId == "c_1_0" && fresh.cell("c_1_0")->loaded,
           "Ada is back where she was, and her cell is loaded");
}

// A strip along the west edge of c_1_0 is walled off from the rest of it: every seam from c_0_0 lands there, and
// nothing leads on. A resident going east must not be stranded on it (residents were, for good, and piled up); they
// go around, and one put down on the strip finds the way out.
// Pip works in c_0_0 and lives two cells east, in c_2_0.
const std::string PipEast =
    "economy 1000 100 50 10 12\n"
    "resident \"pip\" \"Pip\" \"civilian\" \"surveying\" \"A walker.\" \"Hello.\" 30 \"timber\" \"female\" \"average\" "
    "\"saddle\" 3 1 5 1 1 6 18 \"-\" 10 0 1 \"c_2_0\" 8.5 8.5 \"c_0_0\" 8.5 8.5 \"c_2_0\" 9.5 8.5\n";

Fixture pocketed()
{
    auto f = grid(PipEast);
    auto& text = f.cells["c_1_0"];
    const auto rows = text.find("grid:\n") + 6;
    for (int y = 0; y < Side; ++y)
    {
        auto at = rows + std::size_t(y * (Side + 1));
        text[at + 3] = '#';                                 // The wall between the strip and the field.
        if (y == Side - 1)
            text[at] = text[at + 1] = text[at + 2] = '#';   // No way out of the strip to the south either.
    }
    // Seams onto the wall are not made (a build never writes one): drop them on both sides.
    const auto solid = [](double x, double y) { return int(x) == 3 || (int(x) < 3 && int(y) == Side - 1); };
    for (auto& [cell, sides] : f.seams)
    {
        std::istringstream in(sides);
        std::string line, kept;
        while (std::getline(in, line))
        {
            std::istringstream fields(line);
            std::string word, id, name1, name2, from, to;
            double ax, ay, bx, by;
            fields >> word >> std::quoted(id) >> std::quoted(name1) >> std::quoted(from) >> ax >> ay >> std::quoted(to) >> bx >> by;
            if ((from == "c_1_0" && solid(ax, ay)) || (to == "c_1_0" && solid(bx, by)))
                continue;
            kept += line + '\n';
        }
        sides = kept;
    }
    return f;
}

void noOneIsStrandedInAPocket()
{
    {
        auto f = pocketed();
        auto world = load(f);
        world.setTimeOfDay(18.5);                               // Off work in c_0_0: home is two cells east.
        expect(world.entity("pip")->cellId == "c_0_0", "Pip starts at work");
        bool pocketed = false;
        for (int second = 0; second < 3000 && world.entity("pip")->cellId != "c_2_0"; ++second)
        {
            world.tick(1);
            const auto* pip = world.entity("pip");
            pocketed |= pip->cellId == "c_1_0" && pip->position.x < 3;
        }
        expect(world.entity("pip")->cellId == "c_2_0", "Pip gets home, around the strip: " + world.entity("pip")->cellId + " " +
                                                           world.entity("pip")->activity);
        expect(!pocketed, "without ever setting foot on it");
    }
    {
        auto f = pocketed();
        auto world = load(f);
        world.setTimeOfDay(18.5);
        world.tick(1);
        auto* pip = world.entity("pip");
        world.ensureLoaded("c_1_0");
        pip->cellId = "c_1_0";                                  // Put down on the strip, as a seam used to leave them.
        pip->position = {.5, 8.5};
        pip->path.clear();
        for (int second = 0; second < 3000 && pip->cellId != "c_2_0"; ++second)
            world.tick(1);
        expect(pip->cellId == "c_2_0", "Put down on the strip, Pip finds the way out and gets home: " + pip->cellId + " " + pip->activity);
    }
}

// A strip whose one way on is a house's door, the house's only way out (the stage review, doc 64, Phase 13): no way on.
// The strip of pocketed(), with a cottage on it. Its door was taken for a way on (one step ahead), so the strip was no
// pocket, and Pip, every way from c_0_0 into c_1_0 landing on it, would cross onto it, find no way on and cross back,
// and again. She goes round it, as round the bare strip.
void aStripWithAHouseIsNoWayOn()
{
    auto f = pocketed();
    f.manifest += "door \"cot_out\" \"The cottage door\" \"c_1_0\" 1.5 8.5 \"h_cot\" 2.5 4.5 \"cot_in\" 0 0 0 0 \"-\"\n"
                  "door \"cot_in\" \"The cottage door\" \"h_cot\" 2.5 5.5 \"c_1_0\" 1.5 9.5 \"cot_out\" 0 0 0 0 \"-\"\n";
    auto& street = f.cells["c_1_0"];
    street[street.find("grid:\n") + 6 + std::size_t(8 * (Side + 1) + 1)] = '+';
    f.cells["h_cot"] = "id: h_cot\nname: The cottage\ndescription: A small room.\nworld: 16 200 0\noutdoors: false\n"
                       "weather: clear\nsize: 6 6\ngrid:\n......\n......\n......\n......\n......\n..+...\n";
    f.seams["h_cot"] = "";
    const std::string was = "exits \"c_1_0\" 3 \"c_0_0\" \"c_1_1\" \"c_2_0\"\n";
    const auto at = f.manifest.find(was);
    expect(at != std::string::npos, "The fixture lists c_1_0's exits");
    f.manifest.replace(at, was.size(), "exits \"c_1_0\" 4 \"c_0_0\" \"c_1_1\" \"c_2_0\" \"h_cot\"\nexits \"h_cot\" 1 \"c_1_0\"\narea \"h_cot\"\n");
    auto world = load(f);
    world.setTimeOfDay(18.5);                               // Off work in c_0_0: home is two cells east.
    bool strip = false;
    int crossings = 0;
    std::string in = world.entity("pip")->cellId;
    for (int second = 0; second < 3000 && world.entity("pip")->cellId != "c_2_0"; ++second)
    {
        world.tick(1);
        const auto* pip = world.entity("pip");
        strip |= pip->cellId == "c_1_0" && pip->position.x < 3;
        if (pip->cellId != in)
            ++crossings, in = pip->cellId;
    }
    expect(world.entity("pip")->cellId == "c_2_0" && !strip, "Pip gets home round the strip and its cottage, never onto it: " +
                                                                 world.entity("pip")->cellId + " " + world.entity("pip")->activity + " after " +
                                                                 std::to_string(crossings) + " crossings");
}

// Pockets in pairs: the edge tiles either side of one seam lie a step below the rest of both cells, so each can
// only be left for the other. Someone set down there (arriving from offstage, say) clambers out over the step.
void aPairedPocketIsClamberedOutOf()
{
    auto f = grid(PipEast);
    for (const auto* id : {"c_0_0", "c_1_0"})
    {
        auto& text = f.cells[id];
        const int low = std::string(id) == "c_0_0" ? Side - 1 : 0;   // The tiles either side of the seam at row 8.
        std::string heights;
        for (int y = 0; y < Side; ++y)
            for (int x = 0; x < Side; ++x)
                heights += "height: " + std::to_string(x) + " " + std::to_string(y) + " " + (x == low && y == 8 ? "0" : "2") + "\n";
        text.insert(text.find("grid:"), heights);
    }
    auto world = load(f);
    world.setTimeOfDay(18.5);
    world.tick(1);
    auto* pip = world.entity("pip");
    world.ensureLoaded("c_1_0");
    pip->cellId = "c_1_0";
    pip->position = {.5, 8.5};
    pip->path.clear();
    for (int second = 0; second < 3000 && pip->cellId != "c_2_0"; ++second)
        world.tick(1);
    expect(pip->cellId == "c_2_0", "From a paired pocket Pip clambers out and gets home: " + pip->cellId + " " +
                                       std::to_string(pip->position.x) + "," + std::to_string(pip->position.y) + " " + pip->activity);
}

// With simulation tiers, a resident far from every player travels in timed hops between known places.
void anOffstageResidentCommutes()
{
    auto f = grid(Commuter);
    auto world = load(f, true);
    expect(world.tiered(), "Streamed worlds are tiered by default");
    world.setTimeOfDay(18.5);                                   // Off work: home is 18 cells away.
    std::set<std::string> visited;
    std::vector<std::string> order;
    int seconds = 0;
    bool placesValid = true;
    for (; seconds < 6000 && world.entity("pip")->cellId != "c_0_0"; ++seconds)
    {
        world.tick(1);
        const auto* pip = world.entity("pip");
        if (order.empty() || order.back() != pip->cellId)
            order.push_back(pip->cellId);
        visited.insert(pip->cellId);
        const auto* here = world.cell(pip->cellId);
        placesValid &= here && pip->position.x > 0 && pip->position.y > 0 && pip->position.x < here->width &&
                       pip->position.y < here->height;
    }
    const auto* pip = world.entity("pip");
    expect(pip->offstage, "With nobody near, Pip is offstage");
    expect(pip->cellId == "c_0_0", "Pip gets home");
    expect(visited.size() >= 19 && order.size() == visited.size(), "Through every cell on the way, never back into one");
    expect(placesValid, "Always somewhere inside a cell");
    // About as long as walking it (18 cells of 16 tiles at a walk), neither instant nor far slower.
    expect(seconds > 90 && seconds < 400, "The journey takes walking time: " + std::to_string(seconds) + " s");
    for (const auto& [cell, count] : f.loads)
        expect(count <= 1, "A cell is fetched at most once to learn its crossings: " + cell);
    for (int second = 0; second < 300; ++second) world.tick(1);
    expect(world.loadedCells() <= 1, "Nothing stays in memory for an offstage resident: " + std::to_string(world.loadedCells()));
    expect(world.society().conserved(), "Money is conserved");
}

void aPlayerBringsResidentsOnstage()
{
    auto f = grid(Commuter);
    auto world = load(f, true);
    world.setTimeOfDay(12);                                     // Pip is at work in c_9_9.
    world.addPlayer("player-ada", "Ada");                       // At the spawn, far away.
    for (int i = 0; i < 20; ++i) world.tick(.1);
    expect(world.entity("pip")->offstage, "A resident nine cells from the only player is offstage");
    // Ada arrives next door.
    world.ensureLoaded("c_8_9");
    auto* ada = world.entity("player-ada");
    ada->cellId = "c_8_9";
    ada->position = {8.5, 8.5};
    for (int i = 0; i < 10; ++i) world.tick(.1);
    const auto* pip = world.entity("pip");
    expect(!pip->offstage, "A player next door brings Pip onstage");
    expect(world.cell("c_9_9")->loaded, "and Pip's cell into memory");
    const auto* here = world.cell(pip->cellId);
    const auto* ground = here ? here->tile(int(pip->position.x), int(pip->position.y)) : nullptr;
    expect(ground && !ground->solid, "Pip is standing somewhere she can stand");
    // Evening: Pip sets off home, walking in full while Ada is beside her cell.
    world.setTimeOfDay(18.5);
    bool walked = false;
    for (int i = 0; i < 40 && !walked; ++i)
    {
        world.tick(.1);
        walked = !world.entity("pip")->path.empty();
    }
    expect(walked && !world.entity("pip")->offstage, "Onstage, Pip walks a planned path");
    // Ada goes home: Pip is offstage again.
    ada->cellId = "c_0_0";
    ada->position = {8.5, 8.5};
    for (int i = 0; i < 10; ++i) world.tick(.1);
    expect(world.entity("pip")->offstage || world.entity("pip")->cellId == "c_0_0" || world.entity("pip")->cellId == "c_1_0" ||
               world.entity("pip")->cellId == "c_0_1",
           "Once Ada leaves, Pip is offstage again");
    expect(world.entity("pip")->path.empty() || !world.entity("pip")->offstage, "and walks no path offstage");
    // A save made mid-journey restores.
    for (int i = 0; i < 60; ++i) world.tick(1);
    const auto saved = world.save();
    auto fresh = load(f, true);
    const auto restored = fresh.restore(saved);
    expect(restored.ok, "A save with Pip offstage mid-journey restores: " + restored.message);
    expect(fresh.entity("pip")->cellId == world.entity("pip")->cellId, "Pip is where she was");
}

// Bodies at a seam (doc 64, Phase 13): three residents make for the same crossing at once, two along the edge and one
// from the field. Only one can stand on the tile it is crossed by at a time; all three get over. (They stood pressed
// round it for good, and residents near players' bodies went hungry so, for game hours, at the city's seams.)
const std::string Three =
    "economy 1000 100 50 10 12\n"
    "resident \"pip\" \"Pip\" \"civilian\" \"surveying\" \"A walker.\" \"Hello.\" 30 \"timber\" \"female\" \"average\" \"saddle\" "
    "3 1 5 1 1 6 18 \"-\" 10 0 1 \"c_0_0\" 4.5 4.5 \"c_0_0\" 4.5 6.5 \"c_0_0\" 5.5 4.5\n"
    "resident \"kit\" \"Kit\" \"civilian\" \"surveying\" \"A walker.\" \"Hello.\" 30 \"timber\" \"male\" \"average\" \"saddle\" "
    "3 1 5 1 1 6 18 \"-\" 10 0 1 \"c_0_0\" 4.5 10.5 \"c_0_0\" 4.5 12.5 \"c_0_0\" 5.5 10.5\n"
    "resident \"lou\" \"Lou\" \"civilian\" \"surveying\" \"A walker.\" \"Hello.\" 30 \"timber\" \"female\" \"average\" \"saddle\" "
    "3 1 5 1 1 6 18 \"-\" 10 0 1 \"c_0_0\" 2.5 8.5 \"c_0_0\" 2.5 10.5 \"c_0_0\" 3.5 8.5\n";

void aCrossingIsNotHeldShut()
{
    auto f = grid(Three);
    auto world = load(f);
    world.tick(.1);
    const std::vector<std::pair<std::string, Vec2>> starts{{"pip", {15.5, 5.5}}, {"kit", {15.5, 11.5}}, {"lou", {12.5, 8.5}}};
    for (const auto& [who, at] : starts)
    {
        auto* e = world.entity(who);
        expect(e && e->npc, who + " is a resident");
        e->leaderId = "test-held";                  // (Out of their schedules: only sent over the seam.)
        e->cellId = "c_0_0";
        e->position = at;
        e->path.clear();
    }
    for (const auto& [who, at] : starts)
        expect(world.moveTo(who, Side + .15, 8.5).ok, who + " heads for the crossing at row 8");
    int over = 0;
    for (int i = 0; i < 300 && over < 3; ++i)
    {
        world.tick(.1);
        over = 0;
        for (const auto& [who, at] : starts)
            over += world.entity(who)->cellId == "c_1_0";
    }
    std::string where;
    for (const auto& [who, at] : starts)
        if (const auto* e = world.entity(who))
            where += " " + who + "@" + e->cellId + ":" + std::to_string(e->position.x) + "," + std::to_string(e->position.y);
    expect(over == 3, "All three get over the seam, none held off it for good:" + where);
}

// Face to face in a lane a tile wide (doc 64, Phase 13): two residents walk its length from either end, each to where
// the other set off. Pushed straight back, neither got by, for good; held a couple of seconds, each steps half a body's
// width to its right (two side by side fill such a lane), and they pass.
void aLaneATileWide()
{
    auto f = grid(Three);
    auto& text = f.cells["c_3_3"];
    const auto rows = text.find("grid:\n") + 6;
    for (int y = 2; y <= 13; ++y)
        text[rows + std::size_t(y * (Side + 1)) + 7] = text[rows + std::size_t(y * (Side + 1)) + 9] = '#';
    auto world = load(f);
    world.ensureLoaded("c_3_3");
    const Vec2 top{8.5, 3.5}, bottom{8.5, 12.5};
    auto* a = world.entity("pip");
    auto* b = world.entity("kit");
    for (auto* e : {a, b})
    {
        e->leaderId = "test-held";                  // (Out of their schedules: only sent along the lane.)
        e->cellId = "c_3_3";
        e->path.clear();
    }
    a->position = top;
    b->position = bottom;
    a->path = {bottom};
    b->path = {top};
    for (int i = 0; i < 300 && !(a->path.empty() && b->path.empty()); ++i)
        world.tick(.1);
    expect(a->path.empty() && std::hypot(a->position.x - bottom.x, a->position.y - bottom.y) < .05 && b->path.empty() &&
               std::hypot(b->position.x - top.x, b->position.y - top.y) < .05,
           "Face to face in a lane a tile wide, they pass: " + std::to_string(a->position.x) + "," + std::to_string(a->position.y) +
               " and " + std::to_string(b->position.x) + "," + std::to_string(b->position.y));
}

// A strip that leads only into a pocket is no way on (doc 64, Phase 13). A wall cuts c_1_0's top row off from the rest
// of it, and walls hem in the tile of c_2_0 that the strip's east end crosses into: the strip has a way on to c_2_0, but
// it lands where nothing leads on. Pip, at work in c_0_0 beside the strip's west end, goes home to c_2_0 through the
// body of c_1_0, never onto the strip (she was sent onto it, as onto a way on, and crossed back and forth at its end).
void aStripIsNoWayOn()
{
    auto f = grid(PipEast);
    const auto tile = [&](const std::string& id, int x, int y) -> char& {
        auto& text = f.cells[id];
        return text[text.find("grid:\n") + 6 + std::size_t(y * (Side + 1) + x)];
    };
    for (int x = 0; x < Side; ++x)
        tile("c_1_0", x, 1) = '#';
    tile("c_2_0", 1, 0) = tile("c_2_0", 0, 1) = '#';
    for (auto& [cell, sides] : f.seams)                       // (No seam onto a wall: a build writes none.)
    {
        std::istringstream in(sides);
        std::string line, kept;
        while (std::getline(in, line))
        {
            std::istringstream fields(line);
            std::string word, id, name, from, to;
            double ax, ay, bx, by;
            fields >> word >> std::quoted(id) >> std::quoted(name) >> std::quoted(from) >> ax >> ay >> std::quoted(to) >> bx >> by;
            if (tile(from, int(ax), int(ay)) == '#' || tile(to, int(bx), int(by)) == '#')
                continue;
            kept += line + '\n';
        }
        sides = kept;
    }
    auto world = load(f);
    world.setTimeOfDay(18.5);                                   // Off work in c_0_0: home is two cells east.
    world.tick(1);
    auto* pip = world.entity("pip");
    pip->cellId = "c_0_0";
    pip->position = {13.5, .5};                                 // Beside the strip's west end.
    pip->path.clear();
    bool strip = false;
    for (int second = 0; second < 600 && pip->cellId != "c_2_0"; ++second)
    {
        world.tick(1);
        strip |= pip->cellId == "c_1_0" && pip->position.y < 1;
    }
    expect(pip->cellId == "c_2_0" && !strip, "Pip gets home through the body of c_1_0, never onto the strip: " + pip->cellId +
                                                 " " + std::to_string(pip->position.x) + "," + std::to_string(pip->position.y) +
                                                 (strip ? " (on the strip)" : ""));
}

// A cell cut in two (doc 64, Phase 13): a wall runs down c_1_0, and Pip's front door is in its smaller, western half.
// Coming home from work in c_2_0 she crosses into the eastern half, the body of the cell, where every way in from
// c_2_0 lands. There is no way on from there; she must go out and come in again on the right side, from c_1_1's edge
// west of the wall. (Residents crossed back and in again at the same seam for game hours in the city, and went
// hungry.) With the house in memory to tell, the way in is judged by where its way on leaves from.
const std::string PipWest =
    "economy 1000 100 50 10 12\n"
    "resident \"pip\" \"Pip\" \"civilian\" \"surveying\" \"A walker.\" \"Hello.\" 30 \"timber\" \"female\" \"average\" "
    "\"saddle\" 3 1 5 1 1 6 18 \"-\" 10 0 1 \"h_pip\" 2.5 2.5 \"c_2_0\" 8.5 8.5 \"h_pip\" 3.5 2.5\n"
    "door \"pip_door_out\" \"Pip's door\" \"c_1_0\" 2.5 8.5 \"h_pip\" 2.5 4.5 \"pip_door_in\" 0 0 0 0 \"-\"\n"
    "door \"pip_door_in\" \"Pip's door\" \"h_pip\" 2.5 5.5 \"c_1_0\" 2.5 9.5 \"pip_door_out\" 0 0 0 0 \"-\"\n";

void aCellCutInTwo()
{
    auto f = grid(PipWest);
    auto& text = f.cells["c_1_0"];
    const auto rows = text.find("grid:\n") + 6;
    for (int y = 0; y < Side; ++y)
        text[rows + std::size_t(y * (Side + 1)) + 5] = '#';      // The wall, at x = 5.
    text[rows + std::size_t(8 * (Side + 1)) + 2] = '+';          // Pip's front door.
    for (auto& [cell, sides] : f.seams)                       // (No seam onto the wall: a build writes none.)
    {
        std::istringstream in(sides);
        std::string line, kept;
        while (std::getline(in, line))
        {
            std::istringstream fields(line);
            std::string word, id, name, from, to;
            double ax, ay, bx, by;
            fields >> word >> std::quoted(id) >> std::quoted(name) >> std::quoted(from) >> ax >> ay >> std::quoted(to) >> bx >> by;
            if ((from == "c_1_0" && int(ax) == 5) || (to == "c_1_0" && int(bx) == 5))
                continue;
            kept += line + '\n';
        }
        sides = kept;
    }
    f.cells["h_pip"] = "id: h_pip\nname: Pip's house\ndescription: A small room.\nworld: 16 200 0\noutdoors: false\n"
                       "weather: clear\nsize: 6 6\ngrid:\n......\n......\n......\n......\n......\n..+...\n";
    f.seams["h_pip"] = "";
    const std::string was = "exits \"c_1_0\" 3 \"c_0_0\" \"c_1_1\" \"c_2_0\"\n";
    const auto at = f.manifest.find(was);
    expect(at != std::string::npos, "The fixture lists c_1_0's exits");
    f.manifest.replace(at, was.size(), "exits \"c_1_0\" 4 \"c_0_0\" \"c_1_1\" \"c_2_0\" \"h_pip\"\nexits \"h_pip\" 1 \"c_1_0\"\narea \"h_pip\"\n");
    auto world = load(f);
    world.setTimeOfDay(18.5);                                   // Off work in c_2_0: home is through c_1_0's west half.
    world.ensureLoaded("c_2_0");
    auto* pip = world.entity("pip");
    expect(pip != nullptr, "Pip is in the world");
    pip->cellId = "c_2_0";
    pip->position = {8.5, 8.5};
    pip->path.clear();
    int crossings = 0;
    std::string was2 = pip->cellId;
    for (int second = 0; second < 3000 && pip->cellId != "h_pip"; ++second)
    {
        world.tick(1);
        if (pip->cellId != was2)
            ++crossings, was2 = pip->cellId;
    }
    expect(pip->cellId == "h_pip", "Pip gets home, round by c_1_1 to the wall's west side: " + pip->cellId + " " +
                                       std::to_string(pip->position.x) + "," + std::to_string(pip->position.y) + " after " +
                                       std::to_string(crossings) + " crossings");
    expect(crossings <= 6, "without crossing back and forth: " + std::to_string(crossings) + " crossings");
    // And out again in the morning, to work. Her door opens on the wall's west side, not on the body of c_1_0; but that
    // side has ways of its own, so it is no pocket, and her house no dead end (it was taken for one, and cut off from
    // the street for everyone).
    world.setTimeOfDay(6.25);
    for (int second = 0; second < 3000 && pip->cellId != "c_2_0"; ++second)
        world.tick(1);
    expect(pip->cellId == "c_2_0", "In the morning Pip goes out to work: " + pip->cellId + " " + std::to_string(pip->position.x) +
                                       "," + std::to_string(pip->position.y) + " " + pip->activity);
}

// The way on that lands best (the stage review, doc 64, Phase 13). A wall runs across c_1_0 and across c_2_0, each at
// row 8; Pip's home is in c_2_0's north half. The south half of c_1_0 also has ways on into c_2_0, landing in its south
// half: a way on, but into the wrong part of her goal's cell. Counted with the north half's, both made the crossing
// into c_1_0 the best there is, and from the south of c_0_0 she took the nearest, into the south half, and had to go
// round by c_3_0 to get home. She crosses into the north half, and in by it.
const std::string PipNorth =
    "economy 1000 100 50 10 12\n"
    "resident \"pip\" \"Pip\" \"civilian\" \"surveying\" \"A walker.\" \"Hello.\" 30 \"timber\" \"female\" \"average\" "
    "\"saddle\" 3 1 5 1 1 6 18 \"-\" 10 0 1 \"c_2_0\" 8.5 3.5 \"c_0_0\" 8.5 12.5 \"c_2_0\" 9.5 3.5\n";

void theBestWayOn()
{
    auto f = grid(PipNorth);
    const auto tile = [&](const std::string& id, int x, int y) -> char& {
        auto& text = f.cells[id];
        return text[text.find("grid:\n") + 6 + std::size_t(y * (Side + 1) + x)];
    };
    for (const auto* id : {"c_1_0", "c_2_0"})
        for (int x = 0; x < Side; ++x)
            tile(id, x, 8) = '#';
    for (auto& [cell, sides] : f.seams)                       // (No seam onto a wall: a build writes none.)
    {
        std::istringstream in(sides);
        std::string line, kept;
        while (std::getline(in, line))
        {
            std::istringstream fields(line);
            std::string word, id, name, from, to;
            double ax, ay, bx, by;
            fields >> word >> std::quoted(id) >> std::quoted(name) >> std::quoted(from) >> ax >> ay >> std::quoted(to) >> bx >> by;
            if (tile(from, int(ax), int(ay)) == '#' || tile(to, int(bx), int(by)) == '#')
                continue;
            kept += line + '\n';
        }
        sides = kept;
    }
    auto world = load(f);
    world.setTimeOfDay(18.5);                                   // Off work in c_0_0: home is two cells east.
    world.ensureLoaded("c_2_0");                                // (In memory, to tell where its ways land.)
    world.tick(1);
    auto* pip = world.entity("pip");
    pip->cellId = "c_0_0";
    pip->position = {12.5, 12.5};                               // Nearer the south half's crossings.
    pip->path.clear();
    int crossings = 0;
    bool south = false;
    std::string in = pip->cellId;
    for (int second = 0; second < 1200 && !(pip->cellId == "c_2_0" && pip->position.y < 8); ++second)
    {
        world.tick(1);
        south |= pip->cellId == "c_1_0" && pip->position.y > 8;
        if (pip->cellId != in)
            ++crossings, in = pip->cellId;
    }
    expect(pip->cellId == "c_2_0" && pip->position.y < 8 && crossings == 2 && !south,
           "Pip gets home by the north half of c_1_0: " + pip->cellId + " " + std::to_string(pip->position.x) + "," +
               std::to_string(pip->position.y) + " after " + std::to_string(crossings) + " crossings" + (south ? " (by the south half)" : ""));
}

// A spot no way reaches is one walker's trouble, not everyone's (doc 64, Phase 13). Pip's place at home is in a little
// room of the house that no door opens into (as a resident's upstairs in Ridgemere is); Kit lives in the same house,
// in the room the front door opens into. Pip, coming home first, found every way in landing short of her own spot and
// took the house's door for a dead end, for everyone: Kit, close behind, had no way home, and nor had any of the
// household after. Kit gets home.
const std::string TwoHome =
    "economy 1000 100 50 10 12\n"
    "resident \"pip\" \"Pip\" \"civilian\" \"surveying\" \"A walker.\" \"Hello.\" 30 \"timber\" \"female\" \"average\" \"saddle\" "
    "3 1 5 1 1 6 18 \"-\" 10 0 1 \"h_two\" 6.5 0.5 \"c_0_0\" 14.5 8.5 \"h_two\" 7.5 0.5\n"
    "resident \"kit\" \"Kit\" \"civilian\" \"surveying\" \"A walker.\" \"Hello.\" 30 \"timber\" \"male\" \"average\" \"saddle\" "
    "3 1 5 1 1 6 18 \"-\" 10 0 1 \"h_two\" 1.5 1.5 \"c_0_0\" 4.5 8.5 \"h_two\" 1.5 2.5\n"
    "door \"two_door_out\" \"The house door\" \"c_1_0\" 3.5 8.5 \"h_two\" 3.5 4.5 \"two_door_in\" 0 0 0 0 \"-\"\n"
    "door \"two_door_in\" \"The house door\" \"h_two\" 3.5 5.5 \"c_1_0\" 3.5 9.5 \"two_door_out\" 0 0 0 0 \"-\"\n";

void aSpotNoWayReaches()
{
    auto f = grid(TwoHome);
    auto& street = f.cells["c_1_0"];
    street[street.find("grid:\n") + 6 + std::size_t(8 * (Side + 1) + 3)] = '+';
    f.cells["h_two"] = "id: h_two\nname: The house\ndescription: Two rooms.\nworld: 16 200 0\noutdoors: false\n"
                       "weather: clear\nsize: 8 6\ngrid:\n.....#..\n.....#..\n.....###\n........\n........\n...+....\n";
    f.seams["h_two"] = "";
    const std::string was = "exits \"c_1_0\" 3 \"c_0_0\" \"c_1_1\" \"c_2_0\"\n";
    const auto at = f.manifest.find(was);
    expect(at != std::string::npos, "The fixture lists c_1_0's exits");
    f.manifest.replace(at, was.size(), "exits \"c_1_0\" 4 \"c_0_0\" \"c_1_1\" \"c_2_0\" \"h_two\"\nexits \"h_two\" 1 \"c_1_0\"\narea \"h_two\"\n");
    auto world = load(f);
    world.setTimeOfDay(18.5);                                   // Off work in c_0_0, both: home is next door.
    world.ensureLoaded("c_0_0");
    auto* pip = world.entity("pip");
    auto* kit = world.entity("kit");
    expect(pip && kit, "Both are in the world");
    for (auto* e : {pip, kit})
    {
        e->cellId = "c_0_0";
        e->path.clear();
    }
    pip->position = {14.5, 8.5};                                // Pip by the edge, Kit well behind.
    kit->position = {2.5, 8.5};
    bool pipHome = false, pipOut = false;
    for (int second = 0; second < 900 && kit->cellId != "h_two"; ++second)
    {
        world.tick(1);
        pipOut |= pipHome && pip->cellId != "h_two";
        pipHome |= pip->cellId == "h_two";
    }
    expect(kit->cellId == "h_two", "Kit gets home: " + kit->cellId + " " + std::to_string(kit->position.x) + "," +
                                       std::to_string(kit->position.y) + " " + kit->activity);
    // And Pip, home, keeps to the nearest of the house she can stand on to her spot (the stage review): she went out
    // and in again, all night, a route searched each time.
    for (int second = 0; second < 600; ++second)
    {
        world.tick(1);
        pipOut |= pipHome && pip->cellId != "h_two";
        pipHome |= pip->cellId == "h_two";
    }
    expect(pipHome && !pipOut && std::hypot(pip->position.x - 4.5, pip->position.y - .5) < .5,
           "Pip, home, stays in, as near her spot as she can stand: " + pip->cellId + " " + std::to_string(pip->position.x) + "," +
               std::to_string(pip->position.y) + (pipOut ? " (went out again)" : ""));
}

void badSourcesAreRefused()
{
    auto f = grid("");
    f.seams["c_0_0"] += "door \"bogus\" \"Open boundary\" \"c_0_0\" 3.5 3.5 \"c_1_0\" .5 3.5 \"x\" 1 0 1 1 \"E\"\n";
    World world;
    world.setCellSource(f.source());
    expect(!world.loadWorldFiles({{"world.ratw", f.manifest}}, "grid").ok, "A seam that is not on its edge is refused");
    auto g = grid("");
    World unsourced;
    expect(!unsourced.loadWorldFiles({{"world.ratw", g.manifest}}, "grid").ok, "Area records need a source");
    g.cells["c_3_3"] = g.cells["c_3_3"].replace(g.cells["c_3_3"].find("size: 16 16"), 11, "size: 20 16");
    World wrong;
    wrong.setCellSource(g.source());
    expect(wrong.loadWorldFiles({{"world.ratw", g.manifest}}, "grid").ok, "Headers alone load");
    expect(!wrong.ensureLoaded("c_3_3").ok, "A cell whose tiles disagree with its header is refused");
}
} // namespace

// Keeping to the roads (the user, 2026-10-12: NPCs use the roads, not overland the whole way). Two cells side by side,
// of heather, a road along the top of both and down the far side of each. Pip lives at the foot of one road and
// works at the foot of the other: straight across the heather is shorter, but she goes up the road, along it and down,
// whether a player is there to see her walk it (onstage, on tiles) or nobody is (offstage, in hops along the way).
void aWalkKeepsToTheRoad()
{
    constexpr int W = 96;
    const auto roadAt = [](int cell, int x, int y) {
        return (y >= 10 && y <= 12) || (cell == 0 && x >= 10 && x <= 12 && y >= 10) || (cell == 1 && x >= 84 && x <= 86 && y >= 10);
    };
    // Any tile the terrain catalog marks a way will do (the user: what is laid as a road or a path is preferred, with
    // nothing more to do): a dirt road, and a footpath.
    expect(roadTerrain('d') && roadTerrain('/') && roadTerrain('_') && roadTerrain('f') && roadTerrain('8') && !roadTerrain(',') &&
               !roadTerrain('4') && !roadTerrain('#'),
           "The catalog's ways are roads, streets, flagstones, bridges and footpaths, and nothing else");
    for (const char way : {'d', '/'})
    for (const bool seen : {true, false})
    {
        Fixture f;
        std::ostringstream manifest;
        manifest << "RATW_WORLD 3\n";
        for (int c = 0; c < 2; ++c)
        {
            std::ostringstream cell;
            cell << "id: r_" << c << "\nname: Heath " << c << "\ndescription: Heather.\nworld: " << c * W << " 0 0\noutdoors: true\n"
                 << "weather: clear\nsize: " << W << ' ' << W << "\ngrid:\n";
            for (int y = 0; y < W; ++y)
            {
                for (int x = 0; x < W; ++x)
                    cell << (roadAt(c, x, y) ? way : '5');
                cell << '\n';
            }
            f.cells["r_" + std::to_string(c)] = cell.str();
            manifest << "area \"r_" << c << "\"\n";
        }
        for (int k = 0; k < W; ++k)
        {
            std::ostringstream one, two;
            one << "door \"s" << k << "a\" \"Open boundary\" \"r_0\" " << W - .5 << ' ' << k + .5 << " \"r_1\" .5 " << k + .5
                << " \"s" << k << "b\" 1 0 1 1 \"E\"\n";
            two << "door \"s" << k << "b\" \"Open boundary\" \"r_1\" .5 " << k + .5 << " \"r_0\" " << W - .5 << ' ' << k + .5
                << " \"s" << k << "a\" 1 0 1 1 \"W\"\n";
            f.seams["r_0"] += one.str();
            f.seams["r_1"] += two.str();
        }
        manifest << "exits \"r_0\" 1 \"r_1\"\nexits \"r_1\" 1 \"r_0\"\nspawn \"r_0\" 11.5 11.5\n"
                 << "economy 1000 100 50 10 12\n"
                 << "resident \"pip\" \"Pip\" \"civilian\" \"surveying\" \"A walker.\" \"Hello.\" 30 \"timber\" \"female\" \"average\" "
                    "\"saddle\" 3 1 5 1 1 6 18 \"-\" 10 0 1 \"r_0\" 11.5 88.5 \"r_1\" 85.5 88.5 \"r_0\" 11.5 87.5\n";
        f.manifest = manifest.str();
        auto world = load(f, true);
        if (seen)
            world.addPlayer("player-ada", "Ada", "r_0", {48.5, 60.5});   // Both cells on the stage.
        world.setTimeOfDay(5.75);                   // Off to work, from home.
        world.ensureLoaded("r_0");
        if (auto* pip = world.entity("pip"))
        {
            pip->cellId = "r_0";
            pip->position = {11.5, 88.5};
            pip->path.clear();
        }
        int onRoad = 0, offRoad = 0, seconds = 0;
        for (; seconds < 1800; ++seconds)
        {
            world.tick(1);
            const auto* pip = world.entity("pip");
            if (pip->cellId == "r_1" && std::hypot(pip->position.x - 85.5, pip->position.y - 88.5) < 2)
                break;
            const int cell = pip->cellId == "r_1", x = int(std::floor(pip->position.x)), y = int(std::floor(pip->position.y));
            bool near = false;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    near = near || roadAt(cell, x + dx, y + dy);
            (near ? onRoad : offRoad) += 1;
        }
        const auto* pip = world.entity("pip");
        const std::string how = std::string(seen ? "Seen" : "Unseen") + (way == '/' ? " on a footpath" : " on a road");
        expect(pip->offstage != seen, how + ": Pip is " + (seen ? "onstage" : "offstage"));
        expect(pip->cellId == "r_1" && std::hypot(pip->position.x - 85.5, pip->position.y - 88.5) < 2,
               how + ", Pip gets to work: " + pip->cellId + " " + std::to_string(pip->position.x) + "," + std::to_string(pip->position.y) +
                   " " + pip->activity);
        expect(onRoad >= 4 * offRoad, how + ", she keeps to the road: " + std::to_string(onRoad) + " s on it, " +
                                          std::to_string(offRoad) + " s off it");
        // The road is the long way round (about 320 tiles to 170 straight): at a walk, minutes, not hours.
        expect(seconds > 150 && seconds < 900, how + ", it takes walking time along the road: " + std::to_string(seconds) + " s");
    }
}

int main()
{
    try
    {
        headersOnlyUntilNeeded();
        aResidentWalksAcrossTheWorld();
        playersCrossAndReturn();
        anOffstageResidentCommutes();
        aPlayerBringsResidentsOnstage();
        noOneIsStrandedInAPocket();
        aPairedPocketIsClamberedOutOf();
        aCrossingIsNotHeldShut();
        aLaneATileWide();
        aStripIsNoWayOn();
        aStripWithAHouseIsNoWayOn();
        aCellCutInTwo();
        theBestWayOn();
        aSpotNoWayReaches();
        badSourcesAreRefused();
        aWalkKeepsToTheRoad();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Stream tests passed: " << checks << " checks.\n";
    return 0;
}
