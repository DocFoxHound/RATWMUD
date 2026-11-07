#!/usr/bin/env python3
"""The economy controller's calibration (Docs/Design/75-the-economy-controller.md, 5.2 and D2): reads the calibration
runs, picks each end's strength by the selection rule, re-scales the half marks by the measured curve, and writes the
table, the targets at each mark and a report.

    python3 tools/steer_calibrate.py SPEC.json

The selection rule (5.2): each end takes the strongest strength that passes T4-T7 open-loop. The runs are judged by
steer_accept's own tests against the targets agreed before any calibration run (`select_targets`: steer-targets-t1.json,
the t1 run at each end for the tills short, the table's hungerMost and shortMost), since a mark's own calibrated
targets are what this script writes and cannot judge the runs they come from. T5's floor is the run's own seasonal
profile less 2 sigma, never under 600p (3.6: the floor from the mark's own run).

D2: at the right, if the chosen strength adds under 3 points of S over F1 (the right without charters), the right
ships F1 and the charters stay off; at the left, if no strength passes (not even the weakest), the left ships F3.

The half marks (5.2, "re-scaled to the chosen strength by the measured curve"): the lever's half-mark value is set so
the half mark's measure sits halfway between the centre's and the chosen end's. The curve: the centre run (the lever 0)
to the half-mark run (the middle strength's half, at x = +-0.5), straight, and on from the half-mark run with the end
runs' slope.

The targets at each mark (3.5): the measure (G, `m_g_day`, left of the centre; S, `m_upper_share`, right of it; both at
the centre) as the mean of days 120-365, its band two standard deviations of its 28-day mean over those days (at least
`band_least`), the tills short there, and `floor_from`, the run whose seasonal treasury profile gives the mark's floor.

The spec:

    {"dir": "artifacts/economy/r2", "select_targets": "artifacts/economy/r2/steer-targets-t1.json",
     "f1": "steer-b0-right", "centre": "steer-cal-C",
     "ends": {"-1": {"lever": "levelling", "measure": "G", "runs": [["steer-cal-L30", 30], ...]},
              "1": {"lever": "charters", "measure": "S", "runs": [["steer-cal-R33", 33], ...]}},
     "halves": {"-0.5": ["steer-cal-Hm", 20], "0.5": ["steer-cal-Hp", 33]},
     "perturb": {"run": "steer-cal-P", "against": "steer-cal-L40", "lever": "spendAbove", "from": 25, "to": 30},
     "table_out": "...json", "targets_out": "...json", "report": "...md", "data": "Data/Items/city_loop.json"}
"""
import argparse
import collections
import gzip
import json
import os
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import steer_accept  # noqa: E402
import steer_measures  # noqa: E402

FIELDS = {"G": "m_g_day", "S": "m_upper_share"}
BAND_LEAST = {"G": 0.01, "S": 1.0}
SUMM = {"G": "G_day", "S": "S"}          # (steer_measures.summary's names)
WINDOW = (120, 365)
D2_POINTS = 3.0


def f(v, p=2):
    return "-" if v is None or v != v else f"{v:,.{p}f}"


def mean(xs):
    xs = [x for x in xs if x is not None and x == x]
    return statistics.fmean(xs) if xs else float("nan")


def spec_band(days, key):
    field = FIELDS[key]
    tr = steer_accept.trailing([d.get(field) for d in days], 28)
    vals = [t for d, t in zip(days, tr) if WINDOW[0] <= d["day"] <= WINDOW[1] and t is not None]
    target = mean(d.get(field) for d in days if WINDOW[0] <= d["day"] <= WINDOW[1])
    band = max(BAND_LEAST[key], 2 * statistics.pstdev(vals)) if len(vals) > 1 else BAND_LEAST[key]
    return {"field": field, "target": round(target, 4), "band": round(band, 4)}


