#!/usr/bin/env python3
"""The city's loop, reckoned from a world export (Docs/Design/68-the-country-and-the-city-loop.md).

  python3 tools/world_build.py export DIR --world ridgemere2
  python3 tools/city_loop.py DIR                     # posts and wages, the residents' day, the Country, each trade's hours
  python3 tools/city_loop.py DIR --suggest           # also a basket for the Country spread over the trades' spare hours
  python3 tools/city_loop.py DIR --json              # the same as JSON

From the placed residents of the export, Data/Items (items, crafts, businesses, country.json) and the settings below
(the game's defaults, or --set KEY=VALUE), it works out a day of the loop at catalogue prices, as the game's rules do:

- Posts: who pays each post (a shop's till for its keeper and hands, the treasury, the Country, or nobody), as
  Society::payerOf does, and the wages that come to.
- The residents' day: food for everyone (the better value the likelier, among what the city's shops sell) and the
  households' needs (crafts.json), at catalogue prices; who sells it, and what the shops keep after the store room.
- The store room: what the shops draw a day (the goods' cost at the markups), the carriage on it (by weight), the gate
  toll's share for the treasury, the Country's share.
- The Country, steady: it pays its posts, buys the city's goods with the rest (its basket), and pays carriage out; the
  shops then draw again to replace what it bought. Its purchases P solve
      (1 - toll) x (D_residents + (1 - m) x P)  =  W_country + P + carriage on P
  where m is the makers' margin on the basket.
- Each trade's hours: what the residents' and the Country's buying asks of each making trade, against its crew (keeper
  and hands) working 8 hours, flagged over 70%.
- Carriage: the weight moved a day, the wagon loads (a load is WAGON_LOAD weight), the drivers it takes (TRIPS a day each).

Every number is an estimate at the catalogue's prices with everyone fed and every need met; the runs (world_check
--economy) measure the real thing.
"""
from __future__ import annotations

import argparse
import json
import math
import shlex
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import item_catalog as ic  # noqa: E402

ITEMS = ic.ITEMS
CRAFTS = ic.CRAFTS
SUPPLIES = ic.SUPPLIES
COUNTRY = json.loads((ROOT / 'Data/Items/country.json').read_text(encoding='utf-8'))
HOUSEHOLDS = ic._CRAFTS_FILE['households']
# The game's defaults (EconomySpec; Atlas's Economy page changes them per world).
DEFAULTS = {'storeMarkup': 100, 'resaleMarkup': 75, 'toll': 25, 'wageKeeper': 14, 'wageHand': 8, 'wagePublic': 8,
            'wageCountry': 8, 'carriage': 6, 'reserve': 200, 'subsidy': 25}
HUNGER_A_DAY = .0035 * 14400                 # Hunger a game day (Society: .0035 a second).
NOURISH_A_DAY = HUNGER_A_DAY / 1.1           # A food's nourishment counts 1.1 times when eaten.
HOURS = 8                                    # A crew's working day, for the trades' hours.
WAGON_LOAD = 1000                            # Weight a wagon carries (items.json weights).
TRIPS = 2                                    # Round trips a driver makes a day (the estates and villages near).
FLAG = .70


def residents(export: Path):
    out = []
    for line in (export / 'world.ratw').read_text(encoding='utf-8').splitlines():
        if line.startswith('resident '):
            t = shlex.split(line)
            if t[23] in ('-', ''):
                continue                          # (Unplaced.)
            out.append({'id': t[1], 'role': t[3], 'label': t[4], 'age': int(t[7]), 'home': t[23], 'work': t[26]})
    return out


def payers(folk, rules=None):
    """Who pays each resident's post (as Society::payerOf): ('till', keeper id) for a shop's keeper and hands, 'treasury',
    a market id, or None; and the kind of post ('keeper', 'hand', 'watch', 'public', 'country', None)."""
    rules = rules or COUNTRY['wages']['rules']
    shops = defaultdict(list)
    for r in folk:
        if r['role'] == 'merchant':
            shops[r['work']].append(r['id'])
    out = {}
    for r in folk:
        low = r['label'].lower()
        if r['role'] == 'merchant':
            out[r['id']] = (('till', r['id']), 'keeper')
        elif r['role'] == 'guard':
            out[r['id']] = ('treasury', 'watch')
        elif len(shops.get(r['work'], [])) == 1:
            out[r['id']] = (('till', shops[r['work']][0]), 'hand')
        else:
            rule = next((x for x in rules if any(w in low for w in x['words'])), None)
            out[r['id']] = (rule['payer'], 'public' if rule['payer'] == 'treasury' else 'country') if rule else (None, None)
    return out


