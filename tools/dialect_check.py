#!/usr/bin/env python3
"""The dialect check (Docs/Design/83-dialogue-trees.md, Phase 4): does every resident's talk match where they're from?

  python3 tools/dialect_check.py                       the files' rules (no database): lexicon, trees, reach, lore
  python3 tools/dialect_check.py --world ridgemere3    and the world's: origins, character tables, live.dialects
  python3 tools/dialect_check.py --lexicon             the distinctive phrases of each dialect, as the check uses them
  python3 tools/dialect_check.py --listen OUT.md       every tree's lines in all eight voices (talk_check --render-trees)

Failures (exit 1):
  lexicon   a dialect's distinctive phrase (its own grammar's words, said by neither plain nor any other dialect) in a
            rule, scene turn, library scene or tree line whose speaker isn't gated to that dialect's speakers
            (`from=`, `self.`/`a.`/`b.from=`, `b[from=...]`, a tree's `say[from=...]`), or in a player's choice (a player
            has no dialect yet). Greetings, farewells and the like count only at the start of a sentence; single words
            only where `tools/dialect_allow.json` marks them. A line quoting another region is allowed there, with why.
  symbols   a grammar symbol a dialogue tree's line says (#shop_offer#...) that a dialect doesn't define: that dialect's
            speakers would fall back to plain speech in the window.
  renders   a tree line, rendered in every voice (plain and the seven dialects, light and broad), longer than 160
            characters or with a symbol left unfilled (needs a built talk_check; skipped, with a note, without one).
  reach     a rule or scene gated to places (`here.region`) none of which anyone lives in: it never plays (the
            erased Mirebridge: dialect_allow.json "erased").
  origins   (--world) a town where fewer than 70% of the residents placed there (60% for the city) speak its dialect
            (grew up there, or in a town that speaks the same: settings.json "dialect_of"), or a town with no dialect
            and no mapping.
  tables    (--world) a character table (live.npc_voice grammar and rules, by a person) using another dialect's phrase.
  dialects  (--world) a live.dialects row that isn't its file's grammar word for word (the database copy wins at load).
Warnings: lore, a place's name in the regional scenes (scenes/regions) the world doesn't have (--world: its rooms and
cells; always: communities.json and the allowlist's places). Kept as legend in dialect_allow.json "legend".
"""
from __future__ import annotations

import argparse
import collections
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TALK = ROOT / 'Data' / 'Talk'
ALLOW = Path(__file__).resolve().parent / 'dialect_allow.json'
DIALECTS = ['frosthollow', 'coldwater', 'oakhaven', 'mirebridge', 'larkfield', 'southmeadow', 'saltwell']
CITY = 'ridgemere'
# A greeting and its like are ordinary words used as a saying: matched only at the start of a sentence.
SAYINGS = {'greet', 'farewell', 'blessing', 'bless', 'toast', 'thanks', 'agree', 'refuse', 'surprise', 'annoyed',
           'oath', 'saying', 'shop_bye', 'shop_offer', 'talk_open', 'tag', 'opener'}
LIMIT = 160


# ------------------------------------------------------------------ Reading the library

def grammars(root: Path = TALK) -> dict[str, dict[str, list[str]]]:
    """{grammar: {symbol: [alternatives]}}; a symbol's `.broad`, habit marks and [criteria] folded into its name."""
    out = {}
    for f in sorted((root / 'grammar').glob('*.grammar')):
        current = None
        for line in f.read_text().splitlines():
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith('grammar '):
                current = out.setdefault(line[8:].strip(), {})
                continue
            m = re.match(r'^([a-z0-9_]+)(\.broad)?!{0,2}(\[[^\]]*\])?\s*\+?=\s*(.*)$', line)
            if m and current is not None:
                current.setdefault(m.group(1), []).extend(a.strip() for a in m.group(4).split('|') if a.strip())
    return out


