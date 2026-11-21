#!/usr/bin/env python3
"""Walk a settlement's streets, paths and door approaches and report what closes them.

  python3 tools/path_check.py [WORLD]       check Ridgemere 3's city and towns in that dev world (default ridgemere3)
  python3 tools/path_check.py WORLD --json  the same, as JSON

A settlement is a box of outdoor ground in world coordinates (it may span several cells: the walk crosses their seams as
a wolf does). Walking is four-way, tile to tile, as Core's step rules allow (Core/RatwStep.cpp: solid tiles never; a step
of up to half a height, or a whole one onto or off stairs or a slope). These are reported:

- cut off: a path tile (street, road, footpath, bridge, flagstones, packed earth, an archway or doorway) or a door that
  cannot be walked to from the settlement's streets, not even through the buildings its doors join (rooms are taken as
  open inside). Each says whether a prop or other removable obstacle (a barrel, crates, a stall, a lamp, a tree, a
  fence...) is what closes it.
- shut in: a door whose own ground outside (a back door's strip behind its house) reaches the streets only through
  buildings, because a prop or other obstacle closes it off.
- closed path: an obstacle standing across a path with walls or other obstacles on both its sides, so the path has no
  open tile of width there, and going round is a long way (more than DETOUR tiles longer than walking through).
- steep step: two path tiles side by side whose heights differ more than a wolf may step between them.
- lamp in the way: a street lamp standing on a street's walking line. A lamp that interrupts a run of path tiles must
  stand in the street's edge row (against a building, a wall, water or the ground beyond the paving) and leave at least
  STREET_CLEAR open path tiles across the street beside it, so a straight walk along any street is never blocked by it.
  A street narrower than that gets its lamps beside it, off the paving, never in it.

fix(grid) clears each cut off, shut in or closed path (moving a prop aside where there is room, and clearing other obstacles) until
the check passes. tools/refresh_ridgemere3_map.py uses it on the world; tools/test_path_check.py checks the rules.
"""
from __future__ import annotations

import argparse
import collections
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import terrain_catalog as TC

SOLID = TC.SOLID
RAMPS = TC.RAMPS
HEIGHTS = TC.DEFAULT_HEIGHTS
# What a wolf walks a settlement by: the ways (roads, streets, paths, bridges), flagstones, packed earth, cinder yards,
# archways and doorways.
PATH = frozenset(TC.WAYS | set('.>G+'))
# Solid tiles that are not the ground's shape or a building: props, furniture, trees, fences, boulders, lamps.
OBSTACLE = frozenset(c for c in SOLID if TC.BY_CODE[c]['kind'] in ('feature', 'table', 'counter'))
# Obstacles that are carried aside rather than cleared away.
MOVABLE = frozenset("OxuTp=<yNtqgl'iK")
WATER = frozenset(c for c, t in TC.BY_CODE.items() if t['kind'] == 'water')
DETOUR = 8
LAMP = "'"                                     # The street lamp (glyph †).
STREET_CLEAR = 2                               # Open path tiles a lamp on a street must leave beside it.
SEALED = 'sealed yard'                         # A note, not a failure.
FREE_STEP, RAMP_STEP = .5, 1.

# Ridgemere3's active settlements: the walled city and the seven cn_ towns.
# Mirebridge was erased; its old rooms and crossroads are not an active town.
RIDGEMERE3 = {
    'Ridgemere': (150, 130, 490, 472),
    'Coldwater': (1862, 1702, 2051, 1891),
    'Larkfield': (1582, 2286, 1771, 2475),
    'Southmeadow': (2054, 3254, 2243, 3443),
    'Fenshore': (766, 3382, 955, 3571),
    'Saltwell': (3198, 2838, 3387, 3027),
    'Oakhaven': (918, 1126, 1107, 1315),
    'Frosthollow': (2214, 702, 2403, 891),
}


def step_allowed(h0, r0, h1, r1) -> bool:
    rise = abs(h1 - h0)
    return rise <= FREE_STEP + 1e-6 or (rise <= RAMP_STEP + 1e-6 and (r0 or r1))


