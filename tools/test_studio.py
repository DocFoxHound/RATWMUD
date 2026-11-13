#!/usr/bin/env python3
"""The studio (tools/studio.py, doc 66 sections 3 and 4): one host, one sign-in, two suites kept apart; Atlas's Update
from live and the DM's server, maintenance, live switch, Import from dev and releases.

Offline with a stand-in DM (accounts by token, every other method recorded) and a stand-in sync engine; two classes
work for real on scratch databases (dropped afterwards; skipped without the local PostgreSQL): a sign-in, and pull,
import, rollback and the live switch on throwaway worlds (test_world_sync's). DEV and PROD are never touched."""
import ast
import copy
import http.client
import json
import os
import re
from pathlib import Path
import tempfile
import time
import threading
import unittest

import dungeon_master as D
import economy_contract as C
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


def unmigrate_slim(key):
    """Takes migration 0061 back off a throwaway world: live.business_settings back, live.npcs.band_override gone, 0060's
    people_manifest; as DEV is until the user migrates it (doc 79, Phase 9)."""
    store = (W.MIGRATIONS / '0055_store_room.sql').read_text(encoding='utf-8')
    table = store[store.index('CREATE TABLE live.business_settings'):store.index('-- As in 0053')]
    farms = (W.MIGRATIONS / '0060_farms.sql').read_text(encoding='utf-8')
    manifest = farms[farms.index('CREATE OR REPLACE FUNCTION live.people_manifest'):]
    with W.connect(key, 'owner') as conn:
        with conn.transaction():
            conn.execute(table)
            conn.execute(manifest)
            conn.execute('ALTER TABLE live.npcs DROP COLUMN band_override')
            conn.execute("DELETE FROM public.schema_migrations WHERE version = '0061'")


def unmigrate_farms(key):
    """Takes migrations 0061 and 0060 back off a throwaway world (0058 and 0059 kept): as a world is until it is migrated."""
    unmigrate_slim(key)
    sql = (W.MIGRATIONS / '0058_rules.sql').read_text(encoding='utf-8')
    manifest = sql[sql.index('CREATE OR REPLACE FUNCTION live.people_manifest'):]
    with W.connect(key, 'owner') as conn:
        with conn.transaction():
            conn.execute('ALTER TABLE live.economy DROP COLUMN farms')
            conn.execute(manifest)
            conn.execute("DELETE FROM public.schema_migrations WHERE version = '0060'")


def unmigrate_rules(key):
    """Takes migrations 0061, 0060 and 0058 back off a throwaway world (0057 kept): as a world is until it is migrated."""
    unmigrate_farms(key)
    sql = (W.MIGRATIONS / '0057_seasons.sql').read_text(encoding='utf-8')
    manifest = sql[sql.index('CREATE OR REPLACE FUNCTION live.people_manifest'):]
    with W.connect(key, 'owner') as conn:
        with conn.transaction():
            conn.execute('ALTER TABLE live.economy DROP COLUMN rules, DROP COLUMN services')
            conn.execute(manifest)
            conn.execute("DELETE FROM public.schema_migrations WHERE version = '0058'")


def unmigrate_seasons(key):
    """Takes migrations 0061 to 0057 back off a throwaway world (0056 kept): as a world is until it is migrated."""
    unmigrate_rules(key)
    sql = (W.MIGRATIONS / '0056_country.sql').read_text(encoding='utf-8')
    manifest = sql[sql.index('CREATE OR REPLACE FUNCTION live.people_manifest'):]
    with W.connect(key, 'owner') as conn:
        with conn.transaction():
            conn.execute('ALTER TABLE live.economy DROP COLUMN comforts_above, DROP COLUMN comforts_share, DROP COLUMN seasons')
            conn.execute(manifest)
            conn.execute("DELETE FROM public.schema_migrations WHERE version = '0057'")


def unmigrate_store(key):
    """Takes migration 0055 back off a throwaway world already without 0061 to 0057 (unmigrate_seasons): no store room's
    table or columns, 0053's people_manifest."""
    sql = (W.MIGRATIONS / '0053_postless_routes.sql').read_text(encoding='utf-8')
    with W.connect(key, 'owner') as conn:
        with conn.transaction():
            conn.execute('DROP TABLE live.business_settings')
            conn.execute('ALTER TABLE live.economy DROP COLUMN internal_store, DROP COLUMN store_markup, DROP COLUMN store_resale, '
                         'DROP COLUMN resale_markup, DROP COLUMN store_rule')
            conn.execute(sql)
            conn.execute("DELETE FROM public.schema_migrations WHERE version = '0055'")


def core_store_ready() -> bool:
    """Whether world_store has the slim economy (doc 79 Phase 9, the core lane's C8): until it is merged, its K0 names
    raise NotImplementedError and the database flow below can't run."""
    try:
        ST.S.slim_supported(None)
    except NotImplementedError:
        return False
    except Exception:  # noqa: BLE001 - (no connection: implemented)
        return True
    return True


VIEW = {'world': 'w', 'supported': True, 'slim': True, 'economy': {'rules': {}}}


class StandInEconomy:
    """The studio's Economy (studio.Economy): every call recorded and answered."""

    def __init__(self):
        self.calls = []

    def read(self, key):
        self.calls.append(('read', key))
        return dict(VIEW)

    def __getattr__(self, name):
        if not name.startswith('set_'):
            raise AttributeError(name)

        def setter(key, data, editor):
            self.calls.append((name, key, data, editor))
            return {'changed': True, 'detail': f'{name[4:]} changed.', 'before': {'x': 1}, 'after': {'x': 2},
                    'view': dict(VIEW), **({'now': data.get('now') is True} if name == 'set_steer' else {})}
        return setter

    def queue_sync(self, key, who, payload=None):
        self.calls.append(('queue_sync', key, who, payload))
        return 7

    def last_sync(self, key):
        # (Last repair: what the server made of the last economy.sync, shown on the page.)
        return {'id': 7, 'status': 'applied', 'result': 'The economy\'s rules are read again and in force. (2 records skipped)',
                'by': 'dm-user', 'at': '2026-11-12T10:00:00+00:00'}


class EconomyRouteTests(unittest.TestCase):
    """Doc 79, 12 (Phase 9): the Economy page's routes with a stand-in: reads for anyone signed in, writes for dm and
    admin (Atlas's with its session token), each audited as economy.<x> with its before and after and sent to the
    world's running server; the same set on the DM's world; the store room's, the loop's, the comforts' and the farms'
    routes gone."""

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
        # (Last repair: what the game server made of the last economy.sync is on the page: skipped records, a refusal,
        # "expired" when no server took it.)
        self.assertEqual((data['lastSync']['status'], '2 records skipped' in data['lastSync']['result']), ('applied', True), data['lastSync'])
        body = {'set': {'ripple': 40}}
        self.assertEqual(self.call('POST', '/api/atlas/economy/rules', body, token='viewer-token')[0], 403, 'a viewer reads only')
        self.assertEqual(self.call('POST', '/api/atlas/economy/rules', body, token='dm-token')[0], 403, 'Atlas\'s token too')
        self.assertFalse([c for c in self.economy.calls if c[0] != 'read'], 'nothing reached the controls')
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/economy/rules', body, token='dm-token', headers=editor)
        self.assertEqual(status, 200, data)
        self.assertEqual((data['changed'], data['sync'], data['warnings'], data['world']), (True, 7, [], 'w'), data)
        self.assertEqual(data['lastSync']['id'], 7, 'a change\'s reply carries the sync it queued')
        self.assertIn(('set_rules', 'dev-old', body, 'dm-user'), self.economy.calls)
        live = W.live_world()
        status, data = self.call('GET', '/api/dm/economy', token='viewer-token')
        self.assertEqual((status, self.economy.calls[-1]), (200, ('read', live)), data)
        self.assertEqual(self.call('POST', '/api/dm/economy/rules', body, token='viewer-token')[0], 403)
        self.assertEqual(self.call('POST', '/api/dm/economy/rules', dict(body), token='dm-token')[0], 200, 'no Atlas token for the DM')
        self.assertEqual(self.call('POST', '/api/dm/economy/rules', {'world': 'nowhere', **body}, token='dm-token')[0], 400,
                         'a registered world (the DM works on any, its own by default)')

    def test_every_change_is_audited_and_sent_to_the_running_server(self):
        live = W.live_world()
        bodies = {'steer': {'prosperity': 35, 'cause': 'the war levy', 'now': True}, 'needs': {'needsPaused': True},
                  'rules': {'set': {'ripple': 40}}, 'services': {'set': {'groom_and_trim': {'on': False}, 'stabling': None}},
                  'seasons': {'set': {'grain': {'price': [100, 95, 95, 100]}}}, 'imports': {'neverImported': ['salt']},
                  'status': {'status': {'leanDays': 4}}, 'market': {'market': {'events': {'perWeek': 1}}}}
        self.assertEqual({p.rsplit('/', 1)[1] for _, p in ST.ECONOMY_ROUTES if p != '/api/economy'}, set(bodies))
        for route, body in bodies.items():
            self.assertEqual(self.call('POST', f'/api/dm/economy/{route}', body, token='viewer-token')[0], 403, route)
            status, data = self.call('POST', f'/api/dm/economy/{route}', dict(body), token='dm-token')
            self.assertEqual((status, data['sync'], data['detail']), (200, 7, f'{route} changed.'), (route, data))
            self.assertIn((f'set_{route}', live, body, 'dm-user'), self.economy.calls, route)
        synced = [c for c in self.economy.calls if c[0] == 'queue_sync']
        self.assertEqual(synced[0], ('queue_sync', live, 'dm-user', {'now': True}), 'the slider\'s now reaches the server')
        self.assertEqual(synced[1:], [('queue_sync', live, 'dm-user', None)] * 7)
        logged = [(world, action, target, detail) for name, (who, world, action, target, detail) in self.dm.calls if name == 'log']
        self.assertEqual([(w, a, t) for w, a, t, _ in logged],
                         [(live, 'economy.steer', 'steer'), (live, 'economy.needs', 'needs'), (live, 'economy.rules', 'ripple'),
                          (live, 'economy.services', 'services'), (live, 'economy.seasons', 'grain'),
                          (live, 'economy.imports', 'imports'), (live, 'economy.status', 'status'), (live, 'economy.market', 'market')])
        self.assertTrue(all('Before: ' in d and '; after: ' in d for *_, d in logged), 'the before and after audited')
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/economy/steer', {'prosperity': 'abc'}, token='dm-token', headers=editor)
        self.assertEqual((status, data['sync']), (200, 7), data)
        self.assertEqual(self.economy.calls[-1], ('queue_sync', 'dev-old', 'dm-user', {'now': False}))

    def test_the_cut_routes_are_gone(self):
        # Doc 79, 12 and 17: the store room, the businesses' switches, the city loop, the Country's goods, the comforts and
        # the farms' money control nothing, so their routes went with Phase 9.
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        for route in ('store', 'businesses', 'loop', 'goods', 'comforts', 'farms'):
            self.assertEqual(self.call('POST', f'/api/atlas/economy/{route}', {'set': {}}, token='dm-token', headers=editor)[0],
                             404, route)
            self.assertEqual(self.call('POST', f'/api/dm/economy/{route}', {'set': {}}, token='dm-token')[0], 404, route)
            self.assertFalse(hasattr(ST.Economy, f'set_{route}'), route)
        self.assertFalse([c for c in self.economy.calls if c[0] != 'read'])

    def test_route_sets_are_apart(self):
        self.assertEqual(ST.ECONOMY_ROUTES, ST.DM_ECONOMY_ROUTES, 'the same set in both suites (doc 78 M3\'s allowlist)')
        self.assertFalse(ST.ECONOMY_ROUTES & ST.SYNC_ROUTES)
        self.assertFalse({p for _, p in ST.ECONOMY_ROUTES} & (ST.MOVED_TO_ATLAS | ST.LAYER_READS | ST.NOT_IN_ATLAS))


