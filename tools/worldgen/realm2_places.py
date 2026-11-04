"""Ridgemere 2.0's Country, built: the food farms' steadings and the six communities their hands live in
(Docs/Design/70-the-realm.md, Progress, "The steadings and the communities"; doc 71, Phase C; doc 72, Siting).

Laid by worldgen.realm2's `expand` after the realm itself (`lay`), so every write of the realm (and the city writer's,
which lays the realm round the city) makes the same buildings with the same ids. Everything before this step is made
exactly as it was: this only adds, on the farms' sites, the community sites and the ground between them.

- **Steadings.** Each of doc 71's 31 farms (`Data/Economy/country_farms.json`) on its realm2 site: a farmhouse for the
  keeper's family (six beds, `HouseholdMost`) and the barn or store its kind needs (a granary and the water mill for
  grain, a drying house or an apple store for an orchard, a press house over a cellar for vines, a byre and a dairy, a
  pig yard, a shearing barn, a rabbitry, a boat shed with a jetty, an eel house, a hive shed, a salt store or boiling
  house), round a yard with a kitchen garden, a footpath to the nearest road or track.
- **Communities.** Coldwater, Larkfield, Southmeadow, Saltwell and Oakhaven (the woods', doc 72) on their reserved
  town sites, and Fenshore, a new hamlet on the fens' dry ground: cottages for the hired hands and their families (six beds each), a general store
  with the keeper's flat over it, an alehouse or inn, a bakehouse at the two biggest, kitchen gardens, a hen run and a
  goat paddock; streets onto the roads; a footpath to each of their farms, where the roads don't already go. The farms
  near Ridgemere have no community: their hands live in the city.
- **The contract** for placing NPCs (`manifest`, written to Data/Economy/communities.json by a write): for every farm its
  site, workplace, farmhouse and community; for every community its homes and beds, its market's workplaces and the
  posts they want, its gardens; doc 72's works (each site, its crew's home, the walk: a footpath from a community);
  and the walking distances.

Interior ids: `fs_<site>_<building>` for a steading, `cn_<community>_<building>` for a community (each with `_flat`,
`_up` or `_cellar` for its other floors); every door `<interior>_door`, stairs `<interior>_stairs_N`.
"""
from __future__ import annotations

import json
import math
import random
from pathlib import Path

import numpy as np

from . import field
from .buildings import (STYLES, Building, Plan, flat_over, house, shop, shop_with_flat, stairs_between, tavern,
                        works, works_kit)
from .canvas import Canvas, half
from .cities import fp
from .site import Site, Lots, door_tile, footprint_size, STEP

ROOT = Path(__file__).resolve().parents[2]
CONTRACT = ROOT / 'Data' / 'Economy' / 'communities.json'
SEED = 20261103

# The communities' and steadings' looks (Docs/Design/70, the regions): the middle and north country builds in timber on
# stone footings under cedar shingle; the warm south in limewash under red tile; Saltwell in limewashed adobe under flat
# pale roofs; the fens in timber and reed thatch (shingle's colour) on stilts over the wet ground.
STYLES.setdefault('country', {'wall': '#', 'floor': '.', 'home_floor': '.', 'outer': '#', 'roof': 'L'})
STYLES.setdefault('southern', {'wall': ']', 'floor': 'f', 'home_floor': '.', 'outer': ']', 'roof': '['})
STYLES.setdefault('adobe', {'wall': ']', 'floor': '.', 'home_floor': '.', 'outer': ']', 'roof': 'V'})
STYLES.setdefault('fen', {'wall': '#', 'floor': '.', 'home_floor': '.', 'outer': '#', 'roof': 'L'})

HOUSEHOLD_MOST = 6                       # Core/RatwSociety.h: a household of more splits into a free home

# ---------------------------------------------------------------------------------------------------------------
# The farms: doc 71's ids (country_farms.json) on the realm's sites (realm2.SITES). The map's own numbering wins where
# the file's region text says otherwise (doc 70, Progress): sea boats 1-4, beekeepers 1-3 and salt works 1-2 are the
# sites the map labels so.
# ---------------------------------------------------------------------------------------------------------------
FARMS = {
    'grain_farm_1': 'grain_long', 'grain_farm_2': 'grain_lark',
    'orchard_1': 'orchard_greenholt', 'orchard_2': 'orchard_coldwater', 'orchard_3': 'orchard_rainward',
    'orchard_4': 'orchard_coastvale', 'orchard_5': 'orchard_larkfield',
    'vineyard_1': 'vineyard_ochre', 'vineyard_2': 'vineyard_south',
    'cattle_and_dairy_farm_1': 'dairy_southgate', 'cattle_and_dairy_farm_2': 'dairy_rainrun',
    'cattle_and_dairy_farm_3': 'dairy_rainward',
    'pig_farm_1': 'pigs_greenholt', 'pig_farm_2': 'pigs_alder', 'pig_farm_3': 'pigs_south',
    'rabbit_warren_1': 'warren_dunes', 'rabbit_warren_2': 'warren_waste',
    'sheep_and_goat_run_1': 'sheep_downs', 'sheep_and_goat_run_2': 'sheep_foothills',
    'eel_and_fowl_marsh_1': 'marsh_eels',
    'lake_and_river_boat_1': 'fishing_glasswater', 'lake_and_river_boat_2': 'fishing_uppermere',
    'sea_fishing_boat_1': 'fishing_sound', 'sea_fishing_boat_2': 'fishing_northcove',
    'sea_fishing_boat_3': 'fishing_delta', 'sea_fishing_boat_4': 'fishing_fencoast',
    'beekeeper_1': 'bees_rainward', 'beekeeper_2': 'bees_greenholt', 'beekeeper_3': 'bees_south',
    'salt_works_1': 'salt_saltpan', 'salt_works_2': 'salt_fencoast',
}
# Where the file's region text and the map differ (the map wins): recorded in the contract.
MAP_WINS = {
    'orchard_3': 'the file: the coast south of the city; the map: the Rainward orchard, south-east of the city (the same '
                 'near country, cider and a drying house)',
    'sea_fishing_boat_2': 'the file: off the city\'s wharf; the map: North Cove, on the rain coast south of the city',
    'sea_fishing_boat_3': 'the file: a cove on the coast south of the city; the map: the Long River\'s mouth, on the fens\' coast',
    'beekeeper_1': 'the file: among the orchards of the central hills; the map: the Rainward bee garth, on the hills east of the city',
    'beekeeper_2': 'the file: among the orchards of the coast south of the city; the map: the Greenholt bee garth, in the lower Alder valley',
    'beekeeper_3': 'the file: the desert edge\'s scrub; the map: the Sunfields bee garth, by Southmeadow',
    'salt_works_1': 'the file: the fens\' coast; the map: the Saltpan workings, in the Red Waste',
    'salt_works_2': 'the file: the flats of the coast south of the city; the map: the Fen salterns, on the fens\' coast',
    'lake_and_river_boat_1': 'the file: a lake under the northern mountains; the map: Glasswater\'s south shore',
    'lake_and_river_boat_2': 'the file: the plains\' largest river; the map: the Upper Mere, on the Long River above Glasswater',
}

# Each farm site's steading: (its stem, its kind, its look, its community or 'ridgemere'). The hands of the farms within
# a walk of the city live in Ridgemere; the rest in the community that serves them (the user, 2026-11-03).
STEADINGS = {
    'grain_long': ('Longmead', 'grain', 'country', 'larkfield'),
    'grain_lark': ('Larkholm', 'grain', 'southern', 'southmeadow'),
    'orchard_greenholt': ('Greenholt', 'orchard_dry', 'country', 'coldwater'),
    'orchard_coldwater': ('Meresend', 'orchard', 'country', 'coldwater'),
    'orchard_larkfield': ('Plumtree', 'orchard', 'country', 'larkfield'),
    'orchard_rainward': ('Rainward', 'orchard_dry', 'country', 'ridgemere'),
    'orchard_coastvale': ('Coastvale', 'orchard', 'country', 'ridgemere'),
    'vineyard_ochre': ('Ochre', 'vineyard', 'adobe', 'saltwell'),
    'vineyard_south': ('Southmeadow', 'vineyard', 'southern', 'southmeadow'),
    'dairy_southgate': ('Southgate', 'dairy', 'country', 'ridgemere'),
    'dairy_rainrun': ('Rainrun', 'dairy', 'country', 'ridgemere'),
    'dairy_rainward': ('Hillside', 'dairy', 'country', 'ridgemere'),
    'pigs_greenholt': ('Oakhurst', 'pigs', 'country', 'coldwater'),
    'pigs_alder': ('Alder', 'pigs', 'country', 'coldwater'),
    'pigs_south': ('Southwood', 'pigs', 'country', 'larkfield'),
    'sheep_downs': ('Larkdowns', 'sheep', 'country', 'coldwater'),
    'sheep_foothills': ('Greyteeth', 'sheep', 'country', 'coldwater'),
    'warren_dunes': ('Dune', 'warren', 'fen', 'fenshore'),
    'warren_waste': ('Redsand', 'warren', 'adobe', 'saltwell'),
    'fishing_sound': ('Soundside', 'boat', 'country', 'ridgemere'),
    'fishing_northcove': ('North Cove', 'boat', 'country', 'ridgemere'),
    'fishing_delta': ('Rivermouth', 'boat', 'fen', 'fenshore'),
    'fishing_fencoast': ('Fen Shore', 'boat', 'fen', 'fenshore'),
    'fishing_glasswater': ('Glasswater', 'boat', 'country', 'coldwater'),
    'fishing_uppermere': ('Upper Mere', 'boat', 'country', 'coldwater'),
    'marsh_eels': ('Withy Creek', 'eels', 'fen', 'fenshore'),
    'bees_rainward': ('Heatherside', 'bees', 'country', 'ridgemere'),
    'bees_greenholt': ('Alderbank', 'bees', 'country', 'ridgemere'),
    'bees_south': ('Sunfields', 'bees', 'southern', 'southmeadow'),
    'salt_saltpan': ('Saltpan', 'salt_pan', 'adobe', 'saltwell'),
    'salt_fencoast': ('Fenside', 'salt_boil', 'fen', 'fenshore'),
}
MILLS = {'grain_long': 'mill_long', 'grain_lark': 'mill_lark'}

