"""The walled city of Upper Accord: streets, the market plaza, and some hundred and thirty buildings by ward."""
from __future__ import annotations

import random

import numpy as np

from . import field
from .buildings import Building, TRADES, house, hall, shop_with_flat, tavern
from .names import HOUSE_PROSE, TRADE_PROSE, Namer
from .site import Lots, Site

PLAZA = (385, 392)
WARDS = {  # Where each ward's buildings cluster.
    'gate': (318, 414), 'market': PLAZA, 'crafts': (438, 452), 'hill': (366, 318),
    'northeast': (446, 322), 'west': (306, 372), 'southwest': (336, 452), 'east': (466, 418),
}
SHOPS = {  # Trade: (how many, ward)
    'general': (3, 'market'), 'smith': (2, 'crafts'), 'armorer': (2, 'gate'), 'baker': (3, 'market'),
    'butcher': (2, 'market'), 'tanner': (1, 'crafts'), 'apothecary': (2, 'hill'), 'herbalist': (2, 'hill'),
    'tailor': (2, 'market'), 'weaver': (2, 'crafts'), 'carpenter': (2, 'crafts'), 'potter': (1, 'crafts'),
    'jeweler': (1, 'market'), 'scribe': (2, 'hill'), 'chandler': (2, 'market'), 'fletcher': (2, 'gate'),
    'provisioner': (2, 'gate'), 'cooper': (1, 'crafts'), 'mason': (1, 'crafts'), 'fishmonger': (1, 'market'),
    'cartographer': (1, 'hill'), 'brewer': (1, 'crafts'), 'tinker': (1, 'crafts'), 'moneychanger': (1, 'market'),
}


def footprint(w, h, scale=.72):
    return max(6, round(w * scale)), max(5, round(h * scale * .9))


def make_shop(rng, namer, trade, ward):
    # A shop, and the flat above it where its keeper's family lives (Docs/Design/39).
    name = namer.shop(trade)
    rooms, stairs, (w, h) = shop_with_flat(rng, 'city', trade, name, TRADE_PROSE[trade])
    return Building('shop', name, 'city', footprint(w, h), rooms, stairs, district=ward, roof='L', trade=trade)


def make_house(rng, namer, ward, people):
    p = house(rng, 'city', people)
    name = namer.house()
    room = p.room('', name, rng.choice(HOUSE_PROSE))
    return Building('house', name, 'city', footprint(p.w, p.h), [room], district=ward, roof=rng.choice('ZZL'))


def make_tavern(rng, namer, ward, inn):
    plans, stairs = tavern(rng, 'city', inn)
    name = namer.inn() if inn else namer.tavern()
    rooms = [plans[0].room('', name, 'Low beams, a long bar and the din of the gate road; the fire is never allowed out.'
                           if inn else 'Smoke, song and spilled ale under old stone vaults; every table has a story.')]
    pairs = []
    if inn:
        rooms.append(plans[1].room('up', f'{name}, upstairs', 'A corridor of small sleeping rooms under the roof.',
                                   z=1))
        _, x, y = stairs[0]
        pairs.append((('', x, y), ('up', x, y)))
    b = Building('inn' if inn else 'tavern', name, 'city', footprint(plans[0].w, plans[0].h), rooms, pairs, ward)
    return b


def make_civic(rng, kind, name, description, w, h, ward, style='civic'):
    p = hall(rng, style, w, h, kind)
    return Building(kind, name, style, footprint(w, h), [p.room('', name, description)], district=ward)


def streets(c, inner):
    """Cobbled ring road inside the wall, three avenues from the gates to the plaza, and a grid of lanes."""
    from .upper_accord import MAIN_GATE, SOUTH_GATE, STAIR_GATE
    wall_distance = field.distance_to_mask(~inner, 8)
    street = inner & (wall_distance <= 4)
    avenues = [
        [MAIN_GATE, (330, 410), (360, 398), PLAZA],
        [PLAZA, (380, 440), SOUTH_GATE],
        [PLAZA, (374, 340), STAIR_GATE],
        [PLAZA, (430, 384), (486, 370)],
    ]
    for points in avenues:
        street |= c.line_mask(points, 6)
    for x in range(280, 500, 21):
        street |= c.line_mask([(x, 250), (x + 6, 520)], 3)
    for y in range(284, 510, 17):
        street |= c.line_mask([(250, y), (520, y - 5)], 3)
    street &= inner
    plaza = np.zeros_like(inner)
    plaza[PLAZA[1] - 12:PLAZA[1] + 13, PLAZA[0] - 16:PLAZA[0] + 17] = True
    return street, plaza & inner


# Where the plaza's stallholders stand (world tiles), behind every other stall (Docs/Design/39); filled by plaza_fixtures.
STALL_PLACES: list[tuple[int, int]] = []


