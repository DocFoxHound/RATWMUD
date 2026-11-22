"""Quests and a living audience (Docs/Design/85-story-triggers-and-campaign-control.md, 3.6 and Phase 6). Pure: no
database. The runner (story_runner.py) and rehearsals (story_program.py) both play a story's quests through here.

A version 2 story may declare `quests`. A quest has objectives (required, optional or hidden, in any order with `after`,
counted with a goal and a measure), fail rules (events or a deadline), rewards (story effects given to each wolf with
credit: coins from a named purse, items, standing, deeds) and a scope: each wolf's own copy (`player`), one for the party
the wolf was in when it started (`party`), or one for the whole story (`story`). An audience may be `open` (nobody at
first: wolves come in by taking a quest or by `participant.add`) or `everyone` (the active wolves, and each wolf who signs
in). The effects marked "story" in the catalogue are kept here, never sent to the game; what they change reaches the
game as two orders the runner sends (`story.cohort`, `story.quests`).

The state it keeps (inside the run's state):
    quests       {quest: {holder: copy}}   holder: a wolf (player scope), "party:<party>", or "story"
    questOffers  {quest: [wolves]}         offered to them by quest.offer, to accept or decline
    questStarts  {quest: n}                copies started, for a quest's `limit`
    parties      {wolf: party}             as the game tells it (a wolf in none is a party of one)
    offers       {choice: {...}}           npc.offer: story choices laid over residents' windows, this visit
    toasts       {wolf: text}              words for a wolf the runner passes on (a quest that could not be taken)
"""

import bisect
import json

import story_program as rules
import story_triggers as triggers

SCOPES = ('player', 'party', 'story')
LIFETIMES = ('campaign', 'phase')
REPEATS = {'once': None, 'daily': 1, 'weekly': 7, 'always': 0}
STATUSES = ('active', 'ready', 'done', 'failed', 'abandoned', 'expired')
UNDER_WAY = ('active', 'ready')
ENDED = ('done', 'failed', 'abandoned', 'expired')
QUESTS_MAX = 32
OBJECTIVES_MAX = 12
REWARDS_MAX, BONUS_MAX, FAIL_MAX = 8, 4, 4
AUDIENCE_CAP = 5000          # Wolves of one run at most (doc 85, 3.6: the cap becomes 5,000 once per-wolf state is kept apart).
JOINABLE = ('open', 'everyone')

QUEST_EFFECTS = ('quest.offer', 'quest.start', 'quest.complete', 'quest.fail', 'objective.complete', 'objective.fail',
                 'objective.reveal', 'objective.progress')
PARTICIPANT_EFFECTS = ('participant.add', 'participant.remove')
STORY_EFFECTS = (*QUEST_EFFECTS, *PARTICIPANT_EFFECTS, 'npc.offer')
# (A wolf may be named by $actor in these from a trigger of anyone in the world: each takes the wolf into the story.)
TAKES_IN = ('participant.add', 'quest.offer', 'quest.start')
GAME_EVENTS = ('quest accepted', 'quest declined', 'quest handed in', 'quest abandoned', 'dialogue choice')
PARTY_EVENTS = ('party joined', 'party left')
STORY_FACTS = ('quest started', 'quest completed', 'quest failed', 'objective completed')
LEAVES = 'storyline abandoned'     # (A wolf leaving the world story from the journal: an open audience lets them go.)
PURSE_PREFIXES = ('town:', 'till:', 'chapter:')
TOWN_PURSES = ('works', 'watch', 'church')

DEFAULT_WORDS = {'accept': "I'll do it.", 'decline': 'Not now.'}


def ask_words(q):
    return q.get('offer', {}).get('ask') or f'Ask about: {q["title"]}'


def hand_in_words(q):
    return q.get('offer', {}).get('handIn') or f'About {q["title"]}: it\'s done.'


# ------------------------------------------------------------------------------------------------------- the checks

def joinable(p):
    return p.get('audience', {}).get('kind') in JOINABLE


FIXED_CAP = 500             # (A fixed or active audience, as before Phase 6.)


def cap_of(p):
    return p.get('audience', {}).get('cap', AUDIENCE_CAP) if joinable(p) else FIXED_CAP


def check_purse(v):
    """A purse: who pays a coin reward (the user's answer to question 4: never minted)."""
    if not rules.identifier(v) or not v.startswith(PURSE_PREFIXES):
        return False
    parts = v.split(':')
    if parts[0] == 'town':
        return 2 <= len(parts) <= 3 and bool(parts[1]) and (len(parts) == 2 or parts[2] in TOWN_PURSES)
    return len(parts) == 2 and bool(parts[1])


