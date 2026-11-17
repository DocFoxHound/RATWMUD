#!/usr/bin/env python3
"""Moves and tones on every entry of the talk library (Docs/Design/84 §7.1-7.2, §14.1, Phase 7).

Phase 2 tagged the 40 most-spoken families by hand and gave every answer concept a move (its alias, the commonest move
of its lines); the rest said nothing of what they do. This writes a `does` line on each entry that has none (a scene's
per turn, `/` between them), and a `tone` line where a turn's tone isn't its move's own (grave for a death, serious for
a crime). It reads what each line does from where it stands and what it says:

- an answer concept's rules make its alias first, always (a `then` that hands on to the concept owes that move, and C1
  judges the answer by it), and ask back as well when most of their words are a question;
- a topic's opener (rules and a scene's first turn) by its words: a question asks (how, after, plan, opinion, where,
  else a detail), news tells it, a grumble complains, a memory, a plan, a worry, a boast, a tease, a story, a view,
  else it tells of oneself; the topic leans it (gossip tells gossip, the lore tells a story);
- each later turn of a scene answers what the turn before made due (react.news to news: "I heard" known, "Never!" new,
  "Poor soul" sorrow; answer.detail to a question; agree, sympathise, laugh, tease.return...), chosen among the moves the
  one before expects as its words show, and asks back too when it ends on a question;
- barks (said to anyone in earshot) are asides; a shop's greeting greets; a player's greeting, thanks and goodbye are
  answered.

No word of the library changes (the equivalence test, tools/talk_equiv.py). The doc's "every family checked by hand" is
the review this writes (--review OUT.md: each family's moves and a line of each turn), read through and corrected in
FIX below where the reading was wrong.

  python3 tools/talk_tag.py                 report what would be tagged
  python3 tools/talk_tag.py --write         write the does/tone lines
  python3 tools/talk_tag.py --review OUT.md the families, their moves, a line of each turn
  python3 tools/talk_tag.py --check         exit 1 if an entry has no does line (ctest talk_tags)
"""
from __future__ import annotations

import argparse
import collections
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TALK = ROOT / 'Data' / 'Talk'
MOVES = json.loads((TALK / 'moves.json').read_text())['moves']
ALIAS = {k: v['alias'] for k, v in MOVES.items() if 'alias' in v}
REAL = {k: v for k, v in MOVES.items() if 'alias' not in v}


def base(move: str) -> str:
    """A variant's move (react.news.known -> react.news)."""
    return move if move in REAL else move.rsplit('.', 1)[0]


def expects(move: str) -> list[str]:
    return REAL.get(base(move), {}).get('expects', [])


def optional(move: str) -> bool:
    return bool(REAL.get(base(move), {}).get('optional'))


def default_tone(move: str) -> str:
    return REAL.get(base(move), {}).get('tone', 'even')


def kind_of(done: str, owed: str) -> bool:
    return done == owed or (done.startswith(owed + '.') and done not in REAL)


def meets(done: str, owed: str) -> bool:
    """A move done meets one owed: the same, a variant of it, or a fallback of it (down to ack), as the scorer's C1."""
    todo, seen = [owed], set()
    while todo:
        m = todo.pop(0)
        if m in seen:
            continue
        seen.add(m)
        if kind_of(done, m):
            return True
        todo += REAL.get(base(m), {}).get('fallback', [])
    return False


# ------------------------------------------------------------------------------------------------- reading the words
SYM = re.compile(r'#[^#\s]+#')


def plain(text: str) -> str:
    """The words with symbols as a stand-in word, for the cues (#bc_good# reads as its plain words)."""
    return SYM.sub(lambda m: BC_WORDS.get(m.group(0)[1:-1].split('.')[0], 'X'), text).strip()