def grammar_text(text: str) -> dict[str, list[str]]:
    """One grammar's text (a character table's, a live.dialects row's) as {symbol: [alternatives]}."""
    out = {}
    for line in text.splitlines():
        m = re.match(r'^\s*([a-z0-9_]+)(\.broad)?!{0,2}(\[[^\]]*\])?\s*\+?=\s*(.*)$', line)
        if m:
            out.setdefault(m.group(1), []).extend(a.strip() for a in m.group(4).split('|') if a.strip())
    return out


def settings(root: Path = TALK) -> dict:
    return json.loads((root / 'settings.json').read_text())


def groups(root: Path = TALK) -> dict[str, set[str]]:
    out = {}
    for line in (root / 'groups.talk').read_text().splitlines():
        if line.startswith('group '):
            name, members = line[6:].split('=', 1)
            got = set()
            for m in members.split():
                got |= out.get(m, {m})
            out[name.strip()] = got
    return out


def values(token: str) -> tuple[str, str, set[str]]:
    """'b.from!=x|y' -> ('b.from', '!=', {'x', 'y'})."""
    m = re.match(r'^([a-z_.]+)(!=|<=|>=|=|<|>)(.*)$', token)
    if not m:
        return token, '', set()
    return m.group(1), m.group(2), set(m.group(3).split('|'))


TURN = re.compile(r'^([ab])(?![a-z])([^:]*):(.*)$')


def entries(root: Path = TALK) -> list[dict]:
    """Every rule and scene in the .talk files: {'id', 'file', 'line', 'scene', 'when': [tokens], 'turns': [{'who', 'tags':
    [tokens], 'texts': [...], 'line'}]}."""
    out = []
    for f in sorted(root.rglob('*.talk')):
        entry = None
        for number, raw in enumerate(f.read_text().splitlines(), 1):
            line = raw.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith(('scene ', 'rule ')):
                entry = {'id': line.split(None, 1)[1].strip(), 'file': str(f.relative_to(root)), 'line': number,
                         'scene': line.startswith('scene '), 'when': [], 'turns': []}
                out.append(entry)
            elif entry is None:
                continue
            elif line.startswith('when '):
                entry['when'] += line[5:].split()
            elif line.startswith('say ') or line == 'say':
                entry['turns'].append({'who': 'a', 'tags': [], 'texts': split_alts(line[3:]), 'line': number})
            else:
                m = TURN.match(line)
                if m:
                    tags = [t for group in re.findall(r'\[([^\]]*)\]', m.group(2)) for t in group.split()]
                    entry['turns'].append({'who': m.group(1), 'tags': tags, 'texts': split_alts(m.group(3)), 'line': number})
    return out


def split_alts(text: str) -> list[str]:
    """A line's alternatives, each without the conversation tag that may lead it ({k=v}: doc 84 §7.1)."""
    return [re.sub(r'^\{[^}]*\}\s*', '', t.strip()) for t in text.split('|') if t.strip()]


def trees(root: Path = TALK) -> list[dict]:
    """Every dialogue tree's lines: {'tree', 'file', 'line', 'node', 'kind': say|choice, 'criteria': [tokens], 'texts'}."""
    out = []
    for f in sorted((root / 'dialogue').glob('*.tree')):
        tree = node = ''
        for number, raw in enumerate(f.read_text().splitlines(), 1):
            line = raw.strip()
            if line.startswith('tree '):
                tree = line[5:].strip()
            elif line.startswith('node '):
                node = line[5:].strip()
            elif line.startswith('say'):
                m = re.match(r'^say(\[([^\]]*)\])?\s+(.*)$', line)
                if m:
                    out.append({'tree': tree, 'file': f.name, 'line': number, 'node': node, 'kind': 'say',
                                'criteria': (m.group(2) or '').split(), 'texts': split_alts(m.group(3))})
            elif line.startswith('choice '):
                words = re.split(r'\s(?:\[|locked\s|->)', line[7:], maxsplit=1)[0].strip()
                out.append({'tree': tree, 'file': f.name, 'line': number, 'node': node, 'kind': 'choice', 'criteria': [],
                            'texts': [words]})
    return out


