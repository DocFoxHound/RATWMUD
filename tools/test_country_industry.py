"""tools/country_industry.py (Docs/Design/71-the-country-s-industries.md, "How it was measured"): who each store draw
ends with (`streams`), on a synthetic season with the real catalogue's crafts."""
import unittest

import country_industry as C


def season(**more):
    """A season's scaled measures: the vintner resells cider (it makes none) and sells it to the residents; the inn makes
    cider from apples and sells it to the residents and the Country (which buys only from a good's makers); the stall
    resells apples, a few of them to the Country too (as if it made them: a shop's own buyers count, whoever they are)."""
    s = {
        'draws_why': {'cider': {'resale': {'pieces': 20, 'country': 30, 'toll': 10, 'carriage': 0}},
                      'apples': {'cider': {'pieces': 120, 'country': 60, 'toll': 20, 'carriage': 1},
                                 'resale': {'pieces': 50, 'country': 25, 'toll': 8, 'carriage': 0}}},
        'drawn_by': {'vintner': {'cider': {'pieces': 20}}, 'inn': {'apples': {'pieces': 120}},
                     'stall': {'apples': {'pieces': 50}}},
        'sales': {'vintner': {'residents': {'cider': {'pieces': 20, 'coins': 60}}},
                  'inn': {'residents': {'cider': {'pieces': 15, 'coins': 45}}, 'country': {'cider': {'pieces': 45, 'coins': 135}}},
                  'stall': {'residents': {'apples': {'pieces': 40, 'coins': 40}}, 'country': {'apples': {'pieces': 10, 'coins': 10}}}},
        'market': {'cider': {'pieces': 35, 'coins': 105}, 'apples': {'pieces': 40, 'coins': 40}},
        'bought': {'cider': {'pieces': 45, 'coins': 135}, 'apples': {'pieces': 10, 'coins': 10}},
    }
    s.update(more)
    return s


class StreamsTests(unittest.TestCase):
    def setUp(self):
        self.items, _, self.crafts, _, _ = C.catalogue()
        self.assertNotIn('vintner', self.crafts['cider']['makers'], 'the vintner makes no cider (it only resells it)')
        self.assertIn('inn', self.crafts['cider']['makers'])

    def streams(self, **more):
        return C.streams({s: season(**more) for s in C.SEASONS}, self.crafts, self.items)['spring']

    def test_a_resale_draw_ends_with_its_own_shop_s_buyers(self):
        # The vintner's cider: the Country never buys from it (it makes none), so all of it ends with the residents,
        # though the Country buys more cider than the residents do across the city.
        A = self.streams()
        self.assertAlmostEqual(A['other']['cider']['cider']['pieces'], 20)
        self.assertNotIn('cider', A['country'].get('cider', {}), 'none of the vintner\'s cider is the Country\'s')
        self.assertAlmostEqual(A['other']['cider']['cider']['country'], 30, msg='with all its cost')

    def test_a_craft_s_draw_ends_with_its_makers_buyers(self):
        # The inn's apples for cider: by the inn's buyers (a quarter to the residents), not by every cider seller's (the
        # vintner's resale to the residents among them, which would make it 44%).
        A = self.streams()
        self.assertAlmostEqual(A['other']['cider']['apples']['pieces'], 30)
        self.assertAlmostEqual(A['country']['cider']['apples']['pieces'], 90)

    def test_resellers_each_by_their_own_buyers(self):
        # The stall's apples (resold: it makes none) by its own buyers, a fifth of them the Country's.
        A = self.streams()
        self.assertAlmostEqual(A['other']['apples']['apples']['pieces'], 40, msg='apples (no nourishment) are other buying')
        self.assertAlmostEqual(A['country']['apples']['apples']['pieces'], 10)
        # Two resellers of a good, by their draws: the second (a shop of no craft) sells only to the residents.
        A = self.streams(drawn_by={'vintner': {'cider': {'pieces': 20}}, 'inn': {'apples': {'pieces': 120}},
                                   'stall': {'apples': {'pieces': 25}}, 'grocer': {'apples': {'pieces': 25}}},
                         sales={**season()['sales'], 'grocer': {'residents': {'apples': {'pieces': 25, 'coins': 25}}}})
        self.assertAlmostEqual(A['country']['apples']['apples']['pieces'], 25 * .2)

    def test_without_the_shops_draws_every_seller_s_buyers(self):
        # A measured file from before the shops' draws were kept: the good's buyers across the city, as before.
        A = self.streams(drawn_by={})
        self.assertAlmostEqual(A['country']['cider']['cider']['pieces'], 20 * 45 / 80)


if __name__ == '__main__':
    unittest.main()