class FakeConn:
    """A world's connection for studio.Economy offline: its world id, nothing else."""

    def __enter__(self):
        return self

    def __exit__(self, *_):
        return False

    def execute(self, sql, params=()):
        if 'FOR UPDATE' in sql:                     # (A live world's row locked for the edit: Phase 9's second repair.)
            self.locked = getattr(self, 'locked', 0) + 1
            return None
        assert sql.startswith('SELECT id FROM world.worlds'), sql
        return type('Row', (), {'fetchone': lambda _: ('w',)})()

    def transaction(self):
        import contextlib
        return contextlib.nullcontext()


class QueueSyncTests(unittest.TestCase):
    """Phase 9's last repair: the studio tells the running server through migration 0061's dm.queue_economy_sync as the
    role it wrote the change with (Atlas's helper holds only the editor's password), the DM's role only on a world
    before 0061; and reads back what the server made of the last one."""

    class Conn:
        def __init__(self, has, log):
            self.has, self.log = has, log

        def __enter__(self):
            return self

        def __exit__(self, *_):
            return False

        def execute(self, sql, params=()):
            self.log.append((sql.split()[0] + ' ' + ('function' if 'queue_economy_sync(%s' in sql else 'insert' if 'INSERT' in sql
                                                    else 'notify' if 'pg_notify' in sql else 'look'), params))
            row = (self.has,) if 'to_regprocedure' in sql else (41,)
            return type('Row', (), {'fetchone': lambda _: row})()

    def economy(self, has):
        self.own, self.dm = [], []
        economy = ST.Economy(connect=lambda key: self.Conn(has, self.own), connect_dm=lambda key: self.Conn(has, self.dm))
        economy.live = lambda key: False
        return economy

    def test_through_the_function_as_its_own_role(self):
        economy = self.economy(True)
        self.assertEqual(economy.queue_sync('dev', 'Ada', {'now': True}), 41)
        self.assertEqual(self.own[-1], ('SELECT function', ('Ada', '{"now": true}')))
        self.assertEqual(self.dm, [], 'never the DM\'s role where the world has the function')

    def test_as_the_dm_before_0061(self):
        economy = self.economy(False)
        self.assertEqual(economy.queue_sync('dev', 'Ada'), 41)
        self.assertEqual([c[0] for c in self.dm], ['INSERT insert', 'SELECT notify'])


class FakeStore:
    """world_store's economy (contract 1.3) in memory, patched into the studio: economy_value, save_economy and
    patch_economy_rules on one world's row; slim or not; and the studio's live_edit call recorded."""

    COLUMNS = ['world_id', 'treasury', 'store_herbs', 'store_meals', 'needs_paused', 'seasons', 'services', 'rules']

    def __init__(self, case, slim=True, live=False, entity=None, supported=True):
        self.slim, self.live, self.writes = slim, live, []
        self.row = entity or {'treasury': 1000, 'storeHerbs': 100, 'storeMeals': 50, 'needsPaused': False, 'seasons': {},
                              'rules': {}, 'services': {}, 'overrides': {}}
        columns = self.COLUMNS if supported else self.COLUMNS[:4]
        store = self

        def patch(name, value, owner=ST.S):
            old = owner.__dict__[name] if isinstance(owner, type) else getattr(owner, name)
            setattr(owner, name, value)
            case.addCleanup(setattr, owner, name, old)
        patch('economy_value', lambda conn, w: copy.deepcopy(store.row))
        patch('tables_for', lambda conn: {'live.economy': (('world_id',), columns)})
        patch('slim_supported', lambda conn: store.slim)
        patch('patch_economy_rules', self.patch_rules)
        patch('save_economy', self.save)
        patch('apply_edit', self.edit, ST)
        patch('live', staticmethod(lambda key: store.live), ST.Economy)
        self.economy = ST.Economy(connect=lambda key: FakeConn())

    def patch_rules(self, conn, w, set_=None, drop=()):
        rules = {**self.row['rules'], **self.row['overrides']}
        rules = {k: v for k, v in rules.items() if k not in drop}
        rules.update(set_ or {})
        self.writes.append(('patch', dict(set_ or {}), list(drop)))
        self.row['overrides'] = {k: v for k, v in rules.items() if k in C.OVERRIDE_KEYS}
        self.row['rules'] = {k: v for k, v in rules.items() if k not in C.OVERRIDE_KEYS}
        return rules

    def save(self, conn, w, entity):
        if not self.slim and set(entity['overrides']) - {'prosperityCause'}:
            raise ST.S.EconomyRefused('This world needs migration 0061.')
        self.writes.append(('save', copy.deepcopy(entity)))
        self.row = copy.deepcopy(entity)
        return entity

    def edit(self, live_edit, conn, editor, label, ops):
        self.writes.append(('edit', editor, label, copy.deepcopy(ops)))
        self.row = copy.deepcopy(ops[0]['after'])
        return 1


