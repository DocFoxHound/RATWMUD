"""Event–condition–action rules. Signals retain the originating world fact and last for one dispatch.

Version 2 of the language (Docs/Design/85-story-triggers-and-campaign-control.md, 3.2, 3.4, 3.5 and Phase 5): declared,
typed variables in four scopes, conditions over the story's own state, and flow inside a trigger's actions (if / else if /
else, wait, for each, random, run, enable, disable, stop, note, finish). A version 1 program runs exactly as before: every
version 2 path is behind `phase['_v'] == 2`. Core/RatwStoryTriggers.cpp is this file's twin (tools/test_story_native.py)."""

import operator

import story_native

SIGNAL = 'story signal'
COMPARE = {
    '=': operator.eq,
    '!=': operator.ne,
    '>': operator.gt,
    '>=': operator.ge,
    '<': operator.lt,
    '<=': operator.le,
}
LIMIT = 1_000_000_000

# ------------------------------------------------------------------------------------------------- version 2: the words
VAR_TYPES = ('number', 'switch', 'text', 'wolf', 'resident', 'place', 'region', 'item', 'faction',
             'numbers', 'texts', 'wolves', 'residents', 'timer')
LISTS = {'numbers': 'number', 'texts': 'text', 'wolves': 'wolf', 'residents': 'resident'}
TEXTUAL = ('text', 'wolf', 'resident', 'place', 'region', 'item', 'faction')
SCOPES = ('story', 'player', 'party', 'world')
PERSONAL = ('player', 'party')
AMOUNTS = ('quantity', 'coins')
EVENT_FIELDS = ('actor', 'target', 'cell', 'item', 'detail')
LIST_MAX = 500           # Values in a list variable.
EACH_MAX = 500           # Turns of one For each.
TEXT_MAX = 200           # Characters of a text value.
ACTIONS_V2 = 24          # Actions of one trigger, counting those inside If, For each and Random.
NEST_V2 = 3              # If, For each and Random inside one another.
RUN_DEPTH = 8            # Run inside run, at most (cycles are refused when the story is checked).
REAL_WAIT_MIN = 5 / 86400
OUTCOMES = ('success', 'failure', 'abandoned')
ARITH = ('plus', 'minus', 'min', 'max')
# Doc 85, Phase 8: conditions over the world's facts as of the event (game.story_facts, carried on the event as `facts`), and
# over the story's quests. A fact that is missing (a wolf away, no facts at all) is unknown, and unknown never fires.
FACTS_V2 = ('wolf here', 'wolf in region', 'wolf in place', 'wolf in town', 'wolf indoors', 'count in region', 'carries', 'coins',
            'standing', 'reputation', 'wolf is', 'party size', 'in party with', 'resident is', 'resident in place',
            'resident in region', 'resident has', 'hour between', 'calendar is', 'weather is', 'relation is')
QUEST_V2 = ('quest is', 'objective is')
CONDITIONS_V2 = ('compare', 'switch', 'in list', 'phase is', 'visit is', 'timer', 'fired', 'chance', 'days since', *FACTS_V2, *QUEST_V2)
STANCES = ('war', 'hostile', 'tense', 'neutral', 'friendly', 'allied')
WOLF_STATES = ('downed', 'hurt', 'wanted', 'held')
RESIDENT_STATES = ('alive', 'dead', 'indoors', 'out of doors')
RESIDENT_HAS = ('job', 'mood', 'mantle', 'town')
CALENDAR = {'weekday': ('Dawnday', 'Hearthday', 'Stoneday', 'Riverday', 'Emberday', 'Marketday', 'Restday'),
            'season': ('spring', 'summer', 'autumn', 'winter'), 'period': ('dawn', 'day', 'dusk', 'night')}
WEATHERS = ('clear', 'overcast', 'rain', 'storm', 'fog', 'snow', 'sandstorm')
QUEST_STATES = ('none', 'offered', 'active', 'ready', 'done', 'failed', 'abandoned', 'expired')
OBJECTIVE_STATES = ('done', 'failed', 'to do', 'hidden')
# (Each facts condition: its keys besides `test`, and the optional ones.)
FACT_SHAPES = {
    'wolf here': ((), ('who',)), 'wolf in region': (('region',), ('who',)), 'wolf in place': (('place',), ('who',)),
    'wolf in town': (('town',), ('who',)), 'wolf indoors': (('is',), ('who',)), 'count in region': (('region', 'op', 'value'), ()),
    'carries': (('item', 'op', 'value'), ('who',)), 'coins': (('op', 'value'), ('who',)), 'standing': (('faction', 'op', 'value'), ('who',)),
    'reputation': (('town', 'op', 'value'), ('who',)), 'wolf is': (('state', 'is'), ('who',)), 'party size': (('op', 'value'), ('who',)),
    'in party with': (('other',), ('who',)), 'resident is': (('npc', 'state'), ()), 'resident in place': (('npc', 'place'), ()),
    'resident in region': (('npc', 'region'), ()), 'resident has': (('npc', 'what', 'value'), ()), 'hour between': (('from', 'to'), ()),
    'calendar is': (('what', 'value'), ()), 'weather is': (('weather',), ('where',)), 'relation is': (('faction', 'other', 'op', 'stance'), ()),
}
FLOW_V2 = ('if', 'wait', 'each', 'random', 'run', 'enable', 'disable', 'stop', 'note', 'finish')
STATE_V2 = ('subtract', 'multiply', 'clamp', 'turn on', 'turn off', 'toggle', 'remember', 'list add', 'list remove',
            'list clear', 'pick', 'start timer', 'stop timer', 'shift')
DEFAULTS = {'number': 0, 'switch': False, 'timer': None}


def version(phase):
    return phase.get('_v', 1)


def family(kind):
    """The kind a value is compared as: number, switch or text (every reference is an id, a text)."""
    return 'number' if kind == 'number' else 'switch' if kind == 'switch' else 'text'


def initial(decl):
    if 'initial' in decl:
        return decl['initial']
    kind = decl['type']
    return [] if kind in LISTS else DEFAULTS.get(kind, '')


def branches(a):
    """The action lists inside one action, each with its name in an address: then, elif:k, else, actions, choice:k."""
    op = a.get('do') if isinstance(a, dict) else None
    if op == 'if':
        yield 'then', a['then']
        for k, b in enumerate(a.get('elif', ())):
            yield f'elif:{k}', b['then']
        if 'else' in a:
            yield 'else', a['else']
    elif op == 'each':
        yield 'actions', a['actions']
    elif op == 'random':
        for k, c in enumerate(a['choices']):
            yield f'choice:{k}', c['actions']


def branch(a, name):
    if name in ('then', 'else', 'actions'):
        return a[name]
    kind, k = name.split(':')
    return a['elif'][int(k)]['then'] if kind == 'elif' else a['choices'][int(k)]['actions']


def walk(actions, base=()):
    """Every action of a list, nested ones too, each with its address ((2, 'then', 0) is actions[2].then[0])."""
    for i, a in enumerate(actions):
        yield (*base, i), a
        for name, sub in branches(a):
            yield from walk(sub, (*base, i, name))


def walk_loose(actions):
    """As walk, over a program that may be malformed (any action list it can read)."""
    for a in actions if isinstance(actions, list) else ():
        if not isinstance(a, dict):
            continue
        yield a
        lists = [a.get('then'), a.get('else'), a.get('actions')]
        lists += [b.get('then') for b in a.get('elif', ()) if isinstance(b, dict)] if isinstance(a.get('elif'), list) else []
        lists += [c.get('actions') for c in a.get('choices', ()) if isinstance(c, dict)] if isinstance(a.get('choices'), list) else []
        for sub in lists:
            yield from walk_loose(sub)


def part_of(address):
    """An address as the Problems list names it: actions[2].elif[0].then[1], actions[3].choices[1].actions[0]."""
    out, i = f'actions[{address[0]}]', 1
    while i < len(address):
        name, index = address[i], address[i + 1]
        if ':' in name:
            kind, k = name.split(':')
            out += f'.elif[{k}].then[{index}]' if kind == 'elif' else f'.choices[{k}].actions[{index}]'
        else:
            out += f'.{name}[{index}]'
        i += 2
    return out


def conditions_in(c):
    """Every condition of a tree, groups included."""
    yield c
    if c.get('test') in ('all', 'any'):
        for child in c['conditions']:
            yield from conditions_in(child)
    elif c.get('test') == 'not':
        yield from conditions_in(c['condition'])


def rule_conditions(t):
    """Every condition a trigger tests: its own, and those of the If actions inside it."""
    if 'if' in t:
        yield from conditions_in(t['if'])
    for _, a in walk(t['actions']):
        if a['do'] == 'if':
            yield from conditions_in(a['if'])
            for b in a.get('elif', ()):
                yield from conditions_in(b['if'])


def operands_in(o):
    if isinstance(o, dict):
        yield o
        for key in ARITH:
            if key in o:
                for child in o[key]:
                    yield from operands_in(child)


def routes(phase):
    return phase['exits'] + [
        {
            'to': a['phase'],
            'on': 'trigger transition',
            'participant': t.get('audience') == 'participant',
        }
        for t in phase.get('triggers', [])
        for _, a in walk(t['actions'])
        if a['do'] == 'enter'
    ]


def validate(phase):
    import story_program as rules

    require, shape = rules.require, rules.shape
    triggers = phase.get('triggers', [])
    require(
        isinstance(triggers, list) and len(triggers) <= 64,
        'Choose up to 64 triggers per phase.',
    )
    ids, links, runs = set(), {}, {}
    for t in triggers:
        _check_trigger(t, phase, ids, links, runs)
    # Reject cycles even when the current fixtures would never take that path.
    for graph, words in ((links, 'Story signals form a cycle at '), (runs, 'Triggers run one another in a cycle at ')):
        visited = set()

        def visit(name, stack):
            require(name not in stack, words + name)
            if name not in visited:
                for child in graph.get(name, ()):
                    visit(child, stack | {name})
                visited.add(name)

        for name in graph:
            visit(name, set())


