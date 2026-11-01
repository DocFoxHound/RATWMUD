"""Ridgemere 2.0's people (Docs/Design/67, section 5): who is placed in the city, their homes, their work and their
evening places, as a plan of businesses and roles (labels are rendered from the interiors' names when it is written).

Used by worldgen.ridgemere2 (`place`, `write`). Nobody is made or removed: the unplaced keep everything but their places.
"""
from __future__ import annotations

import json
import math
import random
import re
from collections import Counter, defaultdict
from pathlib import Path

import terrain_catalog

from .residents import MARKS

ROOT = Path(__file__).resolve().parents[2]
ANALYSIS = ROOT / 'artifacts' / 'ridgemere2' / 'analysis'
ITEMS = ROOT / 'Data' / 'Items'
SOLID = set(terrain_catalog.SOLID)

KEPT_MANSIONS = ('carlisle', 'thornbury', 'vance', 'stanmore', 'grantham')
ESTATE_HOMES = {
    'estate_mossgrave': ('rm_mossgrave_court', 'rm_mossgrave_court_up', 'rm_mossgrave_court_down',
                         'rm_the_mossgrave_gatehouse', 'rm_mossgrave_cottages'),
    'estate_fell': ('rm_fellhall', 'rm_fellhall_up', 'rm_fellhall_down', 'rm_the_fell_gatehouse', 'rm_fell_cottages'),
    'estate_grayrock': ('rm_grayrock_hall', 'rm_grayrock_hall_up', 'rm_grayrock_hall_down', 'rm_the_grayrock_gatehouse',
                        'rm_grayrock_cottages'),
}
SEAT_HOUSE = {'rm_greatly_house': 'greatly', 'rm_mossgrave_court': 'mossgrave', 'rm_fellhall': 'fell',
              'rm_grayrock_hall': 'grayrock'}

# --- Labels (doc 67, section 4) ---------------------------------------------------------------------------------
# B's new business types, as the plan gives them, for this branch until Data/Items/businesses.json has them: inserted
# before `inn`, and `builders_works` before `smithy`.
STUB_BUSINESSES = [
    ('builders_works', 'works', ['greatly works', "builders' forge"], 'smithy'),
    ('distillery', 'works', ['distiller', 'still house'], 'inn'),
    ('preserver', 'works', ['preserving house', 'preserver'], 'inn'),
    ('vintner', 'shop', ['vintner', 'wine vault', 'wine merchant'], 'inn'),
    ('shipwright', 'works', ['shipwright', 'slipway'], 'inn'),
    ('laundry', 'works', ['washhouse', 'laundr'], 'inn'),
    ('bathhouse', 'works', ['bathhouse', 'bath house'], 'inn'),
    ('barber', 'shop', ['barber'], 'inn'),
    ('pawnbroker', 'shop', ['pawn'], 'inn'),
    ('rag_and_bone', 'yard', ['rag-and-bone', 'rag and bone'], 'inn'),
]
NO_TILL = {'bathhouse', 'laundry', 'shipwright'}       # Businesses with no till (an institution) or nothing to sell.
CLERGY_WORDS = ('chapel', 'priest', 'acolyte', 'cathedral', 'choir', 'prelate', 'shrine', 'chaplain', 'sexton', 'deacon',
                'abbey', 'temple', 'church', 'the mass', 'confession', 'the crypt', 'healer')
FOOD_PRODUCERS = ('farm', 'orchard', 'vineyard', 'apiary', 'dairy_herd', 'swineherd', 'sheep_farm', 'rabbit_farm',
                  'sheep_pasture', 'fishing', 'eel_traps', 'herb_gathering')
RAW_PRODUCERS = ('logging', 'woodcutting', 'mine', 'copper_pit', 'tin_streams', 'clay_pit', 'quarry', 'peat',
                 'reed_beds', 'charcoal_burning', 'salt_pans')


def businesses():
    """[(id, kind, [match])] in file order, with B's new types stubbed in where they are missing."""
    data = json.loads((ITEMS / 'businesses.json').read_text(encoding='utf-8'))['businesses']
    out = [(b['id'], b.get('kind', ''), [m.lower() for m in b.get('match', [])]) for b in data]
    have = {b[0] for b in out}
    for bid, kind, match, before in STUB_BUSINESSES:
        if bid in have:
            continue
        at = next((i for i, b in enumerate(out) if b[0] == before), len(out))
        out.insert(at, (bid, kind, match))
    return out


_BUSINESSES = None
_PRODUCERS = None


def business_for(label):
    """The business a work label names: the first match in file order, lower case, by substring (Core's businessFor)."""
    global _BUSINESSES
    if _BUSINESSES is None:
        _BUSINESSES = businesses()
    t = label.lower()
    for bid, _, match in _BUSINESSES:
        if any(m in t for m in match):
            return bid
    return None


def producer_for(label):
    global _PRODUCERS
    if _PRODUCERS is None:
        crafts = json.loads((ITEMS / 'crafts.json').read_text(encoding='utf-8'))
        _PRODUCERS = [(p['id'], [m.lower() for m in p.get('match', [])]) for p in crafts.get('producers', [])]
    t = label.lower()
    for pid, match in _PRODUCERS:
        if any(m in t for m in match):
            return pid
    return None


def store_goods(business):
    """item_catalog.store_goods (B builds it): making goods (the inputs of the business's crafts) and resale goods
    (its supplies list). Computed here from crafts.json, for the report's Draws column."""
    crafts = json.loads((ITEMS / 'crafts.json').read_text(encoding='utf-8'))
    making = sorted({item for c in crafts['crafts'] if business in c.get('makers', []) for item in c.get('in', {})})
    resale = sorted(crafts.get('supplies', {}).get(business, []) if isinstance(crafts.get('supplies', {}).get(business), list)
                    else [])
    return making, resale


def is_clergy(label):
    t = label.lower()
    return any(w in t for w in CLERGY_WORDS)


def house_head(label):
    seat = any(w in label for w in (' Hall', ' House', ' Manor', ' Court'))
    t = label.lower()
    return t.startswith('ruling ') or t.startswith('holding court') or (t.startswith('keeping ') and not
                                                                          t.startswith('keeping the ') and seat)


def label_problems(label, intended=None, clergy=False, head=False, food=False, at=None):
    """Section 4's label rules: what this label breaks, given the business it is meant to resolve to (`intended`, a
    keeper's) and the business of the place its holder works at (`at`): a word that names a business is allowed only
    there."""
    out = []
    t = label.lower()
    if not label or len(label) > 40:
        out.append('longer than 40 characters')
    got = business_for(label)
    if intended is not None and got != intended:
        out.append(f'resolves to {got}, not {intended}')
    rules = [('keeps the ', 'inn'), ('feeding the ', 'inn'), ('cooking for', 'inn'), (' press', 'press'),
             ('forge', 'smithy'), ('armour', 'armory'), ('armor', 'armory'), ('bank', 'moneychanger'),
             ('stall', 'stall'), ('stables', 'stables'), ('vats', 'dyeworks')]
    for word, allowed in rules:
        if word in t and allowed not in (at, intended) and not (word == ' press' and got == 'printing_house') \
                and not (word in ('feeding the ', 'cooking for') and food):
            out.append(f'"{word.strip()}" outside its business')
    if producer_for(label) not in (None, 'stock_breeding'):
        out.append(f'matches the producer {producer_for(label)}')
    if house_head(label) and not head:
        out.append('reads as a House head')
    if is_clergy(label) != clergy:
        out.append('clergy word' if not clergy else 'clergy without a clergy word')
    return out


REWORD = [('cooking for the', 'cook to the'), ('cooking for', 'cook to'), ('feeding the', 'serving food to the'),
          ('keeps the ', 'keeping the '), (' stall', ' stand'), ('burns charcoal', 'working'), ('Forge', 'Iron'),
          ('forge', 'ironwork'), ('the bank', 'the counting-house'), ('armour', 'kit'), ('vats', 'tubs'),
          ('stables', 'horses')]


def reword(label):
    """A kept label reworded where it breaks a rule (section 5, Jobs, 1)."""
    for a, b in REWORD:
        label = label.replace(a, b)
    return label


def fit(text, limit=40):
    """A label cut to the limit: "The " dropped first, then the end."""
    if len(text) <= limit:
        return text
    t = text.replace(' The ', ' ').replace(' the ', ' ')
    if len(t) <= limit:
        return t
    return t[:limit].rstrip(" ,'&")


# --- The analysis ---------------------------------------------------------------------------------------------------
def analysis():
    load = lambda name: json.loads((ANALYSIS / name).read_text(encoding='utf-8'))
    return {'selected': load('selected.json'), 'par': load('hh.json')['par'], 'mansions': load('mansions.json'),
            'incity': load('incity.json')}


def households(par):
    out = defaultdict(set)
    for npc, root in par.items():
        out[root].add(npc)
    return out


def placed_ids(project, a=None):
    """Who is placed (doc 67, section 5): the analysts' 512, the Harrowgates (to the King's Residence), and the three
    estates' households, closed over households."""
    a = a or analysis()
    people = {p['id']: p for p in project['people']}
    chosen = set(a['selected']) | set(a['mansions'].get('harrowgate', []))
    for rooms in ESTATE_HOMES.values():
        chosen |= {pid for pid, p in people.items() if p['home'] and p['home']['cell'] in rooms}
    hh = households(a['par'])
    out = set()
    for pid in chosen:
        out |= hh.get(a['par'].get(pid, pid), {pid})
    return out & set(people)


MIDDLE_HOUSES, NORTHGATE_HOUSES = 24, 8


def reused_houses(project, structures, placed, reuse_ids):
    """The old houses kept (doc 67, section 3: 8 clerks', 6 Cinders row houses, and since the Dense city 24 middle-class
    houses, not 40: the rest of their households move into the Commons' row houses of two and three homes): the
    single-room houses most of the placed live in, by where their people work."""
    people = [p for p in project['people'] if p['id'] in placed and p['home']]
    rooms = {r['id']: r for r in project['rooms']}
    by_home = defaultdict(list)
    for p in people:
        by_home[p['home']['cell']].append(p)
    cands = []
    for gid, s in structures.items():
        if gid in reuse_ids or not gid.startswith('rm_') or len(s['rooms']) != 1:
            continue
        name = rooms[gid]['name']
        if not name.endswith(' House') or any(m in gid for m in KEPT_MANSIONS + ('harrowgate', 'greatly')):
            continue
        if len(by_home.get(gid, [])) == 0:
            continue
        works = [p['work']['cell'] if p['work'] else '' for p in by_home[gid]]
        office = sum(1 for w in works if any(k in w for k in ('city_office', 'magistrate', 'hall_of_records',
                                                                 'customs', 'counting_house', 'rain_hall')))
        cinder = sum(1 for w in works if any(k in w for k in ('ironworks', 'foundry', 'sawmill', 'kiln', 'tileworks',
                                                                 'glassworks', 'rag_mill', 'charcoal')))
        cands.append((len(by_home[gid]), gid, office, cinder, s['door']['a_area']))
    cands.sort(key=lambda c: (-c[0], c[1]))
    out = {'admin': [], 'cinders': [], 'commons': [], 'northgate': []}
    for n, gid, office, cinder, cell in cands:
        if office and len(out['admin']) < 8:
            out['admin'].append(gid)
        elif cinder and len(out['cinders']) < 6:
            out['cinders'].append(gid)
        elif len(out['commons']) + len(out['northgate']) < MIDDLE_HOUSES:
            out['northgate' if cell in ('w_ridgemere_1_0',) and len(out['northgate']) < NORTHGATE_HOUSES
                else 'commons'].append(gid)
    for gid in [c[1] for c in cands]:                  # Fill the clerks' and the Cinders' rows from what is left.
        if all(gid not in v for v in out.values()):
            if len(out['admin']) < 8:
                out['admin'].append(gid)
            elif len(out['cinders']) < 6:
                out['cinders'].append(gid)
    return out


# --- Posts: what each business needs (doc 67, section 4) ----------------------------------------------------------
# Kinds: keeper (a merchant with a till, label "{word} at {name}"), help ("helping at {name}"), hands ("working at
# {name}"), staff (a civilian with the label given), guard (a guard with the label given). Each with a count.
CARTERS = 6        # The Carters' Yard's drivers: about six for now (the number the country's trade needs is still to be sized).


def k(word, business=None):
    return ('keeper', word, 1, business)


# --- A few inns, dairies later (doc 67, Progress, "A few inns, dairies later") ----------------------------------------
# The user (2026-10-31), on doc 68's thin margins (15 inns at .39 of their wages, 4 dairies at .41): "Have a few inns, we
# will put in dairies later".
#
# Inns: five tills, one to a quarter (INNS), each a keeper and INN_HELP helpers (the city's hot food goes to fewer
# kitchens). The other taverns keep their buildings and stay the evening places of their streets without a till
# (GATHERING: nobody keeps the tap; the residents still spend their evenings there); their old posts are in GATHERING too,
# for putting a till back (move the key into POSTS). The two soup stalls sell other food (OUTDOOR).
INNS = ('r2_the_knucklebones', 'rm_the_sailors_rest', 'rm_the_grey_gull_inn', 'rm_the_grey_goose', 'rm_the_slag_anchor')
INN_HELP = 2
GATHERING = {
    'rm_the_drowned_lantern': [k('keeping the taproom', 'inn')],
    'rm_the_tarred_rope': [k('keeping the taproom', 'inn')],
    'rm_the_bilge': [k('keeping the taproom', 'inn'), ('help', None, 1)],
    'rm_the_rainbarrel': [k('keeping the taproom', 'inn'), ('help', None, 1)],
    'rm_the_soup_kitchen': [k('ladling soup', 'inn'), ('help', None, 1)],
    'rm_the_black_tun': [k('keeping the taproom', 'inn'), ('help', None, 1)],
    'rm_the_last_lamp': [k('keeping the taproom', 'inn'), ('help', None, 1)],
    'rm_the_slate_ledger': [k('keeping the taproom', 'inn')],
}
# Dairies: none for now. DAIRIES = True puts the four back as they were (two shops, a Market Square stall and a Market
# Hall counter: the dairy's own posts, labels and the shops' names and text in worldgen.ridgemere2.DAIRY_SHOPS); False
# gives each place its stand-in: the two shops are provisioners (their presses stand idle; the rooms are not refitted, so
# the map stays the same), the stall a butcher's and the counter a baker's. Nobody's food depends on them: residents buy
# among the foods the city sells, so cheese drops out and its share goes to the rest (tools/city_capacity.py), and the
# Country's basket has no dairy good.
DAIRIES = False
DAIRY = {      # place: (the dairy, its stand-in)
    'r2_cheesemonger': ([k('cheesemaker in the Commons', 'dairy'), ('help', None, 2)],
                        [k('provisioner', 'provisioner'), ('help', None, 2)]),
    'rm_vane_daughters': ([k('cheesemaker', 'dairy'), ('help', None, 1)],
                          [k('provisioner', 'provisioner'), ('help', None, 1)]),
    'market_square': ('cheesemaker on Market Square', 'sausage butcher on Market Square'),
    'r2_market_hall': (('cheesemaker in the Market Hall', 'dairy'), ('baker in the Market Hall', 'bakery')),
}


