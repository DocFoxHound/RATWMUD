"""Ridgemere 2.0: the city (Docs/Design/67-ridgemere-2-the-city.md, section 8 A).

Lays out the city alone on a canvas of 1,024 x 768 tiles (4 x 3 cells): the bay on the west, the wharf inside its sea
wall, the Sump, the Commons and Northgate, the abbey, House Greatly's compound, the Old Keep, the administrative sector,
the Cinders, the Great Palace and the Rise inside the walls; the three Houses' estates and the roads outside. Reused
interiors keep their IDs; new ones are r2_. Then places about 560 of the world's NPCs in it (worldgen.ridgemere2_people)
and writes the city into DEV (ratw_ridgemere2, also called ridgemere2), keeping every NPC, their voices, families and talk.

  cd tools && python3 -m worldgen.ridgemere2 preview [--seed N]     preview images, the report and the manifest
  cd tools && python3 -m worldgen.ridgemere2 place                  the NPC plan (artifacts/ridgemere2/placement.json)
  cd tools && python3 -m worldgen.ridgemere2 write [--world dev]   back up, then write the city into DEV (also called
                                                                   ridgemere2; a new map identity: doc 66)

(python3 tools/worldgen/ridgemere2.py ... works too.) The seed is fixed, so every run makes the same city; a write loads
the world fresh and rewrites only the map and the placed NPCs' places, so it can be run again.

The old generator (realm.py, cities.py) is not used to lay anything out here and its output is unchanged: this module
subclasses its Ridgemere for the helpers (ring, gate, compound, gardens) and keeps its own copies of realm.py's helpers
that are bound to the old 2,048 canvas.
"""
from __future__ import annotations

import argparse
import heapq
import json
import math
import random
import sys
import time
from collections import Counter, defaultdict, deque
from pathlib import Path

import numpy as np

if __package__ in (None, ''):                      # Run as a file: python3 tools/worldgen/ridgemere2.py
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    __package__ = 'worldgen'
    import worldgen  # noqa: F401

from . import field, preview  # noqa: E402
from .buildings import (STYLES, Building, Plan, boarded, cathedral, dorter, flats, hall, keep_tower,  # noqa: E402
                        market_hall, office_floor, palace_antechamber, palace_residence, palace_wing, shed,
                        shop_with_flat, stacks, stairs_between, tavern, works_kit)
from .canvas import DEFAULT_HEIGHT, RAMP, SOLID, Canvas, half  # noqa: E402  (RAMP: district_metrics)
from .cities import CitySite, Ridgemere, fp, grow, shrink  # noqa: E402
from .site import OPPOSITE, STEP as DOOR_STEP, TURNS, Lots, door_tile, footprint_size  # noqa: E402

import terrain_catalog  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / 'artifacts' / 'ridgemere2'
W, H = 768, 768
CELL = 256
SEED = 20261030
REGION = 'ridgemere'

# --- The plan's geometry (doc 67, section 2; "Dense city" in its Progress notes). Canvas tiles; ranges half-open. ----
# The city was made denser on 2026-10-31: the inside of the walls shrank from 488 x 462 to 336 x 330 tiles (about half),
# the districts lost their rectangles and their retaining walls, and the ground climbs from the bay by slopes and
# stair-streets instead of walled tiers.
SHORE = [(96, 0), (110, 120), (168, 170), (168, 610), (120, 660), (96, 768)]
INSIDE = (172, 468, 154, 452)            # x0, x1, y0, y1 inside the wall
QUAY_X = 210                             # the wharf is x 172-210; the Sump and the Cinders meet it there
WALL = 4
DISTRICT_IDS = ['outside', 'wharf', 'abbey', 'northgate', 'greatly', 'sump', 'commons', 'keep', 'admin', 'cinders',
                'palace', 'rise', 'wall']
# The talk quarter each district's buildings are filed under (doc 67, section 5, Talk; talk_seed.quarter_of).
TALK_DISTRICT = {'wharf': 'wharf', 'abbey': 'commons_abbey', 'northgate': 'commons_northgate', 'greatly': 'estate_greatly',
                 'sump': 'sump', 'commons': 'commons', 'keep': 'heights_keep', 'admin': 'office', 'cinders': 'works',
                 'palace': 'office_palace', 'rise': 'heights'}
GROUND = {'wharf': 'f', 'abbey': 'f', 'northgate': '.', 'greatly': ',', 'sump': '.', 'commons': '.', 'keep': 'f',
          'admin': 'f', 'cinders': '>', 'palace': 'f', 'rise': ','}
# How well-off each district is: blurred over the borders, it picks the buildings of the band where two districts meet
# (doc 67, Dense city: the borders blend).
WEALTH = {'wharf': .2, 'abbey': 1.0, 'northgate': 1.0, 'greatly': 2.2, 'sump': 0.0, 'commons': 1.0, 'keep': 1.2,
          'admin': 1.2, 'cinders': .2, 'palace': 2.2, 'rise': 2.0}
# The walled pieces inside the city (the only walls inside it besides the city wall itself).
ABBEY = (210, 290, 154, 216)
GREATLY = (392, 468, 154, 224)
KEEP = (390, 434, 322, 364)
PALACE = (288, 382, 392, 450)
GATES = {'N': (360, 150), 'E': (468, 300), 'S': (412, 452), 'cinder': (240, 452), 'harbour': (190, 452)}
PIERS = [176, 206, 236, 262, 290, 320, 350, 380]
CRANES = [206, 262, 320, 380]
CUSTOMS_PIER, STAIRS_PIER = 380, 290
SQUARES = {'market': ((360, 300), (22, 14)), 'rag': ((244, 302), (11, 7)), 'records': ((330, 372), (13, 7)),
           'palace': ((336, 397), (45, 4)), 'greatly': ((430, 233), (12, 7)), 'pillory': ((412, 311), (11, 6)),
           'clock': ((362, 188), (11, 6))}
SPAWN = (360, 312)
# The three Houses' estates outside the walls, their grounds about 40% smaller than before.
ESTATES = {'mossgrave': ('Timberland Court', (300, 524, 112, 78)), 'fell': ('Fellhall', (552, 266, 128, 104)),
           'grayrock': ('Grayrock Hall', (140, 646, 100, 78))}
# The Rise: five Minor House lots (x, y, w, h), each a house in a small railed garden.
RISE_LOTS = [(388, 374, 22, 28), (418, 374, 22, 28), (442, 374, 22, 28), (388, 408, 22, 28), (418, 408, 22, 28)]
# Old district boxes -> new, to carry the old map's anchors for the reused buildings over.
REMAP = {'wharf': ((172, 222, 154, 616), (172, 210, 154, 444)), 'sump': ((222, 364, 232, 470), (212, 298, 216, 344)),
         'commons': ((364, 660, 250, 470), (300, 466, 228, 350)), 'northgate': ((330, 540, 154, 250), (290, 392, 158, 214)),
         'admin': ((260, 540, 470, 530), (248, 380, 350, 388)), 'cinders': ((222, 320, 470, 616), (212, 270, 376, 450))}


def remap(district, pt):
    if pt is None or district not in REMAP:
        return pt
    (a0, a1, b0, b1), (c0, c1, d0, d1) = REMAP[district]
    return (c0 + (pt[0] - a0) * (c1 - c0) / (a1 - a0), d0 + (pt[1] - b0) * (d1 - d0) / (b1 - b0))


def box(x0, x1, y0, y1):
    m = np.zeros((H, W), dtype=bool)
    m[max(0, y0):max(0, y1), max(0, x0):max(0, x1)] = True
    return m


def blur(a, r):
    """A box blur of radius r (twice, so it is smooth)."""
    a = a.astype(np.float32)
    for _ in range(2):
        for axis in (0, 1):
            pad = [(0, 0), (0, 0)]
            pad[axis] = (r + 1, r)
            s = np.cumsum(np.pad(a, pad, mode='edge'), axis=axis)
            hi = np.take(s, range(2 * r + 1, s.shape[axis]), axis=axis)
            lo = np.take(s, range(0, s.shape[axis] - 2 * r - 1), axis=axis)
            a = (hi - lo) / (2 * r + 1)
    return a


def shore_x(y):
    ys = [p[1] for p in SHORE]
    xs = [p[0] for p in SHORE]
    return np.interp(y, ys, xs)


def cell_of(x, y):
    return int(x) // CELL, int(y) // CELL


def cell_id(cx, cy):
    return f'w_ridgemere_{cx}_{cy}'


