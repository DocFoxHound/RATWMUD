"""The six towns of Docs/Design/60, built out and peopled, and Ridgemere's free beds filled.

The realm generator (worldgen.realm) leaves each town a walled box of empty ground. For each one still untouched this
adds: streets and a crossroads with its well, buildings stamped into the ground with their interiors and doors (the
basics of Docs/Design/39 and the town's own trades), fields outside the walls, and the people who live and work there,
with the watch's route. Everything added carries the town's prefix (tn_<town>_); nothing that exists is changed except
the town's own empty ground and the fields outside its gates. A town whose ground has been edited is refused, with the
edits listed. Then more townsfolk move into Ridgemere's free beds (fill_city).

  cd tools && python3 -m worldgen.towns [--dry-run] [--only larkfield,oakhaven] [--preview DIR]

Re-running skips places already built (their prefix is in use). worldgen.realm calls build_all() when it makes the world.
"""
from __future__ import annotations

import argparse
import math
import random
from pathlib import Path

import numpy as np

import terrain_catalog as catalog
from . import field
from .buildings import TRADES, WORKS, WORKS_TRADE, Building, hall, house, keeper_label, keeper_word, shop_with_flat, tavern, tenement, works
from .canvas import DEFAULT_HEIGHT, Canvas, code, reachable
from .cities import fp, grow, shrink
from .citizens import Folk
from . import residents as R
from .site import Lots, Site
from .realm import OX, OY, SETTLEMENTS

SEED = 3030
CELL = 256
WALLS = set('#|H')
# Ground a settlement may hold and still count as unbuilt: the generator's floors, grass, the odd rock and plant.
NATURAL = {t['code'] for t in catalog.TILES if t['category'] in ('ground', 'nature')} | set('G8d_')   # (and its roads)
OPEN_GROUND = set(".,;\"!35-0rd_f")          # Outside the walls: ground a field or farmstead may take.

HOUSE_PROSE = [
    'A low hearth, a scrubbed table and sleeping places along the walls; it smells of smoke and bread.',
    'Two rooms in one: the family\'s beds at the back, the table and the fire by the door.',
    'Thick walls against the weather, a hearth that is never let out, and a jumble of tools by the door.',
    'Herbs drying from the beams, a chest of good linen, and the marks of children\'s heights on the doorpost.',
    'Plain and close: a table, a fire, and beds pushed together for warmth in winter.',
]
TENEMENT_PROSE = 'Rented sleeping places along every wall, one shared hearth and a table worn smooth by many elbows.'


# Each town (Docs/Design/60: generic for now): its prefix, how it is built, how many live there, what it is known for,
# its buildings (beyond houses: every town has the basics of Docs/Design/39, a general store, a bakery or provisioner,
# a smithy and an inn), its workshops, the work outside its walls, and its names and stories.
TOWNS = {
    'frosthollow': dict(
        prefix='tn_fh_', style='city', street='_', square='f', people=34, near=False, species=[3, 0, 6, 1, 0],
        about='a mining town in a high snowy valley among the Whitefangs',
        buildings=[
            ('inn', 'The Frozen Lantern', 'A low stone inn with a fire in every room and snow piled to the shutters.',
             'Small cold rooms under the roof, each with a pile of furs.'),
            ('shop', 'general', 'Frosthollow Stores', 'Lamp oil, rope, salt and everything else that comes up the pass.'),
            ('shop', 'provisioner', 'The Pass Provisioner', 'Hard bread, dried meat and oats for the miners.'),
            ('shop', 'smith', 'The Pick and Anvil', 'Picks, wedges and lamp hooks for the mines.'),
            ('hall', 'chapel', 'The Snow Chapel', 'A small chapel where the miners\' lamps are blessed each spring.', 16, 11),
            ('hall', 'guard', 'The Watch Post', 'Two bunks, a brazier and a view down the pass.', 14, 9),
        ],
        industry=[('tenement', 6, 'The Miners\' Bunkhouse'),
                  ('works', 'foundry', 'The Frosthollow Smelter', 'Ore from the mines smelted into rough bars for Ridgemere.')],
        houses=7, tenements=1,
        outside=[('trapper', 'traps in the snowy woods', 2, 'gate'), ('woodcutter', 'cuts wood for the fires', 2, 'gate')],
        farms=0, guards=2,
        heads=['Frost', 'Snow', 'Rime', 'Ice', 'Stone', 'Crag', 'Pine', 'Cold'],
        tails=['hollow', 'fell', 'pick', 'vein', 'drift', 'hearth', 'peak', 'well'],
        quirks=['can smell snow a day before it comes', 'keeps a lump of ore for luck'],
        hooks=['lost a brother in a mine collapse', 'means to go down to Ridgemere and never come back up']),
    'coldwater': dict(
        prefix='tn_cw_', style='city', street='_', square='f', people=32, near=False, species=[4, 0, 3, 2, 0],
        about='a lake town on Glasswater, where the High Road climbs toward the mountains',
        buildings=[
            ('inn', 'The Glass Lantern', 'An inn on the lake shore, full of carters bound for the mountains.',
             'Rooms with a view of the lake, if the mist lifts.'),
            ('shop', 'general', 'Coldwater Sundries', 'A little of everything for the road up to the passes.'),
            ('shop', 'baker', 'The Lake Oven', 'Rye bread and fish pies.'),
            ('shop', 'smith', 'The Coldwater Forge', 'Horseshoes and wagon iron for the High Road.'),
            ('shop', 'fishmonger', 'Glasswater Fish', 'Lake trout on cold slabs.'),
            ('hall', 'guard', 'The Watch Post', 'Where the High Road is watched.', 14, 9),
        ],
        industry=[('works', 'stables', 'The Coldwater Stables', 'Horses for the climb to the passes.', 2)],
        houses=7, tenements=1,
        outside=[('fisher', 'fishes the lake', 3, 'shore'), ('farmer', 'farms the lake fields', 2, 'field')],
        farms=1, guards=2,
        heads=['Cold', 'Lake', 'Glass', 'Mere', 'Reed', 'Grey', 'Still', 'Mist'],
        tails=['water', 'shore', 'wick', 'ford', 'well', 'by', 'mere', 'ton'],
        quirks=['reads the weather from the lake', 'swims in the lake even in autumn'],
        hooks=['saw something big move under the ice one winter', 'carts goods to Ridgemere and resents every mile']),
    'oakhaven': dict(
        prefix='tn_oh_', style='city', street='_', square='f', people=34, near=False, species=[3, 2, 1, 3, 1],
        about='a forest town in the Warmwood, among oaks and beeches',
        buildings=[
            ('inn', 'The Acorn Inn', 'A timbered inn under great oaks, warm and green-lit.',
             'Rooms under the eaves with oak shutters.'),
            ('shop', 'general', 'Oakhaven Goods', 'Rope, lamp oil, salt and gossip.'),
            ('shop', 'baker', 'The Hazel Loaf', 'Nut bread and honey cakes.'),
            ('shop', 'smith', 'The Greenwood Smithy', 'Axes, saws and wedges for the woodcutters.'),
            ('shop', 'carpenter', 'Oakhaven Joiners', 'Oak chests, tables and beds that last a lifetime.'),
            ('hall', 'guard', 'The Watch Post', 'A post at the crossroads of the forest ways.', 14, 9),
        ],
        industry=[('works', 'sawmill', 'The Oakhaven Sawmill', 'Oak and beech planks for the south.', 3)],
        houses=7, tenements=1,
        outside=[('woodcutter', 'cuts wood in the Warmwood', 3, 'gate'), ('forager', 'gathers herbs in the woods', 1, 'gate')],
        farms=1, guards=2,
        heads=['Oak', 'Beech', 'Hazel', 'Ash', 'Moss', 'Fern', 'Green', 'Acorn'],
        tails=['haven', 'wood', 'leaf', 'holt', 'field', 'bough', 'dell', 'shaw'],
        quirks=['knows every tree by its shape', 'hums with the birds'],
        hooks=['once got lost in the deep wood for three days', 'carves birds and gives them away']),
    'mirebridge': dict(
        prefix='tn_mb_', style='city', street='_', square='f', people=30, near=False, species=[2, 2, 1, 3, 2],
        about='a fen town by the Long River, built where the old bridge crosses the marsh',
        buildings=[
            ('inn', 'The Eel and Lantern', 'A damp inn on stilts at the fen\'s edge; the eel stew is famous.',
             'Small rooms where the mist comes under the doors.'),
            ('shop', 'general', 'Mirebridge Store', 'Rope, lamp oil and boots for the bog.'),
            ('shop', 'provisioner', 'Fen Provisions', 'Smoked eel, oatcakes and salt.'),
            ('shop', 'smith', 'The Bridge Smithy', 'Eel spears, boat nails and hinges.'),
            ('shop', 'herbalist', 'Marsh Remedies', 'Fen herbs for fen fevers.'),
            ('hall', 'guard', 'The Watch Post', 'Where the bridge is watched.', 14, 9),
        ],
        industry=[('tenement', 6, 'The Reedcutters\' House')],
        houses=6, tenements=1,
        outside=[('eel-trapper', 'traps eels in the fen', 3, 'shore'), ('reedcutter', 'cuts reeds in the marsh', 2, 'gate')],
        farms=1, guards=2,
        heads=['Mire', 'Fen', 'Reed', 'Eel', 'Bog', 'Marsh', 'Sedge', 'Pike'],
        tails=['bridge', 'water', 'more', 'bank', 'ford', 'ditch', 'wade', 'holm'],
        quirks=['never minds wet feet', 'can find a path across any bog'],
        hooks=['swears the marsh lights are the drowned', 'is the best eel trapper on the river, and says so']),
    'larkfield': dict(
        prefix='tn_lf_', style='city', street='_', square='f', people=36, near=False, species=[3, 2, 1, 3, 1],
        about='a market town of the Larkdowns, among rolling hills and open fields',
        buildings=[
            ('inn', 'The Singing Lark', 'A bright inn full of farmers and drovers on market evenings.',
             'Clean rooms smelling of hay and lavender.'),
            ('shop', 'general', 'Larkfield General', 'Everything a farm needs and some things it doesn\'t.'),
            ('shop', 'baker', 'The Wheatsheaf Bakery', 'White bread from the downs\' own wheat.'),
            ('shop', 'smith', 'The Ploughshare Forge', 'Ploughs, scythes and horseshoes.'),
            ('shop', 'butcher', 'Downs Butchers', 'Mutton and beef from the downs.'),
            ('hall', 'guard', 'The Watch Post', 'A post on the Downs Road.', 14, 9),
        ],
        industry=[('works', 'mill', 'The Larkfield Water Mill', 'Wheat from the downs ground into flour.', 2)],
        houses=8, tenements=1,
        outside=[('farmer', 'farms the downs', 3, 'field'), ('shepherd', 'keeps sheep on the downs', 2, 'gate')],
        farms=2, guards=2,
        heads=['Lark', 'Down', 'Wheat', 'Barley', 'Hay', 'Sun', 'Hedge', 'Chalk'],
        tails=['field', 'ley', 'combe', 'well', 'ridge', 'ton', 'ham', 'mead'],
        quirks=['always knows the price of wheat', 'whistles back at the larks'],
        hooks=['dreams of a farm of their own', 'has a sweetheart in Southmeadow']),
    'southmeadow': dict(
        prefix='tn_sm_', style='city', street='_', square='f', people=34, near=False, species=[3, 2, 1, 3, 1],
        about='a farming town in the far south, among grain fields and orchards',
        buildings=[
            ('inn', 'The Apple Tree', 'A cider inn, its yard shaded by an old apple tree.',
             'Rooms that smell of apples in autumn.'),
            ('shop', 'general', 'Southmeadow Stores', 'Seed, twine, salt and lamp oil.'),
            ('shop', 'baker', 'The Harvest Loaf', 'Bread, apple pies and harvest cakes.'),
            ('shop', 'smith', 'The Southmeadow Smithy', 'Sickles, ploughshares and cart iron.'),
            ('shop', 'brewer', 'The Cider House', 'Cider by the jug and by the barrel.'),
            ('hall', 'guard', 'The Watch Post', 'A post at the town gate.', 14, 9),
        ],
        industry=[('works', 'brewery', 'The Southmeadow Press', 'Apples pressed into cider every autumn.', 2)],
        houses=8, tenements=1,
        outside=[('farmer', 'farms the south fields', 3, 'field'), ('orchard hand', 'picks apples in the orchards', 2,
                                                                     'gate')],
        farms=2, guards=2,
        heads=['Apple', 'Meadow', 'Harvest', 'Sheaf', 'Blossom', 'Corn', 'Plum', 'Honey'],
        tails=['meadow', 'field', 'garth', 'ley', 'well', 'ton', 'by', 'croft'],
        quirks=['can tell an apple by its smell', 'never misses a harvest dance'],
        hooks=['wants to plant the biggest orchard in the south', 'once walked all the way to Ridgemere and came back']),
}


