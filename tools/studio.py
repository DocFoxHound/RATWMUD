#!/usr/bin/env python3
"""RATW Studio: one tool, two suites (Docs/Design/66-worlds-atlas-and-dm.md, section 3).

  bash tools/studio.sh [--port N] [--suite atlas|dm]     build the UI when its sources changed, then serve it
  python3 tools/studio.py serve [--port N] [--suite atlas|dm] [--world KEY]
  bash tools/editor.sh / bash tools/dungeon-master.sh   the same, opening on the Atlas / DM suite

One host on one port (default 8765, local only) serves one app (Editor/index.html) with a switch between:

  Atlas  builds worlds, on dev worlds (its world picker switches Atlas's world, as `world_db.py use KEY` does): the
         map and terrain, cells, interiors, doors, places, the roster and NPCs, their homes and jobs, and (moved from
         the DM) patrol routes, NPC areas, spawn rules, wander areas, factions and talk. Its routes:
         /api/atlas/...          tools/map_editor.py's Handler (its own session token still guards every write);
         /api/atlas/layers/...   the DM's NPC, layer, faction and talk methods (tools/dungeon_master.py), only on
                                 Atlas's world. No storylines, no Push to live (the DM's Import from dev).
         /api/atlas/sync/...     Update from live (tools/world_sync.py; doc 66, section 4): GET diff, POST pull
                                 {choices, expect}, on Atlas's world when it has an upstream.
  DM     runs live worlds: the server, live NPC life (kill, revive, move, visit), players, reports, artwork, the
         calendar, money, health, storytellers, chapters. Its routes: /api/dm/... (tools/dungeon_master.py's Handler)
         without the map and NPC authoring that moved to Atlas, and only on live worlds; and the server and imports
         (tools/world_sync.py; doc 66, section 4):
         GET  /api/dm/server/status?world=KEY     a live world's state (running or maintenance), its game server, release
         POST /api/dm/server/maintenance          {world, on, note}: close it (players get a minute) or turn it back on
         POST /api/dm/server/switch               {world, confirm: the world's key again}: the live world, in maintenance
         GET  /api/dm/import/plan?dev=KEY         the dev worlds copied from the live world, and what importing one does
         POST /api/dm/import                      {dev, choices, password, note, expect}: a release, in maintenance
         GET  /api/dm/releases?world=KEY, POST /api/dm/rollback {number, password, note}

Errors from the engine keep their meaning: refused (422, with `errors`), a wrong publish password (401), locked out
(429), conflicts left to choose (409, with `conflicts`, the plan's items), not in maintenance or the worlds moved since
the plan (409). Every pull, import, maintenance change, switch and rollback is audited in that world's DM audit.

One sign-in for the whole tool: the DM accounts (dm.admins in PROD; tools/dungeon_master.py accounts ...). Atlas is
for dm and admin accounts; a viewer sees the DM suite only. The Storykeeper (tools/dm_service.py) stays its own
service, as before. Every request needs the local origin, as both hosts did.
"""
from __future__ import annotations

import argparse
import errno
import http.client
import json
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

import dungeon_master as D
import map_editor
import world_db
import world_store as S

PORT = 8765
SUITES = ('atlas', 'dm')
# What moved from the DM to Atlas (doc 66, section 3): making and changing NPCs and the layers they live by (patrol
# routes, NPC areas, spawn rules, wander areas), factions and talk. The DM suite doesn't offer them; Atlas's NPC
# pages reach them under /api/atlas/layers/, on Atlas's world only.
MOVED_TO_ATLAS = frozenset({
    '/api/npcs/save', '/api/npcs/delete', '/api/npcs/wander',
    '/api/routes/save', '/api/routes/delete', '/api/areas/save', '/api/areas/delete', '/api/spawns/save', '/api/spawns/delete',
    '/api/factions/save', '/api/factions/delete', '/api/factions/claim', '/api/factions/unclaim', '/api/factions/relation',
    '/api/factions/member',
    '/api/talk/voice', '/api/talk/kin', '/api/talk/dialect', '/api/talk/figure',
})
# What those pages read (and the queued syncs' answers, /api/actions/ID).
LAYER_READS = frozenset({'/api/npcs', '/api/world', '/api/ground', '/api/factions', '/api/factions/history', '/api/talk',
                         '/api/talk/dialects', '/api/talk/figures'})
