#!/usr/bin/env python3
"""The farm runs against the bases: per year, per season, per area and farm, the order book and the losers' steps.

    analyse.py RUN.jsonl [--base BASE.jsonl] [--long people_12year.jsonl] [--export EXPORT] [--md]
"""
import argparse
import collections
import json
import shlex
import sys
from pathlib import Path

ROOT = Path('/home/martinb/Documents/GitHub/MUDProject/.claude/worktrees/farms')
SEASONS = ['spring', 'summer', 'autumn', 'winter']
CAP = 2000
FLOAT = 200


def lines(path, upto=None):
    out = []
    with open(path) as f:
        for line in f:
            if line.strip():
                r = json.loads(line)
                if upto is not None and r['day'] > upto:
                    break
                out.append(r)
    return out


def mean(xs):
    xs = list(xs)
    return sum(xs) / len(xs) if xs else 0.0


def years(rows):
    n = (rows[-1]['day'] + 1 + 364) // 365
    for y in range(n):
        rs = [r for r in rows if 365 * y <= r['day'] <= 365 * y + 364]
        if len(rs) >= 56:
            yield y, rs


def year_row(rs, y, farms=True):
    rs = [r for r in rs if r['day'] >= 5] if y == 0 else rs   # (people_summary's: y1 from day 5)
    hr = rs
    first, last = rs[:28], rs[-28:]
    owed, paid = sum(r['wages_owed'] for r in rs), sum(r['wages_paid'] for r in rs)
    profit = sum(r['profit'] for r in rs)
    low = min((r['treasury_low'] for r in rs if r.get('treasury_low', -1) >= 0), default=None)
    night = [r['treasury_night'] for r in rs if r.get('treasury_night', -1) >= 0]
    out = {
        'hungry': mean(r['hungry_mean'] for r in hr), 'hungry_worst': max(r['hungry_most'] for r in hr),
        'hungry_days': sum(1 for r in hr if r['hungry_most'] > 0),
        'wages': 100 * paid / owed, 'tills_short': mean(r['tills_short'] for r in rs),
        't_first': mean(r['treasury'] for r in first), 't_last': mean(r['treasury'] for r in last),
        't_mean': mean(r['treasury'] for r in rs),
        't_low': low if low is not None else min(r['treasury'] for r in rs),
        't_low_line': min(r['treasury'] for r in rs),
        'under_cap': sum(r['under_cap'] for r in rs) if 'under_cap' in rs[0] else sum(1 for r in rs if r['treasury'] < CAP),
        'under_float': sum(1 for x in night if x < FLOAT) if night else sum(1 for r in rs if r['treasury'] < FLOAT),
        'low_under_float': sum(1 for r in rs if 0 <= r.get('treasury_low', 10**9) < FLOAT),
        'unpaid_switch': sum(r.get('support_unpaid_switch', 0) for r in rs),
        'unpaid_ord': sum(r.get('support_unpaid_ordinary', 0) for r in rs),
        'support': mean(r['wage_support'] for r in rs),
        'surplus': sum(r.get('surplus_alms', 0) + r.get('works_spent', 0) for r in rs),
        'alms': mean(r['alms'] for r in rs),
        'g_first': mean(r['gini_earners'] for r in first), 'g_last': mean(r['gini_earners'] for r in last),
        'g_mean': mean(r['gini_earners'] for r in rs),
        'owners': profit / (profit + paid), 'toll': mean(r['toll'] for r in rs),
        'toll_now': rs[-1]['toll_now'], 'alms_now': rs[-1]['alms_now'], 'toll_trim': rs[-1].get('toll_trim', 0),
        'level_end': rs[-1].get('treasury_level', 0),
        'price_lo': min(r['price_level'] for r in rs), 'price_hi': max(r['price_level'] for r in rs),
        'price_sd': mean(r['price_sd'] for r in rs), 'step_mean': rs[-1]['wage_step_mean'],
        'purses_first': mean(r['purses'] for r in first), 'purses_last': mean(r['purses'] for r in last),
        'tills': mean(r['tills'] for r in last), 'country4': mean(r['country'] for r in last),
        'residents': rs[-1]['residents'], 'spent': mean(r['residents_spent'] for r in rs),
        'country_unspent': mean(r.get('country_unspent', 0) for r in rs),
        'households_short': mean(r.get('households_short', 0) for r in rs),
    }
    if farms and 'farms_wages_owed' in rs[0]:
        fo, fp = sum(r['farms_wages_owed'] for r in rs), sum(r['farms_wages_paid'] for r in rs)
        out.update({'farm_wages': 100 * fp / fo if fo else None, 'crew_hungry': sum(r['farms_crew_hungry'] for r in rs),
                    'crew_short': sum(r['farms_crew_short'] for r in rs), 'farms_out': sum(r['farms_out'] for r in rs),
                    'farms_float': rs[-1]['farms_float'], 'crew_purse': rs[-1]['farms_crew_purse'],
                    'farms_spoiled': sum(r['farms_spoiled'] for r in rs), 'farms_grown': sum(r['farms_grown'] for r in rs),
                    'farms_takings': mean(r['farms_takings'] for r in rs), 'farms_basket': mean(r['farms_basket'] for r in rs),
                    'farms_wages_paid_a_day': mean(r['farms_wages_paid'] for r in rs),
                    'table_short': sum(r['farms_table_short'] for r in rs)})
    return out


