"""The three peak campuses: Concord Hall, the Warden Training Grounds and the Warden Order."""
from __future__ import annotations

import random

import numpy as np

from .buildings import Building, Plan, STYLES, hall, house, stacks, stairs_between
from .site import Site


def fp(w, h, scale=.75):
    return max(6, round(w * scale)), max(5, round(h * scale * .85))


def civic(rng, style, kind, name, description, w, h, district, roof=''):
    p = hall(rng, style, w, h, kind)
    return Building(kind, name, style, fp(w, h), [p.room('', name, description)], district=district, roof=roof)


def library(rng, style, name, description, levels, w, h, deep_from, level_text, district):
    """A reading hall above floors of stacks, each reached by stairs from the one above."""
    top = hall(rng, style, w, h, 'reading')
    plans = [top] + [stacks(rng, style, w, h, deep=i + 1 >= deep_from) for i in range(levels)]
    pairs = []
    for i in range(levels):
        x, y = (w - 3, 2) if i % 2 == 0 else (2, h - 3)
        stairs_between(plans[i], plans[i + 1], x, y)
        pairs.append(((plans_key(i), x, y), (plans_key(i + 1), x, y)))
    rooms = [top.room('', name, description)]
    for i, p in enumerate(plans[1:], start=1):
        rooms.append(p.room(plans_key(i), f'{name}, {level_text[i - 1][0]}', level_text[i - 1][1], z=-i))
    return Building('library', name, style, fp(w, h, .6), rooms, pairs, district)


def plans_key(i):
    return '' if i == 0 else f'd{i}'


