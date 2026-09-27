import json
import tempfile
import unittest
from pathlib import Path

import map_editor as editor
import terrain_catalog as catalog


class TerrainCatalogTests(unittest.TestCase):
    def test_generated_tables_are_current(self):
        self.assertEqual(catalog.main(['--check']), 0)

    def test_rules_match_the_original_tiles(self):
        self.assertTrue(set('.#,"T=~:^+%') <= catalog.GLYPHS)
        self.assertTrue(set('#T=%') <= catalog.SOLID)
        self.assertEqual(catalog.RAMPS, frozenset(':^'))
        self.assertEqual(catalog.DEFAULT_HEIGHTS, {'^': .5})
        self.assertIn('k', catalog.SOLID)
        self.assertNotIn('b', catalog.SOLID)

    def test_codes_are_ascii_and_glyphs_are_drawable(self):
        for t in catalog.TILES:
            self.assertTrue('!' <= t['code'] <= '~')
            self.assertLess(ord(t['glyph']), 0x10000)
            self.assertNotIn(t['glyph'], 'W>')

    def test_unsound_catalog_is_refused(self):
        data = json.loads(catalog.SOURCE.read_text(encoding='utf-8'))
        for change in ({'code': 'é'}, {'code': '.'}, {'glyph': 'W'}, {'kind': 'lava'}, {'height': .25},
                       {'opaque': True, 'solid': False}, {'fg': 'red'}):
            broken = json.loads(json.dumps(data))
            broken['tiles'].append({**data['tiles'][-1], 'code': '$', **change})
            with tempfile.TemporaryDirectory() as folder:
                path = Path(folder) / 'terrain.json'
                path.write_text(json.dumps(broken), encoding='utf-8')
                with self.assertRaises(catalog.CatalogError, msg=str(change)):
                    catalog.load(path)

    def test_exporter_accepts_catalog_tiles_and_refuses_others(self):
        p = editor.demo_project()
        p['terrain'] = ['.' * 64 for _ in range(48)]
        p['terrain'][20] = '.' * 10 + 'kbPS_m' + '.' * 48
        self.assertIn('kbPS_m', editor.export_files(p)['cells/field_1.cell'])
        p['terrain'][20] = '.' * 10 + '?' + '.' * 53
        with self.assertRaises(editor.ValidationError):
            editor.export_files(p)


if __name__ == '__main__':
    unittest.main()
