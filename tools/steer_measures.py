#!/usr/bin/env python3
"""The economy controller's measures for a batch of runs (Docs/Design/75-the-economy-controller.md, 3.4), as one
markdown file: G (all households, cash a head, weighted by heads: before and after the tithe and the day's mean), F
(pantry days a head), S, the earners' Gini, hunger, the treasury, the shops, the skim by payer group and how much of it
was idle money, and from the full log the income shares by class, the luxuries and cheese's price.

    python3 tools/steer_measures.py OUT.md RUN [RUN ...] [--dir artifacts/economy/r2] [--from 120] [--to 365]

RUN is a run's file stem (`steer-b0-left` for steer-b0-left.jsonl and steer-b0-left.full.jsonl.gz).
"""
import argparse
import collections
import os
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import econ_report  # noqa: E402  (full_days: the full log with its dictionary applied)
import steer_accept  # noqa: E402  (path, load, trailing)


def mean(xs):
    xs = [x for x in xs if x is not None and x == x and x != -1]
    return statistics.fmean(xs) if xs else float("nan")


def f(v, p=2):
    return "-" if v is None or v != v else f"{v:,.{p}f}"


def _full_span(path, a, b):
    for _meta, names, d in econ_report.full_days(path):
        if d["day"] > b:
            break
        if a <= d["day"]:
            yield names, d


def from_full(path, a, b):
    """Income by class (pennies into each class's households from outside households, a day), luxuries and cheese."""
    income = collections.Counter()
    cheese_pieces = cheese_pennies = 0
    cheese_factor = []
    days = 0
    if not os.path.exists(path):
        return None
    try:
        rows = list(_full_span(path, a, b))
    except (EOFError, OSError):
        rows = []                                   # (A run still writing its full log: the daily lines only.)
    for names, d in rows:
        days += 1
        n = names.__getitem__
        for fr, to, pennies in d.get("flow", []):
            fr, to = n(fr), n(to)
            if to.startswith("hh:") and not fr.startswith("hh:"):
                income[to[3:]] += pennies
        for t in d.get("trade", []):
            if n(t[0]) == "cheese":
                buyer = n(t[1])
                if buyer.startswith("hh:") or buyer.startswith("estate"):
                    cheese_pieces += t[3]
                    cheese_pennies += t[4]
        for w in d.get("ware", []):
            if n(w[1]) == "cheese" and w[2] > 0:
                cheese_factor.append(w[4] / 1000)
    total = sum(income.values()) or 1
    return {
        "income": {k: 100 * v / total for k, v in income.items()},
        "income_day": total / max(days, 1),
        "cheese_price": cheese_pennies / cheese_pieces if cheese_pieces else None,
        "cheese_sold": cheese_pieces / max(days, 1),
        "cheese_shelf": mean(cheese_factor),
    }


