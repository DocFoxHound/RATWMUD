#!/usr/bin/env python3
"""Worlds and the roster in PostgreSQL: exact round trips, minimal writes, conflicts.

Runs in a scratch database dropped afterwards; skipped when the local
PostgreSQL is not running (python3 tools/world_db.py up).
"""
import copy
import secrets
import unittest

import psycopg

import map_editor as E
import roster as R
import world_db as W
import world_store as S
from test_world_db import database_available, superuser


def greyfen():
    world = next(w for w in E.bundled_worlds() if w['id'] == 'Greyfen')
    return E.read_json(world['source'].read_text(encoding='utf-8'))


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class StoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.name = f'ratw_test_{secrets.token_hex(4)}'
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

    def test_roster_round_trip_and_order(self):
        roster = R.load()
        self.assertEqual(S.load_roster(self.conn), roster)
        changed = copy.deepcopy(roster)
        changed['characters'].reverse()
        changed['characters'][0]['name'] = 'Renamed'
        self.assertEqual(S.save_roster(self.conn, changed), R.check_roster(changed))
        S.save_roster(self.conn, roster)
        self.assertEqual(S.load_roster(self.conn), roster)


if __name__ == '__main__':
    unittest.main()
