"""Ridgemere 2.0: the city (Docs/Design/67-ridgemere-2-the-city.md, section 8 A).

Lays out the city alone on a canvas of 1,024 x 768 tiles (4 x 3 cells): the bay on the west, the wharf inside its sea
wall, the Sump, the Commons and Northgate, the abbey, House Greatly's compound, the Old Keep, the administrative sector,
the Cinders, the Great Palace and the Rise inside the walls; the three Houses' estates and the roads outside. Reused
interiors keep their IDs; new ones are r2_. Then places about 560 of the world's NPCs in it (worldgen.ridgemere2_people)
and writes the city into Atlas's world ratw_ridgemere2, keeping every NPC, their voices, families and talk.

  cd tools && python3 -m worldgen.ridgemere2 preview [--seed N]     preview images, the report and the manifest
  cd tools && python3 -m worldgen.ridgemere2 place                  the NPC plan (artifacts/ridgemere2/placement.json)
  cd tools && python3 -m worldgen.ridgemere2 write --world ridgemere2   back up, then write the city into the world

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
from .canvas import DEFAULT_HEIGHT, RAMP, SOLID, Canvas, half  # noqa: E402
from .cities import CitySite, Ridgemere, fp, grow  # noqa: E402
from .site import OPPOSITE, STEP as DOOR_STEP, TURNS, Lots, door_tile, footprint_size  # noqa: E402

import terrain_catalog  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / 'artifacts' / 'ridgemere2'
W, H = 1024, 768
CELL = 256
SEED = 20261030
REGION = 'ridgemere'

# --- The plan's geometry (doc 67, section 2). Coordinates are canvas tiles; ranges are half-open. -----------------
SHORE = [(96, 0), (110, 120), (168, 170), (168, 610), (120, 660), (96, 768)]
INSIDE = (172, 660, 154, 616)            # x0, x1, y0, y1 inside the wall
WALL = 4
LEVEL = {'wharf': .5, 'sump': 1.0, 'cinders': 1.0, 'abbey': 2.0, 'northgate': 2.0, 'commons': 2.0, 'keep': 2.0,
         'admin': 2.0, 'palace': 2.0, 'greatly': 3.0, 'rise': 3.0}
DISTRICT_IDS = ['outside', 'wharf', 'abbey', 'northgate', 'greatly', 'sump', 'commons', 'keep', 'admin', 'cinders',
                'palace', 'rise', 'wall']
# The talk quarter each district's buildings are filed under (doc 67, section 5, Talk; talk_seed.quarter_of).
TALK_DISTRICT = {'wharf': 'wharf', 'abbey': 'commons_abbey', 'northgate': 'commons_northgate', 'greatly': 'estate_greatly',
                 'sump': 'sump', 'commons': 'commons', 'keep': 'heights_keep', 'admin': 'office', 'cinders': 'works',
                 'palace': 'office_palace', 'rise': 'heights'}
GROUND = {'wharf': 'f', 'abbey': 'f', 'northgate': '.', 'greatly': ',', 'sump': '.', 'commons': '.', 'keep': 'f',
          'admin': 'f', 'cinders': '>', 'palace': 'f', 'rise': ','}
GATES = {'N': (476, 150), 'E': (660, 360), 'S': (600, 620), 'cinder': (290, 620), 'harbour': (197, 620)}
PIERS = [200, 245, 290, 335, 380, 425, 470, 515]
CRANES = [245, 335, 425, 515]
SQUARES = {'market': ((470, 360), (24, 16)), 'rag': ((290, 350), (12, 8)), 'records': ((400, 499), (16, 11)),
           'palace': ((430, 542), (60, 9)), 'greatly': ((600, 282), (14, 8))}
SPAWN = (470, 380)
ESTATES = {'mossgrave': ('Timberland Court', (330, 650, 150, 100)), 'fell': ('Fellhall', (780, 300, 160, 130)),
           'grayrock': ('Grayrock Hall', (125, 650, 125, 100))}


def box(x0, x1, y0, y1):
    m = np.zeros((H, W), dtype=bool)
    m[max(0, y0):max(0, y1), max(0, x0):max(0, x1)] = True
    return m


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
    (0, 0): ('Ridgemere Bay, North Headland', 'The north headland of Ridgemere Bay: wet moor above the water, and the '
             'abbey\'s wall and the wharf\'s north end inside the city wall.'),
    (1, 0): ('Ridgemere, Northgate', 'Northgate: the middle town\'s north quarter under the North Gate, the abbey close '
             'at its west end and the North Road running out across the moor.'),
    (2, 0): ('Ridgemere, the Greatly Compound', 'House Greatly\'s walled compound in the city\'s north-east corner, '
             'hard under the wall the Greatlys say they built.'),
    (3, 0): ('The Rainward Moor, North', 'Wet heath and moor north-east of the city walls.'),
    (0, 1): ('Ridgemere, the Wharf', 'The wharf inside the sea wall: piers, cranes and warehouses, and the first '
             'crooked alleys of the Sump running down to the quay.'),
    (1, 1): ('Ridgemere, the Sump and the Commons', 'The Sump\'s alleys and the Rag Market below; up the stairs, the '
             'Commons and Market Square, the city\'s market.'),
    (2, 1): ('Ridgemere, the East Commons', 'The Commons\' east end: the Old Keep, the East Gate and the East Road out '
             'of it.'),
    (3, 1): ('Fellhall', 'House Fell\'s estate on the East Road: the hall, its press house, distillery and cellars.'),
    (0, 2): ('Grayrock Hall', 'The Cinders\' smoke over the south wharf and the slipway; outside the Harbour Gate, '
             'House Grayrock\'s hall on the south headland.'),
    (1, 2): ('Ridgemere, the Cinders and the Palace', 'The Cinders, the city\'s forges and kilns; the Records Office and '
             'the Great Palace above them; outside the Cinder Gate, Timberland Court.'),
    (2, 2): ('Ridgemere, the Rise', 'The Rise, where the Minor Houses keep their walled mansions, and the South Gate.'),
    (3, 2): ('The Rainward Moor, South', 'Wet heath and moor south-east of the city walls.'),
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
             'rm_the_grayrock_iron_store': ['house_mossgrave'], 'rm_the_fell_sawmill': ['house_mossgrave']}


class Spec:
    """A building to place: a new one (`make`: a Building) or a reused structure (`reuse`), with where it goes."""

    def __init__(self, key, name, district, kind, make=None, reuse=None, near=None, homes=0, style='ridgemere',
                 open_door=True, claims=None, business=None):
        self.key, self.name, self.district, self.kind = key, name, district, kind
        self.make, self.reuse, self.near = make, reuse, near
        self.homes, self.style, self.open_door = homes, style, open_door
        self.claims, self.business = claims, business
        self.building = make() if make else None

    @property
    def footprint(self):
        if self.building is not None:
            return self.building.footprint
        return fp(*self.reuse['size'])

    @property
    def area(self):
        fw, fh = self.footprint
        return fw * fh


# ---------------------------------------------------------------------------------------------------------------
# The city
# ---------------------------------------------------------------------------------------------------------------
class RidgemereCity(Ridgemere):
    """Ridgemere 2.0 inside its walls (doc 67, sections 2 and 3)."""

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
        self.edges = np.zeros((H, W), dtype=bool)
        self.infill = Counter()
        self.gate_inside = {'N': (476, 159), 'E': (654, 360), 'S': (600, 609), 'harbour': (197, 610),
                            'cinder': (290, 610), 'customs': (179, 473)}

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
        return float(self.levels_map[int(y), int(x)])

    # -- placing a building ------------------------------------------------------------------------------------------
    def put(self, spec, x0, y0, facing, w=None, h=None, mask=None):
        """Place a spec at (x0, y0) with its door on `facing`; w, h: the block as placed (a reused building's)."""
        c = self.c
        if spec.building is not None:
            b = spec.building
            b.district = TALK_DISTRICT.get(spec.district, spec.district)
            ground = self.level_at(x0 + footprint_size(b, facing)[0] // 2, y0 + footprint_size(b, facing)[1] // 2)
            bid, outside = self.site.place(b, x0, y0, facing, REGION, open_door=spec.open_door, mask=mask,
                                           ground=ground)
            record = self.site.manifest[-1]
            w, h = footprint_size(b, facing)
        else:
            g = spec.reuse
            ground = self.level_at(x0 + w // 2, y0 + h // 2)
            style = STYLES[spec.style]
            c.rect(x0, y0, w, h, style['outer'], ground)
            c.rect(x0 + 1, y0 + 1, w - 2, h - 2, style['roof'][0], ground)
            (dx, dy), outside = door_tile(x0, y0, w, h, facing)
            c.codes[dy, dx] = ord('+')
            cid, lx, ly = self.site.cell_at(dx, dy)
            d = g['door']
            self.site.links.append({'id': d['id'], 'name': d['name'], 'kind': 'door',
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
                r['worldX'], r['worldY'] = dx + 2 * len(record['rooms']), dy
            self.site.manifest.append(record)
        record['key'] = spec.key
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

    def door_ok(self, x0, y0, w, h, facing, street, deep=False):
        """The door's tile and the street tile outside it are in the same cell, the street tile is a street (not a tier
        edge); `deep`: the tile beyond it too, so the door opens onto a passage at least two wide."""
        (dx, dy), (ox, oy) = door_tile(x0, y0, w, h, facing)
        if not (0 <= ox < W and 0 <= oy < H) or not street[oy, ox] or self.edges[oy, ox]:
            return False
        if deep:
            sx, sy = DOOR_STEP[facing]
            px, py = ox + sx, oy + sy
            if not (0 <= px < W and 0 <= py < H) or not street[py, px]:
                return False
        return cell_of(dx, dy) == cell_of(ox, oy)

    def fit(self, lots, spec, near, street, orient=True, limit=60000, facings='SNEW', deep=False):
        """The nearest lot for a spec (both ways round for a reused building, whose facing is cosmetic)."""
        fw, fh = spec.footprint
        shapes = [(fw, fh)]
        if spec.building is None and orient and fw != fh:
            shapes.append((fh, fw))
        best = None
        for sw, sh in shapes:
            got = self.nearest_lot(lots, sw, sh, near, street, facings=facings, deep=deep)
            if got and (best is None or got[0] < best[0]):
                best = got
        return best

    def nearest_lot(self, lots, w_ns, h_ns, near, street, deep=False, facings='SNEW'):
        """Lots.candidates, all at once: the nearest clear footprint whose door opens onto the street (in the same
        cell, not onto a tier edge; `deep`, onto a passage at least two wide). (d, x0, y0, facing, w, h) or None."""
        ys, xs = np.nonzero(lots.street & street & ~self.edges)
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
            ok = (x0 >= 0) & (y0 >= 0) & (x0 + w <= Ww) & (y0 + h <= Hh)
            x0, y0, dxo, dyo, ox, oy = x0[ok], y0[ok], dx[ok], dy[ok], xs[ok], ys[ok]
            area = s[y0 + h, x0 + w] - s[y0, x0 + w] - s[y0 + h, x0] + s[y0, x0]
            ok = (area == w * h) & (dxo // CELL == ox // CELL) & (dyo // CELL == oy // CELL)
            if deep:
                px, py = ox + sx, oy + sy
                inb = (px >= 0) & (py >= 0) & (px < Ww) & (py < Hh)
                ok &= inb
                ok[inb] &= street[py[inb], px[inb]]
            if not ok.any():
                continue
            x0, y0 = x0[ok], y0[ok]
            d = np.hypot(x0 + w / 2 - near[0], y0 + h / 2 - near[1])
            i = int(np.argmin(d))
            if best is None or d[i] < best[0]:
                best = (float(d[i]), int(x0[i]), int(y0[i]), facing, w, h)
        return best

    def place_list(self, lots, specs, street, jitter=6, gap=None, facings='SNEW', deep=False):
        missing = []
        for spec in specs:
            near = spec.near or self.anchor(spec.district)
            near = (near[0] + self.rng.uniform(-jitter, jitter), near[1] + self.rng.uniform(-jitter, jitter))
            got = self.fit(lots, spec, near, street, facings=facings, deep=deep)
            if got is None:
                missing.append(spec)
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

    def new_tavern(self, name, text, style='ridgemere'):
        plans, _ = tavern(self.new_rng, style, False)
        return Building('tavern', name, style, fp(plans[0].w, plans[0].h), [plans[0].room('', name, text)], [], '')

    def new_flats(self, name, homes, beds, size, footprint, notch=None, back=False, style='ridgemere', prose=''):
        plans, pairs = flats(self.new_rng, style, homes, beds, size, notch, back)
        keys, zs = ['', 'up', 'top'][:homes], [0, 1, 2][:homes]
        floors = ['the ground floor', 'the first floor', 'the top floor']
        names = [name if homes == 1 else f'{name}, {floors[i]}' for i in range(homes)]
        texts = [prose or self.new_rng.choice(FLATS_PROSE) for _ in range(homes)]
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
        self.lay_wall()
        self.lay_streets()
        self.edges = self.tier_edges()
        self.reserved = self.edges | grow(self.wall_band, 1) | self.rings
        self.waterfront_map()
        self.abbey()
        self.greatly_compound()
        self.old_keep()
        self.palace()
        self.market_square()
        self.admin()
        self.rise()
        self.commons()
        self.cinders()
        self.wharf()
        self.sump()
        self.paint_edges()
        self.outside()
        self.patrols()
        return self

    def lay_districts(self):
        c = self.c
        d = np.zeros((H, W), dtype=np.int8)
        parts = {
            'wharf': box(172, 222, 154, 616), 'abbey': box(222, 330, 154, 232), 'northgate': box(330, 540, 154, 250),
            'greatly': box(540, 660, 154, 270), 'sump': box(222, 330, 232, 470) | box(330, 364, 250, 470),
            'commons': box(364, 540, 250, 470) | box(540, 660, 270, 470), 'admin': box(260, 540, 470, 530),
            'cinders': box(222, 260, 470, 530) | box(222, 320, 530, 616), 'palace': box(320, 540, 530, 616),
            'rise': box(540, 660, 470, 616), 'keep': box(580, 640, 445, 500)}
        for name, m in parts.items():
            d[m] = DISTRICT_IDS.index(name)
        self.district = d
        self.levels_map = np.full((H, W), np.nan, dtype=np.float32)
        for name, level in LEVEL.items():
            m = d == DISTRICT_IDS.index(name)
            self.levels_map[m] = level
            c.paint(m, GROUND[name], level)
        self.inside = box(*INSIDE)
        # The walled pieces inside the walls (the greatly() pattern: the region and its ring at the piece's height).
        self.rings = np.zeros((H, W), dtype=bool)
        abbey = self.D('abbey')                 # The city wall to the north, a wall two thick round the rest.
        ring = abbey & ~box(224, 328, 154, 230)
        c.paint(ring, 'H', 2.0)
        self.rings |= ring
        greatly = self.D('greatly')             # The city wall north and east, a compound wall west and south.
        ring = greatly & ~box(542, 660, 154, 268)
        c.paint(ring, 'H', 3.0)
        self.rings |= ring
        keep = self.D('keep')                   # Four walls three thick, a tower at each corner.
        ring = keep & ~box(583, 637, 448, 497)
        for tx, ty in ((580, 445), (633, 445), (580, 493), (633, 493)):
            ring |= box(tx, tx + 7, ty, ty + 7)
        c.paint(ring, '#', 2.0)
        self.rings |= ring

    def lay_wall(self):
        c, w = self.c, self.world
        band = box(168, 664, 150, 620) & ~self.inside
        self.wall_band = band
        c.paint(band, '#', 2.0)
        # Towers at the corners and about every 100 tiles, standing out from the wall (never on a gate or an arch).
        for tx, ty, side in ((168, 150, 'NW'), (660, 150, 'NE'), (168, 616, 'SW'), (660, 616, 'SE'), (270, 150, 'N'),
                             (380, 150, 'N'), (580, 150, 'N'), (660, 250, 'E'), (660, 460, 'E'), (660, 540, 'E'),
                             (400, 616, 'S'), (500, 616, 'S'), (240, 616, 'S')):
            m = box(tx - 2, tx + 5, ty - 2, ty + 5) & ~self.inside
            c.paint(m, '#', 2.0)
        spans = {'N': box(474, 479, 146, 154), 'E': box(660, 667, 358, 363), 'S': box(598, 603, 616, 623),
                 'cinder': box(288, 293, 616, 623), 'harbour': box(195, 200, 616, 623)}
        outside_pt = {'N': (476.5, 141.0), 'E': (671.0, 360.5), 'S': (600.5, 627.0), 'cinder': (290.5, 627.0),
                      'harbour': (197.5, 627.0)}
        levels = {'N': 2.0, 'E': 2.0, 'S': 3.0, 'cinder': 1.0, 'harbour': .5}
        self.gate_masks = {}
        for key, m in spans.items():
            m = m & (band | ~self.inside)
            c.paint(m, 'G', levels[key])
            w.gates[('ridgemere', key)] = outside_pt[key]
            w.gate_levels[('ridgemere', key)] = levels[key]
            self.gate_masks[key] = m
        self.arches = {}
        for y in PIERS:                          # The sea wall's pier arches, at the wharf's height.
            m = box(168, 172, y, y + 6)
            c.paint(m, 'G', .5)
            self.arches[y] = m
        slip = box(150, 172, 574, 586)           # The slipway: an opening in the sea wall, a ramp into the water.
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
            ch = {'market': '_', 'rag': 'D', 'records': 'f', 'palace': 'f', 'greatly': '_'}[key]
            c.paint(m, ch, None)
            self.street |= m
            self.spots[f'square_{key}'] = (sx, sy)
        avenues = {
            'High Street': [[(662, 360.5), (364, 360.5)]],
            'Gate Street': [[(476.5, 146), (476.5, 316)]],
            'Crown Street': [[(468.5, 376), (468.5, 497.5)], [(468.5, 497.5), (414, 497.5)], [(400.5, 509), (400.5, 534)]],
            'Keep Street': [[(494, 374.5), (610.5, 374.5)], [(610.5, 374.5), (610.5, 446)]],
            'Rise Lane': [[(610.5, 420.5), (574.5, 420.5)], [(574.5, 420.5), (574.5, 505.5)], [(574.5, 505.5), (600.5, 505.5)],
                          [(600.5, 505.5), (600.5, 621)]],
            'Abbey Row': [[(330, 195.5), (476.5, 195.5)]],
            'Greatly Way': [[(600.5, 271), (600.5, 360.5)]],
        }
        self.avenue_lines = avenues
        for name, lines in avenues.items():
            for pts in lines:
                self.paint_line(pts, 5, '_', None, within=inside_open | self.gate_masks.get('N', box(0, 0, 0, 0))
                                | self.gate_masks['E'] | self.gate_masks['S'])
        for m in (box(438, 507, 312, 322), box(438, 446, 312, 344), box(494, 507, 312, 344)):
            c.paint(m, '_', None)                # About the Market Hall.
            self.street |= m
        # The inner ring, 3 wide (north, east and south; on the west the Wharf Road), and the 3-wide links.
        for m in (box(330, 540, 154, 157), box(657, 660, 270, 616), box(320, 660, 613, 616)):
            m &= inside_open
            c.paint(m, '_', None)
            self.street |= m
        lanes3 = {'Records Lane': [(416, 500.5), (576, 500.5)], "Clerks' Row": [(262, 474.5), (385.5, 474.5), (385.5, 489)],
                  'Cinder Lane': [(222, 500.5), (264, 500.5)], 'Cinder Road': [(290.5, 621), (290.5, 528)]}
        for name, pts in lanes3.items():
            self.paint_line(pts, 3, '_', None, within=inside_open | self.gate_masks['cinder'])
        for m in (box(196, 199, 154, 472), box(196, 222, 469, 472), box(196, 222, 497, 500), box(196, 199, 500, 623)):
            m &= inside_open | self.gate_masks['harbour']
            c.paint(m, '_', None)                # The Wharf Road, round the Customs House by the quay.
            self.street |= m
        self.quay = box(218, 222, 154, 616) & ~self.rings
        self.street |= self.quay
        for y in PIERS:                          # From each pier's arch across the wharf to the quay.
            m = box(168, 218, y + 1, y + 5)
            c.paint(m & ~self.arches[y], '_', None)
            self.street |= m
        rng = self.rng
        gut = [(366.0, 359.5)]                   # The Gut and Tarry Lane: 3 wide, crooked.
        x, y = 366.0, 359.5
        while x > 230:
            x -= 12
            y = y + rng.uniform(-3, 3) - 1.2
            gut.append((max(226.0, x), y))
        gut += [(222.0, 341.0), (216.0, 338.5)]
        self.gut_line = gut
        self.paint_line(gut, 3, '.', None, within=self.D('sump') | self.D('wharf') | self.D('commons'))
        tarry = [(300.5, 232.0)]
        y = 232.0
        while y < 458:
            y += 12
            tarry.append((292.5 + rng.uniform(-3, 3), min(462.0, y)))
        tarry += [(292.5, 470.0), (292.5, 476.0)]
        self.tarry_line = tarry
        self.paint_line(tarry, 3, '.', None, within=(self.D('sump') | self.D('admin')))
        # The abbey's gates (the south a half step down onto Tarry Lane), Greatly's (a half step down onto Greatly
        # Square), the Old Keep's in its north wall facing Keep Street.
        c.paint(box(298, 303, 230, 232), 'G', 1.5)
        c.paint(box(298, 303, 232, 233), '^', 1.0)
        self.street |= box(298, 303, 232, 236)
        c.paint(box(328, 330, 193, 198), 'G', 2.0)
        c.paint(box(598, 603, 268, 270), 'G', 2.5)
        c.paint(box(598, 603, 270, 271), '^', 2.0)
        self.street |= box(598, 603, 270, 274)
        c.paint(box(608, 613, 445, 448), 'G', 2.0)
        self.gate_spots = {'abbey_south': (300, 228), 'abbey_east': (326, 195), 'greatly': (600, 264),
                           'keep': (610, 451)}

    def tier_edges(self):
        """The retaining walls between tiers: the higher of two neighbouring open tiles a whole step or more apart."""
        lvl = np.where(np.isnan(self.levels_map), -9, self.levels_map)
        solid = SOLID[self.c.codes] | self.rings | self.wall_band
        open_ = self.inside & ~solid
        edge = np.zeros((H, W), dtype=bool)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n_l = np.roll(lvl, (dy, dx), (0, 1))
            n_o = np.roll(open_, (dy, dx), (0, 1))
            edge |= open_ & n_o & (lvl - n_l >= 1)
        return edge

    def paint_edges(self):
        stairs = self.edges & self.street
        self.c.paint(self.edges & ~stairs, '#', None)
        self.c.paint(stairs, '^', None)
        self.stairs_mask = stairs

    def ground_of(self, name):
        return self.D(name) & ~self.reserved & ~self.street & ~self.built

    def lanes_in(self, ground, spacing, width, angle, ch):
        lanes = self.lanes(ground, spacing, width, angle=angle) & ground & ~grow(self.built, 1)
        self.c.paint(lanes, ch, None)
        self.street |= lanes
        return lanes

    def fill(self, name, specs, spacing=None, width=3, angle=-.08, lane_ch='_', ground=None, jitter=8, big=130):
        """A district's buildings: the large ones first, each on an avenue; then lanes are cut between them and the
        rest fill in along lanes and avenues. Returns the ground left over."""
        ground = self.ground_of(name) if ground is None else ground
        large = sorted([s for s in specs if s.area >= big], key=lambda s: -s.area)
        small = sorted([s for s in specs if s.area < big], key=lambda s: -s.area)
        lots = Lots(ground & ~self.street, self.street)
        self.place_list(lots, large, self.street, jitter=jitter)
        if spacing:
            self.lanes_in(ground & ~self.built & ~self.street, spacing, width, angle, lane_ch)
        lots = Lots(ground & ~self.street & ~grow(self.built, 1), self.street)
        self.place_list(lots, small, self.street, jitter=jitter)
        return lots.open & ground & ~self.street

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

    def random_point(self, district):
        ys, xs = np.nonzero(self.D(district))
        i = self.rng.randrange(len(xs))
        return float(xs[i]), float(ys[i])

    # -- the wharf ---------------------------------------------------------------------------------------------------
    def waterfront_map(self):
        """Piers out from the arches, cranes at their ends (doc 67, section 2)."""
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
        self.spots['sump_stairs'] = (179, 339)
        self.spots['customs_pier'] = (178, 473)

    def wharf(self):
        specs = [self.reuse_spec(g) for g in (
            'rm_the_sailors_rest', 'rm_the_fish_hall', 'rm_the_wharf_tannery', 'rm_the_ropewalk',
            'rm_the_greatly_bonded_warehouse', 'rm_the_fell_timber_store', 'rm_the_north_wharf_smokehouse',
            'rm_the_drowned_lantern', 'rm_the_tarred_rope', 'rm_wickham_s_chandlery', 'rm_the_bung_hole',
            'rm_the_harbourmaster_s_office', 'rm_the_barracks')]
        specs.append(Spec('r2_new_store', 'The Grayrock New Store', 'wharf', 'store', near=(185, 470),
                          make=lambda: self.new_hall('warehouse', 'The Grayrock New Store', 'Grayrock\'s newest store '
                                                     'on the wharf: bales, casks and crates stacked to the beams, '
                                                     'tallied twice and guarded.', 14, 24), claims=['house_grayrock']))
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
                                                'on the Customs Pier.'], 'civic', footprint=(18, 42)))
        self.put(customs, 176, 476, 'E')
        yard = Spec('r2_grayrock_shipwrights', 'Grayrock Shipwrights', 'wharf', 'works', business='shipwright',
                    claims=['house_grayrock'],
                    make=lambda: self.new_works('shipwright', 'Grayrock Shipwrights', 'House Grayrock\'s yard at the '
                                                'slipway: hulls are built and mended here and run down into the bay.', 5,
                                                footprint=(14, 20)))
        self.put(yard, 176, 566, 'E')
        ground = self.ground_of('wharf') & ~self.quay & ~box(172, 176, 150, 620) & ~box(172, 200, 560, 616)
        for y in PIERS:
            ground &= ~box(168, 182, y - 1, y + 7)
        lots = Lots(ground, self.street)
        self.place_list(lots, sorted(specs, key=lambda s: -s.area), self.street, jitter=4)
        self.sheds_in(lots, 18, 'wharf')
        self.leftover(lots.open & ground, 'f', extra=(('|', .03), ('x', .01), ('O', .01), ('<', .006)))

    def reuse_spec(self, gid, district=None, near=None, kind=None, homes=None):
        dist, k, anchor = REUSE.get(gid, (district, kind, near))
        g = self.structures[gid]
        style = 'manor' if k in ('seat', 'villa') else 'works' if dist == 'cinders' and k == 'works' else 'ridgemere'
        if homes is None:
            homes = {'home': 1, 'lodging': 1, 'shop': 1, 'villa': 2, 'seat': 2}.get(k, 0)
        return Spec(gid, g['name'], district or dist, kind or k, reuse=g, near=near or anchor, style=style, homes=homes,
                    business='stables' if 'stables' in gid else None)

    # -- the abbey and cathedral close ----------------------------------------------------------------------------
    def abbey(self):
        c = self.c
        inner = box(224, 328, 154, 230)
        nave, crypt, pairs = cathedral(self.new_rng, 'ridgemere')
        cat = Spec('r2_cathedral', 'The Cathedral of Ridgemere', 'abbey', 'hall', claims=['council_of_houses'],
                   make=lambda: self.multi('hall', 'The Cathedral of Ridgemere', [nave, crypt], pairs, ['', 'crypt'], [0, -1],
                                           ['The Cathedral of Ridgemere', 'The Cathedral of Ridgemere, the crypt'],
                                           ['The cathedral of the abbey: a great nave of soot-dark stone, pillared '
                                            'aisles, rain on the high windows, the altar and the saint\'s statue at its '
                                            'head.', 'The crypt under the nave: the city\'s old dead in their niches, '
                                            'and the cold of the hill.'], 'ridgemere', footprint=(40, 22)))
        self.put(cat, 236, 157, 'S')
        # The cloister garth: lawn inside an arcade of pillars, south of the cathedral.
        garth = box(256, 278, 186, 206)
        c.paint(garth, ',', 2.0)
        edge = garth & ~box(257, 277, 187, 205)
        ys, xs = np.nonzero(edge)
        pillars = ((xs + ys) % 3 == 0)
        c.codes[ys[pillars], xs[pillars]] = ord('I')
        self.spots['cloister'] = (267, 196)
        # Paths: round the garth, along the cathedral's front, to each gate.
        paths = (box(252, 282, 182, 186) | box(252, 282, 206, 210) | box(252, 256, 182, 210) | box(278, 282, 182, 210)
                 | box(226, 326, 179, 182) | box(298, 303, 210, 230) | box(282, 328, 193, 198))
        paths &= inner
        c.paint(paths, 'f', 2.0)
        self.street |= paths
        S = 'ridgemere'
        specs = [
            Spec('r2_chapter_house', 'The Chapter House', 'abbey', 'hall', near=(240, 195),
                 make=lambda: self.new_hall('hearing', 'The Chapter House', 'The abbey\'s chapter house: stone benches '
                                            'round the walls, the abbot\'s chair, and the rule read aloud each morning.',
                                            16, 12)),
            Spec('r2_abbey_quarters', 'Abbey Quarters', 'abbey', 'lodging', near=(240, 220), homes=1,
                 make=lambda: self.one('house', 'Abbey Quarters', dorter(self.new_rng, S, 12, 20, 12),
                                       'The dorter: a long dormitory of cells, a pallet and a peg in each, where the '
                                       'brothers and sisters of the abbey sleep.', S)),
            Spec('r2_abbey_refectory', 'The Refectory', 'abbey', 'hall', near=(300, 200),
                 make=lambda: self.new_hall('refectory', 'The Refectory', 'The abbey\'s refectory: long tables, a reader\'s '
                                            'lectern and the smell of the day\'s pottage.', 22, 14)),
            Spec('r2_abbey_scriptorium', 'The Scriptorium', 'abbey', 'hall', near=(300, 220),
                 make=lambda: self.new_hall('reading', 'The Scriptorium', 'Desks under the windows where the abbey\'s '
                                            'scribes copy and bind, and shelves of what they have made.', 18, 12)),
            Spec('r2_abbey_brewhouse', 'The Abbey Brewhouse', 'abbey', 'works', near=(315, 215), business='brewery',
                 make=lambda: self.new_works('brewery', 'The Abbey Brewhouse', 'The abbey\'s brewhouse: the brothers\' '
                                             'ale, brewed by the cellarer.', 2, style=S)),
            Spec('r2_abbey_school', 'The Abbey School', 'abbey', 'hall', near=(240, 210),
                 make=lambda: self.new_hall('reading', 'The Abbey School', 'Benches and slates where the abbey teaches the '
                                            'Sump\'s and the Commons\' children their letters.', 18, 11)),
            Spec('r2_abbey_infirmary', 'The Abbey Infirmary', 'abbey', 'hall', near=(315, 190),
                 make=lambda: self.new_hall('healer', 'The Abbey Infirmary', 'The abbey\'s own infirmary: clean beds, '
                                            'herbs drying and a brother on watch.', 18, 10)),
        ]
        ground = inner & ~self.built & ~grow(self.built, 1) & ~garth & ~self.street
        lots = Lots(ground, self.street)
        self.place_list(lots, sorted(specs, key=lambda s: -s.area), self.street, jitter=4)
        self.leftover(lots.open & ground, ',', extra=(('3', .02), ('Y', .01), ('B', .02)))

    # -- House Greatly's compound -------------------------------------------------------------------------------------
    def greatly_compound(self):
        c = self.c
        inner = box(542, 660, 154, 268)
        north = box(542, 660, 154, 194)
        c.paint(north & inner, 'f', 3.0)
        lanes = box(544, 658, 174, 178) | box(574, 579, 174, 222) | box(614, 619, 174, 222) | box(544, 658, 216, 222)
        lanes &= inner
        c.paint(lanes, 'f', 3.0)
        drive = box(598, 603, 213, 268)
        court = box(590, 611, 236, 255)
        c.paint(drive | court, 'd', 3.0)
        c.paint(box(598, 603, 243, 248), 'F', 3.0)           # The fountain in the front yard.
        path = box(603, 630, 231, 235)
        c.paint(path, 'd', 3.0)
        self.street |= lanes | drive | court | path
        self.spots['greatly_fountain'] = (597, 245)
        forge = Spec('r2_greatly_forge', 'The Greatly Forge & Workshop', 'greatly', 'works', business='builders_works',
                     claims=['house_greatly'],
                     make=lambda: self.new_works('builders_works', 'The Greatly Forge & Workshop', 'House Greatly\'s own '
                                                 'works behind the house, for its building trade alone: cramps, nails and '
                                                 'hinges, roof timbers and doors. (The city\'s forges are Timberland\'s.)',
                                                 3, footprint=(24, 14)))
        self.put(forge, 546, 159, 'S')
        house = self.reuse_spec('rm_greatly_house')
        self.put(house, 590, 202, 'S', 19, 11)
        self.put(self.reuse_spec('rm_the_greatly_masons_yard'), 580, 160, 'S', 14, 9)
        self.put(self.reuse_spec('rm_the_greatly_colour_works'), 600, 160, 'S', 13, 7)
        self.put(self.reuse_spec('rm_the_greatly_stables'), 622, 160, 'S', 13, 6)
        self.put(self.reuse_spec('rm_the_greatly_counting_house'), 630, 228, 'W', 14, 7)
        ground = inner & ~self.built & ~self.street & ~north
        self.gardens_plain(ground, 'greatly')
        self.posts['greatly_gardens'] = [(570, 245), (630, 255)]
        self.posts['greatly_gate'] = [(600, 264)]

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
                                             footprint=(20, 20)))
        self.put(tower, 590, 462, 'N')
        barracks = self.reuse_spec('rm_the_watch_house')
        barracks.claims = ['council_of_houses', 'ridgemere_watch']
        self.put(barracks, 624, 458, 'W', 10, 18)
        court = box(583, 637, 448, 497) & ~self.built & ~self.rings
        self.street |= court
        self.posts['keep_yard'] = [(600, 452), (615, 490), (586, 490)]

    # -- the Great Palace ---------------------------------------------------------------------------------------------
    def palace(self):
        c = self.c
        rp, rpairs = palace_residence(self.new_rng, 'palace', 48, 24, 3, 6)
        residence = Spec('r2_kings_residence', "The King's Residence", 'palace', 'seat', homes=2,
                         claims=['council_of_houses'],
                         make=lambda: self.multi('seat', "The King's Residence", rp, rpairs, ['', 'up', 'down'], [0, 1, -1],
                                                 ["The King's Residence", "The King's Residence, the royal bedchambers",
                                                  "The King's Residence, the kitchens"],
                                                 ['The palace\'s west wing, the King\'s Residence: receiving rooms hung '
                                                  'with the Crown\'s banners, looking west over a garden wall at the '
                                                  'Cinders\' smoke.', 'The royal bedchambers above: curtained beds, '
                                                  'fires in every room and rain on tall windows.',
                                                  'The palace kitchens and the servants\' hall beneath the Residence, '
                                                  'warm and never quiet.'], 'palace', footprint=(28, 60)))
        ante = Spec('r2_palace_antechamber', 'The Antechamber', 'palace', 'hall', claims=['council_of_houses'],
                    make=lambda: self.one('hall', 'The Antechamber', palace_antechamber(self.new_rng, 'palace', 40, 24),
                                          'The Great Palace\'s antechamber, its court and senate: tiered benches under '
                                          'the four Houses\' banners where the Council of Houses sits, the Chancellor\'s '
                                          'dais, and the great door on the forecourt.', 'palace', footprint=(28, 40)))
        wp, wpairs = palace_wing(self.new_rng, 'palace', 48, 24, 8)
        wing = Spec('r2_administrative_wing', 'The Administrative Wing', 'palace', 'hall', claims=['council_of_houses'],
                    make=lambda: self.multi('hall', 'The Administrative Wing', wp, wpairs, ['', 'up'], [0, 1],
                                            ['The Administrative Wing', 'The Administrative Wing, upstairs'],
                                            ['The palace\'s east wing: the royal secretary\'s and the treasurer\'s '
                                             'offices, clerks at long desks and the Crown\'s papers in presses.',
                                             'More desks under the eaves, and the Crown\'s rolls in their cabinets.'],
                                            'palace', footprint=(28, 60)))
        # (Footprints are given door edge first, so each block is 60 or 40 along the forecourt and 28 deep.)
        residence.building.footprint = (60, 28)
        ante.building.footprint = (40, 28)
        wing.building.footprint = (60, 28)
        self.put(residence, 350, 552, 'N')
        self.put(ante, 410, 552, 'N')
        self.put(wing, 450, 552, 'N')
        ground = lambda key: self.named[key]['rooms'][0]['id']
        self.passage(ground('r2_kings_residence'), 'E', ground('r2_palace_antechamber'), 'W',
                     "The King's Residence to the Antechamber")
        self.passage(ground('r2_palace_antechamber'), 'E', ground('r2_administrative_wing'), 'W',
                     'The Antechamber to the Administrative Wing')
        # The privy garden behind, walled from the Cinders: lawns, hedges and a fountain (no crops).
        garden = box(322, 540, 584, 612)
        paths = box(340, 348, 534, 612) | box(512, 520, 534, 612) | box(340, 520, 582, 586)
        paths &= self.D('palace') & ~self.built
        c.paint(paths, 'f', 2.0)
        self.street |= paths
        g = garden & ~paths & ~self.built & ~self.reserved
        self.gardens_plain(g, 'palace')
        c.paint(box(428, 433, 596, 601), 'F', 2.0)
        self.posts['palace_gardens'] = [(430, 590), (380, 600)]
        rest = self.D('palace') & ~self.built & ~self.street & ~self.reserved & ~garden
        self.leftover(rest, 'f', extra=(('I', .004), ('S', .002)))

    def passage(self, a, side_a, b, side_b, name):
        """A passage link between two interiors that touch (a door knocked through): a doorway carved in each one's wall
        on the side toward the other, so the room's floor isn't a trap."""
        ra, rb = self.site_room(a), self.site_room(b)
        ea, eb = self.carve(ra, side_a), self.carve(rb, side_b)
        if ea is None or eb is None:
            return False
        self.site.links.append({'id': f'{a}_to_{b}', 'name': name, 'kind': 'passage', 'open': True,
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
        keepers = []
        for x in (452, 458, 479, 485):                  # The north row of stalls: four two-tile stalls.
            c.stamp(x, 351, ['uu'], None)
            keepers.append((x, 350))
        for x, row in ((452, 'uu'), (460, 'u'), (479, 'u'), (485, 'uu')):   # The south row: two double, two single.
            c.stamp(x, 369, [row], None)
            keepers.append((x, 370))
        c.stamp(470, 349, ['U'], None)                   # The well.
        self.posts['market_square'] = keepers
        self.posts['market_porters'] = [(447, 346), (493, 346), (447, 374), (493, 374)]
        hall_plan = market_hall(self.new_rng, 'ridgemere', 20, 48, 6)
        hall_ = Spec('r2_market_hall', 'The Market Hall', 'commons', 'hall', claims=['council_of_houses'],
                     make=lambda: self.one('hall', 'The Market Hall', hall_plan, 'The covered Market Hall on the north side '
                                           'of Market Square: six counters under a slate roof on oak posts, out of the '
                                           'rain, a door at each end.', 'ridgemere', footprint=(20, 48)))
        self.put(hall_, 446, 322, 'W')
        # The Rag Market in the Sump: six makeshift stalls round its edges, a pump against a wall.
        stalls = [((280, 343), (280, 344)), ((288, 343), (288, 344)), ((296, 343), (296, 344)),
                  ((280, 357), (280, 356)), ((288, 357), (288, 356)), ((298, 357), (298, 356))]
        rag = []
        for (sx, sy), keeper in stalls:
            c.stamp(sx, sy, ['u'], None)
            rag.append(keeper)
        c.stamp(302, 345, ['U'], None)
        self.posts['rag_market'] = rag
        self.posts['tray_sellers'] = [(284, 350), (294, 350), (300, 352), (286, 354)]

    # -- the administrative sector ------------------------------------------------------------------------------------
    def admin(self):
        c = self.c
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
        self.put(records, 388, 472, 'S')
        specs = [self.reuse_spec(g) for g in ('rm_the_city_office', 'rm_the_magistrate_s_court', 'rm_the_hall_of_records',
                                               'rm_wetink_lane_scriveners', 'rm_the_slate_ledger')]
        specs[0].near, specs[1].near = (440, 505), (362, 500)
        for i, gid in enumerate(self.houses.get('admin', [])):
            specs.append(self.reuse_spec(gid, 'admin', (300 + 30 * (i % 8), 515), 'home'))
        left = self.fill('admin', specs, spacing=(28, 21), angle=0)
        # The civic style: flagstones, and arcades of pillars along the streets.
        arcade = left & grow(self.street, 1) & ~self.reserved
        ys, xs = np.nonzero(arcade)
        pick = (xs + ys) % 4 == 0
        c.codes[ys[pick], xs[pick]] = ord('I')
        self.leftover(left & ~arcade, 'f', extra=(('S', .002), ('I', .002), ('B', .01)))

    # -- the Commons and Northgate ------------------------------------------------------------------------------------
    def commons(self):
        S = 'ridgemere'
        com, north = [], []
        for gid, (dist, kind, anchor) in REUSE.items():
            if dist in ('commons', 'northgate'):
                (com if dist == 'commons' else north).append(self.reuse_spec(gid))
        for i, gid in enumerate(self.houses.get('commons', [])):
            com.append(self.reuse_spec(gid, 'commons', None, 'home'))
        for i, gid in enumerate(self.houses.get('northgate', [])):
            north.append(self.reuse_spec(gid, 'northgate', None, 'home'))
        prose = 'A tall narrow house of the Commons shared by two families, one up and one down, a stair between.'
        for i in range(10):
            name = DOUBLE_NAMES[i]
            com.append(Spec(f'r2_double_{i}', name, 'commons', 'flats', homes=2, open_door=False,
                            make=lambda name=name: self.new_flats(name, 2, (3, 3), (14, 12), (10, 8), prose=prose)))
        com += [
            Spec('r2_barber', "The Barber's Pole", 'commons', 'shop', homes=1, near=(390, 340), business='barber',
                 make=lambda: self.new_shop('apothecary', "The Barber's Pole", 'A barber-surgeon\'s: a chair by the '
                                            'window, razors and lancets laid out, and a striped pole by the door.')),
            Spec('r2_fell_wine_vaults', 'The Fell Wine Vaults', 'commons', 'shop', homes=1, near=(540, 360),
                 business='vintner', claims=['house_fell'],
                 make=lambda: self.new_shop('provisioner', 'The Fell Wine Vaults', 'House Fell\'s wine merchant in the '
                                            'city: casks of Fellhall wine and brandy racked to the ceiling, cordials '
                                            'and preserves on the counter.')),
            Spec('r2_cheesemonger', 'The Round and Rind', 'commons', 'shop', homes=1, near=(430, 400), business='dairy',
                 make=lambda: self.new_shop('provisioner', 'The Round and Rind', 'A cheesemonger\'s: rounds ripening '
                                            'on cool shelves, butter in crocks and a wire for cutting.')),
            Spec('r2_bathhouse', 'The Bathhouse', 'commons', 'works', near=(372, 420), business='bathhouse',
                 make=lambda: self.new_works('bathhouse', 'The Bathhouse', 'The Commons\' bathhouse by the Sump\'s edge: '
                                             'a penny for the hot pool, two for a towel.', 3, style=S)),
        ]
        for s in com + north:
            if s.near is None:
                s.near = self.random_point(s.district)
        left = self.fill('commons', com, spacing=(26, 19), angle=-.06)
        self.leftover(left, ',', extra=(('Y', .012), ('B', .015), ('3', .01), ('U', .0008)))
        left = self.fill('northgate', north, spacing=(26, 19), angle=-.06)
        self.leftover(left, ',', extra=(('Y', .012), ('B', .015), ('3', .01)))

    # -- the Cinders -------------------------------------------------------------------------------------------------
    def cinders(self):
        specs = [self.reuse_spec(g) for g, (d, k, a) in REUSE.items() if d == 'cinders']
        for i, gid in enumerate(self.houses.get('cinders', [])):
            specs.append(self.reuse_spec(gid, 'cinders', (230 + 12 * i, 610), 'home'))
        specs += [
            Spec('r2_forge_row', "Timberland's Forge Row", 'cinders', 'shop', homes=1, near=(300, 520),
                 business='smithy', claims=['house_mossgrave'],
                 make=lambda: self.new_shop('smith', "Timberland's Forge Row", 'A smithy of House Timberland\'s on the '
                                            'Cinders: horseshoes, hinges and cart iron, the forge never let out.',
                                            style='works')),
            Spec('r2_cinder_wheelwrights', 'Cinder Wheelwrights', 'cinders', 'works', near=(250, 610),
                 business='cartwright',
                 make=lambda: self.new_works('cartwright', 'Cinder Wheelwrights', 'Wheels and axles for the city\'s carts '
                                             'and the Houses\' wagons.', 2, size=(19, 14))),
        ]
        left = self.fill('cinders', specs, spacing=(34, 26), angle=.05, lane_ch='_', big=90)
        lots = Lots(left, self.street)
        self.sheds_in(lots, 16, 'cinders')
        self.leftover(lots.open & left, '>', extra=(('x', .015), ('O', .01), ('<', .012), ('7', .004)))

    # -- the Rise -------------------------------------------------------------------------------------------------------
    def rise(self):
        """Five walled mansions of the Minor Houses, their gates on Rise Lane (doc 67, R15)."""
        c = self.c
        lots = [((548, 508), 'E'), ((606, 508), 'W'), ((548, 543), 'E'), ((606, 543), 'W'), ((548, 578), 'E')]
        for ((x0, y0), side), gid in zip(lots, MINOR_HOUSES):
            lw, lh = 44, 30
            region = box(x0, x0 + lw, y0, y0 + lh)
            ring = region & ~box(x0 + 1, x0 + lw - 1, y0 + 1, y0 + lh - 1)
            c.paint(region, ',', 3.0)
            c.paint(ring, 'H', 3.0)
            self.rings |= ring
            gy = y0 + lh // 2
            gx = x0 + lw - 1 if side == 'E' else x0
            c.paint(box(gx, gx + 1, gy - 1, gy + 2), 'G', 3.0)
            # A path from the gate to the house door, and out to Rise Lane.
            hx0 = x0 + 15
            hy0 = y0 + 11
            out = box(gx + 1, 598, gy - 1, gy + 2) if side == 'E' else box(603, gx, gy - 1, gy + 2)
            inner_path = box(hx0 + 14, gx, gy - 1, gy + 2) if side == 'E' else box(gx + 1, hx0, gy - 1, gy + 2)
            c.paint(out | inner_path, 'f', 3.0)
            self.street |= out | inner_path
            spec = self.reuse_spec(gid, 'rise', None, 'villa')
            # The house's block: 14 x 8 (fp of its 20 x 14 interior), its door toward the gate.
            if side == 'E':
                self.put(spec, hx0, hy0, 'E', 14, 8)
            else:
                self.put(spec, x0 + lw - 15 - 14, hy0, 'W', 14, 8)
            garden = region & ~ring & ~self.built & ~self.street
            self.gardens_plain(garden, gid)
            fx, fy = (x0 + 8, y0 + 6) if side == 'E' else (x0 + lw - 9, y0 + 6)
            c.paint(box(fx, fx + 2, fy, fy + 2), 'F', 3.0)
            self.posts.setdefault('mansion_gardens', []).append((x0 + lw // 2, y0 + 4))
        rest = self.D('rise') & ~self.built & ~self.street & ~self.reserved & ~box(548, 650, 508, 608)
        self.leftover(rest, ',', extra=(('Y', .02), ('B', .02), ('3', .02), ('S', .002)))
        park = box(606, 650, 578, 608) & ~self.street
        self.gardens_plain(park, 'rise_green')

    # -- the Sump ---------------------------------------------------------------------------------------------------------
    def sump(self):
        """The slums by the wharf (doc 67, section 2, "The Sump in detail"): its businesses along the Gut and Tarry Lane
        first; then a crooked grid of alleys one and two tiles wide, cut for dead ends and bent into doglegs; courts in
        the bigger blocks reached by snickets; flats and cottages packed along the alleys, terraced or a snicket apart;
        and whatever is left filled with sheds, boarded-up houses and walled yards."""
        c, rng = self.c, self.rng
        S = self.D('sump')
        self.infill.update({'boarded': 0, 'yards': 0, 'courts': 0, 'dead_ends': 0})
        # 1. Its businesses, on the through-lanes and the Rag Market.
        named = [self.reuse_spec(g) for g, (d, k, a) in REUSE.items() if d == 'sump']
        named += [
            Spec('r2_the_washhouse', 'The Washhouse', 'sump', 'works', near=(250, 400), business='laundry',
                 make=lambda: self.new_works('washhouse', 'The Washhouse', 'Where the Sump and half the Commons send '
                                             'their washing: a penny a bundle, back by dark, still damp.', 6,
                                             style='ridgemere', size=(22, 13))),
            Spec('r2_rag_and_bone_yard', 'The Rag-and-Bone Yard', 'sump', 'works', near=(255, 440),
                 business='rag_and_bone',
                 make=lambda: self.new_works('rag_and_bone', 'The Rag-and-Bone Yard', 'Old Nan Tarrow buys anything, '
                                             'sells most of it back, and knows where every pot in the Sump came from.',
                                             2, style='ridgemere', size=(16, 12))),
            Spec('r2_lamp_store', 'The Lamp Store', 'sump', 'hall', near=(320, 300),
                 make=lambda: self.new_hall('warehouse', 'The Lamp Store', 'The lamplighters\' post: ladders, wicks, '
                                            'casks of fish oil and the city\'s spare lanterns.', 12, 9)),
            Spec('r2_the_knucklebones', 'The Knucklebones', 'sump', 'tavern', near=(330, 410),
                 make=lambda: self.new_tavern('The Knucklebones', 'A dice-and-card tavern: low lamps over every table, '
                                              'the house\'s cut on every pot, and nobody asks where the stake came from.')),
        ]
        lanes = (self.street & S) | (self.quay & grow(S, 2))
        ground = S & ~self.street & ~self.reserved & ~self.built
        lots = Lots(ground, lanes)
        self.place_list(lots, sorted(named, key=lambda s: -s.area), lanes, jitter=3, deep=True)
        # 2. The alleys: a jittered grid, 9-16 apart.
        xs, x = [222], 222
        while x < 350:
            x += rng.randint(9, 16)
            xs.append(min(x, 362))
        ys, y = [233], 233
        while y < 456:
            y += rng.randint(9, 16)
            ys.append(min(y, 468))
        # (Nodes on tile corners: a line two wide along them covers two tiles; a one-wide line is moved to the
        # tiles' middles.)
        node = {(i, j): (float(xs[i] + (rng.randint(-2, 2) if 0 < i < len(xs) - 1 else 0)),
                         float(ys[j] + (rng.randint(-2, 2) if 0 < j < len(ys) - 1 else 0)))
                for i in range(len(xs)) for j in range(len(ys))}
        segments = [((i, j), (i, j + 1)) for i in range(len(xs)) for j in range(len(ys) - 1)] + \
                   [((i, j), (i + 1, j)) for i in range(len(xs) - 1) for j in range(len(ys))]
        rng.shuffle(segments)
        alleys = np.zeros((H, W), dtype=bool)
        narrow = np.zeros((H, W), dtype=bool)
        stubs = []
        free = S & ~self.built & ~self.reserved
        for a, b in segments:
            pa, pb = node[a], node[b]
            r = rng.random()
            if r < .25:                                       # Cut: the blocks either side merge...
                if rng.random() < .5:                         # ...or half of it stays, a dead end.
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
                if a[0] == b[0]:                              # Vertical: step sideways halfway down.
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
        # 3. Courts: small yards inside the biggest blocks, each reached by a one-tile snicket.
        open_ = (alleys | (self.street & S)) & ~self.built
        self.blocks_mask, blocks = label(S & ~open_ & ~self.built & ~self.reserved)
        courts = np.zeros((H, W), dtype=bool)
        snickets = np.zeros((H, W), dtype=bool)
        made = 0
        for lab, (bx0, bx1, by0, by1, area) in sorted(blocks.items(), key=lambda kv: -kv[1][4]):
            if made >= 8:
                break
            bw, bh = bx1 - bx0, by1 - by0
            if bw < 12 or bh < 11:
                continue
            cw, ch_ = rng.randint(6, min(10, bw - 5)), rng.randint(6, min(8, bh - 4))
            cx0, cy0 = bx0 + (bw - cw) // 2, by0 + (bh - ch_) // 2
            court = box(cx0, cx0 + cw, cy0, cy0 + ch_)
            inner_block = self.blocks_mask == lab
            if not (court & inner_block).sum() == cw * ch_:
                continue
            # The snicket: straight out of the court's west or north side to the nearest alley.
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
        c.paint(courts, 'D', 1.0)
        c.paint(snickets, '.', 1.0)
        self.infill['courts'] = made
        self.court_mask = courts
        self.snickets = snickets
        c.paint(alleys & ~self.built, '.', 1.0)
        self.alleys = alleys | snickets
        open_ = (self.alleys | (self.street & S) | courts) & ~self.built
        self.sump_open_planned = open_.copy()
        # Doors open only onto passages two or more wide: the tile outside and the one beyond it both open, not courts.
        lab_o, _ = label(open_ | (self.quay & grow(S, 3)))
        gx, gy = int(self.gut_line[2][0]), int(self.gut_line[2][1])
        joined = lab_o == lab_o[gy, gx]
        door_street = open_ & joined & ~courts & ~snickets & ~narrow
        # 4. Flats and cottages along the alleys.
        ground = S & ~open_ & ~self.built & ~self.reserved
        lots = Lots(ground, door_street, gap=0)
        homes = []
        flats_sizes = [(7, 6), (8, 6), (8, 7), (9, 7), (10, 7), (10, 8), (11, 8), (12, 8), (12, 9), (13, 9), (14, 10),
                       (15, 10), (16, 12), (18, 14), (7, 12), (6, 11), (7, 13), (6, 13), (9, 11), (8, 10), (11, 10)]
        cottage_sizes = [(5, 5), (6, 5), (6, 6), (7, 5), (7, 6), (8, 6), (9, 7), (5, 7)]
        want = [('flats', rng.choice(flats_sizes)) for _ in range(64)] + \
               [('cottage', rng.choice(cottage_sizes)) for _ in range(36)]
        want.sort(key=lambda t: -t[1][0] * t[1][1])
        for kind, (bw, bd) in want:
            near = self.random_point('sump')
            got = None
            for orient in ((bw, bd), (bd, bw)):
                hit = self.nearest_lot(lots, orient[0], orient[1], near, door_street, deep=True)
                if hit and hit[0] < 40:
                    got = (hit[1], hit[2], hit[3], hit[4], hit[5], orient)
                    break
            if not got:
                continue
            x0, y0, facing, w, h, orient = got
            lots.gap = 0 if rng.random() < .6 else 1
            lots.take(x0, y0, w, h)
            self.built[y0:y0 + h, x0:x0 + w] = True
            homes.append({'kind': kind, 'x0': x0, 'y0': y0, 'w': w, 'h': h, 'facing': facing,
                          'fw': orient[0], 'fh': orient[1]})
        # 5. Infill: what is left inside the blocks.
        self.sump_infill(S, open_)
        # 6. The buildings themselves: back doors where the back opens onto a passage, L-shapes, their homes.
        names = sump_names(rng, len(homes))
        open_now = (S | self.D('wharf')) & ~SOLID[c.codes] & ~self.built
        for i, hm in enumerate(homes):
            x0, y0, w, h, facing = hm['x0'], hm['y0'], hm['w'], hm['h'], hm['facing']
            fw, fh = (w, h) if facing in 'NS' else (h, w)
            (bx, by), (ox, oy) = door_tile(x0, y0, w, h, OPPOSITE[facing])
            back = (0 <= ox < W and 0 <= oy < H and open_now[oy, ox] and cell_of(bx, by) == cell_of(ox, oy)
                    and not self.edges[oy, ox])
            notch = None
            mask = None
            if hm['kind'] == 'flats' and fw >= 8 and fh >= 7 and rng.random() < .3:
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
            if hm['kind'] == 'flats':
                n = 3 if fw * fh >= 90 and rng.random() < .6 else 2
                beds = tuple(rng.choice([2, 2, 3, 3, 3, 4, 5]) for _ in range(n))
            else:
                n, beds = 1, (rng.choice([2, 2, 3]),)
            name = names[i]
            b = self.new_flats(name, n, beds, (iw, ih), (fw, fh), notch=notch, back=back, style='ridgemere')
            spec = Spec(f'sump_home_{i}', name, 'sump', 'flats' if n > 1 else 'home', homes=n, open_door=False)
            spec.building = b
            rec = self.put(spec, x0, y0, facing, mask=mask)
            rec['through'] = bool(back)
            rec['lshape'] = notch is not None
            rec['sump_home'] = True
            hm['record'] = rec
        self.sump_homes = homes
        self.sump_passages(homes)
        self.infill['dead_ends'] = len(stubs)
        self.sump_stubs = stubs
        self.sump_narrow = narrow

    def sump_infill(self, S, open_):
        """Sheds, boarded-up houses and walled back-yards in whatever the buildings left inside the blocks."""
        c, rng = self.c, self.rng
        left = S & ~open_ & ~self.built & ~self.reserved
        lab, comps = label(left)
        for k, (x0, x1, y0, y1, area) in comps.items():
            comp = lab == k
            if area <= 2:
                continue
            # Boarded-up houses, then sheds, wherever a rectangle fits.
            for kind, sizes in (('boarded', [(14, 10), (12, 10), (12, 8), (10, 8), (8, 8)]),
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
            # What is still open: a walled back-yard where it is more than a sliver (fenced off from the alleys).
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
                # Side by side, wall to wall or across a one-tile snicket (a cellar run under it).
                if 0 <= bx0 - (ax0 + aw) <= 3 or 0 <= ax0 - (bx0 + bw) <= 3:
                    overlap = min(ay0 + ah, by0 + bh) - max(ay0, by0)
                    if overlap >= 3:
                        side = 'E' if bx0 >= ax0 + aw else 'W'
                        nbr[i].append((j, side))
                        nbr[j].append((i, OPPOSITE[side]))
                elif 0 <= by0 - (ay0 + ah) <= 3 or 0 <= ay0 - (by0 + bh) <= 3:
                    overlap = min(ax0 + aw, bx0 + bw) - max(ax0, bx0)
                    if overlap >= 3:
                        side = 'S' if by0 >= ay0 + ah else 'N'
                        nbr[i].append((j, side))
                        nbr[j].append((i, OPPOSITE[side]))
        used = set()
        chains = []

        def longest(path, seen):
            """The longest chain (up to five) from the path's last building."""
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
        while len(chains) < 6:                              # The longest chains first.
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
                for z in (1, 2):                            # Upstairs too, where both have the floor.
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
        w.places['ridgemere'] = box(160, 672, 142, 628)
        # The three Houses' estates (doc 67, R26), each a walled compound within one cell: the seat, its gatehouse,
        # stables and cottages; no fields, orchards, vineyards or pens.
        gates = {'mossgrave': ('N', (380, 640)), 'fell': ('W', (770, 366)), 'grayrock': ('N', (190, 640))}
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
        cost = w.route_costs()
        roads = {
            'The North Road': [w.gates[('ridgemere', 'N')], (476.5, 0.0)],
            'The South Road': [w.gates[('ridgemere', 'S')], (600.5, 767.0)],
        }
        for name, pts in roads.items():
            w.road(name, pts, width=3, ends=(w.gate_levels[('ridgemere', 'N' if 'North' in name else 'S')], None))
        fell_gate = w.gates[('estate_fell', 'W')]
        east = [w.gates[('ridgemere', 'E')], (fell_gate[0] - 10, 360.5), fell_gate]
        w.road('The East Road', east, width=3, ends=(2.0, w.gate_levels[('estate_fell', 'W')]))
        on = w.route((fell_gate[0] - 10, 360.5), (1023.0, 330.0), w.route_costs())
        w.road('The East Road, past Fellhall', on, width=3)
        line = w.route(w.gates[('ridgemere', 'cinder')], w.gates[('estate_mossgrave', 'N')], w.route_costs())
        w.road('The Cinder Road', line, width=3, ends=(1.0, w.gate_levels[('estate_mossgrave', 'N')]))
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
            'rm_wall_walk': [(334, 155), (430, 155), (536, 155), (476, 250), (590, 282), (658, 300), (658, 400),
                             (658, 500), (658, 600), (560, 614), (430, 614), (330, 614), (197, 600), (197, 400),
                             (197, 200)],
            'rm_street_patrol': [(640, 360), (560, 360), (470, 356), (470, 380), (468, 440), (440, 497), (400, 520),
                                 (430, 540), (468, 420), (470, 360)],
            'rm_wharf_patrol': [(197, 170), (197, 250), (197, 338), (197, 420), (220, 485), (197, 560), (197, 610)],
            'r2_sump_patrol': [(360, 359), (330, 356), (300, 350), (260, 348), (226, 342), (292, 300), (300, 240),
                               (292, 400), (292, 465)],
            'r2_palace_round': [(375, 536), (430, 538), (485, 536), (485, 549), (430, 550), (375, 549)],
            'rm_grayrock_journey': [(197, 300), (197, 600), self.world.gates[('ridgemere', 'harbour')],
                                    self.spots['estate_grayrock_gate']],
        }


# ---------------------------------------------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------------------------------------------
STYLE_ROOF = {'ridgemere': 'Z', 'manor': 'Z', 'works': 'Z', 'civic': 'Q', 'palace': '['}
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
    """An interior roomier than its block (fp is x0.72 wide, x0.65 deep), within reason."""
    return int(np.clip(round(fw / .72), 7, 20)), int(np.clip(round(fh / .65), 6, 18))


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


def make_city(project, seed=SEED, report=print):
    """The city laid out over a copy of the project's rooms: (project as the plan reads it, world, city)."""
    from . import ridgemere2_people as people
    baseline = baseline_people(project)
    project = strip_city(project)
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
    if x0 < 664 and x0 + CELL > 168 and y0 < 620 and y0 + CELL > 150:
        out |= {'ridgemere_watch', 'council_of_houses'}
    out |= {(2, 0): {'house_greatly'}, (1, 2): {'house_mossgrave'}, (3, 1): {'house_fell'}, (0, 0): {'house_grayrock'},
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


def compose(project, world, city, plan, labels, baseline):
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
    for person in p['people']:
        e = plan.get(person['id'])
        if not e or not e.get('placed'):
            person['home'] = person['work'] = person['evening'] = None
            continue
        person['home'] = e['home']
        person['work'] = e['work']
        person['evening'] = e['evening']
        person['role'] = e['role']
        person['route'] = e.get('route') or ('' if e['role'] == 'merchant' else baseline[person['id']]['route']
                                             if e.get('labelKept') else '')
        if person['role'] == 'merchant':
            person['route'] = ''
        person['workLabel'] = labels[person['id']]
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
def checks(p, world, city, plan, labels, walk):
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
    check('R2', '12 cells, all rain, region ridgemere', len(cells) == 12 and all(x['weather'] == 'rain' and
          x['territory']['region'] == REGION for x in cells), f'{len(cells)} cells')
    placed_rooms = {room['id'] for rec in city.records for room in rec['rooms']}
    check('R2', 'every placed interior has region ridgemere',
          all(rooms[r]['territory']['region'] == REGION for r in placed_rooms), f'{len(placed_rooms)} placed interiors')
    import map_editor
    check('R1', 'no cell overlaps another', not map_editor.overlapping_cells(cells))
    bay = np.zeros((H, W), dtype=bool)
    for y in range(170, 610):
        bay[y, :int(shore_x(y))] = True
    built_over = np.isin(codes, [ord(ch) for ch in '8iI:'])
    check('R4', 'water lies west of x = 168 between the headlands', bool((world.water | built_over)[bay].all()))
    sea = codes[150:620, 168:172]
    check('R5', 'the sea wall runs along the quay (x 168-172)', bool(np.isin(sea, [ord('#'), ord('G'), ord(':')]).all()))
    check('R5', 'the wharf, its 8 piers and arches, the cranes and the slipway lie within the wall\'s span (y 150-620)',
          all(150 <= y and y + 6 <= 620 for y in PIERS) and all(codes[y + 2, 169] == ord('G') for y in PIERS)
          and bool(city.slip.any()) and all(150 <= y <= 620 for y in CRANES))
    wharf_x, sump_x = 221, 222
    open_pairs = sum(1 for y in range(232, 470) if not SOLID[codes[y, wharf_x]] and not SOLID[codes[y, sump_x]]
                     and abs(heights[y, wharf_x] - heights[y, sump_x]) <= .5)
    walled = sum(1 for y in range(232, 470) if codes[y, sump_x] in (ord('#'), ord('H')) and
                 not city.built[y, sump_x])
    check('R6', 'the Sump meets the wharf with no wall between', walled == 0 and open_pairs > 0,
          f'{open_pairs} open crossings, {walled} wall tiles on the line')
    check('R6', 'the Sump holds at least 50% of the wharf frontage', (470 - 232) / (616 - 154) >= .5,
          f'{(470 - 232) / (616 - 154):.0%}')
    seen = walk['seen']
    node = walk['node']

    def reached(x, y):
        return node(cell_id(int(x) // CELL, int(y) // CELL), int(x) % CELL, int(y) % CELL) in seen
    gate_ok = {k: reached(*city.gate_inside[k]) and bool(city.gate_masks[k].any()) for k in GATES}
    arch_ok = {y: reached(176, y + 3) and codes[y + 2, 170] == ord('G') for y in (470, 335)}
    check('R5', 'the 5 gates and the 2 guarded pier arches are cut and reached', all(gate_ok.values()) and all(arch_ok.values()),
          f'gates {gate_ok}, arches {arch_ok}')
    inside = box(*INSIDE)
    check('R10', 'the districts partition the inside of the wall', bool((city.district[inside] > 0).all()))
    area = lambda n: int(city.D(n).sum())
    middle = area('northgate') + area('commons')
    check('R15', "the Rise's area is at most half the middle class's", area('rise') <= .5 * middle,
          f'rise {area("rise")}, middle class {middle} ({area("rise") / middle:.2f})')
    ab = city.D('abbey')
    touches = lambda a, b: bool((grow(a, 1) & b).any())
    check('R20', 'the abbey close is in the north-west corner, touching the Sump and the middle class',
          touches(ab, city.D('sump')) and touches(ab, city.D('northgate')) and
          np.nonzero(ab)[1].min() <= 222 and np.nonzero(ab)[0].min() <= 154)
    kp = city.D('keep')
    kys, kxs = np.nonzero(kp)
    check('R16', 'the Keep is at one height, straddles y = 470, with four walls, a courtyard and one gate',
          len(set(np.unique(city.levels_map[kp]))) == 1 and kys.min() < 470 < kys.max() and
          int((codes[445:448, 583:637] == ord('G')).sum()) > 0 and
          int((codes[kp] == ord('G')).sum()) == int((codes[445:448, 608:613] == ord('G')).sum()))
    gn = codes[150:154, 540:660]
    between = ~SOLID[codes[150:154, 540:660]]
    check('R24', 'the Greatly compound sits against the city wall, no public tile between',
          not between.any() and int(city.D('greatly')[154, 545:655].all()))
    fx, fy = city.spots['greatly_fountain']
    forge = city.named['r2_greatly_forge']['block']
    check('R23', 'the Greatly fountain is in the south yard, the forge and workshop in the north yard',
          220 <= fy <= 268 and forge['y'] + forge['h'] <= 194 and codes[245, 600] == ord('F'))
    check('R2', 'gates joining heights more than 0.5 apart have their half step and ^ run',
          heights[231, 300] == 1.5 and codes[232, 300] == ord('^') and heights[269, 600] == 2.5 and codes[270, 600] == ord('^'))
    seam = np.zeros((H, W), dtype=bool)
    for s in (256, 512, 768):
        seam[:, s - 2:s + 2] = True
    for s in (256, 512):
        seam[s - 2:s + 2, :] = True
    lvl = np.where(np.isnan(city.levels_map), -9, city.levels_map)
    across_x = np.zeros((H, W), dtype=bool)          # an edge whose lower side is east or west: a north-south line
    for dx in (1, -1):
        across_x |= city.edges & (lvl - np.roll(lvl, -dx, 1) >= 1)
    across_y = city.edges & ~across_x
    near_x = np.zeros((H, W), dtype=bool)
    near_y = np.zeros((H, W), dtype=bool)
    for s_ in (256, 512, 768):
        near_x[:, s_ - 2:s_ + 2] = True
    for s_ in (256, 512):
        near_y[s_ - 2:s_ + 2, :] = True
    bad_edges = (across_x & near_x) | (across_y & near_y) | (city.stairs_mask & (near_x | near_y))
    check('R2', 'no tier edge or stair within 2 tiles of a cell seam (an edge may cross a seam at right angles)',
          not bad_edges.any(), f'{int(bad_edges.sum())} tiles: {[tuple(map(int, t[::-1])) for t in np.argwhere(bad_edges)[:6]]}')
    est_ok = []
    for house, (_, (x, y, ew, eh)) in ESTATES.items():
        one_cell = cell_of(x, y) == cell_of(x + ew - 1, y + eh - 1)
        outside = not (box(x, x + ew, y, y + eh) & box(168, 664, 150, 620)).any()
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
    # -- the graph walk --
    miss = walk['missing']
    check('R1', 'every link end, home, work spot, evening place and route post is reached from the spawn',
          not any(miss.values()) and not walk['bad_links'],
          '; '.join(f'{k}: {len(v)} ({", ".join(v[:4])})' for k, v in miss.items() if v) +
          (f'; links without an arrival: {walk["bad_links"][:6]}' if walk['bad_links'] else ''))
    return out


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
    narrow = public & ~wide
    share = narrow.sum() / max(1, public.sum())
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
        ('at least 60% of outdoor walkable tiles in alleys 2 wide or less', share >= .6, f'{share:.1%}'),
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
    placement = pp.Placement(base, city)
    plan = placement.run()
    names = {rec['key']: rec['name'] for rec in city.records}
    # Names as the world has them now (renamed by doc 67's lane C, when it has run): each reused room's own name.
    for rec in city.records:
        if rec.get('reused'):
            names[rec['key']] = city.rooms[rec['id']]['name']
    labels = pp.render_labels(plan, placement.people, names, {x['id']: x['workLabel'] for x in current['people']})
    final = compose(current, world, city, plan, labels, {x['id']: x for x in base['people']})
    walk = area_graph_reachable(final, placed_rooms={room['id'] for r in city.records for room in r['rooms']})
    result = checks(final, world, city, plan, labels, walk)
    return {'base': base, 'world': world, 'city': city, 'placement': placement, 'plan': plan, 'labels': labels,
            'project': final, 'walk': walk, 'checks': result}


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
    return {
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
    s = rep['sump']
    lines.append(f'The Sump: coverage {s["coverage"]:.1%} (planned {s["planned"]:.1%}), widest open {s["widest"]}, narrow '
                 f'{s["narrow"]:.0%}, dead ends {s["dead_ends"]}, sizes {s["sizes"]}, L-shapes {s["lshape"]}, through '
                 f'{s["through"]}, passages {s["passages"]} in chains {s["chains"]}, courts {s["courts"]}, homes '
                 f'{s["homes"]}, infill {s["infill"]}')
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
        'THE WHARF': (176, 300), 'ABBEY': (250, 215), 'NORTHGATE': (400, 230), 'GREATLY': (580, 196),
        'THE SUMP': (250, 300), 'THE COMMONS': (520, 420), 'MARKET SQ': (470, 384), 'OLD KEEP': (600, 440),
        'ADMIN': (300, 515), 'CINDERS': (240, 600), 'GREAT PALACE': (420, 545), 'THE RISE': (580, 600),
        'TIMBERLAND COURT': (360, 660), 'FELLHALL': (820, 296), 'GRAYROCK HALL': (140, 660)}.items()]
    preview.overview(c.codes, c.heights, cells, folder / 'overview.png', scale=2, labels=labels, relief=1.5)
    views = {'sump': (215, 225, 155, 250), 'market_square_and_hall': (430, 300, 90, 90), 'keep': (570, 435, 80, 75),
             'palace': (315, 525, 230, 95), 'abbey': (215, 148, 120, 92), 'greatly_compound': (535, 148, 132, 132),
             'records_square': (370, 465, 70, 50), 'wharf': (105, 150, 125, 470), 'cinders': (215, 465, 110, 155),
             'estate_timberland_court': (320, 640, 170, 120), 'estate_fellhall': (770, 290, 180, 150),
             'estate_grayrock_hall': (115, 640, 145, 120), 'rise': (535, 465, 130, 155)}
    for name, (x0, y0, w, h) in views.items():
        preview.glyphs(c.codes, c.heights, folder / f'{name}.png', x0, y0, min(w, W - x0), min(h, H - y0), px=8)
    rooms = {r['id']: r for r in p['rooms']}
    for key in ('r2_records_office', 'r2_the_keep', 'r2_market_hall', 'r2_palace_antechamber', 'r2_kings_residence',
                'r2_administrative_wing', 'r2_cathedral'):
        for room in city.named[key]['rooms']:
            r = rooms[room['id']]
            codes = np.frombuffer(''.join(r['terrain']).encode('ascii'), dtype=np.uint8).reshape(r['height'], r['width'])
            preview.glyphs(codes, np.zeros(codes.shape, dtype=np.float32), folder / f'interior_{room["id"]}.png', 0, 0,
                           r['width'], r['height'], px=14)
    for hm in city.sump_homes[:3]:
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


def write(world_key, seed=SEED, dry=False, report=print):
    """Steps 1-4: back up; load the world fresh and lay the city over it; check; save, with the voices' quarters, in
    one transaction; tell open Atlas sessions to reload. Refuses, writing nothing, if any check fails."""
    import map_editor
    import world_db
    import world_store
    import world_sync
    from . import ridgemere2_people as pp
    if world_key in ('dev', 'prod'):
        raise SystemExit('Ridgemere 2.0 is written only into its own world, never dev or prod.')
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
    with world_db.connect(world_key, 'editor') as conn:
        with conn.transaction():
            new = world_store.save_world(conn, p, revision)
            wid = p['id']
            changed = 0
            for npc, quarter in quarters.items():
                changed += conn.execute('UPDATE live.npc_voice SET quarter = %s WHERE world_id = %s AND npc_id = %s '
                                        'AND quarter IS DISTINCT FROM %s', (quarter, wid, npc, quarter)).rowcount
            world_sync.tell_editors(conn, wid, 'ridgemere2.py', 'Ridgemere 2.0: the city (doc 67)')
    report(f'Saved: revision {new} (+1 for the editors\' reload); {changed} voices given their new quarter.')
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
    args = parser.parse_args(argv)
    if args.command == 'write':
        return write(args.world, args.seed, dry=args.dry_run)
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
