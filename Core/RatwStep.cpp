#include "RatwStep.h"

#include <algorithm>
#include <cmath>

namespace ratw::step
{
bool stepAllowed(const Ground* from, const Ground& to)
{
    const double rise = std::abs(to.height - (from ? from->height : 0.0));
    return rise <= FreeStep + 1e-6 || (rise <= RampStep + 1e-6 && ((from && from->ramp) || to.ramp));
}

bool passable(const Grid& grid, Point p, const Ground* from)
{
    if (!std::isfinite(p.x) || !std::isfinite(p.y))
        return false;
    const Point samples[] = {p, {p.x - Radius, p.y}, {p.x + Radius, p.y}, {p.x, p.y - Radius}, {p.x, p.y + Radius}};
    for (const auto& s : samples)
    {
        const int x = int(std::floor(s.x)), y = int(std::floor(s.y));
        Ground g;
        if (!grid.ground(x, y, g) || g.solid || !stepAllowed(from, g) || grid.closedDoor(x, y))
            return false;
    }
    return true;
}

Point slide(const Grid& grid, Point at, Point proposed, const Ground* from, bool& blocked)
{
    blocked = false;
    if (passable(grid, proposed, from))
        return proposed;
    blocked = true;
    Point accepted = at;
    const double dx = proposed.x - at.x, dy = proposed.y - at.y;
    if (std::abs(dx) > Epsilon && passable(grid, {proposed.x, at.y}, from))
        accepted = {proposed.x, at.y};
    if (std::abs(dy) > Epsilon && passable(grid, {accepted.x, proposed.y}, from))
        accepted.y = proposed.y;
    return accepted;
}

int effectivePace(bool standing, bool exhausted, double stamina, int pace)
{
    // Lying movement first becomes a crouch. A requested sprint never overrides that posture, nor can it circumvent
    // exhaustion recovery.
    if (!standing || exhausted || stamina <= 0.0)
        return 0;
    return std::clamp(pace, 0, 10);
}

double paceSpeed(double dexterity, int pace)
{
    const double sprintSpeed = WalkSpeed * (2.0 + 2.0 * std::clamp(dexterity, 0.0, 1.0));
    return WalkSpeed + (sprintSpeed - WalkSpeed) * (pace / 10.0);
}

double groundSpeed(double flatSpeed, double movementCost, bool crouching, bool npc, double environment)
{
    double speed = flatSpeed / std::max(.1, movementCost);
    if (crouching)
        speed *= .30;
    if (npc)
        speed *= .57;
    return speed * environment;
}

void updateStamina(double& stamina, bool& exhausted, double& rate, int pace, double dt, double movedTime)
{
    if (dt <= 0.0)
        return;
    const double share = pace / 10.0;
    const double grossDrain = SprintDrain * share * share;
    const double before = stamina;
    stamina = std::clamp(before + StaminaRecovery * dt - grossDrain * std::clamp(movedTime, 0.0, dt), 0.0, 100.0);
    rate = (stamina - before) / dt;
    if (stamina <= Epsilon)
    {
        stamina = 0.0;
        exhausted = true;
    }
    else if (exhausted && stamina >= ExhaustionRecovery - Epsilon)
        exhausted = false;
}
} // namespace ratw::step