# Push to live leaves Atlas: it becomes the DM's Import from dev (doc 66, Phase 4).
NOT_IN_ATLAS = frozenset({'/api/publish', '/api/publish/push', '/api/publish/rollback', '/api/publish/pull'})
PUSH_MOVED = ('Push to live has gone: Atlas updates its copy from live (Update from live), and the DM\'s Server & import '
              'tab does the Import from dev (doc 66, section 4).')
LIVE_SWITCH = 'Switching the live server\'s world is done in the DM\'s Server & import tab, during maintenance only.'
# The DM suite's server and import routes (doc 66, section 4), answered by the studio itself over tools/world_sync.py.
SERVER_ROUTES = frozenset({('GET', '/api/server/status'), ('POST', '/api/server/maintenance'), ('POST', '/api/server/switch'),
                           ('GET', '/api/import/plan'), ('POST', '/api/import'), ('GET', '/api/releases'),
                           ('POST', '/api/rollback')})
SYNC_ROUTES = frozenset({('GET', '/api/sync/diff'), ('POST', '/api/sync/pull')})
SWITCH_WARNING = 'Switching the live world is rarely wise: players\' characters and progress belong to each world.'


def dm_offers(method: str, path: str) -> bool:
    """The DM suite: everything the DM host had but the authoring that moved to Atlas."""
    return path not in MOVED_TO_ATLAS


def layer_offers(method: str, path: str) -> bool:
    """Atlas's NPC pages: the moved authoring and what it reads; no sign-in, storyline or live-life routes."""
    if method == 'POST':
        return path in MOVED_TO_ATLAS
    return path in LAYER_READS or (path.startswith('/api/actions/') and path.count('/') == 3)


def atlas_suite(key: str):
    """Atlas's Handler on a dev world's database (Push to live has gone: NOT_IN_ATLAS answers its old paths)."""
    import live_edit
    return map_editor.make_handler(world=live_edit.LiveWorld(key=key))


def shown(key: str) -> dict:
    """A registered world as the app sees it (no database name)."""
    return {k: v for k, v in world_db.world(key).items() if k != 'database'}


class Studio:
    """The tool's state: the DM (accounts, the live worlds), Atlas's world and its Handler, the suite to open on."""

    def __init__(self, dm=None, atlas=atlas_suite, atlas_key=None, suite='atlas', choose=None, sync=None):
        if suite not in SUITES:
            raise ValueError(f'A suite is one of: {", ".join(SUITES)}.')
        self.dm = dm or D.DungeonMaster()
        if sync is None:
            import world_sync
            sync = world_sync.Sync()
        self.sync = sync                       # Pull, import, maintenance, the live switch (tools/world_sync.py).
        self.suite = suite
        self._make_atlas = atlas
        self._choose = choose or (lambda key: world_db.set_local(atlas=key))   # Saved/Config/worlds.local.json
        self._lock = threading.Lock()
        self.dm_suite = D.make_handler(self.dm, offers=dm_offers, kinds=('live',))
        self.layers = D.make_handler(self.dm, offers=layer_offers, world=lambda: self.atlas_key)
        self.atlas_key = atlas_key or world_db.atlas_world()
        self.atlas = atlas(self.atlas_key)

    def use(self, key: str) -> str:
        """Switches Atlas's world (a dev world): this computer's choice, and the Atlas suite's database."""
        if key not in world_db.keys('dev'):
            raise D.DMError(f'Atlas works on dev worlds: {", ".join(world_db.keys("dev")) or "none registered"}.', 400)
        with self._lock:
            handler = self._make_atlas(key)
            self._choose(key)
            self.atlas, self.atlas_key = handler, key
        return key

    def session(self, who: dict) -> dict:
        """Who is signed in, the suite to open on, and each suite's worlds."""
        try:
            live = world_db.live_world()
        except world_db.DatabaseError:
            live = None
        return {'me': who, 'suite': self.suite,
                'atlas': {'available': D.RANK[who['role']] >= D.RANK['dm'], 'world': shown(self.atlas_key),
                          'worlds': [shown(k) for k in world_db.keys('dev')]},
                'dm': {'live': live, 'worlds': [shown(k) for k in world_db.keys('live')], 'switch': LIVE_SWITCH}}

    def copies_of(self, live: str) -> list[str]:
        """The dev worlds whose upstream is this live world: what can be imported into it. Atlas's world first."""
        found = [k for k in world_db.keys('dev') if world_db.upstream(k) == live]
        return sorted(found, key=lambda k: k != self.atlas_key)


