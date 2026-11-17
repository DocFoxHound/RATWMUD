#!/usr/bin/env python3
"""The equivalence test for Phase 7's conversion (Docs/Design/84 §18, P7: "the same possible lines, old against new").

Phase 7 changes how the library is written, not what it can say: short markers became conversation tags
(tools/talk_conv.py), stock short lines became backchannel symbols whose plain value is the old words
(tools/talk_migrate.py), and every entry gained a `does` line and, where it isn't its move's, a `tone` (talk_tag.py).
This reads the library at a git revision (the phase before) and the working tree, entry by entry, and checks:

- every entry is still there, with the same criteria (a converted marker's read `pair.mem.k` is now `conv.k`), the
  same hand-on, limits, priority and weight, and the same remembering less the converted markers;
- every line a plain speaker could say before, they can say still: each alternative of each line, its symbols
  backchannels and discourse words expanded to plain's values, is one the old library had, and each old one is still
  there; a converted marker's tag leads every alternative of its rule;
- an alternative that is new says a discourse word (§10.3: a vent line led in by `#vent#`), and an entry that is new
  is listed; a line changed by hand is named in EDITED below with why.

  python3 tools/talk_equiv.py [--against REV]   (default: origin/r3-talk-p6) exit 1 on any difference
"""
from __future__ import annotations

import argparse
import itertools
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TALK = ROOT / 'Data' / 'Talk'
DEFAULT = 'origin/r3-talk-p6'
# Lines changed by hand in Phase 7, each with why (the only differences allowed beyond the conversion).
EDITED = {
    'e1_food_rm_fog_chowder': 'said "in this rain" in fog (its criteria allow rain, fog or storm): "and no wonder" now',
    'ag_again_any': '"Back again?" once a day (cooldown 1d): every opener now says what it does, so the again_today frame '
                    'put it before 83% of a busy place\'s talks',
    'ag_again_any2': 'the same',
    'w7s_shore_work': '"My nets are tangled past saving." from a weaver at the shore (talk_lint\'s fisher_self, frozen since '
                      'Phase 1): "Look at those nets. Tangled past saving." now, true for anyone there',
    # Phase 8, session 1 (doc 84 §33.2): the town's gossip answers, met by the card answering a death.
    'n7_ga2_merry': 'laughed at a death ("Ha! I\'ll never look at Gunnar Harrowgate the same.", C4): not for a death, a '
                    'hurt or a victim now (topic.claim_kind!=death|violence|victim)',
    'n7_ga2_dry': 'the same ("And the moon\'s made of cheese" to a death)',
    'n7_ga2_sly': 'the same ("Interesting. Who else knows?" to a death)',
    'n7_ga2_chatty': 'the same ("I knew it! Didn\'t I say?" to a death)',
    'n7_ga2_young': 'the same ("I\'m telling everyone!" to a death)',
    'n7_ga2_old': 'the same ("It\'ll be forgotten by the next festival" to a death)',
    'n7_ga2_nervy': 'the same ("If they hear we said it..." to a death)',
    'n7_ga2_cold': '"Doesn\'t surprise me." to a death: not for a death now (n7_ga2_death_cold is the cold wolf\'s)',
    'n7_ga2_devout': '"The gods hear tattle too" to a death: not for a death now (n7_ga2_death_devout is the devout\'s)',
    'n7_ga2_priest': '"I\'ll not hear it" to a death: the same',
    'n7_ga2_warm': '"I hope they\'re all right" of one wolf: #topic.they# (he or she) now',
    'n7_ga2_death': '"I saw them only the other day": "they" for one wolf, and an eyewitness who saw nothing; "No. Not '
                    '#topic.them#. Surely not." now',
    'n7_ga2_death_devout': '"May they find their rest" and "keep them" of one wolf: #topic.they#, #topic.them# now',
    'n7_ga2_death_cold': '"I hardly knew them" of one wolf: #topic.them# now',
    'e5_been_old_broad': '"For a funeral. Didn\'t care for it." was laughed at by tale.react\'s merry answer (C4, the hard gate, '
                         'frozen L10 at Phase 8 session 2, reached by a new pivot): "For a wedding." now, the dry joke kept',
}
# Slice 2's ticking listen (doc 84 §34): eyewitness words from one who saw nothing (T3), said true for anyone now.
EDITED['cr2_theft_suspect'] = '"I saw someone near X\'s door that night." from anyone (T3, unbacked): "Someone was seen near ' \
                              'X\'s door that night." now, and its answer "Nobody got a good look."'
