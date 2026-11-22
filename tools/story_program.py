"""The same deterministic world-story rules for Studio, authoring tools and the world's runner."""

import contextvars
import hashlib
import json
import math
import re
import threading
import time
from pathlib import Path

import story_quests as quests
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
        not missions or ongoing(phase),
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
    if kind == 'bool':
        return type(v) is bool
    return identifier(v) or (kind == 'wolf' and v == '$actor')


def validate_effect(effect, pid, regions=None, scenes=None):
    kind = effect.get('kind') if isinstance(effect, dict) else None
    require(isinstance(kind, str) and kind in EFFECTS, f'{pid}: unknown effect.')
    # (Doc 85, Phase 6: a parameter marked "optional" may be left out.)
    shape(effect, ('kind', *(p['key'] for p in EFFECT_PARAMS[kind] if not p.get('optional'))),
          tuple(p['key'] for p in EFFECT_PARAMS[kind] if p.get('optional')))
    for param in EFFECT_PARAMS[kind]:
        if param.get('optional') and param['key'] not in effect:
            continue
        require(_param_ok(param, effect[param['key']], regions), f"{pid}: invalid {kind}.{param['key']}.")
    if kind == 'character.grant':
        require(quests.check_purse(effect['purse']), f'{pid}: name a purse that pays: town:<town>, town:<town>:works, till:<id> or chapter:<id>.')
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


def variables_of(program):
    """A version 2 program's declared variables by name (doc 85, 3.2): the sound declarations only."""
    decls = program.get('variables', [])
    return {d['id']: d for d in decls if isinstance(d, dict) and isinstance(d.get('id'), str)
            and d.get('type') in triggers.VAR_TYPES and d.get('scope') in triggers.SCOPES} if isinstance(decls, list) else {}


def effective_phase(program, phase):
    if program.get('version') == 2:
        phase = {**phase, '_v': 2, '_vars': variables_of(program)}
        if program.get('quests'):
            # (Doc 85, Phase 6: the story's quests, for the effects that name them; those open in this phase keep it going.)
            phase.update(_questIndex=quests.index(program),
                         _quests=[q['id'] for q in quests.open_quests(program, phase.get('id'))])
    if program.get('scenes'):
        phase = {**phase, '_scenes': {s['id']: s for s in program['scenes']}}
    if program.get('regions'):
        phase = {**phase, '_regions': region_index(program)}
    shared = program.get('triggers', [])
    if shared and (phase.get('campaignRules', bool(phase.get('exits') or phase.get('triggers') or phase.get('_quests')))):
        return {
            **phase,
            '_sharedIds': {t['id'] for t in shared},
            'triggers': [*shared, *phase.get('triggers', [])],
        }
    return phase


def ongoing(phase):
    """A phase that goes on (not an ending): it has exits or triggers, or quests open in it (a small quest's one phase)."""
    return bool(phase.get('exits') or phase.get('triggers') or phase.get('_quests'))


def phases_of(program):
    return [effective_phase(program, phase) for phase in program['phases']]


def possible_effects(phase):
    return [
        *phase['effects'],
        *(a['effect'] for t in phase.get('triggers', []) for _, a in triggers.walk(t['actions']) if a['do'] == 'effect'),
        # (Doc 85, Phase 6: a quest's rewards may be given in any phase: its copies outlast the phase they began in.)
        *(e for q in phase.get('_questIndex', {}).values() for e in (*q.get('rewards', ()), *q.get('bonus', ()))),
    ]


def _indexed(p, phase):
    """A phase (or the campaign scope) with what checking a version 2 trigger needs to know of the whole program: the
    phases, and each phase's own triggers (Go to, Phase is, Enable and Disable name them)."""
    if p.get('version') != 2 or not isinstance(p.get('phases'), list):
        return phase
    phases = [ph for ph in p['phases'] if isinstance(ph, dict) and isinstance(ph.get('id'), str)]
    return {**phase, '_phaseIds': {ph['id'] for ph in phases},
            '_phaseTriggers': {ph['id']: {t['id'] for t in ph.get('triggers', ()) if isinstance(t, dict) and isinstance(t.get('id'), str)}
                               for ph in phases if isinstance(ph.get('triggers', []), list)}}


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
        p['format'] == 'ratw-story-program' and type(p['version']) is int and p['version'] in (1, 2),
        'Unknown story format or version.',
    )
    require(p['version'] == 2 or 'variables' not in p, 'Declared variables need a version 2 story.')
    require(
        identifier(p['id']) and word(p['title'], 100),
        'Give this story an ID and title.',
    )
    # (Doc 85, 5.6: a program may set its own "quiet" alert, or 0 for "this one is slow on purpose".)
    require('quietAfterHours' not in p or integer(p['quietAfterHours'], 0, 8760),
            'Quiet after: 0 (never) to 8760 hours.')


def _check_audience(a):
    if isinstance(a, dict) and a.get('kind') in quests.JOINABLE:
        # (Doc 85, 3.6 and Phase 6: open, nobody at first, wolves come in by taking a quest or by participant.add;
        # everyone, the active wolves and each wolf who signs in. Up to `cap` wolves, 5,000 at most.)
        shape(a, ('kind',) + (('days',) if a['kind'] == 'everyone' else ()), ('cap',))
        require(a['kind'] == 'open' or integer(a['days'], 1, 90), 'Everyone: the wolves active in the last 1–90 real days, and each who signs in.')
        require('cap' not in a or integer(a['cap'], 1, quests.AUDIENCE_CAP), f'An audience of 1 to {quests.AUDIENCE_CAP} wolves at most.')
        return
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
        and (not voices or ongoing(phase)),
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
    require(effect['kind'] not in quests.STORY_EFFECTS, f'{pid}: {effect["kind"]} goes in a trigger, not in what a phase sends as it opens.')
    quests.check_story_effect(effect, None, None)
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
    triggers.validate(_indexed(p, phase))
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
        ongoing(phase) or all(e['kind'] != 'npc.line' for e in phase['effects']),
        'A speaking phase needs an exit.',
    )


def _check_tests(p):
    require(
        isinstance(p['tests'], list) and 1 <= len(p['tests']) <= 32,
        'Include 1–32 rehearsal cases.',
    )


TOP_REQUIRED = ('format', 'version', 'id', 'title', 'audience', 'start', 'phases', 'tests')
TOP_OPTIONAL = ('triggers', 'regions', 'scenes', 'quietAfterHours', 'variables', 'quests')
VARIABLES_MAX = 64


