"""A world-sized drawing surface for generated regions.

The canvas holds one terrain code and one ground height per world tile, draws with the usual primitives
(rectangles, polygons, thick polylines), and at the end is cut into Atlas v3 cells: each cell keeps its own
terrain rows and sparse height overrides, exactly as Atlas stores them.
"""
from __future__ import annotations

import math

import numpy as np

import terrain_catalog as catalog

DEFAULT_HEIGHT = np.zeros(128, dtype=np.float32)
for _code, _height in catalog.DEFAULT_HEIGHTS.items():
    DEFAULT_HEIGHT[ord(_code)] = _height
SOLID = np.zeros(128, dtype=bool)
for _code in catalog.SOLID:
    SOLID[ord(_code)] = True
RAMP = np.zeros(128, dtype=bool)
for _code in catalog.RAMPS:
    RAMP[ord(_code)] = True


def code(ch: str) -> int:
    if ch not in catalog.GLYPHS:
        raise ValueError(f'{ch!r} is not a catalog terrain code')
    return ord(ch)


def half(h):
    """Heights are half-tile steps within ±16. Rounds halves up, so ground that climbs at most ½ a tile per tile
    never quantizes into a full step."""
    return np.clip(np.floor(np.asarray(h, dtype=np.float32) * 2 + .5) / 2, -16, 16)


class Canvas:
    def __init__(self, width: int, height: int, fill: str = ','):
        self.width, self.height = width, height
        self.codes = np.full((height, width), code(fill), dtype=np.uint8)
        self.heights = np.zeros((height, width), dtype=np.float32)
        # Tiles something has claimed (a road, a building, a wall): later scatter and cliff passes leave them alone.
        self.locked = np.zeros((height, width), dtype=bool)

    # -- queries -------------------------------------------------------------------------------------------
    def inside(self, x, y):
        return 0 <= x < self.width and 0 <= y < self.height

    def at(self, x, y) -> str:
        return chr(self.codes[y, x])

    # -- painting ------------------------------------------------------------------------------------------
    def paint(self, mask, ch: str, height=None, lock=True):
        self.codes[mask] = code(ch)
        if height is not None:
            self.heights[mask] = height if np.isscalar(height) else np.asarray(height)[mask]
        if lock:
            self.locked[mask] = True

    def rect(self, x0, y0, w, h, ch: str, height=None, lock=True):
        mask = np.zeros_like(self.locked)
        mask[max(0, y0):max(0, y0 + h), max(0, x0):max(0, x0 + w)] = True
        self.paint(mask, ch, height, lock)
        return mask

    def outline(self, x0, y0, w, h, ch: str, height=None, thickness=1):
        mask = np.zeros_like(self.locked)
        mask[y0:y0 + h, x0:x0 + w] = True
        mask[y0 + thickness:y0 + h - thickness, x0 + thickness:x0 + w - thickness] = False
        self.paint(mask, ch, height)
        return mask

    def polygon_mask(self, points) -> np.ndarray:
        """Tiles whose centres lie inside the polygon (even-odd rule)."""
        ys, xs = np.mgrid[0:self.height, 0:self.width]
        px, py = xs + .5, ys + .5
        inside = np.zeros((self.height, self.width), dtype=bool)
        n = len(points)
        for i in range(n):
            (x1, y1), (x2, y2) = points[i], points[(i + 1) % n]
            if y1 == y2:
                continue
            crosses = ((y1 <= py) & (py < y2)) | ((y2 <= py) & (py < y1))
            xcross = x1 + (py - y1) * (x2 - x1) / (y2 - y1)
            inside ^= crosses & (px < xcross)
        return inside

    def line_mask(self, points, width: float) -> np.ndarray:
        """Tiles within width/2 of a polyline."""
        mask = np.zeros((self.height, self.width), dtype=bool)
        r = width / 2
        for (x1, y1), (x2, y2) in zip(points, points[1:]):
            x0, x3 = int(min(x1, x2) - r - 1), int(max(x1, x2) + r + 2)
            y0, y3 = int(min(y1, y2) - r - 1), int(max(y1, y2) + r + 2)
            x0, y0, x3, y3 = max(x0, 0), max(y0, 0), min(x3, self.width), min(y3, self.height)
            if x0 >= x3 or y0 >= y3:
                continue
            ys, xs = np.mgrid[y0:y3, x0:x3]
            px, py = xs + .5, ys + .5
            dx, dy = x2 - x1, y2 - y1
            length2 = dx * dx + dy * dy or 1e-9
            t = np.clip(((px - x1) * dx + (py - y1) * dy) / length2, 0, 1)
            d = np.hypot(px - (x1 + t * dx), py - (y1 + t * dy))
            mask[y0:y3, x0:x3] |= d <= r
        return mask

    def stamp(self, x0, y0, rows: list[str], height=None, lock=True):
        """Copy a block of terrain rows; spaces keep what is there."""
        for dy, row in enumerate(rows):
            for dx, ch in enumerate(row):
                x, y = x0 + dx, y0 + dy
                if ch == ' ' or not self.inside(x, y):
                    continue
                self.codes[y, x] = code(ch)
                if height is not None:
                    self.heights[y, x] = height
                if lock:
                    self.locked[y, x] = True

    # -- output --------------------------------------------------------------------------------------------
    def cell(self, x0, y0, w, h) -> tuple[list[str], dict[str, float]]:
        """Terrain rows and sparse height overrides (heights that differ from their glyph's default)."""
        codes = self.codes[y0:y0 + h, x0:x0 + w]
        heights = half(self.heights[y0:y0 + h, x0:x0 + w])
        rows = [bytes(row).decode('ascii') for row in codes]
        defaults = DEFAULT_HEIGHT[codes]
        overrides = {}
        for y, x in zip(*np.nonzero(heights != defaults)):
            value = float(heights[y, x])
            overrides[f'{x},{y}'] = int(value) if value == int(value) else value
        return rows, overrides


def walkable_steps(codes: np.ndarray, heights: np.ndarray):
    """For each tile, whether it can be entered from its east and south neighbours (and back): the game's rule."""
    h = half(heights)
    solid = SOLID[codes]
    ramp = RAMP[codes]

    def pair(a_h, b_h, a_s, b_s, a_r, b_r):
        rise = np.abs(a_h - b_h)
        return ~a_s & ~b_s & ((rise <= .5) | ((rise <= 1) & (a_r | b_r)))
    east = pair(h[:, :-1], h[:, 1:], solid[:, :-1], solid[:, 1:], ramp[:, :-1], ramp[:, 1:])
    south = pair(h[:-1, :], h[1:, :], solid[:-1, :], solid[1:, :], ramp[:-1, :], ramp[1:, :])
    return east, south


def reachable(codes: np.ndarray, heights: np.ndarray, start: tuple[int, int]) -> np.ndarray:
    """Tiles a wolf can walk to from `start` (x, y) by the game's step rules, moving between cardinal neighbours."""
    east, south = walkable_steps(codes, heights)
    H, W = codes.shape
    seen = np.zeros((H, W), dtype=bool)
    sx, sy = start
    seen[sy, sx] = True
    frontier = [(sx, sy)]
    while frontier:
        nxt = []
        for x, y in frontier:
            for nx, ny, ok in ((x + 1, y, x + 1 < W and east[y, x]), (x - 1, y, x > 0 and east[y, x - 1]),
                               (x, y + 1, y + 1 < H and south[y, x]), (x, y - 1, y > 0 and south[y - 1, x])):
                if ok and not seen[ny, nx]:
                    seen[ny, nx] = True
                    nxt.append((nx, ny))
        frontier = nxt
    return seen


def distance(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])
