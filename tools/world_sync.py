#!/usr/bin/env python3
"""Pull and import, with conflicts: a dev world and the live world it is a working copy of (its upstream).
Docs/Design/66-worlds-atlas-and-dm.md, section 4.

  python3 tools/world_sync.py status KEY [--state] [--json]   a world's server state, base and release
  python3 tools/world_sync.py diff DEV [--json]               the three-way plan: live-only, dev-only, conflicts
  python3 tools/world_sync.py pull DEV [--choices FILE]       Atlas's Update from live: live's changes into dev
  python3 tools/world_sync.py plan-import DEV [--json]        what an import would do (and what blocks it)
  python3 tools/world_sync.py import DEV [--choices FILE] [--replace-map] --note "..."
                                                              the DM's Import from dev (asks for the publish password;
                                                              --replace-map: a dev world on another map replaces live's)
  python3 tools/world_sync.py init-base DEV [--prefer dev|live]
                                                              take a dev world's first base
  python3 tools/world_sync.py maintenance KEY on|off [--note "..."]
                                                              close a world's game server for maintenance, or reopen it
  python3 tools/world_sync.py map KEY [--new --name NAME]     a world's map identity (--new: stamp a new one, as owner)

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

The map identity (doc 66, "The same map"). Pull and import compare a world with its upstream row by row, which only
means something while both are the same map. Every world has a map identity: a stamp made when a map is generated new
(the Ridgemere 2.0 generator's write, world_db.py new --without-map, or `map KEY --new`), kept by every ordinary edit
(adding a cell included) and copied with the database. A dev world's stamp is admin.settings 'map'; a live world's is
the newest release that records one (summary 'map': every import records the dev world's); a world with neither is on
its original map ('original:<world id>'). Pull (and Atlas's Update from live) refuses when the identities differ. An
import of a different map replaces live's map: only with the replace-the-map confirmation on top of the password, and
only while the live world has every migration the dev world has. Its map (the world part) becomes dev's whole; its live
data is compared three ways as ever (against the base, or with none, live as it is, a live game's field that differs
being a conflict: baseless()), but nothing live has is ever removed: an NPC or profession slot in a place the new map
hasn't is a decision (below), and the old map's NPC areas, claims and patrol posts go. Afterwards both worlds share the
identity and pull works again.

The live world's truth (doc 66, Progress, "The live world's truth: NPCs and markets in conflicts"). The live game owns
its NPCs' live state (world_store.GAME_COLUMNS: an NPC's home and post, what it carries, its age; a row it made, whole)
and the market (the save, live.npc_state, live.economy: never synced). A conflict over a game-owned field defaults to
live's (the plan's `default`, taken when no choice is sent; `defaulted` in the result); authored fields changed on both
sides still need a choice. A place's area, x and y are one field. Rows hanging on an NPC (its voice, family, factions)
that one side removed with it go as the conflict over the NPC ends. A map-replacing import lists every NPC or profession
slot whose place the new map hasn't as a conflict needing a decision for that place ("unplace", dev's place, or a new
place {cell, x, y}: `{"home": "dev" | "unset" | {...}}` in a field-by-field choice); none is unplaced unasked.

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
import re
import secrets
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


class DifferentMap(PublishError):
    """The two worlds are on different maps (their map identities differ): nothing to pull, and an import would
    replace live's map. `maps` is {'dev': identity, 'live': identity, 'same': False}."""
    status = 409

    def __init__(self, message, maps, errors=None):
        super().__init__(message, errors)
        self.maps = maps


class Moved(PublishError):
    """The worlds changed since the plan the choices were made on (a stale `expect`, or choices for conflicts no
    longer there): look at the new plan and choose again."""
    status = 409


_shapes = {}


def shape(conn, table):
    """(columns, key columns, {column: type}) of a table, from the catalog. Kept per database: two worlds may differ
    by a migration (0055's business settings are in one and not the other)."""
    info = getattr(conn, 'info', None)
    known = (getattr(info, 'host', ''), getattr(info, 'port', ''), getattr(info, 'dbname', ''), table)
    if known not in _shapes:
        schema, name = table.split('.')
        cols = conn.execute('''SELECT column_name, data_type FROM information_schema.columns
                               WHERE table_schema = %s AND table_name = %s ORDER BY ordinal_position''', (schema, name)).fetchall()
        if not cols:
            return [], [], {}                  # (A table a world without the latest migrations lacks: not cached.)
        keys = [r[0] for r in conn.execute('''SELECT a.attname FROM pg_index i
            JOIN pg_attribute a ON a.attrelid = i.indrelid AND a.attnum = ANY(i.indkey)
            WHERE i.indrelid = %s::regclass AND i.indisprimary ORDER BY array_position(i.indkey, a.attnum)''', (table,)).fetchall()]
        _shapes[known] = ([c for c, _ in cols], keys, dict(cols))
    return _shapes[known]


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
        # (The map that release was on comes back with it: map_identity reads it from the newest release.)
        was_on = map_identity(prod, 'live', world_id, upto=number)
        apply_world(prod, plan)
        finish(prod, world_id)
        new = record(prod, world_id, editor, note or f'Rolled back to release {number}.', 'rollback', content,
                     {'world': summary, 'live': {}, 'map': {k: was_on[k] for k in ('id', 'name', 'made', 'by', 'how')}},
                     restored=number)
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
    ('live.business_settings', 'business settings', 'live'),   # The store room's per-business switch (doc 67, 6; 0055).
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
        'live.npc_relations': f'npc_id NOT IN {SPAWNED} AND other_id NOT IN {SPAWNED}',
        'live.business_settings': f'npc_id NOT IN {SPAWNED}'}
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


def unshared(dev, live, *snaps) -> set:
    """Tables one of the two worlds lacks (a migration not applied there yet, as 0055's business settings in DEV and
    PROD): counted as having no rows on either side, so nothing is compared, pulled or imported for them. Empties them
    in each snapshot given; returns them."""
    missing = {spec.table for spec in SYNCED if not shape(dev, spec.table)[0] or not shape(live, spec.table)[0]}
    for snap in snaps:
        if snap is not None:
            for table in missing:
                snap[table] = {}
    return missing


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
    gone: tuple = ()                # A map-replacing import: the places (home, work, evening) live's row would keep that
                                    # the new map hasn't (a decision each: dev's, unset, or a new place).

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


def place_columns(table, field):
    """(area, x, y) columns of the place a column belongs to, on an NPC or profession slot; () for any other column."""
    if table not in ('live.npcs', 'live.profession_slots'):
        return ()
    place, _, part = field.rpartition('_')
    return (f'{place}_area', f'{place}_x', f'{place}_y') if place in PLACED and part in ('area', 'x', 'y') else ()


def field_side(item: Item, field) -> str:
    """Which side changed one field of a row both sides have: 'same', 'dev', 'live' or 'conflict' (both, differently,
    or the row is new on both). A place's area and tile (an NPC's or slot's home_area, home_x, home_y) are one field:
    each of them is the place's side, so a place is never half dev's and half live's."""
    group = place_columns(item.spec.table, field)
    if group and field != group[0]:
        return field_side(item, group[0])
    if group:
        pick = lambda row: canonical([(row or {}).get(c) for c in group])
        d, l = pick(item.dev), pick(item.live)
        if d == l:
            return 'same'
        if item.base is None:
            return 'conflict'
        b = pick(item.base)
        return 'live' if d == b else 'dev' if l == b else 'conflict'
    d, l = canonical((item.dev or {}).get(field)), canonical((item.live or {}).get(field))
    if d == l:
        return 'same'
    if item.base is None:
        return 'conflict'
    b = canonical(item.base.get(field))
    return 'live' if d == b else 'dev' if l == b else 'conflict'


