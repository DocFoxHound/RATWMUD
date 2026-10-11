"""The country's farms and works, and the people who work them (Docs/Design/60).

Around Ridgemere, in the Folds, the farms are livestock: sheep runs and rabbit warrens (House Mossgrave's), a dairy and
a piggery, with fisheries on the Sound, logging camps in the Rainwood and a quarry and a mine in the Rainward Hills.
Further south the farms are grain and the orchards begin: round Larkfield and Southmeadow, in the Orchard Vale and the
Sunfields. Each town keeps a few sites of its own: mines above Frosthollow, the lake's fishery and a water mill at
Coldwater, logging and hives in the Warmwood round Oakhaven, eel fishing and clay in the fens at Mirebridge.

Each site is its yard (fields, orchard rows, a fenced pasture, hutches, a quarry pit, a spoil heap, a log yard, drying
racks) and its building (a farmhouse, a barn, a rabbitry, a press house, a mine head...), placed in its cell on ground
that suits it, every door and work spot walkable from the road; a bunkhouse beside it ("<site> Bunkhouse", which the
game finds by name for its hired hands). Its people are new: a farm's family in its farmhouse, a works' keeper, and its
hands in the bunkhouse. They belong to the settlement the site serves (its rooms carry that region).

Run by worldgen.realm after the city and the towns are built.
"""
from __future__ import annotations

import math
import random

import numpy as np

import terrain_catalog as catalog
from . import residents as R
from .buildings import WORKS, Building, house, tenement, works
from .canvas import DEFAULT_HEIGHT, Canvas, code, reachable
from .cities import fp, grow
from .site import Site

SEED = 6060
PREFIX = 'cty_'
BEDS = 10
DISTRICT = 'ind_'                       # (The district the sites' buildings are recorded under.)
# Each settlement's sites: (kind, how many, the cells (cx, cy) to try in order).
FOLDS = [(1, 2), (1, 3), (0, 3), (2, 3), (0, 2), (1, 4)]
SITES = {
    'ridgemere': [('sheep', 4, FOLDS), ('rabbit', 3, FOLDS), ('dairy', 1, FOLDS), ('piggery', 1, FOLDS),
                  ('fishery', 1, [(0, 0), (0, 2), (0, 1)]), ('logging', 2, [(2, 0), (3, 0), (3, 1), (1, 0)]),
                  ('quarry', 1, [(4, 1), (3, 1), (4, 0)]), ('mine', 1, [(4, 0), (4, 1), (3, 0)]),
                  ('apiary', 1, FOLDS)],
    'frosthollow': [('mine', 2, [(6, 1), (6, 2), (7, 1), (5, 1)]), ('logging', 1, [(6, 2), (5, 2), (6, 1)])],
    'coldwater': [('fishery', 1, [(4, 2), (5, 2)]), ('mill', 1, [(4, 3), (4, 2)]), ('farm', 1, [(4, 3), (3, 3), (4, 2)]),
                  ('pasture', 1, [(4, 3), (3, 2), (3, 3)])],
    'oakhaven': [('logging', 2, [(2, 4), (1, 4), (3, 4)]), ('farm', 2, [(2, 4), (2, 5), (3, 4)]),
                 ('apiary', 1, [(2, 4), (1, 4)]), ('piggery', 1, [(2, 5), (2, 4)]), ('orchard', 1, [(2, 5), (3, 5)])],
    'mirebridge': [('fishery', 1, [(1, 6), (2, 6), (1, 7)]), ('claypit', 1, [(1, 6), (2, 6), (1, 7)]),
                   ('farm', 1, [(2, 6), (2, 7), (1, 5)]), ('pasture', 1, [(2, 6), (1, 5), (2, 5)])],
    'larkfield': [('farm', 3, [(5, 4), (4, 4), (5, 5), (4, 5)]), ('orchard', 2, [(5, 5), (6, 5), (5, 4)]),
                  ('dairy', 1, [(5, 4), (6, 4)]), ('sheep', 1, [(6, 4), (5, 3)]), ('mill', 1, [(5, 5), (4, 5), (6, 4)])],
    'southmeadow': [('farm', 4, [(6, 6), (5, 6), (6, 7), (5, 7), (4, 6), (4, 7), (3, 7)]),
                    ('orchard', 3, [(6, 5), (5, 5), (6, 6), (7, 6), (5, 6)]), ('vineyard', 1, [(7, 6), (7, 7), (6, 7)]),
                    ('apiary', 1, [(6, 6), (6, 7)]), ('rabbit', 1, [(7, 6), (6, 7)])],
}
NAMES = {  # The sites' names, in their own country's voice (short: "keeps rabbits at <name>" fits 40 letters).
    'ridgemere': ['Ewecombe', 'Fleecemoor', 'Ramsfold', 'Woolgate', 'Harrowburrow', 'Kitwarren', 'Coneyhill',
                  'Wetmeadow', 'Mudwallow', 'Greywater', 'Cedarfall', 'Fellside', 'Greyscar', 'Rainwash', 'Mossbank'],
    'frosthollow': ['Deepvein', 'Icegrip', 'Snowpine'],
    'coldwater': ['Glasswater', 'Coldwater', 'Lakeside', 'Shorefold'],
    'oakhaven': ['Oakfell', 'Beechcombe', 'Haven Fields', 'Acorn Lea', 'Honeywood', 'Mastwood', 'Russet'],
    'mirebridge': ['Eelwater', 'Clayholt', 'Mirefield', 'Reedfold'],
    'larkfield': ['Larkrise', 'Hayward', 'Barleycombe', 'Pippin Hill', 'Redcoat', 'Whiteleas', 'Downfold', 'Larkmill'],
    'southmeadow': ['Goldfurrow', 'Wheatley', 'Sheaf End', 'Southfield', 'Blossom Vale', 'Applegarth', 'Cherryholt',
                    'Sunvine', 'Bramblehive', 'Meadowarren'],
}
STYLE = {'ridgemere': 'ridgemere'}