def _bc_words() -> dict:
    out = {}
    g = TALK / 'grammar' / 'plain.grammar'
    for line in g.read_text().splitlines():
        m = re.match(r'^(bc_[a-z0-9_]+)\s*=\s*(.*)$', line.strip())
        if m:
            out[m.group(1)] = m.group(2).strip()
    out.update({'greet': 'Good day', 'farewell': 'Farewell', 'thanks': 'Thank you', 'agree': 'Aye', 'refuse': 'No',
                'surprise': 'You don\'t say', 'annoyed': 'Bother', 'oath': 'Gods', 'hedge': 'Well', 'pivot': 'Speaking of which.',
                'anyway': 'Anyway.', 'blessing': 'Bless you', 'bless': 'Gods keep you', 'robbery': 'Daylight robbery'})
    return out


BC_WORDS = _bc_words()

R = lambda p: re.compile(p, re.I)
Q_HOW = R(r"^(and )?(how (are|is it|goes|go|have|do you do|are you keeping|'s (things|life|the day|it going|you))|you keeping well|keeping well|are you well|all well)")
Q_AFTER = R(r"\bhow('s| is| are)( your| the)? (?!it\b|things\b|life\b|the day\b|you\b)\w+|\bis (your|the) \w+ (better|mended|well)|\bany (word|news) (of|from)\b")
Q_PLAN = R(r"^(and )?(will you|are you (going|off|coming|for)|what (are|will) you (doing|do)|where are you (off|going|headed)|going to\b|you're for\b|you'll be\b|when do you)")
Q_OPINION = R(r"\b(what do you (think|make|say)|do you think|don't you think|would you say|what's your view|is it right|isn't it (a )?(shame|sin|disgrace))\b")
Q_WHERE = R(r"^(and )?(where('s| is| are| did| do| was| were)|seen \w+ about)\b")
Q_NAME = R(r"\b(what('s| is) your name|who are you)\b")
Q_INVITE = R(r"^(will|would|won't) you (come|dance|walk|join|sit|eat|sup|share|stay)\b|^(come|walk) (with|and) (me|us)\b.*\?|^(fancy|care for) a\b")
Q_REQUEST = R(r"^((can|could|may) (i|we) (borrow|have|ask|use|take|come|sit|leave)|(can|could|will|would) you (lend|give|help|teach|show|bring|carry|take|watch|mind|fetch|spare|pass|hold|keep)|lend me|spare (me|a))\b")
TEASE = R(r"(#fool#|#insult#|#lump#|#daft#|\blook at you\b|\byou old\b|\bhave you (been|done)\b.*\?|\bwho (brushed|dressed)\b|\bstill (alive|with us)\b|\bcaught any\b)")
LAUGH = R(r"^(ha|hah|haha|heh|hee|ho ho|hoo)\b")
GREET = R(r"^(morning|evening|good (day|morning|evening|afternoon)|hello|hullo|well met|greetings|peace on you|hail)\b")
FAREWELL = R(r"^(farewell|goodbye|good night|night,|see you|be seeing you|take care|mind how you go|go well|i'll be off|off i go)\b")
THANKS = R(r"^(thank|thanks|much obliged|ta\b|bless you for)")
SORRY_FOR = R(r"^(poor (soul|thing|lamb|devil|creature|old)|i'm (so )?sorry|sorry to hear|that's (hard|a shame|awful|terrible|cruel)|what a shame|oh, no|shame\b|that's no life)")
APOLOGY = R(r"^(sorry|forgive me|i meant nothing|no offence|my mistake|i didn't mean)\b")
AGREE = R(r"^(aye|true|it is|that it is|so it is|right enough|quite|exactly|indeed|you're right|i know it|can't argue|same here|me too|nor me|so do i|so am i|they do|it does|always|always is|every (day|year|time|night|one)|they always do)\b")
DISAGREE = R(r"^(no,|nonsense|rubbish|not at all|that's not|i don't (think|agree|hold)|never!|codswallop|i'd not say|you're wrong)")
NEWS_NEW = R(r"^(never!|no!|really\?|you don't say|well,? i never|is that so\?|is that right\?|gods|bright one|what\?|stars above|truly\?|surely not)")
NEWS_KNOWN = R(r"^(i('d)? heard|so i heard|so they say|so they're saying|i know\b|everyone (knows|says)|it's all over|old news|heard it)")
ADVICE = R(r"\b(you (should|ought|want to|need to|must)|try (a|some|putting|rubbing)|best (to|thing)|i'd (put|take|go|see|ask|tell)|if i were you|get yourself|go and see|put (a|some) )\b")
WORRY = R(r"\b(i worry|i'm worried|i fear|i'm afraid|what if|keeps me (up|awake)|frets? me|can't sleep|scares me|nervous)\b")
COMPLAINT = R(r"\b(too dear|can't abide|sick of|fed up|bother|curse|blasted|wretched|robbery|daylight robbery|up again|gone up|dearer|no use|useless|ruined|leaks?|stinks?|blight|plague of|worse than ever|can't get|nobody (listens|cares)|enough to make)\b|^(ugh|bah|pah)\b")
MEMORY = R(r"\b(when i was (a pup|young|little|your age|a lad|a lass)|i remember|years ago|in my day|used to|as a pup|back then|my (gran|granny|grandad|grandmother|grandfather|old mother|father|mother) (used|always|said|would)|long ago)\b")
STORY = R(r"\b(did i ever tell you|there was (once|a)|they say (that )?(once|long ago)|the story goes|legend|the tale)\b")
PLAN = R(r"\b(i'm going to|i mean to|i'll be (going|off|taking)|tomorrow i|next (week|month) i|i'm off to|i'm thinking of|i've a mind to|one day i'll|i want to|i'd like to|i plan)\b")
BOAST = R(r"\b(best in|finest|nobody .{0,20} like (mine|me)|i'm (the best|proud)|proud of|look at (this|these|my)|my (new|best)|i won|i beat)\b")
FEELING = R(r"\b(i love|i miss|i'm (tired|happy|glad|weary|sad|lonely|worn)|makes me (happy|sad|smile)|i feel|my heart)\b")
OPINION = R(r"\b(i think|if you ask me|in my view|the trouble (with|is)|should be|ought to be|it's (wrong|right|a shame|a sin)|there's no (sense|need))\b")
NEWS = R(r"\b(did you hear|have you heard|heard about|they say|word is|news|i heard|guess what|you'll never guess|they're saying)\b")
GRAVE = R(r"\b(died|dead|death|dying|buried|burial|grave|funeral|mourn\w*|grie\w+|murder\w*|killed|drowned|widow\w*|orphan\w*)\b")
SERIOUS = R(r"\b(robbed|beaten|stabbed|thie\w+|stolen|wanted for|the watch (took|has|wants)|gaol|hanged|bandits?|raid\w*|warrant)\b")
TEASE_CONCEPTS = {'talk.friends'}

