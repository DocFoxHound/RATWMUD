"""Industry outside the gates (Docs/Design/42-money-in-circulation.md, Phase 3c, and open question 7): farms, orchards,
vineyards, pasture, dairies, apiaries, quarries, mines, logging camps, fisheries, clay pits and mills built in the
country around the settlements, and the towns' surplus shopkeepers and shop help moved out to work them.

  cd tools && python3 -m worldgen.industry [--dry-run] [--preview DIR] [--out FILE]

Additive, on the world as DEV holds it now (backed up first, under artifacts/backups):
  1. Each site is placed in its cell on ground that suits it (open ground for fields, rock for a quarry, the water's
     edge for a mill or a fishery), as near the settlement as it fits, with its buildings and their interiors. Every
     door and work spot must be walkable from the cell's road (or its edge towards the settlement), or the site goes
     elsewhere. Nothing already built is touched.
  2. The rebalance: each settlement keeps a shop open for about every RESIDENTS_PER_SHOP of its people (cities
     CITY_PER_SHOP), at least one of each kind it has; the rest close (their buildings stand empty). Their keepers and
     help, and help beyond about one for every two open shops, become the sites' workers, keepers of the sites'
     works first. Everyone keeps their name, home, family and looks; only their work changes. Over 64, they retire.
Saved with DEV's revision checked, so an edit made meanwhile stops it rather than being lost.
"""
from __future__ import annotations

import argparse
import json
import math
import random
import re
import time
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np

import terrain_catalog as catalog
from .buildings import Building, house, works, WORKS
from .canvas import DEFAULT_HEIGHT, Canvas, code, reachable
from .cities import fp, grow
from .site import Site
from .western import OX, OY, SETTLEMENTS

SEED = 4242
RESIDENTS_PER_SHOP, CITY_PER_SHOP = 15, 10      # Placeholders (open question 7).
HELP_PER_SHOP = .5
RETIRE = 65
PREFIX = 'ind_'

