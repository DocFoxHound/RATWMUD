"""The building kit: interiors laid out by building type, in the stone of their district.

Every interior is drawn with its street door on the bottom edge and rotated to face its street. A building is
one or more rooms (floors joined by stairs); it records where residents sleep (`beds`) and where someone works
(`work`), so the population phase can move people in without guessing.
"""
from __future__ import annotations

import random
from dataclasses import dataclass, field as dc_field

# Walls and floors by district style.
STYLES = {  # roof: what the outdoor map shows inside the building's outer wall.
    'city': {'wall': '#', 'floor': 'f', 'home_floor': '.', 'outer': '#', 'roof': 'ZL'},
    'civic': {'wall': 'H', 'floor': 'f', 'home_floor': 'f', 'outer': 'H', 'roof': 'Q'},
    'concord': {'wall': 'M', 'floor': 'm', 'home_floor': 'm', 'outer': 'M', 'roof': 'V'},
    'training': {'wall': '#', 'floor': '.', 'home_floor': '.', 'outer': '#', 'roof': 'L'},
    'order': {'wall': '#', 'floor': 'f', 'home_floor': 'f', 'outer': '#', 'roof': 'Q'},
    # Ridgemere: soot-dark stone under wet slate; its works stand on cinder floors.
    'ridgemere': {'wall': '#', 'floor': 'f', 'home_floor': '.', 'outer': '#', 'roof': 'Z'},
    'works': {'wall': '#', 'floor': '>', 'home_floor': '.', 'outer': '#', 'roof': 'Z'},
    'manor': {'wall': 'H', 'floor': 'f', 'home_floor': 'f', 'outer': 'H', 'roof': 'Z'},
    # Ser Ferro: whitewashed walls under red tile; marble in the palace and the cathedral.
    'serferro': {'wall': ']', 'floor': 'f', 'home_floor': 'f', 'outer': ']', 'roof': '['},
    'palace': {'wall': 'M', 'floor': 'm', 'home_floor': 'm', 'outer': ']', 'roof': '['},
}


@dataclass
class Room:
    key: str                      # Suffix of the room ID within its building ('' for the ground floor).
    name: str
    description: str
    rows: list[list[str]]
    z: int = 0
    lighting: dict = dc_field(default_factory=lambda: {'artificial': .8, 'daylightAccess': .5, 'tone': 'warm'})
    beds: list = dc_field(default_factory=list)       # (x, y) tiles where a resident sleeps.
    work: list = dc_field(default_factory=list)       # (x, y) tiles where someone works.
    door: tuple | None = None                          # (x, y) of the street door, for the ground floor.
    stations: list = dc_field(default_factory=list)   # (x, y, station id): work stations (Data/Items/stations.json).
    back: tuple | None = None                          # (x, y) of a second door on the top edge (doc 67), if any.

    @property
    def width(self):
        return len(self.rows[0])

    @property
    def height(self):
        return len(self.rows)


@dataclass
class Building:
    kind: str
    name: str
    style: str
    footprint: tuple[int, int]    # Width and height of the block on the outdoor map (door edge at the bottom).
    rooms: list[Room]
    stairs: list = dc_field(default_factory=list)      # ((room key, x, y), (room key, x, y)) pairs.
    district: str = ''
    roof: str = ''                                     # Roof tile; the style's first roof when empty.
    trade: str = ''                                    # A shop's trade (see TRADES).


