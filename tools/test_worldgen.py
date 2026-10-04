"""The Upper Accord generator: a valid, walkable region that merges into a world and survives the database."""
import copy
import re
import secrets
import unittest

import map_editor as E
import terrain_catalog
from worldgen import upper_accord as UA
from worldgen.site import STEP
from worldgen.canvas import reachable


def greyfen():
    world = next(w for w in E.bundled_worlds() if w['id'] == 'Greyfen')
    return E.read_json(world['source'].read_text(encoding='utf-8'))


class GeneratedRegion(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.canvas, cls.site, _, cls.project = UA.generated_project()

    def test_passes_the_editor_and_exporter_checks(self):
        E.check_project(self.project, for_game=False)

    def test_every_door_can_be_walked_to_from_the_road(self):
        c = self.canvas
        walk = reachable(c.codes, c.heights, (UA.MAIN_ROAD[0][0] + 1, UA.MAIN_ROAD[0][1]))
        for r in self.site.manifest:
            sx, sy = STEP[r['facing']]
            self.assertTrue(walk[r['door']['y'] + sy, r['door']['x'] + sx], r['id'])

    def test_every_interior_is_linked_and_uses_catalog_tiles(self):
        linked = {e['cell'] for link in self.project['links'] for e in (link['a'], link['b'])}
        for area in self.project['cells'] + self.project['rooms']:
            self.assertTrue(set(''.join(area['terrain'])) <= terrain_catalog.GLYPHS, area['id'])
            if 'worldX' in area:
                self.assertIn(area['id'], linked)

    def test_the_places_the_user_asked_for_exist(self):
        names = {r['name'] for r in self.site.manifest}
        for name in ('The Grand Hall of Concord', 'The Chapel of the First Oath', 'The Concord Annex',
                     'The Pup Den', 'Trainee Barracks', "Trainers' Hall", 'The Training Cookhouse',
                     "The Leader's Hall", 'The Warden Crypt Library', 'The Order Refectory'):
            self.assertIn(name, names)
        kinds = [r['kind'] for r in self.site.manifest]
        self.assertGreaterEqual(kinds.count('house'), 80)
        self.assertGreaterEqual(kinds.count('shop'), 35)
        deep = [r for r in self.project['rooms'] if r['z'] <= -3]
        self.assertEqual(len(deep), 2)              # The Annex's deep archive and the Order's founders' vault.

    def test_homes_and_workplaces_are_recorded(self):
        beds = sum(len(room['beds']) for r in self.site.manifest for room in r['rooms'])
        work = sum(len(room['work']) for r in self.site.manifest for room in r['rooms'])
        self.assertGreater(beds, 200)
        self.assertGreater(work, 50)

    def test_residents_fill_the_region(self):
        from worldgen import residents as R
        project = copy.deepcopy(self.project)
        manifest = copy.deepcopy(self.site.manifest)
        people = R.populate(project, manifest)
        world = R.apply(project, people)
        E.check_project(world, for_game=False)
        names = [p['name'] for p in world['people']]
        self.assertTrue(200 <= len(names) <= 250, len(names))     # (Shop families and stallholders: Docs/Design/39.)
        self.assertEqual(len(set(names)), len(names))
        self.assertFalse([n for n in names if any(ch.isdigit() for ch in n)])
        roles = [p['role'] for p in world['people']]
        self.assertGreaterEqual(roles.count('merchant'), 40)
        self.assertGreaterEqual(roles.count('guard'), 15)
        self.assertTrue(all(p['route'] in {r['id'] for r in world['routes']} for p in world['people'] if p['route']))
        self.assertIn('leading the Warden Order', {p['workLabel'] for p in world['people']})
        self.assertEqual({f['id'] for f in world['factions']}, {'warden_order', 'concord', 'accord_watch'})
        claims = {a['id']: a['territory']['claims'] for a in world['cells'] + world['rooms']}
        self.assertEqual(claims['warden_order'], ['warden_order'])
        self.assertEqual(claims['the_grand_hall_of_concord_d1'] if 'the_grand_hall_of_concord_d1' in claims
                         else claims['the_concord_annex_d3'], ['concord'])
        self.assertFalse([r['name'] for r in world['rooms'] if re.fullmatch(r'House \d+', r['name'])])
        # Every shop has its keeper's family in the flat above it, and the keeper's label says the trade; the plaza's
        # stalls have their stallholders (Docs/Design/39).
        flats = {room['id']: r for r in self.site.manifest if r['kind'] == 'shop' for room in r['rooms'] if room['z'] == 1}
        self.assertEqual(len(flats), sum(1 for r in self.site.manifest if r['kind'] == 'shop'))
        keepers = [p for p in world['people'] if p['home']['cell'] in flats and p['role'] == 'merchant']
        self.assertEqual(len(keepers), len(flats), 'one keeper lives over each shop')
        self.assertTrue(all(' at ' in p['workLabel'] for p in keepers), [p['workLabel'] for p in keepers][:5])
        self.assertEqual(sum(1 for p in world['people'] if p['workLabel'].endswith('stall on the plaza')), 16)
        with self.assertRaises(ValueError):
            R.apply(world, people)                                   # Residents are not doubled up by accident.

    def test_merge_adds_beside_an_existing_world_and_moves_the_spawn(self):
        world, _ = E.check_project(greyfen(), for_game=False)
        merged = UA.merge(world, self.project, (1024, 0))
        self.assertEqual(len(merged['cells']), len(world['cells']) + len(self.project['cells']))
        self.assertEqual(merged['spawn']['cell'], 'upper_accord')
        E.check_project(merged, for_game=False)
        with self.assertRaises(UA.MergeError):
            UA.merge(merged, self.project, (1024, 0))                # A second import needs --replace.
        again = UA.merge(merged, self.project, (1024, 0), replace=True)
        self.assertEqual(len(again['rooms']), len(merged['rooms']))
        with self.assertRaises(UA.MergeError):
            UA.merge(world, self.project, (0, 0))                    # Greyfen's town sits at the origin.


try:
    from test_world_db import database_available, superuser
    import world_db as W
    import world_store as S
    import roster as R
    HAVE_DB = database_available()
except Exception:                                                  # pragma: no cover - no psycopg
    HAVE_DB = False


@unittest.skipUnless(HAVE_DB, 'local PostgreSQL not running (python3 tools/world_db.py up)')
class ImportIntoDatabase(unittest.TestCase):
    def test_import_round_trips_through_postgres(self):
        name = f'ratw_test_{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {name} TO ratw_editor')
        try:
            with W.connect('dev', 'owner', dbname=name) as owner:
                W.migrate(owner)
            with W.connect('dev', 'editor', dbname=name) as conn:
                S.save_roster(conn, R.load())
                S.save_world(conn, greyfen(), None, create=True)
                _, _, _, project = UA.generated_project()
                UA.import_dev(project, (1024, 0), False, conn=conn, log=lambda *_: None)
                loaded, revision = S.load_world(conn, 'greyfen')
                self.assertEqual(revision, 2)
                self.assertEqual(loaded['spawn']['cell'], 'upper_accord')
                ours = {c['id']: c for c in loaded['cells']}['upper_accord']
                theirs = {c['id']: c for c in project['cells']}['upper_accord']
                self.assertEqual(ours['terrain'], theirs['terrain'])
                self.assertEqual(ours['heights'], theirs['heights'])
                files = E.export_files(loaded, stream=True)
                self.assertIn('cells/the_grand_hall_of_concord.cell', files)
                with self.assertRaises(UA.MergeError):
                    UA.import_dev(copy.deepcopy(project), (1024, 0), False, conn=conn, log=lambda *_: None)
                import tempfile
                from pathlib import Path
                with tempfile.TemporaryDirectory() as folder:
                    UA.import_dev(project, (0, 0), False, conn=conn, log=lambda *_: None, replace_world=True,
                                  backups=Path(folder))
                    backup = next(Path(folder).glob('greyfen_dev_r2_*.atlas.json'))
                    self.assertIn('upper_accord', backup.read_text(encoding='utf-8'))
                self.assertEqual([w['id'] for w in S.list_worlds(conn)], ['upper_accord'])
                alone, _ = S.load_world(conn, 'upper_accord')
                self.assertEqual(len(alone['cells']), len(project['cells']))
                self.assertFalse(conn.execute("SELECT 1 FROM live.npcs WHERE world_id = 'greyfen'").fetchone())
        finally:
            with superuser() as su:
                su.execute(f'DROP DATABASE {name} WITH (FORCE)')


if __name__ == '__main__':
    unittest.main()
