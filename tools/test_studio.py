#!/usr/bin/env python3
"""The studio (tools/studio.py, doc 66 sections 3 and 4): one host, one sign-in, two suites kept apart; Atlas's Update
from live and the DM's server, maintenance, live switch, Import from dev and releases.

Offline with a stand-in DM (accounts by token, every other method recorded) and a stand-in sync engine; two classes
work for real on scratch databases (dropped afterwards; skipped without the local PostgreSQL): a sign-in, and pull,
import, rollback and the live switch on throwaway worlds (test_world_sync's). DEV and PROD are never touched."""
import copy
import http.client
import json
import os
import re
from pathlib import Path
import tempfile
import threading
import unittest

import dungeon_master as D
import map_editor
import studio as ST
import world_db as W
import world_sync as WS
from test_dungeon_master import Fixture
from test_world_db import database_available, superuser
from test_world_store import greyfen
from test_world_sync import ScratchWorlds

TOKENS = {'admin-token': 'admin', 'dm-token': 'dm', 'viewer-token': 'viewer'}


class StandInDM:
    """The DM's accounts by token; any other method answers and is recorded, so a test sees what reached the DM."""

    def __init__(self):
        self.calls = []

    def session(self, token):
        if token not in TOKENS:
            raise D.DMError('Sign in first.', 401)
        return {'username': f'{TOKENS[token]}-user', 'role': TOKENS[token]}

    def __getattr__(self, name):
        def method(*args, **options):
            self.calls.append((name, args))
            return {'called': name, 'target': next((a for a in args if isinstance(a, str)), None)}
        return method


class StandInSync:
    """tools/world_sync.py's Sync: every call recorded and answered, or refused with `fail`."""

    def __init__(self):
        self.calls, self.fail, self.holder = [], None, None

    def __getattr__(self, name):
        def method(*args):
            self.calls.append((name, args))
            if self.fail is not None:
                raise self.fail
            if name == 'status':
                return {'key': args[0], 'server': {'state': 'running', 'by': '', 'at': None, 'note': ''},
                        'serverRunning': bool(self.holder), 'serverHolder': self.holder, 'release': 4}
            return {'called': name, 'release': 5, 'applied': {'world': {}, 'live': {'NPCs': {'added': 1, 'changed': 2, 'removed': 0}}},
                    'conflicts': 0}
        return method


class StandInBuilds:
    """A dev world's builds (studio.Builds): build 7, current until a rebuild makes 8."""

    def __init__(self):
        self.calls, self.current = [], True

    def status(self, key):
        return {'build': 7 if self.current else 8, 'at': None, 'by': '', 'release': None, 'current': self.current}

    def rebuild(self, key, editor):
        self.calls.append(('rebuild', key, editor))
        return {'build': 7, 'at': None, 'by': '', 'release': None, 'current': True, 'made': not self.current, 'cells': 12}


class Running:
    """A studio on a free port; Atlas without a database (a roster file of its own)."""

    def __init__(self, dm, atlas_key, sync=None, economy=None, builds=None):
        self.temp = tempfile.TemporaryDirectory()
        roster = Path(self.temp.name) / 'roster.json'
        map_editor.roster_lib.save({**map_editor.roster_lib.load(), 'characters': []}, roster)
        self.made, self.chosen, self.dm_chosen, self.dm = [], [], [], dm
        self.spawned = []

        def atlas(key):
            self.made.append(key)
            return map_editor.make_handler(roster_path=roster, ai_config_path=Path(self.temp.name) / 'missing.json')

        def spawn(key, port):
            self.spawned.append((key, port))
            return type('Launcher', (), {'poll': lambda _: None})()
        self.studio = ST.Studio(dm=dm, atlas=atlas, atlas_key=atlas_key, choose=self.chosen.append, sync=sync, economy=economy,
                                builds=builds, servers=ST.Servers(spawn), choose_dm=self.dm_chosen.append)
        self.server = ST.make_server(0, self.studio)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def stop(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.temp.cleanup()

    def call(self, method, path, body=None, token=None, headers=None):
        conn = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=10)
        sent = {**({'Authorization': f'Bearer {token}'} if token else {}), **(headers or {})}
        if body is not None:
            sent['Content-Type'] = 'application/json'
        conn.request(method, path, body=None if body is None else json.dumps(body), headers=sent)
        answer = conn.getresponse()
        status, kind, raw = answer.status, answer.getheader('Content-Type', ''), answer.read()
        conn.close()
        return status, (json.loads(raw) if kind.startswith('application/json') else raw)

    def atlas_token(self, token='dm-token'):
        status, data = self.call('GET', '/api/atlas/session', token=token)
        if status != 200:
            raise AssertionError(f'Atlas session: {status} {data}')
        return data['token']


class Host(unittest.TestCase):
    """A studio with the stand-in DM, made afresh for each test, Atlas on the old map's archive dev-old (a side project, in name only)."""

    def setUp(self):
        self.host = Running(StandInDM(), 'dev-old')
        self.dm, self.chosen, self.made = self.host.dm, self.host.chosen, self.host.made
        self.call, self.atlas_token = self.host.call, self.host.atlas_token

    def tearDown(self):
        self.host.stop()


