#pragma once
// Sight, the same for the server and the browser (Docs/Design/31-responsiveness.md, Phase 4.6): whether a line from a
// wolf's eye reaches a tile, over ground, things standing on it and closed doors. The server (World::lineOfSight)
// and the page (Client/wasm/walk.cpp, which shades the terrain the wolf sees) run this same code.
//
// A template over the grid (no virtual calls: sight is the server's most asked-for question). A Grid has:
//   const T* tile(int x, int y) const     // the tile, or null off the grid; T has .opaque, .height, .stature
//   bool closedAt(int x, int y, double px, double py) const   // a closed door (or other closable fixture) there
#include <algorithm>
#include <cmath>

namespace ratw::sight
{
constexpr double EyeHeight = 0.8;             // Above the ground the wolf stands on.
constexpr double SightTarget = 0.5;           // How high on a tile a line must reach to see it.

// Whether the line from the eye at `from` reaches the middle of the tile at `to`; both within a `width` x `height` grid.
template <class Grid>
bool lineOfSight(const Grid& grid, int width, int height, double fromX, double fromY, double toX, double toY)
{
    if (!std::isfinite(fromX) || !std::isfinite(fromY) || !std::isfinite(toX) || !std::isfinite(toY))
        return false;
    if (fromX < 0 || fromY < 0 || fromX >= width || fromY >= height || toX < 0 || toY < 0 || toX >= width || toY >= height)
        return false;
    // Sight runs from the observer's eye to the middle of the target's tile. Ground, and whatever stands on it,
    // rising above that line hides what lies beyond: the far side of a hill, a plateau above a cliff, a thicket.
    const auto* fromTile = grid.tile(int(std::floor(fromX)), int(std::floor(fromY)));
    const auto* toTile = grid.tile(int(std::floor(toX)), int(std::floor(toY)));
    const double eye = (fromTile ? double(fromTile->height) : 0.0) + EyeHeight;
    // A tall target (a tree, a statue) can show its top over ground that hides its foot.
    const double target = toTile ? double(toTile->height) + std::max(SightTarget, double(toTile->stature)) : SightTarget;
    const int toCellX = int(std::floor(toX)), toCellY = int(std::floor(toY));
    const double dx = toX - fromX, dy = toY - fromY;
    const int count = std::max(1, int(std::ceil(std::sqrt(dx * dx + dy * dy) / 0.12)));
    // The line is sampled every 0.12 tiles: sample i of count lies at fraction i / count of the way. Samples a
    // fraction of a tile apart mostly share a tile, and along a straight line the samples in one tile are
    // consecutive, so each tile crossed is checked once for the whole run of samples in it. Within a run only the
    // height of the sight line changes, and it changes steadily with f, so the lowest point of the line over that
    // tile is at the run's first sample when the line climbs and its last when it falls: ground that rises above
    // the line at any sample in the tile rises above it there. The result is exactly that of checking every sample.
    const auto at = [&](int i, bool y) {
        const double f = double(i) / count;
        return y ? fromY + dy * f : fromX + dx * f;
    };
    // The first sample after `i` that lies beyond tile coordinate `tileAt` along one axis (count if none does).
    const auto leaves = [&](int i, int tileAt, double delta, bool y) {
        if (delta == 0)
            return count;
        const double origin = y ? fromY : fromX;
        const double edge = delta > 0 ? tileAt + 1.0 : double(tileAt);
        const double guess = std::ceil((edge - origin) / delta * count);
        int next = !(guess < count) ? count : guess <= i ? i + 1 : int(guess);
        const auto beyond = [&](int k) { return delta > 0 ? at(k, y) >= edge : at(k, y) < edge; };
        while (next > i + 1 && beyond(next - 1))
            --next;
        while (next < count && !beyond(next))
            ++next;
        return next;
    };
    for (int i = 1; i < count;)
    {
        const double px = at(i, false), py = at(i, true);
        const int x = int(std::floor(px)), y = int(std::floor(py));
        const int end = std::min(leaves(i, x, dx, false), leaves(i, y, dy, true));  // One past this tile's run.
        // The occluding destination itself is visible, without revealing beyond it.
        if (x != toCellX || y != toCellY)
        {
            const auto* t = grid.tile(x, y);
            if (!t || t->opaque)
                return false;
            if (grid.closedAt(x, y, px, py))
                return false;
            if (t != fromTile)
            {
                const double f = double(target - eye >= 0 ? i : end - 1) / count;
                if (double(t->height) + double(t->stature) > eye + (target - eye) * f + 1e-6)
                    return false;
            }
        }
        i = end;
    }
    return true;
}
} // namespace ratw::sight
