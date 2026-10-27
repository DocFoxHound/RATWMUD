#!/usr/bin/env python3
"""One econ_watch run on a page (Docs/Design/64-economy-migration.md, Phase 13, round 10, lane 0), or two side by side.

    python3 tools/econ_model/report.py RUN                # a run's summary
    python3 tools/econ_model/report.py RUN_A RUN_B        # two runs, month by month, side by side
    options: --month DAYS (28: the game's month), --quarter DAYS (84), --top N (5: drains and pumps a quarter)

A run is the directory econ_watch wrote. Its summary:

- month by month: the households' Gini and the poorer half's share, money by kind of holder against the model's
  (tools/econ_model/model_shares.csv; compare.py's groups, with the grown wolves out of work among the homes), the
  labourer's day wage, people, the hungry and the starving split into money-hunger (holding less than the cheapest
  food) and food-hunger (could pay), and each town's treasury, the capital's and the church's (and the Houses');
- each quarter: the accounts that lost the most (drains) and gained the most (pumps), with the kinds of entry that
  moved most of it (a town's caravans counted as one);
- the sentinels fired.

Only the standard library. An older run (before round 10) has no accounts.csv, hunger.csv or sentinels.csv: the quarters
then show the purses' changes from days.jsonl without kinds, the hunger split is made from days.jsonl's hungry
residents against the day's cheapest food in prices.csv, and the sentinels are said to be missing.
"""
import argparse
import csv
import json
import pathlib
import sys
from collections import defaultdict

HERE = pathlib.Path(__file__).resolve().parent
GROUPS = ("homes", "firms", "Houses", "church", "town")
FOUNDING = {"starting money", "initial funding", "a great house's fortune"}   # (Left out of the quarters' nets.)
HOMES = {"merchant", "guard", "producer", "paid civilian", "unpaid civilian", "apprentice", "child", "retired", "resident",
         "player", "adult without work"}


def group_of(kind):
    """A kind of holder's group (econ_watch's groupOf; compare.py's GROUPS, the jobless grown among the homes)."""
    if kind in HOMES or kind.startswith("resident"):
        return "homes"
    if kind in ("shop till", "house till", "farm till"):
        return "firms"
    if kind == "great house":
        return "Houses"
    if kind == "town church":
        return "church"
    if kind in ("town treasury", "capital treasury") or kind.startswith("town "):
        return "town"
    return None


def rows(path):
    with open(path, newline="") as f:
        yield from csv.DictReader(f)


def model_shares():
    path = HERE / "model_shares.csv"
    if not path.exists():
        return {}
    return {int(r["day"]): {g: float(r[g]) for g in GROUPS} for r in rows(path)}


def model_at(model, day):
    days = [d for d in model if d <= day]
    return model[max(days)] if days else None


