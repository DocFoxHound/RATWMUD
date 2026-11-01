#!/usr/bin/env python3
"""What the Country holds, and what it would take to grow it on real farms (Docs/Design/71-the-country-s-industries.md).

    python3 tools/country_industry.py measure SEASON=EVENTS.json ... --out MEASURED.json
    python3 tools/country_industry.py report MEASURED.json ECONOMY.jsonl [--units Data/Economy/country_units.json]
                                            [--unplaced UNPLACED.json] [--from-year 3]

measure: reads world_check --events files (one short run per season: `--day N --economy F --events E`), skips each
run's first three days (the shelves filling) and its last, partial day, and keeps a day's mean of: every store draw by
good (pieces; the till's pennies to the Country, the gate toll and carriage in, paired from the ledger: a draw's three
coin shifts come just before the `made` record that names its good), why it was drawn (resale, or the craft it was drawn
for), the Country's purchases by good, the residents' purchases and what they ate, by good.

report: scales each season's draws to the long run's settled years (ECONOMY.jsonl, years `--from-year` to 12: each season
group's `G_drawn` a day over the short run's; goods outside the groups take the rest of the long run's drawn pennies; the
gate toll at the long run's rate), sorts the goods into the Country's industries, and prints the doc's tables
(markdown). The food is sized money first (the user, 2026-11-01): each food area's revenue (the Country's part of what
the shops pay, less carriage) pays so many wolves (a keeper's or hand's drifted wage, their own food at city prices and
upkeep); the farms (FARMS: their kinds and counts) get the crews it pays; each farm's yield is its share of the city's
draws and the crews' own food, with SLACK. --units writes Data/Economy/country_farms.json: the placement list and the
farms' settings (crew, wages, yields by season) in the form Atlas's Economy page would edit. --unplaced (a JSON list of
DEV's unplaced NPCs, read only: id, role, work_label, age, appearance) compares the crews with that pool.
--real-check also sizes the food from real pre-industrial yields and labour (UNITS, ASSUMPTIONS: the first method, kept
as a cross-check of land and seasonal labour).

Reproduce (doc 71, "How it was measured"): four 14-day runs of the 12-year run's export and binary, one a season
(`world_check EXPORT --simulate 6 342 --no-check --deterministic --threads 2 --no-profile --day N --economy S.jsonl
--events S_events.json`, N = 30, 122, 213, 303), then `measure`, then `report`.
"""
from __future__ import annotations

import argparse
import collections
import json
import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SEASONS = ['spring', 'summer', 'autumn', 'winter']
SEASON_DAYS = {'spring': 92, 'summer': 92, 'autumn': 91, 'winter': 90}   # RatwCalendar.h
GROUPS = ['grain', 'fruit', 'vegetables', 'fish', 'meat', 'dairy', 'honey', 'preserved']
NOURISH_A_DAY = 46          # doc 68, 3: a resident needs about 46 nourishment a day.
WORKDAYS = 6 / 7            # a Restday in seven: a worker works 313 days a year, 78 a season.

# ---------------------------------------------------------------------------------------------------------------------
# The Country's industries: each good the store room supplies, by who would grow or make it.
INDUSTRY = {
    'grain and milling': ['wheat', 'flour', 'oats', 'barley', 'rye', 'straw', 'hay'],
    'vegetables': ['vegetables', 'mushrooms'],
    'orchards': ['apples', 'berries', 'dried_apples', 'fruit_preserves', 'cider', 'apple_brandy'],
    'vineyards': ['grapes', 'wine', 'brandy', 'aqua_vitae'],
    'livestock: meat': ['raw_meat', 'pig', 'sheep', 'mutton', 'rabbit', 'offal', 'eggs', 'cattle', 'goat', 'meal'],
    'dairy': ['milk', 'butter', 'cheese'],
    'livestock: wool, hides, tallow, bone': ['wool', 'hide', 'sheepskin', 'small_pelt', 'tallow', 'bones', 'horn', 'gut'],
    'bees': ['honey', 'beeswax'],
    'fishing': ['fish', 'shellfish'],
    'salt': ['salt'],
    'woods: timber, firewood, bark, charcoal': ['timber', 'firewood', 'oak_bark', 'resin', 'charcoal', 'planks'],
    'mining and metals': ['iron_ore', 'copper_ore', 'tin_ore', 'iron_bar', 'copper_bar', 'tin_bar', 'bronze_bar'],
    'stone, lime, clay, sand': ['stone', 'limestone', 'clay', 'sand', 'lime'],
    'fibre: hemp, flax, linen': ['hemp', 'flax', 'linen_cloth', 'cord'],
    'herbs': ['herbs', 'wormwood', 'willow_bark', 'garlic', 'thyme', 'comfrey', 'yarrow', 'valerian', 'chamomile', 'mint',
              'lavender'],
    "country workshops' goods": ['clay_pot', 'wool_scarf', 'fine_soap', 'pamphlet', 'paper', 'nails', 'cookpot', 'ink',
                                 'leather', 'bandages', 'iron_hoops'],
}
FOOD_INDUSTRIES = ['grain and milling', 'vegetables', 'orchards', 'vineyards', 'livestock: meat', 'dairy', 'bees',
                   'fishing', 'salt']

# ---------------------------------------------------------------------------------------------------------------------
# ASSUMPTIONS. The game's scale: a tile is about a metre (a walk is 2.6 tiles a second, a wolf's trot); a game day is
# 14,400 s; the year is 365 days (Spring 92, Summer 92, Autumn 91, Winter 90), a Restday in seven. A day's food, 46
# nourishment, stands for about 2,500 kcal, so a point of nourishment is about 54 kcal; each primary good's piece is
# given a real weight from what it makes (a measure of wheat is a flour is six loaves, 90 nourishment, so 1.5 kg).
A = {
    'kcal_a_nourishment': 54,
    # Real weight (kg, or litres for milk) of a piece of each primary good, from its nourishment through its craft.
    'kg': {'wheat': 1.5, 'oats': 1.0, 'milk': 0.7, 'apples': 0.7, 'grapes': 1.4, 'berries': 0.5, 'honey': 0.3,
           'fish': 0.8, 'shellfish': 0.8, 'raw_meat': 0.75, 'pig': 3.0, 'mutton': 0.8, 'rabbit': 0.5, 'salt': 0.5,
           'vegetables': 2.5, 'firewood': 10, 'timber': 30},
    # The map's scale for land (the user decides): fields, pasture, orchards and woods drawn at 1/land_scale of their
    # real area (each tile of field stands for land_scale square metres); farmsteads, yards, mills and wharves at 1:1.
    'land_scale': 10,
}

# Production units: what each is, its real yield, its labour (worker-days a year, all in: the whole work, carting and
# upkeep too, from pre-industrial norms), the share of that labour in each season, and where its output arrives.
UNITS = {
    'grain farm': {
        'goods': ['wheat', 'oats'],
        'about': 'Three-field rotation: a third winter wheat (sown in autumn, reaped in late summer), a third spring oats '
                 '(sown in spring, reaped in autumn), a third fallow (grazed, ploughed twice). The farm keeps its own '
                 'seed (a fifth of the crop) and threshes through the winter.',
        'yield_t_ha': {'wheat': 1.0, 'oats': 1.0}, 'seed_share': 0.2, 'fallow_share': 1 / 3,
        'days_ha': 50, 'days_ha_fallow': 6,
        'season_labour': [0.20, 0.30, 0.35, 0.15], 'harvest': 'late summer (wheat) and autumn (oats); sold from the granary all year',
        'unit_ha': 30, 'region': 'central plains (deep soil, flat, on a river or road)', 'terrain': ['plain', 'meadow'],
        'near': ['road', 'river'], 'kind': 'arable',
    },
    'water mill': {
        'goods': ['flour'],
        'about': 'Grinds the grain farms\' wheat to flour (1 measure to 1 flour, crafts.json): a small mill on a stream '
                 'grinds about a tonne a day, so one mill has time to spare; two are kept for the two farm districts.',
        'per_unit_t_day': 1.0, 'staff': 2, 'season_labour': [0.25, 0.25, 0.25, 0.25],
        'region': 'central plains, on a river below the grain farms', 'terrain': ['river'], 'near': ['river', 'road'],
        'footprint_tiles': 30 * 20, 'kind': 'works',
    },
    'orchard': {
        'goods': ['apples'],
        'about': 'Standard apple trees, about 100 to the hectare, 60 kg a tree when grown; picked from late summer through '
                 'autumn and kept in a fruit store (apples keep for months), so the city can draw them all year.',
        'yield_t_ha': {'apples': 6.0}, 'trees_ha': 100, 'days_ha': 55, 'season_labour': [0.10, 0.20, 0.60, 0.10],
        'harvest': 'late summer to autumn; stored', 'unit_ha': 6,
        'region': 'central hills (south-facing slopes, shelter) and the river valleys', 'terrain': ['hills', 'meadow'],
        'near': ['road'], 'kind': 'orchard',
    },
    'vineyard': {
        'goods': ['grapes', 'wine'],
        'about': 'Vines on warm slopes, about 3 t of grapes a hectare; the city\'s press and brewers draw grapes, and the '
                 'estate presses the rest into wine (2 grapes a bottle\'s worth, wine by the cask).',
        'yield_t_ha': {'grapes': 3.0}, 'days_ha': 120, 'season_labour': [0.25, 0.20, 0.45, 0.10],
        'harvest': 'autumn; wine kept in casks', 'unit_ha': 4,
        'region': 'the warm south-facing hills toward the south-east, on the desert\'s edge (dry, sunny)',
        'terrain': ['hills'], 'near': ['road'], 'kind': 'orchard',
    },
    'market garden': {
        'goods': ['vegetables'],
        'about': 'Intensive beds near the city (dung from its stables and streets), 9 t a hectare, worked by hand.',
        'yield_t_ha': {'vegetables': 9.0}, 'days_ha': 200, 'season_labour': [0.30, 0.30, 0.30, 0.10],
        'harvest': 'summer and autumn, roots stored into winter', 'unit_ha': 1,
        'region': 'just outside the city\'s gates (the NW coast\'s rain suits greens and roots)', 'terrain': ['meadow'],
        'near': ['city', 'road'], 'kind': 'arable',
    },
    'dairy farm': {
        'goods': ['milk'],
        'about': 'Cows giving about 1,000 L a year (3 L a day for 300 days), on wet pasture with hay meadows for winter; '
                 'its calves go to the beef herds. A cow needs about 1 ha of grazing and 0.5 ha of hay meadow.',
        'litres_cow_year': 1000, 'ha_head': 1.5, 'herd_per_milker': 1.3, 'days_head': 25,
        'season_labour': [0.25, 0.30, 0.25, 0.20], 'harvest': 'all year, less in winter', 'unit_head': 12,
        'region': 'the NW rainy coast round Ridgemere (lush grass, near the city: milk keeps a day)',
        'terrain': ['meadow', 'pasture'], 'near': ['city', 'road'], 'kind': 'pasture',
    },
    'goat fold': {
        'goods': ['milk'],
        'about': 'Upland goats, about 1 L a day for 200 days; browse on rough hill land too poor for cows; kid meat too.',
        'litres_head_year': 200, 'ha_head': 0.4, 'days_head': 3, 'season_labour': [0.30, 0.30, 0.25, 0.15],
        'harvest': 'spring to autumn', 'unit_head': 40,
        'region': 'the N/NE mountains\' foothills', 'terrain': ['hills', 'mountain'], 'near': ['road'], 'kind': 'pasture',
    },
    'cattle ranch': {
        'goods': ['raw_meat'],
        'about': 'Beef cattle: a cow calves about 0.8 a year, the steers and spare heifers killed at two and a half, about '
                 '180 kg of meat each (the dairies\' bull calves come here too); about 3.5 head stand on the land for each '
                 'one killed a year; 1.2 ha of grazing a head, with hay for the winter.',
        'kg_slaughtered': 180, 'head_per_slaughter': 3.5, 'ha_head': 1.2, 'days_head': 6,
        'season_labour': [0.30, 0.25, 0.30, 0.15], 'harvest': 'killed through the year (more in autumn, salted)',
        'unit_head': 120, 'meat_share': 0.5,
        'region': 'the central plains and hills (grass), on a drove road to the city', 'terrain': ['plain', 'hills', 'meadow'],
        'near': ['road', 'river'], 'kind': 'pasture',
    },
    'piggery': {
        'goods': ['raw_meat', 'pig'],
        'about': 'Pigs on woodland pannage and sties: a sow raises about 9 a year, killed at a year, about 60 kg of meat '
                 'each; one standing animal for each one killed a year, and a tenth as many sows; 0.2 ha of wood each.',
        'kg_slaughtered': 60, 'head_per_slaughter': 1.1, 'ha_head': 0.2, 'days_head': 2.5,
        'season_labour': [0.20, 0.20, 0.40, 0.20], 'harvest': 'killed in autumn and winter (fattened on the mast)',
        'unit_head': 200, 'meat_share': 0.5,
        'region': 'oak and beech woods of the central hills (pannage)', 'terrain': ['forest', 'hills'],
        'near': ['road'], 'kind': 'wood',
    },
    'sheep run': {
        'goods': ['mutton', 'wool', 'sheepskin'],
        'about': 'A meat flock on downs and hill grazing, about 6 to the hectare: the ewes lamb in spring, the lambs and '
                 'hoggets killed in their first autumn and winter (about 14 kg of mutton each), so about 1.4 sheep stand '
                 'for each one killed a year; 1.5 kg of wool a fleece, shorn in early summer.',
        'kg_slaughtered': 14, 'head_per_slaughter': 1.4, 'ha_head': 0.17, 'days_head': 1.5,
        'season_labour': [0.35, 0.30, 0.20, 0.15], 'harvest': 'lambing in spring, shearing in summer, killing in autumn',
        'unit_head': 400,
        'region': 'the central hills and downs; the N/NE foothills', 'terrain': ['hills', 'downs'], 'near': ['road'],
        'kind': 'pasture',
    },
    'rabbit warren': {
        'goods': ['rabbit', 'small_pelt'],
        'about': 'A walled warren on sandy heath: a doe gives about 25 young a year, about 1.2 kg of meat each (so 30 kg a '
                 'doe), 0.1 ha a doe; the warreners trap, feed in winter and mend the banks.',
        'kg_doe': 30, 'ha_doe': 0.1, 'days_doe': 0.5, 'season_labour': [0.20, 0.25, 0.30, 0.25],
        'harvest': 'all year', 'unit_head': 250,
        'region': 'sandy heath and dunes: the SW marsh\'s coast, or the dry edge of the SE desert', 'terrain': ['heath', 'dunes'],
        'near': ['road'], 'kind': 'pasture',
    },
    'apiary': {
        'goods': ['honey', 'beeswax'],
        'about': 'Straw skeps, about 10 kg of honey a hive a year (taken summer to autumn), 150 hives a beekeeper; set by '
                 'orchards, heather and clover.',
        'kg_hive': 10, 'hives_keeper': 150, 'season_labour': [0.25, 0.35, 0.30, 0.10],
        'harvest': 'summer to autumn; kept in crocks', 'unit_head': 150,
        'region': 'beside the orchards and the heather of the central hills', 'terrain': ['hills', 'meadow'],
        'near': ['orchard'], 'kind': 'yard', 'footprint_tiles': 40 * 30,
    },
    'sea fishing boat': {
        'goods': ['fish', 'shellfish'],
        'about': 'A three-wolf boat with nets and lines off the NW coast, landing about 40 kg a day in its good months '
                 '(the winter herring and cod runs) and less in summer; shellfish raked from the shore beds.',
        'crew': 3, 'kg_day': {'spring': 36, 'summer': 28, 'autumn': 36, 'winter': 48},
        'region': 'the NW coast: Ridgemere Sound and the coves north and south of the city', 'terrain': ['coast'],
        'near': ['sea', 'city'], 'kind': 'wharf', 'footprint_tiles': 30 * 15,
    },
    'lake and river boat': {
        'goods': ['fish'],
        'about': 'A two-wolf boat or a weir with traps on a lake or the rivers (trout, perch, pike); steadier, smaller.',
        'crew': 2, 'kg_day': {'spring': 20, 'summer': 18, 'autumn': 20, 'winter': 14},
        'region': 'the lakes (Glasswater and the others) and the rivers of the plains', 'terrain': ['lake', 'river'],
        'near': ['lake', 'river'], 'kind': 'wharf', 'footprint_tiles': 20 * 12,
    },
    'eel and fowl marsh': {
        'goods': ['fish', 'eggs'],
        'about': 'Eel traps, reed cutting and wildfowling in the SW marsh: eels a steady catch, best in autumn\'s run.',
        'crew': 3, 'kg_day': {'spring': 15, 'summer': 15, 'autumn': 30, 'winter': 9},
        'region': 'the SW marsh (south of Ridgemere along the coast)', 'terrain': ['marsh'], 'near': ['water', 'road'],
        'kind': 'wharf', 'footprint_tiles': 25 * 15,
    },
    'salt works': {
        'goods': ['salt'],
        'about': 'Brine pans: on the SW marsh\'s coast the sea is let into clay pans and the brine boiled over peat and '
                 'wood (wet climate); in the SE desert, dry salt pans raked in summer (sun only, but far to carry).',
        't_ha_year': 20, 'workers_t': 0.25, 'season_labour': [0.15, 0.55, 0.25, 0.05],
        'harvest': 'summer (the sun) and the boiling all year; kept dry in sheds', 'unit_t': 6,
        'region': 'the SW marsh\'s coast (salterns, near the city); a desert pan in the SE for later', 'terrain': ['coast', 'marsh', 'desert'],
        'near': ['sea', 'road'], 'kind': 'works',
    },
}