def chartered_cash(dir_, run):
    """S without the chartered tills' own cash: each day, the floats of the tills chartered then (the loop state's list,
    its latest snapshot on or before the day) as percent of all money."""
    loop = os.path.join(dir_, run + "_loop.jsonl")
    full = os.path.join(dir_, run + ".full.jsonl.gz")
    if not (os.path.exists(loop) and os.path.exists(full)):
        return {}
    snaps = []
    with open(loop) as fh:
        for line in fh:
            d = json.loads(line)
            st = d["loop"].get("steer") or {}
            snaps.append((d["day"], {c["till"] for c in st.get("charters", [])}))
    snaps.sort()
    out = {}
    try:
        for _meta, names, d in steer_accept_full(full):
            day = d["day"]
            # (The day's own list: the snapshot taken that night, else the last before it.)
            held = set()
            for sd, tills in snaps:
                if sd <= day:
                    held = tills
            if not held:
                out[day] = 0
                continue
            out[day] = sum(row[2] for row in d.get("till", []) if names[row[0]] in held)
    except (EOFError, OSError):
        pass
    return out


def steer_accept_full(path):
    names = []
    with gzip.open(path, "rt") as fh:
        for line in fh:
            if not line.strip():
                continue
            d = json.loads(line)
            if "meta" in d:
                continue
            names.extend(d.get("names", []))
            yield None, names, d


def select(judge, end, runs):
    """Each strength's T4-T7 rows (year 1) and whether it passes; the strongest passing one."""
    out = []
    for name, strength in runs:
        days = steer_accept.load(judge.dir, name)
        ys = steer_accept.span(days, *WINDOW)
        before = len(judge.rows)
        # (T5's floor from the run's own seasonal profile: the run is this mark's calibration run, whose profile gives
        # the mark's floor (3.6), so the test catches its real dips and its 28-day mean under 600p; a floor from another
        # run's profile counts a treasury held flat at its cap a few pennies under that run's as "under its floor".)
        key = f"{float(end):g}"
        saved = judge.targets["marks"][key].get("floor_from")
        judge.targets["marks"][key]["floor_from"] = name
        judge.t4_t7(name, days, ys, 1, WINDOW[0], WINDOW[1], {"x": float(end), "rung": "full"})
        judge.targets["marks"][key]["floor_from"] = saved
        rows = judge.rows[before:]
        out.append({"run": name, "strength": strength, "rows": rows, "pass": all(r[3] != "FAIL" for r in rows)})
    passing = [o for o in out if o["pass"]]
    return out, (max(passing, key=lambda o: o["strength"]) if passing else None)


def against_ref(judge, end, runs, ref):
    """For the check-in, not the rule: each strength on a fallback rung's bar (no worse than `ref` in T4-T7)."""
    out = []
    for name, strength in runs:
        days = steer_accept.load(judge.dir, name)
        ys = steer_accept.span(days, *WINDOW)
        before = len(judge.rows)
        judge.t4_t7(name, days, ys, 1, WINDOW[0], WINDOW[1], {"x": float(end), "rung": "F1", "ref": ref})
        rows = judge.rows[before:]
        out.append({"run": name, "strength": strength, "rows": rows, "pass": all(r[3] != "FAIL" for r in rows)})
    return out


