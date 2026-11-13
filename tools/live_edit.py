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
RELOAD = 'reload'          # An edit-log key: everything may have changed (tools/world_sync.py pulled or imported).
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

    def __init__(self, conn, world, lock=False):
        self.conn, self.w, self.world = conn, world['id'], world
        # (An edit's NPCs and slots are read FOR UPDATE: the game server writes them too, and its write waits for the
        # edit's, never lost under it. Doc 66, Progress, "The live world's truth".)
        self.lock = lock
        self.size = world['chunk_size']
        self.chunks = {}      # (cx, cy) -> {'glyphs': list, 'heights': dict, 'dirty': bool}
        self.rooms = {}       # id -> {'meta': dict | None, 'tiles': {(x, y): glyph}, 'heights': {(x, y): h}, 'dirty': bool}
        self.pending = {}     # entity key -> new value (None deletes)
        self.head = {}        # name/spawn/bounds changes
        self._tables = None

    @property
    def tables(self):
        """The world's tables (world_store.tables_for): a slim world's NPCs carry their band (doc 79, 14)."""
        if self._tables is None:
            self._tables = S.tables_for(self.conn)
        return self._tables

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
            locked = ' FOR UPDATE' if self.lock and table in S.GAME_TABLE_OF.values() else ''
            found = self.rows(f'SELECT * FROM {table} WHERE world_id = %s AND id = %s{locked}', rest)
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
            return S.economy_value(self.conn, self.w)    # (The kept settings and the host's overrides: doc 79, 14.)
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

    def upsert(self, table, row, tables=None):
        keys, columns = (tables or S.TABLES)[table]
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
            elif kind == 'person':
                # (An NPC's authored band on a slim world: doc 79, 14; the Band picker.)
                slim = self.tables is S.SLIM_TABLES
                if refused := S.bands_refused(self.tables, [value]):
                    raise ValueError(refused)     # (Refused, never dropped unsaid, before 0061.)
                self.upsert('live.npcs', S.npc_row(w, value, self.position('live.npcs', id_), band=slim), self.tables)
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
            # (Held before its rows are made: a room a thousand million tiles a side would be built here, in the
            # editor's process, holding the world's row meanwhile: doc 78's R2.)
            if not all(type(v) is int and 1 <= v <= 4096 for v in (width, height)):
                raise Rejected(f'An interior is 1 to 4096 tiles a side ({rid}).', {})
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
            # The economy's settings (doc 79, 14): checked as a save checks them (map_editor.check_economy), the kept
            # columns and the host's overrides written (world_store.save_economy); an override this world's migrations
            # can't hold is refused (EconomyRefused: "This world needs migration 0061.").
            if value is None:
                raise ValueError('The economy cannot be removed.')
            # (Only what this edit brings is refused: a problem the stored settings already have, which Core reads past,
            # never blocks another change. Doc 79, Phase 9's first repair. The host's overrides held to the studio's
            # strict rules too, Phase 9's second repair: a new bad or oversized one, which Core would skip whole, cause
            # and all, is refused here as the studio refuses it; one already stored is not.)
            stored = S.economy_value(self.conn, w)
            had = set(map_editor.check_economy(stored, strict_overrides=True)) if stored is not None else set()
            problems = [p for p in map_editor.check_economy(value, strict_overrides=True) if p not in had]
            if problems:
                raise ValueError(' '.join(problems))
            try:
                S.save_economy(self.conn, w, value)
            except S.StoreError as error:
                raise ValueError(f'The economy\'s settings can\'t be changed: {error}') from error
            self.economy_changed = True


