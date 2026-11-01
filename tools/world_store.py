#!/usr/bin/env python3
"""Worlds and the character roster in PostgreSQL (see Docs/Design/20-world-database.md).

Converts between the editor's project JSON and database rows. Saving compares
with what is stored and writes only the rows that changed, in one transaction,
and refuses if someone else saved the world since it was opened.

  python3 tools/world_store.py import             import Data/Characters and Data/Worlds into Atlas's world
  python3 tools/world_store.py import --replace   ...replacing worlds already there
  python3 tools/world_store.py list               the world in Atlas's world's database
  (--world KEY: another dev world; python3 tools/world_db.py list)
"""
from __future__ import annotations

import argparse
import copy
import pickle
import sys
import threading

import map_editor
import roster as roster_lib
import world_db

DEFAULT_LIGHTING = {'artificial': 1, 'daylightAccess': 1, 'tone': 'warm'}
DEFAULT_TERRITORY = {'region': 'unassigned', 'claims': [], 'chapter': ''}
# The store room's settings (Docs/Design/67-ridgemere2-city.md, 6; migration 0055): the world's switches and markups
# (live.economy) and each business's own switch (live.business_settings, by its founder), all optional, at these defaults.
STORE_DEFAULTS = map_editor.STORE_DEFAULTS
STORE_COLUMNS = {'internalStore': 'internal_store', 'storeMarkup': 'store_markup', 'storeResale': 'store_resale',
                 'resaleMarkup': 'resale_markup', 'storeRule': 'store_rule'}
# The city's loop (Docs/Design/68-the-country-and-the-city-loop.md, 9; migration 0056): live.economy's columns.
LOOP_DEFAULTS = map_editor.LOOP_DEFAULTS
LOOP_COLUMNS = {'needsPaused': 'needs_paused', 'country': 'country', 'countryReserve': 'country_reserve', 'gateToll': 'gate_toll',
                'carriage': 'carriage', 'wageKeeper': 'wage_keeper', 'wageHand': 'wage_hand', 'wagePublic': 'wage_public',
                'wageCountry': 'wage_country', 'countryOff': 'country_off', 'subsidies': 'subsidies'}
# The seasons and the comforts (doc 68, Progress, "Seasons and the DM's rules"; migration 0057).
SEASON_DEFAULTS = map_editor.SEASON_DEFAULTS
SEASON_COLUMNS = {'comfortsAbove': 'comforts_above', 'comfortsShare': 'comforts_share', 'seasons': 'seasons'}
# The loop's rules and the city's services (doc 68, Progress, "First fixes"; migration 0058): {name: N} and
# {service: {on, price, every}}, only what this world has changed ({}: all as Data/Items/country.json has them).
RULE_DEFAULTS = map_editor.RULE_DEFAULTS
RULE_COLUMNS = {'rules': 'rules', 'services': 'services'}
DEFAULT_ECONOMY = {'treasury': 1000, 'storeHerbs': 100, 'storeMeals': 50, 'dailyHerbs': 10, 'dailyMeals': 12,
                   **STORE_DEFAULTS, 'businesses': {}, **LOOP_DEFAULTS, **SEASON_DEFAULTS, **RULE_DEFAULTS}
NO_STORE = 'this world lacks migration 0055 (python3 tools/world_db.py migrate --db KEY): its store room stays at its defaults'
NO_LOOP = 'this world lacks migration 0056 (python3 tools/world_db.py migrate --db KEY): its city loop stays at its defaults'
NO_SEASONS = ('this world lacks migration 0057 (python3 tools/world_db.py migrate --db KEY): its seasons and comforts stay '
              'as Data/Items/country.json has them')
NO_RULES = ('this world lacks migration 0058 (python3 tools/world_db.py migrate --db KEY): its rules and services stay '
            'as Data/Items/country.json has them')
HERB_PATCH = 'herb_patch'


def default_economy() -> dict:
    """DEFAULT_ECONOMY, a copy of its own (its `businesses` too)."""
    return copy.deepcopy(DEFAULT_ECONOMY)


class StoreError(map_editor.HostError):
    status = 422


class ConflictError(StoreError):
    status = 409


class Unavailable(StoreError):
    status = 503


def num(value):
    """Database doubles back to the JSON the editor wrote: 7.0 -> 7."""
    return int(value) if isinstance(value, float) and value.is_integer() else value


def Jsonb(value):
    from psycopg.types.json import Jsonb as wrap
    return wrap(value)


# --------------------------------------------------------------------------- Tables
# Each table the store writes: its key columns and every column it owns, in order.
# Rows are tuples in that column order; world_id is always the first column.

TABLES = {
    'world.areas': (('world_id', 'id'), ('world_id', 'id', 'kind')),
    'world.terrain_chunks': (('world_id', 'cx', 'cy'), ('world_id', 'cx', 'cy', 'size', 'glyphs', 'heights')),
    'world.cells': (('world_id', 'id'), ('world_id', 'id', 'position', 'name', 'description', 'x', 'y', 'width', 'height', 'z',
                                         'outdoors', 'weather', 'light_artificial', 'light_daylight', 'light_tone',
                                         'region', 'chapter', 'letting')),
    'world.interiors': (('world_id', 'id'), ('world_id', 'id', 'position', 'name', 'description', 'width', 'height', 'glyphs',
                                             'heights', 'z', 'outdoors', 'weather', 'light_artificial', 'light_daylight',
                                             'light_tone', 'region', 'chapter', 'overview_x', 'overview_y', 'letting')),
    'world.links': (('world_id', 'id'), ('world_id', 'id', 'position', 'name', 'kind', 'open',
                                         'a_area', 'a_x', 'a_y', 'b_area', 'b_x', 'b_y')),
    'world.resources': (('world_id', 'id'), ('world_id', 'id', 'kind', 'area', 'x', 'y')),
    # Factions are live data (Dungeon Master); the editor owns their name and color, the Dungeon Master the rest.
    'live.factions': (('world_id', 'id'), ('world_id', 'id', 'position', 'name', 'color')),
    'world.chapters': (('world_id', 'id'), ('world_id', 'id', 'position', 'name')),
    'live.patrol_routes': (('world_id', 'id'), ('world_id', 'id', 'position', 'name')),
    'live.patrol_posts': (('world_id', 'route_id', 'seq'), ('world_id', 'route_id', 'seq', 'area', 'x', 'y')),
    'live.npcs': (('world_id', 'id'), ('world_id', 'id', 'position', 'name', 'role', 'description', 'greeting', 'personality',
                                      'backstory', 'work_label', 'age', 'voice', 'appearance', 'route_id', 'paid', 'purse',
                                      'herbs', 'meals', 'hours_start', 'hours_end', 'home_area', 'home_x', 'home_y',
                                      'work_area', 'work_x', 'work_y', 'evening_area', 'evening_x', 'evening_y', 'joinable',
                                      'protected')),
    'live.profession_slots': (('world_id', 'id'), ('world_id', 'id', 'position', 'name', 'profession', 'work_label', 'route_id',
                                                   'paid', 'purse', 'herbs', 'meals', 'hours_start', 'hours_end',
                                                   'home_area', 'home_x', 'home_y', 'work_area', 'work_x', 'work_y',
                                                   'evening_area', 'evening_x', 'evening_y')),
    'live.economy': (('world_id',), ('world_id', 'treasury', 'store_herbs', 'store_meals', 'daily_herbs', 'daily_meals',
                                     *STORE_COLUMNS.values(), *LOOP_COLUMNS.values(), *SEASON_COLUMNS.values(),
                                     *RULE_COLUMNS.values())),
    # A business kept out of the store room (doc 67, 6): a row only while it is off.
    'live.business_settings': (('world_id', 'npc_id'), ('world_id', 'npc_id', 'internal_store')),
}
# A world with migration 0057 but not 0058: the seasons' columns, not the rules'.
SEASON_TABLES = {**TABLES, 'live.economy': (('world_id',), TABLES['live.economy'][1][
    :6 + len(STORE_COLUMNS) + len(LOOP_COLUMNS) + len(SEASON_COLUMNS)])}
