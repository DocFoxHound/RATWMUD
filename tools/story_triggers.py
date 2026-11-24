"""Event–condition–action rules. Signals retain the originating world fact and last for one dispatch.

Version 2 of the language (Docs/Design/85-story-triggers-and-campaign-control.md, 3.2, 3.4, 3.5 and Phase 5): declared,
typed variables in four scopes, conditions over the story's own state, and flow inside a trigger's actions (if / else if /
else, wait, for each, random, run, enable, disable, stop, note, finish). A version 1 program runs exactly as before: every
version 2 path is behind `phase['_v'] == 2`. Core/RatwStoryTriggers.cpp is this file's twin (tools/test_story_native.py)."""

import json
import math
import re
import operator
from pathlib import Path

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
RANK_MAX = 500           # Wolves Rank puts in a list (doc 86, C3a).
TEXT_MAX = 200           # Characters of a text value.
ACTIONS_V2 = 24          # Actions of one trigger, counting those inside If, For each and Random.
NEST_V2 = 3              # If, For each and Random inside one another.
RUN_DEPTH = 8            # Run inside run, at most (cycles are refused when the story is checked).
REAL_WAIT_MIN = 5 / 86400
OUTCOMES = ('success', 'failure', 'abandoned')
ARITH = ('plus', 'minus', 'min', 'max', 'times', 'div')
TEXT_OPS = ('starts', 'in')   # (Doc 86, C12b: the field condition's begins with and one of.)
# Doc 86, Phase 1a (C2, R1): the story's audience counted, and what share of it took part.
AUDIENCES = ('all', 'active', 'online')
ACTIVE_DAYS = 18          # (A program's activeDays when it gives none: three real days at the normal speed.)
SHARES = ('percent', 'among')
WHERE_TESTS = ('compare', 'switch', 'in list', 'days since', 'quest is', 'objective is')
WHERE_DEPTH, WHERE_LEAVES = 3, 8
# Doc 85, Phase 8: conditions over the world's facts as of the event (game.story_facts, carried on the event as `facts`), and
# over the story's quests. A fact that is missing (a wolf away, no facts at all) is unknown, and unknown never fires.
FACTS_V2 = ('wolf here', 'wolf in region', 'wolf in place', 'wolf in town', 'wolf indoors', 'count in region', 'carries', 'coins',
            'standing', 'reputation', 'wolf is', 'wolf has', 'party size', 'in party with', 'resident is', 'resident in place',
            'resident in region', 'resident has', 'hour between', 'calendar is', 'weather is', 'relation is')
QUEST_V2 = ('quest is', 'objective is')
CONDITIONS_V2 = ('compare', 'switch', 'in list', 'phase is', 'visit is', 'timer', 'fired', 'chance', 'days since', *FACTS_V2, *QUEST_V2,
                 'participation')
STANCES = ('war', 'hostile', 'tense', 'neutral', 'friendly', 'allied')
WOLF_STATES = ('downed', 'hurt', 'wanted', 'held')
RESIDENT_STATES = ('alive', 'dead', 'indoors', 'out of doors')
COUNTED = ('wolves', 'residents')   # (Doc 86, J6: who a count in a region counts.)
RESIDENT_HAS = ('job', 'mood', 'mantle', 'town', 'post')   # (Doc 86, C13b: the post they hold.)
# Doc 86, J10: a wolf's Gift, its tier or its family (Data/Gifts/families.json, the playable ones).
GIFT_TIERS = ('normal', 'gifted', 'quickened')


def _playable_families():
    try:
        doc = json.loads((Path(__file__).resolve().parent.parent / 'Data' / 'Gifts' / 'families.json').read_text(encoding='utf-8'))
        return tuple(f['id'] for f in doc['families'] if not f.get('npcOnly'))
    except (OSError, ValueError, KeyError, TypeError):
        return ('fire', 'earth', 'water', 'wind', 'sound', 'blinker', 'gravity', 'seer')   # (The eight of doc 43.)


GIFT_FAMILIES = _playable_families()
CALENDAR = {'weekday': ('Dawnday', 'Hearthday', 'Stoneday', 'Riverday', 'Emberday', 'Marketday', 'Restday'),
            'season': ('spring', 'summer', 'autumn', 'winter'), 'period': ('dawn', 'day', 'dusk', 'night'),
            'moon': ('new', 'waxing', 'full', 'waning')}   # (Doc 86, J4: the moon in four.)
WEATHERS = ('clear', 'overcast', 'rain', 'storm', 'fog', 'snow', 'sandstorm')
QUEST_STATES = ('none', 'offered', 'active', 'ready', 'done', 'failed', 'abandoned', 'expired')
OBJECTIVE_STATES = ('done', 'failed', 'to do', 'hidden')
# (Each facts condition: its keys besides `test`, and the optional ones.)
FACT_SHAPES = {
    'wolf here': ((), ('who',)), 'wolf in region': (('region',), ('who',)), 'wolf in place': (('place',), ('who',)),
    'wolf in town': (('town',), ('who',)), 'wolf indoors': (('is',), ('who',)),
    'count in region': (('region', 'op', 'value'), ('who', 'list')),   # (Doc 86, J6: the residents of a list, too.)
    'carries': (('item', 'op', 'value'), ('who',)), 'coins': (('op', 'value'), ('who',)), 'standing': (('faction', 'op', 'value'), ('who',)),
    'reputation': (('town', 'op', 'value'), ('who',)), 'wolf is': (('state', 'is'), ('who',)),
    'wolf has': (('what', 'value'), ('who',)), 'party size': (('op', 'value'), ('who',)),
    'in party with': (('other',), ('who',)), 'resident is': (('npc', 'state'), ()), 'resident in place': (('npc', 'place'), ()),
    'resident in region': (('npc', 'region'), ()), 'resident has': (('npc', 'what', 'value'), ()), 'hour between': (('from', 'to'), ()),
    'calendar is': (('what', 'value'), ()), 'weather is': (('weather',), ('where',)), 'relation is': (('faction', 'other', 'op', 'stance'), ()),
}
FLOW_V2 = ('if', 'wait', 'each', 'random', 'run', 'enable', 'disable', 'stop', 'note', 'finish')
STATE_V2 = ('subtract', 'multiply', 'clamp', 'turn on', 'turn off', 'toggle', 'remember', 'list add', 'list remove',
            'list clear', 'pick', 'start timer', 'stop timer', 'shift', 'rank')
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