# ------------------------------------------------------------------ The lexicon

def norm(text: str) -> str:
    return re.sub(r"\s+", ' ', re.sub(r"[^a-z' -]", ' ', text.lower())).strip(" '-")


def lexicon(gram: dict[str, dict[str, list[str]]], allow: dict) -> dict[str, list[tuple[str, bool]]]:
    """{dialect: [(phrase, as_saying)]}: the dialect's words that neither plain nor another dialect says. A phrase of
    two words or more, or a coined one (with a hyphen), counts; a single plain word only where the allowlist marks it.
    as_saying: a greeting's, a farewell's... (matched only at the start of a sentence)."""
    said = collections.defaultdict(lambda: collections.defaultdict(set))   # grammar -> phrase -> symbols
    for g, symbols in gram.items():
        for symbol, alts in symbols.items():
            for a in alts:
                if '#' in a or a.strip() == '~':
                    continue
                p = norm(a)
                if len(p) >= 3:
                    said[g][p].add(symbol)
                    if re.search(r'[.!?;:]\s*\S', a.strip()):
                        SPANNING.add(p)          # (Its own words run over a sentence's end: "Too late. Sunk.")
    ordinary = {norm(p) for p in allow.get('ordinary', [])}
    marked = {d: {norm(w) for w in ws} for d, ws in allow.get('marked', {}).items()}
    out = {}
    for d in DIALECTS:
        others = set().union(*(set(said[g]) for g in gram if g != d))
        own = []
        for p, symbols in sorted(said[d].items()):
            if p in others or p in ordinary:
                continue
            if len(p.split()) < 2 and '-' not in p and p not in marked.get(d, set()):
                continue
            own.append((p, bool(symbols & SAYINGS) and not symbols - SAYINGS))
        out[d] = own
    return out


def strip_symbols(text: str) -> str:
    """The words outside #symbols# (each symbol a break, so nothing matches across one)."""
    return re.sub(r'#[^#\s]*#', ' | ', text)


_PATTERNS: dict[int, tuple] = {}
# The phrases whose own words run over a sentence's end (a dialect's "Ha! Netted."): found across sentences, where every
# other phrase is found only within one, so "Back again? Stone Sunton's been..." isn't Frosthollow's "Back again, stone."
SPANNING: set[str] = set()


def patterns(lex: dict[str, list[tuple[str, bool]]]) -> dict[str, tuple]:
    """Each dialect's phrases as two expressions: anywhere (words), and at a sentence's start (sayings)."""
    key = id(lex)
    if key not in _PATTERNS or _PATTERNS[key][0] is not lex:      # (An id is reused once its lexicon is gone.)
        out = {}
        for d, phrases in lex.items():
            words = sorted((p for p, s in phrases if not s), key=len, reverse=True)
            sayings = sorted((p for p, s in phrases if s), key=len, reverse=True)
            anywhere = re.compile(r"(?<![a-z'-])(" + '|'.join(map(re.escape, words)) + r")(?![a-z'-])") if words else None
            start = re.compile(r"^(" + '|'.join(map(re.escape, sayings)) + r")(?= |$)") if sayings else None
            out[d] = (anywhere, start)
        _PATTERNS[key] = (lex, out)
    return _PATTERNS[key][1]


