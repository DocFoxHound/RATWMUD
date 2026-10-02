"""The two great cities of the western world, designed on the world canvas the western generator lays out.

Ridgemere (Ridgemere Heights, North West) is a rain-soaked medieval industrial city on a harbour. Its walls enclose
only the old city: the government quarter (the Council of Houses, the Magistrate's Court, the Watch), a market and
the Sump, its slums. The quay outside the walls holds the docks, the shipyard and the smokehouses; the great Houses
own walled estates beyond, each with its own industry: Grayrock (quarry and ironworks), Fell (timber and the
sawmill), Ashcombe (charcoal and glass), Brinewater (ships and rope) and Vesk (fish, smoke and salt).

Ser Ferro (The Ser Ferro Marches, South) is a bright city of white walls and red roofs on the great river, built in
tiers that rise from the wharf, where its few slums are, through the middle-class town and the Cathedral Rise to the
elite quarter and, on the east side at the top, the King's palace and court.

Buildings are placed with the Upper Accord kit (buildings.py, site.py): each has its interiors, doors and the beds and
work places its residents are given (citizens.py).
"""
from __future__ import annotations

import math
import random

import numpy as np

from . import field
from .buildings import (Building, TRADES, WORKS, cathedral, hall, house, manor, palace, shop, tavern, tenement,
                        villa, works)
from .canvas import half
from .site import Lots, Site, footprint_size

