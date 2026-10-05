#!/usr/bin/env python3
"""Supply and demand for the whole world (Docs/Design/42-money-in-circulation.md, Phase 3c).

  python3 tools/world_build.py export DIR          # DEV's newest build as files
  python3 tools/supply_balance.py DIR [--town T]   # what the world uses up a day, against what it can bring in and make
  python3 tools/supply_balance.py PROJECT.json     # the same for an Atlas project (a worldgen pass's --out)
  python3 tools/supply_balance.py DIR --regions    # write Data/Items/regions.json: each place's specialties and lacks

From the residents of a world export and Data/Items/crafts.json, as the game runs them:

- Demand a day: food (every wolf eats about 50 nourishment a day, bought by nourishment for the price, as
  Society::buyFood chooses), the households' needs, and the town buyers' baskets. Then, through the crafts, what those
  goods are made from, down to what the land gives.
- Supply a day: every producer's yield (one spell a working hour, in its seasons; the year's average) and what the
  workshops that make each good could make (their batches are quick, so a workshop is limited by its materials, not
  its hours; it is counted so a good with nobody to make it shows).

It prints every good with demand, what is produced, the ratio, and who makes it, and flags goods with no source at all,
too little of it (under 1.2 times demand), or demand in a town with no maker or producer of it (it must be carted in).
All the game's numbers are placeholders, and so are these estimates.
"""
from __future__ import annotations

import json
import shlex
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIR = ROOT / 'Data/Items'
HUNGER_A_DAY = .0035 * 14400                 # Society: hunger a decision (a world second), a game day of them.
EAT_FACTOR = 1.1                              # A food's nourishment counts 1.1 times when eaten (RatwDemand.cpp).


def load(name):
    return json.loads((DIR / name).read_text(encoding='utf-8'))


ITEMS = {i['id']: i for i in load('items.json')['items']}
CRAFTS = load('crafts.json')
BUSINESSES = [b for b in load('businesses.json')['businesses'] if b.get('match')]


def first_match(label: str, rules):
    low = label.lower()
    for rule in rules:
        if any(m and m in low for m in rule['match']):
            return rule
    return None


def residents(export: Path):
    if export.suffix == '.json':                  # An Atlas project (a worldgen pass's --out), not yet built.
        project = json.loads(export.read_text(encoding='utf-8'))
        region = {a['id']: a['territory']['region'] for a in project['cells'] + project['rooms']}
        out = [{'id': p['id'], 'role': p['role'], 'label': p['workLabel'], 'age': p['age'], 'start': p['hours']['start'],
                'end': p['hours']['end'], 'home': p['home']['cell'], 'work': p['work']['cell']} for p in project['people']]
        for r in out:
            r['town'] = region.get(r['home'], '?')
            r['home_town'] = r['town']
        return out
    region, out = {}, []
    for line in (export / 'world.ratw').read_text(encoding='utf-8').splitlines():
        if line.startswith('territory '):
            t = shlex.split(line)
            region[t[1]] = t[2]
        elif line.startswith('resident '):
            t = shlex.split(line)
            out.append({'id': t[1], 'role': t[3], 'label': t[4], 'age': int(t[7]), 'start': float(t[17]),
                        'end': float(t[18]), 'home': t[23], 'work': t[26]})
    for r in out:
        r['home_town'] = region.get(r['home'], '?')
    # A place's work out in the country (a farm, a mine in a wild region) is the place its workers live in.
    settled = Counter(r['home_town'] for r in out)
    for r in out:
        work = region.get(r['work'])
        r['town'] = work if work and settled[work] >= 5 else r['home_town']
    return out


