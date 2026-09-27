#!/usr/bin/env python3
"""Push to live: copy the world from DEV to PROD, as a numbered release.

  python3 tools/publish.py preview                 what a push would change
  python3 tools/publish.py push --note "..."       asks for the publish password
  python3 tools/publish.py releases                release history
  python3 tools/publish.py rollback N --note "..." put release N's world back live
  python3 tools/publish.py pull                    copy PROD's NPCs and their state into DEV

The editor's Push to live button uses the same functions. A push:
  1. validates the DEV world completely (the same checks as Play/Export);
  2. makes PROD's authored world content (terrain, cells, interiors, doors,
     places, resources, factions, chapters) match DEV exactly;
  3. adds live-data seeds without ever overwriting live NPCs: new NPCs,
     profession slots and roster characters are added, and ones already in PROD
     are left exactly as they are (players, events and deaths change them there);
     patrol routes follow DEV, except ones live NPCs walk are never removed; the
     economy is only created if missing; the profession catalog follows DEV;
  4. refuses if live data in PROD would be left standing in a removed place;
  5. records the release with exactly what it published, compiles it into a
     game build (world.builds), and notifies the game server (NOTIFY
     ratw_release) when it commits.
Rollback re-publishes an earlier release's world content through steps 2, 4, 5.
"""
from __future__ import annotations

import argparse
import base64
import datetime
import getpass
import json
import sys

import psycopg

import map_editor
import live_edit as L
import world_build
import world_db
import world_store as S

AUTHORED = ['world.worlds', 'world.areas', 'world.terrain_chunks', 'world.cells', 'world.interiors', 'world.links',
            'world.resources', 'world.places', 'world.chapters', 'world.minimaps']
LABELS = {'world.worlds': 'world settings', 'world.areas': None, 'world.terrain_chunks': 'terrain blocks',
          'world.cells': 'cells', 'world.interiors': 'interiors', 'world.links': 'connections', 'world.resources': 'resources',
          'world.places': 'places', 'world.chapters': 'Chapters', 'world.minimaps': None}
MAX_FAILURES, LOCK_MINUTES = 5, 15
# Bookkeeping that is not content: DEV's edit counter moves on every edit, timestamps on every write.
NOT_CONTENT = ('updated_at', 'revision')
NOTIFY_CHANNEL = 'ratw_release'
LIVE_KEPT = 'kept (live version wins)'
PUBLISH_LOCK = 0x52415457          # Advisory lock key ("RATW") held by a push or rollback.


class PublishError(S.StoreError):
    status = 422

    def __init__(self, message, errors=None):
        super().__init__(message)
        self.errors = errors or []


class WrongPassword(PublishError):
    status = 401


class Locked(PublishError):
    status = 429


# --------------------------------------------------------------------------- Tables

_shapes = {}


def shape(conn, table):
    """(columns, key columns, {column: type}) of a table, from the catalog."""
    if table not in _shapes:
        schema, name = table.split('.')
        cols = conn.execute('''SELECT column_name, data_type FROM information_schema.columns
                               WHERE table_schema = %s AND table_name = %s ORDER BY ordinal_position''', (schema, name)).fetchall()
        keys = [r[0] for r in conn.execute('''SELECT a.attname FROM pg_index i
            JOIN pg_attribute a ON a.attrelid = i.indrelid AND a.attnum = ANY(i.indkey)
            WHERE i.indrelid = %s::regclass AND i.indisprimary ORDER BY array_position(i.indkey, a.attnum)''', (table,)).fetchall()]
        _shapes[table] = ([c for c, _ in cols], keys, dict(cols))
    return _shapes[table]


def world_column(table):
    return 'id' if table == 'world.worlds' else 'world_id'


def read(conn, table, world_id, skip=()):
    """{key: row dict} of one world's rows; `skip` columns are left out (e.g. timestamps)."""
    columns, keys, _ = shape(conn, table)
    columns = [c for c in columns if c not in skip]
    cur = conn.execute(f'SELECT {", ".join(columns)} FROM {table} WHERE {world_column(table)} = %s', (world_id,))
    return {tuple(r[columns.index(k)] for k in keys): dict(zip(columns, r)) for r in cur.fetchall()}