NEWS_TOPICS = {'talk.gossip': 'tell.gossip', 'talk.player': 'tell.gossip', 'talk.crime': 'tell.news', 'talk.news': 'tell.news',
               'talk.deed': 'tell.news', 'talk.event': 'tell.news', 'talk.life': 'tell.news', 'talk.bandits': 'tell.news',
               'talk.caravan': 'tell.news', 'talk.newcomer': 'tell.news', 'talk.ships': 'tell.news', 'talk.harvest': 'tell.news',
               'talk.festival': 'tell.news', 'talk.prices': 'tell.news', 'talk.price_cause': 'tell.news', 'talk.times': 'tell.news'}
TOPIC_LEAN = {'talk.lore': 'tell.story', 'talk.memories': 'tell.memory', 'talk.dreams': 'tell.plan', 'talk.quarrel': 'challenge',
              'talk.accent': 'ask.detail'}
NO_ALIAS = {'bark': 'aside', 'react.customer': 'greet', 'react.friend': 'hail', 'react.fight': 'aside', 'react.rain': 'aside',
            'react.struck': 'aside', 'react.theft': 'aside', 'player.greet': 'greet.return', 'player.bye': 'farewell.return',
            'player.thanks': 'deflect_thanks', 'player.day': 'answer.how', 'player.repeat': 'answer.detail',
            'player.trouble': 'answer.detail', 'player.trade': 'answer.detail', 'player.hours': 'answer.detail',
            'player.job': 'answer.detail', 'player.where': 'answer.whereabouts', 'player.wanted': 'answer.detail'}