def made_by():
    m = defaultdict(list)
    for k in CRAFTS:
        for g in k['out']:
            m[g].append(k)
    return m


MADE = made_by()


def edible(i):
    f = ITEMS.get(i, {}).get('food') or {}
    return f.get('nourish', 0) > 0 and not f.get('drink')


def reckon(export: Path, s: dict, basket_override=None):
    folk = residents(export)
    trade = {r['id']: ic.business_for(r['label']) for r in folk}
    pay = payers(folk)
    crew = Counter()                                        # Keeper + hands of each shop, by trade.
    keepers = Counter()
    for r in folk:
        p, kind = pay[r['id']]
        if kind in ('keeper', 'hand'):
            t = trade.get(p[1])
            if t:
                crew[t] += 1
                keepers[t] += kind == 'keeper'
    present = {t for t in keepers}
    wages = Counter()
    posts = Counter()
    wage_of = {'keeper': s['wageKeeper'], 'hand': s['wageHand'], 'watch': s['wagePublic'], 'public': s['wagePublic'],
               'country': s['wageCountry']}
    for r in folk:
        p, kind = pay[r['id']]
        if kind:
            posts[kind] += 1
            wages['till' if kind in ('keeper', 'hand') else 'treasury' if p == 'treasury' else 'country'] += wage_of[kind]
    # Who sells a good: its makers present (by crafts) and its resellers present (supply lists), shared by their crews,
    # as the game's buyers spread over the shops holding it.
    def sellers(g):
        m = sorted({b for k in MADE.get(g, []) for b in k['makers'] if b in present})
        r = sorted(b for b, goods in SUPPLIES.items() if b in present and g in goods and b not in m)
        return [(b, 'make') for b in m] + [(b, 'resell') for b in r]
    # The residents' day.
    n = len(folk)
    homes = {r['home'] for r in folk}
    grown = sum(r['age'] >= 16 for r in folk)
    foods = [g for g in ITEMS if edible(g) and sellers(g)]
    value = {g: ITEMS[g]['food']['nourish'] / max(1, ITEMS[g]['price']) for g in foods}
    total = sum(value.values())
    demand = Counter()
    for g in foods:
        demand[g] += n * NOURISH_A_DAY * value[g] / total / ITEMS[g]['food']['nourish']
    food_p = sum(demand[g] * ITEMS[g]['price'] for g in foods)
    for need in HOUSEHOLDS['needs']:
        count = (grown if need.get('perPerson') else len(homes)) / need['everyDays']
        avail = [g for g in need['any'] if sellers(g)]
        for g in avail:
            demand[g] += count / len(avail)
    needs_p = sum(demand[g] * ITEMS[g]['price'] for g in demand) - food_p
    # Shops' takings, their draws and their hours.
    resale_share = s['resaleMarkup'] / 100
    sales, net, draws, hours, weight_in = Counter(), Counter(), Counter(), Counter(), 0.0

    def sell(g, units, who, makers=False):
        nonlocal weight_in
        by = [x for x in sellers(g) if not makers or x[1] == 'make']
        if not by:
            return 0
        price = ITEMS[g]['price']
        w = sum(crew[b] or 1 for b, _ in by)
        drawn = 0.0
        for b, how in by:
            u = units * (crew[b] or 1) / w
            sales[b] += u * price
            if how == 'make':
                k = next(k for k in MADE[g] if b in k['makers'])
                batches = u / k['out'][g]
                cost = sum(ITEMS[i]['price'] * q for i, q in k['in'].items()) * s['storeMarkup'] / 100
                drawn += batches * cost
                weight_in += batches * sum(ITEMS[i].get('weight', 0) * q for i, q in k['in'].items())
                net[b] += u * price - batches * cost
                hours[b] += batches * k.get('hours', 0)
            else:
                drawn += u * price * resale_share
                weight_in += u * ITEMS[g].get('weight', 0)
                net[b] += u * price * (1 - resale_share)
        draws[who] += drawn
        return units * price
    unsold = {}
    for g, units in demand.items():
        if not sell(g, units, 'residents'):
            unsold[g] = units * ITEMS[g]['price']
    residents_spend = food_p + needs_p - sum(unsold.values())
    d_res = draws['residents']
    w_res = weight_in
    # The Country's basket: shares of its spending, by good; the makers' margin on it.
    basket = basket_override if basket_override is not None else COUNTRY['markets'][0]['basket']
    btotal = sum(basket.values()) or 1
    m_num = m_den = cw = 0.0
    for g, wt in basket.items():
        k = next((k for k in MADE.get(g, []) if any(b in present for b in k['makers'])), None)
        if not k:
            continue
        v = ITEMS[g]['price'] * k['out'][g]
        c = sum(ITEMS[i]['price'] * q for i, q in k['in'].items()) * s['storeMarkup'] / 100
        share = wt / btotal
        m_num += share * (v - c) / v
        m_den += share
        cw += share * (ITEMS[g].get('weight', 0) / ITEMS[g]['price'])           # weight a penny bought
    margin = m_num / m_den if m_den else .4
    tau = s['toll'] / 100
    rate = s['carriage'] / WAGON_LOAD                                            # pennies a weight (carriage: a load's)
    c_out = rate * cw                                                            # carriage a penny the Country buys
    w_country = wages['country']
    P = max(0.0, ((1 - tau) * d_res - w_country) / ((1 + c_out) - (1 - tau) * (1 - margin)))
    d_country = (1 - margin) * P
    D = d_res + d_country
    # The Country's purchases, good by good, through the makers.
    before = dict(hours)
    country_units = {}
    for g, wt in basket.items():
        units = P * wt / btotal / ITEMS[g]['price']
        country_units[g] = units
        sell(g, units, 'country', makers=True)   # (The Country buys the city's own make, from its makers.)
    weight_cin = weight_in - w_res                                                # inputs drawn for the Country's goods
    weight_out = sum(units * ITEMS[g].get('weight', 0) for g, units in country_units.items())
    carriage_in = rate * weight_in
    carriage_out = rate * weight_out
    toll = tau * D
    trades = []
    for t in sorted(set(crew) | set(hours)):
        cap = crew[t] * HOURS
        res_h = before.get(t, 0)
        cty_h = hours[t] - res_h
        trades.append({'trade': t, 'crew': crew[t], 'capacity_h': cap, 'residents_h': round(res_h, 1),
                       'country_h': round(cty_h, 1), 'use': round(hours[t] / cap, 2) if cap else None,
                       'sales': round(sales[t]), 'net': round(net[t]), 'flag': bool(cap and hours[t] / cap > FLAG)})
    loads = (weight_in + weight_out) / WAGON_LOAD
    return {
        'residents': n, 'grown': grown, 'homes': len(homes), 'posts': dict(posts), 'wages': dict(wages),
        'wages_total': sum(wages.values()),
        'residents_spend': round(residents_spend), 'food': round(food_p), 'needs': round(needs_p),
        'unsold': {g: round(v) for g, v in unsold.items()},
        'draws_residents': round(d_res), 'country_margin': round(margin, 3), 'country_buys': round(P),
        'draws_country': round(d_country), 'draws': round(D), 'toll': round(toll),
        'subsidy': round((carriage_in + carriage_out) * s['subsidy'] / 100),
        'treasury_net': round(toll - wages['treasury'] - (carriage_in + carriage_out) * s['subsidy'] / 100),
        'country_intake': round((1 - tau) * D), 'country_pays_posts': w_country, 'carriage_in': round(carriage_in),
        'carriage_out': round(carriage_out), 'weight_in': round(weight_in), 'weight_out': round(weight_out),
        'wagon_loads': round(loads, 1), 'drivers': math.ceil(loads / TRIPS),
        'shops_net': round(sum(net.values())), 'till_wages': wages['till'],
        'trades': trades, 'country_units': {g: round(u, 1) for g, u in country_units.items()},
    }


