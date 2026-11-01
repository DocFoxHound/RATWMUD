#!/usr/bin/env python3
"""What the Country holds, and what it takes to grow its food on real farms (Docs/Design/71-the-country-s-industries.md).

    python3 tools/country_industry.py measure SEASON=EVENTS.json ... --world EXPORT/world.ratw --out MEASURED.json
    python3 tools/country_industry.py report MEASURED.json ECONOMY.jsonl [--units Data/Economy/country_farms.json]
                                            [--unplaced UNPLACED.json] [--from-year 4] [--real-check] [--dump D.json]

measure: reads world_check --events files (one short run per season: `--day N --economy F --events E`), skips each
run's first three days (the shelves filling) and its last, partial day, and keeps a day's mean of: every store draw by
good and by why it was drawn (pieces; the till's pennies to the Country, the gate toll and carriage in, paired from the
ledger: a draw's three coin shifts come just before the `made` record that names its good), the Country's purchases, the
residents' purchases and meals, each shop's sales by buyer (its business from the export's work labels, as
items::businessFor reads them), the Country's posts' wages, and the run's own economy lines for the same days.

report: fills in the autumn short run's missing wine from the long run's vintner (its loop file, ECONOMY's name with
_loop, or --loop: `fill_wine`), scales each season to the long run's settled years (ECONOMY.jsonl, years `--from-year`
to 12; `scale`: a season group's pieces by its G_drawn and its pennies by its season price, a craft's other inputs with
its group's, the rest to the long run's drawn pennies, the toll at its rate, the residents' food at NourishADay), sorts
the goods into the Country's industries and prints the doc's tables (markdown). Then the switch (the user, 2026-11-01):
the farms take the food over. A farm's crew costs it their wages (they buy their food in the city's shops, which draw it
from the farms); its basket of the city's goods takes what is over the wages, its till's cash cycle charging a lean
season to the others (`cash_cycle`); the Country keeps its other goods, its posts, its patrons and what is left for its
own basket; every basket and purchase draws again from the farms and the Country, solved season by season (`solve`). It
prints the lead table (what each kind could pay on wages alone, solved with the crews' own food and spending:
`wages_only`), the takings, the reserves, the Country before and after, the toll (and with every penny on wages instead
of the basket), the makers' order book, the money the switch holds (the floats and the crews' purses), the treasury's
change (the wage support the losing trades would draw at the settled years' wage steps, `settled_steps` and `support`,
and the long run's settled years replayed night by night with it, `replay`), the yields (season means and the middle-day
values whose blend has them, `middles`) and the season caps against the draws after the switch, the other industries
and their crewed works, the footprints, and the pool of unplaced NPCs
(--unplaced: a JSON list of DEV's unplaced NPCs, read only: id, role, work_label, age, appearance). --units writes
Data/Economy/country_farms.json, the farms' settings as Atlas's Economy page would edit them. --real-check prints the
first draft's sizing from real pre-industrial yields and labour instead (UNITS, ASSUMPTIONS; a cross-check of land and
labour).

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
NOURISH_A_DAY = .0035 * 14400 / 1.1   # RatwCityLoop.cpp NourishADay: hunger .0035 a second, food counted 1.1 times; 45.8
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
        'region': 'the foothills of the snowy mountains along the north', 'terrain': ['hills', 'mountain'], 'near': ['road'], 'kind': 'pasture',
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
        'region': 'the central hills and downs; the northern foothills', 'terrain': ['hills', 'downs'], 'near': ['road'],
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
        'region': 'the west coast: the sea off the city and the coves south of it', 'terrain': ['coast'],
        'near': ['sea', 'city'], 'kind': 'wharf', 'footprint_tiles': 30 * 15,
    },
    'lake and river boat': {
        'goods': ['fish'],
        'about': 'A two-wolf boat or a weir with traps on a lake or the rivers (trout, perch, pike); steadier, smaller.',
        'crew': 2, 'kg_day': {'spring': 20, 'summer': 18, 'autumn': 20, 'winter': 14},
        'region': 'the lakes under the northern mountains and the rivers of the plains', 'terrain': ['lake', 'river'],
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
def businesses_of(world):
    """Each shop's till (till:job:<keeper's id>) and its business, from a world export's residents (`resident "id" "name"
    "role" "work label" ...`), as items::businessFor reads a work label: the first business whose match is in it."""
    import shlex
    biz = json.load(open(ROOT / 'Data/Items/businesses.json'))['businesses']

    def business_for(label):
        w = label.lower()
        return next((b['id'] for b in biz for m in b.get('match', []) if m and m in w), 'other')
    out = {}
    for line in open(world):
        if line.startswith('resident '):
            f = shlex.split(line)
            if len(f) > 4 and f[3] == 'merchant':
                out['till:job:' + f[1]] = business_for(f[4])
    return out


def measure_one(path, tills=None):
    ev = json.load(open(path))['events']
    days = sorted({int(e['day']) for e in ev})
    use = set(days[3:-1])
    n = len(use)
    tills = tills or {}
    biz = lambda t: tills.get(t, 'other')
    pend = collections.defaultdict(lambda: collections.Counter())    # till -> 'country', 'toll', 'carriage' since last draw
    draws = collections.defaultdict(lambda: collections.Counter())   # item -> pieces, country, toll, carriage
    draws_why = collections.defaultdict(lambda: collections.defaultdict(collections.Counter))   # item -> reason -> the same
    why = collections.defaultdict(collections.Counter)                 # item -> reason -> pieces
    bought = collections.defaultdict(collections.Counter)              # Country's purchases: item -> pieces, coins
    market = collections.defaultdict(collections.Counter)              # residents' purchases
    # Sales by business and buyer (the order book): business -> buyer -> item -> pieces, coins. Buyers: residents (their
    # market, food, needs and comforts), services (residents' services), country (the Country's basket), patrons (the
    # Country's and the treasury's patrons' work and kit), city (till to till: the suppliers rule), treasury (its works).
    sales = collections.defaultdict(lambda: collections.defaultdict(lambda: collections.defaultdict(collections.Counter)))
    drawn_by = collections.defaultdict(lambda: collections.defaultdict(collections.Counter))   # business -> item -> the draw's coins
    eaten = collections.Counter()
    made = collections.Counter()                                       # the city's makers' batches' output, by good
    country_wages = collections.Counter()                              # the Country's posts: coins and payees a day
    # Each business's money (doc 71, 2.6, the treasury after the switch): its tills' wages, its keepers' profit, the
    # treasury's wage support to its posts, the city's share of its services, what it buys from other city makers, and
    # its draws' carriage; its tills and posts. A post's till is the one that pays it wages (any day of the run).
    money_by = collections.defaultdict(collections.Counter)
    tills_of = collections.defaultdict(set)
    posts_of = collections.defaultdict(set)
    holder_till = {}
    for e in ev:
        if e['kind'] == 'economy' and e['detail'] == 'wages' and e['actor'].startswith('till:'):
            holder_till[e['target']] = e['actor']
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
                reason = 'resale' if d.startswith('restocked') else d[len('drawn for '):]
                for c in (draws[e['item']], draws_why[e['item']][reason]):
                    c['pieces'] += e['quantity']
                    for k in ('country', 'toll', 'carriage'):
                        c[k] += p[k]
                why[e['item']][reason] += e['quantity']
                drawn_by[biz(t)][e['item']]['pieces'] += e['quantity']
                drawn_by[biz(t)][e['item']]['country'] += p['country']
                drawn_by[biz(t)][e['item']]['toll'] += p['toll']
                money_by[biz(t)]['carriage_in'] += p['carriage']
        elif not inside:
            continue
        elif d == 'wages' and a.startswith('till:'):
            money_by[biz(a)]['wages'] += e['coins']
            tills_of[biz(a)].add(a)
            posts_of[biz(a)].add(t)
        elif d == 'profit' and a.startswith('till:'):
            money_by[biz(a)]['profit'] += e['coins']
        elif d == 'wage support' and a == 'treasury' and t in holder_till:
            money_by[biz(holder_till[t])]['support'] += e['coins']
        elif d == "the city's share of a service" and a.startswith('till:'):
            money_by[biz(a)]['service_share'] += e['coins']
        elif a == 'made' and d == 'made from the store room':
            made[e['item']] += e['quantity']
        elif d == 'to the Country':
            bought[e['item']]['pieces'] += e['quantity']
            bought[e['item']]['coins'] += e['coins']
            sales[biz(t)]['country'][e['item']].update({'pieces': e['quantity'], 'coins': e['coins']})
        elif d == 'carriage out':
            carriage_out += e['coins']
        elif d == 'market' and t.startswith('till:'):
            market[e['item']]['pieces'] += e['quantity']
            market[e['item']]['coins'] += e['coins']
            sales[biz(t)]['residents'][e['item']].update({'pieces': e['quantity'], 'coins': e['coins']})
        elif d == 'a service' and t.startswith('till:'):
            sales[biz(t)]['services'][e['item'] or 'service'].update({'pieces': e['quantity'] or 1, 'coins': e['coins']})
        elif d in ("a patron's work", "a patron's new kit") and t.startswith('till:'):
            sales[biz(t)]['patrons'][e['item'] or 'work'].update({'pieces': e['quantity'] or 1, 'coins': e['coins']})
        elif d == 'from a city maker' and t.startswith('till:'):
            sales[biz(t)]['city'][e['item'] or 'goods'].update({'pieces': e['quantity'], 'coins': e['coins']})
            if a.startswith('till:'):
                money_by[biz(a)]['city_inputs'] += e['coins']
        elif a == 'treasury' and t.startswith('till:') and e['coins'] > 0 and d not in ('wage support', 'refunded: gate toll'):
            sales[biz(t)]['treasury'][e['item'] or d].update({'pieces': e['quantity'], 'coins': e['coins']})
        elif a == 'outside:country' and d == 'wages':
            country_wages['coins'] += e['coins']
            country_wages['payees'] += 1
        elif d == 'eat':
            eaten[e['item']] += e['quantity']
    per = lambda c: {k: {kk: vv / n for kk, vv in v.items()} for k, v in c.items()}
    return {'days': n, 'kept': sorted(use), 'draws': per(draws), 'why': per(why),
            'draws_why': {g: per(r) for g, r in draws_why.items()}, 'bought': per(bought), 'market': per(market),
            'sales': {b: {k: per(v) for k, v in by.items()} for b, by in sales.items()},
            'drawn_by': {b: per(v) for b, v in drawn_by.items()},
            'eaten': {k: v / n for k, v in eaten.items()}, 'made': {k: v / n for k, v in made.items()},
            'carriage_out': carriage_out / n, 'country_wages': {k: v / n for k, v in country_wages.items()},
            'money_by': {b: dict({k: v / n for k, v in c.items()}, tills=len(tills_of[b]), posts=len(posts_of[b]))
                         for b, c in money_by.items()}}


ECON_KEYS = ['residents', 'drawn', 'toll', 'carriage', 'country_spent', 'food_bought', 'residents_spent', 'needs_bought',
             'comforts_bought', 'wages_paid', 'country', 'treasury', 'patrons_country', 'toll_now', 'wage_keeper', 'wage_hand']


def cmd_measure(a):
    tills = businesses_of(a.world) if a.world else {}
    out = {}
    for spec in a.runs:
        season, path = spec.split('=', 1)
        out[season] = measure_one(path, tills)
        econ = Path(path).with_name(Path(path).name.replace('_events.json', '.jsonl'))
        if econ.exists():
            lines = list(load_lines(econ))
            # The economy lines of the same days as the events (a line's `day` counts from the run's first day).
            first = out[season]['kept'][0] - 3
            used = [d for d in lines if d['day'] + first in set(out[season]['kept'])]
            keys = ECON_KEYS + [f'{g}_{x}' for g in GROUPS for x in ('drawn', 'price', 'cap')]
            out[season]['economy'] = {k: sum(d.get(k, 0) for d in used) / len(used) for k in keys}
            out[season]['economy']['days'] = len(used)
    out['businesses'] = tills      # each till's business (for the long run's per-till sales: the vintners' wine)
    json.dump(out, open(a.out, 'w'), indent=1)
    print(f'wrote {a.out}: ' + ', '.join(f'{s} {v["days"]} days' for s, v in out.items() if s in SEASONS))


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


LONG_KEYS = ['residents', 'drawn', 'toll', 'carriage', 'country_spent', 'country_unspent', 'food_bought', 'residents_spent',
             'needs_bought', 'comforts_bought', 'services_spent', 'wages_paid', 'wages_owed', 'country', 'treasury', 'money',
             'purses', 'tills', 'carters', 'profit', 'alms', 'surplus', 'works_spent', 'wage_support', 'patrons_country',
             'patrons_treasury', 'toll_now', 'wage_keeper', 'wage_hand', 'posts', 'posts_held', 'working_age', 'unplaced',
             'arrivals']


def settled(econ_path, from_year, to_year=12):
    acc = collections.defaultdict(lambda: collections.defaultdict(float))
    cnt = collections.Counter()
    years = set()
    lo = collections.defaultdict(lambda: float('inf'))
    hi = collections.defaultdict(lambda: float('-inf'))
    first = {}
    last = {}
    for d in load_lines(econ_path):
        y = d['day'] // 365 + 1
        if y < from_year or y > to_year:
            continue
        years.add(y)
        s = d['season']
        cnt[s] += 1
        for k in LONG_KEYS + [f'{g}_{x}' for g in GROUPS for x in ('drawn', 'cap', 'price', 'sold')]:
            acc[s][k] += d.get(k, 0)
        for k in ('residents', 'country', 'treasury'):
            lo[k], hi[k] = min(lo[k], d.get(k, 0)), max(hi[k], d.get(k, 0))
        first = first or d
        last = d
    out = {s: {k: v / cnt[s] for k, v in acc[s].items()} for s in SEASONS}
    out['range'] = {k: (lo[k], hi[k]) for k in lo}
    out['first'], out['last'] = first, last
    return out, sorted(years)


def season_of(day):
    y, start = day % 365, 0
    for s in SEASONS:
        if y < start + SEASON_DAYS[s]:
            return s
        start += SEASON_DAYS[s]
    return SEASONS[-1]


def vintners_wine(loop_path, businesses, from_day):
    """The long run's vintners' wine sales a day, by season (its loop file's per-till sales over the settled years). A
    vintner only resells (drawFromStore) and its shelves hold about a week, so its sales are its draws."""
    tills = {t for t, b in businesses.items() if b == 'vintner'}
    acc = {s: [] for s in SEASONS}
    for d in load_lines(loop_path):
        if d['day'] >= from_day:
            acc[season_of(d['day'])].append(sum(v.get('wine', 0) for t, v in d['loop']['sales'].items() if t in tills))
    return {s: sum(v) / len(v) for s, v in acc.items() if v}


def settled_steps(loop_path, businesses, from_day, to_day):
    """Each trade's mean wage step in the long run's settled years (pennies a post over the world's wages; judgeWages),
    and its tills, from the loop file's per-till wage records (every 28 days). (Their running sums since the last
    judgement, the wages owed, the keeper's profit and the short days, are empty at those snapshots for all but a few
    tills: the snapshots fall on the tills' judgement days. So a trade's posts are owed their wages at its step, and
    what its tills can pay them is its margin on its orders: doc 71, 2.6.)"""
    steps = collections.defaultdict(list)
    tills = collections.defaultdict(set)
    for d in load_lines(loop_path):
        if from_day <= d['day'] <= to_day:
            for t, w in d['loop'].get('wages', {}).items():
                b = businesses.get(t, 'other')
                tills[b].add(t)
                steps[b].append(w.get('step', 0))
    return {b: {'step': sum(v) / len(v), 'tills': len(tills[b])} for b, v in steps.items()}


def fill_wine(measured, wine):
    """The autumn short run's vintner drew and sold no wine (its kept days, 215-225: it held only cider), though the
    long run's (the wine vaults, the city's one vintner) sold it all year (12.6 / 11.4 / 13.9 / 11.1 a day) and nothing in
    the code makes a vintage. A
    season whose vintners drew under a quarter of the other seasons' mean gets their wine records (the draws for resale,
    the vintners' sales to the residents, the residents' market and spending) at the other seasons' mean a day, times
    the long run's season over its other seasons' mean; the rest of the scaling (the long run's drawn pennies, the
    residents' spending) then takes it in."""
    import copy
    m = copy.deepcopy(measured)
    pcs = {s: m[s]['draws_why'].get('wine', {}).get('resale', {}).get('pieces', 0) for s in SEASONS}
    filled = {}
    for s in SEASONS:
        others = [o for o in SEASONS if o != s and pcs[o] > 0]
        if not others or pcs[s] >= .25 * sum(pcs[o] for o in others) / len(others):
            continue
        k = wine[s] / (sum(wine[o] for o in others) / len(others)) / len(others) if wine else 1 / len(others)
        mean = lambda get: sum(get(o) for o in others) * k
        rs = {kk: mean(lambda o: m[o]['draws_why']['wine']['resale'].get(kk, 0)) for kk in ('pieces', 'country', 'toll', 'carriage')}
        sold = {kk: mean(lambda o: m[o]['sales'].get('vintner', {}).get('residents', {}).get('wine', {}).get(kk, 0))
                for kk in ('pieces', 'coins')}
        m[s]['draws_why'].setdefault('wine', {})['resale'] = rs
        d = m[s]['draws'].setdefault('wine', {})
        for kk, v in rs.items():
            d[kk] = d.get(kk, 0) + v
        m[s]['why'].setdefault('wine', {})['resale'] = rs['pieces']
        m[s]['sales'].setdefault('vintner', {}).setdefault('residents', {})['wine'] = sold
        mk = m[s]['market'].setdefault('wine', {'pieces': 0, 'coins': 0})
        mk['pieces'] = mk.get('pieces', 0) + sold['pieces']
        mk['coins'] = mk.get('coins', 0) + sold['coins']
        m[s]['economy']['residents_spent'] += sold['coins']
        filled[s] = {'pieces': rs['pieces'], 'sold': sold['pieces'], 'factor': k * len(others)}
    return m, filled


def scale(measured, long, group_of, crafts, items):
    """Each season's draws, purchases and meals at the settled years' level (doc 71, "How it was measured"):
    - a season group's goods: pieces by the group's G_drawn (the long run's over the short run's), pennies also by its
      season price (the season's mean G_price over the short run's window: a short run samples days just before the
      season's middle, not the season);
    - a good outside the groups drawn for a craft whose other inputs are in a group follows that group (salt for salt
      fish follows the fish, for jerky and salt pork the meat; firewood for bread the grain), at its own price;
    - the rest (resale outside the groups, and the non-food crafts) take what is left of the long run's drawn pennies;
    - the gate toll at the long run's rate;
    - the residents' food bought at NourishADay a head (the short runs' households were eating down their starting
      stock, so they bought less than they ate), their other buying by their other spending; the Country's by its
      spending."""
    nourish = lambda g: (items.get(g, {}).get('food') or {}).get('nourish', 0)
    out = {}
    for s in SEASONS:
        m, L = measured[s], long[s]
        short = m['economy']
        gf = {g: (L[f'{g}_drawn'] / short[f'{g}_drawn']) if short[f'{g}_drawn'] > 0 else 1 for g in GROUPS}
        pf = {g: (L[f'{g}_price'] / short[f'{g}_price']) if short.get(f'{g}_price') else 1 for g in GROUPS}
        cost = lambda c: c.get('country', 0) + c.get('toll', 0)

        def linked(good, reason):
            """(pieces factor, price factor) for a draw tied to a group; None for the rest."""
            if group_of.get(good) in gf:
                g = group_of[good]
                return gf[g], pf[g]
            k = crafts.get(reason) if reason != 'resale' else None
            if k:
                ins = [(x, n) for x, n in k['in'].items() if group_of.get(x) in gf]
                if ins:
                    w = sum(n for _, n in ins)
                    return sum(gf[group_of[x]] * n for x, n in ins) / w, 1.0
            return None
        fixed = rest_cost = 0.0
        for g, reasons in m['draws_why'].items():
            for r, c in reasons.items():
                f = linked(g, r)
                if f:
                    fixed += cost(c) * f[0] * f[1]
                else:
                    rest_cost += cost(c)
        rest = max(0.0, L['drawn'] - fixed) / rest_cost if rest_cost else 1.0
        draws_why = {}
        for g, reasons in m['draws_why'].items():
            draws_why[g] = {}
            for r, c in reasons.items():
                f = linked(g, r) or (rest, 1.0)
                draws_why[g][r] = {'pieces': c.get('pieces', 0) * f[0], 'country': c.get('country', 0) * f[0] * f[1],
                                   'toll': c.get('toll', 0) * f[0] * f[1], 'carriage': c.get('carriage', 0) * f[0]}
        # The toll at the settled years' rate (the short runs start at the world's 30%; the drift firmed it to 31-33%).
        tr = L['toll'] / sum(c['toll'] for r in draws_why.values() for c in r.values())
        for reasons in draws_why.values():
            for c in reasons.values():
                paid = c['country'] + c['toll']
                c['toll'] *= tr
                c['country'] = paid - c['toll']
        draws = {}
        for g, reasons in draws_why.items():
            d = collections.Counter()
            for c in reasons.values():
                d.update(c)
            draws[g] = dict(d)
        why = {g: {r: c['pieces'] for r, c in reasons.items()} for g, reasons in draws_why.items()}
        # The residents' buying: their food at NourishADay a head; what else they buy by the rest of their spending.
        food_n = sum(c['pieces'] * nourish(g) for g, c in m['market'].items() if nourish(g) >= 10)
        pop = L['residents'] * NOURISH_A_DAY / food_n
        food_short = sum(c['coins'] for g, c in m['market'].items() if nourish(g) >= 10)
        other_short = short['residents_spent'] - food_short
        other = max(0.0, L['residents_spent'] - food_short * pop) / other_short
        res_f = lambda g: pop if nourish(g) >= 10 else other
        cf = L['country_spent'] / short['country_spent']
        sales = {}
        for b, by in m['sales'].items():
            sales[b] = {}
            for buyer, goods in by.items():
                k = (lambda g: res_f(g)) if buyer == 'residents' else (lambda g: other) if buyer == 'services' else \
                    (lambda g: cf) if buyer == 'country' else (lambda g: 1.0)
                sales[b][buyer] = {g: {kk: vv * k(g) for kk, vv in c.items()} for g, c in goods.items()}
        out[s] = {
            'draws': draws, 'draws_why': draws_why, 'why': why,
            'bought': {g: {k: v * cf for k, v in c.items()} for g, c in m['bought'].items()},
            'eaten': {g: v * L['residents'] / short['residents'] for g, v in m['eaten'].items()},
            'market': {g: {k: v * res_f(g) for k, v in c.items()} for g, c in m['market'].items()},
            'sales': sales, 'made': m['made'],
            'carriage_out': m['carriage_out'] * cf, 'factors': gf, 'price_factors': pf, 'rest': rest, 'pop': pop,
            'other': other, 'country_factor': cf, 'country_wages': m.get('country_wages', {}),
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
    # The long run's vintners' wine by season (its loop file: ECONOMY with _loop), to fill the autumn short run's.
    loop = Path(a.loop) if a.loop else Path(a.economy).with_name(Path(a.economy).stem + '_loop.jsonl')
    wine = vintners_wine(loop, measured['businesses'], (a.from_year - 1) * 365) \
        if loop.exists() and measured.get('businesses') else None
    measured, filled = fill_wine(measured, wine)
    # Each trade's wages, keepers' profit and wage support in the settled years (the treasury after the switch, 2.6).
    trades = settled_steps(loop, measured['businesses'], (a.from_year - 1) * 365, 12 * 365 - 1) \
        if loop.exists() and measured.get('businesses') else None
    sc = scale(measured, long, group_of, crafts, items)
    price = lambda g: items.get(g, {}).get('price', 0)
    nourish = lambda g: (items.get(g, {}).get('food') or {}).get('nourish', 0)
    P = []
    say = P.append
    pop = year_mean({s: long[s]['residents'] for s in SEASONS})
    say(f'Settled years {years[0]}-{years[-1]} of the 12-year run; residents {pop:,.0f} on average.\n')
    if wine:
        say("The long run's vintners' wine sales a day: " + ', '.join(f'{s} {v:.1f}' for s, v in wine.items()) +
            '. Filled in (the short run drew none): ' + (', '.join(
                f"{s} {v['pieces']:.1f} drawn for resale, {v['sold']:.1f} sold (x{v['factor']:.2f} the other seasons' mean)"
                for s, v in filled.items()) or 'none') + '.\n')

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
            r.append(f"{c['pieces']:,.0f} / {c['country'] + c['toll']:,.0f}p")
            tot[s].update(c)
        ym = {k: year_mean({s: ind_s[ind][s][k] for s in SEASONS}) for k in ('pieces', 'country', 'toll', 'carriage')}
        gross = ym['country'] + ym['toll']
        sw = [ind_s[ind][s]['country'] + ind_s[ind][s]['toll'] for s in SEASONS]
        r += [f"{ym['pieces']:,.0f} / {gross:,.0f}p", f"{ym['toll']:,.0f}p", f"{ym['carriage']:.1f}p", f"{ym['country']:,.0f}p",
              f"{min(sw) / max(1e-9, gross) * 100:,.0f}-{max(sw) / max(1e-9, gross) * 100:,.0f}%"]
        rows.append(r)
    ymt = {k: year_mean({s: tot[s][k] for s in SEASONS}) for k in ('pieces', 'country', 'toll', 'carriage')}
    rows.append(['**all**'] + [f"**{tot[s]['pieces']:,.0f} / {tot[s]['country'] + tot[s]['toll']:,.0f}p**" for s in SEASONS] +
                [f"**{ymt['pieces']:,.0f} / {ymt['country'] + ymt['toll']:,.0f}p**", f"**{ymt['toll']:,.0f}p**",
                 f"**{ymt['carriage']:,.0f}p**", f"**{ymt['country']:,.0f}p**", ''])
    say(table(['Industry', 'Spring pieces / paid', 'Summer', 'Autumn', 'Winter', 'Year', 'Gate toll', 'Carriage in (the shop pays)',
               "The Country's intake", 'Season swing (of the year mean)'], rows))
    say('')
    # Check against the long run's own totals.
    say('Check, the long run\'s own a day (year mean): drawn ' +
        f"{year_mean({s: long[s]['drawn'] for s in SEASONS}):,.0f}p, toll {year_mean({s: long[s]['toll'] for s in SEASONS}):,.0f}p, "
        f"carriage {year_mean({s: long[s]['carriage'] for s in SEASONS}):,.0f}p, the Country spent "
        f"{year_mean({s: long[s]['country_spent'] for s in SEASONS}):,.0f}p.\n")

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
            ws = ', '.join(f'{k} {v / tw * 100:,.0f}%' for k, v in why.most_common(3))
            rows.append([ind, g, f'{price(g)}p'] + [f'{p:,.0f}' for p in pcs] + [f'{gross:,.0f}p', ws])
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
    say(f'In all {tb:,.0f}p a day of goods (the long run\'s `country_spent` scaled: '
        f"{year_mean({s: long[s]['country_spent'] for s in SEASONS}):,.0f}p), and {co:,.0f}p of carriage out. Of it, food and drink "
        f"made from the Country's own produce: {sum(food_back.values()):,.0f}p ("
        + ', '.join(f'{g} {v:,.0f}p ({buy_p[g]:,.0f})' for g, v in sorted(food_back.items(), key=lambda x: -x[1])) + ').\n')
    say('The top of the basket: ' + ', '.join(f'{g} {v:,.0f}p' for g, v in buy.most_common(20)) + '.\n')

    # The Country's money now (the short runs' ledgers: its posts; the long run: its patrons and carriage out).
    posts = year_mean({s: sc[s]['country_wages'].get('coins', 0) for s in SEASONS})
    payees = year_mean({s: sc[s]['country_wages'].get('payees', 0) for s in SEASONS})
    patrons = year_mean({s: long[s]['patrons_country'] for s in SEASONS})
    intake = year_mean({s: sum(ind_s[i][s]['country'] for i in ind_s) for s in SEASONS})
    say(f"The Country's money a day: in, {intake:,.0f}p (the Country's part of every draw); out, its {payees:,.0f} posts "
        f"{posts:,.0f}p, its patrons {patrons:,.0f}p, carriage out {co:,.0f}p and its basket {tb:,.0f}p "
        f"({intake - posts - patrons - co - tb:+,.0f}p; its purse {year_mean({s: long[s]['country'] for s in SEASONS}):,.0f}p on average, "
        f"{long['range']['country'][0]:,.0f}-{long['range']['country'][1]:,.0f}p).\n")

    # ---- 2. Food: what residents buy, and the chain to primary goods.
    if a.real_check:
        food_and_units(a, sc, long, items, crafts, producers, goods_s, ind_s, buy, pop, say)
    else:
        st = switch_report(a, sc, long, items, crafts, producers, goods_s, ind_s, group_of, say, measured, trades)
        others_table(st['others'], st['kw'], st['hw'], say)
        U, land, labour = real_check_of(st['farms'], sc, goods_s)
        own_t, rng_t = footprints(st['farms'], land)
        say('### The footprints on the map (tiles a farm; a cell is 256 x 256)\n')
        rows = [[f['kind'], f"{f['count']}", f"{f['own_tiles']:,.0f}", f"{f['range_tiles']:,.0f}",
                 f"{f['own_tiles'] * f['count'] / CELL:.2f}", f"{f['range_tiles'] * f['count'] / CELL:.2f}"] for f in st['farms']]
        own_all = sum(f['own_tiles'] * f['count'] for f in st['farms'])
        rng_all = sum(f['range_tiles'] * f['count'] for f in st['farms'])
        rows.append(['**all**', f"**{st['nf']}**", '', '', f'**{own_all / CELL:.1f}**', f'**{rng_all / CELL:.1f}**'])
        say(table(['Farm', 'Farms', 'Own tiles a farm', 'Shared range a farm', 'Own, cells (all)', 'Range, cells (all)'], rows))
        say(f"\nReal land for these yields (the cross-check, drawn at 1:{A['land_scale']}): {own_t:,.0f} tiles of the farms' own "
            f"({own_t / CELL:.1f} cells) and {rng_t:,.0f} of shared range ({rng_t / CELL:.1f} cells), {(own_t + rng_t) / CELL:.1f} "
            f"cells; at 1:1 ten times that. The placement list draws {own_all / CELL:.1f} + {rng_all / CELL:.1f} cells.\n")
        crew = st['n']
        say(f"Real work for these yields (the cross-check's labour norms; the game's posts don't follow it): "
            + ', '.join(f'{s} {labour[s]:,.0f}' for s in SEASONS) + f" workers on the job, against the {crew} crew "
            f"year-round ({sum(max(0, labour[s] - crew) * SEASON_DAYS[s] for s in SEASONS):,.0f} hand-days over the crew a year, "
            f"{sum(max(0, crew - labour[s]) * SEASON_DAYS[s] for s in SEASONS):,.0f} under it).\n")
        st.update({'labour': labour, 'own_t': own_t, 'rng_t': rng_t, 'own_all': own_all, 'rng_all': rng_all})
        if a.unplaced:
            unplaced_pool(a.unplaced, say, st['farms'], st['others'], long)
        if a.units:
            write_farms(a.units, st, st['others'])
        if a.dump:
            json.dump({k: v for k, v in st.items() if k in ('farms', 'even', 'kw', 'hw', 'posts', 'patrons', 'n', 'nf', 'W',
                                                            'tot_r', 'fb', 'B', 'tparts', 'toll_now', 'reserve', 'labour')},
                      open(a.dump, 'w'), indent=1, default=float)
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
    return 'short' if x == float('inf') or x > 1e18 else f'{x:,.0f}{unit}'


def season_workdays(s):
    return SEASON_DAYS[s] * WORKDAYS


def other_industries(sc, crafts, producers):
    """Section 5: the other industries' workers, at the game's own rates (crafts.json: the producers' spells, the
    city's crafts' hours for what the Country makes), FTE (a worker's 8 hours, six days in seven), and plausible ones;
    and their works, each kind with its count, place and the share of the work it does (`job`: workers at the plausible
    rates), so that the workers the money pays can be spread over them, at least one a site (`crew_sites`)."""
    draw = collections.Counter()
    for s in SEASONS:
        for g, c in sc[s]['draws'].items():
            draw[g] += c.get('pieces', 0) * SEASON_DAYS[s] / 365
    hours = lambda pid: producers[pid]['hours']
    out = {}
    fte = lambda h: h / 8 / WORKDAYS
    # The woods: logging (2 timber, 4 firewood, 1 bark a spell), woodcutting for the rest of the firewood, charcoal
    # burning; the Country's iron wants charcoal too (an iron bar: 2 ore and a charcoal). Plausible norms, where the
    # game's are far off: a coppice worker cuts, splits and stacks about a tonne of firewood a day, a feller 1.5 t of logs;
    # a collier and his mate keep a clamp burning about 150 kg of charcoal a day; two sawyers at a pit saw about 20 planks
    # a day (firewood 10 kg a piece, timber 30 kg, charcoal 3 kg).
    log_spells = max(draw['timber'] / 2, draw['oak_bark'] / 1)
    wood_spells = max(0, draw['firewood'] - 4 * log_spells) / 3
    char = draw['charcoal'] + draw['iron_bar']
    planks_h = draw['planks'] / 4 * crafts['planks']['hours']
    jobs = {'fire': draw['firewood'] * 10 / 1000 / WORKDAYS, 'logs': draw['timber'] * 30 / 1500 / WORKDAYS,
            'char': char * 3 / 150 * 2 / WORKDAYS, 'saw': draw['planks'] / 10 / WORKDAYS}
    out['woods: timber, firewood, bark, charcoal'] = {
        'fte': fte(log_spells * hours('logging') + wood_spells * hours('woodcutting') + char * hours('charcoal_burning') + planks_h),
        'plausible': sum(jobs.values()),
        'units': [('logging camp', 2, 'one in the conifer forests of the snowy mountains\' foothills along the north, one in the '
                                     'central hills\' oak woods', jobs['logs']),
                  ("coppice and woodcutters' hut", 2, 'one beside the grain farms\' villages on the plains, one in the woods '
                                                       'nearest the city (the city burns 340-460 firewood a day)', jobs['fire']),
                  ('charcoal hearth', 2, 'in the foothill forests, one beside each bloomery', jobs['char']),
                  ('water sawmill', 2, 'one on a river below each logging camp', jobs['saw'])],
        'goods': ['timber', 'firewood', 'oak_bark', 'charcoal', 'planks']}
    ore = draw['iron_ore'] + 2 * draw['iron_bar']
    mine_h = ore * hours('mine')
    bloom_h = draw['iron_bar'] * crafts['iron_bar']['hours']
    cu_h = max(draw['copper_ore'], draw['tin_ore']) * hours('copper_pit')
    out['mining and metals'] = {
        'fte': fte(mine_h + bloom_h + cu_h + 2 * draw['bronze_bar'] * crafts['bronze_bar']['hours']),
        # (The game's copper pit gives a copper ore and a tin ore a spell, the tin streams two tin: two pits and the
        # streams are two sources of copper and three of tin.)
        'units': [('iron mine', 2, 'two valleys of the snowy mountains along the north', fte(mine_h)),
                  ('bloomery (iron smelter)', 2, 'beside the mines, with charcoal from the foothill forests', fte(bloom_h)),
                  ('copper pit', 2, 'the snowy mountains\' foothills, and the central hills (copper ore, and some tin)', fte(cu_h) * .7),
                  ('tin streams', 1, 'a stream of the central hills (the tin ore)', fte(cu_h) * .3)],
        'goods': ['iron_ore', 'iron_bar', 'copper_ore', 'tin_ore', 'copper_bar', 'bronze_bar']}
    q_h = max(draw['stone'], draw['limestone']) * hours('quarry') + draw['lime'] / 3 * crafts['lime']['hours']
    clay_h = draw['clay'] / 4 * hours('clay_pit')
    out['stone, lime, clay, sand'] = {
        # (A quarry's spell gives clay and sand too in the game; plausibly they are dug from the pits and banks.)
        'fte': fte(q_h + clay_h),
        'units': [('quarry (stone and limestone)', 2, 'the central hills (limestone) and the nearest hills east of the city (building stone)', fte(q_h)),
                  ('clay pit and sand bank', 2, 'river clay on the central plains; sand on the coast south of the city', fte(clay_h))],
        'goods': ['stone', 'limestone', 'clay', 'sand', 'lime']}
    fibre = draw['hemp'] + draw['flax'] + draw['cord'] / 3 + 2 * draw['linen_cloth']
    f_fte = fte(fibre * 0.5 / 2 * 8 + draw['cord'] / 3 * crafts['cord']['hours'] + draw['linen_cloth'] * crafts['linen_cloth']['hours'])
    out['fibre: hemp, flax, linen'] = {
        # Grown as a field of each grain farm (about 2 kg a worker-day all in: pulling, retting, scutching), spun and
        # woven at home in winter (the city's own hours for cord and linen): only half needs its own hands.
        'fte': f_fte, 'plausible': f_fte / 2,
        'units': [('hemp and flax field', 2, 'a field of each grain farm on the plains, its retting pond on the farm\'s river', f_fte / 2)],
        'goods': ['hemp', 'flax', 'cord', 'linen_cloth']}
    out['herbs'] = {'fte': fte(sum(draw[g] for g in INDUSTRY['herbs']) / 3 * hours('herb_gathering')),
                    # About 8p a day: it pays no one. No works of its own: gathered at the marsh's edge by the eel and
                    # fowl marsh's crew and in the central hills' woods, the pennies still the Country's.
                    'units': [], 'goods': INDUSTRY['herbs']}
    ws = INDUSTRY["country workshops' goods"]
    wh = collections.Counter()
    site_of = {'clay_pot': 'pots', 'wool_scarf': 'cottages', 'fine_soap': 'cottages', 'bandages': 'cottages',
               'pamphlet': 'press', 'paper': 'press', 'ink': 'press', 'iron_hoops': 'smithy', 'nails': 'smithy',
               'cookpot': 'smithy', 'leather': 'smithy'}
    for g in ws:
        k = next((k for k in crafts.values() if g in k['out']), None)
        if k:
            wh[site_of.get(g, 'cottages')] += draw[g] / k['out'][g] * k['hours']
    out["country workshops' goods"] = {
        'fte': fte(sum(wh.values())),
        'units': [('village pottery', 2, 'beside the clay pits of the central plains and the coast south of the city', fte(wh['pots'])),
                  ("weavers' and soap boilers' cottages", 3, 'the farm villages (winter work: scarves, soap, bandages)', fte(wh['cottages'])),
                  ('country press and paper mill', 2, 'on the rivers of the central plains, one nearer the city', fte(wh['press'])),
                  ('village smithy and tanyard', 1, 'the grain farms\' village on the plains (iron hoops, nails, cookpots; a little '
                                                    'leather): one, for 1.5 workers\' work', fte(wh['smithy']))],
        'goods': ws}
    for k, v in out.items():
        v['draw'] = {g: draw[g] for g in v['goods'] if draw[g] >= 0.5}
        v.setdefault('plausible', v['fte'])
    return out


def crew_sites(units, workers):
    """Spread an industry's workers over its works: at least one a site, the rest by each kind's share of the work
    (largest remainder). Returns [(kind, count, where, [workers at each site])]; a kind with more sites than it can
    crew loses sites first (the user's rule: collapse into fewer, larger works)."""
    units = [list(u) for u in units]
    while units and sum(u[1] for u in units) > workers:
        u = min((u for u in units if u[1] > 1), key=lambda u: u[3] / u[1], default=None)
        if u is None:
            break
        u[1] -= 1
    tot = sum(u[3] for u in units) or 1
    want = [workers * u[3] / tot for u in units]
    got = [math.floor(w) for w in want]
    for i in sorted(range(len(units)), key=lambda i: -(want[i] - got[i]))[:max(0, workers - sum(got))]:
        got[i] += 1
    for i, u in enumerate(units):               # (At least one a site: from the kinds with most a site.)
        while got[i] < u[1]:
            j = max((j for j in range(len(units)) if got[j] > units[j][1]), key=lambda j: got[j] / units[j][1], default=None)
            if j is None:
                break
            got[j] -= 1
            got[i] += 1
    out = []
    for u, e in zip(units, got):
        n = u[1]
        each = [e // n + (1 if i < e % n else 0) for i in range(n)]
        out.append((u[0], n, u[2], each))
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
        rows.append([g, f'{nourish(g)}', f'{price(g)}p'] + [f"{sc[s]['market'].get(g, {}).get('pieces', 0):,.0f}" for s in SEASONS] +
                    [f'{v:,.0f}', f'{v * nourish(g) / tn * 100:,.0f}%'])
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
        rows.append([g, f'{y["residents"]:,.0f}', f'{y["back"]:,.0f}', f'{y["goods"]:,.0f}',
                     '/'.join(f'{use_d[g][s].total():,.0f}' for s in SEASONS), chain])
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
        rows.append([pg, f'{kg:g} kg', f'{city:,.0f}', f'{year_mean({s: need[pg][s]["residents"] for s in SEASONS}):,.0f}',
                     f'{own:,.0f}', '/'.join(f'{demand[pg][s]:,.0f}' for s in SEASONS),
                     f'{(city + own) * 365 * kg / 1000:.1f} t', ', '.join(GROWN_BY.get(pg, {}).keys())])
    say(table(['Primary good', 'A piece', 'The city draws', 'of it for residents\' food', 'The Country\'s own people eat',
               'All, by season (Sp/Su/Au/Wi)', 'A year (real weight)', 'Grown by'], rows))
    say(f'\nThe Country\'s people: {country_pop:,.0f} (its workers, {country_pop / HOUSEHOLD:,.0f} year-round '
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
        rows.append([name] + [f'{w[s]:.1f}' for s in SEASONS] + [f"{u['staff']}", f"{u['seasonal']}", f"{u['seasonal_days']:,.0f}",
                                                                   u['peak']])
        for s in SEASONS:
            tot[s] += w[s]
        tot['staff'] += u['staff']
        tot['seasonal'] += u['seasonal']
        tot['days'] += u['seasonal_days']
    rows.append(['**food, all**'] + [f'**{tot[s]:,.0f}**' for s in SEASONS] + [f"**{tot['staff']}**", f"**{tot['seasonal']}**",
                                                                             f"**{tot['days']:,.0f}**", ''])
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
        rows.append([name, f"{u['count']}"] + [f"{m['income'][s]:,.0f}/{m['cost'][s]:,.0f}" for s in SEASONS] +
                    [f"{m['income_y']:,.0f}", f"{m['cost_y']:,.0f}", f"{m['net_y']:+,.0f}", f"{m['cover']:.2f}",
                     f"{m['own_y']:,.0f}", f"{m['net_board_y']:+,.0f}", f"{m['cover_board']:.2f}", rv(m['reserve']),
                     f"{m['capital']:,.0f}"])
    ti = sum(m['income_y'] for m in money.values())
    tc = sum(m['cost_y'] for m in money.values())
    to = sum(m['own_y'] for m in money.values())
    rows.append(['**food, all**', ''] + [f"**{sum(m['income'][s] for m in money.values()):,.0f}/{sum(m['cost'][s] for m in money.values()):,.0f}**"
                                         for s in SEASONS] + [f'**{ti:,.0f}**', f'**{tc:,.0f}**', f'**{ti - tc:+,.0f}**', f'**{ti / tc:.2f}**',
                                                              f'**{to:,.0f}**', f'**{ti + to - tc:+,.0f}**', f'**{(ti + to) / tc:.2f}**',
                                                              '**' + rv(sum(m['reserve'] for m in money.values() if m['reserve'] < 1e18)) + '** (the units that pay)',
                                                              f"**{sum(m['capital'] for m in money.values()):,.0f}**"])
    say(table(['Unit', 'Units', 'Spring in/out', 'Summer', 'Autumn', 'Winter', 'From the city (year)', 'Wages and upkeep',
               'Net (cash from the city alone)', 'Cover', "The Country's own people's food", 'Net with it', 'Cover with it',
               'Working money in a steady year', 'Working money to the first harvest (first year)'], rows))
    say('')
    say(f"The Country's people's food bought from the units ({to:,.0f}p a day) against what its workers' households spend on "
        f"it out of their wages ({sum((u['staff'] + year_mean(u['seasonal_s'])) for u in U.values()) * HOUSEHOLD * year_mean(head_p):,.0f}p "
        f"for the food units' people; the rest is the other industries' people's).\n")
    MONEY_TOTALS.update({'from_city': ti, 'cost': tc, 'own': to, 'head_p': year_mean(head_p), 'res_food': res_food})

    # 3b'. The levers, for all the food units and for the ones that fall short.
    toll = year_mean({s: long[s]['toll_now'] for s in SEASONS}) / 100
    cases = [
        ('Drifted wages (16p / 10.5p), cash from the city alone', {}, False),
        ('Drifted wages, and the Country\'s own people buy their food from the units', {}, True),
        ('The default wages (14p / 8p), with the own people\'s food', {'keeper': 14, 'hand': 8}, True),
        ('Drifted wages; harvest and lambing hands are the households\' own (fed, not waged)', {'family_harvest': True}, True),
        (f'Drifted wages; no gate toll on the Country\'s food (it keeps the {toll * 100:,.0f}%)', {'toll_free': toll}, True),
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
        rows.append([label, f'{i:,.0f}p', f'{c:,.0f}p', f'{i - c:+,.0f}p', f'{i / c:.2f}', each])
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
        rows.append([label, f'{i:,.0f}p', f'{c:,.0f}p', f'{i - c:+,.0f}p', f'{i / c:.2f}'])
    say(table(['Combined (with the own people\'s food)', 'Income', 'Wages and upkeep', 'Net', 'Cover'], rows))
    say('')

    # 3c. Per unit, one of each.
    say('### 3c. One unit of each kind (a day, the year\'s mean)\n')
    rows = []
    for name, u in U.items():
        m = money[name]
        n = u['count']
        rows.append([name, f"{u['keepers'] / n:.1f} + {(u['staff'] - u['keepers']) / n:.1f}", f"{u['seasonal'] / n:.1f}",
                     f"{m['income_y'] / n:,.0f}p", f"{m['own_y'] / n:,.0f}p", f"{m['wages_y'] / n:,.0f}p", f"{m['upkeep_y'] / n:.1f}p",
                     f"{m['net_y'] / n:+,.0f}p", f"{m['net_board_y'] / n:+,.0f}p", rv(m['reserve'] / n, 'p'), f"{m['capital'] / n:,.0f}p"])
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
        units_n = sum(n for _, n, _, _ in v['units'])
        wage = v['plausible'] * WAGE['hand_now'] + units_n * (WAGE['keeper_now'] - WAGE['hand_now'])
        v['income'] = inc
        v['wages'] = wage
        rows.append([k, ', '.join(f'{g} {d:,.0f}' for g, d in sorted(v['draw'].items(), key=lambda x: -x[1])),
                     f"{v['fte']:,.0f}", f"{v['plausible']:,.0f}", f"{inc:,.0f}p", f"{wage:,.0f}p", f"{inc / wage:.2f}",
                     '; '.join(f'{n} {u}' for u, n, _, _ in v['units'])])
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
    say(f"Whole Country, a day: from the city {intake:,.0f}p (the food units {ti:,.0f}p, the rest {intake - ti:,.0f}p); "
        f"its people's food bought from its own units {food_spend:,.0f}p (inside the Country: no toll); wages {wages_all:,.0f}p "
        f"(food units {sum(m['wages_y'] for m in money.values()):,.0f}p, the other industries {other_w:,.0f}p), upkeep "
        f"{upkeep_all:,.0f}p, its 91 posts in the city {posts:,.0f}p; the units keep {keep:+,.0f}p. Back to the city: its "
        f"people's wages less their food, its posts and its upkeep, {to_city:,.0f}p, and what the units keep, against the "
        f"{spent:,.0f}p the Country's basket spends now.\n")
    MONEY_TOTALS.update({'intake': intake, 'food_spend': food_spend, 'wages_all': wages_all, 'keep': keep, 'to_city': to_city,
                         'spent_now': spent, 'other_w': other_w, 'other_in': other_in})
    json.dump({'units': U, 'money': money, 'others': others, 'country_pop': country_pop,
               'demand': demand, 'need': {pg: {s: dict(need[pg][s]) for s in SEASONS} for pg in need}},
              open(a.dump, 'w'), indent=1, default=float) if a.dump else None
    if a.units:
        write_units(a.units, U, others)


# ---------------------------------------------------------------------------------------------------------------------
# The switch (doc 71, the user, 2026-11-01): the farms take the food over from the Country. One model of how the game
# pays and feeds a wolf (RatwCityLoop.cpp): a post costs its payer its wage (payWages); the holder buys its food in the
# city's shops out of it (the households' market, NourishADay a head) and spends the rest on needs and comforts, so a
# farm's crew is its wages, and their food comes back to the farms through the shops' draws (drawFromStore: the Country's
# part is the cost less the gate toll; the shop pays carriage on top). The farms' basket (their upkeep: tools, carts,
# barrels, building goods from the city's makers) takes what is over the wages, as the Country's basket takes what is
# over its posts (countryBuys).
FOOD_AREAS_IND = FOOD_INDUSTRIES + ['livestock: wool, hides, tallow, bone']
D_NEAR, D_MID, D_FAR = 'near (within 2 cells of the city)', 'middle (3-8 cells)', 'far (9 cells or more)'
SLACK = 0.10        # yields over what is eaten and drawn, so the shelves don't run dry on a good day
AREAS = [   # The food areas: country.json's season groups, the goods resold with them, salt, and the herds' by-products.
    {'id': 'grain', 'name': 'grain and flour', 'goods': ['flour', 'oats', 'wheat', 'straw']},
    {'id': 'fruit', 'name': 'orchards and vines', 'goods': ['apples', 'berries', 'cider', 'apple_brandy', 'dried_apples',
                                                            'fruit_preserves', 'grapes', 'wine', 'brandy', 'aqua_vitae']},
    {'id': 'meat', 'name': 'meat, milk and the herds', 'goods': ['raw_meat', 'pig', 'mutton', 'rabbit', 'meal', 'sheep', 'offal',
                                                                 'eggs', 'milk', 'butter', 'cheese', 'tallow', 'hide', 'bones',
                                                                 'wool', 'sheepskin', 'small_pelt']},
    {'id': 'fish', 'name': 'fish and shellfish', 'goods': ['fish', 'shellfish']},
    {'id': 'honey', 'name': 'honey', 'goods': ['honey', 'beeswax']},
    {'id': 'salt', 'name': 'salt', 'goods': ['salt']},
]
DROPPED = ['vegetables', 'mushrooms']   # the user, 2026-11-01: "we can drop vegetables entirely. These are wolves after all."
BASKET_SHARE = 0.5  # the farms' basket: about half a farm's takings (its crew the other half; doc 71, 2.6)
# The farms' basket: what farms buy (tools, carts, barrels, rope and nets, building goods), in the shares the Country's
# basket buys them now (country.json), so the trades that make them keep their orders.
SPIRITS = ['aqua_vitae']   # a drink with no nourishment (made from wine)
FARM_GOODS = ['cart_wheel', 'axle', 'barrel', 'iron_hoops', 'sickle', 'farm_gear', 'draw_saw', 'nails', 'fittings', 'lock',
              'chain', 'buckles', 'cookpot', 'leather', 'rope', 'canvas', 'fishing_net', 'planks', 'beam', 'cut_stone',
              'bricks', 'roof_tiles', 'mortar', 'lime', 'whitewash', 'plaster', 'glass_pane']
# The Country's own people who stay in the Country and eat from the farms without buying in the city (no money moves):
# the other industries' workers once placed (section 5) and the Houses' armed retainers (country.json patrons: 24).
ESTATE_PEOPLE = 24
# The granary sells from store at a price a year (the area's mean of the season's prices: grain is most plentiful when it
# is cheapest, so an even price steadies its takings); the rest at the season's, as now: the season's price steadies
# their takings (apples and fish are fewest when they are dearest), and an even one would swing them more.
STORED = ['grain']
FLOAT_DAYS = 2      # a farm's till keeps this many days of its crew's wages before its basket (doc 71, "Reserves")

FARMS = [
    {'kind': 'grain farm', 'area': 'grain', 'count': 2, 'min_hands': 4,
     'sells': {'flour': 1, 'oats': 1, 'wheat': 1, 'straw': 1},
     'where': [('the central plains on the nearer river, east of the city; with a water mill', D_MID),
               ('the central plains on a second river further east; with a water mill', D_MID)],
     'terrain': ['plain', 'meadow'], 'near': ['road', 'river'], 'store': 'a granary and its water mill: the harvest sold from store all year'},
    # Five orchards (the user wants several sources; each pays a sensible crew in every season), two of them with a drying
    # house: those two make the dried apples and the preserves (the Country's winter stores) from their own fruit. All
    # five share the apples, the cider and the berries alike.
    {'kind': 'orchard with a drying house', 'area': 'fruit', 'count': 2, 'min_hands': 2, 'ids': ['orchard_1', 'orchard_3'],
     'sells': {'apples': .4, 'berries': .4, 'cider': .4, 'apple_brandy': .4, 'dried_apples': 1, 'fruit_preserves': 1},
     'where': [('the central hills, a south-facing slope; with a drying house', D_MID),
               ('the sheltered valleys of the coast south of the city (cider country); with a drying house', D_NEAR)],
     'terrain': ['hills', 'meadow'], 'near': ['road'],
     'store': 'a fruit store, a cider press and a drying house: picked late summer to autumn, kept into spring; dried and preserved in autumn for the winter'},
    {'kind': 'orchard', 'area': 'fruit', 'count': 3, 'min_hands': 2, 'ids': ['orchard_2', 'orchard_4', 'orchard_5'],
     'sells': {'apples': .6, 'berries': .6, 'cider': .6, 'apple_brandy': .6},
     'where': [('the central hills, a river valley', D_MID),
               ('the sheltered valleys of the coast south of the city (cider country)', D_NEAR),
               ('a sheltered valley where the central hills meet the plains', D_MID)],
     'terrain': ['hills', 'meadow'], 'near': ['road'], 'store': 'a fruit store and cider press: picked late summer to autumn, kept into spring'},
    {'kind': 'vineyard', 'area': 'fruit', 'count': 1, 'min_hands': 3, 'sells': {'grapes': 1, 'wine': 1, 'brandy': 1, 'aqua_vitae': 1},
     'where': [('the warm south-facing hills at the SE desert\'s edge', D_FAR)],
     'terrain': ['hills'], 'near': ['road'], 'store': 'a press house and cellar: pressed at the harvest, sold by the cask all year'},
    {'kind': 'cattle and dairy farm', 'area': 'meat', 'count': 3, 'min_hands': 3,
     'sells': {'raw_meat': .65, 'meal': .65, 'milk': 1, 'butter': 1, 'cheese': 1, 'hide': .6, 'tallow': .5, 'bones': .5},
     'where': [('the wet coastal pasture just south of the city', D_NEAR), ('the wet coastal pasture just south of the city', D_NEAR),
               ('the meadows east of the city, toward the central hills', D_NEAR)],
     'terrain': ['meadow', 'pasture', 'plain'], 'near': ['city', 'road'], 'store': 'milk daily (it keeps a day: near the city); beef killed through the year'},
    {'kind': 'pig farm', 'area': 'meat', 'count': 3, 'min_hands': 2,
     'sells': {'raw_meat': .35, 'meal': .35, 'pig': 1, 'offal': 1, 'hide': .4, 'tallow': .3, 'bones': .3},
     'where': [('the oak and beech woods of the central hills (pannage)', D_MID)] * 3,
     'terrain': ['forest', 'hills'], 'near': ['road'], 'store': 'fattened on the autumn mast; salt pork carries the winter'},
    {'kind': 'sheep and goat run', 'area': 'meat', 'count': 2, 'min_hands': 2,
     'sells': {'mutton': 1, 'sheep': 1, 'wool': 1, 'sheepskin': 1, 'tallow': .2, 'bones': .2},
     'where': [('the central hills\' downs', D_MID), ('the foothills of the snowy mountains along the north (with the goats)', D_MID)],
     'terrain': ['hills', 'downs', 'mountain'], 'near': ['road'], 'store': 'lambing in spring, shearing in summer, killing in autumn and winter'},
    {'kind': 'rabbit warren', 'area': 'meat', 'count': 2, 'min_hands': 1, 'sells': {'rabbit': 1, 'small_pelt': 1, 'eggs': 1},
     'where': [('the dunes and sandy heath of the SW marsh\'s coast', D_MID), ('the sandy edge of the SE desert', D_FAR)],
     'terrain': ['heath', 'dunes', 'desert'], 'near': ['road'], 'store': 'all year'},
    {'kind': 'sea fishing boat', 'area': 'fish', 'count': 4, 'min_hands': 2, 'sells': {'fish': .5, 'shellfish': 1},
     'where': [('the sea off the city\'s wharf', D_NEAR)] * 2 + [('a fishing cove on the coast south of the city', D_NEAR),
                                                              ('the SW marsh\'s coast', D_MID)],
     'terrain': ['coast'], 'near': ['sea'], 'store': 'the winter runs are the best; shellfish raked from the shore beds; salted and smoked ashore'},
    {'kind': 'lake and river boat', 'area': 'fish', 'count': 2, 'min_hands': 1, 'sells': {'fish': .3},
     'where': [('a lake under the snowy mountains along the north', D_MID), ('the largest river of the central plains', D_MID)],
     'terrain': ['lake', 'river'], 'near': ['lake', 'river'], 'store': 'steady, smaller'},
    {'kind': 'eel and fowl marsh', 'area': 'fish', 'count': 1, 'min_hands': 1, 'sells': {'fish': .2},
     'where': [('the SW marsh (eel traps, reed cutting, wildfowl)', D_MID)],
     'terrain': ['marsh'], 'near': ['water', 'road'], 'store': 'best in the autumn eel run; smoked eels for the winter'},
    {'kind': 'beekeeper', 'area': 'honey', 'count': 3, 'min_hands': 1, 'sells': {'honey': 1, 'beeswax': 1},
     'where': [('among the orchards of the central hills', D_MID), ('among the orchards of the coast south of the city', D_NEAR),
               ('the thyme and heather scrub at the SE desert\'s edge', D_FAR)],
     'terrain': ['hills', 'meadow', 'scrub'], 'near': ['orchard'], 'store': 'taken summer to autumn, kept in crocks for the winter'},
    {'kind': 'salt works', 'area': 'salt', 'count': 1, 'min_hands': 2, 'sells': {'salt': 1},
     'where': [('the SW marsh\'s coast (sea pans: let in, evaporated in summer, boiled over peat the rest of the year)', D_MID)],
     'terrain': ['coast', 'marsh'], 'near': ['sea', 'road'], 'store': 'made all year, most in summer; kept dry in sheds'},
]


def streams(sc, crafts, items):
    """Each season's draws by who they end with: A[s][stream][product][good] = {pieces, country, toll, carriage}. A draw
    ends in its product (the good itself when resold, else the craft's output); the product's buyers share it, by their
    pieces: the residents' food (`food`, nourishment 10 or more), the residents' other buying (`other`: needs, comforts,
    drink), the Country's basket (`country`); a product neither buys (the treasury's works, the patrons, a maker's
    stock) is the city's (`city`)."""
    nourish = lambda g: (items.get(g, {}).get('food') or {}).get('nourish', 0)
    out = {}
    for s in SEASONS:
        a = collections.defaultdict(lambda: collections.defaultdict(lambda: collections.defaultdict(collections.Counter)))
        for g, reasons in sc[s]['draws_why'].items():
            for r, c in reasons.items():
                prod = g if r == 'resale' else (next(iter(crafts[r]['out'])) if r in crafts else r)
                rr = sc[s]['market'].get(prod, {}).get('pieces', 0)
                cc = sc[s]['bought'].get(prod, {}).get('pieces', 0)
                shares = {'city': 1.0} if rr + cc <= 0 else {('food' if nourish(prod) >= 10 else 'other'): rr / (rr + cc),
                                                             'country': cc / (rr + cc)}
                for st, sh in shares.items():
                    if sh > 0:
                        a[st][prod][g].update({k: v * sh for k, v in c.items()})
        out[s] = a
    return out


def vec(d=None):
    return collections.defaultdict(collections.Counter, d or {})


def add(into, frm, k=1.0):
    for g, c in frm.items():
        for kk, vv in c.items():
            into[g][kk] += vv * k
    return into


def coefficients(sc, A, long, items):
    """Per season, the draws (per good: pieces, the Country's part, toll) behind: a resident's food a day (`diet`, a
    head), a penny of the residents' other spending (`other`), today's Country basket (`basket`, all of it), a penny of
    the farms' basket (`farm`), and the rest that the switch leaves as it is (`base`: the residents' and the city's)."""
    nourish = lambda g: (items.get(g, {}).get('food') or {}).get('nourish', 0)
    shares = json.load(open(ROOT / 'Data/Items/country.json'))['markets'][0]['basket']
    out = {}
    for s in SEASONS:
        res = long[s]['residents']
        tot = {st: vec() for st in ('food', 'other', 'country', 'city')}
        for st in tot:
            for prod, gs in A[s][st].items():
                add(tot[st], gs)
        food_coins = sum(c['coins'] for g, c in sc[s]['market'].items() if nourish(g) >= 10)
        services = sum(sum(c['coins'] for c in by.get('services', {}).values()) for by in sc[s]['sales'].values())
        other_coins = sum(c['coins'] for g, c in sc[s]['market'].items() if nourish(g) < 10) + services
        bought = sum(c['coins'] for c in sc[s]['bought'].values())
        fw = {g: shares.get(g, 0) for g in FARM_GOODS}
        tw = sum(fw.values())
        farm = vec()
        for g, w in fw.items():
            spent = sc[s]['bought'].get(g, {}).get('coins', 0)
            if spent > 0:
                add(farm, A[s]['country'].get(g, {}), w / tw / spent)
        # The Country's basket re-weighted (an option, doc 71, 2.6): only the goods the farms' basket doesn't buy and no
        # food or drink made from the farms' goods (the crews buy those in the city now), in its shares now.
        rw = {g: w for g, w in shares.items() if g not in FARM_GOODS and nourish(g) <= 0 and g not in SPIRITS}
        rt = sum(rw.values())
        country2 = vec()
        for g, w in rw.items():
            spent = sc[s]['bought'].get(g, {}).get('coins', 0)
            if spent > 0:
                add(country2, A[s]['country'].get(g, {}), w / rt / spent)
        base = add(add(add(vec(), tot['food']), tot['other']), tot['city'])
        # After the switch nobody sells vegetables (the user: dropped): their draws (the inns' stew) go from every part.
        for v in (base, tot['country'], country2, farm):
            for g in DROPPED:
                v.pop(g, None)
        diet = add(vec(), tot['food'], 1 / res)
        other = add(vec(), tot['other'], 1 / other_coins)
        dropped = sum(x['country'] for g, x in diet.items() if g in DROPPED) * res
        for v in (diet, other):
            for g in DROPPED:
                v.pop(g, None)
        out[s] = {'base': base, 'dropped': dropped, 'basket': tot['country'], 'country2': country2, 'country2_goods': {g: w / rt for g, w in rw.items()},
                  'diet': diet, 'other': other, 'farm': farm, 'food_a_head': food_coins / res, 'services': services,
                  'other_coins': other_coins, 'food_coins': food_coins, 'basket_now': bought, 'residents': res,
                  'carriage_out': sc[s]['carriage_out'] / bought}
    return out


def is_food(g):
    return industry_of(g) in FOOD_AREAS_IND


def season_price(g, s, long, group_of):
    gr = group_of.get(g)
    return long[s].get(f'{gr}_price', 1) or 1 if gr in GROUPS else 1.0


def solve(C, long, group_of, farms, kw, hw, posts, patrons, even=None, reweight=False, FBs=None, farm_basket=True):
    """Season by season, the city's draws after the switch and who gets what. The residents' and the city's draws stay
    (`base`); the Country's basket is what is over its posts and patrons (countryBuys: spare = cash - reserve - owed);
    each farm's basket (the till's spending on it, its carriage out included) is what is over its crew's wages, or the
    cash cycle's (`FBs`: {season: {kind: pennies a day}}, a lean season's shortfall earned back first); the crews buy
    their food at the city's shops (a resident's diet a head) and spend the rest of their wages as the residents spend
    theirs; every basket and every purchase draws from the farms and the Country again, so it is iterated. `even`: the
    farms sell at a price factor a year (per area), not the season's. `farm_basket` False: the farms buy nothing (what
    the toll and the Country would be without their basket)."""
    area_of = {g: ar['id'] for ar in AREAS for g in ar['goods']}
    n = sum(f['count'] * (1 + f['hands']) for f in farms)
    W = {f['kind']: f['count'] * (kw + f['hands'] * hw) for f in farms}
    out = {}
    for s in SEASONS:
        c = C[s]
        co = c['carriage_out']
        b, fb = 0.0, 0.0
        for _ in range(400):
            X = max(0.0, sum(W.values()) - n * c['food_a_head'])
            d = vec()
            parts = {}
            basket = ('country2', b * c['basket_now']) if reweight else ('basket', b)
            for part, k in (('base', 1), basket, ('diet', n), ('other', X), ('farm', fb)):
                add(d, c[part], k)
                parts['basket' if part == 'country2' else part] = {'toll': k * sum(x['toll'] for x in c[part].values()),
                               'food': k * sum(x['country'] for g, x in c[part].items() if is_food(g)),
                               'country': k * sum(x['country'] for g, x in c[part].items() if not is_food(g))}
            rev = {}
            for g, x in d.items():
                if is_food(g):
                    e = (even or {}).get(area_of.get(g, 'meat'))
                    f = e / season_price(g, s, long, group_of) if e else 1.0
                    rev[g] = x['country'] * f
            R = {f['kind']: sum(rev.get(g, 0) * sh for g, sh in f['sells'].items()) for f in farms}
            intake = sum(x['country'] for g, x in d.items() if not is_food(g))
            B = max(0.0, (intake - posts - patrons) / (1 + co))
            if not farm_basket:
                FB = {k: 0.0 for k in R}
            elif FBs:
                FB = dict(FBs[s])
            else:
                FB = {k: max(0.0, R[k] - W[k]) for k in R}
            fb_new = sum(FB.values()) / (1 + co)     # the goods; the rest is the basket's carriage out
            b_new = B / c['basket_now']
            if abs(b_new - b) < 1e-8 and abs(fb_new - fb) < 1e-5:
                break
            b, fb = .5 * b + .5 * b_new, .5 * fb + .5 * fb_new
        out[s] = {'draws': d, 'rev': rev, 'R': R, 'W': W, 'FB': FB, 'fb': fb, 'fb_carriage': sum(FB.values()) - fb,
                  'B': B, 'b': b, 'intake': intake, 'parts': parts, 'reweight': reweight,
                  'margin': intake - posts - patrons, 'X': X, 'n': n,
                  'toll': sum(x['toll'] for x in d.values()),
                  'country_by_ind': {ind: sum(x['country'] for g, x in d.items() if industry_of(g) == ind) for ind in INDUSTRY}}
    return out


MIDDLE = [46, 92 + 46, 184 + 45.5, 275 + 45]   # Society::seasonal (RatwCityLoop.cpp): each season's value at its middle day


def blended(m, day):
    """Society::seasonal: the value on a day of the year from the four middle-day values, a straight line between two
    middles, Winter's running on to Spring's across the year's turn."""
    a = 3
    for i in range(4):
        if day >= MIDDLE[i]:
            a = i
    b = (a + 1) % 4
    frm, to = MIDDLE[a], MIDDLE[b] + (365 if b == 0 else 0)
    at = day + 365 if day < frm else day
    t = min(1.0, max(0.0, (at - frm) / (to - frm)))
    return m[a] + (m[b] - m[a]) * t


def season_means_of(m):
    """The season means (each day at its middle) of a blended curve."""
    out, start = [], 0
    for s in SEASONS:
        out.append(sum(blended(m, d + .5) for d in range(start, start + SEASON_DAYS[s])) / SEASON_DAYS[s])
        start += SEASON_DAYS[s]
    return out


def middles(means):
    """The four middle-day values whose blend (Society::seasonal) has these season means: the blend is linear in them, so
    a 4 x 4 system. A value that would come out under nought is held at nought and the rest solved again (the season
    means then match as nearly as they can, reported by the caller)."""
    cols = [season_means_of([1.0 if j == i else 0.0 for j in range(4)]) for i in range(4)]   # column i: unit at middle i
    M = [[cols[j][i] for j in range(4)] for i in range(4)]
    fixed = set()
    for _ in range(4):
        free = [j for j in range(4) if j not in fixed]
        # Least squares on the free values (normal equations), the fixed ones at nought.
        A = [[sum(M[r][i] * M[r][j] for r in range(4)) for j in free] for i in free]
        y = [sum(M[r][i] * means[r] for r in range(4)) for i in free]
        n = len(free)
        for c in range(n):
            p = max(range(c, n), key=lambda r: abs(A[r][c]))
            A[c], A[p], y[c], y[p] = A[p], A[c], y[p], y[c]
            for r in range(n):
                if r != c and A[c][c]:
                    f = A[r][c] / A[c][c]
                    A[r] = [x - f * z for x, z in zip(A[r], A[c])]
                    y[r] -= f * y[c]
        sol = {j: y[k] / A[k][k] for k, j in enumerate(free)}
        m = [sol.get(j, 0.0) for j in range(4)]
        neg = [j for j in free if m[j] < 0]
        if not neg:
            return m
        fixed.add(min(neg, key=lambda j: m[j]))
    return [max(0.0, x) for x in m]


def drawdown(net):
    """The deepest fall of a running balance over a year from any day (net: a day's net by season)."""
    days = [s for s in SEASONS for _ in range(SEASON_DAYS[s])]
    run, high, worst = 0.0, 0.0, 0.0
    for x in [net[s] for s in days] * 2:
        run += x
        high = max(high, run)
        worst = max(worst, high - run)
    return worst


def cash_cycle(net, float_):
    """A farm's till over a steady year (doc 71, "Reserves"): its basket takes what the till holds over its reserve, the
    reserve being its float and the deepest shortfall of its lean weeks (the drawdown of its takings less its wages); a
    lean season draws the till down toward the float, and the next seasons' surplus refills it before the basket resumes.
    So the basket over the year is the takings less the wages, and a lean season's shortfall is charged to the others.
    Returns the basket a day by season, the reserve, and whether the till holds (a year's takings cover the wages)."""
    short = drawdown(net)
    reserve = float_ + short
    days = [s for s in SEASONS for _ in range(SEASON_DAYS[s])]
    till = reserve
    for _ in range(3):
        acc = {s: 0.0 for s in SEASONS}
        for s in days:
            x = net[s]
            if till + x > reserve:
                acc[s] += till + x - reserve
                till = reserve
            else:
                till += x
    return {s: acc[s] / SEASON_DAYS[s] for s in SEASONS}, reserve, year_mean(net) >= 0


def configure(C, long, group_of, kw, hw, posts, patrons, farms_spec=None):
    """The recommended configuration: each farm's crew is what about half its takings pay at the city's wages (the
    other half its basket), never under its kind's smallest sensible crew; iterated, since the crews' own food and
    spending come back as takings. Then the farms sell at an even price a year (their stores), so the solve is redone
    with each area's price factor a year (the year's takings the same as at the season's prices), and each farm's basket
    follows its till's cash cycle (a lean season charged to the others)."""
    farms = [dict(f, hands=f['min_hands']) for f in (farms_spec or FARMS)]
    for _ in range(30):
        res = solve(C, long, group_of, farms, kw, hw, posts, patrons)
        Rbar = {f['kind']: year_mean({s: res[s]['R'][f['kind']] for s in SEASONS}) for f in farms}
        changed = False
        for f in farms:
            h = max(f['min_hands'], math.floor(((1 - BASKET_SHARE) * Rbar[f['kind']] / f['count'] - kw) / hw + .5))
            changed, f['hands'] = changed or h != f['hands'], h
        if not changed:
            break
    # Each area's price factor a year: its takings at the season's prices over its takings at none.
    area_of = {g: ar['id'] for ar in AREAS for g in ar['goods']}
    at, flat = collections.Counter(), collections.Counter()
    for s in SEASONS:
        for g, v in res[s]['rev'].items():
            at[area_of.get(g, 'meat')] += v * SEASON_DAYS[s]
            flat[area_of.get(g, 'meat')] += v / season_price(g, s, long, group_of) * SEASON_DAYS[s]
    even = {a: (at[a] / flat[a] if flat[a] else 1.0) if a in STORED else None for a in at}
    for ar in AREAS:
        even.setdefault(ar['id'], None)
    # The cash cycle: each farm's basket by season from its till, iterated with the draws it makes.
    FBs, cycle = None, {}
    for _ in range(60):
        res_even = solve(C, long, group_of, farms, kw, hw, posts, patrons, even=even, FBs=FBs)
        new = {s: {} for s in SEASONS}
        for f in farms:
            k = f['kind']
            W = res_even['spring']['W'][k]
            net = {s: res_even[s]['R'][k] - W for s in SEASONS}
            per, reserve, holds = cash_cycle(net, FLOAT_DAYS * W)
            cycle[k] = {'net': net, 'reserve': reserve, 'holds': holds, 'basket': per}
            for s in SEASONS:
                new[s][k] = per[s]
        if FBs and max(abs(new[s][k] - FBs[s][k]) for s in SEASONS for k in new[s]) < 1e-6:
            break
        FBs = new
    return farms, res, res_even, even, FBs, cycle


def wages_only(C, long, group_of, farms, kw, hw, posts, patrons, even=None):
    """What the food areas could pay if every penny of their takings went on wages (doc 71, the lead table): each kind's
    hands (a fraction allowed) set so that its year's takings equal its wages, no basket; solved with the draws, since the
    extra crews' own food and spending come back as takings (as `configure` solves the half-and-half split). Returns
    each kind's wolves (keepers and hands), the solve, and the farms at that crew."""
    fs = [dict(f) for f in farms]
    for _ in range(200):
        r = solve(C, long, group_of, fs, kw, hw, posts, patrons, even=even, farm_basket=False)
        moved = 0.0
        for f in fs:
            R = year_mean({s: r[s]['R'][f['kind']] for s in SEASONS})
            h = (R / f['count'] - kw) / hw
            moved = max(moved, abs(h - f['hands']))
            f['hands'] = h
        if moved < 1e-6:
            break
    r = solve(C, long, group_of, fs, kw, hw, posts, patrons, even=even, farm_basket=False)
    return {f['kind']: f['count'] * (1 + f['hands']) for f in fs}, r, fs


def order_book(sc, C, res, farms):
    """Each trade's sales a day by buyer, now and after the switch (year means). After: the residents', the services',
    the patrons' and the city's as now; the Country's basket at its new size; the farms' basket, spread over the trades
    as the Country buys each of its goods now; the crews' food as a resident's (a head) and their other spending as the
    residents' (a penny)."""
    shares = json.load(open(ROOT / 'Data/Items/country.json'))['markets'][0]['basket']
    fw = {g: shares.get(g, 0) for g in FARM_GOODS}
    tw = sum(fw.values())
    nourish_ok = lambda g: g in FOOD_GOODS
    now = collections.defaultdict(collections.Counter)
    after = collections.defaultdict(collections.Counter)
    for s in SEASONS:
        w = SEASON_DAYS[s] / 365
        S, c, r = sc[s]['sales'], C[s], res[s]
        sellers = collections.defaultdict(dict)
        for b, by in S.items():
            for g, x in by.get('country', {}).items():
                sellers[g][b] = x['coins']
        for b, by in S.items():
            for buyer, goods in by.items():
                coins = sum(x['coins'] for x in goods.values())
                now[b][buyer] += coins * w
                if buyer != 'country':
                    after[b][buyer] += coins * w
            if not r['reweight']:
                after[b]['country'] += sum(x['coins'] for x in by.get('country', {}).values()) * r['b'] * w
            food = sum(x['coins'] for g, x in by.get('residents', {}).items() if nourish_ok(g))
            other = sum(x['coins'] for g, x in by.get('residents', {}).items() if not nourish_ok(g)) + \
                sum(x['coins'] for x in by.get('services', {}).values())
            after[b]['crews: food'] += food / c['residents'] * r['n'] * w
            after[b]['crews: other'] += other / c['other_coins'] * r['X'] * w
        for g, wt in fw.items():
            tot = sum(sellers[g].values())
            for b, v in sellers[g].items():
                after[b]["farms' basket"] += r['fb'] * wt / tw * v / tot * w
        if r['reweight']:
            for g, wt in c['country2_goods'].items():
                tot = sum(sellers[g].values())
                for b, v in sellers[g].items():
                    after[b]['country'] += r['B'] * wt * v / tot * w
    return now, after



FOOD_GOODS = set()      # the goods of nourishment 10 or more (a meal, not a drink): filled from items.json by the report
STATE = {}              # what the report found, for the placement list and the pool
# The order book's trades by the doc's names, and the small service trades in one row.
BIZ_NAME = {'armory': 'armoury', 'jeweler': 'jeweller', 'general': 'general goods', 'papermill': 'paper mill',
            'charcoal': 'charcoal burners', 'builders_works': "builders' works", 'printing_house': 'printing house',
            'rag_and_bone': 'rag and bone', 'builders_yard': "builders' yard"}
SMALL_TRADES = ['tailor', 'perfumer', 'pawnbroker', 'barber', 'moneychanger', 'stables', 'builders_yard']
SMALL_ROW = ("the services and small trades (tailor, perfumer, pawnbroker, barber, moneychanger, stables, builders' "
             "yard)")


def p0(x):
    return f'{x:,.0f}p'


def pb(x):
    """A cell's pennies, blank when nought (the order book)."""
    return '' if abs(x) < .5 else f'{x:,.0f}p'


def needs_a_day(members, items, crafts_households):
    """What a household of `members` grown wolves spends a day on its needs (crafts.json `households`) at the
    catalogue's prices: each need every `everyDays` (in winter `winterDays`), one of its `any` (the mean price), for
    each member if `perPerson`."""
    out = 0.0
    for need in crafts_households['needs']:
        price = sum(items.get(g, {}).get('price', 0) for g in need['any']) / len(need['any'])
        every = {s: need.get('winterDays') if s == 'winter' and need.get('winterDays') else need['everyDays'] for s in SEASONS}
        out += year_mean({s: price / every[s] for s in SEASONS}) * (members if need.get('perPerson') else 1)
    return out


def money_by(measured):
    """Each business's money a day (the short runs, the year's mean): wages, profit, the treasury's wage support to its
    posts, its draws' cost (the Country's part, the toll, carriage), its city inputs and its services' city share, its
    sales to every buyer, its posts."""
    out = collections.defaultdict(collections.Counter)
    for s in SEASONS:
        w = SEASON_DAYS[s] / 365
        m = measured[s]
        for b, c in m.get('money_by', {}).items():
            for k, v in c.items():
                out[b][k] += v * w
        for b, by in m['drawn_by'].items():
            out[b]['draws'] += sum(x.get('country', 0) + x.get('toll', 0) for x in by.values()) * w
        for b, by in m['sales'].items():
            out[b]['sales'] += sum(x['coins'] for goods in by.values() for x in goods.values()) * w
    return out


def switch_report(a, sc, long, items, crafts, producers, goods_s, ind_s, group_of, say, measured=None, trades=None):
    """Doc 71's lead table, 2.2-2.6 and 3: the farms' crews, takings, baskets and yields after the switch, the Country
    after it, the toll and the treasury, the makers' order book, the money the switch holds, the reserves."""
    FOOD_GOODS.update(g for g, i in items.items() if ((i.get('food') or {}).get('nourish', 0)) >= 10)
    nourish = lambda g: (items.get(g, {}).get('food') or {}).get('nourish', 0)
    A = streams(sc, crafts, items)
    C = coefficients(sc, A, long, items)
    ym = lambda f: year_mean({s: f(s) for s in SEASONS})
    country = json.load(open(ROOT / 'Data/Items/country.json'))
    hh = json.load(open(ROOT / 'Data/Items/crafts.json'))['households']
    # A farm's posts are paid whole pennies: the city tills' drifted wages over the settled years, rounded (a setting,
    # fixed: a farm's basket, not a keeper's profit, takes what is over its wages, so the wage drift would never move it).
    kw_mean, hw_mean = ym(lambda s: long[s]['wage_keeper']), ym(lambda s: long[s]['wage_hand'])
    kw, hw = float(round(kw_mean)), float(round(hw_mean))
    posts = ym(lambda s: sc[s]['country_wages'].get('coins', 0))
    payees = ym(lambda s: sc[s]['country_wages'].get('payees', 0))
    patrons = ym(lambda s: long[s]['patrons_country'])
    food_head = ym(lambda s: C[s]['food_a_head'])
    part = lambda key, food: ym(lambda s: sum(x['country'] for g, x in C[s][key].items() if is_food(g) == food))
    diet_farm, diet_country = part('diet', True), part('diet', False)
    other_farm, other_country = part('other', True), part('other', False)
    farms, res_season, res, even, FBs, cycle = configure(C, long, group_of, kw, hw, posts, patrons)
    can_pay, res_wages, farms_wages = wages_only(C, long, group_of, farms, kw, hw, posts, patrons, even=even)
    n = res['spring']['n']
    nf = sum(f['count'] for f in farms)
    W = sum(res['spring']['W'].values())
    Rk = lambda k, r=res: ym(lambda s: r[s]['R'][k])
    R_now = {f['kind']: ym(lambda s: sum(goods_s.get(g, {}).get(s, {}).get('country', 0) * sh for g, sh in f['sells'].items()))
             for f in farms}
    Yr = lambda key, r=res: ym(lambda s: r[s][key])
    say(f"A wolf's cost to its farm is its wage (payWages: the holder buys its own food out of it): a keeper {kw:,.0f}p, a hand "
        f"{hw:,.0f}p (the city tills' drifted wages over the settled years, {kw_mean:.1f}p and {hw_mean:.1f}p, in whole pennies). Its food at the city's shops costs it {food_head:.2f}p "
        f"a day (NourishADay, {NOURISH_A_DAY:.1f}, of the residents' diet) and comes back to the farms through the shops' "
        f"draws: {diet_farm:.2f}p a wolf (the Country's part of the draw, after the gate toll; the shop pays the carriage on "
        f"top), and {diet_country:.2f}p to the Country (the bakers' and smokehouses' firewood). The rest of its wage it "
        f"spends as the residents spend theirs: a penny of it draws {other_farm:.2f}p from the farms (candles' tallow, "
        f"cider's apples, scarves' wool) and {other_country:.2f}p from the Country.\n")

    # ---- The other industries after the switch (the Country's), their works crewed, and the people the yields feed.
    others = other_industries(sc, crafts, producers)
    for k, v in others.items():
        v['income'] = ym(lambda s: res[s]['country_by_ind'].get(k, 0))
        v['income_now'] = ym(lambda s: ind_s[k][s]['country']) if k in ind_s else 0
        units_n = sum(u[1] for u in v['units'])     # (A keeper a works and hands: what its takings could pay.)
        v['can_pay'] = units_n + (v['income'] - units_n * kw) / hw if units_n else 0.0
        v['workers'] = max(0, min(round(v['plausible']), math.floor(v['can_pay']))) if v['units'] else 0
        v['sites'] = crew_sites(v['units'], v['workers']) if v['workers'] else []
    other_workers = sum(v['workers'] for v in others.values())
    table_people = other_workers + ESTATE_PEOPLE

    # ---- Yields: the farm's share of the city's draws after the switch (the crews' food among them), of what the
    # Country's own people eat (the residents' diet a head), and the slack; season means, and the middle-day values whose
    # blend (Society::seasonal) has those means.
    BY = ('tallow', 'hide', 'bones', 'wool', 'sheepskin', 'small_pelt')
    for f in farms:
        f['yield'], f['yield_mid'], f['yield_fit'] = {}, {}, {}
        for g, sh in f['sells'].items():
            if g in DROPPED:
                continue
            per = [(res[s]['draws'].get(g, {}).get('pieces', 0) + table_people * C[s]['diet'].get(g, {}).get('pieces', 0))
                   * sh * (1 + SLACK) / f['count'] for s in SEASONS]
            if max(per) >= 0.05:
                f['yield'][g] = per
                f['yield_mid'][g] = middles(per)
                f['yield_fit'][g] = season_means_of(f['yield_mid'][g])
        main = [g for g in f['yield'] if g not in BY]
        f['yield_s'] = [sum(f['yield'][g][i] for g in main) for i in range(4)]
        f['revenue'] = Rk(f['kind'])
        f['wages'] = res['spring']['W'][f['kind']]
        f['can_pay'] = can_pay[f['kind']]       # (Every penny on wages, the crews' food and spending coming back.)
        cy = cycle[f['kind']]
        net = cy['net']
        f['reserve'] = cy['reserve']
        f['float'] = FLOAT_DAYS * f['wages']
        fbar = f['revenue'] - f['wages']
        f['reserve_fixed'] = drawdown({s: res_season[s]['R'][f['kind']] - f['wages'] - fbar for s in SEASONS})
        f['reserve_fixed_stored'] = drawdown({s: net[s] - fbar for s in SEASONS})
        f['basket'] = ym(lambda s: res[s]['FB'][f['kind']])            # the till's spending on it, its carriage out too
        f['basket_s'] = [res[s]['FB'][f['kind']] for s in SEASONS]
        f['lean'] = [s for s in SEASONS if net[s] < 0]
        # The user's rule, applied to every kind alike: a farm must pay its smallest sensible crew in every season.
        sensible = kw + f['min_hands'] * hw
        f['leanest'] = min(res[s]['R'][f['kind']] for s in SEASONS) / f['count']
        f['sensible'] = sensible
        f['one_more'] = min(res[s]['R'][f['kind']] for s in SEASONS) / (f['count'] + 1) >= sensible

    # ---- The lead table.
    area_name = {ar['id']: ar['name'] for ar in AREAS}
    say('### The lead table: what each food area pays for, its farms, their crews and yields (a day, settled years)\n')
    rows, last = [], None
    for f in farms:
        rows.append([area_name[f['area']] if f['area'] != last else '', f['kind'], f"{f['revenue']:,.0f}p",
                     f"{f['can_pay']:,.0f}", f"{f['count']}", f"1 + {f['hands']}",
                     ' / '.join(f'{v:,.0f}' for v in f['yield_s']),
                     '; '.join(dict.fromkeys(w for w, _ in f['where'])) +
                     f" ({', '.join(dict.fromkeys(d.split(' ')[0] for _, d in f['where']))})"])
        last = f['area']
    say(table(['Area', 'Farm', 'Revenue a day, all its farms', 'Wolves it can pay (wages alone, solved)', 'Farms', 'Crew a farm (keeper + hands)',
               'Yield a farm a day, Sp / Su / Au / Wi (pieces)', 'Region (distance from the city)'], rows))
    tot_r = sum(f['revenue'] for f in farms)
    fb_till = sum(f['basket'] for f in farms)
    fb_goods, fb_carr = Yr('fb'), Yr('fb_carriage')
    w_tot = ym(lambda s: sum(res_wages[s]['R'].values()))
    say(f"\nIn all: {tot_r:,.0f}p a day of takings. On wages alone (every penny of each kind's takings on its crews, solved "
        f"with the draws: the extra crews' food and spending come back as takings) they would pay "
        f"{sum(f['can_pay'] for f in farms):,.0f} wolves, their takings and wages then {w_tot:,.0f}p a day; at today's "
        f"takings with {n} crew, {nf + (tot_r - nf * kw) / hw:,.0f}. "
        f"{nf} farms with crews of {n} ({nf} keepers, {n - nf} hands) costing {W:,.0f}p a day in wages; their basket "
        f"{fb_till:,.0f}p a day ({fb_goods:,.0f}p of the city's goods and {fb_carr:,.0f}p of carriage out). The yields feed the city (its draws after the switch, the crews' food "
        f"among them), the Country's own {table_people} ({other_workers} of the other industries, {ESTATE_PEOPLE} of the "
        f"Houses' estates) and {SLACK * 100:,.0f}% slack.\n")

    # ---- Takings by farm: now and after the switch, season by season.
    say("### The farms' takings: now (the Country's, at the season's own price) and after the switch (a day)\n")
    rows = []
    for f in farms:
        k = f['kind']
        rows.append([k, f"{R_now[k]:,.0f}p"] + [f"{res[s]['R'][k]:,.0f}p" for s in SEASONS] +
                    [f"**{f['revenue']:,.0f}p**", f"{f['wages']:,.0f}p ({f['wages'] / f['revenue'] * 100:,.0f}%)",
                     f"{f['basket']:,.0f}p", ', '.join(f['lean']) or '-',
                     f"{f['leanest']:,.0f}p / {f['sensible']:,.0f}p" + (' (one more would too)' if f['one_more'] else '')])
    rows.append(['**all**', f"**{sum(R_now.values()):,.0f}p**"] + [f"**{sum(res[s]['R'].values()):,.0f}p**" for s in SEASONS] +
                [f"**{tot_r:,.0f}p**", f"**{W:,.0f}p**", f"**{fb_till:,.0f}p**", '', ''])
    say(table(['Farm', 'Now (year)', 'After: Spring', 'Summer', 'Autumn', 'Winter', 'Year', "Crew's wages (of takings)",
               "The farms' basket (its carriage out in it)", 'Seasons under the wages',
               "A farm's leanest season / its smallest crew's wages"], rows))
    say(f"\n(The stored harvests, {', '.join(STORED)}, sell from store at a price a year: " +
        ', '.join(f'{k} {v * 100:.1f}%' for k, v in even.items() if v) + " of the catalogue; the rest at the season's. "
        f"The farms' basket: {fb_till:.1f}p a day of the tills' spending = {fb_goods:.1f}p of goods + {fb_carr:.1f}p of carriage "
        f"out; takings less wages {tot_r - W:.1f}p.)\n")
    # Where the takings come from after the switch.
    src = {p: ym(lambda s: res[s]['parts'][p]['food']) for p in ('base', 'basket', 'diet', 'other', 'farm')}
    src_now = ym(lambda s: sum(x['country'] for st in ('food', 'other', 'city', 'country') for prod in C[s] and A[s][st].values()
                               for g, x in prod.items() if is_food(g)))
    veg_now = ym(lambda s: sum(goods_s.get(g, {}).get(s, {}).get('country', 0) for g in DROPPED))
    lost = ym(lambda s: C[s]['basket'] and sum(x['country'] for g, x in C[s]['basket'].items() if is_food(g))) - src['basket']
    say(f"Where the food's takings come from (a day): now {src_now:,.0f}p (vegetables {veg_now:.1f}p of it), of it {src['base']:,.0f}p the residents' and the "
        f"city's, {src['basket'] + lost:,.0f}p behind the Country's buy-back. After: the residents' {src['base']:,.0f}p; the "
        f"Country's smaller basket {src['basket']:,.0f}p ({lost:,.0f}p lost); the crews' food {src['diet']:,.0f}p and their "
        f"other spending {src['other']:,.0f}p; the farms' basket {src['farm']:,.0f}p (the hides in its leather); in all "
        f"{sum(src.values()):,.0f}p, the farms' takings.\n")
    lost_by = {f['kind']: ym(lambda s: sum(x['country'] * f['sells'].get(g, 0) for g, x in C[s]['basket'].items() if is_food(g)) *
                             (1 - res[s]['b'])) for f in farms}
    say("The buy-back's draws lost, by farm: " + ', '.join(f'{k} {v:,.0f}p' for k, v in sorted(lost_by.items(), key=lambda x: -x[1]) if v >= .5) + '.\n')
    STATE.update({'src': src, 'src_now': src_now, 'lost': lost, 'veg_now': veg_now, 'lost_by': lost_by})

    # ---- Reserves.
    say("### Reserves: what each farm's till must hold (pennies; days of its crew's wages)\n")
    rows = []
    for f in farms:
        rows.append([f['kind'], f"{f['count']}", f"{f['reserve_fixed']:,.0f}p ({f['reserve_fixed'] / f['wages']:,.0f})",
                     f"{f['reserve_fixed_stored']:,.0f}p ({f['reserve_fixed_stored'] / f['wages']:,.0f})",
                     f"{f['reserve']:,.0f}p ({f['reserve'] / f['wages']:,.0f})", f"{f['reserve'] / f['count']:,.0f}p"])
    rf = sum(f['reserve_fixed'] for f in farms)
    rfs = sum(f['reserve_fixed_stored'] for f in farms)
    rr = sum(f['reserve'] for f in farms)
    rows.append(['**all**', f'**{nf}**', f'**{rf:,.0f}p**', f'**{rfs:,.0f}p**', f'**{rr:,.0f}p**', ''])
    say(table(['Farm', 'Farms', "A fixed basket, the season's prices (days)", 'A fixed basket, the stores at a price a year',
               f"The basket takes what is over the wages: {FLOAT_DAYS} days' float, and any lean season's shortfall on top (recommended)", 'A farm'], rows))
    money = ym(lambda s: long[s]['money'])
    say(f"\nThe realm's money {money:,.0f}p; the Country's purse {ym(lambda s: long[s]['country']):,.0f}p and the treasury "
        f"{ym(lambda s: long[s]['treasury']):,.0f}p on average; the households' purses {ym(lambda s: long[s]['purses']):,.0f}p "
        f"({ym(lambda s: long[s]['purses'] / long[s]['residents']):.1f}p a head), the tills {ym(lambda s: long[s]['tills']):,.0f}p.\n")
    STATE.update({'reserve_fixed': rf, 'reserve_fixed_stored': rfs, 'reserve': rr, 'money': money})

    # ---- The Country before and after.
    co_now = ym(lambda s: sc[s]['carriage_out'])
    b_now = ym(lambda s: C[s]['basket_now'])
    intake_now = ym(lambda s: sum(ind_s[i][s]['country'] for i in ind_s))
    nonfood_now = ym(lambda s: sum(ind_s[i][s]['country'] for i in ind_s if i not in FOOD_AREAS_IND))
    B = Yr('B')
    say("### The Country before and after the switch (a day)\n")
    rows = [
        ["Its takings: the food areas' goods", p0(intake_now - nonfood_now), '- (the farms\')'],
        ["Its takings: its other goods (woods, mines, stone, fibre, herbs, workshops)", p0(nonfood_now), p0(Yr('intake'))],
        [f"Its {payees:,.0f} posts (the Houses' people and the wharf's)", p0(posts), p0(posts)],
        ["Its patrons (the Houses' retainers' kit and halls, the wharf's hulls)", p0(patrons), p0(patrons)],
        ["Carriage out", p0(co_now), p0(B * ym(lambda s: C[s]['carriage_out']))],
        ["Its basket of the city's goods", p0(b_now), f"{p0(B)} ({B / b_now * 100:,.0f}%)"],
        ["of the basket, food and drink made from the farms' goods (cider, salt and smoked fish, wine, brandy)",
         p0(ym(lambda s: sum(c['coins'] for g, c in sc[s]['bought'].items() if nourish(g) > 0))),
         p0(ym(lambda s: sum(c['coins'] for g, c in sc[s]['bought'].items() if nourish(g) > 0) * res[s]['b']))],
    ]
    say(table(['', 'Now', 'After'], rows))
    say('\nIts takings over its posts and patrons, by season: ' + ', '.join(f"{s} {res[s]['margin']:+,.0f}p" for s in SEASONS) + '.\n')
    STATE.update({'B': B, 'b_now': b_now, 'intake_now': intake_now, 'nonfood_now': nonfood_now, 'co_now': co_now})
    rows = []
    for k, v in others.items():
        rows.append([k, f"{v['income_now']:,.0f}p", f"{v['income']:,.0f}p"])
    say(table(["The Country's takings by industry", 'Now', 'After'], rows))
    say('')

    # ---- The toll, and without the farms' basket (solved again: the Country's basket then has nothing to stand on).
    tparts = {p: ym(lambda s: res[s]['parts'][p]['toll']) for p in ('base', 'basket', 'diet', 'other', 'farm')}
    toll_now = ym(lambda s: long[s]['toll'])
    toll_after = sum(tparts.values())
    basket_toll_now = ym(lambda s: sum(x['toll'] for x in C[s]['basket'].values()))
    # Without the farms' basket the money must still go round: the same pennies on wages instead (the lead table's
    # wages-only crews, whose food and spending draw again). The farms keeping their takings over their wages (no basket,
    # the 105 crews) is not a steady state: about a thousand pennies a day would pile up in their tills.
    res_nofb = solve(C, long, group_of, farms, kw, hw, posts, patrons, even=even, farm_basket=False)
    toll_w = Yr('toll', res_wages)
    say(f"The gate toll (the treasury, a day): now {toll_now:,.0f}p ({tparts['base']:,.0f}p on the residents' and the city's "
        f"draws, {basket_toll_now:,.0f}p behind the Country's basket). After: {toll_after:,.0f}p "
        f"({(toll_after - toll_now) / toll_now * 100:+.1f}%): the residents' {tparts['base']:,.0f}p, the Country's basket "
        f"{tparts['basket']:,.0f}p, the crews' food {tparts['diet']:,.0f}p and other spending {tparts['other']:,.0f}p, the farms' "
        f"basket {tparts['farm']:,.0f}p. The Country's takings over its posts and patrons, by season: "
        + ', '.join(f"{s} {res[s]['margin']:+,.0f}p" for s in SEASONS) + ".\n")
    say(f"Without the farms' basket, every penny on wages instead ({sum(can_pay.values()):,.0f} crew, solved): the toll "
        f"{toll_w:,.0f}p ({(toll_w - toll_now) / toll_now * 100:+,.0f}%); the Country takes in {Yr('intake', res_wages):,.0f}p "
        f"against {posts + patrons:,.0f}p of posts and patrons (by season: "
        + ', '.join(f"{s} {res_wages[s]['margin']:+,.0f}p" for s in SEASONS) + f"), its basket {Yr('B', res_wages):,.0f}p. "
        f"(If the 105 crews' farms kept what is over their wages, {ym(lambda s: sum(res_nofb[s]['R'].values())) - W:,.0f}p a "
        f"day would pile up in their tills, out of circulation: not a steady state. Its toll would be {Yr('toll', res_nofb):,.0f}p "
        f"and the Country's intake {Yr('intake', res_nofb):,.0f}p.)\n")
    STATE.update({'tparts': tparts, 'toll_now': toll_now, 'toll_after': toll_after, 'basket_toll_now': basket_toll_now,
                  'toll_w': toll_w, 'intake_w': Yr('intake', res_wages), 'B_w': Yr('B', res_wages),
                  'margin_w': {s: res_wages[s]['margin'] for s in SEASONS}, 'can_pay': can_pay})

    # ---- The makers' order book (and with the Country's smaller basket re-weighted to the goods farms don't buy).
    now, after = order_book(sc, C, res, farms)
    res_rw = solve(C, long, group_of, farms, kw, hw, posts, patrons, even=even, reweight=True, FBs=FBs)
    _, after_rw = order_book(sc, C, res_rw, farms)
    say("### The makers' order book, by trade (sales a day; the residents' own buying, the same before and after, left out)\n")
    rows = []
    FB_ = "farms' basket"
    keys = ('country', FB_, 'crews: food', 'crews: other')
    tot, tot_rw = collections.Counter(), collections.Counter()
    change_of = {}
    grouped = collections.OrderedDict()
    for b in now:
        grouped.setdefault(SMALL_ROW if b in SMALL_TRADES else BIZ_NAME.get(b, b), []).append(b)
    gsum = lambda d, bs, k: sum(d[b][k] for b in bs)
    for name in sorted(grouped, key=lambda nm: -gsum(now, grouped[nm], 'country')):
        bs = grouped[name]
        sales_now = sum(sum(now[b].values()) for b in bs)
        cn = gsum(now, bs, 'country')
        d = {k: gsum(after, bs, k) for k in keys}
        change = sum(d.values()) - cn
        change_rw = sum(gsum(after_rw, bs, k) for k in keys) - cn
        for b in bs:
            change_of[b] = sum(after[b][k] for k in keys) - now[b]['country']
        for k in keys:
            tot[k] += d[k]
            tot_rw[k] += gsum(after_rw, bs, k)
        tot['now'] += cn
        tot['sales'] += sales_now
        if cn < 5 and abs(change) < 5:
            continue
        rows.append([name, f"{sales_now:,.0f}p", pb(cn), pb(d['country']), pb(d[FB_]), pb(d['crews: food']),
                     pb(d['crews: other']), f"{change:+,.0f}p ({change / sales_now * 100:+,.0f}%)",
                     pb(gsum(after_rw, bs, 'country')), f"{change_rw:+,.0f}p ({change_rw / sales_now * 100:+,.0f}%)"])
    ch = sum(tot[k] for k in keys) - tot['now']
    ch_rw = sum(tot_rw[k] for k in keys) - tot['now']
    rows.append(['**all**', f"**{tot['sales']:,.0f}p**", f"**{tot['now']:,.0f}p**", f"**{tot['country']:,.0f}p**",
                 f"**{tot[FB_]:,.0f}p**", f"**{tot['crews: food']:,.0f}p**", f"**{tot['crews: other']:,.0f}p**",
                 f"**{ch:+,.0f}p**", f"**{tot_rw['country']:,.0f}p**", f"**{ch_rw:+,.0f}p**"])
    say(table(['Trade', 'Its sales now (all buyers)', "The Country's basket now", "After: the Country's basket",
               "The farms' basket", "The crews' food", "The crews' other spending", 'Change (of its sales)',
               "Or with the Country's basket re-weighted: its orders", 'Change'], rows))
    say(f"\n(Re-weighted: the Country's basket buys only the goods the farms' basket doesn't and no food or drink, in its "
        f"shares now; it is then {Yr('B', res_rw):,.0f}p a day, the toll {Yr('toll', res_rw):,.0f}p.)\n")
    STATE.update({'now': now, 'after': after, 'after_rw': after_rw, 'book_tot': tot, 'book_tot_rw': tot_rw, 'res_rw': res_rw,
                  'change_of': change_of})

    # ---- The crews' households and their money (the households' market: comforts and services only from what is over
    # comforts.above a head, a comforts.share of it a day; food topped up to food.days).
    above, share = country['comforts']['above'], country['comforts']['share']
    food_days = country['food']['days']
    purse = stock = need_all = 0.0
    fill = []
    for f in farms:
        m = 1 + f['hands']
        wage = kw + f['hands'] * hw
        need = needs_a_day(m, items, hh)
        surplus = wage - m * food_head - need
        f['purse'] = above * m + max(0.0, surplus) / share
        f['fill_days'] = (above * m + food_days * m * food_head) / surplus if surplus > 0 else float('inf')
        purse += f['purse'] * f['count']
        stock += food_days * m * food_head * f['count']
        need_all += need * f['count']
        fill.append(f['fill_days'])
    X = Yr('X')
    services_share = ym(lambda s: C[s]['services'] / C[s]['other_coins'])
    crew_services = X * services_share
    city_share = crew_services * country['rules']['servicesCityShare'] / 100
    held = rr + purse
    say(f"### The money the switch holds\n")
    say(f"The crews as households (each farm's crew one household at its steading): their wages {W:,.0f}p, their food "
        f"{n * food_head:,.0f}p, their needs {need_all:,.0f}p (crafts.json `households`, a farm's household's), so "
        f"{W - n * food_head - need_all:,.0f}p a day for comforts and services, which the households' market spends only from "
        f"what a household holds over {above}p a head ({share:.0%} of it a day). A crew's purse settles at {above}p a head "
        f"and five days of that: {purse:,.0f}p in all ({purse / n:,.0f}p a head). Their {food_days} days of food in the larder, "
        f"bought once: {stock:,.0f}p (spent in the food shops: goods, not money held). The farms' floats {rr:,.0f}p. Held in "
        f"all: {held:,.0f}p, {held / money * 100:,.0f}% of the realm's money, {held / ym(lambda s: long[s]['treasury']) * 100:,.0f}% of "
        f"the treasury. From nought a crew reaches the {above}p line in {min(fill):,.0f}-{max(fill):,.0f} days (its larder first), "
        f"its comforts at their full rate about two weeks later. The crews' services {crew_services:,.0f}p a day, "
        f"{country['rules']['servicesCityShare']}% of it the treasury's: {city_share:,.0f}p.\n")
    STATE.update({'purse': purse, 'stock': stock, 'need_all': need_all, 'held': held, 'fill': (min(fill), max(fill)),
                  'crew_services': crew_services, 'city_share': city_share, 'X': X})

    # ---- The treasury after the switch: the toll, the crews' services' share, and the wage support the losing trades'
    # tills would draw (supportWages: 100% of a till's shortfall from what the treasury holds over treasuryCap), each
    # trade at the settled years' wage step (`settled_steps`) and orders; then the treasury's nights replayed with them.
    mb = money_by(measured) if measured else {}
    rules = country['rules']
    floor, drift_days = rules['wageDriftFloor'], rules['wageDriftDays']
    most_k, most_h = rules['wageDriftMost'], rules['wageDriftHandMost']
    base_k, base_h = WAGE['keeper'], WAGE['hand']   # the world's wages, under every till's step (RatwSociety.h)

    def owed_at(k, h, step):
        """A trade's posts' wages a day at a wage step (driftedWage: each post within its bound, never under the floor)."""
        if step > 0:
            return k * (base_k + min(step, most_k)) + h * (base_h + min(step, most_h))
        return k * max(min(base_k, floor), base_k + max(step, -most_k)) + h * max(min(base_h, floor), base_h + max(step, -most_h))

    def can_fall(step):
        """wageFalls: a step falls only while it lowers some post (none under the floor, each within its bound)."""
        return (step > -most_k and base_k + step - 1 >= floor) or (step > -most_h and base_h + step - 1 >= floor)

    # The short runs are the founding world (559 residents, every wage step about nought), so a trade's wages, profit and
    # support are taken from the settled years instead: what its posts are owed at its wage step there (the loop file),
    # and what its tills can pay them, its margin on its orders now (the order book's sales, scaled to the long run, times
    # the short runs' margin on a penny: in the short runs a trade's wages and keepers' profit were its margin, to 5%).
    margin_of = {}
    for b, c in mb.items():
        if c.get('sales', 0) > 0 and c.get('tills'):
            va = max(0.0, 1 - (c['draws'] + c['carriage_in'] + c['city_inputs'] + c['service_share']) / c['sales'])
            k, h = c['tills'], max(0, c['posts'] - c['tills'])
            margin_of[b] = {'va': va, 'k': k, 'h': h, 'step': (trades or {}).get(b, {}).get('step', 0.0),
                            'owed': owed_at(k, h, (trades or {}).get(b, {}).get('step', 0.0)),
                            'founding': owed_at(k, h, 0), 'paid_short': c.get('wages', 0) + c.get('support', 0),
                            'margin': sum(now[b].values()) * va,
                            'margin_short': c['sales'] * va, 'pay_short': c.get('wages', 0) + c.get('profit', 0)}

    def support(changes, ramp=1):
        """The wage support the switch adds (supportWages), by trade, day by day over its first year, the switch's change
        of orders coming in over `ramp` days (the areas switched one at a time): a trade's tills pay its posts from their
        margin on its orders (a lost penny of orders costs them their margin on it), the treasury makes up the rest, and
        a loser short on most days lowers its wage step a penny every wageDriftDays (judgeWages; each post within its
        bound and never under the floor, so a step lowers only the keepers once the hands are at it) while it is short of
        more than it was. At once (its first day, the switch whole) and once drifted (its last day); the extra support each
        day (`daily`, for the treasury's nights); the wages the drift takes from the losers' posts."""
        rows, at_once, after, total, cut_all, base, gain_now, gain_drift = [], 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        daily = [0.0] * 365
        for b, ch in sorted(changes.items(), key=lambda x: x[1]):
            m = margin_of.get(b)
            if not m:
                continue
            va, k, h, owed, step0, avail = m['va'], m['k'], m['h'], m['owed'], m['step'], m['margin']
            gap = max(0.0, owed - avail)         # (Short now by the trade's sums; the rest is one till's against another's.)
            d_now = max(0.0, owed - avail - ch * va) - gap
            paid, o, step, last = 0.0, owed, step0, 0
            for day in range(365):
                sh = max(0.0, o - avail - ch * va * min(1.0, (day + 1) / max(1, ramp)))
                paid += sh - gap                  # (A gainer's smaller shortfall is the treasury's saving.)
                daily[day] += sh - gap
                if (day + 1) % drift_days == 0 and ch < 0 and sh > gap + 1e-9 and can_fall(step):
                    step -= 1
                    o = owed - (owed_at(k, h, step0) - owed_at(k, h, step))
                    last = day + 1
            d_drift = max(0.0, o - avail - ch * va) - gap
            cut = owed - o
            at_once += d_now
            after += d_drift
            gain_now += min(0.0, d_now)
            gain_drift += min(0.0, d_drift)
            total += paid
            cut_all += cut
            base += gap
            if d_now >= 1:
                sp = lambda x: f"{x:+,.0f}p" if abs(x) >= .5 else '0p'
                rows.append([BIZ_NAME.get(b, b), f"{k:,.0f} / {k + h:,.0f}", f"{step0:+,.1f}p", f"{owed:,.0f}p", f"{avail:,.0f}p",
                             f"{ch:+,.0f}p", f"{va * 100:,.0f}%", sp(d_now), sp(d_drift),
                             f"{last / 30:,.0f}" if last else '-', f"{cut:,.0f}p"])
        return {'rows': rows, 'now': at_once, 'drift': after, 'paid': total, 'cut': cut_all, 'base': base, 'daily': daily,
                'gain_now': gain_now, 'gain_drift': gain_drift}

    # The treasury's nights in the settled years, as the economy lines have them, to replay with the switch (each year from
    # its first day, Spring): what it holds; the day's toll, services' share, patrons and its own posts in one (the rest
    # of its change); the wage support, the alms and the surplus's alms and works, in the order of the night.
    nights = {d['day']: d for d in load_lines(a.economy) if (a.from_year - 1) * 365 - 1 <= d['day'] <= 12 * 365 - 1}
    cap, keep, tfloat = rules['treasuryCap'], rules['surplusKeep'], 200   # (TreasuryFloat, RatwSociety.h)
    # An economy line's treasury is the day's end: after the surplus (18:00, last) come a few pennies more (the evening's
    # services and draws). On a night the surplus paid, the treasury held exactly cap + keep + the surplus before it; on
    # the others, the day's end less the evening's mean (as measured on those nights), and no more than cap + keep.
    surplus_nights = [d for d in nights.values() if d.get('surplus', 0) > 0]
    evening = sum(d['treasury'] + d.get('surplus_alms', 0) + d.get('works_spent', 0) - d['surplus'] - cap - keep
                  for d in surplus_nights) / max(1, len(surplus_nights))
    for d in nights.values():
        valve = d.get('surplus_alms', 0) + d.get('works_spent', 0)
        d['before_surplus'] = cap + keep + d['surplus'] if d.get('surplus', 0) > 0 else min(d['treasury'] - evening, cap + keep)
        d['evening'] = d['treasury'] - (d['before_surplus'] - valve)

    def replay(sup_, d_in, ramp=1):
        """Each settled year replayed night by night with the switch from its first day (doc 71, 2.6): tonight the
        treasury has its day's other flows and the switch's toll and services' share (coming in over `ramp` days); then
        the wage support, now and the switch's extra (`sup_['daily']`), from what it holds over treasuryCap (what it can't
        pay goes unpaid); the alms from what it holds over its float; the surplus, what is left over treasuryCap and
        surplusKeep, to the poorest households and the works (spendSurplus, placed as that night placed it). Per year: the
        surplus not paid out (less: more paid out), how far the treasury runs under its own path (the mean and the most),
        its lowest, the support unpaid and the alms unpaid."""
        out = []
        for y in range(a.from_year, 13):
            first = (y - 1) * 365
            if first - 1 not in nights or first + 364 not in nights:
                continue
            T = nights[first - 1]['treasury']
            lost = unpaid = alms_short = under = most = 0.0
            low = float('inf')
            for i in range(365):
                d, p = nights[first + i], nights[first + i - 1]
                valve = d.get('surplus_alms', 0) + d.get('works_spent', 0)
                # The day's flows up to the wage support (the toll, the services' share, the patrons, its own posts).
                day = d['before_surplus'] + d.get('alms', 0) + d.get('wage_support', 0) - p['treasury']
                pre = T + day + d_in * min(1.0, (i + 1) / max(1, ramp))
                want = max(0.0, d.get('wage_support', 0) + sup_['daily'][i])
                give = min(want, max(0.0, pre - cap))
                unpaid += want - give
                t1 = pre - give
                alms = min(d.get('alms', 0), max(0.0, t1 - tfloat))
                alms_short += d.get('alms', 0) - alms
                t2 = t1 - alms
                over = max(0.0, t2 - cap - keep)
                placed = over * (valve / d['surplus'] if d.get('surplus', 0) > 0 else 1.0)
                T = t2 - placed + d['evening']
                lost += valve - placed
                gone = d['treasury'] - T
                under += gone / 365
                most = max(most, gone)
                low = min(low, T)
            out.append({'year': y, 'lost': lost, 'under': under, 'most': most, 'low': low, 'unpaid': unpaid,
                        'alms_short': alms_short, 'end': nights[first + 364]['treasury'] - T})
        return out

    change_rw = {b: sum(after_rw[b][k] for k in keys) - now[b]['country'] for b in now}
    d_toll = toll_after - toll_now
    d_toll_rw = Yr('toll', res_rw) - toll_now
    sup, sup_rw = support(change_of), support(change_rw)
    sup_ph, sup_rw_ph = support(change_of, 180), support(change_rw, 180)
    say("### The treasury after the switch: the wage support the losing trades' tills would draw (a day)\n")
    M = margin_of.values()
    short_now = sum(max(0.0, m['owed'] - m['margin']) for m in M)
    spare_now = sum(max(0.0, m['margin'] - m['owed']) for m in M)
    say(f"(Each trade at its settled years' wage step (the loop file, every 28 days; the world's wages {base_k:.0f}p and "
        f"{base_h:.0f}p, the floor {floor}p): its posts owed {sum(m['owed'] for m in M):,.0f}p a day in all, against "
        f"{sum(m['founding'] for m in M):,.0f}p at the founding's steps (the short runs owed {sum(m['paid_short'] for m in M):,.0f}p). "
        f"What its tills can pay them: its margin on its orders now (the order book's sales times the short runs' margin on "
        f"a penny; in the short runs the margins were {sum(m['margin_short'] for m in M):,.0f}p and their wages and profit "
        f"{sum(m['pay_short'] for m in M):,.0f}p), {sum(m['margin'] for m in M):,.0f}p in all. By the trades' sums they are "
        f"short {short_now:,.0f}p a day and have {spare_now:,.0f}p to spare, against the long run's wage support of "
        f"{ym(lambda s: long[s]['wage_support']):,.0f}p and keepers' profit of {ym(lambda s: long[s]['profit']):,.0f}p (within a "
        f"trade, one till's spare and another's shortfall net).)\n")
    say(table(['Trade', 'Tills / posts', 'Wage step now', "Its posts' wages", 'Its margin on its orders now', 'Orders, change',
               'Of it its margin', 'Wage support, change at once', 'Once its wages have drifted down', 'Months to drift',
               'Wages the drift takes'], sup['rows']))
    room = ym(lambda s: long[s]['treasury']) - rules['treasuryCap']
    base_year = replay({'daily': [0.0] * 365}, 0.0)
    assert all(abs(r['end']) < 1e-6 and abs(r['lost']) < 1e-6 for r in base_year), 'the replay must give the long run back'
    runs = {}
    for name, S, dt in (("The Country's basket as it is", sup, d_toll), ("re-weighted", sup_rw, d_toll_rw)):
        say(f"\n{name}: the treasury's flows a day: the toll {dt:+,.0f}p; the crews' services' share {city_share:+,.0f}p; the "
            f"wage support {-S['now']:+,.0f}p at once (every trade: the gainers' smaller shortfalls, {-S['gain_now']:,.0f}p, netted), {-S['drift']:+,.0f}p once "
            f"the losers' wages have drifted down; in all {dt + city_share - S['now']:+,.0f}p at once, "
            f"{dt + city_share - S['drift']:+,.0f}p after. The extra support over the first year: {S['paid']:,.0f}p (it holds "
            f"{room:,.0f}p over its cap on average); the drift takes {S['cut']:,.0f}p a day from the losers' posts' wages.")
    say(f"\n(Wage support now {sup['base']:,.0f}p a day.) Switched an area at a time over six months: the extra support over "
        f"a year {sup_ph['paid']:,.0f}p (re-weighted {sup_rw_ph['paid']:,.0f}p).\n")
    say("The treasury's nights replayed, each settled year from its first day with the switch (the support from what is over "
        f"its cap, {cap:,}p; the alms from what is over its float; the surplus, what is over {cap + keep:,}p, to the poorest "
        "households and the works): per year, the surplus not paid out (negative: more paid out), how far it runs under its "
        "own path (the year's mean / the most), its lowest, the support unpaid (and the alms unpaid, if any).\n")
    rows = []
    for name, S, dt, ramp in (('all at once', sup, d_toll, 1), ('all at once, re-weighted', sup_rw, d_toll_rw, 1),
                              ('an area at a time over six months', sup_ph, d_toll, 180),
                              ('an area at a time, re-weighted', sup_rw_ph, d_toll_rw, 180)):
        rr_ = replay(S, dt + city_share, ramp)
        runs[name] = rr_
        rows.append([name] + [f"{r['lost']:,.0f}p; {r['under']:,.0f} / {r['most']:,.0f}p; {r['low']:,.0f}p; {r['unpaid']:,.0f}p" +
                              (f" ({r['alms_short']:,.0f}p)" if r['alms_short'] >= .5 else '')
                              for r in rr_])
    say(table(['Switched'] + [f"year {r['year']}" for r in base_year], rows))
    say(f"\n(The long run's own, each year: the surplus paid out " +
        ', '.join(f"{sum(nights[(y - 1) * 365 + i].get('surplus_alms', 0) + nights[(y - 1) * 365 + i].get('works_spent', 0) for i in range(365)):,.0f}p"
                  for y in (r['year'] for r in base_year)) +
        "; its lowest " + ', '.join(f"{min(nights[(y - 1) * 365 + i]['treasury'] for i in range(365)):,.0f}p" for y in (r['year'] for r in base_year)) + '.)\n')
    rng = lambda rr_, k: f"{min(r[k] for r in rr_):,.0f}-{max(r[k] for r in rr_):,.0f}p"
    say('\nOver the settled years (the least to the most):\n')
    say(table(['Switched', 'The surplus not paid out in the first year', 'Under its own path (the year\'s mean)',
               'The most under it', 'Its lowest', 'Support unpaid'],
              [[name, rng(rr_, 'lost'), rng(rr_, 'under'), rng(rr_, 'most'), rng(rr_, 'low'), rng(rr_, 'unpaid')]
               for name, rr_ in runs.items()]))
    say('')
    STATE.update({'sup': sup, 'sup_rw': sup_rw, 'sup_ph': sup_ph, 'sup_rw_ph': sup_rw_ph, 'd_toll': d_toll, 'd_toll_rw': d_toll_rw,
                  'room': room, 'replay': runs})

    # ---- Yields in full.
    say("### Yields per farm, per good (pieces a day, the slack included): season means, Sp / Su / Au / Wi; the middle-day "
        "values (country_farms.json) and their blend's season means\n")
    rows = []
    for f in farms:
        for g, v in f['yield'].items():
            rows.append([f['kind'], g, ' / '.join(f'{x:.1f}' for x in v), ' / '.join(f'{x:.1f}' for x in f['yield_mid'][g]),
                         ' / '.join(f'{x:.1f}' for x in f['yield_fit'][g])])
    say(table(['Farm', 'Good', 'Season means (what it sells)', 'Middle-day values', 'Their blend, season means'], rows))
    say('')
    # Who the yields feed.
    diet_pieces = ym(lambda s: sum(x['pieces'] for g, x in C[s]['diet'].items() if is_food(g)))
    res_pop = ym(lambda s: long[s]['residents'])
    say(f"Who the food feeds (a day): the city's {res_pop:,.0f} residents (the Country's {payees:,.0f} post-holders among them), "
        f"the farms' {n} crew (they buy in the city: in its draws), the Country's own {table_people} who stay in the Country "
        f"({other_workers} workers of the other industries once placed, {ESTATE_PEOPLE} of the Houses' estates: the armed "
        f"retainers, country.json `patrons`), each eating {diet_pieces:.2f} drawn pieces of the residents' diet, and "
        f"{SLACK * 100:,.0f}% slack.\n")
    # The season caps (countryLeft: a group's normal times its supply, checked on every store draw of its goods) against
    # the city's draws after the switch, the crews' food among them (the yields' slack on top).
    rows = []
    for g in GROUPS:
        if g == 'vegetables':
            continue
        after_g = [sum(x['pieces'] for good, x in res[s]['draws'].items() if group_of.get(good) == g) for s in SEASONS]
        cap = [long[s].get(f'{g}_cap', 0) for s in SEASONS]
        now_g = [long[s].get(f'{g}_drawn', 0) for s in SEASONS]
        rows.append([g] + [f"{now_g[i]:,.0f} / {after_g[i]:,.0f} / {cap[i]:,.0f}" + (' (over)' if after_g[i] > cap[i] + .5 else '')
                           for i in range(4)])
    say("The season caps: the group's draws now / after the switch / its cap (pieces a day, season means)\n")
    say(table(['Group', 'Spring', 'Summer', 'Autumn', 'Winter'], rows))
    say('')
    STATE.update({'farms': farms, 'res': res, 'res_season': res_season, 'even': even, 'kw': kw, 'hw': hw, 'C': C, 'A': A,
                  'posts': posts, 'payees': payees, 'patrons': patrons, 'food_head': food_head, 'diet_farm': diet_farm,
                  'diet_country': diet_country, 'other_farm': other_farm, 'other_country': other_country, 'n': n, 'nf': nf,
                  'W': W, 'R_now': R_now, 'others': others, 'other_workers': other_workers, 'table_people': table_people,
                  'tot_r': tot_r, 'fb': fb_till, 'fb_goods': fb_goods, 'fb_carr': fb_carr, 'cycle': cycle})
    return STATE


def others_table(others, kw, hw, say):
    say('### The other industries, the Country\'s after the switch (a day)\n')
    rows = []
    for k, v in others.items():
        sites = '; '.join(f"{n} {u} ({w}): {' + '.join(str(x) for x in each)}" for u, n, w, each in v['sites']) or \
            'no works of its own (it pays no one): gathered by the eel and fowl marsh\'s crew and in the central hills\' woods'
        rows.append([k, ', '.join(f'{g} {d:,.0f}' for g, d in sorted(v['draw'].items(), key=lambda x: -x[1])),
                     f"{v['income_now']:,.0f}p", f"{v['income']:,.0f}p", f"{max(0, math.floor(v['can_pay']))}", f"{v['fte']:,.0f}",
                     f"{v['plausible']:,.0f}", f"{v['workers']}", sites])
    say(table(['Industry', 'Drawn a day', 'The Country\'s takings now', 'After the switch', 'Wolves it can pay (wages; whole wolves)',
               "Workers at the game's rates", 'Workers, plausible', 'Workers counted (fed by the farms)', 'Works (where): workers at each'], rows))
    say('')


OTHER_FOOTPRINT = {   # tiles: the works' own, and woods or ground they work that they share with the wild
    'logging camp': {'own_tiles': 40 * 30, 'shared_range_tiles': 200 * 200},
    "coppice and woodcutters' hut": {'own_tiles': 20 * 15, 'shared_range_tiles': 100 * 100},
    'charcoal hearth': {'own_tiles': 30 * 30, 'shared_range_tiles': 0},
    'water sawmill': {'own_tiles': 30 * 20, 'shared_range_tiles': 0},
    'iron mine': {'own_tiles': 40 * 30, 'shared_range_tiles': 0},
    'bloomery (iron smelter)': {'own_tiles': 30 * 20, 'shared_range_tiles': 0},
    'copper pit': {'own_tiles': 30 * 20, 'shared_range_tiles': 0},
    'tin streams': {'own_tiles': 40 * 15, 'shared_range_tiles': 0},
    'quarry (stone and limestone)': {'own_tiles': 80 * 60, 'shared_range_tiles': 0},
    'clay pit and sand bank': {'own_tiles': 60 * 40, 'shared_range_tiles': 0},
    'hemp and flax field': {'own_tiles': 100 * 70, 'shared_range_tiles': 0},
    'village pottery': {'own_tiles': 25 * 20, 'shared_range_tiles': 0},
    "weavers' and soap boilers' cottages": {'own_tiles': 20 * 15, 'shared_range_tiles': 0},
    'country press and paper mill': {'own_tiles': 30 * 20, 'shared_range_tiles': 0},
    'village smithy and tanyard': {'own_tiles': 25 * 20, 'shared_range_tiles': 0},
}
# The map's scale for the farms (the user, 2026-11-01: "size them somewhere in the middle. They shouldn't be massive but
# a player should feel like they're big"): about 14 cells in all, 6.5 of the farms' own ground (fields, orchards,
# vines, meadows, steadings, pans) and 7.5 of shared range (grazing, heath, woodland pannage), each kind's share as
# real land would have it (the real-yield cross-check, size_units, on these yields).
CELL = 256 * 256
OWN_CELLS, RANGE_CELLS = 6.5, 7.5
# Which of the cross-check's units is which farm, and whether its land is the farm's own or shared range.
REAL_UNITS = {'grain farm': [('grain farm', 'own'), ('water mill', 'own')], 'orchard': [('orchard', 'own')],
              'orchard with a drying house': [('orchard', 'own')],
              'vineyard': [('vineyard', 'own')], 'cattle and dairy farm': [('dairy farm', 'own'), ('cattle ranch', 'range')],
              'pig farm': [('piggery', 'range')], 'sheep and goat run': [('sheep run', 'range'), ('goat fold', 'range')],
              'rabbit warren': [('rabbit warren', 'range')], 'beekeeper': [('apiary', 'own')], 'salt works': [('salt works', 'own')]}
LANDINGS = {'sea fishing boat': 30 * 15, 'lake and river boat': 20 * 12, 'eel and fowl marsh': 25 * 15}   # a landing or jetty
MARSH_RANGE = 150 * 100   # the eel and fowl marsh's reed beds, shared with the wild


def real_check_of(farms, sc, goods_s):
    """The real-yield cross-check (size_units) on the farms' yields: land by kind and labour by season."""
    demand = collections.defaultdict(lambda: {s: 0.0 for s in SEASONS})
    chains = dict(PRIMARY, meal={'raw_meat': 1})
    for f in farms:
        for g, per in f['yield'].items():
            for pg, k in chains.get(g, {g: 1}).items():
                for i, s in enumerate(SEASONS):
                    demand[pg][s] += per[i] * f['count'] * k
    U = size_units(dict(demand), sc, goods_s, goats=0)
    land = {}
    for f in farms:
        units = REAL_UNITS.get(f['kind'])
        if not units:
            continue
        # Two kinds of one real unit (the orchards, with and without a drying house) share its land by their apples.
        k = f['sells'].get('apples', 1) if f['kind'].startswith('orchard') else 1
        own = sum(U[u]['tiles'] + (U[u]['range_tiles'] if how == 'own' else 0) for u, how in units if u in U) * k
        rng = sum(U[u]['range_tiles'] for u, how in units if u in U and how == 'range') * k
        land[f['kind']] = {'own': own, 'range': rng}
    labour = {s: sum(u['workers'][s] for u in U.values()) for s in SEASONS}
    return U, land, labour


def footprints(farms, land):
    own_t = sum(v['own'] for v in land.values())
    rng_t = sum(v['range'] for v in land.values())
    landings = sum(LANDINGS.get(f['kind'], 0) * f['count'] for f in farms)
    for f in farms:
        L = land.get(f['kind'])
        if L:
            f['own_tiles'] = round(L['own'] / own_t * (OWN_CELLS * CELL - landings) / f['count'], -2)
            f['range_tiles'] = round(L['range'] / rng_t * (RANGE_CELLS * CELL - MARSH_RANGE) / f['count'], -2) if rng_t else 0
        else:
            f['own_tiles'] = LANDINGS.get(f['kind'], 0)
            f['range_tiles'] = MARSH_RANGE if f['kind'] == 'eel and fowl marsh' else 0
    return own_t, rng_t


def write_farms(path, st, others):
    """The placement list and the farms' settings, the defaults Atlas's Economy page and the DM's Economy tab edit (doc
    71, 4 and 6): per area its farms, yieldScale, crewScale, wages and price; per farm its crew, wages, basket, reserve
    and yields (middle-day values)."""
    farms, res, even = st['farms'], st['res'], st['even']
    shares = json.load(open(ROOT / 'Data/Items/country.json'))['markets'][0]['basket']
    areas = []
    for area in AREAS:
        fs = [f for f in farms if f['area'] == area['id']]
        out = []
        for f in fs:
            for i in range(f['count']):
                where, dist = f['where'][i % len(f['where'])]
                out.append({
                    'id': (f.get('ids') or [f"{f['kind'].replace(' ', '_')}_{k + 1}" for k in range(f['count'])])[i],
                    'kind': f['kind'], 'region': where, 'distance': dist, 'terrain': f['terrain'], 'near': f['near'],
                    'footprint': {'own_tiles': int(f['own_tiles']), 'shared_range_tiles': int(f['range_tiles'])},
                    'crew': {'keepers': 1, 'hands': f['hands']}, 'minHands': f['min_hands'],
                    'wages': {'keeper': None, 'hand': None},
                    'basketADay': round(f['basket'] / f['count'], 1),
                    'reserve': round(f['reserve'] / f['count']),
                    'yield': {g: [round(x, 1) for x in f['yield_mid'][g]] for g in f['yield']},
                    'sells': f['sells'], 'store': f['store']})
        out.sort(key=lambda x: (x['id'].rsplit('_', 1)[0], int(x['id'].rsplit('_', 1)[1])))
        rev = [round(sum(res[s]['R'][f['kind']] for f in fs), 1) for s in SEASONS]
        areas.append({'id': area['id'], 'name': area['name'], 'goods': area['goods'], 'on': False, 'yieldScale': 100,
                      'crewScale': 100, 'wages': {'keeper': None, 'hand': None},
                      'price': 'year' if even.get(area['id']) else 'season',
                      'priceFactor': round(even[area['id']] * 100) if even.get(area['id']) else None,
                      'revenueADay': rev, 'wolvesItCanPay': round(sum(f['can_pay'] for f in fs)), 'farms': out})
    doc = {
        'format': 'ratw-country-farms', 'version': 3,
        'about': ("The Country's food farms and other works as they would be placed (Docs/Design/71-the-country-s-industries.md). "
                  "Written by tools/country_industry.py from the 12-year run (settled years 4-12); nothing reads it yet. The "
                  "defaults Atlas's Economy page and the DM's Economy tab would edit, per area and per farm. An area: `on` "
                  "(false: the Country still supplies its goods), `yieldScale` (percent, on all its farms' yields), `crewScale` "
                  "(percent, on all its farms' hands, rounded half up), `wages` (pennies a day for its farms' keepers and hands; "
                  "null: the defaults'), `price` ('year': the farms sell from store at `priceFactor` percent of the catalogue "
                  "all year; 'season': at the season's price, as the store room does now). A farm: `crew` (keepers and hands; "
                  "`minHands` is the smallest sensible crew), `wages` (null: the area's, then the defaults'; fixed, not "
                  "drifting: the basket takes what is over them), `basketADay` (the design's mean: what the till spends a "
                  "day on the city's goods, their carriage out included, from what it holds over its wages and `reserve`), "
                  "`reserve` (pennies its till keeps before its basket: two days of its wages, and a lean season's shortfall "
                  "if it has one), `yield` (pieces a day of each good, the value at each season's middle day for Spring, "
                  "Summer, Autumn and Winter, blended between them by Society::seasonal as country.json's seasons are; their "
                  "blend's season means are the doc's 2.4 table). Precedence: a farm's own setting, then its area's, then the "
                  "defaults. The yields are the city's draws after the switch (the crews' own food among them), what the "
                  "Country's own people eat, and `slack`. `footprint`: own_tiles are the farm's own ground (fields, orchard, "
                  "vines, meadow, steading, pans); shared_range_tiles is open grazing, heath or woodland its herds share with "
                  "the wild. `distance` is from Ridgemere, the realm's north-west corner (near: within 2 cells, middle: 3-8, "
                  "far: 9 or more)."),
        'defaults': {'wages': {'keeper': int(st['kw']), 'hand': int(st['hw'])}, 'basketShare': round(BASKET_SHARE * 100),
                     'floatDays': FLOAT_DAYS, 'slack': round(SLACK * 100), 'estatePeople': ESTATE_PEOPLE,
                     'otherWorkers': st['other_workers'], 'openingStockDays': OPENING_DAYS},
        'basket': {g: shares[g] for g in FARM_GOODS if g in shares},
        'dropped': {'goods': DROPPED, 'listedIn': DROPPED_IN},
        'areas': areas,
        'others': [{'industry': k, 'revenueADay': round(v['income']), 'workersItCanPay': max(0, math.floor(v['can_pay'])),
                    'workersPlausible': round(v['plausible']), 'workersCounted': v['workers'],
                    'works': [{'kind': u, 'count': n, 'region': w, 'workers': each, 'footprint': OTHER_FOOTPRINT.get(u, {})}
                              for u, n, w, each in v['sites']],
                    **({} if v['sites'] else {'about': "No works of its own (about 8p a day pays no one): gathered at the "
                                                       "marsh's edge by the eel and fowl marsh's crew and in the central "
                                                       "hills' woods; the pennies stay the Country's."})}
                   for k, v in others.items()],
    }
    text = json.dumps(doc, indent=1)
    text = re.sub(r'\[\s+([^\[\]{}]*?)\s+\]', lambda m: '[' + ', '.join(x.strip() for x in m.group(1).split(',')) + ']', text)
    open(path, 'w').write(text + '\n')
    print(f'wrote {path}', file=sys.stderr)


OPENING_DAYS = 7    # the farms' opening stock: a week of each good's yield, created at the switch ("grown at <farm>")
# Where vegetables are listed, for the build step that drops them from Ridgemere 2.0's economy (the item stays in the
# catalogue for the old map).
DROPPED_IN = ['Data/Items/crafts.json crafts: stew (inn: raw_meat + vegetables -> meal)',
              'Data/Items/crafts.json buys: inn (vegetables)',
              'Data/Items/crafts.json supplies: stall, provisioner (vegetables)',
              'Data/Items/country.json seasons: the vegetables group (vegetables, mushrooms)',
              'Data/Items/recipes.json: meal_stew, mushroom_stew (players\' recipes)',
              'Data/Items/regions.json: the old map\'s places (brings_in)',
              'Data/Economy/ladder.json: vegetables (staple)']


def size_units(demand, sc, goods_s, goats=.25):
    """The units for a demand (primary pieces a day by season), with land, labour by season and the goods they sell
    (`goats`: the share of the milk from goats; the vegetables' gardens only if any are wanted)."""
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
    unit('grain farm', n, f"{arable / n:,.0f} ha arable ({wheat_ha / n:,.0f} wheat, {oats_ha / n:,.0f} oats, {fallow / n:,.0f} fallow)",
         arable, sown * g['days_ha'] + fallow * g['days_ha_fallow'], g['season_labour'],
         f"{t_year('wheat'):,.0f} t wheat, {t_year('oats'):,.0f} t oats a year", extra_tiles=n * 60 * 40)
    # Mills: two, a miller and a hand each.
    unit('water mill', 2, f"{t_year('wheat') / 365 * 1000 / 2:,.0f} kg of wheat a day each (of about 1 t it could)", 0, 0, [.25] * 4,
         'flour', workers={s: 4.0 for s in SEASONS}, fixed_staff=4, extra_tiles=2 * UNITS['water mill']['footprint_tiles'])
    o = UNITS['orchard']
    ha = t_year('apples') / o['yield_t_ha']['apples'] + t_year('berries') / 2   # (berries from the hedges, 2 t a ha of hedge)
    n = max(2, round(ha / o['unit_ha']))
    unit('orchard', n, f"{ha / n:,.0f} ha, {ha / n * o['trees_ha']:,.0f} trees, a fruit store and cider press", ha, ha * o['days_ha'],
         o['season_labour'], f"{t_year('apples'):,.0f} t apples a year", extra_tiles=n * 40 * 30)
    v = UNITS['vineyard']
    ha = t_year('grapes') / v['yield_t_ha']['grapes']
    n = max(2, round(ha / v['unit_ha']))
    unit('vineyard', n, f"{ha / n:.1f} ha of vines, a press house and cellar", ha, ha * v['days_ha'], v['season_labour'],
         f"{t_year('grapes'):.1f} t grapes a year (a third pressed on the estate)", extra_tiles=n * 30 * 20)
    m = UNITS['market garden']
    ha = t_year('vegetables') / m['yield_t_ha']['vegetables']
    n = 2
    if ha > 0:
        unit('market garden', n, f"{ha / n:.1f} ha of beds", ha, max(ha * m['days_ha'], 2 * 313), m['season_labour'],
             f"{t_year('vegetables'):.1f} t vegetables a year", extra_tiles=n * 20 * 15)
    d = UNITS['dairy farm']
    cows = t_year('milk', 1 - goats) * 1000 / d['litres_cow_year']
    head = cows * d['herd_per_milker']
    n = max(2, round(head / d['unit_head']))
    unit('dairy farm', n, f"{cows / n:,.0f} cows in milk, {head / n:,.0f} head", head * d['ha_head'], head * d['days_head'],
         d['season_labour'], f"{t_year('milk', 1 - goats) * 1000:,.0f} L milk a year", extra_tiles=n * 40 * 30)
    gf = UNITS['goat fold']
    herd = t_year('milk', goats) * 1000 / gf['litres_head_year']
    if herd > 0:
        n = max(2, round(herd / gf['unit_head']))
        unit('goat fold', n, f"{herd / n:,.0f} goats", herd * gf['ha_head'], max(herd * gf['days_head'], n * 313 * .5),
             gf['season_labour'], f"{t_year('milk', goats) * 1000:,.0f} L milk a year, kids", extra_tiles=n * 25 * 20)
    c = UNITS['cattle ranch']
    beef = t_year('raw_meat', c['meat_share']) * 1000
    killed = beef / c['kg_slaughtered']
    head = killed * c['head_per_slaughter']
    n = max(2, round(head / c['unit_head']))
    unit('cattle ranch', n, f"{head / n:,.0f} head, {killed / n:,.0f} killed a year", head * c['ha_head'], head * c['days_head'],
         c['season_labour'], f"{beef / 1000:,.0f} t beef a year, hides, tallow", extra_tiles=n * 50 * 40)
    p = UNITS['piggery']
    pork = (t_year('raw_meat', 1 - c['meat_share']) + t_year('pig')) * 1000
    killed = pork / p['kg_slaughtered']
    head = killed * p['head_per_slaughter']
    n = max(2, round(head / p['unit_head']))
    unit('piggery', n, f"{head / n:,.0f} pigs ({head / n / 10:,.0f} sows), {killed / n:,.0f} killed a year", head * p['ha_head'],
         head * p['days_head'], p['season_labour'], f"{pork / 1000:,.0f} t pork a year", extra_tiles=n * 30 * 25)
    sh = UNITS['sheep run']
    mutton = t_year('mutton') * 1000
    killed = mutton / sh['kg_slaughtered']
    flock = killed * sh['head_per_slaughter']
    n = max(2, round(flock / sh['unit_head']))
    unit('sheep run', n, f"{flock / n:,.0f} sheep", flock * sh['ha_head'], flock * sh['days_head'], sh['season_labour'],
         f"{mutton / 1000:,.0f} t mutton, {flock * 1.5 / 1000:.1f} t wool a year", extra_tiles=n * 30 * 25)
    r = UNITS['rabbit warren']
    does = t_year('rabbit') * 1000 / r['kg_doe']
    n = max(2, round(does / r['unit_head']))
    unit('rabbit warren', n, f"{does / n:,.0f} does", does * r['ha_doe'], does * r['days_doe'], r['season_labour'],
         f"{t_year('rabbit'):,.0f} t rabbit a year, pelts", extra_tiles=n * 20 * 15)
    ap = UNITS['apiary']
    hives = t_year('honey') * 1000 / ap['kg_hive']
    n = max(2, round(hives / ap['unit_head']))
    unit('apiary', n, f"{hives / n:,.0f} skeps", 0, hives / ap['hives_keeper'] * 313, ap['season_labour'],
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
         f"marshes (3); a keeper at each of five landings", 0, 0, [.25] * 4, f"{sum(need_kg[s] * SEASON_DAYS[s] for s in SEASONS) / 1000:,.0f} t fish and shellfish a year",
         workers=crews, keepers=5,
         extra_tiles=boats * sea['footprint_tiles'] + n_lake * lake['footprint_tiles'] + n_eel * eel['footprint_tiles'])
    U['fishing']['boats'] = {'sea': boats, 'lake': n_lake, 'eel': n_eel, 'kg_day': need_kg}
    sa = UNITS['salt works']
    t = t_year('salt')
    n = 2
    unit('salt works', n, f"{t / n:.1f} t a year, {t / n / sa['t_ha_year'] * 10000:,.0f} m2 of pans", t / sa['t_ha_year'],
         t * 25, sa['season_labour'], f"{t:,.0f} t salt a year", extra_tiles=n * 30 * 20)
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
           'orchard with a drying house': 'orchards and vines',
           'cattle farm': 'herds and flocks', 'pig farm': 'herds and flocks', 'sheep run': 'herds and flocks',
           'cattle and dairy farm': 'herds and flocks', 'sheep and goat run': 'herds and flocks',
           'rabbit warren': 'herds and flocks', 'dairy farm': 'herds and flocks', 'goat fold': 'herds and flocks',
           'beekeeper': 'bees', 'sea fishing boat': 'fishing and the marsh', 'lake and river boat': 'fishing and the marsh',
           'eel and fowl marsh': 'fishing and the marsh', 'salt works': 'fishing and the marsh',
           'woods: timber, firewood, bark, charcoal': 'woods', 'herbs': 'woods', 'mining and metals': 'mines, stone and clay',
           'stone, lime, clay, sand': 'mines, stone and clay', 'fibre: hemp, flax, linen': 'fields and mills',
           "country workshops' goods": 'trades and shops'}


def unplaced_pool(path, say, farms=None, others=None, long=None):
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
        need_food[POOL_OF[f['kind']]] += f['count'] * (1 + f['hands'])
    for k, v in (others or {}).items():
        need_other[POOL_OF[k]] += v['workers']
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
    if long:
        arrived = long['last'].get('unplaced', 0)
        say(f"The 12-year run's arrivals: the unplaced pool fell from {long['first'].get('unplaced', 0)} (settled years' first "
            f"day) to {arrived} (its last); from DEV's {len(rows)}, {len(rows) - arrived} came in. Each arrival fills a city "
            f"post, so is of working age: {tw} - {len(rows) - arrived} = {tw - (len(rows) - arrived)} of working age would be "
            f"left, against the {sum(need_food.values())} crew and {sum(need_other.values())} other workers "
            f"({tw - (len(rows) - arrived) - sum(need_food.values()) - sum(need_other.values()):+d}).\n")


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
    m.add_argument('--world', help="the export's world.ratw, for each till's business (the order book)")
    r = sub.add_parser('report')
    r.add_argument('measured')
    r.add_argument('economy')
    r.add_argument('--from-year', type=int, default=4)
    r.add_argument('--real-check', action='store_true',
                   help='also size the units from real pre-industrial yields and labour (the first method; a cross-check)')
    r.add_argument('--units')
    r.add_argument('--unplaced')
    r.add_argument('--dump')
    r.add_argument('--loop', help="the long run's loop file (per-till sales); default ECONOMY's name with _loop")
    a = ap.parse_args()
    {'measure': cmd_measure, 'report': cmd_report}[a.cmd](a)


if __name__ == '__main__':
    main()