def hours(r):
    return (r['end'] - r['start']) % 24 or 24


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    export = Path(argv[1])
    only = argv[argv.index('--town') + 1] if '--town' in argv else None
    folk = residents(export)
    producers = CRAFTS['producers']
    crafts = CRAFTS['crafts']
    made_by = defaultdict(list)                   # good -> crafts making it
    for k in crafts:
        for good in k['out']:
            made_by[good].append(k)
    # Who brings in and who makes, by town.
    supply = defaultdict(lambda: defaultdict(float))       # town -> good -> a day
    makers = defaultdict(lambda: defaultdict(int))         # town -> business -> how many
    bringers = defaultdict(Counter)                        # good -> producer kind -> how many
    people = Counter()
    households = defaultdict(set)
    grown = Counter()
    guards = Counter()
    workers = defaultdict(Counter)                         # town -> producer -> how many
    for r in folk:
        town = r['home_town']
        people[town] += 1
        households[town].add(r['home'])
        grown[town] += r['age'] >= 16
        guards[town] += r['role'] == 'guard'
        p = first_match(r['label'], producers)
        if p and r['age'] >= 16:
            seasons = p.get('seasons') or [0, 1, 2, 3]
            spells = hours(r) * min(1.0, 3600 / p['seconds'])
            workers[town][p['id']] += 1
            off = p.get('offSeason', {})
            for good in set(p['out']) | set(off):
                supply[r['town']][good] += spells * (p['out'].get(good, 0) * len(seasons) + off.get(good, 0) * (4 - len(seasons))) / 4
                bringers[good][p['id']] += 1
        if r['role'] == 'merchant':
            b = first_match(r['label'], BUSINESSES)
            if b:
                makers[r['town']][b['id']] += 1
    # Demand a day, by town: food, households, the town's buyers.
    foods = [i for i in ITEMS.values() if i.get('category') == 'food' and i.get('food', {}).get('nourish', 0) > 0
             and not i['food'].get('drink')]
    demand = defaultdict(lambda: defaultdict(float))
    for town, n in people.items():
        if only and town != only:
            continue
        local = {k['id'] for b in makers[town] for k in crafts if b in k['makers']}
        sold = set()
        for k in crafts:
            if k['id'] in local:
                sold |= {g for g in k['out'] if g in ITEMS and ITEMS[g].get('category') == 'food'}
        sold |= {'meal'} if makers[town] else set()
        choice = [f for f in foods if f['id'] in sold] or [ITEMS['meal']]
        # By nourishment for the price (taste averages out): each food's share of the money-worth.
        score = {f['id']: f['food']['nourish'] / max(1, f['price']) for f in choice}
        total = sum(score.values())
        need = n * HUNGER_A_DAY
        for f in choice:
            share = score[f['id']] / total
            demand[town][f['id']] += need * share / (f['food']['nourish'] * EAT_FACTOR)
        for want in CRAFTS['households']['needs']:
            every = want['everyDays']
            count = (grown[town] if want.get('perPerson') else len(households[town])) / every
            # Any one of them, whichever a shop has: shared by what the world brings in of each (made goods evenly).
            made = [g for g in want['any'] if g in made_by]
            weight = {g: (1 if g in made else sum(supply[t].get(g, 0) for t in supply)) for g in want['any']}
            total = sum(weight.values()) or len(want['any'])
            for good in want['any']:
                demand[town][good] += count * (weight[good] or (0 if sum(weight.values()) else 1)) / total
        for t in CRAFTS.get('tools', []):
            demand[town][t['item']] += workers[town][t['producer']] / t['everyDays']
        for business, basket in CRAFTS.get('upkeep', {}).items():
            for good, rate in basket.items():
                demand[town][good] += makers[town].get(business, 0) * rate
        for inst in CRAFTS['institutions']['list']:
            per = inst['per']
            if n < inst.get('minResidents', 0):
                continue
            scale = guards[town] if per == 'guards' else workers[town][per[9:]] if per.startswith('producer:') else \
                (n / 100 if n >= 5 else 0)
            for good, rate in inst['basket'].items():
                demand[town][good] += rate * scale
    # Down through the crafts: what each made good needs.
    def expand(town_demand):
        out = defaultdict(float)
        stack = list(town_demand.items())
        while stack:
            good, n = stack.pop()
            out[good] += n
            k = made_by.get(good, [None])[0]
            if k:
                for raw, m in k['in'].items():
                    stack.append((raw, n * m / k['out'][good]))
        return out
    world_demand, world_supply = defaultdict(float), defaultdict(float)
    carted = defaultdict(list)
    for town in demand:
        full = expand(demand[town])
        for good, n in full.items():
            world_demand[good] += n
            local_maker = any(b in k['makers'] for k in made_by.get(good, []) for b in makers[town])
            if n >= 1 and not local_maker and supply[town].get(good, 0) < n:
                carted[good].append(town)
    for town in supply:
        if only and town != only:
            continue
        for good, n in supply[town].items():
            world_supply[good] += n
    if '--regions' in argv:
        # Data/Items/regions.json: what each place brings in (its specialties, by how much of the world's supply it
        # has) and what it must have carted in.
        world_total = defaultdict(float)
        for town in supply:
            for good, n in supply[town].items():
                world_total[good] += n
        out = {}
        for town in sorted(people):
            if town == '?' or people[town] < 5:
                continue
            special = sorted(((n / world_total[g], g) for g, n in supply[town].items() if world_total[g] and n >= 5),
                             reverse=True)
            lacks = sorted(g for g in carted if town in carted[g])
            out[town] = {'residents': people[town],
                         'specialties': [g for share, g in special if share >= .25][:8],
                         'brings_in': [g for share, g in special][:12],
                         'lacks': lacks}
        path = DIR / 'regions.json'
        path.write_text(json.dumps({'format': 'ratw-regions', 'version': 1,
                                    'about': 'What each place brings in and what it lacks (doc 42, Phase 7), worked out by '
                                             'tools/supply_balance.py --regions from a world export: specialties are goods '
                                             'it brings in a quarter or more of the world\'s supply of; lacks, what its '
                                             'shops need that nobody there makes or brings in, which only caravans bring.',
                                    'places': out}, indent=1, ensure_ascii=False) + '\n', encoding='utf-8')
        print(f'wrote {path}')
        return 0
    print(f'{sum(people.values())} residents in {len(people)} places; a day (year average):')
    print(f"{'good':18} {'demand':>8} {'brought in':>11} {'ratio':>6}  made by / brought in by; short in")
    flags = []
    for good in sorted(world_demand, key=lambda g: -world_demand[g]):
        d = world_demand[good]
        if d < .1:
            continue
        s = world_supply.get(good, 0)
        crafted = made_by.get(good)
        how = ', '.join(sorted({b for k in crafted for b in k['makers']})) if crafted else \
            ', '.join(f'{p} x{c}' for p, c in bringers[good].most_common()) or '-'
        ratio = '' if crafted else f'{s / d:6.2f}' if d else ''
        print(f'{good:18} {d:8.1f} {"" if crafted else f"{s:11.1f}"} {ratio:>6}  {how}'
              + (f'; carted to {len(carted[good])} places' if carted.get(good) else ''))
        if not crafted and s == 0:
            flags.append(f'NO SOURCE: {good} ({d:.0f} a day wanted)')
        elif not crafted and s < 1.2 * d:
            flags.append(f'SHORT: {good}: {s:.0f} brought in against {d:.0f} wanted')
    for good, n in sorted(world_supply.items()):
        if good not in world_demand and n >= 1:
            print(f'{good:18} {"0":>8} {n:11.1f}         (nobody uses it yet)')
    print()
    for f in flags:
        print(f)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