def purses_in(v):
    """Doc 86, M2: every purse a part of a program reads ({"purse": "<purse>"})."""
    if isinstance(v, list):
        for x in v:
            yield from purses_in(x)
    elif isinstance(v, dict):
        if len(v) == 1 and isinstance(v.get('purse'), str):
            yield v['purse']
            return
        for x in v.values():
            if isinstance(x, (dict, list)):
                yield from purses_in(x)


def uses_audience(v):
    """Doc 86, C2 and R1: whether rules (or any part of a program) count the audience: {"audience"}, {"percent"} or
    {"among"} values, or the condition participation. (A trigger's own `audience`, "participant", is not one.)"""
    if isinstance(v, list):
        return any(uses_audience(x) for x in v)
    if not isinstance(v, dict):
        return False
    if (len(v) == 1 and v.get('audience') in AUDIENCES) or v.get('test') == 'participation' or \
            any(isinstance(v.get(k), dict) for k in SHARES):
        return True
    return any(uses_audience(x) for x in v.values() if isinstance(x, (dict, list)))


def wheres_in(v):
    """Every R1 test for each wolf (a `where`) in rules or a part of a program."""
    if isinstance(v, list):
        for x in v:
            yield from wheres_in(x)
    elif isinstance(v, dict):
        if isinstance(v.get('where'), dict) and (v.get('test') == 'participation' or len(v) == 1 or 'participation' in v):
            yield v['where']
        for x in v.values():
            if isinstance(x, (dict, list)):
                yield from wheres_in(x)


def uses_online(v):
    """Whether a part of a program counts the wolves online now (C2): it then watches the world's wolves."""
    if isinstance(v, list):
        return any(uses_online(x) for x in v)
    if not isinstance(v, dict):
        return False
    if (len(v) == 1 and v.get('audience') == 'online') or (v.get('of') == 'online' and (
            v.get('test') == 'participation' or 'participation' in v or any(isinstance(v.get(k), dict) for k in SHARES))):
        return True
    return any(uses_online(x) for x in v.values() if isinstance(x, (dict, list)))


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
        if kind == 'field' and c['op'] in TEXT_OPS:
            # (Doc 86, C12b: begins with a text; one of up to 32 texts.)
            require(c['field'] not in ('coins', 'quantity'), 'Begins with and one of are for the event\'s text fields.')
            require(rules.match_value_ok({c['op']: c['value']}), 'Begins with: 1 to 120 characters; one of: 1 to 32 texts of up to 120.')
        elif kind == 'counter' or c['field'] in ('coins', 'quantity'):
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


def operand_type(o, phase, each=None, depth=0, share_ok=False):
    """What a value in a comparison or a Set is: number, switch or text; a malformed one raises. `share_ok`: a quest's reward
    with credit by share, where {"share": true} is the credited wolf's own amount (doc 86, C3b)."""
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
    if 'purse' in o:
        # (Doc 86, M2: the pennies a purse holds now, from the world's facts.)
        import story_quests
        shape(o, ('purse',))
        require(story_quests.check_purse(o['purse']), story_quests.PURSE_WORDS)
        return 'number'
    if 'share' in o:
        # (Doc 86, C3b: in a reward of a quest credited by share, the credited wolf's own amount.)
        shape(o, ('share',))
        require(o['share'] is True and share_ok,
                'A share ({"share": true}) is only in the rewards of a quest that rewards each wolf by their share (credit: share).')
        return 'number'
    if 'tally' in o:
        # (Doc 86, J11: a number added up, or wolves counted, over every wolf of the story or a list; no 500 cap.)
        shape(o, ('tally',))
        require(each is None, 'Counting over every wolf (tally) is not allowed inside For each, nor inside a test for each wolf.')
        require(depth <= 1, 'Counting over every wolf (tally) may be one level deep in arithmetic at most.')
        _check_tally(o['tally'], phase)
        return 'number'
    if 'audience' in o:
        # (Doc 86, C2: how many wolves the story has, how many are active, how many are online now.)
        shape(o, ('audience',))
        require(o['audience'] in AUDIENCES, 'Count the story\'s wolves: all, active or online.')
        return 'number'
    share = next((k for k in SHARES if k in o), None)
    if share is not None:
        # (Doc 86, R1: the share, or the number, of the active wolves who took part.)
        shape(o, (share,), ('of',))
        require(each is None, 'Counting who took part is not allowed inside For each, nor inside its own test.')
        require(depth <= 1, 'Counting who took part may be one level deep in arithmetic at most.')
        require(o.get('of', 'active') in AUDIENCES, 'Out of the active wolves, all the story\'s wolves, or those online.')
        _check_source(o[share], phase)
        return 'number'
    key = next((k for k in ARITH if k in o), None)
    require(key is not None, 'Unknown value.')
    shape(o, (key,))
    require(depth < 2 and isinstance(o[key], list) and len(o[key]) == 2, 'Arithmetic takes two values, two deep at most.')
    require(all(operand_type(x, phase, each, depth + 1, share_ok) == 'number' for x in o[key]), 'Arithmetic is on numbers.')
    return 'number'