# ---------------------------------------------------------------------------------------------------------------
# The ground
# ---------------------------------------------------------------------------------------------------------------

class Place:
    """One settlement on a canvas of its cell, with the cell's tiles read from DEV."""

    def __init__(self, project, sid, name, kind, box):
        self.sid, self.name, self.kind = sid, name, kind
        x, y, w, h = box
        self.wx, self.wy, self.w, self.h = x + OX, y + OY, w, h
        self.cell = next(c for c in project['cells'] if c['z'] == 0 and c['x'] <= self.wx < c['x'] + c['width']
                         and c['y'] <= self.wy < c['y'] + c['height'])
        c = self.cell
        self.canvas = Canvas(c['width'], c['height'])
        for yy, row in enumerate(c['terrain']):
            self.canvas.codes[yy] = np.frombuffer(row.encode('ascii'), dtype=np.uint8)
        self.canvas.heights[:] = DEFAULT_HEIGHT[self.canvas.codes]
        for key, value in c.get('heights', {}).items():
            hx, hy = map(int, key.split(','))
            self.canvas.heights[hy, hx] = value
        self.before = self.canvas.codes.copy()
        # The box in the cell's own coordinates, and what lies inside the wall.
        self.x0, self.y0 = self.wx - c['x'], self.wy - c['y']
        codes = self.canvas.codes
        box_mask = np.zeros(codes.shape, dtype=bool)
        box_mask[max(0, self.y0 - 2):self.y0 + h + 2, max(0, self.x0 - 2):self.x0 + w + 2] = True
        walls = np.isin(codes, [ord(ch) for ch in WALLS])
        self.walls = walls & box_mask
        # Dry land within the wall: a shore town's wall stops at the water, so the water bounds it there.
        water = np.isin(codes, [ord(ch) for ch in 'W~w'])
        exact = np.zeros(codes.shape, dtype=bool)
        exact[self.y0:self.y0 + h, self.x0:self.x0 + w] = True
        inside = exact & ~walls & ~water & (codes != ord('G'))
        # The largest piece of it: a shore town's box also holds strips of beach outside its wall.
        best, left = None, inside.copy()
        while left.any():
            ys, xs = np.nonzero(left)
            piece = self._fill(left, (int(xs[0]), int(ys[0])))
            left &= ~piece
            if best is None or piece.sum() > best.sum():
                best = piece
        self.inside = best
        self.gates = self._gates(box_mask)

    @staticmethod
    def _fill(mask, seed):
        """Tiles of `mask` connected to `seed` (or to the nearest tile of it)."""
        if not mask[seed[1], seed[0]]:
            ys, xs = np.nonzero(mask)
            i = int(np.argmin((xs - seed[0]) ** 2 + (ys - seed[1]) ** 2))
            seed = (int(xs[i]), int(ys[i]))
        seen = np.zeros_like(mask)
        seen[seed[1], seed[0]] = True
        frontier = [seed]
        H, W = mask.shape
        while frontier:
            nxt = []
            for x, y in frontier:
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < W and 0 <= ny < H and mask[ny, nx] and not seen[ny, nx]:
                        seen[ny, nx] = True
                        nxt.append((nx, ny))
            frontier = nxt
        return seen

    def _gates(self, box_mask):
        """Each gate: its middle, and the tile just inside it."""
        g = (self.canvas.codes == ord('G')) & box_mask
        groups, seen = [], np.zeros_like(g)
        for y, x in zip(*np.nonzero(g)):
            if seen[y, x]:
                continue
            group = self._fill(g & ~seen, (int(x), int(y)))
            seen |= group
            ys, xs = np.nonzero(group)
            mid = (int(round(xs.mean())), int(round(ys.mean())))
            ring = grow(group, 2) & self.inside & ~group
            iy, ix = np.nonzero(ring)
            if not len(ix):
                continue
            j = int(np.argmin((ix - mid[0]) ** 2 + (iy - mid[1]) ** 2))
            out = grow(group, 2) & ~self.inside & ~group & ~self.walls
            oy, ox = np.nonzero(out)
            k = int(np.argmin((ox - mid[0]) ** 2 + (oy - mid[1]) ** 2)) if len(ox) else None
            groups.append({'mid': mid, 'in': (int(ix[j]), int(iy[j])),
                           'out': (int(ox[k]), int(oy[k])) if k is not None else mid})
        return groups

    def edits(self):
        """Tiles inside the wall that are not natural ground: what Atlas (or anyone) has built there."""
        ys, xs = np.nonzero(self.inside)
        return [(int(x), int(y), chr(self.canvas.codes[y, x])) for y, x in zip(ys, xs)
                if chr(self.canvas.codes[y, x]) not in NATURAL]

    def world(self, x, y):
        return self.cell['x'] + x, self.cell['y'] + y

    def write_back(self):
        """The cell's terrain rows and height overrides, from the canvas."""
        rows, overrides = self.canvas.cell(0, 0, self.cell['width'], self.cell['height'])
        self.cell['terrain'] = rows
        self.cell['heights'] = overrides