# Where each settlement's industry goes: (kind, the cells to try in order, how many).
SITES = {
    'upper_accord': [('farm', ['western_approach', 'southwest_slopes', 'north_saddle'], 4), ('orchard', ['western_approach'], 1),
                     ('mine', ['southern_peak', 'eastern_peak', 'southeast_slopes'], 1),
                     ('dairy', ['north_saddle', 'western_approach', 'northeast_heights'], 1),
                     ('piggery', ['western_approach', 'southwest_slopes', 'southeast_slopes'], 1),
                     ('quarry', ['southwest_slopes', 'south_saddle', 'western_approach'], 1),
                     ('pasture', ['north_saddle', 'northeast_heights'], 1),
                     ('logging', ['southwest_slopes', 'northwest_heights', 'western_approach'], 1),
                     ('apiary', ['southeast_slopes', 'western_approach'], 1)],
    'ser_ferro': [('farm', ['w_ser_ferro_marches_1_8', 'w_ser_ferro_marches_1_9'], 4),
                  ('vineyard', ['w_ser_ferro_marches_1_9', 'w_ser_ferro_marches_1_8'], 1),
                  ('orchard', ['w_ser_ferro_marches_1_9', 'w_ser_ferro_marches_1_8'], 1),
                  ('dairy', ['w_ser_ferro_marches_1_8', 'w_ser_ferro_marches_0_8'], 2),
                  ('piggery', ['w_ser_ferro_marches_1_8', 'w_ser_ferro_marches_1_9'], 1),
                  ('mill', ['w_ser_ferro_marches_1_9', 'w_ser_ferro_marches_0_9'], 1),
                  ('claypit', ['w_ser_ferro_marches_1_9', 'w_ser_ferro_marches_0_9'], 1),
                  ('fishery', ['w_ser_ferro_marches_1_9', 'w_ser_ferro_marches_0_9'], 1),
                  ('apiary', ['w_ser_ferro_marches_1_8'], 1)],
    'ridgemere': [('mine', ['w_ridgemere_heights_2_1', 'w_ridgemere_heights_2_0'], 1),
                  ('logging', ['w_ridgemere_heights_2_1', 'w_ridgemere_heights_2_0'], 1),
                  ('fishery', ['w_ridgemere_heights_1_1', 'w_ridgemere_heights_1_0'], 1),
                  ('farm', ['w_ridgemere_heights_2_1', 'w_ridgemere_heights_2_0'], 1),
                  ('pasture', ['w_ridgemere_heights_2_0', 'w_ridgemere_heights_2_1', 'w_ridgemere_heights_1_1'], 1),
                  ('dairy', ['w_ridgemere_heights_2_0', 'w_ridgemere_heights_2_1', 'w_ridgemere_heights_1_1'], 1)],
    'cinderbrook': [('smelter', ['w_ser_ferro_marches_0_8'], 1)],
    'accord_crossing': [('farm', ['w_accord_marches_6_3'], 1)],
    'westmarch': [('dairy', ['w_southwold_downs_2_6'], 1)],
}
# Names for each settlement's sites, in its own tongue.
NAMES = {
    'upper_accord': ['High Terrace', 'Stonefield', 'Westward', 'Rowan Ley', 'Cragfoot', 'Peakside', 'Saddleback', 'Larkrise',
                     'Warden\'s Acre', 'Greyscar', 'Highcombe', 'Cowslip', 'Mudwallow'],
    'ser_ferro': ['Campo d\'Oro', 'Le Spighe', 'Casale Rosso', 'Vigna Alta', 'Poggio Chiaro', 'Il Fiume', 'Santa Lucia',
                  'Il Mulino', 'Fornace Bassa', 'La Riva', 'Le Api', 'Prato Verde', 'Il Porcile', 'Latteria Nuova'],
    'ridgemere': ['Grayrock', 'Cedarfall', 'Rainwash', 'Mossbank', 'Fellside', 'Greywater', 'Wetmeadow'],
    'cinderbrook': ['Cinderbrook'],
    'accord_crossing': ['Crossing Meadow'],
    'westmarch': ['Chalkdown'],
}
STYLE = {'upper_accord': 'city', 'ser_ferro': 'serferro', 'ridgemere': 'ridgemere', 'cinderbrook': 'works',
         'accord_crossing': 'city', 'westmarch': 'city'}
CITIES = {'upper_accord', 'ser_ferro', 'ridgemere'}

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
                        'boots by the door and the smell of the fields.')], district=PREFIX)
    name = kind_spec.get('building_name', kind_spec['name']).format(n=n)
    w = works(rng, style if style == 'serferro' else 'works', b_kind, kind_spec['hands'])
    return Building('works', name, style, fp(w.w, w.h), [w.room('', name, f'{name}: {WORKS[b_kind][1]}.')],
                    district=PREFIX, trade=b_kind)


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
        second = Building('works', nm, style, fp(sw.w, sw.h), [sw.room('', nm, f'{nm}: {WORKS[sk][1]}.')], district=PREFIX, trade=sk)
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


# ------------------------------------------------------------------------------------------------- The people

def settlement_of(project):
    """Region -> settlement id, and each room's region (people are of the settlement whose rooms they live in)."""
    region = {a['id']: a['territory']['region'] for a in project['cells'] + project['rooms']}
    return region