def _check_objective(o, ids, regions):
    rules.shape(o, ('id', 'label', 'on'), ('match', 'goal', 'measure', 'signal', 'after', 'optional', 'hidden'))
    rules.require(rules.identifier(o['id']) and o['id'] not in ids, 'Give every objective of a quest a unique ID.')
    rules.require(rules.word(o['label'], 180), 'Each objective needs a journal line of up to 180 characters.')
    rules.require(
        o['on'] in (*rules.MISSION_EVENTS, triggers.SIGNAL)
        and (o['on'] == triggers.SIGNAL) == ('signal' in o)
        and ('signal' not in o or rules.identifier(o['signal'])),
        'Choose an event of a wolf (or a story signal) for each objective.',
    )
    measure = o.get('measure', 'count')
    rules.require(measure in ('count', 'quantity', 'coins') and rules.integer(o.get('goal', 1), 1, 1000000),
                  'Choose a positive objective goal and measure.')
    rules.require(measure == 'count' or o['on'] == triggers.SIGNAL or o['on'] in rules.MISSION_MEASURES[measure],
                  f'{o["on"]} does not record {measure}. Choose occurrences or another event.')
    fields = o.get('match', {})
    rules.require(isinstance(fields, dict) and all(k in rules.FIELDS and rules.word(v, 400) for k, v in fields.items()),
                  'Invalid objective event filter.')
    rules.check_region_source(o, regions)
    for flag in ('optional', 'hidden'):
        rules.require(type(o.get(flag, False)) is bool, f'An objective is {flag} or not (true or false).')
    after = o.get('after', [])
    rules.require(isinstance(after, list) and len(after) <= OBJECTIVES_MAX and all(rules.identifier(a) for a in after)
                  and len(set(after)) == len(after) and o['id'] not in after,
                  'An objective comes after other objectives of its quest, named once each.')
    ids.add(o['id'])


def _check_fail(rule):
    if isinstance(rule, dict) and 'afterDays' in rule:
        rules.shape(rule, ('afterDays',), ('clock',))
        rules.require(rules.number(rule['afterDays'], 1 / 1440, 36500) and rule.get('clock', 'game') in ('game', 'real'),
                      'A deadline is a minute to 100 years, on the game or the real clock.')
        return
    rules.shape(rule, ('on',), ('match',))
    rules.require(rule['on'] in rules.EVENTS and rule['on'] not in ('clock', triggers.SIGNAL) or rule['on'] == triggers.SIGNAL,
                  'A quest fails on an event of the world, or a deadline.')
    fields = rule.get('match', {})
    rules.require(isinstance(fields, dict) and all(k in (*rules.FIELDS, 'signal') and rules.word(v, 400) for k, v in fields.items()),
                  'Invalid fail rule filter.')


def _check_reward(effect, qid, regions, scenes):
    rules.validate_effect(effect, 'quest ' + qid, regions, scenes)
    rules.require(effect['kind'] not in ('npc.line', 'npc.offer', 'participant.remove'),
                  f'quest {qid}: {effect["kind"]} cannot be a reward.')
    check_story_effect(effect, None, None)


def check_quest(q, p, ids):
    """One quest of a program (3.6)."""
    rules.shape(q, ('id', 'title', 'objectives'),
                ('summary', 'scope', 'contributors', 'lifetime', 'phases', 'repeat', 'giver', 'board', 'offer', 'handIn', 'join',
                 'abandon', 'fail', 'rewards', 'bonus'))
    rules.require(rules.identifier(q['id']) and q['id'] not in ids, 'Give every quest a unique ID.')
    ids.add(q['id'])
    qid = q['id']
    rules.require(rules.word(q['title'], 120) and ('summary' not in q or rules.word(q['summary'], 300)),
                  f'quest {qid}: a title of up to 120 characters, a summary of up to 300.')
    scope = q.get('scope', 'player')
    rules.require(scope in SCOPES, f'quest {qid}: its scope is player, party or story.')
    rules.require('contributors' not in q or (scope == 'story' and rules.integer(q['contributors'], 1, 500)),
                  f'quest {qid}: contributors (1-500) are for a quest of the whole story.')
    rules.require(q.get('lifetime', 'campaign') in LIFETIMES, f'quest {qid}: it lasts the campaign or its phase.')
    phase_ids = {ph['id'] for ph in p['phases'] if isinstance(ph, dict) and 'id' in ph} if isinstance(p.get('phases'), list) else set()
    phases = q.get('phases', [])
    rules.require(isinstance(phases, list) and all(ph in phase_ids for ph in phases) and len(set(phases)) == len(phases)
                  and ('phases' not in q or phases), f'quest {qid}: name phases of this story, once each.')
    repeat = q.get('repeat', 'once')
    rules.require(repeat in REPEATS and (repeat == 'once' or scope != 'story'),
                  f'quest {qid}: it is taken once, daily, weekly or always (a quest of the whole story only once).')
    rules.require('giver' not in q or rules.identifier(q['giver']), f'quest {qid}: choose the resident who gives it.')
    rules.require('board' not in q or rules.identifier(q['board']), f'quest {qid}: choose the town whose notice board shows it.')
    if 'offer' in q:
        rules.require('giver' in q, f'quest {qid}: an offer needs a giver.')
        rules.shape(q['offer'], ('say',), ('ask', 'accept', 'decline', 'handIn', 'thanks'))
        for key, size in (('say', 300), ('ask', 120), ('accept', 120), ('decline', 120), ('handIn', 120), ('thanks', 300)):
            rules.require(key not in q['offer'] or rules.word(q['offer'][key], size),
                          f'quest {qid}: the offer\'s {key} is words of up to {size} characters.')
    rules.require(type(q.get('handIn', 'giver' in q)) is bool and (not q.get('handIn') or 'giver' in q),
                  f'quest {qid}: a quest handed in needs a giver.')
    rules.require(type(q.get('abandon', True)) is bool, f'quest {qid}: abandon is true or false.')
    join = q.get('join', {})
    rules.shape(join, (), ('who', 'region', 'cap', 'limit'))
    rules.require(join.get('who', 'anyone' if joinable(p) else 'participants') in ('anyone', 'participants'),
                  f'quest {qid}: anyone may take it, or only the story\'s wolves.')
    rules.require(join.get('who') != 'anyone' or joinable(p),
                  f'quest {qid}: "anyone" may take it only in a story whose audience is open or everyone.')
    rules.require('region' not in join or join['region'] in rules.region_index(p), f'quest {qid}: choose a region of this story.')
    rules.require('cap' not in join or rules.integer(join['cap'], 1, AUDIENCE_CAP), f'quest {qid}: a cap of 1 to {AUDIENCE_CAP} at once.')
    rules.require('limit' not in join or rules.integer(join['limit'], 1, 100000), f'quest {qid}: a limit of 1 to 100,000 in all.')
    objectives = q['objectives']
    rules.require(isinstance(objectives, list) and 1 <= len(objectives) <= OBJECTIVES_MAX,
                  f'quest {qid}: one to {OBJECTIVES_MAX} objectives.')
    regions = rules.region_index(p)
    oids = set()
    for o in objectives:
        _check_objective(o, oids, regions)
    rules.require(any(not o.get('optional') for o in objectives), f'quest {qid}: at least one objective is required.')
    for o in objectives:
        rules.require(all(a in oids for a in o.get('after', [])), f'quest {qid}: "after" names objectives of this quest.')
    _check_order(objectives, qid)
    fails = q.get('fail', [])
    rules.require(isinstance(fails, list) and len(fails) <= FAIL_MAX, f'quest {qid}: up to {FAIL_MAX} fail rules.')
    for rule in fails:
        _check_fail(rule)
    scenes = {s['id']: s for s in p.get('scenes', []) if isinstance(s, dict) and 'id' in s}
    for key, most in (('rewards', REWARDS_MAX), ('bonus', BONUS_MAX)):
        effects = q.get(key, [])
        rules.require(isinstance(effects, list) and len(effects) <= most, f'quest {qid}: up to {most} {key}.')
        for e in effects:
            _check_reward(e, qid, regions, scenes)
    rules.require(not q.get('bonus') or any(o.get('optional') for o in objectives),
                  f'quest {qid}: a bonus is for doing the optional objectives too: add one.')


