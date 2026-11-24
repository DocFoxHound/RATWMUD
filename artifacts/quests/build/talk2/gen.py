#!/usr/bin/env python3
"""The Ninth Howl's talk library, part 2: gossip and helpful knowledge (doc 86 §6.4-6.6; cast-and-talk.md §4).

  python3 artifacts/quests/build/talk2/extract_packs.py   (the packs' lines from the design files, once)
  python3 artifacts/quests/build/talk2/build_packs.py     (their fixes and additions: packs.json)
  python3 artifacts/quests/build/talk2/gen.py Data/Talk

writes rules/story_ninth_howl.talk and scenes/common/story_ninth_howl.talk, and inserts the window lines into
dialogue/resident.tree (hub, news, gossip, and part 1's nh_story, nh_help, nh_who, nh_ask), dialogue/inn.tree (greet)
and dialogue/watch.tree (hub), each between ">>> Ninth Howl talk, part 2" markers, replaced on a re-run. The hub lines
are the quest packs' targets' and givers' own greetings (packs.json nodes 'hub' and 'watch.hub', priority 4 like part
1's cast greetings), after part 1's and before the hub's choices. Re-run it after anything rewrites those trees.
The campaign's lines are campaign.dsl (its head says how to read it); the packs' are packs.json.
"""
import json, re, sys
from pathlib import Path

HERE = Path(__file__).parent
ALL = 'king|moot_called|reckoning|signals|howls|letters|lights|harvest|bell|moot'
TOWN_OF = {'plain': 'ridgemere', 'coldwater': 'coldwater', 'larkfield': 'larkfield', 'southmeadow': 'southmeadow',
           'mirebridge': 'fenshore', 'saltwell': 'saltwell', 'oakhaven': 'oakhaven', 'frosthollow': 'frosthollow'}
DIALECTS = list(TOWN_OF)
# A speaker's dialect is the speech of where they grew up (self.from; settings.json dialect_of: Fenshore speaks
# Mirebridge's). The window and the rules gate on self.from, which dialect_check reads; self.dialect would say the same.
FROM = {'plain': 'ridgemere', 'coldwater': 'coldwater', 'larkfield': 'larkfield', 'southmeadow': 'southmeadow',
        'mirebridge': 'mirebridge|fenshore', 'saltwell': 'saltwell', 'oakhaven': 'oakhaven', 'frosthollow': 'frosthollow'}


def speech(gates):
    return [('self.from=' + FROM[g.split('=', 1)[1]]) if g.startswith('self.dialect=') else g for g in gates]
BEGIN = '# >>> Ninth Howl talk, part 2 (gossip and helpful knowledge; generated, doc 86 §6.4): {what}'
END = '# <<< Ninth Howl talk, part 2: {what}'


def parse_dsl(path):
    out, stage, dialect = [], None, None
    for n, raw in enumerate(open(path), 1):
        line = raw.rstrip('\n')
        if not line.strip() or line.startswith('#'):
            continue
        if line.startswith('= '):
            stage = line[2:].strip()
            continue
        if line.startswith('- '):
            dialect = line[2:].strip()
            continue
        head, text = line.split(' :: ', 1)
        toks = head.split()
        kind, toks = toks[0], toks[1:]
        gates, town, st = [], None, stage
        for t in toks:
            if t.startswith('@'):
                town = t[1:]
            elif t.startswith('stage='):
                st = t[6:]
            elif t.startswith('weather='):
                gates.append('here.weather=' + t[8:])
            elif t.startswith('time='):
                gates.append('here.time=' + t[5:])
            elif t.startswith('fig='):
                gates.append('world.figure_' + t[4:])
            elif t.startswith('quarter='):
                gates.append('self.quarter=' + t[8:])
            elif '.' in t.split('=')[0]:
                gates.append(t)
            else:
                gates.append('world.mem.' + t)
        out.append(dict(kind=kind, stage=st, dialect=dialect, town=town, gates=gates, text=text.strip(), line=n))
    return out


def stage_gate(st):
    return [] if st in ('none', 'after') else ['world.mem.nh_stage=' + st]


def crit(r, with_dialect=True, extra=()):
    g = stage_gate(r['stage'])
    if with_dialect and r['kind'] != 'X':
        g.append('self.dialect=' + r['dialect'])
    if r['town']:
        g.append('here.region=' + r['town'])
    g += r['gates']
    g += list(extra)
    return g


def say(gates, text, priority=None):
    g = speech(gates) + ([f'priority={priority}'] if priority else [])
    return f'say[{" ".join(g)}] {text}' if g else f'say {text}'


