"""Bridge to the world's compiled event/condition/action interpreter. No database access."""

import ctypes
import json
import os
import sys
from functools import lru_cache
from pathlib import Path


@lru_cache(maxsize=1)
def interpreter():
    override = os.environ.get('RATW_STORY_LIBRARY')
    root = Path(__file__).resolve().parents[1]
    name = {'win32': 'ratw_story.dll', 'darwin': 'libratw_story.dylib'}.get(sys.platform, 'libratw_story.so')
    paths = [root / 'bin' / name, root / 'build-core' / name, root / 'build' / name]
    if os.environ.get('RATW_SERVER'):
        paths.insert(0, Path(os.environ['RATW_SERVER']).resolve().parent / name)
    for path in [Path(override)] if override else paths:
        if path.exists() or override:
            dispatch = ctypes.CDLL(str(path)).ratw_story_dispatch_v1
            dispatch.argtypes = [ctypes.c_char_p]
            dispatch.restype = ctypes.c_char_p
            return dispatch
    return None


def fire(dispatch, phase, state, event, day, elapsed, ready, emit, trace):
    kind = event['kind']
    # Most world facts have no listeners. Avoid copying counters and mission progress in that case.
    rules = [t for t in phase.get('triggers', ()) if t['on'] == kind or (t['on'] == 'gift' and kind == 'gift again')]
    if not rules:
        return None
    actor = event.get('actor', '')
    shared = phase.get('_sharedIds', ())
    # A rule may inspect the story or the event's actor. Other characters' counters and
    # repeat markers for unrelated phases are never inputs to this dispatch.
    existing = state.get('counters', {})
    visible = {'story': existing['story']} if 'story' in existing else {}
    if actor in existing.get('player', {}):
        visible['player'] = {actor: existing['player'][actor]}
    ids = {t['id'] for t in rules}
    global_ids = ids | {phase['id'] + '/' + rid for rid in ids}
    request = {
        'rules': rules,
        'shared': list(shared),
        'disabled': [k for k in state.get('disabled', ()) if k in global_ids],
        'regions': phase.get('_regions', {}),
        'phase': phase['id'],
        'entered': state['entered'],
        'day': day,
        'elapsed': elapsed,
        'fact': event,
        'participant': actor in state['cohort'],
        'ready': list(ready()) if phase.get('missions') and any('if' in t for t in rules) else [],
        'counters': visible,
        'fired': {rid: state['fired'][rid] for rid in ids if rid in state.get('fired', {})},
        'campaignFired': {
            rid: state['campaignFired'][rid] for rid in global_ids if rid in state.get('campaignFired', {})
        },
    }
    result = json.loads(dispatch(json.dumps(request, ensure_ascii=False, allow_nan=False).encode('utf-8')))
    if 'error' in result:
        raise ValueError(result['error'])
    if 'counters' in result:
        counters = state.setdefault('counters', {})
        changes = result['counters']
        if 'story' in changes:
            counters['story'] = changes['story']
        if 'player' in changes:
            counters.setdefault('player', {}).update(changes['player'])
    for key in ('fired', 'campaignFired'):
        if key in result:
            state.setdefault(key, {}).update(result[key])
    for name, timer in result.get('timers', {}).items():
        if timer is None:
            state.get('timers', {}).pop(name, None)
        else:
            state.setdefault('timers', {})[name] = timer
    if result.get('effects'):
        state.setdefault('triggerEffects', []).extend(result['effects'])
    for signal in result.get('signals', ()):
        emit(signal, event)
    if trace is not None and result.get('firedRules'):
        by_id = {rule['id']: rule for rule in rules}
        for rid in result['firedRules']:
            rule = by_id[rid]
            trace.append(
                {
                    'phase': phase['id'],
                    'day': day,
                    'actor': actor,
                    'elapsedDays': elapsed,
                    'actions': [a['effect']['kind'] if a['do'] == 'effect' else a['do'] for a in rule['actions']],
                    'trigger': rid,
                    'scope': 'campaign' if rid in shared else 'phase',
                    'event': event.get('id'),
                    'on': rule['on'],
                    'signal': event.get('signal'),
                }
            )
    return result.get('phase')


# The run's state pieces a version 2 dispatch reads and changes (doc 85, Phase 5): they go to the native twin and come
# back as the Python twin would leave them, key for key (absent stays absent).
STATE_V2 = ('counters', 'fired', 'campaignFired', 'counts', 'timers', 'disabled', 'waiting', 'draws', 'rolls', 'outcome',
            'outcomes', 'parties', 'seed')


def fire_v2(dispatch, phase, state, event, day, elapsed, ready, emit, trace, wait=None):
    """A version 2 story's fact (or, with `wait`, a waiting trigger's remaining actions) through the native twin."""
    import story_triggers

    kind = event['kind']
    rules = phase.get('triggers', [])
    if wait is None and not any(t['on'] == kind or (t['on'] == 'gift' and kind == 'gift again') for t in rules):
        return None
    request = {
        'v': 2,
        'rules': rules,
        'shared': list(phase.get('_sharedIds', ())),
        'regions': phase.get('_regions', {}),
        'phase': phase['id'],
        'entered': state['entered'],
        'began': state.get('began', state['entered']),
        'visits': state['visits'],
        'day': day,
        'elapsed': elapsed,
        'fact': event,
        'cohort': state['cohort'],
        'ready': list(ready()) if phase.get('missions') else [],
        'decls': phase.get('_vars', {}),
        'state': {k: state[k] for k in STATE_V2 if k in state},
    }
    if wait is not None:
        request['resume'] = wait
    result = json.loads(dispatch(json.dumps(request, ensure_ascii=False, allow_nan=False).encode('utf-8')))
    if 'error' in result:
        raise ValueError(result['error'])
    changed = result['state']
    for key in STATE_V2:
        if key in changed:
            state[key] = changed[key]
        else:
            state.pop(key, None)
    if result.get('effects'):
        state.setdefault('triggerEffects', []).extend(result['effects'])
    if trace is not None:
        actor = wait['actor'] if wait is not None else event.get('actor', '')
        by_id = {t['id']: t for t in rules}
        for f in result['firings']:
            t = by_id[f['trigger']]
            entry = story_triggers._firing(phase, event if f.get('via') else {**event, 'kind': t['on']}, actor, day, elapsed, t,
                                           f.get('via'))
            if f.get('notes'):
                entry['notes'] = f['notes']
            trace.append(entry)
    for signal in result.get('signals', ()):
        emit(signal, event)
    return result.get('phase')
