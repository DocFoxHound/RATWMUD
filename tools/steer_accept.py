#!/usr/bin/env python3
"""The economy controller's acceptance (Docs/Design/75-the-economy-controller.md, 5.3): reads a batch of runs (their
daily lines, `<run>.jsonl`), the scorecard (`artifacts/economy/r2/steer-scorecard.json`) and the targets at each mark,
prints PASS or FAIL for every test, writes a markdown report, and exits 1 on any FAIL.

    python3 tools/steer_accept.py BATCH.json [--scorecard FILE] [--out REPORT.md]

The batch:

    {"dir": "artifacts/economy/r2", "baseline": "stable10b", "targets": "artifacts/economy/r2/steer-targets-t1.json",
     "runs": [{"name": "steer-centre-t1", "x": 0, "floors": false, "rung": "full",
               "floors_off": "steer-centre-t1",       (floors-on runs: the same position's floors-off run, for T6)
               "ref": "steer-right-t1",               (rungs F1 and F3: the run whose numbers are the ceiling)
               "legs": [[120, -1], [240, 1], [360, 0]],   (a sweep, T12: the day of each move and where to)
               "off": true}]}                         (steering off, T9)

Rungs (doc 75, 7): `full` and `F2` are judged on the scorecard; `F1` and `F3` on "no worse than `ref`" in T4-T7.
Every test reads the daily lines only. A test with nothing to judge (a field the run doesn't log) says n/a and passes.
"""
import argparse
import collections
import json
import math
import os
import statistics
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(TOOLS)
SEASON_DAYS = 365


def main_checkout():
    import subprocess
    try:
        common = subprocess.check_output(["git", "-C", TREE, "rev-parse", "--git-common-dir"], text=True,
                                         stderr=subprocess.DEVNULL).strip()
        return os.path.dirname(os.path.abspath(os.path.join(TREE, common)))
    except (OSError, subprocess.CalledProcessError):
        return TREE


ROOT = main_checkout()


def path(p):
    if os.path.isabs(p):
        return p
    for base in (ROOT, TREE):
        if os.path.exists(os.path.join(base, p)):
            return os.path.join(base, p)
    return os.path.join(ROOT, p)


_cache = {}


def load(dir_, name):
    f = os.path.join(dir_, name + ".jsonl")
    if f not in _cache:
        with open(f) as fh:
            _cache[f] = [json.loads(line) for line in fh if line.strip()]
    return _cache[f]


def mean(xs):
    xs = [x for x in xs if x is not None and not (isinstance(x, float) and math.isnan(x))]
    return statistics.fmean(xs) if xs else float("nan")


def trailing(series, n):
    """The n-day trailing mean of a list (None where the value is missing)."""
    out, window = [], collections.deque()
    total = 0.0
    for v in series:
        if v is None:
            out.append(out[-1] if out else None)
            continue
        window.append(v)
        total += v
        if len(window) > n:
            total -= window.popleft()
        out.append(total / len(window))
    return out


def windows(days, sc):
    """The judged spans: year 1 from day w0 (120) to 365, then each later whole year."""
    w0, w1 = sc["window"]
    out = [(1, w0, w1)]
    last = days[-1]["day"]
    y = 2
    while (y - 1) * 365 + 364 <= last:
        out.append((y, (y - 1) * 365, (y - 1) * 365 + 364))
        y += 1
    return out


def span(days, a, b):
    return [d for d in days if a <= d["day"] <= b]


def sub(d, *keys, default=None):
    for k in keys:
        if not isinstance(d, dict) or k not in d:
            return default
        d = d[k]
    return d


def wharf_net(d):
    w = d.get("wharf_day") or {}
    return (w.get("in", 0) - w.get("out", 0)) if w else None


def treasury_floor(days, sc):
    """Per year-day: the run's seasonal profile of the treasury's 28-day mean (days 120 on, all its years, a 15-day
    centred mean), less 2 sigma of what is left, never under the scorecard's least (600p)."""
    tr = trailing([d.get("treasury") for d in days], 28)
    by = collections.defaultdict(list)
    for d, t in zip(days, tr):
        if d["day"] >= sc["window"][0] and t is not None:
            by[d["day"] % 365].append(t)
    raw = {k: mean(v) for k, v in by.items()}
    half = sc["profile_smooth"] // 2
    profile = {}
    for k in range(365):
        near = [raw[(k + j) % 365] for j in range(-half, half + 1) if (k + j) % 365 in raw]
        if near:
            profile[k] = mean(near)
    resid = [t - profile[d["day"] % 365] for d, t in zip(days, tr)
             if d["day"] >= sc["window"][0] and t is not None and d["day"] % 365 in profile]
    sigma = statistics.pstdev(resid) if len(resid) > 1 else 0
    # (A floor at least `sigma_least` under the profile, when the scorecard names one: a treasury held flat at its cap
    # has a sigma of a few pennies, and its floor would then count a 20p wobble as a dip.)
    sigma = max(sigma, sc["T5"].get("sigma_least", 0))
    least = sc["T5"]["floor_least"]
    return {k: max(least, v - 2 * sigma) for k, v in profile.items()}, sigma