# --- The sites: each kind's yard and building, placed on ground that suits it (from worldgen.industry and
# worldgen.farms, doc 42) -----------------------------------------------------------------------------------------
OPEN = set(',;"35!-')
FOREST = set('YP$12&7')
ROCK = set('rsXo')
WATER = set('W~')

# Each kind: its yard (w, h) and how it is painted; its building (a works kind, or a farmhouse); its hands and what
# their work is called (the producer it makes them, crafts.json); the keeper who runs its works and sells what they
# make (a business, businesses.json), if any; and the ground it wants.
KINDS = {
    'farm': dict(yard=(22, 12), paint='field', building='farmhouse', hands=5, label='works the fields at {n}',
                 name='{n} Farm', ground='open'),
    'orchard': dict(yard=(18, 12), paint='orchard', building='press_house', hands=4, label='tends the orchard at {n}',
                    name='{n} Orchard', ground='open'),
    'vineyard': dict(yard=(20, 12), paint='vines', building='press_house', hands=4, label='tends the vines at {n}',
                     name='{n} Vineyard', keeper='brewer at {building}', building_name='The {n} Press', ground='open'),
    'pasture': dict(yard=(22, 14), paint='pasture', building='fold', hands=4, label='keeps sheep at {n}',
                    name='{n} Fold', ground='open'),
    'dairy': dict(yard=(18, 12), paint='pasture', building='dairy', hands=6, label='milks the herd at {n}',
                  name='{n} Dairy', keeper='runs {building}', ground='open'),
    'piggery': dict(yard=(16, 10), paint='sty', building='fold', hands=5, label='keeps pigs at {n}',
                    name='{n} Piggery', ground='open'),
    'apiary': dict(yard=(10, 8), paint='hives', building='bee_shed', hands=2, label='keeps bees at {n}',
                   name='{n} Hives', ground='open'),
    'quarry': dict(yard=(18, 12), paint='quarry', building='quarry_office', hands=6, label='cutting stone at {n}',
                   name='{n} Quarry', second=('limeworks', 'The {n} Lime Kilns'), keeper='runs {second}', ground='rock'),
    'mine': dict(yard=(12, 8), paint='spoil', building='mine_head', hands=5, label='digs ore at {n}',
                 name='{n} Mine', ground='rock'),
    'logging': dict(yard=(14, 8), paint='logyard', building='logging_camp', hands=6, label='logging at {n}',
                    name='{n} Logging Camp', ground='forest'),
    'fishery': dict(yard=(10, 4), paint='racks', building='smokehouse', hands=6, label='fishes the waters off {n}',
                    name='{n} Fishery', keeper='runs {building}', building_name='The {n} Smokehouse', ground='shore'),
    'claypit': dict(yard=(14, 10), paint='clay', building='brickworks', hands=4, label='digs clay at {n}',
                    name='{n} Clay Pits', keeper='runs {building}', building_name='The {n} Brickworks', ground='shore'),
    'mill': dict(yard=None, building='mill', hands=2, label='works at {building}'[:40], name='{n} Water Mill',
                 keeper='keeps the water mill at {n}', building_name='The {n} Water Mill', ground='shore'),
    'smelter': dict(yard=None, building='foundry', hands=2, label='works at {building}', name='The {n} Smelter',
                    keeper='runs {building}', building_name='The {n} Smelter', ground='open'),
}
HOURS = (6, 17)


