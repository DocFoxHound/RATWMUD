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
