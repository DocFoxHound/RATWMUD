"""The same deterministic world-story rules for Studio, authoring tools and the world's runner."""

import contextvars
import hashlib
import json
import math
import re
import threading
import time
from pathlib import Path

import story_scenes
import story_triggers as triggers

SCENE_EVENTS = ('scene arrived', 'scene spoken', 'scene posed', 'scene blocked', 'scene finished', 'scene choice', 'scene response', 'scene failed')
REGION_EVENTS = ('region entered', 'region left')
SCENE_EFFECTS = ('npc.walk', 'npc.speak', 'npc.pose')
# Every event, condition, action and effect, with typed parameters and a sentence each, is one file (doc 85, 3.1 rule 2 and
# Phase 4): the tables below are read from it, so the validator, the Story Creator's sentences and pickers, the console
# and the author bridge all say the same thing.
CATALOGUE_FILE = Path(__file__).resolve().parents[1] / 'Data/Storylines/trigger-catalogue.json'
CATALOGUE = json.loads(CATALOGUE_FILE.read_text(encoding='utf-8'))
_SPECIAL = {e['id'] for e in CATALOGUE['events'] if e.get('special')}
EVENTS = tuple(e['id'] for e in CATALOGUE['events'] if e['id'] not in _SPECIAL)
FIELDS = ('actor', 'target', 'cell', 'item', 'detail')
# Actions here map to existing, bounded world operations. Story-only dialogue expires on phase exit.
EFFECT_PARAMS = {e['id']: tuple(e['params']) for e in CATALOGUE['effects']}
EFFECTS = {kind: tuple(param['key'] for param in params) for kind, params in EFFECT_PARAMS.items()}
# Injuries a Dungeon Master may give (Docs/Design/38-injuries.md, phase 5; the game's Core/RatwInjury.cpp has the same).
INJURY_TYPES = tuple(next(param for param in EFFECT_PARAMS['character.injury'] if param['key'] == 'add')['values'])
ID = re.compile(r'[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}\Z')


# A rehearsal is asked for over Studio and the author bridge, so one request must not take the host's CPU: each review
# runs within a time and work budget, and only a couple run at once. The live runner dispatches outside any budget.
REHEARSAL_SECONDS = 8.0
REHEARSAL_STEPS = 200_000          # Facts and signals dispatched, and rules fired, across all of a review's cases.
REHEARSALS_AT_ONCE = 2
_rehearsals = threading.BoundedSemaphore(REHEARSALS_AT_ONCE)
_budget = contextvars.ContextVar('rehearsal_budget', default=None)