def rebalance(project, report):
    """Which keepers and helpers each settlement frees: {sid: [person]} (closed shops' keepers first)."""
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from supply_balance import BUSINESSES, first_match
    region = settlement_of(project)
    people = project['people']
    by_town = defaultdict(list)
    for p in people:
        by_town[region.get(p['home']['cell'], '?')].append(p)
    freed = {}
    closed_cells = {}
    spare_labour = {}
    for sid in SITES:
        folk = by_town.get(sid, [])
        if not folk:
            report(f'{sid}: nobody lives there?')
            continue
        shops = []
        for p in folk:
            if p['role'] != 'merchant':
                continue
            b = first_match(p['workLabel'], BUSINESSES)
            if b and (b.get('kind') in ('works', 'yard') or b['id'] == 'inn'):
                continue                              # (Industry stays, and so do the inns: travellers need them.)
            shops.append((p, b['id'] if b else p['workLabel']))
        target = max(math.ceil(len(folk) / (CITY_PER_SHOP if sid in CITIES else RESIDENTS_PER_SHOP)),
                     len({kind for _, kind in shops}) if sid in CITIES else 5)
        # Close the most duplicated kinds first; the first of each kind stays.
        kinds = Counter(kind for _, kind in shops)
        seen = Counter()
        ranked = []
        for p, kind in sorted(shops, key=lambda pk: pk[0]['id']):
            seen[kind] += 1
            ranked.append((seen[kind] > 1, kinds[kind], seen[kind], p, kind))
        ranked.sort(key=lambda r: (r[0], r[1], r[2]), reverse=True)
        to_close = max(0, len(shops) - target)
        closing = [r[3] for r in ranked[:to_close] if r[0]]
        closed = {p['work']['cell'] for p in closing}
        closed_cells[sid] = closed
        # Their help, and help beyond about one for every two open shops (inns and works keep theirs).
        inns = {p['work']['cell'] for p in folk if p['role'] == 'merchant' and (first_match(p['workLabel'], BUSINESSES) or {}).get('id') == 'inn'}
        help_ = [p for p in folk if p['role'] == 'civilian' and p['age'] >= 16 and p['age'] < RETIRE
                 and p['work']['cell'] not in inns
                 and re.search(r'helping at|helps at|serves at|minds the shop|minding the shop', p['workLabel'])]
        out_help = [p for p in help_ if p['work']['cell'] in closed]
        rest = [p for p in help_ if p['work']['cell'] not in closed]
        keep_help = math.ceil((len(shops) - len(closing)) * HELP_PER_SHOP)
        out_help += sorted(rest, key=lambda p: p['id'])[keep_help:]
        freed[sid] = closing + out_help
        labour = [p for p in folk if p['role'] == 'civilian' and 16 <= p['age'] < RETIRE and
                  re.search(r'carries loads|runs messages|carrying messages|washes linen|washing clothes|sweeps|sweeping|'
                            r'sells from a tray|idling|hauling crates', p['workLabel'])]
        spare_labour[sid] = sorted(labour, key=lambda p: p['id'])[:len(labour) // 2]
        report(f'{sid}: {len(folk)} residents, {len(shops)} shops: {len(closing)} close ({len(shops) - len(closing)} '
               f'stay open); {len(out_help)} of {len(help_)} shop help freed')
    return freed, closed_cells, spare_labour


FIRST_PERSON = [('works the fields', 'work the fields'), ('tends the', 'tend the'), ('keeps', 'keep'),
                ('milks', 'milk'), ('cutting stone', 'cut stone'), ('digs', 'dig'), ('logging', 'fell timber'),
                ('fishes', 'fish'), ('runs', 'run'), ('works at', 'work at'), ('brewer at', 'press the wine at')]
THIRD_PERSON = [('cutting stone', 'cuts stone'), ('logging', 'fells timber'), ('brewer at', 'presses the wine at')]


def reword(person, label, where=None):
    """A resident's new work in their description and greeting (the rest stays theirs)."""
    third, first = label, label
    for a, b in THIRD_PERSON:
        if third.startswith(a):
            third = b + third[len(a):]
    for a, b in FIRST_PERSON:
        if first.startswith(a):
            first = b + first[len(a):]
            break
    d = person['description']
    cut = max(d.rfind(', who '), d.rfind(', '))
    head = d[:cut] if cut > 0 else d.rstrip('.')
    person['description'] = f'{head}, who {where or third}.'
    m = re.match(r'^(.*?[.!?])\s', person['greeting'])
    opening = (m.group(1) + ' ') if m else ''
    if label == 'retired':
        person['greeting'] = f'{opening}I have retired; the shop is shut now.'
    elif label == 'looking for work':
        person['greeting'] = f'{opening}The shop is shut; I am looking for work.'
    else:
        person['greeting'] = f'{opening}I {first} these days.'


# ------------------------------------------------------------------------------------------------- The whole pass

def build(project, report=print):
    """The project with the industry built and the people moved onto it (a copy)."""
    out = {**project, 'cells': [dict(c) for c in project['cells']], 'rooms': list(project['rooms']),
           'links': list(project['links']), 'people': [dict(p) for p in project.get('people', [])]}
    if any(r['id'].startswith(PREFIX) for r in out['rooms']):
        raise ValueError('industry already built (prefix ind_ in use)')
    cells = {c['id']: c for c in out['cells'] if c['z'] == 0}
    centre = {s[0]: (s[3][0] + s[3][2] // 2 + OX, s[3][1] + s[3][3] // 2 + OY) for s in SETTLEMENTS}
    centre['upper_accord'] = (384, 384)
    reserved = {a['id'] for a in out['cells'] + out['rooms']}
    plots = {}
    built = defaultdict(list)
    for sid, wanted in SITES.items():
        rng = random.Random(f'{SEED}:{sid}')
        names = list(NAMES[sid])
        for kind, cell_ids, count in wanted:
            spec = KINDS[kind]
            for _ in range(count):
                n = names.pop(0) if names else f'{sid} {len(built[sid]) + 1}'
                site = None
                for cid in cell_ids:
                    if cid not in cells:
                        continue
                    if cid not in plots:
                        plots[cid] = Plot(out, cells[cid], centre[sid], reserved, sid)
                    plot = plots[cid]
                    plot.region = sid
                    site = place_site(plot, kind, spec, n, STYLE[sid], rng)
                    if site:
                        break
                if not site:
                    report(f'  {sid}: no room for a {kind} ({n})')
                    names.insert(0, n)
                    continue
                site['kind'], site['n'] = kind, n
                built[sid].append(site)
                report(f'  {sid}: {site["name"]} in {site["cell"]}')
    for plot in plots.values():
        if plot.changed:
            plot.write_back()
            out['rooms'] += [{**r, 'worldX': r['worldX'] + plot.cell['x'], 'worldY': r['worldY'] + plot.cell['y']}
                             for r in plot.site.rooms]
            out['links'] += plot.site.links
    # The people.
    freed, closed, spare_labour = rebalance(out, report)
    moved = Counter()
    for sid, sites in built.items():
        pool = sorted(freed.get(sid, []), key=lambda p: (p['role'] != 'merchant', p['id']))
        retired = [p for p in pool if p['age'] >= RETIRE]
        pool = [p for p in pool if p['age'] < RETIRE]
        for p in retired:
            p['role'], p['workLabel'], p['paid'] = 'civilian', 'retired', False
            p['work'] = dict(p['home'])
            reword(p, 'retired', 'has retired from the shop')
            moved['retired'] += 1
        jobs = []
        for s in sites:
            spec = KINDS[s['kind']]
            rec = s['records'][-1] if spec.get('second') else s['records'][0]
            bname = s['records'][0]['name']
            if spec.get('keeper'):
                label = spec['keeper'].format(building=bname, second=rec['name'], n=s['n'])[:40]
                room = rec['rooms'][0]
                spot = room['work'][0] if room['work'] else {'x': 2, 'y': 2}
                jobs.append(('merchant', label, {'cell': room['id'], 'x': spot['x'], 'y': spot['y']}, (6, 18), s))
            for i in range(spec['hands']):
                label = spec['label'].format(site=s['name'], building=bname, n=s['n'])[:40]
                if s['spots']:
                    x, y = s['spots'][i % len(s['spots'])]
                    where = {'cell': s['cell'], 'x': x, 'y': y}
                else:
                    room = s['records'][0]['rooms'][0]
                    spot = room['work'][i % len(room['work'])] if room['work'] else {'x': 2, 'y': 2}
                    where = {'cell': room['id'], 'x': spot['x'], 'y': spot['y']}
                jobs.append(('civilian', label, where, HOURS, s))
        # Keepers first (from the closed shops' keepers), then hands, round the sites so each gets some.
        keepers = [j for j in jobs if j[0] == 'merchant']
        hands = sorted([j for j in jobs if j[0] == 'civilian'], key=lambda j: [x for x in jobs if x[0] == 'civilian'].index(j) % 4)
        merchants = [p for p in pool if p['role'] == 'merchant']
        others = [p for p in pool if p['role'] != 'merchant']
        order = []
        for job in keepers:
            if merchants:
                order.append((merchants.pop(0), job))
            elif others:
                order.append((others.pop(0), job))
        rest = merchants + others
        # Hands still wanted: half the town's labourers (porters, messengers, washers) go out to the sites too.
        short = len(hands) - len(rest)
        if short > 0:
            extra = spare_labour.get(sid, [])[:short]
            rest += extra
            report(f'  {sid}: {len(extra)} of its town labourers go out to the sites too')
        for job in hands:
            if not rest:
                break
            order.append((rest.pop(0), job))
        for p, (role, label, where, hours, s) in order:
            p['role'], p['workLabel'], p['work'] = role, label, where
            p['hours'] = {'start': hours[0], 'end': hours[1]}
            p['paid'] = role != 'merchant'
            p['route'] = ''
            reword(p, label)
            moved[s['kind']] += 1
        if rest:
            report(f'  {sid}: {len(rest)} freed with no site job left; they take a trade of their own')
            for p in rest:
                p['role'], p['workLabel'], p['paid'] = 'civilian', '-', True
                p['work'] = dict(p['home'])
                reword(p, 'looking for work', 'is looking for work')
        report(f'  {sid}: moved {sum(1 for _ in order)} onto its sites; {len(retired)} retired')
    report(f'by kind: {dict(moved)}')
    return out, built, closed


def main(argv=None):
    import map_editor
    import world_db
    import world_store
    from .western import dev_project
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--dry-run', action='store_true', help='Build and check, but do not save to DEV')
    parser.add_argument('--out', type=Path, help='Write the project here as JSON')
    parser.add_argument('--preview', type=Path, help='Write a picture of each changed cell into this folder')
    args = parser.parse_args(argv)
    project, revision = dev_project()
    merged, built, closed = build(project)
    try:
        map_editor.check_project(merged, for_game=False)
        print('Atlas validation: OK')
    except map_editor.ValidationError as error:
        print('Atlas validation failed:', *error.errors[:30], sep='\n  ')
        return 1
    if args.out:
        args.out.write_text(json.dumps(merged), encoding='utf-8')
    if args.preview:
        from .preview import glyphs
        args.preview.mkdir(parents=True, exist_ok=True)
        changed = {s['cell'] for sites in built.values() for s in sites}
        for c in merged['cells']:
            if c['id'] in changed:
                (args.preview / f'{c["id"]}.txt').write_text('\n'.join(c['terrain']), encoding='utf-8')
        print(f'Previews in {args.preview}')
    if args.dry_run:
        print('Dry run: DEV not changed.')
        return 0
    folder = Path(__file__).resolve().parents[2] / 'artifacts/backups'
    folder.mkdir(parents=True, exist_ok=True)
    backup = folder / f'{project["id"]}_dev_r{revision}_before_industry_{time.strftime("%Y%m%d-%H%M%S")}.atlas.json'
    backup.write_text(json.dumps(project, ensure_ascii=False), encoding='utf-8')
    print(f'DEV revision {revision} backed up to {backup}.')
    with world_db.connect('dev', 'editor') as conn:
        new = world_store.save_world(conn, merged, revision)
    print(f'Saved to DEV, revision {new}.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
