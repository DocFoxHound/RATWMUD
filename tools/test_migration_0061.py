#!/usr/bin/env python3
"""Migration 0061 (Docs/Design/79-a-staged-economy.md, 14; Phase 9): schema only, on every path a world takes to it.

(a) 0001 to 0061 on an empty database; (b) 0001 to 0054, then 0055 to 0061 one at a time (PROD's path); (c) 0001 to 0060
with business_settings rows, a season in 4 parts, dead rules, nested rules values and a full economy row, then 0061: every
live.economy row byte-identical, business_settings gone, people_manifest working for a world and for one NPC, and its
lines what tools/manifest_lines.py writes for the same entity.

Each case in its own throwaway database, `ratw_test_s2stage_mig_<hex>`, dropped after; skipped without PostgreSQL.
"""
import json
import secrets
import unittest

import manifest_lines as M
import world_db as W
from test_world_db import database_available, superuser


def files_upto(last):
    return [f for f in W.migration_files() if int(f[0]) <= last]


class Scratch:
    """A throwaway database for one case."""

    def __init__(self):
        self.name = f'ratw_test_s2stage_mig_{secrets.token_hex(4)}'

    def __enter__(self):
        with superuser() as su:
            su.execute(f'CREATE DATABASE {self.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {self.name} TO ratw_editor, ratw_game')
        self.owner = W.connect('dev', 'owner', dbname=self.name)
        return self

    def __exit__(self, *_):
        self.owner.close()
        with superuser() as su:
            su.execute(f'DROP DATABASE {self.name} WITH (FORCE)')


def tables(conn):
    return {r[0] for r in conn.execute("SELECT table_name FROM information_schema.tables WHERE table_schema = 'live'")}


def columns(conn, table):
    return {r[0] for r in conn.execute("SELECT column_name FROM information_schema.columns WHERE table_schema = 'live' "
                                       "AND table_name = %s", (table,))}


def seed_world(conn, world='mig'):
    """A world, an economy row with every dead column set, two residents (one with a business switch)."""
    conn.execute("INSERT INTO world.worlds (id, name) VALUES (%s, 'Migration world')", (world,))
    conn.execute(
        "INSERT INTO live.economy (world_id, treasury, store_herbs, store_meals, daily_herbs, daily_meals, internal_store, "
        "store_markup, store_resale, resale_markup, store_rule, needs_paused, country, country_reserve, gate_toll, carriage, "
        "wage_keeper, wage_hand, wage_public, wage_country, country_off, subsidies, comforts_above, comforts_share, seasons, "
        "rules, services, farms) VALUES (%s, 1234, 40, 30, 7, 9, false, 120, false, 60, true, true, false, 300, 20, 9, 15, 9, "
        "9, 9, '{grain,ale}', '{\"carriage\": 30}', 50, 20, %s, %s, %s, %s)",
        (world,
         json.dumps({'bread': {'supply': [100, 120, 90, 80], 'price': [100, 95, 105, 120], 'shelf': [100, 110, 100, 90],
                               'normal': 12},
                     'fish': {'price': [90, 90, 100, 110], 'shelf': [100, 100, 100, 100]},
                     'odd': {'price': [100, 100]}}),
         json.dumps({'ripple': 40, 'steer': 1, 'almsADay': 12, 'prosperity': 35, 'prosperityCause': 'the good harvest',
                     'nested': {'a': [1, 2]}, 'neverImported': ['salt'],
                     'status': {'caps': {'rewardsADay': 200}}, 'market': {'events': {'perWeek': 2}}}),
         json.dumps({'groom_and_trim': {'on': False, 'price': 6, 'every': 20}}),
         json.dumps({'start': 3, 'areas': {'orchards': {'on': False, 'yieldScale': 80}}})))
    return world


def seed_people(conn, world, settings=True):
    for position, npc in enumerate(('bram', 'cora')):
        conn.execute(
            "INSERT INTO live.npcs (world_id, id, position, name, role, work_label, age, appearance, voice, paid, hours_start, "
            "hours_end, purse, herbs, meals, home_x, home_y, work_x, work_y, evening_x, evening_y) "
            "VALUES (%s, %s, %s, %s, 'civilian', 'Baker', 30, %s, 3, true, 8, 18, 20, 1, 2, 0, 0, 0, 0, 0, 0)",
            (world, npc, position, npc.title(),
             json.dumps({'species': 'wolf', 'sex': 'male', 'stature': 'average', 'pattern': 'plain',
                         'baseColor': '#776655', 'gradientColor': '#665544', 'markingColor': '#554433'})))
    if settings:
        conn.execute("INSERT INTO live.business_settings (world_id, npc_id, internal_store) VALUES (%s, 'bram', false)", (world,))


