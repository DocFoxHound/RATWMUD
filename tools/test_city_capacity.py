#!/usr/bin/env python3
"""tools/city_capacity.py: the capacity estimate a city's businesses are sized by (doc 67, Progress, "Food rebalance").

  python3 tools/test_city_capacity.py
"""
from __future__ import annotations

import sys
import unittest
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import city_capacity as cc  # noqa: E402


def town(n=200, homes=60, guards=5):
    return [{'age': 30 if i % 5 else 10, 'role': 'guard' if i < guards else 'civilian', 'home': f'h{i % homes}'}
            for i in range(n)]


class Estimate(unittest.TestCase):
    def test_food_goes_to_the_makers_and_scales_with_the_people(self):
        biz = Counter(bakery=2, butcher=1, inn=2)
        small, big = cc.estimate(town(100, 30), biz), cc.estimate(town(400, 120, 20), biz)
        self.assertGreater(small['trades']['bakery']['keeperDays'], 0)
        self.assertGreater(small['trades']['butcher']['keeperDays'], 0)
        for b in ('bakery', 'butcher', 'inn'):
            self.assertAlmostEqual(big['trades'][b]['keeperDays'] / small['trades'][b]['keeperDays'], 4, delta=.2)
        self.assertAlmostEqual(small['trades']['bakery']['keeperDays'], small['trades']['bakery']['hours'] / 8)

    def test_a_shared_craft_is_shared_by_how_many_there_are(self):
        # salt_pork is made by butchers and provisioners: more provisioners take a larger share of it.
        one = cc.estimate(town(), Counter(butcher=1, provisioner=1))
        three = cc.estimate(town(), Counter(butcher=1, provisioner=3))
        self.assertGreater(three['trades']['provisioner']['hours'], one['trades']['provisioner']['hours'])
        self.assertLess(three['trades']['butcher']['hours'], one['trades']['butcher']['hours'])

    def test_a_vendor_earns_the_resale_margin_and_no_hours(self):
        est = cc.estimate(town(), Counter(stall=1, rag_and_bone=1))
        rag = est['trades']['rag_and_bone']
        self.assertEqual(rag['hours'], 0)
        self.assertAlmostEqual(rag['net'], rag['sales'] * (1 - cc.RESALE))

    def test_without_dairies_their_food_goes_to_the_foods_sold(self):
        # A few inns, dairies later (doc 67, Progress): with no dairy, cheese isn't on offer; the wolves eat as much, of
        # the rest, so the other food trades get more work and none goes to the dairy.
        foods = Counter(bakery=2, butcher=2, provisioner=2, inn=1)
        both, none = cc.estimate(town(), foods + Counter(dairy=1)), cc.estimate(town(), foods)
        self.assertGreater(both['trades']['dairy']['keeperDays'], 0)
        self.assertNotIn('dairy', none['trades'])
        for b in ('bakery', 'butcher', 'provisioner', 'inn'):
            self.assertGreater(none['trades'][b]['sales'], both['trades'][b]['sales'])
        self.assertAlmostEqual(none['spend']['food'] / both['spend']['food'], 1, delta=.25)

    def test_what_nobody_sells(self):
        est = cc.estimate(town(), Counter(bakery=1))
        self.assertIn('stone', est['unsold'])                 # The Works want stone; no quarry in a town of bakers.


class Balance(unittest.TestCase):
    def test_the_food_check(self):
        est = cc.estimate(town(), Counter(bakery=1, butcher=1))
        demand = {b: est['trades'][b]['keeperDays'] for b in ('bakery', 'butcher')}
        exact = Counter({b: round(d) or 1 for b, d in demand.items()})
        ok, f = cc.food_balance(est, exact)
        self.assertLess(abs(f['ratio'] - 1), .5)
        ok, f = cc.food_balance(est, Counter(bakery=100, butcher=100))
        self.assertFalse(ok)                                   # Far too many.
        ok, f = cc.food_balance(est, Counter(bakery=sum(exact.values()), butcher=0))
        self.assertIn('butcher', f['short'])                   # The total may fit, but a trade below half fails.

    def test_keepers_and_hands(self):
        folk = [{'role': 'merchant', 'label': 'baker at The Penny Loaf', 'age': 40, 'home': 'a', 'work': 'loaf'},
                {'role': 'civilian', 'label': 'helping at The Penny Loaf', 'age': 30, 'home': 'a', 'work': 'loaf'},
                {'role': 'civilian', 'label': 'helping at The Penny Loaf', 'age': 12, 'home': 'a', 'work': 'loaf'},
                {'role': 'civilian', 'label': 'helping upstairs', 'age': 30, 'home': 'a', 'work': 'loaf_up'},
                {'role': 'merchant', 'label': 'butcher on Market Square', 'age': 40, 'home': 'b', 'work': 'w_ridgemere_1_1'},
                {'role': 'civilian', 'label': 'portering in Market Square', 'age': 30, 'home': 'c',
                 'work': 'w_ridgemere_1_1'}]
        keepers, hands = cc.tally(folk, {'loaf': 'loaf', 'loaf_up': 'loaf'})
        self.assertEqual(keepers, Counter(bakery=1, butcher=1))
        self.assertEqual(hands, Counter(bakery=2))            # Grown hands in the building; none at a stall.
        rows = cc.trade_rows(cc.estimate(folk, keepers), keepers, hands, ('bakery',))
        self.assertEqual(rows[0][:4], ('bakery', 1, 1, 2))


if __name__ == '__main__':
    unittest.main()