def hits(text: str, lex: dict[str, list[tuple[str, bool]]]) -> list[tuple[str, str]]:
    """(dialect, phrase) for each distinctive phrase in the text's own words."""
    own = strip_symbols(text)
    plain = norm(own.replace('|', '.'))
    found = []
    if not plain:
        return found
    # A saying stands alone, as a greeting does: the whole sentence, or before a comma ("Dry feet, friend."). Each
    # sentence is its words (no commas) and, for each count of words, whether a comma or its end comes after them.
    sentences = []
    for part in re.split(r'[.!?|;:]', own):
        tokens = norm(part.replace(',', ' qqcommaqq ')).split()
        words, breaks = [], set()
        for t in tokens:
            if t == 'qqcommaqq':
                breaks.add(len(words))
            else:
                words.append(t)
        breaks.add(len(words))
        sentences.append((' '.join(words), breaks))
    for d, (anywhere, start) in patterns(lex).items():
        if anywhere:
            within = [m for part in re.split(r'[.!?|;:]', own) for m in anywhere.findall(norm(part))]
            across = [m for m in anywhere.findall(plain) if m in SPANNING]
            found += [(d, m) for m in dict.fromkeys(within + across)]
        if start:
            for sentence, breaks in sentences:
                for m in [start.match(sentence)]:
                    if m and len(m.group(1).split()) in breaks and (d, m.group(1)) not in found:
                        found.append((d, m.group(1)))
    return found


def speaker_from(when: list[str], tags: list[str], who: str, scene: bool) -> set[str] | None:
    """The places a line's speaker may be from (None: anyone)."""
    keys = {'a': ('a.from', 'self.from', 'from') if scene else ('a.from', 'self.from', 'from'),
            'b': ('b.from', 'other.from') if scene else ('b.from',)}[who]
    if scene and who == 'a':
        keys = ('a.from', 'from', 'self.from')
    places = None
    for token in when:
        key, op, vals = values(token)
        if op == '=' and key in keys:
            places = vals if places is None else places & vals
    for token in tags:
        key, op, vals = values(token)
        if op == '=' and key in ('from', 'self.from'):
            places = vals if places is None else places & vals
    return places


# ------------------------------------------------------------------ The checks on the files

class Report:
    def __init__(self):
        self.failures = collections.defaultdict(list)
        self.warnings = collections.defaultdict(list)
        self.notes = []

    def fail(self, kind: str, message: str):
        self.failures[kind].append(message)

    def warn(self, kind: str, message: str):
        self.warnings[kind].append(message)


def allowed(allow: dict, file: str, phrase: str, text: str) -> bool:
    for a in allow.get('lines', []):
        if a.get('phrase') and norm(a['phrase']) != phrase:
            continue
        if a.get('file') and not file.endswith(a['file']):
            continue
        if a.get('text') and a['text'] not in text:
            continue
        return True
    return False


def check_lexicon(report: Report, lex: dict, dialect_of: dict[str, str], allow: dict, root: Path = TALK) -> int:
    """Each distinctive phrase in a line whose speaker isn't sure to speak its dialect. Returns the lines read."""
    speaks = lambda place: dialect_of.get(place, place)
    read = 0
    for e in entries(root):
        for t in e['turns']:
            places = speaker_from(e['when'], t['tags'], t['who'], e['scene'])
            for text in t['texts']:
                read += 1
                for d, p in hits(text, lex):
                    if places and all(speaks(pl) == d for pl in places):
                        continue
                    if allowed(allow, e['file'], p, text):
                        continue
                    report.fail('lexicon', f"{e['file']}:{t['line']}: {e['id']} ({t['who']}): {d}'s \"{p}\", said by "
                                f"{'anyone' if not places else ' or '.join(sorted(places))}: {text}")
    for line in trees(root):
        for token in line['criteria']:
            pass
        places = None
        for token in line['criteria']:
            key, op, vals = values(token)
            if op == '=' and key in ('from', 'self.from'):
                places = vals
        for text in line['texts']:
            read += 1
            for d, p in hits(text, lex):
                if line['kind'] == 'say' and places and all(speaks(pl) == d for pl in places):
                    continue
                if allowed(allow, line['file'], p, text):
                    continue
                who = 'the player' if line['kind'] == 'choice' else 'anyone' if not places else ' or '.join(sorted(places))
                report.fail('lexicon', f"dialogue/{line['file']}:{line['line']}: {line['tree']}.{line['node']} "
                            f"({line['kind']}): {d}'s \"{p}\", said by {who}: {text}")
    return read


