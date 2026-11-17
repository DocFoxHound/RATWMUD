#!/usr/bin/env python3
"""tools/mantle_pool.py (Docs/Design/84-living-conversations.md §12.1, Phase 10): the forecast's arithmetic (births one for
one with deaths, shared as the fertile couples' mothers are), the pools worked out from bindings and states, and the
committed name pools read as the game reads them. Needs no database."""
import unittest

import mantle_pool as P
import mantle_store as S


class ForecastTests(unittest.TestCase):
    def test_births_follow_deaths_and_go_where_the_mothers_are(self):
        residents = [{'age': 70, 'dialect': 'plain'}] * 10 + [{'age': 30, 'dialect': 'coldwater'}] * 10
        couples = [{'mother_age': 30, 'dialect': 'coldwater', 'children': 0}, {'mother_age': 30, 'dialect': 'plain', 'children': 0},
                   {'mother_age': 30, 'dialect': 'plain', 'children': 0}, {'mother_age': 44, 'dialect': 'larkfield', 'children': 3}]
        out = P.project(residents, couples, years=2)
        deaths = sum(P.yearly_death(r['age']) for r in residents)
        self.assertAlmostEqual(sum(out['years'][0].values()), deaths, places=1, msg='a year: as many births as deaths')
        self.assertAlmostEqual(out['years'][0]['plain'], 2 * out['years'][0]['coldwater'], places=1, msg='two plain mothers to one')
        self.assertNotIn('larkfield', out['years'][0], 'three children already: no more')
        self.assertLess(sum(out['next180'].values()), sum(out['years'][0].values()), 'half a year, fewer')
        self.assertEqual(P.project(residents, [], 1)['years'][0], {}, 'no couple: nobody is born (the births wait)')

    def test_the_pools_from_bindings(self):
        cards = [{'id': 'cw017', 'dialect': 'coldwater', 'honesty': 'plain'}, {'id': 'cw090', 'dialect': 'coldwater', 'honesty': 'sly'},
                 {'id': 'cw001', 'dialect': 'coldwater', 'honesty': None}, {'id': 'cw002', 'dialect': 'coldwater', 'honesty': 'frank'}]
        bindings = [{'mantle': 'cw017', 'alive': False, 'released': 10.0}, {'mantle': 'cw001', 'alive': True, 'released': None},
                    {'mantle': 'cw002', 'alive': False, 'released': 300.0}]
        p = P.pools(cards, bindings, today=400, rest_days=365)['coldwater']
        self.assertEqual((p['spare'], p['resting'], p['ready'], p['worn']), (1, 1, 1, 1))
        self.assertEqual(dict(p['honesty']), {'plain': 1, 'sly': 1, 'frank': 1}, 'the mix of those not worn')
        self.assertEqual(P.pool_of('ridgemere', {}), 'plain')
        self.assertEqual(P.pool_of('fenshore', {'fenshore': 'mirebridge'}), 'mirebridge')
        self.assertEqual(P.pool_of('demo_reach', {}), '')

    def test_fits_as_the_game_decides(self):
        self.assertTrue(S.fits('stage=adult|old', {'sex': 'male'}, 0), 'a newborn will be grown')
        self.assertFalse(S.fits('stage=young|adolescent', {}, 30))
        self.assertFalse(S.fits('sex=female', {'sex': 'male'}, 30))
        self.assertTrue(S.fits('female self.role!=guard', {'sex': 'female', 'role': 'merchant'}, 30))
        self.assertFalse(S.fits('quarter=sump', {}, 30), 'a fact not known does not hold')
        self.assertTrue(S.fits('age>=60', {}, 0) and not S.fits('age<5', {}, 30))


class NameTests(unittest.TestCase):
    def test_the_committed_pools_read_cleanly(self):
        problems, warnings, counts = P.check_names()
        self.assertEqual(problems, [])
        self.assertEqual(set(counts), set(P.DIALECTS), 'a pool for every dialect')
        self.assertTrue(all(c['female'] >= 60 and c['male'] >= 60 for c in counts.values()), counts)

    def test_a_bad_pool_is_refused(self):
        import tempfile
        from pathlib import Path
        with tempfile.TemporaryDirectory() as tmp:
            Path(tmp, 'coldwater.names').write_text('names coldwater\nfemale = Wenna | wenna | Doggett | Tarn\nmale = Tarn | Orrin | Orrin\n'
                                                    'other = Ash\nnamed_for = 2\n')
            problems, _, _ = P.check_names(Path(tmp))
        text = '\n'.join(problems)
        for expected in ("'wenna'", 'Doggett', 'Tarn is both', 'Orrin twice', 'other is not', 'named_for', 'plain: no name pool'):
            self.assertIn(expected, text)


if __name__ == '__main__':
    unittest.main()