EDITED['et_arrest_relief'] = '"I saw them marched off." from anyone (T3, unbacked): "Marched off by the watch, I heard." now; ' \
                             'and the guard\'s "We have them now" of the one arrested: #topic.them#'
# Slice 2's review (doc 84 §34): the talk of a traveller near (topic=player, its stranger.* answers) and of a traveller's
# deed said "them", "they" and "their" of the one wolf, male or female; #topic.them#, #topic.they#, #topic.their# now,
# the verb agreeing ("Ask them." is "Ask him.", "They look tired" is "She looks tired"). 215 alternatives; plural ones
# (travellers, the door watch, boots) and the town's "they say" kept (talk_lint's they_stranger keeps it so).
for _entry in (
    'dd_heard_look', 'dd_heard_named', 'dd_look_curious', 'dd_named_merchant', 'ev_pk_back', 'ev_pk_child_admires',
    'ev_pk_child_follows', 'ev_pk_debt', 'ev_pk_friendly_wave', 'ev_pk_gossip_about', 'ev_pk_guard',
    'ev_pk_long_absence', 'ev_pk_old_approves', 'ev_pk_rivals', 'ev_pk_tavern', 'ev_pk_tavern_story', 'ev_pk_wary',
    'ev_pk_well_thought', 'ev_pl_carter_road_news', 'ev_pl_child_staring', 'ev_pl_farmer_squint',
    'ev_pl_festival_stranger', 'ev_pl_fortress', 'ev_pl_guard_notes', 'ev_pl_handsome', 'ev_pl_hungry_look',
    'ev_pl_innkeeper_sizes', 'ev_pl_merchant_appraises', 'ev_pl_new_face', 'ev_pl_newcomer_hope',
    'ev_pl_noble_dismiss', 'ev_pl_priest_welcome', 'ev_pl_rain_stranger', 'ev_pl_ridgemere_judge', 'ev_pl_road_dust',
    'ev_pl_rude', 'ev_pl_scholar_curious', 'ev_pl_ser_ferro_judge', 'ev_pl_strong', 'ev_pl_suspicious',
    'ev_pl_suspicious_night', 'ev_pl_two_kids', 'h7_dd_ans_any', 'h7_dd_ans_bandits', 'h7_dd_ans_devout',
    'h7_dd_ans_doubt', 'h7_dd_ans_guard', 'h7_dd_ans_rich', 'h7_dd_ans_warm', 'h7_dd_ans_young',
    'h7_dd_open_nickname', 'h7_deed_bandits_near', 'h7_deed_devout', 'h7_deed_guard', 'h7_deed_nickname',
    'h7_deed_poor', 'h7_deed_rich_hire', 'h7_deed_what_for', 'h7_deed_who_now', 'h7_deed_young', 'h7_known_fond',
    'h7_known_help', 'h7_known_sus', 'h7_st_ans_ambitious', 'h7_st_ans_any', 'h7_st_ans_cold', 'h7_st_ans_devout',
    'h7_st_ans_guard', 'h7_st_ans_merchant', 'h7_st_ans_nervy', 'h7_st_ans_rich', 'h7_st_ans_sly', 'h7_st_ans_warm',
    'h7_st_ans_young', 'h7_st_close_admire', 'h7_st_close_doubt', 'h7_st_close_doubt_frank', 'h7_st_close_duty',
    'h7_st_close_fear', 'h7_st_close_hire', 'h7_st_close_kind', 'h7_st_close_pray', 'h7_st_close_road',
    'h7_st_close_shrug', 'h7_st_close_trade', 'h7_st_close_young', 'h7_stranger_armed', 'h7_stranger_bandits_hire',
    'h7_stranger_devout', 'h7_stranger_fear', 'h7_stranger_guard_eye', 'h7_stranger_hire_road',
    'h7_stranger_kind_word', 'h7_stranger_merchant_coin', 'h7_stranger_night', 'h7_stranger_poor_envy',
    'h7_stranger_restless', 'h7_stranger_rich_bodyguard', 'h7_stranger_robbed_ask', 'h7_stranger_rumours',
    'h7_stranger_tavern_drink', 'h7_stranger_thief_eye', 'h7_stranger_tired', 'h7_stranger_young_hero',
    'pl2_known_friendly', 'pl2_known_wary', 'pl2_stranger_child', 'pl_known', 'pl_stranger', 'pl_stranger_whisper',
):
    EDITED[_entry] = 'a traveller near is one wolf: the "them", "they" and "their" said of the traveller are #topic.them#, ' \
                     '#topic.they#, #topic.their# (he or she) now, the verb agreeing'