# --------------------------------------------------------------------------- The live world's truth
# Doc 66, Progress, "The live world's truth: NPCs and markets in conflicts". Which side a conflict defaults to: the live
# game owns its NPCs' live state (world_store.GAME_COLUMNS: an NPC's home and post, what it carries, its age; a row the
# game made, whole), so where both sides changed such a field, live's is kept unless the user chooses dev's. Authored
# fields changed on both sides have no default: the user chooses. A field only one side changed goes that side's way.

def game_made(item: Item) -> bool:
    """A row the live game made (origin 'runtime': a birth), on any side."""
    return any(isinstance(r, dict) and r.get('origin') == 'runtime' for r in (item.base, item.live, item.dev))


def owner(item: Item, field) -> str:
    """'game' (the live game's: its value on live is the truth) or 'authored' (Atlas's)."""
    return 'game' if S.game_owned(item.spec.table, field) or game_made(item) else 'authored'


def field_default(item: Item, field):
    """The side a field of a row both sides have goes by default: the one that changed it, or for one both changed,
    live's when it is the game's; None when the user must choose."""
    side = field_side(item, field)
    if side in ('dev', 'live', 'same'):
        return side
    return 'live' if owner(item, field) == 'game' else None


def default_choice(item: Item):
    """A conflict's default choice, as the user would send it, or None when it needs one: field by field (live's for
    the game's fields both changed, each other field the side that changed it) when every field both changed is the
    game's; 'live' for a row removed on one side when live's side is the game's (it changed a game field, or made the
    row); None for a map-replacing import's row whose place the new map hasn't (a decision each)."""
    if item.side != 'conflict' or item.gone:
        return None
    column = world_column(item.spec.table)
    if item.dev is not None and item.live is not None:
        both = [f for f in differing(item.dev, item.live, column) if field_side(item, f) == 'conflict']
        if any(field_default(item, f) is None for f in both):
            return None
        return {f: 'live' for f in both}
    if game_made(item):
        return 'live'
    if item.dev is None and item.live is not None and item.base is not None:
        if any(owner(item, f) == 'game' for f in differing(item.base, item.live, column)):
            return 'live'                      # (Dev removed it; the live game changed it since: live's.)
    return None


def merged_row(item: Item, picked: dict) -> dict:
    """Live's row with dev's value of each field picked 'dev'."""
    return {**item.live, **{f: item.dev[f] for f, side in picked.items() if side == 'dev' and f in item.dev}}


def outcome_row(item: Item):
    """The row a conflict ends with by default: on a row both sides have, each field the side that changed it (one
    both changed: live's, the game's by default, an authored one still to choose); live's for a row one side removed.
    What a map-replacing import checks against the new map, and what "unplace" keeps."""
    if item.side == 'live' or item.dev is None or item.live is None:
        return item.live
    column = world_column(item.spec.table)
    return merged_row(item, {f: field_default(item, f) or 'live' for f in differing(item.dev, item.live, column)})


def undecided(item: Item) -> list[str]:
    """The fields of a conflicting row both sides have that have no default (authored, changed on both): the user's."""
    if item.dev is None or item.live is None:
        return []
    return [f for f in differing(item.dev, item.live, world_column(item.spec.table)) if field_default(item, f) is None]


# Rows that hang on an NPC (their foreign keys cascade from it): removed in a world with the NPC, so such a removal is
# the NPC's, not theirs. And the other rows each needs to stand (a faction member's faction).
NPC_DEPENDENTS = {'live.npc_voice': ('npc_id',), 'live.npc_relations': ('npc_id', 'other_id'),
                  'live.faction_members': ('npc_id',), 'live.business_settings': ('npc_id',)}
DEPENDENTS_NEED = {'live.faction_members': (('live.factions', 'faction_id'),)}


def removed_with(item: Item, side) -> list[str]:
    """The NPCs a row hanging on them (NPC_DEPENDENTS) names, when `side` ('dev' or 'live') removed it: the ones its
    removal may have come with."""
    if item.spec.table not in NPC_DEPENDENTS or getattr(item, side) is not None:
        return []
    row = item.live if side == 'dev' else item.dev
    return [row[c] for c in NPC_DEPENDENTS[item.spec.table] if row and row.get(c) is not None]


def follows(items) -> dict[str, str]:
    """{id of a row removed with its NPC: the id of the conflict over that NPC}: where one side removed an NPC the other
    changed (a conflict), the rows hanging on it removed with it go as that conflict ends, never on their own."""
    npcs = {i.key[0]: i for i in items if i.spec.table == 'live.npcs' and i.side == 'conflict'}
    out = {}
    for item in items:
        for side in ('dev', 'live'):
            if item.side != side:
                continue
            gone = [npcs[n] for n in removed_with(item, side) if n in npcs and getattr(npcs[n], side) is None]
            if gone:
                out[item.id] = gone[0].id
    return out


def with_their_npcs(items, chosen, onto) -> tuple[set[str], list]:
    """Rows hanging on an NPC that one side removed with her, her removal a conflict, go as that conflict ends (doc 66,
    Progress, "The live world's truth"), in a write onto one side (`onto`: 'live' for an import, 'dev' for a pull;
    `chosen`: resolve()'s rows). Where she stays on `onto`, the other side's removals of them are left out of the write
    (`held`); where `onto` itself had removed her and she comes back, its own removals are undone (`back`: the rows as
    the other side and the base have them, written with her). Either way she keeps her voice, family, factions and
    business; where she goes, they go with her. Returns (held: ids, back: [(spec, key, row)])."""
    other = 'dev' if onto == 'live' else 'live'
    plan = {(i.spec.table, i.key): i for i in items}

    def stays(table, key):
        """Whether a row is on `onto` once the write is made."""
        item = plan.get((table, key))
        if item is None:
            return True                 # (The same on both sides, and a row on one of them names it: on both.)
        if item.side == 'conflict':
            return chosen.get(item.id) is not None
        return getattr(item, onto if item.side == onto else other) is not None

    def with_her(npc_id, side):
        """Whether `side` removed this NPC, her removal a conflict that ends with her on `onto`."""
        npc = plan.get(('live.npcs', (npc_id,)))
        return npc is not None and npc.side == 'conflict' and getattr(npc, side) is None and chosen.get(npc.id) is not None
    held, back = set(), []
    for item in items:
        if item.side not in ('dev', 'live'):
            continue
        names = removed_with(item, item.side)
        if not any(with_her(n, item.side) for n in names):
            continue
        row = item.live if item.side == 'dev' else item.dev            # (The kept row: the base's.)
        needs = [('live.npcs', (n,)) for n in names] + \
                [(table, (row[c],)) for table, c in DEPENDENTS_NEED.get(item.spec.table, ()) if row.get(c) is not None]
        if not all(stays(table, key) for table, key in needs):
            continue                    # (Something else it needs goes: it goes with that.)
        if item.side == other:
            held.add(item.id)
        else:
            back.append((item.spec, item.key, row))
    return held, back


