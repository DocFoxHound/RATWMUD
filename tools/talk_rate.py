#!/usr/bin/env python3
"""Blind ratings of residents' talk (Docs/Design/84-living-conversations.md §15.7, H1 and H2).

  python3 tools/talk_rate.py packet --new RUN_DIR [--old RUN_DIR] [--shuffled RUN_DIR] --out DIR [--key FILE]
          [--each 30] [--seed N] [--title T]
  python3 tools/talk_rate.py score RATINGS [RATINGS ...] [--key DIR/key.json] [--json OUT] [--report OUT.md] [--worst 5]

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
40%+ for the new). RATINGS are one rater each, or several: a ratings.csv (item, rating, writer) is one rater; a JSON
file holds one rater ({"ratings": [{"id": "1", "believable": 4, "coherent": 5, "note": "..."}, ...]}), a list of them,
or {"raters": [...]}, any number of 1-5 axes per item. With more than one axis, H1 is judged on `overall` (each
rating's mean of its axes) and every axis is reported too. For each axis and source: the mean, the item means' spread
and the distribution of the scores; each rater's means; the differences with a 95% bootstrap interval over items
(seeded, the same every time); and agreement between raters (Krippendorff's alpha, interval and ordinal; the mean
pairwise Spearman correlation over items; exact and within-one agreement). `--worst N` lists the new source's N
lowest-rated conversations (ties broken by coherence, then by number) with every rater's notes and the lines as the
run spoke them (entry, moves, what each owed): what to fix first. `--report` writes all of it as Markdown.
"""
from __future__ import annotations

import argparse
import csv
import html
import json
import math
import random
import statistics
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


# ------------------------------------------------------------------ Scoring

TARGETS = {"new": 3.8, "new_minus_shuffled": 2.0, "new_minus_old": 1.0}   # (§15.3's H1.)
NOT_AXES = {"id", "item", "note", "notes", "writer", "rater", "name"}
YES = ("1", "yes", "true", "x")


def read_raters(paths: list) -> list[dict]:
    """Every rater in the files given: {"name", "scores": {item: {axis: value}}, "notes": {item: text},
    "writer": {item: bool}}. A CSV is one rater (axis `rating`); a JSON file one rater, a list, or {"raters": [...]}."""
    raters = []
    for path in paths:
        path = Path(path)
        if path.suffix.lower() == ".csv":
            r = {"name": path.stem, "scores": {}, "notes": {}, "writer": {}}
            with path.open() as f:
                for row in csv.DictReader(f):
                    item = str(row.get("item", "")).strip()
                    if not item or not str(row.get("rating", "")).strip():
                        continue
                    r["scores"][item] = {"rating": float(row["rating"])}
                    if str(row.get("writer", "")).strip():
                        r["writer"][item] = str(row["writer"]).strip().lower() in YES
            raters.append(r)
            continue
        doc = json.loads(path.read_text())
        group = doc.get("raters", [doc]) if isinstance(doc, dict) else doc
        for n, one in enumerate(group, 1):
            r = {"name": str(one.get("rater") or one.get("name") or f"{path.stem} #{n}"), "scores": {}, "notes": {},
                 "writer": {}}
            for x in one.get("ratings", []):
                item = str(x.get("id", x.get("item", ""))).strip()
                if not item:
                    continue
                axes = {k: float(v) for k, v in x.items() if k not in NOT_AXES and isinstance(v, (int, float))
                        and not isinstance(v, bool)}
                if axes:
                    r["scores"][item] = axes
                if str(x.get("note", "")).strip():
                    r["notes"][item] = str(x["note"]).strip()
                if "writer" in x:
                    r["writer"][item] = str(x["writer"]).strip().lower() in YES or x["writer"] is True
            raters.append(r)
    return raters


def ranks(xs: list) -> list[float]:
    order = sorted(range(len(xs)), key=lambda i: xs[i])
    out = [0.0] * len(xs)
    i = 0
    while i < len(order):
        j = i
        while j + 1 < len(order) and xs[order[j + 1]] == xs[order[i]]:
            j += 1
        for k in range(i, j + 1):
            out[order[k]] = (i + j) / 2 + 1
        i = j + 1
    return out


