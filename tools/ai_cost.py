#!/usr/bin/env python3
"""What the NPC voices cost (Docs/Design/28-ai-cost.md): who answered each NPC line, and what the models were paid.

Reads two ledgers, neither with any words in it:
  Saved/Logs/npc-voices.jsonl     every NPC line the game server said, and which route answered it (the game itself,
                                  the exchange library, written lines, or a model)
  Saved/Logs/npc-mind-calls.jsonl every call the NPC Mind made: kind, model, outcome and tokens
and, for money, the prices in the Mind's config (Saved/Config/RATWNPCAI.local.json, or --config), per model and per
million tokens:
  "prices": {"MODEL": {"input": 0.0, "cached_input": 0.0, "output": 0.0}, ...}
(dollars per million tokens, from the provider's price list: the figures change, so they are not kept in code).
The config's key is never read out or printed.

  python3 tools/ai_cost.py                 the last seven days, day by day
  python3 tools/ai_cost.py --days 1 --json the last day, as JSON
"""
from __future__ import annotations

import argparse
from collections import defaultdict
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parent.parent
LOGS = ROOT / "Saved" / "Logs"
CONFIG = ROOT / "Saved" / "Config" / "RATWNPCAI.local.json"
NO_MODEL = ("game", "game+polish", "library", "written")    # Routes that cost nothing (polish is counted as a call).


def read_lines(path: Path, since: float):
    if not path.is_file():
        return
    with path.open(encoding="utf-8") as f:
        for line in f:
            try:
                entry = json.loads(line)
            except ValueError:
                continue
            if isinstance(entry, dict) and isinstance(entry.get("t"), (int, float)) and entry["t"] >= since:
                yield entry


def read_prices(path: Path) -> dict:
    try:
        data = json.loads(path.read_text())
    except (OSError, ValueError):
        return {}
    prices = data.get("prices") if isinstance(data, dict) else None
    if not isinstance(prices, dict):
        return {}
    clean = {}
    for model, p in prices.items():
        if isinstance(p, dict):
            clean[str(model)] = {k: float(p[k]) for k in ("input", "cached_input", "output")
                                 if isinstance(p.get(k), (int, float)) and p[k] >= 0}
    return clean


def cost_of(call: dict, prices: dict) -> float | None:
    p = prices.get(call.get("model", ""))
    if not p or "input" not in p or "output" not in p:
        return None
    prompt, cached = call.get("prompt_tokens", 0), call.get("cached_tokens", 0)
    return ((prompt - cached) * p["input"] + cached * p.get("cached_input", p["input"]) +
            call.get("completion_tokens", 0) * p["output"]) / 1_000_000


def report(voices: list[dict], calls: list[dict], prices: dict) -> dict:
    day = lambda t: time.strftime("%Y-%m-%d", time.localtime(t))
    days: dict[str, dict] = defaultdict(lambda: {"lines": defaultdict(lambda: defaultdict(int)), "calls": {},
                                                 "unpriced": set()})
    for v in voices:
        days[day(v["t"])]["lines"][str(v.get("kind"))][str(v.get("route"))] += 1
    for c in calls:
        d = days[day(c["t"])]
        key = f'{c.get("event")}|{c.get("model")}'
        row = d["calls"].setdefault(key, {"kind": c.get("event"), "model": c.get("model"), "calls": 0, "failed": 0,
                                          "prompt_tokens": 0, "cached_tokens": 0, "completion_tokens": 0, "cost": 0.0})
        row["calls"] += 1
        row["failed"] += c.get("outcome") != "success"
        for k in ("prompt_tokens", "cached_tokens", "completion_tokens"):
            row[k] += int(c.get(k, 0) or 0)
        money = cost_of(c, prices)
        if money is None:
            d["unpriced"].add(str(c.get("model")))
        else:
            row["cost"] += money
    out = {}
    for name in sorted(days):
        d = days[name]
        said = sum(n for routes in d["lines"].values() for n in routes.values())
        free = sum(n for routes in d["lines"].values() for r, n in routes.items() if r in NO_MODEL)
        out[name] = {"lines": {k: dict(v) for k, v in d["lines"].items()}, "linesSaid": said,
                     "withoutModel": free, "withoutModelShare": round(free / said, 3) if said else None,
                     "calls": sorted(d["calls"].values(), key=lambda r: (r["kind"], r["model"])),
                     "cost": round(sum(r["cost"] for r in d["calls"].values()), 4),
                     "cachedShare": round(sum(r["cached_tokens"] for r in d["calls"].values()) /
                                          max(1, sum(r["prompt_tokens"] for r in d["calls"].values())), 3),
                     "unpriced": sorted(d["unpriced"])}
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--days", type=float, default=7)
    parser.add_argument("--voices", type=Path, default=LOGS / "npc-voices.jsonl")
    parser.add_argument("--calls", type=Path, default=LOGS / "npc-mind-calls.jsonl")
    parser.add_argument("--config", type=Path, default=CONFIG)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    since = time.time() - args.days * 86400
    result = report(list(read_lines(args.voices, since)), list(read_lines(args.calls, since)), read_prices(args.config))
    if args.json:
        print(json.dumps(result, indent=2))
        return 0
    if not result:
        print("Nothing in the ledgers for that time (they fill while tools/server.sh runs).")
        return 0
    for name, d in result.items():
        share = f'{d["withoutModelShare"] * 100:.0f}%' if d["withoutModelShare"] is not None else "-"
        print(f'{name}: {d["linesSaid"]} NPC lines, {share} without a model; '
              f'cost ${d["cost"]:.4f}; cached input {d["cachedShare"] * 100:.0f}%')
        for kind, routes in sorted(d["lines"].items()):
            print(f'  {kind:9} ' + ", ".join(f"{r} {n}" for r, n in sorted(routes.items())))
        for r in d["calls"]:
            print(f'  {r["kind"]:9} {r["model"]:22} {r["calls"]:5} calls ({r["failed"]} failed), '
                  f'{r["prompt_tokens"]} in ({r["cached_tokens"]} cached), {r["completion_tokens"]} out, ${r["cost"]:.4f}')
        if d["unpriced"]:
            print(f'  no prices in the config for: {", ".join(d["unpriced"])}')
    return 0


if __name__ == "__main__":
    sys.exit(main())
