"""Phase 8's shared lines (Docs/Design/84 §33): the rules each session writes keep the doc's rules for them (ctest
talk_p8_rules). The library itself (it loads, its coverage, its truth, its dialects) is checked by talk_check,
talk_coverage, talk_lint and dialect_check; these are the promises the shared lines make that those don't see."""
from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TALK = ROOT / 'Data' / 'Talk'
DIALECTS = ['coldwater', 'frosthollow', 'larkfield', 'mirebridge', 'oakhaven', 'saltwell', 'southmeadow']
# The discourse words Phase 8 added (§33.1), in every grammar; the greetings and closings, plain's by who says them and
# each dialect's own.
DISCOURSE = ['marvel', 'pity', 'gladnews', 'uneasy', 'grumble', 'notsobad', 'dunno', 'hearsay', 'howso', 'rest']
CLOSINGS = ['s8_hello', 's8_okay', 's8_bye', 's8_byeback', 's8_nevermind']
# Plain's own, by who says them (no dialect needs them: they're said by the plain words' speakers, with dialect inside).
PLAIN_ONLY = ['s8_wow', 's8_gripe', 's8_evade', 's8_ask', 's8_pupbye', 's8_pupbyeback', 's8_teenbye']


def grammar(name: str) -> dict[str, list[str]]:
    """{symbol (criteria folded in): [alternatives]} of one grammar file."""
    out: dict[str, list[str]] = {}
    for line in (TALK / 'grammar' / f'{name}.grammar').read_text().splitlines():
        m = re.match(r'^([a-z0-9_]+(?:\.broad)?)!{0,2}(\[[^\]]*\])?\s*\+?=\s*(.*)$', line.strip())
        if m:
            out.setdefault(m.group(1) + (m.group(2) or ''), []).extend(a.strip() for a in m.group(3).split('|'))
    return out


def rules() -> list[dict]:
    """Every rule of rules/shared8_*.talk: its id, file, does, when and alternatives."""
    out = []
    for f in sorted((TALK / 'rules').glob('shared8_*.talk')):
        for block in re.split(r'\n(?=rule )', f.read_text()):
            m = re.match(r'rule (\S+)', block)
            if not m:
                continue
            r = {'id': m.group(1), 'file': f.name, 'does': '', 'when': '', 'say': []}
            for line in block.splitlines():
                if line.startswith('does '):
                    r['does'] = line[5:].strip()
                elif line.startswith('when '):
                    r['when'] = line[5:].strip()
                elif line.startswith('say '):
                    r['say'] += [a.strip() for a in line[4:].split('|')]
            out.append(r)
    return out


def words(text: str) -> int:
    return len(re.findall(r"[A-Za-z']+", re.sub(r'#[^#\s]*#', ' ', text)))


class SharedLines(unittest.TestCase):
    def test_every_rule_is_a_move_line_that_says_what_it_does(self):
        for r in rules():
            self.assertTrue(r['does'], f"{r['id']}: no does")
            self.assertNotIn('concept=', r['when'], f"{r['id']}: a move line has no concept (§8.2)")
            self.assertNotRegex(r['when'], r'(^| )topic=', f"{r['id']}: conv.topic, not topic= (a concept)")
            self.assertTrue(r['say'], f"{r['id']}: no words")

    def test_every_symbol_they_say_is_defined_in_plain(self):
        plain = {k.split('[')[0] for k in grammar('plain')}
        for r in rules():
            for a in r['say']:
                for s, rest in re.findall(r'#([a-z0-9_]+)((?:\.[a-z_]+)*)#', a):
                    if s in ('topic', 'self', 'other', 'here', 'world', 'conv', 'pair', 'x') and rest:
                        continue                    # (A fact, not a symbol.)
                    self.assertIn(s, plain, f"{r['id']}: #{s}# isn't a plain symbol")

    def test_the_new_discourse_words_are_in_every_grammar(self):
        for g in ['plain'] + DIALECTS:
            have = {k.split('[')[0] for k in grammar(g)}
            for s in DISCOURSE + CLOSINGS:
                self.assertIn(s, have, f'{g} has no {s}')
        plain = {k.split('[')[0] for k in grammar('plain')}
        for s in PLAIN_ONLY:
            self.assertIn(s, plain, f'plain has no {s}')

    def test_closings_are_short(self):
        # Said in nearly every talk: under six words, never "distinctive" (R4, R5), in every voice.
        for g in ['plain'] + DIALECTS:
            for k, alts in grammar(g).items():
                if k.split('[')[0] in CLOSINGS + ['s8_pupbye', 's8_pupbyeback', 's8_teenbye']:
                    for a in alts:
                        self.assertLess(words(a), 6, f'{g} {k}: "{a}"')
        for r in rules():
            if r['file'] == 'shared8_turns.talk':
                for a in r['say']:
                    self.assertLess(words(a), 6, f"{r['id']}: \"{a}\"")

    def test_feeling_for_the_one_talked_of_is_gated_to_the_claim(self):
        # Pity only for bad news, gladness only for good (truth.json's pity and glad rules, talk_lint's gossip_kind).
        feeling = re.compile(r'#(pity|rest|gladnews)#|\b(poor soul|sorry to hear|good for #|congratulat)', re.I)
        gates = re.compile(r'\b(topic\.claim_kind|topic\.kind|topic\.dir|world\.harvest|world\.times)=')
        for r in rules():
            if any(feeling.search(a) for a in r['say']):
                self.assertRegex(r['when'], gates, f"{r['id']}: feels for the one talked of with no claim kind")

    def test_news_to_one_who_had_not_heard_never_says_they_had(self):
        heard = re.compile(r"\bI('d| had)? heard\b(?! it twice)|\bold news\b|#knew#", re.I)
        for r in rules():
            if r['does'].split()[0] in ('react.news.new', 'react.news.sorrow', 'react.news.glad', 'react.news.doubt'):
                for a in r['say']:
                    self.assertNotRegex(a, heard, f"{r['id']}: \"{a}\"")

    def test_the_sorrowful_and_glad_are_for_one_who_had_not_heard(self):
        # A line that says it's news to them (sorrow, gladness, glee) holds only for one who didn't know (§8.6), or on a
        # topic with no claim (no `knows`).
        for r in rules():
            if r['does'].split()[0] in ('react.news.sorrow', 'react.news.glee') and 'claim_kind' in r['when']:
                self.assertIn('self.knows!=yes', r['when'], f"{r['id']}")

    def test_a_named_one_is_never_they(self):
        # Never "they" for one known wolf (§11): after #topic.subject# or #topic.victim#, their pronoun is a symbol.
        bad = re.compile(r"#topic\.(subject|victim)#[^#|]*\b(they(?! say| were saying| said|'re saying)|them|their)\b", re.I)
        for r in rules():
            for a in r['say']:
                self.assertNotRegex(a, bad, f"{r['id']}: \"{a}\"")

    def test_house_rules_of_the_world(self):
        # Never "dog" (wolves; pups for the young); "private messages", never "tells".
        for r in rules():
            for a in r['say']:
                self.assertNotRegex(a, r'\bdogs?\b', r['id'])
                self.assertNotRegex(a, r'\btells\b', r['id'])
        for g in ['plain'] + DIALECTS:
            for k, alts in grammar(g).items():
                if k.split('[')[0] in DISCOURSE + CLOSINGS + PLAIN_ONLY:
                    for a in alts:
                        self.assertNotRegex(a, r'\bdogs?\b', f'{g} {k}')


if __name__ == '__main__':
    unittest.main()
