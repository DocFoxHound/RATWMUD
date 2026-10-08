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



class MoneyTests(Fixture):
    """The Money view (Docs/Design/42-money-in-circulation.md, Phase 8): where the money is, from the last save."""

    def test_where_the_money_is(self):
        someone = greyfen()['people'][0]['id']
        society = {'accounts': {'treasury': {'cash': 500, 'stock': {}}, 'house:fell': {'cash': 40, 'stock': {}},
                                'till:job:x': {'cash': 12, 'stock': {}}, 'caravan:cv1': {'cash': 7, 'stock': {}},
                                someone: {'cash': 3, 'stock': {}}},
                   'books': {'month': 1}}
        with W.connect('dev', 'game', dbname=self.names['dev']) as game:
            game.execute('SELECT game.save_checkpoint(%s, 9, %s)',
                         ('greyfen', json.dumps({'schema': 1, 'calendarDays': 30.2, 'society': society})))
            game.execute('INSERT INTO game.events (world_id, game_time, kind, actor, target, cell, game_day, detail) '
                         "VALUES ('greyfen', 1, 'reckoning', 'treasury', 'town:x:church', '', 28, %s)", ("X's reckoning",))
        money = self.dm.money('dev')
        self.assertEqual((562, 500, 1), (money['total'], money['capital'], money['month']))
        self.assertEqual([{'id': 'house:fell', 'cash': 40}], money['houses'])
        self.assertEqual({'count': 1, 'total': 12}, money['tills'])
        self.assertEqual(7, money['road']['caravans'])
        self.assertEqual((1, 1), (money['residents']['count'], money['residents']['shortOfFood']))
        self.assertEqual(['reckoning'], [e['kind'] for e in money['events']])
        self.assertIsNone(money['orchestrator'])                                  # Older saves have none.

    ORCHESTRATOR = {'mode': 'shadow', 'day': 12,
                    'steers': [{'id': 'steer-17', 'kind': 'pressure', 'target': '', 'item': '', 'strength': 2.0,
                                'from': 12, 'until': 19, 'note': 'why', 'by': 'dm-master'}],
                    'memory': {}, 'brief': {'day': 12, 'landDistress': 0.12, 'pot': 3200,
                                            'towns': [{'id': 'saltreach', 'people': 40, 'distress': 0.2, 'kind': 'no work'}],
                                            'holders': [], 'orders': [], 'prices': []}}

    def save_orchestrator(self):
        with W.connect('dev', 'game', dbname=self.names['dev']) as game:
            game.execute('SELECT game.save_checkpoint(%s, 9, %s)',
                         ('greyfen', json.dumps({'schema': 1, 'calendarDays': 12.5,
                                                 'society': {'accounts': {}, 'orchestrator': self.ORCHESTRATOR}})))

    def test_the_orchestrators_last_plan(self):
        self.save_orchestrator()
        self.assertEqual(self.ORCHESTRATOR, self.dm.money('dev')['orchestrator'])

    def test_steering_the_orchestrator(self):
        self.save_orchestrator()
        master = self.sign_in('dm-master')
        queued = [self.dm.steer_economy(master, 'dev', 'pressure', '', '', 2, 7, 'Squeeze the rich.'),
                  self.dm.steer_economy(master, 'dev', 'town', 'greyfen', '', 1.5, 14),
                  self.dm.steer_economy(master, 'dev', 'town', 'saltreach', '', 0, 1),      # A town only the brief knows.
                  self.dm.steer_economy(master, 'dev', 'holder', 'house:fell', '', 0, 28, 'Saving for a war.'),
                  self.dm.steer_economy(master, 'dev', 'channel', 'rescue', '', 0, 28),
                  self.dm.steer_economy(master, 'dev', 'price', '*', 'bread', 2.5, 56)]
        self.assertEqual(f"steer-{queued[0]['id']}", queued[0]['steer'])
        with W.connect('dev', 'dm', dbname=self.names['dev']) as conn:
            rows = conn.execute('SELECT kind, target_id, payload, requested_by FROM dm.actions WHERE id = ANY(%s) ORDER BY id',
                                ([q['id'] for q in queued],)).fetchall()
            audited = conn.execute("SELECT count(*) FROM dm.audit WHERE action = 'economy.steer'").fetchone()[0]
        steer = lambda kind, target, item, strength, days, note='': {'kind': kind, 'target': target, 'item': item,
                                                                     'strength': strength, 'days': days, 'note': note}
        self.assertEqual([('economy.steer', 'land', steer('pressure', '', '', 2.0, 7, 'Squeeze the rich.'), 'dm-master'),
                          ('economy.steer', 'greyfen', steer('town', 'greyfen', '', 1.5, 14), 'dm-master'),
                          ('economy.steer', 'saltreach', steer('town', 'saltreach', '', 0.0, 1), 'dm-master'),
                          ('economy.steer', 'house:fell', steer('holder', 'house:fell', '', 0.0, 28, 'Saving for a war.'), 'dm-master'),
                          ('economy.steer', 'rescue', steer('channel', 'rescue', '', 0.0, 28), 'dm-master'),
                          ('economy.steer', '*', steer('price', '*', 'bread', 2.5, 56), 'dm-master')], rows)
        self.assertEqual(6, audited)
        for bad in (lambda: self.dm.steer_economy(master, 'dev', 'drought', '', '', 1, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'pressure', '', '', 0.4, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'pressure', '', '', 3.5, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'pressure', '', '', True, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'pressure', '', '', '2', 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'pressure', '', '', float('nan'), 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'pressure', 'greyfen', '', 2, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'pressure', '', 'bread', 2, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'pressure', '', '', 2, 0),
                    lambda: self.dm.steer_economy(master, 'dev', 'pressure', '', '', 2, 57),
                    lambda: self.dm.steer_economy(master, 'dev', 'pressure', '', '', 2, 7.0),
                    lambda: self.dm.steer_economy(master, 'dev', 'pressure', '', '', 2, True),
                    lambda: self.dm.steer_economy(master, 'dev', 'pressure', '', '', 2, 7, 'x' * 201),
                    lambda: self.dm.steer_economy(master, 'dev', 'pressure', '', '', 2, 7, 'a\nb'),
                    lambda: self.dm.steer_economy(master, 'dev', 'town', 'nowhere', '', 1, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'town', '*', '', 1, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'town', '', '', 1, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'town', 'greyfen', '', 3.5, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'holder', '', '', 2, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'holder', 'house:' + 'f' * 120, '', 2, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'holder', 'house fell', '', 2, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'holder', 'house:fell', '', 1, 7),     # Neither spared nor squeezed.
                    lambda: self.dm.steer_economy(master, 'dev', 'holder', 'house:fell', '', 4.5, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'channel', 'bribes', '', 1, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'channel', 'works', '', 3.5, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'price', 'greyfen', '', 2, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'price', 'greyfen', 'b' * 61, 2, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'price', 'nowhere', 'bread', 2, 7),
                    lambda: self.dm.steer_economy(master, 'dev', 'price', '*', 'bread', 0.4, 7)):
            with self.assertRaises(D.DMError):
                bad()
        with W.connect('dev', 'dm', dbname=self.names['dev']) as conn:
            self.assertEqual(6, conn.execute("SELECT count(*) FROM dm.actions WHERE kind = 'economy.steer'").fetchone()[0])
        with self.assertRaises(D.DMError) as raised:
            self.dm.steer_economy(self.sign_in('dm-viewer'), 'dev', 'pressure', '', '', 2, 7)
        self.assertEqual(403, raised.exception.status)

    def test_ending_a_steer(self):
        master = self.sign_in('dm-master')
        queued = self.dm.end_steer(master, 'dev', 'steer-17')
        with W.connect('dev', 'dm', dbname=self.names['dev']) as conn:
            row = conn.execute('SELECT kind, target_id, payload, requested_by FROM dm.actions WHERE id = %s', (queued['id'],)).fetchone()
        self.assertEqual(('economy.unsteer', 'steer-17', {}, 'dm-master'), row)
        for bad in ('steer-', 'steer-x', '17', 'steer-17 ', 'steer-17\n', None, 17):
            with self.assertRaises(D.DMError):
                self.dm.end_steer(master, 'dev', bad)
        with self.assertRaises(D.DMError) as raised:
            self.dm.end_steer(self.sign_in('dm-viewer'), 'dev', 'steer-17')
        self.assertEqual(403, raised.exception.status)


    def test_funds_and_granaries(self):
        society = {'accounts': {'fund:greyfen:works': {'cash': 30, 'stock': {}},
                                'fund:greyfen:price_support': {'cash': 12, 'stock': {}},
                                'fund:saltreach:food': {'cash': 5, 'stock': {}},
                                'fund:land': {'cash': 200, 'stock': {}},
                                'town:greyfen:granary': {'cash': 40, 'stock': {'bread': 10, 'porridge': 4}},
                                'town:greyfen:watch': {'cash': 9, 'stock': {}}}}
        with W.connect('dev', 'game', dbname=self.names['dev']) as game:
            game.execute('SELECT game.save_checkpoint(%s, 9, %s)',
                         ('greyfen', json.dumps({'schema': 1, 'calendarDays': 3.5, 'society': society})))
        money = self.dm.money('dev')
        self.assertEqual({'greyfen': {'price support': 12, 'works': 30}, 'saltreach': {'food': 5}}, money['funds'])
        self.assertEqual(200, money['landFund'])
        self.assertEqual({'greyfen': {'cash': 40, 'goods': 14}}, money['granaries'])
        self.assertEqual({'watch': 9}, money['towns']['greyfen']['buyers'])          # The granary isn't a buyer.

    def test_the_scenarios(self):
        scenarios = self.dm.scenarios()
        listed = json.loads((D.ROOT / 'Data/Economy/scenarios.json').read_text(encoding='utf-8'))['scenarios']
        self.assertEqual([s['id'] for s in listed], [s['id'] for s in scenarios])
        self.assertIn('famine', [s['id'] for s in scenarios])

    def test_the_staples(self):
        staples = self.dm.staples()
        self.assertLessEqual(len(staples), D.DungeonMaster.STAPLES_MAX)
        self.assertIn('bread', staples)
        self.assertIn('firewood', staples)
        for drink in ('water', 'ale', 'cider'):
            self.assertNotIn(drink, staples)

    def actions(self, ids):
        with W.connect('dev', 'dm', dbname=self.names['dev']) as conn:
            return [r[0] for r in conn.execute('SELECT payload FROM dm.actions WHERE id = ANY(%s) ORDER BY id', (ids,)).fetchall()]

    def test_starting_a_scenario(self):
        self.save_orchestrator()
        master = self.sign_in('dm-master')
        famine = self.dm.start_scenario(master, 'dev', 'famine', town='greyfen')
        self.assertEqual('famine', famine['scenario'])
        staples = self.dm.staples()
        rows = self.actions([q['id'] for q in famine['queued']])
        self.assertEqual(len(staples) + 2, len(rows))
        self.assertTrue(all(q['steer'] == f"steer-{q['id']}" for q in famine['queued']))
        note = 'Scenario: Famine in a town (greyfen)'
        self.assertEqual([{'kind': 'price', 'target': 'greyfen', 'item': good, 'strength': 2.0, 'days': 21, 'note': note}
                          for good in staples], rows[:len(staples)])
        self.assertEqual([{'kind': 'town', 'target': 'greyfen', 'item': '', 'strength': 2.5, 'days': 21, 'note': note},
                          {'kind': 'channel', 'target': 'food', 'item': '', 'strength': 2.0, 'days': 21, 'note': note}],
                         rows[len(staples):])
        miser = self.dm.start_scenario(master, 'dev', 'the_miser', holder='house:fell')
        note = 'Scenario: The miser (house:fell)'
        self.assertEqual([{'kind': 'holder', 'target': 'house:fell', 'item': '', 'strength': 0.0, 'days': 28, 'note': note},
                          {'kind': 'pressure', 'target': '', 'item': '', 'strength': 1.3, 'days': 28, 'note': note}],
                         self.actions([q['id'] for q in miser['queued']]))
        winter = self.dm.start_scenario(master, 'dev', 'hard_winter')
        self.assertEqual(len(staples) + 1, len(winter['queued']))
        self.assertEqual('*', self.actions([winter['queued'][0]['id']])[0]['target'])
        with W.connect('dev', 'dm', dbname=self.names['dev']) as conn:
            before = conn.execute("SELECT count(*) FROM dm.actions WHERE kind = 'economy.steer'").fetchone()[0]
        for bad in (lambda: self.dm.start_scenario(master, 'dev', 'famine'),
                    lambda: self.dm.start_scenario(master, 'dev', 'famine', town='nowhere'),
                    lambda: self.dm.start_scenario(master, 'dev', 'the_miser'),
                    lambda: self.dm.start_scenario(master, 'dev', 'the_miser', holder='house fell'),
                    lambda: self.dm.start_scenario(master, 'dev', 'drought'),
                    lambda: self.dm.start_scenario(master, 'dev', None)):
            with self.assertRaises(D.DMError):
                bad()
        with self.assertRaises(D.DMError) as raised:
            self.dm.start_scenario(master, 'dev', 'drought')
        self.assertEqual(404, raised.exception.status)
        with self.assertRaises(D.DMError) as raised:
            self.dm.start_scenario(self.sign_in('dm-viewer'), 'dev', 'squeeze_the_rich')
        self.assertEqual(403, raised.exception.status)
        with W.connect('dev', 'dm', dbname=self.names['dev']) as conn:
            self.assertEqual(before, conn.execute("SELECT count(*) FROM dm.actions WHERE kind = 'economy.steer'").fetchone()[0])

if __name__ == '__main__':
    unittest.main()