CELL = 256
DESIGNED = ('ridgemere', 'ser_ferro')


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

    # -- buildings -----------------------------------------------------------------------------------------------
    def place_all(self, lots, wanted, gap_jitter=16):
        rng = self.rng
        for b in wanted:
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
        the smaller buildings fill in along lanes and avenues. Returns the lots left over (for yards)."""
        big = [b for b in wanted if b.footprint[0] * b.footprint[1] >= 130]
        small = [b for b in wanted if b.footprint[0] * b.footprint[1] < 130]
        lots = Lots(ground & ~avenues, avenues)
        self.place_all(lots, big, gap_jitter=jitter)
        cut = lanes & ground & ~grow(self.built, 1)
        self.c.paint(cut, lane_ch, None)
        streets = avenues | cut
        lots = Lots(ground & ~streets & ~grow(self.built, 1), streets)
        self.place_all(lots, small, gap_jitter=jitter)
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

    def a_shop(self, trade, name, text, district, style):
        p = shop(self.rng, style, trade)
        room = p.room('', name, f'{name}, {TRADES[trade]["label"].lower()}. {text}')
        return Building('shop', name, style, fp(p.w, p.h), [room], district=district, trade=trade)

    def a_house(self, name, text, district, style, people=3, kind='house'):
        p = house(self.rng, style, people) if kind == 'house' else tenement(self.rng, style, people)
        return Building(kind, name, style, fp(p.w, p.h), [p.room('', name, text)], district=district,
                        roof=STYLE_ROOFS.get(style, ''))

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


STYLE_ROOFS = {'serferro': '[', 'ridgemere': 'Z'}


# ---------------------------------------------------------------------------------------------------------------
# Ridgemere
# ---------------------------------------------------------------------------------------------------------------
HOUSES = {  # House: (cell, estate size, where it would like to be, industry, its seat's name, blurb)
    'grayrock': ([(2, 0)], (104, 84), (600, 160), 'quarry and ironworks', 'Grayrock Hall',
                 'The oldest of the Houses: its quarries cut the stone of Ridgemere and its forges smelt the iron '
                 'that leaves the harbour.'),
    'ashcombe': ([(1, 1), (2, 1), (2, 0)], (80, 64), (462, 420), 'charcoal and glass', 'Ashcombe House',
                 'Charcoal from the rain forest feeds its kilns and its glassworks; its windows glaze half the city.'),
    'fell': ([(2, 1), (2, 0)], (92, 74), (560, 330), 'timber', 'Fellhall',
             'Fell men fell the cedar: the House owns the logging roads, the camps and the great water-driven saw.'),
    'brinewater': ([(1, 0), (2, 0)], (58, 46), (470, 40), 'ships and rope', 'Brinewater House',
                   'Brinewater builds the ships in the harbour and twists the rope that rigs them.'),
    'vesk': ([(1, 0), (1, 1), (2, 0)], (56, 44), (476, 200), 'fish, smoke and salt', 'Vesk Manor',
             'Vesk boats bring in the catch; Vesk smokehouses cure it and Vesk salt pans preserve it.'),
}


class Ridgemere(City):
    def __init__(self, world, site):
        super().__init__(world, site, 1717, 'rm_', 'ridgemere')

    def build(self):
        w, c = self.world, self.c
        cell = self.cell_box(1, 0, 4)
        land = cell & ~w.water
        dist = self.distance_to_water(cell)
        ys, xs = np.mgrid[0:c.height, 0:c.width]
        # The old city: a rough ellipse of land by the harbour, kept a quay's width back from the water.
        cands = np.argwhere(land & (np.abs(dist - 92) < 3))
        cy, cx = min(cands, key=lambda q: math.dist((q[1], q[0]), (350, 92)))
        noise = field.fbm(c.width, c.height, 40, 1718, 2)
        reach = np.hypot((xs - cx) / 84, (ys - cy) / 76) * (1 + (noise - .5) * .25)
        core = blur_mask(land & (dist >= 40) & (reach <= 1), 4) & land & (dist >= 38) & cell
        core = w_components(core, (cx, cy))
        quay = land & (dist < 48) & grow(core, 52) & ~core & cell
        level = w.flatten_mask(core | quay, 12)
        c.paint(core, '.', level)
        c.paint(quay, '_', level)
        wall = self.ring(core, 3)
        c.paint(wall, '#', level)
        w.places['ridgemere'] = core | quay
        w.levels['ridgemere'] = level
        # Towers where the wall turns.
        for tx, ty in self.corners(wall, 7):
            c.rect(tx - 3, ty - 3, 7, 7, '#', level)
        # Gates: the Quay Gate to the harbour, the South Gate to the Southway, the East Gate to the Houses and the
        # Accord Road.
        quay_out, quay_in = self.gate(core, wall, (cx - 90, cy + 10), 'W', 6, 3, level)
        south_out, south_in = self.gate(core, wall, (440, 262), 'S', 6, 3, level, key=('ridgemere', 'S'))
        east_out, east_in = self.gate(core, wall, (512, 110), 'E', 6, 3, level, key=('ridgemere', 'E'))
        inner = core & ~grow(wall, 1)
        self.spots.update({'south_gate': south_in, 'east_gate': east_in, 'quay_gate': quay_in})
        # Districts: the government quarter on the landward (east) side, the market in the middle, the Sump down
        # by the Quay Gate; the chapel between.
        seaward = np.array([quay_in[0] - cx, quay_in[1] - cy], dtype=float)
        seaward /= np.linalg.norm(seaward) or 1
        self.districts = {
            'market': (cx, cy), 'government': (cx - seaward[0] * 42, cy - seaward[1] * 42 - 8),
            'sump': (cx + seaward[0] * 38, cy + seaward[1] * 38 + 14), 'chapel': (cx + 10, cy - 30),
            'southside': (south_in[0], south_in[1] - 14), 'eastgate': (east_in[0] - 14, east_in[1]),
        }
        # Streets: avenues from the three gates to the market square, a lane grid (narrow in the Sump).
        square = inner & (np.hypot(xs - cx, ys - cy) <= 13)
        avenues = np.zeros_like(core)
        for a in (quay_in, south_in, east_in):
            avenues |= c.line_mask([a, (cx, cy)], 5)
        ring_road = self.inner_ring(inner, 3)
        avenues = (avenues | ring_road | square) & inner
        sump = inner & (np.hypot(xs - self.districts['sump'][0], ys - self.districts['sump'][1]) < 46)
        lanes = self.lanes(inner & ~sump, (22, 17), 3, angle=.15) | self.lanes(sump, (14, 11), 2, angle=.15,
                                                                                 offset=(4, 3))
        c.paint(avenues, '_', level)
        c.paint(sump & ~avenues, '.', level)
        self.spots['market'] = (cx, cy)
        c.stamp(cx, cy, ['U'], level)
        for dx, dy in ((-8, -6), (8, -6), (-8, 6), (8, 6), (0, -9), (0, 9)):
            c.stamp(cx + dx, cy + dy, ['u'], level)
        lots, _ = self.district(inner, avenues, lanes, self.old_city(), jitter=12)
        self.yards(lots.open & inner, '.', extra=(('x', .004), ('O', .003)))
        # The quay: a street along the wall outside, buildings on the waterside, piers into the harbour.
        quay_street = quay & grow(core, 5) & ~core
        c.paint(quay_street, '_', level)
        self.districts.update({'quay_north': self.along(quay, cy - 70), 'quay': self.along(quay, cy),
                               'quay_south': self.along(quay, cy + 70)})
        waterfront = quay & (np.abs(dist - 7) <= 2)
        cross = self.lanes(quay, (26, 400), 4, angle=.15)
        quay_ground = quay & (dist >= 3)
        c.paint(waterfront, '_', level)
        qlots, _ = self.district(quay_ground & ~quay_street, (quay_street | waterfront) & quay_ground, cross,
                                 self.quayside(), jitter=10)
        self.piers(quay, level, dist)
        self.salt_and_racks(qlots.open & quay, level)
        self.spots['quay'] = self.districts['quay']
        # Patrols: round the wall inside it, and up and down the quay.
        ring_pts = np.argwhere(ring_road)
        angles = np.arctan2(ring_pts[:, 0] - cy, ring_pts[:, 1] - cx)
        self.spots['wall_walk'] = [(float(ring_pts[i][1]), float(ring_pts[i][0])) for i in
                                   [int(np.argmin(np.abs(np.angle(np.exp(1j * (angles - a))))))
                                    for a in np.linspace(-math.pi, math.pi, 12, endpoint=False)]]
        qs = np.argwhere(waterfront if waterfront.any() else quay_street)
        qs = qs[np.argsort(qs[:, 0])]
        self.spots['quay_walk'] = [(float(p[1]), float(p[0])) for p in qs[::max(1, len(qs) // 8)]]
        # The Houses' estates.
        taken = w.places['ridgemere'] | w.ua_mask
        for other in w.places.values():
            taken = taken | other
        for house, (hcells, (ew, eh), anchor, _, seat, _) in HOUSES.items():
            for hcell in hcells:
                try:
                    x, y = self.find_box(hcell, ew, eh, anchor, taken)
                    break
                except ValueError as error:
                    print('  ', error)
            else:
                raise ValueError(f'No room anywhere for House {house.title()}\'s estate')
            sid = f'estate_{house}'
            toward = east_out if hcell != (1, 0) else east_out
            side = self.facing((x + ew / 2, y + eh / 2), toward)
            region, ground, lvl, insides = self.compound(sid, x, y, ew, eh, '#', ',', {side: toward})
            taken = taken | grow(region, 8)
            getattr(self, f'estate_{house}')(sid, region, ground, lvl, insides[side], (x, y, ew, eh))
            self.roads.append((f'The {house.title()} Road', [f'{sid}.{side}', 'ridgemere.E'], False))
        return self

    @staticmethod
    def facing(frm, to):
        dx, dy = to[0] - frm[0], to[1] - frm[1]
        return ('E' if dx > 0 else 'W') if abs(dx) > abs(dy) else ('S' if dy > 0 else 'N')

    def along(self, quay, y):
        pts = np.argwhere(quay)
        i = int(np.argmin(np.abs(pts[:, 0] - y)))
        return float(pts[i][1]), float(pts[i][0])

    def corners(self, wall, spacing):
        """Tower spots: every so often along the wall where its direction changes."""
        pts = np.argwhere(wall)
        cy, cx = pts.mean(axis=0)
        angles = np.arctan2(pts[:, 0] - cy, pts[:, 1] - cx)
        out = []
        for a in np.linspace(-math.pi, math.pi, 12, endpoint=False):
            i = int(np.argmin(np.abs(np.angle(np.exp(1j * (angles - a))))))
            out.append((int(pts[i][1]), int(pts[i][0])))
        return out

    def old_city(self):
        S, rng = 'ridgemere', self.rng
        b = []
        b.append(self.named('senate', 'The Rain Hall', 'The Council of Houses sits here under a vaulted roof that '
                            'has leaked for two hundred years: tiered benches, the Chancellor\'s seat, and the five '
                            'Houses\' banners heavy with damp.', 30, 18, 'government', 'ridgemere'))
        b.append(self.named('hearing', "The Magistrate's Court", 'Oak rails, a raised bench and a dock worn smooth by '
                            'the hands of the accused. Most cases here are debts, theft from the quay, or both.', 22, 14,
                            'government', 'ridgemere'))
        b.append(self.named('barracks', 'The Watch House', 'Bunks, wet cloaks steaming by the stove and a rack of '
                            'cudgels: the City Watch keeps the walls and, when it can, the peace.', 26, 13,
                            'government', 'ridgemere'))
        b.append(self.named('guard', 'The Gaol', 'Cold cells behind iron-bound doors and a gaoler\'s table by the '
                            'stove. It is never empty.', 16, 10, 'government', 'ridgemere'))
        b.append(self.named('reading', 'The Hall of Tallies', 'Clerks tally the Houses\' dues, the harbour tolls and '
                            'the city\'s debts in ledgers that fill the walls.', 20, 12, 'government', 'ridgemere'))
        b.append(self.named('reading', 'The Customs House', 'Every cask and bale that comes through the Quay Gate is '
                            'weighed, stamped and taxed at these long counters.', 20, 12, 'sump', 'ridgemere'))
        b.append(self.named('chapel', 'The Chapel of the Tide', 'Salt-stained pews and a lamp that is never let out; '
                            'the families of the drowned light candles here.', 22, 16, 'chapel', 'ridgemere'))
        b.append(self.named('healer', 'The Sump Infirmary', 'Straw pallets, boiled rags and too few hands: the Sump '
                            'brings its coughs, burns and crushed paws here.', 18, 10, 'sump', 'ridgemere'))
        b.append(self.named('refectory', 'The Soup Kitchen', 'Long tables, a great pot and a queue that starts '
                            'before dawn. The Houses pay for it, and make sure everyone knows.', 22, 14, 'sump',
                            'ridgemere'))
        b.append(self.a_tavern('The Drowned Lantern', 'Low beams black with smoke, a fire that smells of wet '
                               'wool, and dockhands shouting over one another.', 'sump', 'ridgemere'))
        b.append(self.a_tavern('The Slag & Anchor', 'A forgehands\' tavern: loud, hot and cheap, the floor gritty '
                               'with cinder.', 'market', 'ridgemere'))
        b.append(self.a_tavern('The Rainbarrel Inn', 'The only inn inside the walls that a House factor would '
                               'sleep in: dry beds, strong tea and a fire kept high.', 'eastgate', 'ridgemere', True,
                               'Narrow rooms under a leaking roof, each with a bucket.'))
        for trade, name, text in RIDGEMERE_SHOPS:
            b.append(self.a_shop(trade, name, text, 'market', 'ridgemere'))
        for i in range(16):
            b.append(self.a_house(f'{TENEMENT_NAMES[i % len(TENEMENT_NAMES)]}', rng.choice(TENEMENT_PROSE), 'sump',
                                  'ridgemere', rng.choice([5, 6, 6]), kind='tenement'))
        for i in range(7):
            b.append(self.a_house(f'{RIDGEMERE_FAMILIES[i]} House', rng.choice(RIDGEMERE_HOUSE_PROSE),
                                  ['market', 'chapel', 'southside', 'eastgate'][i % 4], 'ridgemere',
                                  rng.choice([2, 3, 3, 4])))
        return b

    def quayside(self):
        S = 'works'
        b = []
        b.append(self.building('works', 'The Brinewater Yard', 'Brinewater\'s shipyard shed: ' +
                               WORKS['shipwright'][1] + '.', [works(self.rng, S, 'shipwright', 5)], 'quay_south', S))
        b.append(self.building('works', 'Vesk Smokehouse', 'A Vesk smokehouse: ' + WORKS['smokehouse'][1] + '.',
                               [works(self.rng, S, 'smokehouse', 4)], 'quay_north', S))
        b.append(self.building('works', 'Vesk Smokehouse by the Stair', 'The older of the Vesk smokehouses: ' +
                               WORKS['smokehouse'][1] + '.', [works(self.rng, S, 'smokehouse', 3)], 'quay_north', S))
        b.append(self.building('works', 'The Fish Hall', 'Vesk\'s fish market: ' + WORKS['fishmarket'][1] + '.',
                               [works(self.rng, S, 'fishmarket', 4)], 'quay', S))
        for i, (name, house) in enumerate((('Grayrock Iron Store', 'Grayrock'), ('Fell Timber Store', 'Fell'),
                                           ('Ashcombe Glass Store', 'Ashcombe'))):
            b.append(self.named('warehouse', name, f'{house} goods waiting for a ship: stacked, tallied and '
                                'guarded.', 20, 12, ['quay_south', 'quay', 'quay_north'][i], 'ridgemere', roof='Z'))
        b.append(self.named('reading', "The Harbourmaster's Office", 'Tide tables, berth rolls and a window over '
                            'the whole harbour.', 14, 10, 'quay', 'ridgemere'))
        b.append(self.a_tavern('The Tarred Rope', 'A dock tavern built half on the quay and half on old pilings; '
                               'the floor tilts and nobody minds.', 'quay', 'ridgemere'))
        return b

    def piers(self, quay, level, dist):
        """Piers out over the harbour from the quay's water edge."""
        c, w = self.c, self.world
        edge = quay & grow(w.water, 1)
        pts = np.argwhere(edge)
        if not len(pts):
            return
        pts = pts[np.argsort(pts[:, 0])]
        made = []
        for y, x in pts[::max(1, len(pts) // 7)]:
            if any(math.dist((x, y), m) < 26 for m in made):
                continue
            # Out along the direction away from land.
            win = w.water[y - 6:y + 7, x - 6:x + 7]
            wy, wx = np.argwhere(win).mean(axis=0) - 6 if win.any() else (0, -1)
            n = math.hypot(wx, wy) or 1
            dx, dy = wx / n, wy / n
            line = [(x + .5, y + .5), (x + .5 + dx * 22, y + .5 + dy * 22)]
            m = c.line_mask(line, 3) & (w.water | quay) & ~c.locked
            c.paint(m, '8', level)
            head = (int(x + dx * 21), int(y + dy * 21))
            c.stamp(head[0], head[1], ['i'], level)
            made.append((x, y))
        self.spots['piers'] = made

    def salt_and_racks(self, open_quay, level):
        rng = np.random.default_rng(88)
        grain = rng.random(open_quay.shape)
        self.c.codes[open_quay & (grain < .05)] = ord('|')      # Drying racks.
        self.c.codes[open_quay & (grain > .985)] = ord('x')
        self.c.codes[open_quay & (grain > .975) & (grain <= .985)] = ord('O')

    # -- the estates ---------------------------------------------------------------------------------------------
    def estate(self, sid, house, box, gate_in, level, extra, family, servants):
        """A House's seat and grounds; `extra` are the House's works inside its walls."""
        _, _, _, _, seat, blurb = HOUSES[house]
        extra = extra + [
            self.named('warehouse', f'The {house.title()} Stables', 'Horses, carts and the smell of wet straw; the '
                       'House\'s carriages wait here polished for the Council.', 20, 11, f'{sid}_works', 'ridgemere'),
            self.a_house(f'{house.title()} Cottages', 'Workers\' cottages of the House, whitewashed once and grey now, '
                         'a hearth and sleeping places for a family or two.', f'{sid}_works', 'ridgemere', 4),
            self.a_house(f'{house.title()} Row', 'A row of House cottages by the wall, smoke from every chimney.',
                         f'{sid}_works', 'ridgemere', 4)]
        lower, upper, below, stairs = manor(self.rng, 'manor', family, servants)
        hall_text = f'The great hall of {seat}: the House of {house.title()}\'s high seat under its banners. {blurb}'
        b = [self.building('manor', seat, hall_text, [lower, upper, below], f'{sid}_seat', 'manor', stairs,
                           names=[seat, f'{seat}, the family\'s chambers', f'{seat}, kitchens and servants\' hall'],
                           texts=[hall_text, 'Private chambers above the hall: heavy curtains against the rain, a fire '
                                  'in every room, the family\'s beds and chests.', 'Kitchens, pantries and the '
                                  'servants\' bunks, warm from the ovens and never quiet.'])]
        return b + extra

    def estate_yards(self, sid, open_ground, work_ch, work_extra):
        """The ground left in an estate: gardens on the manor's side, a working yard on the gate's side."""
        seat, gate = self.districts[f'{sid}_seat'], self.districts[f'{sid}_works']
        ys, xs = np.mgrid[0:self.c.height, 0:self.c.width]
        garden = open_ground & (np.hypot(xs - seat[0], ys - seat[1]) < np.hypot(xs - gate[0], ys - gate[1]))
        c = self.c
        c.codes[garden] = ord(',')
        grain = np.random.default_rng(self.rng.randrange(1 << 30)).random(garden.shape)
        hedge = garden & (((xs - int(seat[0])) % 8 == 0) | ((ys - int(seat[1])) % 8 == 0))
        c.codes[hedge & (grain < .75)] = ord('B')
        c.codes[garden & ~hedge & (grain < .08)] = ord('3')
        c.codes[garden & ~hedge & (grain > .97)] = ord('Y')
        drive = self.drives.get(sid)
        if drive is not None:
            beside = grow(drive, 1) & ~drive & open_ground & ((xs + ys) % 4 == 0)
            c.codes[beside] = ord('Y')
        self.yards(open_ground & ~garden, work_ch, extra=work_extra)

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

    def estate_grayrock(self, sid, region, ground, level, gate_in, box):
        x, y, w, h = box
        avenues = self.estate_ground(sid, region, ground, level, gate_in)
        lots = Lots(ground & ~avenues, avenues)
        # The quarry: a stepped pit in the far corner from the manor, with a ramp down its terraces.
        seat = self.districts[f'{sid}_seat']
        qx = x + 6 if seat[0] > x + w / 2 else x + w - 46
        qy = y + 6 if seat[1] > y + h / 2 else y + h - 38
        self.quarry(qx, qy, 40, 32, level, lots)
        self.spots[f'{sid}_quarry'] = (qx + 20, qy + 16)
        S = 'works'
        extra = [self.building('works', 'The Grayrock Ironworks', 'Grayrock\'s forge-hall: ' + WORKS['ironworks'][1] +
                               '. The roar never stops, rain or no rain.', [works(self.rng, S, 'ironworks', 6)],
                               f'{sid}_works', S),
                 self.building('works', 'The Quarry Office', 'Grayrock\'s quarry office: ' + WORKS['quarry_office'][1] +
                               '.', [works(self.rng, S, 'quarry_office', 2)], f'{sid}_works', S),
                 self.named('barracks', 'The Grayrock Bunkhouse', 'Quarrymen and forgehands sleep here in shifts, '
                            'their boots grey with stone dust.', 24, 12, f'{sid}_works', 'ridgemere')]
        lots, _ = self.district(ground & lots.open, avenues, np.zeros_like(ground),
                                self.estate(sid, 'grayrock', box, gate_in, level, extra, 5, 5), jitter=6, lane_ch='d')
        self.estate_yards(sid, lots.open, '>', (('X', .02), ('o', .01)))

    def quarry(self, x, y, w, h, level, lots):
        """A stepped pit: four terraces a step apart, cliff faces between them, and a ramp down the middle of one
        side where the carts go."""
        c = self.c
        for step in range(4):
            m = self.box_mask(x + step * 5, y + step * 5, w - step * 10, h - step * 5)
            c.paint(m, 'r' if step < 3 else 'X', level - step)
            if step:
                c.paint(m & ~shrink(m, 1), '%', level - step + 1)
        mid = x + w // 2
        for yy in range(y, y + h):
            band = min(3, max(0, (yy - y) // 5))
            ramp = self.box_mask(mid - 1, yy, 3, 1)
            c.paint(ramp, ':', level - band)
        lots.open[y - 2:y + h + 2, x - 2:x + w + 2] = False
        lots._refresh()

    def box_mask(self, x, y, w, h):
        m = np.zeros(self.c.codes.shape, dtype=bool)
        m[y:y + h, x:x + w] = True
        return m

    def estate_fell(self, sid, region, ground, level, gate_in, box):
        x, y, w, h = box
        avenues = self.estate_ground(sid, region, ground, level, gate_in)
        lots = Lots(ground & ~avenues, avenues)
        # The millpond feeding the wheel, and the log yard.
        px, py = x + 8, y + h // 2 - 8
        pond = self.box_mask(px, py, 18, 14)
        self.c.paint(pond, '~', level - .5)
        lots.open[py - 3:py + 17, px - 3:px + 21] = False
        lots._refresh()
        self.spots[f'{sid}_logyard'] = (x + w // 2, y + h - 14)
        S = 'works'
        extra = [self.building('works', 'The Fell Sawmill', 'The great saw of House Fell: ' + WORKS['sawmill'][1] +
                               '.', [works(self.rng, S, 'sawmill', 6)], f'{sid}_works', S),
                 self.named('barracks', 'The Loggers\' Bunkhouse', 'Loggers back from the camps sleep here, their '
                            'boots and cloaks steaming along the walls.', 24, 12, f'{sid}_works', 'ridgemere')]
        lots, _ = self.district(ground & lots.open, avenues, np.zeros_like(ground),
                                self.estate(sid, 'fell', box, gate_in, level, extra, 5, 4), jitter=6, lane_ch='d')
        self.estate_yards(sid, lots.open, '>', (('<', .05), ('7', .03)))

    def estate_ashcombe(self, sid, region, ground, level, gate_in, box):
        x, y, w, h = box
        avenues = self.estate_ground(sid, region, ground, level, gate_in)
        lots = Lots(ground & ~avenues, avenues)
        S = 'works'
        extra = [self.building('works', 'The Ashcombe Glassworks', 'Ashcombe\'s glasshouse: ' + WORKS['glassworks'][1] +
                               '.', [works(self.rng, S, 'glassworks', 5)], f'{sid}_works', S),
                 self.building('works', 'The Kilnhouse', 'Ashcombe\'s charcoal store: ' + WORKS['kilnhouse'][1] + '.',
                               [works(self.rng, S, 'kilnhouse', 3)], f'{sid}_works', S),
                 self.a_house('Burners\' Row', 'A long cottage where the charcoal burners sleep between burns, '
                              'everything in it faintly smoked.', f'{sid}_works', 'ridgemere', 6, kind='tenement')]
        lots, _ = self.district(ground & lots.open, avenues, np.zeros_like(ground),
                                self.estate(sid, 'ashcombe', box, gate_in, level, extra, 4, 4), jitter=6, lane_ch='d')
        self.estate_yards(sid, lots.open, '>', (('x', .01), ('{', .012)))    # Charcoal kilns smoking in the yard.
        self.spots[f'{sid}_kilns'] = gate_in

    def estate_brinewater(self, sid, region, ground, level, gate_in, box):
        avenues = self.estate_ground(sid, region, ground, level, gate_in)
        lots = Lots(ground & ~avenues, avenues)
        S = 'works'
        extra = [self.building('works', 'The Brinewater Ropewalk', 'Brinewater\'s ropewalk: ' + WORKS['ropewalk'][1] +
                               '.', [works(self.rng, S, 'ropewalk', 4)], f'{sid}_works', S)]
        lots, _ = self.district(ground & lots.open, avenues, np.zeros_like(ground),
                                self.estate(sid, 'brinewater', box, gate_in, level, extra, 4, 4), jitter=6, lane_ch='d')
        self.estate_yards(sid, lots.open, '.', (('x', .01), ('O', .01)))

    def estate_vesk(self, sid, region, ground, level, gate_in, box):
        avenues = self.estate_ground(sid, region, ground, level, gate_in)
        lots = Lots(ground & ~avenues, avenues)
        S = 'works'
        extra = [self.named('warehouse', 'The Vesk Salt Store', 'Sacks and tubs of grey sea salt from the Vesk pans, '
                            'kept dry at great expense.', 18, 10, f'{sid}_works', 'ridgemere')]
        lots, _ = self.district(ground & lots.open, avenues, np.zeros_like(ground),
                                self.estate(sid, 'vesk', box, gate_in, level, extra, 4, 3), jitter=6, lane_ch='d')
        self.estate_yards(sid, lots.open, '.', (('|', .02), ('O', .01)))


def w_components(mask, seed):
    from .western import components
    x, y = int(seed[0]), int(seed[1])
    if not mask[y, x]:
        ys, xs = np.nonzero(mask)
        i = int(np.argmin((xs - x) ** 2 + (ys - y) ** 2))
        x, y = int(xs[i]), int(ys[i])
    return components(mask, [(x, y)]) >= 0


RIDGEMERE_SHOPS = [
    ('chandler', 'Wickham\'s Chandlery', 'Tallow, pitch, oakum and rope: everything a ship or a leaky roof needs.'),
    ('fishmonger', 'The Wet Slab', 'Yesterday\'s catch on wet stone, and today\'s, if you are early.'),
    ('baker', 'Grist & Crust', 'Dense dark loaves that keep for a week in the damp.'),
    ('butcher', 'The Hook and Cleaver', 'Mutton, gull eggs and questions not asked about the rest.'),
    ('apothecary', 'The Cough Jar', 'Remedies for the rain lung, the forge burn and the quay fever.'),
    ('smith', 'Nailers\' Forge', 'Nails, hinges, chain: small iron by the barrel.'),
    ('cooper', 'Staves & Hoops', 'Barrels for fish, salt, pitch and ale.'),
    ('tailor', 'Oilcloth & Wool', 'Oiled cloaks, patched coats and boots that nearly keep the water out.'),
    ('general', 'Sump Sundries', 'Candle ends, string, salt fish, rags: the Sump\'s own shop.'),
    ('moneychanger', 'The Pawn and Ledger', 'Loans against anything, at rates everyone complains about.'),
    ('provisioner', 'Harbour Provisions', 'Ship\'s biscuit, salt pork and dried peas by the sack.'),
    ('tinker', 'Mend & Make Do', 'Pots, locks and lamps put right, or near enough.'),
    ('brewer', 'The Black Tun', 'A thick dark ale brewed with rainwater, which is to say, local.'),
]
TENEMENT_NAMES = ['Soot Row', 'The Drip', 'Tallow Court', 'Cinder Stair', 'The Rookery', 'Brine Yard', 'Gullwing Row',
                  'The Leaks', 'Fishgut Lane', 'Coalhole Court', 'Rope Alley', 'The Warrens', 'Bilge Row',
                  'Rust Court', 'Lantern Yard', 'Mudside']
TENEMENT_PROSE = [
    'A tenement crammed to the rafters: sleeping mats in every corner, washing strung across the room, one hearth.',
    'Damp walls, a smoking stove and too many families for the floor. Someone is always coughing.',
    'A rented warren of curtained-off corners, each one somebody\'s whole home.',
]
RIDGEMERE_HOUSE_PROSE = [
    'A narrow house of black stone, its hearth kept high against the damp.',
    'A craftsman\'s house above a workroom, smelling of wet wool and woodsmoke.',
    'Two rooms, a slate roof that mostly holds, and a rain barrel by the door.',
]
RIDGEMERE_FAMILIES = ['Tarrow', 'Slagwick', 'Coldharbour', 'Nettleby', 'Drayman', 'Kettleby', 'Rainford', 'Hollis',
                      'Marrowby', 'Cobbett', 'Pennock', 'Wetherell']


# ---------------------------------------------------------------------------------------------------------------
# Ser Ferro
# ---------------------------------------------------------------------------------------------------------------
class SerFerro(City):
    TIERS = ['wharf', 'lower', 'rise', 'heights', 'palace']

    def __init__(self, world, site):
        super().__init__(world, site, 2929, 'sf_', 'ser_ferro')

    def build(self):
        w, c = self.world, self.c
        x, y, bw, bh = 12, 2316, 232, 232
        box = self.box_mask(x, y, bw, bh)
        dry = box & ~grow(w.water, 4)
        region = w_components(dry, (x + bw / 2, y + bh / 2))
        region = blur_mask(region, 5) & dry
        dist = self.distance_to_water(box)
        ys, xs = np.mgrid[0:c.height, 0:c.width]
        # Tiers: wharf by the river, then the lower town, the Cathedral Rise, the Heights and the palace terrace,
        # climbing east and away from the river; no tier touches one more than a step from it.
        wob = (field.fbm(c.width, c.height, 30, 2930, 2) - .5) * 12
        # Terraces: each tier also keeps a band's width back from the river, so the city climbs away from the
        # wharf in broad steps rather than slivers.
        tier = np.ones(c.codes.shape, dtype=np.int16)
        tier[(xs + wob > 95) & (dist > 58)] = 2
        tier[(xs + wob > 150) & (dist > 86)] = 3
        tier[(xs + wob > 194) & (dist > 110) & (ys < 2452)] = 4
        tier[dist <= 30] = 0
        tier[~region] = 99
        for _ in range(8):                                  # At most one step between neighbours.
            low = tier.copy()
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                low = np.minimum(low, np.roll(tier, (dy, dx), (0, 1)))
            tier = np.where(region, np.minimum(tier, low + 1), tier)
        base = w.flatten_mask(region, 14)
        heights = base + tier.astype(np.float32)
        c.paint(region, 'f', 0)
        c.heights[region] = heights[region]
        self.tier = np.where(region, tier, -1)
        w.places['ser_ferro'] = region
        w.levels['ser_ferro'] = base + 1
        wall = self.ring(region, 3)
        self.wall = wall
        c.paint(wall, ']', None)
        for tx, ty in Ridgemere.corners(self, wall, 9):
            m = self.box_mask(tx - 3, ty - 3, 7, 7) & region
            c.codes[m] = ord(']')
            c.locked[m] = True
        north_out, north_in = self.gate(region, wall, (128, 2300), 'N', 8, 3, None, key=('ser_ferro', 'N'))
        east_out, east_in = self.gate(region, wall, (260, 2466), 'E', 8, 3, None, key=('ser_ferro', 'E'))
        inner = region & ~grow(wall, 1)
        self.region_mask = region
        self.spots.update({'north_gate': north_in, 'east_gate': east_in})
        # Squares and avenues: the Processional Way from the North Gate to the cathedral square and on up the tiers
        # to the palace forecourt; the market street down to the wharf; the river street along it.
        plaza_c = (122, 2412)
        plaza = inner & (np.abs(xs - plaza_c[0]) <= 18) & (np.abs(ys - plaza_c[1]) <= 11)
        forecourt_c = (216, 2372)
        forecourt = inner & (np.abs(xs - forecourt_c[0]) <= 12) & (np.abs(ys - forecourt_c[1]) <= 10)
        market_c = (62, 2440)
        market = inner & (np.abs(xs - market_c[0]) <= 14) & (np.abs(ys - market_c[1]) <= 10)
        avenue_lines = [[north_in, (124, 2366), (122, 2400)], [(140, 2410), (172, 2400), (200, 2380), forecourt_c],
                        [(118, 2424), (112, 2470), (106, 2520)], [(24, 2440), (104, 2414)],
                        [east_in, (218, 2440), (216, 2384)], [(62, 2330), market_c, (80, 2500)],
                        self.wharf_line(inner, dist)]
        avenues = self.inner_ring(inner, 3)
        for pts_ in avenue_lines:
            if len(pts_) > 1:
                avenues |= c.line_mask(pts_, 5)
        squares = plaza | forecourt | market
        avenues = (avenues | squares) & inner
        # Tier edges: retaining walls, with stairs wherever an avenue or lane crosses (painted once streets are cut).
        up = np.zeros_like(region)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            up |= np.roll(self.tier, (dy, dx), (0, 1)) < self.tier
        up &= inner & (self.tier > 0)
        c.paint(avenues & ~up, '_', None)
        c.paint(squares & (self.tier >= 2), 'm', None)
        c.stamp(plaza_c[0] - 1, plaza_c[1] + 4, ['FFF', 'FSF', 'FFF'], None)
        for dx in (-15, 15):
            c.stamp(plaza_c[0] + dx, plaza_c[1] - 8, ['S'], None)
        c.stamp(forecourt_c[0] - 1, forecourt_c[1] - 1, ['FFF', 'FSF', 'FFF'], None)
        c.stamp(market_c[0], market_c[1], ['U'], None)
        for dx, dy in ((-9, -6), (9, -6), (-9, 6), (9, 6), (0, -7), (0, 7), (-5, 0), (5, 0)):
            c.stamp(market_c[0] + dx, market_c[1] + dy, ['u'], None)
        self.spots.update({'plaza': plaza_c, 'forecourt': forecourt_c, 'market': market_c})
        self.districts = {
            'palace': (226, 2366), 'heights': (176, 2360), 'rise': (122, 2386), 'rise_south': (130, 2448),
            'lower': (50, 2390), 'lower_south': (60, 2480), 'wharf': self.along_wharf(inner, dist, 100),
            'wharf_east': self.along_wharf(inner, dist, 190),
        }
        self.spots['wharf'] = self.districts['wharf']
        # Buildings, tier by tier: a block never straddles two tiers.
        blocked = up | grow(wall, 1)
        lane_spacing = {'wharf': (17, 13), 'lower': (19, 15), 'rise': (21, 16), 'heights': (30, 24), 'palace': (60, 60)}
        streets_all = avenues.copy()
        for k, name in enumerate(self.TIERS):
            ground = inner & ~blocked & (self.tier == k)
            lanes = self.lanes(ground, lane_spacing[name], 3, angle=-.08, offset=(k * 5, k * 3))
            lots, streets = self.district(ground, avenues & ground, lanes & ~up, self.wanted()[name], jitter=14)
            streets_all |= streets
            left = lots.open & ground
            if name == 'palace':
                self.gardens(left)
            elif name == 'heights':
                self.yards(left, ',', extra=(('B', .04), ('Y', .025), ('3', .03), ('F', .002)))
            elif name == 'wharf':
                self.yards(left, '.', extra=(('x', .01), ('O', .01), ('|', .012)))
            else:
                self.yards(left, ',', extra=(('Y', .015), ('B', .01), ('U', .002), ('3', .01)))
        stairs = up & grow(streets_all, 1)
        c.paint(up & ~stairs, ']', None)
        c.paint(stairs, '^', None)
        self.river_piers(inner, dist)
        return self

    def box_mask(self, x, y, w, h):
        m = np.zeros(self.c.codes.shape, dtype=bool)
        m[y:y + h, x:x + w] = True
        return m

    def wharf_line(self, inner, dist):
        band = inner & (np.abs(dist - 14) < 1.5)
        pts = np.argwhere(band)
        if not len(pts):
            return []
        pts = pts[np.argsort(pts[:, 1])]
        out = [(float(p[1]), float(p[0])) for p in pts[::max(1, len(pts) // 12)]]
        return out

    def along_wharf(self, inner, dist, x):
        band = np.argwhere(inner & (np.abs(dist - 12) < 2))
        i = int(np.argmin(np.abs(band[:, 1] - x)))
        return float(band[i][1]), float(band[i][0]) - 4

    def river_piers(self, inner, dist):
        c, w = self.c, self.world
        edge = np.argwhere(inner & (dist <= 12) & (self.tier == 0) & ~self.built)
        if not len(edge):
            return
        edge = edge[np.argsort(edge[:, 1])]
        made = []
        for y, x in edge[::max(1, len(edge) // 8)]:
            if any(abs(x - m) < 24 for m in made):
                continue
            win = w.water[y - 8:y + 9, x - 8:x + 9]
            if not win.any():
                continue
            wy, wx = np.argwhere(win).mean(axis=0) - 8
            n = math.hypot(wx, wy) or 1
            line = [(x + .5, y + .5), (x + .5 + wx / n * 26, y + .5 + wy / n * 26)]
            # A water gate through the wall, and the pier out over the river.
            m = c.line_mask(line, 3) & (w.water | self.wall | (grow(self.wall, 5) & ~self.region_mask)) & ~self.built
            c.paint(m, '8', float(c.heights[y, x]))
            made.append(x)
        self.spots['piers'] = made

    def gardens(self, open_ground):
        """The palace gardens: lawns, clipped hedges, fountains, statues and trees."""
        c = self.c
        ys, xs = np.nonzero(open_ground)
        c.codes[open_ground] = ord(',')
        grain = np.random.default_rng(31).random(open_ground.shape)
        hedge = open_ground & (((np.arange(c.width)[None, :] % 9) == 0) | ((np.arange(c.height)[:, None] % 9) == 0))
        c.codes[hedge & (grain < .7)] = ord('B')
        c.codes[open_ground & (grain > .992)] = ord('F')
        c.codes[open_ground & (grain > .985) & (grain <= .992)] = ord('S')
        c.codes[open_ground & (grain > .95) & (grain <= .985)] = ord('Y')
        c.codes[open_ground & (grain > .9) & (grain <= .95)] = ord('3')

    def wanted(self):
        if getattr(self, '_wanted', None) is None:
            self._wanted = self._make_wanted()
        return self._wanted

    def _make_wanted(self):
        rng = self.rng
        S, P = 'serferro', 'palace'
        out = {k: [] for k in self.TIERS}
        court, royal, below, stairs = palace(rng, P, 6, 12)
        out['palace'].append(self.building('palace', 'The Palace of Ser Ferro', '', [court, royal, below], 'palace', P,
                                           stairs, names=['The Court of Ser Ferro', 'The Royal Apartments',
                                                          'The Palace Kitchens'],
                                           texts=['The throne room of the Kings of Ser Ferro: white marble, red '
                                                  'banners, a carpet running to twin thrones and the court '
                                                  'standing in its finery along the pillars.',
                                                  'The royal apartments: painted ceilings, tall windows over the river '
                                                  'and beds hung with red silk.',
                                                  'Kitchens and the servants\' hall beneath the palace, all copper '
                                                  'pans, bread ovens and hurrying feet.'], roof='[', scale=.62))
        out['palace'].append(self.named('hearing', 'The Hall of Petitions', 'Where the King\'s chancellor hears '
                                        'the petitions of the city twice a week; the benches are always full.', 24, 14,
                                        'palace', P, roof='['))
        out['palace'].append(self.named('barracks', 'The Palace Guardhouse', 'The King\'s own guard: polished '
                                        'breastplates on stands, red cloaks on pegs, bunks made to a finger\'s '
                                        'width.', 24, 12, 'palace', S, roof='['))
        out['palace'].append(self.named('chapel', 'The Chapel Royal', 'A jewel of a chapel for the royal family, '
                                        'gold leaf on white stone.', 18, 12, 'palace', P, roof='['))
        nave, crypt, cstairs = cathedral(rng, P)
        out['rise'].append(self.building('cathedral', 'The Cathedral of the Iron Saint', '', [nave, crypt], 'rise', P,
                                         cstairs, keys=['', 'crypt'], zs=[0, -1],
                                         names=['The Cathedral of the Iron Saint', 'The Cathedral Crypt'],
                                         texts=['The great nave of the Iron Saint, founder of the city: white '
                                                'pillars march to an altar under the saint\'s statue, light falls '
                                                'through tall windows, and the incense never quite clears.',
                                                'The crypt beneath the cathedral, where the Kings and the saint '
                                                'himself lie in niches of white stone.'], roof='[', scale=.62))
        out['rise'].append(self.named('reading', 'The Chapter House', 'The cathedral\'s clergy keep their records, '
                                      'their library and their quarrels here.', 20, 12, 'rise', S, roof='['))
        cl, cu, cst = villa(rng, S, 5, 1)
        out['rise'].append(self.building('house', 'The Clergy House', 'The cathedral clergy live here in plain '
                                         'white rooms.', [cl, cu], 'rise', S, cst, roof='[',
                                         names=['The Clergy House', 'The Clergy House, cells'],
                                         texts=['A plain hall with a long table and a devotional niche.',
                                                'Narrow sleeping cells, one to a priest.']))
        for family in ELITE_FAMILIES:
            lower, upper, vst = villa(rng, S, 4, 2)
            name = f'Palazzo {family}'
            out['heights'].append(self.building('villa', name, '', [lower, upper], 'heights', S, vst, roof='[',
                                                names=[name, f'{name}, upper floor'],
                                                texts=[f'The {family} family\'s palazzo: a marble courtyard with a '
                                                       'fountain, painted walls and servants who never quite stop '
                                                       'moving.', 'Bedchambers opening onto a loggia over the city '
                                                                  'and the river.']))
        out['heights'].append(self.named('senate', "The Merchants' Guildhall", 'The guild of Ser Ferro\'s great '
                                         'merchants meets under gilded beams to set prices and settle feuds.', 24, 14,
                                         'heights', S, roof='['))
        for trade, name, text in (('jeweler', 'Oro e Lume', 'Gold and gems for the court, set with a steady hand.'),
                                  ('moneychanger', 'The Bank of the Red Roof', 'Letters of credit honoured from the '
                                   'river to the mountains.'),
                                  ('tailor', 'Seta Fina', 'Silks and velvets cut to the court\'s latest fancy.')):
            out['heights'].append(self.a_shop(trade, name, text, 'heights', S))
        for trade, name, text in SERFERRO_SHOPS:
            district = 'rise' if trade in ('apothecary', 'scribe', 'cartographer', 'chandler', 'weaver') else \
                rng.choice(['lower', 'lower_south'])
            tier = 'rise' if district == 'rise' else 'lower'
            out[tier].append(self.a_shop(trade, name, text, district, S))
        out['rise'].append(self.a_tavern('The Sunlit Cup', 'A bright tavern on the Cathedral Rise: whitewashed '
                                         'walls, red wine, pilgrims and gossip.', 'rise', S))
        out['lower'].append(self.a_tavern('The Three Bells', 'The lower town\'s favourite: long tables, songs and '
                                          'a landlord who remembers everyone\'s cup.', 'lower', S))
        out['lower'].append(self.a_tavern('The Golden Sheaf Inn', 'An inn for the grain traders who come downriver: '
                                          'a courtyard, a vine and clean beds.', 'lower_south', S, True,
                                          'Whitewashed rooms with shuttered windows onto the courtyard.'))
        out['lower'].append(self.named('barracks', 'The City Guard Barracks', 'The city guard of Ser Ferro: red '
                                       'tabards, polished helms and a sergeant who shouts.', 26, 13, 'lower', S))
        out['lower'].append(self.named('healer', 'The House of Saint Chiara', 'The sisters of the cathedral nurse the '
                                       'sick of the lower town in clean white wards.', 20, 12, 'lower_south', S))
        out['lower'].append(self.named('senate', "The Wool Guild Hall", 'Weavers, dyers and fullers argue here over '
                                       'the price of wool and the colour of the season.', 22, 13, 'lower', S))
        for i in range(50):
            out['lower'].append(self.a_house(f'{sf_family(i)} House',
                                             rng.choice(SERFERRO_HOUSE_PROSE), ['lower', 'lower_south'][i % 2], S,
                                             rng.choice([2, 3, 3, 4])))
        for i in range(18):
            out['rise'].append(self.a_house(f'{sf_family(i + 52)} House',
                                            rng.choice(SERFERRO_HOUSE_PROSE), ['rise', 'rise_south'][i % 2], S,
                                            rng.choice([2, 3, 3])))
        # The wharf: warehouses, the fish market, the harbour office, a dock tavern and the only slums in the city.
        for i, name in enumerate(('The Grain Warehouse', 'The Wine Warehouse')):
            out['wharf'].append(self.named('warehouse', name, 'Sacks, casks and bales stacked to the rafters, waiting '
                                           'for a barge.', 20, 12, ['wharf', 'wharf_east'][i % 2], S, roof='['))
        out['wharf'].append(self.building('works', 'The Fish Market', 'The river fish market: ' +
                                          WORKS['fishmarket'][1] + '.', [works(rng, S, 'fishmarket', 4)], 'wharf', S,
                                          roof='['))
        out['wharf'].append(self.named('reading', 'The River Office', 'The harbourmaster tallies barges, tolls and '
                                       'berths, and complains about all three.', 14, 10, 'wharf_east', S, roof='['))
        out['lower'].append(self.a_tavern('The Muddy Oar', 'A tavern just above the wharf with a sagging floor, river mud on '
                                          'every boot and the cheapest wine in the city.', 'lower_south', S))
        for i in range(7):
            out['wharf'].append(self.a_house(SERFERRO_TENEMENTS[i], rng.choice(SERFERRO_TENEMENT_PROSE), 'wharf', S,
                                             rng.choice([5, 6]), kind='tenement'))
        return out


SERFERRO_SHOPS = [
    ('baker', 'Il Forno d\'Oro', 'Crusty white loaves and sweet rolls at dawn; the queue reaches the fountain.'),
    ('baker', 'Pane del Sole', 'Flatbreads with oil and rosemary from a wood oven.'),
    ('butcher', 'Macelleria Rossi', 'Hams hung to cure, sausages by the yard.'),
    ('general', 'The Red Door', 'Everything a household needs, from lamp oil to lace.'),
    ('potter', 'The White Kiln', 'Glazed white and blue ware, the city\'s pride.'),
    ('weaver', 'Tessitura Bellandi', 'Fine wool and linen in the season\'s colours.'),
    ('carpenter', 'Bottega del Legno', 'Chairs, chests and shutters painted to order.'),
    ('smith', 'La Forgia', 'Iron gates, lamp brackets and the odd sword.'),
    ('cooper', 'Botti Ferrante', 'Wine casks for the whole river valley.'),
    ('brewer', 'The Vine and Vat', 'Wine from the golden hills, by the cup or by the cask.'),
    ('tinker', 'Ottone & Rame', 'Brass and copper mended and made.'),
    ('apothecary', 'Farmacia della Santa', 'Remedies prepared by the cathedral sisters.'),
    ('scribe', 'Penna e Sigillo', 'Letters, contracts and petitions to the palace, beautifully written.'),
    ('cartographer', 'Mappe del Fiume', 'Charts of the river and the roads to every city.'),
    ('chandler', 'Cera Bianca', 'Beeswax candles for the cathedral and anyone who can afford them.'),
    ('provisioner', 'Dispensa del Porto', 'Provisions for barge crews and travellers.'),
    ('fishmonger', 'Pesce del Fiume', 'River fish on ice, eels in tubs.'),
    ('herbalist', 'Erbe di Campo', 'Herbs from the golden fields, dried in bunches.'),
]
ELITE_FAMILIES = ['Valmonte', 'Lucenti', 'Aldobrandi', 'Orsenna', 'Castellane', 'Marenzi']
SERFERRO_FAMILIES = ['Bellandi', 'Castelli', 'Dardano', 'Ferrante', 'Galli', 'Lanza', 'Moretti', 'Neri', 'Orsini',
                     'Pallotta', 'Rinaldi', 'Salvini', 'Toscani', 'Valeri', 'Venturi', 'Albani', 'Benedetti',
                     'Corsini', 'Donati', 'Esposito', 'Fabbri', 'Greco', 'Leone', 'Mancini', 'Marchetti', 'Negri',
                     'Palmieri', 'Ricci', 'Santoro', 'Serra', 'Testa', 'Vitale', 'Zanetti', 'Barone', 'Colombo',
                     'De Luca', 'Ferri', 'Gentile', 'Longo', 'Martini', 'Monti', 'Parisi', 'Rizzo', 'Sala', 'Silvestri',
                     'Villa']
SF_HEADS = ['Bel', 'Cas', 'Dar', 'Fer', 'Gal', 'Lan', 'Mor', 'Ner', 'Ors', 'Ros', 'Sal', 'Tor', 'Val', 'Ven', 'Mar',
            'Col', 'Ben', 'Pal', 'Riv', 'Cor', 'Lu', 'Ser', 'Al', 'Tes']
SF_TAILS = ['andi', 'elli', 'ano', 'ante', 'etti', 'ini', 'ucci', 'one', 'esi', 'ari', 'otti', 'agna', 'ieri', 'uzzi',
            'ale', 'ardi']


def sf_family(i):
    """The i-th Ser Ferro family name: the common names first, then made ones, never repeating."""
    if i < len(SERFERRO_FAMILIES):
        return SERFERRO_FAMILIES[i]
    j = i - len(SERFERRO_FAMILIES)
    return SF_HEADS[(j * 7) % len(SF_HEADS)] + SF_TAILS[(j * 5 + j // len(SF_HEADS)) % len(SF_TAILS)]


SERFERRO_HOUSE_PROSE = [
    'A whitewashed house with red shutters: a tiled floor, a painted table, a saint on the wall.',
    'A tall narrow house with a workshop below and the family above, geraniums at every window.',
    'Cool white rooms around a tiny courtyard with a lemon tree in a pot.',
    'A comfortable middle-class home: good linen, a copper pot on the hearth and a cat on the stair.',
]
SERFERRO_TENEMENTS = ['Mudbank Row', 'The Eel Pens', 'Bargeman\'s Court', 'Rat Stair', 'The Sinks', 'Wetfoot Yard',
                      'The Old Tannery']
SERFERRO_TENEMENT_PROSE = [
    'A crumbling tenement by the river, its whitewash long gone grey; families sleep in every corner.',
    'Damp rooms that flood in spring, curtained into homes. The city\'s poor live here, out of the palace\'s sight.',
]


def build(world):
    """Design both cities on the world's canvas. Returns (site, [cities], extra roads)."""
    site = CitySite(world.c, world.reserved_ids)
    cities = [Ridgemere(world, site).build(), SerFerro(world, site).build()]
    roads = [r for city in cities for r in city.roads]
    return site, cities, roads