# A world with migration 0056 but not 0057: the loop's columns, not the seasons'.
LOOP_TABLES = {**TABLES, 'live.economy': (('world_id',), TABLES['live.economy'][1][:6 + len(STORE_COLUMNS) + len(LOOP_COLUMNS)])}
# A world with migration 0055 but not 0056: the store room's columns, not the loop's.
STORE_TABLES = {**TABLES, 'live.economy': (('world_id',), TABLES['live.economy'][1][:6 + len(STORE_COLUMNS)])}
# A world without migration 0055 (DEV and PROD until the user migrates them): no store columns, no business settings.
LEGACY_TABLES = {**{t: v for t, v in TABLES.items() if t != 'live.business_settings'},
                 'live.economy': (('world_id',), TABLES['live.economy'][1][:6])}
# Children are deleted before parents and written after them; deferred foreign keys settle the rest at commit.
WRITE_ORDER = ['world.areas', 'world.terrain_chunks', 'world.cells', 'world.interiors', 'world.links', 'world.resources',
               'live.factions', 'world.chapters', 'live.patrol_routes', 'live.patrol_posts', 'live.npcs',
               'live.profession_slots', 'live.economy', 'live.business_settings']


def store_supported(conn) -> bool:
    """Whether this world has migration 0055 (the store room's columns and table), as world_build checks fingerprints."""
    return bool(conn.execute("""
        SELECT (SELECT count(*) FROM information_schema.columns WHERE table_schema = 'live' AND table_name = 'economy'
                AND column_name IN ('internal_store', 'store_markup', 'store_resale', 'resale_markup', 'store_rule')) = 5
           AND EXISTS (SELECT 1 FROM information_schema.tables WHERE table_schema = 'live' AND table_name = 'business_settings')
    """).fetchone()[0])


def loop_supported(conn) -> bool:
    """Whether this world has migration 0056 (the city loop's columns)."""
    return bool(conn.execute("""
        SELECT count(*) = %s FROM information_schema.columns WHERE table_schema = 'live' AND table_name = 'economy'
               AND column_name = ANY(%s)""", (len(LOOP_COLUMNS), list(LOOP_COLUMNS.values()))).fetchone()[0])


def seasons_supported(conn) -> bool:
    """Whether this world has migration 0057 (the seasons' and comforts' columns)."""
    return bool(conn.execute("""
        SELECT count(*) = %s FROM information_schema.columns WHERE table_schema = 'live' AND table_name = 'economy'
               AND column_name = ANY(%s)""", (len(SEASON_COLUMNS), list(SEASON_COLUMNS.values()))).fetchone()[0])


def rules_supported(conn) -> bool:
    """Whether this world has migration 0058 (the rules' and services' columns)."""
    return bool(conn.execute("""
        SELECT count(*) = %s FROM information_schema.columns WHERE table_schema = 'live' AND table_name = 'economy'
               AND column_name = ANY(%s)""", (len(RULE_COLUMNS), list(RULE_COLUMNS.values()))).fetchone()[0])


def tables_for(conn) -> dict:
    """The tables (and columns) this world's database has: TABLES, SEASON_TABLES without migration 0058, LOOP_TABLES
    without 0057, STORE_TABLES without 0056, or LEGACY_TABLES without 0055."""
    if not store_supported(conn):
        return LEGACY_TABLES
    if not loop_supported(conn):
        return STORE_TABLES
    if not seasons_supported(conn):
        return LOOP_TABLES
    return TABLES if rules_supported(conn) else SEASON_TABLES


def loop_differs(economy) -> bool:
    """Whether an economy entity sets anything of the city's loop away from its default (which needs migration 0056)."""
    e = economy or {}
    return any(comparable(e.get(k, v)) != comparable(v) for k, v in LOOP_DEFAULTS.items())


def seasons_differ(economy) -> bool:
    """Whether an economy entity sets the seasons or the comforts (which need migration 0057)."""
    e = economy or {}
    return any(comparable(e.get(k, v)) != comparable(v) for k, v in SEASON_DEFAULTS.items())


def rules_differ(economy) -> bool:
    """Whether an economy entity sets the rules or the services (which need migration 0058)."""
    e = economy or {}
    return any(comparable(e.get(k, v)) != comparable(v) for k, v in RULE_DEFAULTS.items())


def loop_refused(tables, economy) -> str | None:
    """Why these settings can't be stored in a world with these tables, or None."""
    if tables is LEGACY_TABLES and store_differs(economy):
        return NO_STORE
    if tables in (LEGACY_TABLES, STORE_TABLES) and loop_differs(economy):
        return NO_LOOP
    if tables is not TABLES and tables is not SEASON_TABLES and seasons_differ(economy):
        return NO_SEASONS
    if tables is not TABLES and rules_differ(economy):
        return NO_RULES
    return None


def store_differs(economy) -> bool:
    """Whether an economy entity sets anything of the store room away from its default (which needs migration 0055)."""
    e = economy or {}
    return (any(e.get(k, v) != v for k, v in STORE_DEFAULTS.items())
            or any(isinstance(s, dict) and s.get('internalStore') is False for s in (e.get('businesses') or {}).values()))
JSON_COLUMNS = {'heights', 'appearance', 'letting', 'subsidies', 'seasons', 'rules', 'services'}


def comparable(value):
    """Rows read back from PostgreSQL compare equal to rows about to be written."""
    if isinstance(value, float):
        return num(value)
    if isinstance(value, list):
        return tuple(value)
    if isinstance(value, dict):
        return tuple(sorted((k, comparable(v)) for k, v in value.items()))
    return value


def lighting(c):
    light = {**DEFAULT_LIGHTING, **c.get('lighting', {})}
    return light['artificial'], light['daylightAccess'], light['tone']


def territory(c):
    t = {**DEFAULT_TERRITORY, **c.get('territory', {})}
    return t['region'], t['chapter']


def letting(c):
    """A place to let (Docs/Design/32, 5.2) as stored: its terms, or None."""
    return dict(c['letting']) if c.get('letting') else None


def claims_of(c):
    return sorted({**DEFAULT_TERRITORY, **c.get('territory', {})}['claims'])


# Territory claims live in live.faction_claims, one row per faction and place: its painted tiles, or the whole place.
# The editor sees each place's claims as a list of faction IDs; keeping a claim keeps its tiles.

def claims_by_area(conn, world_id) -> dict[str, list[str]]:
    out = {}
    for area, faction in conn.execute('SELECT area, faction_id FROM live.faction_claims WHERE world_id = %s ORDER BY area, faction_id',
                                      (world_id,)).fetchall():
        out.setdefault(area, []).append(faction)
    return out


