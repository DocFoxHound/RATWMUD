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
import os

# A throwaway database's name tag (RATW_TEST_DB_TAG, default ''): a run tagged s2stage_ makes ratw_test_s2stage_*.
DB_TAG = os.environ.get('RATW_TEST_DB_TAG', '')


class TargetTests(unittest.TestCase):
    """The worlds the DM works on come from the registry (doc 66): the live ones first, the live server's by default."""

    def test_every_registered_world_live_ones_first(self):
        self.assertEqual(D.targets(), W.keys('live') + W.keys('dev'))
        self.assertEqual(D.targets()[:1], ['prod'])
        self.assertIn('dev-old', D.targets())
        self.assertNotIn('ridgemere2', D.targets(), 'another name for dev: one world, listed once')

    def test_an_alias_reaches_its_world(self):
        reached = []
        dm = D.DungeonMaster(connect=lambda target: reached.append(target) or self.fail('stop here'))
        with self.assertRaises(AssertionError):
            dm.connect('ridgemere2')
        self.assertEqual(reached, ['dev'])

    def test_the_default_is_the_live_world_and_unknown_worlds_are_refused(self):
        self.assertEqual(D.default_target(), W.live_world())
        dm = D.DungeonMaster(connect=lambda target: self.fail('never connects to an unknown world'))
        with self.assertRaisesRegex(D.DMError, 'Choose a world'):
            dm.connect('atlantis')


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
        cls.names = {'prod': f'ratw_test_{DB_TAG}dmp_{tag}', 'dev': f'ratw_test_{DB_TAG}dmd_{tag}'}
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
                owner.execute('TRUNCATE dm.admins, dm.sessions, dm.actions, dm.audit, dm.watchers, dm.watch, dm.health CASCADE')
            with W.connect(target, 'game', dbname=name) as game:
                save_with_player(game)
        with W.connect('prod', 'dm', dbname=self.names['prod']) as conn:
            for username, role in (('dm-admin', 'admin'), ('dm-master', 'dm'), ('dm-viewer', 'viewer')):
                D.create_account(conn, username, role, f'{username}-password')

    def sign_in(self, username):
        return self.dm.session(self.dm.login(username, f'{username}-password')['token'])


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class DungeonMasterTests(Fixture):

    def test_the_world_map_loads_lean_and_its_ground_as_asked(self):
        whole = self.dm.world_map('dev')
        lean = self.dm.world_map('dev', lean=True)
        self.assertTrue(all(c['terrain'] is None and c['preview'] for c in lean['cells']))
        ids = [c['id'] for c in lean['cells']]
        sent = self.dm.ground('dev', ids)['cells']
        for c in whole['cells']:
            self.assertEqual(sent[c['id']]['terrain'], c['terrain'])
            self.assertEqual(S.decode_heights(sent[c['id']]['heightRows']) if 'heightRows' in sent[c['id']] else sent[c['id']]['heights'],
                             c['heights'])
        with self.assertRaises(D.DMError):
            self.dm.ground('dev', [])

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

    def test_a_gift_is_queued_with_its_payload(self):
        master = self.sign_in('dm-master')
        queued = self.dm.request(master, 'prod', 'character.gift', 'player-ada', 'Testing fire', {'gift': 'fire', 'quickened': True})
        self.assertEqual(self.dm.action('prod', queued['id'])['status'], 'queued')
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            row = game.execute("SELECT kind, target_id, payload FROM dm.actions WHERE kind = 'character.gift'").fetchone()
        self.assertEqual(row, ('character.gift', 'player-ada', {'gift': 'fire', 'quickened': True}), 'the game server reads the Gift')
        self.dm.request(master, 'prod', 'character.gift', 'player-ada', '', {'gift': 'water'})   # Any of the eight (doc 43)
        for bad in ('lightning', 'death_walker'):
            with self.assertRaises(D.DMError):
                self.dm.request(master, 'prod', 'character.gift', 'player-ada', '', {'gift': bad})
        taken = self.dm.request(master, 'prod', 'character.gift', 'player-ada', '', {'gift': '', 'quickened': True})
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            row = game.execute('SELECT payload FROM dm.actions WHERE id = %s', (taken['id'],)).fetchone()
        self.assertEqual(row[0], {'gift': '', 'quickened': False}, 'no Gift is never Quickened')
        ada = self.dm.players('prod')['characters'][0]
        self.assertEqual((ada['gift'], ada['quickened']), ('', False), 'the sheet shows the Gift (none yet)')

    def test_an_account_unlock_is_queued_and_its_standing_shown(self):
        # Earned Gift tiers (doc 49, Phase 5): account.unlock against one of the account's characters, and the account's
        # standing (game.account_standing, migration 0034) on the Players tab.
        master = self.sign_in('dm-master')
        queued = self.dm.request(master, 'prod', 'account.unlock', 'player-ada', 'Testing', {'op': 'grant', 'tier': 'gifted'})
        held = self.dm.request(master, 'prod', 'account.unlock', 'player-ada', '', {'op': 'hold', 'tier': 'quickened'})
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            rows = dict(game.execute("SELECT id, payload FROM dm.actions WHERE kind = 'account.unlock'").fetchall())
        self.assertEqual(rows[queued['id']], {'op': 'grant', 'tier': 'gifted'}, 'the game server reads the unlock')
        self.assertEqual(rows[held['id']], {'op': 'hold', 'tier': ''}, 'a hold is for the whole account')
        for bad in ({'op': 'grant'}, {'op': 'grant', 'tier': 'chosen'}, {'op': 'erase', 'tier': 'gifted'}, None):
            with self.assertRaises(D.DMError):
                self.dm.request(master, 'prod', 'account.unlock', 'player-ada', '', bad)
        self.assertIsNone(self.dm.players('prod')['characters'][0]['account'], 'no standing saved yet')
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            owner.execute('''INSERT INTO game.account_standing (world_id, key, position, data)
                             SELECT id, 'ada', 0, %s FROM world.worlds''',
                          (json.dumps({'account': 'ada', 'characters': ['player-ada'], 'giftedAt': 1, 'giftedBy': 'earned', 'hold': True,
                                       'measures': {'socialLevel': 4, 'normalScenes': 12, 'stars': 7, 'starGivers': 3, 'closedStories': 1}}),))
        account = self.dm.players('prod')['characters'][0]['account']
        self.assertEqual((account['name'], account['socialLevel'], account['gifted'], account['quickened'], account['hold']),
                         ('ada', 4, 'earned', None, True), 'the account, its social level, its tiers and its hold')

    def test_a_handle_and_a_profile_are_shown_read_only(self):
        # Doc 50, migration 0035: the account as a person and the character's profile, as the game server saves them.
        self.assertIsNone(self.dm.players('prod')['characters'][0]['person'], 'nothing saved yet')
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            owner.execute('''INSERT INTO game.account_profiles (world_id, key, position, data) SELECT id, 'ada', 0, %s FROM world.worlds''',
                          (json.dumps({'account': 'ada', 'characters': ['player-ada'], 'handle': 'Grey Fox', 'experience': 'guide',
                                       'playedSeconds': 5400}),))
            owner.execute('''INSERT INTO game.profiles (world_id, key, position, data) SELECT id, 'player-ada', 0, %s FROM world.worlds''',
                          (json.dumps({'currently': 'mending nets', 'status': 'lfs'}),))
        ada = self.dm.players('prod')['characters'][0]
        self.assertEqual(ada['person'], {'handle': 'Grey Fox', 'experience': 'guide', 'playedHours': 1.5, 'newcomer': True,
                                         'mentor': '', 'guided': 0}, 'the handle, hours played, and still a newcomer (doc 52)')
        self.assertEqual(('busy', 3), (lambda p: (p['mentor'], p['guided']))(
            D.person_view({'account': 'x', 'mentor': {'on': True, 'available': False, 'guided': 3}})), 'a busy mentor who guided 3')
        self.assertEqual('revoked', D.person_view({'account': 'x', 'mentor': {'revoked': True}})['mentor'], 'revoked by a DM')
        self.assertEqual('dm', D.ACTIONS['mentor.revoke'], 'revoking and restoring are for Dungeon Masters')
        tie = D.tie_view({'id': 'tie-1', 'state': 'active', 'starter': 'river', 'newcomer': 'w1', 'other': 'fen', 'resident': True,
                          'made': 5}, {'w1': 'Nell'})
        self.assertEqual(('Nell', 'fen', True, 'active'), (tie['newcomerName'], tie['otherName'], tie['resident'], tie['state']),
                         'a tie with the newcomer by name, a resident by its id')
        self.assertEqual('dm', D.ACTIONS['tie.end'], 'ending a tie is for Dungeon Masters')
        self.assertFalse(D.person_view({'account': 'x', 'graduated': True})['newcomer'], 'graduated: no longer new')
        self.assertFalse(D.person_view({'account': 'x', 'playedSeconds': 15 * 3600})['newcomer'], '15 hours: no longer new')
        self.assertEqual((ada['profile']['currently'], ada['profile']['status']), ('mending nets', 'lfs'), 'and the profile, to read')
        # Circles (doc 50, migration 0039): its account's circles, members by handle.
        self.assertEqual(ada['circles'], [], 'no circles yet')
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            owner.execute('''INSERT INTO game.circles (world_id, key, position, data) SELECT id, 'circle-1', 0, %s FROM world.worlds''',
                          (json.dumps({'id': 'circle-1', 'name': 'Moot Night', 'members': [
                              {'account': 'ada', 'role': 'keeper'}, {'account': 'bob', 'role': 'member'}], 'nights': []}),))
        self.assertEqual(self.dm.players('prod')['characters'][0]['circles'],
                         [{'name': 'Moot Night', 'role': 'keeper', 'members': ['Grey Fox', 'bob']}], 'its circle, by handles')
        # Stars (doc 51, migration 0040): the account's counted total and givers.
        self.assertIsNone(self.dm.players('prod')['characters'][0]['stars'], 'no stars yet')
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            owner.execute('''INSERT INTO game.star_tallies (world_id, key, position, data) SELECT id, 'ada', 0, %s FROM world.worlds''',
                          (json.dumps({'account': 'ada', 'total': 7, 'givers': ['bob', 'cy'], 'kinds': {'gold': 7}}),))
        self.assertEqual(self.dm.players('prod')['characters'][0]['stars'], {'total': 7, 'from': 2, 'kinds': {'gold': 7}})

    def test_reports_are_listed_and_decided(self):
        # Doc 50, Phase 2 (migration 0036): a report as the game server writes it, listed with its evidence and the block
        # count; a decision queued as report.decide, validated and audited; viewers may read but not decide.
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            owner.execute('''INSERT INTO game.reports (world_id, id, reporter_account, reporter_character, reported_account, reported_character,
                                                       kind, category, note, evidence)
                             SELECT id, 'rep-1', 'cy', 'player-cy', 'bob', 'player-ada', 'speech', 'harassment', 'He keeps at it.', %s
                             FROM world.worlds''', (json.dumps([{'seq': 7, 'at': 1, 'channel': 'ic', 'text': 'Get lost.'}]),))
            owner.execute('''INSERT INTO game.safety_marks (world_id, key, position, data) SELECT id, 'cy|block|bob', 0, %s FROM world.worlds''',
                          (json.dumps({'holder': 'cy', 'kind': 'block', 'target': 'bob', 'character': 'player-ada'}),))
        listed = self.dm.reports('prod')
        self.assertTrue(listed['ready'])
        report = listed['reports'][0]
        self.assertEqual((report['id'], report['status'], report['category'], report['blockedBy']), ('rep-1', 'open', 'harassment', 1))
        self.assertEqual(report['evidence'][0]['text'], 'Get lost.', 'the evidence lines')
        master = self.sign_in('dm-master')
        for bad in (('uphold', 'ban', 0), ('uphold', 'silence', 5), ('punish', '', 0)):
            with self.assertRaises(D.DMError):
                self.dm.decide_report(master, 'prod', 'rep-1', *bad)
        with self.assertRaises(D.DMError):
            self.dm.decide_report(master, 'prod', 'rep-nope', 'dismiss')
        queued = self.dm.decide_report(master, 'prod', 'rep-1', 'uphold', 'silence', 24, 'Repeated harassment.')
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            row = game.execute('SELECT kind, target_id, payload FROM dm.actions WHERE id = %s', (queued['id'],)).fetchone()
        self.assertEqual(row, ('report.decide', 'player-ada', {'report': 'rep-1', 'decision': 'uphold', 'outcome': 'silence', 'hours': 24}))
        viewer = self.sign_in('dm-viewer')
        with self.assertRaises(D.DMError):
            self.dm.decide_report(viewer, 'prod', 'rep-1', 'dismiss')

    def test_an_injury_is_given_or_taken_away(self):
        # Injuries (Docs/Design/38-injuries.md, phase 5): a known kind at a severity and side, or one taken away by id.
        master = self.sign_in('dm-master')
        given = self.dm.request(master, 'prod', 'character.injury', 'player-ada', 'A storyline', {'add': 'cracked_rib', 'severity': 3})
        taken = self.dm.request(master, 'prod', 'character.injury', 'player-ada', '', {'remove': 'injury-1-player-ada'})
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            rows = game.execute('SELECT id, payload FROM dm.actions WHERE kind = %s ORDER BY id', ('character.injury',)).fetchall()
        self.assertEqual([r[1] for r in rows], [{'add': 'cracked_rib', 'severity': 3, 'side': ''}, {'remove': 'injury-1-player-ada'}])
        self.assertEqual([r[0] for r in rows], [given['id'], taken['id']])
        for bad in ({'add': 'broken_heart'}, {'add': 'cracked_rib', 'severity': 4}, {'add': 'torn_ear', 'side': 'up'}, {'remove': ''}, {}):
            with self.assertRaises(D.DMError):
                self.dm.request(master, 'prod', 'character.injury', 'player-ada', '', bad)
        self.assertEqual(self.dm.players('prod')['characters'][0]['injuries'], [], 'the sheet lists injuries (none yet)')

    def test_a_player_is_made_a_dungeon_master_by_an_admin(self):
        master, admin = self.sign_in('dm-master'), self.sign_in('dm-admin')
        self.assertFalse(self.dm.players('prod')['characters'][0]['dungeonMaster'])
        with self.assertRaises(D.DMError) as raised:
            self.dm.request(master, 'prod', 'character.dm', 'player-ada', '', {'dungeonMaster': True})
        self.assertEqual(raised.exception.status, 403, 'only an admin hands out the Dev Console')
        queued = self.dm.request(admin, 'prod', 'character.dm', 'player-ada', 'Testing fights', {'dungeonMaster': True})
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            row = game.execute('SELECT kind, payload FROM dm.actions WHERE id = %s', (queued['id'],)).fetchone()
        self.assertEqual(row, ('character.dm', {'dungeonMaster': True}))
        for wrong in (None, {}, {'dungeonMaster': 'yes'}, {'dungeonMaster': 1}):
            with self.assertRaises(D.DMError):
                self.dm.request(admin, 'prod', 'character.dm', 'player-ada', '', wrong)
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            game.execute("UPDATE game.characters SET data = data || '{\"dungeonMaster\": true}' WHERE key = 'player-ada'")
        self.assertTrue(self.dm.players('prod')['characters'][0]['dungeonMaster'], 'the sheet shows who is one')

    def test_bandits_are_called_with_a_count(self):
        master = self.sign_in('dm-master')
        queued = self.dm.request(master, 'prod', 'bandits.call', 'player-ada', '', {'count': 2})
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            row = game.execute('SELECT kind, payload FROM dm.actions WHERE id = %s', (queued['id'],)).fetchone()
        self.assertEqual(row, ('bandits.call', {'count': 2}))
        for wrong in ({'count': 0}, {'count': 7}, {'count': True}, {'count': '2'}):
            with self.assertRaises(D.DMError):
                self.dm.request(master, 'prod', 'bandits.call', 'player-ada', '', wrong)

    def test_the_live_map_watches_and_moves_people(self):
        viewer, master = self.sign_in('dm-viewer'), self.sign_in('dm-master')
        first = self.dm.live(viewer, 'prod')
        self.assertIsNone(first['frame'], 'no game server has written a frame yet')
        self.assertIsNone(first['age'])
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            watching = game.execute('SELECT EXISTS (SELECT 1 FROM dm.watchers WHERE until > now())').fetchone()[0]
            self.assertTrue(watching, 'looking at the map tells the game server someone is watching')
            frame = {'day': 3.5, 'people': [['player-ada', 'Ada', 'p', 'town', 10.5, 12.5, 0, '', 'walking']], 'shops': []}
            game.execute('INSERT INTO dm.watch (world_id, frame) VALUES (%s, %s)', ('greyfen', json.dumps(frame)))
            game.execute("INSERT INTO game.events (world_id, game_time, game_day, kind, actor, cell) VALUES "
                         "('greyfen', 1, 1, 'theft', 'npc_a', 'town'), ('greyfen', 1, 1, 'economy', 'npc_a', 'town')")
        seen = self.dm.live(viewer, 'prod')
        self.assertEqual(seen['frame'], frame)
        self.assertLess(seen['age'], 60)
        self.assertEqual([e['kind'] for e in seen['events']], ['theft'], 'the quiet events (trade) are left off the map')
        self.assertIsNone(self.dm.live(viewer, 'dev')['frame'], 'PROD and DEV are watched apart')

        town = next(c for c in greyfen()['cells'])['id']
        with self.assertRaises(D.DMError) as raised:
            self.dm.move(viewer, 'prod', 'character.move', 'player-ada', town, 4, 5)
        self.assertEqual(raised.exception.status, 403, 'viewers only watch')
        queued = self.dm.move(master, 'prod', 'character.move', 'player-ada', town, 4, 5, 'Stuck in a wall')
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            row = game.execute('SELECT kind, target_id, payload FROM dm.actions WHERE id = %s', (queued['id'],)).fetchone()
        self.assertEqual(row, ('character.move', 'player-ada', {'cell': town, 'x': 4, 'y': 5}))
        self.assertEqual(self.dm.live(viewer, 'prod')['actions'][0]['id'], queued['id'], 'the map shows how a move went')
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            audit = owner.execute("SELECT detail FROM dm.audit WHERE action = 'character.move'").fetchone()[0]
        self.assertIn('Stuck in a wall', audit)
        for kind, cell, x, y in (('npc.explode', town, 1, 1), ('npc.move', 'nowhere', 1, 1), ('npc.move', town, -1, 1),
                                 ('npc.move', town, 1.5, 1), ('npc.move', town, True, 1)):
            with self.assertRaises(D.DMError):
                self.dm.move(master, 'prod', kind, 'npc_a', cell, x, y)

    def test_visitors_and_rumours_on_the_live_map(self):
        viewer, master = self.sign_in('dm-viewer'), self.sign_in('dm-master')
        town = next(c for c in greyfen()['cells'])['id']
        with self.assertRaises(D.DMError) as raised:
            self.dm.visit(viewer, 'prod', 'A messenger', town, 4, 5, 30)
        self.assertEqual(raised.exception.status, 403, 'viewers only watch')
        queued = self.dm.visit(master, 'prod', '  A   messenger ', town, 4, 5, 30, like='npc_a')
        self.assertTrue(queued['visitor'].startswith('visitor_'))
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            row = game.execute('SELECT kind, target_id, payload FROM dm.actions WHERE id = %s', (queued['id'],)).fetchone()
        self.assertEqual(row, ('visitor.add', queued['visitor'], {'name': 'A messenger', 'cell': town, 'x': 4, 'y': 5, 'minutes': 30, 'like': 'npc_a'}))
        for name, cell, minutes in (('', town, 30), ('x' * 61, town, 30), ('Lost', 'nowhere', 30), ('Brief', town, 0),
                                    ('Long', town, 24 * 60 + 1), ('Half', town, 1.5)):
            with self.assertRaises(D.DMError):
                self.dm.visit(master, 'prod', name, cell, 4, 5, minutes)
        gone = self.dm.leave(master, 'prod', queued['visitor'])
        self.assertEqual(self.dm.action('prod', gone['id'])['status'], 'queued')
        with self.assertRaises(D.DMError):
            self.dm.leave(master, 'prod', 'npc_a')
        kinds = [a['kind'] for a in self.dm.live(viewer, 'prod')['actions']]
        self.assertEqual(kinds[:2], ['visitor.leave', 'visitor.add'], 'the map shows how they went')

        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            for i, holder in enumerate(('npc_a', 'npc_b', 'npc_c')):
                data = {'holder': holder, 'subject': 'player-ada', 'claim': 'stole a pie', 'confidence': .5 + i * .2}
                game.execute('INSERT INTO game.beliefs (world_id, key, position, data) VALUES (%s, %s, %s, %s)',
                             ('greyfen', f'{holder}|player-ada|stole a pie', i, json.dumps(data)))
            game.execute('INSERT INTO game.beliefs (world_id, key, position, data) VALUES (%s, %s, %s, %s)',
                         ('greyfen', 'npc_a|npc_b|is kind', 3, json.dumps({'holder': 'npc_a', 'subject': 'npc_b', 'claim': 'is kind'})))
        going = self.dm.rumours('prod')['rumours']
        self.assertEqual(going[0], {'subject': 'player-ada', 'claim': 'stole a pie', 'holders': ['npc_a', 'npc_b', 'npc_c'], 'sure': .7},
                         'the most widely heard first, with everyone who has heard it')
        self.assertEqual(len(going), 2)

        # Doc 56: deeds on the LIVE map's Fame layer, and a deed going round told in its own words.
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            deed = {'id': 'deed-1', 'kind': 'broke_camp', 'weight': 1, 'doers': ['player-ada'], 'town': 'greyfen', 'cell': town,
                    'day': 10, 'names': {'player-ada': ['Ada']}, 'witnesses': [{'id': 'npc_a', 'as': {'player-ada': 'Ada'}}],
                    'towns': [{'town': 'greyfen', 'carrier': 'here', 'since': 10, 'reach': 0}]}
            game.execute('INSERT INTO game.deeds (world_id, key, position, data) VALUES (%s, %s, %s, %s)',
                         ('greyfen', 'deed-1', 0, json.dumps(deed)))
            game.execute('INSERT INTO game.events (world_id, game_time, game_day, kind, actor, cell, detail) VALUES '
                         "('greyfen', 0, 13, 'deed', 'player-ada', %s, 'drove the bandits off the road at the ford (deed-1)')", (town,))
            game.execute('INSERT INTO game.beliefs (world_id, key, position, data) VALUES (%s, %s, %s, %s)',
                         ('greyfen', 'npc_a|player-ada|deed:deed-1', 4,
                          json.dumps({'holder': 'npc_a', 'subject': 'player-ada', 'claim': 'deed:deed-1', 'as': 'Ada'})))
        fame = self.dm.fame('prod')
        self.assertEqual(fame['deeds'][0]['phrase'], 'drove the bandits off the road at the ford')
        self.assertEqual(fame['deeds'][0]['towns'], [{'town': 'greyfen', 'carrier': 'here', 'reach': 1.0}], 'all over town after 3 days')
        self.assertEqual(fame['deeds'][0]['witnesses'], [{'id': 'npc_a', 'as': {'player-ada': 'Ada'}}])
        self.assertIn('drove the bandits off the road at the ford', [r['claim'] for r in self.dm.rumours('prod')['rumours']])
        self.assertAlmostEqual(D.deed_reach({'carrier': 'here', 'since': 0}, 1, 0), .25)
        self.assertAlmostEqual(D.deed_reach({'carrier': 'caravan', 'since': 0}, 2, 3), .15 + .85 * .5)

        # Doc 57: town projects on the LIVE map's Projects layer, and the DM's hand on them (queued, checked, audited).
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            project = {'id': 'proj-1', 'kind': 'cover', 'title': 'the Greyfen market cover', 'town': 'greyfen', 'cell': town,
                       'x': 4, 'y': 5, 'state': 'open', 'hours': 20, 'worked': 8, 'needs': {'timber': 12},
                       'shown': {'player-ada': 'Ada', 'player-bo': ''},
                       'gifts': [{'who': 'player-ada', 'kind': 'coin', 'amount': 60, 'value': 60},
                                 {'who': 'player-bo', 'kind': 'labour', 'amount': 4, 'value': 3}]}
            game.execute('INSERT INTO game.projects (world_id, key, position, data) VALUES (%s, %s, %s, %s)',
                         ('greyfen', 'proj-1', 0, json.dumps(project)))
        listed = self.dm.projects('prod')
        self.assertEqual(listed['projects'][0]['givers'], [{'id': 'player-ada', 'shown': 'Ada', 'value': 60},
                                                           {'id': 'player-bo', 'shown': 'a friend of the town', 'value': 3}])
        self.assertIn('cover', [k['id'] for k in listed['kinds']])
        self.dm.live_action(master, 'prod', 'project.post', 'greyfen', {'kind': 'watchpost', 'title': 'The north watch'})
        self.dm.live_action(master, 'prod', 'project.cancel', 'proj-1')
        self.dm.live_action(master, 'prod', 'npc.protect', 'npc_a')        # (Doc 57, 6: a resident players can't ruin.)
        self.dm.live_action(master, 'prod', 'storyline.give', 'player-ada', {'template': 'the-debt', 'cast': {'resident': 'npc_a'}})
        for bad in (('storyline.give', 'player-ada', {'template': 'nope'}), ('storyline.tick', 'story-1', {'step': 'x', 'objective': 0}),
                    ('storyline.give', 'player-ada', {'template': 'the-debt', 'cast': {'resident': 'a b'}})):
            with self.assertRaises(D.DMError, msg=repr(bad)):
                self.dm.live_action(master, 'prod', *bad)
        for bad in (('project.post', 'greyfen', {'kind': 'castle'}), ('project.post', 'greyfen', {'kind': 'cover', 'cell': town, 'x': 'a'}),
                    ('character.kill', 'player-ada', {}), ('project.remove', '', {})):
            with self.assertRaises(D.DMError, msg=repr(bad)):
                self.dm.live_action(master, 'prod', *bad)
        with self.assertRaises(D.DMError):
            self.dm.live_action(viewer, 'prod', 'project.remove', 'proj-1')
        with W.connect('prod', 'owner', dbname=self.names['prod']) as owner:
            queued = owner.execute('SELECT kind, target_id, payload FROM dm.actions ORDER BY id').fetchall()
        self.assertEqual([q[:2] for q in queued[-4:]], [('project.post', 'greyfen'), ('project.cancel', 'proj-1'), ('npc.protect', 'npc_a'),
                                                        ('storyline.give', 'player-ada')])
        self.assertEqual(queued[-4][2], {'kind': 'watchpost', 'title': 'The north watch'})

        # Doc 79, 17: the Wharf's credit went with the Wharf's money: no such action.
        with self.assertRaises(D.DMError):
            self.dm.live_action(master, 'prod', 'wharf.credit', 'wharf', {'amount': -300, 'note': 'a lean winter'})

        # Doc 58: storytellers, their tales and kept words; a decision; a visitor saved; a milestone credited from the ledger.
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:
            game.execute('INSERT INTO game.storytellers (world_id, key, position, data) VALUES (%s, %s, %s, %s)',
                         ('greyfen', 'acct-ada', 0, json.dumps({'account': 'acct-ada', 'state': 'applied', 'note': 'Bells.', 'character': 'player-ada'})))
            tale = {'id': 'tale-1', 'kind': 'tale', 'title': 'The Drowned Bell', 'state': 'running', 'author': 'player-ada',
                    'authorAccount': 'acct-ada', 'participants': {'player-ada': {'joined': 1, 'left': -1}},
                    'steps': [{'title': 'The pier', 'objectives': [{'kind': 'told', 'line': 'At the pier', 'doneBy': 'player-ada', 'byHand': True}]}]}
            game.execute('INSERT INTO game.storylines (world_id, key, position, data) VALUES (%s, %s, %s, %s)',
                         ('greyfen', 'tale-1', 0, json.dumps(tale)))
            game.execute('INSERT INTO game.storyteller_log (world_id, key, position, data) VALUES (%s, %s, %s, %s)',
                         ('greyfen', 'slog-1', 0, json.dumps({'id': 'slog-1', 'account': 'acct-ada', 'character': 'player-ada',
                                                               'storyline': 'tale-1', 'kind': 'narrate', 'text': 'Fog on the water.', 'at': 5})))
            game.execute('INSERT INTO game.events (world_id, game_time, game_day, kind, actor, target, cell, detail) VALUES '
                         "('greyfen', 0, 13, 'storyline objective', 'player-ada', 'tale-1', %s, 'At the pier')", (town,))
        tellers = self.dm.storytellers('prod')
        self.assertTrue(tellers['ready'])
        self.assertEqual(tellers['standing'][0]['note'], 'Bells.')
        self.assertEqual(tellers['tales'][0]['steps'][0]['objectives'][0]['byHand'], True)
        self.assertEqual(tellers['log'][0]['text'], 'Fog on the water.')
        self.assertIn('the-debt', [t['id'] for t in tellers['templates']])
        self.dm.live_action(master, 'prod', 'storyteller.decide', 'acct-ada', {'approve': True, 'reason': 'Welcome.'})
        with self.assertRaises(D.DMError):
            self.dm.live_action(master, 'prod', 'storyteller.decide', 'acct-ada', {'approve': 'yes'})
        with self.assertRaises(D.DMError):
            self.dm.live_action(viewer, 'prod', 'storyteller.revoke', 'acct-ada', {})
        self.dm.save_story_visitor(master, 'prod', {'id': 'messenger', 'name': 'A cloaked messenger', 'description': 'Grey cloak.', 'enabled': True})
        self.assertEqual(self.dm.storytellers('prod')['visitors'][0]['approvedBy'], 'dm-master')
        with self.assertRaises(D.DMError):
            self.dm.save_story_visitor(master, 'prod', {'id': 'Bad Id', 'name': 'x'})
        credited = self.dm.credit_milestone(master, 'prod', 'The siege', 'The gate holds', 'great', {'kind': 'storyline', 'storyline': 'tale-1'})
        self.assertEqual(credited['people'], [{'id': 'player-ada', 'lines': ['At the pier']}], 'lines from the ledger only')
        with self.assertRaises(D.DMError):
            self.dm.credit_milestone(master, 'prod', 'The siege', '', 'great', {'kind': 'storyline', 'storyline': 'tale-1'})

    def test_the_servers_health(self):
        viewer = self.sign_in('dm-viewer')
        empty = self.dm.health('prod', 24)
        self.assertFalse(empty['missing'])
        self.assertEqual(empty['windows'], [], 'nothing recorded yet')
        window = {'clients': 3, 'tick': {'mean': 18.5, 'p99': 41, 'max': 60, 'over50': 2},
                  'ping': {'reporting': 2, 'p50': 35, 'p95': 90, 'worst': 140, 'worstWho': 'Ada'},
                  'backlog': {'largestKB': 300, 'slow': 1, 'dropped': 0}, 'traffic': {'outMbps': 1.5}, 'corrections': 4}
        spike = {'ms': 130, 'clients': 3, 'parts': {'world': 70, 'views': 40, 'saves': 0.5}, 'note': 'schedules=55'}
        with W.connect('prod', 'game', dbname=self.names['prod']) as game:   # (As the game server writes them.)
            game.execute("INSERT INTO dm.health (world_id, kind, body) VALUES ('greyfen', 'window', %s), ('greyfen', 'spike', %s)",
                         (json.dumps(window), json.dumps(spike)))
        seen = self.dm.health('prod', 24)
        self.assertEqual(len(seen['windows']), 1)
        w = seen['windows'][0]
        self.assertEqual((w['clients'], w['mean'], w['p99'], w['ping'], w['ping95'], w['slow']), (3, 18.5, 41, 35, 90, 1))
        self.assertEqual(seen['spikes'][0]['ms'], 130)
        self.assertEqual(seen['spikes'][0]['parts'][0], ['world', 70], 'a spike says where its time went, the most first')
        self.assertEqual(seen['players'], [{'name': 'Ada', 'ms': 140, 'at': w['at']}])
        self.assertEqual(self.dm.health('dev', 24)['windows'], [], 'PROD and DEV are kept apart')
        self.assertIsNotNone(viewer)

    def test_roles(self):
        viewer = self.sign_in('dm-viewer')
        self.assertTrue(self.dm.players('prod')['characters'])
        with self.assertRaises(D.DMError) as raised:
            self.dm.request(viewer, 'prod', 'character.kill', 'player-ada')
        self.assertEqual(raised.exception.status, 403)
        master = self.sign_in('dm-master')
        with self.assertRaises(D.DMError):
            self.dm.request(master, 'prod', 'character.explode', 'player-ada')
        # A Story book tied to a world storyline (doc 51, Phase 7): checked, then queued for the game.
        with self.assertRaises(D.DMError):
            self.dm.request(master, 'prod', 'book.storyline', 'player-ada', payload={'book': 'nope', 'storyline': 'x'})
        queued = self.dm.request(master, 'prod', 'book.storyline', 'player-ada', payload={'book': 'book-1', 'storyline': 'The Bandit Winter'})
        self.assertEqual(queued['status'], 'queued')
        with self.assertRaises(D.DMError):
            self.dm.request(master, 'prod', 'character.kill', 'nobody')

    def test_what_the_dm_login_may_touch(self):
        import psycopg
        with W.connect('prod', 'dm', dbname=self.names['prod']) as conn:
            with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                conn.execute("UPDATE world.cells SET name = 'x'")
            with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                conn.execute('SELECT * FROM game.accounts')
        # Players' notes, recaps and private messages are the game's alone (doc 50, migration 0038).
        for table in ('known_wolves', 'scene_recaps', 'private_inbox'):
            with W.connect('prod', 'dm', dbname=self.names['prod']) as conn:
                with self.assertRaises(psycopg.errors.InsufficientPrivilege, msg=table):
                    conn.execute(f'SELECT * FROM game.{table}')
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

    def test_voices_families_dialects_and_figures(self):
        # Talk (Docs/Design/62-talk.md, phase 6): saved after the game's own check, and read again by the game.
        master = self.sign_in('dm-master')
        people = [p['id'] for p in self.world()['people']]
        a, b = people[0], people[1]
        voice = {'from': 'mirebridge', 'accent': 'broad', 'traits': {'temper': 'short', 'humour': 'dry'}, 'quirk': 'counts_things',
                 'persona': 'A wry net-mender.', 'grammar': 'saying! = Dry feet, dry heart', 'rules': 'rule work\nwhen concept=bark working=yes\nsay #saying#.'}
        self.dm.save_voice(master, 'prod', a, voice)
        got = self.dm.talk('prod', a)['voice']
        self.assertEqual((got['from'], got['accent'], got['traits']['temper'], got['persona']), ('mirebridge', 'broad', 'short', 'A wry net-mender.'))
        with self.assertRaises(D.DMError) as refused:
            self.dm.save_voice(master, 'prod', a, {**voice, 'rules': 'rule work\nwhen concept=bark wealthy=yes\nsay Hm.'})
        self.assertIn('unknown fact', str(refused.exception), 'the game\'s own check, with its reasons')
        with self.assertRaises(D.DMError):
            self.dm.save_voice(master, 'prod', a, {**voice, 'traits': {'temper': 'volcanic'}})
        self.dm.save_kin(master, 'prod', a, b, 'spouse')
        self.assertEqual([(f['npc'], f['other'], f['kind']) for f in self.dm.talk('prod', b)['family']], [(a, b, 'spouse')])
        self.dm.save_kin(master, 'prod', a, b, 'spouse', remove=True)
        self.assertEqual(self.dm.talk('prod', b)['family'], [])
        self.dm.save_dialect(master, 'prod', 'mirebridge', 'grammar mirebridge\ngreet = Dry feet')
        self.assertEqual(self.dm.dialects('prod')[0]['name'], 'mirebridge')
        with self.assertRaises(D.DMError):
            self.dm.save_dialect(master, 'prod', 'mirebridge', 'grammar mirebridge\nnot_a_plain_symbol = x')
        with self.assertRaises(D.DMError):
            self.dm.save_dialect(master, 'prod', 'plain', 'grammar plain\ngreet = Hi')
        self.dm.save_figure(master, 'prod', {'id': 'the_lady', 'name': 'The Lady of the Sound', 'role': 'ruler', 'knownIn': 'realm'})
        self.assertEqual(self.dm.figures('prod')[0]['role'], 'ruler')
        with self.assertRaises(D.DMError):
            self.dm.save_figure(master, 'prod', {'id': 'x', 'name': 'X', 'role': 'emperor'})
        self.dm.save_figure(master, 'prod', {'id': 'the_lady'}, remove=True)
        self.assertEqual(self.dm.figures('prod'), [])
        self.assertTrue(all(kind == 'voice.sync' for kind, _ in self.queued()) and len(self.queued()) == 6, 'each read again by the game')

    def test_the_page_saves_over_what_it_saw_keeping_the_games_moves(self):
        # The live world's truth (doc 66, Progress): the page sends the NPC whole with the NPC as it loaded it.
        master = self.sign_in('dm-master')
        place = lambda x, y: {'cell': 'longhouse', 'x': x, 'y': y}

        def game_moves(x, y):
            with W.connect('prod', 'game', dbname=self.names['prod']) as game:
                game.execute("UPDATE live.npcs SET home_area = 'longhouse', home_x = %s, home_y = %s WHERE id = 'fennel'", (x, y))
        fennel = lambda: next(p for p in self.world()['people'] if p['id'] == 'fennel')
        saw = fennel()
        game_moves(5, 5)
        written = self.dm.save_npc(master, 'prod', {**saw, 'greeting': 'From the page.'}, saw)['written']
        self.assertEqual((fennel()['home'], fennel()['greeting'], written['home']), (place(5, 5), 'From the page.', place(5, 5)),
                         "the game's move kept, the page's words saved")
        # The DM puts her back where she was before the game moved her, on purpose: saved, not taken for a stale page.
        saw = fennel()
        self.dm.save_npc(master, 'prod', {**saw, 'home': place(2, 9)}, saw)
        self.assertEqual(fennel()['home'], place(2, 9))
        # Both moved her: refused, naming the field; the game's stays.
        saw = fennel()
        game_moves(3, 3)
        with self.assertRaises(D.DMError) as refused:
            self.dm.save_npc(master, 'prod', {**saw, 'home': place(17, 9)}, saw)
        self.assertEqual(refused.exception.status, 409)
        self.assertIn('home', str(refused.exception))
        self.assertEqual(fennel()['home'], place(3, 3))

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

    def test_board_and_estate_actions_are_for_dungeon_masters(self):
        # Doc 54: a notice taken down, and places set to let or cleared, by a Dungeon Master; never by a viewer.
        master, viewer = self.sign_in('dm-master'), self.sign_in('dm-viewer')
        for kind in ('board.remove', 'estate.set', 'estate.clear', 'deed.award', 'deed.revoke', 'nickname.drop', 'nickname.restore'):
            self.dm.allowed(master, kind)
            with self.assertRaises(D.DMError):
                self.dm.allowed(viewer, kind)

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

    def test_npcs_without_a_map_in_the_manifest_and_the_dm(self):
        # Doc 66, 2: an unplaced NPC, a jobless one and a job holder without a home, as the exporter writes them.
        import map_editor as E
        import roster as R
        with W.connect('dev', 'editor', dbname=self.names['dev']) as editor:
            roster = R.load()
            free = {**roster['characters'][0], 'id': 'free_hand', 'name': 'Free Hand', 'assignment': None, 'profession': ''}
            S.save_roster(editor, {**roster, 'characters': roster['characters'] + [free]})
            project, revision = S.load_world(editor, 'greyfen')
            people = {p['id']: p for p in project['people']}
            people['rook'].update(home=None, work=None, evening=None)
            people['linden'].update(work=None, evening=None)
            project['slots'] = [{'id': 'gate', 'name': 'Gate guard', 'profession': 'guard', 'workLabel': '', 'route': '', 'paid': True,
                                 'purse': 3, 'herbs': 0, 'meals': 1, 'hours': {'start': 6.5, 'end': 18.25},
                                 'home': None, 'work': project['spawn'], 'evening': None}]
            walked = next(p for p in project['people'] if p['route'])
            route = next(r for r in project['routes'] if r['id'] == walked['route'])
            route['posts'] = []                   # Not laid out (a map taken away): its walkers hold their posts.
            S.save_world(editor, project, revision)
            import world_build
            world_build.build(editor, 'test')
            sql = editor.execute("SELECT live.people_manifest('greyfen')").fetchone()[0].split('\n')
            files = E.export_files(S.load_world(editor, 'greyfen')[0], S.load_roster(editor))
        python = [l for l in files['world.ratw'].split('\n') if l.split(' ', 1)[0] in ('economy', 'route', 'resident', 'story')]
        self.assertEqual(sql, python)
        rook = next(l for l in sql if l.startswith('resident "rook"'))
        self.assertTrue(rook.endswith('"-" 0 0 "-" 0 0 "-" 0 0'), rook)
        self.assertFalse(any(l.startswith(f'route "{route["id"]}"') for l in sql))
        guard = next(l for l in sql if l.startswith(f'resident "{walked["id"]}"'))
        self.assertNotIn(f'"{route["id"]}"', guard)
        hand = next(l for l in sql if l.startswith('resident "free_hand"'))
        self.assertTrue(hand.endswith('"-" 0 0 "town" 27.5 21.5 "-" 0 0'), hand)
        data = self.dm.npcs('dev')
        self.assertIsNone(next(p for p in data['people'] if p['id'] == 'rook')['home'])
        self.assertEqual([(h['id'], h['home']) for h in data['holders']], [('free_hand', None)])



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



