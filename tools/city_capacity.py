#!/usr/bin/env python3
"""How much work a city's residents give its businesses a day (doc 65's capacity model, for a city fed by the store
room: Docs/Design/67-ridgemere-2-the-city.md, sections 6 and Progress, "Food rebalance").

  python3 tools/world_build.py export DIR --world ridgemere2   # the world as files
  python3 tools/city_capacity.py DIR                           # what the placed residents buy a day, as work
  python3 tools/city_capacity.py DIR --json                    # the same as JSON

Read only. From the placed residents of a world export (those with a home) and Data/Items/crafts.json:

- Demand a day, with the old demand model: food (every wolf eats about 50 nourishment a day, bought by nourishment for
  the price over the foods the city's own businesses sell, as Society::buyFood chooses), the households' needs (by
  household or by grown resident, among the goods someone in the city sells), the businesses' upkeep, and the town
  buyers' baskets (the Works, the Watch, the church, the hall; by guards or per 100 residents).
- Each good's demand goes to the businesses whose craft makes it (shared by how many of each there are) or, when nobody
  makes it, to those whose supply list resells it. A maker's work is its own last step: the craft's hours for the
  batches wanted, its inputs coming from the store room (at the making markup, 100%); a vendor's margin is what is left
  of the resale markup (75%).
- Keeper-days: those hours over 8, a one-keeper business's worth of work. "Staffed" (when known) is the keepers and
  hands a trade has, a day's work each.

All the game's numbers are placeholders, and so are these estimates. Used by worldgen.ridgemere2's food check.
"""
from __future__ import annotations

import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import supply_balance as sb  # noqa: E402

ITEMS, CRAFTS, BUSINESSES = sb.ITEMS, sb.CRAFTS, sb.BUSINESSES
WORKDAY = 8                     # Hours in a keeper-day.
WAGE = 60                       # A day's wage, for "carried": how many a trade's margin would pay.
STORE, RESALE = 1.00, 0.75      # The store room's markups (doc 67, section 6: making 100, resale 75).
# The food trades (doc 67, Progress, "Food rebalance"): their keepers and hands are measured against food demand.
FOOD_TRADES = ('bakery', 'butcher', 'inn', 'provisioner', 'preserver', 'dairy', 'fishmonger', 'smokehouse', 'brewery',
               'press', 'stall')
HOUSEHOLD_TRADES = ('chandlery', 'smithy', 'weaver', 'apothecary', 'herbalist', 'pottery', 'tinker', 'general', 'armory',
                    'tailor')