def check_len(text, where):
    for alt in text.split(' | '):
        if len(alt) > 160:
            raise SystemExit(f'{where}: over 160 characters: {alt}')


def campaign_window(lines):
    """Lines per node of resident.tree."""
    nodes = {k: [] for k in ('news', 'gossip', 'nh_story', 'nh_help', 'nh_who', 'nh_ask')}
    for r in lines:
        check_len(r['text'], f"campaign.dsl:{r['line']}")
        k, st = r['kind'], r['stage']
        if any(g.startswith('world.figure_') for g in r['gates']):
            continue                                # (A figure line: rules only; the window's render check has none.)
        if k == 'X':
            nodes['nh_story'].append(say(crit(r, False), r['text'], 1))
            continue
        if st in ('none', 'after'):
            # Before the campaign (the grace) and after it (the epilogue): nh_story isn't open, so news and gossip.
            nodes['gossip'].append(say(crit(r), r['text']))
            nodes['news'].append(say(crit(r), r['text']))
            continue
        if k in ('R', 'V', '!'):
            nodes['gossip'].append(say(crit(r), r['text']))
        if k == 'R':
            nodes['nh_story'].append(say(crit(r), r['text'], 2))
        if k in ('V', '!'):
            nodes['nh_who'].append(say(crit(r), r['text'], 2))
        if k == 'H':
            nodes['news'].append(say(crit(r), r['text']))
            nodes['nh_story'].append(say(crit(r), r['text'], 2))
        if k == 'W':
            nodes['nh_help'].append(say(crit(r), r['text'], 2))
        if k == 'A':
            nodes['nh_ask'].append(say(crit(r), r['text'], 2))
    return nodes


def rules(lines):
    """NPC-to-NPC: rumours, villain hints and warnings as talk rules (cast-and-talk §5.1)."""
    out, seen = [], {}
    concept = {'R': ('talk.news|talk.gossip', 'tell.news'), 'V': ('talk.gossip', 'tell.gossip'),
               '!': ('talk.news|talk.bandits', 'tell.news')}
    for r in lines:
        if r['kind'] not in concept:
            continue
        c, does = concept[r['kind']]
        st = {'none': 'grace', 'after': 'after'}.get(r['stage'], r['stage'].split('|')[0])
        key = f"nh_{st}_{r['dialect']}"
        seen[key] = seen.get(key, 0) + 1
        out.append(f"rule {key}_{seen[key]}\nwhen concept={c} {' '.join(speech(crit(r)))}\ndoes {does}\ncooldown 1d\n"
                   f"say {r['text']}\n")
    return out


def pack_window(packs):
    nodes = {'news': [], 'gossip': [], 'inn.greet': [], 'hub': [], 'watch.hub': []}
    for r in packs:
        check_len(r['line'], f"{r['src']} {r['quest']}")
        node = r['node']
        pr = 2 if node == 'news' else (1 if node == 'inn.greet' and not any(g.startswith('self.id') for g in r['gates'])
                                       else (2 if node == 'inn.greet' else None))
        if node == 'news' and any('nh_coin=thin' in g for g in r['gates']):
            pr = 3
        if node in ('hub', 'watch.hub'):
            pr = 4                                  # (A resident's own greeting, as part 1's cast greetings.)
        nodes[node].append((r['quest'], say(r['gates'], r['line'], pr)))
    return nodes