class EconomyTests(unittest.TestCase):
    """studio.Economy against the contract (doc 79, Phase 9 plan 1.3 and 1.6), offline on FakeStore: the slider clamped
    and never refused, its cause; the rules (never the slider's), services, seasons in either shape, needsPaused; the
    host's overrides checked as Core reads them, and only on a world with migration 0061."""

    def test_the_slider_is_clamped_never_refused_with_its_cause(self):
        store = FakeStore(self)
        e = store.economy
        for given, wanted in ((9999, 100), ('abc', 0), (37, 35), (-9999, -100), (None, 0), (True, 0)):
            out = e.set_steer('dev', {'prosperity': given}, 'dm-user')
            self.assertEqual((out['after']['prosperity'], out['view']['prosperity']['value']), (wanted, wanted), given)
        out = e.set_steer('dev', {'prosperity': 35, 'cause': 'the Crown\'s war levy', 'now': True}, 'dm-user')
        self.assertEqual((out['changed'], out['now'], out['view']['prosperity']['cause']), (True, True, 'the Crown\'s war levy'))
        self.assertEqual(store.row['rules']['prosperity'], 35)
        self.assertEqual(store.row['overrides'], {'prosperityCause': 'the Crown\'s war levy'})
        self.assertIn('Prosperity +0 -> +35 (the Crown\'s war levy), now.', out['detail'])
        out = e.set_steer('dev', {'prosperity': 35}, 'dm-user')
        self.assertFalse(out['changed'], 'the cause kept when none is sent')
        out = e.set_steer('dev', {'prosperity': 35, 'cause': ''}, 'dm-user')
        self.assertEqual((out['changed'], store.row['overrides']), (True, {}), 'an empty cause: the world picks one')
        self.assertEqual(store.writes[-1][0], 'edit', 'a dev world\'s slider goes through live_edit, logged (Phase 9\'s first repair)')
        out = e.set_steer('dev', {'prosperity': 35, 'now': True}, 'dm-user')
        self.assertEqual((out['changed'], out['now']), (True, True), 'now alone is a change: the times to the mark at once')
        live = FakeStore(self, live=True)
        live.economy.set_steer('live', {'prosperity': 40}, 'dm-user')
        self.assertEqual(live.writes[-1], ('patch', {'prosperity': 40}, ['prosperityCause']), 'a live world\'s, the row')
        for bad in ({'cause': 'x'}, {'prosperity': 5, 'cause': 'the dog days'}, {'prosperity': 5, 'cause': 'x' * 81},
                    {'prosperity': 5, 'cause': 7}, {'prosperity': 5, 'colour': 1}, []):
            with self.assertRaises(D.DMError) as refused:
                e.set_steer('dev', bad, 'dm-user')
            self.assertEqual(refused.exception.status, 400, bad)
        # A world before 0061 steers too (the cause a rule there); before 0058 it can't.
        old = FakeStore(self, slim=False)
        self.assertTrue(old.economy.set_steer('dev', {'prosperity': -40, 'cause': 'a hard winter'}, 'dm-user')['changed'])
        with self.assertRaises(D.DMError) as refused:
            FakeStore(self, supported=False).economy.set_steer('dev', {'prosperity': 5}, 'dm-user')
        self.assertEqual(refused.exception.status, 409)

    def test_the_rules_services_needs_and_seasons(self):
        store = FakeStore(self)
        e = store.economy
        out = e.set_rules('dev', {'set': {'ripple': 40, 'harvestBias': -10}}, 'dm-user')
        self.assertEqual((out['changed'], store.row['rules']), (True, {'ripple': 40, 'harvestBias': -10}))
        self.assertEqual(store.writes[-1][0], 'edit', 'a dev world\'s change goes through live_edit')
        for bad in ({'set': {'prosperity': 20}}, {'set': {'luck': 1}}, {'set': {'ripple': 71}}, {'set': {'harvestBias': -21}}, {'ripple': 1}):
            with self.assertRaises(D.DMError) as refused:
                e.set_rules('dev', bad, 'dm-user')
            self.assertEqual(refused.exception.status, 400, bad)
        self.assertIn('slider', str(self.refusal(e.set_rules, {'set': {'prosperity': 20}})))
        e.set_rules('dev', {'set': {'ripple': None}}, 'dm-user')
        self.assertEqual(store.row['rules'], {'harvestBias': -10})
        trim = next(s for s in map_editor.service_list() if s['id'] == 'groom_and_trim')
        e.set_services('dev', {'set': {'groom_and_trim': {'on': False}}}, 'dm-user')
        self.assertEqual(store.row['services'], {'groom_and_trim': {'on': False, 'price': trim['price'], 'every': trim['every']}})
        e.set_services('dev', {'set': {'groom_and_trim': {'on': True}}}, 'dm-user')
        self.assertEqual(store.row['services'], {}, 'back at market.json\'s: no entry kept')
        self.assertEqual(self.refusal(e.set_services, {'set': {'haircut': {'on': False}}}).status, 400)
        self.assertTrue(e.set_needs('dev', {'needsPaused': True}, 'dm-user')['changed'])
        self.assertTrue(store.row['needsPaused'])
        self.assertEqual(self.refusal(e.set_needs, {'needsPaused': 'yes'}).status, 400)
        grain = next(g for g in map_editor.season_groups() if g['id'] == 'grain')
        e.set_seasons('dev', {'set': {'grain': {'shelf': [100, 100, 130, 100]}}}, 'dm-user')
        self.assertEqual(store.row['seasons'], {'grain': {'price': grain['price'], 'shelf': [100, 100, 130, 100]}})
        self.assertEqual(self.refusal(e.set_seasons, {'set': {'grain': {'price': [130] * 4}}}).status, 400, 'the season room')
        self.assertEqual(self.refusal(e.set_seasons, {'set': {'grain': {'supply': [1] * 4}}}).status, 400, 'supply went')
        e.set_seasons('dev', {'set': {'grain': None}}, 'dm-user')
        self.assertEqual(store.row['seasons'], {})
        # A world before 0061 keeps the season record's four parts, so its people_manifest's line still reads.
        old = FakeStore(self, slim=False, live=True)
        old.economy.set_seasons('dev', {'set': {'grain': {'shelf': [100, 100, 130, 100]}}}, 'dm-user')
        self.assertEqual(old.row['seasons']['grain'], {'supply': [100] * 4, 'price': grain['price'], 'shelf': [100, 100, 130, 100],
                                                      'normal': 0})
        self.assertEqual(old.writes[-1][0], 'save', 'a live world\'s row, as the DM writes it')

    def test_the_overrides_only_with_migration_0061(self):
        store = FakeStore(self, live=True)
        e = store.economy
        self.assertTrue(e.set_imports('dev', {'neverImported': ['salt', 'cheese', 'salt']}, 'dm-user')['changed'])
        self.assertEqual(store.row['overrides'], {'neverImported': ['salt', 'cheese']})
        self.assertEqual(self.refusal(e.set_imports, {'neverImported': ['moonbeams']}).status, 400)
        e.set_status('dev', {'status': {'leanDays': 4, 'caps': {'grantsADay': 300}}}, 'dm-user')
        self.assertEqual(store.row['overrides']['status'], {'leanDays': 4, 'caps': {'grantsADay': 300}})
        bad = copy.deepcopy(C.load_defaults()['status']['shares'])
        bad[1]['shares'] = [40, 40, 40, 20]
        self.assertIn('sums to 140', str(self.refusal(e.set_status, {'status': {'shares': bad}})))
        self.assertEqual(self.refusal(e.set_status, {'status': {'caps': {'theftADay': 10 ** 6}}}).status, 400)
        out = e.set_market('dev', {'market': {'events': {'perWeek': 1, 'list': ['house_wedding']}, 'wharf': {'ships': ['the Gull']}}},
                           'dm-user')
        kinds = store.row['overrides']['market']['events']['list']
        self.assertEqual((out['changed'], [k['id'] for k in kinds], kinds[0]['words']), (True, ['house_wedding'], 'the House wedding'),
                         'a kind by its id is stored whole')
        self.assertEqual(self.refusal(e.set_market, {'market': {'events': {'list': ['moon_landing']}}}).status, 400)
        self.assertEqual(self.refusal(e.set_market, {'market': {'wharf': {'ships': ['the Seadog']}}}).status, 400)
        for name, body in (('imports', {'neverImported': None}), ('status', {'status': None}), ('market', {'market': {}})):
            getattr(e, f'set_{name}')('dev', body, 'dm-user')
        self.assertEqual(store.row['overrides'], {}, 'each back to the data files\'')
        old = FakeStore(self, slim=False)
        for name, body in (('imports', {'neverImported': ['salt']}), ('status', {'status': {'leanDays': 4}}),
                           ('market', {'market': {'events': {'perWeek': 1}}})):
            refused = self.refusal(getattr(old.economy, f'set_{name}'), body)
            self.assertEqual((refused.status, 'needs migration 0061' in str(refused)), (409, True), name)
        # The rest of the economy still changes there, the cause kept.
        old.economy.set_steer('dev', {'prosperity': 5, 'cause': 'fair weather'}, 'dm-user')
        self.assertTrue(old.economy.set_rules('dev', {'set': {'ripple': 30}}, 'dm-user')['changed'])
        self.assertEqual(old.row['overrides'], {'prosperityCause': 'fair weather'})

    def test_the_view(self):
        store = FakeStore(self, entity={'treasury': 900, 'storeHerbs': 10, 'storeMeals': 5, 'needsPaused': False, 'seasons': {},
                                        'rules': {'prosperity': 37, 'ripple': 40, 'steerLean': 150, 'prosperityCause': 'the levy'},
                                        'services': {}})
        v = store.economy.read('dev')
        self.assertEqual((v['supported'], v['slim'], v['problem']), (True, True, ''))
        self.assertEqual(v['prosperity'], {'value': 35, 'cause': 'the levy', 'low': -100, 'high': 100, 'step': 5})
        self.assertEqual(v['economy']['overrides'], {'prosperityCause': 'the levy'}, 'the cause split out of the rules')
        self.assertEqual(v['economy']['rules'], {'prosperity': 37, 'ripple': 40, 'steerLean': 150})
        self.assertEqual(v['heldRules'], ['steerLean'])
        names = [r['name'] for r in v['ruleList']]
        self.assertNotIn('prosperity', names)
        self.assertIn('harvestBias', names)
        self.assertEqual(next(r for r in v['ruleList'] if r['name'] == 'harvestBias'),
                         {'name': 'harvestBias', 'low': -20, 'high': 20, 'fallback': 0, 'switch': False})
        self.assertTrue(next(r for r in v['ruleList'] if r['name'] == 'neverBroke')['switch'])
        market = map_editor.market_file()
        self.assertEqual([g['id'] for g in v['seasonGroups']], [g['id'] for g in market['seasons']])
        self.assertEqual(v['defaults']['market']['wharf']['never'], market['wharf']['never'])
        self.assertEqual(v['defaults']['market']['events']['perWeek'], market['events']['perWeek'])
        self.assertEqual([k['id'] for k in v['defaults']['market']['events']['list']], [k['id'] for k in market['events']['list']])
        self.assertEqual(set(v['defaults']['status']), set(C.STATUS_KEYS))
        self.assertIn({'id': 'cheese', 'name': next(i['name'] for i in C.items_file()['items'] if i['id'] == 'cheese')}, v['goods'])
        for gone in ('businesses', 'loop', 'basket', 'wharfLists', 'comforts', 'farms', 'farmDefaults', 'internalStore'):
            self.assertNotIn(gone, v)
        self.assertFalse(FakeStore(self, supported=False).economy.read('dev')['supported'])

    def refusal(self, setter, body):
        with self.assertRaises(D.DMError) as refused:
            setter('dev', body, 'dm-user')
        return refused.exception


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
@unittest.skipUnless(core_store_ready(), 'world_store\'s slim economy (doc 79 Phase 9, the core lane) is not merged yet')
class EconomyFlowTests(ScratchWorlds, unittest.TestCase):
    """The Economy page for real on throwaway worlds (test_world_sync's copies of a live Greyfen, migrated to 0061): what
    reaches live.economy and the NPC records, the audit and the server's queue; a world before 0061 (DEV's level) and
    before 0058."""

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

    def lines(self, key):
        return self.one(key, "SELECT live.people_manifest('greyfen')").split('\n')

    def test_the_slider_reaches_the_world_and_its_server(self):
        status, data = self.call('GET', '/api/atlas/economy', token='viewer-token')
        self.assertEqual((status, data['supported'], data['slim'], data['prosperity']['value']), (200, True, True, 0), data)
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/economy/steer', {'prosperity': 37, 'cause': 'the war levy', 'now': True},
                                 token='dm-token', headers=editor)
        self.assertEqual((status, data['prosperity']['value'], data['prosperity']['cause']), (200, 35, 'the war levy'), data)
        self.assertIn('rule "prosperity" 35', self.lines(self.dev))
        overrides = [l for l in self.lines(self.dev) if l.startswith('overrides ')]
        self.assertEqual([json.loads(l[len('overrides '):]) for l in overrides], [{'prosperityCause': 'the war levy'}])
        queued = self.sql(self.dev, "SELECT target_id, requested_by, payload FROM dm.actions WHERE kind = 'economy.sync'")
        self.assertEqual(queued, [('economy', 'dm-user', {'now': True})])
        self.assertEqual([r[0] for r in self.sql(self.dev, 'SELECT action FROM dm.audit')], ['economy.steer'])
        status, data = self.call('POST', '/api/dm/economy/steer', {'world': self.live, 'prosperity': 9999}, token='dm-token')
        self.assertEqual((status, data['prosperity']['value']), (200, 100), data)
        self.assertEqual(self.one(self.live, "SELECT rules->>'prosperity' FROM live.economy"), '100')

    def test_two_dms_at_once_on_a_live_world_keep_both_changes(self):
        # Phase 9's second repair: a rules change on a live world read the row, then wrote the whole rules column back; a
        # slider moved by another DM between the two was lost (audited and its sync queued all the same). The row is now
        # locked from the read to the write: the slider waits, then lands on the rules change.
        real = ST.map_editor.check_economy
        seen, results = [], []

        def slider():
            results.append(self.call('POST', '/api/dm/economy/steer', {'world': self.live, 'prosperity': -50, 'cause': 'the war levy'},
                                     token='dm-token'))

        def meanwhile(*args, **kwargs):
            if not seen:
                seen.append(threading.Thread(target=slider))
                seen[0].start()
                seen[0].join(1.5)               # (Before the repair it finished here, under the change; now it waits.)
            return real(*args, **kwargs)

        ST.map_editor.check_economy = meanwhile
        try:
            status, data = self.call('POST', '/api/dm/economy/rules', {'world': self.live, 'set': {'theftCap': 6}}, token='dm-token')
        finally:
            ST.map_editor.check_economy = real
        self.assertEqual(status, 200, data)
        seen[0].join(15)
        self.assertEqual(results[0][0], 200, results)
        stored = self.sql(self.live, "SELECT rules FROM live.economy WHERE world_id = 'greyfen'")[0][0]
        self.assertEqual((stored.get('theftCap'), stored.get('prosperity'), stored.get('prosperityCause')), (6, -50, 'the war levy'), stored)

    def test_the_slider_on_dev_is_logged_and_now_alone_moves_the_times(self):
        # Doc 79, Phase 9's first repair: on a dev world the slider goes through live_edit (open Atlas sessions follow it,
        # its log holds it); "now" with the value as it is still queues economy.sync {now} (a slider easing to its mark).
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/economy/steer', {'prosperity': 20}, token='dm-token', headers=editor)
        self.assertEqual((status, data['changed']), (200, True), data)
        keys = [r[0] for r in self.sql(self.dev, 'SELECT key FROM world.edits ORDER BY seq')]
        self.assertEqual(keys.count('economy'), 1, 'logged through live_edit')
        status, data = self.call('POST', '/api/atlas/economy/steer', {'prosperity': 20, 'now': True}, token='dm-token', headers=editor)
        self.assertEqual((status, data['changed'], data['sync'] is not None), (200, True, True), data)
        status, data = self.call('POST', '/api/atlas/economy/steer', {'prosperity': 20}, token='dm-token', headers=editor)
        self.assertEqual((status, data['changed'], data['sync']), (200, False, None), 'the same value without now: nothing')
        queued = self.sql(self.dev, "SELECT payload FROM dm.actions WHERE kind = 'economy.sync' ORDER BY id")
        # (Through migration 0061's dm.queue_economy_sync, the last repair: it keeps only what the game reads, {"now": true}
        # or nothing.)
        self.assertEqual([q[0] for q in queued], [{}, {'now': True}])
        self.assertEqual(self.one(self.dev, "SELECT rules->>'prosperity' FROM live.economy"), '20')

    def test_an_empty_never_imported_list_and_a_malformed_market(self):
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/economy/imports', {'neverImported': []}, token='dm-token', headers=editor)
        self.assertEqual((status, data['changed']), (200, True), data)
        self.assertEqual(data['economy']['overrides'].get('neverImported'), [], 'ships carry everything: kept, not dropped')
        # (Phase 9's second repair: it says from when, since a ship already on the timetable keeps its cargo.)
        self.assertIn('from the next week\'s timetable on (a ship already posted keeps its cargo)', data['detail'])
        self.assertIn('overrides {"neverImported": []}', self.lines(self.dev))
        for market in ({'events': [1]}, {'events': 'x'}, {'wharf': [2]}):
            status, data = self.call('POST', '/api/atlas/economy/market', {'market': market}, token='dm-token', headers=editor)
            self.assertEqual(status, 400, (market, data))

    def test_the_rules_seasons_services_and_needs(self):
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        for route, body in (('rules', {'set': {'playerSellMost': 300, 'harvestBias': -5}}), ('needs', {'needsPaused': True}),
                            ('services', {'set': {'groom_and_trim': {'on': False}}}),
                            ('seasons', {'set': {'grain': {'shelf': [100, 100, 130, 100]}}})):
            status, data = self.call('POST', f'/api/atlas/economy/{route}', body, token='dm-token', headers=editor)
            self.assertEqual((status, data['changed'], data['sync'] is not None), (200, True, True), (route, data))
        lines = self.lines(self.dev)
        for line in ('rule "playerSellMost" 300', 'rule "harvestBias" -5', 'loop needs off'):
            self.assertIn(line, lines)
        self.assertTrue(any(l.startswith('season "grain" price ') and l.endswith('shelf 100 100 130 100') for l in lines), lines)
        self.assertTrue(any(l.startswith('service "groom_and_trim" off') for l in lines), lines)
        self.assertEqual(self.call('POST', '/api/atlas/economy/rules', {'set': {'prosperity': 5}}, token='dm-token', headers=editor)[0], 400)
        keys = [r[0] for r in self.sql(self.dev, 'SELECT key FROM world.edits ORDER BY seq')]
        self.assertEqual(keys.count('economy'), 4, 'each through live_edit: open Atlas sessions follow')
        self.assertEqual(len(self.sql(self.dev, "SELECT id FROM dm.actions WHERE kind = 'economy.sync'")), 4)

    def test_the_overrides(self):
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        for route, body in (('imports', {'neverImported': ['salt']}), ('status', {'status': {'leanDays': 4}}),
                            ('market', {'market': {'events': {'perWeek': 1}, 'wharf': {'ships': ['the Gull', 'the Heron']}}})):
            status, data = self.call('POST', f'/api/atlas/economy/{route}', body, token='dm-token', headers=editor)
            self.assertEqual((status, data['changed']), (200, True), (route, data))
        line = next(l for l in self.lines(self.dev) if l.startswith('overrides '))
        self.assertEqual(json.loads(line[len('overrides '):]),
                         {'neverImported': ['salt'], 'status': {'leanDays': 4},
                          'market': {'events': {'perWeek': 1}, 'wharf': {'ships': ['the Gull', 'the Heron']}}})
        self.assertEqual(self.call('POST', '/api/atlas/economy/status', {'status': {'leanDays': 'x'}}, token='dm-token',
                                   headers=editor)[0], 400)
        actions = [r[0] for r in self.sql(self.dev, 'SELECT action FROM dm.audit')]
        self.assertEqual(sorted(actions), ['economy.imports', 'economy.market', 'economy.status'])

    def test_a_world_before_migration_0061(self):
        # DEV's level until the user migrates it: the rules, seasons, services, needs and the slider (its cause too)
        # change; the host's overrides reply 409.
        unmigrate_slim(self.dev)
        status, data = self.call('GET', '/api/atlas/economy', token='dm-token')
        self.assertEqual((status, data['supported'], data['slim']), (200, True, False), data)
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        for route, body in (('imports', {'neverImported': ['salt']}), ('status', {'status': {'leanDays': 4}}),
                            ('market', {'market': {'events': {'perWeek': 1}}})):
            status, data = self.call('POST', f'/api/atlas/economy/{route}', body, token='dm-token', headers=editor)
            self.assertEqual(status, 409, data)
            self.assertIn('needs migration 0061', data['error'])
        for route, body in (('steer', {'prosperity': -20, 'cause': 'a hard winter'}), ('rules', {'set': {'ripple': 30}}),
                            ('seasons', {'set': {'grain': {'shelf': [100, 100, 130, 100]}}}), ('needs', {'needsPaused': True})):
            status, data = self.call('POST', f'/api/atlas/economy/{route}', body, token='dm-token', headers=editor)
            self.assertEqual((status, data['changed']), (200, True), (route, data))
        self.assertIn('rule "prosperity" -20', self.lines(self.dev))
        self.assertEqual(self.one(self.dev, "SELECT rules->>'prosperityCause' FROM live.economy"), 'a hard winter')
        self.assertEqual(sorted(self.one(self.dev, "SELECT seasons->'grain' FROM live.economy")), ['normal', 'price', 'shelf', 'supply'],
                         'four parts, so 0060\'s season line still reads')

    def test_a_world_without_migration_0058(self):
        unmigrate_rules(self.dev)
        status, data = self.call('GET', '/api/atlas/economy', token='dm-token')
        self.assertEqual((status, data['supported']), (200, False), data)
        self.assertIn('lacks migration 0058', data['problem'])
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        for route, body in (('rules', {'set': {'playerSellMost': 300}}), ('steer', {'prosperity': 5}), ('needs', {'needsPaused': True})):
            status, data = self.call('POST', f'/api/atlas/economy/{route}', body, token='dm-token', headers=editor)
            self.assertEqual(status, 409, data)
            self.assertIn('0058', data['error'])
            status, data = self.call('POST', f'/api/dm/economy/{route}', {'world': self.dev, **body}, token='dm-token')
            self.assertEqual(status, 409, data)

    def test_a_world_without_migration_0057_or_0055(self):
        unmigrate_seasons(self.dev)
        status, data = self.call('GET', '/api/atlas/economy', token='dm-token')
        self.assertEqual((status, data['supported']), (200, False), data)
        unmigrate_store(self.dev)
        status, data = self.call('GET', '/api/dm/economy?world=' + self.dev, token='viewer-token')
        self.assertEqual((status, data['supported']), (200, False), data)
        status, data = self.call('POST', '/api/dm/economy/seasons', {'world': self.dev, 'set': {'grain': None}}, token='dm-token')
        self.assertEqual(status, 409, data)


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
        self.assertEqual({r['name'] for r in ST.Economy.rule_list()}, set(map_editor.RULES) - {'prosperity'}, 'the slider\'s apart')

    def test_a_rule_the_game_no_longer_has_is_kept_and_ignored(self):
        # Doc 79, 14 and 17: an old world's rule (the steering's, the loop's) refuses nothing and warns; it reaches the world
        # file as it reaches the database's manifest (Phase 9's second repair: the two writers agreed on nothing here, and
        # the Rules panel said it was never sent), and Core reads it past with a warning. A known rule out of its range is
        # still refused.
        rules = {'steer': 1, 'almsADay': 12, 'neverBroke': 1}
        self.assertEqual(map_editor.check_rules(rules, {}), [])
        self.assertEqual(map_editor.unknown_rules(rules), ['almsADay', 'steer'])
        p = greyfen()
        p['economy']['rules'] = rules
        lines = map_editor.export_files(p)['world.ratw'].splitlines()
        self.assertEqual([l for l in lines if l.startswith('rule ')], ['rule "almsADay" 12', 'rule "neverBroke" 1', 'rule "steer" 1'])
        self.assertTrue(map_editor.check_rules({'halfLifeDurable': 4}, {}), 'under its low')
        self.assertTrue(map_editor.check_rules({'neverBroke': 2}, {}))

    def test_no_steering_but_the_slider(self):
        # Doc 79, 12 and 17: the steering went with the conserving economy; the slider came back carrying prosperity.
        for name in ('steer_at', 'steer_lever', 'steer_edicts', 'steer_ledger', 'STEER_LEVERS'):
            self.assertFalse(hasattr(ST, name), name)
        self.assertTrue(hasattr(ST.Economy, 'set_steer'))
        self.assertFalse(hasattr(map_editor, 'STEER_SETTINGS'))
        for rule in ('steer', 'steerLean'):
            self.assertNotIn(rule, map_editor.RULES)