class Grid:
    """The outdoor tiles of a box [x0, x1) x [y0, y1) of a project's world cells, editable in place."""

    def __init__(self, project, box, name=''):
        self.project, self.name = project, name
        self.x0, self.y0, self.x1, self.y1 = box
        self.w, self.h = self.x1 - self.x0, self.y1 - self.y0
        self.cells = [c for c in project['cells'] if c['x'] < self.x1 and c['x'] + c['width'] > self.x0 and
                      c['y'] < self.y1 and c['y'] + c['height'] > self.y0]
        self.rows = {c['id']: [list(r) for r in c['terrain']] for c in self.cells}
        self.changed = set()
        self.code = [[None] * self.w for _ in range(self.h)]
        self.height = [[0.] * self.w for _ in range(self.h)]
        self.where = {}
        for c in self.cells:
            for y in range(max(c['y'], self.y0), min(c['y'] + c['height'], self.y1)):
                for x in range(max(c['x'], self.x0), min(c['x'] + c['width'], self.x1)):
                    lx, ly = x - c['x'], y - c['y']
                    g = c['terrain'][ly][lx]
                    self.code[y - self.y0][x - self.x0] = g
                    self.height[y - self.y0][x - self.x0] = c['heights'].get(f'{lx},{ly}', HEIGHTS.get(g, 0))
        self.cell_at = {}
        for c in self.cells:
            self.cell_at[c['id']] = c
        # Door ends on this ground: (x, y) in world tiles -> [(link id, other end's area)].
        self.doors = collections.defaultdict(list)
        self.links = []
        for link in project['links']:
            for side, other in (('a', 'b'), ('b', 'a')):
                end = link[side]
                c = self.cell_at.get(end['cell'])
                if c is None:
                    continue
                gx, gy = c['x'] + int(end['x']), c['y'] + int(end['y'])
                if self.inside(gx, gy):
                    self.doors[(gx, gy)].append((link['id'], link[other]['cell']))
            self.links.append((link['a']['cell'], link['b']['cell'], link))

        # Places the world names on this ground (residents' homes and work, patrol posts, the spawn, door ends): never
        # a prop's new spot, nor beside one.
        self.reserved = set()
        stack = [v for k, v in project.items() if k not in ('cells', 'rooms')]
        while stack:
            v = stack.pop()
            if isinstance(v, dict):
                c = self.cell_at.get(v.get('cell'))
                if c is not None and isinstance(v.get('x'), (int, float)) and isinstance(v.get('y'), (int, float)):
                    self.reserved.add((c['x'] + int(v['x']), c['y'] + int(v['y'])))
                stack.extend(v.values())
            elif isinstance(v, list):
                stack.extend(v)

    def near_reserved(self, x, y) -> bool:
        return any((x + dx, y + dy) in self.reserved for dx, dy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)))

    def inside(self, x, y) -> bool:
        return self.x0 <= x < self.x1 and self.y0 <= y < self.y1

    def at(self, x, y):
        return self.code[y - self.y0][x - self.x0] if self.inside(x, y) else None

    def h_at(self, x, y):
        return self.height[y - self.y0][x - self.x0]

    def walkable(self, x, y, opened=frozenset()) -> bool:
        g = self.at(x, y)
        return g is not None and (g not in SOLID or (x, y) in opened)

    def set(self, x, y, code, height=None):
        """Change a tile (and its height override), in the grid and in its cell."""
        for c in self.cells:
            lx, ly = x - c['x'], y - c['y']
            if 0 <= lx < c['width'] and 0 <= ly < c['height']:
                self.rows[c['id']][ly][lx] = code
                if height is not None:
                    c['heights'][f'{lx},{ly}'] = height
                    self.height[y - self.y0][x - self.x0] = height
                self.changed.add(c['id'])
                break
        self.code[y - self.y0][x - self.x0] = code

    def set_height(self, x, y, height):
        self.set(x, y, self.at(x, y), height)

    def commit(self):
        """Write changed rows back into the project's cells."""
        for c in self.cells:
            if c['id'] in self.changed:
                c['terrain'] = [''.join(r) for r in self.rows[c['id']]]

    def neighbours(self, x, y, opened=frozenset(), blocked=frozenset()):
        if not self.walkable(x, y, opened):
            return
        g0, h0 = self.at(x, y), self.h_at(x, y)
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if (nx, ny) in blocked or not self.walkable(nx, ny, opened):
                continue
            if step_allowed(h0, g0 in RAMPS, self.h_at(nx, ny), self.at(nx, ny) in RAMPS):
                yield nx, ny

    def components(self, opened=frozenset()):
        """{tile: component number} of the walkable ground, and each component's size."""
        comp, sizes = {}, []
        for y in range(self.y0, self.y1):
            for x in range(self.x0, self.x1):
                if (x, y) in comp or not self.walkable(x, y, opened):
                    continue
                n = len(sizes)
                comp[(x, y)] = n
                stack, size = [(x, y)], 0
                while stack:
                    p = stack.pop()
                    size += 1
                    for q in self.neighbours(*p, opened):
                        if q not in comp:
                            comp[q] = n
                            stack.append(q)
                sizes.append(size)
        return comp, sizes