class Judge:
    def __init__(self, batch, sc):
        self.batch, self.sc = batch, sc
        self.dir = path(batch.get("dir", "artifacts/economy/r2"))
        with open(path(batch["targets"])) as f:
            self.targets = json.load(f)
        self.base_name = batch.get("baseline", "stable10b")
        self.base = load(self.dir, self.base_name)
        self.rows = []          # (run, year, test, PASS/FAIL/n/a, detail)

    def mark(self, x):
        key = f"{x:g}"
        if key not in self.targets["marks"]:
            raise SystemExit(f"no targets at the mark {key}")
        return self.targets["marks"][key]

    def add(self, run, year, test, ok, detail):
        state = "n/a" if ok is None else ("PASS" if ok else "FAIL")
        self.rows.append((run, year, test, state, detail))

    # -- the measures ------------------------------------------------------------------------------------------------
    def measures(self, x):
        m = self.mark(x)
        if x < 0:
            return {"G": m["G"]}
        if x > 0:
            return {"S": m["S"]}
        return {"G": m["G"], "S": m["S"]}

    def band_series(self, days, spec):
        return trailing([d.get(spec["field"]) for d in days], self.sc["mean_days"])

    @staticmethod
    def inside(v, spec, one_sided):
        if v is None:
            return False
        hi = spec["target"] + spec["band"]
        lo = spec["target"] - spec["band"]
        return v <= hi if one_sided else lo <= v <= hi

    # -- the tests -------------------------------------------------------------------------------------------------
    def t1_t2(self, name, days, run, years):
        floors = run.get("floors", False)
        for key, spec in self.measures(run["x"]).items():
            series = self.band_series(days, spec)
            if all(v is None for v in series):
                self.add(name, 1, f"T1 reach {key}", None, f"no `{spec['field']}`")
                continue
            reached = next((d["day"] for d, v in zip(days, series) if self.inside(v, spec, floors)), None)
            by = self.sc["T1"]["by_day"]
            self.add(name, 1, f"T1 reach {key}", reached is not None and reached <= by,
                     f"{spec['field']} {spec['target']:g}±{spec['band']:g}{' (one-sided)' if floors else ''}: in it by day {reached} (by {by})")
            for y, a, b in years:
                pairs = [(d, v) for d, v in zip(days, series) if a <= d["day"] <= b and not d.get("sup_backoff")]
                if not pairs:
                    continue
                share = sum(self.inside(v, spec, floors) for _, v in pairs) / len(pairs)
                vals = [v for _, v in pairs if v is not None]
                self.add(name, y, f"T2 hold {key}", share >= self.sc["T2"]["share"],
                         f"{100 * share:.0f}% of days in band (at least {100 * self.sc['T2']['share']:.0f}%); "
                         f"{self.sc['mean_days']}-day mean {mean(vals):.3f} ({min(vals):.3f}..{max(vals):.3f})")

    def t3_t8(self, name, ys, y):
        acted = [d for d in ys if d.get("sup_actions")]
        if not any("sup_actions" in d for d in ys):
            self.add(name, y, "T3 no limit cycle", None, "no supervisor logged (`sup_actions`)")
            self.add(name, y, "T8 supervisor", None, "no supervisor logged")
            return
        fires = collections.Counter()
        prev = set()
        for d in ys:
            now = set(d.get("sup_actions") or [])
            for r in now - prev:
                fires[r] += 1
            prev = now
        back = sum(1 for d in ys if d.get("sup_backoff")) / len(ys)
        most = max(fires.values()) if fires else 0
        self.add(name, y, "T3 no limit cycle", most <= self.sc["T3"]["fires_most"] and back <= self.sc["T3"]["backoff_share_most"],
                 f"most fire-and-clear cycles of a rule {most} (at most {self.sc['T3']['fires_most']}); at a back-off "
                 f"{100 * back:.0f}% of days (at most {100 * self.sc['T3']['backoff_share_most']:.0f}%)")
        share = len(acted) / len(ys)
        self.add(name, y, "T8 supervisor", share <= self.sc["T8"]["nights_most"],
                 f"acts on {100 * share:.1f}% of nights (at most {100 * self.sc['T8']['nights_most']:.0f}%)")

    def metrics(self, days, ys, run):
        """The numbers T4-T7 read, for one span."""
        h = [d.get("hungry_mean", 0) for d in ys]
        h28 = [v for d, v in zip(days, trailing([d.get("hungry_mean", 0) for d in days], 28)) if ys[0]["day"] <= d["day"] <= ys[-1]["day"]]
        floor, _ = treasury_floor(load(self.dir, run["floor_from"]) if run.get("floor_from") else days, self.sc)
        tr28 = trailing([d.get("treasury") for d in days], 28)
        under = collections.Counter()
        at_float = collections.Counter()
        for d, t in zip(days, tr28):
            if ys[0]["day"] <= d["day"] <= ys[-1]["day"]:
                if t is not None and t < floor.get(d["day"] % 365, self.sc["T5"]["floor_least"]):
                    under[d.get("season")] += 1
                if d.get("treasury_low", d.get("treasury", 1e9)) <= self.sc["T5"]["float"]:
                    at_float[d.get("season")] += 1
        nets = [wharf_net(d) for d in ys]
        return {
            "h": mean(h), "h28": max(h28) if h28 else 0, "k": mean(d.get("households_short", 0) for d in ys),
            "float": at_float, "under": under,
            "tills": mean(d.get("tills_short") for d in ys), "unpaid": mean(d.get("support_unpaid") for d in ys),
            "conserved": all(d.get("conserved", True) for d in ys),
            "posts": all(d.get("estate_posts_paid", 0) >= d.get("estate_posts_owed", 0) for d in ys),
            "wharf": mean(nets) if any(n is not None for n in nets) else None,
            "treasury": mean(d.get("treasury") for d in ys),
        }

    def t4_t7(self, name, days, ys, y, a, b, run):
        m = self.mark(run["x"])
        # (Each run under its mark's floor: the mark's calibration run's profile where the targets name one, else its own;
        # stable10b under its own.)
        r = self.metrics(days, ys, {"floor_from": m.get("floor_from")})
        base = self.metrics(self.base, span(self.base, a, b), {})
        rung = run.get("rung", "full")
        if rung in ("F1", "F3"):
            ref = load(self.dir, run["ref"])
            rm = self.metrics(ref, span(ref, a, b), {})
            eps = 1e-9
            self.add(name, y, "T4 hunger (no worse than ref)", r["h"] <= rm["h"] + eps and r["k"] <= rm["k"] + eps,
                     f"H {r['h']:.3f} (ref {rm['h']:.3f}); K {r['k']:.2f} (ref {rm['k']:.2f})")
            fl, rfl = sum(r["float"].values()), sum(rm["float"].values())
            self.add(name, y, "T5 treasury (no worse than ref)", fl <= rfl, f"days at the float {fl} (ref {rfl})")
            self.add(name, y, "T6 shops (no worse than ref)", r["tills"] <= rm["tills"] * 1.0001 and r["unpaid"] <= rm["unpaid"] * 1.0001,
                     f"tills short {r['tills']:.1f} (ref {rm['tills']:.1f}); support unpaid {r['unpaid']:.0f} (ref {rm['unpaid']:.0f})")
            self.add(name, y, "T7 books", r["conserved"] and r["posts"], f"conserved {r['conserved']}, estates' posts paid {r['posts']}")
            return
        hm, km = m["hungerMost"], m["shortMost"]
        ok4 = r["h"] <= hm and r["h28"] <= self.sc["T4"]["h28_times"] * hm and r["k"] <= km
        self.add(name, y, "T4 hunger", ok4, f"H {r['h']:.3f} (at most {hm:g}); worst 28-day H {r['h28']:.3f} (at most "
                 f"{self.sc['T4']['h28_times'] * hm:g}); K {r['k']:.2f} (at most {km:g})")
        plus = self.sc["T5"]["plus"]
        seasons = sorted(set(r["float"]) | set(r["under"]) | set(base["float"]) | set(base["under"]), key=str)
        bad = []
        for s in seasons:
            for kind in ("float", "under"):
                if r[kind][s] > base[kind][s] + plus:
                    bad.append(f"{s} {kind} {r[kind][s]} > {base[kind][s]}+{plus}")
        self.add(name, y, "T5 treasury", not bad,
                 f"days at the float {sum(r['float'].values())} (stable10b {sum(base['float'].values())}), under its floor "
                 f"{sum(r['under'].values())} (stable10b {sum(base['under'].values())}), by season against stable10b + {plus}"
                 + (f": {'; '.join(bad)}" if bad else "") + f"; mean {r['treasury']:.0f}p")
        if run.get("floors"):
            ref = load(self.dir, run["floors_off"]) if run.get("floors_off") else None
            if ref is None:
                self.add(name, y, "T6 shops", None, "no floors-off run at this position")
            else:
                rt = mean(d.get("tills_short") for d in span(ref, a, b))
                lim = self.sc["T6"]["floors_times"] * rt
                self.add(name, y, "T6 shops", r["tills"] <= lim and r["unpaid"] <= self.sc["T6"]["unpaid_times"] * base["unpaid"],
                         f"tills short {r['tills']:.1f} (at most {self.sc['T6']['floors_times']:g} x {rt:.1f} = {lim:.1f}, the floors-off run); "
                         f"support unpaid {r['unpaid']:.0f} (at most {self.sc['T6']['unpaid_times']:g} x stable10b's {base['unpaid']:.0f})")
        else:
            tt = m["tills"][str(y)] if isinstance(m["tills"], dict) else m["tills"]
            lim = self.sc["T6"]["times"] * tt
            self.add(name, y, "T6 shops", r["tills"] <= lim and r["unpaid"] <= self.sc["T6"]["unpaid_times"] * base["unpaid"],
                     f"tills short {r['tills']:.1f} (at most {self.sc['T6']['times']:g} x {tt:.1f} = {lim:.1f}); support unpaid "
                     f"{r['unpaid']:.0f} (at most {self.sc['T6']['unpaid_times']:g} x stable10b's {base['unpaid']:.0f})")
        wb = base["wharf"]
        wok = r["wharf"] is None or wb is None or abs(r["wharf"] - wb) <= self.sc["T7"]["wharf_within"]
        self.add(name, y, "T7 books", r["conserved"] and r["posts"] and wok,
                 f"conserved {r['conserved']}, estates' posts paid {r['posts']}, Wharf net {r['wharf'] if r['wharf'] is None else round(r['wharf'])}p a day "
                 f"(stable10b {wb if wb is None else round(wb)}p, within {self.sc['T7']['wharf_within']})")

    def identical(self, name, days, test):
        n = min(len(days), len(self.base))
        for i in range(n):
            d, b = days[i], self.base[i]
            for k, v in b.items():
                if d.get(k) != v:
                    self.add(name, 0, test, False, f"day {b['day']}: `{k}` {d.get(k)!r} against stable10b's {v!r}")
                    return
        self.add(name, 0, test, n > 0, f"{n} daily lines the same as stable10b's on every field it has")

    def t11(self, name, ys, y, run):
        sc = self.sc["T11"]
        h = mean(d.get("hungry_mean", 0) for d in ys)
        after = mean(d.get("households_short_after", 0) for d in ys)
        wharf = mean(sub(d, "never_hungry", "wharf_pennies", default=0) for d in ys)
        # (The floors' skim: the share-out's own is not the floors' money.)
        skim = sum((d.get("skim_for") or {}).get("never hungry", 0) + (d.get("skim_for") or {}).get("never broke", 0) for d in ys)
        idle = sum(d.get("skim_idle", 0) for d in ys) if any("skim_idle" in d for d in ys) else None
        idle_ok = idle is None or skim == 0 or idle / skim >= sc["idle_least"]
        idle_txt = "idle share n/a (not logged)" if idle is None else (f"idle {100 * idle / skim:.0f}% of the skim (at least "
                                                                        f"{100 * sc['idle_least']:.0f}%)" if skim else "no skim")
        self.add(name, y, "T11 floors", h <= sc["hunger_most"] and after <= sc["short_after_most"] and wharf <= sc["wharf_pennies_most"] and idle_ok,
                 f"H {h:.4f} (at most {sc['hunger_most']}); short after relief {after:.2f} (at most {sc['short_after_most']}); "
                 f"Never Hungry's Wharf pennies {wharf:.2f} a day (at most {sc['wharf_pennies_most']}); {idle_txt}")

    def t12(self, name, days, run):
        legs = run["legs"]
        sc = self.sc["T12"]
        for i, (day, x) in enumerate(legs):
            end = legs[i + 1][0] if i + 1 < len(legs) else days[-1]["day"] + 1
            for key, spec in self.measures(x).items():
                series = dict(zip((d["day"] for d in days), self.band_series(days, spec)))
                v0 = series.get(day - 1)
                if v0 is None:
                    continue
                gap = spec["target"] - v0
                half = ninety = None
                over = 0.0
                for dd in range(day, end):
                    v = series.get(dd)
                    if v is None:
                        continue
                    gone = (v - v0) / gap if gap else 1
                    if half is None and gone >= .5:
                        half = dd - day
                    if ninety is None and gone >= .9:
                        ninety = dd - day
                    over = max(over, (v - spec["target"]) * (1 if gap > 0 else -1))
                ok = half is not None and half <= sc["half_days"] and ninety is not None and ninety <= sc["ninety_days"] and over <= spec["band"]
                self.add(name, 0, f"T12 sweep leg {i + 1} {key}", ok,
                         f"day {day} to {x:g}: {v0:.3f} -> {spec['target']:g}; half in {half} days (at most {sc['half_days']}), 90% in "
                         f"{ninety} (at most {sc['ninety_days']}); overshoot {over:.3f} (at most one band, {spec['band']:g})")

    def t13(self, name, days, run, years):
        if len(years) < 3:
            return
        for key, spec in self.measures(run["x"]).items():
            ym = {y: mean(d.get(spec["field"]) for d in span(days, a, b)) for y, a, b in years}
            d12, d23 = abs(ym[2] - ym[1]), abs(ym[3] - ym[2])
            # (The dry run: today's centre moves 0.000 then 0.005 in G, noise; so a change under half a band passes as
            # no drift whatever year 1 to 2 did.)
            small = self.sc["T13"]["noise_bands"] * spec["band"]
            self.add(name, 0, f"T13 no drift {key}", d23 < spec["band"] and (d23 <= d12 + 1e-12 or d23 < small),
                     f"year means {ym[1]:.3f}, {ym[2]:.3f}, {ym[3]:.3f}: year 2 to 3 {d23:.3f} (under one band, {spec['band']:g}, "
                     f"and no more than year 1 to 2, {d12:.3f}, or under {small:g})")

    # -- a run ---------------------------------------------------------------------------------------------------
    def judge(self, run):
        name = run["name"]
        days = load(self.dir, name)
        if run.get("off"):
            self.identical(name, days, "T9 steering off")
            return
        years = windows(days, self.sc)
        if run.get("legs"):
            self.t12(name, days, run)
            return
        if run.get("rung", "full") in ("full", "F2"):
            self.t1_t2(name, days, run, years)
        for y, a, b in years:
            ys = span(days, a, b)
            if not ys:
                continue
            if run.get("rung", "full") in ("full", "F2"):
                self.t3_t8(name, ys, y)
            self.t4_t7(name, days, ys, y, a, b, run)
            if run.get("floors"):
                self.t11(name, ys, y, run)
        if run["x"] == 0 and not run.get("floors") and run.get("rung", "full") in ("full", "F2"):
            self.identical(name, days, "T10 centre")
        if run.get("rung", "full") in ("full", "F2"):
            self.t13(name, days, run, years)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("batch")
    ap.add_argument("--scorecard", default="artifacts/economy/r2/steer-scorecard.json")
    ap.add_argument("--out")
    ap.add_argument("--years", type=int, default=0, help="judge only the first N years (0: all)")
    a = ap.parse_args()
    with open(path(a.batch)) as f:
        batch = json.load(f)
    with open(path(a.scorecard)) as f:
        sc = json.load(f)
    j = Judge(batch, sc)
    for run in batch["runs"]:
        j.judge(run)
    rows = [r for r in j.rows if not a.years or r[1] <= a.years]
    fails = [r for r in rows if r[3] == "FAIL"]
    out = [f"# Acceptance: {os.path.basename(a.batch)}\n",
           f"Scorecard `{a.scorecard}`, targets `{batch['targets']}`, baseline `{j.base_name}`. Year 1 is judged on days "
           f"{sc['window'][0]}-{sc['window'][1]}, later years whole. {len(rows) - len(fails)} of {len(rows)} pass.\n",
           "| run | year | test | result | detail |", "|---|---|---|---|---|"]
    for run, y, test, state, detail in rows:
        out.append(f"| {run} | {y or '-'} | {test} | {state} | {detail} |")
        print(f"{state:4}  {run:28} y{y or '-'}  {test:28} {detail}")
    if a.out:
        with open(a.out, "w") as f:
            f.write("\n".join(out) + "\n")
    print(f"\n{'FAIL' if fails else 'PASS'}: {len(rows) - len(fails)} of {len(rows)} tests pass")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