def two_floor(rng, style, kind, name, description, upper_name, upper_text, w, h, district, roof=''):
    """A hall with private quarters upstairs."""
    lower = hall(rng, style, w, h, kind)
    upper = Plan(w, h, STYLES[style]['wall'], STYLES[style]['home_floor'], rng)
    upper.g[h - 1][w // 2] = STYLES[style]['wall']
    upper.keep = set()
    upper.put(w // 2, 1, 'h')
    upper.run(2, 1, 1, 0, 4, 'k')
    upper.bed(w - 3, 2)
    upper.bed(w - 3, 4)
    upper.run(w // 2 - 2, 5, 1, 0, 4, 'T')
    upper.put(w // 2 - 3, 5, 'c')
    upper.partition(x=w - 6, gap=h // 2)
    stairs_between(lower, upper, 2, h - 3)
    rooms = [lower.room('', name, description), upper.room('up', upper_name, upper_text, z=1)]
    return Building(kind, name, style, fp(w, h), rooms, [(('', 2, h - 3), ('up', 2, h - 3))], district, roof)


def concord(site: Site, seed, box):
    rng = random.Random(seed)
    c = site.canvas
    x, y, w, h, level = box
    # Courtyard gardens, statues and braziers on the marble terrace.
    c.rect(x + 34, y + 36, 40, 26, ',', level)
    for gx in range(x + 36, x + 72, 6):
        c.stamp(gx, y + 38, ['Y'], level)
        c.stamp(gx, y + 59, ['Y'], level)
    c.rect(x + 42, y + 44, 24, 10, 'm', level)
    c.stamp(x + 52, y + 47, ['FFF', 'FSF', 'FFF'], level)
    c.stamp(x + 44, y + 46, ['S'], level)
    c.stamp(x + 63, y + 46, ['S'], level)
    for px in range(x + 36, x + 74, 6):
        c.stamp(px, y + 34, ['i'], level)
    c.rect(x + 84, y + 76, 5, h - 78, 'R', level)                   # The processional carpet from the stair gate.
    buildings = [
        (civic(rng, 'concord', 'senate', 'The Grand Hall of Concord', 'The senate chamber of the free packs: tiered '
               'benches of dark wood face the speaker\'s high seat beneath banners of every pack that ever swore '
               'the Accord. Voices carry here; so do silences.', 46, 32, 'concord'), x + 32, y + 3, 'S'),
        (civic(rng, 'concord', 'chapel', 'The Chapel of the First Oath', 'A cathedral nave of pale pillars and '
               'long pews. Braziers burn before an altar carved with the oath that founded the Wardens.',
               30, 36, 'concord'), x + 4, y + 14, 'E'),
        (civic(rng, 'concord', 'hearing', 'The East Hearing Chamber', 'A high-seated chamber where disputes between '
               'packs are heard and judged.', 22, 16, 'concord'), x + 84, y + 6, 'W'),
        (civic(rng, 'concord', 'hearing', 'The Low Hearing Chamber', 'A smaller chamber for petitions, oaths and '
               'the testimony of the humble.', 20, 15, 'concord'), x + 84, y + 26, 'W'),
        (civic(rng, 'concord', 'hearing', 'The Chamber of Witnesses', 'Witnesses wait on benches here to be called '
               'before the councils.', 20, 14, 'concord'), x + 92, y + 56, 'W'),
        (library(rng, 'concord', 'The Concord Annex', 'The great library of the Accord: a long reading hall of '
                 'lecterns and shelves beneath painted beams, and stairs going down into the mountain.', 3, 34, 24, 3,
                 [('the upper stacks', 'Shelf after shelf of bound laws, treaties and pack histories, lit by '
                   'lamps in niches.'),
                  ('the scroll vaults', 'Scroll racks in long galleries; the air is dry and still and smells of '
                   'old vellum.'),
                  ('the deep archive', 'The oldest records of the Accord, some in scripts no one living reads, '
                   'kept in cool rock far below the hall.')], 'concord'), x + 4, y + 60, 'E'),
        (civic(rng, 'concord', 'barracks', 'Clerks\' Quarters', 'Neat rows of beds for the clerks, scribes and '
               'keepers who serve the Hall.', 24, 12, 'concord'), x + 60, y + 76, 'N'),
        (civic(rng, 'concord', 'refectory', 'The Concord Refectory', 'Long tables and a kitchen that feeds the Hall\'s '
               'keepers, and any councillor who forgets to go home.', 28, 14, 'concord'), x + 26, y + 80, 'N'),
    ]
    for b, bx, by, facing in buildings:
        site.place(b, bx, by, facing, 'upper_accord')


def training(site: Site, seed, box):
    rng = random.Random(seed + 1)
    c = site.canvas
    x, y, w, h, level = box
    grain = np.random.default_rng(seed).random((h, w))
    # Drill square, archery range, sparring rings, boulder field and obstacle course down the terrace.
    c.rect(x + 30, y + 70, 40, 30, 'f', level)
    c.stamp(x + 48, y + 84, ['i'], level)
    for ty in range(y + 106, y + 150, 6):
        c.stamp(x + 94, ty, ['t'], level)
    c.rect(x + 40, y + 104, 1, 48, '_', level)                           # The shooting line.
    for rx, ry in ((x + 8, y + 110), (x + 8, y + 130), (x + 8, y + 150)):
        c.outline(rx, ry, 14, 14, 'q', level)
        c.rect(rx + 6, ry + 13, 2, 1, 'd', level)
        c.stamp(rx + 13, ry + 6, ['y'], level)
    for dy in range(0, 20, 4):
        c.rect(x + 30, y + 160 + dy, 24, 1, 'd', level)
        for dx in range(0, 24, 4):
            c.stamp(x + 30 + dx, y + 158 + dy, ['N'], level)
    for yy in range(y + 150, y + 196):
        for xx in range(x + 62, x + 100):
            r = grain[yy - y, xx - x]
            if r < .06:
                c.stamp(xx, yy, ['o'], level)
            elif r < .12:
                c.stamp(xx, yy, ['X'], level)
    c.rect(x + 8, y + 176, 40, 3, 'X', level)
    c.rect(x + 8, y + 184, 40, 2, '~', level)
    for fx in range(x + 10, x + 46, 6):
        c.rect(fx, y + 190, 1, 4, '|', level)
    c.rect(x + 8, y + 198, 40, 3, 's', level)
    buildings = [
        (civic(rng, 'training', 'barracks', 'The Pup Den', 'Straw bedding in long rows and a watchful trainer\'s '
               'cot by the door; the youngest trainees sleep here in a heap.', 18, 12, 'training'), x + 4, y + 4, 'E'),
        (civic(rng, 'training', 'barracks', 'Trainee Barracks', 'Bunks, kit chests and weapon racks; nothing here '
               'belongs to anyone for long.', 30, 14, 'training'), x + 4, y + 22, 'E'),
        (civic(rng, 'training', 'barracks', 'Trainers\' Hall', 'The trainers\' quarters: a little more room, a '
               'little more quiet, and a wall of tallies on every trainee.', 24, 13, 'training'), x + 4, y + 44, 'E'),
        (civic(rng, 'training', 'refectory', 'The Training Cookhouse', 'Long tables, a roaring oven and more food '
               'than seems reasonable, all of it gone by nightfall.', 30, 16, 'training'), x + 60, y + 6, 'W'),
        (civic(rng, 'training', 'warehouse', 'Training Stores', 'Spare targets, rope, blunted blades and sacks of '
               'sand.', 18, 11, 'training'), x + 70, y + 40, 'W'),
    ]
    for b, bx, by, facing in buildings:
        site.place(b, bx, by, facing, 'upper_accord')


def order(site: Site, seed, box):
    rng = random.Random(seed + 2)
    c = site.canvas
    x, y, w, h, level = box
    c.stamp(x + 50, y + 44, ['U'], level)
    for ix in (x + 40, x + 60):
        c.stamp(ix, y + 44, ['i'], level)
    buildings = [
        (two_floor(rng, 'order', 'leader', 'The Leader\'s Hall', 'A bare hall of old grey stone. The Warden leader\'s '
                   'high seat stands at its head, the council table below it; nothing here is carved or gilded, and '
                   'nothing needs to be.', 'The Leader\'s quarters', 'A plain sleeping room, a hearth, shelves of '
                   'reports and maps. The leader lives as the Wardens do.', 36, 22, 'order'), x + 34, y + 3, 'S'),
        (civic(rng, 'order', 'barracks', 'The Old Barracks', 'Rows of bunks under a low vault worn smooth by '
               'centuries of Wardens.', 30, 14, 'order'), x + 4, y + 6, 'E'),
        (civic(rng, 'order', 'barracks', 'The North Barracks', 'More bunks, more kit, the same cold stone.', 28, 13,
               'order'), x + 76, y + 6, 'W'),
        (civic(rng, 'order', 'refectory', 'The Order Refectory', 'Plain food, long benches, and the only warm '
               'room in the compound in winter.', 30, 16, 'order'), x + 72, y + 34, 'W'),
        (library(rng, 'order', 'The Warden Crypt Library', 'A low stone chapel over a stair into the dark: the '
                 'Order keeps its records among its dead.', 3, 26, 20, 1,
                 [('the first catacomb', 'Narrow galleries of burial niches, shelves wedged between them, '
                   'braziers smoking low.'),
                  ('the ossuary stacks', 'Bones in the walls, books on the shelves, and a silence nobody breaks.'),
                  ('the founders\' vault', 'The Order\'s first Wardens lie here in stone, among the oldest '
                   'records it keeps.')], 'order'), x + 4, y + 34, 'E'),
        (civic(rng, 'order', 'guard', 'The Order Armory', 'Racks of spears and harness, kept oiled and counted.',
               16, 10, 'order'), x + 76, y + 62, 'W'),
    ]
    for i in range(8):
        p = house(rng, 'order', rng.choice([2, 3]))
        name = f'Warden House {"I II III IV V VI VII VIII".split()[i]}'
        buildings.append((Building('house', name, 'order', fp(p.w, p.h), [p.room(
            '', name, 'A permanent Warden household: stone walls, a hearth, bedding and little else.')],
            district='order'), x + 6 + i * 12, y + 76, 'N'))
    for b, bx, by, facing in buildings:
        site.place(b, bx, by, facing, 'upper_accord')


def _near_doors(site: Site, reach=2):
    from .site import STEP
    near = set()
    for r in site.manifest:
        sx, sy = STEP[r['facing']]
        ox, oy = r['door']['x'] + sx, r['door']['y'] + sy
        for dy in range(-reach, reach + 1):
            for dx in range(-reach, reach + 1):
                near.add((ox + dx, oy + dy))
    return near


def _dress(site: Site, floor: str):
    """A painter that only touches bare campus floor, never a building or the ground in front of a door."""
    c = site.canvas
    keep = _near_doors(site)

    def put(x, y, ch):
        if c.inside(x, y) and chr(c.codes[y, x]) == floor and (x, y) not in keep:
            c.codes[y, x] = ord(ch)
    return put


def decorate(site: Site, seed, concord_box, training_box, order_box):
    rng = random.Random(seed + 9)
    # Concord Hall: colonnades inside the wall, banners on it, a processional way of statues and braziers.
    x, y, w, h, _ = concord_box
    c = site.canvas
    put = _dress(site, 'm')
    for px in range(x + 4, x + w - 4, 4):
        put(px, y + 3, 'I')
        put(px, y + h - 4, 'I')
    for py in range(y + 4, y + h - 4, 4):
        put(x + 3, py, 'I')
        put(x + w - 4, py, 'I')
    for bx in range(x + 6, x + w - 6, 8):
        for by in (y, y + h - 1):
            if chr(c.codes[by, bx]) == 'M':
                c.codes[by, bx] = ord('n')
    for by in range(y + 6, y + h - 6, 8):
        for bx in (x, x + w - 1):
            if chr(c.codes[by, bx]) == 'M':
                c.codes[by, bx] = ord('n')
    route = [(x + 86, y + h - 3), (x + 86, y + 68), (x + 49, y + 68), (x + 49, y + 28)]
    carpet = c.line_mask(route, 3) & (c.codes == ord('m'))
    c.codes[carpet] = ord('R')
    for sy in range(y + 72, y + h - 4, 5):
        put(x + 83, sy, 'S')
        put(x + 89, sy, 'S')
    for sx in range(x + 54, x + 84, 6):
        put(sx, y + 65, 'i')
        put(sx, y + 71, 'i')
    put = _dress(site, ',')
    for _ in range(40):
        put(rng.randrange(x + 35, x + 74), rng.randrange(y + 37, y + 62), rng.choice('YBB"'))
    for py in (y + 40, y + 57):
        for px in range(x + 38, x + 46):
            put(px, py, '~')
        for px in range(x + 62, x + 70):
            put(px, py, '~')
    # Training Grounds: rocky ground and loose boulders across the yards, racks by the barracks.
    x, y, w, h, _ = training_box
    put = _dress(site, 'd')
    for _ in range(900):
        px, py = rng.randrange(x + 2, x + w - 2), rng.randrange(y + 60, y + h - 2)
        put(px, py, 'r' if rng.random() < .85 else 'o')
    for py in range(y + 4, y + 64, 6):
        put(x + 16, py, 'y')
    for i in range(x + 26, x + 74):
        put(i, y + 66, 'f')
        put(i, y + 104, 'f')
    for i in range(y + 66, y + 105):
        put(x + 26, i, 'f')
        put(x + 74, i, 'f')
    # The Warden Order: an old graveyard, worn ground, racks and braziers; nothing ornamental.
    x, y, w, h, _ = order_box
    put = _dress(site, 'f')
    for gy in range(y + 58, y + 72, 3):
        for gx in range(x + 20, x + 60, 4):
            put(gx, gy, 'g')
    for _ in range(260):
        px, py = rng.randrange(x + 2, x + w - 2), rng.randrange(y + 2, y + h - 2)
        put(px, py, rng.choice('XX,,."'))
    for px in range(x + 20, x + 60, 5):
        put(px, y + 30, 'y')
    for px in (x + 44, x + 52):
        put(px, y + 19, 'i')
