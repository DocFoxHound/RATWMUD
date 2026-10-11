"""Ridgemere's country: the world of Docs/Design/60-the-new-map.md.

Eight cells by eight (2048 x 2048 tiles), laid out from the plan rather than a drawing. Ridgemere, the one city, stands
on its hills above the Sound in the north-west, with the rain-soaked cedar coast around it and the great Houses'
estates outside its walls. The north-east is mountains and snow; the south-west warm broadleaf forest falling to the
fens and the delta of the Long River; the south-east rolling downs and open fields, with the grain country and the
orchards further south. Six towns lie out in the country, each within one cell. Biomes fade into one another over a
hundred-odd tiles, each breaks into sub-biome patches, and points of interest are scattered so no cell is empty
ground. Roads find their own way between their waypoints, going round lakes and the sea and bridging rivers.

  cd tools && python3 -m worldgen.realm [--preview DIR] [--out DIR] [--import-dev]

World tile = canvas tile (the world starts at 0, 0). Ridgemere and its estates are designed by worldgen.cities, its
people by worldgen.citizens, the towns built out and peopled by worldgen.towns, and the country's farms, orchards and
works, with their people, by worldgen.country (see assemble()).
"""
from __future__ import annotations

import argparse
import heapq
import json
import math
import random
from pathlib import Path

import numpy as np
from PIL import Image

from . import field, preview
from .canvas import Canvas, half

SIZE = 2048
CELL = 256
OX, OY = 0, 0                           # World tile of canvas (0, 0).
SEED = 20261011

BIOME_IDS = ['water', 'wet', 'north', 'temperate', 'warm', 'swamp', 'deep', 'downs', 'fields']
BLEND = 200                             # Biomes fade into each other over roughly twice this many tiles.


def bid(name):
    return BIOME_IDS.index(name)


# --- The plan ----------------------------------------------------------------------------------------------------
# The coast, north to south: (y, x of the shore). West of it is the sea. Ridgemere Sound is the bay below the north
# headland: the city's waterfront is its east shore.
COAST = [(0, 150), (90, 168), (150, 190), (190, 228), (225, 262), (270, 282), (330, 288), (420, 284), (500, 290),
         (560, 282), (610, 255), (650, 205), (700, 160), (780, 136), (900, 118), (1040, 100), (1200, 108),
         (1380, 128), (1520, 160), (1640, 186), (1760, 170), (1880, 140), (2047, 118)]
SOUND = (300, 400)                       # The middle of the Sound, for naming its cells.

RANGES = {  # name: [(x, y, height, spread in tiles)]; each range's peaks are joined by ridges
    'The Whitefangs': [(1260, 110, 12, 60), (1410, 80, 14, 70), (1570, 120, 15, 70), (1730, 80, 16, 80),
                       (1900, 140, 16, 85), (1990, 320, 15, 75), (1850, 330, 14, 65), (1980, 520, 13, 60),
                       (1860, 640, 12, 55), (1990, 760, 11, 55), (1740, 620, 12, 55), (1440, 250, 12, 55),
                       (1310, 300, 10, 50)],
    'The Greyteeth': [(1660, 760, 10, 50), (1520, 820, 9, 45), (1880, 900, 9, 45), (1400, 700, 9, 45)],
    'The Rainward Hills': [(760, 150, 8, 50), (900, 230, 9, 55), (1040, 170, 10, 55), (1120, 330, 9, 50),
                           (980, 400, 7, 45)],
}
HILLS = [  # (x, y, height, spread): neighbouring hills are joined into downs
    (640, 470, 4, 40), (700, 560, 4, 40), (560, 760, 4, 40), (760, 820, 4, 40), (880, 700, 5, 45),
    (180, 1100, 4, 40), (300, 1230, 4, 40), (760, 1080, 4, 40), (860, 1260, 4, 40),
]
# Rivers, source to mouth: (x, y) points and the width at each end. The Long River rises in Glasswater, the lake
# below the mountains, and winds south-west to its delta in the fens; the Rainrun comes down from the Rainward Hills
# into the Sound just south of Ridgemere; the Lark runs out of the downs into the Long River.
RIVERS = [
    ('The Long River', [(1160, 650), (1110, 760), (1060, 880), (990, 1000), (930, 1130), (850, 1260), (770, 1380),
                        (680, 1490), (580, 1600), (470, 1700), (360, 1790), (240, 1860), (140, 1900), (40, 1930)],
     6, 22),
    ('The Rainrun', [(1000, 300), (900, 360), (800, 430), (700, 500), (600, 560), (500, 590), (400, 600),
                     (320, 590), (250, 572)], 4, 10),
    ('The Lark', [(1780, 1120), (1650, 1180), (1520, 1260), (1400, 1320), (1250, 1360), (1100, 1380), (960, 1370),
                  (830, 1345)], 4, 10),
]
LAKE = ((1235, 600), 78, 48)             # Glasswater: centre, reach x, reach y.

