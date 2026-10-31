#!/usr/bin/env python3
"""Pull and import, with conflicts: a dev world and the live world it is a working copy of (its upstream).
Docs/Design/66-worlds-atlas-and-dm.md, section 4.

  python3 tools/world_sync.py status KEY [--state] [--json]   a world's server state, base and release
  python3 tools/world_sync.py diff DEV [--json]               the three-way plan: live-only, dev-only, conflicts
  python3 tools/world_sync.py pull DEV [--choices FILE]       Atlas's Update from live: live's changes into dev
  python3 tools/world_sync.py plan-import DEV [--json]        what an import would do (and what blocks it)
  python3 tools/world_sync.py import DEV [--choices FILE] --note "..."
                                                              the DM's Import from dev (asks for the publish password)
  python3 tools/world_sync.py init-base DEV [--prefer dev|live]
                                                              take a dev world's first base
  python3 tools/world_sync.py maintenance KEY on|off [--note "..."]
                                                              close a world's game server for maintenance, or reopen it

The base. A dev world with an upstream keeps the snapshot of its upstream as of its last pull or import
(admin.sync_base and admin.sync_base_rows in the dev world, migration 0054): every synced row by (table, key). Each
key is compared three ways: base B, dev D, live L.

    D = L                          nothing to do
    D = B, L != B                  live changed: take live's
    L = B, D != B                  dev changed: keep dev's
    both changed, differently      conflict (field by field): the user keeps dev's or live's, or chooses per field
    removed on one side, changed   conflict
    on the other

Choices are a JSON object {conflict id: "dev" | "live" | {field: "dev" | "live", ...}}; a conflict id is the table
and the row's key, e.g. "live.npcs/wren" or "world.terrain_chunks/0/1" ('/' and '%' in a key are %-escaped).

Pull (Atlas): applies every live-only change and the chosen side of every conflict to the dev world, in one
transaction, and makes live's snapshot the new base. Open Atlas sessions reload (a "reload" entry in the edit log).

Import (the DM): only while the live world is in maintenance and its game server has stopped. Compares again, applies
every dev-only change and the chosen side of every conflict to live in one transaction (references checked, players'
data never written), records a release (number, who, note, summary, the authored content for rollback) with its game
build and NOTIFY ratw_release, then brings the dev world level with live (for the rows nobody edited in dev meanwhile)
and makes that the new base.

Synced: the world row (without revision and updated_at), cells, interiors (with world.areas following them), terrain
chunks, links, places, resources, chapters; NPCs (not those a spawn rule made), profession slots, professions, roster
characters, patrol routes with their posts (one unit), NPC areas, spawn rules, factions, their claims, members and
relations, NPC voices, families (npc_relations), dialects, figures, story visitors. Never read or written: game.*
(players, the save, the event log, memories), live.economy, live.npc_state, dm.*. Timestamps (updated_at) and the
world's revision are not compared.

A world's server state (admin.server_state): running or maintenance. The game server hears a change (NOTIFY
ratw_maintenance), tells its players, gives them a minute, saves and exits with status 76; it refuses to start while
its world is in maintenance (also 76); tools/live.sh waits and starts it again (on the live choice) once it is running.
"""
from __future__ import annotations

import argparse
import base64
import datetime
import getpass
import hashlib
import json
import sys
from pathlib import Path
from typing import NamedTuple

import psycopg

import map_editor
import world_build
import world_db
import world_store as S

# --------------------------------------------------------------------------- Releases (from Push to live)

AUTHORED = ['world.worlds', 'world.areas', 'world.terrain_chunks', 'world.cells', 'world.interiors', 'world.links',
            'world.resources', 'world.places', 'world.chapters', 'world.minimaps']
LABELS = {'world.worlds': 'world settings', 'world.areas': None, 'world.terrain_chunks': 'terrain blocks',
          'world.cells': 'cells', 'world.interiors': 'interiors', 'world.links': 'connections', 'world.resources': 'resources',
          'world.places': 'places', 'world.chapters': 'Chapters', 'world.minimaps': None}
MAX_FAILURES, LOCK_MINUTES = 5, 15
# Bookkeeping that is not content: a dev world's edit counter moves on every edit, timestamps on every write.
NOT_CONTENT = ('updated_at', 'revision')
NOTIFY_CHANNEL = 'ratw_release'
MAINTENANCE_CHANNEL = 'ratw_maintenance'
MAINTENANCE_EXIT = 76                # The game server's exit status when its world closes for maintenance.
PUBLISH_LOCK = 0x52415457            # Advisory lock key ("RATW") held by an import, a rollback or a pull.
OWNER_LOCK = (1380013143, 1)         # The game server's own lock on its world (Core/RatwGameOwner.cpp).
STATES = ('running', 'maintenance')


class PublishError(S.StoreError):
    status = 422

    def __init__(self, message, errors=None):
        super().__init__(message)
        self.errors = errors or []


SyncError = PublishError


class WrongPassword(PublishError):
    status = 401


class Locked(PublishError):
    status = 429


class Unresolved(PublishError):
    """Conflicts without a choice: `conflicts` are their descriptions, as the plan lists them."""
    status = 409

    def __init__(self, message, conflicts):
        super().__init__(message, [c['id'] for c in conflicts])
        self.conflicts = conflicts


class NotInMaintenance(PublishError):
    status = 409


_shapes = {}


def shape(conn, table):
    """(columns, key columns, {column: type}) of a table, from the catalog."""
    if table not in _shapes:
        schema, name = table.split('.')
        cols = conn.execute('''SELECT column_name, data_type FROM information_schema.columns
                               WHERE table_schema = %s AND table_name = %s ORDER BY ordinal_position''', (schema, name)).fetchall()
        if not cols:
            return [], [], {}                  # (A table a world without the latest migrations lacks: not cached.)
        keys = [r[0] for r in conn.execute('''SELECT a.attname FROM pg_index i
            JOIN pg_attribute a ON a.attrelid = i.indrelid AND a.attnum = ANY(i.indkey)
            WHERE i.indrelid = %s::regclass AND i.indisprimary ORDER BY array_position(i.indkey, a.attnum)''', (table,)).fetchall()]
        _shapes[table] = ([c for c, _ in cols], keys, dict(cols))
    return _shapes[table]


def world_column(table):
    if table in GLOBAL_TABLES:
        return None
    return 'id' if table == 'world.worlds' else 'world_id'


def read(conn, table, world_id, skip=(), where=''):
    """{key: row dict} of one world's rows; `skip` columns are left out (e.g. timestamps)."""
    columns, keys, _ = shape(conn, table)
    columns = [c for c in columns if c not in skip]
    column = world_column(table)
    sql = f'SELECT {", ".join(columns)} FROM {table}'
    params = ()
    if column:
        sql, params = sql + f' WHERE {column} = %s', (world_id,)
    if where:
        sql += (' AND ' if column else ' WHERE ') + where
    cur = conn.execute(sql, params)
    return {tuple(r[columns.index(k)] for k in keys): dict(zip(columns, r)) for r in cur.fetchall()}


def write(conn, table, row, touch=False):
    _, keys, types = shape(conn, table)
    columns = list(row)
    values = [S.Jsonb(row[c]) if types[c] == 'jsonb' and row[c] is not None else row[c] for c in columns]
    rest = [f'{c} = excluded.{c}' for c in columns if c not in keys]
    if touch and 'updated_at' in types and 'updated_at' not in row:
        rest.append('updated_at = now()')
    conn.execute(f'INSERT INTO {table} ({", ".join(columns)}) VALUES ({", ".join(["%s"] * len(columns))}) '
                 f'ON CONFLICT ({", ".join(keys)}) DO ' + (f'UPDATE SET {", ".join(rest)}' if rest else 'NOTHING'),
                 values)


