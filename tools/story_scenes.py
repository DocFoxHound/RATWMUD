"""Portable authored scenes. Studio and the offline author bridge use these same checks and previews."""

import re
import json
from pathlib import Path

SCENE_DIR = Path(__file__).resolve().parents[1] / 'Data/Storylines/scenes'
OPERATIONS = ('scene.draft', 'scene.import', 'scene.validate', 'scene.rehearse', 'scene.wrap')



VISITOR = re.compile(r'[a-z0-9][a-z0-9_-]{2,39}')   # (Doc 86, T3: a stranger cast in a scene: visitor.add's id.)

def examples():
    return {f.stem: json.loads(f.read_text(encoding='utf8')) for f in sorted(SCENE_DIR.glob('*.json'))}


def blank():
    return {
        'format': 'ratw-scene',
        'version': 1,
        'id': 'new-scene',
        'title': 'New scene',
        'visibility': 'individual',
        'lockMovement': False,
        'cast': [{'id': 'speaker', 'selector': {'role': 'civilian'}}],
        'start': 'greeting',
        'beats': [
            {'id': 'greeting', 'type': 'line', 'speaker': 'speaker', 'text': 'I was hoping to see you.', 'seconds': 4}
        ],
    }


def validate(scene):
    from story_program import identifier, integer, require, shape, word

    shape(
        scene,
        ('format', 'version', 'id', 'title', 'visibility', 'lockMovement', 'cast', 'beats', 'start'),
        ('area', 'camera'),
    )
    require(
        scene['format'] == 'ratw-scene' and integer(scene['version'], 1, 1), 'Unknown scene document format or version.'
    )
    require(identifier(scene['id']) and word(scene['title'], 100), 'Give the scene an ID and title.')
    require(
        scene['visibility'] in ('public', 'individual', 'party') and type(scene['lockMovement']) is bool,
        'Choose scene visibility and a movement setting.',
    )
    require(scene['visibility'] != 'public' or 'area' in scene, 'A public scene needs a viewing area.')
    if 'area' in scene:
        area = scene['area']
        shape(area, ('cell', 'x1', 'y1', 'x2', 'y2'))
        require(
            identifier(area['cell'])
            and all(integer(area[k], 0, 4095) for k in ('x1', 'y1', 'x2', 'y2'))
            and area['x1'] <= area['x2']
            and area['y1'] <= area['y2'],
            'Invalid scene viewing rectangle.',
        )

    def tile(row):
        require(
            identifier(row['cell']) and integer(row['x'], 0, 4095) and integer(row['y'], 0, 4095), 'Invalid scene tile.'
        )

    if 'camera' in scene:
        shape(scene['camera'], ('cell', 'x', 'y'))
        tile(scene['camera'])
    require(isinstance(scene['cast'], list) and 1 <= len(scene['cast']) <= 16, 'Choose 1–16 cast roles.')
    cast = set()
    for role in scene['cast']:
        shape(role, ('id',), ('npc', 'selector', 'visitor'))
        require(identifier(role['id']) and role['id'] not in cast, 'Give each cast role a unique ID.')
        cast.add(role['id'])
        # (Doc 86, T3: or a stranger the same story brings with "Bring a stranger", by their id.)
        require(sum(k in role for k in ('npc', 'selector', 'visitor')) == 1, 'Choose a resident, a cast selector or a stranger.')
        if 'npc' in role:
            require(identifier(role['npc']), 'Choose a resident ID.')
        elif 'visitor' in role:
            require(isinstance(role['visitor'], str) and VISITOR.fullmatch(role['visitor']) is not None,
                    "A stranger's id is 3 to 40 small letters, digits, - and _.")
        else:
            selector = role['selector']
            shape(selector, (), ('role', 'cell'))
            require(selector and all(identifier(v) for v in selector.values()), 'Select a role, a cell, or both.')
    require(isinstance(scene['beats'], list) and 1 <= len(scene['beats']) <= 64, 'Choose 1–64 scene beats.')
    edges = {}
    for beat in scene['beats']:
        require(
            isinstance(beat, dict) and identifier(beat.get('id')) and beat['id'] not in edges,
            'Give each beat a unique ID.',
        )
        kind = beat.get('type')
        required = {
            'line': ('text', 'seconds'),
            'choice': ('text', 'seconds', 'mode', 'options', 'default'),
            'move': ('cell', 'x', 'y', 'pace'),
            'pose': ('posture',),
            'response': ('text', 'seconds', 'mode'),
        }
        require(isinstance(kind, str) and kind in required, 'Unknown scene beat type.')
        optional = {'line': ('ai', 'next'), 'choice': ('ai',), 'response': ('next', 'timeoutNext', 'ai')}
        shape(beat, ('id', 'type', 'speaker', *required[kind]), optional.get(kind, ('next',)))
        if 'ai' in beat:
            shape(beat['ai'], ('personality', 'intent'))
            require(
                all(word(beat['ai'][key], 600) for key in ('personality', 'intent')),
                'Give the AI a personality and intent of up to 600 characters each.',
            )
        require(identifier(beat['speaker']) and beat['speaker'] in cast, 'Choose an existing cast role for this beat.')
        if kind in ('line', 'choice', 'response'):
            require(
                word(beat['text'], 400) and integer(beat['seconds'], 1, 600),
                'Give the beat text and a duration of 1–600 real seconds.',
            )
        if kind == 'move':
            tile(beat)
            require(beat['pace'] in ('walk', 'run'), 'Choose walk or run.')
        if kind == 'pose':
            require(beat['posture'] in ('standing', 'sitting', 'lying'), 'Unknown scene posture.')
        links = []
        if kind == 'choice':
            require(
                beat['mode'] in ('first', 'majority')
                and isinstance(beat['options'], list)
                and 2 <= len(beat['options']) <= 8,
                'A first or majority choice needs 2–8 options.',
            )
            ids = set()
            for option in beat['options']:
                shape(option, ('id', 'label'), ('next',))
                require(
                    identifier(option['id']) and option['id'] not in ids and word(option['label'], 80),
                    'Give each choice option a unique ID and label.',
                )
                ids.add(option['id'])
                if 'next' in option:
                    links.append(option['next'])
            require(identifier(beat['default']) and beat['default'] in ids, 'Choose a default option for the timeout.')
        else:
            if 'next' in beat:
                links.append(beat['next'])
            if kind == 'response':
                require(beat['mode'] in ('first', 'all'), 'Choose first or all responses.')
                if 'timeoutNext' in beat:
                    links.append(beat['timeoutNext'])
        require(all(identifier(link) for link in links), 'Invalid destination beat ID.')
        edges[beat['id']] = links
    require(identifier(scene['start']) and scene['start'] in edges, 'Choose an existing opening beat.')
    visited = set()

    def walk(bid, ancestors):
        require(bid in edges, 'Unknown destination beat ' + bid)
        require(bid not in ancestors, 'Scene beat cycle at ' + bid)
        if bid not in visited:
            for nxt in edges[bid]:
                walk(nxt, ancestors | {bid})
            visited.add(bid)

    walk(scene['start'], set())
    require(len(visited) == len(edges), 'Unreachable scene beats: ' + ', '.join(sorted(edges.keys() - visited)))
    return scene