def _check_tally(t, phase):
    """J11: {"sum": <a wolf's number>, "where": <a wolf's text or switch>, "is": <its value>, "over": "participants" |
    {"list": <a list of wolves>}}; every part optional, where and is together."""
    import story_program as rules

    require = rules.require
    require(isinstance(t, dict), 'A tally: what to add up (sum), which wolves (where and is), over whom.')
    rules.shape(t, (), ('sum', 'where', 'is', 'over'))
    require(('where' in t) == ('is' in t), 'A tally counts the wolves whose variable (where) is a value (is): name both, or neither.')
    if 'sum' in t:
        d = _decl(phase, t['sum'], ('number',), 'number')
        require(d['scope'] == 'player' and 'mirror' not in d, f'"{t["sum"]}" is not a number each wolf keeps: a tally adds up '
                                                              'a player number.')
    if 'where' in t:
        d = _decl(phase, t['where'], ('text', 'switch'), 'text or switch')
        require(d['scope'] == 'player', f'"{t["where"]}" is not each wolf\'s own: a tally reads a player text or switch.')
        ok = type(t['is']) is bool if d['type'] == 'switch' else isinstance(t['is'], str) and len(t['is']) <= TEXT_MAX
        require(ok, f'"{t["where"]}" is a {d["type"]}: the tally\'s value is ' + ('on or off.' if d['type'] == 'switch' else 'a text.'))
    over = t.get('over', 'participants')
    require(over == 'participants' or (isinstance(over, dict) and set(over) == {'list'}),
            'A tally is over the wolves of the story, or a list of wolves ({"list": ...}).')
    if isinstance(over, dict):
        d = _decl(phase, over['list'], ('wolves',), 'list of wolves')
        require(d['scope'] not in PERSONAL, 'A tally is over a list of the story or the world.')


# Doc 86, C1a: what may stand for an id parameter: a variable of its kind (or a text), the event's field that holds one, the
# loop's value over a list of its kind.
OPERAND_VARS = {'wolf': ('wolf',), 'resident': ('resident',), 'place': ('place', 'text'), 'item': ('item', 'text'),
                'faction': ('faction', 'text'), 'town': ('text', 'region')}
OPERAND_FIELDS = {'wolf': ('actor', 'target'), 'resident': ('target', 'item'), 'place': ('cell', 'target'), 'item': ('item',),
                  'faction': ('target', 'detail'), 'town': ('actor', 'target', 'detail')}
_KIND_WORDS = {'int': 'a number', 'wolf': 'a wolf', 'resident': 'a resident', 'place': 'a place', 'item': 'an item',
               'faction': 'a faction', 'town': 'a town'}


def check_operand_param(param, v, phase, each, reward=False, share_ok=False):
    """C1a: a value given for an effect's parameter marked operand, of the parameter's kind. `share_ok` (C3b): a reward of a
    quest credited by share, whose numbers may read the credited wolf's share."""
    import story_program as rules

    require = rules.require
    kind, label = param['type'], param.get('label', param['key'])
    words = _KIND_WORDS.get(kind, 'a value')
    if reward:
        require(not any('field' in o or 'each' in o or o.get('of') == '$each' for o in operands_in(v)),
                f'A reward is given when the quest is done: {label} reads no event field and no loop value.')
    if kind == 'int':
        require(operand_type(v, phase, each, 0, share_ok) == 'number', f'{label} is a number: give a number value.')
        return
    if 'var' in v:
        rules.shape(v, ('var',), ('of',))
        d = _decl(phase, v['var'])
        require(d['type'] in OPERAND_VARS.get(kind, ()) and 'mirror' not in d,
                f'"{v["var"]}" is a {d["type"]}: {label} needs {words}' + (' (or a text)' if 'text' in OPERAND_VARS.get(kind, ()) else '') + '.')
        _check_of(v, d, each)
        return
    if 'field' in v:
        rules.shape(v, ('field',))
        allowed = OPERAND_FIELDS.get(kind, ())
        require(v['field'] in allowed, f'{label} is {words}: the event\'s ' + ' or '.join(allowed) + ' holds one.')
        return
    if 'each' in v:
        rules.shape(v, ('each',))
        require(v['each'] is True and each is not None, 'The value of the loop is only inside For each.')
        require(each == kind or (each == 'text' and kind not in ('wolf', 'resident')),
                f'The loop\'s value is {"a " + each if each else "nothing"}: {label} needs {words}.')
        return
    raise ValueError(f'{label} is {words}: give one, or a variable, the event\'s field or the loop\'s value.')


