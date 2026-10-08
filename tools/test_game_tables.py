#!/usr/bin/env python3
"""The game save split into tables (migration 0012): exact round trips of real
saves, one row per thing, only changed rows written, logins kept private.
Runs in a scratch database dropped afterwards; skipped without the local PostgreSQL."""
import copy
import json
from pathlib import Path
import secrets
import sqlite3
import unittest

import psycopg

import world_db as W
from test_world_db import database_available, superuser

ROOT = Path(__file__).resolve().parent.parent
REAL_SAVES = sorted(p for p in (ROOT / 'Saved').rglob('*.sqlite') if p.is_file())


def real_payloads():
    out = []
    for path in REAL_SAVES:
        try:
            with sqlite3.connect(path) as c:
                row = c.execute('SELECT payload FROM world_state WHERE id = 1').fetchone()
            if row:
                out.append((path.name, row[0]))
        except sqlite3.Error:
            continue
    return out


def rich_payload():
    """A save with every list filled, including log entries that share a key."""
    return {
        'schema': 1, 'sequence': 9, 'revision': 42, 'time': 1234.5, 'calendarDays': 3.25,
        'accounts': {'version': 1, 'entries': [
            {'username': 'ada', 'salt': 's1', 'verifier': 'v1', 'iterations': 1000, 'characters': ['player-ada'], 'creations': {}},
            {'username': 'bea', 'salt': 's2', 'verifier': 'v2', 'iterations': 1000, 'characters': [], 'creations': {}}]},
        'players': [{'id': 'player-ada', 'name': 'Ada', 'cellId': 'town', 'position': [3.5, 4.5], 'age': 19}],
        'npcs': [{'id': 'wren', 'name': 'Wren', 'cellId': 'shop'}, {'id': 'sloe', 'name': 'Sloe', 'cellId': 'town'}],
        'mapMemories': [{'observer': 'player-ada', 'id': 'town', 'glyphs': '....', 'observed': '1111'},
                        {'observer': 'player-ada', 'id': 'shop', 'glyphs': '##', 'observed': '10'}],
        'activeMemory': [{'key': 'wren|player-ada', 'id': 'c1', 'npc': 'wren', 'subject': 'player-ada', 'turns': []}],
        'summaries': [{'id': 'm1', 'npc': 'wren', 'subject': 'player-ada', 'text': 'Bought a meal; polite.'}],
        'ledger': [{'event': 5, 'actor': 'player-ada', 'partner': 'wren', 'reason': 'trade', 'amount': 1},
                   {'event': 5, 'actor': 'player-ada', 'partner': 'wren', 'reason': 'trade', 'amount': 2}],
        'socialRecent': [{'event': 5, 'actor': 'player-ada', 'cell': 'shop', 'words': 12}],
        'socialSessions': [{'id': 's1', 'cell': 'shop', 'members': []}],
        'doors': {'shop_door': True}, 'weather': {'town': 'rain'}, 'director': {'version': 1},
        # People (doc 50): friends (two rows a friendship), a request waiting, a private message kept for an away friend.
        'people': {'accounts': [{'account': 'ada', 'handle': 'Adder', 'characters': ['player-ada']}],
                   'friends': [{'account': 'ada', 'friend': 'bea', 'since': 1, 'shares': True},
                               {'account': 'bea', 'friend': 'ada', 'since': 1, 'shares': False}],
                   'requests': [{'from': 'bea', 'to': 'cyd', 'at': 2}],
                   'inbox': [{'id': 'pm-1', 'to': 'bea', 'from': 'ada', 'fromCharacter': 'player-ada', 'text': 'See you at dawn.', 'at': 3}],
                   'known': [{'owner': 'player-ada', 'other': 'wren', 'firstMet': 1, 'lastMet': 2, 'note': 'Bakes well.'}],
                   'recaps': [{'id': 'rcp-1', 'owner': 'player-ada', 'session': 's1', 'place': 'Shop', 'text': 'You talked.',
                               'at': 4, 'minutes': 5, 'others': [], 'model': False}],
                   'circles': [{'id': 'circle-1', 'name': 'Moot Night', 'created': 1,
                                'members': [{'account': 'ada', 'role': 'keeper', 'shares': False, 'joined': 1}],
                                'invited': [], 'nights': []}],
                   'starTallies': [{'account': 'ada', 'total': 12, 'givers': ['bea']}],
                   'stars': [{'id': 'star-1', 'kind': 'gold', 'giverAccount': 'bea', 'recipientAccount': 'ada', 'at': 5,
                              'counted': True}],
                   'books': [{'id': 'book-1', 'title': 'The Drowned Bell', 'keeper': 'player-ada', 'state': 'open', 'chapters': []}]},
        'text with escapes': 'line\nbreak "quoted" \\ back ☃',
    }


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class GameTableTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.name = f'ratw_test_{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor, ratw_publisher, ratw_game')
        with W.connect('dev', 'owner', dbname=cls.name) as owner:
            W.migrate(owner)
            owner.execute("INSERT INTO world.worlds (id, name) VALUES ('w', 'World')")
        cls.game = W.connect('dev', 'game', dbname=cls.name)

    @classmethod
    def tearDownClass(cls):
        cls.game.close()
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    def setUp(self):
        with W.connect('dev', 'owner', dbname=self.name) as owner:
            owner.execute('DELETE FROM game.checkpoints')
            for (table,) in owner.execute('SELECT tbl FROM game.sections').fetchall():
                owner.execute(f'DELETE FROM game.{table}')

    def save(self, doc, revision=1):
        self.game.execute('SELECT game.save_checkpoint(%s, %s, %s)', ('w', revision, json.dumps(doc) if not isinstance(doc, str) else doc))

    def load(self):
        text = self.game.execute("SELECT game.load_checkpoint('w')").fetchone()[0]
        return None if text is None else json.loads(text)

    def rows(self, table):
        return self.game.execute(f"SELECT key, data, updated_at FROM game.{table} WHERE world_id = 'w' ORDER BY position").fetchall()

    def test_real_saves_round_trip_exactly(self):
        payloads = real_payloads()
        if not payloads:
            self.skipTest('no game saves under Saved/ to try')
        for name, text in payloads:
            with self.subTest(save=name):
                self.save(text)
                self.assertEqual(self.load(), json.loads(text))

    def test_every_list_becomes_rows(self):
        doc = rich_payload()
        self.save(doc)
        self.assertEqual(self.load(), doc)
        self.assertEqual([r[0] for r in self.rows('characters')], ['player-ada'])
        self.assertEqual(self.game.execute("SELECT name FROM game.characters").fetchone()[0], 'Ada')
        self.assertEqual([r[0] for r in self.rows('npcs')], ['wren', 'sloe'])
        self.assertEqual([r[0] for r in self.rows('map_memories')], ['player-ada|town', 'player-ada|shop'])
        self.assertEqual([r[0] for r in self.rows('relationships')], ['5|player-ada|wren|trade', '5|player-ada|wren|trade#2'])
        self.assertEqual(self.game.execute("SELECT npc FROM game.npc_memories").fetchone()[0], 'wren')
        self.assertEqual([r[0] for r in self.rows('friendships')], ['ada|bea', 'bea|ada'])
        self.assertEqual([r[0] for r in self.rows('friend_requests')], ['bea|cyd'])
        self.assertEqual(self.game.execute("SELECT recipient FROM game.private_inbox").fetchone()[0], 'bea')
        self.assertEqual([r[0] for r in self.rows('known_wolves')], ['player-ada|wren'])
        self.assertEqual([r[0] for r in self.rows('scene_recaps')], ['rcp-1'])
        self.assertEqual(self.game.execute("SELECT name FROM game.circles").fetchone()[0], 'Moot Night')
        self.assertEqual(self.game.execute("SELECT total FROM game.star_tallies").fetchone()[0], 12)
        self.assertEqual(self.game.execute("SELECT recipient_account, giver_account FROM game.stars").fetchone(), ('ada', 'bea'))
        self.assertEqual(self.game.execute("SELECT title, state FROM game.story_books").fetchone(), ('The Drowned Bell', 'open'))
        stored = json.loads(self.game.execute("SELECT payload FROM game.checkpoints").fetchone()[0])
        for gone in ('players', 'npcs', 'mapMemories', 'ledger'):
            self.assertNotIn(gone, stored, 'lists live in their tables, not the checkpoint row')
        self.assertEqual(stored['accounts'], {'version': 1}, 'logins are not left in the checkpoint row')

    def test_only_changed_rows_are_written(self):
        doc = rich_payload()
        self.save(doc)
        before = {table: {r[0]: r[2] for r in self.rows(table)} for table in ('npcs', 'characters')}
        changed = copy.deepcopy(doc)
        changed['npcs'][0]['cellId'] = 'town'
        changed['npcs'].append({'id': 'birch', 'name': 'Birch', 'cellId': 'barracks'})
        del changed['players'][0]
        self.save(changed, 2)
        npcs = {r[0]: r[2] for r in self.rows('npcs')}
        self.assertGreater(npcs['wren'], before['npcs']['wren'])
        self.assertEqual(npcs['sloe'], before['npcs']['sloe'], 'an unchanged NPC is not rewritten')
        self.assertIn('birch', npcs)
        self.assertEqual(self.rows('characters'), [], 'a character gone from the save is gone from the table')
        self.assertEqual(self.load(), changed)

    def test_older_saves_without_a_list_round_trip(self):
        doc = rich_payload()
        del doc['socialRecent'], doc['accounts']
        self.save(doc)
        self.assertEqual(self.load(), doc)

    def test_one_row_saves_from_before_still_load(self):
        with W.connect('dev', 'owner', dbname=self.name) as owner:
            owner.execute("""INSERT INTO game.checkpoints (world_id, schema_version, revision, payload)
                             VALUES ('w', 1, 1, '{"schema": 1, "players": []}')""")
        self.assertEqual(self.load(), {'schema': 1, 'players': []})
        self.assertIsNone(self.game.execute("SELECT game.load_checkpoint('nowhere')").fetchone()[0])

    def test_logins_are_for_the_game_server_only(self):
        self.save(rich_payload())
        self.assertEqual(len(self.rows('accounts')), 2)
        for role in ('editor', 'publisher'):
            with W.connect('dev', role, dbname=self.name) as other:
                with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                    other.execute('SELECT * FROM game.accounts')
                with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                    other.execute("SELECT game.save_checkpoint('w', 1, '{}')")
        with W.connect('dev', 'editor', dbname=self.name) as editor:
            self.assertEqual(editor.execute('SELECT count(*) FROM game.characters').fetchone()[0], 1, 'characters stay readable for tools')


if __name__ == '__main__':
    unittest.main()