# Slice 2's review (doc 84 §35): lines changed by hand, each with why.
EDITED.update({
    'e4_fa_priest_named': '"What did you tell them?" of the one priest named: #a.priest_them# (him or her) now',
    'et_rm_arrest_houses': '"Greatly\'s man" of a woman, and "they\'ll be out by noon" of one wolf: "One of Greatly\'s", '
                           '#topic.they# now; "They\'re always owned." is "That sort always is."',
    'et_appr_news': '"Good for them" of one apprentice: #topic.them# now',
    'et_death_spouse': '"I\'ll take them supper" of the widowed one, named: "I\'ll take #topic.where# some supper"; '
                       '"Bright One keep them" of the one dead: #topic.them#',
    'h7_cr_ans_cold': '"Should\'ve locked their door" of one victim: #topic.victim_their#',
    'h7_arrest_visit': '"Their mother", "Bring them a comb" of the one arrested: #topic.their#, #topic.them#',
    'cv_greet_host': 'a host\'s two greetings were 13% of a room\'s lines (R6 by sentence): six more ways to greet a guest',
    'ag_cb_prices': '"Dearer than when we spoke of it last." whichever way prices went, and "I said it would go this way" '
                    'naming nothing: by the price\'s way now (ag_cb_prices_up, _down), the good named',
    'r7_fr_childhood_name': '"Morning, Puddles." at any hour: "Well, if it isn\'t Puddles."',
    'qk_sparrows_bark': '"Morning, you." at any hour: "Back again, you."',
    'cv_pre_work': '"This won\'t earn my keep." from pupils at school: not for a child now (cv_pre_school is theirs)',
    'cv_bye_night': '"Off to bed with me." from one who stays (an innkeeper at work): s8t_bye_leave_night\'s now, said '
                    'only by one free to go, who leaves the room\'s talk for an hour',
    'cv_bye_both2': '"Off I go, then." the same: cv_bye_both_leave\'s now',
    'gs_secret': 'says the claim ("Don\'t tell a soul, but X stole...") and passes it on now (asserts)',
    'et_gs_apprentice': 'the same ("I heard X stole from Y, master.")',
    'm7_home_re_any': '"Ridgemere. I\'ll die talking like it." from one standing in Ridgemere, then told "Long way from home": '
                      'only from one not at home (self.home_here), m7_home_re_local_any else',
    'm7_close_home': '"Long way from home, then." to one born where they stand: not to one at home (other.home_here)',
    'm7_close_home_cold': 'the same',
})
DISCOURSE = {'hedge', 'account', 'ack', 'pivot', 'anyway', 'later', 'resume', 'preclose', 'excuse', 'knew', 'sympathy',
             'concede', 'insist', 'warn', 'welcome_back', 'welcome', 'sorry', 'forgive', 'deflect', 'no_matter',
             'go_ahead', 'put_off', 'vent', 'reassure', 'congrats', 'demur', 'doubt', 'listen'}