class RehearsalBudget:
    def __init__(self, seconds=None, steps=None):
        self.deadline = time.monotonic() + (REHEARSAL_SECONDS if seconds is None else seconds)
        self.steps = REHEARSAL_STEPS if steps is None else steps

    def spend(self, n=1):
        before, self.steps = self.steps, self.steps - n
        if self.steps < 0 or (before // 256 != self.steps // 256 and time.monotonic() > self.deadline):
            raise ValueError(
                'This rehearsal is too large to run in one request. Use fewer or shorter rehearsal cases, '
                'or fewer signals per event, and try again.'
            )


def require(ok, message):
    if not ok:
        raise ValueError(message)


def shape(value, required, optional=()):
    require(isinstance(value, dict), 'Expected a JSON object.')
    missing = set(required) - value.keys()
    extra = value.keys() - set(required) - set(optional)
    require(not missing and not extra, f'Missing fields {sorted(missing)}, unexpected fields {sorted(extra)}.')


def word(v, size=300):
    return isinstance(v, str) and bool(v.strip()) and len(v) <= size and all(ord(c) >= 32 or c == '\n' for c in v)


def identifier(v):
    return isinstance(v, str) and bool(ID.fullmatch(v))


def integer(v, low, high):
    return type(v) is int and low <= v <= high


def number(v, low, high):
    return type(v) in (int, float) and low <= v <= high


# Mission objectives take evidence from the participant who acted. Ambient events can open or close a phase.
MISSION_EVENTS = tuple(e['id'] for e in CATALOGUE['events'] if e.get('mission'))
MISSION_GATE = 'missions completed'
# Positive amounts actually emitted by the native producers in addition to an occurrence.
MISSION_MEASURES = {m: tuple(e['id'] for e in CATALOGUE['events'] if m in e.get('measures', ())) for m in ('quantity', 'coins')}


def validate_missions(phase):
    missions = phase.get('missions', [])
    require(
        isinstance(missions, list) and len(missions) <= 16,
        'Choose up to 16 missions in a phase.',
    )
    ids = set()
    for m in missions:
        shape(
            m,
            ('id', 'title', 'mode', 'objectives'),
            ('players', 'percent', 'contributors'),
        )
        require(
            identifier(m['id']) and m['id'] not in ids and word(m['title'], 120),
            'Each mission needs a unique ID and title.',
        )
        ids.add(m['id'])
        require(
            m['mode'] in ('individual', 'group', 'either'),
            'Choose individual, group, or either.',
        )
        require(
            'contributors' not in m or (m['mode'] == 'group' and integer(m['contributors'], 2, 500)),
            'A group needs at least two contributors.',
        )
        require(
            not ('players' in m and 'percent' in m),
            'Choose a number or percentage for a personal mission.',
        )
        for key, limit in [('players', 500), ('percent', 100)]:
            require(
                key not in m or (m['mode'] == 'individual' and integer(m[key], 1, limit)),
                'Personal completion thresholds only apply to individual missions.',
            )
        require(
            isinstance(m['objectives'], list) and 1 <= len(m['objectives']) <= 8,
            'A mission needs one to eight ordered objectives.',
        )
        for index, o in enumerate(m['objectives']):
            shape(
                o,
                ('label', 'on'),
                ('match', 'goal', 'measure', 'sameAsFirst', 'signal'),
            )
            require(
                'sameAsFirst' not in o or (index > 0 and o['sameAsFirst'] in FIELDS),
                'Choose a field from the first objective to link a later objective.',
            )
            require(
                word(o['label'], 180)
                and o['on'] in (*MISSION_EVENTS, triggers.SIGNAL)
                and (o['on'] == triggers.SIGNAL) == ('signal' in o)
                and ('signal' not in o or identifier(o['signal'])),
                'Choose a participant event and a journal instruction.',
            )
            require(
                o.get('measure', 'count') in ('count', 'quantity', 'coins') and integer(o.get('goal', 1), 1, 1000000),
                'Choose a positive objective goal and measure.',
            )
            measure = o.get('measure', 'count')
            require(
                measure == 'count' or o['on'] == triggers.SIGNAL or o['on'] in MISSION_MEASURES[measure],
                f'{o["on"]} does not record {measure}. Choose occurrences or another event.',
            )
            fields = o.get('match', {})
            require(
                isinstance(fields, dict) and all(k in FIELDS and word(v, 400) for k, v in fields.items()),
                'Invalid mission event filter.',
            )
    require(
        not missions or phase['exits'] or phase.get('triggers'),
        'A mission needs an ongoing phase.',
    )
    return ids


def track_missions(phase, state, event, credited):
    actor = event.get('actor', '')
    if not phase.get('missions') or actor not in state['cohort']:
        return
    ledger = state.get('missions', {})
    for mission in phase['missions']:
        if mission['id'] in credited:
            continue
        individual = mission['mode'] == 'individual'
        record = ledger.get(mission['id'], {'tracks': {}, 'contributors': []})
        key = actor if individual else 'group'
        step, amount = record['tracks'].get(key, [0, 0])
        objectives = mission['objectives']
        if step == len(objectives):
            continue
        objective = objectives[step]
        evidence = event
        if objective['on'] == 'gift' and event['kind'] == 'gift again':
            evidence = {**event, 'kind': 'gift'}
        if objective.get('signal') != evidence.get('signal') or not matches(objective, evidence):
            continue
        measure = objective.get('measure', 'count')
        gain = 1 if measure == 'count' else event.get(measure, 0)
        if gain <= 0:
            continue
        # A notice answer or returned loan can refer to the exact record that opened the mission.
        # Keep bindings per personal track, or one binding for a shared checklist.
        bindings = record.get('bindings', {}).get(key, {})
        link = objective.get('sameAsFirst')
        if link and (not bindings.get(link) or event.get(link) != bindings[link]):
            continue
        if step == 0:
            fields = {o['sameAsFirst'] for o in objectives if 'sameAsFirst' in o}
            if any(not event.get(f) or event[f] != bindings.get(f, event[f]) for f in fields):
                continue
            if fields and not bindings:
                record.setdefault('bindings', {})[key] = {f: event[f] for f in fields}
        if not individual and actor not in record['contributors']:
            record['contributors'].append(actor)
        goal = objective.get('goal', 1)
        amount = min(amount + gain, goal)
        enough_wolves = mission['mode'] != 'group' or len(record['contributors']) >= mission.get('contributors', 2)
        if amount == goal and (step + 1 < len(objectives) or enough_wolves):
            step, amount = step + 1, 0
        credited.add(mission['id'])
        record['tracks'][key] = [step, amount]
        ledger[mission['id']] = record
        state['missions'] = ledger


def mission_snapshot(phase, state):
    result = []
    gates = {name for edge in phase['exits'] for name in edge.get('missions', [])}
    for m in phase.get('missions', []):
        r = state.get('missions', {}).get(m['id'], {})
        tracks = r.get('tracks', {})
        total = len(m['objectives'])
        individual = m['mode'] == 'individual'
        required = (
            m.get('players', max(1, math.ceil(len(state['cohort']) * m.get('percent', 0) / 100))) if individual else 1
        )
        done = sum(v[0] == total for v in tracks.values())
        result.append(
            {
                'id': m['id'],
                'title': m['title'],
                'mode': m['mode'],
                'objectives': m['objectives'],
                'tracks': tracks,
                'contributors': r.get('contributors', []),
                'requiredContributors': m.get('contributors', 2) if m['mode'] == 'group' else 1,
                'completed': done,
                'required': required,
                'optional': m['id'] not in gates,
                'ready': done >= required,
            }
        )
    return result


def check_region_source(rule, regions):
    if rule['on'] in REGION_EVENTS:
        name = rule.get('match', {}).get('target')
        require(regions and (name is None or name in regions), 'A boundary crossing must use a region in this story.')


def _param_ok(param, v, regions):
    """One effect parameter against its catalogue entry (Data/Storylines/trigger-catalogue.json)."""
    kind = param['type']
    if kind == 'int':
        return integer(v, param['min'], param['max']) and not (param.get('nonzero') and v == 0)
    if kind == 'choice':
        return isinstance(v, str) and v in param['values']
    if kind == 'text':
        return word(v, param['max'])
    if kind == 'region':
        return identifier(v) and v in (regions or {})
    return identifier(v) or (kind == 'wolf' and v == '$actor')


def validate_effect(effect, pid, regions=None, scenes=None):
    kind = effect.get('kind') if isinstance(effect, dict) else None
    require(isinstance(kind, str) and kind in EFFECTS, f'{pid}: unknown effect.')
    shape(effect, ('kind', *EFFECTS[kind]))
    for param in EFFECT_PARAMS[kind]:
        require(_param_ok(param, effect[param['key']], regions), f"{pid}: invalid {kind}.{param['key']}.")
    if kind == 'scene.play':
        scene = (scenes or {}).get(effect['scene'])
        require(
            scene and len(effect['cue']) <= 64, f'{pid}: choose an embedded scene and a cue of up to 64 characters.'
        )
        require(effect['player']!='world' or scene['visibility']=='public', 'A private scene needs a character.')
    if kind == 'faction.relation':
        require(
            effect['faction'] != effect['other'],
            'A faction needs a different counterpart.',
        )


def region_index(program):
    return {r['id']: r for r in program.get('regions', [])}


def effective_phase(program, phase):
    if program.get('scenes'):
        phase = {**phase, '_scenes': {s['id']: s for s in program['scenes']}}
    if program.get('regions'):
        phase = {**phase, '_regions': region_index(program)}
    shared = program.get('triggers', [])
    if shared and (phase.get('campaignRules', bool(phase.get('exits') or phase.get('triggers')))):
        return {
            **phase,
            '_sharedIds': {t['id'] for t in shared},
            'triggers': [*shared, *phase.get('triggers', [])],
        }
    return phase


def phases_of(program):
    return [effective_phase(program, phase) for phase in program['phases']]


def possible_effects(phase):
    return [
        *phase['effects'],
        *(a['effect'] for t in phase.get('triggers', []) for a in t['actions'] if a['do'] == 'effect'),
    ]


def _check_scene(scene, scene_docs):
    story_scenes.validate(scene)
    require(scene['id'] not in scene_docs, 'Embedded scenes need distinct IDs.')
    scene_docs[scene['id']] = scene


def _check_region(r, names):
    shape(r, ('id', 'cell', 'x1', 'y1', 'x2', 'y2'))
    require(
        identifier(r['id']) and r['id'] not in names and identifier(r['cell']),
        'Give each region a unique ID and a cell.',
    )
    require(
        all(integer(r[k], 0, 4095) for k in ('x1', 'y1', 'x2', 'y2')) and r['x1'] <= r['x2'] and r['y1'] <= r['y2'],
        'Choose ordered, inclusive tile bounds for the region.',
    )
    names.add(r['id'])


def _check_header(p):
    require(
        p['format'] == 'ratw-story-program' and type(p['version']) is int and p['version'] == 1,
        'Unknown story format or version.',
    )
    require(
        identifier(p['id']) and word(p['title'], 100),
        'Give this story an ID and title.',
    )
    # (Doc 85, 5.6: a program may set its own "quiet" alert, or 0 for "this one is slow on purpose".)
    require('quietAfterHours' not in p or integer(p['quietAfterHours'], 0, 8760),
            'Quiet after: 0 (never) to 8760 hours.')


def _check_audience(a):
    if isinstance(a, dict) and a.get('kind') == 'active':
        shape(a, ('kind', 'days'))
        require(
            integer(a['days'], 1, 90),
            'Active audience needs a lookback of 1–90 real days.',
        )
    else:
        shape(a, ('kind', 'players'))
        require(
            a['kind'] == 'players'
            and isinstance(a['players'], list)
            and 0 < len(a['players']) <= 500
            and all(identifier(v) for v in a['players'])
            and len(set(a['players'])) == len(a['players']),
            'Choose distinct player character IDs.',
        )


def _check_phase_head(p, phase, ids):
    """A phase's own shape, id, journal, guest voices and number of effects; its effective form (campaign triggers,
    regions and scenes joined)."""
    shape(
        phase,
        ('id', 'journal', 'effects', 'exits'),
        ('guestVoices', 'missions', 'triggers', 'campaignRules'),
    )
    require(
        isinstance(phase.get('triggers', []), list) and type(phase.get('campaignRules', True)) is bool,
        'Invalid phase trigger settings.',
    )
    phase = effective_phase(p, phase)
    pid = phase['id']
    require(
        identifier(pid) and pid not in ids and word(phase['journal'], 600),
        'Each phase needs a unique ID and journal entry.',
    )
    ids.add(pid)
    voices = phase.get('guestVoices', [])
    require(
        isinstance(voices, list)
        and len(voices) <= 8
        and all(identifier(v) for v in voices)
        and len(set(voices)) == len(voices)
        and (not voices or phase['exits'] or phase.get('triggers')),
        f'{pid}: choose up to eight distinct guest speakers in an ongoing phase.',
    )
    require(
        isinstance(phase['effects'], list) and len(phase['effects']) <= 12,
        f'{pid}: too many effects.',
    )
    return phase


def _check_entry_effect(effect, phase, speakers):
    pid = phase['id']
    validate_effect(effect, pid, phase.get('_regions'), phase.get('_scenes'))
    if effect['kind'] == 'npc.line':
        require(
            effect['npc'] not in speakers,
            f'{pid}: two lines for the same resident.',
        )
        speakers.add(effect['npc'])


def _check_exit(e, phase, mission_ids):
    pid = phase['id']
    if isinstance(e, dict) and e.get('on') == MISSION_GATE:
        shape(e, ('on', 'to', 'missions'))
        names = e['missions']
        require(
            identifier(e['to'])
            and isinstance(names, list)
            and names
            and all(identifier(k) and k in mission_ids for k in names)
            and len(set(names)) == len(names),
            'A mission exit must name distinct missions from this phase.',
        )
        return
    shape(
        e,
        ('on', 'to'),
        (
            'match',
            'afterDays',
            'players',
            'percent',
            'minQuantity',
            'minCoins',
            'participant',
            'prerequisites',
        ),
    )
    require(
        e['on'] in EVENTS and identifier(e['to']),
        f'{pid}: unknown trigger or destination.',
    )
    m = e.get('match', {})
    require(
        isinstance(m, dict) and all(k in FIELDS and word(v, 400) for k, v in m.items()),
        f'{pid}: invalid event filter.',
    )
    check_region_source(e, phase.get('_regions'))
    require(
        'participant' not in e or type(e['participant']) is bool,
        f'{pid}: participant must be true or false.',
    )
    for key, hi in [
        ('players', 500),
        ('percent', 100),
        ('minQuantity', 1000000),
        ('minCoins', 1000000000),
    ]:
        if key in e:
            require(integer(e[key], 1, hi), f'{pid}: invalid {key}.')
    require(
        not ('players' in e and 'percent' in e),
        f'{pid}: choose one participation gate.',
    )
    if 'prerequisites' in e:
        steps = e['prerequisites']
        require(
            isinstance(steps, list)
            and 1 <= len(steps) <= 8
            and (e.get('participant') or 'percent' in e or 'players' in e),
            'Prerequisites require a participation gate.',
        )
        for step in steps:
            shape(step, ('on',), ('match',))
            require(
                step['on'] in EVENTS and step['on'] not in ('clock', 'dm signal'),
                'Choose a player event as a prerequisite.',
            )
            fields = step.get('match', {})
            require(
                isinstance(fields, dict) and all(k in FIELDS and word(v, 400) for k, v in fields.items()),
                'Invalid prerequisite filter.',
            )
            check_region_source(step, phase.get('_regions'))
    if 'afterDays' in e or e['on'] == 'clock':
        require(
            number(e.get('afterDays'), 1 / 1440, 36500),
            f'{pid}: set a positive game-time delay.',
        )
    if e['on'] == 'clock':
        require(
            not (set(e) - {'on', 'to', 'afterDays'}),
            f'{pid}: clock exits only take a delay.',
        )
    if e['on'] == 'dm signal':
        require(
            set(e) == {'on', 'to', 'match'} and set(m) == {'detail'} and word(m.get('detail'), 80),
            f'{pid}: name the permitted DM signal.',
        )


def _check_phase(p, raw, ids):
    """One phase whole (validate's loop body)."""
    phase = _check_phase_head(p, raw, ids)
    speakers = set()
    for effect in phase['effects']:
        _check_entry_effect(effect, phase, speakers)
    require(
        isinstance(phase['exits'], list) and len(phase['exits']) <= 16,
        f'{phase["id"]}: too many exits.',
    )
    mission_ids = validate_missions(phase)
    triggers.validate(phase)
    for mission in phase.get('missions', []):
        for objective in mission['objectives']:
            check_region_source(objective, phase.get('_regions'))
    for e in phase['exits']:
        _check_exit(e, phase, mission_ids)


def _check_start(p, ids):
    require(identifier(p['start']) and p['start'] in ids, 'Unknown opening phase.')


def _check_graph(p, ids):
    """Every phase reachable from the opening one; rewards for $actor; a speaking phase has an exit."""
    phases = {s['id']: s for s in phases_of(p)}
    reached, todo = set(), [p['start']]
    while todo:
        pid = todo.pop()
        if pid in reached:
            continue
        require(pid in ids, 'Unknown destination ' + pid)
        reached.add(pid)
        todo.extend(e['to'] for e in triggers.routes(phases[pid]))
    require(reached == ids, 'Unreachable phases: ' + ', '.join(sorted(ids - reached)))
    for phase in phases.values():
        _check_phase_paths(p, phases, phase)


def _check_phase_paths(p, phases, phase):
    if any(effect.get('player') == '$actor' for effect in phase['effects']):
        incoming = [e for other in phases.values() for e in triggers.routes(other) if e['to'] == phase['id']]
        require(
            phase['id'] != p['start']
            and all(
                e['on'] == MISSION_GATE or e.get('participant') or 'players' in e or 'percent' in e
                for e in incoming
            ),
            'A reward for $actor requires a participating player on every incoming path.',
        )
    require(
        phase['exits'] or phase.get('triggers') or all(e['kind'] != 'npc.line' for e in phase['effects']),
        'A speaking phase needs an exit.',
    )


def _check_tests(p):
    require(
        isinstance(p['tests'], list) and 1 <= len(p['tests']) <= 32,
        'Include 1–32 rehearsal cases.',
    )


TOP_REQUIRED = ('format', 'version', 'id', 'title', 'audience', 'start', 'phases', 'tests')
TOP_OPTIONAL = ('triggers', 'regions', 'scenes', 'quietAfterHours')


def validate(p):
    shape(p, TOP_REQUIRED, TOP_OPTIONAL)
    scenes = p.get('scenes', [])
    require(isinstance(scenes, list) and len(scenes) <= 32, 'Choose up to 32 embedded scenes.')
    scene_docs = {}
    for scene in scenes:
        _check_scene(scene, scene_docs)
    rectangles = p.get('regions', [])
    require(isinstance(rectangles, list) and len(rectangles) <= 128, 'Choose up to 128 named regions.')
    names = set()
    for r in rectangles:
        _check_region(r, names)
    _check_header(p)
    _check_audience(p['audience'])
    require(
        isinstance(p['phases'], list) and 1 <= len(p['phases']) <= 100,
        'A story has 1–100 phases.',
    )
    shared = p.get('triggers', [])
    require(
        isinstance(shared, list) and len(shared) <= 64,
        'Choose up to 64 campaign triggers.',
    )
    # Validate campaign rules even when every phase opts out. Mission conditions refer to local journals.
    triggers.validate(campaign_scope(p, scene_docs))
    ids = set()
    for phase in p['phases']:
        _check_phase(p, phase, ids)
    _check_start(p, ids)
    _check_graph(p, ids)
    _check_tests(p)
    return p


def campaign_scope(p, scene_docs=None):
    """The pseudo-phase campaign triggers are checked in (no missions: they refer to a phase's own journal)."""
    if scene_docs is None:
        scene_docs = {s['id']: s for s in p.get('scenes', []) if isinstance(s, dict) and 'id' in s}
    return {'id': 'campaign', 'missions': [], 'triggers': p.get('triggers', []), '_regions': region_index(p), '_scenes': scene_docs}


# ----------------------------------------------------------------------------------------------------- every problem
# Doc 85, Phase 3 ("Explain it") and 4.2's Problems list: every problem of a draft at once, each with where it is, where
# `validate` raises on the first. Built from validate's own checks run part by part, so the two cannot disagree: a draft
# with no error here passes `validate`, and one `validate` refuses has at least one (tools/test_story_problems.py).

_ERRORS = (ValueError, KeyError, TypeError, AttributeError, IndexError, StopIteration)


def problems(p):
    """Every problem of a draft (see _problems); should the walk itself fail on a draft too broken to walk, validate's
    first error stands alone."""
    try:
        return _problems(p)
    except _ERRORS:
        try:
            validate(p)
            return []
        except _ERRORS as error:
            return [{'level': 'error', 'text': triggers._message(error), 'path': '', 'at': {'kind': 'program'}}]


def _problems(p):
    """Every error in a draft, each {level, text, path, at}: `path` names the part ("phases[2].triggers[0].actions[1]"),
    `at` is for an editor to go there ({kind, phase, index, id, part}); then the warnings the program alone can tell
    (signals nothing listens for, counters read that nothing sets, timers cancelled that nothing starts)."""
    out = []

    def add(path, at, text, level='error'):
        out.append({'level': level, 'text': text, 'path': path, 'at': at})

    def attempt(path, at, check, *args):
        try:
            check(*args)
            return True
        except _ERRORS as error:
            add(path, at, triggers._message(error))
            return False

    if not isinstance(p, dict):
        add('', {'kind': 'program'}, 'A story program is a JSON object.')
        return out
    missing, extra = set(TOP_REQUIRED) - p.keys(), p.keys() - set(TOP_REQUIRED) - set(TOP_OPTIONAL)
    if missing or extra:
        add('', {'kind': 'program'}, f'Missing fields {sorted(missing)}, unexpected fields {sorted(extra)}.')
    program = {'kind': 'settings'}
    if {'format', 'version', 'id', 'title'} <= p.keys():
        attempt('', program, _check_header, p)
    if 'audience' in p:
        attempt('audience', program, _check_audience, p['audience'])
    scene_docs = {}
    scenes = p.get('scenes', [])
    if attempt('scenes', {'kind': 'scenes'}, require, isinstance(scenes, list) and len(scenes) <= 32, 'Choose up to 32 embedded scenes.'):
        for i, scene in enumerate(scenes):
            attempt(f'scenes[{i}]', {'kind': 'scene', 'index': i, 'id': scene.get('id') if isinstance(scene, dict) else None},
                    _check_scene, scene, scene_docs)
    names = set()
    rectangles = p.get('regions', [])
    if attempt('regions', {'kind': 'regions'}, require, isinstance(rectangles, list) and len(rectangles) <= 128,
               'Choose up to 128 named regions.'):
        for i, r in enumerate(rectangles):
            attempt(f'regions[{i}]', {'kind': 'region', 'index': i, 'id': r.get('id') if isinstance(r, dict) else None},
                    _check_region, r, names)

    # (From here the draft is walked as validate would see it, its broken lists read as empty, so one broken list does not
    # stop the rest being checked: what was wrong with each list is told above.)
    raw_p, p = p, dict(p)
    for key in ('regions', 'scenes', 'triggers'):
        items = p.get(key, [])
        p[key] = [x for x in items if isinstance(x, dict) and isinstance(x.get('id'), str)] if isinstance(items, list) else []
    if not p['scenes'] and 'scenes' not in raw_p:
        del p['scenes']

    def trigger_list(rules_, scope, path, where, ids_taken=(), joined=True):
        """Each trigger of a list, then the list (its length, its signal cycles) once each is sound (and, for a phase that
        runs them, the campaign triggers joined to it)."""
        if not attempt(path, where, require, isinstance(rules_, list) and len(rules_) <= 64,
                       'Choose up to 64 triggers per phase.' if where.get('phase') is not None else 'Choose up to 64 campaign triggers.'):
            return
        seen, sound = set(ids_taken), True
        for i, t in enumerate(rules_):
            tid = t.get('id') if isinstance(t, dict) and isinstance(t.get('id'), str) else None
            at = {**where, 'kind': 'trigger', 'index': i, 'id': tid}
            if tid in seen:
                add(f'{path}[{i}]', at, 'Give every trigger in the phase a unique ID.' if where.get('phase') is not None
                    else 'Give every campaign trigger a unique ID.')
                sound = False
                continue
            found = triggers.trigger_problems(t, scope, seen - {tid})
            for part, text in found:
                add(f'{path}[{i}]{part}', {**at, **({'part': part[1:]} if part else {})}, text)
            sound = sound and not found
            if tid is not None:
                seen.add(tid)
        if sound and joined:
            attempt(path, where, triggers.validate, scope)
        return sound

    shared = raw_p.get('triggers', [])
    shared_sound = trigger_list(shared, campaign_scope(p, scene_docs), 'triggers', {'phase': None}) and shared == p['triggers']
    shared_ids = {t.get('id') for t in shared if isinstance(t, dict) and isinstance(t.get('id'), str)} if isinstance(shared, list) else set()
    ids = set()
    phases = p.get('phases')
    if not attempt('phases', {'kind': 'phases'}, require, isinstance(phases, list) and 1 <= len(phases) <= 100,
                   'A story has 1–100 phases.'):
        phases = []
    sound_phases = True
    for pi, raw in enumerate(phases):
        pid = raw.get('id') if isinstance(raw, dict) and isinstance(raw.get('id'), str) else None
        where = {'phase': pi, 'phaseId': pid}
        before = len(out)
        try:
            phase = _check_phase_head(p, raw, ids)
        except _ERRORS as error:
            add(f'phases[{pi}]', {**where, 'kind': 'phase'}, triggers._message(error))
            sound_phases = False
            if not isinstance(raw, dict) or not isinstance(raw.get('effects'), list) or not isinstance(raw.get('exits'), list) \
                    or not isinstance(pid, str):
                continue
            ids.add(pid)
            try:
                phase = effective_phase(p, raw)
            except _ERRORS:
                continue
        speakers = set()
        for i, effect in enumerate(phase.get('effects', [])):
            attempt(f'phases[{pi}].effects[{i}]', {**where, 'kind': 'effect', 'index': i}, _check_entry_effect, effect, phase, speakers)
        mission_ids = set()
        missions = phase.get('missions', [])
        if attempt(f'phases[{pi}].missions', {**where, 'kind': 'missions'}, require, isinstance(missions, list) and len(missions) <= 16,
                   'Choose up to 16 missions in a phase.'):
            for i, m in enumerate(missions):
                mid = m.get('id') if isinstance(m, dict) and isinstance(m.get('id'), str) else None
                at = {**where, 'kind': 'mission', 'index': i, 'id': mid}
                if mid in mission_ids:
                    add(f'phases[{pi}].missions[{i}]', at, 'Each mission needs a unique ID and title.')
                    continue
                if attempt(f'phases[{pi}].missions[{i}]', at, validate_missions, {**phase, 'missions': [m]}):
                    for j, objective in enumerate(m['objectives']):
                        attempt(f'phases[{pi}].missions[{i}].objectives[{j}]', {**at, 'part': f'objectives[{j}]'},
                                check_region_source, objective, phase.get('_regions'))
                if mid is not None:
                    mission_ids.add(mid)
        own = raw.get('triggers', []) if isinstance(raw, dict) else []
        trigger_list(own, phase, f'phases[{pi}].triggers', where, shared_ids if phase.get('_sharedIds') else (),
                     joined=bool(shared_sound or not phase.get('_sharedIds')))
        exits = phase.get('exits', [])
        if attempt(f'phases[{pi}].exits', {**where, 'kind': 'exits'}, require, isinstance(exits, list) and len(exits) <= 16,
                   f'{pid}: too many exits.'):
            for i, e in enumerate(exits):
                attempt(f'phases[{pi}].exits[{i}]', {**where, 'kind': 'exit', 'index': i}, _check_exit, e, phase, mission_ids)
        sound_phases = sound_phases and len(out) == before
    if 'start' in p:
        attempt('start', program, _check_start, p, ids)
    if sound_phases and not any(e['level'] == 'error' for e in out):
        # (Reachability and the paths into a phase need every phase sound: a broken one would only add noise.)
        attempt('phases', {'kind': 'phases'}, _check_graph, p, ids)
    if 'tests' in p:
        attempt('tests', {'kind': 'tests'}, _check_tests, p)
        for i, case in enumerate(p['tests'] if isinstance(p['tests'], list) else []):
            at = {'kind': 'case', 'index': i, 'id': case.get('name') if isinstance(case, dict) else None}
            if attempt(f'tests[{i}]', at, _check_case, p, case):
                for j, ev in enumerate(case['events']):
                    attempt(f'tests[{i}].events[{j}]', {**at, 'part': f'events[{j}]'}, _check_fact, ev)
    # The safety net: validate's own verdict. (Rehearsal cases are not validate's: review checks them.)
    structural = [e for e in out if not e['path'].startswith('tests[')]
    try:
        validate(raw_p)
        if structural:
            # (Never a problem validate does not see: the walk above is validate's checks part by part.)
            out = [e for e in out if e['path'].startswith('tests[')]
    except _ERRORS as error:
        if not structural:
            add('', {'kind': 'program'}, triggers._message(error))
    if not any(e['level'] == 'error' for e in out):
        out += program_warnings(p)
    return out


def _signal_uses(p):
    """Where each signal is sent (emit, schedule), heard (story-signal triggers and objectives) and cancelled."""
    sent, heard, cancelled, started = {}, {}, {}, {}
    scopes = [(None, 'triggers', p.get('triggers', []))] + [
        (i, f'phases[{i}].triggers', ph.get('triggers', [])) for i, ph in enumerate(p['phases'])]
    for pi, path, rules_ in scopes:
        for ti, t in enumerate(rules_):
            at = {'phase': pi, 'kind': 'trigger', 'index': ti, 'id': t['id']}
            if t['on'] == triggers.SIGNAL:
                heard.setdefault(t['signal'], []).append((f'{path}[{ti}]', at))
            for ai, a in enumerate(t['actions']):
                where = (f'{path}[{ti}].actions[{ai}]', {**at, 'part': f'actions[{ai}]'})
                if a['do'] in ('emit', 'schedule'):
                    sent.setdefault(a['signal'], []).append(where)
                if a['do'] == 'schedule':
                    started.setdefault(a['signal'], []).append(where)
                if a['do'] == 'cancel':
                    cancelled.setdefault(a['signal'], []).append(where)
    for pi, ph in enumerate(p['phases']):
        for mi, m in enumerate(ph.get('missions', [])):
            for oi, o in enumerate(m['objectives']):
                if 'signal' in o:
                    heard.setdefault(o['signal'], []).append(
                        (f'phases[{pi}].missions[{mi}].objectives[{oi}]', {'phase': pi, 'kind': 'mission', 'index': mi, 'id': m['id']}))
    return sent, heard, cancelled, started


def program_warnings(p):
    """What the program alone can tell is likely a mistake (doc 85, 4.2's warnings; 1.6: "a misspelt name is a new counter
    that reads 0, and nothing warns"): a signal sent that nothing hears, one heard that nothing sends (only the console
    could), a timer cancelled that nothing starts, a counter read that nothing sets."""
    out = []
    sent, heard, cancelled, started = _signal_uses(p)
    for name, places in sorted(sent.items()):
        if name not in heard:
            path, at = places[0]
            out.append({'level': 'warning', 'text': f'Signal "{name}" is sent but nothing listens for it.', 'path': path, 'at': at})
    for name, places in sorted(heard.items()):
        if name not in sent:
            path, at = places[0]
            out.append({'level': 'warning', 'text': f'Nothing in the story sends signal "{name}": only the Campaigns console could.',
                        'path': path, 'at': at})
    for name, places in sorted(cancelled.items()):
        if name not in started:
            path, at = places[0]
            out.append({'level': 'warning', 'text': f'Timer "{name}" is cancelled but nothing starts it.', 'path': path, 'at': at})
    read, written = {}, set()

    def walk(c, where):
        if c['test'] in ('all', 'any'):
            for child in c['conditions']:
                walk(child, where)
        elif c['test'] == 'not':
            walk(c['condition'], where)
        elif c['test'] == 'counter':
            read.setdefault((c['scope'], c['key']), where)

    scopes = [(None, 'triggers', p.get('triggers', []))] + [
        (i, f'phases[{i}].triggers', ph.get('triggers', [])) for i, ph in enumerate(p['phases'])]
    for pi, path, rules_ in scopes:
        for ti, t in enumerate(rules_):
            if 'if' in t:
                walk(t['if'], (f'{path}[{ti}].if', {'phase': pi, 'kind': 'trigger', 'index': ti, 'id': t['id'], 'part': 'if'}))
            written.update((a['scope'], a['key']) for a in t['actions'] if a['do'] in ('set', 'add'))
    for (scope, key), (path, at) in sorted(read.items()):
        if (scope, key) not in written:
            out.append({'level': 'warning', 'path': path, 'at': at,
                        'text': f'Counter "{key}" ({scope}) is read but nothing sets it: it stays 0 (a misspelt name?).'})
    return out


def digest(program):
    return hashlib.sha256(
        json.dumps(program, ensure_ascii=False, allow_nan=False, sort_keys=True, separators=(',', ':')).encode()
    ).hexdigest()


def begin(p, cohort, day):
    return {'phase': p['start'], 'entered': day, 'cohort': sorted(set(cohort)), 'votes': {}, 'visits': 1, 'actor': ''}


def phase_of(p, state):
    return effective_phase(p, next(s for s in p['phases'] if s['id'] == state['phase']))


def matches(condition, event):
    if condition['on'] != event['kind']:
        return False
    for key, expected in condition.get('match', {}).items():
        actual = event.get(key, '')
        if key == 'item' and condition['on'] in ('gift', 'gift again') and not any(mark in expected for mark in '~@'):
            actual = re.split('[~@]', actual, 1)[0]
        if actual != expected:
            return False
    return all(
        event.get(field, 0) >= condition[key]
        for key, field in [('minCoins', 'coins'), ('minQuantity', 'quantity')]
        if key in condition
    )


def advance(p, state, event, day, trace=None, elapsed=0):
    """Dispatch one genuine fact and its named signals in FIFO order, within the current visit."""
    if day < state['entered'] or event['kind'] == triggers.SIGNAL:
        return False
    phase = phase_of(p, state)
    if event['kind'] != 'clock':
        return dispatch(p, phase, state, event, day, trace, elapsed)
    clock = {'kind': 'clock', 'id': event.get('id')}
    if dispatch(p, phase, state, clock, day, trace, elapsed):
        return True
    timers = state.get('timers', {})
    for name, timer in sorted(
        timers.items(),
        key=lambda pair: (pair[1].get('clock', 'game'), pair[1]['at'], pair[0]),
    ):
        now = elapsed if timer.get('clock') == 'real' else day
        if timer['at'] > now:
            continue
        if timers.get(name) is not timer:
            continue  # An earlier timer cancelled or replaced this one.
        if timer['interval']:
            timer['at'] = now + timer['interval']
        else:
            del timers[name]
        signal = {
            **clock,
            'kind': triggers.SIGNAL,
            'signal': timer.get('signal', name),
            'actor': timer['actor'],
        }
        if dispatch(p, phase, state, signal, day, trace, elapsed):
            return True  # Remaining campaign timers are still due in the new visit.
    return False


def dispatch(p, phase, state, event, day, trace, elapsed):
    queue = [event]
    emitted, credited = set(), set()

    def emit(name, source):
        if name not in emitted:
            emitted.add(name)
            queue.append({**source, 'kind': triggers.SIGNAL, 'signal': name})

    budget = _budget.get()
    for fact in queue:
        if budget:
            budget.spend()
        track_missions(phase, state, fact, credited)
        trace_start = len(trace) if trace is not None else 0
        target = triggers.fire(
            phase,
            state,
            fact,
            day,
            elapsed,
            lambda: {m['id'] for m in mission_snapshot(phase, state) if m['ready']},
            matches,
            emit,
            trace,
        )
        if budget and trace is not None:
            budget.spend(len(trace) - trace_start)   # (The trace is the reply: it is bounded with the work.)
        if trace is not None and fact.get('cell') and fact.get('x') is not None and fact.get('y') is not None:
            for firing in trace[trace_start:]:
                firing.update(cell=fact['cell'], x=fact['x'], y=fact['y'])
        target = target or exit_for(phase, state, fact, day)
        if target:
            state.pop('previousMissions', None)
            if phase.get('missions'):
                state['previousMissions'] = mission_snapshot(phase, state)
            state.pop('missions', None)
            state.pop('fired', None)
            if 'timers' in state:
                state['timers'] = {k: v for k, v in state['timers'].items() if v.get('scope') == 'campaign'}
            state.update(
                phase=target,
                entered=day,
                votes={},
                progress={},
                visits=state['visits'] + 1,
                actor=fact.get('actor', ''),
            )
            return True
    return False


def exit_for(phase, state, event, day):
    ready = None
    for index, edge in enumerate(phase['exits']):
        actor = event.get('actor', '')
        if (edge.get('participant') or 'players' in edge or 'percent' in edge) and actor not in state['cohort']:
            continue
        before = edge.get('prerequisites', [])
        if before:
            progress = state.setdefault('progress', {}).setdefault(str(index), {})
            step = progress.get(actor, 0)
            if step < len(before):
                if matches(before[step], event):
                    progress[actor] = step + 1
                continue
        if edge['on'] == MISSION_GATE:
            if actor not in state['cohort']:
                continue
            if ready is None:
                ready = {m['id'] for m in mission_snapshot(phase, state) if m['ready']}
            if not ready.issuperset(edge['missions']):
                continue
        elif day < state['entered'] + edge.get('afterDays', 0) or not matches(edge, event):
            continue
        if 'players' in edge or 'percent' in edge:
            votes = state['votes'].setdefault(str(index), [])
            if actor not in votes:
                votes.append(actor)
            needed = edge.get(
                'players',
                max(1, math.ceil(len(state['cohort']) * edge.get('percent', 100) / 100)),
            )
            if len(votes) < needed:
                continue
        return edge['to']
    return None


def record_entry(p, state, path, intended, visit_credits):
    if state.get('previousMissions'):
        visit_credits.append(
            {
                'phase': path[-1],
                'visit': state['visits'] - 1,
                'credits': mission_credits(state['previousMissions']),
            }
        )
    path.append(state['phase'])
    for effect in phase_of(p, state)['effects']:
        effect = dict(effect)
        if effect.get('player') == '$actor':
            effect['player'] = state['actor']
        intended.append({'phase': state['phase'], **effect})


def mission_credits(missions):
    credits = {}
    for mission in missions:
        if mission['mode'] == 'individual':
            total = len(mission['objectives'])
            credits[mission['id']] = sorted(k for k, v in mission['tracks'].items() if v[0] == total)
        else:
            credits[mission['id']] = sorted(mission['contributors']) if mission['ready'] else []
    return credits


def _check_case(p, case):
    """A rehearsal case's shape, name, audience and lists (review's checks, before it runs the case)."""
    shape(
        case,
        ('name', 'cohort', 'events', 'expect'),
        (
            'expectEffects',
            'expectMissions',
            'expectMissionVisits',
            'expectTriggers',
            'expectCounters',
        ),
    )
    require(
        word(case['name'], 100)
        and isinstance(case['cohort'], list)
        and len(case['cohort']) <= 500
        and all(identifier(v) for v in case['cohort']),
        'Invalid rehearsal name or cohort.',
    )
    if p['audience']['kind'] == 'players':
        require(
            set(case['cohort']) == set(p['audience']['players']),
            'Use the fixed audience in the rehearsal.',
        )
    require(
        isinstance(case['events'], list)
        and len(case['events']) <= 256
        and isinstance(case['expect'], list)
        and all(identifier(v) for v in case['expect']),
        'Invalid rehearsal events or expected phases.',
    )


def _check_fact(ev):
    """One rehearsal fact's shape and values."""
    shape(ev, ('id', 'kind', 'day'), (*FIELDS, 'quantity', 'coins', 'elapsedDays', 'x', 'y'))
    require(
        identifier(ev['id'])
        and ev['kind'] in EVENTS
        and (
            ('x' not in ev and 'y' not in ev)
            or (identifier(ev.get('cell')) and integer(ev.get('x'), 0, 4095) and integer(ev.get('y'), 0, 4095))
        )
        and number(ev['day'], 0, 1e8)
        and all(isinstance(ev[k], str) and len(ev[k]) <= 400 for k in FIELDS if k in ev)
        and all(integer(ev[k], -(10**12), 10**12) for k in ('quantity', 'coins') if k in ev),
        'Invalid rehearsal fact.',
    )


def review(p, budget=None):
    """Rehearse every case, within a work and time budget (RehearsalBudget). Too large a request is refused politely."""
    if not _rehearsals.acquire(timeout=REHEARSAL_SECONDS):
        raise ValueError('Other rehearsals are running. Try again in a moment.')
    token = _budget.set(budget or RehearsalBudget())
    try:
        return _review(p)
    finally:
        _budget.reset(token)
        _rehearsals.release()


def _play_case(p, case, step=None):
    """One rehearsal case, fact by fact (as the runner would take them). `step(fact, state, path, trace, intended)` is
    called after each fact, for the workbench's step-through (doc 85, 4.5)."""
    _check_case(p, case)
    state = begin(p, case['cohort'], 0)
    path, intended, last_day, seen = [], [], 0, {}
    visit_credits, trace = [], []
    elapsed = 0

    record_entry(p, state, path, intended, visit_credits)
    if step:
        step(None, state, path, trace, intended)
    for ev in case['events']:
        _check_fact(ev)
        if ev['id'] in seen:
            require(ev == seen[ev['id']], 'A repeated event ID has different contents.')
            if step:
                step(ev, state, path, trace, intended)
            continue
        require(ev['day'] >= last_day, 'Rehearsal game time must move forward.')
        seen[ev['id']] = ev
        require(
            number(ev.get('elapsedDays', elapsed), elapsed, 1e8),
            'Rehearsal elapsed real days must move forward.',
        )
        elapsed = ev.get('elapsedDays', elapsed)
        last_day = ev['day']
        dispatch = [ev] if ev['kind'] in ('clock', 'dm signal') else [{'kind': 'clock'}, ev]
        for fact in dispatch:
            while True:
                source_phase = state['phase']
                moved = advance(p, state, fact, last_day, trace, elapsed)
                intended.extend({'phase': source_phase, **effect} for effect in state.pop('triggerEffects', []))
                if moved:
                    record_entry(p, state, path, intended, visit_credits)
                if not moved or fact['kind'] != 'clock':
                    break
        if step:
            step(ev, state, path, trace, intended)
    missions = mission_snapshot(phase_of(p, state), state)
    credits = mission_credits(missions or state.get('previousMissions', []))
    if missions:
        visit_credits.append({'phase': state['phase'], 'visit': state['visits'], 'credits': credits})
    expected_effects = case.get('expectEffects', intended)
    require(isinstance(expected_effects, list), 'Expected effects must be a list.')
    return {
        'name': case['name'],
        'passed': path == case['expect']
        and expected_effects == intended
        and credits == case.get('expectMissions', credits)
        and visit_credits == case.get('expectMissionVisits', visit_credits)
        and [f['trigger'] for f in trace] == case.get('expectTriggers', [f['trigger'] for f in trace])
        and state.get('counters', {}) == case.get('expectCounters', state.get('counters', {})),
        'triggers': trace,
        'counters': state.get('counters', {}),
        'missionCredits': credits,
        'missionVisits': visit_credits,
        'path': path,
        'expected': case['expect'],
        'effects': intended,
    }


def rehearse_steps(p, index, budget=None, look=None):
    """Step through one rehearsal case (doc 85, 4.5, Phase 4): the state after the opening and after each fact: the phase
    and visit, the path so far, counters, timers, the triggers that fired and the effects sent by that fact, the missions,
    and the phase's journal. `look(state)` adds what the console's explain says of the phase as it then stands."""
    if not _rehearsals.acquire(timeout=REHEARSAL_SECONDS):
        raise ValueError('Other rehearsals are running. Try again in a moment.')
    token = _budget.set(budget or RehearsalBudget())
    try:
        validate(p)
        require(type(index) is int and 0 <= index < len(p['tests']), 'Choose a rehearsal case.')
        steps, marks = [], {'trace': 0, 'effects': 0}

        def step(fact, state, path, trace, intended):
            phase = phase_of(p, state)
            snap = json.loads(json.dumps({k: v for k, v in state.items() if k not in ('cohort',)}, default=sorted))
            steps.append({
                'fact': fact, 'phase': state['phase'], 'visit': state['visits'], 'path': list(path),
                'journal': phase.get('journal', ''), 'counters': snap.get('counters', {}), 'timers': snap.get('timers', {}),
                'fired': trace[marks['trace']:], 'effects': intended[marks['effects']:],
                'missions': mission_snapshot(phase, state), 'ending': not phase['exits'] and not phase.get('triggers'),
                **({'listening': look(state)} if look else {}),
            })
            marks['trace'], marks['effects'] = len(trace), len(intended)

        report = _play_case(p, p['tests'][index], step)
        return {'case': report['name'], 'passed': report['passed'], 'expected': report['expected'], 'steps': steps,
                'cohort': sorted(set(p['tests'][index]['cohort']))}
    finally:
        _budget.reset(token)
        _rehearsals.release()


def _review(p):
    validate(p)
    reports = [_play_case(p, case) for case in p['tests']]
    covered = {phase for case in reports for phase in case['path']}
    fired = {
        (t['scope'], t['phase'] if t['scope'] == 'phase' else '', t['trigger'])
        for case in reports
        for t in case['triggers']
    }
    return {
        'digest': digest(p),
        'uncoveredTriggers': [
            'Campaign / ' + t['id'] for t in p.get('triggers', []) if ('campaign', '', t['id']) not in fired
        ]
        + [
            s['id'] + '/' + t['id']
            for s in p['phases']
            for t in s.get('triggers', [])
            if ('phase', s['id'], t['id']) not in fired
        ],
        'uncovered': [s['id'] for s in p['phases'] if s['id'] not in covered],
        'passed': all(r['passed'] for r in reports),
        'tests': reports,
        'voices': [{'phase': s['id'], 'npc': npc} for s in p['phases'] for npc in s.get('guestVoices', [])],
        'effects': [{'phase': s['id'], **e} for s in phases_of(p) for e in possible_effects(s)],
    }
