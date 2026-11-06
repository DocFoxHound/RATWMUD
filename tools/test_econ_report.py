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
            row.setdefault(table, []).extend([c if isinstance(c, int) else i(c) for c in r] for r in rows)
        f.write(json.dumps(dict({'day': n, 'season': season, 'year_day': n + 1, 'names': fresh}, **row)) + '\n')


def write_run(root, more=None, yields=None, areas=None, econ_more=None, second=None, makes=None):
    meta = {'meta': {'format': 'ratw-econ-full', 'version': 1, 'price_band': 25, 'columns': {},
                     'farms': {'grain_farm_1': {'area': 'grain', 'yield_scale': 1.0,
                                                'yield': yields or {'flour': [10, 10, 10, 10]}}}}}
    if makes:
        meta['meta']['farms']['grain_farm_1']['makes'] = makes
    if second:
        meta['meta']['farms']['grain_farm_2'] = {'area': 'grain', 'yield_scale': 1.0, 'yield': second}
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
            # Sold 10 and eaten 1 a day, and the baker's shelf 4 under (a stock, refilled over a week): 11.57, 5% over.
            self.assertEqual(patch['farms']['grain_farm_1']['flour'], [12.15, 10, 10, 10])
            farms = root / 'country_farms.json'
            farms.write_text(R.dumps({'areas': [{'id': 'grain', 'farms': [{'id': 'grain_farm_1', 'yield': {'flour': [10, 10, 10, 10]}}]}]}))
            # Half way by default, so the next run's patch can't swing back past where it started.
            self.assertEqual(R.apply_patch(root / 'report.md.patch.json', farms), 1)
            self.assertIn('"flour": [11.07, 10, 10, 10]', farms.read_text())
            self.assertEqual(R.main(['--apply', str(root / 'report.md.patch.json'), str(farms), '--damping', '1']), 0)
            self.assertIn('"flour": [12.15, 10, 10, 10]', farms.read_text())
            self.assertEqual(R.apply_patch(root / 'report.md.patch.json', farms, 1, {'oats'}), 0, 'only the goods named')

    def test_crew_hours_and_short_households(self):
        # Round 3 (doc 73): the baker put in 20 hours a day, used 19.5 (bound by its crew) and lost none; a lone pup with
        # no earner was short of food on both days.
        more = lambda n: {'work': [['till:job:baker', 200, 195, 0]],
                          'hh_short': [['pup', 'lower', 1, 9, 0, -1, -1, -1]]}
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            econ = write_run(root, more=more)
            out = root / 'report.md'
            self.assertEqual(R.main([str(econ), '--out', str(out)]), 0)
            text = out.read_text()
            self.assertIn('| bakery | 20.0 | 98% | 0% |', text, "the short trade's crew-hours")
            self.assertIn('Bound by their crew (used 95% or more of their hours): baker (bakery, 19.5 of 20.0 h).', text)
            self.assertIn('1.00 a day, 1 households; 100% with no earner, 100% alone, a mean purse of 9.0p; by class lower 2.', text)
            self.assertNotIn('At the shops\' last hour', text, 'the older rows have no hour')

    def test_short_households_at_the_last_hour(self):
        # Round 10 (doc 73): the rows' last three, the meals lacked, the purse then and the cheapest food on offer: on the
        # first day the pup held 1p against a 2p bun (lacking 2 meals), on the second 3p (lacking 1), so it could pay.
        more = lambda n: {'hh_short': [['pup', 'lower', 1, 9, 0, -1, -1, -1, 2 - n, 1 + 2 * n, 2]]}
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            econ = write_run(root, more=more)
            out = root / 'report.md'
            self.assertEqual(R.main([str(econ), '--out', str(out)]), 0)
            text = out.read_text()
            self.assertIn('a mean purse of 9.0p; by class lower 2.', text, 'the payers are the three before them')
            self.assertIn("At the shops' last hour they lacked 1.5 meals and held 2.0p; 50% could have paid for the cheapest "
                          'food then on offer, 0% found no food in any open shop.', text)

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
            # Sold 3 a day, and the stables' shelf 20 under, a stock refilled over a week (not 20 lost a day): 5.86, 5% over.
            self.assertEqual(patch['farms']['grain_farm_1']['oats'], [6.15, 3, 3, 3])
            self.assertIn('no farm grows', text)
            self.assertIn('wheat (grain) 14', text)
            self.assertIn('| grapes | spring | 6 |', text, "the economy lines' draws cut short, at the full level too")

    def test_the_farms_of_a_good_together(self):
        # Two farms grow flour alike, but the draws took it all from the first (its stock the larger): each is sized by
        # its share of the yield, not by its own sales. Meal spoiled at the second: no stock need for it, only what sold.
        def more(n):
            return {'farm_good': [['grain_farm_2', 'flour', 10, 0, 5, 0, 0, 0], ['grain_farm_1', 'meal', 5, 2, 0, 0, 0, 0],
                                  ['grain_farm_2', 'meal', 5, 2, 6, 3, 0, 0]],   # (Held and spoiled: a glut.)
                    'short': [['till:job:cook', 'meal', 'shop:inn', 1, 14, '']]}
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            econ = write_run(root, more, yields={'flour': [10, 10, 10, 10], 'meal': [5, 5, 5, 5]},
                             second={'flour': [10, 10, 10, 10], 'meal': [5, 5, 5, 5]})
            self.assertEqual(R.main([str(econ), '--out', str(root / 'report.md')]), 0)
            patch = json.loads((root / 'report.md.patch.json').read_text())['farms']
            # Flour: 11 a day taken (all from the first) and the baker's 4 under refilled over a week, half each.
            self.assertEqual(patch['grain_farm_1']['flour'][0], patch['grain_farm_2']['flour'][0])
            self.assertEqual(patch['grain_farm_1']['flour'][0], 6.08)
            # Meal: 4 a day sold, spoiled at a farm: the cook's shelf counts for nothing; 2.1 each.
            self.assertEqual(patch['grain_farm_1']['meal'][0], 2.1)
            self.assertEqual(patch['grain_farm_2']['meal'][0], 2.1)

    def test_the_works_and_the_country(self):
        # Doc 72: a works' area (the fuel's) marked in the farms' table with its rent, each works by name with its House;
        # the Country by its parts from the economy lines: its goods' sales, the estates' rents by House, its posts by rule.
        def more(n):
            return {'farm': [['peat_fens', 'fuel', 52, 70, 26, 26, 0, 0, 0, 0, 0, 0, 44]],
                    'farm_good': [['peat_fens', 'peat', 120, 110, 300, 0, 0, 0]]}
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            econ = write_run(root, more, econ_more={'country': 900, 'country_sales': 600, 'country_rents': 44,
                                                    'country_rents_by_house': {'Timberland': 44}, 'country_posts_owed': 800,
                                                    'country_posts_paid': 790,
                                                    'country_posts_by_rule': {'the sea trade': 440, "the Houses' people": 350},
                                                    'country_posts_held': {'the sea trade': 55, "the Houses' people": 44},
                                                    'country_spent': 300})
            full = Path(str(econ) + '.full.jsonl.gz')
            lines = gzip.open(full, 'rt').read().splitlines()
            meta = json.loads(lines[0])
            meta['meta']['farms']['peat_fens'] = {'area': 'fuel', 'yield_scale': 1.0, 'house': 'Timberland',
                                                  'yield': {'peat': [110, 110, 110, 110]}}
            meta['meta']['works_areas'] = ['fuel']
            with gzip.open(full, 'wt') as f:
                f.write('\n'.join([json.dumps(meta)] + lines[1:]) + '\n')
            out = root / 'report.md'
            self.assertEqual(R.main([str(econ), '--out', str(out)]), 0)
            text = out.read_text()
            self.assertIn('| fuel (works) | 1 | 2 | 70 | 52/52 | 0 | 0 | 44 |', text, 'the works\' area and its rent a day')
            self.assertIn('| peat_fens | Timberland | 2 | 70.0 | 52/52 | 0 | 44.0 | 52 |', text, 'the works by name')
            self.assertIn("the estates' rents 44p (House Timberland 44p)", text)
            self.assertIn("the Wharf's goods to the shops 600p", text, "an older run's country keys read as the Wharf's")
            self.assertIn("the estates' posts 790p of 800p owed (the sea trade 440p, 55 posts, the Houses' people 350p, 44 posts)", text)
            self.assertNotIn("The Houses' estates (a day", text, 'no estates split: no table')

    def test_the_wharf(self):
        # Doc 73, 7: the Wharf beyond the realm, a day: the beyond's pennies in and out and net, the fees, the basket, the
        # credit's course; by good what it landed and took; a new run's keys (wharf, wharf_sales...) read as they are.
        day = {'valve': 1, 'imported': {'butter': [70, 150], 'offal': [5, 10]}, 'taken': {'hide': [30, 90]}, 'basket': 40,
               'in': 140, 'out': 144, 'fee': 18, 'moved': 0, 'credit': 300, 'book_in': 140, 'book_out': 144}
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            econ = write_run(root, None, econ_more={'wharf': 700, 'wharf_sales': 160, 'estate_rents': 0, 'estate_posts_owed': 476,
                                                    'estate_posts_paid': 476, 'basket_spent': 10, 'money': 64000, 'wharf_day': day})
            out = root / 'report.md'
            self.assertEqual(R.main([str(econ), '--out', str(out)]), 0)
            text = out.read_text()
            self.assertIn('## The Wharf: beyond the realm', text)
            self.assertIn('the beyond paid in 140p and was paid 144p (net -4p into the realm)', text)
            self.assertIn("the Wharf's fees 18p; its export basket 40p", text)
            self.assertIn('| butter | 70.0 | 150.0 | 0.0 | 0.0 |', text)
            self.assertIn('| hide | 0.0 | 0.0 | 30.0 | 90.0 |', text)
            self.assertIn("The Wharf's purse: 700 -> 700p", text)
            self.assertIn("the Wharf's goods to the shops 160p", text)

    def test_the_estates(self):
        # Doc 72, Phase 6: each House's estate a row, a day: its rents, the Crown's grants, its posts paid of owed and the
        # nights short, what it bought, its purse.
        estates = {'grayrock': {'house': 'Grayrock', 'rents': 20, 'grants_in': 280, 'grants_out': 0, 'posts_owed': 476,
                                'posts_paid': 470, 'posts': 58, 'spent': 0, 'moved': 0, 'purse': 500, 'levy_out': 12},
                   'crown': {'house': 'the Crown', 'rents': 490, 'grants_in': 0, 'grants_out': 280, 'posts_owed': 94,
                             'posts_paid': 94, 'posts': 11, 'spent': 30, 'moved': 0, 'purse': 900, 'levy_in': 12}}
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            econ = write_run(root, None, econ_more={'country': 500, 'country_sales': 176, 'country_rents': 0,
                                                             'country_rents_by_house': {}, 'country_posts_owed': 570,
                                                             'country_posts_paid': 564, 'country_posts_by_rule': {},
                                                             'country_posts_held': {}, 'country_spent': 30, 'estates': estates})
            out = root / 'report.md'
            self.assertEqual(R.main([str(econ), '--out', str(out)]), 0)
            text = out.read_text()
            self.assertIn("The Houses' estates (a day over the 2 days they were split", text)
            self.assertIn('| House Grayrock | 58 | 20 | 280 | 0 | 0/12 | 470/476 | 2 | 0 | 500 -> 500 (least 500) |', text)
            self.assertIn('| the Crown | 11 | 490 | 0 | 280 | 12/0 | 94/94 | 0 | 30 | 900 -> 900 (least 900) |', text)
            self.assertIn("The Wharf's purse (House Grayrock's; the other estates' below): 500 -> 500p", text)

    def test_the_land_rent(self):
        # Doc 72, open question 2: a food farm's land rent in the farms' table, by farm with its House and what is left over
        # its wages and rent; the Country's line with the land rents by House, "the Crown" as it is named.
        def more(n):
            return {'farm': [['grain_farm_1', 'grain', 132, 160, 66, 66, 0, 50, 0, 0, 0, 0, 44]]}
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            econ = write_run(root, more, econ_more={'country': 900, 'country_sales': 600, 'country_rents': 0,
                                                    'country_rents_by_house': {}, 'country_land_rents': 44,
                                                    'country_land_rents_by_house': {'the Crown': 30, 'Fell': 14},
                                                    'country_posts_owed': 800, 'country_posts_paid': 800,
                                                    'country_posts_by_rule': {'the sea trade': 800},
                                                    'country_posts_held': {'the sea trade': 100}, 'country_spent': 300})
            full = Path(str(econ) + '.full.jsonl.gz')
            lines = gzip.open(full, 'rt').read().splitlines()
            meta = json.loads(lines[0])
            meta['meta']['farms'].setdefault('grain_farm_1', {'area': 'grain', 'yield_scale': 1.0, 'yield': {}})['house'] = 'the Crown'
            meta['meta']['works_areas'] = ['fuel']
            with gzip.open(full, 'wt') as f:
                f.write('\n'.join([json.dumps(meta)] + lines[1:]) + '\n')
            out = root / 'report.md'
            self.assertEqual(R.main([str(econ), '--out', str(out)]), 0)
            text = out.read_text()
            self.assertIn("the farms' land rents 44p (the Crown 30p, House Fell 14p)", text)
            # (The base run's two days of it, rent 0, beside these two: 4 days, its takings, wages, rent, rent %, basket, left.)
            self.assertIn('| grain_farm_1 | the Crown | 4 | 86.0 | 43.0 | 22.0 | 25.6 | 27.5 | 21.0 |', text, 'the farm by name with its House')

    def test_supply_bound(self):
        # Doc 72, Phase 3: the milk all sold or spoiled, the farm out at every day's end and the inn's draws 14 short: what
        # sold was the supply. Its shelf counts though some spoiled (the Restday's), it is listed as supply-bound, and
        # --apply takes its whole step at any damping (the flour, not bound, half way).
        def more(n):
            return {'farm_good': [['grain_farm_1', 'milk', 6, 4, 0, 2, 0, 0]],
                    'short': [['till:job:cook', 'milk', 'shop:inn', 1, 14, '']]}
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            econ = write_run(root, more, yields={'flour': [10, 10, 10, 10], 'milk': [6, 6, 6, 6]})
            out = root / 'report.md'
            self.assertEqual(R.main([str(econ), '--out', str(out)]), 0)
            patch = json.loads((root / 'report.md.patch.json').read_text())
            # 4 a day sold and the cook's 14 under refilled over a week: 6 a day, 5% over.
            self.assertEqual(patch['farms']['grain_farm_1']['milk'][0], 6.3)
            self.assertEqual(patch['whole'], ['milk'])
            self.assertEqual(patch['wholeSeasons'], {'milk': ['spring']}, 'bound in the season seen alone')
            self.assertIn('Supply-bound', out.read_text())
            farms = root / 'country_farms.json'
            farms.write_text(json.dumps({'areas': [{'id': 'grain', 'farms': [{'id': 'grain_farm_1', 'yield': {
                'flour': [10, 10, 10, 10], 'milk': [6, 6, 6, 6]}}]}]}))
            self.assertEqual(R.main(['--apply', str(root / 'report.md.patch.json'), str(farms), '--damping', '0.5']), 0)
            y = json.loads(farms.read_text())['areas'][0]['farms'][0]['yield']
            self.assertEqual(y['milk'][0], 6.3, 'the whole step')
            # A season it was not bound in moves half way, as any good's (a patch's other seasons, here written by hand).
            p = json.loads((root / 'report.md.patch.json').read_text())
            p['farms']['grain_farm_1']['milk'] = [6.3, 8, 8, 8]
            (root / 'two.patch.json').write_text(json.dumps(p))
            farms.write_text(json.dumps({'areas': [{'id': 'grain', 'farms': [{'id': 'grain_farm_1', 'yield': {'milk': [6, 6, 6, 6]}}]}]}))
            self.assertEqual(R.main(['--apply', str(root / 'two.patch.json'), str(farms), '--damping', '0.5']), 0)
            self.assertEqual(json.loads(farms.read_text())['areas'][0]['farms'][0]['yield']['milk'], [6.3, 7, 7, 7],
                             'whole in spring, half way in the seasons not bound')
            self.assertEqual(y['flour'][0], round(10 + .5 * (patch['farms']['grain_farm_1']['flour'][0] - 10), 2), 'half way')

    def test_what_a_farm_makes_of_its_spare(self):
        # Doc 72, after country3b (the user's decision): a dairy's milk past keeping made into cheese (the farm_good row's
        # `used`, its ninth column; an older file's eight still read). The cheese, a good of its area no farm grows as a
        # yield, is had, so not named among the ungrown; the report says what was made, of what, and what spoiled.
        def more(n):
            return {'farm_good': [['grain_farm_1', 'milk', 10, 6, 0, 0, 0, 0, 4], ['grain_farm_1', 'cheese', 1, 0, 5 + n, 0, 0, 0, 0]],
                    'short': [['till:job:cook', 'cheese', 'shop:provisioner', 1, 3, '']]}
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            econ = write_run(root, more, yields={'flour': [10, 10, 10, 10], 'milk': [10, 10, 10, 10]},
                             areas={'grain': ['flour', 'milk', 'cheese', 'butter']}, makes={'cheese': 'milk'})
            out = root / 'report.md'
            self.assertEqual(R.main([str(econ), '--out', str(out)]), 0)
            text = out.read_text()
            self.assertIn('Made at the farms of what would have spoiled', text)
            self.assertIn('cheese 1.0 made of milk (sold 0.0, 6 held at the end)', text)
            self.assertIn('milk 4.0 used, 0.0 spoiled', text)
            self.assertNotIn('cheese (grain)', text, 'made, so not among the goods no farm grows')
            patch = json.loads((root / 'report.md.patch.json').read_text())
            self.assertNotIn('cheese', patch['farms'].get('grain_farm_1', {}), 'no yield of it to patch')

    def test_the_normal_level_alone(self):
        with tempfile.TemporaryDirectory() as d:
            econ = write_run(Path(d))
            Path(str(econ) + '.full.jsonl.gz').unlink()
            out = Path(d) / 'report.md'
            self.assertEqual(R.main([str(econ), '--out', str(out)]), 0)
            text = out.read_text()
            self.assertIn('The normal level only', text)
            self.assertIn('Tills short a night: mean 1.0', text)

    def test_the_steering(self):
        # Doc 74, 12.3: the measures against their targets at the slider, the push, the levers, the skim and its books, the
        # floors' cost; a run without the steering's fields has no such section.
        more = {'steer': 1, 'steer_lean': 0, 'steer_push': 0.0, 'steer_push_lo': -0.15, 'steer_push_hi': 0.15, 'steer_judged': False,
                'm_gini': 0.49, 'm_upper_share': 7.5, 'm_top10': 34.0, 'm_hungry': 0.01, 'm_short': 0, 'm_treasury': 1700,
                'g_mean': 0.49, 's_mean': 7.5, 't10_mean': 34.0, 'h_mean28': 0.01, 'k_mean28': 0, 'tr_mean28': 1700,
                'lev_share_out': 0, 'lev_toll': 0, 'lev_cap': 2000, 'skim': 30, 'skim_for': {'never broke': 30},
                'skim_by': {'shops': 20, 'treasury': 10}, 'skim_top': {'till:job:baker': 20}, 'skim_short': 0, 'relief_fund': 0,
                'relief': {'necessities': 30}, 'never_hungry': {'pieces': 0, 'pennies': 0, 'homes': 0, 'wharf_pieces': 0, 'wharf_pennies': 0},
                'households_short_after': -1, 'never_broke': {'pennies': 30, 'homes': 3}, 'share_out': 0, 'posts_share': 0,
                'ground_rent': 0, 'dividend': 0, 'land_rent_extra': 0}
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            econ = write_run(root, None, econ_more=more)
            out = root / 'report.md'
            self.assertEqual(R.main([str(econ), '--out', str(out)]), 0)
            text = out.read_text()
            self.assertIn('## The steering', text)
            self.assertIn('Where the money rests: +0 (the steering on, 2 days)', text)
            self.assertIn("| the earners' Gini | 0.487 ± 0.035 | 0.490 | yes |", text)
            self.assertIn('| the upper share (%) | 7 ± 2.5 | 7.5 | yes |', text)
            self.assertIn('by purpose never broke 30.0p', text)
            self.assertIn("The skim's books: skimmed 60p, relief paid 60p (necessities 60), fund:relief at the end 0p: balanced", text)
            self.assertIn('Never Broke 30.0p (3.0 homes)', text)
            plain = write_run(root / 'x' if (root / 'x').mkdir() is None else root, None)
            self.assertEqual(R.main([str(plain), '--out', str(out)]), 0)
            self.assertNotIn('## The steering', out.read_text())

    def test_phase5(self):
        # Doc 72, Phase 5: the goods bought from the makers to sell on, the workshops' wages and their Houses' share, the
        # luxuries; and a world without the workshops' buildings said so.
        econ = [{'city_resale': 12, 'city_resale_pieces': 4, 'city_resale_goods': {'fine_soap': 3, 'nails': 1},
                 'luxuries_bought': 6, 'luxuries_spent': 60, 'luxury_households': 5,
                 'workshops': {'cn_coldwater_soapworks': {'owed': 46, 'paid': 46, 'to_house': 30}}},
                {'city_resale': 8, 'city_resale_pieces': 2, 'city_resale_goods': {'fine_soap': 2},
                 'luxuries_bought': 4, 'luxuries_spent': 40, 'luxury_households': 3, 'workshops': {}}]
        text = R.phase5_lines(econ)
        self.assertIn('3.0 pieces, 10.0p (fine_soap 2.5, nails 0.5)', text)
        self.assertIn('cn_coldwater_soapworks wages 46.0p of 46.0p, to its House 30.0p', text)
        self.assertIn('5.0 pieces for 50.0p, 4.0 households', text)
        self.assertIn('No workshop in this world', R.phase5_lines([{'city_resale': 0}]))
        # After country5: the luxuries by the households' class, and the Houses' own tables.
        econ[0].update({'luxury_classes': {'upper': {'pieces': 4, 'spent': 48}, 'middle': {'pieces': 2, 'spent': 12}},
                        'luxuries_houses': 6, 'luxuries_houses_spent': 90})
        econ[1].update({'luxury_classes': {'upper': {'pieces': 4, 'spent': 40}}, 'luxuries_houses': 2,
                        'luxuries_houses_spent': 30})
        text = R.phase5_lines(econ)
        self.assertIn('(upper 4.0 at 11.0p, middle 1.0 at 6.0p); the Houses\' tables 4.0 pieces for 60.0p (15.0p a piece).',
                      text)


if __name__ == '__main__':
    unittest.main()
