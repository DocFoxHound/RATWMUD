#!/usr/bin/env python3
"""A run's market in about two pages (Docs/Design/69-simulation-speed.md, 10): every imbalance, the prices pinned at the
band's edges, the tills short of their wages and why, the money's flows and the treasury's path, each class's welfare,
the largest buyers and sellers of each good, the farms and works (doc 72's: the woods', the peat's, paying rent to their
House; the food farms' land rent, doc 72's open question 2), the Country by its parts (the estates' and the farms' land
rents by House, the sea trade, its posts by rule), and the farms' yields
that would meet their demand, from world_check's
economy file (`--economy F`) and, at `--economy-level full`, its tables (F.full.jsonl.gz):

    python3 tools/econ_report.py ECONOMY.jsonl [--full F.full.jsonl.gz] [--out REPORT.md] [--patch PATCH.json]
                                 [--slack 5] [--top 8]
    python3 tools/econ_report.py --apply PATCH.json Data/Economy/country_farms.json [--damping 0.5] [--goods a,b]

It does all the reading: a person or an agent reads only the report. With the economy file alone (the normal level) it
reports what those lines hold (the treasury, the hungry, the tills short, the farms' wants not met). The farms' patch
(PATCH, default the report's name with ".patch.json") holds, per farm and good, the yield by season (pieces a day at the
season's middle, before the farm's yield scale, as country_farms.json has them) that would have met what was taken of it
and its stock's needs, with SLACK percent more; a season the run did not see keeps its yield. What was taken is what the
farms of the good sold, what the Country's table ate and what it lacked, all the good's farms together, shared out by
their yields (a draw takes from the farms by their stock, so one farm's own sales say little). Its stock's needs are the
shelves the shops' draws left under, a stock and not a day's want: refilled over RefillDays, and only for a good the farms
never spoiled that season (a perishable's shelves kept full would only spoil). (The draws' deficits counted as a day's
want, as before 2026-11-03, sized the yields on a stock counted again each day: the next run's patch swung back.) A good
supply-bound in a season (its farms all out at the day's end on BOUND_SHARE of its days while the draws fell short: what
sold was the supply) counts its shelves' needs even where some spoiled, and is listed in the patch's `whole` (its seasons in `wholeSeasons`).
`--apply` writes those yields into country_farms.json (nothing else changes), DAMPING of the way from each yield now to
the patch's (0.5 by default: half way, so a run's patch can't overshoot and swing back; 1 the whole way), of every
good or of GOODS alone; a supply-bound good the whole way in the seasons it was bound (`wholeSeasons`).
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
REFILL_DAYS = 7            # The days over which a farm good's shops' shelves left under are filled again (the patch).
BOUND_SHARE = 0.4          # A good supply-bound in a season: its farms out at the day's end on this share of its days (the patch).
DAMPING = 0.5              # The share of the way from a yield now to the patch's that `--apply` goes.
CLASS_NAMES = {'lower': 'lower (the Sump, the Rents)', 'middle': 'middle', 'upper': 'upper (the Houses)'}


# The economy lines' keys before doc 73 (Docs/Design/73-the-wharf.md, 2.1): an older run's read under the Wharf's names.
OLD_KEYS = {'country': 'wharf', 'country_spent': 'basket_spent', 'country_unspent': 'basket_unspent',
            'country_sales': 'wharf_sales', 'country_rents': 'estate_rents', 'country_rents_by_house': 'estate_rents_by_house',
            'country_land_rents': 'estate_land_rents', 'country_land_rents_by_house': 'estate_land_rents_by_house',
            'country_posts_owed': 'estate_posts_owed', 'country_posts_paid': 'estate_posts_paid',
            'country_posts_by_rule': 'estate_posts_by_rule', 'country_posts_held': 'estate_posts_held',
            'patrons_country': 'patrons_estates', 'country_basket_shift': 'wharf_basket_shift'}


def renamed(row):
    """An economy line with an older run's Country keys under the Wharf's (the new name wins where both are)."""
    if not any(k in row for k in OLD_KEYS):
        return row
    out = {}
    for k, v in row.items():
        out.setdefault(OLD_KEYS.get(k, k), v)
    for k, v in row.items():
        if k not in OLD_KEYS:
            out[k] = v
    return out


def economy_lines(path):
    rows = []
    if path and Path(path).exists():
        with open(path) as f:
            for line in f:
                if line.strip():
                    rows.append(renamed(json.loads(line)))
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
        self.farms_out = collections.Counter()      # (good, season) -> the days all its farms held none at the day's end
        self.farms_days = collections.Counter()     # (good, season) -> the days its farms were seen
        self.farm = collections.defaultdict(collections.Counter)           # farm -> sums
        self.draw_short = collections.defaultdict(list)   # (good, season, till) -> the pieces its draws lacked, a day
        self.sold_rate = collections.defaultdict(lambda: [0, 0])   # (till, good) -> pieces sold, days it had some or sold
        # Round 3 (doc 73): each maker's crew-hours (put in, used, lost past its bank), and the households short of food
        # (rows, those with no earner, their earners' payers, their class).
        self.work = collections.defaultdict(collections.Counter)
        self.hh_short = collections.Counter()
        self.hh_payers = collections.Counter()
        self.hh_homes = collections.Counter()

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

    def stock_needs(self):
        """By (good, season), the shops' shelves the draws left under, a day of the season: their deficits summed over the
        days and the tills, over the season's days (each till's mean deficit, all the tills together)."""
        total = collections.Counter()
        for (g, s, till), days in self.draw_short.items():
            total[(g, s)] += sum(days)
        return {k: v / max(1, self.season_days[k[1]]) for k, v in total.items()}

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
        held = collections.Counter()                # (Each good's farms' stock together at the day's end.)
        for row in d['farm_good']:
            farm, g, grown, sold, st, spoiled, eaten, short = row[:8]
            used = row[8] if len(row) > 8 else 0    # (Made into another good at the farm: the dairies' spare milk, doc 72.)
            key = (n(farm), n(g))
            stock[n(g)] += st
            fg = self.farm_good[(n(farm), n(g), season)]
            fg.update(grown=grown, sold=sold, spoiled=spoiled, eaten=eaten, short=short, used=used, days=1)
            first = self.farm_stock.setdefault(key, [st, st])
            first[1] = st
            held[n(g)] += st
        for good, pieces in held.items():
            self.farms_days[(good, season)] += 1
            self.farms_out[(good, season)] += pieces == 0
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
        for till, hours, used, lost in d.get('work', []):
            self.work[n(till)].update(hours=hours / 10, used=used / 10, lost=lost / 10, days=1)
        for member, cls, members, purse, earners, *rest in d.get('hh_short', []):
            payers, then = rest[:3], rest[3:]       # (Since doc 73's round 10: the meals lacked, the purse then, the cheapest food.)
            self.hh_short.update(rows=1, alone=members == 1, none=earners == 0, purse=purse, **{'class:' + n(cls): 1})
            if len(then) == 3:
                self.hh_short.update(then=1, lack=then[0], purse_then=then[1], afford=then[2] > 0 and then[1] >= then[2],
                                     no_food=then[2] == 0)
            self.hh_homes[n(member)] += 1
            for p in payers:
                if p >= 0:
                    self.hh_payers[n(p)] += 1
        for row in d['class']:
            w = self.welfare[(season, n(row[0]))]
            w.update(days=1, residents=row[1], households=row[2], hungry_hours=row[3], purse=row[5], median=row[6],
                     comforts=row[7], spent=row[8])
        for row in d['farm']:
            f = self.farm[n(row[0])]
            f.update(days=1, takings=row[3], owed=row[4], paid=row[5], support=row[6], basket=row[7], crew_spent=row[9],
                     crew_short=row[10], rent=row[12] if len(row) > 12 else 0)
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
    """Per farm and good, the yields by season that meet its share of what was taken and its stock's needs, SLACK percent
    over (the module's doc)."""
    farms = (run.meta or {}).get('farms', {})
    by_good = collections.defaultdict(float)
    for farm, f in farms.items():
        for good, y in f['yield'].items():
            by_good[good] += sum(y) / 4 * f['yield_scale']
    patch, changes = {}, []
    unmet = run.unmet_draws()
    needs = run.stock_needs()
    # An area's good no farm of it grows: the Country no longer sends it and no yield can be raised, so it is named apart.
    # (A good a farm makes of a spare yield, the dairies' cheese and butter, is no yield either, but is had: not named.)
    made = {good for f in farms.values() for good in (f.get('makes') or {})}
    ungrown = []
    for area, goods in sorted(((run.meta or {}).get('farm_areas') or {}).items()):
        for good in goods:
            if not by_good.get(good) and good not in made:
                ungrown.append((good, area, sum(unmet[(good, s)] for s in SEASONS)))
    # Each good's season, all its farms together: what was taken a day (on the days its farms were on), whether any
    # spoiled, and its yield now (each farm's share of the patch's).
    taken, spoiled, grown, days = collections.Counter(), set(), collections.Counter(), collections.Counter()
    for (farm, good, season), fg in run.farm_good.items():
        if farm not in farms or not fg['days']:
            continue
        taken[(good, season)] += fg['sold'] + fg['eaten'] + fg['short']
        days[(good, season)] = max(days[(good, season)], fg['days'])
        if fg['spoiled']:
            spoiled.add((good, season))
    for farm, f in farms.items():
        for good, y in f['yield'].items():
            for i, season in enumerate(SEASONS):
                grown[(good, season)] += y[i] * f['yield_scale']
    # Supply-bound (doc 72, Phase 3): a good whose farms all held none at the day's end on BOUND_SHARE of the season's days
    # while the shops' draws of it fell short. What was taken of it is then what was grown less the Country's table, not
    # what was wanted, so a patch sized on it and damped half way rose a few percent a run and stayed short (oats, salt and
    # milk three runs running). Its shelves' needs count even where some spoiled (milk spoils the morning after a Restday,
    # when the inns draw little, and is short the rest of the week: a matter of days, not a glut), and `--apply` takes its
    # whole step (the patch's `whole`).
    bound = {key for key, n in run.farms_days.items() if n and run.farms_out[key] / n >= BOUND_SHARE and needs.get(key, 0) > 0}
    for farm, f in sorted(farms.items()):
        for good, current in sorted(f['yield'].items()):
            new = list(current)
            for i, season in enumerate(SEASONS):
                key = (good, season)
                fg = run.farm_good.get((farm, good, season))
                if not fg or not fg['days'] or f['yield_scale'] <= 0:
                    continue
                demand = taken[key] / days[key] + (0 if key in spoiled and key not in bound else needs.get(key, 0) / REFILL_DAYS)
                # (Its share of the season's yield now; none grown this season, its share of the year's.)
                share = (current[i] * f['yield_scale'] / grown[key] if grown[key] else
                         sum(current) / 4 * f['yield_scale'] / by_good[good] if by_good[good] else 0)
                new[i] = round(demand * share * (1 + run.slack / 100) / f['yield_scale'], 2)
            if new != list(current):
                patch.setdefault(farm, {})[good] = new
                changes.append((max(abs(a - b) / max(b, 1) for a, b in zip(new, current)), farm, good, current, new))
    seasons = {g: [s for s in SEASONS if (g, s) in bound] for g in sorted({g for g, _ in bound})}
    return patch, sorted(changes, reverse=True), ungrown, seasons


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
        if run.work:
            # Round 3 (doc 73): the short trades' crew-hours, and the makers that used every hour they had (bound by
            # their crew, not their sales).
            hours = collections.defaultdict(collections.Counter)
            for till, w in run.work.items():
                hours[run.till_trade.get(till, '?')].update(w)
            say('')
            say('Their crew-hours (a till\'s day: put in, used by its crafts, lost past its bank; the work table):')
            say('')
            out += table(['trade', 'hours/till-day', 'used', 'lost'],
                         [[tr, round(hours[tr]['hours'] / max(1, hours[tr]['days']), 1),
                           f"{100 * hours[tr]['used'] / max(1e-9, hours[tr]['hours']):.0f}%",
                           f"{100 * hours[tr]['lost'] / max(1e-9, hours[tr]['hours']):.0f}%"] for tr, _ in rows if tr in hours]
                         or [['-'] * 4])
            bound = sorted((t for t, w in run.work.items() if w['hours'] > 0 and w['used'] >= 0.95 * w['hours']),
                           key=lambda t: -run.work[t]['hours'])
            say('')
            say('Bound by their crew (used 95% or more of their hours): ' + (', '.join(
                f"{short_name(t)} ({run.till_trade.get(t, '?')}, {run.work[t]['used'] / max(1, run.work[t]['days']):.1f} of "
                f"{run.work[t]['hours'] / max(1, run.work[t]['days']):.1f} h)" for t in bound[:top]) or 'none') + '.')
    elif econ:
        say('')
        say(f'Tills short a night: mean {statistics.fmean(r.get("tills_short", 0) for r in econ):.1f}; wage support '
            f'{sum(r.get("support_by_trade", {}).get("farm", 0) for r in econ)}p to the farms.')
    # By year (doc 73, round 2): what the short tills wanted against what the treasury paid (it pays only from what it
    # holds over its cap, so the paid can climb while the wanted stays level), and the wanted by trade.
    if econ and any('support_wanted' in r for r in econ):
        years = collections.defaultdict(list)
        for r in econ:
            years[r.get('day', 0) // 365].append(r)
        say('')
        say('Wage support by year (a day): what the short tills could not pay (wanted), what the treasury paid, what went unpaid; '
            'and the wanted by trade (the largest, before the support), where logged:')
        say('')
        trades = collections.Counter()
        for r in econ:
            trades.update(r.get('short_by_trade', {}))
        top_trades = [t for t, _ in trades.most_common(6)]
        rows = []
        for y, rs in sorted(years.items()):
            m = lambda k: sum(r.get(k, 0) for r in rs) / len(rs)
            by = collections.Counter()
            for r in rs:
                by.update(r.get('short_by_trade', {}))
            rows.append([str(y + 1), f'{m("support_wanted"):.0f}', f'{m("wage_support"):.0f}', f'{m("support_unpaid"):.0f}']
                        + [f'{by[t] / len(rs):.1f}' for t in top_trades])
        out += table(['year', 'wanted', 'paid', 'unpaid'] + top_trades, rows)
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
        if run.hh_short['rows']:
            h = run.hh_short
            say('')
            say(f"Households short of food at the shops' last hour (the hh_short table): {h['rows'] / max(1, run.days):.2f} a day, "
                f"{len(run.hh_homes)} households; {100 * h['none'] / h['rows']:.0f}% with no earner, {100 * h['alone'] / h['rows']:.0f}% "
                f"alone, a mean purse of {h['purse'] / h['rows']:.1f}p; by class "
                + ', '.join(f"{k[6:]} {v}" for k, v in sorted(h.items()) if k.startswith('class:'))
                + (('; their earners\' payers ' + ', '.join(f'{short_name(p)} {v}' for p, v in run.hh_payers.most_common(top)))
                   if run.hh_payers else '') + '.')
            if h['then']:
                say(f"At the shops' last hour they lacked {h['lack'] / h['then']:.1f} meals and held {h['purse_then'] / h['then']:.1f}p; "
                    f"{100 * h['afford'] / h['then']:.0f}% could have paid for the cheapest food then on offer, "
                    f"{100 * h['no_food'] / h['then']:.0f}% found no food in any open shop.")
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
    # The Wharf and the Houses' estates (doc 72, Phase 0: one purse, "the Houses' estates and the sea trade"; doc 73).
    if econ and any('wharf_sales' in r for r in econ):
        say(country_lines(econ))
    if econ and any(r.get('wharf_day') for r in econ):
        say(wharf_lines(econ))
    # The makers' goods sold on, the country workshops and the luxuries (doc 72, Phase 5).
    if econ and any('city_resale' in r for r in econ):
        say(phase5_lines(econ))
    # 7. Farms.
    if run and run.farm:
        say('## 7. Farms')
        say('')
        # By area (the farms of one alike), and the farms that could not pay their crews by name.
        farms_meta = (run.meta or {}).get('farms', {})
        works_areas = set((run.meta or {}).get('works_areas') or [])
        areas = collections.defaultdict(collections.Counter)
        for farm, f in run.farm.items():
            a = areas[farms_meta.get(farm, {}).get('area', farm)]
            a.update(f)
            a['farms'] += 1
        rows = []
        for area, f in sorted(areas.items()):
            d = max(1, f['days'])
            rows.append([area + (' (works)' if area in works_areas else ''), f['farms'], round(f['days'] / f['farms']),
                         round(f['takings'] / d), f'{f["paid"]}/{f["owed"]}', f['support'], round(f['basket'] / d),
                         round(f['rent'] / d), round(f['crew_spent'] / d), f['crew_short'], f['float_last'], f['crew_purse_last']])
        out += table(['area', 'farms', 'days on', 'takings/farm-day', 'wages paid/owed', 'support', 'basket/farm-day',
                      'rent/farm-day', 'crew spent/farm-day', 'crew short', 'floats', 'crew purses'], rows)
        # The works one by one (doc 72: each pays its crew, the rest to its House's estate).
        works = sorted(farm for farm in run.farm if farms_meta.get(farm, {}).get('area') in works_areas)
        if works:
            say('')
            say('The works (a day; their rent to their House\'s estate, the Country\'s purse at first):')
            say('')
            out += table(['works', 'House', 'days on', 'takings', 'wages paid/owed', 'support', 'rent', 'float'],
                         [[w, farms_meta[w].get('house') or '-', run.farm[w]['days'],
                           round(run.farm[w]['takings'] / max(1, run.farm[w]['days']), 1),
                           f'{run.farm[w]["paid"]}/{run.farm[w]["owed"]}', run.farm[w]['support'],
                           round(run.farm[w]['rent'] / max(1, run.farm[w]['days']), 1), run.farm[w]['float_last']]
                          for w in works])
        # The food farms' land rent (doc 72, open question 2): what each paid its House, and what was left over its wages
        # and its rent (its basket's share; thin where a farm's takings barely pass its crew's wages). A farm whose rule is
        # none (doc 72, Phase 5: the thinnest farms' landRentThin 0) is listed too, once any farm paid one.
        landed = sorted((farm for farm in run.farm if farms_meta.get(farm, {}).get('area') not in works_areas and farm in farms_meta
                         and (run.farm[farm]['rent'] > 0 or farms_meta[farm].get('house'))), key=lambda w: (farms_meta[w].get('area', ''), w))
        if landed and any(run.farm[w]['rent'] > 0 for w in landed):
            say('')
            say('The food farms\' land rent (a day; to the House whose land each is on, the Country\'s purse at first; left: '
                'takings less wages less rent):')
            say('')
            per = lambda w, k: run.farm[w][k] / max(1, run.farm[w]['days'])
            out += table(['farm', 'House', 'days on', 'takings', 'wages owed', 'rent', 'rent %', 'basket', 'left'],
                         [[w, farms_meta[w].get('house') or '-', run.farm[w]['days'], round(per(w, 'takings'), 1),
                           round(per(w, 'owed'), 1), round(per(w, 'rent'), 1),
                           round(100 * run.farm[w]['rent'] / max(1, run.farm[w]['takings']), 1), round(per(w, 'basket'), 1),
                           round(per(w, 'takings') - per(w, 'owed') - per(w, 'rent'), 1)] for w in landed])
        unpaid = [farm for farm, f in sorted(run.farm.items()) if f['paid'] < f['owed']]
        if unpaid:
            say('')
            say('Farms short of their crews\' wages: ' + ', '.join(unpaid[:top]) + ('...' if len(unpaid) > top else '') + '.')
        patch, changes, ungrown, whole = farm_patch(run)
        say('')
        if patch_path:
            Path(patch_path).write_text(dumps({'about': f'yields to meet demand +{run.slack}% (tools/econ_report.py); `whole`: the '
                                                        'supply-bound goods; `wholeSeasons`: the seasons each was bound in, '
                                                        'whose step --apply takes whole',
                                               'farms': patch, 'whole': list(whole), 'wholeSeasons': whole}))
        if whole:
            say('Supply-bound (its farms out at the day\'s end on most days of a season while the shops\' draws fell short: what '
                'sold was the supply, not the demand; `--apply` takes their whole step in those seasons): ' +
                ', '.join(f'{g} ({", ".join(ss)})' for g, ss in whole.items()) + '.')
            say('')
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
        makes = {}
        for f in (run.meta or {}).get('farms', {}).values():
            makes.update(f.get('makes') or {})
        if makes:
            sums = collections.defaultdict(collections.Counter)
            for (farm, good, season), fg in run.farm_good.items():
                sums[good].update(made=fg['grown'], sold=fg['sold'], used=fg.get('used', 0), spoiled=fg['spoiled'])
            last = collections.Counter()
            for (farm, good), (a, b) in run.farm_stock.items():
                last[good] += b
            days = max(1, run.days)
            say('')
            say('Made at the farms of what would have spoiled and of their own milk for it (a day over the run; doc 72, the dairies): ' + '; '.join(
                f'{g} {sums[g]["made"] / days:.1f} made of {src} (sold {sums[g]["sold"] / days:.1f}, {last[g]} held at the end)'
                for g, src in sorted(makes.items())) + '; ' + ', '.join(
                f'{src} {sums[src]["used"] / days:.1f} used, {sums[src]["spoiled"] / days:.1f} spoiled' for src in sorted(set(makes.values()))) + '.')
        stuck = [(k, v) for k, v in run.farm_stock.items() if v[1] > v[0] * 1.5 and v[1] > 50]
        if stuck:
            say('')
            say('Farm stock growing over the run: ' + ', '.join(f'{f} {g} {a}->{b}' for (f, g), (a, b) in sorted(stuck)[:top]) + '.')
        say('')
    return '\n'.join(out) + '\n'


def wharf_lines(econ):
    """The Wharf beyond the realm (Docs/Design/73-the-wharf.md, 7), a day (the economy lines' means): by good, what it
    landed (pieces, and what the realm paid for them after the toll) and what it took from the farms (pieces, and what the
    beyond paid them); its export basket; the beyond's pennies in and out, net, by season too; its fees; the credit at the
    start, the end and its least, and its moves; the money's check (in - out against the realm's money's change)."""
    days = [(r, r['wharf_day']) for r in econ if r.get('wharf_day')]
    n = max(1, len(days))
    landed, landedP, taken, takenP, shops = (collections.Counter() for _ in range(5))
    for _, w in days:
        for g, (pieces, pennies) in (w.get('imported') or {}).items():
            landed[g] += pieces
            landedP[g] += pennies
        for g, (pieces, pennies) in (w.get('taken') or {}).items():
            taken[g] += pieces
            takenP[g] += pennies
        for g, (pieces, pennies) in (w.get('taken_shops') or {}).items():
            shops[g] += pieces
    mean = lambda key: sum(w.get(key, 0) for _, w in days) / n
    credits = [w.get('credit', 0) for _, w in days]
    out = ['## The Wharf: beyond the realm', '',
           f'The valve {"on" if any(w.get("valve") for _, w in days) else "off"} ({n} days). A day: the beyond paid in '
           f'{mean("in"):.0f}p and was paid {mean("out"):.0f}p (net {mean("in") - mean("out"):+.0f}p into the realm); '
           f'the Wharf\'s fees {mean("fee"):.0f}p; its export basket {mean("basket"):.0f}p; the credit moved {mean("moved"):+.0f}p.',
           f'The credit: {credits[0]} -> {credits[-1]}p (least {min(credits)}, most {max(credits)}).'
           + (f' What the takes wanted that the room could not fit: {mean("unfit"):.0f}p a day.' if any('unfit' in w for _, w in days) else ''),
           '']
    # (Round 9, doc 73: the unfit by good, where the run logged it.)
    unfit_by = collections.Counter()
    for _, w in days:
        unfit_by.update(w.get('unfit_by') or {})
    if unfit_by:
        out[-1:-1] = ['Of it, by good (a day): ' + ', '.join(f'{g} {v / n:.0f}p' for g, v in unfit_by.most_common()) + '.']
    # What rotted of the makers' perishable by-products (the rule shopRot, doc 73, round 2), a day by good.
    rotted = collections.Counter()
    for r in econ:
        rotted.update(r.get('rotted') or {})
    if rotted:
        out[-1:-1] = [f'Rotted at the makers (by-products no buyer took, a day): '
                      + ', '.join(f'{g} {v / max(1, len(econ)):.1f}' for g, v in rotted.most_common()) + '.']
    by_season = collections.defaultdict(lambda: [0, 0, 0])
    for r, w in days:
        s = by_season[r.get('season', '?')]
        s[0] += w.get('in', 0)
        s[1] += w.get('out', 0)
        s[2] += 1
    if len(by_season) > 1:
        out += table(['season', 'in a day', 'out a day', 'net a day'],
                     [[k, f'{v[0] / v[2]:.0f}', f'{v[1] / v[2]:.0f}', f'{(v[0] - v[1]) / v[2]:+.0f}'] for k, v in by_season.items()]) + ['']
    goods = sorted(set(landed) | set(taken), key=lambda g: (-(landedP[g] + takenP[g]), g))   # (By name on a tie: a set's order isn't fixed.)
    if goods:
        out += table(['good', 'landed a day', 'paid for it a day', 'taken a day (of it from shops)', 'the beyond paid a day'],
                     [[g, f'{landed[g] / n:.1f}', f'{landedP[g] / n:.1f}',
                       f'{taken[g] / n:.1f}' + (f' ({shops[g] / n:.1f})' if shops[g] else ''), f'{takenP[g] / n:.1f}']
                      for g in goods]) + ['']
    first, last = days[0][0], days[-1][0]
    if 'money' in first and 'money' in last:
        net = days[-1][1].get('book_in', 0) - days[-1][1].get('book_out', 0) - (days[0][1].get('book_in', 0) - days[0][1].get('book_out', 0))
        out += [f'The realm\'s money {first["money"]} -> {last["money"]}p; the Wharf\'s net over the same days {net:+d}p.', '']
    return '\n'.join(out)


def country_lines(econ):
    """The Wharf's purse and the Houses' estates by their parts, a day (the economy lines' means; doc 73: the Country's
    before): in, the Wharf's own goods' sales to the shops after the toll (the sea trade and what it still lands), the
    works' rents by House (the estates') and the food farms' land rents by House (doc 72, open question 2); out, the
    estates' posts by wage rule (paid of owed, and the posts held) and their basket; and the Wharf's purse."""
    n = max(1, len(econ))
    mean = lambda key: sum(r.get(key, 0) for r in econ) / n
    def by(key):
        total = collections.Counter()
        for r in econ:
            total.update(r.get(key) or {})
        return total
    rents, rules, held = by('estate_rents_by_house'), by('estate_posts_by_rule'), by('estate_posts_held')
    land = by('estate_land_rents_by_house')
    named = lambda h: h if h.startswith('the ') else f'House {h}'   # ("the Crown")
    out = ['## The Wharf and the Houses\' estates', '',
           f'In (a day): the Wharf\'s goods to the shops {mean("wharf_sales"):.0f}p (after the toll); the estates\' rents '
           f'{mean("estate_rents"):.0f}p' + (' (' + ', '.join(f'{named(h)} {v / n:.0f}p' for h, v in rents.most_common()) + ')' if rents else '')
           + (f'; the farms\' land rents {mean("estate_land_rents"):.0f}p ('
              + ', '.join(f'{named(h)} {v / n:.0f}p' for h, v in land.most_common()) + ')' if land else '')
           + '.',
           f'Out (a day): the estates\' posts {mean("estate_posts_paid"):.0f}p of {mean("estate_posts_owed"):.0f}p owed'
           + (' (' + ', '.join(f'{k} {v / n:.0f}p, {held[k] / n:.0f} posts' for k, v in rules.most_common()) + ')' if rules else '')
           + f'; their basket {mean("basket_spent"):.0f}p.',
           # (Split into the Houses' estates, doc 72, Phase 6: the Wharf's purse is House Grayrock's; the others' are in
           # the estates' table below.)
           ('The Wharf\'s purse (House Grayrock\'s; the other estates\' below)' if any(r.get('estates') for r in econ) else 'The Wharf\'s purse')
           + f': {econ[0].get("wharf")} -> {econ[-1].get("wharf")}p (least {min(r.get("wharf", 0) for r in econ)}).', '']
    out += estates_lines(econ)
    return '\n'.join(out)


def estates_lines(econ):
    """The Houses' estates (doc 72, Phase 6), a day over the days they were open: what came in from each House's lands
    (the works', the workshops' and the farms' rents; the Country's own sales are the wharf's, Grayrock's), the Crown's
    grants in and out, the Crown's levy on the others' spare (in, the Crown's; out, each's: after estates6), its posts paid of owed (and how many), what it bought, its purse first, last and least, and the
    nights a post went short."""
    days = [r['estates'] for r in econ if r.get('estates')]
    if not days:
        return []
    n = len(days)
    ids = list(dict.fromkeys(k for d in days for k in d))
    rows = []
    for e in ids:
        open_ = [d[e] for d in days if e in d]
        mean = lambda key: sum(x.get(key, 0) for x in open_) / max(1, len(open_))
        named = (open_[0].get('house') or e)
        purses = [x.get('purse', 0) for x in open_]
        rows.append([named if named.startswith('the ') else f'House {named}', f'{mean("posts"):.0f}', f'{mean("rents"):.0f}',
                     f'{mean("grants_in"):.0f}', f'{mean("grants_out"):.0f}', f'{mean("levy_in"):.0f}/{mean("levy_out"):.0f}',
                     f'{mean("posts_paid"):.0f}/{mean("posts_owed"):.0f}',
                     sum(1 for x in open_ if x.get('posts_paid', 0) < x.get('posts_owed', 0)), f'{mean("spent"):.0f}',
                     f'{purses[0]} -> {purses[-1]} (least {min(purses)})'])
    out = ([f'The Houses\' estates (a day over the {n} days they were split; the Wharf\'s own sales are House Grayrock\'s, '
            'which runs it):', ''] +
           table(['estate', 'posts', 'rents in', 'grants in', 'grants out', 'levy in/out', 'posts paid/owed', 'nights short', 'bought', 'purse'], rows)
           + [''])
    # By year and by source (after wharf7b: an estate's income traced): its rents in by where they came from (the food
    # farms' land rents, of which their surplus over their design's basket, the works' rents, the workshops' and the
    # Houses' city businesses' takings, the Wharf's fees), its posts and nights short, and its purse at the year's end.
    if any('land' in d[e] for d in days for e in d):
        years = collections.defaultdict(list)
        for d in days_with_day(econ):
            years[d[0] // 365].append(d[1])
        rows = []
        for e in ids:
            for y, ds in sorted(years.items()):
                open_ = [d[e] for d in ds if e in d]
                if not open_:
                    continue
                mean = lambda key: sum(x.get(key, 0) for x in open_) / len(open_)
                named = (open_[0].get('house') or e)
                keeps = [x['keep'] for x in open_ if x.get('keep', -1) >= 0]
                over = (f'{sum(x["purse"] - x["keep"] for x in open_ if x.get("keep", -1) >= 0) / len(keeps):+.0f}'
                        if keeps else '-')
                rows.append([(named if named.startswith('the ') else f'House {named}'), str(y + 1), f'{mean("rents"):.0f}',
                             f'{mean("land"):.0f} ({mean("surplus"):.0f})', f'{mean("works"):.0f}', f'{mean("shops"):.0f}',
                             f'{mean("fees"):.0f}', f'{mean("grants_in") - mean("grants_out"):+.0f}',
                             f'{mean("levy_in") - mean("levy_out"):+.0f}', f'{mean("posts_paid"):.0f}/{mean("posts_owed"):.0f}',
                             sum(1 for x in open_ if x.get('posts_paid', 0) < x.get('posts_owed', 0)), str(open_[-1].get('purse', 0)),
                             f'{sum(keeps) / len(keeps):.0f}' if keeps else '-', over])
        out += (['The estates by year (a day; land: the food farms\' land rents, in brackets their surplus over their design\'s '
                 'basket; works: the works\' rents; shops: the workshops\' and the Houses\' city businesses\'; fees: the Wharf\'s; '
                 'grants and levy net; the purse at the year\'s end; keep: its mean keep a night, estateKeep, its reserve\'s share, '
                 'its posts\' nights and, the Crown\'s, what the others lack of theirs; over: its purse over its keep a night, '
                 'what it may spend):', ''] +
                table(['estate', 'year', 'rents in', 'land (surplus)', 'works', 'shops', 'fees', 'grants', 'levy', 'posts paid/owed',
                       'nights short', 'purse', 'keep', 'over'], rows) + [''])
    return out


def days_with_day(econ):
    """(day, estates) of the economy lines with the estates open."""
    return [(r.get('day', 0), r['estates']) for r in econ if r.get('estates')]


def dumps(doc):
    """As tools/country_industry.py writes country_farms.json: indent 1, each list of numbers on one line."""
    text = json.dumps(doc, indent=1, ensure_ascii=False)
    return re.sub(r'\[\s+([^\[\]{}]*?)\s+\]', lambda m: '[' + ', '.join(x.strip() for x in m.group(1).split(',')) + ']', text) + '\n'


def apply_patch(patch_path, farms_path, damping=DAMPING, goods=None):
    """The patch's yields written into country_farms.json, DAMPING of the way from each yield now (of GOODS alone, if
    given); a supply-bound good the whole way in the seasons it was bound (the patch's `wholeSeasons`; an older patch's
    `whole` alone: every season)."""
    loaded = json.loads(Path(patch_path).read_text())
    patch = loaded['farms']
    whole = loaded.get('wholeSeasons') or {g: SEASONS for g in loaded.get('whole', [])}
    doc = json.loads(Path(farms_path).read_text())
    done = 0
    for area in doc.get('areas', []):
        for farm in area.get('farms', []):
            for good, new in patch.get(farm['id'], {}).items():
                if good in farm.get('yield', {}) and (goods is None or good in goods):
                    now = farm['yield'][good]
                    steps = [1 if season in whole.get(good, ()) else damping for season in SEASONS]
                    moved = [round(a + step * (b - a), 2) for a, b, step in zip(now, new, steps)]
                    farm['yield'][good] = [int(v) if v == int(v) else v for v in moved]   # (10, not 10.0, as written)
                    done += 1
    Path(farms_path).write_text(dumps(doc))
    return done


def phase5_lines(econ):
    """Doc 72, Phase 5, a day (the economy lines' means): what the shops bought from the city's makers and the country
    workshops to sell on (the resale path, by good); each workshop's wages paid of owed and what went to its House; the
    luxuries (cheese) the households bought, what they paid and how many households bought."""
    n = max(1, len(econ))
    mean = lambda key: sum(r.get(key, 0) for r in econ) / n
    goods, shops = collections.Counter(), {}
    for r in econ:
        goods.update(r.get('city_resale_goods') or {})
        for w, x in (r.get('workshops') or {}).items():
            s = shops.setdefault(w, collections.Counter())
            s.update(x)
            s['days'] += 1
    out = ['## Phase 5: the makers\' goods sold on, the workshops, the luxuries', '',
           f'Bought to sell on from the makers (a day): {mean("city_resale_pieces"):.1f} pieces, {mean("city_resale"):.1f}p'
           + (' (' + ', '.join(f'{g} {v / n:.1f}' for g, v in goods.most_common()) + ')' if goods else '') + '.']
    if shops:
        # (With them since the producers, doc 72's Progress: the Houses' city businesses, the ironworks and the foundry, by
        # their buildings, rm_*.)
        out.append('The workshops and the Houses\' city businesses (a day open): ' + '; '.join(
            f'{w} wages {s["paid"] / s["days"]:.1f}p of {s["owed"] / s["days"]:.1f}p, to its House {s["to_house"] / s["days"]:.1f}p'
            for w, s in sorted(shops.items())) + '.')
    else:
        out.append('No workshop in this world (its buildings not built): their goods the store room\'s.')
    line = (f'The luxuries (a day): {mean("luxuries_bought"):.1f} pieces for {mean("luxuries_spent"):.1f}p, '
            f'{mean("luxury_households"):.1f} households')
    # (After country5, doc 72: by the households' class, and the Houses' own tables, the Country's purse; a piece's mean
    # price.)
    classes = collections.defaultdict(collections.Counter)
    for r in econ:
        for c, x in (r.get('luxury_classes') or {}).items():
            classes[c].update(x)
    if classes:
        line += ' (' + ', '.join(f'{c} {x["pieces"] / n:.1f} at {x["spent"] / max(1, x["pieces"]):.1f}p'
                                 for c, x in sorted(classes.items(), key=lambda kv: -kv[1]['pieces'])) + ')'
    if any('luxuries_houses' in r for r in econ):
        houses, spent = mean('luxuries_houses'), mean('luxuries_houses_spent')
        line += (f'; the Houses\' tables {houses:.1f} pieces for {spent:.1f}p'
                 + (f' ({spent / houses:.1f}p a piece)' if houses > 0 else ''))
    out += [line + '.', '']
    return '\n'.join(out)


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
    ap.add_argument('--damping', type=float, default=DAMPING)
    ap.add_argument('--goods', help='with --apply: only these goods (comma separated)')
    a = ap.parse_args(argv)
    if a.apply:
        target = a.farms_json or a.economy
        if not 0 < a.damping <= 1:
            print('--damping is from 0 (not included) to 1', file=sys.stderr)
            return 1
        goods = set(a.goods.split(',')) if a.goods else None
        print(f'{apply_patch(a.apply, target, a.damping, goods)} yields written to {target} ({a.damping:g} of the way)')
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