# Labour norms for the woods, the mines, the quarries and the rest (section 5), from the game's own producers
# (crafts.json `producers`: a spell's hours and output), where those are plausible.
OTHER_PRODUCERS = {
    'woods: timber, firewood, bark, charcoal': ('logging', 'woodcutting', 'charcoal_burning'),
    'mining and metals': ('mine', 'copper_pit', 'tin_streams'),
    'stone, lime, clay, sand': ('quarry', 'clay_pit'),
    'salt': ('salt_pans',),
    'livestock: wool, hides, tallow, bone': ('sheep_farm',),
    'fibre: hemp, flax, linen': ('farm',),
}

# Wages (doc 68: the defaults, and where the 12-year run's drift left them).
WAGE = {'hand': 8, 'keeper': 14, 'hand_now': 10.5, 'keeper_now': 16}


def load_lines(path):
    with open(path) as f:
        for line in f:
            if line.strip():
                yield json.loads(line)


# ---------------------------------------------------------------------------------------------------------------------
# measure
def measure_one(path):
    ev = json.load(open(path))['events']
    days = sorted({int(e['day']) for e in ev})
    use = set(days[3:-1])
    n = len(use)
    pend = collections.defaultdict(lambda: collections.Counter())    # till -> 'country', 'toll', 'carriage' since last draw
    draws = collections.defaultdict(lambda: collections.Counter())   # item -> pieces, country, toll, carriage
    why = collections.defaultdict(collections.Counter)                 # item -> reason -> pieces
    bought = collections.defaultdict(collections.Counter)              # Country's purchases: item -> pieces, coins
    market = collections.defaultdict(collections.Counter)              # residents' purchases
    eaten = collections.Counter()
    made = collections.Counter()                                       # the city's makers' batches' output, by good
    carriage_out = 0
    for e in ev:
        if e['kind'] != 'economy':
            continue
        d, a, t = e['detail'], e['actor'], e['target']
        inside = int(e['day']) in use
        drawish = d.startswith('restocked from the store room') or d.startswith('drawn for ')
        if a.startswith('till:') and t == 'outside:country' and drawish:
            pend[a]['country'] += e['coins']
        elif a.startswith('till:') and t == 'treasury' and d.startswith('gate toll: '):
            pend[a]['toll'] += e['coins']
        elif a.startswith('till:') and d == 'carriage in':
            pend[a]['carriage'] += e['coins']
        elif a == 'made' and drawish:
            p = pend.pop(t, collections.Counter())
            if inside:
                c = draws[e['item']]
                c['pieces'] += e['quantity']
                c['country'] += p['country']
                c['toll'] += p['toll']
                c['carriage'] += p['carriage']
                why[e['item']]['resale' if d.startswith('restocked') else d[len('drawn for '):]] += e['quantity']
        elif not inside:
            continue
        elif a == 'made' and d == 'made from the store room':
            made[e['item']] += e['quantity']
        elif d == 'to the Country':
            bought[e['item']]['pieces'] += e['quantity']
            bought[e['item']]['coins'] += e['coins']
        elif d == 'carriage out':
            carriage_out += e['coins']
        elif d == 'market' and t.startswith('till:'):
            market[e['item']]['pieces'] += e['quantity']
            market[e['item']]['coins'] += e['coins']
        elif d == 'eat':
            eaten[e['item']] += e['quantity']
    per = lambda c: {k: {kk: vv / n for kk, vv in v.items()} for k, v in c.items()}
    return {'days': n, 'draws': per(draws), 'why': per(why), 'bought': per(bought), 'market': per(market),
            'eaten': {k: v / n for k, v in eaten.items()}, 'made': {k: v / n for k, v in made.items()},
            'carriage_out': carriage_out / n}


def cmd_measure(a):
    out = {}
    for spec in a.runs:
        season, path = spec.split('=', 1)
        out[season] = measure_one(path)
        econ = Path(path).with_name(Path(path).name.replace('_events.json', '.jsonl'))
        if econ.exists():
            lines = list(load_lines(econ))
            used = lines[3:len(lines) - 1] if len(lines) > 4 else lines
            keys = ['residents', 'drawn', 'toll', 'carriage', 'country_spent', 'food_bought'] + [f'{g}_drawn' for g in GROUPS]
            out[season]['economy'] = {k: sum(d.get(k, 0) for d in used) / len(used) for k in keys}
    json.dump(out, open(a.out, 'w'), indent=1)
    print(f'wrote {a.out}: ' + ', '.join(f'{s} {v["days"]} days' for s, v in out.items()))


# ---------------------------------------------------------------------------------------------------------------------
# report
def catalogue():
    items = {i['id']: i for i in json.load(open(ROOT / 'Data/Items/items.json'))['items']}
    country = json.load(open(ROOT / 'Data/Items/country.json'))
    group_of = {}
    for g in country['seasons']['groups']:
        for good in g['goods']:
            group_of.setdefault(good, g['id'])
    crafts = {k['id']: k for k in json.load(open(ROOT / 'Data/Items/crafts.json'))['crafts']}
    producers = {p['id']: p for p in json.load(open(ROOT / 'Data/Items/crafts.json'))['producers']}
    return items, group_of, crafts, producers, country


def industry_of(good):
    for ind, goods in INDUSTRY.items():
        if good in goods:
            return ind
    return 'other'


def settled(econ_path, from_year, to_year=12):
    acc = collections.defaultdict(lambda: collections.defaultdict(float))
    cnt = collections.Counter()
    years = set()
    for d in load_lines(econ_path):
        y = d['day'] // 365 + 1
        if y < from_year or y > to_year:
            continue
        years.add(y)
        s = d['season']
        cnt[s] += 1
        for k in ['residents', 'drawn', 'toll', 'carriage', 'country_spent', 'country_unspent', 'food_bought',
                  'wages_paid', 'country', 'toll_now', 'wage_keeper', 'wage_hand'] + [f'{g}_{x}' for g in GROUPS for x in ('drawn', 'cap', 'price', 'sold')]:
            acc[s][k] += d.get(k, 0)
    return {s: {k: v / cnt[s] for k, v in acc[s].items()} for s in SEASONS}, sorted(years)


def scale(measured, long, group_of):
    """Each season's draws, eaten and bought goods at the settled years' level."""
    out = {}
    for s in SEASONS:
        m, L = measured[s], long[s]
        short = m['economy']
        gf = {g: (L[f'{g}_drawn'] / short[f'{g}_drawn']) if short[f'{g}_drawn'] > 0 else 1 for g in GROUPS}
        cost = lambda c: c.get('country', 0) + c.get('toll', 0)
        in_groups = sum(cost(c) * gf[group_of[g]] for g, c in m['draws'].items() if group_of.get(g) in gf)
        out_groups = sum(cost(c) for g, c in m['draws'].items() if group_of.get(g) not in gf)
        # Goods outside the groups: what is left of the long run's drawn pennies (so the totals are the long run's).
        rest = max(0.0, L['drawn'] - in_groups) / out_groups if out_groups else 1.0
        pop = L['food_bought'] / short['food_bought']
        f = lambda good: gf[group_of[good]] if group_of.get(good) in gf else rest
        draws = {g: {k: v * f(g) for k, v in c.items()} for g, c in m['draws'].items()}
        # The toll at the settled years' rate (the short runs start at the world's 30%; the drift firmed it to 31-33%).
        tr = L['toll'] / sum(c.get('toll', 0) for c in draws.values())
        for c in draws.values():
            paid = c.get('country', 0) + c.get('toll', 0)
            c['toll'] = c.get('toll', 0) * tr
            c['country'] = paid - c['toll']
        out[s] = {
            'draws': draws,
            'why': {g: {k: v * f(g) for k, v in c.items()} for g, c in m['why'].items()},
            'bought': {g: {k: v * L['country_spent'] / short['country_spent'] for k, v in c.items()} for g, c in m['bought'].items()},
            'eaten': {g: v * pop for g, v in m['eaten'].items()},
            'market': {g: {k: v * pop for k, v in c.items()} for g, c in m['market'].items()},
            'made': {g: v * pop for g, v in m['made'].items()},
            'carriage_out': m['carriage_out'] * L['country_spent'] / short['country_spent'], 'factors': gf, 'rest': rest, 'pop': pop,
        }
    return out


def year_mean(per_season):
    return sum(per_season[s] * SEASON_DAYS[s] for s in SEASONS) / 365


def fmt(x, nd=0):
    if isinstance(x, str):
        return x
    if abs(x) >= 1000 and nd == 0:
        return f'{x:,.0f}'
    return f'{x:,.{nd}f}'


def table(head, rows):
    out = ['| ' + ' | '.join(head) + ' |', '|' + '|'.join('---' for _ in head) + '|']
    out += ['| ' + ' | '.join(fmt(c) if not isinstance(c, str) else c for c in r) + ' |' for r in rows]
    return '\n'.join(out)


