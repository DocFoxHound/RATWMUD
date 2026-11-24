#!/usr/bin/env python3
"""The story words check (Docs/Design/86-quests-and-the-ninth-howl.md, engine spec V2, with T4): every word a story says.

  python3 tools/story_words.py PROGRAM.json ...        check programs (offline: no giver's dialect, no world ids)
  python3 tools/story_words.py --stories DIR           every *.json program under a directory
  python3 tools/story_words.py --world ridgemere3 ...  and with the world's residents' voices and ids (read only)
  python3 tools/story_words.py --json ...              the problems as JSON

It reads every word field of a program: offers (say, thanks, during, ask, accept, decline, handIn), npc.offer's words and
reply, npc.line, npc.greet, npc.speak, story.tell, board.post, journal.note, quest titles, summaries and labels, deed
details, titles, claims, a phase's journal, a finish's line, scene lines, and every other text of the catalogue a wolf may
read (a figure's name, title and about; what people say of a market, a harvest or prices; a festival's name, a project's
title, the reasons of a warrant, a payment or a standing). It checks:
  1. symbols: every #symbol# a resident says in the window (an offer, npc.line, npc.greet, a reply) is in the talk library
     (a grammar's symbol, or a fact such as #player.title#); a stray [ or ], or an unclosed #, is refused; words said as
     written (npc.speak, a scene's lines, and every word no resident says) hold no #symbols#.
  2. forbidden words (Data/Storylines/words.json): dog(s), puppy, god(s), deity... and "tell" for a private message: refused.
  3. dialect by the giver (with the world's voices): a resident's words holding another dialect's set phrases (the
     dialect check's lexicon): a warning.
  4. ids in speech: a word like an id (rm111, rm_eleanor_saltcot, wolf-…; with the world, any resident's or place's id): refused.
  5. lengths with every fill at its widest: 300 for offer words, 120 for asks, 200 for npc.line and greetings: refused past.
  6. face banners: a story.tell from a resident is a warning, and refused when it holds quoted speech; an npc.speak that
     speaks to a wolf ("you") is a warning: a resident's words to a wolf go in the window.
  7. a clearing (story.cleared) that begins with "never" or "did not": a warning (the game says "never" itself).
  8. a fact symbol (#player.title#, #topic.x#) in a story's words: a warning (said only where the fact is known).
Approval runs it (story_runner.world_checks: refusals block, warnings are told), and so does the Problems list.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from functools import lru_cache
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORDS = ROOT / 'Data' / 'Storylines' / 'words.json'
TALK = ROOT / 'Data' / 'Talk'
LIMITS = {'say': 300, 'thanks': 300, 'during': 300, 'ask': 120, 'accept': 120, 'decline': 120, 'handIn': 120, 'duringAsk': 120}
# Facts a resident's words may read (#section.name#); a section's names are checked where facts.json lists them all.
FACT_SECTIONS = ('self', 'a', 'b', 'x', 'topic', 'here', 'world', 'player', 'pair', 'conv', 'person', 'them', 'them2')
CHECKED_SECTIONS = ('player',)
SYMBOL = re.compile(r'#([^#\s]*)#')
FILL = re.compile(r'\{\{|\}\}|\{([A-Za-z0-9_][A-Za-z0-9_.:-]*?)(\.of)?\}|\{@[a-z]:[^}]*\}')
QUOTED = re.compile(r'"[^"]+"|“[^”]+”|‘[^’]+’|(?<![A-Za-z])\'[^\']{2,}\'(?![A-Za-z])')
# (A wolf's id is "wolf-" and 32 hex digits: a word such as "wolf-pack" or "wolf-wide" is a word, one with a digit an id.)
ID_WORD = re.compile(r"(?<![A-Za-z0-9_'-])((?:wolf-(?:[0-9a-f]{8,}|[A-Za-z0-9_-]*[0-9][A-Za-z0-9_-]*))|[A-Za-z0-9-]*_[A-Za-z0-9_-]*|[a-z]+[0-9]+[a-z0-9]*)(?![A-Za-z0-9_-])")
YOU = re.compile(r'\b(you|your|yours|yourself)\b', re.I)


@lru_cache(maxsize=1)
def words():
    return json.loads(WORDS.read_text())


@lru_cache(maxsize=1)
def symbols():
    """The talk library's grammar symbols (every grammar's) and the facts.json player names, as the expander knows them."""
    import dialect_check

    names = set()
    for table in dialect_check.grammars(TALK).values():
        names |= set(table)
    facts = json.loads((TALK / 'facts.json').read_text())
    player = {k for k in facts.get('player', {}) if k != 'about'}
    return names, player


@lru_cache(maxsize=1)
def lexicon():
    import dialect_check

    allow = json.loads(dialect_check.ALLOW.read_text()) if dialect_check.ALLOW.exists() else {}
    return dialect_check.lexicon(dialect_check.grammars(TALK), allow)


def dialect_of(home_region, accent):
    """A resident's dialect from their voice row: 'plain' for none (an accent of 'none', the city, no origin)."""
    import dialect_check

    mapping = dialect_check.settings(TALK).get('dialect_of', {})
    d = mapping.get(home_region or '', home_region or '')
    return d if accent != 'none' and d in dialect_check.DIALECTS else 'plain'