def sync_claims(conn, world_id, area, wanted) -> bool:
    """Makes the factions claiming `area` exactly `wanted`: new claims take the whole place. Returns whether anything changed."""
    have = {r[0] for r in conn.execute('SELECT faction_id FROM live.faction_claims WHERE world_id = %s AND area = %s',
                                       (world_id, area)).fetchall()}
    wanted = set(wanted)
    for faction in have - wanted:
        conn.execute('DELETE FROM live.faction_claims WHERE world_id = %s AND area = %s AND faction_id = %s', (world_id, area, faction))
    for faction in sorted(wanted - have):
        conn.execute('INSERT INTO live.faction_claims (world_id, faction_id, area) VALUES (%s, %s, %s)', (world_id, faction, area))
    return have != wanted


def place(anchor):
    return anchor['cell'], anchor['x'], anchor['y']


def npc_place(anchor):
    """An NPC's home, work or evening place as its row's (area, x, y): an unset one (None) is (NULL, 0, 0) (doc 66, 2)."""
    return place(anchor) if anchor else (None, 0, 0)


def cell_bounds(cells) -> tuple[int, int, int, int]:
    """The rectangle (x, y, width, height) that holds every world cell; zeros without cells. Kept on the world row
    for information: the world has no edge, only cells."""
    if not cells:
        return 0, 0, 0, 0
    x0, y0 = min(c['x'] for c in cells), min(c['y'] for c in cells)
    return x0, y0, max(c['x'] + c['width'] for c in cells) - x0, max(c['y'] + c['height'] for c in cells) - y0


# Entities <-> rows. Entities are the JSON the editor edits; cells here are in world coordinates.

def cell_row(w, c, position):
    return (w, c['id'], position, c['name'], c.get('description', ''), c['x'], c['y'], c['width'], c['height'],
            c.get('z', 0), c['outdoors'], c['weather'], *lighting(c), *territory(c), letting(c))


def room_row(w, r, position):
    return (w, r['id'], position, r['name'], r.get('description', ''), r['width'], r['height'], ''.join(r['terrain']),
            dict(r.get('heights', {})), r.get('z', 0), r['outdoors'], r['weather'], *lighting(r), *territory(r),
            r.get('worldX', 0), r.get('worldY', 0), letting(r))


def link_row(w, link, position):
    return (w, link['id'], position, link['name'], link['kind'], link['open'], *place(link['a']), *place(link['b']))


def post_rows(w, route):
    return [(w, route['id'], seq, *place(post)) for seq, post in enumerate(route['posts'])]


def npc_row(w, n, position):
    return (w, n['id'], position, n['name'], n['role'], n['description'], n['greeting'], n.get('personality', ''),
            n.get('backstory', ''), n['workLabel'], n['age'], n['voice'], dict(n['appearance']), n['route'] or None,
            n['paid'], n['purse'], n['herbs'], n['meals'], n['hours']['start'], n['hours']['end'],
            *npc_place(n['home']), *npc_place(n['work']), *npc_place(n['evening']), bool(n.get('joinable', False)), bool(n.get('protected', False)))


def slot_row(w, s, position):
    return (w, s['id'], position, s['name'], s['profession'], s['workLabel'], s['route'] or None, s['paid'], s['purse'],
            s['herbs'], s['meals'], s['hours']['start'], s['hours']['end'],
            *npc_place(s['home']), *npc_place(s['work']), *npc_place(s['evening']))


def economy_row(w, economy, store=True, loop=None, seasons=None, rules=None):
    """A live.economy row; with `store` (a world with migration 0055), the store room's switches and markups too; with
    `loop` (0056; as `store` unless said), the city loop's settings; with `seasons` (0057; as `loop` unless said), the
    seasons' and the comforts'; with `rules` (0058; as `seasons` unless said), the rules' and the services'."""
    e = {**DEFAULT_ECONOMY, **(economy or {})}
    row = (w, e['treasury'], e['storeHerbs'], e['storeMeals'], e['dailyHerbs'], e['dailyMeals'])
    if not store:
        return row
    row += tuple(e[k] for k in STORE_COLUMNS)
    loop = True if loop is None else loop
    if loop:
        row += tuple(sorted(e[k]) if k == 'countryOff' else dict(e[k]) if k == 'subsidies' else e[k] for k in LOOP_COLUMNS)
    seasons = loop if seasons is None else loop and seasons
    if seasons:
        row += tuple(copy.deepcopy(dict(e[k] or {})) if k == 'seasons' else e[k] for k in SEASON_COLUMNS)
    if seasons and (rules is None or rules):
        row += tuple(copy.deepcopy(dict(e[k] or {})) for k in RULE_COLUMNS)
    return row


def economy_row_for(w, economy, tables):
    """economy_row for a world with these tables."""
    return economy_row(w, economy, store=tables is not LEGACY_TABLES, loop=tables in (TABLES, SEASON_TABLES, LOOP_TABLES),
                       seasons=tables in (TABLES, SEASON_TABLES), rules=tables is TABLES)


def business_rows(w, economy):
    """live.business_settings rows: one per business switched off (a row back at its default is not kept)."""
    businesses = (economy or {}).get('businesses') or {}
    return [(w, npc, False) for npc, s in sorted(businesses.items()) if isinstance(s, dict) and s.get('internalStore') is False]


def _details(r):
    return {'z': r['z'], 'outdoors': r['outdoors'], 'weather': r['weather'],
            'lighting': {'artificial': num(r['light_artificial']), 'daylightAccess': num(r['light_daylight']), 'tone': r['light_tone']},
            'territory': {'region': r['region'], 'claims': list(r.get('claims', [])), 'chapter': r['chapter']},
            **({'letting': dict(r['letting'])} if r.get('letting') else {})}


def _anchor(r, k):
    return {'cell': r[f'{k}_area'], 'x': r[f'{k}_x'], 'y': r[f'{k}_y']}


def _npc_anchor(r, k):
    """An NPC's place, or None where it is unset (doc 66, 2: an NPC may have no home, work or evening place)."""
    return _anchor(r, k) if r[f'{k}_area'] is not None else None


def cell_entity(r):
    """A world.cells row as the editor's cell, in world tiles, without its ground (load_world adds that)."""
    return {'id': r['id'], 'name': r['name'], 'description': r['description'], 'x': r['x'], 'y': r['y'],
            'width': r['width'], 'height': r['height'], **_details(r)}


def room_meta(r):
    """A world.interiors row without its tiles."""
    return {'id': r['id'], 'name': r['name'], 'description': r['description'], 'width': r['width'], 'height': r['height'],
            'worldX': r['overview_x'], 'worldY': r['overview_y'], **_details(r)}


def room_entity(r):
    return {**room_meta(r), 'terrain': [r['glyphs'][y * r['width']:(y + 1) * r['width']] for y in range(r['height'])],
            'heights': {k: num(v) for k, v in r['heights'].items()}}


def link_entity(r):
    return {'id': r['id'], 'name': r['name'], 'kind': r['kind'], 'open': r['open'], 'a': _anchor(r, 'a'), 'b': _anchor(r, 'b')}


def _resident(r):
    return {'workLabel': r['work_label'], 'route': r['route_id'] or '', 'paid': r['paid'], 'purse': r['purse'],
            'herbs': r['herbs'], 'meals': r['meals'], 'hours': {'start': num(r['hours_start']), 'end': num(r['hours_end'])},
            'home': _npc_anchor(r, 'home'), 'work': _npc_anchor(r, 'work'), 'evening': _npc_anchor(r, 'evening')}


def person_entity(r):
    return {'id': r['id'], 'name': r['name'], 'role': r['role'], 'description': r['description'], 'greeting': r['greeting'],
            'age': r['age'], 'appearance': r['appearance'], 'voice': r['voice'], **_resident(r),
            'personality': r['personality'], 'backstory': r['backstory'],
            **({'joinable': True} if r.get('joinable') else {}), **({'protected': True} if r.get('protected') else {})}


