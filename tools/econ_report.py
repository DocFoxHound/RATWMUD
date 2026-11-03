#!/usr/bin/env python3
"""A run's market in about two pages (Docs/Design/69-simulation-speed.md, 10): every imbalance, the prices pinned at the
band's edges, the tills short of their wages and why, the money's flows and the treasury's path, each class's welfare,
the largest buyers and sellers of each good, and the farms' yields that would meet their demand, from world_check's
economy file (`--economy F`) and, at `--economy-level full`, its tables (F.full.jsonl.gz):

    python3 tools/econ_report.py ECONOMY.jsonl [--full F.full.jsonl.gz] [--out REPORT.md] [--patch PATCH.json]
                                 [--slack 5] [--top 8]
    python3 tools/econ_report.py --apply PATCH.json Data/Economy/country_farms.json

It does all the reading: a person or an agent reads only the report. With the economy file alone (the normal level) it
reports what those lines hold (the treasury, the hungry, the tills short, the farms' wants not met). The farms' patch
(PATCH, default the report's name with ".patch.json") holds, per farm and good, the yield by season (pieces a day at the
season's middle, before the farm's yield scale, as country_farms.json has them) that would have met the shops' draws of
it, the draws its stock could not fill, and the Country's table, with SLACK percent more; a season the run did not see
keeps its yield. `--apply` writes those yields into country_farms.json (nothing else changes).
"""
from __future__ import annotations

import argparse
import collections
import gzip
import json
import re
import statistics
import sys
from pathlib import Path

SEASONS = ['spring', 'summer', 'autumn', 'winter']
CLASS_NAMES = {'lower': 'lower (the Sump, the Rents)', 'middle': 'middle', 'upper': 'upper (the Houses)'}


def economy_lines(path):
    rows = []
    if path and Path(path).exists():
        with open(path) as f:
            for line in f:
                if line.strip():
                    rows.append(json.loads(line))
    return rows


def full_days(path):
    """The full file's meta, then each day with its rows as dicts of names (the dictionary applied)."""
    names = []
    meta = None
    with gzip.open(path, 'rt') as f:
        for line in f:
            if not line.strip():
                continue
            d = json.loads(line)
            if 'meta' in d:
                meta = d['meta']
                continue
            names.extend(d.get('names', []))
            yield meta, names, d