def cmd_report(a):
    items, group_of, crafts, producers, country = catalogue()
    measured = json.load(open(a.measured))
    long, years = settled(a.economy, a.from_year)
    sc = scale(measured, long, group_of)
    price = lambda g: items.get(g, {}).get('price', 0)
    nourish = lambda g: (items.get(g, {}).get('food') or {}).get('nourish', 0)
    P = []
    say = P.append
    pop = year_mean({s: long[s]['residents'] for s in SEASONS})
    say(f'Settled years {years[0]}-{years[-1]} of the 12-year run; residents {pop:.0f} on average.\n')

    # ---- 1. Per industry, per season: pieces a day, pennies a day (the till's whole cost), the toll, carriage.
    ind_s = collections.defaultdict(lambda: {s: collections.Counter() for s in SEASONS})
    goods_s = collections.defaultdict(lambda: {s: collections.Counter() for s in SEASONS})
    for s in SEASONS:
        for g, c in sc[s]['draws'].items():
            ind = industry_of(g)
            for k in ('pieces', 'country', 'toll', 'carriage'):
                ind_s[ind][s][k] += c.get(k, 0)
                goods_s[g][s][k] += c.get(k, 0)
    say('### 1a. What the city draws from the Country, by industry (a day; settled years)\n')
    rows = []
    tot = {s: collections.Counter() for s in SEASONS}
    order = sorted(ind_s, key=lambda i: -year_mean({s: ind_s[i][s]['country'] + ind_s[i][s]['toll'] for s in SEASONS}))
    for ind in order:
        r = [ind]
        for s in SEASONS:
            c = ind_s[ind][s]
            r.append(f"{c['pieces']:.0f} / {c['country'] + c['toll']:.0f}p")
            tot[s].update(c)
        ym = {k: year_mean({s: ind_s[ind][s][k] for s in SEASONS}) for k in ('pieces', 'country', 'toll', 'carriage')}
        gross = ym['country'] + ym['toll']
        sw = [ind_s[ind][s]['country'] + ind_s[ind][s]['toll'] for s in SEASONS]
        r += [f"{ym['pieces']:.0f} / {gross:.0f}p", f"{ym['toll']:.0f}p", f"{ym['carriage']:.1f}p", f"{ym['country']:.0f}p",
              f"{min(sw) / max(1e-9, gross) * 100:.0f}-{max(sw) / max(1e-9, gross) * 100:.0f}%"]
        rows.append(r)
    ymt = {k: year_mean({s: tot[s][k] for s in SEASONS}) for k in ('pieces', 'country', 'toll', 'carriage')}
    rows.append(['**all**'] + [f"**{tot[s]['pieces']:.0f} / {tot[s]['country'] + tot[s]['toll']:.0f}p**" for s in SEASONS] +
                [f"**{ymt['pieces']:.0f} / {ymt['country'] + ymt['toll']:.0f}p**", f"**{ymt['toll']:.0f}p**",
                 f"**{ymt['carriage']:.0f}p**", f"**{ymt['country']:.0f}p**", ''])
    say(table(['Industry', 'Spring pieces / paid', 'Summer', 'Autumn', 'Winter', 'Year', 'Gate toll', 'Carriage in (the shop pays)',
               "The Country's intake", 'Season swing (of the year mean)'], rows))
    say('')
    # Check against the long run's own totals.
    say('Check, the long run\'s own a day (year mean): drawn ' +
        f"{year_mean({s: long[s]['drawn'] for s in SEASONS}):.0f}p, toll {year_mean({s: long[s]['toll'] for s in SEASONS}):.0f}p, "
        f"carriage {year_mean({s: long[s]['carriage'] for s in SEASONS}):.0f}p, the Country spent "
        f"{year_mean({s: long[s]['country_spent'] for s in SEASONS}):.0f}p.\n")

    # ---- 1b. Goods, food industries in detail.
    say('### 1b. The goods, by industry (a day; pieces by season, the year\'s pennies; why drawn)\n')
    rows = []
    for ind in order:
        for g in sorted((g for g in goods_s if industry_of(g) == ind), key=lambda g: -year_mean({s: goods_s[g][s]['pieces'] for s in SEASONS})):
            pcs = [goods_s[g][s]['pieces'] for s in SEASONS]
            if max(pcs) < 0.5:
                continue
            gross = year_mean({s: goods_s[g][s]['country'] + goods_s[g][s]['toll'] for s in SEASONS})
            why = collections.Counter()
            for s in SEASONS:
                for k, v in sc[s]['why'].get(g, {}).items():
                    why[k] += v * SEASON_DAYS[s] / 365
            tw = sum(why.values()) or 1
            ws = ', '.join(f'{k} {v / tw * 100:.0f}%' for k, v in why.most_common(3))
            rows.append([ind, g, f'{price(g)}p'] + [f'{p:.0f}' for p in pcs] + [f'{gross:.0f}p', ws])
    say(table(['Industry', 'Good', 'Catalogue', 'Spring', 'Summer', 'Autumn', 'Winter', 'Paid a day (year)', 'Drawn for'], rows))
    say('')

    # ---- 1c. What the Country buys back.
    say('### 1c. What the Country buys back from the city (a day, settled years)\n')
    buy = collections.Counter()
    buy_p = collections.Counter()
    for s in SEASONS:
        for g, c in sc[s]['bought'].items():
            buy[g] += c['coins'] * SEASON_DAYS[s] / 365
            buy_p[g] += c['pieces'] * SEASON_DAYS[s] / 365
    co = year_mean({s: sc[s]['carriage_out'] for s in SEASONS})
    tb = sum(buy.values())
    food_back = {g: v for g, v in buy.items() if nourish(g) > 0}
    say(f'In all {tb:.0f}p a day of goods (the long run\'s `country_spent` scaled: '
        f"{year_mean({s: long[s]['country_spent'] for s in SEASONS}):.0f}p), and {co:.0f}p of carriage out. Of it, food and drink "
        f"made from the Country's own produce: {sum(food_back.values()):.0f}p ("
        + ', '.join(f'{g} {v:.0f}p ({buy_p[g]:.0f})' for g, v in sorted(food_back.items(), key=lambda x: -x[1])) + ').\n')
    say('The top of the basket: ' + ', '.join(f'{g} {v:.0f}p' for g, v in buy.most_common(20)) + '.\n')

    # ---- 2. Food: what residents buy, and the chain to primary goods.
    farms = money_first(a, sc, long, items, crafts, goods_s, ind_s, say)
    if a.real_check:
        food_and_units(a, sc, long, items, crafts, producers, goods_s, ind_s, buy, pop, say)
    else:
        others = other_industries(sc, crafts, producers)
        others_table(others, ind_s, say)
        if a.unplaced:
            unplaced_pool(a.unplaced, say, farms, others)
        if a.units:
            write_farms(a.units, farms, others)
    print('\n'.join(P))


# The food chains: a drawn good's primary goods (crafts.json: flour is wheat at the mill, 1 to 1; the Country's own
# fruit store and press make the dried apples, preserves and cider it sells; its winery the wine; its cookshops' meal is
# stew, a piece of meat and one of vegetables).
PRIMARY = {'flour': {'wheat': 1}, 'dried_apples': {'apples': 1.5}, 'fruit_preserves': {'apples': 1, 'honey': 0.5},
           'cider': {'apples': 2}, 'wine': {'grapes': 2}, 'meal': {'raw_meat': 1, 'vegetables': 1}}
# Which unit grows each primary good (shares where two do).
GROWN_BY = {'wheat': {'grain farm': 1}, 'oats': {'grain farm': 1}, 'apples': {'orchard': 1}, 'berries': {'orchard': 1},
            'grapes': {'vineyard': 1}, 'vegetables': {'market garden': 1}, 'milk': {'dairy farm': .75, 'goat fold': .25},
            'raw_meat': {'cattle ranch': .5, 'piggery': .5}, 'pig': {'piggery': 1}, 'mutton': {'sheep run': 1},
            'rabbit': {'rabbit warren': 1}, 'honey': {'apiary': 1}, 'fish': {'fishing': 1}, 'shellfish': {'fishing': 1},
            'salt': {'salt works': 1}}
# Each drawn good's income goes to the unit that sells it (flour: half its 2p is the wheat, half the milling).
SOLD_BY = {'flour': {'grain farm': .5, 'water mill': .5}, 'oats': {'grain farm': 1}, 'straw': {'grain farm': 1},
           'apples': {'orchard': 1}, 'berries': {'orchard': 1}, 'dried_apples': {'orchard': 1},
           'fruit_preserves': {'orchard': 1}, 'cider': {'orchard': 1}, 'grapes': {'vineyard': 1}, 'wine': {'vineyard': 1},
           'vegetables': {'market garden': 1}, 'milk': {'dairy farm': .75, 'goat fold': .25},
           'raw_meat': {'cattle ranch': .5, 'piggery': .5}, 'meal': {'cattle ranch': .5, 'piggery': .5},
           'pig': {'piggery': 1}, 'mutton': {'sheep run': 1}, 'rabbit': {'rabbit warren': 1}, 'honey': {'apiary': 1},
           'beeswax': {'apiary': 1}, 'fish': {'fishing': 1}, 'shellfish': {'fishing': 1}, 'salt': {'salt works': 1},
           # The by-products: the herds' wool, hides, tallow and bone (the user's "the farms' own sales of wool and hides").
           'wool': {'sheep run': 1}, 'sheepskin': {'sheep run': 1}, 'small_pelt': {'rabbit warren': 1},
           'hide': {'cattle ranch': .6, 'piggery': .4}, 'tallow': {'cattle ranch': .5, 'piggery': .3, 'sheep run': .2},
           'bones': {'cattle ranch': .5, 'piggery': .3, 'sheep run': .2}}
# The first year's first income (day of the year, 1-365, from Spring 1) for a unit whose goods come at harvest.
FIRST_INCOME = {'grain farm': 165, 'water mill': 165, 'orchard': 170, 'vineyard': 230, 'market garden': 120,
                'apiary': 130, 'salt works': 140}
MONEY_TOTALS = {}
HOUSEHOLD = 1.5     # people a year-round worker's household feeds: DEV's unplaced, 524 people for about 350 with a trade.
UPKEEP = 1.0        # pennies a worker-day: tools and upkeep (crafts.json farmUpkeep, nails, planks, whitewash; sickles).


def rv(x, unit=''):
    return 'short' if x == float('inf') or x > 1e18 else f'{x:.0f}{unit}'


def season_workdays(s):
    return SEASON_DAYS[s] * WORKDAYS


def other_industries(sc, crafts, producers):
    """Section 5: the other industries' workers, at the game's own rates (crafts.json: the producers' spells, the
    city's crafts' hours for what the Country makes), FTE (a worker's 8 hours, six days in seven)."""
    draw = collections.Counter()
    for s in SEASONS:
        for g, c in sc[s]['draws'].items():
            draw[g] += c.get('pieces', 0) * SEASON_DAYS[s] / 365
    hours = lambda pid: producers[pid]['hours']
    out = {}
    fte = lambda h: h / 8 / WORKDAYS
    # The woods: logging (2 timber, 4 firewood, 1 bark a spell), woodcutting for the rest of the firewood, charcoal
    # burning; the Country's iron wants charcoal too (an iron bar: 2 ore and a charcoal).
    log_spells = max(draw['timber'] / 2, draw['oak_bark'] / 1)
    wood_spells = max(0, draw['firewood'] - 4 * log_spells) / 3
    char = draw['charcoal'] + draw['iron_bar']
    planks_h = draw['planks'] / 4 * crafts['planks']['hours']
    out['woods: timber, firewood, bark, charcoal'] = {
        'fte': fte(log_spells * hours('logging') + wood_spells * hours('woodcutting') + char * hours('charcoal_burning') + planks_h),
        'units': [('logging camp', 4, 'the Rainwood and the cedar slopes of the NW coast; the central hills\' oak woods'),
                  ('coppice and woodcutters\' hut', 4, 'beside every village, for firewood (the city burns 350-510 a day)'),
                  ('charcoal hearth', 3, 'the central hills\' woods, near the bloomeries'),
                  ('saw pit or water sawmill', 1, 'on a river below the logging camps')],
        'goods': ['timber', 'firewood', 'oak_bark', 'charcoal', 'planks']}
    ore = draw['iron_ore'] + 2 * draw['iron_bar']
    out['mining and metals'] = {
        'fte': fte(ore * hours('mine') + draw['iron_bar'] * crafts['iron_bar']['hours'] +
                   max(draw['copper_ore'], draw['tin_ore']) * hours('copper_pit') + 2 * draw['bronze_bar'] * crafts['bronze_bar']['hours']),
        'units': [('iron mine', 2, 'the N/NE mountains (the Whitefangs, the Greyteeth)'),
                  ('bloomery (iron smelter)', 2, 'beside the mines, with charcoal from the hills'),
                  ('copper and tin pit', 1, 'the N/NE mountains\' foothills')],
        'goods': ['iron_ore', 'iron_bar', 'copper_ore', 'tin_ore', 'copper_bar', 'bronze_bar']}
    q_spells = max(draw['stone'], draw['limestone'])
    clay_left = max(0, draw['clay'] - 2 * q_spells)
    out['stone, lime, clay, sand'] = {
        'fte': fte(q_spells * hours('quarry') + clay_left / 4 * hours('clay_pit') + draw['lime'] / 3 * crafts['lime']['hours']),
        'units': [('quarry (stone and limestone)', 2, 'the central hills (limestone) and the Rainward Hills near the city'),
                  ('clay pit and sand bank', 2, 'river clay on the plains; sand on the SW coast')],
        'goods': ['stone', 'limestone', 'clay', 'sand', 'lime']}
    fibre = draw['hemp'] + draw['flax'] + draw['cord'] / 3 + 2 * draw['linen_cloth']
    out['fibre: hemp, flax, linen'] = {
        # Grown as the grain farms' fourth field (about 2 kg a worker-day all in: pulling, retting, scutching), spun and
        # woven at home in winter (the city's own hours for cord and linen).
        'fte': fte(fibre * 0.5 / 2 * 8 + draw['cord'] / 3 * crafts['cord']['hours'] + draw['linen_cloth'] * crafts['linen_cloth']['hours']),
        'units': [('hemp and flax fields', 3, 'on the grain farms (a fourth field), and wet ground by the rivers for retting')],
        'goods': ['hemp', 'flax', 'cord', 'linen_cloth']}
    out['herbs'] = {'fte': fte(sum(draw[g] for g in INDUSTRY['herbs']) / 3 * hours('herb_gathering')),
                    'units': [('herb gatherers\' cottage', 1, 'the Warmwood and the SW marsh\'s edge')],
                    'goods': INDUSTRY['herbs']}
    ws = INDUSTRY["country workshops' goods"]
    wh = 0
    for g in ws:
        k = next((k for k in crafts.values() if g in k['out']), None)
        if k:
            wh += draw[g] / k['out'][g] * k['hours']
    out["country workshops' goods"] = {
        'fte': fte(wh),
        'units': [('village pottery', 1, 'the clay pits of the plains'), ('weavers\' and soap boilers\' cottages', 3,
                   'the villages (winter work for the farm families)'), ('country press and paper mill', 1, 'on a river near the city')],
        'goods': ws}
    for k, v in out.items():
        v['draw'] = {g: draw[g] for g in v['goods'] if draw[g] >= 0.5}
        v['plausible'] = v['fte']
    # Plausible norms where the game's are far off: the woods (a coppice worker cuts, splits and stacks about a tonne of
    # firewood a day, a feller 1.5 t of logs; a collier and his mate keep a clamp burning about 150 kg of charcoal a day;
    # two sawyers at a pit saw about 20 planks a day; firewood 10 kg a piece, timber 30 kg, charcoal 3 kg).
    w = out['woods: timber, firewood, bark, charcoal']
    w['plausible'] = (draw['firewood'] * 10 / 1000 + draw['timber'] * 30 / 1500 + char * 3 / 150 * 2 + draw['planks'] / 10) / WORKDAYS
    # Fibre: grown on the grain farms and spun and woven at home in the winter lull, so only half needs its own hands.
    out['fibre: hemp, flax, linen']['plausible'] = out['fibre: hemp, flax, linen']['fte'] / 2
    return out