def rehearse(scene, choices, responses):
    from story_program import digest, identifier, require

    validate(scene)
    require(
        isinstance(choices, list) and len(choices) <= 64 and all(identifier(i) for i in choices),
        'Supply ordered option IDs for this path.',
    )
    require(
        isinstance(responses, dict)
        and all(identifier(k) and v in ('timeout', 'submitted') for k, v in responses.items()),
        'Supply response beat IDs mapped to submitted or timeout.',
    )
    visited_responses = set()
    beats = {b['id']: b for b in scene['beats']}
    path, selected, seconds = [], [], 0
    current = scene['start']
    while current:
        beat = beats[current]
        seconds += beat.get('seconds', 0)
        current = beat.get('next')
        if beat['type'] == 'choice':
            supplied = len(selected) < len(choices)
            choice = choices[len(selected)] if supplied else beat['default']
            option = next((o for o in beat['options'] if o['id'] == choice), None)
            require(option is not None, f'Unknown option {choice} in beat {beat["id"]}.')
            current = option.get('next')
            selected.append(choice)
            beat = {**beat, 'chosen': choice, 'by': 'selection' if supplied else 'timeout'}
        if beat['type'] == 'response':
            visited_responses.add(beat['id'])
            by = responses.get(beat['id'], 'timeout')
            if by == 'timeout':
                current = beat.get('timeoutNext', current)
            beat = {**beat, 'by': by}
        path.append(beat)
    require(len(choices) <= len(selected), 'Unused choices after this scene path ends.')
    require(not (responses.keys() - visited_responses), 'Unused response expectations on this path.')
    return {
        'scene': scene['id'],
        'title': scene['title'],
        'digest': digest(scene),
        'beats': path,
        'choices': selected,
        'ending': path[-1]['id'],
        'durationSeconds': seconds,
    }