class ImportsTests(unittest.TestCase):
    def test_the_goods_ships_never_carry_come_from_market_json_until_a_world_sets_its_own(self):
        """Doc 79, 12: wharfOff became neverImported, a goods list over the catalogue's goods (the Atlas Wharf panel
        retargeted); market.json's wharf.never until a world sets its own."""
        market = map_editor.market_file().get('wharf') or {}
        defaults = ST.Economy.defaults()
        self.assertEqual(defaults['market']['wharf']['never'], market.get('never'))
        self.assertEqual(defaults['market']['wharf']['ships'], market.get('ships'))
        goods = {g['id'] for g in ST.Economy.goods()}
        self.assertTrue(set(market.get('never') or []) <= goods)
        self.assertTrue(set(market.get('imports') or []) <= goods)


class BindTests(unittest.TestCase):
    def test_the_studio_serves_this_computer_only_unless_told(self):
        """Docs/Design/78, Q25 (Phase M1): plain HTTP guarded by a header check binds 127.0.0.1 by default; --host 0.0.0.0
        is asked for (tools/studio.sh asks for it unless its arguments name a --host; editor.sh and dungeon-master.sh
        ask for 127.0.0.1)."""
        from unittest import mock

        class Bound(Exception):
            pass

        def bound(port, studio, host):
            raise Bound(host)

        saved = os.environ.get('RATW_SERVE_HOSTS')
        try:
            with mock.patch.object(ST, 'make_server', bound), mock.patch.object(ST, 'Studio', lambda **_: None), \
                    mock.patch.object(ST.world_db, 'resolve', lambda *_: 'atlas'), \
                    mock.patch.object(ST, 'lan_names', lambda: ['192.0.2.7']):
                for argv, host in ((['serve'], '127.0.0.1'), (['serve', '--host', '0.0.0.0'], '0.0.0.0')):
                    with self.assertRaises(Bound) as raised:
                        ST.main(argv)
                    self.assertEqual(raised.exception.args[0], host, argv)
        finally:
            os.environ.pop('RATW_SERVE_HOSTS', None)
            if saved is not None:
                os.environ['RATW_SERVE_HOSTS'] = saved
        here = Path(__file__).resolve().parent
        launcher = (here / 'studio.sh').read_text()
        self.assertIn('lan=(--host 0.0.0.0)', launcher)
        for name in ('editor.sh', 'dungeon-master.sh'):
            self.assertIn('--host 127.0.0.1 "$@"', (here / name).read_text(), name)


# ----------------------------------------------------------------------------------------- hosted mode (doc 78, M3)

class HostedDM(StandInDM):
    """The stand-in DM in hosted mode: no database (connect refuses), reports and storytellers as a test sets them, and
    the world map's `lean` recorded."""

    def __init__(self):
        super().__init__()
        self.report_rows, self.standing, self.ai_audit = [], [], None

    def connect(self, target):
        raise D.DMError('No database in this test.', 503)

    def reports(self, target):
        self.calls.append(('reports', (target,)))
        return {'target': target, 'ready': True, 'reports': copy.deepcopy(self.report_rows),
                'actions': [{'id': 1, 'target': r['reportedCharacter']} for r in self.report_rows]}

    def storytellers(self, target):
        self.calls.append(('storytellers', (target,)))
        return {'target': target, 'ready': True, 'standing': copy.deepcopy(self.standing), 'tales': [], 'log': []}

    def world_map(self, target, lean=False):
        self.calls.append(('world_map', (target, lean)))
        return {'lean': lean}


class HostedAtlasWorld:
    """Atlas's world (live_edit.LiveWorld) as the hosted handler sees it: `load(lean)` recorded."""

    def __init__(self, roster):
        self.key, self.loads = 'devworld', []
        self.roster = map_editor.RosterFile(roster)

    def load(self, lean=False):
        self.loads.append(lean)
        return {'project': {}, 'seq': 0, 'gameSeq': 0, 'bands': True}

    def edit(self, data):
        self.edits = getattr(self, 'edits', []) + [data]
        return {'seq': 1}


HOSTED_REGISTRY = {'worlds': [
    {'key': 'world', 'name': 'Ashfall Vale', 'database': 'ratw_test_s2stage_no_such_database', 'kind': 'live'},
    {'key': 'devworld', 'name': 'Ashfall Draft', 'database': 'ratw_test_s2stage_no_such_database_dev', 'kind': 'dev'}]}


class HostedBase(unittest.TestCase):
    """A HostedStudio on stand-ins (no database, nothing on the network): a registry of its own (`world`, live; and
    `devworld`, dev, for Atlas) and an admin folder with a settings copy."""
    world = 'world'

    def setUp(self):
        from unittest import mock
        self.temp = tempfile.TemporaryDirectory(prefix='ratw-hosted-test-')
        self.folder = Path(self.temp.name)
        (self.folder / 'worlds.json').write_text(json.dumps(HOSTED_REGISTRY))
        self.settings(cheats='off')
        self.env = mock.patch.dict(os.environ, {'RATW_WORLDS_FILE': str(self.folder / 'worlds.json'),
                                                'RATW_WORLDS_LOCAL': str(self.folder / 'worlds.local.json'),
                                                'RATW_AI_CONFIG': str(self.folder / 'no-ai.json')})
        self.env.start()
        self.roster = self.folder / 'roster.json'
        map_editor.roster_lib.save({**map_editor.roster_lib.load(), 'characters': []}, self.roster)
        self.make(self.world)

    def make(self, world):
        """A HostedStudio on `world` ('world', live; 'devworld', dev) with fresh stand-ins."""
        self.dm, self.sync, self.economy = HostedDM(), StandInSync(), StandInEconomy()
        self.atlas_world = HostedAtlasWorld(self.roster)
        self.hosted = ST.HostedStudio(ST.Hosted(world, self.folder), dm=self.dm, sync=self.sync, economy=self.economy,
                                      atlas=lambda key: map_editor.make_handler(roster_path=self.roster, world=self.atlas_world,
                                                                                ai_config_path=self.folder / 'no-ai.json', hosted=True))

    def tearDown(self):
        self.assertFalse((self.folder / 'worlds.local.json').exists(), 'no world is ever chosen')
        self.env.stop()
        self.temp.cleanup()

    def settings(self, **values):
        (self.folder / 'settings.json').write_text(json.dumps({'name': 'Ashfall Vale', 'package': 'test-1', **values}))

    def call(self, method, path, body=None, role='dm', wolves=False, query='', headers=None):
        session = {'username': f'pell#a1b2c3', 'account': 'pell', 'role': role, 'mayActOnWolves': wolves}
        raw = json.dumps(body).encode() if body is not None else b''
        status, kind, out = self.hosted(session, method, path, query, {'content-type': 'application/json', **(headers or {})}, raw)
        return status, (json.loads(out) if kind == 'application/json' and out else out)


