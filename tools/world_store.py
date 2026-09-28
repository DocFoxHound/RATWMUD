#!/usr/bin/env python3
"""Worlds and the character roster in PostgreSQL (see Docs/Design/20-world-database.md).

Converts between the editor's project JSON and database rows. Saving compares
with what is stored and writes only the rows that changed, in one transaction,
and refuses if someone else saved the world since it was opened.

  python3 tools/world_store.py import             import Data/Characters and Data/Worlds into DEV
  python3 tools/world_store.py import --replace   ...replacing worlds already there
  python3 tools/world_store.py list               worlds in DEV
"""
from __future__ import annotations

import argparse
import pickle
import sys
import threading

import map_editor
import roster as roster_lib
import world_db

DEFAULT_LIGHTING = {'artificial': 1, 'daylightAccess': 1, 'tone': 'warm'}
DEFAULT_TERRITORY = {'region': 'unassigned', 'claims': [], 'chapter': ''}
DEFAULT_ECONOMY = {'treasury': 1000, 'storeHerbs': 100, 'storeMeals': 50, 'dailyHerbs': 10, 'dailyMeals': 12}
HERB_PATCH = 'herb_patch'


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
                                         'region', 'chapter')),
    'world.interiors': (('world_id', 'id'), ('world_id', 'id', 'position', 'name', 'description', 'width', 'height', 'glyphs',
                                             'heights', 'z', 'outdoors', 'weather', 'light_artificial', 'light_daylight',
                                             'light_tone', 'region', 'chapter', 'overview_x', 'overview_y')),
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
                                      'work_area', 'work_x', 'work_y', 'evening_area', 'evening_x', 'evening_y')),
    'live.profession_slots': (('world_id', 'id'), ('world_id', 'id', 'position', 'name', 'profession', 'work_label', 'route_id',
                                                   'paid', 'purse', 'herbs', 'meals', 'hours_start', 'hours_end',
                                                   'home_area', 'home_x', 'home_y', 'work_area', 'work_x', 'work_y',
                                                   'evening_area', 'evening_x', 'evening_y')),
    'live.economy': (('world_id',), ('world_id', 'treasury', 'store_herbs', 'store_meals', 'daily_herbs', 'daily_meals')),
}
# Children are deleted before parents and written after them; deferred foreign keys settle the rest at commit.
WRITE_ORDER = ['world.areas', 'world.terrain_chunks', 'world.cells', 'world.interiors', 'world.links', 'world.resources',
               'live.factions', 'world.chapters', 'live.patrol_routes', 'live.patrol_posts', 'live.npcs',
               'live.profession_slots', 'live.economy']
JSON_COLUMNS = {'heights', 'appearance'}


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
            c.get('z', 0), c['outdoors'], c['weather'], *lighting(c), *territory(c))


def room_row(w, r, position):
    return (w, r['id'], position, r['name'], r.get('description', ''), r['width'], r['height'], ''.join(r['terrain']),
            dict(r.get('heights', {})), r.get('z', 0), r['outdoors'], r['weather'], *lighting(r), *territory(r),
            r.get('worldX', 0), r.get('worldY', 0))


def link_row(w, link, position):
    return (w, link['id'], position, link['name'], link['kind'], link['open'], *place(link['a']), *place(link['b']))


def post_rows(w, route):
    return [(w, route['id'], seq, *place(post)) for seq, post in enumerate(route['posts'])]


def npc_row(w, n, position):
    return (w, n['id'], position, n['name'], n['role'], n['description'], n['greeting'], n.get('personality', ''),
            n.get('backstory', ''), n['workLabel'], n['age'], n['voice'], dict(n['appearance']), n['route'] or None,
            n['paid'], n['purse'], n['herbs'], n['meals'], n['hours']['start'], n['hours']['end'],
            *place(n['home']), *place(n['work']), *place(n['evening']))


def slot_row(w, s, position):
    return (w, s['id'], position, s['name'], s['profession'], s['workLabel'], s['route'] or None, s['paid'], s['purse'],
            s['herbs'], s['meals'], s['hours']['start'], s['hours']['end'],
            *place(s['home']), *place(s['work']), *place(s['evening']))


def economy_row(w, economy):
    e = {**DEFAULT_ECONOMY, **(economy or {})}
    return (w, e['treasury'], e['storeHerbs'], e['storeMeals'], e['dailyHerbs'], e['dailyMeals'])