def dairy(place):
    """A dairy's posts or label (DAIRY), or its stand-in's while there are no dairies."""
    return DAIRY[place][0 if DAIRIES else 1]


POSTS = {
    # The wharf.
    'rm_the_harbourmaster_s_office': [('staff', 'harbourmaster of the Wharf Office', 1), ('staff', 'clerking at the Wharf Office', 2)],
    'r2_customs_house': [('staff', "collecting the Crown's customs", 1), ('staff', 'weighing at the Customs House', 2)],
    'rm_the_greatly_bonded_warehouse': [('staff', 'keeping the Bonded Warehouse', 1), ('staff', 'tallying in the Bonded Warehouse', 1)],
    'rm_the_fell_timber_store': [('staff', 'keeping the Long Store', 1), ('staff', 'tallying in the Long Store', 1)],
    'r2_new_store': [('staff', 'keeping the New Store', 1), ('staff', 'tallying in the New Store', 1)],
    'r2_grayrock_shipwrights': [k('shipwright', 'shipwright'), ('hands', None, 4)],
    'rm_the_ropewalk': [k('ropemaker at the ropewalk', 'ropewalk'), ('hands', None, 2)],
    'rm_the_bung_hole': [k('provisioner', 'provisioner'), ('help', None, 1)],      # (a ship's provisioner now)
    'rm_wickham_s_chandlery': [k('chandler', 'chandlery'), ('help', None, 1)],
    'rm_the_fish_hall': [k('fishmonger', 'fishmonger'), ('hands', None, 1)],
    'rm_the_north_wharf_smokehouse': [k('runs', 'smokehouse'), ('hands', None, 1)],
    'rm_the_wharf_tannery': [k('tanner', 'tannery'), ('hands', None, 2)],
    'rm_the_sailors_rest': [k('keeping the inn', 'inn'), ('help', None, INN_HELP)],
    # The Cinders.
    'rm_the_grayrock_ironworks': [k('runs', 'ironworks'), ('hands', None, 5)],
    'rm_the_grayrock_foundry': [k('runs', 'foundry'), ('hands', None, 2)],
    'rm_the_fell_sawmill': [k('runs', 'sawmill'), ('hands', None, 3)],
    'rm_the_charcoal_store': [k('runs', 'charcoal'), ('hands', None, 1)],
    'rm_the_grayrock_iron_store': [('staff', 'keeping the Iron Store', 1), ('staff', 'tallying in the Iron Store', 1)],
    'rm_nailers_forge': [k('smith', 'smithy'), ('help', None, 1)],
    'rm_the_anchor_forge': [k('smith', 'smithy'), ('help', None, 1)],
    'r2_forge_row': [k('smith', 'smithy'), ('help', None, 1)],
    'rm_the_rivet_and_rain': [k('armourer', 'armory'), ('help', None, 1)],
    'r2_cinder_wheelwrights': [k('wheelwright', 'cartwright'), ('hands', None, 1)],
    'rm_the_greatly_plasterworks': [k('runs', 'plasterworks'), ('hands', None, 2)],
    'rm_the_lime_kilns': [k('runs', 'brickworks'), ('hands', None, 1)],
    'rm_the_tileworks': [k('runs', 'brickworks'), ('hands', None, 2)],
    'rm_the_cinder_glassworks': [k('runs', 'glassworks'), ('hands', None, 2)],
    'rm_the_rag_mill': [k('runs', 'papermill'), ('hands', None, 1)],
    'rm_the_rainwater_brewery': [k('runs', 'brewery'), ('hands', None, 1)],
    'rm_the_slag_anchor': [k('keeping the taproom', 'inn'), ('help', None, INN_HELP)],
    'r2_cinder_butchers': [k('butcher', 'butcher'), ('help', None, 2)],
    # The Sump.
    'rm_sump_sundries': [k('shopkeeper', 'general'), ('help', None, 1)],
    'rm_gullgut_fish': [k('fishmonger', 'fishmonger'), ('help', None, 1)],
    'rm_harbour_provisions': [k('provisioner', 'provisioner'), ('help', None, 1)],
    'rm_mend_make_do': [k('tinker', 'tinker'), ('help', None, 1)],
    'rm_the_pawn_and_ledger': [k('pawnbroker', 'pawnbroker'), ('help', None, 1)],
    'r2_penny_loaf': [k('baker', 'bakery'), ('help', None, 2)],
    'r2_the_washhouse': [('staff', 'keeping the Washhouse', 1), ('staff', 'washing at the Washhouse', 5)],
    'r2_the_knucklebones': [k('keeping the taproom', 'inn'), ('help', None, INN_HELP)],
    'rm_the_sump_infirmary': [('clergy', "infirmarian of the abbey", 1), ('clergy', 'nursing for the abbey', 1)],
    'rm_the_chapel_of_the_tide': [('clergy', 'priest of the Chapel of the Tide', 1)],
    'r2_lamp_store': [('staff', 'lighting the lamps at dusk', 4)],
    # The abbey.
    'r2_cathedral': [('clergy', 'abbot of the cathedral', 1), ('clergy', 'priest at the cathedral', 2),
                     ('clergy', 'deacon at the cathedral', 1), ('clergy', 'sexton of the cathedral', 1),
                     ('clergy', 'singing in the cathedral choir', 2)],
    'r2_abbey_scriptorium': [('clergy', 'scribe of the abbey', 2)],
    'r2_abbey_brewhouse': [k('brewer at the abbey brewhouse', 'brewery'), ('clergy', 'brother at the abbey brewhouse', 1)],
    'r2_abbey_school': [('clergy', 'teaching at the abbey school', 1)],
    'r2_chapter_house': [('clergy', 'porter of the abbey', 1), ('clergy', 'almoner of the abbey', 1)],
    # The Commons, Northgate and the main market.
    'r2_market_hall': [('staff', 'warden of the Market Hall', 1)],
    'r2_barber': [k('barber', 'barber'), ('help', None, 1)],
    'r2_fell_wine_vaults': [k('vintner', 'vintner'), ('help', None, 1)],
    'r2_cheesemonger': dairy('r2_cheesemonger'),
    'r2_bathhouse': [('staff', 'keeping the Bathhouse', 1), ('staff', 'working at the Bathhouse', 2)],
    'rm_the_eastgate_stables': [k('runs', 'stables'), ('hands', None, 1)],
    'rm_the_commons_school': [('staff', 'teaching letters', 1)],
    'rm_the_house_of_mending': [('clergy', 'healer of the abbey', 2)],
    'rm_the_grey_gull_inn': [k('keeping the inn', 'inn'), ('help', None, INN_HELP)],
    'rm_the_greatly_yard': [k('runs', 'builders_yard'), ('hands', None, 3)],
    # The food rebalance (doc 67, Progress): reused shops given over to food (worldgen.ridgemere2.REFIT refits their
    # rooms), the food shops' hands, and the Carters' Yard.
    'rm_commons_sundries': [k('provisioner', 'provisioner'), ('help', None, 1)],
    'rm_moss_marrow': [k('butcher', 'butcher'), ('help', None, 1)],
    'rm_vane_daughters': dairy('rm_vane_daughters'),
    'rm_the_cough_jar': [k('preserver', 'preserver'), ('help', None, 1)],
    'rm_oilcloth_wool': [k('preserver', 'preserver'), ('help', None, 1)],
    'rm_collar_and_trace': [k('baker', 'bakery'), ('help', None, 1)],
    'rm_rainford_s_store': [k('provisioner', 'provisioner'), ('help', None, 1)],
    'rm_the_grey_goose': [k('cooking for Northgate', 'inn'), ('help', None, INN_HELP)],
    'rm_the_hook_and_cleaver': [k('butcher', 'butcher'), ('help', None, 2)],
    'rm_mutton_lane_butchers': [k('butcher', 'butcher'), ('help', None, 2)],
    'rm_the_wet_slab': [k('fishmonger', 'fishmonger')],
    'rm_southway_provisions': [k('provisioner', 'provisioner')],
    'rm_the_hopyard': [k('brewer', 'brewery')],
    'rm_the_lamp_wick': [k('chandler', 'chandlery'), ('help', None, 2)],
    'r2_carters_yard': [k("keeping the Carters' Yard", 'carters'), ('staff', 'driving for the carters', CARTERS)],
    # House Greatly's compound.
    'rm_greatly_house': [('staff', 'serving at Greatly House', 5)],
    'r2_greatly_forge': [k('builder at the Greatly works', 'builders_works'), ('staff', 'working at the Greatly works', 2)],
    'rm_the_greatly_masons_yard': [k('runs', 'mason'), ('hands', None, 2)],
    'rm_the_greatly_colour_works': [k('runs', 'paintworks'), ('hands', None, 1)],
    'rm_the_greatly_counting_house': [('staff', 'clerking at the Counting House', 3)],
    'rm_the_greatly_stables': [('staff', 'keeping the Greatly stables', 2)],
    # The administrative sector.
    'rm_the_city_office': [('staff', 'running the City Office', 1), ('staff', 'clerking at the City Office', 4)],
    'r2_records_office': [('staff', 'keeping the rolls at the Records Office', 1),
                          ('staff', 'clerking at the Records Office', 4), ('staff', 'copying at the Records Office', 2)],
    'rm_the_magistrate_s_court': [('staff', 'hearing cases', 1), ('staff', 'keeping order in court', 1),
                                  ('staff', "clerking for the magistrate", 1)],
    'rm_the_hall_of_records': [('staff', "carrying for the Couriers' Office", 3)],
    'rm_wetink_lane_scriveners': [k('scribe', 'scriptorium'), ('help', None, 1)],
    # The Great Palace.
    'r2_palace_antechamber': [('staff', 'chancellor of the Council', 1), ('staff', 'usher in the Antechamber', 2),
                              ('staff', 'herald of the Great Palace', 1)],
    'r2_administrative_wing': [('staff', 'royal secretary', 1), ('staff', "the Crown's treasurer", 1),
                               ('staff', 'clerking in the Administrative Wing', 5)],
    'r2_kings_residence': [('staff', "serving in the King's household", 6), ('food', "cooking for the King's household", 2)],
    # The Old Keep and the Watch.
    'r2_the_keep': [('guard', 'commanding the Watch', 1), ('guard', 'sergeant of the Watch', 2),
                    ('guard', 'keeping the cells', 2), ('staff', 'clerking at the Watch Office', 1),
                    ('staff', "keeping the Watch's weapon racks", 1)],
    # The estates.
    'r2_fell_press_house': [k('pressing at the Fell press', 'press'), ('hands', None, 2)],
    'r2_fell_distillery': [k('distiller at the Fell Distillery', 'distillery'), ('hands', None, 2)],
    'r2_fell_preserving_house': [k('preserver at the Fell Preserving House', 'preserver'), ('hands', None, 2)],
    'r2_fell_cellars': [('staff', 'keeping the Fell Cellars', 1)],
}
# Posts that work on a floor of their own: {(structure key, post label): z}. The keeper of the rolls is at the rolls, at
# the top of the three-storey Records Office (doc 67, R18).
POST_FLOOR = {('r2_records_office', 'keeping the rolls at the Records Office'): 2}
# Shops with a flat: a keeper and a helper, the keeper's word from the label it has (kept) or the trade.
SHOP_DEFAULT = [('keeper', None, 1, None), ('help', None, 1)]
# Outdoor posts (doc 67, section 4).
OUTDOOR = {
    # The food rebalance: Market Square's and the Rag Market's stalls sell food (bread, meat, cheese, preserves, greens and
    # fruit, fish, hot soup); the herbalist (for players) and the Rag Market's second-hand dealer stay.
    # (Each stall in its old place: the pie stall's keeper bakes, the cheese stall's makes cheese, the soup stall's
    # ladles soup, the porridge stall's bakes.) A few inns, dairies later: the cheese stall is a sausage butcher's while
    # there are no dairies (DAIRY), and the two soup stalls bake pies (Market Square) and sell provisions (the Rag Market).
    'market_square': ['baker on Market Square', dairy('market_square'), 'greens stall on Market Square',
                      'fruit stall on Market Square', 'preserver on Market Square', 'herbalist on Market Square',
                      'butcher on Market Square', 'pie baker on Market Square'],
    'rag_market': ['provisioner in the Rag Market', 'baker in the Rag Market', 'fishmonger in the Rag Market',
                   'greens stall in the Rag Market', 'rag-and-bone dealer, Rag Market', 'butcher in the Rag Market'],
}
OUTDOOR_BUSINESS = {'greens stall': 'stall', 'fruit stall': 'stall', 'baker': 'bakery', 'cheesemaker': 'dairy',
                    'preserver': 'preserver', 'ladling soup': 'inn', 'butcher': 'butcher', 'herbalist': 'herbalist',
                    'pie baker': 'bakery', 'sausage butcher': 'butcher', 'provisioner': 'provisioner',
                    'fishmonger': 'fishmonger', 'rag-and-bone dealer': 'rag_and_bone'}
HALL_KEEPERS = [('butcher in the Market Hall', 'butcher'), dairy('r2_market_hall'),
                ('cooper in the Market Hall', 'cooperage'), ('carpenter in the Market Hall', 'carpenter'),
                ('preserver in the Market Hall', 'preserver'), ('provisioner in the Market Hall', 'provisioner')]
GATE_POSTS = {'N': 2, 'E': 2, 'S': 2, 'harbour': 1, 'cinder': 1, 'customs': 1}
GATE_LABELS = {'N': 'holding the north gate', 'E': 'holding the east gate', 'S': 'holding the south gate',
               'harbour': 'holding the Harbour Gate', 'cinder': 'holding the Cinder Gate',
               'customs': 'holding the Customs Pier'}
PATROLS = {'rm_wall_walk': (4, 'walking the wall'), 'rm_street_patrol': (4, 'on patrol'),
           'rm_wharf_patrol': (2, 'on patrol on the wharf'), 'r2_sump_patrol': (2, 'on patrol in the Sump'),
           'r2_palace_round': (4, 'guarding the Great Palace')}
# Elastic pools: (district, label, cap, posts key or structure).
POOLS = [
    ('wharf', 'unloading ships at the piers', 14, 'pier'),
    ('wharf', 'working the cranes', 4, 'crane'),
    ('sump', 'carrying from Sump Stairs', 10, 'sump_stairs'),
    ('commons', 'portering in Market Square', 6, 'market_porters'),
    ('sump', 'washing at the Washhouse', 3, 'r2_the_washhouse'),
    ('sump', 'crying wares in the Rag Market', 6, 'tray_sellers'),
]