@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class ChronicleTests(Fixture):
    """Chronicles (Phase 8): a life from the event log, what they carry in mind, and a story written from it offline."""

    def setUp(self):
        super().setUp()
        import npc_mind
        self.dm._writer = (npc_mind.Mind(npc_mind.FixtureProvider(), audit=lambda e: None), 'fixture')
        with W.connect('dev', 'owner', dbname=self.names['dev']) as owner:
            owner.execute('TRUNCATE game.events, dm.stories')
        payload = {'schema': 1,
                   'players': [{'id': 'player-ada', 'name': 'Ada', 'description': 'A grey traveller.'}],
                   'npcs': [{'id': 'wren', 'name': 'Wren', 'description': 'The shopkeeper.'}, {'id': 'sloe', 'name': 'Sloe'}],
                   'bonds': [{'holder': 'wren', 'other': 'player-ada', 'affinity': -20, 'trust': -30, 'familiarity': 40,
                              'fear': 10, 'respect': 0, 'owed': 0, 'lastContact': 3}],
                   'beliefs': [{'holder': 'sloe', 'subject': 'player-ada', 'claim': 'stole from Wren',
                                'source': 'wren', 'confidence': .8, 'day': 3, 'incident': 'inc-1'}],
                   'summaries': [{'id': 'm1', 'npc': 'wren', 'subject': 'player-ada', 'text': 'Ada asked about rope.',
                                  'started': 1, 'consolidated': 2, 'sourceEvents': []}]}
        events = [{'kind': 'conversation', 'actor': 'player-ada', 'target': 'wren', 'day': 1},
                  {'kind': 'economy', 'actor': 'player-ada', 'target': 'wren', 'item': 'meal', 'quantity': 1, 'coins': 6,
                   'day': 1.2, 'detail': 'resident food purchase'},
                  {'kind': 'theft', 'actor': 'player-ada', 'target': 'wren', 'coins': 3, 'cell': 'shop', 'day': 3, 'detail': 'inc-1'},
                  {'kind': 'reported', 'actor': 'wren', 'target': 'sloe', 'day': 3.1, 'detail': 'theft (inc-1)'}]
        with W.connect('dev', 'game', dbname=self.names['dev']) as game:
            game.execute('SELECT game.save_checkpoint(%s, 2, %s)', ('greyfen', json.dumps(payload)))
            game.execute("SELECT game.record_events('greyfen', %s::jsonb)", (json.dumps(events),))

    def test_an_npcs_life_and_mind(self):
        life = self.dm.chronicle('dev', 'wren')
        self.assertEqual('Wren', life['name'])
        self.assertEqual(['Wren first spoke with Ada.', 'Ada stole 3 pennies from Wren.', 'Wren told the watch (Sloe) of a theft.'],
                         [e['text'] for e in life['entries']])
        self.assertEqual('Tallow & Twine', life['entries'][1]['place'])
        self.assertEqual('Earned 6 pennies; sold 1 meal; talked 1 time with Ada.', life['seasons'][0]['text'])
        mind = life['mind']
        self.assertEqual(['Ada asked about rope.'], [m['text'] for m in mind['memories']])
        self.assertEqual([('player-ada', -20)], [(b['who'], b['affinity']) for b in mind['bonds']])
        self.assertEqual('Ada', mind['names']['player-ada'])
        self.assertIsNone(life['story'])

    def test_a_players_life_and_what_is_said_of_them(self):
        life = self.dm.chronicle('dev', 'player-ada')
        self.assertIn('Ada stole 3 pennies from Wren.', [e['text'] for e in life['entries']])
        self.assertEqual('Spent 6 pennies; bought 1 meal; talked 1 time with Wren.', life['seasons'][0]['text'])
        self.assertEqual([('sloe', 'stole from Wren', 'wren')], [(b['holder'], b['claim'], b['source']) for b in life['mind']['said']])
        self.assertEqual([('wren', -30)], [(b['who'], b['trust']) for b in life['mind']['regard']], 'How others regard them')
        self.assertEqual(['Ada asked about rope.'], [m['text'] for m in life['mind']['memories']], 'What NPCs remember of them')

    def test_a_story_written_once_and_kept(self):
        master = self.sign_in('dm-master')
        life = self.dm.write_story(master, 'dev', 'wren')
        self.assertTrue(life['story']['text'].startswith('The chronicle of Wren holds'))
        self.assertEqual((0, 'fixture', 'dm-master'), (life['story']['newer'], life['story']['model'], life['story']['by']))
        self.assertEqual(life['story']['text'], self.dm.chronicle('dev', 'wren')['story']['text'], 'Kept, not rewritten')
        with W.connect('dev', 'game', dbname=self.names['dev']) as game:
            game.execute("SELECT game.record_events('greyfen', %s::jsonb)",
                         (json.dumps([{'kind': 'marriage', 'actor': 'wren', 'target': 'sloe', 'day': 9}]),))
        self.assertEqual(1, self.dm.chronicle('dev', 'wren')['story']['newer'], 'The story is one event behind')
        self.assertEqual(0, self.dm.write_story(master, 'dev', 'wren')['story']['newer'])
        with W.connect('dev', 'dm', dbname=self.names['dev']) as conn:
            self.assertEqual(2, conn.execute("SELECT count(*) FROM dm.audit WHERE action = 'story.write'").fetchone()[0])

    def test_who_may_have_stories_written(self):
        with self.assertRaises(D.DMError) as raised:
            self.dm.write_story(self.sign_in('dm-viewer'), 'dev', 'wren')
        self.assertEqual(403, raised.exception.status)
        self.assertIsNotNone(self.dm.chronicle('dev', 'wren'), 'Viewers may read chronicles')
        with self.assertRaises(D.DMError) as raised:
            self.dm.write_story(self.sign_in('dm-master'), 'dev', 'nobody')
        self.assertEqual(422, raised.exception.status, 'Nothing to write from')
        with self.assertRaises(D.DMError):
            self.dm.chronicle('dev', '')