def lay_streets(place: Place, cfg, rng):
    """A square in the middle with a well, a street from every gate to it, and lanes between; returns (street, square)."""
    c, inside = place.canvas, place.inside
    ys, xs = np.nonzero(inside)
    cx, cy = int(xs.mean()), int(ys.mean())
    if not inside[cy, cx]:
        i = int(np.argmin((xs - cx) ** 2 + (ys - cy) ** 2))
        cx, cy = int(xs[i]), int(ys[i])
    w, h = xs.max() - xs.min(), ys.max() - ys.min()
    # A fortress's parade ground; a town's crossroads with its well (towns have no market square: Docs/Design/39).
    sw, sh = (max(8, int(w * .2)), max(6, int(h * .18))) if cfg.get('fortress') else (7, 5)
    square = np.zeros(inside.shape, dtype=bool)
    square[cy - sh // 2:cy + sh // 2 + 1, cx - sw // 2:cx + sw // 2 + 1] = True
    square &= inside
    street = np.zeros(inside.shape, dtype=bool)
    width = 3 if not cfg.get('fortress') else 4
    # Two streets across the whole place through the square, a street from each gate to them, and a road just inside
    # the wall: four quarters, their buildings facing outward onto the wall road and inward onto the cross streets.
    street |= c.line_mask([(xs.min(), cy + .5), (xs.max() + 1, cy + .5)], width)
    street |= c.line_mask([(cx + .5, ys.min()), (cx + .5, ys.max() + 1)], width)
    for g in place.gates:
        gx, gy = g['in']
        street |= c.line_mask([(gx + .5, gy + .5), (gx + .5, cy + .5)], width)
        street |= c.line_mask([(gx + .5, gy + .5), (cx + .5, gy + .5)], width)
    # A middle lane in quarters deep enough for three rows of houses.
    for lx in (int((xs.min() + cx) / 2), int((cx + xs.max()) / 2)):
        if (cx - xs.min()) / 2 > 26:
            street |= c.line_mask([(lx + .5, ys.min()), (lx + .5, ys.max() + 1)], 2)
    for ly in (int((ys.min() + cy) / 2), int((cy + ys.max()) / 2)):
        if (cy - ys.min()) / 2 > 24:
            street |= c.line_mask([(xs.min(), ly + .5), (xs.max() + 1, ly + .5)], 2)
    keep_off = ~shrink(inside, 2 if cfg.get('fortress') else 1)
    street = (street | (inside & keep_off)) & inside
    return street, square, (cx, cy)


def paint_ground(place: Place, cfg, street, square, centre, rng):
    c = place.canvas
    level = float(np.median(c.heights[place.inside]))
    if cfg['street']:
        c.paint(street & ~square, cfg['street'], level)
    if cfg['square']:
        c.paint(square, cfg['square'], level)
        cx, cy = centre
        c.stamp(cx, cy, ['U'], level)
    return level


# ---------------------------------------------------------------------------------------------------------------
# Buildings
# ---------------------------------------------------------------------------------------------------------------

def make(cfg, entry, rng, family_name=None):
    style = cfg['style']
    kind = entry[0]
    if kind == 'shop':
        # A shop, and the flat above it where its keeper's family lives (Docs/Design/39).
        _, trade, name, text = entry
        rooms, stairs, (w, h) = shop_with_flat(rng, style, trade, name, text)
        return Building('shop', name, style, fp(w, h), rooms, stairs, district=cfg['prefix'], trade=trade)
    if kind == 'works':
        _, works_kind, name, text = entry[:4]
        hands = entry[4] if len(entry) > 4 else 3
        # Inside, the cinder floor of a workshop (Ser Ferro's whitewash and flagstones in its own towns).
        p = works(rng, style if style == 'serferro' else 'works', works_kind, hands)
        return Building('works', name, style, fp(p.w, p.h), [p.room('', name, f'{name}: {WORKS[works_kind][1]}. {text}')],
                        district=cfg['prefix'], trade=works_kind)
    if kind in ('inn', 'tavern'):
        inn = kind == 'inn'
        name, text = entry[1], entry[2]
        plans, stairs = tavern(rng, style, inn)
        rooms = [plans[0].room('', name, text)]
        pairs = []
        if inn:
            rooms.append(plans[1].room('up', f'{name}, upstairs', entry[3], z=1))
            _, sx, sy = stairs[0]
            pairs.append((('', sx, sy), ('up', sx, sy)))
        return Building(kind, name, style, fp(plans[0].w, plans[0].h), rooms, pairs, cfg['prefix'])
    if kind == 'hall':
        _, hall_kind, name, text, w, h = entry
        p = hall(rng, style, w, h, hall_kind)
        return Building(hall_kind, name, style, fp(w, h), [p.room('', name, text)], district=cfg['prefix'])
    if kind == 'house':
        people = entry[1]
        p = house(rng, style, people)
        name = f'{family_name} House'
        return Building('house', name, style, fp(p.w, p.h), [p.room('', name, rng.choice(HOUSE_PROSE))], district=cfg['prefix'])
    if kind == 'tenement':
        p = tenement(rng, style, entry[1])
        name = entry[2]
        return Building('tenement', name, style, fp(p.w, p.h), [p.room('', name, TENEMENT_PROSE)], district=cfg['prefix'])
    raise ValueError(kind)


class TownSite(Site):
    def __init__(self, place: Place, prefix, reserved):
        c = place.cell
        super().__init__(place.canvas, [(c['id'], 0, 0, c['width'], c['height'])])
        self.prefix = prefix
        self.ids |= set(reserved)

    def unique(self, name):
        return super().unique(f'{self.prefix}{name}')


def place_buildings(place: Place, site: TownSite, cfg, street, square, centre, rng, families):
    """Every building of the place on a lot facing a street: the important ones near the square, houses where they fit."""
    open_ground = place.inside & ~street & ~square
    codes = place.canvas.codes
    open_ground &= ~np.isin(codes, [ord(ch) for ch in 'W~w'])
    place.left_out = []                  # Buildings with no room (a lakeside town's pier may take them: pier_quarter).
    lots = Lots(open_ground, street | square, gap=1)
    wanted = [make(cfg, e, rng) for e in cfg['buildings']]
    # Shopkeepers' families live over their shops (Docs/Design/39), so a town needs a house fewer for every three shops.
    shops = sum(1 for e in cfg['buildings'] + cfg.get('industry', []) if e[0] == 'shop')
    for i in range(max(min(3, cfg['houses']), cfg['houses'] - shops // 3)):
        wanted.append(make(cfg, ('house', rng.choice([3, 3, 4, 4, 5])), rng, families[i]))
    for i in range(cfg['tenements']):
        wanted.append(make(cfg, ('tenement', 6, f'The {["Long", "Low", "Old", "Back", "Lane"][i % 5]} House'), rng))
    placed, missing = [], []
    for b in wanted:
        fw, fh = b.footprint
        near = centre if b.kind != 'house' else (centre[0] + rng.randint(-30, 30), centre[1] + rng.randint(-24, 24))
        spot = next(((x0, y0, facing, w, h) for _, x0, y0, facing, w, h in lots.candidates(fw, fh, near, limit=1500)
                     if lots.clear(x0, y0, w, h)), None)
        if spot is None:
            missing.append(b.name)
            place.left_out.append(b)
            continue
        x0, y0, facing, w, h = spot
        bid, outside = site.place(b, x0, y0, facing, place.sid, open_door=b.kind != 'house')
        lots.take(x0, y0, w, h)
        site.manifest[-1]['town'] = place.sid
        placed.append((bid, b, outside))
    place.lots = lots                     # What is left, for doc 35's industry (place_industry).
    return placed, missing


def farms(place: Place, cfg, site: TownSite, rng, families):
    """Fields outside the gates (crops, or salt pans on the coast), each with a farmstead; returns the field tiles."""
    c, codes = place.canvas, place.canvas.codes
    fields = []
    ch = cfg.get('field', '4')
    # Only ground a wolf can walk to from the town: a farm behind a cliff or across the water is no farm.
    walk = reachable(codes, c.heights, place.gates[0]['in'])
    solid = np.isin(codes, [ord(t['code']) for t in catalog.TILES if t.get('solid')])
    wet = np.isin(codes, [ord(x) for x in 'W~w'])
    built = ~np.isin(codes, [ord(t['code']) for t in catalog.TILES if t['category'] in ('ground', 'nature')])
    roads = np.isin(place.before, [ord(x) for x in 'd_8+G'])
    # Ground a field can take: walkable, dry, not a road or anything built (scattered shrubs and flowers are ploughed).
    natural_open = walk & ~solid & ~wet & ~built & ~roads
    clearable = ~wet & ~built & ~roads          # A few trees may be felled for a field.
    taken = grow(place.inside | place.walls, 6)
    H, W = codes.shape
    for n in range(cfg.get('farms', 0)):
        best = None
        for g in place.gates:
            ox, oy = g['out']
            for _ in range(600):
                fw, fh = rng.randint(18, 26), rng.randint(10, 14)
                x0 = ox + rng.randint(-60, 40)
                y0 = oy + rng.randint(-50, 40)
                if x0 < 4 or y0 < 4 or x0 + fw + 16 >= W or y0 + fh + 4 >= H:
                    continue
                area = (slice(y0 - 2, y0 + fh + 2), slice(x0 - 2, x0 + fw + 16))
                if not clearable[area].all() or taken[area].any() or natural_open[area].mean() < .85:
                    continue
                d = math.hypot(x0 - ox, y0 - oy)
                if best is None or d < best[0]:
                    best = (d, x0, y0, fw, fh)
        if best is None:
            break
        _, x0, y0, fw, fh = best
        saved = (c.codes.copy(), c.heights.copy(), len(site.rooms), len(site.links), len(site.manifest))
        level = float(np.median(c.heights[y0:y0 + fh, x0:x0 + fw]))
        c.rect(x0, y0, fw, fh, ch, level)
        c.outline(x0 - 1, y0 - 1, fw + 2, fh + 2, '|', level)
        # A gap in the fence on the side nearest the farmstead, and the farmstead beside the field.
        c.codes[y0 + fh // 2, x0 + fw] = code(',')
        b = make(cfg, ('house', 4), rng, families[cfg['houses'] + n])
        b.name = b.name.replace(' House', ' Farm')
        b.rooms[0].name = b.name
        hx, hy = x0 + fw + 3, y0 + fh // 2 - b.footprint[1] // 2
        bid, outside = site.place(b, hx, hy, 'W', place.sid, open_door=False)
        site.manifest[-1]['town'] = place.sid
        site.manifest[-1]['farm'] = True
        taken[y0 - 8:y0 + fh + 8, x0 - 8:x0 + fw + 24] = True
        # A farm whose door can't be walked to from the town is taken away again.
        door = site.manifest[-1]['door']
        if not reachable(c.codes, c.heights, place.gates[0]['in'])[door['y'], door['x']]:
            c.codes[:], c.heights[:] = saved[0], saved[1]
            del site.rooms[saved[2]:], site.links[saved[3]:], site.manifest[saved[4]:]
            continue
        fields.append([(x, y) for y in range(y0 + 1, y0 + fh - 1, 3) for x in range(x0 + 1, x0 + fw - 1, 3)])
    return fields


def pier_attempt(place: Place, site: TownSite):
    """Places a building, and takes it away again if any door can then no longer be walked to from the gate."""
    c = place.canvas

    def attempt(b, x0, y0, facing):
        saved = (c.codes.copy(), c.heights.copy(), len(site.rooms), len(site.links), len(site.manifest))
        site.place(b, x0, y0, facing, place.sid, open_door=b.kind != 'house')
        site.manifest[-1]['town'] = place.sid
        walk = reachable(c.codes, c.heights, place.gates[0]['in'])
        if all(walk[m['door']['y'], m['door']['x']] for m in site.manifest):
            return True
        c.codes[:], c.heights[:] = saved[0], saved[1]
        del site.rooms[saved[2]:], site.links[saved[3]:], site.manifest[saved[4]:]
        return False
    return attempt


def place_industry(place: Place, site: TownSite, cfg, rng):
    """Doc 35's businesses, built once everything that was here before is in place (so it all stays where it was):
    on the lots left inside the walls, or else outside near a gate on open ground with the door towards the gate (as
    mills, stables and smokehouses often were). A building that would cut a door off from the gate is taken away
    again. Returns the names of any that found no place."""
    c = place.canvas
    start = place.gates[0]['in']

    def doors_reachable():
        walk = reachable(c.codes, c.heights, start)
        return all(walk[m['door']['y'], m['door']['x']] for m in site.manifest)

    def attempt(b, x0, y0, facing):
        saved = (c.codes.copy(), c.heights.copy(), len(site.rooms), len(site.links), len(site.manifest))
        site.place(b, x0, y0, facing, place.sid, open_door=True)
        site.manifest[-1]['town'] = place.sid
        if doors_reachable():
            return True
        c.codes[:], c.heights[:] = saved[0], saved[1]
        del site.rooms[saved[2]:], site.links[saved[3]:], site.manifest[saved[4]:]
        return False

    outside = []
    lots = place.lots
    centre = place.square_centre
    for e in cfg['industry']:
        b = make(cfg, e, rng)
        b.fresh = True
        fw, fh = b.footprint
        for _, x0, y0, facing, w, h in lots.candidates(fw, fh, centre, limit=1500):
            if lots.clear(x0, y0, w, h) and attempt(b, x0, y0, facing):
                lots.take(x0, y0, w, h)
                break
        else:
            if b.kind != 'tenement':
                outside.append(b)
    place.outside_wanted = outside
    return outside_industry(place, site, rng, attempt)


def outside_industry(place: Place, site: TownSite, rng, attempt):
    """Doc 35's workshops with no room inside the walls, built outside near a gate."""
    c, codes = place.canvas, place.canvas.codes
    walk = reachable(codes, c.heights, place.gates[0]['in'])
    solid = np.isin(codes, [ord(t['code']) for t in catalog.TILES if t.get('solid')])
    wet = np.isin(codes, [ord(x) for x in 'W~w'])
    roads = np.isin(place.before, [ord(x) for x in 'd_8+G'])
    open_ground = walk & ~solid & ~wet & ~roads & (codes == place.before)
    taken = grow(place.inside | place.walls, 4) | grow(codes != place.before, 3)
    H, W = codes.shape
    missing = []
    for b in place.outside_wanted:
        best = None
        for g in place.gates:
            ox, oy = g['out']
            for _ in range(500):
                facing = rng.choice('NSEW')
                fw, fh = b.footprint if facing in 'NS' else b.footprint[::-1]
                x0, y0 = ox + rng.randint(-40, 30), oy + rng.randint(-40, 30)
                if x0 < 4 or y0 < 4 or x0 + fw + 4 >= W or y0 + fh + 4 >= H:
                    continue
                area = (slice(y0 - 2, y0 + fh + 2), slice(x0 - 2, x0 + fw + 2))
                if taken[area].any() or open_ground[area].mean() < .9:
                    continue
                dx, dy = ox - (x0 + fw / 2), oy - (y0 + fh / 2)        # The door faces the gate.
                if facing != (('E' if dx > 0 else 'W') if abs(dx) > abs(dy) else ('S' if dy > 0 else 'N')):
                    continue
                d = math.hypot(dx, dy)
                if best is None or d < best[0]:
                    best = (d, x0, y0, facing, fw, fh)
        if best is None:
            missing.append(b.name)
            place.left_out.append(b)
            continue
        _, x0, y0, facing, fw, fh = best
        if not attempt(b, x0, y0, facing):
            missing.append(b.name)
            place.left_out.append(b)
            continue
        taken[y0 - 6:y0 + fh + 6, x0 - 6:x0 + fw + 6] = True
    return missing


def pier_quarter(place: Place, site: TownSite, cfg, rng, attempt):
    """A town hemmed in by water builds out onto it (Docs/Design/39): a timber deck off its shore, walled on the water's
    sides and level with the town, a lane down the middle through a gap in the town wall, and on it whatever found no
    room in the town. Returns the names of any still left out."""
    if not getattr(place, 'left_out', None):
        return []
    c, codes = place.canvas, place.canvas.codes
    H, W = codes.shape
    water = np.isin(codes, [ord(x) for x in 'W~w'])
    level = float(np.median(c.heights[place.inside]))
    wall_ch = max(WALLS, key=lambda ch: int((codes[place.walls] == ord(ch)).sum()))
    built = ~np.isin(codes, [ord(t['code']) for t in catalog.TILES if t['category'] in ('ground', 'nature')])
    # The lake's edge just beyond the town (past its wall, and any strip of beach).
    shore = grow(place.inside | place.walls, 6) & ~place.inside & ~place.walls & water
    ys, xs = np.nonzero(shore)
    centre = place.square_centre
    order = np.argsort((xs - centre[0]) ** 2 + (ys - centre[1]) ** 2)
    need = sum(b.footprint[0] * b.footprint[1] for b in place.left_out)
    across, out = max(30, int(math.sqrt(need * 2.2)) + 10), 26
    for i in order[::7]:
        sx, sy = int(xs[i]), int(ys[i])
        win = water[max(0, sy - 8):sy + 9, max(0, sx - 8):sx + 9]
        wy, wx = np.argwhere(win).mean(axis=0) - 8 if win.any() else (0, 0)
        side = ('E' if wx > 0 else 'W') if abs(wx) >= abs(wy) else ('S' if wy > 0 else 'N')
        if side in 'EW':
            x0 = sx if side == 'E' else sx - out + 1
            y0, w, h = sy - across // 2, out, across
        else:
            y0 = sy if side == 'S' else sy - out + 1
            x0, w, h = sx - across // 2, across, out
        if x0 < 4 or y0 < 4 or x0 + w + 4 >= W or y0 + h + 4 >= H:
            continue
        deck = np.zeros(codes.shape, dtype=bool)
        deck[y0:y0 + h, x0:x0 + w] = True
        deck &= ~place.inside & ~place.walls
        # Over open water: not across a road, a causeway or anything built, and mostly lake.
        if (deck & built & ~water).any() or water[deck].mean() < .85 or (grow(deck, 3) & np.isin(codes, [ord('8')]) & ~deck).any():
            continue
        saved = (c.codes.copy(), c.heights.copy(), len(site.rooms), len(site.links), len(site.manifest))
        c.paint(deck, '8', level)
        rim = deck & ~shrink(deck, 1) & ~grow(place.inside | place.walls, 1)
        c.paint(rim, wall_ch, level)
        # The lane: from just inside the town, through its wall, to the far end of the deck.
        dx, dy = {'E': (1, 0), 'W': (-1, 0), 'S': (0, 1), 'N': (0, -1)}[side]
        back = next((k for k in range(1, 14) if place.inside[sy - dy * k, sx - dx * k]), None)
        if back is None:
            continue                                                # No way into the town from here.
        start = (sx - dx * (back + 1) + .5, sy - dy * (back + 1) + .5)
        end = (sx + dx * (out - 2) + .5, sy + dy * (out - 2) + .5)
        line = c.line_mask([start, end], 3)
        lane = line & (deck | place.inside | place.walls | (grow(deck, 14) & ~built)) & ~(rim & ~grow(place.inside | place.walls, 14) &
                                                                                         ~line)
        c.paint(lane, cfg['street'] or '_', level)          # Through the town's wall, and the deck's own on the landward side.
        floor = deck & ~rim & ~lane
        lots = Lots(floor, lane, gap=1)
        left = []
        for b in place.left_out:
            fw, fh = b.footprint
            for _, bx, by, facing, bw, bh in lots.candidates(fw, fh, (sx, sy), limit=800):
                if lots.clear(bx, by, bw, bh) and attempt(b, bx, by, facing):
                    lots.take(bx, by, bw, bh)
                    break
            else:
                left.append(b.name)
        if len(left) == len(place.left_out):
            c.codes[:], c.heights[:] = saved[0], saved[1]           # Nothing fitted here after all: undo the deck.
            del site.rooms[saved[2]:], site.links[saved[3]:], site.manifest[saved[4]:]
            continue
        place.inside = place.inside | (deck & ~rim)
        place.left_out = [b for b in place.left_out if b.name in left]
        return left
    return [b.name for b in place.left_out]


def shore_spots(place: Place, rng, n=12):
    """Walkable tiles by the water near the place (for fishers and the like)."""
    codes = place.canvas.codes
    water = np.isin(codes, [ord(x) for x in 'W~w'])
    dry = ~np.isin(codes, [ord(t['code']) for t in catalog.TILES if t.get('solid')]) & ~water
    edge = dry & grow(water, 1) & ~place.inside & ~grow(place.walls, 1)
    ys, xs = np.nonzero(edge)
    if not len(xs):
        return []
    cx, cy = place.x0 + place.w / 2, place.y0 + place.h / 2
    order = np.argsort((xs - cx) ** 2 + (ys - cy) ** 2)[:400]
    picks = [(int(xs[i]), int(ys[i])) for i in order]
    rng.shuffle(picks)
    return picks[:n]


def ruin(place: Place, rng):
    """The Ghost Town: two squatters' shacks patched together from the ruins, and nothing else rebuilt."""
    return


# ---------------------------------------------------------------------------------------------------------------
# People
# ---------------------------------------------------------------------------------------------------------------

def labels_for(record):
    """What a building's keeper is called, and what they do (workLabel, at most 40 characters)."""
    kind, name = record['kind'], record['name']
    if kind == 'shop':
        return keeper_word(record['trade']), keeper_label(record['trade'], name)
    if kind in ('inn', 'tavern'):
        return 'innkeeper' if kind == 'inn' else 'tavern keeper', f'keeps {name}'[:40]
    if kind == 'works':
        return WORKS_TRADE.get(record['trade'], 'master'), f'runs {name}'[:40]
    return kind, f'works at {name}'[:40]


def populate(project, place: Place, cfg, manifest, rng, fields, shore):
    P = Folk(project, manifest, rng, cfg['prefix'], R.FEMALE, R.MALE, cfg['heads'], cfg['tails'],
             cfg['quirks'] + R.QUIRKS[:6], cfg['hooks'])
    mine = [b for b in manifest if b.get('town') == place.sid]
    homes = [b for b in mine if b['kind'] in ('house', 'tenement') and not b.get('fresh')]
    rng.shuffle(homes)
    homes.sort(key=lambda b: b['kind'] != 'house')
    inns = [b for b in mine if b['kind'] in ('inn', 'tavern')]
    evening_room = inns[0]['rooms'][0]['id'] if inns else None
    square_world = place.world(*place.square_centre)
    weights = cfg['species']

    def species():
        return rng.choices(['timber', 'maned', 'arctic', 'red', 'ethiopian'], weights)[0]

    def evening(home):
        return P.floor(evening_room, share=True) if evening_room and rng.random() < .6 else home

    def outdoor_spot(where):
        if where == 'field' and fields:
            x, y = rng.choice(rng.choice(fields))
            return P.spot(place.cell['id'], x, y)
        if where == 'shore' and shore:
            x, y = rng.choice(shore)
            return P.spot(place.cell['id'], x, y, share=True)
        g = rng.choice(place.gates)
        x, y = g['out']
        return P.spot(place.cell['id'], x + rng.randint(-3, 3), y + rng.randint(-3, 3), share=True)

    # Each shop's household first, in the flat above it: the keeper, often a partner who helps, and children or an
    # elder in the beds left (Docs/Design/39). They come on top of the town's own count.
    over_shops = shop_households(P, place, [b for b in mine if b['kind'] == 'shop'], rng, species, evening)
    cap = cfg['people'] + over_shops
    # The work there is: each building's keeper (and helpers), the watch, and the work outside the walls.
    jobs = []
    for b in mine:
        if b['kind'] in ('house', 'tenement', 'shop') or b.get('farm') or b.get('fresh'):
            continue
        word, label = labels_for(b)
        if b['kind'] in ('shop', 'inn', 'tavern'):
            hours = (11, 23) if b['kind'] in ('inn', 'tavern') else (7, 18)
            jobs.append(dict(role='merchant', label=label, job=f'the {word} of {place.name}', work=('building', b), hours=hours))
            if b['kind'] in ('inn', 'tavern') and cfg['people'] >= 25:
                jobs.append(dict(role='civilian', label=f'serves at {b["name"]}'[:40], job=f'a server at {b["name"]}',
                                 work=('building', b), hours=(14, 23)))
        elif b['kind'] == 'works':
            # The master who runs it (and sells what it makes), and the hands who work its stations.
            jobs.append(dict(role='merchant', label=label, job=f'the {word} of {place.name}', work=('building', b), hours=(6, 18)))
            hands = sum(len(r['work']) for r in b['rooms']) - 1
            for _ in range(max(0, min(hands, 2))):
                jobs.append(dict(role='civilian', label=f'works at {b["name"]}'[:40], job=f'a hand at {b["name"]}',
                                 work=('building', b), hours=(6, 18)))
        elif b['kind'] == 'chapel':
            jobs.append(dict(role='civilian', label=f'keeps {b["name"]}'[:40], job=f'the keeper of {b["name"]}',
                             work=('building', b), hours=(6, 19)))
        elif b['kind'] == 'healer':
            jobs.append(dict(role='civilian', label='healer', job=f'the healer of {place.name}', work=('building', b), hours=(8, 20)))
        elif b['kind'] == 'warehouse':
            jobs.append(dict(role='civilian', label=f'keeps {b["name"]}'[:40], job='the storekeeper', work=('building', b), hours=(7, 17)))
        elif b['kind'] == 'refectory':
            jobs.append(dict(role='civilian', label='garrison cook', job='the garrison\'s cook', work=('building', b), hours=(5, 19)))
    for job, label, count, where in cfg['outside']:
        for _ in range(count):
            jobs.append(dict(role='civilian', label=label[:40], job=f'a {job} of {place.name}', work=('outside', where),
                             hours=(6, 17) if where != 'gate' else (7, 18)))
    watch = next((b for b in mine if b['kind'] in ('guard', 'barracks')), None)
    leader = next((b for b in mine if b['kind'] == 'leader'), None)
    route = ''
    if cfg['guards']:
        posts = [place.world(*g['in']) for g in place.gates] + [square_world]
        if len(posts) >= 2:
            route = P.route(f'{cfg["prefix"]}watch', f'{place.name} watch', posts)

    def work_place(job):
        kind, what = job['work']
        return P.work(what) if kind == 'building' else outdoor_spot(what)

    # The watch (or the garrison) sleeps where it serves.
    for i in range(cfg['guards']):
        home = (P.bed(watch) if watch else None) or (P.bed(leader) if leader else None)
        if home is None:
            break
        first = i == 0 and leader is not None
        where = leader if first else watch
        P.add(role='guard', work_label=('commands the garrison' if first else 'stands watch')[:40], home=home,
              work=P.work(where) if where else home, evening=home, hours=(6, 18) if i % 2 == 0 else (18, 6),
              route='' if first else route, job='the commander of the garrison' if first else f'one of the watch of {place.name}',
              age=rng.randint(28, 55) if first else rng.randint(19, 45), species=species(), paid=True)
    # Households: a head with a job, a partner (with work if there is any), then children and elders in the beds left.
    for b in homes:
        if len(P.people) >= cap:
            break
        family = None if b['kind'] == 'tenement' else b['name'].replace(' House', '').replace(' Farm', '')
        beds = P.beds(b)
        members = 0
        while beds and len(P.people) < cap:
            home = P.bed(b)
            if home is None:
                break
            beds = P.beds(b)
            if b['kind'] == 'tenement' or members < 2:
                job = jobs.pop(0) if jobs else None
                age = rng.randint(22, 58)
                if job:
                    P.add(role=job['role'], work_label=job['label'], home=home, work=work_place(job), evening=evening(home),
                          hours=job['hours'], family=family, job=job['job'], age=age, species=species())
                else:
                    P.add(role='civilian', work_label='keeps the house', home=home, work=home, evening=evening(home),
                          hours=(8, 17), family=family, job='keeping the family house', age=age, species=species())
            elif rng.random() < .7:
                P.add(role='civilian', work_label='plays about the town', home=home,
                      work=P.spot(place.cell['id'], *place.square_centre, share=True), evening=home, hours=(9, 16),
                      family=family, job='a child of the house', age=rng.randint(8, 15), species=species())
            else:
                P.add(role='civilian', work_label='sits by the well', home=home,
                      work=P.spot(place.cell['id'], *place.square_centre, share=True), evening=home, hours=(10, 15),
                      family=family, job='the eldest of the house', age=rng.randint(62, 80), species=species())
            members += 1
    # Work left with no house to sleep in (a garrison's quartermaster and cook): the watch's or garrison's beds.
    for b in [x for x in (watch, leader) if x]:
        while jobs and len(P.people) < cap + 4:
            home = P.bed(b)
            if home is None:
                break
            job = jobs.pop(0)
            P.add(role=job['role'], work_label=job['label'], home=home, work=work_place(job), evening=home,
                  hours=job['hours'], job=job['job'], age=rng.randint(24, 58), species=species())
    return P, jobs


def shop_households(P, place: Place, shops, rng, species, evening):
    """Each shop's family in its flat upstairs: the keeper (who sells there), a partner who helps in the shop most of
    the time, and children or an elder in the beds left. Returns how many beyond the keepers moved in."""
    extra = 0
    for b in shops:
        family = P.family()
        word, label = labels_for(b)
        home = P.bed(b)
        if home is None:
            continue
        P.add(role='merchant', work_label=label, home=home, work=P.work(b), evening=evening(home), hours=(7, 18),
              family=family, job=f'the {word} of {place.name}, who lives over the shop', age=rng.randint(26, 60),
              species=species())
        members = 1
        while True:
            bed = P.bed(b)
            if bed is None:
                break
            if members == 1 and rng.random() < .8:
                P.add(role='civilian', work_label=f'helps at {b["name"]}'[:40], home=bed,
                      work=P.floor(b['rooms'][0]['id'], share=True), evening=evening(bed), hours=(8, 17), family=family,
                      job=f'helping in the family\'s shop, {b["name"]}', age=rng.randint(24, 58), species=species())
            elif rng.random() < .75:
                P.add(role='civilian', work_label='plays about the town', home=bed,
                      work=P.spot(place.cell['id'], *place.square_centre, share=True), evening=bed, hours=(9, 16),
                      family=family, job='a child of the shop', age=rng.randint(6, 15), species=species())
            else:
                P.add(role='civilian', work_label='minds the shop\'s door', home=bed,
                      work=P.floor(b['rooms'][0]['id'], share=True), evening=bed, hours=(10, 15), family=family,
                      job='the eldest of the family', age=rng.randint(62, 82), species=species())
            members += 1
            extra += 1
    return extra


def staff_industry(P, place: Place, manifest, rng):
    """Doc 35's businesses, staffed after everyone else: a master (and a hand or two at a works) in whatever beds the
    town still has free. Returns the jobs no bed could be found for."""
    P.rng = rng
    mine = [b for b in manifest if b.get('town') == place.sid]
    homes = [b for b in mine if b['kind'] in ('house', 'tenement')]
    rng.shuffle(homes)
    homes.sort(key=lambda b: not b.get('fresh'))
    jobs = []
    for b in [b for b in mine if b.get('fresh') and b['kind'] != 'shop']:
        word, label = labels_for(b)
        jobs.append(dict(role='merchant', label=label, job=f'the {word} of {place.name}', b=b))
        if b['kind'] == 'works':
            hands = sum(len(r['work']) for r in b['rooms']) - 1
            jobs += [dict(role='civilian', label=f'works at {b["name"]}'[:40], job=f'a hand at {b["name"]}', b=b)
                     for _ in range(max(0, min(hands, 2)))]
    unfilled = []
    for job in jobs:
        home = next((bed for bed in (P.bed(h) for h in homes) if bed), None)
        if home is None:
            unfilled.append(job)
            continue
        P.add(role=job['role'], work_label=job['label'], home=home, work=P.work(job['b']), evening=home, hours=(6, 18),
              job=job['job'], age=rng.randint(20, 58))
    return unfilled


# ---------------------------------------------------------------------------------------------------------------
# Ridgemere, filled out
# ---------------------------------------------------------------------------------------------------------------

# Rooms nobody new moves into: halls, inns, the Houses' seats and cottages, guard posts, upstairs rooms.
NOT_HOMES = ('Cottages', 'Gatehouse', 'Hall', 'Court,', 'Greatly House', 'bedchambers', 'Inn', 'Lodge', 'Infirmary', 'Mending', 'Gaol', 'Kitchens', 'Armory', 'Armoury', 'Healer', 'Barracks',
             'Guardhouse', 'Guard House', 'Watch House', 'Pup Den', 'Trainers', 'Saint Chi', 'upstairs', 'the flat above')
# Work a city always has more hands for: (label, what they are, where: 'shop' is a shop floor, 'street' near where
# others already work outdoors), and the share of the newcomers each takes.
CITY_WORK = [
    ('apprenticed at a shop', 'an apprentice', 'shop', 4), ('sweeps and fetches for a shop', 'a shop\'s errand runner', 'shop', 2),
    ('carries loads for hire', 'a porter', 'street', 3), ('sells from a tray in the street', 'a street seller', 'street', 3),
    ('runs messages about the city', 'a messenger', 'street', 2), ('washes linen for hire', 'a washer', 'street', 2),
    ('lights the lamps at dusk', 'a lamplighter', 'street', 1), ('begs at the corners', 'a beggar', 'street', 1),
]
CITIES_FILL = {
    'ridgemere': dict(prefix='rmx_', count=150, species=[4, 0, 2, 3, 0]),
}


def fill_city(project, city, cfg, seed=SEED):
    """More people in the city (cfg['count']): in its free house and tenement beds, at its shops and in its streets."""
    from . import citizens as C
    rng = random.Random(f'{seed}:{city}')
    names = cfg.get('names') or (C.RM_FEMALE, C.RM_MALE, C.RM_HEADS, C.RM_TAILS, C.RM_QUIRKS, C.RM_HOOKS)
    P = Folk(project, [], rng, cfg['prefix'], *names)
    for p in project.get('people', []):
        P.names.add(p['name'])
        P.taken.add((p['home']['cell'], p['home']['x'], p['home']['y']))

    def ours(r):
        return r['id'].startswith('rm_')
    from .site import FRESH_ROOMS
    rooms = [r for r in project['rooms'] if ours(r) and r['id'] not in FRESH_ROOMS]
    homes = [r for r in rooms if not any(word in r['name'] for word in NOT_HOMES)]
    beds = [(r, x, y) for r in homes for y, row in enumerate(r['terrain']) for x, ch in enumerate(row)
            if ch in 'bz' and (r['id'], x, y) not in P.taken]
    rng.shuffle(beds)
    shops = [r for r in rooms if any('=' in row for row in r['terrain']) and not any(w in r['name'] for w in NOT_HOMES)]
    prefix = 'rm_'
    cells = {c['id'] for c in project['cells']}
    anchors = [p['work'] for p in project['people']
               if p['work']['cell'] in cells and (p['id'].startswith(prefix) if prefix else not p['id'].startswith(('rm_', 'sf_', 'tn_')))]
    evenings = [r for r in rooms if 'Inn' in r['name'] or 'Tavern' in r['name'] or 'tavern' in r['description'].lower()]
    pool = [w for w in CITY_WORK for _ in range(w[3])]

    def species():
        return rng.choices(['timber', 'maned', 'arctic', 'red', 'ethiopian'], cfg['species'])[0]

    added = 0
    for r, x, y in beds:
        if added >= cfg['count']:
            break
        if (r['id'], x, y) in P.taken:
            continue
        home = P.spot(r['id'], x, y)
        family = r['name'][:-6] if r['name'].endswith(' House') and ' ' not in r['name'][:-6] else None
        evening = P.floor(rng.choice(evenings)['id'], share=True) if evenings and rng.random() < .5 else home
        roll = rng.random()
        if roll < .16 and anchors:
            a = rng.choice(anchors)
            P.add(role='civilian', work_label='plays in the streets', home=home, work=P.spot(a['cell'], a['x'], a['y'], share=True),
                  evening=home, hours=(9, 16), family=family, job='a child of the city', age=rng.randint(8, 15), species=species())
        elif roll < .26 and anchors:
            a = rng.choice(anchors)
            P.add(role='civilian', work_label='sits and watches the world', home=home,
                  work=P.spot(a['cell'], a['x'], a['y'], share=True), evening=evening, hours=(10, 15), family=family,
                  job='one of the city\'s old ones', age=rng.randint(62, 82), species=species())
        else:
            label, what, where, _ = rng.choice(pool)
            if where == 'shop' and shops:
                work = P.floor(rng.choice(shops)['id'], share=True)
                age = rng.randint(14, 24) if 'apprentice' in what else rng.randint(12, 40)
            elif anchors:
                a = rng.choice(anchors)
                work = P.spot(a['cell'], a['x'] + rng.randint(-4, 4), a['y'] + rng.randint(-4, 4), share=True)
                age = rng.randint(17, 60)
            else:
                continue
            P.add(role='civilian', work_label=label, home=home, work=work, evening=evening,
                  hours=(17, 23) if 'lamps' in label else (7, 18), family=family, job=f'{what} of {city.replace("_", " ").title()}',
                  age=age, species=species())
        added += 1
    return P.people


# ---------------------------------------------------------------------------------------------------------------
# All of it
# ---------------------------------------------------------------------------------------------------------------

def build(project, sid, cfg, reserved, seed=SEED):
    """One settlement built and peopled on a copy of its cell. Returns what to add, or raises with the reason not to."""
    entry = next(s for s in SETTLEMENTS if s[0] == sid)
    _, name, kind, box, _, _ = entry
    place = Place(project, sid, name, kind, box)
    edited = place.edits()
    if edited and not cfg.get('ruined'):
        shown = ', '.join(f'{ch}@{x},{y}' for x, y, ch in edited[:12])
        raise ValueError(f'{name}: its ground has been edited ({len(edited)} tiles: {shown}…); not building over it')
    if not place.gates:
        raise ValueError(f'{name}: no gates found in its wall')
    rng = random.Random(f'{seed}:{sid}')
    site = TownSite(place, cfg['prefix'], reserved)
    heads, tails = cfg['heads'], cfg['tails']
    families, used = [], set()
    while len(families) < cfg['houses'] + cfg.get('farms', 0) + 2:
        f = rng.choice(heads) + rng.choice(tails)
        if f not in used:
            used.add(f)
            families.append(f)
    placed, missing, fields = [], [], []
    if cfg.get('ruined'):
        # Squatters patch two shacks together in the ruins: nothing else is rebuilt.
        codes = place.canvas.codes
        open_ground = place.inside & shrink(place.inside, 3) & (codes == ord('.'))
        solid = np.isin(codes, [ord(t['code']) for t in catalog.TILES if t.get('solid')])
        # Doors open onto ground a wolf can stand on, and that the gate reaches (not behind a fallen wall).
        walk = reachable(codes, place.canvas.heights, place.gates[0]['in'])
        lots = Lots(open_ground & walk, place.inside & ~open_ground & ~solid & walk, gap=2)
        ys, xs = np.nonzero(place.inside)
        centre = (int(xs.mean()), int(ys.mean()))
        for i in range(cfg['houses']):
            b = make(cfg, ('house', 2), rng, families[i])
            b.name = f'{families[i]}\'s Shack'
            b.rooms[0].name = b.name
            b.rooms[0].description = 'Two rooms of a dead house made one, roofed with boards and sacking; a fire in a ' \
                                     'broken hearth, bedding of straw.'
            for _, x0, y0, facing, w, h in lots.candidates(*b.footprint, centre, limit=800):
                if lots.clear(x0, y0, w, h):
                    site.place(b, x0, y0, facing, sid, open_door=False)
                    site.manifest[-1]['town'] = sid
                    lots.take(x0, y0, w, h)
                    break
        place.square_centre = centre
    else:
        street, square, centre = lay_streets(place, cfg, rng)
        paint_ground(place, cfg, street, square, centre, rng)
        place.square_centre = centre
        placed, missing = place_buildings(place, site, cfg, street, square, centre, rng, families)
        if len(placed) < max(1, len(cfg['buildings']) // 2):
            raise ValueError(f'{name}: only {len(placed)} buildings fit; not building it like that')
        fields = farms(place, cfg, site, rng, families)
    shore = shore_spots(place, rng)
    if cfg.get('industry') and not cfg.get('ruined'):
        missing += place_industry(place, site, cfg, random.Random(f'{seed}:{sid}:doc35'))
    if cfg.get('pier') and getattr(place, 'left_out', None):
        still = pier_quarter(place, site, cfg, random.Random(f'{seed}:{sid}:pier'), pier_attempt(place, site))
        missing = [n for n in missing if n in still]
    # Every door must be reachable from a gate on foot, by the game's own rules.
    start = place.gates[0]['in']
    walk = reachable(place.canvas.codes, place.canvas.heights, start)
    for record in site.manifest:
        dx, dy = record['door']['x'], record['door']['y']
        if not walk[dy, dx]:
            raise ValueError(f'{name}: the door of {record["name"]} can\'t be reached from the gate')
    place.write_back()
    rooms = [{**r, 'worldX': r['worldX'] + place.cell['x'], 'worldY': r['worldY'] + place.cell['y']} for r in site.rooms]
    staged = {**project, 'rooms': project['rooms'] + rooms, 'links': project['links'] + site.links}
    P, unfilled = populate(staged, place, cfg, site.manifest, rng, fields, shore)
    unfilled += staff_industry(P, place, site.manifest, random.Random(f'{seed}:{sid}:doc35:people'))
    return {'place': place, 'rooms': rooms, 'links': site.links, 'people': P.people, 'routes': P.routes,
            'manifest': site.manifest, 'missing': missing, 'unfilled': unfilled}


def build_all(project, only=None, report=print):
    """Every unbuilt settlement added to the project (a copy). Returns (project, results by place)."""
    out = {**project, 'cells': [dict(c) for c in project['cells']], 'rooms': list(project['rooms']),
           'links': list(project['links']), 'people': list(project.get('people', [])),
           'routes': list(project.get('routes', []))}
    results = {}
    for sid, cfg in TOWNS.items():
        if only and sid not in only:
            continue
        prefix = cfg['prefix']
        if any(r['id'].startswith(prefix) for r in out['rooms']) or any(p['id'].startswith(prefix) for p in out['people']):
            report(f'{sid}: already built (prefix {prefix} in use); skipped')
            continue
        reserved = {a['id'] for a in out['cells'] + out['rooms']}
        try:
            result = build(out, sid, cfg, reserved)
        except ValueError as e:
            report(f'REFUSED {e}')
            continue
        out['rooms'] += result['rooms']
        out['links'] += result['links']
        out['people'] += result['people']
        out['routes'] += result['routes']
        results[sid] = result
        roles = {}
        for p in result['people']:
            roles[p['role']] = roles.get(p['role'], 0) + 1
        report(f'{sid}: {len(result["manifest"])} buildings, {len(result["rooms"])} interiors, {len(result["people"])} '
               f'people {roles}' + (f'; no room for {result["missing"]}' if result['missing'] else '')
               + (f'; {len(result["unfilled"])} jobs unfilled' if result['unfilled'] else ''))
    for city, cfg in CITIES_FILL.items():
        if only and city not in only:
            continue
        if any(p['id'].startswith(cfg['prefix']) for p in out['people']):
            report(f'{city}: already filled out (prefix {cfg["prefix"]} in use); skipped')
            continue
        people = fill_city(out, city, cfg)
        out['people'] += people
        results[city] = {'people': people, 'rooms': [], 'links': [], 'routes': [], 'manifest': [], 'missing': [], 'unfilled': []}
        report(f'{city}: {len(people)} more people')
    return out, results


def main(argv=None):
    import map_editor
    import world_db
    import world_store
    from .realm import dev_project
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--dry-run', action='store_true', help='Build and check, but do not save to DEV')
    parser.add_argument('--only', help='Comma-separated settlement IDs')
    parser.add_argument('--preview', type=Path, help='Write a picture of each place built into this folder')
    args = parser.parse_args(argv)
    project, revision = dev_project()
    only = set(args.only.split(',')) if args.only else None
    merged, results = build_all(project, only)
    if not results:
        print('Nothing to add.')
        return 0
    if args.preview:
        from .preview import glyphs
        args.preview.mkdir(parents=True, exist_ok=True)
        for sid, r in results.items():
            if 'place' not in r:
                continue
            p = r['place']
            x0, y0 = max(0, p.x0 - 40), max(0, p.y0 - 40)
            w, h = min(p.cell['width'] - x0, p.w + 80), min(p.cell['height'] - y0, p.h + 80)
            glyphs(p.canvas.codes, p.canvas.heights, args.preview / f'{sid}.png', x0, y0, w, h, px=10)
    try:
        map_editor.check_project(merged, for_game=False)      # As saving checks it (streamed worlds may be any size).
    except map_editor.ValidationError as e:
        print(f'Problems: {e}')
        return 1
    added = sum(len(r['people']) for r in results.values())
    print(f'Checked: {len(results)} places, {added} people added; {len(merged["people"])} residents in all.')
    if args.dry_run:
        print('Dry run: DEV not changed.')
        return 0
    # The world as it was, kept before anything is written (as the other generators keep it).
    import json
    import time
    folder = Path(__file__).resolve().parents[2] / 'artifacts/backups'
    folder.mkdir(parents=True, exist_ok=True)
    backup = folder / f'{project["id"]}_dev_r{revision}_before_towns_{time.strftime("%Y%m%d-%H%M%S")}.atlas.json'
    backup.write_text(json.dumps(project, ensure_ascii=False), encoding='utf-8')
    print(f'DEV revision {revision} backed up to {backup}.')
    with world_db.connect('dev', 'editor') as conn:
        new = world_store.save_world(conn, merged, revision)
    print(f'Saved to DEV at revision {new}.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