def _check_order(objectives, qid):
    after = {o['id']: o.get('after', []) for o in objectives}
    state = {}

    def visit(oid):
        rules.require(state.get(oid) != 1, f'quest {qid}: its objectives come after one another in a circle.')
        if state.get(oid) == 2:
            return
        state[oid] = 1
        for a in after[oid]:
            visit(a)
        state[oid] = 2

    for oid in after:
        visit(oid)


def check_quests(p):
    """A program's quests (version 2 only)."""
    qs = p.get('quests', [])
    rules.require(isinstance(qs, list) and len(qs) <= QUESTS_MAX, f'Choose up to {QUESTS_MAX} quests.')
    rules.require(not qs or p.get('version') == 2, 'Quests need a version 2 story.')
    ids = set()
    for q in qs:
        check_quest(q, p, ids)


def index(p):
    return {q['id']: q for q in p.get('quests', []) if isinstance(q, dict) and isinstance(q.get('id'), str)}


def check_story_effect(effect, phase, t):
    """An effect the story keeps itself (quests, who takes part, a choice in a window): its quest and objective exist."""
    kind = effect['kind']
    if kind not in STORY_EFFECTS:
        if kind == 'character.grant':
            rules.require(check_purse(effect['purse']), 'Name a purse that pays: town:<town>, town:<town>:works, till:<id> or chapter:<id>.')
        return
    if phase is not None:
        rules.require(triggers.version(phase) == 2, 'This effect needs a version 2 story.')
    if kind in QUEST_EFFECTS and phase is not None:
        known = phase.get('_questIndex')
        if known is not None:
            q = known.get(effect['quest'])
            rules.require(q is not None, f'Unknown quest "{effect["quest"]}".')
            if 'objective' in effect:
                rules.require(any(o['id'] == effect['objective'] for o in q['objectives']),
                              f'Quest "{q["id"]}" has no objective "{effect["objective"]}".')


def open_in(q, phase_id):
    return not q.get('phases') or phase_id in q['phases']


def open_quests(p, phase_id):
    return [q for q in p.get('quests', []) if open_in(q, phase_id)]


# --------------------------------------------------------------------------------------------------- the audience

def _party(state, wolf):
    return triggers.party_of(state, wolf)


def add_wolf(p, state, wolf):
    """Takes a wolf into the run's audience; False when it is full (its cap)."""
    cohort = state['cohort']
    i = bisect.bisect_left(cohort, wolf)
    if i < len(cohort) and cohort[i] == wolf:
        return True
    if len(cohort) >= cap_of(p) or not rules.identifier(wolf):
        return False
    cohort.insert(i, wolf)
    return True


def remove_wolf(p, state, wolf, day):
    """A wolf leaves the story: their quests under way are abandoned, their offers withdrawn."""
    cohort = state['cohort']
    i = bisect.bisect_left(cohort, wolf)
    if i >= len(cohort) or cohort[i] != wolf:
        return False
    del cohort[i]
    for qid, copies in state.get('quests', {}).items():
        for holder, copy in copies.items():
            if wolf in copy.get('members', ()) and copy['status'] in UNDER_WAY:
                if holder == wolf:
                    _end(copy, 'abandoned', day)
                elif wolf not in copy.setdefault('left', []):
                    copy['left'].append(wolf)
    for qid in list(state.get('questOffers', {})):
        _withdraw(state, qid, wolf)
    return True


def _withdraw(state, qid, wolf):
    offers = state.get('questOffers', {})
    if wolf in offers.get(qid, ()):
        offers[qid].remove(wolf)
        if not offers[qid]:
            del offers[qid]
        if not offers:
            state.pop('questOffers', None)


