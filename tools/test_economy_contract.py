#!/usr/bin/env python3
"""The staged economy's DM and Atlas contract (tools/economy_contract.py; Docs/Design/79-a-staged-economy.md, 12 and 14,
Phase 9): the slider's clamp, plain words, the bands, and the host's overrides of status.json and market.json checked as
Core reads them. Offline: the data files only."""
import copy
import json
import unittest

import economy_contract as E


class SliderTests(unittest.TestCase):
    def test_the_slider_is_clamped_never_refused(self):
        # Doc 79, 12: out of range or not a number, clamped; steps of 5.
        for given, wanted in ((9999, 100), (-9999, -100), ('abc', 0), (None, 0), (True, 0), (float('nan'), 0),
                              (float('inf'), 0), (37, 35), (38, 40), (-37, -35), (2.4, 0), (2.5, 5), (0, 0), ('35', 35), (' 40 ', 40),
                              ([5], 0), (100, 100)):
            self.assertEqual(E.clamp_prosperity(given), wanted, given)
        self.assertEqual(E.PROSPERITY, {'low': -100, 'high': 100, 'step': 5})


class WordsTests(unittest.TestCase):
    def test_plain_words_mirror_core(self):
        self.assertEqual(E.plain_words('blight at the orchards', 80), 'blight at the orchards')
        self.assertIsNone(E.plain_words('x' * 81, 80))
        self.assertEqual(E.plain_words('x' * 80, 80), 'x' * 80)
        self.assertIsNone(E.plain_words('é' * 41, 80), 'bytes, as Core counts them')
        self.assertIsNone(E.plain_words('a\nb', 80))
        self.assertIsNone(E.plain_words(5, 80))
        for bad in ('the dog days', 'Dogs at the gate', 'a DOG', 'dog'):
            self.assertIsNone(E.plain_words(bad, 80), bad)
        for fine in ('dogged work', 'hotdogs', 'the dogwood blooms', 'underdog'):
            self.assertEqual(E.plain_words(fine, 80), fine, fine)
        self.assertTrue(E.forbidden_in_name('the Seadog'))
        self.assertFalse(E.ship_name_fits('the Seadog'))
        self.assertTrue(E.ship_name_fits("the Saltwife's-Heir"))
        self.assertFalse(E.ship_name_fits('the Gull 2'))

    def test_bands(self):
        self.assertEqual(E.BANDS, ('destitute', 'poor', 'comfortable', 'rich'))
        for band in E.BANDS:
            self.assertEqual(E.check_band(band), band)
        for bad in ('Rich', 'wealthy', None, 1, ''):
            self.assertIsNone(E.check_band(bad))