def write(conn, table, row):
    _, keys, types = shape(conn, table)
    columns = list(row)
    values = [S.Jsonb(row[c]) if types[c] == 'jsonb' and row[c] is not None else row[c] for c in columns]
    rest = [c for c in columns if c not in keys]
    conn.execute(f'INSERT INTO {table} ({", ".join(columns)}) VALUES ({", ".join(["%s"] * len(columns))}) '
                 f'ON CONFLICT ({", ".join(keys)}) DO ' + (f'UPDATE SET {", ".join(f"{c} = excluded.{c}" for c in rest)}' if rest else 'NOTHING'),
                 values)


def delete(conn, table, key):
    _, keys, _ = shape(conn, table)
    conn.execute(f'DELETE FROM {table} WHERE ' + ' AND '.join(f'{k} = %s' for k in keys), key)


# Release content is stored as JSON: bytes and timestamps need a round trip.
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


# --------------------------------------------------------------------------- Planning

def the_world(conn, label):
    rows = conn.execute('SELECT id FROM world.worlds').fetchall()
    if len(rows) > 1:
        raise PublishError(f'{label} holds more than one world.')
    return rows[0][0] if rows else None


def plan_world(prod, content, world_id):
    """Deletes and upserts that make PROD's authored content equal `content` ({table: {key: row}})."""
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


def dev_content(dev, world_id):
    return {table: read(dev, table, world_id, skip=NOT_CONTENT) for table in AUTHORED}