def _check_condition(c, depth, phase, missions, each=None):
    import story_program as rules

    require, shape = rules.require, rules.shape
    require(
        isinstance(c, dict) and depth <= 5,
        'Condition groups may nest up to five levels.',
    )
    kind = c.get('test')
    v2 = version(phase) == 2
    if kind in CONDITIONS_V2:
        require(v2, 'This condition needs a version 2 story.')
        _check_condition_v2(c, phase, each)
    elif kind in ('all', 'any'):
        shape(c, ('test', 'conditions'))
        children = c['conditions']
        require(
            isinstance(children, list) and 1 <= len(children) <= 8,
            'A condition group needs 1–8 conditions.',
        )
        for child in children:
            _check_condition(child, depth + 1, phase, missions, each)
    elif kind == 'not':
        shape(c, ('test', 'condition'))
        _check_condition(c['condition'], depth + 1, phase, missions, each)
    elif kind == 'region':
        shape(c, ('test', 'region'))
        require(rules.identifier(c['region']) and c['region'] in phase.get('_regions', {}), 'Unknown story region.')
    elif kind == 'mission':
        shape(c, ('test', 'id'))
        require(
            rules.identifier(c['id']) and c['id'] in missions,
            'Unknown mission in this phase.',
        )
    elif kind in ('counter', 'field'):
        if kind == 'counter':
            require(not v2, 'A version 2 story compares declared variables ("compare"), not counters.')
            shape(c, ('test', 'key', 'scope', 'op', 'value'))
            require(
                rules.identifier(c['key']) and c['scope'] in ('story', 'player'),
                'Choose a counter name and scope.',
            )
        else:
            shape(c, ('test', 'field', 'op', 'value'))
            require(
                c['field'] in (*rules.FIELDS, 'quantity', 'coins'),
                'Unknown event field.',
            )
        if kind == 'counter' or c['field'] in ('coins', 'quantity'):
            require(
                c['op'] in tuple(COMPARE) and rules.integer(c['value'], -LIMIT, LIMIT),
                'Choose a numeric comparison.',
            )
        else:
            require(
                c['op'] in ('=', '!=') and rules.word(c['value'], 400),
                'Choose a text comparison.',
            )
    else:
        raise ValueError('Unknown trigger condition.')


# ----------------------------------------------------------------------------------------- version 2: checking a program

def _decl(phase, name, kinds=None, words='variable'):
    import story_program as rules

    decls = phase.get('_vars', {})
    rules.require(isinstance(name, str) and name in decls, f'Declare the variable "{name}" first (Variables).'
                  if isinstance(name, str) else 'Name a variable.')
    d = decls[name]
    rules.require(kinds is None or d['type'] in kinds, f'"{name}" is a {d["type"]}: choose a {words}.')
    return d


def _check_of(o, d, each):
    import story_program as rules

    if 'of' not in o:
        return
    rules.require(o['of'] in ('$actor', '$each') and d['scope'] in PERSONAL,
                  'Only a variable of each wolf or party is read "of" a wolf ($actor or $each).')
    rules.require(o['of'] != '$each' or each == 'wolf', '$each is a wolf only inside For each over wolves.')


def operand_type(o, phase, each=None, depth=0):
    """What a value in a comparison or a Set is: number, switch or text; a malformed one raises."""
    import story_program as rules

    require, shape = rules.require, rules.shape
    if type(o) is bool:
        return 'switch'
    if type(o) in (int, float):
        require(rules.number(o, -LIMIT, LIMIT), 'A number between plus and minus 1,000,000,000.')
        return 'number'
    if isinstance(o, str):
        require(len(o) <= TEXT_MAX and all(ord(ch) >= 32 for ch in o), f'A text of up to {TEXT_MAX} characters.')
        return 'text'
    require(isinstance(o, dict), 'Unknown value.')
    if 'var' in o:
        shape(o, ('var',), ('of',))
        d = _decl(phase, o['var'])
        require(d['type'] not in LISTS and d['type'] != 'timer',
                f'"{o["var"]}" is a {d["type"]}: read a list with "count" and a timer with "since".')
        _check_of(o, d, each)
        return family(d['type'])
    if 'field' in o:
        shape(o, ('field',))
        require(o['field'] in (*EVENT_FIELDS, *AMOUNTS), 'Unknown event field.')
        return 'number' if o['field'] in AMOUNTS else 'text'
    if 'count' in o:
        shape(o, ('count',), ('of',))
        d = _decl(phase, o['count'], LISTS, 'list')
        _check_of({'of': o['of']} if 'of' in o else {}, d, each)
        return 'number'
    if 'since' in o:
        shape(o, ('since',), ('of',))
        d = _decl(phase, o['since'], ('timer',), 'timer')
        _check_of({'of': o['of']} if 'of' in o else {}, d, each)
        return 'number'
    if 'each' in o:
        shape(o, ('each',))
        require(o['each'] is True and each is not None, 'The value of the loop is only inside For each.')
        return family(each)
    key = next((k for k in ARITH if k in o), None)
    require(key is not None, 'Unknown value.')
    shape(o, (key,))
    require(depth < 2 and isinstance(o[key], list) and len(o[key]) == 2, 'Arithmetic takes two values, two deep at most.')
    require(all(operand_type(x, phase, each, depth + 1) == 'number' for x in o[key]), 'Arithmetic is on numbers.')
    return 'number'


def _check_condition_v2(c, phase, each):
    import story_program as rules

    require, shape = rules.require, rules.shape
    kind = c['test']
    if kind == 'compare':
        shape(c, ('test', 'left', 'op', 'right'))
        left, right = operand_type(c['left'], phase, each), operand_type(c['right'], phase, each)
        require(left == right, f'Compare two values of one kind (a {left} and a {right}).')
        require(c['op'] in COMPARE and (left == 'number' or c['op'] in ('=', '!=')),
                'Numbers take = != > >= < <=; switches and texts take = and !=.')
    elif kind == 'switch':
        shape(c, ('test', 'var', 'is'), ('of',))
        _check_of(c, _decl(phase, c['var'], ('switch',), 'switch'), each)
        require(type(c['is']) is bool, 'A switch is on or off.')
    elif kind == 'in list':
        shape(c, ('test', 'list', 'value'), ('of',))
        d = _decl(phase, c['list'], LISTS, 'list')
        _check_of(c, d, each)
        require(operand_type(c['value'], phase, each) == family(LISTS[d['type']]), 'The value is not of the list\'s kind.')
    elif kind == 'phase is':
        shape(c, ('test', 'phase'))
        require(rules.identifier(c['phase']) and c['phase'] in phase.get('_phaseIds', (c['phase'],)), 'Unknown phase.')
    elif kind == 'visit is':
        shape(c, ('test', 'op', 'value'))
        require(c['op'] in COMPARE and rules.integer(c['value'], 1, 1_000_000), 'Compare the visit with a number from 1.')
    elif kind == 'timer':
        shape(c, ('test', 'signal'), ('owner', 'op', 'days'))
        require(rules.identifier(c['signal']) and c.get('owner', 'story') in ('story', 'player'), 'Name a timer and whose it is.')
        require(('op' in c) == ('days' in c) and ('op' not in c or (c['op'] in COMPARE and rules.number(c['days'], 0, 36500))),
                'Compare the days left with a number of days, or ask only whether it is running.')
    elif kind == 'fired':
        shape(c, ('test', 'trigger', 'op', 'value'), ('in',))
        require(rules.identifier(c['trigger']) and c['trigger'] in {t.get('id') for t in phase.get('triggers', ()) if isinstance(t, dict)
                                                                    and isinstance(t.get('id'), str)},
                'Name a trigger this one can see.')
        require(c.get('in', 'visit') in ('visit', 'run') and c['op'] in COMPARE and rules.integer(c['value'], 0, 1_000_000),
                'Count its firings this visit or this run against a number.')
    elif kind == 'chance':
        shape(c, ('test', 'percent'))
        require(rules.integer(c['percent'], 1, 99), 'A chance of 1 to 99 in 100.')
    elif kind in FACTS_V2:
        _check_fact_condition(c, phase, each)
    elif kind in QUEST_V2:
        if kind == 'quest is':
            shape(c, ('test', 'quest', 'status'), ('of',))
            require(c['status'] in QUEST_STATES, 'A quest is none, offered, active, ready, done, failed, abandoned or expired.')
        else:
            shape(c, ('test', 'quest', 'objective', 'status'), ('of',))
            require(c['status'] in OBJECTIVE_STATES, 'An objective is done, failed, to do or hidden.')
        require(c.get('of', '$actor') in ('$actor', '$each', 'story') and (c.get('of') != '$each' or each == 'wolf'),
                'Whose: the acting wolf, the looped wolf (inside For each over wolves), or the story\'s copy.')
        known = phase.get('_questIndex', {})
        require(c['quest'] in known, f'Unknown quest "{c["quest"]}".' if isinstance(c['quest'], str) else 'Name a quest.')
        if kind == 'objective is':
            require(any(o['id'] == c['objective'] for o in known[c['quest']]['objectives']),
                    f'Quest "{c["quest"]}" has no objective "{c["objective"]}".')
    elif kind == 'days since':
        shape(c, ('test', 'from', 'op', 'days'), ('var', 'of'))
        require(c['from'] in ('phase', 'run', 'var') and ('var' in c) == (c['from'] == 'var') and ('of' not in c or 'var' in c),
                'Count the days since the phase opened, the run began, or a timer variable started.')
        if c['from'] == 'var':
            _check_of(c, _decl(phase, c['var'], ('timer',), 'timer'), each)
        require(c['op'] in COMPARE and rules.number(c['days'], 0, 36500), 'Compare with a number of days.')