def served_routes():
    """Every (method, path) the studio serves, read from the handlers' own code (the DM's, Atlas's and the studio's):
    what the allowlist must decide."""
    import inspect
    found = set()
    decided = re.compile(r"method == '(GET|POST)' and path == '(/api/[^']+)'")
    dm_source = inspect.getsource(D.make_handler)
    dm_routes = set(decided.findall(dm_source)) | {('POST', p) for p in re.findall(r"^\s+'(/api/[^']+)': lambda", dm_source, re.M)}
    studio_routes = ST.SERVER_ROUTES | ST.DM_ECONOMY_ROUTES
    for method, path in dm_routes:
        if ST.dm_offers(method, path) and (method, path) not in studio_routes:
            found.add((method, '/api/dm/' + path[len('/api/'):]))
        if ST.layer_offers(method, path):
            found.add((method, '/api/atlas/layers/' + path[len('/api/'):]))
    found |= {(m, '/api/dm/' + p[len('/api/'):]) for m, p in studio_routes}
    found |= {(m, '/api/atlas/' + p[len('/api/'):]) for m, p in ST.SYNC_ROUTES | ST.PLAY_ROUTES | ST.ECONOMY_ROUTES}
    studio_source = inspect.getsource(ST.make_handler)
    own = studio_source[studio_source.index('        def studio(self, method, path):'):]
    own = own[:own.index('        def play(self, method):')]
    found |= {(m, '/api/studio/' + p[len('/api/'):]) for m, p in decided.findall(own)}
    atlas = inspect.getsource(map_editor.make_handler)
    gets = atlas[atlas.index('        def do_GET(self):'):atlas.index('        def editing(self):')]
    posts = atlas[atlas.index('        def do_POST(self):'):]
    found |= {('GET', '/api/atlas/' + p[len('/api/'):]) for p in re.findall(r"path == '(/api/[^']+)'", gets)}
    named = re.findall(r"path == '(/api/[^']+)'", posts) + [p for group in re.findall(r"path in \(([^)]*)\)", posts)
                                                          for p in re.findall(r"'(/api/[^']+)'", group)]
    found |= {('POST', '/api/atlas/' + p[len('/api/'):]) for p in named}
    found |= {(m, '/api/atlas/' + p[len('/api/'):]) for p in ST.NOT_IN_ATLAS for m in ('GET', 'POST')}
    return found


# The routes HostedStudio answers itself (not the studio's handlers).
HOSTED_OWN = {('GET', '/api/studio/ai'), ('GET', '/api/world'), ('POST', '/api/world/restart'), ('POST', '/api/world/backup'),
              ('POST', '/api/world/pin'), ('POST', '/api/world/share'),
              ('GET', '/api/atlas/map'), *(('POST', f'/api/atlas/map/{act}') for act in ('publish', 'back', 'discard', 'open', 'rebuild'))}


