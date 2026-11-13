#!/usr/bin/env python3
"""Publish: a hosted world's map edits made into the build its game server loads
(Docs/Design/78-hosts-run-their-own-worlds.md, 10.3, 12.3-12.5; Phase M8).

Atlas's edits change the map's source (world.*); players see them once they are built. This module holds what a host's
Editor needs for that, behind tools/studio.py's hosted routes and Package/entrypoint.py:

  unpublished()   "N map changes not published": the map edits (world.edits, MAP_KINDS) after the newest build's
                  edits_through (migration 0065).
  memory()        whether this computer can Publish: 5 GB free, from the cgroup and MemAvailable (M0b: the build peaks
                  at 3.8 GB beside the running server and its save's fork). Shown before the host starts.
  the child       ONE process that validates and then builds (python3 tools/world_publish.py child): the source's limits,
                  orphans and constraints, then world_build.build on the world it loaded (whose export is the Atlas
                  validator), then the world's save against the new map (leases, stalls, doors made steps; wolves who
                  would wake at the spawn are a warning). RLIMIT_AS 6 GiB, so a build too big dies alone, and one
                  transaction, so a killed build leaves nothing.
  Publisher       the job: 6 an hour an account, the memory check, a backup or a refusal, the child (stopped after 10
                  minutes), then how the world moves onto the new map (open only to the host, now with a minute's
                  warning, or when everyone has left); "Back to the previous map", "Discard unpublished changes", "Open
                  to everyone", and the update rebuild's question.
  fallback, newest  the root entrypoint's: the previous map published again when a new one won't start; the newest
                  build's format against what this package's server loads.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import threading
import time
from pathlib import Path

import world_build
import world_db
import world_store as S

GIB = 1 << 30
NEED = int(os.environ.get('RATW_PUBLISH_NEED_MB', '5120')) * (1 << 20)        # Free memory Publish asks for (M0b: N = 5 GB).
CHILD_LIMIT = int(os.environ.get('RATW_PUBLISH_RLIMIT_MB', '6144')) * (1 << 20)   # The child's RLIMIT_AS (its VmPeak: 4.1 GB).
CHILD_SECONDS = 600                 # The child is stopped after this (M0b: 81 s on a slow core).
AN_HOUR = 6                         # Publishes an hour an account.
BACKUP_WAIT = 300                   # Seconds for the backup service to make the before-publish dump.
CLOSE_WAIT = 240                    # Seconds for the server to close (its minute's warning, the save, the entrypoint).
# What waits for Publish (doc 78, 11): terrain, cells, interiors, doors, places, resources and profession-slot fills.
# The rest of live_edit's keys (person, route, faction, economy) are the live layers', which reach the game without a
# build.
MAP_KINDS = ('tile', 'height', 'rtile', 'rheight', 'room', 'cell', 'link', 'chapter', 'slot')
MAP_KEYS = ('name', 'spawn', 'herb')
RESTARTS = ('host', 'now', 'later')
NO_SOURCE = 'This world\'s map can\'t be published: it has no map source.'
MAP_KEY_SQL = "(split_part(key, ':', 1) = ANY(%s) OR key = ANY(%s))"


class Refused(Exception):
    """Publish refused, with the problems in plain words: [{'text', 'cell'?, 'x'?, 'y'?}] (a "Show me" for each cell)."""

    def __init__(self, errors, status=422):
        self.errors = [e if isinstance(e, dict) else {'text': str(e)} for e in errors]
        self.status = status
        super().__init__('; '.join(e['text'] for e in self.errors))


def map_key(key: str) -> bool:
    return key in MAP_KEYS or key.partition(':')[0] in MAP_KINDS


# ------------------------------------------------------------------------------------------------ what is unpublished

def newest_build(conn):
    """{id, through, format, at, by, copyOf, failed} of the newest build, or None. A world before migration 0065 has
    format 1 and no count."""
    if world_build.publish_columns(conn):
        row = conn.execute('''SELECT id, edits_through, format, created_at, created_by, copy_of, failed FROM world.builds
                              ORDER BY id DESC LIMIT 1''').fetchone()
    else:
        row = conn.execute('''SELECT id, NULL, 1, created_at, created_by, NULL, false FROM world.builds
                              ORDER BY id DESC LIMIT 1''').fetchone()
    if not row:
        return None
    return {'id': row[0], 'through': row[1], 'format': row[2], 'at': row[3].isoformat(), 'by': row[4], 'copyOf': row[5],
            'failed': bool(row[6])}


def unpublished(conn) -> dict:
    """{count, build, through, last: {at, by} | None}: the map edits after the newest build's edits_through. `count` None
    when it can't be known (no build, or one made before the count was kept)."""
    build = newest_build(conn)
    if not build or build['through'] is None:
        return {'count': None, 'build': build['id'] if build else None, 'through': None, 'last': None}
    row = conn.execute(f'SELECT count(*), max(seq) FROM world.edits WHERE seq > %s AND {MAP_KEY_SQL}',
                       (build['through'], list(MAP_KINDS), list(MAP_KEYS))).fetchone()
    last = None
    if row[0]:
        at, by = conn.execute('SELECT at, editor FROM world.edits WHERE seq = %s', (row[1],)).fetchone()
        last = {'at': at.isoformat(), 'by': by}
    return {'count': row[0], 'build': build['id'], 'through': build['through'], 'last': last}


# ------------------------------------------------------------------------------------------------------------ memory

def _number(path: str):
    try:
        text = Path(path).read_text().strip()
    except OSError:
        return None
    return int(text) if text.isdigit() else None