SCENES = '''# The Ninth Howl's NPC-to-NPC scenes, one per stage (cast-and-talk.md §4.13; doc 86 §6.4). Gated on the campaign's
# talk memory (C8 talk.remember writes it; the DM seeds it by hand until then), so the test town never plays them and
# the talk scorecards don't move. No line dates the Moot (decisions §6.4): "the Moot night", and "Midwinter" only under
# nh_moot=midwinter. No names: roles only.

scene nh_s1_king
when topic=news world.mem.nh_stage=king|moot_called
does tell.news / react.news / assess
a : The old King's gone up, and a patrolman keeps the Palace lamp. | They say the wolf with the Palace lamp still walks the Sump round.
b[from=mirebridge|fenshore] : A fen wolf, I hear. Talks like the old ones on the boardwalk, no? | Fen-sure, the drowned get a king.
b[quarter=sump] : One of ours, then. About time. | He's walked my alley of an evening for years.
b : A patrolman? The Houses won't like that. | Strange times.
a : Times change. | Bright One keep him, whoever he is.

scene nh_s2_debts
when topic=gossip world.mem.nh_stage=reckoning
does tell.news / react.news / assess
a : The Counting House chalked my neighbour's door. | They want the debtors' howls by the new moon.
b[quarter=sump] : They chalked mine. I scrubbed it. They chalked it again. | A howl for a note. I'd sooner starve.
b[from=saltwell] : A loan the borrower cannot refuse is a leash. | The well remembers who drew from it.
b : A wolf of the road is working off notes, I hear. | Then there's hope.
a : Hard days. | I'll scrub till the moon's up.

scene nh_s3_lists
when topic=gossip world.mem.nh_lists=gathering
does tell.news / react.news / assess
a : There's a wolf going round asking who keeps a list of who pays the Watch. | Somebody's putting the Watch lists together.
b[quarter=sump] : About time. The Sump's paid for the Watch twice, once in coin and once in looking away. | Let them. We've nothing to hide down here but hunger.
b : Careful. Lists like that get wolves hurt. | I'd keep my head down, if I were asked.
a : Maybe so. | Bright One keep whoever's carrying it.

scene nh_s4_howls
when topic=day world.mem.nh_stage=howls
does tell.news / react.news / assess
a : Did you hear the chorus on the square? | Somebody's teaching the old howl again.
b[from=coldwater] : One for the drowned, one for a catch and one for supper, eh? I'd not swear which. | Like a rising fish, it went.
b[from=larkfield] : Up at the end like a price. That's ours. | Full weight, that howl.
b : I heard. I'd forgotten it went like that. | The old folk used to sing that one.
a : Next time, I'm joining in. | I'll hum it till I've got it.

scene nh_s5_letters
when topic=gossip world.mem.nh_stage=letters
does tell.news / react.news / assess
a : Somebody's carrying the Grayrock lord's letters out to his road. All forty. | I gave mine up at last. A wolf of the road took it.
b[quarter=wharf|sump] : Forty letters. Not one answered in six years. | He'll read them or he won't.
b[from=oakhaven] : And Fell's stakes in our cut, mm? Somebody walk those bounds. | Mark every ninth cedar, mm?
b : The lord's coming home, they say. | Home? That I'll believe when I see it.
a : We'll see. | The Bright One keep the letters dry.

scene nh_s6_lights
when topic=news world.mem.nh_lights=walking
does tell.news / react.news / assess
a : Lights in the fens, every night now. | They say the drowned are walking.
b[from=mirebridge|fenshore] : Only the cold ones are the drowned. The warm ones are lamps, no? | Dark One's mire, they want something said.
b[from=saltwell] : A light that smells of lamp oil is no ghost, #address#. | A mirage of the fen.
b : Lamps, more like. Somebody's moving goods. | I'd not follow one.
a : Maybe so. | Then let somebody say the names.

scene nh_s7_ford
when topic=bandits world.mem.nh_ford=siege
does tell.news / react.news / assess
a : The Captain's whole company is at the Larkfield ford. | Three nights, they say, before the harvest's safe.
b[from=larkfield] : Every sack in Longmead's on the walls. Scales and silver! | Grain for a rampart.
b[from=southmeadow] : If the runner comes, we go. Sheaf and sickle. | Round by the vine lanes.
b : Then I hope enough wolves go. | Bright One keep the ford.
a : Let's hope so. | I'm taking bread down tomorrow.

scene nh_s8_bell
when topic=lore world.mem.nh_stage=bell
does tell.news / react.news / assess
a : They say a bell rings under the Upper Mere ice. | The Chancellor wept over a paper, and half the city saw it.
b[from=coldwater] : Lie flat on the ice and you'll hear it, eh? | Bright One's tide, it's ringing.
b[from=mirebridge|fenshore] : Our bell. Mud and eels, I'd walk the Long River for it. | It's ours, under there.
b : A bell and a writ in one season. | Strange times, these.
a : Maybe so. | The Moot's coming either way.

scene nh_s9_moot
when topic=day world.mem.nh_stage=moot
does tell.news / react.news / assess
a : Which post are you standing on the Moot night? | Every post on the Line wants a keeper.
b[from=frosthollow] : The Pass. Lamp lit. | Frosthollow starts it. Cold iron.
b[from=saltwell] : The pool, #address#. Saltwell starts the south road. | By the well, with a still heart.
b : The square, if my legs hold. | I'll listen for the Bay Light bell.
a : Then howl loud. | I'll listen for yours.

scene nh_s9_moot_winter
when topic=day world.mem.nh_stage=moot world.mem.nh_moot=midwinter
does tell.news / react.news / assess
a : A Midwinter Moot. Which post are you standing? | Every post on the Line wants a keeper, and a coat.
b[from=coldwater] : The mere post. A howl carries over ice, eh? | The skipper's, with a hot stone in my pocket.
b : The square, if my paws don't freeze to it. | I'll listen for the Bay Light bell.
a : Then howl loud, and stamp your feet. | I'll listen for yours.

scene nh_s10_after
when topic=day world.mem.nh_howl=ran
does tell.news / react.news / assess
a : Did you hear it come in off the Bay Light? Like a wave up the hill. | I counted a hundred breaths and there it was, the far howl.
b[from=coldwater] : Glasswater answered, eh? I'd swear I heard the mere in it. | Calm water, and a howl over it.
b : I heard. I've never heard the like. | Every town on one howl. Who'd have thought it.
a : A night to remember. | The Bright One heard it too, I'd say.
'''