# ---------------------------------------------------------------------------------------------------------------
# The mini realm: only what City and Ridgemere use, on this canvas
# ---------------------------------------------------------------------------------------------------------------
class MiniRealm:
    STEP = 4

    def __init__(self, seed=SEED):
        self.seed = seed
        self.rng = random.Random(seed)
        self.c = Canvas(W, H, ',')
        z = lambda dtype=bool: np.zeros((H, W), dtype=dtype)
        self.water, self.ocean, self.lake, self.river = z(), z(), z(), z()
        self.biome = z(np.int16)
        self.places, self.levels, self.gates, self.gate_levels, self.road_lines = {}, {}, {}, {}, {}
        self.ua_mask = z()
        self.reserved_ids = set()
        self.city_cells = set()
        self.causeway = z()
        self.no_road = None
        self.features = []

    # -- the land ----------------------------------------------------------------------------------------------
    def lay_land(self):
        """The bay west of the shore; wet moorland and heath east of it, rising gently from the water."""
        c = self.c
        ys, xs = np.mgrid[0:H, 0:W]
        shore = shore_x(np.arange(H))[:, None]
        sea = xs < shore
        self.water[:] = sea
        self.ocean[:] = sea
        dist = xs - shore
        shallow = sea & (dist > -3)
        c.codes[sea] = ord('W')
        c.codes[shallow] = ord('~')
        c.heights[sea] = 0
        noise = field.fbm(W, H, 60, self.seed % 1000, 3)
        rise = np.clip(dist / 140, 0, 1) * 1.6 + (noise - .5) * .6
        c.heights[~sea] = half(np.clip(.5 + rise, .5, 2.5))[~sea]
        # Wet moorland: grass, heath, tall grass, moss and ferns, scattered trees and shrubs, a few boulders.
        grain = np.random.default_rng(self.seed).random((H, W))
        n2 = field.fbm(W, H, 18, self.seed % 997 + 3, 2)
        land = ~sea
        codes = np.full((H, W), ord(','), dtype=np.uint8)
        codes[n2 > .58] = ord('5')
        codes[(n2 < .36)] = ord('"')
        codes[(grain < .03)] = ord('!')
        codes[(grain > .985)] = ord('Y')
        codes[(grain > .975) & (grain <= .985)] = ord('B')
        codes[(grain > .97) & (grain <= .975)] = ord('&')
        codes[(grain > .968) & (grain <= .97)] = ord('o')
        codes[(grain > .962) & (grain <= .968) & (n2 > .5)] = ord('P')
        c.codes[land] = codes[land]
        self.biome[:] = np.where(sea, 0, 1)

    # -- realm.py's helpers, sized to this canvas ------------------------------------------------------------------
    def flatten_mask(self, region, margin=10):
        """Level ground under `region`, eased into the land around it over `margin` tiles."""
        c = self.c
        Hh, Ww = c.codes.shape
        ys, xs = np.nonzero(region)
        level = float(half(np.median(c.heights[ys, xs])))
        y0, y1 = max(0, ys.min() - margin - 2), min(Hh, ys.max() + margin + 3)
        x0, x1 = max(0, xs.min() - margin - 2), min(Ww, xs.max() + margin + 3)
        near = field.distance_to_mask(region[y0:y1, x0:x1], margin + 1)
        weight = 1 - field.smoothstep(0, margin, near)
        sub = c.heights[y0:y1, x0:x1]
        keep = c.locked[y0:y1, x0:x1]
        sub[:] = np.where(keep, sub, half(sub * (1 - weight) + level * weight))
        return level

    def flatten(self, x0, y0, w, h, margin=6):
        Hh, Ww = self.c.codes.shape
        region = np.zeros((Hh, Ww), dtype=bool)
        region[max(0, y0):y0 + h, max(0, x0):x0 + w] = True
        return self.flatten_mask(region, margin)

    def _mask(self, ys, xs):
        m = np.zeros(self.c.codes.shape, dtype=bool)
        m[ys, xs] = True
        return m

    def settlement(self, sid, box_, level, floor=',', wall='H', thick=2):
        """A walled, levelled box (realm.settlement's walls, without its shore and gate search)."""
        c = self.c
        x, y, w, h = box_
        region = np.zeros(c.codes.shape, dtype=bool)
        region[y:y + h, x:x + w] = True
        inner = region.copy()
        inner[y + thick:y + h - thick, x + thick:x + w - thick] = False
        c.paint(region, floor, level)
        c.paint(inner, wall, level)
        self.places[sid] = region
        self.levels[sid] = level
        return region

    def route_costs(self):
        """What a road pays to cross each 4-tile square (realm.route_costs on this canvas)."""
        s = self.STEP
        Hh, Ww = self.c.codes.shape
        ny, nx = Hh // s, Ww // s
        coarse = lambda a: a.reshape(ny, s, nx, s).mean(axis=(1, 3))
        wet = coarse((self.water & (self.lake | self.ocean)).astype(np.float32))
        river = coarse((self.water & ~(self.lake | self.ocean)).astype(np.float32))
        h = coarse(self.c.heights)
        gy, gx = np.gradient(h)
        slope = np.hypot(gx, gy) / s
        shore = grow(wet > 0, 2) & ~(wet > 0)
        cost = 1 + 90 * wet + 20 * river + 3 * shore + 60 * slope ** 2 + .25 * np.maximum(0, h - 5)
        blocked = np.zeros((ny, nx), dtype=bool)
        for region in self.places.values():
            blocked |= coarse(region.astype(np.float32)) > 0
        if self.no_road is not None:
            blocked |= coarse(self.no_road.astype(np.float32)) > 0
        cost[blocked] = np.inf
        return cost

    def route(self, a, b, cost):
        """The cheapest way from a to b over the grid (A*), as canvas points, its corners rounded."""
        s = self.STEP
        ny, nx = cost.shape
        start = (min(ny - 1, int(a[1]) // s), min(nx - 1, int(a[0]) // s))
        goal = (min(ny - 1, int(b[1]) // s), min(nx - 1, int(b[0]) // s))

        def estimate(node):
            dy, dx = abs(node[0] - goal[0]), abs(node[1] - goal[1])
            return max(dx, dy) + .41 * min(dx, dy)
        open_ = [(estimate(start), 0.0, start)]
        came, spent = {start: None}, {start: 0.0}
        steps = [(-1, 0, 1), (1, 0, 1), (0, -1, 1), (0, 1, 1), (-1, -1, 1.41), (-1, 1, 1.41), (1, -1, 1.41), (1, 1, 1.41)]
        while open_:
            _, g, node = heapq.heappop(open_)
            if node == goal:
                break
            if g > spent[node]:
                continue
            for dy, dx, length in steps:
                nxt = (node[0] + dy, node[1] + dx)
                if not (0 <= nxt[0] < ny and 0 <= nxt[1] < nx):
                    continue
                here = cost[nxt] if nxt != goal else 1.0
                if not np.isfinite(here):
                    continue
                step = length * (here + (cost[node] if np.isfinite(cost[node]) else 1.0)) / 2
                if g + step < spent.get(nxt, np.inf):
                    spent[nxt] = g + step
                    came[nxt] = node
                    heapq.heappush(open_, (g + step + estimate(nxt), g + step, nxt))
        if goal not in came:
            return [a, b]
        path, node = [], goal
        while node is not None:
            path.append((node[1] * s + s / 2, node[0] * s + s / 2))
            node = came[node]
        path.reverse()
        path[0], path[-1] = a, b
        for _ in range(3):
            smooth = [path[0]]
            for (x1, y1), (x2, y2) in zip(path, path[1:]):
                smooth += [(.75 * x1 + .25 * x2, .75 * y1 + .25 * y2), (.25 * x1 + .75 * x2, .25 * y1 + .75 * y2)]
            path = smooth + [path[-1]]
        return path

    def road(self, name, points, width=3, causeway=False, ends=(None, None)):
        """A road that climbs no faster than half a step a tile, the land eased into it (realm.road on this canvas)."""
        c = self.c
        Hh, Ww = c.codes.shape
        samples = []
        for (x1, y1), (x2, y2) in zip(points, points[1:]):
            n = max(1, int(math.hypot(x2 - x1, y2 - y1) * 2))
            for i in range(n):
                t = i / n
                samples.append((x1 + (x2 - x1) * t, y1 + (y2 - y1) * t))
        samples.append(points[-1])
        xy = np.array(samples, dtype=np.float32)
        ix = np.clip(xy[:, 0].astype(int), 0, Ww - 1)
        iy = np.clip(xy[:, 1].astype(int), 0, Hh - 1)
        ground = c.heights[iy, ix].astype(np.float32)
        ground[self.water[iy, ix]] = 0
        kernel = np.ones(81, dtype=np.float32) / 81
        profile = np.convolve(np.pad(ground, 40, mode='edge'), kernel, mode='valid')
        profile[0], profile[-1] = ground[0], ground[-1]
        fixed = np.zeros(len(profile), dtype=bool)
        for level, part in zip(ends, (slice(0, 16), slice(-16, None))):
            if level is not None:
                profile[part] = level
                fixed[part] = True
        step = .45 * .5
        for _ in range(3):
            for i in range(1, len(profile)):
                if not fixed[i]:
                    profile[i] = np.clip(profile[i], profile[i - 1] - step, profile[i - 1] + step)
            for i in range(len(profile) - 2, -1, -1):
                if not fixed[i]:
                    profile[i] = np.clip(profile[i], profile[i + 1] - step, profile[i + 1] + step)
        mask = c.line_mask(samples[::2] + [samples[-1]], width)
        reach = int(width / 2 + 16)
        best = np.full((Hh, Ww), 1e9, dtype=np.float32)
        hts = np.zeros((Hh, Ww), dtype=np.float32)
        for k in range(0, len(xy), 2):
            x0, y0 = max(0, int(xy[k, 0]) - reach), max(0, int(xy[k, 1]) - reach)
            x1, y1 = min(Ww, int(xy[k, 0]) + reach + 1), min(Hh, int(xy[k, 1]) + reach + 1)
            yy, xx = np.mgrid[y0:y1, x0:x1]
            d = (xx + .5 - xy[k, 0]) ** 2 + (yy + .5 - xy[k, 1]) ** 2
            b_, bh = best[y0:y1, x0:x1], hts[y0:y1, x0:x1]
            better = d < b_
            b_[better] = d[better]
            bh[better] = profile[k]
        near = best < reach * reach
        dist = np.sqrt(best[near])
        blend = 1 - field.smoothstep(width / 2, reach, dist)
        free = ~c.locked[near]
        old = c.heights[near]
        c.heights[near] = np.where(free, half(old * (1 - blend) + hts[near] * blend), old)
        paint = mask & ~c.locked
        bridge = paint & self.water
        c.codes[paint & ~self.water] = ord('d')
        c.codes[bridge] = ord('8')
        c.heights[bridge] = .5
        c.locked |= paint
        self.road_lines[name] = [tuple(map(float, p)) for p in points]
        if causeway:
            self.causeway |= bridge
        return mask

    def mark_cliffs(self):
        """Wherever a drop cannot be walked, the higher tile is a cliff face (unless something was built there)."""
        c = self.c
        h = c.heights
        higher = np.zeros(c.codes.shape, dtype=bool)
        higher[:, :-1] |= h[:, :-1] - h[:, 1:] > 1
        higher[:, 1:] |= h[:, 1:] - h[:, :-1] > 1
        higher[:-1, :] |= h[:-1, :] - h[1:, :] > 1
        higher[1:, :] |= h[1:, :] - h[:-1, :] > 1
        ramp = np.isin(c.codes, [ord(':'), ord('^'), ord('G')])
        c.codes[higher & ~c.locked & ~self.water & ~ramp] = ord('%')

    def cells(self, names):
        """The 12 cells: ids w_ridgemere_<cx>_<cy>, weather rain, region ridgemere (doc 67, section 2)."""
        out = []
        for cy in range(H // CELL):
            for cx in range(W // CELL):
                x0, y0 = cx * CELL, cy * CELL
                rows, heights = self.c.cell(x0, y0, CELL, CELL)
                name, blurb = names.get((cx, cy), ('Ridgemere', ''))
                out.append({'id': cell_id(cx, cy), 'name': name, 'description': blurb, 'x': x0, 'y': y0,
                            'width': CELL, 'height': CELL, 'z': 0, 'outdoors': True, 'weather': 'rain',
                            'territory': {'region': REGION, 'claims': [], 'chapter': ''},
                            'terrain': rows, 'heights': heights})
        return out


CELL_NAMES = {
    (0, 0): ('Ridgemere Bay, North Headland', 'The north headland of Ridgemere Bay: the Mole running out into the bay to '
             'the Bay Light, the abbey\'s wall over the wharf\'s north end, and the Hundred Steps climbing from the quay.'),
    (1, 0): ('Ridgemere, Northgate', 'Northgate under the North Gate: tight middle-town streets round the Clock Tower, '
             'and House Greatly\'s walled compound in the corner, the Wall-Builders on Greatly Square below its gate.'),
    (2, 0): ('The Rainward Moor, North', 'Wet heath and moor north-east of the city walls.'),
    (0, 1): ('Ridgemere, the Wharf and the Sump', 'The wharf inside the sea wall, its piers and cranes; behind it the '
             'Sump\'s alleys, the Rag Market, the Cisterns, and the old hulk the Mercy hauled up on the quay; the Cinders\' '
             'smoke to the south.'),
    (1, 1): ('Ridgemere, the Commons and the Rise', 'The middle town climbing east from the Sump: Market Square and its '
             'Hall, the Records Office, the Great Palace, the Old Keep over Pillory Square, and the Rise\'s railed '
             'mansions under the South Gate.'),
    (2, 1): ('Fellhall', 'House Fell\'s estate on the East Road: the hall, its press house, distillery and cellars.'),
    (0, 2): ('Grayrock Hall', 'House Grayrock\'s hall on the south headland, at the end of the Wharf Road.'),
    (1, 2): ('Timberland Court', 'House Timberland\'s court on the moor south of the city, at the end of the Cinder '
             'Road.'),
    (2, 2): ('The Rainward Moor, South', 'Wet heath and moor south-east of the city walls.'),
}


# ---------------------------------------------------------------------------------------------------------------
# Reused interiors: the old city's buildings, by their ground floor
# ---------------------------------------------------------------------------------------------------------------
def placements():
    return json.loads((ART / 'old-map-placements.json').read_text(encoding='utf-8'))


def structures(project, old):
    """The world's buildings as structures (doc 67, section 3): {ground floor id: {rooms, door, size}}, from its
    interiors, their stairs, and the old map's doors (old-map-placements.json)."""
    rooms = {r['id']: r for r in project['rooms']}
    adj = defaultdict(set)
    for link in project['links']:
        a, b = link['a']['cell'], link['b']['cell']
        if a in rooms and b in rooms:
            adj[a].add(b)
            adj[b].add(a)
    doors = {}
    for d in old['doors']:
        doors.setdefault(d['b_area'], d)
    out, seen = {}, set()
    for rid in rooms:
        if rid in seen:
            continue
        group, todo = [], [rid]
        while todo:
            x = todo.pop()
            if x in seen:
                continue
            seen.add(x)
            group.append(x)
            todo += sorted(adj[x])
        ground = next((g for g in group if g in doors), None)
        if ground is None:
            continue
        d = doors[ground]
        r = rooms[ground]
        out[ground] = {'ground': ground, 'rooms': sorted(group, key=lambda g: (g != ground, g)), 'door': d,
                       'size': (r['width'], r['height']), 'name': r['name']}
    return out


def beds_in(room):
    return [(x, y) for y, row in enumerate(room['terrain']) for x, ch in enumerate(row) if ch in 'bz']


# What each reused building is (doc 67, section 3): ground floor id -> (district, kind, anchor).
# Kinds: home (a house), shop (a shop with its flat), tavern, inn, works, hall (an office or institution), store,
# lodging, seat, villa. Anchors are canvas points the building is placed near.
REUSE = {
    # The wharf (House Grayrock's).
    'rm_the_barracks': ('wharf', 'lodging', (205, 300)),
    'rm_the_greatly_bonded_warehouse': ('wharf', 'store', (185, 420)),
    'rm_the_fell_timber_store': ('wharf', 'store', (185, 530)),
    'rm_the_ropewalk': ('wharf', 'works', (180, 380)),
    'rm_the_fish_hall': ('wharf', 'works', (205, 250)),
    'rm_the_north_wharf_smokehouse': ('wharf', 'works', (185, 230)),
    'rm_the_wharf_tannery': ('wharf', 'works', (205, 600)),
    'rm_the_harbourmaster_s_office': ('wharf', 'hall', (205, 455)),
    'rm_wickham_s_chandlery': ('wharf', 'shop', (205, 360)),
    'rm_the_bung_hole': ('wharf', 'shop', (205, 400)),
    'rm_the_sailors_rest': ('wharf', 'inn', (185, 290)),
    'rm_the_drowned_lantern': ('wharf', 'tavern', (205, 330)),
    'rm_the_tarred_rope': ('wharf', 'tavern', (185, 180)),
    # The Cinders: Timberland's forges, Greatly's kilns and plaster, the mills.
    'rm_the_grayrock_ironworks': ('cinders', 'works', (240, 560)),
    'rm_the_grayrock_foundry': ('cinders', 'works', (260, 560)),
    'rm_the_fell_sawmill': ('cinders', 'works', (280, 600)),
    'rm_the_charcoal_store': ('cinders', 'works', (300, 580)),
    'rm_the_grayrock_iron_store': ('cinders', 'store', (235, 600)),
    'rm_nailers_forge': ('cinders', 'shop', (240, 520)),
    'rm_the_anchor_forge': ('cinders', 'shop', (250, 500)),
    'rm_the_rivet_and_rain': ('cinders', 'shop', (300, 545)),
    'rm_the_greatly_plasterworks': ('cinders', 'works', (300, 600)),
    'rm_the_lime_kilns': ('cinders', 'works', (305, 560)),
    'rm_the_tileworks': ('cinders', 'works', (280, 540)),
    'rm_the_cinder_glassworks': ('cinders', 'works', (240, 590)),
    'rm_the_rag_mill': ('cinders', 'works', (230, 540)),
    'rm_the_rainwater_brewery': ('cinders', 'works', (265, 590)),
    'rm_the_slag_anchor': ('cinders', 'tavern', (245, 485)),
    'rm_forge_row': ('cinders', 'home', (285, 560)),
    'rm_kiln_row': ('cinders', 'home', (270, 575)),
    # The Sump.
    'rm_sump_sundries': ('sump', 'shop', (300, 340)),
    'rm_gullgut_fish': ('sump', 'shop', (270, 355)),
    'rm_harbour_provisions': ('sump', 'shop', (240, 345)),
    'rm_mend_make_do': ('sump', 'shop', (300, 380)),
    'rm_the_pawn_and_ledger': ('sump', 'shop', (320, 355)),
    'rm_the_bilge': ('sump', 'tavern', (340, 350)),
    'rm_the_rainbarrel': ('sump', 'tavern', (292, 420)),
    'rm_the_sump_infirmary': ('sump', 'hall', (292, 300)),
    'rm_the_soup_kitchen': ('sump', 'hall', (285, 445)),
    'rm_the_chapel_of_the_tide': ('sump', 'hall', (232, 330)),
    # The Commons and Northgate: the middle town.
    'rm_bridge_stores': ('commons', 'shop', (420, 380)),
    'rm_cedar_row_joiners': ('northgate', 'shop', (470, 200)),
    'rm_clearwater_glass': ('commons', 'shop', (430, 340)),
    'rm_collar_and_trace': ('commons', 'shop', (630, 380)),
    'rm_commons_sundries': ('commons', 'shop', (440, 390)),
    'rm_greywool': ('commons', 'shop', (510, 330)),
    'rm_grist_crust': ('commons', 'shop', (400, 360)),
    'rm_hollis_physic': ('commons', 'shop', (520, 390)),
    'rm_moss_marrow': ('commons', 'shop', (530, 340)),
    'rm_mutton_lane_butchers': ('commons', 'shop', (400, 400)),
    'rm_needle_drip': ('commons', 'shop', (500, 400)),
    'rm_oilcloth_wool': ('commons', 'shop', (540, 380)),
    'rm_pennock_sons': ('commons', 'shop', (420, 320)),
    'rm_rainford_s_store': ('northgate', 'shop', (500, 230)),
    'rm_silver_rain': ('commons', 'shop', (560, 455)),
    'rm_soundings': ('commons', 'shop', (410, 420)),
    'rm_southgate_farriers': ('commons', 'shop', (645, 340)),
    'rm_southgate_ironmongers': ('commons', 'shop', (645, 385)),
    'rm_southway_provisions': ('commons', 'shop', (380, 300)),
    'rm_staves_hoops': ('commons', 'shop', (390, 430)),
    'rm_the_cedar_phial': ('commons', 'shop', (525, 455)),
    'rm_the_cough_jar': ('commons', 'shop', (420, 440)),
    'rm_the_council_press': ('commons', 'shop', (450, 420)),
    'rm_the_greatly_bank': ('commons', 'shop', (560, 330)),
    'rm_the_grey_goose': ('northgate', 'shop', (420, 220)),
    'rm_the_harness_shop': ('commons', 'shop', (620, 420)),
    'rm_the_hook_and_cleaver': ('northgate', 'shop', (440, 240)),
    'rm_the_hopyard': ('commons', 'shop', (380, 450)),
    'rm_the_lamp_wick': ('commons', 'shop', (500, 440)),
    'rm_the_morning_oven': ('commons', 'shop', (450, 300)),
    'rm_the_rain_pot': ('commons', 'shop', (480, 420)),
    'rm_the_sugar_loaf': ('northgate', 'shop', (500, 180)),
    'rm_the_wet_loaf': ('commons', 'shop', (530, 420)),
    'rm_the_wet_slab': ('commons', 'shop', (390, 380)),
    'rm_vane_daughters': ('commons', 'shop', (480, 300)),
    'rm_the_black_tun': ('commons', 'tavern', (400, 450)),
    'rm_the_last_lamp': ('commons', 'tavern', (640, 360)),
    'rm_the_grey_gull_inn': ('commons', 'inn', (560, 400)),
    'rm_the_eastgate_stables': ('commons', 'works', (640, 400)),
    'rm_the_commons_school': ('commons', 'hall', (590, 440)),
    'rm_the_house_of_mending': ('commons', 'hall', (420, 280)),
    'rm_the_greatly_yard': ('commons', 'works', (620, 300)),
    # The administrative sector.
    'rm_the_city_office': ('admin', 'hall', (430, 500)),
    'rm_the_magistrate_s_court': ('admin', 'hall', (370, 500)),
    'rm_the_hall_of_records': ('admin', 'hall', (470, 485)),
    'rm_wetink_lane_scriveners': ('admin', 'shop', (350, 485)),
    'rm_the_slate_ledger': ('admin', 'tavern', (330, 515)),
    # The Old Keep's barracks.
    'rm_the_watch_house': ('keep', 'lodging', None),
    # The Rise: the five Minor Houses kept (doc 67, Q6).
    'rm_carlisle_house': ('rise', 'villa', None),
    'rm_thornbury_house': ('rise', 'villa', None),
    'rm_vance_house': ('rise', 'villa', None),
    'rm_stanmore_house': ('rise', 'villa', None),
    'rm_grantham_house': ('rise', 'villa', None),
    # House Greatly's compound.
    'rm_greatly_house': ('greatly', 'seat', None),
    'rm_the_greatly_masons_yard': ('greatly', 'works', None),
    'rm_the_greatly_colour_works': ('greatly', 'works', None),
    'rm_the_greatly_stables': ('greatly', 'works', None),
    'rm_the_greatly_counting_house': ('greatly', 'hall', None),
    # The estates outside the walls.
    'rm_mossgrave_court': ('estate_mossgrave', 'seat', None),
    'rm_the_mossgrave_gatehouse': ('estate_mossgrave', 'home', None),
    'rm_the_mossgrave_summerhouse': ('estate_mossgrave', 'hall', None),
    'rm_the_mossgrave_stables': ('estate_mossgrave', 'works', None),
    'rm_mossgrave_cottages': ('estate_mossgrave', 'home', None),
    'rm_fellhall': ('estate_fell', 'seat', None),
    'rm_the_fell_gatehouse': ('estate_fell', 'home', None),
    'rm_the_fell_stables': ('estate_fell', 'works', None),
    'rm_fell_cottages': ('estate_fell', 'home', None),
    'rm_grayrock_hall': ('estate_grayrock', 'seat', None),
    'rm_the_grayrock_gatehouse': ('estate_grayrock', 'home', None),
    'rm_the_grayrock_summerhouse': ('estate_grayrock', 'hall', None),
    'rm_the_grayrock_stables': ('estate_grayrock', 'works', None),
    'rm_grayrock_cottages': ('estate_grayrock', 'home', None),
}
# Interiors whose House changed (doc 67, section 2, Faction claims).
CLAIMS_TO = {'rm_the_greatly_bonded_warehouse': ['house_grayrock'], 'rm_the_fell_timber_store': ['house_grayrock'],
             'rm_the_grayrock_ironworks': ['house_mossgrave'], 'rm_the_grayrock_foundry': ['house_mossgrave'],
             'rm_the_grayrock_iron_store': ['house_mossgrave'], 'rm_the_fell_sawmill': ['house_mossgrave'],
             # Timberland owns the city's forges (R29): the reused smithies, the armourer's and the charcoal store.
             'rm_the_anchor_forge': ['house_mossgrave'], 'rm_the_anchor_forge_flat': ['house_mossgrave'],
             'rm_nailers_forge': ['house_mossgrave'], 'rm_nailers_forge_flat': ['house_mossgrave'],
             'rm_the_rivet_and_rain': ['house_mossgrave'], 'rm_the_rivet_and_rain_flat': ['house_mossgrave'],
             'rm_the_charcoal_store': ['house_mossgrave'],
             # Grayrock owns the wharf (R28): its office and the ropewalk.
             'rm_the_harbourmaster_s_office': ['house_grayrock'], 'rm_the_ropewalk': ['house_grayrock']}
# Reused interiors renamed for the city (doc 67, section 3): {id: (name, description)}. Their door links take the name.
ROOM_RENAMES = {
    'rm_the_hall_of_records': ("The Couriers' Office", "The Couriers' Office: pigeonholes of letters and parcels to the "
                               "rafters, a bell on the counter, and couriers coming in wet from every quarter of the city "
                               "with the Records Office's post."),
}
# Reused shops given over to another trade by the food rebalance (doc 67, Progress, "Food rebalance"): {ground floor id:
# (trade (buildings.TRADES), new name or None, description)}. The shop keeps its rooms, door, stairs and flat; its wall of
# goods and its fixtures are refitted for the new trade (refit_shop), and the keeper's post is in POSTS.
REFIT = {
    'rm_rainford_s_store': ('provisioner', None, "Rainford's Store, provisioner. Sacks of oats and flour by the door, "
                            'salt fish and smoked rabbit hung from the beams: what a traveller wants before the north '
                            'road, and what Northgate wants for its supper.'),
    'rm_the_grey_goose': ('cook', None, "The Grey Goose, cookshop. A hearth at each end, a long counter and a pot of stew "
                          "that is never quite empty: Northgate's suppers, eaten standing or carried home in a crock."),
    'rm_the_bung_hole': ('provisioner', None, "The Bung Hole, ship's provisioner. Casks of salt pork, sacks of oats and "
                         'strings of smoked fish for the Grayrock crews, sold over a counter worn smooth by a hundred '
                         'sailing days.'),
    'rm_moss_marrow': ('butcher', None, 'Moss & Marrow, butcher. Marrow bones, mutton and rabbit on hooks, sausages in '
                       'loops, and a block scrubbed white every evening.'),
    'rm_vane_daughters': ('cheesemaker', None, 'Vane & Daughters, cheesemongers. Rounds ripening on slatted shelves, '
                          'butter in crocks, and the press creaking in the back.'),
    'rm_the_cough_jar': ('preserver', 'The Honey Crock', 'The Honey Crock, preserving shop. Preserving pans on the '
                         'hearth, crocks of apple jam sealed with wax, and apple rings drying on racks by the window.'),
    'rm_oilcloth_wool': ('preserver', 'The Apple Loft', 'The Apple Loft, preserving shop. Dried apples in strings, '
                         'preserves in stone jars, and the sweet smoke of the drying racks over everything.'),
    'rm_collar_and_trace': ('baker', 'The Eastgate Bakehouse', 'The Eastgate Bakehouse, bakery. Loaves for the carters '
                            'and the gate before dawn, honey cakes by noon, and an oven that warms the street outside.'),
    'rm_commons_sundries': ('provisioner', 'Commons Provisions', 'Commons Provisions, provisioner. Flour and oats by '
                            'the sack, smoked fish and salt pork by the pound, milk in the mornings.'),
}


class Spec:
    """A building to place: a new one (`make`: a Building) or a reused structure (`reuse`), with where it goes."""

    def __init__(self, key, name, district, kind, make=None, reuse=None, near=None, homes=0, style='ridgemere',
                 open_door=True, claims=None, business=None):
        self.key, self.name, self.district, self.kind = key, name, district, kind
        self.make, self.reuse, self.near = make, reuse, near
        self.homes, self.style, self.open_door = homes, style, open_door
        self.claims, self.business = claims, business
        self.scale = .72                  # (a reused building's block: its interior's size times this)
        self.building = make() if make else None

    @property
    def footprint(self):
        if self.building is not None:
            return self.building.footprint
        return fp(*self.reuse['size'], scale=self.scale)

    @property
    def area(self):
        fw, fh = self.footprint
        return fw * fh


# ---------------------------------------------------------------------------------------------------------------
# The city
# ---------------------------------------------------------------------------------------------------------------
class RidgemereCity(Ridgemere):
    """Ridgemere 2.0 inside its walls (doc 67, sections 2 and 3, and its Dense city notes)."""

    def __init__(self, world, site, project, old):
        super().__init__(world, site)
        # (Ridgemere's own seed and prefix are the old map's; this city's are its own.)
        self.rng = random.Random(world.seed)
        self.new_rng = random.Random(world.seed * 7 + 67)
        self.prefix = 'r2_'
        self.project, self.old = project, old
        self.rooms = {r['id']: r for r in project['rooms']}
        self.structures = structures(project, old)
        self.records = []                 # every placed structure: manifest records
        self.spots = {}                   # named outdoor places (canvas x, y)
        self.posts = defaultdict(list)    # outdoor work posts by kind: [(x, y)]
        self.named = {}                   # key -> manifest record
        self.passages = []
        self.sump_stats = {}
        self.yards_count = 0
        self.structure_district = {}
        self.street = np.zeros((H, W), dtype=bool)
        self.edges = np.zeros((H, W), dtype=bool)     # (no retaining edges any more: kept for the lot finders)
        self.stairs_mask = np.zeros((H, W), dtype=bool)
        self.mole = np.zeros((H, W), dtype=bool)
        self.infill = Counter()
        self.landmarks = []               # [{'name', 'where', 'what', 'x', 'y', 'interiors'}]
        self.stitches = Counter()
        self.unplaced = []                # specs that found no lot (a check fails on any)
        self.gate_inside = {'N': (360, 159), 'E': (462, 300), 'S': (412, 446), 'harbour': (190, 446),
                            'cinder': (240, 446), 'customs': (179, CUSTOMS_PIER + 3)}

    # -- helpers ---------------------------------------------------------------------------------------------------
    def D(self, name):
        return self.district == DISTRICT_IDS.index(name)

    def paint_line(self, pts, width, ch, height=None, within=None):
        m = self.c.line_mask(pts, width)
        if within is not None:
            m &= within
        self.c.paint(m, ch, height)
        self.street |= m
        return m

    def paint_box(self, x0, x1, y0, y1, ch, height=None, street=True):
        m = box(x0, x1, y0, y1)
        self.c.paint(m, ch, height)
        if street:
            self.street |= m
        return m

    def level_at(self, x, y):
        return float(self.c.heights[int(y), int(x)])

    def landmark(self, name, where, what, x, y, interiors=()):
        self.landmarks.append({'name': name, 'district': where, 'what': what, 'x': int(x), 'y': int(y),
                               'interiors': list(interiors)})

    # -- placing a building ------------------------------------------------------------------------------------------
    def put(self, spec, x0, y0, facing, w=None, h=None, mask=None):
        """Place a spec at (x0, y0) with its door on `facing`; w, h: the block as placed (a reused building's). The block
        stands at the height of the ground outside its door."""
        c = self.c
        if spec.building is not None:
            b = spec.building
            b.district = TALK_DISTRICT.get(spec.district, spec.district)
            fw_, fh_ = footprint_size(b, facing)
            _, (ox, oy) = door_tile(x0, y0, fw_, fh_, facing)
            ground = self.level_at(ox, oy)
            bid, outside = self.site.place(b, x0, y0, facing, REGION, open_door=spec.open_door, mask=mask,
                                           ground=ground)
            record = self.site.manifest[-1]
            w, h = fw_, fh_
        else:
            g = spec.reuse
            (dx, dy), outside = door_tile(x0, y0, w, h, facing)
            ground = self.level_at(*outside)
            style = STYLES[spec.style]
            c.rect(x0, y0, w, h, style['outer'], ground)
            c.rect(x0 + 1, y0 + 1, w - 2, h - 2, style['roof'][0], ground)
            c.codes[dy, dx] = ord('+')
            cid, lx, ly = self.site.cell_at(dx, dy)
            d = g['door']
            # Named from the interior as the world has it now (so a rename, doc 67 lane C, survives a rewrite); every
            # old door was named "<its ground room> door".
            self.site.links.append({'id': d['id'], 'name': f"{g['name']} door", 'kind': 'door',
                                    'open': spec.kind not in ('home', 'villa', 'seat', 'lodging'),
                                    'a': {'cell': cid, 'x': lx, 'y': ly}, 'b': {'cell': g['ground'], 'x': d['b_x'], 'y': d['b_y']}})
            bid = g['ground']
            record = {'id': bid, 'name': g['name'], 'kind': spec.kind, 'district': TALK_DISTRICT.get(spec.district, spec.district),
                      'style': spec.style, 'trade': '', 'door': {'x': dx, 'y': dy}, 'facing': facing, 'reused': True,
                      'rooms': []}
            for rid in g['rooms']:
                r = self.rooms[rid]
                record['rooms'].append({'id': rid, 'name': r['name'], 'z': r.get('z', 0),
                                        'beds': [{'x': x, 'y': y} for x, y in beds_in(r)], 'work': [], 'stations': []})
                r['territory'] = {**r.get('territory', {}), 'region': REGION}
                r['territory'].setdefault('claims', [])
                r['territory'].setdefault('chapter', '')
                if rid in CLAIMS_TO:
                    r['territory']['claims'] = list(CLAIMS_TO[rid])
                elif spec.claims:
                    r['territory']['claims'] = list(spec.claims)
                r['worldX'], r['worldY'] = dx + 2 * len(record['rooms']), dy
            self.site.manifest.append(record)
        record['key'] = spec.key
        record['claims'] = list(spec.claims) if spec.claims else None     # (compose's claims for the new rooms)
        record['block'] = {'x': x0, 'y': y0, 'w': w, 'h': h}
        record['outside'] = {'x': int(outside[0]), 'y': int(outside[1])}
        record['area'] = spec.district
        record['business'] = spec.business
        record['homes'] = spec.homes
        record['kindOf'] = spec.kind
        self.built[y0:y0 + h, x0:x0 + w] = True if mask is None else (self.built[y0:y0 + h, x0:x0 + w] | mask)
        self.records.append(record)
        self.named[spec.key] = record
        self.structure_district[bid] = spec.district
        return record

    def fit(self, lots, spec, near, street, orient=True, facings='SNEW', deep=False, back=False):
        """The nearest lot for a spec (both ways round for a reused building, whose facing is cosmetic)."""
        fw, fh = spec.footprint
        shapes = [(fw, fh)]
        if spec.building is None and orient and fw != fh:
            shapes.append((fh, fw))
        best = None
        for sw, sh in shapes:
            got = self.nearest_lot(lots, sw, sh, near, street, facings=facings, deep=deep, back=back)
            if got and (best is None or got[0] < best[0]):
                best = got
        return best

    def nearest_lot(self, lots, w_ns, h_ns, near, street, deep=False, facings='SNEW', back=False):
        """The nearest clear footprint whose door opens onto the street, in the same cell (`deep`: onto a passage at
        least two wide; `back`: a back door on the far side onto a street too). (d, x0, y0, facing, w, h) or None."""
        ys, xs = np.nonzero(lots.street & street)
        if not len(xs):
            return None
        best = None
        s = lots.sum
        Hh, Ww = lots.open.shape
        for facing in facings:
            w, h = (w_ns, h_ns) if facing in 'NS' else (h_ns, w_ns)
            sx, sy = DOOR_STEP[facing]
            dx, dy = xs - sx, ys - sy
            if facing == 'S':
                x0, y0 = dx - w // 2, dy - (h - 1)
            elif facing == 'N':
                x0, y0 = dx - w // 2, dy
            elif facing == 'E':
                x0, y0 = dx - (w - 1), dy - h // 2
            else:
                x0, y0 = dx, dy - h // 2
            ok = (x0 >= 1) & (y0 >= 1) & (x0 + w <= Ww - 1) & (y0 + h <= Hh - 1)
            x0, y0, dxo, dyo, ox, oy = x0[ok], y0[ok], dx[ok], dy[ok], xs[ok], ys[ok]
            area = s[y0 + h, x0 + w] - s[y0, x0 + w] - s[y0 + h, x0] + s[y0, x0]
            ok = (area == w * h) & (dxo // CELL == ox // CELL) & (dyo // CELL == oy // CELL)
            if deep:
                px, py = ox + sx, oy + sy
                inb = (px >= 0) & (py >= 0) & (px < Ww) & (py < Hh)
                ok &= inb
                ok[inb] &= street[py[inb], px[inb]]
            if back:                                     # The far side's middle, one step out, is a street too.
                if facing == 'S':
                    bx, by, tx, ty = x0 + w // 2, y0 - 1, x0 + w // 2, y0
                elif facing == 'N':
                    bx, by, tx, ty = x0 + w // 2, y0 + h, x0 + w // 2, y0 + h - 1
                elif facing == 'E':
                    bx, by, tx, ty = x0 - 1, y0 + h // 2, x0, y0 + h // 2
                else:
                    bx, by, tx, ty = x0 + w, y0 + h // 2, x0 + w - 1, y0 + h // 2
                inb = (bx >= 0) & (by >= 0) & (bx < Ww) & (by < Hh)
                ok &= inb
                ok[inb] &= street[by[inb], bx[inb]] & (bx[inb] // CELL == tx[inb] // CELL) & (by[inb] // CELL == ty[inb] // CELL)
            if not ok.any():
                continue
            x0, y0 = x0[ok], y0[ok]
            d = np.hypot(x0 + w / 2 - near[0], y0 + h / 2 - near[1])
            i = int(np.argmin(d))
            if best is None or d[i] < best[0]:
                best = (float(d[i]), int(x0[i]), int(y0[i]), facing, w, h)
        return best

    def place_list(self, lots, specs, street, jitter=6, gap=None, facings='SNEW', deep=False, report=True):
        missing = []
        for spec in specs:
            near = spec.near or self.anchor(spec.district)
            near = (near[0] + self.rng.uniform(-jitter, jitter), near[1] + self.rng.uniform(-jitter, jitter))
            got = self.fit(lots, spec, near, street, facings=facings, deep=deep,
                           back=bool(spec.building is not None and getattr(spec.building, 'back', False)))
            if got is None:
                missing.append(spec)
                if report:
                    print(f'  ridgemere2: no lot for {spec.name} ({spec.district})')
                continue
            _, x0, y0, facing, w, h = got
            self.put(spec, x0, y0, facing, w, h)
            if gap is not None:
                lots.gap = gap
            lots.take(x0, y0, w, h)
        return missing

    def anchor(self, district):
        ys, xs = np.nonzero(self.D(district))
        return float(xs.mean()), float(ys.mean())

    # -- new buildings ---------------------------------------------------------------------------------------------
    def multi(self, kind, name, plans, stairs, keys, zs, names, texts, style, footprint=None, district='', trade=''):
        """A building of several floors from plans already joined by stairs."""
        rooms = [p.room(k, n, t, z=z) for p, k, z, n, t in zip(plans, keys, zs, names, texts)]
        fw_fh = footprint or fp(plans[0].w, plans[0].h)
        return Building(kind, name, style, fw_fh, rooms, list(stairs), district, STYLE_ROOF.get(style, ''), trade)

    def one(self, kind, name, plan, text, style, footprint=None, trade=''):
        return Building(kind, name, style, footprint or fp(plan.w, plan.h), [plan.room('', name, text)], [], '',
                        STYLE_ROOF.get(style, ''), trade)

    def new_hall(self, hkind, name, text, w, h, style='ridgemere', footprint=None):
        return self.one('hall', name, hall(self.new_rng, style, w, h, hkind), text, style, footprint)

    def new_works(self, kind, name, text, hands=3, style='works', size=None, footprint=None):
        from .buildings import WORKS
        p = works_kit(self.new_rng, style, kind, hands, size)
        b = self.one('works', name, p, f'{name}: {WORKS[kind][1]}. {text}', style, footprint, trade=kind)
        return b

    def new_shop(self, trade, name, text, style='ridgemere', people=None):
        rooms, stairs, (w, h) = shop_with_flat(self.new_rng, style, trade, name, text, people=people)
        return Building('shop', name, style, fp(w, h), rooms, stairs, '', '', trade)

    def own_shop(self, trade, name, text, style='ridgemere'):
        """A shop with its flat drawn from its own random sequence (so adding it moves nothing else): (footprint, rooms,
        stairs) for a Building."""
        rooms, stairs, (w, h) = shop_with_flat(random.Random(f'{name}:shop'), style, trade, name, text)
        return fp(w, h), rooms, stairs

    def new_tavern(self, name, text, style='ridgemere', footprint=None):
        plans, _ = tavern(self.new_rng, style, False)
        return Building('tavern', name, style, footprint or fp(plans[0].w, plans[0].h),
                        [plans[0].room('', name, text)], [], '')

    def new_flats(self, name, homes, beds, size, footprint, notch=None, back=False, style='ridgemere', prose='',
                  texts=None):
        plans, pairs = flats(self.new_rng, style, homes, beds, size, notch, back)
        keys, zs = ['', 'up', 'top'][:homes], [0, 1, 2][:homes]
        floors = ['the ground floor', 'the first floor', 'the top floor']
        names = [name if homes == 1 else f'{name}, {floors[i]}' for i in range(homes)]
        texts = texts or [prose or self.new_rng.choice(FLATS_PROSE) for _ in range(homes)]
        b = self.multi('flats' if homes > 1 else 'house', name, plans, pairs, keys, zs, names, texts, style, footprint)
        b.back = back
        return b

    def new_house(self, name, people, text, style='ridgemere', footprint=None):
        """A cottage or a single house: one home (flats() with one floor)."""
        return self.new_flats(name, 1, (max(2, (people + 1) // 2),), (10, 8), footprint, prose=text, style=style)

    # -- the layout -----------------------------------------------------------------------------------------------------
    def build(self, houses):
        """Lay out the city. `houses`: {district: [reused house ids]} (the people's step chooses them)."""
        self.houses = houses
        self.lay_districts()
        self.lay_heights()
        self.lay_wall()
        self.lay_streets()
        self.reserved = grow(self.wall_band, 1) | self.rings
        self.waterfront_map()
        self.abbey()
        self.greatly_compound()
        self.old_keep()
        self.palace()
        self.market_square()
        self.rise()
        self.fixed_landmarks()
        self.wharf()
        # 1. The large buildings of each quarter on its avenues, and the Sump's own on the Gut and Tarry Lane.
        small = {}
        small['admin'] = self.place_large('admin', self.admin_specs(), 120)
        small['cinders'] = self.place_large('cinders', self.cinders_specs(), 90)
        self.food_addition('cinders')
        com, north = self.commons_specs()
        small['commons'] = self.place_large('commons', com, 130)
        self.food_addition('commons')
        small['northgate'] = self.place_large('northgate', north, 130)
        self.sump_named()
        # 2. Lanes and alleys, then the borders stitched across by many passages.
        self.lanes_in(self.ground_of('admin'), (34, 26), 3, 0.0, '_')
        self.lanes_in(self.ground_of('cinders'), (30, 22), 3, .05, '_')
        self.lanes_in(self.ground_of('commons'), (30, 19), 3, -.06, '_')
        self.lanes_in(self.ground_of('northgate'), (30, 19), 3, -.06, '_')
        self.sump_alleys()
        self.stitch_borders()
        self.connect_streets()
        self.sunken_second_door()
        for where in ('commons', 'northgate'):          # The Tallow Arcade: a lot with a street at each end.
            lots = Lots(self.ground_of(where), self.street, gap=0)
            if not self.place_list(lots, [self.arcade_spec], self.street, jitter=4, report=False):
                break
            self.arcade_spec.near = None
        # 3. The rest of each quarter, terraced along its lanes.
        for name in ('admin', 'cinders', 'commons', 'northgate'):
            self.place_small(name, small[name])
        if 'r2_tallow_arcade' in self.named:
            rec = self.named['r2_tallow_arcade']
            self.landmark('The Tallow Arcade', 'commons', 'a covered arcade, a through-building of pillars and benches '
                          'between two streets of the Commons on the Sump\'s edge, out of the rain',
                          rec['door']['x'], rec['door']['y'], [r['id'] for r in rec['rooms']])
        self.commons_rows()
        self.sump_homes()
        # 4. What is left: yards, sheds and the ground; then the stairs where streets climb, and the doors' sills.
        for name, grass, extra in (('admin', 'f', (('S', .002), ('I', .002), ('B', .01))),
                                   ('cinders', '>', (('x', .015), ('O', .01), ('<', .012), ('7', .004))),
                                   ('commons', '.', (('Y', .006), ('B', .01), ('3', .006), ('O', .006))),
                                   ('northgate', '.', (('Y', .006), ('B', .01), ('3', .006))),
                                   ('rise', ',', (('Y', .02), ('B', .02), ('3', .02)))):
            self.infill_yards(name, grass, extra)
        self.paint_stairs()
        self.outside()
        self.fix_doors()
        self.patrols()
        return self

    # -- districts and the ground --------------------------------------------------------------------------------------
    def lay_districts(self):
        """The districts partition the inside of the walls, but none is a rectangle: their borders are drawn on
        coordinates warped by noise (up to 14 tiles), so the Sump, the Commons, the Rise and the rest run into each
        other irregularly. Only the walled pieces (the abbey, the Greatly compound, the Keep), the palace and the
        squares keep their shapes."""
        c, seed = self.c, self.world.seed
        ys, xs = np.mgrid[0:H, 0:W]
        A = 14
        X = xs + (field.fbm(W, H, 34, seed % 1000 + 21, 3) - .5) * 2 * A * 1.6
        Y = ys + (field.fbm(W, H, 34, seed % 1000 + 47, 3) - .5) * 2 * A * 1.6
        idx = DISTRICT_IDS.index
        d = np.full((H, W), idx('commons'), dtype=np.int8)
        d[(X < 298) & (Y < 346)] = idx('sump')
        d[(X >= 292) & (X < 396) & (Y < 216)] = idx('northgate')
        cinders = ((Y >= 372) & (X < 244)) | ((Y >= 392) & (X < 290))
        d[(Y >= 346) & (X < 382) & ~cinders] = idx('admin')
        d[cinders] = idx('cinders')
        d[(Y >= 392) & (X >= 290) & (X < 382)] = idx('palace')
        d[(Y >= 350) & (X >= 382)] = idx('rise')
        fixed = [('sump', box(QUAY_X, QUAY_X + 6, 218, 372)), ('wharf', box(172, QUAY_X, 154, 452)), ('abbey', box(*ABBEY)), ('greatly', box(*GREATLY)),
                 ('keep', box(*KEEP)), ('palace', box(*PALACE)), ('commons', box(334, 386, 258, 316)),
                 ('sump', box(232, 257, 294, 311)), ('admin', box(316, 345, 348, 381)), ('commons', box(417, 444, 225, 242)),
                 ('commons', box(400, 425, 304, 320)), ('northgate', box(350, 375, 175, 196)),
                 ('northgate', box(290, 300, 154, 205))]
        for x, y, w_, h_ in RISE_LOTS:
            fixed.append(('rise', box(x - 2, x + w_ + 2, y - 2, y + h_ + 2)))
        for name, m in fixed:
            d[m] = idx(name)
        self.inside = box(*INSIDE)
        d[~self.inside] = 0
        # Small islands of one district inside another (the noise's crumbs) join their neighbours.
        for name in DISTRICT_IDS[1:-1]:
            m = d == idx(name)
            ll, inf = label(m)
            for k, (x0, x1, y0, y1, size) in inf.items():
                if size < 60:
                    comp = ll == k
                    ring_ = grow(comp, 1) & ~comp & (d > 0)
                    if ring_.any():
                        d[comp] = Counter(d[ring_].tolist()).most_common(1)[0][0]
        for name, m in fixed:
            d[m & self.inside] = idx(name)
        self.district = d
        # How well-off the ground is, blurred over the borders: picks what is built in the band where two meet.
        wealth = np.zeros((H, W), dtype=np.float32)
        for name, v in WEALTH.items():
            wealth[d == idx(name)] = v
        self.wealth = blur(wealth, 9)
        # The walled pieces inside the walls.
        self.rings = np.zeros((H, W), dtype=bool)
        ax0, ax1, ay0, ay1 = ABBEY
        self.abbey_ring = box(*ABBEY) & ~box(ax0 + 2, ax1 - 2, ay0, ay1 - 2)
        gx0, gx1, gy0, gy1 = GREATLY
        self.greatly_ring = box(*GREATLY) & ~box(gx0 + 2, gx1, gy0, gy1 - 2)
        kx0, kx1, ky0, ky1 = KEEP
        ring = box(*KEEP) & ~box(kx0 + 3, kx1 - 3, ky0 + 3, ky1 - 3)
        for tx, ty in ((kx0, ky0), (kx1 - 6, ky0), (kx0, ky1 - 6), (kx1 - 6, ky1 - 6)):
            ring |= box(tx, tx + 6, ty, ty + 6)
        self.keep_ring = ring
        self.rings = self.abbey_ring | self.greatly_ring | self.keep_ring

    def lay_heights(self):
        """The ground climbs from the bay eastward, as Seattle's does: about half a step every 40 tiles, with low hills
        on it, and no step on a district's border more than anywhere else. Squares, the palace and the walled pieces are
        levelled, and the ground eases into them (never more than 0.12 a tile), so nowhere outdoors is a step more than
        half a tile: the climbs are slopes and stair-streets, never walls."""
        c, seed = self.c, self.world.seed
        ys, xs = np.mgrid[0:H, 0:W]
        t = np.clip((xs - 222 + .0025 * (ys - 300) ** 2) / 236, 0, 1)   # (contours bow, across the borders)
        hills = field.fbm(W, H, 70, seed % 991 + 11, 3)
        F = 1.0 + 2.3 * t * t * (3 - 2 * t) + (hills - .5) * .9 * np.clip(t * 3, 0, 1) + \
            .3 * np.clip((ys - 300) / 130, -1, 1) * t
        F = F.astype(np.float32)
        flat = []
        mean = lambda m: float(half(F[m].mean()))
        flat.append((box(172, QUAY_X, 150, 456), .5))
        for b in (ABBEY, KEEP):
            m = box(*b)
            flat.append((m, mean(m)))
        g = box(*GREATLY) | box(417, 444, 224, 242)          # (Greatly Square at the compound's own level, its gate's)
        flat.append((g, mean(box(*GREATLY))))
        keep_sq = box(400, 425, 304, 326)                      # (Pillory Square at the Keep's)
        flat.append((keep_sq, flat[2][1]))
        px0, px1, py0, py1 = PALACE                            # (the forecourt and the palace; its garden slopes)
        for m in (box(334, 386, 258, 316), box(232, 257, 294, 311), box(316, 345, 348, 381), box(px0 + 2, px1, py0, py0 + 32),
                  box(350, 375, 175, 196)):
            flat.append((m, mean(m)))
        L = .12
        for m, v in flat:
            dist = field.distance_to_mask(m, 40)
            F = np.clip(F, v - L * dist, v + L * dist)
        for m, v in flat:
            F[m] = v
        self.flats_ = flat
        h = half(F)
        fixed_m = np.zeros((H, W), dtype=bool)
        for m, _ in flat:
            fixed_m |= m
        for _ in range(12):
            hi = np.full((H, W), -99.0, dtype=np.float32)
            lo = np.full((H, W), 99.0, dtype=np.float32)
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nh = np.roll(h, (dy, dx), (0, 1))
                hi = np.maximum(hi, nh)
                lo = np.minimum(lo, nh)
            fix = ~fixed_m & ((hi - h > .5) | (h - lo > .5)) & (hi - lo <= 1)
            if not fix.any():
                break
            h[fix] = ((hi + lo) / 2)[fix]
            h = half(h)
        inside = self.inside
        c.heights[inside] = h[inside]
        self.levels_map = np.full((H, W), np.nan, dtype=np.float32)
        self.levels_map[inside] = h[inside]
        for name in GROUND:
            m = self.D(name)
            c.paint(m, GROUND[name], None)
        self.abbey_level = float(h[box(*ABBEY)].max())
        self.greatly_level = float(h[box(*GREATLY)].max())
        self.keep_level = float(h[box(*KEEP)].max())
        c.paint(self.abbey_ring, 'H', None)
        c.paint(self.greatly_ring, 'H', None)
        c.paint(self.keep_ring, '#', None)

    def lay_wall(self):
        c, w = self.c, self.world
        band = box(168, 472, 150, 456) & ~self.inside
        self.wall_band = band
        c.paint(band, '#', 2.0)
        # Towers at the corners and about every 90 tiles, standing out from the wall (never on a gate or an arch).
        for tx, ty in ((168, 150), (468, 150), (168, 452), (468, 452), (250, 150), (300, 150), (420, 150), (468, 220),
                       (468, 380), (300, 452), (360, 452)):
            m = box(tx - 2, tx + 5, ty - 2, ty + 5) & ~self.inside
            c.paint(m, '#', 2.0)
        spans = {'N': box(358, 363, 146, 154), 'E': box(468, 475, 298, 303), 'S': box(410, 415, 452, 459),
                 'cinder': box(238, 243, 452, 459), 'harbour': box(188, 192, 452, 459)}
        outside_pt = {'N': (360.5, 141.0), 'E': (479.0, 300.5), 'S': (412.5, 463.0), 'cinder': (240.5, 463.0),
                      'harbour': (190.5, 463.0)}
        inner = {'N': (360, 155), 'E': (467, 300), 'S': (412, 451), 'cinder': (240, 451), 'harbour': (190, 451)}
        self.gate_masks = {}
        for key, m in spans.items():
            m = m & (band | ~self.inside)
            x, y = inner[key]
            level = float(c.heights[y, x])
            c.paint(m, 'G', level)
            w.gates[('ridgemere', key)] = outside_pt[key]
            w.gate_levels[('ridgemere', key)] = level
            self.gate_masks[key] = m
        self.arches = {}
        for y in PIERS:                          # The sea wall's pier arches, at the wharf's height.
            m = box(168, 172, y, y + 6)
            c.paint(m, 'G', .5)
            self.arches[y] = m
        slip = box(150, 172, 414, 426)           # The slipway: an opening in the sea wall, a ramp into the water.
        ys, xs = np.nonzero(slip)
        c.codes[ys, xs] = ord(':')
        c.heights[ys, xs] = half(np.clip((xs - 150) / 21 * .5, 0, .5))
        c.locked[slip] = True
        self.world.water[slip] = False
        self.slip = slip

    def lay_streets(self):
        c = self.c
        inside_open = self.inside & ~self.rings
        for key, ((sx, sy), (hx, hy)) in SQUARES.items():
            m = box(sx - hx, sx + hx + 1, sy - hy, sy + hy + 1) & inside_open
            ch = {'market': '_', 'rag': 'D', 'records': 'f', 'palace': 'f', 'greatly': '_', 'pillory': 'f',
                  'clock': '_'}[key]
            c.paint(m, ch, None)
            self.street |= m
            self.spots[f'square_{key}'] = (sx, sy)
        gates = self.gate_masks['N'] | self.gate_masks['E'] | self.gate_masks['S']
        avenues = {
            'High Street': ([[(474, 300.5), (382, 300.5)]], 5),
            'Gate Street': ([[(360.5, 146), (360.5, 258)]], 5),
            'Crown Street': ([[(360.5, 316), (360.5, 372.5)], [(360.5, 372.5), (343, 372.5)],
                              [(330.5, 379), (330.5, 394)]], 5),
            'Keep Street': ([[(382, 313.5), (402, 313.5)], [(412.5, 316), (412.5, 323)]], 5),
            'Rise Lane': ([[(384.5, 316), (384.5, 373)]], 4),
            'Rise Walk': ([[(372, 371.5), (468, 371.5)]], 4),
            'South Lane': ([[(414.5, 371.5), (414.5, 459)]], 4),
            'Mews Lane': ([[(384, 440.5), (468, 440.5)]], 3),
            'Abbey Row': ([[(288, 186.5), (360.5, 186.5)]], 5),
            'Greatly Way': ([[(430.5, 224), (430.5, 300.5)]], 5),
        }
        self.avenue_lines = {k: v[0] for k, v in avenues.items()}
        for name, (lines, width) in avenues.items():
            for pts in lines:
                self.paint_line(pts, width, '_', None, within=inside_open | gates)
        for m in (box(334, 386, 258, 264), box(334, 340, 258, 287), box(380, 386, 258, 287)):
            c.paint(m, '_', None)                # About the Market Hall.
            self.street |= m
        # The inner ring, 3 wide (north, east and south; on the west the Wharf Road), the wall-walk patrol's line.
        for m in (box(290, 392, 154, 157), box(465, 468, 224, 452), box(274, 468, 449, 452)):
            m &= inside_open
            c.paint(m, '_', None)
            self.street |= m
        lanes3 = {"Clerks' Row": [(244, 372.5), (317, 372.5)], 'Records Lane': [(344, 378.5), (380, 378.5), (380, 370)],
                  'Cinder Lane': [(QUAY_X, 404.5), (290, 404.5)], 'Cinder Road': [(240.5, 459), (240.5, 374)],
                  'Palace Walk': [(276, 394.5), (291, 394.5)], 'Slag Steps': [(224.5, 352), (224.5, 392)]}
        for name, pts in lanes3.items():
            narrow = name == 'Slag Steps'                # (down from the Sump into the Cinders: an alley, two wide)
            self.paint_line(pts, 2 if narrow else 3, '.' if narrow else '_', None,
                            within=inside_open | self.gate_masks['cinder'])
        m = box(188, 192, 154, 459) & (inside_open | self.gate_masks['harbour'])
        c.paint(m, '_', None)                    # The Wharf Road.
        self.street |= m
        self.quay = box(206, QUAY_X, 154, 452) & ~self.rings
        c.paint(self.quay, 'f', None)
        self.street |= self.quay
        for y in PIERS:                          # From each pier's arch across the wharf to the quay.
            m = box(168, 192 if y == CUSTOMS_PIER else 206, y + 1, y + 5)
            c.paint(m & ~self.arches[y], '_', None)
            self.street |= m
        rng = self.rng
        gut = [(338.0, 300.5)]                   # The Gut and Tarry Lane: 3 wide, crooked.
        x, y = 338.0, 300.5
        while x > 222:
            x -= 11
            y = y + rng.uniform(-2.5, 2.5) - .7
            y = float(np.clip(y, 288, 306))
            gut.append((max(214.0, x), y))
        gut += [(QUAY_X + 1.0, 293.0), (204.0, 293.0)]
        self.gut_line = gut
        self.paint_line(gut, 3, '.', None, within=self.D('sump') | self.D('wharf') | self.D('commons'))
        tarry = [(250.5, 216.0)]
        y = 216.0
        while y < 358:
            y += 12
            tarry.append((248.5 + rng.uniform(-3, 3), min(364.0, y)))
        tarry += [(248.5, 374.0)]
        self.tarry_line = tarry
        self.paint_line(tarry, 3, '.', None, within=(self.D('sump') | self.D('admin') | self.D('cinders')))
        # The walled pieces' gates: the abbey's south (onto Tarry Lane) and east (onto Abbey Row), Greatly's onto
        # Greatly Square, the Old Keep's onto Pillory Square. Each gate is at its piece's level, a half step down if the
        # ground outside is lower.
        self.piece_gate(box(248, 253, 214, 216), self.abbey_level, box(248, 253, 216, 218))
        self.piece_gate(box(288, 290, 184, 189), self.abbey_level, box(290, 292, 184, 189))
        self.piece_gate(box(428, 433, 222, 224), self.greatly_level, box(428, 433, 224, 226))
        self.piece_gate(box(409, 414, 322, 325), self.keep_level, box(409, 414, 320, 322))
        self.gate_spots = {'abbey_south': (250, 211), 'abbey_east': (286, 186), 'greatly': (430, 219),
                           'keep': (412, 328)}

    def piece_gate(self, gate, level, outside):
        """A walled piece's gate at its level; where the ground outside is more than half a step away, the gate is at the
        half step and the tiles outside it a stair."""
        c = self.c
        out = float(np.median(c.heights[outside]))
        if abs(out - level) <= .5:
            c.paint(gate, 'G', level)
        else:
            c.paint(gate, 'G', float(half((out + level) / 2)))
            c.paint(outside, '^', None)
        self.street |= gate | outside

    def ground_of(self, name):
        return self.D(name) & ~self.reserved & ~self.street & ~self.built

    def lanes_in(self, ground, spacing, width, angle, ch, offset=(0, 0)):
        lanes = self.lanes(ground, spacing, width, angle=angle, offset=offset) & ground & ~grow(self.built, 1)
        self.c.paint(lanes, ch, None)
        self.street |= lanes
        return lanes

    def place_large(self, name, specs, big):
        """A quarter's large buildings, each on an avenue or square; returns the rest."""
        large = sorted([s for s in specs if s.area >= big], key=lambda s: -s.area)
        small = [s for s in specs if s.area < big]
        lots = Lots(self.ground_of(name), self.street)
        missing = self.place_list(lots, large, self.street, jitter=6, report=False)
        return small + missing

    def place_small(self, name, specs, gap=0):
        """The rest of a quarter, terraced along its lanes (wall to wall, no yards between)."""
        lots = Lots(self.ground_of(name), self.street, gap=gap)
        order = sorted(specs, key=lambda s: -s.area)
        missing = self.place_list(lots, order, self.street, jitter=4, report=False)
        if missing:                                    # Anywhere in the quarter, then.
            lots = Lots(self.ground_of(name), self.street, gap=0)
            for s in missing:
                s.near = None
            missing = self.place_list(lots, missing, self.street, jitter=40, report=False)
            for s in missing:                          # A reused building's block can be smaller than its rooms.
                s.scale = .58
            missing = self.place_list(lots, missing, self.street, jitter=40, report=False)
            if missing and name in NEIGHBOUR:           # Or stand on the edge of the next quarter.
                lots = Lots((self.ground_of(name) | self.ground_of(NEIGHBOUR[name])) & grow(self.D(name), 24),
                            self.street, gap=0)
                for s in missing:
                    s.near = self.anchor(name)
            for s in self.place_list(lots, missing, self.street, jitter=40, report=False):
                self.unplaced.append(s.key)
                print(f'  ridgemere2: {s.name} not placed ({name})')
        return lots

    def stitch_borders(self):
        """Many crossings between neighbouring quarters: every 14 tiles or so along each border, a passage two wide
        straight across from a street on one side to a street on the other (no wall, no step, between them)."""
        c = self.c
        pairs = [('sump', 'commons'), ('sump', 'northgate'), ('sump', 'admin'), ('sump', 'cinders'), ('cinders', 'admin'),
                 ('admin', 'commons'), ('admin', 'rise'), ('commons', 'northgate'), ('commons', 'rise'),
                 ('palace', 'admin'), ('palace', 'cinders'), ('palace', 'rise')]
        blocked = self.built | self.reserved | ~self.inside | self.D('wharf') | self.D('keep') | self.D('abbey') | \
            self.D('greatly')
        for a, b in pairs:
            A, B = self.D(a), self.D(b)
            border = A & grow(B, 1)
            ys, xs = np.nonzero(border)
            if not len(xs):
                continue
            order = np.lexsort((ys, xs)) if (xs.max() - xs.min()) > (ys.max() - ys.min()) else np.lexsort((xs, ys))
            chosen = []
            for i in order:
                p = (int(xs[i]), int(ys[i]))
                if all(abs(p[0] - q[0]) + abs(p[1] - q[1]) >= 14 for q in chosen):
                    chosen.append(p)
            for (x, y) in chosen:
                ends = []
                for side in (A, B):
                    near = self.street & side & ~self.rings
                    y0, y1, x0, x1 = max(0, y - 16), min(H, y + 17), max(0, x - 16), min(W, x + 17)
                    sy, sx = np.nonzero(near[y0:y1, x0:x1])
                    if not len(sx):
                        break
                    k = int(np.argmin(np.abs(sx + x0 - x) + np.abs(sy + y0 - y)))
                    ends.append((float(sx[k] + x0) + .5, float(sy[k] + y0) + .5))
                if len(ends) < 2:
                    continue
                (ax, ay), (bx, by) = ends
                pts = [(ax, ay), (bx, ay), (bx, by)] if self.rng.random() < .5 else [(ax, ay), (ax, by), (bx, by)]
                m = c.line_mask(pts, 2)
                if (m & blocked).any() or (m & self.rag_stalls).any():
                    continue
                ch = '.' if a in ('sump', 'cinders') or b in ('sump',) else '_'
                c.paint(m & ~self.street, ch, None)
                self.street |= m
                self.stitches[f'{a}/{b}'] += 1

    def connect_streets(self):
        """Every piece of street joins the city's network: a lane cut off by the buildings round it gets a passage to
        the nearest street that is joined (if one can be cut through open ground), or stops being a street (so no door
        opens onto it)."""
        c = self.c
        net = self.street & self.inside & ~SOLID[c.codes]
        lab, info = label(net)
        sx, sy = SPAWN
        main = lab[sy, sx]
        blocked = self.built | self.reserved | ~self.inside | SOLID[c.codes] & ~net
        for k, (x0, x1, y0, y1, size) in info.items():
            if k == main:
                continue
            comp = lab == k
            ys, xs = np.nonzero(comp)
            X0, X1, Y0, Y1 = max(0, x0 - 30), min(W, x1 + 30), max(0, y0 - 30), min(H, y1 + 30)
            my, mx = np.nonzero(lab[Y0:Y1, X0:X1] == main)
            done = False
            if len(mx):
                mx, my = mx + X0, my + Y0
                pick = np.arange(0, len(xs), max(1, len(xs) // 40))
                best = None
                for i in pick:
                    d = np.abs(mx - xs[i]) + np.abs(my - ys[i])
                    j = int(np.argmin(d))
                    if best is None or d[j] < best[0]:
                        best = (int(d[j]), int(xs[i]), int(ys[i]), int(mx[j]), int(my[j]))
                if best and best[0] <= 30:
                    _, ax, ay, bx, by = best
                    for pts in ([(ax + .5, ay + .5), (bx + .5, ay + .5), (bx + .5, by + .5)],
                                [(ax + .5, ay + .5), (ax + .5, by + .5), (bx + .5, by + .5)]):
                        m = c.line_mask(pts, 2) & ~net
                        if not (m & blocked).any():
                            ch = '.' if self.D('sump')[ay, ax] or self.D('cinders')[ay, ax] else '_'
                            c.paint(m, ch, None)
                            self.street |= m
                            net |= m
                            done = True
                            self.infill['connected'] += 1
                            break
            if not done:
                self.street[comp] = False
                self.infill['streets_dropped'] += 1

    def infill_yards(self, name, grass, extra):
        """A quarter's leftover ground: sheds and outhouses where they fit, small fenced back-yards in what is shut in by
        buildings, and the rest its own ground with its clutter."""
        c, rng = self.c, self.rng
        D = self.D(name)
        left = D & ~self.street & ~self.built & ~self.reserved & self.inside
        lab, comps = label(left)
        for k, (x0, x1, y0, y1, area) in comps.items():
            if area < 9:
                continue
            comp = lab == k
            for _ in range(1 if name in ('commons', 'northgate', 'rise') else 4):
                spot = None
                for bw, bh in ((6, 4), (5, 4), (4, 4), (4, 3), (3, 3)):
                    spot = fit_rect(comp & ~grow(self.street, 1), bw, bh) or fit_rect(comp & ~grow(self.street, 1), bh, bw)
                    if spot:
                        break
                if not spot:
                    break
                rx, ry, rw, rh = spot
                c.stamp(rx, ry, shed(rw, rh), None)
                comp[ry:ry + rh, rx:rx + rw] = False
                self.built[ry:ry + rh, rx:rx + rw] = True
                self.infill[f'{name}_sheds'] += 1
            rest = comp & ~self.built
            if rest.sum() >= 6 and not (rest & grow(self.street, 1)).any():
                ry, rx = np.nonzero(rest)
                for j in range(0, len(rx), 7):
                    c.codes[ry[j], rx[j]] = ord(rng.choice('xO'))
                self.infill[f'{name}_yards'] += 1
        self.leftover(D & ~self.street & ~self.built & ~self.reserved & self.inside, grass, extra=extra)

    def paint_stairs(self):
        """Where a street climbs half a step, its upper tile is a stair: the stair-streets that climb from the wharf."""
        c = self.c
        h = c.heights
        walk = self.street & self.inside & np.isin(c.codes, [ord(x) for x in '_.fdD']) & ~self.built
        up = np.zeros((H, W), dtype=bool)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nh = np.roll(h, (dy, dx), (0, 1))
            nw = np.roll(walk, (dy, dx), (0, 1))
            up |= walk & nw & (h - nh >= .5)
        flat_squares = np.zeros((H, W), dtype=bool)
        for m, _ in self.flats_:
            flat_squares |= m
        up &= ~shrink(flat_squares, 1)
        c.paint(up, '^', None)
        self.stairs_mask |= up

    def fix_doors(self):
        """Each street door's sill at the height of the street it opens onto."""
        c = self.c
        for link in self.site.links:
            if link['kind'] != 'door' or not link['a']['cell'].startswith(('@', 'w_')):
                continue
            cid = link['a']['cell']
            cx, cy = map(int, cid.lstrip('@').split('_')[-2:])
            x, y = cx * CELL + link['a']['x'], cy * CELL + link['a']['y']
            best = None
            for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0)):
                nx_, ny_ = x + dx, y + dy
                if 0 <= nx_ < W and 0 <= ny_ < H and not SOLID[c.codes[ny_, nx_]] and c.codes[ny_, nx_] != ord('+'):
                    if best is None or self.street[ny_, nx_]:
                        best = (nx_, ny_)
            if best:
                c.heights[y, x] = c.heights[best[1], best[0]]

    def leftover(self, open_ground, grass=',', extra=(), density=.08):
        """Whatever is left between buildings: the district's ground, with its clutter."""
        rng = np.random.default_rng(self.rng.randrange(1 << 30))
        noise = field.fbm(W, H, 10, self.rng.randrange(1000), 2)
        free = open_ground & ~self.world.water & ~self.street
        self.c.codes[free & (noise > .55)] = ord(grass)
        grain = rng.random(free.shape)
        for ch, share in extra:
            self.c.codes[free & (grain < share)] = ord(ch)
            grain = rng.random(free.shape)
        self.c.locked[free] = True

    def sheds_in(self, lots, n, district, sizes=((3, 3), (4, 3), (5, 4), (6, 4), (4, 4), (3, 4))):
        """Sheds, privies and lean-tos: solid, no interior, on the leftover ground beside the streets."""
        made = 0
        for i in range(n * 6):
            if made >= n:
                break
            w_, h_ = self.rng.choice(sizes)
            near = self.random_point(district)
            got = self.nearest_lot(lots, w_, h_, near, lots.street)
            if got:
                _, x0, y0, facing, w, h = got
                self.c.stamp(x0, y0, shed(w, h), None)
                self.built[y0:y0 + h, x0:x0 + w] = True
                lots.take(x0, y0, w, h)
                made += 1
        self.infill['sheds'] += made
        return made

    def random_point(self, district, where=None):
        m = self.D(district) if where is None else where
        ys, xs = np.nonzero(m)
        i = self.rng.randrange(len(xs))
        return float(xs[i]), float(ys[i])

    # -- the wharf ---------------------------------------------------------------------------------------------------
    def waterfront_map(self):
        """Piers out from the arches, cranes at their ends; the north pier runs on as the Mole, a stone breakwater out
        into the bay with the Bay Light at its head (doc 67, Dense city)."""
        c = self.c
        for y in PIERS:
            m = box(112, 168, y, y + 6)
            c.paint(m, '8', .5)
            self.world.water[m] = False
            for x in range(114, 168, 8):
                c.codes[y, x] = ord('i')          # Mooring posts along its north edge.
            self.posts['pier'].append((140, y + 3))
            self.posts['pier'].append((158, y + 3))
        for y in CRANES:
            c.paint(box(113, 116, y + 1, y + 4), 'I', .5)
            self.posts['crane'].append((117, y + 2))
        mole = box(72, 112, PIERS[0], PIERS[0] + 6)
        c.paint(mole, 'f', .5)
        for x in range(74, 112, 6):
            c.codes[PIERS[0] + 5, x] = ord('i')   # Bollards along its south edge.
        self.world.water[mole] = False
        self.mole |= mole
        self.street |= mole
        self.spots['sump_stairs'] = (179, STAIRS_PIER + 3)
        self.spots['customs_pier'] = (178, CUSTOMS_PIER + 3)

    def wharf(self):
        specs = [self.reuse_spec(g) for g in (
            'rm_the_sailors_rest', 'rm_the_fish_hall', 'rm_the_wharf_tannery', 'rm_the_ropewalk',
            'rm_the_greatly_bonded_warehouse', 'rm_the_fell_timber_store', 'rm_the_north_wharf_smokehouse',
            'rm_the_drowned_lantern', 'rm_the_tarred_rope', 'rm_wickham_s_chandlery', 'rm_the_bung_hole',
            'rm_the_harbourmaster_s_office', 'rm_the_barracks')]
        specs.append(Spec('r2_new_store', 'The Grayrock New Store', 'wharf', 'store', near=(180, 340),
                          make=lambda: self.new_hall('warehouse', 'The Grayrock New Store', 'Grayrock\'s newest store '
                                                     'on the wharf: bales, casks and crates stacked to the beams, '
                                                     'tallied twice and guarded.', 14, 24, footprint=(9, 14)),
                          claims=['house_grayrock']))
        # The Customs House, the Crown's, at the Customs Pier; the shipwright's yard behind the slipway.
        low = hall(self.new_rng, 'civic', 30, 14, 'reading')
        up = office_floor(self.new_rng, 'civic', 30, 14, 6, door=False)
        stairs_between(low, up, 27, 10)
        customs = Spec('r2_customs_house', 'The Customs House', 'wharf', 'hall', claims=['council_of_houses'],
                       make=lambda: self.multi('hall', 'The Customs House', [low, up], [(('', 27, 10), ('up', 27, 10))],
                                               ['', 'up'], [0, 1], ['The Customs House', 'The Customs House, the collector\'s office'],
                                               ['The Crown\'s Customs House on the wharf: long counters where every cask '
                                                'and bale off the ships is weighed, stamped and taxed.',
                                                'The collector\'s office over the counters: ledgers, seals and a window '
                                                'on the Customs Pier.'], 'civic', footprint=(28, 14)))
        self.put(customs, 192, CUSTOMS_PIER - 6, 'W')
        yard = Spec('r2_grayrock_shipwrights', 'Grayrock Shipwrights', 'wharf', 'works', business='shipwright',
                    claims=['house_grayrock'],
                    make=lambda: self.new_works('shipwright', 'Grayrock Shipwrights', 'House Grayrock\'s yard at the '
                                                'slipway: hulls are built and mended here and run down into the bay.', 5,
                                                footprint=(20, 14)))
        self.put(yard, 174, 410, 'E')
        ground = self.ground_of('wharf') & ~self.quay & ~box(172, 174, 150, 458) & ~box(172, 188, 406, 430)
        for y in PIERS:
            ground &= ~box(168, 182, y - 1, y + 7)
        lots = Lots(ground, self.street)
        self.place_list(lots, sorted(specs, key=lambda s: -s.area), self.street, jitter=4)
        self.sheds_in(lots, 14, 'wharf')
        self.leftover(lots.open & ground, 'f', extra=(('|', .03), ('x', .01), ('O', .01), ('<', .006)))

    def reuse_spec(self, gid, district=None, near=None, kind=None, homes=None, scale=.72):
        dist, k, anchor = REUSE.get(gid, (district, kind, near))
        g = self.structures[gid]
        style = 'manor' if k in ('seat', 'villa') else 'works' if dist == 'cinders' and k == 'works' else 'ridgemere'
        if homes is None:
            homes = {'home': 1, 'lodging': 1, 'shop': 1, 'villa': 2, 'seat': 2}.get(k, 0)
        s = Spec(gid, g['name'], district or dist, kind or k, reuse=g, near=near or remap(dist, anchor), style=style,
                 homes=homes, business='stables' if 'stables' in gid else None)
        s.scale = scale
        return s

    # -- the abbey and cathedral close ----------------------------------------------------------------------------
    def abbey(self):
        c = self.c
        ax0, ax1, ay0, ay1 = ABBEY
        inner = box(ax0 + 2, ax1 - 2, ay0, ay1 - 2)
        lv = self.abbey_level
        nave, crypt, pairs = cathedral(self.new_rng, 'ridgemere')
        cat = Spec('r2_cathedral', 'The Cathedral of Ridgemere', 'abbey', 'hall', claims=['council_of_houses'],
                   make=lambda: self.multi('hall', 'The Cathedral of Ridgemere', [nave, crypt], pairs, ['', 'crypt'], [0, -1],
                                           ['The Cathedral of Ridgemere', 'The Cathedral of Ridgemere, the crypt'],
                                           ['The cathedral of the abbey: a great nave of soot-dark stone, pillared '
                                            'aisles, rain on the high windows, the altar and the saint\'s statue at its '
                                            'head.', 'The crypt under the nave: the city\'s old dead in their niches, '
                                            'and the cold of the hill.'], 'ridgemere', footprint=(34, 20)))
        self.put(cat, 214, 156, 'S')
        # The cloister garth: lawn inside an arcade of pillars, east of the cathedral.
        garth = box(256, 280, 157, 175)
        c.paint(garth, ',', lv)
        edge = garth & ~box(257, 279, 158, 174)
        ys, xs = np.nonzero(edge)
        pillars = ((xs + ys) % 3 == 0)
        c.codes[ys[pillars], xs[pillars]] = ord('I')
        self.spots['cloister'] = (268, 166)
        # Paths: along the cathedral's front, to each gate.
        paths = box(212, 288, 176, 180) | box(248, 253, 180, 214) | box(253, 288, 184, 189)
        paths &= inner
        c.paint(paths, 'f', lv)
        self.street |= paths
        S = 'ridgemere'
        specs = [
            Spec('r2_chapter_house', 'The Chapter House', 'abbey', 'hall', near=(225, 186),
                 make=lambda: self.new_hall('hearing', 'The Chapter House', 'The abbey\'s chapter house: stone benches '
                                            'round the walls, the abbot\'s chair, and the rule read aloud each morning.',
                                            14, 10)),
            Spec('r2_abbey_quarters', 'Abbey Quarters', 'abbey', 'lodging', near=(225, 205), homes=1,
                 make=lambda: self.one('house', 'Abbey Quarters', dorter(self.new_rng, S, 12, 20, 12),
                                       'The dorter: a long dormitory of cells, a pallet and a peg in each, where the '
                                       'brothers and sisters of the abbey sleep.', S, footprint=(13, 8))),
            Spec('r2_abbey_refectory', 'The Refectory', 'abbey', 'hall', near=(270, 200),
                 make=lambda: self.new_hall('refectory', 'The Refectory', 'The abbey\'s refectory: long tables, a reader\'s '
                                            'lectern and the smell of the day\'s pottage.', 18, 12)),
            Spec('r2_abbey_scriptorium', 'The Scriptorium', 'abbey', 'hall', near=(240, 200),
                 make=lambda: self.new_hall('reading', 'The Scriptorium', 'Desks under the windows where the abbey\'s '
                                            'scribes copy and bind, and shelves of what they have made.', 16, 10)),
            Spec('r2_abbey_brewhouse', 'The Abbey Brewhouse', 'abbey', 'works', near=(280, 205), business='brewery',
                 make=lambda: self.new_works('brewery', 'The Abbey Brewhouse', 'The abbey\'s brewhouse: the brothers\' '
                                             'ale, brewed by the cellarer.', 2, style=S, size=(14, 10))),
            Spec('r2_abbey_school', 'The Abbey School', 'abbey', 'hall', near=(268, 182),
                 make=lambda: self._school()),
            Spec('r2_abbey_infirmary', 'The Abbey Infirmary', 'abbey', 'hall', near=(225, 196),
                 make=lambda: self.new_hall('healer', 'The Abbey Infirmary', 'The abbey\'s own infirmary: clean beds, '
                                            'herbs drying and a brother on watch.', 16, 9)),
        ]
        ground = inner & ~self.built & ~grow(self.built, 1) & ~garth & ~self.street
        lots = Lots(ground, self.street, gap=0)
        self.place_list(lots, sorted(specs, key=lambda s: -s.area), self.street, jitter=4)
        self.leftover(lots.open & ground, ',', extra=(('3', .02), ('Y', .01), ('B', .02)))

    # -- House Greatly's compound -------------------------------------------------------------------------------------
    def greatly_compound(self):
        """House Greatly's compound, its grounds less than half what they were: the forge and workshop, the masons',
        the colour works and the stables in the back (north) yard; the house; the fountain in the front (south) yard."""
        c = self.c
        lv = self.greatly_level
        gx0, gx1, gy0, gy1 = GREATLY
        inner = box(gx0 + 2, gx1, gy0, gy1 - 2)
        north = box(gx0 + 2, gx1, gy0, 171)
        c.paint(north & inner, 'f', lv)
        lanes = box(394, 468, 171, 175) | box(394, 398, 175, 220) | box(394, 428, 216, 220)
        c.paint(lanes & inner, 'f', lv)
        drive = box(428, 433, 190, 222)
        court = box(421, 440, 200, 215)
        path = box(440, 446, 197, 201)
        c.paint(drive | court | path, 'd', lv)
        c.paint(box(428, 433, 205, 210), 'F', lv)            # The fountain in the front yard.
        self.street |= (lanes & inner) | drive | court | path
        self.spots['greatly_fountain'] = (427, 207)
        forge = Spec('r2_greatly_forge', 'The Greatly Forge & Workshop', 'greatly', 'works', business='builders_works',
                     claims=['house_greatly'],
                     make=lambda: self.new_works('builders_works', 'The Greatly Forge & Workshop', 'House Greatly\'s own '
                                                 'works behind the house, for its building trade alone: cramps, nails and '
                                                 'hinges, roof timbers and doors. (The city\'s forges are Timberland\'s.)',
                                                 3, footprint=(24, 14)))
        self.put(forge, 396, 157, 'S')
        self.put(self.reuse_spec('rm_greatly_house'), 421, 179, 'S', 19, 11)
        self.put(self.reuse_spec('rm_the_greatly_masons_yard'), 422, 162, 'S', 14, 9)
        self.put(self.reuse_spec('rm_the_greatly_colour_works'), 438, 164, 'S', 13, 7)
        self.put(self.reuse_spec('rm_the_greatly_stables'), 453, 165, 'S', 13, 6)
        self.put(self.reuse_spec('rm_the_greatly_counting_house'), 446, 196, 'W', 14, 7)
        ground = inner & ~self.built & ~self.street & ~north & ~box(394, 468, 171, 175)
        self.gardens_plain(ground, 'greatly')
        self.posts['greatly_gardens'] = [(406, 205), (454, 212)]
        self.posts['greatly_gate'] = [(430, 219)]

    def gardens_plain(self, ground, key, trees='Y'):
        """gardens(crops=False): lawns, clipped hedges, flowerbeds and specimen trees; no kitchen garden, orchard,
        pond or field (doc 67, R34)."""
        c = self.c
        rng = np.random.default_rng(sum(map(ord, key)))
        grain = rng.random((H, W))
        ys, xs = np.mgrid[0:H, 0:W]
        c.codes[ground] = ord(',')
        hedge = ground & (((xs // 2) % 9 == 0) | ((ys // 2) % 7 == 0)) & (grain < .7)
        c.codes[hedge] = ord('B')
        c.codes[ground & ~hedge & (grain > .93)] = ord('3')
        c.codes[ground & ~hedge & (grain < .025)] = ord(trees)
        c.codes[ground & ~hedge & (grain > .985)] = ord('S')
        c.locked[ground] = True

    # -- the Old Keep ----------------------------------------------------------------------------------------------------
    def old_keep(self):
        plans, pairs = keep_tower(self.new_rng, 'civic', 22, 20)
        tower = Spec('r2_the_keep', 'The Keep', 'keep', 'hall', claims=['council_of_houses', 'ridgemere_watch'],
                     make=lambda: self.multi('hall', 'The Keep', plans, pairs, ['', 'up', 'down'], [0, 1, -1],
                                             ['The Watch Office', 'The Keep, the captain\'s rooms',
                                              'The Keep, the cells'],
                                             ['The Old Keep\'s tower, the Watch Office now: the duty sergeant\'s table, '
                                              'the day\'s rolls, wet cloaks by the door and racks of cudgels.',
                                              'The captain\'s rooms at the top of the stair, and the Watch\'s weapon '
                                              'racks: spears, crossbows and the cudgels counted every night.',
                                              'The cells under the Keep: a corridor of barred cells cut into the hill, '
                                              'a bench in each, and the gaoler\'s table at the end.'], 'civic',
                                             footprint=(16, 16)))
        self.put(tower, 396, 338, 'N')
        barracks = self.reuse_spec('rm_the_watch_house')
        barracks.claims = ['council_of_houses', 'ridgemere_watch']
        self.put(barracks, 420, 330, 'W', 10, 18)
        kx0, kx1, ky0, ky1 = KEEP
        court = box(kx0 + 3, kx1 - 3, ky0 + 3, ky1 - 3) & ~self.built & ~self.rings
        self.c.paint(court, 'f', None)
        self.street |= court
        self.posts['keep_yard'] = [(406, 328), (416, 358), (398, 358)]

    # -- the Great Palace ---------------------------------------------------------------------------------------------
    def palace(self):
        """The Great Palace on its forecourt, its three parts touching; behind them a modest privy garden (no wall: the
        palace's guards are its wall)."""
        c = self.c
        rp, rpairs = palace_residence(self.new_rng, 'palace', 48, 24, 3, 6)
        residence = Spec('r2_kings_residence', "The King's Residence", 'palace', 'seat', homes=2,
                         claims=['council_of_houses'],
                         make=lambda: self.multi('seat', "The King's Residence", rp, rpairs, ['', 'up', 'down'], [0, 1, -1],
                                                 ["The King's Residence", "The King's Residence, the royal bedchambers",
                                                  "The King's Residence, the kitchens"],
                                                 ['The palace\'s west wing, the King\'s Residence: receiving rooms hung '
                                                  'with the Crown\'s banners, looking west over the Cinders\' smoke.',
                                                  'The royal bedchambers above: curtained beds, '
                                                  'fires in every room and rain on tall windows.',
                                                  'The palace kitchens and the servants\' hall beneath the Residence, '
                                                  'warm and never quiet.'], 'palace', footprint=(22, 34)))
        ante = Spec('r2_palace_antechamber', 'The Antechamber', 'palace', 'hall', claims=['council_of_houses'],
                    make=lambda: self.one('hall', 'The Antechamber', palace_antechamber(self.new_rng, 'palace', 40, 24),
                                          'The Great Palace\'s antechamber, its court and senate: tiered benches under '
                                          'the four Houses\' banners where the Council of Houses sits, the Chancellor\'s '
                                          'dais, and the great door on the forecourt.', 'palace', footprint=(22, 24)))
        wp, wpairs = palace_wing(self.new_rng, 'palace', 48, 24, 8)
        wing = Spec('r2_administrative_wing', 'The Administrative Wing', 'palace', 'hall', claims=['council_of_houses'],
                    make=lambda: self.multi('hall', 'The Administrative Wing', wp, wpairs, ['', 'up'], [0, 1],
                                            ['The Administrative Wing', 'The Administrative Wing, upstairs'],
                                            ['The palace\'s east wing: the royal secretary\'s and the treasurer\'s '
                                             'offices, clerks at long desks and the Crown\'s papers in presses.',
                                             'More desks under the eaves, and the Crown\'s rolls in their cabinets.'],
                                            'palace', footprint=(22, 34)))
        # (Footprints are given door edge first: 34, 24 and 34 along the forecourt, 22 deep.)
        residence.building.footprint = (34, 22)
        ante.building.footprint = (24, 22)
        wing.building.footprint = (34, 22)
        self.put(residence, 290, 402, 'N')
        self.put(ante, 324, 402, 'N')
        self.put(wing, 348, 402, 'N')
        ground = lambda key: self.named[key]['rooms'][0]['id']
        self.passage(ground('r2_kings_residence'), 'E', ground('r2_palace_antechamber'), 'W',
                     "The King's Residence to the Antechamber")
        self.passage(ground('r2_palace_antechamber'), 'E', ground('r2_administrative_wing'), 'W',
                     'The Antechamber to the Administrative Wing')
        # The privy garden behind: lawns, hedges and a fountain (no crops), with walks round it.
        garden = box(290, 380, 424, 449)
        paths = box(288, 290, 392, 449) | box(380, 382, 392, 449) | box(290, 380, 435, 438)
        c.paint(paths & ~self.built, 'f', None)
        self.street |= paths & ~self.built
        g = garden & ~paths & ~self.built & ~self.reserved & ~self.street
        self.gardens_plain(g, 'palace')
        c.paint(box(334, 337, 428, 431), 'F', None)
        self.posts['palace_gardens'] = [(336, 444), (308, 430)]

    def passage(self, a, side_a, b, side_b, name):
        """A passage link between two interiors that touch (a door knocked through): a doorway carved in each one's wall
        on the side toward the other, so the room's floor isn't a trap."""
        ra, rb = self.site_room(a), self.site_room(b)
        ea, eb = self.carve(ra, side_a), self.carve(rb, side_b)
        if ea is None or eb is None:
            return False
        lid = f'{a}_to_{b}'
        if len(lid) > 48:                                # (Atlas ids are at most 48 characters)
            lid = f'{lid[:41]}_{__import__("hashlib").md5(lid.encode()).hexdigest()[:6]}'
        self.site.links.append({'id': lid, 'name': name, 'kind': 'passage', 'open': True,
                                'a': {'cell': a, 'x': ea[0], 'y': ea[1]}, 'b': {'cell': b, 'x': eb[0], 'y': eb[1]}})
        self.passages.append((a, b))
        return True

    def site_room(self, rid):
        for r in self.site.rooms:
            if r['id'] == rid:
                return r
        raise KeyError(rid)

    def carve(self, room, side, taken=None):
        """A doorway in a room's wall on `side` (the room's own tiles are drawn as on the map): the wall tile becomes '+',
        with an open floor tile inside it. Returns its (x, y), or None."""
        rows = [list(r) for r in room['terrain']]
        w, h = room['width'], room['height']
        used = {(e['x'], e['y']) for link in self.site.links for e in (link['a'], link['b']) if e['cell'] == room['id']}
        if side in 'EW':
            x, ix = (w - 1, w - 2) if side == 'E' else (0, 1)
            order = sorted(range(2, h - 2), key=lambda y: abs(y - h // 2))
            cands = [(x, y, ix, y) for y in order]
        else:
            y, iy = (h - 1, h - 2) if side == 'S' else (0, 1)
            order = sorted(range(2, w - 2), key=lambda x: abs(x - w // 2))
            cands = [(x, y, x, iy) for x in order]
        for x, y, ix, iy in cands:
            if (x, y) in used or (ix, iy) in used:
                continue
            if rows[iy][ix] in SOLID_CHARS or rows[iy][ix] in '+^':
                continue
            if rows[y][x] not in WALL_CHARS:
                continue
            rows[y][x] = '+'
            room['terrain'] = [''.join(r) for r in rows]
            return (x, y)
        return None

    # -- the market ----------------------------------------------------------------------------------------------------
    def market_square(self):
        c = self.c
        (mx, my), _ = SQUARES['market']
        keepers = []
        for x in (mx - 18, mx - 12, mx + 9, mx + 15):    # The north row of stalls: four two-tile stalls.
            c.stamp(x, my - 9, ['uu'], None)
            keepers.append((x, my - 10))
        for x, row in ((mx - 18, 'uu'), (mx - 10, 'u'), (mx + 9, 'u'), (mx + 15, 'uu')):   # The south row.
            c.stamp(x, my + 9, [row], None)
            keepers.append((x, my + 10))
        c.stamp(mx, my - 11, ['U'], None)                 # The well.
        self.posts['market_square'] = keepers
        self.posts['market_porters'] = [(mx - 21, my - 4), (mx + 21, my - 4), (mx - 21, my + 4), (mx + 21, my + 4)]
        hall_plan = market_hall(self.new_rng, 'ridgemere', 20, 48, 6)
        hall_ = Spec('r2_market_hall', 'The Market Hall', 'commons', 'hall', claims=['council_of_houses'],
                     make=lambda: self.one('hall', 'The Market Hall', hall_plan, 'The covered Market Hall on the north side '
                                           'of Market Square: six counters under a slate roof on oak posts, out of the '
                                           'rain, a door at each end.', 'ridgemere', footprint=(16, 40)))
        self.put(hall_, 340, 264, 'W')
        # The Rag Market in the Sump: six makeshift stalls round its edges, a pump against a wall.
        (rx, ry), _ = SQUARES['rag']
        stalls = [((rx - 8, ry - 6), (rx - 8, ry - 5)), ((rx, ry - 6), (rx, ry - 5)), ((rx + 8, ry - 6), (rx + 8, ry - 5)),
                  ((rx - 8, ry + 6), (rx - 8, ry + 5)), ((rx + 1, ry + 6), (rx + 1, ry + 5)), ((rx + 8, ry + 6), (rx + 8, ry + 5))]
        rag = []
        self.rag_stalls = np.zeros((H, W), dtype=bool)   # (The Sump's alleys are cut round them, never over them.)
        for (sx, sy), keeper in stalls:
            c.stamp(sx, sy, ['u'], None)
            self.rag_stalls[sy, sx] = True
            rag.append(keeper)
        c.stamp(rx + 10, ry - 2, ['U'], None)
        self.posts['rag_market'] = rag
        self.posts['tray_sellers'] = [(rx - 5, ry - 1), (rx + 4, ry - 1), (rx + 6, ry + 2), (rx - 3, ry + 3)]

    # -- the administrative sector ------------------------------------------------------------------------------------
    def admin_specs(self):
        ground_p = hall(self.new_rng, 'civic', 30, 20, 'reading')
        up = office_floor(self.new_rng, 'civic', 30, 20, 8, door=False)
        top = stacks(self.new_rng, 'civic', 30, 20, deep=False)
        stairs_between(ground_p, up, 27, 17)
        stairs_between(up, top, 2, 17)
        records = Spec('r2_records_office', 'The Records Office', 'admin', 'hall', claims=['council_of_houses'],
                       make=lambda: self.multi('hall', 'The Records Office', [ground_p, up, top],
                                               [(('', 27, 17), ('up', 27, 17)), (('up', 2, 17), ('top', 2, 17))],
                                               ['', 'up', 'top'], [0, 1, 2],
                                               ['The Records Office', 'The Records Office, the clerks\' office',
                                                'The Records Office, the rolls'],
                                               ['The three-storey Records Office on Records Square: the counter hall '
                                                'and the public reading room, where deeds, wills, births and deaths are '
                                                'searched for a fee.', 'The clerks\' office: rows of desks under the '
                                                'windows, a clerk at each, copying, checking and sealing.',
                                                'The rolls at the top of the house: shelves of ledgers and deed-boxes to '
                                                'the rafters, every household of Ridgemere written down.'], 'civic',
                                               footprint=(24, 16)))
        self.put(records, 318, 349, 'S')
        specs = [self.reuse_spec(g, scale=.85) for g in ('rm_the_city_office', 'rm_the_magistrate_s_court',
                                                         'rm_the_hall_of_records', 'rm_wetink_lane_scriveners',
                                                         'rm_the_slate_ledger')]
        specs[0].near, specs[1].near, specs[2].near = (368, 360), (298, 362), (290, 384)
        for s in specs:
            s.style = 'civic' if s.kind == 'hall' else s.style
        for i, gid in enumerate(self.houses.get('admin', [])):
            specs.append(self.reuse_spec(gid, 'admin', (256 + 14 * (i % 8), 362 + 16 * (i % 2)), 'home', scale=.8))
        return specs

    # -- the Commons and Northgate ------------------------------------------------------------------------------------
    def commons_specs(self):
        S = 'ridgemere'
        com, north = [], []
        for gid, (dist, kind, anchor) in REUSE.items():
            if dist in ('commons', 'northgate'):
                (com if dist == 'commons' else north).append(self.reuse_spec(gid, scale=.85))
        # The middle town's houses, their lots narrower: the largest stand nearest the Rise, in its better style.
        houses = [(gid, 'commons') for gid in self.houses.get('commons', [])] + \
                 [(gid, 'northgate') for gid in self.houses.get('northgate', [])]
        houses.sort(key=lambda t: -self.structures[t[0]]['size'][0] * self.structures[t[0]]['size'][1])
        rise_edge = self.D('commons') & (self.wealth > 1.25)
        for i, (gid, dist) in enumerate(houses):
            s = self.reuse_spec(gid, dist, None, 'home', scale=.85)
            if dist == 'commons' and i < 6 and rise_edge.any():
                s.near = self.random_point('commons', rise_edge)
                s.style = 'manor'
            else:
                s.near = self.random_point(dist)
            (com if dist == 'commons' else north).append(s)
        com += [
            Spec('r2_barber', "The Barber's Pole", 'commons', 'shop', homes=1, near=(326, 300), business='barber',
                 make=lambda: self.new_shop('apothecary', "The Barber's Pole", 'A barber-surgeon\'s: a chair by the '
                                            'window, razors and lancets laid out, and a striped pole by the door.')),
            Spec('r2_fell_wine_vaults', 'The Fell Wine Vaults', 'commons', 'shop', homes=1, near=(372, 280),
                 business='vintner', claims=['house_fell'],
                 make=lambda: self.new_shop('provisioner', 'The Fell Wine Vaults', 'House Fell\'s wine merchant in the '
                                            'city: casks of Fellhall wine and brandy racked to the ceiling, cordials '
                                            'and preserves on the counter.')),
            Spec('r2_cheesemonger', 'The Round and Rind', 'commons', 'shop', homes=1, near=(350, 330), business='dairy',
                 make=lambda: self.new_shop('provisioner', 'The Round and Rind', 'A cheesemonger\'s: rounds ripening '
                                            'on cool shelves, butter in crocks and a wire for cutting.')),
            Spec('r2_bathhouse', 'The Bathhouse', 'commons', 'works', near=(312, 318), business='bathhouse',
                 make=lambda: self.new_works('bathhouse', 'The Bathhouse', 'The public bathhouse on the Sump\'s edge, fed '
                                             'from the Cisterns: a penny for the hot pool, two for a towel, and steam '
                                             'rolling out of its door into the rain.', 3, style=S, size=(18, 12))),
        ]
        # The Tallow Arcade: a covered way through a row of shops' backs, from one street to the next.
        arc = Plan(9, 18, '#', 'f', self.new_rng)
        arc.back_door()
        for y in range(3, 15, 4):
            arc.put(2, y, 'I')
            arc.put(6, y, 'I')
            arc.put(1, y + 1, 'p')
            arc.put(7, y + 2, 'O')
        arcade = Spec('r2_tallow_arcade', 'The Tallow Arcade', 'commons', 'hall', near=(318, 270),
                      make=lambda: self._through('The Tallow Arcade', arc, 'A covered arcade through the backs of the '
                                                 'houses, out of the rain: a vaulted way of soot-black pillars, '
                                                 'benches against the walls, chandlers\' smell and wet boots, from one '
                                                 'street to the next.', (6, 13)))
        self.arcade_spec = arcade
        return com, north

    def _school(self):
        """The abbey school, with a door at each end (its children come and go by both)."""
        p = hall(self.new_rng, 'ridgemere', 16, 10, 'reading')
        x = next((x for x in sorted(range(1, p.w - 1), key=lambda x: abs(x - p.w // 2)) if p.g[1][x] == p.floor), p.w // 2)
        p.g[1][x] = p.floor
        p.back_door(x)
        return self._through('The Abbey School', p, 'Benches and slates where the abbey teaches the Sump\'s and the '
                             'Commons\' children their letters, a door to the close at each end.', (12, 4))

    def _through(self, name, plan, text, footprint):
        b = self.one('hall', name, plan, text, 'ridgemere', footprint)
        b.back = True
        return b

    def commons_rows(self):
        """The middle town's new houses: row houses of two and three homes on narrow lots (no yards), and on the Sump's
        edge cheaper tenements of three homes creeping in; then whatever lot is left along a lane takes one more row."""
        made = []
        lots = {name: Lots(self.ground_of(name), self.street, gap=0) for name in ('commons', 'northgate')}
        sump_edge = self.D('commons') & (self.wealth < .72)
        names = iter(ROW_NAMES)
        tnames = iter(sump_names(random.Random(self.world.seed + 5), 40))
        plan = [('tenement', 'commons', 3)] * TENEMENTS + [('row', 'commons', 3)] * ROWS_COMMONS + \
               [('row', 'commons', 2)] * ROWS_COMMONS_2 + [('row', 'northgate', 3)] * ROWS_NORTHGATE
        for i, (kind, dist, homes) in enumerate(plan):
            if kind == 'tenement':
                name = f'{next(tnames).split()[0]} Rents'
                style, beds, prose = 'ridgemere', tuple(self.rng.choice([2, 3, 3, 4]) for _ in range(homes)), None
                near = self.random_point('commons', sump_edge) if sump_edge.any() else None
                sizes = [(8, 7), (9, 7), (7, 9), (8, 8)]
            else:
                name = next(names)
                style, beds, prose = 'ridgemere', tuple(self.rng.choice([2, 3, 3]) for _ in range(homes)), ROW_PROSE
                near = self.random_point(dist)
                sizes = [(7, 9), (8, 9), (7, 10), (8, 8), (9, 8)]
            fw, fh = self.rng.choice(sizes)
            iw, ih = int(np.clip(round(fw * 1.3), 8, 12)), int(np.clip(round(fh * 1.2), 8, 12))
            spec = Spec(f'r2_row_{i}', name, dist, 'flats', homes=homes, open_door=False, near=near,
                        make=lambda name=name, homes=homes, beds=beds, iw=iw, ih=ih, fw=fw, fh=fh, prose=prose, style=style:
                        self.new_flats(name, homes, beds, (iw, ih), (fw, fh), style=style, prose=prose or ''))
            got = self.place_list(lots[dist], [spec], self.street, jitter=6, report=False)
            if got:                                         # No lot near: anywhere in the quarter.
                spec.near = self.random_point(dist)
                got = self.place_list(lots[dist], [spec], self.street, jitter=60, report=False)
            if not got:
                made.append(spec.key)
                self.named[spec.key]['tenement'] = kind == 'tenement'
        self.rows_made = made

    # -- the Cinders -------------------------------------------------------------------------------------------------
    def cinders_specs(self):
        specs = [self.reuse_spec(g, scale=.74) for g, (d, k, a) in REUSE.items() if d == 'cinders']
        for i, gid in enumerate(self.houses.get('cinders', [])):
            specs.append(self.reuse_spec(gid, 'cinders', (216 + 9 * i, 444), 'home', scale=.66))
        specs += [
            Spec('r2_forge_row', "Timberland's Forge Row", 'cinders', 'shop', homes=1, near=(262, 380),
                 business='smithy', claims=['house_mossgrave'],
                 make=lambda: self.new_shop('smith', "Timberland's Forge Row", 'A smithy of House Timberland\'s on the '
                                            'Cinders: horseshoes, hinges and cart iron, the forge never let out.',
                                            style='works')),
            Spec('r2_cinder_wheelwrights', 'Cinder Wheelwrights', 'cinders', 'works', near=(230, 430),
                 business='cartwright',
                 make=lambda: self.new_works('cartwright', 'Cinder Wheelwrights', 'Wheels and axles for the city\'s carts '
                                             'and the Houses\' wagons.', 2, size=(16, 12))),
        ]
        return specs

    def food_addition(self, district):
        """The food rebalance's new buildings (doc 67, Progress), each placed after its quarter's large buildings and
        from its own random sequence, so the rest of the city is drawn as before: a butcher for the Cinders' crews, and
        the Carters' Yard inside the East Gate (the roads to Fellhall and the country), whose wagons take the city's
        goods out and bring the country's in."""
        specs = {
            'cinders': Spec('r2_cinder_butchers', 'Cinder Butchers', 'cinders', 'shop', homes=1, near=(280, 432),
                            business='butcher',
                            make=lambda: Building('shop', 'Cinder Butchers', 'works', *self.own_shop(
                                'butcher', 'Cinder Butchers', 'Mutton, pork and rabbit for the forges\' and the kilns\' '
                                'crews, salt pork by the slab, and bones for the pot at a farthing.', 'works'), '', '',
                                'butcher')),
            'commons': Spec('r2_carters_yard', "The Carters' Yard", 'commons', 'works', near=(452, 290),
                            business='carters',
                            make=lambda: self.new_works('carters_yard', "The Carters' Yard", 'Inside the East Gate, '
                                                        'where the Fellhall road and the country road come in: the '
                                                        'teams go out loaded before dawn and come back after dark.', 7,
                                                        style='ridgemere', size=(22, 13))),
        }
        spec = specs[district]
        rng, self.rng = self.rng, random.Random(f'{self.world.seed}:{spec.key}')
        try:
            for gap, jitter in ((None, 6), (0, 6), (0, 40)):
                lots = Lots(self.ground_of(district), self.street) if gap is None else \
                    Lots(self.ground_of(district), self.street, gap=gap)
                if not self.place_list(lots, [spec], self.street, jitter=jitter, report=False):
                    return
                spec.near = None if jitter == 40 else spec.near
            self.unplaced.append(spec.key)
            print(f'  ridgemere2: {spec.name} not placed ({district})')
        finally:
            self.rng = rng

    # -- the Rise -------------------------------------------------------------------------------------------------------
    def rise(self):
        """Five Minor House mansions, each in a small railed garden (their grounds about 40% smaller than before), their
        gates on Rise Walk and the Mews; no walls."""
        c = self.c
        for (x0, y0, lw, lh), gid in zip(RISE_LOTS, MINOR_HOUSES):
            region = box(x0, x0 + lw, y0, y0 + lh)
            rail = region & ~box(x0 + 1, x0 + lw - 1, y0 + 1, y0 + lh - 1)
            c.paint(region, ',', None)
            c.paint(rail, '|', None)
            # The gate on the lot's north side (onto Rise Walk) or south side (onto the Mews), and a path to the door.
            north = y0 < 404
            gx = x0 + lw // 2
            gy = y0 if north else y0 + lh - 1
            gate = box(gx - 1, gx + 2, gy, gy + 1)
            c.paint(gate, 'f', None)
            spec = self.reuse_spec(gid, 'rise', None, 'villa')
            bw, bh = 14, 8
            hx = x0 + (lw - bw) // 2
            hy = y0 + lh - 1 - 3 - bh if north else y0 + 4
            path = box(gx - 1, gx + 2, y0 + 1, hy) if north else box(gx - 1, gx + 2, hy + bh, y0 + lh - 1)
            c.paint(path, 'f', None)
            out = box(gx - 1, gx + 2, y0 - 2, y0) if north else box(gx - 1, gx + 2, y0 + lh, y0 + lh + 2)
            c.paint(out & ~self.street, 'f', None)
            self.street |= gate | path | out
            self.put(spec, hx, hy, 'N' if north else 'S', bw, bh)
            garden = region & ~rail & ~self.built & ~self.street
            self.gardens_plain(garden, gid)
            fx, fy = x0 + 3, (y0 + 3 if not north else y0 + 4)
            c.paint(box(fx, fx + 2, fy, fy + 2), 'F', None)
            self.posts.setdefault('mansion_gardens', []).append((x0 + lw - 5, y0 + lh // 2))

    # -- the landmarks ---------------------------------------------------------------------------------------------------
    def fixed_landmarks(self):
        """The city's memorable places (doc 67, Dense city): the Bay Light on the Mole, the Clock Tower, the
        Wall-Builders, Pillory Square, the Mercy, the Cisterns and the Runnel, and the Hundred Steps."""
        c = self.c
        # The Bay Light: a lighthouse at the head of the Mole, two floors, the lamp at the top.
        low = Plan(9, 9, 'H', 'f', self.new_rng)
        top = Plan(9, 9, 'H', 'f', self.new_rng)
        top.g[8][4] = 'H'
        top.keep = set()
        stairs_between(low, top, 6, 6)
        low.put(2, 2, 'O')
        low.put(6, 2, 'x')
        low.put(2, 6, 'k')
        for x, y in ((3, 3), (5, 3), (3, 5), (5, 5)):
            top.put(x, y, 'i')
        light = Spec('r2_bay_light', 'The Bay Light', 'wharf', 'hall', claims=['house_grayrock'],
                     make=lambda: self.multi('hall', 'The Bay Light', [low, top], [(('', 6, 6), ('up', 6, 6))],
                                             ['', 'up'], [0, 1], ['The Bay Light', 'The Bay Light, the lamp room'],
                                             ['The foot of the lighthouse at the head of the Mole: oil casks, coils of '
                                              'wick and a stair winding up the wet stone, the bay slapping at the door.',
                                              'The lamp room at the top of the Bay Light: four great braziers behind '
                                              'thick glass, kept burning all night for the ships feeling their way in '
                                              'through the rain.'], 'civic', footprint=(8, 8)))
        self.put(light, 64, PIERS[0] - 1, 'E')
        self.world.water[PIERS[0] - 1:PIERS[0] + 7, 64:72] = False
        self.mole[PIERS[0] - 1:PIERS[0] + 7, 64:72] = True
        self.landmark('The Bay Light and the Mole', 'wharf', 'a stone breakwater running 100 tiles out into the bay '
                      'from the north pier, a two-storey lighthouse at its head', 68, PIERS[0] + 3,
                      [r['id'] for r in self.named['r2_bay_light']['rooms']])
        # The Clock Tower on Clock Square, where Abbey Row meets Gate Street.
        (qx, qy), (_, qh) = SQUARES['clock']
        low = Plan(8, 8, 'H', 'f', self.new_rng)
        up = Plan(8, 8, 'H', 'f', self.new_rng)
        up.g[7][4] = 'H'
        up.keep = set()
        stairs_between(low, up, 6, 5)
        low.put(2, 2, 'o')
        low.put(5, 2, 'o')
        up.put(3, 2, 'U')
        up.put(2, 4, 'x')
        tower = Spec('r2_clock_tower', 'The Clock Tower', 'northgate', 'hall', claims=['house_greatly'],
                     make=lambda: self.multi('hall', 'The Clock Tower', [low, up], [(('', 6, 5), ('up', 6, 5))],
                                             ['', 'up'], [0, 1], ['The Clock Tower', 'The Clock Tower, the bell loft'],
                                             ['The foot of the Clock Tower that House Greatly raised over Northgate: the '
                                              'weights hanging in their shaft and the iron works of the clock ticking '
                                              'above.', 'The bell loft: the great bell that rings the hours over the '
                                              'rain, the clock\'s four dials glowing at night, and the whole wet city '
                                              'below.'], 'civic', footprint=(7, 7)))
        self.put(tower, qx + 4, qy - qh - 7, 'S')
        self.landmark('The Clock Tower', 'northgate', 'a two-storey clock and bell tower on Clock Square, where Abbey '
                      'Row meets Gate Street', qx + 7, qy - qh - 4, [r['id'] for r in self.named['r2_clock_tower']['rooms']])
        # The Wall-Builders: the statue of House Greatly's masons on Greatly Square.
        (gx, gy), _ = SQUARES['greatly']
        c.paint(box(gx + 7, gx + 10, gy - 3, gy), 'S', None)
        self.landmark('The Wall-Builders', 'commons', 'a statue group of the first Greatly masons with trowel and plumb '
                      'line, on Greatly Square below the compound gate: "They raised the wall; the wall raised the city"',
                      gx + 8, gy - 2)
        # Pillory Square before the Keep gate: the gallows, the pillory and the stocks.
        (px, py), _ = SQUARES['pillory']
        for x in (px - 9, px - 6):
            c.stamp(x, py - 4, ['I'], None)                 # The gallows' posts.
        c.stamp(px + 6, py - 4, ['I'], None)                # The pillory.
        c.stamp(px + 5, py + 3, ['ppp'], None)              # The stocks.
        self.posts['pillory'] = [(px - 3, py), (px + 3, py + 1)]
        self.landmark('Pillory Square', 'commons', 'the square before the Old Keep\'s gate: the gallows, the pillory and '
                      'the stocks where the Watch shows its catch', px, py)
        # The Mercy: an old hulk hauled up on the quay at the Sump's edge, the Knucklebones inside it.
        hx0, hy0, hw, hh = QUAY_X, 318, 24, 11
        mask = np.zeros((hh, hw), dtype=bool)
        for x in range(hw):
            half_h = 5 if x < 14 else 5 - (x - 13) * .45
            for y in range(hh):
                mask[y, x] = abs(y - 5) <= half_h + .01
        mercy = Spec('r2_the_knucklebones', 'The Knucklebones', 'sump', 'tavern',
                     make=lambda: self.new_tavern('The Knucklebones', 'A dice-and-card tavern in the hull of the Mercy, '
                                                  'an old Grayrock carrack hauled up the quay and left: ship\'s lamps '
                                                  'over every table, the ribs overhead, the house\'s cut on every pot, '
                                                  'and nobody asks where the stake came from.', footprint=(11, 24)))
        rec = self.put(mercy, hx0, hy0, 'W', mask=mask)
        whole = np.zeros((H, W), dtype=bool)
        whole[hy0:hy0 + hh, hx0:hx0 + hw] = mask
        inner = whole.copy()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            inner &= np.roll(whole, (dy, dx), (0, 1))
        hull = whole & ~inner & (c.codes != ord('+'))
        c.paint(hull, '<', None)
        c.paint(inner, 'L', None)
        c.stamp(hx0 + 12, hy0 + 5, ['I'], None)             # The stump of her mast.
        self.landmark('The Mercy', 'sump', 'an old Grayrock carrack hauled up on the quay at the Sump\'s edge, her hull '
                      'the Knucklebones tavern', hx0 + 10, hy0 + 5, [r['id'] for r in rec['rooms']])
        # The Cisterns: three stone rain tanks on a court in the Sump, and the Runnel, the gutter channel down to the
        # Gut and along it to the quay.
        cx0, cy0 = 262, 272
        court = box(cx0, cx0 + 16, cy0, cy0 + 11)
        c.paint(court, 'f', None)
        for i in range(3):
            tx = cx0 + 1 + 5 * i
            c.stamp(tx, cy0 + 1, ['QQQQ', 'QUUQ', 'QUUQ', 'QQQQ'], None)
            self.built[cy0 + 1:cy0 + 5, tx:tx + 4] = True
        self.street |= court & ~self.built
        self.cistern_court = court
        run = self.c.line_mask([(cx0 + 8.5, cy0 + 11), (cx0 + 8.5, 296)], 3) & self.D('sump') & ~self.built
        c.paint(run, '.', None)
        self.street |= run
        gut_edge = self.c.line_mask([(x, y + 1.5) for x, y in self.gut_line], 1) & self.c.line_mask(self.gut_line, 3)
        channel = (self.c.line_mask([(cx0 + 8.5, cy0 + 11), (cx0 + 8.5, 296)], 1) & run) | \
            (gut_edge & ~box(334, 400, 250, 330) & ~self.D('wharf'))
        c.paint(channel, '~', None)
        self.runnel = channel
        self.landmark('The Cisterns and the Runnel', 'sump', 'three stone rain tanks on Cistern Court that feed the '
                      'bathhouse, and the Runnel, an open gutter channel running from them down the Gut to the quay',
                      cx0 + 8, cy0 + 6)
        # The Hundred Steps: a stair-street climbing from the quay up through the north Sump to Abbey Row.
        steps = [(QUAY_X - 1.0, 236.5), (238.0, 236.5), (262.0, 224.5), (294.5, 224.5), (294.5, 188.0)]
        m = self.c.line_mask(steps, 2) & self.inside & ~self.rings & ~self.built & ~self.D('abbey')
        c.paint(m, '^', None)
        self.street |= m
        self.hundred_steps = m
        self.landmark('The Hundred Steps', 'sump', 'a stair-street two wide climbing from the quay up through the north '
                      'Sump and the Northgate edge to Abbey Row', 262, 230)

    # -- the Sump ---------------------------------------------------------------------------------------------------------
    def sump_named(self):
        """The Sump's own places on the Gut, Tarry Lane and the quay (its shops go later, on its edge)."""
        S = self.D('sump')
        named = [self.reuse_spec(g) for g, (d, k, a) in REUSE.items() if d == 'sump' and k != 'shop']
        named += [
            Spec('r2_the_washhouse', 'The Washhouse', 'sump', 'works', near=(230, 330), business='laundry',
                 make=lambda: self.new_works('washhouse', 'The Washhouse', 'Where the Sump and half the Commons send '
                                             'their washing: a penny a bundle, back by dark, still damp.', 6,
                                             style='ridgemere', size=(18, 12))),
            # (The food rebalance: the Sump's own bakery where the rag-and-bone yard stood; the Rag Market keeps its
            # second-hand dealer.)
            Spec('r2_penny_loaf', 'The Penny Loaf', 'sump', 'works', near=(250, 336), business='bakery',
                 make=lambda: self.new_works('bakehouse', 'The Penny Loaf', 'The Sump\'s bakehouse: coarse brown loaves '
                                             'a penny each from before dawn, the oven\'s warmth leaking out into the '
                                             'alley, and a queue in the rain.', 3, style='ridgemere', size=(14, 10))),
            Spec('r2_lamp_store', 'The Lamp Store', 'sump', 'hall', near=(280, 260),
                 make=lambda: self.new_hall('warehouse', 'The Lamp Store', 'The lamplighters\' post: ladders, wicks, '
                                            'casks of fish oil and the city\'s spare lanterns.', 12, 9)),
        ]
        # The Sunken Street: the city's old street level, under the Sump's edge, with a stair-house at each end.
        sunk = Plan(54, 7, '#', '_', self.new_rng)
        for x in range(3, 52, 6):
            sunk.g[0][x] = 'j'
            sunk.g[6][x] = 'j' if (x, 6) != sunk.door else sunk.g[6][x]
            sunk.put(x, 1, 'I')
            sunk.put(x, 5, 'I')
        for x, y, ch in ((8, 3, '~'), (9, 3, '~'), (20, 2, 'X'), (21, 2, 'X'), (33, 4, '~'), (40, 2, 'O'), (46, 4, 'X'),
                         (14, 4, 'O'), (27, 1, 'S')):
            sunk.put(x, y, ch)
        self.sunken = Spec('r2_sunken_street', 'The Sunken Street', 'sump', 'hall', near=(276, 320),
                           make=lambda: Building('hall', 'The Sunken Street', 'ridgemere', (6, 6),
                                                 [sunk.room('', 'The Sunken Street', 'The old street level of Ridgemere, '
                                                            'paved over when the Greatlys raised the city on its new '
                                                            'walls: a cobbled lane under the houses, its shopfronts '
                                                            'bricked up, water dripping from the vaults, lamps where '
                                                            'someone has hung them.', z=-1)], [], '', 'Q', ''))
        named.append(self.sunken)
        lanes = (self.street & S) | (self.quay & grow(S, 2))
        ground = S & ~self.street & ~self.reserved & ~self.built
        lots = Lots(ground, lanes)
        self.place_list(lots, sorted(named, key=lambda s: -s.area), lanes, jitter=3, deep=True)
        self.landmark('The Sunken Street', 'sump', 'the city\'s old street level under the Sump\'s edge, a vaulted cobbled '
                      'lane 54 tiles long, reached by a stair-house in the Sump and another in the Commons',
                      *self.named['r2_sunken_street']['outside'].values(),
                      [r['id'] for r in self.named['r2_sunken_street']['rooms']])

    def sunken_second_door(self):
        """The Sunken Street's other stair-house, in the Commons: a small house whose door leads down into its east
        end."""
        c = self.c
        rec = self.named['r2_sunken_street']
        room = self.site_room(rec['rooms'][0]['id'])
        ground = self.ground_of('commons') & self.inside
        lots = Lots(ground, self.street, gap=0)
        got = self.nearest_lot(lots, 6, 6, (318, 322), self.street)
        if not got:
            return
        _, x0, y0, facing, w, h = got
        c.rect(x0, y0, w, h, '#', None)
        c.rect(x0 + 1, y0 + 1, w - 2, h - 2, 'Q', None)
        (dx, dy), (ox, oy) = door_tile(x0, y0, w, h, facing)
        c.codes[dy, dx] = ord('+')
        c.heights[y0:y0 + h, x0:x0 + w] = c.heights[oy, ox]
        self.built[y0:y0 + h, x0:x0 + w] = True
        side = 'E' if room['width'] > room['height'] else 'S'
        end = self.carve(room, side)
        cid, lx, ly = self.site.cell_at(dx, dy)
        self.site.links.append({'id': f'{room["id"]}_door_east', 'name': 'The Sunken Street, the Commons stair',
                                'kind': 'door', 'open': True, 'a': {'cell': cid, 'x': lx, 'y': ly},
                                'b': {'cell': room['id'], 'x': end[0], 'y': end[1]}})
        self.sunken_east = (dx, dy)

    def sump_alleys(self):
        """A crooked grid of alleys one and two tiles wide, 8 to 13 apart, cut for dead ends and bent into doglegs;
        courts in the bigger blocks reached by snickets."""
        c, rng = self.c, self.rng
        S = self.D('sump')
        self.infill.update({'boarded': 0, 'yards': 0, 'courts': 0, 'dead_ends': 0})
        ys_, xs_ = np.nonzero(S)
        bx0, bx1, by0, by1 = int(xs_.min()), int(xs_.max()) + 1, int(ys_.min()), int(ys_.max()) + 1
        xs, x = [bx0 + rng.randint(7, 11)], bx0
        x = xs[0]
        while x < bx1 - 6:
            x += rng.randint(12, 17)
            xs.append(min(x, bx1 - 1))
        ys, y = [by0 + 1], by0 + 1
        while y < by1 - 6:
            y += rng.randint(11, 16)
            ys.append(min(y, by1 - 1))
        node = {(i, j): (float(xs[i] + (rng.randint(-2, 2) if 0 < i < len(xs) - 1 else 0)),
                         float(ys[j] + (rng.randint(-2, 2) if 0 < j < len(ys) - 1 else 0)))
                for i in range(len(xs)) for j in range(len(ys))}
        segments = [((i, j), (i, j + 1)) for i in range(len(xs)) for j in range(len(ys) - 1)] + \
                   [((i, j), (i + 1, j)) for i in range(len(xs) - 1) for j in range(len(ys))]
        rng.shuffle(segments)
        alleys = np.zeros((H, W), dtype=bool)
        narrow = np.zeros((H, W), dtype=bool)
        stubs = []
        free = S & ~self.built & ~self.reserved & ~self.hundred_steps & ~self.runnel & ~self.cistern_court
        for a, b in segments:
            pa, pb = node[a], node[b]
            r = rng.random()
            if r < .25:                                       # Cut: the blocks either side merge...
                if rng.random() < .55:                        # ...or half of it stays, a dead end.
                    mid = ((pa[0] + pb[0]) / 2, (pa[1] + pb[1]) / 2)
                    pts = [pa, mid] if rng.random() < .5 else [pb, mid]
                    m = c.line_mask(pts, 2) & free
                    alleys |= m
                    stubs.append(pts)
                continue
            pts = [pa, pb]
            if rng.random() < .15:                            # A dogleg.
                off = rng.choice([-1, 1]) * rng.randint(2, 4)
                mid = ((pa[0] + pb[0]) / 2, (pa[1] + pb[1]) / 2)
                if a[0] == b[0]:
                    pts = [pa, (pa[0], mid[1]), (pa[0] + off, mid[1]), (pb[0] + off, pb[1])]
                else:
                    pts = [pa, (mid[0], pa[1]), (mid[0], pa[1] + off), (pb[0], pb[1] + off)]
            width = 2 if rng.random() < .78 else 1
            if width == 1:
                pts = [(x + .5, y + .5) for x, y in pts]
            m = c.line_mask(pts, width) & free
            alleys |= m
            if width == 1:
                narrow |= m
        alleys &= ~self.rag_stalls & ~self.cistern_court & ~self.hundred_steps & ~self.runnel
        # Courts: small yards inside the biggest blocks, each reached by a one-tile snicket.
        open_ = (alleys | (self.street & S)) & ~self.built
        self.blocks_mask, blocks = label(S & ~open_ & ~self.built & ~self.reserved)
        courts = np.zeros((H, W), dtype=bool)
        snickets = np.zeros((H, W), dtype=bool)
        made = 0
        for lab, (bx0, bx1, by0, by1, area) in sorted(blocks.items(), key=lambda kv: -kv[1][4]):
            if made >= 6:
                break
            bw, bh = bx1 - bx0, by1 - by0
            if bw < 11 or bh < 10:
                continue
            cw, ch_ = rng.randint(6, min(7, bw - 5)), rng.randint(6, min(7, bh - 4))
            cx0, cy0 = bx0 + (bw - cw) // 2, by0 + (bh - ch_) // 2
            court = box(cx0, cx0 + cw, cy0, cy0 + ch_)
            inner_block = self.blocks_mask == lab
            if not (court & inner_block).sum() == cw * ch_:
                continue
            best = None
            for side, (sx, sy, dx, dy) in {'W': (cx0 - 1, cy0 + ch_ // 2, -1, 0), 'E': (cx0 + cw, cy0 + ch_ // 2, 1, 0),
                                           'N': (cx0 + cw // 2, cy0 - 1, 0, -1), 'S': (cx0 + cw // 2, cy0 + ch_, 0, 1)}.items():
                path = []
                x, y = sx, sy
                for _ in range(20):
                    if open_[y, x]:
                        if best is None or len(path) < len(best):
                            best = path[:]
                        break
                    if not S[y, x] or self.built[y, x] or self.reserved[y, x]:
                        break
                    path.append((x, y))
                    x, y = x + dx, y + dy
            if best is None or not best:
                continue
            courts |= court
            for x, y in best:
                snickets[y, x] = True
            made += 1
        c.paint(courts, 'D', None)
        c.paint(snickets, '.', None)
        self.infill['courts'] = made
        self.court_mask = courts
        self.snickets = snickets
        c.paint(alleys & ~self.built, '.', None)
        self.alleys = alleys | snickets
        self.street |= (alleys | snickets | courts) & ~self.built
        self.sump_stubs = stubs
        self.sump_narrow = narrow

    def sump_homes(self):
        """The Sump's own shops on its edge with the Commons (its better street), then its houses: three homes to most
        (never more), on small floors, packed along the alleys, terraced or a snicket apart; then whatever is left is
        sheds, boarded-up houses and walled yards."""
        c, rng = self.c, self.rng
        S = self.D('sump')
        open_ = (self.alleys | (self.street & S) | self.court_mask) & ~self.built
        self.sump_open_planned = open_.copy()
        lab_o, _ = label(open_ | (self.quay & grow(S, 3)))
        (gx, gy), _ = SQUARES['rag']
        joined = lab_o == lab_o[gy, gx]
        door_street = (open_ | (self.quay & grow(S, 2))) & joined & ~self.court_mask & ~self.snickets & ~self.sump_narrow
        # 1. The shops, on the edge toward the Commons.
        edge = S & (self.wealth > .3)
        shops = [self.reuse_spec(g) for g, (d, k, a) in REUSE.items() if d == 'sump' and k == 'shop']
        for s in shops:
            s.near = self.random_point('sump', edge) if edge.any() else s.near
        lots = Lots(S & ~open_ & ~self.built & ~self.reserved, door_street, gap=0)
        self.place_list(lots, shops, door_street, jitter=3, deep=True)
        # 2. The houses.
        homes = []
        flats_sizes = [(6, 6), (7, 6), (8, 6), (7, 7), (8, 7), (9, 6), (6, 7), (6, 8), (9, 7), (10, 6), (7, 8), (10, 7),
                       (8, 8), (11, 7), (6, 9)]
        cottage_sizes = [(5, 5), (6, 5), (5, 6), (6, 6)]
        want = [('flats', rng.choice(flats_sizes), 3) for _ in range(46)] + \
               [('flats', rng.choice(flats_sizes), 2) for _ in range(SUMP_TWOS)] + \
               [('cottage', rng.choice(cottage_sizes), 1) for _ in range(SUMP_COTTAGES)]
        want.sort(key=lambda t: -t[1][0] * t[1][1])
        offered = 0
        last = None
        for kind, (bw, bd), n in want:
            if offered >= SUMP_HOMES and n == 3:
                continue
            lots.street = door_street
            near = last if last and rng.random() < .85 else self.random_point('sump')
            got = None
            for orient in ((bw, bd), (bd, bw)):
                hit = self.nearest_lot(lots, orient[0], orient[1], near, door_street, deep=True)
                if hit and hit[0] < 70:
                    got = (hit[1], hit[2], hit[3], hit[4], hit[5], orient)
                    break
            if not got:
                continue
            x0, y0, facing, w, h, orient = got
            lots.gap = 0 if rng.random() < .7 else 1
            lots.take(x0, y0, w, h)
            self.built[y0:y0 + h, x0:x0 + w] = True
            homes.append({'kind': kind, 'x0': x0, 'y0': y0, 'w': w, 'h': h, 'facing': facing,
                          'fw': orient[0], 'fh': orient[1], 'n': n})
            last = (x0 + w / 2, y0 + h / 2)
            offered += n
        # 3. Snickets: where a house's back is a few tiles from an alley, a passage one wide is cut through to it, and
        # the house gets a back door (a through-building).
        for hm in homes:
            x0, y0, w, h, facing = hm['x0'], hm['y0'], hm['w'], hm['h'], hm['facing']
            (bx, by), (ox, oy) = door_tile(x0, y0, w, h, OPPOSITE[facing])
            sx, sy = DOOR_STEP[OPPOSITE[facing]]
            path, x, y = [], ox, oy
            for _ in range(5):
                if not (0 <= x < W and 0 <= y < H) or cell_of(x, y) != cell_of(bx, by):
                    path = None
                    break
                if open_[y, x] and not self.court_mask[y, x]:
                    break
                if not S[y, x] or self.built[y, x] or self.reserved[y, x] or self.street[y, x]:
                    path = None
                    break
                path.append((x, y))
                x, y = x + sx, y + sy
            else:
                path = None
            if path and rng.random() < .8:
                for x, y in path:
                    c.codes[y, x] = ord('.')
                    self.street[y, x] = True
                    open_[y, x] = True
                    self.snickets[y, x] = True
                self.infill['back_snickets'] += 1
        # 4. Infill: what is left inside the blocks.
        self.sump_infill(S, open_)
        # 4. The buildings themselves: back doors where the back opens onto a passage, L-shapes, their homes.
        names = sump_names(rng, len(homes))
        open_now = (S | self.D('wharf')) & ~SOLID[c.codes] & ~self.built
        for i, hm in enumerate(homes):
            x0, y0, w, h, facing = hm['x0'], hm['y0'], hm['w'], hm['h'], hm['facing']
            fw, fh = (w, h) if facing in 'NS' else (h, w)
            (bx, by), (ox, oy) = door_tile(x0, y0, w, h, OPPOSITE[facing])
            back = (0 <= ox < W and 0 <= oy < H and open_now[oy, ox] and cell_of(bx, by) == cell_of(ox, oy))
            notch = None
            mask = None
            if hm['kind'] == 'flats' and min(fw, fh) >= 6 and fw * fh >= 42 and rng.random() < .6:
                nw, nh = max(2, round(fw * .3)), max(2, round(fh * .3))
                corner = rng.choice(['NE', 'NW'])
                plan_mask = [[True] * fw for _ in range(fh)]
                for yy in range(nh):
                    for xx in (range(fw - nw, fw) if 'E' in corner else range(nw)):
                        plan_mask[yy][xx] = False
                mask = np.array(rotate_rows(plan_mask, TURNS[facing]), dtype=bool)
                iw, ih = sump_interior(fw, fh)
                notch = (corner, max(2, round(nw * iw / fw)), max(2, round(nh * ih / fh)))
                if back and not mask[by - y0, bx - x0]:
                    back = False
            iw, ih = sump_interior(fw, fh)
            n = hm['n']
            beds = tuple(rng.choice([2, 2, 3, 3, 3, 4]) for _ in range(n))
            name = names[i]
            b = self.new_flats(name, n, beds, (iw, ih), (fw, fh), notch=notch, back=back, style='ridgemere')
            spec = Spec(f'sump_home_{i}', name, 'sump', 'flats' if n > 1 else 'home', homes=n, open_door=False)
            spec.building = b
            rec = self.put(spec, x0, y0, facing, mask=mask)
            rec['through'] = bool(back)
            rec['lshape'] = notch is not None
            rec['sump_home'] = True
            hm['record'] = rec
        self.sump_homes_ = homes
        self.sump_passages(homes)
        self.infill['dead_ends'] = len(self.sump_stubs)

    def sump_infill(self, S, open_):
        """Sheds, boarded-up houses and walled back-yards in whatever the buildings left inside the blocks."""
        c, rng = self.c, self.rng
        left = S & ~open_ & ~self.built & ~self.reserved & ~self.street
        lab, comps = label(left)
        for k, (x0, x1, y0, y1, area) in comps.items():
            comp = lab == k
            if area <= 2:
                continue
            for kind, sizes in (('boarded', [(12, 9), (10, 8), (9, 7), (8, 7), (7, 6)]),
                                ('shed', [(6, 4), (5, 4), (4, 4), (4, 3), (3, 3)])):
                for _ in range(8):
                    placed = False
                    for bw, bh in sizes:
                        spot = fit_rect(comp, bw, bh) or fit_rect(comp, bh, bw)
                        if spot:
                            rx, ry, rw, rh = spot
                            c.stamp(rx, ry, boarded(rw, rh) if kind == 'boarded' else shed(rw, rh), None)
                            comp[ry:ry + rh, rx:rx + rw] = False
                            self.built[ry:ry + rh, rx:rx + rw] = True
                            self.infill['boarded' if kind == 'boarded' else 'sheds'] += 1
                            placed = True
                            break
                    if not placed:
                        break
            rest = comp & ~self.built
            if rest.sum() >= 6:
                touch = rest & grow(open_, 1)
                c.paint(touch, '|', None)
                c.paint(rest & ~touch, rng.choice('.,'), None)
                ry, rx = np.nonzero(rest & ~touch)
                for j in range(0, len(rx), 9):
                    c.codes[ry[j], rx[j]] = ord(rng.choice('xO'))
                self.infill['yards'] += 1

    def sump_passages(self, homes):
        """Passages between neighbouring Sump buildings (back rooms knocked through, cellar and attic runs): chains of
        three to five buildings, a passage on the ground floor of each pair and another upstairs where both have one."""
        recs = [hm['record'] for hm in homes]
        nbr = defaultdict(list)
        for i, a in enumerate(recs):
            ax0, ay0, aw, ah = a['block']['x'], a['block']['y'], a['block']['w'], a['block']['h']
            for j, b in enumerate(recs):
                if j <= i:
                    continue
                bx0, by0, bw, bh = b['block']['x'], b['block']['y'], b['block']['w'], b['block']['h']
                if 0 <= bx0 - (ax0 + aw) <= 4 or 0 <= ax0 - (bx0 + bw) <= 4:
                    overlap = min(ay0 + ah, by0 + bh) - max(ay0, by0)
                    if overlap >= 3:
                        side = 'E' if bx0 >= ax0 + aw else 'W'
                        nbr[i].append((j, side))
                        nbr[j].append((i, OPPOSITE[side]))
                elif 0 <= by0 - (ay0 + ah) <= 4 or 0 <= ay0 - (by0 + bh) <= 4:
                    overlap = min(ax0 + aw, bx0 + bw) - max(ax0, bx0)
                    if overlap >= 3:
                        side = 'S' if by0 >= ay0 + ah else 'N'
                        nbr[i].append((j, side))
                        nbr[j].append((i, OPPOSITE[side]))
        used = set()
        chains = []

        def longest(path, seen):
            if len(path) == 5:
                return path
            best = path
            for j, s in nbr[path[-1][0]]:
                if j not in seen:
                    got = longest(path + [(j, s)], seen | {j})
                    if len(got) > len(best):
                        best = got
                        if len(best) == 5:
                            break
            return best
        while len(chains) < 6:
            best = []
            for start in sorted(nbr):
                if start in used:
                    continue
                chain = longest([(start, None)], {start} | used)
                if len(chain) > len(best):
                    best = chain
                    if len(best) == 5:
                        break
            if len(best) < 3:
                break
            chains.append(best)
            used |= {k for k, _ in best}
        made = 0
        self.sump_chains = []
        for chain in chains:
            ids = []
            for (a, _), (b, side) in zip(chain, chain[1:]):
                ra, rb = recs[a], recs[b]
                ga, gb = ra['rooms'][0]['id'], rb['rooms'][0]['id']
                if self.passage(ga, side, gb, OPPOSITE[side], f'{ra["name"]} to {rb["name"]}'):
                    made += 1
                for z in (1, 2):
                    ua = [r['id'] for r in ra['rooms'] if r['z'] == z]
                    ub = [r['id'] for r in rb['rooms'] if r['z'] == z]
                    if ua and ub and self.passage(ua[0], side, ub[0], OPPOSITE[side],
                                                  f'{ra["name"]} to {rb["name"]}, floor {z}'):
                        made += 1
                ids.append(ra['id'])
            ids.append(recs[chain[-1][0]]['id'])
            self.sump_chains.append(ids)
        self.infill['passages'] = made

    # -- outside the walls ------------------------------------------------------------------------------------------------
    def outside(self):
        c, w = self.c, self.world
        # The land eases up to the walls' feet: within 30 tiles, toward the level of the ground inside.
        lv = np.where(np.isnan(self.levels_map), -1, self.levels_map).astype(np.float32)
        lv[self.wall_band] = -1
        for key, m in self.gate_masks.items():
            lv[m] = w.gate_levels[('ridgemere', key)]
        known = lv >= 0
        t = lv.copy()
        for _ in range(32):
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                sh = np.roll(t, (dy, dx), (0, 1))
                fill = ~known & (sh >= 0)
                t[fill] = sh[fill]
                known |= fill
        near = field.distance_to_mask(self.inside | self.wall_band, 32)
        weight = 1 - field.smoothstep(3, 30, near)
        freem = ~c.locked & ~w.water & (t >= 0) & ~self.inside & ~self.wall_band
        c.heights[freem] = half(c.heights * (1 - weight) + t * weight)[freem]
        w.places['ridgemere'] = box(160, 480, 142, 456)
        # The three Houses' estates (doc 67, R26), each a walled compound within one cell: the seat, its gatehouse,
        # stables and cottages; no fields, orchards, vineyards or pens. Their grounds are smaller than they were.
        gates = {'mossgrave': ('N', (356, 518)), 'fell': ('W', (544, 310)), 'grayrock': ('N', (190, 640))}
        for house, (seat, (x, y, ew, eh)) in ESTATES.items():
            sid = f'estate_{house}'
            side, target = gates[house]
            region, ground, level, insides = self.compound(sid, x, y, ew, eh, 'H', ',', {side: target})
            avenues = self.estate_ground(sid, region, ground, level, insides[side])
            self.street |= avenues
            self.levels_map[region] = level
            specs = self.estate_specs(house, sid)
            lots = Lots(ground & ~avenues, avenues)
            seat_spec = specs[0]
            seat_spec.near = self.districts[f'{sid}_seat']
            for sp in specs[1:]:
                gx, gy = self.districts[f'{sid}_works']
                sx_, sy_ = self.districts[f'{sid}_seat']
                sp.near = ((gx * 2 + sx_) / 3, (gy * 2 + sy_) / 3)
            self.place_list(lots, [seat_spec], avenues, jitter=2)
            self.place_list(lots, sorted(specs[1:], key=lambda s: -s.area), avenues, jitter=4)
            self.gardens_plain(lots.open & ground & ~avenues, sid, trees='$' if house == 'fell' else 'Y')
            self.spots[f'{sid}_gate'] = insides[side]
            self.posts[f'{sid}_gardens'] = [self.districts[f'{sid}_seat']]
            self.posts[f'{sid}_gate'] = [insides[side]]
        # The roads: out of the North, East and South Gates to the canvas's edge, the Cinder Road to Timberland Court
        # and the Wharf Road on from the Harbour Gate to Grayrock Hall.
        roads = {
            'The North Road': ([w.gates[('ridgemere', 'N')], (360.5, 0.0)], 'N'),
            'The South Road': ([w.gates[('ridgemere', 'S')], (412.5, 767.0)], 'S'),
        }
        for name, (pts, key) in roads.items():
            w.road(name, pts, width=3, ends=(w.gate_levels[('ridgemere', key)], None))
        fell_gate = w.gates[('estate_fell', 'W')]
        east = [w.gates[('ridgemere', 'E')], (fell_gate[0] - 10, 300.5), fell_gate]
        w.road('The East Road', east, width=3, ends=(w.gate_levels[('ridgemere', 'E')], w.gate_levels[('estate_fell', 'W')]))
        on = w.route((fell_gate[0] - 10, 300.5), (767.0, 250.0), w.route_costs())
        w.road('The East Road, past Fellhall', on, width=3)
        line = w.route(w.gates[('ridgemere', 'cinder')], w.gates[('estate_mossgrave', 'N')], w.route_costs())
        w.road('The Cinder Road', line, width=3, ends=(w.gate_levels[('ridgemere', 'cinder')],
                                                       w.gate_levels[('estate_mossgrave', 'N')]))
        line = w.route(w.gates[('ridgemere', 'harbour')], w.gates[('estate_grayrock', 'N')], w.route_costs())
        w.road('The Wharf Road', line, width=3, ends=(.5, w.gate_levels[('estate_grayrock', 'N')]))
        w.mark_cliffs()

    def estate_specs(self, house, sid):
        S = 'ridgemere'
        seat = {'mossgrave': 'rm_mossgrave_court', 'fell': 'rm_fellhall', 'grayrock': 'rm_grayrock_hall'}[house]
        out = [self.reuse_spec(seat, sid, None, 'seat')]
        for gid, (d, k, a) in REUSE.items():
            if d == sid and gid != seat:
                out.append(self.reuse_spec(gid))
        title = {'mossgrave': 'Timberland', 'fell': 'Fell', 'grayrock': 'Grayrock'}[house]
        out.append(Spec(f'r2_{house}_cottage', f'{title} Lodge Cottage', sid, 'home', homes=1, open_door=False,
                        make=lambda: self.new_house(f'{title} Lodge Cottage', 3, 'An estate cottage for the grooms and '
                                                    'gardeners, whitewashed once and grey now.', footprint=(8, 6))))
        if house == 'fell':
            out += [
                Spec('r2_fell_press_house', 'The Fell Press House', sid, 'works', business='press', claims=['house_fell'],
                     make=lambda: self.new_works('press_house', 'The Fell Press House', 'House Fell\'s press house: the '
                                                 'great screw press, treading vats and the new wine in its casks.', 3,
                                                 style=S, size=(18, 12))),
                Spec('r2_fell_distillery', 'The Fell Distillery', sid, 'works', business='distillery',
                     claims=['house_fell'],
                     make=lambda: self.new_works('distillery', 'The Fell Distillery', 'House Fell\'s still house, where '
                                                 'its wine becomes brandy and aqua vitae.', 3, style=S)),
                Spec('r2_fell_preserving_house', 'The Fell Preserving House', sid, 'works', business='preserver',
                     claims=['house_fell'],
                     make=lambda: self.new_works('preserving_house', 'The Fell Preserving House', 'Where House Fell\'s '
                                                 'fruit is put up for the winter: preserves, cordials and dried apples.',
                                                 2, style=S)),
                Spec('r2_fell_cellars', 'The Fell Cellars', sid, 'store', claims=['house_fell'],
                     make=lambda: self.new_hall('warehouse', 'The Fell Cellars', 'House Fell\'s cellars: wine and brandy '
                                                'ageing in the cool, cask after cask.', 12, 22)),
            ]
        return out

    # -- patrols --------------------------------------------------------------------------------------------------------
    def patrols(self):
        """Posts for every route in use (doc 67, section 4): the wall walk round the abbey and the Greatly compound by
        street, the streets, the wharf, the Sump, the palace round, and Lord Grayrock's road."""
        self.route_points = {
            'rm_wall_walk': [(292, 155), (360, 155), (388, 155), (430, 232), (466, 240), (466, 320), (466, 400),
                             (466, 450), (414, 450), (360, 450), (300, 450), (190, 440), (190, 330), (190, 170)],
            'rm_street_patrol': [(460, 300), (400, 300), (360, 316), (360, 340), (360, 365), (330, 382), (327, 397),
                                 (412, 311), (384, 330), (360, 300)],
            'rm_wharf_patrol': [(190, 170), (190, 240), (190, 293), (190, 350), (190, CUSTOMS_PIER + 3), (190, 420),
                                (190, 446)],
            'r2_sump_patrol': [(336, 300), (300, 298), (270, 297), (244, 302), (220, 295), (249, 260), (250, 222),
                               (248, 320), (248, 340), (270, 284)],
            'r2_palace_round': [(295, 397), (336, 397), (378, 397), (381, 415), (336, 436), (289, 415)],
            'rm_grayrock_journey': [(190, 300), (190, 440), self.world.gates[('ridgemere', 'harbour')],
                                    self.spots['estate_grayrock_gate']],
        }


# ---------------------------------------------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------------------------------------------
STYLE_ROOF = {'ridgemere': 'Z', 'manor': 'Z', 'works': 'Z', 'civic': 'Q', 'palace': '['}
# How many homes the city's new houses offer (doc 67, Dense city): the Sump's houses three homes to most, never more;
# the middle town's new row houses two and three, its Sump-edge tenements three.
SUMP_HOMES, SUMP_TWOS, SUMP_COTTAGES = 84, 3, 3
NEIGHBOUR = {'cinders': 'admin', 'admin': 'cinders', 'commons': 'northgate', 'northgate': 'commons'}
TENEMENTS, ROWS_COMMONS, ROWS_COMMONS_2, ROWS_NORTHGATE = 4, 9, 3, 3
ROW_NAMES = ['Lantern Row', 'Saltgate Row', 'Rainford Row', 'Two Gables', 'Kettle Close', 'Ash Walk Houses',
             'Mossy Steps Houses', "Chandlers' Row", 'Wet Lane Houses', 'Greystone Row', 'Pewter Row', 'Slate Row',
             'Cooper\'s Close', 'Weavers\' Terrace', 'Gutter Lane Houses', 'Bell Row', 'Clockface Houses', 'Mason Row',
             'Tallow Row', 'Drizzle Row']
ROW_PROSE = ('A home on one floor of a narrow row house of the Commons, two or three families stacked up its stair: a '
             'hearth, a table, beds against the wall and a window on the lane.')
SOLID_CHARS = set(terrain_catalog.SOLID)
WALL_CHARS = set('#HM]')
MINOR_HOUSES = ['rm_carlisle_house', 'rm_thornbury_house', 'rm_vance_house', 'rm_stanmore_house', 'rm_grantham_house']
DOUBLE_NAMES = ['Lantern Row Houses', 'Saltgate Houses', 'Rainford Pair', 'Two Gables', 'Kettle Close Houses',
                'Ash Walk Houses', 'Mossy Steps Houses', 'Chandlers\' Pair', 'Wet Lane Houses', 'Greystone Pair']
FLATS_PROSE = [
    'A home of one or two low rooms in a crowded house of the Sump: a hearth, a table, beds against the damp wall, and '
    'the neighbours through the floor.',
    'Rented rooms in the Sump: a pot on the fire, washing strung across the ceiling, and rain in the bucket by the door.',
    'Two rooms up a stair that leans: patched blankets, a crate for a table, and a window onto the alley below.',
    'A family\'s rooms in a Sump house: everything they own within reach of the fire, and the smell of the wharf.',
]
SUMP_HEADS = ['Gull', 'Tar', 'Drip', 'Brine', 'Soot', 'Rope', 'Kelp', 'Mud', 'Rain', 'Bilge', 'Tallow', 'Cinder', 'Fog',
              'Wick', 'Net', 'Salt', 'Gutter', 'Lantern', 'Crab', 'Eel', 'Pitch', 'Oakum', 'Rag', 'Bucket', 'Mussel',
              'Cockle', 'Barrow', 'Hook', 'Anchor', 'Ladle', 'Peg', 'Slop']
SUMP_TAILS = ['Rents', 'Buildings', 'Court House', 'Stair House', 'Corner', 'Lodgings', 'Rooms', 'End', 'Houses', 'Steps',
              'Row House', 'Yard House']


def sump_names(rng, n):
    names = [f'{a} {b}' for a in SUMP_HEADS for b in SUMP_TAILS]
    rng.shuffle(names)
    return names[:n]


def sump_interior(fw, fh):
    """A Sump floor: small rooms, a little roomier than the block (doc 67, Dense city: smaller rooms in the Sump)."""
    return int(np.clip(round(fw * 1.2), 8, 14)), int(np.clip(round(fh * 1.2), 7, 12))


def rotate_rows(rows, turns):
    for _ in range(turns % 4):
        rows = [list(r) for r in zip(*rows[::-1])]
    return rows


def label(mask):
    """4-connected components of a mask: (an int map, -1 elsewhere; {label: (x0, x1, y0, y1, size)})."""
    labels = np.full(mask.shape, -1, dtype=np.int32)
    info = {}
    ys, xs = np.nonzero(mask)
    n = 0
    Hh, Ww = mask.shape
    for y0, x0 in zip(ys, xs):
        if labels[y0, x0] >= 0:
            continue
        todo = deque([(x0, y0)])
        labels[y0, x0] = n
        bx0 = bx1 = x0
        by0 = by1 = y0
        size = 0
        while todo:
            x, y = todo.popleft()
            size += 1
            bx0, bx1, by0, by1 = min(bx0, x), max(bx1, x), min(by0, y), max(by1, y)
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < Ww and 0 <= ny < Hh and mask[ny, nx] and labels[ny, nx] < 0:
                    labels[ny, nx] = n
                    todo.append((nx, ny))
        info[n] = (int(bx0), int(bx1) + 1, int(by0), int(by1) + 1, size)
        n += 1
    return labels, info


def fit_rect(comp, w, h):
    """The first (x, y, w, h) where a w x h rectangle lies wholly inside the mask, or None."""
    ys, xs = np.nonzero(comp)
    if not len(xs):
        return None
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    sub = comp[y0:y1, x0:x1].astype(np.int32)
    if sub.shape[0] < h or sub.shape[1] < w:
        return None
    s = np.zeros((sub.shape[0] + 1, sub.shape[1] + 1), dtype=np.int32)
    s[1:, 1:] = sub.cumsum(0).cumsum(1)
    tot = s[h:, w:] - s[:-h, w:] - s[h:, :-w] + s[:-h, :-w]
    hit = np.argwhere(tot == w * h)
    if not len(hit):
        return None
    ry, rx = hit[len(hit) // 2]
    return int(x0 + rx), int(y0 + ry), w, h


# ---------------------------------------------------------------------------------------------------------------
# Making the city and the plan
# ---------------------------------------------------------------------------------------------------------------
BASELINE = ART / 'people-before-city.json'
BEFORE = ART / 'preview' / 'before-dense' / 'density.json'      # (the city before doc 67's Dense city, measured)
PEOPLE_KEYS = ('role', 'workLabel', 'home', 'work', 'evening', 'route')


def has_city(project):
    return any(c['id'].startswith('w_ridgemere_') for c in project['cells']) or \
        any(r['id'].startswith('r2_') for r in project['rooms'])


def baseline_people(project):
    """Each NPC's role, label, places and route as they were before the city was first written (kept in
    artifacts/ridgemere2/people-before-city.json from that first write), so that writing again makes the same plan."""
    if BASELINE.exists():
        return json.loads(BASELINE.read_text(encoding='utf-8'))
    if has_city(project):
        raise SystemExit(f'The world already has the city but {BASELINE} is missing: the plan needs the NPCs as they '
                         'were before it was written.')
    return {x['id']: {k: x[k] for k in PEOPLE_KEYS} for x in project['people']}


def strip_city(project):
    """The world without a city written before: its cells, its r2_ interiors and every link but the old stairs (all
    made again, with the same ids)."""
    p = json.loads(json.dumps(project))
    p['rooms'] = [r for r in p['rooms'] if not r['id'].startswith('r2_')]
    ids = {r['id'] for r in p['rooms']}
    p['links'] = [l for l in p['links'] if l['kind'] == 'stairs' and l['a']['cell'] in ids and l['b']['cell'] in ids]
    p['cells'] = []
    for r in p['routes']:
        r['posts'] = []
    return p


def refit_shop(room, trade):
    """A reused shop's ground floor furnished for another trade, in place: the wall of goods opposite its street door
    (buildings.shop's goods row) and the fixtures two rows in from the door are made the new trade's; walls, the door,
    the stairs, the counter and the floor are untouched. Doing it twice changes nothing."""
    from .buildings import STAND_IN, TRADES
    g = [list(row) for row in room['terrain']]
    h, w = len(g), len(g[0])
    door = next(((x, y) for y in range(h) for x in range(w) if g[y][x] == '+' and (x in (0, w - 1) or y in (0, h - 1))),
                None)
    if door is None:
        return False
    dx, dy = door
    # The shop as buildings.shop lays it out (door on the bottom edge): canonical (cx, cy) -> the room's (x, y).
    if dy == h - 1:
        cw, ch_, at = w, h, lambda cx, cy: (cx, cy)
    elif dy == 0:
        cw, ch_, at = w, h, lambda cx, cy: (w - 1 - cx, h - 1 - cy)
    elif dx == 0:
        cw, ch_, at = h, w, lambda cx, cy: (w - 1 - cy, cx)
    else:
        cw, ch_, at = h, w, lambda cx, cy: (cy, cx)
    floor = Counter(c for row in g[1:-1] for c in row[1:-1]).most_common(1)[0][0]
    fixed = set('#HM]j+^=' + floor)
    t = TRADES[trade]
    goods = t['goods']
    for cx in range(1, cw - 1):
        x, y = at(cx, 1)
        if g[y][x] not in fixed:
            c = goods[cx % len(goods)]
            g[y][x] = STAND_IN['oven'] if c == 'v' and t.get('oven_goods') else c
    spots = [(2, ch_ - 3), (3, ch_ - 3), (cw - 3, ch_ - 3), (cw - 4, ch_ - 3)]
    for cx, cy in spots:
        x, y = at(cx, cy)
        if g[y][x] not in fixed:
            g[y][x] = floor
    for fx, fy, c, _ in t.get('fixtures', ()):
        x, y = at(fx if fx >= 0 else cw + fx, fy if fy >= 0 else ch_ + fy)
        if g[y][x] == floor and (x, y) != (dx, dy):
            g[y][x] = c
    room['terrain'] = [''.join(row) for row in g]
    return True


def refit_rooms(project):
    """REFIT: each reused shop given over to another trade, its name (and its flat's), description and furniture."""
    rooms = {r['id']: r for r in project['rooms']}
    for rid, (trade, name, text) in REFIT.items():
        r = rooms.get(rid)
        if r is None:
            continue
        refit_shop(r, trade)
        r['description'] = text
        if name:
            r['name'] = name
            flat = rooms.get(f'{rid}_flat')
            if flat is not None:
                flat['name'] = f'{name}, the flat above'


def make_city(project, seed=SEED, report=print):
    """The city laid out over a copy of the project's rooms: (project as the plan reads it, world, city)."""
    from . import ridgemere2_people as people
    baseline = baseline_people(project)
    project = strip_city(project)
    for r in project['rooms']:
        if r['id'] in ROOM_RENAMES:
            r['name'], r['description'] = ROOM_RENAMES[r['id']]
    refit_rooms(project)
    for x in project['people']:
        x.update(json.loads(json.dumps(baseline[x['id']])))
    old = placements()
    world = MiniRealm(seed)
    world.lay_land()
    world.reserved_ids = {r['id'] for r in project['rooms']} | {l['id'] for l in project['links']}
    site = CitySite(world.c, world.reserved_ids)
    site.cells = [(cell_id(cx, cy), cx * CELL, cy * CELL, CELL, CELL) for cy in range(H // CELL) for cx in range(W // CELL)]
    site.prefix = 'r2_'
    city = RidgemereCity(world, site, project, old)
    placed = people.placed_ids(project)
    houses = people.reused_houses(project, city.structures, placed, set(REUSE))
    t0 = time.time()
    city.build(houses)
    report(f'ridgemere2: laid out in {time.time() - t0:.1f} s: {len(city.records)} structures, {len(site.rooms)} new '
           f'interiors, {len(site.links)} new links')
    return project, world, city


def claims_of_cell(cx, cy):
    out = set()
    x0, y0 = cx * CELL, cy * CELL
    if x0 < 472 and x0 + CELL > 168 and y0 < 456 and y0 + CELL > 150:
        out |= {'ridgemere_watch', 'council_of_houses'}
    out |= {(1, 0): {'house_greatly'}, (1, 2): {'house_mossgrave'}, (2, 1): {'house_fell'}, (0, 0): {'house_grayrock'},
            (0, 1): {'house_grayrock'}, (0, 2): {'house_grayrock'}}.get((cx, cy), set())
    return sorted(out)


def default_claims(district, key):
    if district.startswith('estate_'):
        return ['house_' + district.split('_')[1]]
    if district == 'greatly':
        return ['house_greatly']
    if district in ('keep',):
        return ['council_of_houses', 'ridgemere_watch']
    return ['council_of_houses']


def compose(project, world, city, plan, labels, baseline, place_names=None):
    """The world as a project: the 12 cells, every interior (new ones added), the links, the spawn, the routes' posts,
    and the people's places (doc 67, section 8 A, the write's step 2)."""
    p = json.loads(json.dumps(project))
    p['cells'] = world.cells(CELL_NAMES)
    for c in p['cells']:
        cx, cy = c['x'] // CELL, c['y'] // CELL
        c['territory'] = {'region': REGION, 'claims': claims_of_cell(cx, cy), 'chapter': ''}
    rooms = {r['id']: r for r in city.project['rooms']}       # (the reused ones carry their new region and spot)
    p['rooms'] = [rooms[r['id']] for r in p['rooms']]
    spec_claims = {}
    for rec in city.records:
        for room in rec['rooms']:
            spec_claims[room['id']] = rec
    for r in city.site.rooms:
        rec = spec_claims.get(r['id'])
        claims = (rec and rec.get('claims')) or default_claims(rec['area'] if rec else '', r['id'])
        r['territory'] = {'region': REGION, 'claims': sorted(set(claims)), 'chapter': ''}
        p['rooms'].append(r)
    p['links'] = [l for l in p['links'] if l['kind'] == 'stairs'] + city.site.links
    sx, sy = SPAWN
    p['spawn'] = {'cell': cell_id(sx // CELL, sy // CELL), 'x': sx % CELL, 'y': sy % CELL}
    # The routes: posts on every route in use; the two new ones.
    have = {r['id'] for r in p['routes']}
    names = {'r2_sump_patrol': 'Sump patrol', 'r2_palace_round': 'The palace round'}
    for rid in names:
        if rid not in have:
            p['routes'].append({'id': rid, 'name': names[rid], 'posts': []})
    pl = __import__('worldgen.ridgemere2_people', fromlist=['Placement'])
    snap = pl.Placement(p, city)
    snap.taken = set()
    for r in p['routes']:
        pts = city.route_points.get(r['id'])
        if pts:
            r['posts'] = [x for x in (snap.outdoor(*pt, share=True) for pt in pts) if x]
    # The people.
    city.text_fixes, city.text_fixed = Counter(), []
    for person in p['people']:
        e = plan.get(person['id'])
        if not e or not e.get('placed'):
            person['home'] = person['work'] = person['evening'] = None
            continue
        before = person['workLabel'] if person['home'] else None       # (the label as the world has it now)
        person['home'] = e['home']
        person['work'] = e['work']
        person['evening'] = e['evening']
        person['role'] = e['role']
        person['route'] = e.get('route') or ('' if e['role'] == 'merchant' else baseline[person['id']]['route']
                                             if e.get('labelKept') else '')
        if person['role'] == 'merchant':
            person['route'] = ''
        person['workLabel'] = labels[person['id']]
        if e.get('hours'):                                  # (A post the jobs by class moved: its hours with it.)
            person['hours'] = dict(e['hours'])
        person.update(pl.ROYAL_TEXT.get(person['id'], {}))
        person.update(pl.MINOR_TEXT.get(person['id'], {}) if e.get('record') == pl.MINOR_POSTS.get(person['id'], ('',))[0]
                      else {})
        # Their description, greeting and backstory to fit the post (doc 67, Progress, "Food rebalance").
        text, why = pl.person_text(person, e, person['workLabel'], before, place_names or {})
        if text:
            person.update(text)
            city.text_fixes[why] += 1
            city.text_fixed.append((person['id'], why))
    # The Market Square keepers first: the game takes the first merchant of the market's cell as its middle.
    first = [pp for pp in p['people'] if plan.get(pp['id'], {}).get('record') == 'market_square'
             and plan[pp['id']].get('post') == 'stallkeeper']
    rest = [pp for pp in p['people'] if pp not in first]
    p['people'] = first + rest
    return p


# ---------------------------------------------------------------------------------------------------------------
# The graph walk: every link end, home, work spot and gate reached from the spawn (doc 67, 8 A, the write's step 3)
# ---------------------------------------------------------------------------------------------------------------
def area_graph_reachable(project, spawn=None, placed_rooms=None):
    """Flood-fills each cell and interior by the game's step rules (a whole canvas for the cells, seams crossed only
    where both tiles are open and within 0.55), crosses every door, stairs and passage at its ends, and reports what
    the spawn reaches. Returns {'reached': set of (area, x, y) components' keys, 'missing': [...]} as a dict of lists."""
    cells = project['cells']
    rooms = {r['id']: r for r in project['rooms']}
    spawn = spawn or project['spawn']
    # The cells as one canvas.
    x0 = min(c['x'] for c in cells)
    y0 = min(c['y'] for c in cells)
    Ww = max(c['x'] + c['width'] for c in cells) - x0
    Hh = max(c['y'] + c['height'] for c in cells) - y0
    codes = np.full((Hh, Ww), ord('W'), dtype=np.uint8)
    heights = np.zeros((Hh, Ww), dtype=np.float32)
    cell_at = {}
    for c in cells:
        cx, cy = c['x'] - x0, c['y'] - y0
        for y, row in enumerate(c['terrain']):
            codes[cy + y, cx:cx + len(row)] = np.frombuffer(row.encode('ascii'), dtype=np.uint8)
        heights[cy:cy + c['height'], cx:cx + c['width']] = DEFAULT_HEIGHT[codes[cy:cy + c['height'], cx:cx + c['width']]]
        for key, v in c['heights'].items():
            x, y = map(int, key.split(','))
            heights[cy + y, cx + x] = v
        cell_at[c['id']] = (cx, cy)
    out_labels = _components(codes, heights, seams=CELL)
    labels = {'@cells': out_labels}
    for rid, r in rooms.items():
        rc = np.frombuffer(''.join(r['terrain']).encode('ascii'), dtype=np.uint8).reshape(r['height'], r['width'])
        rh = DEFAULT_HEIGHT[rc].copy()
        for key, v in r.get('heights', {}).items():
            x, y = map(int, key.split(','))
            rh[y, x] = v
        labels[rid] = _components(rc, rh)

    def node(area, x, y):
        if area in cell_at:
            cx, cy = cell_at[area]
            v = int(out_labels[cy + y, cx + x])
            return ('@cells', v) if v >= 0 else None
        lab = labels.get(area)
        if lab is None:
            return None
        v = int(lab[y, x])
        return (area, v) if v >= 0 else None

    def arrival(area, x, y):
        """The component a link end leads into: the end's own, else a neighbour's (the arrival tile)."""
        got = node(area, x, y)
        if got:
            return got
        for dx, dy in ((0, -1), (1, 0), (0, 1), (-1, 0)):
            try:
                got = node(area, x + dx, y + dy)
            except IndexError:
                got = None
            if got:
                return got
        return None
    graph = defaultdict(set)
    bad_links = []
    for link in project['links']:
        a = arrival(link['a']['cell'], link['a']['x'], link['a']['y'])
        b = arrival(link['b']['cell'], link['b']['x'], link['b']['y'])
        if a is None or b is None:
            bad_links.append(link['id'])
            continue
        graph[a].add(b)
        graph[b].add(a)
    start = arrival(spawn['cell'], spawn['x'], spawn['y'])
    seen = {start}
    todo = [start]
    while todo:
        n = todo.pop()
        for m in graph[n]:
            if m not in seen:
                seen.add(m)
                todo.append(m)
    missing = defaultdict(list)
    for link in project['links']:
        if placed_rooms is not None and not all(link[e]['cell'] in placed_rooms or link[e]['cell'] in cell_at
                                                for e in ('a', 'b')):
            continue                                   # (The stock interiors, unplaced, keep their stairs.)
        for end in ('a', 'b'):
            e = link[end]
            if arrival(e['cell'], e['x'], e['y']) not in seen:
                missing['links'].append(f'{link["id"]}.{end}')
                break
    placed_areas = set()
    for person in project['people']:
        for key in ('home', 'work', 'evening'):
            spot = person.get(key)
            if spot and arrival(spot['cell'], spot['x'], spot['y']) not in seen:
                missing[key].append(f'{person["id"]} ({spot["cell"]} {spot["x"]},{spot["y"]})')
            if spot:
                placed_areas.add(spot['cell'])
    for r in project['routes']:
        for i, post in enumerate(r['posts']):
            if arrival(post['cell'], post['x'], post['y']) not in seen:
                missing['posts'].append(f'{r["id"]}#{i}')
    return {'start': start, 'reached': len(seen), 'nodes': len(graph), 'bad_links': bad_links, 'missing': dict(missing),
            'seen': seen, 'node': arrival, 'labels': labels, 'cell_at': cell_at, 'origin': (x0, y0)}


def _components(codes, heights, seams=None):
    """Components of walkable tiles by the game's step rule (seams, if given, crossed only within 0.55)."""
    Hh, Ww = codes.shape
    h = half(heights)
    solid = SOLID[codes]
    ramp = RAMP[codes]

    def pair(a_h, b_h, a_s, b_s, a_r, b_r):
        rise = np.abs(a_h - b_h)
        return ~a_s & ~b_s & ((rise <= .5) | ((rise <= 1) & (a_r | b_r)))
    east = pair(h[:, :-1], h[:, 1:], solid[:, :-1], solid[:, 1:], ramp[:, :-1], ramp[:, 1:])
    south = pair(h[:-1, :], h[1:, :], solid[:-1, :], solid[1:, :], ramp[:-1, :], ramp[1:, :])
    if seams:
        for x in range(seams - 1, Ww - 1, seams):
            east[:, x] = ~solid[:, x] & ~solid[:, x + 1] & (np.abs(h[:, x] - h[:, x + 1]) <= .55)
        for y in range(seams - 1, Hh - 1, seams):
            south[y, :] = ~solid[y, :] & ~solid[y + 1, :] & (np.abs(h[y, :] - h[y + 1, :]) <= .55)
    labels = np.full((Hh, Ww), -1, dtype=np.int32)
    n = 0
    flat_e = east
    flat_s = south
    for y0, x0 in zip(*np.nonzero(~solid)):
        if labels[y0, x0] >= 0:
            continue
        labels[y0, x0] = n
        todo = [(x0, y0)]
        while todo:
            x, y = todo.pop()
            if x + 1 < Ww and flat_e[y, x] and labels[y, x + 1] < 0:
                labels[y, x + 1] = n
                todo.append((x + 1, y))
            if x > 0 and flat_e[y, x - 1] and labels[y, x - 1] < 0:
                labels[y, x - 1] = n
                todo.append((x - 1, y))
            if y + 1 < Hh and flat_s[y, x] and labels[y + 1, x] < 0:
                labels[y + 1, x] = n
                todo.append((x, y + 1))
            if y > 0 and flat_s[y - 1, x] and labels[y - 1, x] < 0:
                labels[y - 1, x] = n
                todo.append((x, y - 1))
        n += 1
    return labels


# ---------------------------------------------------------------------------------------------------------------
# The checks (doc 67, section 8 A, Acceptance) and the report
# ---------------------------------------------------------------------------------------------------------------
def checks(p, world, city, plan, labels, walk, capacity=None, classes=None):
    """Every acceptance check of section 8 A: [(requirement, name, ok, detail)]."""
    from . import ridgemere2_people as pp
    out = []

    def check(req, name, ok, detail=''):
        out.append({'req': req, 'check': name, 'ok': bool(ok), 'detail': detail})
    c = world.c
    codes, heights = c.codes, c.heights
    people = {x['id']: x for x in p['people']}
    placed = [x for x in p['people'] if x['home']]
    rooms = {r['id']: r for r in p['rooms']}
    new_rooms = {r['id'] for r in city.site.rooms}
    # -- the map --
    cells = p['cells']
    n_cells = (W // CELL) * (H // CELL)
    check('R2', f'{n_cells} cells, all rain, region ridgemere', len(cells) == n_cells and all(x['weather'] == 'rain' and
          x['territory']['region'] == REGION for x in cells), f'{len(cells)} cells')
    placed_rooms = {room['id'] for rec in city.records for room in rec['rooms']}
    check('R2', 'every placed interior has region ridgemere',
          all(rooms[r]['territory']['region'] == REGION for r in placed_rooms), f'{len(placed_rooms)} placed interiors')
    import map_editor
    check('R1', 'no cell overlaps another', not map_editor.overlapping_cells(cells))
    bay = np.zeros((H, W), dtype=bool)
    for y in range(170, 610):
        bay[y, :int(shore_x(y))] = True
    built_over = np.isin(codes, [ord(ch) for ch in '8iI:']) | city.mole
    check('R4', 'water lies west of x = 168 between the headlands (the piers, the Mole and the Bay Light aside)',
          bool((world.water | built_over)[bay].all()))
    x0_, x1_, y0_, y1_ = INSIDE
    sea = codes[y0_ - WALL:y1_ + WALL, 168:172]
    check('R5', 'the sea wall runs along the quay (x 168-172)', bool(np.isin(sea, [ord('#'), ord('G'), ord(':')]).all()))
    check('R5', 'the wharf, its 8 piers and arches, the cranes and the slipway lie within the wall\'s span',
          all(y0_ - WALL <= y and y + 6 <= y1_ + WALL for y in PIERS) and all(codes[y + 2, 169] == ord('G') for y in PIERS)
          and bool(city.slip.any()) and all(y0_ <= y <= y1_ for y in CRANES))
    wharf_x, sump_x = QUAY_X - 1, QUAY_X
    sump_rows = [y for y in range(y0_, y1_) if city.district[y, sump_x] == DISTRICT_IDS.index('sump')]
    open_pairs = sum(1 for y in sump_rows if not SOLID[codes[y, wharf_x]] and not SOLID[codes[y, sump_x]]
                     and abs(heights[y, wharf_x] - heights[y, sump_x]) <= .5)
    walled = sum(1 for y in sump_rows if codes[y, sump_x] in (ord('#'), ord('H')) and not city.built[y, sump_x])
    check('R6', 'the Sump meets the wharf with no wall between', walled == 0 and open_pairs > 0,
          f'{open_pairs} open crossings, {walled} wall tiles on the line')
    check('R6', 'the Sump holds at least 50% of the wharf frontage', len(sump_rows) / (y1_ - y0_) >= .5,
          f'{len(sump_rows) / (y1_ - y0_):.0%}')
    seen = walk['seen']
    node = walk['node']

    def reached(x, y):
        return node(cell_id(int(x) // CELL, int(y) // CELL), int(x) % CELL, int(y) % CELL) in seen
    gate_ok = {k: reached(*city.gate_inside[k]) and bool(city.gate_masks[k].any()) for k in GATES}
    arch_ok = {y: reached(176, y + 3) and codes[y + 2, 170] == ord('G') for y in (CUSTOMS_PIER, STAIRS_PIER)}
    check('R5', 'the 5 gates and the 2 guarded pier arches are cut and reached', all(gate_ok.values()) and all(arch_ok.values()),
          f'gates {gate_ok}, arches {arch_ok}')
    inside = box(*INSIDE)
    check('R10', 'the districts partition the inside of the wall', bool((city.district[inside] > 0).all()))
    touches = lambda a, b: bool((grow(a, 1) & b).any())
    S, A = city.D('sump'), city.D('admin')
    check('R10', 'the Sump is bordered on the east by the middle class', touches(S, city.D('commons')) and
          np.nonzero(city.D('commons') & grow(S, 1))[1].mean() > np.nonzero(S)[1].mean())
    south_edge = S & ~np.roll(S, -1, 0) & inside
    below = np.roll(city.district, -1, 0)[south_edge]
    below = below[below != DISTRICT_IDS.index('sump')]
    share = float(np.mean(below == DISTRICT_IDS.index('admin'))) if below.size else 0
    check('R11', 'the Sump is bordered on the south by the administrative sector (most of its south edge)',
          share >= .5, f'{share:.0%} of the Sump\'s south edge meets admin')
    check('R17', 'the administrative sector borders the Sump (north) and the upper class (east)',
          touches(A, S) and touches(A, city.D('rise')))
    area = lambda n: int(city.D(n).sum())
    middle = area('northgate') + area('commons')
    check('R15', "the Rise's area is at most half the middle class's", area('rise') <= .5 * middle,
          f'rise {area("rise")}, middle class {middle} ({area("rise") / middle:.2f})')
    ab = city.D('abbey')
    check('R20', 'the abbey close is in the north-west corner, touching the Sump and the middle class',
          touches(ab, S) and touches(ab, city.D('northgate')) and
          np.nonzero(ab)[1].min() <= QUAY_X and np.nonzero(ab)[0].min() <= y0_)
    kp = city.D('keep')
    kx0, kx1, ky0, ky1 = KEEP
    court = kp & ~city.keep_ring & ~city.built
    check('R16', 'the Keep is at one height on the Commons/Rise border, with four walls, a courtyard and one gate',
          len(set(np.unique(heights[court]))) == 1 and touches(kp, city.D('commons')) and touches(kp, city.D('rise')) and
          int((codes[kp] == ord('G')).sum()) > 0 and
          int((codes[kp] == ord('G')).sum()) == int((codes[ky0:ky0 + 3, kx0:kx1] == ord('G')).sum()))
    gx0, gx1, gy0, gy1 = GREATLY
    between = ~SOLID[codes[y0_ - WALL:y0_, gx0:gx1]]
    check('R24', 'the Greatly compound sits against the city wall, no public tile between',
          not between.any() and bool(city.D('greatly')[y0_, gx0 + 5:gx1 - 5].all()))
    fx, fy = city.spots['greatly_fountain']
    forge = city.named['r2_greatly_forge']['block']
    house = city.named['rm_greatly_house']['block']
    check('R23', 'the Greatly fountain is in the south yard, the forge and workshop in the north yard',
          house['y'] + house['h'] <= fy <= gy1 and forge['y'] + forge['h'] <= house['y'] and
          bool((codes[gy0:gy1, gx0:gx1] == ord('F')).any()))
    # -- no walls between the districts, and slopes rather than tiers --
    m_ = district_metrics(city, walk)
    check('', 'no wall tile on a district border, other than the city wall and the walled pieces (the Keep, the abbey, '
          'the Greatly compound)', m_['wallsOnBorders'] == 0, f'{m_["wallsOnBorders"]} tiles')
    check('R2', 'no step of more than half a tile between neighbouring open tiles inside the walls (a stair at most 1)',
          not m_['badSteps'], f'{len(m_["badSteps"])}: {m_["badSteps"][:6]}')
    check('R2', 'the ground climbs from the bay: the Rise and the Greatly compound stand at least 1.5 above the Sump',
          m_['meanHeight']['rise'] - m_['meanHeight']['sump'] >= 1.5 and
          m_['meanHeight']['greatly'] - m_['meanHeight']['sump'] >= 1.5, f'{m_["meanHeight"]}')
    check('', 'height steps do not follow the borders between the quarters (a step at most twice as likely on a border '
          'as off it)', m_['stepOnBorder'] <= max(.03, 2 * m_['stepOffBorder']),
          f'{m_["stepOnBorder"]:.1%} of border pairs step, {m_["stepOffBorder"]:.1%} elsewhere')
    irregular = {n: m_['districts'][n]['fill'] for n in ('sump', 'commons', 'northgate', 'admin', 'cinders', 'rise')}
    check('', 'the districts are not rectangles (each fills under 90% of its bounding box)',
          all(v < .9 for v in irregular.values()), f'{irregular}')
    need = {'commons/sump': 8, 'northgate/sump': 2, 'admin/sump': 3, 'commons/rise': 3, 'admin/commons': 2,
            'commons/northgate': 4, 'admin/cinders': 2, 'cinders/sump': 1, 'admin/rise': 2}
    short = {k: m_['crossings'].get(k, 0) for k, v in need.items()
             if m_['crossings'].get(k, 0) < v and m_['openBorder'].get(k, 0) < 8 * v}
    check('', 'many street crossings between neighbouring districts (separate openings, or as many 8-tile lengths of '
          'open border)', not short,
          f'short: {short}; all: {m_["crossings"]}')
    old_px = 225456
    smaller = 1 - int(inside.sum()) / old_px
    check('', 'the city inside the walls is 35-65% smaller than before (225,456 tiles)', .35 <= smaller <= .65,
          f'{int(inside.sum())} tiles, {smaller:.0%} smaller')
    wanted = set(REUSE) | {g for v in city.houses.values() for g in v} | {
        'r2_customs_house', 'r2_grayrock_shipwrights', 'r2_new_store', 'r2_cathedral', 'r2_chapter_house',
        'r2_abbey_quarters', 'r2_abbey_refectory', 'r2_abbey_scriptorium', 'r2_abbey_brewhouse', 'r2_abbey_school',
        'r2_abbey_infirmary', 'r2_greatly_forge', 'r2_the_keep', 'r2_kings_residence', 'r2_palace_antechamber',
        'r2_administrative_wing', 'r2_market_hall', 'r2_records_office', 'r2_barber', 'r2_fell_wine_vaults',
        'r2_cheesemonger', 'r2_bathhouse', 'r2_forge_row', 'r2_cinder_wheelwrights', 'r2_the_washhouse',
        'r2_penny_loaf', 'r2_cinder_butchers', 'r2_carters_yard', 'r2_lamp_store', 'r2_the_knucklebones',
        'r2_fell_press_house', 'r2_fell_distillery', 'r2_fell_preserving_house', 'r2_fell_cellars', 'r2_mossgrave_cottage', 'r2_fell_cottage', 'r2_grayrock_cottage',
        'r2_bay_light', 'r2_clock_tower', 'r2_sunken_street', 'r2_tallow_arcade'}
    lost = sorted(k for k in wanted if k not in city.named)
    check('R30', 'every building the plan keeps or adds is placed (the same businesses, houses and landmarks)',
          not lost and not city.unplaced, ', '.join(lost + city.unplaced))
    check('', '6-10 landmarks, each named and described', 6 <= len(city.landmarks) <= 12 and
          all(lm['name'] and lm['what'] for lm in city.landmarks), ', '.join(lm['name'] for lm in city.landmarks))
    est_ok = []
    for house, (_, (x, y, ew, eh)) in ESTATES.items():
        one_cell = cell_of(x, y) == cell_of(x + ew - 1, y + eh - 1)
        outside = not (box(x, x + ew, y, y + eh) & box(168, 472, 150, 456)).any()
        est_ok.append(one_cell and outside)
    check('R26', 'the three estates are outside the wall, each within one cell', all(est_ok))
    check('R34', 'no field, orchard, vineyard or pen glyphs (crop fields) anywhere', not (codes == ord('4')).any())
    check('R34', 'resources empty and no game spawns', not p.get('herbPatch') and not p.get('resources'))
    # -- the Sump --
    m = city.sump_metrics(walk)
    for name, ok, detail in m['checks']:
        check('R7-R9', name, ok, detail)
    # -- buildings --
    rec = city.named['r2_records_office']
    zs = sorted(r['z'] for r in rec['rooms'])
    rl = [l for l in p['links'] if l['kind'] == 'stairs' and l['a']['cell'].startswith(rec['id'])]
    tiles = {(l['a']['x'], l['a']['y']) for l in rl}
    check('R18', 'the Records Office has three floors (z 0, 1, 2), two stair pairs on distinct tiles',
          zs == [0, 1, 2] and len(rl) == 2 and len(tiles) == 2)
    blocks = {k: city.named[k]['block'] for k in ('r2_kings_residence', 'r2_palace_antechamber', 'r2_administrative_wing')}
    res, ante, wing = blocks['r2_kings_residence'], blocks['r2_palace_antechamber'], blocks['r2_administrative_wing']
    passages = [l for l in p['links'] if l['kind'] == 'passage']
    pal_ids = {city.named[k]['rooms'][0]['id'] for k in blocks}
    pal_pass = [l for l in passages if l['a']['cell'] in pal_ids and l['b']['cell'] in pal_ids]
    check('R19', 'the palace\'s three parts touch, joined by passages, the Residence west and the Wing east',
          res['x'] + res['w'] == ante['x'] and ante['x'] + ante['w'] == wing['x'] and len(pal_pass) == 2 and
          ante['y'] > SQUARES['records'][0][1])
    homes_of = Counter()
    room_struct = {room['id']: r['id'] for r in city.records for room in r['rooms']}
    sleepers = Counter(x['home']['cell'] for x in placed)
    for room in sleepers:
        homes_of[room_struct.get(room, room)] += 1
    check('R32', 'no structure has more than 3 homes', max(homes_of.values()) <= 3,
          f'most {max(homes_of.values())}: {homes_of.most_common(1)}')
    design = Counter(r.get('homes', 0) for r in city.records)
    check('R32', 'no structure is built with more than 3 homes', max(design) <= 3, f'{dict(sorted(design.items()))}')
    sump_built = [r for r in city.records if r.get('sump_home')]
    sump3 = sum(1 for r in sump_built if r.get('homes') == 3)
    sump_used = Counter(homes_of[r['id']] for r in sump_built if homes_of[r['id']])
    check('R32', 'the Sump\'s houses are mostly three homes, and most lived-in ones hold three households',
          sump3 >= .7 * len(sump_built) and sump_used[3] >= .6 * sum(sump_used.values()),
          f'{sump3} of {len(sump_built)} built with 3 homes; lived in: {dict(sorted(sump_used.items()))}')
    bad_beds = []
    for room, n in sleepers.items():
        beds = sum(row.count('b') + row.count('z') for row in rooms[room]['terrain'])
        if beds < max(2, math.ceil(n / 2)) or n > 4 * beds:
            bad_beds.append(f'{room}: {n} people, {beds} beds')
    check('R32', 'every home has at least ceil(people / 2) beds, and at least 2', not bad_beds, '; '.join(bad_beds[:8]))
    check('', 'links number 2,048 or fewer', len(p['links']) <= 2048, f'{len(p["links"])} links')
    # -- people --
    n = len(placed)
    check('R31', '480-580 placed, every one with a home', 480 <= n <= 580 and all(x['home'] for x in placed), f'{n} placed')
    jobless = [x['id'] for x in placed if 16 <= x['age'] <= 59 and not x['work'] and x['role'] != 'guard']
    jobless = [j for j in jobless if not (people[j]['route'] and people[j]['role'] == 'guard')]
    check('R33', 'every placed NPC aged 16-59 has work', not jobless, ', '.join(jobless[:10]))
    bad = []
    for x in placed:
        e = plan[x['id']]
        intended = e.get('business') if x['role'] == 'merchant' else None
        clergy = e.get('post') == 'clergy' or (e.get('record') in ('r2_cathedral',) and e.get('post') != 'pool') or \
            pp.is_clergy(x['workLabel']) and e.get('record') in CLERGY_PLACES
        head = pp.house_head(x['workLabel']) and e.get('labelKept')
        food = e.get('post') == 'food' or (x['role'] == 'merchant' and intended == 'inn')
        food = food or (e.get('record') == 'r2_kings_residence' and e.get('post') == 'food')
        probs = pp.label_problems(x['workLabel'], intended, clergy=clergy, head=head, food=food, at=e.get('at'))
        if x['role'] == 'merchant' and intended is None:
            got = pp.business_for(x['workLabel'])
            if got is None or got in pp.NO_TILL:
                probs.append(f'a merchant without a till business ({got})')
        if probs:
            bad.append(f'{x["id"]} "{x["workLabel"]}": {"; ".join(probs)}')
    check('R33', 'every label is at most 40 characters and breaks none of the label rules', not bad,
          f'{len(bad)}: ' + ' | '.join(bad[:12]))
    par = pp.analysis()['par']
    hh = pp.households(par)
    split = [r for r, members in hh.items() if 0 < len({m for m in members if people[m]['home']}) < len(members)]
    check('R31', 'no family is split', not split, ', '.join(split[:8]))
    keyed = defaultdict(int)
    for x in placed:
        if x['role'] == 'merchant' and not x['work']:
            keyed['(no work)'] += 2
        elif x['role'] == 'merchant' and not x['work']['cell'].startswith('w_'):
            keyed[room_struct.get(x['work']['cell'], x['work']['cell'])] += 1
    hall_id = city.named['r2_market_hall']['id']
    many = {k: v for k, v in keyed.items() if v > 1 and k != hall_id}
    missing_keeper = []
    for r in city.records:
        posts = pp.POSTS.get(r['key']) or (pp.SHOP_DEFAULT if r.get('kindOf') == 'shop' else [])
        if any(it[0] == 'keeper' for it in posts) and keyed.get(r['id'], 0) == 0:
            missing_keeper.append(r['key'])
    check('R35', 'exactly one merchant per till business; every business that uses the store has its keeper',
          not many and not missing_keeper, f'two or more: {many}; none: {missing_keeper}')
    guards = [x for x in placed if x['role'] == 'guard']
    used_routes = {x['route'] for x in placed if x['route']}
    posts_ok = all(r['posts'] for r in p['routes'] if r['id'] in used_routes)
    check('R33', '30 guards, with posts on every route in use', len(guards) == 30 and posts_ok,
          f'{len(guards)} guards; routes in use {sorted(used_routes)}')
    clergy_bad = [x['id'] for x in placed if pp.is_clergy(x['workLabel']) and plan[x['id']].get('record') not in
                  CLERGY_PLACES]
    check('', 'clergy have clergy words, and nobody else has them', not clergy_bad, ', '.join(clergy_bad[:8]))
    prod = [(x['id'], x['workLabel'], pp.producer_for(x['workLabel'])) for x in placed if pp.producer_for(x['workLabel'])]
    food = [t for t in prod if t[2] in pp.FOOD_PRODUCERS + pp.RAW_PRODUCERS]
    check('R34', 'no placed NPC works a food or raw-resource producer (stables listed)', not food,
          f'stables: {[t[0] for t in prod if t[2] == "stock_breeding"]}; bad: {food[:6]}')
    sq, rag = SQUARES['market'][0], SQUARES['rag'][0]
    check('R13', 'the Rag Market is more than 40 tiles from Market Square', math.dist(sq, rag) > 40, f'{math.dist(sq, rag):.0f}')
    square_m = sum(1 for x in placed if x['role'] == 'merchant' and plan[x['id']].get('record') == 'market_square')
    hall_m = sum(1 for x in placed if x['role'] == 'merchant' and plan[x['id']].get('record') == 'r2_market_hall')
    hall = city.named['r2_market_hall']
    doors = [(hall['door']['x'], hall['door']['y'])] + ([(hall['door2']['x'], hall['door2']['y'])] if 'door2' in hall else [])
    stalls = city.posts['market_square']
    near = max(min(math.dist(d, s) for s in stalls) for d in doors)
    check('R14', 'Market Square has more merchants than the Market Hall; the Hall\'s doors within 40 of its stalls',
          square_m > hall_m and near <= 40, f'square {square_m}, hall {hall_m}, doors {near:.0f} from the stalls')
    ev = Counter(plan[x['id']].get('eveningPlace') for x in placed)
    over = {k: v for k, v in ev.items() if k != 'home' and v > 40}
    check('', 'no evening place takes more than 40', not over, f'{dict(ev)}')
    # -- the food rebalance (doc 67, Progress): the capacity estimate of what the placed residents buy a day --
    if capacity is not None:
        cap = capacity['after']
        f = cap['food']
        rows = ', '.join(f'{b} {k}+{h}/{d:.1f}' for b, _, k, h, d in cap['rows'][:11])
        check('R35', "the food trades' keepers and hands within 25% of the food they sell a day (keeper-days of their "
              'own crafts, tools/city_capacity.py), and no food trade below half of its own', cap['ok'],
              f'{f["staffed"]} staffed for {f["demand"]:.1f} keeper-days ({f["ratio"]:.0%}); below half: '
              f'{f["short"] or "none"}; keepers+hands/demand: {rows}')
        check('', 'the Carters\' Yard has its keeper and drivers', cap['businesses'].get('carters') == 1 and
              sum(1 for x in placed if x['workLabel'] == 'driving for the carters') == pp.CARTERS)
    # -- the jobs by class (doc 67, Progress): each household's earners in work of its quarter's class --
    if classes is not None:
        c_ = classes['after']
        per = ', '.join(f'{k} {v["matched"]}/{v["earners"]}' for k, v in c_['classes'].items())
        check('', f'at least {CLASS_TARGET:.0%} of earners hold a job of their household\'s class (the Watch and the '
              'clergy apart)', c_['rate'] >= CLASS_TARGET, f'{c_["matched"]}/{c_["earners"]} ({c_["rate"]:.1%}): {per}')
        check('', 'every Minor House household has an upper job (a large business, a merchant\'s, high office)',
              not c_['minorWithoutUpper'], ', '.join(c_['minorWithoutUpper']))
        check('', 'nobody from the Sump, the Cinders or the wharf keeps a larger business', not c_['largeLower'],
              '; '.join(c_['largeLower'][:6]))
        check('', "the Rag Market's stalls and the Soup Kitchen are kept by middle-class households",
              not c_.get('ragNotMiddle'), '; '.join(c_.get('ragNotMiddle', [])[:7]))
    # -- the graph walk --
    miss = walk['missing']
    check('R1', 'every link end, home, work spot, evening place and route post is reached from the spawn',
          not any(miss.values()) and not walk['bad_links'],
          '; '.join(f'{k}: {len(v)} ({", ".join(v[:4])})' for k, v in miss.items() if v) +
          (f'; links without an arrival: {walk["bad_links"][:6]}' if walk['bad_links'] else ''))
    return out


CLASS_TARGET = .85                       # (the jobs by class: the share of earners in work of their class)
CLERGY_PLACES = {'r2_cathedral', 'r2_abbey_scriptorium', 'r2_abbey_brewhouse', 'r2_abbey_school', 'r2_chapter_house',
                 'rm_the_sump_infirmary', 'rm_the_chapel_of_the_tide', 'rm_the_house_of_mending'}


def sump_metrics(self, walk):
    """The Sump's targets (doc 67, section 2): coverage, open space, narrow passages, dead ends, shapes, doors, the
    through-buildings, passages and courts."""
    c = self.c
    S = self.D('sump')
    land = int(S.sum())
    walkable = S & ~SOLID[c.codes]
    # Public: reached from the spawn (the outdoor component the walk started in).
    lab = walk['labels']['@cells']
    start = walk['start']
    public = walkable & (lab == start[1]) if start and start[0] == '@cells' else walkable
    solid = S & SOLID[c.codes]
    enclosed = walkable & ~public
    coverage = (int(solid.sum()) + int(enclosed.sum())) / land
    # Planned coverage: the buildings and infill against the land before the alleys (counted from the building list).
    planned = (int(self.built[S].sum())) / land
    exempt = box(SQUARES['rag'][0][0] - 13, SQUARES['rag'][0][0] + 14, SQUARES['rag'][0][1] - 9, SQUARES['rag'][0][1] + 10)
    exempt |= self.c.line_mask(self.gut_line, 3) | self.c.line_mask(self.tarry_line, 3) | grow(self.court_mask, 1)
    exempt |= grow(self.cistern_court, 1) | grow(self.hundred_steps, 1) | grow(self.runnel, 2)
    open_ = public & ~exempt
    dist = field.distance_to_mask(~open_ | ~S, 8)
    widest = int(dist[open_].max()) * 2 if open_.any() else 0
    # Narrow: in a run of open tiles two or fewer wide, across one way or the other.
    allpub = ~SOLID[c.codes] & (lab == start[1]) if start and start[0] == '@cells' else ~SOLID[c.codes]
    core3 = allpub.copy()                              # Tiles at the middle of an open 3 x 3 square...
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            core3 &= np.roll(allpub, (dy, dx), (0, 1))
    wide = core3.copy()                               # ...and the square's tiles: open space three wide or more.
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            wide |= np.roll(core3, (dy, dx), (0, 1))
    # (The Sump's named lanes, squares and courts are wide on purpose: the Gut and Tarry Lane, the Rag Market, Cistern
    # Court, the Hundred Steps and the courts. The share is of the rest, its alleys and passages; the share of all its
    # public tiles is reported beside it.)
    named_open = exempt & S
    narrow = public & ~wide & ~named_open
    share = narrow.sum() / max(1, (public & ~named_open).sum())
    share_all = (public & ~wide).sum() / max(1, public.sum())
    recs = [r for r in self.records if r.get('area') == 'sump']
    homes = [r for r in recs if r.get('sump_home')]
    sizes = {(r['block']['w'], r['block']['h']) for r in recs}
    sizes = {tuple(sorted(s)) for s in sizes}
    lshape = sum(1 for r in homes if r.get('lshape'))
    through = sum(1 for r in homes if r.get('through'))
    bad_doors = []
    for r in recs:
        if r.get('kindOf') in ('flats', 'home', 'shop', 'tavern', 'works', 'hall', 'lodging'):
            (dx, dy) = r['door']['x'], r['door']['y']
            ox, oy = r['outside']['x'], r['outside']['y']
            sx, sy = ox - dx, oy - dy
            if not (allpub[oy, ox] and allpub[oy + sy, ox + sx]):
                bad_doors.append(r['id'])
    passages = sum(1 for l in self.site.links if l['kind'] == 'passage' and not l['id'].startswith(
        tuple(self.named[k]['rooms'][0]['id'] for k in ('r2_kings_residence', 'r2_palace_antechamber'))))
    chains = [len(ch) for ch in getattr(self, 'sump_chains', [])]
    dead_ends = 0
    for (ax, ay), (bx, by) in self.sump_stubs:              # A stub still ending in a wall or a building.
        dx, dy = np.sign(bx - ax), np.sign(by - ay)
        ex, ey = int(bx + dx * 1.5), int(by + dy * 1.5)
        if 0 <= ex < W and 0 <= ey < H and not public[ey, ex]:
            dead_ends += 1
    checks = [
        ('built coverage of at least 55% of the Sump', coverage >= .55, f'{coverage:.1%} (buildings and infill blocks alone: {planned:.1%})'),
        ('no open space wider than 12 tiles (bar the Rag Market, the Gut, Tarry Lane and the courts)', widest <= 12,
         f'widest {widest}'),
        ('at least 60% of outdoor walkable tiles (outside its named lanes, squares and courts) in alleys 2 wide or '
         'less', share >= .6, f'{share:.1%} ({share_all:.1%} of all its public tiles)'),
        ('at least 10 dead ends', dead_ends >= 10, f'{dead_ends} alley stubs ending blind (of {len(self.sump_stubs)} laid)'),
        ('at least 12 distinct footprint sizes and 8 L-shapes', len(sizes) >= 12 and lshape >= 8,
         f'{len(sizes)} sizes, {lshape} L-shaped'),
        ('every home and work door on a passage at least 2 wide', not bad_doors, ', '.join(bad_doors[:8])),
        ('at least 14 through-buildings', through >= 14, f'{through}'),
        ('at least 25 passages, in 4-6 chains of 3-5 buildings', passages >= 25 and 4 <= len(chains) <= 6 and
         all(3 <= n <= 5 for n in chains), f'{passages} passages, chains {chains}'),
        ('at least 6 courts', self.infill['courts'] >= 6, f'{self.infill["courts"]}'),
    ]
    return {'coverage': coverage, 'planned': planned, 'widest': widest, 'narrow': float(share), 'dead_ends': dead_ends,
            'sizes': len(sizes), 'lshape': lshape, 'through': through, 'passages': passages, 'chains': chains,
            'courts': self.infill['courts'], 'homes': len(homes), 'infill': dict(self.infill), 'checks': checks}


RidgemereCity.sump_metrics = sump_metrics


CLASS_IDS = [DISTRICT_IDS.index(n) for n in ('sump', 'commons', 'northgate', 'admin', 'cinders', 'palace', 'rise')]


def district_metrics(city, walk, residents=None):
    """Density and borders by district (doc 67, Dense city): land, built coverage (solid tiles and tiles shut in, not
    public), public open tiles, residents and open tiles per resident; the wall tiles on district borders (other than
    the walled pieces' and the city wall), the height steps on and off the borders, the crossings between districts
    (separate openings), how irregular each district is, and every step over half a tile between open tiles."""
    c = city.c
    codes, h = c.codes, c.heights
    lab = walk['labels']['@cells']
    start = walk['start']
    solid = SOLID[codes]
    public = ~solid & (lab == start[1]) if start and start[0] == '@cells' else ~solid
    inside = box(*INSIDE)
    d = city.district
    out = {'districts': {}, 'insideArea': int(inside.sum()),
           'builtInside': round(float((inside & ~public).sum()) / int(inside.sum()), 3)}
    residents = residents or {}
    for name in DISTRICT_IDS[1:-1]:
        D = city.D(name) & inside
        land = int(D.sum())
        if not land:
            continue
        ys, xs = np.nonzero(D)
        bbox = (xs.max() - xs.min() + 1) * (ys.max() - ys.min() + 1)
        open_ = int((D & public).sum())
        n = residents.get(name, 0)
        out['districts'][name] = {'land': land, 'built': round(1 - open_ / land, 3), 'open': open_, 'residents': n,
                                  'openPerResident': round(open_ / n, 1) if n else None, 'fill': round(land / bbox, 3),
                                  'meanHeight': round(float(h[D].mean()), 2)}
    out['meanHeight'] = {k: v['meanHeight'] for k, v in out['districts'].items()}
    border = np.zeros(d.shape, dtype=bool)
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        nb = np.roll(d, (dy, dx), (0, 1))
        border |= (d != nb) & (nb > 0) & (d > 0)
    border &= inside
    wallish = np.isin(codes, [ord(ch) for ch in '#HM]'])
    out['wallsOnBorders'] = int((grow(border, 1) & inside & wallish & ~city.rings & ~city.built & ~city.wall_band).sum())
    # Steps: pairs of neighbouring open tiles inside the walls.
    on = off = on_n = off_n = 0
    bad = []
    cross = defaultdict(lambda: np.zeros(d.shape, dtype=bool))
    walk_ok = ~solid & inside
    for dy, dx in ((0, 1), (1, 0)):
        nb = np.roll(d, (-dy, -dx), (0, 1))
        both = walk_ok & np.roll(walk_ok, (-dy, -dx), (0, 1))
        both[-1, :] = False
        both[:, -1] = False
        rise = np.abs(h - np.roll(h, (-dy, -dx), (0, 1)))
        ramp = RAMP[codes] | np.roll(RAMP[codes], (-dy, -dx), (0, 1))
        step = both & (rise > 0)
        diff = both & (d != nb) & (d > 0) & (nb > 0)
        classes = np.isin(d, CLASS_IDS) & np.isin(nb, CLASS_IDS)
        diff_c = diff & classes
        same_c = both & ~diff & np.isin(d, CLASS_IDS)
        on_n += int(diff_c.sum())
        on += int((diff_c & step).sum())
        off_n += int(same_c.sum())
        off += int((same_c & step).sum())
        too = both & ((rise > 1) | ((rise > .5) & ~ramp))
        too &= ~(np.isin(codes, [ord('G')]) | np.roll(np.isin(codes, [ord('G')]), (-dy, -dx), (0, 1)))
        for y, x in np.argwhere(too)[:20]:
            bad.append((int(x), int(y)))
        ok = diff & public & np.roll(public, (-dy, -dx), (0, 1)) & ((rise <= .5) | ((rise <= 1) & ramp))
        for y, x in zip(*np.nonzero(ok)):
            k = '/'.join(sorted((DISTRICT_IDS[d[y, x]], DISTRICT_IDS[nb[y, x]])))
            cross[k][y, x] = True
    out['stepOnBorder'] = on / max(1, on_n)
    out['stepOffBorder'] = off / max(1, off_n)
    out['badSteps'] = bad
    out['crossings'] = {}
    out['openBorder'] = {}
    for k, m in sorted(cross.items()):
        _, info = label(grow(m, 1))
        out['crossings'][k] = len(info)
        out['openBorder'][k] = int(m.sum())
    return out


def count_dead_ends(public, region):
    """Alley ends: open tiles of the region with exactly one open neighbour, counted once per cluster of such tiles
    that a passage of one or two tiles ends in."""
    p = public & region
    n = np.zeros(p.shape, dtype=np.int32)
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        n += np.roll(p, (dy, dx), (0, 1))
    tips = p & (n <= 1)
    # A two-wide alley's end: two side-by-side tiles with two open neighbours each, the end tile row closed.
    two = p & (n == 2)
    pair = np.zeros(p.shape, dtype=bool)
    for dy, dx in ((0, 1), (1, 0)):
        other = np.roll(two, (-dy, -dx), (0, 1))
        pair |= two & other
    _, info = label(tips | pair)
    return len(info)


# ---------------------------------------------------------------------------------------------------------------
# The plan, the report, the manifest and the previews
# ---------------------------------------------------------------------------------------------------------------
def make_all(project, seed=SEED, report=print):
    """The city, the NPC plan and the finished project, its graph walk and its checks."""
    from . import ridgemere2_people as pp
    current = strip_city(project)
    base, world, city = make_city(project, seed, report)
    placement = pp.Placement(base, city, current={x['id']: x for x in current['people']})
    plan = placement.run()
    names = {rec['key']: rec['name'] for rec in city.records}
    # Names as the world has them now (renamed by doc 67's lane C, when it has run): each reused room's own name.
    for rec in city.records:
        if rec.get('reused'):
            names[rec['key']] = city.rooms[rec['id']]['name']
    labels = pp.render_labels(plan, placement.people, names, {x['id']: x['workLabel'] for x in current['people']})
    final = compose(current, world, city, plan, labels, {x['id']: x for x in base['people']}, names)
    walk = area_graph_reachable(final, placed_rooms={room['id'] for r in city.records for room in r['rooms']})
    capacity = {'before': food_capacity(current), 'after': food_capacity(final)}
    classes = pp.class_report(placement, plan, labels)
    result = checks(final, world, city, plan, labels, walk, capacity, classes)
    return {'base': base, 'world': world, 'city': city, 'placement': placement, 'plan': plan, 'labels': labels,
            'project': final, 'walk': walk, 'checks': result, 'capacity': capacity, 'classes': classes}


def buildings_by_stairs(project):
    """{interior: its building} (the interiors joined by stairs, named by the first of them)."""
    adj = defaultdict(set)
    for l in project['links']:
        if l['kind'] == 'stairs':
            adj[l['a']['cell']].add(l['b']['cell'])
            adj[l['b']['cell']].add(l['a']['cell'])
    out = {}
    for r in sorted(adj):
        if r in out:
            continue
        group, todo = set(), [r]
        while todo:
            x = todo.pop()
            if x not in group:
                group.add(x)
                todo += adj[x]
        for x in group:
            out[x] = min(group)
    return out


def food_capacity(project):
    """The capacity estimate (tools/city_capacity.py) for a project's placed people: what they buy a day as keeper-days
    of each trade, the trades' keepers and hands, and the food check's figures."""
    import city_capacity as cc
    folk, keepers, hands = cc.from_project(project, buildings_by_stairs(project))
    est = cc.estimate(folk, keepers)
    ok, food = cc.food_balance(est, keepers + hands)
    rows = cc.trade_rows(est, keepers, hands, cc.FOOD_TRADES + cc.HOUSEHOLD_TRADES)
    return {'ok': ok, 'food': food, 'rows': rows, 'businesses': dict(keepers),
            'foodBusinesses': sum(keepers.get(b, 0) for b in cc.FOOD_TRADES),
            'householdBusinesses': sum(keepers.get(b, 0) for b in cc.HOUSEHOLD_TRADES)}


def write_outputs(m, folder=ART, images=True):
    """artifacts/ridgemere2: the preview images, city-report.json and .txt, buildings.json and placement.json."""
    from . import ridgemere2_people as pp
    folder.mkdir(parents=True, exist_ok=True)
    city, world, p, plan = m['city'], m['world'], m['project'], m['plan']
    people = {x['id']: x for x in p['people']}
    placed = [x for x in p['people'] if x['home']]
    # The manifest for talk_seed (artifacts/<world key>/buildings.json): every placed structure, with its district.
    manifest = []
    for rec in city.records:
        manifest.append({k: rec[k] for k in ('id', 'name', 'kind', 'district', 'style', 'trade', 'door', 'facing', 'rooms')
                         if k in rec} | {'block': rec['block'], 'reused': bool(rec.get('reused')), 'key': rec['key'],
                                         'business': rec.get('business'), 'area': rec['area']})
    (folder / 'buildings.json').write_text(json.dumps(manifest, indent=1), encoding='utf-8')
    # The NPC plan: business and role, not label text (labels are rendered when it is written).
    out = {}
    for pid, e in sorted(plan.items()):
        if not e.get('placed'):
            out[pid] = {'placed': False, 'why': e['why']}
            continue
        out[pid] = {'placed': True, 'home': e['home'], 'work': e['work'], 'evening': e['evening'],
                    'eveningPlace': e.get('eveningPlace'), 'business': e.get('business'), 'role': e['role'],
                    'post': e.get('post'), 'template': e.get('template'), 'structure': e.get('record'),
                    'route': people[pid]['route'], 'homeKept': e.get('homeKept')}
    (folder / 'placement.json').write_text(json.dumps(out, indent=0), encoding='utf-8')
    # The report.
    rep = report_data(m)
    (folder / 'city-report.json').write_text(json.dumps(rep, indent=1), encoding='utf-8')
    (folder / 'city-report.txt').write_text(report_text(rep), encoding='utf-8')
    if images:
        previews(m, folder / 'preview')
    return rep


def report_data(m):
    from . import ridgemere2_people as pp
    city, p, plan, labels = m['city'], m['project'], m['plan'], m['labels']
    placed = [x for x in p['people'] if x['home']]
    room_struct = {room['id']: r for r in city.records for room in r['rooms']}
    by_dist = defaultdict(lambda: Counter())
    for r in city.records:
        d = r['area']
        by_dist[d]['buildings'] += 1
        by_dist[d][f'kind:{r.get("kindOf")}'] += 1
    homes = Counter(x['home']['cell'] for x in placed)
    per_struct = Counter()
    for room in homes:
        rec = room_struct.get(room)
        per_struct[rec['id'] if rec else room] += 1
        if rec:
            by_dist[rec['area']]['homes'] += 1
            by_dist[rec['area']]['beds'] += sum(row.count('b') + row.count('z') for row in
                                                next(r for r in p['rooms'] if r['id'] == room)['terrain'])
    for x in placed:
        e = plan[x['id']]
        rec = city.named.get(e.get('record')) if e.get('record') else None
        d = rec['area'] if rec else ('outdoors' if e.get('work') else 'none')
        if e.get('work') or e.get('route'):
            by_dist[d]['posts'] += 1
        if x['role'] == 'merchant':
            by_dist[d][f'business:{e.get("business") or pp.business_for(x["workLabel"])}'] += 1
    jobs = Counter()
    for x in placed:
        e = plan[x['id']]
        jobs[e.get('post') or 'none'] += 1
    biz = Counter(pp.business_for(x['workLabel']) for x in placed if x['role'] == 'merchant')
    draws = {}
    for b in sorted(k for k in biz if k):
        making, resale = pp.store_goods(b)
        draws[b] = {'making': making, 'resale': resale}
    links = Counter(l['kind'] for l in p['links'])
    sump = city.sump_metrics(m['walk'])
    sump.pop('checks', None)
    residents = Counter()
    for x in placed:
        rec = room_struct.get(x['home']['cell'])
        if rec:
            residents[rec['area']] += 1
    density = district_metrics(city, m['walk'], residents)
    density.pop('badSteps', None)
    design = Counter(r.get('homes', 0) for r in city.records if r.get('homes'))
    sump_design = Counter(r.get('homes', 0) for r in city.records if r.get('sump_home'))
    used = Counter(per_struct[r['id']] for r in city.records if per_struct.get(r['id']))
    sump_used = Counter(per_struct[r['id']] for r in city.records if r.get('sump_home') and per_struct.get(r['id']))
    before = {}
    if BEFORE.exists():
        before = json.loads(BEFORE.read_text(encoding='utf-8'))
    return {
        'density': density, 'before': before, 'landmarks': city.landmarks,
        'homesPerStructure': {'built': dict(sorted(design.items())), 'livedIn': dict(sorted(used.items())),
                              'sumpBuilt': dict(sorted(sump_design.items())), 'sumpLivedIn': dict(sorted(sump_used.items()))},
        'world': {'cells': len(p['cells']), 'interiors': len(p['rooms']), 'newInteriors': len(city.site.rooms),
                  'links': dict(links), 'structures': len(city.records)},
        'districts': {d: dict(v) for d, v in sorted(by_dist.items())},
        'homes': {'total': len(homes), 'perStructureMax': max(per_struct.values()),
                  'perStructure': dict(Counter(per_struct.values())), 'aloneHomes': m['placement'].alone},
        'people': {'placed': len(placed), 'unplaced': len(p['people']) - len(placed),
                   'unplacedWhy': dict(Counter(e['why'] for e in plan.values() if not e.get('placed'))),
                   'roles': dict(Counter(x['role'] for x in placed)), 'posts': dict(jobs),
                   'evenings': dict(Counter(plan[x['id']].get('eveningPlace') for x in placed))},
        'businesses': dict(sorted((k or '-', v) for k, v in biz.items())),
        'draws': draws,
        'labels': sorted([[x['id'], x['role'], x['workLabel'], pp.business_for(x['workLabel'])] for x in placed]),
        'stables': [x['id'] for x in placed if pp.producer_for(x['workLabel']) == 'stock_breeding'],
        'sump': sump,
        'checks': m['checks'],
        'capacity': {**m['capacity'], 'textFixes': dict(getattr(city, 'text_fixes', {})),
                     'textFixed': getattr(city, 'text_fixed', [])},
        'classes': m.get('classes'),
        'issues': m['placement'].issues[:50],
        'notes': ['The store room is not used by any NPC yet: lane B builds its restocking rule behind a switch that is '
                  'off (doc 67, Q2).',
                  'House Timberland keeps the ids of House Mossgrave (house_mossgrave, rm_mossgrave_court...): only the '
                  'text changes (lane C), and the ids still read "mossgrave" in Atlas and the DM.'],
    }


def report_text(rep):
    lines = ['Ridgemere 2.0: the city (doc 67), the generator\'s report', '']
    w = rep['world']
    lines.append(f'{w["cells"]} cells; {w["interiors"]} interiors ({w["newInteriors"]} new); {w["structures"]} structures '
                 f'placed; links {w["links"]}')
    pe = rep['people']
    lines.append(f'People: {pe["placed"]} placed, {pe["unplaced"]} unplaced {pe["unplacedWhy"]}; roles {pe["roles"]}')
    lines.append(f'Posts: {pe["posts"]}')
    lines.append(f'Evenings: {pe["evenings"]}')
    h = rep['homes']
    lines.append(f'Homes: {h["total"]}; most in one structure {h["perStructureMax"]}; structures by homes {h["perStructure"]}; '
                 f'{h["aloneHomes"]} living alone')
    lines.append('')
    lines.append('By district:')
    for d, v in rep['districts'].items():
        lines.append(f'  {d}: ' + ', '.join(f'{k} {n}' for k, n in sorted(v.items())))
    lines.append('')
    lines.append('Businesses with a till (merchants): ' + ', '.join(f'{k} {v}' for k, v in rep['businesses'].items()))
    lines.append('')
    lines.append('Draws (generated: item_catalog.store_goods, from crafts.json):')
    for b, d in rep['draws'].items():
        lines.append(f'  {b}: making {", ".join(d["making"]) or "-"}; resale {", ".join(d["resale"]) or "-"}')
    lines.append('')
    d, b = rep['density'], rep.get('before') or {}
    bd = b.get('districts', {})
    lines.append(f'Density (doc 67, Dense city): inside the walls {d["insideArea"]:,} tiles (before {b.get("inside_area", 0):,}), '
                 f'built {d["builtInside"]:.0%} (before {b.get("built_inside%", 0)}%)')
    lines.append('  district     land (before)     built (before)   open tiles a resident (before)   residents')
    for name, v in d['districts'].items():
        o = bd.get(name, {})
        lines.append(f'  {name:11} {v["land"]:6} ({o.get("land", "-"):>6})   {v["built"]:5.0%} ({o.get("built%", "-"):>5}%)   '
                     f'{str(v["openPerResident"]):>8} ({str(o.get("open/res", "-")):>7})            {v["residents"]}')
    hp = rep['homesPerStructure']
    lines.append(f'Homes in one structure: built {hp["built"]}, lived in {hp["livedIn"]}; the Sump: built {hp["sumpBuilt"]}, '
                 f'lived in {hp["sumpLivedIn"]}')
    lines.append(f'Walls on district borders: {d["wallsOnBorders"]} (before: {b.get("tier_edge_wall_tiles", "-")} retaining-wall '
                 f'tiles); height steps on the quarters\' borders {d["stepOnBorder"]:.1%}, elsewhere {d["stepOffBorder"]:.1%}')
    lines.append(f'Crossings between districts (separate openings): {d["crossings"]} (before: {b.get("crossings", "-")})')
    lines.append(f'Open border (tiles): {d["openBorder"]}')
    lines.append('')
    lines.append('Landmarks:')
    for lm in rep['landmarks']:
        lines.append(f'  {lm["name"]} ({lm["district"]}, {lm["x"]},{lm["y"]}): {lm["what"]}' +
                     (f'; interiors {", ".join(lm["interiors"])}' if lm['interiors'] else ''))
    lines.append('')
    s = rep['sump']
    lines.append(f'The Sump: coverage {s["coverage"]:.1%} (planned {s["planned"]:.1%}), widest open {s["widest"]}, narrow '
                 f'{s["narrow"]:.0%}, dead ends {s["dead_ends"]}, sizes {s["sizes"]}, L-shapes {s["lshape"]}, through '
                 f'{s["through"]}, passages {s["passages"]} in chains {s["chains"]}, courts {s["courts"]}, homes '
                 f'{s["homes"]}, infill {s["infill"]}')
    lines.append('')
    cap = rep.get('capacity')
    if cap:
        b, a = cap['before'], cap['after']
        lines.append('The trades against what the placed residents buy a day (tools/city_capacity.py; keeper-days: the '
                     'hours of each trade\'s own last step over 8), before and after this write:')
        lines.append(f'  {"trade":12} {"businesses":>17} {"keepers":>13} {"hands":>11} {"keeper-days":>15}')
        for (t, bb, bk, bh, bd), (_, ab, ak, ah, ad) in zip(b['rows'], a['rows']):
            lines.append(f'  {t:12} {bb:8} -> {ab:<5} {bk:6} -> {ak:<4} {bh:4} -> {ah:<4} {bd:6.1f} -> {ad:<5.1f}')
        for k, v in (('before', b), ('after', a)):
            f = v['food']
            lines.append(f'  {k}: food {v["foodBusinesses"]} businesses, {f["staffed"]} keepers and hands for '
                         f'{f["demand"]:.1f} keeper-days ({f["ratio"]:.0%}), below half: {f["short"] or "none"}; '
                         f'household goods {v["householdBusinesses"]} businesses')
        lines.append(f'  People\'s text written again: {cap.get("textFixes", {})}')
        lines.append('')
    cl = rep.get('classes')
    if cl:
        lines.append('Jobs by class (doc 67, Progress): earners whose job is of their household\'s class (the Watch and the '
                     'clergy apart), before and after the class pass:')
        for k, v in (('before', cl['before']), ('after', cl['after'])):
            if v:
                lines.append(f'  {k}: {v["matched"]}/{v["earners"]} ({v["rate"]:.1%}); ' + ', '.join(
                    f'{c} {x["matched"]}/{x["earners"]} ({x["rate"]:.0%})' for c, x in v['classes'].items()) +
                    f'; own cases {v["ownCases"]}; Minor House households without an upper job: '
                    f'{v["minorWithoutUpper"] or "none"}; larger businesses kept from the Sump, the Cinders or the wharf: '
                    f'{len(v["largeLower"])}; Rag Market keepers not of the middle class: {len(v.get("ragNotMiddle", []))}')
        lines.append('  Not of their class (after):')
        lines += [f'    - {u}' for u in cl['after']['unmatched']]
        lines.append(f'  Moved by the class pass ({len(cl["moves"])}):')
        lines += [f'    - {u}' for u in cl['moves']]
        lines.append('')
    lines.append('Checks:')
    for c in rep['checks']:
        lines.append(f'  [{"ok" if c["ok"] else "FAIL"}] {c["req"]:6} {c["check"]}' + (f': {c["detail"]}' if c['detail'] else ''))
    lines.append('')
    lines.append(f'Stables (stock_breeding, horses not food): {", ".join(rep["stables"])}')
    lines.append('')
    lines += ['Notes:'] + [f'  - {n}' for n in rep['notes']]
    if rep['issues']:
        lines += ['', 'Issues:'] + [f'  - {i}' for i in rep['issues']]
    lines += ['', 'Labels (npc, role, label, business):']
    lines += [f'  {a:30} {b:9} {c:42} {d}' for a, b, c, d in rep['labels']]
    return '\n'.join(lines) + '\n'


def previews(m, folder):
    folder.mkdir(parents=True, exist_ok=True)
    world, city, p = m['world'], m['city'], m['project']
    c = world.c
    cells = [{'x': x['x'], 'y': x['y'], 'width': CELL, 'height': CELL, 'name': x['name'][:34]} for x in p['cells']]
    labels = [((x - 20, y), name) for name, (x, y) in {
        'THE WHARF': (186, 250), 'ABBEY': (236, 200), 'NORTHGATE': (330, 168), 'GREATLY': (420, 196),
        'THE SUMP': (240, 330), 'THE COMMONS': (420, 270), 'MARKET SQ': (365, 322), 'OLD KEEP': (412, 332),
        'ADMIN': (290, 356), 'CINDERS': (236, 420), 'GREAT PALACE': (340, 406), 'THE RISE': (430, 400),
        'TIMBERLAND COURT': (356, 520), 'FELLHALL': (600, 262), 'GRAYROCK HALL': (190, 642), 'BAY LIGHT': (70, 168)}.items()]
    preview.overview(c.codes, c.heights, cells, folder / 'overview.png', scale=2, labels=labels, relief=1.5)
    city_box = (160, 142, 320, 320)
    x0, y0, w, h = city_box
    crop = np.s_[y0:y0 + h, x0:x0 + w]
    preview.overview(c.codes[crop], c.heights[crop], [], folder / 'city.png', scale=4,
                     labels=[((x - x0, y - y0), t) for (x, y), t in labels if x0 <= x < x0 + w and y0 <= y < y0 + h],
                     relief=1.5)
    views = {'sump': (205, 205, 110, 150), 'market_square_and_hall': (320, 250, 90, 80), 'keep': (380, 290, 90, 80),
             'palace': (264, 340, 130, 106), 'abbey': (205, 150, 100, 72), 'greatly_compound': (385, 150, 87, 100),
             'records_square': (300, 340, 70, 50), 'wharf': (60, 150, 160, 300), 'cinders': (205, 340, 80, 108),
             'estate_timberland_court': (290, 514, 132, 98), 'estate_fellhall': (542, 256, 148, 124),
             'estate_grayrock_hall': (130, 636, 120, 98), 'rise': (370, 340, 102, 108),
             'commons_west_edge': (270, 250, 90, 110), 'commons_east': (370, 210, 102, 130),
             'northgate': (280, 150, 160, 80), 'mercy_and_cisterns': (205, 260, 80, 80)}
    for name, (x0, y0, w, h) in views.items():
        preview.glyphs(c.codes, c.heights, folder / f'{name}.png', x0, y0, min(w, W - x0), min(h, H - y0), px=8)
    for stale in ('interior_r2_eel_rents.png', 'interior_r2_eel_rents_up.png', 'interior_r2_peg_corner.png',
                  'interior_r2_peg_corner_up.png', 'interior_r2_rag_buildings.png', 'interior_r2_rag_buildings_up.png'):
        if (folder / stale).exists():
            (folder / stale).unlink()
    rooms = {r['id']: r for r in p['rooms']}
    for key in ('r2_records_office', 'r2_the_keep', 'r2_market_hall', 'r2_palace_antechamber', 'r2_kings_residence',
                'r2_administrative_wing', 'r2_cathedral', 'r2_bay_light', 'r2_clock_tower', 'r2_the_knucklebones',
                'r2_sunken_street', 'r2_tallow_arcade'):
        if key not in city.named:
            continue
        for room in city.named[key]['rooms']:
            r = rooms[room['id']]
            codes = np.frombuffer(''.join(r['terrain']).encode('ascii'), dtype=np.uint8).reshape(r['height'], r['width'])
            preview.glyphs(codes, np.zeros(codes.shape, dtype=np.float32), folder / f'interior_{room["id"]}.png', 0, 0,
                           r['width'], r['height'], px=14)
    for hm in city.sump_homes_[:3]:
        for room in hm['record']['rooms']:
            r = rooms[room['id']]
            codes = np.frombuffer(''.join(r['terrain']).encode('ascii'), dtype=np.uint8).reshape(r['height'], r['width'])
            preview.glyphs(codes, np.zeros(codes.shape, dtype=np.float32), folder / f'interior_{room["id"]}.png', 0, 0,
                           r['width'], r['height'], px=14)


# ---------------------------------------------------------------------------------------------------------------
# The write (doc 67, section 8 A): back up, load fresh, check, save, tell editors
# ---------------------------------------------------------------------------------------------------------------
def load(world_key):
    import world_db
    import world_store
    with world_db.connect(world_key, 'editor') as conn:
        wid = world_store.list_worlds(conn)[0]['id']
        project, revision = world_store.load_world(conn, wid)
    return project, revision


def backup(world_key, project, revision):
    import subprocess
    import world_db
    folder = ROOT / 'artifacts' / 'backups'
    folder.mkdir(parents=True, exist_ok=True)
    ts = time.strftime('%Y%m%d-%H%M%S')
    info = world_db.conninfo(world_key, 'owner')
    dump = folder / f'ratw_{world_key}_{ts}.pgdump'
    env = dict(__import__('os').environ, PGPASSWORD=info['password'])
    args = ['-h', info['host'], '-p', str(info['port']), '-U', info['user'], '-Fc', '-f', str(dump), info['dbname']]
    try:
        subprocess.run(['pg_dump', *args], check=True, env=env, capture_output=True, timeout=600)
    except (FileNotFoundError, subprocess.CalledProcessError):
        # No pg_dump on the host: the one in the database's container.
        with open(dump, 'wb') as out:
            subprocess.run(['docker', 'exec', '-e', f'PGPASSWORD={info["password"]}', 'ratw-postgres-1', 'pg_dump', '-h',
                            '127.0.0.1', '-p', '5432', '-U', info['user'], '-Fc', info['dbname']], check=True, stdout=out,
                           timeout=600)
    atlas = folder / f'{project["id"]}_{world_key}_r{revision}_{ts}.atlas.json'
    atlas.write_text(json.dumps(project, ensure_ascii=False), encoding='utf-8')
    return dump, atlas


def write(world_key, seed=SEED, dry=False, report=print, map_mode='auto'):
    """Steps 1-4: back up; load the world fresh and lay the city over it; check; save, with the voices' quarters, in
    one transaction; tell open Atlas sessions to reload. Refuses, writing nothing, if any check fails."""
    import map_editor
    import world_db
    import world_store
    import world_sync
    from . import ridgemere2_people as pp
    entry = world_db.world(world_key)
    if entry['kind'] != 'dev' or entry['database'] == 'ratw_dev':
        raise SystemExit('Ridgemere 2.0 is written only into its own world (dev, also called ridgemere2), never a live '
                         'world or the old map\'s archive dev-old.')
    project, revision = load(world_key)
    m = make_all(project, seed, report)
    failed = [c for c in m['checks'] if not c['ok']]
    rep = write_outputs(m)
    p = m['project']
    p.pop('relations', None)
    try:
        map_editor.check_project(p, for_game=False, playable=True)
    except map_editor.ValidationError as error:
        report('Atlas validation failed:\n  ' + '\n  '.join(error.errors[:30]))
        return 1
    if failed:
        report('Checks failed; nothing written:\n  ' + '\n  '.join(f'{c["check"]}: {c["detail"]}' for c in failed))
        return 1
    if dry:
        report('All checks pass (dry run: nothing written).')
        return 0
    dump, atlas = backup(world_key, project, revision)
    if not BASELINE.exists():
        BASELINE.write_text(json.dumps(baseline_people(project), indent=0), encoding='utf-8')
        report(f'The NPCs as they were before the city: {BASELINE}.')
    report(f'Backed up to {dump} and {atlas}. (To restore: close every connection, then pg_restore --clean -d '
           f'"<owner conninfo>" {dump}.)')
    quarters = voice_quarters(m)
    # As the owner: a changed layout is a new map, so the world gets a new map identity (admin.settings, the owner's) in
    # the same transaction, and pull and import with a world on the old map are refused from then on (doc 66, "The same
    # map"); a write that changes people alone keeps the identity (world_sync.stamp_for_layout, by the layout's
    # fingerprint; --new-map or --same-map say so outright).
    with world_db.connect(world_key, 'owner') as conn:
        with conn.transaction():
            new = world_store.save_world(conn, p, revision)
            # A new map identity only when the layout changed (or --new-map): a write of people alone stays on its map.
            ident, kept = world_sync.stamp_for_layout(conn, world_sync.layout_fingerprint(p),
                                                      'Ridgemere 2.0: the city (doc 67)', 'worldgen.ridgemere2', 'generated',
                                                      map_mode)
            wid = p['id']
            changed = 0
            for npc, quarter in quarters.items():
                changed += conn.execute('UPDATE live.npc_voice SET quarter = %s WHERE world_id = %s AND npc_id = %s '
                                        'AND quarter IS DISTINCT FROM %s', (quarter, wid, npc, quarter)).rowcount
            world_sync.tell_editors(conn, wid, 'ridgemere2.py', 'Ridgemere 2.0: the city (doc 67)')
    report(f'Saved: revision {new} (+1 for the editors\' reload); {changed} voices given their new quarter; ' +
           (f'the same map, {ident["id"]} (its layout unchanged).' if kept == 'kept' else
            f'a new map identity, {ident["id"]} (the layout changed; pull and import with a world on the old map are refused).'))
    return 0


def voice_quarters(m):
    """The talk quarter of each placed NPC's home, from the manifest's district (talk_seed.quarter_of)."""
    import talk_seed
    city, p = m['city'], m['project']
    room_rec = {room['id']: r for r in city.records for room in r['rooms']}
    out = {}
    for x in p['people']:
        if not x['home']:
            continue
        rec = room_rec.get(x['home']['cell'])
        if rec:
            q = talk_seed.quarter_of(rec['district'])
            if q:
                out[x['id']] = q
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    for name in ('preview', 'place', 'write'):
        s = sub.add_parser(name)
        s.add_argument('--seed', type=int, default=SEED)
        s.add_argument('--world', default='ridgemere2')
        s.add_argument('--from-file', type=Path, help='a project JSON (load_world\'s) instead of the database')
        s.add_argument('--dry-run', action='store_true')
        if name == 'write':
            m = s.add_mutually_exclusive_group()
            m.add_argument('--new-map', action='store_const', const='new', dest='map_mode',
                           help='stamp a new map identity whatever the layout')
            m.add_argument('--same-map', action='store_const', const='same', dest='map_mode',
                           help='keep the world\'s map identity whatever the layout')
    args = parser.parse_args(argv)
    if args.command == 'write':
        return write(args.world, args.seed, dry=args.dry_run, map_mode=args.map_mode or 'auto')
    if args.from_file:
        data = json.loads(args.from_file.read_text(encoding='utf-8'))
        project = data.get('project', data)
    else:
        project, _ = load(args.world)
    m = make_all(project, args.seed)
    rep = write_outputs(m, images=args.command == 'preview')
    failed = [c for c in m['checks'] if not c['ok']]
    print(f'{rep["people"]["placed"]} placed, {rep["people"]["unplaced"]} unplaced; {len(m["checks"]) - len(failed)} of '
          f'{len(m["checks"])} checks pass')
    for c in failed:
        print(f'  FAIL {c["check"]}: {c["detail"][:300]}')
    return 0 if not failed else 1


if __name__ == '__main__':
    raise SystemExit(main())