def _check_who(v, each, words='the wolf'):
    import story_program as rules

    rules.require(v in ('$actor', '$each') or rules.identifier(v), f'Name {words}: the acting wolf, the looped wolf, or a wolf.')
    rules.require(v != '$each' or each == 'wolf', '$each is a wolf only inside For each over wolves.')


def _check_fact_condition(c, phase, each):
    """A condition over the world's facts (doc 85, 3.4 and Phase 8)."""
    import story_program as rules

    require = rules.require
    kind = c['test']
    needed, optional = FACT_SHAPES[kind]
    rules.shape(c, ('test', *needed), optional)
    if 'who' in c:
        _check_who(c['who'], each)
    if 'op' in c:
        ops = ('=', '!=', '>=', '<=') if kind == 'relation is' else tuple(COMPARE)
        require(c['op'] in ops, 'Choose a comparison.')
    if 'value' in c and kind not in ('resident has', 'calendar is'):
        low, high = {'standing': (-100, 100), 'reputation': (-1000, 1000), 'party size': (1, 12),
                     'count in region': (0, 5000), 'carries': (0, 100000)}.get(kind, (0, LIMIT))
        require(rules.integer(c['value'], low, high), f'A whole number from {low} to {high}.')
    for key in ('region',):
        if key in c:
            require(rules.identifier(c[key]) and c[key] in phase.get('_regions', {}), 'Unknown story region.')
    for key in ('place', 'town', 'faction', 'other', 'npc', 'item'):
        if key in c and not (kind == 'in party with' and key == 'other'):
            require(rules.identifier(c[key]), f'Name the {key}.')
    if kind == 'in party with':
        _check_who(c['other'], each, 'the other wolf')
    if 'is' in c:
        require(type(c['is']) is bool, 'Yes or no.')
    if kind == 'wolf is':
        require(c['state'] in WOLF_STATES, 'Downed, hurt, wanted or held.')
    elif kind == 'resident is':
        require(c['state'] in RESIDENT_STATES, 'Alive, dead, indoors or out of doors.')
    elif kind == 'resident has':
        require(c['what'] in RESIDENT_HAS and rules.word(c['value'], 80), 'A job, mood, mantle or town, and what it is.')
    elif kind == 'hour between':
        require(rules.integer(c['from'], 0, 23) and rules.integer(c['to'], 0, 23), 'Hours from 0 to 23.')
    elif kind == 'calendar is':
        require(c['what'] in CALENDAR and c['value'] in CALENDAR[c['what']], 'A weekday, a season or a time of day, and which.')
    elif kind == 'weather is':
        require(c['weather'] in WEATHERS, 'Clear, overcast, rain, storm, fog, snow or sandstorm.')
        require(c.get('where', '$actor') == '$actor' or rules.identifier(c['where']), 'Where the acting wolf is, or a place.')
    elif kind == 'relation is':
        require(c['stance'] in STANCES, 'War, hostile, tense, neutral, friendly or allied.')


class _Checking:
    """What checking one trigger's version 2 actions carries down the tree."""

    def __init__(self, t, phase, links, runs, collect=None):
        self.t, self.phase, self.links, self.runs, self.collect = t, phase, links, runs, collect
        self.total = 0


def _check_actions_v2(actions, cx, base=(), depth=0, each=None):
    import story_program as rules

    rules.require(isinstance(actions, list) and 1 <= len(actions) <= ACTIONS_V2, f'Choose 1–{ACTIONS_V2} actions in each list.')
    for i, a in enumerate(actions):
        address = (*base, i)
        try:
            cx.total += 1
            rules.require(cx.total <= ACTIONS_V2, f'A trigger has at most {ACTIONS_V2} actions, counting those inside If, '
                                                  'For each and Random.')
            _check_action_v2(a, cx, address, depth, each, i == len(actions) - 1)
        except (ValueError, KeyError, TypeError, AttributeError, IndexError) as error:
            if cx.collect is None:
                raise
            cx.collect.append(('.' + part_of(address), _message(error)))


def _check_action_v2(a, cx, address, depth, each, last):
    import story_program as rules

    require, shape = rules.require, rules.shape
    t, phase = cx.t, cx.phase
    require(isinstance(a, dict), 'Expected a trigger action.')
    kind = a.get('do')
    missions = {m['id'] for m in phase.get('missions', [])}

    def var(d_key='var', kinds=None, words='variable'):
        d = _decl(phase, a[d_key], kinds, words)
        _check_of(a, d, each)
        return d

    if kind in ('set', 'add', 'subtract', 'multiply'):
        require('key' not in a, 'In a version 2 story, Set and Add name a declared variable ("var").')
        shape(a, ('do', 'var', 'value'), ('of',))
        d = var()
        require(d['type'] not in LISTS and d['type'] != 'timer', 'Lists and timers have their own actions.')
        value = operand_type(a['value'], phase, each)
        if kind == 'set':
            require(value == family(d['type']), f'"{a["var"]}" is a {d["type"]}: set it to a {family(d["type"])}.')
        else:
            require(d['type'] == 'number' and value == 'number', 'Arithmetic is on a number variable, by a number.')
    elif kind == 'clamp':
        shape(a, ('do', 'var', 'min', 'max'), ('of',))
        var(kinds=('number',), words='number')
        require(rules.integer(a['min'], -LIMIT, LIMIT) and rules.integer(a['max'], -LIMIT, LIMIT) and a['min'] <= a['max'],
                'Keep it between two whole numbers, the lower first.')
    elif kind in ('turn on', 'turn off', 'toggle'):
        shape(a, ('do', 'var'), ('of',))
        var(kinds=('switch',), words='switch')
    elif kind == 'remember':
        shape(a, ('do', 'var', 'from'), ('of',))
        var(kinds=TEXTUAL, words='text or a reference')
        require(a['from'] in EVENT_FIELDS or (a['from'] == 'each' and each in ('wolf', 'resident', 'text')),
                "Remember one of the event's ids (actor, target, cell, item, detail), or the loop's.")
    elif kind in ('list add', 'list remove'):
        shape(a, ('do', 'var', 'value'), ('of',))
        d = var(kinds=tuple(LISTS), words='list')
        require(operand_type(a['value'], phase, each) == family(LISTS[d['type']]), 'The value is not of the list\'s kind.')
    elif kind == 'list clear':
        shape(a, ('do', 'var'), ('of',))
        var(kinds=tuple(LISTS), words='list')
    elif kind == 'pick':
        shape(a, ('do', 'list', 'var'), ('of',))
        source = _decl(phase, a['list'], LISTS, 'list')
        d = var()
        require(d['type'] not in LISTS and d['type'] != 'timer' and family(d['type']) == family(LISTS[source['type']]),
                'Pick into a variable of the list\'s kind.')
        require(source['scope'] not in PERSONAL or d['scope'] in PERSONAL or 'of' not in a, 'Whose list?')
    elif kind in ('start timer', 'stop timer'):
        shape(a, ('do', 'var'), ('of',))
        var(kinds=('timer',), words='timer')
    elif kind == 'shift':
        shape(a, ('do', 'signal', 'days'), ('owner',))
        require(rules.identifier(a['signal']) and a.get('owner', 'story') in ('story', 'player'), 'Name the timer and whose it is.')
        require(rules.number(a['days'], -36500, 36500) and a['days'] != 0, 'Move it by a number of days (before: negative).')
        require(a.get('owner') != 'player' or t.get('audience') == 'participant', 'A personal timer requires a participant trigger.')
    elif kind == 'emit':
        shape(a, ('do', 'signal'))
        require(rules.identifier(a['signal']), 'Give the emitted signal a name.')
        if t['on'] == SIGNAL:
            cx.links.setdefault(t['signal'], set()).add(a['signal'])
    elif kind in ('schedule', 'cancel'):
        _check_timer_action(a, t)
    elif kind == 'effect':
        shape(a, ('do', 'effect'))
        effect = a['effect']
        if isinstance(effect, dict) and '$each' in effect.values():
            require(each in ('wolf', 'resident'), '$each names a wolf or a resident only inside For each.')
            stand = '$actor' if each == 'wolf' else 'each'
            for param in rules.EFFECT_PARAMS.get(effect.get('kind'), ()):
                if effect.get(param['key']) == '$each':
                    require(param['type'] == each or (param['type'] == 'character' and each in ('wolf', 'resident')),
                            f'$each is a {each}: it cannot be {param["label"]}.')
            effect = {k: (stand if v == '$each' else v) for k, v in effect.items()}
        rules.validate_effect(effect, phase['id'], phase.get('_regions'), phase.get('_scenes'))
        require(effect['kind'] != 'npc.line', 'Set resident dialogue on phase entry.')
        rules.quests.check_story_effect(effect, phase, t)
        # (Doc 85, Phase 6: taking a wolf in, or offering or giving them a quest, may name the acting wolf from a trigger of
        # anyone in the world: that is how an open audience is joined.)
        require(a['effect'].get('player') != '$actor' or t.get('audience') == 'participant'
                or effect['kind'] in rules.quests.TAKES_IN, 'An actor effect requires a participant trigger.')
    elif kind == 'enter':
        shape(a, ('do', 'phase'))
        require(rules.identifier(a['phase']) and last, 'Enter phase must be the last action.')
        require(each is None, 'Go to a phase after the For each, not inside it.')
    elif kind == 'if':
        shape(a, ('do', 'if', 'then'), ('elif', 'else'))
        require(depth < NEST_V2, f'If, For each and Random nest {NEST_V2} deep at most.')
        _check_condition(a['if'], 1, phase, missions, each)
        elifs = a.get('elif', [])
        require(isinstance(elifs, list) and len(elifs) <= 4, 'Up to four Else ifs.')
        for b in elifs:
            shape(b, ('if', 'then'))
            _check_condition(b['if'], 1, phase, missions, each)
        for name, sub in branches(a):
            _check_actions_v2(sub, cx, (*address, name), depth + 1, each)
    elif kind == 'each':
        shape(a, ('do', 'over', 'actions'), ('list',))
        require(each is None, 'A For each cannot be inside another.')
        require(depth < NEST_V2, f'If, For each and Random nest {NEST_V2} deep at most.')
        require(a['over'] in ('participants', 'party', 'list') and ('list' in a) == (a['over'] == 'list'),
                'Loop over the participants, the acting wolf\'s party, or a list variable.')
        if a['over'] == 'list':
            d = _decl(phase, a['list'], LISTS, 'list')
            require(d['scope'] not in PERSONAL, 'Loop over a list of the story or the world (a wolf\'s own: Pick from it).')
            kind_each = LISTS[d['type']]
        else:
            kind_each = 'wolf'
        _check_actions_v2(a['actions'], cx, (*address, 'actions'), depth + 1, kind_each)
    elif kind == 'random':
        shape(a, ('do', 'choices'))
        require(depth < NEST_V2, f'If, For each and Random nest {NEST_V2} deep at most.')
        require(isinstance(a['choices'], list) and 2 <= len(a['choices']) <= 8, 'Random chooses between 2 and 8 lists.')
        for choice in a['choices']:
            shape(choice, ('weight', 'actions'))
            require(rules.integer(choice['weight'], 1, 100), 'Each choice weighs 1 to 100.')
        for name, sub in branches(a):
            _check_actions_v2(sub, cx, (*address, name), depth + 1, each)
    elif kind == 'wait':
        shape(a, ('do', 'days'), ('clock',))
        require(each is None, 'A wait cannot be inside For each.')
        clock = a.get('clock', 'game')
        require(clock in ('game', 'real') and rules.number(a['days'], 1 / 1440 if clock == 'game' else REAL_WAIT_MIN, 36500),
                'Wait at least a minute of the world\'s clock, or 5 seconds of the real one.')
    elif kind == 'run':
        shape(a, ('do', 'trigger'), ('check',))
        ids = {x.get('id') for x in phase.get('triggers', ()) if isinstance(x, dict) and isinstance(x.get('id'), str)}
        require(rules.identifier(a['trigger']) and a['trigger'] in ids and a['trigger'] != t['id'],
                'Run another trigger this one can see.')
        require(type(a.get('check', False)) is bool, 'Check its conditions, or not.')
        cx.runs.setdefault(t['id'], set()).add(a['trigger'])
    elif kind in ('enable', 'disable'):
        shape(a, ('do', 'trigger'), ('phase',))
        require(rules.identifier(a['trigger']) and ('phase' not in a or rules.identifier(a['phase'])), 'Name a trigger.')
        if 'phase' in a:
            known = phase.get('_phaseTriggers')
            require(known is None or a['trigger'] in known.get(a['phase'], ()), f'Phase {a["phase"]} has no trigger {a["trigger"]}.')
        else:
            require(a['trigger'] in {x.get('id') for x in phase.get('triggers', ()) if isinstance(x, dict) and isinstance(x.get('id'), str)},
                    'Name a trigger this one can see (or the phase of another).')
    elif kind == 'stop':
        shape(a, ('do',))
    elif kind == 'note':
        shape(a, ('do', 'text'))
        require(rules.word(a['text'], 300), 'A note is up to 300 characters.')
    elif kind == 'finish':
        shape(a, ('do', 'outcome', 'text'), ('who',))
        who = a.get('who', 'everyone')
        require(a['outcome'] in OUTCOMES and rules.word(a['text'], 300), 'Finish with success, failure or abandoned, and a line.')
        require(who in ('everyone', '$actor', '$each'), 'Finish for everyone, the acting wolf or the wolf of the loop.')
        require(who != 'everyone' or (last and each is None), 'Finishing the story for everyone is the last action.')
        require(who != '$actor' or t.get('audience') == 'participant', 'A wolf\'s own ending requires a participant trigger.')
        require(who != '$each' or each == 'wolf', '$each is a wolf only inside For each over wolves.')
    else:
        raise ValueError('Unknown trigger action.')