def plan_seeds(dev, prod, world_id):
    """Live-data seeds: what a push adds, updates, removes and leaves alone in PROD, in a safe order."""
    plan, summary = [], {}
    # The roster is shared by the whole world and authored in the editor; jobs refer to it.
    # Professions are a catalog and follow DEV; characters are people, and once live they belong to PROD.
    for table, label in (('live.professions', 'professions'), ('live.characters', 'roster characters')):
        columns = [c for c in shape(dev, table)[0] if c != 'updated_at']
        rows_d = {r[0]: dict(zip(columns, r)) for r in dev.execute(f'SELECT {", ".join(columns)} FROM {table}').fetchall()}
        rows_p = {r[0]: dict(zip(columns, r)) for r in prod.execute(f'SELECT {", ".join(columns)} FROM {table}').fetchall()}
        new = [k for k in rows_d if k not in rows_p]
        if table == 'live.professions':
            updated = [k for k in rows_d if k in rows_p and rows_d[k] != rows_p[k]]
            plan += [('write', table, rows_d[k]) for k in new + updated]
            summary[label] = {'added': len(new), 'updated': len(updated)}
        else:
            plan += [('write', table, rows_d[k]) for k in new]
            kept = sum(1 for k in rows_p if k not in rows_d or rows_d[k] != rows_p[k])
            summary[label] = {'added': len(new), LIVE_KEPT: kept}
    # NPC layers (routes, painted areas, spawn rules) are run by the Dungeon Master once live: a push adds new ones
    # only. Areas before spawns before NPCs, which refer to them.
    routes_d, routes_p = read(dev, 'live.patrol_routes', world_id), read(prod, 'live.patrol_routes', world_id)
    posts_d = read(dev, 'live.patrol_posts', world_id)
    new_routes = [k for k in routes_d if k not in routes_p]
    for key in new_routes:
        plan.append(('write', 'live.patrol_routes', routes_d[key]))
        plan += [('write', 'live.patrol_posts', row) for pk, row in sorted(posts_d.items()) if pk[1] == key[1]]
    summary['patrol routes'] = {'added': len(new_routes), LIVE_KEPT: sum(
        1 for k in routes_p if k not in routes_d or routes_d[k] != routes_p[k])}
    for table, label in (('live.npc_areas', 'NPC areas'), ('live.spawns', 'spawn rules')):
        d, p_ = read(dev, table, world_id, skip=('updated_at',)), read(prod, table, world_id, skip=('updated_at',))
        new = [k for k in d if k not in p_]
        plan += [('write', table, d[k]) for k in new]
        summary[label] = {'added': len(new), LIVE_KEPT: sum(1 for k in p_ if k not in d or d[k] != p_[k])}
    # Factions and their territory claims: the Dungeon Master runs them once live; a push adds new ones only.
    factions_p = read(prod, 'live.factions', world_id, skip=('updated_at',))
    for table, label in (('live.factions', 'factions'), ('live.faction_claims', 'territory claims')):
        d, p_ = read(dev, table, world_id, skip=('updated_at',)), read(prod, table, world_id, skip=('updated_at',))
        new = [k for k in d if k not in p_]
        plan += [('write', table, d[k]) for k in new]
        summary[label] = {'added': len(new), LIVE_KEPT: sum(1 for k in p_ if k not in d or d[k] != p_[k])}
    # Named NPCs and profession slots: once live they belong to PROD. Player interactions, events and deaths
    # happen there, so a push only ever adds new ones; DEV's edits to live ones, and removals, stay in DEV.
    for table, label in (('live.npcs', 'NPCs'), ('live.profession_slots', 'profession slots')):
        d = read(dev, table, world_id, skip=('updated_at',))
        p = read(prod, table, world_id, skip=('updated_at',))
        if table == 'live.npcs':
            # NPCs a DEV server spawned belong to that server; PROD's spawn rules make their own.
            d = {k: row for k, row in d.items() if not row.get('spawn_id')}
        counts = {'added': 0, LIVE_KEPT: 0}
        for key, row in d.items():
            if key not in p:
                plan.append(('write', table, {**row, 'origin': 'authored'})); counts['added'] += 1
            elif {**row, 'origin': p[key]['origin']} != p[key]:
                counts[LIVE_KEPT] += 1
        counts[LIVE_KEPT] += sum(1 for key in p if key not in d)
        summary[label] = counts
        if table == 'live.npcs':
            npcs_after = set(p) | set(d)
    # Faction members and relations: new ones only, and only between factions and NPCs that PROD will have.
    factions_after = set(factions_p) | set(read(dev, 'live.factions', world_id))
    for table, label, refs in (('live.faction_members', 'faction members', lambda r: [(r['world_id'], r['faction_id'])],),
                               ('live.faction_relations', 'faction relations',
                                lambda r: [(r['world_id'], r['faction_id']), (r['world_id'], r['other_id'])])):
        d, p_ = read(dev, table, world_id, skip=('updated_at',)), read(prod, table, world_id, skip=('updated_at',))
        new = [k for k, row in d.items() if k not in p_ and all(f in factions_after for f in refs(row))
               and (table != 'live.faction_members' or (world_id, row['npc_id']) in npcs_after)]
        plan += [('write', table, d[k]) for k in new]
        summary[label] = {'added': len(new), LIVE_KEPT: sum(1 for k in p_ if k not in d or d[k] != p_[k])}
    # The economy is live state: a push only creates it.
    if not read(prod, 'live.economy', world_id):
        plan += [('write', 'live.economy', row) for row in read(dev, 'live.economy', world_id, skip=('updated_at',)).values()]
        summary['town economy'] = {'created': 1}
    return plan, summary


def validate_dev(dev):
    """Every problem that would block Play/Export blocks a push too."""
    world_id = the_world(dev, 'DEV')
    if not world_id:
        raise PublishError('DEV has no world to publish.')
    project, revision = S.load_world(dev, world_id)
    try:
        _, warnings = map_editor.check_project(project, for_game=False)     # The game server streams: no size limit.
        map_editor.export_files(project, S.load_roster(dev), stream=True)   # What the live server loads.
    except map_editor.ValidationError as error:
        return world_id, revision, error.errors, []
    return world_id, revision, [], warnings


def preview(dev, prod):
    world_id, revision, errors, warnings = validate_dev(dev)
    live_id = the_world(prod, 'PROD')
    if live_id and live_id != world_id:
        errors = errors + [f'PROD holds a different world ({live_id}) than DEV ({world_id}).']
    _, world = plan_world(prod, dev_content(dev, world_id), world_id)
    _, seeds = plan_seeds(dev, prod, world_id)
    changed = something(world, seeds)
    return {'world': world_id, 'devRevision': revision, 'release': latest_release(prod, world_id), 'errors': errors,
            'warnings': warnings, 'changed': changed, 'summary': {'world': world, 'live': seeds}}