# ------------------------------------------------------------------------------------------------------- the copies

def holder_of(q, state, wolf):
    scope = q.get('scope', 'player')
    return wolf if scope == 'player' else 'story' if scope == 'story' else 'party:' + _party(state, wolf)


def copy_of(q, state, wolf, under_way=False):
    """(holder, copy) the wolf takes part in for this quest: their own, their party's (by its members, fixed when it
    started), or the story's one copy; the newest under way first."""
    copies = state.get('quests', {}).get(q['id'], {})
    scope = q.get('scope', 'player')
    if scope == 'player':
        c = copies.get(wolf)
        return (wolf, c) if c and (not under_way or c['status'] in UNDER_WAY) else (None, None)
    if scope == 'story':
        c = copies.get('story')
        return ('story', c) if c and wolf in state['cohort'] and (not under_way or c['status'] in UNDER_WAY) else (None, None)
    best = (None, None)
    for holder, c in copies.items():
        if wolf in c.get('members', ()):
            if c['status'] in UNDER_WAY and wolf not in c.get('left', ()):
                return holder, c
            if not under_way and (best[1] is None or c.get('began', 0) > best[1].get('began', 0)):
                best = (holder, c)
    return best


def again(q, copy):
    """The game day a wolf may take an ended quest again (None: never)."""
    if copy['status'] == 'done':
        gap = REPEATS[q.get('repeat', 'once')]
        return None if gap is None else copy.get('ended', 0) + gap
    return copy.get('ended', 0)


def under_way(state, qid):
    return sum(c['status'] in UNDER_WAY for c in state.get('quests', {}).get(qid, {}).values())


def full(q, state):
    join = q.get('join', {})
    return ('cap' in join and under_way(state, q['id']) >= join['cap']) or \
        ('limit' in join and state.get('questStarts', {}).get(q['id'], 0) >= join['limit'])


def _end(copy, status, day):
    copy['status'] = status
    copy['ended'] = day