# The Houses' own people work their own House's business (doc 67, section 7; lane C's tools/rename_house.py WORK_MOVES):
# "keeping the wharf's books" goes from Greatly to Grayrock, so Tobias Grayrock takes Julian Greatly's desk at the
# (renamed) Grayrock Bonded Warehouse and Julian goes to the Greatly Yard; the Fell heir leaves the Timberland Sawmill for
# his own House's hall. {npc: (structure key, label, preferred spot or None)}. Applied after the evenings, so the rest of
# the plan (and its random draws) is unchanged.
HOUSE_POSTS = {
    'rm_tobias_grayrock_': ('rm_the_greatly_bonded_warehouse', "keeping the wharf's books", (8, 8)),
    'rm_julian_greatly': ('rm_the_greatly_yard', "keeping the Greatly Yard's books", (3, 2)),
    'rm_corwin_fell_': ('rm_fellhall', 'keeping the Fell cellars', None),
    # Two figures the plan had put to the wrong work (the food rebalance's text pass found them): Lord Grayrock, "always
    # at the wharf", was a deacon at the cathedral; the Chancellor of Ridgemere was one of the City Office's clerks.
    'rm_aldric_grayrock_': ('rm_the_harbourmaster_s_office', 'overseeing the Grayrock wharf', None),
    'rm_oskar_gullworth': ('rm_the_city_office', 'Chancellor of Ridgemere', None),
    # The jobs by class (doc 67, Progress): the Lord of House Greatly, "the richest wolf in Ridgemere", was one of his own
    # Counting House's three clerks; he oversees it now (the "counting house" wage rule: the Country's, as before).
    'rm_ambrose_greatly': ('rm_the_greatly_counting_house', 'overseeing the Counting House', None),
    # The Crown (doc 67, Q3): the King reigns from the Antechamber's dais (the speaker's seat at (20, 2)); the Queen and
    # their daughter keep the royal household upstairs in the Residence. (A spot (x, y, z) is on the structure's floor z.)
    'rm_gunnar_harrowgate': ('r2_palace_antechamber', 'reigning at the Great Palace', (20, 3)),
    'rm_oda_harrowgate': ('r2_kings_residence', "the Queen, at the King's Residence", (24, 17, 1)),
    'rm_ottilie_harrowgate': ('r2_kings_residence', "the King's daughter, at the Residence", (21, 17, 1)),
}
# The royal family's own text (doc 67, Q3): the Crown, not the Minor House they were (an iron merchant's household in the
# Heights). Applied by the write to these NPCs' description, greeting and backstory.
ROYAL_TEXT = {
    'rm_gunnar_harrowgate': {
        'description': 'A long-limbed russet timber wolf with a braided cord at the throat: the King of Ridgemere, who '
                       'reigns from the Great Palace.',
        'greeting': 'Good day to you. The Crown hears petitions in the Antechamber.',
        'backstory': 'Gunnar was born in the Sump and wears the Crown now; he has not forgotten the alleys, and the '
                     'Houses have not let him.'},
    'rm_oda_harrowgate': {
        'description': "A stocky sandy arctic wolf with pale, nearly silver eyes: the Queen, the King's wife, of the royal "
                       'household.',
        'greeting': 'Looking for someone? The King sits in the Antechamber today.',
        'backstory': "Oda keeps the royal household's purse herself and counts it, coin by coin."},
    'rm_ottilie_harrowgate': {
        'description': "A stocky cream red wolf with amber eyes that miss little: the King's daughter, of the royal "
                       'household.',
        'greeting': 'Take your time; there is no hurry. The palace keeps its own hours.',
        'backstory': "Ottilie carries the Crown's petitions to the City Office herself, and goes back every week to ask "
                     'after them.'},
}


# --- Jobs by class (doc 67, Progress, "Jobs by class") ----------------------------------------------------------------
# The user (2026-10-31): "Employment type should match the region of the city. ... Minor Houses should own larger
# businesses or should be merchants, along with high government positions. Middle-Class families will be stall and shop
# owners and lower government positions. Lower-class will be waged labor and similar unskilled work."
#
# A household's class is where it lives: the Rise's five Minor Houses and the Great Houses' families (their seats, the
# Greatly compound and the three estates) are upper; the Commons, Northgate and the administrative sector's houses are
# middle; the Sump, the Cinders and the wharf are lower. Where the quarters blend, the home's building decides: a shop
# with its flat is a shopkeeper's (middle, in any quarter); the Commons' "... Rents" tenements on the Sump's edge are
# lower. Those who live in a House's seat, villa or the King's Residence without being of its family (servants, stewards,
# the estates' gatehouses and cottages) live in service: lower. The Watch's barracks and the clergy are their own cases.
HOUSE_FAMILIES = ('Carlisle', 'Thornbury', 'Vance', 'Stanmore', 'Grantham', 'Greatly', 'Fell', 'Grayrock', 'Timberland',
                  'Harrowgate')
MINOR_FAMILIES = ('Carlisle', 'Thornbury', 'Vance', 'Stanmore', 'Grantham')
# The larger businesses: {structure key: who owns them}. 'own': the city's independent large businesses, owned by whoever
# keeps them (the keeper takes the till's profit, doc 68): an upper job. 'house': a Great House's works, yards, stores and
# bank, owned by the House: their master or storekeeper runs it for the House, a middle job. Nobody from the Sump, the
# Cinders or the wharf keeps either.
LARGE = {
    'r2_carters_yard': 'own', 'rm_the_rainwater_brewery': 'own', 'rm_the_rag_mill': 'own', 'rm_the_wharf_tannery': 'own',
    'rm_the_grey_gull_inn': 'own', 'rm_the_sailors_rest': 'own',
    'rm_the_grayrock_ironworks': 'house', 'rm_the_grayrock_foundry': 'house', 'rm_the_fell_sawmill': 'house',
    'rm_the_grayrock_iron_store': 'house', 'rm_the_greatly_plasterworks': 'house', 'rm_the_tileworks': 'house',
    'rm_the_lime_kilns': 'house', 'rm_the_cinder_glassworks': 'house', 'r2_greatly_forge': 'house',
    'rm_the_greatly_yard': 'house', 'rm_the_greatly_masons_yard': 'house', 'rm_the_greatly_colour_works': 'house',
    'rm_the_greatly_bank': 'house', 'r2_grayrock_shipwrights': 'house', 'rm_the_ropewalk': 'house',
    'rm_the_greatly_bonded_warehouse': 'house', 'rm_the_fell_timber_store': 'house', 'r2_new_store': 'house',
    'r2_fell_press_house': 'house', 'r2_fell_distillery': 'house', 'r2_fell_preserving_house': 'house',
}
# The keepers of these are the large business's (a warehouse's keeper is a staff post, not a till's).
LARGE_KEEPERS = ('keeping the bonded warehouse', 'keeping the long store', 'keeping the new store', 'keeping the iron store')
# A job's class by its label (lower case, by substring), after the posts' own rules (Placement.job_class): high government,
# the law, the Council, finance and the Heights' physician are upper; lower government, clerks and the offices' skilled
# staff are middle; service, carrying, washing, the lamps and the grounds are lower.
UPPER_WORDS = ('reigning at', 'the queen,', "the king's daughter", 'chancellor', 'royal secretary', "the crown's treasurer",
               "collecting the crown's customs", 'harbourmaster', 'running the city office', 'keeping the rolls',
               'hearing cases', 'overseeing', 'warden of the market hall', 'pleading cases', "drafting the council's",
               'lending', 'visiting the sick of the heights')
MIDDLE_WORDS = ('clerking', 'copying', 'tallying', 'weighing', 'couriers', 'teaching', 'herald', 'usher',
                'keeping order in court', 'keeping the washhouse', 'keeping the bathhouse', 'keeping the fell cellars',
                "keeping the watch's", 'keeping the bonded warehouse', 'keeping the long store', 'keeping the new store',
                'keeping the iron store')
LOWER_WORDS = ('serving', 'in service to', 'keeping the chambers', 'stewarding', 'cooking for the king', 'lighting the lamps',
               'working at', 'washing at', 'driving for', 'keeping the greatly stables', 'unloading', 'working the cranes',
               'carrying from', 'portering', 'crying wares', 'gate', 'garden', 'groom', 'stable')
LEISURE = ('at home in the heights', 'reading in the gardens', 'walking the gardens')
# The Rag Market's stalls and the Soup Kitchen's ladle are shopkeeping like any other (middle; the user, 2026-10-31: "The
# Rag Market stalls should be run by middle class adults before sump adults"), and they come first: a Sump keeper there
# counts against the plan more than one at another counter, so the class pass gives them to the Commons before any other
# till it can't fill from the middle class. (The tray sellers crying wares beside them are waged street work, lower.)
# (Since a few inns, the Soup Kitchen has no till (GATHERING); it stays listed for when it has one again.)
RAG_FIRST = ('rag_market', 'rm_the_soup_kitchen')
CLASS_SCORE = {('upper', 'upper'): 1, ('middle', 'middle'): 1, ('lower', 'lower'): 1, ('upper', 'lower'): -1,
               ('lower', 'upper'): -2}
# The Minor Houses' members at leisure ("at home in the Heights") and the Carlisle who drove a carters' wagon take the
# city's independent large businesses and a high office: {npc: (structure key, post kind, post label or None)}. Whoever
# held each is moved by the class pass.
MINOR_POSTS = {
    'rmx_orm_carlisle': ('r2_carters_yard', 'keeper', None),
    'rm_mab_carlisle': ('rm_the_sailors_rest', 'keeper', None),
    'rm_desmond_grantham': ('rm_the_rainwater_brewery', 'keeper', None),
    'rm_karin_stanmore': ('r2_administrative_wing', 'staff', "the Crown's treasurer"),
    'rm_eleanor_vance': ('rm_the_rag_mill', 'keeper', None),
}
# Their own text (their descriptions keep "of the X household"; applied by the write, as ROYAL_TEXT is).
MINOR_TEXT = {
    'rmx_orm_carlisle': {
        'description': "A long-limbed cream timber wolf with a white blaze down the chest, of the Carlisle household, who "
                       "keeps the Carters' Yard by the East Gate for the Carlisles.",
        'greeting': "Ho! Come in, come in! The Carlisles' wagons go out at dawn. What do you want carried?",
        'backstory': 'Orm believes the Houses will fall out with each other before the next winter. Orm drove a team for '
                     'the yard for a season to learn the roads, and keeps it now for the Carlisles.'},
    'rm_mab_carlisle': {
        'description': "A rangy cream arctic wolf with one torn ear, of the Carlisle household, who keeps The Sailors' Rest "
                       'on the wharf for the Carlisles.',
        'greeting': "Good day to you. Dry yourself by the fire; the Sailors' Rest keeps a good one.",
        'backstory': "Mab carries a letter meant for Lord Grayrock, who is never home to take it. Mab took the Sailors' "
                     'Rest for the Carlisles, to be on the wharf when his ships come in.'},
    'rm_desmond_grantham': {
        'description': 'A long-limbed grey arctic wolf with a grey-frosted muzzle, of the Grantham household, who runs The '
                       "Rainwater Brewery, the Granthams' brewhouse in the Cinders.",
        'greeting': 'Good day to you. Ale by the barrel, and no credit.',
        'backstory': 'Desmond believes the Houses will fall out with each other before the next winter. Desmond put the '
                     'Grantham money into the Rainwater Brewery: the city drinks, whoever is winning.'},
    'rm_karin_stanmore': {
        'description': "A tall grey timber wolf with a coat kept meticulously clean, of the Stanmore household, who keeps "
                       "the Crown's purse in the Administrative Wing as its treasurer.",
        'greeting': 'Good day to you. The Crown pays what it owes, in its own time.',
        'backstory': "Karin keeps a secret list of which Houses pay the Watch to look away. Karin was made the Crown's "
                     'treasurer on the Stanmores\' word in the Council, and the list has grown longer since.'},
    'rm_eleanor_vance': {
        'description': 'A compact sandy timber wolf with a braided cord at the throat, of the Vance household, who runs The '
                       "Rag Mill, where the Vances' ledgers are made.",
        'greeting': 'Good day to you. Paper by the quire; the Council buys the rest.',
        'backstory': 'Eleanor believes the Houses will fall out with each other before the next winter. Eleanor runs the '
                     "Vances' Rag Mill: every quarrel needs paper."},
}
# A swapped post's fields (the person keeps the rest: home, evening; the post's hours go with it, as 'hours').
JOB_KEYS = ('work', 'business', 'role', 'post', 'template', 'record', 'route', 'kept', 'recruit', 'promoted', 'demoted',
            'at', 'labelKept')
SWAPPABLE = ('keeper', 'stallkeeper', 'hallkeeper', 'help', 'hands', 'staff', 'food', 'pool')
# Where an outdoor post is, for the tie-break that keeps work near home.
POST_DISTRICT = {'market_square': 'commons', 'rag_market': 'sump', 'pier': 'wharf', 'crane': 'wharf',
                 'sump_stairs': 'sump', 'tray_sellers': 'sump', 'market_porters': 'commons'}
CLERK_WORDS = ('clerk', 'copy', 'copi', 'record', 'ledger', 'tally', 'scribe', 'letters', 'books', 'count')
# A line of backstory for whoever changes job here, by the class and kind of the job they go to.
CLASS_BACK = {
    'middle/office': ("{first} learned letters and figures young, and was glad to put them to use when a desk came free.",
                      '{first} left the old work for a desk when one came free, and writes a careful, square hand.'),
    'middle/works': ('{first} was taken on to run {name} for the House that owns it, and answers to its steward for every '
                     'load.', '{first} came up through the Commons trades to run {name}; the House that owns it counts '
                     'every farthing.'),
    'middle/shop': ('{first} took over the counter here when it came free, and means to keep it in the family.',),
    'middle/work': ('{first} took the post when it came free: respectable work, as the family wanted.',),
    'lower/work': ('{first} lost the old post to someone better placed, and took the work the city had going.',
                   '{first} went back to waged work when the old place went to someone better placed, and does not '
                   'complain where anyone can hear.'),
    'upper/work': ('{first} took {name} on when its old keeper gave it up, and keeps its profit now.',),
}