def estimate(folk, biz, items=None, crafts=None):
    """What `folk` (dicts with 'age', 'role' and 'home') buy a day from the businesses `biz` ({business id: how many}):
    {'people', 'grown', 'homes', 'guards', 'spend': {source: p}, 'trades': {business: {'count', 'sales', 'net', 'hours',
    'keeperDays'}}, 'unsold': {good: p a day nobody in the city sells}}."""
    items = items or ITEMS
    crafts = crafts or CRAFTS
    biz = Counter({b: n for b, n in biz.items() if n})
    folk = list(folk)
    n = len(folk)
    homes = {r['home'] for r in folk}
    grown = sum(r['age'] >= 16 for r in folk)
    guards = sum(r['role'] == 'guard' for r in folk)
    made_by = defaultdict(list)
    for k in crafts['crafts']:
        for g in k['out']:
            made_by[g].append(k)
    supplies = crafts.get('supplies', {})

    def sellers(g):
        return ([b for k in made_by.get(g, []) for b in k['makers'] if biz[b]],
                [b for b, goods in supplies.items() if g in goods and biz[b]])

    # The foods on offer: what the city's makers make and its vendors resell, and the inns' meal.
    is_food = lambda g: g in items and items[g].get('category') == 'food'
    sold = {'meal'}
    for k in crafts['crafts']:
        if any(biz[b] for b in k['makers']):
            sold |= {g for g in k['out'] if is_food(g)}
    for b, goods in supplies.items():
        if biz[b]:
            sold |= {g for g in goods if is_food(g)}
    choice = [i for i in items.values() if i.get('category') == 'food' and i.get('food', {}).get('nourish', 0) > 0
              and not i['food'].get('drink') and i['id'] in sold]
    score = {f['id']: f['food']['nourish'] / max(1, f['price']) for f in choice}
    total = sum(score.values()) or 1
    demand, source = defaultdict(float), {}
    need = n * sb.HUNGER_A_DAY
    for f in choice:
        demand[f['id']] += need * score[f['id']] / total / (f['food']['nourish'] * sb.EAT_FACTOR)
        source[f['id']] = 'food'
    for want in crafts['households']['needs']:
        count = (grown if want.get('perPerson') else len(homes)) / want['everyDays']
        avail = [g for g in want['any'] if any(sellers(g))] or want['any']
        for g in avail:
            demand[g] += count / len(avail)
            source.setdefault(g, 'household')
    for business, basket in crafts.get('upkeep', {}).items():
        for g, rate in basket.items():
            demand[g] += biz.get(business, 0) * rate
            source.setdefault(g, 'upkeep')
    for inst in crafts['institutions']['list']:
        per = inst['per']
        if per.startswith('producer:') or n < inst.get('minResidents', 0):
            continue
        scale = guards if per == 'guards' else n / 100
        for g, rate in inst['basket'].items():
            demand[g] += rate * scale
            source.setdefault(g, inst['id'])
    sales, net, hours = defaultdict(float), defaultdict(float), defaultdict(float)
    spend, unsold = defaultdict(float), {}
    for g, d in demand.items():
        price = items[g]['price'] if g in items else 0
        spend[source[g]] += d * price
        makers, vendors = sellers(g)
        if makers:
            k = next(k for k in made_by[g] if any(biz[b] for b in k['makers']))
            bs = [b for b in k['makers'] if biz[b]]
            batches = d / k['out'][g]
            cost = sum(items[i]['price'] * q for i, q in k['in'].items()) * STORE
            value = items[g]['price'] * k['out'][g]
            w = sum(biz[b] for b in bs)
            for b in bs:
                share = biz[b] / w
                sales[b] += batches * value * share
                net[b] += batches * (value - cost) * share
                hours[b] += batches * k.get('hours', 0) * share
        elif vendors:
            w = sum(biz[b] for b in vendors)
            for b in vendors:
                share = biz[b] / w
                sales[b] += d * price * share
                net[b] += d * price * (1 - RESALE) * share
        else:
            unsold[g] = d * price
    trades = {b: {'count': biz[b], 'sales': sales[b], 'net': net[b], 'hours': hours[b],
                  'keeperDays': hours[b] / WORKDAY} for b in sorted(set(biz) | set(sales))}
    return {'people': n, 'grown': grown, 'homes': len(homes), 'guards': guards, 'merchants': sum(biz.values()),
            'spend': dict(spend), 'trades': trades, 'unsold': unsold}


def food_balance(est, staff, low=.5, band=.25):
    """The food trades' staffed keeper-days (`staff`: {business: keepers + hands}) against what is demanded of them:
    (ok, {'demand', 'staffed', 'ratio', 'short': [trades below `low` of their demand]}). ok: the staffed total within
    `band` of the demanded total and no food trade below `low`."""
    demand = sum(est['trades'].get(b, {}).get('keeperDays', 0) for b in FOOD_TRADES)
    staffed = sum(staff.get(b, 0) for b in FOOD_TRADES)
    short = [b for b in FOOD_TRADES if est['trades'].get(b, {}).get('keeperDays', 0) > 0
             and staff.get(b, 0) < low * est['trades'][b]['keeperDays']]
    ratio = staffed / demand if demand else 0
    return abs(ratio - 1) <= band and not short, {'demand': demand, 'staffed': staffed, 'ratio': ratio, 'short': short}


def tally(folk, building_of=None):
    """(keepers, hands) by business of placed residents (dicts with 'role', 'label', 'age' and 'work', a place id). A
    keeper is a merchant, its business its label's; a hand is a grown civilian working in the same building (by
    `building_of`: {interior: building}, else the same interior) as keepers of one business. Outdoor stalls (on a
    cell) have no hands, and nor has a hall of several trades."""
    where = (lambda w: building_of.get(w, w)) if building_of else (lambda w: w)
    keepers, hands = Counter(), Counter()
    keeper_at = defaultdict(set)
    for r in folk:
        if r['role'] == 'merchant':
            b = sb.first_match(r['label'], BUSINESSES)
            bid = b['id'] if b else '?'
            keepers[bid] += 1
            if r['work'] and not r['work'].startswith('w_'):
                keeper_at[where(r['work'])].add(bid)
    for r in folk:
        at = keeper_at.get(where(r['work'])) if r['work'] else None
        if r['role'] == 'civilian' and r['age'] >= 16 and at and len(at) == 1:
            hands[next(iter(at))] += 1
    return keepers, hands