def check_fills(text, param, phase, each, of_allowed=True, where=None):
    """C1b: the fills of a version 2 text ({name}, {name.of}): declared, not a list or a timer, a wolf's own only "of", and
    the words within their limit with every fill at its widest."""
    import story_program as rules

    require = rules.require
    limit = param.get('max', 300) if isinstance(param, dict) else param
    label = where or (param.get('label', param.get('key', 'the words')) if isinstance(param, dict) else 'the words')
    found = fills_in(text)
    if not found and '{{' not in text and '}}' not in text:
        return
    widest = len(FILL.sub(lambda m: '{' if m.group(0) == '{{' else '}' if m.group(0) == '}}' else '', text))
    for name, of in found:
        d = _decl(phase, name)
        require(d['type'] not in LISTS and d['type'] != 'timer', f'{{{name}}}: a {d["type"]} cannot fill words (a list or a timer).')
        if of:
            require(of_allowed, f'{label}: the same for every wolf, so it fills no wolf\'s own value ({{{name}.of}}).')
            require(d['scope'] in PERSONAL, f'{{{name}.of}}: only a variable of each wolf or party is read "of" a wolf.')
        else:
            require(d['scope'] not in PERSONAL, f'{{{name}}} is each wolf\'s own: write {{{name}.of}}.')
        widest += FILL_WIDEST.get(d['type'], FILL_NAME) if 'mirror' not in d else FILL_WIDEST['number']
    require(widest <= limit, f'{label} is too long once filled: up to {widest} characters with every fill at its widest '
                             f'(a number 11, a text {TEXT_MAX}, a wolf\'s name 24, another name 40); the limit is {limit}.')


def _check_source(src, phase):
    """R1's who counts: {"list": a story or world list of wolves} or {"where": a test for each wolf} (exactly one)."""
    import story_program as rules

    require = rules.require
    require(isinstance(src, dict) and ('list' in src) != ('where' in src),
            'Count the wolves in a list, or those who meet a test (one of the two).')
    if 'list' in src:
        d = _decl(phase, src['list'], ('wolves',), 'list of wolves')
        require(d['scope'] in ('story', 'world'), 'Count a list of wolves of the story or the world.')
    else:
        _check_where(src['where'], phase)


def _check_where(c, phase):
    """R1 rule 5: the test for each wolf (the wolf as $each). Variables (of $each for a wolf's own), compare, switch, in
    list, days since and the quest conditions; no facts, event fields, loop values or counting inside; 3 deep, 8 leaves."""
    import story_program as rules

    require = rules.require
    leaves = []

    def walk(node, depth):
        require(isinstance(node, dict) and depth <= WHERE_DEPTH, f'The test for each wolf nests {WHERE_DEPTH} deep at most.')
        kind = node.get('test')
        if kind in ('all', 'any'):
            rules.shape(node, ('test', 'conditions'))
            require(isinstance(node['conditions'], list) and 1 <= len(node['conditions']) <= 8, 'A condition group needs 1–8 conditions.')
            for child in node['conditions']:
                walk(child, depth + 1)
            return
        if kind == 'not':
            rules.shape(node, ('test', 'condition'))
            walk(node['condition'], depth + 1)
            return
        require(kind in WHERE_TESTS, 'The test for each wolf may compare values, read switches, lists, days and quests: '
                                     'not the world\'s facts or the event (it is counted outside one).')
        leaves.append(node)
        require(len(leaves) <= WHERE_LEAVES, f'The test for each wolf has {WHERE_LEAVES} conditions at most.')
        for side in ('left', 'right', 'value'):
            for o in operands_in(node.get(side)):
                require('field' not in o and 'each' not in o, 'The test for each wolf reads no event field and no loop value.')
        _check_condition_v2(node, phase, 'wolf')
        decls = phase.get('_vars', {})
        for o in [node] + [o for side in ('left', 'right', 'value') for o in operands_in(node.get(side))]:
            name = o.get('var') or o.get('list') or o.get('count') or o.get('since')
            if name in decls and decls[name]['scope'] in PERSONAL:
                require(o.get('of') == '$each', f'Read "{name}" of each wolf ($each) in the test for each wolf.')
        if kind in QUEST_V2:
            require(node.get('of') in ('$each', 'story'), 'In the test for each wolf, a quest is read for each wolf ($each) or the story.')

    walk(c, 1)


