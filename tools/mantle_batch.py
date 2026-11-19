#!/usr/bin/env python3
"""The mantle writer's batch helper (Docs/Mantle-writing-handbook.md; Docs/Design/84-living-conversations.md §14, P14/P15):
what is left to write, the next batch's briefs and ids, the checks G2 doesn't make, and every per-batch gate in one run.
Only reads the world (one read-only transaction); writes only under the --out folder it is given and a temporary folder.

  python3 tools/mantle_batch.py facts [--world ridgemere3] [--out Saved/Mantles/residents.jsonl]
      Every resident's facts as the game sees them (talk_check --world W --residents --json, a scratch world read only):
      the --facts file every other command (and mantle_brief, mantle_check) can take, so nobody waits a minute per call.
  python3 tools/mantle_batch.py todo [--world ridgemere3] [--facts F] [--town T] [--quarter Q] [--dialect D] [--limit N] [--json]
      Placed, living residents who wear no mantle and have none written for them (in the world's rows, any state, or in a
      file under Data/Talk/mantles-export or any --dir), most heard first (artifacts/talk/most-heard*.tsv; the unranked,
      such as pups born in the game, last), with their town, quarter, dialect, accents, tier, stage and work.
  python3 tools/mantle_batch.py plan --name BATCH --town T[,T...] [--quarter Q[,Q]] [--dialect D] [--size N] [--world ridgemere3]
                                    [--facts F] [--out Saved/Mantles/BATCH]
      The next batch: the first N of `todo` for those towns (and quarters: 'none' for Ridgemere residents with none; or
      dialects; each a comma list), most heard first, each given the next free mantle id of
      its dialect, and for each a brief (tools/mantle_brief.py's, with the id, export path, sizes and, for plain speech, the
      plain grammar's symbols added) and a starting file (the header filled from the facts; every TODO must go). Writes
      OUT/batch.tsv, OUT/briefs/<id>.md, OUT/start/<id>.mantle and OUT/index-<town>.md (tools/mantle_index.py's). Nothing
      is written to the export or the world: copy a starting file to its export path and write it there.
  python3 tools/mantle_batch.py lint PATH... [--world ridgemere3] [--facts F] [--json]
      What G1 (talk_check --mantle) and G2 (mantle_check) don't catch, by mantle and item key. Refused: every word of the
      deity family (godsend, godly...); "they" of one named wolf (talk_lint's they_named); a story's `ask :` that asks
      the teller about the story ("Did you catch one?": the listener's question, which belongs in the `b` line after it);
      TODO left; a header that doesn't fit its wearer (dialect, accents, honesty kept, all seven axes, fits their stage,
      a pup's sections); a scene line whose alternatives don't line up with its threads; a decline, refusal or doubt of
      fewer than six words with no hedge. Warned: a scene thread ending on a question; an age claim with no age gate;
      the brief's own words copied (its reasons and likes go to every writer of the town); stock lines ("Thank you for
      telling me"); too many alternatives of three words or fewer; a tier's parts under the pilot's counts; a file away
      from its export path. (Until fix 8 the lint also ran G2's rules over tagged scene lines, refused a pronoun in the
      persona and a window line that would hide its node's facts: G2 makes those checks now, so mantle_load does too.)
  python3 tools/mantle_batch.py gates PATH... [--world ridgemere3] [--facts F] [--no-g3]
      Every per-batch gate on the files named (a directory: every .mantle under it): G1, G2 --strict over each touched
      dialect's whole folder (so a 5-word run shared with any neighbour shows), the lint above, and G3 (talk_check
      --mantles-from a copy of just these files --mantle-coverage). Exit 1 if any refuses.
  python3 tools/mantle_batch.py pools PATH... [--facts F]
      For spares (P15), from the files alone: each dialect's spares' share of sly and guarded honesty against its living
      residents' (within 5 points, doc 84 §9.5). `mantle_check.py --pools --world ridgemere3 [PATH...]` is the gate: the
      same honesty test on the rows and the drafts together, and each pool's health over five forecast years. What a
      spare must be (no written_for, a fit by stage only, young, adolescent and grown sections) is the lint's.

The world is DEV, `ridgemere3` (ratw_ridgemere3). A live world is refused. talk_check is $RATW_TALK_CHECK, else the newest
build*/talk_check of this checkout, else of the main checkout (a worktree has no build of its own); it is always given
this checkout's Data/Talk (--data), so a worktree checks its own files.
"""
from __future__ import annotations

import argparse
import collections
import csv
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
TALK = ROOT / 'Data' / 'Talk'
EXPORT = TALK / 'mantles-export'
sys.path.insert(0, str(HERE))
import dialect_check  # noqa: E402
import mantle_check as C  # noqa: E402
import mantle_text as T  # noqa: E402
import talk_lint  # noqa: E402

WORLD = 'ridgemere3'
PREFIX = {'plain': 'rm', 'coldwater': 'cw', 'larkfield': 'lf', 'southmeadow': 'sm', 'oakhaven': 'oh', 'frosthollow': 'fh',
          'mirebridge': 'mb', 'saltwell': 'sw'}