def spearman(xs: list, ys: list) -> float | None:
    if len(xs) < 3:
        return None
    rx, ry = ranks(xs), ranks(ys)
    mx, my = statistics.fmean(rx), statistics.fmean(ry)
    sxy = sum((a - mx) * (b - my) for a, b in zip(rx, ry))
    sxx = sum((a - mx) ** 2 for a in rx)
    syy = sum((b - my) ** 2 for b in ry)
    return sxy / math.sqrt(sxx * syy) if sxx and syy else None


def krippendorff(units: list[list[float]], metric: str = "interval") -> float | None:
    """Krippendorff's alpha over units (each the values its raters gave; units with fewer than 2 are left out), by the
    coincidence matrix (Krippendorff 2011, "Computing Krippendorff's alpha-reliability"): interval or ordinal."""
    units = [u for u in units if len(u) >= 2]
    values = sorted({v for u in units for v in u})
    if len(values) < 2:
        return None
    index = {v: i for i, v in enumerate(values)}
    o = [[0.0] * len(values) for _ in values]
    for u in units:
        m = len(u)
        for i, a in enumerate(u):
            for j, b in enumerate(u):
                if i != j:
                    o[index[a]][index[b]] += 1 / (m - 1)
    nc = [sum(row) for row in o]
    n = sum(nc)

    def delta(c: int, k: int) -> float:
        if metric == "interval":
            return (values[c] - values[k]) ** 2
        lo, hi = min(c, k), max(c, k)                  # (Ordinal: the marginals between, by rank.)
        return (sum(nc[lo:hi + 1]) - (nc[lo] + nc[hi]) / 2) ** 2

    d_o = sum(o[c][k] * delta(c, k) for c in range(len(values)) for k in range(len(values)))
    d_e = sum(nc[c] * nc[k] * delta(c, k) for c in range(len(values)) for k in range(len(values))) / (n - 1)
    return 1 - d_o / d_e if d_e else None


def bootstrap(a: list[float], b: list[float], reps: int = 4000, seed: int = 84) -> list[float]:
    """A 95% interval for mean(a) - mean(b), resampling each source's items (their means over raters)."""
    rng = random.Random(seed)
    diffs = sorted(statistics.fmean(rng.choices(a, k=len(a))) - statistics.fmean(rng.choices(b, k=len(b)))
                   for _ in range(reps))
    return [diffs[int(0.025 * reps)], diffs[int(0.975 * reps) - 1]]


def r3(x):
    return None if x is None else round(x, 3)


def conversation(k: dict, runs: dict) -> list[dict]:
    """The lines an item showed, as its run spoke them (entry, moves, what each owed), or [] when the run is absent."""
    where = (k.get("run", ""), k.get("source") == "shuffled")
    if where not in runs:
        d = talk_score.ROOT / where[0]
        runs[where] = talk_score.Run(d, shuffled=where[1]) if where[0] and (d / "lines.jsonl").is_file() else None
    run = runs[where]
    if run is None:
        return []
    out = []
    for l in run.by_conv.get(int(k["conv"]), []):
        out.append({"who": run.residents.get(l["who"], {}).get("name", "Someone"), "text": l.get("text", ""),
                    "entry": l.get("entry", ""), "moves": l.get("moves", []), "owes": l.get("owes", []),
                    "answers": l.get("answers", [])})
    head = run.conv_head.get(int(k["conv"]), {})
    if out:
        out[0]["conv"] = {x: head.get(x) for x in ("topic", "ended", "stake", "chain") if head.get(x)}
    return out