@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class CalendarTests(NpcFixture):
    """Schedules (Phase 9): the world's week and festivals, and calling one for a town."""

    def test_the_date_and_the_week(self):
        with W.connect('dev', 'game', dbname=self.names['dev']) as game:
            game.execute('SELECT game.save_checkpoint(%s, 3, %s)', ('greyfen', json.dumps({'schema': 1, 'calendarDays': 5.5})))
        cal = self.dm.calendar('dev')
        self.assertEqual(('Marketday', 'Spring 6, Year 1', 12.0, 0, 1), (cal['today']['weekday'], cal['today']['date'],
                         cal['today']['hour'], cal['today']['nextMarket'], cal['today']['nextRest']))
        self.assertEqual((40, 'Spring 46, Year 1'), (cal['today']['nextFestival'], cal['today']['festivalDate']))
        self.assertEqual(['greyfen'], [c['id'] for c in cal['communities']])
        with W.connect('dev', 'game', dbname=self.names['dev']) as game:
            game.execute('SELECT game.save_checkpoint(%s, 4, %s)', ('greyfen', json.dumps({'schema': 1, 'calendarDays': 50.2})))
        self.assertEqual((87, 'Summer 46, Year 1'), (self.dm.calendar('dev')['today']['nextFestival'],
                                                     self.dm.calendar('dev')['today']['festivalDate']))

    def test_calling_a_festival(self):
        master = self.sign_in('dm-master')
        queued = self.dm.call_festival(master, 'dev', 'greyfen', 'The Lantern Night', 2)
        with W.connect('dev', 'dm', dbname=self.names['dev']) as conn:
            row = conn.execute('SELECT kind, target_id, payload FROM dm.actions WHERE id = %s', (queued['id'],)).fetchone()
        self.assertEqual(('festival.call', 'greyfen', {'name': 'The Lantern Night', 'inDays': 2}), row)
        self.assertEqual([queued['id']], [a['id'] for a in self.dm.calendar('dev')['actions']])
        for bad in (lambda: self.dm.call_festival(master, 'dev', 'nowhere'),
                    lambda: self.dm.call_festival(master, 'dev', 'greyfen', 'x' * 61),
                    lambda: self.dm.call_festival(master, 'dev', 'greyfen', '', 31),
                    lambda: self.dm.call_festival(master, 'dev', 'greyfen', '', True)):
            with self.assertRaises(D.DMError):
                bad()
        with self.assertRaises(D.DMError) as raised:
            self.dm.call_festival(self.sign_in('dm-viewer'), 'dev', 'greyfen')
        self.assertEqual(403, raised.exception.status)


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class ArtworkTests(Fixture):
    """Uploaded portraits (Docs/Design/29-client-polish.md, phase 9): the review queue and a queued decision."""

    def upload(self, art_id, status='pending', reported=False):
        with W.connect('dev', 'game', dbname=self.names['dev']) as game:
            game.execute('''INSERT INTO game.artwork (world_id, id, account, character_id, status, sha256, png_base64, reported)
                            VALUES ('greyfen', %s, 'ada', 'player-ada', %s, %s, 'iVBORw0KGgo=', %s)''',
                         (art_id, status, 'ab' * 32, reported))

    def test_the_queue_and_a_decision(self):
        self.upload('art-one')
        self.upload('art-two', reported=True)
        self.upload('art-old', status='approved')
        queue = self.dm.artwork('dev')
        self.assertTrue(queue['ready'])
        self.assertEqual(['art-two', 'art-one'], [p['id'] for p in queue['pending']])       # Reported first.
        self.assertEqual(('Ada', 'iVBORw0KGgo='), (queue['pending'][0]['name'], queue['pending'][0]['png']))
        self.assertEqual(['art-old'], [p['id'] for p in queue['recent']])
        self.assertNotIn('png', queue['recent'][0])
        master = self.sign_in('dm-master')
        queued = self.dm.review_artwork(master, 'dev', 'art-one', 'reject', 'Not a wolf.')
        with W.connect('dev', 'dm', dbname=self.names['dev']) as conn:
            row = conn.execute('SELECT kind, target_id, payload FROM dm.actions WHERE id = %s', (queued['id'],)).fetchone()
        self.assertEqual(('artwork.review', 'art-one', {'decision': 'reject', 'reason': 'Not a wolf.'}), row)
        self.assertEqual([queued['id']], [a['id'] for a in self.dm.artwork('dev')['actions']])
        for bad in (lambda: self.dm.review_artwork(master, 'dev', 'art-none', 'approve'),
                    lambda: self.dm.review_artwork(master, 'dev', 'art-one', 'maybe'),
                    lambda: self.dm.review_artwork(master, 'dev', 'art-one', 'reject', 'x' * 401)):
            with self.assertRaises(D.DMError):
                bad()
        with self.assertRaises(D.DMError) as raised:
            self.dm.review_artwork(self.sign_in('dm-viewer'), 'dev', 'art-one', 'approve')
        self.assertEqual(403, raised.exception.status)