class Run:
    """Everything the report needs, aggregated as the days stream past."""

    def __init__(self, slack):
        self.slack = slack
        self.meta = None
        self.days = 0
        self.trade = collections.defaultdict(lambda: [0, 0, 0])            # (good, buyer, seller) -> pieces, pennies, base
        self.short = collections.defaultdict(lambda: [0, 0, 0, collections.Counter()])   # (good, shop, season) -> pieces, buyers, days, instead
        self.stock = collections.defaultdict(dict)                         # good -> day -> pieces (shops' and farms')
        self.pinned = collections.defaultdict(lambda: [0, 0, 0])           # good -> high days, low days, days priced
        self.factor_low, self.factor_high = 10 ** 9, 0                     # The shops' stock factors' range (thousandths)
        self.tills = collections.defaultdict(lambda: collections.Counter())  # till -> sums
        self.till_trade = {}
        self.flows = collections.defaultdict(collections.Counter)          # season -> (from, to) -> pennies
        self.season_days = collections.Counter()
        self.welfare = collections.defaultdict(collections.Counter)        # (season, class) -> sums
        self.farm_good = collections.defaultdict(collections.Counter)      # (farm, good, season) -> sums
        self.farm_stock = {}                                               # (farm, good) -> [first, last]
        self.farm = collections.defaultdict(collections.Counter)           # farm -> sums
        self.draw_short = collections.defaultdict(list)   # (good, season, till) -> the pieces its draws lacked, a day
        self.sold_rate = collections.defaultdict(lambda: [0, 0])   # (till, good) -> pieces sold, days it had some or sold

    def unmet_draws(self):
        """By (good, season), the pieces the shops would have drawn, had the supply sent them. A draw's lack is its shelf's
        deficit (its largest in an hour, each day), a stock and not a day's sales: a shop that sells the good loses at most
        its day's sales a day (its mean on the days it held or sold some), however far under its shelf stands; a craft's
        input, never sold, counts its deficit as it stands (an hour's batches, under its day's use)."""
        total = collections.Counter()
        for (g, s, till), days in self.draw_short.items():
            sold, held = self.sold_rate.get((till, g), (0, 0))
            rate = sold / held if sold and held else None
            total[(g, s)] += sum(days) if rate is None else sum(min(k, rate) for k in days)
        return total

    def add(self, meta, names, d):
        self.meta = meta
        n = names.__getitem__
        season = d['season']
        day = d['day']
        self.days += 1
        self.season_days[season] += 1
        for g, b, s, pieces, pennies, base in d['trade']:
            t = self.trade[(n(g), n(b), n(s))]
            t[0] += pieces
            t[1] += pennies
            t[2] += base
        for shop, g, buyer, buyers, pieces, instead in d['short']:
            row = self.short[(n(g), n(shop), season)]
            row[0] += pieces
            row[1] += buyers
            row[2] += 1
            if n(instead):
                row[3][n(instead)] += pieces
            if n(buyer).startswith('shop:'):
                self.draw_short[(n(g), season, n(shop))].append(pieces)
        band = (meta or {}).get('price_band', 25) * 10
        stock = collections.Counter()
        for till, g, st, shelf, price, factor, sold in d['ware']:
            good = n(g)
            stock[good] += st
            if sold or st:
                r = self.sold_rate[(n(till), good)]
                r[0] += sold
                r[1] += 1
                self.factor_low, self.factor_high = min(self.factor_low, factor), max(self.factor_high, factor)
                p = self.pinned[good]
                p[2] += 1
                p[0] += factor >= 1000 + band - 1
                p[1] += factor <= 1000 - band + 1
        for farm, g, grown, sold, st, spoiled, eaten, short in d['farm_good']:
            key = (n(farm), n(g))
            stock[n(g)] += st
            fg = self.farm_good[(n(farm), n(g), season)]
            fg.update(grown=grown, sold=sold, spoiled=spoiled, eaten=eaten, short=short, days=1)
            first = self.farm_stock.setdefault(key, [st, st])
            first[1] = st
        for good, pieces in stock.items():
            self.stock[good][day] = pieces
        for row in d['till']:
            till, trade = n(row[0]), n(row[1])
            self.till_trade[till] = trade
            (cash, sales, takings, draws, draw_cost, toll, carriage, inputs, owed, paid, support, profit, crew) = row[2:15]
            t = self.tills[till]
            t.update(days=1, takings=takings, costs=draw_cost + carriage + inputs, owed=owed, paid=paid, support=support,
                     profit=profit, crew=crew, sales=sales)
            if owed > 0 and paid < owed:
                t['short_days'] += 1
                t['shortfall'] += owed - paid
        for a, b, coins in d['flow']:
            self.flows[season][(n(a), n(b))] += coins
        for row in d['class']:
            w = self.welfare[(season, n(row[0]))]
            w.update(days=1, residents=row[1], households=row[2], hungry_hours=row[3], purse=row[5], median=row[6],
                     comforts=row[7], spent=row[8])
        for row in d['farm']:
            f = self.farm[n(row[0])]
            f.update(days=1, takings=row[3], owed=row[4], paid=row[5], support=row[6], basket=row[7], crew_spent=row[9],
                     crew_short=row[10])
            f['crew_purse_last'] = row[8]
            f['float_last'] = row[2]


def table(head, rows):
    out = ['| ' + ' | '.join(head) + ' |', '|' + '---|' * len(head)]
    out += ['| ' + ' | '.join(str(c) for c in r) + ' |' for r in rows]
    return out


def short_name(account):
    return account.replace('till:job:', '').replace('farm:', '')


def slope_per_year(series):
    """Pieces a year by least squares over (day, pieces)."""
    if len(series) < 3:
        return 0.0
    xs = sorted(series)
    ys = [series[x] for x in xs]
    mx, my = statistics.fmean(xs), statistics.fmean(ys)
    var = sum((x - mx) ** 2 for x in xs)
    return 365 * sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / var if var else 0.0


