#!/usr/bin/env python3
"""G2, the mantle text's own rules (Docs/Design/84-living-conversations.md §9.6, §11, §14.4, §17.3; Phase 9): what the
game's loader (G1, `talk_check --mantle`) can't see. The same checks run on a batch of drafts, on the committed export and
fixture in ctest, and on every save (tools/mantle_store.py, which Atlas, the DM and mantle_load all write through).

  python3 tools/mantle_check.py PATH... [--world KEY] [--strict] [--json]     .mantle files (a directory: all under it)
  python3 tools/mantle_check.py --one ID --world KEY [--strict] [--json]      one mantle as the world's rows hold it
  --facts RESIDENTS.jsonl: the wearers' facts (talk_check --world KEY --residents --json), for the slots they fill
  python3 tools/mantle_check.py --pools --world KEY [PATH...] [--years 5] [--json]
      The pools (doc 84 §9.5, §12.1; P15's gate): each dialect's spare + ready mantles against the births forecast
      (tools/mantle_pool.py's), year by year, the wearers' deaths coming back once rested, green when a year starts with
      1.5 x its half year's births + 2 (the game's line); and the spares' share of sly + guarded within 5 points of the
      dialect's living residents'. PATHs: spare drafts not yet in the rows, counted in. Exit 1 if any pool isn't green.

What it refuses (each finding names the mantle and the item's key):
  * names: a first name or surname of any NPC of the world (live.npcs, living or dead: --world) outside a slot, and any
    capitalised word that isn't a sentence's first and isn't the realm's (tools/mantle_rules.json's allow, the places
    of tools/dialect_allow.json and Data/Economy/communities.json): names come only through slots (§11);
  * world text: no "dog", no "gods" (the Bright One and the Dark One), never "tells" for private messages, male or female
    only (mantle_rules.json's forbidden);
  * pronouns: he, she, him, her, his or hers in a sentence that names a person by a slot (#other.*#, #x.*#, one's kin...):
    a wolf's pronouns come from their sex (#other.they#);
  * every line of a scene or story, the tagged ones too (`b[...] #tag :`, `a@tag :`: fix 8; they were skipped before);
  * the persona: no pronoun (he, she, they...: it passes to a later wearer of either sex);
  * window lines (fix 8): a `dlg` line on a node whose tree's lines report facts (the news, the gossip, the business, a
    bare shop, a lodger, a warrant...: window_facts) tests at least one of them, so it says which case it is; the game
    makes such a line stand aside wherever a fact line holds (yieldToTree), but the writer says it;
  * a word's value that ends its own sentence (".", ";", "!", "?": defect 14, it breaks the window's templates);
  * truth (§9.6, Data/Talk/truth.json): words that take something for granted ("my nets", "back home", fog) need their
    item's criteria, its section's stage or the fit to imply it;
  * dialect (§10.1): another dialect's words (tools/dialect_check.py's lexicon); its own dialect's words written out where
    its accents allow none (they must come through symbols);
  * uniqueness (§9.1): no 5-word run shared with another mantle of the dialect; with --world, no oath or saying the
    same as another mantle's worn in the same town, a quirk at most twice in a town;
  * habits (§14.4): its oath and saying in at most 25% of its alternatives (the wearer's own: a listener's `b :` lines
    count neither here nor for the density);
  * the listener's words (Phase 11): a bare `b :` line is said by any listener in their own speech, so it never writes
    the mantle's dialect out (only `b[from=...]` or `b[dialect=...]` may); through symbols it says each listener's;
  * slots (§11, Phase 11): a slot is filled only when its fact is (mantle_rules.json's slots), so a line saying one is
    backed by it for truth ("my #self.spouse_word#": married); with --facts (a saved `talk_check --residents --json`),
    an item whose every alternative says a slot its wearer hasn't is warned (never heard from them);
  * the tier's minimum (mantle_rules.json): a `ready` mantle's symbols, lines, its answers to the core moves, window lines,
    stories, and a spare's young and adolescent sections; a draft under it is warned.
Warned, and refused with --strict (the batches of Phase 11 on): the tier's size (§9.3), the dialect density (60% of a
non-plain mantle's alternatives with a dialect marker: §10.1). Exit 1 on any refusal. Only reads.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import dialect_check  # noqa: E402
import mantle_text as T  # noqa: E402
import talk_lint  # noqa: E402
import talk_truth  # noqa: E402

ROOT = HERE.parent
TALK = ROOT / 'Data' / 'Talk'
RULES = HERE / 'mantle_rules.json'
SLOT = re.compile(r'#[^#\s]*#')
TAG = re.compile(r'^\{[^}]*\}\s*')
TELLING = {'says', 'said', 'told', 'tells', 'asked', 'reckons', 'swears'}
# A scene's or story's line: `: words` (a story's teller), `ask : words`, `a : words`, `b? : words`, `b[crit] : words`,
# `b[crit] #tag : words` (marks a branch), `a@tag : words`, `a@tag[crit] : words` (the branch's answer).
SCENE_LINE = re.compile(r'^(?:(?P<ask>ask)\s*|(?P<who>[ab])\??(?:@[a-z0-9_]+)?(?:\[(?P<crit>[^\]]*)\])?(?:\s*#[a-z0-9_]+)*\s*)?'
                        r':\s*(?P<words>.*)$')
STAGE = {'any': [], 'grown': ['self.stage=adult|old'], 'young': ['self.stage=young|adolescent'], 'adolescent': ['self.stage=adolescent'],
         'old': ['self.stage=old']}


@dataclass
class Finding:
    mantle: str
    key: str
    message: str
    warning: bool = False

    def __str__(self):
        return f"{'warning ' if self.warning else ''}{self.mantle}{' ' + self.key if self.key else ''}: {self.message}"


def rules() -> dict:
    return json.loads(RULES.read_text())


def realm_words(extra: set | None = None) -> set:
    """Capitalised words and phrases a mantle may say mid-sentence."""
    r = rules()
    allow = json.loads((HERE / 'dialect_allow.json').read_text())
    words = set(r.get('allow', [])) | set(allow.get('places', [])) | set(allow.get('legend', {}))
    communities = ROOT / 'Data' / 'Economy' / 'communities.json'
    if communities.exists():
        doc = json.loads(communities.read_text())
        for key in ('communities', 'farms', 'works'):
            for c in doc.get(key, []):
                if isinstance(c.get('name'), str):
                    words.add(re.sub(r'^the ', '', c['name']))
    return words | set(extra or ())


# ------------------------------------------------------------------ What a mantle says, item by item

def alternatives(text: str) -> list[str]:
    return [TAG.sub('', a.strip()) for a in dialect_check.split_alts(text)]


def said(it: T.Item) -> list[tuple[list[str], list[str], str]]:
    """(texts, criteria tokens, who) for each line of an item: its words, what stands behind them (the item's criteria
    and its section's stage), and who says them (a: the wearer, b: the one spoken to, p: the player's choice)."""
    out = []
    stage = list(STAGE.get(it.section, []))
    lines = it.body.split('\n')
    first = lines[0].strip()
    if it.kind == 'symbol':
        m = T.SYMBOL.match(first)
        return [(alternatives(first[m.end():]), (m.group(4) or '[]')[1:-1].split(), 'a')] if m else []
    if it.kind in ('line', 'opinion', 'dlg'):
        kw = first.split(None, 1)[0]
        _, head, crit, words, _, _ = T.split_item(first[len(kw):])
        tokens = crit.split() + stage
        if it.kind == 'dlg':
            tokens.append('concept=dlg.' + head)
        return [(alternatives(words), tokens, 'a')]
    when = []
    for raw in lines[1:]:
        t = raw.strip()
        if t.startswith('when '):
            when += t[5:].split()
    if it.kind == 'story':
        _, _, crit, _, _, _ = T.split_item(first[len('story'):])
        when += crit.split()
    for raw in lines[1:]:
        t = raw.strip()
        if not t or t.startswith('#'):
            continue
        if it.kind in ('rule', 'graft') and (t.startswith('say ') or t.startswith('say[')):
            m = re.match(r'^say(?:\[([^\]]*)\])?\s*(.*)$', t)
            out.append((alternatives(m.group(2)), when + (m.group(1) or '').split() + stage, 'a'))
        elif it.kind == 'graft' and t.startswith('choice '):
            words = t[7:].rsplit('->', 1)[0]
            out.append(([re.sub(r'\[[^\]]*\]\s*$', '', words).strip()], [], 'p'))
        elif it.kind in ('scene', 'story'):
            # (Tagged lines too, fix 8: the listener's `b[...] #tag :` that marks a branch, the wearer's `a@tag :` that
            # follows one. Before, this reader skipped them, and 52 problems in the pilot went unseen.)
            m = SCENE_LINE.match(t)
            if m:
                who = 'b' if m.group('who') == 'b' else 'a'
                out.append((alternatives(m.group('words')), when + (m.group('crit') or '').split() + (stage if who == 'a' else []), who))
    return out


def plain_words(text: str) -> str:
    return SLOT.sub(' ', text)


def sentences(text: str) -> list[str]:
    return [s for s in re.split(r'(?<=[.!?;:])\s+|\s+-\s+|\s+—\s+', text) if s.strip()]


# ------------------------------------------------------------------ The checks

def slot_facts(text: str, slots: dict) -> list:
    """What the slots a line says take for granted, as criteria (mantle_rules.json's slots)."""
    out = []
    for raw in SLOT.findall(text):
        name = raw.strip('#').split('.capitalize')[0]
        for need in slots.get(name, []):
            key, _, values = need.partition('=')
            out.append((key, '=', set(values.split('|'))) if values else (key, 'has', set()))
    return out


def unfilled(text: str, slots: dict, facts: dict) -> list[str]:
    """The slots a line says that this wearer's facts leave empty."""
    out = []
    for raw in SLOT.findall(text):
        name = raw.strip('#').split('.capitalize')[0]
        if name.startswith('self.') and name in slots and facts.get(name[5:]) in (None, '', 'no', 'none'):
            out.append(name)
    return out


LISTENER_GATE = re.compile(r'^(?:b\.|other\.)?(?:from|dialect)=')
PERSONA_PRONOUN = re.compile(r"\b(he|she|him|her|his|hers|himself|herself|they|them|their|theirs|themselves|he's|she's|"
                             r"they're|he'd|she'd|they'd|he'll|she'll|they'll)\b", re.I)
# How a window line is said, not what it reports: the hour, the meeting, the mood, the trade (and the section's stage),
# and a customer just come in (here.came_in, doc 84 Phase 12: the keeper looked up; a greeting's flavour, not news).
COSMETIC = {'topic.meeting', 'here.time', 'pair.disposition', 'trade', 'priority', 'concept', 'self.stage', 'stage', 'here.came_in'}


# The plain case of the commonest such nodes, for the message.
PLAIN_CASE = {'resident.news': '!world.news !world.event_words world.wagons!=raided world.death!=recent world.wedding!=recent',
              'resident.gossip': 'here.gossip!=yes', 'resident.business': 'business!=thriving|failing',
              'shop.greet': 'here.shop!=bare', 'inn.greet': 'here.inn_helper!=yes', 'inn.rest': 'here.lodging!=yes',
              'watch.hub': 'here.wanted=no', 'watch.fine': 'here.wanted=no', 'watch.law': 'here.wanted=no',
              'church.greet': 'here.day!=rest', 'church.service': 'here.day!=rest', 'master.learn': 'here.teaches=yes'}


def window_facts(talk: Path = TALK) -> dict[str, set]:
    """For each window node (<tree>.<node>), the facts its own tree's lines turn on (Data/Talk/dialogue/*.tree: the news,
    the gossip, the business, a bare shop, a warrant...), but for COSMETIC and a line of priority 10 or more (which wins
    whatever a mantle says). A mantle's line there outranks the tree's (+5): one that tests none of them would hide them
    ("Nothing worth the telling" while the salt road is shut), so the game makes it stand aside (Library::compileMantle,
    yieldToTree), and G2 refuses it, so the writer says which case it is."""
    out = {}
    for f in sorted((talk / 'dialogue').glob('*.tree')):
        tree = node = None
        for raw in f.read_text().splitlines():
            t = raw.strip()
            if t.startswith('tree '):
                tree = t.split()[1]
            elif t.startswith('node '):
                node = f'{tree}.{t.split()[1]}'
            elif t.startswith('say[') and node and ']' in t:
                toks = t[4:t.index(']')].split()
                pr = [int(x.split('=')[1]) for x in toks if x.startswith('priority=') and x.split('=')[1].isdigit()]
                if pr and pr[0] >= 10:
                    continue
                for k in crit_keys(' '.join(toks)):
                    if k not in COSMETIC:
                        out.setdefault(node, set()).add(k)
    return out


def crit_keys(crit: str) -> set:
    """The facts criteria test: `world.news`, `!world.news`, `here.shop!=bare` -> world.news, here.shop."""
    return {re.split(r'!=|<=|>=|=|<|>', tok.lstrip('!'))[0] for tok in crit.split()}


def check(mantles: list[T.Mantle], *, world: dict | None = None, strict: bool = False, others: list[T.Mantle] | None = None,
          facts: dict | None = None) -> list[Finding]:
    """Every finding for these mantles. `world` (from world_facts): the names of every NPC, where each wearer lives and
    who wears what; `others`: mantles of the world beside these (for uniqueness)."""
    r = rules()
    schema = talk_lint.schema(TALK)
    truth_rules, ids = talk_truth.load(TALK)
    implied = talk_truth.concepts(TALK)
    gram = dialect_check.grammars(TALK)
    allow = json.loads((HERE / 'dialect_allow.json').read_text())
    lex = dialect_check.lexicon(gram, allow)
    realm = realm_words()
    realm_lower = {w.lower() for w in realm}
    long_realm = [p.split() for p in realm if len(p.split()) > 2]   # ("Chapel of the Tide": a phrase, word by word)
    forbidden = [(re.compile(f['match'], re.I), f['why']) for f in r['forbidden']]
    pronoun = re.compile(r['pronouns'], re.I)
    person_slot = re.compile(r['person_slots'])
    slots = r.get('slots', {})
    names = set((world or {}).get('names', ()))
    reported = window_facts()
    out: list[Finding] = []
    everyone = list(mantles) + [o for o in (others or []) if o.id not in {m.id for m in mantles}]
    runs: dict[str, dict[tuple, set]] = {}            # dialect -> 5-word run -> the mantles saying it
    said_runs: dict[tuple, list] = {}                 # (mantle, key) -> its runs
    for m in everyone:
        for it in m.items:
            for texts, _, who in said(it):
                if who == 'p':
                    continue
                for t in texts:
                    words = re.findall(r"[a-z']+", plain_words(t).lower())
                    for i in range(len(words) - 4):
                        run = tuple(words[i:i + 5])
                        runs.setdefault(m.dialect, {}).setdefault(run, set()).add(m.id)
                        said_runs.setdefault((m.id, it.key), []).append(run)
    shared = {}
    for m in mantles:
        for (mid, key), found in said_runs.items():
            if mid != m.id:
                continue
            for run in found:
                others_ = runs[m.dialect][run] - {m.id}
                if others_:
                    shared.setdefault((mid, key), (sorted(others_)[0], ' '.join(run)))
                    break
    for m in mantles:
        def fail(key, message, warning=False):
            out.append(Finding(m.id, key, message, warning))
        own_symbols = {it.symbol.split('.')[0] for it in m.items if it.kind == 'symbol'}
        habits = {it.symbol.split('.')[0] for it in m.items if it.kind == 'symbol' and re.match(r'^(?:@\S+\s+)?[a-z0-9_.]+!', it.body)}
        dialect_symbols = set(gram.get(m.dialect, {})) if m.dialect != 'plain' else set()
        total = marked = habit_lines = 0
        texts_of = [(it, said(it)) for it in m.items]
        for text, key in [(m.persona, 'persona'), (m.title, 'title')]:
            texts_of.append((T.Item(key=key, kind='card'), [([text], [], 'a')]))
        wearer_facts = (facts or {}).get(m.written_for) if m.written_for else None
        for it, lines in texts_of:
            if wearer_facts is not None and it.kind not in ('symbol', 'card'):
                said_ = [t for texts, _, who in lines if who == 'a' for t in texts if t]
                missing = [unfilled(t, slots, wearer_facts) for t in said_]
                if said_ and all(missing):
                    fail(it.key, f'says {", ".join(sorted({x for xs in missing for x in xs}))}, which its wearer '
                                 f'({m.written_for}) hasn\'t: never heard from them (for a later wearer?)', True)
            for texts, tokens, who in lines:
                for t in texts:
                    if not t:
                        continue
                    low = plain_words(t)
                    for pattern, why in forbidden:
                        if pattern.search(low):
                            fail(it.key, f'"{t}": {why}')
                    # Names only through slots: a name of the world's anywhere but a sentence's start; there (where "Stone
                    # keeps..." is a word), when another name, a comma or a word of telling follows it ("Hollin says...").
                    for s_ in sentences(low):
                        ws = re.findall(r"[A-Za-z][A-Za-z'-]*|,", s_)
                        for i, w in enumerate(ws):
                            if w not in names or w in realm:
                                continue
                            nxt = ws[i + 1] if i + 1 < len(ws) else ''
                            if i > 0 or nxt in names or nxt == ',' or not nxt or nxt.lower() in TELLING:
                                fail(it.key, f'"{w}" is the name of someone in the world: names come only through slots '
                                             f'(#self.spouse#...)')
                    for s in sentences(low):
                        tokens_ = re.findall(r"[A-Za-z][A-Za-z'-]*", s)
                        in_phrase = {j + k for j in range(len(tokens_)) for p_ in long_realm
                                     if [re.sub(r"'s$", '', x) for x in tokens_[j:j + len(p_)]] == p_ for k in range(len(p_))}
                        for i, w in enumerate(tokens_[1:], 1):
                            if not w[0].isupper() or w in names or i in in_phrase:
                                continue
                            two = f'{w} {tokens_[i + 1]}' if i + 1 < len(tokens_) else ''
                            prev2 = f'{tokens_[i - 1]} {w}'
                            bare = lambda x: re.sub(r"'s$", '', x)      # (A possessive: "the Bright One's ear".)
                            if (w in realm or w.lower() in realm_lower or bare(two) in realm or bare(prev2) in realm or
                                    bare(w) in realm):
                                continue
                            fail(it.key, f'"{w}" is capitalised mid-sentence: a name? Names come only through slots; a place of '
                                         f'the realm goes in mantle_rules.json allow')
                    if person_slot.search(t) and pronoun.search(plain_words(t)):
                        fail(it.key, f'"{t}": he, she, him or her beside a person named by a slot; say it by their sex '
                                     f'(#other.they#, #x.their#)')
                    if it.kind == 'card' or who == 'p':
                        continue
                    # (The wearer's own alternatives, for the habits and the density: a scene's `b :` lines are any
                    # listener's, said in their own speech, so they neither count for the mantle's dialect nor against it.)
                    counted = it.kind != 'symbol' and who == 'a'

                    if counted:
                        total += 1
                        if any(f'#{h}' in t for h in habits):
                            habit_lines += 1
                    # Dialect: another's words never; its own written out only where its accents allow it.
                    own = 'mirebridge' if m.dialect == 'fenshore' else m.dialect
                    hits = dialect_check.hits(t, lex)
                    gated = any(LISTENER_GATE.match(x) for x in tokens)
                    for d, phrase in hits:
                        if d != own:
                            fail(it.key, f'"{phrase}" is {d} speech, not {m.dialect}\'s')
                        elif who == 'b' and not gated:
                            fail(it.key, f'"{phrase}" is {d} speech written out in a listener\'s line: any listener says it, in '
                                         f'their own speech; through a symbol, or b[from=...]')
                        elif 'none' in m.accents:
                            fail(it.key, f'"{phrase}" is {d} speech written out, and its wearer may speak with no accent: '
                                         f'through a symbol (the dialect\'s word, or its own words)')
                    syms = {x.strip('#').split('.')[0] for x in SLOT.findall(t)}
                    if counted and ((syms & (own_symbols | dialect_symbols)) or any(d == own for d, _ in hits)):
                        marked += 1
                    # Truth: what the words take for granted, behind them.
                    crit = talk_lint.criteria(tokens, 'a' if who == 'a' else 'b', False, schema) + talk_lint.held(tokens, implied)
                    if who == 'a':
                        crit += slot_facts(t, slots)
                    crit.append(('self.mantle', '=', {m.id}))
                    for rule in talk_truth.matches(truth_rules, t):
                        if rule.topics and talk_lint.on_topic(crit, rule.topics) is not True:
                            continue                # (Scoped, as talk_lint scopes it: "That's hard" on news only.)
                        if not talk_lint.backed(rule, crit):
                            needs = ', '.join(f'{n.key}{n.op}{"|".join(sorted(n.values))}' for n in rule.needs + rule.needs_any)
                            fail(it.key, f'"{t}" takes for granted what its criteria don\'t say ({rule.id}: {needs})')
                    if ids.search(t):
                        fail(it.key, f'"{t}" holds an identifier')
            if it.kind == 'symbol':
                for t in lines[0][0] if lines else []:
                    if t.rstrip().endswith(('.', ';', '!', '?')):
                        fail(it.key, f'"{t}" ends its own sentence: a word is spliced into lines ("Morning to you; the loaves are '
                                     f'honest., neighbour"), so it ends without a stop (defect 14)')
        # Shared runs (uniqueness within the dialect).
        for (mid, key), (other, run) in shared.items():
            if mid == m.id:
                fail(key, f'"{run}" is word for word in {other} (no two mantles of a dialect share a 5-word run)')
        # Habits.
        if total and habit_lines / total > r['habits']:
            fail('', f'its habits are in {habit_lines} of {total} alternatives ({habit_lines / total:.0%}; at most {r["habits"]:.0%})')
        # Density.
        if m.dialect != 'plain' and total and marked / total < r['density']:
            fail('', f'{marked} of {total} alternatives carry a dialect marker ({marked / total:.0%}; {r["density"]:.0%} wanted)', not strict)
        # The tier's minimum and size.
        tier = r['tiers'].get(m.tier, {})
        need = tier.get('minimum', {})
        lines_ = [it for it in m.items if it.kind in ('line', 'rule', 'scene', 'story')]
        counts = {'symbols': sum(it.kind == 'symbol' for it in m.items), 'lines': len(lines_),
                  'dlg': sum(it.kind == 'dlg' for it in m.items), 'stories': sum(it.kind == 'story' for it in m.items)}
        short = [f'{counts[k]} {k} of {need[k]}' for k in ('symbols', 'lines', 'dlg', 'stories') if counts[k] < need.get(k, 0)]
        done = set()
        for it in m.items:
            if it.kind == 'line':
                kw = it.body.split(None, 1)[0]
                _, head, _, _, _, _ = T.split_item(it.body.split('\n')[0][len(kw):])
                for mv in head.split():
                    parts = mv.split('.')
                    done |= {'.'.join(parts[:i]) for i in range(1, len(parts) + 1)}
            elif it.kind == 'rule':
                for raw in it.body.split('\n'):
                    if raw.strip().startswith('does '):
                        for mv in raw.strip()[5:].split():
                            parts = mv.split('.')
                            done |= {'.'.join(parts[:i]) for i in range(1, len(parts) + 1)}
        short += [f'no line answering {mv}' for mv in need.get('moves', []) if mv not in done]
        short += [f'no {s} section' for s in need.get('sections', []) if not any(it.section == s for it in m.items)]
        if short:
            fail('', f'under its tier\'s minimum ({m.tier}): ' + '; '.join(short), m.state != 'ready')
        size = len([it for it in m.items if it.kind != 'symbol'])
        if tier.get('size') and size < tier['size']:
            fail('', f'{size} items of the {m.tier} tier\'s {tier["size"]} (§9.3)', not strict)
        # The marks name its own habits (the loader checks they are its words).
        for mark in m.marks:
            if mark not in habits:
                fail('marks', f"'{mark}' is not a habit of its own (name! in its words)")
            else:
                # (Phase 11: a mark is drawn again for a wearer the neighbours would remember it in, §12.4, from the
                # dialect's bank or else from the mark's own other alternatives: one alternative can't be drawn again.)
                alts = {a for it in m.items if it.kind == 'symbol' and it.symbol.split('.')[0] == mark for a in (said(it)[0][0] if said(it) else [])}
                if len(alts) < 2:
                    fail('marks', f"'{mark}' has {len(alts)} alternative: a mark needs two or more, to be drawn again (§12.4)")
        if m.persona and '#self.first#' not in m.persona and m.persona:
            fail('persona', 'the persona names its wearer by #self.first# (it is passed on)', True)
        if PERSONA_PRONOUN.search(plain_words(m.persona or '')):
            fail('persona', f'"{PERSONA_PRONOUN.search(plain_words(m.persona)).group(0)}": the persona passes to the next wearer, '
                            f'of either sex; say it with no pronoun ("#self.first# keeps..., and keeps a jar of stones")')
        # A window line on a node whose tree reports facts says which case it is (fix 8, doc 84 §42).
        for it in m.items:
            if it.kind != 'dlg':
                continue
            _, head, crit, _, _, _ = T.split_item(it.body.split('\n')[0][len('dlg'):])
            need = reported.get(head.strip(), set())
            if need and not (crit_keys(crit) & need):
                case = PLAIN_CASE.get(head.strip(), ' '.join(f'{k}!=...' for k in sorted(need)))
                fail(it.key, f'{head.strip()}: its tree reports {", ".join(sorted(need))} and this line tests none of them '
                             f'(it would hide them; the game makes it stand aside): gate it on its case, the plain one '
                             f'[{case}], or one line a case (handbook §4.5.8)')
    # In a world: who wears what, where; a town's oaths, sayings and quirks.
    if world:
        town = world.get('town', {})
        wearer = world.get('wearer', {})
        by_town: dict[str, list[T.Mantle]] = {}
        for m in everyone:
            w = wearer.get(m.id) or m.written_for
            if w and town.get(w):
                by_town.setdefault(town[w], []).append(m)
        for m in mantles:
            if m.written_for and m.written_for not in world.get('npcs', set()):
                out.append(Finding(m.id, 'written_for', f'{m.written_for} is not an NPC of the world'))
            w = wearer.get(m.id) or m.written_for
            if not w or not town.get(w):
                continue
            neighbours = [o for o in by_town[town[w]] if o.id != m.id]
            for sym in ('oath', 'saying'):
                mine = {a for it in m.items if it.kind == 'symbol' and it.symbol == sym for a in said(it)[0][0]}
                for o in neighbours:
                    theirs = {a for it in o.items if it.kind == 'symbol' and it.symbol == sym for a in said(it)[0][0]}
                    for same in sorted(mine & theirs):
                        out.append(Finding(m.id, f'words.{sym}', f'"{same}" is {o.id}\'s {sym} too, in the same town'))
            if m.quirk and sum(o.quirk == m.quirk for o in neighbours) >= 2:
                out.append(Finding(m.id, 'quirk', f'{m.quirk}: three in one town have it (two at most)'))
    return out


# ------------------------------------------------------------------ The world's side

def town_of(home_area: str) -> str:
    """The town a home is in, by its area's id: a town's cells are cn_<town>_..., Ridgemere's rm_ and r2_."""
    m = re.match(r'^cn_([a-z]+)_', home_area or '')
    if m:
        return m.group(1)
    return 'ridgemere' if re.match(r'^(rm|r2|rmx)_', home_area or '') else ''


def world_facts(conn, world_id: str) -> dict:
    """What the world adds: every NPC's names and ids, the town each lives in, and who wears which mantle."""
    names, npcs, town = set(), set(), {}
    for npc_id, name, home in conn.execute('SELECT id, name, coalesce(home_area, \'\') FROM live.npcs WHERE world_id = %s',
                                           (world_id,)).fetchall():
        npcs.add(npc_id)
        town[npc_id] = town_of(home)
        for part in re.findall(r"[A-Z][A-Za-z'-]+", name or ''):
            names.add(part)
    wearer = {}
    if conn.execute("SELECT to_regclass('live.mantles') IS NOT NULL").fetchone()[0]:
        for mantle_id, npc_id in conn.execute('SELECT mantle_id, npc_id FROM live.npc_voice WHERE world_id = %s AND mantle_id IS NOT '
                                              'NULL AND released_day IS NULL', (world_id,)).fetchall():
            wearer[mantle_id] = npc_id
    return {'names': names - {'Old', 'The', 'Grandam'}, 'npcs': npcs, 'town': town, 'wearer': wearer}


def world_id_of(conn) -> str:
    worlds = [r[0] for r in conn.execute('SELECT id FROM world.worlds').fetchall()]
    if len(worlds) != 1:
        raise SystemExit(f'Expected one world in the database, found {worlds}')
    return worlds[0]


def world_mantles(conn, world_id: str, only: str | None = None) -> list[T.Mantle]:
    """The world's mantles, as their rows hold them."""
    cols = ('id', 'dialect', 'tier', 'state', 'format', 'accents', 'title', 'written_for', 'fits', 'prefers', 'traits', 'quirk',
            'card', 'persona', 'revision', 'written_by')
    rows = conn.execute(f"SELECT {', '.join(cols)} FROM live.mantles WHERE world_id = %s" + (' AND id = %s' if only else '') +
                        ' ORDER BY id', (world_id, only) if only else (world_id,)).fetchall()
    out = []
    for row in rows:
        r = dict(zip(cols, row))
        items = [dict(zip(('key', 'section', 'kind', 'move', 'position', 'body'), i)) for i in conn.execute(
            'SELECT key, section, kind, move, position, body FROM live.mantle_lines WHERE world_id = %s AND mantle_id = %s',
            (world_id, r['id'])).fetchall()]
        out.append(T.read(T.write(T.Mantle.from_columns(r, items)), f'db:{r["id"]}'))
    return out


# ------------------------------------------------------------------ The pools (--pools: doc 84 §9.5, §12.1, P15's gate)

def pool_health(report: dict, drafts: list[T.Mantle] | None = None) -> dict:
    """Each dialect's pool, year by year, from mantle_pool's forecast (`report`) and any spare drafts not yet in the rows:
    {d: {'years': [{'year', 'supply', 'births', 'back', 'green'}], 'honesty': (spares %, living %), 'ok': bool, 'why': [...]}}.
    A year is green when its start's supply (spare + ready mantles, the game's own line: §12.1) is at least 1.5 x the half
    year's births + 2; each year the births are drawn from it and the wearers' deaths come back to it once rested (the
    resting ones in the first). The spares' share of sly + guarded is within 5 points of the dialect's living residents'
    (§9.5; a pool with no spares yet has nothing to compare)."""
    out = {}
    extra = {}
    for m in drafts or []:
        if m.tier == 'spare':
            extra.setdefault(m.dialect, []).append(m)
    for d, p in report['pools'].items():
        supply = p['spare'] + p['ready'] + len(extra.get(d, []))
        years, why = [], []
        back = p['resting']
        deaths = p.get('wearerDeathsByYear') or []
        for y, births in enumerate(p['birthsByYear'], 1):
            green = supply >= 1.5 * births / 2 + 2
            years.append({'year': y, 'supply': supply, 'births': births, 'back': back, 'green': green})
            supply = max(0, round(supply - births + back))
            back = deaths[y - 1] if y - 1 < len(deaths) else 0
        short = [y for y in years if not y['green']]
        if short:
            f = short[0]
            why.append(f"{len(short)} of {len(years)} years short (year {f['year']}: {f['supply']} to give, {f['births']:.1f} "
                       f"births; green from {1.5 * f['births'] / 2 + 2:.1f})")
        sly = lambda mix: round(mix.get('sly', 0) + mix.get('guarded', 0), 1) if mix else None
        honesty = dict(p['honesty'] or {})
        drafts_d = extra.get(d, [])
        spares = p['spare'] + len(drafts_d)
        if drafts_d:
            # (The drafts' honesty beside the rows' spares, weighted by count.)
            counts = {h: honesty.get(h, 0) * p['spare'] / 100 for h in ('frank', 'plain', 'guarded', 'sly')}
            for m in drafts_d:
                h = m.traits.get('honesty', 'plain')
                counts[h] = counts.get(h, 0) + 1
            honesty = {h: 100 * n / spares for h, n in counts.items()} if spares else {}
        pool_sly, living_sly = (sly(honesty) if spares else None), sly(p['livingHonesty'])
        if pool_sly is not None and living_sly is not None and abs(pool_sly - living_sly) > 5:
            why.append(f'honesty: {pool_sly:.1f}% of its spares sly or guarded, {living_sly:.1f}% of its living (within 5 points)')
        out[d] = {'years': years, 'honesty': (pool_sly, living_sly), 'spares': spares, 'ok': not why, 'why': why}
    return out


def pools_main(a) -> int:
    """mantle_check --pools --world KEY [PATH...]: the pools' health for --years forecast years and the spares' honesty mix."""
    import mantle_pool as P
    import world_db as W
    with W.connect(W.resolve(a.world), 'editor') as conn:
        with conn.transaction():
            data = P.read_world(conn)
    report = P.forecast(data, a.years)
    # The wearers' deaths by dialect, year by year (their mantles come back once rested).
    dialect_of = {c['id']: c['dialect'] for c in data['cards']}
    for d, p in report['pools'].items():
        ages = [data['npcs'][b['npc']]['age'] for b in data['bindings']
                if b['alive'] and b['released'] is None and dialect_of.get(b['mantle']) == d and b['npc'] in data['npcs']]
        p['wearerDeathsByYear'] = [round(sum(P.yearly_death(age + y) for age in ages), 2) for y in range(a.years)]
    have = {c['id'] for c in data['cards']}
    drafts = []
    for f in files(a.paths):
        try:
            m = T.read(f.read_text(), str(f))
        except T.MantleError as e:
            print('\n'.join(e.problems))
            return 1
        if m.id not in have:
            drafts.append(m)
    health = pool_health(report, drafts)
    if a.json:
        print(json.dumps(health, indent=1))
    else:
        print(f"{report['world']}: day {report['day']:.1f}; pools over {a.years} forecast years" +
              (f' (with {len(drafts)} drafts not in the rows)' if drafts else ''))
        print(f"{'pool':<12} {'spares':>6}  {'years green':<12} honesty (spares / living, % sly+guarded)")
        for d, h in health.items():
            g = ''.join('+' if y['green'] else '-' for y in h['years'])
            ps, ls = h['honesty']
            print(f"{d:<12} {h['spares']:>6}  {g:<12} {'-' if ps is None else ps} / {'-' if ls is None else ls}"
                  f"{'' if h['ok'] else '   ' + '; '.join(h['why'])}")
        bad = [d for d, h in health.items() if not h['ok']]
        print(f'{len(health)} pools: {len(bad)} not green' + (f' ({", ".join(bad)})' if bad else ''))
    return 1 if any(not h['ok'] for h in health.values()) else 0


def files(paths: list[str]) -> list[Path]:
    out = []
    for p in map(Path, paths):
        out += sorted(p.rglob('*.mantle')) if p.is_dir() else [p]
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('paths', nargs='*')
    ap.add_argument('--world')
    ap.add_argument('--one')
    ap.add_argument('--strict', action='store_true')
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--facts', help="the wearers' facts: a saved talk_check --world KEY --residents --json")
    ap.add_argument('--pools', action='store_true', help='the pools\' health and the spares\' honesty (with --world; PATHs: spare drafts)')
    ap.add_argument('--years', type=int, default=5)
    a = ap.parse_args()
    if a.pools:
        if not a.world:
            ap.error('--pools needs --world')
        return pools_main(a)
    facts = None
    if a.facts:
        facts = {}
        for line in Path(a.facts).read_text().splitlines():
            if line.startswith('{'):
                d = json.loads(line)
                facts[d['id']] = d
    mantles, problems = [], []
    for f in files(a.paths):
        try:
            mantles.append(T.read(f.read_text(), str(f)))
        except T.MantleError as e:
            problems += e.problems
    world, others = None, []
    if a.world:
        import world_db as W
        with W.connect(W.resolve(a.world), 'editor') as conn:
            world_id = world_id_of(conn)
            world = world_facts(conn, world_id)
            others = world_mantles(conn, world_id)
            if a.one:
                mantles = [m for m in others if m.id == a.one]
                if not mantles:
                    problems.append(f'{a.one}: no such mantle in {a.world}')
    elif a.one:
        ap.error('--one needs --world')
    found = check(mantles, world=world, strict=a.strict, others=others, facts=facts)
    errors = [f for f in found if not f.warning]
    if a.json:
        print(json.dumps({'problems': problems, 'findings': [vars(f) for f in found]}, indent=1))
    else:
        for p in problems:
            print(p)
        for f in found:
            print(f)
        print(f'{len(mantles)} mantles: {len(errors) + len(problems)} refusals, {len(found) - len(errors)} warnings')
    return 1 if errors or problems else 0


if __name__ == '__main__':
    sys.exit(main())