@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class ChapterTests(Fixture):
    """Chapters, their camps and their pending dealings with the factions (Docs/Design/32): the screen and a decision."""

    def save_chapters(self):
        town = next(c for c in greyfen()['cells'])
        doc = {'schema': 1, 'players': [{'id': 'player-ada', 'name': 'Ada', 'cell': town['id'], 'x': 1, 'y': 1}],
               'chapters': {'next': 2, 'chapters': [{'id': 'ch-1', 'name': 'Ash Wardens', 'level': 3, 'renown': 120,
                                                     'members': [{'id': 'player-ada', 'rank': 0}], 'claimCell': town['id'], 'sworn': ['moss']}]},
               'camps': {'next': 3, 'sites': [{'id': 'site-1', 'chapter': 'ch-1', 'cell': town['id'], 'name': 'Ash Camp', 'x': 4, 'y': 5}],
                         'structures': [{'id': 'st-1', 'site': 'site-1', 'kind': 'hall', 'x': 6, 'y': 7, 'built': True, 'condition': 80}],
                         'staff': []},
               'factions': {'treaties': [{'id': 'tr-1', 'faction': 'wardens', 'chapter': 'ch-1', 'state': 'pending', 'tithe': 5},
                                         {'id': 'tr-0', 'faction': 'wardens', 'chapter': 'ch-1', 'state': 'ended'}],
                            'levies': [], 'houses': [{'chapter': 'ch-1', 'faction': 'wardens', 'state': 'pending', 'day': 3}]}}
        with W.connect('dev', 'game', dbname=self.names['dev']) as game:
            game.execute('SELECT game.save_checkpoint(%s, 2, %s)', ('greyfen', json.dumps(doc)))
        return town

    def test_the_screen_and_decisions(self):
        town = self.save_chapters()
        view = self.dm.chapters('dev')
        self.assertEqual(['Ash Wardens'], [c['name'] for c in view['chapters']])
        self.assertEqual(([{'id': 'player-ada', 'name': 'Ada', 'rank': 0}], town['name'], 1),
                         (view['chapters'][0]['members'], view['chapters'][0]['holdName'], view['chapters'][0]['sworn']))
        site = view['sites'][0]
        self.assertEqual((town['x'], town['y'], ['hall']), (site['cellX'], site['cellY'], [s['kind'] for s in site['structures']]))
        self.assertEqual(['tr-1', 'tr-0'], [t['id'] for t in view['treaties']])
        self.assertEqual(['pending'], [h['state'] for h in view['houses']])
        master = self.sign_in('dm-master')
        treaty = self.dm.decide(master, 'dev', 'treaty', 'tr-1', True, reason='Fair terms.')
        house = self.dm.decide(master, 'dev', 'house', 'ch-1', False, faction='wardens')
        with W.connect('dev', 'dm', dbname=self.names['dev']) as conn:
            rows = conn.execute('SELECT kind, target_id, payload FROM dm.actions WHERE id IN (%s, %s) ORDER BY id',
                                (treaty['id'], house['id'])).fetchall()
        self.assertEqual([('treaty.decide', 'tr-1', {'approve': True}),
                          ('house.decide', 'ch-1', {'approve': False, 'faction': 'wardens'})], rows)
        self.assertEqual({treaty['id'], house['id']}, {a['id'] for a in self.dm.chapters('dev')['actions']})
        for bad in (lambda: self.dm.decide(master, 'dev', 'treaty', 'tr-0', True),          # No longer pending.
                    lambda: self.dm.decide(master, 'dev', 'house', 'ch-1', True, faction='nobody'),
                    lambda: self.dm.decide(master, 'dev', 'levy', 'tr-1', True),
                    lambda: self.dm.decide(master, 'dev', 'treaty', 'tr-1', 'yes')):
            with self.assertRaises(D.DMError):
                bad()
        with self.assertRaises(D.DMError) as raised:
            self.dm.decide(self.sign_in('dm-viewer'), 'dev', 'treaty', 'tr-1', True)
        self.assertEqual(403, raised.exception.status)