def food_and_units(a, sc, long, items, crafts, producers, goods_s, ind_s, buy, pop, say):
    price = lambda g: items.get(g, {}).get('price', 0)
    nourish = lambda g: (items.get(g, {}).get('food') or {}).get('nourish', 0)
    # 2a. The residents' food as bought (in a run's steady state what is bought is eaten).
    say('### 2a. What the residents buy to eat (pieces a day, settled years)\n')
    bought = collections.Counter()
    n_s = {s: 0.0 for s in SEASONS}
    for s in SEASONS:
        for g, c in sc[s]['market'].items():
            if nourish(g) > 0:
                bought[g] += c['pieces'] * SEASON_DAYS[s] / 365
                n_s[s] += c['pieces'] * nourish(g)
    tn = sum(bought[g] * nourish(g) for g in bought)
    rows = []
    for g, v in bought.most_common():
        if v < 1:
            continue
        rows.append([g, f'{nourish(g)}', f'{price(g)}p'] + [f"{sc[s]['market'].get(g, {}).get('pieces', 0):.0f}" for s in SEASONS] +
                    [f'{v:.0f}', f'{v * nourish(g) / tn * 100:.0f}%'])
    say(table(['Food', 'Nourish', 'Price', 'Spring', 'Summer', 'Autumn', 'Winter', 'Year', 'Of the nourishment'], rows))
    say('\nNourishment bought a head a day: ' + ', '.join(f"{s} {n_s[s] / long[s]['residents']:.1f}" for s in SEASONS) +
        ' (a resident needs about 46; the rest is what households carried at the short runs\' start).\n')

    # 2b. Each drawn food good by use, then traced to its primary goods.
    def res_share(product, s):
        r = sc[s]['market'].get(product, {}).get('pieces', 0)
        c = sc[s]['bought'].get(product, {}).get('pieces', 0)
        return r / (r + c) if r + c > 0 else 1.0
    craft_out = {k: next(iter(c['out'])) for k, c in crafts.items()}
    use_d = collections.defaultdict(lambda: {s: collections.Counter() for s in SEASONS})     # drawn good
    need = collections.defaultdict(lambda: {s: collections.Counter() for s in SEASONS})      # primary good
    for s in SEASONS:
        for g, reasons in sc[s]['why'].items():
            if industry_of(g) not in FOOD_INDUSTRIES:
                continue
            for r, v in reasons.items():
                prod = g if r == 'resale' else craft_out.get(r, r)
                share = res_share(prod, s)
                u = 'residents' if nourish(prod) > 0 else 'goods'
                use_d[g][s][u] += v * share
                use_d[g][s]['back'] += v * (1 - share)
                for pg, k in PRIMARY.get(g, {g: 1}).items():
                    need[pg][s][u] += v * share * k
                    need[pg][s]['back'] += v * (1 - share) * k
    say('### 2b. The Country\'s food goods by use, and the primary goods behind them (pieces a day)\n')
    rows = []
    for g in sorted(use_d, key=lambda g: -year_mean({s: use_d[g][s].total() for s in SEASONS})):
        y = {u: year_mean({s: use_d[g][s][u] for s in SEASONS}) for u in ('residents', 'back', 'goods')}
        if sum(y.values()) < 0.5:
            continue
        chain = ' + '.join(f'{k:g} {pg}' for pg, k in PRIMARY.get(g, {g: 1}).items())
        rows.append([g, f'{y["residents"]:.0f}', f'{y["back"]:.0f}', f'{y["goods"]:.0f}',
                     '/'.join(f'{use_d[g][s].total():.0f}' for s in SEASONS), chain])
    say(table(['Drawn good', 'Residents\' food', 'Made into what the Country buys back', 'Other goods (salves, wax)',
               'All, by season (Sp/Su/Au/Wi)', 'A piece is'], rows))
    say('')

    # The Country's own people eat too: the residents' diet a head, in primary goods.
    per_head = {pg: {s: need[pg][s]['residents'] / long[s]['residents'] for s in SEASONS} for pg in need}
    others = other_industries(sc, crafts, producers)
    other_fte = sum(v['fte'] for v in others.values())

    # 2c. The units: iterate (the producers' own food depends on how many they are).
    country_pop = 0.0
    for _ in range(30):
        demand = {pg: {s: need[pg][s].total() + per_head[pg][s] * country_pop for s in SEASONS} for pg in need}
        U = size_units(demand, sc, goods_s)
        workers = sum(u['fte_year'] for u in U.values()) + other_fte
        new_pop = workers * HOUSEHOLD
        if abs(new_pop - country_pop) < 0.5:
            break
        country_pop = new_pop
    say('### 2c. Primary production needed (a day and a year): the city, and the Country\'s own people\n')
    rows = []
    for pg in sorted(need, key=lambda pg: -year_mean({s: need[pg][s].total() for s in SEASONS}) * A['kg'].get(pg, 0)):
        city = year_mean({s: need[pg][s].total() for s in SEASONS})
        own = year_mean({s: per_head[pg][s] * country_pop for s in SEASONS})
        kg = A['kg'].get(pg, 0)
        if city < 0.5:
            continue
        rows.append([pg, f'{kg:g} kg', f'{city:.0f}', f'{year_mean({s: need[pg][s]["residents"] for s in SEASONS}):.0f}',
                     f'{own:.0f}', '/'.join(f'{demand[pg][s]:.0f}' for s in SEASONS),
                     f'{(city + own) * 365 * kg / 1000:.1f} t', ', '.join(GROWN_BY.get(pg, {}).keys())])
    say(table(['Primary good', 'A piece', 'The city draws', 'of it for residents\' food', 'The Country\'s own people eat',
               'All, by season (Sp/Su/Au/Wi)', 'A year (real weight)', 'Grown by'], rows))
    say(f'\nThe Country\'s people: {country_pop:.0f} (its workers, {country_pop / HOUSEHOLD:.0f} year-round '
        f'equivalents in all its industries, {HOUSEHOLD:g} to a household), eating the residents\' diet.\n')

    # 2d. The units.
    say('### 2d. The production units\n')
    rows = []
    for name, u in U.items():
        rows.append([name, f"{u['count']}", u['size'], u['area'], f"{u['tiles']:,.0f}",
                     f"{u['range_tiles']:,.0f}" if u['range_tiles'] else '-', f"{u['tiles'] / u['count']:,.0f}", u['out']])
    say(table(['Unit', 'How many', 'Each', 'Land (real, all units)', f"Own map tiles, all (land at 1:{A['land_scale']}, steadings 1:1)",
               'Shared range, tiles (1:10)', 'Own tiles each', 'Output'], rows))
    say(f"\nAll units' own tiles: {sum(u['tiles'] for u in U.values()):,.0f}; shared range: "
        f"{sum(u['range_tiles'] for u in U.values()):,.0f} (a cell is 65,536).\n")
    # Each unit's output a worker-day against the game's own producers (crafts.json: a spell's output over its hours,
    # an 8-hour day).
    say('Output a worker-day, these norms against the game\'s own producers (crafts.json, 8 hours):\n')
    game = {'grain farm': ('farm', ['wheat', 'oats']), 'orchard': ('orchard', ['apples']), 'vineyard': ('vineyard', ['grapes']),
            'dairy farm': ('dairy_herd', ['milk']), 'goat fold': ('sheep_pasture', ['milk']), 'piggery': ('swineherd', ['pig']),
            'sheep run': ('sheep_farm', ['mutton']), 'rabbit warren': ('rabbit_farm', ['rabbit']),
            'apiary': ('apiary', ['honey']), 'fishing': ('fishing', ['fish']), 'salt works': ('salt_pans', ['salt'])}
    rows = []
    for name, (pid, goods) in game.items():
        u = U[name]
        days = sum(u['workers'][s] * season_workdays(s) for s in SEASONS)
        share = {'goat fold': .25, 'dairy farm': .75}.get(name, 1)
        pieces = sum(demand.get(g, {}).get(s, 0) * SEASON_DAYS[s] for g in goods for s in SEASONS) * share
        if name == 'fishing':
            pieces = sum(demand.get(g, {}).get(s, 0) * SEASON_DAYS[s] for g in ('fish', 'shellfish') for s in SEASONS)
        if name == 'piggery':     # its raw meat too, as pigs (a pig is four pieces of meat)
            pieces += sum(demand.get('raw_meat', {}).get(s, 0) * SEASON_DAYS[s] for s in SEASONS) * .5 / 4
        pr = producers[pid]
        g_day = sum(pr['out'].get(g, 0) for g in goods) * 8 / pr['hours']
        rows.append([name, f'{pieces / days:.1f}', f"{g_day:.1f} ({pid})", f'{pieces / days / g_day:.1f}x'])
    say(table(['Unit', 'These norms (pieces)', "The game's producer", 'Ratio'], rows))
    say('')

    # 3a. Labour by season.
    say('### 3a. Labour by season (workers on the job, all units of the kind)\n')
    rows = []
    tot = collections.Counter()
    for name, u in U.items():
        w = u['workers']
        rows.append([name] + [f'{w[s]:.1f}' for s in SEASONS] + [f"{u['staff']}", f"{u['seasonal']}", f"{u['seasonal_days']:.0f}",
                                                                   u['peak']])
        for s in SEASONS:
            tot[s] += w[s]
        tot['staff'] += u['staff']
        tot['seasonal'] += u['seasonal']
        tot['days'] += u['seasonal_days']
    rows.append(['**food, all**'] + [f'**{tot[s]:.0f}**' for s in SEASONS] + [f"**{tot['staff']}**", f"**{tot['seasonal']}**",
                                                                             f"**{tot['days']:.0f}**", ''])
    say(table(['Unit', 'Spring', 'Summer', 'Autumn', 'Winter', 'Year-round staff (keepers and hands)', 'Seasonal hands at the peak',
               'Seasonal hand-days a year', 'Peak'], rows))
    say('')

    # The Country's own food, priced: its people buy what they eat from its own units at the store's price (the
    # catalogue's times the season's; no gate toll, no carriage), the wheat as flour from the mills.
    season_price = {s: {g: long[s].get(f'{g}_price', 1) or 1 for g in GROUPS} for s in SEASONS}
    group_of = {}
    for gr in json.load(open(ROOT / 'Data/Items/country.json'))['seasons']['groups']:
        for good in gr['goods']:
            group_of.setdefault(good, gr['id'])
    sold_as = {'wheat': 'flour'}
    own = {name: {s: 0.0 for s in SEASONS} for name in U}
    head_p = {s: 0.0 for s in SEASONS}
    for pg, by_s in per_head.items():
        good = sold_as.get(pg, pg)
        for s in SEASONS:
            unit_p = price(good) * season_price[s].get(group_of.get(good), 1)
            head_p[s] += by_s[s] * unit_p
            for un, share in SOLD_BY.get(good, GROWN_BY.get(pg, {})).items():
                if un in own:
                    own[un][s] += by_s[s] * country_pop * unit_p * share
    res_food = year_mean({s: sum(c['coins'] for g, c in sc[s]['market'].items() if nourish(g) > 0) / long[s]['residents']
                          for s in SEASONS})
    say(f"A head of the Country eats {year_mean(head_p):.1f}p of its own produce a day at the store's prices "
        f"({', '.join(f'{s} {head_p[s]:.1f}p' for s in SEASONS)}), against the {res_food:.1f}p a resident pays the city's "
        f"shops for food; a year-round worker's household ({HOUSEHOLD:g}) {HOUSEHOLD * year_mean(head_p):.1f}p.\n")

    # 3b. Money per unit.
    say('### 3b. Money: each unit\'s income from the city against its wages and upkeep (pennies a day)\n')
    rows = []
    money = {}
    for name, u in U.items():
        m = unit_money(name, u, own[name], head_p)
        money[name] = m
        rows.append([name, f"{u['count']}"] + [f"{m['income'][s]:.0f}/{m['cost'][s]:.0f}" for s in SEASONS] +
                    [f"{m['income_y']:.0f}", f"{m['cost_y']:.0f}", f"{m['net_y']:+.0f}", f"{m['cover']:.2f}",
                     f"{m['own_y']:.0f}", f"{m['net_board_y']:+.0f}", f"{m['cover_board']:.2f}", rv(m['reserve']),
                     f"{m['capital']:.0f}"])
    ti = sum(m['income_y'] for m in money.values())
    tc = sum(m['cost_y'] for m in money.values())
    to = sum(m['own_y'] for m in money.values())
    rows.append(['**food, all**', ''] + [f"**{sum(m['income'][s] for m in money.values()):.0f}/{sum(m['cost'][s] for m in money.values()):.0f}**"
                                         for s in SEASONS] + [f'**{ti:.0f}**', f'**{tc:.0f}**', f'**{ti - tc:+.0f}**', f'**{ti / tc:.2f}**',
                                                              f'**{to:.0f}**', f'**{ti + to - tc:+.0f}**', f'**{(ti + to) / tc:.2f}**',
                                                              '**' + rv(sum(m['reserve'] for m in money.values() if m['reserve'] < 1e18)) + '** (the units that pay)',
                                                              f"**{sum(m['capital'] for m in money.values()):.0f}**"])
    say(table(['Unit', 'Units', 'Spring in/out', 'Summer', 'Autumn', 'Winter', 'From the city (year)', 'Wages and upkeep',
               'Net (cash from the city alone)', 'Cover', "The Country's own people's food", 'Net with it', 'Cover with it',
               'Working money in a steady year', 'Working money to the first harvest (first year)'], rows))
    say('')
    say(f"The Country's people's food bought from the units ({to:.0f}p a day) against what its workers' households spend on "
        f"it out of their wages ({sum((u['staff'] + year_mean(u['seasonal_s'])) for u in U.values()) * HOUSEHOLD * year_mean(head_p):.0f}p "
        f"for the food units' people; the rest is the other industries' people's).\n")
    MONEY_TOTALS.update({'from_city': ti, 'cost': tc, 'own': to, 'head_p': year_mean(head_p), 'res_food': res_food})

    # 3b'. The levers, for all the food units and for the ones that fall short.
    toll = year_mean({s: long[s]['toll_now'] for s in SEASONS}) / 100
    cases = [
        ('Drifted wages (16p / 10.5p), cash from the city alone', {}, False),
        ('Drifted wages, and the Country\'s own people buy their food from the units', {}, True),
        ('The default wages (14p / 8p), with the own people\'s food', {'keeper': 14, 'hand': 8}, True),
        ('Drifted wages; harvest and lambing hands are the households\' own (fed, not waged)', {'family_harvest': True}, True),
        (f'Drifted wages; no gate toll on the Country\'s food (it keeps the {toll * 100:.0f}%)', {'toll_free': toll}, True),
        ('Drifted wages; grain dearer (flour 3p, oats 1.5p: the grain farms\' and mills\' prices half as much again)',
         {'price_up': {'grain farm': 1.5, 'water mill': 1.5}}, True),
    ]
    short = [n for n, m in money.items() if m['cover_board'] < 1]
    rows = []
    for label, kw, with_own in cases:
        ms = {n: unit_money(n, u, own[n], head_p, **kw) for n, u in U.items()}
        i = sum(m['income_y'] + (m['own_y'] if with_own else 0) for m in ms.values())
        c = sum(m['cost_y'] for m in ms.values())
        each = ', '.join(f"{n} {(ms[n]['income_y'] + (ms[n]['own_y'] if with_own else 0)) / ms[n]['cost_y']:.2f}" for n in short)
        rows.append([label, f'{i:.0f}p', f'{c:.0f}p', f'{i - c:+.0f}p', f'{i / c:.2f}', each])
    say(table(['Case (all food units, a day)', 'Income', 'Wages and upkeep', 'Net', 'Cover', 'Cover of the units short in case 2'], rows))
    say('')
    # Mixed farms: the grain farms keep the pigs (and the city's cows and beds with the dairies).
    mixes = [('mixed farm: a grain farm with its pigs', ['grain farm', 'piggery']),
             ('dairy with its market garden', ['dairy farm', 'market garden']),
             ('upland fold: sheep with goats', ['sheep run', 'goat fold'])]
    rows = []
    for label, parts in mixes:
        i = sum(money[p]['income_y'] + money[p]['own_y'] for p in parts)
        c = sum(money[p]['cost_y'] for p in parts)
        rows.append([label, f'{i:.0f}p', f'{c:.0f}p', f'{i - c:+.0f}p', f'{i / c:.2f}'])
    say(table(['Combined (with the own people\'s food)', 'Income', 'Wages and upkeep', 'Net', 'Cover'], rows))
    say('')

    # 3c. Per unit, one of each.
    say('### 3c. One unit of each kind (a day, the year\'s mean)\n')
    rows = []
    for name, u in U.items():
        m = money[name]
        n = u['count']
        rows.append([name, f"{u['keepers'] / n:.1f} + {(u['staff'] - u['keepers']) / n:.1f}", f"{u['seasonal'] / n:.1f}",
                     f"{m['income_y'] / n:.0f}p", f"{m['own_y'] / n:.0f}p", f"{m['wages_y'] / n:.0f}p", f"{m['upkeep_y'] / n:.1f}p",
                     f"{m['net_y'] / n:+.0f}p", f"{m['net_board_y'] / n:+.0f}p", rv(m['reserve'] / n, 'p'), f"{m['capital'] / n:.0f}p"])
    say(table(['Unit', 'Keepers + year-round hands', 'Seasonal hands at the peak', 'From the city', "From the Country's own people",
               'Wages', 'Upkeep', 'Net, the city alone', 'Net with its own people', 'Working money, steady year',
               'Working money, first year'], rows))
    say('')
    if a.unplaced:
        unplaced_pool(a.unplaced, say)

    # 5. Other industries.
    say('### 5. The other industries (the game\'s own rates)\n')
    rows = []
    for k, v in others.items():
        inc = year_mean({s: ind_s[k][s]['country'] for s in SEASONS}) if k in ind_s else 0
        units_n = sum(n for _, n, _ in v['units'])
        wage = v['plausible'] * WAGE['hand_now'] + units_n * (WAGE['keeper_now'] - WAGE['hand_now'])
        v['income'] = inc
        v['wages'] = wage
        rows.append([k, ', '.join(f'{g} {d:.0f}' for g, d in sorted(v['draw'].items(), key=lambda x: -x[1])),
                     f"{v['fte']:.0f}", f"{v['plausible']:.0f}", f"{inc:.0f}p", f"{wage:.0f}p", f"{inc / wage:.2f}",
                     '; '.join(f'{n} {u}' for u, n, _ in v['units'])])
    say(table(['Industry', 'Drawn a day', "Workers at the game's rates (FTE)", 'Workers, plausible (FTE)', "The Country's intake",
               'Wages (a keeper a unit, 10.5p hands)', 'Cover', 'Units'], rows))
    say('')

    # The whole Country's money, a day: what comes in from the city, what goes round inside it, what goes back.
    intake = year_mean({s: sum(ind_s[i][s]['country'] for i in ind_s) for s in SEASONS})
    other_in = sum(v['income'] for v in others.values())
    other_w = sum(v['wages'] for v in others.values())
    posts = 91 * WAGE['hand']
    spent = year_mean({s: long[s]['country_spent'] for s in SEASONS})
    food_spend = country_pop * year_mean(head_p)
    wages_all = sum(m['wages_y'] for m in money.values()) + other_w
    upkeep_all = sum(m['upkeep_y'] for m in money.values())
    keep = intake + food_spend - wages_all - upkeep_all - posts
    to_city = wages_all - food_spend + posts + upkeep_all
    say(f"Whole Country, a day: from the city {intake:.0f}p (the food units {ti:.0f}p, the rest {intake - ti:.0f}p); "
        f"its people's food bought from its own units {food_spend:.0f}p (inside the Country: no toll); wages {wages_all:.0f}p "
        f"(food units {sum(m['wages_y'] for m in money.values()):.0f}p, the other industries {other_w:.0f}p), upkeep "
        f"{upkeep_all:.0f}p, its 91 posts in the city {posts:.0f}p; the units keep {keep:+.0f}p. Back to the city: its "
        f"people's wages less their food, its posts and its upkeep, {to_city:.0f}p, and what the units keep, against the "
        f"{spent:.0f}p the Country's basket spends now.\n")
    MONEY_TOTALS.update({'intake': intake, 'food_spend': food_spend, 'wages_all': wages_all, 'keep': keep, 'to_city': to_city,
                         'spent_now': spent, 'other_w': other_w, 'other_in': other_in})
    json.dump({'units': U, 'money': money, 'others': others, 'country_pop': country_pop,
               'demand': demand, 'need': {pg: {s: dict(need[pg][s]) for s in SEASONS} for pg in need}},
              open(a.dump, 'w'), indent=1, default=float) if a.dump else None
    if a.units:
        write_units(a.units, U, others)


