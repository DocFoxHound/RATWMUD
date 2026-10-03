#pragma once
// Walking, the same for the server and the browser (Docs/Design/31-responsiveness.md, Phase 3): how fast a wolf goes,
// where it may stand, how it slides along what blocks it, and what moving costs in stamina. It knows nothing of the
// world but a grid of ground, so the browser can run it (as WebAssembly, Client/wasm) on the tiles it holds, and the
// server (World::integrate, and its checks of where a client says its wolf is) on the cell's own.
//
// Crossing doors, stairs and a cell's edges stays with the server: only it knows what lies beyond.
#include <cstdint>

namespace ratw::step
{
constexpr double Radius = 0.065;              // A wolf's footprint, each way from its centre (tiles).
// A wolf's body, each way from its centre (tiles): no two stand closer than twice this. They crowd and nudge each
// other (everyone, players and residents alike), but never stand on top of one another. Two side by side fill a
// one-tile passage.
constexpr double BodyRadius = 0.25;
constexpr double WalkSpeed = 2.6;             // Tiles a second, at a walk, on flat ground.
constexpr double StaminaRecovery = 5.0;       // A second.
constexpr double SprintDrain = 15.0;          // A second, at a full sprint.
constexpr double ExhaustionRecovery = 20.0;   // Stamina again before an exhausted wolf may hurry.
constexpr double FreeStep = 0.5, RampStep = 1.0;   // The height a step may climb; on a slope or stairs, more.
constexpr double Epsilon = 1e-7;

struct Point
{
    double x = 0, y = 0;
};

// One tile of ground, as walking sees it.
struct Ground
{
    bool solid = false;
    double height = 0;
    bool ramp = false;                        // A slope or stairs.
    double movementCost = 1;
};

// The ground a step is taken on: a tile, or false off the grid; and whether a closed door stands on a tile.
class Grid
{
  public:
    virtual ~Grid() = default;
    virtual bool ground(int x, int y, Ground& out) const = 0;
    virtual bool closedDoor(int x, int y) const = 0;
};

// Whether a step from one height to another may be taken.
bool stepAllowed(const Ground* from, const Ground& to);
// Whether a wolf may stand at `p`: its footprint clear of solid ground, closed doors and steps too high from `from`,
// and from the ground under its own centre. (The last matters where tiles of different heights meet at a corner: a
// footprint can reach from a middling tile onto both a lower and a higher one, and once its centre is on the lower,
// no step from there is allowed. A wolf that went there could never leave.)
bool passable(const Grid& grid, Point p, const Ground* from);
// Where a wolf going from `at` toward `proposed` ends: there, or slid along whatever blocks it, or where it was.
// `blocked`: it couldn't go all the way.
Point slide(const Grid& grid, Point at, Point proposed, const Ground* from, bool& blocked);

// The pace a wolf may really keep (0 walk ... 10 sprint): none but a walk crouched, sitting, lying or exhausted.
int effectivePace(bool standing, bool exhausted, double stamina, int pace);
// Its speed on flat ground (tiles a second), from its dexterity (0..1) and that pace.
double paceSpeed(double dexterity, int pace);
// Its speed on this ground: slower over costly ground, crouched, as a resident, or in heavy weather (`environment`,
// the weather's factor, 1 for none).
double groundSpeed(double flatSpeed, double movementCost, bool crouching, bool npc, double environment);
// Stamina after `dt` seconds, `movedTime` of them spent moving at that pace. Sets `exhausted` and the rate of change.
void updateStamina(double& stamina, bool& exhausted, double& rate, int pace, double dt, double movedTime);
} // namespace ratw::step