def describe(item: Item, followed=None) -> dict:
    """One row of the plan, for the screens and the command line. `followed`: follows(items), for its NPC."""
    table, column = item.spec.table, world_column(item.spec.table)
    fields = sorted(set(differing(item.base, item.dev, column)) | set(differing(item.base, item.live, column))
                    | set(differing(item.dev, item.live, column)))
    any_row = item.dev or item.live or item.base or {}
    name = any_row.get('name') if isinstance(any_row.get('name'), str) else None
    return {'id': item.id, 'table': table, 'label': item.spec.label, 'part': item.spec.part, 'key': list(item.key),
            'name': name or '/'.join(str(k) for k in item.key), 'side': item.side,
            'dev': change(item.base, item.dev), 'live': change(item.base, item.live),
            'fields': [{'field': f, 'base': shown(item.base.get(f)) if item.base and item.base.get(f) != UNKNOWN else None,
                        'dev': shown((item.dev or {}).get(f)) if item.dev else None,
                        'live': shown((item.live or {}).get(f)) if item.live else None,
                        # (Rows on both sides: which side changed the field, 'same' or 'conflict'; else null.)
                        'changed': field_side(item, f) if item.dev is not None and item.live is not None else None,
                        'owner': owner(item, f),
                        'default': field_default(item, f) if item.dev is not None and item.live is not None else None}
                       for f in fields],
            'fieldChoices': item.side == 'conflict' and item.dev is not None and item.live is not None,
            # The choice taken when the user makes none (the game's truth); null: the user must choose.
            'default': default_choice(item),
            # A map-replacing import: the places the new map hasn't, each needing a decision (dev's, unset, a new place).
            'gone': [{'place': p, 'area': (outcome_row(item) or {}).get(f'{p}_area'),
                      'dev': {'cell': item.dev[f'{p}_area'], 'x': item.dev[f'{p}_x'], 'y': item.dev[f'{p}_y']}
                      if item.dev and item.dev.get(f'{p}_area') is not None else None} for p in item.gone],
            'placeChoices': bool(item.gone),
            # A row one side removed with its NPC, the NPC a conflict: it goes as that conflict ends (with_their_npcs).
            'follows': (followed or {}).get(item.id)}


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


def defaulted(items, choices) -> list[str]:
    """The conflicts no choice was sent for that take their default (the game's truth)."""
    choices = choices if isinstance(choices, dict) else {}
    return [i.id for i in items if i.side == 'conflict' and i.id not in choices and default_choice(i) is not None]


def place_of(row, place):
    return {'cell': row[f'{place}_area'], 'x': row[f'{place}_x'], 'y': row[f'{place}_y']} \
        if row and row.get(f'{place}_area') is not None else None


def set_place(row, place, value):
    row[f'{place}_area'], row[f'{place}_x'], row[f'{place}_y'] = \
        (value['cell'], value['x'], value['y']) if value else (None, 0, 0)


def resolve(items, choices, areas=None) -> dict[str, dict | None]:
    """The row each conflict ends with on both sides, from the user's choices; a conflict without one takes its
    default (default_choice: the live game's truth) if it has one. Refuses missing or stale choices.

    `areas` (a map-replacing import: the new map's cells and interiors): a row whose place the new map hasn't needs a
    decision for it, as `{place: "dev" | "unset" | {cell, x, y}}` in a field-by-field choice (place: home, work or
    evening), or "unplace" (the row by default, each such place unset), or "dev"; never left standing nowhere."""
    choices = choices or {}
    if not isinstance(choices, dict):
        raise PublishError('Choices must be an object: {conflict id: "dev" | "live" | {field: "dev" | "live"}}.')
    conflicts = {i.id: i for i in items if i.side == 'conflict'}
    stale = sorted(set(choices) - set(conflicts))
    if stale:
        raise Moved('Some choices are for conflicts that are no longer there (the worlds changed): look again.', stale)
    missing = [conflicts[c] for c in conflicts if c not in choices and default_choice(conflicts[c]) is None]
    if missing:
        raise Unresolved(f'{len(missing)} conflict{"s" if len(missing) != 1 else ""} to resolve: choose dev or live for each'
                         + (', and where the new map hasn\'t their place, unplace or a new place.' if any(i.gone for i in missing)
                            else '.'), [describe(i) for i in missing])
    out, problems = {}, []
    for cid, item in conflicts.items():
        choice = choices[cid] if cid in choices else default_choice(item)
        places = {}
        if choice == 'unplace' and item.gone:
            # (The row as it ends by default, each field the side that changed it, with each gone place unset: never
            # an authored field both changed settled unasked.)
            if left := undecided(item):
                problems.append(f'{cid}: {", ".join(left)} changed on both sides: choose dev or live for each, field by '
                                f'field with "unset" for {", ".join(item.gone)}.')
                continue
            out[cid] = dict(outcome_row(item))
            places = {p: 'unset' for p in item.gone}
        elif choice == 'dev' and item.gone and item.dev is None:
            # (A map-replacing import never removes what live has: a row dev lacks is placed or unplaced, not taken away.)
            problems.append(f'{cid}: {item.spec.label} dev hasn\'t: choose "unplace" or a new place.')
            continue
        elif choice in ('dev', 'live'):
            out[cid] = item.dev if choice == 'dev' else item.live
        elif isinstance(choice, dict):
            places = {p: v for p, v in choice.items() if p in PLACED and item.gone}
            choice = {f: v for f, v in choice.items() if f not in places}
            if item.dev is None or item.live is None:
                if not item.gone or choice:
                    problems.append(f'{cid}: removed on one side; choose "dev" or "live" for the whole row.')
                    continue
                out[cid] = dict(item.live)
            else:
                fields = differing(item.dev, item.live, world_column(item.spec.table))
                # (Each field chosen is "dev" or "live", whatever the field: anything else is refused, never used.)
                wrong = [f for f, side in choice.items() if side not in ('dev', 'live')]
                choice = {f: side for f, side in choice.items() if f not in wrong}
                # (A place's area, x and y are one field: one side for the three. A pick on any of them is the place's,
                # one the two sides happen to agree on too; two sides picked for one place are refused.)
                sides = {}
                for f, side in choice.items():
                    if group := place_columns(item.spec.table, f):
                        sides.setdefault(group, set()).add(side)
                split = [g[0].rpartition('_')[0] for g, picked in sides.items() if len(picked) > 1]
                if split:
                    problems.append(f'{cid}: a place is one field: choose one side for all of its area, x and y '
                                    f'({", ".join(split)}).')
                    continue
                choice = {**{f: side for f, side in choice.items() if not place_columns(item.spec.table, f)},
                          **{c: next(iter(picked)) for g, picked in sides.items() for c in g if c in fields}}
                # A field only one side changed goes that side's way unless chosen; one both changed must be chosen,
                # but for the game's (live's: the live world's truth).
                auto = {f: s for f in fields if (s := field_default(item, f)) is not None}
                bad = wrong + [f for f in choice if f not in fields]
                unchosen = [f for f in fields if f not in choice and f not in auto]
                if bad or unchosen:
                    problems.append(f'{cid}: choose "dev" or "live" for each of {", ".join(unchosen or fields)}'
                                    + (f' (not {", ".join(bad)})' if bad else ''))
                    continue
                out[cid] = merged_row(item, {**auto, **choice})
        else:
            problems.append(f'{cid}: choose "dev", "live" or a field-by-field map.')
            continue
        row = out[cid]
        if areas is None or row is None or item.spec.table not in ('live.npcs', 'live.profession_slots'):
            continue
        row = out[cid] = dict(row)
        for place, how in places.items():
            if how == 'dev' and item.dev is not None:
                set_place(row, place, place_of(item.dev, place))
            elif how == 'unset':
                set_place(row, place, None)
            elif isinstance(how, dict) and isinstance(how.get('cell'), str) and all(
                    isinstance(how.get(k), int) and not isinstance(how.get(k), bool) for k in ('x', 'y')):
                if how['cell'] not in areas:
                    problems.append(f'{cid}: {how["cell"]} is not on the new map.')
                    continue
                set_place(row, place, how)
            else:
                problems.append(f'{cid}: the {place} is "dev", "unset" or a place {{cell, x, y}} on the new map.')
        lost = [f'{p} ({row[f"{p}_area"]})' for p in PLACED if row.get(f'{p}_area') is not None and row[f'{p}_area'] not in areas]
        if lost:
            problems.append(f'{cid} ({row.get("name") or item.key[0]}): not on the new map: {", ".join(lost)}. Choose dev\'s place, '
                            'unset it (unplace) or a new place.')
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