class Run:
    """One fact's quest work: where its own facts go (`push`), the day and the clock."""

    def __init__(self, p, phase, state, day, elapsed, push):
        self.p, self.phase, self.state, self.day, self.elapsed, self.push = p, phase, state, day, elapsed, push
        self.quests = index(p)

    def fact(self, kind, actor, qid, item=''):
        self.push({'kind': kind, 'actor': actor, 'target': '', 'detail': qid, 'item': item})

    def toast(self, wolf, text):
        self.state.setdefault('toasts', {})[wolf] = text

    # ---- starting, offering, accepting
    def may_start(self, q, wolf, invited=False):
        """Why a wolf may not take a quest now ('' when they may)."""
        state = self.state
        if not open_in(q, state['phase']):
            return 'That is not on offer just now.'
        who = q.get('join', {}).get('who', 'anyone' if joinable(self.p) else 'participants')
        if wolf not in state['cohort'] and not invited and (who == 'participants' or not joinable(self.p)):
            return 'That is only for those already in the story.'
        holder, c = copy_of(q, state, wolf)
        if c is not None and c['status'] in UNDER_WAY and (holder != 'story' or q.get('scope') != 'story'):
            return 'You have taken that on already.'
        if c is not None and c['status'] in ENDED and q.get('scope', 'player') != 'story':
            when = again(q, c)
            if when is None:
                return 'That has been done.'
            if self.day is not None and self.day < when:
                return 'Not again so soon.'
        if q.get('scope') == 'story' and c is not None and c['status'] in ENDED:
            return 'That has been done.'
        if q.get('scope') != 'story' or c is None:
            if full(q, state):
                return 'Enough wolves are on that already.'
        if wolf not in state['cohort'] and len(state['cohort']) >= cap_of(self.p):
            return 'The story has all the wolves it can take.'
        return ''

    def start(self, q, wolf, by='', invited=False, force=False):
        """Starts the wolf's copy (their party's, the story's). Returns '' or why not."""
        why = '' if force else self.may_start(q, wolf, invited)
        if why:
            return why
        state = self.state
        if not add_wolf(self.p, state, wolf):
            return 'The story has all the wolves it can take.'
        scope = q.get('scope', 'player')
        copies = state.setdefault('quests', {}).setdefault(q['id'], {})
        if scope == 'story':
            c = copies.get('story')
            if c is not None and c['status'] in UNDER_WAY:
                if wolf not in c['members']:
                    c['members'].append(wolf)
                _withdraw(state, q['id'], wolf)
                return ''
        if scope == 'party':
            party = _party(state, wolf)
            members = sorted({wolf, *(w for w, pt in state.get('parties', {}).items() if pt == party)})
            members = [m for m in members if add_wolf(self.p, state, m)]
            holder = 'party:' + party
        else:
            members = [wolf]
            holder = wolf if scope == 'player' else 'story'
        old = copies.get(holder)
        c = {'status': 'active', 'began': self.day if self.day is not None else 0, 'times': (old or {}).get('times', 0),
             'members': members, 'objectives': {}}
        if self.elapsed:
            c['beganReal'] = self.elapsed
        if by:
            c['by'] = by
        copies[holder] = c
        starts = state.setdefault('questStarts', {})
        starts[q['id']] = starts.get(q['id'], 0) + 1
        for m in members:
            _withdraw(state, q['id'], m)
        self.fact('quest started', wolf, q['id'])
        return ''

    def offer(self, q, wolf):
        why = self.may_start(q, wolf, invited=True)
        if why:
            return why
        if not add_wolf(self.p, self.state, wolf):
            return 'The story has all the wolves it can take.'
        offers = self.state.setdefault('questOffers', {}).setdefault(q['id'], [])
        if wolf not in offers:
            offers.append(wolf)
        return ''

    # ---- progress
    def credit(self, c, wolf):
        if wolf not in c.setdefault('credit', []):
            c['credit'].append(wolf)

    def objective_done(self, q, holder, c, o, actor):
        rec = c['objectives'].setdefault(o['id'], {})
        if rec.get('done') or rec.get('failed'):
            return
        rec.update(n=o.get('goal', 1), done=True, shown=True)
        self.fact('objective completed', actor, q['id'], o['id'])
        self.settle(q, holder, c, actor)

    def settle(self, q, holder, c, actor):
        """A copy whose required objectives are done is ready to hand in (with a giver) or done."""
        if c['status'] != 'active':
            return
        required = [o for o in q['objectives'] if not o.get('optional')]
        if not all(c['objectives'].get(o['id'], {}).get('done') for o in required):
            return
        if q.get('scope') == 'story' and len(c.get('members', ())) < q.get('contributors', 1):
            return
        if q.get('handIn', 'giver' in q):
            c['status'] = 'ready'
            return
        self.complete(q, holder, c, actor)

    def complete(self, q, holder, c, actor):
        if c['status'] not in UNDER_WAY:
            return
        _end(c, 'done', self.day if self.day is not None else 0)
        c['times'] = c.get('times', 0) + 1
        for o in q['objectives']:
            c['objectives'].setdefault(o['id'], {})['shown'] = True
        wolves = self.credited(q, c)
        self.reward(q, q.get('rewards', []), wolves)
        optional = [o for o in q['objectives'] if o.get('optional')]
        if q.get('bonus') and optional and all(c['objectives'].get(o['id'], {}).get('done') for o in optional):
            c['bonus'] = True
            self.reward(q, q['bonus'], wolves)
        self.fact('quest completed', actor, q['id'])

    def credited(self, q, c):
        """Each wolf with credit: the holder; a party's members still in it, and those who left after helping; the story's
        contributors."""
        members = c.get('members', [])
        left = set(c.get('left', ()))
        helped = set(c.get('credit', ()))
        return [m for m in members if m not in left or m in helped]

    def reward(self, q, effects, wolves):
        """Rewards as story effects: one for each wolf with credit where the effect names $actor, else once."""
        for effect in effects:
            per_wolf = [param['key'] for param in rules.EFFECT_PARAMS[effect['kind']]
                        if param['type'] in ('wolf', 'character') and effect.get(param['key']) == '$actor']
            for wolf in (wolves if per_wolf else [None]):
                e = dict(effect)
                for key in per_wolf:
                    e[key] = wolf
                if e['kind'] in STORY_EFFECTS:
                    self.story_effect(e, wolf or '')
                else:
                    e['reward'] = q['id']
                    self.state.setdefault('triggerEffects', []).append(e)

    def fail(self, q, holder, c, actor):
        if c['status'] not in UNDER_WAY:
            return
        _end(c, 'failed', self.day if self.day is not None else 0)
        self.fact('quest failed', actor, q['id'])

    # ---- the facts
    def track(self, fact):
        """An event's progress on every copy it counts for, its fail rules, and the quest events of the game."""
        state, kind = self.state, fact['kind']
        actor = fact.get('actor', '')
        if kind == 'clock':
            self.deadlines()
            return
        if kind in GAME_EVENTS or kind == LEAVES or kind in PARTY_EVENTS or (kind == 'arrival' and self.p['audience']['kind'] == 'everyone'):
            self.game_event(fact)
        for q in self.quests.values():
            copies = state.get('quests', {}).get(q['id'])
            if not copies:
                continue
            for holder, c in list(copies.items()):
                if c['status'] not in UNDER_WAY:
                    continue
                for rule in q.get('fail', ()):
                    if 'on' in rule and _matches(rule, fact):
                        self.fail(q, holder, c, actor)
                        break
            if actor not in state['cohort']:
                continue
            holder, c = copy_of(q, state, actor, under_way=True)
            if c is None:
                continue   # (A copy ready to hand in still counts its optional objectives.)
            if q.get('scope') == 'party' and actor in c.get('left', ()):
                continue
            done = {oid for oid, r in c['objectives'].items() if r.get('done')}
            progressed = False
            whole = q.get('scope') == 'story'
            for o in q['objectives']:
                rec = c['objectives'].get(o['id'], {})
                if rec.get('failed') or not set(o.get('after', ())) <= done or (rec.get('done') and not whole):
                    continue
                evidence = {**fact, 'kind': 'gift'} if o['on'] == 'gift' and kind == 'gift again' else fact
                if o.get('signal') != evidence.get('signal') or not rules.matches(o, evidence):
                    continue
                if rec.get('done'):
                    # (The story's one copy: a wolf doing what is done already still counts among its contributors.)
                    progressed = progressed or actor not in c['members']
                    continue
                measure = o.get('measure', 'count')
                gain = 1 if measure == 'count' else fact.get(measure, 0)
                if gain <= 0:
                    continue
                rec = c['objectives'].setdefault(o['id'], {})
                rec['n'] = min(rec.get('n', 0) + gain, o.get('goal', 1))
                progressed = True
                if rec['n'] >= o.get('goal', 1):
                    rec.update(done=True, shown=True)
                    self.fact('objective completed', actor, q['id'], o['id'])
            if progressed:
                self.credit(c, actor)
                if q.get('scope') == 'story' and actor not in c['members']:
                    c['members'].append(actor)
                self.settle(q, holder, c, actor)

    def deadlines(self):
        """A clock fact: every copy past its deadline fails."""
        for q in self.quests.values():
            for rule in q.get('fail', ()):
                if 'afterDays' not in rule:
                    continue
                real = rule.get('clock') == 'real'
                for holder, c in list(self.state.get('quests', {}).get(q['id'], {}).items()):
                    if c['status'] not in UNDER_WAY:
                        continue
                    now = self.elapsed if real else self.day
                    began = c.get('beganReal', 0) if real else c.get('began', 0)
                    if now is not None and now >= began + rule['afterDays']:
                        self.fail(q, holder, c, '')

    def game_event(self, fact):
        state, kind, wolf = self.state, fact['kind'], fact.get('actor', '')
        if kind in PARTY_EVENTS:
            party = fact.get('target', '')
            parties = state.setdefault('parties', {})
            if kind == 'party joined' and party:
                parties[wolf] = party
            elif parties.get(wolf) == party:
                del parties[wolf]
                for copies in state.get('quests', {}).values():
                    c = copies.get('party:' + party)
                    if c and wolf in c.get('members', ()) and c['status'] in UNDER_WAY and wolf not in c.setdefault('left', []):
                        c['left'].append(wolf)
            if not parties:
                state.pop('parties', None)
            return
        if kind == 'arrival':
            if wolf and wolf not in state['cohort']:
                add_wolf(self.p, state, wolf)
            return
        if kind == LEAVES:
            if fact.get('mine', True) and joinable(self.p):
                remove_wolf(self.p, state, wolf, self.day)
            return
        if kind == 'dialogue choice':
            choice = state.get('offers', {}).get(fact.get('detail', ''))
            if choice and choice.get('once') and wolf not in choice.setdefault('picked', []):
                choice['picked'].append(wolf)
            return
        q = self.quests.get(fact.get('detail', ''))
        if q is None:
            return
        giver = fact.get('item', '')
        if kind == 'quest accepted':
            invited = wolf in state.get('questOffers', {}).get(q['id'], ())
            board = giver.startswith('board:')
            if not giver and not invited:
                return
            if board and giver[6:] != q.get('board'):
                return   # (Taken from a town's notice board: the one the quest is posted on.)
            if giver and not board and giver != q.get('giver'):
                return
            why = self.start(q, wolf, by=giver, invited=invited)
            if why:
                self.toast(wolf, f'{q["title"]}: {why}')
        elif kind == 'quest declined':
            _withdraw(state, q['id'], wolf)
        elif kind == 'quest handed in':
            holder, c = copy_of(q, state, wolf, under_way=True)
            if c is not None and c['status'] == 'ready' and giver == q.get('giver'):
                self.credit(c, wolf)
                self.complete(q, holder, c, wolf)
        elif kind == 'quest abandoned':
            if q.get('abandon', True) is False or q.get('scope') == 'story':
                return
            holder, c = copy_of(q, state, wolf, under_way=True)
            if c is None:
                return
            if holder == wolf:
                _end(c, 'abandoned', self.day)
            else:
                if wolf not in c.setdefault('left', []):
                    c['left'].append(wolf)
                if all(m in c['left'] for m in c['members']):
                    _end(c, 'abandoned', self.day)

    # ---- the story's own effects (from a trigger, a reward, or an admin)
    def story_effect(self, effect, actor):
        state, kind = self.state, effect['kind']
        wolf = effect.get('player', '')
        if wolf == '$actor':
            wolf = actor
        if kind == 'participant.add':
            add_wolf(self.p, state, wolf)
            return
        if kind == 'participant.remove':
            remove_wolf(self.p, state, wolf, self.day)
            return
        if kind == 'npc.offer':
            state.setdefault('offers', {})[effect['id']] = {
                'npc': effect['npc'], 'words': effect['words'], 'reply': effect['reply'],
                **({'player': effect['player'] if effect['player'] != '$actor' else actor} if effect.get('player') else {}),
                'once': bool(effect.get('once')), 'picked': []}
            return
        q = self.quests.get(effect.get('quest'))
        if q is None or not wolf:
            return
        if kind == 'quest.start':
            why = self.start(q, wolf, invited=True)
            if why:
                self.toast(wolf, f'{q["title"]}: {why}')
            return
        if kind == 'quest.offer':
            self.offer(q, wolf)
            return
        holder, c = copy_of(q, state, wolf, under_way=True)
        if c is None:
            return
        if kind == 'quest.complete':
            self.complete(q, holder, c, wolf)
        elif kind == 'quest.fail':
            self.fail(q, holder, c, wolf)
        else:
            o = next((o for o in q['objectives'] if o['id'] == effect['objective']), None)
            if o is None:
                return
            rec = c['objectives'].setdefault(o['id'], {})
            if kind == 'objective.reveal':
                rec['shown'] = True
            elif kind == 'objective.fail':
                if not rec.get('done'):
                    rec.update(failed=True, shown=True)
                    if not o.get('optional'):
                        self.fail(q, holder, c, wolf)
            elif kind == 'objective.complete':
                self.credit(c, wolf)
                self.objective_done(q, holder, c, o, wolf)
            elif kind == 'objective.progress' and c['status'] == 'active' and not rec.get('done') and not rec.get('failed'):
                rec['n'] = min(rec.get('n', 0) + effect['amount'], o.get('goal', 1))
                self.credit(c, wolf)
                if rec['n'] >= o.get('goal', 1):
                    rec.update(done=True, shown=True)
                    self.fact('objective completed', wolf, q['id'], o['id'])
                    self.settle(q, holder, c, wolf)


