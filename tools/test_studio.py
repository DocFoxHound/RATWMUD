#!/usr/bin/env python3
"""The studio (tools/studio.py, doc 66 sections 3 and 4): one host, one sign-in, two suites kept apart; Atlas's Update
from live and the DM's server, maintenance, live switch, Import from dev and releases.

Offline with a stand-in DM (accounts by token, every other method recorded) and a stand-in sync engine; two classes
work for real on scratch databases (dropped afterwards; skipped without the local PostgreSQL): a sign-in, and pull,
import, rollback and the live switch on throwaway worlds (test_world_sync's). DEV and PROD are never touched."""
import http.client
import json
import os
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
        self.calls, self.fail = [], None

    def __getattr__(self, name):
        def method(*args):
            self.calls.append((name, args))
            if self.fail is not None:
                raise self.fail
            if name == 'status':
                return {'key': args[0], 'server': {'state': 'running', 'by': '', 'at': None, 'note': ''}, 'serverRunning': False,
                        'release': 4}
            return {'called': name, 'release': 5, 'applied': {'world': {}, 'live': {'NPCs': {'added': 1, 'changed': 2, 'removed': 0}}},
                    'conflicts': 0}
        return method


class Running:
    """A studio on a free port; Atlas without a database (a roster file of its own)."""

    def __init__(self, dm, atlas_key, sync=None, economy=None):
        self.temp = tempfile.TemporaryDirectory()
        roster = Path(self.temp.name) / 'roster.json'
        map_editor.roster_lib.save({**map_editor.roster_lib.load(), 'characters': []}, roster)
        self.made, self.chosen, self.dm = [], [], dm

        def atlas(key):
            self.made.append(key)
            return map_editor.make_handler(roster_path=roster, ai_config_path=Path(self.temp.name) / 'missing.json')
        self.studio = ST.Studio(dm=dm, atlas=atlas, atlas_key=atlas_key, choose=self.chosen.append, sync=sync, economy=economy)
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
    """A studio with the stand-in DM, made afresh for each test, Atlas on Ridgemere 2.0 (in name only)."""

    def setUp(self):
        self.host = Running(StandInDM(), 'ridgemere2')
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
        self.assertEqual(data['atlas']['world']['key'], 'ridgemere2')
        self.assertEqual([w['key'] for w in data['atlas']['worlds']], W.keys('dev'))
        self.assertEqual([w['key'] for w in data['dm']['worlds']], W.keys('live'))
        self.assertEqual(data['dm']['live'], W.live_world())
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
            self.assertEqual(self.call('POST', '/api/atlas/layers/' + path, {'target': 'ridgemere2'}, token='admin-token')[0],
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
        self.assertEqual(self.dm.calls[-1][1][1], W.live_world(), 'the live world by default')
        for path in ('players', 'storytellers', 'chapters', 'calendar', 'npcs', 'world'):
            self.assertEqual(self.call('GET', '/api/dm/' + path, token='viewer-token')[0], 200, path)

    def test_the_dm_suite_works_on_live_worlds(self):
        status, data = self.call('GET', '/api/dm/players?target=ridgemere2', token='admin-token')
        self.assertEqual(status, 400, data)
        self.assertIn('live', data['error'])
        status, data = self.call('GET', f'/api/dm/players?target={W.live_world()}', token='admin-token')
        self.assertEqual((status, data['target']), (200, W.live_world()))

    def test_atlas_npc_pages_work_on_atlas_world_only(self):
        status, data = self.call('GET', '/api/atlas/layers/npcs', token='dm-token')
        self.assertEqual((status, data), (200, {'called': 'npcs', 'target': 'ridgemere2'}))
        status, data = self.call('GET', f'/api/atlas/layers/npcs?target={W.live_world()}', token='dm-token')
        self.assertEqual(status, 409, data)
        route = {'id': 'north_walk', 'name': 'North walk', 'posts': []}
        status, data = self.call('POST', '/api/atlas/layers/routes/save', {'route': route}, token='dm-token')
        self.assertEqual(status, 200, data)
        name, args = self.dm.calls[-1]
        self.assertEqual((name, args[1], args[2]), ('save_route', 'ridgemere2', route))
        self.assertEqual(args[0]['username'], 'dm-user', 'audited as the signed-in account')
        status, _ = self.call('POST', '/api/atlas/layers/factions/save', {'target': W.live_world(), 'faction': {}}, token='dm-token')
        self.assertEqual(status, 409, 'never a live world from Atlas')
        self.assertEqual(self.call('GET', '/api/atlas/layers/actions/7', token='dm-token')[0], 200)

    def test_switching_atlas_world(self):
        self.assertEqual(self.call('POST', '/api/studio/atlas-world', {'key': 'dev'}, token='viewer-token')[0], 403)
        status, data = self.call('POST', '/api/studio/atlas-world', {'key': W.live_world()}, token='dm-token')
        self.assertEqual(status, 400, data)
        self.assertEqual(self.chosen, [])
        old = self.atlas_token()
        status, data = self.call('POST', '/api/studio/atlas-world', {'key': 'dev'}, token='dm-token')
        self.assertEqual((status, data['world']['key']), (200, 'dev'), data)
        self.assertEqual((self.chosen, self.made), (['dev'], ['ridgemere2', 'dev']))
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


class SyncRouteTests(unittest.TestCase):
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
        self.sync = StandInSync()
        self.host = Running(StandInDM(), 'dev', self.sync)
        self.dm, self.call = self.host.dm, self.host.call

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
        # A side project has nothing to update from.
        self.assertEqual(self.call('POST', '/api/studio/atlas-world', {'key': 'ridgemere2'}, token='dm-token')[0], 200)
        status, data = self.call('GET', '/api/atlas/sync/diff', token='dm-token')
        self.assertEqual(status, 409, data)
        self.assertIn('no upstream', data['error'])

    def test_server_status_and_maintenance(self):
        status, data = self.call('GET', '/api/dm/server/status', token='viewer-token')
        self.assertEqual(status, 200, data)
        self.assertEqual((data['key'], data['live']), ('prod', 'prod'))
        self.assertIn('rarely wise', data['warning'])
        self.assertEqual(self.call('GET', '/api/dm/server/status?world=other', token='viewer-token')[1]['key'], 'other')
        self.assertEqual(self.call('GET', '/api/dm/server/status?world=dev', token='viewer-token')[0], 400, 'live worlds only')
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
                          ({'world': 'dev', 'confirm': 'dev'}, 'a dev world'), ({'world': 'prod', 'confirm': 'prod'}, 'already live')):
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
        self.assertEqual(self.call('GET', '/api/dm/import/plan?dev=ridgemere2', token='viewer-token')[0], 400, 'not a copy of prod')
        self.sync.fail = WS.PublishError('dev has no base yet: take one.')
        status, data = self.call('GET', '/api/dm/import/plan', token='viewer-token')
        self.assertEqual((status, data['plan'], data['problem']), (200, None, 'dev has no base yet: take one.'), 'said, with the worlds')

    def test_import_and_the_engine_s_errors(self):
        body = {'dev': 'dev', 'choices': {'live.npcs/wren': 'dev'}, 'password': 'secret', 'note': 'Houses', 'expect': 'd1'}
        self.assertEqual(self.call('POST', '/api/dm/import', body, token='viewer-token')[0], 403)
        self.assertEqual(self.call('POST', '/api/dm/import', {**body, 'password': ''}, token='dm-token')[0], 400)
        self.assertEqual(self.call('POST', '/api/dm/import', {**body, 'dev': 'ridgemere2'}, token='dm-token')[0], 400)
        self.assertEqual(self.sync.calls, [])
        status, data = self.call('POST', '/api/dm/import', body, token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertEqual(self.sync.calls[-1], ('import_', ('dev', {'live.npcs/wren': 'dev'}, 'secret', 'dm-user', 'Houses', 'd1')))
        self.assertEqual(self.logged()[-1][1:4], ('prod', 'world.import', 'dev'))
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


class EconomyRouteTests(unittest.TestCase):
    """Doc 67, 6: the Economy controls' routes with a stand-in: reads for anyone signed in, writes for dm and admin with
    Atlas's session token, each audited in Atlas's world."""

    def setUp(self):
        self.economy = StandInEconomy()
        self.host = Running(StandInDM(), 'ridgemere2', economy=self.economy)
        self.dm, self.call = self.host.dm, self.host.call

    def tearDown(self):
        self.host.stop()

    def test_who_may_read_and_change_them(self):
        self.assertEqual(self.call('GET', '/api/atlas/economy')[0], 401)
        status, data = self.call('GET', '/api/atlas/economy', token='viewer-token')
        self.assertEqual(status, 200, data)
        self.assertEqual((data['world'], data['worldInfo']['key']), ('w', 'ridgemere2'))
        body = {'storeMarkup': 50}
        self.assertEqual(self.call('POST', '/api/atlas/economy/store', body, token='viewer-token')[0], 403, 'a viewer reads only')
        self.assertEqual(self.call('POST', '/api/atlas/economy/store', body, token='dm-token')[0], 403, 'Atlas\'s token too')
        self.assertFalse([c for c in self.economy.calls if c[0] != 'read'], 'nothing reached the controls')
        editor = {'X-RATW-Editor': self.host.atlas_token()}
        status, data = self.call('POST', '/api/atlas/economy/store', body, token='dm-token', headers=editor)
        self.assertEqual(status, 200, data)
        self.assertIn(('set_store', 'ridgemere2', body, 'dm-user'), self.economy.calls)
        status, data = self.call('POST', '/api/atlas/economy/businesses', {'set': {'rm_smith': False}}, token='admin-token', headers=editor)
        self.assertEqual(status, 200, data)
        bulk = {'all': False, 'business': 'smithy', 'district': 'works'}
        self.assertEqual(self.call('POST', '/api/atlas/economy/businesses', bulk, token='dm-token', headers=editor)[0], 200)
        self.assertIn(('set_businesses', 'ridgemere2', bulk, 'dm-user'), self.economy.calls, 'the bulk filters reach the controls')
        logged = [args for name, args in self.dm.calls if name == 'log']
        self.assertEqual([(who['username'], world, action, target) for who, world, action, target, _ in logged],
                         [('dm-user', 'ridgemere2', 'economy.store', 'store room'), ('admin-user', 'ridgemere2', 'economy.businesses', 'rm_smith'),
                          ('dm-user', 'ridgemere2', 'economy.businesses', 'businesses')])
        # Doc 68, 9: the loop's levers in Atlas, and the DM's view of a live world's (dm and admin change it, no Atlas token).
        status, data = self.call('POST', '/api/atlas/economy/loop', {'gateToll': 30}, token='dm-token', headers=editor)
        self.assertEqual(status, 200, data)
        self.assertIn(('set_loop', 'ridgemere2', {'gateToll': 30}, 'dm-user'), self.economy.calls)
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
        self.assertEqual(self.call('POST', '/api/dm/economy/loop', {'world': 'ridgemere2', 'gateToll': 1}, token='dm-token')[0], 400,
                         'the DM works on live worlds')

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
        self.assertEqual(data['loop'], {k: v for k, v in ST.map_editor.LOOP_DEFAULTS.items() if k != 'countryOff'})
        self.assertTrue(data['basket'] and all({'item', 'share', 'percent', 'makers'} <= set(b) for b in data['basket']))
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
        self.assertEqual(data['countryOff'], ['iron_bar'])
        lines = self.one(self.dev, "SELECT live.people_manifest('greyfen')").split('\n')
        self.assertIn('loop needs on country on reserve 200 toll 20 carriage 6 wages 14 9 8 8', lines)
        self.assertIn('countryoff "iron_bar"', lines)
        self.assertIn('subsidy "carriage" 50', lines)
        for bad in ({'gateToll': 95}, {'subsidies': {'roads': 5}}, {'treasury': 1}):
            self.assertEqual(self.call('POST', '/api/atlas/economy/loop', bad, token='dm-token', headers=editor)[0], 400, bad)
        self.assertEqual(self.call('POST', '/api/atlas/economy/goods', {'set': {'nothing_at_all': False}}, token='dm-token',
                                   headers=editor)[0], 400)
        status, data = self.call('POST', '/api/atlas/economy/goods', {'all': True}, token='dm-token', headers=editor)
        self.assertEqual(data['countryOff'], [])
        # The DM, on the live world: the treasury's levers.
        status, data = self.call('POST', '/api/dm/economy/loop', {'world': self.live, 'subsidies': {'carriage': 10}}, token='dm-token')
        self.assertEqual(status, 200, data)
        self.assertIn('subsidy "carriage" 10', self.one(self.live, "SELECT live.people_manifest('greyfen')").split('\n'))
        self.assertIn('economy.loop', [r[0] for r in self.sql(self.live, 'SELECT action FROM dm.audit')])

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


if __name__ == '__main__':
    unittest.main()
