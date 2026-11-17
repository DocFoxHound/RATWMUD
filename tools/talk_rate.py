#!/usr/bin/env python3
"""Blind ratings of residents' talk (Docs/Design/84-living-conversations.md §15.7, H1 and H2).

  python3 tools/talk_rate.py packet --new RUN_DIR [--old RUN_DIR] [--shuffled RUN_DIR] --out DIR [--key FILE]
          [--each 30] [--seed N] [--title T]
  python3 tools/talk_rate.py score DIR/ratings.csv [--key DIR/key.json] [--json OUT]

`packet` makes 90 conversations (30 from the new system, 30 from the old library, 30 from the shuffled control: the
--new run's lines-shuffled.jsonl unless --shuffled names another run) in a random order, numbered only, with nothing
on the page saying which is which: DIR/rate.html (rate each 1-5 for how naturally it flows, and tick "written whole by
a writer?"; "Download ratings" saves ratings.csv), DIR/ratings.csv (the same, blank, for rating in a spreadsheet), and
DIR/packet.md (the same packet as a Markdown sheet, to rate in any editor), and DIR/key.json, or --key FILE (which
source each number came from: don't open it before rating; --key keeps it away from the packet the rater is given).
Each source's conversations are sampled over topic family and length, of 2 lines or more. Nothing tells the sources
apart but the talk itself: the page and the sheet give a number, the speakers' names and the lines, no place, day,
run, entry or conversation id; a shuffled conversation keeps its real one's first line, so a conversation already
taken from one source is never taken again from another, and no two items open with the same words. At Phase 0
there is only today's system, so --old is left out (or given the same run) and the packet is today's against its
shuffled control.

`score` reads the ratings with the key: H1, the mean rating of each source, new minus shuffled (target 2.0+) and new
minus old (target 1.0+), against 3.8+ for the new; H2, the share of each source taken for a writer's whole work (target
40%+ for the new).
"""
from __future__ import annotations

import argparse
import csv
import html
import json
import random
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import talk_listen  # noqa: E402
import talk_score  # noqa: E402