AXES = ('temper', 'talk', 'warmth', 'humour', 'honesty', 'piety', 'drive')
# The pilot's counts (doc 84 §40: the medians of its 8 full and 35 standard mantles, a little under), the floor a batch
# is held to beside §9.3's item totals (92 / 55 / 64, which G2 --strict refuses under).
TARGETS = {'full': {'scene': 27, 'story': 5, 'opinion': 8, 'line': 30, 'rule': 6, 'dlg': 12, 'graft': 2, 'symbol': 16},
           'standard': {'scene': 17, 'story': 3, 'opinion': 5, 'line': 18, 'rule': 4, 'dlg': 4, 'graft': 1, 'symbol': 12},
           'spare': {'scene': 18, 'story': 3, 'opinion': 5, 'line': 18, 'rule': 4, 'dlg': 4, 'graft': 1, 'symbol': 12}}
DEITY = dialect_check.DEITY
# A story's `ask :` that asks the teller about the story (the listener's question, misplaced: fix 8, cw102 and 180 more):
# "Did you catch one?", "Were you hurt?", "What did you do?". (Not "Did you ever...?", "Would you have...?", "Do you know...?":
# those ask the listener.)
LISTENERS_ASK = re.compile(r"^(?:and )?(?:(?:did|were|weren't|was) you\b(?! ever)|what did you\b|where were you\b|how did you\b|"
                           r"who taught you\b|was (?:your|that your)\b|is that why you\b|did (?:she|he|they|it|the \w+|something) "
                           r"[^?]*\byou(?:r|rs)?\b)", re.I)
DISPREFERRED = ('decline', 'refuse', 'disagree', 'react.news.doubt', 'react.complaint.disagree', 'decline_apology')
HEDGES = re.compile(r'#(hedge|account|excuse|demur|doubt|refuse|sorry|put_off|concede|dunno|pardon)(\.[a-z]+)*#')
STOCK = re.compile(r"^(thank you for telling me|is that so|i see|indeed|how interesting|good|very good|interesting|"
                   r"that is interesting|i understand|of course|yes|no|okay|ok)[.!?]*$", re.I)
AGE_CLAIM = re.compile(r"\b(?:(?:twenty|thirty|forty|fifty|sixty|seventy|eighty|ninety|a hundred|\d{2,3})(?:-\w+)? "
                       r"(?:years|winters|summers|springs|autumns|seasons))\b|\bwhen i was (?:twenty|thirty|forty|fifty|sixty|"
                       r"seventy|eighty|\d{2})\b|\bin (?:all )?my (?:\w+ )?years\b|\bat my age\b", re.I)
WORD = re.compile(r"[a-z']+")


# ------------------------------------------------------------------ The world, read only

def world_db():
    import world_db as W
    return W


def resolve(world: str) -> str:
    W = world_db()
    key = W.resolve(world)
    if W.kind(key) != 'dev':
        raise SystemExit(f'mantle_batch: {key} is a {W.kind(key)} world; mantles are written in DEV (ridgemere3) only')
    if key != WORLD:
        print(f'(mantle_batch: world {key}, not {WORLD}: is that meant?)', file=sys.stderr)
    return key


def read_only(world: str):
    W = world_db()
    conn = W.connect(resolve(world), 'editor', autocommit=False)
    conn.execute('SET TRANSACTION READ ONLY')
    return conn


def talk_check() -> Path:
    found = os.environ.get('RATW_TALK_CHECK')
    if found:
        return Path(found)
    roots = [ROOT]
    try:
        common = subprocess.run(['git', '-C', str(ROOT), 'rev-parse', '--git-common-dir'], capture_output=True, text=True,
                                check=True).stdout.strip()
        main = (ROOT / common).resolve().parent if not Path(common).is_absolute() else Path(common).resolve().parent
        if main != ROOT:
            roots.append(main)
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    for r in roots:
        built = sorted(r.glob('build*/talk_check'), key=lambda p: p.stat().st_mtime, reverse=True)
        if built:
            return built[0]
    raise SystemExit('mantle_batch: talk_check is not built (cmake --build build-core --target talk_check in the main '
                     'checkout), or set RATW_TALK_CHECK')


def read_facts(path: str | None, world: str) -> dict[str, dict]:
    if path:
        lines = Path(path).read_text().splitlines()
    else:
        print('(mantle_batch: no --facts; asking talk_check for every resident, about 15 s; `facts` saves them)',
              file=sys.stderr)
        run = subprocess.run([str(talk_check()), '--data', str(TALK), '--world', resolve(world), '--residents', '--json'],
                             capture_output=True, text=True, timeout=900)
        if run.returncode != 0:
            raise SystemExit('mantle_batch: talk_check failed:\n' + run.stderr[-2000:])
        lines = run.stdout.splitlines()
    out = {}
    for line in lines:
        if line.startswith('{'):
            r = json.loads(line)
            out[r['id']] = r
    return out


def mantle_files(dirs: list[Path]) -> list[Path]:
    out = []
    for d in dirs:
        out += sorted(d.rglob('*.mantle')) if d.is_dir() else ([d] if d.suffix == '.mantle' else [])
    return out


