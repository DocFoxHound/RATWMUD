#!/usr/bin/env python3
"""The economy controller's runs (Docs/Design/75-the-economy-controller.md, 5.1; after steer-smokes.sh).

Reads a manifest of variants, builds each one's export folder (export70's cells and seams hard-linked, nothing copied;
its world.ratw with the variant's rule lines after the store line), runs them five at a time, each on its own cores with
stable10b's flags, waits for every one to exit, then runs econ_report on each. Never writes DEV or PROD: every run is a
scratch copy of an export folder.

    python3 tools/steer_runs.py MANIFEST.json [--only NAME ...] [--prepare] [--reports-only]

The manifest:

    {
      "base": "artifacts/economy/r2/export70",      (relative to the main checkout, or absolute)
      "out": "artifacts/economy/r2",                (where the runs' files go)
      "prefix": "steer-b0-",                        (each run's files: <prefix><name>.jsonl, .out, .exit, ...)
      "days": 366,                                  (the run's length: from 06:00 on day 0, days x 24 hours)
      "binary": ".claude/worktrees/r2-controller/build-c/world_check",
      "data": ".claude/worktrees/r2-controller/Data",
      "runs": [
        {"name": "left", "rules": {"steer": 1, "steerLean": 0},
         "table": "path/to/steer-table.json",      (optional: world_check --steer-table)
         "at": [[120, "steerLean", 0]],             (optional: --rule-at DAY NAME VALUE, as the DM's economy.sync)
         "days": 480}                               (optional: this run's own length)
      ]
    }

The slots (doc 75's task: cores 2-5, 6-9, 10-13, 14-17 with four threads, 18-19 with two). A batch of more than five
runs goes in waves of five.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import time

TOOLS = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(TOOLS)
SLOTS = [("2-5", 4), ("6-9", 4), ("10-13", 4), ("14-17", 4), ("18-19", 2)]


def main_checkout():
    # The worktree's main checkout (artifacts live there): git's common dir's parent.
    try:
        common = subprocess.check_output(["git", "-C", TREE, "rev-parse", "--git-common-dir"], text=True).strip()
        return os.path.dirname(os.path.abspath(os.path.join(TREE, common)))
    except (OSError, subprocess.CalledProcessError):
        return TREE


ROOT = main_checkout()


def path(p):
    return p if os.path.isabs(p) else os.path.join(ROOT, p)


def rule_lines(rules):
    return [f'rule "{k}" {int(v)}' for k, v in rules.items()]


def prepare(m, run):
    base = path(m["base"])
    dest = os.path.join(path(m["out"]), "export-" + m["prefix"] + run["name"])
    os.makedirs(dest, exist_ok=True)
    for part in ("cells", "seams"):
        target = os.path.join(dest, part)
        if os.path.islink(target):
            os.remove(target)
        if not os.path.isdir(target):
            # (Hard links where it can, a copy across file systems.)
            if subprocess.call(["cp", "-al", os.path.join(base, part), target], stderr=subprocess.DEVNULL) != 0:
                shutil.rmtree(target, ignore_errors=True)
                subprocess.check_call(["cp", "-a", os.path.join(base, part), target])
    lines = rule_lines(run.get("rules", {}))
    with open(os.path.join(base, "world.ratw")) as f, open(os.path.join(dest, "world.ratw"), "w") as o:
        stored = False
        for line in f:
            o.write(line)
            if line.startswith("store ") and not stored:
                stored = True
                for r in lines:
                    o.write(r + "\n")
        if lines and not stored:
            sys.exit(f"{base}/world.ratw has no store line for the rules")
    return dest


def command(m, run, slot):
    days = int(run.get("days", m.get("days", 366)))
    out = os.path.join(path(m["out"]), m["prefix"] + run["name"])
    cores, threads = slot
    cmd = ["taskset", "-c", cores, path(m["binary"]), os.path.join(path(m["out"]), "export-" + m["prefix"] + run["name"]),
           "--simulate", "6", str(6 + days * 24), "--no-check", "--deterministic", "--threads", str(threads), "--no-profile",
           "--economy", out + ".jsonl", "--loop-state", out + "_loop.jsonl", "--farms", "0",
           "--economy-level", "full", "--economy-full", out + ".full.jsonl.gz"]
    for day, name, value in run.get("at", []):
        cmd += ["--rule-at", str(int(day)), name, str(int(value))]
    if run.get("table"):
        cmd += ["--steer-table", path(run["table"])]
    return cmd, out


def run_batch(m, runs):
    env = dict(os.environ, RATW_DATA_DIR=path(m["data"]))
    for i in range(0, len(runs), len(SLOTS)):
        wave = runs[i:i + len(SLOTS)]
        procs = []
        for run, slot in zip(wave, SLOTS):
            prepare(m, run)
            cmd, out = command(m, run, slot)
            with open(out + ".cmd", "w") as f:
                f.write(" ".join(cmd) + "\n")
            log = open(out + ".out", "w")
            procs.append((run["name"], out, subprocess.Popen(cmd, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT), log))
            print(f"started {run['name']} on cores {slot[0]}", flush=True)
        start = time.time()
        for name, out, p, log in procs:
            code = p.wait()
            log.close()
            with open(out + ".exit", "w") as f:
                f.write(f"{code}\n")
            print(f"{name} exited {code} after {(time.time() - start) / 60:.1f} min", flush=True)


def reports(m, runs):
    for run in runs:
        out = os.path.join(path(m["out"]), m["prefix"] + run["name"])
        if not os.path.exists(out + ".jsonl"):
            continue
        cmd = [sys.executable, os.path.join(TOOLS, "econ_report.py"), out + ".jsonl", "--out", out + "-report.md"]
        if os.path.exists(out + ".full.jsonl.gz"):
            cmd[3:3] = ["--full", out + ".full.jsonl.gz"]
        code = subprocess.call(cmd, cwd=ROOT, stdout=subprocess.DEVNULL)
        print(f"report {run['name']}: {'ok' if code == 0 else 'exit ' + str(code)}", flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("manifest")
    ap.add_argument("--only", nargs="*")
    ap.add_argument("--prepare", action="store_true", help="the export folders only")
    ap.add_argument("--reports-only", action="store_true")
    a = ap.parse_args()
    with open(a.manifest) as f:
        m = json.load(f)
    runs = [r for r in m["runs"] if not a.only or r["name"] in a.only]
    if not runs:
        sys.exit("no runs")
    for key in ("binary", "data", "base"):
        if not os.path.exists(path(m[key])):
            sys.exit(f"no {key}: {path(m[key])}")
    shutil.copy(a.manifest, os.path.join(path(m["out"]), m["prefix"] + "manifest.json"))
    if a.prepare:
        for r in runs:
            print(prepare(m, r))
        return
    if not a.reports_only:
        run_batch(m, runs)
    reports(m, runs)


if __name__ == "__main__":
    main()
