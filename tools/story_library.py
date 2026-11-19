"""Portable event types. Each entry becomes a mission in a campaign or an independently rehearsable story."""

import json
from pathlib import Path

LIBRARY = Path(__file__).resolve().parents[1] / 'Data/Storylines/event-library.json'


def entries():
    return json.loads(LIBRARY.read_text(encoding='utf-8'))


def mission(entry):
    return {key: entry[key] for key in ('id', 'title', 'mode', 'objectives')}


def blank():
    return {
        'format': 'ratw-story-program',
        'version': 1,
        'id': 'new-story',
        'title': 'New story',
        'audience': {'kind': 'active', 'days': 7},
        'start': 'opening',
        'phases': [{'id': 'opening', 'journal': 'The story begins.', 'effects': [], 'exits': [], 'triggers': []}],
        'tests': [{'name': 'Opening', 'cohort': ['player-ash'], 'events': [], 'expect': ['opening']}],
    }


def components(entry):
    """Copyable rules and journal listeners; the same pieces are used to construct the playable example."""
    parts = []
    if opening := entry.get('opening'):
        parts.append(
            {
                'role': 'opening',
                'label': opening['journal'],
                'trigger': {
                    'id': entry['id'] + '-opening',
                    **{k: v for k, v in opening.items() if k != 'journal'},
                    'actions': [{'do': 'enter', 'phase': 'work'}],
                },
            }
        )
    for i, o in enumerate(entry['objectives'], 1):
        name = entry['id'] + '-' + str(i)
        rule = {'id': name, 'on': o['on'], 'audience': 'participant', 'actions': [{'do': 'emit', 'signal': name}]}
        if 'match' in o:
            rule['match'] = o['match']
        objective = {k: v for k, v in o.items() if k not in ('on', 'match')}
        parts.append(
            {
                'role': 'objective',
                'label': o['label'],
                'trigger': rule,
                'objective': {**objective, 'on': 'story signal', 'signal': name},
            }
        )
    mode = entry['mode']
    gate = {'mode': mode}
    if mode == 'individual':
        gate['percent'] = 100
    if mode == 'group':
        gate['contributors'] = 2
    labels = {
        'individual': 'Every eligible player finishes a personal checklist',
        'group': 'At least two different players contribute',
        'either': 'One player or a shared effort',
    }
    parts.append({'role': 'participation', 'label': labels[mode], 'gate': gate})
    for effect in entry.get('ending', []):
        parts.append({'role': 'outcome', 'label': effect['kind'], 'action': {'do': 'effect', 'effect': effect}})
    return parts


def draft(event_id):
    entry = next((e for e in entries() if e['id'] == event_id), None)
    if not entry:
        raise ValueError('Unknown event type.')
    parts = components(entry)
    evidence = [part for part in parts if part['role'] == 'objective']
    gate = next(part['gate'] for part in parts if part['role'] == 'participation')
    m = {**mission(entry), **gate, 'objectives': [part['objective'] for part in evidence]}
    work = {
        'id': 'work',
        'journal': m['title'],
        'missions': [m],
        'triggers': [part['trigger'] for part in evidence],
        'effects': [],
        'exits': [{'on': 'missions completed', 'missions': [m['id']], 'to': 'resolved'}],
    }
    outcomes = [part['action'] for part in parts if part['role'] == 'outcome']
    if outcomes:
        work['triggers'].append(
            {
                'id': entry['id'] + '-outcome',
                'on': 'story signal',
                'signal': m['objectives'][-1]['signal'],
                'if': {'test': 'mission', 'id': m['id']},
                'audience': 'participant',
                'once': 'visit',
                'actions': outcomes,
            }
        )
    phases = [
        work,
        {'id': 'resolved', 'journal': m['title'] + ' is complete.', 'effects': [], 'exits': []},
    ]
    opening = entry.get('opening')
    if opening:
        phases.insert(
            0,
            {
                'id': 'watch',
                'journal': opening['journal'],
                'effects': [],
                'exits': [],
                'triggers': [parts[0]['trigger']],
            },
        )
    first, second = 'player-ash', 'player-birch'

    def scenario(name, team):
        events = []

        def add(on, fields):
            events.append({'kind': on, 'day': 0, 'id': 'fact-' + str(len(events) + 1), **fields})

        if opening:
            add(opening['on'], opening.get('match', {}))
        links = {o['sameAsFirst']: 'fixture-reference' for o in entry['objectives'] if 'sameAsFirst' in o}
        for i, o in enumerate(entry['objectives']):
            goal = o.get('goal', 1)
            measure = o.get('measure', 'count')
            # Make every team member contribute even for a single bulk delivery.
            amounts = [1] * goal if measure == 'count' else [goal]
            if len(entry['objectives']) == 1 and len(team) > 1 and len(amounts) == 1:
                amounts = [max(1, goal // 2), max(1, goal - goal // 2)]
            for j, n in enumerate(amounts):
                add(
                    o['on'],
                    {
                        'actor': team[(i + j) % len(team)],
                        **(
                            links
                            if i == 0
                            else {o['sameAsFirst']: links[o['sameAsFirst']]}
                            if 'sameAsFirst' in o
                            else {}
                        ),
                        **o.get('match', {}),
                        **({measure: n} if measure != 'count' else {}),
                    },
                )
        return {
            'name': name,
            'cohort': [first, second],
            'events': events,
            'expect': [p['id'] for p in phases],
            'expectMissions': {m['id']: sorted(team)},
        }

    team = [first] if m['mode'] == 'individual' else [first, second]
    cases = [scenario('Complete the objectives', team)]
    if m['mode'] == 'individual':
        cases[0]['cohort'] = [first]
        waiting = scenario('A personal finisher leaves the mission open for other players', [first])
        waiting['expect'] = waiting['expect'][:-1]
        cases.append(waiting)
    if m['mode'] == 'either':
        cases.append(scenario('One wolf completes the same mission', [first]))
    if m['mode'] == 'group':
        solo = scenario('A lone volunteer still needs another contributor', [first])
        solo['expect'] = solo['expect'][:-1]
        solo['expectMissions'][m['id']] = []
        cases.append(solo)
    return {
        'format': 'ratw-story-program',
        'version': 1,
        'id': event_id,
        'title': entry['title'],
        'audience': {'kind': 'active', 'days': 7},
        'start': phases[0]['id'],
        'phases': phases,
        'tests': cases,
    }
