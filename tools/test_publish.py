#!/usr/bin/env python3
"""Push to live, now an import (tools/publish.py over tools/world_sync.py, doc 66 section 4): the password and its
lockout, validation, a first import into an empty live world, only what changed, live's own changes kept, refusals,
releases and rollback, and dev builds. A throwaway live world and a dev world (its upstream that live one) in a scratch
registry, dropped afterwards; skipped when the local PostgreSQL is not running. (The three-way comparison itself:
test_world_sync.py.)"""
import os
from pathlib import Path
import secrets
import tempfile
import unittest

import live_edit as L
import map_editor as E
import publish as P
import world_build as B
import roster as R
import world_db as W
import world_store as S
import world_sync as WS
from test_world_db import database_available, superuser
from test_world_store import greyfen


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class PublishTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        registry = Path(cls.folder.name) / 'worlds.json'
        registry.write_text(W.WORLDS_FILE.read_text(encoding='utf-8'), encoding='utf-8')
        cls.saved_env = dict(os.environ)
        os.environ.update({'RATW_WORLDS_FILE': str(registry), 'RATW_WORLDS_LOCAL': str(Path(cls.folder.name) / 'local.json')})
        cls.tag, cls.count = secrets.token_hex(3), 0

    @classmethod
    def tearDownClass(cls):
        os.environ.clear()
        os.environ.update(cls.saved_env)
        cls.folder.cleanup()
        W.worlds()

    def setUp(self):
        type(self).count += 1
        self.live, self.devkey = f'p{self.tag}_l{self.count}', f'p{self.tag}_d{self.count}'
        quiet = lambda *_: None
        W.new_world(self.live, 'Live', 'live', log=quiet)
        W.new_world(self.devkey, 'Dev', 'dev', upstream_key=self.live, log=quiet)
        self.dev = W.connect(self.devkey, 'editor')
        S.save_roster(self.dev, R.load())
        S.save_world(self.dev, greyfen(), None, create=True)
        WS.init_base(self.devkey, 'Ada')                 # (The live world is empty: everything in dev is dev's.)
        self.prod = W.connect(self.live, 'owner')
        self.publisher = P.Publisher(worlds=(self.devkey, self.live))

    def tearDown(self):
        self.dev.close()
        self.prod.close()
        with superuser() as su:
            for key in (self.live, self.devkey):
                su.execute(f'DROP DATABASE IF EXISTS ratw_{key} WITH (FORCE)')

    def push(self, password='password', note='', choices=None):
        return self.publisher.push({'password': password, 'editor': 'Ada', 'note': note, 'choices': choices or {}})

    def maintenance(self):
        WS.maintenance(self.live, True, 'Ada')

    def edit(self, *ops):
        return L.apply_edit(self.dev, 'test', 'Ada', 'Test edit', [{'key': k, 'before': b, 'after': a} for k, b, a in ops])

    def prod_world(self):
        return S.load_world(self.prod, 'greyfen')[0]

    def test_preview_of_a_first_import(self):
        info = self.publisher.preview()
        self.assertEqual((info['world'], info['release'], info['errors'], info['changed']), ('greyfen', 0, [], True))
        self.assertEqual(info['summary']['world']['cells'], {'added': 1, 'changed': 0, 'removed': 0})
        npcs = self.dev.execute('SELECT count(*) FROM live.npcs').fetchone()[0]
        self.assertEqual(info['summary']['live']['NPCs']['added'], npcs)
        self.assertTrue(any('not in maintenance' in b for b in info['blocked']))
        with self.assertRaises(P.NotInMaintenance):
            self.push()

    def test_push_makes_live_match_dev_and_tells_the_game(self):
        self.maintenance()
        listener = W.connect(self.live, 'game')
        listener.execute('LISTEN ratw_release')
        result = self.push(note='First release')
        self.assertEqual(result['release'], 1)
        dev, _ = S.load_world(self.dev, 'greyfen')
        self.assertEqual(self.prod_world(), dev)
        note = next(listener.notifies(timeout=2, stop_after=1))
        self.assertEqual(note.channel, 'ratw_release')
        self.assertIn('"release": 1', note.payload)
        # The release was compiled for the game: exactly the exporter's streamed layout, readable by the game role.
        build_id, release, files = listener.execute('SELECT id, release, files FROM world.builds').fetchone()
        self.assertEqual(release, 1)
        self.assertIn(f'"build": {build_id}', note.payload)
        expected = E.export_files(self.prod_world(), S.load_roster(self.prod), stream=True)
        self.assertEqual(files, {'world.ratw': expected['world.ratw']})
        streamed = B.game_files(listener, build_id)
        self.assertEqual(streamed, {k: v for k, v in expected.items() if k.startswith(('world.ratw', 'cells/', 'seams/'))})
        listener.close()
        self.assertEqual(self.publisher.preview()['changed'], False)
        with self.assertRaisesRegex(P.PublishError, 'nothing to import'):
            self.push()
        releases = self.publisher.preview()['releases']
        self.assertEqual([(r['number'], r['by'], r['note'], r['kind']) for r in releases], [(1, 'Ada', 'First release', 'import')])

    def test_wrong_passwords_then_lockout(self):
        self.maintenance()
        for _ in range(P.MAX_FAILURES):
            with self.assertRaises(P.WrongPassword):
                self.push('guess')
        with self.assertRaises(P.Locked):
            self.push('password')                     # Even the right one, for the rest of the window.
        self.assertIsNone(P.the_world(self.prod, 'PROD'))

    def test_only_what_changed_is_pushed(self):
        self.maintenance()
        self.push()
        dev, _ = S.load_world(self.dev, 'greyfen')
        before = dev['cells'][0]['terrain'][2][3]
        self.edit(('tile:3,2', None if before == '.' else before, '~'))
        summary = self.publisher.preview()['summary']
        self.assertEqual(summary['world'], {'terrain blocks': {'added': 0, 'changed': 1, 'removed': 0}})
        self.assertEqual(summary['live'], {})
        self.assertEqual(self.push()['release'], 2)
        self.assertEqual(self.prod_world()['cells'][0]['terrain'][2][3], '~')

    def test_what_changed_in_play_is_kept_and_comes_back_to_dev(self):
        self.maintenance()
        self.push()
        # In play, an NPC's life changed (renamed here to stand in for deaths, events, player dealings).
        self.prod.execute("UPDATE live.npcs SET name = 'Changed in play' WHERE id = 'wren'")
        dev, _ = S.load_world(self.dev, 'greyfen')
        people = {p['id']: p for p in dev['people']}
        newcomer = {**people['sloe'], 'id': 'newcomer', 'name': 'Newcomer'}
        self.edit(('person:sloe', people['sloe'], {**people['sloe'], 'greeting': 'Hello from dev.'}),
                  ('person:rook', people['rook'], None),
                  ('person:newcomer', None, newcomer))
        result = self.push()
        self.assertEqual(result['summary']['live']['NPCs'], {'added': 1, 'changed': 1, 'removed': 1})
        live = {p['id']: p for p in self.prod_world()['people']}
        self.assertEqual(live['wren']['name'], 'Changed in play', "a change made live is never undone by dev's old copy")
        self.assertEqual(live['sloe']['greeting'], 'Hello from dev.')
        self.assertNotIn('rook', live, 'removed in dev, unchanged live: removed')
        self.assertEqual(live['newcomer']['name'], 'Newcomer')
        mirrored = {p['id']: p for p in S.load_world(self.dev, 'greyfen')[0]['people']}
        self.assertEqual(mirrored['wren']['name'], 'Changed in play', 'and dev ends level with live')
        self.assertEqual(result['dev']['updated'], 1)

    def test_pull(self):
        self.maintenance()
        self.push()
        self.prod.execute("UPDATE live.npcs SET purse = 7 WHERE id = 'wren'")
        self.prod.execute("UPDATE live.npc_voice SET persona = 'Changed live' WHERE npc_id = 'wren'")
        result = self.publisher.pull({'editor': 'Ada'})
        self.assertEqual(result['applied']['live']['NPCs'], {'added': 0, 'changed': 1, 'removed': 0})
        self.assertEqual(self.dev.execute("SELECT purse FROM live.npcs WHERE id = 'wren'").fetchone()[0], 7)
        self.assertEqual(self.publisher.pull({'editor': 'Ada'})['applied'], {'world': {}, 'live': {}}, 'pulling again changes nothing')

    def test_problems_in_dev_block_the_push(self):
        self.maintenance()
        dev, _ = S.load_world(self.dev, 'greyfen')
        self.edit(('spawn', dev['spawn'], None))
        info = self.publisher.preview()
        self.assertTrue(any('spawn' in e for e in info['errors']))
        with self.assertRaises(P.PublishError) as raised:
            self.push()
        self.assertTrue(any('spawn' in e for e in raised.exception.errors))

    def test_rollback_restores_an_earlier_world(self):
        self.maintenance()
        self.push()
        first = self.prod_world()
        before = first['cells'][0]['terrain'][2][3]
        self.edit(('tile:3,2', None if before == '.' else before, '~'))
        self.push()
        self.assertEqual(self.prod_world()['cells'][0]['terrain'][2][3], '~')
        with self.assertRaises(P.WrongPassword):
            self.publisher.rollback({'release': 1, 'password': 'nope', 'editor': 'Ada'})
        result = self.publisher.rollback({'release': 1, 'password': 'password', 'editor': 'Ada'})
        self.assertEqual(result['release'], 3)
        self.assertEqual([c['terrain'] for c in self.prod_world()['cells']], [c['terrain'] for c in first['cells']])
        latest = self.publisher.preview()['releases'][0]
        self.assertEqual((latest['kind'], latest['restored']), ('rollback', 1))
        builds = self.prod.execute('SELECT release, files FROM world.builds ORDER BY id').fetchall()
        self.assertEqual([b[0] for b in builds], [1, 2, 3])
        self.assertEqual(builds[2][1], builds[0][1])
        # The rollback is live's own change: the next pull brings it to dev.
        plan = WS.diff(self.devkey)
        self.assertEqual([c['id'] for c in plan['liveChanges']], ['world.terrain_chunks/0/0'])

    def test_dev_build(self):
        """A dev build for a local server, made with the editor's own login."""
        build_id, count = B.build(self.dev, 'test')
        self.assertEqual(count, 6)                              # six cells, each a row
        self.assertEqual(B.latest(self.dev)['id'], build_id)
        # Filling a profession slot from the roster is recorded in that database's roster.
        roster = S.load_roster(self.dev)
        free = {**roster['characters'][0], 'id': 'free_guard', 'name': 'Free Guard', 'assignment': None, 'profession': ''}
        S.save_roster(self.dev, {**roster, 'characters': roster['characters'] + [free]})
        dev, _ = S.load_world(self.dev, 'greyfen')
        slot = {'id': 'gate_guard', 'name': 'Gate guard', 'profession': 'guard', 'workLabel': '', 'route': '', 'paid': True,
                'purse': 0, 'herbs': 0, 'meals': 0, 'hours': {'start': 8, 'end': 17},
                'home': dev['spawn'], 'work': dev['spawn'], 'evening': dev['spawn']}
        self.edit(('slot:gate_guard', None, slot))
        B.build(self.dev, 'test')
        holder = next(c for c in S.load_roster(self.dev)['characters'] if c['id'] == 'free_guard')
        self.assertEqual(holder['assignment'], {'world': 'greyfen', 'slot': 'gate_guard'})

    def test_an_unchanged_world_is_not_built_again(self):
        """Launching a dev world again with nothing changed reuses its build; any edit, live layer or roster change doesn't."""
        build_id, count = B.build(self.dev, 'test')
        self.assertEqual(B.current(self.dev), (build_id, count))
        before = S.load_world(self.dev, 'greyfen')[0]['cells'][0]['terrain'][2][3]
        self.edit(('tile:3,2', None if before == '.' else before, '~'))
        self.assertIsNone(B.current(self.dev), 'an edit makes the build out of date')
        build_id, count = B.build(self.dev, 'test')
        self.assertEqual(B.current(self.dev), (build_id, count))
        self.dev.execute("INSERT INTO live.factions (world_id, id, name, color, position) VALUES ('greyfen', 'wardens', 'Wardens', '#2255aa', 99)")
        self.assertIsNone(B.current(self.dev), 'a new faction makes the build out of date')
        B.build(self.dev, 'test')
        self.assertIsNotNone(B.current(self.dev))
        roster = S.load_roster(self.dev)
        S.save_roster(self.dev, {**roster, 'characters': roster['characters'][1:]})
        self.assertIsNone(B.current(self.dev), 'a roster change makes the build out of date')


if __name__ == '__main__':
    unittest.main()
