"""The same deterministic world-story rules for Studio, authoring tools and the world's runner."""

import contextvars
import hashlib
import json
import math
import re
import threading
import time

import story_scenes
import story_triggers as triggers

SCENE_EVENTS = ('scene arrived', 'scene spoken', 'scene posed', 'scene blocked', 'scene finished', 'scene choice', 'scene response', 'scene failed')
REGION_EVENTS = ('region entered', 'region left')
SCENE_EFFECTS = ('npc.walk', 'npc.speak', 'npc.pose')
EVENTS = (
    *SCENE_EVENTS,
    *REGION_EVENTS,
    'conversation',
    'arrival',
    'enter place',
    'departure',
    'gift',
    'gift again',
    'economy',
    'contract done',
    'hunted',
    'fight ends',
    'gift used',
    'death',
    'revival',
    'marriage',
    'relocation',
    'raid',
    'bandits gather',
    'camp cleared',
    'bandits scattered',
    'patrol',
    'caravan arrives',
    'festival',
    'project built',
    'project gift',
    'festival entered',
    'carter robbed',
    'storyline objective',
    'storyline step',
    'tale ended',
    'story phase',
    'archive work',
    'contract taken',
    'contract posted',
    'deed',
    'festival won',
    'fine paid',
    'forage',
    'groomed',
    'hunt',
    'huntRole',
    'introduced',
    'lease',
    'lease ended',
    'lent',
    'letter sent',
    'letter delivered',
    'loan returned',
    'notice answered',
    'notice posted',
    'pact sealed',
    'paid off bandits',
    'performance',
    'reported',
    'shared meal',
    'ship docks',
    'ship lost',
    'ship sails',
    'ship stormbound',
    'stall rented',
    'stall sale',
    'story told',
    'table game',
    'tended',
    'together',
    'told the wardens',
    'trouble heard',
    'trouble solved',
    'vouch',
    'vouched to the wardens',
    'warrant',
    'witnessed',
    'project ruined',
    'harvest over',
    'wagon overdue',
    'wagon held up',
    'caravan lost',
    'caravan departs',
    'festival called',
    'arrest',
    'assault',
    'bandit falls',
    'bandits called',
    'bandits creep',
    'bandits demand',
    'caravan dropped',
    'caravan home',
    'caravan passes',
    'cart arrives',
    'cart departs',
    'cart missed',
    'cart stocks the stalls',
    'carter hurt',
    'character created',
    'cleared',
    'contract given back',
    'crier',
    'deed revoked',
    'downed',
    'event added',
    'event begins',
    'event notice',
    'event posted',
    'event rolled',
    'fight',
    'harvest called',
    'held for a story',
    'letter returned',
    'loan unreturned',
    'matchmaking',
    'milestone',
    'mourning',
    'nickname',
    'nickname dropped',
    'notice removed',
    'nudge over',
    'old age',
    'prices reset',
    'project cancelled',
    'project posted',
    'project proposed',
    'project removed',
    'promise',
    'promise broken',
    'released',
    'resident letter',
    'robbed',
    'sermon',
    'ship posted',
    'ships cleared',
    'spawn',
    'stall closed',
    'stolen goods smelt',
    'stopped by the watch',
    'story prize',
    'storyline begun',
    'storyline done',
    'table game begun',
    'tale begun',
    'tale joined',
    'tie ended',
    'times cause',
    'times nudged',
    'vouch broken',
    'wagon cancelled',
    'wagon delayed',
    'wagon posted',
    'welcome',
    'welcomed back',
    'clock',
    'dm signal',
)
FIELDS = ('actor', 'target', 'cell', 'item', 'detail')
# Actions here map to existing, bounded world operations. Story-only dialogue expires on phase exit.
EFFECTS = {
    'scene.play': ('scene', 'player', 'cue'),
    'npc.line': ('npc', 'text'),
    'npc.walk': ('npc', 'cell', 'x', 'y', 'pace', 'cue'),
    'npc.speak': ('npc', 'text', 'cue'),
    'npc.pose': ('npc', 'posture', 'cue'),
    'story.area.damage': ('region', 'points'),
    'story.area.item': ('region', 'item', 'quantity'),
    'story.area.buff': ('region', 'buff', 'hours'),
    'story.area.spawn': ('region', 'count'),
    'npc.move': ('npc', 'cell', 'x', 'y'),
    'npc.protect': ('npc',),
    'npc.unprotect': ('npc',),
    'npc.revive': ('npc',),
    'character.injury': ('player', 'add', 'severity'),
    'ship.storm': ('ship',),
    'harvest.override': ('area', 'word', 'days', 'words'),
    'band.override': ('npc', 'band', 'days'),
    'festival.call': ('town', 'name', 'inDays'),
    'storyline.give': ('player', 'template', 'resident', 'place'),
    'faction.relation': ('faction', 'other', 'stance', 'disposition', 'reason'),
    'bandits.call': ('player', 'count'),
    'story.practice': ('player', 'skill'),
    'times.nudge': ('market', 'points', 'days', 'words'),
    'deed.award': ('player', 'detail'),
    'project.post': ('town', 'project', 'cell', 'x', 'y', 'title'),
}
# Injuries a Dungeon Master may give (Docs/Design/38-injuries.md, phase 5; the game's Core/RatwInjury.cpp has the same).
INJURY_TYPES = ('torn_flank', 'bitten_foreleg', 'bitten_hindleg', 'torn_ear_acute', 'wrenched_neck', 'deep_gash', 'cut_foreleg',
                'cut_muzzle', 'cut_shoulder', 'bruised_ribs', 'cracked_rib', 'sprained_foreleg', 'knocked_senseless', 'burned_paws',
                'singed_coat', 'burned_muzzle', 'torn_ear', 'bent_tail', 'scarred_muzzle', 'scarred_flank', 'burn_scars',
                'notched_nose', 'clouded_eye', 'permanent_limp', 'bad_back', 'stiff_shoulder')
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
MISSION_EVENTS = (
    *REGION_EVENTS,
    'bandit falls',
    'downed',
    'archive work',
    'contract taken',
    'contract done',
    'conversation',
    'deed',
    'enter place',
    'festival entered',
    'festival won',
    'fine paid',
    'forage',
    'gift',
    'gift again',
    'gift used',
    'groomed',
    'hunt',
    'huntRole',
    'hunted',
    'introduced',
    'lease',
    'lease ended',
    'lent',
    'letter sent',
    'letter delivered',
    'loan returned',
    'notice answered',
    'notice posted',
    'pact sealed',
    'paid off bandits',
    'performance',
    'project gift',
    'reported',
    'shared meal',
    'stall rented',
    'stall sale',
    'story told',
    'table game',
    'tended',
    'together',
    'told the wardens',
    'trouble heard',
    'trouble solved',
    'vouch',
    'vouched to the wardens',
    'witnessed',
)
MISSION_GATE = 'missions completed'
# Positive amounts actually emitted by the native producers in addition to an occurrence.
MISSION_MEASURES = {
    'quantity': (
        'deed',
        'festival won',
        'forage',
        'gift',
        'gift again',
        'hunt',
        'hunted',
        'lent',
        'loan returned',
        'project gift',
        'stall sale',
        'table game',
        'together',
    ),
    'coins': (
        'archive work',
        'contract taken',
        'contract done',
        'festival entered',
        'festival won',
        'fine paid',
        'gift',
        'gift again',
        'lease',
        'lease ended',
        'letter sent',
        'notice posted',
        'paid off bandits',
        'project gift',
        'stall rented',
        'stall sale',
        'table game',
        'trouble solved',
    ),
}


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