def _check_condition_v2(c, phase, each):
    import story_program as rules

    require, shape = rules.require, rules.shape
    kind = c['test']
    if kind == 'participation':
        # (Doc 86, R1: enough of the active wolves took part.)
        shape(c, ('test', 'percent'), ('source', 'list', 'where', 'of', 'floor', 'cap'))
        require(each is None, 'Counting who took part is not allowed inside For each.')
        _check_source({k: c[k] for k in ('list', 'where') if k in c}, phase)
        require('source' not in c or c['source'] == ('list' if 'list' in c else 'where'), 'Who counts: the list, or the test.')
        require(c.get('of', 'active') in AUDIENCES, 'Out of the active wolves, all the story\'s wolves, or those online.')
        require(rules.integer(c['percent'], 1, 100), 'A share of 1 to 100%.')
        for key in ('floor', 'cap'):
            require(key not in c or rules.integer(c[key], 1, 5000), f'The {key}: 1 to 5,000 wolves.')
        require(c.get('floor', 1) <= c.get('cap', 5000), 'The floor is above the cap.')
        return
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
    if kind == 'count in region':
        # (Doc 86, J6: the story's wolves, or the residents of a list of the story or the world.)
        who = c.get('who', 'wolves')
        require(who in COUNTED, 'Count the story\'s wolves or the residents of a list.')
        require(('list' in c) == (who == 'residents'), 'Counting residents names their list, and only then.')
        if 'list' in c:
            d = _decl(phase, c['list'], ('residents',), 'list of residents')
            require(d['scope'] in ('story', 'world'), 'Count the residents of a list the story or the world keeps (not one kept for each wolf).')
    elif 'who' in c:
        _check_who(c['who'], each)
    if 'op' in c:
        ops = ('=', '!=', '>=', '<=') if kind == 'relation is' else tuple(COMPARE)
        require(c['op'] in ops, 'Choose a comparison.')
    if 'value' in c and kind not in ('resident has', 'calendar is', 'wolf has'):
        low, high = {'standing': (-100, 100), 'reputation': (-1000, 1000), 'party size': (1, 12),
                     'count in region': (0, 5000), 'carries': (0, 100000)}.get(kind, (0, LIMIT))
        require(rules.integer(c['value'], low, high), f'A whole number from {low} to {high}.')
    for key in ('region',):
        if key in c:
            require(rules.identifier(c[key]) and c[key] in phase.get('_regions', {}), 'Unknown story region.')
    for key in ('place', 'town', 'faction', 'other', 'npc', 'item'):
        if key in c and not (kind == 'in party with' and key == 'other'):
            # (Doc 86, J9: an item may be a story's own, item^tag.)
            require(rules.item_ok(c[key]) if key == 'item' else rules.identifier(c[key]), f'Name the {key}.')
    if kind == 'in party with':
        _check_who(c['other'], each, 'the other wolf')
    if 'is' in c:
        require(type(c['is']) is bool, 'Yes or no.')
    if kind == 'wolf is':
        require(c['state'] in WOLF_STATES, 'Downed, hurt, wanted or held.')
    elif kind == 'resident is':
        require(c['state'] in RESIDENT_STATES, 'Alive, dead, indoors or out of doors.')
    elif kind == 'resident has':
        require(c['what'] in RESIDENT_HAS and isinstance(c['value'], str) and (rules.word(c['value'], 80) or (c['what'] == 'post' and c['value'] == '')),
                'A job, mood, mantle, town or post, and what it is.')
    elif kind == 'wolf has':
        # (Doc 86, J10: normal, gifted or quickened; or a playable family, or none: a normal wolf's.)
        require(c['what'] in ('tier', 'family'), "A wolf's Gift: its tier or its family.")
        require(c['value'] in GIFT_TIERS if c['what'] == 'tier' else c['value'] in ('', *GIFT_FAMILIES),
                'A tier is normal, gifted or quickened.' if c['what'] == 'tier' else
                'A family is one of ' + ', '.join(GIFT_FAMILIES) + ' (or none, for a normal wolf).')
    elif kind == 'hour between':
        require(rules.integer(c['from'], 0, 23) and rules.integer(c['to'], 0, 23), 'Hours from 0 to 23.')
    elif kind == 'calendar is':
        require(c['what'] in CALENDAR and c['value'] in CALENDAR[c['what']], 'A weekday, a season, a time of day or the moon, and which.')
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
        require('mirror' not in d, f'"{a[d_key]}" mirrors a world variable: it is read, never set.')   # (Doc 86, N3.)
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
    elif kind == 'rank':
        # (Doc 86, C3a: the top wolves by a number each wolf keeps, into a list of wolves of the story or the world.)
        shape(a, ('do', 'var', 'top', 'into'), ('over', 'list', 'min'))
        require(each is None, 'Rank is not allowed inside For each: rank first, then loop over the list.')
        d = _decl(phase, a['var'], ('number',), 'number')
        require(d['scope'] == 'player' and 'mirror' not in d, f'"{a["var"]}" is not a number each wolf keeps: rank by a player number.')
        require(operand_type(a['top'], phase) == 'number' and (not _num(a['top']) or 1 <= a['top'] <= RANK_MAX),
                f'How many: 1 to {RANK_MAX}, or a number value.')
        require('min' not in a or operand_type(a['min'], phase) == 'number', 'At least: a number, or a number value.')
        over = a.get('over', 'participants')
        require(over in ('participants', 'list') and ('list' in a) == (over == 'list'),
                'Rank the wolves of the story, or those of a list (name the list).')
        if over == 'list':
            src = _decl(phase, a['list'], ('wolves',), 'list of wolves')
            require(src['scope'] not in PERSONAL, 'Rank the wolves of a list of the story or the world.')
        into = _decl(phase, a['into'], ('wolves',), 'list of wolves')
        require(into['scope'] not in PERSONAL, 'Rank into a list of wolves of the story or the world.')
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
        # (Doc 86, C1: a parameter marked operand may be a value of its kind; a text marked fills may hold {name}.)
        rules.validate_effect(effect, phase['id'], phase.get('_regions'), phase.get('_scenes'),
                              operand=lambda param, v: check_operand_param(param, v, phase, each))
        for param in rules.EFFECT_PARAMS.get(effect['kind'], ()):
            if param.get('fills') and isinstance(effect.get(param['key']), str):
                check_fills(effect[param['key']], param, phase, each)
        require(effect['kind'] != 'npc.line', 'Set resident dialogue on phase entry.')
        rules.quests.check_story_effect(effect, phase, t)
        # (Doc 86, C8: what one resident remembers of one wolf is for a wolf of the story: the acting one of a participant
        # trigger, or the looped one.)
        require(not (effect['kind'] in ('talk.remember', 'talk.forget') and effect.get('scope') == 'pair' and a['effect'].get('player') == '$actor'
                     and t.get('audience') != 'participant'), rules.PAIR_WORDS)
        # (Doc 85, Phase 6: taking a wolf in, or offering or giving them a quest, may name the acting wolf from a trigger of
        # anyone in the world: that is how an open audience is joined.)
        require(a['effect'].get('player') != '$actor' or t.get('audience') == 'participant'
                or effect['kind'] in rules.quests.TAKES_IN or effect['kind'] in rules.quests.NOTICE_EFFECTS,
            'An actor effect requires a participant trigger.')   # (Doc 86, C12a: news only names the wolf: any trigger may.)
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
    require(rules.match_ok(match), 'Invalid trigger filters.')
    # (Doc 85, Phase 10: a watched event the game can only look for when it is told what to look at: a resident to come
    # near, a purse and the amount it crosses, a faction and the standing. Doc 86, C12b: those stay exact texts.)
    needs = rules.NEEDS_MATCH.get(t['on'], ())
    require(all(match.get(k) for k in needs), f'"{t["on"]}" needs its ' + ' and '.join(rules.match_word(t['on'], k) for k in needs) + ' named.')
    require(all(isinstance(match[k], str) for k in needs),
            f'"{t["on"]}" needs its ' + ' and '.join(rules.match_word(t['on'], k) for k in needs) + ' as exact words (not "begins with" or "one of"): '
            'the game must know what to look at.')
    if t['on'] in ('purse crossed', 'standing crossed') and 'detail' in match:
        require(re.fullmatch(r'-?[0-9]{1,9}', match['detail']) is not None, f'"{t["on"]}" names a whole number to cross.')
    rules.check_moon(t['on'], match)
    rules.check_variable_changed(t['on'], match, phase)
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


