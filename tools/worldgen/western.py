"""The western world around Upper Accord, from the author's drawing (western_world_map.png).

The drawing is 2048 pixels square; at the scale where Upper Accord's region matches its place on it, one pixel is
1.25 tiles, so the world is a 2560 x 2560 canvas of 256-tile cells. Upper Accord's existing 768 x 768 region sits in
its east edge and is left as it is (apart from the springs on Southern Peak that feed the Eastern Vale's river); the
terrain around it is blended to meet its edges so the two join.

Biomes follow the drawing's colours: cold and wintery in the north, rain-soaked cedar coast around Ridgemere,
temperate through the middle, grassland and plain in the south with golden grain country around Ser Ferro, swamp and
deep swamp in the south-east. Biomes fade into one another over a hundred-odd tiles; each breaks into sub-biome
patches, and points of interest are scattered so no cell is empty ground. Cities, towns and fortresses are
placeholder walls with open ground inside, each within one cell. Roads are routed around lakes and the sea and bridge
rivers where they must.

  cd tools && python3 -m worldgen.western [--preview DIR] [--out DIR] [--import-dev]
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

DRAWING = Path('/home/martinb/Pictures/western_world_map.png')
SIZE = 2560
CELL = 256
OX, OY = -1792, -512                    # World tile of canvas (0, 0): Upper Accord keeps its coordinates.
UA = (1792, 512, 768, 768)              # Upper Accord's region on the canvas (x, y, width, height).
SEED = 20260927
PX = 1.28                               # Canvas tiles per pixel of the 2000-pixel view the plan was drawn on.

BIOMES = {  # id: drawing colour (None: not drawn, laid out from the plan)
    'water': (0, 176, 240), 'north': (162, 221, 114), 'temperate': (118, 187, 62), 'plains': (168, 219, 38),
    'swamp': (112, 145, 53), 'deep': (83, 99, 41), 'highland': (102, 127, 82), 'wet': None, 'golden': None,
}
BIOME_IDS = list(BIOMES)
DRAWN = [b for b, colour in BIOMES.items() if colour]
BLEND = 260                             # Biomes fade into each other over roughly twice this many tiles.


def p(x, y):
    """A point on the plan (2000-pixel view of the drawing) as canvas tiles."""
    return x * PX, y * PX


def pts(*coords):
    return [p(x, y) for x, y in coords]


def bid(name):
    return BIOME_IDS.index(name)


# --- The plan, traced from the drawing ---------------------------------------------------------------------------
RANGES = {  # name: [(plan x, plan y, height, spread in tiles)]; each range's peaks are joined by ridges
    'Whitecrown': [(565, 45, 15, 70), (675, 40, 16, 75), (800, 40, 16, 80), (930, 30, 15, 70), (750, 80, 14, 60),
                   (870, 70, 14, 60), (820, 120, 13, 55), (640, 160, 13, 55), (560, 200, 12, 55), (505, 170, 13, 55)],
    'Ridgemere spine': [(450, 280, 13, 55), (500, 330, 13, 55), (430, 330, 12, 50), (480, 380, 12, 50),
                        (385, 400, 11, 50), (440, 440, 11, 50)],
    'Westwall': [(320, 460, 12, 50), (260, 490, 12, 50), (250, 550, 12, 50), (300, 600, 13, 55), (245, 630, 12, 50),
                 (290, 670, 12, 50), (235, 690, 11, 50), (290, 735, 12, 50), (240, 790, 11, 50), (300, 820, 11, 45),
                 (370, 780, 10, 45), (375, 850, 11, 45), (440, 900, 10, 45), (530, 920, 11, 45), (490, 960, 10, 45),
                 (545, 1015, 10, 45)],
    'Eastwall': [(540, 440, 11, 50), (600, 490, 12, 50), (630, 550, 12, 50), (680, 600, 12, 50), (740, 640, 11, 45),
                 (690, 680, 12, 50), (730, 720, 11, 45), (670, 760, 12, 50), (730, 800, 11, 45), (790, 830, 10, 45),
                 (670, 840, 11, 45)],
    'South-western knot': [(50, 930, 11, 50), (135, 875, 12, 50), (120, 960, 10, 45), (190, 930, 10, 45)],
    'Isle': [(140, 195, 12, 60)],
}
HILLS = [  # (plan x, plan y, height, spread): neighbouring hills are joined into ridges of downs
    (1060, 50, 5, 45), (990, 110, 5, 45), (880, 175, 4, 45), (740, 200, 4, 45), (650, 230, 4, 40), (390, 525, 4, 35),
    (770, 885, 4, 40), (730, 920, 4, 40), (860, 935, 4, 40), (790, 975, 4, 40), (600, 1080, 4, 40), (660, 1105, 4, 40),
    (630, 1145, 4, 40), (225, 990, 4, 40), (130, 1025, 4, 40), (40, 1015, 4, 40), (250, 1035, 4, 40), (370, 1100, 5, 45),
    (290, 1145, 5, 45), (210, 1190, 4, 40), (530, 1225, 4, 40), (530, 1300, 4, 40), (485, 1355, 4, 40), (440, 1420, 4, 40),
    (350, 1480, 4, 40), (300, 1545, 4, 40), (265, 1610, 4, 40), (215, 1675, 4, 40),
]
WET = ((380, 260), 680, 720)            # The rain coast around Ridgemere: centre on the plan, reach in tiles.
GOLDEN = ((200, 1850), 820, 700)        # Ser Ferro's grain country: centre on the plan, reach in tiles.

# Settlements, each inside one cell: id, name, kind, (x, y, w, h) on the canvas, {gate side: the point the gate faces},
# and whether its wall follows the shore (keeping to the land inside the box) instead of the box.
SETTLEMENTS = [
    ('ridgemere', 'Ridgemere', 'city', (262, 6, 244, 244), {'S': (480, 262), 'E': (512, 134)}, True),
    ('ser_ferro', 'Ser Ferro', 'city', (12, 2316, 232, 232), {'N': (128, 2304), 'E': (256, 2430)}, True),
    ('northern_fortress', 'The Northern Fortress', 'fortress', (1580, 156, 84, 84), {'S': None, 'E': None}, False),
    ('ghost_town', 'The Ghost Town', 'ghost', (2320, 100, 70, 56), {'S': None, 'W': None}, False),
    ('saltreach', 'Saltreach', 'town', (150, 555, 70, 56), {'E': None}, True),
    ('hollowmere_village', 'Hollowmere', 'village', (628, 1066, 60, 48), {'N': None, 'S': None}, False),
    ('lakeside', 'Lakeside', 'town', (1286, 1284, 120, 96), {'N': (1330, 1270), 'E': (1460, 1330)}, True),
    ('isle_fortress', 'The Isle Fortress', 'fortress', (1552, 1308, 84, 84), {'W': None}, False),
    ('westmarch', 'Westmarch', 'town', (520, 1562, 70, 56), {'N': None, 'E': None}, False),
    ('accord_crossing', 'Accord Crossing', 'town', (1680, 954, 70, 56), {'N': None, 'S': None}, False),
    ('fenhollow', 'Fenhollow', 'town', (2330, 1984, 70, 56), {'E': None, 'W': None}, False),
    ('amberford', 'Amberford', 'town', (1630, 1960, 70, 56), {'N': None, 'W': None}, False),
    ('cinderbrook', 'Cinderbrook', 'town', (40, 2200, 70, 56), {'E': None}, False),
    ('dark_fortress', 'The Dark Fortress', 'fortress', (1590, 2420, 90, 90), {'E': None}, False),
]
UA_ENTRY = (1791, 942)                   # Where Upper Accord's main road leaves its region, on the canvas.
# Roads: name, waypoints ('<settlement>.<side>' is that gate), and whether it goes straight (a causeway). Between
# waypoints the road finds its own way, keeping off lakes and the sea and bridging rivers where it must.
ROADS = [
    ('Ridgemere Southway', ['ridgemere.S', *pts((380, 300), (300, 380), (160, 490), (150, 700), (160, 820), (220, 900),
                                              (330, 960), (420, 1050), (460, 1150), (430, 1280), (330, 1410), (200, 1540),
                                              (130, 1680), (180, 1800)), 'ser_ferro.N'], False),
    ('The Accord Road', ['ridgemere.E', *pts((600, 370), (640, 470), (690, 545), (780, 545), (880, 650), (950, 680),
                                            (1010, 675), (1110, 635), (1300, 690), (1360, 715)), UA_ENTRY], False),
    ('The Fortress Road', ['northern_fortress.S', *pts((1390, 300), (1395, 400), (1380, 560), (1360, 715))], False),
    ('The Crossing Spur', ['accord_crossing.N', p(1360, 715)], False),
    ('The Southern Road', ['accord_crossing.S', (1770, 1180), (1925, 1470), *pts((1580, 1330), (1450, 1420), (1250, 1560), (1100, 1640), (900, 1700),
                                                    (600, 1770), (360, 1860)), 'ser_ferro.E'], False),
    ('The Valley Road', [*pts((420, 1050), (560, 1180), (700, 1320), (900, 1420), (1100, 1520)), 'amberford.W'], False),
    ('The Amberford Spur', ['amberford.N', p(1250, 1560)], False),
    ('The Fen Road', [p(1580, 1330), *pts((1650, 1400), (1780, 1440)), 'fenhollow.W'], False),
    ('The Fen Road South', ['fenhollow.E', *pts((1815, 1700), (1810, 1800), (1760, 1990))], False),
    ('The Dark Road', [p(1810, 1800), *pts((1600, 1830), (1400, 1860)), 'dark_fortress.E'], False),
    ('The Lake Causeway', ['lakeside.E', 'isle_fortress.W'], True),
    ('The Lake Road', ['lakeside.N', p(1110, 635)], False),
    ('The Valley Path', ['hollowmere_village.N', (650, 900), (600, 760), p(500, 520), p(470, 260)], False),
    ('The Valley Path South', ['hollowmere_village.S', p(420, 1050)], False),
    ('The Saltreach Spur', ['saltreach.E', p(160, 490)], False),
    ('The Westmarch Spur', ['westmarch.N', p(460, 1150)], False),
    ('The Cinderbrook Road', ['cinderbrook.E', 'ser_ferro.N'], False),
]
REGIONS = [  # (name, plan x, plan y, blurb): cells are named for the nearest
    ('Ridgemere Heights', 380, 150, 'Rain-soaked cedar forest, moss and grey stone around Ridgemere, where the rain '
                                    'seldom stops.'),
    ('The Whitecrown', 780, 70, 'The high northern range: snowfields, ice-bound cliffs and bitter passes.'),
    ('The Frostmarch', 1100, 250, 'Open cold country of tundra and taiga, frozen ponds and bitter wind.'),
    ('Bleakwatch Moor', 1350, 150, 'Snow-heath and rock around the Northern Fortress, long-watched and empty.'),
    ('The Ghostwind Barrens', 1800, 180, 'White, silent barrens where an abandoned town still stands.'),
    ('The Isle of Grey Horns', 140, 230, 'A lone mountain isle off the rain coast, wrapped in mist, moss and cedar.'),
    ('The Saltreach Coast', 110, 560, 'A wet western shore of mossy bluffs, coves and grey water.'),
    ('The Westwall', 260, 650, 'The western wall of the valley: forested flanks and bare peaks.'),
    ('The Eastwall', 700, 700, 'A long ridge closing the valley from the east.'),
    ('Hollowmere Valley', 480, 700, 'A green, sheltered valley between two ridges, with a quiet village.'),
    ('The Greenholt', 1000, 500, 'Rolling temperate country of woods, meadows and old farms.'),
    ('The Accord Marches', 1250, 800, 'Farms and woodland along the roads below Upper Accord.'),
    ('The Mirrormere Shore', 950, 1100, 'The wooded and reedy shores of the great lake.'),
    ('Mirrormere', 1180, 1250, 'The great lake, grey-blue and deep, with an island fortress at its heart.'),
    ('The Southwold Downs', 300, 1200, 'Long rolling downs and hill country west of the plains.'),
    ('The Sunreach', 800, 1500, 'Wide southern plains of tall grass, wildflowers and scattered stone.'),
    ('The Amber Steppe', 1350, 1600, 'Dry golden steppe, mesas and scrub along the southern road.'),
    ('The Ser Ferro Marches', 250, 1750, 'Golden grain fields, hedgerows and rolling sunlit hills around Ser Ferro.'),
    ('The Riverlands', 700, 1900, 'Flood meadows and reed banks along the great river.'),
    ('The Eastern Vale', 1750, 1150, 'Green country falling from Upper Accord toward the rivers; a tarn below '
                                     'Southern Peak feeds the vale\'s river.'),
    ('The Mirelands', 1800, 1450, 'Reed marsh and bog where the rivers slow and spread.'),
    ('The Drowned Deep', 1550, 1850, 'The deep swamp: black water, drowned forest and the Dark Fortress.'),
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
                  ('Farmland', {',': 6, '"': 1}, 'Y', .005, []),
                  ('Pine woods', {',': 5, '.': 1}, 'P', .36, [('7', .006)]),
                  ('Old wood', {',': 5, '"': 2}, 'Y', .45, [('B', .03), ('7', .01)])],
    'plains': [('Prairie', {'"': 6, ';': 3}, 'Y', .002, []),
               ('Short grass', {';': 6, ',': 3}, None, 0, [('o', .001)]),
               ('Wildflower steppe', {';': 5, '3': 3, ',': 1}, None, 0, []),
               ('Scrubland', {';': 6, '.': 1}, 'B', .10, [('o', .004)]),
               ('Savanna', {';': 6, '"': 2}, 'Y', .012, [('B', .01)]),
               ('Dry flats', {';': 4, '.': 3, 'X': 1}, None, 0, [('o', .003)])],
    'golden': [('Grain country', None, None, 0, []),                  # The field patchwork (golden_fields).
               ('Golden meadow', {';': 6, '3': 2, '"': 1}, 'Y', .006, [('B', .004)]),
               ('Grain country', None, None, 0, []),
               ('Orchard hills', {',': 4, ';': 3, '3': 1}, 'Y', .07, [('B', .01)])],
    'swamp': [('Reed marsh', {',': 3, 'E': 4, 'w': 2}, None, 0, []),
              ('Bog', {'w': 3, 'D': 3, ',': 2}, '2', .05, []),
              ('Mire woods', {',': 3, 'D': 2, 'w': 1}, 'Y', .18, [('2', .06)]),
              ('Mud flats', {'D': 5, 'w': 2, 'E': 1}, None, 0, [])],
    'deep': [('Drowned forest', {'w': 4, 'D': 3}, '2', .22, [('E', .05)]),
             ('Black mire', {'w': 6, 'E': 2, 'D': 1}, '2', .05, []),
             ('Sunken woods', {'w': 3, 'D': 3, ',': 1}, 'Y', .22, [('2', .05)])],
}
FEATURES = {
    'north': ['frozen_lake', 'ruins', 'stone_circle', 'camp', 'outcrop', 'wolf_den', 'hot_spring', 'ice_ridge',
              'cairn', 'grove', 'ravine'],
    'wet': ['ruins', 'grove', 'camp', 'pond', 'shrine', 'great_tree', 'outcrop', 'wolf_den', 'ravine', 'cairn',
            'stone_circle', 'watchtower'],
    'temperate': ['ruins', 'stone_circle', 'camp', 'pond', 'grove', 'outcrop', 'farmstead', 'great_tree', 'shrine',
                  'ravine', 'watchtower', 'wolf_den', 'orchard'],
    'plains': ['mesa', 'great_tree', 'stone_circle', 'camp', 'pond', 'farmstead', 'ruins', 'wagon', 'watchtower',
               'flower_field', 'cairn', 'outcrop', 'dry_creek'],
    'golden': ['farmstead', 'farmstead', 'orchard', 'great_tree', 'wagon', 'pond', 'shrine', 'flower_field',
               'watchtower', 'stone_circle', 'camp'],
    'swamp': ['sunken_ruins', 'boardwalk', 'hummock', 'dead_circle', 'camp', 'pond', 'grove'],
    'deep': ['sunken_ruins', 'boardwalk', 'hummock', 'dead_circle', 'drowned_shrine'],
}
# The Eastern Vale's river rises in a tarn below Southern Peak (one of Upper Accord's cells), fed by streams from
# springs on the peak's southern flank. Points are canvas tiles; the springs are in Upper Accord's region.
TARN = (2246, 1382, 46, 30)             # centre x, y, reach x, y
SPRINGS = [(2102, 1217), (2162, 1210), (2222, 1217)]
STREAM_MOUTHS = [(2217, 1279), (2242, 1279), (2267, 1279)]   # Where each stream leaves Upper Accord's region.
RIVER_FROM = 1400                       # The drawn river is redrawn from the tarn down to here...
RIVER_TO = 1700                         # ...where it keeps the drawing's course and width.


# --- The drawing -------------------------------------------------------------------------------------------------
def read_drawing():
    """Biome of every canvas tile, from the drawing's colours."""
    image = Image.open(DRAWING).convert('RGB').resize((SIZE, SIZE), Image.NEAREST)
    rgb = np.asarray(image).astype(np.int32)
    palette = np.array([BIOMES[b] for b in DRAWN], dtype=np.int32)
    d = ((rgb[:, :, None, :] - palette[None, None, :, :]) ** 2).sum(-1)
    label = np.array([bid(b) for b in DRAWN], dtype=np.int8)[d.argmin(-1)]
    unsure = d.min(-1) > 45 ** 2                       # Ink, labels and markers: take the colour around them.
    for _ in range(5):                                  # ...and the anti-aliased fringe of the ink, which reads as
        unsure = grow(unsure)                           # dark swamp or grey highland.
    ys, xs = np.mgrid[0:SIZE, 0:SIZE]
    swampish = np.isin(label, [bid('swamp'), bid('deep')])
    unsure |= swampish & ((ys < 1600) | (xs < 1300))     # The drawn swamps are all in the south-east.
    unsure |= (label == bid('highland')) & ((ys > 800) | (xs > 900))   # ...and the grey around Ridgemere.
    label[unsure] = -1
    return fill_unknown(label, 80, bid('temperate'))