def conserved(rows):
    bad = [r['day'] for r in rows if not r['conserved'] or r['money'] != r['money_start'] + r.get('farms_minted', 0)]
    return bad


def areas_of(rows):
    return [k[len('farm_'):-len('_on')] for k in rows[0] if k.startswith('farm_') and k.endswith('_on')]


def season_key(r):
    return (r['day'] // 365, r['season'])


def per_area(rows):
    """By area, by year and season: days on, stock-out days, the most out in a day, farm wages paid / owed, stock at the
    season's end, spoiled of grown, takings / wages / basket a day."""
    out = {}
    for a in areas_of(rows):
        by = collections.OrderedDict()
        for r in rows:
            if not r.get(f'farm_{a}_on'):
                continue
            k = season_key(r)
            s = by.setdefault(k, {'days': 0, 'out_days': 0, 'out_most': 0, 'out': 0, 'owed': 0, 'paid': 0, 'support': 0,
                                  'grown': 0, 'spoiled': 0, 'takings': 0, 'basket': 0, 'stock_end': 0, 'stock_min': 10**9,
                                  'float_end': 0, 'float_min': 10**9, 'table_short': 0, 'crew_hungry': 0, 'sold': 0})
            s['days'] += 1
            o = r[f'farm_{a}_out']
            s['out'] += o
            s['out_days'] += o > 0
            s['out_most'] = max(s['out_most'], o)
            for f, k2 in (('wages_owed', 'owed'), ('wages_paid', 'paid'), ('support', 'support'), ('grown', 'grown'),
                          ('spoiled', 'spoiled'), ('takings', 'takings'), ('basket', 'basket'), ('table_short', 'table_short'),
                          ('crew_hungry', 'crew_hungry'), ('sold', 'sold')):
                s[k2] += r[f'farm_{a}_{f}']
            s['stock_end'] = r[f'farm_{a}_stock']
            s['stock_min'] = min(s['stock_min'], r[f'farm_{a}_stock'])
            s['float_end'] = r[f'farm_{a}_float']
            s['float_min'] = min(s['float_min'], r[f'farm_{a}_float'])
        out[a] = by
    return out


def per_farm(rows):
    """By farm and season: days its till paid its crew in full, paid / owed, its float's least."""
    out = collections.defaultdict(lambda: collections.OrderedDict())
    for r in rows:
        for fid, f in r.get('farms', {}).items():
            k = season_key(r)
            s = out[fid].setdefault(k, {'days': 0, 'full': 0, 'owed': 0, 'paid': 0, 'float_min': 10**9, 'basket': 0, 'takings': 0})
            s['days'] += 1
            s['full'] += f['wages_paid'] >= f['wages_owed']
            s['owed'] += f['wages_owed']
            s['paid'] += f['wages_paid']
            s['float_min'] = min(s['float_min'], f['float'])
            s['basket'] += f['basket']
            s['takings'] += f['takings']
    return out


def businesses_of(world):
    biz = json.load(open(ROOT / 'Data/Items/businesses.json'))['businesses']

    def business_for(label):
        w = label.lower()
        return next((b['id'] for b in biz for m in b.get('match', []) if m and m in w), 'other')
    out = {}
    for line in open(world):
        if line.startswith('resident '):
            f = shlex.split(line)
            if len(f) > 4 and f[3] == 'merchant':
                out['till:job:' + f[1]] = business_for(f[4])
    return out


def order_book(loop_path, base_loop_path, world, years_=(0, 1, 2)):
    """Each trade's smoothed sales (pennies a day at the catalogue's price), the mean of the year's 28-day snapshots, the
    run against the base; and its mean wage step at the year's last snapshot."""
    price = {i['id']: i.get('price', 0) for i in json.load(open(ROOT / 'Data/Items/items.json'))['items']}
    bus = businesses_of(world)

    def load(p):
        out = collections.defaultdict(lambda: collections.defaultdict(list))
        steps = collections.defaultdict(lambda: collections.defaultdict(list))
        for d in lines(p):
            y = d['day'] // 365
            per = collections.Counter()
            for t, goods in d['loop'].get('sales', {}).items():
                per[bus.get(t, 'other')] += sum(n * price.get(g, 0) for g, n in goods.items())
            for b, v in per.items():
                out[y][b].append(v)
            for t, w in d['loop'].get('wages', {}).items():
                steps[y][bus.get(t, 'other')].append((d['day'], w.get('step', 0)))
        return out, steps
    run, rsteps = load(loop_path)
    base, bsteps = load(base_loop_path)
    res = {}
    for y in years_:
        if y not in run or y not in base:
            continue
        trades = sorted(set(run[y]) | set(base[y]))
        rows = []
        for b in trades:
            r, s = mean(run[y].get(b, [])), mean(base[y].get(b, []))
            last_r = [st for day, st in rsteps[y].get(b, []) if day == max(dd for dd, _ in rsteps[y][b])] if rsteps[y].get(b) else []
            last_b = [st for day, st in bsteps[y].get(b, []) if day == max(dd for dd, _ in bsteps[y][b])] if bsteps[y].get(b) else []
            rows.append((b, s, r, mean(last_b), mean(last_r)))
        res[y] = rows
    return res


def fmt(x, nd=0):
    if x is None:
        return '-'
    if isinstance(x, float):
        return f'{x:,.{nd}f}'
    return f'{x:,}'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('run')
    ap.add_argument('--base')
    ap.add_argument('--long')
    ap.add_argument('--export')
    ap.add_argument('--book', action='store_true')
    a = ap.parse_args()
    rows = lines(a.run)
    last = rows[-1]['day']
    sets = [('run', rows)]
    if a.base:
        sets.append(('base', lines(a.base, last)))
    if a.long:
        sets.append(('long', lines(a.long, last)))
    bad = conserved(rows)
    print(f'# {a.run}: {len(rows)} days; conserved (counting the minted) every day: {"yes" if not bad else "NO " + str(bad[:10])}; '
          f'farms minted {rows[-1].get("farms_minted", 0)}p; money {rows[0]["money_start"]} -> {rows[-1]["money"]}')
    keys = ['hungry', 'hungry_worst', 'hungry_days', 'wages', 'tills_short', 't_first', 't_last', 't_mean', 't_low', 't_low_line',
            'under_cap', 'under_float', 'low_under_float', 'unpaid_switch', 'unpaid_ord', 'support', 'surplus', 'alms', 'g_first',
            'g_last', 'g_mean', 'owners', 'toll', 'toll_now', 'toll_trim', 'alms_now', 'level_end', 'price_lo', 'price_hi',
            'price_sd', 'step_mean', 'purses_first', 'purses_last', 'tills', 'country4', 'residents', 'spent', 'country_unspent',
            'households_short', 'farm_wages', 'crew_hungry', 'crew_short', 'farms_out', 'farms_float', 'crew_purse',
            'farms_spoiled', 'farms_grown', 'farms_takings', 'farms_basket', 'farms_wages_paid_a_day', 'table_short']
    nd = {'hungry': 3, 'wages': 2, 'g_first': 3, 'g_last': 3, 'g_mean': 3, 'owners': 3, 'price_lo': 3, 'price_hi': 3,
          'price_sd': 3, 'step_mean': 2, 'tills_short': 1, 'farm_wages': 2, 'country_unspent': 0, 'households_short': 2}
    table = {}
    for name, rs in sets:
        for y, yr in years(rs):
            table[(name, y)] = year_row(yr, y)
    ys = sorted({y for _, y in table})
    head = [f'{n} y{y + 1}' for y in ys for n, _ in sets]
    print(f'{"":24}' + ''.join(f'{h:>12}' for h in head))
    for k in keys:
        vals = []
        for y in ys:
            for n, _ in sets:
                v = table.get((n, y), {}).get(k)
                vals.append(fmt(v, nd.get(k, 0)) if v is not None else '-')
        print(f'{k:24}' + ''.join(f'{v:>12}' for v in vals))
    print('\n## Per area (season by season): days on, stock-out days (most in a day), wages paid of owed, support, '
          'stock end (least), spoiled of grown, takings / basket a day, table short, crew hungry')
    for ar, by in per_area(rows).items():
        print(f'### {ar}')
        for (y, s), v in by.items():
            d = v['days']
            print(f'  y{y + 1} {s:7} on {d:3}  out {v["out_days"]:3} ({v["out_most"]:3}, {v["out"]:5})  wages {v["paid"]}/{v["owed"]} '
                  f'({100 * v["paid"] / v["owed"] if v["owed"] else 0:.1f}%) support {v["support"]}  stock {v["stock_end"]} ({v["stock_min"]})  '
                  f'spoiled {v["spoiled"]}/{v["grown"]} ({100 * v["spoiled"] / v["grown"] if v["grown"] else 0:.1f}%)  '
                  f'sold {v["sold"] / d:.0f}/d takings {v["takings"] / d:.0f}/d basket {v["basket"] / d:.0f}/d  float {v["float_end"]} '
                  f'({v["float_min"]})  table_short {v["table_short"]} hungry {v["crew_hungry"]:.0f}')
    print('\n## Per farm: seasons not paid in full (days paid in full / days, paid / owed, least float)')
    pf = per_farm(rows)
    for fid, by in pf.items():
        short = [(k, v) for k, v in by.items() if v['full'] < v['days']]
        worst = min(v['float_min'] for v in by.values())
        if short:
            print(f'  {fid}: ' + '; '.join(f'y{k[0] + 1} {k[1]} {v["full"]}/{v["days"]} {v["paid"]}/{v["owed"]}' for k, v in short)
                  + f'  least float {worst}')
    print(f'  ({sum(1 for by in pf.values() if all(v["full"] == v["days"] for v in by.values()))} of {len(pf)} farms paid in full every day)')
    if a.book and a.base and a.export:
        print('\n## Order book: trade, base sales a day (pennies), run, change; wage step base -> run at the year\'s last snapshot')
        book = order_book(a.run.replace('.jsonl', '_loop.jsonl'), a.base.replace('.jsonl', '_loop.jsonl'), Path(a.export) / 'world.ratw')
        for y, rws in book.items():
            print(f'### y{y + 1}')
            rws.sort(key=lambda t: -(t[1]))
            tot_b = sum(t[1] for t in rws)
            tot_r = sum(t[2] for t in rws)
            for b, s, r, sb, sr in rws:
                if s < 5 and r < 5:
                    continue
                print(f'  {b:22} {s:8.0f} {r:8.0f} {r - s:+7.0f} ({100 * (r - s) / s if s else 0:+5.0f}%)   step {sb:+5.1f} -> {sr:+5.1f}')
            print(f'  {"all":22} {tot_b:8.0f} {tot_r:8.0f} {tot_r - tot_b:+7.0f} ({100 * (tot_r - tot_b) / tot_b:+5.1f}%)')


if __name__ == '__main__' and sys.argv[1:2] != ['--against']:
    main()


def against(run_path, base_path):
    """The treasury against the base, day by day: under its path (the base's treasury less the run's), the surplus not
    paid out, and the switch's cost a day (what the treasury paid out and kept, less the base's)."""
    run = lines(run_path)
    base = {r['day']: r for r in lines(base_path, run[-1]['day'])}
    out = []
    for y, rs in years(run):
        rs = [r for r in rs if r['day'] >= 5] if y == 0 else rs
        bs = [base[r['day']] for r in rs if r['day'] in base]
        if len(bs) != len(rs):
            continue
        gap = [b['treasury'] - r['treasury'] for r, b in zip(rs, bs)]
        sr = sum(r.get('surplus_alms', 0) + r.get('works_spent', 0) for r in rs)
        sb = sum(b.get('surplus_alms', 0) + b.get('works_spent', 0) for b in bs)
        prev_r = [x for x in run if x['day'] == rs[0]['day'] - 1]
        prev_b = base.get(rs[0]['day'] - 1)
        t0r = prev_r[0]['treasury'] if prev_r else rs[0]['treasury']
        t0b = prev_b['treasury'] if prev_b else bs[0]['treasury']
        cost = ((sb - sr) + (bs[-1]['treasury'] - t0b) - (rs[-1]['treasury'] - t0r)) / len(rs)
        unpaid_r = sum(r.get('support_unpaid_switch', 0) + r.get('support_unpaid_ordinary', 0) for r in rs)
        unpaid_b = sum(b.get('support_unpaid_switch', 0) + b.get('support_unpaid_ordinary', 0) for b in bs)
        out.append({'year': y + 1, 'under_mean': mean(gap), 'under_most': max(gap), 'surplus_less': sb - sr,
                    'surplus_less_pct': 100 * (sb - sr) / sb if sb else 0, 'cost_a_day': cost,
                    'unpaid_run': unpaid_r, 'unpaid_base': unpaid_b, 'unpaid_extra': unpaid_r - unpaid_b,
                    'support_more': mean(r['wage_support'] for r in rs) - mean(b['wage_support'] for b in bs),
                    'toll_more': mean(r['toll'] for r in rs) - mean(b['toll'] for b in bs),
                    'alms_more': mean(r['alms'] for r in rs) - mean(b['alms'] for b in bs)})
    return out


if __name__ == '__main__' and len(sys.argv) > 1 and sys.argv[1] == '--against':
    for row in against(sys.argv[2], sys.argv[3]):
        print(' '.join(f'{k} {v:,.1f}' if isinstance(v, float) else f'{k} {v:,}' for k, v in row.items()))
