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


_NEEDS = {}


def _needs(rules):
    """(Cached by the rules list: a phase's own list lives as long as its program is read.)"""
    hit = _NEEDS.get(id(rules))
    if hit is not None and hit[0] is rules:
        return hit[1]
    if len(_NEEDS) > 256:
        _NEEDS.clear()
    needs = _needs_of(rules)
    _NEEDS[id(rules)] = (rules, needs)
    return needs


def _needs_of(rules):
    """What a version 2 dispatch's rules read beyond the event (doc 85, Phase 8): the world's facts (and of which wolves: the
    acting one, those named, or every one when a count or the looped wolf is read), and the story's quests."""
    import story_triggers

    out = {'facts': False, 'quests': False, 'all': False, 'named': set(), 'audience': story_triggers.uses_audience(rules)}
    for t in rules:
        # (Doc 86, R1: a test for each wolf may read the quests.)
        wheres = [c for w in story_triggers.wheres_in(t) for c in story_triggers.conditions_in(w)]
        for c in [*story_triggers.rule_conditions(t), *wheres]:
            kind = c.get('test')
            if kind in story_triggers.QUEST_V2:
                out['quests'] = True
            elif kind in story_triggers.FACTS_V2:
                out['facts'] = True
                for key in ('who', 'other'):
                    v = c.get(key)
                    if v == '$each':
                        out['all'] = True
                    elif isinstance(v, str) and v != '$actor':
                        out['named'].add(v)
                if kind == 'count in region':
                    out['all'] = True
    return out


def _trim_facts(facts, needs, actor):
    """The world's facts as the native twin is sent them: only the wolves its conditions can read (the same answers, a
    smaller request: a world's facts may list hundreds of wolves)."""
    if not needs['facts']:
        return {}
    wolves = facts.get('wolves')
    if needs['all'] or not isinstance(wolves, dict):
        return facts
    keep = {actor, *needs['named']}
    return {**facts, 'wolves': {k: v for k, v in wolves.items() if k in keep}}


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
    needs = _needs(rules)
    if needs['audience']:
        # (Doc 86, C2 and R1: the audience as the Python twin counts it, the active and online wolves by id.)
        source_event = wait['event'] if wait is not None else event
        aud = story_triggers.audience_of(state, day, phase.get('_activeDays', story_triggers.ACTIVE_DAYS),
                                         story_triggers.facts_of(source_event))
        request['audience'] = {**aud, 'online': None if aud['online'] is story_triggers.MISSING else aud['online']}
    if needs['quests'] and phase.get('_questIndex'):
        # (Doc 85, Phase 8: "quest is" and "objective is" read the copies, the offers, and of each quest its scope and hidden
        # objectives.)
        request['questDefs'] = {qid: {'scope': q.get('scope', 'player'),
                                      'objectives': [{'id': o['id'], 'hidden': bool(o.get('hidden'))} for o in q['objectives']]}
                                for qid, q in phase['_questIndex'].items()}
        request['quests'] = state.get('quests', {})
        request['questOffers'] = state.get('questOffers', {})
    source = request['resume']['event'] if wait is not None else event
    if 'facts' in source:
        trimmed = _trim_facts(source['facts'], needs, wait['actor'] if wait is not None else event.get('actor', ''))
        if wait is not None:
            request['resume'] = {**wait, 'event': {**wait['event'], 'facts': trimmed}}
        else:
            request['fact'] = {**event, 'facts': trimmed}
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