# Settlements (the towns), each inside one cell: id, name, kind, (x, y, w, h) on the canvas, {gate side: the point
# the gate faces}, and whether its wall follows the shore. Ridgemere is designed by worldgen.cities, which opens its
# gates ('ridgemere', 'N'/'E'/'S') for the roads below.
SETTLEMENTS = [
    ('frosthollow', 'Frosthollow', 'town', (1572, 404, 70, 56), {'W': None, 'S': None}, False),
    ('coldwater', 'Coldwater', 'town', (1052, 560, 70, 56), {'W': None, 'N': None, 'S': None}, False),
    ('oakhaven', 'Oakhaven', 'town', (560, 1120, 70, 56), {'N': None, 'E': None, 'S': None}, False),
    ('mirebridge', 'Mirebridge', 'town', (300, 1600, 70, 56), {'N': None, 'E': None}, False),
    ('larkfield', 'Larkfield', 'town', (1330, 1170, 70, 56), {'W': None, 'E': None, 'S': None}, False),
    ('southmeadow', 'Southmeadow', 'town', (1630, 1720, 70, 56), {'N': None, 'W': None}, False),
]
CITY = 'ridgemere'
# Roads: name, waypoints ('<settlement>.<side>' is that gate), and whether it goes straight. Between waypoints the road
# finds its own way, keeping off the lake and the sea and bridging rivers where it must.
ROADS = [
    ('The Southway', ['ridgemere.S', (470, 700), (540, 880), (590, 1030), 'oakhaven.N'], False),
    ('The Fen Road', ['oakhaven.S', (560, 1330), (450, 1480), 'mirebridge.N'], False),
    ('The High Road', ['ridgemere.E', (760, 380), (900, 470), 'coldwater.W'], False),
    ('The Pass Road', ['coldwater.N', (1150, 470), (1330, 440), (1480, 430), 'frosthollow.W'], False),
    ('The Downs Road', ['coldwater.S', (1100, 800), (1200, 980), 'larkfield.W'], False),
    ('The Riverway', ['oakhaven.E', (820, 1170), (1050, 1180), 'larkfield.W'], False),
    ('The South Road', ['larkfield.S', (1430, 1400), (1560, 1580), 'southmeadow.N'], False),
    ('The Meadow Road', ['mirebridge.E', (640, 1740), (950, 1800), (1250, 1790), (1500, 1760), 'southmeadow.W'], False),
    ('The Eastmarch Road', ['larkfield.E', (1650, 1180), (1850, 1250)], False),
    ('The Mine Track', ['frosthollow.S', (1640, 560), (1660, 700)], False),
]
REGIONS = [  # (name, x, y, blurb): cells are named for the nearest
    ('Ridgemere Sound', 140, 420, 'The grey Sound below Ridgemere: ships at anchor, gulls, and rain on the water.'),
    ('Ridgemere', 430, 330, 'The rain city on its hills above the Sound: walls, wharves, smoke and slate roofs.'),
    ('The Rainwood', 720, 120, 'Cedar rainforest, fern and moss on the hills behind Ridgemere, where the rain '
                               'seldom stops.'),
    ('The Houses\' Lands', 700, 400, 'Wet green country east of the city, where the great Houses keep their estates.'),
    ('The Rainward Hills', 1000, 250, 'Steep cedar-dark hills where the Rainrun rises.'),
    ('The Folds', 450, 760, 'Sheep country south of Ridgemere: stone walls, wet pasture and rabbit warrens.'),
    ('The Whitefangs', 1700, 160, 'The high range of the north-east: snowfields, ice-bound cliffs and bitter passes.'),
    ('The Frost Valley', 1600, 430, 'A high snowy valley among the peaks, where Frosthollow keeps its fires lit.'),
    ('Glasswater', 1240, 610, 'A cold lake under the mountains, clear as glass; the Long River rises here.'),
    ('The Greyteeth', 1700, 800, 'Lower, broken mountains of scree and pine south of the Whitefangs.'),
    ('The Eastern Snows', 1950, 560, 'Snowfields and frozen tarns along the eastern edge of the world.'),
    ('The Greenholt', 900, 760, 'Mixed woods and meadows in the middle country, between the rain and the snow.'),
    ('The Warmwood', 400, 1150, 'Warm broadleaf forest: oak, beech and hazel, ferns and glades, thick with birdsong.'),
    ('The Rivermeet', 820, 1330, 'Where the Lark joins the Long River among water meadows and woods.'),
    ('The Sallow Fens', 520, 1620, 'Reed marsh, bog and alder carr along the lower Long River.'),
    ('The Drowned Delta', 200, 1850, 'The delta of the Long River: black water, drowned trees and channels that '
                                     'shift with every flood.'),
    ('The Larkdowns', 1400, 1050, 'Rolling downs and open fields, larks overhead and the wind in the grass.'),
    ('The High Downs', 1800, 1050, 'Long bare downs rolling to the eastern edge of the world.'),
    ('The Sunfields', 1100, 1650, 'The grain country: a patchwork of golden fields, hedgerows and farms.'),
    ('The Orchard Vale', 1450, 1500, 'Orchards and hay meadows along the south road, sweet with blossom in spring.'),
    ('The Southern Meads', 1800, 1850, 'Meadows, fields and farmsteads at the southern edge of the world.'),
    ('The Fenshore', 650, 1900, 'Wet fields and marsh along the southern shore.'),
]
SUBBIOMES = {  # name, (ground codes and weights), tree codes, tree density, extra (code, density)...
    'north': [('Snowfields', {'*': 8, '-': 2}, '1', .02, [('o', .004)]),
              ('Taiga', {'-': 5, '*': 4}, '1', .30, [('7', .004), ('o', .002)]),
              ('Tundra', {'-': 7, ',': 1, 'r': 2}, '1', .01, [('J', .01), ('o', .004)]),
              ('Frozen fen', {'-': 5, 'J': 3, '*': 2}, None, 0, [('E', .06)]),
              ('Rock barrens', {'r': 5, 's': 2, '*': 2}, '1', .01, [('o', .02)])],
    'wet': [('Cedar rainforest', {'!': 5, ',': 3, '&': 2}, '$P', .38, [('7', .012), ('&', .06)]),
            ('Fern glades', {'&': 3, '!': 4, ',': 3}, '$', .06, [('7', .006)]),
            ('Wet meadow', {',': 5, '"': 3, '!': 2, 'D': 1}, 'Y', .02, [('~', .004)]),
            ('Mossy bluffs', {'!': 4, 'r': 4, ',': 2}, 'P', .08, [('o', .02)]),
            ('Alder bottoms', {'!': 3, 'D': 2, ',': 3, 'E': 1}, 'Y', .25, [('&', .05)]),
            ('Old growth', {'!': 6, '&': 2, ',': 1}, '$', .5, [('7', .02)])],
    'temperate': [('Mixed forest', {',': 6, '"': 1}, 'YP', .28, [('B', .02), ('7', .006)]),
                  ('Meadow', {',': 5, '"': 3, '3': 1}, 'Y', .01, [('B', .004)]),
                  ('Heath', {'5': 5, ',': 3}, 'P', .02, [('o', .008), ('B', .02)]),
                  ('Pine woods', {',': 5, '.': 1}, 'P', .36, [('7', .006)]),
                  ('Old wood', {',': 5, '"': 2}, 'Y', .45, [('B', .03), ('7', .01)])],
    'warm': [('Oak wood', {',': 5, '"': 2, '&': 1}, 'Y', .34, [('B', .02), ('7', .006)]),
             ('Beech hanger', {',': 4, '!': 2, '&': 2}, 'Y', .46, [('7', .01)]),
             ('Glade', {',': 5, '3': 2, '"': 2}, 'Y', .03, [('B', .01)]),
             ('Fern brake', {'&': 5, ',': 3, '!': 1}, 'Y', .14, [('7', .004)]),
             ('Bramble scrub', {',': 4, '"': 3}, 'B', .16, [('Y', .02)]),
             ('Hazel coppice', {',': 5, '&': 1, '3': 1}, 'B', .3, [('Y', .05)])],
    'swamp': [('Reed marsh', {',': 3, 'E': 4, 'w': 2}, None, 0, []),
              ('Bog', {'w': 3, 'D': 3, ',': 2}, '2', .05, []),
              ('Alder carr', {',': 3, 'D': 2, 'w': 1}, 'Y', .2, [('2', .05)]),
              ('Mud flats', {'D': 5, 'w': 2, 'E': 1}, None, 0, [])],
    'deep': [('Drowned forest', {'w': 4, 'D': 3}, '2', .22, [('E', .05)]),
             ('Black mire', {'w': 6, 'E': 2, 'D': 1}, '2', .05, []),
             ('Sunken woods', {'w': 3, 'D': 3, ',': 1}, 'Y', .22, [('2', .05)])],
    'downs': [('Rolling grass', {',': 6, '"': 3, '3': 1}, 'Y', .003, []),
              ('Chalk down', {',': 5, ';': 3, '3': 1}, None, 0, [('o', .002)]),
              ('Copse country', {',': 5, '"': 2}, 'Y', .06, [('B', .02)]),
              ('Hay meadow', {'"': 6, ',': 2, '3': 2}, None, 0, []),
              ('Gorse down', {',': 4, '5': 3}, 'B', .1, [])],
    'fields': [('Grain country', None, None, 0, []),                  # The field patchwork (fields()).
               ('Hay meadow', {'"': 5, ',': 3, '3': 2}, 'Y', .006, [('B', .004)]),
               ('Grain country', None, None, 0, []),
               ('Orchard hills', {',': 5, '"': 2, '3': 1}, 'Y', .09, [('B', .01)])],
}
FEATURES = {
    'north': ['frozen_lake', 'ruins', 'stone_circle', 'camp', 'outcrop', 'wolf_den', 'hot_spring', 'ice_ridge',
              'cairn', 'grove', 'ravine'],
    'wet': ['ruins', 'grove', 'camp', 'pond', 'shrine', 'great_tree', 'outcrop', 'wolf_den', 'ravine', 'cairn',
            'stone_circle', 'watchtower'],
    'temperate': ['ruins', 'stone_circle', 'camp', 'pond', 'grove', 'outcrop', 'farmstead', 'great_tree', 'shrine',
                  'ravine', 'watchtower', 'wolf_den', 'orchard'],
    'warm': ['grove', 'great_tree', 'pond', 'camp', 'ruins', 'shrine', 'wolf_den', 'stone_circle', 'flower_field',
             'great_tree', 'outcrop'],
    'downs': ['stone_circle', 'great_tree', 'camp', 'pond', 'farmstead', 'ruins', 'wagon', 'watchtower',
              'flower_field', 'cairn', 'outcrop', 'orchard'],
    'fields': ['farmstead', 'farmstead', 'orchard', 'orchard', 'great_tree', 'wagon', 'pond', 'shrine',
               'flower_field', 'watchtower'],
    'swamp': ['sunken_ruins', 'boardwalk', 'hummock', 'dead_circle', 'camp', 'pond', 'grove'],
    'deep': ['sunken_ruins', 'boardwalk', 'hummock', 'dead_circle', 'drowned_shrine'],
}
WEATHER = {'north': 'snow', 'wet': 'rain', 'deep': 'fog', 'swamp': 'overcast'}


def grow(mask):
    grown = mask.copy()
    grown[1:] |= mask[:-1]
    grown[:-1] |= mask[1:]
    grown[:, 1:] |= mask[:, :-1]
    grown[:, :-1] |= mask[:, 1:]
    return grown


def box_blur(a, r):
    """Mean over a (2r+1)-tile square, edges extended; three passes approach a gaussian."""
    for axis in (0, 1):
        pad = [(0, 0), (0, 0)]
        pad[axis] = (r + 1, r)
        c = np.cumsum(np.pad(a, pad, mode='edge'), axis=axis, dtype=np.float32)
        n = 2 * r + 1
        a = (c[n:] - c[:-n]) / n if axis == 0 else (c[:, n:] - c[:, :-n]) / n
    return a


def components(mask, seeds):
    """Which of the seed points each True tile is connected to (by flood fill, 4-way): an int map, -1 elsewhere."""
    from PIL import ImageDraw
    out = np.full(mask.shape, -1, dtype=np.int16)
    for i, (x, y) in enumerate(seeds):
        x, y = int(x), int(y)
        if not mask[y, x] or out[y, x] >= 0:
            continue
        image = Image.fromarray(mask.astype(np.uint8) * 255).copy()   # A copy: a shared array is read-only.
        ImageDraw.floodfill(image, (x, y), 128)
        out[np.asarray(image) == 128] = i
    return out


def tree_edges(points, longest=None):
    """The edges of a minimum spanning tree over the points (Prim's), skipping any longer than `longest`."""
    if len(points) < 2:
        return []
    inside, edges = {0}, []
    while len(inside) < len(points):
        best = None
        for i in inside:
            for j in range(len(points)):
                if j not in inside:
                    d = math.dist(points[i][:2], points[j][:2])
                    if best is None or d < best[0]:
                        best = (d, i, j)
        inside.add(best[2])
        if longest is None or best[0] <= longest:
            edges.append((best[1], best[2]))
    return edges


def blob(r, ry, rng):
    """An irregular round shape: the radius (along x; ry along y) at each angle, wobbled so it never reads as a circle."""
    a1, a2, a3 = (rng.uniform(0, 2 * math.pi) for _ in range(3))
    k = rng.uniform(.12, .22)

    def inside(dx, dy):
        angle = math.atan2(dy, dx)
        scale = 1 + k * math.sin(2 * angle + a1) + .7 * k * math.sin(3 * angle + a2) + .4 * k * math.sin(5 * angle + a3)
        return (dx / (r * scale)) ** 2 + (dy / (ry * scale)) ** 2
    return inside


def soft(value, a, b):
    """0 below a, 1 above b, smooth between (a > b runs the other way)."""
    return field.smoothstep(a, b, value) if a < b else 1 - field.smoothstep(b, a, value)