class Run:
    def __init__(self, path):
        self.path = pathlib.Path(path)
        if not (self.path / "days.jsonl").exists():
            raise SystemExit(f"{self.path}: no days.jsonl (not an econ_watch run?)")
        self.stamp = (self.path / "run.txt").read_text().splitlines() if (self.path / "run.txt").exists() else []
        self.days = {}           # day -> its record (the day's end; the run's start and end too)
        self.end = None
        self.notes = []
        self.unconserved = []
        with open(self.path / "days.jsonl") as f:
            for line in f:
                if not line.strip():
                    continue
                try:
                    d = json.loads(line)
                except json.JSONDecodeError:
                    self.notes.append("days.jsonl ends in a broken line (a run cut short?)")
                    break
                if not d.get("conserved", True):
                    self.unconserved.append(d["day"])
                if d.get("when") == "end":
                    self.end = d
                elif d.get("when") in ("day end", "start"):
                    self.days[d["day"]] = d
        if self.end is None and self.days:
            self.end = self.days[max(self.days)]
            self.notes.append("no end record: the run didn't finish (or is still running)")
        self.first, self.last = min(self.days), self.end["day"]
        self.hunger = self.load_hunger()
        self.accounts = (self.path / "accounts.csv").exists() and (self.path / "account_flows.csv").exists()

    def shares(self, d):
        held = defaultdict(float)
        total = 0.0
        for kind, (cash, _n) in d["holders"].items():
            if cash > 0:
                total += cash
                g = group_of(kind)
                if g:
                    held[g] += cash
        return {g: held[g] / total if total else 0.0 for g in GROUPS}

    def load_hunger(self):
        """Day -> (hungry, of them money-hungry, starving, of them money-starving), or None where it can't be told."""
        out = {}
        path = self.path / "hunger.csv"
        if path.exists():
            acc = defaultdict(lambda: [0, 0, 0, 0])
            for r in rows(path):
                a = acc[int(r["day"])]
                a[0] += int(r["hungry"]); a[1] += int(r["hungry_poor"]); a[2] += int(r["starving"]); a[3] += int(r["starving_poor"])
            return {d: tuple(v) for d, v in acc.items()}
        # An older run: the day's hungry residents (days.jsonl) against the day's cheapest food a resident bought, on
        # average (prices.csv).
        self.notes.append("no hunger.csv: money-hunger told from days.jsonl's hungry against prices.csv's cheapest food")
        cheapest = {}
        if (self.path / "prices.csv").exists():
            for r in rows(self.path / "prices.csv"):
                if r["kind"] == "resident food purchase" and int(r["units"]) > 0:
                    day, each = int(r["day"]), int(r["coins"]) / int(r["units"])
                    cheapest[day] = min(cheapest.get(day, each), each)
        for day, d in self.days.items():
            price = cheapest.get(day - 1) or cheapest.get(day)
            hungry = d.get("hungry_residents", [])
            poor = [h for h in hungry if price and h.get("cash", 0) < price]
            out[day] = (len(hungry), len(poor) if price else None, sum(h["hunger"] >= 90 for h in hungry),
                        sum(h["hunger"] >= 90 for h in poor) if price else None)
        return out

    def months(self, month):
        """The record days a month apart from the first, and the last."""
        picked = [d for d in sorted(self.days) if (d - self.first) % month == 0]
        if self.last not in picked and self.last in self.days:
            picked.append(self.last)
        return picked

    def record(self, day):
        return self.days.get(day) or (self.end if day == self.last else None)

    def treasuries(self, d):
        purses = d.get("purses", {})
        out = {k[7:]: v for k, v in purses.items() if k.startswith("stores:")}
        out["(capital)"] = purses.get("treasury", 0)
        out["(church)"] = purses.get("town:all:church", 0)
        out["(Houses)"] = sum(v for k, v in purses.items() if k.startswith("house:"))
        return out

    def sentinels(self):
        path = self.path / "sentinels.csv"
        return list(rows(path)) if path.exists() else None

    def quarter_moves(self, start, end):
        """Each account's net over the days (start, end]: in, out, its kinds. Caravans by their town, as one."""
        moves = defaultdict(lambda: {"in": 0, "out": 0, "kinds": defaultdict(int), "holder": "", "town": "", "balance": 0})
        if self.accounts:
            names = {}
            for r in rows(self.path / "accounts.csv"):
                day = int(r["day"])
                if not start < day <= end:
                    continue
                name = f"caravans of {r['town']}" if r["account"].startswith("caravan:") else r["account"]
                names[r["account"]] = name
                m = moves[name]
                m["in"] += int(r["in"]); m["out"] += int(r["out"])
                m["holder"], m["town"] = r["holder"], r["town"]
                if not r["account"].startswith("caravan:"):
                    m["balance"] = int(r["balance"])
            for r in rows(self.path / "account_flows.csv"):
                day = int(r["day"])
                if start < day <= end and r["account"] in names:
                    m = moves[names[r["account"]]]
                    if r["kind"] in FOUNDING:
                        m[r["dir"]] -= int(r["coins"])    # (The founding's money isn't a pump.)
                    else:
                        m["kinds"][r["kind"]] += (1 if r["dir"] == "in" else -1) * int(r["coins"])
            return moves
        # An older run: the purses' change (days.jsonl), no kinds.
        a, b = self.record(start), self.record(end)
        if a and b:
            for k in set(a.get("purses", {})) | set(b.get("purses", {})):
                change = b["purses"].get(k, 0) - a["purses"].get(k, 0)
                m = moves[k]
                m["in"], m["out"], m["balance"] = max(0, change), max(0, -change), b["purses"].get(k, 0)
        return moves


def fmt_k(v):
    return f"{v / 1000:,.1f}k" if abs(v) >= 1000 else f"{v:,.0f}"