def ends_q(t: str) -> bool:
    return t.rstrip(' "\'').endswith('?')


def question(t: str) -> str:
    """The ask a question is (its last sentence)."""
    last = re.split(r'(?<=[.!])\s+', t.strip())[-1]
    for pat, move in ((Q_NAME, 'ask.name'), (Q_INVITE, 'invite'), (Q_REQUEST, 'request'), (Q_HOW, 'ask.how'),
                      (Q_PLAN, 'ask.plan'), (Q_OPINION, 'ask.opinion'),
                      (Q_WHERE, 'ask.whereabouts'), (Q_AFTER, 'ask.after')):
        if pat.search(last):
            return move
    return 'ask.detail'


def opener(texts: list[str], concepts: set) -> str:
    """What a topic's first line does, by its words (the majority of its alternatives) and the topic."""
    votes = collections.Counter(opener_one(plain(t), concepts, t) for t in texts if t.strip())
    if not votes:
        return 'tell.self'
    top = votes.most_common()
    # A question beside statements: what most of them do; a tie goes to the statement (its answer is the safer owed).
    best = max(top, key=lambda kv: (kv[1], not kv[0].startswith('ask.')))
    return best[0]


def opener_one(t: str, concepts: set, raw: str = '') -> str:
    topic = next((c for c in sorted(concepts) if c.startswith('talk.')), '')
    news = topic in NEWS_TOPICS and ('#topic.' in raw or NEWS.search(t) or topic == 'talk.gossip')
    if GREET.search(t):
        return 'greet'
    asked = [x for x in sentences(t) if ends_q(x)]
    if asked:
        if NEWS.search(asked[0]) and news:
            return NEWS_TOPICS[topic]
        return question(asked[-1])
    if LAUGH.search(t):
        return 'tell.story' if topic == 'talk.lore' else 'tell.self'
    if topic in TEASE_CONCEPTS and TEASE.search(raw or t):
        return 'tease'
    if news:
        return NEWS_TOPICS[topic]
    if WORRY.search(t):
        return 'tell.worry'
    if STORY.search(t):
        return 'tell.story'
    if MEMORY.search(t):
        return 'tell.memory'
    if PLAN.search(t):
        return 'tell.plan'
    if COMPLAINT.search(t):
        return 'tell.complaint'
    if BOAST.search(t):
        return 'tell.boast'
    if OPINION.search(t):
        return 'give.opinion'
    if FEELING.search(t):
        return 'tell.feeling'
    return TOPIC_LEAN.get(topic, 'tell.self')