def var_store(state, scope, wolf, write=False, members=None):
    """The store a variable lives in: one for the story or the world, one per wolf or party. None: a wolf not in the
    story (nothing of theirs is read or written). `members`: the cohort as a set, when the caller has it (R1's count)."""
    if scope in PERSONAL:
        if wolf not in (state['cohort'] if members is None else members):
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
        self.each_wolf = False   # (Doc 86, C1b: whether the loop is over wolves, so {name.of} reads the looped wolf.)
        self.vars = phase.get('_vars', {}) if phase else {}
        self._audience = None
        self.members = None   # (Doc 86, R1: the cohort as a set while a test is counted for each wolf.)
        self.share = None     # (Doc 86, C3b: the credited wolf's share, while a reward of a quest credited by share is given.)

    def audience(self):
        """Doc 86, C2: the story's wolves counted (all, active, online) and the active and online ones, worked out once per
        firing; the native twin is sent the same (story_native.fire_v2)."""
        if self._audience is None:
            self._audience = audience_of(self.state, self.day, (self.phase or {}).get('_activeDays', ACTIVE_DAYS),
                                         facts_of(self.event) if self.event else None)
        return self._audience

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


def audience_of(state, day, active_days, facts=None):
    """Doc 86, C2: {all, active, online, activeIds, onlineIds}. Active: the story's wolves seen (signed in, or the actor of
    any event: state['seen'], kept by the runner and by a rehearsal) within `active_days` game days of `day`. Online: the
    story's wolves among the signed-in wolves of the world's facts, else as the run last knew (state['online']), else
    unknown (MISSING, None for the ids). A rehearsal's case may give the counts (`audience`) or the active wolves."""
    cohort = state.get('cohort', [])
    members = set(cohort)
    given = state.get('rehearsalActive')
    if given is not None:
        active = sorted(w for w in set(given) if w in members)
    else:
        seen = state.get('seen') or {}
        if day is None:
            day = max(seen.values(), default=0)
        since = day - active_days
        active = [w for w in cohort if seen.get(w, -1e18) >= since]
    wolves = facts.get('wolves') if isinstance(facts, dict) else None
    if isinstance(wolves, dict):
        online = sorted(w for w in wolves if w in members)
    elif isinstance(state.get('online'), list):
        online = sorted(w for w in state['online'] if w in members)
    else:
        online = None
    out = {'all': len(cohort), 'active': len(active), 'online': MISSING if online is None else len(online),
           'activeIds': active, 'onlineIds': online}
    for key, n in (state.get('rehearsalAudience') or {}).items():
        out[key] = n
    return out