def _details(r):
    return {'z': r['z'], 'outdoors': r['outdoors'], 'weather': r['weather'],
            'lighting': {'artificial': num(r['light_artificial']), 'daylightAccess': num(r['light_daylight']), 'tone': r['light_tone']},
            'territory': {'region': r['region'], 'claims': list(r.get('claims', [])), 'chapter': r['chapter']}}


def _anchor(r, k):
    return {'cell': r[f'{k}_area'], 'x': r[f'{k}_x'], 'y': r[f'{k}_y']}


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
            'home': _anchor(r, 'home'), 'work': _anchor(r, 'work'), 'evening': _anchor(r, 'evening')}


def person_entity(r):
    return {'id': r['id'], 'name': r['name'], 'role': r['role'], 'description': r['description'], 'greeting': r['greeting'],
            'age': r['age'], 'appearance': r['appearance'], 'voice': r['voice'], **_resident(r),
            'personality': r['personality'], 'backstory': r['backstory']}


def slot_entity(r):
    return {'id': r['id'], 'name': r['name'], 'profession': r['profession'], **_resident(r)}


def economy_entity(r):
    return {'treasury': r['treasury'], 'storeHerbs': r['store_herbs'], 'storeMeals': r['store_meals'],
            'dailyHerbs': r['daily_herbs'], 'dailyMeals': r['daily_meals']}


def dict_rows(conn, sql, params):
    cur = conn.execute(sql, params)
    names = [d.name for d in cur.description]
    return [dict(zip(names, r)) for r in cur.fetchall()]


def project_rows(p: dict, chunk_size: int) -> dict[str, list[tuple]]:
    """Every row a validated (atlas v3) project stores, per table. Cells and terrain are stored in world coordinates."""
    w = p['id']
    rows = {table: [] for table in TABLES}
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
    rows['live.economy'].append(economy_row(w, p.get('economy', {})))
    return rows


def stored_rows(conn, world_id: str) -> dict[str, list[tuple]]:
    out = {}
    for table, (_, columns) in TABLES.items():
        out[table] = [tuple(r) for r in conn.execute(
            f'SELECT {", ".join(columns)} FROM {table} WHERE world_id = %s', (world_id,)).fetchall()]
    return out


def write_rows(conn, desired: dict[str, list[tuple]], current: dict[str, list[tuple]]) -> int:
    """Deletes, updates and inserts only what differs. Returns the number of rows changed."""
    changed = 0
    plans = {}
    for table in WRITE_ORDER:
        keys, columns = TABLES[table]
        n = len(keys)
        have = {r[:n]: r for r in current[table]}
        want = {r[:n]: r for r in desired[table]}
        stale = [k for k in have if k not in want]
        fresh = [r for k, r in want.items() if k not in have or tuple(map(comparable, have[k])) != tuple(map(comparable, r))]
        plans[table] = (stale, fresh)
    for table in reversed(WRITE_ORDER):
        keys, _ = TABLES[table]
        for key in plans[table][0]:
            conn.execute(f'DELETE FROM {table} WHERE ' + ' AND '.join(f'{k} = %s' for k in keys), key)
            changed += 1
    for table in WRITE_ORDER:
        keys, columns = TABLES[table]
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


def save_world(conn, project: dict, revision: int | None, create: bool = False) -> int:
    """Validates and stores a project. `revision` is the one it was opened at (None to create).

    Returns the new revision. Raises ConflictError if the world changed since it was opened.
    """
    p, _ = map_editor.check_project(project, for_game=False)
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
        write_rows(conn, project_rows(p, chunk_size), stored_rows(conn, p['id']))
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
    economy = dicts('SELECT * FROM live.economy WHERE world_id = %s')
    e = economy[0] if economy else None
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
        'economy': economy_entity(e) if e else dict(DEFAULT_ECONOMY),
        'herbPatch': {'cell': herb[0][0], 'x': herb[0][1], 'y': herb[0][2]} if herb else None,
        'slots': slots,
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
    """The shared roster in DEV, with the same load/save interface as a roster file."""

    def __init__(self, connect=lambda: world_db.connect('dev', 'editor')):
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
    args = parser.parse_args(argv)
    try:
        with world_db.connect('dev', 'editor') as conn:
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
