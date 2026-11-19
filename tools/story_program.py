"""The same deterministic world-story rules for Studio, authoring tools and the world's runner."""

import hashlib
import json
import math
import re

EVENTS = (
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
    'clock',
    'dm signal',
)
FIELDS = ('actor', 'target', 'cell', 'item', 'detail')
# Actions here map to existing, bounded world operations. Story-only dialogue expires on phase exit.
EFFECTS = {
    'npc.line': ('npc', 'text'),
    'npc.move': ('npc', 'cell', 'x', 'y'),
    'festival.call': ('town', 'name', 'inDays'),
    'storyline.give': ('player', 'template', 'resident', 'place'),
    'faction.relation': ('faction', 'other', 'stance', 'disposition', 'reason'),
    'bandits.call': ('player', 'count'),
    'story.practice': ('player', 'skill'),
    'times.nudge': ('market', 'points', 'days', 'words'),
    'deed.award': ('player', 'detail'),
    'project.post': ('town', 'project', 'cell', 'x', 'y', 'title'),
}
ID = re.compile(r'[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}\Z')


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
    require(isinstance(missions, list) and len(missions) <= 16, 'Choose up to 16 missions in a phase.')
    ids = set()
    for m in missions:
        shape(m, ('id', 'title', 'mode', 'objectives'), ('players', 'percent', 'contributors'))
        require(
            identifier(m['id']) and m['id'] not in ids and word(m['title'], 120),
            'Each mission needs a unique ID and title.',
        )
        ids.add(m['id'])
        require(m['mode'] in ('individual', 'group', 'either'), 'Choose individual, group, or either.')
        require(
            'contributors' not in m or (m['mode'] == 'group' and integer(m['contributors'], 2, 500)),
            'A group needs at least two contributors.',
        )
        require(not ('players' in m and 'percent' in m), 'Choose a number or percentage for a personal mission.')
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
            shape(o, ('label', 'on'), ('match', 'goal', 'measure', 'sameAsFirst'))
            require(
                'sameAsFirst' not in o or (index > 0 and o['sameAsFirst'] in FIELDS),
                'Choose a field from the first objective to link a later objective.',
            )
            require(
                word(o['label'], 180) and o['on'] in MISSION_EVENTS,
                'Choose a participant event and a journal instruction.',
            )
            require(
                o.get('measure', 'count') in ('count', 'quantity', 'coins') and integer(o.get('goal', 1), 1, 1000000),
                'Choose a positive objective goal and measure.',
            )
            measure = o.get('measure', 'count')
            require(
                measure == 'count' or o['on'] in MISSION_MEASURES[measure],
                f'{o["on"]} does not record {measure}. Choose occurrences or another event.',
            )
            fields = o.get('match', {})
            require(
                isinstance(fields, dict) and all(k in FIELDS and word(v, 400) for k, v in fields.items()),
                'Invalid mission event filter.',
            )
    require(not missions or phase['exits'], 'A mission needs an ongoing phase.')
    return ids


def track_missions(phase, state, event):
    actor = event.get('actor', '')
    if not phase.get('missions') or actor not in state['cohort']:
        return
    ledger = state.get('missions', {})
    for mission in phase['missions']:
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
        if not matches(objective, evidence):
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


def validate(p):
    shape(p, ('format', 'version', 'id', 'title', 'audience', 'start', 'phases', 'tests'))
    require(
        p['format'] == 'ratw-story-program' and type(p['version']) is int and p['version'] == 1,
        'Unknown story format or version.',
    )
    require(identifier(p['id']) and word(p['title'], 100), 'Give this story an ID and title.')
    a = p['audience']
    if isinstance(a, dict) and a.get('kind') == 'active':
        shape(a, ('kind', 'days'))
        require(integer(a['days'], 1, 90), 'Active audience needs a lookback of 1–90 real days.')
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
    require(isinstance(p['phases'], list) and 1 <= len(p['phases']) <= 100, 'A story has 1–100 phases.')
    ids = set()
    for phase in p['phases']:
        shape(phase, ('id', 'journal', 'effects', 'exits'), ('guestVoices', 'missions'))
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
            and (not voices or phase['exits']),
            f'{pid}: choose up to eight distinct guest speakers in an ongoing phase.',
        )
        require(isinstance(phase['effects'], list) and len(phase['effects']) <= 12, f'{pid}: too many effects.')
        speakers = set()
        for effect in phase['effects']:
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
                elif k == 'points':
                    ok = integer(v, -100, 100) and v != 0
                elif k == 'days':
                    ok = integer(v, 1, 366)
                elif k in ('text', 'name', 'title', 'detail', 'words'):
                    ok = word(v, 200 if k in ('text', 'detail') else 80)
                else:
                    ok = identifier(v) or (k == 'player' and v == '$actor')
                require(ok, f'{pid}: invalid {kind}.{k}.')
            if kind == 'faction.relation':
                require(effect['faction'] != effect['other'], 'A faction needs a different counterpart.')
            if kind == 'npc.line':
                require(effect['npc'] not in speakers, f'{pid}: two lines for the same resident.')
                speakers.add(effect['npc'])
        require(isinstance(phase['exits'], list) and len(phase['exits']) <= 16, f'{pid}: too many exits.')
        mission_ids = validate_missions(phase)
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
            require(e['on'] in EVENTS and identifier(e['to']), f'{pid}: unknown trigger or destination.')
            m = e.get('match', {})
            require(
                isinstance(m, dict) and all(k in FIELDS and word(v, 400) for k, v in m.items()),
                f'{pid}: invalid event filter.',
            )
            require(
                'participant' not in e or type(e['participant']) is bool, f'{pid}: participant must be true or false.'
            )
            for key, hi in [('players', 500), ('percent', 100), ('minQuantity', 1000000), ('minCoins', 1000000000)]:
                if key in e:
                    require(integer(e[key], 1, hi), f'{pid}: invalid {key}.')
            require(not ('players' in e and 'percent' in e), f'{pid}: choose one participation gate.')
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
            if 'afterDays' in e or e['on'] == 'clock':
                require(number(e.get('afterDays'), 1 / 1440, 36500), f'{pid}: set a positive game-time delay.')
            if e['on'] == 'clock':
                require(not (set(e) - {'on', 'to', 'afterDays'}), f'{pid}: clock exits only take a delay.')
            if e['on'] == 'dm signal':
                require(
                    set(e) == {'on', 'to', 'match'} and set(m) == {'detail'} and word(m.get('detail'), 80),
                    f'{pid}: name the permitted DM signal.',
                )
    require(identifier(p['start']) and p['start'] in ids, 'Unknown opening phase.')
    phases = {s['id']: s for s in p['phases']}
    reached, todo = set(), [p['start']]
    while todo:
        pid = todo.pop()
        if pid in reached:
            continue
        require(pid in ids, 'Unknown destination ' + pid)
        reached.add(pid)
        todo.extend(e['to'] for e in phases[pid]['exits'])
    require(reached == ids, 'Unreachable phases: ' + ', '.join(sorted(ids - reached)))
    for phase in p['phases']:
        if any(effect.get('player') == '$actor' for effect in phase['effects']):
            incoming = [e for other in p['phases'] for e in other['exits'] if e['to'] == phase['id']]
            require(
                phase['id'] != p['start']
                and all(
                    e['on'] == MISSION_GATE or e.get('participant') or 'players' in e or 'percent' in e
                    for e in incoming
                ),
                'A reward for $actor requires a participating player on every incoming path.',
            )
        require(
            phase['exits'] or all(e['kind'] != 'npc.line' for e in phase['effects']),
            'A speaking phase needs an exit.',
        )
    require(isinstance(p['tests'], list) and 1 <= len(p['tests']) <= 32, 'Include 1–32 rehearsal cases.')
    return p