# ---------------------------------------------------------------------------------------------- the word fields

def _site(path, at, text, kind, speaker=None, limit=None, spoken=False, voiced=None):
    """`spoken`: a resident says it (their dialect is checked); `voiced`: said through their voice, so #symbols# fill (an
    offer's words, npc.line, npc.greet, a choice's reply). A scene's lines and npc.speak are said as written."""
    return {'path': path, 'at': at, 'text': text, 'kind': kind, 'speaker': speaker, 'limit': limit, 'spoken': spoken,
            'voiced': spoken if voiced is None else voiced}


EFFECT_WORDS = {
    # kind: [(param, what, spoken by (a param naming the resident), limit)]
    'npc.line': [('text', 'line', 'npc', 200)], 'npc.greet': [('text', 'greet', 'npc', 200)],
    'npc.speak': [('text', 'speak', 'npc', None)], 'npc.offer': [('words', 'choice', None, None), ('reply', 'reply', 'npc', None)],
    'story.tell': [('text', 'tell', None, None)], 'board.post': [('text', 'board', None, None)],
    'journal.note': [('text', 'journal', None, None)], 'deed.award': [('detail', 'deed', None, None)],
    'title.give': [('title', 'title', None, None)], 'belief.plant': [('claim', 'claim', None, None)],
    'story.ping': [('label', 'label', None, None)], 'visitor.add': [('name', 'name', None, None), ('description', 'look', None, None)],
    # (Every other text of the catalogue a wolf may read: told, posted, or said of in the world.)
    'title.take': [('title', 'title', None, None)], 'deed.revoke': [('detail', 'deed', None, None)],
    'figure.set': [('name', 'name', None, None), ('title', 'title', None, None), ('about', 'about', None, None)],
    'times.nudge': [('words', 'news', None, None)], 'harvest.override': [('words', 'news', None, None)],
    'prices.reset': [('words', 'news', None, None)], 'festival.call': [('name', 'name', None, None)],
    'project.post': [('title', 'title', None, None)], 'warrant.issue': [('reason', 'reason', None, None)],
    'character.grant': [('reason', 'reason', None, None)], 'standing.adjust': [('reason', 'reason', None, None)],
    'faction.relation': [('reason', 'reason', None, None)],
    'item.give': [('name', 'name', None, 60)],   # (Doc 86, J9: a story's own item's name, shown in the pack.)
    'foe.add': [('name', 'name', None, 60), ('description', 'look', None, None)],   # (Doc 86, C10: a foe's name and looks.)
}
# Said as written, never in a voice: a resident's words aloud (npc.speak) fill no #symbols#.
UNVOICED = frozenset({'npc.speak'})
# The catalogue's text parameters no wolf reads: every other is in EFFECT_WORDS (test_story_words checks).
EXEMPT = frozenset({'cast.pick.job', 'cast.pick.quarter'})   # (Doc 86, C7: what a casting looks for, never said to anyone.)


def effect_sites(program):
    """Every effect of the program with where it is (as story_runner._effect_sites), the loose way: a program that is not
    sound yet is read as far as it can be."""
    import story_triggers

    for qi, q in enumerate(_list(program.get('quests'))):
        for part in ('rewards', 'bonus'):
            for ei, e in enumerate(_list(q.get(part))):
                if isinstance(e, dict):
                    yield (f'quests[{qi}].{part}[{ei}]', {'kind': 'quest', 'index': qi, 'id': q.get('id'), 'part': f'{part}[{ei}]'}, e,
                           q.get('giver') if isinstance(q.get('giver'), str) else None)   # (Doc 86, C7: a cast giver's dialect is not known.)
    phases = _list(program.get('phases'))
    for pi, ph in enumerate(phases):
        for ei, e in enumerate(_list(ph.get('effects'))):
            if isinstance(e, dict):
                yield f'phases[{pi}].effects[{ei}]', {'phase': pi, 'phaseId': ph.get('id'), 'kind': 'effect', 'index': ei}, e, None
    lists = [(None, None, 'triggers', _list(program.get('triggers')))] + [
        (i, ph.get('id'), f'phases[{i}].triggers', _list(ph.get('triggers'))) for i, ph in enumerate(phases)]
    for pi, pid, path, rules in lists:
        for ti, t in enumerate(rules):
            if not isinstance(t, dict):
                continue
            for k, a in enumerate(story_triggers.walk_loose(t.get('actions'))):
                if a.get('do') == 'effect' and isinstance(a.get('effect'), dict):
                    yield (f'{path}[{ti}].actions', {'phase': pi, 'phaseId': pid, 'kind': 'trigger', 'index': ti, 'id': t.get('id'),
                                                     'part': f'action {k + 1}'}, a['effect'], None)