def from_project(project, building_of=None):
    """(folk, keepers, hands) of an Atlas project's placed people (those with a home)."""
    folk = [{'role': x['role'], 'label': x['workLabel'], 'age': x['age'], 'home': x['home']['cell'],
             'work': x['work']['cell'] if x['work'] else ''} for x in project['people'] if x['home']]
    return (folk, *tally(folk, building_of))


def from_export(export: Path):
    """(folk, businesses, staff) of an export's placed residents (staff: keepers and hands, by interior)."""
    folk = [r for r in sb.residents(export) if r['home'] not in ('-', '')]
    keepers, hands = tally(folk)
    return folk, keepers, keepers + hands


def trade_rows(est, keepers, hands, trades=FOOD_TRADES):
    """[(trade, businesses, keepers, hands, keeper-days demanded)] for the report."""
    return [(b, keepers.get(b, 0), keepers.get(b, 0), hands.get(b, 0), est['trades'].get(b, {}).get('keeperDays', 0))
            for b in trades]


def table(est, staff=None):
    """The estimate as text: each trade's count, sales, margin, hours and keeper-days (and staffed, when given)."""
    out = [f'placed residents {est["people"]}, grown {est["grown"]}, homes {est["homes"]}, guards {est["guards"]}, '
           f'merchants {est["merchants"]}',
           'spending a day by source: ' + ', '.join(f'{k} {v:.0f}p' for k, v in est['spend'].items()) +
           f'; total {sum(est["spend"].values()):.0f}p', '',
           f'{"trade":16} {"exist":>5} {"sales/d":>8} {"net/d":>7} {"hours/d":>7} {"carried":>7} {"keeper-days":>11}'
           + (f' {"staffed":>7}' if staff is not None else '')]
    rows = sorted(est['trades'].items(), key=lambda kv: -kv[1]['net'])
    tot = Counter()
    for b, t in rows:
        out.append(f'{b:16} {t["count"]:5} {t["sales"]:8.0f} {t["net"]:7.0f} {t["hours"]:7.1f} {t["net"] / WAGE:7.1f} '
                   f'{t["keeperDays"]:11.1f}' + (f' {staff.get(b, 0):7}' if staff is not None else ''))
        tot.update({k: t[k] for k in ('count', 'sales', 'net', 'hours', 'keeperDays')})
        tot['staffed'] += staff.get(b, 0) if staff is not None else 0
    out.append(f'{"ALL":16} {tot["count"]:5.0f} {tot["sales"]:8.0f} {tot["net"]:7.0f} {tot["hours"]:7.1f} '
               f'{tot["net"] / WAGE:7.1f} {tot["keeperDays"]:11.1f}' +
               (f' {tot["staffed"]:7.0f}' if staff is not None else ''))
    if staff is not None:
        ok, f = food_balance(est, staff)
        out.append(f'\nfood trades: {f["staffed"]} staffed for {f["demand"]:.1f} keeper-days demanded '
                   f'({f["ratio"]:.0%}); below half: {f["short"] or "none"}')
    out.append('\nwanted but nobody in the city sells (p a day): ' +
               (', '.join(f'{g} {v:.0f}' for g, v in sorted(est['unsold'].items(), key=lambda x: -x[1])) or 'nothing'))
    return '\n'.join(out)


def main(argv):
    args = [a for a in argv[1:] if not a.startswith('--')]
    if len(args) != 1:
        print(__doc__)
        return 2
    folk, biz, staff = from_export(Path(args[0]))
    est = estimate(folk, biz)
    if '--json' in argv:
        ok, food = food_balance(est, staff)
        print(json.dumps({**est, 'staff': staff, 'food': {**food, 'ok': ok}}, indent=1))
    else:
        print(table(est, staff))
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