def _matches(rule, fact):
    if rule['on'] == triggers.SIGNAL:
        if fact['kind'] != triggers.SIGNAL:
            return False
        m = rule.get('match', {})
        return all(fact.get(k, '') == v for k, v in m.items())
    return rules.matches(rule, fact)


def take_story_effects(p, phase, state, day, elapsed, push):
    """The story effects a firing queued, kept here: taken out of `triggerEffects` (the game's own stay, in order)."""
    pending = state.get('triggerEffects')
    if not pending or not any(e['kind'] in STORY_EFFECTS for e in pending):
        return
    mine = [e for e in pending if e['kind'] in STORY_EFFECTS]
    state['triggerEffects'] = [e for e in pending if e['kind'] not in STORY_EFFECTS]
    run = Run(p, phase, state, day, elapsed, push)
    for e in mine:
        run.story_effect(e, e.get('player', ''))
    if not state['triggerEffects']:
        del state['triggerEffects']


def relevant(p):
    """Whether the quest machinery has anything to do for a program (a story without quests, with a fixed audience, is
    played exactly as before)."""
    return bool(p.get('quests')) or joinable(p) or _uses_story_effects(p)


def _uses_story_effects(p):
    for ph in p.get('phases', []):
        for t in [*p.get('triggers', []), *ph.get('triggers', [])]:
            for _, a in triggers.walk(t.get('actions', [])):
                if a.get('do') == 'effect' and a['effect'].get('kind') in STORY_EFFECTS:
                    return True
    return False


