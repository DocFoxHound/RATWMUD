#!/usr/bin/env python3
"""Live, shared editing of the one world in PostgreSQL.

Every editor action becomes a batch of keyed edits. Each edit names what it
changes, the value the editor saw before and the value it wants now:

  name  spawn  herb  economy              world settings
  tile:X,Y  height:X,Y                     world terrain (world tiles; "." is null)
  cell:ID  room:ID  link:ID  faction:ID  chapter:ID  person:ID  slot:ID  route:ID
  rtile:ROOM:X,Y  rheight:ROOM:X,Y         interior terrain (interior tiles)

A batch is applied whole or not at all. If anything in it changed since the
editor last saw it, the batch is refused and the reply names who changed it.
Writers take turns on the world row, so the edit log is in commit order and
every editor can follow it with "changes after seq N". The same values are
built by the editor's live.ts; entities are compared as canonical JSON.
"""
from __future__ import annotations

import json
import psycopg

import map_editor
import world_db
import world_store as S
import terrain_catalog

MAX_OPS = 250_000
PRESENCE_SECONDS = 20
MAX_CHANGES = 20_000
GLYPHS = terrain_catalog.GLYPHS


def half_step(key, value):
    """A height override: a half-tile step from -16 to 16 (quarter steps are retired)."""
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not -16 <= value <= 16 or value * 2 != int(value * 2):
        raise ValueError(f'{key}: heights are half-tile steps from -16 to 16.')
    return float(value) if value != int(value) else int(value)


class Conflict(S.StoreError):
    status = 409

    def __init__(self, message, conflicts, current):
        super().__init__(message)
        self.conflicts, self.current = conflicts, current


class Rejected(S.StoreError):
    status = 422

    def __init__(self, message, current):
        super().__init__(message)
        self.current = current


def canonical(value):
    def clean(v):
        if isinstance(v, float) and v.is_integer():
            return int(v)
        if isinstance(v, dict):
            return {k: clean(x) for k, x in v.items()}
        if isinstance(v, list):
            return [clean(x) for x in v]
        return v
    return json.dumps(clean(value), sort_keys=True, separators=(',', ':'))


def xy(text):
    x, y = text.split(',')
    return int(x), int(y)


# --------------------------------------------------------------------------- One batch