def make_handler(studio: Studio):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            pass     # Never log bodies, passwords or tokens.

        def as_(self, cls):
            """This request as one suite's Handler sees it: the same connection, headers and state."""
            view = cls.__new__(cls)
            view.__dict__ = self.__dict__
            return view

        def reply(self, status, data):
            return self.as_(studio.dm_suite).reply(status, data)

        def trusted(self):
            return self.as_(studio.dm_suite).trusted()

        def signed_in(self):
            return studio.dm.session(self.as_(studio.dm_suite).token())

        def builder(self):
            who = self.signed_in()
            if D.RANK[who['role']] < D.RANK['dm']:
                raise D.DMError('Atlas is for dm and admin accounts; a viewer has the DM suite.', 403)
            return who

        def do_GET(self):
            self.route('GET')

        def do_POST(self):
            self.route('POST')

        def route(self, method):
            if not self.trusted():
                return self.reply(403, {'error': 'Local studio origin required.'})
            parts = urlsplit(self.path)
            path = parts.path
            if not path.startswith('/api/'):
                # The built app and Atlas's portraits.
                return self.as_(studio.atlas).static(path) if method == 'GET' else self.reply(405, {'error': 'Method not allowed.'})
            try:
                for prefix, suite in (('/api/atlas/layers/', 'layers'), ('/api/atlas/', 'atlas'), ('/api/dm/', 'dm'),
                                      ('/api/studio/', 'studio')):
                    if path.startswith(prefix):
                        break
                else:
                    return self.reply(404, {'error': 'Not found: the studio\'s routes are /api/atlas/... and /api/dm/....'})
                inner = '/api/' + path[len(prefix):]
                if suite == 'studio':
                    return self.studio(method, inner)
                self.path = inner + ('?' + parts.query if parts.query else '')
                if suite == 'dm':
                    if (method, inner) in SERVER_ROUTES:
                        return self.server_routes(method, inner, parse_qs(parts.query))
                    return self.as_(studio.dm_suite).dispatch(method)
                if suite == 'atlas' and (method, inner) in SYNC_ROUTES:
                    return self.update_from_live(method)
                self.builder()
                if suite == 'layers':
                    return self.as_(studio.layers).dispatch(method)
                if inner in NOT_IN_ATLAS:
                    return self.reply(404, {'error': PUSH_MOVED})
                atlas = self.as_(studio.atlas)
                return atlas.do_GET() if method == 'GET' else atlas.do_POST()
            except D.DMError as error:
                return self.reply(error.status, {'error': str(error)})
            except map_editor.HostError as error:            # tools/world_sync.py's refusals (and world_store's).
                answer = {'error': str(error), 'errors': list(getattr(error, 'errors', []) or [])}
                if getattr(error, 'conflicts', None) is not None:
                    answer['conflicts'] = error.conflicts
                return self.reply(getattr(error, 'status', 422), answer)
            except world_db.DatabaseError as error:          # An unknown world, or the local choice unusable.
                return self.reply(400, {'error': str(error)})
            except (ValueError, TypeError, KeyError):
                return self.reply(400, {'error': 'Malformed request.'})
            except Exception as error:  # noqa: BLE001 - e.g. a world's database behind on its migrations: say so, don't hang up.
                first = (str(error).strip().splitlines() or [''])[0]
                return self.reply(500, {'error': f'The studio could not answer ({type(error).__name__}: {first[:300]}).'})

        def writer(self, who):
            if D.RANK[who['role']] < D.RANK['dm']:
                raise D.DMError(f'Your role ({who["role"]}) cannot do that: it needs a dm or admin account.', 403)
            return who

        def audited(self, who, world, action, target, detail, result):
            """Records it in that world's DM audit; the change is made by then, so a failure here is a warning."""
            try:
                studio.dm.log(who, world, action, target, detail)
            except Exception as error:  # noqa: BLE001
                result = {**result, 'warnings': [*result.get('warnings', []), f'Not audited: {error}']}
            return result

        def live_key(self, asked=''):
            """A live world: the one asked for, else the live server's."""
            key = asked or world_db.live_world()
            if world_db.kind(key) != 'live':
                raise D.DMError(f'{key} is a dev world; the DM works on live worlds ({", ".join(world_db.keys("live"))}).', 400)
            return key

        def copy_of(self, live, dev):
            copies = studio.copies_of(live)
            if dev not in copies:
                raise D.DMError(f'{dev or "(none)"} is not a working copy of {live}: import from '
                                f'{", ".join(copies) or "a dev world whose upstream is " + live}.', 400)
            return dev

        def update_from_live(self, method):
            """Atlas's Update from live: the plan, and the pull, on Atlas's world (it must have an upstream)."""
            who = self.builder()
            key = studio.atlas_key
            live = world_db.upstream(key)
            if not live:
                raise D.DMError(f'{world_db.world(key)["name"]} ({key}) has no upstream live world: it is a side project, with '
                                'nothing to update from. Only a dev world copied from a live one (world_db.py new KEY --upstream '
                                'LIVE --from LIVE) can be updated from live.', 409)
            if method == 'GET':
                plan = studio.sync.diff(key)
                try:
                    status = studio.sync.status(live)
                    server = {'state': status['server'], 'running': status['serverRunning'], 'release': status['release']}
                except S.StoreError:
                    server = None
                return self.reply(200, {**plan, 'devWorld': shown(key), 'liveWorld': shown(live), 'liveServer': server})
            if not self.as_(studio.atlas).editing():
                raise D.DMError('Local editor session required.', 403)
            data = self.as_(studio.dm_suite).body()
            expect = data.get('expect') or None
            result = studio.sync.pull(key, data.get('choices') or {}, who['username'], None if expect is None else str(expect))
            applied = sum(n for part in (result.get('applied') or {}).values() for c in part.values() for n in c.values())
            return self.reply(200, self.audited(who, key, 'world.pull', live,
                                                f'Updated from {live} (live release {result.get("release")}): {applied} rows '
                                                f'changed, {result.get("conflicts", 0)} conflicts chosen.', result))

        def server_routes(self, method, path, query):
            """The DM's server and imports: maintenance, the live switch, Import from dev, releases and rollback."""
            who = self.signed_in()

            def asked(name):
                return str((query.get(name) or [''])[0])
            if method == 'GET':
                if path == '/api/server/status':
                    key = self.live_key(asked('world'))
                    return self.reply(200, {**studio.sync.status(key), 'live': world_db.live_world(), 'warning': SWITCH_WARNING})
                if path == '/api/releases':
                    key = self.live_key(asked('world'))
                    return self.reply(200, {'world': key, 'releases': studio.sync.releases(key)})
                live = self.live_key()                                         # /api/import/plan
                copies = studio.copies_of(live)
                dev = asked('dev') or (copies[0] if copies else '')
                answer = {'live': shown(live), 'candidates': [shown(k) for k in copies], 'dev': dev or None, 'plan': None,
                          'problem': '', 'errors': []}
                if dev:
                    self.copy_of(live, dev)
                    try:
                        answer['plan'] = studio.sync.plan_import(dev)
                    except S.StoreError as error:      # E.g. no base yet: say why, with the worlds still listed.
                        answer['problem'], answer['errors'] = str(error), list(getattr(error, 'errors', []) or [])
                return self.reply(200, answer)
            self.writer(who)
            data = self.as_(studio.dm_suite).body()
            note = str(data.get('note') or '')[:400]
            if path == '/api/server/maintenance':
                key, on = self.live_key(str(data.get('world') or '')), data.get('on')
                if not isinstance(on, bool):
                    raise D.DMError('Say {"on": true} to start maintenance or {"on": false} to end it.', 400)
                result = studio.sync.maintenance(key, on, who['username'], note)
                return self.reply(200, self.audited(who, key, 'server.maintenance' if on else 'server.running', key,
                                                    ('Started maintenance' if on else 'Ended maintenance') + (f': {note}' if note else '.'),
                                                    result))
            if path == '/api/server/switch':
                key = str(data.get('world') or '')
                if world_db.kind(key) != 'live':
                    raise D.DMError(f'{key} is a dev world; the live server runs live worlds.', 400)
                if str(data.get('confirm') or '') != key:
                    raise D.DMError(f'{SWITCH_WARNING} To switch anyway, type the world\'s key ({key}) to confirm.', 400)
                was = world_db.live_world()
                if key == was:
                    raise D.DMError(f'{key} is already the live world.', 400)
                result = studio.sync.switch_live(key, who['username'])
                result = self.audited(who, was, 'server.switch', key, f'Switched the live world from {was} to {key}.', result)
                return self.reply(200, self.audited(who, key, 'server.switch', key, f'Became the live world (was {was}).', result))
            password = data.get('password')
            if not isinstance(password, str) or not password:
                raise D.DMError('Enter the publish password.', 400)
            if path == '/api/import':
                live = self.live_key()
                dev = self.copy_of(live, str(data.get('dev') or ''))
                expect = data.get('expect') or None
                result = studio.sync.import_(dev, data.get('choices') or {}, password, who['username'], note,
                                             None if expect is None else str(expect))
                return self.reply(200, self.audited(who, live, 'world.import', dev,
                                                    f'Release {result.get("release")} imported from {dev}' + (f': {note}' if note else '.'),
                                                    result))
            key = self.live_key(str(data.get('world') or ''))                 # /api/rollback
            number = data.get('number')
            if not isinstance(number, int) or isinstance(number, bool) or number < 1:
                raise D.DMError('Choose a release to roll back to.', 400)
            result = studio.sync.rollback(key, number, password, who['username'], note)
            return self.reply(200, self.audited(who, key, 'world.rollback', str(number),
                                                f'Rolled back to release {number} (now release {result.get("release")})'
                                                + (f': {note}' if note else '.'), result))

        def studio(self, method, path):
            if method == 'GET' and path == '/api/ping':
                return self.reply(200, {'app': 'ratw-studio', 'suite': studio.suite})
            if method == 'GET' and path == '/api/session':
                return self.reply(200, studio.session(self.signed_in()))
            if method == 'POST' and path == '/api/atlas-world':
                self.builder()
                data = self.as_(studio.dm_suite).body()
                return self.reply(200, {'world': shown(studio.use(str(data.get('key', ''))))})
            raise D.DMError('Not found.', 404)

    return Handler


