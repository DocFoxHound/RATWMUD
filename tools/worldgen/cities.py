"""Ridgemere, the one city of Docs/Design/60, designed on the world canvas the realm generator lays out.

A rain-soaked harbour city on its hills above the Sound, in tiers: the wharf and the docks along the water outside the
west wall; inside it, the works (the industry sector) and the Sump (the slums) on the lowest tier by the water; the City
Office quarter, Harbour Hill and the Commons (the middle town) above them; and the Heights, where the rich live and
House Greatly keeps its walled house and gardens, at the top. Three market squares, one to each part of the city. The
wall has three gates to the country (north, east, south) and three water gates onto the wharf; houses spill out along
the roads beyond the gates. Houses Fell, Grayrock and Mossgrave keep their estates and gardens outside the walls.

Buildings are placed with the building kit (buildings.py, site.py): each has its interiors, doors and the beds and
work places its residents are given (citizens.py).
"""
from __future__ import annotations

import math
import random

import numpy as np

from . import field
from .buildings import (Building, TRADES, WORKS, cathedral, hall, house, manor, palace, shop, shop_with_flat, tavern, tenement,
                        villa, works)
from .canvas import half
from .site import Lots, Site, footprint_size

CELL = 256
DESIGNED = ('ridgemere',)


def fp(w, h, scale=.72):
    """An interior's footprint on the outdoor map (interiors are drawn roomier than their blocks)."""
    return max(6, round(w * scale)), max(5, round(h * scale * .9))


