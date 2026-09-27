#!/usr/bin/env python3
"""Dungeon Master service: sign-in, roles, the players sheet, and queued live actions.
Scratch PROD and DEV databases, dropped afterwards; skipped without the local PostgreSQL."""
import json
import secrets
import unittest

import dungeon_master as D
import world_db as W
import world_store as S
from test_world_db import database_available, superuser
from test_world_store import greyfen


def save_with_player(conn, dead=False):
    town = next(c for c in greyfen()['cells'])
    payload = {'schema': 1, 'players': [{'id': 'player-ada', 'name': 'Ada', 'cell': town['id'], 'x': 10.5, 'y': 12.5, 'age': 21,
                                         'strength': 55, 'dexterity': 61, 'wisdom': 33, 'stamina': 90, 'sneakSkill': 12,
                                         'hearingSkill': 4, 'scentSkill': 7, 'hearing': 1, 'vision': 1, 'smell': 1,
                                         'posture': 'lying' if dead else 'standing', 'dead': dead}]}
    conn.execute('SELECT game.save_checkpoint(%s, 1, %s)', ('greyfen', json.dumps(payload)))


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class Fixture(unittest.TestCase):
    """Scratch PROD and DEV with Greyfen and the DM accounts; no tests of its own."""

    @classmethod
    def setUpClass(cls):
        W.ensure_roles()
        tag = secrets.token_hex(4)
        cls.names = {'prod': f'ratw_test_dmp_{tag}', 'dev': f'ratw_test_dmd_{tag}'}
        with superuser() as su:
            for name in cls.names.values():
                su.execute(f'CREATE DATABASE {name} OWNER ratw_owner')
                su.execute(f'GRANT CONNECT ON DATABASE {name} TO ratw_editor, ratw_publisher, ratw_game, ratw_dm')
        for target, name in cls.names.items():
            with W.connect(target, 'owner', dbname=name) as owner:
                W.migrate(owner)
            with W.connect(target, 'editor', dbname=name) as editor:
                S.save_roster(editor, S.load_roster(editor))
                S.save_world(editor, greyfen(), None, create=True)
        cls.now = [1_800_000_000.0]
        cls.dm = D.DungeonMaster(lambda target: W.connect(target, 'dm', dbname=cls.names[target]), clock=lambda: cls.now[0])

    @classmethod
    def tearDownClass(cls):
        with superuser() as su:
            for name in cls.names.values():
                su.execute(f'DROP DATABASE {name} WITH (FORCE)')

    def setUp(self):
        for target, name in self.names.items():
            with W.connect(target, 'owner', dbname=name) as owner:
                owner.execute('TRUNCATE dm.admins, dm.sessions, dm.actions, dm.audit CASCADE')
            with W.connect(target, 'game', dbname=name) as game:
                save_with_player(game)
        with W.connect('prod', 'dm', dbname=self.names['prod']) as conn:
            for username, role in (('dm-admin', 'admin'), ('dm-master', 'dm'), ('dm-viewer', 'viewer')):
                D.create_account(conn, username, role, f'{username}-password')

    def sign_in(self, username):
        return self.dm.session(self.dm.login(username, f'{username}-password')['token'])


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class DungeonMasterTests(Fixture):

    def test_sign_in_and_sessions(self):
        result = self.dm.login('dm-master', 'dm-master-password')
        self.assertEqual((result['username'], result['role']), ('dm-master', 'dm'))
        self.assertEqual(self.dm.session(result['token'])['role'], 'dm')
        self.dm.logout(result['token'])
        with self.assertRaises(D.DMError) as raised:
            self.dm.session(result['token'])
        self.assertEqual(raised.exception.status, 401)

    def test_wrong_passwords_lock_the_account_for_a_while(self):
        for _ in range(D.MAX_FAILURES):
            with self.assertRaises(D.DMError):
                self.dm.login('dm-master', 'guess')
        with self.assertRaises(D.DMError) as raised:
            self.dm.login('dm-master', 'dm-master-password')
        self.assertEqual(raised.exception.status, 429)
        self.now[0] += D.LOCK_MINUTES * 60 + 1
        self.assertEqual(self.dm.login('dm-master', 'dm-master-password')['role'], 'dm')

    def test_passwords_are_only_stored_as_hashes(self):
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            stored = json.dumps([r[0] for r in owner.execute('SELECT password FROM dm.admins').fetchall()])
        self.assertNotIn('password', stored.replace('"hash"', '').replace('dm-', ''))
        self.assertNotIn('dm-master-password', stored)

    def test_the_players_sheet(self):
        sheet = self.dm.players('prod')
        ada = sheet['characters'][0]
        self.assertEqual((ada['name'], ada['dead'], ada['stats']['dexterity'], ada['skills']['sneakSkill']), ('Ada', False, 61, 12))
        self.assertEqual((ada['worldX'], ada['worldY']), (10.5, 12.5), 'world-map position of a character in a world cell')
        self.assertEqual(sheet['world']['name'], 'Greyfen Crossing')

    def test_kill_and_resurrect_are_queued_for_the_game_server_and_audited(self):
        master = self.sign_in('dm-master')
        queued = self.dm.request(master, 'prod', 'character.kill', 'player-ada', 'Testing')
        self.assertEqual(self.dm.action('prod', queued['id'])['status'], 'queued')
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            row = game.execute('SELECT kind, target_id, requested_by FROM dm.actions').fetchone()
            self.assertEqual(row, ('character.kill', 'player-ada', 'dm-master'), 'the game server can read its queue')
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            audit = owner.execute("SELECT who, detail FROM dm.audit WHERE action = 'character.kill'").fetchone()
        self.assertEqual(audit[0], 'dm-master')
        self.assertIn('Testing', audit[1])
        self.assertEqual(self.dm.players('dev')['actions'], [], 'PROD and DEV queues are separate')

    def test_roles(self):
        viewer = self.sign_in('dm-viewer')
        self.assertTrue(self.dm.players('prod')['characters'])
        with self.assertRaises(D.DMError) as raised:
            self.dm.request(viewer, 'prod', 'character.kill', 'player-ada')
        self.assertEqual(raised.exception.status, 403)
        master = self.sign_in('dm-master')
        with self.assertRaises(D.DMError):
            self.dm.request(master, 'prod', 'character.explode', 'player-ada')
        with self.assertRaises(D.DMError):
            self.dm.request(master, 'prod', 'character.kill', 'nobody')

    def test_what_the_dm_login_may_touch(self):
        import psycopg
        with W.connect('prod', 'dm', dbname=self.names['prod']) as conn:
            with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                conn.execute("UPDATE world.cells SET name = 'x'")
            with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                conn.execute('SELECT * FROM game.accounts')
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                game.execute('SELECT * FROM dm.admins')

    def test_the_three_default_accounts(self):
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            owner.execute('TRUNCATE dm.admins CASCADE')
        with W.connect('prod', 'dm', dbname=self.names['prod']) as conn:
            made = D.create_defaults(conn)
            self.assertEqual([(u, r) for u, r, _ in made], [('dm-admin', 'admin'), ('dm-master', 'dm'), ('dm-viewer', 'viewer')])
            self.assertEqual(len({p for _, _, p in made}), 3, 'each has its own password')
            self.assertEqual(D.create_defaults(conn), [], 'never replaces existing accounts')
        for username, _, password in made:
            self.assertEqual(self.dm.login(username, password)['username'], username)


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class NpcFixture(Fixture):
    def world(self, target='prod'):
        with W.connect(target, 'dm', dbname=self.names[target]) as conn:
            return S.load_world(conn, 'greyfen')[0]

    def queued(self, target='prod'):
        with W.connect(target, 'owner', dbname=self.names[target]) as owner:
            return owner.execute('SELECT kind, target_id FROM dm.actions ORDER BY id').fetchall()


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class NpcTests(NpcFixture):
    """NPC Management: named NPCs changed through the live-edit log and synced into a running server."""

    def test_create_change_and_delete_a_named_npc(self):
        master = self.sign_in('dm-master')
        wren = next(p for p in self.world()['people'] if p['id'] == 'wren')
        newcomer = {**wren, 'id': 'hazel', 'name': 'Hazel', 'role': 'civilian', 'workLabel': 'sweeping'}
        self.dm.save_npc(master, 'prod', newcomer)
        self.assertIn('hazel', [p['id'] for p in self.world()['people']])
        self.dm.save_npc(master, 'prod', {**newcomer, 'greeting': 'Mind the broom.'})
        self.assertEqual(next(p for p in self.world()['people'] if p['id'] == 'hazel')['greeting'], 'Mind the broom.')
        self.dm.delete_npc(master, 'prod', 'hazel')
        self.assertNotIn('hazel', [p['id'] for p in self.world()['people']])
        self.assertEqual(self.queued(), [('npc.sync', 'hazel')] * 3, 'every change is synced into a running server')
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            log = owner.execute("SELECT editor, key FROM world.edits WHERE key = 'person:hazel' ORDER BY seq").fetchall()
        self.assertEqual(log, [('dm-master', 'person:hazel')] * 3, 'changes go through the live-edit log, so Atlas sees them')

    def test_bad_placements_are_refused_with_the_reason(self):
        master = self.sign_in('dm-master')
        wren = next(p for p in self.world()['people'] if p['id'] == 'wren')
        with self.assertRaises(D.DMError) as raised:
            self.dm.save_npc(master, 'prod', {**wren, 'home': {'cell': 'town', 'x': 0, 'y': 0}})    # A wall.
        self.assertIn('home', str(raised.exception))
        with self.assertRaises(D.DMError):
            self.dm.save_npc(master, 'prod', {**wren, 'route': 'no_such_route'})
        self.assertEqual(self.queued(), [])

    def test_viewers_cannot_change_npcs(self):
        viewer = self.sign_in('dm-viewer')
        wren = next(p for p in self.world()['people'] if p['id'] == 'wren')
        for attempt in (lambda: self.dm.save_npc(viewer, 'prod', wren), lambda: self.dm.delete_npc(viewer, 'prod', 'wren'),
                        lambda: self.dm.npc_life(viewer, 'prod', 'wren', True)):
            with self.assertRaises(D.DMError) as raised:
                attempt()
            self.assertEqual(raised.exception.status, 403)

    def test_kill_and_revive_reach_a_running_server_and_the_next_start(self):
        master = self.sign_in('dm-master')
        self.dm.npc_life(master, 'prod', 'sloe', True)
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            alive, state = owner.execute("SELECT alive, state FROM live.npc_state WHERE npc_id = 'sloe'").fetchone()
        self.assertEqual((alive, state['dead']), (False, True))
        self.assertEqual(self.dm.npcs('prod')['dead'], ['sloe'])
        self.dm.npc_life(master, 'prod', 'sloe', False)
        self.assertEqual(self.dm.npcs('prod')['dead'], [])
        self.assertEqual(self.queued(), [('npc.kill', 'sloe'), ('npc.revive', 'sloe')])
        with self.assertRaises(D.DMError):
            self.dm.npc_life(master, 'prod', 'nobody', True)

    def test_people_manifest_matches_the_exporter_for_job_holders_too(self):
        import map_editor as E
        with W.connect('dev', 'editor', dbname=self.names['dev']) as editor:
            import roster as R
            roster = R.load()
            free = {**roster['characters'][0], 'id': 'free_hand', 'name': 'Free Hand', 'assignment': None, 'profession': '',
                    'traits': ['steady', 'dry'], 'personality': 'Quiet.'}
            S.save_roster(editor, {**roster, 'characters': roster['characters'] + [free]})
            project, revision = S.load_world(editor, 'greyfen')
            spot = project['spawn']
            project['slots'] = [{'id': 'gate', 'name': 'Gate guard', 'profession': 'guard', 'workLabel': '', 'route': '', 'paid': True,
                                 'purse': 3, 'herbs': 0, 'meals': 1, 'hours': {'start': 6.5, 'end': 18.25},
                                 'home': spot, 'work': spot, 'evening': spot}]
            S.save_world(editor, project, revision)
            import world_build
            world_build.build(editor, 'test')
            sql = editor.execute("SELECT live.people_manifest('greyfen')").fetchone()[0].split('\n')
            files = E.export_files(S.load_world(editor, 'greyfen')[0], S.load_roster(editor))
        python = [l for l in files['world.ratw'].split('\n') if l.split(' ', 1)[0] in ('economy', 'route', 'resident', 'story')]
        self.assertTrue(any('"free_hand"' in l for l in sql), 'the job holder is a resident')
        self.assertEqual(sql, python)