def make_server(port=PORT, studio=None):
    server = ThreadingHTTPServer(('127.0.0.1', port), make_handler(studio or Studio()))
    server.daemon_threads = True
    return server


def running_here(port: int) -> bool:
    """Whether a studio already answers on this port."""
    try:
        conn = http.client.HTTPConnection('127.0.0.1', port, timeout=2)
        conn.request('GET', '/api/studio/ping')
        answer = conn.getresponse()
        return answer.status == 200 and json.loads(answer.read()).get('app') == 'ratw-studio'
    except (OSError, ValueError):
        return False


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('command', nargs='?', default='serve', choices=('serve',))
    parser.add_argument('--port', type=int, default=PORT)
    parser.add_argument('--suite', choices=SUITES, default='atlas', help='the suite the app opens on')
    parser.add_argument('--world', help="Atlas's world for this run (default: Atlas's, python3 tools/world_db.py use KEY)")
    args = parser.parse_args(argv)
    try:
        key = world_db.resolve(args.world or 'atlas', 'dev')
        studio = Studio(atlas_key=key, suite=args.suite)
    except world_db.DatabaseError as error:
        parser.error(str(error))
    url = f'http://127.0.0.1:{args.port}/?suite={args.suite}'
    try:
        server = make_server(args.port, studio)
    except OSError as error:
        if error.errno == errno.EADDRINUSE and running_here(args.port):
            print(f'RATW Studio is already running: {url}', flush=True)
            return 0
        print(f'Port {args.port} is taken ({error.strerror}); choose another with --port N.', file=sys.stderr)
        return 1
    try:
        live = f'the live server runs {world_db.live_world()}'
    except world_db.DatabaseError as error:
        live = str(error)
    print(f'RATW Studio: {url}\n'
          f'Atlas on {key} ("{world_db.world(key)["name"]}", {world_db.world(key)["database"]}); the DM on live worlds '
          f'({live}). Sign in with a DM account. Local-only. Ctrl+C to stop.', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