def _check_timer_action(a, t):
    import story_program as rules

    require, shape = rules.require, rules.shape
    kind = a['do']
    if kind == 'schedule':
        shape(
            a,
            ('do', 'signal', 'days'),
            ('repeat', 'scope', 'clock', 'owner'),
        )
        require(
            rules.number(a['days'], 1 / 1440, 36500)
            and type(a.get('repeat', False)) is bool
            and a.get('scope', 'visit') in ('visit', 'campaign')
            and a.get('clock', 'game') in ('game', 'real'),
            'Choose a valid timer lifetime, clock and positive delay.',
        )
    else:
        shape(a, ('do', 'signal'), ('owner',))
    require(
        a.get('owner', 'story') in ('story', 'player'),
        'Choose a timer owner.',
    )
    require(
        a.get('owner') != 'player' or t.get('audience') == 'participant',
        'A personal timer requires a participant trigger.',
    )
    require(rules.identifier(a['signal']), 'Name the timer signal.')


def _check_trigger(t, phase, ids, links, runs=None, collect=None):
    """One trigger's checks (validate's loop body): its id against `ids` (added to), its signal links into `links`, the
    triggers it runs into `runs`. `collect`, a list, gathers the problems of its version 2 actions one by one."""
    import story_program as rules

    require, shape = rules.require, rules.shape
    missions = {m['id'] for m in phase.get('missions', [])}
    v2 = version(phase) == 2

    def counter(c):
        require(
            rules.identifier(c['key']) and c['scope'] in ('story', 'player'),
            'Choose a counter name and scope.',
        )

    def condition(c, depth):
        _check_condition(c, depth, phase, missions)

    shape(
        t,
        ('id', 'on', 'actions'),
        ('match', 'signal', 'if', 'once', 'audience', 'afterDays', 'folder', 'note', 'enabled'),
    )
    require(
        rules.identifier(t['id']) and t['id'] not in ids,
        'Give every trigger in the phase a unique ID.',
    )
    # The Story Creator's folder and note (doc 85, 4.2; Phase 4): words for the author, which the engine ignores.
    require('folder' not in t or rules.word(t['folder'], 80), "A trigger's folder is a name of up to 80 characters.")
    require('note' not in t or rules.word(t['note'], 300), "A trigger's note is up to 300 characters.")
    # (Phase 5: "enabled": false is a trigger that starts turned off, until an Enable action or an admin turns it on.)
    require('enabled' not in t or (v2 and type(t['enabled']) is bool), 'Only a version 2 story starts a trigger turned off.')
    ids.add(t['id'])
    require(
        t['on'] in (*rules.EVENTS, SIGNAL) and t['on'] != 'dm signal',
        'Unknown trigger event.',
    )
    require(
        (t['on'] == SIGNAL) == ('signal' in t) and ('signal' not in t or rules.identifier(t['signal'])),
        'Choose a named story signal.',
    )
    require(
        t.get('audience', 'world') in ('world', 'participant')
        and t.get('once', 'each') in ('each', 'visit', 'player', 'campaign', 'member'),
        'Choose a trigger audience and repeat policy.',
    )
    if t['on'] == 'clock':
        require(
            rules.number(t.get('afterDays'), 1 / 1440, 36500)
            and t.get('once') == 'visit'
            and t.get('audience', 'world') == 'world',
            'Timers need a positive delay and fire once per visit.',
        )
    else:
        require('afterDays' not in t, 'Only timers take a delay.')
    match = t.get('match', {})
    require(
        isinstance(match, dict) and all(k in rules.FIELDS and rules.word(v, 400) for k, v in match.items()),
        'Invalid trigger filters.',
    )
    rules.check_region_source(t, phase.get('_regions'))
    if 'if' in t:
        condition(t['if'], 1)
    actions = t['actions']
    if v2:
        _check_actions_v2(actions, _Checking(t, phase, links, runs if runs is not None else {}, collect))
        return
    require(
        isinstance(actions, list) and 1 <= len(actions) <= 12,
        'Choose 1–12 ordered actions per trigger.',
    )
    for index, a in enumerate(actions):
        require(isinstance(a, dict), 'Expected a trigger action.')
        kind = a.get('do')
        if kind in ('add', 'set'):
            shape(a, ('do', 'key', 'scope', 'value'))
            counter(a)
            require(
                rules.integer(a['value'], -LIMIT, LIMIT) or a['value'] in ('coins', 'quantity'),
                'Choose a number, event coins or quantity.',
            )
        elif kind == 'emit':
            shape(a, ('do', 'signal'))
            require(rules.identifier(a['signal']), 'Give the emitted signal a name.')
            if t['on'] == SIGNAL:
                links.setdefault(t['signal'], set()).add(a['signal'])
        elif kind in ('schedule', 'cancel'):
            _check_timer_action(a, t)
        elif kind == 'effect':
            shape(a, ('do', 'effect'))
            rules.validate_effect(a['effect'], phase['id'], phase.get('_regions'), phase.get('_scenes'))
            require(
                a['effect']['kind'] != 'npc.line',
                'Set resident dialogue on phase entry.',
            )
            rules.quests.check_story_effect(a['effect'], phase, t)
            require(
                a['effect'].get('player') != '$actor' or t.get('audience') == 'participant',
                'An actor effect requires a participant trigger.',
            )
        elif kind == 'enter':
            shape(a, ('do', 'phase'))
            require(
                rules.identifier(a['phase']) and index == len(actions) - 1,
                'Enter phase must be the last action.',
            )
        elif kind in FLOW_V2 or kind in STATE_V2:
            raise ValueError('This action needs a version 2 story.')
        else:
            raise ValueError('Unknown trigger action.')