def grow(mask):
    grown = mask.copy()
    grown[1:] |= mask[:-1]
    grown[:-1] |= mask[1:]
    grown[:, 1:] |= mask[:, :-1]
    grown[:, :-1] |= mask[:, 1:]
    return grown


def fill_unknown(label, rounds, default):
    """Unknown (-1) tiles take the label of their nearest known neighbour."""
    for _ in range(rounds):
        if not (label < 0).any():
            break
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            shifted = shift(label, dy, dx)
            fill = (label < 0) & (shifted >= 0)
            label[fill] = shifted[fill]
    label[label < 0] = default
    return label


def shift(a, dy, dx):
    """The array moved by (dy, dx) without wrapping: uncovered edges read as unknown (-1)."""
    out = np.full_like(a, -1)
    h, w = a.shape
    out[max(0, dy):h + min(0, dy), max(0, dx):w + min(0, dx)] = a[max(0, -dy):h - max(0, dy), max(0, -dx):w - max(0, dx)]
    return out


def warp(label, amount, seed):
    """Organic borders: every tile takes the label from a little way off, pushed about by smooth noise."""
    ys, xs = np.mgrid[0:SIZE, 0:SIZE]
    wx = (field.fbm(SIZE, SIZE, 160, seed, 3) - .5) * 2 * amount
    wy = (field.fbm(SIZE, SIZE, 160, seed + 1, 3) - .5) * 2 * amount
    sx = np.clip((xs + wx).astype(int), 0, SIZE - 1)
    sy = np.clip((ys + wy).astype(int), 0, SIZE - 1)
    return label[sy, sx]


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