def queue_economy_sync(conn, who, payload=None):
    """economy.sync for the world's running game server (doc 79, 12: it reads live.economy again and applies it at once);
    the action's id, or None if it couldn't be queued (no server to tell is no error: an unapplied action expires after
    ten minutes). Since migration 0061 as Atlas's own role, through dm.queue_economy_sync (Phase 9's second repair: Atlas
    logged in as the DM's role, through the registry key "dev", and on a registry without it, or a host with no DM
    password, the server was never told); before it as the DM's role on the same database."""
    try:
        if conn.execute("SELECT to_regprocedure('dm.queue_economy_sync(text, jsonb)') IS NOT NULL").fetchone()[0]:
            return conn.execute('SELECT dm.queue_economy_sync(%s, %s)', (str(who)[:60] or 'Atlas', S.Jsonb(payload or {}))).fetchone()[0]
    except psycopg.Error:
        pass                                    # (A world before 0061: the DM's role, below.)
    try:
        names = world_db.keys()
        key = next((k for k in names if world_db.world(k)['database'] == conn.info.dbname), names[0] if names else 'dev')
        with world_db.connect(key, 'dm', dbname=conn.info.dbname, host=conn.info.host, port=conn.info.port) as dm:
            action = dm.execute('INSERT INTO dm.actions (kind, target_id, requested_by, payload) VALUES (%s, %s, %s, %s) RETURNING id',
                                ('economy.sync', 'economy', str(who)[:60] or 'Atlas', S.Jsonb(payload or {}))).fetchone()[0]
            dm.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action}),))
            return action
    except (world_db.DatabaseError, psycopg.Error, KeyError):
        return None


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


# --------------------------------------------------------------------------- The game's changes
# Doc 66, Progress, "The live world's truth": on a world a game server runs, the game changes NPCs (an arrival's home)
# without the edit log, so an open Atlas holds the value it replaced. live.game_changes (migration 0059) says which rows
# and fields the game changed, and to what.

def games_change(conn, world_id, key, now):
    """Which game-owned fields of `key`'s entity (person: or slot:) are as the game left them, and the values the game
    replaced: ({field: [values it replaced, oldest first]}, the newest logged row), or ({}, None) when the game never
    changed it (or the world doesn't log the game's changes)."""
    kind, _, rid = key.partition(':')
    if kind not in S.GAME_FIELDS or now is None:
        return {}, None
    if not S.game_log_supported(conn):
        return {}, None
    rows = conn.execute('SELECT before, after FROM live.game_changes WHERE world_id = %s AND table_name = %s AND row_key = %s '
                        'ORDER BY seq', (world_id, S.GAME_TABLE_OF[kind], S.Jsonb([rid]))).fetchall()
    if not rows:
        return {}, None
    last = rows[-1][1]
    replaced = {}
    for field in S.GAME_FIELDS[kind]:
        if last is None or canonical(S.entity_value(field, last)) != canonical(now.get(field)):
            continue                   # (Not as the game left it: someone changed it since, or the game never did.)
        olds = [S.entity_value(field, before) for before, _ in rows if before is not None]
        olds = [v for v in olds if canonical(v) != canonical(now.get(field))]
        if olds:
            replaced[field] = olds
    return replaced, last


def merge_games_change(conn, world_id, key, before, now, after):
    """An edit made on a view older than the game's change (its `before` isn't the row now): ('merged', the row to
    write: the edit's own changes on the game's row) when the two differ only in game-owned fields the game changed;
    ('conflict', [{field, base, mine, game}]) when the edit changed one of those fields too, differently; None when
    it isn't the game's change (another editor's: refused as ever)."""
    if not (isinstance(before, dict) and isinstance(now, dict) and isinstance(after, dict)):
        return None
    differs = [f for f in sorted(set(before) | set(now)) if canonical(before.get(f)) != canonical(now.get(f))]
    replaced, _ = games_change(conn, world_id, key, now)
    if not differs or any(f not in replaced for f in differs):
        return None
    mine = [f for f in sorted(set(before) | set(after)) if canonical(before.get(f)) != canonical(after.get(f))]
    both = [f for f in differs if f in mine and canonical(after.get(f)) != canonical(now.get(f))]
    if both:
        return 'conflict', [{'field': f, 'base': before.get(f), 'mine': after.get(f), 'game': now.get(f)} for f in both]
    return 'merged', {**now, **{f: after.get(f) for f in mine if f in after}}


