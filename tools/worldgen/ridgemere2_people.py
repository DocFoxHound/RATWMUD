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


def reused_houses(project, structures, placed, reuse_ids):
    """The old houses kept (doc 67, section 3: 40 middle-class, 8 clerks', 6 Cinders row houses): the single-room
    houses most of the placed live in, by where their people work."""
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
        elif len(out['commons']) + len(out['northgate']) < 40:
            out['northgate' if cell in ('w_ridgemere_1_0',) and len(out['northgate']) < 14 else 'commons'].append(gid)
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
def k(word, business=None):
    return ('keeper', word, 1, business)


POSTS = {
    # The wharf.
    'rm_the_harbourmaster_s_office': [('staff', 'harbourmaster of the Wharf Office', 1), ('staff', 'clerking at the Wharf Office', 2)],
    'r2_customs_house': [('staff', "collecting the Crown's customs", 1), ('staff', 'weighing at the Customs House', 2)],
    'rm_the_greatly_bonded_warehouse': [('staff', 'keeping the Bonded Warehouse', 1), ('staff', 'tallying in the Bonded Warehouse', 1)],
    'rm_the_fell_timber_store': [('staff', 'keeping the Long Store', 1), ('staff', 'tallying in the Long Store', 1)],
    'r2_new_store': [('staff', 'keeping the New Store', 1), ('staff', 'tallying in the New Store', 1)],
    'r2_grayrock_shipwrights': [k('shipwright', 'shipwright'), ('hands', None, 4)],
    'rm_the_ropewalk': [k('ropemaker at the ropewalk', 'ropewalk'), ('hands', None, 2)],
    'rm_the_bung_hole': [k('cooper', 'cooperage'), ('help', None, 1)],
    'rm_wickham_s_chandlery': [k('chandler', 'chandlery'), ('help', None, 1)],
    'rm_the_fish_hall': [k('fishmonger', 'fishmonger'), ('hands', None, 2)],
    'rm_the_north_wharf_smokehouse': [k('runs', 'smokehouse'), ('hands', None, 1)],
    'rm_the_wharf_tannery': [k('tanner', 'tannery'), ('hands', None, 2)],
    'rm_the_sailors_rest': [k('keeping the inn', 'inn'), ('help', None, 2)],
    'rm_the_drowned_lantern': [k('keeping the taproom', 'inn'), ('help', None, 1)],
    'rm_the_tarred_rope': [k('keeping the taproom', 'inn'), ('help', None, 1)],
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
    'rm_the_rainwater_brewery': [k('runs', 'brewery'), ('hands', None, 2)],
    'rm_the_slag_anchor': [k('keeping the taproom', 'inn'), ('help', None, 1)],
    # The Sump.
    'rm_sump_sundries': [k('shopkeeper', 'general'), ('help', None, 1)],
    'rm_gullgut_fish': [k('fishmonger', 'fishmonger'), ('help', None, 1)],
    'rm_harbour_provisions': [k('provisioner', 'provisioner'), ('help', None, 1)],
    'rm_mend_make_do': [k('tinker', 'tinker'), ('help', None, 1)],
    'rm_the_pawn_and_ledger': [k('pawnbroker', 'pawnbroker'), ('help', None, 1)],
    'r2_rag_and_bone_yard': [k('rag-and-bone dealer', 'rag_and_bone'), ('hands', None, 1)],
    'r2_the_washhouse': [('staff', 'keeping the Washhouse', 1), ('staff', 'washing at the Washhouse', 5)],
    'rm_the_bilge': [k('keeping the taproom', 'inn'), ('help', None, 1)],
    'rm_the_rainbarrel': [k('keeping the taproom', 'inn'), ('help', None, 1)],
    'r2_the_knucklebones': [k('keeping the taproom', 'inn'), ('help', None, 1)],
    'rm_the_sump_infirmary': [('clergy', "infirmarian of the abbey", 1), ('clergy', 'nursing for the abbey', 1)],
    'rm_the_soup_kitchen': [k('ladling soup', 'inn'), ('help', None, 1)],
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
    'r2_cheesemonger': [k('cheesemaker in the Commons', 'dairy'), ('help', None, 1)],
    'r2_bathhouse': [('staff', 'keeping the Bathhouse', 1), ('staff', 'working at the Bathhouse', 2)],
    'rm_the_eastgate_stables': [k('runs', 'stables'), ('hands', None, 1)],
    'rm_the_commons_school': [('staff', 'teaching letters', 1)],
    'rm_the_house_of_mending': [('clergy', 'healer of the abbey', 2)],
    'rm_the_grey_gull_inn': [k('keeping the inn', 'inn'), ('help', None, 2)],
    'rm_the_black_tun': [k('keeping the taproom', 'inn'), ('help', None, 1)],
    'rm_the_last_lamp': [k('keeping the taproom', 'inn'), ('help', None, 1)],
    'rm_the_greatly_yard': [k("runs the Greatly builders' yard", 'builders_yard'), ('hands', None, 3)],
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
    'rm_the_slate_ledger': [k('keeping the taproom', 'inn'), ('help', None, 1)],
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
    'r2_fell_preserving_house': [k('preserver at the Fell Preserving House', 'preserver'), ('hands', None, 1)],
    'r2_fell_cellars': [('staff', 'keeping the Fell Cellars', 1)],
}
# Posts that work on a floor of their own: {(structure key, post label): z}. The keeper of the rolls is at the rolls, at
# the top of the three-storey Records Office (doc 67, R18).
POST_FLOOR = {('r2_records_office', 'keeping the rolls at the Records Office'): 2}
# Shops with a flat: a keeper and a helper, the keeper's word from the label it has (kept) or the trade.
SHOP_DEFAULT = [('keeper', None, 1, None), ('help', None, 1)]
# Outdoor posts (doc 67, section 4).
OUTDOOR = {
    'market_square': ['pie stall on Market Square', 'cheese stall on Market Square', 'weaver on Market Square',
                      'potter on Market Square', 'tinker on Market Square', 'herbalist on Market Square',
                      'chandler on Market Square', 'shopkeeper on Market Square'],
    'rag_market': ['soup stall in the Rag Market', 'porridge stall in the Rag Market', 'fishmonger in the Rag Market',
                   'tinker in the Rag Market', 'rag-and-bone dealer, Rag Market', 'shopkeeper in the Rag Market'],
}
OUTDOOR_BUSINESS = {'pie stall': 'stall', 'cheese stall': 'stall', 'weaver': 'weaver', 'potter': 'pottery',
                    'tinker': 'tinker', 'herbalist': 'herbalist', 'chandler': 'chandlery', 'shopkeeper': 'general',
                    'soup stall': 'stall', 'porridge stall': 'stall', 'fishmonger': 'fishmonger',
                    'rag-and-bone dealer': 'rag_and_bone'}