# The communities: (id, name, site, look, street tile, cottages, market, gardens, hens, goats, what it is).
# Market entries: (key, kind, name, text, posts) where kind is 'store' (a general store, the keeper's flat over it),
# 'alehouse' or 'inn' (an inn has guest rooms upstairs), 'bakehouse' (a baker's, with a flat when `flat`).
COMMUNITIES = [
    dict(id='coldwater', name='Coldwater', site='coldwater', style='country', street='d', cottages=9, gardens=4,
         about='A lakeside village of the Greenholt where the East, River, High and Downs Roads meet north of '
               'Glasswater: timber cottages under cedar shingle round a green and its well, a store, an inn and a '
               'bakehouse, kitchen gardens, hens and goats. The hands of the orchards, piggeries, sheepwalks and lake '
               'boats round about live here.',
         market=[('store', 'store', 'Coldwater Stores', 'Shelves of rope, lamp oil, salt, nails and cloth, and whatever '
                  'else the carts bring up the East Road; the keeper knows every farm\'s account by heart.',
                  [('shopkeeper', 1), ('shop hand', 1)]),
                 ('inn', 'inn', 'The Glasswater Inn', 'A long low taproom of smoke-dark timber with a fire at each end: '
                  'drovers, boatmen and orchard hands crowd its benches of an evening.',
                  [('innkeeper', 1), ('pot hand', 1), ('cook', 1)]),
                 ('bakehouse', 'bakehouse', 'Coldwater Bakehouse', 'A warm brick oven and the village\'s bread on the '
                  'racks by dawn; the farms\' wives bring their own dough to be baked for a penny.', [('baker', 1)])]),
    dict(id='larkfield', name='Larkfield', site='larkfield', style='country', street='d', cottages=4, gardens=3,
         about='A village at the Long River\'s crossing among the middle country\'s grain: timber cottages round a '
               'green and its well, a store, an alehouse and a bakehouse baking the Longmead mill\'s flour, kitchen '
               'gardens, hens and goats. The hands of Longmead, the Plumtree orchard and the Southwood piggery live here.',
         market=[('store', 'store', 'Larkfield Stores', 'Sacks, sickles, salt and lamp oil, a barrel of nails and a bolt '
                  'or two of cloth: what a grain village needs between market days.', [('shopkeeper', 1)]),
                 ('alehouse', 'alehouse', 'The Sheaf', 'A plain alehouse by the green, its benches worn smooth by the '
                  'harvest crews; a sheaf of last year\'s wheat hangs over the door.', [('alehouse keeper', 1), ('pot hand', 1)]),
                 ('bakehouse', 'bakehouse', 'Larkfield Bakehouse', 'An oven fired before dawn with the mill\'s own flour, '
                  'the loaves set out on boards to cool.', [('baker', 1)], False)]),
    dict(id='southmeadow', name='Southmeadow', site='southmeadow', style='southern', street='d', cottages=3, gardens=2,
         about='A hamlet of the warm Sunfields: limewashed cottages under red tile round a green and its well, a '
               'store and an alehouse, kitchen gardens, hens and goats, the vines and the Lark\'s grain fields round it. '
               'The hands of Larkholm, the Southmeadow vines and the Sunfields bee garth live here.',
         market=[('store', 'store', 'Southmeadow Stores', 'Pruning knives, twine, salt and oil, and the hamlet\'s letters '
                  'waiting on a shelf for whoever goes up the road.', [('shopkeeper', 1)]),
                 ('alehouse', 'alehouse', 'The Vine and Sickle', 'A cool limewashed alehouse with the summer\'s wine '
                  'in casks along one wall and a vine trained over the door.', [('alehouse keeper', 1)])]),
    dict(id='fenshore', name='Fenshore', site='fenshore', style='fen', street='8', cottages=3, gardens=2,
         about='A hamlet of the Sallow Fens on a low island of dry ground: timber houses thatched with reed stand on '
               'stilts over the wet, joined by boardwalks; a store, an alehouse, kitchen gardens on the dry ground, '
               'hens and goats. The eel-catchers, salt boilers, warreners and the fen coast\'s boatmen live here.',
         market=[('store', 'store', 'Fenshore Stores', 'Eel traps, net twine, peat, salt and lamp oil, and a barrel of '
                  'something strong under the counter.', [('shopkeeper', 1)]),
                 ('alehouse', 'alehouse', 'The Heron', 'A smoky alehouse on stilts, its floor boards springing underfoot; '
                  'smoked eel hangs from the rafters and the fen fog presses at the shutters.', [('alehouse keeper', 1)])]),
    dict(id='saltwell', name='Saltwell', site='saltwell', style='adobe', street='f', cottages=3, gardens=2, by_water=True,
         about='The oasis hamlet of the Red Waste by its spring pool, on the way to the Saltpan: limewashed adobe houses '
               'with flat roofs round a well, a store and a caravan inn, gardens watered from the pool, hens and goats. '
               'The salt rakers, the Ochre vines\' hands and the warreners live here.',
         market=[('store', 'store', 'Saltwell Stores', 'Water skins, salt sacks, lamp oil and rope, dates from the '
                  'oasis and whatever the last caravan left.', [('shopkeeper', 1)]),
                 ('inn', 'inn', 'The Oasis Inn', 'A cool adobe inn with thick walls against the heat, a courtyard '
                  'cistern and rooms upstairs for the carters on the Saltpan road.', [('innkeeper', 1), ('pot hand', 1)])]),
    # The sixth (doc 72, Siting): the woods' village on its reserved town site, home of the Oakhaven cut's crew and, at
    # Phase 5, a press and paper mill; sized like Southmeadow and Fenshore, with room for the woods' families.
    dict(id='oakhaven', name='Oakhaven', site='oakhaven', style='country', street='d', cottages=3, gardens=2,
         about='A village at the head of Cedar Sound where the Rainrun and the Alder come down to the sea: timber '
               'cottages under cedar shingle round a green and its well, a store and an alehouse, kitchen gardens, hens '
               'and goats, the Rainwood standing dark above it. The fellers of the Oakhaven cut live here.',
         market=[('store', 'store', 'Oakhaven Stores', 'Axe helves, saw files, rope, salt and lamp oil, and a stack of '
                  'cedar shakes by the door waiting for the next cart down the Sound Road.', [('shopkeeper', 1)]),
                 ('alehouse', 'alehouse', 'The Felled Cedar', 'A low alehouse of split cedar by the green, sawdust on '
                  'the floor and the fellers\' wet coats steaming by the fire.', [('alehouse keeper', 1)])]),
]
# The Country's works doc 72 builds (its Siting, 2026-11-03): each site and where its crew will live. A community's are
# joined to it by a footpath, as its farms are; Ridgemere's (the Rainwood camp and coppice, the granite quarry) are
# within a walk of the gates. Places only: the works' posts and goods are the industries' steps (doc 72, Phases 2-3).
WORKS = {
    'timber_rainwood': 'ridgemere', 'timber_oakhaven': 'oakhaven', 'coppice_rainwood': 'ridgemere',
    'charcoal_greenholt': 'larkfield', 'peat_fens': 'fenshore', 'quarry_granite': 'ridgemere',
    'clay_mirebridge': 'fenshore', 'clay_fens': 'southmeadow',
}
# Each built works' id in country_farms.json (doc 72, Phase 2: the fuel area's works; the quarry and the pits are Phase 3's).
WORKS_IDS = {
    'timber_rainwood': 'logging_rainwood', 'timber_oakhaven': 'logging_oakhaven', 'coppice_rainwood': 'coppice_rainwood',
    'charcoal_greenholt': 'coppice_greenholt', 'peat_fens': 'peat_fens',
}
WORK_SPOTS = 6           # A works' work spots: open ground nearest its centre, in its centre's cell.
FENSHORE = dict(id='fenshore', kind='town', name='Fenshore', near=(900, 3450), search=240, size=40,
                plan='community (the fen farms\' hands)',
                about='a hamlet on the fens\' last dry ground, among the eel creeks, the salterns, the dunes\' warren and '
                      'the coast\'s landings')

COTTAGE_PROSE = {
    'country': ['A timber cottage on stone footings: a hearth, a scrubbed table and beds along the walls, the rain loud '
                'on the shingles.',
                'Plain and close under the shingles: a fire that is never let out, a table, and beds for a family pushed '
                'against the walls.',
                'A cottage of split cedar boards, the smell of woodsmoke in everything; boots by the door and beds along '
                'the walls.'],
    'southern': ['A limewashed cottage, cool under its red tiles: a hearth, a long table and beds along the walls.',
                 'Thick white walls and a tiled roof; a table under the window, a hearth, and beds for a family.'],
    'adobe': ['A limewashed adobe house, cool in the heat of the day: a hearth, a table, water jars by the door and beds '
              'along the walls.',
              'Thick mud-brick walls and a flat roof: a hearth, mats and beds for a family, and a water jar in the corner.'],
    'fen': ['A stilted house of tarred timber under reed thatch, its boards creaking: a peat fire, a table and beds along '
            'the walls.',
            'A reed-thatched house on stilts above the wet, the fen mist at the shutters: a hearth, a table, beds for a '
            'family.'],
}
FARMHOUSE_PROSE = {
    'country': 'The farmhouse: a big hearth, a long table for the household and beds for the family along the walls; '
               'muddy boots and a ledger of the farm\'s accounts by the door.',
    'southern': 'The farmhouse, limewashed and cool: a hearth, a long table and beds for the family; strings of onions '
                'and a ledger of the farm\'s accounts by the door.',
    'adobe': 'The farmhouse of thick adobe: a hearth, water jars, a long table and beds for the family; the farm\'s '
             'accounts in a chest by the door.',
    'fen': 'The keeper\'s house on its stilts: a peat fire, a long table, beds for the family and the farm\'s tallies '
           'notched on a stick by the door.',
}
KIND_NAMES = {   # the steading as a whole, and the keeper's home
    'grain': ('Farm', 'Farmhouse'), 'orchard': ('Orchard', 'Orchard House'), 'orchard_dry': ('Orchard', 'Orchard House'),
    'vineyard': ('Vineyard', 'Vineyard House'), 'dairy': ('Dairy Farm', 'Farmhouse'), 'pigs': ('Piggery', 'Farmhouse'),
    'sheep': ('Fold', 'Farmhouse'), 'warren': ('Warren', 'Warren House'), 'boat': ('Landing', 'Cottage'),
    'eels': ('Marsh', 'Cottage'), 'bees': ('Bee Garth', 'Cottage'), 'salt_pan': ('Workings', 'Cottage'),
    'salt_boil': ('Salterns', 'Cottage'),
}


# ---------------------------------------------------------------------------------------------------------------
# Interiors
# ---------------------------------------------------------------------------------------------------------------
def more_work(room, n):
    """Work spots for n posts in a room: its own first, then the free floor nearest them (off the way in from the
    door, never a bed or a stair)."""
    if len(room.work) >= n:
        return
    rows = room.rows
    H, W = len(rows), len(rows[0])
    taken = set(map(tuple, room.work)) | set(map(tuple, room.beds))
    dx, dy = room.door if room.door else (W // 2, H - 1)
    first = room.work[0] if room.work else (W // 2, H // 2)
    free = [(x, y) for y in range(1, H - 1) for x in range(1, W - 1)
            if rows[y][x] in '.f' and (x, y) not in taken and abs(x - dx) + abs(y - dy) > 2
            and all(rows[y + b][x + a] != '^' for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1)))]
    free.sort(key=lambda p: (abs(p[0] - first[0]) + abs(p[1] - first[1]), p[1], p[0]))
    for p in free:
        if len(room.work) >= n:
            break
        if all(abs(p[0] - q[0]) + abs(p[1] - q[1]) >= 2 for q in room.work):
            room.work.append(p)


def one(kind, name, style, p, text):
    return Building(kind, name, style, fp(p.w, p.h), [p.room('', name, text)])


def farmhouse(rng, style, name):
    p = house(rng, style, HOUSEHOLD_MOST)
    return one('house', name, style, p, FARMHOUSE_PROSE[style])


def cottage(rng, style, name):
    p = house(rng, style, HOUSEHOLD_MOST)
    return one('house', name, style, p, rng.choice(COTTAGE_PROSE[style]))