def joined(grid, comp, sizes):
    """Each component's group once the buildings' doors join them (rooms taken as open inside): {component: group}."""
    parent = list(range(len(sizes)))
    rooms = {}

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        a, b = find(a), find(b)
        if a != b:
            parent[b] = a

    nodes = {}

    def node(area):
        if area not in nodes:
            nodes[area] = len(parent)
            parent.append(len(parent))
        return nodes[area]

    for (x, y), ends in grid.doors.items():
        here = [comp[p] for p in [(x, y)] + [(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))]
                if p in comp and grid.walkable(*p)]
        for _, area in ends:
            if area in grid.cell_at:
                continue                     # (A seam or a door to another outdoor cell: walked as ground.)
            for c in here:
                union(node(area), c)
    for a, b, _ in grid.links:
        if a not in grid.cell_at and b not in grid.cell_at:
            union(node(a), node(b))
    return {c: find(c) for c in range(len(sizes))}


def door_tiles(grid):
    """The tiles a wolf stands on to use each outdoor door: [(door tile, link ids)]."""
    out = []
    for (x, y), ends in grid.doors.items():
        if all(area in grid.cell_at for _, area in ends):
            continue
        out.append(((x, y), sorted({i for i, _ in ends})))
    return out


def check(grid):
    """The problems on this ground: a list of dicts (kind, x, y, cell, what, blockers)."""
    problems = []
    comp, sizes = grid.components()
    if not sizes:
        return problems
    group = joined(grid, comp, sizes)
    group_size = collections.Counter()
    for c, s in enumerate(sizes):
        group_size[group[c]] += s
    main = group_size.most_common(1)[0][0]
    opened = frozenset((x, y) for y in range(grid.y0, grid.y1) for x in range(grid.x0, grid.x1)
                       if grid.at(x, y) in OBSTACLE)
    comp_open = None
    seen_groups = set()

    def report(kind, x, y, what, blockers=()):
        cell, lx, ly = local(grid, x, y)
        problems.append({'kind': kind, 'x': x, 'y': y, 'cell': cell, 'local': [lx, ly], 'what': what,
                         'blockers': sorted(blockers)})

    # Cut off: path tiles and doors outside the main group.
    doors = door_tiles(grid)
    door_at = {p for p, _ in doors}
    for (x, y), ids in doors:
        if not grid.walkable(x, y) or comp.get((x, y)) is None or group[comp[(x, y)]] != main:
            if comp_open is None:
                comp_open = obstacle_paths(grid, comp, group, main, opened)
            key = ('door', group.get(comp.get((x, y))), (x, y))
            blockers = comp_open(x, y)
            report('cut off', x, y, f'door {", ".join(ids)} cannot be reached' +
                   (' (obstacles close it)' if blockers else ''), blockers or ())
    # Shut in: a door whose own ground outside is apart from the streets, reached from them only through buildings (a
    # back door's strip behind its house), where obstacles close that ground off. Whoever comes out by it finds no way
    # on and goes back in, and out again, for good (Ridgemere 3, 2026-11-20: a barrel shut the Oakum Stair House's back
    # door in, and its household came and went through it all day).
    ground = max(range(len(sizes)), key=lambda c: sizes[c])
    ground_open = None
    for (x, y), ids in doors:
        c = comp.get((x, y))
        if c is None or c == ground or group[c] != main:
            continue                             # (On the streets' own ground; or cut off, above.)
        if ground_open is None:
            ground_open = obstacle_paths(grid, comp, {k: k for k in range(len(sizes))}, ground, opened)
        blockers = ground_open(x, y)
        if blockers:
            report('shut in', x, y, f'door {", ".join(ids)} opens onto {sizes[c]} tiles of ground that obstacles close '
                   f'off from the streets', blockers)
    for y in range(grid.y0, grid.y1):
        for x in range(grid.x0, grid.x1):
            if grid.at(x, y) not in PATH or (x, y) in door_at:
                continue
            g = group[comp[(x, y)]]
            if g == main or g in seen_groups:
                continue
            seen_groups.add(g)
            if comp_open is None:
                comp_open = obstacle_paths(grid, comp, group, main, opened)
            blockers = comp_open(x, y)
            n = sum(s for c, s in enumerate(sizes) if group[c] == g)
            if blockers:
                report('cut off', x, y, f'{n} tiles of ground with a path, closed off by obstacles', blockers)
            else:
                # Walled round with no door and nothing in the way: a yard nobody is meant to enter (a note).
                report(SEALED, x, y, f'{n} tiles of ground walled round, with no door')
    # Closed paths: obstacles across a path with no open tile beside them.
    for run in closed_runs(grid):
        (ax, ay), (bx, by), tiles = run
        report('closed path', tiles[0][0], tiles[0][1],
               f'{"".join(grid.at(*t) for t in tiles)} across the path from ({ax},{ay}) to ({bx},{by})', tiles)
    # Lamps on a street's walking line.
    for y in range(grid.y0, grid.y1):
        for x in range(grid.x0, grid.x1):
            if grid.at(x, y) == LAMP:
                why = in_the_way(grid, x, y)
                if why:
                    report(LAMP_IN_THE_WAY, x, y, f'a street lamp {why}')
    # Steep steps between path tiles.
    for y in range(grid.y0, grid.y1):
        for x in range(grid.x0, grid.x1):
            g = grid.at(x, y)
            if g not in PATH:
                continue
            for nx, ny in ((x + 1, y), (x, y + 1)):
                n = grid.at(nx, ny)
                if n in PATH and not step_allowed(grid.h_at(x, y), g in RAMPS, grid.h_at(nx, ny), n in RAMPS):
                    report('steep step', x, y, f'height {grid.h_at(x, y)} beside {grid.h_at(nx, ny)} at ({nx},{ny})')
                    problems[-1]['to'] = [nx, ny]
    return problems