class Plot:
    """One cell being built on: its canvas, what is free, where a wolf comes in from (towards the settlement)."""

    def __init__(self, project, cell, toward, reserved, region):
        self.cell, self.region = cell, region
        c = cell
        self.canvas = Canvas(c['width'], c['height'])
        for yy, row in enumerate(c['terrain']):
            self.canvas.codes[yy] = np.frombuffer(row.encode('ascii'), dtype=np.uint8)
        self.canvas.heights[:] = DEFAULT_HEIGHT[self.canvas.codes]
        for key, value in c.get('heights', {}).items():
            hx, hy = map(int, key.split(','))
            self.canvas.heights[hy, hx] = value
        self.before = self.canvas.codes.copy()
        self.site = Site(self.canvas, [(c['id'], 0, 0, c['width'], c['height'])])
        self.site.ids |= set(reserved)
        codes = self.canvas.codes
        solid = np.isin(codes, [ord(t['code']) for t in catalog.TILES if t.get('solid')])
        # Where one comes in: the road tile nearest the settlement, or the walkable tile on the cell's edge nearest it.
        tx = min(max(toward[0] - c['x'], 0), c['width'] - 1)
        ty = min(max(toward[1] - c['y'], 0), c['height'] - 1)
        self.toward = (tx, ty)
        roads = np.isin(codes, [ord(ch) for ch in 'd_8'])
        if not roads.any():
            roads = np.zeros_like(roads)
            roads[0, :] = roads[-1, :] = roads[:, 0] = roads[:, -1] = True
            roads &= ~solid & ~np.isin(codes, [ord(ch) for ch in 'W~w'])
        ys, xs = np.nonzero(roads)
        i = int(np.argmin((xs - tx) ** 2 + (ys - ty) ** 2))
        self.start = (int(xs[i]), int(ys[i]))
        self.walk = reachable(codes, self.canvas.heights, self.start)
        self.taken = np.zeros(codes.shape, dtype=bool)
        built = ~np.isin(codes, [ord(t['code']) for t in catalog.TILES if t['category'] in ('ground', 'nature')])
        self.taken |= grow(built | np.isin(codes, [ord(ch) for ch in 'd_8+G']), 4)
        self.changed = False

    def mask(self, chars):
        return np.isin(self.canvas.codes, [ord(ch) for ch in chars])

    def write_back(self):
        rows, overrides = self.canvas.cell(0, 0, self.cell['width'], self.cell['height'])
        self.cell['terrain'] = rows
        self.cell['heights'] = overrides