def economy_rows(conn):
    return conn.execute("SELECT world_id, to_jsonb(e) - 'updated_at' FROM live.economy e ORDER BY world_id").fetchall()


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class Migration0061Tests(unittest.TestCase):

    def check_slim(self, conn):
        self.assertNotIn('business_settings', tables(conn))
        self.assertIn('band_override', columns(conn, 'npcs'))
        # The schema keeps every dead column (D20): nothing dropped from live.economy.
        for dead in ('internal_store', 'store_markup', 'daily_herbs', 'country', 'subsidies', 'comforts_above', 'farms'):
            self.assertIn(dead, columns(conn, 'economy'), dead)

    def test_a_fresh_database_runs_every_migration(self):
        with Scratch() as db:
            W.migrate(db.owner)
            self.assertIn('0061', W.applied_migrations(db.owner))
            self.check_slim(db.owner)
            seed_world(db.owner, 'fresh')
            seed_people(db.owner, 'fresh', settings=False)
            db.owner.execute("UPDATE live.npcs SET band_override = 'rich' WHERE id = 'bram'")
            with self.assertRaises(Exception):     # The band's check holds.
                db.owner.execute("UPDATE live.npcs SET band_override = 'wealthy' WHERE id = 'bram'")

    def test_prods_path_one_at_a_time_from_0054(self):
        with Scratch() as db:
            W.migrate(db.owner, files_upto(54))
            for version in range(55, 62):
                applied = W.migrate(db.owner, files_upto(version))
                self.assertEqual(len(applied), 1, version)
            self.check_slim(db.owner)

    def test_0060_rows_are_kept_byte_for_byte(self):
        with Scratch() as db:
            W.migrate(db.owner, files_upto(60))
            world = seed_world(db.owner)
            seed_people(db.owner, world)
            before = economy_rows(db.owner)
            old = db.owner.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0]
            self.assertIn('store off 120 resale off 60 rule on', old)
            self.assertEqual(db.owner.execute('SELECT count(*) FROM live.business_settings').fetchone()[0], 1)
            self.assertIn('rule "prosperityCause" the good harvest', old)   # (0060 prints every rule as text.)
            W.migrate(db.owner)
            self.check_slim(db.owner)
            self.assertEqual(economy_rows(db.owner), before)
            npcs = db.owner.execute("SELECT id, purse, herbs, meals, band_override FROM live.npcs ORDER BY id").fetchall()
            self.assertEqual(npcs, [('bram', 20, 1, 2, None), ('cora', 20, 1, 2, None)])
            text = db.owner.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0]
            lines = text.split('\n')
            self.assertEqual(lines[0], 'economy 1234 40 30')
            self.assertIn('loop needs off', lines)
            for gone in ('store ', 'nostore ', 'countryoff ', 'subsidy ', 'comforts ', 'farms ', 'farmarea ', 'farm '):
                self.assertFalse([l for l in lines if l.startswith(gone)], gone)
            self.assertIn('season "bread" price 100 95 105 120 shelf 100 110 100 90', lines)
            self.assertIn('season "fish" price 90 90 100 110 shelf 100 100 100 100', lines)
            self.assertFalse([l for l in lines if l.startswith('season "odd"')])
            rules = [l for l in lines if l.startswith('rule ')]
            self.assertEqual(rules, ['rule "almsADay" 12', 'rule "prosperity" 35', 'rule "ripple" 40', 'rule "steer" 1'])
            overrides = [l for l in lines if l.startswith('overrides ')]
            self.assertEqual(len(overrides), 1)
            self.assertEqual(json.loads(overrides[0][len('overrides '):]),
                             {'prosperityCause': 'the good harvest', 'neverImported': ['salt'],
                              'status': {'caps': {'rewardsADay': 200}}, 'market': {'events': {'perWeek': 2}}})
            # One NPC alone: only its own lines.
            db.owner.execute("UPDATE live.npcs SET band_override = 'poor' WHERE id = 'cora'")
            one = db.owner.execute("SELECT live.people_manifest(%s, 'cora')", (world,)).fetchone()[0].split('\n')
            self.assertTrue(one[0].startswith('resident "cora"'), one)
            self.assertEqual(one[1:], ['band "cora" poor'])
            # The same entity through tools/manifest_lines.py gives the same economy lines (the overrides as JSON).
            row = db.owner.execute("SELECT treasury, store_herbs, store_meals, needs_paused, seasons, rules, services "
                                   "FROM live.economy WHERE world_id = %s", (world,)).fetchone()
            rules_ = {k: v for k, v in row[5].items() if isinstance(v, int) and not isinstance(v, bool)}
            entity = {'treasury': row[0], 'storeHerbs': row[1], 'storeMeals': row[2], 'needsPaused': row[3],
                      'seasons': row[4], 'rules': rules_, 'services': row[6],
                      'overrides': {k: row[5][k] for k in M.OVERRIDE_KEYS if k in row[5]}}
            written = M.economy_lines(entity)
            db_economy = [l for l in db.owner.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0].split('\n')
                          if l.split(' ', 1)[0] in ('economy', 'loop', 'season', 'rule', 'service', 'overrides')]
            self.assertEqual(M.comparable(written), M.comparable(db_economy))

    def test_the_game_may_still_read_the_manifest(self):
        with Scratch() as db:
            W.migrate(db.owner)
            world = seed_world(db.owner, 'game')
            seed_people(db.owner, world, settings=False)
            with W.connect('dev', 'game', dbname=db.name) as game:
                self.assertTrue(game.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0])


if __name__ == '__main__':
    unittest.main()