# ---------------------------------------------------------------------------------------------------------------------
# The money-first method (the user, 2026-11-01: "figure out how many wolves each area of food can pay for and go from
# there, and decide how many 'farms' could look reasonable, and how much their yield needs to be"). Each food area (a
# season group of country.json, and salt) has its farms: their kinds, how many of each, what each sells (`sells`: a
# share of each good's pennies; flour's 2p is half the wheat and half the milling) and what it grows (`grows`: a share
# of each good's pieces), where each lies, and how big it is drawn on the map.
SLACK = 0.10        # yields over demand, so the shelves don't run dry on a good day
D_NEAR, D_MID, D_FAR = 'near (within 2 cells of the city)', 'middle (3-8 cells)', 'far (9 cells or more)'
AREAS = [   # The food areas, for their revenue: country.json's season groups, the goods resold with them, and salt.
    {'id': 'grain', 'name': 'grain and flour', 'goods': ['flour', 'oats', 'wheat', 'straw']},
    {'id': 'fruit', 'name': 'orchards and vines (fresh fruit, cider, wine)',
     'goods': ['apples', 'berries', 'cider', 'apple_brandy', 'grapes', 'wine', 'brandy', 'aqua_vitae']},
    {'id': 'preserved', 'name': 'dried and preserved fruit', 'goods': ['dried_apples', 'fruit_preserves']},
    {'id': 'meat', 'name': 'meat and livestock', 'goods': ['raw_meat', 'pig', 'mutton', 'rabbit', 'meal', 'sheep', 'offal', 'eggs']},
    {'id': 'byproducts', 'name': "the herds' by-products (tallow, hides, bone, wool)",
     'goods': ['tallow', 'hide', 'bones', 'wool', 'sheepskin', 'small_pelt']},
    {'id': 'fish', 'name': 'fish and shellfish', 'goods': ['fish', 'shellfish']},
    {'id': 'dairy', 'name': 'milk and dairy', 'goods': ['milk', 'butter', 'cheese']},
    {'id': 'honey', 'name': 'honey', 'goods': ['honey', 'beeswax']},
    {'id': 'vegetables', 'name': 'vegetables', 'goods': ['vegetables', 'mushrooms']},
    {'id': 'salt', 'name': 'salt', 'goods': ['salt']},
]
# The farms: each kind's count, its smallest sensible crew (a keeper and `min_hands`), what it sells (a share of each
# good's pennies) and grows (a share of its pieces; the same shares), where each lies and how big it is drawn. Goods
# two kinds share (meat, milk, fish) are split so that each kind can pay its crew; the small trades that can't pay a
# crew alone are run by a bigger farm: the mills by two of the grain farms, the drying houses by two of the orchards,
# the dairies by the cattle farms (their calves are the beef), the goats by the sheep runs.
FARMS = [
    {'kind': 'grain farm', 'area': 'grain', 'count': 3, 'min_hands': 2,
     'sells': {'flour': 1, 'oats': 1, 'wheat': 1, 'straw': 1},
     'where': [('central plains, on the river nearest the city; with a water mill', D_MID),
               ('central plains, on the river further east; with a water mill', D_MID), ('central plains, on the road east', D_MID)],
     'terrain': ['plain', 'meadow'], 'near': ['road', 'river'], 'tiles': 180 * 150 + 60 * 40, 'range': 0,
     'store': 'a granary: the harvest (wheat late summer, oats autumn) sold through the year; two farms grind at their mill'},
    {'kind': 'orchard', 'area': 'fruit', 'count': 5, 'min_hands': 2,
     'sells': {'apples': 1, 'berries': 1, 'cider': 1, 'apple_brandy': 1, 'dried_apples': 1, 'fruit_preserves': 1},
     'where': [('central hills, south-facing slopes; with a drying house', D_MID), ('central hills, south-facing slopes', D_MID),
               ('central hills, a river valley', D_MID), ('the sheltered valleys of the NW coast (cider country); with a drying house', D_NEAR),
               ('the sheltered valleys of the NW coast (cider country)', D_NEAR)],
     'terrain': ['hills', 'meadow'], 'near': ['road'], 'tiles': 90 * 70 + 30 * 20, 'range': 0,
     'store': 'a fruit store and cider press: picked late summer to autumn, kept into spring; two dry and preserve for the winter'},
    {'kind': 'vineyard', 'area': 'fruit', 'count': 2, 'min_hands': 2, 'sells': {'grapes': 1, 'wine': 1, 'brandy': 1, 'aqua_vitae': 1},
     'where': [('the warm south-facing hills at the SE desert\'s edge', D_FAR), ('central hills, the sunniest slope', D_MID)],
     'terrain': ['hills'], 'near': ['road'], 'tiles': 70 * 50 + 30 * 20, 'range': 0,
     'store': 'a press house and cellar: the vintage in autumn, wine by the cask all year'},
    {'kind': 'cattle and dairy farm', 'area': 'meat', 'count': 3, 'min_hands': 2,
     'sells': {'raw_meat': .65, 'meal': .65, 'milk': .85, 'butter': 1, 'cheese': 1, 'hide': .6, 'tallow': .5, 'bones': .5},
     'where': [('the NW coast\'s wet pasture just outside the city', D_NEAR)] * 2 + [('the foot of the Rainward Hills', D_NEAR)],
     'terrain': ['meadow', 'pasture', 'plain'], 'near': ['city', 'road'], 'tiles': 120 * 100 + 50 * 40, 'range': 200 * 200,
     'store': 'milk daily (it keeps a day: near the city); beef killed through the year, more in autumn, salted'},
    {'kind': 'pig farm', 'area': 'meat', 'count': 3, 'min_hands': 2,
     'sells': {'raw_meat': .35, 'meal': .35, 'pig': 1, 'offal': 1, 'hide': .4, 'tallow': .3, 'bones': .3},
     'where': [('the oak and beech woods of the central hills (pannage)', D_MID)] * 2 + [('the Rainwood\'s edge near the city', D_NEAR)],
     'terrain': ['forest', 'hills'], 'near': ['road'], 'tiles': 40 * 30, 'range': 150 * 150,
     'store': 'fattened on the autumn mast; salt pork carries the winter'},
    {'kind': 'sheep and goat run', 'area': 'meat', 'count': 2, 'min_hands': 2,
     'sells': {'mutton': 1, 'sheep': 1, 'wool': 1, 'sheepskin': 1, 'tallow': .2, 'bones': .2, 'milk': .15},
     'where': [('the central hills\' downs', D_MID), ('the N/NE mountains\' foothills (with the goats)', D_MID)],
     'terrain': ['hills', 'downs', 'mountain'], 'near': ['road'], 'tiles': 30 * 25, 'range': 250 * 250,
     'store': 'lambing in spring, shearing in summer, killing in autumn and winter; goats\' milk spring to autumn'},
    {'kind': 'rabbit warren', 'area': 'meat', 'count': 2, 'min_hands': 1, 'sells': {'rabbit': 1, 'small_pelt': 1, 'eggs': 1},
     'where': [('the dunes and sandy heath of the SW marsh\'s coast', D_MID), ('the sandy edge of the SE desert', D_FAR)],
     'terrain': ['heath', 'dunes', 'desert'], 'near': ['road'], 'tiles': 120 * 100, 'range': 0, 'store': 'all year'},
    {'kind': 'sea fishing boat', 'area': 'fish', 'count': 4, 'min_hands': 2, 'sells': {'fish': .45, 'shellfish': 1},
     'where': [('Ridgemere Sound, at the city\'s wharf', D_NEAR)] * 2 + [('a cove on the coast north of the city', D_NEAR),
                                                                      ('the SW marsh\'s coast', D_MID)],
     'terrain': ['coast'], 'near': ['sea'], 'tiles': 30 * 15, 'range': 0,
     'store': 'the winter runs are the best; shellfish raked from the shore beds'},
    {'kind': 'lake and river boat', 'area': 'fish', 'count': 2, 'min_hands': 1, 'sells': {'fish': .35},
     'where': [('Glasswater and the lakes under the N/NE mountains', D_MID), ('the Long River on the central plains', D_MID)],
     'terrain': ['lake', 'river'], 'near': ['lake', 'river'], 'tiles': 20 * 12, 'range': 0, 'store': 'steady, smaller'},
    {'kind': 'eel and fowl marsh', 'area': 'fish', 'count': 1, 'min_hands': 1, 'sells': {'fish': .2},
     'where': [('the SW marsh (eel traps, reed cutting, wildfowl)', D_MID)],
     'terrain': ['marsh'], 'near': ['water', 'road'], 'tiles': 25 * 15, 'range': 150 * 100, 'store': 'best in the autumn eel run'},
    {'kind': 'beekeeper', 'area': 'honey', 'count': 3, 'min_hands': 1, 'sells': {'honey': 1, 'beeswax': 1},
     'where': [('among the orchards of the central hills', D_MID)] * 2 + [('the thyme and heather scrub at the SE desert\'s edge', D_FAR)],
     'terrain': ['hills', 'meadow', 'scrub'], 'near': ['orchard'], 'tiles': 40 * 30, 'range': 0,
     'store': 'taken summer to autumn, kept in crocks for the winter'},
    {'kind': 'salt works', 'area': 'salt', 'count': 2, 'min_hands': 1, 'sells': {'salt': 1},
     'where': [('the SW marsh\'s coast (brine pans, boiled)', D_MID), ('a salt pan in the SE desert (raked in summer)', D_FAR)],
     'terrain': ['coast', 'marsh', 'desert'], 'near': ['sea', 'road'], 'tiles': 60 * 40, 'range': 0,
     'store': 'made in summer, kept dry in sheds'},
    {'kind': 'market garden', 'area': 'vegetables', 'count': 2, 'min_hands': 1, 'sells': {'vegetables': 1, 'mushrooms': 1},
     'where': [('outside the city\'s gates (worked by the cattle and dairy farms\' households)', D_NEAR)] * 2,
     'terrain': ['meadow'], 'near': ['city', 'road'], 'tiles': 60 * 40, 'range': 0, 'store': 'summer and autumn; roots into winter'},
]