RULES_HEAD = '''# The Ninth Howl: rumours, villain hints and warnings between residents (cast-and-talk.md §4, §5.1; doc 86 §6.4-6.6).
# Generated from the same source as the window's gossip lines in dialogue/resident.tree (talk library part 2). Every
# rule is gated on the campaign's talk memory (world.mem.nh_*: C8 talk.remember, or the DM by hand until C8 lands) and
# the speaker's own dialect, so the test town never says them and the scorecards don't move. Rumours ride talk.news and
# talk.gossip, villain hints talk.gossip, warnings talk.news and talk.bandits. No names (roles, or a figure once C9 or
# the DM's save_figure sets it); no line dates the Moot (decisions §6.4).

'''

NODE_NOTE = {
    'nh_story': '# By stage and dialect: rumours and thanks to the heroes (cast-and-talk §4; R, H). The recap is above.',
    'nh_help': '# By stage and dialect: where to lend a paw (cast-and-talk §4; W), each town its own (@town lines).',
    'nh_who': '# By stage and dialect: who is behind it, and warnings (cast-and-talk §4; V, !).',
    'nh_ask': '# By stage and dialect: who to ask (cast-and-talk §4; A), each town its own (@town lines).',
}

NODES_HEAD = '''# The Ninth Howl's window (cast-and-talk.md §1, §4, §5.2; doc 86 §6.2, §6.4). The campaign's news opens from `news`:
# nh_story (the recap and the stage's talk, by dialect), nh_help (where to lend a paw), nh_who (who's behind it, and
# warnings), nh_ask (who to ask). Every line is gated on world.mem.nh_stage (C8, or the DM by hand) and the speaker's
# dialect; @town lines on here.region (innkeepers among them: they give helpful knowledge and recaps only). The cast
# slot, the pledge and the backing (part 1, cast-and-talk §3, §3.20) are further choices of nh_story; it shows eight at
# most (help, who, ask, the cast slot, pledge, back, and the two ways out).
'''


def block(what, body):
    return [BEGIN.format(what=what)] + body + [END.format(what=what)]


def strip_block(text, what):
    b, e = BEGIN.format(what=what), END.format(what=what)
    return re.sub(re.escape(b) + r'.*?' + re.escape(e) + r'\n', '', text, flags=re.S)


def insert_after(text, anchor, lines):
    i = text.index(anchor) + len(anchor)
    return text[:i] + '\n'.join(lines) + '\n' + text[i:]


def insert_in_hub(text, lines, tree):
    """Before the hub's `mood` and `free` (its choices follow), so after every greeting part 1 wrote."""
    m = re.search(r'^node hub\n(?:(?!node ).*\n)*?free\n', text, flags=re.M)
    if not m:
        raise SystemExit(f'{tree}: node hub has no free line')
    i = m.end() - len('free\n')
    while text.rfind('\n', 0, i - 1) >= 0 and text[text.rfind('\n', 0, i - 1) + 1:i].startswith('mood '):
        i = text.rfind('\n', 0, i - 1) + 1         # (and before the hub's mood, which closes its lines)
    return text[:i] + '\n'.join(lines) + '\n' + text[i:]


HUB_NOTE = ['# The quest packs: their targets\' and givers\' own greetings, gated on the line\'s talk key (quests-large',
            '# notes, "Talk lines for wave G"; C8, or the DM by hand), priority 4 like the cast\'s above.']