def _message(error):
    """A check's words: ValueError's own; anything else is a malformed part (a missing field, a wrong type)."""
    if isinstance(error, ValueError):
        return str(error)
    if isinstance(error, KeyError):
        return f'Missing field {error}.'
    return f'Malformed: {type(error).__name__} {error}'[:300]


def trigger_problems(t, phase, ids):
    """Every problem of one trigger, each with where in the trigger it is (doc 85, Phase 3: the Problems list, where
    `validate` stops at the first): its head (event, filters, firing rule), its conditions (`.if`), each action
    (`.actions[i]`; in a version 2 story nested ones too, `.actions[2].then[0]`). `ids` are the other trigger ids of the
    phase. A list of (subpath, words); empty when it is sound."""
    errors = (ValueError, KeyError, TypeError, AttributeError, IndexError)
    v2 = version(phase) == 2
    try:
        _check_trigger(t, phase, set(ids), {})
        return []
    except errors as error:
        whole = _message(error)
    most = ACTIONS_V2 if v2 else 12
    if not isinstance(t, dict) or not isinstance(t.get('actions'), list) or not 1 <= len(t['actions']) <= most:
        return [('', whole)]
    # The head alone, with one harmless action in place of its own; then its conditions; then each action alone.
    head = {k: v for k, v in t.items() if k != 'if'}
    head['actions'] = [{'do': 'stop'} if v2 else {'do': 'set', 'key': 'probe', 'scope': 'story', 'value': 0}]
    try:
        _check_trigger(head, phase, set(ids), {})
    except errors as error:
        return [('', _message(error))]
    out = []
    if 'if' in t:
        try:
            _check_trigger({**head, 'if': t['if']}, phase, set(ids), {})
        except errors as error:
            out.append(('.if', _message(error)))
    if v2:
        found = []
        try:
            _check_trigger({**head, 'actions': t['actions']}, phase, set(ids), {}, {}, found)
        except errors as error:
            found.append(('', _message(error)))
        out += found
        return out or [('', whole)]
    for index, action in enumerate(t['actions']):
        try:
            _check_trigger({**head, 'actions': [action]}, phase, set(ids), {})
            if isinstance(action, dict) and action.get('do') == 'enter' and index != len(t['actions']) - 1:
                out.append((f'.actions[{index}]', 'Enter phase must be the last action.'))
        except errors as error:
            out.append((f'.actions[{index}]', _message(error)))
    return out or [('', whole)]


# --------------------------------------------------------------------------------------------- version 2: running a program