def tree_symbols(gram: dict, root: Path = TALK) -> set[str]:
    """The grammar symbols the trees' lines say (not facts: those have a dot)."""
    used = set()
    for line in trees(root):
        if line['kind'] != 'say':
            continue
        for text in line['texts']:
            for name in re.findall(r'#([a-z0-9_]+)(?:\.[a-z]+)?#', text):
                if name in gram.get('plain', {}):
                    used.add(name)
    # A symbol a used symbol says (one level is all the grammar's own nesting needs here).
    for name in list(used):
        for alt in gram['plain'].get(name, []):
            used |= {n for n in re.findall(r'#([a-z0-9_]+)#', alt) if n in gram['plain']}
    return used


def check_symbols(report: Report, gram: dict, root: Path = TALK):
    for name in sorted(tree_symbols(gram, root)):
        for d in (d for d in DIALECTS if d in gram):
            if name not in gram[d]:
                report.fail('symbols', f"#{name}#, said in a dialogue tree, isn't in {d}.grammar: its speakers fall back to plain")


def check_reach(report: Report, homes: set[str], allow: dict, root: Path = TALK):
    """Entries gated to places nobody lives in."""
    grp = groups(root)
    erased = set(allow.get('erased', []))
    living = (homes | {'wild'}) - erased             # (Out on the roads, anyone may stand in the wild.)
    for e in entries(root):
        for token in e['when']:
            key, op, vals = values(token)
            if key in ('here.region', 'region') and op == '=':
                places = set().union(*(grp.get(v, {v}) for v in vals))
                if not places & living:
                    report.fail('reach', f"{e['file']}:{e['line']}: {e['id']} is for {'|'.join(sorted(vals))}, where "
                                f"nobody lives: it never plays")


# Words that start a sentence or name a person: never a place.
NOT_PLACES = {'The', 'A', 'An', 'I', 'My', 'Our', 'Your', 'His', 'Her', 'Their', 'Lord', 'Lady', 'Old', 'Every', 'Some',
              'One', 'Two', 'Three', 'They', 'We', 'You', 'He', 'She', 'It', 'This', 'That', 'There', 'When', 'Where'}


def lore_names(text: str) -> list[str]:
    """Capitalised names in a line that look like places ("the Frozen Lantern", "Glasswater"), not first words, not
    "I'll"; a possessive's "'s" left off."""
    found = []
    for sentence in re.split(r'(?<=[.!?])\s+|\||[,;:()"]', strip_symbols(text)):
        words = [re.sub(r"[^A-Za-z'-]", '', w) for w in sentence.strip().split()]
        name: list[str] = []

        def close():
            while name and name[-1] in ('of', 'and', 'the'):
                name.pop()
            if name:
                found.append(' '.join(name))
            name.clear()

        for i, w in enumerate(words):
            base = w[:-2] if w.endswith("'s") else w
            capital = base[:1].isupper() and "'" not in base and base not in ('I',)
            if i == 0 and not name:
                continue                                # (A sentence's first word.)
            if capital and (name or base not in NOT_PLACES):
                name.append(base)
                if w.endswith("'s"):
                    close()
            elif name and w in ('of', 'and', 'the'):
                name.append(w)
            else:
                close()
        close()
    return found


def known_names(world_names: set[str], allow: dict) -> set[str]:
    names = set(world_names) | set(allow.get('places', [])) | set(allow.get('legend', {}))
    communities = ROOT / 'Data' / 'Economy' / 'communities.json'
    if communities.exists():
        doc = json.loads(communities.read_text())
        for key in ('communities', 'farms', 'works'):
            for c in doc.get(key, []):
                for field in ('name', 'title'):
                    if isinstance(c.get(field), str):
                        names.add(c[field])
                about = c.get('about') if isinstance(c.get('about'), str) else ''
                names |= set(lore_names('x ' + about))
    out = set()
    for n in names:
        for part in [n.lower()] + re.split(r',| of | and ', n.lower()):
            part = re.sub(r'^the ', '', part.strip())
            if part:
                out.add(part)
                out.add(re.sub(r"'s$", '', part))
    return out


