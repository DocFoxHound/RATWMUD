"""Event–condition–action rules. Signals retain the originating world fact and last for one dispatch."""

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
    for t in triggers:
        _check_trigger(t, phase, ids, links)
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


def _check_condition(c, depth, phase, missions):
    import story_program as rules

    require, shape = rules.require, rules.shape
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
            _check_condition(child, depth + 1, phase, missions)
    elif kind == 'not':
        shape(c, ('test', 'condition'))
        _check_condition(c['condition'], depth + 1, phase, missions)
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


def _check_trigger(t, phase, ids, links):
    """One trigger's checks (validate's loop body): its id against `ids` (added to), its signal links into `links`."""
    import story_program as rules

    require, shape = rules.require, rules.shape
    missions = {m['id'] for m in phase.get('missions', [])}

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
        ('match', 'signal', 'if', 'once', 'audience', 'afterDays', 'folder', 'note'),
    )
    require(
        rules.identifier(t['id']) and t['id'] not in ids,
        'Give every trigger in the phase a unique ID.',
    )
    # The Story Creator's folder and note (doc 85, 4.2; Phase 4): words for the author, which the engine ignores.
    require('folder' not in t or rules.word(t['folder'], 80), "A trigger's folder is a name of up to 80 characters.")
    require('note' not in t or rules.word(t['note'], 300), "A trigger's note is up to 300 characters.")
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
            rules.validate_effect(a['effect'], phase['id'], phase.get('_regions'), phase.get('_scenes'))
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
    (`.actions[i]`). `ids` are the other trigger ids of the phase. A list of (subpath, words); empty when it is sound."""
    errors = (ValueError, KeyError, TypeError, AttributeError, IndexError)
    try:
        _check_trigger(t, phase, set(ids), {})
        return []
    except errors as error:
        whole = _message(error)
    if not isinstance(t, dict) or not isinstance(t.get('actions'), list) or not 1 <= len(t['actions']) <= 12:
        return [('', whole)]
    # The head alone, with one harmless action in place of its own; then its conditions; then each action alone.
    head = {k: v for k, v in t.items() if k != 'if'}
    head['actions'] = [{'do': 'set', 'key': 'probe', 'scope': 'story', 'value': 0}]
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
    for index, action in enumerate(t['actions']):
        try:
            _check_trigger({**head, 'actions': [action]}, phase, set(ids), {})
            if isinstance(action, dict) and action.get('do') == 'enter' and index != len(t['actions']) - 1:
                out.append((f'.actions[{index}]', 'Enter phase must be the last action.'))
        except errors as error:
            out.append((f'.actions[{index}]', _message(error)))
    return out or [('', whole)]


def judge(c, state, event, ready, regions=None, known=None):
    """The recording form of `satisfied` (doc 85, 5.5; the Campaigns console's "why", Python only: the live path needs no
    tree): the same verdict for every node, true, false or None (unknown), with the values behind it. `known` names the
    event fields that are known; None means all of them (a real event). A test of a field that is not known, or of a
    place when the event has none, is unknown, with `needs` saying what it waits for."""
    kind = c['test']
    if kind in ('all', 'any'):
        children = [judge(child, state, event, ready, regions, known) for child in c['conditions']]
        decisive = kind == 'any'
        verdicts = [child['verdict'] for child in children]
        verdict = decisive if decisive in verdicts else None if None in verdicts else not decisive
        return {'test': kind, 'verdict': verdict, 'conditions': children}
    if kind == 'not':
        child = judge(c['condition'], state, event, ready, regions, known)
        return {'test': 'not', 'verdict': None if child['verdict'] is None else not child['verdict'], 'condition': child}
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


def trigger_key(phase, trigger_id):
    """How state.disabled names a trigger (doc 85, 3.5 and Phase 2): a campaign trigger by its id, a phase's own trigger
    as phase/id (it stays off on a later visit of that phase)."""
    return trigger_id if trigger_id in phase.get('_sharedIds', ()) else phase['id'] + '/' + trigger_id


def counter_store(state, scope, actor, write=False):
    if scope == 'player' and actor not in state['cohort']:
        return {}
    if write:
        store = state.setdefault('counters', {}).setdefault(scope, {})
        return store.setdefault(actor, {}) if scope == 'player' else store
    store = state.get('counters', {}).get(scope, {})
    return store.get(actor, {}) if scope == 'player' else store


def satisfied(c, state, event, ready, regions=None):
    """A missing historical position is unknown, including when an author negates the test."""
    kind = c['test']
    if kind in ('all', 'any'):
        decisive = kind == 'any'
        unknown = False
        for child in c['conditions']:
            value = satisfied(child, state, event, ready, regions)
            if value is decisive:
                return decisive
            unknown = unknown or value is None
        return None if unknown else not decisive
    if kind == 'not':
        value = satisfied(c['condition'], state, event, ready, regions)
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


def fire(phase, state, event, day, elapsed, ready, matches, emit, trace):
    native = story_native.interpreter()
    if native:
        return story_native.fire(native, phase, state, event, day, elapsed, ready, emit, trace)
    actor = event.get('actor', '')
    disabled = set(state.get('disabled', ()))
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
        if 'if' in t and not satisfied(t['if'], state, event, ready, phase.get('_regions')):
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
        target = perform(t, state, event, actor, day, elapsed, emit)
        if target:
            return target
    return None


def perform(t, state, event, actor, day, elapsed, emit):
    """A trigger's actions, in order, for the acting wolf: the target phase of an `enter`, or None. (Also the Campaigns
    console's "fire now", doc 85 Phase 2.)"""
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