def _initial_ok(kind, v, regions):
    if kind in triggers.LISTS:
        return (isinstance(v, list) and len(v) <= triggers.LIST_MAX and len({json.dumps(x) for x in v}) == len(v)
                and all(_initial_ok(triggers.LISTS[kind], x, regions) for x in v))
    if kind == 'number':
        return integer(v, -triggers.LIMIT, triggers.LIMIT)
    if kind == 'switch':
        return type(v) is bool
    if kind == 'text':
        return isinstance(v, str) and len(v) <= triggers.TEXT_MAX and all(ord(c) >= 32 for c in v)
    if kind == 'region':
        return v in regions
    return identifier(v)


def _check_variable(d, names, regions):
    """One declared variable (doc 85, 3.2): a unique name, a type, a scope, and its first value, a note and a label."""
    shape(d, ('id', 'type', 'scope'), ('initial', 'note', 'show', 'clock'))
    require(identifier(d['id']) and d['id'] not in names, 'Give every variable a unique name.')
    require(d['type'] in triggers.VAR_TYPES and d['scope'] in triggers.SCOPES,
            'A variable has a type (' + ', '.join(triggers.VAR_TYPES) + ') and a scope (story, player, party, world).')
    require('clock' not in d or (d['type'] == 'timer' and d['clock'] in ('game', 'real')), "A timer variable's clock is game or real.")
    require('note' not in d or word(d['note'], 300), "A variable's note is up to 300 characters.")
    require('show' not in d or (word(d['show'], 60) and d['type'] in ('number', 'switch', 'timer')),
            'A number, switch or timer may be shown, under a label of up to 60 characters.')
    require('initial' not in d or (d['type'] != 'timer' and _initial_ok(d['type'], d['initial'], regions)),
            f'The first value of "{d["id"]}" is not a {d["type"]}' + (' (a timer starts unset).' if d['type'] == 'timer' else '.'))
    names.add(d['id'])