class HostedAllowlistTests(HostedBase):
    """Doc 78, 7 and 18 ("the allowlist"): deny by default; every route the studio serves is allowed with a role or
    denied by name; each allowed route refuses the roles below it; denied and unknown routes are 404 for anyone."""

    def test_every_route_the_studio_serves_is_allowed_or_denied_by_name(self):
        served = served_routes()
        self.assertGreater(len(served), 120, 'the scan found the handlers\' routes')
        undecided = sorted(r for r in served if r not in ST.HOSTED_ROUTES and r not in ST.HOSTED_DENIED)
        self.assertEqual(undecided, [], 'each route the studio serves needs a decision in HOSTED_ROUTES or HOSTED_DENIED')
        self.assertFalse(set(ST.HOSTED_ROUTES) & set(ST.HOSTED_DENIED))
        stale = sorted(r for r in ST.HOSTED_ROUTES if r not in served and r not in HOSTED_OWN)
        self.assertEqual(stale, [], 'an allowed route the studio no longer serves')
        # The patterned routes: a queued action's answer, on the DM's world and on Atlas's layers.
        self.assertEqual(ST.hosted_role('GET', '/api/dm/actions/42'), 'viewer')
        self.assertEqual(ST.hosted_role('GET', '/api/atlas/layers/actions/42'), 'builder')
        for odd in ('/api/dm/actions/x', '/api/dm/actions/', '/api/dm/actions/1/2', '/api/dm/actions/1?x'):
            self.assertIsNone(ST.hosted_role('GET', odd), odd)
        self.assertTrue(all(role in ('viewer', 'dm', 'builder', 'owner') for role in ST.HOSTED_ROUTES.values()),
                        'every allowed route names its role, and none is admin')

    def test_each_allowed_route_refuses_the_roles_below_it(self):
        roles = ('viewer', 'dm', 'builder', 'owner')
        self.make('devworld')                                         # (Atlas's routes reached on a dev world.)
        routes = [*ST.HOSTED_ROUTES.items(), (('GET', '/api/dm/actions/42'), 'viewer'),
                  (('GET', '/api/atlas/layers/actions/42'), 'builder')]
        for (method, path), lowest in routes:
            for role in roles:
                status, data = self.call(method, path, role=role, wolves=True)
                if roles.index(role) < roles.index(lowest):
                    self.assertEqual(status, 403, f'{role} on {method} {path}: {data}')
                else:
                    self.assertNotEqual(status, 403, f'{role} on {method} {path}: {data}')
                    self.assertFalse(status == 404 and data == {'error': 'Not found.'}, f'{role} on {method} {path}')

    def test_denied_and_unknown_routes_are_404_for_the_owner_too(self):
        for (method, path) in [*ST.HOSTED_DENIED, ('GET', '/api/nothing'), ('POST', '/api/dm/server/switch'),
                               ('GET', '/api/dm/npcs/save'), ('POST', '/api/dm/npcs/save'), ('POST', '/api/dm/talk/voice'),
                               ('GET', '/api/atlas/export'), ('PUT', '/api/dm/me'), ('GET', '/api/studio/session/x')]:
            self.assertEqual(self.call(method, path, role='owner', wolves=True), (404, {'error': 'Not found.'}), (method, path))
        self.assertEqual(self.dm.calls, [], 'a refused route reaches nothing')
        self.assertEqual(self.sync.calls, [])
        self.assertFalse((self.folder / 'worlds.local.json').exists(), 'no world is ever chosen')

    def test_the_session_is_the_editor_s_and_admin_never_is(self):
        status, me = self.call('GET', '/api/dm/me', role='builder')
        self.assertEqual(status, 200)
        self.assertEqual((me['username'], me['account'], me['role'], me['hosted'], me['cheats'], me['mayActOnWolves']),
                         ('pell#a1b2c3', 'pell', 'builder', True, 'off', False))
        self.assertTrue(self.call('GET', '/api/dm/me', role='owner')[1]['mayActOnWolves'], 'the owner may, always')
        self.assertEqual(self.call('GET', '/api/dm/me', role='admin')[0], 401, 'no hosted session is ever admin')
        status, session = self.call('GET', '/api/studio/session', role='dm')
        self.assertEqual(status, 200, session)
        self.assertEqual((session['hosted'], session['dm']['world'], [w['key'] for w in session['dm']['worlds']],
                          session['atlas']['available'], session['atlas']['reason'], session['myWorld']),
                         (True, 'world', ['world'], False, ST.ATLAS_LATER, False))
        self.assertTrue(self.call('GET', '/api/studio/session', role='owner')[1]['myWorld'])
        self.assertEqual(self.call('GET', '/api/dm/worlds')[1]['worlds'], [{'key': 'world', 'name': 'Ashfall Vale', 'kind': 'live'}])
        # The world's name in the Editor's headings is world.toml's (the settings copy), as players and the host see it,
        # whatever the registry says (R1: "Ridgemere 2.0" beside "Ridgemere").
        self.settings(name='Ashfall')
        session = self.call('GET', '/api/studio/session', role='dm')[1]
        self.assertEqual((session['dm']['worlds'][0]['name'], session['atlas']['world']['name']), ('Ashfall', 'Ashfall'))
        # The DM's acts are audited as the account and its session.
        self.assertEqual(self.call('POST', '/api/dm/actions', {'kind': 'bandits.call', 'characterId': 'c1'})[0], 200)
        self.assertEqual(self.dm.calls[-1][1][0]['username'], 'pell#a1b2c3')
        # Another world is never reached: one world.
        self.assertEqual(self.call('GET', '/api/dm/players', query='target=prod')[0], 400)
        self.assertEqual(self.call('GET', '/api/dm/economy', query='world=prod')[0], 400)

    def test_atlas_needs_builder_and_waits_for_map_editing_on_a_live_world(self):
        self.assertEqual(self.call('GET', '/api/atlas/roster', role='dm')[0], 403)
        self.assertEqual(self.call('GET', '/api/atlas/roster', role='builder'), (409, {'error': ST.ATLAS_LATER}))
        self.make('devworld')
        status, roster = self.call('GET', '/api/atlas/roster', role='builder')
        self.assertEqual(status, 200, roster)
        session = self.call('GET', '/api/studio/session', role='dm')[1]
        self.assertEqual((session['atlas']['available'], session['atlas']['reason']), (False, 'Your account is for the DM tool only.'))
        self.assertTrue(self.call('GET', '/api/studio/session', role='builder')[1]['atlas']['available'])
        # Atlas's writes take the session in place of its editor token: a builder's, not a dm's.
        status, data = self.call('POST', '/api/atlas/roster/plan', {'slots': []}, role='builder')
        self.assertNotIn(status, (403, 404), data)

    def test_the_world_is_only_ever_sent_lean(self):
        """M0a: the whole world is 235 MB raw on DEV (45 MB gzipped), over the Editor's 32 MB caps."""
        self.assertEqual(self.call('GET', '/api/dm/world', role='viewer')[0], 200)
        self.assertEqual(self.call('GET', '/api/dm/world', role='viewer', query='lean=0')[0], 200)
        self.assertEqual([c[1][1] for c in self.dm.calls if c[0] == 'world_map'], [True, True])
        self.make('devworld')
        self.assertEqual(self.call('GET', '/api/atlas/live/world', role='builder')[0], 200)
        self.assertEqual(self.atlas_world.loads, [True])

    def test_players_wolves_cheats_and_the_dev_console(self):
        """Q24: kill, injure, move and bring back a player's wolf need the owner or the flag; Q7: gifts and tiers need
        cheats; character.dm never (no hosted account is admin)."""
        for kind in ('character.kill', 'character.injury', 'character.resurrect'):
            status, data = self.call('POST', '/api/dm/actions', {'kind': kind, 'characterId': 'c1'}, role='builder')
            self.assertEqual((status, data['error']), (403, ST.WOLVES_REASON), kind)
            self.assertEqual(self.call('POST', '/api/dm/actions', {'kind': kind, 'characterId': 'c1'}, role='dm', wolves=True)[0], 200)
            self.assertEqual(self.call('POST', '/api/dm/actions', {'kind': kind, 'characterId': 'c1'}, role='owner')[0], 200)
        move = {'kind': 'character.move', 'id': 'c1', 'cell': 'square', 'x': 1, 'y': 1}
        self.assertEqual(self.call('POST', '/api/dm/live/move', move)[0], 403)
        self.assertEqual(self.call('POST', '/api/dm/live/move', move, wolves=True)[0], 200)
        self.assertEqual(self.call('POST', '/api/dm/live/move', {**move, 'kind': 'npc.move'})[0], 200, 'an NPC needs no flag')
        for kind in ('mentor.revoke', 'tie.end', 'bandits.call', 'character.grant'):
            self.assertEqual(self.call('POST', '/api/dm/actions', {'kind': kind, 'characterId': 'c1'})[0], 200, kind)
        for kind in ST.CHEAT_KINDS:
            status, data = self.call('POST', '/api/dm/actions', {'kind': kind, 'characterId': 'c1'}, role='owner')
            self.assertEqual(status, 403)
            self.assertIn('cheats', data['error'])
        self.settings(cheats='host')
        for kind in ST.CHEAT_KINDS:
            self.assertEqual(self.call('POST', '/api/dm/actions', {'kind': kind, 'characterId': 'c1'})[0], 200, kind)
        self.assertEqual(self.call('GET', '/api/dm/me')[1]['cheats'], 'host')
        for cheats in ('host', 'everyone'):
            self.settings(cheats=cheats)
            status, data = self.call('POST', '/api/dm/actions', {'kind': 'character.dm', 'characterId': 'c1',
                                                                 'payload': {'dungeonMaster': True}}, role='owner')
            self.assertEqual((status, data['error']), (403, ST.NO_DEV_CONSOLE))

    def test_close_and_open_are_the_owner_s_and_never_the_publisher_s(self):
        self.assertEqual(self.call('POST', '/api/dm/server/maintenance', {'on': True}, role='builder')[0], 403)
        self.assertEqual(self.call('POST', '/api/world/restart', role='builder')[0], 403)
        status, data = self.call('POST', '/api/world/restart', role='owner')
        self.assertEqual((status, data), (200, {'ok': True, 'restarting': True}))
        made = list((self.folder / 'requests').iterdir())
        self.assertEqual([p.read_text() for p in made], ['restart\n'], 'a kind-only request for the root entrypoint')
        self.assertEqual(self.dm.calls[-1][0], 'log')
        self.assertEqual(self.sync.calls, [], 'never through the sync engine (the publisher)')

    def test_backups_by_hand_and_pins_are_the_owner_s_and_capped(self):
        """Doc 78, 12.1 (M5): a backup now and a pin are kind-only requests (a pin names only the backup's time), the
        owner's; a sixth pin is refused here before the backup service would; no backup service, no requests."""
        self.assertEqual(self.call('POST', '/api/world/backup', role='owner'), (409, {'error': ST.NO_BACKUPS}))
        now = int(time.time())
        items = [{'name': f'ratw-2026111{i}T140300Z-daily', 'at': now - i * 86400, 'size': 1000,
                  'pinnedUntil': now + 86400 if i < 5 else None} for i in range(6)]
        (self.folder / 'backups.json').write_text(json.dumps({'v': 1, 'at': now, 'backups': items, 'disk': {}}))
        for role in ('viewer', 'dm', 'builder'):
            self.assertEqual(self.call('POST', '/api/world/backup', role=role)[0], 403, role)
            self.assertEqual(self.call('POST', '/api/world/pin', {'name': items[5]['name'], 'pinned': True}, role=role)[0], 403)
        self.assertEqual(self.call('POST', '/api/world/backup', role='owner'), (200, {'ok': True, 'asked': True}))
        status, data = self.call('POST', '/api/world/pin', {'name': items[5]['name'], 'pinned': True}, role='owner')
        self.assertEqual((status, data['error']), (409, '5 backups are pinned already: unpin one first.'))
        self.assertEqual(self.call('POST', '/api/world/pin', {'name': 'ratw-x/../../etc', 'pinned': True}, role='owner')[0], 404)
        self.assertEqual(self.call('POST', '/api/world/pin', {'name': items[0]['name'], 'pinned': False}, role='owner')[0], 200)
        made = sorted(p.read_text() for p in (self.folder / 'requests').iterdir())
        self.assertEqual(made, ['backup-by-hand\n', 'unpin-20261110T140300Z\n'])
        shown = ST.Hosted(folder=self.folder).backups()        # (What My world shows: test_world_admin reads it whole.)
        self.assertEqual((shown['pins'], shown['last']['name'], shown['pinCap']), (5, items[0]['name'], 5))

    def test_share_my_world_is_the_owner_s_one_at_a_time_and_three_an_hour(self):
        """Doc 78, 10.5 (M9): Share my world is a kind-only `share` request for root, the owner's; refused while one is
        being made or a request already waits, and after three this hour; with an older package (no share.json) there
        is nothing to ask."""
        self.assertEqual(self.call('POST', '/api/world/share', role='owner'), (409, {'error': ST.NO_SHARE}))
        now = int(time.time())
        state = {'v': 1, 'state': 'idle', 'at': now, 'recent': [], 'files': []}
        (self.folder / 'share.json').write_text(json.dumps(state))
        for role in ('viewer', 'dm', 'builder'):
            self.assertEqual(self.call('POST', '/api/world/share', role=role)[0], 403, role)
        self.assertEqual(self.call('POST', '/api/world/share', role='owner'), (200, {'ok': True, 'asked': True}))
        self.assertEqual([p.read_text() for p in (self.folder / 'requests').iterdir()], ['share\n'])
        self.assertEqual(self.dm.calls[-1][0], 'log')
        status, data = self.call('POST', '/api/world/share', role='owner')
        self.assertEqual((status, data['error']), (409, 'Your world\'s file is being made now: one at a time.'),
                         'a request waiting for root')
        for p in (self.folder / 'requests').iterdir():
            p.unlink()
        (self.folder / 'share.json').write_text(json.dumps({**state, 'state': 'working', 'started': now}))
        self.assertEqual(self.call('POST', '/api/world/share', role='owner')[0], 409, 'one being made')
        files = [{'name': 'ashfall-vale-2026-11-12.ratwpkg', 'size': 5_000_000, 'at': now},
                 {'name': '../../etc/passwd', 'size': 1, 'at': now}]
        (self.folder / 'share.json').write_text(json.dumps({**state, 'state': 'ready', 'file': 'ashfall-vale-2026-11-12.ratwpkg',
                                                            'size': 5_000_000, 'recent': [now - 4000, now - 60, now - 30, now - 10],
                                                            'files': files}))
        status, data = self.call('POST', '/api/world/share', role='owner')
        self.assertEqual((status, data['error']), (429, 'Your world has been shared 3 times this hour, the most an hour: '
                                                        'try again later.'))
        shown = ST.Hosted(folder=self.folder).share()           # (What My world shows: test_world_admin reads it whole.)
        self.assertEqual((shown['state'], shown['file'], shown['thisHour'], [f['name'] for f in shown['files']]),
                         ('ready', 'ashfall-vale-2026-11-12.ratwpkg', 3, ['ashfall-vale-2026-11-12.ratwpkg']),
                         'only a share\'s own file names')
        (self.folder / 'share.json').write_text(json.dumps({**state, 'state': 'failed', 'file': 'x; rm -rf',
                                                            'problem': {'code': 'disk', 'free': 10, 'need': 99, 'where': 'folder'}}))
        shown = ST.Hosted(folder=self.folder).share()
        self.assertEqual((shown['file'], shown['problem']), (None, {'code': 'disk', 'free': 10, 'need': 99, 'where': 'folder'}))
        self.assertEqual(self.call('POST', '/api/world/share', role='owner')[0], 200, 'a failed one can be tried again')

    def test_page_files_are_the_built_app_only(self):
        status, kind = self.hosted({'username': 'pell#a1b2c3', 'role': 'viewer'}, 'GET', '/', '', {}, b'')[:2]
        self.assertEqual((status, kind), (200, 'text/html'))
        for path in ('/../tools/studio.py', '/assets/../../Database/.env', '/portraits/../x.png', '/index.py'):
            self.assertEqual(self.hosted({'username': 'pell#a1b2c3', 'role': 'viewer'}, 'GET', path, '', {}, b'')[0], 404, path)
        self.assertEqual(self.hosted({'username': 'pell#a1b2c3', 'role': 'viewer'}, 'POST', '/', '', {}, b'')[0], 405)

    def test_the_images_signed_editor_is_served_bundle_and_portraits_too(self):
        """Doc 78, M4 and M3: the image's Editor is /opt/ratw/Editor/dist (stage.sh's app/Editor/dist: the built page,
        its signed bundle.json and portraits/ inside, Data/Portraits beside it), the studio's DIST and PORTRAITS beside
        tools/; the shell's proxy asks for /bundle.json first, then only the files it lists."""
        from unittest import mock
        self.assertEqual((map_editor.DIST.relative_to(map_editor.ROOT).as_posix(),
                          map_editor.PORTRAITS.relative_to(map_editor.ROOT).as_posix(),
                          Path(ST.__file__).resolve().parent.relative_to(map_editor.ROOT).as_posix()),
                         ('Editor/dist', 'Data/Portraits', 'tools'), '/opt/ratw/tools beside /opt/ratw/Editor/dist')
        dist, portraits = self.folder / 'app' / 'Editor' / 'dist', self.folder / 'app' / 'Data' / 'Portraits'
        (dist / 'assets').mkdir(parents=True)
        (dist / 'portraits').mkdir()
        portraits.mkdir(parents=True)
        (dist / 'index.html').write_text('<!doctype html><script src="./assets/a.js"></script>')
        (dist / 'assets' / 'a.js').write_text('run()')
        (dist / 'bundle.json').write_text('{"v": 1, "key": "ratw-0", "manifest": "", "signature": ""}')
        for folder in (dist / 'portraits', portraits):
            (folder / 'timber.png').write_bytes(b'\x89PNG timber')
        who = {'username': 'pell#a1b2c3', 'role': 'viewer'}
        with mock.patch.object(map_editor, 'DIST', dist), mock.patch.object(map_editor, 'PORTRAITS', portraits):
            for path, kind, body in (('/bundle.json', 'application/json', (dist / 'bundle.json').read_bytes()),
                                     ('/', 'text/html', (dist / 'index.html').read_bytes()),
                                     ('/assets/a.js', 'text/javascript', b'run()'),
                                     ('/portraits/timber.png', 'image/png', b'\x89PNG timber')):
                self.assertEqual(self.hosted(who, 'GET', path, '', {}, b''), (200, kind, body), path)
            for path in ('/portraits/../bundle.json', '/portraits/Timber.png', '/bundle.json/..', '/../Data/Portraits/timber.png'):
                self.assertEqual(self.hosted(who, 'GET', path, '', {}, b'')[0], 404, path)

    def test_the_browser_rule_holds_inside_too(self):
        """I9: the helper refuses Origin, Cookie and Sec-Fetch-*; the hosted handlers' own check says so again."""
        handler = self.hosted.handler.__new__(self.hosted.handler)
        handler.editor_session = {'username': 'pell#a1b2c3', 'role': 'owner'}
        import http.client as hc
        import io
        for extra in ('Origin: https://evil.example', 'Cookie: a=b', 'Sec-Fetch-Site: cross-site', ''):
            handler.headers = hc.parse_headers(io.BytesIO(((extra + '\r\n') if extra else '').encode() + b'\r\n'))
            self.assertEqual(map_editor.hosted_trusted(handler), not extra, extra)
        handler.editor_session = None
        self.assertFalse(map_editor.hosted_trusted(handler), 'no session, no trust')
        # And the user's own studio ignores an editor_session: its Host check stands.
        plain = ST.make_handler(ST.Studio(dm=StandInDM(), atlas=lambda key: map_editor.make_handler(), atlas_key='dev-old',
                                          sync=StandInSync(), choose=lambda k: None, choose_dm=lambda k: None))
        view = plain.__new__(plain)
        view.editor_session = {'username': 'x', 'role': 'owner'}
        view.headers = hc.parse_headers(io.BytesIO(b'Host: evil.example\r\n\r\n'))
        view.server = type('S', (), {'server_port': 8765})()
        self.assertFalse(view.trusted())


