"""tools/econ_report.py (Docs/Design/69-simulation-speed.md, 10): a run's report from world_check's economy file and its
full level's tables (a small fixture written here), and the farms' patch, applied."""
import gzip
import json
import tempfile
import unittest
from pathlib import Path

import econ_report as R


class Fixture:
    """Writes the tables as world_check does: a meta line, then a line a day whose names are numbered as they appear."""

    def __init__(self):
        self.ids = {}

    def id(self, name, fresh):
        if name not in self.ids:
            self.ids[name] = len(self.ids)
            fresh.append(name)
        return self.ids[name]

    def day(self, n, season, f, more=None):
        fresh = []
        i = lambda s: self.id(s, fresh)
        row = {'day': n, 'season': season, 'year_day': n + 1}
        row['trade'] = [[i('flour'), i('shop:bakery'), i('farm:grain_farm_1'), 10, 12, 10],
                        [i('bread'), i('hh:lower'), i('till:job:baker'), 6, 30, 24],
                        [i('bread'), i('hh:upper'), i('till:job:baker'), 2, 10, 8]]
        row['till'] = [[i('till:job:baker'), i('bakery'), 40, 8, 50, 10, 30, 2, 5, 5, 30, 10, 20, 0, 3]]
        row['ware'] = [[i('till:job:baker'), i('bread'), 4, 8, 1250, 1250, 8]]
        row['farm'] = [[i('grain_farm_1'), i('grain'), 100, 12, 20, 20, 0, 5, 60, 8, 0, 0]]
        row['farm_good'] = [[i('grain_farm_1'), i('flour'), 12, 10, 50 + n * 5, 0, 1, 0]]
        row['flow'] = [[i('shops'), i('treasury'), 2], [i('hh:lower'), i('shops'), 30], [i('treasury'), i('hh:lower'), 7]]
        row['short'] = [[i('till:job:baker'), i('flour'), i('shop:bakery'), 1, 4, i('')],
                        [i('-'), i('peat'), i('hh:lower'), 3, 3, i('firewood')]]
        row['class'] = [[i('lower'), 10, 4, 6, 1, 200, 40, 2, 30], [i('upper'), 2, 1, 0, 0, 300, 300, 1, 10]]
        for table, rows in (more(n) if more else {}).items():
            row[table] += [[c if isinstance(c, int) else i(c) for c in r] for r in rows]
        f.write(json.dumps(dict({'day': n, 'season': season, 'year_day': n + 1, 'names': fresh}, **row)) + '\n')


def write_run(root, more=None, yields=None, areas=None, econ_more=None):
    meta = {'meta': {'format': 'ratw-econ-full', 'version': 1, 'price_band': 25, 'columns': {},
                     'farms': {'grain_farm_1': {'area': 'grain', 'yield_scale': 1.0,
                                                'yield': yields or {'flour': [10, 10, 10, 10]}}}}}
    if areas:
        meta['meta']['farm_areas'] = areas
    fx = Fixture()
    econ = root / 'run.jsonl'
    with gzip.open(str(econ) + '.full.jsonl.gz', 'wt') as f:
        f.write(json.dumps(meta) + '\n')
        for n in range(2):
            fx.day(n, 'spring', f, more)
    econ.write_text(''.join(json.dumps({'day': n, 'season': 'spring', 'conserved': True, 'residents': 12, 'treasury': 500 - n,
                                        'births': 0, 'deaths': 0, 'tills_short': 1, **(econ_more or {})}) + '\n'
                            for n in range(2)))
    return econ