class StudioTests(Host):
    def test_the_app_is_served_to_anyone_on_this_computer_and_nothing_to_another_origin(self):
        status, page = self.call('GET', '/')
        self.assertEqual(status, 200)
        self.assertIn(b'<!doctype html>', page.lower())
        status, data = self.call('GET', '/api/studio/ping', headers={'Origin': 'http://example.com'})
        self.assertEqual(status, 403, data)
        status, data = self.call('GET', '/api/studio/ping', headers={'Host': 'example.com'})
        self.assertEqual(status, 403, data)
        self.assertEqual(self.call('GET', '/api/studio/ping'), (200, {'app': 'ratw-studio', 'suite': 'atlas'}))

    def test_one_sign_in_for_both_suites_and_each_suite_its_worlds(self):
        self.assertEqual(self.call('GET', '/api/studio/session')[0], 401)
        status, data = self.call('GET', '/api/studio/session', token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertEqual(data['me'], {'username': 'dm-user', 'role': 'dm'})
        self.assertTrue(data['atlas']['available'])
        self.assertEqual(data['atlas']['world']['key'], 'dev-old')
        self.assertEqual([w['key'] for w in data['atlas']['worlds']], W.keys('dev'))
        self.assertEqual([w['key'] for w in data['dm']['worlds']], W.keys(), 'the DM may work on any world')
        self.assertEqual((data['dm']['live'], data['dm']['world']), (W.live_world(), W.dm_world()))
        self.assertNotIn('database', json.dumps(data), 'no database names leave the host')
        self.assertFalse(self.call('GET', '/api/studio/session', token='viewer-token')[1]['atlas']['available'])

    def test_atlas_is_for_dm_and_admin_accounts(self):
        self.assertEqual(self.call('GET', '/api/atlas/session')[0], 401)
        self.assertEqual(self.call('GET', '/api/atlas/session', token='viewer-token')[0], 403)
        self.assertEqual(self.call('GET', '/api/atlas/layers/npcs', token='viewer-token')[0], 403)
        self.assertTrue(self.atlas_token('admin-token'))
        self.assertEqual(self.dm.calls, [])

    def test_atlas_writes_still_need_atlas_session_token(self):
        status, project = self.call('GET', '/api/atlas/demo', token='dm-token')
        self.assertEqual(status, 200, project)
        self.assertEqual(self.call('POST', '/api/atlas/validate', project, token='dm-token')[0], 403)
        status, data = self.call('POST', '/api/atlas/validate', project, token='dm-token',
                                 headers={'X-RATW-Editor': self.atlas_token()})
        self.assertEqual(status, 200, data)
        self.assertTrue(data['valid'])
        # ...and the token alone, without a sign-in, is not enough.
        self.assertEqual(self.call('POST', '/api/atlas/validate', project, headers={'X-RATW-Editor': self.atlas_token()})[0], 401)

    def test_push_to_live_is_not_in_atlas(self):
        token = self.atlas_token()
        status, data = self.call('GET', '/api/atlas/publish', token='admin-token')
        self.assertEqual(status, 404, data)
        self.assertIn('Import from dev', data['error'])
        for path in ('push', 'rollback', 'pull'):
            status, _ = self.call('POST', f'/api/atlas/publish/{path}', {'password': 'x'}, token='admin-token',
                                  headers={'X-RATW-Editor': token})
            self.assertEqual(status, 404, path)

    def test_atlas_offers_no_storyline_or_live_routes(self):
        for path in ('storytellers', 'chapters', 'live', 'live/fame', 'players', 'reports', 'artwork', 'calendar', 'money',
                     'health', 'chronicle', 'me', 'worlds'):
            for prefix in ('/api/atlas/layers/', '/api/atlas/'):
                self.assertEqual(self.call('GET', prefix + path, token='admin-token')[0], 404, prefix + path)
        for path in ('milestone', 'storytellers/visitor', 'chapters/decide', 'chronicle/story', 'npcs/life', 'live/move',
                     'live/visit', 'live/action', 'actions', 'festivals/call', 'economy/tax', 'reports/decide', 'login'):
            self.assertEqual(self.call('POST', '/api/atlas/layers/' + path, {'target': 'dev-old'}, token='admin-token')[0],
                             404, path)
        self.assertEqual(self.dm.calls, [], 'nothing reached the DM')

    def test_the_dm_suite_offers_no_map_or_npc_authoring(self):
        for path in sorted(ST.MOVED_TO_ATLAS):
            status, data = self.call('POST', '/api/dm' + path[len('/api'):], {'target': 'prod', 'id': 'x'}, token='admin-token')
            self.assertEqual(status, 404, (path, data))
        for path in ('live/edit', 'live/sync', 'validate', 'export', 'roster', 'playtest'):     # Atlas's own routes.
            self.assertEqual(self.call('POST', '/api/dm/' + path, {}, token='admin-token')[0], 404, path)
        self.assertEqual(self.dm.calls, [], 'nothing reached the DM')
        # Live life stays: kill and revive, the players, the storylines.
        status, data = self.call('POST', '/api/dm/npcs/life', {'id': 'npc_ada', 'dead': True}, token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertEqual(self.dm.calls[-1][0], 'npc_life')
        self.assertEqual(self.dm.calls[-1][1][1], W.dm_world(), 'the DM\'s world by default')
        for path in ('players', 'storytellers', 'chapters', 'calendar', 'npcs', 'world'):
            self.assertEqual(self.call('GET', '/api/dm/' + path, token='viewer-token')[0], 200, path)

    def test_the_dm_suite_works_on_its_own_world_chosen_apart_from_atlas(self):
        # Any world: a live one, a dev one (DEV run as the server, the same world Atlas edits, or another).
        for key in ('prod', 'dev', 'dev-old'):
            status, data = self.call('GET', f'/api/dm/players?target={key}', token='admin-token')
            self.assertEqual((status, data['target']), (200, key))
        # Choosing the DM's world: dm and admin; Atlas's stays as it was.
        self.assertEqual(self.call('POST', '/api/studio/dm-world', {'key': 'prod'}, token='viewer-token')[0], 403)
        self.assertEqual(self.call('POST', '/api/studio/dm-world', {'key': 'nowhere'}, token='dm-token')[0], 400)
        status, data = self.call('POST', '/api/studio/dm-world', {'key': 'prod'}, token='dm-token')
        self.assertEqual((status, data['world']['key']), (200, 'prod'), data)
        self.assertEqual((self.host.dm_chosen, self.chosen), (['prod'], []))
        self.assertEqual(self.call('POST', '/api/studio/dm-world', {'key': 'ridgemere2'}, token='admin-token')[1]['world']['key'], 'dev')

    def test_atlas_npc_pages_work_on_atlas_world_only(self):
        status, data = self.call('GET', '/api/atlas/layers/npcs', token='dm-token')
        self.assertEqual((status, data), (200, {'called': 'npcs', 'target': 'dev-old'}))
        status, data = self.call('GET', f'/api/atlas/layers/npcs?target={W.live_world()}', token='dm-token')
        self.assertEqual(status, 409, data)
        route = {'id': 'north_walk', 'name': 'North walk', 'posts': []}
        status, data = self.call('POST', '/api/atlas/layers/routes/save', {'route': route}, token='dm-token')
        self.assertEqual(status, 200, data)
        name, args = self.dm.calls[-1]
        self.assertEqual((name, args[1], args[2]), ('save_route', 'dev-old', route))
        self.assertEqual(args[0]['username'], 'dm-user', 'audited as the signed-in account')
        status, _ = self.call('POST', '/api/atlas/layers/factions/save', {'target': W.live_world(), 'faction': {}}, token='dm-token')
        self.assertEqual(status, 409, 'never a live world from Atlas')
        self.assertEqual(self.call('GET', '/api/atlas/layers/actions/7', token='dm-token')[0], 200)

    def test_switching_atlas_world(self):
        self.assertEqual(self.call('POST', '/api/studio/atlas-world', {'key': 'dev'}, token='viewer-token')[0], 403)
        status, data = self.call('POST', '/api/studio/atlas-world', {'key': 'prod'}, token='dm-token')
        self.assertEqual(status, 400, data)
        self.assertEqual(self.chosen, [])
        old = self.atlas_token()
        status, data = self.call('POST', '/api/studio/atlas-world', {'key': 'dev'}, token='dm-token')
        self.assertEqual((status, data['world']['key']), (200, 'dev'), data)
        self.assertEqual((self.chosen, self.made), (['dev'], ['dev-old', 'dev']))
        self.assertEqual(self.call('GET', '/api/atlas/layers/npcs', token='dm-token')[1]['target'], 'dev')
        self.assertEqual(self.call('GET', '/api/studio/session', token='dm-token')[1]['atlas']['world']['key'], 'dev')
        self.assertNotEqual(self.atlas_token(), old, 'a new Atlas session for the new world')

    def test_the_old_hosts_paths_are_not_the_studio_s(self):
        for path in ('/api/session', '/api/me', '/api/live/world', '/api/players'):
            self.assertEqual(self.call('GET', path, token='admin-token')[0], 404, path)

    def test_route_sets_are_apart(self):
        self.assertFalse(ST.MOVED_TO_ATLAS & ST.LAYER_READS)
        self.assertTrue(all(ST.layer_offers('POST', p) and not ST.dm_offers('POST', p) for p in ST.MOVED_TO_ATLAS))
        self.assertFalse(ST.layer_offers('GET', '/api/actions/7/x'))


class SignInTests(Fixture):
    """A DM account signs in once for the whole tool; its role decides Atlas; the moved authoring reaches Atlas's world."""

    def setUp(self):
        super().setUp()
        self.host = Running(self.dm, 'dev')       # The fixture's scratch DEV is Atlas's world here.

    def tearDown(self):
        self.host.stop()

    def login(self, username):
        status, data = self.host.call('POST', '/api/dm/login', {'username': username, 'password': f'{username}-password'})
        self.assertEqual(status, 200, data)
        return data['token']

    def test_one_sign_in_opens_both_suites(self):
        master, viewer = self.login('dm-master'), self.login('dm-viewer')
        self.assertEqual(self.host.call('GET', '/api/dm/me', token=master)[1]['role'], 'dm')
        self.assertTrue(self.host.atlas_token(master))
        self.assertEqual(self.host.call('GET', '/api/atlas/session', token=viewer)[0], 403)
        self.assertEqual(self.host.call('GET', '/api/dm/players?target=prod', token=viewer)[0], 200)
        status, _ = self.host.call('POST', '/api/dm/logout', {}, token=master)
        self.assertEqual(status, 200)
        self.assertEqual(self.host.call('GET', '/api/atlas/session', token=master)[0], 401)

    def test_npc_areas_are_painted_in_atlas_world_not_from_the_dm(self):
        master = self.login('dm-master')
        cell = greyfen()['cells'][0]['id']
        area = {'id': 'studio_meadow', 'name': 'Studio meadow', 'kind': 'wander', 'cell': cell, 'tiles': [[1, 1], [2, 1]]}
        self.assertEqual(self.host.call('POST', '/api/dm/areas/save', {'target': 'prod', 'area': area}, token=master)[0], 404)
        status, data = self.host.call('POST', '/api/atlas/layers/areas/save', {'area': area}, token=master)
        self.assertEqual(status, 200, data)
        with W.connect('dev', 'dm', dbname=self.names['dev']) as conn:
            self.assertEqual(conn.execute("SELECT name FROM live.npc_areas WHERE id = 'studio_meadow'").fetchone(), ('Studio meadow',))
        with W.connect('prod', 'dm', dbname=self.names['prod']) as conn:
            self.assertIsNone(conn.execute("SELECT 1 FROM live.npc_areas WHERE id = 'studio_meadow'").fetchone())
        status, data = self.host.call('GET', '/api/atlas/layers/npcs', token=master)
        self.assertEqual((status, data['target']), (200, 'dev'))
        self.assertIn('studio_meadow', [a['id'] for a in data['areas']])


class RouteHost:
    """The new routes with stand-ins: who may call them, what reaches the engine, and its errors as HTTP. Atlas on DEV
    (in name only: its upstream is PROD); a scratch registry adds a second live world for the switch."""

    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        registry = Path(self.folder.name) / 'worlds.json'
        entries = json.loads(W.WORLDS_FILE.read_text(encoding='utf-8'))['worlds']
        entries.append({'key': 'other', 'name': 'Other live', 'database': 'ratw_other_unused', 'kind': 'live'})
        registry.write_text(json.dumps({'worlds': entries}), encoding='utf-8')
        self.saved = {k: os.environ.get(k) for k in ('RATW_WORLDS_FILE', 'RATW_WORLDS_LOCAL')}
        os.environ.update({'RATW_WORLDS_FILE': str(registry), 'RATW_WORLDS_LOCAL': str(Path(self.folder.name) / 'local.json')})
        W.worlds()
        W.set_local(live=self.LIVE)
        self.sync, self.builds = StandInSync(), StandInBuilds()
        self.host = Running(StandInDM(), 'dev', self.sync, builds=self.builds)
        self.dm, self.call = self.host.dm, self.host.call

    LIVE = 'prod'          # (PROD as the live server: the import flow. ServedDevRouteTests: DEV, the default now.)

    def tearDown(self):
        self.host.stop()
        for key, value in self.saved.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value
        W.worlds()
        self.folder.cleanup()

    def logged(self):
        return [args for name, args in self.dm.calls if name == 'log']


class SyncRouteTests(RouteHost, unittest.TestCase):
    """PROD as the live server (selectable): the import flow."""

    def test_update_from_live_needs_an_upstream_a_builder_and_atlas_token_to_pull(self):
        self.assertEqual(self.call('GET', '/api/atlas/sync/diff')[0], 401)
        self.assertEqual(self.call('GET', '/api/atlas/sync/diff', token='viewer-token')[0], 403)
        status, data = self.call('GET', '/api/atlas/sync/diff', token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertEqual((data['called'], data['devWorld']['key'], data['liveWorld']['key']), ('diff', 'dev', 'prod'))
        self.assertEqual(data['liveServer'], {'state': {'state': 'running', 'by': '', 'at': None, 'note': ''}, 'running': False,
                                              'release': 4})
        body = {'choices': {'live.npcs/wren': {'name': 'live'}}, 'expect': 'abc'}
        self.assertEqual(self.call('POST', '/api/atlas/sync/pull', body, token='dm-token')[0], 403, 'Atlas\'s token too')
        status, data = self.call('POST', '/api/atlas/sync/pull', body, token='dm-token',
                                 headers={'X-RATW-Editor': self.host.atlas_token()})
        self.assertEqual(status, 200, data)
        self.assertEqual(self.sync.calls[-1], ('pull', ('dev', {'live.npcs/wren': {'name': 'live'}}, 'dm-user', 'abc')))
        (who, world, action, target, detail), = self.logged()
        self.assertEqual((who['username'], world, action, target), ('dm-user', 'dev', 'world.pull', 'prod'))
        self.assertIn('3 rows', detail)
        # Another map: refused, both maps named, for the page to say so.
        maps = {'dev': {'id': 'map-new', 'name': 'Ridgemere 2.0: the city'}, 'live': {'id': 'original:w', 'name': "W's original map"},
                'same': False}
        self.sync.fail = WS.DifferentMap('PROD is on a different map: there is nothing to update from.', maps)
        status, data = self.call('GET', '/api/atlas/sync/diff', token='dm-token')
        self.assertEqual((status, data['maps'], data['error']), (409, maps, 'PROD is on a different map: there is nothing to update from.'))
        self.sync.fail = None
        # A side project has nothing to update from.
        self.assertEqual(self.call('POST', '/api/studio/atlas-world', {'key': 'dev-old'}, token='dm-token')[0], 200)
        status, data = self.call('GET', '/api/atlas/sync/diff', token='dm-token')
        self.assertEqual(status, 409, data)
        self.assertIn('no upstream', data['error'])

    def test_server_status_and_maintenance(self):
        status, data = self.call('GET', '/api/dm/server/status', token='viewer-token')
        self.assertEqual(status, 200, data)
        self.assertEqual((data['key'], data['live']), ('prod', 'prod'))
        self.assertIn('rarely wise', data['warning'])
        self.assertEqual(self.call('GET', '/api/dm/server/status?world=other', token='viewer-token')[1]['key'], 'other')
        self.assertEqual(self.call('GET', '/api/dm/server/status?world=dev', token='viewer-token')[1]['kind'], 'dev', 'a dev world too')
        self.assertEqual(self.call('GET', '/api/dm/server/status?world=nowhere', token='viewer-token')[0], 400)
        self.assertEqual(self.call('POST', '/api/dm/server/maintenance', {'on': True}, token='viewer-token')[0], 403)
        self.assertEqual(self.call('POST', '/api/dm/server/maintenance', {'on': 'yes'}, token='dm-token')[0], 400)
        status, data = self.call('POST', '/api/dm/server/maintenance', {'on': True, 'note': 'New houses'}, token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertEqual(self.sync.calls[-1], ('maintenance', ('prod', True, 'dm-user', 'New houses')))
        self.call('POST', '/api/dm/server/maintenance', {'world': 'prod', 'on': False}, token='admin-token')
        self.assertEqual(self.sync.calls[-1], ('maintenance', ('prod', False, 'admin-user', '')))
        self.assertEqual([a[2] for a in self.logged()], ['server.maintenance', 'server.running'])

    def test_switching_the_live_world_asks_twice(self):
        for body, why in (({'world': 'other'}, 'no confirmation'), ({'world': 'other', 'confirm': 'yes'}, 'not the key'),
                          ({'world': 'nowhere', 'confirm': 'nowhere'}, 'no such world'), ({'world': 'prod', 'confirm': 'prod'}, 'already live')):
            status, data = self.call('POST', '/api/dm/server/switch', body, token='admin-token')
            self.assertEqual(status, 400, (why, data))
        self.assertIn('rarely wise', self.call('POST', '/api/dm/server/switch', {'world': 'other'}, token='admin-token')[1]['error'])
        self.assertEqual(self.call('POST', '/api/dm/server/switch', {'world': 'other', 'confirm': 'other'}, token='viewer-token')[0], 403)
        self.assertEqual(self.sync.calls, [])
        self.sync.fail = WS.NotInMaintenance('prod is not in maintenance.')
        status, data = self.call('POST', '/api/dm/server/switch', {'world': 'other', 'confirm': 'other'}, token='admin-token')
        self.assertEqual((status, data['error']), (409, 'prod is not in maintenance.'))
        self.sync.fail = None
        status, data = self.call('POST', '/api/dm/server/switch', {'world': 'other', 'confirm': 'other'}, token='admin-token')
        self.assertEqual(status, 200, data)
        self.assertEqual(self.sync.calls[-1], ('switch_live', ('other', 'admin-user')))
        self.assertEqual([(a[1], a[2]) for a in self.logged()], [('prod', 'server.switch'), ('other', 'server.switch')])

    def test_import_plan_lists_the_live_world_s_copies(self):
        status, data = self.call('GET', '/api/dm/import/plan', token='viewer-token')
        self.assertEqual(status, 200, data)
        self.assertEqual(([c['key'] for c in data['candidates']], data['dev'], data['live']['key']), (['dev'], 'dev', 'prod'))
        self.assertEqual(self.sync.calls[-1], ('plan_import', ('dev',)))
        self.assertEqual(self.call('GET', '/api/dm/import/plan?dev=dev-old', token='viewer-token')[0], 400, 'not a copy of prod')
        self.sync.fail = WS.PublishError('dev has no base yet: take one.')
        status, data = self.call('GET', '/api/dm/import/plan', token='viewer-token')
        self.assertEqual((status, data['plan'], data['problem']), (200, None, 'dev has no base yet: take one.'), 'said, with the worlds')

    def test_import_and_the_engine_s_errors(self):
        body = {'dev': 'dev', 'choices': {'live.npcs/wren': 'dev'}, 'password': 'secret', 'note': 'Houses', 'expect': 'd1'}
        self.assertEqual(self.call('POST', '/api/dm/import', body, token='viewer-token')[0], 403)
        self.assertEqual(self.call('POST', '/api/dm/import', {**body, 'password': ''}, token='dm-token')[0], 400)
        self.assertEqual(self.call('POST', '/api/dm/import', {**body, 'dev': 'dev-old'}, token='dm-token')[0], 400)
        self.assertEqual(self.sync.calls, [])
        status, data = self.call('POST', '/api/dm/import', body, token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertEqual(self.sync.calls[-1], ('import_', ('dev', {'live.npcs/wren': 'dev'}, 'secret', 'dm-user', 'Houses', 'd1', False)))
        self.assertEqual(self.logged()[-1][1:4], ('prod', 'world.import', 'dev'))
        # Another map: the replace-the-map confirmation reaches the engine only as a true replaceMap.
        for asked, sent in (('yes', False), (True, True)):
            self.assertEqual(self.call('POST', '/api/dm/import', {**body, 'replaceMap': asked}, token='dm-token')[0], 200)
            self.assertEqual(self.sync.calls[-1][1][-1], sent)
        maps = {'dev': {'id': 'map-new', 'name': 'Ridgemere 2.0: the city'}, 'live': {'id': 'original:w', 'name': "W's original map"},
                'same': False}
        self.sync.fail = WS.DifferentMap('PROD is on a different map: confirm it.', maps)
        status, data = self.call('POST', '/api/dm/import', body, token='dm-token')
        self.assertEqual((status, data['maps']), (409, maps), 'the screens are told both maps')
        left = [{'id': 'live.npcs/wren', 'fields': []}]
        for error, code in ((WS.WrongPassword('Wrong publish password.'), 401), (WS.Locked('Too many tries.'), 429),
                            (WS.NotInMaintenance('Not in maintenance.'), 409), (WS.Moved('The worlds changed.'), 409),
                            (WS.Unresolved('1 conflict to resolve.', left), 409),
                            (WS.PublishError('Refused.', ['rook would be stranded']), 422)):
            self.sync.fail = error
            status, data = self.call('POST', '/api/dm/import', body, token='dm-token')
            self.assertEqual((status, data['error']), (code, str(error)), type(error).__name__)
            self.assertEqual(data['errors'], error.errors)
        self.assertEqual(data['errors'], ['rook would be stranded'])
        self.sync.fail = WS.Unresolved('1 conflict to resolve.', left)
        self.assertEqual(self.call('POST', '/api/dm/import', body, token='dm-token')[1]['conflicts'], left)

    def test_releases_and_rollback(self):
        status, data = self.call('GET', '/api/dm/releases', token='viewer-token')
        self.assertEqual((status, data['world']), (200, 'prod'))
        self.assertEqual(self.call('POST', '/api/dm/rollback', {'number': 2, 'password': 'p'}, token='viewer-token')[0], 403)
        self.assertEqual(self.call('POST', '/api/dm/rollback', {'number': 'two', 'password': 'p'}, token='dm-token')[0], 400)
        status, data = self.call('POST', '/api/dm/rollback', {'number': 2, 'password': 'p', 'note': 'Oops'}, token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertEqual(self.sync.calls[-1], ('rollback', ('prod', 2, 'p', 'dm-user', 'Oops')))
        self.assertEqual(self.logged()[-1][2:4], ('world.rollback', '2'))

    def test_the_session_says_where_switching_is(self):
        self.assertIn('Server & import', self.call('GET', '/api/studio/session', token='dm-token')[1]['dm']['switch'])


class ServedDevRouteTests(RouteHost, unittest.TestCase):
    """DEV run as the only server (the default since 2026-10-31: the live choice and the DM on DEV, Atlas on DEV too):
    the DM's server routes on a dev world, what makes no sense there refused with why, Atlas's edits to the server
    (Rebuild), starting a server, and Atlas's Play."""

    LIVE = 'dev'

    def test_the_dm_runs_dev_as_the_server(self):
        session = self.call('GET', '/api/studio/session', token='dm-token')[1]
        self.assertEqual((session['dm']['world'], session['dm']['live'], session['atlas']['world']['key']), ('dev', 'dev', 'dev'))
        status, data = self.call('GET', '/api/dm/server/status', token='viewer-token')
        self.assertEqual((status, data['key'], data['kind'], data['live']), (200, 'dev', 'dev', 'dev'), data)
        self.assertEqual(data['build'], {'build': 7, 'at': None, 'by': '', 'release': None, 'current': True})
        self.assertIn('nothing to import', data['servedDev'])
        self.assertIn('prod', [w['key'] for w in data['switchTo']], 'PROD stays selectable')
        # Maintenance works as on a live world.
        self.assertEqual(self.call('POST', '/api/dm/server/maintenance', {'on': True}, token='dm-token')[0], 200)
        self.assertEqual(self.sync.calls[-1], ('maintenance', ('dev', True, 'dm-user', '')))
        # The DM's NPC life and players go to DEV.
        self.assertEqual(self.call('POST', '/api/dm/npcs/life', {'id': 'wren', 'dead': True}, token='dm-token')[0], 200)
        self.assertEqual(self.dm.calls[-1][1][1], 'dev')

    def test_import_and_rollback_are_refused_on_dev_with_why(self):
        status, data = self.call('GET', '/api/dm/import/plan', token='viewer-token')
        self.assertEqual((status, data['candidates'], data['plan']), (200, [], None))
        self.assertIn('nothing to import', data['problem'])
        status, data = self.call('POST', '/api/dm/import', {'dev': 'dev', 'password': 'p'}, token='dm-token')
        self.assertEqual(status, 409, data)
        self.assertIn('nothing to import', data['error'])
        status, data = self.call('GET', '/api/dm/releases', token='viewer-token')
        self.assertEqual((status, data['releases']), (200, []))
        self.assertIn('builds, not releases', data['note'])
        status, data = self.call('POST', '/api/dm/rollback', {'number': 2, 'password': 'p'}, token='dm-token')
        self.assertEqual(status, 409, data)
        self.assertEqual([c for c in self.sync.calls if c[0] in ('import_', 'rollback', 'plan_import')], [], 'nothing reached the engine')
        # Into PROD (the DM pointed at it), the import is offered again.
        status, data = self.call('GET', '/api/dm/import/plan?world=prod', token='viewer-token')
        self.assertEqual(([c['key'] for c in data['candidates']], data['live']['key']), (['dev'], 'prod'))

    def test_rebuild_sends_atlas_s_edits_to_the_server(self):
        self.assertEqual(self.call('POST', '/api/dm/server/rebuild', {}, token='viewer-token')[0], 403)
        status, data = self.call('POST', '/api/dm/server/rebuild', {'world': 'prod'}, token='dm-token')
        self.assertEqual(status, 409, 'a live world\'s builds come from its imports')
        self.builds.current = False
        status, data = self.call('POST', '/api/dm/server/rebuild', {}, token='dm-token')
        self.assertEqual((status, data['made'], data['build']), (200, True, 7), data)
        self.assertEqual(self.builds.calls, [('rebuild', 'dev', 'dm-user')])
        self.assertEqual(self.logged()[-1][1:3], ('dev', 'server.rebuild'))

    def test_switching_the_live_server_to_a_dev_world_and_back(self):
        W.set_local(live='prod')
        status, data = self.call('POST', '/api/dm/server/switch', {'world': 'dev', 'confirm': 'dev'}, token='admin-token')
        self.assertEqual(status, 200, data)
        self.assertEqual(self.sync.calls[-1], ('switch_live', ('dev', 'admin-user')))

    def test_starting_a_world_s_server(self):
        self.assertEqual(self.call('POST', '/api/dm/server/start', {}, token='viewer-token')[0], 403)
        status, data = self.call('POST', '/api/dm/server/start', {}, token='dm-token')
        self.assertEqual((status, data['started'], data['world']), (200, True, 'dev'), data)
        self.assertEqual(self.host.spawned[0][0], 'dev')
        self.assertIn(self.host.spawned[0][1], ST.GAME_PORTS)
        status, data = self.call('POST', '/api/dm/server/start', {}, token='dm-token')
        self.assertEqual((status, data['started'], data['starting']), (200, False, True), 'once: it is still starting')
        self.assertEqual(len(self.host.spawned), 1)
        self.assertTrue(self.call('GET', '/api/dm/server/status', token='viewer-token')[1]['starting'])

    def test_play_opens_the_running_server_of_atlas_s_world(self):
        self.assertEqual(self.call('GET', '/api/atlas/play/server', token='viewer-token')[0], 403)
        status, data = self.call('GET', '/api/atlas/play/server', token='dm-token')
        self.assertEqual((status, data['play'], data['canStart'], data['world']['key']), (200, None, True, 'dev'), data)
        self.assertIn('No game server is running', data['why'])
        self.assertIn('bash tools/live.sh server dev', data['why'])
        holder = {'pid': 4, 'host': 'elsewhere-not-here', 'port': 7790, 'local': False, 'name': 'ratw_server pid 4 on elsewhere-not-here port 7790'}
        self.sync.holder = holder
        status, data = self.call('GET', '/api/atlas/play/server', token='dm-token')
        self.assertEqual(data['play'], {'port': 7790, 'host': 'elsewhere-not-here', 'local': False}, 'a server on another computer')
        self.sync.holder = {**holder, 'host': ST.socket.gethostname()}
        status, data = self.call('GET', '/api/atlas/play/server', token='dm-token')
        self.assertEqual(data['play'], {'port': 7790, 'host': None, 'local': False}, 'this computer: the page\'s own host')
        self.assertFalse(data['canStart'])
        status, data = self.call('POST', '/api/atlas/play/server/start', {}, token='dm-token')
        self.assertEqual(status, 409, 'already running')
        self.sync.holder = None
        status, data = self.call('POST', '/api/atlas/play/server/start', {}, token='dm-token')
        self.assertEqual((status, data['started']), (200, True), data)
        self.assertEqual(self.host.spawned[-1][0], 'dev')

    def test_play_addresses(self):
        self.assertIsNone(ST.play_address(None))
        self.assertIsNone(ST.play_address({'port': None, 'host': 'h'}), 'a server that doesn\'t say its port')
        self.assertEqual(ST.play_address({'port': 7788, 'host': 'h', 'local': True}, here='h'), {'port': 7788, 'host': None, 'local': True})
        self.assertEqual(ST.play_address({'port': 7788, 'host': 'h.local'}, here='h')['host'], None)


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class SyncFlowTests(ScratchWorlds, unittest.TestCase):
    """The user's flow through the studio on throwaway worlds (a live Greyfen and its dev copy): Atlas updates from
    live choosing a conflict field by field; the DM closes the world, imports choosing a conflict, rolls back and
    reopens; and the live switch. The DM's accounts are stand-ins; its audit is the real one, in each world."""

    def setUp(self):
        super().setUp()
        W.set_local(atlas=self.dev, live=self.live)
        self.dm = StandInDM()
        self.dm.log = D.DungeonMaster(lambda key: W.connect(key, 'dm')).log
        self.host = Running(self.dm, self.dev)
        self.call = self.host.call

    def tearDown(self):
        self.host.stop()
        super().tearDown()

    def audit(self, key):
        return [r[0] for r in self.sql(key, 'SELECT action FROM dm.audit ORDER BY at, action')]

    def test_update_from_live_then_import_into_it(self):
        self.sql(self.dev, "UPDATE live.npcs SET name = 'Wrenna', greeting = 'Dev greeting.' WHERE id = 'wren'")
        self.sql(self.live, "UPDATE live.npcs SET name = 'Wren the Elder', purse = 5 WHERE id = 'wren'")
        self.sql(self.live, "INSERT INTO live.factions (world_id, id, name) VALUES ('greyfen', 'wardens', 'Wardens')")
        status, plan = self.call('GET', '/api/atlas/sync/diff', token='dm-token')
        self.assertEqual(status, 200, plan)
        self.assertEqual((plan['counts'], plan['liveWorld']['key']), ({'live': 1, 'dev': 0, 'conflicts': 1}, self.live))
        self.assertTrue(plan['warnings'], 'the live world is running')
        token = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/sync/pull', {'expect': plan['digest']}, token='dm-token', headers=token)
        self.assertEqual(status, 409, data)
        self.assertEqual([c['id'] for c in data['conflicts']], ['live.npcs/wren'])
        status, data = self.call('POST', '/api/atlas/sync/pull', {'choices': {'live.npcs/wren': 'dev'}, 'expect': '0' * 40},
                                 token='dm-token', headers=token)
        self.assertEqual(status, 409, 'the worlds moved since that plan')
        status, data = self.call('POST', '/api/atlas/sync/pull', {'choices': {'live.npcs/wren': {'name': 'dev'}},
                                                                  'expect': plan['digest']}, token='dm-token', headers=token)
        self.assertEqual(status, 200, data)
        self.assertEqual(self.sql(self.dev, "SELECT name, greeting, purse FROM live.npcs WHERE id = 'wren'")[0],
                         ('Wrenna', 'Dev greeting.', 5))
        self.assertEqual(self.one(self.dev, "SELECT name FROM live.factions WHERE id = 'wardens'"), 'Wardens')
        self.assertEqual(self.audit(self.dev), ['world.pull'])

        # The DM: Import from dev is blocked until maintenance.
        status, data = self.call('GET', '/api/dm/import/plan', token='viewer-token')
        self.assertEqual(status, 200, data)
        self.assertEqual(([c['key'] for c in data['candidates']], data['dev']), ([self.dev], self.dev))
        self.assertTrue(any('not in maintenance' in b for b in data['plan']['blocked']))
        self.assertEqual([c['id'] for c in data['plan']['devChanges']], ['live.npcs/wren'])
        body = {'dev': self.dev, 'password': 'password', 'note': 'Wren renamed'}
        self.assertEqual(self.call('POST', '/api/dm/import', body, token='dm-token')[0], 409, 'not in maintenance')
        status, data = self.call('POST', '/api/dm/server/maintenance', {'on': True, 'note': 'Renaming'}, token='dm-token')
        self.assertEqual((status, data['server']['state'], data['server']['by']), (200, 'maintenance', 'dm-user'), data)
        status, data = self.call('GET', '/api/dm/server/status', token='viewer-token')
        self.assertEqual((data['key'], data['server']['state'], data['serverRunning']), (self.live, 'maintenance', False))
        # Live moves during maintenance (the DM's own change): a conflict at import.
        self.sql(self.live, "UPDATE live.npcs SET greeting = 'Live greeting.' WHERE id = 'wren'")
        status, plan = self.call('GET', '/api/dm/import/plan', token='dm-token')
        self.assertEqual((plan['plan']['blocked'], [c['id'] for c in plan['plan']['conflicts']]), ([], ['live.npcs/wren']))
        self.assertEqual(self.call('POST', '/api/dm/import', {**body, 'password': 'guess'}, token='dm-token')[0], 401)
        status, data = self.call('POST', '/api/dm/import', body, token='dm-token')
        self.assertEqual((status, [c['id'] for c in data['conflicts']]), (409, ['live.npcs/wren']))
        status, data = self.call('POST', '/api/dm/import', {**body, 'choices': {'live.npcs/wren': {'greeting': 'live'}},
                                                             'expect': plan['plan']['digest']}, token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertEqual(data['release'], 1)
        self.assertEqual(self.sql(self.live, "SELECT name, greeting FROM live.npcs WHERE id = 'wren'")[0], ('Wrenna', 'Live greeting.'))
        status, data = self.call('GET', '/api/dm/releases', token='viewer-token')
        self.assertEqual([(r['number'], r['kind'], r['by']) for r in data['releases']], [(1, 'import', 'dm-user')])
        self.sql(self.dev, "UPDATE world.interiors SET name = 'The New Shop' WHERE id = 'shop'")
        status, data = self.call('POST', '/api/dm/import', {**body, 'note': 'Shop sign'}, token='dm-token')
        self.assertEqual((status, data.get('release')), (200, 2), data)
        status, data = self.call('POST', '/api/dm/rollback', {'number': 1, 'password': 'password', 'note': 'Old sign'}, token='dm-token')
        self.assertEqual((status, data.get('release')), (200, 3), data)
        self.assertNotEqual(self.one(self.live, "SELECT name FROM world.interiors WHERE id = 'shop'"), 'The New Shop')
        status, data = self.call('POST', '/api/dm/server/maintenance', {'on': False}, token='dm-token')
        self.assertEqual((status, data['server']['state']), (200, 'running'))
        self.assertEqual(self.audit(self.live), ['server.maintenance', 'world.import', 'world.import', 'world.rollback',
                                                 'server.running'])

    def test_switching_the_live_world(self):
        other = f'{self.live}o'
        W.new_world(other, 'Other', 'live', source=self.seed, log=lambda *_: None)
        try:
            switch = {'world': other, 'confirm': other}
            self.assertEqual(self.call('POST', '/api/dm/server/switch', {'world': other, 'confirm': 'x'}, token='admin-token')[0], 400)
            status, data = self.call('POST', '/api/dm/server/switch', switch, token='admin-token')
            self.assertEqual(status, 409, data)
            self.assertIn('not in maintenance', data['error'])
            self.call('POST', '/api/dm/server/maintenance', {'on': True}, token='admin-token')
            status, data = self.call('POST', '/api/dm/server/switch', switch, token='admin-token')
            self.assertEqual((status, data['live'], data['was']), (200, other, self.live), data)
            self.assertEqual(W.live_world(), other)
            status, data = self.call('GET', '/api/dm/server/status', token='viewer-token')
            self.assertEqual((data['key'], data['server']['state']), (other, 'maintenance'), 'it opens when the DM turns it on')
            self.assertEqual(self.call('GET', '/api/studio/session', token='viewer-token')[1]['dm']['live'], other)
            self.assertEqual(self.audit(other), ['server.switch'])
            # Atlas's dev world is a copy of the old live world: nothing to import into the new one.
            self.assertEqual(self.call('GET', '/api/dm/import/plan', token='dm-token')[1]['candidates'], [])
        finally:
            with superuser() as su:
                su.execute(f'DROP DATABASE IF EXISTS ratw_{other} WITH (FORCE)')


def unmigrate_farms(key):
    """Takes migration 0060 back off a throwaway world (0058 and 0059 kept): as a world is until it is migrated."""
    sql = (W.MIGRATIONS / '0058_rules.sql').read_text(encoding='utf-8')
    manifest = sql[sql.index('CREATE OR REPLACE FUNCTION live.people_manifest'):]
    with W.connect(key, 'owner') as conn:
        with conn.transaction():
            conn.execute('ALTER TABLE live.economy DROP COLUMN farms')
            conn.execute(manifest)
            conn.execute("DELETE FROM public.schema_migrations WHERE version = '0060'")


def unmigrate_rules(key):
    """Takes migrations 0060 and 0058 back off a throwaway world (0057 kept): as a world is until it is migrated."""
    unmigrate_farms(key)
    sql = (W.MIGRATIONS / '0057_seasons.sql').read_text(encoding='utf-8')
    manifest = sql[sql.index('CREATE OR REPLACE FUNCTION live.people_manifest'):]
    with W.connect(key, 'owner') as conn:
        with conn.transaction():
            conn.execute('ALTER TABLE live.economy DROP COLUMN rules, DROP COLUMN services')
            conn.execute(manifest)
            conn.execute("DELETE FROM public.schema_migrations WHERE version = '0058'")


def unmigrate_seasons(key):
    """Takes migrations 0058 and 0057 back off a throwaway world (0056 kept): as a world is until it is migrated."""
    unmigrate_rules(key)
    sql = (W.MIGRATIONS / '0056_country.sql').read_text(encoding='utf-8')
    manifest = sql[sql.index('CREATE OR REPLACE FUNCTION live.people_manifest'):]
    with W.connect(key, 'owner') as conn:
        with conn.transaction():
            conn.execute('ALTER TABLE live.economy DROP COLUMN comforts_above, DROP COLUMN comforts_share, DROP COLUMN seasons')
            conn.execute(manifest)
            conn.execute("DELETE FROM public.schema_migrations WHERE version = '0057'")


class StandInEconomy:
    """Atlas's Economy controls (studio.Economy): every call recorded and answered."""

    def __init__(self):
        self.calls = []

    def read(self, key):
        self.calls.append(('read', key))
        return {'world': 'w', 'supported': True, 'internalStore': True, 'businesses': []}

    def set_store(self, key, data, editor):
        self.calls.append(('set_store', key, data, editor))
        return {'changed': True, 'detail': 'The store room: storeMarkup 50.'}

    def set_businesses(self, key, data, editor):
        self.calls.append(('set_businesses', key, data, editor))
        return {'changed': True, 'detail': 'The store room for 1 business: 0 on, 1 off.'}

    def set_loop(self, key, data, editor):
        self.calls.append(('set_loop', key, data, editor))
        return {'changed': True, 'detail': 'The city\'s loop: gateToll 30.'}

    def set_goods(self, key, data, editor):
        self.calls.append(('set_goods', key, data, editor))
        return {'changed': True, 'detail': 'The Country supplies 0 of 1 good set.'}

    def set_seasons(self, key, data, editor):
        self.calls.append(('set_seasons', key, data, editor))
        return {'changed': True, 'detail': 'The seasons: grain supply 50/100/150/40.'}

    def set_comforts(self, key, data, editor):
        self.calls.append(('set_comforts', key, data, editor))
        return {'changed': True, 'detail': 'The comforts: comfortsAbove 40.'}

    def set_rules(self, key, data, editor):
        self.calls.append(('set_rules', key, data, editor))
        return {'changed': True, 'detail': 'The rules: almsADay 12.'}

    def set_services(self, key, data, editor):
        self.calls.append(('set_services', key, data, editor))
        return {'changed': True, 'detail': 'The services: groom_and_trim on false.'}

    def set_farms(self, key, data, editor):
        self.calls.append(('set_farms', key, data, editor))
        return {'changed': True, 'detail': 'The farms: grain yieldScale 120.'}

    def queue_sync(self, key, who):
        self.calls.append(('queue_sync', key, who))
        return 7


class EconomyRouteTests(unittest.TestCase):
    """Doc 67, 6: the Economy controls' routes with a stand-in: reads for anyone signed in, writes for dm and admin with
    Atlas's session token, each audited in Atlas's world."""

    def setUp(self):
        self.economy = StandInEconomy()
        self.host = Running(StandInDM(), 'dev-old', economy=self.economy)
        self.dm, self.call = self.host.dm, self.host.call

    def tearDown(self):
        self.host.stop()

    def test_who_may_read_and_change_them(self):
        self.assertEqual(self.call('GET', '/api/atlas/economy')[0], 401)
        status, data = self.call('GET', '/api/atlas/economy', token='viewer-token')
        self.assertEqual(status, 200, data)
        self.assertEqual((data['world'], data['worldInfo']['key']), ('w', 'dev-old'))
        body = {'storeMarkup': 50}
        self.assertEqual(self.call('POST', '/api/atlas/economy/store', body, token='viewer-token')[0], 403, 'a viewer reads only')
        self.assertEqual(self.call('POST', '/api/atlas/economy/store', body, token='dm-token')[0], 403, 'Atlas\'s token too')
        self.assertFalse([c for c in self.economy.calls if c[0] != 'read'], 'nothing reached the controls')
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/economy/store', body, token='dm-token', headers=editor)
        self.assertEqual(status, 200, data)
        self.assertIn(('set_store', 'dev-old', body, 'dm-user'), self.economy.calls)
        status, data = self.call('POST', '/api/atlas/economy/businesses', {'set': {'rm_smith': False}}, token='admin-token', headers=editor)
        self.assertEqual(status, 200, data)
        bulk = {'all': False, 'business': 'smithy', 'district': 'works'}
        self.assertEqual(self.call('POST', '/api/atlas/economy/businesses', bulk, token='dm-token', headers=editor)[0], 200)
        self.assertIn(('set_businesses', 'dev-old', bulk, 'dm-user'), self.economy.calls, 'the bulk filters reach the controls')
        logged = [args for name, args in self.dm.calls if name == 'log']
        self.assertEqual([(who['username'], world, action, target) for who, world, action, target, _ in logged],
                         [('dm-user', 'dev-old', 'economy.store', 'store room'), ('admin-user', 'dev-old', 'economy.businesses', 'rm_smith'),
                          ('dm-user', 'dev-old', 'economy.businesses', 'businesses')])
        # Doc 68, 9: the loop's levers in Atlas, and the DM's view of a live world's (dm and admin change it, no Atlas token).
        status, data = self.call('POST', '/api/atlas/economy/loop', {'gateToll': 30}, token='dm-token', headers=editor)
        self.assertEqual(status, 200, data)
        self.assertIn(('set_loop', 'dev-old', {'gateToll': 30}, 'dm-user'), self.economy.calls)
        self.assertEqual(self.call('POST', '/api/atlas/economy/goods', {'set': {'fish': False}}, token='dm-token', headers=editor)[0], 200)
        self.assertEqual(self.call('POST', '/api/atlas/economy/loop', {'gateToll': 30}, token='dm-token')[0], 403, 'Atlas\'s token')
        live = W.live_world()
        status, data = self.call('GET', '/api/dm/economy', token='viewer-token')
        self.assertEqual(status, 200, data)
        self.assertEqual(self.economy.calls[-1], ('read', live))
        self.assertEqual(self.call('POST', '/api/dm/economy/loop', {'subsidies': {'carriage': 10}}, token='viewer-token')[0], 403)
        status, data = self.call('POST', '/api/dm/economy/loop', {'subsidies': {'carriage': 10}}, token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertIn(('set_loop', live, {'subsidies': {'carriage': 10}}, 'dm-user'), self.economy.calls)
        self.assertEqual(self.call('POST', '/api/dm/economy/loop', {'world': 'nowhere', 'gateToll': 1}, token='dm-token')[0], 400,
                         'a registered world (the DM works on any, its own by default)')

    def test_the_dms_rules_reach_the_running_server(self):
        # Doc 68, Progress, "Seasons and the DM's rules": every setting from the DM, on the DM's world, audited, each
        # change queued as economy.sync for its running server; Atlas's changes too.
        live = W.live_world()
        bodies = {'store': {'resaleMarkup': 60}, 'loop': {'gateToll': 25}, 'goods': {'set': {'fish': False}},
                  'seasons': {'set': {'grain': {'supply': [50, 100, 150, 40]}}}, 'comforts': {'comfortsAbove': 40},
                  'rules': {'set': {'almsADay': 12}}, 'services': {'set': {'groom_and_trim': {'on': False}, 'stabling': None}},
                  'farms': {'set': {'areas': {'grain': {'yieldScale': 120}}}}}
        for route, body in bodies.items():
            self.assertEqual(self.call('POST', f'/api/dm/economy/{route}', body, token='viewer-token')[0], 403, route)
            status, data = self.call('POST', f'/api/dm/economy/{route}', dict(body), token='dm-token')
            self.assertEqual(status, 200, (route, data))
            self.assertEqual(data['sync'], 7, 'queued for the running server')
        named = {'store': 'set_store', 'loop': 'set_loop', 'goods': 'set_goods', 'seasons': 'set_seasons', 'comforts': 'set_comforts',
                 'rules': 'set_rules', 'services': 'set_services', 'farms': 'set_farms'}
        for route, body in bodies.items():
            self.assertIn((named[route], live, body, 'dm-user'), self.economy.calls, route)
        self.assertEqual([c for c in self.economy.calls if c[0] == 'queue_sync'], [('queue_sync', live, 'dm-user')] * 8)
        logged = [(world, action, target) for name, (who, world, action, target, _) in self.dm.calls if name == 'log']
        self.assertEqual(logged, [(live, 'economy.store', 'store room'), (live, 'economy.loop', 'city loop'),
                                  (live, 'economy.goods', 'the Country\'s goods'), (live, 'economy.seasons', 'grain'),
                                  (live, 'economy.comforts', 'comforts'), (live, 'economy.rules', 'almsADay'),
                                  (live, 'economy.services', 'services'), (live, 'economy.farms', 'grain')])
        self.assertEqual(self.call('POST', '/api/dm/economy/businesses', {'set': {'x': False}}, token='dm-token')[0], 404,
                         'a business\'s own switch stays Atlas\'s')
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/economy/seasons', {'set': {'grain': None}}, token='dm-token', headers=editor)
        self.assertEqual((status, data.get('sync')), (200, 7), data)
        for route, body in (('rules', {'set': {'treasuryCap': None}}), ('services', {'set': {'cart_hire': {'every': 30}}}),
                            ('farms', {'set': {'start': 0}})):
            status, data = self.call('POST', f'/api/atlas/economy/{route}', body, token='dm-token', headers=editor)
            self.assertEqual((status, data.get('sync')), (200, 7), (route, data))
            self.assertIn((f'set_{route}', 'dev-old', body, 'dm-user'), self.economy.calls)
        self.assertIn(('queue_sync', 'dev-old', 'dm-user'), self.economy.calls, 'Atlas\'s edits reach a server running its world')

    def test_the_steerings_rules_are_gone(self):
        # Doc 79, 12 and 17: the steering went with the conserving economy: its on/off and its lean are no rules a world
        # sets, and the old lean is not mapped across (old worlds open at prosperity 0). (The slider's route,
        # /api/economy/steer, comes back in Phase 9 carrying `prosperity`, with its test retargeted: doc 79, 16.10.)
        for rule in ('steer', 'steerLean'):
            self.assertNotIn(rule, map_editor.RULES)
            self.assertEqual(map_editor.check_rules({rule: 150}, {}), [], 'a world holding it is kept, warned of')
        self.assertFalse([c for c in self.economy.calls if c[0] == 'set_steer'])

    def test_route_sets_are_apart(self):
        self.assertFalse(ST.ECONOMY_ROUTES & ST.SYNC_ROUTES)
        self.assertFalse({p for _, p in ST.ECONOMY_ROUTES} & (ST.MOVED_TO_ATLAS | ST.LAYER_READS | ST.NOT_IN_ATLAS))


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class EconomyFlowTests(ScratchWorlds, unittest.TestCase):
    """The Economy controls for real on a throwaway dev world (test_world_sync's copy of a live Greyfen): the world's
    switches, one business and many, what reaches the NPC records, the edit log and the audit; a world without 0055."""

    def setUp(self):
        super().setUp()
        W.set_local(atlas=self.dev, live=self.live)
        self.dm = StandInDM()
        self.dm.log = D.DungeonMaster(lambda key: W.connect(key, 'dm')).log
        self.host = Running(self.dm, self.dev)
        self.call = self.host.call

    def tearDown(self):
        self.host.stop()
        super().tearDown()

    def test_the_store_room_switched_for_the_world_and_by_business(self):
        status, data = self.call('GET', '/api/atlas/economy', token='viewer-token')
        self.assertEqual(status, 200, data)
        self.assertTrue(data['supported'])
        self.assertEqual((data['internalStore'], data['storeMarkup'], data['storeResale'], data['resaleMarkup'], data['storeRule']),
                         (True, 100, True, 75, False))
        shops = {b['npc']: b for b in data['businesses']}
        self.assertEqual(set(shops), {'wren', 'brann'}, 'every placed merchant')
        brann = shops['brann']
        self.assertEqual((brann['business'], brann['effective'], brann['internalStore']), ('smithy', 'on', True))
        self.assertEqual([g['item'] for g in brann['making']], ['bronze_bar', 'charcoal', 'iron_bar', 'leather', 'timber'])
        self.assertEqual(next(g for g in brann['making'] if g['item'] == 'iron_bar')['cost'], next(g for g in brann['making'] if g['item'] == 'iron_bar')['price'])
        self.assertEqual([g['item'] for g in shops['wren']['resale']], ['meal', 'herbs'])
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/economy/businesses', {'set': {'brann': False}}, token='dm-token', headers=editor)
        self.assertEqual(status, 200, data)
        self.assertEqual(next(b for b in data['businesses'] if b['npc'] == 'brann')['effective'], 'off (this business)')
        self.assertEqual(self.sql(self.dev, 'SELECT npc_id, internal_store FROM live.business_settings'), [('brann', False)])
        self.assertIn('nostore "brann"', self.one(self.dev, "SELECT live.people_manifest('greyfen')").split('\n'))
        status, data = self.call('POST', '/api/atlas/economy/store', {'internalStore': False, 'resaleMarkup': 40}, token='dm-token',
                                 headers=editor)
        self.assertEqual(status, 200, data)
        self.assertEqual(next(b for b in data['businesses'] if b['npc'] == 'brann')['effective'], 'off (whole world)')
        self.assertEqual(next(b for b in data['businesses'] if b['npc'] == 'wren')['resale'][0]['cost'], 3, '40% of a 6p meal, rounded up')
        self.assertIn('store off 100 resale on 40 rule off', self.one(self.dev, "SELECT live.people_manifest('greyfen')").split('\n'))
        self.assertEqual(self.one(self.dev, 'SELECT treasury FROM live.economy'), 1000, 'the rest of the economy kept')
        # Every listed business at once, filtered or not; back at its default, no row is kept.
        status, data = self.call('POST', '/api/atlas/economy/businesses', {'all': True}, token='dm-token', headers=editor)
        self.assertEqual((status, data['count']), (200, 2), data)
        self.assertEqual(self.one(self.dev, 'SELECT count(*) FROM live.business_settings'), 0)
        status, data = self.call('POST', '/api/atlas/economy/businesses', {'all': False, 'business': 'smithy'}, token='dm-token', headers=editor)
        self.assertEqual((status, data['count']), (200, 1), data)
        self.assertEqual(self.sql(self.dev, 'SELECT npc_id FROM live.business_settings'), [('brann',)], 'only the smithy')
        status, data = self.call('POST', '/api/atlas/economy/businesses', {'all': False, 'district': 'nowhere'}, token='dm-token', headers=editor)
        self.assertEqual((status, data['count'], data['changed']), (200, 0, False), data)
        status, data = self.call('POST', '/api/atlas/economy/businesses', {'all': True}, token='dm-token', headers=editor)
        self.assertEqual(self.one(self.dev, 'SELECT count(*) FROM live.business_settings'), 0)
        self.assertEqual(self.call('POST', '/api/atlas/economy/store', dict(ST.S.STORE_DEFAULTS), token='dm-token',
                                   headers=editor)[0], 200)
        self.assertFalse(any(l.startswith(('store ', 'nostore ')) for l in self.one(self.dev, "SELECT live.people_manifest('greyfen')").split('\n')))
        # Refusals: an unknown business, a markup out of range.
        self.assertEqual(self.call('POST', '/api/atlas/economy/businesses', {'set': {'nobody': False}}, token='dm-token', headers=editor)[0], 400)
        self.assertEqual(self.call('POST', '/api/atlas/economy/store', {'storeMarkup': 5000}, token='dm-token', headers=editor)[0], 400)
        self.assertEqual(self.call('POST', '/api/atlas/economy/store', {'treasury': 5}, token='dm-token', headers=editor)[0], 400)
        # The edit log keeps open Workshop sessions current; the audit says who.
        keys = [r[0] for r in self.sql(self.dev, 'SELECT key FROM world.edits ORDER BY seq')]
        self.assertEqual(keys.count('economy'), 6)
        actions = [r[0] for r in self.sql(self.dev, 'SELECT action FROM dm.audit ORDER BY at')]
        self.assertEqual(actions.count('economy.businesses'), 4)
        self.assertEqual(actions.count('economy.store'), 2)

    def test_the_city_loop_and_the_countrys_goods(self):
        # Doc 68, 9: the loop's settings and the per-good supply, what reaches the NPC records; the DM's levers on the
        # live world.
        status, data = self.call('GET', '/api/atlas/economy', token='viewer-token')
        self.assertEqual(status, 200, data)
        self.assertTrue(data['loopSupported'])
        self.assertEqual(data['loop'], {k: v for k, v in ST.map_editor.LOOP_DEFAULTS.items() if k != 'wharfOff'})
        # The Country's basket went with Data/Items/city_loop.json (doc 79, 17): the view shows none, and Phase 9
        # removes the view.
        self.assertEqual(data['basket'], [])
        self.assertIn('restocking rule', data['ruleWarning'], 'the rule is off in this world')
        goods = {g['item']: g for g in data['goods']}
        self.assertTrue(goods['iron_bar']['supplied'])
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/economy/loop', {'gateToll': 20, 'wageHand': 9, 'subsidies': {'carriage': 50}},
                                 token='dm-token', headers=editor)
        self.assertEqual(status, 200, data)
        self.assertEqual((data['loop']['gateToll'], data['loop']['wageHand'], data['loop']['subsidies']), (20, 9, {'carriage': 50}))
        status, data = self.call('POST', '/api/atlas/economy/goods', {'set': {'iron_bar': False}}, token='dm-token', headers=editor)
        self.assertEqual(status, 200, data)
        self.assertEqual(data['wharfOff'], ['iron_bar'])
        lines = self.one(self.dev, "SELECT live.people_manifest('greyfen')").split('\n')
        self.assertIn('loop needs on country on reserve 200 toll 20 carriage 6 wages 14 9 8 8', lines)
        self.assertIn('countryoff "iron_bar"', lines)
        self.assertIn('subsidy "carriage" 50', lines)
        for bad in ({'gateToll': 95}, {'subsidies': {'roads': 5}}, {'treasury': 1}):
            self.assertEqual(self.call('POST', '/api/atlas/economy/loop', bad, token='dm-token', headers=editor)[0], 400, bad)
        self.assertEqual(self.call('POST', '/api/atlas/economy/goods', {'set': {'nothing_at_all': False}}, token='dm-token',
                                   headers=editor)[0], 400)
        status, data = self.call('POST', '/api/atlas/economy/goods', {'all': True}, token='dm-token', headers=editor)
        self.assertEqual(data['wharfOff'], [])
        # The DM, on the live world: the treasury's levers.
        status, data = self.call('POST', '/api/dm/economy/loop', {'world': self.live, 'subsidies': {'carriage': 10}}, token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertIn('subsidy "carriage" 10', self.one(self.live, "SELECT live.people_manifest('greyfen')").split('\n'))
        self.assertIn('economy.loop', [r[0] for r in self.sql(self.live, 'SELECT action FROM dm.audit')])

    def test_the_seasons_and_the_comforts(self):
        # Doc 68, Progress: city_loop.json's groups with the world's changes; what reaches the NPC records; economy.sync
        # queued for a running server; back to city_loop.json's; refusals.
        status, data = self.call('GET', '/api/atlas/economy', token='viewer-token')
        self.assertEqual(status, 200, data)
        self.assertTrue(data['seasonsSupported'])
        groups = {g['id']: g for g in data['seasons']}
        self.assertTrue({'grain', 'fruit', 'fish', 'meat', 'preserved'} <= set(groups), groups)
        self.assertFalse(groups['grain']['changed'])
        self.assertEqual(groups['grain']['supply'], groups['grain']['file']['supply'])
        self.assertEqual(data['comforts']['comfortsAbove'], None)
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/economy/seasons', {'set': {'grain': {'supply': [50, 100, 150, 40], 'normal': 90}}},
                                 token='dm-token', headers=editor)
        self.assertEqual(status, 200, data)
        grain = next(g for g in data['seasons'] if g['id'] == 'grain')
        self.assertEqual((grain['changed'], grain['supply'], grain['normal'], grain['price']),
                         (True, [50, 100, 150, 40], 90, grain['file']['price']), 'the rest of the group as the file has it')
        status, data = self.call('POST', '/api/atlas/economy/comforts', {'comfortsAbove': 40}, token='dm-token', headers=editor)
        self.assertEqual((status, data['comforts']['comfortsAbove'], data['comforts']['comfortsShare']), (200, 40, None), data)
        lines = self.one(self.dev, "SELECT live.people_manifest('greyfen')").split('\n')
        price = ' '.join(map(str, grain['file']['price']))
        self.assertIn(f'season "grain" supply 50 100 150 40 price {price} shelf 100 100 100 100 normal 90', lines)
        self.assertIn('comforts 40 -', lines)
        queued = self.sql(self.dev, "SELECT kind, target_id, requested_by FROM dm.actions WHERE kind = 'economy.sync'")
        self.assertEqual(queued, [('economy.sync', 'economy', 'dm-user')] * 2, 'each change sent to the running server')
        # The DM, on the same dev world (doc 66: the DM works on any world).
        status, data = self.call('POST', '/api/dm/economy/seasons', {'world': self.dev, 'set': {'fish': {'price': [100, 120, 100, 70]}}},
                                 token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertIn('economy.seasons', [r[0] for r in self.sql(self.dev, 'SELECT action FROM dm.audit')])
        # Back to city_loop.json's: no record, no row of it kept.
        status, data = self.call('POST', '/api/atlas/economy/seasons', {'set': {'grain': None, 'fish': None}}, token='dm-token', headers=editor)
        self.assertEqual(status, 200, data)
        self.assertEqual(self.one(self.dev, 'SELECT seasons FROM live.economy'), {})
        self.call('POST', '/api/atlas/economy/comforts', {'comfortsAbove': None}, token='dm-token', headers=editor)
        lines = self.one(self.dev, "SELECT live.people_manifest('greyfen')").split('\n')
        self.assertFalse([l for l in lines if l.startswith(('season ', 'comforts '))], lines)
        for bad in ({'set': {'grain': {'supply': [50, 100, 150]}}}, {'set': {'grain': {'price': [5, 100, 100, 100]}}},
                    {'set': {'nowhere': {'normal': 5}}}, {'set': {'grain': {'colour': 1}}}, {'grain': {}}):
            self.assertEqual(self.call('POST', '/api/atlas/economy/seasons', bad, token='dm-token', headers=editor)[0], 400, bad)
        self.assertEqual(self.call('POST', '/api/atlas/economy/comforts', {'comfortsShare': 101}, token='dm-token', headers=editor)[0], 400)

    def test_the_rules_and_the_services(self):
        # Doc 68, Progress, "First fixes" (migration 0058): city_loop.json's rules and services beside the world's changes;
        # what reaches the NPC records; economy.sync queued; back to city_loop.json's; refusals.
        status, data = self.call('GET', '/api/atlas/economy', token='viewer-token')
        self.assertEqual(status, 200, data)
        self.assertTrue(data['rulesSupported'])
        self.assertEqual((data['rules'], data['services']), ({}, {}))
        self.assertEqual(set(data['ruleDefaults']), set(map_editor.RULE_LIMITS))
        listed = {s['id']: s for s in data['serviceList']}
        self.assertEqual((listed['groom_and_trim']['business'], listed['groom_and_trim']['name']), ('barber', 'A groom and trim'))
        trim = listed['groom_and_trim']
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/economy/rules', {'set': {'playerSellMost': 300, 'neverBroke': 1}}, token='dm-token', headers=editor)
        self.assertEqual((status, data['rules']), (200, {'playerSellMost': 300, 'neverBroke': 1}), data)
        status, data = self.call('POST', '/api/atlas/economy/services', {'set': {'groom_and_trim': {'on': False}}},
                                 token='dm-token', headers=editor)
        self.assertEqual(status, 200, data)
        self.assertEqual(data['services'], {'groom_and_trim': {'on': False, 'price': trim['price'], 'every': trim['every']}},
                         'stored whole: the rest as city_loop.json has it')
        lines = self.one(self.dev, "SELECT live.people_manifest('greyfen')").split('\n')
        self.assertIn('rule "playerSellMost" 300', lines)
        self.assertIn('rule "neverBroke" 1', lines)
        self.assertIn(f'service "groom_and_trim" off price {trim["price"]} every {trim["every"]}', lines)
        queued = self.sql(self.dev, "SELECT kind FROM dm.actions WHERE kind = 'economy.sync'")
        self.assertEqual(len(queued), 2, 'each change sent to the running server')
        # The DM, on the same dev world.
        status, data = self.call('POST', '/api/dm/economy/rules', {'world': self.dev, 'set': {'ripple': 40}}, token='dm-token')
        self.assertEqual((status, data['rules'].get('ripple')), (200, 40), data)
        actions = [r[0] for r in self.sql(self.dev, 'SELECT action FROM dm.audit')]
        self.assertTrue({'economy.rules', 'economy.services'} <= set(actions), actions)
        # Back to city_loop.json's: no record, nothing kept.
        self.call('POST', '/api/atlas/economy/rules', {'set': {'playerSellMost': None, 'neverBroke': None, 'ripple': None}},
                  token='dm-token', headers=editor)
        status, data = self.call('POST', '/api/atlas/economy/services', {'set': {'groom_and_trim': {'on': True}}},
                                 token='dm-token', headers=editor)
        self.assertEqual((status, data['services']), (200, {}), 'back at city_loop.json\'s: no entry kept')
        self.assertEqual((self.one(self.dev, 'SELECT rules FROM live.economy'), self.one(self.dev, 'SELECT services FROM live.economy')), ({}, {}))
        lines = self.one(self.dev, "SELECT live.people_manifest('greyfen')").split('\n')
        self.assertFalse([l for l in lines if l.startswith(('rule ', 'service '))], lines)
        for route, bad in (('rules', {'set': {'luck': 1}}), ('rules', {'set': {'playerSellMost': 5001}}), ('rules', {'set': {'neverBroke': 2}}),
                           ('rules', {'set': {'depthScale': 9}}), ('rules', {'set': {'almsADay': 12}}), ('rules', {'playerSellMost': 1}), ('services', {'set': {'haircut': {'on': False}}}),
                           ('services', {'set': {'groom_and_trim': {'price': 0}}}), ('services', {'set': {'groom_and_trim': {'colour': 1}}}),
                           ('services', {'set': {'groom_and_trim': {'on': 'no'}}})):
            self.assertEqual(self.call('POST', f'/api/atlas/economy/{route}', bad, token='dm-token', headers=editor)[0], 400, (route, bad))

    def test_the_farms(self):
        # Doc 71, 4 (migration 0060): country_farms.json's farms beside the world's changes; what reaches the NPC records;
        # economy.sync queued; the DM's too; back to country_farms.json's; refusals; the farms from the world's save.
        status, data = self.call('GET', '/api/atlas/economy', token='viewer-token')
        self.assertEqual(status, 200, data)
        self.assertTrue(data['farmsSupported'])
        self.assertEqual(data['farms'], {})
        d = data['farmDefaults']
        self.assertEqual((d['start'], d['every'], d['keeper'], d['hand'], len(d['areas'])),
                         (-1, 28, 16, 10, len(map_editor.farms_file()['areas'])))
        grain = next(a for a in d['areas'] if a['id'] == 'grain')
        self.assertEqual(([f['id'] for f in grain['farms']], grain['price'], grain['priceFactor']),
                         (['grain_farm_1', 'grain_farm_2'], 'year', 101))
        self.assertEqual(len(grain['farms'][0]['yield']['flour']), 4, 'a good\'s four middle-day values')
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        body = {'set': {'start': 0, 'every': 30, 'areas': {'grain': {'on': True, 'yieldScale': 120}},
                        'farms': {'grain_farm_1': {'hand': 11}}}}
        status, data = self.call('POST', '/api/atlas/economy/farms', body, token='dm-token', headers=editor)
        self.assertEqual(status, 200, data)
        self.assertEqual(data['farms'], {'start': 0, 'every': 30, 'areas': {'grain': {'on': True, 'yieldScale': 120}},
                                         'farms': {'grain_farm_1': {'hand': 11}}})
        status, data = self.call('POST', '/api/atlas/economy/farms', {'set': {'areas': {'grain': {'crewScale': 150}}}},
                                 token='dm-token', headers=editor)
        self.assertEqual(data['farms']['areas']['grain'], {'on': True, 'yieldScale': 120, 'crewScale': 150}, 'the rest as it was')
        lines = self.one(self.dev, "SELECT live.people_manifest('greyfen')").split('\n')
        self.assertIn('farms start 0 every 30 order - wages - -', lines)
        self.assertIn('farmarea "grain" on on yield 120 crew 150 wages - - factor -', lines)
        self.assertIn('farm "grain_farm_1" yield - crew - wages - 11', lines)
        queued = self.sql(self.dev, "SELECT kind FROM dm.actions WHERE kind = 'economy.sync'")
        self.assertEqual(len(queued), 2, 'each change sent to the running server')
        status, data = self.call('POST', '/api/dm/economy/farms', {'world': self.dev, 'set': {'keeper': 17}}, token='dm-token')
        self.assertEqual((status, data['farms'].get('keeper')), (200, 17), data)
        self.assertIn('economy.farms', [r[0] for r in self.sql(self.dev, 'SELECT action FROM dm.audit')])
        # Back to country_farms.json's: no record, nothing kept.
        status, data = self.call('POST', '/api/atlas/economy/farms', {'set': {'start': None, 'every': None, 'keeper': None,
                                 'areas': {'grain': None}, 'farms': {'grain_farm_1': {'hand': None}}}}, token='dm-token', headers=editor)
        self.assertEqual((status, data['farms']), (200, {}), data)
        self.assertEqual(self.one(self.dev, 'SELECT farms FROM live.economy'), {})
        lines = self.one(self.dev, "SELECT live.people_manifest('greyfen')").split('\n')
        self.assertFalse([l for l in lines if l.startswith(('farms ', 'farmarea ', 'farm '))], lines)
        for bad in ({'set': {'start': -2}}, {'set': {'every': 0}}, {'set': {'order': ['grain', 'grain']}},
                    {'set': {'areas': {'gardens': {'on': True}}}}, {'set': {'areas': {'grain': {'yieldScale': 1001}}}},
                    {'set': {'areas': {'grain': {'on': 'yes'}}}}, {'set': {'farms': {'grain_farm_9': {'hand': 1}}}},
                    {'set': {'luck': 1}}, {'set': {'areas': []}}, {'start': 0}):
            self.assertEqual(self.call('POST', '/api/atlas/economy/farms', bad, token='dm-token', headers=editor)[0], 400, bad)

    def test_a_farm_country_farms_json_no_longer_has(self):
        # Doc 71, 4: a world's setting for a farm the file no longer has (regenerated with fewer farms: their ids are
        # numbered by count) is kept, as Core keeps it, and blocks nothing: the other economy routes and the farms' own
        # still change, on the dev world (the edit log) and the live one (the row); a change may not name it, but may
        # clear it.
        stale = {'farms': {'orchard_9': {'hand': 11}}}
        for key in (self.dev, self.live):
            self.sql(key, 'UPDATE live.economy SET farms = %s', (json.dumps(stale),))
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/economy/rules', {'set': {'playerSellMost': 300}}, token='dm-token', headers=editor)
        self.assertEqual((status, data.get('rules')), (200, {'playerSellMost': 300}), data)
        status, data = self.call('POST', '/api/atlas/economy/farms', {'set': {'areas': {'grain': {'yieldScale': 120}}}},
                                 token='dm-token', headers=editor)
        self.assertEqual((status, data.get('farms')), (200, {**stale, 'areas': {'grain': {'yieldScale': 120}}}), data)
        status, data = self.call('POST', '/api/dm/economy/rules', {'world': self.live, 'set': {'playerSellMost': 310}}, token='dm-token')
        self.assertEqual(status, 200, data)
        status, data = self.call('POST', '/api/dm/economy/farms', {'world': self.live, 'set': {'keeper': 17}}, token='dm-token')
        self.assertEqual((status, data.get('farms')), (200, {**stale, 'keeper': 17}), data)
        self.assertIn('farm "orchard_9" yield - crew - wages - 11', self.one(self.live, "SELECT live.people_manifest('greyfen')").split('\n'))
        for bad in ({'farms': {'orchard_9': {'hand': 12}}}, {'areas': {'gardens': {'on': True}}}, {'order': ['grain', 'gardens']}):
            status, data = self.call('POST', '/api/dm/economy/farms', {'world': self.live, 'set': bad}, token='dm-token')
            self.assertEqual(status, 400, (bad, data))
            self.assertIn('names nothing country_farms.json has', data['error'])
        status, data = self.call('POST', '/api/dm/economy/farms', {'world': self.live, 'set': {'farms': {'orchard_9': None}}},
                                 token='dm-token')
        self.assertEqual((status, data.get('farms')), (200, {'keeper': 17}), data)

    def test_a_world_without_migration_0060(self):
        unmigrate_farms(self.dev)
        status, data = self.call('GET', '/api/atlas/economy', token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertTrue(data['rulesSupported'])
        self.assertFalse(data['farmsSupported'])
        self.assertIn('lacks migration 0060', data['farmsProblem'])
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        body = {'set': {'start': 0}}
        status, data = self.call('POST', '/api/atlas/economy/farms', body, token='dm-token', headers=editor)
        self.assertEqual(status, 409, data)
        self.assertIn('0060', data['error'])
        status, data = self.call('POST', '/api/dm/economy/farms', {'world': self.dev, **body}, token='dm-token')
        self.assertEqual(status, 409, data)
        self.assertIn('0060', data['error'])
        status, data = self.call('POST', '/api/atlas/economy/rules', {'set': {'playerSellMost': 300}}, token='dm-token', headers=editor)
        self.assertEqual((status, data['rules']), (200, {'playerSellMost': 300}), 'the rules still change on a world with 0058')

    def test_no_view_of_the_removed_economy(self):
        # Doc 79, 17: the save's loop (the self-correcting rules' drift), its steering and the farms' tills are gone, and
        # the economy page shows none of them.
        status, data = self.call('GET', f'/api/dm/economy?world={self.dev}', token='viewer-token')
        self.assertEqual(status, 200, data)
        for gone in ('drift', 'steering', 'farmsSaved'):
            self.assertNotIn(gone, data)

    def test_a_world_without_migration_0058(self):
        unmigrate_rules(self.dev)
        status, data = self.call('GET', '/api/atlas/economy', token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertTrue(data['seasonsSupported'])
        self.assertFalse(data['rulesSupported'])
        self.assertIn('lacks migration 0058', data['rulesProblem'])
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        for route, body in (('rules', {'set': {'playerSellMost': 300}}), ('services', {'set': {'groom_and_trim': {'on': False}}})):
            status, data = self.call('POST', f'/api/atlas/economy/{route}', body, token='dm-token', headers=editor)
            self.assertEqual(status, 409, data)
            self.assertIn('0058', data['error'])
            status, data = self.call('POST', f'/api/dm/economy/{route}', {'world': self.dev, **body}, token='dm-token')
            self.assertEqual(status, 409, data)
            self.assertIn('0058', data['error'])
        status, data = self.call('POST', '/api/atlas/economy/comforts', {'comfortsAbove': 30}, token='dm-token', headers=editor)
        self.assertEqual((status, data['comforts']['comfortsAbove']), (200, 30), 'the comforts still change on a world with 0057')

    def test_a_world_without_migration_0057(self):
        unmigrate_seasons(self.dev)
        status, data = self.call('GET', '/api/atlas/economy', token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertTrue(data['loopSupported'])
        self.assertFalse(data['seasonsSupported'])
        self.assertIn('lacks migration 0057', data['seasonsProblem'])
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        for route, body in (('seasons', {'set': {'grain': {'normal': 5}}}), ('comforts', {'comfortsAbove': 30})):
            status, data = self.call('POST', f'/api/atlas/economy/{route}', body, token='dm-token', headers=editor)
            self.assertEqual(status, 409, data)
            self.assertIn('0057', data['error'])
            status, data = self.call('POST', f'/api/dm/economy/{route}', {'world': self.dev, **body}, token='dm-token')
            self.assertEqual(status, 409, data)
            self.assertIn('0057', data['error'])
        status, data = self.call('POST', '/api/atlas/economy/loop', {'gateToll': 22}, token='dm-token', headers=editor)
        self.assertEqual(status, 200, data)
        self.assertEqual(data['loop']['gateToll'], 22, 'the loop\'s settings still change on a world with 0056')

    def test_a_world_without_migration_0055(self):
        from test_world_sync import unmigrate
        unmigrate(self.dev)
        status, data = self.call('GET', '/api/atlas/economy', token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertFalse(data['supported'])
        self.assertIn('lacks migration 0055', data['problem'])
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/economy/store', {'storeMarkup': 50}, token='dm-token', headers=editor)
        self.assertEqual(status, 409, data)
        self.assertIn('0055', data['error'])


class RuleListTests(unittest.TestCase):
    """The rules the studio offers are Core's (Rules[] in Core/RatwItems.h), with their ranges."""

    def test_every_rule_listed_with_core(self):
        core = (Path(__file__).resolve().parent.parent / 'Core/RatwItems.h').read_text(encoding='utf-8')
        start = core.index('inline constexpr RuleInfo Rules[]')
        names = re.findall(r'\{"([A-Za-z]+)", (-?\d+), (\d+), (-?\d+)\}', core[start:core.index('};', start)])
        self.assertEqual({n for n, *_ in names}, set(map_editor.RULE_LIMITS), 'Core\'s rules and the studio\'s alike')
        for name, low, high, fallback in names:
            self.assertEqual(int(high), map_editor.RULE_LIMITS[name], name)
            self.assertEqual((int(low), int(high), int(fallback)), map_editor.RULES[name], name)
        self.assertEqual(set(map_editor.rule_defaults()), set(map_editor.RULES), 'a default for every rule')

    def test_a_rule_the_game_no_longer_has_is_kept_and_ignored(self):
        # Doc 79, 14 and 17: an old world's rule (the steering's, the loop's) refuses nothing, warns, and never reaches
        # the world file (Core would skip it: doc 79, 14); a known rule out of its range is still refused.
        rules = {'steer': 1, 'almsADay': 12, 'neverBroke': 1}
        self.assertEqual(map_editor.check_rules(rules, {}), [])
        self.assertEqual(map_editor.unknown_rules(rules), ['almsADay', 'steer'])
        self.assertEqual(map_editor.rule_lines({'rules': rules}), ['rule "neverBroke" 1'])
        self.assertTrue(map_editor.check_rules({'halfLifeDurable': 4}, {}), 'under its low')
        self.assertTrue(map_editor.check_rules({'neverBroke': 2}, {}))

    def test_no_steering(self):
        # Doc 79, 17: the steering went with the conserving economy.
        for name in ('steer_at', 'steer_lever', 'steer_edicts', 'steer_ledger', 'STEER_LEVERS'):
            self.assertFalse(hasattr(ST, name), name)
        self.assertFalse(hasattr(ST.Economy, 'set_steer'))
        self.assertFalse(hasattr(map_editor, 'STEER_SETTINGS'))


def W_default_economy():
    import world_store
    return world_store.default_economy()


class WharfListsTests(unittest.TestCase):
    def test_the_wharf_lists_come_from_market_json_once_city_loop_json_is_gone(self):
        """Doc 79, 17: the Wharf's goods moved to market.json as names only; the panel still shows them."""
        market = map_editor.market_file().get('wharf') or {}
        lists = ST.Economy.wharf_lists({})
        self.assertTrue(market.get('imports') and market.get('takes'), 'market.json names the Wharf\'s goods')
        self.assertEqual(sorted(lists['imports']), sorted(market['imports']))
        self.assertEqual(sorted(lists['takes']), sorted(market['takes']))
        self.assertEqual(lists['never'], market.get('never'))
        self.assertTrue(all(v == {} for v in lists['imports'].values()), 'goods with no limits set')
        # A world's own city_loop.json lists, while it has them, are shown as they are.
        own = {'wharf': {'imports': {'salt': {'most': 3}}, 'takes': {}, 'never': ['cheese']}}
        self.assertEqual(ST.Economy.wharf_lists(own), {'imports': {'salt': {'most': 3}}, 'takes': {}, 'never': ['cheese']})


if __name__ == '__main__':
    unittest.main()
