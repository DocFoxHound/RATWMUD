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
        return r


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