def plaza_fixtures(c):
    px, py = PLAZA
    c.stamp(px - 1, py - 1, ['FFF', 'FSF', 'FFF'], 0.0)
    STALL_PLACES.clear()
    for x in range(px - 13, px + 14, 3):
        for y in (py - 9, py - 6, py + 6, py + 9):
            if abs(x - px) > 3:
                c.stamp(x, y, ['u'], 0.0)
                # Each pair of rows faces an aisle between them; the stallholder stands on the far side.
                if (x - px + 13) % 6 == 0:
                    STALL_PLACES.append((x, y - 1 if y in (py - 9, py + 6) else y + 1))
    for x, y in ((px - 14, py - 3), (px + 14, py - 3), (px - 14, py + 3), (px + 14, py + 3)):
        c.stamp(x, y, ['Y'], 0.0)
    for x, y in ((px - 15, py - 11), (px + 15, py - 11), (px - 15, py + 11), (px + 15, py + 11)):
        c.stamp(x, y, ['U'], 0.0)
    for x in (px - 5, px + 5):
        c.stamp(x, py, ['p'], 0.0)


def build_city(site: Site, inner, seed):
    c = site.canvas
    rng = random.Random(seed)
    namer = Namer(seed)
    street, plaza = streets(c, inner)
    c.paint(inner, '.', 0.0)
    c.paint(street | plaza, '_', 0.0)
    plaza_fixtures(c)
    lots = Lots(inner & ~street & ~plaza, street | plaza)

    wanted = []
    wanted.append(make_civic(rng, 'barracks', 'Gate Watch Barracks', 'Bunks in rows, racks of spears, the smell of '
                             'wet wool: the city watch sleeps here between shifts at the Main Gate.', 28, 14, 'gate'))
    wanted.append(make_civic(rng, 'barracks', 'South Watch Barracks', 'The south gate\'s watch keeps its bunks, '
                             'boots and grudges here.', 24, 13, 'southwest'))
    for gate, ward in (('Main Gate', 'gate'), ('South Gate', 'southwest'), ('Stair Gate', 'hill')):
        wanted.append(make_civic(rng, 'guard', f'{gate} Guardhouse', f'A cramped stone guardhouse beside the '
                                 f'{gate}: a table of tallies, a rack of spears, two cots.', 12, 8, ward))
    wanted.append(make_civic(rng, 'healer', 'House of Mending', 'Clean straw, boiled water and quiet; the city\'s '
                             'healers tend the hurt and the old here.', 20, 12, 'hill'))
    wanted.append(make_civic(rng, 'healer', 'Southside Infirmary', 'A plainer place of mending for the crafts ward, '
                             'smelling of salve and smoke.', 16, 10, 'crafts'))
    wanted.append(make_civic(rng, 'reading', 'Hall of Records', 'Deeds, tallies and the city\'s rolls of residents, '
                             'kept on long shelves by patient clerks.', 22, 14, 'hill'))
    for i, ward in enumerate(('gate', 'crafts', 'east')):
        wanted.append(make_civic(rng, 'warehouse', ('Gate', 'Crafts', 'East')[i] + ' Warehouse', 'Crates and casks '
                                 'stacked to the beams, tallied and chalk-marked.', 20, 12, ward, 'city'))
    wanted.append(make_tavern(rng, namer, 'gate', True))
    wanted.append(make_tavern(rng, namer, 'market', True))
    for ward in ('gate', 'market', 'crafts'):
        wanted.append(make_tavern(rng, namer, ward, False))
    for trade, (count, ward) in SHOPS.items():
        for _ in range(count):
            wanted.append(make_shop(rng, namer, trade, ward))
    homes = ['northeast', 'west', 'southwest', 'east', 'hill', 'crafts', 'gate', 'market']
    for i in range(80):
        wanted.append(make_house(rng, namer, homes[i % len(homes)], rng.choice([2, 2, 3, 3, 4])))

    placed = []
    for b in wanted:
        near = WARDS[b.district]
        jitter = (near[0] + rng.uniform(-20, 20), near[1] + rng.uniform(-20, 20))
        fw, fh = b.footprint
        for _, x0, y0, facing, w, h in lots.candidates(fw, fh, jitter, limit=100000):
            if lots.clear(x0, y0, w, h):
                bid, outside = site.place(b, x0, y0, facing, 'upper_accord', open_door=b.kind != 'house')
                lots.take(x0, y0, w, h)
                placed.append((bid, b, outside))
                break
    yards(c, lots.open, rng)
    return placed, street | plaza


def yards(c, open_ground, rng):
    """What is left between the buildings: packed-earth yards, garden grass, a few trees and wells."""
    noise = field.fbm(c.width, c.height, 10, 99, 2)
    grass = open_ground & (noise > .55)
    c.codes[grass] = ord(',')
    grain = np.random.default_rng(5).random(open_ground.shape)
    trees = open_ground & (noise > .62) & (grain < .08)
    c.codes[trees] = ord('Y')
    wells = open_ground & (grain > .9985)
    c.codes[wells] = ord('U')