def world_row(conn, lock=False):
    rows = S.dict_rows(conn, 'SELECT * FROM world.worlds' + (' FOR UPDATE' if lock else ''), ())
    if not rows:
        raise S.StoreError('There is no world in the database yet. Run `python3 tools/world_store.py import`.')
    return rows[0]


def latest_seq(conn):
    return conn.execute('SELECT coalesce(max(seq), 0) FROM world.edits').fetchone()[0]


def load(conn, lean=False, with_game=False):
    """The whole world for an editor that is starting, and the edit it is current to. Lean, each world cell comes as
    its outline and a preview (S.load_world), and its ground is asked for as it comes into view (ground())."""
    with conn.transaction():
        conn.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')
        world = world_row(conn)
        project, _ = S.load_world(conn, world['id'], ground=not lean)
        # (With the newest game change the project holds, read in the same snapshot: the heartbeat brings the rest.)
        return (project, latest_seq(conn), S.game_seen(conn)) if with_game else (project, latest_seq(conn))


def camps(conn):
    """What the players' Chapters have built (Docs/Design/32), as the DEV game server last saved it, for Atlas to draw
    read-only: [{cell, x, y, kind, built, condition, site, chapter, colour}], cell-local tiles. Empty before
    migration 0030 or with no world."""
    if not conn.execute("SELECT to_regclass('game.camp_structures') IS NOT NULL").fetchone()[0]:
        return []
    rows = conn.execute("""
        SELECT s.data->>'cell', (b.data->>'x')::integer, (b.data->>'y')::integer, b.data->>'kind',
               coalesce((b.data->>'built')::boolean, false), coalesce((b.data->>'condition')::real, 100),
               coalesce(nullif(s.data->>'name', ''), 'Camp'), coalesce(c.data->>'name', s.chapter), coalesce(c.data->>'colour', '#92bacd')
        FROM game.camp_structures b
        JOIN game.camp_sites s ON s.world_id = b.world_id AND s.key = b.site
        LEFT JOIN game.chapters c ON c.world_id = s.world_id AND c.key = s.chapter
        WHERE b.world_id = (SELECT id FROM world.worlds LIMIT 1)
        ORDER BY s.position, b.position""").fetchall()
    return [{'cell': r[0], 'x': r[1], 'y': r[2], 'kind': r[3], 'built': r[4], 'condition': r[5], 'site': r[6], 'chapter': r[7], 'colour': r[8]}
            for r in rows]


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