def validate_effect(effect, pid, regions=None, scenes=None):
    kind = effect.get('kind') if isinstance(effect, dict) else None
    require(isinstance(kind, str) and kind in EFFECTS, f'{pid}: unknown effect.')
    shape(effect, ('kind', *EFFECTS[kind]))
    for k in EFFECTS[kind]:
        v = effect[k]
        if k in ('x', 'y', 'inDays'):
            ok = integer(v, 0, 30 if k == 'inDays' else 4095)
        elif k == 'stance':
            ok = v in ('allied', 'friendly', 'neutral', 'tense', 'hostile', 'war')
        elif k == 'disposition':
            ok = integer(v, -100, 100)
        elif k == 'reason':
            ok = word(v, 300)
        elif k == 'count':
            ok = integer(v, 1, 6)
        elif k == 'skill':
            ok = v in ('craft', 'commerce', 'labour', 'service', 'travel', 'watch')
        elif k == 'region':
            ok = identifier(v) and v in (regions or {})
        elif k == 'pace':
            ok = v in ('walk', 'run')
        elif k == 'posture':
            ok = v in ('standing', 'sitting', 'lying')
        elif k == 'buff':
            ok = v == 'fed'
        elif k == 'quantity':
            ok = integer(v, 1, 100)
        elif k == 'hours':
            ok = integer(v, 1, 168)
        elif k == 'points' and kind == 'story.area.damage':
            ok = integer(v, 1, 99)
        elif k == 'points':
            ok = integer(v, -100, 100) and v != 0
        elif k == 'days':
            ok = integer(v, 1, 112 if kind == 'band.override' else 366)
        elif k == 'word' and kind == 'harvest.override':
            ok = v in ('poor', 'fair', 'good', 'clear')
        elif k == 'band':
            ok = v in ('destitute', 'poor', 'comfortable', 'rich', 'clear')
        elif k == 'add' and kind == 'character.injury':
            ok = v in INJURY_TYPES
        elif k == 'severity':
            ok = integer(v, 1, 3)
        elif k in ('text', 'name', 'title', 'detail', 'words'):
            ok = word(v, 200 if k in ('text', 'detail') else 80)
        else:
            ok = identifier(v) or (k == 'player' and v == '$actor')
        require(ok, f'{pid}: invalid {kind}.{k}.')
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