class StandInPublisher:
    """tools/world_publish.py's Publisher, recorded: what the hosted routes hand it."""

    def __init__(self):
        self.calls, self.place, self.refuse = [], None, None

    def _did(self, name, *args):
        self.calls.append((name, *args))
        if self.refuse:
            raise self.refuse
        return {'ok': True}

    def state(self):
        return {'unpublished': {'count': 2}, 'job': None}

    def summary(self):
        return {'unpublished': 2, 'memory': {'ok': True, 'words': 'This computer can publish map changes.'}}

    def ahead(self, body):
        return self.place

    def start(self, who, restart, rebuild=False):
        return self._did('start', who['username'], restart, rebuild)

    def back(self, who, discard_too=False):
        return self._did('back', discard_too)

    def discard(self, who):
        return self._did('discard')

    def open_all(self, who):
        return self._did('open')


class HostedPublishTests(HostedBase):
    """Doc 78, 10.3 and 12.3 (Phase M8): Publish's routes are the builder's on a world with its map's source (the update
    rebuild's answer the owner's), a refusal comes back with each problem, Atlas's edits are the signed-in account's,
    a live edit ahead of the map is refused, and there is no Publish without a backup."""

    def test_publish_is_the_builder_s_on_a_world_with_its_map_source(self):
        import world_publish
        self.assertEqual(self.call('GET', '/api/atlas/map', role='dm')[0], 403)
        self.assertEqual(self.call('GET', '/api/atlas/map', role='builder'), (409, {'error': ST.ATLAS_LATER}))
        self.assertIsNone(self.hosted.map_summary(), 'a world without its map\'s source has no map lines')
        self.make('devworld')
        pub = self.hosted.publisher = StandInPublisher()
        self.assertEqual(self.call('GET', '/api/atlas/map', role='builder'), (200, {'unpublished': {'count': 2}, 'job': None}))
        self.assertEqual(self.hosted.map_summary()['unpublished'], 2, 'My world\'s count')
        self.assertEqual(self.call('POST', '/api/atlas/map/publish', {'restart': 'host'}, role='builder')[0], 200)
        self.assertEqual(self.call('POST', '/api/atlas/map/back', {'discard': True}, role='builder')[0], 200)
        self.assertEqual(self.call('POST', '/api/atlas/map/discard', {}, role='builder')[0], 200)
        self.assertEqual(self.call('POST', '/api/atlas/map/open', {}, role='builder')[0], 200)
        self.assertEqual(self.call('POST', '/api/atlas/map/rebuild', {'discard': True}, role='builder')[0], 403, 'the owner\'s')
        self.assertEqual(self.call('POST', '/api/atlas/map/rebuild', {'discard': True}, role='owner')[0], 200)
        self.assertEqual(pub.calls, [('start', 'pell#a1b2c3', 'host', False), ('back', True), ('discard',), ('open',),
                                     ('discard',), ('start', 'pell#a1b2c3', 'later', True)])
        pub.refuse = world_publish.Refused([{'text': 'A wolf rents a flat in old_inn.', 'cell': 'old_inn', 'x': 2, 'y': 3}])
        status, data = self.call('POST', '/api/atlas/map/back', {}, role='builder')
        self.assertEqual((status, data['error'], data['errors'][0]['cell']), (422, 'A wolf rents a flat in old_inn.', 'old_inn'))
        pub.refuse = world_publish.Refused(['One at a time.'], 409)
        self.assertEqual(self.call('POST', '/api/atlas/map/publish', {'restart': 'now'}, role='builder')[0], 409)

    def test_atlas_edits_are_the_signed_in_account_s(self):
        self.make('devworld')
        status, data = self.call('POST', '/api/atlas/live/edit', {'clientId': 'c1', 'editor': 'Someone Else', 'label': 'a wall',
                                                                  'ops': [{'key': 'tile:1,1', 'before': None, 'after': '#'}]}, role='builder')
        self.assertEqual(status, 200, data)
        self.assertEqual(self.atlas_world.edits[0]['editor'], 'pell', 'the session\'s account, whatever the page said')
        self.assertEqual(self.call('POST', '/api/atlas/live/edit', {'clientId': 'c1', 'ops': []}, role='dm')[0], 403)

    def test_a_live_edit_ahead_of_the_map_is_refused(self):
        self.make('devworld')
        pub = self.hosted.publisher = StandInPublisher()
        act = {'kind': 'npc.move', 'characterId': 'mara', 'cell': 'new_inn'}
        self.assertEqual(self.call('POST', '/api/dm/actions', act)[0], 200)
        pub.place = 'new_inn'
        status, data = self.call('POST', '/api/dm/actions', act)
        self.assertEqual(status, 409)
        self.assertIn('Publish the map first: new_inn', data['error'])
        self.assertEqual(len([c for c in self.dm.calls if c[0] == 'queue' or 'npc.move' in str(c)]), 1, 'the refused act reached nothing')

    def test_no_publish_without_a_backup(self):
        self.assertEqual(self.hosted._backup_first(wait=0), ST.NO_BACKUP_NO_PUBLISH, 'an old compose.yaml: no backup service')
        now = int(time.time())
        state = {'v': 1, 'at': now, 'backups': [], 'disk': {}}
        (self.folder / 'backups.json').write_text(json.dumps(state))
        self.assertIn('in time', self.hosted._backup_first(wait=0))
        self.assertEqual([p.name.split('.')[0] for p in (self.folder / 'requests').iterdir()], ['backup-before-publish'])

        def made(_seconds):
            stamp = time.strftime('%Y%m%dT%H%M%SZ', time.gmtime(now))
            (self.folder / 'backups.json').write_text(json.dumps({**state, 'backups': [
                {'name': f'ratw-{stamp}-before-publish', 'at': now + 1, 'size': 9}]}))
        self.assertIsNone(self.hosted._backup_first(wait=30, sleep=made), 'the dump is there: Publish goes on')

        def full(_seconds):
            (self.folder / 'backups.json').write_text(json.dumps({**state, 'problem': {'code': 'disk', 'at': now + 1, 'free': 5}}))
        self.assertIn('nearly full', self.hosted._backup_first(wait=30, sleep=full))


class HostedPrivacyTests(HostedBase):
    """Doc 78, 9 (Q8, Q10): accounts only as short pairwise ids; reports filtered, the reporter hidden and none about the
    host's own wolves; NPC conversations without the players' words; the settings view an allowlist."""
    HOST = 'acct-' + 'b' * 32
    PLAYER = 'acct-' + '1234abcd' + 'c' * 24

    def test_accounts_are_short_pairwise_ids_everywhere(self):
        value = {'account': self.PLAYER, 'list': [f'the account {self.PLAYER}'], self.PLAYER: {'name': 'Ash'}, 'n': 3,
                 'user': 'dm-master'}
        self.assertEqual(ST.short_accounts(value), {'account': 'acct-1234abcd', 'list': ['the account acct-1234abcd'],
                                                    'acct-1234abcd': {'name': 'Ash'}, 'n': 3, 'user': 'dm-master'})
        self.dm.players = lambda target: {'characters': [{'name': 'Ash', 'account': {'name': self.PLAYER}}],
                                          'holders': {self.PLAYER: 5}}
        status, data = self.call('GET', '/api/dm/players', role='viewer')
        self.assertEqual(status, 200)
        self.assertNotIn(self.PLAYER, json.dumps(data))
        self.assertEqual(data['characters'][0]['account']['name'], 'acct-1234abcd')
        # The Prosperity tab's player money (doc 79, 9.3) names holders by account: short there too.
        self.dm.prosperity = lambda target: {'players': {'holders': [{'holder': self.PLAYER, 'name': f'the account {self.PLAYER[5:]}'}]}}
        text = json.dumps(self.call('GET', '/api/dm/prosperity', role='viewer')[1])
        self.assertNotIn(self.PLAYER, text)
        self.assertIn('acct-1234abcd', text)

    def test_reports_hide_the_reporter_and_never_show_the_host_s_own_wolves(self):
        (self.folder / 'central.json').write_text(json.dumps({'kind': 'community', 'host': self.HOST}))
        row = lambda i, reported, char: {'id': i, 'reporter': self.PLAYER, 'reporterName': 'Fern', 'reported': reported,
                                        'reportedCharacter': char, 'reportedName': char, 'evidence': [{'text': 'line'}],
                                        'earlier': [{'id': 'r2'}, {'id': 'r9'}]}
        self.dm.report_rows = [row('r1', 'acct-' + 'd' * 32, 'wolf-d'), row('r2', self.HOST, 'wolf-host')]
        self.assertEqual(self.call('GET', '/api/dm/reports', role='viewer')[0], 403, 'players\' words: dm and up')
        status, data = self.call('GET', '/api/dm/reports')
        self.assertEqual(status, 200, data)
        self.assertEqual([r['id'] for r in data['reports']], ['r1'], 'never about the host\'s own wolves')
        report = data['reports'][0]
        self.assertEqual((report['reporter'], report['reporterName'], report['reporterHidden'], report['evidenceFrom']),
                         ('', '', True, 'world'))
        self.assertEqual(report['earlier'], [{'id': 'r9'}])
        self.assertEqual([a['target'] for a in data['actions']], ['wolf-d'])
        self.assertNotIn('Fern', json.dumps(data))
        self.assertNotIn(self.HOST, json.dumps(data))
        # Deciding one about the host's wolf is refused as if it weren't there.
        self.assertEqual(self.call('POST', '/api/dm/reports/decide', {'id': 'r2', 'decision': 'dismiss'})[0], 404)
        self.assertEqual(self.call('POST', '/api/dm/reports/decide', {'id': 'r1', 'decision': 'dismiss'})[0], 200)

    def test_npc_conversations_carry_the_npc_s_side_only(self):
        life = {'name': 'Bram', 'mind': {'memories': [{'npc': 'bram', 'text': 'Ash asked about iron.'}],
                                         'conversations': [{'npc': 'bram', 'subject': 'player-ash', 'turns': [
                                             {'who': 'player-ash', 'text': 'my password is hunter2'},
                                             {'who': 'bram', 'text': 'Iron is dear this week.'}]}]}}
        self.dm.chronicle = lambda target, subject: copy.deepcopy(life)
        status, data = self.call('GET', '/api/dm/chronicle', role='viewer', query='id=bram')
        self.assertEqual(status, 200)
        talk = data['mind']['conversations'][0]
        self.assertEqual((talk['turns'], talk['npcSideOnly']), ([{'who': 'bram', 'text': 'Iron is dear this week.'}], True))
        self.assertNotIn('hunter2', json.dumps(data))
        self.assertEqual(data['mind']['memories'], life['mind']['memories'], 'the NPC\'s own summaries stay')

    def test_core_s_word_for_word_memories_never_reach_the_editor(self):
        # R1 security, 1: without an AI key Core's "Conversation record." memories are never rewritten, and they hold
        # the players' typed words. Only the Mind's "Summary." ones reach an Editor account.
        life = {'name': 'Bram', 'mind': {'memories': [
            {'npc': 'bram', 'subject': 'player-ada', 'text': 'Conversation record. Ada said: \u201cmy pin is 4471\u201d '},
            {'npc': 'bram', 'subject': 'player-ada', 'text': '  conversation RECORD. Ada said: \u201cmeet at the mill\u201d'},
            {'npc': 'bram', 'subject': 'player-ada', 'text': 'Summary. Ada asked about the mill.'}]}}
        self.dm.chronicle = lambda target, subject: copy.deepcopy(life)
        status, data = self.call('GET', '/api/dm/chronicle', role='viewer', query='id=player-ada')
        self.assertEqual(status, 200)
        self.assertNotIn('4471', json.dumps(data))
        self.assertNotIn('meet at the mill', json.dumps(data))
        self.assertEqual([m['text'] for m in data['mind']['memories']], ['Summary. Ada asked about the mill.'])
        self.assertEqual(data['mind']['recordsHidden'], 2)

    def test_a_storyteller_named_short_is_made_whole_for_the_decision(self):
        self.dm.standing = [{'account': self.PLAYER, 'state': 'applied'}, {'account': 'acct-' + 'e' * 32, 'state': 'applied'}]
        status, _ = self.call('POST', '/api/dm/live/action', {'kind': 'storyteller.decide', 'id': 'acct-1234abcd',
                                                               'payload': {'approve': True}})
        self.assertEqual(status, 200)
        self.assertEqual(self.dm.calls[-1][0], 'live_action')
        self.assertEqual(self.dm.calls[-1][1][3], self.PLAYER)
        self.assertEqual(self.call('POST', '/api/dm/live/action', {'kind': 'storyteller.decide', 'id': 'acct-99999999'})[0], 404)

    def test_the_settings_view_is_an_allowlist(self):
        self.settings(cheats='everyone', player_limit=40, workers=3, host_token='rh_' + 'x' * 40, ai_key='sk-secret',
                      editor={'enabled': True, 'owner': 'host-k7q2'}, ai={'key': 'sk-secret', 'monthly_tokens': 5})
        shown = ST.Hosted('world', self.folder).shown_settings()
        self.assertEqual(shown, {'player_limit': 40, 'workers': 3, 'cheats': 'everyone', 'editor': {'enabled': True}})
        self.settings(world={'cheats': 'nonsense', 'player_limit': 'many'})
        self.assertEqual(ST.Hosted('world', self.folder).shown_settings(),
                         {'player_limit': None, 'workers': None, 'cheats': 'off', 'editor': {'enabled': True}})

    def test_log_lines_for_the_owner_lose_addresses_and_tokens(self):
        line = ('2026-11-12 10:00:01 player 203.0.113.9:51234 and 2001:db8::7 with rh_' + 'k' * 30 + ' and Bearer '
                + 'A' * 43 + ' crashed\x07')
        clean = ST.scrub(line)
        for gone in ('203.0.113.9', '2001:db8::7', 'rh_kk', 'AAAA', '\x07'):
            self.assertNotIn(gone, clean)
        self.assertIn('10:00:01', clean, 'times are kept')
        (self.folder / 'exits.json').write_text(json.dumps({'exits': [{'at': 1, 'code': 139, 'lines': [line, 7]}]}))
        self.assertEqual(ST.Hosted('world', self.folder).exits(), [{'at': 1, 'code': 139, 'lines': [clean]}])


    def test_the_owner_s_folder_whole_this_computer_and_the_server_s_state(self):
        """R1: the owner's own folder path is never scrubbed (M-d); a sign-in from the world's own computer reads "from
        this computer"; the Server tab's `hostServer` is server.json, checked (H2)."""
        long_path = '/home/pell/Documents/' + 'worlds-of-the-long-valley-and-its-wolves-2026' + '/ashfall'
        self.settings(folder=long_path + '\x07')
        hosted = ST.Hosted('world', self.folder)
        self.assertEqual(hosted.folder_path(), long_path)
        local = {'172.20.0.1', '127.0.0.1'}
        self.assertEqual(hosted.this_computer('Signed in from 172.20.0.1.', local), 'Signed in from this computer.')
        self.assertEqual(hosted.this_computer('Signed in from 203.0.113.9 with an Editor code.', local),
                         'Signed in from 203.0.113.9 with an Editor code.')
        self.assertIsNone(hosted.server())
        (self.folder / 'server.json').write_text(json.dumps({'up': False, 'reason': 'world.toml', 'code': None,
                                                              'failures': 0, 'retryAt': None, 'at': 1}))
        self.assertEqual(hosted.server(), {'up': False, 'reason': 'world.toml', 'code': None, 'failures': 0, 'retryIn': None})