def _check_variables(p):
    decls = p.get('variables', [])
    require(isinstance(decls, list) and len(decls) <= VARIABLES_MAX, f'Declare up to {VARIABLES_MAX} variables.')
    names, regions = set(), set(region_index(p))
    for d in decls:
        _check_variable(d, names, regions)


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
    _check_variables(p)
    _check_audience(p['audience'])
    quests.check_quests(p)
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
    triggers.validate(_indexed(p, campaign_scope(p, scene_docs)))
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
    scope = {'id': 'campaign', 'missions': [], 'triggers': p.get('triggers', []), '_regions': region_index(p), '_scenes': scene_docs}
    if p.get('version') == 2:
        scope.update(_v=2, _vars=variables_of(p))
    return scope


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

    decls = p.get('variables', [])
    if 'variables' in p and attempt('variables', {'kind': 'variables'}, require,
                                    isinstance(decls, list) and len(decls) <= VARIABLES_MAX, f'Declare up to {VARIABLES_MAX} variables.'):
        taken = set()
        for i, d in enumerate(decls):
            attempt(f'variables[{i}]', {'kind': 'variable', 'index': i, 'id': d.get('id') if isinstance(d, dict) else None},
                    _check_variable, d, taken, names)

    # (Doc 85, Phase 6: each quest's problems where it is.)
    qs = p.get('quests', [])
    if 'quests' in p and attempt('quests', {'kind': 'quests'}, require, isinstance(qs, list) and len(qs) <= quests.QUESTS_MAX
                                 and (not qs or p.get('version') == 2), f'Choose up to {quests.QUESTS_MAX} quests (a version 2 story).'):
        taken = set()
        for i, q in enumerate(qs):
            attempt(f'quests[{i}]', {'kind': 'quest', 'index': i, 'id': q.get('id') if isinstance(q, dict) else None},
                    quests.check_quest, q, p, taken)

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
    shared_sound = trigger_list(shared, _indexed(p, campaign_scope(p, scene_docs)), 'triggers', {'phase': None}) and shared == p['triggers']
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
        trigger_list(own, _indexed(p, phase), f'phases[{pi}].triggers', where, shared_ids if phase.get('_sharedIds') else (),
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
            for address, a in triggers.walk(t['actions']):
                part = triggers.part_of(address)
                where = (f'{path}[{ti}].{part}', {**at, 'part': part})
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
            written.update((a['scope'], a['key']) for _, a in triggers.walk(t['actions']) if a['do'] in ('set', 'add') and 'key' in a)
    for (scope, key), (path, at) in sorted(read.items()):
        if (scope, key) not in written:
            out.append({'level': 'warning', 'path': path, 'at': at,
                        'text': f'Counter "{key}" ({scope}) is read but nothing sets it: it stays 0 (a misspelt name?).'})
    if p.get('version') == 2:
        used = variable_uses(p)
        for i, d in enumerate(p.get('variables', [])):
            if d['id'] not in used:
                out.append({'level': 'warning', 'path': f'variables[{i}]', 'at': {'kind': 'variable', 'index': i, 'id': d['id']},
                            'text': f'Variable "{d["id"]}" is declared but nothing reads or changes it.'})
    return out


def variable_uses(p):
    """Version 2: where each declared variable is read or changed, by name: a list of (path, at, how) with how 'read' or
    'write'."""
    out = {}
    scopes = [(None, 'triggers', p.get('triggers', []))] + [
        (i, f'phases[{i}].triggers', ph.get('triggers', [])) for i, ph in enumerate(p['phases'])]
    reads = ('var', 'count', 'since')

    def condition(c, path, at):
        for x in triggers.conditions_in(c):
            for key in ('var', 'list'):
                if key in x and x.get('test') in triggers.CONDITIONS_V2:
                    out.setdefault(x[key], []).append((path, at, 'read'))
            for side in ('left', 'right', 'value'):
                for o in triggers.operands_in(x.get(side)):
                    for key in reads:
                        if key in o:
                            out.setdefault(o[key], []).append((path, at, 'read'))

    for pi, path, rules_ in scopes:
        for ti, t in enumerate(rules_):
            at = {'phase': pi, 'kind': 'trigger', 'index': ti, 'id': t['id']}
            if 'if' in t:
                condition(t['if'], f'{path}[{ti}].if', {**at, 'part': 'if'})
            for address, a in triggers.walk(t['actions']):
                part = triggers.part_of(address)
                where = (f'{path}[{ti}].{part}', {**at, 'part': part})
                if a['do'] == 'if':
                    for c in [a['if'], *(b['if'] for b in a.get('elif', ()))]:
                        condition(c, *where)
                if 'var' in a and a['do'] != 'each':
                    out.setdefault(a['var'], []).append((*where, 'write'))
                if 'list' in a:
                    out.setdefault(a['list'], []).append((*where, 'read'))
                for o in triggers.operands_in(a.get('value')):
                    for key in reads:
                        if key in o:
                            out.setdefault(o[key], []).append((*where, 'read'))
    return out


def digest(program):
    return hashlib.sha256(
        json.dumps(program, ensure_ascii=False, allow_nan=False, sort_keys=True, separators=(',', ':')).encode()
    ).hexdigest()


def begin(p, cohort, day, seed=None):
    state = {'phase': p['start'], 'entered': day, 'cohort': sorted(set(cohort)), 'votes': {}, 'visits': 1, 'actor': ''}
    if p.get('version') == 2:
        # (Version 2: the seed of its rolls (the run's id; a rehearsal's own or the program's), the day it began, and the
        # triggers that start turned off.)
        state.update(seed=seed or p['id'], began=day)
        off = triggers.initially_off(p['phases'], p.get('triggers', []))
        if off:
            state['disabled'] = off
    return state


def leave_visit(phase, state, target, day, actor, p=None):
    """A run moves to another phase: the closing visit's missions kept, its timers, once-marks, counts and waits dropped
    (a campaign trigger's or timer's stay); with the program, its quests that last a phase end (doc 85, Phase 6)."""
    if p is not None and quests.relevant(p):
        quests.leave_visit(p, state, target, day)
    state.pop('previousMissions', None)
    if phase.get('missions'):
        state['previousMissions'] = mission_snapshot(phase, state)
    state.pop('missions', None)
    state.pop('fired', None)
    if 'timers' in state:
        state['timers'] = {k: v for k, v in state['timers'].items() if v.get('scope') == 'campaign'}
    if 'counts' in state:
        state['counts'].pop('visit', None)
    if 'waiting' in state:
        state['waiting'] = [w for w in state['waiting'] if w['scope'] == 'campaign']
        if not state['waiting']:
            del state['waiting']
    state.update(
        phase=target,
        entered=day,
        votes={},
        progress={},
        visits=state['visits'] + 1,
        actor=actor,
    )


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
    """Dispatch one genuine fact and its named signals in FIFO order, within the current visit. (A finished run reads
    nothing more; a clock fact also resumes the version 2 waits that are due, after the timers.)"""
    if day < state['entered'] or event['kind'] == triggers.SIGNAL or state.get('outcome'):
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
        if state.get('outcome'):
            return False
    while state.get('waiting'):
        # (The soonest due wait each time, game clock first, then the order they began: a resumed one can start another,
        # never due at once, since the shortest wait is a minute or 5 seconds.)
        waiting = state['waiting']
        due = [(w['clock'], w['at'], i) for i, w in enumerate(waiting) if w['at'] <= (elapsed if w['clock'] == 'real' else day)]
        if not due:
            break
        index = min(due)[2]
        wait = waiting[index]
        state['waiting'] = waiting[:index] + waiting[index + 1:]
        if not state['waiting']:
            del state['waiting']
        if dispatch(p, phase, state, wait['event'], day, trace, elapsed, wait):
            return True
        if state.get('outcome'):
            return False
    return False


def dispatch(p, phase, state, event, day, trace, elapsed, wait=None):
    """One fact and the signals it sends, in order. With `wait`, the first is a waiting trigger's remaining actions
    (the fact it waited with is not tracked or tried again)."""
    queue = [event]
    emitted, credited = set(), set()

    def emit(name, source):
        if name not in emitted:
            emitted.add(name)
            queue.append({**source, 'kind': triggers.SIGNAL, 'signal': name})

    budget = _budget.get()
    # (Doc 85, Phase 6: quests. Their own facts, "quest started", "objective completed"..., follow the fact that caused
    # them in the queue, as signals do; the story effects a firing queued are kept by the story, never sent.)
    quest_run = bool(p.get('quests')) or quests.joinable(p)
    story_effects = p.get('version') == 2
    for n, fact in enumerate(queue):
        if budget:
            budget.spend()
        resuming = wait is not None and n == 0
        if not resuming:
            track_missions(phase, state, fact, credited)
            if quest_run:
                quests.Run(p, phase, state, day, elapsed, queue.append).track(fact)
        trace_start = len(trace) if trace is not None else 0
        ready = lambda: {m['id'] for m in mission_snapshot(phase, state) if m['ready']}
        if resuming:
            target = triggers.resume(phase, state, wait, day, elapsed, ready, emit, trace)
        else:
            target = triggers.fire(phase, state, fact, day, elapsed, ready, matches, emit, trace)
        if story_effects:
            quests.take_story_effects(p, phase, state, day, elapsed, queue.append)
        if budget and trace is not None:
            budget.spend(len(trace) - trace_start)   # (The trace is the reply: it is bounded with the work.)
        if trace is not None and fact.get('cell') and fact.get('x') is not None and fact.get('y') is not None:
            for firing in trace[trace_start:]:
                firing.update(cell=fact['cell'], x=fact['x'], y=fact['y'])
        if state.get('outcome'):
            return False   # (Version 2's finish: the run has ended; nothing more is tried.)
        target = target or (None if resuming else exit_for(phase, state, fact, day))
        if target:
            leave_visit(phase, state, target, day, fact.get('actor', ''), p)
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
            # (Version 2, doc 85 Phase 5: the seed of its rolls, rolls pinned in order (0-99 each), the parties of its
            # wolves, the variables expected at the end (only those named), and how the run ended.)
            'seed',
            'rolls',
            'parties',
            'expectVariables',
            'expectOutcome',
            # (Doc 85, Phase 6: each quest's copies expected at the end, by holder: {quest: {wolf|party:<p>|story: status}}.)
            'expectQuests',
        ),
    )
    require('expectQuests' not in case or (isinstance(case['expectQuests'], dict) and all(
        isinstance(v, dict) and all(s in ('offered', 'none', *quests.STATUSES) for s in v.values())
        for v in case['expectQuests'].values())), 'Expected quests are {quest: {holder: status}}.')
    require('seed' not in case or word(case['seed'], 80), 'A seed is a word of up to 80 characters.')
    require('rolls' not in case or (isinstance(case['rolls'], list) and len(case['rolls']) <= 256
                                    and all(integer(r, 0, 99) for r in case['rolls'])), 'Pinned rolls are numbers 0 to 99.')
    require('parties' not in case or (isinstance(case['parties'], dict) and all(
        identifier(k) and identifier(v) for k, v in case['parties'].items())), 'Parties name each wolf\'s party.')
    require('expectOutcome' not in case or case['expectOutcome'] in (None, *triggers.OUTCOMES), 'Expect success, failure, '
            'abandoned or none.')
    require('expectVariables' not in case or (isinstance(case['expectVariables'], dict)
                                              and set(case['expectVariables']) <= set(triggers.SCOPES)),
            'Expected variables are listed by scope (story, player, party, world).')
    require(
        word(case['name'], 100)
        and isinstance(case['cohort'], list)
        and len(case['cohort']) <= quests.AUDIENCE_CAP
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
    state = begin(p, case['cohort'], 0, case.get('seed'))
    if case.get('rolls'):
        state['rolls'] = list(case['rolls'])
    if case.get('parties'):
        state['parties'] = dict(case['parties'])
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
    v2 = p.get('version') == 2
    return {
        'name': case['name'],
        'passed': path == case['expect']
        and expected_effects == intended
        and ('expectQuests' not in case or _quests_match(state, case['expectQuests']))
        and credits == case.get('expectMissions', credits)
        and visit_credits == case.get('expectMissionVisits', visit_credits)
        and [f['trigger'] for f in trace] == case.get('expectTriggers', [f['trigger'] for f in trace])
        and state.get('counters', {}) == case.get('expectCounters', state.get('counters', {}))
        and ('expectVariables' not in case or _variables_match(p, state, case['expectVariables']))
        and ('expectOutcome' not in case or (state.get('outcome') or {}).get('outcome') == case['expectOutcome']),
        **({'outcome': state.get('outcome'), 'outcomes': state.get('outcomes', {})} if v2 else {}),
        **({'quests': {qid: {h: c['status'] for h, c in copies.items()} for qid, copies in state.get('quests', {}).items()},
            'cohort': list(state['cohort'])} if p.get('quests') or quests.joinable(p) else {}),
        'triggers': trace,
        'counters': state.get('counters', {}),
        'missionCredits': credits,
        'missionVisits': visit_credits,
        'path': path,
        'expected': case['expect'],
        'effects': intended,
    }


def _quests_match(state, expected):
    """A case's expected quests: each holder's status ('offered' for an offer not yet taken, 'none' for no copy)."""
    for qid, holders in expected.items():
        copies = state.get('quests', {}).get(qid, {})
        for holder, status in holders.items():
            have = copies[holder]['status'] if holder in copies else \
                'offered' if holder in state.get('questOffers', {}).get(qid, ()) else 'none'
            if have != status:
                return False
    return True


def _variables_match(p, state, expected):
    """The variables a case names, against the run's (a variable never set has its first value)."""
    decls = variables_of(p)
    stores = state.get('counters', {})
    for scope, values in expected.items():
        require(isinstance(values, dict), 'Expected variables are listed by scope.')
        groups = values.items() if scope in triggers.PERSONAL else [(None, values)]
        for owner, named in groups:
            have = stores.get(scope, {}).get(owner, {}) if owner is not None else stores.get(scope, {})
            require(isinstance(named, dict), 'Expected variables of a wolf or party are listed under it.')
            for name, value in named.items():
                require(name in decls and decls[name]['scope'] == scope, f'No {scope} variable "{name}" is declared.')
                if have.get(name, triggers.initial(decls[name])) != value:
                    return False
    return True


def upgrade(p):
    """A version 1 program as version 2, doing the same (doc 85, 3.1 rule 3 and Phase 5): every counter it reads or
    writes declared as a number of its scope (one used in both scopes: the wolf's gets ".player" after its name), each
    counter test a `compare`, each Set and Add on a declared variable. Its rehearsals give the same paths, effects,
    triggers and values. A version 2 program comes back as it is."""
    validate(p)
    if p['version'] == 2:
        return p
    q = json.loads(json.dumps(p))
    rules_ = [*q.get('triggers', []), *(t for ph in q['phases'] for t in ph.get('triggers', []))]
    used = []
    for t in rules_:
        for c in (triggers.conditions_in(t['if']) if 'if' in t else ()):
            if c['test'] == 'counter' and (c['key'], c['scope']) not in used:
                used.append((c['key'], c['scope']))
        for a in t['actions']:
            if a['do'] in ('set', 'add') and (a['key'], a['scope']) not in used:
                used.append((a['key'], a['scope']))
    both = {k for k, sc in used if sc == 'player' and (k, 'story') in used}
    name = {(k, sc): k + '.player' if k in both and sc == 'player' else k for k, sc in used}

    def condition(c):
        if c['test'] in ('all', 'any'):
            return {**c, 'conditions': [condition(x) for x in c['conditions']]}
        if c['test'] == 'not':
            return {**c, 'condition': condition(c['condition'])}
        if c['test'] == 'counter':
            return {'test': 'compare', 'left': {'var': name[(c['key'], c['scope'])]}, 'op': c['op'], 'right': c['value']}
        return c

    for t in rules_:
        if 'if' in t:
            t['if'] = condition(t['if'])
        t['actions'] = [{'do': a['do'], 'var': name[(a['key'], a['scope'])],
                         'value': a['value'] if isinstance(a['value'], int) else {'field': a['value']}}
                        if a['do'] in ('set', 'add') else a for a in t['actions']]
    for case in q['tests']:
        player = case.get('expectCounters', {}).get('player')
        if both and isinstance(player, dict):
            case['expectCounters']['player'] = {w: {name.get((k, 'player'), k): v for k, v in vals.items()} for w, vals in player.items()}
    out = {}
    for key, value in q.items():
        out[key] = 2 if key == 'version' else value
        if key == 'start' and used:
            out['variables'] = [{'id': name[(k, sc)], 'type': 'number', 'scope': sc} for k, sc in used]
    validate(out)
    return out


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
                'missions': mission_snapshot(phase, state), 'ending': not ongoing(phase),
                **({'quests': quests.board(p, state), 'cohort': list(state['cohort'])} if p.get('quests') else {}),
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


# ------------------------------------------------------------------------- moving a running campaign onto a new version
# Doc 85, Phase 5b (the user's answer to question 5, 2026-11-21: yes). A ninety-day campaign that needs a fix on day 40
# keeps its wolves, its phase, its variables and its missions. `version_diff` says what changed between two approved
# programs; `migration` maps a running state onto the new one: what is kept, reset, defaulted or dropped, each with why.
# Pure: the runner (story_runner.Steer.do_migrate) takes it as a command between two world events, and nothing is
# delivered by the move itself.

def _json(v):
    return json.dumps(v, sort_keys=True, separators=(',', ':'))


def _trigger_table(p):
    """Every trigger of a program by the key the state uses for it (a campaign trigger by its id, a phase's own as
    phase/id), with where it lives."""
    out = {t['id']: {'rule': t, 'scope': 'campaign', 'phase': None} for t in p.get('triggers', [])}
    for ph in p['phases']:
        for t in ph.get('triggers', []):
            out[ph['id'] + '/' + t['id']] = {'rule': t, 'scope': 'phase', 'phase': ph['id']}
    return out


def _changed_fields(a, b, skip=()):
    return sorted(k for k in set(a) | set(b) if k not in skip and _json(a.get(k)) != _json(b.get(k)))


def declared(p):
    """What a program keeps in `counters`, by (scope, name): a version 2 story's declarations; a version 1 story's
    counters as its triggers read or write them (whole numbers that start at 0)."""
    if p.get('version') == 2:
        return {(d['scope'], d['id']): d for d in variables_of(p).values()}
    out = {}
    for t in [*p.get('triggers', []), *(t for ph in p['phases'] for t in ph.get('triggers', []))]:
        for c in (triggers.conditions_in(t['if']) if 'if' in t else ()):
            if c.get('test') == 'counter':
                out[(c['scope'], c['key'])] = {'id': c['key'], 'scope': c['scope'], 'type': 'number'}
        for _, a in triggers.walk(t['actions']):
            if a['do'] in ('set', 'add') and 'key' in a:
                out[(a['scope'], a['key'])] = {'id': a['key'], 'scope': a['scope'], 'type': 'number'}
    return out


def _schedules_of(p):
    out = {}
    for t in [*p.get('triggers', []), *(t for ph in p['phases'] for t in ph.get('triggers', []))]:
        for _, a in triggers.walk(t['actions']):
            if a['do'] == 'schedule':
                out.setdefault(a['signal'], []).append({k: a.get(k) for k in ('days', 'repeat', 'scope', 'clock', 'owner')})
    return out


def _story_signals_heard(phase):
    """The story signals a phase (with its campaign triggers) reacts to: what a running timer needs to still be heard."""
    heard = {t['signal'] for t in phase.get('triggers', []) if t['on'] == triggers.SIGNAL}
    heard |= {o['signal'] for m in phase.get('missions', []) for o in m['objectives'] if 'signal' in o}
    return heard


def version_diff(old, new):
    """What changed from one approved program to another, for the admin to read before moving a run (doc 85, Phase 5b):
    triggers, variables, phases, missions, timers (the signals the story can start), regions, scenes and the header."""
    validate(old)
    validate(new)

    def by_id(items):
        return {x['id']: x for x in items}

    def compare(a, b, skip=()):
        added = sorted(set(b) - set(a))
        removed = sorted(set(a) - set(b))
        changed = [{'id': k, 'what': _changed_fields(a[k], b[k], skip)} for k in sorted(set(a) & set(b))
                   if _json({f: v for f, v in a[k].items() if f not in skip}) != _json({f: v for f, v in b[k].items() if f not in skip})]
        return {'added': added, 'removed': removed, 'changed': changed}

    ta, tb = _trigger_table(old), _trigger_table(new)
    trig = compare({k: v['rule'] for k, v in ta.items()}, {k: v['rule'] for k, v in tb.items()})
    for c in trig['changed']:
        c['wordsOnly'] = set(c['what']) <= {'folder', 'note'}
    da, db = declared(old), declared(new)
    names_a = {f'{sc}:{n}': d for (sc, n), d in da.items()}
    names_b = {f'{sc}:{n}': d for (sc, n), d in db.items()}
    plain_a, plain_b = {}, {}
    for (sc, n), d in da.items():
        plain_a.setdefault(n, []).append(d)
    for (sc, n), d in db.items():
        plain_b.setdefault(n, []).append(d)
    variables = {'added': [], 'removed': [], 'retyped': [], 'rescoped': [], 'changed': [], 'renamed': []}
    if old.get('version') != 2 and new.get('version') == 2:
        # (Version 1 onto version 2: a wolf's counter named in both scopes is `<name>.player` now, as `upgrade` names it.)
        for (sc, n) in sorted(da):
            if sc == 'player' and ('story', n) in da and ('player', n + '.player') in db and ('player', n) not in db:
                variables['renamed'].append({'id': n, 'scope': 'player', 'to': n + '.player'})
                names_a.pop(f'player:{n}')
                names_b.pop(f'player:{n}.player')
    for key in sorted(set(names_a) | set(names_b)):
        a, b = names_a.get(key), names_b.get(key)
        sc, n = key.split(':', 1)
        if a and b:
            if a['type'] != b['type'] or a.get('clock', 'game') != b.get('clock', 'game'):
                variables['retyped'].append({'id': n, 'scope': sc, 'from': a['type'], 'to': b['type']})
            elif _json(a) != _json(b):
                variables['changed'].append({'id': n, 'scope': sc, 'what': _changed_fields(a, b)})
        elif a and n in plain_b and not any(d['scope'] == sc for d in plain_b[n]):
            if not any(x['id'] == n and x['scope'] == sc for x in variables['rescoped']):
                variables['rescoped'].append({'id': n, 'from': sc, 'to': plain_b[n][0]['scope']})
        elif b and n in plain_a and not any(d['scope'] == sc for d in plain_a[n]):
            continue   # (The other end of a rescope, listed once.)
        elif a:
            variables['removed'].append({'id': n, 'scope': sc})
        else:
            variables['added'].append({'id': n, 'scope': sc, 'type': b['type']})
    pa, pb = by_id(old['phases']), by_id(new['phases'])
    phases = compare(pa, pb)
    missions = {'added': [], 'removed': [], 'changed': []}
    for pid in sorted(set(pa) & set(pb)):
        ma, mb = by_id(pa[pid].get('missions', [])), by_id(pb[pid].get('missions', []))
        missions['added'] += [{'phase': pid, 'id': m} for m in sorted(set(mb) - set(ma))]
        missions['removed'] += [{'phase': pid, 'id': m} for m in sorted(set(ma) - set(mb))]
        missions['changed'] += [{'phase': pid, 'id': m, 'what': _changed_fields(ma[m], mb[m]),
                                 'objectives': _json(ma[m]['objectives']) != _json(mb[m]['objectives']) or ma[m]['mode'] != mb[m]['mode']}
                                for m in sorted(set(ma) & set(mb)) if _json(ma[m]) != _json(mb[m])]
    sa, sb = _schedules_of(old), _schedules_of(new)
    timers = {'added': sorted(set(sb) - set(sa)), 'removed': sorted(set(sa) - set(sb)),
              'changed': sorted(s for s in set(sa) & set(sb) if _json(sa[s]) != _json(sb[s]))}
    out = {
        'triggers': trig, 'variables': variables, 'phases': phases, 'missions': missions, 'timers': timers,
        'regions': compare(by_id(old.get('regions', [])), by_id(new.get('regions', []))),
        'scenes': compare(by_id(old.get('scenes', [])), by_id(new.get('scenes', []))),
        'header': _changed_fields({k: v for k, v in old.items() if k not in ('phases', 'triggers', 'variables', 'regions', 'scenes', 'tests')},
                                  {k: v for k, v in new.items() if k not in ('phases', 'triggers', 'variables', 'regions', 'scenes', 'tests')}),
        'tests': _json(old['tests']) != _json(new['tests']),
    }
    out['same'] = not any(out[k]['added'] or out[k]['removed'] or out[k]['changed']
                          for k in ('triggers', 'phases', 'missions', 'regions', 'scenes', 'timers')) \
        and not any(out['variables'].values()) and not out['header']
    return out


def _remaining(actions, address):
    """What a wait at `address` (its `next`) still has to run: the list it points into, and after each action it is
    inside, the rest of that action's list. None when the address no longer points anywhere."""
    out, lst, i = [], actions, 0
    while i < len(address) - 1:
        index, name = address[i], address[i + 1]
        if not isinstance(index, int) or index >= len(lst):
            return None
        holder = lst[index]
        out.append((holder.get('do'), lst[index + 1:]))
        try:
            lst = triggers.branch(holder, name)
        except (KeyError, IndexError, ValueError, TypeError, AttributeError):
            return None
        i += 2
    if not isinstance(address[-1], int) or address[-1] > len(lst):
        return None
    out.append(('list', lst))
    return out


def migration(old, new, state, land=None, run='', began=None):
    """A running story's state carried onto a new version of its program (doc 85, Phase 5b). Returns (state, report);
    the state is None when the move cannot be made as asked (report['blocked'] says why: the current phase is gone and no
    phase to land in was named, or the one named cannot hold the run).

    Kept: the phase (or the one named to land in), the visit and its day, the audience; variables by name, scope and
    type (a version 1 run's counters by `upgrade`'s names, a wolf's counter named in both scopes as `<name>.player`);
    timers whose signal still has a listener; a mission's progress when its objectives and mode are unchanged; the
    once-marks, counts and turned-off triggers by trigger; votes and steps of an exit that is unchanged; a wait only when
    what it still has to run is unchanged. Reset: a mission whose objectives changed. Defaulted: a variable retyped,
    rescoped or holding what its new type cannot. Dropped: what the new version no longer has. Triggers newly off in the
    new version ("enabled": false) are turned off in the run; those newly on stay as the run has them."""
    validate(old)
    validate(new)
    state = json.loads(json.dumps(state))
    report = {'kept': [], 'reset': [], 'defaulted': [], 'dropped': [], 'added': [], 'warnings': [], 'blocked': None}
    kept, reset, defaulted, dropped, added, warn = (report[k] for k in ('kept', 'reset', 'defaulted', 'dropped', 'added', 'warnings'))
    report['diff'] = version_diff(old, new)
    report['from'] = {'digest': digest(old), 'version': old['version'], 'title': old['title']}
    report['to'] = {'digest': digest(new), 'version': new['version'], 'title': new['title']}
    here = state['phase']
    new_ids = [ph['id'] for ph in new['phases']]
    report['phase'] = {'from': here, 'to': None, 'choices': new_ids, 'gone': here not in new_ids}
    if new['id'] != old['id']:
        report['blocked'] = 'A new version keeps its story\'s id ("' + old['id'] + '"): this is another story.'
        return None, report
    if here in new_ids:
        if land not in (None, '', here):
            report['blocked'] = f'Phase {here} is in the new version: the run stays in it (move it, then go to another phase).'
            return None, report
        land = here
    elif not land:
        report['blocked'] = f'Phase {here} is not in the new version: name the phase to land in.'
        return None, report
    elif land not in new_ids:
        report['blocked'] = f'The new version has no phase {land}.'
        return None, report
    report['phase']['to'] = land
    old_phase = phase_of(old, state)
    landing = effective_phase(new, next(ph for ph in new['phases'] if ph['id'] == land))
    if not ongoing(landing):
        report['blocked'] = f'Phase {land} ends the story: land in an ongoing phase (then go to {land} from the console).'
        return None, report
    cohort = state['cohort']
    for ph in phases_of(new):
        for e in possible_effects(ph):
            if e['kind'] == 'scene.play' and e['player'] not in ('$actor', '$each', 'world') and e['player'] not in cohort:
                report['blocked'] = f'Scene initiator {e["player"]} is not in this run\'s audience.'
                return None, report
    if land != here:
        kept.append({'what': 'phase', 'key': land, 'why': f'{here} is not in the new version: the visit carries on as {land}, its entry effects not sent again.'})
    else:
        kept.append({'what': 'phase', 'key': land, 'why': f'Visit {state["visits"]} of {land} carries on (its entry effects are not sent again).'})
    rename_phase = (lambda key: land + key[len(here):] if land != here and key.startswith(here + '/') else key)
    v2 = new.get('version') == 2

    # ---- variables (counters): by name, scope and type
    da, db = declared(old), declared(new)
    names_v1_both = set()
    if old.get('version') != 2 and v2:
        names_v1_both = {n for (sc, n) in da if sc == 'player' and ('story', n) in da}
    counters = state.get('counters', {})
    out = {}

    def put(scope, owner, name, value):
        bucket = out.setdefault(scope, {})
        if scope in ('player', 'party'):
            bucket = bucket.setdefault(owner, {})
        bucket[name] = value

    def fits(d, value):
        kind = d['type']
        if kind == 'timer':
            return isinstance(value, (int, float)) and not isinstance(value, bool)
        if kind == 'number':
            return integer(value, -triggers.LIMIT, triggers.LIMIT)
        return _initial_ok(kind, value, set(region_index(new)))

    def carry(scope, owner, name, value):
        whose = {'wolf': owner} if scope == 'player' else {'party': owner} if scope == 'party' else {}
        candidates = [name + '.player', name] if scope == 'player' and name in names_v1_both else [name]
        target = next((c for c in candidates if (scope, c) in db), None)
        if target is None:
            other = next((d for (sc, n), d in db.items() if n == name), None)
            if other:
                defaulted.append({'what': 'variable', 'key': name, 'scope': scope, **whose, 'value': value,
                                  'why': f'it is now {other["scope"]}\'s, not {scope}\'s: it starts at its first value there.'})
            else:
                dropped.append({'what': 'variable', 'key': name, 'scope': scope, **whose, 'value': value,
                                'why': 'the new version no longer has it.'})
            return
        d, before = db[(scope, target)], da.get((scope, name))
        if before and (before['type'] != d['type'] or before.get('clock', 'game') != d.get('clock', 'game')):
            defaulted.append({'what': 'variable', 'key': target, 'scope': scope, **whose, 'value': value,
                              'why': f'it was a {before["type"]}, it is a {d["type"]} now: it starts at its first value.'})
            return
        if not fits(d, value):
            defaulted.append({'what': 'variable', 'key': target, 'scope': scope, **whose, 'value': value,
                              'why': f'{value!r} is not a {d["type"]} the new version allows: it starts at its first value.'})
            return
        put(scope, owner, target, value)
        kept.append({'what': 'variable', 'key': target, 'scope': scope, **whose, 'value': value,
                     **({'renamed': name, 'why': f'renamed from {name} (a version 1 counter named in both scopes).'} if target != name else {})})

    for scope in ('story',):
        for name, value in sorted(counters.get(scope, {}).items()):
            carry(scope, '', name, value)
    for scope in ('player', 'party'):
        for owner, values in sorted(counters.get(scope, {}).items()):
            for name, value in sorted(values.items()):
                carry(scope, owner, name, value)
    if out:
        state['counters'] = out
    else:
        state.pop('counters', None)
    for (sc, n), d in sorted(db.items()):
        if (sc, n) not in da and not any(nn == n for _, nn in da) and not any(x.get('renamed') and x['key'] == n for x in kept):
            if sc == 'world':
                added.append({'what': 'variable', 'key': n, 'scope': sc, 'why': 'kept by the world: this story reads what the world holds.'})
            else:
                added.append({'what': 'variable', 'key': n, 'scope': sc, 'why': 'new: it starts at its first value.'})
    for (sc, n), d in sorted(db.items()):
        if sc == 'world' and (sc, n) in da and da[(sc, n)]['type'] != d['type']:
            warn.append(f'World variable {n} is a {d["type"]} now: a value the world keeps as a {da[(sc, n)]["type"]} is not read by this story.')

    # ---- timers: kept while their signal still has a listener
    heard_here = _story_signals_heard(landing)
    heard_any = set().union(*(_story_signals_heard(ph) for ph in phases_of(new)))
    sched_old, sched_new = _schedules_of(old), _schedules_of(new)
    timers_now = {}
    for key, t in sorted(state.get('timers', {}).items()):
        signal = t.get('signal', key.split('/')[0])
        heard = heard_any if t.get('scope') == 'campaign' else heard_here
        if signal in heard:
            timers_now[key] = t
            line = {'what': 'timer', 'key': key, 'at': t['at']}
            if signal in sched_old and signal in sched_new and _json(sched_old[signal]) != _json(sched_new[signal]):
                line['why'] = 'it keeps its time; the new version starts it differently from now on.'
            kept.append(line)
        else:
            dropped.append({'what': 'timer', 'key': key, 'at': t['at'],
                            'why': 'nothing in the new version listens for its signal' + ('' if t.get('scope') == 'campaign' else f' in {land}') + '.'})
    if timers_now:
        state['timers'] = timers_now
    else:
        state.pop('timers', None)

    # ---- triggers: once-marks, counts, turned off, waits
    ta, tb = _trigger_table(old), _trigger_table(new)
    same_rule = lambda k_old, k_new: k_old in ta and k_new in tb and _json(ta[k_old]['rule']) == _json(tb[k_new]['rule'])
    shared_new = landing.get('_sharedIds', set())
    BUCKET = {'visit': 'fired', 'player': 'fired', 'campaign': 'campaignFired', 'member': 'campaignFired'}
    for bucket in ('fired', 'campaignFired'):
        marks = {}
        for token, owners in sorted(state.get(bucket, {}).items()):
            key = rename_phase(token)
            if bucket == 'fired':
                rule = next((t for t in landing.get('triggers', []) if t['id'] == token), None)
            else:
                rule = tb.get(key, {}).get('rule')
            if rule is None:
                dropped.append({'what': 'fired', 'key': token, 'why': 'the new version has no such trigger here.'})
            elif BUCKET.get(rule.get('once', 'each')) != bucket:
                dropped.append({'what': 'fired', 'key': token, 'why': f'it fires {rule.get("once", "each")} now: it may fire again.'})
            else:
                marks[key] = owners
                old_k, new_k = (token, key) if bucket == 'campaignFired' else (
                    triggers.trigger_key(old_phase, token), triggers.trigger_key(landing, token))
                kept.append({'what': 'fired', 'key': new_k, 'wolves': [w for w in owners if w != '*'],
                             **({} if same_rule(old_k, new_k) else {'why': 'it has changed, and still counts as fired.'})})
        if marks:
            state[bucket] = marks
        else:
            state.pop(bucket, None)
    if v2:
        counts = {}
        for scope, values in sorted(state.get('counts', {}).items()):
            for token, n in sorted(values.items()):
                key = rename_phase(token)
                if key in tb:
                    counts.setdefault(scope, {})[key] = n
                else:
                    dropped.append({'what': 'count', 'key': token, 'why': 'the new version has no such trigger.'})
        if counts:
            state['counts'] = counts
        else:
            state.pop('counts', None)
        if old.get('version') != 2:
            warn.append('Version 1 did not count firings: "fired" conditions count from now.')
    elif state.pop('counts', None):
        warn.append('Version 1 does not count firings: the counts are dropped.')
    off = set()
    for token in sorted(state.get('disabled', ())):
        key = rename_phase(token)
        if key in tb:
            off.add(key)
            kept.append({'what': 'off', 'key': key})
        else:
            dropped.append({'what': 'off', 'key': token, 'why': 'the new version has no such trigger.'})
    for key, item in sorted(tb.items()):
        was = ta.get(key) or ta.get(here + key[len(land):] if land != here and key.startswith(land + '/') else None)
        if item['rule'].get('enabled') is False and v2 and (was is None or was['rule'].get('enabled') is not False):
            if key not in off:
                off.add(key)
                added.append({'what': 'off', 'key': key, 'why': 'the new version starts it turned off.'})
        elif was is not None and was['rule'].get('enabled') is False and item['rule'].get('enabled') is not False and key in off:
            warn.append(f'{key} starts on in the new version; it stays off in this run (turn it on from Triggers).')
    if off:
        state['disabled'] = sorted(off)
    else:
        state.pop('disabled', None)
    waits = []
    for w in state.get('waiting', []):
        old_key = w['trigger'] if w['scope'] == 'campaign' else here + '/' + w['trigger']
        new_key = w['trigger'] if w['scope'] == 'campaign' else land + '/' + w['trigger']
        line = {'what': 'wait', 'key': new_key, 'at': w['at'], 'next': w['next'], **({'wolf': w['actor']} if w.get('actor') else {})}
        if not v2:
            dropped.append({**line, 'why': 'version 1 has no waits: the rest of its actions will not run.'})
            continue
        a, b = ta.get(old_key), tb.get(new_key)
        if b is None:
            dropped.append({**line, 'why': 'the new version has no such trigger: the rest of its actions will not run.'})
            continue
        if w['scope'] != 'campaign' and w['trigger'] in shared_new:
            dropped.append({**line, 'why': 'it is a campaign trigger now: the rest of its actions will not run.'})
            continue
        rest_a = _remaining(a['rule']['actions'], w['next']) if a else None
        rest_b = _remaining(b['rule']['actions'], w['next'])
        if rest_a is None or rest_b is None or _json(rest_a) != _json(rest_b):
            dropped.append({**line, 'why': 'what it still had to run has changed: the rest of its actions will not run.'})
            continue
        waits.append({**w, 'phase': land if w['scope'] != 'campaign' else w['phase']})
        kept.append({**line, 'why': 'what it still has to run is unchanged: it resumes as it would have.'})
    if waits:
        state['waiting'] = waits
    else:
        state.pop('waiting', None)

    # ---- missions, and an exit's votes and steps
    ma = {m['id']: m for m in old_phase.get('missions', [])}
    mb = {m['id']: m for m in landing.get('missions', [])}
    ledger = {}
    for mid, record in sorted(state.get('missions', {}).items()):
        if mid not in mb:
            dropped.append({'what': 'mission', 'key': mid, 'why': f'{land} has no such mission in the new version.'})
        elif mid in ma and _json(ma[mid]['objectives']) == _json(mb[mid]['objectives']) and ma[mid]['mode'] == mb[mid]['mode']:
            ledger[mid] = record
            kept.append({'what': 'mission', 'key': mid, **({'why': 'it has changed; its objectives have not: progress kept.'}
                                                          if _json(ma[mid]) != _json(mb[mid]) else {})})
        else:
            reset.append({'what': 'mission', 'key': mid, 'why': 'its objectives (or who plays it) changed: its progress starts again.'})
    for mid in sorted(set(mb) - set(ma)):
        added.append({'what': 'mission', 'key': mid, 'why': 'new in this phase: nobody has made progress yet.'})
    if ledger:
        state['missions'] = ledger
    else:
        state.pop('missions', None)
    for field in ('votes', 'progress'):
        held = {}
        for index, value in sorted(state.get(field, {}).items()):
            i = int(index)
            same = i < len(old_phase['exits']) and i < len(landing['exits']) and _json(old_phase['exits'][i]) == _json(landing['exits'][i])
            if same:
                held[index] = value
                kept.append({'what': 'vote' if field == 'votes' else 'steps', 'key': f'exit {i + 1}'})
            elif value:
                dropped.append({'what': 'vote' if field == 'votes' else 'steps', 'key': f'exit {i + 1}',
                                'why': 'that exit has changed: its count starts again.'})
        if field in state:
            state[field] = held

    # ---- quests (doc 85, Phase 6): kept by id when their objectives and scope are unchanged, else their progress reset
    lines = {'kept': kept, 'reset': reset, 'dropped': dropped, 'added': added}
    quests.migrate(old, new, state, lambda kind, what, why: lines[kind].append(
        {'what': 'quest', 'key': what.split(' ', 1)[1], 'why': why[0].upper() + why[1:] + '.'}))
    qa, qb = quests.index(old), quests.index(new)
    report['diff']['quests'] = {'added': sorted(set(qb) - set(qa)), 'removed': sorted(set(qa) - set(qb)),
                                'changed': sorted(k for k in set(qa) & set(qb) if _json(qa[k]) != _json(qb[k]))}
    if not new.get('quests'):
        for key in ('questOffers', 'questStarts'):
            state.pop(key, None)

    # ---- version 2's own memory
    if v2:
        if old.get('version') != 2:
            state.setdefault('seed', run or new['id'])
            state.setdefault('began', began if began is not None else state['entered'])
            added.append({'what': 'seed', 'key': 'rolls', 'why': 'version 2 rolls from the run\'s own seed from now on.'})
    else:
        for key in ('seed', 'began', 'draws', 'rolls', 'outcomes', 'parties'):
            if state.pop(key, None) is not None and key == 'outcomes':
                warn.append("Version 1 has no endings of a wolf's own: those recorded are dropped.")
    if _json(old['audience']) != _json(new['audience']):
        warn.append(f'The audience stays this run\'s {len(cohort)} wolves: the new version\'s audience is for new runs.')
    report['diff']['triggers']['newlyOff'] = sorted(x['key'] for x in added if x['what'] == 'off')
    state['phase'] = land
    return state, report