def answer_one(t: str, owed: list[str], before: str) -> list[str]:
    """What a later turn does, answering what the turn before made due (owed: its expects; before: its last move)."""
    t = plain(t)
    q = ends_q(t)
    first = None
    b = base(before)
    if b in ('ask.how',):
        first = 'answer.how'
    elif b == 'ask.after':
        first = 'answer.after'
    elif b == 'ask.plan':
        first = 'answer.plan'
    elif b == 'ask.whereabouts':
        first = 'answer.whereabouts'
    elif b == 'ask.opinion':
        first = 'give.opinion'
    elif b == 'ask.name':
        first = 'introduce.return'
    elif b in ('ask.detail', 'ask.advice'):
        first = 'give.advice' if b == 'ask.advice' else 'answer.detail'
    elif b in ('tell.news', 'tell.gossip'):
        lone_q = q and len(sentences(t)) == 1
        first = ('react.news.known' if NEWS_KNOWN.search(t) else 'react.news.new' if NEWS_NEW.search(t) else
                 'react.news.sorrow' if SORRY_FOR.search(t) else 'ask.detail' if lone_q else
                 'give.advice' if ADVICE.search(t) else 'give.opinion' if OPINION.search(t) else 'react.news')
    elif b == 'tell.complaint':
        first = ('sympathise' if SORRY_FOR.search(t) else 'react.complaint.agree' if AGREE.search(t) else
                 'react.complaint.disagree' if DISAGREE.search(t) else 'give.advice' if ADVICE.search(t) else
                 'counter' if q and DISAGREE.search(t) else 'react.complaint')
    elif b == 'tell.worry':
        first = 'sympathise' if SORRY_FOR.search(t) else 'give.advice' if ADVICE.search(t) else 'ask.detail' if q else 'reassure'
    elif b == 'tell.feeling':
        first = 'sympathise' if SORRY_FOR.search(t) else 'react.self'
    elif b in ('tell.boast',):
        first = 'tease' if LAUGH.search(t) else 'react.self'
    elif b in ('tell.plan',):
        first = 'ask.detail' if q else 'react.self'
    elif b in ('tell.story', 'tell.memory'):
        first = 'laugh' if LAUGH.search(t) and b == 'tell.story' else 'react.story'
    elif b in ('tease', 'join.tease'):
        first = 'tease.laugh' if LAUGH.search(t) else 'tease.return'
    elif b == 'give.opinion':
        lone_q = q and len(sentences(t)) == 1
        first = ('disagree' if DISAGREE.search(t) else 'agree' if AGREE.search(t) else 'ask.detail' if lone_q else
                 'give.opinion')
    elif b == 'challenge':
        first = 'concede' if APOLOGY.search(t) else 'counter'
    elif b in ('greet', 'hail', 'again.open', 'kin.open'):
        first = 'greet.return' if GREET.search(t) else None
    elif b == 'tell.self':
        first = ('laugh' if LAUGH.search(t) else 'sympathise' if SORRY_FOR.search(t) else 'ask.detail' if q else
                 'react.self')
    elif b in ('offer',):
        first = 'accept'
    elif b in ('request',):
        first = 'decline' if DISAGREE.search(t) or re.match(r"(?i)^(no|not|i can't|can't|sorry)\b", t) else 'grant'
    elif b in ('invite',):
        first = 'decline' if re.match(r"(?i)^(no|not|i can't|can't|sorry)\b", t) else 'accept_invite'
    elif b in ('warn',):
        first = 'thank' if THANKS.search(t) else 'ack'
    elif b in ('praise',):
        first = 'deflect_praise'
    elif b in ('apologise',):
        first = 'accept_apology'
    elif b in ('farewell', 'preclose'):
        first = 'farewell.return' if b == 'farewell' else 'accept_preclose'
    if first is None:
        # Nothing was owed (a reaction, an answer): this turn says something of its own, as an opener would.
        if FAREWELL.search(t):
            return ['farewell']
        if THANKS.search(t):
            return ['thank']
        if LAUGH.search(t):
            return ['laugh'] if b in ('tell.self', 'tell.story', 'tell.boast', 'tease') else ['tell.self']
        if q:
            return [question(t)]
        if ADVICE.search(t):
            return ['give.advice']
        if WORRY.search(t):
            return ['tell.worry']
        if COMPLAINT.search(t):
            return ['tell.complaint']
        if MEMORY.search(t):
            return ['tell.memory']
        if b.startswith('answer.') or b in ('react.news', 'react.self', 'react.story', 'react.complaint', 'sympathise',
                                            'reassure', 'assess', 'agree', 'give.advice', 'counter', 'tease.return',
                                            'tease.laugh', 'laugh', 'disagree', 'answer.detail'):
            return ['assess'] if AGREE.search(t) or len(t.split()) <= 4 else ['tell.self']
        return ['tell.self']
    out = [first]
    # Asks back too: a turn that answers and then asks ("Never! Who told you?").
    if q and not first.startswith('ask.') and first not in ('counter',):
        ask = question(t)
        if ask != first:
            out.append(ask)
    return out


def sentences(t: str) -> list[str]:
    return [x for x in re.split(r'(?<=[.!?])\s+', t.strip()) if x]


def answer(texts: list[str], before: str) -> list[str]:
    votes = collections.Counter(tuple(answer_one(t, expects(before), before)) for t in texts if t.strip())
    if not votes:
        return ['ack']
    return list(max(votes.most_common(), key=lambda kv: (kv[1], len(kv[0]) == 1))[0])