def check_lore(report: Report, world_names: set[str], allow: dict, root: Path = TALK):
    known = known_names(world_names, allow)
    people = set(allow.get('people', [])) | set(allow.get('names', []))
    seen = collections.Counter()
    where = {}
    for e in entries(root):
        if not e['file'].startswith('scenes/regions/'):
            continue
        for t in e['turns']:
            for text in t['texts']:
                for name in lore_names(text):
                    low = name.lower()
                    if low in known or re.sub(r'^the ', '', low) in known or name in people or name.split()[0] in people:
                        continue
                    seen[name] += 1
                    where.setdefault(name, f"{e['file']}:{t['line']}")
    for name, n in seen.most_common():
        report.warn('lore', f"\"{name}\" ({n}x, first {where[name]}) isn't a place the world has")


# ------------------------------------------------------------------ The world's checks

def residents(world: str) -> list[dict]:
    """Every resident's facts as the server places them (talk_check --world KEY --residents --json)."""
    talk_check = newest_talk_check()
    if not talk_check:
        sys.exit('No talk_check built: cmake --build <a build dir> --target talk_check')
    run = subprocess.run([str(talk_check), '--world', world, '--residents', '--json'], capture_output=True, text=True,
                         timeout=900)
    if run.returncode != 0:
        sys.exit(f'talk_check --residents failed:\n{run.stderr[-2000:]}')
    return [json.loads(line) for line in run.stdout.splitlines() if line.startswith('{')]


def newest_talk_check() -> Path | None:
    found = sorted(ROOT.glob('build*/talk_check'), key=lambda p: p.stat().st_mtime, reverse=True)
    return found[0] if found else None


def check_origins(report: Report, people: list[dict], dialect_of: dict[str, str]) -> dict:
    """Per town: residents placed there and the share who speak its dialect."""
    speaks = lambda place: dialect_of.get(place, place)
    known = set(DIALECTS) | {CITY}
    towns = collections.defaultdict(lambda: [0, 0])
    for p in people:
        home = p.get('_home') or ''
        if not home:
            continue
        towns[home][0] += 1
        towns[home][1] += bool(p.get('from')) and speaks(p['from']) == speaks(home)
    out = {}
    for town, (n, local) in sorted(towns.items(), key=lambda kv: -kv[1][0]):
        share = local / n if n else 0
        out[town] = {'residents': n, 'speak_its_dialect': local, 'share': round(share, 3)}
        if speaks(town) not in known:
            report.fail('origins', f'{town}: no dialect of its own and none in settings.json dialect_of ({n} residents)')
            continue
        bar = .6 if town == CITY else .7
        if share < bar:
            report.fail('origins', f'{town}: {local} of {n} residents ({share:.0%}) speak its dialect; at least {bar:.0%} should')
    return out


def check_tables(report: Report, rows: list[tuple], lex: dict, dialect_of: dict[str, str]):
    speaks = lambda place: dialect_of.get(place, place)
    for npc, origin, grammar, rules in rows:
        own = speaks(origin or '')
        texts = [a for alts in grammar_text(grammar or '').values() for a in alts]
        texts += [t for line in (rules or '').splitlines() if line.strip().startswith(('say', 'a ', 'a:', 'b ', 'b:'))
                  for t in split_alts(line.split(':', 1)[-1] if not line.strip().startswith('say') else line.strip()[3:])]
        for text in texts:
            for d, p in hits(text, lex):
                if d != own:
                    report.fail('tables', f"{npc} (from {origin or 'nowhere'}): their own words say {d}'s \"{p}\": {text}")