REPORT = {'v': 1, 'day': 40, 'at': 40.4,
          'prosperity': {'slider': -10, 'target': -20, 'cause': 'the long frost', 'by': 'dm', 'neverHungry': False, 'neverBroke': False},
          'markets': [{'id': 'grey', 'name': 'Greyfen', 'times': {'word': 'lean', 'score': -0.4, 'cause': 'the long frost'},
                       'households': {'destitute': 1, 'poor': 3, 'comfortable': 6, 'rich': 1}, 'overridden': 1, 'hungry': 2,
                       'short': 1, 'thieves': 0.6, 'beggars': 1, 'dole': 2, 'nudge': None, 'resetting': None},
                      {'id': 'port', 'name': 'Saltmouth', 'times': {'word': 'ordinary', 'score': 0, 'cause': ''},
                       'households': {'destitute': 0, 'poor': 1, 'comfortable': 4, 'rich': 2}, 'overridden': 0, 'hungry': 0,
                       'short': 0, 'thieves': 0.1, 'beggars': 0, 'dole': 0, 'nudge': None, 'resetting': None}],
          'pressures': [{'market': 'grey', 'good': 'bread', 'P': 0.5, 'cause': 'the long frost'}],
          'areas': [{'id': 'orchards', 'name': 'The orchards', 'harvest': 'poor',
                     'override': {'word': 'poor', 'until': 60, 'words': 'blight at the orchards'}},
                    {'id': 'fields', 'name': 'The fields', 'harvest': 'fair', 'override': None}],
          'eventKinds': [{'id': 'wedding', 'words': 'a House wedding', 'needs': []}, {'id': 'fire', 'words': 'a fire', 'needs': []}],
          'weekDays': [35, 36, 37, 38, 39, 40, 41]}