def tone(texts: list[str]) -> str:
    """A turn's tone where its words make it plain: grave (a death) or serious (a crime), by most of its alternatives."""
    # (A place's name is no death: the Drowned Lantern is an inn.)
    said = [re.sub(r"\b(?:Drowned|Dead|Widow's|Gallows) [A-Z]\w+", 'X', plain(t)) for t in texts if t.strip()]
    if not said:
        return ''
    if sum(1 for t in said if GRAVE.search(t)) * 2 >= len(said):
        return 'grave'
    if sum(1 for t in said if GRAVE.search(t) or SERIOUS.search(t)) * 2 >= len(said):
        return 'serious'
    return ''


# --------------------------------------------------------------------------------------------------- the files
HEAD = re.compile(r'^\s*([ab])(\?)?((?:@[a-z0-9_]+)*)(\[[^\]]*\])?((?:\s+#[a-z0-9_]+)*)\s*:(.*)$')


class Entry:
    def __init__(self, file: Path, start: int, kind: str, id: str):
        self.file, self.start, self.kind, self.id = file, start, kind, id
        self.concepts, self.does, self.tone, self.then = set(), None, None, None
        self.turns = []          # [(who, [texts])]
        self.when_at = start     # the line after which to write
        self.has_moves = False

    @property
    def scene(self) -> bool:
        return self.kind == 'scene'

    @property
    def family(self) -> str:
        return self.id.rsplit('_', 1)[0]


def read(root: Path = TALK) -> list[Entry]:
    out = []
    for f in sorted(root.rglob('*.talk')):
        lines = f.read_text().split('\n')
        e = None
        last_variant_tagged = False
        last_who, last_opt = None, None
        for i, raw in enumerate(lines):
            line = raw.strip()
            if line.startswith(('rule ', 'scene ')):
                e = Entry(f, i, line.split()[0], line.split()[1])
                out.append(e)
                last_variant_tagged, last_who, last_opt = False, None, None
                continue
            if e is None or not line or line.startswith('#'):
                continue
            if line.startswith('when '):
                e.when_at = i
                for tok in line[5:].split():
                    if tok.startswith('concept='):
                        e.concepts |= set(tok[8:].split('|'))
                    elif tok.startswith('topic='):
                        e.concepts |= {'talk.' + v for v in tok[6:].split('|')}
            elif line.startswith('does ') or line == 'does':
                e.does = line[5:].strip()
            elif line.startswith('tone '):
                e.tone = line[5:].strip()
            elif line.startswith('then '):
                e.then = line.split(':', 1)[1].strip()
            elif line.startswith('say ') and e.kind == 'rule':
                alts = [re.sub(r'^\{[^}]*\}\s*', '', a.strip()) for a in line[4:].split('|')]
                if not e.turns:
                    e.turns.append(('a', []))
                e.turns[0][1].extend(alts)
            elif e.kind == 'scene':
                m = HEAD.match(raw)
                if not m:
                    continue
                who, opt, needs, tags = m.group(1), bool(m.group(2)), m.group(3), m.group(4)
                alts = [re.sub(r'^\{[^}]*\}\s*', '', a.strip()) for a in m.group(6).split('|')]
                joins = bool(e.turns) and last_who == who and last_variant_tagged and last_opt == opt
                if joins:
                    e.turns[-1][1].extend(alts)
                else:
                    e.turns.append((who, list(alts)))
                last_who, last_opt = who, opt
                last_variant_tagged = bool(tags) or bool(needs)
    return out