def delete(conn, table, key):
    _, keys, _ = shape(conn, table)
    conn.execute(f'DELETE FROM {table} WHERE ' + ' AND '.join(f'{k} = %s' for k in keys), key)


# Release content and the base are stored as JSON: bytes and timestamps need a round trip.
def to_json(row):
    out = {}
    for k, v in row.items():
        if isinstance(v, (bytes, memoryview)):
            v = {'$bytes': base64.b64encode(bytes(v)).decode()}
        elif isinstance(v, datetime.datetime):
            v = {'$time': v.isoformat()}
        out[k] = v
    return out


def from_json(row):
    out = {}
    for k, v in row.items():
        if isinstance(v, dict) and set(v) == {'$bytes'}:
            v = base64.b64decode(v['$bytes'])
        elif isinstance(v, dict) and set(v) == {'$time'}:
            v = datetime.datetime.fromisoformat(v['$time'])
        out[k] = v
    return out


def the_world(conn, label):
    rows = conn.execute('SELECT id FROM world.worlds').fetchall()
    if len(rows) > 1:
        raise PublishError(f'{label} holds more than one world.')
    return rows[0][0] if rows else None


def plan_world(prod, content, world_id):
    """Deletes and upserts that make a live world's authored content equal `content` ({table: {key: row}}): rollback."""
    plan, summary = {}, {}
    for table in AUTHORED:
        have = read(prod, table, world_id, skip=NOT_CONTENT)
        want = content[table]
        stale = [k for k in have if k not in want]
        fresh = [row for k, row in want.items() if have.get(k) != row]
        added = sum(1 for k in want if k not in have)
        plan[table] = (stale, fresh)
        if LABELS[table]:
            summary[LABELS[table]] = {'added': added, 'changed': len(fresh) - added, 'removed': len(stale)}
    return plan, summary


def apply_world(prod, plan):
    for table in reversed(AUTHORED):
        for key in plan[table][0]:
            delete(prod, table, key)
    for table in AUTHORED:
        for row in plan[table][1]:
            write(prod, table, row)


def dev_content(conn, world_id):
    """The authored content of a world, table by table: what a release stores for rollback."""
    return {table: read(conn, table, world_id, skip=NOT_CONTENT) for table in AUTHORED}


def validate_dev(dev):
    """Every problem that would block Play/Export blocks an import too."""
    world_id = the_world(dev, 'The dev world')
    if not world_id:
        raise PublishError('The dev world has no world to publish.')
    project, revision = S.load_world(dev, world_id)
    try:
        _, warnings = map_editor.check_project(project, for_game=False)     # The game server streams: no size limit.
        map_editor.export_files(project, S.load_roster(dev), stream=True)   # What the live server loads.
    except map_editor.ValidationError as error:
        return world_id, revision, error.errors, []
    return world_id, revision, [], warnings


def something(*summaries):
    """Whether a summary changes anything (things kept because the game changed them do not count)."""
    return any(n for part in summaries for counts in part.values() for k, n in counts.items() if not k.startswith('kept'))


def latest_release(prod, world_id):
    return prod.execute('SELECT coalesce(max(number), 0) FROM admin.releases WHERE world_id = %s', (world_id,)).fetchone()[0]