def leave_visit(p, state, target, day):
    """A run moves to another phase: copies of quests that last their phase end ("expired"), offers of quests not open in
    the next phase are withdrawn, and the visit's story choices go."""
    for q in p.get('quests', []):
        copies = state.get('quests', {}).get(q['id'], {})
        if q.get('lifetime') == 'phase':
            for c in copies.values():
                if c['status'] in UNDER_WAY:
                    _end(c, 'expired', day)
        if not open_in(q, target):
            state.get('questOffers', {}).pop(q['id'], None)
    if 'questOffers' in state and not state['questOffers']:
        del state['questOffers']
    state.pop('offers', None)


# ------------------------------------------------------------------------------------------------------- what is seen

def view(q, c, holder, state, wolf, status=None):
    status = status or c['status']
    seen, hidden = [], 0
    done = {oid for oid, r in (c or {}).get('objectives', {}).items() if r.get('done')}
    for o in q['objectives']:
        r = (c or {}).get('objectives', {}).get(o['id'], {})
        if o.get('hidden') and not r.get('shown') and not r.get('done'):
            hidden += 1
            continue
        seen.append({'id': o['id'], 'label': o['label'], 'optional': bool(o.get('optional')), 'done': bool(r.get('done')),
                     'failed': bool(r.get('failed')), 'amount': r.get('n', 0), 'goal': o.get('goal', 1),
                     'open': set(o.get('after', ())) <= done})
    v = {'id': q['id'], 'title': q['title'], 'summary': q.get('summary', ''), 'scope': q.get('scope', 'player'),
         'status': status, 'objectives': seen, 'hiddenLeft': hidden,
         'handIn': status == 'ready' and 'giver' in q,
         'abandon': status in UNDER_WAY and q.get('abandon', True) and q.get('scope', 'player') != 'story',
         'accept': status == 'offered' and 'giver' not in q}
    if 'giver' in q:
        v['giver'] = q['giver']
    if c and status in ENDED:
        v['again'] = again(q, c)
    if c and status in UNDER_WAY:
        ends = [c.get('began', 0) + r['afterDays'] for r in q.get('fail', ()) if 'afterDays' in r and r.get('clock', 'game') == 'game']
        if ends:
            v['deadline'] = round(min(ends), 4)
    if c and q.get('scope', 'player') != 'player':
        v['members'] = list(c.get('members', []))
    return v


def views(p, state, wolf):
    """What a wolf's journal shows of the story's quests: offered, under way and ended (each quest once)."""
    out = []
    for q in p.get('quests', []):
        holder, c = copy_of(q, state, wolf)
        if c is not None and c['status'] in UNDER_WAY:
            out.append(view(q, c, holder, state, wolf))
        elif wolf in state.get('questOffers', {}).get(q['id'], ()):
            out.append(view(q, None, None, state, wolf, 'offered'))
        elif c is not None:
            out.append(view(q, c, holder, state, wolf))
    return out


def trackers(p, state, wolf):
    """Shown variables (3.2, `show`): the story's, the wolf's, their party's and the world's, as label and value."""
    out = []
    if p.get('version') != 2:
        return out
    counters = state.get('counters', {})
    for d in p.get('variables', []):
        if not d.get('show') or d['type'] not in ('number', 'switch'):
            continue
        scope = d['scope']
        if scope == 'player':
            store = counters.get('player', {}).get(wolf, {})
        elif scope == 'party':
            store = counters.get('party', {}).get(_party(state, wolf), {})
        else:
            store = counters.get(scope, {})
        out.append({'label': d['show'], 'value': store.get(d['id'], triggers.initial(d))})
    return out


def journal(p, state, wolf):
    j = {'quests': views(p, state, wolf), 'trackers': trackers(p, state, wolf)}
    if wolf in state.get('toasts', {}):
        j['toast'] = state['toasts'][wolf]
    return j