def validate(p):
    shape(
        p,
        ('format', 'version', 'id', 'title', 'audience', 'start', 'phases', 'tests'),
        ('triggers', 'regions', 'scenes'),
    )
    scenes = p.get('scenes', [])
    require(isinstance(scenes, list) and len(scenes) <= 32, 'Choose up to 32 embedded scenes.')
    scene_docs = {}
    for scene in scenes:
        story_scenes.validate(scene)
        require(scene['id'] not in scene_docs, 'Embedded scenes need distinct IDs.')
        scene_docs[scene['id']]=scene
    rectangles = p.get('regions', [])
    require(isinstance(rectangles, list) and len(rectangles) <= 128, 'Choose up to 128 named regions.')
    names = set()
    for r in rectangles:
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
    require(
        p['format'] == 'ratw-story-program' and type(p['version']) is int and p['version'] == 1,
        'Unknown story format or version.',
    )
    require(
        identifier(p['id']) and word(p['title'], 100),
        'Give this story an ID and title.',
    )
    a = p['audience']
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
    triggers.validate({'id': 'campaign', 'missions': [], 'triggers': shared, '_regions': region_index(p), '_scenes': scene_docs})
    ids = set()
    for phase in p['phases']:
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
        speakers = set()
        for effect in phase['effects']:
            validate_effect(effect, pid, phase.get('_regions'), phase.get('_scenes'))
            kind = effect['kind']
            if kind == 'npc.line':
                require(
                    effect['npc'] not in speakers,
                    f'{pid}: two lines for the same resident.',
                )
                speakers.add(effect['npc'])
        require(
            isinstance(phase['exits'], list) and len(phase['exits']) <= 16,
            f'{pid}: too many exits.',
        )
        mission_ids = validate_missions(phase)
        triggers.validate(phase)
        for mission in phase.get('missions', []):
            for objective in mission['objectives']:
                check_region_source(objective, phase.get('_regions'))
        for e in phase['exits']:
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
                continue
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
    require(identifier(p['start']) and p['start'] in ids, 'Unknown opening phase.')
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
    require(
        isinstance(p['tests'], list) and 1 <= len(p['tests']) <= 32,
        'Include 1–32 rehearsal cases.',
    )
    return p


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


def _review(p):
    validate(p)
    reports = []
    for case in p['tests']:
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
        state = begin(p, case['cohort'], 0)
        path, intended, last_day, seen = [], [], 0, {}
        visit_credits, trace = [], []
        elapsed = 0

        record_entry(p, state, path, intended, visit_credits)
        for ev in case['events']:
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
            if ev['id'] in seen:
                require(ev == seen[ev['id']], 'A repeated event ID has different contents.')
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
        missions = mission_snapshot(phase_of(p, state), state)
        credits = mission_credits(missions or state.get('previousMissions', []))
        if missions:
            visit_credits.append({'phase': state['phase'], 'visit': state['visits'], 'credits': credits})
        expected_effects = case.get('expectEffects', intended)
        require(isinstance(expected_effects, list), 'Expected effects must be a list.')
        reports.append(
            {
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
        )
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