BOARD = {'cell': {'grey': {}, 'port': {}},
         'committed': [{'id': 'd7', 'kind': 'wagon', 'from': 'grey', 'to': 'port', 'day': 41, 'hour': 8, 'manifest': {'bread': 4}}],
         'ships': [{'id': 's3', 'port': 'port', 'name': 'the Gull', 'day': 41, 'hour': 9, 'brings': [['salt', 6]], 'status': 'due'},
                   {'id': 's2', 'port': 'port', 'name': 'the Tern', 'day': 38, 'brings': [], 'status': 'storm', 'lost': True}],
         'lostShips': {'the Heron': 37},
         'events': [{'id': 'e4', 'market': 'grey', 'kind': 'wedding', 'day': 42, 'hour': 11, 'status': 'due', 'by': 'dm',
                     'words': 'the wedding up at the Heights'}],
         'nudges': {'grey': {'points': -30, 'until': 45, 'cause': 'the frost', 'told': True}},
         'recent': [{'day': 39, 'delta': 0.35, 'market': 'grey', 'good': 'bread', 'cause': 'the long frost'},
                    {'day': 39, 'delta': 0.05, 'market': 'grey', 'good': 'salt', 'cause': ''}]}
ROADS = {'caravans': [{'id': 'c9', 'from': 'port', 'to': 'grey', 'kind': 'wagon', 'departure': 'd6', 'departed': 40.3,
                       'status': 'travelling', 'manifest': [{'good': 'salt', 'n': 3}]}]}