class Batch:
    """Reads current values and applies new ones for one transaction, then writes the rows that changed."""

    ENTITIES = {  # kind: (table, reader, builder)
        'cell': ('world.cells', S.cell_entity, S.cell_row),
        'link': ('world.links', S.link_entity, S.link_row),
        'person': ('live.npcs', S.person_entity, S.npc_row),
        'slot': ('live.profession_slots', S.slot_entity, S.slot_row),
    }

    def __init__(self, conn, world):
        self.conn, self.w, self.world = conn, world['id'], world
        self.size = world['chunk_size']
        self.chunks = {}      # (cx, cy) -> {'glyphs': list, 'heights': dict, 'dirty': bool}
        self.rooms = {}       # id -> {'meta': dict | None, 'tiles': {(x, y): glyph}, 'heights': {(x, y): h}, 'dirty': bool}
        self.pending = {}     # entity key -> new value (None deletes)
        self.head = {}        # name/spawn/bounds changes

    # -- reading ---------------------------------------------------------------
    def rows(self, sql, *params):
        return S.dict_rows(self.conn, sql, (self.w, *params))

    def chunk(self, x, y):
        key = (x // self.size, y // self.size)
        if key not in self.chunks:
            row = self.conn.execute('SELECT glyphs, heights FROM world.terrain_chunks WHERE world_id = %s AND cx = %s AND cy = %s',
                                    (self.w, *key)).fetchone()
            self.chunks[key] = {'glyphs': list(row[0]) if row else ['.'] * self.size * self.size,
                                'heights': dict(row[1]) if row else {}, 'dirty': False}
        return self.chunks[key], x - key[0] * self.size, y - key[1] * self.size

    def room(self, rid):
        if rid not in self.rooms:
            found = self.rows('SELECT * FROM world.interiors WHERE world_id = %s AND id = %s', rid)
            if found:
                r = {**found[0], 'claims': self.claims(rid)}
                tiles = {(x, y): r['glyphs'][y * r['width'] + x] for y in range(r['height']) for x in range(r['width'])}
                heights = {xy(k): S.num(v) for k, v in r['heights'].items()}
                self.rooms[rid] = {'meta': S.room_meta(r), 'tiles': tiles, 'heights': heights, 'dirty': False}
            else:
                self.rooms[rid] = {'meta': None, 'tiles': {}, 'heights': {}, 'dirty': False}
        return self.rooms[rid]

    def claims(self, area):
        return [r[0] for r in self.conn.execute('SELECT faction_id FROM live.faction_claims WHERE world_id = %s AND area = %s '
                                                'ORDER BY faction_id', (self.w, area)).fetchall()]

    def current(self, key):
        if key in self.pending:
            return self.pending[key]
        kind, _, rest = key.partition(':')
        if kind == 'tile':
            c, lx, ly = self.chunk(*xy(rest))
            glyph = c['glyphs'][ly * self.size + lx]
            return None if glyph == '.' else glyph
        if kind == 'height':
            c, lx, ly = self.chunk(*xy(rest))
            return S.num(c['heights'].get(f'{lx},{ly}'))
        if kind in ('rtile', 'rheight'):
            rid, _, at = rest.partition(':')
            room = self.room(rid)
            return (room['tiles'] if kind == 'rtile' else room['heights']).get(xy(at))
        if kind == 'room':
            return self.room(rest)['meta']
        if kind in self.ENTITIES:
            table, reader, _ = self.ENTITIES[kind]
            found = self.rows(f'SELECT * FROM {table} WHERE world_id = %s AND id = %s', rest)
            if found and kind == 'cell':
                found[0]['claims'] = self.claims(rest)
            return reader(found[0]) if found else None
        if kind == 'route':
            found = self.rows('SELECT id, name FROM live.patrol_routes WHERE world_id = %s AND id = %s', rest)
            if not found:
                return None
            posts = [{'cell': a, 'x': x, 'y': y} for a, x, y in self.conn.execute(
                'SELECT area, x, y FROM live.patrol_posts WHERE world_id = %s AND route_id = %s ORDER BY seq', (self.w, rest))]
            return {'id': rest, 'name': found[0]['name'], 'posts': posts}
        if kind == 'faction':
            found = self.rows('SELECT id, name, color FROM live.factions WHERE world_id = %s AND id = %s', rest)
            return found[0] if found else None
        if kind == 'chapter':
            found = self.rows('SELECT id, name FROM world.chapters WHERE world_id = %s AND id = %s', rest)
            return found[0] if found else None
        if key == 'name':
            return self.world['name']
        if key == 'spawn':
            w = self.world
            return {'cell': w['spawn_area'], 'x': w['spawn_x'], 'y': w['spawn_y']} if w['spawn_area'] else None
        if key == 'herb':
            found = self.rows("SELECT area, x, y FROM world.resources WHERE world_id = %s AND id = %s", S.HERB_PATCH)
            return {'cell': found[0]['area'], 'x': found[0]['x'], 'y': found[0]['y']} if found else None
        if key == 'economy':
            found = self.rows('SELECT * FROM live.economy WHERE world_id = %s')
            return S.economy_entity(found[0]) if found else None
        raise ValueError(f'Unknown edit key: {key}')

    # -- applying ----------------------------------------------------------------
    def set(self, key, value):
        kind, _, rest = key.partition(':')
        if kind == 'tile':
            if value is not None and (not isinstance(value, str) or value not in GLYPHS):
                raise ValueError(f'{key}: not a terrain glyph.')
            c, lx, ly = self.chunk(*xy(rest))
            c['glyphs'][ly * self.size + lx] = value or '.'
            c['dirty'] = True
        elif kind == 'height':
            c, lx, ly = self.chunk(*xy(rest))
            if value is None:
                c['heights'].pop(f'{lx},{ly}', None)
            else:
                c['heights'][f'{lx},{ly}'] = half_step(key, value)
            c['dirty'] = True
        elif kind in ('rtile', 'rheight', 'room'):
            rid, _, at = rest.partition(':')
            room = self.room(rid)
            if kind == 'room':
                if value is not None and value.get('id') != rid:
                    raise ValueError(f'{key}: ID does not match.')
                room['meta'] = value
            else:
                target = room['tiles'] if kind == 'rtile' else room['heights']
                if value is None:
                    target.pop(xy(at), None)
                elif kind == 'rtile' and value not in GLYPHS:
                    raise ValueError(f'{key}: not a terrain glyph.')
                else:
                    target[xy(at)] = value if kind == 'rtile' else half_step(key, value)
            room['dirty'] = True
        elif kind in ('cell', 'link', 'person', 'slot', 'route', 'faction', 'chapter'):
            if value is not None and value.get('id') != rest:
                raise ValueError(f'{key}: ID does not match.')
            self.pending[key] = value
        elif key in ('name', 'spawn', 'herb', 'economy'):
            self.pending[key] = value
        else:
            raise ValueError(f'Unknown edit key: {key}')

    def position(self, table, id_):
        row = self.conn.execute(f'SELECT position FROM {table} WHERE world_id = %s AND id = %s', (self.w, id_)).fetchone()
        if row:
            return row[0]
        return self.conn.execute(f'SELECT coalesce(max(position) + 1, 0) FROM {table} WHERE world_id = %s', (self.w,)).fetchone()[0]

    def upsert(self, table, row):
        keys, columns = S.TABLES[table]
        rest = [c for c in columns if c not in keys]
        values = [S.Jsonb(v) if c in S.JSON_COLUMNS else v for c, v in zip(columns, row)]
        self.conn.execute(f'INSERT INTO {table} ({", ".join(columns)}) VALUES ({", ".join(["%s"] * len(columns))}) '
                          f'ON CONFLICT ({", ".join(keys)}) DO UPDATE SET ' + ', '.join(f'{c} = excluded.{c}' for c in rest), values)

    def area(self, id_, kind):
        self.conn.execute('INSERT INTO world.areas (world_id, id, kind) VALUES (%s, %s, %s) '
                          'ON CONFLICT (world_id, id) DO UPDATE SET kind = excluded.kind', (self.w, id_, kind))

    def flush(self):
        w = self.w
        # Deletions first (areas cascade to their doors), then everything else.
        for key, value in self.pending.items():
            kind, _, id_ = key.partition(':')
            if value is not None:
                continue
            if kind == 'cell':
                self.conn.execute('DELETE FROM live.faction_claims WHERE world_id = %s AND area = %s', (w, id_))
                self.conn.execute('DELETE FROM live.npc_areas WHERE world_id = %s AND area = %s', (w, id_))   # Spawn rules go with them.
                self.conn.execute("DELETE FROM world.areas WHERE world_id = %s AND id = %s AND kind = 'cell'", (w, id_))
            elif kind in ('link', 'person', 'slot', 'faction', 'chapter'):
                table = {'link': 'world.links', 'person': 'live.npcs', 'slot': 'live.profession_slots',
                         'faction': 'live.factions', 'chapter': 'world.chapters'}[kind]
                self.conn.execute(f'DELETE FROM {table} WHERE world_id = %s AND id = %s', (w, id_))
            elif kind == 'route':
                self.conn.execute('DELETE FROM live.patrol_routes WHERE world_id = %s AND id = %s', (w, id_))
        for rid, room in self.rooms.items():
            if room['dirty'] and room['meta'] is None:
                self.conn.execute('DELETE FROM live.faction_claims WHERE world_id = %s AND area = %s', (w, rid))
                self.conn.execute('DELETE FROM live.npc_areas WHERE world_id = %s AND area = %s', (w, rid))   # Spawn rules go with them.
                self.conn.execute("DELETE FROM world.areas WHERE world_id = %s AND id = %s AND kind = 'interior'", (w, rid))

        # Factions first: the places' claims refer to them.
        for key, value in sorted(self.pending.items(), key=lambda kv: not kv[0].startswith('faction:')):
            kind, _, id_ = key.partition(':')
            if value is None and kind not in ('spawn', 'herb', 'name', 'economy'):
                continue
            if key in ('spawn', 'herb', 'name', 'economy'):
                self.head_change(key, value)
            elif kind == 'cell':
                self.area(id_, 'cell')
                self.upsert('world.cells', S.cell_row(w, value, self.position('world.cells', id_)))
                S.sync_claims(self.conn, w, id_, S.claims_of(value))
            elif kind in self.ENTITIES:
                table, _, build = self.ENTITIES[kind]
                self.upsert(table, build(w, value, self.position(table, id_)))
            elif kind == 'faction':
                self.upsert('live.factions', (w, id_, self.position('live.factions', id_), value['name'], value['color']))
            elif kind == 'chapter':
                self.upsert('world.chapters', (w, id_, self.position('world.chapters', id_), value['name']))
            elif kind == 'route':
                self.upsert('live.patrol_routes', (w, id_, self.position('live.patrol_routes', id_), value['name']))
                self.conn.execute('DELETE FROM live.patrol_posts WHERE world_id = %s AND route_id = %s', (w, id_))
                for row in S.post_rows(w, value):
                    self.upsert('live.patrol_posts', row)

        for rid, room in self.rooms.items():
            if not room['dirty'] or room['meta'] is None:
                continue
            meta, width, height = room['meta'], room['meta']['width'], room['meta']['height']
            terrain = [''.join(room['tiles'].get((x, y), '.') for x in range(width)) for y in range(height)]
            heights = {f'{x},{y}': h for (x, y), h in sorted(room['heights'].items(), key=lambda kv: kv[0][::-1])
                       if x < width and y < height}
            self.area(rid, 'interior')
            self.upsert('world.interiors', S.room_row(w, {**meta, 'terrain': terrain, 'heights': heights},
                                                        self.position('world.interiors', rid)))
            S.sync_claims(self.conn, w, rid, S.claims_of(meta))

        if any(key.startswith('cell:') for key in self.pending):
            # The rectangle around every cell, kept on the world row for information (the world has no edge).
            self.conn.execute('''UPDATE world.worlds w SET min_x = b.x, min_y = b.y, width = b.w, height = b.h FROM (
                                     SELECT coalesce(min(x), 0) AS x, coalesce(min(y), 0) AS y,
                                            coalesce(max(x + width) - min(x), 0) AS w, coalesce(max(y + height) - min(y), 0) AS h
                                     FROM world.cells WHERE world_id = %s) b WHERE w.id = %s''', (w, w))
        for (cx, cy), c in self.chunks.items():
            if not c['dirty']:
                continue
            glyphs = ''.join(c['glyphs'])
            if glyphs.strip('.') or c['heights']:
                self.upsert('world.terrain_chunks', (w, cx, cy, self.size, glyphs, c['heights']))
            else:
                self.conn.execute('DELETE FROM world.terrain_chunks WHERE world_id = %s AND cx = %s AND cy = %s', (w, cx, cy))

    def head_change(self, key, value):
        w = self.w
        if key == 'name':
            if not isinstance(value, str) or not value.strip():
                raise ValueError('The world needs a name.')
            self.conn.execute('UPDATE world.worlds SET name = %s WHERE id = %s', (value, w))
        elif key == 'spawn':
            spawn = S.place(value) if value else (None, None, None)
            self.conn.execute('UPDATE world.worlds SET spawn_area = %s, spawn_x = %s, spawn_y = %s WHERE id = %s', (*spawn, w))
        elif key == 'herb':
            self.conn.execute('DELETE FROM world.resources WHERE world_id = %s AND id = %s', (w, S.HERB_PATCH))
            if value:
                self.conn.execute("INSERT INTO world.resources (world_id, id, kind, area, x, y) VALUES (%s, %s, 'herb', %s, %s, %s)",
                                  (w, S.HERB_PATCH, *S.place(value)))
        elif key == 'economy':
            self.upsert('live.economy', S.economy_row(w, value))


# --------------------------------------------------------------------------- Service

def label_for(key, value):
    kind, _, rest = key.partition(':')
    name = value.get('name') if isinstance(value, dict) else None
    if kind in ('tile', 'height'):
        return f'the terrain at {rest.replace(",", ", ")}'
    if kind in ('rtile', 'rheight'):
        room, _, at = rest.partition(':')
        return f'the floor of {room} at {at.replace(",", ", ")}'
    nouns = {'cell': 'cell', 'room': 'interior', 'link': 'connection', 'person': 'NPC', 'slot': 'profession slot',
             'route': 'patrol route', 'faction': 'faction', 'chapter': 'Chapter'}
    if kind in nouns:
        return f'{nouns[kind]} {name or rest}'
    return {'name': 'the world name', 'spawn': 'the player spawn', 'herb': 'the herb patch', 'economy': 'the town economy',
            }.get(key, key)


def world_row(conn, lock=False):
    rows = S.dict_rows(conn, 'SELECT * FROM world.worlds' + (' FOR UPDATE' if lock else ''), ())
    if not rows:
        raise S.StoreError('There is no world in the database yet. Run `python3 tools/world_store.py import`.')
    return rows[0]


def latest_seq(conn):
    return conn.execute('SELECT coalesce(max(seq), 0) FROM world.edits').fetchone()[0]


def load(conn, lean=False):
    """The whole world for an editor that is starting, and the edit it is current to. Lean, each world cell comes as
    its outline and a preview (S.load_world), and its ground is asked for as it comes into view (ground())."""
    with conn.transaction():
        conn.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')
        world = world_row(conn)
        project, _ = S.load_world(conn, world['id'], ground=not lean)
        return project, latest_seq(conn)


MAX_GROUND_CELLS = 32


def ground(conn, ids):
    """These cells' ground and the edit it is current to: {id: {x, y, width, height, terrain, heightRows}}, heights as
    S.encode_heights rows (or `heights` as an object, for a cell whose heights don't fit them)."""
    if not isinstance(ids, list) or not 0 < len(ids) <= MAX_GROUND_CELLS or not all(isinstance(i, str) and 0 < len(i) <= 80 for i in ids):
        raise ValueError(f'Ask for the ground of 1 to {MAX_GROUND_CELLS} cells.')
    with conn.transaction():
        conn.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')
        world = world_row(conn)
        cells = S.load_ground(conn, world['id'], ids)
        seq = latest_seq(conn)
    out = {}
    for cid, c in cells.items():
        rows = S.encode_heights(c['heights'], c['width'], c['height'])
        out[cid] = {k: c[k] for k in ('x', 'y', 'width', 'height', 'terrain')}
        out[cid].update({'heightRows': rows} if rows is not None else {'heights': c['heights']})
    return out, seq


def fill_ground(conn, project):
    """A project from an editor that loaded lean may have cells whose ground it never fetched (terrain None): gives
    them their ground as it is now, in place. Anything else is left as sent."""
    missing = [c['id'] for c in project.get('cells', []) if isinstance(c, dict) and c.get('terrain') is None and isinstance(c.get('id'), str)]
    if not missing:
        return project
    world = world_row(conn)
    for i in range(0, len(missing), 256):
        found = S.load_ground(conn, world['id'], missing[i:i + 256])
        for c in project['cells']:
            g = found.get(c.get('id')) if isinstance(c, dict) and c.get('terrain') is None else None
            if g and all(c.get(k) == g[k] for k in ('x', 'y', 'width', 'height')):
                c['terrain'], c['heights'] = g['terrain'], g['heights']
                c.pop('preview', None)
    return project


def apply_edit(conn, client_id, editor, label, ops):
    """Applies one batch. Returns the new seq; raises Conflict or Rejected with the current values."""
    if not isinstance(ops, list) or not ops or len(ops) > MAX_OPS:
        raise ValueError('An edit needs 1 to 250000 changes.')
    keys = [op.get('key') if isinstance(op, dict) else None for op in ops]
    if not all(isinstance(k, str) and 0 < len(k) <= 160 for k in keys) or len(set(keys)) != len(keys):
        raise ValueError('Every change needs its own key.')
    current = {}
    try:
        with conn.transaction():
            batch = Batch(conn, world_row(conn, lock=True))
            conflicts = []
            for op in ops:
                now = current[op['key']] = batch.current(op['key'])
                if canonical(now) != canonical(op.get('before')):
                    last = conn.execute('SELECT editor, at FROM world.edits WHERE world_id = %s AND key = %s ORDER BY seq DESC LIMIT 1',
                                        (batch.w, op['key'])).fetchone()
                    conflicts.append({'key': op['key'], 'label': label_for(op['key'], now or op.get('before')),
                                      'editor': last[0] if last else '', 'at': last[1].isoformat() if last else None})
            if conflicts:
                who = sorted({c['editor'] for c in conflicts if c['editor']})
                raise Conflict(f'Changed by {", ".join(who) or "someone else"} since you last saw it.', conflicts, current)
            for op in ops:
                batch.set(op['key'], op.get('after'))
            batch.flush()
            conn.execute('UPDATE world.worlds SET revision = revision + 1, updated_at = now() WHERE id = %s', (batch.w,))
            cur = conn.cursor()
            cur.executemany('''INSERT INTO world.edits (world_id, batch, editor, client_id, label, key, before, after)
                               VALUES (%s, txid_current(), %s, %s, %s, %s, %s, %s)''',
                            [(batch.w, editor, client_id, label[:200], op['key'], S.Jsonb(op.get('before')), S.Jsonb(op.get('after')))
                             for op in ops])
            return latest_seq(conn)
    except (psycopg.errors.IntegrityError, ValueError, KeyError, TypeError, AttributeError) as error:
        message = getattr(getattr(error, 'diag', None), 'message_primary', None) or str(error)
        if isinstance(error, psycopg.errors.ForeignKeyViolation):
            message = 'It refers to a place that was just removed or changed by someone else.'
        elif isinstance(error, psycopg.errors.ExclusionViolation):
            message = 'It would make two world cells overlap.'
        raise Rejected(f'The database refused this change: {message}', current) from error


def change_claims(conn, client_id, editor, label, change):
    """Runs `change(conn)`, which writes live.faction_claims directly (the Dungeon Master, copying from live), and logs
    every place whose claiming factions changed as a cell or interior edit, so open editors see it. Returns its result."""
    with conn.transaction():
        world = world_row(conn, lock=True)
        old = S.claims_by_area(conn, world['id'])
        result = change(conn)
        new = S.claims_by_area(conn, world['id'])
        kinds = dict(conn.execute('SELECT id, kind FROM world.areas WHERE world_id = %s', (world['id'],)).fetchall())
        batch, ops = Batch(conn, world), []
        for area in sorted(set(old) | set(new)):
            if old.get(area, []) == new.get(area, []) or area not in kinds:
                continue
            key = f'{"cell" if kinds[area] == "cell" else "room"}:{area}'
            after = batch.current(key)
            ops.append((key, {**after, 'territory': {**after['territory'], 'claims': old.get(area, [])}}, after))
        if ops:
            conn.execute('UPDATE world.worlds SET revision = revision + 1, updated_at = now() WHERE id = %s', (world['id'],))
            conn.cursor().executemany(
                'INSERT INTO world.edits (world_id, batch, editor, client_id, label, key, before, after) '
                'VALUES (%s, txid_current(), %s, %s, %s, %s, %s, %s)',
                [(world['id'], editor, client_id, label[:200], key, S.Jsonb(before), S.Jsonb(after)) for key, before, after in ops])
        return result


def sync(conn, client_id, editor, color, since, state, world=None):
    """Heartbeat: records where this editor is, returns everyone else's edits after `since` and who is online.

    `world` is the world the editor holds; if DEV now holds a different one (it was replaced), the editor reloads.
    """
    if world and world != world_row(conn)['id']:
        return {'reload': True, 'seq': since, 'changes': [], 'editors': []}
    with conn.transaction():
        conn.execute('''INSERT INTO world.editors (client_id, editor, color, state, seen_at) VALUES (%s, %s, %s, %s, now())
                        ON CONFLICT (client_id) DO UPDATE SET editor = excluded.editor, color = excluded.color,
                        state = excluded.state, seen_at = now()''', (client_id, editor, color, S.Jsonb(state)))
        conn.execute("DELETE FROM world.editors WHERE seen_at < now() - interval '1 hour'")
        rows = conn.execute('''SELECT seq, client_id, editor, label, key, after FROM world.edits WHERE seq > %s
                               ORDER BY seq LIMIT %s''', (since, MAX_CHANGES + 1)).fetchall()
        others = conn.execute(f'''SELECT client_id, editor, color, state, extract(epoch FROM now() - seen_at)
                                  FROM world.editors WHERE client_id <> %s AND seen_at > now() - interval '{PRESENCE_SECONDS} seconds'
                                  ORDER BY editor''', (client_id,)).fetchall()
    editors = [{'clientId': r[0], 'editor': r[1], 'color': r[2], 'state': r[3], 'idle': round(float(r[4]), 1)} for r in others]
    if len(rows) > MAX_CHANGES:
        return {'reload': True, 'seq': rows[-1][0], 'changes': [], 'editors': editors}
    changes = [{'seq': r[0], 'clientId': r[1], 'editor': r[2], 'label': r[3], 'key': r[4], 'after': r[5]} for r in rows]
    return {'seq': rows[-1][0] if rows else since, 'changes': changes, 'editors': editors}


class LiveWorld:
    """The editor host's backend: the one world in DEV, edited live."""

    def __init__(self, connect=None):
        self._connect = connect or world_db.pooled('dev', 'editor')   # The host asks several times a second.
        self.roster = S.DbRoster(self.connect)

    def connect(self):
        try:
            return self._connect()
        except world_db.DatabaseError as error:
            raise S.Unavailable(f'The DEV database is not reachable. Start it with `python3 tools/world_db.py up`. ({error})') from error

    def load(self, lean=False):
        with self.connect() as conn:
            project, seq = load(conn, lean)
        return {'project': project, 'seq': seq}

    def ground(self, ids):
        with self.connect() as conn:
            cells, seq = ground(conn, ids)
        return {'cells': cells, 'seq': seq}

    def fill(self, project):
        with self.connect() as conn:
            return fill_ground(conn, project)

    def edit(self, data):
        with self.connect() as conn:
            seq = apply_edit(conn, str(data.get('clientId', ''))[:64], str(data.get('editor', '')).strip()[:60] or 'Someone',
                             str(data.get('label', '')), data.get('ops'))
        return {'seq': seq}

    def sync(self, data):
        since = data.get('since')
        if not isinstance(since, int) or since < 0:
            raise ValueError('since must be an edit number.')
        color = data.get('color') if isinstance(data.get('color'), str) else '#a8c7ad'
        state = data.get('state') if isinstance(data.get('state'), dict) else {}
        with self.connect() as conn:
            return sync(conn, str(data.get('clientId', ''))[:64], str(data.get('editor', '')).strip()[:60] or 'Someone',
                        color if len(color) == 7 else '#a8c7ad', since, state,
                        data.get('world') if isinstance(data.get('world'), str) else None)