class ReportTests(unittest.TestCase):
    def test_the_report(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            econ = write_run(root)
            out = root / 'report.md'
            self.assertEqual(R.main([str(econ), '--out', str(out)]), 0)
            text = out.read_text()
            self.assertLess(len(text.splitlines()), 130, 'about two pages')
            self.assertIn('# Economy report: 2 days (spring 2)', text)
            self.assertIn('| peat | - | spring | 6 | 6 | 2 | firewood 6 |', text, 'a need no shop held, and what was taken')
            self.assertIn('| flour | baker | spring | 8 | 2 | 2 | - |', text, "the shop's draw cut short")
            self.assertIn('| bread | 100% | 0% | 2 |', text, 'pinned at the top')
            self.assertIn('| bakery | 1 | 2 | 40 | 40 | 50 | 40 | 30 | input costs 1 |', text, 'short, and why')
            self.assertIn('treasury>hh:lower 7', text)
            self.assertIn('The treasury: 500 -> 499p', text)
            self.assertIn('| spring | lower (the Sump, the Rents) | 10 | 0.60 | 20 | 40 | 0.50 | 7.5 |', text)
            self.assertIn('| bread | 8.0 | 40 | 1.25 | hh:lower 75% | baker 100% |', text, 'the largest buyer and seller')
            patch = json.loads((root / 'report.md.patch.json').read_text())
            # Sold 10 and eaten 1 a day, and the baker's 4 pieces short a day: 15, and 5% over.
            self.assertEqual(patch['farms']['grain_farm_1']['flour'], [15.75, 10, 10, 10])
            farms = root / 'country_farms.json'
            farms.write_text(R.dumps({'areas': [{'id': 'grain', 'farms': [{'id': 'grain_farm_1', 'yield': {'flour': [10, 10, 10, 10]}}]}]}))
            self.assertEqual(R.apply_patch(root / 'report.md.patch.json', farms), 1)
            self.assertIn('"flour": [15.75, 10, 10, 10]', farms.read_text())

    def test_draws_short_as_lost_sales(self):
        # The stables sold 3 oats on the day it held some, then stood 20 under its shelf both days: at most its day's sales
        # are lost a day, not its shelf's deficit. The mill's wheat (an input, never sold) counts as it stands, and the
        # grain area's wheat, which no farm grows, is named. The economy lines' farm draws cut short (the wine makers'
        # grapes) are reported beside the full tables.
        def more(n):
            return {'ware': [['till:job:ostler', 'oats', 5 if n == 0 else 0, 20, 1000, 1000, 3 if n == 0 else 0]],
                    'farm_good': [['grain_farm_1', 'oats', 3, 3, 0, 0, 0, 0]],
                    'short': [['till:job:ostler', 'oats', 'shop:stables', 1, 20, ''],
                              ['till:job:miller', 'wheat', 'shop:mill', 1, 7, '']]}
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            econ = write_run(root, more, yields={'flour': [10, 10, 10, 10], 'oats': [3, 3, 3, 3]},
                             areas={'grain': ['flour', 'oats', 'wheat']}, econ_more={'farms_out_goods': {'grapes': 3}})
            out = root / 'report.md'
            self.assertEqual(R.main([str(econ), '--out', str(out)]), 0)
            text = out.read_text()
            patch = json.loads((root / 'report.md.patch.json').read_text())
            # Sold 3 a day and at most 3 a day lost (6 over the two days): 6, and 5% over.
            self.assertEqual(patch['farms']['grain_farm_1']['oats'], [6.3, 3, 3, 3])
            self.assertIn('no farm grows', text)
            self.assertIn('wheat (grain) 14', text)
            self.assertIn('| grapes | spring | 6 |', text, "the economy lines' draws cut short, at the full level too")

    def test_the_normal_level_alone(self):
        with tempfile.TemporaryDirectory() as d:
            econ = write_run(Path(d))
            Path(str(econ) + '.full.jsonl.gz').unlink()
            out = Path(d) / 'report.md'
            self.assertEqual(R.main([str(econ), '--out', str(out)]), 0)
            text = out.read_text()
            self.assertIn('The normal level only', text)
            self.assertIn('Tills short a night: mean 1.0', text)


if __name__ == '__main__':
    unittest.main()
