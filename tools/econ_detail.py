#!/usr/bin/env python3
"""Reads world_check's --economy-detail file (a JSON line a game day: per business and per household, from the
society's ledger) and prints where the money goes: per trade, per household class and kind, the hungry, and the
treasury and the Country (Docs/Design/68-the-country-and-the-city-loop.md, "The first simulations").

    python3 tools/econ_detail.py DETAIL.jsonl [--from D] [--to D] [--buildings artifacts/ridgemere2/buildings.json]
                                 [--json OUT.json] [--check]

--from/--to: the days to average over (default: day 5 to the last). --buildings gives each household its class by the
building it lives in (doc 67, "Jobs by class": seats and villas upper for the House's family, the Sump, the Cinders, the
wharf, the estates' cottages and the "... Rents" lower, shops and the rest middle; the Keep's barracks the Watch).
--check: every till's and household's cash moves by what it took in less what it paid out (the ledger is complete).
"""
import argparse
import collections
import json
import statistics
import sys

HOUSE_FAMILIES = {'carlisle', 'thornbury', 'vance', 'stanmore', 'grantham', 'greatly', 'fell', 'grayrock', 'timberland',
                  'harrowgate'}


def load(path):
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line:
                yield json.loads(line)


def class_map(buildings_path):
    """Room id -> (area, kind, name) of its building."""
    if not buildings_path:
        return {}
    rooms = {}
    for b in json.load(open(buildings_path)):
        for r in b['rooms']:
            rooms[r['id']] = (b.get('area') or '', b.get('kind') or '', b.get('name') or '')
    return rooms


def household_class(key, ids, rooms):
    cell = key.split('|')[0]
    if cell not in rooms:
        return 'unknown'
    area, kind, name = rooms[cell]
    surnames = {i.split('_')[-1] for i in ids}
    if kind in ('seat', 'villa'):
        return 'upper' if surnames & HOUSE_FAMILIES else 'lower'
    if area == 'keep':
        return 'watch'
    if area.startswith('estate_'):
        return 'lower'
    if kind == 'shop':
        return 'middle'
    if area in ('sump', 'cinders', 'wharf') or name.endswith(' Rents'):
        return 'lower'
    return 'middle'


def kind_of(posts):
    """A household's kind by its earners' posts: keeper, waged (hand, public, the watch, the Country's) or none."""
    p = posts.split(',')
    if 'keeper' in p:
        return 'keeper'
    if any(x in ('hand', 'public', 'watch', 'country') for x in p):
        return 'waged'
    return 'no wage'


