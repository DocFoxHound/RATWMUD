#!/usr/bin/env python3
"""world_build.py export: a build written out as the files world_check and game_load read. No database: a stand-in
connection answers the two queries game_files() makes."""
import tempfile
import unittest
from pathlib import Path

import world_build as B


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


if __name__ == '__main__':
    unittest.main()