def _list(v):
    return [x for x in v if isinstance(x, dict)] if isinstance(v, list) else []


def sites(program):
    """Every word field of a program: {path, at, text, kind, speaker (a resident's id, when a resident says it), limit,
    spoken (said by a resident: their voice fills #symbols#)}."""
    out = []
    for path, at, e, giver in effect_sites(program):
        for key, what, by, limit in EFFECT_WORDS.get(e.get('kind'), ()):
            text = e.get(key)
            if not isinstance(text, str) or not text:
                continue
            speaker = e.get(by) if by and isinstance(e.get(by), str) else (giver if what == 'greet' and not isinstance(e.get('npc'), str) else None)
            out.append({**_site(f'{path}.{key}', at, text, what, speaker, limit, spoken=by is not None,
                                voiced=by is not None and e.get('kind') not in UNVOICED),
                        'effect': e.get('kind'), 'from': e.get('from') if e.get('kind') == 'story.tell' else None,
                        'claimKind': e.get('claimKind') if e.get('kind') == 'belief.plant' else None})
    for qi, q in enumerate(_list(program.get('quests'))):
        at = {'kind': 'quest', 'index': qi, 'id': q.get('id')}
        for key in ('title', 'summary'):
            if isinstance(q.get(key), str):
                out.append(_site(f'quests[{qi}].{key}', {**at, 'part': key}, q[key], 'quest'))
        for oi, o in enumerate(_list(q.get('objectives'))):
            if isinstance(o.get('label'), str):
                out.append(_site(f'quests[{qi}].objectives[{oi}].label', {**at, 'part': f'objectives[{oi}]'}, o['label'], 'label'))
        offer = q.get('offer') if isinstance(q.get('offer'), dict) else {}
        giver = q.get('giver') if isinstance(q.get('giver'), str) else None
        for key, limit in LIMITS.items():
            if isinstance(offer.get(key), str):
                spoken = key in ('say', 'thanks', 'during')   # (The giver says these; the rest are the wolf's choices.)
                out.append(_site(f'quests[{qi}].offer.{key}', {**at, 'part': 'offer.' + key}, offer[key], 'offer' if spoken else 'choice',
                                 giver if spoken else None, limit, spoken))
    for pi, ph in enumerate(_list(program.get('phases'))):
        if isinstance(ph.get('journal'), str):
            out.append(_site(f'phases[{pi}].journal', {'phase': pi, 'phaseId': ph.get('id'), 'kind': 'phase'}, ph['journal'], 'journal'))
    import story_triggers
    for pi, ph in [(None, program)] + list(enumerate(_list(program.get('phases')))):
        for ti, t in enumerate(_list(ph.get('triggers'))):
            for a in story_triggers.walk_loose(t.get('actions')):
                if a.get('do') == 'finish' and isinstance(a.get('text'), str):
                    base = 'triggers' if pi is None else f'phases[{pi}].triggers'
                    out.append(_site(f'{base}[{ti}]', {'phase': pi, 'kind': 'trigger', 'index': ti, 'id': t.get('id'), 'part': 'finish'},
                                     a['text'], 'finish'))
    for si, scene in enumerate(_list(program.get('scenes'))):
        cast = {c.get('id'): c.get('npc') for c in _list(scene.get('cast'))}
        for bi, beat in enumerate(_list(scene.get('beats'))):
            if beat.get('type') in ('line', 'choice', 'response') and isinstance(beat.get('text'), str):
                speaker = cast.get(beat.get('speaker')) if isinstance(cast.get(beat.get('speaker')), str) else None
                out.append(_site(f'scenes[{si}].beats[{bi}].text', {'kind': 'scene', 'index': si, 'id': scene.get('id'), 'part': f'beats[{bi}]'},
                                 beat['text'], 'scene', speaker, None, spoken=beat.get('type') != 'choice', voiced=False))
    return out