def server_holder(conn) -> dict | None:
    """The game server holding this world, as its claim names it ("ratw_server pid N on HOST port P", " local" when it
    serves its own computer only): {pid, host, port, local, name}; None when none does. (The studio's Play opens it.)"""
    row = conn.execute('''SELECT a.application_name FROM pg_locks l JOIN pg_stat_activity a ON a.pid = l.pid
                          WHERE l.locktype = 'advisory' AND l.classid = %s::int::oid AND l.objid = %s::int::oid AND l.objsubid = 2
                          AND l.database = (SELECT oid FROM pg_database WHERE datname = current_database())''', OWNER_LOCK).fetchone()
    if not row:
        return None
    name = row[0] or ''
    found = re.match(r'ratw_server pid (\d+) on (\S+)(?: port (\d+))?( local)?', name)
    return {'pid': int(found[1]) if found else None, 'host': found[2] if found else None,
            'port': int(found[3]) if found and found[3] else None, 'local': bool(found and found[4]), 'name': name}


def set_state(conn, state, editor='', note=''):
    if state not in STATES:
        raise PublishError(f'A server state is running or maintenance, not {state!r}.')
    if not shape(conn, 'admin.server_state')[0]:
        raise PublishError('This world has no server state yet: python3 tools/world_db.py migrate (migration 0054).')
    conn.execute('UPDATE admin.server_state SET state = %s, changed_by = %s, changed_at = now(), note = %s',
                 (state, (editor or '')[:60], (note or '')[:400]))
    return server_state(conn)


# --------------------------------------------------------------------------- The map identity

MAP_SETTING = 'map'                  # admin.settings key: a dev world's map identity (written by the owner only).


def _has(conn, table):
    return conn.execute('SELECT to_regclass(%s) IS NOT NULL', (table,)).fetchone()[0]


def _identity(value, source, **extra):
    if not isinstance(value, dict) or not isinstance(value.get('id'), str) or not value['id']:
        return None
    return {'id': value['id'], 'name': str(value.get('name') or value['id']), 'made': value.get('made'),
            'by': str(value.get('by') or ''), 'how': str(value.get('how') or ''), 'from': source, **extra}


def stamped_map(conn):
    """The world's own stamp (admin.settings 'map'), or None. (Read, never guessed: a role that may not read the
    settings gets an error, not the original map.)"""
    if not _has(conn, 'admin.settings'):
        return None
    row = conn.execute('SELECT value FROM admin.settings WHERE key = %s', (MAP_SETTING,)).fetchone()
    return _identity(row[0], 'stamp') if row else None


def released_map(conn, world_id, upto=None):
    """The map the newest release (up to `upto`) recorded, or None."""
    if world_id is None or not _has(conn, 'admin.releases'):
        return None
    row = conn.execute('''SELECT number, summary->'map' FROM admin.releases WHERE world_id = %s AND summary ? 'map'
                          AND (%s::integer IS NULL OR number <= %s::integer) ORDER BY number DESC LIMIT 1''',
                       (world_id, upto, upto)).fetchone()
    return _identity(row[1], 'release', release=row[0]) if row else None


def original_map(conn, world_id):
    """A world stamped by nothing (made before map identities): its original map, named after it."""
    if world_id is None:
        return {'id': 'none', 'name': 'no map', 'made': None, 'by': '', 'how': 'none', 'from': 'original'}
    row = conn.execute('SELECT name FROM world.worlds WHERE id = %s', (world_id,)).fetchone()
    name = row[0] if row and row[0] else world_id
    return {'id': f'original:{world_id}', 'name': f"{name}'s original map", 'made': None, 'by': '', 'how': 'original',
            'from': 'original'}


def map_identity(conn, of_kind, world_id=None, upto=None):
    """A world's map identity, {id, name, made, by, how, from: stamp|release|original[, release]}. A dev world: its
    stamp, else the newest release's (a copy of a live world), else its original map. A live world: the newest
    release's (an import records the dev world's), else its stamp (copied with it), else its original map."""
    world_id = world_id if world_id is not None else the_world(conn, 'This world')
    if of_kind == 'live':
        found = released_map(conn, world_id, upto) or stamped_map(conn)
    else:
        found = stamped_map(conn) or released_map(conn, world_id, upto)
    return found or original_map(conn, world_id)


def new_identity(name, by='', how='new'):
    made = datetime.datetime.now(datetime.timezone.utc)
    return {'id': f'map-{made:%Y%m%d}-{secrets.token_hex(6)}', 'name': str(name)[:200], 'made': made.isoformat(),
            'by': str(by)[:60], 'how': how}


LAYOUT_KEYS = {'cells': ('id', 'x', 'y', 'z', 'width', 'height', 'outdoors', 'terrain', 'heights'),
               'rooms': ('id', 'worldX', 'worldY', 'z', 'width', 'height', 'outdoors', 'terrain', 'heights'),
               'links': ('id', 'kind', 'open', 'a', 'b')}


def layout_fingerprint(project):
    """A map's layout as a digest: its cells, interiors and links, their terrain, heights and places, and the spawn. Not
    people, names, descriptions, lighting, weather or claims: a write that changes only those is on the same map."""
    import hashlib
    parts = {kind: sorted(([{k: x.get(k) for k in keys} for x in project.get(kind) or []]), key=lambda x: str(x['id']))
             for kind, keys in LAYOUT_KEYS.items()}
    parts['spawn'] = project.get('spawn')
    text = json.dumps(parts, sort_keys=True, separators=(',', ':'), default=str)
    return 'sha256:' + hashlib.sha256(text.encode('utf-8')).hexdigest()


