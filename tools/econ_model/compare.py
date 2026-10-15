"""The game beside the model (Docs/Design/64-economy-migration.md, Phase 0): reads an econ_watch run and runs (or
reads) the model for as many days, and prints both side by side, week by week, in measures that don't depend on how
much money either holds: shares of money by kind of holder, the household Gini and the poorer half's share, prices in
a labourer's day-wages (LD), people, households, idle, hunger, the money minted.

    python3 tools/econ_model/compare.py ECON_WATCH_OUT [--every 7] [--model-days N] [--start normal] [--seed 1]

ECON_WATCH_OUT is the directory econ_watch wrote (its days.jsonl, with the "model" measures). The model is run for the
same number of days unless --model-days says otherwise. Where a measure isn't comparable yet (the game's skill scale
isn't the model's, so its "masters" are not the model's) the line says so.
"""
import argparse
import json
import pathlib

import model

# The game's kinds of holder (econ_watch's `holder()`) grouped as the model's.
GROUPS = {
    "homes": lambda k: k in ("merchant", "guard", "producer", "paid civilian", "unpaid civilian", "apprentice", "child",
                             "retired", "resident", "player") or k.startswith("resident"),
    "firms": lambda k: k in ("shop till", "house till", "farm till"),
    "Houses": lambda k: k == "great house",
    "church": lambda k: k == "town church",
    "town": lambda k: k in ("town treasury", "capital treasury") or (k.startswith("town ") and k != "town church"),
}


def game_rows(out):
    rows = []
    for line in open(pathlib.Path(out) / "days.jsonl"):
        d = json.loads(line)
        if d.get("when") not in ("day end", "start") or "model" not in d:
            continue
        held = {k: v[0] for k, v in d["holders"].items()}
        total = sum(v for v in held.values() if v > 0) or 1
        shares = {g: sum(v for k, v in held.items() if test(k)) / total for g, test in GROUPS.items()}
        shares["other"] = 1 - sum(shares.values())
        m = d["model"]
        rows.append(dict(day=d["day"], people=m["people"], homes=m["households"], gini=m["household_gini"],
                         poor=m["poorer_half"], idle=m["idle"], hungry=d["hungry"], wage=m["wage"],
                         bread=m["ld"].get("bread", 0), wheat=m["ld"].get("wheat", 0), sword=m["ld"].get("sword", 0),
                         rapier=m["ld"].get("rapier", 0), minted=d["minted"], **{"$" + g: s for g, s in shares.items()}))
    return rows


def model_rows(days, start, seed):
    w = model.World(start, seed)
    rows = []
    for _ in range(days + 1):
        w.step()
        r = w.log[-1]
        total = sum(r.get(f"money_{k}", 0) for k in ("household", "firm", "house", "church", "treasury")) or 1
        rows.append(dict(day=r["day"], people=r["people"], homes=r["homes"], gini=r["gini"], poor=r["poorer_half"],
                         idle=r["idle"], hungry=r["hungry"], wage=r["wage"], bread=r["bread_ld"],
                         wheat=r["wheat_ld"], sword=r["sword_ld"], rapier=r["rapier_ld"], minted=0,
                         **{"$homes": r.get("money_household", 0) / total, "$firms": r.get("money_firm", 0) / total,
                            "$Houses": r.get("money_house", 0) / total, "$church": r.get("money_church", 0) / total,
                            "$town": r.get("money_treasury", 0) / total, "$other": 0.0}))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--every", type=int, default=7)
    ap.add_argument("--model-days", type=int)
    ap.add_argument("--start", default="normal")
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()
    game = game_rows(a.out)
    if not game:
        raise SystemExit(f"no econ_watch days with the model's measures in {a.out}")
    days = a.model_days or game[-1]["day"]
    sim = {r["day"]: r for r in model_rows(days, a.start, a.seed)}
    cols = [("people", "{:6.0f}"), ("homes", "{:5.0f}"), ("gini", "{:5.2f}"), ("poor", "{:5.2f}"), ("idle", "{:4.0f}"),
            ("hungry", "{:5.0f}"), ("bread", "{:6.3f}"), ("wheat", "{:6.3f}"), ("sword", "{:5.2f}"),
            ("rapier", "{:6.1f}"), ("$homes", "{:5.2f}"), ("$firms", "{:5.2f}"), ("$Houses", "{:5.2f}"),
            ("$church", "{:5.2f}"), ("$town", "{:5.2f}"), ("$other", "{:5.2f}")]
    print(f"game: {a.out}; model: {a.start}, seed {a.seed}. Prices in a labourer's day-wages; $ = share of all money.")
    print("day   who   " + " ".join(f"{c:>{max(6, len(c))}}" for c, _ in cols))
    for g in game:
        if g["day"] % a.every and g is not game[-1]:
            continue
        for who, r in (("game", g), ("model", sim.get(g["day"]))):
            if r is None:
                continue
            print(f"{g['day']:4}  {who:5} " + " ".join(f.format(r[c]).rjust(max(6, len(c))) for c, f in cols))
    steps = [(g["day"], g["minted"] - p["minted"]) for p, g in zip(game, game[1:]) if g["minted"] != p["minted"]]
    print("\nminted, day by day (the game founds over its first days; after that it should be none, but the mint's): "
          + (", ".join(f"day {d} {m:+,}p" for d, m in steps) or "nothing") + ". Masters aren't "
          "compared until Phase 8 brings the game's skill scale to the model's; $other is bandits' hoards, contracts' "
          "escrow and home stores.")


if __name__ == "__main__":
    main()
