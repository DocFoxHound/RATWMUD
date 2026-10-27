"""The model's money by kind of holder, week by week (Docs/Design/64-economy-migration.md, Phase 13, round 10): the
targets econ_watch's share sentinel and report.py hold a run's shares against. Written to model_shares.csv beside this
file (checked in), as compare.py groups the holders: homes, firms, Houses, church, town. Each week is the mean of a few
seeds' runs, so one seed's noise isn't taken for the model.

    python3 tools/econ_model/model_shares.py [--days 1092] [--seeds 4] [--start normal] [--out model_shares.csv]

A run longer than the file holds its last week's shares (the model is settled by its third year).
"""
import argparse
import pathlib

from compare import model_rows

GROUPS = ("homes", "firms", "Houses", "church", "town")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--days", type=int, default=1092)
    ap.add_argument("--seeds", type=int, default=4)
    ap.add_argument("--start", default="normal")
    ap.add_argument("--out", default=str(pathlib.Path(__file__).with_name("model_shares.csv")))
    a = ap.parse_args()
    runs = [{r["day"]: r for r in model_rows(a.days, a.start, seed)} for seed in range(1, a.seeds + 1)]
    with open(a.out, "w") as f:
        f.write("day," + ",".join(GROUPS) + "\n")
        for day in range(0, a.days + 1, 7):
            rows = [run[day] for run in runs if day in run]
            if rows:
                f.write(f"{day}," + ",".join(f"{sum(r['$' + g] for r in rows) / len(rows):.4f}" for g in GROUPS) + "\n")
    print(f"{a.out}: {a.days} days, {a.seeds} seeds, start {a.start}")


if __name__ == "__main__":
    main()