def read_mantle(path: Path, text: str | None = None) -> T.Mantle | None:
    try:
        return T.read(text if text is not None else path.read_text(), str(path))
    except T.MantleError as e:
        print(f'(unreadable, skipped: {path}: {e.problems[0]})', file=sys.stderr)
        return None


def most_heard() -> dict[str, int]:
    files = sorted((ROOT / 'artifacts' / 'talk').glob('most-heard*.tsv'), key=lambda p: p.stat().st_mtime)
    if not files:
        return {}
    rows = csv.DictReader((l for l in files[-1].read_text().splitlines() if not l.startswith('#')), delimiter='\t')
    return {r['id']: int(r['rank']) for r in rows if r.get('rank', '').isdigit()}


def tier_of(r: dict) -> str:
    """mantle_brief's rule (doc 84 §9.3): merchants, innkeepers, priests, guards, masters and House family are full."""
    return 'full' if r.get('role') == 'merchant' or r.get('job') in ('innkeeper', 'priest', 'guard') or \
        r.get('master') == 'yes' or r.get('house') else 'standard'


def accents_of(r: dict) -> str:
    a = r.get('accent') or 'none'
    return 'none' if a == 'none' else ('light broad' if a == 'light' else 'broad light')


def world_state(world: str, dirs: list[Path]) -> dict:
    """Who is placed and living, who wears what, which mantles exist (rows and files) and for whom."""
    with read_only(world) as conn:
        wid = C.world_id_of(conn)
        npcs = {r[0]: {'name': r[1], 'home': r[2] or '', 'age': r[3]} for r in conn.execute(
            'SELECT id, name, home_area, age FROM live.npcs WHERE world_id = %s', (wid,)).fetchall()}
        alive = dict(conn.execute('SELECT npc_id, alive FROM live.npc_state WHERE world_id = %s', (wid,)).fetchall())
        dead = {r[0] for r in conn.execute("SELECT key FROM game.npcs WHERE world_id = %s AND data->>'dead' = 'true'",
                                           (wid,)).fetchall()}
        quarter = dict(conn.execute('SELECT npc_id, quarter FROM live.npc_voice WHERE world_id = %s', (wid,)).fetchall())
        worn = dict(conn.execute('SELECT npc_id, mantle_id FROM live.npc_voice WHERE world_id = %s AND mantle_id IS NOT NULL '
                                 'AND released_day IS NULL', (wid,)).fetchall())
        rows = conn.execute('SELECT id, written_for, state FROM live.mantles WHERE world_id = %s', (wid,)).fetchall()
        region = dict(conn.execute('SELECT id, region FROM world.cells WHERE world_id = %s UNION ALL SELECT id, region FROM '
                                   'world.interiors WHERE world_id = %s', (wid, wid)).fetchall())
    for r in npcs.values():                     # (The town is the home's region, as mantle_brief and mantle_index read it:
        r['town'] = region.get(r['home']) or C.town_of(r['home'])   # a farm's fs_ home is in its town's region.)
    ids = {r[0] for r in rows}
    written = collections.defaultdict(set)
    for mid, wf, _ in rows:
        if wf:
            written[wf].add(mid)
    for f in mantle_files(dirs):
        m = read_mantle(f)
        if m:
            ids.add(m.id)
            if m.written_for:
                written[m.written_for].add(m.id)
    for f in mantle_files([ROOT / 'Tests' / 'fixtures' / 'mantles']):
        m = read_mantle(f)
        if m:
            ids.add(m.id)
    placed = {n for n, r in npcs.items() if r['home'] and alive.get(n, True) and n not in dead}
    return {'npcs': npcs, 'placed': placed, 'quarter': quarter, 'worn': worn, 'written': written, 'ids': ids}


def todo_rows(world: str, facts: dict, dirs: list[Path], town: str = '', quarter: str = '', dialect: str = '') -> list[dict]:
    st = world_state(world, dirs)
    rank = most_heard()
    out = []
    for npc in st['placed']:
        if npc in st['worn'] or st['written'].get(npc):
            continue
        f = facts.get(npc, {})
        n = st['npcs'][npc]
        row = {'rank': rank.get(npc, 9999), 'id': npc, 'name': n['name'], 'town': n['town'],
               'quarter': st['quarter'].get(npc) or '', 'dialect': f.get('dialect', '?'), 'accents': accents_of(f),
               'tier': tier_of(f), 'stage': f.get('stage', '?'), 'sex': f.get('sex', '?'), 'age': f.get('age', n['age']),
               'work': f.get('trade_words') or f.get('job') or '-'}
        if town and row['town'] not in town.split(','):
            continue
        if quarter and row['quarter'] not in quarter.split(',') and not ('none' in quarter.split(',') and not row['quarter']):
            continue
        if dialect and row['dialect'] not in dialect.split(','):
            continue
        out.append(row)
    out.sort(key=lambda r: (r['rank'], r['id']))
    return out


def next_ids(dialect: str, n: int, taken: set) -> list[str]:
    p = PREFIX[dialect]
    nums = [int(i[2:]) for i in taken if i.startswith(p) and i[2:].isdigit() and int(i[2:]) < 900]
    start = max([100] + nums) + 1
    return [f'{p}{start + k}' for k in range(n)]