def digest(program):
    return hashlib.sha256(
        json.dumps(program, ensure_ascii=False, allow_nan=False, sort_keys=True, separators=(',', ':')).encode()
    ).hexdigest()


def begin(p, cohort, day):
    return {'phase': p['start'], 'entered': day, 'cohort': sorted(set(cohort)), 'votes': {}, 'visits': 1, 'actor': ''}


def phase_of(p, state):
    return next(s for s in p['phases'] if s['id'] == state['phase'])


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


def advance(p, state, event, day):
    """Consume one ordered fact. The caller commits its cursor together with votes and entry effects."""
    if day < state['entered']:
        return False
    phase = phase_of(p, state)
    track_missions(phase, state, event)
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
            needed = edge.get('players', max(1, math.ceil(len(state['cohort']) * edge.get('percent', 100) / 100)))
            if len(votes) < needed:
                continue
        state.pop('previousMissions', None)
        if phase.get('missions'):
            state['previousMissions'] = mission_snapshot(phase, state)
        state.pop('missions', None)
        state.update(phase=edge['to'], entered=day, votes={}, progress={}, visits=state['visits'] + 1, actor=actor)
        return True
    return False


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


def review(p):
    validate(p)
    reports = []
    for case in p['tests']:
        shape(
            case, ('name', 'cohort', 'events', 'expect'), ('expectEffects', 'expectMissions', 'expectMissionVisits')
        )
        require(
            word(case['name'], 100)
            and isinstance(case['cohort'], list)
            and len(case['cohort']) <= 500
            and all(identifier(v) for v in case['cohort']),
            'Invalid rehearsal name or cohort.',
        )
        if p['audience']['kind'] == 'players':
            require(set(case['cohort']) == set(p['audience']['players']), 'Use the fixed audience in the rehearsal.')
        require(
            isinstance(case['events'], list)
            and len(case['events']) <= 256
            and isinstance(case['expect'], list)
            and all(identifier(v) for v in case['expect']),
            'Invalid rehearsal events or expected phases.',
        )
        state = begin(p, case['cohort'], 0)
        path, intended, last_day, seen = [], [], 0, {}
        visit_credits = []

        record_entry(p, state, path, intended, visit_credits)
        for ev in case['events']:
            shape(ev, ('id', 'kind', 'day'), (*FIELDS, 'quantity', 'coins'))
            require(
                identifier(ev['id'])
                and ev['kind'] in EVENTS
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
            last_day = ev['day']
            if ev['kind'] not in ('clock', 'dm signal') and advance(p, state, {'kind': 'clock'}, last_day):
                record_entry(p, state, path, intended, visit_credits)
            if advance(p, state, ev, last_day):
                record_entry(p, state, path, intended, visit_credits)
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
                and visit_credits == case.get('expectMissionVisits', visit_credits),
                'missionCredits': credits,
                'missionVisits': visit_credits,
                'path': path,
                'expected': case['expect'],
                'effects': intended,
            }
        )
    covered = {phase for case in reports for phase in case['path']}
    return {
        'digest': digest(p),
        'uncovered': [s['id'] for s in p['phases'] if s['id'] not in covered],
        'passed': all(r['passed'] for r in reports),
        'tests': reports,
        'voices': [{'phase': s['id'], 'npc': npc} for s in p['phases'] for npc in s.get('guestVoices', [])],
        'effects': [{'phase': s['id'], **e} for s in p['phases'] for e in s['effects']],
    }
