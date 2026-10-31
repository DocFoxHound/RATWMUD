#!/usr/bin/env python3
"""The studio (tools/studio.py, doc 66 section 3): one host, one sign-in, two suites kept apart.

Offline with a stand-in DM (accounts by token, every other method recorded); one class signs in for real on scratch
databases (dropped afterwards; skipped without the local PostgreSQL)."""
import http.client
import json
from pathlib import Path
import tempfile
import threading
import unittest

import dungeon_master as D
import map_editor
import studio as ST
import world_db as W
from test_dungeon_master import Fixture
from test_world_store import greyfen

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


class Running:
    """A studio on a free port; Atlas without a database (a roster file of its own)."""

    def __init__(self, dm, atlas_key):
        self.temp = tempfile.TemporaryDirectory()
        roster = Path(self.temp.name) / 'roster.json'
        map_editor.roster_lib.save({**map_editor.roster_lib.load(), 'characters': []}, roster)
        self.made, self.chosen, self.dm = [], [], dm

        def atlas(key):
            self.made.append(key)
            return map_editor.make_handler(roster_path=roster, ai_config_path=Path(self.temp.name) / 'missing.json')
        self.studio = ST.Studio(dm=dm, atlas=atlas, atlas_key=atlas_key, choose=self.chosen.append)
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


if __name__ == '__main__':
    unittest.main()