HALL_KEEPERS = [('tailor in the Market Hall', 'tailor'), ('saddler in the Market Hall', 'saddlery'),
                ('cooper in the Market Hall', 'cooperage'), ('carpenter in the Market Hall', 'carpenter'),
                ('jeweller in the Market Hall', 'jeweler'), ('provisioner in the Market Hall', 'provisioner')]
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


class Placement:
    """The NPC plan: for every NPC, placed (home, work, evening, business, role, route) or unplaced and why."""

    def __init__(self, project, city, seed=67):
        self.project, self.city = project, city
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
            work = self.spot_near(room, spot[0], spot[1]) if spot else None
            self.plan[pid].update({'work': work or self.work_spot(rec, 'staff'), 'business': None, 'role': 'civilian',
                                   'post': 'staff', 'template': label, 'record': record, 'route': '', 'kept': False,
                                   'labelKept': False,
                                   'at': self.at.get(record)})

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
        # to the Commons' double houses first.
        free = {room: info for room, info in rooms.items() if info['new'] and not info['communal']}
        order_s = sorted([r for r, i in free.items() if i['district'] == 'sump'])
        order_c = sorted([r for r, i in free.items() if i['district'] == 'commons'])
        self.rng.shuffle(order_s)
        units.sort(key=lambda u: (-len(u), u[0]))
        for u in units:
            old = self.people[u[0]]['home']['cell'] if self.people[u[0]]['home'] else ''
            house_like = old.endswith('_house') or old.endswith('_house_2') or old in ('rm_the_heights_watch_post',)
            prefs = (order_c + order_s) if house_like else (order_s + order_c)
            need = max(2, math.ceil(len(u) / 2))
            for room in prefs:
                if load[room] == 0 and rooms[room]['beds'] >= need:
                    break
            else:
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
        for pid in sorted(self.placed, key=lambda q: (self.people[q]['role'] != 'merchant', q)):
            p = self.people[pid]
            if not p['work']:
                continue
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
                # workplace is kept and has no list of posts (the estates' stables, the summerhouses).
                if rec is not None and not label_problems(p['workLabel'], None, clergy=is_clergy(p['workLabel']),
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
        for q in list(guard_posts):
            if not recruits:
                break
            pid = recruits.pop(0)
            q['filled'] = pid
            jobs[pid] = {'kind': q['kind'], 'post': q, 'record': q['record'], 'recruit': True}
        # 3. Fixed posts from those whose posts were lost and the idle, by what they did.
        lost = [pid for pid in adults if idle(pid)]
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
            pid = pool.pop(0)
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
        # Every other open post, from whoever is idle (those with nothing to do first).
        lost.sort(key=lambda x: (bool(self.people[x]['work']), x))
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
        for pid in [x for x in adults if idle(x) and self.people[x]['age'] <= 69]:
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
        for pid in [x for x in adults if idle(x) and self.people[x]['age'] <= 59]:
            fish = 'fishes' in self.people[pid]['workLabel'] or 'nets' in self.people[pid]['workLabel']
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
                    entry.update({'work': self.floor(self.records[school]['rooms'][0]['id']), 'business': None,
                                  'role': 'civilian',
                                  'post': 'at lessons', 'record': school})
                else:
                    entry.update({'work': None, 'business': None, 'role': 'civilian',
                                  'post': 'retired' if age >= 60 else 'a child at home', 'record': None})
                continue
            self.work_entry(pid, j, entry)

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
            entry.update({'work': self.outdoor(x, y), 'business': None, 'role': 'civilian', 'post': 'kept',
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
                   'northgate': ['rm_the_black_tun', 'rm_the_last_lamp', 'rm_the_grey_gull_inn'],
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
                entry['evening'] = self.floor(room)
                entry['eveningPlace'] = ev
            else:
                entry['evening'] = dict(home) if home else None
                entry['eveningPlace'] = 'home'
        self.evening_load = load


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
                label = fit(tpl) if tpl.startswith(('keeping', 'ladling', 'brewer', 'builder', 'pressing', 'distiller',
                                                    'preserver', 'cheesemaker', 'ropemaker', 'rag-and-bone')) and \
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
