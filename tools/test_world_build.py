#!/usr/bin/env python3
"""world_build.py export: a build written out as the files world_check and game_load read. No database: a stand-in
connection answers the two queries game_files() makes."""
import os
import secrets
import tempfile
import unittest
from pathlib import Path

import world_build as B
from test_world_db import database_available, superuser

DB_TAG = os.environ.get('RATW_TEST_DB_TAG', '')


class Rows:
    def __init__(self, rows):
        self.rows = rows

    def fetchone(self):
        return self.rows[0]

    def fetchall(self):
        return self.rows


class Build:
    """A build's manifest files and two cells, as world.builds and world.build_cells hold them."""
    def execute(self, sql, params):
        if 'FROM world.builds' in sql:
            return Rows([({'world.ratw': 'RATW_WORLD 3\narea "a"\n', 'regions.json': '{}'},)])
        return Rows([('a', 'header\ngrid:\n...', 'east b\n'), ('b', 'header\ngrid:\n...', '')])


class ExportTests(unittest.TestCase):
    def test_writes_the_manifest_cells_and_seams(self):
        with tempfile.TemporaryDirectory() as folder:
            count = B.export(Build(), 7, Path(folder))
            self.assertEqual(count, 6)
            root = Path(folder)
            self.assertEqual((root / 'world.ratw').read_text(), 'RATW_WORLD 3\narea "a"\n')
            self.assertEqual((root / 'regions.json').read_text(), '{}')
            self.assertEqual((root / 'cells' / 'a.cell').read_text(), 'header\ngrid:\n...')
            self.assertEqual((root / 'seams' / 'a').read_text(), 'east b\n')
            self.assertEqual((root / 'seams' / 'b').read_text(), '', 'a cell without seams still has its file')


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class FingerprintTests(unittest.TestCase):
    """Doc 79, Phase 9 (U8): live.economy is not in a build's fingerprint, so a DM's lever never makes a new build (nor
    restarts a dev server); an NPC's edit still does."""

    def test_a_lever_write_leaves_the_build_current(self):
        import roster as R
        import world_db as W
        import world_store as S
        from test_world_store import greyfen
        name = f'ratw_test_{DB_TAG}{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {name} TO ratw_editor')
        try:
            with W.connect('dev', 'owner', dbname=name) as owner:
                W.migrate(owner)
            with W.connect('dev', 'editor', dbname=name) as conn:
                S.save_roster(conn, R.load())
                S.save_world(conn, greyfen(), None, create=True)
                B.build(conn, 'test')
                self.assertTrue(B.current(conn))
                S.patch_economy_rules(conn, 'greyfen', {'prosperity': 40, 'prosperityCause': 'a fair'})
                S.save_economy(conn, 'greyfen', {**S.economy_value(conn, 'greyfen'), 'needsPaused': True})
                self.assertTrue(B.current(conn), 'a lever: the build is still current')
                conn.execute("UPDATE live.npcs SET band_override = 'rich' WHERE id = 'brann'")
                self.assertIsNone(B.current(conn), 'an NPC changed: a new build is due')
        finally:
            with superuser() as su:
                su.execute(f'DROP DATABASE {name} WITH (FORCE)')


if __name__ == '__main__':
    unittest.main()