LAMP_IN_THE_WAY = 'lamp in the way'


def street_tile(grid, x, y) -> bool:
    """Open path one walks a street by (a street, road, footpath, flagstones, packed earth, a doorway...)."""
    return grid.at(x, y) in PATH and grid.walkable(x, y)


def street_edge(grid, x, y) -> bool:
    """Whether (x, y) bounds a street: a building, a wall, water, the ground beyond the paving, the box's edge. Another
    prop or lamp is not an edge (it stands in the street too)."""
    g = grid.at(x, y)
    if g is None:
        return True
    if g in OBSTACLE:
        return False
    return not grid.walkable(x, y) or g not in PATH


def in_the_way(grid, x, y):
    """Why something solid at (x, y) would stand on a street's walking line, or None. It is in a street's line along an
    axis when the tiles before and after it on that axis are both street; then it must have an edge on one side across
    the street, and at least STREET_CLEAR street tiles on the other."""
    for dx, dy in ((1, 0), (0, 1)):
        if not (street_tile(grid, x - dx, y - dy) and street_tile(grid, x + dx, y + dy)):
            continue
        run = 'an east-west' if dx else 'a north-south'
        px, py = dy, dx                         # (Across the street.)
        e1, e2 = street_edge(grid, x + px, y + py), street_edge(grid, x - px, y - py)
        if e1 and e2:
            return f'fills {run} street one tile wide'
        if not e1 and not e2:
            return 'stands in the middle of a street, with no edge beside it'
        sx, sy = (-px, -py) if e1 else (px, py)
        clear = 0
        while clear < STREET_CLEAR and street_tile(grid, x + sx * (clear + 1), y + sy * (clear + 1)):
            clear += 1
        if clear < STREET_CLEAR:
            return f'leaves {run} street only {clear} tile wide beside it'
    return None


def failures(problems):
    """The problems that fail the check (a sealed yard is only noted)."""
    return [p for p in problems if p['kind'] != SEALED]


def local(grid, x, y):
    for c in grid.cells:
        if c['x'] <= x < c['x'] + c['width'] and c['y'] <= y < c['y'] + c['height']:
            return c['id'], x - c['x'], y - c['y']
    return '', x, y


