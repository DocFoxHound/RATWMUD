"""Ridgemere 2.0: the realm (Docs/Design/70-the-realm.md).

Grows DEV's map east and south from Ridgemere's 3 x 3 cells to 16 x 16 cells of 256 tiles (4,096 tiles square), Ridgemere
staying in the north-west corner with the sea and its bay on the west. The city's cells keep their coordinates, and
everything in them that is built (the walls and all inside them, the bay, the wharf and piers, the Houses' grounds, the
roads from the gates, every door, post and NPC spot) keeps its tiles and heights exactly; only the wild ground around
them is made again, as the rain coast's woods, so that it blends into the realm.

The land is laid out from fields, not drawn: an elevation (the Whitefangs along the north, the Greyteeth in the
north-east, the Ochre Hills running south from them, the Rainward Hills and the coast range by the city, the rolling
middle country, the low Sallow Fens in the south-west, the Red Waste's plateau in the south-east), the rain a westerly
wind drops as it crosses it (so the far side of the Ochre Hills is dry), the warmth by latitude, height and the sea,
and the water that runs off: priority-flood drainage, so every river runs downhill to the sea, a lake or the Saltpan,
and the lakes are where the land holds water. Biomes come from warmth and wetness on bent coordinates, so their
borders interleave rather than run along lines.

  cd tools && python3 -m worldgen.realm2 preview [--seed N] [--out DIR]   the realm, previews in artifacts/realm2/
  cd tools && python3 -m worldgen.realm2 write [--world dev]     back up, then add the realm to DEV (add-only: the same map)

The seed is fixed, so every run makes the same realm; worldgen.ridgemere2's write lays it round the city too.
"""
from __future__ import annotations

import argparse
import gzip
import heapq
import json
import math
import random
import re
import sys
import time
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    __package__ = 'worldgen'
    import worldgen  # noqa: F401

from . import field  # noqa: E402
from .canvas import DEFAULT_HEIGHT, RAMP, SOLID, half  # noqa: E402

import terrain_catalog  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / 'artifacts' / 'realm2'
SIZE = 4096
CELL = 256
N = SIZE // CELL
G = 8                                   # The coarse grid: one node every 8 tiles.
NG = SIZE // G
SEED = 20261101
CITY = 768                              # Ridgemere's cells: x and y 0-768 (3 x 3 cells), kept where they are.
MAP_NAME = 'Ridgemere 2.0: the city (doc 67)'

# The wild ground in the city's cells that may be made again (the moor round the walls); anything else stays.
WILD = set(',5"!YB&oP%3')


def cell_id(cx, cy):
    return f'w_ridgemere_{cx}_{cy}'


# ---------------------------------------------------------------------------------------------------------------
# Small field helpers
# ---------------------------------------------------------------------------------------------------------------
def blur(a, r):
    """A box blur of radius r, twice (so it is smooth), on any shape."""
    a = a.astype(np.float32)
    if r <= 0:
        return a
    for _ in range(2):
        for axis in (0, 1):
            pad = [(0, 0), (0, 0)]
            pad[axis] = (r + 1, r)
            s = np.cumsum(np.pad(a, pad, mode='edge'), axis=axis, dtype=np.float64)
            hi = np.take(s, range(2 * r + 1, s.shape[axis]), axis=axis)
            lo = np.take(s, range(0, s.shape[axis] - 2 * r - 1), axis=axis)
            a = ((hi - lo) / (2 * r + 1)).astype(np.float32)
    return a


def up(a, f=G):
    """A coarse field (one value every f tiles, at the middle of its square) to tiles, bilinear."""
    n0, n1 = a.shape
    def axis(n):
        t = np.clip((np.arange(n * f) + .5) / f - .5, 0, n - 1)
        i0 = np.floor(t).astype(int)
        i1 = np.minimum(i0 + 1, n - 1)
        return i0, i1, (t - i0).astype(np.float32)
    y0, y1, wy = axis(n0)
    x0, x1, wx = axis(n1)
    rows = a[y0] * (1 - wy)[:, None] + a[y1] * wy[:, None]
    return (rows[:, x0] * (1 - wx)[None, :] + rows[:, x1] * wx[None, :]).astype(np.float32)


def down(a, f=G, how='mean'):
    n0, n1 = a.shape[0] // f, a.shape[1] // f
    b = a.reshape(n0, f, n1, f)
    return b.mean(axis=(1, 3)) if how == 'mean' else b.max(axis=(1, 3)) if how == 'max' else b.min(axis=(1, 3))


def smooth(a, b, v):
    return field.smoothstep(a, b, v)


def dist_to(mask, limit):
    """Tiles (4-way steps) from each tile to the nearest True one, capped at `limit`."""
    return field.distance_to_mask(mask, limit)


def ridged(w, h, cell, seed, octaves=4):
    return 1 - np.abs(2 * field.fbm(w, h, cell, seed, octaves) - 1)


def noise_at(xs, ys, cell, seed):
    """Value noise (as field.value_noise) at any points, so the coordinates may be bent (domain warping)."""
    rng = np.random.default_rng(seed)
    u, v = np.asarray(xs, dtype=np.float32) / cell, np.asarray(ys, dtype=np.float32) / cell
    lo_u, lo_v = int(math.floor(float(u.min()))), int(math.floor(float(v.min())))
    gw, gh = int(math.floor(float(u.max()))) - lo_u + 3, int(math.floor(float(v.max()))) - lo_v + 3
    lattice = rng.random((gh, gw), dtype=np.float32)
    x0, y0 = np.floor(u).astype(np.int32), np.floor(v).astype(np.int32)
    fx, fy = u - x0, v - y0
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    x0 -= lo_u
    y0 -= lo_v
    a, b = lattice[y0, x0], lattice[y0, x0 + 1]
    c, d = lattice[y0 + 1, x0], lattice[y0 + 1, x0 + 1]
    return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy


def ridges_at(xs, ys, cell, seed, octaves=4, gain=1.8):
    """Ridged multifractal noise (0-1) at any points: sharp crests that branch into spurs, each octave's detail
    strongest on the crests of the one before (so the valleys between stay smooth)."""
    total = np.zeros(np.shape(xs), dtype=np.float32)
    weight = np.ones(np.shape(xs), dtype=np.float32)
    amp, norm = 1.0, 0.0
    for o in range(octaves):
        n = 1 - np.abs(2 * noise_at(xs, ys, cell / 2 ** o, seed + 131 * o) - 1)
        n = n * n * weight
        weight = np.clip(n * gain, 0, 1)
        total += n * amp
        norm += amp
        amp *= .5
    return total / norm


def grid_xy(n, f):
    ys, xs = np.mgrid[0:n, 0:n].astype(np.float32)
    return xs * f + f / 2, ys * f + f / 2


def ellipse(xs, ys, cx, cy, rx, ry, angle=0.0, power=2.0):
    c, s = math.cos(angle), math.sin(angle)
    dx, dy = xs - cx, ys - cy
    u, v = (dx * c + dy * s) / rx, (-dx * s + dy * c) / ry
    return np.exp(-(u * u + v * v) ** (power / 2))


def seg_dist(xs, ys, a, b):
    (ax, ay), (bx, by) = a, b
    dx, dy = bx - ax, by - ay
    t = np.clip(((xs - ax) * dx + (ys - ay) * dy) / (dx * dx + dy * dy or 1e-9), 0, 1)
    return np.hypot(xs - (ax + t * dx), ys - (ay + t * dy)), t


def chaikin(pts, n=3):
    for _ in range(n):
        out = [pts[0]]
        for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
            out += [(.75 * x1 + .25 * x2, .75 * y1 + .25 * y2), (.25 * x1 + .75 * x2, .25 * y1 + .75 * y2)]
        pts = out + [pts[-1]]
    return pts


def densify(pts, step=1.0):
    out = []
    for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
        n = max(1, int(math.hypot(x2 - x1, y2 - y1) / step))
        out += [(x1 + (x2 - x1) * k / n, y1 + (y2 - y1) * k / n) for k in range(n)]
    out.append(pts[-1])
    return out


# ---------------------------------------------------------------------------------------------------------------
# The plan: where things are (tiles; north up, y grows southward)
# ---------------------------------------------------------------------------------------------------------------
# The coast south of the city (y, x of the shore): the rain coast's coves and points, then the fens' wide bay.
COAST = [(768, 96), (840, 84), (960, 132), (1080, 104), (1180, 70), (1300, 92), (1420, 150), (1560, 128), (1700, 176),
         (1860, 150), (2000, 210), (2140, 250), (2280, 330), (2420, 430), (2560, 560), (2700, 690), (2840, 800),
         (2980, 870), (3120, 900), (3260, 880), (3400, 820), (3540, 740), (3680, 660), (3820, 600), (3960, 560),
         (4096, 540)]
# Cedar Sound: an arm of the sea reaching east into the rain coast (x, y, half-width).
SOUND = [(70, 1300, 46), (220, 1330, 40), (380, 1300, 34), (520, 1350, 30), (640, 1420, 26), (760, 1400, 20),
         (860, 1330, 14), (920, 1290, 8)]
# Mountains: peak chains (x, y, height, spread). The Whitefangs along the north, the Greyteeth in the north-east, the
# Ochre Hills south from them (their lee is the Red Waste), the Rainward Hills behind the city, the coast range.
RANGES = {
    'whitefangs': [(1000, 330, 7.5, 110), (1280, 260, 9.5, 150), (1560, 360, 11, 170), (1850, 260, 12, 190),
                   (2150, 400, 10.5, 170), (2460, 300, 12.5, 200), (2780, 440, 12, 190), (3080, 300, 12.5, 200),
                   (3380, 500, 12, 200), (3700, 360, 12, 200), (3980, 560, 11, 180)],
    'front': [(1380, 640, 8, 120), (1620, 760, 9, 130), (1900, 700, 10, 140), (2650, 840, 10, 140),
              (2950, 760, 11, 140), (3150, 640, 12, 140)],
    'greyteeth': [(3380, 500, 12, 200), (3560, 860, 12, 150), (3860, 980, 11.5, 150), (3480, 1180, 10.5, 130),
                  (3880, 1380, 10, 130), (3600, 1560, 8.5, 110), (3240, 1000, 9.5, 110)],
    'ochre': [(3600, 1560, 9, 110), (3260, 1780, 8, 100), (3060, 2060, 7.5, 95), (2930, 2380, 7, 90),
              (2830, 2700, 6.5, 85), (2760, 3020, 6.5, 85), (2680, 3330, 6, 80), (2620, 3650, 5.5, 75),
              (2580, 3980, 5, 70)],
    'rainward': [(860, 260, 4.5, 90), (980, 560, 5.5, 100), (900, 860, 4.5, 90), (1160, 980, 5.5, 100),
                 (1000, 330, 7.5, 110)],
    'coast': [(520, 1060, 4.5, 80), (460, 1560, 5.5, 90), (620, 1840, 5, 90), (560, 2160, 4, 80)],
}
# The ranges low enough to be hills, rounded rather than worn into crests (lay_elevation).
LOW_RANGES = {'rainward', 'coast'}
# The pass: a high valley in the Whitefangs (the mountain-pass town's) and the saddle north of it to the map's edge.
PASS_VALLEY = (2300, 760, 180, 120)     # x, y, rx, ry
PASS_SADDLE = [(2300, 760), (2330, 560), (2310, 360), (2290, 120), (2290, 0)]
# Basins: Glasswater (the plains' great lake), the tarns, the Saltpan (the Red Waste's dry lake, the only sink).
GLASSWATER = (1960, 2080, 260, 150, -.35)          # x, y, rx, ry, angle
TARNS = [(1700, 560, 60, 40), (2620, 640, 50, 36), (3260, 760, 56, 40), (1240, 520, 44, 32)]
SALTPAN = (3520, 3330, 300, 190, .3)
OASIS = (3240, 2930)
# The Red Waste's forms (x, y, rx, ry): its dune seas, its mesas and buttes, its badlands under the Ochre Hills.
DUNES = [(3800, 2740, 330, 250), (3560, 3900, 460, 170)]
MESAS = [(3360, 2480, 300, 190), (3960, 3380, 190, 330)]
BADLANDS = [(3060, 3150, 170, 330), (3000, 3560, 150, 260), (3180, 3380, 120, 160)]
# The city's roads that leave its cells, and where a realm road takes each on: (the first realm tile, the city's last).
GATE_ROADS = {'E': ((772, 250), (767, 250)), 'S': ((412, 772), (412, 767)), 'N': ((366, 40), (361, 40))}
# The reserved sites (doc 70): (id, kind, working name, near (x, y), how far to look, size (radius), what it is for in
# doc 71's food plan or the industries ('spare': another candidate), what the ground is). Towns take the old map's
# town names (doc 60) where the country suits them, so their people can follow. Sizes: about 6.5 cells of the farms'
# own fields and 7.5 of shared range (the user: "somewhere in the middle ... a player should feel like they're big").
SITES = [
    ('oakhaven', 'town', 'Oakhaven', (1010, 1220), 180, 60, 'town', 'a harbour town at the head of Cedar Sound, where the Rainrun and the Alder come down to the sea; timber and fish'),
    ('frosthollow', 'town', 'Frosthollow', (2300, 800), 240, 60, 'town', 'the mountain town in the high valley under the Whitefangs\' pass; its mines above it'),
    ('coldwater', 'town', 'Coldwater', (1980, 1900), 260, 60, 'town', 'a lake town on Glasswater\'s north shore, where the East Road and the River Road meet'),
    ('larkfield', 'town', 'Larkfield', (1640, 2400), 200, 60, 'town', 'the river-crossing town where the River Road crosses the Long River, among the middle country\'s grain'),
    ('mirebridge', 'town', 'Mirebridge', (1420, 3020), 160, 60, 'town', 'the fen-edge town on the last dry ground where the rivers enter the Sallow Fens'),
    ('southmeadow', 'town', 'Southmeadow', (2150, 3350), 260, 60, 'town', 'a market town of the warm south-middle country, among orchards and vines'),
    ('saltwell', 'town', 'Saltwell', (3310, 2930), 140, 60, 'town', 'the oasis town of the Red Waste, by its spring pool, on the way to the Saltpan'),
    # Doc 71's food producers.
    ('grain_long', 'farmland', 'the Long River grain', (1760, 2330), 260, 182, 'grain farm 1 (large, with a water mill)', 'grain land on the Long River between Glasswater and Larkfield, in several fields'),
    ('mill_long', 'mill', 'the Long River mill race', 'grain_long', 240, 12, 'grain farm 1\'s water mill', 'a bank of the Long River for a water mill'),
    ('grain_lark', 'farmland', 'the Lark grain', (2150, 2800), 300, 182, 'grain farm 2 (large, with a water mill)', 'grain land along the Lark in the warm middle country, in several fields'),
    ('mill_lark', 'mill', 'the Lark mill race', 'grain_lark', 260, 12, 'grain farm 2\'s water mill', 'a bank of the Lark for a water mill'),
    ('orchard_greenholt', 'orchard', 'the Greenholt orchard', (1550, 1550), 260, 78, 'orchard (central hills) 1', 'apples and pears on the Greenholt\'s swells'),
    ('orchard_coldwater', 'orchard', 'the Upper Mere orchard', (2180, 1700), 260, 78, 'orchard (central hills) 2', 'apples on the slopes above the Upper Mere'),
    ('orchard_larkfield', 'orchard', 'the Larkfield orchard', (1900, 2650), 260, 78, 'orchard (central hills) 3', 'plums and apples south of the Long River'),
    ('orchard_rainward', 'orchard', 'the Rainward orchard', (640, 900), 220, 75, 'orchard (NW coast valley) 1', 'apples on a sheltered slope south-east of the city'),
    ('orchard_coastvale', 'orchard', 'the Coastvale orchard', (480, 1560), 260, 75, 'orchard (NW coast valley) 2', 'apples and cherries in a coast-range valley south of Cedar Sound'),
    ('vineyard_ochre', 'vineyard', 'the Ochre vineyard', (2760, 2700), 260, 87, 'vineyard 1 (SE desert edge)', 'vines on the Ochre Hills\' warm west slopes, at the Red Waste\'s edge'),
    ('vineyard_south', 'vineyard', 'the Southmeadow vines', (2400, 3150), 260, 87, 'vineyard 2 (central hills)', 'vines on the south-middle hills'),
    ('dairy_southgate', 'dairy', 'the Southgate dairy', (430, 960), 220, 85, 'cattle and dairy farm 1', 'a dairy farm\'s hay meadows and steading outside the South Gate'),
    ('dairy_rainrun', 'dairy', 'the Rainrun dairy', (820, 1000), 220, 85, 'cattle and dairy farm 2', 'hay meadows on the Rainrun\'s bottom land'),
    ('dairy_rainward', 'dairy', 'the Rainward dairy', (900, 560), 220, 85, 'cattle and dairy farm 3', 'hay meadows on the Rainward Hills\' lower slopes'),
    ('range_city', 'woodpasture', 'the city common', (620, 640), 160, 165, 'cattle range (shared, near the city)', 'wood pasture east and south of the walls: the cattle graze under the fir and cedar'),
    ('range_southgate', 'pasture', 'the Southgate range', (380, 1180), 260, 165, 'cattle range (shared, NW coast)', 'cattle grazing along the coast north of Cedar Sound'),
    ('range_rainrun', 'pasture', 'the Rainrun range', (900, 850), 260, 165, 'cattle range (shared, NW coast)', 'cattle grazing in the Rainrun valley'),
    ('pigs_greenholt', 'pigfarm', 'the Greenholt piggery', (1450, 1700), 260, 45, 'pig farm 1', 'a pig farm\'s yards at the edge of the Greenholt woods'),
    ('pigwood_greenholt', 'woodpasture', 'the Greenholt pannage', 'pigs_greenholt', 320, 115, 'pig farm 1\'s woods (shared range)', 'oak and beech woods for the pigs\' pannage'),
    ('pigs_alder', 'pigfarm', 'the Alder piggery', (1750, 1400), 260, 45, 'pig farm 2', 'a pig farm\'s yards in the Alder valley\'s woods'),
    ('pigwood_alder', 'woodpasture', 'the Alder pannage', 'pigs_alder', 320, 115, 'pig farm 2\'s woods (shared range)', 'woods for the pigs'),
    ('pigs_south', 'pigfarm', 'the Southwood piggery', (1500, 2050), 300, 45, 'pig farm 3', 'a pig farm\'s yards in the woods west of Glasswater'),
    ('pigwood_south', 'woodpasture', 'the Southwood pannage', 'pigs_south', 320, 115, 'pig farm 3\'s woods (shared range)', 'woods for the pigs'),
    ('sheep_downs', 'pasture', 'the Larkdowns sheepwalk', (2500, 2100), 280, 230, 'sheep and goat run (central downs)', 'sheep and goats on the dry downs'),
    ('sheep_foothills', 'pasture', 'the Greyteeth sheepwalk', (2780, 1550), 300, 220, 'sheep and goat run (N/NE foothills)', 'sheep and goats on the Greyteeth\'s foothills'),
    ('warren_dunes', 'warren', 'the Fenshore warren', (760, 3650), 320, 60, 'rabbit warren (SW marsh dunes)', 'rabbits in the dunes along the fens\' sea shore'),
    ('warren_waste', 'warren', 'the Ochre warren', (2950, 2600), 300, 60, 'rabbit warren (SE desert edge)', 'rabbits on the scrubby edge of the Red Waste'),
    ('fishing_sound', 'fishing', 'the Sound landing', (600, 1300), 160, 12, 'sea fishing boat 1 (Ridgemere\'s sound)', 'a landing on Cedar Sound'),
    ('fishing_northcove', 'fishing', 'North Cove', (130, 1050), 160, 12, 'sea fishing boat 2 (a north cove)', 'a cove on the rain coast south of the city'),
    ('fishing_delta', 'fishing', 'the Long River mouth', (920, 3060), 220, 12, 'sea fishing boat 3 (SW marsh coast)', 'the Long River\'s mouth'),
    ('fishing_fencoast', 'fishing', 'the Fen shore', (700, 3800), 300, 12, 'sea fishing boat 4 (SW marsh coast)', 'the fens\' sea shore'),
    ('fishing_glasswater', 'fishing', 'Glasswater landing', (1900, 2160), 300, 12, 'lake and river boat 1', 'Glasswater\'s south shore'),
    ('fishing_uppermere', 'fishing', 'the Upper Mere landing', (2420, 1650), 220, 12, 'lake and river boat 2', 'the Upper Mere, above Glasswater'),
    ('marsh_eels', 'marsh', 'the eel creeks', (1000, 3450), 260, 95, 'the eel and fowl marsh', 'creeks, pools and reed beds for eels and wildfowl'),
    ('bees_rainward', 'apiary', 'the Rainward bee garth', (980, 300), 240, 26, 'beekeeper 1', 'heather and clover on the Rainward Hills'),
    ('bees_greenholt', 'apiary', 'the Greenholt bee garth', (1300, 1150), 260, 26, 'beekeeper 2', 'meadow flowers in the lower Alder valley'),
    ('bees_south', 'apiary', 'the Sunfields bee garth', (2000, 3100), 260, 26, 'beekeeper 3', 'meadows and orchards of the south-middle country'),
    ('salt_saltpan', 'salt', 'the Saltpan workings', (3400, 3330), 300, 30, 'salt works 1 (SE desert pan)', 'salt raked from the Saltpan\'s crust'),
    ('salt_fencoast', 'saltcoast', 'the Fen salterns', (880, 3350), 300, 34, 'salt works 2 (SW marsh coast)', 'flat shore for sea-salt pans'),
    ('fishing_southcove', 'fishing', 'Seal Rocks', (190, 1800), 200, 12, 'spare: sea fishing', 'a landing among the rain coast\'s rocks'),
    ('fishing_larkfield', 'fishing', 'the Larkfield weir', (1580, 2470), 220, 12, 'spare: river boat', 'the Long River at Larkfield'),
    ('fishing_rainmere', 'fishing', 'Rainmere landing', (1000, 760), 160, 12, 'spare: lake boat', 'Rainmere, in the Rainward Hills'),
    # The industries (doc 61): timber, charcoal, stone, clay, ore (the Country's, for now).
    ('timber_rainwood', 'timber', 'the Rainwood cut', (600, 1150), 200, 24, 'timber (coastal rainforest)', 'cedar and fir in the Rainwood, near the city'),
    ('timber_coast', 'timber', 'the coast range cut', (350, 2000), 260, 24, 'timber (coastal rainforest)', 'big cedar in the coast range'),
    ('timber_sound', 'timber', 'the Soundside cut', (700, 1500), 260, 24, 'timber (coastal rainforest)', 'cedar on Cedar Sound\'s south shore'),
    ('timber_front', 'timber', 'the Kestrel pines', (1700, 1000), 260, 24, 'timber (foothills)', 'pine under the Whitefangs'),
    ('timber_upper', 'timber', 'the Greyteeth pines', (2700, 1050), 300, 24, 'timber (foothills)', 'pine on the upper Long River'),
    ('charcoal_greenholt', 'charcoal', 'the Greenholt charcoal hearths', (1450, 1900), 300, 18, 'charcoal', 'charcoal burners\' hearths in the Greenholt woods'),
    ('charcoal_rainwood', 'charcoal', 'the Rainwood charcoal hearths', (520, 1250), 260, 18, 'charcoal', 'charcoal burners\' hearths in the Rainwood'),
    ('quarry_granite', 'quarry', 'the Rainward granite quarry', (1000, 420), 220, 20, 'stone (granite)', 'granite for the city\'s walls and buildings (House Greatly\'s trade)'),
    ('quarry_slate', 'quarry', 'the Rainward slate quarry', (1300, 650), 260, 18, 'stone (slate)', 'slate for the city\'s roofs'),
    ('quarry_lime', 'quarry', 'the lime quarry', (1500, 900), 260, 20, 'stone (limestone)', 'limestone for the lime kilns and the plasterworks'),
    ('quarry_sandstone', 'quarry', 'the Ochre sandstone quarry', (2900, 2900), 260, 20, 'stone (sandstone)', 'red sandstone in the Ochre Hills'),
    ('clay_mirebridge', 'clay', 'the Mirebridge clay pits', (1100, 3200), 260, 14, 'clay', 'clay for bricks and tiles'),
    ('clay_fens', 'clay', 'the Sallow clay pits', (1500, 3400), 300, 14, 'clay', 'clay in the fens'),
    ('mine_frost_iron', 'mine', 'the Frosthollow iron mine', (2450, 620), 260, 14, 'ore (iron; N mountains)', 'iron ore above Frosthollow'),
    ('mine_frost_lead', 'mine', 'the Kestrel lead and silver mine', (2100, 620), 260, 14, 'ore (lead, silver; N mountains)', 'lead and silver ore above the pass'),
    ('mine_grey_iron', 'mine', 'the Greyteeth iron mine', (3300, 1250), 300, 14, 'ore (iron; NE mountains)', 'iron ore in the Greyteeth'),
    ('mine_grey_silver', 'mine', 'the Greyteeth silver mine', (3500, 1000), 300, 14, 'ore (silver; NE mountains)', 'silver in the high Greyteeth'),
    ('mine_grey_tin', 'mine', 'the Greyteeth tin mine', (3750, 1350), 300, 14, 'ore (tin; NE mountains)', 'tin ore under the eastern spurs'),
    ('mine_ochre_copper', 'mine', 'the Ochre copper mine', (3000, 2400), 300, 14, 'spare: ore (copper)', 'copper in the Ochre Hills'),
]
# Kinds whose ground is a zone eased rather than levelled; kinds left as the land is (only thinned); fields and range.
FIELDS = {'farmland', 'pasture', 'orchard', 'vineyard', 'dairy', 'woodpasture', 'warren', 'marsh', 'apiary'}
UNCLEARED = {'woodpasture', 'marsh'}
OWN_FIELDS = {'farmland', 'orchard', 'vineyard', 'dairy', 'pigfarm', 'warren', 'apiary', 'mill'}
RANGE = {'pasture', 'woodpasture'}
SHORE_TOWNS = {'oakhaven', 'coldwater', 'larkfield', 'mirebridge', 'saltwell'}
# What a site's ground is cleared to (codes picked evenly), and a share of its edge given another (code).
GROUND_OF = {
    'town': (',,,"', 0, ''), 'farmland': (',,,,""', .05, 'B'), 'pasture': (',,,,""3', .03, 'B'), 'orchard': (',,,"', 0, ''),
    'vineyard': (';,,', 0, ''), 'fishing': ('00,', 0, ''), 'mine': ('XXrr.', 0, ''), 'quarry': ('rrX', 0, ''),
    'clay': ('DD.', 0, ''), 'salt': ('))))0', 0, ''), 'timber': (',&&!', .3, '7'), 'mill': ('.,0', 0, ''),
    'dairy': (',,,,""', .04, 'B'), 'pigfarm': (',.,D', 0, ''), 'woodpasture': (',', 0, ''), 'warren': ('"0,;', 0, ''),
    'marsh': ('', 0, ''), 'apiary': (',,,3"', 0, ''), 'charcoal': (',..', .2, '7'), 'saltcoast': ('0D.', 0, ''),
}
CLEARABLE = [ord(c) for c in '$PY1Bo7?@2']
LEVELLED = {'town', 'mine', 'quarry', 'clay', 'salt', 'timber', 'fishing', 'mill', 'pigfarm', 'charcoal', 'saltcoast'}
# The roads (name, from, to, width): from the city's gates to the towns, and between them.
ROADS = [
    ('The Pass Road', 'N', 'frosthollow', 3),
    ('The Pass', 'frosthollow', (2290, 6), 3),
    ('The East Road', 'E', 'coldwater', 3),
    ('The Sound Road', 'S', 'oakhaven', 3),
    ('The Coast Road', 'oakhaven', 'mirebridge', 3),
    ('The River Road', 'coldwater', 'larkfield', 3),
    ('The Fen Road', 'larkfield', 'mirebridge', 3),
    ('The High Road', 'frosthollow', 'coldwater', 3),
    ('The South Road', 'larkfield', 'southmeadow', 3),
    ('The Waste Road', 'southmeadow', 'saltwell', 3),
    ('The Downs Road', 'coldwater', 'saltwell', 3),
]
# Each biome's ground (weights), trees (codes, picked evenly), their density in clumps, and extras (code, share).
PALETTES = {
    'rainforest': ({'!': 4, '&': 4, ',': 1.2, '"': .4}, '$$$$PPPY', .58, [('7', .025), ('o', .005), ('B', .03), ('2', .003)]),
    'woodland': ({',': 4, '"': 2, '&': 1.2, '!': 1, '3': .4}, 'YYYYYPB', .5, [('B', .03), ('7', .006), ('o', .003)]),
    'meadow': ({',': 6, '"': 4, '3': 1.2, '5': .5}, 'YYB', .05, [('B', .02), ('o', .003)]),
    'heath': ({'5': 5, ',': 3, '!': 1, '"': 1}, 'PB', .06, [('o', .01), ('B', .03)]),
    'steppe': ({';': 6, ',': 3, '"': 1, '0': .5}, 'B', .05, [('2', .002), ('o', .004)]),
    'scrub': ({';': 4, '0': 3, '(': 1.2, '.': 1, 'X': .8}, 'B', .05, [('?', .014), ('o', .006), ('2', .002)]),
    'sand': ({'0': 9, ';': 1, '(': 1.2, 'X': .8}, '', 0, [('?', .004), ('o', .002)]),
    'montane': ({',': 3, '!': 2, '-': 1, '&': .5}, 'PPPP1', .55, [('o', .01), ('7', .01)]),
    'subalpine': ({',': 3, '-': 3, '3': .3, 'r': .8, '!': .6}, '1P', .16, [('o', .02)]),
    'alpine': ({'-': 3, 'r': 3, 's': 2, '*': 1}, '', 0, [('o', .03)]),
    'snow': ({'*': 10, 'r': 1, 's': .6}, '', 0, []),
    'marsh': ({'"': 3, 'E': 3, '!': 2, 'D': 2, 'w': 2, ',': 1}, '@@@2Y', .14, [('B', .02)]),
}
# Open country is painted calmly: its ground picked tile by tile (not in blotches) from a mix drifting slowly between
# two, a over b (a pasture's short grass and a hay meadow's tall grass and flowers; the downs' dry grass and their greener
# hollows), b in the hollows, over about this many tiles; then its scattered trees or shrubs (codes, density in clumps)
# and extras (code, share).
CALM = {
    'meadow': ({',': 8, '"': 1.6, '3': .25}, {'"': 6, ',': 3, '3': .9}, 220, 'Y', .004, [('B', .006), ('o', .001)]),
    'steppe': ({';': 8, ',': 1.5, '"': .3}, {',': 5, ';': 4, '"': 1}, 160, 'B', .03, [('o', .002), ('2', .0005)]),
    'scrub': ({';': 5, '0': 3, '.': .6, 'X': .5, '(': .5}, {'0': 5, ';': 3, 'X': 1, '(': 1}, 140, 'BB?', .05, [('o', .004)]),
    'sand': ({'0': 9, ';': 1, 'X': .6, '(': .6}, {'0': 7, 'X': 1.5, '(': 1.5, ';': .5}, 200, '?', .002, [('o', .002)]),
}
# The Rainrun's valley, down the east side of the city's hills to the head of Cedar Sound.
RAINRUN = [(1180, 60), (1000, 420), (900, 760), (880, 1060), (930, 1290)]
# The Long River's valley, from the Greyteeth through Glasswater to the fens' delta.
LONG_RIVER = [(3200, 980), (2900, 1250), (2560, 1560), (2240, 1860), (1960, 2080), (1640, 2460), (1300, 2820),
              (980, 3140), (700, 3420), (560, 3600)]