# ---------------------------------------------------------------------------------------------- the checks

def _plain(text):
    """The words with fills and #symbols# taken out (each a break)."""
    return SYMBOL.sub(' | ', FILL.sub(' | ', text))


def _widest(text, decls):
    import story_triggers

    n = len(story_triggers.FILL.sub(lambda m: '{' if m.group(0) == '{{' else '}' if m.group(0) == '}}' else '', text))
    for m in story_triggers.FILL.finditer(text):
        if m.group(1) is None:
            continue
        name = m.group(1)
        d = decls.get(name, {})
        n += story_triggers.FILL_WIDEST.get(d.get('type'), story_triggers.FILL_NAME) if 'mirror' not in d else 11
    return n


def check(program, voices=None, ids=None):
    """The problems of a program's words: [{path, at, text, blocks}] (blocks: approval refuses it). `voices`: {resident:
    dialect} ('plain' for none), from the world; `ids`: the world's resident and place ids, for rule 4."""
    import story_program

    out = []
    seen = set()

    def add(site, text, blocks):
        key = (site['path'], text)
        if key not in seen:
            seen.add(key)
            out.append({'path': site['path'], 'at': site['at'], 'text': text, 'blocks': blocks})

    if not isinstance(program, dict):
        return out
    decls = story_program.variables_of(program)
    grammar, player_facts = symbols()
    forbidden, phrases = words()['forbidden'], words()['phrases']
    for s in sites(program):
        text, where = s['text'], s['path']
        # 2. forbidden words, and "tell" for a private message.
        plain = _plain(text)
        for word in re.findall(r"[A-Za-z]+", plain):
            if word.lower() in forbidden:
                add(s, f'"{word}" is never said in this world: say {forbidden[word.lower()]} ({where}).', True)
        low = ' '.join(re.findall(r"[a-z']+", plain.lower()))
        for phrase, instead in phrases.items():
            if re.search(r'(?<![a-z])' + re.escape(phrase) + r'(?![a-z])', low):
                add(s, f'"{phrase}": say "{instead}" ({where}).', True)
        # 1. symbols, and brackets.
        if text.count('#') % 2:
            add(s, f'An unclosed # in {where}: write a symbol as #name#.', True)
        if '[' in text or ']' in text:
            add(s, f'A bracket in {where}: the talk expander reads [ ] as its own; leave them out.', True)
        for name in SYMBOL.findall(text):
            if not s['voiced']:
                add(s, f'#{name}# in {where}: only a resident\'s words in the window fill #symbols# (an offer, npc.line, npc.greet, '
                       'a reply); these are shown as written.', True)
                continue
            if not _symbol_known(name, grammar, player_facts):
                add(s, f'#{name}# in {where} is not in the talk library: a grammar symbol, or a fact such as #player.title#.', True)
            elif name.split('.')[0] not in grammar:
                # (A fact is known only for some wolves and residents, a title only where it is known: without it the
                # words are said with their symbols left out. A tree line gated on it says it only where it is known.)
                add(s, f'#{name}# in {where} reads a fact not every wolf has here: without it, it is left out of the words '
                       '(a line gated on it in the resident\'s tree says it only where it is known).', False)
        # 4. ids in speech (and in any word a wolf reads).
        for token in ID_WORD.findall(plain):
            if token.lower().startswith('wolf-') or '_' in token or re.fullmatch(r'[a-z]+[0-9]+[a-z0-9]*', token):
                add(s, f'"{token}" looks like an id in {where}: say a name (or fill one: {{name}}).', True)
        if ids:
            # (Only a word shaped like an id: a plain word that is also a place's id, "gate", "coldwater", is a word.)
            for token in re.findall(r"[a-z][a-z0-9_-]{2,}", plain):
                if token in ids and re.search(r'[0-9_-]', token):
                    add(s, f'"{token}" is an id of this world in {where}: say a name (or fill one: {{name}}).', True)
        # 5. lengths with every fill at its widest.
        if s['limit'] and _widest(text, decls) > s['limit']:
            add(s, f'{where} is too long with every fill at its widest: up to {s["limit"]} characters.', True)
        # 3. the giver's dialect.
        if voices and s['spoken'] and s['speaker'] in voices:
            own = voices[s['speaker']]
            import dialect_check
            for d, phrase in dialect_check.hits(text, lexicon()):
                if d != own:
                    add(s, f'{s["speaker"]} speaks {own if own != "plain" else "plain speech"}, but {where} says {d}\'s "{phrase}".', False)
        # 6. face banners and speech aloud.
        if s.get('effect') == 'story.tell' and isinstance(s.get('from'), str) and s['from']:
            if QUOTED.search(text):
                add(s, f'{where}: a resident\'s words to a wolf go in the dialogue window (an offer, npc.greet, npc.line, a scene), '
                       'not quoted in a banner with their face.', True)
            else:
                add(s, f'{where}: a banner with a resident\'s face is not the dialogue window: tell it plainly, or give the words '
                       'to the resident (npc.greet, npc.line).', False)
        if s.get('effect') == 'npc.speak' and YOU.search(plain):
            add(s, f'{where}: said aloud to a wolf ("you"): a resident\'s words to a wolf go in the dialogue window (npc.greet).', False)
        # 7. a clearing's words.
        if s.get('effect') == 'belief.plant' and s.get('claimKind') == 'story.cleared' and re.match(r'\s*(never|did not)\b', text, re.I):
            add(s, f'{where}: a clearing is said "<name> never <claim>": write the thing not done ("took the silver"), not "{text}".', False)
    return out