def slope(points):
    """Least-squares slope of (lever, measure)."""
    if len(points) < 2:
        return None
    xs, ys = zip(*points)
    mx, my = statistics.fmean(xs), statistics.fmean(ys)
    den = sum((x - mx) ** 2 for x in xs)
    return sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / den if den else None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("spec")
    ap.add_argument("--scorecard", default="artifacts/economy/r2/steer-scorecard.json")
    a = ap.parse_args()
    with open(steer_accept.path(a.spec)) as fh:
        spec = json.load(fh)
    with open(steer_accept.path(a.scorecard)) as fh:
        sc = json.load(fh)
    dir_ = steer_accept.path(spec.get("dir", "artifacts/economy/r2"))
    floor_note = "T5's floor: the run's own seasonal profile less 2 sigma (the scorecard as written)."
    if spec.get("sigma_least_from"):
        # (The floor reading proposed at the check-in: a dip is real when it is as deep as the centre's own noise.)
        _, sig = steer_accept.treasury_floor(steer_accept.load(dir_, spec["sigma_least_from"]), sc)
        sc = dict(sc, T5=dict(sc["T5"], sigma_least=round(sig, 1)))
        floor_note = (f"T5's floor: the run's own seasonal profile less 2 sigma, sigma at least {spec['sigma_least_from']}'s "
                      f"({sig:.0f}p), so the floor is at least {2 * sig:.0f}p under the profile (a reading proposed at the "
                      f"check-in, not the scorecard as written).")
    judge = steer_accept.Judge({"dir": spec.get("dir", "artifacts/economy/r2"), "targets": spec["select_targets"],
                                "baseline": spec.get("baseline", "stable10b")}, sc)
    with open(steer_accept.path(spec["data"])) as fh:
        data = json.load(fh)["steering"]
    marks = [-1, -0.5, 0, 0.5, 1]

    every = [r for e in spec["ends"].values() for r, _ in e["runs"]] + [spec["centre"]] + [h[0] for h in spec["halves"].values()]
    if spec.get("perturb"):
        every.append(spec["perturb"]["run"])
    every.append(spec["f1"])
    summ = {r: steer_measures.summary(dir_, r, *WINDOW) for r in dict.fromkeys(every)}
    for r in summ:
        days = steer_accept.load(dir_, r)
        ys = steer_accept.span(days, *WINDOW)
        cash = chartered_cash(dir_, r)
        summ[r]["S_ex"] = mean(d.get("m_upper_share") - 100 * cash.get(d["day"], 0) / d["money"] for d in ys
                               if d.get("m_upper_share") is not None and d.get("money")) if cash else summ[r]["S"]
        summ[r]["charters"] = mean(d.get("charters") for d in ys)
        summ[r]["charter_profit"] = mean(d.get("charter_profit") for d in ys)
        summ[r]["tithe"] = mean(d.get("tithe") for d in ys)
        summ[r]["dividend"] = mean(d.get("dividend") for d in ys)
        summ[r]["ground_rent_treasury"] = mean(d.get("ground_rent_treasury") for d in ys)
        est = lambda d: sum(e.get("purse", 0) for e in (d.get("estates") or {}).values())  # noqa: E731
        e0 = [est(d) for d in days if 120 <= d["day"] < 148]
        e1 = [est(d) for d in days if 338 <= d["day"] <= 365]
        summ[r]["estates_from"], summ[r]["estates_to"] = mean(e0), mean(e1)
        summ[r]["toll"] = mean(d.get("toll_now") for d in ys)

    # -- the selection rule -------------------------------------------------------------------------------------------
    chosen, trials = {}, {}
    for end, e in spec["ends"].items():
        trials[end], chosen[end] = select(judge, end, e["runs"])

    f1_bar = against_ref(judge, "1", spec["ends"]["1"]["runs"], spec["f1"])

    # -- D2 ------------------------------------------------------------------------------------------------------------
    rung = {}
    left, right = chosen.get("-1"), chosen.get("1")
    rung["-1"] = "full" if left else "F3"
    f1_s = summ[spec["f1"]]["S"]
    if not right:
        rung["1"] = "F1"
        d2_right = "no strength passes T4-T7, so the right takes its fallback rung (5.2) whatever the charters add; what " \
                   "they add: " + "; ".join(
                       f"{r} {summ[r]['S']:.1f}% ({summ[r]['S'] - f1_s:+.1f} points over F1's {f1_s:.1f}%, "
                       f"{summ[r]['S_ex'] - f1_s:+.1f} without the chartered tills' own cash)" for r, _ in spec["ends"]["1"]["runs"])
    else:
        gain = summ[right["run"]]["S"] - f1_s
        gain_ex = summ[right["run"]]["S_ex"] - f1_s
        rung["1"] = "full" if gain >= D2_POINTS else "F1"
        d2_right = (f"{right['run']} holds S {summ[right['run']]['S']:.1f}% against F1's {f1_s:.1f}% ({spec['f1']}): "
                    f"{gain:+.1f} points ({gain_ex:+.1f} without the chartered tills' own cash)")

    # -- the table: the ends and the half marks --------------------------------------------------------------------
    levers = {}
    curve_notes = []
    for end, e in spec["ends"].items():
        lever, key = e["lever"], e["measure"]
        row = list(data["levers"][lever])
        side = 0 if float(end) < 0 else 4
        half = 1 if float(end) < 0 else 3
        c = chosen[end]
        if not c or rung[end] != "full":
            row[side] = 0
            row[half] = 0
            levers[lever] = row
            curve_notes.append(f"{lever}: {end} ships {rung[end]}; the lever is 0 at both its marks")
            continue
        row[side] = c["strength"]
        pts = [(s, summ[r][SUMM[key]]) for r, s in e["runs"]]
        k = slope(pts)
        hname, hval = spec["halves"][f"{float(end) / 2:g}"]
        m_c, m_end, m_h = summ[spec["centre"]][SUMM[key]], summ[c["run"]][SUMM[key]], summ[hname][SUMM[key]]
        want = (m_c + m_end) / 2
        # (The measured curve at the half mark: from the centre (the lever 0, the measure m_c) to the half-mark run (the
        # lever hval, m_h), then on from it with the end runs' slope. A straight line from the half-mark run alone would
        # carry the ends' shallow slope back to 0, where the curve is steepest.)
        if (m_c - want) * (want - m_h) >= 0 and m_h != m_c:
            v = hval * (want - m_c) / (m_h - m_c)
        elif k:
            v = hval + (want - m_h) / k
        else:
            v = c["strength"] / 2
        v = max(0.0, min(float(c["strength"]), v))
        row[half] = round(v, 1)
        levers[lever] = row
        curve_notes.append(f"{lever}: {key} at the centre {m_c:.3f}, at the end ({c['run']}) {m_end:.3f}, so the half mark aims at "
                           f"{want:.3f}; {hname} ({lever} {hval:g} at x {float(end) / 2:g}) held {m_h:.3f}; the end runs' slope "
                           f"{f(k, 5)} a point; on the curve from the centre through {hname}, {lever} {row[half]:g} at the half mark (was null: "
                           f"{c['strength'] / 2:g})")
    table = {"about": "Doc 75, Phase 3: the calibrated table, written by tools/steer_calibrate.py from the calibration runs "
                      "(the levers it sets; every other lever as in city_loop.json).", "steering": {"levers": levers}}

    # -- the targets at each mark -------------------------------------------------------------------------------------
    tdata = data["targets"]
    targets = {"about": "Doc 75, Phase 3: each mark's targets from the calibration runs (tools/steer_calibrate.py): G "
                        "(m_g_day) left of the centre, S (m_upper_share) right of it, both at the centre, as the mean of days "
                        "120-365 of the run at that mark, the band 2 standard deviations of its 28-day mean there (at least "
                        "0.01 in G, 1 point in S); hungerMost and shortMost the table's; tills the run's tills short; "
                        "floor_from the run whose seasonal treasury profile gives the mark's floor. A half mark's target is "
                        "halfway between the centre's and the end's; its band, tills and floor are the half-mark run's.",
               "marks": {}}
    runs_at = {"0": spec["centre"]}
    for end in spec["ends"]:
        c = chosen[end]
        runs_at[f"{float(end):g}"] = c["run"] if c and rung[end] == "full" else None
        runs_at[f"{float(end) / 2:g}"] = spec["halves"][f"{float(end) / 2:g}"][0] if c and rung[end] == "full" else None
    for i, x in enumerate(marks):
        key_x = f"{x:g}"
        run = runs_at.get(key_x)
        m = {"hungerMost": tdata["hungerMost"][i], "shortMost": tdata["shortMost"][i]}
        if run is None:
            m["note"] = "this side ships its fallback rung; its targets are the rung's reference run's"
            targets["marks"][key_x] = m
            continue
        days = steer_accept.load(dir_, run)
        keys = ["G", "S"] if x == 0 else (["G"] if x < 0 else ["S"])
        for key in keys:
            m[key] = spec_band(days, key)
        if x in (-0.5, 0.5):
            end = "-1" if x < 0 else "1"
            c = chosen[end]
            key = "G" if x < 0 else "S"
            if c:
                m[key]["target"] = round((summ[spec["centre"]][SUMM[key]] + summ[c["run"]][SUMM[key]]) / 2, 4)
        m["tills"] = {"1": round(summ[run]["tills"], 2)}
        m["floor_from"] = run
        m["treasury"] = round(summ[run]["treasury"], 0)
        targets["marks"][key_x] = m

    for p, obj in ((spec["table_out"], table), (spec["targets_out"], targets)):
        with open(steer_accept.path(p) if os.path.exists(steer_accept.path(p)) else p, "w") as fh:
            json.dump(obj, fh, indent=1)
            fh.write("\n")

    # -- the report ----------------------------------------------------------------------------------------------------
    o = ["# The calibration (doc 75, 5.2 and D2)\n",
         f"Written by `tools/steer_calibrate.py {a.spec}`. One-year runs, open-loop (the table only, no supervisor), the "
         f"toggles off, measured on days {WINDOW[0]}-{WINDOW[1]}. The selection is judged by steer_accept's T4-T7 against "
         f"`{spec['select_targets']}` (the t1 run at each end), baseline stable10b. {floor_note}\n",
         "## The selection rule\n", "| end | run | strength | T4 | T5 | T6 | T7 | passes |", "|---|---|---|---|---|---|---|---|"]
    for end, ts in trials.items():
        for t in ts:
            st = {r[2].split()[0]: r[3] for r in t["rows"]}
            o.append(f"| {end} | {t['run']} | {t['strength']} | {st.get('T4')} | {st.get('T5')} | {st.get('T6')} | {st.get('T7')} | "
                     f"{'yes' if t['pass'] else 'no'} |")
    o.append("")
    for end, ts in trials.items():
        c = chosen[end]
        o.append(f"- **{end}:** {('chosen ' + c['run'] + ', ' + spec['ends'][end]['lever'] + ' ' + str(c['strength'])) if c else 'no strength passes'}; "
                 f"rung **{rung[end]}**.")
    o += ["", "Every test's detail:\n", "| run | test | result | detail |", "|---|---|---|---|"]
    for ts in trials.values():
        for t in ts:
            for r in t["rows"]:
                o.append(f"| {r[0]} | {r[2]} | {r[3]} | {r[4]} |")
    o += ["", f"For the check-in, not the rule: the right's strengths on F1's own bar (no worse than {spec['f1']}, the right "
          "without charters, in T4-T7):\n", "| run | test | result | detail |", "|---|---|---|---|"]
    for t in f1_bar:
        for r in t["rows"]:
            o.append(f"| {r[0]} | {r[2]} | {r[3]} | {r[4]} |")
    o += ["", "## D2\n", f"- The right: {d2_right}. The bar is {D2_POINTS:g} points: rung **{rung['1']}**.",
          f"- The left: {'a strength passes' if left else 'not even the weakest strength passes'}: rung **{rung['-1']}**.", "",
          "## The measures (days 120-365, means a day)\n",
          "G: the Gini of cash a head over all households, weighted by heads, the day's mean of before and after the tithe. "
          "S: the upper class, the estates and the Houses' tills (chartered ones among them), percent of all money; "
          "S without the chartered tills' own cash takes their floats out (the relabelling, not money moved). "
          "Estates: the five estates' funds, the mean of days 120-147 and of days 338-365.\n",
          "| run | G (day) | G before | G after | Gini earners | S % | S without chartered cash | charters | H/h | K | treasury mean (least; 28-day least) | float days | toll | tills short | wage support | support unpaid | tithe moved | charter profit | dividend | estates' funds | conserved |",
          "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for r, s in summ.items():
        o.append(f"| {r} | {f(s['G_day'],3)} | {f(s['G_before'],3)} | {f(s['G_after'],3)} | {f(s['G_earners'],3)} | {f(s['S'],1)} | {f(s['S_ex'],1)} | "
                 f"{f(s['charters'],1)} | {f(s['H'],3)} | {f(s['K'])} | {f(s['treasury'],0)} ({f(s['treasury_least'],0)}; {f(s['tr28_least'],0)}) | "
                 f"{s['float_days']} | {f(s['toll'],1)} | {f(s['tills'],1)} | {f(s['support'],0)} | {f(s['unpaid'],0)} | {f(s['tithe'],0)} | "
                 f"{f(s['charter_profit'],0)} | {f(s['dividend'],0)} | {f(s['estates_from'],0)} -> {f(s['estates_to'],0)} | {s['conserved']} |")
    o += ["", "Income by class (the full log), the luxuries and cheese (its rules untouched):\n",
          "| run | upper % | middle % | lower % | Houses' luxuries p/day | cheese p a piece | cheese pieces/day |", "|---|---|---|---|---|---|---|"]
    for r, s in summ.items():
        fu = s["full"] or {}
        inc = fu.get("income", {})
        o.append(f"| {r} | {f(inc.get('upper'),1)} | {f(inc.get('middle'),1)} | {f(inc.get('lower'),1)} | {f(s['lux_houses'],0)} | "
                 f"{f(fu.get('cheese_price'),2)} | {f(fu.get('cheese_sold'),1)} |")
    o += ["", "## The curves and the half marks\n"] + [f"- {n}" for n in curve_notes]
    if spec.get("perturb"):
        p = spec["perturb"]
        sp, sa = summ[p["run"]], summ[p["against"]]
        dt = sp["tills"] - sa["tills"]
        o.append(f"- The left's coupling ({p['run']} against {p['against']}: {p['lever']} {p['from']}p -> {p['to']}p at -1): tills short "
                 f"{sa['tills']:.1f} -> {sp['tills']:.1f} ({dt / (p['to'] - p['from']):+.2f} a penny); G {sa['G_day']:.3f} -> {sp['G_day']:.3f}; "
                 f"H {sa['H']:.3f} -> {sp['H']:.3f}; treasury {sa['treasury']:.0f} -> {sp['treasury']:.0f}p.")
    o += ["", "## The table written\n", f"`{spec['table_out']}`:\n", "```json", json.dumps(levers, indent=1), "```", "",
          "## The targets at each mark\n", f"`{spec['targets_out']}`:\n",
          "| mark | measure | target | band | tills short | treasury | floor from |", "|---|---|---|---|---|---|---|"]
    for x, m in targets["marks"].items():
        for key in ("G", "S"):
            if key in m:
                o.append(f"| {x} | {key} ({m[key]['field']}) | {m[key]['target']} | {m[key]['band']} | {m['tills']['1']} | {m['treasury']:.0f} | {m['floor_from']} |")
        if "note" in m:
            o.append(f"| {x} | - | - | - | - | - | {m['note']} |")
    o += ["", "The treasury floors (T5; the floor run's 28-day mean, its seasonal profile less 2 sigma, never under 600p):\n",
          "| mark | floor run | sigma | floor least | floor most |", "|---|---|---|---|---|"]
    for x, m in targets["marks"].items():
        if m.get("floor_from"):
            fl, sig = steer_accept.treasury_floor(steer_accept.load(dir_, m["floor_from"]), sc)
            o.append(f"| {x} | {m['floor_from']} | {sig:.0f} | {min(fl.values()):.0f} | {max(fl.values()):.0f} |")
    with open(spec["report"], "w") as fh:
        fh.write("\n".join(o) + "\n")
    print("\n".join(o))


if __name__ == "__main__":
    main()
