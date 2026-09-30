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
        badSourcesAreRefused();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Stream tests passed: " << checks << " checks.\n";
    return 0;
}