def score_ratings(key: dict, raters: list[dict], worst: int = 5) -> dict:
    axes: list[str] = []
    for r in raters:
        for s in r["scores"].values():
            axes += [a for a in s if a not in axes]
    if len(axes) > 1:
        for r in raters:
            for s in r["scores"].values():
                s["overall"] = statistics.fmean(s[a] for a in axes if a in s)
        axes.append("overall")
    main = "overall" if "overall" in axes else axes[0] if axes else "rating"
    sources = [s for s in ("new", "old", "shuffled") if any(k["source"] == s for k in key.values())]
    sources += sorted({k["source"] for k in key.values()} - set(sources))
    items = sorted(key, key=lambda i: int(i) if str(i).isdigit() else 0)
    per_item = {a: {i: [r["scores"][i][a] for r in raters if a in r["scores"].get(i, {})] for i in items} for a in axes}
    by_axis, diffs = {}, {}
    for a in axes:
        by_axis[a] = {}
        for s in sources:
            mine = [i for i in items if key[i]["source"] == s and per_item[a][i]]
            values = [v for i in mine for v in per_item[a][i]]
            if not values:
                continue
            means = [statistics.fmean(per_item[a][i]) for i in mine]
            dist: dict[str, int] = {}
            for v in sorted(values):
                label = f"{v:g}"
                dist[label] = dist.get(label, 0) + 1
            by_axis[a][s] = {"mean": r3(statistics.fmean(values)), "items": len(mine), "ratings": len(values),
                             "item_sd": r3(statistics.pstdev(means)), "item_median": r3(statistics.median(means)),
                             "dist": dist,
                             "low_items": sum(1 for m in means if m < 2.5),     # (Most raters said it fails.)
                             "high_items": sum(1 for m in means if m >= 4),
                             "by_rater": {r["name"]: r3(statistics.fmean([r["scores"][i][a] for i in mine
                                                                         if a in r["scores"].get(i, {})] or [0]))
                                          for r in raters}}
        diffs[a] = {}
        for other in ("shuffled", "old"):
            if "new" in by_axis[a] and other in by_axis[a]:
                na = [statistics.fmean(per_item[a][i]) for i in items if key[i]["source"] == "new" and per_item[a][i]]
                nb = [statistics.fmean(per_item[a][i]) for i in items if key[i]["source"] == other and per_item[a][i]]
                diffs[a][f"new_minus_{other}"] = {"diff": r3(by_axis[a]["new"]["mean"] - by_axis[a][other]["mean"]),
                                                  "ci95": [r3(x) for x in bootstrap(na, nb)]}
    agreement = {}
    if len(raters) >= 2:
        for a in axes:
            units = [per_item[a][i] for i in items]
            pairs, exact, within = [], [], []
            for x in range(len(raters)):
                for y in range(x + 1, len(raters)):
                    both = [i for i in items if a in raters[x]["scores"].get(i, {}) and a in raters[y]["scores"].get(i, {})]
                    xs = [raters[x]["scores"][i][a] for i in both]
                    ys = [raters[y]["scores"][i][a] for i in both]
                    if rho := spearman(xs, ys):
                        pairs.append(rho)
                    exact += [p == q for p, q in zip(xs, ys)]
                    within += [abs(p - q) <= 1 for p, q in zip(xs, ys)]
            agreement[a] = {"alpha_interval": r3(krippendorff(units, "interval")),
                            "alpha_ordinal": r3(krippendorff(units, "ordinal")),
                            "spearman_pairwise_mean": r3(statistics.fmean(pairs)) if pairs else None,
                            "exact_pct": r3(100.0 * sum(exact) / len(exact)) if exact else None,
                            "within_one_pct": r3(100.0 * sum(within) / len(within)) if within else None}
    mean = {s: v["mean"] for s, v in by_axis.get(main, {}).items()}
    h1 = {"axis": main, "mean": mean, "rated": {s: v["ratings"] for s, v in by_axis.get(main, {}).items()},
          "targets": TARGETS, "by_axis": by_axis, "diffs": diffs}
    for name, d in diffs.get(main, {}).items():
        h1[name] = d["diff"]
    met = {}
    if "new" in mean:
        met["new"] = mean["new"] >= TARGETS["new"]
    for name in ("new_minus_shuffled", "new_minus_old"):
        if name in h1:
            met[name] = h1[name] >= TARGETS[name]
    h1["met"] = met
    h1["passed"] = bool(met) and all(met.values())
    # What goes with a low rating (on the main axis), read from the runs: each source by length, and the new source by
    # how its talks ended and by the moves they made (a move in 3 items or more: the items' mean with it and without).
    runs: dict = {}
    spoken = {i: conversation(key[i], runs) for i in items}
    item_mean = {i: statistics.fmean(per_item[main][i]) for i in items if per_item[main][i]}

    def split(group: dict) -> dict:
        return {k: {"items": len(v), "mean": r3(statistics.fmean(v))} for k, v in group.items() if v}

    by_length = {}
    for s in sources:
        group: dict[str, list] = {}
        for i in items:
            if key[i]["source"] == s and i in item_mean and spoken[i]:
                n = len(spoken[i])
                group.setdefault("2" if n <= 2 else "3" if n == 3 else "4-5" if n <= 5 else "6+", []).append(item_mean[i])
        if group:
            by_length[s] = split(dict(sorted(group.items())))
            # The packet samples lengths evenly; a listener hears them as the run spoke them. Weighted so (over the
            # lengths rated), the source's mean is what its run's talk would score.
            heard: dict[str, int] = {}
            for where in {(key[i].get("run", ""), s == "shuffled") for i in items if key[i]["source"] == s}:
                if runs.get(where) is not None:
                    for c in runs[where].by_conv.values():
                        if len(c) >= 2:
                            b = "2" if len(c) <= 2 else "3" if len(c) == 3 else "4-5" if len(c) <= 5 else "6+"
                            heard[b] = heard.get(b, 0) + 1
            total = sum(heard.get(b, 0) for b in by_length[s])
            if total:
                for b, v in by_length[s].items():
                    v["heard_pct"] = r3(100.0 * heard.get(b, 0) / total)
                h1.setdefault("weighted_by_heard_length", {})[s] = r3(
                    sum(v["mean"] * heard.get(b, 0) for b, v in by_length[s].items()) / total)
    h1["by_length"] = by_length
    new_items = [i for i in items if key[i]["source"] == "new" and i in item_mean and spoken[i]]
    if new_items:
        ended: dict[str, list] = {}
        for i in new_items:
            ended.setdefault(spoken[i][0].get("conv", {}).get("ended") or "unmarked", []).append(item_mean[i])
        h1["new_by_ending"] = split(ended)
        moves = sorted({m for i in new_items for l in spoken[i] for m in l["moves"]})
        h1["new_by_move"] = {}
        for m in moves:
            with_it = [item_mean[i] for i in new_items if any(m in l["moves"] for l in spoken[i])]
            without = [item_mean[i] for i in new_items if not any(m in l["moves"] for l in spoken[i])]
            if len(with_it) >= 3 and without:
                h1["new_by_move"][m] = {"items": len(with_it), "with": r3(statistics.fmean(with_it)),
                                        "without": r3(statistics.fmean(without))}
    writer: dict[str, list] = {}
    for r in raters:
        for i, w in r["writer"].items():
            if i in key:
                writer.setdefault(key[i]["source"], []).append(w)
    out = {"raters": [r["name"] for r in raters], "items": len(items), "axes": axes, "H1": h1,
           "agreement": agreement,
           "H2": {s: r3(100.0 * sum(v) / len(v)) for s, v in writer.items() if v} or "not rated"}
    if worst:
        ranked = sorted((i for i in items if key[i]["source"] == "new" and per_item[main][i]),
                        key=lambda i: (statistics.fmean(per_item[main][i]),
                                       statistics.fmean(per_item.get("coherent", per_item[main])[i] or [0]), int(i)))
        cut = ranked[:worst]
        if cut:                                     # (A tie at the last place is listed whole.)
            last = statistics.fmean(per_item[main][cut[-1]])
            cut += [i for i in ranked[worst:] if statistics.fmean(per_item[main][i]) == last]
        out["worst"] = [{"item": i, "conv": key[i].get("conv"), "run": key[i].get("run"),
                         "mean": {a: r3(statistics.fmean(per_item[a][i])) for a in axes if per_item[a][i]},
                         "notes": {r["name"]: r["notes"][i] for r in raters if i in r["notes"]},
                         "lines": spoken[i]} for i in cut]
    return out