HEAD = re.compile(r'^(\s*[ab]\??(?:@[a-z0-9_]+)*(?:\[[^\]]*\])?(?:\s+#[a-z0-9_]+)*\s*):(.*)$')


def git_files(rev: str) -> dict[str, str]:
    names = subprocess.run(['git', '-C', str(ROOT), 'ls-tree', '-r', '--name-only', rev, 'Data/Talk'], capture_output=True,
                           text=True, check=True).stdout.split()
    out = {}
    for n in names:
        if n.endswith(('.talk', '.grammar')):
            out[n[len('Data/Talk/'):]] = subprocess.run(['git', '-C', str(ROOT), 'show', f'{rev}:{n}'], capture_output=True,
                                                        text=True, check=True).stdout
    return out


def tree_files() -> dict[str, str]:
    return {str(f.relative_to(TALK)): f.read_text() for f in sorted(TALK.rglob('*')) if f.suffix in ('.talk', '.grammar')}


def plain_symbols(files: dict[str, str]) -> dict[str, list[str]]:
    out = {}
    for line in files.get('grammar/plain.grammar', '').splitlines():
        m = re.match(r'^([a-z0-9_]+)\s*=\s*(.*)$', line.strip())
        if m:
            out.setdefault(m.group(1), []).extend(a.strip() for a in m.group(2).split('|') if a.strip())
    return out


def entries(files: dict[str, str]) -> dict[str, dict]:
    out = {}
    for name, text in files.items():
        if not name.endswith('.talk'):
            continue
        e = None
        for raw in text.split('\n'):
            line = raw.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith(('rule ', 'scene ')):
                e = {'file': name, 'kind': line.split()[0], 'keys': [], 'lines': [], 'remember': []}
                out[line.split()[1]] = e
                continue
            if e is None:
                continue
            if line.startswith(('does ', 'tone ')) or line in ('does', 'tone'):
                continue
            if line.startswith('remember '):
                e['remember'].append(line)
            elif line.startswith('say ') or line == 'say':
                e['lines'].append(('say', [a.strip() for a in line[3:].split('|')]))
            else:
                m = HEAD.match(line)
                if m and e['kind'] == 'scene':
                    e['lines'].append((re.sub(r'\s+', ' ', m.group(1).strip()), [a.strip() for a in m.group(2).split('|')]))
                else:
                    e['keys'].append(line)
    return out


def converted_keys(old: dict, new: dict) -> set[str]:
    keys = set()
    for id, o in old.items():
        n = new.get(id)
        if not n:
            continue
        for r in set(o['remember']) - set(n['remember']):
            m = re.match(r'^remember (pair|other): ([a-z0-9_]+)', r)
            if m:
                keys.add(m.group(2))
    return keys


def norm_key(line: str, keys: set[str]) -> str:
    return re.sub(r'(?<![a-z0-9_.])(?:pair|self|other)\.mem\.(' + '|'.join(map(re.escape, sorted(keys))) + r')(?![a-z0-9_])',
                  r'conv.\1', line) if keys else line


def strip_tag(alt: str) -> tuple[str, str]:
    m = re.match(r'^\{([^}]*)\}\s*(.*)$', alt)
    return (m.group(1), m.group(2)) if m else ('', alt)


