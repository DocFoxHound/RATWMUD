#!/usr/bin/env python3
"""A phase's scorecard (Docs/Design/84-living-conversations.md §15.8): every listener profile run by talk_sim over
Ridgemere 3, side by side, scored with the shuffled control and compared with the committed baseline.

  python3 tools/talk_card.py --fixtures DIR --out artifacts/talk/runs/NAME [--modes frozen,fixtures] [--profiles L1,...]
          [--world ridgemere3] [--build build-core] [--jobs 10] [--hours H] [--freeze-at DAY]
          [--baseline artifacts/talk/scorecard-baseline.json] [--bless] [--most-heard OUT.tsv] [--rescore]

Modes: `frozen` (the phase-end measure: the recorded world frozen at --freeze-at, noon of the fixture's first whole day
unless said, so every phase starts from the same moment) and `fixtures` (talk replayed over the whole recorded run).
Each profile is its own talk_sim process (--jobs at once); each writes OUT/MODE-PROFILE/ (lines, convs, residents, run,
and the shuffled control). Then OUT/scorecard-MODE.json and .md (tools/talk_score.py) and, with --baseline, the diff
against artifacts/talk/scorecard-baseline.json (frozen) or scorecard-baseline-fixtures.json: a broken gate exits 1.
--bless writes those baselines instead (Phase 0, or on purpose, saying why). --refreeze IDS --why TEXT takes only those
metrics' rows into the baselines from this card (a gate a phase closed, locked where it closed it; a metric whose
definition changed, measured again), keeping the rest. --rescore scores the runs already in OUT again (after a change to
tools/talk_score.py), running nothing.

A talk_sim that fails (any exit but 0) fails the card: its profile's old transcripts are removed before it runs, so a
failed run is never scored from what an earlier one left, and a profile asked for with no run.json is a problem in
itself (so is one the baseline has and the card hasn't). Nothing is blessed or refrozen from a card with a failed run.

--most-heard OUT.tsv: every placed resident (talk_check --world ... --residents --json), most heard first, over the
fixture runs (else the frozen ones): the order the mantles are written in (§14.3, within each batch).

DIR is a fixture talk_sim --tick --record wrote (Phase 0's: artifacts/talk/fixtures/p0-r3-25d). The database is read
only, as a scratch server reads it; RATW_DATABASE_URL is used when set.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import json
import math
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import talk_score  # noqa: E402

PROFILES = [f"L{i}" for i in range(1, 11)]


OUTPUTS = ("lines.jsonl", "convs.jsonl", "residents.jsonl", "run.json", "lines-shuffled.jsonl", "convs-shuffled.jsonl")


def freeze_day(fixtures: Path) -> float:
    try:
        m = json.loads((fixtures / "fixture.json").read_text())
    except (OSError, ValueError) as e:
        raise SystemExit(f"talk_card: no fixture at {fixtures} ({e})")
    cps = m.get("checkpoints", [])
    first_whole = math.floor(cps[1]["day"] if len(cps) > 1 else cps[0]["day"])
    return first_whole + .5


def run_one(build: Path, world: str, fixtures: Path, mode: str, profile: str, out: Path, hours: float | None,
            freeze_at: float) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    for name in OUTPUTS:                            # (Never scored from what an earlier run left: a failed run leaves none.)
        (out / name).unlink(missing_ok=True)
    cmd = [str(build / "talk_sim"), "--world", world, "--fixtures", str(fixtures), "--profile", profile,
           "--ablate", "shuffle", "--run", f"{mode}-{profile}", "--out", str(out)]
    if mode == "frozen":
        cmd += ["--freeze-at", f"{freeze_at}"]
    if hours:
        cmd += ["--hours", str(hours)]
    started = time.monotonic()
    p = subprocess.run(cmd, capture_output=True, text=True)
    (out / "talk_sim.log").write_text(p.stderr)
    return {"mode": mode, "profile": profile, "rc": p.returncode, "wall": time.monotonic() - started,
            "tail": p.stderr.strip().splitlines()[-1:] if p.stderr else []}


def most_heard(build: Path, world: str, runs: list[Path], out: Path):
    """Every placed resident, most heard first (lines heard by any listener over the runs given)."""
    heard: dict[str, int] = {}
    profiles: dict[str, set] = {}
    for d in runs:
        for r in talk_score.read_jsonl(d / "residents.jsonl"):
            n = int(r.get("_lines", 0))
            if n:
                heard[r["id"]] = heard.get(r["id"], 0) + n
                profiles.setdefault(r["id"], set()).add(d.name.split("-", 1)[-1])
    p = subprocess.run([str(build / "talk_check"), "--world", world, "--residents", "--json"], capture_output=True, text=True)
    everyone = [json.loads(l) for l in p.stdout.splitlines() if l.startswith("{")]
    total = sum(heard.values()) or 1
    everyone.sort(key=lambda r: (-heard.get(r["id"], 0), r.get("_home", ""), r["id"]))
    with out.open("w") as f:
        f.write("# The most-heard residents (Docs/Design/84 §14.3, §15; tools/talk_card.py --most-heard): lines each was "
                "heard saying over the runs " + ", ".join(d.name for d in runs) + ". Every placed resident, the most heard "
                "first: the order mantles are written in within each batch.\n")
        f.write("rank\tid\tname\thome\tquarter\twork\tdialect_from\taccent\tlines_heard\tshare_pct\tprofiles\n")
        for i, r in enumerate(everyone, 1):
            n = heard.get(r["id"], 0)
            f.write("\t".join(str(x) for x in (
                i, r["id"], r.get("name", ""), r.get("_home", ""), r.get("quarter", ""),
                r.get("_work_label", ""), r.get("from", ""), r.get("accent", ""), n, f"{100 * n / total:.2f}",
                ",".join(sorted(profiles.get(r["id"], ())))) ) + "\n")
    return everyone, heard


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--modes", default="frozen,fixtures")
    parser.add_argument("--profiles", default=",".join(PROFILES))
    parser.add_argument("--world", default="ridgemere3")
    parser.add_argument("--build", type=Path, default=ROOT / "build-core")
    parser.add_argument("--jobs", type=int, default=10)
    parser.add_argument("--hours", type=float)
    parser.add_argument("--freeze-at", type=float)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--bless", action="store_true")
    parser.add_argument("--refreeze", default="", help="metric ids whose baseline rows this card replaces (with --why)")
    parser.add_argument("--why", default="", help="why --refreeze (kept in the baseline)")
    parser.add_argument("--most-heard", type=Path)
    parser.add_argument("--rescore", action="store_true", help="score the runs already in --out, running nothing")
    args = parser.parse_args()
    refreeze = [m for m in args.refreeze.split(",") if m]
    if refreeze and not args.why:
        parser.error("--refreeze needs --why")
    if not os.environ.get("RATW_DATABASE_URL"):
        print("talk_card: RATW_DATABASE_URL isn't set; talk_sim asks tools/world_db.py for the connection", file=sys.stderr)
    freeze_at = args.freeze_at or freeze_day(args.fixtures)
    modes = [m for m in args.modes.split(",") if m]
    profiles = [p for p in args.profiles.split(",") if p]
    jobs = [(m, p) for m in modes for p in profiles]
    started = time.monotonic()
    results = []
    if args.rescore:
        jobs = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_one, args.build, args.world, args.fixtures, m, p, args.out / f"{m}-{p}", args.hours,
                               freeze_at) for m, p in jobs]
        for f in concurrent.futures.as_completed(futures):
            r = f.result()
            results.append(r)
            print(f"talk_card: {r['mode']} {r['profile']}: rc {r['rc']} in {r['wall']:.0f} s {' '.join(r['tail'])}",
                  file=sys.stderr)
    status = 0
    summary = {"freeze_at": freeze_at, "fixtures": str(args.fixtures), "wall_seconds": time.monotonic() - started,
               "runs": sorted(results, key=lambda r: (r["mode"], r["profile"]))}
    failed = [r for r in results if r["rc"] != 0]
    for r in failed:
        print(f"talk_card: {r['mode']} {r['profile']} failed (rc {r['rc']}): {' '.join(r['tail'])}; see "
              f"{args.out / (r['mode'] + '-' + r['profile']) / 'talk_sim.log'}", file=sys.stderr)
        status = 1
    for mode in modes:
        dirs = [args.out / f"{mode}-{p}" for p in profiles if (args.out / f"{mode}-{p}" / "run.json").is_file()]
        absent = [p for p in profiles if not (args.out / f"{mode}-{p}" / "run.json").is_file()]
        if absent:
            print(f"talk_card: {mode}: no run for {', '.join(absent)}", file=sys.stderr)
            status = 1
        if not dirs:
            continue
        card = talk_score.scorecard(dirs)
        card["mode"] = mode
        card["freeze_at"] = freeze_at if mode == "frozen" else None
        (args.out / f"scorecard-{mode}.json").write_text(json.dumps(card, indent=1, ensure_ascii=False) + "\n")
        (args.out / f"scorecard-{mode}.md").write_text(talk_score.markdown(card))
        base = ROOT / "artifacts" / "talk" / ("scorecard-baseline.json" if mode == "frozen" else f"scorecard-baseline-{mode}.json")
        if args.baseline and mode != "frozen":
            base = args.baseline.with_name(args.baseline.stem + f"-{mode}.json")
        elif args.baseline:
            base = args.baseline
        if (args.bless or refreeze) and (failed or absent):
            print(f"talk_card: {mode}: nothing blessed or refrozen from a card with a failed or missing run", file=sys.stderr)
            status = 1
        elif args.bless:
            base.parent.mkdir(parents=True, exist_ok=True)
            base.write_text(json.dumps(card, indent=1, ensure_ascii=False) + "\n")
            print(f"talk_card: {mode} baseline written to {base}", file=sys.stderr)
        elif refreeze and base.is_file():
            base.write_text(json.dumps(talk_score.refreeze(json.loads(base.read_text()), card, refreeze, args.why),
                                       indent=1, ensure_ascii=False) + "\n")
            print(f"talk_card: {mode}: {', '.join(refreeze)} refrozen in {base}", file=sys.stderr)
        if base.is_file() and not args.bless:
            problems = talk_score.check(card, json.loads(base.read_text()))
            (args.out / f"scorecard-{mode}-diff.txt").write_text("\n".join(problems) + ("\n" if problems else "no gate broken\n"))
            for p in problems:
                print(f"talk_card: {mode}: {p}", file=sys.stderr)
            status = status or (1 if problems else 0)
    if args.most_heard:
        source = "fixtures" if "fixtures" in modes else modes[0]
        dirs = [args.out / f"{source}-{p}" for p in profiles if (args.out / f"{source}-{p}" / "residents.jsonl").is_file()]
        most_heard(args.build, args.world, dirs, args.most_heard)
        print(f"talk_card: the most-heard list in {args.most_heard}", file=sys.stderr)
    if not args.rescore:                            # (A rescore keeps the runs' own record.)
        (args.out / "card.json").write_text(json.dumps(summary, indent=1) + "\n")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