def report(out: dict) -> str:
    h1 = out["H1"]
    names = {"new": "new (this slice)", "old": "old (the baseline)", "shuffled": "shuffled (the control)"}
    lines = [f"# H1: blind ratings", "",
             f"{len(out['raters'])} raters, {out['items']} conversations; axes {', '.join(out['axes'])} (H1 judged on "
             f"`{h1['axis']}`). Targets: new {TARGETS['new']}+, new - shuffled {TARGETS['new_minus_shuffled']}+, new - old "
             f"{TARGETS['new_minus_old']}+. " + ("**Met.**" if h1["passed"] else "**Not met** (" + ", ".join(
                 k for k, v in h1["met"].items() if not v) + ")."), ""]
    for a in out["axes"]:
        table = h1["by_axis"][a]
        lines += [f"## {a}", "", "| source | mean | item sd | median | below 2.5 | 4+ | scores 1..5 | by rater |",
                  "|---|---|---|---|---|---|---|---|"]
        for s, v in table.items():
            dist = " ".join(f"{k}:{n}" for k, n in v["dist"].items())
            raters = " / ".join(f"{x:g}" for x in v["by_rater"].values())
            lines.append(f"| {names.get(s, s)} | **{v['mean']:.2f}** | {v['item_sd']:.2f} | {v['item_median']:.2f} | "
                         f"{v['low_items']} of {v['items']} | {v['high_items']} | {dist} | {raters} |")
        for name, d in h1["diffs"].get(a, {}).items():
            lines.append(f"\n{name.replace('_', ' ')}: **{d['diff']:+.2f}** (95% {d['ci95'][0]:+.2f} to {d['ci95'][1]:+.2f})")
        if a in out["agreement"]:
            g = out["agreement"][a]
            lines.append(f"\nagreement: alpha interval {g['alpha_interval']}, ordinal {g['alpha_ordinal']}; Spearman "
                         f"{g['spearman_pairwise_mean']}; exact {g['exact_pct']:.1f}%, within one {g['within_one_pct']:.1f}%")
        lines.append("")
    if h1.get("by_length"):
        lines += [f"## What goes with the rating (`{h1['axis']}`)", "", "By length (lines; the items rated, and the "
                  "share of the run's talk of 2 lines or more at that length): " + "; ".join(
            f"{names.get(s, s)} " + ", ".join(f"{k}: {v['mean']:.2f} ({v['items']}; {v.get('heard_pct', 0):.0f}%)"
                                              for k, v in g.items()) for s, g in h1["by_length"].items()) + "."]
        if h1.get("weighted_by_heard_length"):
            lines.append("\nWeighted to the lengths each run spoke (the packet samples lengths evenly): " + ", ".join(
                f"{names.get(s, s)} {v:.2f}" for s, v in h1["weighted_by_heard_length"].items()) + ".")
        if h1.get("new_by_ending"):
            lines.append("\nThe new source by how it ended: " + ", ".join(
                f"{k} {v['mean']:.2f} ({v['items']})" for k, v in h1["new_by_ending"].items()) + ".")
        if h1.get("new_by_move"):
            lines += ["", "| move (new source) | items | with it | without |", "|---|---|---|---|"]
            for m, v in sorted(h1["new_by_move"].items(), key=lambda x: x[1]["with"] - x[1]["without"]):
                lines.append(f"| {m} | {v['items']} | {v['with']:.2f} | {v['without']:.2f} |")
        lines.append("")
    if out.get("worst"):
        lines += ["## The new source's lowest", ""]
        for w in out["worst"]:
            mean = ", ".join(f"{a} {v:.2f}" for a, v in w["mean"].items())
            lines += [f"### {w['item']} (conversation {w['conv']}; {mean})", ""]
            for l in w["lines"]:
                lines.append(f"> **{l['who']}:** {l['text']}  ")
                lines.append(f"> `{l['entry']}` {' + '.join(l['moves'])}" + (f", owes {' | '.join(l['owes'])}" if l['owes'] else "") + "  ")
            if w["lines"] and w["lines"][0].get("conv"):
                c = w["lines"][0]["conv"]
                lines.append(f"\n{', '.join(f'{k}: {v}' for k, v in c.items() if k != 'chain')}; chain {' > '.join(c.get('chain', []))}")
            for who, note in w["notes"].items():
                lines.append(f"- {who}: {note}")
            lines.append("")
    return "\n".join(lines) + "\n"