def memory(need: int = None, root: str = '/') -> dict:
    """Whether this computer can Publish now (doc 78, 10.3, step 3): {need, free, limit, ok}. `free` is the smaller of
    the kernel's MemAvailable and what the container's cgroup has left (memory.max less memory.current; "max": no
    limit); None when neither can be read, which doesn't refuse."""
    need = NEED if need is None else need
    available = None
    try:
        for line in Path(root, 'proc/meminfo').read_text().splitlines():
            if line.startswith('MemAvailable:'):
                available = int(line.split()[1]) * 1024
    except (OSError, ValueError, IndexError):
        pass
    limit = _number(str(Path(root, 'sys/fs/cgroup/memory.max')))
    used = _number(str(Path(root, 'sys/fs/cgroup/memory.current')))
    left = limit - used if limit is not None and used is not None else None
    free = min((v for v in (available, left) if v is not None), default=None)
    return {'need': need, 'free': free, 'limit': limit, 'ok': free is None or free >= need}


def gb(size) -> str:
    return '?' if size is None else f'{size / GIB:.1f} GB'


def memory_words(m: dict) -> str:
    """The line the Map tab and My world show before the host starts."""
    if m['ok']:
        return 'This computer can publish map changes.'
    return (f'This computer can\'t publish yet: publishing needs about {gb(m["need"])} of memory free, and this world has '
            f'{gb(m["free"])}. Close other programs, or give Docker more memory (the hosting guide: "Memory to Publish").')


# ------------------------------------------------------------------------------------------- the checks (step 1)

def source_problems(conn, world_id) -> list[dict]:
    """The source's own problems, before the world is loaded: the limits a package's import holds a map to (the
    client's: tools/world_package.py) and live data left in removed places (world_sync.orphans)."""
    import world_package
    import world_sync
    found = []
    for table, most in world_package.ROWS_MOST.items():
        if table.startswith('world.') and conn.execute('SELECT to_regclass(%s) IS NOT NULL', (table,)).fetchone()[0]:
            count = conn.execute(f'SELECT count(*) FROM {table}').fetchone()[0]
            if count > most:
                found.append({'text': f'The map has {count} {table.split(".")[1].replace("_", " ")}; the most a world holds is {most}.'})
    found += [{'text': text} for text in world_sync.orphans(conn, world_id)]
    return found


def _float(text):
    """A saved number, or None when the save holds something else there (a cast in SQL would stop the whole read)."""
    try:
        value = float(text)
    except (TypeError, ValueError):
        return None
    return value if value == value and abs(value) < 1e9 else None


def read_saved(conn, world_id) -> dict:
    """What the world's save says that a new map could strand (as ratw_dm, which reads game.*): every wolf's place
    (online or away: game.characters), the leases (game.lodgings), the stalls and the doors' states (the checkpoint)."""
    def table(name):
        return conn.execute('SELECT to_regclass(%s) IS NOT NULL', (name,)).fetchone()[0]
    out = {'wolves': [], 'leases': [], 'stalls': [], 'doors': {}}
    if table('game.characters'):
        out['wolves'] = [{'id': r[0], 'name': r[1] or r[0], 'cell': r[2], 'x': _float(r[3]), 'y': _float(r[4])} for r in conn.execute(
            """SELECT key, data->>'name', data->>'cell', data->>'x', data->>'y' FROM game.characters
               WHERE world_id = %s AND data ? 'cell'""", (world_id,)).fetchall()]
    if table('game.lodgings'):
        out['leases'] = [{'id': r[0], 'holder': r[1], 'cell': r[2], 'kind': r[3] or 'lodging', 'x': _float(r[4]), 'y': _float(r[5])}
                         for r in conn.execute(
            """SELECT key, data->>'holder', data->>'cell', data->>'kind', data->>'x', data->>'y'
               FROM game.lodgings WHERE world_id = %s AND coalesce(data->>'cell', '') <> ''""", (world_id,)).fetchall()]
    if table('game.checkpoints'):
        row = conn.execute("""SELECT payload::jsonb #> '{people,stalls}', payload::jsonb -> 'doors' FROM game.checkpoints
                              WHERE world_id = %s""", (world_id,)).fetchone()
        if row:
            out['stalls'] = [s for s in row[0] or [] if isinstance(s, dict) and s.get('cell')]
            out['doors'] = {k: v for k, v in (row[1] or {}).items() if isinstance(v, bool)}
    return out