RIVER_NAMES = ['the Lark', 'the Whitewater', 'the Kestrel', 'the Greybeck', 'the Coldbeck', 'the Shiver', 'the Tumble',
               'the Brack', 'the Hollowrun', 'the Sallow', 'the Rush', 'the Mossbeck', 'the Stillwater', 'the Linnet']
LAKE_NAMES = {(1960, 2080): 'Glasswater', (2440, 1660): 'the Upper Mere', (1010, 720): 'Rainmere', (1750, 540): 'Highmere',
              (2620, 640): 'Kestrel Tarn', (3260, 780): 'Greytarn', (1250, 600): 'Slate Tarn', (3250, 1980): 'Dustmere', (1360, 1290): 'Alder Mere'}
REGION_IDS = {'sea': 'western_sea', 'sound': 'cedar_sound', 'rainward': 'rainward_hills', 'rainwood': 'rainwood',
              'whitefangs': 'whitefangs', 'greyteeth': 'greyteeth', 'greenholt': 'greenholt', 'downs': 'larkdowns',
              'sunfields': 'sunfields', 'ochre': 'ochre_hills', 'fens': 'sallow_fens', 'waste': 'red_waste',
              'saltpan': 'saltpan'}
# Each region's usual weather (what climateOf reads: rain wet, snow cold, fog a marsh, sandstorm arid, clear temperate).
WEATHER = {'sea': 'rain', 'sound': 'rain', 'rainward': 'rain', 'rainwood': 'rain', 'whitefangs': 'snow', 'greyteeth': 'snow',
           'greenholt': 'clear', 'downs': 'clear', 'sunfields': 'clear', 'ochre': 'sandstorm', 'fens': 'fog',
           'waste': 'sandstorm', 'saltpan': 'sandstorm'}
REGION_TEXT = {
    'sea': ('The Western Sea', 'Grey water off the rain coast, rocks and islets standing out of the swell.'),
    'sound': ('Cedar Sound', 'A long arm of the sea reaching into the Rainwood, its shores steep with cedar and fir.'),
    'rainward': ('The Rainward Hills', 'Wet hills behind Ridgemere: fir and cedar in the folds, mossy rock and meadow on the tops, and the Whitefangs\' first snows above.'),
    'rainwood': ('The Rainwood', 'The rain coast\'s old forest: cedar and fir standing over fern and moss, fallen trunks rotting green, streams loud in ferny ravines.'),
    'whitefangs': ('The Whitefangs', 'The northern range: scree and bare rock, pine below and snowfields above, tarns in the high hollows.'),
    'greyteeth': ('The Greyteeth', 'The north-east\'s broken mountains: snow on the heights, pine in the valleys, and dry grass on their eastern spurs.'),
    'greenholt': ('The Greenholt', 'The middle country: rolling meadow and woodland, copses on the swells and streams in the hollows.'),
    'downs': ('The Larkdowns', 'Long dry swells of grass under a wide sky, shrubs in the hollows, the Ochre Hills on the skyline.'),
    'sunfields': ('The Sunfields', 'The warm south-middle country: open grassland and woods on gentle swells, good ground for grain, orchards and vines.'),
    'ochre': ('The Ochre Hills', 'Broken hills of red and yellow rock between the downs and the Red Waste: sandstone crags and ochre scree laid in beds, dry grass and scrub thinning to sand. The rain seldom gets this far.'),
    'fens': ('The Sallow Fens', 'Reed beds, bog and black pools, creeks winding through willow carr; mist lies here most mornings.'),
    'waste': ('The Red Waste', 'Dry country in the hills\' rain shadow: sand and gravel, scrub and cactus, red rock, dunes driven by the wind.'),
    'saltpan': ('The Saltpan', 'A dry lake bed of white salt crust in the heart of the Red Waste, a bitter pool at its lowest.'),
}
TOWN_GROUND = 'A level stretch of cleared ground here waits for a town.'
# The city's cells whose wild ground is now the rain coast's woods: their names and descriptions to match.
CITY_RENAMES = {
    (2, 0): ('The Rainward Woods, North', 'Cleared ground and fir woods north-east of the city walls, rising toward the Rainward Hills.'),
    (2, 2): ('The Rainward Woods, South', 'Rough pasture and fir woods south-east of the city walls, thickening into the Rainwood.'),
    (2, 1): ('Fellhall', 'House Fell\'s estate on the East Road, among cleared ground and fir woods: the hall, its press house, distillery and cellars.'),
    (1, 2): ('Timberland Court', 'House Timberland\'s court in the cleared woods south of the city, at the end of the Cinder Road.'),
}


def compass(cx, cy, cells):
    if not cells:
        return ''
    mx = sum(c[0] for c in cells) / len(cells)
    my = sum(c[1] for c in cells) / len(cells)
    sx, sy = max(1.0, (max(c[0] for c in cells) - min(c[0] for c in cells)) / 3), max(1.0, (max(c[1] for c in cells) - min(c[1] for c in cells)) / 3)
    ns = 'North' if cy < my - sy * .5 else 'South' if cy > my + sy * .5 else ''
    ew = 'West' if cx < mx - sx * .5 else 'East' if cx > mx + sx * .5 else ''
    return (f'{ns}-{ew.lower()}' if ns and ew else ns or ew or 'Middle')
OVERVIEW_LABELS = [((60, 1180), 'CEDAR SOUND'), ((1300, 120), 'THE WHITEFANGS'), ((3350, 780), 'THE GREYTEETH'),
                   ((1000, 560), 'RAINWARD HILLS'), ((300, 1700), 'THE RAINWOOD'), ((1500, 1500), 'THE GREENHOLT'),
                   ((2450, 1900), 'THE LARKDOWNS'), ((2700, 2250), 'OCHRE HILLS'), ((3300, 2380), 'THE RED WASTE'),
                   ((3330, 3170), 'THE SALTPAN'), ((800, 3400), 'THE SALLOW FENS'), ((1900, 3650), 'THE SUNFIELDS'),
                   ((1720, 2200), 'GLASSWATER'), ((20, 90), 'RIDGEMERE')]
# Close-ups (x, y, w, h) for the previews: each region, and the blends between them.
CLOSE_UPS = {
    'ridgemere-and-the-rainward-woods': (0, 0, 1100, 1000),
    'cedar-sound-and-oakhaven': (300, 1000, 1000, 700),
    'whitefangs-and-frosthollow': (1850, 250, 1000, 800),
    'glasswater-and-the-greenholt': (1550, 1650, 1000, 800),
    'sallow-fens-and-the-delta': (600, 2650, 1000, 900),
    'red-waste-saltpan-and-dunes': (2950, 2550, 1100, 1100),
    'blend-greenholt-downs-ochre-waste': (2200, 2100, 1300, 900),
    'blend-rainwood-to-whitefangs': (950, 250, 1000, 900),
    'blend-greenholt-to-fens': (900, 2300, 1000, 900),
    'greyteeth': (3200, 500, 896, 1100),
}