class Plan:
    """A room being furnished: walls around a floor, door on the bottom edge, a clear lane in from the door."""

    def __init__(self, w, h, wall, floor, rng: random.Random):
        self.w, self.h, self.wall, self.floor, self.rng = w, h, wall, floor, rng
        self.g = [[floor] * w for _ in range(h)]
        for x in range(w):
            self.g[0][x] = self.g[h - 1][x] = wall
        for y in range(h):
            self.g[y][0] = self.g[y][w - 1] = wall
        self.door = (w // 2, h - 1)
        self.g[h - 1][w // 2] = '+'
        self.keep = {(w // 2, h - 2), (w // 2, h - 3)}     # The lane in from the door stays clear.
        self.beds, self.work, self.stations = [], [], []

    def free(self, x, y):
        return 0 < x < self.w - 1 and 0 < y < self.h - 1 and self.g[y][x] == self.floor and (x, y) not in self.keep

    def put(self, x, y, ch):
        if self.free(x, y):
            self.g[y][x] = ch
            return True
        return False

    def run(self, x, y, dx, dy, n, ch):
        for i in range(n):
            self.put(x + dx * i, y + dy * i, ch)

    def partition(self, x=None, y=None, gap=None):
        """An inner wall across the room with a doorway gap in it."""
        if x is not None:
            gap = gap if gap is not None else self.h // 2
            for yy in range(1, self.h - 1):
                if abs(yy - gap) > 0 and (x, yy) not in self.keep:
                    self.g[yy][x] = self.wall
        else:
            gap = gap if gap is not None else self.w // 2
            for xx in range(1, self.w - 1):
                if abs(xx - gap) > 0 and (xx, y) not in self.keep:
                    self.g[y][xx] = self.wall
            self.keep |= {(gap, y - 1), (gap, y + 1)}

    def bed(self, x, y, ch='b'):
        if self.put(x, y, ch):
            self.beds.append((x, y))

    def worker(self, x, y):
        if self.free(x, y):
            self.work.append((x, y))
            self.keep.add((x, y))

    def station(self, x, y, kind, ch=None):
        """A work station (doc 35), recorded; drawn with `ch` or its stand-in tile until stations are placed objects."""
        if self.put(x, y, ch or STAND_IN[kind]):
            self.stations.append((x, y, kind))
            return True
        return False

    def room(self, key, name, description, **kw):
        r = Room(key, name, description, self.g, beds=self.beds, work=self.work, door=self.door,
                 stations=self.stations, **kw)
        if getattr(self, 'back', None):
            r.back = self.back
        return r

    # Doc 67 (Ridgemere 2.0): L-shaped plans and a second door. Plans that don't call them are unchanged.
    def notch(self, corner, w, h):
        """An L-shape: the `corner` ('NE', 'NW'; the street door's side is S) w x h tiles become solid wall, outside
        the room, which wraps round them."""
        xs = range(0, w) if 'W' in corner else range(self.w - w, self.w)
        ys = range(0, h) if 'N' in corner else range(self.h - h, self.h)
        for y in ys:
            for x in xs:
                self.g[y][x] = self.wall
        # The notch's inner corner gets its own wall line: the tiles beside it stay walls only where they bound floor.
        self.notched = (corner, w, h)

    def back_door(self, x=None):
        """A second door on the top edge, onto another alley (a through-building)."""
        x = self.w // 2 if x is None else x
        self.g[0][x] = '+'
        self.back = (x, 0)
        self.keep |= {(x, 1), (x, 2)}


# The terrain tile each station is drawn with until the engine places stations as objects with their own glyphs
# (Data/Items/stations.json has those glyphs).
STAND_IN = {
    'hearth': 'h', 'oven': 'v', 'smokehouse': '|', 'brew_kettle': 'O', 'press': 'x', 'millstones': 'o',
    'butcher_block': 'T', 'rendering_pot': 'h', 'tanning_pits': '~', 'leather_bench': 'T', 'spinning_wheel': 'c',
    'loom': 'T', 'dye_vat': '~', 'tailor_table': 'T', 'smelter': '{', 'forge': 'A', 'crucible': '{',
    'grindstone': 'o', 'jeweler_bench': 'T', 'workbench': 'T', 'pole_lathe': 'T', 'saw_pit': '<', 'sawmill': '<',
    'potter_wheel': 'c', 'kiln': '{', 'lime_kiln': '{', 'mason_banker': 'o', 'glass_furnace': '{',
    'charcoal_clamp': '{', 'still': 'O', 'apothecary_bench': 'T', 'drying_rack': '|', 'salt_pan': '~',
    'chandler_kettle': 'h', 'ropewalk': 'x', 'paper_vat': '~', 'printing_press': 'x', 'scribe_desk': 'l',
    'siege_frame': '<', 'wall_crossbow': 'y', 'siege_crossbow': 'y',
}


def stairs_between(lower: Plan, upper: Plan, x, y):
    """A stair tile at (x, y) on both floors, so the two can be linked as stairs."""
    for p in (lower, upper):
        p.g[y][x] = '^'
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 < nx < p.w - 1 and 0 < ny < p.h - 1 and p.g[ny][nx] not in ('.', 'f', 'm', 'R', '^'):
                p.g[ny][nx] = p.floor
        p.keep.add((x, y + 1))


def rotate(rows: list[list[str]], turns: int) -> list[list[str]]:
    """Quarter turns clockwise."""
    for _ in range(turns % 4):
        rows = [list(r) for r in zip(*rows[::-1])]
    return rows


def turn_point(x, y, w, h, turns):
    for _ in range(turns % 4):
        x, y, w, h = h - 1 - y, x, h, w
    return x, y


# -- recipes ----------------------------------------------------------------------------------------------------

def house(rng, style, people):
    s = STYLES[style]
    w, h = rng.choice([(12, 9), (13, 9), (12, 10), (14, 10)]) if people <= 3 else (16, 11)
    p = Plan(w, h, s['wall'], s['home_floor'], rng)
    p.put(w // 2, 1, 'h')
    p.run(w // 2 - 1, 4, 1, 0, 3, 'T')
    p.put(w // 2 - 1, 5, 'c')
    p.put(w // 2 + 1, 5, 'c')
    p.put(w // 2 - 1, 3, 'c')
    spots = [(1, 1), (1, 3), (w - 2, 1), (w - 2, 3), (1, 5), (w - 2, 5), (3, 1), (w - 4, 1)]
    for x, y in spots[:people]:
        p.bed(x, y)
    p.put(w - 2, h - 2, rng.choice('xO'))
    p.put(1, h - 2, rng.choice('xOk'))
    return p


def shop(rng, style, trade):
    """A shopfront: goods and a counter at the back, the keeper behind it, a customer floor in front."""
    s = STYLES[style]
    w, h = rng.choice([(16, 11), (18, 12), (15, 11)])
    p = Plan(w, h, s['wall'], s['floor'], rng)
    goods = TRADES[trade]['goods']
    for x in range(1, w - 1):
        ch = goods[x % len(goods)]
        if ch == 'v' and TRADES[trade].get('oven_goods'):
            p.station(x, 1, 'oven', ch)
        else:
            p.put(x, 1, ch)
    p.run(2, 4, 1, 0, w - 4, '=')
    p.g[4][w - 3] = p.floor                                     # A gap at the end of the counter.
    p.worker(w // 2, 3)
    for x, y, ch, kind in TRADES[trade].get('fixtures', ()):
        x, y = x if x >= 0 else w + x, y if y >= 0 else h + y
        if kind:
            p.station(x, y, kind, ch)
        else:
            p.put(x, y, ch)
    p.put(1, h - 2, 'O')
    p.put(w - 2, h - 2, 'x')
    return p


def keeper_word(trade):
    """What a shop's keeper is called ("baker", "shopkeeper"): the word the game server knows the shop by in their work
    label (Data/Items/businesses.json `match`; Docs/Design/39)."""
    label = TRADES[trade]['label'].lower()
    return {'smithy': 'smith', 'bakery': 'baker', 'tannery': 'tanner', 'stonemason': 'mason', 'general goods': 'shopkeeper',
            'armorer': 'armourer', 'printing house': 'printer', 'harness-maker': 'saddler'}.get(label, label)


def keeper_label(trade, name):
    """A shop keeper's work label: "baker at The Amber Loaf" (at most 40 characters, the trade word always kept)."""
    word = keeper_word(trade)
    return f'{word} at {name}'[:40]


FLAT_PROSE = [
    'The family\'s rooms over the shop: a hearth, a table worn smooth, and beds under the eaves; the smell of the '
    'trade comes up the stairs.',
    'Low rooms above the shop, warm from the hearth below, with the family\'s things crowded onto every shelf.',
    'A home over the counter: beds along the walls, a pot on the fire, and the shop\'s ledgers on the table at night.',
    'Up the stairs from the shop, the family keeps a plain room or two: hearth, table, chests and bedding.',
]


def flat_over(rng, style, below: Plan, people):
    """The keeper's flat above a shop: a family home with no street door, reached by stairs from the shop's customer
    floor (on the side away from the door and the counter). Returns the plan and the stair tile."""
    s = STYLES[style]
    w, h = below.w, below.h
    up = Plan(w, h, s['wall'], s['home_floor'], rng)
    up.g[h - 1][w // 2] = s['wall']                         # No street door upstairs.
    up.keep = set()
    sx, sy = w - 2, h - 4
    stairs_between(below, up, sx, sy)
    up.put(w // 2, 1, 'h')
    up.run(w // 2 - 1, 4, 1, 0, 3, 'T')
    for x, y in ((w // 2 - 1, 5), (w // 2 + 1, 5), (w // 2 - 1, 3)):
        up.put(x, y, 'c')
    spots = [(1, 1), (1, 3), (w - 2, 1), (1, 5), (3, 1), (w - 4, 1), (1, 7), (w - 2, 3)]
    for x, y in spots[:people]:
        up.bed(x, y)
    up.put(1, h - 2, rng.choice('kxO'))
    up.put(3, h - 2, 'k')
    return up, (sx, sy)


def shop_with_flat(rng, style, trade, name, text, people=None, flat_text=None):
    """A shop and its keeper's flat upstairs, as rooms ('' the shop, 'flat' above it) and the stairs between.
    Returns (rooms, stairs, (w, h)) for a Building."""
    p = shop(rng, style, trade)
    # The flat from its own sequence, so a settlement's layout doesn't shift for having flats in it.
    own = random.Random(f'{name}:flat')
    people = people or own.choice([2, 3, 3, 4])
    up, (sx, sy) = flat_over(own, style, p, people)
    rooms = [p.room('', name, f'{name}, {TRADES[trade]["label"].lower()}. {text}'),
             up.room('flat', f'{name}, the flat above', flat_text or own.choice(FLAT_PROSE), z=1)]
    return rooms, [(('', sx, sy), ('flat', sx, sy))], (p.w, p.h)


def tavern(rng, style, inn=False):
    s = STYLES[style]
    w, h = (24, 16) if inn else (20, 14)
    p = Plan(w, h, s['wall'], s['floor'], rng)
    p.run(2, 3, 1, 0, 8, '=')
    p.worker(5, 2)
    p.run(1, 1, 1, 0, 9, 'O')
    p.put(w - 2, 1, 'h')
    for ty in range(6, h - 3, 3):
        for tx in range(3, w - 3, 5):
            if (tx, ty) not in p.keep:
                p.run(tx, ty, 1, 0, 2, 'T')
                p.put(tx - 1, ty, 'c')
                p.put(tx + 2, ty, 'c')
    rooms = [p]
    stair_pairs = []
    if inn:
        up = Plan(w, h, s['wall'], s['floor'], rng)
        up.g[h - 1][w // 2] = s['wall']                         # Upstairs has no street door.
        up.keep = set()
        for i, x in enumerate(range(1, w - 1, 5)):
            if x + 4 < w - 1:
                for yy in range(1, 6):
                    up.g[yy][x + 4] = s['wall']
            up.bed(x + 1, 2)
            up.bed(x + 1, 3)
        up.partition(y=6, gap=w // 2)
        stairs_between(p, up, w - 3, h - 3)
        stair_pairs.append(('up', w - 3, h - 3))
        rooms.append(up)
    return rooms, stair_pairs


def hall(rng, style, w, h, kind):
    """Large civic and campus halls."""
    s = STYLES[style]
    p = Plan(w, h, s['wall'], s['floor'], rng)
    if kind == 'senate':
        # Tiered benches face a raised speaker's seat under banners; a carpet runs up the aisle.
        for x in range(1, w - 1, 4):
            p.g[0][x] = 'n'
        for x in (3, w - 4):
            for y in range(3, h - 3, 4):
                p.put(x, y, 'I')
        p.put(w // 2, 2, 'e')
        p.put(w // 2 - 2, 2, 'i')
        p.put(w // 2 + 2, 2, 'i')
        p.run(w // 2 - 4, 4, 1, 0, 9, 'T')
        for y in range(7, h - 3, 2):
            p.run(5, y, 1, 0, w // 2 - 7, 'p')
            p.run(w // 2 + 3, y, 1, 0, w // 2 - 8, 'p')
        for y in range(5, h - 1):
            if p.g[y][w // 2] == p.floor:
                p.g[y][w // 2] = 'R'
        p.worker(w // 2, 3)
    elif kind == 'chapel':
        for x in (4, w - 5):
            for y in range(3, h - 3, 3):
                p.put(x, y, 'I')
        p.put(w // 2, 2, 'a')
        p.put(w // 2 - 3, 2, 'i')
        p.put(w // 2 + 3, 2, 'i')
        p.put(w // 2 - 1, 3, 'l')
        for y in range(5, h - 3, 2):
            p.run(6, y, 1, 0, w // 2 - 7, 'p')
            p.run(w // 2 + 2, y, 1, 0, w // 2 - 8, 'p')
        for y in range(4, h - 1):
            if p.g[y][w // 2] == p.floor:
                p.g[y][w // 2] = 'R'
        p.worker(w // 2, 4)
    elif kind == 'hearing':
        p.put(w // 2, 2, 'e')
        p.put(w // 2 - 2, 2, 'l')
        p.put(w // 2 + 2, 2, 'l')
        p.run(w // 2 - 3, 4, 1, 0, 7, 'T')
        for y in range(6, h - 3, 2):
            p.run(2, y, 1, 0, w // 2 - 3, 'p')
            p.run(w // 2 + 2, y, 1, 0, w // 2 - 3, 'p')
        p.g[0][w // 2] = 'n'
        p.worker(w // 2, 3)
    elif kind == 'refectory':
        p.run(1, 1, 1, 0, 4, 'v')
        p.put(5, 1, 'h')
        p.run(1, 3, 1, 0, 8, '=')
        p.worker(4, 2)
        for y in range(6, h - 3, 3):
            for x0 in range(3, w - 6, 12):
                p.run(x0, y, 1, 0, 8, 'T')
                p.run(x0, y - 1, 1, 0, 8, 'p')
                p.run(x0, y + 1, 1, 0, 8, 'p')
        p.put(w - 2, 1, 'O')
        p.put(w - 3, 1, 'O')
        p.put(w - 2, 2, 'x')
    elif kind == 'barracks':
        for x in range(2, w - 2, 3):
            if (x, 1) not in p.keep:
                p.bed(x, 1, 'z' if style == 'training' and w < 20 else 'b')
            if (x, h - 3) not in p.keep and h > 8:
                p.bed(x, h - 3, 'z' if style == 'training' and w < 20 else 'b')
        p.run(w // 2 - 3, h // 2 - 1, 1, 0, 6, 'T')
        p.put(1, h // 2, 'y')
        p.put(w - 2, h // 2, 'y')
        p.put(w - 2, h // 2 - 1, 'x')
    elif kind == 'leader':
        # The Warden leader's hall: a bare stone chamber, a high seat, a long table for the council.
        p.put(w // 2, 2, 'e')
        p.put(w // 2 - 3, 1, 'i')
        p.put(w // 2 + 3, 1, 'i')
        p.run(w // 2 - 5, 5, 1, 0, 11, 'T')
        p.run(w // 2 - 5, 4, 1, 0, 11, 'c')
        p.run(w // 2 - 5, 6, 1, 0, 11, 'c')
        for x in (2, w - 3):
            for y in range(2, h - 2, 3):
                p.put(x, y, 'I')
        p.put(1, 1, 'y')
        p.put(w - 2, 1, 'y')
        p.worker(w // 2, 3)
    elif kind == 'guard':
        p.run(1, 1, 1, 0, 3, 'y')
        for x in range(5, w - 1, 2):
            p.bed(x, 1)
        p.run(2, 4, 1, 0, 3, 'T')
        p.put(w - 2, h - 2, 'x')
        p.worker(3, 5)
    elif kind == 'healer':
        for x in range(2, w - 2, 3):
            p.bed(x, 1)
        p.run(1, 4, 1, 0, 4, 'k')
        p.put(w - 2, 4, 'h')
        p.run(w // 2 - 1, 5, 1, 0, 3, 'T')
        p.worker(w // 2, 6)
    elif kind == 'warehouse':
        for y in range(1, h - 3, 2):
            for x in range(1, w - 1):
                if x % 4 != 0:
                    p.put(x, y, rng.choice('xxO'))
        p.worker(w // 2, h - 3)
    elif kind == 'reading':
        for x in range(1, w - 1):
            p.put(x, 1, 'k')
        for y in range(4, h - 3, 3):
            for x in range(2, w - 2, 5):
                p.put(x, y, 'l')
                p.run(x + 1, y, 1, 0, 2, 'T')
        p.worker(w // 2, 3)
    return p


def stacks(rng, style, w, h, deep: bool):
    """A floor of a library: shelf rows with aisles; `deep` makes it a catacomb of niches and tombs."""
    s = STYLES[style]
    p = Plan(w, h, s['wall'], s['floor'], rng)
    p.g[h - 1][w // 2] = s['wall']
    p.keep = set()
    for y in range(2, h - 2, 3):
        for x in range(2, w - 2):
            if x % 6 not in (0, 1):
                p.put(x, y, 'K' if deep and rng.random() < .3 else 'k')
    if deep:
        for x in range(1, w - 1, 2):
            p.g[0][x] = 'C'
            p.g[h - 1][x] = 'C'
        for y in range(1, h - 1, 2):
            p.g[y][0] = 'C'
            p.g[y][w - 1] = 'C'
        for x in range(4, w - 4, 7):
            p.put(x, h - 3, 'g')
        p.put(1, 1, 'i')
        p.put(w - 2, 1, 'i')
    p.put(1, h - 2, 'l')
    return p


# A shop: its goods along the back wall, its label, and its fixtures (x, y, tile, station or None; negative x or y
# counts from the far edge). The fixtures of the trades that were here before doc 35 are unchanged (residents are
# placed by shuffling a room's floor, so one more fixture would move them all); doc 35 only says which are stations.
# `oven_goods`: the goods row's ovens are stations too.
TRADES = {
    'general': {'goods': 'kxO', 'label': 'General goods'},
    'smith': {'goods': 'yxy', 'label': 'Smithy', 'fixtures': [(2, -3, 'A', 'forge'), (3, -3, 'h', 'hearth'), (-3, -3, 'O', None)]},
    'armorer': {'goods': 'yyx', 'label': 'Armorer', 'fixtures': [(2, -3, 'A', 'forge')]},
    'baker': {'goods': 'vvx', 'label': 'Bakery', 'fixtures': [(2, -3, 'T', 'workbench'), (3, -3, 'T', 'workbench')], 'oven_goods': True},
    'butcher': {'goods': 'OTx', 'label': 'Butcher', 'fixtures': [(2, -3, 'T', 'butcher_block'), (3, -3, 'T', 'butcher_block')]},
    'tanner': {'goods': 'OOx', 'label': 'Tannery', 'fixtures': [(2, -3, '~', 'tanning_pits'), (3, -3, '~', 'tanning_pits'), (-3, -3, 'O', None)]},
    'apothecary': {'goods': 'kkO', 'label': 'Apothecary', 'fixtures': [(2, -3, 'T', 'apothecary_bench')]},
    'herbalist': {'goods': 'kOk', 'label': 'Herbalist', 'fixtures': [(2, -3, 'T', 'apothecary_bench')]},
    'tailor': {'goods': 'kxk', 'label': 'Tailor', 'fixtures': [(2, -3, 'T', 'tailor_table'), (3, -3, 'T', 'tailor_table')]},
    'weaver': {'goods': 'xkx', 'label': 'Weaver', 'fixtures': [(2, -3, 'T', 'loom'), (-3, -3, 'T', 'spinning_wheel')]},
    'carpenter': {'goods': 'xxT', 'label': 'Carpenter', 'fixtures': [(2, -3, 'T', 'workbench'), (-3, -3, 'x', None)]},
    'potter': {'goods': 'kkx', 'label': 'Potter', 'fixtures': [(2, -3, 'v', 'kiln')]},
    'jeweler': {'goods': 'kxk', 'label': 'Jeweler'},
    'scribe': {'goods': 'kKk', 'label': 'Scribe', 'fixtures': [(2, -3, 'l', 'scribe_desk'), (-3, -3, 'l', 'scribe_desk')]},
    'chandler': {'goods': 'kOk', 'label': 'Chandler', 'fixtures': [(2, -3, 'h', 'chandler_kettle')]},
    'fletcher': {'goods': 'yxk', 'label': 'Fletcher', 'fixtures': [(2, -3, 'T', 'workbench')]},
    'provisioner': {'goods': 'xOx', 'label': 'Provisioner'},
    'cooper': {'goods': 'OOO', 'label': 'Cooper', 'fixtures': [(2, -3, 'x', 'workbench')]},
    'mason': {'goods': 'xox', 'label': 'Stonemason', 'fixtures': [(2, -3, 'o', 'mason_banker'), (-3, -3, 'o', 'mason_banker')]},
    'fishmonger': {'goods': 'OTO', 'label': 'Fishmonger', 'fixtures': [(2, -3, '~', None)]},
    'cartographer': {'goods': 'kKk', 'label': 'Cartographer', 'fixtures': [(2, -3, 'T', 'scribe_desk'), (3, -3, 'l', 'scribe_desk')]},
    'brewer': {'goods': 'OOO', 'label': 'Brewer', 'fixtures': [(2, -3, 'O', 'brew_kettle'), (3, -3, 'O', 'brew_kettle'), (-3, -3, 'v', 'kiln')]},
    'tinker': {'goods': 'xkx', 'label': 'Tinker', 'fixtures': [(2, -3, 'A', 'forge')]},
    'moneychanger': {'goods': 'kxk', 'label': 'Moneychanger'},
    # Doc 35's new shops.
    'perfumer': {'goods': 'kOk', 'label': 'Perfumer', 'fixtures': [(2, -3, 'O', 'still'), (3, -3, 'O', 'still'), (-3, -3, 'T', 'apothecary_bench')]},
    'printer': {'goods': 'kKk', 'label': 'Printing house', 'fixtures': [(2, -3, 'x', 'printing_press'), (-3, -3, '{', 'crucible'), (-4, -3, 'T', 'apothecary_bench')]},
    'saddler': {'goods': 'xkx', 'label': 'Harness-maker', 'fixtures': [(2, -3, 'T', 'leather_bench'), (-3, -3, 'T', 'leather_bench')]},
    'glassblower': {'goods': 'kxk', 'label': 'Glassblower', 'fixtures': [(2, -3, '{', 'glass_furnace'), (-3, -3, 'o', 'grindstone')]},
}


# -- the two great cities ---------------------------------------------------------------------------------------

def tenement(rng, style, beds=6):
    """A crowded rented house: sleeping places along every wall, one hearth, one table."""
    s = STYLES[style]
    w, h = rng.choice([(13, 10), (14, 10), (12, 11)])
    p = Plan(w, h, s['wall'], s['home_floor'], rng)
    p.put(w // 2, 1, 'h')
    p.run(w // 2 - 1, 4, 1, 0, 3, 'T')
    spots = [(1, 1), (1, 3), (1, 5), (w - 2, 1), (w - 2, 3), (w - 2, 5), (3, 1), (w - 4, 1), (1, 7), (w - 2, 7)]
    for x, y in spots[:beds]:
        p.bed(x, y, rng.choice('bz'))
    p.put(1, h - 2, rng.choice('xO'))
    return p


def manor(rng, style, family, servants):
    """A noble House's seat: a great hall below (high seat, long table, the House's banners) and the family's
    chambers above; the servants sleep in the undercroft beside the kitchens."""
    s = STYLES[style]
    w, h = 26, 17
    lower = Plan(w, h, s['wall'], s['floor'], rng)
    for x in range(2, w - 2, 4):
        lower.g[0][x] = 'n'
    lower.put(w // 2, 2, 'e')
    lower.put(w // 2 - 2, 2, 'i')
    lower.put(w // 2 + 2, 2, 'i')
    lower.run(w // 2 - 6, 6, 1, 0, 13, 'T')
    lower.run(w // 2 - 6, 5, 1, 0, 13, 'c')
    lower.run(w // 2 - 6, 7, 1, 0, 13, 'c')
    lower.put(1, 1, 'h')
    lower.put(w - 2, 1, 'h')
    for y in range(9, h - 1):
        if lower.g[y][w // 2] == lower.floor:
            lower.g[y][w // 2] = 'R'
    lower.worker(w // 2, 3)
    lower.worker(w // 2 - 4, 8)
    upper = Plan(w, h, s['wall'], s['home_floor'], rng)
    upper.g[h - 1][w // 2] = s['wall']
    upper.keep = set()
    upper.partition(x=w // 2, gap=h // 2)
    upper.partition(y=h // 2 + 1, gap=w // 4)
    for i, (x, y) in enumerate([(2, 2), (w // 2 - 3, 2), (w // 2 + 2, 2), (w - 3, 2), (2, h - 3), (w - 3, h - 3),
                                (w // 2 + 2, h - 3)][:family]):
        upper.bed(x, y)
    upper.put(w // 2 - 3, h - 3, 'k')
    upper.put(w - 5, 5, 'h')
    upper.run(3, 5, 1, 0, 4, 'T')
    below = Plan(w, h, s['wall'], s['home_floor'], rng)
    below.g[h - 1][w // 2] = s['wall']
    below.keep = set()
    below.run(1, 1, 1, 0, 5, 'v')
    below.put(6, 1, 'h')
    below.run(2, 3, 1, 0, 6, 'T')
    below.worker(4, 4)
    for i in range(servants):
        below.bed(w - 3 - 2 * (i % 5), 1 + 3 * (i // 5), 'z')
    below.run(1, h - 3, 1, 0, 6, 'O')
    stairs_between(lower, upper, 2, h - 3)
    stairs_between(lower, below, w - 3, h - 3)
    return lower, upper, below, [(('', 2, h - 3), ('up', 2, h - 3)), (('', w - 3, h - 3), ('down', w - 3, h - 3))]


WORKS = {  # kind: (size, what fills it)
    'ironworks': ((24, 15), 'furnaces along the back wall, anvils before them, pig iron stacked by the door'),
    'glassworks': ((22, 14), 'a glory-hole furnace in the middle, benches around it, crates of cooling glass'),
    'sawmill': ((26, 14), 'saw benches in rows between stacks of sawn cedar; the wheel outside drives them'),
    'smokehouse': ((18, 12), 'rows of racks over smouldering braziers; the smoke finds its way into everything'),
    'ropewalk': ((44, 7), 'a long shed where hemp is twisted into rope along its whole length'),
    'shipwright': ((22, 14), 'timbers, frames and tools; a half-shaped keel on trestles'),
    'fishmarket': ((22, 14), 'long slab tables, barrels of brine and a gutter to the sea'),
    'quarry_office': ((14, 10), 'a tally table, picks and wedges, and a map of the faces pinned to the wall'),
    'kilnhouse': ((18, 12), 'charcoal sacks to the rafters and a kiln that never cools'),
    # Doc 35's industry: big workshops laid out from their stations (WORKS_STATIONS).
    'tannery': ((22, 13), 'lime pits and bark-liquor pits in rows, hides pegged on frames, and a smell that reaches '
                'the next street'),
    'foundry': ((20, 13), 'crucible furnaces, casting pits of sand and the moulds of bells, buckles and type'),
    'dyeworks': ((20, 12), 'steaming vats of blue, red and yellow, and cloth hung dripping from the beams'),
    'papermill': ((20, 12), 'rag vats, a stamping mill and stacks of new sheets pressed between felts'),
    'brickworks': ((22, 13), 'kilns along the back wall and ranks of drying bricks and tiles on the floor'),
    'limeworks': ((18, 12), 'a lime kiln breathing white dust and a mason\'s banker by the door'),
    'saltworks': ((22, 12), 'broad shallow pans over slow fires, and salt raked into grey heaps'),
    'mill': ((16, 12), 'a pair of millstones turned by the wheel outside, and flour in the air like fog'),
    'arsenal': ((24, 14), 'engineers\' frames where emplacement crossbows are built, a forge for the prods and racks '
                'of great bolts'),
    'cartwright': ((20, 12), 'wheels on stands, axles on trestles, and a forge for the tyres'),
    'weaving_shed': ((22, 12), 'looms in two rows, spinning wheels by the windows and the clack of shuttles'),
    'brewery': ((20, 12), 'brewing coppers, mash tuns and casks to the ceiling'),
    'stables': ((20, 11), 'stalls of heavy draught horses, hay to the rafters and harness on every peg'),
    # Doc 42's industry outside the gates (worldgen.industry).
    'dairy': ((16, 11), 'churns and cheese presses, and cool shelves of rounds ripening in the dark'),
    'press_house': ((16, 11), 'a great screw press, treading vats and casks stacked along the walls'),
    'mine_head': ((14, 10), 'the shaft head with its windlass, ore barrows, and lamps on pegs by the ladder'),
    'fold': ((16, 10), 'pens of hurdles, a shearing floor and bales of fleece'),
    'bee_shed': ((10, 8), 'straw skeps on shelves, smokers, and crocks of honey sealed with wax'),
    'logging_camp': ((18, 10), 'axes, saws and wedges on the walls, bunks by the stove, and the smell of cedar'),
    # Doc 42's sheep and rabbit farms (worldgen.farms, the user, 2026-10-06).
    'barn': ((18, 11), 'lambing pens of hurdles, a shearing floor, and hay to the rafters'),
    'rabbitry': ((24, 14), 'rows of hutches stacked three high, straw and greens, and a skinning bench by the door'),
    # Doc 42's plaster works (the user, 2026-10-06): Ser Ferro's, from the mines' quicklime and the farms' fur.
    'plasterworks': ((20, 12), 'a lime kiln at the back, troughs of slaked lime, bales of combed fur and tubs of plaster '
                     'by the door'),
    # Doc 61's building trade (House Greatly's): the masons' yard and the colour works.
    'masons_yard': ((22, 13), 'bankers in rows under an open roof, blocks waiting to be dressed, and mortar boards by '
                    'the door'),
    'colour_works': ((18, 12), 'grinding slabs and mullers, crocks of ochre and umber, and linseed oil warming by the '
                     'stove'),
}

# Each new works kind: its stations in order of importance, and how many of each.
WORKS_STATIONS = {
    'tannery': [('tanning_pits', 6), ('leather_bench', 2), ('rendering_pot', 1)],
    'foundry': [('crucible', 3), ('smelter', 1), ('workbench', 2)],
    'dyeworks': [('dye_vat', 6), ('tailor_table', 1)],
    'papermill': [('paper_vat', 3), ('press', 2)],
    'brickworks': [('kiln', 4), ('workbench', 2)],
    'limeworks': [('lime_kiln', 2), ('mason_banker', 2)],
    'saltworks': [('salt_pan', 6)],
    'mill': [('millstones', 2)],
    'arsenal': [('siege_frame', 2), ('forge', 2), ('workbench', 2)],
    'cartwright': [('workbench', 4), ('forge', 1)],
    'weaving_shed': [('loom', 6), ('spinning_wheel', 3)],
    'brewery': [('brew_kettle', 4), ('press', 1), ('kiln', 1)],
    'stables': [],
    'dairy': [('press', 2), ('workbench', 1)],
    'press_house': [('press', 2)],
    'mine_head': [('workbench', 2)],
    'fold': [('workbench', 1)],
    'bee_shed': [('workbench', 1)],
    'logging_camp': [('workbench', 2)],
    'barn': [],
    'rabbitry': [],
    'plasterworks': [('lime_kiln', 2), ('mason_banker', 2)],
    'masons_yard': [('mason_banker', 4), ('lime_kiln', 1)],
    'colour_works': [('workbench', 3), ('press', 1)],
}
# What its keeper is called (towns use it for the work label) and the trade whose skill it uses.
WORKS_TRADE = {
    'ironworks': 'smith', 'glassworks': 'glassblower', 'sawmill': 'carpenter', 'smokehouse': 'fishmonger',
    'ropewalk': 'chandler', 'shipwright': 'shipwright', 'fishmarket': 'fishmonger', 'quarry_office': 'mason',
    'kilnhouse': 'charcoal burner', 'tannery': 'tanner', 'foundry': 'founder', 'dyeworks': 'dyer',
    'papermill': 'papermaker', 'brickworks': 'brickmaker', 'limeworks': 'lime burner', 'saltworks': 'salter',
    'mill': 'miller', 'arsenal': 'engineer', 'cartwright': 'cartwright', 'weaving_shed': 'weaver',
    'brewery': 'brewer', 'stables': 'horse-dealer', 'dairy': 'dairy keeper', 'press_house': 'vintner',
    'mine_head': 'mine captain', 'fold': 'shepherd', 'bee_shed': 'beekeeper', 'logging_camp': 'logging boss',
    'barn': 'shepherd', 'rabbitry': 'rabbit keeper', 'plasterworks': 'plasterer', 'masons_yard': 'mason',
    'colour_works': 'colourman',
}


def works(rng, style, kind, workers=4):
    """A place of industry: the trade's fixtures in rows with aisles, and places for its hands to work."""
    s = STYLES[style]
    (w, h), _ = WORKS[kind]
    p = Plan(w, h, s['wall'], s['floor'], rng)
    if kind in WORKS_STATIONS:
        spots = laid_out(p, kind)
    elif kind == 'ironworks':
        for x in range(2, w - 2, 4):
            p.station(x, 1, 'smelter')
            p.put(x + 1, 1, '{')
            p.station(x, 3, 'forge')
        for x in range(2, w - 2, 3):
            p.put(x, h - 3, 'x')
        spots = [(x + 1, 4) for x in range(2, w - 2, 4)]
    elif kind == 'glassworks':
        p.station(w // 2, h // 2 - 2, 'glass_furnace')
        for dx in (-1, 0, 1):
            for dy in (-1, 0):
                p.put(w // 2 + dx, h // 2 + dy - 1, '{')
        for x in (3, w - 4):
            p.run(x, 3, 0, 1, 5, 'T')
        p.run(2, h - 3, 1, 0, w - 4, 'x')
        spots = [(w // 2 - 3, h // 2 - 1), (w // 2 + 3, h // 2 - 1), (4, 4), (w - 5, 4), (w // 2, h // 2 + 1)]
    elif kind == 'sawmill':
        for y in range(2, h - 3, 4):
            p.station(w // 2 - 2, y, 'sawmill')
            p.run(3, y, 1, 0, 6, 'T')
            p.run(w - 9, y, 1, 0, 6, 'T')
            p.run(w // 2 - 2, y, 1, 0, 4, '<')
        p.g[h // 2][w - 1] = '}'
        spots = [(5, 3), (w - 7, 3), (5, 7), (w - 7, 7), (5, 11), (w - 7, 11)]
    elif kind == 'smokehouse':
        for y in range(2, h - 3, 3):
            p.station(2, y, 'smokehouse')
            p.run(3, y, 1, 0, w - 5, '|')
            p.put(w // 2 - 3, y + 1, 'i')
            p.put(w // 2 + 3, y + 1, 'i')
        spots = [(2, h - 3), (w - 3, h - 3), (4, 3), (w - 5, 3)]
    elif kind == 'ropewalk':
        p.station(1, 2, 'ropewalk', 'O')
        p.put(w - 2, 2, 'O')
        for x in range(4, w - 4, 6):
            p.put(x, 1, 'x')
        spots = [(3, 3), (w // 3, 3), (2 * w // 3, 3), (w - 4, 3)]
    elif kind == 'shipwright':
        p.run(3, 3, 1, 0, w - 6, '<')
        p.station(3, 7, 'workbench')
        p.run(4, 7, 1, 0, w - 7, 'T')
        p.station(2, 1, 'forge')
        p.put(w - 3, 1, 'y')
        spots = [(4, 5), (w - 5, 5), (w // 2, 5), (w // 2, 8)]
    elif kind == 'fishmarket':
        for y in range(3, h - 3, 3):
            p.run(3, y, 1, 0, w - 6, 'T')
        p.run(1, 1, 1, 0, 5, 'O')
        spots = [(4, 4), (w - 5, 4), (w // 2, 7), (4, 10)]
    elif kind == 'quarry_office':
        p.run(3, 3, 1, 0, 4, 'T')
        p.put(1, 1, 'y')
        p.put(w - 2, 1, 'x')
        spots = [(5, 4), (w - 3, 4)]
    else:  # kilnhouse
        p.run(1, 1, 1, 0, w - 2, 'x')
        p.station(w // 2, h // 2, 'charcoal_clamp')
        spots = [(w // 2 - 2, h // 2), (w // 2 + 2, h // 2), (3, h - 3)]
    for x, y in spots[:max(1, workers)]:
        p.worker(x, y)
    return p


def laid_out(p: Plan, kind):
    """A new works kind: its stations along the back wall and down the side walls, a work spot in front of each,
    stock by the door. Returns the work spots."""
    w, h = p.w, p.h
    if kind == 'stables':
        for x in range(2, w - 2, 3):          # Stalls along the back, hay bales between, harness racks by the door.
            p.put(x, 1, 'z')
            p.put(x + 1, 2, '|')
        p.run(2, h - 3, 1, 0, 4, 'x')
        return [(w // 2, 4), (3, 4), (w - 4, 4)]
    if kind == 'barn':
        for x in range(2, w - 2, 4):          # Lambing pens along the back (straw behind hurdles), hay by the door.
            p.put(x, 1, 'z')
            p.put(x + 1, 1, 'z')
            p.put(x + 2, 2, '|')
        p.run(2, h - 3, 1, 0, 5, 'x')
        return [(w // 2, 4), (3, 4), (w - 4, 4), (w // 2, h - 5)]
    if kind == 'rabbitry':
        # Rows of hutches (crates of straw, stacked) down the shed with aisles between, a skinning bench by the door.
        rows = list(range(2, h - 4, 3))
        for y in rows:
            for x in range(2, w - 2):
                p.put(x, y, 'x' if x % 3 else 'z')
        p.put(w - 3, h - 3, 'T')
        return [(x, y + 1) for y in rows for x in range(3, w - 3, 5)]
    places = [(x, 1) for x in range(2, w - 2, 3)] + [(1, y) for y in range(4, h - 4, 3)] + \
             [(w - 2, y) for y in range(4, h - 4, 3)] + [(x, h // 2) for x in range(4, w - 4, 4)]
    spots = []
    i = 0
    for station, n in WORKS_STATIONS[kind]:
        for _ in range(n):
            while i < len(places):
                x, y = places[i]
                i += 1
                if p.station(x, y, station):
                    front = (x, y + 1) if y == 1 else (x + 1, y) if x == 1 else (x - 1, y) if x == w - 2 else (x, y + 1)
                    spots.append(front)
                    break
    for x in range(2, w // 2 - 2, 2):
        p.put(x, h - 2, 'x' if x % 4 else 'O')
    return spots


def villa(rng, style, family, servants):
    """A wealthy family's town house: a salon with a courtyard fountain below, bedchambers above."""
    s = STYLES[style]
    w, h = 20, 14
    lower = Plan(w, h, s['wall'], s['floor'], rng)
    lower.put(w // 2, h // 2 - 1, 'F')
    for x in (3, w - 4):
        lower.put(x, 2, 'I')
        lower.put(x, h - 4, 'I')
    lower.run(2, 1, 1, 0, 4, 'k')
    lower.put(w - 3, 1, 'h')
    lower.run(w - 7, 4, 1, 0, 3, 'T')
    lower.put(w - 8, 4, 'c')
    lower.put(w - 4, 4, 'c')
    for i in range(servants):
        lower.bed(1 + 2 * i, h - 2 if i % 2 == 0 else h - 3, 'z')
    lower.worker(w // 2 + 2, h // 2)
    upper = Plan(w, h, s['wall'], s['home_floor'], rng)
    upper.g[h - 1][w // 2] = s['wall']
    upper.keep = set()
    upper.partition(x=w // 2, gap=h // 2)
    for x, y in [(2, 2), (w - 3, 2), (2, h - 3), (w - 3, h - 3), (w // 2 - 3, 2), (w // 2 + 2, 2)][:family]:
        upper.bed(x, y)
    upper.put(w // 2 + 2, h - 3, 'R')
    stairs_between(lower, upper, 2, h - 3 if servants < 1 else 5)
    return lower, upper, [(('', 2, h - 3 if servants < 1 else 5), ('up', 2, h - 3 if servants < 1 else 5))]


def cathedral(rng, style):
    """A great nave with pillared aisles, the altar and the saint's statue at its head, and a crypt beneath."""
    s = STYLES[style]
    w, h = 40, 26
    nave = Plan(w, h, s['wall'], s['floor'], rng)
    for x in (6, w - 7):
        for y in range(3, h - 3, 3):
            nave.put(x, y, 'I')
    for x in range(3, w - 3, 5):
        nave.g[0][x] = 'j'
    nave.put(w // 2, 2, 'a')
    nave.put(w // 2, 1, 'S')
    for x in (w // 2 - 4, w // 2 + 4):
        nave.put(x, 2, 'i')
    nave.put(w // 2 - 2, 4, 'l')
    for y in range(6, h - 3, 2):
        nave.run(8, y, 1, 0, w // 2 - 9, 'p')
        nave.run(w // 2 + 2, y, 1, 0, w // 2 - 10, 'p')
    for y in range(4, h - 1):
        if nave.g[y][w // 2] == nave.floor:
            nave.g[y][w // 2] = 'R'
    nave.worker(w // 2 + 1, 3)
    nave.worker(w // 2 - 1, 3)
    nave.worker(3, 3)
    crypt = stacks(rng, style, w - 10, h - 8, deep=True)
    stairs_between(nave, crypt, 3, h - 10)
    return nave, crypt, [(('', 3, h - 10), ('crypt', 3, h - 10))]


def palace(rng, style, royals, servants):
    """The prince's palace: the throne room of the court below, the royal apartments above, and the kitchens and
    servants' hall beneath."""
    s = STYLES[style]
    w, h = 36, 22
    court = Plan(w, h, s['wall'], s['floor'], rng)
    for x in range(2, w - 2, 3):
        court.g[0][x] = 'n'
    court.put(w // 2, 2, 'e')
    court.put(w // 2 - 2, 2, 'e')
    for x in (w // 2 - 5, w // 2 + 4):
        court.put(x, 2, 'i')
    for x in (5, w - 6):
        for y in range(4, h - 3, 3):
            court.put(x, y, 'I')
    for y in range(3, h - 1):
        if court.g[y][w // 2] == court.floor:
            court.g[y][w // 2] = 'R'
    for x, y in ((8, 6), (w - 9, 6), (8, h - 6), (w - 9, h - 6)):
        court.put(x, y, 'S')
    court.worker(w // 2 + 2, 3)
    court.worker(w // 2 - 4, 4)
    court.worker(w // 2 + 3, 5)
    royal = Plan(w, h, s['wall'], s['home_floor'], rng)
    royal.g[h - 1][w // 2] = s['wall']
    royal.keep = set()
    royal.partition(x=w // 3, gap=h // 2)
    royal.partition(x=2 * w // 3, gap=h // 2)
    for x, y in [(2, 2), (w // 3 + 2, 2), (2 * w // 3 + 2, 2), (w - 3, 2), (2, h - 3), (w - 3, h - 3)][:royals]:
        royal.bed(x, y)
    royal.run(w // 3 + 3, h // 2 + 2, 1, 0, 6, 'R')
    royal.put(w // 2, h - 4, 'F')
    below = Plan(w, h, s['wall'], s['home_floor'], rng)
    below.g[h - 1][w // 2] = s['wall']
    below.keep = set()
    below.run(1, 1, 1, 0, 8, 'v')
    below.put(9, 1, 'h')
    below.run(2, 4, 1, 0, 10, 'T')
    below.worker(5, 3)
    below.worker(8, 5)
    for i in range(servants):
        below.bed(w - 3 - 2 * (i % 8), 1 + 3 * (i // 8), 'z')
    below.run(1, h - 3, 1, 0, 8, 'O')
    stairs_between(court, royal, 3, h - 3)
    stairs_between(court, below, w - 4, h - 3)
    return court, royal, below, [(('', 3, h - 3), ('up', 3, h - 3)), (('', w - 4, h - 3), ('down', w - 4, h - 3))]


# -- doc 67: Ridgemere 2.0's pieces --------------------------------------------------------------------------------
# New recipes and works kinds only: nothing above changes its output (the old map is made exactly as before).

WORKS.update({
    'distillery': ((18, 12), 'copper stills over slow fires, a worm-tub of cold water and casks of spirit ageing in rows'),
    'preserving_house': ((18, 12), 'preserving pans on the hearths, racks of apple rings drying, and crocks sealed with '
                         'wax'),
    'builders_works': ((24, 14), 'a forge for hinges, nails and cramps at one end, joiners\' benches at the other, and '
                       'stacked timber, stone and tile between'),
    'washhouse': ((22, 13), 'coppers steaming over their fires, tubs and wringers in rows, and wet linen hung from every '
                  'beam'),
    'rag_and_bone': ((16, 12), 'heaps of rags, old iron, bottles and broken chairs, sorted into piles nobody else could '
                     'read'),
    'bathhouse': ((24, 16), 'a hot pool and a cold, benches round the walls and a stove that roars all day'),
})
WORKS_STATIONS.update({
    'distillery': [('still', 3), ('workbench', 1)],
    'preserving_house': [('hearth', 2), ('drying_rack', 3), ('workbench', 1)],
    'builders_works': [('forge', 2), ('workbench', 3)],
    'washhouse': [],
    'rag_and_bone': [],
    'bathhouse': [],
})
WORKS_TRADE.update({
    'distillery': 'distiller', 'preserving_house': 'preserver', 'builders_works': 'builder', 'washhouse': 'launderer',
    'rag_and_bone': 'rag-and-bone dealer', 'bathhouse': 'bath keeper',
})
# Doc 67's food rebalance: the shops a city of food sellers needs (TRADES is only ever read by key), and the carters'
# yard that takes the city's goods out to the country and brings the country's in.
TRADES.update({
    'cheesemaker': {'goods': 'OxO', 'label': 'Cheesemonger', 'fixtures': [(2, -3, 'x', 'press'), (3, -3, 'T', 'workbench')]},
    'preserver': {'goods': 'OkO', 'label': 'Preserving shop', 'fixtures': [(2, -3, 'h', 'hearth'),
                                                                           (-3, -3, '|', 'drying_rack')]},
    'cook': {'goods': 'OxO', 'label': 'Cookshop', 'fixtures': [(2, -3, 'h', 'hearth'), (3, -3, 'h', 'hearth'),
                                                               (-3, -3, 'T', None)]},
})
WORKS['bakehouse'] = ((14, 10), 'a bread oven across the back wall, a kneading trough and boards, and sacks of flour '
                      'by the door')
WORKS_STATIONS['bakehouse'] = [('oven', 3), ('workbench', 2)]
WORKS_TRADE['bakehouse'] = 'baker'
WORKS['carters_yard'] =((24, 14), 'a wagon shed of four bays, harness on the posts and a yard of beaten earth where the '
                         'teams are put to before dawn')
WORKS_STATIONS['carters_yard'] = []
WORKS_TRADE['carters_yard'] = 'carter'


def plain_works(p: Plan, kind):
    """Doc 67's works without stations: their furniture, and the places their hands work."""
    w, h = p.w, p.h
    if kind == 'washhouse':                      # Coppers along the back, tubs in rows.
        for x in range(2, w - 2, 3):
            p.put(x, 1, 'h')
        spots = []
        for y in range(4, h - 4, 3):
            for x in range(3, w - 4, 4):
                p.put(x, y, 'O')
                p.put(x + 1, y, 'O')
                spots.append((x + 2, y))
        return spots
    if kind == 'bathhouse':                      # Two pools of shallow water, benches below them, a stove.
        for y in range(3, h // 2 - 1):
            for x in range(3, w // 2 - 2):
                p.g[y][x] = '~'
            for x in range(w // 2 + 2, w - 3):
                p.g[y][x] = '~'
        p.run(2, h // 2 + 1, 1, 0, w // 2 - 3, 'p')
        p.run(w // 2 + 2, h // 2 + 1, 1, 0, w // 2 - 4, 'p')
        p.put(1, 1, 'h')
        return [(w // 2, 3), (w // 2, h // 2), (3, h - 3)]
    if kind == 'carters_yard':                   # Wagon bays along the back, stalls for the teams, an open yard.
        spots = []
        for x in range(2, w - 4, 5):             # Each bay: a wagon (crates on its bed) under the shed roof.
            for y in (2, 3):
                p.put(x, y, 'x')
                p.put(x + 1, y, 'x')
            p.put(x + 3, 1, 'z')                 # Straw for the horses between the bays.
            spots.append((x + 2, 4))
        p.run(1, h // 2, 1, 0, w - 2, '|')       # The shed's rail; the yard below it, open earth.
        for x in (w // 2 - 1, w // 2, w // 2 + 1, 3, w - 4):
            p.g[h // 2][x] = p.floor
        for y in range(h // 2 + 1, h - 1):
            for x in range(1, w - 1):
                if (x, y) not in p.keep and p.g[y][x] == p.floor:
                    p.g[y][x] = 'd'
        p.put(w - 3, h - 3, 'U')                  # The trough.
        p.put(2, h - 3, 'O')
        return spots + [(w // 2, h - 3)]
    # rag_and_bone: heaps of crates and barrels.
    for y in range(2, h - 4, 3):
        for x in range(2, w - 2):
            if x % 5 != 0:
                p.put(x, y, 'x' if (x + y) % 3 else 'O')
    return [(5, 3), (w - 6, 3), (w // 2, h - 3)]


def works_kit(rng, style, kind, workers=4, size=None):
    """A works of doc 67 (or any laid-out WORKS kind at another size): its stations laid out, its hands' places."""
    s = STYLES[style]
    (w, h), _ = WORKS[kind]
    if size:
        w, h = size
    p = Plan(w, h, s['wall'], s['floor'], rng)
    if WORKS_STATIONS.get(kind):
        spots = laid_out(p, kind)
    else:
        spots = plain_works(p, kind)
    for x, y in spots[:max(1, workers)]:
        p.worker(x, y)
    if not p.work:
        p.worker(w // 2, h - 3)
    return p


def home_floor(p: Plan, beds, rng):
    """A family's room: a hearth, a table and a stool, beds along the walls (as many as asked that fit)."""
    w, h = p.w, p.h
    p.put(w // 2, 1, 'h')
    if w >= 8 and h >= 7:
        p.run(w // 2 - 1, h // 2, 1, 0, 2, 'T')
        p.put(w // 2 - 2, h // 2, 'c')
    spots = [(1, 1), (w - 2, 1), (1, 3), (w - 2, 3), (1, 5), (w - 2, 5), (3, 1), (w - 4, 1), (1, 7), (w - 2, 7)]
    n = 0
    for x, y in spots:
        if n >= beds:
            break
        if 0 < x < w - 1 and 0 < y < h - 1 and p.free(x, y):
            p.bed(x, y, rng.choice('bbz'))
            n += 1
    if rng.random() < .6:
        p.put(w - 2, h - 2, rng.choice('xOk'))
    return n


def flats(rng, style, homes=2, beds=(3, 3, 3), size=(12, 10), notch=None, back=False):
    """Up to three homes stacked in one house (doc 67): a home on each floor at z 0, 1 and 2, each its own interior with
    its own beds, joined by stairs on different tiles; the ground floor holds the street door (and a back door for a
    through-building). Returns (plans, stair pairs); the floors' keys are '', 'up' and 'top'."""
    s = STYLES[style]
    w, h = size
    homes = max(1, min(3, homes))
    plans = []
    for i in range(homes):
        p = Plan(w, h, s['wall'], s['home_floor'], rng)
        if i > 0:
            p.g[h - 1][w // 2] = s['wall']            # Upstairs has no street door.
            p.keep = set()
        if notch:
            p.notch(*notch)
        if i == 0 and back:
            p.back_door()
        plans.append(p)
    pairs = []
    keys = ['', 'up', 'top']
    # Stairs: floor 0 to 1 by one side wall, 1 to 2 by the other, both clear of the doors.
    for i in range(1, homes):
        sx, sy = (w - 2, h - 3) if i == 1 else (1, h - 3)
        stairs_between(plans[i - 1], plans[i], sx, sy)
        pairs.append(((keys[i - 1], sx, sy), (keys[i], sx, sy)))
    for i, p in enumerate(plans):
        home_floor(p, beds[i] if i < len(beds) else beds[-1], rng)
    return plans, pairs


def market_hall(rng, style, w=20, h=48, counters=6):
    """A covered market hall, drawn with its long side up the page (its doors on both short ends, the bottom and the
    top): six stall counters along the long walls, each with its keeper's place behind it and an open tile before it,
    and the warden's post by the street door. No player places: those are on the square (doc 67, Q8)."""
    s = STYLES[style]
    p = Plan(w, h, s['wall'], s['floor'], rng)
    p.back_door()
    keepers = []
    rows = [h // 4, h // 2, 3 * h // 4]
    for y in rows:
        for x_counter, x_keeper in ((3, 2), (w - 4, w - 3)):
            if len(keepers) >= counters:
                break
            p.run(x_counter, y - 1, 0, 1, 3, '=')
            p.put(x_keeper, y - 2, 'x')
            p.put(x_keeper, y + 2, 'O')
            keepers.append((x_keeper, y))
    for x, y in keepers:
        p.worker(x, y)
    p.worker(w // 2 + 2, h - 3)                      # The warden, by the street door.
    for y in range(5, h - 5, 6):
        p.put(w // 2 - 3, y, 'I')                     # The roof's posts.
        p.put(w // 2 + 3, y, 'I')
    return p


def gaol_cells(rng, style, w=22, h=20, cells=8):
    """The cells beneath the Keep: a corridor between two rows of 3 x 3 cells, each fronted by bars (|) with a one-tile
    opening and a bench at the back, the gaoler's post at the corridor's end. (Links have no lock and nothing closes:
    the bars are cosmetic.) It has no street door: the stairs come down into the corridor."""
    s = STYLES[style]
    p = Plan(w, h, s['wall'], s['floor'], rng)
    p.g[h - 1][w // 2] = s['wall']
    p.keep = set()
    per_row = max(1, cells // 2)
    for top in (True, False):
        y0 = 1 if top else h - 4                       # The cell's three rows of floor.
        bars = y0 + 3 if top else y0 - 1
        for i in range(per_row):
            x0 = 2 + i * 4
            if x0 + 3 >= w - 1:
                break
            for y in range(y0, y0 + 3):
                p.g[y][x0 - 1] = s['wall']
                p.g[y][x0 + 3] = s['wall']
            for x in range(x0, x0 + 3):
                p.g[bars][x] = '|'
            p.g[bars][x0 + 1] = '+'                   # The cell's opening.
            p.g[y0 if top else y0 + 2][x0 + 1] = 'p'   # The bench, at the back.
    p.worker(w - 3, h // 2)
    p.put(w - 2, h // 2 - 1, 'T')
    return p


def office_floor(rng, style, w, h, desks=6, door=True):
    """A floor of desks: rows of tables, each with a lectern and a clerk's place, cabinets along the back wall."""
    s = STYLES[style]
    p = Plan(w, h, s['wall'], s['floor'], rng)
    if not door:
        p.g[h - 1][w // 2] = s['wall']
        p.keep = set()
    for x in range(1, w - 1):
        if x % 4:
            p.put(x, 1, 'k')
    n = 0
    for y in range(4, h - 3, 3):
        for x in range(2, w - 3, 5):
            if n >= desks:
                break
            if p.free(x, y) and p.free(x + 1, y) and p.free(x, y + 1):
                p.put(x, y, 'T')
                p.put(x + 1, y, 'l')
                p.worker(x, y + 1)
                n += 1
    return p


def keep_tower(rng, style, w=22, h=20):
    """The Keep's tower: the Watch Office on the courtyard (desks, the duty sergeant's table, racks by the door), the
    captain's rooms and the Watch's weapon racks above, and the cells below. Returns (plans, stair pairs): '', 'up',
    'down'."""
    s = STYLES[style]
    office = office_floor(rng, style, w, h, desks=4)
    office.put(1, h - 2, 'y')
    office.put(w - 2, h - 2, 'y')
    office.worker(w // 2 + 2, 3)
    up = Plan(w, h, s['wall'], s['floor'], rng)
    up.g[h - 1][w // 2] = s['wall']
    up.keep = set()
    up.partition(x=w // 2, gap=h // 2)
    for y in range(2, h - 2, 2):
        up.put(w - 2, y, 'y')                          # The Watch's weapon racks.
    up.put(4, 2, 'k')
    up.run(3, 5, 1, 0, 4, 'T')
    up.worker(5, 6)
    up.worker(w - 4, h // 2)
    cells = gaol_cells(rng, style, w, h)
    stairs_between(office, up, 2, h - 3)
    stairs_between(office, cells, w - 3, h // 2)
    return [office, up, cells], [(('', 2, h - 3), ('up', 2, h - 3)), (('', w - 3, h // 2), ('down', w - 3, h // 2))]


def palace_antechamber(rng, style, w, h):
    """The Great Palace's antechamber: the court and senate where the Council of Houses sits, the senate hall's layout
    scaled to the block: benches, a dais, and the great door on the forecourt."""
    return hall(rng, style, w, h, 'senate')


def palace_wing(rng, style, w, h, desks):
    """The Administrative Wing: two floors of offices. Returns (plans, stair pairs)."""
    low = office_floor(rng, style, w, h, desks=desks)
    up = office_floor(rng, style, w, h, desks=desks, door=False)
    stairs_between(low, up, w - 3, h - 3)
    return [low, up], [(('', w - 3, h - 3), ('up', w - 3, h - 3))]


def palace_residence(rng, style, w, h, royals, servants):
    """The King's Residence: the receiving rooms on the forecourt, the royal bedchambers above, and the kitchens and the
    servants' hall beneath (as palace() lays them). Returns (plans, stair pairs): '', 'up', 'down'."""
    s = STYLES[style]
    court = Plan(w, h, s['wall'], s['floor'], rng)
    for x in range(2, w - 2, 3):
        court.g[0][x] = 'n'
    court.put(w // 2, 2, 'e')
    for x in (5, w - 6):
        for y in range(4, h - 3, 3):
            court.put(x, y, 'I')
    for y in range(3, h - 1):
        if court.g[y][w // 2] == court.floor:
            court.g[y][w // 2] = 'R'
    court.worker(w // 2 + 2, 3)
    court.worker(w // 2 - 3, 4)
    royal = Plan(w, h, s['wall'], s['home_floor'], rng)
    royal.g[h - 1][w // 2] = s['wall']
    royal.keep = set()
    royal.partition(x=w // 3, gap=h // 2)
    royal.partition(x=2 * w // 3, gap=h // 2)
    for x, y in [(2, 2), (w // 3 + 2, 2), (2 * w // 3 + 2, 2), (w - 3, 2), (2, h - 3), (w - 3, h - 3)][:max(2, royals)]:
        royal.bed(x, y)
    royal.put(w // 2, h - 4, 'F')
    below = Plan(w, h, s['wall'], s['home_floor'], rng)
    below.g[h - 1][w // 2] = s['wall']
    below.keep = set()
    below.run(1, 1, 1, 0, 8, 'v')
    below.station(9, 1, 'hearth', 'h')
    below.run(2, 4, 1, 0, 10, 'T')
    below.worker(5, 3)
    below.worker(8, 5)
    for i in range(max(2, servants)):
        below.bed(w - 3 - 2 * (i % 8), 1 + 3 * (i // 8), 'z')
    below.run(1, h - 3, 1, 0, 8, 'O')
    stairs_between(court, royal, 3, h - 3)
    stairs_between(court, below, w - 4, h - 3)
    return [court, royal, below], [(('', 3, h - 3), ('up', 3, h - 3)), (('', w - 4, h - 3), ('down', w - 4, h - 3))]


def dorter(rng, style, cells=10, w=20, h=12):
    """The abbey's dorter: a dormitory of cells, a pallet in each, along both long walls."""
    s = STYLES[style]
    p = Plan(w, h, s['wall'], s['home_floor'], rng)
    n = 0
    for x in range(2, w - 2, 2):
        for y in (1, h - 3):
            if n < cells and p.free(x, y):
                p.bed(x, y, 'z')
                n += 1
    p.run(w // 2 - 2, h // 2, 1, 0, 4, 'T')
    p.put(1, h // 2, 'k')
    p.worker(w // 2, h // 2 + 1)
    return p


def shed(w, h):
    """A shed, privy or lean-to on the outdoor map: solid, with no interior (rows to stamp)."""
    if h <= 2:
        return ['#' * w] * h
    return ['#' * w] + ['#' + 'L' * (w - 2) + '#' for _ in range(h - 2)] + ['#' * w]


def boarded(w, h):
    """A boarded-up house on the outdoor map: walls with boarded windows round a fallen roof, solid, no interior."""
    rows = []
    for y in range(h):
        if y in (0, h - 1):
            rows.append(''.join('|' if 0 < x < w - 1 and x % 3 == 1 else '#' for x in range(w)))
        else:
            rows.append('#' + ''.join('Z' if (x * 7 + y * 3) % 5 else 'L' for x in range(1, w - 1)) + '#')
    return rows