def mean(xs):
    xs = list(xs)
    return sum(xs) / len(xs) if xs else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('detail')
    ap.add_argument('--from', dest='start', type=int, default=5)
    ap.add_argument('--to', dest='end', type=int, default=10 ** 9)
    ap.add_argument('--buildings')
    ap.add_argument('--json')
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()
    rooms = class_map(a.buildings)
    days = [d for d in load(a.detail)]
    if a.check:
        bad = 0
        for prev, d in zip(days, days[1:]):
            for group in ('tills', 'households'):
                for k, v in d[group].items():
                    if k not in prev[group] or (group == 'households' and prev[group][k]['ids'] != v['ids']):
                        continue
                    moved = v['cash'] - prev[group][k]['cash']
                    flow = sum(v['in'].values()) - sum(v['out'].values())
                    if moved != flow:
                        bad += 1
                        if bad <= 10:
                            print(f'  day {d["day"]} {k}: cash moved {moved}, flows {flow}', file=sys.stderr)
        print(f'check: {bad} accounts whose cash moved by other than their flows', file=sys.stderr)
    span = [d for d in days if a.start <= d['day'] <= a.end]
    n = len(span)
    print(f'days {span[0]["day"]}-{span[-1]["day"]} ({n})')

    # Per trade.
    trade = collections.defaultdict(lambda: collections.Counter())
    tills_of = collections.defaultdict(set)
    short_days = collections.Counter()
    for d in span:
        for till, t in d['tills'].items():
            tr = t['trade'] or '?'
            tills_of[tr].add(till)
            c = trade[tr]
            c['residents'] += t['in'].get('residents', 0)
            c['country'] += t['in'].get('country', 0)
            c['draws'] += t['out'].get('draws', 0) - t['in'].get('draws refunded', 0)
            c['carriage'] += t['out'].get('carriage in', 0)
            c['owes'] += t['owes']
            c['wages'] += t['out'].get('wages', 0)
            c['profit'] += t['out'].get('profit', 0)
            c['cash'] += t['cash']
            c['stock'] += t['stock_value']
            c['carriage_takings'] += t['in'].get('carriage in', 0) + t['in'].get('carriage out', 0) + t['in'].get('carriage subsidy', 0)
            if t['out'].get('wages', 0) < t['owes']:
                short_days[tr] += 1
    rows = []
    for tr, c in trade.items():
        k = len(tills_of[tr])
        sales = (c['residents'] + c['country']) / n
        net = sales - (c['draws'] + c['carriage']) / n
        rows.append({'trade': tr, 'tills': k, 'residents': c['residents'] / n, 'country': c['country'] / n,
                     'draws': c['draws'] / n, 'carriage': c['carriage'] / n, 'owes': c['owes'] / n, 'wages': c['wages'] / n,
                     'profit': c['profit'] / n, 'cash': c['cash'] / n, 'stock': c['stock'] / n,
                     'margin': net, 'cover': net / (c['owes'] / n) if c['owes'] else 0,
                     'short': short_days[tr] / (n * k), 'carriage_takings': c['carriage_takings'] / n})
    rows.sort(key=lambda r: r['cover'])
    print('\nper trade, pennies a day (all its tills): sales to residents, to the Country, draws+carriage, '
          'value added, wages owed, paid, profit; cover = value added / wages owed; short = share of till-days paying '
          'less than owed')
    print(f'{"trade":14} {"n":>3} {"resid":>7} {"cntry":>7} {"draws":>7} {"added":>7} {"owes":>6} {"paid":>6} '
          f'{"profit":>7} {"cover":>6} {"short":>6} {"till":>6} {"stock":>6}')
    for r in rows:
        print(f'{r["trade"]:14} {r["tills"]:>3} {r["residents"]:7.1f} {r["country"]:7.1f} {r["draws"] + r["carriage"]:7.1f} '
              f'{r["margin"]:7.1f} {r["owes"]:6.0f} {r["wages"]:6.1f} {r["profit"]:7.1f} {r["cover"]:6.2f} '
              f'{r["short"]:6.0%} {r["cash"] / r["tills"]:6.0f} {r["stock"] / r["tills"]:6.0f}')
    tot = collections.Counter()
    for r in rows:
        for k in ('residents', 'country', 'draws', 'carriage', 'owes', 'wages', 'profit', 'margin'):
            tot[k] += r[k]
    print(f'{"all":14} {sum(r["tills"] for r in rows):>3} {tot["residents"]:7.0f} {tot["country"]:7.0f} '
          f'{tot["draws"] + tot["carriage"]:7.0f} {tot["margin"]:7.0f} {tot["owes"]:6.0f} {tot["wages"]:6.0f} {tot["profit"]:7.0f}')

    # Profit: lumpy? per till, the days it paid profit and how much.
    print('\nprofit to keepers: tills paying any, the share of the profit paid by the top 10 tills')
    by_till = collections.Counter()
    days_paid = collections.Counter()
    for d in span:
        for till, t in d['tills'].items():
            p = t['out'].get('profit', 0)
            by_till[till] += p
            days_paid[till] += p > 0
    allp = sum(by_till.values())
    top = by_till.most_common(10)
    print(f'  {sum(1 for v in by_till.values() if v > 0)} of {len(by_till)} tills paid profit; {allp / n:.0f}p a day; '
          f'top 10: {sum(v for _, v in top) / allp if allp else 0:.0%}')
    for till, v in top:
        t = span[-1]['tills'].get(till, {})
        print(f'    {till:40} {t.get("trade", "?"):12} {v / n:6.1f}p a day, on {days_paid[till]} of {n} days')

    # Households.
    print('\nhouseholds, a day on average, by class and kind: income (wages by payer, profit, alms), food, other '
          'spending, cash at 23:00, hungry hours a member')
    hh = collections.defaultdict(lambda: collections.Counter())
    hh_n = collections.Counter()
    for d in span:
        for key, h in d['households'].items():
            cls = household_class(key, h['ids'].split(','), rooms) if rooms else '-'
            g = (cls, kind_of(h['posts']))
            c = hh[g]
            hh_n[g] += 1
            c['members'] += h['n']
            c['adults'] += h['adults']
            for k, v in h['in'].items():
                c['in:' + k] += v
            for k, v in h['out'].items():
                c['out:' + k] += v
            c['cash'] += h['cash']
            c['hungry_hours'] += h['hungry_hours']
            c['hungry_households'] += h['hungry_hours'] > 0
            c['food_pieces'] += h['pieces'].get('food', 0)
    keys = sorted({k for c in hh.values() for k in c if k.startswith(('in:', 'out:'))})
    print(f'{"class":8} {"kind":8} {"hh":>4} {"size":>5} ' + ' '.join(f'{k[:13]:>13}' for k in keys) +
          f' {"cash":>6} {"hungryh":>8} {"hungry hh":>9}')
    for g in sorted(hh):
        c, m = hh[g], hh_n[g]
        print(f'{g[0]:8} {g[1]:8} {m / n:4.0f} {c["members"] / m:5.2f} ' + ' '.join(f'{c[k] / m:13.2f}' for k in keys) +
              f' {c["cash"] / m:6.0f} {c["hungry_hours"] / c["members"]:8.2f} {c["hungry_households"] / n:9.1f}')

    # The hungry: which households carry the hungry hours, and what they had.
    print('\nthe hungriest households (hungry member-hours over the span), with their posts, size, income a day, cash')
    hungry = collections.Counter()
    info = {}
    for d in span:
        for key, h in d['households'].items():
            hungry[key] += h['hungry_hours']
            i = info.setdefault(key, collections.Counter())
            i['income'] += sum(h['in'].values())
            i['food'] += h['out'].get('food', 0)
            i['wages'] += sum(v for k, v in h['in'].items() if k.startswith('wages'))
            i['alms'] += h['in'].get('alms', 0)
            i['days'] += 1
    last = {}
    for d in span:
        last.update(d['households'])
    allh = sum(hungry.values())
    print(f'  {allh} hungry member-hours in all ({allh / n:.0f} a day); {sum(1 for v in hungry.values() if v)} households '
          f'ever hungry')
    for key, v in hungry.most_common(20):
        if not v:
            break
        h, i = last[key], info[key]
        cls = household_class(key, h['ids'].split(','), rooms) if rooms else '-'
        print(f'    {key[:44]:44} {cls:7} {h["posts"][:30]:30} n{h["n"]} {v:5d}h  income {i["income"] / i["days"]:5.1f} '
              f'(wages {i["wages"] / i["days"]:4.1f}, alms {i["alms"] / i["days"]:4.1f}) food {i["food"] / i["days"]:5.1f} '
              f'cash {h["cash"]}')

    # The public purses.
    print('\nthe treasury, the Country: in and out by kind, a day')
    acc = collections.defaultdict(lambda: collections.Counter())
    for d in span:
        for k, v in d['accounts'].items():
            for kk, vv in v['in'].items():
                acc[k]['in:' + kk] += vv
            for kk, vv in v['out'].items():
                acc[k]['out:' + kk] += vv
    for k, c in acc.items():
        print(f'  {k}: ' + ', '.join(f'{kk} {vv / n:.0f}' for kk, vv in sorted(c.items())))
        print(f'    cash {span[0]["accounts"][k]["cash"]} -> {span[-1]["accounts"][k]["cash"]}')
    if a.json:
        json.dump({'trades': rows}, open(a.json, 'w'), indent=1)


if __name__ == '__main__':
    main()