def offers(p, state):
    """The givers' offers for the game's dialogue windows, in this phase."""
    out = []
    for q in open_quests(p, state['phase']):
        o, join = q.get('offer', {}), q.get('join', {})
        entry = {'quest': q['id'], 'title': q['title'], 'summary': q.get('summary', ''),
                 'ask': ask_words(q), 'say': o.get('say') or q.get('summary') or q['title'],
                 'accept': o.get('accept', DEFAULT_WORDS['accept']), 'decline': o.get('decline', DEFAULT_WORDS['decline']),
                 'handIn': hand_in_words(q), 'thanks': o.get('thanks', ''),
                 'who': join.get('who', 'anyone' if joinable(p) else 'participants'),
                 **({'region': join['region']} if 'region' in join else {}), 'full': full(q, state)}
        if 'giver' in q:
            out.append({**entry, 'npc': q['giver']})
        if 'board' in q:
            # (A town's notice board: the quest under QUESTS, taken with TAKE IT ON; handed in to its giver if it has one.)
            out.append({**entry, 'npc': '', 'board': q['board']})
    return out


def choices(state):
    return [{'id': cid, 'npc': c['npc'], 'words': c['words'], 'reply': c['reply'],
             'players': [c['player']] if c.get('player') else None, 'once': c.get('once', False), 'picked': c.get('picked', [])}
            for cid, c in sorted(state.get('offers', {}).items())]


def board(p, state):
    """The console's Quests grid (doc 85, 5.3): each quest with every copy, its counts, and whether it is open now."""
    out = []
    for q in p.get('quests', []):
        copies = state.get('quests', {}).get(q['id'], {})
        rows = []
        for holder, c in sorted(copies.items()):
            rows.append({'holder': holder, 'kind': 'story' if holder == 'story' else 'party' if holder.startswith('party:') else 'wolf',
                         'members': c.get('members', []), 'left': c.get('left', []), 'status': c['status'],
                         'began': c.get('began'), 'ended': c.get('ended'), 'times': c.get('times', 0),
                         'objectives': c.get('objectives', {}), 'credit': c.get('credit', []), 'bonus': bool(c.get('bonus'))})
        counts = {s: 0 for s in ('offered', *STATUSES)}
        for c in copies.values():
            counts[c['status']] += 1
        counts['offered'] = len(state.get('questOffers', {}).get(q['id'], ()))
        join = q.get('join', {})
        out.append({'id': q['id'], 'title': q['title'], 'summary': q.get('summary', ''), 'scope': q.get('scope', 'player'),
                    'lifetime': q.get('lifetime', 'campaign'), 'repeat': q.get('repeat', 'once'), 'giver': q.get('giver'),
                    'open': open_in(q, state['phase']),
                    'objectives': [{'id': o['id'], 'label': o['label'], 'optional': bool(o.get('optional')),
                                    'hidden': bool(o.get('hidden')), 'goal': o.get('goal', 1), 'measure': o.get('measure', 'count'),
                                    'after': o.get('after', [])} for o in q['objectives']],
                    'copies': rows, 'counts': counts, 'offered': state.get('questOffers', {}).get(q['id'], []),
                    'starts': state.get('questStarts', {}).get(q['id'], 0), 'cap': join.get('cap'), 'limit': join.get('limit')})
    return out


def fingerprint(p, state):
    """What the game's copy of the journals and offers depends on, to send them only when it changed."""
    shown = [d['id'] for d in p.get('variables', []) if d.get('show')] if p.get('version') == 2 else []
    return json.dumps([state.get('quests'), state.get('questOffers'), state.get('offers'), state['phase'],
                       state.get('toasts'), state.get('parties') if any(q.get('scope') == 'party' for q in p.get('quests', [])) else None,
                       state.get('counters') if shown else None], sort_keys=True, separators=(',', ':'), default=str)


# ------------------------------------------------------------------------------------------------- moving a run (5b)

def migrate(old, new, state, report):
    """Quests onto a new version (doc 85, Phase 5b's notes for Phase 6): a quest kept by id when its objectives (and
    scope) are unchanged; its progress reset when they changed (copies stay under way, every objective at nothing); gone
    in the new version, its copies and offers dropped. `report(kind, what, why)` adds a line."""
    before, after = index(old), index(new)
    copies = state.get('quests', {})
    for qid in sorted(set(copies) | set(state.get('questOffers', {}))):
        q_new = after.get(qid)
        if q_new is None:
            copies.pop(qid, None)
            state.get('questOffers', {}).pop(qid, None)
            report('dropped', f'quest {qid}', 'the new version has no such quest')
            continue
        q_old = before.get(qid, {})
        same = rules._json(q_old.get('objectives')) == rules._json(q_new['objectives']) and \
            q_old.get('scope', 'player') == q_new.get('scope', 'player')
        if same:
            report('kept', f'quest {qid}', 'its objectives are unchanged')
            continue
        moved = 0
        for c in copies.get(qid, {}).values():
            if c['status'] in UNDER_WAY:
                c['objectives'] = {}
                c['status'] = 'active'
                moved += 1
        if q_old.get('scope', 'player') != q_new.get('scope', 'player'):
            copies.pop(qid, None)
            report('reset', f'quest {qid}', 'its scope changed: every copy starts again')
        else:
            report('reset', f'quest {qid}', f'its objectives changed: the progress of {moved} cop{"y" if moved == 1 else "ies"} under way starts again')
    for qid in sorted(set(after) - set(before)):
        report('added', f'quest {qid}', 'new in this version')
    if 'questOffers' in state and not state['questOffers']:
        del state['questOffers']
    if 'quests' in state and not state['quests']:
        del state['quests']
