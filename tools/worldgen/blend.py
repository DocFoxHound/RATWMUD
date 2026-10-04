"""Blending ground across a cell edge (Docs/Design/39): where two generators' ground meets in a straight line.

Upper Accord's three northern cells (its heights) meet the western world's cold moors and barrens along their top
edge: grass and pine on one side, tundra, snow and rock on the other, changing in a single row. This dithers the two
together over a band either side: tiles near the edge take the ground of the other side (sampled from just across
it), less and less with distance. Only natural ground changes, and only for ground of the same kind (open for open,
a tree for a tree), so nothing becomes walkable or blocked that wasn't; heights, roads, buildings, water and cliffs
stay exactly as they are. Deterministic (the same seed gives the same ground).
"""
from __future__ import annotations

import random

import terrain_catalog as catalog

NORTHERN = ('northwest_heights', 'northern_ridge', 'northeast_heights')
INSIDE, OUTSIDE = 40, 16            # Band widths in tiles: into Upper Accord's cells, and into the cells beyond.
# Ground that may change, or be copied: the catalog's natural tiles, but no water, ice or cliffs.
NATURAL = {t['code'] for t in catalog.TILES if t['category'] == 'nature'} - set('Ww~J%')
SOLID = {t['code'] for t in catalog.TILES if t.get('solid')}


class Ground:
    """The project's outdoor cells, readable and writable by world tile."""

    def __init__(self, project):
        self.cells = [c for c in project['cells'] if c.get('z', 0) == 0]
        self.rows = {}

    def _cell(self, x, y):
        for c in self.cells:
            if c['x'] <= x < c['x'] + c['width'] and c['y'] <= y < c['y'] + c['height']:
                return c
        return None

    def get(self, x, y):
        c = self._cell(x, y)
        if not c:
            return None
        rows = self.rows.get(c['id'])
        row = rows[y - c['y']] if rows else c['terrain'][y - c['y']]
        return row[x - c['x']]

    def set(self, x, y, ch):
        c = self._cell(x, y)
        rows = self.rows.setdefault(c['id'], [list(r) for r in c['terrain']])
        rows[y - c['y']][x - c['x']] = ch

    def write_back(self):
        for c in self.cells:
            if c['id'] in self.rows:
                c['terrain'] = [''.join(r) for r in self.rows.pop(c['id'])]


def falloff(d, band, strength):
    return strength * max(0.0, 1 - d / band) ** 1.6


def blend_north(project, cells=NORTHERN, inside=INSIDE, outside=OUTSIDE, seed=3939):
    """Blends each named cell's top edge into the ground north of it. Returns how many tiles changed."""
    rng = random.Random(seed)
    g = Ground(project)
    changed = 0

    def swap(x, y, sx, sy):
        nonlocal changed
        here, there = g.get(x, y), g.get(sx, sy)
        if here is None or there is None or here == there:
            return
        if here not in NATURAL or there not in NATURAL or (here in SOLID) != (there in SOLID):
            return
        g.set(x, y, there)
        changed += 1

    for cid in cells:
        cell = next((c for c in project['cells'] if c['id'] == cid), None)
        if cell is None:
            continue
        edge = cell['y']
        for x in range(cell['x'], cell['x'] + cell['width']):
            # Into the cell: the northern ground, thinning out with distance from the edge.
            for d in range(inside):
                if rng.random() < falloff(d, inside, .9):
                    swap(x, edge + d, x + rng.randint(-4, 4), edge - 1 - rng.randint(0, 10))
            # Beyond it: a little of the cell's own ground, over a narrower band.
            for d in range(1, outside):
                if rng.random() < falloff(d, outside, .45):
                    swap(x, edge - d, x + rng.randint(-4, 4), edge + rng.randint(0, 10))
    g.write_back()
    return changed