def apply_edit(conn, client_id, editor, label, ops, merged=None, queue_sync=True, synced=None):
    """Applies one batch. Returns the new seq; raises Conflict or Rejected with the current values.

    An `economy` change queues economy.sync for the running server after it commits (`queue_sync`; the studio passes
    False, queuing its own: doc 79, Phase 9's U9); `synced` (a dict, if given) gets the action's id under 'sync'.

    An NPC or slot the game server changed since the editor saw it (doc 66, Progress, "The live world's truth") is not
    refused for that: the edit's own changes are made on the game's row, and `merged` (a dict, if given) gets the row
    written for each such key. Where the edit changed a field the game changed too, the batch is refused with those
    in `conflicts` (`game: true`, each field's base, mine and game's): the game's stays until the editor chooses."""
    if not isinstance(ops, list) or not ops or len(ops) > MAX_OPS:
        raise ValueError('An edit needs 1 to 250000 changes.')
    keys = [op.get('key') if isinstance(op, dict) else None for op in ops]
    if not all(isinstance(k, str) and 0 < len(k) <= 160 for k in keys) or len(set(keys)) != len(keys):
        raise ValueError('Every change needs its own key.')
    current = {}
    try:
        with conn.transaction():
            batch = Batch(conn, world_row(conn, lock=True), lock=True)
            conflicts, games = [], {}
            for op in ops:
                now = current[op['key']] = batch.current(op['key'])
                if canonical(now) != canonical(op.get('before')):
                    found = merge_games_change(conn, batch.w, op['key'], op.get('before'), now, op.get('after'))
                    if found and found[0] == 'merged':
                        games[op['key']] = found[1]
                        continue
                    if found:
                        conflicts.append({'key': op['key'], 'label': label_for(op['key'], now), 'editor': 'The game',
                                          'at': None, 'game': True, 'fields': found[1]})
                        continue
                    last = conn.execute('SELECT editor, at FROM world.edits WHERE world_id = %s AND key = %s ORDER BY seq DESC LIMIT 1',
                                        (batch.w, op['key'])).fetchone()
                    conflicts.append({'key': op['key'], 'label': label_for(op['key'], now or op.get('before')),
                                      'editor': last[0] if last else '', 'at': last[1].isoformat() if last else None})
            if conflicts:
                who = sorted({c['editor'] for c in conflicts if c['editor']})
                raise Conflict(f'Changed by {", ".join(who) or "someone else"} since you last saw it.', conflicts, current)
            ops = [{**op, 'before': current[op['key']], 'after': games[op['key']]} if op['key'] in games else op for op in ops]
            if merged is not None:
                merged.update(games)
            for op in ops:
                batch.set(op['key'], op.get('after'))
            batch.flush()
            conn.execute('UPDATE world.worlds SET revision = revision + 1, updated_at = now() WHERE id = %s', (batch.w,))
            cur = conn.cursor()
            cur.executemany('''INSERT INTO world.edits (world_id, batch, editor, client_id, label, key, before, after)
                               VALUES (%s, txid_current(), %s, %s, %s, %s, %s, %s)''',
                            [(batch.w, editor, client_id, label[:200], op['key'], S.Jsonb(op.get('before')), S.Jsonb(op.get('after')))
                             for op in ops])
            seq = latest_seq(conn)
        if queue_sync and getattr(batch, 'economy_changed', False):
            action = queue_economy_sync(conn, editor)
            if synced is not None:
                synced['sync'] = action
        return seq
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


def game_feed(conn, world, game_since):
    """The NPCs and slots the game server changed after game change `game_since` (live.game_changes), as edits of their
    keys with their values now (the editor applies them as anyone's): ([change], the newest game change). Each carries
    `seq`, the newest edit its value holds (read in the caller's snapshot), so an editor's own later edit of the key
    is newer than it and an earlier one older."""
    if not isinstance(game_since, int) or not S.game_log_supported(conn):
        return [], game_since
    rows = conn.execute('''SELECT max(seq), table_name, row_key->>0, (array_agg(op ORDER BY seq))[1] FROM live.game_changes
                           WHERE world_id = %s AND seq > %s AND table_name IN ('live.npcs', 'live.profession_slots')
                           AND jsonb_array_length(row_key) = 1 GROUP BY table_name, row_key ORDER BY 1 LIMIT %s''',
                        (world['id'], game_since, MAX_CHANGES)).fetchall()
    if not rows:
        return [], game_since
    batch = Batch(conn, world)
    kinds = {table: kind for kind, table in S.GAME_TABLE_OF.items()}
    seq = latest_seq(conn)
    changes = []
    for _, table, rid, op in rows:
        key = f'{kinds[table]}:{rid}'
        after = batch.current(key)
        what = 'born' if op == 'insert' else 'gone' if after is None else 'placed' if kinds[table] == 'person' else 'changed'
        changes.append({'seq': seq, 'clientId': 'game', 'editor': 'The game', 'label': f'{label_for(key, after)} {what} by the game',
                        'key': key, 'after': after})
    return changes, max(r[0] for r in rows)