def obstacle_paths(grid, comp, group, main, opened):
    """A function: the fewest obstacles to clear for (x, y) to reach the main group (empty if none would do)."""
    in_main = {p for p, c in comp.items() if group[c] == main}

    def search(x, y):
        # 0-1 search from the tile over walkable ground (cost 0) and obstacles (cost 1) to the main group.
        start = (x, y)
        if not grid.walkable(x, y, opened):
            return []
        best = {start: 0}
        back = {}
        queue = collections.deque([start])
        while queue:
            p = queue.popleft()
            if p in in_main:
                path, q = [], p
                while q in back:
                    if grid.at(*q) in OBSTACLE:
                        path.append(q)
                    q = back[q]
                if grid.at(*q) in OBSTACLE:
                    path.append(q)
                return path
            for q in grid.neighbours(*p, opened):
                cost = best[p] + (1 if grid.at(*q) in OBSTACLE else 0)
                if cost < best.get(q, 1 << 30):
                    best[q] = cost
                    back[q] = p
                    (queue.append if cost > best[p] else queue.appendleft)(q)
        return []

    return search


def detour(grid, a, b, blocked, limit):
    """Steps from a to b within `limit` of a without crossing `blocked` (None if longer)."""
    best = {a: 0}
    queue = collections.deque([a])
    while queue:
        p = queue.popleft()
        if p == b:
            return best[p]
        if best[p] >= limit:
            continue
        for q in grid.neighbours(*p, blocked=blocked):
            if q not in best:
                best[q] = best[p] + 1
                queue.append(q)
    return None


def closed_runs(grid):
    """Obstacle runs across a path: [(a, b, tiles)] where a and b are path tiles on either side."""
    out, seen = [], set()
    for y in range(grid.y0, grid.y1):
        for x in range(grid.x0, grid.x1):
            if grid.at(x, y) not in OBSTACLE:
                continue
            for dx, dy in ((1, 0), (0, 1)):
                if ((x, y), dx) in seen:
                    continue
                tiles = [(x, y)]
                ax, ay = x - dx, y - dy
                while grid.at(ax, ay) in OBSTACLE:
                    tiles.insert(0, (ax, ay))
                    ax, ay = ax - dx, ay - dy
                bx, by = x + dx, y + dy
                while grid.at(bx, by) in OBSTACLE:
                    tiles.append((bx, by))
                    bx, by = bx + dx, by + dy
                for t in tiles:
                    seen.add((t, dx))
                if grid.at(ax, ay) not in PATH or grid.at(bx, by) not in PATH or len(tiles) > 4:
                    continue
                if not all(sealed(grid, t, dy, dx) for t in tiles):
                    continue                   # Some open tile beside the run: the path keeps its width there.
                straight = len(tiles) + 1
                around = detour(grid, (ax, ay), (bx, by), frozenset(tiles), straight + DETOUR)
                if around is not None and around - straight <= DETOUR:
                    continue
                out.append(((ax, ay), (bx, by), tiles))
    return out


def sealed(grid, tile, px, py):
    """Whether the cross-section through an obstacle (perpendicular px, py) meets no open ground on either side."""
    for s in (1, -1):
        x, y = tile[0] + s * px, tile[1] + s * py
        while grid.at(x, y) in OBSTACLE:
            x, y = x + s * px, y + s * py
        if grid.walkable(x, y):
            return False
    return True


def ground_for(grid, x, y):
    """What a cleared tile becomes: the commonest walkable path code beside it, else the commonest walkable code."""
    around = [grid.at(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))]
    paths = [g for g in around if g in PATH and g not in '+G']
    ground = [g for g in around if g is not None and g not in SOLID and g not in '+G' and g not in WATER and
              g not in RAMPS]
    pick = collections.Counter(paths or ground or ['.']).most_common(1)[0][0]
    return pick


