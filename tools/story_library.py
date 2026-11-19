"""Portable event types. Each entry becomes a mission in a campaign or an independently rehearsable story."""

import json
from pathlib import Path

LIBRARY = Path(__file__).resolve().parents[1] / 'Data/Storylines/event-library.json'


def entries():
    return json.loads(LIBRARY.read_text(encoding='utf-8'))


def mission(entry):
    return {key: entry[key] for key in ('id', 'title', 'mode', 'objectives')}


def draft(event_id):
    entry = next((e for e in entries() if e['id'] == event_id), None)
    if not entry:
        raise ValueError('Unknown event type.')
    m = mission(entry)
    if m['mode'] == 'individual':
        m['percent'] = 100  # Every member may finish a standalone personal story on their own schedule.
    work = {
        'id': 'work',
        'journal': m['title'],
        'missions': [m],
        'effects': [],
        'exits': [{'on': 'missions completed', 'missions': [m['id']], 'to': 'resolved'}],
    }
    phases = [
        work,
        {'id': 'resolved', 'journal': m['title'] + ' is complete.', 'effects': entry.get('ending', []), 'exits': []},
    ]
    opening = entry.get('opening')
    if opening:
        phases.insert(
            0,
            {
                'id': 'watch',
                'journal': opening['journal'],
                'effects': [],
                'exits': [{k: v for k, v in opening.items() if k != 'journal'} | {'to': 'work'}],
            },
        )
    first, second = 'player-ash', 'player-birch'

    def scenario(name, team):
        events = []

        def add(on, fields):
            events.append({'kind': on, 'day': 0, 'id': 'fact-' + str(len(events) + 1), **fields})

        if opening:
            add(opening['on'], opening.get('match', {}))
        links = {o['sameAsFirst']: 'fixture-reference' for o in m['objectives'] if 'sameAsFirst' in o}
        for i, o in enumerate(m['objectives']):
            goal = o.get('goal', 1)
            measure = o.get('measure', 'count')
            # Make every team member contribute even for a single bulk delivery.
            amounts = [1] * goal if measure == 'count' else [goal]
            if len(m['objectives']) == 1 and len(team) > 1 and len(amounts) == 1:
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