def fnv(text):
    """FNV-1a, 64 bits, over UTF-8: the seeded rolls of chance, random and pick (the native twin has the same)."""
    h = 0xcbf29ce484222325
    for b in text.encode('utf-8'):
        h = ((h ^ b) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h


def draw(state, n):
    """One seeded roll in [0, n): a rehearsal's pinned roll (a number 0-99) first, else the hash of the run's seed and the
    number of rolls so far. Replays and rehearsals give the same answers."""
    k = state.get('draws', 0)
    state['draws'] = k + 1
    rolls = state.get('rolls')
    if rolls:
        r = rolls.pop(0)
        if not rolls:
            state.pop('rolls')
        return r * n // 100
    return fnv(f'{state.get("seed", "")}:{k}') % n


def party_of(state, wolf):
    """A wolf's party, as the run knows it (a rehearsal's `parties`; until the game tells the runner, a party of one)."""
    return state.get('parties', {}).get(wolf, wolf)


def var_store(state, scope, wolf, write=False):
    """The store a variable lives in: one for the story or the world, one per wolf or party. None: a wolf not in the
    story (nothing of theirs is read or written)."""
    if scope in PERSONAL:
        if wolf not in state['cohort']:
            return None
        owner = wolf if scope == 'player' else party_of(state, wolf)
        if write:
            return state.setdefault('counters', {}).setdefault(scope, {}).setdefault(owner, {})
        return state.get('counters', {}).get(scope, {}).get(owner, {})
    if write:
        return state.setdefault('counters', {}).setdefault(scope, {})
    return state.get('counters', {}).get(scope, {})


MISSING = object()


class Ctx:
    """One firing as it runs: the trigger, the event, the acting wolf, the loop's value, and where notes go."""

    def __init__(self, phase, state, event, actor, day, elapsed, ready, emit, rule=None, entry=None, firings=None):
        self.phase, self.state, self.event, self.actor = phase, state, event, actor
        self.day, self.elapsed, self.ready, self.emit = day, elapsed, ready, emit
        self.rule, self.entry, self.firings = rule, entry, firings
        self.each, self.target, self.depth = None, None, 0
        self.vars = phase.get('_vars', {}) if phase else {}

    def wolf(self, of):
        return self.each if of == '$each' else self.actor

    def now(self, clock):
        return self.elapsed if clock == 'real' else self.day

    def note(self, text):
        if self.entry is not None:
            self.entry.setdefault('notes', []).append(text)


def _field(event, name):
    value = event.get(name)
    if value is None:
        return 0 if name in AMOUNTS else ''
    return value


def value_of(o, x):
    """An operand's value now, or MISSING (a value of a wolf not in the story, a timer not started, no loop)."""
    if not isinstance(o, dict):
        return o
    if 'var' in o or 'count' in o or 'since' in o:
        name = o.get('var') or o.get('count') or o.get('since')
        d = x.vars[name]
        store = var_store(x.state, d['scope'], x.wolf(o.get('of')))
        if store is None:
            return MISSING
        value = store.get(name, initial(d))
        if 'count' in o:
            return len(value)
        if 'since' in o:
            return MISSING if value is None or x.day is None else x.now(d.get('clock', 'game')) - value
        return value
    if 'field' in o:
        return _field(x.event, o['field'])
    if 'each' in o:
        return MISSING if x.each is None else x.each
    key = next(k for k in ARITH if k in o)
    a, b = (value_of(v, x) for v in o[key])
    if a is MISSING or b is MISSING:
        return MISSING
    return a + b if key == 'plus' else a - b if key == 'minus' else min(a, b) if key == 'min' else max(a, b)


def test_v2(c, x):
    """A version 2 condition: True, False, or None (unknown: no clock to count days by)."""
    kind, state = c['test'], x.state
    if kind == 'compare':
        left, right = value_of(c['left'], x), value_of(c['right'], x)
        return left is not MISSING and right is not MISSING and COMPARE[c['op']](left, right)
    if kind == 'switch':
        value = value_of({'var': c['var'], **({'of': c['of']} if 'of' in c else {})}, x)
        return value is not MISSING and value is c['is']
    if kind == 'in list':
        values = value_of({'var': c['list'], **({'of': c['of']} if 'of' in c else {})}, x)
        value = value_of(c['value'], x)
        return values is not MISSING and value is not MISSING and value in values
    if kind == 'phase is':
        return state['phase'] == c['phase']
    if kind == 'visit is':
        return COMPARE[c['op']](state['visits'], c['value'])
    if kind == 'timer':
        key = c['signal'] + '/' + x.actor if c.get('owner') == 'player' else c['signal']
        timer = state.get('timers', {}).get(key)
        if timer is None:
            return False
        if 'op' not in c:
            return True
        if x.day is None:
            return None
        return COMPARE[c['op']](timer['at'] - x.now(timer.get('clock', 'game')), c['days'])
    if kind == 'fired':
        bucket = state.get('counts', {}).get(c.get('in', 'visit'), {})
        return COMPARE[c['op']](bucket.get(trigger_key(x.phase, c['trigger']), 0), c['value'])
    if kind == 'chance':
        return draw(state, 100) < c['percent']
    if kind in FACTS_V2:
        return fact_test(c, x)[0]
    if kind in QUEST_V2:
        return quest_test(c, x)[0]
    if kind == 'days since':
        if x.day is None:
            return None
        if c['from'] == 'var':
            since = value_of({'since': c['var'], **({'of': c['of']} if 'of' in c else {})}, x)
        else:
            since = x.day - (state['entered'] if c['from'] == 'phase' else state.get('began', state['entered']))
        return since is not MISSING and COMPARE[c['op']](since, c['days'])
    raise ValueError('Unknown trigger condition.')


# ------------------------------------------------------------------------------------- Phase 8: the world's facts, quests

def facts_of(event):
    """The world's facts an event carries (game.story_facts as of it), or None."""
    f = event.get('facts') if isinstance(event, dict) else None
    return f if isinstance(f, dict) else None


def without_facts(event):
    """An event as kept (in a wait): without the facts it carried, which are read again when it resumes."""
    return {k: v for k, v in event.items() if k != 'facts'} if 'facts' in event else event


def _who(x, v):
    return (x.each or '') if v == '$each' else x.actor if v == '$actor' else v


def _inside(r, at):
    cx, cy = at.get('x'), at.get('y')
    return (type(cx) is int and type(cy) is int and at.get('cell') == r['cell'] and r['x1'] <= cx <= r['x2']
            and r['y1'] <= cy <= r['y2'])


def _num(v):
    return type(v) in (int, float) and type(v) is not bool


def fact_test(c, x):
    """A condition over the world's facts: (verdict True/False/None, the value behind it). The twin of Dispatch2::fact in
    Core/RatwStoryTriggers.cpp."""
    kind = c['test']
    f = facts_of(x.event)
    if f is None:
        return None, None
    wolves = f.get('wolves') if isinstance(f.get('wolves'), dict) else {}
    world = f.get('world') if isinstance(f.get('world'), dict) else {}
    if kind == 'hour between':
        h = world.get('hour')
        if not _num(h):
            return None, None
        a, b = c['from'], c['to']
        return (a <= h <= b) if a <= b else (h >= a or h <= b), h
    if kind == 'calendar is':
        v = world.get(c['what'])
        return (None, None) if not isinstance(v, str) else (v == c['value'], v)
    if kind == 'relation is':
        rel = f.get('relations') if isinstance(f.get('relations'), dict) else {}
        v = rel.get(c['faction'] + '>' + c['other'])
        if v not in STANCES:
            return None, None
        a, b = STANCES.index(v), STANCES.index(c['stance'])
        return COMPARE[c['op']](a, b), v
    if kind == 'count in region':
        r = x.phase.get('_regions', {}).get(c['region'])
        if r is None:
            return None, None
        cohort = set(x.state.get('cohort', ()))
        n = sum(1 for wid, w in wolves.items() if wid in cohort and isinstance(w, dict) and _inside(r, w))
        return COMPARE[c['op']](n, c['value']), n
    if kind.startswith('resident'):
        res = f.get('residents') if isinstance(f.get('residents'), dict) else {}
        r = res.get(c['npc'])
        if not isinstance(r, dict):
            return None, None
        alive = r.get('alive') is True
        if kind == 'resident is':
            state = c['state']
            v = alive if state == 'alive' else not alive if state == 'dead' else alive and (r.get('indoors') is True) == (state == 'indoors')
            return v, 'alive' if alive else 'dead'
        if kind == 'resident in place':
            return r.get('cell') == c['place'], r.get('cell')
        if kind == 'resident in region':
            area = x.phase.get('_regions', {}).get(c['region'])
            return (None, None) if area is None else (_inside(area, r), {k: r.get(k) for k in ('cell', 'x', 'y')})
        v = r.get(c['what'])
        v = v if isinstance(v, str) else ''
        return v == c['value'], v
    if kind == 'weather is':
        weather = f.get('weather') if isinstance(f.get('weather'), dict) else {}
        where = c.get('where', '$actor')
        if where == '$actor':
            w = wolves.get(x.actor)
            where = w.get('cell') if isinstance(w, dict) else None
        v = weather.get(where) if isinstance(where, str) else None
        return (None, None) if not isinstance(v, str) else (v == c['weather'], v)
    wolf = _who(x, c.get('who', '$actor'))
    w = wolves.get(wolf)
    if kind == 'wolf here':
        return isinstance(w, dict), isinstance(w, dict)
    if not isinstance(w, dict):
        return None, None
    if kind == 'wolf in region':
        r = x.phase.get('_regions', {}).get(c['region'])
        return (None, None) if r is None else (_inside(r, w), {k: w.get(k) for k in ('cell', 'x', 'y')})
    if kind == 'wolf in place':
        return w.get('cell') == c['place'], w.get('cell')
    if kind == 'wolf in town':
        return w.get('town') == c['town'], w.get('town')
    if kind == 'wolf indoors':
        v = w.get('indoors') is True
        return v == c['is'], v
    if kind == 'wolf is':
        v = w.get(c['state']) is True
        return v == c['is'], v
    if kind == 'in party with':
        o = wolves.get(_who(x, c['other']))
        party = w.get('party')
        v = isinstance(party, str) and party != '' and isinstance(o, dict) and o.get('party') == party
        return v, party if isinstance(party, str) else ''
    if kind == 'party size':
        n = w.get('partySize', 1)
        return (None, None) if not _num(n) else (COMPARE[c['op']](n, c['value']), n)
    if kind == 'carries':
        held = w.get('carries') if isinstance(w.get('carries'), dict) else {}
        n = held.get(c['item'], 0)
        return (None, None) if not _num(n) else (COMPARE[c['op']](n, c['value']), n)
    if kind == 'coins':
        n = w.get('coins')
        return (None, None) if not _num(n) else (COMPARE[c['op']](n, c['value']), n)
    if kind == 'standing':
        st = w.get('standing') if isinstance(w.get('standing'), dict) else {}
        n = st.get(c['faction'])
        return (None, None) if not _num(n) else (COMPARE[c['op']](n, c['value']), n)
    if kind == 'reputation':
        rep = w.get('reputation') if isinstance(w.get('reputation'), dict) else {}
        n = rep.get(c['town'], 0)
        return (None, None) if not _num(n) else (COMPARE[c['op']](n, c['value']), n)
    raise ValueError('Unknown trigger condition.')


def _quest_copy(q, state, wolf):
    """The copy a wolf takes part in (story_quests.copy_of, without importing it: the twin reads the same)."""
    copies = state.get('quests', {}).get(q['id'], {})
    scope = q.get('scope', 'player')
    if scope == 'player':
        return copies.get(wolf)
    if scope == 'story':
        return copies.get('story') if wolf in state['cohort'] else None
    best = None
    for c in copies.values():
        if wolf in c.get('members', ()):
            if c['status'] in ('active', 'ready') and wolf not in c.get('left', ()):
                return c
            if best is None or c.get('began', 0) > best.get('began', 0):
                best = c
    return best


def quest_test(c, x):
    """`quest is` and `objective is` (doc 85, 3.4): a quest's or an objective's state for a wolf or the story's copy."""
    q = x.phase.get('_questIndex', {}).get(c['quest'])
    if q is None:
        return None, None
    of = c.get('of', '$actor')
    if of == 'story':
        copy = x.state.get('quests', {}).get(q['id'], {}).get('story')
        wolf = None
    else:
        wolf = _who(x, of)
        copy = _quest_copy(q, x.state, wolf)
    if c['test'] == 'quest is':
        if copy is not None:
            status = copy['status']
        elif wolf is not None and wolf in x.state.get('questOffers', {}).get(q['id'], ()):
            status = 'offered'
        else:
            status = 'none'
        return status == c['status'], status
    o = next(o for o in q['objectives'] if o['id'] == c['objective'])
    rec = (copy or {}).get('objectives', {}).get(o['id'], {})
    status = ('done' if rec.get('done') else 'failed' if rec.get('failed')
              else 'hidden' if o.get('hidden') and not rec.get('shown') else 'to do')
    return status == c['status'], status


def _run(actions, x, base, start=0):
    """Run actions[start:]. None to go on; or 'stop', 'wait', 'enter' (x.target), 'finish' to end the trigger here."""
    for i in range(start, len(actions)):
        r = _act(actions[i], x, (*base, i))
        if r:
            return r
    return None


def _resume(actions, x, base, address):
    """Run from an address (the action after a wait), then the rest of every list it is inside."""
    i = address[0]
    if len(address) > 1:
        name = address[1]
        r = _resume(branch(actions[i], name), x, (*base, i, name), address[2:])
        if r:
            return r
        i += 1
    return _run(actions, x, base, i)


def _wolves(a, x):
    state = x.state
    if a['over'] == 'participants':
        return list(state['cohort'])
    if a['over'] == 'party':
        if x.actor not in state['cohort']:
            return []
        party = party_of(state, x.actor)
        return [w for w in state['cohort'] if party_of(state, w) == party]
    values = list(value_of({'var': a['list']}, x))
    return [w for w in values if w in state['cohort']] if x.vars[a['list']]['type'] == 'wolves' else values


def _number(value):
    return max(-LIMIT, min(LIMIT, value))


def _set_var(a, x, fn):
    """Change one variable by fn(current) (MISSING: leave it)."""
    name = a['var']
    d = x.vars[name]
    store = var_store(x.state, d['scope'], x.wolf(a.get('of')), True)
    if store is None:
        return
    value = fn(store.get(name, initial(d)))
    if value is not MISSING:
        store[name] = value


def _key_of(x, name, phase_id=None):
    return phase_id + '/' + name if phase_id else trigger_key(x.phase, name)


def count_firing(phase, state, trigger_id):
    """Version 2: how often each trigger fired, this visit and this run (the `fired` condition)."""
    key = trigger_key(phase, trigger_id)
    counts = state.setdefault('counts', {})
    for bucket in ('visit', 'run'):
        counts.setdefault(bucket, {})[key] = counts.get(bucket, {}).get(key, 0) + 1


def _act(a, x, address):
    op, state = a['do'], x.state
    if op == 'enter':
        x.target = a['phase']
        return 'enter'
    if op == 'effect':
        effect = dict(a['effect'])
        for k, v in effect.items():
            if v == '$actor' and k == 'player':
                effect[k] = x.actor
            elif v == '$each' and x.each is not None:
                effect[k] = x.each
        state.setdefault('triggerEffects', []).append(effect)
    elif op in ('schedule', 'cancel'):
        name = action_signal = a['signal']
        # A slash cannot appear in a signal ID. Shared keys stay compatible with older releases.
        key = name + '/' + x.actor if a.get('owner') == 'player' else name
        if op == 'cancel':
            state.get('timers', {}).pop(key, None)
            return None
        state.setdefault('timers', {})[key] = {
            'signal': action_signal,
            'at': (x.elapsed if a.get('clock') == 'real' else x.day) + a['days'],
            'clock': a.get('clock', 'game'),
            'actor': x.actor,
            'interval': a['days'] if a.get('repeat') else 0,
            'scope': a.get('scope', 'visit'),
        }
    elif op == 'emit':
        x.emit(a['signal'], x.event)
    elif op in ('set', 'add') and 'key' in a:
        if a['scope'] != 'player' or x.actor in state['cohort']:
            bucket = counter_store(state, a['scope'], x.actor, True)
            value = a['value']
            if isinstance(value, str):
                value = x.event.get(value, 0)
            if op == 'add':
                value += bucket.get(a['key'], 0)
            bucket[a['key']] = max(-LIMIT, min(LIMIT, value))
    elif op == 'if':
        if test(a['if'], x) is True:
            return _run(a['then'], x, (*address, 'then'))
        for k, b in enumerate(a.get('elif', ())):
            if test(b['if'], x) is True:
                return _run(b['then'], x, (*address, f'elif:{k}'))
        if 'else' in a:
            return _run(a['else'], x, (*address, 'else'))
    elif op == 'each':
        before = x.each
        for wolf in _wolves(a, x)[:EACH_MAX]:
            x.each = wolf
            r = _run(a['actions'], x, (*address, 'actions'))
            if r:
                x.each = before
                return r
        x.each = before
    elif op == 'random':
        total = sum(c['weight'] for c in a['choices'])
        roll = draw(state, total)
        for k, c in enumerate(a['choices']):
            if roll < c['weight']:
                return _run(c['actions'], x, (*address, f'choice:{k}'))
            roll -= c['weight']
    elif op == 'wait':
        clock = a.get('clock', 'game')
        state.setdefault('waiting', []).append({
            'trigger': x.rule['id'], 'scope': 'campaign' if x.rule['id'] in x.phase.get('_sharedIds', ()) else 'visit',
            'phase': x.phase['id'], 'at': x.now(clock) + a['days'], 'clock': clock, 'actor': x.actor, 'event': without_facts(x.event),
            'next': [*address[:-1], address[-1] + 1],
        })
        return 'wait'
    elif op == 'run':
        other = next((t for t in x.phase.get('triggers', ()) if t['id'] == a['trigger']), None)
        if other is None or x.depth >= RUN_DEPTH:
            return None
        if a.get('check'):
            if trigger_key(x.phase, other['id']) in set(state.get('disabled', ())):
                return None
            if 'if' in other and test(other['if'], x) is not True:
                return None
        entry = _firing(x.phase, x.event, x.actor, x.day, x.elapsed, other, via='run')
        if x.firings is not None:
            x.firings.append(entry)
        count_firing(x.phase, state, other['id'])
        inner = Ctx(x.phase, state, x.event, x.actor, x.day, x.elapsed, x.ready, x.emit, other, entry, x.firings)
        inner.depth = x.depth + 1
        r = _run(other['actions'], inner, ())
        if r in ('enter', 'finish'):
            x.target = inner.target
            return r
    elif op in ('enable', 'disable'):
        key = _key_of(x, a['trigger'], a.get('phase'))
        disabled = set(state.get('disabled', ()))
        (disabled.discard if op == 'enable' else disabled.add)(key)
        if disabled:
            state['disabled'] = sorted(disabled)
        else:
            state.pop('disabled', None)
    elif op == 'stop':
        return 'stop'
    elif op == 'note':
        x.note(a['text'])
    elif op == 'finish':
        who = a.get('who', 'everyone')
        record = {'outcome': a['outcome'], 'text': a['text'], 'day': x.day}
        if who == 'everyone':
            state['outcome'] = record
            return 'finish'
        wolf = x.wolf(who)
        if wolf in state['cohort']:
            state.setdefault('outcomes', {})[wolf] = record
    elif op in ('set', 'add', 'subtract', 'multiply'):
        value = value_of(a['value'], x)
        if value is MISSING:
            return None
        if op == 'set':
            _set_var(a, x, lambda _: _number(value) if x.vars[a['var']]['type'] == 'number' else value)
        else:
            fn = {'add': lambda v: v + value, 'subtract': lambda v: v - value, 'multiply': lambda v: v * value}[op]
            _set_var(a, x, lambda v: _number(fn(v)))
    elif op == 'clamp':
        _set_var(a, x, lambda v: max(a['min'], min(a['max'], v)))
    elif op in ('turn on', 'turn off', 'toggle'):
        _set_var(a, x, lambda v: True if op == 'turn on' else False if op == 'turn off' else not v)
    elif op == 'remember':
        value = (x.each if x.each is not None else MISSING) if a['from'] == 'each' else _field(x.event, a['from'])
        _set_var(a, x, lambda _: value if value is MISSING else str(value)[:TEXT_MAX])
    elif op in ('list add', 'list remove', 'list clear'):
        value = value_of(a['value'], x) if op != 'list clear' else None
        if value is MISSING:
            return None

        def change(values):
            values = list(values)
            if op == 'list clear':
                return []
            if op == 'list add':
                if value not in values and len(values) < LIST_MAX:
                    values.append(value)
            elif value in values:
                values.remove(value)
            return values

        _set_var(a, x, change)
    elif op == 'pick':
        values = value_of({'var': a['list'], **({'of': a['of']} if 'of' in a else {})}, x)
        if values is MISSING or not values:
            return None
        chosen = values[draw(state, len(values))]
        _set_var(a, x, lambda _: chosen)
    elif op in ('start timer', 'stop timer'):
        if op == 'stop timer':
            d = x.vars[a['var']]
            store = var_store(state, d['scope'], x.wolf(a.get('of')), True)
            if store is not None:
                store.pop(a['var'], None)
            return None
        clock = x.vars[a['var']].get('clock', 'game')
        _set_var(a, x, lambda _: x.now(clock))
    elif op == 'shift':
        key = a['signal'] + '/' + x.actor if a.get('owner') == 'player' else a['signal']
        timer = state.get('timers', {}).get(key)
        if timer is not None:
            timer['at'] = timer['at'] + a['days']
    return None


def test(c, x):
    """A condition inside a firing (version 2 aware): True, False or None."""
    kind = c['test']
    if kind in ('all', 'any'):
        decisive = kind == 'any'
        unknown = False
        for child in c['conditions']:
            value = test(child, x)
            if value is decisive:
                return decisive
            unknown = unknown or value is None
        return None if unknown else not decisive
    if kind == 'not':
        value = test(c['condition'], x)
        return None if value is None else not value
    if kind in CONDITIONS_V2:
        return test_v2(c, x)
    return satisfied(c, x.state, x.event, x.ready, x.phase.get('_regions'))


def judge(c, state, event, ready, regions=None, known=None, phase=None, day=None, elapsed=0):
    """The recording form of `satisfied` (doc 85, 5.5; the Campaigns console's "why", Python only: the live path needs no
    tree): the same verdict for every node, true, false or None (unknown), with the values behind it. `known` names the
    event fields that are known; None means all of them (a real event). A test of a field that is not known, or of a
    place when the event has none, is unknown, with `needs` saying what it waits for. A version 2 condition needs the
    `phase`; a chance is unknown (it is rolled when it fires)."""
    kind = c['test']
    if kind in ('all', 'any'):
        children = [judge(child, state, event, ready, regions, known, phase, day, elapsed) for child in c['conditions']]
        decisive = kind == 'any'
        verdicts = [child['verdict'] for child in children]
        verdict = decisive if decisive in verdicts else None if None in verdicts else not decisive
        return {'test': kind, 'verdict': verdict, 'conditions': children}
    if kind == 'not':
        child = judge(c['condition'], state, event, ready, regions, known, phase, day, elapsed)
        return {'test': 'not', 'verdict': None if child['verdict'] is None else not child['verdict'], 'condition': child}
    if kind in CONDITIONS_V2:
        node = dict(c)
        if kind == 'chance':
            return {**node, 'verdict': None, 'needs': 'roll'}
        if kind in FACTS_V2 or kind in QUEST_V2:
            # (Doc 85, Phase 8: the world's facts as of the event, and the story's quests; the fact read is shown.)
            wolfish = c.get('who', '$actor') == '$actor' or c.get('of', '$actor') == '$actor' and kind in QUEST_V2
            if kind in FACTS_V2 and facts_of(event) is None:
                return {**node, 'verdict': None, 'needs': 'facts', **({'expected': c['value']} if 'value' in c else {})}
            if wolfish and known is not None and 'actor' not in known and kind not in (
                    'hour between', 'calendar is', 'relation is', 'count in region') and not kind.startswith('resident'):
                return {**node, 'verdict': None, 'needs': 'wolf', **({'expected': c['value']} if 'value' in c else {})}
            x = Ctx(phase or {}, state, event, event.get('actor', ''), day, elapsed, ready, None)
            verdict, value = (fact_test if kind in FACTS_V2 else quest_test)(c, x)
            return {**node, 'verdict': verdict, 'fact': value, **({'needs': 'facts'} if verdict is None else {}),
                    **({'expected': c['value']} if 'value' in c else {}),
                    **({'wolf': x.actor} if wolfish and x.actor and kind not in ('hour between', 'calendar is', 'relation is') else {})}
        fields = {o['field'] for side in ('left', 'right', 'value') if side in c for o in operands_in(c[side]) if 'field' in o}
        if known is not None and not fields <= known:
            return {**node, 'verdict': None, 'needs': 'event'}
        personal = _personal(c, phase)
        if personal and known is not None and 'actor' not in known:
            return {**node, 'verdict': None, 'needs': 'wolf'}
        x = Ctx(phase or {}, state, event, event.get('actor', ''), day, elapsed, ready, None)
        verdict = test_v2(c, x)
        values = {side: _shown(value_of(c[side], x)) for side in ('left', 'right', 'value') if side in c}
        if kind in ('switch',):
            values['value'] = _shown(value_of({'var': c['var'], **({'of': c['of']} if 'of' in c else {})}, x))
        return {**node, 'verdict': verdict, **({'values': values} if values else {}),
                **({'wolf': x.actor} if personal and x.actor else {})}
    if kind == 'region':
        r = (regions or {}).get(c['region'], {})
        node = {'test': 'region', 'region': c['region'], 'bounds': {k: r.get(k) for k in ('cell', 'x1', 'y1', 'x2', 'y2')}}
        if known is not None and not {'cell', 'x', 'y'} <= known:
            return {**node, 'verdict': None, 'needs': 'place'}
        verdict = satisfied(c, state, event, ready, regions)
        return {**node, 'verdict': verdict, 'value': {k: event.get(k) for k in ('cell', 'x', 'y')},
                **({'needs': 'place'} if verdict is None else {})}
    if kind == 'mission':
        return {'test': 'mission', 'id': c['id'], 'verdict': c['id'] in ready()}
    node = {k: c[k] for k in ('test', 'op', 'value')}
    if kind == 'counter':
        node.update(key=c['key'], scope=c['scope'], expected=c['value'])
        actor = event.get('actor')
        if c['scope'] == 'player' and known is not None and 'actor' not in known:
            return {**node, 'value': None, 'verdict': None, 'needs': 'wolf'}
        if c['scope'] == 'player' and actor not in state['cohort']:
            return {**node, 'value': None, 'verdict': False, 'why': 'not in the story', **({'wolf': actor} if actor else {})}
        value = counter_store(state, c['scope'], actor).get(c['key'], 0)
        return {**node, 'value': value, 'verdict': COMPARE[c['op']](value, c['value']),
                **({'wolf': actor} if c['scope'] == 'player' else {})}
    node.update(field=c['field'], expected=c['value'])
    if known is not None and c['field'] not in known:
        return {**node, 'value': None, 'verdict': None, 'needs': 'event'}
    value = event.get(c['field'], 0 if c['field'] in ('coins', 'quantity') else '')
    return {**node, 'value': value, 'verdict': COMPARE[c['op']](value, c['value'])}


def _shown(v):
    return None if v is MISSING else v


def _personal(c, phase):
    """Whether a version 2 condition reads a variable of the acting wolf (or their party), or their timer."""
    decls = (phase or {}).get('_vars', {})
    names = [c.get('var'), c.get('list')] + [o.get('var') or o.get('count') or o.get('since')
                                             for side in ('left', 'right', 'value') if side in c for o in operands_in(c[side])]
    return any(decls.get(n, {}).get('scope') in PERSONAL for n in names if n) or (c['test'] == 'timer' and c.get('owner') == 'player')


def uses_wolf(c, phase=None):
    """Whether a condition tree depends on which wolf acts (a player counter, or a version 2 personal variable)."""
    return any(x.get('test') == 'counter' and x.get('scope') == 'player' or (x.get('test') in CONDITIONS_V2 and _personal(x, phase))
               for x in conditions_in(c))


def trigger_key(phase, trigger_id):
    """How state.disabled names a trigger (doc 85, 3.5 and Phase 2): a campaign trigger by its id, a phase's own trigger
    as phase/id (it stays off on a later visit of that phase)."""
    return trigger_id if trigger_id in phase.get('_sharedIds', ()) else phase['id'] + '/' + trigger_id


def initially_off(program_phases, shared):
    """The keys of the triggers a version 2 story starts with turned off ("enabled": false)."""
    keys = {t['id'] for t in shared if t.get('enabled') is False}
    keys |= {ph['id'] + '/' + t['id'] for ph in program_phases for t in ph.get('triggers', ()) if t.get('enabled') is False}
    return sorted(keys)


def counter_store(state, scope, actor, write=False):
    if scope == 'player' and actor not in state['cohort']:
        return {}
    if write:
        store = state.setdefault('counters', {}).setdefault(scope, {})
        return store.setdefault(actor, {}) if scope == 'player' else store
    store = state.get('counters', {}).get(scope, {})
    return store.get(actor, {}) if scope == 'player' else store


def satisfied(c, state, event, ready, regions=None, phase=None, day=None, elapsed=0):
    """A missing historical position is unknown, including when an author negates the test. (A version 2 condition is
    judged with the `phase` and `day` it is asked in.)"""
    kind = c['test']
    if kind in CONDITIONS_V2:
        return test_v2(c, Ctx(phase or {}, state, event, event.get('actor', ''), day, elapsed, ready, None))
    if kind in ('all', 'any'):
        decisive = kind == 'any'
        unknown = False
        for child in c['conditions']:
            value = satisfied(child, state, event, ready, regions, phase, day, elapsed)
            if value is decisive:
                return decisive
            unknown = unknown or value is None
        return None if unknown else not decisive
    if kind == 'not':
        value = satisfied(c['condition'], state, event, ready, regions, phase, day, elapsed)
        return None if value is None else not value
    if kind == 'region':
        x, y, cell = event.get('x'), event.get('y'), event.get('cell')
        if type(x) is not int or type(y) is not int or not (0 <= x <= 4095 and 0 <= y <= 4095) or not cell:
            return None
        r = regions[c['region']]
        return cell == r['cell'] and r['x1'] <= x <= r['x2'] and r['y1'] <= y <= r['y2']
    if kind == 'mission':
        return c['id'] in ready()
    if kind == 'counter':
        if c['scope'] == 'player' and event.get('actor') not in state['cohort']:
            return False
        value = counter_store(state, c['scope'], event.get('actor')).get(c['key'], 0)
    else:
        value = event.get(c['field'], 0 if c['field'] in ('coins', 'quantity') else '')
    return COMPARE[c['op']](value, c['value'])


def _firing(phase, event, actor, day, elapsed, t, via=None):
    entry = {
        'phase': phase['id'],
        'day': day,
        'actor': actor,
        'elapsedDays': elapsed,
        'actions': [a['effect']['kind'] if a['do'] == 'effect' else a['do'] for a in t['actions']],
        'trigger': t['id'],
        'scope': 'campaign' if t['id'] in phase.get('_sharedIds', ()) else 'phase',
        'event': event.get('id'),
        'on': event['kind'],
        'signal': event.get('signal'),
    }
    if via:
        entry['via'] = via
    return entry


def fire(phase, state, event, day, elapsed, ready, matches, emit, trace):
    native = story_native.interpreter()
    if native:
        if version(phase) == 2:
            return story_native.fire_v2(native, phase, state, event, day, elapsed, ready, emit, trace)
        return story_native.fire(native, phase, state, event, day, elapsed, ready, emit, trace)
    actor = event.get('actor', '')
    disabled = set(state.get('disabled', ()))
    v2 = version(phase) == 2
    for t in phase.get('triggers', ()):
        if disabled and trigger_key(phase, t['id']) in disabled:
            continue
        kind = event['kind']
        if t['on'] == 'gift' and kind == 'gift again':
            kind = 'gift'
        if t['on'] != kind or event.get('signal') != t.get('signal') or not matches(t, {**event, 'kind': kind}):
            continue
        repeat = t.get('once', 'each')
        if (t.get('audience') == 'participant' or repeat in ('player', 'member')) and actor not in state['cohort']:
            continue
        if kind == 'clock' and day < state['entered'] + t['afterDays']:
            continue
        owner = actor if repeat in ('player', 'member') else '*'
        bucket = 'campaignFired' if repeat in ('campaign', 'member') else 'fired'
        token = t['id'] if bucket == 'fired' or t['id'] in phase.get('_sharedIds', ()) else phase['id'] + '/' + t['id']
        if repeat != 'each' and owner in state.get(bucket, {}).get(token, []):
            continue
        if v2:
            x = Ctx(phase, state, {**event, 'kind': kind}, actor, day, elapsed, ready, emit, t, None, trace)
            if 'if' in t and test(t['if'], x) is not True:
                continue
        elif 'if' in t and not satisfied(t['if'], state, event, ready, phase.get('_regions')):
            continue
        if repeat != 'each':
            state.setdefault(bucket, {}).setdefault(token, []).append(owner)
        entry = None
        if trace is not None:
            entry = _firing(phase, {**event, 'kind': kind}, actor, day, elapsed, t)
            trace.append(entry)
        if v2:
            count_firing(phase, state, t['id'])
            x.entry, x.event = entry, event
            r = _run(t['actions'], x, ())
            if r == 'enter':
                return x.target
            if r == 'finish':
                return None
            continue
        target = perform(t, state, event, actor, day, elapsed, emit)
        if target:
            return target
    return None


def resume(phase, state, wait, day, elapsed, ready, emit, trace):
    """A waiting trigger's remaining actions (version 2's wait): the target phase of an `enter`, or None."""
    native = story_native.interpreter()
    if native:
        return story_native.fire_v2(native, phase, state, wait['event'], day, elapsed, ready, emit, trace, wait)
    t = next((t for t in phase.get('triggers', ()) if t['id'] == wait['trigger']), None)
    if t is None:
        return None
    entry = _firing(phase, wait['event'], wait['actor'], day, elapsed, t, via='wait')
    if trace is not None:
        trace.append(entry)
    x = Ctx(phase, state, wait['event'], wait['actor'], day, elapsed, ready, emit, t, entry, trace)
    r = _resume(t['actions'], x, (), wait['next'])
    return x.target if r == 'enter' else None


def perform(t, state, event, actor, day, elapsed, emit, phase=None, entry=None, ready=None, trace=None):
    """A trigger's actions, in order, for the acting wolf: the target phase of an `enter`, or None. (Also the Campaigns
    console's "fire now", doc 85 Phase 2; a version 2 trigger needs its `phase`.)"""
    if phase is not None and version(phase) == 2:
        x = Ctx(phase, state, event, actor, day, elapsed, ready or (lambda: set()), emit, t, entry, trace)
        r = _run(t['actions'], x, ())
        return x.target if r == 'enter' else None
    for action in t['actions']:
        op = action['do']
        if op == 'enter':
            return action['phase']
        if op == 'effect':
            effect = dict(action['effect'])
            if effect.get('player') == '$actor':
                effect['player'] = actor
            state.setdefault('triggerEffects', []).append(effect)
        elif op in ('schedule', 'cancel'):
            name = action['signal']
            # A slash cannot appear in a signal ID. Shared keys stay compatible with older releases.
            key = name + '/' + actor if action.get('owner') == 'player' else name
            if op == 'cancel':
                state.get('timers', {}).pop(key, None)
                continue
            state.setdefault('timers', {})[key] = {
                'signal': name,
                'at': (elapsed if action.get('clock') == 'real' else day) + action['days'],
                'clock': action.get('clock', 'game'),
                'actor': actor,
                'interval': action['days'] if action.get('repeat') else 0,
                'scope': action.get('scope', 'visit'),
            }
        elif op == 'emit':
            emit(action['signal'], event)
        elif action['scope'] != 'player' or actor in state['cohort']:
            bucket = counter_store(state, action['scope'], actor, True)
            value = action['value']
            if isinstance(value, str):
                value = event.get(value, 0)
            if op == 'add':
                value += bucket.get(action['key'], 0)
            bucket[action['key']] = max(-LIMIT, min(LIMIT, value))
    return None
