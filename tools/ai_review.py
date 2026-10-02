#!/usr/bin/env python3
"""A blind review of cheaper NPC voices (Docs/Design/28-ai-cost.md, step 5): sample lines answered three ways (the
game itself, the small model, the main model) and sample exchanges three ways (the library, the small model, the main
model), side by side in shuffled columns, the key hidden at the end. Judge which sound right before relying on them.

Makes paid calls (about two per sample): run it only when you mean to.

  python3 tools/ai_review.py                      writes artifacts/ai-review/index.html
  python3 tools/ai_review.py --fixture --out DIR  offline, for tests
"""
from __future__ import annotations

import argparse
import html
import json
import random
import subprocess
import sys
from pathlib import Path

import npc_bridge as bridge
import npc_mind as mind

ROOT = Path(__file__).resolve().parent.parent
SAMPLES = ROOT / "Data" / "Voice" / "review_samples.json"
LIBRARY = ROOT / "Data" / "Voice" / "library.json"
CONFIG = ROOT / "Saved" / "Config" / "RATWNPCAI.local.json"
CHECK = ROOT / "build-core" / "voice_check"


def game_answers(samples: Path, check: Path) -> list[dict]:
    out = subprocess.run([str(check), str(ROOT / "Data" / "Voice"), str(samples)], capture_output=True, text=True,
                         timeout=120, check=True).stdout.strip().splitlines()[-1]
    return json.loads(out)


def model_reply(m: "mind.Mind", sample: dict) -> str:
    # The context the game itself would send (voice_check builds it as talk() does), trimmed as the game trims it.
    context = {k: v for k, v in sample["context"].items() if isinstance(v, str)}
    context["scene"] = context.get("scene", "")[:4000]
    context["backstory"] = context.get("backstory", "")[:12000]
    try:
        return m.dialogue(context)["text"]
    except bridge.BridgeError as error:
        return f"(no answer: {error.code})"


def library_exchange(library: dict, ex: dict, seed: int) -> list:
    fitting = [e for e in library.get("exchanges", []) if e["kind"] == ex["kind"] and e.get("band", "any") in (ex["band"], "any")]
    if not fitting:
        return []
    chosen = fitting[seed % len(fitting)]
    fill = lambda t: t.format_map({k: ex.get(k, "") for k in ("teller", "listener", "subject", "claim", "news", "day")})
    return [[who, fill(text)] for who, text in chosen["lines"]]


def model_exchange(m: "mind.Mind", ex: dict) -> list:
    facts = {"gossip": [f'{ex["teller"]} has heard that {ex.get("subject")} {ex.get("claim")}, and tells {ex["listener"]}.'],
             "news": [ex.get("news", "") + "."], "quarrel": [f'{ex["teller"]} and {ex["listener"]} do not get on.'],
             "day": [ex.get("day", "")]}.get(ex["kind"], [])
    try:
        got = m.exchange({"a": {"name": ex["teller"]}, "b": {"name": ex["listener"]}, "topic": {"kind": ex["kind"], "facts": facts}})
        return [[l["speaker"], l["text"]] for l in got["lines"]]
    except bridge.BridgeError as error:
        return [["a", f"(no answer: {error.code})"]]


def page(rows: list[dict]) -> str:
    cells = []
    key = []
    for i, row in enumerate(rows):
        options = list(row["answers"].items())
        random.Random(i).shuffle(options)
        key.append(f'<li>{i + 1}: ' + ", ".join(f"{'ABC'[j]} = {html.escape(src)}" for j, (src, _) in enumerate(options)) + "</li>")
        tds = "".join(f'<td><b>{"ABC"[j]}</b><div>{text}</div></td>' for j, (_, text) in enumerate(options))
        cells.append(f'<tr><th>{i + 1}. {row["title"]}</th>{tds}</tr>')
    return f"""<!doctype html><html><head><meta charset="utf-8"><title>NPC voices review</title>
<style>:root{{--bg:#111a18;--fg:#e6e0d4;--muted:#8b9b91;--line:#2d3b36}}
@media (prefers-color-scheme: light){{:root{{--bg:#faf8f3;--fg:#222;--muted:#666;--line:#ddd}}}}
body{{background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,sans-serif;margin:24px auto;max-width:1200px;padding:0 16px}}
table{{border-collapse:collapse;width:100%}}td,th{{border:1px solid var(--line);padding:8px;vertical-align:top;text-align:left}}
th{{width:24%;color:var(--muted);font-weight:600}}td div{{margin-top:4px}}p{{color:var(--muted)}}</style></head><body>
<h1>NPC voices: a blind review</h1>
<p>Each row is answered three ways, in a shuffled order. Mark which sound like the NPC and which don't; then open the
key. Lines the game answers itself are always true to the world (real prices, real hours); the question is whether
they sound right.</p>
<table>{"".join(cells)}</table>
<details><summary>Key</summary><ol>{"".join(key)}</ol></details></body></html>
"""


def lines_html(lines: list) -> str:
    return "<br>".join(f'{"A" if who == "a" else "B"}: {html.escape(text)}' for who, text in lines) or "(nothing fitting)"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--samples", type=Path, default=SAMPLES)
    parser.add_argument("--config", type=Path, default=CONFIG)
    parser.add_argument("--fixture", action="store_true", help="offline answers, no cost")
    parser.add_argument("--check", type=Path, default=CHECK, help="the voice_check program (built with build-core)")
    parser.add_argument("--out", type=Path, default=ROOT / "artifacts" / "ai-review")
    args = parser.parse_args()
    if args.fixture:
        provider, voice, light = mind.FixtureProvider(), "fixture", "fixture-light"
    else:
        config = bridge.load_config(args.config)
        provider, voice, light = mind.OpenAIProvider(config), config.model, config.light
    quiet = lambda entry: None
    # One reviewer asks everything in a row: no per-speaker limits here (play keeps them).
    unlimited = lambda: mind.Budget(10_000, 10_000)
    big = mind.Mind(provider, budget=unlimited(), audit=quiet, models={"voice": voice, "light": voice}, timeout=20)
    small = mind.Mind(provider, budget=unlimited(), audit=quiet, models={"voice": light, "light": light}, timeout=20)
    big.tiers = mind.Tiers("generous")
    small.tiers = mind.Tiers("generous")
    samples = json.loads(args.samples.read_text())
    rows = []
    for s in game_answers(args.samples, args.check):
        game = html.escape(s["answer"]) if s["route"] == "game" else "<i>(the game passes this to a model)</i>"
        rows.append({"title": html.escape(f'To {s["name"]}: "{s["say"]}"'),
                     "answers": {"the game": game, f"small model ({light})": html.escape(model_reply(small, s)),
                                 f"main model ({voice})": html.escape(model_reply(big, s))}})
    library = json.loads(LIBRARY.read_text()) if LIBRARY.is_file() else {}
    for i, ex in enumerate(samples.get("exchanges", [])):
        rows.append({"title": html.escape(f'Overheard: {ex["kind"]} ({ex["band"]})'),
                     "answers": {"the library": lines_html(library_exchange(library, ex, i)),
                                 f"small model ({light})": lines_html(model_exchange(small, ex)),
                                 f"main model ({voice})": lines_html(model_exchange(big, ex))}})
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "index.html").write_text(page(rows), encoding="utf-8")
    print(f"{len(rows)} rows written to {args.out / 'index.html'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
