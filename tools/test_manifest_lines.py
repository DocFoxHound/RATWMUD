#!/usr/bin/env python3
"""tools/manifest_lines.py: the economy's lines and a resident's band line, as live.people_manifest writes them since
migration 0061 (doc 79, 14). Offline, fixed strings; the database's own parity is test_world_store's and
test_migration_0061's."""
import json
import unittest

import manifest_lines as M


class EconomyLinesTests(unittest.TestCase):
    def test_the_defaults_are_one_economy_line(self):
        self.assertEqual(M.economy_lines({}), ['economy 1000 100 50'])
        self.assertEqual(M.economy_lines({'treasury': 7, 'storeHerbs': 0, 'storeMeals': 3, 'dailyHerbs': 10}), ['economy 7 0 3'])

    def test_every_line_in_order(self):
        entity = {'treasury': 1234, 'storeHerbs': 40, 'storeMeals': 30, 'needsPaused': True,
                  'seasons': {'fish': {'price': [90, 90, 100, 110], 'shelf': [100] * 4},
                              'bread': {'supply': [1, 2, 3, 4], 'price': [100, 95, 105, 120], 'shelf': [100, 110, 100, 90], 'normal': 12},
                              'odd': {'price': [100, 100], 'shelf': [100, 100]}},
                  'rules': {'ripple': 40, 'prosperity': -35, 'neverBroke': 1, 'flag': True, 'half': 1.5},
                  'services': {'groom_and_trim': {'on': False, 'price': 6, 'every': 20}},
                  'overrides': {'prosperityCause': 'the "good" harvest', 'unknown': 1, 'neverImported': ['salt']}}
        lines = M.economy_lines(entity)
        self.assertEqual(lines[:6], [
            'economy 1234 40 30', 'loop needs off',
            'season "bread" price 100 95 105 120 shelf 100 110 100 90',
            'season "fish" price 90 90 100 110 shelf 100 100 100 100',
            'rule "neverBroke" 1', 'rule "prosperity" -35'])
        self.assertEqual(lines[6:8], ['rule "ripple" 40', 'service "groom_and_trim" off price 6 every 20'])
        self.assertTrue(lines[8].startswith('overrides '))
        self.assertEqual(json.loads(lines[8][len('overrides '):]),
                         {'prosperityCause': 'the "good" harvest', 'neverImported': ['salt']})
        self.assertEqual(len(lines), 9)

    def test_an_overrides_line_too_long_is_left_out(self):
        big = {'overrides': {'market': {'wharf': {'ships': ['the Gull ' + 'x' * 70000]}}}}
        self.assertEqual(M.economy_lines(big), ['economy 1000 100 50'])

    def test_quotes_as_live_q(self):
        self.assertEqual(M.quote('a "b" \\c'), '"a \\"b\\" \\\\c"')

    def test_the_band_line(self):
        self.assertEqual(M.person_lines({'id': 'bram', 'band': 'poor'}), ['band "bram" poor'])
        self.assertEqual(M.person_lines({'id': 'bram', 'band': None}), [])
        self.assertEqual(M.person_lines({'id': 'bram', 'band': 'wealthy'}), [])
        self.assertEqual(M.person_lines({'id': 'bram'}), [])

    def test_comparable_reads_the_overrides_as_json(self):
        a = ['rule "a" 1', 'overrides {"status": {}, "market": {"x": 1}}']
        b = ['overrides {"market": {"x": 1}, "status": {}}', 'rule "a" 1']
        self.assertEqual(M.comparable(a), M.comparable(b))


class ExporterTests(unittest.TestCase):
    """map_editor's exporter writes these lines (and none of the old economy's)."""

    def test_the_exporter_writes_the_new_lines(self):
        import map_editor as E
        world = next(w for w in E.bundled_worlds() if w['id'] == 'Greyfen')
        project = E.read_json(world['source'].read_text(encoding='utf-8'))
        project['economy'] = {**project.get('economy', {}), 'rules': {'ripple': 40, 'steer': 1}, 'needsPaused': True,
                              'businesses': {project['people'][0]['id']: {'internalStore': False}}}
        project['people'][0]['band'] = 'rich'
        files = E.export_files(project, E.roster_lib.load())
        lines = files['world.ratw'].split('\n')
        economy = [l for l in lines if l.split(' ', 1)[0] in M.ECONOMY_KINDS]
        self.assertEqual(economy[0].count(' '), 3, 'three numbers')
        self.assertIn('rule "ripple" 40', economy)
        self.assertNotIn('rule "steer" 1', economy, 'a rule Core no longer has stays out of the world file')
        self.assertIn('loop needs off', economy)
        for old in ('store ', 'nostore ', 'countryoff ', 'subsidy ', 'comforts ', 'farms ', 'farmarea ', 'farm '):
            self.assertFalse([l for l in lines if l.startswith(old)], old)
        self.assertIn(f'band "{project["people"][0]["id"]}" rich', lines)


if __name__ == '__main__':
    unittest.main()