def map_for_layout(stamp, layout, mode='auto'):
    """Whether a generator's write keeps the world's map identity ('keep') or stamps a new one ('new'): 'new' and 'same'
    say so outright; 'auto' keeps a stamp whose recorded layout is this one, and stamps anew otherwise (a stamp without a
    layout, from before fingerprints, can't be shown to be the same map)."""
    if mode not in ('auto', 'new', 'same'):
        raise ValueError(f'map mode {mode!r}: auto, new or same')
    if mode == 'new' or stamp is None:
        return 'new'
    if mode == 'same':
        return 'keep'
    return 'keep' if stamp.get('layout') == layout else 'new'


def stamp_for_layout(conn, layout, name, by='', how='generated', mode='auto'):
    """A generator's write (as the owner, in its transaction): keeps the stamp when the layout is the same map
    (map_for_layout; recording the layout on it if it had none) or stamps a new identity carrying the layout. Returns
    (identity, 'kept' or 'new')."""
    row = conn.execute('SELECT value FROM admin.settings WHERE key = %s', (MAP_SETTING,)).fetchone() \
        if _has(conn, 'admin.settings') else None
    stamp = row[0] if row and _identity(row[0], 'stamp') else None
    if map_for_layout(stamp, layout, mode) == 'keep':
        if stamp.get('layout') != layout:
            stamp_map(conn, name, identity={**stamp, 'layout': layout})
        return _identity(stamp, 'stamp'), 'kept'
    return stamp_map(conn, name, by, how, identity={**new_identity(name, by, how), 'layout': layout}), 'new'


def stamp_map(conn, name, by='', how='new', identity=None):
    """Gives a world a new map identity (its map was generated new): admin.settings 'map'. Needs the owner (the
    settings are the owner's), inside the caller's transaction or its own. Returns the identity. (`identity`: a whole
    stamp to write instead, e.g. one restored; a generator's write uses stamp_for_layout.)"""
    value = identity or new_identity(name, by, how)
    conn.execute('''INSERT INTO admin.settings (key, value, updated_at) VALUES (%s, %s, now())
                    ON CONFLICT (key) DO UPDATE SET value = excluded.value, updated_at = now()''',
                 (MAP_SETTING, S.Jsonb(value)))
    return _identity(value, 'stamp')


def maps_of(dev, live, dev_world=None, live_world=None):
    """{'dev': identity, 'live': identity, 'same': bool} of a dev world and its upstream."""
    d, l = map_identity(dev, 'dev', dev_world), map_identity(live, 'live', live_world)
    # (A world with no map yet, an empty one, takes any map: its first import is no replacement.)
    return {'dev': d, 'live': l, 'same': d['id'] == l['id'] or 'none' in (d['id'], l['id'])}


def maps_differ_text(maps, dev_key, live_key):
    """'PROD is on a different map (its name) from DEV (its name)'."""
    return (f'{live_key.upper()} is on a different map ({maps["live"]["name"]}) from {dev_key.upper()} '
            f'({maps["dev"]["name"]})')


def different_maps_text(maps, dev_key, live_key):
    """The plain refusal of a pull between two maps."""
    return (f'{maps_differ_text(maps, dev_key, live_key)}: there is nothing to update from until {dev_key.upper()}\'s map is imported into '
            f'{live_key.upper()} (the DM\'s Import from dev, replacing {live_key.upper()}\'s map).')


def schema_columns(conn):
    """{table: {column}} of the synced schemas, as this connection sees them."""
    out = {}
    for table, column in conn.execute('''SELECT table_schema || '.' || table_name, column_name FROM information_schema.columns
                                         WHERE table_schema IN ('world', 'live')''').fetchall():
        out.setdefault(table, set()).add(column)
    return out


def behind(dev, live):
    """What the dev world's schema has and the live world's lacks (migrations not applied there): ['table', 'table
    (N columns)', ...]. A live world behind is fine for a pull or an import on one map (the tables one side lacks are
    left out); a new map needs everything its dev world has."""
    d, l = schema_columns(dev), schema_columns(live)
    out = []
    for table in sorted(d):
        if table not in l:
            out.append(table)
        elif d[table] - l[table]:
            out.append(f'{table} ({len(d[table] - l[table])} column{"s" if len(d[table] - l[table]) != 1 else ""}: '
                       f'{", ".join(sorted(d[table] - l[table])[:6])}{"..." if len(d[table] - l[table]) > 6 else ""})')
    return out


# Live data that stands on the map: what a new map lets go of when it no longer has the place.
PLACED = ('home', 'work', 'evening')


def let_go_of_old_map(conn, world_id):
    """After a map-replacing import's rows are written (inside its transaction): what live kept that stands in a place
    the new map hasn't and that no one decides about is let go, never removed with its people: a spawned NPC (each
    server's own) has each such place unset (doc 66, 2); patrol posts, faction claims and NPC areas (with their spawn
    rules; who wandered one wanders nowhere) there go. Returns the counts. (Every other NPC and profession slot in such
    a place was a conflict the user decided: resolve(), with the new map's areas. One still there is refused by
    finish(), naming it.)"""
    gone = 'NOT EXISTS (SELECT 1 FROM world.areas a WHERE a.world_id = t.world_id AND a.id = t.{})'
    counts = {}
    ids = set()
    for place in PLACED:
        ids |= {r[0] for r in conn.execute(
            f'UPDATE live.npcs t SET {place}_area = NULL, {place}_x = 0, {place}_y = 0 WHERE t.world_id = %s AND '
            f't.spawn_id IS NOT NULL AND t.{place}_area IS NOT NULL AND ' + gone.format(f'{place}_area') + ' RETURNING t.id',
            (world_id,)).fetchall()}
    counts['spawned'] = len(ids)
    for table, label in (('live.patrol_posts', 'posts'), ('live.faction_claims', 'claims'), ('live.npc_areas', 'npcAreas')):
        counts[label] = conn.execute(f'DELETE FROM {table} t WHERE t.world_id = %s AND ' + gone.format('area'),
                                     (world_id,)).rowcount
    return counts


def preview_let_go(items, dev_snap):
    """What a map-replacing import would let go of, from the plan: the NPCs and profession slots whose place the new map
    hasn't (each a conflict for the user: dev's place, unset, or a new place), and live's patrol posts, claims and NPC
    areas that stay as live has them and stand in places dev's map hasn't. ({counts}, [NPC names, at most 30])."""
    areas = map_areas(dev_snap)
    counts, names = {'npcs': 0, 'slots': 0, 'posts': 0, 'claims': 0, 'npcAreas': 0}, []
    for item in items:
        row = item.live
        table = item.spec.table
        if item.gone:
            counts['npcs' if table == 'live.npcs' else 'slots'] += 1
            if table == 'live.npcs' and len(names) < 30:
                names.append(str((row or {}).get('name') or item.key[0]))
            continue
        if item.side != 'live' or item.spec.part != 'live' or row is None:
            continue
        if table == 'live.patrol_routes':
            counts['posts'] += sum(1 for post in row.get('posts', []) if post.get('area') not in areas)
        elif table == 'live.faction_claims' and row.get('area') not in areas:
            counts['claims'] += 1
        elif table == 'live.npc_areas' and row.get('area') not in areas:
            counts['npcAreas'] += 1
    return counts, names


