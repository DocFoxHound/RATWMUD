"""Event–condition–action rules. Signals retain the originating world fact and last for one dispatch."""

import operator

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


def routes(phase):
    return phase['exits'] + [
        {
            'to': a['phase'],
            'on': 'trigger transition',
            'participant': t.get('audience') == 'participant',
        }
        for t in phase.get('triggers', [])
        for a in t['actions']
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
    ids, links = set(), {}
    missions = {m['id'] for m in phase.get('missions', [])}

    def counter(c):
        require(
            rules.identifier(c['key']) and c['scope'] in ('story', 'player'),
            'Choose a counter name and scope.',
        )

    def condition(c, depth):
        require(
            isinstance(c, dict) and depth <= 5,
            'Condition groups may nest up to five levels.',
        )
        kind = c.get('test')
        if kind in ('all', 'any'):
            shape(c, ('test', 'conditions'))
            children = c['conditions']
            require(
                isinstance(children, list) and 1 <= len(children) <= 8,
                'A condition group needs 1–8 conditions.',
            )
            for child in children:
                condition(child, depth + 1)
        elif kind == 'not':
            shape(c, ('test', 'condition'))
            condition(c['condition'], depth + 1)
        elif kind == 'mission':
            shape(c, ('test', 'id'))
            require(
                rules.identifier(c['id']) and c['id'] in missions,
                'Unknown mission in this phase.',
            )
        elif kind in ('counter', 'field'):
            if kind == 'counter':
                shape(c, ('test', 'key', 'scope', 'op', 'value'))
                counter(c)
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

    for t in triggers:
        shape(
            t,
            ('id', 'on', 'actions'),
            ('match', 'signal', 'if', 'once', 'audience', 'afterDays'),
        )
        require(
            rules.identifier(t['id']) and t['id'] not in ids,
            'Give every trigger in the phase a unique ID.',
        )
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
        if 'if' in t:
            condition(t['if'], 1)
        actions = t['actions']
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
            elif kind == 'effect':
                shape(a, ('do', 'effect'))
                rules.validate_effect(a['effect'], phase['id'])
                require(
                    a['effect']['kind'] != 'npc.line',
                    'Set resident dialogue on phase entry.',
                )
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
            else:
                raise ValueError('Unknown trigger action.')
    # Reject cycles even when the current fixtures would never take that path.
    visited = set()

    def visit(name, stack):
        require(name not in stack, 'Story signals form a cycle at ' + name)
        if name not in visited:
            for child in links.get(name, ()):
                visit(child, stack | {name})
            visited.add(name)

    for name in links:
        visit(name, set())


def counter_store(state, scope, actor, write=False):
    if scope == 'player' and actor not in state['cohort']:
        return {}
    if write:
        store = state.setdefault('counters', {}).setdefault(scope, {})
        return store.setdefault(actor, {}) if scope == 'player' else store
    store = state.get('counters', {}).get(scope, {})
    return store.get(actor, {}) if scope == 'player' else store


def satisfied(c, state, event, ready):
    kind = c['test']
    if kind == 'all':
        return all(satisfied(x, state, event, ready) for x in c['conditions'])
    if kind == 'any':
        return any(satisfied(x, state, event, ready) for x in c['conditions'])
    if kind == 'not':
        return not satisfied(c['condition'], state, event, ready)
    if kind == 'mission':
        return c['id'] in ready()
    if kind == 'counter':
        if c['scope'] == 'player' and event.get('actor') not in state['cohort']:
            return False
        value = counter_store(state, c['scope'], event.get('actor')).get(c['key'], 0)
    else:
        value = event.get(c['field'], 0 if c['field'] in ('coins', 'quantity') else '')
    return COMPARE[c['op']](value, c['value'])


def fire(phase, state, event, day, elapsed, ready, matches, emit, trace):
    actor = event.get('actor', '')
    for t in phase.get('triggers', ()):
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
        if 'if' in t and not satisfied(t['if'], state, event, ready):
            continue
        if repeat != 'each':
            state.setdefault(bucket, {}).setdefault(token, []).append(owner)
        if trace is not None:
            trace.append(
                {
                    'phase': phase['id'],
                    'day': day,
                    'actor': actor,
                    'elapsedDays': elapsed,
                    'actions': [a['effect']['kind'] if a['do'] == 'effect' else a['do'] for a in t['actions']],
                    'trigger': t['id'],
                    'scope': 'campaign' if t['id'] in phase.get('_sharedIds', ()) else 'phase',
                    'event': event.get('id'),
                    'on': kind,
                    'signal': event.get('signal'),
                }
            )
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