def share_need(d, percent, floor=1, cap=None):
    """R1 rule 4 (decisions §6.2): the wolves needed out of d. The floor never asks for more wolves than are active; a cap is
    always enough; never below 1."""
    need = max(min(floor, d), -(-percent * d // 100))
    if cap is not None:
        need = min(need, cap)
    return max(need, 1)


def counted(src, of, x):
    """R1 rule 1-2: (S, D), the wolves who count and those counted against, or None when D is unknown (online, not known)."""
    aud = x.audience()
    d = aud['activeIds'] if of == 'active' else list(x.state.get('cohort', [])) if of == 'all' else aud['onlineIds']
    if d is None:
        return None
    if 'list' in src:
        decl = x.vars[src['list']]
        store = var_store(x.state, decl['scope'], '')
        members = set(store.get(src['list'], initial(decl)))
        return [w for w in d if w in members], d
    out, before, members = [], x.each, x.members
    x.members = frozenset(x.state.get('cohort', ()))
    try:
        for w in d:
            x.each = w
            if test(src['where'], x) is True:
                out.append(w)
    finally:
        x.each, x.members = before, members
    return out, d


def value_of(o, x):
    """An operand's value now, or MISSING (a value of a wolf not in the story, a timer not started, no loop)."""
    if not isinstance(o, dict):
        return o
    if 'audience' in o:
        return x.audience()[o['audience']]
    if 'share' in o:
        return MISSING if x.share is None else x.share
    if 'tally' in o:
        return tally(o['tally'], x)
    share = next((k for k in SHARES if k in o), None)
    if share is not None:
        r = counted(o[share], o.get('of', 'active'), x)
        if r is None:
            return MISSING
        s, d = r
        return len(s) if share == 'among' else (100 * len(s) // len(d) if d else 0)
    if 'purse' in o:
        # (Doc 86, M2: what the purse holds now, as the world's facts of the event say; not known, no value.)
        f = facts_of(x.event) if x.event else None
        n = ((f or {}).get('purses') or {}).get(o['purse']) if isinstance((f or {}).get('purses'), dict) else None
        return n if _num(n) else MISSING
    if 'var' in o and 'mirror' in x.vars.get(o['var'], {}):
        return mirror_value(x.vars[o['var']], x, o.get('of'))
    if 'var' in o or 'count' in o or 'since' in o:
        name = o.get('var') or o.get('count') or o.get('since')
        d = x.vars[name]
        store = var_store(x.state, d['scope'], x.wolf(o.get('of')), members=x.members)
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
    if key == 'div':
        # (Doc 86, C2: floored toward zero, as the native twin's trunc of a double; by 0, no value.)
        return MISSING if b == 0 else int(a / b)
    if key == 'times':
        # (Exact below 2^53, as the native twin's double is; past it, the double's own product, so both twins agree.)
        product = a * b
        return product if abs(product) <= 2 ** 53 else float(a) * float(b)
    return a + b if key == 'plus' else a - b if key == 'minus' else min(a, b) if key == 'min' else max(a, b)


def tally(t, x):
    """J11: over every wolf of the story (or of a list of wolves), those whose `where` is `is` (every one without), their
    `sum` added up (counted without); a wolf with no value adds 0. The native twin's Dispatch2::tally."""
    state = x.state
    cohort = state.get('cohort', [])
    over = t.get('over', 'participants')
    if isinstance(over, dict):
        d = x.vars[over['list']]
        members = set(cohort)
        wolves = [w for w in (var_store(state, d['scope'], '') or {}).get(over['list'], initial(d)) if w in members]
    else:
        wolves = cohort
    players = state.get('counters', {}).get('player', {})
    where, total = t.get('where'), 0
    blank = initial(x.vars[where]) if where else None
    for w in wolves:
        own = players.get(w, {})
        if where is not None and own.get(where, blank) != t['is']:
            continue
        if 'sum' in t:
            v = own.get(t['sum'], 0)
            total += v if _num(v) else 0
        else:
            total += 1
    return total


def rank(a, x):
    """C3a: the wolves of `over` with a stored number `var` (at least `min`), highest first, ties by the seed (fnv of seed|wolf),
    the first `top` (1 to 500) into the list `into` (cleared first). A value not known for top or min: nothing changes."""
    state = x.state
    top = value_of(a['top'], x)
    least = value_of(a['min'], x) if 'min' in a else None
    if top is MISSING or least is MISSING or not _num(top) or (least is not None and not _num(least)):
        x.note('skipped: rank has no value')
        return
    top = max(1, min(RANK_MAX, math.floor(top)))
    cohort = state.get('cohort', [])
    if a.get('over', 'participants') == 'list':
        d = x.vars[a['list']]
        members = set(cohort)
        wolves = [w for w in (var_store(state, d['scope'], '') or {}).get(a['list'], initial(d)) if w in members]
    else:
        wolves = cohort
    players, name, seed = state.get('counters', {}).get('player', {}), a['var'], str(state.get('seed', ''))
    found = []
    for w in wolves:
        own = players.get(w)
        if not own or name not in own or not _num(own[name]):
            continue
        v = own[name]
        if least is not None and v < least:
            continue
        found.append((-v, fnv(seed + '|' + w), w))
    found.sort()
    d = x.vars[a['into']]
    var_store(state, d['scope'], '', True)[a['into']] = [w for _, _, w in found[:top]]


def mirror_value(d, x, of=None, wolf=None):
    """Doc 86, N3: a shown number of each wolf that mirrors a world variable: the wolf's `key` text names which (`names`);
    never stored. An unknown or empty key reads 0; a wolf not in the story, no value."""
    m = d['mirror']
    key = x.vars.get(m['key'], {})
    who = wolf if wolf is not None else x.wolf(of)
    store = var_store(x.state, key.get('scope', 'player'), who, members=x.members)
    if store is None:
        return MISSING
    name = m['names'].get(store.get(m['key'], initial(key)) if key else '')
    if not name or name not in x.vars:
        return 0
    v = var_store(x.state, 'world', '').get(name, initial(x.vars[name]))
    return v if _num(v) else 0


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
    if kind == 'participation':
        r = counted(c, c.get('of', 'active'), x)
        if r is None:
            return None
        s, d = r
        return bool(d) and len(s) >= share_need(len(d), c['percent'], c.get('floor', 1), c.get('cap'))
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
        if c.get('who') == 'residents':
            # (Doc 86, J6: the listed residents the facts have, alive and inside; none of them known: unknown.)
            listed = value_of({'var': c['list']}, x)
            if listed is MISSING or not isinstance(listed, list):
                return None, None
            res = f.get('residents') if isinstance(f.get('residents'), dict) else {}
            ids = sorted({i for i in listed if isinstance(i, str)})
            known = [res[i] for i in ids if isinstance(res.get(i), dict)]
            if ids and not known:
                return None, None
            n = sum(1 for one in known if one.get('alive') is True and _inside(r, one))
            return COMPARE[c['op']](n, c['value']), n
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
    if kind == 'wolf has':
        # (Doc 86, J10: the facts' gift, {tier, family}; none written: unknown.)
        g = w.get('gift')
        v = g.get(c['what']) if isinstance(g, dict) else None
        return (None, None) if not isinstance(v, str) else (v == c['value'], v)
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


# ------------------------------------------------------------------- doc 86, Phase 2 (C1): values in effects, fills in words

_SPECS = {}
# (A fill's widest, for the length check: a number, a text, a wolf's name, any other name.)
FILL_WIDEST = {'number': 11, 'switch': 3, 'text': TEXT_MAX, 'wolf': 24}
FILL_NAME = 40
NAME_TOKENS = {'wolf': 'w', 'resident': 'r', 'place': 'p', 'item': 'i', 'faction': 'f'}
FILL = re.compile(r'\{\{|\}\}|\{([A-Za-z0-9_][A-Za-z0-9_.:-]*?)(\.of)?\}|[{}]')


def effect_specs():
    """{effect: {param: spec}} for the parameters marked operand or fills in the catalogue (the native twin is sent the same)."""
    if not _SPECS:
        import story_program as rules
        for kind, params in rules.EFFECT_PARAMS.items():
            mine = {p['key']: {k: p[k] for k in ('type', 'min', 'max', 'nonzero', 'operand', 'fills') if k in p}
                    for p in params if p.get('operand') or p.get('fills')}
            if mine:
                _SPECS[kind] = mine
    return _SPECS


def fills_in(text):
    """The fills of a text: [(name, of)]; raises on a lone brace."""
    out = []
    for m in FILL.finditer(text):
        token = m.group(0)
        if token in ('{{', '}}'):
            continue
        if m.group(1) is None:
            raise ValueError('A lone { or } in words: write {{ or }} for a brace, or {name} for a variable.')
        out.append((m.group(1), bool(m.group(2))))
    return out


def fills_in_quietly(text):
    """Whether a text holds any {name} fill (a lone brace is no fill)."""
    return any(m.group(1) is not None for m in FILL.finditer(text))


def _fill_one(name, of, x):
    d = x.vars.get(name)
    if d is None:
        return MISSING, None
    wolf = (x.each if x.each_wolf and x.each is not None else x.actor) if of else None
    if 'mirror' in d:
        return mirror_value(d, x, wolf=wolf if of else ''), d
    store = var_store(x.state, d['scope'], wolf if of else '', members=x.members)
    if store is None:
        return MISSING, d
    return store.get(name, initial(d)), d


def fill_text(text, x):
    """C1b: a version 2 story's words with their fills: a number's digits, a switch's yes or no, a text as it is, and a wolf, a
    resident, a place, an item or a faction as a name token ({@w:<id>}...) the game turns into the name. A fill with no value is
    nothing, with a note. {{ and }} are braces."""
    def one(m):
        token = m.group(0)
        if token == '{{':
            return '{'
        if token == '}}':
            return '}'
        if m.group(1) is None:
            return token
        v, d = _fill_one(m.group(1), bool(m.group(2)), x)
        if v is MISSING or v is None:
            x.note(f'fill {m.group(1)} has no value')
            return ''
        kind = d['type']
        if kind == 'switch':
            return 'yes' if v is True else 'no'
        if kind == 'number' or _num(v):
            return str(int(v)) if _num(v) else ''
        if kind in NAME_TOKENS:
            return '{@' + NAME_TOKENS[kind] + ':' + v + '}' if isinstance(v, str) and v else ''
        return v if isinstance(v, str) else ''
    return FILL.sub(one, text)


def resolve_effect(effect, x):
    """C1a and C1b: an effect as it is sent, its operand parameters worked out (a number floored and kept to its bounds; an id
    a text) and its words filled. None when a value is missing (or a number that may not be 0 is 0): it is not sent, and the
    firing's history says why."""
    specs = effect_specs().get(effect.get('kind'))
    if not specs:
        return effect
    out = dict(effect)
    for key, p in specs.items():
        v = out.get(key)
        if p.get('operand') and isinstance(v, dict):
            r = value_of(v, x)
            if p['type'] == 'int':
                if r is MISSING or not _num(r):
                    x.note(f'skipped: {key} has no value')
                    return None
                n = max(p.get('min', -LIMIT), min(p.get('max', LIMIT), math.floor(r)))
                if p.get('nonzero') and n == 0:
                    x.note(f'skipped: {key} is 0')
                    return None
                out[key] = n
            else:
                if not isinstance(r, str) or not r:
                    x.note(f'skipped: {key} has no value')
                    return None
                out[key] = r
        elif p.get('fills') and isinstance(v, str) and ('{' in v or '}' in v):
            out[key] = fill_text(v, x)
    return out


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
        effect = resolve_effect(effect, x)
        if effect is not None:
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
        before, before_wolf = x.each, x.each_wolf
        x.each_wolf = a['over'] != 'list' or x.vars[a['list']]['type'] == 'wolves'
        for wolf in _wolves(a, x)[:EACH_MAX]:
            x.each = wolf
            r = _run(a['actions'], x, (*address, 'actions'))
            if r:
                x.each, x.each_wolf = before, before_wolf
                return r
        x.each, x.each_wolf = before, before_wolf
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
    elif op == 'rank':
        rank(a, x)
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
        if kind == 'participation':
            # (Doc 86, R1: the counts behind it, for the console's "why".)
            x = Ctx(phase or {}, state, event, event.get('actor', ''), day, elapsed, ready, None)
            r = counted(c, c.get('of', 'active'), x)
            if r is None:
                return {**node, 'verdict': None, 'needs': 'online'}
            s, d = r
            need = share_need(len(d), c['percent'], c.get('floor', 1), c.get('cap')) if d else None
            return {**node, 'verdict': bool(d) and len(s) >= need, 'fact': {'among': len(s), 'of': len(d), 'need': need}}
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
    return {**node, 'value': value, 'verdict': field_compare(c['op'], value, c['value'])}


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
    return field_compare(c['op'], value, c['value'])


def field_compare(op, value, expected):
    """The field condition's comparison: the six of COMPARE, and (doc 86, C12b) begins with and one of."""
    if op in TEXT_OPS:
        import story_program as rules
        return rules.match_one({op: expected}, value if isinstance(value, str) else '')
    return COMPARE[op](value, expected)


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
