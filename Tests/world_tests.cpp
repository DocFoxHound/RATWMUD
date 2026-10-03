#include "RatwWorld.h"
#include "RatwStep.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <random>
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
    expect(near(p.facing, 0), "Facing request does not instantly rotate the wolf");
    w.tick(.1);
    expect(p.facing < 0 && p.facing > -std::acos(-1.0) / 2, "Idle facing turns gradually toward cursor");
    advance(w, .6);
    expect(near(p.facing, -std::acos(-1.0) / 2), "Idle facing eventually reaches cursor direction");
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
void gradualFacing()
{
    World w;
    auto& p = player(w);
    const double pi = std::acos(-1.0);
    p.facing = pi - .1;
    const double target = -pi + .1;
    const Vec2 origin = p.position;
    expect(w.face("p", origin.x + std::cos(target), origin.y + std::sin(target)).ok,
           "Facing accepts a target across the angular wrap boundary");
    const double before = p.facing;
    w.tick(.02);
    const double step = std::remainder(p.facing - before, 2 * pi);
    expect(step > 0 && step < .2, "Wrapped facing takes the short arc, not nearly a full revolution");
    advance(w, .2);
    expect(near(std::remainder(p.facing - target, 2 * pi), 0), "Wrapped turn lands on exact requested heading");
    expect(near(p.position.x, origin.x) && near(p.position.y, origin.y), "Stationary turning never translates actor");
    expect(!p.turning, "Completed manual turn releases its turning flag");
    expect(w.face("p", origin.x, origin.y - 5).ok, "A second stationary turn may be requested");
    w.move("p", 1, 0);
    w.tick(.1);
    expect(near(p.facing, 0) && !p.turning, "Actual movement cancels a pending manual turn and owns facing");
    w.stop("p");
    const double movementFacing = p.facing;
    advance(w, .5);
    expect(near(p.facing, movementFacing), "Canceled turn does not resume when movement stops");
    expect(!w.face("p", std::numeric_limits<double>::quiet_NaN(), 4).ok, "Nonfinite manual facing target is rejected");
    expect(!w.face("absent", 1, 1).ok, "Unknown actor cannot turn");
}

