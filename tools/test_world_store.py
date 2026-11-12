#!/usr/bin/env python3
"""Worlds and the roster in PostgreSQL: exact round trips, minimal writes, conflicts.

Runs in a scratch database dropped afterwards; skipped when the local
PostgreSQL is not running (python3 tools/world_db.py up).
"""
import copy
import json
import secrets
import threading
import time
import unittest

import psycopg

import manifest_lines as M
import map_editor as E
import roster as R
import world_db as W
import world_store as S
from test_world_db import database_available, superuser
import os

# A throwaway database's name tag (RATW_TEST_DB_TAG, default ''): a run tagged s2stage_ makes ratw_test_s2stage_*.
DB_TAG = os.environ.get('RATW_TEST_DB_TAG', '')


def greyfen():
    world = next(w for w in E.bundled_worlds() if w['id'] == 'Greyfen')
    return E.read_json(world['source'].read_text(encoding='utf-8'))


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class StoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.name = f'ratw_test_{DB_TAG}{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor')
        with W.connect('dev', 'owner', dbname=cls.name) as owner:
            W.migrate(owner)
        cls.conn = W.connect('dev', 'editor', dbname=cls.name)
        S.save_roster(cls.conn, R.load())

    @classmethod
    def tearDownClass(cls):
        cls.conn.close()
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    def setUp(self):
        self.conn.execute('DELETE FROM world.worlds')

    def create(self, project):
        return S.save_world(self.conn, project, None, create=True)

    def test_greyfen_round_trip_produces_identical_game_files(self):
        source = greyfen()
        self.create(source)
        loaded, revision = S.load_world(self.conn, 'greyfen')
        self.assertEqual(revision, 1)
        roster = R.load()
        before, after = E.export_files(source, roster), E.export_files(loaded, roster)
        del before['atlas.json'], after['atlas.json']     # The embedded source spells out default lighting.
        self.assertEqual(before, after)

    def test_resaving_unchanged_world_writes_nothing(self):
        self.create(greyfen())
        loaded, _ = S.load_world(self.conn, 'greyfen')
        p, _ = E.check_project(loaded)
        with self.conn.transaction():
            self.assertEqual(S.write_rows(self.conn, S.project_rows(p, 128), S.stored_rows(self.conn, 'greyfen')), 0)

    def test_one_tile_edit_writes_one_chunk(self):
        self.create(greyfen())
        loaded, revision = S.load_world(self.conn, 'greyfen')
        town = loaded['cells'][0]['terrain']
        town[3] = town[3][:5] + '~' + town[3][6:]
        p, _ = E.check_project(loaded)
        with self.conn.transaction():
            self.assertEqual(S.write_rows(self.conn, S.project_rows(p, 128), S.stored_rows(self.conn, 'greyfen')), 1)
            raise psycopg.Rollback
        self.assertEqual(S.save_world(self.conn, loaded, revision), revision + 1)
        self.assertEqual(S.load_world(self.conn, 'greyfen')[0]['cells'][0]['terrain'][3][5], '~')

    def test_a_whole_world_save_keeps_npcs_the_game_server_made_since_it_was_read(self):
        # (A dev world run as the server, doc 66: births and newcomers land in live.npcs without raising the world's
        # revision, so a project read before them, e.g. the generator's or apply_terrain's, must not delete them.)
        self.create(greyfen())
        loaded, revision = S.load_world(self.conn, 'greyfen')
        columns = [c for c in S.TABLES['live.npcs'][1] if c not in ('id',)]
        self.conn.execute(f"INSERT INTO live.npcs (id, origin, {', '.join(columns)}) SELECT 'born_x', 'runtime', "
                          f"{', '.join(columns)} FROM live.npcs WHERE id = 'wren'")
        gone = next(p['id'] for p in loaded['people'] if p['id'] != 'wren')
        loaded['people'] = [p for p in loaded['people'] if p['id'] != gone]       # An authored NPC removed on purpose.
        S.save_world(self.conn, loaded, revision)
        ids = {r[0] for r in self.conn.execute('SELECT id FROM live.npcs').fetchall()}
        self.assertIn('born_x', ids, 'born while the project was open: kept')
        self.assertNotIn(gone, ids, 'an authored NPC the save leaves out still goes')

    def test_loads_after_the_first_reuse_the_ground_but_never_a_stale_one(self):
        self.create(greyfen())
        first, revision = S.load_world(self.conn, 'greyfen')
        again, _ = S.load_world(self.conn, 'greyfen')
        self.assertEqual(first, again)
        again['cells'][0]['terrain'][0] = 'changed by a caller'
        self.assertEqual(S.load_world(self.conn, 'greyfen')[0], first)          # Each load gets its own copy.
        # Made again from scratch with other ground, at the same revision number: the new ground, not the kept one.
        self.conn.execute('DELETE FROM world.worlds')
        other = copy.deepcopy(first)
        row = other['cells'][0]['terrain'][2]
        other['cells'][0]['terrain'][2] = '~' + row[1:]
        self.create(other)
        remade, remade_revision = S.load_world(self.conn, 'greyfen')
        self.assertEqual(remade_revision, revision)
        self.assertEqual(remade['cells'][0]['terrain'][2][0], '~')

    def test_stale_saves_and_duplicate_creates_are_refused(self):
        self.create(greyfen())
        loaded, revision = S.load_world(self.conn, 'greyfen')
        S.save_world(self.conn, loaded, revision)
        with self.assertRaises(S.ConflictError):
            S.save_world(self.conn, loaded, revision)     # Someone else saved since this copy was opened.
        with self.assertRaises(S.ConflictError):
            self.create(greyfen())

    def test_recutting_cells_moves_edges_in_one_save(self):
        demo = E.demo_project()
        demo['id'] = 'juniper'
        self.create(demo)
        loaded, revision = S.load_world(self.conn, 'juniper')
        # Widen the first cell and shrink its neighbour (the ground moves with the edge): overlapping only part-way
        # through the save.
        a, b = loaded['cells'][0], loaded['cells'][1]
        a['terrain'] = [ra + rb[:8] for ra, rb in zip(a['terrain'], b['terrain'])]
        b['terrain'] = [rb[8:] for rb in b['terrain']]
        a['width'] += 8
        b['x'] += 8
        b['width'] -= 8
        S.save_world(self.conn, loaded, revision)
        cells = S.load_world(self.conn, 'juniper')[0]['cells']
        self.assertEqual((cells[0]['width'], cells[1]['x']), (40, 40))

    def test_deleting_an_interior_removes_its_doors(self):
        self.create(greyfen())
        loaded, revision = S.load_world(self.conn, 'greyfen')
        room = loaded['rooms'][0]
        for person in loaded['people']:                    # Anyone living or working there moves out first.
            for k in ('home', 'work', 'evening'):
                if person[k]['cell'] == room['id']:
                    person[k] = dict(loaded['spawn'])
        loaded['rooms'] = [r for r in loaded['rooms'] if r['id'] != room['id']]
        loaded['links'] = [l for l in loaded['links'] if room['id'] not in (l['a']['cell'], l['b']['cell'])]
        S.save_world(self.conn, loaded, revision)
        again = S.load_world(self.conn, 'greyfen')[0]
        self.assertNotIn(room['id'], [r['id'] for r in again['rooms']])
        self.assertEqual(len(again['links']), len(loaded['links']))

    def test_npcs_without_a_map_round_trip_and_export(self):
        # Doc 66, 2: no home (unplaced), a home and no work (jobless), a job without a workplace, in rows and the manifest.
        p = greyfen()
        people = {x['id']: x for x in p['people']}
        people['rook'].update(home=None, work=None, evening=None)
        people['linden'].update(work=None, evening=None)
        self.create(p)
        loaded, revision = S.load_world(self.conn, 'greyfen')
        again = {x['id']: x for x in loaded['people']}
        self.assertEqual((again['rook']['home'], again['rook']['work'], again['rook']['evening']), (None, None, None))
        self.assertEqual((again['linden']['work'], again['linden']['evening']), (None, None))
        self.assertEqual(again['linden']['home'], people['linden']['home'])
        rows = dict(self.conn.execute("SELECT id, home_area FROM live.npcs WHERE id IN ('rook', 'linden')").fetchall())
        self.assertEqual(rows, {'rook': None, 'linden': 'cottage'})
        manifest = E.export_files(loaded, R.load())['world.ratw']
        rook = next(l for l in manifest.split('\n') if l.startswith('resident "rook"'))
        self.assertTrue(rook.endswith(' "-" 0 0 "-" 0 0 "-" 0 0'), rook)
        linden = next(l for l in manifest.split('\n') if l.startswith('resident "linden"'))
        self.assertTrue(linden.endswith(' "cottage" 9.5 7.5 "-" 0 0 "-" 0 0'), linden)
        # Given a home again.
        again['rook']['home'] = dict(people['linden']['home'])
        S.save_world(self.conn, loaded, revision)
        self.assertEqual(self.conn.execute("SELECT home_area FROM live.npcs WHERE id = 'rook'").fetchone()[0], 'cottage')

    def test_a_world_of_interiors_without_a_spawn_saves_but_does_not_export(self):
        # Ridgemere 2.0 (doc 66): every building kept, no map and no spawn yet.
        p = greyfen()
        for person in p['people']:
            for k in ('home', 'work', 'evening'):
                if person[k]['cell'] == 'town':
                    person[k] = None
        p['herbPatch'] = None
        p['routes'] = []
        for person in p['people']:
            person['route'] = ''
        p['links'] = [l for l in p['links'] if 'town' not in (l['a']['cell'], l['b']['cell'])]
        p['cells'], p['spawn'] = [], None
        _, warnings = E.check_project(p, for_game=False, playable=False)
        self.assertTrue(any('no player spawn' in w for w in warnings), warnings)
        self.create(p)
        loaded, _ = S.load_world(self.conn, 'greyfen')
        self.assertEqual((loaded['cells'], loaded['spawn']), ([], None))
        with self.assertRaisesRegex(E.ValidationError, 'spawn'):
            E.export_files(loaded, R.load(), stream=True)
        loaded['spawn'] = {'cell': 'longhouse', 'x': 2, 'y': 9}       # A spawn indoors is enough: no outdoor cell needed.
        files = E.export_files(loaded, R.load(), stream=True)
        self.assertIn('spawn "longhouse" 2.5 9.5', files['world.ratw'])

    def test_larger_terrain_blocks(self):
        self.conn.execute("INSERT INTO world.worlds (id, name, chunk_size) VALUES ('big', 'Big', 512)")
        demo = {**E.demo_project(), 'id': 'big'}
        S.save_world(self.conn, demo, 0)
        rows = self.conn.execute("SELECT size, char_length(glyphs) FROM world.terrain_chunks WHERE world_id = 'big'").fetchall()
        self.assertEqual(rows, [(512, 512 * 512)])
        self.assertEqual([c['terrain'] for c in S.load_world(self.conn, 'big')[0]['cells']],
                         [c['terrain'] for c in E.to_v3(copy.deepcopy(demo))['cells']])

    def test_slots_need_professions_from_the_roster(self):
        p = greyfen()
        p['slots'] = [{'id': 'odd_job', 'name': 'Odd job', 'profession': 'astronomer', 'workLabel': '', 'route': '', 'paid': True,
                       'purse': 0, 'herbs': 0, 'meals': 0, 'hours': {'start': 8, 'end': 17},
                       'home': p['spawn'], 'work': p['spawn'], 'evening': p['spawn']}]
        with self.assertRaisesRegex(S.StoreError, 'astronomer'):
            self.create(p)

    # ---- Doc 79, 14 (Phase 9): the slim economy -------------------------------------------------------------------

    def economy_lines(self, project):
        lines = E.export_files(project, R.load())['world.ratw'].split('\n')
        return [l for l in lines if l.split(' ', 1)[0] in M.ECONOMY_KINDS]

    def database_economy_lines(self):
        manifest = self.conn.execute("SELECT live.people_manifest('greyfen')").fetchone()[0].split('\n')
        return [l for l in manifest if l.split(' ', 1)[0] in M.ECONOMY_KINDS]

    def test_the_slim_economy_round_trips_and_writes_the_manifests_lines(self):
        # The kept columns, the rules' integers apart from the host's overrides in the same jsonb; the exporter's lines
        # (tools/manifest_lines.py) and live.people_manifest's alike (the overrides as JSON).
        self.assertTrue(S.slim_supported(self.conn))
        self.assertIs(S.tables_for(self.conn), S.SLIM_TABLES)
        p = greyfen()
        overrides = {'prosperityCause': 'the silver strike', 'neverImported': ['salt'],
                     'status': {'caps': {'rewardsADay': 300}}, 'market': {'events': {'perWeek': 1}}}
        p['economy'] = {'treasury': 900, 'storeHerbs': 80, 'storeMeals': 40, 'needsPaused': True,
                        'seasons': {'grain': {'price': [120, 100, 80, 130], 'shelf': [100, 100, 100, 100]}},
                        'rules': {'ripple': 40, 'prosperity': 35}, 'services': {'groom_and_trim': {'on': False, 'price': 2, 'every': 5}},
                        'overrides': overrides}
        self.create(p)
        loaded, revision = S.load_world(self.conn, 'greyfen')
        self.assertEqual(loaded['economy'], p['economy'])
        stored = self.conn.execute("SELECT rules FROM live.economy WHERE world_id = 'greyfen'").fetchone()[0]
        self.assertEqual(stored, {'ripple': 40, 'prosperity': 35, **overrides})
        self.assertEqual(M.comparable(self.economy_lines(loaded)), M.comparable(self.database_economy_lines()))
        self.assertEqual([json.loads(l[len('overrides '):]) for l in self.economy_lines(loaded) if l.startswith('overrides ')], [overrides])
        with self.conn.transaction():
            self.assertEqual(S.write_rows(self.conn, S.project_rows(E.check_project(loaded)[0], 128, S.SLIM_TABLES),
                                          S.stored_rows(self.conn, 'greyfen')), 0, 'saved unchanged: nothing written')
            raise psycopg.Rollback
        # A dead setting an older project carries is neither refused nor written; its column keeps what it holds.
        self.conn.execute("UPDATE live.economy SET store_markup = 77, daily_herbs = 3 WHERE world_id = 'greyfen'")
        old = {**loaded['economy'], 'storeMarkup': 120, 'dailyHerbs': 9, 'dailyMeals': 9, 'gateToll': 20}
        revision = S.save_world(self.conn, {**loaded, 'economy': old}, revision)
        self.assertEqual(self.conn.execute("SELECT store_markup, daily_herbs FROM live.economy").fetchone(), (77, 3))
        self.assertEqual(S.load_world(self.conn, 'greyfen')[0]['economy'], p['economy'])
        # Back at the defaults: one economy line.
        loaded, revision = S.load_world(self.conn, 'greyfen')
        S.save_world(self.conn, {**loaded, 'economy': S.default_economy()}, revision)
        self.assertEqual(self.database_economy_lines(), ['economy 1000 100 50 10 12'])

    def test_save_economy_keeps_what_it_doesnt_know_and_patch_takes_two_writers(self):
        self.create(greyfen())
        self.conn.execute("""UPDATE live.economy SET rules = '{"nested": {"a": 1}, "steer": 1}' WHERE world_id = 'greyfen'""")
        entity = {**S.default_economy(), 'rules': {'ripple': 30}, 'overrides': {'prosperityCause': 'a fair'}, 'dailyHerbs': 4}
        after = S.save_economy(self.conn, 'greyfen', entity)
        self.assertEqual(after['rules'], {'ripple': 30}, 'the entity\'s rules are the whole of the integer rules')
        self.assertEqual(after['overrides'], {'prosperityCause': 'a fair'})
        self.assertEqual(self.conn.execute('SELECT rules FROM live.economy').fetchone()[0],
                         {'nested': {'a': 1}, 'ripple': 30, 'prosperityCause': 'a fair'}, 'what it doesn\'t know kept')
        # Phase 9's last repair: a rule's name that would split the manifest's line is refused by both writers, whoever
        # calls them; one the row already holds (written some other way) doesn't block a save, and is never printed.
        for name in ('zz\nbogus', 'a b'):
            with self.assertRaisesRegex(S.EconomyRefused, 'not a rule'):
                S.save_economy(self.conn, 'greyfen', {**entity, 'rules': {'ripple': 30, name: 5}})
            with self.assertRaisesRegex(S.EconomyRefused, 'not a rule'):
                S.patch_economy_rules(self.conn, 'greyfen', {name: 5})
        self.assertEqual(self.conn.execute('SELECT rules FROM live.economy').fetchone()[0],
                         {'nested': {'a': 1}, 'ripple': 30, 'prosperityCause': 'a fair'}, 'nothing of a refused save written')
        self.conn.execute("""UPDATE live.economy SET rules = rules || '{"zz\\nbogus": 5}' WHERE world_id = 'greyfen'""")
        held = S.economy_value(self.conn, 'greyfen')
        self.assertIn('zz\nbogus', held['rules'])
        self.assertEqual(S.save_economy(self.conn, 'greyfen', {**held, 'needsPaused': True})['needsPaused'], True, 'a held one lets a save by')
        self.assertFalse([l for l in self.conn.execute("SELECT live.people_manifest('greyfen')").fetchone()[0].split('\n')
                          if 'bogus' in l], 'and the manifest never prints it')
        self.conn.execute("""UPDATE live.economy SET rules = rules - 'zz\nbogus', needs_paused = false WHERE world_id = 'greyfen'""")
        # patch_economy_rules: one statement, so two writers' keys both land.
        other = W.connect('dev', 'editor', dbname=self.name)
        try:
            done = threading.Event()
            with self.conn.transaction():
                S.patch_economy_rules(self.conn, 'greyfen', {'prosperity': 40})
                worker = threading.Thread(target=lambda: (S.patch_economy_rules(other, 'greyfen', {'harvestBias': 5},
                                                                                drop=['prosperityCause']), done.set()))
                worker.start()
                time.sleep(.3)
                self.assertFalse(done.is_set(), 'the second waits for the first')
            worker.join(10)
            self.assertTrue(done.is_set())
        finally:
            other.close()
        rules = self.conn.execute('SELECT rules FROM live.economy').fetchone()[0]
        self.assertEqual((rules['prosperity'], rules['harvestBias'], 'prosperityCause' in rules), (40, 5, False))

    def test_an_empty_never_imported_list_is_kept(self):
        # Doc 79, Phase 9's first repair: [] says ships carry every good (in place of market.json's wharf.never, cheese);
        # it was dropped as "empty" on every write, so the world went back to cheese and the file and the database
        # disagreed.
        self.create(greyfen())
        entity = {**S.default_economy(), 'overrides': {'neverImported': []}}
        self.assertEqual(S.save_economy(self.conn, 'greyfen', entity)['overrides'], {'neverImported': []})
        self.assertEqual(self.conn.execute('SELECT rules FROM live.economy').fetchone()[0], {'neverImported': []})
        self.assertEqual(S.economy_value(self.conn, 'greyfen')['overrides'], {'neverImported': []})
        line = [l for l in self.database_economy_lines() if l.startswith('overrides ')]
        self.assertEqual(line, ['overrides {"neverImported": []}'])
        self.assertEqual(M.comparable(M.economy_lines(entity)), M.comparable(self.database_economy_lines()), 'the file says the same')
        loaded, revision = S.load_world(self.conn, 'greyfen')
        S.save_world(self.conn, loaded, revision)
        self.assertEqual(S.economy_value(self.conn, 'greyfen')['overrides'], {'neverImported': []}, 'a whole save keeps it')

    def test_a_stored_override_core_skips_blocks_no_save_or_build(self):
        # Doc 79, 14: a bad override is skipped by Core with a warning, never refused; one stored past the studio (a hand,
        # a package, a data file changed since) must not stop a whole save, a build, an NPC's save or another edit.
        import live_edit as L
        import world_build
        self.create(greyfen())
        S.patch_economy_rules(self.conn, 'greyfen', {'status': {'shares': [{'at': -100, 'shares': [40, 40, 40, 20]},
                                                                          {'at': 100, 'shares': [2, 12, 70, 16]}]},
                                                     'neverImported': ['nonsense_good']})
        loaded, revision = S.load_world(self.conn, 'greyfen')
        revision = S.save_world(self.conn, loaded, revision)
        errors, warnings = E.economy_problems(loaded['economy'])
        self.assertEqual(errors, [])
        self.assertTrue(any('sums to 140' in w for w in warnings) and any('nonsense_good' in w for w in warnings), warnings)
        self.assertTrue(E.check_economy(loaded['economy'], strict_overrides=True), 'the studio still refuses one it would write')
        self.assertTrue(world_build.build(self.conn, 'test'), 'a build')
        stored = S.economy_value(self.conn, 'greyfen')
        L.apply_edit(self.conn, 'a1', 'Ada', 'The treasury', [{'key': 'economy', 'before': stored, 'after': {**stored, 'treasury': 1200}}],
                     queue_sync=False)
        self.assertEqual(S.economy_value(self.conn, 'greyfen')['treasury'], 1200, 'another edit goes')
        with self.assertRaises(L.Rejected):
            now = S.economy_value(self.conn, 'greyfen')
            L.apply_edit(self.conn, 'a1', 'Ada', 'Bad', [{'key': 'economy', 'before': now, 'after': {**now, 'treasury': -5}}],
                         queue_sync=False)

    def test_an_npcs_band_round_trips_and_its_line_follows_its_records(self):
        p = greyfen()
        people = {q['id']: q for q in p['people']}
        people['brann']['band'] = 'poor'
        people['brann']['protected'] = True
        self.create(p)
        self.assertEqual(self.conn.execute("SELECT band_override FROM live.npcs WHERE id = 'brann'").fetchone()[0], 'poor')
        loaded, _ = S.load_world(self.conn, 'greyfen')
        self.assertEqual({q['id']: q.get('band') for q in loaded['people'] if q.get('band')}, {'brann': 'poor'})
        one = self.conn.execute("SELECT live.people_manifest('greyfen', 'brann')").fetchone()[0].split('\n')
        self.assertEqual(one[-2:], ['protected "brann"', 'band "brann" poor'])
        lines = E.export_files(loaded, R.load())['world.ratw'].split('\n')
        at = lines.index('band "brann" poor')
        self.assertEqual(lines[at - 1], 'protected "brann"', 'after its protected record, as the database writes it')
        self.conn.execute("UPDATE live.npcs SET band_override = NULL WHERE id = 'brann'")
        self.assertNotIn('band "brann" poor', self.conn.execute("SELECT live.people_manifest('greyfen')").fetchone()[0])

    def test_people_manifest_matches_the_exporter(self):
        # (Moved from test_dungeon_master, doc 79 Phase 9: the parity of the database's records and the exporter's, the
        # residents, a job holder (a profession slot) and the economy's, sorted as both sort.)
        import world_build
        roster = R.load()
        free = {**roster['characters'][0], 'id': 'free_hand', 'name': 'Free Hand', 'assignment': None, 'profession': '',
                'traits': ['steady', 'dry'], 'personality': 'Quiet.'}
        S.save_roster(self.conn, {**roster, 'characters': roster['characters'] + [free]})
        p = greyfen()
        p['economy'] = {**p['economy'], 'rules': {'ripple': 30}, 'overrides': {'prosperityCause': 'a fair'}}
        p['people'][1]['band'] = 'rich'
        self.create(p)
        project, revision = S.load_world(self.conn, 'greyfen')
        spot = project['spawn']
        project['slots'] = [{'id': 'gate', 'name': 'Gate guard', 'profession': 'guard', 'workLabel': '', 'route': '', 'paid': True,
                             'purse': 3, 'herbs': 0, 'meals': 1, 'hours': {'start': 6.5, 'end': 18.25},
                             'home': spot, 'work': spot, 'evening': spot}]
        S.save_world(self.conn, project, revision)
        world_build.build(self.conn, 'test')
        kinds = ('route', 'resident', 'story', 'joinable', 'protected', 'band', *M.ECONOMY_KINDS)
        sql = [l for l in self.conn.execute("SELECT live.people_manifest('greyfen')").fetchone()[0].split('\n')
               if l.split(' ', 1)[0] in kinds]
        files = E.export_files(S.load_world(self.conn, 'greyfen')[0], S.load_roster(self.conn))
        python = [l for l in files['world.ratw'].split('\n') if l.split(' ', 1)[0] in kinds]
        self.assertTrue(any('"free_hand"' in l for l in sql), 'the job holder is a resident')
        self.assertEqual(M.comparable(sql), M.comparable(python))
        S.save_roster(self.conn, roster)

    def test_a_missing_economy_row_is_made_from_the_defaults(self):
        # Ridgemere 2.0 has no live.economy row: its first save makes one from DEFAULT_ECONOMY (1000p), never the
        # table's column defaults (a treasury of 0).
        self.create(greyfen())
        self.conn.execute('DELETE FROM live.economy')
        loaded, revision = S.load_world(self.conn, 'greyfen')
        self.assertEqual(loaded['economy'], S.DEFAULT_ECONOMY)
        loaded['economy']['rules']['ripple'] = 10
        self.assertEqual(S.DEFAULT_ECONOMY['rules'], {}, 'the defaults are never shared')
        S.save_world(self.conn, loaded, revision)
        row = S.dict_rows(self.conn, 'SELECT * FROM live.economy', ())[0]
        self.assertEqual((row['treasury'], row['store_herbs'], row['store_meals'], row['rules']), (1000, 100, 50, {'ripple': 10}))

    def test_roster_round_trip_and_order(self):
        roster = R.load()
        self.assertEqual(S.load_roster(self.conn), roster)
        changed = copy.deepcopy(roster)
        changed['characters'].reverse()
        changed['characters'][0]['name'] = 'Renamed'
        self.assertEqual(S.save_roster(self.conn, changed), R.check_roster(changed))
        S.save_roster(self.conn, roster)
        self.assertEqual(S.load_roster(self.conn), roster)