def expansions(text: str, plain: dict[str, list[str]], limit: int = 400) -> set[str]:
    """The text with each symbol Phase 7 added (a backchannel, a discourse word: `plain` holds only those) replaced by
    each of plain's values; the rest left as written."""
    parts = re.split(r'(#[a-z0-9_]+(?:\.[a-z]+)?#)', text)
    choices = []
    for p in parts:
        m = re.match(r'^#([a-z0-9_]+)(\.[a-z]+)?#$', p)
        if m and m.group(1) in plain:
            vals = plain[m.group(1)]
            if m.group(2) == '.capitalize':
                vals = [v[:1].upper() + v[1:] for v in vals]
            choices.append(vals)
        else:
            choices.append([p])
    out = set()
    for combo in itertools.product(*choices):
        out.add(''.join(combo))
        if len(out) >= limit:
            break
    return out


def compare(rev: str) -> list[str]:
    old_files, new_files = git_files(rev), tree_files()
    old, new = entries(old_files), entries(new_files)
    before = plain_symbols(old_files)
    plain = {k: v for k, v in plain_symbols(new_files).items() if k not in before}    # (Phase 7's own symbols.)
    keys = converted_keys(old, new)
    problems = []
    added = sorted(set(new) - set(old))
    for id in sorted(set(old) - set(new)):
        problems.append(f'{id}: gone')
    for id in sorted(set(old) & set(new)):
        o, n = old[id], new[id]
        where = f'{n["file"]}: {id}'
        if id not in EDITED and [norm_key(k, keys) for k in o['keys']] != [norm_key(k, keys) for k in n['keys']]:
            problems.append(f'{where}: its criteria or keywords changed beyond the conversion')
        gone = [r for r in o['remember'] if r not in n['remember']]
        tags = set()
        for r in gone:
            m = re.match(r'^remember (pair|other): ([a-z0-9_]+)(?:=(\S+))?\s+(\d+[mh])$', r)
            if not m or (m.group(4).endswith('h') and int(m.group(4)[:-1]) > 1):
                problems.append(f'{where}: "{r}" is gone but wasn\'t a short marker')
            else:
                tags.add(f'{m.group(2)}={m.group(3) or "yes"}')
        if [r for r in n['remember'] if r not in o['remember']]:
            problems.append(f'{where}: remembers something new')
        if len(o['lines']) != len(n['lines']):
            problems.append(f'{where}: {len(o["lines"])} lines before, {len(n["lines"])} now')
            continue
        for (oh, oalts), (nh, nalts) in zip(o['lines'], n['lines']):
            if norm_key(oh, keys) != norm_key(nh, keys):
                problems.append(f'{where}: a line\'s speaker or tags changed ({oh} / {nh})')
                continue
            olds = [strip_tag(a)[1] for a in oalts]
            possible = set()
            for a in nalts:
                tag, words = strip_tag(a)
                have = set(tag.split())
                if tags - have:
                    problems.append(f'{where}: "{words[:50]}" lacks the tags {sorted(tags - have)}')
                exp = expansions(words, plain)
                possible |= exp
                if not (exp & set(olds)) and id not in EDITED and not re.search(r'#(' + '|'.join(DISCOURSE) + r')(\.[a-z]+)?#', words):
                    problems.append(f'{where}: "{words[:60]}" is a new line with no discourse word')
            for a in olds:
                if a not in possible and id not in EDITED:
                    problems.append(f'{where}: "{a[:60]}" can no longer be said')
    for id in added:
        print(f'talk_equiv: new entry {new[id]["file"]}: {id}')
    return problems


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--against', default=DEFAULT)
    a = ap.parse_args(argv)
    try:
        problems = compare(a.against)
    except subprocess.CalledProcessError as e:
        print(f'talk_equiv: no revision {a.against} to compare with ({e.stderr.strip()[:200]})')
        return 1
    for p in problems:
        print('talk_equiv:', p)
    if not problems:
        print(f'talk_equiv: every line said before {a.against} can be said now, word for word for a plain speaker; '
              f'nothing else changed but tags, moves and tones ({len(EDITED)} entries changed by hand, each said why)')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