class CitySite(Site):
    """One site for both cities: room IDs carry the city's prefix and never collide with the world's."""

    def __init__(self, canvas, reserved):
        super().__init__(canvas, [(f'@{cx}_{cy}', cx * CELL, cy * CELL, CELL, CELL)
                                  for cy in range(canvas.height // CELL) for cx in range(canvas.width // CELL)])
        self.ids |= set(reserved)
        self.prefix = ''

    def unique(self, name):
        return super().unique(f'{self.prefix}{name}')


def grow(mask, n=1):
    for _ in range(n):
        g = mask.copy()
        g[1:] |= mask[:-1]
        g[:-1] |= mask[1:]
        g[:, 1:] |= mask[:, :-1]
        g[:, :-1] |= mask[:, 1:]
        mask = g
    return mask


def shrink(mask, n=1):
    return ~grow(~mask, n)


def blur_mask(mask, r):
    a = mask.astype(np.float32)
    for axis in (0, 1):
        c = np.cumsum(np.pad(a, [(r + 1, r) if i == axis else (0, 0) for i in range(2)], mode='edge'), axis=axis)
        n = 2 * r + 1
        a = (c[n:] - c[:-n]) / n if axis == 0 else (c[:, n:] - c[:, :-n]) / n
    return a > .5


class City:
    def __init__(self, world, site, seed, prefix, region):
        self.world, self.site, self.c = world, site, world.c
        self.rng = random.Random(seed)
        # Doc 35's additions draw from their own sequence, so the buildings that were already here keep their sizes.
        self.new_rng = random.Random(seed * 7 + 35)
        self.prefix, self.region = prefix, region
        self.placed = []                    # (building id, Building, manifest record)
        self.districts = {}                 # name -> anchor (canvas x, y)
        self.roads = []                     # extra roads: (name, stops, straight)
        self.spots = {}                     # named outdoor places residents are given (canvas x, y)
        self.built = np.zeros(self.c.codes.shape, dtype=bool)     # every building's block
        self.drives = {}

    # -- ground --------------------------------------------------------------------------------------------------
    def cell_box(self, cx, cy, inset=0):
        m = np.zeros(self.c.codes.shape, dtype=bool)
        m[cy * CELL + inset:(cy + 1) * CELL - inset, cx * CELL + inset:(cx + 1) * CELL - inset] = True
        return m

    def distance_to_water(self, box, limit=120):
        """Distance to the nearest water (capped), computed on the box's window only."""
        ys, xs = np.nonzero(box)
        y0, y1 = max(0, ys.min() - limit), min(self.c.height, ys.max() + limit + 1)
        x0, x1 = max(0, xs.min() - limit), min(self.c.width, xs.max() + limit + 1)
        out = np.full(self.c.codes.shape, limit, dtype=np.float32)
        out[y0:y1, x0:x1] = field.distance_to_mask(self.world.water[y0:y1, x0:x1], limit)
        return out

    def find_box(self, cell, w, h, anchor, taken, inset=8, step=6):
        """The best w x h box in the cell for an estate: dry, clear of other places, fairly level, near `anchor`."""
        cx, cy = cell
        heights, water = self.c.heights, self.world.water
        best = None
        for y in range(cy * CELL + inset, (cy + 1) * CELL - inset - h + 1, step):
            for x in range(cx * CELL + inset, (cx + 1) * CELL - inset - w + 1, step):
                if water[y - 4:y + h + 4, x - 4:x + w + 4].any() or taken[y - 6:y + h + 6, x - 6:x + w + 6].any():
                    continue
                if self.world.ua_mask[y:y + h, x:x + w].any():
                    continue
                area = heights[y:y + h:3, x:x + w:3]
                score = math.dist((x + w / 2, y + h / 2), anchor) + 18 * float(area.std()) + 3 * float(np.ptp(area))
                if best is None or score < best[0]:
                    best = (score, x, y)
        if best is None:
            raise ValueError(f'{self.region}: no room for a {w}x{h} estate in cell {cell}')
        return best[1], best[2]

    def inner_ring(self, ground, width):
        """A lane just inside a walled ground, all the way round."""
        ys, xs = np.nonzero(ground)
        y0, y1, x0, x1 = ys.min() - 2, ys.max() + 3, xs.min() - 2, xs.max() + 3
        d = field.distance_to_mask(~ground[y0:y1, x0:x1], width + 2)
        out = np.zeros(ground.shape, dtype=bool)
        out[y0:y1, x0:x1] = ground[y0:y1, x0:x1] & (d <= width)
        return out

    def ring(self, region, thick):
        ys, xs = np.nonzero(region)
        y0, y1, x0, x1 = ys.min() - 2, ys.max() + 3, xs.min() - 2, xs.max() + 3
        inner = field.distance_to_mask(~region[y0:y1, x0:x1], thick + 1)
        out = np.zeros(region.shape, dtype=bool)
        out[y0:y1, x0:x1] = region[y0:y1, x0:x1] & (inner <= thick)
        return out

    def gate(self, region, ring, target, side, width, thick, level, key=None, ch='G'):
        """Open a gate in the wall nearest `target`, through the wall's depth; returns the point just outside."""
        ry, rx = np.nonzero(ring)
        i = int(np.argmin((rx - target[0]) ** 2 + (ry - target[1]) ** 2))
        gx, gy = int(rx[i]), int(ry[i])
        vx, vy = {'N': (0, -1), 'S': (0, 1), 'W': (-1, 0), 'E': (1, 0)}[side]
        reach = width + thick + 2
        yy, xx = np.mgrid[max(0, gy - reach):gy + reach + 1, max(0, gx - reach):gx + reach + 1]
        depth = (gx - xx) * vx + (gy - yy) * vy
        lateral = np.abs((xx - gx) * vy - (yy - gy) * vx)
        near = ring[yy, xx] & (lateral <= width // 2) & (depth >= -1) & (depth <= thick + 1)
        m = np.zeros(region.shape, dtype=bool)
        m[yy[near], xx[near]] = True
        self.c.paint(m, ch, level)
        ox, oy = gx, gy
        for _ in range(60):
            ox, oy = ox + vx, oy + vy
            if not region[oy, ox]:
                break
        out = (float(ox + vx * 4), float(oy + vy * 4))
        if key:
            self.world.gates[key] = out
            self.world.gate_levels[key] = float(self.c.heights[gy, gx])
        inside = (gx - vx * (thick + 2), gy - vy * (thick + 2))
        return out, inside

    def compound(self, sid, x, y, w, h, wall, floor, gates, thick=2):
        """A walled, levelled compound: its ground, wall and gates. Returns (region, inner ground, level)."""
        region = np.zeros(self.c.codes.shape, dtype=bool)
        region[y:y + h, x:x + w] = True
        level = self.world.flatten_mask(region, 12)
        ring = self.ring(region, thick)
        self.c.paint(region, floor, level)
        self.c.paint(ring, wall, level)
        self.world.places[sid] = region
        self.world.levels[sid] = level
        insides = {}
        for side, target in gates.items():
            _, insides[side] = self.gate(region, ring, target, side, 5, thick, level, key=(sid, side))
        return region, region & ~grow(ring, 1), level, insides

    def estate_ground(self, sid, region, ground, level, gate_in):
        c = self.c
        ys, xs = np.nonzero(region)
        x, y, w, h = xs.min(), ys.min(), xs.max() - xs.min() + 1, ys.max() - ys.min() + 1
        far = (x + w - gate_in[0] + x, y + h - gate_in[1] + y)       # The manor stands away from the gate.
        self.districts[f'{sid}_seat'] = (min(max(far[0], x + 20), x + w - 20), min(max(far[1], y + 18), y + h - 18))
        self.districts[f'{sid}_works'] = gate_in
        drive = c.line_mask([gate_in, self.districts[f'{sid}_seat']], 4) & ground
        self.drives[sid] = drive
        avenues = drive | self.inner_ring(ground, 3)
        c.paint(avenues, 'd', level)
        return avenues

    @staticmethod
    def facing(frm, to):
        dx, dy = to[0] - frm[0], to[1] - frm[1]
        return ('E' if dx > 0 else 'W') if abs(dx) > abs(dy) else ('S' if dy > 0 else 'N')

    def works_quarter(self, sid, name, cells, size, toward, city_gate, wanted, style, wall_ch, ground_ch, extra):
        """A walled quarter of workshops outside the city (doc 35), found like an estate (clear of every place and
        road), with its own road to the city gate. Its buildings use the districts f'{sid}_works' (by its gate) and
        f'{sid}_seat' (the far side). Called once the rest of the world is laid out (build_quarters)."""
        w = self.world
        taken = w.ua_mask | self.c.locked | np.isin(self.c.codes, [ord(ch) for ch in 'd_8G+'])
        for other in w.places.values():
            taken = taken | other
        # The full size near the city if it fits, else smaller (fewer of its buildings may then find a lot).
        spot = None
        for scale in (1, .9, .8, .7):
            ew, eh = int(size[0] * scale), int(size[1] * scale)
            for cell in cells:
                try:
                    spot = self.find_box(cell, ew, eh, toward, grow(taken, 6))
                    break
                except ValueError:
                    continue
            if spot:
                break
        if spot is None:
            print(f'  {self.region}: no room anywhere for {name}')
            return
        x, y = spot
        print(f'  {self.region}: {name} is {ew}x{eh} at ({x}, {y})')
        side = self.facing((x + ew / 2, y + eh / 2), toward)
        region, ground, level, insides = self.compound(sid, x, y, ew, eh, wall_ch, ground_ch, {side: toward})
        avenues = self.estate_ground(sid, region, ground, level, insides[side])
        lanes = self.lanes(ground, (24, 18), 3, angle=.1)
        lots, _ = self.district(ground & ~avenues, avenues, lanes & ~avenues, wanted(), jitter=10, lane_ch='d')
        self.yards(lots.open & ground, ground_ch, extra=extra)
        self.spots[sid] = insides[side]
        # Its road, routed now on the finished map from the quarter's gate to the city's.
        a, b = w.gates[(sid, side)], w.gates[city_gate]
        line = w.route(a, b, w.route_costs())[:-1] + [b]
        road = f'{name} Road' if name.startswith('The ') else f'The {name} Road'
        w.road_lines[road] = line
        w.road(road, line, ends=(w.gate_levels[(sid, side)], w.gate_levels[city_gate]))

    # -- buildings -----------------------------------------------------------------------------------------------
    def place_all(self, lots, wanted, gap_jitter=16):
        for b in wanted:
            rng = self.new_rng if getattr(b, 'fresh', False) else self.rng
            near = self.districts[b.district]
            jitter = (near[0] + rng.uniform(-gap_jitter, gap_jitter), near[1] + rng.uniform(-gap_jitter, gap_jitter))
            fw, fh = b.footprint
            for _, x0, y0, facing, w, h in lots.candidates(fw, fh, jitter, limit=60000):
                if lots.clear(x0, y0, w, h):
                    self.site.prefix = self.prefix
                    bid, outside = self.site.place(b, x0, y0, facing, self.region, open_door=b.kind not in ('house', 'tenement', 'villa', 'manor'))
                    lots.take(x0, y0, w, h)
                    self.built[y0:y0 + h, x0:x0 + w] = True
                    record = self.site.manifest[-1]
                    record['city'] = self.region
                    self.placed.append((bid, b, record))
                    break
            else:
                print(f'  {self.region}: no lot for {b.name} ({b.kind}, {b.district})')

    def district(self, ground, avenues, lanes, wanted, jitter=14, lane_ch='_'):
        """Build a district: the large buildings first, each on an avenue; then the lanes are cut between them and
        the smaller buildings fill in along lanes and avenues; doc 35's additions (fresh) take what lots are left, so
        the buildings that were here before keep their places. Returns the lots left over (for yards)."""
        fresh = [b for b in wanted if getattr(b, 'fresh', False)]
        wanted = [b for b in wanted if not getattr(b, 'fresh', False)]
        big = [b for b in wanted if b.footprint[0] * b.footprint[1] >= 130]
        small = [b for b in wanted if b.footprint[0] * b.footprint[1] < 130]
        lots = Lots(ground & ~avenues, avenues)
        self.place_all(lots, big, gap_jitter=jitter)
        cut = lanes & ground & ~grow(self.built, 1)
        self.c.paint(cut, lane_ch, None)
        streets = avenues | cut
        lots = Lots(ground & ~streets & ~grow(self.built, 1), streets)
        self.place_all(lots, small, gap_jitter=jitter)
        self.place_all(lots, fresh, gap_jitter=jitter)
        return lots, streets

    def named(self, kind, name, text, w, h, district, style, roof=''):
        p = hall(self.rng, style, w, h, kind)
        return Building(kind, name, style, fp(w, h), [p.room('', name, text)], district=district, roof=roof)

    def building(self, kind, name, text, plans, district, style, stairs=(), roof='', trade='', keys=None,
                 names=None, texts=None, zs=None, scale=.72):
        rooms = []
        for i, p in enumerate(plans):
            key = (keys or ['', 'up', 'down'])[i]
            rooms.append(p.room(key, (names or [name])[i] if i < len(names or [name]) else name,
                                (texts or [text])[i] if i < len(texts or [text]) else text,
                                z=(zs or [0, 1, -1])[i]))
        return Building(kind, name, style, fp(plans[0].w, plans[0].h, scale), rooms, list(stairs), district, roof, trade)

    def a_shop(self, trade, name, text, district, style, fresh=False):
        # A shop, and the flat above it where its keeper's family lives (Docs/Design/39).
        rooms, stairs, (w, h) = shop_with_flat(self.new_rng if fresh else self.rng, style, trade, name, text)
        b = Building('shop', name, style, fp(w, h), rooms, stairs, district=district, trade=trade)
        b.fresh = fresh
        return b

    def market_stalls(self, cx, cy, columns, rows, level):
        """Rows of market stalls on a square (Docs/Design/39): two-tile stalls across the square in facing rows, with the
        aisle between them; each stallholder's place behind their stall. Records the places in spots['stalls']."""
        c = self.c
        places = self.spots.setdefault('stalls', [])
        for dy in rows:
            behind = 1 if dy > 0 else -1
            for dx in columns:
                x, y = cx + dx, cy + dy
                c.stamp(x, y, ['uu'], level)
                places.append((x, y + behind))
        return places

    def a_works(self, kind, name, text, district, style, hands=3, roof=''):
        """One of doc 35's workshops: its stations laid out, the trade recorded so its master and hands are found."""
        b = self.building('works', name, f'{name}: {WORKS[kind][1]}. {text}',
                          [works(self.new_rng, 'works' if style == 'ridgemere' else style, kind, hands)], district,
                          style, roof=roof or STYLE_ROOFS.get(style, ''), trade=kind)
        b.fresh = True
        return b

    def a_house(self, name, text, district, style, people=3, kind='house', fresh=False):
        rng = self.new_rng if fresh else self.rng
        p = house(rng, style, people) if kind == 'house' else tenement(rng, style, people)
        b = Building(kind, name, style, fp(p.w, p.h), [p.room('', name, text)], district=district,
                     roof=STYLE_ROOFS.get(style, ''))
        b.fresh = fresh
        return b

    def a_tavern(self, name, text, district, style, inn=False, upstairs_text=''):
        plans, stairs = tavern(self.rng, style, inn)
        rooms = [plans[0].room('', name, text)]
        pairs = []
        if inn:
            rooms.append(plans[1].room('up', f'{name}, upstairs', upstairs_text, z=1))
            _, x, y = stairs[0]
            pairs.append((('', x, y), ('up', x, y)))
        return Building('inn' if inn else 'tavern', name, style, fp(plans[0].w, plans[0].h), rooms, pairs, district)

    def yards(self, open_ground, grass=',', extra=(), density=.08):
        """Whatever is left between buildings."""
        rng = np.random.default_rng(self.rng.randrange(1 << 30))
        noise = field.fbm(self.c.width, self.c.height, 10, self.rng.randrange(1000), 2)
        free = open_ground & ~self.world.water
        self.c.codes[free & (noise > .55)] = ord(grass)
        grain = rng.random(free.shape)
        for ch, share in extra:
            self.c.codes[free & (grain < share)] = ord(ch)
            grain = rng.random(free.shape)

    def lanes(self, region, spacing, width, angle=0.0, offset=(0, 0)):
        ys, xs = np.nonzero(region)
        y0, y1, x0, x1 = ys.min(), ys.max(), xs.min(), xs.max()
        m = np.zeros(region.shape, dtype=bool)
        sx, sy = spacing
        ca, sa = math.cos(angle), math.sin(angle)
        L = max(x1 - x0, y1 - y0) * 2
        cx, cy = (x0 + x1) / 2 + offset[0], (y0 + y1) / 2 + offset[1]
        for k in range(-40, 41):
            ax, ay = cx + k * sx * ca, cy + k * sx * sa
            m |= self.c.line_mask([(ax - L * sa, ay + L * ca), (ax + L * sa, ay - L * ca)], width)
            bx, by = cx - k * sy * sa, cy + k * sy * ca
            m |= self.c.line_mask([(bx - L * ca, by - L * sa), (bx + L * ca, by + L * sa)], width)
        return m & region




STYLE_ROOFS = {'ridgemere': 'Z', 'manor': 'Z', 'works': 'Z'}


# ---------------------------------------------------------------------------------------------------------------
# Ridgemere (Docs/Design/60)
# ---------------------------------------------------------------------------------------------------------------
# The walled city: an outline on the canvas, cut back from the Sound by the wharf's width and from the Rainrun.
WALL_OUTLINE = [(352, 212), (430, 198), (520, 196), (592, 212), (630, 262), (640, 340), (636, 420), (620, 500),
                (582, 556), (500, 574), (420, 570), (356, 554), (338, 470), (336, 380), (338, 290)]
WHARF_WIDTH = 46                         # From the water's edge to the city wall.
# Squares: centre and half-size (x, y); each market has two facing rows of four stalls.
SQUARES = {'office': ((470, 352), (16, 10)), 'heights_market': ((522, 298), (14, 9)),
           'commons_market': ((522, 470), (15, 9)), 'sump_market': ((392, 428), (14, 9))}
MARKETS = {  # square: (its name, what its stalls sell, how the stallholders call)
    'heights_market': ('the Heights Market', ['cheese', 'honey', 'fruit', 'pie', 'nut', 'egg', 'bread', 'pickle'],
                       'Fine {food}, fresh this morning. For the Heights, only the best.'),
    'commons_market': ('the Commons Market', ['bread', 'sausage', 'cheese', 'egg', 'fruit', 'pie', 'pickle', 'nut'],
                       '{Food}! Good {food}, fair price, out of the rain!'),
    'sump_market': ('the Sump Market', ['fish', 'soup', 'porridge', 'bread', 'sausage', 'pickle', 'egg', 'nut'],
                    'Hot {food}, two pence! Hot {food}!'),
}
DISTRICTS = {  # anchor (x, y) of each district: buildings of a district are placed near it, on their own tier
    'works': (398, 524), 'works_w': (360, 492), 'sump': (388, 412), 'sump_n': (370, 352),
    'harbour_hill': (400, 268), 'office': (470, 352), 'office_e': (520, 372),
    'commons': (522, 470), 'commons_s': (520, 532), 'commons_e': (596, 474),
    'heights': (520, 290), 'heights_n': (470, 226), 'heights_e': (602, 368),
}
# The tiers: 0 the wharf (outside the wall), 1 the works and the Sump, 2 the middle town (the City Office, Harbour
# Hill, the Commons), 3 the Heights. Each district's tier, and its streets (lane spacing x, y, and width).
ZONES = {'works': (1, (34, 26), 3), 'sump': (1, (18, 13), 2), 'office': (2, (28, 21), 3), 'commons': (2, (26, 19), 3),
         'heights': (3, (36, 28), 3)}
ZONE_GROUND = {'works': '>', 'sump': '.', 'office': 'f', 'commons': '.', 'heights': ','}
HOUSES = {  # House: (seat, colour words, its trade, blurb, where its estate would like to be, the cells to try, size)
    'greatly': ('Greatly House', 'the Greatlys', 'ships, the wharf and the bank',
                'The richest of the Houses and the only one inside the walls: Greatly ships carry Ridgemere\'s iron, '
                'timber and wool away, and Greatly money lends the city what it needs.', None, None, (88, 66)),
    'fell': ('Fellhall', 'the Fells', 'timber',
             'Fell men fell the cedar: the House owns the logging camps of the Rainwood and the great saw on the '
             'Rainrun.', (720, 110), [(2, 0), (3, 0), (2, 1)], (150, 112)),
    'grayrock': ('Grayrock Hall', 'the Grayrocks', 'stone and iron',
                 'The oldest of the Houses: its quarries cut the stone of Ridgemere and its forges smelt the iron that '
                 'leaves the wharf.', (820, 330), [(3, 1), (2, 1), (3, 0)], (150, 112)),
    'mossgrave': ('Mossgrave Court', 'the Mossgraves', 'wool and the Folds',
                  'The Mossgraves own the sheep runs and warrens of the Folds south of the city, and the looms that turn '
                  'their fleece into Ridgemere grey.', (700, 700), [(2, 2), (2, 3), (1, 2)], (150, 112)),
}
SUBURBS = {  # outside the gates: (name, the gate, the box)
    'southgate': ('Rainrun Bridge', 'S', (452, 584, 128, 104)),
    'eastgate': ('Eastgate Without', 'E', (648, 318, 96, 120)),
    'northgate': ('Cedar Row', 'N', (436, 112, 116, 78)),
}


class Ridgemere(City):
    """The rain city on its hills above the Sound."""

    def __init__(self, world, site):
        super().__init__(world, site, 1717, 'rm_', 'ridgemere')
        self.markets = {}

    def mask(self, x, y, w, h):
        m = np.zeros(self.c.codes.shape, dtype=bool)
        m[max(0, y):y + h, max(0, x):x + w] = True
        return m

    def build(self):
        w, c = self.world, self.c
        H, W = c.codes.shape
        ys, xs = np.mgrid[0:H, 0:W]
        box = self.mask(290, 186, 360, 404)
        land = box & ~w.water
        dist = self.distance_to_sea(box, 160)
        river = grow(w.river, 10)
        # The walled city, and the wharf between it and the water.
        outline = c.polygon_mask(WALL_OUTLINE)
        core = outline & land & (dist >= WHARF_WIDTH) & ~river
        core = blur_mask(core, 4) & land & (dist >= WHARF_WIDTH - 2) & ~river
        core = w_components(core, (480, 380))
        wharf = land & (dist < WHARF_WIDTH + 2) & grow(core, WHARF_WIDTH + 6) & ~core & ~river & (ys > 200) & (ys < 580)
        wharf = w_components(wharf, (320, 400))
        self.core, self.wharf = core, wharf
        # The tiers: rising from the water toward the north-east, never more than a step between neighbours; the
        # band inside the west wall stays on the first tier, so the water gates are a step up from the wharf.
        wob = (field.fbm(W, H, 30, 1719, 2) - .5) * .08
        u, v = (xs - 335) / 300, (575 - ys) / 375
        low, high = .75 * u + .25 * v + wob, .5 * u + .5 * v + wob    # Down by the water; up to the north-east.
        tier = np.where(low < .36, 1, np.where(high > .62, 3, 2)).astype(np.int16)
        tier[xs < 362] = 1
        tier[~core] = 99
        tier[wharf] = 0
        for _ in range(10):
            low = tier.copy()
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                low = np.minimum(low, np.roll(tier, (dy, dx), (0, 1)))
            tier = np.where(core, np.minimum(tier, low + 1), tier)
        self.tier = np.where(core | wharf, tier, -1)
        base = w.flatten_mask(core | wharf, 14)
        self.base = base
        self.lift_around(core | wharf, base)
        c.paint(core, 'f', None)
        c.paint(wharf, '.', base)
        c.heights[core] = (base + self.tier)[core]
        w.places['ridgemere'] = core | wharf
        w.levels['ridgemere'] = base + 2
        # The wall, its towers and its gates: three to the country, three water gates onto the wharf.
        wall = self.ring(core, 3)
        self.wall = wall
        c.paint(wall, '#', None)
        gates = {}
        for side, target in (('N', (476, 180)), ('E', (660, 380)), ('S', (500, 594))):
            out, inside = self.gate(core, wall, target, side, 7, 3, None, key=('ridgemere', side))
            gates[side] = (out, inside)
        water_gates = []
        for y in (276, 404, 506):
            out, inside = self.gate(core, wall, (300, y), 'W', 5, 3, None)
            water_gates.append((out, inside))
            # The gateway's floor half a step above the wharf and half below the first tier: an archway is no ramp,
            # so a whole step either side would leave the wharf cut off from the city.
            near = (np.abs(xs - out[0]) <= 12) & (np.abs(ys - out[1]) <= 9) & (c.codes == ord('G'))
            c.heights[near] = base + .5
        # Towers where the wall turns, after the gates and clear of them (a tower on a gate would wall it up again).
        openings = [p for g in list(gates.values()) + water_gates for p in g]
        for tx, ty in self.corners(wall, 7):
            if any(math.dist((tx, ty), p) < 16 for p in openings):
                continue
            m = self.mask(tx - 3, ty - 3, 7, 7) & core
            c.codes[m] = ord('#')
            c.locked[m] = True
        self.gates_in = {s: g[1] for s, g in gates.items()}
        self.spots.update({'north_gate': gates['N'][1], 'east_gate': gates['E'][1], 'south_gate': gates['S'][1]})
        self.spots['water_gates'] = [g[1] for g in water_gates]
        inner = core & ~grow(wall, 1)
        self.inner = inner
        self.districts = dict(DISTRICTS)
        # Squares and avenues: from each gate to the City Office square, the markets on the way; a road inside the
        # wall all the way round.
        squares = np.zeros_like(core)
        for key, ((sx, sy), (hx, hy)) in SQUARES.items():
            squares |= inner & (np.abs(xs - sx) <= hx) & (np.abs(ys - sy) <= hy)
        office = SQUARES['office'][0]
        lines = [[gates['N'][1], (474, 250), office], [gates['E'][1], (560, 372), office],
                 [gates['S'][1], (512, 520), SQUARES['commons_market'][0], (494, 412), office],
                 [water_gates[0][1], (420, 290), office], [water_gates[1][1], SQUARES['sump_market'][0], (430, 380), office],
                 [water_gates[2][1], (400, 520), SQUARES['commons_market'][0]],
                 [SQUARES['heights_market'][0], (496, 326), office], [SQUARES['heights_market'][0], (566, 300), (604, 330)],
                 [(360, 470), (452, 470), SQUARES['commons_market'][0]]]
        avenues = self.inner_ring(inner, 3)
        for pts_ in lines:
            avenues |= c.line_mask(pts_, 5)
        avenues = (avenues | squares) & inner
        # Tier edges: retaining walls, with stairs wherever a street crosses (painted once the streets are cut).
        up = np.zeros_like(core)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            up |= np.roll(self.tier, (dy, dx), (0, 1)) < self.tier
        up &= inner & (self.tier > 0)
        c.paint(avenues & ~up, '_', None)
        for key, ((sx, sy), _) in SQUARES.items():
            self.spots[key] = (sx, sy)
        c.stamp(office[0] - 1, office[1] - 1, ['FFF', 'FSF', 'FFF'], None)
        for dx in (-12, 12):
            c.stamp(office[0] + dx, office[1] - 7, ['S'], None)
        for key in MARKETS:
            (sx, sy), _ = SQUARES[key]
            self.market(key, sx, sy)
        # House Greatly's walled house and gardens at the top of the Heights, before anything else is placed.
        # Buildings, zone by zone: a block never straddles two tiers.
        self.greatly_region = np.zeros_like(core)
        self.greatly(inner & ~up & ~avenues & (self.tier == 3))
        blocked = up | grow(wall, 1) | self.built | grow(self.greatly_region, 2)
        zones = self.zones(inner)
        wanted = self.wanted()
        streets_all = avenues.copy()
        for name, (k, spacing, width) in ZONES.items():
            ground = zones[name] & ~blocked & ~grow(self.built, 1)
            c.codes[ground & ~avenues] = ord(ZONE_GROUND[name])
            lanes = self.lanes(ground, spacing, width, angle=-.12, offset=(k * 5, k * 3))
            lots, streets = self.district(ground, avenues & ground, lanes & ~up, wanted[name], jitter=14)
            streets_all |= streets
            left = lots.open & ground
            if name == 'heights':
                self.yards(left, ',', extra=(('B', .05), ('Y', .03), ('3', .03), ('F', .002), ('S', .001)))
            elif name == 'works':
                self.yards(left, '>', extra=(('x', .012), ('O', .01), ('<', .01), ('7', .004)))
            elif name == 'sump':
                self.yards(left, '.', extra=(('x', .006), ('O', .004), ('D', .03), ('|', .006)))
            else:
                self.yards(left, ',', extra=(('Y', .012), ('B', .012), ('U', .002), ('3', .008)))
        stairs = up & grow(streets_all, 1)
        c.paint(up & ~stairs, '#', None)
        c.paint(stairs, '^', None)
        self.streets = streets_all
        # The wharf and the docks outside the west wall, the suburbs outside the gates, the Houses' estates.
        self.waterfront(dist, base)
        for key, (name, side, box_) in SUBURBS.items():
            self.suburb(key, name, side, box_)
        self.estates()
        self.patrols()
        w.city_cells = self.city_cells()
        w.no_road = self.built.copy()
        return self

    # -- ground --------------------------------------------------------------------------------------------------
    def lift_around(self, region, base, reach=34):
        """The land round the city rises to meet its tiers: within `reach` tiles each tile eases toward the height of
        the nearest tier, so the walls stand on their hill rather than on a step."""
        c = self.c
        ys, xs = np.nonzero(region)
        y0, y1 = max(0, ys.min() - reach - 2), min(c.height, ys.max() + reach + 3)
        x0, x1 = max(0, xs.min() - reach - 2), min(c.width, xs.max() + reach + 3)
        t = np.where(region[y0:y1, x0:x1], self.tier[y0:y1, x0:x1], -1).astype(np.int16)
        known = t >= 0
        for _ in range(reach):                       # Each unknown tile takes a neighbour's tier.
            if known.all():
                break
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                shifted = np.roll(t, (dy, dx), (0, 1))
                fill = ~known & (shifted >= 0)
                t[fill] = shifted[fill]
                known |= fill
        near = field.distance_to_mask(region[y0:y1, x0:x1], reach + 1)
        weight = 1 - field.smoothstep(4, reach, near)
        target = base + np.maximum(t, 0)
        sub = c.heights[y0:y1, x0:x1]
        free = ~c.locked[y0:y1, x0:x1] & ~self.world.water[y0:y1, x0:x1] & (t >= 0)
        sub[:] = np.where(free, half(sub * (1 - weight) + target * weight), sub)

    def distance_to_sea(self, box, limit=120):
        """Distance to the Sound (capped), on the box's window only: the river doesn't make a wharf."""
        ys, xs = np.nonzero(box)
        y0, y1 = max(0, ys.min() - limit), min(self.c.height, ys.max() + limit + 1)
        x0, x1 = max(0, xs.min() - limit), min(self.c.width, xs.max() + limit + 1)
        out = np.full(self.c.codes.shape, limit, dtype=np.float32)
        out[y0:y1, x0:x1] = field.distance_to_mask(self.world.ocean[y0:y1, x0:x1], limit)
        return out

    def zones(self, inner):
        ys, xs = np.mgrid[0:self.c.height, 0:self.c.width]
        t = self.tier
        return {'works': inner & (t == 1) & (ys >= 446), 'sump': inner & (t == 1) & (ys < 446),
                'office': inner & (t == 2) & (ys < 420), 'commons': inner & (t == 2) & (ys >= 420),
                'heights': inner & (t == 3)}

    def market(self, key, cx, cy):
        """Two facing rows of four two-tile stalls across a square, the stallholders' places behind them, a well."""
        c = self.c
        places = []
        for dy in (-5, 5):
            behind = 1 if dy > 0 else -1
            for dx in (-11, -5, 3, 9):
                c.stamp(cx + dx, cy + dy, ['uu'], None)
                places.append((cx + dx, cy + dy + behind))
        c.stamp(cx, cy, ['U'], None)
        self.spots[f'stalls_{key}'] = places
        self.spots.setdefault('stalls', []).extend(places)

    def corners(self, wall, spacing):
        """Tower spots: every so often round the wall."""
        pts = np.argwhere(wall)
        cy, cx = pts.mean(axis=0)
        angles = np.arctan2(pts[:, 0] - cy, pts[:, 1] - cx)
        out = []
        for a in np.linspace(-math.pi, math.pi, 16, endpoint=False):
            i = int(np.argmin(np.abs(np.angle(np.exp(1j * (angles - a))))))
            out.append((int(pts[i][1]), int(pts[i][0])))
        return out

    # -- House Greatly -------------------------------------------------------------------------------------------
    def greatly(self, heights_ground):
        """Greatly House: a walled garden at the top of the Heights, the house at its back, its gate on a street."""
        c = self.c
        gw, gh = HOUSES['greatly'][6]
        best = None
        for y in range(196, 330, 4):
            for x in range(470, 640 - gw, 4):
                area = heights_ground[y:y + gh, x:x + gw]
                if area.size and area.all():
                    score = math.dist((x + gw / 2, y + gh / 2), (586, 250))
                    if best is None or score < best[0]:
                        best = (score, x, y)
        if best is None:
            print('  ridgemere: no room for Greatly House')
            return
        _, x, y = best
        region = self.mask(x, y, gw, gh)
        level = float(c.heights[y + gh // 2, x + gw // 2])
        ring = self.ring(region, 2)
        c.paint(region, ',', level)
        c.paint(ring, 'H', level)
        # Its gate on the side facing the Heights Market.
        sx, sy = SQUARES['heights_market'][0]
        side = self.facing((x + gw / 2, y + gh / 2), (sx, sy))
        middle = {'N': (x + gw / 2, y - 6), 'S': (x + gw / 2, y + gh + 6), 'W': (x - 6, y + gh / 2),
                  'E': (x + gw + 6, y + gh / 2)}[side]
        _, gate_in = self.gate(region, ring, middle, side, 5, 2, level, key=('estate_greatly', side))
        self.world.places['estate_greatly'] = region
        ground = region & ~grow(ring, 1)
        sid = 'estate_greatly'
        avenues = self.estate_ground(sid, region, ground, level, gate_in)
        lots = Lots(ground & ~avenues, avenues)
        extra = [self.named('reading', 'The Greatly Counting House', 'Ledgers of every Greatly cargo and loan, clerks at '
                            'high desks, and a strongroom door that is never left unlocked.', 20, 12, f'{sid}_works',
                            'manor'),
                 self.named('warehouse', 'The Greatly Stables', 'Carriage horses groomed to a shine and the House\'s '
                            'black carriage, kept dry at any cost.', 18, 10, f'{sid}_works', 'ridgemere')]
        lots, _ = self.district(ground & lots.open, avenues, np.zeros_like(ground),
                                self.estate('greatly', sid, extra, 5, 6), jitter=6, lane_ch='d')
        self.gardens(sid, lots.open & ground, 'greatly')
        self.greatly_region = region
        self.spots['greatly_gate'] = gate_in

    # -- the wanted buildings --------------------------------------------------------------------------------------
    def wanted(self):
        S, rng = 'ridgemere', self.rng
        out = {k: [] for k in ZONES}
        # The City Office quarter.
        o = out['office']
        o.append(self.named('reading', 'The City Office', 'The heart of the city\'s business: long counters where '
                            'clerks take the tolls, the rents and the fines, issue the permits and keep the rolls of '
                            'every household. The Chancellor\'s door is at the back, and it is always shut.', 32, 18,
                            'office', S))
        o.append(self.named('senate', 'The Rain Hall', 'The Council of Houses sits here under a vaulted roof that has '
                            'leaked for two hundred years: tiered benches, the Chancellor\'s seat, and the four Houses\' '
                            'banners heavy with damp.', 30, 18, 'office', S))
        o.append(self.named('hearing', "The Magistrate's Court", 'Oak rails, a raised bench and a dock worn smooth by '
                            'the hands of the accused. Most cases here are debts, theft from the wharf, or both.', 22, 14,
                            'office', S))
        o.append(self.named('barracks', 'The Watch House', 'Bunks, wet cloaks steaming by the stove and a rack of '
                            'cudgels: the City Watch keeps the walls and, when it can, the peace.', 28, 14, 'office_e', S))
        o.append(self.a_works('arsenal', 'The City Arsenal', 'The Watch\'s wall crossbows are built and kept here, '
                              'and counted twice.', 'office_e', S, 3))
        o.append(self.named('guard', 'The Gaol', 'Cold cells behind iron-bound doors and a gaoler\'s table by the '
                            'stove. It is never empty.', 16, 10, 'office_e', S))
        o.append(self.named('reading', 'The Hall of Records', 'Deeds, wills, births and deaths in ledgers to the '
                            'rafters; the clerks find anything, for a fee.', 22, 12, 'office', S))
        o.append(self.named('chapel', 'The Chapel of the Tide', 'Salt-stained pews and a lamp that is never let out; '
                            'the families of the drowned light candles here.', 22, 16, 'harbour_hill', S))
        o.append(self.a_tavern('The Slate & Ledger', 'Clerks\' ale and pies at long tables, and every rumour from the '
                               'City Office by noon.', 'office', S))
        for trade, name, text in OFFICE_SHOPS:
            o.append(self.a_shop(trade, name, text, 'office', S))
        for i in range(44):
            o.append(self.a_house(f'{family_name(i)} House', rng.choice(RIDGEMERE_HOUSE_PROSE),
                                  'harbour_hill' if i % 2 else 'office', S, rng.choice([3, 3, 4, 4])))
        # The Commons: the middle town.
        m = out['commons']
        m.append(self.a_tavern('The Grey Gull Inn', 'The inn by the South Gate: dry beds, strong tea and a fire kept '
                               'high for travellers off the Southway.', 'commons_s', S, True,
                               'Narrow rooms under a slate roof, each with a bucket for the drip.'))
        m.append(self.a_tavern('The Black Tun', 'A brewers\' tavern: thick dark ale brewed with rainwater, which is to '
                               'say, local.', 'commons', S))
        m.append(self.named('healer', 'The House of Mending', 'Clean beds, boiled linen and a healer who has seen every '
                            'kind of cough the rain can give.', 18, 10, 'commons_e', S))
        m.append(self.named('reading', 'The Commons School', 'Benches, slates and a teacher with a voice like a gull: '
                            'the Commons\' children learn their letters and their sums here.', 18, 11, 'commons_e', S))
        for trade, name, text in COMMONS_SHOPS:
            m.append(self.a_shop(trade, name, text, 'commons', S))
        for i in range(52):
            m.append(self.a_house(f'{family_name(44 + i)} House', rng.choice(RIDGEMERE_HOUSE_PROSE),
                                  ['commons', 'commons_s', 'commons_e'][i % 3], S, rng.choice([3, 3, 4, 4, 5])))
        # The Heights.
        h = out['heights']
        h.append(self.a_tavern('The Rainbarrel', 'A gentlefolk\'s tavern: dark panelling, good wine from somewhere '
                               'dry, and a fire nobody has to pay for.', 'heights', S))
        h.append(self.named('guard', 'The Heights Watch Post', 'A tidy guard post where the Watch keeps an eye on the '
                            'better streets.', 14, 9, 'heights_e', S))
        for trade, name, text in HEIGHTS_SHOPS:
            h.append(self.a_shop(trade, name, text, 'heights', S))
        for i, family in enumerate(HEIGHTS_FAMILIES):
            lower, upper, stairs = villa(rng, 'manor', rng.choice([3, 4, 4]), rng.choice([1, 2, 2]))
            name = f'{family} House'
            h.append(self.building('villa', name, '', [lower, upper], ['heights', 'heights_n', 'heights_e'][i % 3],
                                   'manor', stairs, roof='Z', names=[name, f'{name}, the bedchambers'],
                                   texts=[f'The {family}s\' town house: {rng.choice(HEIGHTS_PROSE)}',
                                          'Bedchambers under the slates: curtained beds, a fire in each room and rain on '
                                          'the windows.'], scale=.66))
        # The Sump: the slums.
        s = out['sump']
        s.append(self.named('refectory', 'The Soup Kitchen', 'Long tables, a great pot and a queue that starts before '
                            'dawn. The Houses pay for it, and make sure everyone knows.', 22, 14, 'sump', S))
        s.append(self.named('healer', 'The Sump Infirmary', 'Straw pallets, boiled rags and too few hands: the Sump '
                            'brings its coughs, burns and crushed paws here.', 18, 10, 'sump', S))
        s.append(self.a_tavern('The Drowned Lantern', 'Low beams black with smoke, a fire that smells of wet wool, and '
                               'dockhands shouting over one another.', 'sump', S))
        s.append(self.a_tavern('The Bilge', 'A cellar tavern where the floor is always wet and the ale is always '
                               'cheap.', 'sump_n', S))
        for trade, name, text in SUMP_SHOPS:
            s.append(self.a_shop(trade, name, text, 'sump', S))
        for i in range(44):
            s.append(self.a_house(tenement_name(i), rng.choice(TENEMENT_PROSE),
                                  'sump' if i % 3 else 'sump_n', S, rng.choice([5, 6, 6]), kind='tenement'))
        # The works: the industry sector by the docks.
        k = out['works']
        for kind, name, text, hands in WORKS_SECTOR:
            k.append(self.a_works(kind, name, text, 'works' if hands > 3 else 'works_w', S, hands))
        k.append(self.a_tavern('The Slag & Anchor', 'A forgehands\' tavern: loud, hot and cheap, the floor gritty with '
                               'cinder.', 'works', S))
        for trade, name, text in WORKS_SHOPS:
            k.append(self.a_shop(trade, name, text, 'works', S))
        for i, name in enumerate(WORKS_LODGINGS):
            k.append(self.a_house(name, 'Workers\' lodgings of the works: sleeping places along every wall and the smell '
                                  'of the forges in everything.', 'works_w' if i % 2 else 'works', S, 6,
                                  kind='tenement'))
        return out

    # -- the wharf -------------------------------------------------------------------------------------------------
    def waterfront(self, dist, level):
        """The wharf between the wall and the Sound: a street along the wall and one along the water, warehouses and
        the Fish Hall on the north wharf, the Customs House in the middle, the docks to the south; piers out."""
        c, w = self.c, self.world
        ys, xs = np.mgrid[0:c.height, 0:c.width]
        wharf = self.wharf
        street = wharf & grow(self.core, 4) & ~self.core
        edge = wharf & (np.abs(dist - 6) <= 2)
        cross = self.lanes(wharf, (30, 400), 4, angle=.05)
        ground = wharf & (dist >= 3)
        c.paint(street | edge, '_', level)
        self.districts.update({'wharf_north': self.along(wharf, 270), 'wharf': self.along(wharf, 400),
                               'docks': self.along(wharf, 520), 'docks_s': self.along(wharf, 556)})
        lots, streets = self.district(ground & ~street & ~edge, (street | edge) & ground, cross, self.wharf_buildings(),
                                      jitter=10)
        self.piers(wharf, level, dist)
        open_ = lots.open & wharf
        grain = np.random.default_rng(88).random(open_.shape)
        c.codes[open_ & (grain < .04)] = ord('|')            # Drying racks and net poles.
        c.codes[open_ & (grain > .985)] = ord('x')
        c.codes[open_ & (grain > .975) & (grain <= .985)] = ord('O')
        c.codes[open_ & (ys > 490) & (grain > .94) & (grain <= .975)] = ord('<')    # Timber stacked by the docks.
        self.spots['wharf'] = self.districts['wharf']
        self.spots['docks'] = self.districts['docks']
        qs = np.argwhere(edge if edge.any() else street)
        qs = qs[np.argsort(qs[:, 0])]
        self.spots['wharf_walk'] = [(float(p[1]), float(p[0])) for p in qs[::max(1, len(qs) // 10)]]

    def along(self, region, y):
        pts = np.argwhere(region)
        i = int(np.argmin(np.abs(pts[:, 0] - y)))
        return float(pts[i][1]), float(pts[i][0])

    def wharf_buildings(self):
        S, b = 'works', []
        b.append(self.building('works', 'The Greatly Yard', 'House Greatly\'s shipyard: ' + WORKS['shipwright'][1] +
                               '. A hull stands half-planked on the slip outside.', [works(self.rng, S, 'shipwright', 5)],
                               'docks', S))
        b.append(self.building('works', 'The Ropewalk', 'The long shed of the docks: ' + WORKS['ropewalk'][1] + '.',
                               [works(self.rng, S, 'ropewalk', 4)], 'docks_s', S))
        b.append(self.building('works', 'The Fish Hall', 'The city\'s fish market: ' + WORKS['fishmarket'][1] + '.',
                               [works(self.rng, S, 'fishmarket', 4)], 'wharf_north', S))
        b.append(self.building('works', 'The North Wharf Smokehouse', 'A smokehouse on the wharf: ' +
                               WORKS['smokehouse'][1] + '.', [works(self.rng, S, 'smokehouse', 4)], 'wharf_north', S))
        b.append(self.building('works', 'Old Kettle Smokehouse', 'The oldest smokehouse on the wharf: ' +
                               WORKS['smokehouse'][1] + '.', [works(self.rng, S, 'smokehouse', 3)], 'wharf_north', S))
        b.append(self.named('reading', 'The Customs House', 'Every cask and bale that crosses the wharf is weighed, '
                            'stamped and taxed at these long counters.', 22, 12, 'wharf', 'ridgemere'))
        b.append(self.named('reading', "The Harbourmaster's Office", 'Tide tables, berth rolls and a window over the '
                            'whole Sound.', 14, 10, 'wharf', 'ridgemere'))
        for name, house, district in (('The Greatly Bonded Warehouse', 'Greatly', 'wharf'),
                                      ('The Grayrock Iron Store', 'Grayrock', 'docks'),
                                      ('The Fell Timber Store', 'Fell', 'docks'),
                                      ('The Mossgrave Wool Store', 'Mossgrave', 'wharf')):
            b.append(self.named('warehouse', name, f'{house} goods waiting for a ship: stacked, tallied and guarded.',
                                22, 12, district, 'ridgemere', roof='Z'))
        b.append(self.a_tavern('The Tarred Rope', 'A dock tavern built half on the wharf and half on old pilings; the '
                               'floor tilts and nobody minds.', 'wharf', 'ridgemere'))
        b.append(self.a_tavern('The Sailors\' Rest', 'A sailors\' inn on the wharf: hammocks upstairs, a long room below '
                               'and every tongue of every port.', 'wharf_north', 'ridgemere', True,
                               'Hammocks and sea chests in rows under the eaves.'))
        b.append(self.a_shop('chandler', 'Wickham\'s Chandlery', 'Tallow, pitch, oakum and rope: everything a ship or '
                             'a leaky roof needs.', 'wharf', 'ridgemere'))
        b.append(self.a_shop('fishmonger', 'The Wet Slab', 'Yesterday\'s catch on wet stone, and today\'s, if you are '
                             'early.', 'wharf_north', 'ridgemere'))
        for name in ('The Net Loft', 'Dockside Row', 'The Hulks'):
            b.append(self.a_house(name, 'Sailors\' and dockers\' lodgings over the wharf: hammocks, sea chests and the '
                                  'smell of tar.', 'docks', 'ridgemere', 6, kind='tenement'))
        return b

    def piers(self, wharf, level, dist):
        """Piers out over the Sound from the wharf's water edge."""
        c, w = self.c, self.world
        edge = wharf & grow(w.water, 1)
        pts = np.argwhere(edge)
        if not len(pts):
            return
        pts = pts[np.argsort(pts[:, 0])]
        made = []
        for y, x in pts[::max(1, len(pts) // 12)]:
            if any(math.dist((x, y), m) < 26 for m in made):
                continue
            win = w.water[y - 6:y + 7, x - 6:x + 7]
            wy, wx = np.argwhere(win).mean(axis=0) - 6 if win.any() else (0, -1)
            n = math.hypot(wx, wy) or 1
            dx, dy = wx / n, wy / n
            line = [(x + .5, y + .5), (x + .5 + dx * 26, y + .5 + dy * 26)]
            m = c.line_mask(line, 3) & (w.water | wharf) & ~c.locked
            c.paint(m, '8', level)
            head = (int(x + dx * 25), int(y + dy * 25))
            c.stamp(head[0], head[1], ['i'], level)
            made.append((x, y))
        self.spots['piers'] = made

    # -- outside the walls -----------------------------------------------------------------------------------------
    def suburb(self, key, name, side, box_):
        """Houses outside a gate along its road: level ground at the gate's height, a street from the gate through it,
        lanes off it, a tavern and a few shops."""
        c, w = self.c, self.world
        x, y, bw, bh = box_
        region = self.mask(x, y, bw, bh) & ~grow(w.water, 3) & ~self.core & ~grow(self.wall, 2)
        for other in w.places.values():
            region &= ~grow(other, 3)
        region = w_components(blur_mask(region, 3) & region, (x + bw / 2, y + bh / 2))
        gate_out = w.gates[('ridgemere', side)]
        level = w.gate_levels[('ridgemere', side)]
        ys, xs = np.nonzero(region)
        y0, y1, x0, x1 = ys.min() - 16, ys.max() + 17, xs.min() - 16, xs.max() + 17
        near = field.distance_to_mask(region[y0:y1, x0:x1], 15)
        k = 1 - field.smoothstep(0, 14, near)
        sub = c.heights[y0:y1, x0:x1]
        free = ~c.locked[y0:y1, x0:x1] & ~w.water[y0:y1, x0:x1]
        sub[:] = np.where(free, half(sub * (1 - k) + level * k), sub)
        c.paint(region, ',', level, lock=False)
        sid = f'suburb_{key}'
        w.places[sid] = region
        w.levels[sid] = level
        # Its street: the road out of the gate, straight through.
        vx, vy = {'N': (0, -1), 'S': (0, 1), 'W': (-1, 0), 'E': (1, 0)}[side]
        far = (gate_out[0] + vx * 200, gate_out[1] + vy * 200)
        street = c.line_mask([(gate_out[0] - vx * 6, gate_out[1] - vy * 6), far], 5) & region
        lanes = self.lanes(region, (52, 26), 3, angle=0) & region
        self.districts[sid] = (x + bw / 2, y + bh / 2)
        self.districts[f'{sid}_gate'] = gate_out
        c.paint(street, 'd', level)
        lots, streets = self.district(region & ~street, street, lanes, self.suburb_buildings(key, name, sid), jitter=18,
                                      lane_ch='d')
        self.yards(lots.open & region, ',', extra=(('Y', .015), ('B', .02), ('|', .01), ('4', .02)))
        self.spots[sid] = gate_out
        # The road still runs through it: roads route round places, so it is no longer one of them.
        del w.places[sid]
        self.suburb_masks = getattr(self, 'suburb_masks', []) + [region]

    def suburb_buildings(self, key, name, sid):
        S, rng = 'ridgemere', self.rng
        b = []
        extra = {'southgate': [self.a_tavern('The Bridge Lamp', 'A tavern on the Rainrun bridge: carters, drovers off '
                                             'the Southway and the smell of wet dogs.', f'{sid}_gate', S),
                               self.a_shop('smith', 'Southgate Farriers', 'Horseshoes, cart iron and a fire that never '
                                           'goes out.', f'{sid}_gate', S),
                               self.a_shop('provisioner', 'Bridge Stores', 'Sacks, rope and road food for the Southway.',
                                           sid, S)],
                 'eastgate': [self.a_tavern('The Last Lamp', 'The last tavern before the High Road: a long bar for '
                                            'drovers and a bench for whoever falls asleep.', f'{sid}_gate', S),
                              self.a_works('stables', 'The Eastgate Stables', 'Draught horses for every wagon that leaves '
                                           'by the High Road.', f'{sid}_gate', S, 2),
                              self.a_shop('saddler', 'Collar and Trace', 'Draught harness for the Houses\' carts, work '
                                          'harness for everyone else.', sid, S)],
                 'northgate': [self.a_shop('carpenter', 'Cedar Row Joiners', 'Doors, shutters and coffins, all in '
                                           'Rainwood cedar.', f'{sid}_gate', S)]}[key]
        b += extra
        count = {'southgate': 16, 'eastgate': 12, 'northgate': 9}[key]
        for i in range(count):
            family = SUBURB_FAMILIES[(sum(map(ord, key)) + i * 7) % len(SUBURB_FAMILIES)]
            b.append(self.a_house(f'{family} House', rng.choice(SUBURB_PROSE), sid, S, rng.choice([3, 4, 4, 5])))
        # (Each suburb's family names are its own: the counter steps through them.)
        seen = set()
        for x in b:
            while x.kind == 'house' and x.name in seen:
                x.name = x.name.replace(' House', ' Cottage')
            seen.add(x.name)
        return b

    # -- the Houses' estates -------------------------------------------------------------------------------------
    def estates(self):
        w = self.world
        taken = w.places['ridgemere'].copy()
        for other in w.places.values():
            taken |= grow(other, 6)
        for m in getattr(self, 'suburb_masks', []):
            taken |= grow(m, 10)
        taken |= grow(w.river, 6)
        gate_for = {'fell': 'N', 'grayrock': 'E', 'mossgrave': 'S'}
        for house, (seat, _, _, _, anchor, cells, (ew, eh)) in HOUSES.items():
            if house == 'greatly':
                continue
            spot = None
            for scale in (1, .9, .8):
                bw, bh = int(ew * scale), int(eh * scale)
                for cell in cells:
                    try:
                        spot = self.find_box(cell, bw, bh, anchor, taken)
                        break
                    except ValueError:
                        continue
                if spot:
                    break
            if spot is None:
                raise ValueError(f'No room for House {house.title()}\'s estate')
            x, y = spot
            sid = f'estate_{house}'
            toward = w.gates[('ridgemere', gate_for[house])]
            side = self.facing((x + bw / 2, y + bh / 2), toward)
            middle = {'N': (x + bw / 2, y - 6), 'S': (x + bw / 2, y + bh + 6), 'W': (x - 6, y + bh / 2),
                      'E': (x + bw + 6, y + bh / 2)}[side]
            region, ground, lvl, insides = self.compound(sid, x, y, bw, bh, 'H', ',', {side: middle})
            taken |= grow(region, 8)
            avenues = self.estate_ground(sid, region, ground, lvl, insides[side])
            lots = Lots(ground & ~avenues, avenues)
            lots, _ = self.district(ground & lots.open, avenues, np.zeros_like(ground),
                                    self.estate(house, sid, self.estate_extra(house, sid), 5, 6), jitter=8, lane_ch='d')
            self.gardens(sid, lots.open & ground, house)
            self.roads.append((f'The {house.title()} Drive', [f'{sid}.{side}', f'ridgemere.{gate_for[house]}'], False))
            self.spots[f'{sid}_gate'] = insides[side]
            print(f'  ridgemere: {seat} at ({x}, {y}), {bw}x{bh}')

    def estate_extra(self, house, sid):
        S = 'ridgemere'
        b = [self.named('warehouse', f'The {house.title()} Stables', 'Horses, carriages and the smell of wet straw; '
                        'the House\'s coach waits here polished for the Council.', 20, 11, f'{sid}_works', S),
             self.a_house(f'The {house.title()} Gatehouse', 'The gatekeeper\'s lodge: a hearth, a bed and a window on '
                          'the drive.', f'{sid}_works', S, 2),
             self.a_house(f'{house.title()} Cottages', 'Estate cottages for the gardeners and grooms, whitewashed once '
                          'and grey now.', f'{sid}_works', S, 5, kind='tenement'),
             self.named('chapel', f'The {house.title()} Summerhouse', 'A pillared garden house among the flowerbeds, '
                        'where the family takes tea when the rain allows.', 14, 10, f'{sid}_seat', 'manor')]
        if house == 'grayrock':
            b.append(self.named('reading', 'The Quarry Office', 'Grayrock\'s office for its quarries in the Rainward '
                                'Hills: tally tables, maps of the faces, and stone samples on every shelf.', 14, 10,
                                f'{sid}_works', S))
        if house == 'fell':
            b.append(self.named('reading', 'The Fell Timber Office', 'Where Fell sells its cedar by the board-foot and '
                                'tallies the logging camps.', 14, 10, f'{sid}_works', S))
        if house == 'mossgrave':
            b.append(self.named('warehouse', 'The Mossgrave Fleece Barn', 'Bales of Folds fleece waiting for the looms, '
                                'and the smell of lanolin.', 18, 10, f'{sid}_works', S))
        return b

    def estate(self, house, sid, extra, family, servants):
        """A House's seat (a manor: great hall, the family's chambers above, the kitchens and servants' hall below)
        and its other buildings."""
        seat, _, _, blurb, _, _, _ = HOUSES[house]
        lower, upper, below, stairs = manor(self.rng, 'manor', family, servants)
        hall_text = f'The great hall of {seat}: the House of {house.title()}\'s high seat under its banners. {blurb}'
        b = [self.building('manor', seat, hall_text, [lower, upper, below], f'{sid}_seat', 'manor', stairs,
                           names=[seat, f'{seat}, the family\'s chambers', f'{seat}, kitchens and servants\' hall'],
                           texts=[hall_text, 'Private chambers above the hall: heavy curtains against the rain, a fire '
                                  'in every room, the family\'s beds and chests.', 'Kitchens, pantries and the '
                                  'servants\' bunks, warm from the ovens and never quiet.'])]
        return b + extra

    def gardens(self, sid, open_ground, house):
        """The grounds of a great House, laid out on the axis from its gate to its house: a drive between trees; the
        kitchen garden and orchard on one side by the gate; a parterre of clipped hedges round flowerbeds, statues at
        its corners, halfway up; a gravel forecourt with a fountain before the house; a pond with a temple on its island
        on the other side; lawns and specimen trees about the house; a belt of woodland inside the wall."""
        c = self.c
        place = self.world.places[sid]
        (ax, ay), (bx, by) = self.districts[f'{sid}_works'], self.districts[f'{sid}_seat']
        length = math.hypot(bx - ax, by - ay) or 1
        ux, uy = (bx - ax) / length, (by - ay) / length
        ys, xs = np.mgrid[0:c.height, 0:c.width]
        t = (xs - ax) * ux + (ys - ay) * uy                      # Along the axis, from the gate.
        side = -(xs - ax) * uy + (ys - ay) * ux                  # Across it.
        width = float(np.abs(side[place]).max())
        rng = np.random.default_rng(sum(map(ord, sid)))
        grain = rng.random(open_ground.shape)
        g = open_ground
        c.codes[g] = ord(',')
        tree = '$P' if house == 'fell' else 'YY$' if house == 'mossgrave' else 'Y'
        kinds = np.array([ord(ch) for ch in tree], dtype=np.uint8)

        def trees(mask):
            c.codes[mask] = kinds[(grain[mask] * 997).astype(int) % len(tree)]
        # The woodland belt inside the wall, with a walk through it.
        belt = g & ~shrink(place, 9 if house != 'greatly' else 6)
        walk = belt & shrink(place, 5) & ~shrink(place, 6)
        trees(belt & ~walk & (grain < .55))
        c.codes[belt & ~walk & (grain >= .55) & (grain < .62)] = ord('&' if house == 'fell' else 'B')
        c.codes[walk] = ord('.')
        inner = g & ~belt
        # The kitchen garden and the orchard, by the gate on the working side.
        kitchen = inner & (t < .34 * length) & (side > 7)
        rows = kitchen & ((np.round(t).astype(int) % 4) == 0)
        c.codes[kitchen & ~rows & (side < width * .55)] = ord('4')
        orchard = kitchen & (side >= width * .55)
        c.codes[orchard & ((np.round(t).astype(int) % 3) == 0) & ((np.round(side).astype(int) % 3) == 0)] = ord('Y')
        c.codes[rows & ~orchard] = ord('.')
        # The parterre: beds of flowers in clipped hedges either side of the drive, gravel between.
        parterre = inner & (t > .4 * length) & (t < .72 * length) & (np.abs(side) > 4) & (np.abs(side) < width * .62)
        ti, si = np.round(t).astype(int), np.round(np.abs(side)).astype(int)
        hedge = parterre & (((ti % 8) == 0) | ((si % 7) == 0))
        gravel = parterre & (((ti % 8) == 1) | ((si % 7) == 1))
        c.codes[parterre] = ord('3')
        c.codes[parterre & ~hedge & ~gravel & (grain < .3)] = ord(',')
        c.codes[gravel] = ord('f')
        c.codes[hedge] = ord('B')
        corners = parterre & ((ti % 16) == 0) & ((si % 14) == 0)
        c.codes[corners] = ord('S')
        # The forecourt: gravel round a fountain before the house.
        fx, fy = ax + ux * .8 * length, ay + uy * .8 * length
        court = inner & (np.hypot(xs - fx, ys - fy) <= 8)
        c.codes[court] = ord('f')
        fountain = court & (np.hypot(xs - fx, ys - fy) <= 1.6)
        c.codes[fountain] = ord('F')
        c.codes[int(round(fy)), int(round(fx))] = ord('S')
        # The pond, on the side away from the kitchen garden, with a temple on its island.
        px, py = ax + ux * .25 * length + uy * width * .5, ay + uy * .25 * length - ux * width * .5
        r = max(5, min(12, width * .22))
        d = np.hypot((xs - px) / (r * 1.5), (ys - py) / r)
        pond = inner & (d <= 1)
        c.codes[pond] = ord('~')
        c.codes[pond & (d <= .55)] = ord('W')
        c.codes[inner & ~pond & (d <= 1.25)] = np.where(grain[inner & ~pond & (d <= 1.25)] < .4, ord('E'), ord(','))
        island = pond & (d <= .2)
        c.codes[island] = ord(',')
        if island.any():
            iy, ix = np.argwhere(island)[0]
            c.codes[iy, ix] = ord('I')
        # About the house: lawns, specimen trees and flowerbeds.
        lawns = inner & ~kitchen & ~parterre & ~court & (d > 1.25)
        trees(lawns & (grain < .035))
        c.codes[lawns & (grain > .975)] = ord('3')
        c.codes[lawns & (grain > .965) & (grain <= .975)] = ord('B')
        # Trees along the drive.
        drive = self.drives.get(sid)
        if drive is not None:
            beside = grow(drive, 1) & ~drive & g & ~court & ((xs + ys) % 3 == 0)
            trees(beside)

    # -- patrols and the city's cells ------------------------------------------------------------------------------
    def patrols(self):
        ring_pts = np.argwhere(self.inner_ring(self.inner, 3) & ~self.built)
        cy, cx = ring_pts.mean(axis=0)
        angles = np.arctan2(ring_pts[:, 0] - cy, ring_pts[:, 1] - cx)
        self.spots['wall_walk'] = [(float(ring_pts[i][1]), float(ring_pts[i][0])) for i in
                                   [int(np.argmin(np.abs(np.angle(np.exp(1j * (angles - a))))))
                                    for a in np.linspace(-math.pi, math.pi, 16, endpoint=False)]]
        # Through the middle town: the squares, the markets and the gates.
        self.spots['street_walk'] = [self.gates_in['N'], SQUARES['heights_market'][0], SQUARES['office'][0],
                                     self.gates_in['E'], SQUARES['commons_market'][0], self.gates_in['S'],
                                     SQUARES['sump_market'][0], self.spots['water_gates'][1], SQUARES['office'][0]]

    def city_cells(self):
        """The cells the city and its suburbs mostly fill: named Ridgemere."""
        region = self.world.places['ridgemere'].copy()
        for m in getattr(self, 'suburb_masks', []):
            region |= m
        out = set()
        for cy in range(self.c.height // CELL):
            for cx in range(self.c.width // CELL):
                if region[cy * CELL:(cy + 1) * CELL, cx * CELL:(cx + 1) * CELL].mean() > .12:
                    out.add((cx, cy))
        return out


def w_components(mask, seed):
    from .realm import components
    x, y = int(seed[0]), int(seed[1])
    if not mask[y, x]:
        ys, xs = np.nonzero(mask)
        i = int(np.argmin((xs - x) ** 2 + (ys - y) ** 2))
        x, y = int(xs[i]), int(ys[i])
    return components(mask, [(x, y)]) >= 0


OFFICE_SHOPS = [
    ('scribe', 'Wetink Lane Scriveners', 'Contracts, debts and letters home, in ink that never quite dries.'),
    ('printer', 'The Council Press', 'Proclamations of the Council of Houses, tide tables and the broadsheet everyone '
     'pretends not to read.'),
    ('cartographer', 'Soundings', 'Charts of the Sound, the coast and every road out of Ridgemere.'),
    ('chandler', 'The Lamp & Wick', 'Candles and lamp oil for the City Office, which burns through both.'),
    ('apothecary', 'The Cough Jar', 'Remedies for the rain lung, the forge burn and the wharf fever.'),
]
COMMONS_SHOPS = [
    ('baker', 'Grist & Crust', 'Dense dark loaves that keep for a week in the damp.'),
    ('baker', 'The Morning Oven', 'Rolls, buns and the smell that gets the Commons out of bed.'),
    ('butcher', 'The Hook and Cleaver', 'Mutton from the Folds, rabbit by the brace, sausages by the yard.'),
    ('smith', 'Nailers\' Forge', 'Nails, hinges, chain: small iron by the barrel.'),
    ('cooper', 'Staves & Hoops', 'Barrels for fish, salt, pitch and ale.'),
    ('tailor', 'Oilcloth & Wool', 'Oiled cloaks, patched coats and boots that nearly keep the water out.'),
    ('weaver', 'Greywool', 'Heavy Mossgrave wool for heavy wet weather.'),
    ('general', 'Commons Sundries', 'Thread, soap, lamp wicks and a barrel of everything else.'),
    ('potter', 'The Rain Pot', 'Crocks, jugs and the rain barrels every house needs.'),
    ('carpenter', 'Pennock & Sons', 'Tables, shutters and the odd coffin.'),
    ('brewer', 'The Hopyard', 'Small beer for the Commons\' tables and strong ale for its taverns.'),
    ('herbalist', 'Moss & Marrow', 'Rain-lung remedies and burn salve.'),
    ('tinker', 'Mend & Make Do', 'Pots, locks and lamps put right, or near enough.'),
    ('provisioner', 'Southway Provisions', 'Flour, oats, dried peas and salt pork by the sack.'),
    ('fletcher', 'The Grey Goose', 'Arrows for the Watch and the Houses\' hunts.'),
    ('general', 'Rainford\'s Store', 'A little of everything, a lot of gossip.'),
    ('butcher', 'Mutton Lane Butchers', 'Folds mutton and rabbit pies.'),
    ('baker', 'The Wet Loaf', 'Barley bread and oatcakes for the south end.'),
    ('smith', 'Southgate Ironmongers', 'Pots, pans, grates and nails.'),
    ('cooper', 'The Bung Hole', 'Small casks and buckets.'),
    ('tailor', 'Needle & Drip', 'Mending, mostly.'),
]
HEIGHTS_SHOPS = [
    ('jeweler', 'Silver & Rain', 'Jet, silver and river pearls for the Heights\' finest.'),
    ('perfumer', 'The Cedar Phial', 'Cedar, rose and costly musk: something to smell of besides wet wool.'),
    ('tailor', 'Vane & Daughters', 'Fine dark cloth cut to the Houses\' fashion.'),
    ('glassblower', 'Clearwater Glass', 'Goblets, spectacles and window glass for the better houses.'),
    ('moneychanger', 'The Greatly Bank', 'House Greatly\'s bank: loans, letters of credit, and a vault under the floor.'),
    ('apothecary', 'Hollis Physic', 'Tonics and powders for those who can pay for them.'),
    ('baker', 'The Sugar Loaf', 'Cakes, tarts and white bread for the Heights\' tables.'),
]
SUMP_SHOPS = [
    ('general', 'Sump Sundries', 'Candle ends, string, salt fish, rags: the Sump\'s own shop.'),
    ('moneychanger', 'The Pawn and Ledger', 'Loans against anything, at rates everyone complains about.'),
    ('fishmonger', 'Gullgut Fish', 'Heads, tails and whatever the wharf could not sell.'),
    ('provisioner', 'Harbour Provisions', 'Ship\'s biscuit, salt pork and dried peas by the sack.'),
]
WORKS_SECTOR = [  # kind, name, text, hands
    ('ironworks', 'The Grayrock Ironworks', 'Grayrock\'s forge-hall: the roar never stops, rain or no rain.', 6),
    ('foundry', 'The Grayrock Foundry', 'Bronze for fittings and the harbour bells, iron for the Houses\' gates.', 3),
    ('sawmill', 'The Fell Sawmill', 'The great saw of House Fell, driven by a wheel on the Rainrun\'s last fall.', 6),
    ('weaving_shed', 'The Mossgrave Wool Shed', 'Mossgrave fleece spun and woven into Ridgemere grey by the bolt.', 4),
    ('dyeworks', 'The Blue Vats', 'Woad-blue for the Watch\'s cloaks and grey for everyone else.', 3),
    ('tannery', 'The Wharf Tannery', 'Hides off the ships and the Folds, bark from the Rainwood, lime from Grayrock.', 4),
    ('glassworks', 'The Cinder Glassworks', 'Window glass for half the city, from Fell charcoal and wharf sand.', 4),
    ('brickworks', 'The Tileworks', 'Slate is dear; kiln tile is cheap.', 3),
    ('brewery', 'The Rainwater Brewery', 'The city\'s ale, brewed by the vat and carted out by the barrel.', 3),
    ('papermill', 'The Rag Mill', 'The Sump sells its rags here; the City Office buys the paper back.', 3),
    ('cartwright', 'The Wagon Yard', 'Wagons for the High Road and the Southway, and carts for everyone.', 3),
    ('limeworks', 'The Lime Kilns', 'Grayrock limestone burnt for the mortar of the whole city.', 2),
    ('kilnhouse', 'The Charcoal Store', 'Fell charcoal for the forges and the glassworks.', 3),
    ('saltworks', 'The Salt Pans', 'Sea brine boiled grey over coal fires; the salt that keeps the smokehouses going.', 3),
]
WORKS_SHOPS = [
    ('smith', 'The Anchor Forge', 'Chain, anchors and boat-iron for the docks.'),
    ('armorer', 'The Rivet and Rain', 'Brigandines and kettle helms for the Watch and the Houses\' guards; oiled against '
     'rust twice a day.'),
    ('saddler', 'The Harness Shop', 'Work harness for the wagon yard\'s horses.'),
]
WORKS_LODGINGS = ['Forge Row', 'Cinder Court', 'Sawdust Yard', 'The Long Shed', 'Vat Court', 'Lime Yard', 'Pan Row',
                  'Rag Court', 'Kiln Row', 'The Barracks']

def family_name(i):
    """The i-th family name of the middle town: the common names first, then made ones, never repeating."""
    if i < len(RIDGEMERE_FAMILIES):
        return RIDGEMERE_FAMILIES[i]
    j = i - len(RIDGEMERE_FAMILIES)
    return MADE_HEADS[(j * 7) % len(MADE_HEADS)] + MADE_TAILS[(j * 5 + j // len(MADE_HEADS)) % len(MADE_TAILS)]


def tenement_name(i):
    """The i-th tenement's name: Soot Row, The Drip...; then Soot Row Back, and so on."""
    name = TENEMENT_NAMES[i % len(TENEMENT_NAMES)]
    return name if i < len(TENEMENT_NAMES) else f'{name} Back' if i < 2 * len(TENEMENT_NAMES) else f'{name} {i}'


MADE_HEADS = ['Ash', 'Black', 'Brack', 'Cold', 'Dun', 'Fen', 'Grey', 'Hal', 'Kettle', 'Lang', 'Moss', 'Nether', 'Rain',
              'Salt', 'Thorn', 'Wet', 'Wick', 'Mire', 'Stone', 'Tarn']
MADE_TAILS = ['by', 'wick', 'ford', 'well', 'ham', 'ley', 'ridge', 'cote', 'more', 'stead', 'worth', 'dale', 'combe']
TENEMENT_NAMES = ['Soot Row', 'The Drip', 'Tallow Court', 'Cinder Stair', 'The Rookery', 'Brine Yard', 'Gullwing Row',
                  'The Leaks', 'Fishgut Lane', 'Coalhole Court', 'Rope Alley', 'The Warrens', 'Bilge Row',
                  'Rust Court', 'Lantern Yard', 'Mudside', 'Wet Stair', 'Gutter Row', 'Crowsfoot Court', 'The Sinks',
                  'Rag Row', 'Tarpot Yard', 'The Hollows', 'Weeping Court', 'Old Pump Row', 'Drain Alley']
TENEMENT_PROSE = [
    'A tenement crammed to the rafters: sleeping mats in every corner, washing strung across the room, one hearth.',
    'Damp walls, a smoking stove and too many families for the floor. Someone is always coughing.',
    'A rented warren of curtained-off corners, each one somebody\'s whole home.',
]
RIDGEMERE_HOUSE_PROSE = [
    'A narrow house of black stone, its hearth kept high against the damp.',
    'A craftsman\'s house above a workroom, smelling of wet wool and woodsmoke.',
    'Two rooms, a slate roof that mostly holds, and a rain barrel by the door.',
    'A tidy clerk\'s house: a good table, a clock that is always right, and boots drying by the fire.',
]
RIDGEMERE_FAMILIES = ['Tarrow', 'Slagwick', 'Coldharbour', 'Nettleby', 'Drayman', 'Kettleby', 'Rainford', 'Hollis',
                      'Marrowby', 'Cobbett', 'Pennock', 'Wetherell', 'Ashdown', 'Blackwood', 'Brindle', 'Carrow',
                      'Dunmore', 'Elsworth', 'Farrow', 'Gilbey', 'Halloway', 'Inchley', 'Jessop', 'Kendrick', 'Lumley',
                      'Mallory', 'Netherby', 'Oakes', 'Pickering', 'Quarles', 'Redfern', 'Sallow', 'Thackeray',
                      'Underhill', 'Varley', 'Wainwright', 'Yardley', 'Abbot', 'Bramhall', 'Catterick', 'Dimmock',
                      'Eldridge', 'Fenwick', 'Gorringe', 'Hatherley', 'Ingram']
HEIGHTS_FAMILIES = ['Ashby', 'Wexley', 'Thornbury', 'Calloway', 'Ravensworth', 'Harrowgate', 'Pemberton', 'Lockwood',
                    'Stanmore', 'Vance', 'Aldwych', 'Beaumont', 'Carlisle', 'Dorrington', 'Everard', 'Fairfax',
                    'Grantham', 'Hawthorne']
HEIGHTS_PROSE = ['a marble hall, a fountain in the courtyard and portraits of the family looking down on visitors.',
                 'tall windows over the Sound, a salon of dark wood and a fire that never goes out.',
                 'a house of dressed stone with a garden behind high walls, quiet but for the rain.']
SUBURB_FAMILIES = ['Ash', 'Barrow', 'Cotton', 'Dale', 'Esk', 'Ford', 'Gale', 'Heath', 'Ives', 'Judd', 'Knowles', 'Lane',
                   'Marsh', 'Noakes', 'Orme', 'Platt', 'Rook', 'Shaw', 'Tull', 'Vane', 'Webb', 'Yeo', 'Birch', 'Croft',
                   'Dunn', 'Frith', 'Gill', 'Holt', 'Kemp', 'Lowe', 'Moss', 'Nash', 'Pike', 'Reed', 'Stone', 'Toft',
                   'Wren']
SUBURB_PROSE = ['A cottage outside the walls: cheaper rent, a patch of garden and a long walk to work.',
                'A low house by the road, its roof green with moss and a dog asleep in the door.',
                'Two rooms and a lean-to, the family\'s goat tethered at the back.']


def build_quarters(world):
    """(The old works quarters outside the walls: Ridgemere keeps its industry inside them now.)"""


def build(world):
    """Design Ridgemere on the world's canvas. Returns (site, [cities], extra roads)."""
    site = CitySite(world.c, world.reserved_ids)
    cities = [Ridgemere(world, site).build()]
    roads = [r for city in cities for r in city.roads]
    return site, cities, roads
