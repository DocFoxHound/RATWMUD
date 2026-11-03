"""tools/farms_summary.py (Docs/Design/71-the-country-s-industries.md, 4, Phase B): a run's farms and treasury, year by
year, from world_check's economy lines (synthetic here)."""
import unittest

import farms_summary as F


def day(n, season, **more):
    row = {'day': n, 'season': season, 'conserved': True, 'farms_minted': 900, 'treasury_low': 2500 - n, 'under_cap': 0,
           'support_wanted': 10, 'support_unpaid_switch': 2, 'support_unpaid_ordinary': 1, 'surplus_paid': 5,
           'surplus_alms': 4, 'works_spent': 1, 'toll_now': 30.5, 'toll_trim': .5, 'alms_now': 10, 'alms_trim': 0,
           'price_level': 1.0, 'farm_grain_on': 1, 'farm_grain_wages_paid': 112, 'farm_grain_wages_owed': 112,
           'farm_grain_out': 0, 'farm_grain_grown': 100, 'farm_grain_spoiled': 5, 'farm_grain_takings': 200,
           'farm_grain_basket': 80, 'farm_grain_stock': 700, 'farm_grain_crew_spent': 90, 'farm_grain_crew_short': 0,
           'farm_salt_on': 0}
    row.update(more)
    return row


class SummaryTests(unittest.TestCase):
    def test_a_year(self):
        rows = [day(n, 'spring' if n < 92 else 'summer' if n < 184 else 'autumn' if n < 275 else 'winter') for n in range(365)]
        rows[10].update({'farm_grain_wages_paid': 50, 'farm_grain_out': 2, 'under_cap': 1, 'treasury_low': 100,
                         'farms_out_goods': {'flour': 2, 'oats': 0}, 'farms_sold_goods': {'flour': 365}})
        text = F.summary(rows, base=[day(n, 'spring', surplus_alms=8, works_spent=2) for n in range(365)])
        self.assertIn('money conserved every day: True', text)
        self.assertIn('made 900p', text)
        self.assertIn('| 1 | 100 / ', text, 'the lowest in spring')
        self.assertIn('| 1 / 0 / 0 / 0 |', text, 'a night under its cap, in spring')
        self.assertIn('730 + 365', text, 'the unpaid support, the switch\'s apart')
        self.assertIn('1,825 (-1,825 on the base\'s 3,650)', text, 'the surplus against the base year')
        self.assertIn('| grain | 365 | 99.7% | 1 / 0 / 0 / 0 | 5.0% |', text)
        self.assertIn('| salt | off |', text)
        self.assertIn('Days a good ran short (Sp / Su / Au / Wi), year 1: flour 1 / 0 / 0 / 0 (sold 1.0 a day)', text,
                      'by good, a good with no want unfilled left out')


if __name__ == '__main__':
    unittest.main()