def suggest(export: Path, s: dict):
    """A basket over the making trades' spare hours: each trade's spare (70% of its crew's day less what the residents
    ask of it) spent on its goods the Country buys (its crafts' goods that aren't food the residents live on, weapons or
    armour), weighted by what an hour of each makes."""
    base = reckon(export, s, basket_override={})
    spare = {t['trade']: max(0.0, FLAG * t['capacity_h'] - t['residents_h']) for t in base['trades']}
    skip = {'weapon', 'armor', 'siege', 'luxury'}
    out = Counter()
    for t, h in spare.items():
        if h <= 0:
            continue
        ks = [k for k in CRAFTS if t in k['makers'] and k.get('hours')]
        goods = []
        for k in ks:
            for g in k['out']:
                it = ITEMS.get(g, {})
                if it.get('category') in skip or (edible(g) and g not in ('salt_fish', 'smoked_fish', 'salt_pork', 'jerky',
                                                                          'smoked_mutton', 'cheese', 'fruit_preserves', 'dried_apples')):
                    continue
                v = ITEMS[g]['price'] * k['out'][g]
                c = sum(ITEMS[i]['price'] * q for i, q in k['in'].items())
                if v > c:
                    goods.append((g, k))
        if not goods:
            continue
        per = h / len(goods)                     # the trade's spare hours shared among its goods
        for g, k in goods:
            out[g] += per / k['hours'] * k['out'][g] * ITEMS[g]['price']     # pennies of it those hours make
    return dict(sorted(out.items(), key=lambda x: -x[1]))


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('export', type=Path)
    ap.add_argument('--suggest', action='store_true')
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--set', action='append', default=[], metavar='KEY=VALUE')
    a = ap.parse_args(argv)
    s = dict(DEFAULTS)
    for kv in a.set:
        k, v = kv.split('=', 1)
        s[k] = float(v)
    r = reckon(a.export, s)
    if a.suggest:
        r['suggested_basket'] = {g: round(v) for g, v in suggest(a.export, s).items()}
    if a.json:
        print(json.dumps(r, indent=1))
        return 0
    print(f"{r['residents']} residents ({r['grown']} grown, {r['homes']} homes); posts {r['posts']}")
    print(f"wages a day by payer {r['wages']} = {r['wages_total']}p")
    print(f"residents spend {r['residents_spend']}p a day (food {r['food']}, needs {r['needs']}); unsold {r['unsold']}")
    print(f"shops draw for the residents' goods {r['draws_residents']}p; the Country buys {r['country_buys']}p "
          f"(margin {r['country_margin']}), its goods' inputs {r['draws_country']}p; all draws {r['draws']}p")
    print(f"toll {r['toll']}p against the treasury's wages {r['wages']['treasury']}p and its carriage subsidy {r['subsidy']}p "
          f"(net {r['treasury_net']}p); "
          f"the Country takes in {r['country_intake']}p, pays its posts {r['country_pays_posts']}p")
    print(f"carriage in {r['carriage_in']}p ({r['weight_in']} weight), out {r['carriage_out']}p ({r['weight_out']} weight): "
          f"{r['wagon_loads']} wagon loads, {r['drivers']} drivers")
    print(f"shops' net {r['shops_net']}p against their wages {r['till_wages']}p (+ carriage {r['carriage_in'] + r['carriage_out']}p to the carters)")
    print(f"\n{'trade':16}{'crew':>5}{'cap h':>7}{'resid h':>8}{'cntry h':>8}{'use':>6}{'sales':>7}{'net':>6}")
    for t in r['trades']:
        print(f"{t['trade']:16}{t['crew']:5}{t['capacity_h']:7}{t['residents_h']:8}{t['country_h']:8}"
              f"{(t['use'] if t['use'] is not None else '-'):>6}{t['sales']:7}{t['net']:6}{'  OVER 70%' if t['flag'] else ''}")
    if a.suggest:
        print('\nsuggested basket (pennies a day each trade could spare):', r['suggested_basket'])
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