def farm_patch(run):
    """Per farm and good, the yields by season that meet its share of the demand, SLACK percent over."""
    farms = (run.meta or {}).get('farms', {})
    by_good = collections.defaultdict(float)
    for farm, f in farms.items():
        for good, y in f['yield'].items():
            by_good[good] += sum(y) / 4 * f['yield_scale']
    patch, changes = {}, []
    unmet = run.unmet_draws()
    # An area's good no farm of it grows: the Country no longer sends it and no yield can be raised, so it is named apart.
    ungrown = []
    for area, goods in sorted(((run.meta or {}).get('farm_areas') or {}).items()):
        for good in goods:
            if not by_good.get(good):
                ungrown.append((good, area, sum(unmet[(good, s)] for s in SEASONS)))
    for farm, f in sorted(farms.items()):
        for good, current in sorted(f['yield'].items()):
            share = (sum(current) / 4 * f['yield_scale'] / by_good[good]) if by_good[good] else 0
            new = list(current)
            for i, season in enumerate(SEASONS):
                fg = run.farm_good.get((farm, good, season))
                if not fg or not fg['days'] or f['yield_scale'] <= 0:
                    continue
                demand = (fg['sold'] + fg['eaten'] + fg['short'] + share * unmet[(good, season)]) / fg['days']
                new[i] = round(demand * (1 + run.slack / 100) / f['yield_scale'], 2)
            if new != list(current):
                patch.setdefault(farm, {})[good] = new
                changes.append((max(abs(a - b) / max(b, 1) for a, b in zip(new, current)), farm, good, current, new))
    return patch, sorted(changes, reverse=True), ungrown


