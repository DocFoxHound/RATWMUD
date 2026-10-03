// The browser's walking and sight (Docs/Design/31-responsiveness.md, Phases 3 and 4.6): the server's own rules
// (Core/RatwStep.h, Core/RatwSight.h), compiled to WebAssembly by tools/build_wasm.sh into Client/src/wasm/walk.wasm.
// The page puts the cell it holds into the grid (Client/src/game/walker.ts) and asks for one step at a time, and for
// what its wolf can see when it reaches a new tile; the server checks every pose it is sent.
#include "RatwSight.h"
#include "RatwStep.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace ratw::step;

namespace
{
// One tile as the page writes it: 16 bytes, little-endian.
struct Tile
{
    float height;
    float cost;
    float stature;                                // How far what stands on it rises, for sight.
    std::uint8_t solid, ramp, closedDoor, opaque;
};
static_assert(sizeof(Tile) == 16, "the page writes 16-byte tiles");

int width = 0, height = 0;
std::vector<Tile> tiles;
std::vector<std::uint8_t> seen;
double result[3];

// The grid as sight (RatwSight.h) asks for it.
struct SightGrid
{
    const Tile* tile(int x, int y) const
    {
        return x < 0 || y < 0 || x >= width || y >= height ? nullptr : &tiles[std::size_t(y) * std::size_t(width) + std::size_t(x)];
    }
    bool closedAt(int x, int y, double, double) const { return tile(x, y)->closedDoor != 0; }
};

class Cell final : public Grid
{
  public:
    bool ground(int x, int y, Ground& out) const override
    {
        if (x < 0 || y < 0 || x >= width || y >= height)
            return false;
        const auto& t = tiles[std::size_t(y) * std::size_t(width) + std::size_t(x)];
        out = {t.solid != 0, t.height, t.ramp != 0, t.cost};
        return true;
    }
    bool closedDoor(int x, int y) const override
    {
        return x >= 0 && y >= 0 && x < width && y < height && tiles[std::size_t(y) * std::size_t(width) + std::size_t(x)].closedDoor;
    }
} grid;

bool groundAt(double x, double y, Ground& out) { return grid.ground(int(std::floor(x)), int(std::floor(y)), out); }
} // namespace

extern "C"
{
// A grid of `w` by `h` tiles, all open floor, for the page to fill in: where its first tile is.
__attribute__((export_name("walk_grid"))) Tile* walkGrid(int w, int h)
{
    width = w > 0 && h > 0 && w <= 4096 && h <= 4096 ? w : 0;
    height = width ? h : 0;
    tiles.assign(std::size_t(width) * std::size_t(height), Tile{0, 1, 0, 0, 0, 0, 0});
    seen.assign(tiles.size(), 0);
    return tiles.data();
}

// Where three numbers are left by walk_step: x, y, and 1 if the step was blocked.
__attribute__((export_name("walk_result"))) double* walkResult() { return result; }

// One step of `dt` seconds from (x, y) heading (ix, iy), at `flat` tiles a second on flat ground, crouched or not, in
// weather slowing it by `environment`: as World::integrate takes it, sliding along whatever blocks it.
__attribute__((export_name("walk_step"))) void walkStep(double x, double y, double ix, double iy, double flat, int crouching,
                                                         double environment, double dt)
{
    result[0] = x;
    result[1] = y;
    result[2] = 0;
    const double length = std::hypot(ix, iy);
    Ground start;
    if (length < Epsilon || dt <= 0 || !groundAt(x, y, start))
        return;
    const double speed = groundSpeed(flat, start.movementCost, crouching != 0, false, environment);
    const double travel = speed * dt;
    bool blocked = false;
    const auto to = slide(grid, {x, y}, {x + ix / length * travel, y + iy / length * travel}, &start, blocked);
    result[0] = to.x;
    result[1] = to.y;
    result[2] = blocked ? 1 : 0;
}

// Whether a wolf may stand at (x, y), stepping from (fx, fy).
__attribute__((export_name("walk_passable"))) int walkPassable(double x, double y, double fx, double fy)
{
    Ground from;
    const bool known = groundAt(fx, fy, from);
    return passable(grid, {x, y}, known ? &from : nullptr) ? 1 : 0;
}

// What a wolf at (x, y) with this sight range can see: one byte a tile (1 seen), in the grid's order, as
// World::visibleTileMask works it out on the server.
__attribute__((export_name("walk_sight"))) std::uint8_t* walkSight(double x, double y, double range)
{
    std::fill(seen.begin(), seen.end(), 0);
    if (!(range > 0) || !width)
        return seen.data();
    const int x0 = std::max(0, int(std::floor(x - range))), x1 = std::min(width - 1, int(std::ceil(x + range)));
    const int y0 = std::max(0, int(std::floor(y - range))), y1 = std::min(height - 1, int(std::ceil(y + range)));
    const SightGrid grid;
    for (int ty = y0; ty <= y1; ++ty)
        for (int tx = x0; tx <= x1; ++tx)
        {
            const double px = tx + .5, py = ty + .5;
            if (std::hypot(px - x, py - y) <= range && ratw::sight::lineOfSight(grid, width, height, x, y, px, py))
                seen[std::size_t(ty) * std::size_t(width) + std::size_t(tx)] = 1;
        }
    return seen.data();
}
}