def food_uses(sc, crafts, items):
    """Each drawn food good's pieces a day by season and by use: for residents' food, made into what the Country buys
    back, or into other goods (salves, wax); from why it was drawn and who bought what it made."""
    nourish = lambda g: (items.get(g, {}).get('food') or {}).get('nourish', 0)
    craft_out = {k: next(iter(c['out'])) for k, c in crafts.items()}

    def res_share(product, s):
        r = sc[s]['market'].get(product, {}).get('pieces', 0)
        c = sc[s]['bought'].get(product, {}).get('pieces', 0)
        return r / (r + c) if r + c > 0 else 1.0
    use_d = collections.defaultdict(lambda: {s: collections.Counter() for s in SEASONS})
    for s in SEASONS:
        for g, reasons in sc[s]['why'].items():
            for r, v in reasons.items():
                prod = g if r == 'resale' else craft_out.get(r, r)
                share = res_share(prod, s)
                use_d[g][s]['residents' if nourish(prod) > 0 else 'goods'] += v * share
                use_d[g][s]['back'] += v * (1 - share)
    return use_d


def money_first(a, sc, long, items, crafts, goods_s, ind_s, say):
    nourish = lambda g: (items.get(g, {}).get('food') or {}).get('nourish', 0)
    keeper_w = year_mean({s: long[s]['wage_keeper'] for s in SEASONS})
    hand_w = year_mean({s: long[s]['wage_hand'] for s in SEASONS})
    # A day's food at city prices: what a resident pays for the food it eats (drinks and wine left out: comforts).
    food = year_mean({s: sum(c['coins'] for g, c in sc[s]['market'].items() if nourish(g) >= 10) / long[s]['residents']
                      for s in SEASONS})
    K = keeper_w + food + UPKEEP
    H = hand_w + food + UPKEEP
    use_d = food_uses(sc, crafts, items)
    res = {s: long[s]['residents'] for s in SEASONS}
    toll_rate = year_mean({s: long[s]['toll_now'] for s in SEASONS}) / 100
    # The same diet bought at the Country's own prices: what the shops pay the Country a piece for the goods behind it.
    head_p = {s: sum(use_d[g][s]['residents'] / res[s] * (goods_s[g][s].get('country', 0) + goods_s[g][s].get('toll', 0)) /
                     goods_s[g][s]['pieces'] for g in use_d if g in goods_s and goods_s[g][s].get('pieces')) for s in SEASONS}
    MONEY_TOTALS['head_p'] = year_mean(head_p)
    say(f"A wolf's cost a day, at the settled years' means: a keeper {keeper_w:.1f}p and a hand {hand_w:.1f}p (the city's "
        f"tills' drifted wages), its own food at city prices {food:.1f}p (what a resident pays a day for what it eats, drink "
        f"left out), and upkeep {UPKEEP:g}p (tools, nails, planks, whitewash): **a keeper {K:.1f}p, a hand {H:.1f}p.** "
        f"(The same food at the Country's own prices, the goods behind it as the shops pay for them: {year_mean(head_p):.1f}p.)\n")

    def rev(g, s):           # a good's revenue a day: the Country's part of what the shops pay, less carriage in
        c = goods_s.get(g, {}).get(s, {})
        return c.get('country', 0) - c.get('carriage', 0)

    farms = []
    for k in FARMS:
        inc = {s: sum(rev(g, s) * sh for g, sh in k['sells'].items()) for s in SEASONS}
        n = k['count']
        R = year_mean(inc)
        x = (R / n - K) / H                                  # hands a farm the money pays
        hands = max(0, math.floor(x + 0.1))                  # (within a tenth of a hand: the keeper takes a little less)
        crew_cost = n * (K + hands * H)
        need = n * (K + k['min_hands'] * H)
        farms.append({**k, 'income_s': inc, 'revenue': R, 'can_pay': n + (R - n * K) / H,
                      'can_pay_s': {s: n + (inc[s] - n * K) / H for s in SEASONS}, 'hands': hands, 'crew': 1 + hands,
                      'crew_cost': crew_cost, 'short': max(0.0, need - R), 'need': need,
                      'reserve': reserve(inc, {s: crew_cost for s in SEASONS}, {s: 0 for s in SEASONS})})
    wolves = sum(f['count'] * f['crew'] for f in farms)
    # The yields: the city's draws of the farm's goods and the producers' own food (the crews eat the residents' diet),
    # with a little slack, shared over the farms.
    for f in farms:
        y = {}
        for g, share in f['sells'].items():
            per = []
            for s in SEASONS:
                city = goods_s.get(g, {}).get(s, {}).get('pieces', 0)
                own = wolves * use_d.get(g, {}).get(s, collections.Counter())['residents'] / res[s]
                per.append((city + own) * share * (1 + SLACK) / f['count'])
            if max(per) >= 0.05:
                y[g] = per
        f['yield'] = y
        main = [g for g in y if g not in AREAS[4]['goods']]
        f['yield_s'] = [sum(y[g][i] for g in main) for i in range(4)]

    # 1. The table for the user.
    say('### The table: what each food area can pay, its farms, their crews and yields\n')
    rows = []
    name_of = {ar['id']: ar['name'] for ar in AREAS}
    last = None
    for f in farms:
        flag = ''
        if f['hands'] < f['min_hands']:
            flag = f" (**short {f['short']:.0f}p a day** of 1 + {f['min_hands']} a farm)"
        elif f['revenue'] < f['crew_cost']:
            flag = f" (short {f['crew_cost'] - f['revenue']:.0f}p a day)"
        rows.append([name_of[f['area']] if f['area'] != last else '', f['kind'],
                     f"{f['revenue']:.0f}p ({min(f['income_s'].values()):.0f}-{max(f['income_s'].values()):.0f})",
                     f"{f['can_pay']:.1f}", f"{f['count']}", f"1 + {f['hands']}{flag}",
                     ' / '.join(f'{v:.0f}' for v in f['yield_s']), ' / '.join(f"{v / f['crew']:.0f}" for v in f['yield_s']),
                     '; '.join(dict.fromkeys(w for w, _ in f['where'])) + f" ({', '.join(dict.fromkeys(d.split(' ')[0] for _, d in f['where']))})"])
        last = f['area']
    say(table(['Area', 'Farm', 'Revenue a day, all its farms (season low-high)', 'Wolves it can pay', 'Farms',
               'Crew a farm (keeper + hands)', 'Yield a farm a day, Sp / Su / Au / Wi (pieces)', 'A worker a day',
               'Region (distance from the city)'], rows))
    tot_r = sum(f['revenue'] for f in farms)
    n_f = sum(f['count'] for f in farms)
    say(f"\nIn all: {tot_r:.0f}p a day, {sum(f['can_pay'] for f in farms):.0f} wolves it can pay, {n_f} farms, crews of "
        f"{wolves} wolves ({n_f} keepers, {wolves - n_f} hands), costing {sum(f['crew_cost'] for f in farms):.0f}p a day. "
        f"The yields include {SLACK * 100:.0f}% slack and the {wolves} producers' own food.\n")

    # 2. The revenue in full, by area and season.
    say('### Revenue by area and season (pennies a day, settled years)\n')
    rows = []
    for ar in AREAS:
        gross = year_mean({s: sum(goods_s.get(g, {}).get(s, {}).get('country', 0) + goods_s.get(g, {}).get(s, {}).get('toll', 0)
                                  for g in ar['goods']) for s in SEASONS})
        toll = year_mean({s: sum(goods_s.get(g, {}).get(s, {}).get('toll', 0) for g in ar['goods']) for s in SEASONS})
        carr = year_mean({s: sum(goods_s.get(g, {}).get(s, {}).get('carriage', 0) for g in ar['goods']) for s in SEASONS})
        inc = {s: sum(rev(g, s) for g in ar['goods']) for s in SEASONS}
        MONEY_TOTALS.setdefault('area_rev', {})[ar['id']] = inc
        back = sum(rev(g, s) * use_d[g][s]['back'] / use_d[g][s].total() * SEASON_DAYS[s] / 365
                   for g in ar['goods'] for s in SEASONS if g in use_d and use_d[g][s].total())
        pcs = year_mean({s: sum(goods_s.get(g, {}).get(s, {}).get('pieces', 0) for g in ar['goods']) for s in SEASONS})
        rows.append([ar['name'], f'{pcs:.0f}', f'{gross:.0f}p', f'{toll:.0f}p', f'{carr:.1f}p'] + [f'{inc[s]:.0f}p' for s in SEASONS] +
                    [f'**{year_mean(inc):.0f}p**', f'{back:.0f}p' if back >= 1 else '-'])
    say(table(['Area', 'Pieces a day', "The shops pay", 'Gate toll', 'Carriage in', 'Revenue Spring', 'Summer', 'Autumn', 'Winter',
               'Year', 'of it, behind what the Country buys back'], rows))
    say('')

    # 3. Season by season: what it can pay, and the reserve.
    say('### Season by season: wolves each kind can pay, and the reserve its crews need\n')
    rows = []
    for f in farms:
        lean = [s for s in SEASONS if f['income_s'][s] < f['crew_cost']]
        rows.append([f['kind'], f"{f['count']} x (1 + {f['hands']})"] + [f"{f['can_pay_s'][s]:.1f}" for s in SEASONS] +
                    [f"{f['crew_cost']:.0f}p", ', '.join(lean) or '-', f"{f['reserve']:.0f}p",
                     f"{f['reserve'] / max(1e-9, f['crew_cost']):.0f}"])
    say(table(['Farm', 'Crews', 'Can pay, Spring', 'Summer', 'Autumn', 'Winter', 'The crews cost a day', 'Lean seasons',
               'Reserve to carry them (all farms of the kind)', 'In days of wages'], rows))
    say('')

    # 4. The levers for the kinds short of a sensible crew.
    short = [f for f in farms if f['hands'] < f['min_hands'] or f['revenue'] < f['crew_cost']]
    if short:
        say('### The levers, for the kinds that fall short (pennies a day, all farms of the kind)\n')
        rows = []
        for f in short:
            gap = max(f['short'], f['crew_cost'] - f['revenue'])
            no_toll = f['revenue'] * toll_rate / (1 - toll_rate)
            own_food = f['count'] * (1 + f['min_hands']) * (food - MONEY_TOTALS.get('head_p', 4.3))
            price = gap / f['revenue'] * 100 if f['revenue'] > 0 else float('inf')
            rows.append([f['kind'], f"{f['count']} x (1 + {f['min_hands']}): {f['need']:.0f}p", f"{f['revenue']:.0f}p",
                         f'{gap:.0f}p', f'+{no_toll:.0f}p', f'+{own_food:.0f}p', f'+{price:.0f}%' if price < 1e9 else '-'])
        say(table(['Farm', 'A sensible crew costs', 'Revenue', 'Short', 'No gate toll on its goods', "Its crew's food at the Country's own prices",
                   'Or its prices up by'], rows))
        say('')

    # 5. Yields in full, per good (a farm, a day).
    say('### Yields per farm, per good (pieces a day, Sp / Su / Au / Wi; with the slack and the producers\' food)\n')
    rows = []
    for f in farms:
        for g, v in f['yield'].items():
            city = [goods_s.get(g, {}).get(s, {}).get('pieces', 0) * f['sells'][g] / f['count'] for s in SEASONS]
            rows.append([f['kind'], g, ' / '.join(f'{x:.1f}' for x in v), ' / '.join(f'{x:.1f}' for x in city)])
    say(table(['Farm', 'Good', 'Yield, a farm a day', "of it the city's draws (the rest: the producers' food and the slack)"], rows))
    say('')
    MONEY_TOTALS.update({'K': K, 'H': H, 'food': food, 'keeper_w': keeper_w, 'hand_w': hand_w, 'wolves': wolves,
                         'revenue': tot_r, 'toll_rate': toll_rate})

    # 6. The whole Country's money once the farms take the food over.
    intake = year_mean({s: sum(ind_s[i][s]['country'] for i in ind_s) for s in SEASONS})
    spent = year_mean({s: long[s]['country_spent'] for s in SEASONS})
    n_f = sum(f['count'] for f in farms)
    wages = n_f * keeper_w + (wolves - n_f) * hand_w
    food_all = wolves * food
    upkeep = wolves * UPKEEP
    food_toll = sum(year_mean({s: sum(goods_s.get(g, {}).get(s, {}).get('toll', 0) for g in ar['goods']) for s in SEASONS})
                    for ar in AREAS)
    food_country = year_mean({s: sum(goods_s.get(g, {}).get(s, {}).get('country', 0) for ar in AREAS for g in ar['goods'])
                              for s in SEASONS})
    keeps = intake - food_country
    say('### The Country\'s money once the farms take the food over (a day)\n')
    say(table(['', 'Pennies a day'], [
        ["The Country's intake now (all its goods, after the toll)", f'{intake:.0f}p'],
        ['of it, the food areas\' (to the farms; they pay the carriage in, ' +
         f'{food_country - tot_r:.0f}p, out of it)', f'{food_country:.0f}p'],
        ['The Country keeps (its other goods)', f'{keeps:.0f}p'],
        ['... for its 91 posts in the city (8p)', f'{91 * WAGE["hand"]:.0f}p'],
        ['... and its basket of the city\'s goods (now)', f'{keeps - 91 * WAGE["hand"]:.0f}p ({spent:.0f}p)'],
        ['The farms\' crews\' wages, spent in the city', f'{wages:.0f}p'],
        ['their food (at city prices)', f'{food_all:.0f}p'],
        ['the farms\' upkeep (city goods: tools, nails, planks)', f'{upkeep:.0f}p'],
        ['The farms keep', f'{tot_r - wages - food_all - upkeep:+.0f}p'],
        ['The gate toll the food pays the treasury (of its whole toll)', f'{food_toll:.0f}p ({year_mean({s: long[s]["toll"] for s in SEASONS}):.0f}p)'],
    ]))
    say('')
    MONEY_TOTALS.update({'intake': intake, 'spent_now': spent, 'crew_wages': wages, 'crew_food': food_all, 'food_toll': food_toll})
    return farms