class SqlTextTests(unittest.TestCase):
    """I14 (doc 78, 6.1 and 18): no request value ever becomes SQL text or an identifier. Every f-string, concatenation
    or += that builds SQL in the hosted studio's files and the modules they write through (doc 78's R1 review: the test
    once read three files, upper-case keywords only, and missed += and fragments with no keyword) takes its parts only
    from these names, each fixed in code. A new one fails here until it is read and named."""
    FILES = ('studio.py', 'dungeon_master.py', 'map_editor.py', 'world_admin.py', 'live_edit.py', 'world_store.py',
             'world_sync.py', 'world_db.py', 'chronicle.py')
    TABLES = 'a table name from a fixed tuple or dict in code (S.TABLES, GLOBAL_TABLES, literal loops)'
    COLUMNS = "column names from that table's fixed spec (TABLES / shape(): the database's own catalogue)"
    MARKS = "'%s' placeholders, as many as the fixed columns"
    FIXED = {('dungeon_master.py', 'order'): 'a constant ORDER BY clause written just above it',
             ('dungeon_master.py', 'table'): "chapters(): the rows lambda's table, called only with literal names",
             **{('world_admin.py', k): 'a module constant (minutes or hours)' for k in ('IDLE_MINUTES', 'SESSION_HOURS', 'STEP_UP_MINUTES')},
             ('live_edit.py', 'table'): TABLES, ('live_edit.py', 'locked'): "' FOR UPDATE' or ''",
             ('live_edit.py', "' FOR UPDATE' if lock else ''"): 'a literal', ('live_edit.py', 'PRESENCE_SECONDS'): 'a module constant',
             ('live_edit.py', "', '.join(columns)"): COLUMNS, ('live_edit.py', "', '.join(keys)"): COLUMNS,
             ('live_edit.py', "', '.join(['%s'] * len(columns))"): MARKS,
             ('live_edit.py', "', '.join((f'{c} = excluded.{c}' for c in rest))"): COLUMNS,
             ('world_store.py', 'table'): TABLES, ('world_store.py', 'k'): COLUMNS, ('world_store.py', 'update'): COLUMNS,
             ('world_store.py', "', '.join(columns)"): COLUMNS, ('world_store.py', "', '.join(keys)"): COLUMNS,
             ('world_store.py', "', '.join(['%s'] * len(columns))"): MARKS,
             ('world_store.py', "', '.join((f'{c} = excluded.{c}' for c in rest))"): COLUMNS,
             ('world_store.py', "' AND '.join((f'{k} = %s' for k in keys))"): COLUMNS,
             ('world_store.py', "' AND '.join(where)"): 'literal conditions with %s placeholders (changes_since)',
             ('world_store.py', "' FOR UPDATE' if table in lock else ''"): 'a literal',
             ('world_store.py', "f'DO UPDATE SET {update}' if update else 'DO NOTHING'"): COLUMNS,
             ('world_sync.py', 'table'): TABLES, ('world_sync.py', 'k'): COLUMNS, ('world_sync.py', 'column'): COLUMNS,
             ('world_sync.py', "', '.join(columns)"): COLUMNS, ('world_sync.py', "', '.join(keys)"): COLUMNS,
             ('world_sync.py', "', '.join(rest)"): COLUMNS, ('world_sync.py', "', '.join(['%s'] * len(columns))"): MARKS,
             ('world_sync.py', "' AND '.join((f'{k} = %s' for k in keys))"): COLUMNS,
             ('world_sync.py', 'f"UPDATE SET {\', \'.join(rest)}" if rest else \'NOTHING\''): COLUMNS,
             ('world_sync.py', "' AND ' if column else ' WHERE '"): 'literals', ('world_sync.py', 'sql'): 'read(): built here from the above',
             ('world_sync.py', 'where'): "read(): ONLY's literal conditions, the only caller's", ('world_sync.py', 'LOCK_MINUTES'): 'a module constant',
             ('world_sync.py', 'place'): 'PLACED, a module tuple of column stems', ('world_sync.py', "gone.format('area')"): 'a literal',
             ('world_sync.py', "gone.format(f'{place}_area')"): 'PLACED',
             ('world_db.py', 'table'): TABLES, ('world_db.py', 'column'): 'literal column names in the loops above it',
             ('world_db.py', 'query'): '_rows(): every caller passes literal SQL', ('world_db.py', 'on.format(column)'): 'the same',
             **{('world_db.py', f"on.format('{c}')"): 'a literal' for c in ('area', 'area_id', 'wander_area')}}
    # SQL text: a whole statement in any case, or the upper-case words and fragments this code writes SQL with.
    SQL = re.compile(r'(?i:\bselect\b[^;]*\bfrom\b|\binsert\s+into\b|\bdelete\s+from\b|\bupdate\s+[a-z_.]+\s+(?:[a-z_]+\s+)?set\b'
                     r'|\bon\s+conflict\b|%s)'
                     r'|\b(?:WHERE|ORDER BY|GROUP BY|VALUES|JOIN|LIMIT|RETURNING|UPDATE SET|DO UPDATE|DO NOTHING|FOR UPDATE)\b'
                     r"|\bFROM [a-z_]+\.|\binterval '|^\s*(?:AND|OR)\b|\b(?:AND|OR)\s*$")

    def sql(self, node) -> bool:
        if isinstance(node, ast.Constant):
            return isinstance(node.value, str) and bool(self.SQL.search(node.value))
        if isinstance(node, ast.JoinedStr):
            return self.sql(ast.Constant(''.join(v.value for v in node.values if isinstance(v, ast.Constant) and isinstance(v.value, str))))
        if isinstance(node, ast.IfExp):
            return self.sql(node.body) or self.sql(node.orelse)
        return False

    def test_sql_is_built_only_from_fixed_names(self):
        here = Path(__file__).resolve().parent
        seen = set()
        for name in self.FILES:
            source = (here / name).read_text(encoding='utf-8')
            for node in ast.walk(ast.parse(source)):
                parts = []
                if isinstance(node, ast.JoinedStr) and self.sql(node):
                    parts = [p.value for p in node.values if isinstance(p, ast.FormattedValue)]
                elif isinstance(node, (ast.BinOp, ast.AugAssign)) and isinstance(node.op, (ast.Add, ast.Mod)):
                    sides = [node.left, node.right] if isinstance(node, ast.BinOp) else [node.target, node.value]
                    if any(self.sql(s) for s in sides):
                        parts = [s for s in sides if not isinstance(s, (ast.Constant, ast.BinOp, ast.JoinedStr))]
                elif isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr == 'format':
                    if self.sql(node.func.value) and isinstance(node.func.value, ast.Constant):
                        self.fail(f'{name}:{node.lineno}: SQL built with str.format')
                for part in parts:
                    expr = ast.unparse(part)
                    self.assertIn((name, expr), self.FIXED, f'{name}:{node.lineno}: {{{expr}}} in SQL text')
                    seen.add((name, expr))
        self.assertEqual(set(self.FIXED) - seen, set(), 'names no longer used: take them off FIXED')
        chapters = (here / 'dungeon_master.py').read_text(encoding='utf-8')
        calls = re.findall(r'\brows\(([^)]*)\)', chapters[chapters.index('    def chapters('):chapters.index('    def decide(')])
        self.assertTrue(calls and all(re.fullmatch(r"'[a-z_]+'", c.split(',')[0].strip()) for c in calls), calls)
        # (world_sync.read's `where`: its callers pass ONLY's literals, nothing else.)
        for caller in ('world_sync.py', 'world_package.py'):
            text = (here / caller).read_text(encoding='utf-8')
            self.assertEqual(sorted(set(re.findall(r"\bwhere=('{2}|[A-Za-z_.]+\()", text))),
                             ["''", 'ONLY.get('] if caller == 'world_sync.py' else ['world_sync.ONLY.get('])

    def test_the_scan_sees_what_it_once_missed(self):
        # The R1 review's blind spots, each caught.
        for code in ("q = f'select * from t where id = {x}'", "q = 'SELECT 1 FROM t WHERE a = 1'\nq += ' AND b = ' + x",
                     "q = f' AND {x}'", "q = f'ORDER BY {col}'", "q = 'SELECT 1 FROM t ' + order"):
            found = []
            for node in ast.walk(ast.parse(code)):
                if isinstance(node, ast.JoinedStr) and self.sql(node):
                    found.append(node)
                elif isinstance(node, (ast.BinOp, ast.AugAssign)) and isinstance(node.op, ast.Add):
                    sides = [node.left, node.right] if isinstance(node, ast.BinOp) else [node.target, node.value]
                    if any(self.sql(s) for s in sides) and any(not isinstance(s, (ast.Constant, ast.BinOp, ast.JoinedStr)) for s in sides):
                        found.append(node)
            self.assertTrue(found, code)


if __name__ == '__main__':
    unittest.main()