@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class LayerTests(NpcFixture):
    """Patrol routes, painted NPC areas, spawn rules and who wanders where (phase 3)."""

    def setUp(self):
        super().setUp()
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            owner.execute('DELETE FROM live.npc_areas')          # Spawn rules and wander links go with them.

    def test_patrol_routes(self):
        master = self.sign_in('dm-master')
        route = {'id': 'night_round', 'name': 'Night round', 'posts': [{'cell': 'town', 'x': 30, 'y': 37}, {'cell': 'town', 'x': 22, 'y': 19}]}
        self.dm.save_route(master, 'prod', route)
        self.dm.save_route(master, 'prod', {**route, 'posts': route['posts'] + [{'cell': 'town', 'x': 13, 'y': 14}]})
        self.assertEqual(len(next(r for r in self.dm.npcs('prod')['routes'] if r['id'] == 'night_round')['posts']), 3)
        with self.assertRaises(D.DMError):
            self.dm.save_route(master, 'prod', {**route, 'posts': [{'cell': 'town', 'x': 0, 'y': 0}]})     # A wall.
        with self.assertRaises(D.DMError) as raised:
            self.dm.delete_route(master, 'prod', 'town_watch')
        self.assertIn('still walk this route', str(raised.exception))
        self.dm.delete_route(master, 'prod', 'night_round')
        self.assertNotIn('night_round', [r['id'] for r in self.dm.npcs('prod')['routes']])
        self.assertEqual(self.queued(), [('layers.sync', 'night_round')] * 3)
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            log = owner.execute("SELECT editor FROM world.edits WHERE key = 'route:night_round'").fetchall()
        self.assertEqual(len(log), 3, 'route changes go through the live-edit log')

    def test_areas_wandering_and_spawn_rules(self):
        master = self.sign_in('dm-master')
        self.dm.save_area(master, 'prod', {'id': 'square', 'name': 'Market square', 'kind': 'wander', 'cell': 'town',
                                           'tiles': [[30, 20], [31, 20], [30, 20]]})
        self.dm.set_wander(master, 'prod', 'wren', 'square')
        self.dm.save_area(master, 'prod', {'id': 'pens', 'name': 'Rat pens', 'kind': 'spawn', 'cell': 'town', 'tiles': [[40, 30]]})
        self.dm.save_spawn(master, 'prod', {'id': 'rats', 'name': 'Rats', 'area': 'pens', 'template': 'wren', 'count': 3,
                                            'respawnMinutes': 10, 'enabled': True})
        data = self.dm.npcs('prod')
        self.assertEqual([(a['id'], a['tiles']) for a in data['areas']], [('square', [[30, 20], [31, 20]]), ('pens', [[40, 30]])])
        self.assertEqual(data['wanders'], {'wren': 'square'})
        self.assertEqual((data['spawns'][0]['count'], data['spawns'][0]['alive']), (3, 0))
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            manifest = game.execute("SELECT live.people_manifest('greyfen', 'wren')").fetchone()[0]
        self.assertIn('wander "wren" 2 "town" 30.5 20.5 "town" 31.5 20.5', manifest)
        for bad in ({'id': 'x', 'name': 'X', 'kind': 'wander', 'cell': 'town', 'tiles': [[999, 0]]},
                    {'id': 'x', 'name': 'X', 'kind': 'lair', 'cell': 'town', 'tiles': []},
                    {'id': 'x', 'name': 'X', 'kind': 'plan', 'cell': 'nowhere', 'tiles': []},
                    {'id': 'X!', 'name': 'X', 'kind': 'plan', 'cell': 'town', 'tiles': []}):
            with self.assertRaises(D.DMError):
                self.dm.save_area(master, 'prod', bad)
        for bad in ({'count': 0}, {'template': 'nobody'}, {'area': 'nowhere'}, {'respawnMinutes': -1}):
            with self.assertRaises(D.DMError):
                self.dm.save_spawn(master, 'prod', {'id': 'r2', 'name': 'R', 'area': 'pens', 'template': 'wren', 'count': 1,
                                                    'respawnMinutes': 0, **bad})
        self.dm.delete_area(master, 'prod', 'pens')
        self.assertEqual(self.dm.npcs('prod')['spawns'], [], 'rules go with their area')
        self.dm.delete_area(master, 'prod', 'square')
        self.assertEqual(self.dm.npcs('prod')['wanders'], {})
        self.assertEqual([k for k, _ in self.queued()], ['layers.sync'] * 5, 'spawn rules need no sync; the server reads them')

    def test_viewers_cannot_change_layers(self):
        viewer = self.sign_in('dm-viewer')
        for attempt in (lambda: self.dm.save_area(viewer, 'prod', {'id': 'a', 'name': 'A', 'kind': 'plan', 'cell': 'town', 'tiles': []}),
                        lambda: self.dm.delete_route(viewer, 'prod', 'town_watch'),
                        lambda: self.dm.set_wander(viewer, 'prod', 'wren', None)):
            with self.assertRaises(D.DMError) as raised:
                attempt()
            self.assertEqual(raised.exception.status, 403)