def sync(conn, client_id, editor, color, since, state, world=None, game_since=None):
    """Heartbeat: records where this editor is, returns everyone else's edits after `since` and who is online.

    `world` is the world the editor holds; if DEV now holds a different one (it was replaced), the editor reloads.
    `game_since`: the game server's changes after that one come too (doc 66, Progress, "The live world's truth"), as
    edits of their keys, and `gameSeq` says the newest.
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
    # A pull or import (tools/world_sync.py) rewrites rows the keyed edits don't describe, and logs RELOAD: reload.
    if len(rows) > MAX_CHANGES or any(r[4] == RELOAD for r in rows):
        return {'reload': True, 'seq': rows[-1][0], 'changes': [], 'editors': editors}
    changes = [{'seq': r[0], 'clientId': r[1], 'editor': r[2], 'label': r[3], 'key': r[4], 'after': r[5]} for r in rows]
    seq = rows[-1][0] if rows else since
    with conn.transaction():
        # (One snapshot: the game's rows as they are and the newest edit they hold, its seq; edits are numbered as they
        # commit, under the world row's lock.)
        conn.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')
        made, game_seq = game_feed(conn, world_row(conn), game_since)
    changes += made
    return {'seq': seq, 'changes': changes, 'editors': editors, **({'gameSeq': game_seq} if isinstance(game_seq, int) else {})}


class LiveWorld:
    """The editor host's backend: the one world in a dev world's database (Atlas's, tools/world_db.py), edited live."""

    def __init__(self, connect=None, key=None):
        self.key = key or ('dev' if connect else world_db.atlas_world())
        self._connect = connect or world_db.pooled(self.key, 'editor')   # The host asks several times a second.
        self.roster = S.DbRoster(self.connect)

    def connect(self):
        try:
            return self._connect()
        except world_db.DatabaseError as error:
            raise S.Unavailable(f'The {self.key} database is not reachable. Start it with `python3 tools/world_db.py up`. ({error})') from error

    def load(self, lean=False):
        with self.connect() as conn:
            project, seq, game_seq = load(conn, lean, with_game=True)
            bands = S.slim_supported(conn)          # (Migration 0061: an NPC's own band; the Band picker greyed without it.)
        return {'project': project, 'seq': seq, 'gameSeq': game_seq, 'bands': bands}

    def ground(self, ids):
        with self.connect() as conn:
            cells, seq = ground(conn, ids)
        return {'cells': cells, 'seq': seq}

    def camps(self):
        with self.connect() as conn:
            return {'buildings': camps(conn)}

    def fill(self, project):
        with self.connect() as conn:
            return fill_ground(conn, project)

    def edit(self, data):
        merged, synced = {}, {}
        with self.connect() as conn:
            seq = apply_edit(conn, str(data.get('clientId', ''))[:64], str(data.get('editor', '')).strip()[:60] or 'Someone',
                             str(data.get('label', '')), data.get('ops'), merged, synced=synced)
        # (Rows written as the edit's changes on the game's: the editor shows them as written.)
        return {'seq': seq, **({'merged': merged} if merged else {}), **synced}

    def sync(self, data):
        since = data.get('since')
        if not isinstance(since, int) or since < 0:
            raise ValueError('since must be an edit number.')
        color = data.get('color') if isinstance(data.get('color'), str) else '#a8c7ad'
        state = data.get('state') if isinstance(data.get('state'), dict) else {}
        with self.connect() as conn:
            return sync(conn, str(data.get('clientId', ''))[:64], str(data.get('editor', '')).strip()[:60] or 'Someone',
                        color if len(color) == 7 else '#a8c7ad', since, state,
                        data.get('world') if isinstance(data.get('world'), str) else None,
                        data.get('gameSince') if isinstance(data.get('gameSince'), int) and not isinstance(data.get('gameSince'), bool) else None)