class OverrideTests(unittest.TestCase):
    def setUp(self):
        self.defaults = E.load_defaults()
        self.goods = E.base_goods()

    def check(self, o):
        return E.check_overrides(o, self.goods, self.defaults)

    def test_the_shipped_defaults_pass_as_overrides(self):
        d = self.defaults
        self.assertTrue(set(d['status']) <= set(E.STATUS_KEYS))
        self.assertEqual(set(d['status']), set(E.STATUS_KEYS), 'status.json has every key a host may change')
        o = {'status': copy.deepcopy(d['status']),
             'market': {'events': {k: copy.deepcopy(d['market']['events'][k]) for k in ('perWeek', 'list', 'causes')},
                        'wharf': {'ships': list(d['market']['wharf']['ships'])}},
             'neverImported': list(d['market']['wharf'].get('never') or []), 'prosperityCause': 'a hard winter'}
        errors, _ = self.check(o)
        self.assertEqual(errors, [])
        self.assertIn('cheese', self.goods)
        self.assertEqual(self.check({}), ([], []))

    def test_unknown_and_too_big(self):
        errors, _ = self.check({'steer': 1})
        self.assertTrue(errors and 'No such override' in errors[0])
        self.assertTrue(self.check([])[0])
        big = {'status': {'street': {'note': 'x' * 13000}}}
        self.assertTrue(any('at most 12000' in e for e in self.check(big)[0]))

    def test_the_cause(self):
        self.assertEqual(self.check({'prosperityCause': 'the Crown\'s war levy'})[0], [])
        for bad in ('x' * 81, 'the dog tax', 7, 'a\tb'):
            self.assertTrue(self.check({'prosperityCause': bad})[0], bad)

    def test_never_imported(self):
        self.assertEqual(self.check({'neverImported': ['cheese', 'salt']})[0], [])
        self.assertEqual(self.check({'neverImported': []})[0], [], 'ships carry everything')
        self.assertTrue(self.check({'neverImported': ['unobtainium']})[0])
        self.assertTrue(self.check({'neverImported': ['salt', 'salt']})[0])
        self.assertTrue(self.check({'neverImported': 'salt'})[0])

    def test_status_shares(self):
        shares = copy.deepcopy(self.defaults['status']['shares'])
        self.assertEqual(self.check({'status': {'shares': shares}})[0], [])
        bad = copy.deepcopy(shares)
        bad[1]['shares'] = [40, 40, 40, 20]                     # Sums to 140 (section 16.6's case).
        self.assertTrue(any('sums to 140' in e for e in self.check({'status': {'shares': bad}})[0]))
        bad = copy.deepcopy(shares)
        bad[2]['shares'] = [1, 13, 70, 16]                      # Under 2%.
        self.assertTrue(any('at least 2%' in e for e in self.check({'status': {'shares': bad}})[0]))
        bad = copy.deepcopy(shares)
        bad[2]['shares'] = [20, 12, 52, 16]                     # Destitute rises with the slider.
        self.assertTrue(any('never rises' in e for e in self.check({'status': {'shares': bad}})[0]))
        bad = copy.deepcopy(shares)
        bad[2]['shares'] = [2, 12, 81, 5]                       # Rich falls.
        self.assertTrue(any('never falls' in e for e in self.check({'status': {'shares': bad}})[0]))
        self.assertTrue(self.check({'status': {'shares': shares[:1]}})[0], 'two rows at least')
        self.assertTrue(self.check({'status': {'shares': [shares[0], {'at': 50, 'shares': [2, 12, 70, 16]}]}})[0],
                        'from -100 to 100')

    def test_status_caps_have_a_ceiling(self):
        caps = self.defaults['status']['caps']
        self.assertEqual(self.check({'status': {'caps': {'rewardsADay': caps['rewardsADay'] * 5, 'grantsADay': 2500}}})[0], [])
        self.assertTrue(self.check({'status': {'caps': {'rewardsADay': caps['rewardsADay'] * 5 + 1}}})[0], '5x the default')
        self.assertTrue(self.check({'status': {'caps': {'grantsADay': 2501}}})[0])
        self.assertTrue(self.check({'status': {'caps': {'theftADay': -1}}})[0])
        self.assertTrue(self.check({'status': {'caps': {'luckADay': 1}}})[0])
        # welcomeDays is a period: shorter is the generous way (a daily welcome), so it is at least the shipped week.
        week = caps['welcomeDays']
        self.assertTrue(self.check({'status': {'caps': {'welcomeDays': 0}}})[0], 'a welcome every day refused')
        self.assertTrue(self.check({'status': {'caps': {'welcomeDays': week - 1}}})[0])
        self.assertEqual(self.check({'status': {'caps': {'welcomeDays': week * 2}}})[0], [], 'a rarer welcome is fine')

    def test_market_json_says_what_it_holds(self):
        # Phase 9's first repair: market.json's own description named houseBusinesses and estates as kept, after both
        # went, and the DM's Economy tab as the season records' editor.
        market = json.loads((E.ROOT / 'Data' / 'Economy' / 'market.json').read_text(encoding='utf-8'))
        about = market['about']
        for gone in ('houseBusinesses', 'estates'):
            self.assertNotIn(gone, market)
        self.assertNotIn('nothing in Core reads those five yet', about)
        self.assertNotIn("the DM's Economy tab", about)
        for kept in ('services', 'luxuries', 'wharf', 'events'):
            self.assertIn(kept, market)
            self.assertIn(f'`{kept}`', about, kept)

    def test_status_parts_keep_their_shape(self):
        self.assertEqual(self.check({'status': {'pay': {'keeper': 20}, 'leanDays': 4, 'rows': {'poor': {'short': 20}}}})[0], [])
        for bad in ({'treasury': {}}, {'pay': {'keeper': 'lots'}}, {'pay': 5}, {'rows': {'noble': {}}},
                    {'rows': {'poor': {'colour': 1}}}, {'leanDays': 'three'}, {'leanDays': -1}, {'hold': {'oneOff': None}}):
            self.assertTrue(self.check({'status': bad})[0], bad)

    def test_market_events(self):
        d = self.defaults['market']
        kinds = copy.deepcopy(d['events']['list'])
        self.assertEqual(self.check({'market': {'events': {'perWeek': 3, 'list': kinds[:3]}}})[0], [])
        self.assertTrue(self.check({'market': {'events': {'perWeek': 4}}})[0])
        self.assertTrue(self.check({'market': {'events': {'rate': 1}}})[0], 'only the four paths')
        self.assertTrue(self.check({'market': {'wharf': {'never': []}}})[0], 'never is neverImported\'s')
        own = {'id': 'harvest_fair', 'words': 'the harvest fair', 'notice': 'A fair in {market}.', 'goods': {'cider': .2}}
        self.assertEqual(self.check({'market': {'events': {'list': [own]}}})[0], [], 'a host\'s own kind')
        for bad in ({**own, 'id': 'Harvest Fair'}, {**own, 'id': 'dog_show'}, {**own, 'words': ''},
                    {**own, 'words': 'the dog fair'}, {**own, 'notice': 'x' * 281}, {**own, 'cry': 'x' * 201},
                    {**own, 'needs': 'moon'}, {**own, 'goods': {}}, 'harvest_fair'):
            self.assertTrue(self.check({'market': {'events': {'list': [bad]}}})[0], bad)
        self.assertTrue(self.check({'market': {'events': {'list': [own, own]}}})[0], 'a kind twice')
        _, warnings = self.check({'market': {'events': {'list': [{**own, 'goods': {'cider': .2, 'moonbeams': .1}}]}}})
        self.assertTrue(any('moonbeams' in w for w in warnings), 'an unknown good is warned of, as Core skips it')
        self.assertTrue(self.check({'market': {'events': {'list': [own] * 65}}})[0])

    def test_market_causes_and_ships(self):
        causes = copy.deepcopy(self.defaults['market']['events']['causes'])
        self.assertEqual(self.check({'market': {'events': {'causes': causes}}})[0], [])
        self.assertTrue(self.check({'market': {'events': {'causes': {'noticeDown': 'Hard times ahead.'}}}})[0], 'needs {cause}')
        self.assertTrue(self.check({'market': {'events': {'causes': {'down': [{'id': 'x', 'words': ''}]}}}})[0])
        self.assertTrue(self.check({'market': {'events': {'causes': {'sideways': []}}}})[0])
        ships = self.defaults['market']['wharf']['ships']
        self.assertEqual(self.check({'market': {'wharf': {'ships': ships[:2]}}})[0], [])
        for bad in ([], ['the Seadog'], ['the Gull', 'the Gull'], ['x' * 41], ['the Gull 2'], 'the Gull'):
            self.assertTrue(self.check({'market': {'wharf': {'ships': bad}}})[0], bad)

    def test_the_constants(self):
        self.assertEqual(E.OVERRIDE_KEYS, ('prosperityCause', 'neverImported', 'status', 'market'))
        self.assertEqual(E.MARKET_PATHS, (('events', 'perWeek'), ('events', 'list'), ('events', 'causes'), ('wharf', 'ships')))
        self.assertEqual((E.CAUSE_MOST, E.OVERRIDE_BYTES_MOST, E.GRANT_MOST, E.CAP_CEILING), (80, 12000, 2500, 5))
        self.assertEqual(E.HARVEST_WORDS, ('poor', 'fair', 'good'))
        # A leaf: never map_editor or world_store.
        source = (E.ROOT / 'tools' / 'economy_contract.py').read_text(encoding='utf-8')
        self.assertNotRegex(source, r'^\s*(import|from)\s+(map_editor|world_store|studio|live_edit)\b')
        json.dumps(E.load_defaults())


if __name__ == '__main__':
    unittest.main()