def harmless(grid, x, y, avoid, streets=True):
    """Whether a prop may stand at (x, y): open ground against something solid, away from doors, closing nothing."""
    g = grid.at(x, y)
    if g is None or g in SOLID or g in '+G' or g in WATER or g in RAMPS or (x, y) in avoid:
        return False
    if grid.near_reserved(x, y):
        return False
    if not streets and g in '_8':
        return False                         # (First choice: off the cobbles and bridges.)
    if any(abs(x - dx) + abs(y - dy) <= 2 for dx, dy in grid.doors):
        return False
    if in_the_way(grid, x, y):
        return False                         # (At a street's edge, never on its walking line.)
    sides = [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    open_sides = [p for p in sides if grid.walkable(*p)]
    if len(open_sides) < 2 or len(open_sides) == 4:
        return False                         # (Not a nook nobody could stand in; tucked aside, never out in a lane.)
    for p in open_sides[1:]:
        if detour(grid, open_sides[0], p, frozenset({(x, y)}), 8) is None:
            return False                     # Its open neighbours stay joined close by without it.
    for dx, dy in ((1, 0), (0, 1)):
        a, b = (x - dx, y - dy), (x + dx, y + dy)
        if grid.walkable(*a) and grid.walkable(*b) and not (grid.walkable(x + dy, y + dx) or grid.walkable(x - dy, y - dx)):
            return False                     # It would stand across a one-tile way.
    return True


def clear(grid, tiles, counts, moves):
    """Clear obstacles off a path, carrying movable props aside where there is room."""
    tiles = list(tiles)
    avoid = set(tiles)
    for x, y in tiles:
        g = grid.at(x, y)
        if g not in OBSTACLE:
            continue
        grid.set(x, y, ground_for(grid, x, y))
        if g in MOVABLE:
            spot = None
            for streets in (False, True):
                for r in range(1, 7):
                    ring = sorted((abs(dx) + abs(dy), x + dx, y + dy) for dx in range(-r, r + 1) for dy in range(-r, r + 1)
                                  if max(abs(dx), abs(dy)) == r)
                    spot = next(((sx, sy) for _, sx, sy in ring if grid.inside(sx, sy) and
                                 abs(grid.h_at(sx, sy) - grid.h_at(x, y)) < 1e-6 and
                                 harmless(grid, sx, sy, avoid, streets)), None)
                    if spot:
                        break
                if spot:
                    break
            if spot:
                grid.set(*spot, g)
                avoid.add(spot)
                counts['props moved aside'] += 1
                moves.append((g, (x, y), spot))
            else:
                counts['props removed (no room beside the path)'] += 1
                moves.append((g, (x, y), None))
        else:
            counts['obstacles cleared'] += 1
            moves.append((g, (x, y), None))


def ease(grid, problem, counts, margin=3):
    """Grade a steep step between path tiles: the lower side is raised in half steps, across the path and a little
    beside it, until it meets its own ground."""
    (x, y), (nx, ny) = (problem['x'], problem['y']), tuple(problem['to'])
    if grid.h_at(x, y) < grid.h_at(nx, ny):
        (x, y), (nx, ny) = (nx, ny), (x, y)
    dx, dy = nx - x, ny - y
    high = grid.h_at(x, y)
    for side in range(-margin, margin + 1):
        sx, sy = nx + side * dy, ny + side * dx
        for k in range(16):
            tx, ty = sx + k * dx, sy + k * dy
            want = high - FREE_STEP * (k + 1)
            if not grid.walkable(tx, ty) or grid.at(tx, ty) in WATER or grid.h_at(tx, ty) >= want:
                break
            grid.set_height(tx, ty, want)
            counts['tiles graded beside a steep step'] += 1


def fix(grid, counts=None, moves=None, rounds=12):
    """Clear what the check finds on this ground until it passes (or nothing more can be cleared). The remaining problems."""
    counts = counts if counts is not None else collections.Counter()
    moves = moves if moves is not None else []
    for _ in range(rounds):
        problems = check(grid)
        steps = [p for p in problems if p['kind'] == 'steep step']
        for p in steps:
            ease(grid, p, counts)
        todo = [p for p in problems if p['blockers'] and p['kind'] != SEALED]
        if not todo and not steps:
            return problems
        done = set()
        for p in todo:
            tiles = [tuple(t) for t in p['blockers'] if tuple(t) not in done]
            done.update(tiles)
            clear(grid, tiles, counts, moves)
    return check(grid)


def settlements(project, boxes=None):
    return {name: Grid(project, box, name) for name, box in (boxes or RIDGEMERE3).items()}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('world', nargs='?', default='ridgemere3')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args(argv)
    import world_db
    import world_store
    with world_db.connect(args.world, 'editor') as conn:
        project, revision = world_store.load_world(conn, 'upper_accord')
    report = {'world': args.world, 'revision': revision, 'settlements': {}}
    total = 0
    for name, grid in settlements(project).items():
        problems = check(grid)
        total += len(failures(problems))
        report['settlements'][name] = problems
    if args.json:
        print(json.dumps(report, indent=1))
    else:
        print(f'{args.world} revision {revision}:')
        for name, problems in report['settlements'].items():
            bad = failures(problems)
            notes = len(problems) - len(bad)
            print(f'  {name}: {"clear" if not bad else f"{len(bad)} problem(s)"}' +
                  (f' ({notes} sealed yard(s) noted)' if notes else ''))
            for p in bad[:40]:
                print(f'    {p["kind"]} at {p["cell"]} {tuple(p["local"])}: {p["what"]}')
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main())