def check_dialect_rows(report: Report, rows: dict[str, str], root: Path = TALK):
    for f in sorted((root / 'grammar').glob('*.grammar')):
        if f.stem == 'plain':
            continue
        if f.stem not in rows:
            report.fail('dialects', f'live.dialects has no {f.stem} row (the file is used)')
        elif rows[f.stem] != f.read_text():
            report.fail('dialects', f'live.dialects {f.stem} differs from {f.name}: the database copy wins at load '
                        f'(tools/talk_seed.py --replace dialects writes the files)')


# ------------------------------------------------------------------ Rendering the trees

def renders(seed: int = 1) -> list[dict] | None:
    """talk_check --render-trees --json: every tree line in every voice ([{tree, node, line, voice, accent, text,
    problem}]); None without a talk_check that knows it."""
    talk_check = newest_talk_check()
    if not talk_check:
        return None
    run = subprocess.run([str(talk_check), '--render-trees', '--json', '--seed', str(seed)], capture_output=True,
                         text=True, timeout=600)
    if run.returncode not in (0, 1) or not run.stdout.startswith('['):
        return None
    return json.loads(run.stdout)


def check_renders(report: Report, rendered: list[dict] | None):
    if rendered is None:
        report.notes.append('renders: skipped (no talk_check built with --render-trees)')
        return
    for r in rendered:
        if r.get('problem'):
            report.fail('renders', f"dlg.{r['tree']}.{r['node']} ({r['voice']}, {r['accent']}): {r['problem']}: {r['text']}")


def listen(rendered: list[dict], out: Path):
    """The listen sheet: each tree's nodes, each line in the eight voices."""
    by = collections.defaultdict(lambda: collections.defaultdict(list))
    for r in rendered:
        by[(r['tree'], r['node'])][(r['rule'], r['alt'])].append(r)
    lines = ['# Dialogue trees in every voice', '',
             'Every line of every dialogue tree (Data/Talk/dialogue), rendered offline by `talk_check --render-trees` in '
             'plain speech and the seven dialects at a broad accent (the dialect\'s words every time; a light accent uses '
             'them about half the time). Fenshore speaks as Mirebridge; Saltwell has its own. Generated by '
             '`python3 tools/dialect_check.py --listen`.', '']
    tree = ''
    for (t, node), variants in by.items():
        if t != tree:
            lines += [f'## {t}', '']
            tree = t
        lines.append(f'### {t}.{node}')
        for (rule, alt), rs in variants.items():
            crit = rs[0].get('criteria') or ''
            voices = {r['voice']: r['text'] for r in rs if r['voice'] == 'plain' or r['accent'] == 'broad'}
            head = f'- `{rs[0]["template"]}`' + (f' [{crit}]' if crit else '')
            if len(set(voices.values())) == 1:
                lines.append(head + f' (every voice): {next(iter(voices.values()))}')
                continue
            lines.append(head)
            for voice, text in voices.items():
                lines.append(f'  - {voice}: {text}')
        lines.append('')
    out.write_text('\n'.join(lines) + '\n')


# ------------------------------------------------------------------ Main