def others_table(others, ind_s, say):
    say('### The other industries (more briefly)\n')
    rows = []
    for k, v in others.items():
        inc = year_mean({s: ind_s[k][s]['country'] - ind_s[k][s]['carriage'] for s in SEASONS}) if k in ind_s else 0
        units_n = sum(n for _, n, _ in v['units'])
        wage = units_n * MONEY_TOTALS['K'] + max(0, v['plausible'] - units_n) * MONEY_TOTALS['H']
        v['income'], v['cost'] = inc, wage
        v['can_pay'] = units_n + (inc - units_n * MONEY_TOTALS['K']) / MONEY_TOTALS['H']
        rows.append([k, ', '.join(f'{g} {d:.0f}' for g, d in sorted(v['draw'].items(), key=lambda x: -x[1])),
                     f"{inc:.0f}p", f"{v['can_pay']:.0f}", f"{v['fte']:.0f}", f"{v['plausible']:.0f}", f"{inc / wage:.2f}",
                     '; '.join(f'{n} {u} ({w})' for u, n, w in v['units'])])
    say(table(['Industry', 'Drawn a day', 'Revenue a day', 'Wolves it can pay', "Workers at the game's rates",
               'Workers, plausible', 'Cover at plausible', 'Units (where)'], rows))
    say('')


OTHER_FOOTPRINT = {   # tiles: the works' own, and woods or ground they work that they share with the wild
    'logging camp': {'own_tiles': 40 * 30, 'shared_range_tiles': 200 * 200},
    "coppice and woodcutters' hut": {'own_tiles': 20 * 15, 'shared_range_tiles': 100 * 100},
    'charcoal hearth': {'own_tiles': 30 * 30, 'shared_range_tiles': 0},
    'saw pit or water sawmill': {'own_tiles': 30 * 20, 'shared_range_tiles': 0},
    'iron mine': {'own_tiles': 40 * 30, 'shared_range_tiles': 0},
    'bloomery (iron smelter)': {'own_tiles': 30 * 20, 'shared_range_tiles': 0},
    'copper and tin pit': {'own_tiles': 30 * 20, 'shared_range_tiles': 0},
    'quarry (stone and limestone)': {'own_tiles': 80 * 60, 'shared_range_tiles': 0},
    'clay pit and sand bank': {'own_tiles': 60 * 40, 'shared_range_tiles': 0},
    'hemp and flax fields': {'own_tiles': 80 * 60, 'shared_range_tiles': 0},
    "herb gatherers' cottage": {'own_tiles': 15 * 10, 'shared_range_tiles': 150 * 150},
    'village pottery': {'own_tiles': 25 * 20, 'shared_range_tiles': 0},
    "weavers' and soap boilers' cottages": {'own_tiles': 20 * 15, 'shared_range_tiles': 0},
    'country press and paper mill': {'own_tiles': 30 * 20, 'shared_range_tiles': 0},
}


def write_farms(path, farms, others):
    """The placement list and the farms' settings, in the form Atlas's Economy page would edit (doc 71, 4 and 6)."""
    areas = []
    for area in AREAS:
        fs = [f for f in farms if f['area'] == area['id']]
        made_by = sorted({f['kind'] for f in farms if any(g in area['goods'] for g in f['sells'])} - {f['kind'] for f in fs})
        out = []
        for f in fs:
            for i in range(f['count']):
                where, dist = f['where'][i % len(f['where'])]
                out.append({
                    'id': f"{f['kind'].replace(' ', '_')}_{i + 1}", 'kind': f['kind'], 'region': where, 'distance': dist,
                    'terrain': f['terrain'], 'near': f['near'],
                    'footprint': {'own_tiles': f['tiles'], 'shared_range_tiles': f['range']},
                    'crew': {'keepers': 1, 'hands': f['hands']},
                    'wages': {'keeper': None, 'hand': None},
                    'yield': {g: [round(x, 1) for x in v] for g, v in f['yield'].items()},
                    'reserveDays': round(f['reserve'] / max(1e-9, f['crew_cost'])),
                    'sells': f['sells'], 'store': f['store']})
        areas.append({'id': area['id'], 'name': area['name'], 'goods': area['goods'], 'yieldScale': 100,
                      'revenueADay': [round(MONEY_TOTALS['area_rev'][area['id']][s], 1) for s in SEASONS],
                      'madeBy': made_by, 'farms': out})
    doc = {
        'format': 'ratw-country-farms', 'version': 1,
        'about': ("The Country's food farms and other works as they would be placed (Docs/Design/71-the-country-s-industries.md). "
                  "Written by tools/country_industry.py from the 12-year run (settled years 4-12); nothing reads it yet. Each "
                  "farm is a setting Atlas's Economy page and the DM's Economy tab would edit, per farm and per area: `crew` "
                  "(keepers and hands), `wages` (pennies a day; null: the world's own, the city's drifting keeper's and hand's), "
                  "`yield` (pieces a day of each good, for Spring, Summer, Autumn and Winter, the value at each season's middle "
                  "day as country.json's seasons are), and an area's `yieldScale` (percent, on all its farms). The yields are "
                  "the city's draws plus the producers' own food plus `slack`, shared over the farms. `footprint`: own_tiles are "
                  "the farm's own (fields, orchard, steading); shared_range_tiles is open grazing or woodland its herds share "
                  "with the wild. `distance` is from Ridgemere (the realm's north-west corner)."),
        'defaults': {'wages': {'keeper': round(MONEY_TOTALS['keeper_w'], 1), 'hand': round(MONEY_TOTALS['hand_w'], 1)},
                     'foodADay': round(MONEY_TOTALS['food'], 1), 'upkeepADay': UPKEEP, 'slack': SLACK},
        'areas': areas,
        'others': [{'industry': k, 'revenueADay': round(v['income']), 'workersItCanPay': max(0, math.floor(v['can_pay'])),
                    'workersPlausible': round(v['plausible']),
                    'units': [{'kind': u, 'count': n, 'region': w, 'footprint': OTHER_FOOTPRINT.get(u, {})}
                              for u, n, w in v['units']]} for k, v in others.items()],
    }
    text = json.dumps(doc, indent=1)
    # Short lists of numbers or words on one line.
    text = re.sub(r'\[\s+([^\[\]{}]*?)\s+\]', lambda m: '[' + ', '.join(x.strip() for x in m.group(1).split(',')) + ']', text)
    open(path, 'w').write(text + '\n')
    print(f'wrote {path}', file=sys.stderr)


def size_units(demand, sc, goods_s):
    """The units for a demand (primary pieces a day by season), with land, labour by season and the goods they sell."""
    kg = A['kg']
    t_year = lambda pg, share=1.0: sum(demand.get(pg, {}).get(s, 0) * SEASON_DAYS[s] for s in SEASONS) * kg.get(pg, 0) * share / 1000
    U = {}
    ls = A['land_scale']

    def unit(name, count, size, ha, worker_days, profile, out, keepers=None, extra_tiles=0, workers=None, fixed_staff=None):
        spec = UNITS.get(name, {})
        w = workers or {s: worker_days * p / season_workdays(s) for s, p in zip(SEASONS, profile)}
        low = min(w.values())
        staff = fixed_staff if fixed_staff is not None else max(count, math.ceil(low - 1e-9))
        seasonal_s = {s: max(0.0, w[s] - staff) for s in SEASONS}
        seasonal = math.ceil(max(seasonal_s.values()) - 1e-9)
        seasonal_days = sum(seasonal_s[s] * SEASON_DAYS[s] for s in SEASONS)
        peak = max(SEASONS, key=lambda s: w[s])
        # Open grazing and woodland pannage are a range the herds share with the wild (it needn't be the unit's alone);
        # fields, orchards, vines, beds, meadows and the steadings are the unit's own.
        ranged = spec.get('kind') in ('pasture', 'wood') and name != 'dairy farm'
        tiles = (0 if ranged else ha * 10000 / ls) + extra_tiles
        U[name] = {'count': count, 'size': size, 'area': f'{ha:,.0f} ha' if ha else '-', 'ha': ha, 'tiles': tiles,
                   'range_tiles': ha * 10000 / ls if ranged else 0, 'steading_tiles': extra_tiles,
                   'out': out, 'workers': w, 'staff': staff, 'seasonal': seasonal, 'seasonal_days': seasonal_days,
                   'seasonal_s': seasonal_s, 'peak': peak, 'fte_year': sum(w[s] * SEASON_DAYS[s] for s in SEASONS) / 365,
                   'keepers': keepers or count, 'region': spec.get('region', ''), 'sells': [g for g, v in SOLD_BY.items() if name in v],
                   'income_s': {s: sum(goods_s.get(g, {}).get(s, {}).get('country', 0) * share for g, sh in SOLD_BY.items()
                                       for un, share in sh.items() if un == name) for s in SEASONS}}

    # Grain: three fields (wheat, oats, fallow); seed kept back.
    g = UNITS['grain farm']
    wheat_ha = t_year('wheat') / (g['yield_t_ha']['wheat'] * (1 - g['seed_share']))
    oats_ha = t_year('oats') / (g['yield_t_ha']['oats'] * (1 - g['seed_share']))
    sown = wheat_ha + oats_ha
    fallow = sown / 2
    arable = sown + fallow
    n = max(2, round(arable / g['unit_ha']))
    unit('grain farm', n, f"{arable / n:.0f} ha arable ({wheat_ha / n:.0f} wheat, {oats_ha / n:.0f} oats, {fallow / n:.0f} fallow)",
         arable, sown * g['days_ha'] + fallow * g['days_ha_fallow'], g['season_labour'],
         f"{t_year('wheat'):.0f} t wheat, {t_year('oats'):.0f} t oats a year", extra_tiles=n * 60 * 40)
    # Mills: two, a miller and a hand each.
    unit('water mill', 2, f"{t_year('wheat') / 365 * 1000 / 2:.0f} kg of wheat a day each (of about 1 t it could)", 0, 0, [.25] * 4,
         'flour', workers={s: 4.0 for s in SEASONS}, fixed_staff=4, extra_tiles=2 * UNITS['water mill']['footprint_tiles'])
    o = UNITS['orchard']
    ha = t_year('apples') / o['yield_t_ha']['apples'] + t_year('berries') / 2   # (berries from the hedges, 2 t a ha of hedge)
    n = max(2, round(ha / o['unit_ha']))
    unit('orchard', n, f"{ha / n:.0f} ha, {ha / n * o['trees_ha']:.0f} trees, a fruit store and cider press", ha, ha * o['days_ha'],
         o['season_labour'], f"{t_year('apples'):.0f} t apples a year", extra_tiles=n * 40 * 30)
    v = UNITS['vineyard']
    ha = t_year('grapes') / v['yield_t_ha']['grapes']
    n = max(2, round(ha / v['unit_ha']))
    unit('vineyard', n, f"{ha / n:.1f} ha of vines, a press house and cellar", ha, ha * v['days_ha'], v['season_labour'],
         f"{t_year('grapes'):.1f} t grapes a year (a third pressed on the estate)", extra_tiles=n * 30 * 20)
    m = UNITS['market garden']
    ha = t_year('vegetables') / m['yield_t_ha']['vegetables']
    n = 2
    unit('market garden', n, f"{ha / n:.1f} ha of beds", ha, max(ha * m['days_ha'], 2 * 313), m['season_labour'],
         f"{t_year('vegetables'):.1f} t vegetables a year", extra_tiles=n * 20 * 15)
    d = UNITS['dairy farm']
    cows = t_year('milk', .75) * 1000 / d['litres_cow_year']
    head = cows * d['herd_per_milker']
    n = max(2, round(head / d['unit_head']))
    unit('dairy farm', n, f"{cows / n:.0f} cows in milk, {head / n:.0f} head", head * d['ha_head'], head * d['days_head'],
         d['season_labour'], f"{t_year('milk', .75) * 1000:,.0f} L milk a year", extra_tiles=n * 40 * 30)
    gf = UNITS['goat fold']
    goats = t_year('milk', .25) * 1000 / gf['litres_head_year']
    n = max(2, round(goats / gf['unit_head']))
    unit('goat fold', n, f"{goats / n:.0f} goats", goats * gf['ha_head'], max(goats * gf['days_head'], n * 313 * .5),
         gf['season_labour'], f"{t_year('milk', .25) * 1000:,.0f} L milk a year, kids", extra_tiles=n * 25 * 20)
    c = UNITS['cattle ranch']
    beef = t_year('raw_meat', c['meat_share']) * 1000
    killed = beef / c['kg_slaughtered']
    head = killed * c['head_per_slaughter']
    n = max(2, round(head / c['unit_head']))
    unit('cattle ranch', n, f"{head / n:.0f} head, {killed / n:.0f} killed a year", head * c['ha_head'], head * c['days_head'],
         c['season_labour'], f"{beef / 1000:.0f} t beef a year, hides, tallow", extra_tiles=n * 50 * 40)
    p = UNITS['piggery']
    pork = (t_year('raw_meat', 1 - c['meat_share']) + t_year('pig')) * 1000
    killed = pork / p['kg_slaughtered']
    head = killed * p['head_per_slaughter']
    n = max(2, round(head / p['unit_head']))
    unit('piggery', n, f"{head / n:.0f} pigs ({head / n / 10:.0f} sows), {killed / n:.0f} killed a year", head * p['ha_head'],
         head * p['days_head'], p['season_labour'], f"{pork / 1000:.0f} t pork a year", extra_tiles=n * 30 * 25)
    sh = UNITS['sheep run']
    mutton = t_year('mutton') * 1000
    killed = mutton / sh['kg_slaughtered']
    flock = killed * sh['head_per_slaughter']
    n = max(2, round(flock / sh['unit_head']))
    unit('sheep run', n, f"{flock / n:.0f} sheep", flock * sh['ha_head'], flock * sh['days_head'], sh['season_labour'],
         f"{mutton / 1000:.0f} t mutton, {flock * 1.5 / 1000:.1f} t wool a year", extra_tiles=n * 30 * 25)
    r = UNITS['rabbit warren']
    does = t_year('rabbit') * 1000 / r['kg_doe']
    n = max(2, round(does / r['unit_head']))
    unit('rabbit warren', n, f"{does / n:.0f} does", does * r['ha_doe'], does * r['days_doe'], r['season_labour'],
         f"{t_year('rabbit'):.0f} t rabbit a year, pelts", extra_tiles=n * 20 * 15)
    ap = UNITS['apiary']
    hives = t_year('honey') * 1000 / ap['kg_hive']
    n = max(2, round(hives / ap['unit_head']))
    unit('apiary', n, f"{hives / n:.0f} skeps", 0, hives / ap['hives_keeper'] * 313, ap['season_labour'],
         f"{t_year('honey'):.1f} t honey a year, wax", extra_tiles=n * ap['footprint_tiles'])
    # Fishing: the eel marsh and the lake boats first, the sea boats the rest; boats kept for the busiest season.
    sea, lake, eel = UNITS['sea fishing boat'], UNITS['lake and river boat'], UNITS['eel and fowl marsh']
    n_eel, n_lake = 2, 4
    need_kg = {s: (demand.get('fish', {}).get(s, 0) * kg['fish'] + demand.get('shellfish', {}).get(s, 0) * kg['shellfish'])
               for s in SEASONS}
    sea_kg = {s: max(0.0, need_kg[s] - n_eel * eel['kg_day'][s] - n_lake * lake['kg_day'][s]) for s in SEASONS}
    boats = math.ceil(max(sea_kg[s] / sea['kg_day'][s] for s in SEASONS) - 1e-9)
    active = {s: sea_kg[s] / sea['kg_day'][s] for s in SEASONS}
    crews = {s: (active[s] * sea['crew'] + n_lake * lake['crew'] + n_eel * eel['crew']) * 7 / 6 for s in SEASONS}
    unit('fishing', boats + n_lake + n_eel, f"{boats} sea boats (3 each), {n_lake} lake and river boats (2), {n_eel} eel and fowl "
         f"marshes (3); a keeper at each of five landings", 0, 0, [.25] * 4, f"{sum(need_kg[s] * SEASON_DAYS[s] for s in SEASONS) / 1000:.0f} t fish and shellfish a year",
         workers=crews, keepers=5,
         extra_tiles=boats * sea['footprint_tiles'] + n_lake * lake['footprint_tiles'] + n_eel * eel['footprint_tiles'])
    U['fishing']['boats'] = {'sea': boats, 'lake': n_lake, 'eel': n_eel, 'kg_day': need_kg}
    sa = UNITS['salt works']
    t = t_year('salt')
    n = 2
    unit('salt works', n, f"{t / n:.1f} t a year, {t / n / sa['t_ha_year'] * 10000:.0f} m2 of pans", t / sa['t_ha_year'],
         t * 25, sa['season_labour'], f"{t:.0f} t salt a year", extra_tiles=n * 30 * 20)
    return U