void postureMovement()
{
    World w;
    auto& p = player(w);
    p.position = {16.5, 12.5};
    const Vec2 origin = p.position;
    expect(w.setPosture("p", "sitting").ok && p.posture == "sitting", "Sitting is an explicit actor posture");
    for (int i = 0; i < 4; ++i)
    {
        expect(w.move("p", 1, 0).ok, "Held movement is accepted during standing preparation");
        w.tick(.1);
        expect(near(p.position.x, origin.x), "Sitting wolf cannot translate during initial rise");
    }
    advance(w, .8);
    expect(p.posture == "standing" && p.position.x > origin.x + .4,
           "Repeated held input does not restart rise and eventually walks standing");
    w.stop("p");
    w.setPosture("p", "sitting");
    const Vec2 stopped = p.position;
    w.move("p", 1, 0);
    w.tick(.1);
    w.stop("p");
    advance(w, 1.2);
    expect(near(p.position.x, stopped.x) && near(p.position.y, stopped.y),
           "Releasing movement during rising prevents delayed ghost movement");
    expect(p.path.empty() && near(p.input.x, 0) && near(p.input.y, 0), "Release clears all queued travel intent");

    p.position = origin;
    w.setPosture("p", "lying");
    expect(p.posture == "lying", "Lying is an explicit actor posture");
    w.move("p", 1, 0);
    w.tick(.1);
    expect(near(p.position.x, origin.x), "Lying wolf does not instantly enter moving crouch");
    advance(w, .6);
    expect(p.posture == "crouching" && p.position.x > origin.x,
           "Moving out of lying enters crouch-walk rather than full-speed standing");
    const double crouchStart = p.position.x;
    advance(w, .5);
    const double crouchDistance = p.position.x - crouchStart;
    expect(crouchDistance > .1 && crouchDistance < .65, "Crouch-walk is substantially slower than ordinary walk");
    w.stop("p");
    expect(p.posture == "crouching", "Stopping crouch-walk preserves low posture");
    expect(w.setPosture("p", "standing").ok, "Crouching wolf can explicitly stand");
    w.tick(.1);
    expect(p.posture != "standing", "Explicit stand from crouching has a short preparation time");
    advance(w, .8);
    expect(p.posture == "standing", "Explicit stand eventually finishes");
    const double walkStart = p.position.x;
    w.move("p", 1, 0);
    advance(w, .5);
    expect(p.position.x - walkStart > crouchDistance * 2, "Standing restores normal walking speed");
    w.stop("p");

    p.position = origin;
    w.setPosture("p", "sitting");
    expect(w.moveTo("p", 21.5, 12.5).ok, "Click path may be queued from sitting");
    w.tick(.1);
    expect(near(p.position.x, origin.x) && !p.path.empty(), "Click path waits through standing preparation");
    advance(w, 4);
    expect(near(p.position.x, 21.5, .08) && p.path.empty(), "Queued click path resumes and completes after rising");
    p.position = origin;
    w.setPosture("p", "lying");
    w.setPosture("p", "standing");
    advance(w, .5);
    expect(p.posture != "standing", "Standing fully from lying takes longer than a quick crouch transition");
    advance(w, .8);
    expect(p.posture == "standing", "Lying wolf can finish explicit full stand");
    expect(!w.setPosture("p", "teleporting").ok, "Unrecognized posture is rejected");
    expect(!w.setPosture("absent", "sitting").ok, "Unknown actor cannot change posture");
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
    // (The scout, who stands in that lane, kept out of it: walking into someone nudges them and is slowed by it.)
    w.entity("npc_scout")->leaderId = "test-frozen";
    w.entity("npc_scout")->position = {16.5, 15.5};
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
// Bodies (step::BodyRadius): wolves crowd and nudge each other but never stand on top of one another.
void bodies()
{
    const auto distance = [](Vec2 p, Vec2 q) { return std::hypot(p.x - q.x, p.y - q.y); };
    World w;
    for (const auto& [id, e] : w.entities())
        if (e.npc)
            w.entity(id)->cellId = "loft";            // (The tavern to themselves.)
    auto& a = player(w, "a");
    auto& b = player(w, "b");
    const double body = step::BodyRadius * 2;
    a.position = {16.5, 12.5};
    b.position = a.position;
    w.tick(.1);
    expect(distance(a.position, b.position) > .1, "Two on the very same spot are pushed apart at once");
    advance(w, .5);
    expect(distance(a.position, b.position) >= body * .8 - 1e-6, "and soon stand apart, pressed a little: at rest, they settle");
    expect(std::abs((a.position.x + b.position.x) / 2 - 16.5) < .01, "each giving way half");
    // Walking into someone nudges them along, and is slowed by it; they are never walked through.
    a.position = {15.5, 12.5};
    b.position = {16.5, 12.5};
    w.move("a", 1, 0);
    double closest = 1e9;
    for (int i = 0; i < 20; ++i)
    {
        w.tick(.05);
        closest = std::min(closest, distance(a.position, b.position));
    }
    expect(b.position.x > 16.7, "The one walked into is nudged along: " + std::to_string(b.position.x));
    expect(a.position.x < b.position.x && closest > body * .7, "and never walked through");
    expect(a.position.x < 15.5 + 2.6, "Pushing someone along is slower than walking free");
    w.stop("a");
    // A crowd fills a room's floor rather than a single tile.
    a.position = {16.5, 22.5};
    for (int i = 0; i < 8; ++i)
    {
        auto& q = w.addPlayer("crowd" + std::to_string(i), "crowd");
        q.cellId = "exterior";
        q.position = {16.5, 1.5};
    }
    expect(w.interact("a", "door_main", "open").ok, "Crowded destination cannot block a portal");
    expect(a.cellId == "exterior" && near(a.position.y, 1.5), "Crowded portal preserves authored arrival");
    advance(w, 3);
    double tightest = 1e9;
    for (int i = 0; i < 8; ++i)
    {
        const auto& q = *w.entity("crowd" + std::to_string(i));
        tightest = std::min(tightest, distance(q.position, a.position));
        for (int j = i + 1; j < 8; ++j)
            tightest = std::min(tightest, distance(q.position, w.entity("crowd" + std::to_string(j))->position));
    }
    expect(tightest >= body * .8 - 1e-6, "and spreads out round it, none on top of another: " + std::to_string(tightest));
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
void stealthPerception()
{
    World w;
    auto& observer = player(w, "observer");
    auto& source = player(w, "source");
    observer.position = {16.5, 12.5};
    source.position = {26.5, 12.5};
    expect(w.visionClarity("observer", "source") > 0, "Standing wolf is visible along unobstructed room sightline");
    expect(w.setPosture("source", "lying").ok, "Stealth fixture can lie down");
    w.move("source", -1, 0);
    advance(w, .7);
    w.stop("source");
    source.position = {26.5, 12.5};
    expect(source.posture == "crouching", "Lying-to-movement produces persistent stealth posture");
    expect(w.visionClarity("observer", "source") == 0, "Crouching wolf is entirely invisible from afar");
    expect(!hasEntity(w.snapshot("observer"), "source"), "Distant sneaker is removed from observer snapshot");
    expect(w.actions("observer", "source").empty(), "Hidden sneaker has no discoverable target actions");
    expect(!w.interact("observer", "source", "inspect").ok,
           "Forged inspect of hidden sneaker cannot reveal identity or state");
    expect(!w.interact("observer", "source", "speak").ok, "Forged target interaction cannot identify hidden sneaker");
    expect(w.hearingClarity("observer", "source") > 0 && !w.perceive("observer", "source").identifiable,
           "Speaking while hidden may be heard without exposing speaker identity");
    source.position = {22.5, 12.5};
    source.sneakSkill = 0;
    expect(w.visionClarity("observer", "source") > 0, "Novice sneaker becomes visible within close detection range");
    source.sneakSkill = 100;
    expect(w.visionClarity("observer", "source") == 0, "Higher sneak skill shrinks visual detection range");
    source.position = {18.5, 12.5};
    expect(w.visionClarity("observer", "source") > 0 && hasEntity(w.snapshot("observer"), "source"),
           "Even expert sneakers are visible at sufficiently close range");
    const double crouchSpeech = w.hearingClarity("observer", "source", Voice::Speak);
    const double crouchWhisper = w.hearingClarity("observer", "source", Voice::Whisper);
    source.posture = "standing";
    expect(near(w.hearingClarity("observer", "source", Voice::Speak), crouchSpeech) &&
               near(w.hearingClarity("observer", "source", Voice::Whisper), crouchWhisper),
           "Sneak posture does not secretly change deliberate speech or whisper volume");
    observer.position = {22.5, 6.5};
    source.position = {24.5, 6.5};
    source.posture = "crouching";
    expect(w.visionClarity("observer", "source") == 0, "Nearby sneaker remains hidden behind opaque closed door");
    expect(!hasEntity(w.snapshot("observer"), "source"), "Close detection cannot bypass line of sight");

    observer.position = {16.5, 12.5};
    source.position = {19.5, 12.5};
    source.posture = "standing";
    source.sneakSkill = 0;
    w.move("source", 1, 0);
    w.tick(.1);
    const double walking = w.movementAudibility("observer", "source");
    expect(walking > 0, "Nearby moving wolf produces audible movement");
    source.posture = "crouching";
    const double sneaking = w.movementAudibility("observer", "source");
    expect(sneaking < walking, "Crouch movement is quieter than standing movement at same distance");
    source.sneakSkill = 100;
    expect(w.movementAudibility("observer", "source") < sneaking, "Sneak skill further reduces movement audibility");
    source.sneakSkill = 0;
    source.position = {19.5, 12.5};
    observer.hearingSkill = 0;
    const double noviceHearing = w.movementAudibility("observer", "source");
    observer.hearingSkill = 100;
    expect(w.movementAudibility("observer", "source") > noviceHearing,
           "Trained hearing improves detection of quiet moving wolves");
    observer.earHealth = .1;
    expect(w.movementAudibility("observer", "source") < noviceHearing,
           "Ear injury can outweigh trained movement hearing");
    observer.earHealth = 0;
    source.position = {16.7, 12.5};
    expect(w.movementAudibility("observer", "source") == 0, "Total deafness prevents hearing even nearby sneaking");
    observer.earHealth = 1;
    w.stop("source");
    expect(w.movementAudibility("observer", "source") == 0, "Stationary actor does not generate phantom footsteps");
    w.move("source", 1, 0);
    w.tick(.1);
    source.cellId = "exterior";
    expect(w.movementAudibility("observer", "source") == 0, "Movement sounds do not leak across unrelated cells");
    expect(w.movementAudibility("absent", "source") == 0 && w.movementAudibility("observer", "absent") == 0,
           "Unknown listeners and movement sources have no audibility");
}

void posturePortalTransitions()
{
    World w;
    auto& p = player(w);
    p.position = {16.5, 22.5};
    w.setPosture("p", "sitting");
    expect(w.interact("p", "door_main", "open").ok, "Seated wolf can request opening and entering portal");
    expect(w.door("door_main")->open && p.cellId == "tavern", "Portal opens but cannot bypass standing delay");
    w.tick(.1);
    expect(p.cellId == "tavern", "Crossing remains queued during the initial stand");
    advance(w, .9);
    expect(p.cellId == "exterior" && p.posture == "standing", "Queued portal completes once standing is ready");
    expect(near(p.position.x, 16.5) && near(p.position.y, 1.5) && p.path.empty(),
           "Delayed portal preserves arrival anchor and stops");
    w.setPosture("p", "lying");
    expect(w.interact("p", "door_yard", "enter").ok, "Lying wolf may request entering an already open portal");
    w.tick(.1);
    expect(p.cellId == "exterior", "Open portal cannot bypass lying-to-crouch delay");
    advance(w, .7);
    expect(p.cellId == "tavern" && p.posture == "crouching", "Lying portal crossing preserves resulting sneak posture");
    w.setPosture("p", "sitting");
    w.interact("p", "door_main", "enter");
    w.stop("p");
    advance(w, 1);
    expect(p.cellId == "tavern", "Stop cancels queued portal crossing as well as ordinary movement");
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
// Half-step heights, slopes, cliffs, and sight that follows the ground.
void terrainElevation()
{
    World w;
    auto& p = player(w);
    auto* tavern = w.cell("tavern");
    for (int x = 10; x <= 24; ++x)
        *tavern->tile(x, 12) = tileFromGlyph('.');
    auto walk = [&](double from) {
        p.position = {from, 12.5};
        w.move("p", 1, 0);
        advance(w, 1);
        w.stop("p");
        return p.position.x;
    };
    tavern->tile(17, 12)->height = 1;
    expect(walk(16.5) < 17, "A full step without a slope is a ledge");
    tavern->tile(17, 12)->height = .5;
    expect(walk(16.5) > 17, "A half step is walked freely");
    *tavern->tile(17, 12) = tileFromGlyph(':');
    tavern->tile(17, 12)->height = 1;
    expect(walk(16.5) > 17, "A slope lets a full step be walked");
    tavern->tile(18, 12)->height = 2;
    expect(walk(17.5) > 18, "Leaving a slope climbs a further full step");
    tavern->tile(18, 12)->height = 2.5;
    expect(walk(17.5) < 18, "More than a full step is never walkable, even from a slope");
    *tavern->tile(17, 12) = tileFromGlyph('%');
    expect(walk(16.5) < 17, "A cliff face is never walkable, even when level");
    expect(tileFromGlyph('%').terrain == Terrain::Cliff && !tileFromGlyph('%').opaque &&
               tileFromGlyph(':').height == 0,
           "Cliffs are solid but see-through; slopes have no height of their own");

    for (int x = 10; x <= 24; ++x)
        *tavern->tile(x, 12) = tileFromGlyph('.');
    expect(w.lineOfSight("tavern", {12.5, 12.5}, {18.5, 12.5}), "Level ground hides nothing");
    tavern->tile(15, 12)->height = 1;
    expect(!w.lineOfSight("tavern", {12.5, 12.5}, {18.5, 12.5}), "A rise hides the ground behind it");
    expect(w.lineOfSight("tavern", {12.5, 12.5}, {15.5, 12.5}), "The rise itself stays visible");
    tavern->tile(12, 12)->height = 1.5;
    expect(w.lineOfSight("tavern", {12.5, 12.5}, {18.5, 12.5}), "Higher ground sees over the rise");
    tavern->tile(12, 12)->height = 0;
    tavern->tile(18, 12)->height = 2;
    expect(w.lineOfSight("tavern", {12.5, 12.5}, {18.5, 12.5}), "A peak behind a lower rise shows above it");
    tavern->tile(15, 12)->height = 0;
    *tavern->tile(16, 12) = tileFromGlyph('%');
    tavern->tile(18, 12)->height = 0;
    expect(w.lineOfSight("tavern", {12.5, 12.5}, {18.5, 12.5}), "A cliff hides only by its height");
    tavern->tile(16, 12)->height = 3;
    for (int x = 17; x <= 20; ++x)
        tavern->tile(x, 12)->height = 3;
    expect(!w.lineOfSight("tavern", {12.5, 12.5}, {19.5, 12.5}), "A plateau top is hidden from below its cliff");
    expect(w.lineOfSight("tavern", {17.5, 12.5}, {10.5, 12.5}), "From a cliff's edge the lowland is in view");
    expect(!w.lineOfSight("tavern", {19.5, 12.5}, {13.5, 12.5}), "Stepping back from the edge hides the ground below");

    // Catalog tiles: what stands on the ground hides by its stature, not by raising the ground.
    for (int x = 10; x <= 24; ++x)
        *tavern->tile(x, 12) = tileFromGlyph('.');
    *tavern->tile(15, 12) = tileFromGlyph('P');
    expect(!w.lineOfSight("tavern", {12.5, 12.5}, {18.5, 12.5}), "A pine hides what stands behind it");
    expect(tavern->tile(15, 12)->height == 0 && tavern->tile(15, 12)->solid, "A pine is solid but keeps its ground");
    *tavern->tile(15, 12) = tileFromGlyph('T');
    expect(w.lineOfSight("tavern", {12.5, 12.5}, {18.5, 12.5}), "A table is too low to hide anything");
    *tavern->tile(15, 12) = tileFromGlyph('.');
    tavern->tile(15, 12)->height = 1;
    *tavern->tile(18, 12) = tileFromGlyph('P');
    expect(w.lineOfSight("tavern", {12.5, 12.5}, {18.5, 12.5}), "A treetop shows over a rise that hides its foot");
    *tavern->tile(18, 12) = tileFromGlyph('.');
    expect(!w.lineOfSight("tavern", {12.5, 12.5}, {18.5, 12.5}), "Without the tree the rise hides that ground");
    tavern->tile(15, 12)->height = 0;
    *tavern->tile(16, 12) = tileFromGlyph('b');
    expect(walk(15.5) > 16, "A bed is walkable");
    *tavern->tile(17, 12) = tileFromGlyph('k');
    expect(walk(16.5) < 17, "A bookshelf is not");
    for (const auto& info : terrainCatalog())
        expect(terrainInfo(info.code) == &info && tileFromGlyph(info.code).glyph == info.code &&
                   info.glyph != u'W' && (!info.opaque || info.solid),
               "Every catalog tile is found by its code and is internally consistent");
    expect(!terrainInfo('?') && !terrainInfo(' ') && !terrainInfo(char(0xC3)), "Unknown codes are not tiles");

    // A goal walled off from the walker has no route, and the search says so without exploring everything.
    for (int x = 10; x <= 24; ++x)
        *tavern->tile(x, 12) = tileFromGlyph('.');
    p.cellId = "tavern";
    p.position = {12.5, 12.5};
    for (int y = 10; y <= 14; ++y)
        for (int x = 18; x <= 22; ++x)
            if (y == 10 || y == 14 || x == 18 || x == 22)
                *tavern->tile(x, y) = tileFromGlyph('#');
    expect(!w.moveTo("p", 20.5, 12.5).ok && p.path.empty(), "A walled-off goal has no route");
    expect(w.moveTo("p", 16.5, 12.5).ok && !p.path.empty(), "Open ground beside it still does");
    tavern->tile(18, 12)->height = 0;
    *tavern->tile(18, 12) = tileFromGlyph('.');
    expect(w.moveTo("p", 20.5, 12.5).ok && !p.path.empty(), "Opening the wall opens the route (regions follow edits)");

    Weather parsed = Weather::Clear;
    expect(parseWeather("sandstorm", parsed) && parsed == Weather::Sandstorm &&
               std::string(weatherName(Weather::Storm)) == "storm" && !parseWeather("hail", parsed) &&
               parsed == Weather::Sandstorm,
           "Weather names round-trip and unknown names are refused");
    w.setWeather("exterior", Weather::Clear);
    const double clearSight = w.environmentAt("exterior").sight;
    w.setWeather("exterior", Weather::Storm);
    const auto storm = w.environmentAt("exterior");
    w.setWeather("exterior", Weather::Sandstorm);
    const auto dust = w.environmentAt("exterior");
    expect(storm.sight < clearSight && storm.hearing < 1 && dust.sight < storm.sight && dust.scent < storm.scent,
           "Storms and sandstorms dull the senses, sand most of all");
    World restarted;
    expect(restarted.restore(w.save()).ok && restarted.cell("exterior")->weather == Weather::Sandstorm,
           "New weather kinds survive a restart");
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
    p.sneakSkill = 72;
    p.hearingSkill = 38;
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
    expect(near(returned->sneakSkill, 72) && near(returned->hearingSkill, 38),
           "Sneak and hearing training survive restart independently of sense health");
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
    bad.players[0].sneakSkill = std::numeric_limits<double>::quiet_NaN();
    reject(bad, "Reject nonfinite saved sneak skill");
    bad = saved;
    bad.players[0].sneakSkill = -1;
    reject(bad, "Reject negative saved sneak skill");
    bad = saved;
    bad.players[0].sneakSkill = 101;
    reject(bad, "Reject saved sneak skill above supported range");
    bad = saved;
    bad.players[0].hearingSkill = std::numeric_limits<double>::infinity();
    reject(bad, "Reject nonfinite saved hearing skill");
    bad = saved;
    bad.players[0].hearingSkill = -1;
    reject(bad, "Reject negative saved hearing skill");
    bad = saved;
    bad.players[0].hearingSkill = 101;
    reject(bad, "Reject saved hearing skill above supported range");
    bad = saved;
    bad.players[0].turnTarget = std::numeric_limits<double>::quiet_NaN();
    reject(bad, "Reject nonfinite saved turn target");
    bad = saved;
    bad.players[0].postureRemaining = std::numeric_limits<double>::infinity();
    reject(bad, "Reject nonfinite saved posture timer");
    bad = saved;
    bad.players[0].posture = "rising";
    bad.players[0].postureRemaining = .4;
    reject(bad, "Reject transitional posture without a valid final target");
    bad = saved;
    bad.players[0].postureTarget = "standing";
    reject(bad, "Reject stable posture carrying a stale transition target");
    bad = saved;
    bad.players[0].posture = "invisible";
    reject(bad, "Reject unsupported saved posture");
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
void movementPersistence()
{
    World w;
    auto& p = player(w);
    expect(p.sneakSkill == 0 && p.hearingSkill == 0, "New or legacy-default actors start with untrained skills");
    p.sneakSkill = 100;
    p.hearingSkill = 100;
    w.face("p", p.position.x, p.position.y - 4);
    w.tick(.1);
    expect(p.turning, "Persistence fixture contains a pending gradual turn");
    auto saved = w.save();
    expect(!saved.players[0].turning, "Manual turning intent is not persisted");
    World fresh;
    expect(fresh.restore(saved).ok, "Maximum supported skills survive a valid restore");
    const double restoredFacing = fresh.entity("p")->facing;
    advance(fresh, 1);
    expect(near(fresh.entity("p")->facing, restoredFacing), "Restart cannot resume an old unfinished manual turn");
    w.setPosture("p", "lying");
    w.move("p", 1, 0);
    w.tick(.1);
    expect(p.posture == "rising", "Persistence fixture contains an in-progress posture transition");
    saved = w.save();
    expect(saved.players[0].posture == "crouching" && saved.players[0].postureRemaining == 0 &&
               saved.players[0].postureTarget.empty(),
           "Save resolves rising to stable target posture without persisting a timer");
    expect(fresh.restore(saved).ok, "Normalized crouch state restores safely");
    const Vec2 position = fresh.entity("p")->position;
    advance(fresh, 1);
    expect(fresh.entity("p")->posture == "crouching" && near(fresh.entity("p")->position.x, position.x) &&
               near(fresh.entity("p")->position.y, position.y),
           "Restarted crouch has no queued travel or half-finished transition");
}

// Scent tests isolate authored residents so anonymous aggregate cues can be
// asserted without accidentally relying on a demo NPC's position or routine.
void isolateScentResidents(World& w)
{
    quiet(w);
    for (const auto& entry : w.entities())
        if (entry.second.npc)
        {
            auto* resident = w.entity(entry.first);
            resident->cellId = "loft";
            resident->position = {8.5, 6.5};
        }
}
void scentWindAndWeather()
{
    World w;
    auto& observer = player(w, "observer");
    auto& source = player(w, "source");
    isolateScentResidents(w);
    observer.cellId = source.cellId = "exterior";
    observer.position = {22.5, 12.5};
    source.position = {14.5, 12.5};
    w.setWeather("exterior", Weather::Clear);
    const double pi = std::acos(-1.0);
    expect(w.setWind("exterior", 0, .5).ok, "Outdoor wind accepts eastward air flow");
    const double downwind = w.scentClarity("observer", "source");
    expect(downwind > 0, "Downwind observer detects an upwind wolf beyond calm scent range");
    expect(w.scentClarity("source", "observer") < downwind,
           "Scent transport follows wind direction instead of symmetric distance alone");
    expect(w.setWind("exterior", pi, .5).ok, "Wind direction can reverse");
    expect(w.scentClarity("source", "observer") > w.scentClarity("observer", "source"),
           "Reversing wind reverses which wolf has the scent advantage");
    w.setWind("exterior", pi / 2, .5);
    expect(w.scentClarity("observer", "source") < downwind,
           "Crosswind carries less useful scent than direct downwind flow");
    w.setWind("exterior", 0, 0);
    expect(w.scentClarity("observer", "source") == 0, "Calm air does not grant long-distance smell");
    source.position = {21.0, 12.5};
    expect(w.scentClarity("observer", "source") > 0, "Nearby wolves remain scentable in still air");
    const auto calm = w.scentCues("observer");
    observer.eyeHealth = 0;
    const auto blindCalm = w.scentCues("observer");
    expect(!blindCalm.empty() && !blindCalm.front().windborne, "Calm proximity cue is not labeled windborne");
    expect(calm.empty(), "A clearly visible source does not duplicate itself as an unknown scent cue");
    source.position = {14.5, 12.5};
    w.setWind("exterior", 0, .5);
    w.setWeather("exterior", Weather::Rain);
    const double rain = w.scentClarity("observer", "source");
    expect(rain < downwind, "Rain attenuates airborne scent compared with clear weather");
    w.setWeather("exterior", Weather::Snow);
    expect(w.scentClarity("observer", "source") < downwind,
           "Snow attenuates airborne scent compared with clear weather");
    w.setWeather("exterior", Weather::Clear);
    observer.scentSkill = 100;
    const double trained = w.scentClarity("observer", "source");
    expect(trained > downwind, "Scent training improves a marginal downwind detection");
    observer.noseHealth = .1;
    expect(w.scentClarity("observer", "source") < downwind, "Nose injury can outweigh trained scent ability");
    observer.noseHealth = 0;
    source.position = {22.6, 12.5};
    expect(w.scentClarity("observer", "source") == 0 && w.scentCues("observer").empty(),
           "Complete loss of smell suppresses even adjacent body scent");
    observer.noseHealth = 1;
    observer.smell = 0;
    expect(w.scentClarity("observer", "source") == 0, "Zero smell sensitivity disables scent perception");
    observer.smell = 1;
    expect(w.scentClarity("observer", "observer") == 0, "A wolf never receives a cue for its own scent");
    expect(w.scentClarity("absent", "source") == 0 && w.scentClarity("observer", "absent") == 0,
           "Unknown scent observers and sources produce no detection");
    source.position = {1e300, 12.5};
    expect(w.scentClarity("observer", "source") == 0,
           "Huge finite scent coordinates are rejected before air-grid conversion");
    source.position = {std::numeric_limits<double>::quiet_NaN(), 12.5};
    expect(w.scentClarity("observer", "source") == 0, "Nonfinite scent coordinates cannot corrupt perception");
    source.cellId = "tavern";
    expect(w.scentClarity("observer", "source") == 0, "Live body scent stays inside its stored local cell");
}
void scentAirPathsAndPrivacy()
{
    World w;
    auto& observer = player(w, "observer");
    auto& source = player(w, "source");
    isolateScentResidents(w);
    observer.position = {22.5, 6.5};
    source.position = {24.5, 6.5};
    expect(w.scentClarity("observer", "source") == 0, "Closed sealed pantry prevents airborne body scent");
    expect(w.scentCues("observer").empty(), "Sealed air barrier cannot leak a scent direction cue");
    expect(w.interact("observer", "door_pantry", "open").ok, "Scent fixture opens the actual air barrier");
    expect(w.scentClarity("observer", "source") > 0, "Opening an air barrier admits nearby body scent");
    w.interact("observer", "door_pantry", "close");
    expect(w.scentClarity("observer", "source") == 0, "Closing an air barrier immediately cuts its live scent route");

    observer.position = {16.5, 12.5};
    source.position = {18.5, 12.5};
    observer.smell = 2;
    auto* wall = w.cell("tavern")->tile(17, 12);
    wall->glyph = '#';
    wall->terrain = Terrain::Wall;
    wall->solid = wall->opaque = true;
    expect(!w.lineOfSight("tavern", observer.position, source.position), "Corner fixture blocks direct visual sight");
    expect(w.scentClarity("observer", "source") > 0, "Scent travels around an obstacle through a connected air path");
    expect(!hasEntity(w.snapshot("observer"), "source"), "Scent around an obstacle never supplies a visual wolf token");
    expect(w.actions("observer", "source").empty() && !w.interact("observer", "source", "inspect").ok,
           "Knowing an anonymous scent direction never unlocks target identity or inspection");
    expect(!w.perceive("observer", "source").identifiable,
           "Scent detection cannot upgrade an unseen speaker to an identified character");
    const double standingScent = w.scentClarity("observer", "source");
    source.posture = "crouching";
    source.sneakSkill = 100;
    expect(near(w.scentClarity("observer", "source"), standingScent),
           "An expert sneak remains equally scentable; stealth does not erase body odor");

    observer.eyeHealth = 0;
    observer.earHealth = 0;
    const auto knowledgeBefore = w.memories("observer");
    const auto blind = w.snapshot("observer");
    expect(!blind.scentCues.empty() && !blind.movementHeard,
           "A blind deaf wolf can receive scent without visual or movement-sound detection");
    expect(!hasEntity(blind, "source") && blind.entities.size() == 1,
           "Scent-only snapshot includes the observer but no hidden character record");
    bool unchanged = w.memories("observer").size() == knowledgeBefore.size();
    for (const auto& entry : knowledgeBefore)
    {
        const auto& after = w.memories("observer").at(entry.first);
        unchanged = unchanged && after.knowledge == entry.second.knowledge && after.glyphs == entry.second.glyphs &&
                    after.observed == entry.second.observed;
    }
    expect(unchanged, "Receiving scent alone cannot promote cells or terrain into visual map memory");
    source.position = {16.5, 13.5};
    source.velocity = {.2, 0};
    source.posture = "standing";
    observer.noseHealth = 0;
    observer.earHealth = 1;
    expect(w.snapshot("observer").movementHeard && w.snapshot("observer").scentCues.empty(),
           "An anosmic blind wolf can still hear nearby movement independently of scent");
    source.velocity = {};
    expect(!w.snapshot("observer").movementHeard, "Stationary wolves do not leave stale movement-heard flags");

    World sealed;
    auto& outside = player(sealed, "outside");
    auto& inside = player(sealed, "inside");
    isolateScentResidents(sealed);
    outside.position = {16.5, 11.5};
    inside.position = {18.5, 12.5};
    outside.smell = 2;
    for (int y = 11; y <= 13; ++y)
        for (int x = 17; x <= 19; ++x)
            if ((x != 18 || y != 12) && (x != 17 || y != 11))
            {
                auto* barrier = sealed.cell("tavern")->tile(x, y);
                barrier->terrain = Terrain::Wall;
                barrier->solid = barrier->opaque = true;
            }
    expect(sealed.scentClarity("outside", "inside") == 0,
           "Air cannot leak diagonally through the touching corners of a sealed enclosure");
    auto* opening = sealed.cell("tavern")->tile(17, 12);
    *opening = Tile{};
    expect(sealed.scentClarity("outside", "inside") > 0,
           "Opening a real cardinal air connection restores scent into an enclosure");
    const double indoorScent = sealed.scentClarity("outside", "inside");
    sealed.setWeather("tavern", Weather::Rain);
    expect(near(sealed.scentClarity("outside", "inside"), indoorScent),
           "Outdoor rain attenuation does not reduce scent inside a sheltered room");
}
void anonymousScentSectors()
{
    World w;
    auto& observer = player(w, "observer");
    auto& source = player(w, "source");
    isolateScentResidents(w);
    observer.cellId = source.cellId = "exterior";
    observer.position = {22.5, 12.5};
    source.position = {14.5, 12.5};
    observer.eyeHealth = 0;
    w.setWeather("exterior", Weather::Clear);
    w.setWind("exterior", 0, .5);
    auto cues = w.scentCues("observer");
    expect(cues.size() == 1 && cues[0].sector == 4 && cues[0].windborne,
           "Eastward air flow reveals a broad west/upwind source sector, not the air-flow heading");
    expect(cues[0].strength >= 1 && cues[0].strength <= 3, "Scent intensity is quantized to three coarse levels");
    auto& another = player(w, "another");
    another.cellId = "exterior";
    another.position = {13.5, 12.5};
    cues = w.scentCues("observer");
    expect(cues.size() == 1 && cues[0].sector == 4,
           "Multiple hidden wolves in one direction aggregate into one cue without an exact count");
    expect(w.snapshot("observer").scentCues.size() == 1,
           "Observer snapshot uses the same anonymous aggregated scent projection");
    w.removePlayer("another");
    w.setWind("exterior", 0, 0);
    const double pi = std::acos(-1.0);
    for (int sector = 0; sector < 8; ++sector)
    {
        const double angle = sector * pi / 4;
        source.position = {observer.position.x + 1.4 * std::cos(angle), observer.position.y + 1.4 * std::sin(angle)};
        cues = w.scentCues("observer");
        expect(cues.size() == 1 && cues[0].sector == sector,
               "Eight coarse scent sectors preserve east-zero, clockwise screen-space compass convention");
    }
    observer.eyeHealth = 1;
    expect(w.scentCues("observer").empty(), "A source stops being an unknown scent marker once visually perceived");
    w.removePlayer("source");
    expect(w.scentCues("observer").empty(),
           "Removing a wolf removes its body scent without inventing persistent tracks");
    expect(w.scentCues("absent").empty(), "Unknown observers receive no scent projection");
}
void windAndScentPersistence()
{
    World w;
    auto& p = player(w);
    p.smell = .8;
    p.noseHealth = .7;
    p.scentSkill = 64;
    const double pi = std::acos(-1.0);
    expect(w.setWind("exterior", pi / 3, .6, true).ok, "Outdoor variable wind has an explicit persistent base");
    expect(!w.setWind("tavern", 0, .5).ok, "Sheltered indoor cell rejects nonzero authored wind");
    expect(w.setWind("tavern", 0, 0).ok && w.windAt("tavern").strength == 0,
           "Sheltered indoor wind remains calm even when the outdoor weather blows");
    const auto previous = w.windAt("exterior");
    w.tick(.01);
    const auto next = w.windAt("exterior");
    expect(std::abs(std::remainder(next.direction - previous.direction, 2 * pi)) < .02 &&
               std::abs(next.strength - previous.strength) < .02,
           "Weather-clock wind gusts evolve smoothly instead of jumping at frame boundaries");
    advance(w, 3.7);
    const auto effective = w.windAt("exterior");
    const auto saved = w.save();
    expect(saved.winds.count("exterior") && near(saved.winds.at("exterior").direction, pi / 3) &&
               near(saved.winds.at("exterior").strength, .6) && saved.winds.at("exterior").variable,
           "Persistence stores authored base wind, not a transient gust sample");
    World restored;
    expect(restored.restore(saved).ok, "Wind and scent senses restore alongside existing state");
    const auto* returned = restored.entity("p");
    expect(near(returned->smell, .8) && near(returned->noseHealth, .7) && near(returned->scentSkill, 64),
           "Smell sensitivity, nose health, and scent training survive restart independently");
    const auto restartedWind = restored.windAt("exterior");
    expect(near(restartedWind.direction, effective.direction, 1e-10) &&
               near(restartedWind.strength, effective.strength, 1e-10),
           "Saved simulation clock restores the same live gust phase");
    auto legacy = saved;
    legacy.winds.clear();
    legacy.players[0].smell = 1;
    legacy.players[0].noseHealth = 1;
    legacy.players[0].scentSkill = 0;
    expect(restored.restore(legacy).ok, "Legacy save without wind entries remains accepted");
    expect(near(restored.entity("p")->smell, 1) && near(restored.entity("p")->noseHealth, 1) &&
               restored.entity("p")->scentSkill == 0,
           "Legacy-default actors retain healthy smell and untrained scent skill");

    expect(restored.restore(saved).ok, "Validation fixture returns to authored wind and scent state");
    auto reject = [&](PersistedWorld bad, const std::string& reason) {
        expect(!restored.restore(bad).ok, reason);
        const auto still = restored.windAt("exterior");
        expect(near(still.direction, effective.direction, 1e-10) && near(still.strength, effective.strength, 1e-10) &&
                   restored.entity("p")->scentSkill == 64 && restored.time() == saved.time,
               "Rejected wind or scent restore is atomic for actor state, base wind, and gust clock");
    };
    auto bad = saved;
    bad.winds["exterior"].direction = std::numeric_limits<double>::quiet_NaN();
    reject(bad, "Reject nonfinite persisted wind heading");
    bad = saved;
    bad.winds["exterior"].strength = std::numeric_limits<double>::infinity();
    reject(bad, "Reject nonfinite persisted wind strength");
    bad = saved;
    bad.winds["exterior"].strength = -1;
    reject(bad, "Reject negative persisted wind strength");
    bad = saved;
    bad.winds["exterior"].strength = 1.1;
    reject(bad, "Reject persisted wind above its normalized range");
    bad = saved;
    bad.winds["tavern"].strength = .5;
    reject(bad, "Reject outdoor-strength wind injected into a sheltered room");
    bad = saved;
    bad.winds["nonexistent"] = {};
    reject(bad, "Reject wind records for unknown cells");
    bad = saved;
    bad.players[0].smell = -1;
    reject(bad, "Reject negative persisted smell sensitivity");
    bad = saved;
    bad.players[0].smell = std::numeric_limits<double>::quiet_NaN();
    reject(bad, "Reject nonfinite persisted smell sensitivity");
    bad = saved;
    bad.players[0].noseHealth = 1.1;
    reject(bad, "Reject persisted nose health outside the injury fraction range");
    bad = saved;
    bad.players[0].noseHealth = -1;
    reject(bad, "Reject negative persisted nose health");
    bad = saved;
    bad.players[0].noseHealth = std::numeric_limits<double>::quiet_NaN();
    reject(bad, "Reject nonfinite persisted nose health");
    bad = saved;
    bad.players[0].scentSkill = std::numeric_limits<double>::infinity();
    reject(bad, "Reject nonfinite persisted scent training");
    bad = saved;
    bad.players[0].scentSkill = -1;
    reject(bad, "Reject negative persisted scent skill");
    bad = saved;
    bad.players[0].scentSkill = 101;
    reject(bad, "Reject persisted scent skill above the supported range");
    expect(!w.setWind("absent", 0, .5).ok, "Unknown cell cannot receive wind edits");
    expect(!w.setWind("exterior", std::numeric_limits<double>::quiet_NaN(), .5).ok,
           "Nonfinite live wind heading is rejected");
    expect(!w.setWind("exterior", 0, std::numeric_limits<double>::infinity()).ok,
           "Nonfinite live wind strength is rejected");
    expect(!w.setWind("exterior", 0, -.1).ok && !w.setWind("exterior", 0, 1.1).ok,
           "Live wind strengths outside normalized range are rejected");
    expect(near(w.cell("exterior")->wind.direction, pi / 3) && near(w.cell("exterior")->wind.strength, .6),
           "Rejected live wind edits preserve the valid authored wind");
    expect(w.windAt("absent").strength == 0, "Unknown cells return no measurable wind");
    expect(w.setWind("exterior", pi * 6 + .4, 1).ok && near(w.windAt("exterior").direction, .4),
           "Finite wind headings normalize around the compass without changing direction");
    const auto constant = w.windAt("exterior");
    advance(w, 1);
    expect(near(w.windAt("exterior").direction, constant.direction, 1e-10) &&
               near(w.windAt("exterior").strength, constant.strength, 1e-10),
           "Nonvariable wind does not acquire unintended weather-clock gusts");
}
// Where flat tiles of 4, 4.5 and 5 meet at a corner (as at Warden Order's north edge), a footprint reaching from
// the 4.5 onto both the 4 and the 5 must be refused: once its centre was on the 4, no step was allowed from there,
// not even back, and residents walked into it and stayed for good, asking for routes every half second.
void cornerTrap()
{
    struct Corner final : step::Grid
    {
        bool ground(int x, int y, step::Ground& out) const override
        {
            if (x < 0 || y < 0 || x > 1 || y > 1)
                return false;
            const double heights[2][2] = {{4.0, 4.0}, {4.5, 5.0}};   // [y][x]
            out = {false, heights[y][x], false, 1};
            return true;
        }
        bool closedDoor(int, int) const override { return false; }
    } grid;
    const step::Ground middling{false, 4.5, false, 1}, low{false, 4.0, false, 1};
    const step::Point trap{1.006, .994};
    expect(!step::passable(grid, trap, &middling), "A spot that can be stepped into but never out of is refused");
    expect(step::passable(grid, {1.006, .9}, &low) && step::passable(grid, {.9, 1.1}, &middling),
           "Ground beside it, standing on one height, is fine");
    bool blocked = false;
    const auto at = step::slide(grid, {.9, 1.1}, trap, &middling, blocked);
    expect(blocked && step::passable(grid, at, &middling), "Walking toward it stops short, somewhere it can walk on from");
}

void schedules()
{
    World w;
    const auto start = w.entity("npc_scout")->position;
    advance(w, 68);
    const auto* scout = w.entity("npc_scout");
    expect(scout->cellId == "exterior" || scout->position.y > start.y + 1,
           "Scheduled resident walks toward next routine");
    const auto* life = w.society().resident("npc_scout");
    expect(life && life->task == "paid work" && life->goalCell == "exterior" &&
               scout->activity == life->task + " — " + life->reason,
           "The authoritative daytime work task and its reason are exposed as current activity");
    // The same with routes searched on the route thread (doc 31, Phase 5): the resident waits a tick or two for it.
    World routed;
    routed.setRoutesOffThread(true);
    advance(routed, 68);
    const auto* walker = routed.entity("npc_scout");
    expect(walker->cellId == "exterior" || walker->position.y > start.y + 1,
           "A resident whose route is searched off the game thread still walks toward the next routine");
    World party;
    party.entity("npc_scout")->leaderId = "p";
    const auto held = party.entity("npc_scout")->position;
    advance(party, 68);
    expect(near(party.entity("npc_scout")->position.x, held.x) && near(party.entity("npc_scout")->position.y, held.y),
           "Recruitment suspends autonomous schedule");
}
} // namespace
// Dungeon Master kill and resurrect: the dead lie still and cannot move until brought back.
void deathAndResurrection()
{
    World w;
    auto& p = player(w);
    expect(w.move("p", 1, 0).ok, "Moving before death");
    expect(w.setDead("p", true).ok, "Kill a character");
    expect(p.dead && p.posture == "lying" && p.state == "dead", "The dead lie down");
    const Vec2 fell = p.position;
    w.tick(.5);
    expect(near(p.position.x, fell.x) && near(p.position.y, fell.y), "The dead do not keep moving");
    expect(!w.move("p", 1, 0).ok && !w.moveTo("p", fell.x + 2, fell.y).ok, "The dead cannot move");
    expect(!w.setDead("p", true).ok, "Killing the dead is refused");
    expect(w.setDead("p", false).ok && !p.dead && p.posture == "standing", "Resurrect");
    expect(!w.setDead("p", false).ok, "Resurrecting the living is refused");
    expect(w.move("p", 1, 0).ok, "The living move again");
    expect(!w.setDead("nobody", true).ok, "Unknown characters are refused");
}

// Commands name an actor by ID; an ID nobody has moves nothing and makes nobody.
void forgedActors()
{
    World w;
    player(w);
    const auto count = w.entities().size();
    for (const auto* id : {"player-forged", "npc_nobody", ""})
        expect(!w.move(id, 1, 0).ok, std::string("Movement for an unknown actor is refused: ") + id);
    w.tick(.5);
    expect(w.entities().size() == count && !w.entity("player-forged"), "And no actor is made by it");
}

// The herb patch is gathered from beside it, out under the sky: not through a roof, and not from afar.
void herbPatchReach()
{
    World w;
    auto& p = player(w, "player-herbs");
    const auto patch = w.herbPatchPosition();
    const auto herbs = [&] { return Society::stock(*w.society().account("player-herbs"), "herbs"); };
    const int before = herbs();
    p.cellId = "tavern";
    p.position = patch;
    expect(!w.cell("tavern")->outdoors && w.herbPatchCell() != "tavern", "The tavern is a roofed room apart from the patch");
    expect(!w.gather("player-herbs").ok && herbs() == before, "Nothing is gathered from the same spot indoors");
    p.cellId = w.herbPatchCell();
    p.position = {patch.x, patch.y + 1.8};
    expect(!w.gather("player-herbs").ok && herbs() == before, "Nothing is gathered from beyond reach");
    p.position = {patch.x, patch.y + 1.5};
    expect(w.gather("player-herbs").ok && herbs() > before, "Gathered from within reach");
}

// lineOfSight() checks each tile a ray crosses once rather than every sample along it. This is the plain version, a
// check at every sample, and the two must agree on every ray over rough ground with walls, trees and closed doors.
bool sampledSight(const World& w, const Cell& c, Vec2 from, Vec2 to)
{
    constexpr double EyeHeight = 0.8, SightTarget = 0.5;   // As in RatwWorld.cpp.
    const auto* fromTile = c.tile(int(std::floor(from.x)), int(std::floor(from.y)));
    const auto* toTile = c.tile(int(std::floor(to.x)), int(std::floor(to.y)));
    const double eye = (fromTile ? fromTile->height : 0.0) + EyeHeight;
    const double target = toTile ? toTile->height + std::max(SightTarget, toTile->stature) : SightTarget;
    const int count = std::max(1, int(std::ceil(std::hypot(to.x - from.x, to.y - from.y) / 0.12)));
    for (int i = 1; i < count; ++i)
    {
        const double f = double(i) / count;
        const Vec2 p{from.x + (to.x - from.x) * f, from.y + (to.y - from.y) * f};
        const int x = int(std::floor(p.x)), y = int(std::floor(p.y));
        if (x == int(std::floor(to.x)) && y == int(std::floor(to.y)))
            continue;
        const auto* t = c.tile(x, y);
        if (!t || t->opaque)
            return false;
        if (t != fromTile && t->height + t->stature > eye + (target - eye) * f + 1e-6)
            return false;
        for (const auto& [id, d] : w.doors())
            if (d.cellId == c.id && !d.passage && id.rfind("stairs_", 0) != 0 && !d.open &&
                int(std::floor(d.position.x)) == x && int(std::floor(d.position.y)) == y)
                return false;
    }
    return true;
}
void sightRaysMatchSampling()
{
    World w;
    auto* tavern = w.cell("tavern");
    std::mt19937 random(20260927);
    std::uniform_real_distribution<double> unit(0, 1);
    for (int y = 1; y < tavern->height - 1; ++y)
        for (int x = 1; x < tavern->width - 1; ++x)
        {
            auto* t = tavern->tile(x, y);
            if (t->opaque || t->solid)
                continue;                           // Keep the tavern's walls and fixtures where they are.
            const double roll = unit(random);
            if (roll < .04)
                *t = tileFromGlyph('P');
            else if (roll < .07)
                *t = tileFromGlyph('#');
            else
                t->height = std::floor(unit(random) * 7) * .5;
        }
    int agreed = 0, visible = 0;
    for (int ray = 0; ray < 40000; ++ray)
    {
        // Tile centres (as views use), arbitrary points, and straight and diagonal lines.
        const bool centres = ray % 3 == 0;
        auto point = [&] {
            const double x = unit(random) * tavern->width, y = unit(random) * tavern->height;
            return centres ? Vec2{std::floor(x) + .5, std::floor(y) + .5} : Vec2{x, y};
        };
        const Vec2 from = point();
        Vec2 to = point();
        if (ray % 7 == 1)
            to.y = from.y;
        else if (ray % 7 == 2)
            to.x = from.x;
        else if (ray % 7 == 3)
        {
            const double run = std::min(tavern->width - from.x, tavern->height - from.y) * unit(random);
            to = {from.x + run, from.y + run};
        }
        const bool fast = w.lineOfSight("tavern", from, to), plain = sampledSight(w, *tavern, from, to);
        if (fast != plain)
            throw std::runtime_error("Sight rays disagree from (" + std::to_string(from.x) + "," + std::to_string(from.y) +
                                     ") to (" + std::to_string(to.x) + "," + std::to_string(to.y) + ")");
        ++agreed;
        visible += fast;
    }
    expect(agreed == 40000, "Every sight ray agrees with checking each sample");
    expect(visible > 4000 && visible < 36000, "The rays tested both clear and blocked sight");
}

// Several players' sight worked out together (prepareViews, on several threads) is exactly what each would have seen.
void preparedViewsMatch()
{
    const auto setUp = [](World& w) {
        quiet(w);
        const std::vector<std::pair<std::string, Vec2>> spots{{"a", {4.5, 4.5}}, {"b", {12.5, 12.5}}, {"c", {20.5, 8.5}},
                                                               {"d", {27.5, 18.5}}, {"e", {6.5, 19.5}}};
        for (const auto& [id, at] : spots)
        {
            w.addPlayer(id, id);
            w.entity(id)->position = at;
        }
    };
    World plain, prepared;
    setUp(plain);
    setUp(prepared);
    for (int step = 0; step < 6; ++step)
    {
        for (auto* w : {&plain, &prepared})
            for (const auto* id : {"a", "b", "c", "d", "e"})
                w->entity(id)->position.x += .37;
        prepared.prepareViews({"a", "b", "c", "d", "e", "nobody"});
        for (const auto* id : {"a", "b", "c", "d", "e"})
        {
            const auto one = plain.snapshot(id), other = prepared.snapshot(id);
            expect(one.visibleTiles == other.visibleTiles && one.rememberedTiles == other.rememberedTiles,
                   std::string("Prepared sight is the same sight for ") + id);
            expect(one.entities.size() == other.entities.size(), "and shows the same people");
        }
    }
    for (const auto* id : {"a", "b", "c", "d", "e"})
        for (const auto& [cell, memory] : plain.memories(id))
            expect(prepared.memories(id).at(cell).observed == memory.observed, "and leaves the same map memory");
}

bool samePath(const std::vector<Vec2>& a, const std::vector<Vec2>& b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].x != b[i].x || a[i].y != b[i].y)
            return false;
    return true;
}
// A route asked for again from the same place to the same place is the one found before; a change to the ground
// is never answered from memory.
void pathsAreRemembered()
{
    World w;
    quiet(w);
    w.addPlayer("p", "p");
    const auto walkFromCorner = [&] {
        w.stop("p");
        w.entity("p")->position = {5.5, 5.5};
        expect(w.moveTo("p", 20.5, 12.5).ok, "A route across the tavern");
        return w.entity("p")->path;
    };
    const auto first = walkFromCorner();
    const auto [hits, misses] = w.pathCacheStats();
    const auto again = walkFromCorner();
    expect(samePath(again, first), "The same route again");
    expect(w.pathCacheStats().first > hits && w.pathCacheStats().second == misses, "found without searching");
    auto* tavern = w.cell("tavern");
    for (int y = 1; y < tavern->height - 1; ++y)
        if (!tavern->tile(12, y)->solid && y != 12)
            *tavern->tile(12, y) = tileFromGlyph('#');       // A wall across the room, leaving one gap.
    const auto changed = walkFromCorner();
    expect(w.pathCacheStats().second > misses, "New ground means a new search");
    expect(!samePath(changed, first), "and a new route around the wall");
    // A Chapter's building in the gap (Docs/Design/32): the routes kept are forgotten, and there is no way through.
    w.obstacles["tavern"].insert({12, 12});
    w.stop("p");
    w.entity("p")->position = {5.5, 5.5};
    expect(!w.moveTo("p", 20.5, 12.5).ok, "a building closes the gap");
    w.obstacles["tavern"].clear();
    expect(samePath(walkFromCorner(), changed), "and gone, the way is open again");
    // One standing on the way: the route goes round it, not through it.
    w.obstacles["tavern"].insert({8, 8});
    w.obstacles["tavern"].insert({9, 9});
    for (const auto& at : walkFromCorner())
        expect(!w.obstacles["tavern"].count({int(std::floor(at.x)), int(std::floor(at.y))}), "the route steps round the building");
    w.obstacles["tavern"].clear();
}

// The event log: what happened, who, to whom, where; economy entries alongside the world's own; taken once.
void eventLog()
{
    World w;
    quiet(w);
    w.addPlayer("player-log", "Logger");
    w.takeEvents();                                           // Whatever setting up the world recorded.
    auto* customer = w.entity("player-log");
    customer->cellId = "tavern";
    customer->position = {9.5, 7.5};
    w.entity("npc_keeper")->position = {9.5, 6.5};
    expect(w.trade("player-log", "npc_keeper", "meal", 1, true).ok, "A purchase");
    expect(w.setDead("npc_keeper", true).ok && w.setDead("npc_keeper", false).ok, "A death and a revival");
    w.recordEvent({"arrival", "player-log", {}, {}, 0, 0, {}, 0, 0, "logged in"});
    const auto events = w.takeEvents();
    const auto find = [&](const std::string& kind) {
        for (const auto& e : events)
            if (e.kind == kind)
                return &e;
        return static_cast<const WorldEvent*>(nullptr);
    };
    const auto* sale = find("economy");
    expect(sale && sale->item == "meal" && sale->quantity == 1 && sale->coins > 0 &&
               (sale->actor == "npc_keeper" || sale->target == "npc_keeper"),
           "The sale is logged with its goods and price");
    const auto* death = find("death");
    expect(death && death->actor == "npc_keeper" && death->cell == "tavern", "The death is logged where it happened");
    expect(find("revival") && find("arrival") && find("arrival")->detail == "logged in", "and the rest");
    expect(death->time == w.time() && death->day == w.calendarDays(), "with the time it happened");
    expect(w.takeEvents().empty(), "Events are handed over once");
    for (int i = 0; i < int(World::EventsKept) * 2; ++i)
        w.recordEvent({"noise", "player-log", {}, {}, 0, 0, {}, 0, 0, {}});
    const auto kept = w.takeEvents();
    expect(kept.size() <= World::EventsKept + World::EventsKept / 4 && w.droppedEvents() > 0,
           "Uncollected events are bounded, and the drops counted");
    WorldEvent odd;
    odd.actor = "wr\"en";
    odd.detail = std::string("line\nbreak \xff") + std::string(500, 'z');
    odd.cell = std::string(100, 'c');
    const auto json = eventsJson({odd});
    expect(json.find("\"kind\":\"event\"") != std::string::npos, "An event always has a kind");
    expect(json.find("wr\\\"en") != std::string::npos && json.find("line\\u000abreak") != std::string::npos,
           "Quotes and line breaks are escaped");
    expect(json.find('\xff') == std::string::npos, "Bytes that aren't UTF-8 are left out");
    expect(json.find(std::string(401, 'z')) == std::string::npos && json.find(std::string(81, 'c')) == std::string::npos,
           "Text is cut to what the log holds");
}

int main()
{
    try
    {
        deathAndResurrection();
        forgedActors();
        herbPatchReach();
        authoredWorld();
        continuousMovement();
        gradualFacing();
        postureMovement();
        clickPathing();
        transitions();
        bodies();
        sensesAndWeather();
        stealthPerception();
        posturePortalTransitions();
        mapMemory();
        elevationAndPersistence();
        terrainElevation();
        sightRaysMatchSampling();
        preparedViewsMatch();
        pathsAreRemembered();
        eventLog();
        cornerTrap();
        schedules();
        cellFiles();
        persistedRoundtripAndPrivacy();
        malformedPersistence();
        movementPersistence();
        scentWindAndWeather();
        scentAirPathsAndPrivacy();
        anonymousScentSectors();
        windAndScentPersistence();
        std::cout << "Passed " << checks << " world behavior assertions.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Failed after " << checks << " assertions: " << error.what() << "\n";
        return 1;
    }
}