def ellipse_weight(centre, rx, ry, softness=.35):
    """1 well inside an ellipse on the plan, fading to 0 at its edge (shaped by noise)."""
    cx, cy = p(*centre)
    ys, xs = np.mgrid[0:SIZE, 0:SIZE].astype(np.float32)
    d = np.sqrt(((xs - cx) / rx) ** 2 + ((ys - cy) / ry) ** 2)
    return 1 - field.smoothstep(1 - softness, 1, d)


def blob(r, ry, rng):
    """An irregular round shape: the radius (along x; ry along y) at each angle, wobbled so it never reads as a circle."""
    a1, a2, a3 = (rng.uniform(0, 2 * math.pi) for _ in range(3))
    k = rng.uniform(.12, .22)

    def inside(dx, dy):
        angle = math.atan2(dy, dx)
        scale = 1 + k * math.sin(2 * angle + a1) + .7 * k * math.sin(3 * angle + a2) + .4 * k * math.sin(5 * angle + a3)
        return (dx / (r * scale)) ** 2 + (dy / (ry * scale)) ** 2
    return inside


# --- Building the world ------------------------------------------------------------------------------------------
class Western:
    def __init__(self, ua_edges):
        self.rng = random.Random(SEED)
        self.c = Canvas(SIZE, SIZE, ',')
        self.ua_edges = ua_edges           # Heights and codes of Upper Accord's region, by position in it.
        self.ua_mask = np.zeros((SIZE, SIZE), dtype=bool)
        x, y, w, h = UA
        self.ua_mask[y:y + h, x:x + w] = True
        self.features = []                  # (canvas x, y, kind)
        self.gates = {}                     # (settlement, side) -> the canvas point just outside that gate
        self.places = {}                    # settlement -> its ground (a mask), which roads route around
        self.ua_streams = []                # Upper Accord tiles (region-local x, y) the streams run through
        self.causeway = np.zeros((SIZE, SIZE), dtype=bool)
        self.road_lines = {}                # road name -> the canvas points it follows
        self.levels = {}                    # settlement -> the height of its ground
        self.gate_levels = {}               # (settlement, side) -> the height of that gate, where it differs
        self.reserved_ids = set()           # Room IDs the world already uses (the cities' interiors avoid them)
        self.city_site, self.cities, self.city_roads = None, [], []

    # The land ----------------------------------------------------------------------------------------------------
    def lay_biomes(self):
        drawn = read_drawing()
        self.water = warp(drawn == bid('water'), 6, SEED + 7)
        self.eastern_vale_river(drawn == bid('water'))
        land = drawn.copy()
        land[land == bid('water')] = -1
        land = fill_unknown(land, 60, bid('temperate'))    # Under water, the biome of the nearest shore.
        label = warp(land, 55, SEED + 3)
        ys, xs = np.mgrid[0:SIZE, 0:SIZE]
        # The far north is always cold.
        label[(ys < 420 + (field.fbm(SIZE, SIZE, 200, SEED + 9, 2) - .5) * 160)] = bid('north')
        # The rain coast: Ridgemere's grey highland and everything near it (fading, via the blend, into snow to the
        # east and temperate land to the south).
        wet = (ellipse_weight(*WET, softness=.4) * (.85 + .3 * field.fbm(SIZE, SIZE, 120, SEED + 60, 2))) > .5
        label[wet | (label == bid('highland'))] = bid('wet')
        # Ser Ferro's golden grain country: the plains (and the southern edge of the downs) around the city.
        golden = (ellipse_weight(*GOLDEN, softness=.4) * (.85 + .3 * field.fbm(SIZE, SIZE, 120, SEED + 61, 2))) > .5
        label[golden & np.isin(label, [bid('plains'), bid('temperate')]) & (ys > 1900)] = bid('golden')
        label[golden & (label == bid('plains'))] = bid('golden')
        water = self.water
        self.ocean = components(water, [(20, 20), (10, 700), (60, 300)]) >= 0
        self.lake = components(water, [p(1100, 1260), p(1000, 1150)]) >= 0
        # The lake's north-east lobe ran into Upper Accord's region, leaving no shore for the road the drawing
        # takes down the lake's east side: it keeps a 36-tile shore from the region instead.
        x0, y0, w, hgt = UA
        cx, cy = np.clip(xs, x0, x0 + w - 1), np.clip(ys, y0, y0 + hgt - 1)
        near_ua = np.maximum(np.abs(xs - cx), np.abs(ys - cy)) <= 36
        water[self.lake & near_ua] = False
        self.lake &= ~near_ua
        self.river = water & ~self.ocean & ~self.lake
        # The drawing paints both swamps one olive; the deep swamp is the one south of the great river.
        great = components(water, [p(1100, 1790)]) >= 0
        rows = np.where(great, np.arange(SIZE)[:, None], -1).max(axis=0)          # The river's south bank, by column.
        south = (ys > rows[None, :]) & (rows[None, :] >= 0)
        label[(label == bid('swamp')) & south] = bid('deep')
        self.label = label
        self.biome = self.blend(label)
        self.sub = self.patches()

    def blend(self, label):
        """Each tile picks among the biomes near it, weighted by how much of each is around: where two meet they
        interleave in clumps and specks over a band ~2 x BLEND wide, rather than meeting at a line."""
        present = [k for k in range(len(BIOME_IDS)) if k != bid('water') and (label == k).any()]
        clumps = field.fbm(SIZE, SIZE, 30, SEED + 70, 3)
        specks = np.random.default_rng(SEED + 71).random((SIZE, SIZE), dtype=np.float32)
        best = np.full((SIZE, SIZE), -9, dtype=np.float32)
        out = label.copy()
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

    def eastern_vale_river(self, drawn_water):
        """The river below Upper Accord: the drawing has it spring full-width from the region's edge. Instead a tarn
        below Southern Peak, fed by three streams from its springs, lets a narrow river out that widens downstream to
        the drawing's river."""
        water = self.water
        x_lo, x_hi = 2150, 2345
        centre = {}
        for y in range(UA[1] + UA[3], RIVER_TO + 40):
            xs = np.nonzero(drawn_water[y, x_lo:x_hi])[0]
            if len(xs):
                centre[y] = x_lo + float(xs.mean())
        ys = np.array(sorted(centre))
        cs = np.convolve(np.pad([centre[y] for y in ys], 30, mode='edge'), np.ones(61) / 61, mode='valid')
        water[UA[1] + UA[3]:RIVER_TO, x_lo:x_hi] = False
        tx, ty, rx, ry = TARN
        wobble = field.fbm(SIZE, SIZE, 9, SEED + 80, 2)
        for y, cx in zip(ys, cs):
            if y < ty or y >= RIVER_TO + 40:
                continue
            t = min(1, max(0, (y - ty) / (RIVER_TO - ty)))
            if y < RIVER_FROM:                                    # From the tarn's outlet, bend onto the course.
                cx = tx + (cx - tx) * (y - ty) / (RIVER_FROM - ty)
            width = 7 + 17 * t * t
            x0, x1 = int(cx - width / 2), int(cx + width / 2) + 1
            water[y, x0:x1] = True
        self.water_tarn = np.zeros((SIZE, SIZE), dtype=bool)
        shape = blob(rx, ry, random.Random(SEED + 81))
        for y in range(ty - ry - 8, ty + ry + 9):
            for x in range(tx - rx - 8, tx + rx + 9):
                if shape(x - tx, y - ty) <= 1 + (wobble[y, x] - .5) * .2:
                    self.water_tarn[y, x] = True
        water |= self.water_tarn
        # The streams in the vale: from where each leaves Upper Accord's region, winding down into the tarn.
        rng = random.Random(SEED + 82)
        self.streams = np.zeros((SIZE, SIZE), dtype=bool)
        for (mx, my) in STREAM_MOUTHS:
            end = (tx + (mx - STREAM_MOUTHS[1][0]) * .6, ty - ry * .6)
            length = int(math.dist((mx, my), end))
            phase, amp = rng.uniform(0, 6), rng.uniform(2, 4)
            for i in range(length + 1):
                t = i / max(1, length)
                x = mx + (end[0] - mx) * t + amp * math.sin(t * 9 + phase) * (1 - t)
                y = my + (end[1] - my) * t
                self.streams[int(y), int(x):int(x) + 2] = True
        water |= self.streams
        self.ua_streams = self.trace_ua_streams()

    def trace_ua_streams(self):
        """The streams on Southern Peak: from each spring downhill (drawn toward its mouth at the region's edge),
        one tile wide. Returned as Upper Accord region-local tiles; the peak's cell is patched on import."""
        h = self.ua_edges['heights']
        x0, y0 = UA[0], UA[1]
        wobble = np.random.default_rng(SEED + 83)
        tiles = []
        for (sx, sy), (mx, my) in zip(SPRINGS, STREAM_MOUTHS):
            x, y = sx - x0, sy - y0
            goal = (mx - x0, my - y0)
            seen = {(x, y)}
            path = [(x, y)]
            for _ in range(800):
                if y >= goal[1]:
                    break
                options = []
                for dx, dy in ((-1, 1), (0, 1), (1, 1), (-1, 0), (1, 0)):
                    nx, ny = x + dx, y + dy
                    if (nx, ny) in seen or not (0 <= nx < UA[2] and 0 <= ny < UA[3]):
                        continue
                    score = h[ny, nx] + .09 * math.dist((nx, ny), goal) + wobble.random() * .4
                    options.append((score, nx, ny))
                if not options:
                    break
                _, nx, ny = min(options)
                if nx != x and ny != y:                   # A diagonal step: fill the corner so the stream is joined.
                    path.append((nx, y))
                x, y = nx, ny
                seen.add((x, y))
                path.append((x, y))
            tiles += path
        return sorted(set(tiles))

    def lay_heights(self):
        xs, ys = field.grid(SIZE, SIZE)
        h = np.zeros((SIZE, SIZE), dtype=np.float32)
        b = self.biome
        rolling = field.fbm(SIZE, SIZE, 70, SEED + 20, 4)
        knolls = np.maximum(0, rolling - .56) * 9               # Low hills in patches; most lowland stays level.
        for name, scale in (('north', 1.3), ('wet', 1.2), ('temperate', 1.0), ('plains', .55), ('swamp', .25),
                            ('deep', .15)):
            mask = b == bid(name)
            h[mask] += knolls[mask] * scale
        # Ser Ferro's country rolls everywhere, gently: long sunlit swells rather than scattered knolls.
        golden = box_blur(box_blur((b == bid('golden')).astype(np.float32), 40), 40)
        swell = np.maximum(0, field.fbm(SIZE, SIZE, 120, SEED + 24, 3) - .3) * 7
        h += golden * swell
        # Domain warp: ranges and hills are laid on bent coordinates, so nothing comes out round.
        wx = (field.fbm(SIZE, SIZE, 150, SEED + 25, 3) - .5) * 110 + (field.fbm(SIZE, SIZE, 40, SEED + 26, 2) - .5) * 26
        wy = (field.fbm(SIZE, SIZE, 150, SEED + 27, 3) - .5) * 110 + (field.fbm(SIZE, SIZE, 40, SEED + 28, 2) - .5) * 26
        for peaks in RANGES.values():
            points = [(*p(x, y), height, spread) for x, y, height, spread in peaks]
            for i, j in tree_edges(points):
                self.ridge(h, points[i], points[j], wx, wy, 1.0)
            for x, y, height, spread in points:
                self.ridge(h, (x, y, height, spread), (x + 1, y, height, spread), wx, wy, 1.0)
        hills = [(*p(x, y), height, spread) for x, y, height, spread in HILLS]
        for i, j in tree_edges(hills, longest=130):
            self.ridge(h, hills[i], hills[j], wx, wy, .9)
        for hill in hills:
            self.ridge(h, hill, (hill[0] + 1, hill[1], hill[2], hill[3]), wx, wy, .9)
        # Spurs and gullies on the ranges (ridged noise), and fine roughness on the heights.
        mountain = field.smoothstep(3, 9, h)
        ridged = 1 - np.abs(2 * field.fbm(SIZE, SIZE, 55, SEED + 29, 4) - 1)
        h += (ridged - .55) * 5 * mountain
        h += (field.fbm(SIZE, SIZE, 14, SEED + 22, 2) - .5) * 1.6 * mountain
        # The shore: land slopes to the water's edge; water itself is level.
        near_water = field.distance_to_mask(self.water & ~self.streams, 12)
        h = np.where(self.water & ~self.streams, 0, h * field.smoothstep(0, 12, near_water))
        # Upper Accord: the ground within 48 tiles of its region rises or falls to meet its edge exactly.
        self.blend_to_ua(h)
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

    def blend_to_ua(self, h):
        x0, y0, w, hgt = UA
        edge = self.ua_edges['heights']
        ys, xs = np.mgrid[0:SIZE, 0:SIZE]
        cx, cy = np.clip(xs, x0, x0 + w - 1), np.clip(ys, y0, y0 + hgt - 1)
        d = np.maximum(np.abs(xs - cx), np.abs(ys - cy))
        near = (d > 0) & (d <= 48)
        target = edge[cy[near] - y0, cx[near] - x0]
        weight = 1 - field.smoothstep(0, 48, d[near].astype(np.float32))
        h[near] = h[near] * (1 - weight) + target * weight
        # The edge's own profile, carried straight out, leaves stripes; smooth them away from the edge itself.
        smooth = box_blur(box_blur(h, 5), 5)
        band = (d > 0) & (d <= 60)
        s = field.smoothstep(2, 16, d[band].astype(np.float32))
        h[band] = h[band] * (1 - s) + smooth[band] * s

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
                    self.golden_fields(codes, mask)
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
        codes[slope_wood & (self.biome == bid('temperate'))] = ord('P')
        codes[slope_wood & (self.biome == bid('north'))] = ord('1')
        codes[slope_wood & (self.biome == bid('wet'))] = np.where(pick[slope_wood & (self.biome == bid('wet'))] < .6,
                                                                  ord('$'), ord('P'))
        # Bare rock, scree and snow on the heights (lower in the cold north).
        cold = self.biome == bid('north')
        codes[(h >= 6) & (detail > .5)] = ord('r')
        codes[(h >= 8) & (detail > .35)] = ord('r')
        codes[(h >= 9) & (detail > .62)] = ord('s')
        codes[((h >= 11) | (cold & (h >= 6))) & (detail > .4)] = ord('*')
        codes[(h >= 13)] = ord('*')
        wet = self.biome == bid('wet')
        codes[wet & (h >= 6) & (h < 11) & (detail < .45)] = ord('!')    # Moss on the rain coast's rocks.
        # Water: deep in the middle, wadeable at the edges; streams are shallow all through.
        shore = field.distance_to_mask(~self.water, 8)
        codes[self.water] = ord('W')
        codes[self.water & (shore <= 3)] = ord('~')
        swampy = (self.biome == bid('swamp')) | (self.biome == bid('deep'))
        codes[self.water & (shore <= 5) & swampy] = ord('w')
        # Snow thins toward the edge of the cold north: tundra, then grass, show through as it gives out.
        thin = (self.biome == bid('north')) & (codes == ord('*')) & (h < 11)
        cover = np.clip(self.north_weight * 1.4 - .15, 0, 1)
        codes[thin & (grain > cover)] = np.where(pick[thin & (grain > cover)] < cover[thin & (grain > cover)] + .3,
                                                 ord('-'), ord(','))
        frozen = self.water & cold & ~self.ocean
        codes[frozen & (shore <= 6)] = ord('J')
        codes[self.streams] = ord('~')
        # Beaches and banks: sand, the rain coast's shingle and swamp reeds.
        land_edge = field.distance_to_mask(self.water & ~self.streams, 6)
        bank = ~self.water & (land_edge <= 3) & (detail > .35)
        codes[bank & ~cold & ~swampy & ~wet] = ord('0')
        codes[bank & wet] = np.where(pick[bank & wet] < .3, ord('o'), ord('r'))
        codes[~self.water & (land_edge <= 2) & swampy] = ord('E')
        free = ~c.locked
        c.codes[free] = codes[free]

    def golden_fields(self, codes, mask):
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
        keep = (self.ua_mask | c.locked)[y0:y1, x0:x1]
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
            soft = box_blur(region.astype(np.float32), 5) > .5
            region = soft & dry
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
            # The way out: straight out of the side it faces.
            vx, vy = {'N': (0, -1), 'S': (0, 1), 'W': (-1, 0), 'E': (1, 0)}[side]
            # The opening: every wall tile across the gate's width, through the wall's whole depth.
            reach = gw + thick + 2
            yy, xx = np.mgrid[max(0, gy - reach):gy + reach + 1, max(0, gx - reach):gx + reach + 1]
            depth = (gx - xx) * vx + (gy - yy) * vy            # How far in from the gate's outer face.
            lateral = np.abs((xx - gx) * vy - (yy - gy) * vx)
            near = ring[yy, xx] & (lateral <= gw // 2) & (depth >= -1) & (depth <= thick + 1)
            c.paint(self._mask(yy[near], xx[near]), 'G', level)
            ox, oy = gx, gy
            for _ in range(40):
                ox, oy = ox + vx, oy + vy
                if not region[int(oy), int(ox)]:
                    break
            self.gates[(sid, side)] = (float(ox + vx * 4), float(oy + vy * 4))
        if kind == 'ghost':
            rng = random.Random(sid)
            for _ in range(60):                   # Broken walls, rubble and the dead trees of an abandoned place.
                bx, by = rng.randrange(x, x + w), rng.randrange(y, y + h)
                c.stamp(bx, by, ['X' if ring[by, bx] else rng.choice('X2.,')], level)
        self.features.append((x + w // 2, y + h // 2, kind))

    @staticmethod
    def _mask(ys, xs):
        m = np.zeros((SIZE, SIZE), dtype=bool)
        m[ys, xs] = True
        return m

    # Roads -------------------------------------------------------------------------------------------------------
    STEP = 4                                # Roads are routed on a grid of 4-tile squares.

    def route_costs(self):
        """What it costs a road to cross each 4-tile square: water dearly (so lakes and the sea are gone round and
        rivers bridged only where needed), the lake shore a little (so roads keep a few tiles back from it), steep
        and high ground by its slope, and settlements and Upper Accord not at all."""
        s, n = self.STEP, SIZE // self.STEP
        coarse = lambda a: a.reshape(n, s, n, s).mean(axis=(1, 3))
        wet = coarse((self.water & (self.lake | self.ocean | self.water_tarn)).astype(np.float32))
        river = coarse((self.water & ~(self.lake | self.ocean | self.water_tarn) & ~self.streams).astype(np.float32))
        h = coarse(self.c.heights)
        gy, gx = np.gradient(h)
        slope = np.hypot(gx, gy) / s
        shore = grow(grow(wet > 0)) & ~(wet > 0)
        cost = 1 + 90 * wet + 20 * river + 3 * shore + 60 * slope ** 2 + .25 * np.maximum(0, h - 5)
        blocked = coarse(self.ua_mask.astype(np.float32)) > 0
        for region in self.places.values():
            blocked |= coarse(region.astype(np.float32)) > 0
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
        taken = self.water | self.ua_mask
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
                points = [points[0]] + [self.dry_point(q) for q in points[1:-1]] + [points[-1]]
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
        free = ~c.locked[near] & ~self.ua_mask[near]
        old = c.heights[near]
        c.heights[near] = np.where(free, half(old * (1 - blend) + hts[near] * blend), old)
        paint = mask & ~self.ua_mask & ~c.locked
        bridge = paint & self.water
        c.codes[paint & ~self.water] = ord('d')
        c.codes[bridge] = ord('8')
        c.locked |= paint
        if causeway:
            self.causeway |= bridge

    # Points of interest -----------------------------------------------------------------------------------------
    def scatter_features(self):
        rng = self.rng
        spacing = 78
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
        return (not self.ua_mask[y, x] and not self.water[y, x] and not self.c.locked[y, x]
                and not self.ua_mask[min(SIZE - 1, y + 20), min(SIZE - 1, x + 20)]
                and not self.ua_mask[max(0, y - 20), max(0, x - 20)])

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
        if 0 <= x < SIZE and 0 <= y < SIZE and not c.locked[y, x] and not self.ua_mask[y, x] and not self.water[y, x]:
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
        tree = {'north': '1', 'swamp': 'Y', 'deep': '2', 'plains': 'Y', 'wet': '$', 'golden': 'Y'}.get(
            biome, self.rng.choice('YP'))
        r = self.rng.randrange(7, 13)
        inside = blob(r, r * self.rng.uniform(.6, 1), self.rng)
        for dy in range(-r - 4, r + 5):
            for dx in range(-r - 4, r + 5):
                if inside(dx, dy) <= 1 and self.rng.random() < .55:
                    self.put(x + dx, y + dy, tree)
        self.disc(x, y, 2, '3' if biome in ('temperate', 'plains', 'golden') else ',')

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

    def f_mesa(self, x, y, biome):
        r = self.rng.randrange(14, 24)
        top = self.c.heights[y, x] + self.rng.choice([3, 3.5, 4])
        inside = blob(r, r / 1.2, self.rng)
        for dy in range(-r - 6, r + 7):
            for dx in range(-r - 7, r + 8):
                d = inside(dx, dy)
                if d <= 1:
                    self.put(x + dx, y + dy, '%' if d > .82 else (';' if self.rng.random() < .8 else 'X'), top)
        edge = next((i for i in range(r * 2, 0, -1) if inside(-i, 0) <= 1), r)
        for i in range(4):
            self.put(x - edge + i, y, ':', top - 1.5 + i * .5)

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

    def f_dry_creek(self, x, y, biome):
        a = self.rng.uniform(0, math.pi)
        for t in range(-45, 46):
            cx = x + t * math.cos(a) + 4 * math.sin(t / 7)
            cy = y + t * math.sin(a) + 4 * math.cos(t / 9)
            for s in (-1, 0, 1):
                self.put(int(cx - s * math.sin(a)), int(cy + s * math.cos(a)), 'X' if s == 0 else '.')

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
                if 0 <= px < SIZE and 0 <= py < SIZE and not self.c.locked[py, px] and not self.ua_mask[py, px]:
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
    def build(self):
        self.lay_biomes()
        self.lay_heights()
        from . import cities
        for s in SETTLEMENTS:
            if s[0] not in cities.DESIGNED:
                self.settlement(*s)
        self.city_site, self.cities, self.city_roads = cities.build(self)
        self.lay_roads()
        self.lay_ground()
        placed = self.scatter_features()
        self.mark_cliffs()
        # Nothing of the new world may touch Upper Accord's region.
        x, y, w, h = UA
        self.c.codes[y:y + h, x:x + w] = ord(',')
        self.c.heights[y:y + h, x:x + w] = 0
        return placed

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
        c.codes[higher & ~c.locked & ~self.water & ~ramp & ~self.ua_mask] = ord('%')

    def cells(self):
        """Atlas cells for every 256-tile square outside Upper Accord that holds land."""
        c = self.c
        centres = [(name, *p(x, y), blurb) for name, x, y, blurb in REGIONS]
        out = []
        for cy in range(SIZE // CELL):
            for cx in range(SIZE // CELL):
                x0, y0 = cx * CELL, cy * CELL
                if self.ua_mask[y0 + 128, x0 + 128]:
                    continue
                codes = c.codes[y0:y0 + CELL, x0:x0 + CELL]
                if (codes == ord('W')).mean() > .97:
                    continue                      # Open sea: the world simply ends.
                region = min(centres, key=lambda r: (r[1] - x0 - 128) ** 2 + (r[2] - y0 - 128) ** 2)
                rows, heights = c.cell(x0, y0, CELL, CELL)
                biome_here = np.bincount(self.biome[y0:y0 + CELL, x0:x0 + CELL].ravel(), minlength=len(BIOME_IDS))
                main = BIOME_IDS[int(biome_here.argmax())]
                weather = {'north': 'snow', 'wet': 'rain', 'deep': 'fog', 'swamp': 'overcast'}.get(main, 'clear')
                out.append({'region': region[0], 'blurb': region[3], 'cx': cx, 'cy': cy, 'biome': main,
                            'x': x0 + OX, 'y': y0 + OY, 'terrain': rows, 'heights': heights, 'weather': weather})
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
                cells.append({'id': f'w_{slug}_{g["cx"]}_{g["cy"]}', 'name': name, 'description': g['blurb'],
                              'x': g['x'], 'y': g['y'], 'width': CELL, 'height': CELL, 'z': 0, 'outdoors': True,
                              'weather': g['weather'], 'territory': {'region': slug, 'claims': [], 'chapter': ''},
                              'terrain': g['terrain'], 'heights': g['heights']})
        return cells


def ua_edges_from(project):
    """Upper Accord's region as a height and code grid (from DEV, so its own edges are matched exactly)."""
    heights = np.zeros((UA[3], UA[2]), dtype=np.float32)
    codes = np.full((UA[3], UA[2]), ord(','), dtype=np.uint8)
    from .canvas import DEFAULT_HEIGHT
    for c in project['cells']:
        x0, y0 = c['x'], c['y']
        if not (0 <= x0 < UA[2] and 0 <= y0 < UA[3]):
            continue
        for y, row in enumerate(c['terrain']):
            codes[y0 + y, x0:x0 + len(row)] = np.frombuffer(row.encode('ascii'), dtype=np.uint8)
            heights[y0 + y, x0:x0 + len(row)] = DEFAULT_HEIGHT[codes[y0 + y, x0:x0 + len(row)]]
        for key, value in c['heights'].items():
            x, y = map(int, key.split(','))
            heights[y0 + y, x0 + x] = value
    return {'heights': heights, 'codes': codes}


# Upper Accord tiles a stream may run over: open ground and plants (not cliffs, walls, buildings or ramps).
STREAM_OVER = set(',."\'rs*-!&$PY1B3o5;D')


def with_streams(project, tiles):
    """The project with the streams painted into the Upper Accord cells they cross (its heights kept as they were).
    Returns the new project and the ids of the cells changed."""
    import terrain_catalog
    changed, cells = set(), []
    stream_default = terrain_catalog.DEFAULT_HEIGHTS.get('~', 0)
    by_cell = {}
    for x, y in tiles:
        for c in project['cells']:
            if c['x'] <= x < c['x'] + c['width'] and c['y'] <= y < c['y'] + c['height']:
                by_cell.setdefault(c['id'], []).append((x - c['x'], y - c['y']))
                break
    for c in project['cells']:
        if c['id'] not in by_cell:
            cells.append(c)
            continue
        rows = [list(r) for r in c['terrain']]
        heights = dict(c['heights'])
        for x, y in by_cell[c['id']]:
            old = rows[y][x]
            if old not in STREAM_OVER:
                continue
            level = heights.get(f'{x},{y}', terrain_catalog.DEFAULT_HEIGHTS.get(old, 0))
            rows[y][x] = '~'
            if level == stream_default:
                heights.pop(f'{x},{y}', None)
            else:
                heights[f'{x},{y}'] = level
        cells.append({**c, 'terrain': [''.join(r) for r in rows], 'heights': heights})
        changed.add(c['id'])
    return {**project, 'cells': cells}, changed


# Interiors, doors, people and routes of the two designed cities, and (doc 30) of the towns built into the w_ cells
# and the cities' newcomers: replaced together, the towns rebuilt by worldgen.towns at the end of assemble().
GENERATED = ('rm_', 'sf_', 'tn_', 'rmx_', 'sfx_')


def claims_for(record):
    """Who holds a building of the two cities (faction IDs)."""
    name, district, city = record['name'], record['district'], record.get('city')
    if city == 'ridgemere':
        if district.startswith('estate_'):
            return ['house_' + district.split('_')[1]]
        for key, house in (('Brinewater', 'brinewater'), ('Vesk', 'vesk'), ('Fish Hall', 'vesk'),
                           ('Grayrock', 'grayrock'), ('Fell', 'fell'), ('Ashcombe', 'ashcombe')):
            if key in name:
                return ['house_' + house]
        if name in ('The Watch House', 'The Gaol'):
            return ['council_of_houses', 'ridgemere_watch']
        return ['council_of_houses']
    if name in ('The Cathedral of the Iron Saint', 'The Chapter House', 'The Clergy House', 'The House of Saint Chiara'):
        return ['church_iron_saint']
    if name == "The Merchants' Guildhall" or district == 'heights' and record['kind'] == 'shop':
        return ['merchants_guild']
    if name == 'The City Guard Barracks':
        return ['ser_ferro_guard']
    return ['crown_of_ser_ferro']


def assemble(project, world, cells):
    """The DEV project with the generated world: every w_ cell replaced, the streams painted into Upper Accord, the
    two cities' interiors, doors, people, routes and factions replaced, and everything else kept as it is."""
    from . import citizens
    streamed, _ = with_streams(project, world.ua_streams)
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
    ours = lambda i: i.startswith(GENERATED)
    merged = dict(streamed)
    merged['cells'] = [c for c in streamed['cells'] if not c['id'].startswith('w_')] + cells
    merged['rooms'] = [r for r in streamed['rooms'] if not ours(r['id'])] + rooms
    merged['links'] = [link for link in streamed['links']
                       if not (ours(link['id']) or ours(link['a']['cell']) or ours(link['b']['cell']))] + links
    merged['people'] = [p for p in streamed.get('people', []) if not ours(p['id'])]
    merged['routes'] = [r for r in streamed.get('routes', []) if not ours(r['id'])]
    people, routes = citizens.populate(merged, site.manifest, world)
    merged['people'] = merged['people'] + people
    merged['routes'] = merged['routes'] + routes
    known = {f['id'] for f in merged.get('factions', [])}
    merged['factions'] = merged.get('factions', []) + [f for f in citizens.FACTIONS if f['id'] not in known]
    # Claims: each city's buildings by who holds them; its cells by the city and the Houses whose estates lie there.
    room_claims = {}
    for record in site.manifest:
        for room in record['rooms']:
            room_claims[room['id']] = claims_for(record)
    for r in merged['rooms']:
        if r['id'] in room_claims:
            r['territory'] = {**r['territory'], 'claims': room_claims[r['id']]}
    cell_claims = {}
    for sid, region in world.places.items():
        if sid in ('ridgemere', 'ser_ferro') or sid.startswith('estate_'):
            ys, xs = np.nonzero(region)
            key = f'@{int(xs.mean()) // CELL}_{int(ys.mean()) // CELL}'
            add = {'ridgemere': ['council_of_houses', 'ridgemere_watch'],
                   'ser_ferro': ['crown_of_ser_ferro', 'ser_ferro_guard']}.get(sid, ['house_' + sid[7:]])
            cell_claims.setdefault(by_grid[key], set()).update(add)
    for c in merged['cells']:
        if c['id'] in cell_claims:
            c['territory'] = {**c['territory'], 'claims': sorted(cell_claims[c['id']])}
    # The towns, village and fortresses built out again on the new ground, and the cities' newcomers moved back in.
    from . import towns
    merged, _ = towns.build_all(merged, report=lambda line: print(f'  towns: {line}'))
    return merged


def dev_project():
    import world_db
    import world_store
    with world_db.connect('dev', 'editor') as conn:
        wid = world_store.list_worlds(conn)[0]['id']
        return world_store.load_world(conn, wid)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--preview', type=Path)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--import-dev', action='store_true')
    args = parser.parse_args(argv)
    project, revision = dev_project()
    world = Western(ua_edges_from(project))
    world.reserved_ids = {a['id'] for a in project['cells'] + project['rooms'] if not a['id'].startswith(GENERATED)}
    placed = world.build()
    cells = world.cells()
    streamed, touched = with_streams(project, world.ua_streams)
    merged = assemble(project, world, cells)
    roles = {}
    for person in merged['people']:
        if person['id'].startswith(GENERATED):
            roles[person['id'][:3] + person['role']] = roles.get(person['id'][:3] + person['role'], 0) + 1
    print(f'{len(cells)} cells, {placed} scattered points of interest, {len(SETTLEMENTS)} settlements; '
          f'streams painted into {sorted(touched)}')
    print(f'cities: {len(world.city_site.manifest)} buildings, {len(world.city_site.rooms)} interiors; '
          f'residents {sorted(roles.items())}; world total {len(merged["people"])} residents, '
          f'{len(merged["routes"])} routes, {len(merged["links"])} links')
    print(f'height overrides: {sum(len(c["heights"]) for c in cells)}')
    import map_editor
    try:
        map_editor.check_project(merged, for_game=False)
        print('Atlas validation: OK')
    except map_editor.ValidationError as error:
        print('Atlas validation failed:', *error.errors[:30], sep='\n  ')
        return 1
    if args.preview:
        args.preview.mkdir(parents=True, exist_ok=True)
        c = world.c
        codes, heights = c.codes.copy(), c.heights.copy()
        x, y, w, h = UA
        ua = ua_edges_from(streamed)
        codes[y:y + h, x:x + w] = ua['codes']
        heights[y:y + h, x:x + w] = ua['heights']
        np.savez_compressed(args.preview / 'canvas.npz', codes=codes, heights=heights, biome=world.biome)
        overview = [{'x': cell['x'] - OX, 'y': cell['y'] - OY, 'width': CELL, 'height': CELL, 'name': cell['name'][:28]}
                    for cell in cells]
        preview.overview(codes, heights, overview, args.preview / 'world_full.png', scale=1, relief=2.2)
        image = Image.open(args.preview / 'world_full.png')
        image.resize((1280, 1280), Image.LANCZOS).save(args.preview / 'world.png')
    if args.out:
        args.out.mkdir(parents=True, exist_ok=True)
        (args.out / 'western_cells.json').write_text(json.dumps(cells), encoding='utf-8')
        (args.out / 'ua_streams.json').write_text(json.dumps(world.ua_streams), encoding='utf-8')
        (args.out / 'roads.json').write_text(json.dumps(world.road_lines), encoding='utf-8')
        (args.out / 'buildings.json').write_text(json.dumps(world.city_site.manifest, indent=1), encoding='utf-8')
        (args.out / 'project.json').write_text(json.dumps(merged), encoding='utf-8')
    if args.import_dev:
        import world_db
        import world_store
        # Every generated cell, interior, door, resident and route is replaced; anything else authored is kept.
        with world_db.connect('dev', 'editor') as conn:
            new = world_store.save_world(conn, merged, revision)
        print(f'Saved to DEV, revision {new}.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
