#!/usr/bin/env python3
"""Push to live: password, validation, exact copy of the world, live data kept,
refusals, releases and rollback. Uses scratch DEV and PROD databases dropped
afterwards; skipped when the local PostgreSQL is not running."""
import secrets
import unittest

import live_edit as L
import map_editor as E
import publish as P
import world_build as B
import roster as R
import world_db as W
import world_store as S
from test_world_db import database_available, superuser
from test_world_store import greyfen


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class PublishTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        tag = secrets.token_hex(4)
        cls.names = {'dev': f'ratw_test_dev_{tag}', 'prod': f'ratw_test_prod_{tag}'}
        with superuser() as su:
            for name in cls.names.values():
                su.execute(f'CREATE DATABASE {name} OWNER ratw_owner')
                su.execute(f'GRANT CONNECT ON DATABASE {name} TO ratw_editor, ratw_publisher, ratw_game')
        cls.owner = {db: W.connect(db, 'owner', dbname=name) for db, name in cls.names.items()}
        for conn in cls.owner.values():
            W.migrate(conn)
        W.seed_publish_password(cls.owner['prod'])
        cls.dev = W.connect('dev', 'editor', dbname=cls.names['dev'])
        S.save_roster(cls.dev, R.load())
        cls.publisher = P.Publisher(lambda db: W.connect(db, 'publisher', dbname=cls.names[db]))

    @classmethod
    def tearDownClass(cls):
        cls.dev.close()
        for conn in cls.owner.values():
            conn.close()
        with superuser() as su:
            for name in cls.names.values():
                su.execute(f'DROP DATABASE {name} WITH (FORCE)')

    def setUp(self):
        for conn in self.owner.values():
            conn.execute('DELETE FROM world.worlds')
            conn.execute('DELETE FROM admin.releases')
            conn.execute('DELETE FROM admin.publish_attempts')
        S.save_world(self.dev, greyfen(), None, create=True)

    def push(self, password='password', note=''):
        return self.publisher.push({'password': password, 'editor': 'Ada', 'note': note})

    def edit(self, *ops):
        return L.apply_edit(self.dev, 'test', 'Ada', 'Test edit', [{'key': k, 'before': b, 'after': a} for k, b, a in ops])

    def prod_world(self):
        return S.load_world(self.owner['prod'], 'greyfen')[0]

    def test_preview_of_a_first_push(self):
        info = self.publisher.preview()
        self.assertEqual((info['world'], info['release'], info['errors'], info['changed']), ('greyfen', 0, [], True))
        self.assertEqual(info['summary']['world']['cells'], {'added': 1, 'changed': 0, 'removed': 0})
        self.assertEqual(info['summary']['live']['NPCs']['added'], 10)

    def test_push_makes_prod_match_dev_and_tells_the_game(self):
        listener = W.connect('prod', 'game', dbname=self.names['prod'])
        listener.execute('LISTEN ratw_release')
        result = self.push(note='First release')
        self.assertEqual(result['release'], 1)
        dev, _ = S.load_world(self.dev, 'greyfen')
        self.assertEqual(self.prod_world(), dev)
        note = next(listener.notifies(timeout=2, stop_after=1))
        self.assertEqual(note.channel, 'ratw_release')
        self.assertIn('"release": 1', note.payload)
        # The release was compiled for the game: exactly the exporter's streamed layout, readable by the game role:
        # the manifest in world.builds, each cell (header, file, seams) in world.build_cells.
        build_id, release, files = listener.execute('SELECT id, release, files FROM world.builds').fetchone()
        self.assertEqual(release, 1)
        self.assertIn(f'"build": {build_id}', note.payload)
        expected = E.export_files(self.prod_world(), S.load_roster(self.owner['prod']), stream=True)
        self.assertEqual(files, {'world.ratw': expected['world.ratw']})
        self.assertTrue(files['world.ratw'].startswith('RATW_WORLD 3\n'))
        streamed = B.game_files(listener, build_id)
        self.assertEqual(streamed, {k: v for k, v in expected.items() if k.startswith(('world.ratw', 'cells/', 'seams/'))})
        header = listener.execute("SELECT header FROM world.build_cells WHERE build_id = %s AND cell_id = 'town'", (build_id,)).fetchone()[0]
        self.assertTrue(header.endswith('grid:\n') and 'size: 64 40' in header)
        listener.close()
        self.assertEqual(self.publisher.preview()['changed'], False)
        with self.assertRaisesRegex(P.PublishError, 'nothing to push'):
            self.push()
        releases = self.publisher.preview()['releases']
        self.assertEqual([(r['number'], r['by'], r['note'], r['kind']) for r in releases], [(1, 'Ada', 'First release', 'push')])

    def test_wrong_passwords_then_lockout(self):
        for _ in range(P.MAX_FAILURES):
            with self.assertRaises(P.WrongPassword):
                self.push('guess')
        with self.assertRaises(P.Locked):
            self.push('password')                     # Even the right one, for the rest of the window.
        self.assertIsNone(P.the_world(self.owner['prod'], 'PROD'))

    def test_only_what_changed_is_pushed(self):
        self.push()
        dev, _ = S.load_world(self.dev, 'greyfen')
        before = dev['cells'][0]['terrain'][2][3]
        self.edit(('tile:3,2', None if before == '.' else before, '~'))
        summary = self.publisher.preview()['summary']
        self.assertEqual(summary['world']['terrain blocks'], {'added': 0, 'changed': 1, 'removed': 0})
        self.assertEqual(summary['world']['cells'], {'added': 0, 'changed': 0, 'removed': 0})
        self.assertEqual(self.push()['release'], 2)
        self.assertEqual(self.prod_world()['cells'][0]['terrain'][2][3], '~')

    def test_live_npcs_are_never_overwritten(self):
        self.push()
        prod = self.owner['prod']
        # In play, an NPC's life changed (renamed here to stand in for deaths, events, player dealings).
        prod.execute("UPDATE live.npcs SET name = 'Changed in play' WHERE world_id = 'greyfen' AND position = 0")
        npc_id = prod.execute("SELECT id FROM live.npcs WHERE world_id = 'greyfen' AND position = 0").fetchone()[0]
        dev, _ = S.load_world(self.dev, 'greyfen')
        first = next(p for p in dev['people'] if p['id'] == npc_id)
        second, third = dev['people'][1], dev['people'][2]
        newcomer = {**second, 'id': 'newcomer', 'name': 'Newcomer'}
        self.edit((f'person:{npc_id}', first, {**first, 'name': 'Renamed in DEV'}),
                  (f'person:{second["id"]}', second, {**second, 'greeting': 'Hello from DEV.'}),
                  (f'person:{third["id"]}', third, None),
                  ('person:newcomer', None, newcomer))
        result = self.push()
        self.assertEqual(result['summary']['live']['NPCs'], {'added': 1, P.LIVE_KEPT: 3})
        people = {p['id']: p for p in self.prod_world()['people']}
        self.assertEqual(people[npc_id]['name'], 'Changed in play')
        self.assertNotEqual(people[second['id']]['greeting'], 'Hello from DEV.')
        self.assertIn(third['id'], people, 'An NPC removed in DEV stays alive in PROD')
        self.assertEqual(people['newcomer']['name'], 'Newcomer')
        # And DEV now mirrors the live NPCs, keeping its own newcomer.
        self.assertEqual(result['pulled']['skipped'], [])
        mirrored = {p['id']: p for p in S.load_world(self.dev, 'greyfen')[0]['people']}
        self.assertEqual(mirrored[npc_id]['name'], 'Changed in play')
        self.assertIn(third['id'], mirrored)
        self.assertIn('newcomer', mirrored)

    def test_dev_copies_the_live_npc_state(self):
        self.push()
        prod = self.owner['prod']
        prod.execute("UPDATE live.npcs SET purse = 7 WHERE world_id = 'greyfen' AND position = 1")
        prod.execute("""INSERT INTO live.npc_state (world_id, npc_id, state) VALUES
                        ('greyfen', 'wren', '{"cell": "shop", "x": 3.5, "y": 4.5, "cash": 55}')""")
        prod.execute("UPDATE live.economy SET treasury = 777 WHERE world_id = 'greyfen'")
        before = self.dev.execute('SELECT max(seq) FROM world.edits').fetchone()[0] or 0
        result = self.publisher.pull({'editor': 'Ada'})
        self.assertEqual((result['copied']['NPCs'], result['copied']['NPC running states'], result['copied']['town economy']), (1, 1, 1))
        dev = S.load_world(self.dev, 'greyfen')[0]
        self.assertEqual(dev['people'][1]['purse'], 7)
        self.assertEqual(dev['economy']['treasury'], 777)
        state = self.dev.execute("SELECT state, updated_at > now() - interval '1 minute' FROM live.npc_state WHERE npc_id = 'wren'").fetchone()
        self.assertEqual((state[0]['cash'], state[1]), (55, True))
        # Open editors hear about it through the live-edit log.
        keys = [r[0] for r in self.dev.execute('SELECT key FROM world.edits WHERE seq > %s', (before,)).fetchall()]
        self.assertIn('economy', keys)
        self.assertEqual(self.publisher.pull({'editor': 'Ada'})['copied']['NPCs'], 0, 'Copying again changes nothing')

    def test_npc_layers_are_added_only_and_copied_back(self):
        dev = self.owner['dev']
        cell = dev.execute("SELECT id FROM world.areas WHERE world_id = 'greyfen' ORDER BY id LIMIT 1").fetchone()[0]
        npc_id = dev.execute("SELECT id FROM live.npcs WHERE world_id = 'greyfen' AND position = 1").fetchone()[0]
        dev.execute("""INSERT INTO live.npc_areas (world_id, id, name, kind, area, tiles)
                       VALUES ('greyfen', 'square', 'Square', 'wander', %s, '[[3, 4], [4, 4]]')""", (cell,))
        dev.execute("""INSERT INTO live.spawns (world_id, id, name, area_id, template_id, count)
                       VALUES ('greyfen', 'rats', 'Rats', 'square', %s, 2)""", (npc_id,))
        dev.execute("UPDATE live.npcs SET wander_area = 'square' WHERE world_id = 'greyfen' AND id = %s", (npc_id,))
        routes = dev.execute("SELECT count(*) FROM live.patrol_routes WHERE world_id = 'greyfen'").fetchone()[0]
        result = self.push()
        live = result['summary']['live']
        self.assertEqual((live['NPC areas'], live['spawn rules']), ({'added': 1, P.LIVE_KEPT: 0}, {'added': 1, P.LIVE_KEPT: 0}))
        self.assertEqual(live['patrol routes'], {'added': routes, P.LIVE_KEPT: 0})
        prod = self.owner['prod']
        self.assertEqual(prod.execute("SELECT wander_area FROM live.npcs WHERE world_id = 'greyfen' AND id = %s", (npc_id,)).fetchone()[0], 'square')
        # Once live, the Dungeon Master runs them: DEV's changes are not pushed, and DEV takes PROD's back.
        prod.execute("UPDATE live.npc_areas SET tiles = '[[5, 5]]' WHERE id = 'square'")
        prod.execute("UPDATE live.spawns SET count = 5 WHERE id = 'rats'")
        prod.execute("""INSERT INTO live.npc_areas (world_id, id, name, kind, area, tiles)
                        VALUES ('greyfen', 'docks', 'Docks', 'plan', %s, '[[1, 1]]')""", (cell,))
        prod.execute("UPDATE live.npcs SET wander_area = 'docks' WHERE world_id = 'greyfen' AND id = %s", (npc_id,))
        dev.execute("UPDATE live.npc_areas SET name = 'Renamed in DEV' WHERE id = 'square'")
        dev.execute("DELETE FROM live.spawns WHERE id = 'rats'")
        dev.execute("""INSERT INTO live.npc_areas (world_id, id, name, kind, area) VALUES ('greyfen', 'field', 'Field', 'plan', %s)""", (cell,))
        self.edit((f'person:{npc_id}', S.load_world(self.dev, 'greyfen')[0]['people'][1],
                   {**S.load_world(self.dev, 'greyfen')[0]['people'][1], 'greeting': 'Changed so there is a push.'}))
        result = self.push()
        self.assertEqual(result['summary']['live']['NPC areas'], {'added': 1, P.LIVE_KEPT: 2})
        self.assertEqual(result['summary']['live']['spawn rules'], {'added': 0, P.LIVE_KEPT: 1})
        self.assertEqual(prod.execute("SELECT name, tiles FROM live.npc_areas WHERE id = 'square'").fetchone(), ('Square', [[5, 5]]))
        self.assertEqual(prod.execute("SELECT count FROM live.spawns WHERE id = 'rats'").fetchone()[0], 5)
        self.assertEqual(dev.execute("SELECT name, tiles FROM live.npc_areas WHERE id = 'square'").fetchone(), ('Square', [[5, 5]]))
        self.assertEqual(dev.execute("SELECT count FROM live.spawns WHERE id = 'rats'").fetchone()[0], 5)
        self.assertEqual(dev.execute("SELECT wander_area FROM live.npcs WHERE world_id = 'greyfen' AND id = %s", (npc_id,)).fetchone()[0], 'docks')
        self.assertIsNotNone(dev.execute("SELECT 1 FROM live.npc_areas WHERE id = 'field'").fetchone(), 'DEV keeps its own new area')
        # A patrol route changed in play comes back to DEV through the live-edit log.
        route = prod.execute("SELECT id FROM live.patrol_routes WHERE world_id = 'greyfen' ORDER BY id LIMIT 1").fetchone()
        if route:
            prod.execute("UPDATE live.patrol_routes SET name = 'Night round' WHERE id = %s", route)
            pulled = self.publisher.pull({'editor': 'Ada'})
            self.assertEqual(pulled['copied']['patrol routes'], 1)
            self.assertEqual(dev.execute("SELECT name FROM live.patrol_routes WHERE id = %s", route).fetchone()[0], 'Night round')

    def test_factions_are_added_only_and_copied_back(self):
        dev = self.owner['dev']
        project = S.load_world(self.dev, 'greyfen')[0]
        town = {k: v for k, v in project['cells'][0].items() if k not in ('terrain', 'heights')}
        watch, guild = {'id': 'watch', 'name': 'Town Watch', 'color': '#aabbcc'}, {'id': 'guild', 'name': 'Tallow Guild', 'color': '#ccbbaa'}
        self.edit(('faction:watch', None, watch), ('faction:guild', None, guild),
                  (f'cell:{town["id"]}', town, {**town, 'territory': {**town['territory'], 'claims': ['watch']}}))
        dev.execute("UPDATE live.factions SET kind = 'city' WHERE id = 'watch'")
        dev.execute("INSERT INTO live.faction_members (world_id, faction_id, npc_id, rank) VALUES ('greyfen', 'watch', 'sloe', 'sergeant')")
        dev.execute("""INSERT INTO live.faction_relations (world_id, faction_id, other_id, disposition, stance, reason)
                       VALUES ('greyfen', 'watch', 'guild', 40, 'friendly', 'They pay their dues.')""")
        live = self.push()['summary']['live']
        self.assertEqual([live[k]['added'] for k in ('factions', 'territory claims', 'faction members', 'faction relations')], [2, 1, 1, 1])
        prod = self.owner['prod']
        self.assertEqual(prod.execute("SELECT kind FROM live.factions WHERE id = 'watch'").fetchone()[0], 'city')
        # In play the Dungeon Master changes them; DEV's own changes are not pushed over those.
        prod.execute("UPDATE live.faction_relations SET disposition = -60, stance = 'hostile'")
        prod.execute("UPDATE live.faction_claims SET tiles = '[[5, 5]]'")
        prod.execute("UPDATE live.factions SET name = 'The Watch' WHERE id = 'watch'")
        dev.execute("UPDATE live.faction_relations SET reason = 'Changed in DEV'")
        cells = S.load_world(self.dev, 'greyfen')[0]['cells']
        self.edit(('faction:hunters', None, {'id': 'hunters', 'name': 'Hunters', 'color': '#123456'}))
        result = self.push()
        live = result['summary']['live']
        self.assertEqual((live['factions'], live['faction relations']), ({'added': 1, P.LIVE_KEPT: 1}, {'added': 0, P.LIVE_KEPT: 1}))
        self.assertEqual(prod.execute("SELECT disposition, reason FROM live.faction_relations").fetchone(), (-60, 'They pay their dues.'))
        # DEV copies PROD's back, through the edit log for what the editor shows.
        self.assertEqual(result['pulled']['copied']['factions'], 1)
        self.assertEqual(dev.execute("SELECT name FROM live.factions WHERE id = 'watch'").fetchone()[0], 'The Watch')
        self.assertEqual(dev.execute("SELECT tiles FROM live.faction_claims").fetchone()[0], [[5, 5]])
        self.assertEqual(dev.execute("SELECT disposition, stance FROM live.faction_relations").fetchone(), (-60, 'hostile'))
        self.assertEqual([f['name'] for f in S.load_world(self.dev, 'greyfen')[0]['factions']], ['The Watch', 'Tallow Guild', 'Hunters'])

    def test_release_that_would_strand_live_npcs_is_refused(self):
        self.push()
        dev, _ = S.load_world(self.dev, 'greyfen')
        room = dev['rooms'][0]
        # In play, someone moved into the first interior; in DEV it is deleted.
        self.owner['prod'].execute("UPDATE live.npcs SET home_area = %s, home_x = 1, home_y = 1, origin = 'runtime' "
                                   "WHERE world_id = 'greyfen' AND position = 0", (room['id'],))
        self.owner['prod'].execute("INSERT INTO live.npc_areas (world_id, id, name, kind, area) VALUES ('greyfen', 'den', 'Den', 'plan', %s)",
                                   (room['id'],))
        self.owner['prod'].execute("INSERT INTO live.factions (world_id, id, name) VALUES ('greyfen', 'rats', 'Rats')")
        self.owner['prod'].execute("INSERT INTO live.faction_claims (world_id, faction_id, area) VALUES ('greyfen', 'rats', %s)", (room['id'],))
        ops = []
        for person in dev['people']:
            if any(person[k]['cell'] == room['id'] for k in ('home', 'work', 'evening')):
                moved = {**person, **{k: dict(dev['spawn']) for k in ('home', 'work', 'evening') if person[k]['cell'] == room['id']}}
                ops.append((f'person:{person["id"]}', person, moved))
        meta = {k: v for k, v in room.items() if k not in ('terrain', 'heights')}
        links = [l for l in dev['links'] if room['id'] in (l['a']['cell'], l['b']['cell'])]
        self.edit(*ops, *((f'link:{l["id"]}', l, None) for l in links), (f'room:{room["id"]}', meta, None))
        with self.assertRaises(P.PublishError) as raised:
            self.push()
        self.assertIn(room['id'], ' '.join(raised.exception.errors))
        self.assertTrue(any('NPC area Den' in e for e in raised.exception.errors))
        self.assertTrue(any('Faction rats claims' in e for e in raised.exception.errors))
        self.assertIn(room['id'], [r['id'] for r in self.prod_world()['rooms']], 'PROD is unchanged')

    def test_problems_in_dev_block_the_push(self):
        dev, _ = S.load_world(self.dev, 'greyfen')
        self.edit(('spawn', dev['spawn'], None))
        info = self.publisher.preview()
        self.assertTrue(any('spawn' in e for e in info['errors']))
        with self.assertRaises(P.PublishError) as raised:
            self.push()
        self.assertTrue(any('spawn' in e for e in raised.exception.errors))

    def test_rollback_restores_an_earlier_world(self):
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
        # The rollback has its own build of the restored world, which is what the game loads next.
        builds = self.owner['prod'].execute('SELECT release, files FROM world.builds ORDER BY id').fetchall()
        self.assertEqual([b[0] for b in builds], [1, 2, 3])
        self.assertEqual(builds[2][1], builds[0][1])
        # DEV is untouched: pushing again brings its newer world back.
        self.assertTrue(self.publisher.preview()['changed'])


    def test_dev_build(self):
        """A DEV build for a local server, made with the editor's own login."""
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
        try:
            self.edit(('slot:gate_guard', None, slot))
            B.build(self.dev, 'test')
            holder = next(c for c in S.load_roster(self.dev)['characters'] if c['id'] == 'free_guard')
            self.assertEqual(holder['assignment'], {'world': 'greyfen', 'slot': 'gate_guard'})
        finally:
            S.save_roster(self.dev, roster)


if __name__ == '__main__':
    unittest.main()
