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
# Session 2's (§33.3): the other owed moves' words in every grammar, and plain's who-says-it for them.
DISCOURSE2 = ['funny', 'cheek', 'modest', 'secret', 'advise', 'helpout', 'wellsaid', 'poorshow', 'fine', 'planless',
              'pardon', 'tale']
PLAIN_ONLY2 = ['s8_self', 's8_laugh', 's8_teaseback', 's8_thanks', 's8_praise', 's8_modest', 's8_sure', 's8_fine',
               's8_story', 's8_memory', 's8_extend', 's8_welcome']
# Callbacks (shared8_callbacks.talk) are the one place an opener is the point (§33.1): a concept there reads the ledger.
LEDGER = re.compile(r'\bpair\.(talked|recall|last_talk|last_outcome|kept_invite|thread)\b')


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
            r = {'id': m.group(1), 'file': f.name, 'does': '', 'when': '', 'say': [], 'weight': 1.0}
            for line in block.splitlines():
                if line.startswith('does '):
                    r['does'] = line[5:].strip()
                elif line.startswith('when '):
                    r['when'] = (r['when'] + ' ' + line[5:].strip()).strip()
                elif line.startswith('weight '):
                    r['weight'] = float(line[7:])
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
            if 'concept=' in r['when']:
                self.assertEqual(r['file'], 'shared8_callbacks.talk', f"{r['id']}: a move line has no concept (§8.2)")
                self.assertRegex(r['when'], LEDGER, f"{r['id']}: a callback opener reads the ledger")
                self.assertRegex(r['when'], r'(^| )concept=talk\.[a-z_]+(\|talk\.[a-z_]+)*( |$)', r['id'])
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
        for g in ['plain'] + DIALECTS:
            have = {k.split('[')[0] for k in grammar(g)}
            for s in DISCOURSE2:
                self.assertIn(s, have, f'{g} has no {s}')
        for s in PLAIN_ONLY2:
            self.assertIn(s, plain, f'plain has no {s}')

    def test_every_pivot_is_a_step_the_graph_has(self):
        # A pivot line (§8.5) is made for an edge of topics.json, or a turn within one topic: conv.from and conv.topic.
        import json
        topics = json.loads((TALK / 'topics.json').read_text())['topics']
        for r in rules():
            if r['does'].split()[0] != 'pivot':
                continue
            src = re.search(r'conv\.from=([a-z_|]+)', r['when'])
            dst = re.search(r'conv\.topic=([a-z_|]+)', r['when'])
            self.assertTrue(src and dst, f"{r['id']}: a pivot names the step (conv.from, conv.topic)")
            for a in src.group(1).split('|'):
                self.assertIn(a, topics, r['id'])
                for b in dst.group(1).split('|'):
                    self.assertIn(b, topics, r['id'])
                    if len(src.group(1).split('|')) == 1 and len(dst.group(1).split('|')) == 1:
                        self.assertTrue(a == b or b in topics[a].get('next', {}), f"{r['id']}: {a} to {b} isn't an edge")

    def test_laughs_and_views_keep_their_place(self):
        # Never a laugh at what was said gravely (§8.11); an everyday view or pushback says nothing of one of the realm's
        # issues (the engine leaves it unsaid once one is aired: Phase 6's stance lines speak there); the dispreferred and
        # the seldom weigh little (§7.2), and what an extend is comes of the grammar, so it never outranks the leave.
        for r in rules():
            moves = r['does'].split()
            if moves[0] == 'laugh':
                self.assertRegex(r['when'], r'conv\.tone(!=serious\|grave\|tender|=light)', r['id'])
            if moves[0] in ('counter', 'give.opinion'):
                self.assertNotRegex(r['when'], r'conv\.(issue|agree)|self\.(stance|op)', r['id'])
            if moves[0] in ('take_offence', 'refuse', 'decline_apology', 'counter'):
                self.assertLessEqual(r['weight'], 0.5, r['id'])
            if moves[0] == 'extend':
                self.assertLessEqual(r['weight'], 0.35, r['id'])
                self.assertEqual(r['when'], '', f"{r['id']}: an extend's moment is the grammar's (s8_extend)")

    def test_answers_about_oneself_are_true_to_the_speaker(self):
        # How one is, by one's own facts (§33.3): "well" only where nothing ails them, grieving only in mourning.
        for r in rules():
            moves = r['does'].split()
            if moves[0] == 'answer.how.well':
                self.assertRegex(r['when'], r'self\.(mood|mood_why)=', r['id'])
            if moves[0] == 'answer.how.grieving':
                self.assertIn('self.mourning=', r['when'], r['id'])
            if moves[0] == 'answer.how.busy':
                self.assertIn('self.working=yes', r['when'], r['id'])

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
                if k.split('[')[0] in DISCOURSE + CLOSINGS + PLAIN_ONLY + DISCOURSE2 + PLAIN_ONLY2:
                    for a in alts:
                        self.assertNotRegex(a, r'\bdogs?\b', f'{g} {k}')


if __name__ == '__main__':
    unittest.main()
