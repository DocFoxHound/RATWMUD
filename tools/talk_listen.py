#!/usr/bin/env python3
"""Listening to talk_sim's transcripts (Docs/Design/84-living-conversations.md §15.6): one self-contained page, and a
Markdown listen sheet.

  python3 tools/talk_listen.py RUN_DIR [RUN_DIR ...] --html OUT.html [--packet 60] [--seed N] [--title T]
  python3 tools/talk_listen.py RUN_DIR [RUN_DIR ...] --md OUT.md [--packet 60]

The page lays conversations out like the story log, with chips for the speakers (name, job, where they grew up, accent);
hovering a line shows where it came from (its rule or scene, the alternative, the grammar layers whose words filled it,
its habits, its moves and what it owed, once those exist). Each conversation has fine / off / wrong buttons and a note;
"Export marks" saves them as JSON, to be filed under artifacts/talk/marks/ (a mark names the run, the conversation and
each line's entry, so a fix finds its rule; a mantle's line will name the mantle and its item's key).

--packet N samples N conversations spread over dialect x pair kind (family, friends, acquaintances, strangers...) x topic
family (news or everyday) x length, so a listen covers the kinds of talk rather than the commonest; without it, every
conversation is on the page.
"""
from __future__ import annotations

import argparse
import collections
import html
import json
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import talk_score  # noqa: E402