def run(world: str = '', root: Path = TALK, allow_path: Path = ALLOW, people: list[dict] | None = None,
        render: bool = True) -> tuple[Report, dict]:
    allow = json.loads(allow_path.read_text()) if allow_path.exists() else {}
    gram = grammars(root)
    conf = settings(root)
    dialect_of = conf.get('dialect_of', {})
    lex = lexicon(gram, allow)
    report = Report()
    summary = {'lines_read': check_lexicon(report, lex, dialect_of, allow, root)}
    check_symbols(report, gram, root)
    if render:
        check_renders(report, renders())
    world_names: set[str] = set()
    homes = set(groups(root).get('towns', set())) | set(groups(root).get('cities', set()))
    if world:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import world_db
        key = world_db.resolve(world, 'dev')
        people = residents(key) if people is None else people
        homes = {p['_home'] for p in people if p.get('_home')}
        summary['origins'] = check_origins(report, people, dialect_of)
        with world_db.connect(key, 'editor') as conn:
            ids = [r[0] for r in conn.execute('SELECT id FROM world.worlds').fetchall()]
            wid = ids[0]
            check_tables(report, conn.execute("SELECT npc_id, home_region, grammar, rules FROM live.npc_voice "
                                              "WHERE world_id = %s AND written_by <> 'seed'", (wid,)).fetchall(), lex, dialect_of)
            check_dialect_rows(report, {r[0]: r[1] for r in conn.execute(
                'SELECT name, grammar FROM live.dialects WHERE world_id = %s', (wid,)).fetchall()}, root)
            world_names = {r[0] for r in conn.execute('SELECT name FROM world.interiors WHERE world_id = %s', (wid,)).fetchall() if r[0]}
            for table in ('world.places', 'world.cells', 'live.factions', 'world.chapters'):
                world_names |= {r[0] for r in conn.execute(f'SELECT name FROM {table} WHERE world_id = %s', (wid,)).fetchall() if r[0]}
    elif people is not None:
        homes = {p['_home'] for p in people if p.get('_home')}
        summary['origins'] = check_origins(report, people, dialect_of)
    check_reach(report, homes, allow, root)
    check_lore(report, world_names, allow, root)
    return report, summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--world', default='', help='also check a world (its residents, character tables, dialect rows)')
    parser.add_argument('--residents', default='', help='a talk_check --residents --json file instead of running it')
    parser.add_argument('--json', action='store_true', help='the report as JSON')
    parser.add_argument('--lexicon', action='store_true', help="print each dialect's distinctive phrases and stop")
    parser.add_argument('--listen', default='', help='write every tree line in every voice to this Markdown file')
    parser.add_argument('--no-render', action='store_true', help="don't render the trees (no talk_check needed)")
    parser.add_argument('--warnings', action='store_true', help='list every warning (by default, the first 30)')
    args = parser.parse_args()
    if args.lexicon:
        allow = json.loads(ALLOW.read_text()) if ALLOW.exists() else {}
        for d, phrases in lexicon(grammars(), allow).items():
            print(f"{d} ({len(phrases)}): " + ' | '.join(p + (' (saying)' if s else '') for p, s in phrases))
        return 0
    if args.listen:
        rendered = renders()
        if rendered is None:
            sys.exit('No talk_check with --render-trees built')
        listen(rendered, Path(args.listen))
        print(f'Wrote {args.listen} ({len(rendered)} renderings)')
        return 0
    people = None
    if args.residents:
        people = [json.loads(line) for line in Path(args.residents).read_text().splitlines() if line.startswith('{')]
    report, summary = run(args.world, people=people, render=not args.no_render)
    if args.json:
        print(json.dumps({'failures': report.failures, 'warnings': report.warnings, 'notes': report.notes,
                          'summary': summary}, indent=1))
    else:
        if 'origins' in summary:
            print('Origins (residents placed; who speak the place\'s dialect):')
            for town, o in summary['origins'].items():
                print(f"  {town:12} {o['residents']:4}  {o['speak_its_dialect']:4}  {o['share']:.0%}")
        for kind, items in report.failures.items():
            print(f'FAIL {kind}: {len(items)}')
            for m in items:
                print('  ' + m)
        for kind, items in report.warnings.items():
            print(f'warn {kind}: {len(items)}')
            for m in items[:None if args.warnings else 30]:
                print('  ' + m)
        for n in report.notes:
            print('note ' + n)
        print(f"{summary['lines_read']} lines read; "
              f"{sum(map(len, report.failures.values()))} failures, {sum(map(len, report.warnings.values()))} warnings")
    return 1 if report.failures else 0


if __name__ == '__main__':
    sys.exit(main())