def report(econ, run, top, patch_path):
    out = []
    say = out.append
    days = len(econ) or run.days
    seasons = collections.Counter(r.get('season', '?') for r in econ) if econ else run.season_days
    say(f'# Economy report: {days} days ({", ".join(f"{s} {seasons[s]}" for s in SEASONS if seasons[s])})')
    say('')
    if econ:
        conserved = all(r.get('conserved', True) for r in econ)
        say(f'Money conserved every day: {conserved}. Residents {econ[0].get("residents")} -> {econ[-1].get("residents")}; '
            f'births {sum(r.get("births", 0) for r in econ)}, deaths {sum(r.get("deaths", 0) for r in econ)}.')
        say('')
    if run is None:
        say('_The normal level only: run with `--economy-level full` for the market tables._')
        say('')
    # 1. Imbalances.
    say('## 1. Imbalances')
    if run:
        rows = sorted(run.short.items(), key=lambda kv: -kv[1][0])[:top]
        say('Goods short (wants no open shop or draw met; "-" is every shop):')
        say('')
        out += table(['good', 'shop', 'season', 'pieces', 'buyer-days', 'days', 'taken instead'],
                     [[g, short_name(shop), s, v[0], v[1], v[2], ', '.join(f'{k} {c}' for k, c in v[3].most_common(2)) or '-']
                      for (g, shop, s), v in rows] or [['-'] * 7])
        say('')
        grow = []
        traded = collections.Counter()
        for (g, b, sl), t in run.trade.items():
            traded[g] += t[0]
        for good, series in run.stock.items():
            # (After the first week, when founding shops and farms fill their stores.)
            later = {d: v for d, v in series.items() if d >= min(series) + 7}
            slope = slope_per_year(later)
            sold = traded[good] / max(1, run.days)
            mean = statistics.fmean(later.values()) if later else 0
            if slope > 0 and slope > 0.2 * max(mean, 1):
                grow.append((slope / max(sold, 0.1), good, slope, mean, sold))
        grow.sort(reverse=True)
        if run.days < 28:
            say(f'Goods piling up: the run is {run.days} days; a month or more is needed for the stock\'s growth.')
        else:
            say('Goods piling up (stock in shops and farms after the first week, growth a year; sales a day of any seller;'
                ' the days of sales a year adds):')
            say('')
            out += table(['good', 'growth/yr', 'mean stock', 'sold/day', 'days of sales/yr'],
                         [[g, round(s), round(m), round(d, 1), round(r)] for r, g, s, m, d in grow[:top]] or [['-'] * 5])
        say('')
    if econ:
        # (The economy lines' count of every farm draw cut short, a craft's input among them, at either level.)
        out_goods = collections.Counter()
        for r in econ:
            for g, k in (r.get('farms_out_goods') or {}).items():
                out_goods[(g, r.get('season'))] += k
        say('The farms\' stock short of a shop\'s want (draws), by good and season:')
        say('')
        out += table(['good', 'season', 'draws short'], [[g, s, k] for (g, s), k in out_goods.most_common(top)] or [['-'] * 3])
    say('')
    # 2. Prices pinned.
    if run:
        band = (run.meta or {}).get('price_band', 25)
        say(f'## 2. Price factors pinned at the band (1 +/- {band}%)')
        rows = sorted(((v[0] / v[2], v[1] / v[2], g, v[2]) for g, v in run.pinned.items() if v[2] and (v[0] or v[1])),
                      key=lambda r: -max(r[0], r[1]))[:top]
        say('')
        if rows:
            out += table(['good', 'share at the top', 'share at the foot', 'till-days'],
                         [[g, f'{h:.0%}', f'{l:.0%}', n] for h, l, g, n in rows])
        else:
            say('None.')
        if run.factor_high:
            say('')
            say(f'The shops\' stock factors ranged {run.factor_low / 1000:.2f} to {run.factor_high / 1000:.2f}.')
        say('')
    # 3. Tills short.
    say('## 3. Tills short of their wages')
    if run:
        by_trade = collections.defaultdict(lambda: collections.Counter())
        for till, t in run.tills.items():
            if not t['short_days']:
                continue
            takings, costs, owed = t['takings'], t['costs'], t['owed']
            if takings and costs >= 0.7 * takings:
                why = 'input costs'
            elif takings and owed >= 0.6 * takings and t['crew'] / max(1, t['days']) >= 3:
                why = 'too many hands'
            else:
                why = 'low sales'
            b = by_trade[run.till_trade.get(till, '?')]
            b.update(tills=1, short_days=t['short_days'], shortfall=t['shortfall'], support=t['support'], takings=takings,
                     costs=costs, owed=owed, days=t['days'])
            b['why:' + why] += 1
        rows = sorted(by_trade.items(), key=lambda kv: -kv[1]['shortfall'])[:top]
        say('')
        out += table(['trade', 'tills', 'short days', 'shortfall', 'support', 'takings/day', 'costs/day', 'owed/day', 'why'],
                     [[tr, b['tills'], b['short_days'], b['shortfall'], b['support'], round(b['takings'] / b['days']),
                       round(b['costs'] / b['days']), round(b['owed'] / b['days']),
                       ', '.join(f'{k[4:]} {v}' for k, v in sorted(b.items()) if k.startswith('why:'))]
                      for tr, b in rows] or [['-'] * 9])
    elif econ:
        say('')
        say(f'Tills short a night: mean {statistics.fmean(r.get("tills_short", 0) for r in econ):.1f}; wage support '
            f'{sum(r.get("support_by_trade", {}).get("farm", 0) for r in econ)}p to the farms.')
    say('')
    # 4. Money.
    say('## 4. Money: flows by season and the treasury')
    if run:
        say('')
        rows = []
        for s in SEASONS:
            if not run.season_days[s]:
                continue
            f = run.flows[s]
            n = run.season_days[s]
            rows.append([s, '; '.join(f'{a}>{b} {round(c / n)}' for (a, b), c in f.most_common(6))])
        out += table(['season', 'largest flows (pennies a day)'], rows)
        drains = collections.Counter()
        gains = collections.Counter()
        for s, f in run.flows.items():
            for (a, b), c in f.items():
                if a == 'treasury' and b != 'treasury':
                    drains[b] += c
                if b == 'treasury' and a != 'treasury':
                    gains[a] += c
        say('')
        say(f'The treasury paid out (a day): {", ".join(f"{k} {round(v / run.days)}" for k, v in drains.most_common(5))}; '
            f'took in: {", ".join(f"{k} {round(v / run.days)}" for k, v in gains.most_common(5))}.')
    if econ:
        path = []
        for s in SEASONS:
            t = [r['treasury'] for r in econ if r.get('season') == s and 'treasury' in r]
            if t:
                path.append(f'{s} {t[0]} -> {t[-1]} (least {min(t)})')
        say('')
        say(f'The treasury: {econ[0].get("treasury")} -> {econ[-1].get("treasury")}p; ' + '; '.join(path) + '.')
    say('')
    # 5. Welfare.
    say('## 5. Welfare by class (a season\'s mean)')
    say('')
    if run:
        rows = []
        for (s, c), w in sorted(run.welfare.items(), key=lambda kv: (SEASONS.index(kv[0][0]) if kv[0][0] in SEASONS else 9, kv[0][1])):
            d = max(1, w['days'])
            res, homes = w['residents'] / d, max(1, w['households'] / d)
            rows.append([s, CLASS_NAMES.get(c, c), round(res), f'{w["hungry_hours"] / d / max(1, res):.2f}',
                         round(w['purse'] / d / max(1, res)), round(w['median'] / d), f'{w["comforts"] / d / homes:.2f}',
                         round(w['spent'] / d / homes, 1)])
        out += table(['season', 'class', 'residents', 'hungry h/head/day', 'purse/head', 'household median', 'comforts/home/day',
                      'spent/home/day'], rows)
    elif econ:
        say(f'Hungry (mean of hours): {statistics.fmean(r.get("hungry_mean", 0) for r in econ):.1f}; households short a day: '
            f'{statistics.fmean(r.get("households_short", 0) for r in econ):.1f}; Gini of earners {econ[-1].get("gini_earners")}.')
    say('')
    # 6. Buyers and sellers.
    if run:
        say('## 6. The largest buyers and sellers by good (the goods with the most pennies)')
        goods = collections.defaultdict(lambda: [0, 0, 0, collections.Counter(), collections.Counter()])
        for (g, b, s), (pieces, pennies, base) in run.trade.items():
            x = goods[g]
            x[0] += pieces
            x[1] += pennies
            x[2] += base
            x[3][b] += pennies
            x[4][s] += pennies
        say('')
        rows = []
        for g, x in sorted(goods.items(), key=lambda kv: -kv[1][1])[:top + 4]:
            buyer, bp = x[3].most_common(1)[0]
            seller, sp = x[4].most_common(1)[0]
            rows.append([g, round(x[0] / run.days, 1), round(x[1] / run.days), f'{x[1] / x[2]:.2f}' if x[2] else '-',
                         f'{buyer} {bp / x[1]:.0%}' if x[1] else buyer, f'{short_name(seller)} {sp / x[1]:.0%}' if x[1] else seller])
        out += table(['good', 'pieces/day', 'pennies/day', 'price factor', 'largest buyer', 'largest seller'], rows)
        say('')
    # 7. Farms.
    if run and run.farm:
        say('## 7. Farms')
        say('')
        # By area (the farms of one alike), and the farms that could not pay their crews by name.
        farms_meta = (run.meta or {}).get('farms', {})
        areas = collections.defaultdict(collections.Counter)
        for farm, f in run.farm.items():
            a = areas[farms_meta.get(farm, {}).get('area', farm)]
            a.update(f)
            a['farms'] += 1
        rows = []
        for area, f in sorted(areas.items()):
            d = max(1, f['days'])
            rows.append([area, f['farms'], round(f['days'] / f['farms']), round(f['takings'] / d), f'{f["paid"]}/{f["owed"]}',
                         f['support'], round(f['basket'] / d), round(f['crew_spent'] / d), f['crew_short'], f['float_last'],
                         f['crew_purse_last']])
        out += table(['area', 'farms', 'days on', 'takings/farm-day', 'wages paid/owed', 'support', 'basket/farm-day',
                      'crew spent/farm-day', 'crew short', 'floats', 'crew purses'], rows)
        unpaid = [farm for farm, f in sorted(run.farm.items()) if f['paid'] < f['owed']]
        if unpaid:
            say('')
            say('Farms short of their crews\' wages: ' + ', '.join(unpaid[:top]) + ('...' if len(unpaid) > top else '') + '.')
        patch, changes, ungrown = farm_patch(run)
        say('')
        if patch_path:
            Path(patch_path).write_text(dumps({'about': f'yields to meet demand +{run.slack}% (tools/econ_report.py)',
                                               'farms': patch}))
        say(f'Yields that would meet demand + {run.slack}% (pieces a day by season, before the yield scale): '
            f'{len(changes)} changes, the patch in `{patch_path}` (`--apply` it to country_farms.json). The largest:')
        say('')
        out += table(['farm', 'good', 'now', 'balanced'],
                     [[farm, g, ' / '.join(f'{v:g}' for v in cur), ' / '.join(f'{v:g}' for v in new)]
                      for _, farm, g, cur, new in changes[:top]] or [['-'] * 4])
        short_ungrown = [u for u in ungrown if u[2]]
        if short_ungrown:
            say('')
            say('Goods of a farm area no farm grows (no yield to raise: give a farm one in country_farms.json), with the '
                'draws they lacked: ' + ', '.join(f'{g} ({a}) {round(k)}' for g, a, k in sorted(short_ungrown, key=lambda u: -u[2])[:top])
                + ('...' if len(short_ungrown) > top else '') + '.')
        stuck = [(k, v) for k, v in run.farm_stock.items() if v[1] > v[0] * 1.5 and v[1] > 50]
        if stuck:
            say('')
            say('Farm stock growing over the run: ' + ', '.join(f'{f} {g} {a}->{b}' for (f, g), (a, b) in sorted(stuck)[:top]) + '.')
        say('')
    return '\n'.join(out) + '\n'