class OlderWorld:
    """A world at an older migration (UPTO, the first not applied): loads, saves and builds; what it can't hold refused."""
    UPTO = '0061'

    @classmethod
    def setUpClass(cls):
        cls.name = f'ratw_test_{DB_TAG}{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor')
        with W.connect('dev', 'owner', dbname=cls.name) as owner:
            W.migrate(owner, [m for m in W.migration_files() if m[0] < cls.UPTO])
        cls.conn = W.connect('dev', 'editor', dbname=cls.name)
        S.save_roster(cls.conn, R.load())

    @classmethod
    def tearDownClass(cls):
        cls.conn.close()
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    def saved(self):
        self.conn.execute('DELETE FROM world.worlds')
        source = greyfen()
        S.save_world(self.conn, source, None, create=True)
        loaded, revision = S.load_world(self.conn, 'greyfen')
        p, _ = E.check_project(loaded)
        with self.conn.transaction():
            self.assertEqual(S.write_rows(self.conn, S.project_rows(p, 128, S.tables_for(self.conn)), S.stored_rows(self.conn, 'greyfen')), 0)
            raise psycopg.Rollback
        return source, loaded, revision

    def refused(self, loaded, revision, change, words):
        with self.assertRaisesRegex(S.StoreError, words):
            S.save_world(self.conn, {**loaded, 'economy': {**loaded['economy'], **change}}, revision)


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class WithoutMigration55Tests(OlderWorld, unittest.TestCase):
    """A world without migration 0055 (PROD, at 0054): the economy's starting amounts only; needs, seasons, rules refused."""
    UPTO = '0055'

    def test_loads_saves_builds_and_refuses_what_it_cant_hold(self):
        self.assertIs(S.tables_for(self.conn), S.LEGACY_TABLES)
        source, loaded, revision = self.saved()
        self.assertEqual(loaded['economy'], {**S.default_economy(), **{k: source['economy'][k] for k in ('treasury', 'storeHerbs', 'storeMeals')}})
        roster = R.load()
        before, after = E.export_files(source, roster), E.export_files(loaded, roster)
        del before['atlas.json'], after['atlas.json']
        self.assertEqual(before, after)
        self.assertEqual(S.save_world(self.conn, {**loaded, 'economy': {**loaded['economy'], 'storeMarkup': 50}}, revision), revision + 1,
                         'a dead setting is ignored, never refused')
        self.refused(loaded, revision + 1, {'needsPaused': True}, '0056')
        self.refused(loaded, revision + 1, {'rules': {'ripple': 10}}, '0058')
        import world_build
        self.assertTrue(world_build.fingerprint(self.conn, 'greyfen', S.load_roster(self.conn)))


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class WithoutMigration58Tests(OlderWorld, unittest.TestCase):
    """A world with 0057 but not 0058: needs and seasons kept; the rules and services refused, naming 0058."""
    UPTO = '0058'

    def test_loads_and_saves_and_refuses_the_rules(self):
        self.assertIs(S.tables_for(self.conn), S.SEASON_TABLES)
        _, loaded, revision = self.saved()
        grain = {'price': [120, 100, 80, 130], 'shelf': [100, 100, 100, 100]}
        revision = S.save_world(self.conn, {**loaded, 'economy': {**loaded['economy'], 'needsPaused': True, 'seasons': {'grain': grain}}},
                                revision)
        self.assertEqual(S.load_world(self.conn, 'greyfen')[0]['economy']['seasons'], {'grain': grain})
        self.refused(loaded, revision, {'rules': {'ripple': 10}}, '0058')
        with self.assertRaises(S.EconomyRefused):
            S.save_economy(self.conn, 'greyfen', {**S.default_economy(), 'overrides': {'prosperityCause': 'a fair'}})


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class WithoutMigration61Tests(OlderWorld, unittest.TestCase):
    """A world at 0060 (DEV until the user migrates it): pre-slim. The rules (prosperity too), the slider's cause, seasons,
    services and needs are kept; the host's other overrides refused, saying it needs 0061; no band is written."""
    UPTO = '0061'

    def test_pre_slim_keeps_the_kept_and_refuses_the_rest(self):
        self.assertFalse(S.slim_supported(self.conn))
        self.assertIs(S.tables_for(self.conn), S.TABLES)
        _, loaded, revision = self.saved()
        economy = {**loaded['economy'], 'rules': {'prosperity': 35, 'ripple': 40}, 'overrides': {'prosperityCause': 'a fair'}}
        revision = S.save_world(self.conn, {**loaded, 'economy': economy}, revision)
        self.assertEqual(S.load_world(self.conn, 'greyfen')[0]['economy'], economy)
        self.assertEqual(S.patch_economy_rules(self.conn, 'greyfen', {'prosperity': 50, 'prosperityCause': 'a better fair'})['prosperity'], 50)
        for change in ({'overrides': {'neverImported': ['salt']}}, {'overrides': {'status': {'caps': {}}, 'prosperityCause': 'x'}}):
            self.refused(loaded, revision + 0, change, '0061')
            with self.assertRaisesRegex(S.EconomyRefused, '0061'):
                S.save_economy(self.conn, 'greyfen', {**economy, **change})
        with self.assertRaises(S.EconomyRefused):
            S.patch_economy_rules(self.conn, 'greyfen', {'market': {'events': {'perWeek': 1}}})
        # A band in a pre-slim world (no column to hold it): refused, never dropped unsaid (doc 79, Phase 9's first
        # repair), by a whole save and by a live edit; the business switches untouched.
        import live_edit as L
        loaded, revision = S.load_world(self.conn, 'greyfen')
        loaded['people'][0]['band'] = 'rich'
        with self.assertRaisesRegex(S.EconomyRefused, 'needs migration 0061'):
            S.save_world(self.conn, loaded, revision)
        loaded['people'][0]['band'] = None
        S.save_world(self.conn, loaded, revision)
        self.assertFalse(any(q.get('band') for q in S.load_world(self.conn, 'greyfen')[0]['people']))
        key = 'person:' + loaded['people'][0]['id']
        before = L.Batch(self.conn, L.world_row(self.conn)).current(key)
        with self.assertRaisesRegex(L.Rejected, 'needs migration 0061'):
            L.apply_edit(self.conn, 'a1', 'Ada', 'A band', [{'key': key, 'before': before, 'after': {**before, 'band': 'rich'}}])
        self.assertEqual(self.conn.execute('SELECT count(*) FROM live.business_settings').fetchone()[0], 0)


if __name__ == '__main__':
    unittest.main()