def granary(rng, style, name):
    s = STYLES[style]
    w, h = 20, 12
    p = Plan(w, h, s['wall'], s['floor'], rng)
    for y in (1, 2):
        for x in range(1, w - 1):
            if x % 5:
                p.put(x, y, 'x')
    for x in range(2, w - 2):
        if x % 4 and x not in (w // 2, w // 2 - 1):
            p.put(x, 5, 'O')
    p.run(1, h - 2, 1, 0, 3, 'x')
    for spot in ((w // 2, 4), (4, 4), (w - 5, 4), (w // 2 - 3, 7)):
        p.worker(*spot)
    return one('works', name, style, p, f'{name}: bins of grain and stacked sacks of flour and oats to the rafters, '
               'the floor swept for the cats, the harvest kept here and sold from store all year.')


def drying_house(rng, style, name):
    s = STYLES[style]
    w, h = 16, 10
    p = Plan(w, h, s['wall'], s['floor'], rng)
    for x in range(2, w - 2, 2):
        p.station(x, 1, 'drying_rack')
    p.station(1, h // 2, 'hearth')
    p.run(3, 5, 1, 0, w - 6, '|')
    p.put(w - 2, h - 2, 'x')
    p.put(w - 3, h - 2, 'O')
    for spot in ((w // 2, 3), (4, 3), (w - 4, 3)):
        p.worker(*spot)
    return one('works', name, style, p, f'{name}: racks of sliced apples and plums over a slow fire, the air sweet '
               'and smoky; crates of dried fruit by the door.')


def fruit_store(rng, style, name):
    s = STYLES[style]
    w, h = 16, 10
    p = Plan(w, h, s['wall'], s['floor'], rng)
    for x in range(1, w - 1):
        if x % 4:
            p.put(x, 1, 'x')
    p.station(2, 4, 'press')
    for x in range(5, w - 2):
        if x % 3:
            p.put(x, 4, 'O')
    for spot in ((w // 2, 3), (3, 6)):
        p.worker(*spot)
    return one('works', name, style, p, f'{name}: apples laid in straw in their crates, a cider press, and barrels of '
               'the year\'s cider; the smell of fruit is everywhere.')


def press_house(rng, style, name):
    p = works_kit(rng, style, 'press_house', 3)
    s = STYLES[style]
    down = Plan(p.w, p.h, s['wall'], s['floor'], random.Random(f'{name}:cellar'))
    down.g[p.h - 1][p.w // 2] = s['wall']
    down.keep = set()
    sx, sy = 1, p.h - 4
    stairs_between(p, down, sx, sy)
    for y in range(2, p.h - 2, 3):
        for x in range(3, p.w - 2):
            down.put(x, y, 'O')
    down.worker(p.w // 2, 1 if down.free(p.w // 2, 1) else 3)
    rooms = [p.room('', name, f'{name}: a great screw press and treading vats, baskets of grapes in the season and '
                    'casks along the walls.'),
             down.room('cellar', f'{name}, the cellar', 'A cool cellar under the press house: casks of the year\'s wine '
                       'in rows in the dark, and the smell of oak and must.', z=-1,
                       lighting={'artificial': .35, 'daylightAccess': 0, 'tone': 'cool'})]
    return Building('works', name, style, fp(p.w, p.h), rooms, [(('', sx, sy), ('cellar', sx, sy))])


def byre(rng, style, name):
    p = works_kit(rng, style, 'barn', 3)
    return one('works', name, style, p, f'{name}: stalls for the cows along the back, hay to the rafters and the warm '
               'smell of the herd; the milking stools hang by the door.')


def dairy_room(rng, style, name):
    p = works_kit(rng, style, 'dairy', 3)
    return one('works', name, style, p, f'{name}: churns and cheese presses, the butter worked on cold slabs, and '
               'rounds of cheese ripening on the shelves.')


def pig_yard(rng, style, name):
    s = STYLES[style]
    w, h = 18, 11
    p = Plan(w, h, s['wall'], s['floor'], rng)
    for x in range(1, w - 1):
        if x % 4 == 0:
            for y in (1, 2, 3):
                p.put(x, y, '|')
        elif x % 4 == 2:
            p.put(x, 1, 'z')
        elif x % 4 == 1:
            p.put(x, 3, 'O')
    p.station(w - 3, h - 3, 'butcher_block')
    for spot in ((w // 2, 5), (3, 5), (w - 4, 5), (w // 2, h - 3)):
        p.worker(*spot)
    return one('works', name, style, p, f'{name}: sties of hurdles along the back, straw and troughs, sows and their '
               'litters; a block for the killing season by the door.')


def shearing_barn(rng, style, name):
    p = works_kit(rng, style, 'barn', 3)
    return one('works', name, style, p, f'{name}: lambing pens of hurdles, a shearing floor, bales of fleece and hay to '
               'the rafters; the goats\' milking stand in the corner.')


def rabbitry(rng, style, name):
    p = works_kit(rng, style, 'rabbitry', 2)
    return one('works', name, style, p, f'{name}: hutches stacked along the walls, sacks of greens and a skinning '
               'bench by the door; nets and ferrets\' boxes for the warren.')


def boat_shed(rng, style, name):
    s = STYLES[style]
    w, h = 16, 10
    p = Plan(w, h, s['wall'], s['floor'], rng)
    p.run(3, 3, 1, 0, w - 6, '<')                     # (a boat on trestles)
    for x in range(1, w - 1, 3):
        p.put(x, 1, 'x')
    p.put(w - 2, 1, 'O')
    p.station(2, 6, 'workbench')
    p.put(w - 3, 6, 'O')
    for spot in ((w // 2, 5), (4, 5), (w - 5, 5)):
        p.worker(*spot)
    return one('works', name, style, p, f'{name}: the boat drawn up on trestles for caulking, nets hung to dry from '
               'the beams, creels and barrels of salt for the catch.')


def eel_house(rng, style, name):
    p = works(rng, style, 'smokehouse', 2)
    return one('works', name, style, p, f'{name}: eel traps of withy stacked by the door, tanks of live eels, and '
               'racks of eels and fowl over the smouldering peat.')


def hive_shed(rng, style, name):
    p = works_kit(rng, style, 'bee_shed', 2)
    return one('works', name, style, p, f'{name}: straw skeps on shelves, smokers and veils, and crocks of honey '
               'sealed with wax.')


def salt_store(rng, style, name):
    s = STYLES[style]
    w, h = 16, 10
    p = Plan(w, h, s['wall'], s['floor'], rng)
    for x in range(1, w - 1):
        if x % 3:
            p.put(x, 1, 'x')
            p.put(x, 2, 'x')
    p.put(1, 5, 'y')
    p.put(w - 2, 5, 'O')
    for spot in ((w // 2, 4), (4, 4), (w - 5, 4)):
        p.worker(*spot)
    return one('works', name, style, p, f'{name}: grey heaps of raked salt drying in sacks, rakes and shovels on the '
               'wall, and the dust of it on everything.')


def boiling_house(rng, style, name):
    p = works_kit(rng, style, 'saltworks', 3, size=(18, 11))
    return one('works', name, style, p, f'{name}: shallow iron pans over slow peat fires, the brine let in from the sea '
               'pans outside boiling down to grey salt; steam and smoke under the reed roof.')


def water_mill(rng, style, name):
    p = works_kit(rng, style, 'mill', 2)
    return one('works', name, style, p, f'{name}: a pair of millstones turned by the wheel on the river outside, flour '
               'in the air like fog and the farm\'s sacks waiting to be ground.')


# A steading's buildings by kind: (key, builder, role, the building's word). The first is the keeper's home.
STEADING_KINDS = {
    'grain': [('granary', granary, 'work', 'Granary')],
    'orchard_dry': [('drying', drying_house, 'work', 'Drying House')],
    'orchard': [('store', fruit_store, 'work', 'Apple Store')],
    'vineyard': [('press', press_house, 'work', 'Press House')],
    'dairy': [('byre', byre, 'work', 'Byre'), ('dairy', dairy_room, 'work', 'Dairy')],
    'pigs': [('sties', pig_yard, 'work', 'Pig Yard')],
    'sheep': [('barn', shearing_barn, 'work', 'Shearing Barn')],
    'warren': [('rabbitry', rabbitry, 'work', 'Rabbitry')],
    'boat': [('shed', boat_shed, 'work', 'Boat Shed')],
    'eels': [('eelhouse', eel_house, 'work', 'Eel House')],
    'bees': [('hives', hive_shed, 'work', 'Hive Shed')],
    'salt_pan': [('store', salt_store, 'work', 'Salt Store')],
    'salt_boil': [('boiling', boiling_house, 'work', 'Boiling House')],
}


# ---------------------------------------------------------------------------------------------------------------
# Placing: a window of the realm as a canvas, its buildings stamped with Site, written back
# ---------------------------------------------------------------------------------------------------------------
class Window(Site):
    """A box of the realm as a canvas: buildings placed on it get their links in the realm's cells; ids are given
    (`key`) under a prefix, never made from names."""

    def __init__(self, realm, x0, y0, w, h, prefix, ids):
        from .realm2 import SIZE, CELL, cell_id
        x0, y0 = max(0, x0), max(0, y0)
        w, h = min(w, SIZE - x0), min(h, SIZE - y0)
        self.x0, self.y0, self.w, self.h = x0, y0, w, h
        c = Canvas(w, h)
        c.codes[:] = realm.codes[y0:y0 + h, x0:x0 + w]
        c.heights[:] = realm.h[y0:y0 + h, x0:x0 + w]
        cells = []
        for cy in range(y0 // CELL, (y0 + h - 1) // CELL + 1):
            for cx in range(x0 // CELL, (x0 + w - 1) // CELL + 1):
                cells.append((cell_id(cx, cy), cx * CELL - x0, cy * CELL - y0, CELL, CELL))
        super().__init__(c, cells)
        self.realm = realm
        self.prefix = prefix
        self.ids = ids
        self.key = None

    def unique(self, name):
        bid = f'{self.prefix}{self.key}'
        assert bid not in self.ids and len(bid) <= 36, bid
        self.ids.add(bid)
        return bid

    def put(self, key, b, x, y, facing, region, ground, open_door=True):
        """Place building `b` with its block's top-left at window (x, y); returns its manifest record."""
        self.key = key
        self.place(b, x, y, facing, region, open_door=open_door, ground=ground)
        rec = self.manifest[-1]
        return rec

    def back(self):
        """Write the canvas back into the realm, and the rooms' places on the map into world coordinates."""
        r, x0, y0 = self.realm, self.x0, self.y0
        r.codes[y0:y0 + self.h, x0:x0 + self.w] = self.canvas.codes
        r.h[y0:y0 + self.h, x0:x0 + self.w] = self.canvas.heights
        for room in self.rooms:
            room['worldX'] += x0
            room['worldY'] += y0
        for rec in self.manifest:
            rec['door'] = {'x': rec['door']['x'] + x0, 'y': rec['door']['y'] + y0}

    def world(self, x, y):
        return x + self.x0, y + self.y0


def turn_rect(x, y, w, h, W, H, k):
    """A rectangle in a W x H box after k quarter turns clockwise (as buildings.rotate turns rows)."""
    for _ in range(k % 4):
        x, y, w, h, W, H = H - y - h, x, h, w, H, W
    return x, y, w, h


TURN_FACING = {'S': 'W', 'W': 'N', 'N': 'E', 'E': 'S'}


def turn_facing(f, k):
    for _ in range(k % 4):
        f = TURN_FACING[f]
    return f


def steading_layout(buildings, yard=(14, 9)):
    """A yard with the home on its north side (door south), the first work building on its east (door west), a second
    on its west (door east); a kitchen garden below; the gate in the yard's south side. Returns (box w, h, parts) in a
    box whose top-left is (0, 0): parts are ('yard'|'garden'|'gate'|building index, x, y, w, h, facing)."""
    YW, YH = yard
    sizes = [b.footprint for b in buildings]
    YW = max(YW, sizes[0][0] + 2)
    YH = max(YH, max([s[0] for s in sizes[1:]] or [0]) - 2)
    parts = [('yard', 0, 0, YW, YH, '')]
    w0, h0 = sizes[0]
    parts.append((0, YW // 2 - w0 // 2, -h0, w0, h0, 'S'))
    if len(sizes) > 1:
        fw, fh = sizes[1]
        parts.append((1, YW, YH // 2 - fw // 2, fh, fw, 'W'))
    if len(sizes) > 2:
        fw, fh = sizes[2]
        parts.append((2, -fh, YH // 2 - fw // 2, fh, fw, 'E'))
    parts.append(('garden', 1, YH + 2, min(10, YW // 2 - 2), 6, ''))
    parts.append(('gate', YW // 2, YH, 1, 1, ''))
    x0 = min(p[1] for p in parts) - 2
    y0 = min(p[2] for p in parts) - 2
    x1 = max(p[1] + p[3] for p in parts) + 2
    y1 = max(p[2] + p[4] for p in parts) + 2
    return x1 - x0, y1 - y0, [(p[0], p[1] - x0, p[2] - y0, p[3], p[4], p[5]) for p in parts]


def turned_layout(layout, k):
    W, H, parts = layout
    out = []
    for name, x, y, w, h, f in parts:
        nx, ny, nw, nh = turn_rect(x, y, w, h, W, H, k)
        out.append((name, nx, ny, nw, nh, turn_facing(f, k) if f else ''))
    return (W, H) if k % 2 == 0 else (H, W), out


def integral(a):
    s = np.zeros((a.shape[0] + 1, a.shape[1] + 1), dtype=np.float64)
    s[1:, 1:] = a.astype(np.float64).cumsum(0).cumsum(1)
    return s


def box_sums(s, w, h):
    """Sums over every w x h box (indexed by its top-left) of the integral image s."""
    return s[h:, w:] - s[:-h, w:] - s[h:, :-w] + s[:-h, :-w]


class Places:
    """The step itself: everything it builds, and the contract."""

    def __init__(self, realm, report=print):
        from . import realm2 as R
        self.R, self.realm, self.report = R, realm, report
        self.ids = set()
        self.rooms, self.links = [], []
        self.built = np.zeros(realm.codes.shape, dtype=bool)     # (tiles of compounds and communities' cores)
        self.held = np.zeros(realm.codes.shape, dtype=bool)      # (what footpaths leave alone: the same)
        self.zone = np.zeros(realm.codes.shape, dtype=np.int16)  # (each compound's or core's number, for routing)
        self.zones = 0
        self.steadings, self.communities = {}, {}
        self.paths = []
        self.site_index = {s['id']: i for i, s in enumerate(realm.sites, 1)}
        self.sites = {s['id']: s for s in realm.sites}
        # Each farm site's crew (country_farms.json), so its workplace has a place for each of them to work.
        cf = json.loads((ROOT / 'Data' / 'Economy' / 'country_farms.json').read_text(encoding='utf-8'))
        self.crew = {FARMS[f['id']]: f['crew']['keepers'] + f['crew']['hands'] for a in cf['areas'] if not a.get('works')
                     for f in a['farms']}

    # -- Fenshore's ground ------------------------------------------------------------------------------------------
    def place_fenshore(self):
        R, r = self.R, self.realm
        spec = FENSHORE
        size = spec['size']
        G = R.G
        wc = R.down(r.water.astype(np.float32), G)
        hc = R.down(r.h, G)
        gy, gx = np.gradient(hc)
        slope = np.hypot(gx, gy) / G
        heldc = R.down(r.hold.astype(np.float32), G, 'max') > 0
        marsh = R.down(r.biomes['marsh'].astype(np.float32), G)
        xs, ys = R.grid_xy(R.NG, G)
        rr = max(1, size // G)
        tx, ty = spec['near']
        d = np.hypot(xs - tx, ys - ty)
        # The fens are threaded with creeks and pools: the hamlet's core goes where the dry ground is widest (its middle
        # at least 14 tiles from water), near the five fen farms.
        S = r.codes.shape[0]
        reach = spec['search'] + 64
        y0, x0 = max(0, ty - reach) // G * G, max(0, tx - reach) // G * G
        far = np.zeros((R.NG, R.NG), dtype=np.float32)
        dw = R.dist_to(r.water[y0:ty + reach, x0:tx + reach], 64).astype(np.float32)
        sub = R.down(dw[:dw.shape[0] // G * G, :dw.shape[1] // G * G], G, 'min')
        far[y0 // G:y0 // G + sub.shape[0], x0 // G:x0 // G + sub.shape[1]] = sub
        score = R.blur(slope, max(2, rr // 2)) * 40 + R.blur(marsh, rr) * 4 + d / spec['search'] * 6 - np.minimum(far, 48) / 3
        crowd = R.spread(r.taken, R.site_kernel('town', size))
        score[(d > spec['search']) | heldc | crowd | (far < 14)] = np.inf
        j = int(np.argmin(score))
        if not np.isfinite(score.ravel()[j]):
            raise SystemExit('Fenshore: no dry ground for it near the fen farms')
        cy, cx = divmod(j, R.NG)
        site = {'id': spec['id'], 'kind': 'town', 'name': spec['name'], 'x': int(cx * G + G // 2),
                'y': int(cy * G + G // 2), 'size': size, 'for': spec['plan'], 'about': spec['about']}
        r.prepare(site)
        r.sites.append(site)
        self.site_index[site['id']] = len(r.sites)
        self.sites[site['id']] = site
        # Its footpath to the nearest road (as every reserved site has, doc 70).
        cost = r.route_cost()
        roadc = R.down(r.road_mask.astype(np.float32), G, 'max') > 0
        pts = R.route_to(cost, (site['x'], site['y']), roadc)
        if pts and len(pts) > 1:
            r.road('track:fenshore', pts, 2, ends=(site.get('level'), None), ch='/')
            site['track'] = len(pts)
        self.report(f'  Fenshore at {site["x"]}, {site["y"]} (level {site.get("level")}, {site["tiles"]} tiles)')

    # -- steadings: where each stands ------------------------------------------------------------------------------
    def find_spot(self, layout, target, search, own, need_water=None, margin=4):
        """The best place and turn for a compound near target: dry, unbuilt, off roads, on its own site's ground or
        none's, as level as can be. need_water: (part, distance) a part's back must be within `distance` of water."""
        r = self.realm
        S = r.codes.shape[0]
        best = None
        for k in range(4):
            (W, H), parts = turned_layout(layout, k)
            m = margin
            tx, ty = target
            x0, y0 = max(0, tx - search - W - m), max(0, ty - search - H - m)
            x1, y1 = min(S, tx + search + W + m), min(S, ty + search + H + m)
            win = np.s_[y0:y1, x0:x1]
            gof = r.ground_of[win]
            bad = (r.water[win] | r.hold[win] | r.keep[win] | r.road_mask[win] | self.built[win] |
                   ((gof != 0) & (gof != own)) | (r.codes[win] == ord('8')) | (r.codes[win] == ord('%')))
            hv = r.h[win].astype(np.float64)
            bw, bh = W + 2 * m, H + 2 * m
            if bad.shape[0] < bh or bad.shape[1] < bw:
                continue
            nb = box_sums(integral(bad), bw, bh)
            s1 = box_sums(integral(hv), W, H)[m:m + nb.shape[0], m:m + nb.shape[1]]
            s2 = box_sums(integral(hv * hv), W, H)[m:m + nb.shape[0], m:m + nb.shape[1]]
            n = W * H
            std = np.sqrt(np.maximum(0, s2 / n - (s1 / n) ** 2))
            yy, xx = np.mgrid[0:nb.shape[0], 0:nb.shape[1]]
            cx = x0 + xx + m + W / 2
            cy = y0 + yy + m + H / 2
            dist = np.hypot(cx - tx, cy - ty)
            score = dist + std * 30
            score[(nb > 0) | (dist > search) | (std > 1.0)] = np.inf
            if need_water is not None:
                score[~self.water_behind(parts, need_water, x0 + m, y0 + m, nb.shape)] = np.inf
            j = int(np.argmin(score))
            if np.isfinite(score.ravel()[j]) and (best is None or score.ravel()[j] < best[0]):
                yi, xi = divmod(j, nb.shape[1])
                best = (float(score.ravel()[j]), x0 + xi + m, y0 + yi + m, k)
        return best

    def water_behind(self, parts, need, ox, oy, shape):
        """For each candidate top-left (ox + i, oy + j), whether the back of part `need[0]` lies within need[1] tiles of
        water (a mill's wheel on its river)."""
        r = self.realm
        name, dist = need
        part = next(p for p in parts if p[0] == name)
        _, x, y, w, h, f = part
        back = {'S': (x + w // 2, y - 1, 0, -1), 'N': (x + w // 2, y + h, 0, 1),
                'E': (x - 1, y + h // 2, -1, 0), 'W': (x + w, y + h // 2, 1, 0)}[f]
        bx, by, dx, dy = back
        ok = np.zeros(shape, dtype=bool)
        H, W = shape
        S = r.codes.shape[0]
        for step in range(dist):
            X0, Y0 = ox + bx + dx * step, oy + by + dy * step
            sub = r.water[max(0, Y0):min(S, Y0 + H), max(0, X0):min(S, X0 + W)]
            if sub.shape == shape:
                ok |= sub
        return ok

    def level_compound(self, x0, y0, W, H, margin=10):
        """The compound's box at one level (the median of its ground), the land round it eased over `margin` tiles;
        water, the city's held ground and roads untouched."""
        r = self.realm
        S = r.codes.shape[0]
        level = float(half(np.median(r.h[y0:y0 + H, x0:x0 + W])))
        X0, Y0 = max(0, x0 - margin), max(0, y0 - margin)
        X1, Y1 = min(S, x0 + W + margin), min(S, y0 + H + margin)
        yy, xx = np.mgrid[Y0:Y1, X0:X1]
        dx = np.maximum(0, np.maximum(x0 - xx, xx - (x0 + W - 1)))
        dy = np.maximum(0, np.maximum(y0 - yy, yy - (y0 + H - 1)))
        d = np.hypot(dx, dy)
        blend = 1 - field.smoothstep(0, margin, d)
        win = np.s_[Y0:Y1, X0:X1]
        free = ~r.water[win] & ~r.hold[win] & ~r.road_mask[win]
        hv = r.h[win]
        hv[free] = (hv * (1 - blend) + level * blend)[free]
        box = np.s_[y0:y0 + H, x0:x0 + W]
        r.h[box][~r.water[box]] = level
        # Trees, scrub, rocks and cliffs off the compound and a little round it.
        cv = r.codes[win]
        clear = (d <= 2) & free & np.isin(cv, [ord(c) for c in '$PY1Bo7?@2%&X'])
        cv[clear] = ord(',') if r.wet[y0, x0] >= .34 else ord(';')
        return level

    def plan_steading(self, site_id):
        stem, kind, style, community = STEADINGS[site_id]
        site = self.sites[site_id]
        rng = random.Random(f'{SEED}:{site_id}')
        whole, home_word = KIND_NAMES[kind]
        name = f'{stem} {whole}'
        builds = [('home', farmhouse(rng, style, f'{stem} {home_word}'), 'home')]
        for key, fn, role, word in STEADING_KINDS[kind]:
            builds.append((key, fn(rng, style, f'{stem} {word}'), role))
        work_rooms = [b.rooms[0] for _, b, role in builds if role == 'work']
        more_work(work_rooms[0], self.crew[site_id] - sum(len(r.work) for r in work_rooms[1:]))
        layout = steading_layout([b for _, b, _ in builds])
        target = (site['x'], site['y'])
        search = 70 if kind in ('boat', 'salt_boil') else 90
        best = self.find_spot(layout, target, search, self.site_index[site_id])
        if best is None:
            best = self.find_spot(layout, target, search * 2, self.site_index[site_id])
        if best is None:
            raise SystemExit(f'steading {site_id}: no ground for it near {target}')
        _, x0, y0, k = best
        (W, H), parts = turned_layout(layout, k)
        level = self.level_compound(x0, y0, W, H)
        zone = self.claim(np.s_[y0:y0 + H, x0:x0 + W])
        gate = next(p for p in parts if p[0] == 'gate')
        st = {'site': site_id, 'name': name, 'kind': kind, 'style': style, 'community': community, 'box': (x0, y0, W, H),
              'level': level, 'parts': parts, 'builds': builds, 'gate': (x0 + gate[1], y0 + gate[2]), 'turn': k,
              'zone': zone}
        self.steadings[site_id] = st
        if site_id in MILLS:
            self.plan_mill(site_id, st)
        return st

    def plan_mill(self, farm_id, st):
        stem, _, style, community = STEADINGS[farm_id]
        mill = self.sites[MILLS[farm_id]]
        rng = random.Random(f'{SEED}:{mill["id"]}')
        b = water_mill(rng, style, f'{stem} Mill')
        # The mill on its river: a small yard in front (door south of it), its back to the water.
        fw, fh = b.footprint
        parts = [(0, 0, 0, fw, fh, 'S'), ('yard', 0, fh, fw, 5, ''), ('gate', fw // 2, fh + 4, 1, 1, '')]
        layout = (fw, fh + 5, parts)
        best = None
        for search in (40, 80, 140):
            best = self.find_spot(layout, (mill['x'], mill['y']), search, self.site_index[mill['id']],
                                  need_water=(0, 3), margin=0)
            if best:
                break
        if best is None:
            raise SystemExit(f'{mill["id"]}: no bank for the mill near {mill["x"]}, {mill["y"]}')
        _, x0, y0, k = best
        (W, H), tparts = turned_layout(layout, k)
        level = self.level_compound(x0, y0, W, H, margin=6)
        zone = self.claim(np.s_[y0:y0 + H, x0:x0 + W])
        gate = next(p for p in tparts if p[0] == 'gate')
        st['mill'] = {'site': mill['id'], 'box': (x0, y0, W, H), 'level': level, 'parts': tparts, 'build': b,
                      'gate': (x0 + gate[1], y0 + gate[2]), 'turn': k, 'zone': zone}

    def gate_lane(self, part):
        """A footpath from the compound's gate straight out past its edge (the footpath to the road goes on from there);
        the gate moved to its end."""
        r = self.realm
        gx, gy = part['gate']
        dx, dy = STEP[turn_facing('S', part['turn'])]
        x0, y0, W, H = part['box']
        x, y = gx, gy
        while x0 - 1 <= x <= x0 + W and y0 - 1 <= y <= y0 + H:
            if not r.water[y, x] and not r.hold[y, x]:
                r.codes[y, x] = ord('/')
                r.h[y, x] = part['level']
            x, y = x + dx, y + dy
        part['lane'] = (gx, gy, x - dx, y - dy)

    def claim(self, box, mask=None):
        """A compound's or core's ground: built on (no other compound here), held (no footpath painted or eased across
        it), and its own zone for routing."""
        self.zones += 1
        sub = np.ones(self.built[box].shape, dtype=bool) if mask is None else mask
        self.built[box] |= sub
        self.held[box] |= sub
        self.zone[box][sub] = self.zones
        return self.zones

    # -- paths ---------------------------------------------------------------------------------------------------
    def lay_path(self, name, a, b, ends, cost, held, join_roads=True):
        """A footpath from a to b over the coarse grid, along roads and tracks where they go (painted only off them);
        returns its length in tiles (the route's, roads included)."""
        R, r = self.R, self.realm
        pts = R.route(cost, a, b)
        length = int(sum(math.dist(p, q) for p, q in zip(pts, pts[1:])))
        samples = R.densify(pts, 1.0)
        S = r.codes.shape[0]
        near_road = []
        for x, y in samples:
            xi, yi = int(x), int(y)
            sub = r.road_mask[max(0, yi - 3):yi + 4, max(0, xi - 3):xi + 4]
            near_road.append(bool(sub.any()) and not held[min(S - 1, yi), min(S - 1, xi)])
        # Runs off the roads, each carried a few tiles onto the road it joins.
        runs, i = [], 0
        n = len(samples)
        while i < n:
            if near_road[i]:
                i += 1
                continue
            j = i
            while j < n and not near_road[j]:
                j += 1
            runs.append((max(0, i - 4), min(n, j + 4)))
            i = j
        old_hold, old_locked = r.hold, r.locked
        r.hold, r.locked = old_hold | held, old_locked | held
        try:
            for k, (i, j) in enumerate(runs):
                if j - i < 3:
                    continue
                e = (ends[0] if i == 0 else None, ends[1] if j == n else None)
                r.road(f'path:{name}:{k}', samples[i:j], 2, ends=e, ch='/')
        finally:
            r.hold, r.locked = old_hold, old_locked
        self.paths.append({'name': name, 'length': length, 'from': [int(a[0]), int(a[1])], 'to': [int(b[0]), int(b[1])]})
        return length

    # -- stamping a steading -------------------------------------------------------------------------------------
    def stamp_steading(self, st):
        r = self.realm
        x0, y0, W, H = st['box']
        win = Window(r, x0 - 4, y0 - 4, W + 8, H + 8, f'fs_{st["site"]}_', self.ids)
        ox, oy = x0 - win.x0, y0 - win.y0
        c = win.canvas
        level = st['level']
        region = st['community']
        yard_ch = '.'
        out = {'site': st['site'], 'name': st['name'], 'kind': st['kind'], 'community': st['community'],
               'buildings': []}
        for part in st['parts']:
            name, x, y, w, h, f = part
            if name == 'yard':
                c.rect(ox + x, oy + y, w, h, yard_ch, level)
            elif name == 'garden':
                self.garden(c, ox + x, oy + y, w, h, level, 'kitchen garden')
                out['garden'] = self.garden_record(win, ox + x, oy + y, w, h, 'kitchen garden')
        for part in st['parts']:
            name, x, y, w, h, f = part
            if not isinstance(name, int):
                continue
            key, b, role = st['builds'][name]
            rec = win.put(key, b, ox + x, oy + y, f, region, level, open_door=True)
            out['buildings'].append(self.building_record(win, rec, role, b))
        win.back()
        self.rooms += win.rooms
        self.links += win.links
        if 'mill' in st:
            m = st['mill']
            mx, my, mW, mH = m['box']
            mw = Window(r, mx - 4, my - 4, mW + 8, mH + 8, f'fs_{st["site"]}_', self.ids)
            ox, oy = mx - mw.x0, my - mw.y0
            for part in m['parts']:
                name, x, y, w, h, f = part
                if name == 'yard':
                    mw.canvas.rect(ox + x, oy + y, w, h, '.', m['level'])
            for part in m['parts']:
                name, x, y, w, h, f = part
                if name == 0:
                    rec = mw.put('mill', m['build'], ox + x, oy + y, f, region, m['level'])
                    out['buildings'].append(self.building_record(mw, rec, 'work', m['build']))
                    self.wheel(mw, ox + x, oy + y, w, h, f)
            mw.back()
            self.rooms += mw.rooms
            self.links += mw.links
        return out

    def wheel(self, win, x, y, w, h, facing):
        """The water wheel on the mill's back wall, in the river (a race of shallow water to it if the bank is wide)."""
        r = self.realm
        bx, by, dx, dy = {'S': (x + w // 2, y - 1, 0, -1), 'N': (x + w // 2, y + h, 0, 1),
                          'E': (x - 1, y + h // 2, -1, 0), 'W': (x + w, y + h // 2, 1, 0)}[facing]
        c = win.canvas
        for step in range(3):
            X, Y = bx + dx * step, by + dy * step
            wx, wy = win.world(X, Y)
            if r.water[wy, wx]:
                c.codes[Y, X] = ord('}')
                for s2 in range(step):
                    c.codes[by + dy * s2, bx + dx * s2] = ord('~')
                    c.heights[by + dy * s2, bx + dx * s2] = float(r.h[wy, wx])
                return

    def jetty(self, st):
        """A landing's jetty: planks from the shore by the site out over the water, about ten tiles."""
        r = self.realm
        site = self.sites[st['site']]
        x, y = site['x'], site['y']
        S = r.codes.shape[0]
        R = 14
        sub = r.water[max(0, y - R):y + R + 1, max(0, x - R):x + R + 1]
        ys, xs = np.nonzero(sub)
        if not len(xs):
            return None
        wx, wy = xs.mean() + max(0, x - R), ys.mean() + max(0, y - R)
        ang = math.atan2(wy - y, wx - x)
        dx, dy = math.cos(ang), math.sin(ang)
        deck = float(half(r.h[y, x]))
        laid = []
        for i in range(0, 26):
            px, py = x + dx * i, y + dy * i
            for ox, oy in ((0, 0), (-dy, dx)):
                tx, ty = int(round(px + ox)), int(round(py + oy))
                if not (0 <= tx < S and 0 <= ty < S) or r.hold[ty, tx]:
                    continue
                if r.water[ty, tx] or r.codes[ty, tx] in (ord('0'), ord('D'), ord('E')):
                    r.codes[ty, tx] = ord('8')
                    r.h[ty, tx] = deck
                    laid.append((tx, ty))
            if sum(1 for tx, ty in laid if r.water[ty, tx]) >= 20:
                break
        return {'x': x, 'y': y, 'tiles': len(laid)}

    def garden(self, c, x, y, w, h, level, kind):
        """A fenced plot: crops (a kitchen garden), bare earth with a hen house (a hen run), grass with a goat shed (a
        goat paddock); a gap in the fence on its south side."""
        inner = {'kitchen garden': '4', 'hen run': '.', 'goat paddock': ','}[kind]
        c.rect(x, y, w, h, inner, level)
        c.outline(x, y, w, h, '|', level)
        c.codes[y + h - 1, x + w // 2] = ord(inner)
        if kind == 'kitchen garden':
            for xx in range(x + 1, x + w - 1):
                c.codes[y + h // 2, xx] = ord('.')            # (a path between the beds)
        elif kind == 'hen run':
            c.stamp(x + 1, y + 1, ['#L#' if i == 1 else '###' for i in range(3)][:3], level)
        else:
            c.stamp(x + w - 5, y + 1, ['####', '#LL#', '####'], level)

    def garden_record(self, win, x, y, w, h, kind):
        wx, wy = win.world(x, y)
        cid, lx, ly = win.cell_at(x, y)
        return {'kind': kind, 'cell': cid, 'x': lx, 'y': ly, 'width': w, 'height': h, 'worldX': wx, 'worldY': wy,
                'tiles': int((w - 2) * (h - 2))}

    def building_record(self, win, rec, role, b):
        dx, dy = rec['door']['x'], rec['door']['y']
        cid, lx, ly = win.cell_at(dx, dy)
        rooms = []
        for room in rec['rooms']:
            rooms.append({'area': room['id'], 'name': room['name'], 'z': room['z'], 'beds': room['beds'],
                          'work': room['work'], 'stations': room['stations']})
        return {'id': rec['id'], 'name': rec['name'], 'role': role, 'kind': b.kind, 'trade': b.trade,
                'door': {'cell': cid, 'x': lx, 'y': ly, 'worldX': dx + win.x0, 'worldY': dy + win.y0},
                'rooms': rooms}

    # -- communities ---------------------------------------------------------------------------------------------
    def plan_community(self, spec):
        site = self.sites[spec['site']]
        core = 44 if spec['id'] != 'fenshore' else 36
        cx, cy = site['x'], site['y']
        S = self.realm.codes.shape[0]
        box = np.s_[max(0, cy - core):cy + core + 1, max(0, cx - core):cx + core + 1]
        yy, xx = np.mgrid[box]
        zone = self.claim(box, np.hypot(xx - cx, yy - cy) <= core)
        self.built[max(0, cy - core - 16):cy + core + 17, max(0, cx - core - 16):cx + core + 17] = True
        self.communities[spec['id']] = {'spec': spec, 'site': site, 'core': core, 'zone': zone}

    def stamp_community(self, spec):
        r = self.realm
        info = self.communities[spec['id']]
        site, core = info['site'], info['core']
        cx, cy = site['x'], site['y']
        R0 = 96
        win = Window(r, cx - R0, cy - R0, 2 * R0 + 1, 2 * R0 + 1, f'cn_{spec["id"]}_', self.ids)
        c = win.canvas
        lx, ly = cx - win.x0, cy - win.y0
        rng = random.Random(f'{SEED}:{spec["id"]}')
        H, W = c.codes.shape
        yy, xx = np.mgrid[0:H, 0:W]
        dist = np.hypot(xx - lx, yy - ly)
        box = np.s_[win.y0:win.y0 + H, win.x0:win.x0 + W]
        water = r.water[box]
        hold = r.hold[box]
        level = float(site.get('level', half(np.median(c.heights[dist < 20]))))
        fen = spec['id'] == 'fenshore'
        lift = .5 if fen else 0.0
        if fen:
            # Fenshore stands on stilts: its boardwalks and houses may cross the bog pools (never a creek, river or the
            # sea), a half step over the hamlet's dry ground.
            water = water & ~(r.pool[box] & ~r.river[box] & ~r.creek[box] & ~r.sea[box])
        street_ch = spec['street']
        # The streets: from the green to every road and footpath that reaches the core's edge.
        way = np.isin(c.codes, [ord(ch) for ch in 'd/8_f']) & (dist > core - 6) & (dist < core + 18) & ~water
        street = np.zeros((H, W), dtype=bool)
        ends = []
        lab = self.R.components4(way)
        for v in np.unique(lab[way]):
            ys, xs = np.nonzero(lab == v)
            k = int(np.argmin(np.hypot(xs - lx, ys - ly)))
            ends.append((int(xs[k]), int(ys[k])))
        if len(ends) < 2:                                    # (at least a cross of lanes)
            for ang in (0, math.pi / 2, math.pi, 3 * math.pi / 2):
                ends.append((int(lx + math.cos(ang) * (core - 4)), int(ly + math.sin(ang) * (core - 4))))
        for ex, ey in ends:
            street |= c.line_mask([(ex + .5, ey + .5), (lx + .5, ly + .5)], 3)
        # A lane round the green, the green itself (grass and its well).
        green = (np.abs(xx - lx) <= 6) & (np.abs(yy - ly) <= 4)
        ring = (np.abs(xx - lx) <= 9) & (np.abs(yy - ly) <= 7) & ~green
        street |= ring
        # A lane round the houses: an oval loop out from the green, crossing the streets.
        rho = np.hypot((xx - lx) / 1.25, yy - ly)
        for loop in ((19, 32) if spec['cottages'] <= 4 else (19, 35)):
            street |= (np.abs(rho - loop) < 1.0)
        street &= ~water & ~hold
        heights = c.heights.copy()
        c.paint(street, street_ch, np.full(heights.shape, level + lift, np.float32) if fen else heights)
        c.paint(green, ',' if spec['style'] != 'adobe' else '.', heights)
        c.stamp(lx + 3, ly - 1, ['U'], float(c.heights[ly - 1, lx + 3]))      # (the well; the green's middle stays open)
        if fen:
            # The hamlet's wet ground between the boardwalks: mud and reeds where it isn't kept dry.
            wet = (dist < core + 10) & ~street & ~green & ~water & ~hold & np.isin(c.codes, [ord(ch) for ch in ',";'])
            pick = np.random.default_rng(SEED).random((H, W))
            c.codes[wet & (pick < .18)] = ord('E')
            c.codes[wet & (pick >= .18) & (pick < .3)] = ord('D')
        # Lots: the core's open ground, a door onto a street.
        open_ground = (dist < core + 10) & ~street & ~green & ~water & ~hold & \
            ~np.isin(c.codes, [ord(ch) for ch in ('d/8_fWU%' if fen else 'd/8_fWw~U%')])
        open_ground &= ~self.R.SOLID[c.codes] | np.isin(c.codes, [ord(ch) for ch in '$PY1Bo7?@2'])
        lots = Lots(open_ground, street, gap=2)
        style = spec['style']
        out = {'id': spec['id'], 'name': spec['name'], 'site': spec['site'], 'style': style,
               'centre': {'x': cx, 'y': cy}, 'about': spec['about'], 'homes': [], 'market': [], 'gardens': []}
        wanted = []
        for entry in spec['market']:
            key, kind, name, text, posts = entry[:5]
            flat = entry[5] if len(entry) > 5 else True
            b = self.market_building(rng, style, kind, name, text, flat)
            more_work(b.rooms[0], sum(n for _, n in posts))
            wanted.append((key, b, 'market', posts))
        for i in range(spec['cottages']):
            name = f'{spec["name"]} Cottage {i + 1}' if style != 'fen' else f'{spec["name"]} Stilt House {i + 1}'
            wanted.append((f'cottage_{i + 1}', cottage(rng, style, name), 'home', None))
        missing = []
        for key, b, role, posts in wanted:
            fw, fh = b.footprint
            near = (lx, ly) if role == 'market' else (lx + rng.randint(-24, 24), ly + rng.randint(-20, 20))
            spot = next(((x0, y0, facing, w, h) for _, x0, y0, facing, w, h in lots.candidates(fw, fh, near, limit=6000)
                         if lots.clear(x0, y0, w, h)), None)
            if spot is None:
                missing.append(b.name)
                continue
            x0, y0, facing, w, h = spot
            ground = level + lift if fen else float(np.median(c.heights[y0:y0 + h, x0:x0 + w]))
            rec = win.put(key, b, x0, y0, facing, spec['id'], float(half(ground)), open_door=role != 'home')
            lots.take(x0, y0, w, h)
            br = self.building_record(win, rec, role, b)
            if role == 'home':
                out['homes'].append(br)
            else:
                br['posts'] = [{'post': p, 'count': n} for p, n in posts]
                br['business'] = {'store': 'general', 'inn': 'inn', 'alehouse': 'inn', 'bakehouse': 'bakery'}[
                    next(e[1] for e in spec['market'] if e[0] == key)]
                out['market'].append(br)
        if missing:
            self.failed = (c, win)
            raise SystemExit(f'{spec["name"]}: no room for {", ".join(missing)}')
        # Food of its own: kitchen gardens, a hen run, a goat paddock, round the edge of the village (the core's lots
        # kept for houses later), on dry open ground clear of the buildings, streets and paths.
        from .cities import grow
        trees = np.isin(c.codes, [ord(ch) for ch in '$PY1Bo7?@2'])
        building = self.R.SOLID[c.codes] & ~trees
        ways = np.isin(c.codes, [ord(ch) for ch in 'd/8_f+U'])
        g_open = (dist > core - 8) & (dist < core + 44) & ~water & ~hold & ~grow(building, 3) & ~grow(ways, 2) & \
            ~grow(street | green, 1)
        g_lots = Lots(g_open, street, gap=1)
        near_water = None
        if spec.get('by_water'):                       # (Saltwell's gardens round the oasis pool, watered from it)
            ys, xs = np.nonzero(r.water[box] & (dist < 120))
            if len(xs):
                near_water = (int(xs.mean()), int(ys.mean()))
        plots = [('kitchen garden', 12, 8)] * spec['gardens'] + [('hen run', 9, 7), ('goat paddock', 13, 9)]
        for kind, gw, gh in plots:
            spot = self.free_rect(g_lots, gw, gh, near_water or (lx, ly), rng, 28 if near_water else core)
            if spot is None:
                self.failed = (c, win)
                raise SystemExit(f'{spec["name"]}: no room for a {kind}')
            x0, y0 = spot
            g_level = float(half(np.median(c.heights[y0:y0 + gh, x0:x0 + gw])))
            self.garden(c, x0, y0, gw, gh, g_level, kind)
            g_lots.take(x0, y0, gw, gh)
            lots.take(x0, y0, gw, gh)
            out['gardens'].append(self.garden_record(win, x0, y0, gw, gh, kind))
        # Room to grow: how many more cottages the core's free lots would take (the user: "if we need to expand the
        # communities and add more houses we can").
        building = self.R.SOLID[c.codes] & ~trees
        fenced = np.isin(c.codes, [ord(ch) for ch in '|4'])
        ways = np.isin(c.codes, [ord(ch) for ch in 'd/8_f'])
        free = (dist < core + 30) & ~water & ~hold & ~grow(building | fenced, 2) & ~ways & ~green & ~street
        spare = Lots(free, street | ways, gap=2)
        n = 0
        while n < 40:
            spot = next(((x0, y0, w, h) for _, x0, y0, facing, w, h in spare.candidates(12, 9, (lx, ly), limit=3000)
                         if spare.clear(x0, y0, w, h)), None)
            if spot is None:
                break
            spare.take(*spot)
            n += 1
        out['roomToGrow'] = {'cottageLots': n, 'openTiles': int(lots.open.sum())}
        win.back()
        self.rooms += win.rooms
        self.links += win.links
        return out

    def market_building(self, rng, style, kind, name, text, flat=True):
        if kind == 'store':
            rooms, stairs, (w, h) = shop_with_flat(rng, style, 'general', name, text, people=4)
            return Building('shop', name, style, fp(w, h), rooms, stairs, trade='general')
        if kind == 'bakehouse':
            if flat:
                rooms, stairs, (w, h) = shop_with_flat(rng, style, 'baker', name, text, people=4)
                return Building('shop', name, style, fp(w, h), rooms, stairs, trade='baker')
            p = shop(rng, style, 'baker')
            return Building('shop', name, style, fp(p.w, p.h), [p.room('', name, f'{name}, bakery. {text}')],
                            trade='baker')
        inn = kind == 'inn'
        plans, stairs = tavern(rng, style, inn)
        rooms = [plans[0].room('', name, text)]
        pairs = []
        if inn:
            rooms.append(plans[1].room('up', f'{name}, upstairs', 'Small rooms under the roof for travellers, a pallet '
                                       'and a blanket in each, and a jug of water.', z=1))
            _, sx, sy = stairs[0]
            pairs.append((('', sx, sy), ('up', sx, sy)))
        else:
            # The alehouse keeper's family over the taproom.
            up, (sx, sy) = flat_over(random.Random(f'{name}:flat'), style, plans[0], 3)
            rooms.append(up.room('flat', f'{name}, the rooms above', 'The keeper\'s family\'s rooms over the taproom: '
                                 'a hearth, a table and beds, and the noise of the evening coming up through the boards.',
                                 z=1))
            pairs.append((('', sx, sy), ('flat', sx, sy)))
        return Building('inn' if inn else 'tavern', name, style, fp(plans[0].w, plans[0].h), rooms, pairs)

    @staticmethod
    def free_rect(lots, w, h, centre, rng, core):
        H, W = lots.open.shape
        cx, cy = centre
        best = None
        for _ in range(4000):
            ang = rng.random() * 2 * math.pi
            rad = core * (.8 + rng.random() * 1.1)
            x0, y0 = int(cx + math.cos(ang) * rad - w / 2), int(cy + math.sin(ang) * rad - h / 2)
            if lots.clear(x0 - 1, y0 - 1, w + 2, h + 2):
                d = math.hypot(x0 + w / 2 - cx, y0 + h / 2 - cy)
                if best is None or d < best[0]:
                    best = (d, x0, y0)
        return None if best is None else best[1:]

    # -- the whole step ------------------------------------------------------------------------------------------
    def run(self, project):
        R, r = self.R, self.realm
        # Ids already in the world (but our own, made again here) are never reused.
        self.ids |= {x['id'] for x in project['rooms'] if not is_ours(x['id'])}
        self.ids |= {x['id'] for x in project['links'] if not is_ours(x['id'])}
        self.place_fenshore()
        for spec in COMMUNITIES:
            self.plan_community(spec)
        for site_id in STEADINGS:
            self.plan_steading(site_id)
        # Footpaths: each steading (and mill) to the nearest road or track; each community to each of its farms.
        cost = r.route_cost()
        G, NG = R.G, R.NG
        # (The Saltpan's crust is walked on: only open water, and the city's held ground, bar a footpath.)
        dry = (R.down(r.water.astype(np.float32), G) <= .15) & ~(R.down(r.hold.astype(np.float32), G, 'max') > 0)
        cost[np.isinf(cost) & dry] = 3.0
        zc = self.zone.reshape(NG, G, NG, G).max(axis=(1, 3))

        def only(base, *allowed):                         # (no route through another compound or core)
            c = base.copy()
            c[(zc > 0) & ~np.isin(zc, allowed)] = np.inf
            return c
        held = self.held
        for site_id, st in self.steadings.items():
            for which, part in (('steading', st), ('mill', st.get('mill'))):
                if not part:
                    continue
                gx, gy = part['gate']
                self.gate_lane(part)
                roadc = (R.down(r.road_mask.astype(np.float32), G, 'max') > 0) & (zc == 0)
                pts = R.route_to(only(cost, part['zone']), (gx + .5, gy + .5), roadc)
                if pts and len(pts) > 1:
                    old_hold, old_locked = r.hold, r.locked
                    r.hold, r.locked = old_hold | held, old_locked | held
                    try:
                        r.road(f'path:{site_id}:{which}', [(gx + .5, gy + .5)] + pts[1:], 2,
                               ends=(part['level'], None), ch='/')
                    finally:
                        r.hold, r.locked = old_hold, old_locked
        roadc = R.down(r.road_mask.astype(np.float32), G, 'max') > 0
        cheap = np.where(roadc, cost * .3, cost)
        walks = {}
        for site_id, st in self.steadings.items():
            com = st['community']
            if com == 'ridgemere':
                continue
            cinfo = self.communities[com]
            csite = cinfo['site']
            gx, gy = st['gate']
            walks[site_id] = self.lay_path(f'{com}:{site_id}', (csite['x'] + .5, csite['y'] + .5), (gx + .5, gy + .5),
                                           (csite.get('level'), st['level']), only(cheap, cinfo['zone'], st['zone']), held)
            if 'mill' in st:
                m = st['mill']
                mx, my = m['gate']
                walks[MILLS[site_id]] = self.lay_path(f'{com}:{MILLS[site_id]}', (csite['x'] + .5, csite['y'] + .5),
                                                      (mx + .5, my + .5), (csite.get('level'), m['level']),
                                                      only(cheap, cinfo['zone'], m['zone']), held)
        self.walks = walks
        # The buildings.
        farms_out = {}
        for site_id, st in self.steadings.items():
            farms_out[site_id] = self.stamp_steading(st)
            if st['kind'] == 'boat':
                farms_out[site_id]['jetty'] = self.jetty(st)
        coms = {}
        for spec in COMMUNITIES:
            coms[spec['id']] = self.stamp_community(spec)
            self.sites[spec['site']]['built'] = spec['about']
        # The works' footpaths (doc 72), last: after the communities are laid out, so a path reaching a village makes
        # no new street there and its houses stand where they did (their beds are the placed people's).
        for site_id, com in WORKS.items():
            if com == 'ridgemere':
                continue
            site, cinfo = self.sites[site_id], self.communities[com]
            csite = cinfo['site']
            walks[site_id] = self.lay_path(f'{com}:{site_id}', (csite['x'] + .5, csite['y'] + .5),
                                           (site['x'] + .5, site['y'] + .5), (csite.get('level'), site.get('level')),
                                           only(cheap, cinfo['zone']), held)
        self.farms_out, self.coms = farms_out, coms
        # The world's rooms and links: ours made again, the rest as they were.
        project['rooms'] = [x for x in project['rooms'] if not is_ours(x['id'])] + self.rooms
        project['links'] = [x for x in project['links'] if not is_ours(x['id'])] + self.links
        self.report(f'  places: {len(self.steadings)} steadings, {len(coms)} communities, {len(self.rooms)} interiors, '
                    f'{len(self.links)} links, {len(self.paths)} footpaths')
        self.contract = self.manifest()
        return self.contract

    def work_spots(self, site_id):
        """A works' place of work out of doors (doc 72): its centre's cell, and the WORK_SPOTS tiles of open ground nearest
        its centre in that cell (at least 3 apart), each local to the cell."""
        s = self.sites[site_id]
        cx, cy = s['x'], s['y']
        CELL = self.R.CELL
        X, Y = cx // CELL, cy // CELL
        codes = self.realm.codes
        cand = []
        for dy in range(-10, 11):
            for dx in range(-10, 11):
                x, y = cx + dx, cy + dy
                if x // CELL == X and y // CELL == Y and open_ground(codes[y, x]):
                    cand.append((dx * dx + dy * dy, x - X * CELL, y - Y * CELL))
        cand.sort()
        picked = []
        for _, x, y in cand:
            if all(abs(x - a) + abs(y - b) >= 3 for a, b in picked):
                picked.append((x, y))
            if len(picked) == WORK_SPOTS:
                break
        return {'area': self.R.cell_id(X, Y), 'name': s['name'], 'work': [{'x': x, 'y': y} for x, y in picked]}

    # -- the checks (realm2's, with the realm's walk) -------------------------------------------------------------
    def checks(self, lab, main):
        """(name, ok, detail) for each: the doors reached, each farm's in one walk with its community, the homes' sizes,
        the hands where the user put them, the ids and the words."""
        import re
        c = self.contract
        S = lab.shape[0]

        def label_at(door):
            x, y = door['worldX'], door['worldY']
            for dx, dy in ((0, 0), (0, 1), (0, -1), (1, 0), (-1, 0)):
                if 0 <= y + dy < S and 0 <= x + dx < S and lab[y + dy, x + dx] >= 0:
                    return int(lab[y + dy, x + dx])
            return -1
        doors = []
        for f in c['farms']:
            doors.append((f['farm'], f['door'], f['community']))
            for b in self.farms_out[f['site']]['buildings']:
                doors.append((b['id'], b['door'], f['community']))
        for com in c['communities']:
            for h in com['homes']:
                doors.append((h['area'], h['door'], com['id']))
            for m in com['market']:
                doors.append((m['area'], m['door'], com['id']))
        lost = [i for i, d, _ in doors if label_at(d) != main]
        out = [('the steadings\' and communities\' doors all reached from the city', not lost, ', '.join(lost[:10]))]
        # Each farm's workplace in one walk with its community's homes (or Ridgemere's spawn).
        apart = []
        for f in c['farms']:
            work = [b for b in self.farms_out[f['site']]['buildings'] if b['role'] == 'work']
            if f['community'] == 'ridgemere':
                homes = [main]
            else:
                com = next(k for k in c['communities'] if k['id'] == f['community'])
                homes = {label_at(h['door']) for h in com['homes']}
            if any(label_at(b['door']) not in homes for b in work):
                apart.append(f['farm'])
        out.append(('every farm\'s workplaces in one walk with its community\'s homes', not apart, ', '.join(apart)))
        big = [h['area'] for com in c['communities'] for h in com['homes'] + com['flats'] if h['capacity'] > HOUSEHOLD_MOST]
        big += [f['home']['area'] for f in c['farms'] if f['home']['capacity'] != HOUSEHOLD_MOST]
        out.append(('homes for at most six, farmhouses for six', not big, ', '.join(big)))
        want = {'coldwater': 21, 'larkfield': 11, 'southmeadow': 8, 'fenshore': 8, 'saltwell': 5, 'oakhaven': 0}   # (a fibre hand at each grain farm, a vine hand fewer at each vineyard: doc 72)
        got = {com['id']: com['hands'] for com in c['communities']}
        out.append(('the hired hands where the user put them (21, 11, 8, 8, 5, Oakhaven none; 23 in Ridgemere)',
                    got == want and c['ridgemere']['hands'] == 23, f'{got}, Ridgemere {c["ridgemere"]["hands"]}'))
        ids = [x['id'] for x in self.rooms + self.links]
        long_ = [i for i in ids if not re.fullmatch(r'[a-z][a-z0-9_-]{0,47}', i)]
        out.append(('the places\' ids are world ids (48 characters at most)', not long_ and len(ids) == len(set(ids)),
                    ', '.join(long_[:5])))
        text = ' '.join(x['name'] + ' ' + x.get('description', '') for x in self.rooms) + json.dumps(c)
        out.append(('no word "dog" in the places\' text', not re.search(r'\bdogs?\b', text.lower()), ''))
        return out

    # -- the contract --------------------------------------------------------------------------------------------
    def manifest(self):
        cf = json.loads((ROOT / 'Data' / 'Economy' / 'country_farms.json').read_text(encoding='utf-8'))
        farms = []
        hands = {}
        for area in cf['areas']:
            if area.get('works'):
                continue                             # (Doc 72's works: `works`, below.)
            for f in area['farms']:
                site_id = FARMS[f['id']]
                st = self.farms_out[site_id]
                home = next(b for b in st['buildings'] if b['role'] == 'home')
                work = [b for b in st['buildings'] if b['role'] == 'work']
                crew = f['crew']
                hands[st['community']] = hands.get(st['community'], 0) + crew['hands']
                farms.append({
                    'farm': f['id'], 'kind': f['kind'], 'area': area['id'], 'site': site_id,
                    'steading': st['name'], 'community': st['community'],
                    'keepers': crew['keepers'], 'hands': crew['hands'],
                    'workplace': work[0]['rooms'][0]['area'],
                    'workplaces': [{'area': room['area'], 'name': room['name'], 'work': room['work'],
                                    'stations': room['stations']} for b in work for room in b['rooms'] if room['work']],
                    'home': {'area': home['rooms'][0]['area'], 'name': home['name'], 'capacity': len(home['rooms'][0]['beds']),
                             'beds': home['rooms'][0]['beds']},
                    'door': home['door'],
                    'garden': st.get('garden'),
                    'jetty': st.get('jetty'),
                    'walk': self.walks.get(site_id),
                    'mapWins': MAP_WINS.get(f['id'], ''),
                })
        coms = []
        for spec in COMMUNITIES:
            c = self.coms[spec['id']]
            homes = [{'area': b['rooms'][0]['area'], 'name': b['name'], 'capacity': len(b['rooms'][0]['beds']),
                      'beds': b['rooms'][0]['beds'], 'door': b['door']} for b in c['homes']]
            market = []
            flats = []
            for b in c['market']:
                rooms = b['rooms']
                market.append({'area': rooms[0]['area'], 'name': b['name'], 'business': b['business'],
                               'posts': b['posts'], 'work': rooms[0]['work'], 'stations': rooms[0]['stations'],
                               'door': b['door'], 'otherRooms': [x['area'] for x in rooms[1:]]})
                for room in rooms[1:]:
                    if room['beds'] and not room['area'].endswith('_up'):
                        flats.append({'area': room['area'], 'name': room['name'], 'capacity': len(room['beds']),
                                      'beds': room['beds'], 'for': b['name']})
                    elif room['beds']:
                        market[-1]['guestBeds'] = len(room['beds'])
            served = [f['farm'] for f in farms if f['community'] == spec['id']]
            coms.append({'id': spec['id'], 'name': spec['name'], 'site': spec['site'], 'centre': c['centre'],
                         'about': spec['about'], 'style': spec['style'], 'region': spec['id'],
                         # Not a town yet (the user, 2026-11-03: doc 71, Phase C Progress): a plain place after a restart
                         # too; a later step flips it, and World::setupTowns then makes it a town of its own.
                         'town': False,
                         'hands': hands.get(spec['id'], 0), 'farms': served,
                         'beds': sum(h['capacity'] for h in homes) + sum(f['capacity'] for f in flats),
                         'homes': homes, 'flats': flats, 'market': market, 'gardens': c['gardens'],
                         'roomToGrow': c['roomToGrow']})
        return {
            'format': 'ratw-communities', 'version': 1,
            'about': 'The Country\'s farm steadings and communities as built on the realm (Docs/Design/70, Progress, '
                     '"The steadings and the communities"; doc 71, Phase C), written by tools/worldgen/realm2_places.py '
                     'with each realm write. Homes and workplaces are interiors (world.interiors / world.areas id; the '
                     'project\'s rooms): a resident\'s home is live.npcs home_area = the home\'s `area`, home_x/home_y '
                     'a bed; a post\'s work_area = the workplace `area`, work_x/work_y a `work` spot. At most '
                     f'{HOUSEHOLD_MOST} to a home (HouseholdMost). An interior\'s region is its community '
                     '(`ridgemere` for the farms near the city), so a farm\'s hands and its keeper are "near" it. A '
                     'community\'s `town` (false) keeps it a plain place on a restart, never a town of its own '
                     '(World::setupTowns), until a later step makes it one.',
            'householdMost': HOUSEHOLD_MOST,
            'workLabels': {
                'about': 'The game knows a shop or works by its keeper\'s work label (Data/Items/businesses.json `match`, '
                         'at most 40 characters). A market post\'s label should hold its business\'s word; a farm '
                         'post\'s should hold none of them (or it becomes that city business), so not "dairy", '
                         '"cheesemaker", "water mill", "salt house", "salt pans", "smokehouse", " press", "vintner", '
                         '"brewer", "butcher", "baker", "tavern", "taproom", "keeps the ".',
                'general': 'shopkeeper at <store>', 'inn': 'keeps the taproom at <inn or alehouse>',
                'bakery': 'baker at <bakehouse>',
                'farm': 'e.g. "keeps Longmead Farm", "farmhand at Longmead", "boatman at Rivermouth", "eel-catcher"'},
            'ridgemere': {'farms': [f['farm'] for f in farms if f['community'] == 'ridgemere'],
                          'hands': hands.get('ridgemere', 0),
                          'note': 'These farms\' hands live in Ridgemere\'s own homes and lodgings and walk out; their '
                                  'keepers\' families live at the steadings.'},
            'communities': coms,
            'farms': farms,
            # Doc 72's works (places only: no work rooms, posts or goods yet): the site, its crew's home, the walk.
            # Doc 72's works: the site, its crew's home, the walk; a built works' id in country_farms.json and its work
            # spots out of doors (no work room: its posts' work_area is the cell, work_x/work_y a spot).
            'works': [{'site': k, 'name': self.sites[k]['name'], 'kind': self.sites[k]['kind'], 'community': com,
                       'centre': {'x': self.sites[k]['x'], 'y': self.sites[k]['y']}, 'size': self.sites[k]['size'],
                       'walk': self.walks.get(k), 'works': WORKS_IDS.get(k),
                       'workplace': self.work_spots(k) if k in WORKS_IDS else None} for k, com in WORKS.items()],
            'paths': self.paths,
        }


def open_ground(code):
    """A tile one may stand and work on: open ground, grass or a low feature, not solid and not a way."""
    import terrain_catalog
    t = terrain_catalog.BY_CODE.get(chr(int(code)))
    return bool(t) and t['kind'] in ('floor', 'grass', 'feature') and not t['solid'] and not t['way']


def is_ours(ident):
    return ident.startswith(('fs_', 'cn_'))


def lay(realm, project, report=print):
    """The steadings and the communities on the realm just laid (before its cells are cut): changes realm.codes and
    realm.h, adds Fenshore to realm.sites, puts the interiors and links into `project`; returns the contract."""
    import time
    t0 = time.time()
    places = Places(realm, report)
    out = places.run(project)
    realm.places = places
    realm.timings['places'] = round(time.time() - t0, 1)
    report(f'  places: {realm.timings["places"]} s')
    return out


PREVIEW_STEADINGS = ['grain_long', 'vineyard_ochre', 'fishing_glasswater', 'marsh_eels', 'dairy_southgate',
                     'sheep_downs']


def previews(realm, folder):
    """Each community close up (four pixels a tile), a few steadings (and Longmead's mill), and a map of the
    communities, each with a line to each of its farms."""
    from PIL import ImageDraw, ImageFont
    from . import realm2 as R
    folder.mkdir(parents=True, exist_ok=True)
    c = realm.contract
    for com in c['communities']:
        x, y = com['centre']['x'], com['centre']['y']
        R.picture(realm.codes, realm.h, box=(x - 90, y - 90, 180, 180), scale=4, relief=1.0).save(folder / f'{com["id"]}.png')
    farms = {f['site']: f for f in c['farms']}
    for site in PREVIEW_STEADINGS:
        d = farms[site]['door']
        R.picture(realm.codes, realm.h, box=(d['worldX'] - 45, d['worldY'] - 45, 90, 90), scale=5,
                  relief=1.0).save(folder / f'steading-{site}.png')
    mill = next(b for b in realm.places.farms_out['grain_long']['buildings'] if b['id'].endswith('_mill'))
    d = mill['door']
    R.picture(realm.codes, realm.h, box=(d['worldX'] - 40, d['worldY'] - 40, 80, 80), scale=5,
              relief=1.0).save(folder / 'steading-grain_long-mill.png')
    # The map: communities (white), steadings by community (coloured), the footpaths laid.
    img = R.picture(realm.codes, realm.h, scale=.25, relief=1.3)
    dr = ImageDraw.Draw(img)
    font = ImageFont.truetype(R.FONT, 11)
    colours = {'coldwater': (80, 200, 255), 'larkfield': (255, 220, 60), 'southmeadow': (255, 140, 60),
               'fenshore': (120, 255, 160), 'saltwell': (255, 120, 220), 'oakhaven': (170, 120, 255),
               'ridgemere': (230, 230, 230)}
    # Doc 72's works: a diamond each, a dotted line to its community.
    for w in c.get('works', []):
        x, y = w['centre']['x'] / 4, w['centre']['y'] / 4
        if w['community'] != 'ridgemere':
            com = next(k for k in c['communities'] if k['id'] == w['community'])
            dr.line([(com['centre']['x'] / 4, com['centre']['y'] / 4), (x, y)], fill=colours[w['community']], width=1)
        dr.polygon([(x, y - 5), (x + 5, y), (x, y + 5), (x - 5, y)], outline=(0, 0, 0), fill=colours[w['community']])
    for f in c['farms']:
        d = f['door']
        x, y = d['worldX'] / 4, d['worldY'] / 4
        if f['community'] != 'ridgemere':
            com = next(k for k in c['communities'] if k['id'] == f['community'])
            dr.line([(com['centre']['x'] / 4, com['centre']['y'] / 4), (x, y)], fill=colours[f['community']], width=1)
        dr.rectangle([x - 3, y - 3, x + 3, y + 3], outline=(0, 0, 0), fill=colours[f['community']])
    for com in c['communities']:
        x, y = com['centre']['x'] / 4, com['centre']['y'] / 4
        dr.ellipse([x - 6, y - 6, x + 6, y + 6], outline=(0, 0, 0), fill=colours[com['id']])
        dr.text((x + 8, y - 8), f'{com["name"]} ({com["beds"]} beds, {com["hands"]} hands)', fill=(255, 255, 255),
                font=font, stroke_width=2, stroke_fill=(0, 0, 0))
    dr.text((20, 20), 'Ridgemere (its farms\' hands live in the city); squares farms, diamonds works',
            fill=(255, 255, 255), font=font, stroke_width=2, stroke_fill=(0, 0, 0))
    img.save(folder / 'communities-and-farms.png')


def write_contract(manifest, path=CONTRACT):
    path.write_text(json.dumps(manifest, indent=1, ensure_ascii=False) + '\n', encoding='utf-8')
