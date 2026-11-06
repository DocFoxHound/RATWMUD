#!/usr/bin/env python3
"""The slider's runs (Docs/Design/74-steering-the-economy.md, Progress, "The slider's runs"): the steered runs against
stable10b, by year, as one markdown file. Reads only the daily lines (`<run>.jsonl`).

    python3 artifacts/economy/r2/steer-compare.py OUT.md [--runs observe centre left right centre-floors] [--sweep sweep]
"""
import argparse
import collections
import json
import os
import statistics

R2 = os.path.dirname(os.path.abspath(__file__))
LEVERS = [("lev_share_out", "share-out %"), ("lev_posts_share", "hands' share %"), ("lev_toll", "toll lean"),
          ("lev_surplus_keep", "treasury keep p"), ("lev_cap", "treasury cap p"), ("lev_alms_to", "poorest raised to p"),
          ("lev_ground_rent", "ground rent %"), ("lev_rent_scale", "rent scale %"), ("lev_dividend", "dividend %"),
          ("lev_rich_above", "rich hold p"), ("lev_spend_above", "spending keep p")]
FLOWS = [("share_out", "share-out"), ("posts_share", "hands' share"), ("ground_rent", "ground rent"),
         ("dividend", "dividend"), ("land_rent_extra", "rent scale extra"), ("surplus_paid", "surplus paid"),
         ("ground_rent_treasury", "ground rent to the treasury")]


SUFFIX = ""


def load(name):
    if name != "stable10b":
        name += SUFFIX
    with open(os.path.join(R2, name + ".jsonl")) as f:
        return [json.loads(line) for line in f]


def mean(xs):
    xs = [x for x in xs if x is not None]
    return statistics.fmean(xs) if xs else float("nan")


