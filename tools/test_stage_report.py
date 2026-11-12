#!/usr/bin/env python3
"""tools/stage_report.py: a soak's --stage file read (doc 79, 16.13; Phase 11). Offline, on lines made here."""
import json
import os
import subprocess
import sys
import tempfile
import unittest

import stage_report as R


def line(day, wallets, board, wall=2.5):
    market = {'id': 'ridgemere', 'households': 10, 'bands': [1, 2, 6, 1], 'hungry': 0, 'short': 0, 'dole': 0, 'thefts': day % 2,
              'price': 0.95 + day / 1000, 'goods': 40, 'atHigh': 0, 'atLow': 1 if day == 3 else 0, 'times': 'ordinary',
              'score': 0, 'cause': ''}
    bands = {b: {'households': n, 'share': n / 10, 'hungry': 0, 'thefts': 0}
             for b, n in (('destitute', 1), ('poor', 2), ('comfortable', 6), ('rich', 1))}
    return {'runDay': day, 'calendar': day, 'date': f'Dawnday, spring {day}', 'wall': wall, 'slider': 0, 'sliderTarget': 0,
            'sliderCause': '', 'bands': bands, 'thefts': {'thefts': 1, 'tried': 0, 'fromTills': 0}, 'markets': [market],
            'seen': {'caravan departs': 5, 'caravan arrives': 4, 'raid': 1, 'cart departs': 12, 'ship docks': 0,
                     'event begins': 2},
            'committed': {}, 'faucets': {'made': 0, 'sunk': 0, 'byReason': {}}, 'realBalanced': True,
            'sizes': {'accounts': {'stagedKinds': wallets}, 'board': {'cells': board, 'bytes': 1000 + board}}}


class StageReport(unittest.TestCase):
    def test_a_size_that_keeps_rising_is_growing_and_one_that_levels_is_not(self):
        lines = [line(d, 1000 + 50 * d, min(160, 30 * d)) for d in range(1, 31)]
        out = R.report(lines)
        self.assertIn('accounts.stagedKinds', out[-1])
        self.assertNotIn('board.cells', out[-1])     # (Up to its bound early, then flat.)
        self.assertTrue(any('GROWING' in l and 'stagedKinds' in l for l in out))

    def test_the_days_the_markets_and_money(self):
        lines = [line(d, 1000, 100, wall=2 + d / 100) for d in range(1, 11)]
        out = R.report(lines)
        self.assertTrue(out[0].startswith('10 days; wall a day: first 2.01 s'))
        self.assertTrue(any(l.startswith('  ridgemere') and 'low 1' in l for l in out))
        self.assertEqual(out[-1], 'growing: none')
        self.assertFalse(any('NOT BALANCED' in l for l in out))
        lines[4]['realBalanced'] = False
        self.assertTrue(any('NOT BALANCED on 5' in l for l in R.report(lines)))
        self.assertLess(len(R.report(lines, days=False)), len(R.report(lines)))

    def test_hunger_over_the_day_and_the_thieves(self):
        lines = [line(d, 1000, 100) for d in range(1, 4)]
        out = R.report(lines)
        self.assertFalse(any(l.startswith('thieves over') for l in out), 'older lines have none')
        for d, l in enumerate(lines):
            for k, b in l['bands'].items():
                b['hungryMost'] = d if k == 'destitute' else 0
                b['hungryMean'] = .5 if k == 'destitute' else 0
            l['thieves'] = {'considered': 2, 'hungerHigh': 1, 'hungry': 1, 'hungryShut': 1, 'hungryNoShop': 0, 'foodMarks': 0,
                            'pocketMarks': 2}
        out = R.report(lines)
        self.assertTrue(any(l.startswith('hungry households over the day') and 'most at any hour 2/0/0/0' in l for l in out))
        self.assertIn('thieves over the run: considered 6, hungerHigh 3, hungry 3, hungryShut 3, hungryNoShop 0, foodMarks 0, '
                      'pocketMarks 6; thefts from tills 0', out)

    def test_the_command(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'stage.jsonl')
            with open(path, 'w', encoding='utf-8') as f:
                for n in range(1, 4):
                    f.write(json.dumps(line(n, 10, 10)) + '\n')
            run = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'stage_report.py'), path, '--brief'],
                                 capture_output=True, text=True, check=True)
            self.assertIn('3 days', run.stdout)
            self.assertIn('growing: none', run.stdout)


if __name__ == '__main__':
    unittest.main()