def _symbol_known(name, grammar, player_facts):
    parts = name.split('.')
    if parts[0] in grammar:
        return True   # (A grammar symbol, with or without a modifier: #greet#, #saying.capitalize#.)
    if len(parts) >= 2 and parts[0] in FACT_SECTIONS:
        if parts[0] in CHECKED_SECTIONS:
            rest = parts[1]
            return rest in player_facts or any(p.endswith('.*') and rest == p[:-2] for p in player_facts)
        return True
    return False


def world_voices(conn, world, program):
    """The dialects of the residents a program's words are said by ({id: dialect}), from live.npc_voice."""
    speakers = sorted({s['speaker'] for s in sites(program) if s['speaker']})
    if not speakers or conn.execute("SELECT to_regclass('live.npc_voice')").fetchone()[0] is None:
        return {}
    return {npc: dialect_of(region, accent) for npc, region, accent in conn.execute(
        'SELECT npc_id, home_region, accent FROM live.npc_voice WHERE world_id=%s AND npc_id=ANY(%s)', (world, speakers))}


def world_ids(conn, world, program):
    """The world's resident and place ids that a program's words hold as words (rule 4)."""
    tokens = sorted({t for s in sites(program) for t in re.findall(r"[a-z][a-z0-9_-]{2,}", _plain(s['text'])) if re.search(r'[0-9_-]', t)})
    if not tokens:
        return set()
    found = {r[0] for r in conn.execute('SELECT id FROM dm.story_residents WHERE world_id=%s AND id=ANY(%s)', (world, tokens))}
    found |= {r[0] for r in conn.execute('SELECT id FROM world.areas WHERE world_id=%s AND id=ANY(%s)', (world, tokens))}
    return found


def check_world(conn, world, program):
    return check(program, world_voices(conn, world, program), world_ids(conn, world, program))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('programs', nargs='*', help='program files (JSON)')
    parser.add_argument('--stories', default='', help='every *.json program under this directory')
    parser.add_argument('--world', default='', help='also read the world\'s voices and ids (read only)')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    files = [Path(p) for p in args.programs]
    if args.stories:
        files += sorted(Path(args.stories).rglob('*.json'))
    conn = world = None
    if args.world:
        import world_db
        conn = world_db.connect(world_db.resolve(args.world, 'dev'), 'editor')
        conn.execute('SET TRANSACTION READ ONLY')
        world = conn.execute('SELECT id FROM world.worlds LIMIT 1').fetchone()[0]
    report, failed = {}, False
    for f in files:
        try:
            program = json.loads(f.read_text())
        except (OSError, ValueError) as error:
            report[str(f)] = [{'path': '', 'text': f'not a program: {error}', 'blocks': True}]
            failed = True
            continue
        if not isinstance(program, dict) or program.get('format') != 'ratw-story-program':
            continue
        found = check_world(conn, world, program) if conn else check(program)
        report[str(f)] = found
        failed |= any(p['blocks'] for p in found)
    if conn:
        conn.rollback()
        conn.close()
    if args.json:
        print(json.dumps(report, indent=1, ensure_ascii=False))
    else:
        for f, found in report.items():
            for p in found:
                print(f'{"FAIL" if p["blocks"] else "warn"} {f}: {p["text"]}')
        print(f'{len(report)} programs; {sum(len(v) for v in report.values())} problems.')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    sys.exit(main())