def years(days):
    out = collections.OrderedDict()
    for d in days:
        out.setdefault(min(d["day"] // 365, 2) + 1, []).append(d)
    return out


def estate_posts(d):
    e = d.get("estates") or {}
    owed = sum(v.get("posts_owed", 0) for v in e.values())
    paid = sum(v.get("posts_paid", 0) for v in e.values())
    return owed, paid


def run_year(ys):
    owed = sum(estate_posts(d)[0] for d in ys)
    paid = sum(estate_posts(d)[1] for d in ys)
    pushes = [d.get("steer_push", 0) for d in ys]
    return {
        "days": len(ys),
        "push": (mean(pushes), min(pushes), max(pushes)),
        "gini": mean(d.get("gini_earners") for d in ys),
        "m_gini": mean(d.get("m_gini") for d in ys),
        "upper": mean(d.get("m_upper_share") for d in ys),
        "top10": mean(d.get("m_top10") for d in ys),
        "hungry": mean(d.get("hungry_mean") for d in ys),
        "short": mean(d.get("households_short") for d in ys),
        "treasury": mean(d.get("treasury") for d in ys),
        "treasury_min": min(d.get("treasury", 0) for d in ys),
        "toll": mean(d.get("toll_now") for d in ys),
        "support": mean(d.get("wage_support") for d in ys),
        "unpaid": mean(d.get("support_unpaid") for d in ys),
        "tills_short": mean(d.get("tills_short") for d in ys),
        "alms": mean(d.get("alms") for d in ys),
        "surplus": mean(d.get("surplus_paid", d.get("surplus")) for d in ys),
        "posts": (paid, owed),
        "conserved": all(d.get("conserved") for d in ys),
        "money": ys[-1].get("money"),
        "levers": {k: mean(d.get(k) for d in ys) for k, _ in LEVERS},
        "flows": {k: mean(d.get(k) for d in ys) for k, _ in FLOWS},
        "skim": mean(d.get("skim") for d in ys),
    }


def f(v, p=2):
    if v is None or (isinstance(v, float) and v != v):
        return "-"
    return f"{v:,.{p}f}"


def comparison(runs, out):
    base = years(load("stable10b"))
    rows = {name: years(load("steer-" + name)) for name in runs}
    out.append("## The runs by year, against stable10b\n")
    out.append("Means of the daily lines (the measures at 23:00 except `m_*`, the steering's own at 18:00). The upper share and "
               "the top tenth have no field in stable10b; `observe` (the steering on with its band 0 wide, the push held at 0) "
               "is stable10b with them logged.\n")
    head = ("| run | year | push mean (least..most) | Gini earners | upper share % | top-10% % | hungry/h | short/day | "
            "treasury mean (least) | toll | wage support/day | support unpaid/day | tills short | alms/day | estates' posts paid/owed | conserved | money at end |")
    out.append(head)
    out.append("|" + "|".join(["---"] * (head.count("|") - 1)) + "|")
    for y in (1, 2, 3):
        b = run_year(base[y])
        out.append(f"| stable10b | {y} | - | {f(b['gini'],3)} | - | - | {f(b['hungry'],3)} | {f(b['short'])} | "
                   f"{f(b['treasury'],0)} ({f(b['treasury_min'],0)}) | {f(b['toll'],1)} | {f(b['support'],0)} | {f(b['unpaid'],0)} | "
                   f"{f(b['tills_short'],1)} | {f(b['alms'],0)} | {b['posts'][0]}/{b['posts'][1]} | {b['conserved']} | {b['money']} |")
    for name, ys in rows.items():
        for y, days in ys.items():
            r = run_year(days)
            p = r["push"]
            out.append(f"| {name} | {y} | {f(p[0])} ({f(p[1])}..{f(p[2])}) | {f(r['gini'],3)} | {f(r['upper'],1)} | {f(r['top10'],1)} | "
                       f"{f(r['hungry'],3)} | {f(r['short'])} | {f(r['treasury'],0)} ({f(r['treasury_min'],0)}) | {f(r['toll'],1)} | "
                       f"{f(r['support'],0)} | {f(r['unpaid'],0)} | {f(r['tills_short'],1)} | {f(r['alms'],0)} | "
                       f"{r['posts'][0]}/{r['posts'][1]} | {r['conserved']} | {r['money']} |")
    out.append("")
    out.append("## The levers' year means\n")
    head = "| run | year | " + " | ".join(n for _, n in LEVERS) + " |"
    out.append(head)
    out.append("|" + "|".join(["---"] * (head.count("|") - 1)) + "|")
    for name, ys in rows.items():
        for y, days in ys.items():
            r = run_year(days)
            out.append(f"| {name} | {y} | " + " | ".join(f(r["levers"][k], 1) for k, _ in LEVERS) + " |")
    out.append("")
    out.append("## The levers' money a day (year means)\n")
    head = "| run | year | " + " | ".join(n for _, n in FLOWS) + " | skim |"
    out.append(head)
    out.append("|" + "|".join(["---"] * (head.count("|") - 1)) + "|")
    for name, ys in rows.items():
        for y, days in ys.items():
            r = run_year(days)
            out.append(f"| {name} | {y} | " + " | ".join(f(r["flows"][k], 0) for k, _ in FLOWS) + f" | {f(r['skim'],0)} |")
    out.append("")


def judgings(name, out, most=40):
    days = load("steer-" + name)
    out.append(f"### The judgings of `{name}`\n")
    n = 0
    for d in days:
        if d.get("steer_judged") or (d.get("steer_why") and "turned on" not in d.get("steer_why", "") and n < most):
            out.append(f"- {d.get('steer_why')}")
            n += 1
            if n >= most:
                out.append("- ...")
                break
    out.append("")


def floors(name, out):
    days = load("steer-" + name)
    out.append(f"## The floors and the skim (`{name}`)\n")
    out.append("| quarter | days | Never Hungry pennies/day (pieces, of them the Wharf's) | Never Broke pennies/day (homes) | skim/day | "
               "skim by payer group (share) | largest group | relief paid | fund:relief at end | short after | skim short |")
    out.append("|---|---|---|---|---|---|---|---|---|---|---|")
    prev = 0
    unbalanced = 0
    total_skim = total_relief = 0
    for q in range(0, len(days), 91):
        qs = days[q:q + 91]
        by = collections.Counter()
        skim = relief = 0
        for d in qs:
            by.update(d.get("skim_by", {}))
            skim += d.get("skim", 0)
            r = sum((d.get("relief") or {}).values())
            relief += r
            if d.get("skim", 0) != r + d.get("relief_fund", 0) - prev:
                unbalanced += 1
            prev = d.get("relief_fund", 0)
        total_skim += skim
        total_relief += relief
        nh = sum(d["never_hungry"]["pennies"] for d in qs)
        nhp = sum(d["never_hungry"]["pieces"] for d in qs)
        nhw = sum(d["never_hungry"]["wharf_pieces"] for d in qs)
        nb = sum(d["never_broke"]["pennies"] for d in qs)
        nbh = sum(d["never_broke"]["homes"] for d in qs)
        tot = sum(by.values()) or 1
        groups = ", ".join(f"{g} {100 * v / tot:.0f}%" for g, v in by.most_common(5))
        top = by.most_common(1)[0] if by else ("-", 0)
        out.append(f"| {q // 91 + 1} (days {qs[0]['day']}-{qs[-1]['day']}) | {len(qs)} | {nh / len(qs):.1f} ({nhp}, {nhw}) | "
                   f"{nb / len(qs):.1f} ({nbh / len(qs):.1f}) | {skim / len(qs):.1f} | {groups} | {top[0]} {100 * top[1] / tot:.0f}% | "
                   f"{relief} | {qs[-1].get('relief_fund', 0)} | {sum(d.get('households_short_after', 0) for d in qs)} | "
                   f"{sum(d.get('skim_short', 0) for d in qs)} |")
    out.append("")
    out.append(f"The skim's books: skimmed {total_skim}p, paid out {total_relief}p, fund:relief at the end "
               f"{days[-1].get('relief_fund', 0)}p; days where skim != relief + the fund's change: {unbalanced}.\n")


def sweep(name, out):
    days = load("steer-" + name)
    out.append(f"## The sweep (`{name}`): the slider moved mid-run\n")
    out.append("The slider at the centre from day 0, moved to far left (-100) at day 365 and far right (+100) at day 730 by "
               "`--rule-at` (each applied as the DM's economy.sync is: the world's rules with that one changed, adopted).\n")
    out.append("| day | lean | push | band | share-out % | hands' % | toll lean | ground rent % | dividend % | rich hold p | "
               "Gini (56-day mean) | upper % (56-day) | top-10% (56-day) | hungry/h (28) | short (28) | treasury (28) |")
    out.append("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    marks = sorted(set([0, 55, 100, 200, 300, 364] + [365 + k for k in (0, 1, 7, 28, 56, 84, 112, 168, 252, 364)] +
                       [730 + k for k in (0, 1, 7, 28, 56, 84, 112, 168, 252, 364)]))
    for d in days:
        if d["day"] in marks:
            out.append(f"| {d['day']} | {d.get('steer_lean')} | {f(d.get('steer_push'))} | {f(d.get('steer_push_lo'))}..{f(d.get('steer_push_hi'))} | "
                       f"{f(d.get('lev_share_out'),1)} | {f(d.get('lev_posts_share'),1)} | {f(d.get('lev_toll'),1)} | "
                       f"{f(d.get('lev_ground_rent'),1)} | {f(d.get('lev_dividend'),1)} | {f(d.get('lev_rich_above'),0)} | "
                       f"{f(d.get('g_mean'),3)} | {f(d.get('s_mean'),1)} | {f(d.get('t10_mean'),1)} | {f(d.get('h_mean28'),3)} | "
                       f"{f(d.get('k_mean28'),2)} | {f(d.get('tr_mean28'),0)} |")
    out.append("")
    # How fast: days from each move until the 56-day mean has gone half and 90% of the way to where the run settles
    # (its last 91 days' mean).
    out.append("How fast the measures followed each move (days after the move until the 56-day mean had gone half / 90% of "
               "the way from its value at the move to its mean over the last 91 days before the next move):\n")
    for start, end, label in ((365, 730, "centre -> far left"), (730, len(days), "far left -> far right")):
        seg = [d for d in days if start <= d["day"] < end]
        if not seg:
            continue
        line = [f"- **{label}** (day {start}): the push {f(days[start - 1].get('steer_push'))} -> {f(seg[0].get('steer_push'))} the "
                "same night;"]
        for key, label2 in (("g_mean", "Gini"), ("s_mean", "upper share"), ("t10_mean", "top-10%")):
            v0 = days[start - 1].get(key)
            settle = mean(d.get(key) for d in seg[-91:])
            if v0 is None or settle != settle:
                continue
            half = ninety = None
            for d in seg:
                gone = (d.get(key) - v0) / (settle - v0) if settle != v0 else 1
                if half is None and gone >= .5:
                    half = d["day"] - start
                if ninety is None and gone >= .9:
                    ninety = d["day"] - start
            line.append(f" {label2} {f(v0, 3)} -> {f(settle, 3)}: half in {half} days, 90% in {ninety} days;")
        out.append("".join(line))
    out.append("")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--runs", nargs="*", default=["observe", "centre", "left", "right", "centre-floors"])
    ap.add_argument("--sweep", default="sweep")
    ap.add_argument("--floors", default="centre-floors")
    ap.add_argument("--suffix", default="", help="the runs' suffix (-60 for the smokes)")
    a = ap.parse_args()
    global SUFFIX
    SUFFIX = a.suffix
    runs = [r for r in a.runs if os.path.exists(os.path.join(R2, "steer-" + r + SUFFIX + ".jsonl"))]
    out = ["# The slider's runs: steered against stable10b (doc 74)\n"]
    comparison(runs, out)
    if a.floors in runs:
        floors(a.floors, out)
    if a.sweep and os.path.exists(os.path.join(R2, "steer-" + a.sweep + SUFFIX + ".jsonl")):
        sweep(a.sweep, out)
        runs = runs + [a.sweep]
    out.append("## The judgings\n")
    for r in runs:
        judgings(r, out)
    with open(a.out, "w") as fh:
        fh.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