def score(args) -> int:
    first = Path(args.ratings[0])
    key = json.loads((args.key or first.parent / "key.json").read_text())["items"]
    raters = read_raters(args.ratings)
    if not raters or not any(r["scores"] for r in raters):
        print("talk_rate: no ratings read", file=sys.stderr)
        return 1
    unknown = sorted({i for r in raters for i in r["scores"] if i not in key})
    if unknown:
        print(f"talk_rate: items not in the key: {', '.join(unknown[:10])}", file=sys.stderr)
        return 1
    out = score_ratings(key, raters, args.worst)
    text = json.dumps(out, indent=1, ensure_ascii=False)
    if args.json:
        args.json.write_text(text + "\n")
    if args.report:
        args.report.write_text(report(out))
    print(report(out) if args.report else text)
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
    s.add_argument("ratings", type=Path, nargs="+", help="ratings.csv (one rater) or JSON (one rater or several)")
    s.add_argument("--key", type=Path)
    s.add_argument("--json", type=Path)
    s.add_argument("--report", type=Path, help="the same as Markdown")
    s.add_argument("--worst", type=int, default=5, help="the new source's lowest N, with notes and lines (0: none)")
    args = parser.parse_args()
    return packet(args) if args.command == "packet" else score(args)


if __name__ == "__main__":
    raise SystemExit(main())