def hunger_text(h):
    if not h:
        return "-"
    hungry, poor, starving, spoor = h
    if poor is None:
        return f"{hungry}/{starving}"
    return f"{hungry}({poor}m,{hungry - poor}f)/{starving}({spoor}m)"


def summary(run, a):
    model = model_shares()
    print(f"== {run.path}")
    for line in run.stamp[:2]:
        print("   " + line)
    print(f"   days {run.first} to {run.last}; money conserved {'every day' if not run.unconserved else 'NOT on days ' + str(run.unconserved[:10])}")
    minted = [(d, run.days[d]["minted"] - run.days[p]["minted"]) for p, d in zip(sorted(run.days), sorted(run.days)[1:])
              if d > run.first + 2 and run.days[d]["minted"] != run.days[p]["minted"]]
    if minted:
        print("   minted after the founding: " + ", ".join(f"day {d} {m:+,}p" for d, m in minted[:8]))
    for n in run.notes:
        print("   note: " + n)
    if not model:
        print("   note: no tools/econ_model/model_shares.csv: shares without the model's")

    print(f"\nMonth by month ({a.month} days). Shares of all money, game/model. Hungry (m money-hunger, f food-hunger)/starving.")
    head = f"{'day':>4} {'gini':>5} {'poor½':>5} " + " ".join(f"{g:>11}" for g in GROUPS) + f" {'wage':>6} {'people':>6}  hungry/starving"
    print(head)
    for day in run.months(a.month):
        d = run.record(day)
        m = d["model"]
        s = run.shares(d)
        t = model_at(model, day) if model else None
        cells = " ".join(f"{s[g]:5.3f}/{t[g]:.3f}" if t else f"{s[g]:11.3f}" for g in GROUPS)
        print(f"{day:4} {m['household_gini']:5.3f} {m['poorer_half']:5.3f} {cells} {m['wage']:6.1f} {m['people']:6} "
              f" {hunger_text(run.hunger.get(day))}")

    print(f"\nTreasuries, month by month (the towns' stores, the capital's, the church's, and the Houses' purses):")
    names = sorted({k for day in run.months(a.month) for k in run.treasuries(run.record(day))},
                   key=lambda k: (k.startswith("("), k))
    print(f"{'day':>4} " + " ".join(f"{n[:11]:>11}" for n in names))
    for day in run.months(a.month):
        t = run.treasuries(run.record(day))
        print(f"{day:4} " + " ".join(f"{fmt_k(t.get(n, 0)):>11}" for n in names))

    print(f"\nDrains and pumps by quarter ({a.quarter} days): each account's net, with the kinds that moved most of it"
          + (" (the founding's money left out)" if run.accounts else " (no accounts.csv: purses' changes, no kinds)") + ".")
    start = run.first
    while start < run.last:
        end = min(start + a.quarter, run.last)
        moves = run.quarter_moves(start, end)
        nets = sorted(((m["in"] - m["out"], k, m) for k, m in moves.items()), key=lambda x: x[0])
        print(f"  days {start}-{end}:")
        for label, picked in (("drains", nets[:a.top]), ("pumps", list(reversed(nets[-a.top:])))):
            for net, k, m in picked:
                if (label == "drains" and net >= 0) or (label == "pumps" and net <= 0):
                    continue
                kinds = sorted(m["kinds"].items(), key=lambda x: x[1] if net < 0 else -x[1])[:3]
                if run.accounts:
                    ratio = f", out/in {m['out'] / m['in']:.2f}" if m["in"] else ""
                    flows = f"in {m['in']:,}, out {m['out']:,}{ratio}; holds {m['balance']:,}"
                else:
                    flows = f"holds {m['balance']:,}"
                print(f"    {label[:-1]:5} {k[:44]:44} {net:+11,}p ({flows})")
                if kinds:
                    print("          " + "; ".join(f"{kind} {c:+,}" for kind, c in kinds))
        start = end

    fired = run.sentinels()
    print()
    if fired is None:
        print("Sentinels: no sentinels.csv (a run from before round 10).")
    elif not fired:
        print("Sentinels: none fired.")
    else:
        by = defaultdict(list)
        for r in fired:
            by[r["kind"]].append(r)
        print(f"Sentinels: {len(fired)} fired (" + ", ".join(f"{k} {len(v)}" for k, v in sorted(by.items())) + "):")
        for r in fired[:a.sentinels]:
            print(f"  day {r['day']:>4} {r['kind']:6} {r['subject']}: {r['detail']}")
        if len(fired) > a.sentinels:
            print(f"  ... and {len(fired) - a.sentinels} more (sentinels.csv)")