@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class FactionTests(NpcFixture):
    """Factions: the factions themselves, territory claims, relations and members (phase 4)."""

    def setUp(self):
        super().setUp()
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            owner.execute('DELETE FROM live.factions')

    def test_factions_claims_members_and_relations(self):
        master = self.sign_in('dm-master')
        self.dm.save_faction(master, 'prod', {'id': 'watch', 'name': 'Town Watch', 'color': '#aabbcc', 'kind': 'city',
                                               'description': 'Keeps the peace.'})
        self.dm.save_faction(master, 'prod', {'id': 'guild', 'name': 'Tallow Guild', 'color': '#ccbbaa', 'kind': 'guild'})
        self.assertEqual([f['name'] for f in self.world()['factions']], ['Town Watch', 'Tallow Guild'], 'Atlas sees them')
        self.dm.save_claim(master, 'prod', 'watch', 'town', [[3, 4], [3, 4], [5, 6]])
        self.dm.save_claim(master, 'prod', 'guild', 'shop', [])
        data = self.dm.factions('prod')
        self.assertEqual(data['claims'], [{'faction': 'guild', 'area': 'shop', 'tiles': []},
                                          {'faction': 'watch', 'area': 'town', 'tiles': [[3, 4], [5, 6]]}])
        world = self.world()
        self.assertEqual({c['id']: c['territory']['claims'] for c in world['cells'] + world['rooms']}['town'], ['watch'])
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            log = [r[0] for r in owner.execute("SELECT key FROM world.edits WHERE editor = 'dm-master' ORDER BY seq").fetchall()]
            manifest = owner.execute("SELECT live.people_manifest('greyfen')").fetchone()[0]
        self.assertEqual(log, ['faction:watch', 'faction:guild', 'cell:town', 'room:shop'], 'claims reach Atlas as place edits')
        self.assertIn('faction "watch" "Town Watch" "#aabbcc"', manifest)
        self.assertIn('claims "town" 1 "watch"', manifest)
        self.dm.set_member(master, 'prod', 'watch', 'sloe', 'sergeant')
        self.dm.save_relation(master, 'prod', 'watch', 'guild', 40, 'friendly', 'They pay their dues.')
        self.dm.save_relation(master, 'prod', 'watch', 'guild', -70, 'hostile', 'Smuggling.')
        data = self.dm.factions('prod')
        self.assertEqual(data['members'], [{'faction': 'watch', 'npc': 'sloe', 'rank': 'sergeant'}])
        self.assertEqual([(r['disposition'], r['stance']) for r in data['relations']], [(-70, 'hostile')])
        self.assertEqual([h['stance'] for h in self.dm.relation_history('prod', 'watch', 'guild')], ['hostile', 'friendly'])
        for bad in (lambda: self.dm.save_relation(master, 'prod', 'watch', 'watch', 0, 'neutral'),
                    lambda: self.dm.save_relation(master, 'prod', 'watch', 'guild', 101, 'neutral'),
                    lambda: self.dm.save_relation(master, 'prod', 'watch', 'guild', 0, 'smitten'),
                    lambda: self.dm.save_claim(master, 'prod', 'watch', 'town', [[999, 0]]),
                    lambda: self.dm.save_faction(master, 'prod', {'id': 'x', 'name': 'X', 'color': 'red'})):
            with self.assertRaises(D.DMError):
                bad()
        self.dm.delete_claim(master, 'prod', 'guild', 'shop')
        self.dm.delete_faction(master, 'prod', 'watch')
        data = self.dm.factions('prod')
        self.assertEqual(([f['id'] for f in data['factions']], data['claims'], data['members'], data['relations']), (['guild'], [], [], []))
        self.assertEqual({c['id']: c['territory']['claims'] for c in self.world()['cells']}['town'], [])
        self.assertEqual([k for k, _ in self.queued()], ['factions.sync'] * 6)

    def test_viewers_cannot_change_factions(self):
        viewer = self.sign_in('dm-viewer')
        for attempt in (lambda: self.dm.save_faction(viewer, 'prod', {'id': 'a', 'name': 'A', 'color': '#000000'}),
                        lambda: self.dm.save_relation(viewer, 'prod', 'a', 'b', 0, 'neutral'),
                        lambda: self.dm.set_member(viewer, 'prod', 'a', 'sloe', '')):
            with self.assertRaises(D.DMError) as raised:
                attempt()
            self.assertEqual(raised.exception.status, 403)


if __name__ == '__main__':
    unittest.main()