PAGE = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Talk ratings</title>
<style>
:root {{ --bg:#f6f3ee; --card:#fffdf9; --ink:#2b2622; --muted:#7a7068; --line:#e3dcd2; --accent:#5a4a8a; }}
@media (prefers-color-scheme: dark) {{ :root:not([data-theme="light"]) {{ --bg:#1d1b19; --card:#26231f; --ink:#ece6dd;
        --muted:#a39a8f; --line:#3a352f; --accent:#a99be0; }} }}
:root[data-theme="dark"] {{ --bg:#1d1b19; --card:#26231f; --ink:#ece6dd; --muted:#a39a8f; --line:#3a352f; --accent:#a99be0; }}
* {{ box-sizing:border-box; }}
body {{ margin:0; background:var(--bg); color:var(--ink); font:15px/1.5 Georgia, serif; }}
main {{ max-width:760px; margin:0 auto; padding:24px 16px 80px; }}
.bar {{ position:sticky; top:0; background:var(--bg); padding:8px 0; border-bottom:1px solid var(--line);
        font:13px system-ui, sans-serif; display:flex; gap:8px; flex-wrap:wrap; align-items:center; }}
.c {{ background:var(--card); border:1px solid var(--line); border-radius:10px; padding:12px 14px; margin:12px 0; }}
.n {{ font:12px system-ui, sans-serif; color:var(--muted); }}
.who {{ font-weight:600; color:var(--accent); }}
.r {{ font:13px system-ui, sans-serif; margin-top:8px; display:flex; gap:10px; flex-wrap:wrap; align-items:center; }}
button {{ font:13px system-ui, sans-serif; border:1px solid var(--line); background:var(--card); color:var(--ink);
          border-radius:6px; padding:4px 10px; cursor:pointer; }}
</style></head><body><main>
<h1>How natural is it?</h1>
<p>{n} conversations between residents, in no order. Rate each from 1 (makes no sense) to 5 (as natural as real people),
and tick the box if it reads as if one writer wrote the whole exchange. Nothing here says where any of them came from.</p>
<div class="bar"><span id="done"></span><button onclick="download()">Download ratings</button></div>
{body}
</main><script>
const KEY = 'talk-rate-{stamp}';
let r = {{}};
try {{ r = JSON.parse(localStorage.getItem(KEY) || '{{}}'); }} catch (e) {{ r = {{}}; }}
function save() {{ try {{ localStorage.setItem(KEY, JSON.stringify(r)); }} catch (e) {{}}
  document.getElementById('done').textContent = Object.values(r).filter(x => x.rating).length + ' of {n} rated'; }}
document.querySelectorAll('.c').forEach(c => {{
  const id = c.dataset.id, x = r[id] || {{}};
  c.querySelectorAll('input[type=radio]').forEach(i => {{ i.checked = String(x.rating) === i.value;
    i.onchange = () => {{ r[id] = r[id] || {{}}; r[id].rating = Number(i.value); save(); }}; }});
  const w = c.querySelector('input[type=checkbox]'); w.checked = !!x.writer;
  w.onchange = () => {{ r[id] = r[id] || {{}}; r[id].writer = w.checked; save(); }};
}});
function download() {{
  let csv = 'item,rating,writer\\n';
  document.querySelectorAll('.c').forEach(c => {{ const x = r[c.dataset.id] || {{}};
    csv += c.dataset.id + ',' + (x.rating || '') + ',' + (x.writer ? 1 : 0) + '\\n'; }});
  const a = document.createElement('a'); a.href = URL.createObjectURL(new Blob([csv], {{ type: 'text/csv' }}));
  a.download = 'ratings.csv'; a.click();
}}
save();
</script></body></html>
"""


def opener(lines: list) -> str:
    return " ".join(lines[0].get("text", "").lower().split()) if lines else ""


def sample(run: talk_score.Run, n: int, rng: random.Random, taken: set | None = None, openers: set | None = None) -> list:
    """n conversations of 2 lines or more, spread over topic family x length. `taken` holds (run dir, conversation)
    pairs already in the packet (a shuffled conversation is its real one's number in the same run dir) and `openers`
    the first lines already used: neither is taken again, and both are added to."""
    taken = set() if taken is None else taken
    openers = set() if openers is None else openers
    where = str(run.dir.resolve())
    convs = [(run, run.conv_head.get(c, {"conv": c}), run.by_conv[c]) for c in sorted(run.by_conv)
             if len(run.by_conv[c]) >= 2 and (where, c) not in taken]
    strata: dict[tuple, list] = {}
    for x in convs:
        strata.setdefault((talk_listen.family(x[1].get("topic", "")), min(len(x[2]), 5)), []).append(x)
    for v in strata.values():
        rng.shuffle(v)
    out = []
    keys = sorted(strata)
    while len(out) < n and any(strata[k] for k in keys):
        for k in keys:
            while strata[k] and len(out) < n:
                x = strata[k].pop()
                first = opener(x[2])
                if first in openers:
                    continue                        # (The same words opening two items would pair them.)
                openers.add(first)
                taken.add((where, x[1].get("conv")))
                out.append(x)
                break
    return out


def packet(args) -> int:
    rng = random.Random(args.seed)
    new = talk_score.Run(args.new)
    sources = [("new", new)]
    if args.old and args.old.resolve() != args.new.resolve():
        sources.append(("old", talk_score.Run(args.old)))
    shuffled = talk_score.Run(args.shuffled or args.new, shuffled=True)
    if not shuffled.lines:
        print("talk_rate: no shuffled control (run talk_sim with --ablate shuffle)", file=sys.stderr)
        return 1
    sources.append(("shuffled", shuffled))
    items = []
    taken: set = set()
    openers: set = set()
    for name, run in sources:
        picked = sample(run, args.each, rng, taken, openers)
        if len(picked) < args.each:
            print(f"talk_rate: only {len(picked)} conversations of 2 lines or more from the {name} source "
                  f"({talk_score.relative(run.dir)}), {args.each} asked", file=sys.stderr)
            return 1
        for r, head, lines in picked:
            items.append({"source": name, "run": talk_score.relative(run.dir), "conv": head.get("conv"), "lines": lines,
                          "residents": r.residents})
    rng.shuffle(items)
    args.out.mkdir(parents=True, exist_ok=True)
    key_path = args.key or args.out / "key.json"
    key_path.parent.mkdir(parents=True, exist_ok=True)
    body = []
    sheet = [f"# {args.title}", "",
             f"{len(items)} conversations overheard between residents, in no particular order. Rate each from 1 (it makes "
             "no sense) to 5 (as natural as real people talking), and say yes if it reads as though one writer wrote the "
             "whole exchange. Nothing here says where any of them came from: judge only the talk. The same packet is "
             "on a page with buttons (`rate.html`, beside this file: \"Download ratings\" saves `ratings.csv`), or fill "
             "in `ratings.csv` (item, rating, writer as 1 or 0) by hand.", ""]
    key = {"about": "talk_rate.py's key: which source each item came from. Open only after rating.", "items": {}}
    for i, it in enumerate(items, 1):
        rows = "".join(f'<div><span class="who">{html.escape(it["residents"].get(l["who"], {}).get("name", "Someone"))}:'
                       f'</span> {html.escape(l["text"])}</div>' for l in it["lines"])
        radios = "".join(f'<label><input type="radio" name="r{i}" value="{v}"> {v}</label>' for v in range(1, 6))
        body.append(f'<section class="c" data-id="{i}"><div class="n">{i}</div>{rows}<div class="r">{radios}'
                    f'<label><input type="checkbox"> written whole by a writer?</label></div></section>')
        sheet.append(f"## {i}")
        sheet.append("")
        for l in it["lines"]:
            sheet.append(f"> **{it['residents'].get(l['who'], {}).get('name', 'Someone')}:** {l['text']}  ")
        sheet.append("")
        sheet.append("Rating (1-5): ___ · Written whole by a writer? ___")
        sheet.append("")
        key["items"][str(i)] = {"source": it["source"], "run": it["run"], "conv": it["conv"]}
    stamp = str(zlib.crc32(json.dumps([args.seed, sorted(key["items"].items())]).encode()) % 10**8)
    (args.out / "rate.html").write_text(PAGE.format(n=len(items), body="\n".join(body), stamp=stamp))
    (args.out / "packet.md").write_text("\n".join(sheet) + "\n")
    key["counts"] = {name: sum(1 for it in items if it["source"] == name) for name, _ in sources}
    key_path.write_text(json.dumps(key, indent=1) + "\n")
    with (args.out / "ratings.csv").open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["item", "rating", "writer"])
        for i in range(1, len(items) + 1):
            w.writerow([i, "", ""])
    print(f"talk_rate: {len(items)} conversations ({', '.join(n for n, _ in sources)}) in {args.out}; the key in "
          f"{key_path}", file=sys.stderr)
    return 0


def score(args) -> int:
    key = json.loads((args.key or args.ratings.parent / "key.json").read_text())["items"]
    by: dict[str, list] = {}
    writer: dict[str, list] = {}
    with args.ratings.open() as f:
        for row in csv.DictReader(f):
            k = key.get(str(row.get("item", "")).strip())
            if not k or not str(row.get("rating", "")).strip():
                continue
            by.setdefault(k["source"], []).append(float(row["rating"]))
            writer.setdefault(k["source"], []).append(str(row.get("writer", "")).strip() in ("1", "yes", "true", "x"))
    mean = {s: sum(v) / len(v) for s, v in by.items() if v}
    out = {"H1": {"mean": mean, "rated": {s: len(v) for s, v in by.items()}},
           "H2": {s: 100.0 * sum(v) / len(v) for s, v in writer.items() if v}}
    if "new" in mean and "shuffled" in mean:
        out["H1"]["new_minus_shuffled"] = mean["new"] - mean["shuffled"]
    if "new" in mean and "old" in mean:
        out["H1"]["new_minus_old"] = mean["new"] - mean["old"]
    text = json.dumps(out, indent=1)
    if args.json:
        args.json.write_text(text + "\n")
    print(text)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("packet")
    p.add_argument("--new", type=Path, required=True)
    p.add_argument("--old", type=Path)
    p.add_argument("--shuffled", type=Path)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--key", type=Path, help="where the answer key goes (default OUT/key.json)")
    p.add_argument("--each", type=int, default=30)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--title", default="How natural is it?")
    s = sub.add_parser("score")
    s.add_argument("ratings", type=Path)
    s.add_argument("--key", type=Path)
    s.add_argument("--json", type=Path)
    args = parser.parse_args()
    return packet(args) if args.command == "packet" else score(args)


if __name__ == "__main__":
    raise SystemExit(main())