# ------------------------------------------------------------------ facts, todo, plan

def cmd_facts(a) -> int:
    out = Path(a.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    run = subprocess.run([str(talk_check()), '--data', str(TALK), '--world', resolve(a.world), '--residents', '--json'],
                         capture_output=True, text=True, timeout=900)
    if run.returncode != 0:
        print(run.stderr[-2000:], file=sys.stderr)
        return 1
    lines = [l for l in run.stdout.splitlines() if l.startswith('{')]
    out.write_text('\n'.join(lines) + '\n')
    print(f'{len(lines)} residents\' facts -> {out}')
    return 0


def cmd_todo(a) -> int:
    facts = read_facts(a.facts, a.world)
    rows = todo_rows(a.world, facts, [EXPORT] + [Path(d) for d in a.dir], a.town, a.quarter, a.dialect)
    if a.json:
        print(json.dumps(rows[:a.limit or None], indent=1))
        return 0
    by = collections.Counter((r['town'], r['quarter'] or '-', r['dialect']) for r in rows)
    print(f'{len(rows)} placed, living residents wear no mantle and have none written'
          + (f' (town {a.town})' if a.town else '') + (f' (quarter {a.quarter})' if a.quarter else '')
          + (f' (dialect {a.dialect})' if a.dialect else ''))
    print('by town / quarter / dialect: ' + ', '.join(f'{t}/{q}/{d} {n}' for (t, q, d), n in sorted(by.items())))
    tiers = collections.Counter(r['tier'] for r in rows)
    print(f'tiers: full {tiers["full"]}, standard {tiers["standard"]}; pups and adolescents '
          f'{sum(r["stage"] in ("young", "adolescent") for r in rows)}')
    print('rank\tid\tname\ttown\tquarter\tdialect\taccents\ttier\tstage\tsex\tage\twork')
    for r in rows[:a.limit or None]:
        print('\t'.join(str(r[k]) for k in ('rank', 'id', 'name', 'town', 'quarter', 'dialect', 'accents', 'tier', 'stage',
                                              'sex', 'age', 'work')))
    return 0


def plain_symbols() -> str:
    names, seen = [], set()
    for raw in (TALK / 'grammar' / 'plain.grammar').read_text().splitlines():
        m = re.match(r'^([a-z0-9_]+)(\.broad)?(!{0,2})(\[[^\]]*\])?\s*\+?=', raw.strip())
        if m and not m.group(2) and m.group(1) not in seen:
            seen.add(m.group(1))
            names.append('#' + m.group(1) + '#')
    return ' '.join(n for n in names if not n.startswith('#bc_')) + '\nbackchannels: ' + \
        ' '.join(n for n in names if n.startswith('#bc_'))


def starting_file(mid: str, f: dict, npc: str, name: str, dialect: str, batch: str) -> str:
    traits = ' '.join(f'{ax}={f.get(ax, "plain" if ax == "honesty" else "")}' for ax in AXES)
    young = f.get('stage') in ('young', 'adolescent')
    prefers = []
    if f.get('role') == 'merchant':
        prefers.append('role=merchant')
    if f.get('job') not in (None, '', 'none', 'child'):
        prefers.append(f'job={f["job"]}')
    lines = [
        f'# {mid}, TODO title (doc 84 P14, batch {batch}): written for {npc} ({name}: {f.get("trade_words") or f.get("job") or "-"}, '
        f'{f.get("from_town") or "-"}-born). Name-free; every TODO must go. Window lines on the news, gossip, business, '
        f'shop and inn nodes say their case ([here.shop!=bare]...); a story\'s ask is the teller\'s question (handbook 4.5.6, 4.5.8).',
        f'mantle {mid}',
        'state ready',
        f'dialect {dialect}',
        f'accent {accents_of(f)}',
        f'tier {tier_of(f)}',
        f'written_for {npc}',
        'title TODO',
        'fits stage=young|adolescent|adult|old' if young else 'fits stage=adult|old',
        f'prefers {" ".join(prefers) or "TODO"}',
        f'traits {traits}',
        f'quirk {f.get("quirk") or "TODO"}',
        f'likes {f.get("likes") or "TODO"}',
        f'dislikes {f.get("dislikes") or "TODO"}',
        'fears TODO',
        'hopes TODO',
        'topics todo +0.0',
        'styles comfort=TODO gossip=TODO refuse=TODO tease=TODO',
        'marks oath saying',
        'persona #self.first# TODO (one line; no pronoun at all: a mantle passes to either sex)',
        '',
        'words',
        'oath! = TODO | TODO',
        'saying! = TODO | TODO',
        '',
        'any',
        '',
        'grown',
    ]
    if young:
        lines += ['', 'young', '', 'adolescent']
    return '\n'.join(lines) + '\n'


def cmd_plan(a) -> int:
    import mantle_brief as B
    import mantle_index as I
    facts = read_facts(a.facts, a.world)
    dirs = [EXPORT] + [Path(d) for d in a.dir]
    rows = todo_rows(a.world, facts, dirs, a.town, a.quarter, a.dialect)[:a.size]
    if not rows:
        print('nothing left to write there')
        return 0
    out = Path(a.out or ROOT / 'Saved' / 'Mantles' / a.name)
    (out / 'briefs').mkdir(parents=True, exist_ok=True)
    (out / 'start').mkdir(parents=True, exist_ok=True)
    st = world_state(a.world, dirs)
    taken = set(st['ids'])
    for d in ([Path(x) for x in a.dir] if a.dir else []):
        for f in mantle_files([d]):
            m = read_mantle(f)
            if m:
                taken.add(m.id)
    wrows = B.world_rows(resolve(a.world), [r['id'] for r in rows])
    table = ['id\tnpc\tname\tdialect\ttier\taccents\tstage\ttown\tquarter\trank\twork\texport_path']
    for r in rows:
        if r['dialect'] not in PREFIX:
            print(f'{r["id"]}: no dialect ({r["dialect"]}); skipped', file=sys.stderr)
            continue
        mid = next_ids(r['dialect'], 1, taken)[0]
        taken.add(mid)
        path = f'Data/Talk/mantles-export/{r["dialect"]}/{mid}.mantle'
        b = B.brief(resolve(a.world), r['id'], facts, wrows, 20)
        tier = tier_of(facts.get(r['id'], {}))
        t = TARGETS[tier]
        extra = [f'\n## This batch ({a.name})',
                 f'- mantle id **{mid}**, file `{path}` (start from `{out / "start" / (mid + ".mantle")}`)',
                 f'- tier **{tier}**: at least {92 if tier == "full" else 55} items that aren\'t symbols (G2 --strict); the '
                 f'pilot\'s counts: {t["symbol"]}+ words, {t["scene"]}+ scenes, {t["story"]} stories, {t["opinion"]}+ '
                 f'opinions, {t["line"]}+ lines, {t["rule"]} barks, {t["dlg"]}+ window lines, {t["graft"]} graft(s)',
                 f'- dialect **{r["dialect"]}**, accents `{r["accents"]}`'
                 + ('  (accent none: no dialect word written out anywhere; dialect only through symbols)' if r['accents'] == 'none' else '')]
        if r['dialect'] == 'plain':
            q = r['quarter'] or 'none'
            extra += [f'- plain speech (Ridgemere): no density rule; the register of the quarter ({q}): the Heights formal, the '
                      'Estate genteel, the Office clerkly, the Commons ordinary, the Works and the Sump rough, the Wharf salt',
                      '\n## The plain grammar\'s symbols (#symbol#; every one exists in every dialect)', plain_symbols()]
        (out / 'briefs' / f'{mid}.md').write_text(B.text(b) + '\n'.join(extra) + '\n')
        (out / 'start' / f'{mid}.mantle').write_text(starting_file(mid, facts.get(r['id'], {}), r['id'], r['name'],
                                                                   r['dialect'], a.name))
        table.append('\t'.join(str(x) for x in (mid, r['id'], r['name'], r['dialect'], tier, r['accents'], r['stage'],
                                                r['town'], r['quarter'] or '-', r['rank'], r['work'], path)))
    (out / 'batch.tsv').write_text('\n'.join(table) + '\n')
    towns = sorted({r['town'] for r in rows})
    for town in towns:
        mantles = I.read_dirs([EXPORT])
        tw = I.towns_of(resolve(a.world), mantles)
        chosen = sorted((m for mid, m in mantles.items() if tw.get(mid) == town), key=lambda m: m.id)
        (out / f'index-{town}.md').write_text(I.show(town, I.index(chosen)))
    print(f'{len(table) - 1} mantles planned -> {out}/batch.tsv; briefs in {out}/briefs, starting files in {out}/start, '
          f'the town index in {out}/index-<town>.md')
    print('\n'.join(table))
    return 0


# ------------------------------------------------------------------ lint

def turns(it: T.Item) -> list[tuple[str, str, list[str]]]:
    """A scene's or story's lines: (who, criteria, alternatives), tags and all."""
    out = []
    for raw in it.body.split('\n')[1:]:
        t = raw.strip()
        m = re.match(r'^(ask|[ab]\??(?:\[[^\]]*\])?(?:\s*#[a-z0-9_]+)?|a@[a-z0-9_]+(?:\[[^\]]*\])?)?\s*:\s*(.*)$', t)
        if not m or t.startswith(('when ', 'does ', 'tone ')):
            continue
        head = m.group(1) or ''
        who = 'b' if head.startswith('b') else 'a'
        crit = re.search(r'\[([^\]]*)\]', head)
        out.append((who, crit.group(1) if crit else '', C.alternatives(m.group(2))))
    return out


def texts_of(it: T.Item) -> list[tuple[str, list[str]]]:
    """(who, alternatives) for every line an item says: a scene's or story's lines with their tags (a: the wearer, b: the
    one spoken to), else G2's reading (p: a player's choice in a graft)."""
    if it.kind in ('scene', 'story'):
        return [(who, alts) for who, _, alts in turns(it)]
    return [(who, texts) for texts, _, who in C.said(it)]


def item_criteria(it: T.Item) -> str:
    lines = it.body.split('\n')
    crit = ' '.join(l.strip()[5:] for l in lines[1:] if l.strip().startswith('when '))
    first = lines[0]
    m = re.search(r'\[([^\]]*)\]', first.split(':', 1)[0])
    return crit + ' ' + (m.group(1) if m else '')


def runs5(text: str) -> set:
    w = WORD.findall(text.lower())
    return {' '.join(w[i:i + 5]) for i in range(len(w) - 4)}


def lint(paths: list[str], world: str | None, facts: dict | None) -> list[C.Finding]:
    out: list[C.Finding] = []
    files = C.files(paths)
    batch = [(f, read_mantle(f)) for f in files]
    batch = [(f, m) for f, m in batch if m]
    # (G2's rules over the tagged lines, the persona's pronouns and the window lines that would hide facts were the lint's
    # until fix 8; mantle_check (G2) makes them now, so mantle_load refuses them too.)
    for f, m in batch:
        text = f.read_text()
        fx = (facts or {}).get(m.written_for, {}) if m.written_for else {}
        # 2. The deity family, anywhere (comments aside).
        for n, raw in enumerate(text.split('\n'), 1):
            if raw.lstrip().startswith('#') and not raw.lstrip().startswith('#self'):
                continue
            if DEITY.search(raw):
                out.append(C.Finding(m.id, f'line {n}', f'"{DEITY.search(raw).group(0)}": the deities are the Bright One and '
                                                       f'the Dark One; no god, gods, godsend, godly...'))
            if re.search(r'\b(TODO|todo)\b', raw):
                out.append(C.Finding(m.id, f'line {n}', 'a TODO is left'))
        # 4. The header against the wearer.
        if m.tier == 'spare':
            if m.written_for:
                out.append(C.Finding(m.id, 'written_for', 'a spare is written for nobody'))
            if re.search(r'\b(sex|role|job|class)=', m.fits or ''):
                out.append(C.Finding(m.id, 'fits', 'a spare fits any newborn of its dialect: stage only, never sex, role or job'))
            for s in ('young', 'adolescent', 'grown'):
                if not any(it.section == s for it in m.items):
                    out.append(C.Finding(m.id, s, f'a spare needs {"an" if s[0] == "a" else "a"} {s} section'))
        elif not m.written_for:
            out.append(C.Finding(m.id, 'written_for', 'a mantle for a resident names them (written_for)'))
        missing = [ax for ax in AXES if ax not in m.traits]
        if missing:
            out.append(C.Finding(m.id, 'traits', f'all seven axes are needed (a newborn takes the card whole): {", ".join(missing)} missing'))
        if fx:
            if fx.get('dialect') and m.dialect != fx['dialect']:
                out.append(C.Finding(m.id, 'dialect', f'{m.written_for} speaks {fx["dialect"]} (where they grew up), not {m.dialect}'))
            acc = fx.get('accent') or 'none'
            if acc not in m.accents:
                out.append(C.Finding(m.id, 'accent', f'{m.written_for} speaks with accent {acc}: the accents must include it '
                                                     f'(the brief says `{accents_of(fx)}`)'))
            if fx.get('honesty') and m.traits.get('honesty') != fx['honesty']:
                out.append(C.Finding(m.id, 'traits', f'honesty is kept as the wearer\'s ({fx["honesty"]}): it chooses thieves'))
            stage = fx.get('stage', '')
            fit_st = re.search(r'stage=([a-z|]+)', m.fits or '')
            if stage and fit_st and stage not in fit_st.group(1).split('|'):
                out.append(C.Finding(m.id, 'fits', f'{m.written_for} is {stage}: the fit refuses them, so it would never bind'))
            if stage in ('young', 'adolescent'):
                for s in ('young', 'adolescent'):
                    if not any(it.section == s for it in m.items):
                        out.append(C.Finding(m.id, s, f'{m.written_for} is a pup ({stage}): {"an" if s[0] == "a" else "a"} {s} section is needed'))
            if tier_of(fx) != m.tier and m.tier != 'spare':
                out.append(C.Finding(m.id, 'tier', f'the brief says {tier_of(fx)} for {m.written_for}', True))
        rel = f.resolve()
        if rel.parent.name != m.dialect or rel.stem != m.id:
            out.append(C.Finding(m.id, '', f'the file belongs at Data/Talk/mantles-export/{m.dialect}/{m.id}.mantle', True))
        counts = collections.Counter(it.kind for it in m.items)
        label = {'scene': 'scenes', 'story': 'stories', 'opinion': 'opinions', 'line': 'lines', 'rule': 'barks (rules)',
                 'dlg': 'window lines', 'graft': 'grafts', 'symbol': 'words (symbols)'}
        short = [f'{counts[k]} {label[k]} of {n}' for k, n in TARGETS.get(m.tier, {}).items() if counts[k] < n]
        if short:
            out.append(C.Finding(m.id, '', 'under the pilot\'s counts for its tier: ' + ', '.join(short), True))
        # 5. Items.
        brief_runs = set()
        for k, v in fx.items():
            if isinstance(v, str) and (k.startswith(('op_why', 'op_whys')) or k.endswith('_words')):
                for part in v.split('\n'):
                    brief_runs |= runs5(part)
        own_alts, short_alts = 0, 0
        for it in m.items:
            crit = item_criteria(it)
            said = texts_of(it)
            for who, texts in said:
                for t in texts:
                    if talk_lint.they_named(t):
                        out.append(C.Finding(m.id, it.key, f'"{t}": "they" (them, their) after one wolf named by a slot; a '
                                                           f'wolf is male or female: say the name again, #topic.they#, or '
                                                           f'rephrase (if "they" means something else, make that plain)'))
            if it.kind == 'story':
                for raw in it.body.split('\n')[1:]:
                    am = re.match(r'^\s*ask\s*:\s*(.*)$', raw)
                    if am and LISTENERS_ASK.search(am.group(1).strip()):
                        out.append(C.Finding(m.id, it.key, f'"ask : {am.group(1).strip()}": `ask :` is the teller\'s question to '
                                                           f'the listener ("Do you keep anything...?"), and this asks the teller '
                                                           f'about the story: it is the listener\'s, so put it in the `b` line '
                                                           f'after ("You found the better water. Did you catch one?")'))
            if it.kind in ('scene', 'story'):
                ts = turns(it)
                threads = max((len(alts) for _, _, alts in ts), default=1)
                for who, c, alts in ts:
                    if len(alts) not in (1, threads):
                        out.append(C.Finding(m.id, it.key, f'an `{who}` line has {len(alts)} alternatives where it has '
                                                           f'{threads} threads: each line has one, or one a thread'))
                if it.kind == 'scene' and ts:
                    last = [alts for who, c, alts in ts if not c][-1:] or [ts[-1][2]]
                    for alt in last[0]:
                        if alt.rstrip().endswith('?'):
                            out.append(C.Finding(m.id, it.key, f'a thread ends on a question nobody answers: "{alt}"', True))
            for who, texts in said:
                if who != 'a':
                    continue
                for t in texts:
                    if it.kind != 'symbol':
                        own_alts += 1
                        bare = C.SLOT.sub('', t).strip(' .,!?')
                        if len(WORD.findall(bare.lower())) <= 3:
                            short_alts += 1
                    if it.kind != 'symbol' and STOCK.match(t.strip()):
                        out.append(C.Finding(m.id, it.key, f'"{t}": a stock line the blind raters marked down (stiff, '
                                                           f'clerkly); say it as this wolf would', True))
                    if AGE_CLAIM.search(t) and not re.search(r'self\.(age|stage)=|stage=old', crit + ' ' + it.section) and \
                            it.section not in ('old',):
                        out.append(C.Finding(m.id, it.key, f'"{t}": a claim of years needs an age gate (self.age>=N, or the '
                                                           f'old section), or a later, younger wearer says it untrue', True))
                    if brief_runs & runs5(C.plain_words(t)):
                        out.append(C.Finding(m.id, it.key, f'"{t}": the brief\'s own words (every writer of the town has them); '
                                                           f'say it in this wolf\'s', True))
            if it.kind == 'line':
                head = it.body.split(':', 1)[0].split()
                moves = [w for w in head[1:] if not w.startswith(('@', '['))]
                if moves and moves[0] in DISPREFERRED:
                    for t in C.said(it)[0][0]:
                        if len(WORD.findall(C.SLOT.sub(' x ', t).lower())) < 6 and not HEDGES.search(t):
                            out.append(C.Finding(m.id, it.key, f'"{t}": a {moves[0]} hedges and gives a reason (#hedge#, '
                                                               f'#account#), or runs six words or more; "No." is never one'))
        if own_alts and short_alts / own_alts > .3:
            out.append(C.Finding(m.id, '', f'{short_alts} of {own_alts} of the wearer\'s alternatives are three words or fewer '
                                           f'({short_alts / own_alts:.0%}): talk that sounds clipped; aim under 30%', True))
    return out


def cmd_lint(a) -> int:
    facts = read_facts(a.facts, a.world) if (a.facts or a.world) else None
    found = lint(a.paths, a.world, facts)
    errors = [f for f in found if not f.warning]
    if a.json:
        print(json.dumps([vars(f) for f in found], indent=1))
    else:
        for f in found:
            print(f)
        print(f'{len(C.files(a.paths))} mantles: {len(errors)} refusals, {len(found) - len(errors)} warnings (lint)')
    return 1 if errors else 0


# ------------------------------------------------------------------ gates

def cmd_gates(a) -> int:
    files = C.files(a.paths)
    if not files:
        print('no .mantle files there')
        return 1
    facts_path = a.facts
    tmp = Path(tempfile.mkdtemp(prefix='mantle-gates-'))
    try:
        if not facts_path:
            facts_path = str(tmp / 'residents.jsonl')
            ns = argparse.Namespace(world=a.world, out=facts_path)
            if cmd_facts(ns):
                return 1
        facts = read_facts(facts_path, a.world)
        tc = str(talk_check())
        results = {}
        print(f'== G1: the game\'s loader ({tc} --data {TALK} --mantle)')
        g1 = subprocess.run([tc, '--data', str(TALK), '--mantle'] + [str(f) for f in files], capture_output=True, text=True)
        print('\n'.join(l for l in g1.stdout.splitlines() if not l.startswith('ok ')))
        results['G1'] = g1.returncode == 0
        mantles = [read_mantle(f) for f in files]
        dialects = sorted({m.dialect for m in mantles if m})
        dirs = sorted({str(f.resolve().parent) for f in files} | {str(EXPORT / d) for d in dialects if (EXPORT / d).is_dir()})
        print(f'== G2: mantle_check --strict over {", ".join(dirs)}')
        g2 = subprocess.run([sys.executable, str(HERE / 'mantle_check.py')] + dirs +
                            ['--world', resolve(a.world), '--strict', '--facts', facts_path], capture_output=True, text=True)
        print(g2.stdout.strip() or g2.stderr.strip())
        results['G2'] = g2.returncode == 0
        print('== The lint (what G1 and G2 miss)')
        found = lint([str(f) for f in files], a.world, facts)
        for x in found:
            print(x)
        errors = [x for x in found if not x.warning]
        print(f'{len(files)} mantles: {len(errors)} refusals, {len(found) - len(errors)} warnings (lint)')
        results['lint'] = not errors
        if not a.no_g3:
            g3dir = tmp / 'g3'
            for f, m in zip(files, mantles):
                if m:
                    (g3dir / m.dialect).mkdir(parents=True, exist_ok=True)
                    shutil.copy(f, g3dir / m.dialect / f.name)
            print(f'== G3: talk_check --world {resolve(a.world)} --mantles-from (these {len(files)} only) --mantle-coverage')
            g3 = subprocess.run([tc, '--data', str(TALK), '--world', resolve(a.world), '--mantles-from', str(g3dir),
                                 '--mantle-coverage'], capture_output=True, text=True, timeout=1800)
            print(g3.stdout.strip())
            results['G3'] = g3.returncode == 0
        print('== ' + ', '.join(f'{k} {"pass" if v else "FAIL"}' for k, v in results.items()))
        return 0 if all(results.values()) else 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


# ------------------------------------------------------------------ pools (spares)

def cmd_pools(a) -> int:
    facts = read_facts(a.facts, a.world)
    living = collections.defaultdict(collections.Counter)
    for r in facts.values():
        living[r.get('dialect', '?')][r.get('honesty', 'plain')] += 1
    spares = collections.defaultdict(collections.Counter)
    bad = 0
    for f in C.files(a.paths):
        m = read_mantle(f)
        if not m or m.tier != 'spare':
            continue
        spares[m.dialect][m.traits.get('honesty', 'plain')] += 1
    print('dialect       spares  sly+guarded (spares / living)   ok')
    for d in sorted(set(spares) | set(PREFIX)):
        s, l = spares.get(d, collections.Counter()), living.get(d, collections.Counter())
        sn, ln = sum(s.values()), sum(l.values())
        sp = 100 * (s['sly'] + s['guarded']) / sn if sn else 0
        lp = 100 * (l['sly'] + l['guarded']) / ln if ln else 0
        ok = not sn or abs(sp - lp) <= 5
        bad += not ok
        print(f'{d:<13} {sn:>6}  {sp:5.1f} / {lp:5.1f}                  {"ok" if ok else "OFF (within 5 points, doc 84 §9.5)"}')
    return 1 if bad else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('facts')
    p.add_argument('--world', default=WORLD)
    p.add_argument('--out', default=str(ROOT / 'Saved' / 'Mantles' / 'residents.jsonl'))
    p = sub.add_parser('todo')
    p.add_argument('--world', default=WORLD)
    p.add_argument('--facts')
    p.add_argument('--town', default='')
    p.add_argument('--quarter', default='', help="a Ridgemere quarter, or 'none' for those with none")
    p.add_argument('--dialect', default='')
    p.add_argument('--dir', action='append', default=[])
    p.add_argument('--limit', type=int, default=0)
    p.add_argument('--json', action='store_true')
    p = sub.add_parser('plan')
    p.add_argument('--name', required=True)
    p.add_argument('--town', required=True)
    p.add_argument('--quarter', default='')
    p.add_argument('--dialect', default='')
    p.add_argument('--size', type=int, default=40)
    p.add_argument('--world', default=WORLD)
    p.add_argument('--facts')
    p.add_argument('--dir', action='append', default=[], help="other batches' folders (Saved/Mantles): their residents and ids "
                                                               'are taken')
    p.add_argument('--out')
    p = sub.add_parser('lint')
    p.add_argument('paths', nargs='+')
    p.add_argument('--world', default=WORLD)
    p.add_argument('--facts')
    p.add_argument('--json', action='store_true')
    p = sub.add_parser('gates')
    p.add_argument('paths', nargs='+')
    p.add_argument('--world', default=WORLD)
    p.add_argument('--facts')
    p.add_argument('--no-g3', action='store_true')
    p = sub.add_parser('pools')
    p.add_argument('paths', nargs='+')
    p.add_argument('--world', default=WORLD)
    p.add_argument('--facts')
    a = ap.parse_args()
    return {'facts': cmd_facts, 'todo': cmd_todo, 'plan': cmd_plan, 'lint': cmd_lint, 'gates': cmd_gates,
            'pools': cmd_pools}[a.cmd](a)


if __name__ == '__main__':
    sys.exit(main())