class ProsperityTests(Fixture):
    """The Prosperity tab (doc 79, 12; Phase 9): Core's report from the checkpoint with the week's wagons, ships and events,
    player coin made against sunk, the word on the street and the log; and the DM's acts on the staged economy."""

    def plant(self, target='dev', report=True, faucets=None, stores=None):
        society = {'accounts': {'stores:greyfen': {'cash': 900, 'stock': {}}, 'stores:saltmouth': {'cash': 0, 'stock': {}},
                                'player-ada': {'cash': 40, 'stock': {}}, 'treasury': {'cash': 5, 'stock': {}}},
                   'board': BOARD, 'staged': {'faucets': faucets if faucets is not None else {
                       '38': {'acct-ada': {'reward': [10, 0, 0, 0], 'trade': [0, 4, 1, 0]}},
                       '40': {'acct-ada': {'dm grant: for the bell': [25, 0, 0, 0]}, 'player-bo': {'theft': [3, 0, 0, 0]}}}}}
        if report:
            society['report'] = REPORT
        with W.connect(target, 'owner', dbname=self.names[target]) as owner:
            owner.execute('DELETE FROM game.events')
        with W.connect(target, 'game', dbname=self.names[target]) as game:
            game.execute('SELECT game.save_checkpoint(%s, 9, %s)',
                         ('greyfen', json.dumps({'schema': 1, 'calendarDays': 40.4, 'society': society, 'roads': ROADS})))
            for day, kind, actor, target_, item, detail in (
                    (40.1, 'theft', 'npc_a', 'npc_b', 'bread', 'inc-1'), (39.5, 'theft', 'npc_a', 'npc_b', '', 'inc-0'),
                    (40.2, 'caravan arrives', 'c8', 'grey', 'd5', 'from port, guarded'),
                    (40.2, 'wagon cancelled', 'd4', 'port', 'grey', 'the road is closed'),
                    (39.0, 'times nudged', 'grey', 'grey', '', 'the frost'), (38.0, 'ship lost', 's2', 'port', 'the Tern', 'lost at sea'),
                    (2.0, 'times nudged', 'grey', 'grey', '', 'long ago')):
                game.execute('INSERT INTO game.events (world_id, game_time, game_day, kind, actor, target, cell, item, detail) '
                             "VALUES ('greyfen', 0, %s, %s, %s, %s, '', %s, %s)", (day, kind, actor, target_, item, detail))

    def test_the_prosperity_view(self):
        self.plant()
        master = self.sign_in('dm-master')
        self.dm.act(master, 'dev', 'times.nudge', 'grey', {'points': -30, 'days': 5, 'words': 'the frost'})
        view = self.dm.prosperity('dev')
        self.assertEqual(view['report'], REPORT, 'the report is Core\'s, passed through')
        self.assertFalse(view['stale'])
        self.assertEqual((view['gameDay'], view['target']), (40.4, 'dev'))
        self.assertEqual(view['towns'], [{'community': 'greyfen', 'treasury': 900}, {'community': 'saltmouth', 'treasury': 0}])
        wagons = {w['id']: w for w in view['week']['wagons']}
        self.assertEqual(wagons['c9'], {'id': 'c9', 'from': 'port', 'to': 'grey', 'day': 40, 'state': 'travelling', 'goods': ['salt']})
        self.assertEqual(wagons['d7']['state'], 'posted')
        self.assertEqual(wagons['d7']['goods'], ['bread'])
        self.assertEqual((wagons['c8']['state'], wagons['c8']['from']), ('arrived', 'port'))
        self.assertEqual((wagons['d4']['state'], wagons['d4']['from']), ('cancelled', 'grey'))
        ships = {s['name']: s for s in view['week']['ships']}
        self.assertEqual((ships['the Gull']['state'], ships['the Gull']['goods']), ('due', ['salt']))
        self.assertEqual(ships['the Tern']['state'], 'lost')
        self.assertEqual(ships['the Heron']['state'], 'lost', 'a ship lost lately, off the timetable')
        self.assertEqual(view['week']['events'][0]['by'], 'dm')
        players = view['players']
        self.assertEqual([d['day'] for d in players['days']], [38, 40])
        self.assertEqual(players['days'][0], {'day': 38, 'made': 10, 'sunk': 4, 'goodsMade': 1, 'goodsSunk': 0})
        ada = next(h for h in players['holders'] if h['holder'] == 'acct-ada')
        self.assertEqual((ada['made'], ada['sunk'], ada['byReason']['dm grant: for the bell']), (35, 4, [25, 0]))
        self.assertEqual(players['holders'][0]['holder'], 'acct-ada', 'the most coin first')
        self.assertEqual(view['thefts'], 1, "today's only")
        kinds = {w['kind'] for w in view['word']}
        self.assertTrue({'slider', 'times', 'harvest', 'ship', 'event', 'cause'} <= kinds, view['word'])
        self.assertIn({'kind': 'slider', 'text': 'the long frost'}, view['word'])
        self.assertLessEqual(len(view['word']), 20)
        log = view['log']
        self.assertEqual(log[0]['kind'], 'times.nudge', 'the newest first: the DM\'s act')
        self.assertIn('ship lost', [e['kind'] for e in log])
        self.assertNotIn('long ago', ' '.join(e['text'] for e in log), 'a 28-day log')
        ripples = [e for e in log if e['kind'] == 'ripple']
        self.assertEqual(len(ripples), 1, 'big ripples only (over 0.2)')
        self.assertIn('Trencher bread at Greyfen dearer', ripples[0]['text'], 'the good by its name')
        for gone in ('total', 'houses', 'funds', 'carters', 'residents', 'capital'):
            self.assertNotIn(gone, view)

    def test_the_tab_names_what_it_shows(self):
        # Phase 9's second repair: a player's coin by its account's characters' names (the faucet door keeps it by the
        # account), the log's players and residents by name, the goods by the catalogue's names, and the realm's own
        # treasury beside the towns' (a world with none of its own, as Ridgemere 2.0, had an empty table and no word why).
        self.plant()
        with W.connect('dev', 'game', dbname=self.names['dev']) as game:
            game.execute("INSERT INTO game.account_profiles (world_id, key, position, data) VALUES ('greyfen', 'acct-ada', 0, %s)",
                         (json.dumps({'account': 'acct-ada', 'characters': ['player-ada']}),))
        with W.connect('dev', 'owner', dbname=self.names['dev']) as owner:
            npc, npc_name = owner.execute("SELECT id, name FROM live.npcs WHERE world_id = 'greyfen' ORDER BY id LIMIT 1").fetchone()
        master = self.sign_in('dm-master')
        self.dm.act(master, 'dev', 'character.grant', 'player-ada', {'coins': 5, 'reason': 'for the bell'})
        self.dm.act(master, 'dev', 'band.override', npc, {'band': 'poor', 'days': 3})
        view = self.dm.prosperity('dev')
        self.assertEqual(next(h for h in view['players']['holders'] if h['holder'] == 'acct-ada')['name'], 'Ada')
        texts = [e['text'] for e in view['log'] if e.get('state')]
        self.assertTrue(any('character.grant Ada:' in t for t in texts), texts)
        self.assertTrue(any(f'band.override {npc_name}:' in t for t in texts), texts)
        self.assertEqual(view['goodNames'], {'bread': 'Trencher bread'})
        self.assertIn({'kind': 'cause', 'text': 'Trencher bread at Greyfen: the long frost'}, view['word'])
        self.assertEqual(view['treasury'], 5, 'the realm\'s own treasury')

    def test_the_npc_tab_knows_whom_the_bands_cover(self):
        # Phase 9's second repair: the NPC tab offered a band for a while on every NPC and said it took; the game refuses
        # one no household holds. The view names whom the bands cover (the save's residents), keeps the act's outcome
        # among the NPC's actions, and says whether the world holds an NPC's own band (migration 0061).
        society = {'residents': {'wren': {}, 'brann': {}}, 'report': REPORT}
        with W.connect('dev', 'game', dbname=self.names['dev']) as game:
            game.execute('SELECT game.save_checkpoint(%s, 9, %s)',
                         ('greyfen', json.dumps({'schema': 1, 'calendarDays': 40.4, 'society': society})))
        master = self.sign_in('dm-master')
        self.dm.act(master, 'dev', 'band.override', 'sloe', {'band': 'poor', 'days': 3})
        view = self.dm.npcs('dev')
        self.assertEqual(view['bandable'], ['brann', 'wren'])
        self.assertTrue(view['bandsAuthored'])
        self.assertIn('band.override', [a['kind'] for a in view['actions']], 'its outcome on the NPC tab')
        with W.connect('dev', 'owner', dbname=self.names['dev']) as owner:
            owner.execute('DELETE FROM game.checkpoints')
        self.assertIsNone(self.dm.npcs('dev')['bandable'], 'no save yet: not known')

    def test_the_log_says_what_the_server_made_of_each_act(self):
        # Doc 79, Phase 9's first repair: an act is in the log with its outcome, never as done when the server refused it
        # or it was never applied; the band in force on an NPC is the report's.
        self.plant()
        master = self.sign_in('dm-master')
        ids = {name: self.dm.act(master, 'dev', kind, target, payload)['id'] for name, kind, target, payload in (
            ('grant', 'character.grant', 'player-ada', {'coins': 23, 'reason': 'for the lost ewe'}),
            ('over', 'character.grant', 'player-ada', {'coins': 2500, 'reason': 'too much'}),
            ('storm', 'ship.storm', 's1', {}), ('nudge', 'times.nudge', 'grey', {'points': 5}))}
        with W.connect('dev', 'owner', dbname=self.names['dev']) as owner:
            for name, status, result in (('grant', 'applied', 'Granted 23p: for the lost ewe.'),
                                         ('over', 'refused', 'Over the day\'s grants for that player (477p left today).'),
                                         ('storm', 'expired', 'Not applied within ten minutes.')):
                owner.execute('UPDATE dm.actions SET status = %s, result = %s WHERE id = %s', (status, result, ids[name]))
        log = {e['text'].split(':', 2)[1].strip(): e for e in self.dm.prosperity('dev')['log'] if e.get('state')}
        texts = [e['text'] for e in self.dm.prosperity('dev')['log'] if e.get('state')]
        self.assertTrue(any('character.grant' in t and 'done (Granted 23p' in t for t in texts), texts)
        self.assertTrue(any('character.grant' in t and 'refused by the game (Over the day' in t for t in texts), texts)
        self.assertTrue(any('ship.storm s1: never applied' in t for t in texts), texts)
        self.assertTrue(any('times.nudge Greyfen: waiting for the game server' in t for t in texts), texts)
        self.assertTrue(log, texts)
        report = {**REPORT, 'bandOverrides': [{'household': 'h:cottage_1', 'band': 'poor', 'until': 52, 'members': ['npc_x', 'npc_y']}]}
        self.assertEqual(D.npc_band_override(report, 'npc_y'), {'band': 'poor', 'until': 52, 'household': 'h:cottage_1'})
        self.assertIsNone(D.npc_band_override(report, 'npc_z'))
        self.assertEqual(D.act_before('band.override', 'npc_x', {'report': report})['band'], 'poor', 'the audit\'s before')

    def test_an_old_or_missing_checkpoint(self):
        self.plant(report=False)
        with W.connect('dev', 'owner', dbname=self.names['dev']) as owner:
            owner.execute("UPDATE game.checkpoints SET saved_at = now() - interval '10 minutes'")
        view = self.dm.prosperity('dev')
        self.assertIsNone(view['report'], 'no report yet (an older save, or a server before Phase 9)')
        self.assertTrue(view['stale'])
        self.assertTrue(any(w['kind'] == 'ship' for w in view['word']), 'the word from the Board alone')
        with W.connect('dev', 'owner', dbname=self.names['dev']) as owner:
            owner.execute('DELETE FROM game.checkpoints')
        view = self.dm.prosperity('dev')
        self.assertEqual((view['report'], view['writtenAt'], view['gameDay'], view['stale']), (None, None, None, True))
        self.assertEqual(view['week'], {'wagons': [], 'ships': [], 'events': []})

    def test_each_act_is_for_dms_clamped_queued_and_audited(self):
        self.plant()
        master, viewer = self.sign_in('dm-master'), self.sign_in('dm-viewer')
        npc = greyfen()['people'][0]['id']
        cases = [
            ('times.nudge', 'grey', {'points': -400, 'days': 9999, 'words': '  the frost  '},
             {'points': -100, 'days': 366, 'words': 'the frost'}),
            ('times.nudge', 'grey', {'points': 0}, {'points': 0, 'days': 7, 'words': ''}),
            ('harvest.override', 'orchards', {'word': 'poor', 'days': 0, 'words': 'blight at the orchards'},
             {'word': 'poor', 'days': 1, 'words': 'blight at the orchards'}),
            ('harvest.override', 'fields', {'word': 'good'}, {'word': 'good', 'words': ''}),
            ('harvest.override', 'orchards', {'word': 'clear', 'days': 4}, {'word': 'clear'}),
            ('event.add', 'grey', {'kind': 'wedding', 'day': 500, 'hour': 30}, {'kind': 'wedding', 'day': 48, 'hour': 23}),
            ('event.add', 'grey', {'kind': 'fire', 'day': 3}, {'kind': 'fire', 'day': 40, 'hour': 9}),
            ('event.cancel', 'e4', {'anything': 1}, {}),
            ('ship.storm', 's3', {}, {}),
            ('prices.reset', 'all', {}, {}),
            ('prices.reset', 'grey', {'words': 'fresh stock'}, {'words': 'fresh stock'}),
            ('band.override', npc, {'band': 'destitute', 'days': 500}, {'band': 'destitute', 'days': 112}),
            ('band.override', npc, {'band': 'rich'}, {'band': 'rich', 'days': 28}),
            ('band.override', npc, {'band': 'clear', 'days': 3}, {'band': 'clear'}),
            ('character.grant', 'player-ada', {'coins': 99999, 'reason': 'for the bell'}, {'coins': 2500, 'reason': 'for the bell'}),
            ('character.grant', 'player-ada', {'coins': -12.4, 'reason': 'a broken window'}, {'coins': -12, 'reason': 'a broken window'}),
        ]
        for kind, target, payload, clean in cases:
            with self.subTest(kind=kind, payload=payload):
                with self.assertRaises(D.DMError) as raised:
                    self.dm.act(viewer, 'dev', kind, target, payload)
                self.assertEqual(raised.exception.status, 403)
                reply = self.dm.act(master, 'dev', kind, target, payload, reason='a story')
                self.assertEqual((reply['status'], reply['payload']), ('queued', clean))
        with W.connect('dev', 'owner', dbname=self.names['dev']) as owner:
            queued = owner.execute('SELECT kind, target_id, payload, requested_by FROM dm.actions ORDER BY id').fetchall()
            audit = owner.execute("SELECT action, target, detail, who FROM dm.audit WHERE action <> 'login' ORDER BY sequence").fetchall()
        self.assertEqual([(q[0], q[1], q[2]) for q in queued], [(k, t, c) for k, t, _, c in cases])
        self.assertEqual({q[3] for q in queued}, {'dm-master'})
        self.assertEqual([a[0] for a in audit], [k for k, *_ in cases])
        nudge = audit[0][2]
        self.assertIn('before (as last saved): {"cause": "the frost", "points": -30', nudge)
        self.assertIn('asked: {"days": 366, "points": -100', nudge)
        self.assertRegex(nudge, r'grey \(#\d+\)', 'the dm.actions row it queued')
        self.assertIn('a story', nudge)
        self.assertIn('before (as last saved): {"cash": 40}', audit[-2][2], "the player's cash before a grant")
        self.assertIn('before (as last saved): {"day": 42, "kind": "wedding", "market": "grey", "status": "due"}', audit[7][2])
        self.assertFalse([k for k, *_ in cases if k.startswith('economy.')])

    def test_acts_refused_only_for_an_unknown_target_or_a_malformed_payload(self):
        self.plant()
        master = self.sign_in('dm-master')
        for kind, target, payload, status in (
                ('times.nudge', 'atlantis', {'points': 5}, 404), ('times.nudge', 'grey', {'points': 'lots'}, 422),
                ('harvest.override', 'grey', {'word': 'poor'}, 404), ('harvest.override', 'orchards', {'word': 'blight'}, 422),
                ('event.add', 'grey', {'kind': 'flood'}, 422), ('event.add', 'grey', {'kind': 'Bad Kind'}, 422),
                ('event.cancel', 'e99', {}, 404), ('prices.reset', 'nowhere', {}, 404),
                ('prices.reset', 'grey', {'words': 'the old ' + 'd' + 'og market'}, 422),
                ('band.override', 'nobody', {'band': 'rich'}, 404), ('band.override', greyfen()['people'][0]['id'], {'band': 'royal'}, 422),
                ('character.grant', 'player-nobody', {'coins': 5, 'reason': 'x'}, 404),
                ('character.grant', 'player-ada', {'coins': 0, 'reason': 'x'}, 422),
                ('character.grant', 'player-ada', {'coins': 5}, 422),
                ('character.grant', 'player-ada', {'coins': 5, 'reason': 'x' * 81}, 422),
                ('character.grant', 'player-ada', {'coins': True, 'reason': 'x'}, 422),
                ('wharf.credit', 'wharf', {'amount': -300}, 422), ('economy.sync', 'economy', {}, 422)):
            with self.subTest(kind=kind, target=target, payload=payload):
                with self.assertRaises(D.DMError) as raised:
                    self.dm.act(master, 'dev', kind, target, payload)
                self.assertEqual(raised.exception.status, status, str(raised.exception))
        # The words of a 404 (Phase 9's first repair: "No such markets", "No such npc").
        for kind, target, words in (('prices.reset', 'nowhere', 'No such market: nowhere.'),
                                    ('band.override', 'player-cid', 'No such resident: player-cid.'),
                                    ('character.grant', 'player-nobody', 'No such player: player-nobody.')):
            with self.assertRaises(D.DMError) as raised:
                self.dm.act(master, 'dev', kind, target, {'band': 'rich', 'coins': 5, 'reason': 'x'})
            self.assertEqual(str(raised.exception), words)
        # A ship is the game server's to judge: one rolled since the last checkpoint (a new week's) is not refused here.
        self.assertEqual(self.dm.act(master, 'dev', 'ship.storm', 's99', {})['status'], 'queued')
        # The same acts reach the generic routes: /api/live/action and, for a character, /api/actions.
        self.assertEqual(self.dm.live_action(master, 'dev', 'ship.storm', 's3')['payload'], {})
        self.assertEqual(self.dm.request(master, 'dev', 'character.grant', 'player-ada', payload={'coins': 3, 'reason': 'thanks'})['payload'],
                         {'coins': 3, 'reason': 'thanks'})
        self.assertEqual(self.dm.grant(master, 'dev', 'player-ada', -5, 'a fine')['payload'], {'coins': -5, 'reason': 'a fine'})
        # Without a report (a server before Phase 9), markets come from the Board and areas are the server's to judge.
        self.plant(report=False)
        self.dm.act(master, 'dev', 'harvest.override', 'anywhere', {'word': 'good'})
        with self.assertRaises(D.DMError):
            self.dm.act(master, 'dev', 'times.nudge', 'atlantis', {'points': 5})
        self.dm.act(master, 'dev', 'event.add', 'grey', {'kind': 'house_wedding'})
        with self.assertRaises(D.DMError):
            self.dm.act(master, 'dev', 'event.add', 'grey', {'kind': 'wedding'})

    def test_the_routes(self):
        self.plant()
        import threading
        from http.client import HTTPConnection
        server = D.make_server(0, self.dm)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        port = server.server_port
        try:
            token = self.dm.login('dm-master', 'dm-master-password')['token']
            viewer = self.dm.login('dm-viewer', 'dm-viewer-password')['token']

            def call(method, path, body=None, who=token):
                http = HTTPConnection('127.0.0.1', port, timeout=10)
                headers = {'Authorization': f'Bearer {who}', 'Host': f'127.0.0.1:{port}'}
                if body is not None:
                    headers['Content-Type'] = 'application/json'
                http.request(method, path, json.dumps(body) if body is not None else None, headers)
                reply = http.getresponse()
                return reply.status, json.loads(reply.read())
            status, view = call('GET', '/api/prosperity?target=dev', who=viewer)
            self.assertEqual((status, view['report']['day']), (200, 40))
            self.assertEqual(call('GET', '/api/money?target=dev')[0], 404, 'the Money view went')
            status, done = call('POST', '/api/prosperity/act', {'target': 'dev', 'kind': 'times.nudge', 'id': 'grey',
                                                                'payload': {'points': 300, 'days': 2}, 'reason': 'boom at the port'})
            self.assertEqual((status, done['status'], done['payload']['points']), (200, 'queued', 100))
            self.assertEqual(call('GET', f"/api/actions/{done['id']}?target=dev")[1]['status'], 'queued', 'polled as any action')
            self.assertEqual(call('POST', '/api/prosperity/act', {'target': 'dev', 'kind': 'ship.storm', 'id': 's3'}, who=viewer)[0], 403)
            status, done = call('POST', '/api/players/grant', {'target': 'dev', 'character': 'player-ada', 'coins': 40, 'reason': 'the bell'})
            self.assertEqual((status, done['payload']), (200, {'coins': 40, 'reason': 'the bell'}))
        finally:
            server.shutdown()
            server.server_close()

    def test_no_economy_levers(self):
        for gone in ('set_tax_rate', 'set_mint', 'relief_grant', 'set_holdings_tax', 'money'):
            self.assertFalse(hasattr(self.dm, gone))
        self.assertFalse([k for k in D.ACTIONS if k.startswith('economy.')])
        acts = ('times.nudge', 'harvest.override', 'event.add', 'event.cancel', 'ship.storm', 'prices.reset', 'band.override',
                'character.grant')
        for kind in acts:
            self.assertEqual(D.ACTIONS.get(kind), 'dm', kind)
            self.assertIn(kind, D.LIVE_KINDS)
        self.assertEqual(set(D.LIVE_ACTS), set(acts))


if __name__ == '__main__':
    unittest.main()