# --- Building the world ------------------------------------------------------------------------------------------
class Realm:
    def __init__(self):
        self.rng = random.Random(SEED)
        self.c = Canvas(SIZE, SIZE, ',')
        self.features = []                  # (canvas x, y, kind)
        self.gates = {}                     # (settlement, side) -> the canvas point just outside that gate
        self.places = {}                    # settlement -> its ground (a mask), which roads route around
        self.road_lines = {}                # road name -> the canvas points it follows
        self.levels = {}                    # settlement -> the height of its ground
        self.gate_levels = {}               # (settlement, side) -> the height of that gate, where it differs
        self.reserved_ids = set()           # Room IDs the world already uses (the city's interiors avoid them)
        self.city_site, self.cities, self.city_roads = None, [], []
        self.causeway = np.zeros((SIZE, SIZE), dtype=bool)
        self.ua_mask = np.zeros((SIZE, SIZE), dtype=bool)    # (The kit's name for ground nothing may touch: none here.)

    # The land ----------------------------------------------------------------------------------------------------
    def lay_water(self):
        ys, xs = np.mgrid[0:SIZE, 0:SIZE].astype(np.float32)
        shore = np.interp(np.arange(SIZE), [y for y, _ in COAST], [x for _, x in COAST]).astype(np.float32)
        # The shore wanders: coves and points at a few scales, not a ruled line.
        wobble = (field.fbm(SIZE, SIZE, 90, SEED + 1, 3) - .5) * 70 + (field.fbm(SIZE, SIZE, 18, SEED + 2, 2) - .5) * 16
        self.ocean = xs < shore[ys.astype(int)] + wobble * (1 - .85 * self.city_shore_mask(xs, ys))
        # Fill any lone sea specks inland and lone land specks at sea: the sea is the piece joined to the west edge.
        self.ocean = components(self.ocean, [(2, y) for y in range(10, SIZE, 200)]) >= 0
        water = self.ocean.copy()
        # Glasswater.
        (lx, ly), rx, ry = LAKE
        shape = blob(rx, ry, random.Random(SEED + 3))
        self.lake = np.zeros((SIZE, SIZE), dtype=bool)
        wob = field.fbm(SIZE, SIZE, 16, SEED + 4, 2)
        for y in range(ly - ry * 2, ly + ry * 2):
            for x in range(lx - rx * 2, lx + rx * 2):
                if shape(x - lx, y - ly) <= 1 + (wob[y, x] - .5) * .35:
                    self.lake[y, x] = True
        water |= self.lake
        # The rivers: each a wandering line, widening downstream.
        self.river = np.zeros((SIZE, SIZE), dtype=bool)
        for i, (name, points, w0, w1) in enumerate(RIVERS):
            self.river |= self.river_mask(points, w0, w1, SEED + 10 + i)
        water |= self.river
        self.water = water

    @staticmethod
    def city_shore_mask(xs, ys):
        """Along Ridgemere's waterfront the shore keeps nearly straight, so the wharf can run along it."""
        return np.exp(-(((ys - 380) / 150) ** 2)) * (xs < 400)

    def river_mask(self, points, w0, w1, seed):
        rng = random.Random(seed)
        # Densify the line with meanders: a sideways wobble of a few tiles, slow along the river.
        pts = []
        for (x1, y1), (x2, y2) in zip(points, points[1:]):
            n = max(2, int(math.dist((x1, y1), (x2, y2)) / 6))
            for k in range(n):
                t = k / n
                pts.append((x1 + (x2 - x1) * t, y1 + (y2 - y1) * t))
        pts.append(points[-1])
        total = len(pts)
        phase, amp = rng.uniform(0, 6), rng.uniform(6, 12)
        mask = np.zeros((SIZE, SIZE), dtype=bool)
        for k in range(1, total):
            (x1, y1), (x2, y2) = pts[k - 1], pts[k]
            dx, dy = x2 - x1, y2 - y1
            n = math.hypot(dx, dy) or 1
            off = amp * math.sin(k / 9 + phase) + amp * .4 * math.sin(k / 3.7 + 2 * phase)
            ox, oy = -dy / n * off, dx / n * off
            t = k / total
            width = w0 + (w1 - w0) * t
            mask |= self.c.line_mask([(x1 + ox, y1 + oy), (x2 + ox, y2 + oy)], width)
        return mask

    def lay_biomes(self):
        ys, xs = np.mgrid[0:SIZE, 0:SIZE].astype(np.float32)
        # Bent coordinates, so no border is a straight line.
        wx = xs + (field.fbm(SIZE, SIZE, 180, SEED + 20, 3) - .5) * 260
        wy = ys + (field.fbm(SIZE, SIZE, 180, SEED + 21, 3) - .5) * 260
        label = np.full((SIZE, SIZE), bid('temperate'), dtype=np.int8)
        # The south: warm forest west, downs east; fields along the south; fens and the delta in the south-west.
        south = wy > 980
        label[south & (wx < 1000)] = bid('warm')
        label[south & (wx >= 1000)] = bid('downs')
        label[(wy > 1450) & (wx > 760) & (wx < 1900)] = bid('fields')
        label[(wy > 1360) & (wx > 1250) & (wx < 1700)] = bid('fields')        # The Orchard Vale.
        fen = (wy > 1460) & (wx < 820 - (wy - 1460) * .1)
        label[fen] = bid('swamp')
        label[(wy > 1720) & (wx < 460)] = bid('deep')
        # The north-east: snow, and the mountains always cold.
        label[(wx > 1150) & (wy < 760 + (wx - 1150) * .12)] = bid('north')
        # The rain coast round Ridgemere, its hills and the Houses' lands.
        wet = ((wx - 420) / 720) ** 2 + ((wy - 360) / 560) ** 2 < 1
        label[wet & (label != bid('north'))] = bid('wet')
        label[self.ocean] = bid('wet')                   # (Under water: the biome of the nearest shore.)
        self.label = label
        self.biome = self.blend(label)
        # The middle country keeps its own: the Greenholt between the rain, the snow and the south.
        self.sub = self.patches()

    def blend(self, label):
        """Each tile picks among the biomes near it, weighted by how much of each is around: where two meet they
        interleave in clumps and specks over a band ~2 x BLEND wide, rather than meeting at a line."""
        present = [k for k in range(len(BIOME_IDS)) if k != bid('water') and (label == k).any()]
        clumps = field.fbm(SIZE, SIZE, 30, SEED + 70, 3)
        specks = np.random.default_rng(SEED + 71).random((SIZE, SIZE), dtype=np.float32)
        best = np.full((SIZE, SIZE), -9, dtype=np.float32)
        out = label.copy()
        self.north_weight = np.zeros((SIZE, SIZE), dtype=np.float32)
        for n, k in enumerate(present):
            weight = box_blur(box_blur(box_blur((label == k).astype(np.float32), BLEND // 2), BLEND // 2), BLEND // 2)
            noise = (np.roll(clumps, (n * 311, n * 197), (0, 1)) - .5) * 1.3 + (np.roll(specks, n * 523, 1) - .5) * .45
            if k == bid('north'):
                self.north_weight = weight
            score = np.where(weight > .01, weight + noise, -9)
            better = score > best
            best[better] = score[better]
            out[better] = k
        return out

    def patches(self):
        rng = np.random.default_rng(SEED)
        step = 150
        n = SIZE // step + 2
        jitter = rng.random((n, n, 2)) * step
        variant = rng.integers(0, 1000, (n, n))
        ys, xs = np.mgrid[0:SIZE, 0:SIZE].astype(np.float32)
        xs = xs + (field.fbm(SIZE, SIZE, 110, SEED + 11, 3) - .5) * 130
        ys = ys + (field.fbm(SIZE, SIZE, 110, SEED + 12, 3) - .5) * 130
        xs = xs + (field.fbm(SIZE, SIZE, 14, SEED + 13, 2) - .5) * 36    # Ragged, not ruled, borders...
        ys = ys + (field.fbm(SIZE, SIZE, 14, SEED + 14, 2) - .5) * 36
        xs = xs + rng.normal(0, 9, (SIZE, SIZE)).astype(np.float32)       # ...that interleave for a few tiles.
        ys = ys + rng.normal(0, 9, (SIZE, SIZE)).astype(np.float32)
        gx, gy = np.clip((xs // step).astype(int), 0, n - 2), np.clip((ys // step).astype(int), 0, n - 2)
        best = np.full((SIZE, SIZE), 1e12, dtype=np.float32)
        chosen = np.zeros((SIZE, SIZE), dtype=np.int32)
        for oy in (-1, 0, 1):
            for ox in (-1, 0, 1):
                cx, cy = np.clip(gx + ox, 0, n - 1), np.clip(gy + oy, 0, n - 1)
                sx = cx * step + jitter[cy, cx, 0]
                sy = cy * step + jitter[cy, cx, 1]
                d = (xs - sx) ** 2 + (ys - sy) ** 2
                better = d < best
                best[better] = d[better]
                chosen[better] = variant[cy, cx][better]
        return chosen

    def lay_heights(self):
        h = np.zeros((SIZE, SIZE), dtype=np.float32)
        b = self.biome
        rolling = field.fbm(SIZE, SIZE, 70, SEED + 20, 4)
        knolls = np.maximum(0, rolling - .56) * 9               # Low hills in patches; most lowland stays level.
        for name, scale in (('north', 1.3), ('wet', 1.2), ('temperate', 1.0), ('warm', .7), ('swamp', .2),
                            ('deep', .1), ('fields', .4)):
            mask = b == bid(name)
            h[mask] += knolls[mask] * scale
        # The downs roll everywhere: long swells rather than scattered knolls, gentler in the grain country.
        downs = box_blur(box_blur((b == bid('downs')).astype(np.float32), 40), 40)
        fields = box_blur(box_blur((b == bid('fields')).astype(np.float32), 40), 40)
        swell = np.maximum(0, field.fbm(SIZE, SIZE, 110, SEED + 24, 3) - .28) * 9
        h += downs * swell + fields * swell * .45
        # Domain warp: ranges and hills are laid on bent coordinates, so nothing comes out round.
        wx = (field.fbm(SIZE, SIZE, 150, SEED + 25, 3) - .5) * 110 + (field.fbm(SIZE, SIZE, 40, SEED + 26, 2) - .5) * 26
        wy = (field.fbm(SIZE, SIZE, 150, SEED + 27, 3) - .5) * 110 + (field.fbm(SIZE, SIZE, 40, SEED + 28, 2) - .5) * 26
        for peaks in RANGES.values():
            for i, j in tree_edges(peaks):
                self.ridge(h, peaks[i], peaks[j], wx, wy, 1.0)
            for x, y, height, spread in peaks:
                self.ridge(h, (x, y, height, spread), (x + 1, y, height, spread), wx, wy, 1.0)
        for i, j in tree_edges(HILLS, longest=150):
            self.ridge(h, HILLS[i], HILLS[j], wx, wy, .9)
        for hill in HILLS:
            self.ridge(h, hill, (hill[0] + 1, hill[1], hill[2], hill[3]), wx, wy, .9)
        # Spurs and gullies on the ranges (ridged noise), and fine roughness on the heights.
        mountain = field.smoothstep(3, 9, h)
        ridged = 1 - np.abs(2 * field.fbm(SIZE, SIZE, 55, SEED + 29, 4) - 1)
        h += (ridged - .55) * 5 * mountain
        h += (field.fbm(SIZE, SIZE, 14, SEED + 22, 2) - .5) * 1.6 * mountain
        # The valleys the towns and roads need in the mountains: Frosthollow's, and the pass up to it.
        for (x, y, w, hh), depth in (((1500, 380, 220, 140), .25), ((1100, 400, 420, 160), .45)):
            ys, xs = np.mgrid[0:SIZE, 0:SIZE].astype(np.float32)
            d = np.hypot((xs - x - w / 2) / (w / 2), (ys - y - hh / 2) / (hh / 2))
            k = 1 - field.smoothstep(.6, 1.15, d)
            h = h * (1 - k * (1 - depth)) + k * depth * np.minimum(h, 9)
        # The shore: land slopes to the water's edge; water itself is level.
        near_water = field.distance_to_mask(self.water, 12)
        h = np.where(self.water, 0, h * field.smoothstep(0, 12, near_water))
        self.c.heights[:] = half(np.clip(h, -16, 16))

    def ridge(self, h, a, b, wx, wy, strength):
        """A ridge from peak a to peak b (x, y, height, spread): a crest that dips to a saddle between them, falling
        away steeply to foothills, measured on warped coordinates."""
        (ax, ay, ah, aw), (bx, by, bh, bw) = a, b
        w = (aw + bw) / 2 * 1.25
        reach = int(w * 3.2 + 60)
        x0, x1 = max(0, int(min(ax, bx)) - reach), min(SIZE, int(max(ax, bx)) + reach)
        y0, y1 = max(0, int(min(ay, by)) - reach), min(SIZE, int(max(ay, by)) + reach)
        ys, xs = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        px, py = xs + wx[y0:y1, x0:x1], ys + wy[y0:y1, x0:x1]
        dx, dy = bx - ax, by - ay
        t = np.clip(((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy or 1e-9), 0, 1)
        d = np.hypot(px - (ax + t * dx), py - (ay + t * dy))
        crest = (ah + (bh - ah) * t) * (1 - .3 * np.sin(np.pi * t) * (math.dist((ax, ay), (bx, by)) > 5)) * strength
        core = crest * np.exp(-(d / w) ** 1.6)
        foot = .38 * crest * np.exp(-(d / (2.8 * w)) ** 2)
        view = h[y0:y1, x0:x1]
        np.maximum(view, np.maximum(core, foot), out=view)

    def lay_ground(self):
        c = self.c
        rng = np.random.default_rng(SEED + 30)
        grain = rng.random((SIZE, SIZE), dtype=np.float32)
        pick = rng.random((SIZE, SIZE), dtype=np.float32)
        detail = field.fbm(SIZE, SIZE, 12, SEED + 31, 2)
        codes = np.full((SIZE, SIZE), ord(','), dtype=np.uint8)
        for biome, subs in SUBBIOMES.items():
            in_biome = self.biome == bid(biome)
            which = self.sub % len(subs)
            for i, (name, ground, trees, density, extras) in enumerate(subs):
                mask = in_biome & (which == i)
                if not mask.any():
                    continue
                if ground is None:
                    self.field_patchwork(codes, mask)
                    continue
                # Ground: weighted by clumped noise, so each kind forms patches rather than salt and pepper.
                keys, weights = list(ground), np.array(list(ground.values()), dtype=np.float32)
                edges = np.cumsum(weights / weights.sum())
                mix = (detail * .7 + pick * .3)
                choice = np.searchsorted(edges, np.clip(mix, 0, .9999))
                for k, key in enumerate(keys):
                    codes[mask & (choice == k)] = ord(key)
                if trees:
                    clumps = field.fbm(SIZE, SIZE, 18, SEED + 40 + i, 2)
                    chance = density * np.clip((clumps - .3) * 2.5, 0, 2.2)
                    grove = mask & (grain < chance)
                    kinds = np.array([ord(t) for t in trees], dtype=np.uint8)
                    codes[grove] = kinds[(pick[grove] * len(trees)).astype(int) % len(trees)]
                for code, share in extras:
                    codes[mask & (pick < share) & (grain > .5)] = ord(code)
        h = c.heights
        # Mountain flanks: forest climbing the lower slopes, thinning with height.
        flank = field.fbm(SIZE, SIZE, 40, SEED + 32, 3)
        slope_wood = (h >= 2.5) & (h < 8) & (flank > .48) & (grain < .34 - h * .03)
        codes[slope_wood & np.isin(self.biome, [bid('temperate'), bid('downs')])] = ord('P')
        codes[slope_wood & (self.biome == bid('warm'))] = ord('Y')
        codes[slope_wood & (self.biome == bid('north'))] = ord('1')
        wet_wood = slope_wood & (self.biome == bid('wet'))
        codes[wet_wood] = np.where(pick[wet_wood] < .6, ord('$'), ord('P'))
        # Bare rock, scree and snow on the heights (lower in the cold north-east).
        cold = self.biome == bid('north')
        codes[(h >= 6) & (detail > .5)] = ord('r')
        codes[(h >= 8) & (detail > .35)] = ord('r')
        codes[(h >= 9) & (detail > .62)] = ord('s')
        codes[((h >= 11) | (cold & (h >= 6))) & (detail > .4)] = ord('*')
        codes[(h >= 13)] = ord('*')
        wet = self.biome == bid('wet')
        codes[wet & (h >= 6) & (h < 11) & (detail < .45)] = ord('!')    # Moss on the rain coast's rocks.
        # Water: deep in the middle, wadeable at the edges.
        shore = field.distance_to_mask(~self.water, 8)
        codes[self.water] = ord('W')
        codes[self.water & (shore <= 3)] = ord('~')
        swampy = (self.biome == bid('swamp')) | (self.biome == bid('deep'))
        codes[self.water & (shore <= 5) & swampy] = ord('w')
        # Snow thins toward the edge of the cold north-east: tundra, then grass, show through as it gives out.
        thin = (self.biome == bid('north')) & (codes == ord('*')) & (h < 11)
        cover = np.clip(self.north_weight * 1.4 - .15, 0, 1)
        sparse = thin & (grain > cover)
        codes[sparse] = np.where(pick[sparse] < cover[sparse] + .3, ord('-'), ord(','))
        frozen = self.water & cold & ~self.ocean & (h >= 0)
        codes[frozen & (shore <= 6)] = ord('J')
        # Beaches and banks: sand, the rain coast's shingle and the fens' reeds.
        land_edge = field.distance_to_mask(self.water, 6)
        bank = ~self.water & (land_edge <= 3) & (detail > .35)
        codes[bank & ~cold & ~swampy & ~wet] = ord('0')
        codes[bank & wet] = np.where(pick[bank & wet] < .3, ord('o'), ord('r'))
        codes[~self.water & (land_edge <= 2) & swampy] = ord('E')
        free = ~c.locked
        c.codes[free] = codes[free]

    def field_patchwork(self, codes, mask):
        """A patchwork of grain fields, stubble, pasture and flowers, parcelled by hedgerows with the odd tree."""
        ys, xs = np.mgrid[0:SIZE, 0:SIZE].astype(np.float32)
        a = .35
        u = xs * math.cos(a) + ys * math.sin(a) + (field.fbm(SIZE, SIZE, 90, SEED + 90, 2) - .5) * 60
        v = -xs * math.sin(a) + ys * math.cos(a) + (field.fbm(SIZE, SIZE, 90, SEED + 91, 2) - .5) * 60
        pu, pv = np.floor(u / 38).astype(np.int64), np.floor(v / 27).astype(np.int64)
        parcel = ((pu * 73856093) ^ (pv * 19349663)) % 100
        crop = np.select([parcel < 58, parcel < 72, parcel < 82, parcel < 88], [ord('4'), ord(';'), ord('"'), ord('3')],
                         ord(','))
        noise = np.random.default_rng(SEED + 92).random((SIZE, SIZE), dtype=np.float32)
        hedge = ((u % 38) < 1.3) | ((v % 27) < 1.3)
        field_codes = np.where(hedge, np.where(noise < .07, ord('Y'), np.where(noise < .75, ord('B'), ord(','))), crop)
        codes[mask] = field_codes[mask].astype(np.uint8)

    # Places ------------------------------------------------------------------------------------------------------
    def flatten_mask(self, region, margin=10):
        """Level ground under `region` (a mask), eased into the land around it over `margin` tiles."""
        c = self.c
        ys, xs = np.nonzero(region)
        level = float(half(np.median(c.heights[ys, xs])))
        y0, y1 = max(0, ys.min() - margin - 2), min(SIZE, ys.max() + margin + 3)
        x0, x1 = max(0, xs.min() - margin - 2), min(SIZE, xs.max() + margin + 3)
        near = field.distance_to_mask(region[y0:y1, x0:x1], margin + 1)
        weight = 1 - field.smoothstep(0, margin, near)
        sub = c.heights[y0:y1, x0:x1]
        keep = c.locked[y0:y1, x0:x1]
        sub[:] = np.where(keep, sub, half(sub * (1 - weight) + level * weight))
        return level

    def flatten(self, x0, y0, w, h, margin=6):
        region = np.zeros((SIZE, SIZE), dtype=bool)
        region[max(0, y0):y0 + h, max(0, x0):x0 + w] = True
        return self.flatten_mask(region, margin)

    def settlement(self, sid, name, kind, box, gates, shore):
        c = self.c
        x, y, w, h = box
        region = np.zeros((SIZE, SIZE), dtype=bool)
        region[y:y + h, x:x + w] = True
        if shore:
            # Keep to the land, a few tiles back from the water, and to the one piece of it the box mostly holds.
            dry = region & ~grow(grow(grow(grow(self.water))))
            dy, dx = np.nonzero(dry)
            i = int(np.argmin((dx - (x + w / 2)) ** 2 + (dy - (y + h / 2)) ** 2))
            region = components(dry, [(dx[i], dy[i])]) >= 0
            # Smooth the outline so the wall runs along the shore rather than stepping with every cove.
            region = (box_blur(region.astype(np.float32), 5) > .5) & dry
        else:
            self.water[region] = False
        level = self.flatten_mask(region, 10)
        floor = {'city': '.', 'town': '.', 'village': ',', 'fortress': 'f', 'ghost': '.'}[kind]
        wall = {'city': 'H', 'town': '#', 'village': '|', 'fortress': '#', 'ghost': '#'}[kind]
        thick = {'city': 3, 'town': 2, 'village': 1, 'fortress': 4, 'ghost': 2}[kind]
        y0, y1, x0, x1 = y - 2, y + h + 2, x - 2, x + w + 2
        inner = field.distance_to_mask(~region[y0:y1, x0:x1], thick + 1)
        ring = np.zeros((SIZE, SIZE), dtype=bool)
        ring[y0:y1, x0:x1] = region[y0:y1, x0:x1] & (inner <= thick)
        c.paint(region, floor, level)
        c.paint(ring, wall, level)
        self.water[region] = False
        self.places[sid] = region
        self.levels[sid] = level
        ry, rx = np.nonzero(ring)
        gw = 8 if kind == 'city' else 5
        default = {'N': (x + w / 2, y - 10), 'S': (x + w / 2, y + h + 10), 'W': (x - 10, y + h / 2),
                   'E': (x + w + 10, y + h / 2)}
        for side, target in gates.items():
            tx, ty = target or default[side]
            i = int(np.argmin((rx - tx) ** 2 + (ry - ty) ** 2))
            gx, gy = int(rx[i]), int(ry[i])
            vx, vy = {'N': (0, -1), 'S': (0, 1), 'W': (-1, 0), 'E': (1, 0)}[side]
            # The opening: every wall tile across the gate's width, through the wall's whole depth.
            reach = gw + thick + 2
            yy, xx = np.mgrid[max(0, gy - reach):gy + reach + 1, max(0, gx - reach):gx + reach + 1]
            depth = (gx - xx) * vx + (gy - yy) * vy
            lateral = np.abs((xx - gx) * vy - (yy - gy) * vx)
            near = ring[yy, xx] & (lateral <= gw // 2) & (depth >= -1) & (depth <= thick + 1)
            c.paint(self._mask(yy[near], xx[near]), 'G', level)
            ox, oy = gx, gy
            for _ in range(40):
                ox, oy = ox + vx, oy + vy
                if not region[int(oy), int(ox)]:
                    break
            self.gates[(sid, side)] = (float(ox + vx * 4), float(oy + vy * 4))
        self.features.append((x + w // 2, y + h // 2, kind))

    @staticmethod
    def _mask(ys, xs):
        m = np.zeros((SIZE, SIZE), dtype=bool)
        m[ys, xs] = True
        return m

    # Roads -------------------------------------------------------------------------------------------------------
    STEP = 4                                # Roads are routed on a grid of 4-tile squares.

    def route_costs(self):
        """What it costs a road to cross each 4-tile square: water dearly (so the lake and the sea are gone round and
        rivers bridged only where needed), the shore a little (so roads keep a few tiles back from it), steep and high
        ground by its slope, and settlements not at all."""
        s, n = self.STEP, SIZE // self.STEP
        coarse = lambda a: a.reshape(n, s, n, s).mean(axis=(1, 3))
        wet = coarse((self.water & (self.lake | self.ocean)).astype(np.float32))
        river = coarse((self.water & ~(self.lake | self.ocean)).astype(np.float32))
        h = coarse(self.c.heights)
        gy, gx = np.gradient(h)
        slope = np.hypot(gx, gy) / s
        shore = grow(grow(wet > 0)) & ~(wet > 0)
        cost = 1 + 90 * wet + 20 * river + 3 * shore + 60 * slope ** 2 + .25 * np.maximum(0, h - 5)
        blocked = np.zeros((n, n), dtype=bool)
        for region in self.places.values():
            blocked |= coarse(region.astype(np.float32)) > 0
        if self.no_road is not None:                # The city's buildings outside its walls: the suburbs' houses.
            blocked |= coarse(self.no_road.astype(np.float32)) > 0
        cost[blocked] = np.inf
        return cost

    def route(self, a, b, cost):
        """The cheapest way from a to b over the grid (A*), as canvas points."""
        s, n = self.STEP, cost.shape[0]
        start = (min(n - 1, int(a[1]) // s), min(n - 1, int(a[0]) // s))
        goal = (min(n - 1, int(b[1]) // s), min(n - 1, int(b[0]) // s))

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
                if not (0 <= nxt[0] < n and 0 <= nxt[1] < n):
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
        for _ in range(3):                      # Chaikin: round the grid's corners into curves.
            smooth = [path[0]]
            for (x1, y1), (x2, y2) in zip(path, path[1:]):
                smooth += [(.75 * x1 + .25 * x2, .75 * y1 + .25 * y2), (.25 * x1 + .75 * x2, .25 * y1 + .75 * y2)]
            path = smooth + [path[-1]]
        return path

    def dry_point(self, point):
        """The nearest tile to `point` that is dry land outside every settlement (a road's waypoint)."""
        x, y = int(point[0]), int(point[1])
        taken = self.water.copy()
        for region in self.places.values():
            taken |= region
        for r in range(0, 200, 2):
            x0, y0 = max(0, x - r), max(0, y - r)
            ys, xs = np.nonzero(~taken[y0:y + r + 1, x0:x + r + 1])
            if len(xs):
                i = int(np.argmin((xs + x0 - x) ** 2 + (ys + y0 - y) ** 2))
                return (float(xs[i] + x0), float(ys[i] + y0))
        return point

    def lay_roads(self):
        cost = self.route_costs()
        for name, stops, straight in ROADS + self.city_roads:
            points = [self.gates[tuple(s.split('.'))] if isinstance(s, str) else s for s in stops]
            # A road that ends at a gate arrives at the gate's own level.
            ends = [self.gate_levels.get(tuple(s.split('.')), self.levels[s.split('.')[0]]) if isinstance(s, str)
                    else None for s in (stops[0], stops[-1])]
            if not straight:
                points = [points[0]] + [self.dry_point(q) for q in points[1:-1]] + \
                         [points[-1] if isinstance(stops[-1], str) else self.dry_point(points[-1])]
            if straight:
                self.road(name, points, causeway=True, ends=ends)
                continue
            line = []
            for a, b in zip(points, points[1:]):
                line += self.route(a, b, cost)[:-1]
            self.road_lines[name] = line + [points[-1]]
            self.road(name, line + [points[-1]], ends=ends)

    def road(self, name, points, width=4, causeway=False, ends=(None, None)):
        """A road that climbs no faster than half a step a tile, crossing water on a bridge."""
        c = self.c
        samples = []
        for (x1, y1), (x2, y2) in zip(points, points[1:]):
            n = max(1, int(math.hypot(x2 - x1, y2 - y1) * 2))
            for i in range(n):
                t = i / n
                samples.append((x1 + (x2 - x1) * t, y1 + (y2 - y1) * t))
        samples.append(points[-1])
        xy = np.array(samples, dtype=np.float32)
        ix = np.clip(xy[:, 0].astype(int), 0, SIZE - 1)
        iy = np.clip(xy[:, 1].astype(int), 0, SIZE - 1)
        ground = c.heights[iy, ix].astype(np.float32)
        wet = self.water[iy, ix]
        ground[wet] = 0
        kernel = np.ones(81, dtype=np.float32) / 81        # Smooth over about 40 tiles of road.
        padded = np.pad(ground, 40, mode='edge')
        profile = np.convolve(padded, kernel, mode='valid')
        profile[0], profile[-1] = ground[0], ground[-1]
        fixed = np.zeros(len(profile), dtype=bool)
        for level, part in zip(ends, (slice(0, 16), slice(-16, None))):
            if level is not None:                         # The last 8 tiles to a gate are at the gate's level.
                profile[part] = level
                fixed[part] = True
        step = .45 * .5                                    # Samples are half a tile apart.
        for _ in range(3):
            for i in range(1, len(profile)):
                if not fixed[i]:
                    profile[i] = np.clip(profile[i], profile[i - 1] - step, profile[i - 1] + step)
            for i in range(len(profile) - 2, -1, -1):
                if not fixed[i]:
                    profile[i] = np.clip(profile[i], profile[i + 1] - step, profile[i + 1] + step)
        mask = c.line_mask(samples[::2] + [samples[-1]], width)
        # Each tile near the road takes the profile of the nearest sample, and the land eases into it over a
        # dozen tiles either side so the road runs on a graded bench rather than between cliffs.
        reach = int(width / 2 + 16)
        best = np.full((SIZE, SIZE), 1e9, dtype=np.float32)
        hts = np.zeros((SIZE, SIZE), dtype=np.float32)
        for k in range(0, len(xy), 2):
            x0, y0 = max(0, int(xy[k, 0]) - reach), max(0, int(xy[k, 1]) - reach)
            x1, y1 = min(SIZE, int(xy[k, 0]) + reach + 1), min(SIZE, int(xy[k, 1]) + reach + 1)
            ys, xs = np.mgrid[y0:y1, x0:x1]
            d = (xs + .5 - xy[k, 0]) ** 2 + (ys + .5 - xy[k, 1]) ** 2
            box, boxh = best[y0:y1, x0:x1], hts[y0:y1, x0:x1]
            better = d < box
            box[better] = d[better]
            boxh[better] = profile[k]
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
        c.locked |= paint
        if causeway:
            self.causeway |= bridge

    # Points of interest -----------------------------------------------------------------------------------------
    def scatter_features(self):
        rng = self.rng
        spacing = 72
        taken = [(x, y) for x, y, _ in self.features]
        attempts = 0
        placed = 0
        cells = {}
        while attempts < 30000:
            attempts += 1
            x, y = rng.randrange(20, SIZE - 20), rng.randrange(20, SIZE - 20)
            if not self.buildable(x, y):
                continue
            if any((x - a) ** 2 + (y - b) ** 2 < spacing ** 2 for a, b in taken):
                continue
            self.feature(x, y)
            taken.append((x, y))
            placed += 1
            cells[(x // CELL, y // CELL)] = cells.get((x // CELL, y // CELL), 0) + 1
        # Every cell gets at least two, even if dart-throwing missed it.
        for cy in range(SIZE // CELL):
            for cx in range(SIZE // CELL):
                for _ in range(40):
                    if cells.get((cx, cy), 0) >= 2:
                        break
                    x, y = cx * CELL + rng.randrange(24, 232), cy * CELL + rng.randrange(24, 232)
                    if self.buildable(x, y) and all((x - a) ** 2 + (y - b) ** 2 > 40 ** 2 for a, b in taken):
                        self.feature(x, y)
                        taken.append((x, y))
                        cells[(cx, cy)] = cells.get((cx, cy), 0) + 1
        return placed

    def buildable(self, x, y):
        if self.water[y, x] or self.c.locked[y, x]:
            return False
        # Not within reach of a settlement or the city's grounds (their own fields and farms go there).
        return not any(region[max(0, y - 30):y + 31, max(0, x - 30):x + 31].any() for region in self.places.values())

    def feature(self, x, y):
        biome = BIOME_IDS[self.biome[y, x]]
        table = FEATURES.get(biome, FEATURES['temperate'])
        if self.c.heights[y, x] >= 9:
            table = ['cairn', 'outcrop', 'wolf_den', 'camp', 'ruins']
        kind = self.rng.choice(table)
        getattr(self, 'f_' + kind)(x, y, biome)
        self.features.append((x, y, kind))

    # The stamps: each lays a small place into the ground around (x, y).
    def put(self, x, y, ch, height=None):
        c = self.c
        if 0 <= x < SIZE and 0 <= y < SIZE and not c.locked[y, x] and not self.water[y, x]:
            c.codes[y, x] = ord(ch)
            if height is not None:
                c.heights[y, x] = height
            return True
        return False

    def disc(self, x, y, r, ch, height=None, ry=None):
        """An irregular patch about r (by ry) tiles across."""
        ry = ry or r
        inside = blob(r, ry, self.rng)
        for dy in range(-int(ry * 1.5) - 1, int(ry * 1.5) + 2):
            for dx in range(-int(r * 1.5) - 1, int(r * 1.5) + 2):
                if inside(dx, dy) <= 1:
                    self.put(x + dx, y + dy, ch, height)

    def ring(self, x, y, r, ch, step=1):
        for a in range(0, 360, step):
            self.put(int(round(x + r * math.cos(math.radians(a)))), int(round(y + r * math.sin(math.radians(a)))), ch)

    def level_here(self, x, y, r):
        self.flatten(x - r, y - r, 2 * r, 2 * r, 5)
        return self.c.heights[y, x]

    def f_ruins(self, x, y, biome):
        rng = self.rng
        lvl = self.level_here(x, y, 12)
        w, h = rng.randrange(10, 18), rng.randrange(8, 14)
        for i in range(w):
            for j in (0, h - 1):
                if rng.random() < .65:
                    self.put(x - w // 2 + i, y - h // 2 + j, '#', lvl)
        for j in range(h):
            for i in (0, w - 1):
                if rng.random() < .65:
                    self.put(x - w // 2 + i, y - h // 2 + j, '#', lvl)
        for _ in range(25):
            self.put(x + rng.randrange(-w, w), y + rng.randrange(-h, h), 'X')
        self.put(x, y, rng.choice('SCU'), lvl)

    def f_watchtower(self, x, y, biome):
        lvl = self.level_here(x, y, 6)
        for i in range(-3, 4):
            for j in (-3, 3):
                if self.rng.random() < .7:
                    self.put(x + i, y + j, '#', lvl)
                    self.put(x + j, y + i, '#', lvl)
        for _ in range(14):
            self.put(x + self.rng.randrange(-6, 7), y + self.rng.randrange(-6, 7), 'X')

    def f_stone_circle(self, x, y, biome):
        lvl = self.level_here(x, y, 9)
        r = self.rng.randrange(5, 8)
        self.disc(x, y, r + 1, {'north': '-', 'wet': '!'}.get(biome, 'f'), lvl)
        self.ring(x, y, r, '6', 360 // (r * 2))
        self.put(x, y, self.rng.choice('a6S'), lvl)

    def f_dead_circle(self, x, y, biome):
        lvl = self.level_here(x, y, 9)
        self.disc(x, y, 8, 'D', lvl)
        self.ring(x, y, 7, '2', 36)
        self.ring(x, y, 4, '6', 60)
        self.put(x, y, 'i', lvl)

    def f_camp(self, x, y, biome):
        lvl = self.level_here(x, y, 6)
        self.disc(x, y, 5, '.', lvl)
        self.put(x, y, '9', lvl)
        for dx, dy, ch in ((-2, 0, '7'), (2, 0, '7'), (0, -2, '7'), (-3, 3, 'z'), (3, 3, 'z'), (4, -3, 'x'), (5, -3, 'O')):
            self.put(x + dx, y + dy, ch, lvl)

    def f_pond(self, x, y, biome):
        r = self.rng.randrange(6, 12)
        water = {'north': 'J', 'swamp': 'w', 'deep': 'w'}.get(biome, '~')
        self.disc(x, y, r + 2, {'north': '-', 'wet': '!'}.get(biome, 'E'), ry=r)
        self.disc(x, y, r, water, 0, ry=max(3, r - 3))

    def f_frozen_lake(self, x, y, biome):
        r = self.rng.randrange(14, 24)
        self.disc(x, y, r + 3, '*', ry=r)
        self.disc(x, y, r, 'J', 0, ry=max(5, r - 6))
        self.disc(x + r // 3, y, max(3, r // 4), 'W', 0)

    def f_hot_spring(self, x, y, biome):
        self.disc(x, y, 9, 'r')
        self.disc(x, y, 7, '.', 0)
        self.disc(x, y, 4, '~', 0)
        self.ring(x, y, 7, 'o', 45)

    def f_ice_ridge(self, x, y, biome):
        a = self.rng.uniform(0, math.pi)
        for t in range(-25, 26):
            px, py = int(x + t * math.cos(a) + 3 * math.sin(t / 5)), int(y + t * math.sin(a))
            self.put(px, py, 'J')
            if abs(t) % 5 == 0:
                self.put(px + 1, py, '%')

    def f_grove(self, x, y, biome):
        tree = {'north': '1', 'swamp': 'Y', 'deep': '2', 'wet': '$'}.get(biome, self.rng.choice('YP'))
        r = self.rng.randrange(7, 13)
        inside = blob(r, r * self.rng.uniform(.6, 1), self.rng)
        for dy in range(-r - 4, r + 5):
            for dx in range(-r - 4, r + 5):
                if inside(dx, dy) <= 1 and self.rng.random() < .55:
                    self.put(x + dx, y + dy, tree)
        self.disc(x, y, 2, '3' if biome in ('temperate', 'warm', 'downs', 'fields') else ',')

    def f_great_tree(self, x, y, biome):
        self.disc(x, y, 6, '3' if biome != 'wet' else '&')
        self.disc(x, y, 2, 'Y' if biome != 'wet' else '$')
        self.ring(x, y, 8, '6', 90)

    def f_orchard(self, x, y, biome):
        lvl = self.level_here(x, y, 14)
        for dy in range(-10, 11, 3):
            for dx in range(-12, 13, 3):
                self.put(x + dx, y + dy, 'Y', lvl)
        for i in range(-13, 14):
            self.put(x + i, y - 12, '|')
            self.put(x + i, y + 12, '|')

    def f_flower_field(self, x, y, biome):
        self.disc(x, y, self.rng.randrange(12, 22), '3', ry=self.rng.randrange(8, 14))

    def f_shrine(self, x, y, biome):
        lvl = self.level_here(x, y, 6)
        self.disc(x, y, 4, 'f', lvl)
        self.put(x, y, 'S', lvl)
        self.put(x - 2, y + 2, 'i', lvl)
        self.put(x + 2, y + 2, 'i', lvl)

    def f_drowned_shrine(self, x, y, biome):
        self.disc(x, y, 6, 'D', 0)
        self.put(x, y, 'S', 0)
        self.ring(x, y, 4, 'I', 60)
        self.ring(x, y, 8, 'w', 20)

    def f_cairn(self, x, y, biome):
        self.disc(x, y, 3, 'r')
        self.disc(x, y, 1, 'o')
        self.put(x, y + 3, '6')

    def f_outcrop(self, x, y, biome):
        r = self.rng.randrange(8, 14)
        top = self.c.heights[y, x] + self.rng.choice([2, 2.5, 3])
        inside = blob(r, r / 1.3, self.rng)
        for dy in range(-r - 4, r + 5):
            for dx in range(-r - 5, r + 6):
                d = inside(dx, dy)
                if d <= 1:
                    ch = '%' if d > .72 else ('o' if self.rng.random() < .15 else ('!' if biome == 'wet' else 'r'))
                    self.put(x + dx, y + dy, ch, top)
        edge = next((i for i in range(r * 2, 0, -1) if inside(i, 0) <= 1), r)
        for i in range(3):                    # A way up.
            self.put(x + edge - i, y, ':', top - 1 + i * .5)

    def f_ravine(self, x, y, biome):
        a = self.rng.uniform(0, math.pi)
        length = self.rng.randrange(30, 60)
        bend, phase = self.rng.uniform(4, 10), self.rng.uniform(0, 6)
        for t in range(-length, length + 1):
            wobble = bend * math.sin(t / 13 + phase)
            cx, cy = x + t * math.cos(a) - wobble * math.sin(a), y + t * math.sin(a) + wobble * math.cos(a)
            base = self.c.heights[int(cy) % SIZE, int(cx) % SIZE] - 2.5
            for s in range(-3, 4):
                px, py = int(cx - s * math.sin(a)), int(cy + s * math.cos(a))
                if abs(s) == 3:
                    self.put(px, py, '%', base + 2.5)
                elif abs(s) <= 1:
                    self.put(px, py, '~' if biome != 'north' else 'J', base)
                else:
                    self.put(px, py, 'r', base)
        for t in (-length, length):                      # Walkable ends.
            wobble = bend * math.sin(t / 13 + phase)
            self.put(int(x + t * math.cos(a) - wobble * math.sin(a)), int(y + t * math.sin(a) + wobble * math.cos(a)), ':')

    def f_wolf_den(self, x, y, biome):
        top = self.c.heights[y, x] + 2
        inside = blob(7, 5, self.rng)
        for dy in range(-8, 9):
            for dx in range(-10, 11):
                d = inside(dx, dy)
                if d <= 1:
                    self.put(x + dx, y + dy, '%' if d > .6 else 'r', top)
        mouth = next((j for j in range(8, 0, -1) if inside(0, j) <= 1), 5)
        self.put(x, y + mouth, 'G', top - .5)         # The den's mouth.
        self.ring(x, y + mouth + 3, 3, 'o', 60)

    def f_farmstead(self, x, y, biome):
        lvl = self.level_here(x, y, 18)
        self.c.rect(x - 5, y - 4, 10, 7, '#', lvl)
        self.c.rect(x - 4, y - 3, 8, 5, 'L', lvl)
        self.put(x, y + 3, '.', lvl)
        for fx, fy in ((-17, -14), (2, -14), (-17, 6), (6, 6)):
            for dy in range(8):
                for dx in range(12):
                    self.put(x + fx + dx, y + fy + dy, '4', lvl)
        self.put(x + 7, y - 2, 'U', lvl)
        for i in range(-18, 19):
            self.put(x + i, y - 16, '|')
            self.put(x + i, y + 15, '|')

    def f_wagon(self, x, y, biome):
        for dx, dy, ch in ((0, 0, 'x'), (1, 0, 'x'), (2, 0, 'O'), (0, 1, '7'), (3, 1, 'X'), (-2, -1, 'X'), (1, 2, 'x')):
            self.put(x + dx, y + dy, ch)

    def f_sunken_ruins(self, x, y, biome):
        self.disc(x, y, 12, 'w', 0)
        for i in range(-8, 9):
            for j in (-6, 6):
                if self.rng.random() < .55:
                    self.put(x + i, y + j, '#', 0)
            if self.rng.random() < .5:
                self.put(x - 8, y + i // 2, '#', 0)
        for _ in range(12):
            self.put(x + self.rng.randrange(-7, 8), y + self.rng.randrange(-5, 6), 'X', .5)

    def f_boardwalk(self, x, y, biome):
        a = self.rng.uniform(0, math.pi)
        for t in range(-30, 31):
            cx = int(x + t * math.cos(a) + 3 * math.sin(t / 6))
            cy = int(y + t * math.sin(a))
            for s in (0, 1):
                px, py = cx + s, cy
                if 0 <= px < SIZE and 0 <= py < SIZE and not self.c.locked[py, px]:
                    self.c.codes[py, px] = ord('8')
                    self.c.heights[py, px] = 0
            if t % 3 == 0:
                self.put(cx - 2, cy, 'w', 0)
                self.put(cx + 3, cy, 'w', 0)

    def f_hummock(self, x, y, biome):
        r = self.rng.randrange(6, 10)
        self.disc(x, y, r + 3, 'w', 0)
        self.disc(x, y, r, ',', 1)
        self.disc(x, y, r - 2, 'Y' if self.rng.random() < .5 else '2', 1.5)
        self.put(x + r, y, ':', .5)

    # Output -----------------------------------------------------------------------------------------------------
    def build(self, city=True):
        self.lay_water()
        self.lay_biomes()
        self.lay_heights()
        for s in SETTLEMENTS:
            self.settlement(*s)
        if city:
            from . import cities
            self.city_site, self.cities, self.city_roads = cities.build(self)
        else:
            self.stand_in_city()
        self.lay_roads()
        self.lay_ground()
        placed = self.scatter_features()
        self.mark_cliffs()
        if city:
            from . import cities
            cities.build_quarters(self)
            self.mark_cliffs()
        return placed

    def stand_in_city(self):
        """For a look at the land alone: Ridgemere as a plain walled box with its three gates."""
        self.settlement(CITY, 'Ridgemere', 'city', (300, 220, 300, 260),
                        {'N': (450, 200), 'E': (620, 350), 'S': (450, 500)}, True)

    def mark_cliffs(self):
        """Wherever a drop cannot be walked, the higher tile is a cliff face (unless something was built there)."""
        c = self.c
        h = c.heights
        higher = np.zeros((SIZE, SIZE), dtype=bool)
        higher[:, :-1] |= h[:, :-1] - h[:, 1:] > 1
        higher[:, 1:] |= h[:, 1:] - h[:, :-1] > 1
        higher[:-1, :] |= h[:-1, :] - h[1:, :] > 1
        higher[1:, :] |= h[1:, :] - h[:-1, :] > 1
        ramp = np.isin(c.codes, [ord(':'), ord('^'), ord('G')])
        c.codes[higher & ~c.locked & ~self.water & ~ramp] = ord('%')

    def cells(self):
        """Atlas cells for every 256-tile square that holds land."""
        c = self.c
        out = []
        for cy in range(SIZE // CELL):
            for cx in range(SIZE // CELL):
                x0, y0 = cx * CELL, cy * CELL
                codes = c.codes[y0:y0 + CELL, x0:x0 + CELL]
                if (codes == ord('W')).mean() > .97:
                    continue                      # Open sea: the world simply ends.
                region = min(REGIONS, key=lambda r: (r[1] - x0 - 128) ** 2 + (r[2] - y0 - 128) ** 2)
                if self.city_cells and (cx, cy) in self.city_cells:
                    region = next(r for r in REGIONS if r[0] == 'Ridgemere')
                rows, heights = c.cell(x0, y0, CELL, CELL)
                land = ~self.water[y0:y0 + CELL, x0:x0 + CELL]
                biome_here = np.bincount(self.biome[y0:y0 + CELL, x0:x0 + CELL][land].ravel(), minlength=len(BIOME_IDS))
                main = BIOME_IDS[int(biome_here.argmax())]
                out.append({'region': region[0], 'blurb': region[3], 'cx': cx, 'cy': cy, 'biome': main,
                            'x': x0 + OX, 'y': y0 + OY, 'terrain': rows, 'heights': heights,
                            'weather': WEATHER.get(main, 'clear')})
        # Names: the region's name, and a quarter of it where a region spans several cells.
        by_region = {}
        for cell in out:
            by_region.setdefault(cell['region'], []).append(cell)
        cells = []
        for region, group in by_region.items():
            mx = sum(g['cx'] for g in group) / len(group)
            my = sum(g['cy'] for g in group) / len(group)
            used = {}
            for g in group:
                dx, dy = g['cx'] - mx, g['cy'] - my
                quarter = ' '.join(filter(None, ['North' if dy < -.4 else 'South' if dy > .4 else '',
                                                 'West' if dx < -.4 else 'East' if dx > .4 else ''])) or 'Heart'
                used[quarter] = used.get(quarter, 0) + 1
                if used[quarter] > 1:
                    quarter += f' {"I" * used[quarter]}'
                name = region if len(group) == 1 else f'{region}, {quarter}'
                slug = ''.join(ch if ch.isalnum() else '_' for ch in region.lower().replace('the ', '')).strip('_')
                slug = slug.replace('__', '_')
                cells.append({'id': f'w_{slug}_{g["cx"]}_{g["cy"]}', 'name': name, 'description': g['blurb'],
                              'x': g['x'], 'y': g['y'], 'width': CELL, 'height': CELL, 'z': 0, 'outdoors': True,
                              'weather': g['weather'], 'territory': {'region': slug, 'claims': [], 'chapter': ''},
                              'terrain': g['terrain'], 'heights': g['heights']})
        return cells

    city_cells = ()
    no_road = None


def render(world, cells, folder: Path):
    """world.png (the whole world, a pixel per two tiles) and world_full.png (a pixel a tile), labelled."""
    folder.mkdir(parents=True, exist_ok=True)
    c = world.c
    np.savez_compressed(folder / 'canvas.npz', codes=c.codes, heights=c.heights, biome=world.biome)
    overview = [{'x': cell['x'] - OX, 'y': cell['y'] - OY, 'width': CELL, 'height': CELL, 'name': cell['name'][:30]}
                for cell in cells]
    labels = [((x - 20, y - 14), n) for n, x, y in
              [(name, box[0], box[1]) for _, name, _, box, _, _ in SETTLEMENTS]]
    preview.overview(c.codes, c.heights, overview, folder / 'world_full.png', scale=1, relief=2.2, labels=labels)
    image = Image.open(folder / 'world_full.png')
    image.resize((1024, 1024), Image.LANCZOS).save(folder / 'world.png')
    # Ridgemere and its estates, closer.
    x0, y0, x1, y1 = 270, 40, 900, 760
    preview.overview(c.codes[y0:y1, x0:x1], c.heights[y0:y1, x0:x1], [], folder / 'ridgemere_region.png', scale=2,
                     relief=1.6)
    preview.glyphs(c.codes, c.heights, folder / 'ridgemere_city.png', 286, 186, 360, 410, px=6)


GENERATED = ('rm_', 'tn_', 'rmx_', 'cty_')
ECONOMY = {'treasury': 20000, 'storeHerbs': 1500, 'storeMeals': 900, 'dailyHerbs': 150, 'dailyMeals': 200}


def claims_for(record):
    """Who holds a building of Ridgemere (faction IDs)."""
    name, district = record['name'], record['district']
    if district.startswith('estate_'):
        return ['house_' + district.split('_')[1]]
    for house in ('Greatly', 'Grayrock', 'Fell', 'Mossgrave'):
        if house in name:
            return ['house_' + house.lower()]
    if name in ('The Watch House', 'The Gaol', 'The Heights Watch Post', 'The City Arsenal'):
        return ['council_of_houses', 'ridgemere_watch']
    return ['council_of_houses']


def assemble(base, world, cells, report=print):
    """The new world as a project: the realm's cells, Ridgemere's interiors, doors, people, routes and factions, the
    towns built out and peopled, and the country's sites. `base` (DEV's project) gives only its ID, chapters and
    profession slots: nothing of the old world's places or people is kept."""
    from . import citizens, country, towns
    site = world.city_site
    by_grid = {f'@{(c["x"] - OX) // CELL}_{(c["y"] - OY) // CELL}': c['id'] for c in cells}
    links = []
    for link in site.links:
        link = json.loads(json.dumps(link))
        for end in ('a', 'b'):
            if link[end]['cell'].startswith('@'):
                link[end]['cell'] = by_grid[link[end]['cell']]
        links.append(link)
    rooms = [{**r, 'worldX': r['worldX'] + OX, 'worldY': r['worldY'] + OY} for r in site.rooms]
    city = world.cities[0]
    office = city.spots['office']
    spawn_cell = by_grid[f'@{int(office[0]) // CELL}_{int(office[1]) // CELL}']
    sx, sy = int(office[0]) % CELL, int(office[1]) % CELL + 12
    project = {'format': 'ratw-atlas', 'version': 3, 'id': base.get('id', 'ridgemere'), 'name': 'Ridgemere',
               'cells': cells, 'rooms': rooms, 'links': links,
               'spawn': {'cell': spawn_cell, 'x': sx, 'y': sy}, 'factions': list(citizens.FACTIONS),
               'chapters': base.get('chapters', []), 'people': [], 'routes': [], 'economy': dict(ECONOMY),
               'slots': base.get('slots', [])}
    people, routes = citizens.populate(project, site.manifest, world)
    project['people'], project['routes'] = people, routes
    report(f'ridgemere: {len(site.manifest)} buildings, {len(rooms)} interiors, {len(people)} people')
    # Claims: the city's buildings by who holds them; its cells by the city, and the estates' by their Houses.
    room_claims = {room['id']: claims_for(record) for record in site.manifest for room in record['rooms']}
    for r in project['rooms']:
        if r['id'] in room_claims:
            r['territory'] = {**r['territory'], 'claims': room_claims[r['id']]}
    cell_claims = {}
    for sid, region in world.places.items():
        if sid == CITY or sid.startswith('estate_'):
            ys, xs = np.nonzero(region)
            key = f'@{int(xs.mean()) // CELL}_{int(ys.mean()) // CELL}'
            add = ['council_of_houses', 'ridgemere_watch'] if sid == CITY else ['house_' + sid[7:]]
            cell_claims.setdefault(by_grid[key], set()).update(add)
    for c in project['cells']:
        if c['id'] in cell_claims:
            c['territory'] = {**c['territory'], 'claims': sorted(cell_claims[c['id']])}
    # The towns built out and peopled, Ridgemere filled out, and the country's farms and works.
    project, _ = towns.build_all(project, report=lambda line: report(f'  towns: {line}'))
    project, _ = country.build(project, report=lambda line: report(f'  country: {line}'))
    # The herb patch: by the Southway, outside the city.
    gx, gy = world.gates[(CITY, 'S')]
    from .residents import People
    P = People(project, [], random.Random(SEED))
    project['herbPatch'] = P.world(int(gx) + OX + 30, int(gy) + OY + 40, share=False)
    return project


def retire_ids(project, old_ids, keep_ids):
    """Gives every regenerated resident whose ID someone in the old world had a new one (a trailing underscore, as the
    generators make a taken ID unique). Returns {old: new}."""
    taken = set(old_ids) | {p['id'] for p in project['people']}
    renamed = {}
    for p in project['people']:
        if p['id'] in old_ids and p['id'] not in keep_ids:
            new = p['id']
            while new in taken:
                new += '_'
            taken.add(new)
            renamed[p['id']] = new
            p['id'] = new
    return renamed


def clear_old_state(conn, world_id, gone):
    """The saved game state of residents no longer in the world (`gone`: their IDs): their bodies, bonds, rumours they
    held or that were about them, NPC memories and their DM-set state. Players' and everyone else's stay. The game drops
    their lives and purses itself when it next loads its save. Returns rows deleted, by table."""
    gone = sorted(gone)
    out = {}
    for table, where in (('live.npc_state', 'npc_id = ANY(%s)'),
                         ('game.npcs', 'key = ANY(%s)'),
                         ('game.bonds', "(data->>'holder' = ANY(%s) OR data->>'other' = ANY(%s))"),
                         ('game.beliefs', "(data->>'holder' = ANY(%s) OR data->>'subject' = ANY(%s))"),
                         ('game.npc_memories', "data->>'npc' = ANY(%s)")):
        args = (world_id, gone) + ((gone,) if where.count('%s') == 2 else ())
        out[table] = conn.execute(f'DELETE FROM {table} WHERE world_id = %s AND {where}', args).rowcount
    return out


def saved_residents(world_id):
    """The residents DEV's game save knows ({ID: home cell}), and those it still has a body for. Whatever world they lived
    in, a newcomer mustn't wake up in one's saved life."""
    import world_db
    with world_db.connect('dev', 'owner') as conn:
        row = conn.execute('SELECT payload FROM game.checkpoints WHERE world_id = %s', (world_id,)).fetchone()
        bodies = {r[0] for r in conn.execute('SELECT key FROM game.npcs WHERE world_id = %s', (world_id,)).fetchall()}
    if not row:
        return {}, bodies
    payload = row[0] if isinstance(row[0], dict) else json.loads(row[0])
    return {rid: life.get('homeCell', '') for rid, life in payload.get('society', {}).get('residents', {}).items()}, bodies


def dev_project():
    import world_db
    import world_store
    with world_db.connect('dev', 'editor') as conn:
        wid = world_store.list_worlds(conn)[0]['id']
        return world_store.load_world(conn, wid)


def render_project(project, folder: Path):
    """The finished world as the cells hold it (the towns and the country's sites built in): world_built.png, a pixel
    a tile, and towns/<town>.png close up."""
    from .canvas import DEFAULT_HEIGHT
    codes = np.full((SIZE, SIZE), ord('W'), dtype=np.uint8)
    heights = np.zeros((SIZE, SIZE), dtype=np.float32)
    for c in project['cells']:
        x0, y0 = c['x'] - OX, c['y'] - OY
        for y, row in enumerate(c['terrain']):
            codes[y0 + y, x0:x0 + len(row)] = np.frombuffer(row.encode('ascii'), dtype=np.uint8)
        heights[y0:y0 + c['height'], x0:x0 + c['width']] = DEFAULT_HEIGHT[codes[y0:y0 + c['height'], x0:x0 + c['width']]]
        for key, value in c['heights'].items():
            x, y = map(int, key.split(','))
            heights[y0 + y, x0 + x] = value
    overview = [{'x': c['x'] - OX, 'y': c['y'] - OY, 'width': CELL, 'height': CELL, 'name': c['name'][:30]}
                for c in project['cells']]
    preview.overview(codes, heights, overview, folder / 'world_built.png', scale=1, relief=2.2)
    (folder / 'towns').mkdir(parents=True, exist_ok=True)
    for sid, name, _, (x, y, w, h), _, _ in SETTLEMENTS:
        x0, y0 = max(0, x - 150), max(0, y - 150)
        preview.overview(codes[y0:y + h + 150, x0:x + w + 150], heights[y0:y + h + 150, x0:x + w + 150], [],
                         folder / 'towns' / f'{sid}.png', scale=3, relief=1.4)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--preview', type=Path, help='Write preview images here')
    parser.add_argument('--land', action='store_true', help='The land alone (Ridgemere as a plain walled box)')
    parser.add_argument('--out', type=Path, help='Write the whole project here as JSON')
    parser.add_argument('--import-dev', action='store_true', help='Replace DEV\'s world with this one (backed up first)')
    args = parser.parse_args(argv)
    world = Realm()
    placed = world.build(city=not args.land)
    cells = world.cells()
    print(f'{len(cells)} cells, {placed} scattered points of interest, {len(SETTLEMENTS)} towns')
    if args.preview:
        render(world, cells, args.preview)
    if args.land:
        return 0
    base, revision = dev_project() if args.import_dev else ({'id': 'upper_accord'}, None)
    project = assemble(base, world, cells)
    roles = {}
    for person in project['people']:
        roles[person['role']] = roles.get(person['role'], 0) + 1
    print(f'{len(project["people"])} residents {roles}; {len(project["rooms"])} interiors; {len(project["links"])} '
          f'links; {len(project["routes"])} routes; height overrides {sum(len(c["heights"]) for c in cells)}')
    import map_editor
    try:
        map_editor.check_project(project, for_game=False)
        print('Atlas validation: OK')
    except map_editor.ValidationError as error:
        print('Atlas validation failed:', *error.errors[:30], sep='\n  ')
        return 1
    if args.preview:
        render_project(project, args.preview)
    if args.out:
        args.out.mkdir(parents=True, exist_ok=True)
        (args.out / 'project.json').write_text(json.dumps(project), encoding='utf-8')
        (args.out / 'roads.json').write_text(json.dumps(world.road_lines), encoding='utf-8')
        (args.out / 'buildings.json').write_text(json.dumps(world.city_site.manifest, indent=1), encoding='utf-8')
    if args.import_dev:
        import time
        import world_db
        import world_store
        saved, bodies = saved_residents(base['id'])
        old_ids = {p['id'] for p in base.get('people', [])} | set(saved)
        # Someone this world already had stays: the same ID and name (a world made by this generator, made again), and,
        # if the game's save knows the ID, it still has their body, in the same home (else it is someone else's saved
        # life, or one already let go).
        same = {(p['id'], p['name']) for p in base.get('people', [])}
        kept = {p['id'] for p in project['people'] if (p['id'], p['name']) in same
                and (p['id'] not in saved or (p['id'] in bodies and saved[p['id']] == p['home']['cell']))}
        renamed = retire_ids(project, old_ids, kept)
        print(f'{len(kept)} residents kept; {len(renamed)} newcomers given IDs no one in the world before had')
        folder = Path(__file__).resolve().parents[2] / 'artifacts/backups'
        folder.mkdir(parents=True, exist_ok=True)
        backup = folder / f'{base["id"]}_dev_r{revision}_before_realm_{time.strftime("%Y%m%d-%H%M%S")}.atlas.json'
        backup.write_text(json.dumps(base, ensure_ascii=False), encoding='utf-8')
        print(f'DEV revision {revision} backed up to {backup}.')
        with world_db.connect('dev', 'editor') as conn:
            new = world_store.save_world(conn, project, revision)
        print(f'Saved to DEV, revision {new}.')
        with world_db.connect('dev', 'owner') as conn:
            with conn.transaction():
                cleared = clear_old_state(conn, project['id'], old_ids - kept)
        print(f'The old residents\' saved state cleared: {cleared}.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