def tag(e: Entry) -> tuple[list[list[str]], list[str]]:
    """(moves per turn, tones per turn: '' for the move's own)."""
    moves, tones = [], []
    alias = next((ALIAS[c] for c in sorted(e.concepts) if c in ALIAS), None)
    plainalias = next((NO_ALIAS[c] for c in sorted(e.concepts) if c in NO_ALIAS), None)
    for k, (who, texts) in enumerate(e.turns):
        if k == 0:
            if not e.scene and alias:
                ms = [alias]
                qs = sum(1 for t in texts if ends_q(plain(t)))
                if qs * 2 > len(texts) and not alias.startswith('ask.') and alias not in ('farewell', 'farewell.return'):
                    ms.append(question(plain(next(t for t in texts if ends_q(plain(t))))))
            elif plainalias and not (e.scene and plainalias == 'aside' and len(e.turns) > 1):
                ms = [plainalias]
            else:
                ms = [opener(texts, e.concepts)]
        else:
            ms = answer(texts, moves[-1][-1])
        if e.scene and k > 0 and who == e.turns[k - 1][0]:
            # The same speaker again (a turn the other let pass): carrying on, owed nothing new.
            ms = [m for m in ms if m.startswith(('tell.', 'ask.', 'give.')) or m in ('assess', 'laugh')] or ['tell.self']
        moves.append(ms)
        t = tone(texts)
        light_after_grave = any(tones[j] == 'grave' for j in range(max(0, k - 2), k)) and default_tone(ms[0]) == 'light'
        if t == default_tone(ms[0]):
            t = ''
        if light_after_grave and not t:
            t = 'even'
        tones.append(t)
    return moves, tones


FIX: dict[str, str] = {}   # entry id -> its does line, where the reading above was wrong (the review's corrections)


def write(entries: list[Entry]) -> collections.Counter:
    by_file = collections.defaultdict(list)
    stats = collections.Counter()
    for e in entries:
        if e.does is not None or not e.turns:
            continue
        moves, tones = tag(e)
        does = FIX.get(e.id) or ' / '.join(' '.join(m) for m in moves)
        lines = [f'does {does}']
        if any(tones) and e.tone is None:
            lines.append('tone ' + ' / '.join(t or default_tone(m[0]) for t, m in zip(tones, moves))
                         if e.scene else f'tone {tones[0] or default_tone(moves[0][0])}')
        by_file[e.file].append((e.when_at, lines))
        stats['scenes' if e.scene else 'rules'] += 1
        stats['with tone'] += bool(any(tones))
    for f, inserts in by_file.items():
        text = f.read_text().split('\n')
        for at, lines in sorted(inserts, reverse=True):
            text[at + 1:at + 1] = lines
        f.write_text('\n'.join(text))
    return stats


def review(entries: list[Entry], out: Path):
    fam = collections.defaultdict(list)
    for e in entries:
        if e.does is None and e.turns:
            fam[(str(e.file.relative_to(TALK)), e.family)].append(e)
    rows = ['# Phase 7 tagging review (tools/talk_tag.py --review)', '']
    for (f, family), es in sorted(fam.items()):
        rows.append(f'## {f} {family} ({len(es)})')
        for e in es[:4]:
            moves, tones = tag(e)
            rows.append(f'- {e.id} [{",".join(sorted(e.concepts))}]' + (f' then {e.then}' if e.then else ''))
            for (who, texts), m, t in zip(e.turns, moves, tones):
                rows.append(f'    {who}: {" ".join(m)}{" (" + t + ")" if t else ""}  | {texts[0][:90]}')
        rows.append('')
    out.write_text('\n'.join(rows))


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--write', action='store_true')
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--review', type=Path)
    ap.add_argument('--root', type=Path, default=TALK)
    a = ap.parse_args(argv)
    entries = read(a.root)
    todo = [e for e in entries if e.does is None and e.turns]
    if a.check:
        if todo:
            print(f'talk_tag: {len(todo)} entries say nothing of what they do (first {todo[0].id}): run tools/talk_tag.py --write')
            return 1
        print(f'talk_tag: all {len(entries)} entries say what they do')
        return 0
    if a.review:
        review(entries, a.review)
        print(f'review written: {a.review}')
        return 0
    counts = collections.Counter()
    for e in todo:
        moves, _ = tag(e)
        for m in moves:
            counts[' '.join(m)] += 1
    print(f'{len(todo)} of {len(entries)} entries to tag; turns by moves:')
    for m, n in counts.most_common(40):
        print(f'  {n:5}  {m}')
    if a.write:
        print(dict(write(entries)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
