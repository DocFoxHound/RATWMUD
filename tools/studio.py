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
                                 Atlas's world. No storylines, no Push to live (the DM's Import from dev, Phase 4).
  DM     runs live worlds: the server, live NPC life (kill, revive, move, visit), players, reports, artwork, the
         calendar, money, health, storytellers, chapters. Its routes: /api/dm/... (tools/dungeon_master.py's Handler)
         without the map and NPC authoring that moved to Atlas, and only on live worlds.

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
from urllib.parse import urlsplit

import dungeon_master as D
import map_editor
import world_db

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
PUSH_MOVED = 'Push to live has left Atlas: it becomes the DM\'s Import from dev (doc 66, Phase 4).'
LIVE_SWITCH = 'Switching the live server\'s world is the DM\'s, during maintenance (doc 66, Phase 4); not yet.'


def dm_offers(method: str, path: str) -> bool:
    """The DM suite: everything the DM host had but the authoring that moved to Atlas."""
    return path not in MOVED_TO_ATLAS


def layer_offers(method: str, path: str) -> bool:
    """Atlas's NPC pages: the moved authoring and what it reads; no sign-in, storyline or live-life routes."""
    if method == 'POST':
        return path in MOVED_TO_ATLAS
    return path in LAYER_READS or (path.startswith('/api/actions/') and path.count('/') == 3)


def atlas_suite(key: str):
    """Atlas's Handler on a dev world's database, without Push to live."""
    import live_edit
    return map_editor.make_handler(world=live_edit.LiveWorld(key=key), push_unavailable=PUSH_MOVED)


def shown(key: str) -> dict:
    """A registered world as the app sees it (no database name)."""
    return {k: v for k, v in world_db.world(key).items() if k != 'database'}


class Studio:
    """The tool's state: the DM (accounts, the live worlds), Atlas's world and its Handler, the suite to open on."""

    def __init__(self, dm=None, atlas=atlas_suite, atlas_key=None, suite='atlas', choose=None):
        if suite not in SUITES:
            raise ValueError(f'A suite is one of: {", ".join(SUITES)}.')
        self.dm = dm or D.DungeonMaster()
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
                    return self.as_(studio.dm_suite).dispatch(method)
                self.builder()
                if suite == 'layers':
                    return self.as_(studio.layers).dispatch(method)
                if inner in NOT_IN_ATLAS:
                    return self.reply(404, {'error': PUSH_MOVED})
                atlas = self.as_(studio.atlas)
                return atlas.do_GET() if method == 'GET' else atlas.do_POST()
            except D.DMError as error:
                return self.reply(error.status, {'error': str(error)})
            except (ValueError, TypeError, KeyError):
                return self.reply(400, {'error': 'Malformed request.'})
            except Exception as error:  # noqa: BLE001 - e.g. a world's database behind on its migrations: say so, don't hang up.
                first = (str(error).strip().splitlines() or [''])[0]
                return self.reply(500, {'error': f'The studio could not answer ({type(error).__name__}: {first[:300]}).'})

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