class MapView:
    """A build as the checks read it: which places it has, each one's grid, and its doors. From the files of a build
    being made (`files`), or a stored one (`conn`, `build_id`)."""

    def __init__(self, files=None, conn=None, build_id=None):
        import terrain_catalog
        self.solid_glyphs = terrain_catalog.SOLID
        self._files, self._conn, self._build = files, conn, build_id
        if files is not None:
            self.ids = {name[6:-5] for name in files if name.startswith('cells/')}
            manifest = files['world.ratw']
        else:
            self.ids = {r[0] for r in conn.execute('SELECT cell_id FROM world.build_cells WHERE build_id = %s', (build_id,))}
            manifest = conn.execute("SELECT files->>'world.ratw' FROM world.builds WHERE id = %s", (build_id,)).fetchone()[0]
        self.doors = {}
        for line in manifest.split('\n'):        # (Not splitlines: a name may hold U+2028, which is no line end here.)
            if line.startswith('door '):
                try:
                    f = shlex.split(line)
                    if len(f) >= 15:
                        self.doors[f[1]] = {'name': f[2], 'cell': f[3], 'x': float(f[4]), 'y': float(f[5]),
                                            'open': f[10] == '1', 'passage': f[13] == '1' or f[1].startswith('stairs_')}
                except ValueError:
                    continue
        self._grids = {}

    def grid(self, cell):
        if cell not in self._grids:
            if self._files is not None:
                body = self._files.get(f'cells/{cell}.cell')
            else:
                row = self._conn.execute('SELECT body FROM world.build_cells WHERE build_id = %s AND cell_id = %s',
                                         (self._build, cell)).fetchone()
                body = row[0] if row else None
            self._grids[cell] = body.partition('\ngrid:\n')[2].split('\n') if body else None
        return self._grids[cell]

    def stands(self, cell, x, y, doors=None) -> bool:
        """Whether someone can stand at (x, y) of a place: there is ground there, and (given the save's door states,
        `doors`) no closed door over it."""
        grid = self.grid(cell)
        if grid is None or x is None or y is None or x != x or y != y or abs(x) > 1e9 or abs(y) > 1e9:
            return False
        tx, ty = int(x // 1), int(y // 1)
        if not (0 <= ty < len(grid) and 0 <= tx < len(grid[ty])) or grid[ty][tx] in self.solid_glyphs:
            return False
        for id_, d in self.doors.items() if doors is not None else ():
            if d['cell'] == cell and int(d['x'] // 1) == tx and int(d['y'] // 1) == ty and not d['passage']:
                if not doors.get(id_, d['open']):
                    return False
        return True


def saved_problems(view: MapView, saved: dict) -> tuple[list[dict], list[dict]]:
    """The world's save against a map (errors, warnings). What the server lets go by itself needs no check (M0b): NPCs,
    residents' homes, weather, lighting, memories and doors of removed places, and a wolf in a removed place or (since
    M8's World::restore) on a tile now solid, who wakes at the spawn: those wolves are a warning. Refused: a lease or a
    stall in a place removed or built over (the game checks neither when it loads), and a door the save holds closed that
    the map makes open steps."""
    errors, warnings = [], []
    for lease in saved['leases']:
        if lease['cell'] not in view.ids:
            errors.append({'text': f'{lease["holder"] or "A wolf"} rents a {lease["kind"]} in {lease["cell"]}, which this map '
                                   'removes. End the lease first (the DM tool), or keep the place.'})
        elif (lease['x'] is not None and lease['y'] is not None and lease['x'] >= 0 and lease['y'] >= 0   # (A whole place's
              and not view.stands(lease['cell'], lease['x'], lease['y'])):                                  # has no bed: -1, -1.)
            errors.append({'text': f'{lease["holder"] or "A wolf"} rents a {lease["kind"]} in {lease["cell"]}, and this map '
                                   'builds over its spot. End the lease first, or keep the spot clear.',
                           'cell': lease['cell'], 'x': lease['x'], 'y': lease['y']})
    for stall in saved['stalls']:
        x, y = stall.get('x'), stall.get('y')
        if stall['cell'] not in view.ids:
            errors.append({'text': f'{stall.get("keeper") or "A wolf"} keeps a market stall in {stall["cell"]}, which this '
                                   'map removes. Close the stall first, or keep the place.'})
        elif isinstance(x, (int, float)) and isinstance(y, (int, float)) and not view.stands(stall['cell'], x, y):
            errors.append({'text': f'{stall.get("keeper") or "A wolf"} keeps a market stall in {stall["cell"]}, and this map '
                                   'builds over its spot. Close the stall first, or keep the spot clear.',
                           'cell': stall['cell'], 'x': x, 'y': y})
    for id_, is_open in saved['doors'].items():
        door = view.doors.get(id_)
        if door and door['passage'] and not is_open:
            errors.append({'text': f'The door {door["name"] or id_} in {door["cell"]} is closed in the world as it stands, and '
                                   'this map makes it open steps, which can\'t be closed. Keep it a door, or open it in '
                                   'the game first.', 'cell': door['cell'], 'x': door['x'], 'y': door['y']})
    moved = [w for w in saved['wolves'] if w['cell'] in view.ids and not view.stands(w['cell'], w['x'], w['y'], saved['doors'])]
    if moved:
        names = ', '.join(sorted(w['name'] for w in moved)[:8]) + (' and others' if len(moved) > 8 else '')
        warnings.append({'text': f'{len(moved)} {"wolf" if len(moved) == 1 else "wolves"} last stood where this map has a wall or '
                                 f'a closed door ({names}): {"it wakes" if len(moved) == 1 else "they wake"} at the world\'s '
                                 'starting place.', 'cell': moved[0]['cell'], 'x': moved[0]['x'], 'y': moved[0]['y']})
    return errors, warnings


def located(text: str, project) -> dict:
    """A validator's problem with its place for "Show me" (doc 78, 10.3, step 1), where its words name one: the spawn,
    a cell or interior, a door or stairs (its first side), or a resident (where they work, else live). The validator
    says what is wrong in words alone (tools/map_editor.py: check_project and the export)."""
    out = {'text': str(text)}
    if not isinstance(project, dict):
        return out
    try:
        spot = None
        if re.match(r'(Spawn\b|Choose a player spawn)', out['text']):
            spot = project.get('spawn')
        else:
            words = set(re.findall(r'[A-Za-z0-9_-]{2,80}', out['text']))
            areas = {a['id'] for a in (project.get('cells') or []) + (project.get('rooms') or [])} & words
            if areas:
                spot = {'cell': sorted(areas, key=out['text'].index)[0]}
            else:
                link = next((l for l in project.get('links') or [] if l.get('id') in words), None)
                person = next((n for n in project.get('people') or [] if n.get('id') in words or n.get('name') in out['text']), None)
                field = next((k for k in ('home', 'work', 'evening') if re.search(rf'\b{k}\b', out['text'])), None)
                spot = (link or {}).get('a') or (person or {}).get(field) or (person or {}).get('work') or (person or {}).get('home')
        if isinstance(spot, dict) and isinstance(spot.get('cell'), str) and spot['cell']:
            out['cell'] = spot['cell']
            if all(isinstance(spot.get(k), (int, float)) and not isinstance(spot.get(k), bool) for k in ('x', 'y')):
                out['x'], out['y'] = spot['x'], spot['y']
    except (TypeError, KeyError, ValueError, AttributeError):
        pass
    return out


# ------------------------------------------------------------------------------------------------------ the child

def _say(**line) -> None:
    print(json.dumps(line), flush=True)


def child(key: str, by: str, dbname: str | None = None) -> int:
    """Validates, then builds, in this one process (doc 78, 10.3, steps 1, 4 and 5). Its lines on stdout are JSON: its
    steps as they come ({"step", "done", "of"}), and last {"result": {ok, build, cells, errors, warnings}}. Exit 0 built,
    3 refused (the errors), 4 out of memory, 1 anything else."""
    import resource
    import psycopg
    import map_editor
    try:
        resource.setrlimit(resource.RLIMIT_AS, (CHILD_LIMIT, CHILD_LIMIT))
    except (ValueError, OSError):
        pass
    _dies_with_parent()
    extra = {'dbname': dbname} if dbname else {}
    loaded = {}
    try:
        _say(step='checking')
        with world_db.connect(key, 'dm', **extra) as dm:
            world = dm.execute('SELECT id FROM world.worlds').fetchone()
            if not world:
                raise Refused([NO_SOURCE])
            saved = read_saved(dm, world[0])
        with world_db.connect(key, 'editor', **extra) as conn:
            problems = source_problems(conn, world[0])
            if problems:
                raise Refused(problems)
            warnings = []

            def check(files):
                errors, warned = saved_problems(MapView(files=files), saved)
                if errors:
                    raise Refused(errors)
                warnings.extend(warned)
                _say(step='storing')

            def tick(step, done, of):
                _say(step='building', done=done, of=of)

            with conn.transaction():
                # One snapshot for the whole load (about 10 s of reads): an edit saved meanwhile is in the build whole or
                # not at all, and counted to match (R2: read committed could build half of one).
                conn.execute('SET TRANSACTION ISOLATION LEVEL REPEATABLE READ')
                through = world_build.newest_edit(conn, world[0]) if world_build.publish_columns(conn) else None
                _say(step='loading')
                project, _ = S.load_world(conn, world[0])
                loaded['project'] = project
                _say(step='validating')
                build_id, cells = world_build.build(conn, by, project=project, edits_through=through, progress=tick, check=check)
                try:
                    # Every deferred constraint now, in words (the laptop's import path does the same: world_sync.finish).
                    conn.execute('SET CONSTRAINTS ALL IMMEDIATE')
                except psycopg.errors.IntegrityError as error:
                    raise Refused([f'The map\'s tables disagree: {error.diag.message_primary}']) from error
        _say(result={'ok': True, 'build': build_id, 'cells': cells, 'errors': [], 'warnings': warnings})
        return 0
    except map_editor.ValidationError as error:
        _say(result={'ok': False, 'errors': [located(e, loaded.get('project')) for e in error.errors[:200]], 'warnings': []})
        return 3
    except Refused as error:
        _say(result={'ok': False, 'errors': error.errors[:200], 'warnings': []})
        return 3
    except MemoryError:
        _say(result={'ok': False, 'memory': True, 'errors': [{'text': 'The build ran out of memory: nothing was changed.'}],
                     'warnings': []})
        return 4
    except (world_db.DatabaseError, S.StoreError) as error:
        _say(result={'ok': False, 'errors': [{'text': f'The map could not be built: {error}'}], 'warnings': []})
        return 1
    except psycopg.errors.SerializationFailure:
        _say(result={'ok': False, 'errors': [{'text': 'The map was changed while it was being built: nothing was published. '
                                                      'Publish again.'}], 'warnings': []})
        return 3
    except Exception as error:  # noqa: BLE001 - said, never a bare "exit 1" (its stderr goes nowhere); nothing was stored.
        _say(result={'ok': False, 'errors': [{'text': f'The map could not be built ({type(error).__name__}): nothing was '
                                                      'changed.'}], 'warnings': []})
        return 1


def _dies_with_parent() -> None:
    """The child never outlives the helper (or the entrypoint) that started it (R2): a build finishing with nobody to
    move the world onto it would lose the host's choice. Linux's PR_SET_PDEATHSIG; and its own alarm past the limit."""
    import signal
    try:
        import ctypes
        ctypes.CDLL(None, use_errno=True).prctl(1, int(signal.SIGKILL), 0, 0, 0)        # (PR_SET_PDEATHSIG.)
        if os.getppid() == 1:
            os._exit(1)
    except (OSError, AttributeError, ValueError):
        pass
    try:
        signal.alarm(CHILD_SECONDS + 60)
    except (OSError, ValueError, AttributeError):
        pass


def run_child(key: str, by: str, on_step=None, dbname: str | None = None, seconds: float = CHILD_SECONDS, env=None) -> dict:
    """The child run and read: its result ({ok, build, cells, errors, warnings}), with `code` its exit status. Killed
    after `seconds`; killed by the kernel or its limit, it changed nothing (one transaction)."""
    command = [sys.executable, str(Path(__file__).resolve()), 'child', '--world', key, '--by', by]
    if dbname:
        command += ['--dbname', dbname]
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, env=env)
    timed_out = []

    def stop():
        timed_out.append(True)
        process.kill()
    timer = threading.Timer(seconds, stop)
    timer.start()
    result = None
    try:
        for line in process.stdout:
            try:
                value = json.loads(line)
            except ValueError:
                continue
            if isinstance(value, dict) and isinstance(value.get('result'), dict):
                result = value['result']
            elif isinstance(value, dict) and on_step:
                on_step(value)
        code = process.wait()
    finally:
        timer.cancel()
        process.stdout.close()
    if timed_out:
        return {'ok': False, 'code': code, 'errors': [{'text': f'The build took more than {int(seconds // 60)} minutes and was '
                                                               'stopped: nothing was changed.'}], 'warnings': []}
    if result is None or (code != 0 and result.get('ok')):
        why = 'it ran out of memory' if code in (-9, 137, 4) else f'it stopped (exit {code})'
        return {'ok': False, 'code': code, 'memory': code in (-9, 137, 4),
                'errors': [{'text': f'The build did not finish: {why}. Nothing was changed.'}], 'warnings': []}
    return {**result, 'code': code}


# -------------------------------------------------------------------------------------------------- discard, go back

def discard(conn, editor: str) -> dict:
    """Discard unpublished map changes (doc 78, 12.3): the map source put back to the newest build's edits_through, from
    the before-values in world.edits, as one more edit (so open editors follow it). A key whose value isn't what its last
    edit left (changed some other way since) refuses the whole discard. Returns {discarded, keys}."""
    import live_edit
    with conn.transaction():
        world = live_edit.world_row(conn, lock=True)
        build = newest_build(conn)
        if not build or build['through'] is None:
            raise Refused(['This world\'s map was built before changes were counted: publish once, and Discard works '
                           'from then on.'], 409)
        rows = conn.execute(f'''SELECT seq, key, before, after FROM world.edits WHERE world_id = %s AND seq > %s
                                AND {MAP_KEY_SQL} ORDER BY seq''',
                            (world['id'], build['through'], list(MAP_KINDS), list(MAP_KEYS))).fetchall()
        first, last = {}, {}
        for seq, key, before, after in rows:
            first.setdefault(key, before)
            last[key] = after
        batch = live_edit.Batch(conn, world)
        ops, changed = [], []
        for key in reversed(list(first)):                       # (Undone in the order made, backwards.)
            now = batch.current(key)
            if live_edit.canonical(now) != live_edit.canonical(last[key]):
                changed.append(key)
            elif live_edit.canonical(now) != live_edit.canonical(first[key]):
                ops.append({'key': key, 'before': now, 'after': first[key]})
        if changed:
            raise Refused([f'{len(changed)} of these changes were changed again some other way since ({", ".join(changed[:5])}'
                           f'{"…" if len(changed) > 5 else ""}): nothing was discarded. Publish, or put them right in the '
                           'map editor.'], 409)
        if ops:
            live_edit.apply_edit(conn, 'discard', editor[:60] or 'Someone', 'Discarded unpublished map changes', ops)
        # The source is the newest build's again: the count starts after this edit.
        conn.execute('UPDATE world.builds SET edits_through = %s WHERE id = %s',
                     (world_build.newest_edit(conn, world['id']), build['id']))
    return {'discarded': len(rows), 'keys': len(first)}


def residents_problems(conn, view: MapView, world_id) -> list[dict]:
    """The residents' and the patrols' places against a map the world would go back to: one who has since been given a
    home, a work place or a post where that map has no ground (a room made after it, a wall it still has) would stop
    the game server loading the world at all ("Place for ... is missing, off-center or on solid terrain"). The first
    twenty, each with its place."""
    found = []

    def check(who, what, area, x, y):
        if area is None or x is None or y is None or len(found) >= 20:
            return
        if area not in view.ids:
            found.append({'text': f'{who} {what} in {area}, which the previous map doesn\'t have. Move them first (the map '
                                  'editor\'s People), or keep this map.'})
        elif not view.stands(area, x + 0.5, y + 0.5):
            found.append({'text': f'{who} {what} in {area} where the previous map has a wall. Move them first (the map '
                                  'editor\'s People), or keep this map.', 'cell': area, 'x': x, 'y': y})
    places = 'home_area, home_x, home_y, work_area, work_x, work_y, evening_area, evening_x, evening_y'
    rows = conn.execute(f'SELECT name, {places} FROM live.npcs WHERE world_id = %s ORDER BY id', (world_id,)).fetchall()
    rows += conn.execute(f'''SELECT c.name, {", ".join("s." + p.strip() for p in places.split(","))}
                             FROM live.profession_slots s JOIN live.characters c ON c.status = 'active'
                              AND c.assignment = jsonb_build_object('world', s.world_id, 'slot', s.id)
                             WHERE s.world_id = %s ORDER BY s.id''', (world_id,)).fetchall()
    for name, *spots in rows:
        for what, at in (('lives', 0), ('works', 3), ('spends the evening', 6)):
            check(name, what, *spots[at:at + 3])
    for name, area, x, y in conn.execute('''SELECT r.name, p.area, p.x, p.y FROM live.patrol_posts p JOIN live.patrol_routes r
                                            ON r.world_id = p.world_id AND r.id = p.route_id WHERE p.world_id = %s
                                            ORDER BY r.id, p.seq''', (world_id,)).fetchall():
        check(f'The patrol {name}', 'has a post', area, x, y)
    return found


def back_problems(conn, dm_conn, build_id) -> tuple[list[dict], list[dict]]:
    """Publish's checks the other way (doc 78, 12.3): the world's save, and its residents' places, against the previous
    map."""
    world = conn.execute('SELECT id FROM world.worlds').fetchone()
    view = MapView(conn=conn, build_id=build_id)
    errors, warnings = saved_problems(view, read_saved(dm_conn, world[0]))
    return errors + residents_problems(conn, view, world[0]), warnings


def unpublished_places(conn) -> set:
    """The places the map's source has that the newest build lacks: a live edit naming one is ahead of the map."""
    build = newest_build(conn)
    if not build:
        return set()
    return {r[0] for r in conn.execute('''SELECT id FROM world.areas a WHERE NOT EXISTS
        (SELECT 1 FROM world.build_cells c WHERE c.build_id = %s AND c.cell_id = a.id)''', (build['id'],))}


AHEAD_PLACE = ('Publish the map first: {who} would be in {place}, which is in the map editor and not yet in the world '
               'players are in (the Publish page).')
AHEAD_TILE = ('Publish the map first: {who} would stand in {place} where the world players are in has a wall or no '
              'ground yet (the Publish page).')


def _spots(value, depth=0):
    """Every {cell, x, y} in a resident's or a route's value (home, work, evening, a route's posts)."""
    if depth > 6:
        return
    if isinstance(value, dict):
        cell, x, y = value.get('cell'), value.get('x'), value.get('y')
        if isinstance(cell, str) and cell and all(isinstance(v, (int, float)) and not isinstance(v, bool) and abs(v) < 1e9 for v in (x, y)):
            yield {'cell': cell, 'x': x, 'y': y}
        for v in value.values():
            yield from _spots(v, depth + 1)
    elif isinstance(value, list):
        for v in value[:2000]:
            yield from _spots(v, depth + 1)


def strings(value):
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        for k, v in value.items():
            yield k
            yield from strings(v)
    elif isinstance(value, list):
        for v in value:
            yield from strings(v)


# ---------------------------------------------------------------------------------------------- the entrypoint's

def newest(conn) -> dict:
    """For the root entrypoint: the newest build, whether this package's server loads its format, and the count."""
    build = newest_build(conn)
    count = unpublished(conn)['count'] if build else None
    return {'build': build['id'] if build else None, 'format': build['format'] if build else None,
            'readable': bool(build) and build['format'] in world_build.READS, 'unpublished': count,
            'reads': list(world_build.READS)}


def fallback(conn, good=None) -> dict:
    """The newest build didn't start (doc 78, 12.4): it is marked failed and the previous map published again under a new
    id. {failed, now} or {failed, now: None, why} when there is no previous map to go back to. `good`: the build the
    server last ran well on (the entrypoint's note), gone back to when it is older, unfailed, readable and still has its
    cells (R2: after two bad maps in a row the one before the newest is bad too)."""
    build = newest_build(conn)
    if not build:
        return {'failed': None, 'now': None, 'why': 'no build'}
    before = None
    if isinstance(good, int) and world_build.publish_columns(conn):
        row = conn.execute('''SELECT b.id FROM world.builds b WHERE b.id = %s AND b.id < %s AND NOT b.failed
                              AND b.format = ANY(%s) AND coalesce(b.copy_of, b.id) <> %s
                              AND EXISTS (SELECT 1 FROM world.build_cells c WHERE c.build_id = b.id)''',
                           (good, build['id'], list(world_build.READS), build['copyOf'] or build['id'])).fetchone()
        before = row[0] if row else None
    if before is None:
        before = world_build.previous(conn)
    if before is None:
        return {'failed': build['id'], 'now': None, 'why': 'no previous map'}
    made, _ = world_build.republish(conn, before, 'fallback', failed=build['id'])
    return {'failed': build['id'], 'now': made}


# ------------------------------------------------------------------------------------------------------ the job

class Publisher:
    """Publish for a host's Editor (tools/studio.py's HostedStudio): one job at a time, in a thread of the helper."""

    def __init__(self, world: str, connect_editor, connect_dm, hosted=None, clock=time.time, run=run_child,
                 request_backup=None, sleep=time.sleep, log=None, read_memory=memory):
        self.world, self.connect_editor, self.connect_dm, self.hosted = world, connect_editor, connect_dm, hosted
        self.clock, self.run, self.sleep, self.log, self.read_memory = clock, run, sleep, log or (lambda *a: None), read_memory
        self.request_backup = request_backup
        self.lock = threading.Lock()
        self.job: dict | None = None
        self.thread: threading.Thread | None = None
        self._ahead = (None, set())
        self._view, self._view_lock = (None, None), threading.Lock()

    # -- what the page shows ----------------------------------------------------------------------------------------
    def host_only(self) -> tuple[bool, str]:
        """Whether "Open only to me" can be chosen, and why not (doc 78, 10.3, step 6): it needs a host the world knows
        by its tickets, so a valid host token."""
        if self.hosted is None or not self.hosted.host_account():
            return False, ('This needs your host token to be valid: the world lets in only its host, and without a valid '
                           'token it knows none. (Check the token in the world folder, then Restart.)')
        return True, ''

    def state(self) -> dict:
        with self.connect_editor() as conn:
            count = unpublished(conn)
            before = world_build.previous(conn)
            build = newest_build(conn)
        with self.connect_dm() as conn:
            import world_sync
            server = world_sync.server_state(conn)['state']
        m = self.read_memory()
        can, why = self.host_only()
        with self.lock:
            job = dict(self.job) if self.job else None
        folder = getattr(self.hosted, 'folder', None)
        fell = _read(folder, 'fallback.json')
        rebuild = _read(folder, 'rebuild.json')
        return {'unpublished': count, 'memory': {**m, 'words': memory_words(m)}, 'job': job, 'serverState': server,
                'hostOnly': {'available': can, 'reason': why}, 'previous': before, 'build': build,
                'fallback': fell if fell and build and fell.get('now') == build['id'] else None,
                'rebuild': rebuild if rebuild.get('needed') and build and build['format'] not in world_build.READS else None,
                'perHour': AN_HOUR}

    def summary(self) -> dict:
        """My world's lines: the count and whether this computer can publish."""
        with self.connect_editor() as conn:
            count = unpublished(conn)
        m = self.read_memory()
        return {'unpublished': count['count'], 'memory': {**m, 'words': memory_words(m)}}

    def ahead(self, body) -> str | None:
        """A live edit that names a place the running map lacks (doc 78, 10.3): the place, or None."""
        with self.connect_editor() as conn:
            mark = conn.execute('''SELECT (SELECT coalesce(max(seq), 0) FROM world.edits),
                                          (SELECT coalesce(max(id), 0) FROM world.builds)''').fetchone()
            if self._ahead[0] != mark:
                self._ahead = (mark, unpublished_places(conn))
        places = self._ahead[1]
        if not places:
            return None
        return next((s for s in strings(body) if s in places), None)

    def ahead_ops(self, ops) -> str | None:
        """Atlas's own edit of a resident or a patrol route (the live layers: they reach the game without a build) that
        puts someone where the map players are in has no ground: in a place not published yet, or on a tile that map
        walls (the closing run: the game server then refuses the whole world at its next start, "Place for ... is
        missing, off-center or on solid terrain"). Why, in words, or None. Map edits themselves wait for Publish."""
        if not isinstance(ops, list):
            return None
        spots = []
        for op in ops:
            key = op.get('key') if isinstance(op, dict) else None
            if isinstance(key, str) and key.partition(':')[0] in ('person', 'route') and op.get('after') is not None:
                name = op['after'].get('name') if isinstance(op['after'], dict) else None
                spots += [(name if isinstance(name, str) and name else key.partition(':')[2], spot) for spot in _spots(op['after'])]
        if not spots:
            return None
        with self._view_lock, self.connect_editor() as conn:
            build = newest_build(conn)
            if not build:
                return None
            if self._view[0] != build['id']:
                self._view = (build['id'], MapView(conn=conn, build_id=build['id']))
            view = self._view[1]
            view._conn = conn
            try:
                for who, spot in spots[:400]:
                    if spot['cell'] not in view.ids:
                        return AHEAD_PLACE.format(who=who[:60], place=spot['cell'][:80])
                    if not view.stands(spot['cell'], spot['x'] + 0.5, spot['y'] + 0.5):
                        return AHEAD_TILE.format(who=who[:60], place=spot['cell'][:80])
            finally:
                view._conn = None
        return None

    # -- the acts -----------------------------------------------------------------------------------------------------
    def _busy(self):
        if self.job and self.job['step'] not in ('done', 'refused', 'failed'):
            raise Refused(['The map is being published now: one at a time.'], 409)

    def _this_hour(self, account: str, action: str = 'map.publish') -> int:
        with self.connect_dm() as conn:
            return conn.execute("""SELECT count(*) FROM dm.audit WHERE action = %s AND at > %s
                                   AND split_part(who, '#', 1) = %s""", (action, self.clock() - 3600, account.split('#')[0])).fetchone()[0]

    def start(self, who: dict, restart: str, rebuild: bool = False) -> dict:
        """Publish: refused at once when another runs, the account has published 6 times this hour, memory is short, the
        choice can't be had, or nothing changed; else the job, which a thread carries on."""
        if restart not in RESTARTS:
            raise Refused(['Choose how the world moves onto the new map: open only to you, now, or when everyone has left.'], 400)
        with self.lock:
            self._busy()
            if restart == 'host':
                can, why = self.host_only()
                if not can:
                    raise Refused([why], 409)
            if not rebuild and self._this_hour(who['username']) >= AN_HOUR:
                raise Refused([f'You have published {AN_HOUR} times this hour, the most an hour: try again later.'], 429)
            m = self.read_memory()
            if not m['ok']:
                raise Refused([f'Publishing needs about {gb(m["need"])} of memory free; this world has {gb(m["free"])}.'], 409)
            self.log(who, 'map.publish', self.world, f'Published the map ({ {"host": "open only to the host", "now": "now", "later": "when everyone has left"}[restart]}).')
            self.job = {'id': int(self.clock() * 1000), 'by': who['username'].split('#')[0], 'at': self.clock(), 'step': 'backup',
                        'done': 0, 'of': 0, 'errors': [], 'warnings': [], 'build': None, 'restart': restart, 'rebuild': rebuild}
            self.thread = threading.Thread(target=self._work, args=(who, restart), daemon=True)
            self.thread.start()
            return dict(self.job)

    def rebuild(self, who: dict, discard_first: bool = False) -> dict:
        """The update rebuild's answer (doc 78, 12.5): the map built again for a server that can't load it as built, with
        the unpublished changes or discarding them first. Only when the newest build is in fact unreadable. Outside the
        hourly count (the closing run: the world is down until it is rebuilt, and an owner who had published six times
        that hour couldn't start it), and nothing is discarded before the rebuild is known to be able to start."""
        with self.connect_editor() as conn:
            build = newest_build(conn)
        if not build or build['format'] in world_build.READS:
            raise Refused(['The map doesn\'t need rebuilding: this world\'s server loads it as it is.'], 409)
        with self.lock:
            self._busy()
        m = self.read_memory()
        if not m['ok']:
            raise Refused([f'Rebuilding needs about {gb(m["need"])} of memory free; this world has {gb(m["free"])}.'], 409)
        if discard_first:
            self.discard(who)
        return self.start(who, 'later', rebuild=True)

    def _set(self, **fields):
        with self.lock:
            self.job.update(fields)

    def _work(self, who, restart):
        try:
            if self.request_backup is not None:
                problem = self.request_backup()                 # (Step 2: a backup, or no Publish.)
                if problem:
                    return self._set(step='refused', errors=[{'text': problem}])
            self._set(step='checking')
            result = self.run(self.world, f'Editor: {self.job["by"]}'[:60],
                              lambda s: self._set(step=s.get('step', 'building'), done=s.get('done', 0), of=s.get('of', 0)))
            if not result.get('ok'):
                return self._set(step='refused' if result.get('code') == 3 else 'failed', errors=result.get('errors') or [],
                                 warnings=result.get('warnings') or [])
            self._set(build=result.get('build'), cells=result.get('cells'), warnings=result.get('warnings') or [])
            if restart != 'later':
                self._set(step='restarting')
                kept = self.move(who, 'host_only' if restart == 'host' else 'running',
                                 'A new map is being published: back in a minute.')
                if kept:
                    self._set(warnings=(self.job.get('warnings') or []) + [{'text': kept}])
            self._set(step='done')
        except Exception as error:  # noqa: BLE001 - the job says it failed, never why in the request's words.
            self._set(step='failed', errors=[{'text': f'Publishing stopped ({type(error).__name__}). Nothing else was changed.'}])

    def move(self, who: dict, then: str, note: str) -> str | None:
        """The world onto its newest map now: closed with its minute's warning (maintenance, exit 76), and once the
        entrypoint says its server has stopped, set to `then` (running, or host_only), which starts it again. Closing and
        opening the world are its owner's (R2): a world the owner closed stays closed for anyone else, and one open only
        to its host stays so; then what was kept is returned in words, for the job."""
        import world_sync
        by, owner = who['username'], who.get('role') == 'owner'
        server = self.hosted.server() if self.hosted is not None else None
        with self.connect_dm() as conn:
            was = world_sync.server_state(conn)['state']
            if was == 'maintenance' and not owner:
                return 'The world is closed, and it stays closed: it starts on this map when its owner opens it (My world).'
            kept = None
            if was == 'host_only' and then == 'running' and not owner:
                then, kept = 'host_only', 'The world stays open only to its host, on this map: its owner opens it to everyone.'
            world_sync.set_state(conn, 'maintenance', by, note)
        try:
            if server is not None and server.get('up'):
                deadline = self.clock() + CLOSE_WAIT
                while self.clock() < deadline:
                    now = self.hosted.server()
                    if now is None or (not now.get('up') and now.get('reason') in ('closed', 'exit', 'world.toml')):
                        break
                    self.sleep(1)
        finally:
            # (Whatever stopped the wait, the world isn't left closed by a Publish.)
            with self.connect_dm() as conn:
                world_sync.set_state(conn, then, by, '' if then == 'running' else 'Open only to the host: a new map is being looked at.')
        return kept

    def open_all(self, who: dict) -> dict:
        """"Open to everyone", after a look at the new map alone."""
        import world_sync
        with self.connect_dm() as conn:
            if world_sync.server_state(conn)['state'] != 'host_only':
                raise Refused(['The world isn\'t open only to you just now.'], 409)
            world_sync.set_state(conn, 'running', who['username'], '')
        self.log(who, 'map.open', self.world, 'Opened the world to everyone on its new map.')
        return {'ok': True}

    def discard(self, who: dict) -> dict:
        with self.lock:
            self._busy()
            with self.connect_editor() as conn:
                out = discard(conn, who['username'].split('#')[0])
        self.log(who, 'map.discard', self.world, f'Discarded {out["discarded"]} unpublished map changes.')
        return {'ok': True, **out}

    def back(self, who: dict, discard_too: bool = False) -> dict:
        """"Go back to the previous map": Publish's checks the other way, the previous build published again under a new
        id, the world moved onto it now (staying open only to the host if it was), and the unpublished changes discarded
        with it when asked."""
        import world_sync
        with self.lock:
            self._busy()
            if self._this_hour(who['username'], 'map.back') >= AN_HOUR:
                raise Refused([f'The world has gone back {AN_HOUR} times this hour, the most an hour: try again later.'], 429)
            with self.connect_editor() as conn, self.connect_dm() as dm:
                before = world_build.previous(conn)
                if before is None:
                    raise Refused(['There is no previous map to go back to.'], 409)
                errors, warnings = back_problems(conn, dm, before)
                if errors:
                    raise Refused(errors)
                was = world_sync.server_state(dm)['state']
                # (One change: a discard that is refused leaves the map where it was, R2.)
                with conn.transaction():
                    made, _ = world_build.republish(conn, before, f'Editor: {who["username"].split("#")[0]}'[:60])
                    out = {'ok': True, 'build': made, 'warnings': warnings}
                    if discard_too:
                        out.update(discard(conn, who['username'].split('#')[0]))
        self.log(who, 'map.back', self.world, 'Went back to the previous map' + (', discarding the unpublished changes.' if discard_too else '.'))
        threading.Thread(target=self._moved, args=(who, 'host_only' if was == 'host_only' else 'running'), daemon=True).start()
        return out

    def _moved(self, who, then):
        try:
            self.move(who, then, 'The world is going back to its previous map: back in a minute.')
        except Exception:  # noqa: BLE001 - the world stays closed; My world's Open opens it.
            pass


def _read(folder, name) -> dict:
    try:
        value = json.loads((Path(folder) / name).read_text(encoding='utf-8')) if folder else {}
    except (OSError, ValueError):
        return {}
    return value if isinstance(value, dict) else {}


# ------------------------------------------------------------------------------------------------------------- main

def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('command', choices=('child', 'newest', 'fallback', 'memory'))
    parser.add_argument('--world', default='world')
    parser.add_argument('--by', default='Editor')
    parser.add_argument('--dbname', help='tests: a database by name, in place of the world\'s own')
    parser.add_argument('--good', type=int, help='fallback: the build the server last ran well on')
    args = parser.parse_args(argv)
    if args.command == 'child':
        return child(args.world, args.by, args.dbname)
    if args.command == 'memory':
        print(json.dumps(memory()))
        return 0
    extra = {'dbname': args.dbname} if args.dbname else {}
    try:
        with world_db.connect(args.world, 'editor', **extra) as conn:
            print(json.dumps(newest(conn) if args.command == 'newest' else fallback(conn, args.good)))
    except (world_db.DatabaseError, S.StoreError) as error:
        print(json.dumps({'error': str(error)[:300]}))
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