def slot_entity(r):
    return {'id': r['id'], 'name': r['name'], 'profession': r['profession'], **_resident(r)}


def economy_entity(r, businesses=None):
    """A live.economy row as the editor's economy: a world without migration 0055 reads the store room's defaults.
    `businesses`: live.business_settings rows ({npc_id, internal_store}) for the businesses switched off."""
    return {'treasury': r['treasury'], 'storeHerbs': r['store_herbs'], 'storeMeals': r['store_meals'],
            'dailyHerbs': r['daily_herbs'], 'dailyMeals': r['daily_meals'],
            **{k: r.get(c, STORE_DEFAULTS[k]) for k, c in STORE_COLUMNS.items()},
            'businesses': {b['npc_id']: {'internalStore': False} for b in businesses or [] if b['internal_store'] is False},
            **{k: (sorted(r[c]) if k == 'countryOff' else dict(r[c]) if k == 'subsidies' else r[c]) if c in r
               else copy.deepcopy(LOOP_DEFAULTS[k]) for k, c in LOOP_COLUMNS.items()},
            **{k: ((dict(r[c] or {}) if k == 'seasons' else r[c]) if c in r else copy.deepcopy(SEASON_DEFAULTS[k]))
               for k, c in SEASON_COLUMNS.items()},
            **{k: copy.deepcopy(dict(r[c] or {})) if c in r else copy.deepcopy(RULE_DEFAULTS[k]) for k, c in RULE_COLUMNS.items()}}


def economy_value(conn, world_id):
    """The economy entity as stored (Atlas's keyed edit `economy`), or None while the world has no live.economy row."""
    found = dict_rows(conn, 'SELECT * FROM live.economy WHERE world_id = %s', (world_id,))
    if not found:
        return None
    businesses = (dict_rows(conn, 'SELECT npc_id, internal_store FROM live.business_settings WHERE world_id = %s ORDER BY npc_id',
                            (world_id,)) if store_supported(conn) else [])
    return economy_entity(found[0], businesses)


def dict_rows(conn, sql, params):
    cur = conn.execute(sql, params)
    names = [d.name for d in cur.description]
    return [dict(zip(names, r)) for r in cur.fetchall()]


