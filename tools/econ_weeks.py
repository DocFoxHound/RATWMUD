#!/usr/bin/env python3
"""Week-by-week (or N-day) means of world_check's --economy file, for long runs and for comparing what-if runs
(Docs/Design/68-the-country-and-the-city-loop.md, "The first simulations").

    python3 tools/econ_weeks.py RUN.jsonl [RUN2.jsonl ...] [--days 7] [--from D] [--fields a,b,...] [--summary]

--summary prints one line per run over --from to the end: the gates of doc 68, 10 (hungry mean and worst, households
fed, wages paid, tills short, the treasury's and the Country's change, Gini) and the day's flows.
"""
import argparse
import json

DEFAULT = ('hungry_mean,hungry_most,households_fed,households_short,residents_spent,food_bought,country_spent,'
           'country_unspent,drawn,wages_owed,wages_paid,tills_short,profit,alms,treasury,country,tills,purses,gini_earners')


def load(path):
    return [json.loads(l) for l in open(path) if l.strip()]


def summary(name, days, start):
    span = [d for d in days if d['day'] >= start]
    n = len(span)
    m = lambda k: sum(d[k] for d in span) / n
    fed = min(d['households_fed'] / max(1, d['households']) for d in span)
    paid = sum(d['wages_paid'] for d in span) / max(1, sum(d['wages_owed'] for d in span))
    res = span[-1]['residents']
    print(f'{name[:28]:28} d{span[0]["day"]}-{span[-1]["day"]} hungry {m("hungry_mean"):5.2f} ({100 * m("hungry_mean") / res:4.1f}%) '
          f'worst {max(d["hungry_most"] for d in span):3d} fed>={fed:5.1%} wages {paid:5.1%} tillsShort {m("tills_short"):4.1f} '
          f'profit {m("profit"):5.0f} alms {m("alms"):4.0f} treasury {span[0]["treasury"]:5d}->{span[-1]["treasury"]:5d} '
          f'country {m("country"):5.0f} unspent {m("country_unspent"):4.0f} purses {span[0]["purses"]}->{span[-1]["purses"]} '
          f'gini {span[0]["gini_earners"]:.2f}->{span[-1]["gini_earners"]:.2f} conserved {all(d["conserved"] for d in days)}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('runs', nargs='+')
    ap.add_argument('--days', type=int, default=7)
    ap.add_argument('--from', dest='start', type=int, default=0)
    ap.add_argument('--fields', default=DEFAULT)
    ap.add_argument('--summary', action='store_true')
    a = ap.parse_args()
    for path in a.runs:
        days = load(path)
        if a.summary:
            summary(path.split('/')[-1], days, a.start)
            continue
        fields = a.fields.split(',')
        print(path)
        print('days     ' + ' '.join(f'{f[:11]:>11}' for f in fields))
        for i in range(a.start, len(days), a.days):
            chunk = days[i:i + a.days]
            row = []
            for f in fields:
                vals = [d.get(f, 0) for d in chunk]
                v = vals[-1] if f in ('treasury', 'country', 'tills', 'purses', 'gini_earners') else sum(vals) / len(vals)
                row.append(f'{v:11.2f}' if isinstance(v, float) and abs(v) < 10 else f'{v:11.0f}')
            print(f'{chunk[0]["day"]:3}-{chunk[-1]["day"]:3}  ' + ' '.join(row))


if __name__ == '__main__':
    main()