def map_areas(snap):
    """The cells' and interiors' ids of a snapshot's map."""
    return {k[0] for table in AREAS_OF for k in snap.get(table, {})}


def replacing(items, areas=None):
    """A map-replacing import's plan from the three-way one: the map (the world part) is dev's whole, and nothing live
    has is removed (a row dev lacks stays as live has it). An NPC or profession slot that would keep, as live has it (or
    as its conflict ends by default), a home, work or evening place the new map (`areas`) hasn't is a conflict needing a
    decision for each such place (doc 66, Progress, "The live world's truth"): never unplaced unasked. (Spawned NPCs,
    patrol posts, claims and NPC areas there are let go: let_go_of_old_map.)"""
    out = []
    for item in items:
        if item.spec.part == 'world':
            item = item._replace(side='dev')
        elif item.dev is None and item.live is not None:
            item = item._replace(side='live')
        if areas is not None and item.side in ('live', 'conflict') and item.spec.table in ('live.npcs', 'live.profession_slots'):
            row = outcome_row(item)
            gone = tuple(p for p in PLACED if row and row.get(f'{p}_area') is not None and row[f'{p}_area'] not in areas)
            if gone:
                item = item._replace(side='conflict', gone=gone)
        out.append(item)
    return out


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


def replacing_base(dev, live_key):
    """The base a map-replacing import compares live data against: the dev world's own base of this upstream if it
    has one (so live's changes since are live's), else None, for baseless() once both worlds are read. The header
    says which."""
    header, base = load_base(dev)
    if header is not None and header['upstream'] == live_key:
        return header, base
    return {'upstream': live_key, 'world': None, 'release': 0, 'takenAt': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'by': '', 'how': "none (live as it is now; where the live game's fields differ, not known)", 'rows': 0}, None


UNKNOWN = {'$base': 'none'}         # A field's base where none is known (baseless()): unlike any value of either side.


def baseless(snap_dev, snap_live) -> Snapshot:
    """The base of a map-replacing import whose dev world has none of its upstream (the real port: a new map made in
    DEV, imported into PROD, which was never DEV's upstream): live as it is now, but for each live game's field
    (world_store.game_owned: an NPC's home and post, what it carries, its age; a row the game made, whole) that differs
    on a row both have, there UNKNOWN: which side changed it can't be known. So an authored difference counts as dev's
    (DEV's words come across), and one of the game's is a conflict, live's by default (PROD keeps where its game put
    everyone, and what they carry) and dev's when chosen (DEV's generator re-homing someone on its new map); doc 66,
    Progress, "The live world's truth". A base of live as it is would make every difference dev's, and DEV's rows
    overwrite PROD's; one with dev's game fields would make each of them live's outright, never DEV's."""
    out = Snapshot({table: dict(rows) for table, rows in snap_live.items()})
    for table, rows in snap_live.items():
        if table not in SPECS or SPECS[table].part != 'live':
            continue
        theirs = snap_dev.get(table, {})
        for key, (_, row) in rows.items():
            found = theirs.get(key)
            if found is None:
                continue
            mixed = {c: UNKNOWN if (S.game_owned(table, c, row) or S.game_owned(table, c, found[1]))
                     and canonical(found[1].get(c)) != canonical(v) else v for c, v in row.items()}
            out[table][key] = (digest_of(mixed), mixed)
    return out


def same_map_or_refuse(dev, live, dev_key, live_key):
    """The two worlds' maps ({'dev', 'live', 'same'}); refuses (DifferentMap) when they differ: a pull, an Update from
    live, compares the same map only."""
    maps = maps_of(dev, live)
    if not maps['same']:
        raise DifferentMap(different_maps_text(maps, dev_key, live_key), maps)
    return maps


def plan(dev, live, dev_key, live_key, replace=False):
    """(plan JSON, items, snapshots) of a dev world against its upstream. On one map only, unless `replace` (an
    import's plan): then two maps give the map-replacing plan, against the base or, with none, live as it is."""
    maps = maps_of(dev, live)
    if not maps['same'] and not replace:
        raise DifferentMap(different_maps_text(maps, dev_key, live_key), maps)
    snap_live, live_id, release = read_live(live)
    with dev.transaction():
        dev.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')
        if maps['same']:
            header, base = base_or_refuse(dev, dev_key, live_key)
        else:
            header, base = replacing_base(dev, live_key)
        dev_id = the_world(dev, 'The dev world')
        snap_dev = snapshot(dev, dev_id)
    if base is None:
        base = baseless(snap_dev, snap_live)
        header['rows'] = base.count()
    unshared(dev, live, base, snap_dev, snap_live)
    world_id = same_world(dev_id, live_id)
    items = three_way(base, snap_dev, snap_live)
    if not maps['same']:
        items = replacing(items, map_areas(snap_dev))
    followed = follows(items)
    by_side = {s: [describe(i, followed) for i in items if i.side == s] for s in ('live', 'dev', 'conflict')}
    info = {'dev': dev_key, 'live': live_key, 'world': world_id, 'base': header, 'liveRelease': release,
            'counts': {'live': len(by_side['live']), 'dev': len(by_side['dev']), 'conflicts': len(by_side['conflict'])},
            'summary': {'live': summarize(items, lambda its: [(i, i.base, i.live) for i in its if i.side == 'live']),
                        'dev': summarize(items, lambda its: [(i, i.base, i.dev) for i in its if i.side == 'dev'])},
            'liveChanges': by_side['live'], 'devChanges': by_side['dev'], 'conflicts': by_side['conflict'],
            'digest': plan_digest(items), 'maps': maps, 'replacesMap': not maps['same'],
            # (Conflicts taken as their default, the live game's truth, unless the user chooses: doc 66, Progress.)
            'defaults': {i.id: default_choice(i) for i in items if i.side == 'conflict' and default_choice(i) is not None}}
    if not maps['same']:
        counts, names = preview_let_go(items, snap_dev)
        info['letGo'] = {**counts, 'names': names}
        if any(i.gone for i in items):
            # The new map's places, for the user to pick a new home or post from.
            info['areas'] = sorted(({'id': k[0], 'name': row.get('name') or k[0], 'kind': AREAS_OF[table],
                                     'width': row.get('width'), 'height': row.get('height')}
                                    for table in AREAS_OF for k, (_, row) in snap_dev.get(table, {}).items()),
                                   key=lambda a: (a['kind'], str(a['name']).lower(), a['id']))
    info['economy'] = economy_kept(dev, live, dev_id, live_id)
    return info, items, (base, snap_dev, snap_live, world_id, release, header)


def economy_kept(dev, live, dev_id, live_id):
    """The live world's economy settings (live.economy: its rules, wages, toll, treasury's start, the store room's
    switches) where they differ from the dev world's: never synced, each world's own, so live keeps them whatever dev
    has (the live game's truth, doc 66, Progress). {'fields': [column], 'dev': {column: value}, 'live': {...}}."""
    try:
        d = read(dev, 'live.economy', dev_id, skip=NOT_CONTENT) if dev_id else {}
        l = read(live, 'live.economy', live_id, skip=NOT_CONTENT) if live_id else {}
    except psycopg.Error:
        return {'fields': [], 'dev': {}, 'live': {}}
    d, l = next(iter(d.values()), {}), next(iter(l.values()), {})
    fields = [c for c in sorted(set(d) & set(l)) if c != 'world_id' and canonical(normalized({c: d[c]})) != canonical(normalized({c: l[c]}))]
    return {'fields': fields, 'dev': {c: shown(normalized({c: d[c]})[c]) for c in fields},
            'live': {c: shown(normalized({c: l[c]})[c]) for c in fields}}