def suitable(plot: Plot, ground):
    """Tiles a site of this ground may stand on (its yard and building), and a mask it must be near (or None)."""
    codes = plot.canvas.codes
    walk = plot.walk & ~plot.taken
    if ground == 'open':
        return walk & plot.mask(OPEN | set('4')), None
    if ground == 'rock':
        return walk & plot.mask(OPEN | ROCK | set('4')), plot.mask(ROCK | set('%'))
    if ground == 'forest':
        return walk & plot.mask(OPEN | set('!&')), plot.mask(FOREST)
    if ground == 'shore':
        return walk & plot.mask(OPEN | set('0D') | ROCK), plot.mask(WATER)
    raise ValueError(ground)


def building_for(kind_spec, n, style, rng, families):
    """The site's building: a farmhouse (a house), or a works of its kind."""
    b_kind = kind_spec['building']
    if b_kind == 'farmhouse':
        p = house(rng, style if style in ('city', 'serferro', 'ridgemere') else 'city', 4)
        name = f'{n} Farmhouse'
        return Building('house', name, style, fp(p.w, p.h), [p.room('', name, 'A farmhouse kitchen with a long table, '
                        'boots by the door and the smell of the fields.')], district=DISTRICT)
    name = kind_spec.get('building_name', kind_spec['name']).format(n=n)
    w = works(rng, style if style == 'serferro' else 'works', b_kind, kind_spec['hands'])
    return Building('works', name, style, fp(w.w, w.h), [w.room('', name, f'{name}: {WORKS[b_kind][1]}.')],
                    district=DISTRICT, trade=b_kind)