def unit_money(name, u, own, head_p, keeper=None, hand=None, family_harvest=False, toll_free=0.0, price_up=None):
    """A kind of unit's money a day by season: income from the city (the Country's part of what the shops pay, after
    the gate toll) against its wages (a keeper each, year-round hands, seasonal hands the days they work) and upkeep.
    The levers: keeper and hand wages; family_harvest (the seasonal hands are the households' own, fed, not waged);
    toll_free (the toll's rate taken off food, so the unit keeps it); price_up ({good: factor} on the unit's prices)."""
    keeper = WAGE['keeper_now'] if keeper is None else keeper
    hand = WAGE['hand_now'] if hand is None else hand
    keepers = u['keepers']
    hands = max(0, u['staff'] - keepers)
    wage_staff = keepers * keeper + hands * hand
    seasonal_wage = 0 if family_harvest else hand
    cost = {s: wage_staff + u['seasonal_s'][s] * seasonal_wage + u['workers'][s] * 6 / 7 * UPKEEP for s in SEASONS}
    inc = dict(u['income_s'])
    if toll_free:
        inc = {s: v / (1 - toll_free) for s, v in inc.items()}
    if price_up:
        f = price_up.get(name, 1)
        inc = {s: v * f for s, v in inc.items()}
        own = {s: v * f for s, v in own.items()}
    iy = year_mean(inc)
    cy = year_mean(cost)
    wages_y = year_mean({s: wage_staff + u['seasonal_s'][s] * seasonal_wage for s in SEASONS})
    # The first year from Spring 1: costs every day, income from the first harvest (the herds and boats at once).
    first = FIRST_INCOME.get(name, 1)
    capital, day = 0.0, 1
    for s in SEASONS:
        for _ in range(SEASON_DAYS[s]):
            if day >= first:
                break
            capital += cost[s]
            day += 1
    return {'income': inc, 'cost': cost, 'income_y': iy, 'cost_y': cy, 'net_y': iy - cy, 'cover': iy / cy if cy else 0,
            'wages_y': wages_y, 'upkeep_y': cy - wages_y, 'capital': capital, 'first_income_day': first,
            # With board: the units' people buy their food from the Country's units (own) out of their wages, so each
            # unit takes in what its share of the Country's own food is worth.
            'own': own, 'own_y': year_mean(own), 'net_board_y': iy + year_mean(own) - cy,
            'cover_board': (iy + year_mean(own)) / cy if cy else 0,
            'reserve': reserve(inc, cost, own) if iy + year_mean(own) >= cy else float('inf')}


POOL = [   # (the pool's group, words in the work label), first match; a label's place is dropped first.
    ('fields and mills', ['works the fields', 'farms the', 'water mill']),
    ('orchards and vines', ['orchard', 'picks apples', 'tends the vines', 'fell wine']),
    ('herds and flocks', ['raises sheep', 'keeps sheep', 'milks the herd', 'keeps pigs', 'keeps rabbits', 'dairy', 'stables']),
    ('bees', ['keeps bees']),
    ('fishing and the marsh', ['fishes', 'traps eels', 'smokehouse', 'fishmonger', 'cuts reeds']),
    ('woods', ['logging', 'cuts wood', 'sawmill', 'traps in the', 'gathers herbs']),
    ('mines, stone and clay', ['digs ore', 'cutting stone', 'digs clay', 'lime kilns', 'brickworks', 'smelter', 'timberland iron']),
    ('at home', ['keeps the house']),
    ('trades and shops', ['smith', 'baker', 'brewer', 'provisioner', 'shopkeeper', 'butcher', 'carpenter', 'herbalist', 'keeps the ',
                          'runs the ', "minds the shop", 'selling', 'press']),
    ('the watch', ['stands watch']),
    ('service and labour', ['in service', 'serves', 'helps', 'works', "ships' accounts", 'between voyages']),
    ('children', ['plays about', 'at lessons']),
    ('the old', ['sits by the well', 'at leisure', 'visiting']),
    ('the Heights', ['at home in the heights']),
]


POOL_OF = {'grain farm': 'fields and mills', 'water mill': 'fields and mills', 'market garden': 'fields and mills',
           'orchard': 'orchards and vines', 'vineyard': 'orchards and vines', 'fruit drying house': 'orchards and vines',
           'cattle farm': 'herds and flocks', 'pig farm': 'herds and flocks', 'sheep run': 'herds and flocks',
           'cattle and dairy farm': 'herds and flocks', 'sheep and goat run': 'herds and flocks',
           'rabbit warren': 'herds and flocks', 'dairy farm': 'herds and flocks', 'goat fold': 'herds and flocks',
           'beekeeper': 'bees', 'sea fishing boat': 'fishing and the marsh', 'lake and river boat': 'fishing and the marsh',
           'eel and fowl marsh': 'fishing and the marsh', 'salt works': 'fishing and the marsh',
           'woods: timber, firewood, bark, charcoal': 'woods', 'herbs': 'woods', 'mining and metals': 'mines, stone and clay',
           'stone, lime, clay, sand': 'mines, stone and clay', 'fibre: hemp, flax, linen': 'fields and mills',
           "country workshops' goods": 'trades and shops'}


def unplaced_pool(path, say, farms=None, others=None):
    rows = json.load(open(path))
    groups = collections.defaultdict(list)
    for r in rows:
        label = (r.get('work_label') or '').lower()
        g = next((g for g, words in POOL if any(w in label for w in words)), 'other')
        if g in ('at home', 'service and labour') and r['age'] < 16:
            g = 'children'
        groups[g].append(r)
    say(f'### 3d. The unplaced NPCs in DEV ({len(rows)}; read only)\n')
    out = []
    order = [g for g, _ in POOL] + ['other']
    for g in order:
        rs = groups.get(g, [])
        if not rs:
            continue
        sex = collections.Counter(json.loads(r['appearance']).get('sex') for r in rs if r.get('appearance'))
        working = [r for r in rs if 16 <= r['age'] < 60]
        ids = collections.Counter(r['id'].split('_')[0] for r in rs)
        out.append([g, f'{len(rs)}', f'{len(working)}', f"{sum(1 for r in rs if r['age'] < 16)}", f"{sum(1 for r in rs if r['age'] >= 60)}",
                    f"{sex.get('female', 0)} / {sex.get('male', 0)}", f"{min(r['age'] for r in rs)}-{max(r['age'] for r in rs)}",
                    ', '.join(f'{k} {v}' for k, v in ids.most_common())])
    working = sum(1 for r in rows if 16 <= r['age'] < 60)
    out.append(['**all**', f'**{len(rows)}**', f'**{working}**', f"**{sum(1 for r in rows if r['age'] < 16)}**",
                f"**{sum(1 for r in rows if r['age'] >= 60)}**", '', '', ''])
    say(table(['Old trade', 'NPCs', 'Working age (16-59)', 'Under 16', '60 and over', 'Female / male', 'Ages', 'Id prefix (cty, tn, rm)'], out))
    say('')
    if not farms:
        return
    need_food, need_other = collections.Counter(), collections.Counter()
    for f in farms:
        need_food[POOL_OF[f['kind']]] += f['count'] * f['crew']
    for k, v in (others or {}).items():
        need_other[POOL_OF[k]] += min(round(v['plausible']), max(0, math.floor(v['can_pay'])))
    # (The market gardens are worked by the cattle farms' households: no crew of their own.)
    out = []
    for g in order:
        w = sum(1 for r in groups.get(g, []) if 16 <= r['age'] < 60)
        nf, no = need_food.get(g, 0), need_other.get(g, 0)
        if not (w or nf or no):
            continue
        out.append([g, f'{w}', f'{nf}' if nf else '-', f'{no}' if no else '-', f'{w - nf - no:+d}'])
    tw = sum(1 for r in rows if 16 <= r['age'] < 60)
    out.append(['**all**', f'**{tw}**', f'**{sum(need_food.values())}**', f'**{sum(need_other.values())}**',
                f'**{tw - sum(need_food.values()) - sum(need_other.values()):+d}**'])
    say('The farms\' crews and the other industries\' workers against the pool, by old trade (working age):\n')
    say(table(['Old trade', 'Unplaced, working age', 'The food farms\' crews', 'The other industries (what they can pay, at most the plausible)',
               'Spare (+) or short (-)'], out))
    say('')


def reserve(inc, cost, own):
    """The working money a unit needs in a steady year: the deepest fall of its running balance (its income from the
    city and from the Country's own people, less wages and upkeep), over a year from any day."""
    days = [s for s in SEASONS for _ in range(SEASON_DAYS[s])]
    net = [inc[s] + own[s] - cost[s] for s in days]
    run, high, worst = 0.0, 0.0, 0.0
    for x in net + net:
        run += x
        high = max(high, run)
        worst = max(worst, high - run)
    return worst


def write_units(path, U, others):
    rows = []
    for name, u in U.items():
        spec = UNITS.get(name, {})
        rows.append({'kind': name, 'count': u['count'], 'each': u['size'], 'region': u['region'] or spec.get('region', ''),
                     'terrain': spec.get('terrain', []), 'near': spec.get('near', []),
                     'land_ha_real_all': round(u['ha'], 1), 'tiles_all': round(u['tiles']), 'tiles_each': round(u['tiles'] / u['count'])})
    json.dump({'units': rows, 'others': others}, open(path, 'w'), indent=1, default=float)


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='cmd', required=True)
    m = sub.add_parser('measure')
    m.add_argument('runs', nargs='+', help='SEASON=EVENTS.json')
    m.add_argument('--out', required=True)
    r = sub.add_parser('report')
    r.add_argument('measured')
    r.add_argument('economy')
    r.add_argument('--from-year', type=int, default=4)
    r.add_argument('--real-check', action='store_true',
                   help='also size the units from real pre-industrial yields and labour (the first method; a cross-check)')
    r.add_argument('--units')
    r.add_argument('--unplaced')
    r.add_argument('--dump')
    a = ap.parse_args()
    {'measure': cmd_measure, 'report': cmd_report}[a.cmd](a)


if __name__ == '__main__':
    main()