def check_password(prod, password, editor, action):
    """Verifies the publish password; after 5 failures in 15 minutes imports are locked for the rest of that window."""
    failures = prod.execute(f'''SELECT count(*), min(at) FROM admin.publish_attempts WHERE NOT ok
        AND at > now() - interval '{LOCK_MINUTES} minutes'
        AND at > coalesce((SELECT max(at) FROM admin.publish_attempts WHERE ok), '-infinity')''').fetchone()
    if failures[0] >= MAX_FAILURES:
        wait = LOCK_MINUTES - int((datetime.datetime.now(datetime.timezone.utc) - failures[1]).total_seconds() // 60)
        raise Locked(f'Too many wrong passwords. Publishing is locked for about {max(1, wait)} more minute{"s" if wait != 1 else ""}.')
    ok = isinstance(password, str) and world_db.verify_publish_password(prod, password)
    prod.execute('INSERT INTO admin.publish_attempts (editor, action, ok) VALUES (%s, %s, %s)', (editor[:60], action, ok))
    if not ok:
        left = MAX_FAILURES - failures[0] - 1
        raise WrongPassword('Wrong publish password.' + (f' {left} more tr{"ies" if left != 1 else "y"} before publishing locks.' if left < 3 else ''))


def orphans(prod, world_id):
    """Live data that would stand in a place that no longer exists."""
    found = []
    for table, what in (('live.npcs', 'NPC'), ('live.profession_slots', 'profession slot')):
        for id_, name, area in prod.execute(f'''SELECT t.id, t.name, a.area FROM {table} t,
                LATERAL (VALUES (t.home_area), (t.work_area), (t.evening_area)) AS a(area)
                WHERE t.world_id = %s AND a.area IS NOT NULL   -- (An unset place stands nowhere: doc 66, 2.)
                AND NOT EXISTS (SELECT 1 FROM world.areas w WHERE w.world_id = t.world_id AND w.id = a.area)''',
                                            (world_id,)).fetchall():
            found.append(f'{what} {name} ({id_}) is placed in {area}, which this removes.')
    for route, area in prod.execute('''SELECT DISTINCT p.route_id, p.area FROM live.patrol_posts p WHERE p.world_id = %s
            AND NOT EXISTS (SELECT 1 FROM world.areas w WHERE w.world_id = p.world_id AND w.id = p.area)''', (world_id,)).fetchall():
        found.append(f'Patrol route {route} has a post in {area}, which this removes.')
    for id_, name, area in prod.execute('''SELECT a.id, a.name, a.area FROM live.npc_areas a WHERE a.world_id = %s
            AND NOT EXISTS (SELECT 1 FROM world.areas w WHERE w.world_id = a.world_id AND w.id = a.area)''', (world_id,)).fetchall():
        found.append(f'NPC area {name} ({id_}) lies in {area}, which this removes.')
    for faction, area in prod.execute('''SELECT c.faction_id, c.area FROM live.faction_claims c WHERE c.world_id = %s
            AND NOT EXISTS (SELECT 1 FROM world.areas w WHERE w.world_id = c.world_id AND w.id = c.area)''', (world_id,)).fetchall():
        found.append(f'Faction {faction} claims {area}, which this removes.')
    return sorted(set(found))


def finish(prod, world_id, who='the live world'):
    """Checks everything that is only settled at commit, with messages an author can act on."""
    lost = orphans(prod, world_id)
    if lost:
        raise PublishError(f'Live data in {who} would be left in removed places. Move or remove it first.', lost)
    try:
        prod.execute('SET CONSTRAINTS ALL IMMEDIATE')
    except psycopg.errors.IntegrityError as error:
        raise PublishError(f'{who[0].upper() + who[1:]} refused the change: {error.diag.message_primary}'
                           + (f' ({error.diag.message_detail})' if error.diag.message_detail else '')) from error


def record(prod, world_id, editor, note, kind, content, summary, restored=None):
    """Records a release with its authored content, compiles its game build and tells the game server (at commit)."""
    number = latest_release(prod, world_id) + 1
    stored = {table: [to_json(r) for r in rows.values()] for table, rows in content.items()}
    prod.execute('''INSERT INTO admin.releases (world_id, number, pushed_by, note, kind, restored, content, summary)
                    VALUES (%s, %s, %s, %s, %s, %s, %s, %s)''',
                 (world_id, number, editor[:60], note[:2000], kind, restored, S.Jsonb(stored), S.Jsonb(summary)))
    # The game loads builds, not rows: compile this release for it in the same transaction.
    try:
        build_id, _ = world_build.build(prod, editor, release=number)
    except map_editor.ValidationError as error:
        raise PublishError('The world could not be compiled for the game.', error.errors) from error
    prod.execute('SELECT pg_notify(%s, %s)', (NOTIFY_CHANNEL, json.dumps({'world': world_id, 'release': number, 'kind': kind,
                                                                        'build': build_id})))
    return number


def rollback(prod, number, password, editor, note=''):
    """Puts release `number`'s world content back live, as a new release. Live data is left as it is."""
    check_password(prod, password, editor, 'rollback')
    with prod.transaction():
        world_id = the_world(prod, 'The live world')
        row = prod.execute('SELECT content FROM admin.releases WHERE world_id = %s AND number = %s', (world_id, number)).fetchone()
        if not row or row[0] is None:
            raise PublishError(f'Release {number} does not exist or has no stored world.')
        prod.execute('SELECT pg_advisory_xact_lock(%s)', (PUBLISH_LOCK,))
        content = {}
        for table in AUTHORED:
            _, keys, _ = shape(prod, table)
            rows = [from_json(r) for r in row[0].get(table, [])]
            content[table] = {tuple(r[k] for k in keys): r for r in rows}
        plan, summary = plan_world(prod, content, world_id)
        apply_world(prod, plan)
        finish(prod, world_id)
        new = record(prod, world_id, editor, note or f'Rolled back to release {number}.', 'rollback', content,
                     {'world': summary, 'live': {}}, restored=number)
    return {'release': new, 'summary': {'world': summary, 'live': {}}}


def releases(prod, limit=50):
    world_id = the_world(prod, 'The live world')
    rows = prod.execute('''SELECT number, pushed_at, pushed_by, note, kind, restored, summary FROM admin.releases
                           WHERE world_id = %s ORDER BY number DESC LIMIT %s''', (world_id, limit)).fetchall()
    return [{'number': r[0], 'at': r[1].isoformat(), 'by': r[2], 'note': r[3], 'kind': r[4], 'restored': r[5], 'summary': r[6]}
            for r in rows]


# --------------------------------------------------------------------------- What is synced

class Spec(NamedTuple):
    table: str
    label: str
    part: str                       # 'world' (authored content) or 'live' (live data)


SYNCED = [Spec(*s) for s in (
    ('world.worlds', 'world settings', 'world'),
    ('world.cells', 'cells', 'world'),
    ('world.interiors', 'interiors', 'world'),
    ('world.terrain_chunks', 'terrain blocks', 'world'),
    ('world.links', 'connections', 'world'),
    ('world.resources', 'resources', 'world'),
    ('world.places', 'places', 'world'),
    ('world.chapters', 'Chapters', 'world'),
    ('live.professions', 'professions', 'live'),
    ('live.characters', 'roster characters', 'live'),
    ('live.patrol_routes', 'patrol routes', 'live'),          # With its posts, one unit (the row's "posts").
    ('live.npc_areas', 'NPC areas', 'live'),
    ('live.spawns', 'spawn rules', 'live'),
    ('live.factions', 'factions', 'live'),
    ('live.faction_claims', 'territory claims', 'live'),
    ('live.npcs', 'NPCs', 'live'),
    ('live.profession_slots', 'profession slots', 'live'),
    ('live.faction_members', 'faction members', 'live'),
    ('live.faction_relations', 'faction relations', 'live'),
    ('live.npc_voice', 'NPC voices', 'live'),
    ('live.npc_relations', 'families', 'live'),
    ('live.dialects', 'dialects', 'live'),
    ('live.figures', 'figures', 'live'),
    ('live.story_visitors', 'story visitors', 'live'),
)]
SPECS = {s.table: s for s in SYNCED}
GLOBAL_TABLES = ('live.professions', 'live.characters')    # (One world a database: these have no world column.)
VOLATILE = ('updated_at', 'revision')
# NPCs a spawn rule made belong to the server running them; each world's rules make their own.
SPAWNED = '(SELECT id FROM live.npcs WHERE spawn_id IS NOT NULL)'
ONLY = {'live.npcs': 'spawn_id IS NULL', 'live.npc_voice': f'npc_id NOT IN {SPAWNED}',
        'live.faction_members': f'npc_id NOT IN {SPAWNED}',
        'live.npc_relations': f'npc_id NOT IN {SPAWNED} AND other_id NOT IN {SPAWNED}'}
AREAS_OF = {'world.cells': 'cell', 'world.interiors': 'interior'}


def canonical(row) -> str:
    return json.dumps(row, sort_keys=True, separators=(',', ':'), ensure_ascii=False)


def digest_of(row) -> str:
    return hashlib.sha1(canonical(row).encode('utf-8')).hexdigest()


def normalized(row: dict) -> dict:
    """A row as JSON values (bytes and timestamps tagged), exactly as it reads back from the stored base."""
    return json.loads(json.dumps(to_json(row)))


def row_key(table, row, keys) -> tuple:
    column = world_column(table)
    return tuple(row[k] for k in keys if k != column)


def item_id(table, key) -> str:
    esc = lambda v: str(v).replace('%', '%25').replace('/', '%2F')
    return '/'.join([table] + [esc(v) for v in key])


class Snapshot(dict):
    """{table: {key: (digest, row)}}: every synced row of one world, as JSON values, without volatile columns."""

    def row(self, table, key):
        found = self.get(table, {}).get(key)
        return found[1] if found else None

    def digest(self, table, key):
        found = self.get(table, {}).get(key)
        return found[0] if found else None

    def count(self):
        return sum(len(rows) for rows in self.values())


def snapshot(conn, world_id) -> Snapshot:
    """One world's synced content (read it inside one transaction for a consistent copy)."""
    snap = Snapshot()
    for spec in SYNCED:
        table = spec.table
        columns, keys, _ = shape(conn, table)
        rows = {}
        if columns and (world_id is not None or table in GLOBAL_TABLES):
            found = read(conn, table, world_id, skip=VOLATILE, where=ONLY.get(table, ''))
            if table == 'live.patrol_routes':
                posts = {}
                for route, seq, area, x, y in conn.execute('SELECT route_id, seq, area, x, y FROM live.patrol_posts '
                                                           'WHERE world_id = %s ORDER BY route_id, seq', (world_id,)).fetchall():
                    posts.setdefault(route, []).append({'seq': seq, 'area': area, 'x': x, 'y': y})
                for row in found.values():
                    row['posts'] = posts.get(row['id'], [])
            for row in found.values():
                row = normalized(row)
                rows[row_key(table, row, keys)] = (digest_of(row), row)
        snap[table] = rows
    return snap


# --------------------------------------------------------------------------- The base

def load_base(conn):
    """(header, Snapshot) of a dev world's base, or (None, None) if it has none."""
    if not shape(conn, 'admin.sync_base')[0]:
        raise PublishError('This world has no sync tables yet: python3 tools/world_db.py migrate (migration 0054).')
    row = conn.execute('SELECT upstream, world_id, release, taken_at, taken_by, how, rows FROM admin.sync_base').fetchone()
    if not row:
        return None, None
    header = {'upstream': row[0], 'world': row[1], 'release': row[2], 'takenAt': row[3].isoformat(), 'by': row[4],
              'how': row[5], 'rows': row[6]}
    snap = Snapshot({spec.table: {} for spec in SYNCED})
    for table, key, digest, data in conn.execute('SELECT table_name, key, digest, row FROM admin.sync_base_rows').fetchall():
        snap.setdefault(table, {})[tuple(key)] = (digest, data)
    return header, snap


def save_base(conn, upstream, world_id, release, snap: Snapshot, by='', how='pull'):
    """Replaces a world's base (inside the caller's transaction, or its own)."""
    with conn.transaction():
        conn.execute('DELETE FROM admin.sync_base_rows')
        conn.execute('''INSERT INTO admin.sync_base (upstream, world_id, release, taken_at, taken_by, how, rows)
                        VALUES (%s, %s, %s, now(), %s, %s, %s)
                        ON CONFLICT (singleton) DO UPDATE SET upstream = excluded.upstream, world_id = excluded.world_id,
                        release = excluded.release, taken_at = now(), taken_by = excluded.taken_by, how = excluded.how,
                        rows = excluded.rows''', (upstream, world_id, release, by[:60], how, snap.count()))
        conn.cursor().executemany('INSERT INTO admin.sync_base_rows (table_name, key, digest, row) VALUES (%s, %s, %s, %s)',
                                  [(table, S.Jsonb(list(key)), digest, S.Jsonb(row))
                                   for table, rows in snap.items() for key, (digest, row) in rows.items()])


def release_of(conn, world_id):
    if world_id is None or not shape(conn, 'admin.releases')[0]:
        return 0
    return latest_release(conn, world_id)


# --------------------------------------------------------------------------- Three-way

class Item(NamedTuple):
    spec: Spec
    key: tuple
    side: str                       # 'live', 'dev' or 'conflict'
    base: dict | None
    dev: dict | None
    live: dict | None

    @property
    def id(self):
        return item_id(self.spec.table, self.key)


def three_way(base: Snapshot, dev: Snapshot, live: Snapshot) -> list[Item]:
    """Every key whose dev and live rows differ, with the side that changed it (or a conflict), in SYNCED order."""
    items = []
    for spec in SYNCED:
        b, d, l = base.get(spec.table, {}), dev.get(spec.table, {}), live.get(spec.table, {})
        for key in sorted(set(b) | set(d) | set(l), key=lambda k: tuple(str(v) for v in k)):
            B, D, L = (s.get(key, (None, None))[0] for s in (b, d, l))
            if D == L:
                continue
            side = 'live' if D == B else 'dev' if L == B else 'conflict'
            items.append(Item(spec, key, side, base.row(spec.table, key), dev.row(spec.table, key), live.row(spec.table, key)))
    return items


def change(before, after):
    if before is None:
        return 'absent' if after is None else 'added'
    if after is None:
        return 'removed'
    return 'unchanged' if canonical(before) == canonical(after) else 'changed'


LONG = 300


def shown(value):
    """A value for the screens: long ones (terrain glyphs, heights) as their size and a digest."""
    text = canonical(value)
    if len(text) <= LONG:
        return value
    return {'$long': len(text), 'digest': hashlib.sha1(text.encode('utf-8')).hexdigest()[:12]}


def differing(a, b, column=None):
    """The fields in which two rows (either may be None) differ, the world column left out."""
    a, b = a or {}, b or {}
    return [f for f in sorted(set(a) | set(b)) if f != column and canonical(a.get(f)) != canonical(b.get(f))]


def field_side(item: Item, field) -> str:
    """Which side changed one field of a row both sides have: 'same', 'dev', 'live' or 'conflict' (both, differently,
    or the row is new on both)."""
    d, l = canonical((item.dev or {}).get(field)), canonical((item.live or {}).get(field))
    if d == l:
        return 'same'
    if item.base is None:
        return 'conflict'
    b = canonical(item.base.get(field))
    return 'live' if d == b else 'dev' if l == b else 'conflict'


def describe(item: Item) -> dict:
    """One row of the plan, for the screens and the command line."""
    table, column = item.spec.table, world_column(item.spec.table)
    fields = sorted(set(differing(item.base, item.dev, column)) | set(differing(item.base, item.live, column))
                    | set(differing(item.dev, item.live, column)))
    any_row = item.dev or item.live or item.base or {}
    name = any_row.get('name') if isinstance(any_row.get('name'), str) else None
    return {'id': item.id, 'table': table, 'label': item.spec.label, 'part': item.spec.part, 'key': list(item.key),
            'name': name or '/'.join(str(k) for k in item.key), 'side': item.side,
            'dev': change(item.base, item.dev), 'live': change(item.base, item.live),
            'fields': [{'field': f, 'base': shown((item.base or {}).get(f)) if item.base else None,
                        'dev': shown((item.dev or {}).get(f)) if item.dev else None,
                        'live': shown((item.live or {}).get(f)) if item.live else None,
                        # (Rows on both sides: which side changed the field, 'same' or 'conflict'; else null.)
                        'changed': field_side(item, f) if item.dev is not None and item.live is not None else None}
                       for f in fields],
            'fieldChoices': item.side == 'conflict' and item.dev is not None and item.live is not None}


def summarize(items, side_of):
    """{part: {label: {added, changed, removed}}} of the rows that change on one side."""
    out = {'world': {}, 'live': {}}
    for item, before, after in side_of(items):
        what = change(before, after)
        if what in ('added', 'changed', 'removed'):
            counts = out[item.spec.part].setdefault(item.spec.label, {'added': 0, 'changed': 0, 'removed': 0})
            counts[what] += 1
    return out


def plan_digest(items) -> str:
    h = hashlib.sha1()
    for item in items:
        h.update(f'{item.id}|{item.side}|{digest_of(item.base)}|{digest_of(item.dev)}|{digest_of(item.live)}\n'.encode())
    return h.hexdigest()


def resolve(items, choices) -> dict[str, dict | None]:
    """The row each conflict ends with on both sides, from the user's choices. Refuses missing or stale choices."""
    choices = choices or {}
    if not isinstance(choices, dict):
        raise PublishError('Choices must be an object: {conflict id: "dev" | "live" | {field: "dev" | "live"}}.')
    conflicts = {i.id: i for i in items if i.side == 'conflict'}
    stale = sorted(set(choices) - set(conflicts))
    if stale:
        raise PublishError('Some choices are for conflicts that are no longer there (the worlds changed): look again.', stale)
    missing = [conflicts[c] for c in conflicts if c not in choices]
    if missing:
        raise Unresolved(f'{len(missing)} conflict{"s" if len(missing) != 1 else ""} to resolve: choose dev or live for each.',
                         [describe(i) for i in missing])
    out, problems = {}, []
    for cid, item in conflicts.items():
        choice = choices[cid]
        if choice in ('dev', 'live'):
            out[cid] = item.dev if choice == 'dev' else item.live
        elif isinstance(choice, dict):
            if item.dev is None or item.live is None:
                problems.append(f'{cid}: removed on one side; choose "dev" or "live" for the whole row.')
                continue
            fields = differing(item.dev, item.live, world_column(item.spec.table))
            # A field only one side changed goes that side's way unless chosen; one both changed must be chosen.
            auto = {f: s for f in fields if (s := field_side(item, f)) != 'conflict'}
            bad = [f for f in choice if f not in fields or choice[f] not in ('dev', 'live')]
            unchosen = [f for f in fields if f not in choice and f not in auto]
            if bad or unchosen:
                problems.append(f'{cid}: choose "dev" or "live" for each of {", ".join(unchosen or fields)}'
                                + (f' (not {", ".join(bad)})' if bad else ''))
                continue
            picked = {**auto, **choice}
            out[cid] = {**item.live, **{f: item.dev[f] for f in fields if picked[f] == 'dev'}}
        else:
            problems.append(f'{cid}: choose "dev", "live" or a field-by-field map.')
    if problems:
        raise PublishError('Some choices cannot be used.', problems)
    return out


# --------------------------------------------------------------------------- Applying rows

def stored(table, row):
    """A JSON row back into column values for the database."""
    return from_json({k: v for k, v in row.items() if k != 'posts'})


def apply_rows(conn, world_id, changes, have: Snapshot):
    """Makes each (spec, key, row or None) so in one world: removals first (children before parents), then writes
    (parents before children). Inside the caller's transaction; constraints are checked by finish()."""
    order = {s.table: i for i, s in enumerate(SYNCED)}
    changes = sorted(changes, key=lambda c: order[c[0].table])
    removed = [(s, k) for s, k, row in changes if row is None and have.row(s.table, k) is not None]
    written = [(s, k, row) for s, k, row in changes if row is not None]
    if any(s.table == 'world.worlds' for s, _ in removed):
        raise PublishError('The world itself is never removed by a pull or an import.')
    # Areas that go: their cells or interiors, unless the same id stays as the other kind. Doors into them must go too.
    staying = {k[0] for s, k, _ in written if s.table in AREAS_OF}
    gone_areas = sorted({k[0] for s, k in removed if s.table in AREAS_OF} - staying)
    if gone_areas:
        gone_links = {k[0] for s, k in removed if s.table == 'world.links'}
        kept = [r[0] for r in conn.execute('SELECT id FROM world.links WHERE world_id = %s AND (a_area = ANY(%s) OR b_area = ANY(%s))',
                                           (world_id, gone_areas, gone_areas)).fetchall() if r[0] not in gone_links]
        if kept:
            raise PublishError('Doors would be left leading into removed places.',
                               [f'Connection {k} leads into a place this removes.' for k in sorted(kept)])
    rewritten_cells = [k for s, k, _ in written if s.table == 'world.cells' and have.row(s.table, k) is not None]
    for spec, key in reversed(removed):
        table = spec.table
        _, keys, _ = shape(conn, table)
        column = world_column(table)
        full = tuple(world_id if k == column else key[[c for c in keys if c != column].index(k)] for k in keys)
        delete(conn, table, full)
        if table in AREAS_OF and key[0] in gone_areas:
            conn.execute('DELETE FROM world.areas WHERE world_id = %s AND id = %s', (world_id, key[0]))
    for key in rewritten_cells:              # (A moved cell may pass over another moving out of its way.)
        conn.execute('DELETE FROM world.cells WHERE world_id = %s AND id = %s', (world_id, key[0]))
    for spec, key, row in written:
        table = spec.table
        if table in AREAS_OF:
            conn.execute('INSERT INTO world.areas (world_id, id, kind) VALUES (%s, %s, %s) '
                         'ON CONFLICT (world_id, id) DO UPDATE SET kind = excluded.kind', (world_id, key[0], AREAS_OF[table]))
        write(conn, table, stored(table, row), touch=True)
        if table == 'live.patrol_routes':
            conn.execute('DELETE FROM live.patrol_posts WHERE world_id = %s AND route_id = %s', (world_id, key[0]))
            for post in row.get('posts', []):
                conn.execute('INSERT INTO live.patrol_posts (world_id, route_id, seq, area, x, y) VALUES (%s, %s, %s, %s, %s, %s)',
                             (world_id, key[0], post['seq'], post['area'], post['x'], post['y']))


def lock_world(conn):
    """One writer at a time: our own lock, and the world row (Atlas's edits take turns on it)."""
    conn.execute('SELECT pg_advisory_xact_lock(%s)', (PUBLISH_LOCK,))
    conn.execute('SELECT id FROM world.worlds FOR UPDATE').fetchall()


def tell_editors(conn, world_id, editor, label):
    """Open Atlas sessions reload: the world changed under them in ways the edit log's keys don't describe."""
    if world_id is None:
        return
    conn.execute('UPDATE world.worlds SET revision = revision + 1, updated_at = now() WHERE id = %s', (world_id,))
    conn.execute('''INSERT INTO world.edits (world_id, batch, editor, client_id, label, key, before, after)
                    VALUES (%s, txid_current(), %s, 'world-sync', %s, 'reload', NULL, NULL)''', (world_id, editor[:60], label[:200]))


# --------------------------------------------------------------------------- Server state

def server_state(conn) -> dict:
    """{'state': running|maintenance, 'by', 'at', 'note'} (running for a world without the table yet)."""
    if not shape(conn, 'admin.server_state')[0]:
        return {'state': 'running', 'by': '', 'at': None, 'note': ''}
    row = conn.execute('SELECT state, changed_by, changed_at, note FROM admin.server_state').fetchone()
    if not row:
        return {'state': 'running', 'by': '', 'at': None, 'note': ''}
    return {'state': row[0], 'by': row[1], 'at': row[2].isoformat(), 'note': row[3]}


def server_running(conn) -> bool:
    """Whether a game server holds this world (its owner lock), as far as this database can see."""
    return bool(conn.execute('''SELECT 1 FROM pg_locks WHERE locktype = 'advisory' AND classid = %s::int::oid AND objid = %s::int::oid
                                AND objsubid = 2 AND database = (SELECT oid FROM pg_database WHERE datname = current_database())''',
                             OWNER_LOCK).fetchone())


def set_state(conn, state, editor='', note=''):
    if state not in STATES:
        raise PublishError(f'A server state is running or maintenance, not {state!r}.')
    if not shape(conn, 'admin.server_state')[0]:
        raise PublishError('This world has no server state yet: python3 tools/world_db.py migrate (migration 0054).')
    conn.execute('UPDATE admin.server_state SET state = %s, changed_by = %s, changed_at = now(), note = %s',
                 (state, (editor or '')[:60], (note or '')[:400]))
    return server_state(conn)


# --------------------------------------------------------------------------- The two worlds

def pair(dev_key):
    """(dev key, live key) of a dev world with an upstream."""
    entry = world_db.world(dev_key)
    if entry['kind'] != 'dev':
        raise PublishError(f'{dev_key} is a live world; pull and import are for a dev world with an upstream.')
    if not entry.get('upstream'):
        raise PublishError(f'{dev_key} ("{entry["name"]}") has no upstream live world: it is a side project, with nothing to '
                           'pull from or import into.')
    return dev_key, entry['upstream']


def same_world(dev_id, live_id):
    if dev_id and live_id and dev_id != live_id:
        raise PublishError(f'The dev world holds {dev_id} and the live world {live_id}: they are not copies of one world.')
    return dev_id or live_id


def read_live(live):
    """The live world's snapshot, world and release, from one consistent read."""
    with live.transaction():
        live.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')
        world_id = the_world(live, 'The live world')
        return snapshot(live, world_id), world_id, release_of(live, world_id)


def base_or_refuse(dev, dev_key, live_key):
    header, base = load_base(dev)
    if header is None:
        raise PublishError(f'{dev_key} has no base yet: take one with python3 tools/world_sync.py init-base {dev_key} '
                           '(the differences there are now count as changes made in dev).')
    if header['upstream'] != live_key:
        raise PublishError(f'{dev_key}\'s base is from {header["upstream"]}, but its upstream is now {live_key}: take a new base '
                           f'(python3 tools/world_sync.py init-base {dev_key}).')
    return header, base


def plan(dev, live, dev_key, live_key):
    """(plan JSON, items, snapshots) of a dev world against its upstream."""
    snap_live, live_id, release = read_live(live)
    with dev.transaction():
        dev.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')
        header, base = base_or_refuse(dev, dev_key, live_key)
        dev_id = the_world(dev, 'The dev world')
        snap_dev = snapshot(dev, dev_id)
    world_id = same_world(dev_id, live_id)
    items = three_way(base, snap_dev, snap_live)
    by_side = {s: [describe(i) for i in items if i.side == s] for s in ('live', 'dev', 'conflict')}
    info = {'dev': dev_key, 'live': live_key, 'world': world_id, 'base': header, 'liveRelease': release,
            'counts': {'live': len(by_side['live']), 'dev': len(by_side['dev']), 'conflicts': len(by_side['conflict'])},
            'summary': {'live': summarize(items, lambda its: [(i, i.base, i.live) for i in its if i.side == 'live']),
                        'dev': summarize(items, lambda its: [(i, i.base, i.dev) for i in its if i.side == 'dev'])},
            'liveChanges': by_side['live'], 'devChanges': by_side['dev'], 'conflicts': by_side['conflict'],
            'digest': plan_digest(items)}
    return info, items, (base, snap_dev, snap_live, world_id, release, header)


def check_expected(info, expect):
    if expect and expect != info['digest']:
        raise PublishError('The worlds changed since this plan was made: look at the new plan and choose again.')


def counts_of(changes, have):
    out = {}
    for spec, key, row in changes:
        what = change(have.row(spec.table, key), row)
        if what in ('added', 'changed', 'removed'):
            counts = out.setdefault(spec.part, {}).setdefault(spec.label, {'added': 0, 'changed': 0, 'removed': 0})
            counts[what] += 1
    return {'world': out.get('world', {}), 'live': out.get('live', {})}


class Sync:
    """Pull, import, the base and the server state, for hosts (Atlas, the DM, the one tool) and the command line.
    Every world is reached as ratw_publisher (the one role that reaches both a dev and a live world), unless
    `connect(key)` says otherwise."""

    def __init__(self, connect=None):
        self._connect = connect or (lambda key: world_db.connect(key, 'publisher'))

    def connect(self, key):
        try:
            return self._connect(key)
        except world_db.DatabaseError as error:
            raise S.Unavailable(f'Cannot reach the {key} world\'s database: {error}') from error

    # -- reading -----------------------------------------------------------------
    def status(self, key):
        entry = world_db.world(key)
        with self.connect(key) as conn:
            world_id = the_world(conn, key)
            header = None
            if entry['kind'] == 'dev' and shape(conn, 'admin.sync_base')[0]:
                row = conn.execute('SELECT upstream, world_id, release, taken_at, taken_by, how, rows FROM admin.sync_base').fetchone()
                if row:
                    header = {'upstream': row[0], 'world': row[1], 'release': row[2], 'takenAt': row[3].isoformat(),
                              'by': row[4], 'how': row[5], 'rows': row[6]}
            return {'key': key, 'name': entry['name'], 'kind': entry['kind'], 'upstream': entry.get('upstream'), 'world': world_id,
                    'server': server_state(conn), 'serverRunning': server_running(conn), 'base': header,
                    'release': release_of(conn, world_id)}

    def diff(self, dev_key):
        """The three-way plan (Atlas's Update from live shows it; warns while the live server runs)."""
        dev_key, live_key = pair(dev_key)
        with self.connect(dev_key) as dev, self.connect(live_key) as live:
            info, _, _ = plan(dev, live, dev_key, live_key)
            running = server_running(live) or server_state(live)['state'] == 'running'
        info['warnings'] = ['The live server is running: live data may still move after this; the import compares again.'] if running else []
        return info

    def plan_import(self, dev_key):
        """What Import from dev would do, and what blocks it now (maintenance, a running server, the dev world's problems)."""
        dev_key, live_key = pair(dev_key)
        with self.connect(dev_key) as dev, self.connect(live_key) as live:
            info, _, _ = plan(dev, live, dev_key, live_key)
            _, revision, errors, warnings = validate_dev(dev)
            state, running = server_state(live), server_running(live)
            history = releases(live) if info['world'] else []
        blocked = []
        if state['state'] != 'maintenance':
            blocked.append(f'{live_key} is not in maintenance: close it first (the DM, or python3 tools/world_sync.py maintenance {live_key} on).')
        elif running:
            blocked.append(f'{live_key}\'s game server has not stopped yet (it closes within a minute of maintenance).')
        if errors:
            blocked.append('The dev world has problems that block publishing.')
        if not info['counts']['dev'] and not info['counts']['conflicts']:
            blocked.append('There is nothing to import: live already has every change made in dev.')
        return {**info, 'devRevision': revision, 'errors': errors, 'warnings': warnings, 'server': state,
                'serverRunning': running, 'blocked': blocked, 'releases': history}

    # -- the base ----------------------------------------------------------------
    def init_base(self, dev_key, editor='', prefer='dev'):
        """A dev world's first base: its upstream's snapshot now (every difference counts as dev's), or with
        prefer='live' its own (every difference counts as live's: a pull then makes dev equal live)."""
        dev_key, live_key = pair(dev_key)
        if prefer not in ('dev', 'live'):
            raise PublishError('prefer is dev or live.')
        with self.connect(dev_key) as dev, self.connect(live_key) as live:
            snap_live, live_id, release = read_live(live)
            with dev.transaction():
                dev.execute('SELECT pg_advisory_xact_lock(%s)', (PUBLISH_LOCK,))
                dev_id = the_world(dev, 'The dev world')
                world_id = same_world(dev_id, live_id)
                base = snap_live if prefer == 'dev' else snapshot(dev, dev_id)
                load_base(dev)                               # (Refuses a world without the tables.)
                save_base(dev, live_key, world_id, release, base, editor, 'init')
        return {'dev': dev_key, 'live': live_key, 'world': world_id, 'release': release, 'rows': base.count(), 'prefer': prefer}

    # -- pull --------------------------------------------------------------------
    def pull(self, dev_key, choices=None, editor='', expect=None):
        """Atlas's Update from live: every live-only change and the chosen side of each conflict into the dev world,
        in one transaction; live's snapshot becomes the base."""
        dev_key, live_key = pair(dev_key)
        editor = str(editor or 'Someone')[:60]
        with self.connect(dev_key) as dev, self.connect(live_key) as live:
            snap_live, live_id, release = read_live(live)
            with dev.transaction():
                lock_world(dev)
                header, base = base_or_refuse(dev, dev_key, live_key)
                dev_id = the_world(dev, 'The dev world')
                world_id = same_world(dev_id, live_id)
                snap_dev = snapshot(dev, dev_id)
                items = three_way(base, snap_dev, snap_live)
                check_expected({'digest': plan_digest(items)}, expect)
                chosen = resolve(items, choices)
                changes = [(i.spec, i.key, i.live) for i in items if i.side == 'live']
                changes += [(i.spec, i.key, chosen[i.id]) for i in items if i.side == 'conflict' and
                            canonical(chosen[i.id]) != canonical(i.dev)]
                apply_rows(dev, world_id, changes, snap_dev)
                if world_id:
                    finish(dev, world_id, 'the dev world')
                if changes:
                    tell_editors(dev, world_id, editor, f'Updated from {live_key}')
                save_base(dev, live_key, world_id, release, snap_live, editor, 'pull')
        running = None
        try:
            with self.connect(live_key) as live:
                running = server_state(live)['state'] == 'running'
        except S.StoreError:
            pass
        return {'dev': dev_key, 'live': live_key, 'world': world_id, 'release': release, 'applied': counts_of(changes, snap_dev),
                'conflicts': sum(1 for i in items if i.side == 'conflict'), 'kept': sum(1 for i in items if i.side == 'dev'),
                'warnings': ['The live server is running: live data may still move; the import compares again.'] if running else []}

    # -- import ------------------------------------------------------------------
    def import_(self, dev_key, choices=None, password=None, editor='', note='', expect=None):
        """The DM's Import from dev: during maintenance only; one transaction on live and a release; then dev is
        brought level with live and takes it as its base."""
        dev_key, live_key = pair(dev_key)
        editor = str(editor or 'Someone')[:60]
        with self.connect(dev_key) as dev, self.connect(live_key) as live:
            state = server_state(live)
            if state['state'] != 'maintenance':
                raise NotInMaintenance(f'{live_key} is not in maintenance. Close it first: its players are told and its game '
                                       f'server stops (python3 tools/world_sync.py maintenance {live_key} on).')
            check_password(live, password, editor, 'import')
            with dev.transaction():
                dev.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')     # One consistent copy of dev.
                header, base = base_or_refuse(dev, dev_key, live_key)
                dev_id, _, errors, _ = validate_dev(dev)
                if errors:
                    raise PublishError('The dev world has problems that block publishing.', errors)
                snap_dev = snapshot(dev, dev_id)
            with live.transaction():
                taken = live.execute('SELECT pg_try_advisory_xact_lock(%s::int, %s::int)', OWNER_LOCK).fetchone()[0]
                if not taken:
                    raise PublishError(f'{live_key}\'s game server is still running (it closes within a minute of maintenance); '
                                       'try again when it has stopped.')
                live.execute('SELECT pg_advisory_xact_lock(%s)', (PUBLISH_LOCK,))
                live_id = the_world(live, 'The live world')
                world_id = same_world(dev_id, live_id)
                snap_live = snapshot(live, live_id)
                items = three_way(base, snap_dev, snap_live)
                check_expected({'digest': plan_digest(items)}, expect)
                chosen = resolve(items, choices)
                changes = [(i.spec, i.key, i.dev) for i in items if i.side == 'dev']
                changes += [(i.spec, i.key, chosen[i.id]) for i in items if i.side == 'conflict' and
                            canonical(chosen[i.id]) != canonical(i.live)]
                if not changes:
                    raise PublishError('There is nothing to import: live already has every change made in dev.')
                apply_rows(live, world_id, changes, snap_live)
                finish(live, world_id)
                summary = counts_of(changes, snap_live)
                number = record(live, world_id, editor, note, 'import', dev_content(live, world_id), summary)
                after = snapshot(live, world_id)              # (After the build: it saves the roster's assignments.)
            level = self._level(dev, dev_key, live_key, world_id, number, base, snap_dev, after, editor)
        return {'release': number, 'summary': summary, 'conflicts': sum(1 for i in items if i.side == 'conflict'), 'dev': level}

    def _level(self, dev, dev_key, live_key, world_id, number, base, planned, after, editor):
        """After an import: dev takes live's version of every row nobody edited in dev meanwhile, and the base
        follows. A row edited in dev during the import keeps its edit and its old base (so it is still compared
        three ways next time)."""
        try:
            with dev.transaction():
                lock_world(dev)
                now = snapshot(dev, world_id)
                changes, moved = [], []
                for spec in SYNCED:
                    t = spec.table
                    for key in set(now.get(t, {})) | set(after.get(t, {})) | set(planned.get(t, {})):
                        if now.digest(t, key) == after.digest(t, key):
                            continue
                        if now.digest(t, key) == planned.digest(t, key):
                            changes.append((spec, key, after.row(t, key)))
                        else:
                            moved.append((t, key))
                apply_rows(dev, world_id, changes, now)
                finish(dev, world_id, 'the dev world')
                if changes:
                    tell_editors(dev, world_id, editor, f'Imported into {live_key} (release {number})')
                new_base = Snapshot({spec.table: dict(after.get(spec.table, {})) for spec in SYNCED})
                for table, key in moved:
                    # Live's row is the base where dev's planned row went live; else the old base stays.
                    if planned.digest(table, key) != after.digest(table, key):
                        old = base.get(table, {}).get(key)
                        if old is None:
                            new_base[table].pop(key, None)
                        else:
                            new_base[table][key] = old
                save_base(dev, live_key, world_id, number, new_base, editor, 'import')
            return {'updated': len(changes), 'editedMeanwhile': [item_id(t, k) for t, k in moved]}
        except (PublishError, psycopg.Error) as error:
            # Live has the release; dev keeps its rows and its old base, so the next pull brings live's changes in.
            return {'updated': 0, 'error': f'The dev world was not brought level with live: {error}'}

    # -- the server and the live world --------------------------------------------
    def maintenance(self, key, on, editor='', note=''):
        with self.connect(key) as conn:
            set_state(conn, 'maintenance' if on else 'running', editor,
                      note or ('Closed for maintenance.' if on else ''))
        return self.status(key)

    def switch_live(self, key, editor=''):
        """The live server's world (world_db.py serve-world): only while the current live world is in maintenance (its
        server stopped). The new one is put in maintenance too, so the launcher waits until the DM opens it."""
        target = world_db.resolve(key, 'live')
        current = world_db.local_choice()['live']
        if current in world_db.keys('live') and current != target:
            with self.connect(current) as conn:
                if server_state(conn)['state'] != 'maintenance':
                    raise NotInMaintenance(f'The live world {current} is not in maintenance: close it first, so its players '
                                           f'are told and its server stops (python3 tools/world_sync.py maintenance {current} on).')
        with self.connect(target) as conn:
            if server_state(conn)['state'] != 'maintenance':
                set_state(conn, 'maintenance', editor, f'Switched to from {current}; opens when the DM turns it on.')
        world_db.set_local(live=target)
        return {'live': target, 'was': current, **{k: v for k, v in self.status(target).items() if k in ('server', 'name')}}

    def releases(self, live_key):
        with self.connect(world_db.resolve(live_key, 'live')) as live:
            return releases(live)

    def rollback(self, live_key, number, password, editor='', note=''):
        with self.connect(world_db.resolve(live_key, 'live')) as live:
            return rollback(live, number, password, str(editor or 'Someone'), note or '')


# Module-level calls for the screens and tools (each opens its own connections).
def status(key, connect=None):
    return Sync(connect).status(key)


def diff(dev_key, connect=None):
    return Sync(connect).diff(dev_key)


def pull(dev_key, choices=None, editor='', connect=None, expect=None):
    return Sync(connect).pull(dev_key, choices, editor, expect)


def plan_import(dev_key, connect=None):
    return Sync(connect).plan_import(dev_key)


def import_(dev_key, choices=None, password=None, editor='', note='', connect=None, expect=None):
    return Sync(connect).import_(dev_key, choices, password, editor, note, expect)


def init_base(dev_key, editor='', prefer='dev', connect=None):
    return Sync(connect).init_base(dev_key, editor, prefer)


def maintenance(key, on, editor='', note='', connect=None):
    return Sync(connect).maintenance(key, on, editor, note)


def switch_live(key, editor='', connect=None):
    return Sync(connect).switch_live(key, editor)


def base_from_copy(conn, upstream_key, editor=''):
    """world_db.py new --from LIVE: the copy is its upstream as it was, so it is its own first base (taken from the
    copy, before anyone can change either). The copy's server state starts running."""
    with conn.transaction():
        world_id = the_world(conn, 'The copy')
        snap = snapshot(conn, world_id)
        conn.execute('DELETE FROM admin.sync_base')
        save_base(conn, upstream_key, world_id, release_of(conn, world_id), snap, editor, 'copy')
        conn.execute("UPDATE admin.server_state SET state = 'running', changed_by = %s, changed_at = now(), note = ''", (editor[:60],))
    return snap.count()


# --------------------------------------------------------------------------- Command line

def counts_line(summary):
    parts = []
    for part in ('world', 'live'):
        for label, counts in summary.get(part, {}).items():
            done = ', '.join(f'{n} {k}' for k, n in counts.items() if n)
            if done:
                parts.append(f'    {label}: {done}')
    return '\n'.join(parts) or '    nothing'


def short(value):
    text = json.dumps(value, ensure_ascii=False)
    return text if len(text) <= 70 else text[:67] + '...'


def print_plan(info):
    base = info['base']
    print(f'{info["dev"]} against {info["live"]} (world {info["world"]}, live release {info["liveRelease"]}); base taken '
          f'{base["takenAt"][:19]} by {base["by"] or "?"} ({base["how"]}, release {base["release"]})')
    print(f'  live changed ({info["counts"]["live"]}): taken by a pull\n{counts_line(info["summary"]["live"])}')
    print(f'  dev changed ({info["counts"]["dev"]}): taken by an import\n{counts_line(info["summary"]["dev"])}')
    print(f'  conflicts ({info["counts"]["conflicts"]}):')
    for c in info['conflicts']:
        print(f'    {c["id"]} ({c["name"]}): dev {c["dev"]}, live {c["live"]}')
        for f in c['fields'][:12]:
            print(f'      {f["field"]}: base {short(f["base"])} | dev {short(f["dev"])} | live {short(f["live"])}')
    for w in info.get('warnings', []):
        print(f'  warning: {w}')


def load_choices(path):
    if not path:
        return {}
    try:
        return json.loads(Path(path).read_text(encoding='utf-8'))
    except (OSError, ValueError) as error:
        raise PublishError(f'Cannot read the choices in {path}: {error}') from error


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    s = sub.add_parser('status', help="a world's server state, base and release")
    s.add_argument('key')
    s.add_argument('--state', action='store_true', help='print only running or maintenance (for the launchers)')
    s.add_argument('--json', action='store_true')
    for name in ('diff', 'plan-import'):
        p = sub.add_parser(name)
        p.add_argument('dev')
        p.add_argument('--json', action='store_true')
    p = sub.add_parser('pull', help="Update from live: live's changes into the dev world")
    p.add_argument('dev')
    p.add_argument('--choices', help='a JSON file: {conflict id: "dev" | "live" | {field: "dev" | "live"}}')
    p = sub.add_parser('import', help='Import from dev into its live world (during maintenance)')
    p.add_argument('dev')
    p.add_argument('--choices')
    p.add_argument('--note', default='')
    p = sub.add_parser('init-base', help="take a dev world's first base")
    p.add_argument('dev')
    p.add_argument('--prefer', choices=('dev', 'live'), default='dev',
                   help="dev (default): differences there are now count as dev's; live: as live's")
    m = sub.add_parser('maintenance', help="close a world's game server for maintenance (on), or open it again (off)")
    m.add_argument('key')
    m.add_argument('switch', choices=('on', 'off'))
    m.add_argument('--note', default='')
    args = parser.parse_args(argv)
    sync, me = Sync(), getpass.getuser()
    try:
        if args.command == 'status':
            key = world_db.resolve(args.key)
            info = sync.status(key)
            if args.state:
                print(info['server']['state'])
            elif args.json:
                print(json.dumps(info, indent=1))
            else:
                srv = info['server']
                print(f'{key} ("{info["name"]}", {info["kind"]}' + (f', upstream {info["upstream"]}' if info['upstream'] else '') + ')')
                print(f'  server: {srv["state"]}' + (f' since {srv["at"][:19]} by {srv["by"] or "?"}' if srv['at'] else '')
                      + (f': {srv["note"]}' if srv['note'] else '') + ('; a game server holds it' if info['serverRunning'] else ''))
                print(f'  release: {info["release"]}')
                if info['kind'] == 'dev' and info['upstream']:
                    b = info['base']
                    print(f'  base: {b["upstream"]} release {b["release"]}, taken {b["takenAt"][:19]} by {b["by"] or "?"} ({b["how"]}, '
                          f'{b["rows"]} rows)' if b else f'  base: none (python3 tools/world_sync.py init-base {key})')
        elif args.command in ('diff', 'plan-import'):
            info = sync.diff(args.dev) if args.command == 'diff' else sync.plan_import(args.dev)
            if args.json:
                print(json.dumps(info, indent=1))
            else:
                print_plan(info)
                for line in info.get('blocked', []):
                    print(f'  blocked: {line}')
                for e in info.get('errors', []):
                    print(f'    problem: {e}')
        elif args.command == 'pull':
            result = sync.pull(args.dev, load_choices(args.choices), me)
            print(f'Pulled {result["live"]} into {result["dev"]} (release {result["release"]}):\n{counts_line(result["applied"])}')
            for w in result['warnings']:
                print(f'  warning: {w}')
        elif args.command == 'import':
            choices = load_choices(args.choices)
            password = getpass.getpass('Publish password: ')
            result = sync.import_(args.dev, choices, password, me, args.note)
            print(f'Release {result["release"]} is live.\n{counts_line(result["summary"])}')
            if result['dev'].get('error'):
                print(result['dev']['error'], file=sys.stderr)
        elif args.command == 'init-base':
            r = sync.init_base(args.dev, me, args.prefer)
            print(f'{r["dev"]}: base taken from {r["live"]} (release {r["release"]}, {r["rows"]} rows); differences there are '
                  f'now count as {r["prefer"]}\'s.')
        elif args.command == 'maintenance':
            key = world_db.resolve(args.key)
            info = sync.maintenance(key, args.switch == 'on', me, args.note)
            print(f'{key}: {info["server"]["state"]}' + (' (its game server tells its players, saves and stops within a minute)'
                                                        if args.switch == 'on' else ' (a waiting launcher starts its server)'))
    except Unresolved as error:
        print(str(error), file=sys.stderr)
        for c in error.conflicts:
            print(f'  {c["id"]} ({c["name"]}): dev {c["dev"]}, live {c["live"]}; fields {", ".join(f["field"] for f in c["fields"])}',
                  file=sys.stderr)
        return 1
    except (PublishError, S.StoreError, world_db.DatabaseError) as error:
        print('\n'.join([str(error)] + [f'  {e}' for e in getattr(error, 'errors', [])]), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