class Placement:
    """The NPC plan: for every NPC, placed (home, work, evening, business, role, route) or unplaced and why."""

    def __init__(self, project, city, seed=67, current=None):
        """`current`: {npc: the NPC as the world has it now} (its role, label and work), so that a rewrite keeps whoever
        holds a post in it (the food rebalance: only those whose post is gone or new change job)."""
        self.project, self.city = project, city
        self.current = current or {}
        self.rng = random.Random(seed)
        self.people = {p['id']: p for p in project['people']}
        self.rooms = {r['id']: r for r in project['rooms']}
        for r in city.site.rooms:
            self.rooms[r['id']] = r
        self.a = analysis()
        self.par = self.a['par']
        self.hh = households(self.par)
        self.records = {rec['key']: rec for rec in city.records}
        self.room_record = {room['id']: rec for rec in city.records for room in rec['rooms']}
        self.plan = {}
        self.taken = set()                      # (room, x, y) spots given
        self.issues = []

    # -- spots -------------------------------------------------------------------------------------------------------
    def walkable(self, area, x, y):
        if area.startswith('w_ridgemere_'):              # A cell: the canvas itself.
            cx, cy = map(int, area.rsplit('_', 2)[1:])
            if not (0 <= x < 256 and 0 <= y < 256):
                return False
            ch = chr(self.city.c.codes[cy * 256 + y, cx * 256 + x])
            return ch not in SOLID and ch not in '+^'
        r = self.rooms.get(area)
        if r is None:
            return False
        return 0 <= y < r['height'] and 0 <= x < r['width'] and r['terrain'][y][x] not in SOLID and \
            r['terrain'][y][x] not in '+^' and (x, y) in self.reach(area)

    def reach(self, area):
        """The tiles of an interior someone can walk to from its doors and stairs."""
        if not hasattr(self, '_reach'):
            self._reach = {}
        if area not in self._reach:
            r = self.rooms[area]
            ends = [(e['x'], e['y']) for l in self.project['links'] + self.city.site.links for e in (l['a'], l['b'])
                    if e['cell'] == area]
            seen = set()
            todo = []
            for x, y in ends:
                for dx, dy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)):
                    t = (x + dx, y + dy)
                    if 0 <= t[0] < r['width'] and 0 <= t[1] < r['height'] and r['terrain'][t[1]][t[0]] not in SOLID:
                        todo.append(t)
            while todo:
                t = todo.pop()
                if t in seen:
                    continue
                seen.add(t)
                x, y = t
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < r['width'] and 0 <= ny < r['height'] and (nx, ny) not in seen and \
                            r['terrain'][ny][nx] not in SOLID:
                        todo.append((nx, ny))
            self._reach[area] = seen
        return self._reach[area]

    def link_ends(self):
        if not hasattr(self, '_ends'):
            links = self.project['links'] + self.city.site.links
            self._ends = {(e['cell'], e['x'], e['y']) for l in links for e in (l['a'], l['b'])}
            for l in links:                          # The tile beside each end, where someone arrives, too.
                for e in (l['a'], l['b']):
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        self._ends.add((e['cell'], e['x'] + dx, e['y'] + dy))
        return self._ends

    def spot_near(self, area, x, y, share=False):
        for r in range(0, 9):
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    if max(abs(dx), abs(dy)) != r:
                        continue
                    p = (area, x + dx, y + dy)
                    if self.walkable(*p) and p not in self.link_ends() and (share or p not in self.taken):
                        if not share:
                            self.taken.add(p)
                        return {'cell': area, 'x': p[1], 'y': p[2]}
        return None

    def floor(self, area):
        if area.startswith('w_ridgemere_'):
            return self.spot_near(area, 128, 128)
        r = self.rooms[area]
        tiles = [(x, y) for y in range(1, r['height'] - 1) for x in range(1, r['width'] - 1)
                 if self.walkable(area, x, y) and (area, x, y) not in self.link_ends()]
        self.rng.shuffle(tiles)
        for x, y in tiles:
            if (area, x, y) not in self.taken:
                self.taken.add((area, x, y))
                return {'cell': area, 'x': x, 'y': y}
        x, y = tiles[0] if tiles else (r['width'] // 2, r['height'] // 2)
        return {'cell': area, 'x': x, 'y': y}

    def outdoor(self, cx, cy, share=False):
        """A canvas tile as a cell spot (the nearest free walkable one)."""
        ix, iy = int(cx), int(cy)
        cell = f'w_ridgemere_{ix // 256}_{iy // 256}'
        got = self.spot_near(cell, ix % 256, iy % 256, share)
        if got is None:
            self.issues.append(f'no open tile near ({cx}, {cy})')
        return got

    def bed_spot(self, room):
        """A place to sleep: beside one of the room's beds, or on its straw."""
        r = self.rooms[room]
        beds = [(x, y) for y, row in enumerate(r['terrain']) for x, ch in enumerate(row) if ch in 'bz']
        self.rng.shuffle(beds)
        for x, y in beds:
            got = self.spot_near(room, x, y)
            if got:
                return got
        return self.floor(room)

    # -- the plan --------------------------------------------------------------------------------------------------------
    def run(self):
        P = placed_ids(self.project, self.a)
        self.placed = P
        for pid, p in self.people.items():
            if pid not in P:
                why = 'a town' if pid.startswith('tn_') else 'the country' if pid.startswith('cty_') else \
                    'a farm' if p['home'] and 'bunkhouse' in p['home']['cell'] else 'a Minor House not kept' \
                    if p['home'] and any(m in p['home']['cell'] for m in self.a['mansions']) else 'outside the city'
                self.plan[pid] = {'placed': False, 'why': why}
        self.homes()
        self.jobs()
        self.evenings()
        self.house_posts()
        self.class_jobs()
        return self.plan

    def house_posts(self):
        """HOUSE_POSTS: a House's family member moved to his own House's business (a civilian staff post)."""
        moving = [(pid, *post) for pid, post in HOUSE_POSTS.items()
                  if self.plan.get(pid, {}).get('placed') and post[0] in self.records]
        for pid, *_ in moving:                                  # Free the spots they leave first.
            w = self.plan[pid].get('work')
            if w:
                self.taken.discard((w['cell'], w['x'], w['y']))
        for pid, record, label, spot in moving:
            rec = self.records[record]
            room = rec['rooms'][0]['id']
            if spot and len(spot) == 3:
                room = next(r['id'] for r in rec['rooms'] if r['z'] == spot[2])
            now = self.current.get(pid) or {}
            if not spot and now.get('workLabel') == label and (now.get('work') or {}).get('cell') in \
                    {r['id'] for r in rec['rooms']}:
                spot = (now['work']['x'], now['work']['y'])             # (Their post now: the spot they work at.)
                room = now['work']['cell']
            work = self.spot_near(room, spot[0], spot[1]) if spot else None
            self.plan[pid].update({'work': work or self.work_spot(rec, 'staff'), 'business': None, 'role': 'civilian',
                                   'post': 'staff', 'template': label, 'record': record, 'route': '', 'kept': False,
                                   'labelKept': False,
                                   'at': self.at.get(record)})

    # -- jobs by class (doc 67, Progress, "Jobs by class") ---------------------------------------------------------------
    def home_class(self, pid):
        """(class, why) of a placed NPC's household, by the building it lives in."""
        rec = self.room_record.get(((self.plan.get(pid) or {}).get('home') or {}).get('cell'))
        if rec is None:
            return 'lower', 'no home building'
        kind, area = rec.get('kindOf'), rec['area']
        family = self.people[pid]['name'].split()[-1] in HOUSE_FAMILIES
        if kind in ('seat', 'villa'):
            return ('upper', f'of the House, at {rec["name"]}') if family else ('lower', f'in service at {rec["name"]}')
        if area == 'keep':
            return 'watch', "the Watch's barracks"
        if area.startswith('estate_'):
            return 'lower', f"an estate's gatehouse or cottages, {rec['name']}"
        if kind == 'shop':
            return 'middle', f'a shop with its flat, {rec["name"]}'
        if area in ('sump', 'cinders', 'wharf'):
            return 'lower', f'the {area}'
        if (self.city.named.get(rec['key']) or {}).get('tenement') or rec['name'].endswith(' Rents'):
            return 'lower', f"a tenement on the Sump's edge, {rec['name']}"
        return 'middle', area

    def label_of(self, pid, e):
        """The label a post will have (render_labels' for the posts the class pass moves; the kept label otherwise)."""
        if e.get('labelKept') or e.get('post') in ('kept', 'outdoor'):
            return (self.current.get(pid) or self.people[pid])['workLabel']
        return e.get('template') or ''

    def job_class(self, pid, e=None, label=None):
        """(class, why) of the job a post gives pid (None for children, pupils and the retired; 'clergy' and 'watch' are
        their own cases; 'leisure' is no job)."""
        e = self.plan[pid] if e is None else e
        label = self.label_of(pid, e) if label is None else label
        t, post, rec, age = label.lower(), e.get('post'), e.get('record'), self.people[pid]['age']
        if age < 16 or post in ('at lessons', 'retired', 'a child at home') or 'lessons' in t:
            return None, 'not an earner'
        if e.get('role') == 'guard':
            return 'watch', 'the Watch'
        if post == 'clergy' or is_clergy(label):
            return 'clergy', 'the abbey'
        if t.startswith(LEISURE):
            return (None, 'retired') if age >= 60 else ('leisure', 'at leisure')
        if self.home_class(pid)[0] == 'upper' and (pid in HOUSE_POSTS or post in ('kept', 'outdoor')):
            return 'upper', "the House's own"
        if house_head(label):
            return 'upper', 'a House head'
        if post in ('keeper', 'stallkeeper', 'hallkeeper'):
            owner = LARGE.get(rec)
            if owner == 'own':
                return 'upper', 'owns a large business'
            if owner == 'house':
                return 'middle', "runs a Great House's works for it"
            return 'middle', 'keeps a shop, stall or counter'
        if post in ('help', 'hands'):
            home = self.room_record.get(((self.plan.get(pid) or {}).get('home') or {}).get('cell'))
            if home is not None and home['key'] == rec:
                return 'middle', 'helps in the family shop'
            return 'lower', 'a hand'
        for words, cls, why in ((UPPER_WORDS, 'upper', 'high office'), (MIDDLE_WORDS, 'middle', 'lower office'),
                                (LOWER_WORDS, 'lower', 'waged or service work')):
            if any(w in t for w in words):
                return cls, why
        if post == 'pool':
            return 'lower', 'waged work'
        return 'middle', 'unclassified'

    def rag_keeper(self, e):
        """Whether a post keeps a Rag Market stall or the Soup Kitchen (RAG_FIRST)."""
        return e.get('post') in ('keeper', 'stallkeeper', 'hallkeeper') and e.get('record') in RAG_FIRST

    def large_keeper(self, e):
        """Whether a post keeps one of the larger businesses (LARGE)."""
        if e.get('post') in ('keeper', 'stallkeeper', 'hallkeeper'):
            return e.get('record') in LARGE
        return e.get('post') == 'staff' and (e.get('template') or '').lower().startswith(LARGE_KEEPERS)

    def movable(self, pid):
        """Whether the class pass may give pid another post: an NPC of 16-69 in a post of the city's lists (not the
        Watch, the clergy, a House's own, a figure with a description of their own, or a shopkeeper's family in the shop
        they live over)."""
        e, p = self.plan.get(pid) or {}, self.people[pid]
        if not e.get('placed') or not 16 <= p['age'] <= 69 or e.get('post') not in SWAPPABLE or e.get('role') == 'guard':
            return False
        if pid in HOUSE_POSTS or pid in ROYAL_TEXT or pid in MINOR_POSTS:
            return False
        if self.job_class(pid)[0] in (None, 'clergy', 'watch', 'leisure'):
            return False
        home = self.room_record.get((e.get('home') or {}).get('cell'))
        if home is not None and home['key'] == e.get('record'):
            return False
        look = split_description(p['description'])
        return look is not None and not any(t in look[1] for t in KEY_TITLES)

    def post_district(self, e):
        rec = e.get('record')
        return POST_DISTRICT.get(rec) or (self.records[rec]['area'] if rec in self.records else None)

    def class_jobs(self):
        """The class pass: the Minor Houses' members take the city's independent large businesses and a high office
        (MINOR_POSTS), then posts are swapped between NPCs of 16-69 while that gives more of them a job of their
        household's class (and moves the Sump's, the Cinders' and the wharf's out of the larger businesses), best swap
        first. Homes, evenings and the posts themselves (their labels, spots, roles, hours) don't change; only who holds
        them."""
        placed = [pid for pid in sorted(self.plan) if self.plan[pid].get('placed')]
        self.class_before = class_summary(self, placed, lambda pid: self.label_of(pid, self.plan[pid]))
        self.class_moves = {}
        # 1. The Minor Houses' claims.
        pool_load = Counter(self.plan[x].get('template') for x in placed if self.plan[x].get('post') == 'pool')
        for pid, (record, kind, label) in sorted(MINOR_POSTS.items()):
            if not self.plan.get(pid, {}).get('placed'):
                continue
            holder = next((h for h in placed if self.plan[h].get('record') == record and self.plan[h].get('post') == kind
                           and label in (None, self.plan[h].get('template'))), None)
            if holder is None or holder == pid:
                continue
            job = {k: self.plan[holder].get(k) for k in JOB_KEYS} | {'hours': dict(self.people[holder]['hours'])}
            mine = {k: self.plan[pid].get(k) for k in JOB_KEYS} | {'hours': dict(self.people[pid]['hours'])}
            if mine['post'] in SWAPPABLE:
                self.plan[holder].update(mine, kept=False)
            else:                                   # (A job of the Houses' own: the one displaced goes to a pool.)
                dist, label, cap, where = next((p for p in POOLS if pool_load[p[1]] < p[2]),
                                               ('wharf', 'unloading ships at the piers', 999, 'pier'))
                pool_load[label] += 1
                for k in JOB_KEYS:
                    self.plan[holder].pop(k, None)
                self.work_entry(holder, {'kind': 'pool', 'label': label, 'where': where, 'record': where},
                                self.plan[holder])
                self.plan[holder]['at'] = self.at.get(where)
            self.plan[pid].update(job, kept=False, labelKept=False)
            self.class_moves[pid] = self.class_moves[holder] = 'claim'
        # 2. Swaps.
        movers = [pid for pid in placed if self.movable(pid)]
        # (A post's hours go with it: a till opens when its keeper is at it.)
        slots = [{k: self.plan[pid].get(k) for k in JOB_KEYS} | {'hours': self.plan[pid].get('hours') or
                                                                  dict(self.people[pid]['hours'])} for pid in movers]
        hc = {pid: self.home_class(pid)[0] for pid in movers}
        slot_of = {pid: i for i, pid in enumerate(movers)}
        start = dict(slot_of)
        cache = {}

        def value(pid, i):
            key = (pid, i)
            if key not in cache:
                e = slots[i]
                p = self.people[pid]
                if e.get('post') in ('keeper', 'stallkeeper', 'hallkeeper') and p['age'] < 18 or \
                        e.get('post') == 'pool' and p['age'] >= 60:         # (The pools are 16-59.)
                    cache[key] = (-99, 0)
                    return cache[key]
                jc = self.job_class(pid, e)[0]
                score = CLASS_SCORE.get((hc[pid], jc), 1 if hc[pid] == jc else 0)
                if hc[pid] == 'lower' and self.large_keeper(e):
                    score -= 3
                if hc[pid] == 'lower' and self.rag_keeper(e):
                    score -= 1
                # The tie-break: the same trade, a merchant to a till, a clerk to a desk, work near home.
                cur = self.current.get(pid) or p
                labs = (cur['workLabel'], p['workLabel'])
                aff = 0
                trade = e.get('business') or e.get('at')
                if e.get('post') in ('keeper', 'stallkeeper', 'hallkeeper', 'help', 'hands') and trade and \
                        trade in {business_for(x) for x in labs}:
                    aff += 3
                if e.get('post') in ('keeper', 'stallkeeper', 'hallkeeper') and cur['role'] == 'merchant':
                    aff += 2
                if self.large_keeper(e) and p['age'] >= 30:         # (A larger business wants some years behind it.)
                    aff += 2
                tpl = (e.get('template') or '').lower()
                if any(w in tpl for w in CLERK_WORDS) and any(w in x.lower() for x in labs for w in CLERK_WORDS):
                    aff += 2
                home = self.room_record.get(self.plan[pid]['home']['cell'])
                if home is not None and self.post_district(e) == home['area']:
                    aff += 1
                cache[key] = (score, aff)
            return cache[key]
        # The Rag Market first: each of its keepers from the Sump gives the till to a Commons keeper of another till,
        # that till goes to a Sump earner in waged work, and the Sump keeper takes that work (a three-way move: the
        # city has no middle-class adults out of work, so the mismatch moves to a till the Commons can spare, one in the
        # Sump's, the wharf's or the Cinders' own streets first).
        self.rag_cycles, cycled = [], set()
        for a in [x for x in movers if self.rag_keeper(slots[slot_of[x]]) and value(x, slot_of[x])[0] < 1]:
            ir = slot_of[a]
            best = None
            for m in movers:
                im = slot_of[m]
                if m == a or m in cycled or value(m, im)[0] != 1 or hc[m] != 'middle' or self.rag_keeper(slots[im]) or \
                        slots[im].get('post') not in ('keeper', 'stallkeeper', 'hallkeeper') or value(m, ir)[0] != 1:
                    continue
                near = 3 if self.post_district(slots[im]) in ('sump', 'wharf', 'cinders') else 0
                for b in movers:
                    ib = slot_of[b]
                    if b in (a, m) or b in cycled or hc[b] != 'lower' or value(b, ib)[0] != 1 or value(a, ib)[0] != 1 or \
                            value(b, im)[0] < 0:
                        continue
                    fit = value(m, ir)[1] + value(b, im)[1] + value(a, ib)[1] + near
                    if best is None or fit > best[0]:
                        best = (fit, m, b)
            if best is None:
                continue
            _, m, b = best
            im, ib = slot_of[m], slot_of[b]
            slot_of[m], slot_of[b], slot_of[a] = ir, im, ib
            self.rag_cycles.append((a, m, b))
            cycled |= {a, m, b}
        while True:
            best = None
            for x in movers:
                ix = slot_of[x]
                vx = value(x, ix)
                if vx[0] >= 1:
                    continue
                for y in movers:
                    if y == x:
                        continue
                    iy = slot_of[y]
                    vy = value(y, iy)
                    a, b = value(x, iy), value(y, ix)
                    gain = a[0] + b[0] - vx[0] - vy[0]
                    if gain <= 0:
                        continue
                    cand = (gain, a[1] + b[1] - vx[1] - vy[1], x, y)
                    if best is None or cand[:2] > best[:2]:
                        best = cand
            if best is None:
                break
            _, _, x, y = best
            slot_of[x], slot_of[y] = slot_of[y], slot_of[x]
        for pid in movers:
            if slot_of[pid] != start[pid]:
                self.plan[pid].update(slots[slot_of[pid]], kept=False, labelKept=False)
                self.class_moves[pid] = 'Rag Market first' if pid in cycled else 'swap'
        for pid in self.class_moves:                     # (For the backstory line: the class and kind of the job.)
            cls, why = self.job_class(pid)
            e = self.plan[pid]
            office = any(w in self.label_of(pid, e).lower() for w in ('clerk', 'copy', 'tally', 'weigh'))
            kind = 'office' if why == 'lower office' and office else 'works' if LARGE.get(e.get('record')) == 'house' and \
                e.get('post') == 'keeper' else 'shop' if e.get('post') in ('keeper', 'stallkeeper', 'hallkeeper') and \
                cls == 'middle' else 'work'
            e['classMove'] = f'{cls}/{kind}' if f'{cls}/{kind}' in CLASS_BACK else f'{cls}/work'

    # -- homes ---------------------------------------------------------------------------------------------------------
    def home_rooms(self):
        """Every home in the city (doc 67: an interior someone names as home, with its beds): {room: capacity}."""
        out = {}
        for rec in self.city.records:
            kind = rec.get('kindOf')
            rooms = rec['rooms']
            if rec.get('reused'):
                if kind == 'shop':
                    homes = [r for r in rooms if r['id'].endswith('_flat')]
                elif kind in ('home', 'lodging'):
                    homes = rooms[:1]
                elif kind == 'villa':
                    homes = [r for r in rooms if r['z'] in (0, 1)]
                elif kind == 'seat':
                    homes = [r for r in rooms if r['z'] in (1, -1)]
                else:
                    homes = []
            else:
                if kind in ('flats', 'home', 'lodging'):
                    homes = rooms
                elif kind == 'seat':
                    homes = [r for r in rooms if r['z'] in (1, -1)]
                elif kind == 'shop':
                    homes = [r for r in rooms if r['z'] == 1]
                else:
                    homes = []
            for r in homes:
                beds = len(r['beds'])
                communal = any(w in r['id'] for w in ('barracks', 'watch_house', 'quarters', 'lodging'))
                out[r['id']] = {'beds': beds, 'cap': beds if communal else 2 * beds, 'district': rec['area'],
                                'structure': rec['id'], 'communal': communal, 'new': not rec.get('reused')}
        return out

    def homes(self):
        rooms = self.home_rooms()
        self.home_cap = rooms
        load = Counter()
        P = sorted(self.placed)
        moved = []
        family = {}
        for pid in P:
            p = self.people[pid]
            home = p['home']['cell'] if p['home'] else None
            # Seats and Greatly House: the family keeps the chambers, the servants move to the servants' floor.
            if home and home.endswith('_up') and home[:-3] in SEAT_HOUSE:
                surname = SEAT_HOUSE[home[:-3]]
                if surname not in pid:
                    down = home[:-3] + '_down'
                    self.plan[pid] = {'placed': True, 'home': self.bed_spot(down), 'homeKept': False}
                    load[down] += 1
                    continue
            if home in rooms and home not in ('rm_harrowgate_house', 'rm_harrowgate_house_up'):
                self.plan[pid] = {'placed': True, 'home': dict(p['home']), 'homeKept': True}
                self.taken.add((home, p['home']['x'], p['home']['y']))
                load[home] += 1
            else:
                moved.append(pid)
        # The Harrowgates move into the King's Residence (doc 67, Q3).
        for pid in self.a['mansions'].get('harrowgate', []):
            if pid in moved:
                moved.remove(pid)
                floors = {r['z']: r['id'] for r in self.records['r2_kings_residence']['rooms']}
                up = floors[1] if self.people[pid]['home']['cell'].endswith('_up') else floors[-1]
                self.plan[pid] = {'placed': True, 'home': self.bed_spot(up), 'homeKept': False}
                load[up] += 1
        # Everyone else in units: each household its own, singles from one old home in twos and threes (some alone).
        units = []
        seen = set()
        by_old = defaultdict(list)
        for pid in moved:
            if pid in seen:
                continue
            members = sorted(self.hh.get(self.par.get(pid, pid), {pid}) & set(moved))
            seen |= set(members)
            if len(members) == 1:
                by_old[self.people[pid]['home']['cell'] if self.people[pid]['home'] else ''].append(pid)
            else:
                units.append(members)
        alone = 0
        for old, singles in sorted(by_old.items()):
            singles = sorted(singles)
            while singles:
                p0 = self.people[singles[0]]
                if alone < 10 and 18 <= p0['age'] <= 32:
                    units.append([singles.pop(0)])
                    alone += 1
                    continue
                n = 3 if len(singles) >= 5 or len(singles) == 3 else 2 if len(singles) >= 2 else 1
                units.append(singles[:n])
                singles = singles[n:]
        self.alone = alone
        # Where each unit goes: by where it lived (doc 67, section 5): old rows and tenements to the Sump, old houses
        # to the Commons' row houses first. Houses fill floor by floor, one house after another (doc 67, Dense city: more
        # families together), so the Sump's houses hold three households each and the empty ones are left whole.
        free = {room: info for room, info in rooms.items() if info['new'] and not info['communal']}

        def by_house(district):
            houses = defaultdict(list)
            for r, i in free.items():
                if i['district'] in district:
                    houses[i['structure']].append(r)
            order = sorted(houses)
            self.rng.shuffle(order)
            order.sort(key=lambda h: -len(houses[h]))           # (the three-home houses first)
            return [r for h in order for r in sorted(houses[h])]
        order_s = by_house(('sump',))
        order_c = by_house(('commons', 'northgate'))
        units.sort(key=lambda u: (-len(u), u[0]))
        for u in units:
            old = self.people[u[0]]['home']['cell'] if self.people[u[0]]['home'] else ''
            house_like = old.endswith('_house') or old.endswith('_house_2') or old in ('rm_the_heights_watch_post',)
            prefs = (order_c + order_s) if house_like else (order_s + order_c)
            room = next((r for r in prefs if load[r] == 0), None)
            if room is None:
                room = min(prefs, key=lambda r: load[r] / max(1, rooms[r]['cap']))
                self.issues.append(f'unit {u} shares {room}')
            for pid in u:
                self.plan[pid] = {'placed': True, 'home': self.bed_spot(room), 'homeKept': False}
            load[room] += len(u)
        self.home_load = load
        for room, n in load.items():
            self.ensure_beds(room, n)

    def ensure_beds(self, room, n):
        """At least ceil(n / 2) beds, and two (doc 67, section 3): straw bedding laid on free floor if short."""
        r = self.rooms[room]
        rows = [list(x) for x in r['terrain']]
        have = sum(row.count('b') + row.count('z') for row in rows)
        need = max(2, math.ceil(n / 2))
        if have >= need:
            return
        ends = self.link_ends()
        for y in range(1, r['height'] - 1):
            for x in range(1, r['width'] - 1):
                if have >= need:
                    break
                if rows[y][x] in '.fm,' and (room, x, y) not in ends and (room, x, y) not in self.taken and \
                        (rows[y][x - 1] in SOLID or rows[y - 1][x] in SOLID):
                    rows[y][x] = 'z'
                    have += 1
        r['terrain'] = [''.join(row) for row in rows]
        self.issues.append(f'{room}: straw bedding added for {n}')

    # -- jobs ------------------------------------------------------------------------------------------------------------
    def name_of(self, key):
        rec = self.records.get(key)
        return rec['name'] if rec else key

    def jobs(self):
        """Kept posts, moved trades, new fixed posts, elastic pools, children at lessons, the old retired (section 5)."""
        city = self.city
        placed_rooms = {room['id']: rec for rec in city.records for room in rec['rooms']}
        posts = []                   # open posts: dicts
        for rec in city.records:
            key = rec['key']
            spec = POSTS.get(key)
            if spec is None and rec.get('kindOf') == 'shop':
                # A shop kept: its keeper's word as its keeper had it ("saddler at The Harness Shop").
                rooms_ = {r['id'] for r in rec['rooms']}
                words = [x['workLabel'].split(' at ')[0] for x in self.people.values()
                         if x['role'] == 'merchant' and x['work'] and x['work']['cell'] in rooms_ and ' at ' in x['workLabel']]
                word = words[0] if words else 'shopkeeper'
                spec = [('keeper', word, 1, business_for(f'{word} at {rec["name"]}')), ('help', None, 1)]
            if spec is None:
                continue
            for item in spec:
                kind, label, n = item[0], item[1], item[2]
                business = item[3] if len(item) > 3 else rec.get('business')
                for _ in range(n):
                    posts.append({'kind': kind, 'label': label, 'business': business, 'record': key, 'filled': None})
        for key, labels in OUTDOOR.items():
            for i, label in enumerate(labels):
                posts.append({'kind': 'stallkeeper', 'label': label, 'record': key, 'index': i, 'filled': None,
                              'business': OUTDOOR_BUSINESS[label.split(' on ')[0].split(' in ')[0].split(',')[0]]})
        for i, (label, business) in enumerate(HALL_KEEPERS):
            posts.append({'kind': 'hallkeeper', 'label': label, 'business': business, 'record': 'r2_market_hall',
                          'index': i, 'filled': None})
        for gate, n in GATE_POSTS.items():
            for _ in range(n):
                posts.append({'kind': 'gate', 'label': GATE_LABELS[gate], 'gate': gate, 'record': None, 'filled': None})
        for route, (n, label) in PATROLS.items():
            for _ in range(n):
                posts.append({'kind': 'patrol', 'label': label, 'route': route, 'record': None, 'filled': None})
        self.posts = posts
        # The business of each workplace (its keeper's, or the building's own), for the label rules.
        self.at = {rec['key']: rec.get('business') for rec in city.records}
        for q in posts:
            if q['kind'] == 'keeper' and q.get('business'):
                self.at[q['record']] = q['business']
        by_record = defaultdict(list)
        for p in posts:
            by_record[p['record']].append(p)
        adults = [pid for pid in sorted(self.placed) if self.people[pid]['age'] >= 16]
        jobs = {}
        # 1. Kept posts: work in a reused interior that is placed; a merchant keeps the till of its building.
        MOVED = {'rm_the_customs_house': 'r2_customs_house', 'rm_the_gaol': 'r2_the_keep',
                 'rm_the_heights_watch_post': 'r2_the_keep', 'rm_the_rain_hall': 'r2_palace_antechamber',
                 'rm_the_bridge_lamp': 'r2_the_knucklebones', 'rm_the_wagon_yard': 'r2_cinder_wheelwrights',
                 'rm_old_kettle_smokehouse': 'rm_the_north_wharf_smokehouse', 'rm_the_old_kettle_smokehouse':
                     'rm_the_north_wharf_smokehouse', 'rm_the_watch_house': 'r2_the_keep'}
        # 0. Whoever holds a post in the world as it is now keeps it (the food rebalance; since the jobs by class, first:
        # a rewrite then keeps the class pass's moves, whatever the posts were before the city).
        self.keep_current(posts, jobs, adults, placed_rooms)
        pool_labels = {p[1] for p in POOLS}
        for pid in sorted(self.placed, key=lambda q: (self.people[q]['role'] != 'merchant', q)):
            p = self.people[pid]
            if not p['work'] or pid in jobs or (self.current.get(pid) or {}).get('workLabel') in pool_labels:
                continue                                # (In a pool now: kept there, below.)
            room = p['work']['cell']
            rec = placed_rooms.get(room)
            target = MOVED.get(room) or (rec['key'] if rec else None)
            if rec is not None and rec['key'] != target:
                rec = None                                  # (Its post moved: a new spot at the new place.)
            if room.startswith('rm_the_hall_of_records'):
                target = 'r2_records_office'
                rec = None                                  # (The Hall is the Couriers' Office now: a new spot.)
            if target is None:
                continue
            spec_posts = by_record.get(target)
            seat = target in ('rm_greatly_house', 'rm_mossgrave_court', 'rm_fellhall', 'rm_grayrock_hall') or \
                (rec is not None and rec.get('kindOf') in ('villa', 'seat'))
            if seat or (rec is not None and not spec_posts and p['role'] != 'merchant'):
                # A House's own people keep their posts at home (heads, family, servants), and so does anyone whose
                # workplace is kept and has no list of posts (the estates' stables, the summerhouses); not a tavern
                # without its till (GATHERING), whose helpers' work is gone.
                if rec is not None and target not in GATHERING and not label_problems(p['workLabel'], None, clergy=is_clergy(p['workLabel']),
                                                          head=house_head(p['workLabel'])):
                    jobs[pid] = {'kind': 'kept', 'record': target, 'role': p['role'] if p['role'] != 'merchant'
                                 else 'civilian', 'work': self.keep_spot(p['work'])}
                continue
            open_ = [q for q in by_record.get(target, []) if q['filled'] is None]
            if p['role'] == 'merchant':
                kinds = ('keeper', 'help', 'hands', 'staff')
            elif p['role'] == 'guard':
                kinds = ('guard',)
            else:
                kinds = ('staff', 'clergy', 'help', 'hands', 'food')
            slot = None
            for kind in kinds:
                slot = next((q for q in open_ if q['kind'] == kind), None)
                if slot:
                    break
            if slot is None:
                continue
            slot['filled'] = pid
            jobs[pid] = {'kind': slot['kind'], 'post': slot, 'record': target, 'kept': bool(rec) and rec['key'] == target,
                         'work': self.keep_spot(p['work']) if rec else None}
        # 2. The Watch: guards to the Keep's posts, the gates and the patrols, then fifteen more from the idle.
        guards = [pid for pid in adults if self.people[pid]['role'] == 'guard' and pid not in jobs]
        guard_posts = [q for q in posts if q['kind'] in ('guard', 'gate', 'patrol') and q['filled'] is None]

        def label_pref(pid, q):
            t = self.people[pid]['workLabel']
            if q['kind'] == 'patrol':
                route = self.people[pid]['route']
                return 0 if route == q['route'] else 1
            if q['kind'] == 'gate':
                return 0 if t == q['label'] else 2
            return 0 if 'command' in t and 'command' in q['label'] else 3
        for pid in sorted(guards, key=lambda g: (self.people[g]['workLabel'] != 'commanding the Watch', g)):
            if not guard_posts:
                break
            q = min(guard_posts, key=lambda q: label_pref(pid, q))
            guard_posts.remove(q)
            q['filled'] = pid
            jobs[pid] = {'kind': q['kind'], 'post': q, 'record': q['record']}
        idle = lambda pid: pid not in jobs
        recruits = [pid for pid in adults if idle(pid) and 18 <= self.people[pid]['age'] <= 45
                    and self.people[pid]['role'] != 'merchant'
                    and any(w in self.people[pid]['workLabel'] for w in ('carries loads', 'runs messages', 'begs',
                                                                           'begging', 'sweeps and fetches', 'picking rags',
                                                                           'running errands', 'sweeping'))]
        recruits += [pid for pid in adults if idle(pid) and 18 <= self.people[pid]['age'] <= 45 and
                     not self.people[pid]['work'] and self.people[pid]['role'] == 'civilian' and pid not in recruits]
        # Those of the Watch in the world as it is now first, in the same order, so a rewrite keeps them in it (a few
        # inns: a tavern's helper whose work went is not taken into the Watch in a guard's place).
        recruits.sort(key=lambda pid: (self.current.get(pid) or {}).get('role') != 'guard')
        for q in list(guard_posts):
            if not recruits:
                break
            pid = recruits.pop(0)
            q['filled'] = pid
            jobs[pid] = {'kind': q['kind'], 'post': q, 'record': q['record'], 'recruit': True}
        # 3. Fixed posts from those whose posts were lost and the idle, by what they did: first those whose post in a
        # building of the city is gone now, then those in a pool now (no real job), then the rest.

        def lost_first(x):
            c = self.current.get(x, {})
            w = (c.get('work') or {}).get('cell', '')
            rec = placed_rooms.get(w)
            had_post = rec is not None and rec.get('kindOf') not in ('seat', 'villa') and c.get('workLabel') not in \
                pool_labels
            return (0 if had_post else 1 if c.get('workLabel') in pool_labels else 2, x)
        lost = sorted((pid for pid in adults if idle(pid)), key=lost_first)
        PREF = [
            ('washing at the Washhouse', ('washes linen', 'washing clothes')),
            ('keeping the Washhouse', ('washes linen', 'washing clothes')),
            ('lighting the lamps at dusk', ('lights the lamps',)),
            ("carrying for the Couriers' Office", ('runs messages',)),
            ('clerking at the Records Office', ('copying', 'records', 'clerk')),
            ('copying at the Records Office', ('copying', 'records', 'clerk')),
            ('keeping the rolls at the Records Office', ('keeping the records',)),
        ]
        for label, words in PREF:
            for q in posts:
                if q['filled'] is None and q['label'] == label:
                    pid = next((x for x in lost if idle(x) and any(w in self.people[x]['workLabel'] for w in words)), None)
                    if pid:
                        q['filled'] = pid
                        jobs[pid] = {'kind': q['kind'], 'post': q, 'record': q['record']}
        # Stall merchants to the markets (Market Square and the Market Hall from the Commons and the Heights; the Rag
        # Market from the Sump), other merchants to the keepers' places still open, then to new businesses.
        merchants = [pid for pid in adults if idle(pid) and self.people[pid]['role'] == 'merchant']
        sump_stall = [pid for pid in merchants if 'Sump Market' in self.people[pid]['workLabel']]
        other = [pid for pid in merchants if pid not in sump_stall]
        for q in [q for q in posts if q['kind'] == 'stallkeeper' and q['record'] == 'rag_market' and q['filled'] is None]:
            pool = sump_stall or other
            if pool:
                pid = pool.pop(0)
                q['filled'] = pid
                jobs[pid] = {'kind': 'stallkeeper', 'post': q, 'record': 'rag_market'}
        for q in [q for q in posts if q['kind'] in ('stallkeeper', 'hallkeeper', 'keeper') and q['filled'] is None]:
            pool = other or sump_stall
            if not pool:
                break
            # (A merchant of the same trade first, so the fewest change trade: the food rebalance.)
            same = next((x for x in pool if business_for(self.people[x]['workLabel']) == q.get('business')), None)
            pid = same or pool[0]
            pool.remove(pid)
            q['filled'] = pid
            jobs[pid] = {'kind': q['kind'], 'post': q, 'record': q['record']}
        for pid in other + sump_stall:                       # Merchants left over: helpers (civilians).
            q = next((q for q in posts if q['kind'] in ('help', 'hands') and q['filled'] is None), None)
            if q:
                q['filled'] = pid
                jobs[pid] = {'kind': q['kind'], 'post': q, 'record': q['record'], 'demoted': True}
        # Keepers still missing: from the idle civilians, made merchants.
        for q in [q for q in posts if q['kind'] in ('stallkeeper', 'hallkeeper', 'keeper') and q['filled'] is None]:
            pid = next((x for x in lost if idle(x) and 20 <= self.people[x]['age'] <= 59), None)
            if pid:
                q['filled'] = pid
                jobs[pid] = {'kind': q['kind'], 'post': q, 'record': q['record'], 'promoted': True}
        # Clergy: from the chapel's and the healers' titles, then the idle.
        for q in [q for q in posts if q['kind'] == 'clergy' and q['filled'] is None]:
            pid = next((x for x in lost if idle(x) and any(w in self.people[x]['workLabel'] for w in
                                                          ('chapel', 'sick', 'heal', 'rags'))), None) or \
                next((x for x in lost if idle(x) and 18 <= self.people[x]['age'] <= 59 and not self.people[x]['work']), None)
            if pid:
                q['filled'] = pid
                jobs[pid] = {'kind': 'clergy', 'post': q, 'record': q['record']}
        # Every other open post, from whoever is idle: those in a pool now (no real job) first, then those with nothing
        # to do.
        lost.sort(key=lambda x: (lost_first(x)[0], bool(self.people[x]['work']), x))
        for q in posts:
            if q['filled'] is None and q['kind'] not in ('guard', 'gate', 'patrol'):
                pid = next((x for x in lost if idle(x) and self.people[x]['age'] <= 59), None)
                if pid is None:
                    break
                q['filled'] = pid
                jobs[pid] = {'kind': q['kind'], 'post': q, 'record': q['record']}
        # Those of a House's grounds keep their work outdoors there (its gardens and gate).
        grounds = {'estate_mossgrave': 'estate_mossgrave', 'estate_fell': 'estate_fell', 'estate_grayrock':
                   'estate_grayrock', 'greatly': 'greatly', 'rise': 'mansion', 'palace': 'palace'}
        # (Not those in a pool now: the class pass put them there, and they are kept there below.)
        for pid in [x for x in adults if idle(x) and self.people[x]['age'] <= 69
                    and (self.current.get(x) or {}).get('workLabel') not in pool_labels]:
            home = self.plan[pid]['home']['cell']
            area = self.room_record[home]['area'] if home in self.room_record else None
            label = self.people[pid]['workLabel']
            if area in grounds and not label_problems(label, None, clergy=False, head=house_head(label)) and \
                    self.people[pid]['role'] != 'merchant':
                key = grounds[area]
                where = f'{key}_gate' if 'gate' in label and f'{key}_gate' in self.city.posts else f'{key}_gardens'
                jobs[pid] = {'kind': 'outdoor', 'where': where, 'record': None}
        # 4. Elastic pools: every placed NPC aged 16-59 still idle.
        caps = Counter()
        pool_list = list(POOLS)
        for pid in [x for x in adults if idle(x) and self.people[x]['age'] <= 59]:      # (Their pool now, if it has room.)
            now = self.current.get(pid, {}).get('workLabel')
            choice = next((p for p in pool_list if p[1] == now and caps[now] < p[2]), None)
            if choice:
                caps[now] += 1
                jobs[pid] = {'kind': 'pool', 'label': now, 'where': choice[3], 'record': choice[3]}
        for pid in [x for x in adults if idle(x) and self.people[x]['age'] <= 59]:
            fish ='fishes' in self.people[pid]['workLabel'] or 'nets' in self.people[pid]['workLabel']
            choice = None
            for dist, label, cap, where in pool_list:
                if fish and where != 'pier':
                    continue
                if caps[label] < cap:
                    choice = (dist, label, cap, where)
                    break
            if choice is None:
                choice = ('wharf', 'unloading ships at the piers', 999, 'pier')
            caps[choice[1]] += 1
            jobs[pid] = {'kind': 'pool', 'label': choice[1], 'where': choice[3], 'record': choice[3]}
        self.jobs_ = jobs
        # Write each placed NPC's work into the plan.
        for pid in sorted(self.placed):
            p = self.people[pid]
            entry = self.plan[pid]
            j = jobs.get(pid)
            entry['at'] = (j['post'].get('business') if j and j.get('post') and j['kind'] in ('stallkeeper', 'hallkeeper')
                           else self.at.get(j.get('record')) if j else None)
            age = p['age']
            j = jobs.get(pid)
            if j is None:
                if 6 <= age <= 15:
                    school = 'r2_abbey_school' if self.rng.random() < .35 else 'rm_the_commons_school'
                    now = self.current.get(pid, {})
                    for key in ('r2_abbey_school', 'rm_the_commons_school'):      # (The school they go to now.)
                        if now.get('workLabel') == 'at lessons' and (now.get('work') or {}).get('cell') in \
                                {r['id'] for r in self.records[key]['rooms']}:
                            school = key
                    room = self.records[school]['rooms'][0]['id']
                    spot = self.floor(room)
                    if now.get('workLabel') == 'at lessons' and (now.get('work') or {}).get('cell') == room:
                        spot = dict(now['work'])                # (The place they sit now: a rewrite moves nobody.)
                    entry.update({'work': spot, 'business': None, 'role': 'civilian', 'post': 'at lessons',
                                  'record': school})
                else:
                    entry.update({'work': None, 'business': None, 'role': 'civilian',
                                  'post': 'retired' if age >= 60 else 'a child at home', 'record': None})
                continue
            self.work_entry(pid, j, entry)

    def keep_current(self, posts, jobs, adults, placed_rooms):
        """Each idle NPC who holds an open post in the world as it is now (`self.current`) keeps it: a staff or clergy
        post by its label, a keeper's, a helper's or a hand's by the building, a market stall or counter by its place."""
        HELPING = ('helping', 'working', 'printing', 'works at', 'minding')

        def near(spot, cell, x, y):
            return spot and spot['cell'] == cell and abs(spot['x'] - x) <= 2 and abs(spot['y'] - y) <= 2
        by_record = defaultdict(list)
        for q in posts:
            if q['filled'] is None and q['kind'] not in ('guard', 'gate', 'patrol'):
                by_record[q['record']].append(q)
        for pid in adults:
            c = self.current.get(pid)
            if pid in jobs or not c or not c.get('work') or not c.get('home'):
                continue
            label, spot = c['workLabel'], c['work']
            rec = placed_rooms.get(spot['cell'])
            cands = list(by_record.get(rec['key'], [])) if rec else []
            if spot['cell'].startswith('w_ridgemere_') and c['role'] == 'merchant':
                cands += by_record.get('market_square', []) + by_record.get('rag_market', [])
            for q in cands:
                if q['filled'] is not None:
                    continue
                kind = q['kind']
                if kind in ('staff', 'clergy', 'food'):
                    ok = q['label'] == label
                elif kind in ('help', 'hands'):
                    ok = c['role'] != 'merchant' and label.startswith(HELPING)
                elif kind == 'keeper':
                    ok = c['role'] == 'merchant'
                elif kind == 'stallkeeper':
                    x, y = self.city.posts[q['record']][q['index']]
                    cx, cy = x // 256, y // 256
                    ok = near(spot, f'w_ridgemere_{cx}_{cy}', x % 256, y % 256)
                elif kind == 'hallkeeper':
                    room = rec['rooms'][0] if rec else None
                    w = room['work'][q['index']] if room and q['index'] < len(room['work']) else None
                    ok = bool(w) and near(spot, room['id'], w['x'], w['y']) and c['role'] == 'merchant'
                else:
                    ok = False
                if ok:
                    q['filled'] = pid
                    inside = kind in ('staff', 'clergy', 'food', 'help', 'hands', 'keeper')
                    jobs[pid] = {'kind': kind, 'post': q, 'record': q['record'], 'current': True, 'kept': inside,
                                 'work': self.keep_spot(spot) if inside else None}
                    break

    def work_entry(self, pid, j, entry):
        p = self.people[pid]
        kind = j['kind']
        post = j.get('post')
        role = 'merchant' if kind in ('keeper', 'stallkeeper', 'hallkeeper') else 'guard' if kind in \
            ('guard', 'gate', 'patrol') else 'civilian'
        rec = self.records.get(j.get('record')) if j.get('record') else None
        work = j.get('work')
        route = ''
        business = post.get('business') if post else None
        if kind == 'kept':
            entry.update({'work': work, 'business': None, 'role': p['role'] if p['role'] != 'merchant' else 'civilian',
                          'post': 'kept', 'labelKept': True, 'record': j['record'], 'route': p['route']})
            return
        if kind == 'outdoor':
            spots = self.city.posts[j['where']]
            x, y = spots[self.rng.randrange(len(spots))]
            work = self.outdoor(x, y)
            now = (self.current.get(pid) or {}).get('work')
            if now and work and now['cell'] == work['cell'] and any(
                    (sx // 256, sy // 256) == tuple(map(int, now['cell'].rsplit('_', 2)[1:])) and
                    abs(sx % 256 - now['x']) <= 2 and abs(sy % 256 - now['y']) <= 2 for sx, sy in spots):
                work = dict(now)                            # (The spot they work now, by the same post.)
            entry.update({'work': work, 'business': None, 'role': 'civilian', 'post': 'kept',
                          'labelKept': True, 'record': None, 'route': p['route']})
            return
        if kind == 'pool':
            where = j['where']
            if where in self.city.posts:
                spots = self.city.posts[where]
                x, y = spots[self.rng.randrange(len(spots))]
                work = self.outdoor(x, y)
            elif where == 'sump_stairs':
                work = self.outdoor(*self.city.spots['sump_stairs'])
            else:
                work = self.floor(self.records[where]['rooms'][0]['id'])
            now = self.current.get(pid, {})
            if now.get('workLabel') == j['label'] and now.get('work') and work and \
                    now['work']['cell'].startswith('w_') == work['cell'].startswith('w_'):
                work = dict(now['work'])                    # (The same pool: the spot they work now.)
            entry.update({'work': work, 'business': None, 'role': 'civilian', 'post': 'pool', 'template': j['label'],
                          'record': where})
            return
        if kind in ('stallkeeper',):
            x, y = self.city.posts[post['record']][post['index']]
            work = self.outdoor(x, y)
        elif kind == 'hallkeeper':
            room = rec['rooms'][0]
            spots = room['work']
            sx, sy = spots[post['index']]['x'], spots[post['index']]['y']
            work = self.spot_near(room['id'], sx, sy)
        elif kind == 'gate':
            gx, gy = self.city.gate_inside[post['gate']]
            work = self.outdoor(gx, gy)
        elif kind == 'patrol':
            route = post['route']
            work = None
        elif work is None and rec is not None:
            z = POST_FLOOR.get((j.get('record'), post.get('label') if post else None))
            if z is not None:                           # A post on its own floor (the middle of the room).
                room = self.rooms[next(r['id'] for r in rec['rooms'] if r['z'] == z)]
                work = self.spot_near(room['id'], room['width'] // 2, room['height'] // 2)
            work = work or self.work_spot(rec, kind)
        keeper_word = post.get('label') if post else None
        entry.update({'work': work, 'business': business, 'role': role, 'post': kind, 'template': keeper_word,
                      'record': j.get('record'), 'route': route, 'kept': bool(j.get('kept')),
                      'recruit': bool(j.get('recruit')), 'promoted': bool(j.get('promoted')),
                      'demoted': bool(j.get('demoted'))})

    def keep_spot(self, spot):
        """A kept work spot, or the nearest one that can be reached (and isn't taken)."""
        if spot is None:
            return None
        got = self.spot_near(spot['cell'], spot['x'], spot['y'])
        return got or self.floor(spot['cell'])

    def work_spot(self, rec, kind):
        """A work spot in a building: one of its rooms' marked places, else a free floor tile (a keeper's place first)."""
        for room in rec['rooms']:
            for w in room['work']:
                if (room['id'], w['x'], w['y']) not in self.taken and self.walkable(room['id'], w['x'], w['y']):
                    got = self.spot_near(room['id'], w['x'], w['y'])
                    if got:
                        return got
        ground = rec['rooms'][0]['id']
        return self.floor(ground)

    # -- evenings ------------------------------------------------------------------------------------------------------
    def evenings(self):
        """The nearest tavern or inn of the home's district, at most 40 to a place; parents of children under six,
        children, and the Rise's, the compound's, the palace's and the estates' people at home (section 5)."""
        taverns = {'sump': ['rm_the_bilge', 'rm_the_rainbarrel', 'r2_the_knucklebones', 'rm_the_soup_kitchen'],
                   'wharf': ['rm_the_drowned_lantern', 'rm_the_tarred_rope', 'rm_the_sailors_rest'],
                   'cinders': ['rm_the_slag_anchor'],
                   'commons': ['rm_the_black_tun', 'rm_the_last_lamp', 'rm_the_grey_gull_inn'],
                   'northgate': ['rm_the_grey_goose', 'rm_the_black_tun', 'rm_the_last_lamp',
                                 'rm_the_grey_gull_inn'],
                   'admin': ['rm_the_slate_ledger']}
        load = Counter()
        young_parents = set()
        rel = self.project.get('relations', [])
        for r in rel:
            if r['kind'] == 'parent' and r['other'] in self.placed and self.people[r['other']]['age'] < 6:
                young_parents.add(r['npc'])
        for pid in sorted(self.placed):
            entry = self.plan[pid]
            p = self.people[pid]
            home = entry['home']
            hdist = self.home_cap.get(home['cell'], {}).get('district') if home else None
            if hdist is None:
                rec = self.room_record.get(home['cell']) if home else None
                hdist = rec['area'] if rec else None
            ev = None
            if p['age'] >= 16 and pid not in young_parents:
                if entry.get('role') == 'guard':
                    ev = 'rm_the_watch_house'
                elif entry.get('post') == 'clergy':
                    ev = 'r2_cathedral'
                else:
                    for t in taverns.get(hdist, []):
                        if load[t] < 40:
                            ev = t
                            break
            if ev:
                room = self.records[ev]['rooms'][0]['id'] if ev in self.records else ev
                load[ev] += 1
                now = (self.current.get(pid) or {}).get('evening')
                # (The same place as now: the spot they spend it at, so a rewrite keeps the evenings as they are.)
                entry['evening'] = (now and now['cell'] == room and self.spot_near(room, now['x'], now['y'])) or \
                    self.floor(room)
                entry['eveningPlace'] = ev
            else:
                entry['evening'] = dict(home) if home else None
                entry['eveningPlace'] = 'home'
        self.evening_load = load


def class_summary(placement, pids, label_of):
    """The class-match report (doc 67, Progress, "Jobs by class"): per household class, how many earners hold a job of
    their class, and the rest with the reason; the Watch and the clergy apart; the Minor Houses' upper jobs; and who from
    the Sump, the Cinders or the wharf keeps a larger business."""
    out = {c: {'earners': 0, 'matched': 0} for c in ('upper', 'middle', 'lower')}
    unmatched, own = [], Counter()
    upper_households, minor_households, large_lower, rag_lower = set(), set(), [], []
    for pid in pids:
        e = placement.plan[pid]
        label = label_of(pid)
        jc, jwhy = placement.job_class(pid, e, label)
        hc, hwhy = placement.home_class(pid)
        family = placement.people[pid]['name'].split()[-1]
        hh = placement.par.get(pid, pid)
        if hc == 'upper' and family in MINOR_FAMILIES:
            minor_households.add(hh)
        if jc is None:
            continue
        if jc == 'upper' and hc == 'upper':
            upper_households.add(hh)
        if hc == 'lower' and placement.large_keeper(e):
            large_lower.append(f'{pid} ({hwhy}): {label}')
        if hc != 'middle' and placement.rag_keeper(e):
            rag_lower.append(f'{pid} ({hc}, {hwhy}): {label}')
        if jc in ('clergy', 'watch') or hc == 'watch':
            own[jc if jc in ('clergy', 'watch') else 'watch'] += 1
            continue
        out[hc]['earners'] += 1
        if jc == hc:
            out[hc]['matched'] += 1
        else:
            an = lambda w: f'an {w}' if w[0] in 'aeiou' else f'a {w}'
            unmatched.append(f'{pid}: {an(hc)} household ({hwhy}) in {an(jc)} job ({jwhy}): {label}')
    earners = sum(v['earners'] for v in out.values())
    matched = sum(v['matched'] for v in out.values())
    for v in out.values():
        v['rate'] = round(v['matched'] / v['earners'], 3) if v['earners'] else 1.0
    return {'classes': out, 'earners': earners, 'matched': matched, 'rate': round(matched / earners, 3) if earners else 1.0,
            'ownCases': dict(own), 'unmatched': unmatched,
            'minorWithoutUpper': sorted(minor_households - upper_households), 'largeLower': large_lower,
            'ragNotMiddle': rag_lower}


def class_report(placement, plan, labels):
    """The class-match report of the finished plan (its labels rendered), with the class pass's before."""
    pids = [pid for pid in sorted(plan) if plan[pid].get('placed')]
    after = class_summary(placement, pids, lambda pid: labels[pid])
    moves = sorted(getattr(placement, 'class_moves', {}).items())
    return {'before': getattr(placement, 'class_before', None), 'after': after,
            'moves': [f'{pid} ({why}): {placement.home_class(pid)[0]}, now {labels[pid]}' for pid, why in moves]}


def render_labels(plan, people, names, current=None):
    """Each placed NPC's work label, from its post and the (freshly loaded) names of the interiors (doc 67, section 5:
    "Labels at write time"). `names`: {structure key: its name}."""
    out = {}
    for pid, e in plan.items():
        if not e.get('placed'):
            continue
        p = people[pid]
        post, tpl = e.get('post'), e.get('template')
        name = names.get(e.get('record'), '')
        label = None
        if e.get('labelKept'):
            label = (current or {}).get(pid, p['workLabel'])
            if label_problems(label, None, clergy=is_clergy(label), head=house_head(label), at=e.get('at')):
                label = reword(label)
        elif post == 'at lessons':
            label = 'at lessons'
        elif post in ('retired',):
            label = p['workLabel'] if p['age'] >= 60 else 'at home'
        elif post == 'a child at home':
            label = p['workLabel'] if p['age'] < 6 else 'at home'
        elif post == 'keeper':
            # The label as the world has it now (renamed by doc 67's lane C, when it has run), not the one before the city.
            kept = (current or {}).get(pid, p['workLabel']) if e.get('kept') and p['role'] == 'merchant' else None
            if kept and business_for(kept) == e.get('business') and not label_problems(kept, e.get('business')):
                label = kept
            elif tpl in (None, 'runs'):
                label = fit(f'runs {name}')
            elif ' ' in tpl and not tpl.endswith(' at'):
                label = fit(tpl) if tpl.startswith(('keeping', 'ladling', 'cooking', 'brewer', 'builder', 'pressing',
                                                    'distiller', 'preserver', 'cheesemaker', 'ropemaker', 'rag-and-bone')) and \
                    tpl not in ('rag-and-bone dealer',) else fit(f'{tpl} at {name}')
            else:
                label = fit(f'{tpl} at {name}')
        elif post in ('stallkeeper', 'hallkeeper', 'gate', 'patrol', 'guard', 'staff', 'clergy', 'food', 'pool'):
            if post in ('gate', 'patrol', 'guard') and e.get('kept') is False and not e.get('recruit') and \
                    p['role'] == 'guard' and post != 'guard' and p['workLabel'] == tpl:
                label = p['workLabel']
            else:
                label = tpl
        elif post in ('help', 'hands'):
            at = e.get('at')
            kept = (current or {}).get(pid, p['workLabel']) if e.get('kept') else None
            options = ([kept] if kept and (kept.startswith(('helping', 'minding', 'works at'))) else []) + \
                [fit(f'helping at {name}' if post == 'help' else f'working at {name}')] + \
                ([fit(f'printing at {name}')] if at == 'printing_house' else []) + \
                ['helping in the shop' if post == 'help' else 'working in the yard']
            label = next((o for o in options if not label_problems(o, None, at=at)), options[-1])
        if not label:
            label = p['workLabel']
        out[pid] = label
    return out


# --- People's text (doc 67, Progress, "Food rebalance") ------------------------------------------------------------
# A placed NPC's description is "A <build> <coat> wolf with <mark>, <what they do>." and their greeting "<an opening>
# <a line of their work>" (worldgen.residents). Where the work part no longer fits the job (they moved, or their job
# changed), it is written again from the post; whoever changes job here also gets a line of backstory.
OPENINGS = ('Welcome, welcome.', 'Say what you need.', 'Yes?', 'Well, look at you.', 'Oh, a new face. Where from?',
            'You have found the best in the city.', 'Take your time; there is no hurry.', 'Ho! Come in, come in!',
            'Looking, or buying?', 'Mm. Morning. Or whatever it is.', 'Good day to you.')
# business: (keeper's noun, what a stall of it sells, words that show the job, greeting lines, a hand's work, backstory)
TRADE_TEXT = {
    'bakery': ('baker', 'bread', ('bak', 'bread', 'loaf', 'loaves', 'oven', 'bun'),
               ('Bread from the oven, a penny the loaf.', 'Still warm. Mind your paws on the crust.',
                'Honey cakes after noon, if there are any left.'),
               'who works the ovens at {name}',
               ('{first} went to the ovens when the city wanted more bread than it had bakers, and has been up before '
                'the bells every day since.', '{first} learned to bake in a hurry, the season the markets went over '
                'to food; the loaves are better now.')),
    'butcher': ('butcher', 'meat', ('butcher', 'meat', 'mutton', 'cleaver', 'block', 'sausage', 'pork', 'marrow'),
                ('Mutton, rabbit or pork? The bones go free with it.', 'Fresh off the block this morning.',
                 'Salt pork by the slab, if you are going far.'),
                'who cuts and hangs the meat at {name}',
                ('{first} took up the cleaver when the city wanted more butchers, and has steady paws for it.',
                 '{first} came to the meat trade late, and talks about it as if born to it.')),
    'inn': ('cook', 'soup', ('tavern', 'taproom', 'inn', 'soup', 'stew', 'cook', 'supper', 'ale', 'bowl', 'hearth',
                             'ladle', 'kitchen'),
            ('Stew in the pot, bread on the side.', 'A hot bowl for a penny. Sit, if there is room.',
             'Soup, and plenty of it. Mind, it is hot.'),
            'who serves at {name}',
            ('{first} ladles out more hot suppers in a day now than in a week before the markets changed.',
             '{first} took to cooking for strangers when the city wanted feeding, and found it suited.')),
    'provisioner': ('provisioner', 'provisions', ('provision', 'stores', 'salt', 'oats', 'flour', 'larder', 'supplies'),
                    ('Oats, flour, salt fish. What are you short of?', 'Weighed fair, every ounce of it.',
                     'Smoked, salted or dried: it keeps.'),
                    'who weighs out the stores at {name}',
                    ('{first} gave up the old trade for provisions when the city turned to food, and has never sold '
                     'so much.', '{first} keeps the stores counted to the last sack, and knows who is behind on '
                     'paying.')),
    'preserver': ('preserver', 'preserves', ('preserv', 'jam', 'dried', 'apple', 'crock', 'jar'),
                  ('Preserves that keep till spring, dried apples by the string.',
                   'Taste? Go on. The honey is from Fellhall.', 'Jars back, if you have them; a farthing off.'),
                  'who stirs the preserving pans at {name}',
                  ('{first} took up the preserving pans when the city wanted food that keeps, and smells of apples '
                   'from dawn to dark.', '{first} learned preserving from a Fell cook and swears by honey, not sugar.')),
    'dairy': ('cheesemaker', 'cheese', ('chees', 'dairy', 'butter', 'milk', 'round', 'curd'),
              ('Cheese by the round or the wedge. Butter in the crock.', 'This one is a month old. That one, three.',
               'Milk from the morning carts, while it lasts.'),
              'who turns the cheeses at {name}',
              ('{first} came to cheesemaking when the city wanted more food than it had sellers, and talks of rinds '
               'now the way others talk of the weather.', '{first} keeps the cheeses turned to the day, and the '
               'butter cool even in summer.')),
    'fishmonger': ('fishmonger', 'fish', ('fish', 'catch', 'eel', 'herring', 'gut'),
                   ('Caught this morning, most of it.', 'Fish! Fresh off the boats!',
                    'Salt fish keeps. Fresh fish does not.'),
                   'who guts and salts the fish at {name}',
                   ('{first} has gutted fish since the markets went over to food, and no longer notices the smell.',
                    '{first} can tell a day-old herring at ten paces.')),
    'stall': ('stallholder', 'greens', ('stall', 'greens', 'fruit', 'veg', 'produce', 'turnip', 'apple'),
              ('Greens, turnips, apples. Cheap, and none of it rotten.', 'Fresh off the country carts this morning.',
               'Two for a farthing, and I will pick you good ones.'),
              'who helps at {name}',
              ('{first} took a food stall when the market went over to food, and sells out most days.',
               '{first} is at the carts before dawn to have the best of the country\'s greens.')),
    'carters': ('carter', 'cartage', ('cart', 'wagon', 'team', 'load', 'haul', 'carter', 'road'),
                ('Out at dawn, back after dark. What do you want carried?', 'Mind the wheels.',
                 'The country road is mud to the hocks. We go anyway.'),
                'who drives a team for {name}',
                ('{first} drives for the Carters\' Yard now, out on the country roads more days than not, and is paid '
                 'by the load.', '{first} took to the wagons when the Carters\' Yard wanted drivers, and knows every '
                 'rut between the East Gate and Fellhall.')),
}
STALL_THINGS = {'herbalist': 'herb', 'rag_and_bone': 'second-hand', 'chandlery': 'candle', 'cooperage': "cooper's",
                'carpenter': "carpenter's"}
WHERE = {'market_square': 'on Market Square', 'rag_market': 'in the Rag Market', 'r2_market_hall': 'in the Market Hall'}
GENERIC_BACK = ('{first} took this work when the city gave its markets over to food, and has kept at it.',)
# Posts by their label: (label or its start, what they do, words that show it, a greeting line).
POST_TEXT = [
    ('unloading ships at the piers', 'a docker on the piers', ('dock', 'pier', 'ship', 'cargo', 'wharf', 'unload'),
     'Mind your feet. Cargo coming through.'),
    ('working the cranes', 'who works the wharf cranes', ('crane', 'wharf', 'pier', 'ship', 'hook'),
     'Stand clear of the hook.'),
    ('carrying from Sump Stairs', 'a porter at the Sump Stairs', ('porter', 'carr', 'load', 'stair'),
     'Anything carried, anywhere in the city, for a copper or two.'),
    ('portering in Market Square', 'a market porter on Market Square', ('porter', 'carr', 'load', 'market'),
     'Carry that for you? Two coppers to the Commons.'),
    ('crying wares in the Rag Market', 'who cries wares from a tray in the Rag Market', ('tray', 'wares', 'seller'),
     'Laces, buttons, a needle? Cheap as you like.'),
    ('washing at the Washhouse', 'a washer at the Washhouse', ('wash', 'laund', 'linen'), 'A penny a bundle, back by dark.'),
    ('keeping the Washhouse', 'who keeps the Washhouse', ('wash', 'laund', 'linen'), 'A penny a bundle, back by dark.'),
    ('driving for the carters', "who drives a team for the Carters' Yard", TRADE_TEXT['carters'][2],
     'Out at dawn, back after dark. What do you want carried?'),
    ('keeping the rolls', 'the keeper of the rolls at the Records Office', ('rolls', 'record'),
     'Every birth, every death, every debt.'),
    ('clerking at', 'a clerk at {at}', ('clerk', 'record', 'office', 'ledger'),
     'It is written down somewhere. I will find it.'),
    ('copying at', 'a copyist at {at}', ('copy', 'copi', 'clerk', 'scribe', 'record'), 'Fair copies, two days, no blots.'),
    ('clerking in', 'a clerk in {at}', ('clerk', 'office', 'ledger', 'wing'), 'It is written down somewhere. I will find '
     'it.'),
    ('clerking for the magistrate', "the magistrate's clerk", ('clerk', 'magistr', 'court'),
     'The court sits at ten. Do not be late.'),
    ('weighing at', 'a weigher at {at}', ('weigh', 'custom', 'scale'),
     'Every bale on the scales, and the Crown takes its share.'),
    ('tallying in', 'who tallies the bales in {at}', ('tally', 'warehouse', 'store', 'bale', 'clerk'),
     'Every bale counted in and counted out.'),
    ("carrying for the Couriers' Office", "a courier for the Couriers' Office", ('courier', 'letter', 'messag', 'post'),
     'A letter for you? No? Then let me by.'),
    ('lighting the lamps at dusk', "one of the city's lamplighters", ('lamp', 'light'), 'Dusk comes early in the rain.'),
    ("serving in the King's household", "a servant in the King's household", ('servant', 'king', 'royal', 'palace'),
     'The household keeps its own hours.'),
    ("cooking for the King's household", "a cook in the King's household", ('cook', 'kitchen', 'king'),
     'The King eats what the city eats, only more of it.'),
    ('serving at ', 'a servant at {at}', ('servant', 'serv', 'household'), 'Mind the floors, they have just been done.'),
]
GUARD_WORDS = ('watch', 'guard', 'gate', 'patrol', 'wall', 'captain', 'sergeant', 'cells', 'gaol')
CLERGY_TEXT = ('abbey', 'cathedral', 'priest', 'chapel', 'brother', 'sister', 'choir', 'deacon', 'sexton', 'abbot',
               'almoner', 'scribe', 'healer', 'infirm', 'nurs', 'church')
STOP = {'the', 'with', 'working', 'helping', 'keeping', 'from', 'into', 'that', 'this', 'their', 'there', 'today', 'runs',
        "king's", "city's"}
UNIQUE = ('abbot', 'harbourmaster', 'chancellor', 'royal secretary', 'herald', 'keeper', "crown's", 'warden')


def _words(*texts):
    out = []
    for t in texts:
        for w in re.findall(r"[a-z']+", (t or '').lower()):
            if len(w) >= 4 and w not in STOP:
                out.append(w[:6].rstrip("'"))
    return tuple(out)


def _noun_phrase(label):
    """A post's label as what someone is: "a priest at the cathedral", "the abbot of the cathedral", "who spends the
    days hearing cases"."""
    first, _, rest = label.partition(' ')
    if first in VERBS:
        return f'who {VERBS[first]} {rest}'
    if first.endswith('ing'):
        return f'who spends the days {label}'
    if label.startswith('the '):
        return label
    return f'the {label}' if label.startswith(UNIQUE) else f'an {label}' if label[0] in 'aeiou' else f'a {label}'


VERBS = {'working': 'works', 'keeping': 'keeps', 'teaching': 'teaches', 'singing': 'sings', 'weighing': 'weighs',
         'guarding': 'guards', 'walking': 'walks', 'holding': 'holds', 'serving': 'serves', 'minding': 'minds',
         'helping': 'helps', 'hearing': 'hears', 'nursing': 'nurses', 'commanding': 'commands', 'running': 'runs',
         'clerking': 'clerks', 'collecting': 'collects', 'reading': 'reads', 'tending': 'tends'}
KEY_TITLES = ('Lord', 'Lady', 'Chancellor', 'Captain', 'Magistrate', 'King of', 'Queen', 'Abbot', 'Prelate', 'Speaker',
              ' heir', 'steward', ' wife', 'husband', 'daughter', 'widow')


def hash_pick(key, n):
    """A choice that doesn't move between runs (Python's hash() of a string does)."""
    return sum(map(ord, key)) % n if n else 0


def job_text(entry, label, names):
    """What a placed NPC's post is, in words: (what they do, a greeting line, words that show it, backstory lines), or
    None where the text is their own (kept posts, children, the retired, the grounds' people)."""
    post, business, record = entry.get('post'), entry.get('business'), entry.get('record')
    if post in (None, 'kept', 'at lessons', 'retired', 'a child at home', 'outdoor') or entry.get('labelKept'):
        return None
    name = names.get(record, '') if record else ''
    t = TRADE_TEXT.get(business or entry.get('at'))
    if post in ('keeper', 'stallkeeper', 'hallkeeper') and business:
        noun = label.split(' at ')[0].split(' on ')[0].split(' in ')[0]
        thing, words, greet, back = noun, (), (), GENERIC_BACK
        if t:
            noun, thing, words, greet, _, back = t
        if post == 'stallkeeper':
            thing = label.split(' stall')[0] if business == 'stall' else STALL_THINGS.get(business, thing)
            phrase = f'who keeps a {thing} stall {WHERE[record]}'
        elif post == 'hallkeeper':
            thing = STALL_THINGS.get(business, thing)
            phrase = f'who keeps a {thing} counter in the Market Hall'
        elif business == 'inn':
            phrase = (f'who keeps the taproom at {name}' if 'taproom' in label else f'who ladles the soup at {name}'
                      if 'ladling' in label else f'who keeps the cookshop at {name}' if 'cooking' in label
                      else f'who keeps {name}')
        elif business == 'carters':
            phrase = f'who keeps {name} by the East Gate'
        elif label.startswith('runs '):
            phrase = f'who runs {name}'
        else:
            phrase = f'who keeps {name}' if noun.endswith('ing') else f'the {noun} who keeps {name}'
        line = greet[hash_pick(label, len(greet))] if greet else f'Welcome to {name or "the market"}.'
        shown = _words(thing, noun) if post != 'keeper' else _words(name, label, thing)   # (A market's name shows nothing.)
        return phrase, line, tuple(words) + shown, back
    if post in ('help', 'hands'):
        phrase = t[4].format(name=name) if t else f'who helps at {name}' if post == 'help' else f'a hand at {name}'
        greet = t[3][hash_pick(label, len(t[3]))] if t else 'Mind yourself; there is work going on.'
        return phrase, greet, (t[2] if t else ()) + _words(name) + ('help', 'hand', 'shop', 'work', 'family', 'eldest'), \
            (t[5] if t else GENERIC_BACK)
    if post in ('gate', 'patrol', 'guard'):
        phrase = _noun_phrase(label) if 'Watch' in label else f'a guard of the Watch, {label}'
        return phrase, 'Keep moving. Keep dry, if you can.', GUARD_WORDS, \
            ("{first} took the Watch's coat when the city wanted guards, and has kept it on.",)
    if post == 'clergy':
        return _noun_phrase(label), 'The abbey keeps its doors open. Peace on you.', CLERGY_TEXT + _words(label), \
            ("{first} came to the abbey's work when it wanted hands, and stayed.",)
    for start, phrase, words, greet in POST_TEXT:
        if label.startswith(start):
            at = label[len(start):].strip() or name
            return phrase.format(at=at), greet, tuple(words) + _words(at), GENERIC_BACK
    return _noun_phrase(label), 'There is work to be done.', _words(label, name), GENERIC_BACK


def split_description(text):
    """(the looks, what they do) of a generated description, or None for a description of its own."""
    for mark in MARKS:
        at = text.find(f'with {mark}, ')
        if at >= 0:
            cut = at + len(f'with {mark}')
            return text[:cut], text[cut + 2:].rstrip('.')
    return None


def split_greeting(text):
    for opening in sorted(OPENINGS, key=len, reverse=True):
        if text.startswith(opening):
            return opening, text[len(opening):].strip()
    return None


def gerund(verb):
    """"carries" -> "carrying", "runs" -> "running", "lights" -> "lighting"."""
    if verb.endswith('ies'):
        base = verb[:-3] + 'y'
    elif verb.endswith(('shes', 'ches', 'xes', 'sses')):
        base = verb[:-2]
    else:
        base = verb[:-1]
    if base.endswith('e') and not base.endswith('ee'):
        return base[:-1] + 'ing'
    if len(base) == 3 and base[0] not in 'aeiou' and base[1] in 'aeiou' and base[2] not in 'aeiouwxy':
        return base + base[-1] + 'ing'
    return base + 'ing'


def job_lines():
    """Every line of backstory the people's-text passes write for a job (TRADE_TEXT's, CLASS_BACK's, the Watch's, the
    abbey's and GENERIC_BACK), as templates."""
    out = [line for t in TRADE_TEXT.values() for line in t[5]] + [line for v in CLASS_BACK.values() for line in v]
    return out + list(GENERIC_BACK) + ["{first} took the Watch's coat when the city wanted guards, and has kept it on.",
                                       "{first} came to the abbey's work when it wanted hands, and stayed."]


def old_job_lines(story, first, keep=None):
    """A backstory without the lines the people's-text passes wrote for an earlier job (`keep` stays)."""
    for t in job_lines():
        pattern = re.escape(t).replace(re.escape('{first}'), re.escape(first)).replace(re.escape('{name}'), r"[^.]+?")
        for m in list(re.finditer(r'(?:^|\s)(' + pattern + ')', story)):
            if m.group(1) != keep:
                story = story.replace(m.group(1), '', 1)
    return re.sub(r'\s{2,}', ' ', story).strip()


def person_text(person, entry, label, before_label, names):
    """A placed NPC's description, greeting and backstory for their post: ({field: new text} for what changes, why):
    'job' when their job changed in this write, 'stale' when the work in their text no longer fits it, 'grammar' for a
    greeting like "I am carries loads for hire today" (anyone placed: then only the verb is mended)."""
    mended = re.sub(r"\bI am ([a-z]+s)\b", lambda m: m.group(0) if m.group(1) in ('always', 'yours', 'perhaps', 'less')
                    else f'I am {gerund(m.group(1))}', person['greeting'])
    grammar = ({'greeting': mended}, 'grammar') if mended != person['greeting'] else ({}, None)
    if person['id'] in HOUSE_POSTS or person['id'] in ROYAL_TEXT or person['id'] in MINOR_TEXT or person['age'] < 16:
        return grammar
    look = split_description(person['description'])
    if look is None or any(t in look[1] for t in KEY_TITLES):
        return grammar                  # (A figure of the city with a description of their own: the Houses, the Crown...)
    job = job_text(entry, label, names)
    if job is None:
        return grammar
    phrase, line, words, back = job
    if entry.get('classMove') in CLASS_BACK:            # (Moved by the jobs by class: a line of why.)
        back = CLASS_BACK[entry['classMove']]
    out, why = {}, None
    changed = before_label is not None and before_label != label
    desc = split_description(person['description'])
    greet = split_greeting(person['greeting'])
    stale = desc is not None and not any(w in desc[1].lower() for w in words)
    broken = greet is not None and re.match(r"I am [a-z]+s\b", greet[1]) is not None
    if changed or stale:
        why = 'job' if changed else 'stale'
        if desc is not None:
            out['description'] = f'{desc[0]}, {phrase}.'
        out['greeting'] = f'{greet[0] if greet else "Good day to you."} {line}'
        if changed:
            first = person['name'].split()[0]
            extra = back[hash_pick(person['id'], len(back))].format(first=first,
                                                                    name=names.get(entry.get('record'), '') or 'it')
            # (A few inns, dairies later: the lines these passes wrote for an earlier job go, so a cheesemaker turned
            # docker no longer "keeps the cheeses turned to the day"; the NPC's own backstory stays.)
            story = old_job_lines(person['backstory'], first, keep=extra)
            if extra not in story:
                story = f'{story.rstrip()} {extra}'.strip()
            out['backstory'] = story
    elif broken:
        why = 'grammar'
        out['greeting'] = f'{greet[0]} {line}'
    out = {k: v for k, v in out.items() if v != person.get(k)}
    return out, (why if out else None)