def check_expected(info, expect):
    if expect and expect != info['digest']:
        raise Moved('The worlds changed since this plan was made: look at the new plan and choose again.')


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
                    'server': server_state(conn), 'serverRunning': server_running(conn), 'serverHolder': server_holder(conn), 'base': header,
                    'release': release_of(conn, world_id), 'map': map_identity(conn, entry['kind'], world_id)}

    def diff(self, dev_key):
        """The three-way plan (Atlas's Update from live shows it; warns while the live server runs). Refused
        (DifferentMap) when the two worlds are on different maps."""
        dev_key, live_key = pair(dev_key)
        with self.connect(dev_key) as dev, self.connect(live_key) as live:
            info, _, _ = plan(dev, live, dev_key, live_key)
            running = server_running(live) or server_state(live)['state'] == 'running'
        info['warnings'] = ['The live server is running: live data may still move after this; the import compares again.'] if running else []
        return info

    def plan_import(self, dev_key):
        """What Import from dev would do, and what blocks it now (maintenance, a running server, the dev world's problems).
        Two maps give the map-replacing plan (`replacesMap`, `maps`, `letGo`): blocked while live lacks migrations dev
        has; the import itself needs the replace-the-map confirmation."""
        dev_key, live_key = pair(dev_key)
        with self.connect(dev_key) as dev, self.connect(live_key) as live:
            info, _, _ = plan(dev, live, dev_key, live_key, replace=True)
            _, revision, errors, warnings = validate_dev(dev)
            state, running = server_state(live), server_running(live)
            history = releases(live) if info['world'] else []
            lacking = behind(dev, live) if info['replacesMap'] else []
        blocked = []
        if info['replacesMap']:
            warnings = [maps_differ_text(info['maps'], dev_key, live_key) + ': importing replaces '
                        f'{live_key.upper()}\'s map with {dev_key.upper()}\'s.', *warnings]
            if lacking:
                blocked.append(f'{live_key.upper()} lacks migrations {dev_key.upper()} has ({"; ".join(lacking)}): a new map needs '
                               f'them all. Migrating {live_key.upper()} is the user\'s decision (python3 tools/world_db.py migrate '
                               f'--db {live_key}); nothing is imported until then.')
        if state['state'] != 'maintenance':
            blocked.append(f'{live_key} is not in maintenance: close it first (the DM, or python3 tools/world_sync.py maintenance {live_key} on).')
        elif running:
            blocked.append(f'{live_key}\'s game server has not stopped yet (it closes within a minute of maintenance).')
        if info['economy']['fields']:
            warnings = [*warnings, f'{dev_key.upper()}\'s economy settings differ from {live_key.upper()}\'s '
                        f'({", ".join(info["economy"]["fields"][:8])}{"..." if len(info["economy"]["fields"]) > 8 else ""}): '
                        f'{live_key.upper()} keeps its own (the live game\'s; never imported).']
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
            same_map_or_refuse(dev, live, dev_key, live_key)       # (One map only: doc 66, "The same map".)
            snap_live, live_id, release = read_live(live)
            with dev.transaction():
                lock_world(dev)
                header, base = base_or_refuse(dev, dev_key, live_key)
                dev_id = the_world(dev, 'The dev world')
                world_id = same_world(dev_id, live_id)
                snap_dev = snapshot(dev, dev_id)
                unshared(dev, live, base, snap_dev, snap_live)
                items = three_way(base, snap_dev, snap_live)
                check_expected({'digest': plan_digest(items)}, expect)
                chosen = resolve(items, choices)
                held, back = with_their_npcs(items, chosen, 'dev')     # (An NPC dev keeps keeps what hangs on it.)
                changes = [(i.spec, i.key, i.live) for i in items if i.side == 'live' and i.id not in held]
                changes += [(i.spec, i.key, chosen[i.id]) for i in items if i.side == 'conflict' and
                            canonical(chosen[i.id]) != canonical(i.dev)]
                changes += back
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
                'conflicts': sum(1 for i in items if i.side == 'conflict'), 'kept': sum(1 for i in items if i.side == 'dev') - len(back),
                'defaulted': defaulted(items, choices),
                'warnings': ['The live server is running: live data may still move; the import compares again.'] if running else []}

    # -- import ------------------------------------------------------------------
    def import_(self, dev_key, choices=None, password=None, editor='', note='', expect=None, replace_map=False):
        """The DM's Import from dev: during maintenance only; one transaction on live and a release; then dev is
        brought level with live and takes it as its base. A dev world on another map replaces live's map only with
        `replace_map` (the confirmation, on top of the password) and only while live has every migration dev has; live
        data is never removed (what stood on the old map is let go: let_go_of_old_map)."""
        dev_key, live_key = pair(dev_key)
        editor = str(editor or 'Someone')[:60]
        with self.connect(dev_key) as dev, self.connect(live_key) as live:
            state = server_state(live)
            if state['state'] != 'maintenance':
                raise NotInMaintenance(f'{live_key} is not in maintenance. Close it first: its players are told and its game '
                                       f'server stops (python3 tools/world_sync.py maintenance {live_key} on).')
            maps = maps_of(dev, live)
            if not maps['same']:
                if replace_map is not True:
                    raise DifferentMap(f'{maps_differ_text(maps, dev_key, live_key)}. Importing replaces '
                                       f'{live_key.upper()}\'s map with {dev_key.upper()}\'s: confirm it (replace the map) to go on.',
                                       maps)
                lacking = behind(dev, live)
                if lacking:
                    raise PublishError(f'{live_key.upper()} lacks migrations {dev_key.upper()} has: a new map needs them all. '
                                       f'Migrating {live_key.upper()} is the user\'s decision (python3 tools/world_db.py migrate '
                                       f'--db {live_key}).', lacking)
            check_password(live, password, editor, 'import')
            with dev.transaction():
                dev.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')     # One consistent copy of dev.
                if maps['same']:
                    header, base = base_or_refuse(dev, dev_key, live_key)
                else:
                    header, base = load_base(dev)
                    if header is None or header['upstream'] != live_key:
                        base = None                       # (Live as it is, read in the import's own transaction.)
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
                if map_identity(live, 'live', live_id)['id'] != maps['live']['id']:
                    raise Moved('The live world\'s map changed since: look at the new plan and choose again.')
                snap_live = snapshot(live, live_id)
                if base is None:
                    base = baseless(snap_dev, snap_live)          # (Live as it is; the game's fields that differ, not known.)
                missing = unshared(dev, live, base, snap_dev, snap_live)
                items = three_way(base, snap_dev, snap_live)
                areas = None if maps['same'] else map_areas(snap_dev)
                if areas is not None:
                    items = replacing(items, areas)
                check_expected({'digest': plan_digest(items)}, expect)
                chosen = resolve(items, choices, areas)
                held, back = with_their_npcs(items, chosen, 'live')    # (An NPC live keeps keeps what hangs on it.)
                changes = [(i.spec, i.key, i.dev) for i in items if i.side == 'dev' and i.id not in held]
                changes += [(i.spec, i.key, chosen[i.id]) for i in items if i.side == 'conflict' and
                            canonical(chosen[i.id]) != canonical(i.live)]
                changes += back
                if not changes:
                    if not any(i.side in ('dev', 'conflict') for i in items):
                        raise PublishError('There is nothing to import: live already has every change made in dev.')
                    # (Dev's changes all end as live has them: its conflicts on live's side, what hangs on an NPC live
                    # keeps kept. The pull brings dev level, with them.)
                    raise PublishError('There is nothing to import: every change made in dev ends as live has it (the '
                                       'conflicts on live\'s side, the live game\'s truth, and what hangs on an NPC live '
                                       'keeps). Update from live brings dev level with it.')
                apply_rows(live, world_id, changes, snap_live)
                let_go = None if maps['same'] else let_go_of_old_map(live, world_id)
                if let_go is not None:
                    # (Unplaced by the user's decision, each, where the new map hasn't their place.)
                    unset = [i for i in items if i.gone and chosen.get(i.id) is not None
                             and any(chosen[i.id].get(f'{p}_area') is None for p in i.gone)]
                    let_go.update(npcs=sum(1 for i in unset if i.spec.table == 'live.npcs'),
                                  slots=sum(1 for i in unset if i.spec.table == 'live.profession_slots'))
                finish(live, world_id)
                summary = counts_of(changes, snap_live)
                # The release records the map live is on now (the dev world's): a live world's identity (map_identity).
                recorded = {**summary, 'map': {k: maps['dev'][k] for k in ('id', 'name', 'made', 'by', 'how')}}
                if let_go is not None:
                    recorded['replacedMap'] = {k: maps['live'][k] for k in ('id', 'name', 'made', 'by', 'how')}
                    recorded['letGo'] = let_go
                    summary = {**summary, 'letGo': let_go}
                number = record(live, world_id, editor, note, 'import', dev_content(live, world_id), recorded)
                after = snapshot(live, world_id)              # (After the build: it saves the roster's assignments.)
                unshared(dev, live, after)
            level = self._level(dev, dev_key, live_key, world_id, number, base, snap_dev, after, editor, missing)
        return {'release': number, 'summary': summary, 'conflicts': sum(1 for i in items if i.side == 'conflict'), 'dev': level,
                'replacedMap': not maps['same'], 'map': maps['dev'], 'defaulted': defaulted(items, choices)}

    def _level(self, dev, dev_key, live_key, world_id, number, base, planned, after, editor, missing=()):
        """After an import: dev takes live's version of every row nobody edited in dev meanwhile, and the base
        follows. A row edited in dev during the import keeps its edit and its old base (so it is still compared
        three ways next time)."""
        try:
            with dev.transaction():
                lock_world(dev)
                now = snapshot(dev, world_id)
                for table in missing:                        # (A table one side lacks: left as it is.)
                    now[table] = {}
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
        server stopped). The new one is put in maintenance too, so the launcher waits until the DM opens it. It may be a
        dev world, served as the live server (DEV while PROD is closed: doc 66, Progress, "A dev world as the live
        server")."""
        target = world_db.resolve(key)
        current = world_db.canonical(world_db.local_choice()['live'])
        if current == target:                  # (Already the live one: nothing to switch, nothing closed.)
            world_db.set_local(live=target)
            return {'live': target, 'was': current, **{k: v for k, v in self.status(target).items() if k in ('server', 'name')}}
        if current in world_db.keys() and current != target:
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


def import_(dev_key, choices=None, password=None, editor='', note='', connect=None, expect=None, replace_map=False):
    return Sync(connect).import_(dev_key, choices, password, editor, note, expect, replace_map)


def stamp(key, name, editor='', how='new'):
    """`map KEY --new`: a world's new map identity, as its owner."""
    with world_db.connect(world_db.resolve(key), 'owner') as conn:
        with conn.transaction():
            return stamp_map(conn, name, editor, how)


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
    maps = info.get('maps')
    if maps:
        print(f'  map: {maps["dev"]["name"]} ({maps["dev"]["id"]}) in dev, {maps["live"]["name"]} ({maps["live"]["id"]}) live'
              + ('' if maps['same'] else ': DIFFERENT MAPS, an import replaces live\'s map (--replace-map)'))
    if info.get('letGo'):
        g = info['letGo']
        print(f'  let go with the old map: {g["npcs"]} NPCs and {g["slots"]} profession slots unplaced, {g["posts"]} patrol '
              f'posts, {g["claims"]} claims and {g["npcAreas"]} NPC areas' + (f' (NPCs: {", ".join(g["names"][:10])}...)' if g['names'] else ''))
    print(f'  dev changed ({info["counts"]["dev"]}): taken by an import\n{counts_line(info["summary"]["dev"])}')
    print(f'  conflicts ({info["counts"]["conflicts"]}):')
    for c in info['conflicts']:
        default = c.get('default')
        print(f'    {c["id"]} ({c["name"]}): dev {c["dev"]}, live {c["live"]}; '
              + ('needs a choice' if default is None else "default: live's (the game's truth)" if default == 'live'
                 else f'default: field by field, {", ".join(f"{k} live" for k in default) or "each field the side that changed it"}'))
        for f in c['fields'][:12]:
            print(f'      {f["field"]}: base {short(f["base"])} | dev {short(f["dev"])} | live {short(f["live"])}'
                  + (' (the game\'s)' if f.get('owner') == 'game' else ''))
        for g in c.get('gone', []):
            print(f'      {g["place"]} in {g["area"]}, not on the new map: "unplace", dev\'s ({short(g["dev"])}) or a new place')
    if info.get('economy', {}).get('fields'):
        print(f'  economy settings kept live (never imported): {", ".join(info["economy"]["fields"])}')
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
    p.add_argument('--replace-map', action='store_true',
                   help="the dev world is on another map: replace the live world's map with it (live data is kept)")
    p = sub.add_parser('map', help="a world's map identity (--new: stamp a new one, as the database owner)")
    p.add_argument('key')
    p.add_argument('--new', action='store_true', help='the map was made new: give the world a new identity')
    p.add_argument('--name', default='', help='with --new: what the map is called (e.g. "Ridgemere 2.0: the city")')
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
                print(f'  map: {info["map"]["name"]} ({info["map"]["id"]})')
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
            result = sync.import_(args.dev, choices, password, me, args.note, None, args.replace_map)
            print(f'Release {result["release"]} is live.\n{counts_line(result["summary"])}')
            if result['dev'].get('error'):
                print(result['dev']['error'], file=sys.stderr)
        elif args.command == 'map':
            key = world_db.resolve(args.key)
            if args.new:
                if not args.name.strip():
                    raise PublishError('Name the new map: --name "..."')
                ident = stamp(key, args.name.strip(), me)
                print(f'{key}: a new map identity, {ident["name"]} ({ident["id"]}). Pull and import with any world on '
                      'another map are refused (an import into live replaces its map).')
            else:
                ident = sync.status(key)['map']
                print(f'{key}: {ident["name"]} ({ident["id"]}; from its {ident["from"]}'
                      + (f' {ident["release"]}' if ident.get('release') else '') + ')')
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
