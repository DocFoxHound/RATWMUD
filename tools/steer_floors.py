#!/usr/bin/env python3
"""The safety supervisor's treasury floors (Docs/Design/75-the-economy-controller.md, 3.6; Phase 4): for each mark, the
floors-off calibration run's seasonal treasury profile (its 28-day mean, a 15-day centred mean, days 120 on) less 2
sigma, sigma at least the scorecard's `T5.sigma_least` (the centre's own noise, stable10b's 191p), never under 600p, as
steer_accept's T5 judges it; written as 13 months of 28 days (the year's last day the 13th's), each the mean of its
days' floors, into the steering table's `treasuryFloors` (a mark with no run: null, read halfway between its neighbours).

    python3 tools/steer_floors.py --runs -1=steer-cal-L50 0=steer-cal-C 1=steer-cal-R33 [--data Data/Items/city_loop.json]
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import steer_accept  # noqa: E402

MARKS = ["-1", "-0.5", "0", "0.5", "1"]
MONTHS = 13


def floors(run, sc, dir_):
    fl, sigma = steer_accept.treasury_floor(steer_accept.load(dir_, run), sc)
    out = []
    for m in range(MONTHS):
        days = range(m * 28, 365 if m == MONTHS - 1 else (m + 1) * 28)
        vals = [fl[d] for d in days if d in fl]
        out.append(round(sum(vals) / len(vals)) if vals else sc["T5"]["floor_least"])
    return out, sigma


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--runs", nargs="+", required=True, help="MARK=RUN")
    ap.add_argument("--dir", default="artifacts/economy/r2")
    ap.add_argument("--scorecard", default="artifacts/economy/r2/steer-scorecard.json")
    ap.add_argument("--data", help="city_loop.json to write the floors into (else printed)")
    a = ap.parse_args()
    with open(steer_accept.path(a.scorecard)) as fh:
        sc = json.load(fh)
    dir_ = steer_accept.path(a.dir)
    rows = [None] * 5
    for spec in a.runs:
        mark, run = spec.split("=", 1)
        rows[MARKS.index(f"{float(mark):g}")], sigma = floors(run, sc, dir_)
        print(f"{mark:>4} {run}: sigma {sigma:.0f}p, floors {rows[MARKS.index(f'{float(mark):g}')]}")
    if a.data:
        p = steer_accept.path(a.data)
        with open(p) as fh:
            text = fh.read()
        data = json.loads(text)
        data["steering"]["treasuryFloors"] = rows
        with open(p, "w") as fh:
            json.dump(data, fh, indent=2, ensure_ascii=False)
            fh.write("\n")
    else:
        print(json.dumps(rows))


if __name__ == "__main__":
    main()
