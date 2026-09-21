#include "RatwWorld.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

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
bool near(double a, double b, double tolerance = .015)
{
    return std::abs(a - b) <= tolerance;
}
void advance(World& w, double seconds)
{
    for (double t = 0; t < seconds - .001; t += .1)
        w.tick(.1);
}
void quiet(World& w)
{
    for (const auto& e : w.entities())
        if (e.second.npc)
            w.entity(e.first)->leaderId = "test-frozen";
}
Entity& player(World& w, const std::string& id = "p")
{
    quiet(w);
    return w.addPlayer(id, id);
}
const MapCell* mapCell(const Snapshot& s, const std::string& id)
{
    for (const auto& c : s.worldMap)
        if (c.id == id)
            return &c;
    return nullptr;
}
bool hasEntity(const Snapshot& s, const std::string& id)
{
    for (const auto& e : s.entities)
        if (e.id == id)
            return true;
    return false;
}

void authoredWorld()
{
    World w;
    expect(w.cells().size() == 3, "Three separately authored cells");
    expect(w.cell("tavern")->width == 32 && w.cell("tavern")->height == 24, "Standard tavern is 32x24");
    expect(w.cell("exterior")->width > 32, "Exterior demonstrates scrolling-size cell");
    expect(w.cell("loft")->worldZ == 1, "Loft lives on the upper layer");
    expect(w.entities().size() == 6, "Six authored NPC residents");
    for (const auto& entry : w.entities())
    {
        const auto& e = entry.second;
        expect(!w.cell(e.cellId)->tile(int(e.position.x), int(e.position.y))->solid,
               "NPC spawns on traversable terrain");
        expect(!e.description.empty(), "NPC has authored description");
    }
}
void continuousMovement()
{
    World w;
    auto& p = player(w);
    const Vec2 start = p.position;
    expect(w.move("p", 1, 0).ok, "Accept direct input");
    w.tick(.1);
    expect(near(p.position.x, start.x + .26), "Movement is sub-tile continuous");
    expect(near(p.position.y, start.y), "Horizontal input preserves y");
    expect(near(p.facing, 0), "Facing follows actual travel");
    w.stop("p");
    const auto stopped = p.position;
    w.tick(.3);
    expect(near(p.position.x, stopped.x), "Stop prevents continued movement");
    expect(w.face("p", p.position.x, p.position.y - 5).ok, "Idle facing is accepted");
    expect(near(p.facing, -std::acos(-1.0) / 2), "Idle facing points toward cursor");
    const auto facing = p.facing;
    w.tick(.2);
    expect(near(p.facing, facing), "Idle facing persists");
    w.move("p", 1, 1);
    expect(!w.face("p", 0, 0).ok, "Movement has priority over explicit facing");
    const auto diagonal = p.position;
    w.tick(.1);
    expect(near(std::hypot(p.position.x - diagonal.x, p.position.y - diagonal.y), .26),
           "Diagonal movement has no speed advantage");
    w.stop("p");
    p.position = {1.1, 12.5};
    w.move("p", -1, 0);
    w.tick(5);
    expect(p.position.x >= 1.06, "Large timestep does not tunnel through wall");
    expect(!w.move("p", std::numeric_limits<double>::quiet_NaN(), 0).ok, "NaN input rejected");
    expect(!w.moveTo("p", std::numeric_limits<double>::infinity(), 0).ok, "Infinite destination rejected");
    expect(!w.moveTo("p", 1e300, 1e300).ok, "Huge finite destination rejected before grid conversion");
    expect(!w.lineOfSight("tavern", {1, 1}, {1e300, 1e300}), "Huge LOS endpoint rejected before sampling");
    const double before = w.time();
    w.tick(-1);
    w.tick(std::numeric_limits<double>::infinity());
    expect(w.time() == before, "Invalid time cannot corrupt world clock");
}
void clickPathing()
{
    World w;
    auto& p = player(w);
    p.position = {9.5, 17.5};
    expect(w.moveTo("p", 15.5, 17.5).ok, "A* finds route around low table");
    advance(w, 8);
    expect(near(p.position.x, 15.5, .08) && near(p.position.y, 17.5, .08), "Click path reaches exact sub-tile goal");
    expect(p.path.empty(), "Completed path is cleared");
    p.position = {16.5, 12.5};
    expect(w.moveTo("p", 21.5, 12.5).ok, "Straight click accepts exact unsnapped route");
    advance(w, 1);
    expect(near(p.position.x, 19.1, .02) && near(p.position.y, 12.5, .02),
           "Click travel keeps direct-movement speed without quarter-grid zigzag");
    w.stop("p");
    expect(!w.moveTo("p", 12, 16.5).ok, "Cannot path inside solid table");
    expect(!w.moveTo("p", -100, 500).ok, "Invalid distant click has no route");
    p.position = {19.5, 6.5};
    expect(w.moveTo("p", 26.5, 6.5).ok, "Closed door route approaches the obstruction");
    advance(w, 4);
    expect(p.position.x < 23, "Clicking never auto-opens pantry");
    expect(!w.door("door_pantry")->open, "Pantry remains closed until explicit action");
    expect(w.interact("p", "door_pantry", "open").ok, "Explicit close-range open succeeds");
    const Vec2 stopped = p.position;
    advance(w, .5);
    expect(near(p.position.x, stopped.x), "Opening same-cell door leaves player stopped");
    expect(p.path.empty(), "Opening does not resume canceled click path");
    expect(w.moveTo("p", 26.5, 6.5).ok, "New click traverses opened pantry");
    advance(w, 3);
    expect(p.position.x > 26.4, "Fresh click reaches the pantry");
}
void transitions()
{
    World w;
    auto& p = player(w);
    p.position = {16.5, 20.5};
    expect(w.moveTo("p", 16.5, 25).ok, "Click toward closed boundary approaches door");
    advance(w, 3);
    expect(p.cellId == "tavern", "Closed edge does not auto-transition");
    expect(w.interact("p", "door_main", "open").ok, "Explicit portal Open crosses cells");
    expect(p.cellId == "exterior", "Portal entered exterior");
    expect(near(p.position.x, 16.5) && near(p.position.y, 1.5), "Exact authored arrival anchor is preserved");
    expect(p.path.empty() && near(p.velocity.x, 0) && near(p.velocity.y, 0), "Transition cancels motion");
    expect(p.transitioned, "Client receives movement-release latch");
    advance(w, 1);
    expect(near(p.position.y, 1.5), "No held motion carries into destination");
    expect(w.door("door_yard")->open, "Linked door state is consistent");
    expect(w.moveTo("p", 16.5, -.2).ok, "Click path can target unobstructed map edge");
    advance(w, 2);
    expect(p.cellId == "tavern" && near(p.position.y, 22.5), "Open edge auto-transitions and stops at matching anchor");
    p.position = {16.5, 22.5};
    w.move("p", 0, 1);
    advance(w, 2);
    expect(p.cellId == "exterior" && near(p.position.y, 1.5), "Direct WASD can cross an open edge");
    expect(!w.interact("p", "door_pantry", "open").ok, "Cannot interact across stored cells");
    expect(!w.interact("p", "door_yard", "teleport").ok, "Unknown verbs rejected");
}
void gentleCollision()
{
    World w;
    auto& a = player(w, "a");
    auto& b = player(w, "b");
    a.position = {16.5, 12.5};
    b.position = a.position;
    w.tick(.1);
    expect(std::abs(a.position.x - b.position.x) > .001, "Identical positions get gentle separation");
    expect(std::abs(a.position.x - 16.5) < .03, "Gentle bump is small");
    a.position = {15.5, 12.5};
    b.position = {16.5, 12.5};
    w.move("a", 1, 0);
    advance(w, 1);
    expect(a.position.x > 17.5, "Other wolves are not rigid blockers");
    w.stop("a");
    a.position = {16.5, 22.5};
    for (int i = 0; i < 8; ++i)
    {
        auto& q = w.addPlayer("crowd" + std::to_string(i), "crowd");
        q.cellId = "exterior";
        q.position = {16.5, 1.5};
    }
    expect(w.interact("a", "door_main", "open").ok, "Crowded destination cannot block a portal");
    expect(a.cellId == "exterior" && near(a.position.y, 1.5), "Crowded portal preserves authored arrival");
}
void sensesAndWeather()
{
    World w;
    auto& a = player(w, "a");
    auto& b = player(w, "b");
    a.position = {2.5, 12.5};
    b.position = {17.5, 12.5};
    expect(near(w.hearingClarity("a", "b"), 1), "Normal speech is clear within half reference cell");
    b.position = {25.5, 12.5};
    expect(w.hearingClarity("a", "b") > 0 && w.hearingClarity("a", "b") < 1,
           "Longer distance progressively loses speech");
    expect(w.hearingClarity("a", "b", Voice::Whisper) == 0, "Whisper is close-range");
    a.earHealth = .10;
    expect(w.hearingClarity("a", "b") == 0, "Ear damage can eliminate distant comprehension");
    a.earHealth = 0;
    b.position = {2.6, 12.5};
    expect(w.hearingClarity("a", "b") == 0, "Complete deafness is respected");
    a.earHealth = 1;
    a.position = {22.5, 6.5};
    b.position = {26.5, 6.5};
    expect(w.visionClarity("a", "b") == 0, "Closed pantry blocks visual action");
    expect(!hasEntity(w.snapshot("a"), "b"), "Hidden actor omitted from observer snapshot");
    expect(w.hearingClarity("a", "b", Voice::Yell) > 0, "Muted voice may be heard through barrier");
    expect(!w.perceive("a", "b", Voice::Yell).identifiable, "Unseen heard speaker has no identifiable source");
    w.interact("a", "door_pantry", "open");
    expect(w.visionClarity("a", "b") > 0, "Opening door permits line of sight");
    a.cellId = "exterior";
    a.position = {15.5, 12.5};
    b.cellId = "exterior";
    b.position = {30.5, 12.5};
    w.setWeather("exterior", Weather::Clear);
    const double clear = w.visionClarity("a", "b");
    w.setWeather("exterior", Weather::Fog);
    expect(w.visionClarity("a", "b") < clear, "Fog reduces visible distance");
    w.setWeather("exterior", Weather::Clear);
    a.position = {16.5, 7.5};
    w.move("a", 0, 1);
    w.tick(.1);
    const double clearMove = a.position.y - 7.5;
    a.position = {16.5, 7.5};
    w.setWeather("exterior", Weather::Rain);
    w.tick(.1);
    expect(a.position.y - 7.5 < clearMove, "Rain changes movement cost outdoors");
    a.cellId = "tavern";
    a.position = {16.5, 22.5};
    b.cellId = "exterior";
    b.position = {16.5, 1.5};
    expect(w.hearingClarity("a", "b", Voice::Speak) == 0, "Normal speech does not cross cells in MVP");
    expect(w.hearingClarity("a", "b", Voice::Yell) > 0, "Yell propagates through adjacent acoustic portal");
    const double muffled = w.hearingClarity("a", "b", Voice::Yell);
    w.interact("a", "door_main", "open");
    a.cellId = "tavern";
    a.position = {16.5, 20.5};
    expect(w.hearingClarity("a", "b", Voice::Yell) >= muffled, "Open portal improves transmission");
}
void mapMemory()
{
    World w;
    auto& p = player(w);
    auto s = w.snapshot("p");
    expect(mapCell(s, "exterior") == nullptr, "Unseen adjacent cell absent");
    expect(mapCell(s, "loft") == nullptr, "Opaque pantry hides vertical portal");
    p.position = {22.5, 6.5};
    w.interact("p", "door_pantry", "open");
    p.position = {27.5, 5.5};
    s = w.snapshot("p");
    const auto* loft = mapCell(s, "loft");
    expect(loft && loft->visible && loft->knowledge == Knowledge::Glimpsed,
           "Seeing through opening earns coarse glimpse");
    expect(loft->rememberedGlyphs.empty(), "Glimpsed cell does not reveal detailed terrain");
    expect(s.isometric, "Visible vertical stack activates isometric view");
    p.position = {21.5, 12.5};
    s = w.snapshot("p");
    loft = mapCell(s, "loft");
    expect(loft && !loft->visible, "Known adjacent cell remains dim after losing sight");
    expect(!s.isometric, "Remembered vertical outline alone stays top-down");
    p.position = {28.5, 5.5};
    expect(w.interact("p", "stairs_up", "enter").ok, "Steps are explicit transition action");
    expect(p.cellId == "loft", "Entered loft");
    w.snapshot("p");
    p.position = {3.5, 9.5};
    w.interact("p", "stairs_down", "enter");
    p.position = {21.5, 12.5};
    s = w.snapshot("p");
    loft = mapCell(s, "loft");
    expect(loft && loft->knowledge == Knowledge::Visited && !loft->rememberedGlyphs.empty(),
           "Physical visit upgrades permanent detail");
    const auto savedGlyphs = loft->rememberedGlyphs;
    w.cell("loft")->tile(8, 6)->glyph = 'X';
    s = w.snapshot("p");
    expect(mapCell(s, "loft")->rememberedGlyphs == savedGlyphs,
           "Unseen changed terrain does not update remembered map");
    auto saved = w.save();
    saved.time += 3600 * 24 * 365;
    World restored;
    expect(restored.restore(saved).ok, "Persistence accepts permanent map memory");
    auto after = restored.snapshot("p");
    expect(mapCell(after, "loft") && mapCell(after, "loft")->knowledge == Knowledge::Visited,
           "Memory survives restart and a year without decay");
    restored.entity("p")->cellId = "exterior";
    restored.entity("p")->position = {16.5, 2.5};
    after = restored.snapshot("p");
    expect(mapCell(after, "loft") == nullptr, "Previously visited nonadjacent cell is not displayed");
}
void elevationAndPersistence()
{
    World w;
    auto& p = player(w);
    p.position = {16.5, 12.5};
    auto* high = w.cell("tavern")->tile(17, 12);
    high->height = 3;
    w.move("p", 1, 0);
    advance(w, 1);
    expect(p.position.x < 17, "Large elevation discontinuity is not traversable");
    high->height = .5;
    w.move("p", 1, 0);
    advance(w, 1);
    expect(p.position.x > 17, "Small authored height step is traversable");
    w.stop("p");
    p.typing = true;
    p.speakingUntil = 999;
    p.path = {{19, 12}};
    p.input = {1, 0};
    auto saved = w.save();
    expect(saved.players.size() == 1, "Persistence contains player state");
    w.entity("npc_scout")->leaderId = "p";
    w.entity("npc_scout")->position = {18.7, 12.5};
    w.setWeather("exterior", Weather::Fog);
    saved = w.save();
    expect(saved.npcs.size() == 6, "Persistence contains resident continuity");
    expect(!saved.players[0].typing && saved.players[0].path.empty() && saved.players[0].speakingUntil == 0,
           "Transient presence and movement are not persisted");
    World fresh;
    expect(fresh.restore(saved).ok, "Valid world restores");
    expect(near(fresh.entity("p")->position.x, p.position.x), "Restored location is retained");
    expect(fresh.entity("npc_scout")->leaderId == "p" && near(fresh.entity("npc_scout")->position.x, 18.7),
           "Companion recruitment and NPC position survive restart");
    expect(fresh.cell("exterior")->weather == Weather::Fog, "Weather survives restart");
    auto invalid = saved;
    invalid.players[0].position.x = std::numeric_limits<double>::infinity();
    expect(!fresh.restore(invalid).ok, "Invalid persisted position rejected atomically");
    expect(near(fresh.entity("p")->position.x, p.position.x), "Invalid restore preserves previous state");
    invalid = saved;
    invalid.players[0].position.x = 1e300;
    expect(!fresh.restore(invalid).ok, "Huge persisted coordinate rejected without integer overflow");
    invalid = saved;
    invalid.doorStates["door_main"] = true;
    invalid.doorStates["door_yard"] = false;
    expect(!fresh.restore(invalid).ok, "Inconsistent paired portal state is rejected");
}
void cellFiles()
{
    World w;
    for (const std::string id : {"tavern", "exterior", "loft"})
    {
        const Cell before = *w.cell(id);
        expect(w.loadCellFile(std::string(RATW_SOURCE_DIR) + "/Data/Cells/" + id + ".cell").ok,
               "Authored cell file loads");
        const auto* loaded = w.cell(id);
        expect(loaded->width == before.width && loaded->height == before.height, "File and embedded dimensions match");
        expect(loaded->worldZ == before.worldZ && loaded->weather == before.weather,
               "File and embedded metadata match");
        bool identical = loaded->tiles.size() == before.tiles.size();
        for (std::size_t i = 0; i < loaded->tiles.size() && identical; ++i)
            identical = loaded->tiles[i].glyph == before.tiles[i].glyph;
        expect(identical, "File and embedded terrain match");
    }
    expect(!w.loadCellFile("/nonexistent/ratw.cell").ok, "Missing cell content reports failure");
    expect(w.snapshot("missing").entities.empty(), "Unknown observer has no data");
    expect(w.snapshot("npc_keeper").cell.id == "tavern", "NPC snapshot does not crash without player memories");
}
void persistedRoundtripAndPrivacy()
{
    World original;
    auto& p = player(original);
    p.position = {22.5, 6.5};
    original.interact("p", "door_pantry", "open");
    p.position = {28.5, 5.5};
    original.interact("p", "stairs_up", "enter");
    original.snapshot("p");
    p.position = {3.5, 9.5};
    original.interact("p", "stairs_down", "enter");
    p.position = {22.5, 6.5};
    original.interact("p", "door_pantry", "close");
    p.position = {21.5, 12.5};
    auto& q = original.addPlayer("q", "Second wolf");
    q.position = {16.5, 22.5};
    original.interact("q", "door_main", "open");
    p.posture = "sitting";
    p.state = "watching the embers";
    p.facing = 1.25;
    p.hearing = .8;
    p.earHealth = .7;
    p.vision = 1.1;
    p.eyeHealth = .9;
    p.speakingColor = 17;
    original.entity("npc_scout")->leaderId = "p";
    original.entity("npc_scout")->activity = "resting beside the hearth";
    original.setWeather("exterior", Weather::Snow);
    auto before = original.snapshot("p");
    expect(mapCell(before, "exterior") && mapCell(before, "exterior")->knowledge == Knowledge::Glimpsed,
           "Roundtrip fixture includes glimpsed exterior");
    expect(mapCell(before, "loft") && mapCell(before, "loft")->knowledge == Knowledge::Visited &&
               !mapCell(before, "loft")->visible,
           "Roundtrip fixture includes out-of-sight visited loft");
    const auto saved = original.save();
    World restored;
    expect(restored.restore(saved).ok, "Full persisted world restores into new simulation");
    const auto after = restored.snapshot("p");
    const auto* returned = restored.entity("p");
    expect(returned->posture == "sitting" && returned->state == "watching the embers" && near(returned->facing, 1.25),
           "Posture, authored state and idle facing survive reload");
    expect(near(returned->hearing, .8) && near(returned->earHealth, .7) && near(returned->vision, 1.1) &&
               near(returned->eyeHealth, .9) && returned->speakingColor == 17,
           "Sense modifiers, injuries and selected color survive reload");
    expect(restored.entity("npc_scout")->leaderId == "p" &&
               restored.entity("npc_scout")->activity == "resting beside the hearth",
           "Resident relationship and activity survive reload");
    expect(restored.door("door_main")->open && restored.door("door_yard")->open && !restored.door("door_pantry")->open,
           "Both sides of open portal and closed interior barrier survive reload");
    expect(restored.cell("exterior")->weather == Weather::Snow, "Weather is included in full roundtrip");
    expect(after.visibleTiles == before.visibleTiles && after.rememberedTiles == before.rememberedTiles,
           "Local observer visibility and remembered masks are stable after reload");
    expect(mapCell(after, "loft")->rememberedGlyphs == mapCell(before, "loft")->rememberedGlyphs,
           "Visited detail survives exact roundtrip");
    expect(mapCell(after, "exterior")->rememberedGlyphs.empty(),
           "Glimpsed memory gains no detailed terrain from restore");
    expect(!hasEntity(after, "q") && !hasEntity(after, "npc_cook") && !hasEntity(after, "npc_scribe"),
           "Restart does not reveal actors in another cell or behind closed pantry");
    bool hiddenProperties = true;
    for (std::size_t i = 0; i < after.cell.tiles.size(); ++i)
        if (!after.visibleTiles[i])
        {
            const auto& t = after.cell.tiles[i];
            hiddenProperties = hiddenProperties && !t.solid && !t.opaque && t.height == 0 && t.movementCost == 1;
            if (!after.rememberedTiles[i])
                hiddenProperties = hiddenProperties && t.glyph == ' ';
        }
    expect(hiddenProperties, "Hidden local tiles transmit no live geometry attributes after reload");
    restored.cell("loft")->tile(8, 6)->glyph = 'X';
    expect(mapCell(restored.snapshot("p"), "loft")->rememberedGlyphs == mapCell(before, "loft")->rememberedGlyphs,
           "Remote mutation after reload cannot refresh remembered details");
    const auto savedAgain = restored.save();
    World twice;
    expect(twice.restore(savedAgain).ok, "Second save/reload remains valid");
    expect(twice.memories("p").at("loft").glyphs == saved.memories.at("p").at("loft").glyphs,
           "Repeat reload does not manufacture observations");
}
void malformedPersistence()
{
    World source;
    auto& p = player(source);
    p.state = "preserve me";
    const auto saved = source.save();
    World destination;
    expect(destination.restore(saved).ok, "Malformed-state tests start from valid save");
    auto reject = [&](PersistedWorld bad, const std::string& reason) {
        expect(!destination.restore(bad).ok, reason);
        expect(destination.entity("p") && destination.entity("p")->state == "preserve me" &&
                   destination.time() == saved.time && !destination.door("door_main")->open,
               "Rejected restore is atomic");
    };
    auto bad = saved;
    bad.time = 1e300;
    reject(bad, "Reject finite but unusably large persisted clock");
    bad = saved;
    bad.memories["p"]["tavern"].knowledge = static_cast<Knowledge>(99);
    reject(bad, "Reject invalid knowledge enum");
    bad = saved;
    bad.memories["p"]["tavern"].worldZ = std::numeric_limits<double>::quiet_NaN();
    reject(bad, "Reject nonfinite remembered map coordinates");
    bad = saved;
    bad.memories["p"]["tavern"].observed.pop_back();
    reject(bad, "Reject mismatched observed and glyph arrays");
    bad = saved;
    bad.memories["p"]["tavern"].width = 0;
    reject(bad, "Reject impossible remembered dimensions");
    bad = saved;
    bad.memories["p"]["tavern"].knowledge = Knowledge::Glimpsed;
    reject(bad, "Reject detailed terrain attached to coarse glimpse");
    bad = saved;
    auto& memory = bad.memories["p"]["tavern"];
    memory.observed[0] = false;
    memory.glyphs[0] = 'S';
    reject(bad, "Reject stored terrain detail that has never been observed");
    bad = saved;
    bad.doorStates.erase("door_yard");
    bad.doorStates["door_main"] = true;
    reject(bad, "Reject partial door restore inconsistent with linked side");
    bad = saved;
    bad.doorStates["stairs_up"] = false;
    bad.doorStates["stairs_down"] = false;
    reject(bad, "Reject closed door state for permanent open steps");
    bad = saved;
    bad.players[0].position = {23.5, 6.5};
    reject(bad, "Reject actor embedded in saved-closed pantry threshold");
    bad = saved;
    bad.players[0].position = {1.01, 12.5};
    reject(bad, "Reject actor core intersecting solid wall");
    bad = saved;
    bad.players[0].hearing = -1;
    reject(bad, "Reject negative saved hearing sensitivity");
    bad = saved;
    bad.players[0].eyeHealth = 1.5;
    reject(bad, "Reject invalid saved injury fraction");
    bad = saved;
    bad.players.push_back(bad.players.front());
    reject(bad, "Reject duplicate player identity");
    bad = saved;
    bad.players[0].id = "npc_keeper";
    reject(bad, "Reject saved player taking resident identity");
    bad = saved;
    bad.weather["exterior"] = static_cast<Weather>(33);
    reject(bad, "Reject invalid saved weather enum");
}
void schedules()
{
    World w;
    const auto start = w.entity("npc_scout")->position;
    advance(w, 68);
    const auto* scout = w.entity("npc_scout");
    expect(scout->cellId == "exterior" || scout->position.y > start.y + 1,
           "Scheduled resident walks toward next routine");
    expect(scout->activity == "checking the road", "Schedule exposes current activity");
    World party;
    party.entity("npc_scout")->leaderId = "p";
    const auto held = party.entity("npc_scout")->position;
    advance(party, 68);
    expect(near(party.entity("npc_scout")->position.x, held.x) && near(party.entity("npc_scout")->position.y, held.y),
           "Recruitment suspends autonomous schedule");
}
} // namespace
int main()
{
    try
    {
        authoredWorld();
        continuousMovement();
        clickPathing();
        transitions();
        gentleCollision();
        sensesAndWeather();
        mapMemory();
        elevationAndPersistence();
        schedules();
        cellFiles();
        persistedRoundtripAndPrivacy();
        malformedPersistence();
        std::cout << "Passed " << checks << " world behavior assertions.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Failed after " << checks << " assertions: " << error.what() << "\n";
        return 1;
    }
}