PAGE = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>{title}</title>
<style>
:root {{ --bg:#f6f3ee; --card:#fffdf9; --ink:#2b2622; --muted:#7a7068; --line:#e3dcd2; --chip:#efe7dc; --fine:#2f7d4f;
        --off:#b7791f; --wrong:#b23b3b; --accent:#5a4a8a; }}
@media (prefers-color-scheme: dark) {{ :root:not([data-theme="light"]) {{ --bg:#1d1b19; --card:#26231f; --ink:#ece6dd;
        --muted:#a39a8f; --line:#3a352f; --chip:#332e28; --accent:#a99be0; }} }}
:root[data-theme="dark"] {{ --bg:#1d1b19; --card:#26231f; --ink:#ece6dd; --muted:#a39a8f; --line:#3a352f; --chip:#332e28;
        --accent:#a99be0; }}
* {{ box-sizing:border-box; }}
body {{ margin:0; background:var(--bg); color:var(--ink); font:15px/1.5 Georgia, 'Iowan Old Style', serif; }}
main {{ max-width:860px; margin:0 auto; padding:24px 16px 80px; }}
h1 {{ font-size:22px; margin:0 0 4px; }} .sub {{ color:var(--muted); font-size:13px; margin-bottom:18px; }}
.bar {{ position:sticky; top:0; background:var(--bg); padding:8px 0; display:flex; gap:8px; flex-wrap:wrap; z-index:2;
        border-bottom:1px solid var(--line); margin-bottom:12px; font:13px system-ui, sans-serif; }}
button {{ font:13px system-ui, sans-serif; border:1px solid var(--line); background:var(--card); color:var(--ink);
          border-radius:6px; padding:4px 10px; cursor:pointer; }}
.conv {{ background:var(--card); border:1px solid var(--line); border-radius:10px; padding:12px 14px; margin:12px 0; }}
.head {{ font:12px system-ui, sans-serif; color:var(--muted); margin-bottom:8px; display:flex; flex-wrap:wrap; gap:6px; }}
.chip {{ background:var(--chip); border-radius:999px; padding:1px 8px; }}
.line {{ margin:4px 0; position:relative; }}
.who {{ font-weight:600; color:var(--accent); }}
.line .why {{ display:none; position:absolute; left:0; top:100%; z-index:3; background:var(--ink); color:var(--bg);
              font:12px/1.4 ui-monospace, monospace; padding:6px 8px; border-radius:6px; max-width:100%; }}
.line:hover .why, .line:focus .why {{ display:block; }}
.marks {{ display:flex; gap:6px; align-items:center; margin-top:8px; flex-wrap:wrap; }}
.marks button.on[data-m="fine"] {{ background:var(--fine); color:#fff; }}
.marks button.on[data-m="off"] {{ background:var(--off); color:#fff; }}
.marks button.on[data-m="wrong"] {{ background:var(--wrong); color:#fff; }}
.marks input {{ flex:1; min-width:160px; font:13px system-ui, sans-serif; padding:4px 6px; border:1px solid var(--line);
                border-radius:6px; background:var(--bg); color:var(--ink); }}
</style></head><body><main>
<h1>{title}</h1><div class="sub">{subtitle}</div>
<div class="bar"><span id="count"></span><button onclick="exportMarks()">Export marks</button>
<button onclick="document.documentElement.dataset.theme = document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark'">Light / dark</button></div>
{body}
</main>
<script>
const KEY = {key};
let marks = {{}};
try {{ marks = JSON.parse(localStorage.getItem(KEY) || '{{}}'); }} catch (e) {{ marks = {{}}; }}
function save() {{ try {{ localStorage.setItem(KEY, JSON.stringify(marks)); }} catch (e) {{}} count(); }}
function count() {{ const n = Object.values(marks).filter(m => m.mark).length;
  document.getElementById('count').textContent = n + ' of {n} marked'; }}
document.querySelectorAll('.conv').forEach(c => {{
  const id = c.dataset.id, m = marks[id] || {{}};
  c.querySelectorAll('.marks button').forEach(b => {{
    if (m.mark === b.dataset.m) b.classList.add('on');
    b.onclick = () => {{ marks[id] = marks[id] || {{}}; marks[id].mark = marks[id].mark === b.dataset.m ? '' : b.dataset.m;
      c.querySelectorAll('.marks button').forEach(x => x.classList.toggle('on', x.dataset.m === marks[id].mark)); save(); }};
  }});
  const input = c.querySelector('.marks input'); input.value = m.note || '';
  input.oninput = () => {{ marks[id] = marks[id] || {{}}; marks[id].note = input.value; save(); }};
}});
function exportMarks() {{
  const out = [];
  document.querySelectorAll('.conv').forEach(c => {{ const m = marks[c.dataset.id];
    if (m && (m.mark || m.note)) out.push({{ run: c.dataset.run, conv: Number(c.dataset.conv), mark: m.mark || '',
      note: m.note || '', entries: JSON.parse(c.dataset.entries) }}); }});
  const blob = new Blob([JSON.stringify({{ page: KEY, marks: out }}, null, 1)], {{ type: 'application/json' }});
  const a = document.createElement('a'); a.href = URL.createObjectURL(blob); a.download = KEY + '-marks.json'; a.click();
}}
count();
</script></body></html>
"""


def speaker_chip(r: dict) -> str:
    name = r.get("name") or r.get("id", "?")
    job = r.get("_work_label") if r.get("_work_label") not in (None, "", "-") else r.get("job", "")
    origin = r.get("from", "") or "?"
    accent = r.get("accent", "")
    accent = "no" if accent in ("", "none") else accent
    return f"{name}, {job or 'no work'}; from {origin.title()}, {accent} accent"


def family(topic: str) -> str:
    return "news" if topic in talk_score.NEWSY else "everyday"


def conversations(runs: list[talk_score.Run]):
    """(run, head, lines) for every conversation heard."""
    for run in runs:
        for cid in sorted(run.by_conv):
            lines = run.by_conv[cid]
            if lines:
                yield run, run.conv_head.get(cid, {"conv": cid}), lines


def packet(convs: list, n: int, seed: int) -> list:
    """N conversations spread over strata: dialect x pair kind x topic family x length."""
    rng = random.Random(seed)
    strata = collections.defaultdict(list)
    for run, head, lines in convs:
        first = run.residents.get(lines[0]["who"], {})
        key = (first.get("_dialect", ""), head.get("facts", {}).get("pair.band", ""), family(head.get("topic", "")),
               min(len(lines), 5))
        strata[key].append((run, head, lines))
    for v in strata.values():
        rng.shuffle(v)
    keys = sorted(strata)
    rng.shuffle(keys)
    out = []
    while len(out) < n and any(strata[k] for k in keys):
        for k in keys:
            if strata[k] and len(out) < n:
                out.append(strata[k].pop())
    out.sort(key=lambda x: (label(x[0]), x[1].get("conv", 0)))
    return out


def label(run) -> str:
    """A run's own name in the page and its marks: talk_sim's --run (frozen-L1, fixtures-L1), never only the profile, which
    both modes share (a mark on frozen-L1's conversation 12 isn't one on fixtures-L1's)."""
    return getattr(run, "label", None) or run.meta.get("run") or run.dir.name


def labels(runs: list) -> None:
    """Each run's label, unique among those given (two runs of one name: the folder's name after it)."""
    seen = collections.Counter(run.meta.get("run") or run.dir.name for run in runs)
    for run in runs:
        name = run.meta.get("run") or run.dir.name
        run.label = name if seen[name] == 1 else f"{name}:{run.dir.parent.name}/{run.dir.name}"


def render_html(chosen: list, title: str, subtitle: str) -> str:
    body = []
    for run, head, lines in chosen:
        people = []
        for l in lines:
            if l["who"] not in people:
                people.append(l["who"])
        chips = "".join(f'<span class="chip">{html.escape(speaker_chip(run.residents.get(p, {"id": p})))}</span>' for p in people)
        facts = head.get("facts", {})
        where = f"{facts.get('here.place', '')} in {head.get('cell', '')}, {facts.get('here.region', '')}; " \
                f"day {head.get('day', 0):.2f}, hour {head.get('hour', 0):.0f}; {facts.get('_weather', '')}"
        meta = f'<span class="chip">{html.escape(label(run))} #{head.get("conv")}</span><span class="chip">topic ' \
               f'{html.escape(head.get("topic", ""))}</span><span class="chip">{html.escape(facts.get("pair.band", ""))}</span>' \
               f'<span class="chip">{html.escape(where)}</span><span class="chip">ended: {html.escape(head.get("reason", ""))}</span>'
        rows = []
        for l in lines:
            who = run.residents.get(l["who"], {}).get("name", l["who"])
            why = (f"entry {l.get('entry')} (alternative {l.get('alt')}); layers {', '.join(l.get('used', [])) or 'none'}; "
                   f"habits {', '.join(l.get('habits', [])) or 'none'}; moves {', '.join(l.get('moves', [])) or '(Phase 2)'}"
                   f"; owed {', '.join(l.get('owes', [])) or '-'}; dialect {l.get('dialect')}, {l.get('accent')}")
            rows.append(f'<div class="line" tabindex="0"><span class="who">{html.escape(who)}:</span> '
                        f'{html.escape(l["text"])}<div class="why">{html.escape(why)}</div></div>')
        entries = json.dumps([l.get("entry", "") for l in lines])
        cid = f"{label(run)}-{head.get('conv')}"
        body.append(f'<section class="conv" data-id="{html.escape(cid)}" data-run="{html.escape(label(run))}" '
                    f'data-conv="{head.get("conv")}" data-entries=\'{html.escape(entries)}\'><div class="head">{meta}'
                    f'</div><div class="people head">{chips}</div>{"".join(rows)}<div class="marks">'
                    f'<button data-m="fine">fine</button><button data-m="off">off</button><button data-m="wrong">wrong'
                    f'</button><input placeholder="A note (what\'s off, what it should be)"></div></section>')
    key = json.dumps("talk-listen-" + "".join(c if c.isalnum() else "-" for c in title.lower())[:60])
    return PAGE.format(title=html.escape(title), subtitle=html.escape(subtitle), body="\n".join(body), key=key, n=len(chosen))


def render_md(chosen: list, title: str, subtitle: str) -> str:
    out = [f"# {title}", "", subtitle, ""]
    for i, (run, head, lines) in enumerate(chosen, 1):
        facts = head.get("facts", {})
        out.append(f"## {i}. {head.get('topic', '')}, {facts.get('pair.band', '')}: {facts.get('here.place', '')} in "
                   f"`{head.get('cell', '')}` ({facts.get('here.region', '')}), day {head.get('day', 0):.1f}, hour "
                   f"{head.get('hour', 0):.0f}, {facts.get('_weather', '')}  ")
        out.append(f"`{label(run)}` conversation {head.get('conv')}; ended: {head.get('reason', '')}")
        out.append("")
        people = []
        for l in lines:
            if l["who"] not in people:
                people.append(l["who"])
        for p in people:
            out.append(f"- *{speaker_chip(run.residents.get(p, {'id': p}))}*")
        out.append("")
        for l in lines:
            who = run.residents.get(l["who"], {}).get("name", l["who"])
            out.append(f"> **{who}:** {l['text']}  ")
            out.append(f"> <sub>`{l.get('entry')}` alt {l.get('alt')}"
                       + (f", {'/'.join(l.get('used', []))}" if l.get("used") else "") + "</sub>  ")
        out.append("")
        out.append("Mark: fine / off / wrong. Note:")
        out.append("")
    return "\n".join(out) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("runs", nargs="+", type=Path)
    parser.add_argument("--html", type=Path)
    parser.add_argument("--md", type=Path)
    parser.add_argument("--packet", type=int, default=0)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--title", default="Residents' talk: a listen")
    parser.add_argument("--subtitle", default="")
    args = parser.parse_args()
    runs = [talk_score.Run(d) for d in args.runs]
    labels(runs)
    convs = list(conversations(runs))
    chosen = packet(convs, args.packet, args.seed) if args.packet else convs
    subtitle = args.subtitle or (f"{len(chosen)} conversations of {len(convs)} from {', '.join(label(r) for r in runs)} "
                                 "(talk_sim, Docs/Design/84 §15.6). Hover a line for where it came from.")
    if args.html:
        args.html.write_text(render_html(chosen, args.title, subtitle))
    if args.md:
        args.md.write_text(render_md(chosen, args.title, subtitle))
    if not (args.html or args.md):
        parser.error("give --html and/or --md")
    print(f"talk_listen: {len(chosen)} conversations", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