def project_rows(p: dict, chunk_size: int, tables=None) -> dict[str, list[tuple]]:
    """Every row a validated (atlas v3) project stores, per table. Cells and terrain are stored in world coordinates.
    `tables`: the database's (tables_for), TABLES unless said; without migration 0055 the store room's are left out."""
    tables = TABLES if tables is None else tables
    w = p['id']
    rows = {table: [] for table in tables}
    for c in p['cells']:
        rows['world.areas'].append((w, c['id'], 'cell'))
    for r in p['rooms']:
        rows['world.areas'].append((w, r['id'], 'interior'))

    # Terrain: each cell's ground, in world tiles, in chunk_size squares. Chunks that are plain ground are not stored.
    chunks = {}                                    # (cx, cy) -> (glyph list, heights)
    def chunk(wx, wy):
        key = (wx // chunk_size, wy // chunk_size)
        if key not in chunks:
            chunks[key] = (['.'] * (chunk_size * chunk_size), {})
        return chunks[key], wx - key[0] * chunk_size, wy - key[1] * chunk_size
    for c in p['cells']:
        for y, row in enumerate(c['terrain']):
            for x, g in enumerate(row):
                if g != '.':
                    (glyphs, _), lx, ly = chunk(c['x'] + x, c['y'] + y)
                    glyphs[ly * chunk_size + lx] = g
        for key, h in c.get('heights', {}).items():
            x, y = map(int, key.split(','))
            (_, local), lx, ly = chunk(c['x'] + x, c['y'] + y)
            local[f'{lx},{ly}'] = h
    for (cx, cy), (glyphs, local) in sorted(chunks.items()):
        rows['world.terrain_chunks'].append((w, cx, cy, chunk_size, ''.join(glyphs), local))

    for i, c in enumerate(p['cells']):
        rows['world.cells'].append(cell_row(w, c, i))
    for i, r in enumerate(p['rooms']):
        rows['world.interiors'].append(room_row(w, r, i))
    for i, link in enumerate(p['links']):
        rows['world.links'].append(link_row(w, link, i))
    if p.get('herbPatch'):
        rows['world.resources'].append((w, HERB_PATCH, 'herb', *place(p['herbPatch'])))
    for i, f in enumerate(p.get('factions', [])):
        rows['live.factions'].append((w, f['id'], i, f['name'], f['color']))
    for i, c in enumerate(p.get('chapters', [])):
        rows['world.chapters'].append((w, c['id'], i, c['name']))
    for i, r in enumerate(p.get('routes', [])):
        rows['live.patrol_routes'].append((w, r['id'], i, r['name']))
        rows['live.patrol_posts'] += post_rows(w, r)
    for i, n in enumerate(p.get('people', [])):
        rows['live.npcs'].append(npc_row(w, n, i))
    for i, slot in enumerate(p.get('slots', [])):
        rows['live.profession_slots'].append(slot_row(w, slot, i))
    # (A world without a live.economy row gets one at DEFAULT_ECONOMY's values, never the table's column defaults.)
    rows['live.economy'].append(economy_row_for(w, p.get('economy', {}), tables))
    if 'live.business_settings' in tables:
        rows['live.business_settings'] = business_rows(w, p.get('economy', {}))
    return rows


def stored_rows(conn, world_id: str, tables=None) -> dict[str, list[tuple]]:
    out = {}
    for table, (_, columns) in (tables_for(conn) if tables is None else tables).items():
        out[table] = [tuple(r) for r in conn.execute(
            f'SELECT {", ".join(columns)} FROM {table} WHERE world_id = %s', (world_id,)).fetchall()]
    return out


def write_rows(conn, desired: dict[str, list[tuple]], current: dict[str, list[tuple]], tables=None) -> int:
    """Deletes, updates and inserts only what differs. Returns the number of rows changed. `tables`: as project_rows
    made `desired` (TABLES, or LEGACY_TABLES without the store room's)."""
    if not tables:
        economy = (desired.get('live.economy') or [()])[0]
        tables = (LEGACY_TABLES if 'live.business_settings' not in desired
                  else STORE_TABLES if economy and len(economy) == len(STORE_TABLES['live.economy'][1])
                  else LOOP_TABLES if economy and len(economy) == len(LOOP_TABLES['live.economy'][1])
                  else SEASON_TABLES if economy and len(economy) == len(SEASON_TABLES['live.economy'][1]) else TABLES)
    order = [t for t in WRITE_ORDER if t in tables]
    changed = 0
    plans = {}
    for table in order:
        keys, columns = tables[table]
        n = len(keys)
        have = {r[:n]: r for r in current[table]}
        want = {r[:n]: r for r in desired[table]}
        stale = [k for k in have if k not in want]
        fresh = [r for k, r in want.items() if k not in have or tuple(map(comparable, have[k])) != tuple(map(comparable, r))]
        plans[table] = (stale, fresh)
    for table in reversed(order):
        keys, _ = tables[table]
        for key in plans[table][0]:
            conn.execute(f'DELETE FROM {table} WHERE ' + ' AND '.join(f'{k} = %s' for k in keys), key)
            changed += 1
    for table in order:
        keys, columns = tables[table]
        rest = [c for c in columns if c not in keys]
        update = ', '.join(f'{c} = excluded.{c}' for c in rest) if rest else None
        sql = (f'INSERT INTO {table} ({", ".join(columns)}) VALUES ({", ".join(["%s"] * len(columns))}) '
               f'ON CONFLICT ({", ".join(keys)}) ' + (f'DO UPDATE SET {update}' if update else 'DO NOTHING'))
        for row in plans[table][1]:
            conn.execute(sql, [Jsonb(v) if c in JSON_COLUMNS else v for c, v in zip(columns, row)])
            changed += 1
    return changed


# --------------------------------------------------------------------------- Worlds

def list_worlds(conn) -> list[dict]:
    rows = conn.execute('''
        SELECT w.id, w.name, w.revision, w.updated_at,
               (SELECT count(*) FROM world.areas a WHERE a.world_id = w.id),
               (SELECT count(*) FROM live.npcs n WHERE n.world_id = w.id)
             + (SELECT count(*) FROM live.profession_slots s WHERE s.world_id = w.id)
        FROM world.worlds w ORDER BY w.name''').fetchall()
    return [{'id': r[0], 'name': r[1], 'revision': r[2], 'updatedAt': r[3].isoformat(), 'cells': r[4], 'people': r[5]}
            for r in rows]


def keep_made_by_the_server(conn, world_id: str, desired: dict, current: dict) -> list[str]:
    """NPCs the game server made (births and newcomers, origin 'runtime'; a spawn rule's, spawn_id set) that a
    whole-world save doesn't list are kept as they are, never deleted: the save's project was read before the server
    made them, and the world's revision (which every keyed edit raises) doesn't move when it does. Matters once a dev
    world is served as the live server (doc 66, Progress): the generator's or apply_terrain's save would otherwise
    silently remove residents born meanwhile. Removing one is a keyed edit (Atlas's delete). The ids kept."""
    made = {r[0] for r in conn.execute("SELECT id FROM live.npcs WHERE world_id = %s AND (origin = 'runtime' OR spawn_id IS NOT NULL)",
                                       (world_id,)).fetchall()}
    listed = {r[1] for r in desired['live.npcs']}
    kept = [r for r in current['live.npcs'] if r[1] in made and r[1] not in listed]
    desired['live.npcs'].extend(kept)
    return [r[1] for r in kept]


# --------------------------------------------------------------------------- The live world's truth
# Doc 66, Progress, "The live world's truth: NPCs and markets in conflicts". The live game owns its NPCs' live state and
# the market; Atlas owns the map, the buildings and the NPCs as written. Of the rows both write, these columns are the
# game's (an arrival's home and post, what it carries, its age; on a profession slot, also the roster character the
# build gave it, on a roster character its assignment); a row the game made (origin 'runtime': a birth) is the game's
# whole. Everything else in them is authored. (The rest of the game's truth, the households, purses, tills, stock,
# shelves, prices, wages, the toll and alms, the treasury and the loop, is in its save, game.checkpoints, and
# live.npc_state, which no save of Atlas's and no pull or import ever writes; live.economy is each world's own.)
PLACES = ('home', 'work', 'evening')
PLACE_COLUMNS = tuple(f'{p}_{c}' for p in PLACES for c in ('area', 'x', 'y'))
GAME_COLUMNS = {
    'live.npcs': (*PLACE_COLUMNS, 'purse', 'herbs', 'meals', 'age', 'origin'),
    'live.profession_slots': (*PLACE_COLUMNS, 'purse', 'herbs', 'meals', 'character_id', 'origin'),
    'live.characters': ('status', 'assignment', 'origin'),
}
# The same, as the editor's entities carry them (live_edit's person: and slot: keys).
GAME_FIELDS = {'person': (*PLACES, 'purse', 'herbs', 'meals', 'age'), 'slot': (*PLACES, 'purse', 'herbs', 'meals')}
GAME_TABLE_OF = {'person': 'live.npcs', 'slot': 'live.profession_slots'}


def game_owned(table, column, row=None) -> bool:
    """Whether a column of a live table is the live game's (or the whole row is: one the game made)."""
    return column in GAME_COLUMNS.get(table, ()) or (isinstance(row, dict) and row.get('origin') == 'runtime')


def game_log_supported(conn) -> bool:
    """Whether this world logs the game server's changes (migration 0059)."""
    return bool(conn.execute("SELECT to_regclass('live.game_changes') IS NOT NULL").fetchone()[0])


def game_seen(conn) -> int:
    """The newest game change this world has logged (0 for none, or a world without migration 0059): what a reader
    has seen, for later saves to compare against."""
    if not game_log_supported(conn):
        return 0
    return conn.execute('SELECT coalesce(max(seq), 0) FROM live.game_changes').fetchone()[0]


def game_changes(conn, world_id, table=None, since=None, revision=None) -> dict[str, dict]:
    """{row id: {'before': the row before the game's first change since, 'after': as the game left it (None:
    removed), 'op': the first change's, 'seq': the last's}} for the rows the game changed after game change `since`
    (or, without it, while the world's revision was `revision` or later), in one table (or every one, keyed
    'table/id'). Only rows keyed by one id (NPCs, slots)."""
    if not game_log_supported(conn):
        return {}
    where, params = ['world_id = %s', 'jsonb_array_length(row_key) = 1'], [world_id]
    if table:
        where.append('table_name = %s')
        params.append(table)
    if since is not None:
        where.append('seq > %s')
        params.append(since)
    elif revision is not None:
        where.append('revision >= %s')
        params.append(revision)
    out = {}
    for seq, name, key, op, before, after in conn.execute(
            f'SELECT seq, table_name, row_key, op, before, after FROM live.game_changes WHERE {" AND ".join(where)} ORDER BY seq',
            params).fetchall():
        rid = key[0] if table else f'{name}/{key[0]}'
        if rid not in out:
            out[rid] = {'before': before, 'op': op}
        out[rid].update(after=after, seq=seq)
    return out


def _same(a, b) -> bool:
    return comparable(num(a) if isinstance(a, float) else a) == comparable(num(b) if isinstance(b, float) else b)


def keep_the_games_truth(conn, world_id, desired: dict, current: dict, revision=None, seen=None, choices=None, tables=None):
    """A whole-world save over a world a game server changes (doc 66, Progress, "The live world's truth"): for each NPC or
    profession slot the game changed since the save's project was read (its log, after game change `seen`; without it,
    at the project's revision or later), each game-owned field (a place: area, x and y together; purse, stock, age)
    the save would change back is the game's again: the save's value where the save hadn't changed it (it still holds the
    value the game replaced), and by default where both changed it, a conflict. `choices` {conflict id: 'atlas' |
    'game' | {field: 'atlas' | 'game'}} keep the save's instead. Authored fields are the save's. Returns {'kept': [row
    ids kept as the game left them], 'conflicts': [{id, table, name, fields: [{field, base, atlas, game, chosen}]}]}."""
    tables = tables or TABLES
    choices = choices or {}
    kept, conflicts = [], []
    for table in GAME_TABLE_OF.values():
        if table not in tables:
            continue
        changed = game_changes(conn, world_id, table, since=seen, revision=None if seen is not None else revision)
        if not changed:
            continue
        _, columns = tables[table]
        at = {c: i for i, c in enumerate(columns)}
        have = {r[1]: r for r in current[table]}
        want = {r[1]: i for i, r in enumerate(desired[table])}
        for rid, change in changed.items():
            if rid not in want:
                continue                      # (Not in the save: kept by keep_made_by_the_server, or Atlas removed it.)
            row = list(desired[table][want[rid]])
            now = have.get(rid)
            cid = f'{table}/{rid}'
            if now is None:
                # The game removed it (a birth it couldn't make): the save doesn't bring it back unless chosen.
                if change['op'] == 'insert' and choices.get(cid) != 'atlas':
                    desired[table].pop(want[rid])
                    want = {r[1]: i for i, r in enumerate(desired[table])}
                    kept.append(cid)
                continue
            base = change['before'] or {}
            groups = [(p, [f'{p}_area', f'{p}_x', f'{p}_y']) for p in PLACES] + \
                     [(c, [c]) for c in GAME_COLUMNS[table] if c not in PLACE_COLUMNS and c in at]
            fields, touched = [], False
            for field, cols in groups:
                if not all(c in at for c in cols):
                    continue
                mine, game = [row[at[c]] for c in cols], [now[at[c]] for c in cols]
                before = [base.get(c) for c in cols] if change['op'] != 'insert' else None
                if all(_same(m, g) for m, g in zip(mine, game)):
                    continue
                if before is not None and all(_same(m, b) for m, b in zip(mine, before)):
                    for c, g in zip(cols, game):       # (The save still holds what the game replaced: the game's.)
                        row[at[c]] = g
                    touched = True
                    continue
                if before is not None and all(_same(g, b) for g, b in zip(game, before)):
                    continue                           # (The game put it back as it was: the save's change stands.)
                choice = choices.get(cid)
                pick = choice.get(field) if isinstance(choice, dict) else choice
                shown = lambda v: None if v is None else v[0] if len(cols) == 1 else \
                    {'cell': v[0], 'x': v[1], 'y': v[2]} if v[0] is not None else None
                fields.append({'field': field, 'base': shown(before), 'atlas': shown(mine), 'game': shown(game),
                               'chosen': 'atlas' if pick == 'atlas' else 'game'})
                if pick != 'atlas':
                    for c, g in zip(cols, game):
                        row[at[c]] = g
                    touched = True
            if fields:
                conflicts.append({'id': cid, 'table': table, 'name': row[at['name']] if 'name' in at else rid, 'fields': fields})
            if touched:
                desired[table][want[rid]] = tuple(row)
                kept.append(cid)
    return {'kept': kept, 'conflicts': conflicts}


def save_world(conn, project: dict, revision: int | None, create: bool = False, *, seen: int | None = None,
               choices: dict | None = None, outcome: dict | None = None) -> int:
    """Validates and stores a project. `revision` is the one it was opened at (None to create).

    Returns the new revision. Raises ConflictError if the world changed since it was opened. What a game server changed
    meanwhile (doc 66, Progress, "The live world's truth": `seen`, game_seen() when the project was read) is kept as
    the game left it (keep_the_games_truth, with `choices`); `outcome`, a dict, is given what was kept and the conflicts.
    """
    p, _ = map_editor.check_project(project, for_game=False, playable=False)
    p['id'] = map_editor.world_id(p)
    with conn.transaction():
        row = conn.execute('SELECT revision, chunk_size FROM world.worlds WHERE id = %s FOR UPDATE', (p['id'],)).fetchone()
        if row is None:
            if not create:
                raise ConflictError(f'World {p["id"]} is not in the database any more. Save it as a new world instead.')
            conn.execute('INSERT INTO world.worlds (id, name) VALUES (%s, %s)', (p['id'], p['name']))
            current_revision, chunk_size = 0, conn.execute(
                'SELECT chunk_size FROM world.worlds WHERE id = %s', (p['id'],)).fetchone()[0]
        else:
            current_revision, chunk_size = row
            if create:
                raise ConflictError(f'A world with ID {p["id"]} already exists. Rename this world or open the existing one.')
            if revision != current_revision:
                raise ConflictError(f'{p["name"]} was saved elsewhere since you opened it (revision {current_revision}, '
                                    f'yours {revision}). Reopen it to see those changes.')
        missing = sorted({s['profession'] for s in p.get('slots', [])}
                         - {r[0] for r in conn.execute('SELECT id FROM live.professions').fetchall()})
        if missing:
            raise StoreError(f'Profession slots use professions the roster does not have: {", ".join(missing)}.')
        tables = tables_for(conn)
        if refused := loop_refused(tables, p.get('economy')):
            raise StoreError(f'The economy\'s settings can\'t be saved: {refused}.')
        desired, current = project_rows(p, chunk_size, tables), stored_rows(conn, p['id'], tables)
        keep_made_by_the_server(conn, p['id'], desired, current)
        truth = keep_the_games_truth(conn, p['id'], desired, current, revision, seen, choices, tables) if row else \
            {'kept': [], 'conflicts': []}
        if outcome is not None:
            outcome.update(truth)
        write_rows(conn, desired, current, tables)
        conn.execute('DELETE FROM live.faction_claims WHERE world_id = %s AND NOT (area = ANY(%s))',
                     (p['id'], [a['id'] for a in p['cells'] + p['rooms']]))
        for area in p['cells'] + p['rooms']:
            sync_claims(conn, p['id'], area['id'], claims_of(area))
        spawn = place(p['spawn']) if p.get('spawn') else (None, None, None)
        new_revision = current_revision + 1
        conn.execute('''UPDATE world.worlds SET name = %s, spawn_area = %s, spawn_x = %s, spawn_y = %s,
                        min_x = %s, min_y = %s, width = %s, height = %s, revision = %s, updated_at = now() WHERE id = %s''',
                     (p['name'], *spawn, *cell_bounds(p['cells']), new_revision, p['id']))
    return new_revision


_ground_lock = threading.Lock()
_ground: dict = {}          # (database, world) -> (version, pickled {cell id: (terrain rows, heights)})


def _ground_key(conn, world_id):
    info = getattr(conn, 'info', None)
    return (getattr(info, 'host', ''), getattr(info, 'port', ''), getattr(info, 'dbname', ''), world_id)


def _cached_ground(conn, world_id, version):
    with _ground_lock:
        hit = _ground.get(_ground_key(conn, world_id))
    return pickle.loads(hit[1]) if hit and hit[0] == version else None


def _keep_ground(conn, world_id, version, ground):
    data = pickle.dumps(ground, protocol=5)
    with _ground_lock:
        _ground[_ground_key(conn, world_id)] = (version, data)


def _cut_ground(q, cells, size):
    """Every cell's terrain rows and heights, cut from the terrain chunks it covers."""
    return _cut(q('SELECT cx, cy, glyphs, heights FROM world.terrain_chunks WHERE world_id = %s'), cells, size)


def _cut(chunk_rows, cells, size):
    """Each cell's terrain rows and heights, cut from these terrain chunks (cx, cy, glyphs, heights)."""
    ground = {}
    chunks = {(cx, cy): (glyphs, local) for cx, cy, glyphs, local in chunk_rows}
    for c in cells:
        rows = []
        for y in range(c['y'], c['y'] + c['height']):
            row, cy = [], y // size
            for cx in range(c['x'] // size, (c['x'] + c['width'] - 1) // size + 1):
                x0, x1 = max(c['x'], cx * size), min(c['x'] + c['width'], (cx + 1) * size)
                found = chunks.get((cx, cy))
                start = (y - cy * size) * size + x0 - cx * size
                row.append(found[0][start:start + x1 - x0] if found else '.' * (x1 - x0))
            rows.append(''.join(row))
        found = []                                 # (y, x, height), sorted into row order below.
        w, h = c['width'], c['height']
        for cy in range(c['y'] // size, (c['y'] + c['height'] - 1) // size + 1):
            for cx in range(c['x'] // size, (c['x'] + c['width'] - 1) // size + 1):
                ox, oy = cx * size - c['x'], cy * size - c['y']
                for key, value in (chunks.get((cx, cy)) or ('', {}))[1].items():
                    lx, _, ly = key.partition(',')
                    x, y = ox + int(lx), oy + int(ly)
                    if 0 <= x < w and 0 <= y < h:
                        found.append((y, x, value))
        found.sort()
        ground[c['id']] = (rows, {f'{x},{y}': int(v) if isinstance(v, float) and v.is_integer() else v for y, x, v in found})
    return ground


PREVIEW_STEP = 4
HEIGHT_CODES = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+-*'   # -16 .. 16 by halves.


def _previews(chunk_rows, cells, size):
    """Every PREVIEW_STEP-th glyph of every PREVIEW_STEP-th row of each cell, from the chunks' glyphs (cx, cy, glyphs)."""
    chunks = {(cx, cy): glyphs for cx, cy, glyphs in chunk_rows}
    out = {}
    for c in cells:
        rows = []
        for y in range(c['y'], c['y'] + c['height'], PREVIEW_STEP):
            cy, row = y // size, []
            for x in range(c['x'], c['x'] + c['width'], PREVIEW_STEP):
                found = chunks.get((x // size, cy))
                row.append(found[(y - cy * size) * size + x - (x // size) * size] if found else '.')
            rows.append(''.join(row))
        out[c['id']] = rows
    return out


def encode_heights(heights: dict, width: int, height: int):
    """A cell's heights as rows of one character a tile ('.' for none, else HEIGHT_CODES by half steps from -16): a
    fraction of the size of the "x,y" object, which is most of a world's data. None if a height doesn't fit."""
    rows = [['.'] * width for _ in range(height)]
    for key, value in heights.items():
        x, _, y = key.partition(',')
        index = value * 2 + 32
        if not float(index).is_integer() or not 0 <= index < len(HEIGHT_CODES) or not (0 <= int(x) < width and 0 <= int(y) < height):
            return None
        rows[int(y)][int(x)] = HEIGHT_CODES[int(index)]
    return [''.join(r) for r in rows]


def decode_heights(rows) -> dict:
    """encode_heights, undone."""
    out = {}
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch != '.':
                v = (HEIGHT_CODES.index(ch) - 32) / 2
                out[f'{x},{y}'] = int(v) if v.is_integer() else v
    return out


def load_ground(conn, world_id: str, ids) -> dict:
    """These world cells' ground as it is now, read from only the terrain chunks they cover: {id: {x, y, width, height,
    terrain, heights}}. Cells that don't exist are left out."""
    head = conn.execute('SELECT chunk_size FROM world.worlds WHERE id = %s', (world_id,)).fetchone()
    if not head or not ids:
        return {}
    size = head[0]
    cells = [{'id': r[0], 'x': r[1], 'y': r[2], 'width': r[3], 'height': r[4]} for r in conn.execute(
        'SELECT id, x, y, width, height FROM world.cells WHERE world_id = %s AND id = ANY(%s)', (world_id, list(ids))).fetchall()]
    wanted = {(cx, cy) for c in cells for cy in range(c['y'] // size, (c['y'] + c['height'] - 1) // size + 1)
              for cx in range(c['x'] // size, (c['x'] + c['width'] - 1) // size + 1)}
    chunk_rows = [r for r in conn.execute(
        'SELECT cx, cy, glyphs, heights FROM world.terrain_chunks WHERE world_id = %s AND cx = ANY(%s) AND cy = ANY(%s)',
        (world_id, sorted({x for x, _ in wanted}), sorted({y for _, y in wanted}))).fetchall() if (r[0], r[1]) in wanted]
    ground = _cut(chunk_rows, cells, size)
    return {c['id']: {**c, 'terrain': ground[c['id']][0], 'heights': ground[c['id']][1]} for c in cells}


def load_world(conn, world_id: str, ground: bool = True) -> tuple[dict, int]:
    """The project as the editor edits it (atlas v3: every world cell with its own ground), and its revision.

    Without `ground`, each world cell comes as its outline: `terrain` and `heights` are None, and `preview` holds every
    PREVIEW_STEP-th tile of every PREVIEW_STEP-th row, enough to draw it zoomed out. Its ground is fetched as it comes
    into view (load_ground). Interiors always come whole."""
    head = conn.execute('''SELECT name, spawn_area, spawn_x, spawn_y, chunk_size, revision, updated_at
                           FROM world.worlds WHERE id = %s''', (world_id,)).fetchone()
    if head is None:
        raise StoreError(f'Unknown world: {world_id}.')
    name, spawn_area, spawn_x, spawn_y, size, revision, updated = head
    q = lambda sql: conn.execute(sql, (world_id,)).fetchall()

    claims = claims_by_area(conn, world_id)
    dicts = lambda sql: dict_rows(conn, sql, (world_id,))
    places = lambda sql: [{**r, 'claims': claims.get(r['id'], [])} for r in dicts(sql)]
    cells = [cell_entity(r) for r in places('SELECT * FROM world.cells WHERE world_id = %s ORDER BY position')]
    if ground:
        _ground_into(conn, q, world_id, cells, size, revision, updated)
    else:
        previews = _previews(q('SELECT cx, cy, glyphs FROM world.terrain_chunks WHERE world_id = %s'), cells, size)
        for c in cells:
            c['terrain'], c['heights'], c['preview'] = None, None, previews[c['id']]
    rooms = [room_entity(r) for r in places('SELECT * FROM world.interiors WHERE world_id = %s ORDER BY position')]
    links = [link_entity(r) for r in dicts('SELECT * FROM world.links WHERE world_id = %s ORDER BY position')]
    herb = q("SELECT area, x, y FROM world.resources WHERE world_id = %s AND kind = 'herb' ORDER BY id")
    posts = {}
    for route_id, area, x, y in q('SELECT route_id, area, x, y FROM live.patrol_posts WHERE world_id = %s ORDER BY route_id, seq'):
        posts.setdefault(route_id, []).append({'cell': area, 'x': x, 'y': y})
    people = [person_entity(r) for r in dicts('SELECT * FROM live.npcs WHERE world_id = %s ORDER BY position')]
    slots = [slot_entity(r) for r in dicts('SELECT * FROM live.profession_slots WHERE world_id = %s ORDER BY position')]
    economy = economy_value(conn, world_id)
    project = {
        'format': 'ratw-atlas', 'version': 3, 'id': world_id, 'name': name,
        'cells': cells, 'rooms': rooms, 'links': links,
        'spawn': {'cell': spawn_area, 'x': spawn_x, 'y': spawn_y} if spawn_area else None,
        'factions': [{'id': r[0], 'name': r[1], 'color': r[2]}
                     for r in q('SELECT id, name, color FROM live.factions WHERE world_id = %s ORDER BY position')],
        'chapters': [{'id': r[0], 'name': r[1]} for r in q('SELECT id, name FROM world.chapters WHERE world_id = %s ORDER BY position')],
        'people': people,
        'routes': [{'id': r[0], 'name': r[1], 'posts': posts.get(r[0], [])}
                   for r in q('SELECT id, name FROM live.patrol_routes WHERE world_id = %s ORDER BY position')],
        'economy': economy if economy is not None else default_economy(),
        'herbPatch': {'cell': herb[0][0], 'x': herb[0][1], 'y': herb[0][2]} if herb else None,
        'slots': slots,
        # Families (doc 62, decision 8): `npc` is `other`'s spouse, or `npc` is `other`'s parent.
        'relations': [{'npc': r[0], 'other': r[1], 'kind': r[2]}
                      for r in q('SELECT npc_id, other_id, kind FROM live.npc_relations WHERE world_id = %s '
                                 'ORDER BY npc_id, other_id, kind')],
    }
    return project, revision


def _ground_into(conn, q, world_id, cells, size, revision, updated):
    """Gives each cell its ground (terrain rows and heights), from the kept copy while that is still current."""
    # Each cell's ground, cut from the terrain chunks it covers: the slow part of a load, and changed only by edits
    # that raise the revision, so kept per revision (a fresh copy each time; callers may change what they get).
    # Not only the revision: a world made again from scratch starts its revisions over, and publishing writes PROD's
    # rows as DEV has them. Any write to a chunk or a cell gives that row a new version (xmin), so these catch them all.
    version = (revision, updated, size) + conn.execute('''
        SELECT (SELECT coalesce(max(xmin::text::bigint), 0) || ':' || count(*) FROM world.terrain_chunks WHERE world_id = %s),
               (SELECT coalesce(max(xmin::text::bigint), 0) || ':' || count(*) FROM world.cells WHERE world_id = %s)''',
        (world_id, world_id)).fetchone()
    ground = _cached_ground(conn, world_id, version)
    if ground is None or any(c['id'] not in ground for c in cells):
        ground = _cut_ground(q, cells, size)
        _keep_ground(conn, world_id, version, ground)
    for c in cells:
        c['terrain'], c['heights'] = ground[c['id']]


# --------------------------------------------------------------------------- Roster

PROFILE_KEYS = ('appearance', 'voice', 'description', 'personality', 'traits', 'backstory', 'greeting', 'preferences')


def load_roster(conn) -> dict:
    professions = [{'id': r[0], 'name': r[1], 'behavior': r[2], 'workLabel': r[3], 'description': r[4],
                    'hours': {'start': num(r[5]), 'end': num(r[6])}, 'paid': r[7]}
                   for r in conn.execute('''SELECT id, name, behavior, work_label, description, hours_start, hours_end, paid
                                            FROM live.professions ORDER BY position''').fetchall()]
    characters = []
    for r in conn.execute('''SELECT id, name, age, status, profession, origin, assignment, profile
                             FROM live.characters ORDER BY position''').fetchall():
        profile = r[7]
        characters.append({'id': r[0], 'name': r[1], 'age': r[2], **{k: profile[k] for k in PROFILE_KEYS if k in profile},
                           'status': r[3], 'profession': r[4] or '', 'assignment': r[6], 'origin': r[5]})
    return roster_lib.check_roster({'format': 'ratw-roster', 'version': 1, 'professions': professions, 'characters': characters})


def save_roster(conn, roster: dict) -> dict:
    """Replaces the stored roster with a validated one, writing only changed rows."""
    r = roster_lib.check_roster(roster)
    with conn.transaction():
        conn.execute('SET CONSTRAINTS ALL DEFERRED')
        current = load_roster(conn)
        old_p = {p['id']: p for p in current['professions']}
        old_c = {c['id']: c for c in current['characters']}
        positions = {p['id']: i for i, p in enumerate(current['professions'])}
        for i, p in enumerate(r['professions']):
            if old_p.get(p['id']) != p or positions.get(p['id']) != i:
                conn.execute('''INSERT INTO live.professions (id, position, name, behavior, work_label, description, hours_start, hours_end, paid)
                                VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s) ON CONFLICT (id) DO UPDATE SET
                                position = excluded.position, name = excluded.name, behavior = excluded.behavior,
                                work_label = excluded.work_label, description = excluded.description,
                                hours_start = excluded.hours_start, hours_end = excluded.hours_end, paid = excluded.paid''',
                             (p['id'], i, p['name'], p['behavior'], p['workLabel'], p['description'],
                              p['hours']['start'], p['hours']['end'], p['paid']))
        positions = {c['id']: i for i, c in enumerate(current['characters'])}
        for i, c in enumerate(r['characters']):
            if old_c.get(c['id']) != c or positions.get(c['id']) != i:
                conn.execute('''INSERT INTO live.characters (id, position, name, age, status, profession, origin, assignment, profile, updated_at)
                                VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, now()) ON CONFLICT (id) DO UPDATE SET
                                position = excluded.position, name = excluded.name, age = excluded.age, status = excluded.status,
                                profession = excluded.profession, origin = excluded.origin, assignment = excluded.assignment,
                                profile = excluded.profile, updated_at = now()''',
                             (c['id'], i, c['name'], c['age'], c['status'], c['profession'] or None, c['origin'],
                              Jsonb(c['assignment']) if c['assignment'] is not None else None,
                              Jsonb({k: c[k] for k in PROFILE_KEYS if k in c})))
        for cid in old_c.keys() - {c['id'] for c in r['characters']}:
            conn.execute('DELETE FROM live.characters WHERE id = %s', (cid,))
        for pid in old_p.keys() - {p['id'] for p in r['professions']}:
            conn.execute('DELETE FROM live.professions WHERE id = %s', (pid,))
    return load_roster(conn)


class DbRoster:
    """The shared roster in a dev world (Atlas's unless told otherwise), with the same load/save interface as a
    roster file."""

    def __init__(self, connect=lambda: world_db.connect(world_db.atlas_world(), 'editor')):
        self.connect = connect

    def load(self) -> dict:
        with self.connect() as conn:
            return load_roster(conn)

    def save(self, roster: dict) -> dict:
        with self.connect() as conn:
            return save_roster(conn, roster)


# --------------------------------------------------------------------------- Import

def import_files(conn, replace: bool = False, log=print) -> None:
    """One-time move of Data/Characters/roster.json and the bundled world into the database.

    There is one world; the bundled atlas becomes it unless the database already has one.
    """
    save_roster(conn, roster_lib.load())
    log('Imported the character roster.')
    bundled = map_editor.bundled_worlds()
    if len(bundled) > 1:
        raise StoreError('There can be only one world, but Data/Worlds has several.')
    for world in bundled:
        project = map_editor.read_json(world['source'].read_text(encoding='utf-8'))
        wid = map_editor.world_id(project)
        other = conn.execute('SELECT id FROM world.worlds WHERE id <> %s', (wid,)).fetchone()
        if other:
            raise StoreError(f'The database already holds the world {other[0]}; there can be only one.')
        existing = conn.execute('SELECT revision FROM world.worlds WHERE id = %s', (wid,)).fetchone()
        if existing and not replace:
            log(f'{world["id"]}: already in the database as {wid} (use --replace to overwrite).')
            continue
        revision = save_world(conn, project, existing[0] if existing else None, create=not existing)
        log(f'{world["id"]}: imported as {wid}, revision {revision}.')


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    i = sub.add_parser('import', help='import the roster and bundled worlds into DEV')
    i.add_argument('--replace', action='store_true', help='overwrite worlds already in the database')
    sub.add_parser('list', help='list worlds in DEV')
    parser.add_argument('--world', default='atlas', help="the dev world (default Atlas's)")
    args = parser.parse_args(argv)
    try:
        with world_db.connect(world_db.resolve(args.world, 'dev'), 'editor') as conn:
            if args.command == 'import':
                import_files(conn, args.replace)
            else:
                for w in list_worlds(conn):
                    print(f'{w["id"]:<24} {w["name"]:<32} revision {w["revision"]:<4} {w["cells"]} places, {w["people"]} people')
    except (world_db.DatabaseError, StoreError, map_editor.ValidationError, roster_lib.RosterError) as error:
        print(getattr(error, 'errors', None) and '\n'.join(error.errors) or error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