def something(*summaries):
    """Whether a push would change anything (things kept because the game changed them do not count)."""
    return any(n for part in summaries for counts in part.values() for k, n in counts.items() if not k.startswith('kept'))


def latest_release(prod, world_id):
    return prod.execute('SELECT coalesce(max(number), 0) FROM admin.releases WHERE world_id = %s', (world_id,)).fetchone()[0]


# --------------------------------------------------------------------------- Password

def check_password(prod, password, editor, action):
    """Verifies the publish password; after 5 failures in 15 minutes pushes are locked for the rest of that window."""
    failures = prod.execute(f'''SELECT count(*), min(at) FROM admin.publish_attempts WHERE NOT ok
        AND at > now() - interval '{LOCK_MINUTES} minutes'
        AND at > coalesce((SELECT max(at) FROM admin.publish_attempts WHERE ok), '-infinity')''').fetchone()
    if failures[0] >= MAX_FAILURES:
        wait = LOCK_MINUTES - int((datetime.datetime.now(datetime.timezone.utc) - failures[1]).total_seconds() // 60)
        raise Locked(f'Too many wrong passwords. Pushing is locked for about {max(1, wait)} more minute{"s" if wait != 1 else ""}.')
    ok = isinstance(password, str) and world_db.verify_publish_password(prod, password)
    prod.execute('INSERT INTO admin.publish_attempts (editor, action, ok) VALUES (%s, %s, %s)', (editor[:60], action, ok))
    if not ok:
        left = MAX_FAILURES - failures[0] - 1
        raise WrongPassword('Wrong publish password.' + (f' {left} more tr{"ies" if left != 1 else "y"} before pushing locks.' if left < 3 else ''))


# --------------------------------------------------------------------------- Publishing

def orphans(prod, world_id):
    """Live data in PROD that would stand in a place that no longer exists."""
    found = []
    for table, what in (('live.npcs', 'NPC'), ('live.profession_slots', 'profession slot')):
        for id_, name, area in prod.execute(f'''SELECT t.id, t.name, a.area FROM {table} t,
                LATERAL (VALUES (t.home_area), (t.work_area), (t.evening_area)) AS a(area)
                WHERE t.world_id = %s AND NOT EXISTS (SELECT 1 FROM world.areas w WHERE w.world_id = t.world_id AND w.id = a.area)''',
                                            (world_id,)).fetchall():
            found.append(f'{what} {name} ({id_}) is placed in {area}, which this release removes.')
    for route, area in prod.execute('''SELECT DISTINCT p.route_id, p.area FROM live.patrol_posts p WHERE p.world_id = %s
            AND NOT EXISTS (SELECT 1 FROM world.areas w WHERE w.world_id = p.world_id AND w.id = p.area)''', (world_id,)).fetchall():
        found.append(f'Patrol route {route} has a post in {area}, which this release removes.')
    for id_, name, area in prod.execute('''SELECT a.id, a.name, a.area FROM live.npc_areas a WHERE a.world_id = %s
            AND NOT EXISTS (SELECT 1 FROM world.areas w WHERE w.world_id = a.world_id AND w.id = a.area)''', (world_id,)).fetchall():
        found.append(f'NPC area {name} ({id_}) lies in {area}, which this release removes.')
    for faction, area in prod.execute('''SELECT c.faction_id, c.area FROM live.faction_claims c WHERE c.world_id = %s
            AND NOT EXISTS (SELECT 1 FROM world.areas w WHERE w.world_id = c.world_id AND w.id = c.area)''', (world_id,)).fetchall():
        found.append(f'Faction {faction} claims {area}, which this release removes.')
    return sorted(set(found))


def apply_world(prod, plan):
    for table in reversed(AUTHORED):
        for key in plan[table][0]:
            delete(prod, table, key)
    for table in AUTHORED:
        for row in plan[table][1]:
            write(prod, table, row)


def record(prod, world_id, editor, note, kind, content, summary, restored=None):
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


def finish(prod, world_id):
    """Checks everything that is only settled at commit, with messages an author can act on."""
    lost = orphans(prod, world_id)
    if lost:
        raise PublishError('Live data in PROD would be left in removed places. Move or remove it in DEV first.', lost)
    try:
        prod.execute('SET CONSTRAINTS ALL IMMEDIATE')
    except psycopg.errors.IntegrityError as error:
        raise PublishError(f'PROD refused the release: {error.diag.message_primary}') from error


def push(dev, prod, password, editor, note=''):
    """Publishes DEV to PROD after checking the password. Returns the release number and summary."""
    check_password(prod, password, editor, 'push')
    with dev.transaction():
        dev.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')     # One consistent copy of DEV.
        world_id, _, errors, _ = validate_dev(dev)
        if errors:
            raise PublishError('The DEV world has problems that block publishing.', errors)
        content = dev_content(dev, world_id)
        with prod.transaction():
            live_id = the_world(prod, 'PROD')
            if live_id and live_id != world_id:
                raise PublishError(f'PROD holds a different world ({live_id}) than DEV ({world_id}).')
            prod.execute('SELECT pg_advisory_xact_lock(%s)', (PUBLISH_LOCK,))     # One push or rollback at a time.
            plan, world_summary = plan_world(prod, content, world_id)
            seeds, seed_summary = plan_seeds(dev, prod, world_id)
            if not something(world_summary, seed_summary):
                raise PublishError('PROD already matches DEV; there is nothing to push.')
            apply_world(prod, plan)
            for action, table, row in seeds:
                if action == 'write':
                    write(prod, table, row)
                else:
                    delete(prod, table, row)
            finish(prod, world_id)
            summary = {'world': world_summary, 'live': seed_summary}
            number = record(prod, world_id, editor, note, 'push', content, summary)
    return {'release': number, 'summary': summary}


def rollback(prod, number, password, editor, note=''):
    """Puts release `number`'s world content back live, as a new release. Live data is left as it is."""
    check_password(prod, password, editor, 'rollback')
    with prod.transaction():
        world_id = the_world(prod, 'PROD')
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


# --------------------------------------------------------------------------- Live NPC state into DEV

def pull_npcs(dev, prod, editor='Live copy'):
    """Makes DEV's NPCs match PROD's: named NPCs, profession slots, roster characters, economy and running state.

    DEV-only NPCs (not pushed yet) are left alone. Named NPCs and slots go through the live-edit log, so open
    editors see them change; an NPC whose home, work place or route does not exist in DEV is skipped and listed.
    """
    world_id = the_world(prod, 'PROD')
    if not world_id:
        raise PublishError('PROD has no world yet; push one first.')
    if the_world(dev, 'DEV') != world_id:
        raise PublishError(f'DEV holds a different world than PROD ({world_id}).')
    rows = lambda conn, sql: S.dict_rows(conn, sql, (world_id,))
    areas = {r['id'] for r in rows(dev, 'SELECT id FROM world.areas WHERE world_id = %s')}
    # NPC layers first (NPCs refer to them): painted areas and spawn rules directly, routes with the NPCs below.
    layer_copies = {}
    with dev.transaction():
        for table in ('live.npc_areas', 'live.spawns'):
            mine, theirs = read(dev, table, world_id, skip=('updated_at',)), read(prod, table, world_id, skip=('updated_at',))
            known = areas if table == 'live.npc_areas' else {r['id'] for r in rows(dev, 'SELECT id FROM live.npc_areas WHERE world_id = %s')}
            changed = [row for row in theirs.values() if mine.get((row['world_id'], row['id'])) != row
                       and row['area' if table == 'live.npc_areas' else 'area_id'] in known]
            for row in changed:
                write(dev, table, row)
            layer_copies[table] = len(changed)
    route_rows = {r['id']: r for r in rows(prod, 'SELECT id, name FROM live.patrol_routes WHERE world_id = %s ORDER BY position')}
    posts = {}
    for r in rows(prod, 'SELECT route_id, area, x, y FROM live.patrol_posts WHERE world_id = %s ORDER BY route_id, seq'):
        posts.setdefault(r['route_id'], []).append({'cell': r['area'], 'x': r['x'], 'y': r['y']})
    dev_routes = {r['id']: {'id': r['id'], 'name': r['name'], 'posts': []} for r in rows(dev, 'SELECT id, name FROM live.patrol_routes WHERE world_id = %s')}
    for r in rows(dev, 'SELECT route_id, area, x, y FROM live.patrol_posts WHERE world_id = %s ORDER BY route_id, seq'):
        dev_routes[r['route_id']]['posts'].append({'cell': r['area'], 'x': r['x'], 'y': r['y']})
    route_ops = []
    for rid, r in route_rows.items():
        live_route = {'id': rid, 'name': r['name'], 'posts': posts.get(rid, [])}
        if live_route['posts'] and all(p['cell'] in areas for p in live_route['posts']) and L.canonical(dev_routes.get(rid)) != L.canonical(live_route):
            route_ops.append({'key': f'route:{rid}', 'before': dev_routes.get(rid), 'after': live_route})
    routes = set(dev_routes) | set(route_rows)
    # Factions go through the live-edit log too (open editors see them); their claims, members and relations follow below.
    dev_factions = {r['id']: r for r in rows(dev, 'SELECT id, name, color FROM live.factions WHERE world_id = %s')}
    faction_ops = [{'key': f'faction:{r["id"]}', 'before': dev_factions.get(r['id']), 'after': r}
                   for r in rows(prod, 'SELECT id, name, color FROM live.factions WHERE world_id = %s ORDER BY position')
                   if dev_factions.get(r['id']) != r]
    professions = {r[0] for r in dev.execute('SELECT id FROM live.professions').fetchall()}
    ops, skipped, copied = route_ops + faction_ops, [], {'NPCs': 0, 'profession slots': 0, 'patrol routes': len(route_ops),
                                                         'factions': len(faction_ops),
                                                'NPC areas': layer_copies['live.npc_areas'], 'spawn rules': layer_copies['live.spawns']}
    for kind, label, table, entity in (('person', 'NPCs', 'live.npcs', S.person_entity),
                                       ('slot', 'profession slots', 'live.profession_slots', S.slot_entity)):
        mine = {r['id']: entity(r) for r in rows(dev, f'SELECT * FROM {table} WHERE world_id = %s')}
        for live in (entity(r) for r in rows(prod, f'SELECT * FROM {table} WHERE world_id = %s')):
            missing = [live[k]['cell'] for k in ('home', 'work', 'evening') if live[k]['cell'] not in areas]
            if live['route'] and live['route'] not in routes:
                missing.append(f'route {live["route"]}')
            if kind == 'slot' and live['profession'] not in professions:
                missing.append(f'profession {live["profession"]}')
            if missing:
                skipped.append(f'{live["name"]} ({live["id"]}): DEV has no {", ".join(sorted(set(missing)))}.')
            elif L.canonical(mine.get(live['id'])) != L.canonical(live):
                ops.append({'key': f'{kind}:{live["id"]}', 'before': mine.get(live['id']), 'after': live})
                copied[label] += 1
    economy_live = rows(prod, 'SELECT * FROM live.economy WHERE world_id = %s')
    economy_dev = rows(dev, 'SELECT * FROM live.economy WHERE world_id = %s')
    if economy_live and (not economy_dev or S.economy_entity(economy_dev[0]) != S.economy_entity(economy_live[0])):
        ops.append({'key': 'economy', 'before': S.economy_entity(economy_dev[0]) if economy_dev else None,
                    'after': S.economy_entity(economy_live[0])})
    if ops:
        for attempt in range(2):                       # An editor may touch one of them at the same moment.
            try:
                L.apply_edit(dev, 'live-copy', editor, 'Copied NPC state from live', ops)
                break
            except L.Conflict as conflict:
                if attempt:
                    raise PublishError('Someone was editing these NPCs in DEV; try again in a moment.') from conflict
                for op in ops:
                    op['before'] = conflict.current.get(op['key'], op['before'])
    copied.update(copy_faction_details(dev, prod, world_id, editor))
    with dev.transaction():
        # Which area each NPC wanders and which rule spawned them: columns the editor's NPC shape does not carry.
        for npc_id, wander, spawn in prod.execute('SELECT id, wander_area, spawn_id FROM live.npcs WHERE world_id = %s', (world_id,)).fetchall():
            dev.execute('UPDATE live.npcs n SET wander_area = (SELECT id FROM live.npc_areas WHERE world_id = n.world_id AND id = %s),'
                        ' spawn_id = (SELECT id FROM live.spawns WHERE world_id = n.world_id AND id = %s) WHERE world_id = %s AND id = %s',
                        (wander, spawn, world_id, npc_id))
        columns = [c for c in shape(dev, 'live.characters')[0] if c != 'updated_at']
        mine = {r[0]: dict(zip(columns, r)) for r in dev.execute(f'SELECT {", ".join(columns)} FROM live.characters').fetchall()}
        characters = []
        for row in (dict(zip(columns, r)) for r in prod.execute(f'SELECT {", ".join(columns)} FROM live.characters').fetchall()):
            if row['profession'] and row['profession'] not in professions:
                skipped.append(f'Roster character {row["name"]} ({row["id"]}): DEV has no profession {row["profession"]}.')
            elif mine.get(row['id']) != row:
                write(dev, 'live.characters', row)
                characters.append(row)
        # Newer than DEV's own save, so a DEV server applies them at its next start.
        states = prod.execute('SELECT npc_id, alive, state FROM live.npc_state WHERE world_id = %s', (world_id,)).fetchall()
        dev.execute('DELETE FROM live.npc_state WHERE world_id = %s', (world_id,))
        for npc_id, alive, state in states:
            dev.execute('INSERT INTO live.npc_state (world_id, npc_id, alive, state, updated_at) VALUES (%s, %s, %s, %s, now())',
                        (world_id, npc_id, alive, S.Jsonb(state)))
    return {'copied': {**copied, 'roster characters': len(characters), 'NPC running states': len(states),
                       'town economy': int(any(op['key'] == 'economy' for op in ops))}, 'skipped': skipped}


def copy_faction_details(dev, prod, world_id, editor):
    """After the factions themselves: their kind and description, territory claims, members and relations, from PROD
    into DEV wherever DEV has the factions, places and NPCs they refer to. DEV's own additions are kept."""
    ids = lambda sql: {r[0] for r in dev.execute(sql, (world_id,)).fetchall()}
    factions, areas = ids('SELECT id FROM live.factions WHERE world_id = %s'), ids('SELECT id FROM world.areas WHERE world_id = %s')
    npcs = ids('SELECT id FROM live.npcs WHERE world_id = %s')
    def changed(table, keep):
        mine, theirs = read(dev, table, world_id, skip=('updated_at',)), read(prod, table, world_id, skip=('updated_at',))
        return [row for key, row in theirs.items() if mine.get(key) != row and keep(row)]
    def copy_claims(conn):
        rows = changed('live.faction_claims', lambda r: r['faction_id'] in factions and r['area'] in areas)
        for row in rows:
            write(conn, 'live.faction_claims', row)
        return len(rows)
    with dev.transaction():
        for fid, kind, description in prod.execute('SELECT id, kind, description FROM live.factions WHERE world_id = %s', (world_id,)).fetchall():
            dev.execute('UPDATE live.factions SET kind = %s, description = %s WHERE world_id = %s AND id = %s', (kind, description, world_id, fid))
    claims = L.change_claims(dev, 'live-copy', editor, 'Copied territory from live', copy_claims)
    with dev.transaction():
        members = changed('live.faction_members', lambda r: r['faction_id'] in factions and r['npc_id'] in npcs)
        relations = changed('live.faction_relations', lambda r: r['faction_id'] in factions and r['other_id'] in factions)
        for table, rows in (('live.faction_members', members), ('live.faction_relations', relations)):
            for row in rows:
                write(dev, table, row)
    return {'territory claims': claims, 'faction members': len(members), 'faction relations': len(relations)}


def releases(prod, limit=50):
    world_id = the_world(prod, 'PROD')
    rows = prod.execute('''SELECT number, pushed_at, pushed_by, note, kind, restored, summary FROM admin.releases
                           WHERE world_id = %s ORDER BY number DESC LIMIT %s''', (world_id, limit)).fetchall()
    return [{'number': r[0], 'at': r[1].isoformat(), 'by': r[2], 'note': r[3], 'kind': r[4], 'restored': r[5], 'summary': r[6]}
            for r in rows]


class Publisher:
    """Push to live for the editor host: DEV and PROD as ratw_publisher."""

    def __init__(self, connect=None):
        self._connect = connect or (lambda database: world_db.connect(database, 'publisher'))

    def connect(self, database):
        try:
            return self._connect(database)
        except world_db.DatabaseError as error:
            raise S.Unavailable(f'Cannot reach the {database.upper()} database: {error}') from error

    def preview(self):
        with self.connect('dev') as dev, self.connect('prod') as prod:
            return {**preview(dev, prod), 'releases': releases(prod)}

    def push(self, data):
        with self.connect('dev') as dev, self.connect('prod') as prod:
            result = push(dev, prod, data.get('password'), str(data.get('editor') or 'Someone'), str(data.get('note') or ''))
            # DEV then mirrors the live NPCs, so the next edits start from what is actually happening in PROD.
            try:
                result['pulled'] = pull_npcs(dev, prod, 'Live copy')
            except (PublishError, S.StoreError) as error:
                result['pulled'] = {'error': str(error)}
            return result

    def pull(self, data):
        with self.connect('dev') as dev, self.connect('prod') as prod:
            return pull_npcs(dev, prod, str(data.get('editor') or 'Live copy'))

    def rollback(self, data):
        number = data.get('release')
        if not isinstance(number, int):
            raise PublishError('Choose a release to roll back to.')
        with self.connect('prod') as prod:
            return rollback(prod, number, data.get('password'), str(data.get('editor') or 'Someone'), str(data.get('note') or ''))


# --------------------------------------------------------------------------- Command line

def describe(summary):
    lines = []
    for part in ('world', 'live'):
        for what, counts in summary.get(part, {}).items():
            done = ', '.join(f'{n} {k}' for k, n in counts.items() if n)
            if done:
                lines.append(f'  {what}: {done}')
    return '\n'.join(lines) or '  nothing'


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('preview')
    p = sub.add_parser('push'); p.add_argument('--note', default='')
    sub.add_parser('releases')
    sub.add_parser('pull', help="copy PROD's NPCs and their running state into DEV")
    r = sub.add_parser('rollback'); r.add_argument('release', type=int); r.add_argument('--note', default='')
    args = parser.parse_args(argv)
    publisher = Publisher()
    try:
        if args.command == 'preview':
            info = publisher.preview()
            print(f'{info["world"]}: DEV revision {info["devRevision"]}, live release {info["release"]}')
            print('\n'.join(f'  problem: {e}' for e in info['errors']) or describe(info['summary']))
        elif args.command == 'pull':
            result = publisher.pull({'editor': getpass.getuser()})
            print('Copied into DEV: ' + ', '.join(f'{n} {k}' for k, n in result['copied'].items()))
            print('\n'.join(f'  skipped: {s}' for s in result['skipped']))
        elif args.command == 'releases':
            for rel in publisher.preview()['releases']:
                print(f'{rel["number"]:>4}  {rel["at"][:19]}  {rel["kind"]:<8} {rel["by"]:<16} {rel["note"]}')
        else:
            password = getpass.getpass('Publish password: ')
            data = {'password': password, 'editor': getpass.getuser(), 'note': args.note, 'release': getattr(args, 'release', None)}
            result = publisher.push(data) if args.command == 'push' else publisher.rollback(data)
            print(f'Release {result["release"]} is live.\n{describe(result["summary"])}')
    except (PublishError, S.StoreError) as error:
        print('\n'.join([str(error)] + [f'  {e}' for e in getattr(error, 'errors', [])]), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