def side_by_side(runs, a):
    model = model_shares()
    print("Side by side: " + "  vs  ".join(f"{chr(65 + i)} = {r.path}" for i, r in enumerate(runs)))
    for i, r in enumerate(runs):
        print(f"  {chr(65 + i)}: days {r.first}-{r.last}" + (f"; {r.stamp[0]}" if r.stamp else ""))
        for n in r.notes:
            print(f"     note: {n}")
    days = sorted(set(runs[0].months(a.month)) | set(runs[1].months(a.month)))
    cols = [("gini", lambda r, d: d["model"]["household_gini"], "{:.3f}"),
            ("poor½", lambda r, d: d["model"]["poorer_half"], "{:.3f}")]
    cols += [(g, (lambda g: lambda r, d: r.shares(d)[g])(g), "{:.3f}") for g in GROUPS]
    cols += [("wage", lambda r, d: d["model"]["wage"], "{:.1f}"),
             ("hungry", lambda r, d: (r.hunger.get(d["day"]) or (d["hungry"],))[0], "{}"),
             ("money-h", lambda r, d: (r.hunger.get(d["day"]) or (0, None))[1], "{}"),
             ("starving", lambda r, d: d["starving"], "{}"),
             ("treasuries", lambda r, d: fmt_k(sum(v for k, v in r.treasuries(d).items() if k != "(Houses)")), "{}"),
             ("House purses", lambda r, d: fmt_k(r.treasuries(d)["(Houses)"]), "{}")]
    print(f"\nMonth by month ({a.month} days), A | B" + (" | model" if model else "") + ":")
    print(f"{'day':>4} " + " ".join(f"{c:>{19 if c in GROUPS and model else 13}}" for c, _, _ in cols))
    for day in days:
        cells = []
        for c, get, f in cols:
            vals = []
            for r in runs:
                d = r.record(day)
                v = get(r, d) if d else None
                vals.append("-" if v is None else f.format(v))
            if c in GROUPS and model:
                t = model_at(model, day)
                vals.append(f"{t[c]:.3f}" if t else "-")
            cells.append("|".join(vals).rjust(19 if c in GROUPS and model else 13))
        print(f"{day:4} " + " ".join(cells))
    print("\nSentinels:")
    for i, r in enumerate(runs):
        fired = r.sentinels()
        if fired is None:
            print(f"  {chr(65 + i)}: no sentinels.csv")
            continue
        by = defaultdict(list)
        for s in fired:
            by[s["kind"]].append(s)
        print(f"  {chr(65 + i)}: " + (", ".join(f"{k} {len(v)} (first day {v[0]['day']}: {v[0]['subject']})" for k, v in sorted(by.items()))
                                      or "none fired"))
    print(f"\nThe last quarter's drains, each run (top {a.top}):")
    for i, r in enumerate(runs):
        start = max(r.first, r.last - a.quarter)
        moves = r.quarter_moves(start, r.last)
        nets = sorted(((m["in"] - m["out"], k, m) for k, m in moves.items()), key=lambda x: x[0])[:a.top]
        print(f"  {chr(65 + i)} (days {start}-{r.last}): " + "; ".join(
            f"{k} {net:+,}p" + (f" [{max(m['kinds'].items(), key=lambda x: -x[1])[0]}]" if m["kinds"] else "")
            for net, k, m in nets if net < 0))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("runs", nargs="+", help="one run's directory, or two to set side by side")
    ap.add_argument("--month", type=int, default=28)
    ap.add_argument("--quarter", type=int, default=84)
    ap.add_argument("--top", type=int, default=5)
    ap.add_argument("--sentinels", type=int, default=40, help="sentinels listed at most")
    a = ap.parse_args()
    if len(a.runs) > 2:
        ap.error("one run, or two side by side")
    runs = [Run(p) for p in a.runs]
    if len(runs) == 1:
        summary(runs[0], a)
    else:
        side_by_side(runs, a)


if __name__ == "__main__":
    try:
        main()
    except BrokenPipeError:
        sys.exit(0)