def main(talk):
    talk = Path(talk)
    camp = parse_dsl(HERE / 'campaign.dsl')
    packs = json.load(open(HERE / 'packs.json'))
    cw = campaign_window(camp)
    pw = pack_window(packs)

    # resident.tree
    p = talk / 'dialogue' / 'resident.tree'
    t = p.read_text()
    for what in ('news lines', 'news choice', 'gossip lines', 'campaign nodes'):
        t = strip_block(t, what)
    news = ['# The campaign: thanks to the heroes and, after the Moot, how it ended (cast-and-talk §4).'] + cw['news']
    news += ['# The quest packs: helpful knowledge (priority 2) and the thin purse (priority 3) (quests-*.md, talk).']
    news += [s for _, s in pw['news']]
    t = insert_after(t, 'node news\nsay #news_none#\n', block('news lines', news))
    gossip = ['# The campaign: rumours, villain hints and warnings, by stage and dialect; the grace; the epilogue. No',
              '# priority, so a fresh real claim (here.gossip=yes) still competes (cast-and-talk §5.1).'] + cw['gossip']
    gossip += ['# The quest packs: "someone did it" lines (quests-*.md, talk).'] + [s for _, s in pw['gossip']]
    t = insert_after(t, 'node gossip\nsay #gossip_none#\n', block('gossip lines', gossip))
    t = strip_block(t, 'hub lines')
    t = insert_in_hub(t, block('hub lines', HUB_NOTE + [s for _, s in pw['hub']]), 'resident.tree')
    # The campaign's own nodes are part 1's (cast-and-talk §3, §5.2): its nh_story, nh_help, nh_who and nh_ask hold the
    # recap and the plain floor. This fills them by stage and dialect, after each node's bare say.
    for node in ('nh_story', 'nh_help', 'nh_who', 'nh_ask'):
        t = strip_block(t, node + ' lines')
        m = re.search(r'^node ' + node + r'\n(?:#.*\n)*say [^\n]*\n', t, flags=re.M)
        if not m:
            raise SystemExit(f'resident.tree has no node {node} (part 1 writes it)')
        lines = [l for l in cw[node] if not l.startswith('say[world.mem.nh_stage=') or 'priority=1]' not in l]
        t = t[:m.end()] + '\n'.join(block(node + ' lines', [NODE_NOTE[node]] + lines)) + '\n' + t[m.end():]
    p.write_text(t)

    # inn.tree: the campaign's recaps (priority 1) and the large lines' and medium innkeepers' lines.
    p = talk / 'dialogue' / 'inn.tree'
    t = strip_block(p.read_text(), 'inn recaps')
    recaps = ['# The campaign: a recap for a late joiner, one per stage (cast-and-talk §4.2).']
    recaps += [say(crit(r, False), r['text'], 1) for r in camp if r['kind'] == 'X']
    recaps += ['# The quest packs: recaps and innkeepers\' own lines (quests-medium.md, quests-large.md).']
    recaps += [s for _, s in pw['inn.greet']]
    anchor = 'say[pair.disposition=hostile priority=10] You. Say what you want and go. | What now? | I\'ll serve you, and that\'s all.\n'
    t = insert_after(t, anchor, block('inn recaps', recaps))
    p.write_text(t)

    # watch.tree: the watch's own targets' greetings (its hub).
    p = talk / 'dialogue' / 'watch.tree'
    t = strip_block(p.read_text(), 'hub lines')
    t = insert_in_hub(t, block('hub lines', HUB_NOTE + [s for _, s in pw['watch.hub']]), 'watch.tree')
    p.write_text(t)

    (talk / 'rules' / 'story_ninth_howl.talk').write_text(RULES_HEAD + '\n'.join(rules(camp)))
    (talk / 'scenes' / 'common' / 'story_ninth_howl.talk').write_text(SCENES)

    # Counts per stage and dialect (the design's six-a-stage, and each town's own helpful line).
    report = {}
    for r in camp:
        if r['kind'] == 'X':
            continue
        for st in (r['stage'].split('|') if r['stage'] not in ('none', 'after') else [r['stage']]):
            k = (st, r['dialect'])
            d = report.setdefault(k, {'all': 0, 'default': 0, 'town': 0})
            d['all'] += 1
            if not r['gates']:
                d['default'] += 1
            if r['town'] and r['kind'] in 'WA':
                d['town'] += 1
    bad = [(k, v) for k, v in report.items() if k[0] not in ('none', 'after') and (v['default'] < 6 or v['town'] < 1)]
    print('campaign lines:', len(camp), ' pack lines:', len(packs))
    print('per node:', {k: len(v) for k, v in cw.items()}, {k: len(v) for k, v in pw.items()})
    print('stage x dialect short of six defaults or a town line:', bad or 'none')


if __name__ == '__main__':
    main(sys.argv[1])