def dumps(doc):
    """As tools/country_industry.py writes country_farms.json: indent 1, each list of numbers on one line."""
    text = json.dumps(doc, indent=1, ensure_ascii=False)
    return re.sub(r'\[\s+([^\[\]{}]*?)\s+\]', lambda m: '[' + ', '.join(x.strip() for x in m.group(1).split(',')) + ']', text) + '\n'


def apply_patch(patch_path, farms_path):
    patch = json.loads(Path(patch_path).read_text())['farms']
    doc = json.loads(Path(farms_path).read_text())
    done = 0
    for area in doc.get('areas', []):
        for farm in area.get('farms', []):
            for good, new in patch.get(farm['id'], {}).items():
                if good in farm.get('yield', {}):
                    farm['yield'][good] = new
                    done += 1
    Path(farms_path).write_text(dumps(doc))
    return done


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('economy', nargs='?')
    ap.add_argument('farms_json', nargs='?')
    ap.add_argument('--full')
    ap.add_argument('--out')
    ap.add_argument('--patch')
    ap.add_argument('--slack', type=float, default=5)
    ap.add_argument('--top', type=int, default=8)
    ap.add_argument('--apply')
    a = ap.parse_args(argv)
    if a.apply:
        target = a.farms_json or a.economy
        print(f'{apply_patch(a.apply, target)} yields written to {target}')
        return 0
    econ = economy_lines(a.economy)
    full = a.full or (a.economy + '.full.jsonl.gz' if a.economy else None)
    run = None
    if full and Path(full).exists():
        run = Run(a.slack)
        for meta, names, d in full_days(full):
            run.add(meta, names, d)
    if not econ and run is None:
        print('nothing to read', file=sys.stderr)
        return 1
    patch = a.patch or ((a.out or a.economy or 'report') + '.patch.json')
    text = report(econ, run, a.top, patch)
    if a.out:
        Path(a.out).write_text(text)
        print(f'{a.out}: {len(text.splitlines())} lines')
    else:
        sys.stdout.write(text)
    return 0


if __name__ == '__main__':
    sys.exit(main())