class Realm2:
    def __init__(self, city_codes, city_heights, seed=SEED, report=print, protect=()):
        self.seed = seed
        self.report = report
        self.rng = random.Random(seed)
        self.protect = list(protect)
        self.city_codes = city_codes
        self.city_heights = city_heights
        z = lambda dtype=np.float32: np.zeros((SIZE, SIZE), dtype=dtype)
        self.codes = np.full((SIZE, SIZE), ord(','), dtype=np.uint8)
        self.h = z()
        self.keep = z(bool)          # tiles left exactly as they are (code and height): the city's built things
        self.hold = z(bool)          # tiles whose height is held (the keep and a margin round it)
        self.locked = z(bool)        # tiles nothing later may repaint (the keep, roads, sites)
        self.sites = []
        self.roads = {}
        self.features = []
        self.timings = {}

    def t(self, name, t0):
        self.timings[name] = round(time.time() - t0, 1)
        self.report(f'  {name}: {self.timings[name]} s')

    # -- the city's cells: what is kept ----------------------------------------------------------------------
    def lay_city(self):
        t0 = time.time()
        cc, ch = self.city_codes, self.city_heights
        wild = np.isin(cc, [ord(c) for c in WILD])
        built = ~wild
        # The city's own water (the bay) stays; so does everything built, and a margin round it keeps its height.
        keep = np.zeros((CITY, CITY), dtype=bool)
        keep |= built
        # The walls' box and the Houses' grounds whole, with a margin: nothing in them is wild ground to remake.
        from .ridgemere2 import INSIDE
        x0, x1, y0, y1 = INSIDE
        keep[y0 - 6:y1 + 6, x0 - 6:x1 + 6] = True             # (the walls and all inside them)
        from .ridgemere2 import ESTATES
        for _, (_, (x, y, w, hh)) in ESTATES.items():
            keep[max(0, y - 2):y + hh + 2, max(0, x - 2):x + w + 2] = True
        for x, y in self.protect:                          # (every NPC's outdoor spot, post, door end and the spawn)
            if 0 <= x < CITY and 0 <= y < CITY:
                keep[max(0, y - 2):y + 3, max(0, x - 2):x + 3] = True
        built = built | keep
        near = field.distance_to_mask(built, 8)
        hold = keep | (near <= 4)
        self.keep[:CITY, :CITY] = keep
        self.hold[:CITY, :CITY] = hold
        self.codes[:CITY, :CITY] = cc
        self.h[:CITY, :CITY] = ch
        self.locked |= self.keep
        self.city_wild = wild & ~keep
        self.city_built_c = down(keep.astype(np.float32), G) > .3
        self.t('city', t0)

    # -- the sea ------------------------------------------------------------------------------------------------
    def lay_sea(self):
        t0 = time.time()
        ys = np.arange(SIZE, dtype=np.float32)
        shore = np.interp(ys, [y for y, _ in COAST], [x for _, x in COAST]).astype(np.float32)
        wob = (field.fbm(SIZE, SIZE, 110, self.seed + 1, 3) - .5) * 120 + (field.fbm(SIZE, SIZE, 22, self.seed + 2, 3) - .5) * 26
        yy = np.arange(SIZE)[:, None].astype(np.float32)
        fade = smooth(CITY, CITY + 160, yy)                 # The shore leaves the city's exactly where it is.
        xs = np.arange(SIZE)[None, :].astype(np.float32)
        sea = xs < shore[:, None] + wob * fade
        # Cedar Sound.
        pts = densify(chaikin([(x, y) for x, y, _ in SOUND], 3), 4)
        widths = np.interp(np.linspace(0, 1, len(pts)), np.linspace(0, 1, len(SOUND)), [w for _, _, w in SOUND])
        # Its shores wander in coves and points, as a fjord's do: the distance from its line, over its width, against
        # noise at two scales.
        sw = field.fbm(1024, 512, 20, self.seed + 3, 2)
        bx0, by0, bx1, by1 = 0, 1150, 1100, 1600
        near = np.full((by1 - by0, bx1 - bx0), 9.0, dtype=np.float32)
        for k, ((x, y), w) in enumerate(zip(pts, widths)):
            w = w * (.8 + .5 * sw[int(y) // 4 % 512, int(x) // 4 % 1024])
            R = int(w * 1.8) + 3
            x0, x1, y0, y1 = max(bx0, int(x) - R), min(bx1, int(x) + R), max(by0, int(y) - R), min(by1, int(y) + R)
            yy_, xx_ = np.mgrid[y0:y1, x0:x1]
            view = near[y0 - by0:y1 - by0, x0 - bx0:x1 - bx0]
            np.minimum(view, np.hypot(xx_ + .5 - x, yy_ + .5 - y) / w, out=view)
        wav = (field.fbm(bx1 - bx0, by1 - by0, 26, self.seed + 5, 3) - .5) * .7 + \
            (field.fbm(bx1 - bx0, by1 - by0, 7, self.seed + 6, 2) - .5) * .25
        sound = np.zeros((SIZE, SIZE), dtype=bool)
        sound[by0:by1, bx0:bx1] = near + wav < 1
        sea |= sound
        # Inside the city's cells the sea is the city's own.
        sea[:CITY, :CITY] = np.isin(self.city_codes, [ord('W'), ord('~')]) & ~np.isin(self.city_codes, [ord('8')])
        # A few rocks and islets off the rain coast.
        rng = random.Random(self.seed + 4)
        self.islets = []
        for _ in range(26):
            y = rng.uniform(CITY + 40, 2300)
            x = float(np.interp(y, [y_ for y_, _ in COAST], [x_ for _, x_ in COAST])) - rng.uniform(14, 60)
            r = rng.choice([2, 3, 3, 4, 6, 9, 14])
            if x - r < 4:
                continue
            yy_, xx_ = np.mgrid[int(y - r - 2):int(y + r + 3), int(x - r - 2):int(x + r + 3)]
            n = field.fbm(r * 2 + 5, r * 2 + 5, max(2, r / 2), rng.randrange(10 ** 6), 2)
            m = np.hypot(xx_ + .5 - x, yy_ + .5 - y) < r * (.7 + .6 * n)
            sea[yy_[m], xx_[m]] = False
            self.islets.append((x, y, r))
        # The sea is what joins the west edge; specks of sea inland and of land at sea go.
        self.sea = keep_joined(sea, [(1, y) for y in range(8, SIZE, 64)])
        self.sea[:CITY, :CITY] = sea[:CITY, :CITY]
        land = ~self.sea
        lone = ~keep_joined(land, [(SIZE - 2, y) for y in range(8, SIZE, 64)])
        islets = np.zeros_like(land)
        for x, y, r in self.islets:
            islets[int(y) - r - 3:int(y) + r + 4, int(x) - r - 3:int(x) + r + 4] = True
        self.sea |= lone & ~islets & (np.arange(SIZE)[:, None] >= CITY)
        self.t('sea', t0)

    # -- the shape of the land (coarse) ---------------------------------------------------------------------------
    def lay_elevation(self):
        t0 = time.time()
        xs, ys = grid_xy(NG, G)
        s = self.seed
        sea = down(self.sea.astype(np.float32)) > .5
        self.sea_c = sea
        dc = field.distance_to_mask(sea, 220).astype(np.float32) * G                  # tiles from the sea
        # Bent coordinates, so nothing comes out round or ruled.
        wx = (field.fbm(NG, NG, 40, s + 10, 3) - .5) * 220 + (field.fbm(NG, NG, 9, s + 11, 2) - .5) * 50
        wy = (field.fbm(NG, NG, 40, s + 12, 3) - .5) * 220 + (field.fbm(NG, NG, 9, s + 13, 2) - .5) * 50
        px, py = xs + wx, ys + wy
        # The land rises from the sea; the south-west lies low (the fens), the south-east high (the Red Waste).
        base = .7 + 3.0 * smooth(0, 1400, dc)
        fens = ellipse(px, py, 700, 3500, 1300, 1050, .5, 2.4)
        waste = ellipse(px, py, 3600, 3350, 1050, 1000, 0, 2.6)
        middle = ellipse(px, py, 1900, 2300, 1200, 900, -.3, 2)
        base = base * (1 - .82 * fens) + 2.2 * waste - .6 * middle * (1 - fens)
        # Rolling country: long swells everywhere but the fens, stronger in the hills north of the middle.
        swell = field.fbm(NG, NG, 30, s + 14, 4)
        hills = 1 + 1.4 * ellipse(px, py, 1500, 1350, 900, 520, .2) + 1.2 * ellipse(px, py, 2400, 2700, 700, 600, 0)
        e = base + (swell - .45) * 2.0 * hills * (1 - .9 * fens)
        # The fall of the land: high in the north-east, low in the south-west, so the middle drains to the fens; the
        # Red Waste falls to the Saltpan.
        e = e + 2.2 * smooth(-500, 3000, px - py * .9) * (1 - waste)
        e = e - 2.6 * ellipse(px, py, SALTPAN[0], SALTPAN[1], 1000, 800, .3, 1.3)
        # The Long River's valley, guiding the drainage from the Greyteeth through Glasswater to the fens.
        lv = np.full(e.shape, 1e9, dtype=np.float32)
        lt = np.zeros(e.shape, dtype=np.float32)
        total = sum(math.dist(a, b) for a, b in zip(LONG_RIVER, LONG_RIVER[1:]))
        run = 0.0
        for a, b in zip(LONG_RIVER, LONG_RIVER[1:]):
            d, t = seg_dist(px, py, a, b)
            better = d < lv
            lv[better] = d[better]
            lt[better] = ((run + t * math.dist(a, b)) / total)[better]
            run += math.dist(a, b)
        trough = np.exp(-(lv / 300) ** 2)
        e = e - 1.6 * trough * smooth(0, .15, lt)
        e = e * (1 - .7 * trough) + .7 * trough * np.minimum(e, .8 + 4.5 * (1 - lt))
        # The Rainrun's valley east of the city, down to the head of Cedar Sound: the hills behind Ridgemere drain to
        # it, not through the city.
        rd = np.full(e.shape, 1e9, dtype=np.float32)
        for a, b in zip(RAINRUN, RAINRUN[1:]):
            rd = np.minimum(rd, seg_dist(px * .3 + xs * .7, py * .3 + ys * .7, a, b)[0])
        e = e - 1.4 * np.exp(-(rd / 110) ** 2)
        # The ranges, on bent coordinates: the mountains' crests worn into valleys and the sharp ridges between them as
        # the water off them would wear them (carve_range); the low hills by the coast (the Rainward Hills, the coast
        # range) rounded, with ridged noise for their spurs and gullies.
        mount = np.zeros_like(e)
        hills = np.zeros_like(e)
        for name, peaks in RANGES.items():
            plan = hills if name in LOW_RANGES else mount
            for a, b in zip(peaks, peaks[1:]):
                if math.dist(a[:2], b[:2]) < 520:          # (a gap wider than that is a pass between two ranges)
                    ridge(plan, a, b, px, py)
            for p in peaks:
                ridge(plan, p, (p[0] + 1, p[1], p[2], p[3]), px, py)
        rid = ridged(NG, NG, 14, s + 15, 4)
        hills = hills + (rid - .5) * 4.5 * smooth(2.5, 10, hills) * (hills > 1.5)
        e = np.maximum(e, e * .35 + hills)                 # (the low fens raised above the sea, too)
        e = np.maximum(e, carve_range(e, mount, sea, px, py, s))
        mount = np.maximum(mount, hills)
        # The pass: a high valley and the saddle north of it.
        vx, vy, rx, ry = PASS_VALLEY
        valley = ellipse(xs, ys, vx, vy, rx, ry, 0, 2.2)
        e = e * (1 - valley) + valley * np.minimum(e, 7.0)
        sd = np.full(e.shape, 1e9, dtype=np.float32)
        for a, b in zip(PASS_SADDLE, PASS_SADDLE[1:]):
            d, _ = seg_dist(xs, ys, a, b)
            sd = np.minimum(sd, d)
        cut = np.exp(-(sd / 90) ** 2)
        e = e * (1 - cut) + cut * np.minimum(e, 7.0 + 2.5 * smooth(760, 300, ys))
        # Basins: Glasswater, the tarns, the Saltpan.
        gx, gy, grx, gry, ga = GLASSWATER
        bowl = ellipse(px * .2 + xs * .8, py * .2 + ys * .8, gx, gy, grx, gry, ga, 2.2)
        e = e - 2.4 * bowl
        for tx, ty, trx, tr in TARNS:
            e = e - 2.0 * ellipse(xs, ys, tx, ty, trx, tr, 0, 2)
        sx, sy, srx, sry, sa = SALTPAN
        pan = ellipse(px * .3 + xs * .7, py * .3 + ys * .7, sx, sy, srx, sry, sa, 2.6)
        e = e - 3.4 * pan
        # By the city, the land meets the city's own (about 1-2.5 by its edges).
        near_city = np.clip(1 - np.maximum(xs - CITY, ys - CITY) / 400, 0, 1) * ((xs < CITY + 400) & (ys < CITY + 400))
        e = e * (1 - .55 * near_city) + .55 * near_city * np.clip(e, 1, 4.5)
        # In the city's cells, the city's own ground (its wild ground rises gently east and south from it).
        cityh = down(self.city_heights, G)
        n = CITY // G
        ci = np.clip(cityh, 0, None) + .4 * smooth(300, 768, np.maximum(xs[:n, :n], ys[:n, :n]))
        e[:n, :n] = np.where(self.city_built_c, cityh, np.maximum(ci, .6 * e[:n, :n] + .4 * ci))
        e[sea] = -.5
        self.e = e.astype(np.float32)
        self.px, self.py = px, py
        self.fens_c, self.waste_c, self.mount_c = fens, waste, mount
        self.t('elevation', t0)

    # -- rain, warmth (coarse) ------------------------------------------------------------------------------------
    def lay_climate(self):
        t0 = time.time()
        e = np.maximum(self.e, 0)
        sea = self.sea_c
        # A wind from the west-north-west: each path starts at the west edge (or the north edge) and crosses the land,
        # raining as it climbs and as it goes; over the sea it takes water up again.
        slope = .32                                         # southward per eastward step
        starts = np.arange(-int(NG * slope) - 2, NG, dtype=np.float32)
        q = np.where(starts >= 0, 1.0, .55).astype(np.float32)
        rain = np.zeros((NG, NG), dtype=np.float32)
        hits = np.zeros((NG, NG), dtype=np.float32)
        prev = np.zeros_like(q)
        for x in range(NG):
            y = starts + slope * x
            inside = (y >= 0) & (y < NG)
            yi = np.clip(y.astype(int), 0, NG - 1)
            here = e[yi, x]
            wet = sea[yi, x]
            lift = np.maximum(0, here - prev) * inside
            p = np.where(wet, 0, q * (.0065 + .055 * lift)) * inside
            q = np.where(wet & inside & (x < 40), np.minimum(1, q + .08), q - p)
            np.add.at(rain, (yi[inside], np.full(inside.sum(), x)), p[inside])
            np.add.at(hits, (yi[inside], np.full(inside.sum(), x)), 1)
            prev = np.where(inside, here, prev)
        rain = rain / np.maximum(hits, 1)
        rain = blur(rain, 6)
        phys = np.clip(rain / np.percentile(rain[~sea], 85), 0, 1.4)
        xs, ys = grid_xy(NG, G)
        px, py = self.px, self.py
        # The plan's wetness, blended with the wind's: the rain coast and the fens wet, the middle fair, the Red Waste dry.
        plan = (.55 + .45 * ellipse(px, py, 300, 900, 900, 1500, 0, 2) + .35 * self.fens_c
                + .3 * smooth(3, 10, self.mount_c) * (1 - smooth(1400, 2000, py))
                + .2 * smooth(1.5, 6, blur(self.mount_c, 10)) * (1 - smooth(1200, 1900, py))
                - .55 * ellipse(px, py, 3700, 3500, 1250, 1200, 0, 2.2) - .15 * smooth(2000, 3600, px))
        m = np.clip(.45 * phys + .55 * plan, 0, 1.2)
        m[:2] = m[2]
        m[sea] = 1
        self.m_c = blur(m, 3)
        self.t('climate', t0)

    # -- water: drainage, rivers and lakes (coarse) --------------------------------------------------------------
    def lay_drainage(self):
        t0 = time.time()
        e = self.e.copy()
        sea = self.sea_c
        xs, ys = grid_xy(NG, G)
        sx, sy = SALTPAN[:2]
        sink = np.zeros_like(sea)
        sink[int(sy) // G - 2:int(sy) // G + 3, int(sx) // G - 2:int(sx) // G + 3] = True
        edge = np.zeros_like(sea)
        edge[-1, :] = edge[:, -1] = True                      # (Rivers may leave the map south, east and north.)
        edge[0, CITY // G:] = True
        seeds = sea | sink | edge
        # The water runs round the city's cells, never through them (its own drains are its own).
        n = CITY // G
        e = e.copy()
        e[:n + 1, :n + 1] = np.where(sea[:n + 1, :n + 1], e[:n + 1, :n + 1], 60)
        filled, _ = priority_flood(e, seeds)
        # Shallow hollows filled flat would send rivers across them in ruled lines: a little noise on the flats, and
        # the flood again, lets them wander.
        wander = field.fbm(NG, NG, 6, self.seed + 30, 2)
        hollow = (filled - e > 1e-4) & ~seeds
        filled, _ = priority_flood(np.where(hollow, filled + wander * .08, e).astype(np.float32), seeds)
        box = np.zeros_like(sea)
        box[:n + 1, :n + 1] = True
        filled = np.where(box, self.e, filled).astype(np.float32)
        self.filled = filled
        # Downstream: the lowest of the eight neighbours of the filled surface (always lower, by the flood's epsilon).
        down_idx = steepest(filled, seeds)
        order = np.argsort(-filled, axis=None)
        rainfall = np.clip(self.m_c, 0, 1.2) ** 1.6 + .02
        acc = rainfall.ravel().astype(np.float64).copy()
        di = down_idx.ravel()
        for i in order:
            d = di[i]
            if d >= 0:
                acc[d] += acc[i]
        self.acc = acc.reshape(e.shape)
        self.down = down_idx
        self.sink_c = sink
        lake = (filled - e > .5) & ~sea
        # Only basins of some size are lakes; the rest are hollows the rivers run through.
        self.lake_c = (drop_small(lake, 40) | ((filled - e > .3) & self.designed_lakes())) & ~box
        self.t('drainage', t0)

    def designed_lakes(self):
        xs, ys = grid_xy(NG, G)
        m = ellipse(xs, ys, *GLASSWATER[:4], GLASSWATER[4], 2) > .3
        for tx, ty, rx, ry in TARNS:
            m |= ellipse(xs, ys, tx, ty, rx, ry, 0, 2) > .3
        return m

    def rivers(self, threshold=2500.0, stream=260.0):
        """The river and stream lines: chains of coarse nodes from their sources down to the sea, a lake, the Saltpan or
        a bigger river, each with its flow at every point."""
        acc, down_idx = self.acc, self.down
        big = acc >= stream
        n = NG
        flat_down = down_idx.ravel()
        up_count = np.zeros(n * n, dtype=np.int32)
        idx = np.nonzero(big.ravel())[0]
        for i in idx:
            d = flat_down[i]
            if d >= 0 and big.ravel()[d]:
                up_count[d] += 1
        heads = [i for i in idx if up_count[i] == 0]
        seen = np.zeros(n * n, dtype=bool)
        lines = []
        # Longest first: trace from each head; a line stops where it meets one traced before (its confluence).
        heads.sort(key=lambda i: -self.filled.ravel()[i])
        for h in heads:
            chain, i = [], h
            while i >= 0:
                chain.append(i)
                if seen[i]:
                    break
                seen[i] = True
                if self.sea_c.ravel()[i] or self.sink_c.ravel()[i]:
                    break
                i = flat_down[i]
            if len(chain) >= 4:
                lines.append(chain)
        out = []
        for chain in lines:
            pts = [((i % n) * G + G / 2, (i // n) * G + G / 2) for i in chain]
            flow = [float(acc.ravel()[i]) for i in chain]
            out.append({'pts': pts, 'flow': flow, 'major': max(flow) >= threshold})
        return out


    # -- the ground's height, tile by tile ----------------------------------------------------------------------
    def lay_heights(self):
        t0 = time.time()
        s = self.seed
        Y, X = np.mgrid[0:SIZE, 0:SIZE].astype(np.float32)
        self.X, self.Y = X, Y
        h = blur(up(self.e), 4)
        m = smooth(2.5, 9, blur(up(self.mount_c), 4))
        fens = up(self.fens_c)
        waste = up(self.waste_c) * smooth(.45, .2, up(self.m_c))
        self.mountain, self.fens, self.waste = m, fens, waste
        # The regions' edges, bent and ragged, for what is painted by region (the fens, the badlands, the dunes).
        self.jit = (field.fbm(SIZE, SIZE, 160, s + 47, 3) - .5) * .7 + (field.fbm(SIZE, SIZE, 24, s + 48, 2) - .5) * .3
        self.fens_j = fens + self.jit
        # On the ranges: the crests' finer spurs and the gullies between them (ridged, on bent coordinates, sharpest on
        # the high ground), and a fine roughness of rock and scree. Elsewhere long gentle swells, barely rough at all.
        rows, cols = np.nonzero(m.max(axis=1) > .01)[0], np.nonzero(m.max(axis=0) > .01)[0]
        win = np.s_[rows.min():rows.max() + 1, cols.min():cols.max() + 1]
        wx, wy = X[win], Y[win]
        bend = (noise_at(wx, wy, 140, s + 38) - .5) * 60
        spurs = ridges_at(wx + bend, wy - bend, 110, s + 40, 3)
        h[win] += (spurs - .3) * 2.6 * m[win] * smooth(3, 8, h[win])
        del wx, wy, bend, spurs
        # The Ochre Hills: at the desert's edge a broad belt of broken hills, the sandstone laid in beds (so their slopes
        # climb in steps), cut by gullies. (The country either side of them is dry too: dry_edge, for the ground.)
        xs_c, ys_c = grid_xy(NG, G)
        od = np.full((NG, NG), 1e9, dtype=np.float32)
        for a, b in zip(RANGES['ochre'], RANGES['ochre'][1:]):
            od = np.minimum(od, seg_dist(self.px, self.py, a[:2], b[:2])[0])
        south = smooth(1250, 1550, ys_c)
        self.dry_edge = up(smooth(380, 120, od) * south) * (1 - fens)
        belt = up(smooth(290, 70, od) * smooth(1300, 1650, ys_c) * (blur(self.lake_c.astype(np.float32), 4) < .02)) * (1 - fens)
        rows, cols = np.nonzero(belt.max(axis=1) > .01)[0], np.nonzero(belt.max(axis=0) > .01)[0]
        win = np.s_[rows.min():rows.max() + 1, cols.min():cols.max() + 1]
        wx, wy = X[win], Y[win]
        bend = (noise_at(wx, wy, 160, s + 60) - .5) * 80
        hills = ridges_at(wx + bend, wy - bend * .6, 130, s + 61, 4)
        beds = np.floor(hills * 7 + (noise_at(wx, wy, 40, s + 62) - .5) * .8) / 7
        h[win] += belt[win] * (beds * 5.5 + hills * 1.5 - 1.0)
        del wx, wy, bend, hills, beds
        h += (field.fbm(SIZE, SIZE, 14, s + 41, 2) - .5) * (.9 * m + .12 * (1 - fens))
        h += (field.fbm(SIZE, SIZE, 170, s + 42, 3) - .5) * 1.6 * (1 - fens) * (1 - m)
        # The highest peaks: their heights eased in under the game's 16.
        h = np.where(h > 12, 12 + 3.7 * np.tanh((h - 12) / 3.7), h)
        crag = (m > .62) & (h > 13)
        # The Red Waste: dunes, mesas and buttes, badlands.
        self.dunes = np.zeros((SIZE, SIZE), dtype=np.float32)
        for cx, cy, rx, ry in DUNES:
            self.dunes = np.maximum(self.dunes, smooth(.25, .6, ellipse(X, Y, cx, cy, rx, ry, 0, 2.5)))
        warp = (field.fbm(SIZE, SIZE, 90, s + 43, 3) - .5) * 70
        wind = (X * .95 + Y * .31 + warp) / 46.0
        f = wind - np.floor(wind)
        dune = np.where(f < .72, f / .72, (1 - f) / .28)
        dune = dune ** 1.4 * (.6 + .8 * field.fbm(SIZE, SIZE, 120, s + 44, 2))
        h += dune * 2.0 * self.dunes
        self.mesa = np.zeros((SIZE, SIZE), dtype=np.float32)
        for cx, cy, rx, ry in MESAS:
            self.mesa = np.maximum(self.mesa, smooth(.3, .6, ellipse(X, Y, cx, cy, rx, ry, 0, 2.5) + self.jit * .8))
        mn = field.fbm(SIZE, SIZE, 60, s + 45, 3)
        tops = smooth(.55, .58, mn) * (self.mesa > .5)
        h += 4.0 * tops + 1.5 * smooth(.66, .69, mn) * self.mesa
        crag |= (tops > .05) & (tops < .95)
        self.mesa_top = tops > .95
        self.bad = np.zeros((SIZE, SIZE), dtype=np.float32)
        for cx, cy, rx, ry in BADLANDS:
            self.bad = np.maximum(self.bad, smooth(.3, .6, ellipse(X, Y, cx, cy, rx, ry, 0, 2.2) + self.jit * .9))
        br = ridged(SIZE, SIZE, 46, s + 46, 3)
        self.band = np.floor(br * 5 + (field.fbm(SIZE, SIZE, 30, s + 49, 2) - .5) * .6)
        terr = self.band / 5 * 3.2
        h += terr * self.bad
        crag |= self.bad > .5
        self.h = h
        self.crag = crag
        self.t('heights', t0)

    # -- water, tile by tile ------------------------------------------------------------------------------------
    def lay_water(self):
        t0 = time.time()
        h = self.h
        sea = self.sea
        # Lakes: where the drained surface stands above the ground, in the basins the drainage found.
        fu = blur(up(self.filled), 4)
        basin = up(self.lake_c.astype(np.float32)) > .2
        basin = grow_mask(basin, 24)
        lake = basin & (fu - h > .15) & ~sea
        lake = drop_small_full(lake, 400)
        lab = components4(lake)
        self.lake = lake
        self.lake_level = np.zeros((SIZE, SIZE), dtype=np.float32)
        self.lakes = []
        for k in np.unique(lab[lake]):
            mk = lab == k
            ys, xs = np.nonzero(mk)
            level = float(half(np.median(fu[mk])))
            self.lake_level[mk] = level
            self.lakes.append({'x': float(xs.mean()), 'y': float(ys.mean()), 'level': level, 'tiles': int(mk.sum()),
                               'box': (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))})
        # The Saltpan: the Red Waste's dry lake, flat salt round a brine pool.
        sx, sy = SALTPAN[:2]
        shape = ellipse(self.X, self.Y, sx, sy, SALTPAN[2] * .9, SALTPAN[3] * .9, SALTPAN[4], 2.4) + self.jit * .5
        pan = shape > .5
        self.pan = pan
        level = float(half(np.percentile(h[pan], 15)))
        self.pan_level = level
        dp = dist_to(pan, 160)
        h[:] = np.where(dp < 160, np.minimum(h, level + .5 + .035 * dp), h)
        h[pan] = level
        # Rivers and streams.
        self.lay_rivers()
        self.lay_fens()
        # The sea: level, and the land comes down to it; on the rain coast it may meet it in a bluff.
        dsea = dist_to(sea, 40)
        bluff = (field.fbm(SIZE, SIZE, 50, self.seed + 50, 2) > .55) & (self.Y < 2300) & (self.fens < .3)
        self.crag |= bluff & (dsea <= 4) & ~sea
        shore_cap = np.where(dsea < 40, .5 + dsea * np.where(bluff, .9, .14), 99)
        land = ~sea
        h[:] = np.where(land, np.maximum(.5, np.minimum(h, shore_cap)), 0)
        self.dsea = dsea
        # Lakes level, their shores above them (but not the water running in or out: a river's surface stays its own,
        # which lay_rivers keeps at or above the lake upstream of it and at or below it downstream).
        h[lake] = self.lake_level[lake]
        dl = dist_to(lake, 6)
        lv = grow_values(self.lake_level, lake, 6)
        rim = (dl > 0) & (dl <= 6) & ~sea & ~self.river & ~self.wash & ~self.creek & ~self.pool
        h[rim] = np.maximum(h[rim], lv[rim] + .5)
        self.t('water', t0)

    def lay_rivers(self):
        h = self.h
        lines = self.rivers()
        xs_c = self.fens_c
        river_d = np.full((SIZE, SIZE), 1e6, dtype=np.float32)
        river_s = np.zeros((SIZE, SIZE), dtype=np.float32)
        river_w = np.zeros((SIZE, SIZE), dtype=np.float32)
        river_dry = np.zeros((SIZE, SIZE), dtype=bool)
        river_v = np.zeros((SIZE, SIZE), dtype=np.float32)
        self.river_lines = []
        rng = random.Random(self.seed + 60)
        # Biggest first, so a tributary meets the river it joins at that river's own surface.
        lines.sort(key=lambda l: -max(l['flow']))
        fu = self.filled
        m_c = self.m_c
        for li, line in enumerate(lines):
            pts, flow = line['pts'], line['flow']
            # In the fens only the big rivers are kept as lines; their creeks are the fens' own (lay_fens).
            if sample(self.fens_c, *pts[0]) > .55 and not line['major']:
                continue
            # Not in the city's cells: their own ground drains as it always has.
            inside = [k for k, (x, y) in enumerate(pts) if x < CITY + 4 and y < CITY + 4]
            if inside:
                k0 = max(inside) + 1
                if k0 >= len(pts) - 3:
                    continue
                pts, flow = pts[k0:], flow[k0:]
            # Not through the city: a stream that reaches the city's built ground ends there.
            cut = None
            for k, (x, y) in enumerate(pts):
                if x < CITY + 8 and y < CITY + 8 and self.hold[min(SIZE - 1, int(y)), min(SIZE - 1, int(x))]:
                    cut = k
                    break
            if cut is not None:
                if cut < 4:
                    continue
                pts, flow = pts[:cut], flow[:cut]
            fine = densify(chaikin(pts, 3), 2.0)
            fl = np.interp(np.linspace(0, 1, len(fine)), np.linspace(0, 1, len(flow)), flow)
            # Meanders: a slow sideways wander, wider for bigger rivers and on flat ground.
            phase = rng.uniform(0, 100)
            out = []
            for k, (x, y) in enumerate(fine):
                a = fine[max(0, k - 3)]
                b = fine[min(len(fine) - 1, k + 3)]
                dx, dy = b[0] - a[0], b[1] - a[1]
                n = math.hypot(dx, dy) or 1
                flat = 1 - float(sample(self.mount_c, x, y) > 4)
                amp = (4 + 10 * min(1, fl[k] / 3000)) * (.4 + .6 * flat)
                off = amp * (math.sin(k / 23 + phase) * .7 + math.sin(k / 9.3 + phase * 1.7) * .3)
                ramp = min(1, k / 20, (len(fine) - 1 - k) / 20)
                out.append((x - dy / n * off * ramp, y + dx / n * off * ramp))
            fine = out
            width = np.clip(1.4 + 2.6 * np.sqrt(fl / 1000), 1.4, 16)
            surf = np.array([sample(fu, x, y) for x, y in fine], dtype=np.float32)
            surf = np.minimum.accumulate(np.maximum(surf, 0))
            end_x, end_y = fine[-1]
            ex, ey = min(SIZE - 1, int(end_x)), min(SIZE - 1, int(end_y))
            if self.sea[ey, ex]:
                end = 0.0
            elif river_d[ey, ex] < 3:
                end = float(river_s[ey, ex])
            else:
                end = float(surf[-1])
            L = len(surf)
            surf = np.minimum(surf, end + .012 * 2.0 * (L - 1 - np.arange(L)))
            surf = np.maximum(surf, end)
            # Through a lake the water is the lake's: never above a lake upstream, never below one downstream.
            lv = np.array([self.lake_level[min(SIZE - 1, int(y)), min(SIZE - 1, int(x))]
                           if self.lake[min(SIZE - 1, int(y)), min(SIZE - 1, int(x))] else np.nan for x, y in fine],
                          dtype=np.float32)
            if np.isfinite(lv).any():
                hi = np.minimum.accumulate(np.where(np.isfinite(lv), lv, np.inf))
                lo = np.maximum.accumulate(np.where(np.isfinite(lv), lv, -np.inf)[::-1])[::-1]
                surf = np.minimum.accumulate(np.minimum(np.maximum(surf, lo), hi))
            dry = float(np.mean([sample(m_c, x, y) for x, y in fine[::10]])) < .3
            valley = 26 if not dry else 12
            for k in range(0, L, 2):
                x, y = fine[k]
                w = width[k]
                R = int(w / 2 + valley)
                x0, x1 = max(0, int(x) - R), min(SIZE, int(x) + R + 1)
                y0, y1 = max(0, int(y) - R), min(SIZE, int(y) + R + 1)
                if x0 >= x1 or y0 >= y1:
                    continue
                d = np.hypot(self.X[y0:y1, x0:x1] + .5 - x, self.Y[y0:y1, x0:x1] + .5 - y) - w / 2
                view = river_d[y0:y1, x0:x1]
                better = d < view
                view[better] = d[better]
                river_s[y0:y1, x0:x1][better] = surf[k]
                river_w[y0:y1, x0:x1][better] = w
                river_dry[y0:y1, x0:x1][better] = dry
                river_v[y0:y1, x0:x1][better] = valley
            self.river_lines.append({'pts': fine[::8] + [fine[-1]], 'course': fine, 'flow': float(fl.max()), 'width': float(width.max()),
                                     'dry': dry, 'major': line['major'], 'from': fine[0], 'to': fine[-1],
                                     'top': float(surf[0]), 'mouth': float(surf[-1])})
        water = (river_d < 0) & ~self.sea & ~self.lake & ~self.keep
        wet = water & ~river_dry
        self.river = wet
        self.wash = water & river_dry
        self.river_d, self.river_s, self.river_w = river_d, river_s, river_w
        # The valleys: the land falls to each river's banks; gentler on the plains, steeper in the hills.
        near = (river_d < river_v) & ~self.sea & ~self.lake
        steep = .1 + .35 * self.mountain + .25 * (self.fens < .2) * smooth(.6, .9, up(self.m_c)) * (self.Y < 2400)
        target = river_s + .5 + steep * np.maximum(river_d, 0)
        h[near] = np.minimum(h[near], target[near])
        bank = near & (river_d >= 0) & (river_d < 3)
        h[bank] = np.maximum(h[bank], river_s[bank] + .5)
        h[wet] = river_s[wet]
        h[self.wash] = river_s[self.wash] + .5
        # Ravines: on the rain coast the streams cut steep-sided, ferny clefts.
        self.ravine = near & (river_d < 10) & (self.mountain < .5) & (up(self.m_c) > .7) & (self.Y < 2300) & (river_w < 5)

    def lay_fens(self):
        """The Sallow Fens: a web of creeks and pools on the low ground, the Long River's delta through it."""
        h = self.h
        fz = (self.fens_j > .5) & (h < 2.2) & ~self.sea & ~self.river & ~self.lake & (self.river_d > 2)
        s = self.seed
        c1 = np.abs(field.fbm(SIZE, SIZE, 150, s + 70, 3) - .5)
        c2 = np.abs(field.fbm(SIZE, SIZE, 70, s + 71, 3) - .5)
        creek = ((c1 < .010) | (c2 < .007)) & fz
        pools = (field.fbm(SIZE, SIZE, 26, s + 72, 3) > .64) & fz
        self.creek = creek & ~self.keep
        self.pool = pools & ~creek & ~self.keep
        h[fz] = np.minimum(h[fz], 1.0 + (h[fz] - 1.0) * .3)
        h[self.creek | self.pool] = np.maximum(0, h[self.creek | self.pool] - .5)

    # -- the city's wild ground meets the realm; no step a wolf can't take but where the land is meant to be sheer --
    def settle(self):
        t0 = time.time()
        h = self.h
        # In the city's cells: the city's own heights by everything kept, easing into the realm's over 40 tiles.
        d = dist_to(self.hold, 48)
        w = smooth(0, 44, d)
        n = CITY
        old = self.city_heights
        h[:n, :n] = np.where(self.hold[:n, :n], old, old * (1 - w[:n, :n]) + h[:n, :n] * w[:n, :n])
        far = up(field.distance_to_mask(down(self.hold.astype(np.float32), G, 'max') > 0, 60).astype(np.float32)) * G
        self.from_city = np.where(d < 48, d, np.maximum(d, far))
        # The land may not climb more than a walkable step a tile but in crags (mountain crests, mesa rims, the
        # badlands, the rain coast's bluffs and ravines); water and everything held keep their heights.
        fixed = self.hold | self.sea | self.lake | self.river | self.pan | self.creek | self.pool
        slope = np.where(self.crag | self.ravine, 3.0, .45).astype(np.float32)
        limit_slope(h, slope, fixed)
        self.h = half(np.clip(h, -16, 16))
        self.h[self.hold] = self.city_heights_full()[self.hold]
        self.t('settle', t0)

    def city_heights_full(self):
        out = np.zeros((SIZE, SIZE), dtype=np.float32)
        out[:CITY, :CITY] = self.city_heights
        return out

    # -- the ground: biomes from warmth and wetness, each its own mix ------------------------------------------------
    def lay_ground(self):
        t0 = time.time()
        s = self.seed
        h, X, Y = self.h, self.X, self.Y
        rng = np.random.default_rng(s + 80)
        grain = rng.random((SIZE, SIZE), dtype=np.float32)
        pick = rng.random((SIZE, SIZE), dtype=np.float32)
        self.mixr = rng.random((SIZE, SIZE), dtype=np.float32)
        detail = field.fbm(SIZE, SIZE, 13, s + 81, 2)
        clump = field.fbm(SIZE, SIZE, 22, s + 82, 3)
        self.grain, self.pick, self.detail, self.clump = grain, pick, detail, clump
        mild = np.exp(-up(field.distance_to_mask(self.sea_c, 200).astype(np.float32)) * G / 700)
        # The lie of the land: how steep, which way it faces (north faces are colder), crests (above the ground about
        # them) and hollows (below it).
        gy, gx = np.gradient(blur(h, 2))
        slope = np.hypot(gx, gy)
        north = np.clip(gy / (slope + .03), -1, 1) * smooth(.02, .1, slope)
        lap = blur(h, 2) - blur(h, 8)
        del gy, gx
        self.slope, self.lap = slope, lap
        temp = 14 - 10.5 * (1 - Y / SIZE) - .95 * np.maximum(h, 0) + 4 * mild + 3 * self.waste
        temp -= 3 * smooth(520, 0, Y) * smooth(900, 1300, X)       # (the Whitefangs go on north, colder, past the edge)
        temp += (field.fbm(SIZE, SIZE, 40, s + 83, 3) - .5) * 1.2 + (grain - .5) * .6
        # How cold a place is for its snow and its trees: a north face or a hollow holds the cold (and the snow), a crest
        # is swept bare by the wind.
        cold = temp - 2.8 * north + 2.4 * np.clip(lap, -1, 1)
        # Wetness drifts slowly over the land (the regions blend over hundreds of tiles, picked tile by tile at the
        # edges rather than in blotches).
        wet = up(self.m_c) + (field.fbm(SIZE, SIZE, 300, s + 84, 3) - .5) * .12 + \
            (field.fbm(SIZE, SIZE, 60, s + 91, 2) - .5) * .05 + (pick - .5) * .04
        # The desert's edge: the Ochre Hills are dry country, their own rain wrung out on the downs below them, so the
        # green thins to dry grass, scrub and sand across them whatever the wind brings.
        dry = self.dry_edge
        k = smooth(0, .25, dry)
        wet = wet * (1 - k) + np.minimum(wet, .46 - .24 * dry) * k
        xs_c, ys_c = grid_xy(NG, G)
        self.temp, self.wet, self.cold = temp, wet, cold
        codes = np.full((SIZE, SIZE), ord(','), dtype=np.uint8)
        land = ~(self.sea | self.lake | self.river | self.pan | self.creek | self.pool)
        # The biomes' edges are ragged, interleaving over tens of tiles: their thresholds read the wetness jittered.
        wj = wet + (field.fbm(SIZE, SIZE, 45, s + 94, 3) - .5) * .14 + (grain - .5) * .05
        B = {}
        hills = dry > .35                                           # (no snow, pine or heather on the dry hills)
        snow_v = cold + (clump - .5) * 2.2
        B['snow'] = land & (snow_v < -4.3) & ~hills
        B['alpine'] = land & ~B['snow'] & (cold < -4.2) & ~hills
        B['subalpine'] = land & (cold >= -4.2) & (cold < -1.5) & ~hills
        B['montane'] = land & (cold >= -1.5) & (cold < 3) & (wj >= .32) & ~hills
        warm = land & ~(B['snow'] | B['alpine'] | B['subalpine'] | B['montane'])
        # The middle country's woods and open ground: big woods and wide meadows, by a slow noise leaning to woods
        # where it is wetter; the rain coast all forest.
        woods = field.fbm(SIZE, SIZE, 260, s + 90, 3) + (wet - .56) * 1.4 + (field.fbm(SIZE, SIZE, 30, s + 89, 2) - .5) * .1
        # The rain coast (the city's country, the Rainwood, the sound) is rain forest down to the shore.
        coastal = up(smooth(1100, 500, field.distance_to_mask(self.sea_c, 160).astype(np.float32) * G) *
                     smooth(2500, 2100, ys_c) * smooth(1700, 1300, xs_c))
        rain = .7 - .16 * coastal
        B['rainforest'] = warm & (wj >= rain)
        B['woodland'] = warm & (wj >= .45) & (wj < rain) & (woods > .53)
        B['meadow'] = warm & (wj >= .38) & (wj < rain) & ~B['woodland']
        B['steppe'] = warm & (wj >= .26) & (wj < .38)
        B['scrub'] = warm & (wj >= .14) & (wj < .26)
        B['sand'] = warm & (wj < .14)
        # The rain coast's hills above the woods: heath and moor, on the Rainward Hills' tops (not round the city).
        heath = (B['woodland'] | B['rainforest'] | B['meadow']) & (h > 4.5) & (wet > .5) & \
            (field.fbm(SIZE, SIZE, 90, s + 85, 2) > .52) & (self.from_city > 300) & (dry < .05)
        B['heath'] = heath
        for k in ('woodland', 'rainforest', 'meadow'):
            B[k] &= ~heath
        fz = (self.fens_j > .5) & (h < 2.6) & land & (wet > .5)
        B['marsh'] = fz
        for k in list(B):
            if k != 'marsh':
                B[k] &= ~fz
        # Round the city: clear ground along its roads, by its gates and walls and the Houses' grounds (everything it
        # holds), the forest standing close beyond.
        near = smooth(30, 6, self.from_city)
        self.near_city = near
        self.biomes = B
        P = PALETTES
        for name, mask in B.items():
            if name in CALM:
                self.paint_calm(codes, mask, *CALM[name])
            else:
                self.paint_palette(codes, mask, P[name], thin=near)
        # The plains' copses and hedgerows; the downs' gorse.
        self.copses(codes, B['meadow'] & (near < .2))
        self.hedgerows(codes, B['meadow'] & (near < .2) & (field.fbm(SIZE, SIZE, 420, s + 92, 2) > .42))
        # The mountains: bare rock on the steep faces and the crests, rock standing out of the snow, scree at the foot
        # of the rock, glaciers in the high hollows; the pines snowy near the tree line.
        fine = field.fbm(SIZE, SIZE, 9, s + 93, 2)
        hi = land & (cold < -3.2) & ~hills
        steep = slope > (.13 + .1 * np.clip(north, 0, 1))
        rock = hi & (steep | (lap > .22))
        codes[rock] = np.where(pick[rock] < .62, ord('r'), np.where(pick[rock] < .9, ord('s'), ord('o')))
        out = B['snow'] & ((lap > .13 + .14 * (fine - .5)) | (slope > .105 + .07 * (fine - .5)))
        codes[out] = np.where(pick[out] < .7, ord('r'), ord('s'))
        apron = hi & ~rock & ~B['snow'] & (slope > .07) & (lap < -.05)
        codes[apron] = np.where(pick[apron] < .55, ord('s'), np.where(pick[apron] < .7, ord('o'), codes[apron]))
        ice = B['snow'] & (lap < -.12) & (slope < .12) & (snow_v < -8.5)
        codes[ice] = ord('J')
        snowy = (B['montane'] | B['subalpine']) & (codes == ord('P')) & (cold < 0) & (pick < .45 - .12 * cold)
        codes[snowy] = ord('1')
        parched = (B['subalpine'] | B['alpine']) & (wet < .32) & np.isin(codes, [ord('P'), ord('1')])   # (no pines in dry ground)
        codes[parched] = np.where(pick[parched] < .5, ord('B'), ord(';'))
        self.rock = rock
        # The Ochre Hills: red sandstone on their crests and steep faces, ochre scree and sand below, dry grass and
        # scrub between.
        bare = land & ~fz & (dry > .3) & ((slope > .09) | (lap > .16) | (fine * dry > .62))
        beds = (np.floor(h * 2 + (detail - .5) * 1.2).astype(np.int32) % 5)[bare]
        strata = np.array([ord(c) for c in '(((0('], dtype=np.uint8)
        scree = np.array([ord(c) for c in '(0X;('], dtype=np.uint8)
        steep_ = slope[bare] > .14
        codes[bare] = np.where(steep_ | (lap[bare] > .2), strata[beds], scree[(beds + (pick[bare] * 2).astype(int)) % 5])
        cact = bare & (grain < .006)
        codes[cact] = ord('?')
        self.ochre_rock = bare
        del fine
        # Overlays: the Red Waste's forms, the coast, the banks, the ravines.
        # The badlands: their terraces' risers banded red, yellow and grey by height, the flats between sandstone and
        # sand, the gullies sand.
        bad = land & (self.bad > .5)
        riser = bad & (slope > .11)
        bands = np.array([ord(c) for c in '((X(0(r'], dtype=np.uint8)
        codes[riser] = bands[np.floor(h[riser] * 2).astype(np.int32) % len(bands)]
        flat = bad & ~riser
        codes[flat] = np.where(pick[flat] < .5, ord('('), np.where(pick[flat] < .75, ord('0'), np.where(pick[flat] < .9, ord(';'), ord('X'))))
        gully = bad & (lap < -.2)
        codes[gully] = np.where(pick[gully] < .75, ord('0'), np.where(pick[gully] < .9, ord('X'), ord('B')))
        mt = land & (self.mesa > .3) & ((h > 7) | self.mesa_top)
        top = mt & self.mesa_top
        codes[top] = np.where(pick[top] < .55, ord('('), np.where(pick[top] < .85, ord(';'), np.where(pick[top] < .95, ord('0'), ord('?'))))
        flank = mt & ~self.mesa_top
        beds = np.floor(h[flank] * 2).astype(np.int32) % 4
        codes[flank] = np.array([ord(c) for c in '((0('], dtype=np.uint8)[beds]
        dz = land & (self.dunes > .5) & (wet < .3)
        self.mix(codes, dz, {'0': 12, ';': 1}, trees='?', density=.003)
        codes[self.pan] = ord(')')
        sx, sy = SALTPAN[:2]
        brine = self.pan & (ellipse(X, Y, sx + 40, sy + 20, 80, 46, SALTPAN[4], 2) + (detail - .5) * .5 + self.jit * .6 > .5)
        codes[brine] = ord('~')
        h[brine] = self.pan_level
        self.brine = brine
        # The coast: mossy rock, boulders and shingle on the rain coast, a little sand in the coves, mud flats and reeds
        # by the fens.
        dsea = self.dsea
        coast = land & (dsea <= 3 + 2 * (detail > .55))
        rocky = coast & (Y < 2400) & (detail > .3)
        codes[rocky] = np.where(pick[rocky] < .34, ord('r'), np.where(pick[rocky] < .6, ord('!'), np.where(
            pick[rocky] < .76, ord('o'), np.where(pick[rocky] < .88, ord('s'), ord('&')))))
        cove = coast & (Y < 2400) & (detail <= .3) & (dsea <= 3)
        codes[cove] = np.where(pick[cove] < .05, ord('7'), np.where(pick[cove] < .3, ord('s'), ord('0')))
        # In the rain forest the hollows are ferny: fern and moss under the trees, where the water runs after rain.
        fern = land & B['rainforest'] & (lap < -.18) & ~SOLID[codes]
        codes[fern] = np.where(pick[fern] < .55, ord('&'), np.where(pick[fern] < .85, ord('!'), codes[fern]))
        flats = land & (dsea <= 18) & (self.fens > .4)
        codes[flats] = np.where(pick[flats] < .55, ord('D'), np.where(pick[flats] < .8, ord('E'), ord('"')))
        # Dunes along the fens' sea shore, between the mud flats: sand and marram grass.
        dunes = land & (dsea <= 12 + 22 * field.fbm(SIZE, SIZE, 60, s + 88, 2)) & (dsea > 2) & (self.fens_j > .35) & \
            (self.Y > 2500 + 400 * field.fbm(SIZE, SIZE, 200, s + 89, 2)) & \
            (field.fbm(SIZE, SIZE, 120, s + 87, 2) > .5) & ~self.creek & ~self.pool
        codes[dunes] = np.where(pick[dunes] < .55, ord('0'), np.where(pick[dunes] < .9, ord('"'), ord('B')))
        self.shore_dunes = dunes
        # Ravines on the rain coast: ferns and moss along the streams.
        rav = land & self.ravine
        codes[rav] = np.where(pick[rav] < .38, ord('&'), np.where(pick[rav] < .6, ord('!'), np.where(pick[rav] < .7, ord('7'), codes[rav])))
        # River banks: reeds by slow water, alder and willow, tall grass; in the dry country a green strip.
        rd = self.river_d
        bank = land & (rd >= 0) & (rd < 4) & ~self.wash
        dryb = bank & (wet < .35)
        slow = bank & (self.mountain < .3) & (temp > 1)
        codes[slow] = np.where(pick[slow] < .22, ord('E'), np.where(pick[slow] < .3, ord('@'), np.where(pick[slow] < .42, ord('"'), codes[slow])))
        codes[dryb] = np.where(pick[dryb] < .2, ord('Y'), np.where(pick[dryb] < .55, ord('"'), ord(',')))
        cold = bank & (temp <= 1)
        codes[cold] = np.where(pick[cold] < .4, ord('o'), ord('r'))
        # Dry washes: sand and gravel.
        wb = self.wash
        codes[wb] = np.where(pick[wb] < .6, ord('0'), np.where(pick[wb] < .85, ord('X'), ord(';')))
        # Lake shores.
        dl = dist_to(self.lake, 4)
        ls = land & (dl > 0) & (dl <= 2)
        codes[ls & (temp > 2)] = np.where(pick[ls & (temp > 2)] < .5, ord('0'), ord('E'))
        codes[ls & (temp <= 2)] = np.where(pick[ls & (temp <= 2)] < .6, ord('r'), ord('o'))
        # Water.
        self.paint_water(codes)
        # The oasis.
        ox, oy = OASIS
        od = np.hypot(X - ox, Y - oy) * (1 + .35 * (field.fbm(SIZE, SIZE, 14, s + 86, 2) - .5))
        pool = od < 20
        ring = (od >= 20) & (od < 52) & land
        codes[ring] = np.where(pick[ring] < .16, ord('Y'), np.where(pick[ring] < .5, ord('"'), np.where(pick[ring] < .75, ord(','), ord('3'))))
        codes[pool] = np.where(od[pool] < 12, ord('W'), ord('~'))
        level = float(half(np.median(h[ring])))
        h[pool] = max(.5, level - .5)
        h[ring] = np.maximum(h[ring], level - .5 + .0)
        self.oasis = pool
        keep = self.keep
        codes[keep] = self.codes[keep]
        self.codes = codes
        self.t('ground', t0)

    def paint_palette(self, codes, mask, pal, thin=None):
        ground, trees, density, extras = pal
        if not mask.any():
            return
        self.mix(codes, mask, ground, trees=None)
        if trees:
            chance = density * (np.clip((self.clump - .32) * 2.6, 0, 2.2) + (.12 if trees.count('$') > 2 else 0))
            if thin is not None:                        # (the rain forest is never open, only thinner between groves)
                chance = chance * (1 - .85 * thin)
            grove = mask & (self.grain < chance)
            kinds = np.array([ord(t) for t in trees], dtype=np.uint8)
            codes[grove] = kinds[(self.pick[grove] * len(trees)).astype(int) % len(trees)]
        for code, share in extras:
            codes[mask & (self.pick < share) & (self.grain > .5)] = ord(code)
        if thin is not None:
            # Near the city the woods were cleared long ago: rough grass, bracken and moss.
            open_ = mask & (thin > .15) & (self.grain < thin * .8)
            codes[open_] = np.where(self.pick[open_] < .5, ord(','), np.where(self.pick[open_] < .72, ord('"'),
                                    np.where(self.pick[open_] < .88, ord('&'), ord('!'))))

    def paint_calm(self, codes, mask, a, b, scale, trees, density, extras):
        """Open ground (CALM): each tile picked from a mix drifting slowly from `a` to `b` (and to `b` in the hollows),
        so the ground changes over hundreds of tiles with no blotches; then scattered trees in clumps, and extras."""
        if not mask.any():
            return
        idx = np.nonzero(mask)
        slow = field.fbm(SIZE, SIZE, scale, self.seed + 95 + int(scale), 3)[idx]
        t = np.clip(smooth(.38, .62, slow) + .8 * smooth(-.08, -.35, self.lap[idx]), 0, 1)
        keys = sorted(set(a) | set(b))
        w = np.stack([(1 - t) * a.get(k, 0) + t * b.get(k, 0) for k in keys], axis=1)
        edges = np.cumsum(w / w.sum(axis=1, keepdims=True), axis=1)
        choice = (self.mixr[idx][:, None] > edges).sum(axis=1)
        codes[idx] = np.array([ord(k) for k in keys], dtype=np.uint8)[np.minimum(choice, len(keys) - 1)]
        if trees:
            chance = density * np.clip((self.clump - .32) * 2.6, 0, 2.2)
            grove = mask & (self.grain < chance)
            kinds = np.array([ord(c) for c in trees], dtype=np.uint8)
            codes[grove] = kinds[(self.pick[grove] * len(trees)).astype(int) % len(trees)]
        for code, share in extras:
            codes[mask & (self.pick < share) & (self.grain > .5)] = ord(code)

    def copses(self, codes, mask, spacing=96, chance=.5):
        """Copses in open country: a few in every square of `spacing` tiles, rounded, ragged, broadleaf with a shrubby
        edge and the odd pine; lone trees about them."""
        rng = np.random.default_rng(self.seed + 96)
        n = SIZE // spacing + 1
        for gy in range(n):
            for gx in range(n):
                if rng.random() >= chance:
                    continue
                x, y = (gx + rng.random()) * spacing, (gy + rng.random()) * spacing
                r = 4 + 17 * rng.random() ** 2
                R_ = int(r * 1.4) + 2
                x0, x1, y0, y1 = max(0, int(x) - R_), min(SIZE, int(x) + R_), max(0, int(y) - R_), min(SIZE, int(y) + R_)
                if x0 >= x1 or y0 >= y1 or not mask[int(min(y, SIZE - 1)), int(min(x, SIZE - 1))]:
                    continue
                win = np.s_[y0:y1, x0:x1]
                d = np.hypot(self.X[win] + .5 - x, self.Y[win] + .5 - y) / (r * (.7 + .6 * self.detail[win]))
                here = mask[win] & (d < 1)
                pick = self.pick[win]
                tree = here & (pick < .78 - .3 * d)
                cv = codes[win]
                cv[here & (d > .75) & (pick < .5)] = ord('B')
                cv[tree] = np.where(pick[tree] < .07, ord('P'), ord('Y'))
        lone = mask & (self.grain < .0025) & (self.pick > .5)
        codes[lone] = ord('Y')

    def hedgerows(self, codes, mask, spacing=150):
        """Hedgerows across open farmland: the edges between fields (the borders of a Voronoi of points jittered over a
        grid, on bent coordinates, so the fields are irregular), 2 tiles of shrub with a tree now and then and gaps to
        walk through."""
        rows, cols = np.nonzero(mask.any(axis=1))[0], np.nonzero(mask.any(axis=0))[0]
        if not len(rows):
            return
        y0, y1, x0, x1 = rows.min(), rows.max() + 1, cols.min(), cols.max() + 1
        win = np.s_[y0:y1, x0:x1]
        X, Y = self.X[win], self.Y[win]
        bx = X + (noise_at(X, Y, 110, self.seed + 97) - .5) * 70
        by = Y + (noise_at(X, Y, 110, self.seed + 98) - .5) * 70
        rng = np.random.default_rng(self.seed + 99)
        n = SIZE // spacing + 3
        jx, jy = rng.random((n, n)), rng.random((n, n))
        gx0, gy0 = np.floor(bx / spacing).astype(int), np.floor(by / spacing).astype(int)
        d1 = np.full(X.shape, 1e9, dtype=np.float32)
        d2 = np.full(X.shape, 1e9, dtype=np.float32)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                cx, cy = np.clip(gx0 + dx, 0, n - 1), np.clip(gy0 + dy, 0, n - 1)
                d = np.hypot(bx - (cx + jx[cy, cx]) * spacing, by - (cy + jy[cy, cx]) * spacing)
                d2 = np.minimum(d2, np.maximum(d1, d))
                d1 = np.minimum(d1, d)
        # Each field its own: a hay meadow (tall grass, flowers) or a grazed pasture (short grass), and shades between.
        hay = np.zeros(X.shape, dtype=np.float32)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                cx, cy = np.clip(gx0 + dx, 0, n - 1), np.clip(gy0 + dy, 0, n - 1)
                d = np.hypot(bx - (cx + jx[cy, cx]) * spacing, by - (cy + jy[cy, cx]) * spacing)
                hay = np.where(d <= d1 + 1e-3, ((cx * 7919 + cy * 104729) % 97) / 96.0, hay)
        del bx, by, gx0, gy0
        pick = self.pick[win]
        cv = codes[win]
        field_ = mask[win] & ~SOLID[cv] & (cv != ord('B'))
        r = self.mixr[win]
        tall = .06 + .55 * hay ** 2
        cv[field_] = np.where(r[field_] < tall[field_], ord('"'), np.where(r[field_] > .985 - .03 * hay[field_], ord('3'), ord(',')))
        edge = mask[win] & (d2 - d1 < 2.0) & (self.detail[win] > .36)
        cv[edge] = np.where(pick[edge] < .12, ord('Y'), ord('B'))

    def mix(self, codes, mask, ground, trees=None, density=0.0):
        keys = list(ground)
        weights = np.array(list(ground.values()), dtype=np.float32)
        edges = np.cumsum(weights / weights.sum())
        mixv = np.clip(self.detail * .84 + self.pick * .16, 0, .9999)
        choice = np.searchsorted(edges, mixv[mask])
        sub = np.array([ord(k) for k in keys], dtype=np.uint8)[np.clip(choice, 0, len(keys) - 1)]
        codes[mask] = sub
        if trees:
            t = mask & (self.grain < density)
            codes[t] = ord(trees)

    def paint_water(self, codes):
        sea, lake = self.sea, self.lake
        codes[sea] = ord('W')
        ds = dist_to(~sea, 5)
        codes[sea & (ds <= 3)] = ord('~')
        dl = dist_to(~lake, 5)
        codes[lake] = ord('W')
        codes[lake & (dl <= 3)] = ord('~')
        cold = lake & (self.h >= 7) & (dl <= 2)
        codes[cold] = ord('J')
        r = self.river
        deep = r & (self.river_w >= 4) & (self.river_d < -1.6)
        codes[r] = ord('~')
        codes[deep] = ord('W')
        codes[self.creek] = ord('~')
        codes[self.pool] = np.where(self.pick[self.pool] < .7, ord('w'), ord('~'))
        fz = (self.fens_j > .5) & ~sea & ~lake & ~r & ~self.creek & ~self.pool
        edge = fz & (dist_to(self.creek | self.pool, 2) <= 1)
        codes[edge] = np.where(self.pick[edge] < .5, ord('E'), np.where(self.pick[edge] < .7, ord('D'), codes[edge]))

    def mark_cliffs(self):
        """Wherever a step can't be walked, the higher tile is a cliff face (rock, unless something stands there)."""
        h, codes = self.h, self.codes
        hi = np.zeros(codes.shape, dtype=bool)
        dh = h[:, :-1] - h[:, 1:]
        hi[:, :-1] |= dh > .5
        hi[:, 1:] |= -dh > .5
        dv = h[:-1, :] - h[1:, :]
        hi[:-1, :] |= dv > .5
        hi[1:, :] |= -dv > .5
        water = np.isin(codes, [ord('W'), ord('~'), ord('w'), ord('J')])
        ramp = RAMP[codes]
        way = np.isin(codes, [ord(c) for c in terrain_catalog.WAYS])
        mark = hi & ~self.locked & ~water & ~ramp & ~way
        codes[mark] = ord('%')
        return int(mark.sum())


    # -- the reserved sites: the places the towns, farms and works will stand (doc 70); nothing is built on them ---------
    def water_mask(self):
        return self.sea | self.lake | self.river | self.creek | self.pool | self.pan_water()

    def pan_water(self):
        return getattr(self, 'brine', np.zeros((SIZE, SIZE), dtype=bool)) | getattr(self, 'oasis', np.zeros((SIZE, SIZE), dtype=bool))

    def lay_sites(self):
        t0 = time.time()
        h = self.h
        water = self.water_mask()
        self.water = water
        hc = down(h, G)
        wc = down(water.astype(np.float32), G)
        rc = down(self.river.astype(np.float32), G)
        big = down((self.river & (self.river_w >= 3)).astype(np.float32), G)      # (a mill on a river, not a brook)
        sc = down(self.sea.astype(np.float32), G)
        gy, gx = np.gradient(hc)
        slope = np.hypot(gx, gy) / G
        heldc = down(self.hold.astype(np.float32), G, 'max') > 0
        self.taken = np.zeros((NG, NG), dtype=bool)
        biome_c = {k: down(v.astype(np.float32), G) for k, v in self.biomes.items()}
        forest = biome_c['rainforest'] + biome_c['montane'] + biome_c['woodland']
        mount_c = down(self.mountain, G)
        xs, ys = grid_xy(NG, G)
        placed = {}
        for spec in SITES:
            key, kind, name, near, search, size, plan, about = spec
            tx, ty = (placed[near]['x'], placed[near]['y']) if isinstance(near, str) else near
            r = max(1, size // G)
            near_water = blur(wc, max(2, (size + 24) // G))
            rough = blur(slope, max(2, r // 2))
            score = rough * 40 + (blur(wc, r + 1) * 60 if kind not in ('mill', 'fishing', 'saltcoast') else 0)
            if kind in ('fishing', 'clay', 'salt'):
                score = score + np.where(near_water > .03, 0, 50)
            if kind == 'mill':
                score = score + np.where(blur(big, 2) > .02, 0, 80)
            if kind == 'saltcoast':
                score = score + np.where(blur(sc, 3) > .05, 0, 80)
            if kind == 'farmland':
                score = score + np.where(blur(rc, r // 2 + 4) > .004, 0, 40)
            if kind in ('mine', 'quarry'):
                score = score + np.where(mount_c > .3, 0, 8) + rough * -20
            if kind in ('timber', 'woodpasture', 'pigfarm', 'charcoal'):
                score = score + (1 - blur(forest, max(2, r // 2))) * 20
            if kind == 'marsh':
                score = score + (1 - blur(biome_c['marsh'], r // 2 + 1)) * 40
            if kind == 'town':
                shore = blur(wc, (size + 24) // G) > (.03 if key in SHORE_TOWNS else .012)
                score = score + np.where(shore, 0, 30 if key in SHORE_TOWNS else 4)
            d = np.hypot(xs - tx, ys - ty)
            score = score + d / max(search, 1) * 6
            wet = wc > (.5 if kind in ('fishing', 'clay', 'salt', 'mill', 'saltcoast', 'marsh') else .02)
            crowd = blur(self.taken.astype(np.float32), max(1, r // 2)) > .01 if kind != 'mill' else self.taken
            score[(d > search) | heldc | crowd | wet] = np.inf
            if kind == 'salt':
                score[down(self.pan.astype(np.float32), G) < .2] = np.inf
            j = int(np.argmin(score))
            if not np.isfinite(score.ravel()[j]):
                self.report(f'  site {key}: no ground for it near {tx},{ty}')
                continue
            cy, cx = divmod(j, NG)
            x, y = cx * G + G // 2, cy * G + G // 2
            if kind == 'fishing':
                x, y = self.landing(x, y, size)
            elif kind == 'mill':
                x, y = self.landing(x, y, size + 60, self.river & (self.river_w >= 3))
            elif kind == 'saltcoast':
                x, y = self.landing(x, y, size, self.sea)
            rr = r + 2
            self.taken[max(0, cy - rr):cy + rr + 1, max(0, cx - rr):cx + rr + 1] = True
            site = {'id': key, 'kind': kind, 'name': name, 'x': int(x), 'y': int(y), 'size': size, 'for': plan,
                    'about': about}
            self.prepare(site)
            self.sites.append(site)
            placed[key] = site
        self.t('sites', t0)

    def landing(self, x, y, size, water=None):
        """A fishing site (or a mill, or salterns) sits on the shore: the dry tile nearest the water near (x, y)."""
        R = size + 40
        x0, y0 = max(0, x - R), max(0, y - R)
        sub = (self.water if water is None else water)[y0:y + R, x0:x + R]
        dry = ~sub & ~self.hold[y0:y + R, x0:x + R]
        shore = dry & (dist_to(sub, 3) <= 2)
        ys, xs = np.nonzero(shore)
        if not len(xs):
            return x, y
        k = int(np.argmin(np.hypot(xs + x0 - x, ys + y0 - y)))
        return int(xs[k] + x0), int(ys[k] + y0)

    def prepare(self, site):
        """The ground made ready: a town's level, cleared; a field's or pasture's cleared and eased; a works' pad."""
        kind, x, y, size = site['kind'], site['x'], site['y'], site['size']
        R = int(size * 1.6) + 40
        x0, x1, y0, y1 = max(0, x - R), min(SIZE, x + R), max(0, y - R), min(SIZE, y + R)
        X, Y = self.X[y0:y1, x0:x1], self.Y[y0:y1, x0:x1]
        n = field.fbm(x1 - x0, y1 - y0, max(6, size / 3), self.seed + 900 + x + y, 2)
        if kind == 'farmland':                          # (a grain farm: about 2 x size tiles square, its corners rounded)
            shape = ((np.abs(X - x) / size) ** 6 + (np.abs(Y - y) / size) ** 6) ** (1 / 6) + (n - .5) * .12
        else:
            shape = np.hypot((X - x) / size, (Y - y) / (size * (.75 if kind in FIELDS else 1))) + (n - .5) * .35
        area = (shape < 1) & ~self.water[y0:y1, x0:x1] & ~self.hold[y0:y1, x0:x1]
        hv = self.h[y0:y1, x0:x1]
        cv = self.codes[y0:y1, x0:x1]
        if kind in UNCLEARED:
            if kind == 'woodpasture':                   # (pannage: the trees stay, the undergrowth is grazed off)
                under = (shape < 1) & np.isin(self.codes[y0:y1, x0:x1], [ord('&'), ord('B'), ord('7')]) & ~self.keep[y0:y1, x0:x1]
                self.codes[y0:y1, x0:x1][under] = ord(',')
            site['tiles'] = int(((shape < 1) & ~self.water[y0:y1, x0:x1] & ~self.hold[y0:y1, x0:x1]).sum())
            return
        if kind in LEVELLED:
            level = float(half(np.median(hv[area]))) if area.any() else float(hv[y - y0, x - x0])
            ease = 1 - smooth(1, 1 + max(8, size * .5) / size, shape)
            free = ~self.hold[y0:y1, x0:x1] & ~self.water[y0:y1, x0:x1]
            hv[free] = (hv * (1 - ease) + level * ease)[free]
            site['level'] = level
        else:
            # Fields, pastures, orchards, vineyards: the land eased (half its bumps), never levelled.
            mean = blur(hv, 10)
            ease = (1 - smooth(.7, 1.1, shape)) * .5
            free = ~self.hold[y0:y1, x0:x1] & ~self.water[y0:y1, x0:x1]
            hv[free] = (hv * (1 - ease) + mean * ease)[free]
        pick = self.pick[y0:y1, x0:x1]
        free = area & ~self.keep[y0:y1, x0:x1]
        if kind in LEVELLED and kind != 'town':
            # A works' pad: its own ground (rubble, rock, mud, salt, a landing's sand, a cutting's stumps and ferns).
            g = GROUND_OF[kind]
            keys = np.array([ord(c) for c in g[0]], dtype=np.uint8)
            idx = np.minimum((pick * len(keys)).astype(int), len(keys) - 1)
            cv[free] = keys[idx[free]]
            if g[1]:
                edge = free & (shape > .85) & (pick < g[1])
                cv[edge] = ord(g[2])
        else:
            # A town's ground, a field's, a range's: the trees, scrub and boulders cleared off, the country's own grass
            # left (dry grass in the dry country), thinning out over the edge rather than stopping at a line.
            wet = self.wet[y0:y1, x0:x1]
            keep_trees = .22 if kind == 'pasture' else .1 if kind in ('dairy', 'orchard') else 0
            clear = free & np.isin(cv, CLEARABLE) & (pick < (1 - smooth(.8, 1.12, shape) * .9) * (1 - keep_trees))
            local = np.where(wet < .34, ord(';'), np.where(pick < .7, ord(','), ord('"'))).astype(np.uint8)
            cv[clear] = local[clear]
            if kind == 'farmland':
                self.fields(site, np.s_[y0:y1, x0:x1], free & (shape < .97))
        site['tiles'] = int(area.sum())
        self.locked[y0:y1, x0:x1] |= area & (shape < .6) & (kind == 'town')

    def fields(self, site, win, inside, n=9):
        """A grain farm's ground as several fields round its middle (where the steading will stand): the borders of a
        Voronoi of a few points over it, drawn as hedgerows of shrub with a tree now and then and gaps to pass."""
        rng = np.random.default_rng(self.seed + 950 + site['x'])
        size = site['size']
        pts = [(site['x'] + (rng.random() * 2 - 1) * size * .8, site['y'] + (rng.random() * 2 - 1) * size * .8) for _ in range(n)]
        X, Y = self.X[win], self.Y[win]
        d1 = np.full(X.shape, 1e9, dtype=np.float32)
        d2 = np.full(X.shape, 1e9, dtype=np.float32)
        bend = (self.detail[win] - .5) * 14
        for px_, py_ in pts:
            d = np.hypot(X + bend - px_, Y - bend - py_)
            d2 = np.minimum(d2, np.maximum(d1, d))
            d1 = np.minimum(d1, d)
        pick = self.pick[win]
        hedge = inside & (d2 - d1 < 2.2) & (self.mixr[win] > .08)
        self.codes[win][hedge] = np.where(pick[hedge] < .1, ord('Y'), ord('B'))
        site['fields'] = n

    # -- roads ----------------------------------------------------------------------------------------------------
    def route_cost(self):
        s = G
        hc = down(self.h, s)
        gy, gx = np.gradient(hc)
        slope = np.hypot(gx, gy) / s
        wet = down((self.sea | self.lake | self.pan).astype(np.float32), s)
        river = down((self.river | self.creek | self.pool).astype(np.float32), s)
        marsh = down(self.biomes['marsh'].astype(np.float32), s)
        cost = 1 + 400 * slope ** 2 + 40 * river + 2.5 * marsh + .1 * np.maximum(0, hc - 6)
        cost[wet > .15] = np.inf
        cost[down(self.hold.astype(np.float32), s, 'max') > 0] = np.inf
        return cost

    def lay_roads(self):
        t0 = time.time()
        cost = self.route_cost()
        self.road_mask = np.zeros((SIZE, SIZE), dtype=bool)
        towns = {s['id']: s for s in self.sites if s['kind'] == 'town'}
        ends = {k: v for k, v in GATE_ROADS.items()}
        def point(key):
            if key in towns:
                return (towns[key]['x'], towns[key]['y']), towns[key].get('level')
            if key in ends:
                (x, y), _ = ends[key]
                return (x, y), float(self.h[y, x])
            return key, None
        for name, a, b, width in ROADS:
            (pa, la), (pb, lb) = point(a), point(b)
            if isinstance(a, str) and a in ends:
                la = float(self.city_heights[ends[a][1][1], ends[a][1][0]])
            pts = route(cost, pa, pb)
            if isinstance(a, str) and a in ends:
                pts = [ends[a][1]] + pts             # (from the city's road itself)
            self.road(name, pts, width, ends=(la, lb))
        # Tracks: from every other site to the nearest road.
        for site in self.sites:
            if site['kind'] == 'town':
                continue
            pts = route_to(cost, (site['x'], site['y']), down(self.road_mask.astype(np.float32), G, 'max') > 0)
            if pts and len(pts) > 1:
                ex, ey = int(pts[-1][0]), int(pts[-1][1])
                y0, x0 = max(0, ey - 12), max(0, ex - 12)
                ys, xs = np.nonzero(self.road_mask[y0:ey + 13, x0:ex + 13])
                if len(xs):
                    k = int(np.argmin(np.hypot(xs + x0 - ex, ys + y0 - ey)))
                    pts = pts + [(float(xs[k] + x0) + .5, float(ys[k] + y0) + .5)]
                self.road(f'track:{site["id"]}', pts, 2, ends=(site.get('level'), None), ch='/')
                site['track'] = len(pts)
        self.t('roads', t0)

    def road(self, name, points, width=3, ends=(None, None), ch='d'):
        """A road that climbs no faster than half a step in two tiles, the land eased into it (ridgemere2's road on the
        realm: held ground keeps its heights, kept tiles their codes, water is bridged)."""
        h = self.h
        samples = densify(points, .5)
        xy = np.array(samples, dtype=np.float32)
        ix = np.clip(xy[:, 0].astype(int), 0, SIZE - 1)
        iy = np.clip(xy[:, 1].astype(int), 0, SIZE - 1)
        ground = h[iy, ix].astype(np.float32)
        kernel = np.ones(81, dtype=np.float32) / 81
        profile = np.convolve(np.pad(ground, 40, mode='edge'), kernel, mode='valid')
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
        reach = int(width / 2 + 16)
        xs0, xs1 = max(0, int(xy[:, 0].min()) - reach - 1), min(SIZE, int(xy[:, 0].max()) + reach + 2)
        ys0, ys1 = max(0, int(xy[:, 1].min()) - reach - 1), min(SIZE, int(xy[:, 1].max()) + reach + 2)
        best = np.full((ys1 - ys0, xs1 - xs0), 1e9, dtype=np.float32)
        hts = np.zeros_like(best)
        for k in range(0, len(xy), 2):
            x0, y0 = max(xs0, int(xy[k, 0]) - reach), max(ys0, int(xy[k, 1]) - reach)
            x1, y1 = min(xs1, int(xy[k, 0]) + reach + 1), min(ys1, int(xy[k, 1]) + reach + 1)
            yy, xx = np.mgrid[y0:y1, x0:x1]
            d = (xx + .5 - xy[k, 0]) ** 2 + (yy + .5 - xy[k, 1]) ** 2
            b_ = best[y0 - ys0:y1 - ys0, x0 - xs0:x1 - xs0]
            bh = hts[y0 - ys0:y1 - ys0, x0 - xs0:x1 - xs0]
            better = d < b_
            b_[better] = d[better]
            bh[better] = profile[k]
        win = np.s_[ys0:ys1, xs0:xs1]
        dist = np.sqrt(best)
        near = dist < reach
        blend = 1 - field.smoothstep(width / 2, reach, dist)
        water = self.water[win]
        hold = self.hold[win]
        hv = h[win]
        upd = near & ~hold & ~water
        hv[upd] = half(hv * (1 - blend) + hts * blend)[upd]
        on = (dist <= width / 2) & ~self.locked[win]
        if ch == '/':
            on &= ~self.road_mask[win]                   # (a track joining a road keeps the road's tiles)
        cv = self.codes[win]
        if ch != '/':
            # A road's verges are kept clear: no tree, shrub or boulder within a few tiles of it.
            verge = (dist <= width / 2 + 4) & ~on & ~self.locked[win] & ~water & np.isin(cv, CLEARABLE)
            cv[verge] = np.where(self.wet[win][verge] < .34, ord(';'), np.where(self.pick[win][verge] < .75, ord(','), ord('"')))
        bridge = on & water
        cv[on & ~water] = ord(ch)
        cv[bridge] = ord('8')
        hv[bridge] = hts[bridge]
        hv[on & ~water] = hts[on & ~water]
        self.road_mask[win] |= on
        self.locked[win] |= on
        self.roads[name] = {'points': [tuple(map(float, p)) for p in points[::max(1, len(points) // 60)]] + [tuple(map(float, points[-1]))],
                            'length': int(sum(math.dist(a, b) for a, b in zip(points, points[1:]))),
                            'bridges': int(bridge.sum()), 'width': width, 'kind': 'track' if ch == '/' else 'road'}


    # -- the whole of it -------------------------------------------------------------------------------------------
    def build(self):
        t0 = time.time()
        self.lay_city()
        self.lay_sea()
        self.lay_elevation()
        self.lay_climate()
        self.lay_drainage()
        self.lay_heights()
        self.lay_water()
        self.settle()
        self.lay_ground()
        self.lay_sites()
        self.lay_roads()
        self.cliffs = self.mark_cliffs()
        # The city's own cells keep to the tiles the game knew before the realm (a server built before the new ones
        # still loads the city): willow as broadleaf, cactus as shrub, sandstone as rock, salt as sand.
        box = self.codes[:CITY, :CITY]
        for new, old in (('@', 'Y'), ('?', 'B'), ('(', 'r'), (')', '0')):
            box[box == ord(new)] = ord(old)
        self.name_waters()
        self.codes[self.keep] = self.city_codes_full()[self.keep]
        self.h[self.hold] = self.city_heights_full()[self.hold]
        self.report(f'realm2: laid out in {time.time() - t0:.0f} s')
        return self

    def city_codes_full(self):
        out = np.zeros((SIZE, SIZE), dtype=np.uint8)
        out[:CITY, :CITY] = self.city_codes
        return out

    def name_waters(self):
        """Names for the big rivers and lakes (by where they run), for the cells' descriptions and the doc."""
        lines = [l for l in self.river_lines if l['major'] and not l['dry'] and
                 not (self.fens_j[min(SIZE - 1, int(l['from'][1])), min(SIZE - 1, int(l['from'][0]))] > .5)]
        lines.sort(key=lambda l: -l['flow'])
        names = {}
        for l in lines:
            (fx, fy), (tx, ty) = l['from'], l['to']
            if 'long' not in names and ty > 2400 and self.sea[min(SIZE - 1, int(ty)), min(SIZE - 1, int(tx))]:
                names['long'] = l
                l['name'] = 'the Long River'
            elif math.dist((tx, ty), (930, 1290)) < 200 and fy < ty and 'rainrun' not in names and fx < 1300:
                names['rainrun'] = l
                l['name'] = 'the Rainrun'
            elif math.dist((tx, ty), (930, 1290)) < 260 and 'alder' not in names:
                names['alder'] = l
                l['name'] = 'the Alder'
        spare = iter(RIVER_NAMES)
        for l in lines:
            if 'name' not in l and l['flow'] > 5000 and len(l['pts']) * 16 >= 1200:
                l['name'] = next(spare, '')
        for l in self.river_lines:
            if l['dry'] and l['major']:
                l['name'] = 'the Dry Wash' if not any(x.get('name') == 'the Dry Wash' for x in self.river_lines) else 'a dry wash'
        for lake in self.lakes:
            lake['name'] = ''
        by_size = sorted(self.lakes, key=lambda k: -k['tiles'])
        for lake in by_size:
            for (lx, ly), nm in LAKE_NAMES.items():
                if not lake['name'] and math.dist((lx, ly), (lake['x'], lake['y'])) < 260 and nm not in [k['name'] for k in self.lakes]:
                    lake['name'] = nm
        for lake in by_size:
            if not lake['name']:
                lake['name'] = 'a tarn' if lake['level'] > 6 else 'a mere'

    # -- the cells ------------------------------------------------------------------------------------------------
    def region_of(self, cx, cy):
        x0, y0 = cx * CELL, cy * CELL
        win = np.s_[y0:y0 + CELL, x0:x0 + CELL]
        n = CELL * CELL
        share = {k: float(v[win].sum()) / n for k, v in self.biomes.items()}
        sea = float(self.sea[win].sum()) / n
        x, y = x0 + CELL / 2, y0 + CELL / 2
        desert = share['sand'] + share['scrub'] + float((self.bad[win] > .5).sum()) / n
        cold = share['snow'] + share['alpine'] + share['subalpine']
        ochre = float((self.ochre_rock[win] | (self.dry_edge[win] > .7)).sum()) / n
        if sea > .55:
            return 'sound' if 1150 < y < 1550 and x > 150 else 'sea'
        if self.pan[win].any():
            return 'saltpan'
        if share['marsh'] > .3:
            return 'fens'
        if ochre > .45 and y > 1500:
            return 'ochre'
        if desert > .45:
            return 'waste'
        if cold > .3 or (y < 600 and x > 1000):
            return 'greyteeth' if x > 3200 and y > 500 else 'whitefangs'
        if x < 1500 and y < 1100:
            return 'rainward'
        if 1150 < y < 1550 and x < 1000:
            return 'sound'
        if share['rainforest'] > .35 and x < 1700:
            return 'rainwood'
        if share['steppe'] > .3:
            return 'downs' if y < 3000 else 'sunfields'
        if y > 2800:
            return 'sunfields'
        return 'greenholt'

    def cells(self, city_cells):
        """Every cell: the city's nine (their wild ground made again, all else as it was) and the realm's new ones."""
        t0 = time.time()
        out = []
        by_id = {c['id']: c for c in city_cells}
        sites_in = defaultdict(list)
        for s in self.sites:
            sites_in[(s['x'] // CELL, s['y'] // CELL)].append(s)
        lakes_in = defaultdict(list)
        for lake in self.lakes:
            x0, y0, x1, y1 = lake['box']
            for cy in range(y0 // CELL, y1 // CELL + 1):
                for cx in range(x0 // CELL, x1 // CELL + 1):
                    lakes_in[(cx, cy)].append(lake['name'])
        rivers_in = defaultdict(set)
        for l in self.river_lines:
            if l.get('name') and l['major']:
                for x, y in l['pts']:
                    rivers_in[(int(x) // CELL, int(y) // CELL)].add(l['name'])
        self.regions = {}
        counts = Counter()
        for cy in range(N):
            for cx in range(N):
                x0, y0 = cx * CELL, cy * CELL
                cid = cell_id(cx, cy)
                codes = self.codes[y0:y0 + CELL, x0:x0 + CELL]
                if cid in by_id:
                    c = json.loads(json.dumps({k: v for k, v in by_id[cid].items() if k not in ('terrain', 'heights')}))
                    if (cx, cy) in CITY_RENAMES:
                        c['name'], c['description'] = CITY_RENAMES[(cx, cy)]
                    region = 'ridgemere'
                else:
                    if np.isin(codes, [ord('W'), ord('~')]).all():
                        continue                         # (open sea, nothing in it: left off the map)
                    region = self.region_of(cx, cy)
                    counts[region] += 1
                    c = {'id': cid, 'x': x0, 'y': y0, 'width': CELL, 'height': CELL, 'z': 0, 'outdoors': True,
                         'weather': WEATHER[region] if region != 'whitefangs' or y0 < 900 else 'snow',
                         'lighting': {'artificial': 0, 'daylightAccess': 1, 'tone': 'neutral'},
                         'territory': {'region': REGION_IDS[region], 'claims': [], 'chapter': ''}}
                    c['name'], c['description'] = self.describe(cx, cy, region, sites_in[(cx, cy)],
                                                                lakes_in[(cx, cy)], rivers_in[(cx, cy)])
                    c['weather'] = self.weather_of(cx, cy, region)
                self.regions[cid] = region
                rows = [bytes(row).decode('ascii') for row in codes]
                hh = half(self.h[y0:y0 + CELL, x0:x0 + CELL])
                defaults = DEFAULT_HEIGHT[codes]
                ys, xs = np.nonzero(hh != defaults)
                vals = hh[ys, xs]
                heights = {f'{x},{y}': (int(v) if v == int(v) else float(v)) for x, y, v in zip(xs.tolist(), ys.tolist(), vals.tolist())}
                c['terrain'], c['heights'] = rows, heights
                out.append(c)
        self.region_counts = counts
        self.t('cells', t0)
        return out

    def weather_of(self, cx, cy, region):
        x0, y0 = cx * CELL, cy * CELL
        win = np.s_[y0:y0 + CELL, x0:x0 + CELL]
        n = CELL * CELL
        snowy = float((self.biomes['snow'][win] | self.biomes['alpine'][win]).sum()) / n
        if region in ('whitefangs', 'greyteeth') and (snowy > .2 or float(np.median(self.temp[win])) < -3):
            return 'snow'
        # The desert's edge is dry country whatever its region: arid (no rain but its own rare showers, RatwWeather).
        if WEATHER[region] == 'clear' and float(np.mean(self.dry_edge[win])) > .5:
            return 'sandstorm'
        return WEATHER[region]

    def describe(self, cx, cy, region, sites, lakes, rivers):
        name, text = REGION_TEXT[region]
        towns = [s for s in sites if s['kind'] == 'town']
        quals = []
        if towns:
            quals.append(towns[0]['name'])
        named_lakes = [l for l in lakes if l and not l.startswith('a ')]
        if not quals and named_lakes:
            quals.append(named_lakes[0])
        if not quals:
            others = [s for s in sites if s['kind'] in ('mine', 'quarry', 'salt')]
            if others:
                quals.append(others[0]['name'][4:].capitalize() if others[0]['name'].startswith('the ') else others[0]['name'])
        if not quals:
            quals.append(compass(cx, cy, self.region_cells(region)))
        full = f'{name}, {quals[0]}' if quals[0] else name
        bits = [text]
        if towns:
            bits.append(TOWN_GROUND)
        for l in sorted(set(lakes))[:2]:
            if l and not l.startswith('a '):
                bits.append(f'{l} lies here.')
        rv = sorted(r for r in rivers if r)
        if rv:
            bits.append(f'{" and ".join(r[0].upper() + r[1:] for r in rv[:2])} {"runs" if len(rv) == 1 else "run"} through it.')
        return full[:80], ' '.join(bits)

    def region_cells(self, region):
        if not hasattr(self, '_rc'):
            self._rc = defaultdict(list)
            for cy in range(N):
                for cx in range(N):
                    if cx >= CITY // CELL or cy >= CITY // CELL:
                        self._rc[self.region_of(cx, cy)].append((cx, cy))
        return self._rc[region]

    # -- the checks -------------------------------------------------------------------------------------------
    def walk(self):
        """Components of the whole map by the game's step rule (cell seams crossed within 0.55)."""
        codes, h = self.codes, half(self.h)
        solid = SOLID[codes]
        ramp = RAMP[codes]
        def pair(a_h, b_h, a_s, b_s, a_r, b_r):
            rise = np.abs(a_h - b_h)
            return ~a_s & ~b_s & ((rise <= .5) | ((rise <= 1) & (a_r | b_r)))
        east = pair(h[:, :-1], h[:, 1:], solid[:, :-1], solid[:, 1:], ramp[:, :-1], ramp[:, 1:])
        south = pair(h[:-1, :], h[1:, :], solid[:-1, :], solid[1:, :], ramp[:-1, :], ramp[1:, :])
        for x in range(CELL - 1, SIZE - 1, CELL):
            east[:, x] = ~solid[:, x] & ~solid[:, x + 1] & (np.abs(h[:, x] - h[:, x + 1]) <= .55)
        for y in range(CELL - 1, SIZE - 1, CELL):
            south[y, :] = ~solid[y, :] & ~solid[y + 1, :] & (np.abs(h[y, :] - h[y + 1, :]) <= .55)
        return label_runs(~solid, east, south)

    def city_walk(self, codes, heights, project, cells):
        """The door ends in the city's cells its own ground (alone, as it was) leaves unreached from the spawn."""
        h = half(heights)
        solid = SOLID[codes]
        ramp = RAMP[codes]
        def pair(a_h, b_h, a_s, b_s, a_r, b_r):
            rise = np.abs(a_h - b_h)
            return ~a_s & ~b_s & ((rise <= .5) | ((rise <= 1) & (a_r | b_r)))
        east = pair(h[:, :-1], h[:, 1:], solid[:, :-1], solid[:, 1:], ramp[:, :-1], ramp[:, 1:])
        south = pair(h[:-1, :], h[1:, :], solid[:-1, :], solid[1:, :], ramp[:-1, :], ramp[1:, :])
        for x in range(CELL - 1, CITY - 1, CELL):
            east[:, x] = ~solid[:, x] & ~solid[:, x + 1] & (np.abs(h[:, x] - h[:, x + 1]) <= .55)
        for y in range(CELL - 1, CITY - 1, CELL):
            south[y, :] = ~solid[y, :] & ~solid[y + 1, :] & (np.abs(h[y, :] - h[y + 1, :]) <= .55)
        lab = label_runs(~solid, east, south)
        at = {c['id']: (c['x'], c['y']) for c in cells if is_city_cell(c)}
        sp = project['spawn']
        main = lab[at[sp['cell']][1] + sp['y'], at[sp['cell']][0] + sp['x']]
        reach = lab == main
        out = set()
        for link in project['links']:
            for end in ('a', 'b'):
                e = link[end]
                if e['cell'] in at:
                    x, y = at[e['cell']][0] + e['x'], at[e['cell']][1] + e['y']
                    if not any(0 <= y + dy < CITY and 0 <= x + dx < CITY and reach[y + dy, x + dx]
                               for dx, dy in ((0, 0), (0, -1), (1, 0), (0, 1), (-1, 0))):
                        out.add(f'{link["id"]}.{end}')
        return out

    def checks(self, project, cells, pristine_codes, pristine_heights):
        t0 = time.time()
        out = []
        def check(name, ok, detail=''):
            out.append({'check': name, 'ok': bool(ok), 'detail': detail})
        lab = self.walk()
        sp = project['spawn']
        sc = next(c for c in cells if c['id'] == sp['cell'])
        main = int(lab[sc['y'] + sp['y'], sc['x'] + sp['x']])
        check('the spawn stands on open ground', main >= 0)
        reach = lab == main
        self.reach = reach
        # The city: every kept tile exactly as it was.
        kc = self.codes[:CITY, :CITY][self.keep[:CITY, :CITY]]
        kh = half(self.h[:CITY, :CITY])[self.keep[:CITY, :CITY]]
        pc = pristine_codes[self.keep[:CITY, :CITY]]
        ph = half(pristine_heights)[self.keep[:CITY, :CITY]]
        check('every kept tile of the city as it was (code and height)', np.array_equal(kc, pc) and np.array_equal(kh, ph),
              f'{int((kc != pc).sum())} codes, {int((kh != ph).sum())} heights differ')
        # Every door, stair and passage end in a cell still on open ground and reached from the spawn.
        at = {c['id']: (c['x'], c['y']) for c in cells}
        bad = []
        for link in project['links']:
            for end in ('a', 'b'):
                e = link[end]
                if e['cell'] in at:
                    x, y = at[e['cell']][0] + e['x'], at[e['cell']][1] + e['y']
                    ok = any(0 <= y + dy < SIZE and 0 <= x + dx < SIZE and reach[y + dy, x + dx]
                             for dx, dy in ((0, 0), (0, -1), (1, 0), (0, 1), (-1, 0)))
                    if not ok:
                        bad.append(f'{link["id"]}.{end}')
        # (Some of the Sump's back doors open on courts reached only through a building: the same before as after.)
        before = self.city_walk(pristine_codes, pristine_heights, project, cells)
        new = sorted(set(bad) - before)
        check('every door the city\'s own ground reached is still reached from the spawn', not new,
              ', '.join(new[:10]) + f' ({len(bad)} reached only through buildings, as before)')
        # Every outdoor spot of an NPC and every patrol post.
        spots = []
        for x in project['people']:
            for k in ('home', 'work', 'evening'):
                s = x.get(k)
                if s and s['cell'] in at:
                    spots.append((at[s['cell']][0] + s['x'], at[s['cell']][1] + s['y'], x['id']))
        for r in project['routes']:
            for s in r['posts']:
                if s['cell'] in at:
                    spots.append((at[s['cell']][0] + s['x'], at[s['cell']][1] + s['y'], r['id']))
        lost = [w for x, y, w in spots if not any(reach[y + dy, x + dx] for dx, dy in ((0, 0), (0, -1), (1, 0), (0, 1), (-1, 0))
                                                  if 0 <= y + dy < SIZE and 0 <= x + dx < SIZE)]
        check('every NPC\'s outdoor spot and patrol post still reached', not lost, ', '.join(lost[:10]))
        # The gates' roads lead out into the realm; every town and site is reached.
        for key, (start, last) in GATE_ROADS.items():
            check(f'the {key} gate road goes on into the realm', reach[start[1], start[0]] and reach[last[1], last[0]])
        missing = [s['id'] for s in self.sites if not reach[s['y'], s['x']]]
        check('every reserved site is reached from the city', not missing, ', '.join(missing))
        towns = [s for s in self.sites if s['kind'] == 'town']
        check('5-7 town sites', 5 <= len(towns) <= 7, str(len(towns)))
        # Every region walked into: each new cell with open land has most of it reached.
        poor = []
        for c in cells:
            x0, y0 = c['x'], c['y']
            land = ~SOLID[self.codes[y0:y0 + CELL, x0:x0 + CELL]]
            if land.sum() > 2000 and reach[y0:y0 + CELL, x0:x0 + CELL].sum() < .7 * land.sum():
                poor.append(f'{c["id"]} {int(reach[y0:y0 + CELL, x0:x0 + CELL].sum() * 100 / land.sum())}%')
        check('in every cell, most open ground is reached from the city', len(poor) <= 3, ', '.join(poor[:12]))
        self.reached_share = float(reach.sum()) / float((~SOLID[self.codes]).sum())
        # Water runs downhill: along every river, tile by tile from its source to its mouth and through the lakes on its
        # way, the water's surface never rises (a bridge's deck is the road's, not the water's).
        water = (self.river | self.lake | self.sea | self.creek | self.pool) & ~self.keep & (self.codes != ord('8'))
        uphill, worst, where = 0, 0.0, []
        for l in self.river_lines:
            if l['dry']:
                continue
            last, prev = None, None
            for x, y in l['course']:
                xi, yi = min(SIZE - 1, int(x)), min(SIZE - 1, int(y))
                if (xi, yi) == prev or not water[yi, xi]:
                    continue
                prev = (xi, yi)
                v = float(self.h[yi, xi])
                if last is not None and v > last:
                    uphill += 1
                    worst = max(worst, v - last)
                    where.append(f'{xi},{yi}')
                last = v if last is None else min(last, v)
                if self.sea[yi, xi]:
                    break                                   # (its mouth)
        check('no river runs uphill', uphill == 0, f'{uphill} rises (the worst {worst:g}): {", ".join(where[:12])}')
        check('heights within the game\'s +-16', float(self.h.max()) <= 16 and float(self.h.min()) >= -16)
        check('no word "dog" in the realm\'s text', not any(re.search(r'\bdogs?\b', (c['name'] + ' ' + c['description']).lower())
                                                           for c in cells))
        check('the cells tile the map without overlap', len({(c['x'], c['y']) for c in cells}) == len(cells))
        self.t('checks', t0)
        return out


def ridge(h, a, b, px, py):
    """A ridge from peak a to peak b (x, y, height, spread): a crest dipping to a saddle between them, falling away
    steeply to its foothills."""
    (ax, ay, ah, aw), (bx, by, bh, bw) = a, b
    w = (aw + bw) / 2
    d, t = seg_dist(px, py, (ax, ay), (bx, by))
    long = math.dist((ax, ay), (bx, by)) > 5
    crest = (ah + (bh - ah) * t) * (1 - .32 * np.sin(np.pi * t) * long)
    core = crest * np.exp(-(d / w) ** 1.5)
    foot = .4 * crest * np.exp(-(d / (2.6 * w)) ** 2)
    np.maximum(h, np.maximum(core, foot), out=h)


def octagonal_distance(mask, limit):
    """Steps from each node to the nearest True one (4-way and 8-way steps in turn, so nearly round), capped."""
    dist = np.where(mask, 0, limit).astype(np.float32)
    current = mask.copy()
    for d in range(1, limit):
        grown = current.copy()
        grown[1:, :] |= current[:-1, :]
        grown[:-1, :] |= current[1:, :]
        grown[:, 1:] |= current[:, :-1]
        grown[:, :-1] |= current[:, 1:]
        if d % 2 == 0:
            grown[1:, 1:] |= current[:-1, :-1]
            grown[1:, :-1] |= current[:-1, 1:]
            grown[:-1, 1:] |= current[1:, :-1]
            grown[:-1, :-1] |= current[1:, 1:]
        fresh = grown & ~current
        if not fresh.any():
            break
        dist[fresh] = d
        current = grown
    return dist


def carve_range(foot, plan, sea, px, py, seed):
    """The ranges as water would have worn them (the coarse grid): the plan's mass (`plan`, the crests' heights) is
    drained (priority flood, steepest descent) and wherever enough ground drains through a node a valley runs; the
    land rises from each valley floor to the divides between them, so the crests are the sharp lines halfway between
    valleys, branching as the valleys do, and highest where the valleys are far apart and the plan is high."""
    core = smooth(.6, 9, plan)                                   # how much of a range it is (0-1)
    crests = ridges_at(px * 1.05 + py * .18, py * .9 - px * .12, 260, seed + 15, 4)
    first = foot + plan * (.55 + .6 * crests)
    seeds = sea | (core < .02)
    seeds[0, :] = seeds[-1, :] = seeds[:, 0] = seeds[:, -1] = True
    filled, _ = priority_flood(first, seeds)
    down = steepest(filled, seeds).ravel()
    area = np.ones(first.size, dtype=np.float64)
    for i in np.argsort(-filled, axis=None):
        if down[i] >= 0:
            area[down[i]] += area[i]
    area = area.reshape(first.shape)
    valleys = (area >= 7) & (core > .02)
    apart = blur(octagonal_distance(valleys, 24), 1)
    # The valley floors climb into the range (lowest where the most water has gathered); the walls rise from them.
    floor = foot + core * np.clip(plan * .4 - .55 * np.log1p(area / 7), 0, None)
    walls = plan * (.4 + .5 * crests) * (1 - np.exp(-apart / 2.4))
    return floor + core * walls


STEPS8 = [(-1, 0, 1), (1, 0, 1), (0, -1, 1), (0, 1, 1), (-1, -1, 1.41), (-1, 1, 1.41), (1, -1, 1.41), (1, 1, 1.41)]


def route(cost, a, b):
    """The cheapest way from a to b over the coarse grid (A*), as tile points, its corners rounded."""
    ny, nx = cost.shape
    start = (min(ny - 1, int(a[1]) // G), min(nx - 1, int(a[0]) // G))
    goal = (min(ny - 1, int(b[1]) // G), min(nx - 1, int(b[0]) // G))

    def estimate(node):
        dy, dx = abs(node[0] - goal[0]), abs(node[1] - goal[1])
        return max(dx, dy) + .41 * min(dx, dy)
    open_ = [(estimate(start), 0.0, start)]
    came, spent = {start: None}, {start: 0.0}
    while open_:
        _, g, node = heapq.heappop(open_)
        if node == goal:
            break
        if g > spent[node]:
            continue
        here_cost = cost[node] if np.isfinite(cost[node]) else 1.0
        for dy, dx, length in STEPS8:
            nxt = (node[0] + dy, node[1] + dx)
            if not (0 <= nxt[0] < ny and 0 <= nxt[1] < nx):
                continue
            c = cost[nxt] if nxt != goal else 1.0
            if not np.isfinite(c):
                continue
            step = length * (c + here_cost) / 2
            if g + step < spent.get(nxt, np.inf):
                spent[nxt] = g + step
                came[nxt] = node
                heapq.heappush(open_, (g + step + estimate(nxt), g + step, nxt))
    if goal not in came:
        raise RuntimeError(f'no way from {a} to {b}')
    return _path(came, goal, a, b)


def route_to(cost, a, targets):
    """The cheapest way from a to the nearest target node (Dijkstra), as tile points; None if there is none."""
    ny, nx = cost.shape
    start = (min(ny - 1, int(a[1]) // G), min(nx - 1, int(a[0]) // G))
    open_ = [(0.0, start)]
    came, spent = {start: None}, {start: 0.0}
    goal = None
    while open_:
        g, node = heapq.heappop(open_)
        if g > spent[node]:
            continue
        if targets[node] and node != start:
            goal = node
            break
        if g > 3000:
            break
        here_cost = cost[node] if np.isfinite(cost[node]) else 1.0
        for dy, dx, length in STEPS8:
            nxt = (node[0] + dy, node[1] + dx)
            if not (0 <= nxt[0] < ny and 0 <= nxt[1] < nx):
                continue
            c = cost[nxt] if not targets[nxt] else 1.0
            if not np.isfinite(c):
                continue
            step = length * (c + here_cost) / 2
            if g + step < spent.get(nxt, np.inf):
                spent[nxt] = g + step
                came[nxt] = node
                heapq.heappush(open_, (g + step, nxt))
    if goal is None:
        return None
    return _path(came, goal, a, (goal[1] * G + G / 2, goal[0] * G + G / 2))


def _path(came, goal, a, b):
    path, node = [], goal
    while node is not None:
        path.append((node[1] * G + G / 2, node[0] * G + G / 2))
        node = came[node]
    path.reverse()
    path[0], path[-1] = tuple(a), tuple(b)
    if len(path) == 1:
        path.append(tuple(b))
    return chaikin(path, 3)


def keep_joined(mask, seeds):
    """The parts of `mask` joined (4-way) to any seed tile."""
    lab = components4(mask)
    keep_labels = {int(lab[y, x]) for x, y in seeds if mask[y, x]}
    return np.isin(lab, list(keep_labels)) & mask


def components4(mask):
    """Labels of the 4-connected parts of a mask (numpy only: run-lengths along rows, joined by union-find)."""
    return label_runs(mask, mask[:, :-1] & mask[:, 1:], mask[:-1, :] & mask[1:, :])


def label_runs(open_, east, south):
    """Component labels (-1 for closed tiles) where `east[y, x]` joins (x, y) to (x+1, y) and `south[y, x]` joins (x, y)
    to (x, y+1). Runs of tiles joined along a row are nodes; south joins are unioned with array-wide pointer jumping."""
    H, W = open_.shape
    # A run starts at an open tile not joined to its west neighbour.
    joined_w = np.zeros((H, W), dtype=bool)
    joined_w[:, 1:] = east
    start = open_ & ~joined_w
    run_id = np.cumsum(start.ravel()).reshape(H, W) - 1
    run_id = np.where(open_, run_id, -1)
    nruns = int(start.sum())
    parent = np.arange(nruns, dtype=np.int64)
    a = run_id[:-1, :][south]
    b = run_id[1:, :][south]
    pair = np.unique(np.stack([np.minimum(a, b), np.maximum(a, b)], 1), axis=0) if len(a) else np.zeros((0, 2), np.int64)
    a, b = pair[:, 0], pair[:, 1]
    while len(a):
        ra, rb = parent[a], parent[b]
        while True:                                     # compress
            ra2, rb2 = parent[ra], parent[rb]
            if np.array_equal(ra2, ra) and np.array_equal(rb2, rb):
                break
            ra, rb = ra2, rb2
        diff = ra != rb
        if not diff.any():
            break
        lo, hi = np.minimum(ra[diff], rb[diff]), np.maximum(ra[diff], rb[diff])
        np.minimum.at(parent, hi, lo)
        # Full compression of the forest.
        while True:
            p2 = parent[parent]
            if np.array_equal(p2, parent):
                break
            parent = p2
        a, b = a[diff], b[diff]
    while True:
        p2 = parent[parent]
        if np.array_equal(p2, parent):
            break
        parent = p2
    return np.where(run_id >= 0, parent[np.maximum(run_id, 0)], -1)


def sample(a, x, y):
    """A coarse field's value at tile (x, y), bilinear."""
    fx, fy = x / G - .5, y / G - .5
    n0, n1 = a.shape
    x0, y0 = int(min(max(math.floor(fx), 0), n1 - 1)), int(min(max(math.floor(fy), 0), n0 - 1))
    x1, y1 = min(x0 + 1, n1 - 1), min(y0 + 1, n0 - 1)
    tx, ty = min(max(fx - x0, 0), 1), min(max(fy - y0, 0), 1)
    top = a[y0, x0] * (1 - tx) + a[y0, x1] * tx
    bot = a[y1, x0] * (1 - tx) + a[y1, x1] * tx
    return float(top * (1 - ty) + bot * ty)


def grow_mask(mask, r):
    return dist_to(mask, r + 1) <= r


def grow_values(values, mask, r):
    """Each tile within r of the mask takes the highest value of the mask near it."""
    v = np.where(mask, values, -1e9).astype(np.float32)
    for _ in range(r):
        w = v.copy()
        w[1:, :] = np.maximum(w[1:, :], v[:-1, :])
        w[:-1, :] = np.maximum(w[:-1, :], v[1:, :])
        w[:, 1:] = np.maximum(w[:, 1:], v[:, :-1])
        w[:, :-1] = np.maximum(w[:, :-1], v[:, 1:])
        v = w
    return v


def drop_small_full(mask, n):
    return drop_small(mask, n)


def limit_slope(h, slope, fixed, rounds=3):
    """No tile more than `slope` above or below a neighbour (by row and column sweeps), the fixed ones unchanged."""
    free = ~fixed
    H, W = h.shape
    for _ in range(rounds):
        for low in (True, False):
            for axis in (1, 0):
                for rev in (False, True):
                    n = W if axis == 1 else H
                    order = range(n - 2, -1, -1) if rev else range(1, n)
                    step = 1 if rev else -1
                    for i in order:
                        if axis == 1:
                            cur, prev, s_, f_ = h[:, i], h[:, i + step], slope[:, i], free[:, i]
                        else:
                            cur, prev, s_, f_ = h[i], h[i + step], slope[i], free[i]
                        cand = np.minimum(cur, prev + s_) if low else np.maximum(cur, prev - s_)
                        np.copyto(cur, cand, where=f_)


def priority_flood(e, seeds, eps=1e-3):
    n0, n1 = e.shape
    filled = e.astype(np.float64).copy()
    done = seeds.copy()
    heap = [(float(e[y, x]), int(y), int(x)) for y, x in zip(*np.nonzero(seeds))]
    heapq.heapify(heap)
    parent = np.full(e.shape, -1, dtype=np.int64)
    steps = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)]
    while heap:
        h, y, x = heapq.heappop(heap)
        for dy, dx in steps:
            ny, nx = y + dy, x + dx
            if 0 <= ny < n0 and 0 <= nx < n1 and not done[ny, nx]:
                done[ny, nx] = True
                v = max(float(e[ny, nx]), h + eps)
                filled[ny, nx] = v
                parent[ny, nx] = y * n1 + x
                heapq.heappush(heap, (v, ny, nx))
    return filled.astype(np.float32), parent


def steepest(filled, outlets):
    n0, n1 = filled.shape
    pad = np.pad(filled.astype(np.float64), 1, constant_values=np.inf)
    best = np.zeros(filled.shape, dtype=np.float64)
    pick = np.full(filled.shape, -1, dtype=np.int64)
    ys, xs = np.mgrid[0:n0, 0:n1]
    for dy, dx in [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)]:
        nb = pad[1 + dy:1 + dy + n0, 1 + dx:1 + dx + n1]
        drop = (filled - nb) / math.hypot(dx, dy)
        better = drop > best
        best[better] = drop[better]
        pick[better] = ((ys + dy) * n1 + (xs + dx))[better]
    pick[outlets] = -1
    return pick


def drop_small(mask, n):
    lab = components4(mask)
    if not mask.any():
        return mask
    ids, counts = np.unique(lab[mask], return_counts=True)
    big = ids[counts >= n]
    return np.isin(lab, big) & mask


# ---------------------------------------------------------------------------------------------------------------
# Pictures
# ---------------------------------------------------------------------------------------------------------------
_FG = np.zeros((128, 3), dtype=np.float32)
_BG = np.zeros((128, 3), dtype=np.float32)
for _t in terrain_catalog.TILES:
    _FG[ord(_t['code'])] = [int(_t['fg'][i:i + 2], 16) for i in (1, 3, 5)]
    _BG[ord(_t['code'])] = [int(_t['bg'][i:i + 2], 16) for i in (1, 3, 5)]
# Trees and shrubs drawn a shade darker than the ground under them (their shadow), so woods, copses and hedges read on
# the map as the player's glyphs read in the game.
_CANOPY = np.ones(128, dtype=np.float32)
for _c, _v in (('Y', .72), ('P', .78), ('$', .8), ('1', .85), ('@', .74), ('2', .8), ('B', .84), ('?', .86), ('7', .86)):
    _CANOPY[ord(_c)] = _v
FONT = '/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf'


def picture(codes, heights, box=None, scale=1.0, relief=1.0):
    """The land as a map: each tile its catalog colours mixed, lit from the north-west by its height, shrunk (by
    averaging, water kept visible) or grown (by repeating) to `scale` pixels a tile."""
    from PIL import Image
    if box:
        x0, y0, w, h = box
        codes, heights = codes[y0:y0 + h, x0:x0 + w], heights[y0:y0 + h, x0:x0 + w]
    rgb = (_BG[codes] * .42 + _FG[codes] * .58) * _CANOPY[codes][..., None]
    f = int(round(1 / scale)) if scale < 1 else 1
    H, W = codes.shape
    hh = heights.astype(np.float32)
    if f > 1:
        H, W = H // f * f, W // f * f
        water = np.isin(codes[:H, :W], [ord('W'), ord('~'), ord('w')]).reshape(H // f, f, W // f, f).mean(axis=(1, 3))
        wcol = (_BG[codes[:H, :W]] * .42 + _FG[codes[:H, :W]] * .58).reshape(H // f, f, W // f, f, 3)
        wm = np.isin(codes[:H, :W], [ord('W'), ord('~'), ord('w')]).reshape(H // f, f, W // f, f, 1)
        wavg = (wcol * wm).sum(axis=(1, 3)) / np.maximum(wm.sum(axis=(1, 3)), 1)
        rgb = rgb[:H, :W].reshape(H // f, f, W // f, f, 3).mean(axis=(1, 3))
        k = np.clip(water * 2.2, 0, 1)[..., None]
        rgb = rgb * (1 - k) + wavg * k
        hh = hh[:H, :W].reshape(H // f, f, W // f, f).mean(axis=(1, 3))
    hs = blur(hh, 1)
    gy, gx = np.gradient(hs)
    shade = np.clip(1 + (-gx - gy) * .5 * relief, .35, 1.7)[..., None]
    tint = np.clip((hh[..., None] + 4) / 24, 0, 1)
    rgb = np.clip(rgb * shade * (.8 + .4 * tint), 0, 255)
    img = Image.fromarray(rgb.astype(np.uint8), 'RGB')
    if scale > 1:
        img = img.resize((int(img.width * scale), int(img.height * scale)), Image.NEAREST)
    return img


# ---------------------------------------------------------------------------------------------------------------
# The city's cells as they were before the realm, the expansion, the write
# ---------------------------------------------------------------------------------------------------------------
SNAPSHOT = ART / 'city-cells.json.gz'


def city_canvas(cells):
    codes = np.zeros((CITY, CITY), dtype=np.uint8)
    heights = np.zeros((CITY, CITY), dtype=np.float32)
    for c in cells:
        x0, y0 = c['x'], c['y']
        for y, row in enumerate(c['terrain']):
            codes[y0 + y, x0:x0 + len(row)] = np.frombuffer(row.encode('ascii'), dtype=np.uint8)
        heights[y0:y0 + c['height'], x0:x0 + c['width']] = DEFAULT_HEIGHT[codes[y0:y0 + c['height'], x0:x0 + c['width']]]
        for key, v in c['heights'].items():
            x, y = map(int, key.split(','))
            heights[y0 + y, x0 + x] = v
    return codes, heights


def is_city_cell(c):
    return c['x'] < CITY and c['y'] < CITY


def pristine_city(project):
    """The city's nine cells as the city writer made them: the world's own while it has no realm round it, else the
    snapshot kept when the realm was first laid (checked against what the world holds now)."""
    cells = [c for c in project['cells'] if is_city_cell(c)]
    if len(project['cells']) == len(cells):
        return cells, False
    if not SNAPSHOT.exists():
        raise SystemExit(f'The world has the realm already but {SNAPSHOT} is missing: write the city again '
                         '(python3 -m worldgen.ridgemere2 write), which lays the realm round it.')
    ground = {c['id']: c for c in json.loads(gzip.decompress(SNAPSHOT.read_bytes()).decode('utf-8'))}
    # The cells as the world has them (name, weather, claims ...), with the ground they had before the realm.
    return [{**c, 'terrain': ground[c['id']]['terrain'], 'heights': ground[c['id']]['heights']} for c in cells], True


def keep_snapshot(cells):
    ART.mkdir(parents=True, exist_ok=True)
    data = json.dumps([{k: c[k] for k in ('id', 'x', 'y', 'width', 'height', 'terrain', 'heights')} for c in cells],
                      separators=(',', ':'))
    SNAPSHOT.write_bytes(gzip.compress(data.encode('utf-8'), 9, mtime=0))


def outdoor_points(project, cells):
    at = {c['id']: (c['x'], c['y']) for c in cells}
    pts = []
    for x in project.get('people', []):
        for k in ('home', 'work', 'evening'):
            s = x.get(k)
            if s and s['cell'] in at:
                pts.append((at[s['cell']][0] + s['x'], at[s['cell']][1] + s['y']))
    for r in project.get('routes', []):
        for s in r['posts']:
            if s['cell'] in at:
                pts.append((at[s['cell']][0] + s['x'], at[s['cell']][1] + s['y']))
    for link in project.get('links', []):
        for e in ('a', 'b'):
            if link[e]['cell'] in at:
                pts.append((at[link[e]['cell']][0] + link[e]['x'], at[link[e]['cell']][1] + link[e]['y']))
    sp = project.get('spawn')
    if sp and sp['cell'] in at:
        pts.append((at[sp['cell']][0] + sp['x'], at[sp['cell']][1] + sp['y']))
    return pts


def expand(project, seed=SEED, report=print, city_cells=None):
    """Lays the realm round the city: the project's cells become the city's nine (wild ground remade) and the realm's.
    Returns the realm (its sites, roads, rivers, lakes and checks) after changing `project` in place."""
    if city_cells is None:
        city_cells, from_snapshot = pristine_city(project)
    else:
        from_snapshot = False
    codes, heights = city_canvas(city_cells)
    if from_snapshot:
        # The world's city must still be the snapshot's: its built tiles unchanged since the realm was laid.
        now_codes, now_heights = city_canvas([c for c in project['cells'] if is_city_cell(c)])
        built = ~np.isin(codes, [ord(c) for c in WILD])
        if not (np.array_equal(now_codes[built], codes[built]) and np.array_equal(now_heights[built], heights[built])):
            raise SystemExit('The city\'s built ground differs from the snapshot the realm was laid round: write the city '
                             'again (python3 -m worldgen.ridgemere2 write), which lays the realm round it.')
    realm = Realm2(codes, heights, seed, report, protect=outdoor_points(project, city_cells))
    realm.build()
    cells = realm.cells(city_cells)
    realm.city_cells = city_cells                       # (kept as the snapshot only once a write has saved: keep_snapshot)
    project['cells'] = cells
    realm.result = realm.checks(project, cells, codes, heights)
    return realm


def report_of(realm, project):
    kinds = Counter(s['kind'] for s in realm.sites)
    roads = {k: v for k, v in realm.roads.items() if v['kind'] == 'road'}
    tracks = {k: v for k, v in realm.roads.items() if v['kind'] == 'track'}
    rivers = [l for l in realm.river_lines if l['major'] and not l['dry']]
    return {
        'size': {'tiles': SIZE, 'cells': len(project['cells']), 'cellSize': CELL, 'grid': N},
        'regions': dict(Counter(realm.regions.values())),
        'weather': dict(Counter(c['weather'] for c in project['cells'])),
        'sites': dict(kinds), 'siteList': realm.sites,
        'reserved': {'ownFields': sum(s.get('tiles', 0) for s in realm.sites if s['kind'] in OWN_FIELDS
                                      and not s['for'].startswith('spare')),
                     'range': sum(s.get('tiles', 0) for s in realm.sites if s['kind'] in RANGE),
                     'spare': sum(s.get('tiles', 0) for s in realm.sites if s['for'].startswith('spare')),
                     'cellTiles': CELL * CELL},
        'roads': {k: {'length': v['length'], 'bridges': v['bridges']} for k, v in roads.items()},
        'tracks': {'count': len(tracks), 'length': sum(v['length'] for v in tracks.values())},
        'rivers': [{'name': l.get('name', ''), 'length': int(len(l['pts']) * 16), 'width': round(l['width'], 1),
                    'from': [int(v) for v in l['from']], 'to': [int(v) for v in l['to']], 'top': l['top'],
                    'mouth': l['mouth']} for l in sorted(rivers, key=lambda l: -l['flow'])],
        'streams': sum(1 for l in realm.river_lines if not l['major'] and not l['dry']),
        'washes': sum(1 for l in realm.river_lines if l['dry']),
        'lakes': [{k: lake[k] for k in ('name', 'x', 'y', 'level', 'tiles')} for lake in sorted(realm.lakes, key=lambda k: -k['tiles'])],
        'tiles': {chr(k): int(v) for k, v in zip(*np.unique(realm.codes, return_counts=True))},
        'cliffs': realm.cliffs, 'reachedShare': round(realm.reached_share, 4),
        'checks': realm.result, 'timings': realm.timings,
    }


SITE_COLOURS = {'town': (255, 70, 60), 'farmland': (255, 225, 0), 'mill': (255, 160, 0), 'orchard': (255, 150, 200),
                'vineyard': (190, 80, 220), 'dairy': (255, 255, 190), 'pasture': (170, 255, 120),
                'pigfarm': (230, 120, 120), 'woodpasture': (120, 170, 60), 'warren': (210, 180, 140),
                'apiary': (255, 200, 60), 'fishing': (0, 220, 255), 'marsh': (60, 160, 160), 'salt': (255, 255, 255),
                'saltcoast': (220, 240, 255), 'timber': (20, 110, 30), 'charcoal': (90, 60, 40),
                'quarry': (215, 215, 215), 'clay': (160, 95, 45), 'mine': (40, 40, 40)}


def previews(realm, folder):
    """The overview (a labelled map and a plain one) and the close-ups of each region and of the blends between them."""
    from PIL import ImageDraw, ImageFont
    folder.mkdir(parents=True, exist_ok=True)
    plain = picture(realm.codes, realm.h, scale=.25, relief=1.3)
    plain.save(folder / 'overview.png')
    big = picture(realm.codes, realm.h, scale=.5, relief=1.1)
    big.save(folder / 'overview-large.png')
    img = plain.copy()
    d = ImageDraw.Draw(img)
    font = ImageFont.truetype(FONT, 11)
    small = ImageFont.truetype(FONT, 9)
    colours = SITE_COLOURS
    for name, r in realm.roads.items():
        if r['kind'] == 'road':
            d.line([(x / 4, y / 4) for x, y in r['points']], fill=(222, 190, 120), width=2)
    for s in realm.sites:
        x, y = s['x'] / 4, s['y'] / 4
        rr = 5 if s['kind'] == 'town' else 3
        d.ellipse([x - rr, y - rr, x + rr, y + rr], outline=(0, 0, 0), fill=colours[s['kind']])
        if s['kind'] == 'town':
            d.text((x + 7, y - 7), s['name'], fill=(255, 255, 255), font=font, stroke_width=2, stroke_fill=(0, 0, 0))
    for (lx, ly), text in OVERVIEW_LABELS:
        d.text((lx / 4, ly / 4), text, fill=(255, 240, 170), font=font, stroke_width=2, stroke_fill=(0, 0, 0))
    y = 800
    for kind, col in colours.items():
        d.ellipse([10, y, 18, y + 8], outline=(0, 0, 0), fill=col)
        d.text((22, y - 2), kind, fill=(255, 255, 255), font=small, stroke_width=2, stroke_fill=(0, 0, 0))
        y += 11
    img.save(folder / 'overview-labelled.png')
    for name, box in CLOSE_UPS.items():
        picture(realm.codes, realm.h, box=box, scale=1, relief=1.2).save(folder / f'{name}.png')


def write(world_key, seed=SEED, dry=False, report=print, map_mode='same'):
    """Back up; load the world; lay the realm round its city; check; save (the same map: an expansion keeps the map's
    identity and records its new layout); tell open Atlas sessions to reload. Refuses, writing nothing, if a check fails."""
    import map_editor
    import world_db
    import world_store
    import world_sync
    from . import ridgemere2 as r2
    entry = world_db.world(world_key)
    if entry['kind'] != 'dev' or entry['database'] in ('ratw_dev', 'ratw_prod'):
        raise SystemExit('The realm is written only into Ridgemere 2.0\'s own world (dev, also called ridgemere2).')
    project, revision, seen = r2.load(world_key)      # (With the game's changes seen: doc 66, Progress, "The live world's truth".)
    realm = expand(project, seed, report)
    rep = report_of(realm, project)
    ART.mkdir(parents=True, exist_ok=True)
    (ART / 'realm-report.json').write_text(json.dumps(rep, indent=1), encoding='utf-8')
    previews(realm, ART)
    failed = [c for c in realm.result if not c['ok']]
    for c in realm.result:
        report(f'  {"ok  " if c["ok"] else "FAIL"} {c["check"]} {c["detail"][:200]}')
    p = project
    p.pop('relations', None)
    t0 = time.time()
    try:
        map_editor.check_project(p, for_game=False, playable=True)
    except map_editor.ValidationError as error:
        report('Atlas validation failed:\n  ' + '\n  '.join(error.errors[:30]))
        return 1
    report(f'  Atlas validation: {time.time() - t0:.0f} s')
    if failed:
        report('Checks failed; nothing written.')
        return 1
    if dry:
        report('All checks pass (dry run: nothing written).')
        return 0
    before, before_revision, _ = r2.load(world_key)    # (The world as it is now, the project above being remade.)
    dump, atlas = r2.backup(world_key, before, before_revision)
    report(f'Backed up to {dump} and {atlas}.')
    t0 = time.time()
    with world_db.connect(world_key, 'owner') as conn:
        with conn.transaction():
            outcome = {}
            new = world_store.save_world(conn, p, revision, seen=seen, outcome=outcome)
            ident, kept = world_sync.stamp_for_layout(conn, world_sync.layout_fingerprint(p), MAP_NAME, 'worldgen.realm2',
                                                      'generated', map_mode)
            world_sync.tell_editors(conn, p['id'], 'realm2.py', 'Ridgemere 2.0: the realm (doc 70)')
    keep_snapshot(realm.city_cells)                     # (Saved: the city the realm was laid round is the one kept.)
    report(f'Saved in {time.time() - t0:.0f} s: revision {new} (+1 for the editors\' reload); ' +
           (f'the same map, {ident["id"]} ({ident.get("name", "")}), its new layout recorded.' if kept == 'kept' else
            f'a new map identity, {ident["id"]}.'))
    for line in world_store.truth_lines(outcome):        # (What the game server changed meanwhile: doc 66, Progress.)
        report(f'  {line}')
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    for name in ('preview', 'write'):
        s = sub.add_parser(name)
        s.add_argument('--seed', type=int, default=SEED)
        s.add_argument('--world', default='dev')
        s.add_argument('--dry-run', action='store_true')
        if name == 'preview':
            s.add_argument('--out', type=Path, default=ART, help='the folder for the previews and the report')
        if name == 'write':
            m = s.add_mutually_exclusive_group()
            m.add_argument('--new-map', action='store_const', const='new', dest='map_mode')
            m.add_argument('--auto-map', action='store_const', const='auto', dest='map_mode')
    args = parser.parse_args(argv)
    if args.command == 'write':
        return write(args.world, args.seed, dry=args.dry_run, map_mode=args.map_mode or 'same')
    from . import ridgemere2 as r2
    project, _, _ = r2.load(args.world)
    realm = expand(project, args.seed)
    rep = report_of(realm, project)
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / 'realm-report.json').write_text(json.dumps(rep, indent=1), encoding='utf-8')
    previews(realm, args.out)
    failed = [c for c in realm.result if not c['ok']]
    for c in realm.result:
        print(f'  {"ok  " if c["ok"] else "FAIL"} {c["check"]} {c["detail"][:200]}')
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