def paint_yard(plot: Plot, kind, x0, y0, w, h, rng):
    """The yard: fields, rows of trees or vines, a pasture, a quarry pit, a spoil heap... Returns its work spots."""
    c = plot.canvas
    keep = None                                      # (Heights stay as the land lies: only a field is levelled.)
    spots = []
    if kind == 'field':
        level = float(np.median(c.heights[y0:y0 + h, x0:x0 + w]))
        c.rect(x0, y0, w, h, '4', level)
        c.outline(x0 - 1, y0 - 1, w + 2, h + 2, '|', level)
        c.codes[y0 + h // 2, x0 - 1] = code(',')
        c.codes[y0 + h // 2, x0 + w] = code(',')
        spots = [(x, y) for y in range(y0 + 1, y0 + h - 1, 3) for x in range(x0 + 1, x0 + w - 1, 3)]
    elif kind in ('orchard', 'vines', 'hives'):
        tree = {'orchard': 'Y', 'vines': 'B', 'hives': 'O'}[kind]
        c.rect(x0, y0, w, h, '3' if kind == 'hives' else ',', keep)
        for y in range(y0 + 1, y0 + h - 1, 3):
            for x in range(x0 + 1, x0 + w - 1, 1 if kind == 'vines' else 3):
                if kind != 'vines' or (x - x0) % 6 != 3:            # (Gaps in the vine rows to walk through.)
                    c.codes[y, x] = code(tree)
        spots = [(x, y) for y in range(y0 + 2, y0 + h - 1, 3) for x in range(x0 + 2, x0 + w - 1, 4)]
    elif kind == 'pasture':
        c.rect(x0, y0, w, h, '"', keep)
        c.outline(x0 - 1, y0 - 1, w + 2, h + 2, '|')
        c.codes[y0 + h // 2, x0 - 1] = code(',')
        c.codes[y0 + h // 2, x0 + w] = code(',')
        spots = [(x, y) for y in range(y0 + 2, y0 + h - 1, 4) for x in range(x0 + 2, x0 + w - 1, 5)]
    elif kind == 'quarry':
        c.rect(x0, y0, w, h, 's', keep)
        c.rect(x0 + 2, y0 + 2, w - 4, h - 4, 'r', keep)
        for _ in range(5):
            c.codes[y0 + rng.randint(3, h - 4), x0 + rng.randint(3, w - 4)] = code('o')
        spots = [(x, y) for y in range(y0 + 3, y0 + h - 3, 3) for x in range(x0 + 3, x0 + w - 3, 4)]
    elif kind == 'spoil':
        c.rect(x0, y0, w, h, 'X', keep)
        c.rect(x0 + 1, y0 + 1, w - 2, 2, '>', keep)
        spots = [(x, y0 + 1) for x in range(x0 + 1, x0 + w - 1, 2)]
    elif kind == 'logyard':
        c.rect(x0, y0, w, h, '>', keep)
        for x in range(x0 + 1, x0 + w - 1, 3):
            c.codes[y0 + 1, x] = code('<')
        spots = [(x, y0 + h - 2) for x in range(x0 + 1, x0 + w - 1, 2)]
    elif kind == 'racks':
        for x in range(x0, x0 + w, 2):
            c.codes[y0, x] = code('x')
        spots = [(x, y0 + 2) for x in range(x0, x0 + w, 2)]
    elif kind == 'sty':
        c.rect(x0, y0, w, h, 'D', keep)
        c.outline(x0 - 1, y0 - 1, w + 2, h + 2, '|')
        c.codes[y0 + h // 2, x0 - 1] = code(',')
        c.codes[y0 + h // 2, x0 + w] = code(',')
        spots = [(x, y) for y in range(y0 + 2, y0 + h - 1, 4) for x in range(x0 + 2, x0 + w - 1, 4)]
    elif kind == 'clay':
        c.rect(x0, y0, w, h, 'D', keep)
        spots = [(x, y) for y in range(y0 + 1, y0 + h - 1, 3) for x in range(x0 + 1, x0 + w - 1, 3)]
    elif kind == 'hutches':
        # A rabbit farm's yard (worldgen.farms): rows of hutches (crates on straw) inside a fence, aisles between.
        c.rect(x0, y0, w, h, ',', keep)
        c.outline(x0 - 1, y0 - 1, w + 2, h + 2, '|')
        c.codes[y0 + h // 2, x0 - 1] = code(',')
        c.codes[y0 + h // 2, x0 + w] = code(',')
        rows = list(range(y0 + 1, y0 + h - 1, 3))
        for y in rows:
            for x in range(x0 + 1, x0 + w - 1):
                if (x - x0) % 7 != 0:                # (Gaps to walk between the hutches.)
                    c.codes[y, x] = code('x' if (x - x0) % 2 else 'z')
        spots = [(x, y + 1) for y in rows for x in range(x0 + 2, x0 + w - 2, 5) if y + 1 < y0 + h]
    return spots


def place_site(plot: Plot, kind, spec, n, style, rng):
    """A site on the plot: its yard, then its building beside it (and a second, a quarry's lime kilns); every door and
    work spot walkable from where one comes in. Returns {'name', 'records', 'spots'} or None."""
    can, near_mask = suitable(plot, spec['ground'])
    if near_mask is not None:
        near_ok = grow(near_mask, 6)
    c = plot.canvas
    H, W = c.codes.shape
    tx, ty = plot.toward
    b = building_for(spec, n, style, rng, None)
    second = None
    if spec.get('second'):
        sk, sname = spec['second']
        sw = works(rng, style if style == 'serferro' else 'works', sk, 2)
        nm = sname.format(n=n)
        second = Building('works', nm, style, fp(sw.w, sw.h), [sw.room('', nm, f'{nm}: {WORKS[sk][1]}.')], district=DISTRICT, trade=sk)
    yw, yh = spec['yard'] if spec['yard'] else (0, 0)
    bw, bh = b.footprint
    sw_, sh_ = second.footprint if second else (0, 0)
    total_w = yw + (3 if yw else 0) + bw + (3 + sw_ if second else 0)
    total_h = max(yh, bh, sh_)
    tries = []
    for _ in range(5000):
        x0 = rng.randint(4, max(5, W - total_w - 6))
        y0 = rng.randint(4, max(5, H - total_h - 6))
        if x0 + total_w + 4 >= W or y0 + total_h + 4 >= H:
            continue
        area = (slice(y0 - 1, y0 + total_h + 1), slice(x0 - 1, x0 + total_w + 1))
        if plot.taken[area].any() or can[area].mean() < .85:
            continue
        if near_mask is not None and not near_ok[area].any():
            continue
        tries.append((math.hypot(x0 + total_w / 2 - tx, y0 + total_h / 2 - ty), x0, y0))
    tries.sort()
    for _, x0, y0 in tries[:25]:
        saved = (c.codes.copy(), c.heights.copy(), len(plot.site.rooms), len(plot.site.links), len(plot.site.manifest))
        spots = paint_yard(plot, spec.get('paint'), x0, y0, yw, yh, rng) if yw else []
        bx = x0 + yw + (3 if yw else 0)
        by = y0 + (total_h - bh) // 2
        _, out1 = plot.site.place(b, bx, by, 'S', plot.region, open_door=True)
        records, outs = [plot.site.manifest[-1]], [out1]
        if second:
            _, out2 = plot.site.place(second, bx + bw + 3, y0 + (total_h - sh_) // 2, 'S', plot.region, open_door=True)
            records.append(plot.site.manifest[-1])
            outs.append(out2)
        walk = reachable(c.codes, c.heights, plot.start)
        # The tile before each door is walked to, at the door's own height (Atlas checks the arrival tile).
        doors_ok = all(0 <= ox < W and 0 <= oy < H and walk[oy, ox] and walk[r['door']['y'], r['door']['x']]
                       for r, (ox, oy) in zip(records, outs))
        spots = [(x, y) for x, y in spots if walk[y, x]]
        if doors_ok and (not yw or len(spots) >= max(2, spec['hands'] // 2)):
            plot.taken[max(0, y0 - 6):y0 + total_h + 6, max(0, x0 - 6):x0 + total_w + 6] = True
            plot.walk = walk
            plot.changed = True
            return {'name': spec['name'].format(n=n), 'records': records, 'spots': spots, 'cell': plot.cell['id']}
        c.codes[:], c.heights[:] = saved[0], saved[1]
        del plot.site.rooms[saved[2]:], plot.site.links[saved[3]:], plot.site.manifest[saved[4]:]
    return None


FARM_KINDS = {
    'sheep': dict(yard=(26, 16), paint='pasture', building='barn', hands=4, label='raises sheep at {n}',
                  name='{n} Sheep Farm', building_name='{n} Barn', ground='open'),
    'rabbit': dict(yard=(24, 14), paint='hutches', building='rabbitry', hands=5, label='keeps rabbits at {n}',
                   name='{n} Rabbit Farm', building_name='{n} Rabbitry', ground='open'),
}
KINDS.update(FARM_KINDS)


def place_lone(plot, b, rng):
    """One building near plot.toward, on open ground, its door walkable; returns its rooms' ids, or None."""
    c = plot.canvas
    H, W = c.codes.shape
    can, _ = suitable(plot, 'open')
    fw, fh = b.footprint
    tx, ty = plot.toward
    tries = []
    for _ in range(4000):
        x0, y0 = tx + rng.randint(-90, 90), ty + rng.randint(-90, 90)
        if x0 < 4 or y0 < 4 or x0 + fw + 4 >= W or y0 + fh + 4 >= H:
            continue
        area = (slice(y0 - 1, y0 + fh + 1), slice(x0 - 1, x0 + fw + 1))
        if plot.taken[area].any() or can[area].mean() < .8:
            continue
        tries.append((math.hypot(x0 - tx, y0 - ty), x0, y0))
    tries.sort()
    for _, x0, y0 in tries[:20]:
        saved = (c.codes.copy(), c.heights.copy(), len(plot.site.rooms), len(plot.site.links), len(plot.site.manifest))
        _, outside = plot.site.place(b, x0, y0, 'S', plot.region, open_door=True)
        rec = plot.site.manifest[-1]
        walk = reachable(c.codes, c.heights, plot.start)
        ox, oy = outside
        if 0 <= ox < W and 0 <= oy < H and walk[oy, ox] and walk[rec['door']['y'], rec['door']['x']]:
            plot.taken[max(0, y0 - 4):y0 + fh + 4, max(0, x0 - 4):x0 + fw + 4] = True
            plot.walk = walk
            plot.changed = True
            return [r['id'] for r in rec['rooms']]
        c.codes[:], c.heights[:] = saved[0], saved[1]
        del plot.site.rooms[saved[2]:], plot.site.links[saved[3]:], plot.site.manifest[saved[4]:]
    return None


def cell_at(project, cx, cy):
    from .realm import CELL, OX, OY
    x, y = cx * CELL + OX, cy * CELL + OY
    return next((c for c in project['cells'] if c.get('z', 0) == 0 and c['x'] == x and c['y'] == y), None)


def centres():
    from .realm import SETTLEMENTS
    out = {s[0]: (s[3][0] + s[3][2] // 2, s[3][1] + s[3][3] // 2) for s in SETTLEMENTS}
    out['ridgemere'] = (480, 380)
    return out


def names_for(sid):
    """(female, male, family heads, family tails) of a settlement's people."""
    from . import citizens as C
    from .towns import TOWNS
    if sid == 'ridgemere':
        return C.RM_FEMALE, C.RM_MALE, C.RM_HEADS, C.RM_TAILS
    cfg = TOWNS[sid]
    return R.FEMALE, R.MALE, cfg['heads'], cfg['tails']


def build(project, report=print):
    """The project with the country's sites and their people (a copy)."""
    from .citizens import Folk
    out = {**project, 'cells': [dict(c) for c in project['cells']], 'rooms': list(project['rooms']),
           'links': list(project['links']), 'people': [dict(p) for p in project.get('people', [])]}
    reserved = {a['id'] for a in out['cells'] + out['rooms']}
    centre = centres()
    plots, sites = {}, []
    for sid, wanted in SITES.items():
        rng = random.Random(f'{SEED}:{sid}')
        names = list(NAMES[sid])
        style = STYLE.get(sid, 'city')
        for kind, count, cells in wanted:
            spec = KINDS[kind]
            for k in range(count):
                n = names.pop(0) if names else f'{sid.title()} {len(sites) + 1}'
                site = None
                turn = (k + len(sites)) % len(cells)          # Spread the sites over their cells.
                for cx, cy in cells[turn:] + cells[:turn]:
                    cell = cell_at(out, cx, cy)
                    if cell is None:
                        continue
                    if cell['id'] not in plots:
                        plots[cell['id']] = Plot(out, cell, centre[sid], reserved, sid)
                    plot = plots[cell['id']]
                    plot.region = sid
                    plot.toward = (min(max(centre[sid][0] - cell['x'], 0), cell['width'] - 1),
                                   min(max(centre[sid][1] - cell['y'], 0), cell['height'] - 1))
                    site = place_site(plot, kind, spec, n, style, rng)
                    if site:
                        break
                if not site:
                    report(f'  {sid}: no room for a {kind} ({n})')
                    continue
                # Its bunkhouse, beside its building.
                door = site['records'][0]['door']
                plot.toward = (door['x'], door['y'] + 2)
                p = tenement(rng, 'works', BEDS)
                bname = f'{n} Bunkhouse'
                bunk = Building('tenement', bname, style, fp(p.w, p.h), [p.room('', bname, 'Ten bunks along the walls, a '
                                'stove and a table, and pegs for the hands\' coats: where those who work the place sleep.')],
                                district=DISTRICT)
                bunk_rooms = place_lone(plot, bunk, rng)
                if not bunk_rooms:
                    report(f'  {sid}: no room for a bunkhouse at {n}')
                site.update(sid=sid, kind=kind, n=n, spec=spec, bunk=bunk_rooms or [], plot=plot)
                sites.append(site)
                report(f'  {sid}: {site["name"]} in {site["cell"]}')
    for plot in plots.values():
        if plot.changed:
            plot.write_back()
            out['rooms'] += [{**r, 'worldX': r['worldX'] + plot.cell['x'], 'worldY': r['worldY'] + plot.cell['y']}
                             for r in plot.site.rooms]
            out['links'] += plot.site.links
    # The people: a farm's family in its farmhouse, a works' keeper, and the hands in the bunkhouse.
    folks = {}
    added = []
    for site in sites:
        sid, spec, n = site['sid'], site['spec'], site['n']
        if sid not in folks:
            female, male, heads, tails = names_for(sid)
            staged = {**out, 'people': out['people'] + added}
            folks[sid] = Folk(staged, [], random.Random(f'{SEED}:{sid}:people'), PREFIX, female, male, heads, tails,
                              R.QUIRKS, R.HOOKS)
            folks[sid].areas = {a['id']: a for a in out['cells'] + out['rooms']}
        P = folks[sid]
        rng = P.rng
        record = site['records'][0]
        rec = site['records'][-1] if spec.get('second') else record
        bname = record['name']
        label = spec['label'].format(n=n, site=site['name'], building=bname)[:40]
        cell = site['cell']
        beds = [(rid, b['x'], b['y']) for r in record['rooms'] if record['kind'] == 'house' for b in r['beds']]
        bunk_beds = []
        for rid in site['bunk']:
            room = P.areas[rid]
            bunk_beds += [(rid, x, y) for y, row in enumerate(room['terrain']) for x, ch in enumerate(row) if ch in 'bz']

        def bed():
            pool = beds if beds else bunk_beds
            while pool:
                rid, x, y = pool.pop(0)
                if (rid, x, y) not in P.taken:
                    return P.spot(rid, x, y)
            return None

        def work(i):
            if site['spots']:
                x, y = site['spots'][i % len(site['spots'])]
                return P.spot(cell, x, y, share=True)
            room = rec['rooms'][0]
            spots = room['work'] or [{'x': 2, 'y': 2}]
            s = spots[i % len(spots)]
            return P.spot(room['id'], s['x'], s['y'], share=True)
        family = P.family()
        start = len(P.people)
        if spec.get('keeper'):
            keeper = spec['keeper'].format(building=bname, second=rec['name'], n=n)[:40]
            room = rec['rooms'][0]
            s = (room['work'] or [{'x': 2, 'y': 2}])[0]
            home = bed() or P.floor(rec['rooms'][0]['id'], share=True)
            P.add(role='merchant', work_label=keeper, home=home, work=P.spot(room['id'], s['x'], s['y']), evening=home,
                  hours=(6, 18), family=family, job=f'who runs {rec["name"]}', age=rng.randint(30, 60))
        hands = max(2, spec['hands'] - 1)
        for i in range(hands):
            # A farm's own family works it first, from the farmhouse; the rest are hands in the bunkhouse.
            home = bed()
            if home is None:
                break
            own = home['cell'] in [r['id'] for r in record['rooms']]
            P.add(role='civilian', work_label=label, home=home, work=work(i), evening=home, hours=HOURS,
                  family=family if own else None, job=label if not own else f'of the family that farms {n}',
                  age=rng.randint(17, 58))
        if record['kind'] == 'house':                 # A farm child or two in the farmhouse beds left.
            for _ in range(2):
                home = bed() if beds else None
                if home is None:
                    break
                P.add(role='civilian', work_label='helps about the farm', home=home, work=work(0), evening=home,
                      hours=(9, 16), family=family, job='a child of the farm', age=rng.randint(7, 15), paid=False, purse=0)
        added += P.people[start:]
    out['people'] += added
    report(f'{len(sites)} sites, {len(added)} people to work them')
    return out, sites