def wrap(scene, player=None):
    from story_program import identifier, require

    validate(scene)
    require(player is None or identifier(player), 'Choose a character ID for the audience.')
    public = scene['visibility'] == 'public'
    wait_for_arrival = not public and player is None
    journal_title = 'A private scene' if wait_for_arrival else scene['title']
    play = {
        'kind': 'scene.play',
        'scene': scene['id'],
        'cue': 'presentation',
        'player': 'world' if public else player or '$actor',
    }
    playing = {
        'id': 'playing',
        'journal': journal_title,
        'effects': [] if wait_for_arrival else [play],
        'exits': [
            {'on': 'scene finished', 'match': {'detail': 'presentation'}, 'to': 'complete'},
            {'on': 'scene failed', 'match': {'detail': 'presentation'}, 'to': 'interrupted'},
        ],
    }
    phases = [playing]
    start = 'playing'
    actor = player or 'player-ash'
    opening = []
    if wait_for_arrival:
        start = 'arrival'
        phases.insert(
            0,
            {
                'id': start,
                'journal': journal_title,
                'effects': [],
                'exits': [],
                'triggers': [
                    {
                        'id': 'begin',
                        'on': 'arrival',
                        'audience': 'participant',
                        'actions': [{'do': 'effect', 'effect': play}, {'do': 'enter', 'phase': 'playing'}],
                    }
                ],
            },
        )
        opening = [{'kind': 'arrival', 'actor': actor, 'day': 0, 'id': 'arrive'}]
    cases = []
    for event, end, journal in [
        ('scene finished', 'complete', 'The scene has ended.'),
        ('scene failed', 'interrupted', 'The scene was interrupted.'),
    ]:
        phases.append({'id': end, 'journal': journal, 'effects': [], 'exits': []})
        cases.append(
            {
                'name': journal,
                'cohort': [actor],
                'events': [*opening, {'kind': event, 'detail': 'presentation', 'id': end, 'day': 1}],
                'expect': ([start] if wait_for_arrival else []) + ['playing', end],
            }
        )
    return {
        'format': 'ratw-story-program',
        'version': 1,
        'id': scene['id'],
        'title': journal_title,
        'audience': {'kind': 'players', 'players': [player]} if player else {'kind': 'active', 'days': 7},
        'scenes': [scene],
        'start': start,
        'phases': phases,
        'tests': cases,
    }


def request(data):
    import story_program as rules

    op = data['op']
    if op == 'scene.draft':
        if 'example' not in data:
            return {'scene': blank()}
        name = data['example']
        available = examples()
        rules.require(isinstance(name, str) and name in available, 'Choose a known scene example.')
        return {'scene': available[name]}
    scene = validate(data.get('scene'))
    if op == 'scene.validate':
        return {'digest': rules.digest(scene)}
    if op == 'scene.rehearse':
        return rehearse(scene, data.get('choices', []), data.get('responses', {}))
    if op == 'scene.wrap':
        return {'program': wrap(scene, data.get('player'))}
    rules.require(op == 'scene.import', 'Unknown scene operation.')
    p = data.get('program')
    rules.require(isinstance(p, dict) and isinstance(p.get('scenes', []), list), 'Choose a story program.')
    existing = p.get('scenes', [])
    rules.require(
        not any(isinstance(s, dict) and s.get('id') == scene['id'] for s in existing),
        'A scene with this ID already exists.',
    )
    return {'program': rules.validate({**p, 'scenes': [*existing, scene]})}