def summary(dir_, run, a, b):
    days = steer_accept.load(dir_, run)
    ys = [d for d in days if a <= d["day"] <= b]
    tr28 = steer_accept.trailing([d.get("treasury") for d in days], 28)
    skim = sum(d.get("skim", 0) for d in ys)
    by = collections.Counter()
    for d in ys:
        by.update(d.get("skim_by") or {})
    lux = collections.Counter()
    for d in ys:
        for c, v in (d.get("luxury_classes") or {}).items():
            lux[c] += v.get("spent", 0)
    full = from_full(os.path.join(dir_, run + ".full.jsonl.gz"), a, b)
    n = len(ys) or 1
    return {
        "days": len(ys),
        "G_day": mean(d.get("m_g_day") for d in ys), "G_after": mean(d.get("m_g_after") for d in ys),
        "G_before": mean(d.get("m_g_before") for d in ys), "F": mean(d.get("m_food_gini") for d in ys),
        "G_earners": mean(d.get("gini_earners") for d in ys), "S": mean(d.get("m_upper_share") for d in ys),
        "top10": mean(d.get("m_top10") for d in ys),
        "H": mean(d.get("hungry_mean") for d in ys), "K": mean(d.get("households_short") for d in ys),
        "K_after": mean(d.get("households_short_after") for d in ys),
        "treasury": mean(d.get("treasury") for d in ys), "treasury_least": min((d.get("treasury_low", d.get("treasury")) for d in ys), default=None),
        "float_days": sum(1 for d in ys if d.get("treasury_low", 1e9) <= 200),
        "tr28_least": min((t for d, t in zip(days, tr28) if a <= d["day"] <= b and t is not None), default=None),
        "tills": mean(d.get("tills_short") for d in ys), "unpaid": mean(d.get("support_unpaid") for d in ys),
        "support": mean(d.get("wage_support") for d in ys), "alms": mean(d.get("alms") for d in ys),
        "skim": skim / n,
        "floors_skim": sum((d.get("skim_for") or {}).get("never hungry", 0) + (d.get("skim_for") or {}).get("never broke", 0) for d in ys) / n,
        "idle": sum(d.get("skim_idle", 0) for d in ys) / n, "busy": sum(d.get("skim_busy", 0) for d in ys) / n,
        "skim_by": {k: v / n for k, v in by.most_common()},
        "skim_short": sum(d.get("skim_short", 0) for d in ys) / n,
        "guard": sum(1 for d in ys if d.get("skim_guard")),
        "nb_credit": mean(steer_accept.sub(d, "never_broke", "credit", default=0) for d in ys),
        "nb_spent": mean(steer_accept.sub(d, "never_broke", "pennies", default=0) for d in ys),
        "nh_pennies": mean(steer_accept.sub(d, "never_hungry", "pennies", default=0) for d in ys),
        "nh_wharf": mean(steer_accept.sub(d, "never_hungry", "wharf_pennies", default=0) for d in ys),
        "lux": {k: v / n for k, v in lux.items()}, "lux_houses": mean(d.get("luxuries_houses_spent") for d in ys),
        "conserved": all(d.get("conserved", True) for d in ys), "money": ys[-1].get("money") if ys else None,
        "full": full,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("out")
    ap.add_argument("runs", nargs="+")
    ap.add_argument("--dir", default="artifacts/economy/r2")
    ap.add_argument("--from", dest="a", type=int, default=120)
    ap.add_argument("--to", dest="b", type=int, default=365)
    a = ap.parse_args()
    dir_ = steer_accept.path(a.dir)
    rows = {r: summary(dir_, r, a.a, a.b) for r in a.runs}
    o = [f"# The controller's measures (doc 75, 3.4), days {a.a}-{a.b}\n",
         "Means a day. G: the Gini of cash a head over all households, weighted by heads (the day's mean of before and "
         "after the tithe; with no tithe they are the same); F: the same Gini of pantry days a head; S: the upper class, "
         "the estates and the Houses' tills, percent of all money.\n",
         "## The distribution and the city\n",
         "| run | G (day) | G after | F | Gini earners | S % | top-10% | H/h | K | K after relief | treasury mean (least; 28-day least) | float days | tills short | wage support | support unpaid | alms | conserved |",
         "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for r, s in rows.items():
        o.append(f"| {r} | {f(s['G_day'],3)} | {f(s['G_after'],3)} | {f(s['F'],3)} | {f(s['G_earners'],3)} | {f(s['S'],1)} | {f(s['top10'],1)} | "
                 f"{f(s['H'],3)} | {f(s['K'])} | {f(s['K_after'])} | {f(s['treasury'],0)} ({f(s['treasury_least'],0)}; {f(s['tr28_least'],0)}) | "
                 f"{s['float_days']} | {f(s['tills'],1)} | {f(s['support'],0)} | {f(s['unpaid'],0)} | {f(s['alms'],0)} | {s['conserved']} |")
    o += ["", "## The skim and the floors\n",
          "| run | skim/day (of it the floors') | the floors' idle (first pass) | busy (second pass) | busy share of the floors' | short | guard days | by payer group (pennies a day) | Never Broke credit given / spent | Never Hungry pennies (Wharf's) |",
          "|---|---|---|---|---|---|---|---|---|---|"]
    for r, s in rows.items():
        busy = s["busy"] / s["floors_skim"] if s["floors_skim"] else float("nan")
        groups = ", ".join(f"{k} {v:.0f}" for k, v in s["skim_by"].items())
        o.append(f"| {r} | {f(s['skim'],0)} ({f(s['floors_skim'],0)}) | {f(s['idle'],0)} | {f(s['busy'],0)} | {f(100 * busy,0)}% | {f(s['skim_short'],1)} | {s['guard']} | "
                 f"{groups or '-'} | {f(s['nb_credit'],0)} / {f(s['nb_spent'],0)} | {f(s['nh_pennies'],2)} ({f(s['nh_wharf'],2)}) |")
    o += ["", "## Income by class, the luxuries and cheese\n",
          "Income: pennies into each class's households from outside the households (wages, profit, alms, relief, dividends), "
          "as shares of all of it. Cheese: what households and the Houses paid a piece, the pieces a day, and its shelves' "
          "price (its rules untouched).\n",
          "| run | income/day | upper % | middle % | lower % | luxuries by class (pennies a day) | Houses' luxuries | cheese p a piece | cheese pieces/day | cheese shelf price p (mean of shelves holding it) |",
          "|---|---|---|---|---|---|---|---|---|---|"]
    for r, s in rows.items():
        fu = s["full"] or {}
        inc = fu.get("income", {})
        lux = ", ".join(f"{k} {v:.0f}" for k, v in sorted(s["lux"].items()))
        o.append(f"| {r} | {f(fu.get('income_day'),0)} | {f(inc.get('upper'),1)} | {f(inc.get('middle'),1)} | {f(inc.get('lower'),1)} | "
                 f"{lux or '-'} | {f(s['lux_houses'],0)} | {f(fu.get('cheese_price'),2)} | {f(fu.get('cheese_sold'),1)} | {f(fu.get('cheese_shelf'),3)} |")
    with open(a.out, "w") as fh:
        fh.write("\n".join(o) + "\n")
    print("\n".join(o))


if __name__ == "__main__":
    main()
