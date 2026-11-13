#pragma once
// The screen (Docs/Design/80-the-whole-screen.md): a player is shown every tile on its screen, whatever its wolf's
// line of sight, the light or the weather. "The screen" is this rectangle of tiles round the wolf: as much as the page
// shows at its widest zoom (Client/src/game/screen.ts holds the same numbers, and never draws more), and a few tiles
// over. The server (World::screenOf) and the page (screen.ts, screenRect) work it out alike.
#include <algorithm>
#include <cmath>

namespace ratw::screen
{
constexpr int HalfWidth = 96;    // Tiles shown each side of the wolf's own: 193 across,
constexpr int HalfHeight = 56;   // 113 down.

// Tiles x0..x1 by y0..y1, both ends in, in the cell's own coordinates (below 0 and past its size in the ground round it).
struct Rect
{
    int x0 = 0, y0 = 0, x1 = -1, y1 = -1;
    bool has(int x, int y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
    bool holds(double x, double y) const { return has(int(std::floor(x)), int(std::floor(y))); }
};

// Along one axis: `half` tiles each side of the wolf's own, moved to lie within the ground there is to show (`low` to
// `high`, both in), as the page's map stops at the edge of the ground; all of it where there is less than a screen's.
inline void span(double at, int half, int low, int high, int& first, int& last)
{
    first = std::max(low, std::min(int(std::floor(at)) - half, high - 2 * half));
    last = std::min(high, first + 2 * half);
}

// The screen of a wolf at (x, y) in a cell `width` x `height`, with `band` tiles of ground shown round the cell (0
// indoors, or far from every edge).
inline Rect around(double x, double y, int width, int height, int band)
{
    Rect r;
    span(x, HalfWidth, -band, width + band - 1, r.x0, r.x1);
    span(y, HalfHeight, -band, height + band - 1, r.y0, r.y1);
    return r;
}
} // namespace ratw::screen
