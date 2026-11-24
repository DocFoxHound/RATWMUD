"""Version 2 story programs that use every new condition and action (doc 85, Phase 5), with rehearsal cases that pin what
they do: tools/test_story_v2.py checks them, and tools/test_story_native.py runs them through both engines."""

import copy

W = ['ash', 'birch', 'cedar']


def ev(i, kind, day, **fields):
    return {'id': f'e{i}', 'kind': kind, 'day': day, **fields}


LEDGER = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'v2-ledger', 'title': 'The rope ledger',
    'audience': {'kind': 'players', 'players': W}, 'start': 'gather',
    'variables': [
        {'id': 'ropes', 'type': 'number', 'scope': 'story', 'show': 'Ropes delivered'},
        {'id': 'hauled', 'type': 'number', 'scope': 'player'},
        {'id': 'gate-open', 'type': 'switch', 'scope': 'story'},
        {'id': 'finder', 'type': 'wolf', 'scope': 'story'},
        {'id': 'signed', 'type': 'wolves', 'scope': 'story'},
        {'id': 'clues', 'type': 'texts', 'scope': 'party'},
        {'id': 'best', 'type': 'number', 'scope': 'world', 'initial': 2},
        {'id': 'since-gate', 'type': 'timer', 'scope': 'story'},
        {'id': 'cheers', 'type': 'number', 'scope': 'story', 'initial': 10},
        {'id': 'motto', 'type': 'text', 'scope': 'story', 'initial': 'hold fast'},
    ],
    'phases': [
        {'id': 'gather', 'journal': 'Bring rope to the gate.', 'effects': [], 'exits': [], 'triggers': [
            {'id': 'rope', 'on': 'gift', 'audience': 'participant', 'match': {'item': 'rope'}, 'actions': [
                {'do': 'add', 'var': 'hauled', 'value': {'field': 'quantity'}},
                {'do': 'add', 'var': 'ropes', 'value': {'field': 'quantity'}},
                {'do': 'list add', 'var': 'signed', 'value': {'field': 'actor'}},
                {'do': 'if', 'if': {'test': 'compare', 'left': {'var': 'ropes'}, 'op': '>=', 'right': 5},
                 'then': [{'do': 'turn on', 'var': 'gate-open'}, {'do': 'remember', 'var': 'finder', 'from': 'actor'},
                          {'do': 'start timer', 'var': 'since-gate'}, {'do': 'note', 'text': 'The gate opens.'},
                          {'do': 'enter', 'phase': 'gate'}],
                 'elif': [{'if': {'test': 'compare', 'left': {'count': 'signed'}, 'op': '>=', 'right': 2},
                           'then': [{'do': 'note', 'text': 'Two have signed.'},
                                    {'do': 'effect', 'effect': {'kind': 'deed.award', 'player': '$actor', 'detail': 'Signed second.'}}]}],
                 'else': [{'do': 'note', 'text': 'More rope wanted.'}]},
            ]},
            {'id': 'talk', 'on': 'conversation', 'audience': 'participant', 'actions': [
                {'do': 'list add', 'var': 'clues', 'value': {'field': 'detail'}},
                {'do': 'if', 'if': {'test': 'in list', 'list': 'clues', 'value': 'smith'},
                 'then': [{'do': 'run', 'trigger': 'praise', 'check': True}, {'do': 'enable', 'trigger': 'praise'},
                          {'do': 'run', 'trigger': 'praise', 'check': True}]},
            ]},
            {'id': 'praise', 'on': 'story signal', 'signal': 'never-sent', 'enabled': False, 'actions': [
                {'do': 'subtract', 'var': 'cheers', 'value': 3},
                {'do': 'multiply', 'var': 'cheers', 'value': 4},
                {'do': 'clamp', 'var': 'cheers', 'min': 0, 'max': 20},
                {'do': 'set', 'var': 'motto', 'value': 'the smith knows'},
            ]},
        ]},
        {'id': 'gate', 'journal': 'The gate is open.', 'effects': [], 'exits': [], 'triggers': [
            {'id': 'arrive', 'on': 'arrival', 'audience': 'participant', 'once': 'player', 'actions': [
                {'do': 'each', 'over': 'participants', 'actions': [
                    {'do': 'add', 'var': 'hauled', 'of': '$each', 'value': 1},
                    {'do': 'if', 'if': {'test': 'compare', 'left': {'var': 'hauled', 'of': '$each'}, 'op': '>', 'right': 3},
                     'then': [{'do': 'effect', 'effect': {'kind': 'deed.award', 'player': '$each', 'detail': 'Hauled the most.'}}]},
                ]},
                {'do': 'set', 'var': 'best', 'value': {'max': [{'var': 'best'}, {'var': 'hauled'}]}},
                {'do': 'list remove', 'var': 'signed', 'value': {'field': 'actor'}},
            ]},
            {'id': 'close', 'on': 'departure', 'audience': 'participant',
             'if': {'test': 'all', 'conditions': [
                 {'test': 'switch', 'var': 'gate-open', 'is': True},
                 {'test': 'days since', 'from': 'var', 'var': 'since-gate', 'op': '>=', 'days': 2},
                 {'test': 'phase is', 'phase': 'gate'}, {'test': 'visit is', 'op': '=', 'value': 2},
                 {'test': 'fired', 'trigger': 'arrive', 'op': '>=', 'value': 2}]},
             'actions': [
                 {'do': 'toggle', 'var': 'gate-open'},
                 {'do': 'list clear', 'var': 'signed'},
                 {'do': 'finish', 'who': '$actor', 'outcome': 'success', 'text': 'Saw the gate shut behind the last rope.'},
                 {'do': 'finish', 'outcome': 'success', 'text': 'The rope ledger is closed.'},
             ]},
        ]},
    ],
    'tests': [
        {'name': 'Five ropes and two days', 'cohort': W, 'parties': {'ash': 'ash', 'birch': 'ash'},
         'events': [
             ev(1, 'gift', 1, actor='ash', item='rope', quantity=2),
             ev(2, 'conversation', 1.2, actor='birch', target='smith1', detail='smith'),
             ev(3, 'gift', 1.5, actor='birch', item='rope', quantity=1),
             ev(4, 'gift', 2, actor='ash', item='rope', quantity=2),
             ev(5, 'arrival', 2.5, actor='ash'),
             ev(6, 'arrival', 3, actor='cedar'),
             ev(7, 'departure', 3.5, actor='ash'),
             ev(8, 'departure', 4.5, actor='birch'),
             ev(9, 'gift', 5, actor='ash', item='rope', quantity=9),
         ],
         'expect': ['gather', 'gate'],
         'expectOutcome': 'success',
         'expectVariables': {'story': {'ropes': 5, 'gate-open': False, 'finder': 'ash', 'signed': [], 'cheers': 20,
                                       'motto': 'the smith knows'},
                             'player': {'ash': {'hauled': 6}, 'birch': {'hauled': 3}, 'cedar': {'hauled': 2}},
                             'party': {'ash': {'clues': ['smith']}}, 'world': {'best': 5}}},
    ],
}

FLOW = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'v2-flow', 'title': 'The bell and the wait',
    'audience': {'kind': 'players', 'players': W}, 'start': 'watch',
    'variables': [
        {'id': 'rung', 'type': 'number', 'scope': 'story'},
        {'id': 'luck', 'type': 'text', 'scope': 'story'},
        {'id': 'names', 'type': 'texts', 'scope': 'story', 'initial': ['alder', 'briar', 'cress']},
        {'id': 'chosen', 'type': 'text', 'scope': 'story'},
        {'id': 'lucky', 'type': 'number', 'scope': 'player'},
    ],
    'triggers': [
        {'id': 'campaign-bell', 'on': 'story signal', 'signal': 'bell', 'actions': [
            {'do': 'add', 'var': 'rung', 'value': 1},
            {'do': 'wait', 'days': 0.5},
            {'do': 'add', 'var': 'rung', 'value': 10},
            {'do': 'note', 'text': 'The bell rang out.'},
        ]},
    ],
    'phases': [
        {'id': 'watch', 'journal': 'Keep the watch.', 'effects': [], 'exits': [], 'triggers': [
            {'id': 'call', 'on': 'conversation', 'audience': 'participant', 'actions': [
                {'do': 'emit', 'signal': 'bell'},
                {'do': 'random', 'choices': [
                    {'weight': 1, 'actions': [{'do': 'set', 'var': 'luck', 'value': 'low'}]},
                    {'weight': 3, 'actions': [{'do': 'set', 'var': 'luck', 'value': 'high'}]}]},
                {'do': 'pick', 'list': 'names', 'var': 'chosen'},
                {'do': 'if', 'if': {'test': 'chance', 'percent': 50},
                 'then': [{'do': 'add', 'var': 'lucky', 'value': 1}]},
                {'do': 'if', 'if': {'test': 'compare', 'left': {'var': 'luck'}, 'op': '=', 'right': 'high'},
                 'then': [{'do': 'wait', 'days': 1}, {'do': 'schedule', 'signal': 'late', 'days': 1},
                          {'do': 'shift', 'signal': 'late', 'days': 0.5}, {'do': 'stop'}, {'do': 'note', 'text': 'never'}]},
                {'do': 'note', 'text': 'Unlucky, no wait.'},
            ]},
            {'id': 'late-bell', 'on': 'story signal', 'signal': 'late',
             'if': {'test': 'compare', 'left': {'var': 'rung'}, 'op': '>=', 'right': 11}, 'actions': [{'do': 'enter', 'phase': 'dawn'}]},
            {'id': 'check-late', 'on': 'arrival', 'if': {'test': 'all', 'conditions': [
                {'test': 'timer', 'signal': 'late'}, {'test': 'timer', 'signal': 'late', 'op': '<', 'days': 2}]}, 'actions': [
                {'do': 'note', 'text': 'The late bell is set.'}, {'do': 'disable', 'trigger': 'check-late'}]},
        ]},
        {'id': 'dawn', 'journal': 'Dawn.', 'effects': [], 'exits': [], 'triggers': [
            {'id': 'end', 'on': 'arrival', 'if': {'test': 'days since', 'from': 'phase', 'op': '>=', 'days': 1},
             'actions': [{'do': 'enable', 'trigger': 'check-late', 'phase': 'watch'}, {'do': 'enter', 'phase': 'watch'}]},
        ]},
    ],
    'tests': [
        {'name': 'Pinned rolls', 'cohort': W, 'rolls': [80, 10, 20],
         'events': [ev(1, 'conversation', 1, actor='ash'), ev(2, 'arrival', 1.2, actor='birch'),
                    ev(3, 'arrival', 2.1, actor='birch'), ev(4, 'arrival', 3, actor='cedar'),
                    ev(5, 'arrival', 4.6, actor='cedar'), ev(6, 'arrival', 5.7, actor='cedar')],
         'expect': ['watch', 'dawn', 'watch'],
         'expectVariables': {'story': {'rung': 11, 'luck': 'high', 'chosen': 'alder'}, 'player': {'ash': {'lucky': 1}}}},
        {'name': 'Hashed rolls', 'cohort': W, 'seed': 'night',
         'events': [ev(1, 'conversation', 1, actor='ash'), ev(2, 'conversation', 1.1, actor='birch'),
                    ev(3, 'conversation', 1.2, actor='cedar'), ev(4, 'arrival', 3, actor='cedar'),
                    ev(5, 'arrival', 5, actor='ash')],
         'expect': ['watch', 'dawn']},
    ],
}


# ----- Doc 86, Phase 1a: readiness and participation (C2 `audience`, `times`, `div`; R1 `percent`, `among`, `participation`;
# R2 a phase's `ready` block, its exit, `auto`, `then`, `slowAfterDays`, `autoAfterDays`, Advance). Both twins run its
# triggers (tools/test_story_native.py); the readiness is the story's own (story_program.readiness).
MUSTER = ['ash', 'birch', 'cedar', 'dell', 'elder', 'fern']
HAULED_TWICE = {'test': 'compare', 'left': {'var': 'hauls', 'of': '$each'}, 'op': '>=', 'right': 2}

READY = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'v2-muster', 'title': 'The muster at the ford',
    'audience': {'kind': 'players', 'players': MUSTER}, 'start': 'muster', 'activeDays': 3,
    'variables': [
        {'id': 'did', 'type': 'wolves', 'scope': 'story', 'note': 'Who brought rope to the muster (once each).'},
        {'id': 'hauls', 'type': 'number', 'scope': 'player'},
        {'id': 'rope', 'type': 'number', 'scope': 'story', 'show': 'Rope at the ford'},
        {'id': 'goal', 'type': 'number', 'scope': 'story'},
        {'id': 'half', 'type': 'number', 'scope': 'story'},
        {'id': 'share', 'type': 'number', 'scope': 'story', 'show': 'Share who hauled (%)'},
        {'id': 'twice', 'type': 'number', 'scope': 'story'},
        {'id': 'online', 'type': 'number', 'scope': 'story'},
        {'id': 'cheered', 'type': 'switch', 'scope': 'story'},
    ],
    'phases': [
        {'id': 'muster', 'journal': 'Bring rope to the ford: the crossing goes when enough of you have.', 'effects': [],
         'missions': [{'id': 'scout', 'title': 'Scout the ford', 'mode': 'individual', 'percent': 1,
                       'objectives': [{'label': 'Walk the ford.', 'on': 'enter place', 'match': {'target': 'ford'}}]}],
         'ready': {'all': [
             {'participation': {'list': 'did'}, 'of': 'active', 'percent': 35, 'floor': 3, 'cap': 150,
              'label': 'Wolves who brought rope'},
             {'minDays': 2, 'since': 'chapter', 'label': 'Days of the muster'},
             {'any': [{'missions': ['scout'], 'label': 'The ford scouted'},
                      {'test': {'test': 'switch', 'var': 'cheered', 'is': True}, 'label': 'or the muster cheered'}],
              'label': 'Scouted, or cheered'}],
             'auto': False, 'slowAfterDays': 4, 'autoAfterDays': 6},
         'exits': [{'on': 'ready', 'to': 'ford'}],
         'triggers': [
             {'id': 'rope', 'on': 'gift', 'audience': 'participant', 'match': {'item': 'rope'}, 'actions': [
                 {'do': 'add', 'var': 'hauls', 'value': 1},
                 {'do': 'add', 'var': 'rope', 'value': {'field': 'quantity'}},
                 {'do': 'list add', 'var': 'did', 'value': {'field': 'actor'}},
                 {'do': 'set', 'var': 'goal', 'value': {'max': [3, {'times': [{'audience': 'active'}, 2]}]}},
                 {'do': 'set', 'var': 'half', 'value': {'div': [{'audience': 'all'}, 2]}},
                 {'do': 'set', 'var': 'share', 'value': {'percent': {'list': 'did'}, 'of': 'active'}},
                 {'do': 'set', 'var': 'twice', 'value': {'among': {'where': HAULED_TWICE}, 'of': 'all'}},
             ]},
             {'id': 'cheer', 'on': 'conversation', 'audience': 'participant', 'once': 'visit',
              'if': {'test': 'participation', 'source': 'where', 'where': HAULED_TWICE, 'of': 'all', 'percent': 15, 'floor': 1},
              'actions': [{'do': 'turn on', 'var': 'cheered'}, {'do': 'note', 'text': 'The muster cheers its haulers.'}]},
             {'id': 'count', 'on': 'arrival', 'audience': 'participant', 'actions': [
                 {'do': 'set', 'var': 'online', 'value': {'audience': 'online'}}]},
         ]},
        {'id': 'ford', 'journal': 'The rope is strung: cross at first light.', 'effects': [],
         'ready': {'all': [{'value': {'var': 'rope'}, 'atLeast': {'var': 'goal'}, 'label': 'Rope for every active wolf'}],
                   'auto': True, 'then': {'on': 'day begins'}},
         'exits': [{'on': 'ready', 'to': 'crossed'}]},
        {'id': 'crossed', 'journal': 'The muster crossed the ford.', 'effects': [], 'exits': []},
    ],
    'tests': [
        {'name': 'Ready by who took part and by time; Advance; first light', 'cohort': MUSTER, 'events': [
            *(ev(i, 'arrival', 0.5, actor=w) for i, w in enumerate(MUSTER)),
            ev(10, 'gift', 1, actor='ash', item='rope', quantity=4),
            ev(11, 'gift', 1.1, actor='birch', item='rope', quantity=4),
            ev(12, 'gift', 1.2, actor='ash', item='rope', quantity=2),
            {'check': 'ready', 'met': False},
            ev(13, 'gift', 1.5, actor='cedar', item='rope', quantity=3),
            ev(14, 'conversation', 1.6, actor='ash', target='rm111'),
            {'check': 'ready', 'met': False},
            ev(15, 'clock', 2.1),
            {'check': 'ready', 'met': True},
            {'advance': 'now'},
            ev(16, 'clock', 2.5),
            ev(17, 'day begins', 3, detail='Restday'),
        ], 'expect': ['muster', 'ford', 'crossed'],
         'expectVariables': {'story': {'rope': 13, 'goal': 12, 'half': 3, 'share': 50, 'twice': 1, 'online': 6, 'cheered': True},
                             'player': {'ash': {'hauls': 2}}}},
        {'name': 'A wolf who went quiet leaves both counts; Advance anyway', 'cohort': MUSTER, 'events': [
            ev(1, 'arrival', 0, actor='ash'),
            ev(2, 'gift', 0.5, actor='ash', item='rope', quantity=20),
            ev(3, 'departure', 0.6, actor='ash'),
            ev(4, 'arrival', 3.8, actor='dell'),
            ev(5, 'enter place', 3.9, actor='dell', target='ford'),
            {'check': 'ready', 'met': False},
            ev(6, 'clock', 4.5),
            {'advance': 'anyway'},
            ev(7, 'day begins', 5, detail='Dawnday'),
        ], 'expect': ['muster', 'ford', 'crossed'], 'expectVariables': {'story': {'share': 100, 'online': 1}}},
        {'name': 'Ready with no admin about: it goes by itself', 'cohort': MUSTER, 'activeWolves': ['ash', 'birch', 'cedar'],
         'audience': {'active': 3}, 'events': [
            ev(1, 'gift', 0.2, actor='ash', item='rope', quantity=3),
            ev(2, 'gift', 0.3, actor='birch', item='rope', quantity=3),
            ev(3, 'gift', 0.4, actor='cedar', item='rope', quantity=3),
            ev(4, 'enter place', 0.5, actor='cedar', target='ford'),
            ev(5, 'clock', 2.5),
            {'check': 'ready', 'met': True},
            ev(6, 'clock', 8.6),
            ev(7, 'day begins', 9, detail='Hearthday'),
        ], 'expect': ['muster', 'ford', 'crossed'], 'expectVariables': {'story': {'goal': 6, 'share': 100}}},
    ],
}


def all_samples():
    return [copy.deepcopy(LEDGER), copy.deepcopy(FLOW), copy.deepcopy(READY), copy.deepcopy(TALK), copy.deepcopy(VALUES),
            copy.deepcopy(THANKS), copy.deepcopy(LINE), copy.deepcopy(HOWLS), copy.deepcopy(LETTERS),
            copy.deepcopy(FOES), copy.deepcopy(CROWN)]


# ----- Doc 85, Phase 6: quests (tools/test_story_quests.py; both twins' parity, tools/test_story_native.py)
ASH, BIRCH, ELM = 'player-ash', 'player-birch', 'player-elm'


def quest_fact(i, kind, day, actor='', **fields):
    return {'id': f'e{i}', 'kind': kind, 'day': day, 'actor': actor, **fields}


QUEST_SMALL = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'carter-axle', 'title': 'An axle for the carter',
    'audience': {'kind': 'open', 'cap': 50}, 'start': 'open',
    'phases': [{'id': 'open', 'journal': 'The carter at the ford needs an axle.', 'effects': [], 'exits': []}],
    'quests': [{
        'id': 'axle', 'title': 'An axle for the carter', 'summary': 'The carter at the ford is stuck.', 'giver': 'rm111',
        'offer': {'say': 'My axle split at the ford. Could you find me another?', 'accept': 'I will find you one.'},
        'repeat': 'daily', 'join': {'cap': 2},
        'objectives': [
            {'id': 'ask', 'label': 'Ask the carter which part gave way.', 'on': 'conversation', 'match': {'target': 'rm111'}},
            {'id': 'bring', 'label': 'Bring two axles.', 'on': 'gift', 'match': {'target': 'rm111', 'item': 'axle'},
             'after': ['ask'], 'goal': 2, 'measure': 'quantity'},
            {'id': 'wheel', 'label': 'A spare wheel would not go amiss.', 'optional': True, 'on': 'gift',
             'match': {'target': 'rm111', 'item': 'cart_wheel'}},
            {'id': 'thief', 'label': 'Find who cut the axle.', 'optional': True, 'hidden': True, 'on': 'conversation', 'match': {'target': 'rm119'}},
        ],
        'rewards': [{'kind': 'deed.award', 'player': '$actor', 'detail': 'Got the carter moving.'},
                    {'kind': 'character.grant', 'player': '$actor', 'coins': 12, 'purse': 'town:greyfen', 'reason': 'the carter thanks you'}],
        'bonus': [{'kind': 'item.give', 'player': '$actor', 'item': 'bread', 'quantity': 2}],
    }],
    'tests': [{'name': 'Ash takes it, does it and hands it in', 'cohort': [], 'events': [
        quest_fact(1, 'quest accepted', 1, ASH, target='run', item='rm111', detail='axle'),
        quest_fact(2, 'gift', 1.1, ASH, target='rm111', item='axle', quantity=2),
        quest_fact(3, 'conversation', 1.2, ASH, target='rm111'),
        quest_fact(4, 'gift', 1.3, ASH, target='rm111', item='axle', quantity=2),
        quest_fact(5, 'quest handed in', 1.4, ASH, target='run', item='rm111', detail='axle'),
    ], 'expect': ['open'], 'expectQuests': {'axle': {ASH: 'done'}},
        'expectEffects': [
            {'phase': 'open', 'kind': 'deed.award', 'player': ASH, 'detail': 'Got the carter moving.', 'reward': 'axle'},
            {'phase': 'open', 'kind': 'character.grant', 'player': ASH, 'coins': 12, 'purse': 'town:greyfen',
             'reason': 'the carter thanks you', 'reward': 'axle'}]}],
}

QUEST_CAMPAIGN = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'ford-campaign', 'title': 'The ford in winter',
    'audience': {'kind': 'everyone', 'days': 30}, 'start': 'muster',
    'variables': [{'id': 'ropes', 'type': 'number', 'scope': 'story', 'show': 'Ropes laid'}],
    'regions': [{'id': 'ford', 'cell': 'grey', 'x1': 2, 'y1': 2, 'x2': 8, 'y2': 8}],
    'phases': [
        {'id': 'muster', 'journal': 'The ford must be roped before the freeze.', 'effects': [], 'exits': [
            {'on': 'story signal', 'match': {'detail': ''}, 'to': 'freeze'}] if False else [], 'triggers': [
            {'id': 'greet', 'on': 'arrival', 'actions': [
                {'do': 'effect', 'effect': {'kind': 'quest.offer', 'quest': 'scout', 'player': '$actor'}}]},
            {'id': 'roped', 'on': 'quest completed', 'audience': 'participant', 'match': {'detail': 'ropes'}, 'actions': [
                {'do': 'add', 'var': 'ropes', 'value': 1}, {'do': 'enter', 'phase': 'freeze'}]},
            {'id': 'ask-hale', 'on': 'conversation', 'audience': 'participant', 'match': {'target': 'rm119'}, 'actions': [
                {'do': 'effect', 'effect': {'kind': 'npc.offer', 'npc': 'rm119', 'id': 'hale-news', 'words': 'Any news of the ice?',
                                            'reply': 'Thin at the edges.', 'once': True}}]},
        ]},
        {'id': 'freeze', 'journal': 'The ford is roped.', 'effects': [], 'exits': [], 'triggers': [
            {'id': 'late', 'on': 'conversation', 'audience': 'participant', 'actions': [{'do': 'note', 'text': 'Talked in the freeze.'}]}]},
    ],
    'quests': [
        {'id': 'scout', 'title': 'Scout the ice', 'scope': 'player', 'lifetime': 'phase', 'phases': ['muster'],
         'objectives': [{'id': 'walk', 'label': 'Walk the ford.', 'on': 'region entered', 'match': {'target': 'ford'}}],
         'fail': [{'afterDays': 2}],
         'rewards': [{'kind': 'deed.award', 'player': '$actor', 'detail': 'Scouted the ice.'}]},
        {'id': 'ropes', 'title': 'Rope the ford', 'scope': 'party', 'giver': 'rm112', 'phases': ['muster'],
         'objectives': [{'id': 'rope', 'label': 'Give Wren three ropes.', 'on': 'gift', 'match': {'target': 'rm112', 'item': 'rope'},
                         'goal': 3, 'measure': 'quantity'},
                        {'id': 'tell', 'label': 'Tell Ivo the ford is roped.', 'on': 'conversation', 'match': {'target': 'rm111'}}],
         'handIn': False,
         'rewards': [{'kind': 'standing.adjust', 'player': '$actor', 'faction': 'greyfen', 'points': 2, 'reason': 'roped the ford'}]},
        {'id': 'watch', 'title': 'Keep the ice watch', 'scope': 'story', 'contributors': 2,
         'objectives': [{'id': 'watch', 'label': 'Two wolves keep watch at the ford.', 'on': 'region entered', 'match': {'target': 'ford'}}],
         'rewards': [{'kind': 'deed.award', 'player': '$actor', 'detail': 'Kept the ice watch.'}]},
    ],
    'tests': [{'name': 'A party ropes the ford', 'cohort': [ASH, BIRCH], 'parties': {ASH: 'p1', BIRCH: 'p1'}, 'events': [
        quest_fact(1, 'arrival', 1, ELM),
        quest_fact(2, 'quest accepted', 1, ELM, target='r', detail='scout'),
        quest_fact(3, 'quest accepted', 1.1, ASH, target='r', item='rm112', detail='ropes'),
        quest_fact(4, 'gift', 1.2, BIRCH, target='rm112', item='rope', quantity=2),
        quest_fact(5, 'conversation', 1.3, ASH, target='rm111'),
        quest_fact(6, 'gift', 1.4, ASH, target='rm112', item='rope', quantity=1),
    ], 'expect': ['muster', 'freeze'], 'expectQuests': {'ropes': {'party:p1': 'done'}, 'scout': {ELM: 'expired'}},
        'expectVariables': {'story': {'ropes': 1}}}],
}


# ----- Doc 85, Phase 8: the world's facts and the new events (tools/test_story_facts.py; both twins, tools/test_story_native.py)
def _probe(conditions, var, first=0):
    """Ifs that add 2^k to a number for each condition k that holds (so a rehearsal's final value says which held)."""
    return [{'do': 'if', 'if': c, 'then': [{'do': 'add', 'var': var, 'value': 2 ** (first + k)}]} for k, c in enumerate(conditions)]


FACTS_WOLF = [
    {'test': 'wolf here'}, {'test': 'wolf in region', 'region': 'gate'}, {'test': 'wolf in place', 'place': 'grey'},
    {'test': 'wolf in town', 'town': 'greyfen'}, {'test': 'wolf indoors', 'is': False},
    {'test': 'carries', 'item': 'rope', 'op': '>=', 'value': 2}, {'test': 'coins', 'op': '>', 'value': 10},
    {'test': 'standing', 'faction': 'greyfen_watch', 'op': '>=', 'value': 20},
    {'test': 'reputation', 'who': '$actor', 'town': 'greyfen', 'op': '>=', 'value': 1},
    {'test': 'wolf is', 'state': 'wanted', 'is': False}, {'test': 'party size', 'op': '>=', 'value': 2},
    {'test': 'in party with', 'other': BIRCH}, {'test': 'wolf here', 'who': ELM},
]
FACTS_WORLD = [
    {'test': 'count in region', 'region': 'gate', 'op': '>=', 'value': 2}, {'test': 'resident is', 'npc': 'rm111', 'state': 'alive'},
    {'test': 'resident in place', 'npc': 'rm111', 'place': 'grey'}, {'test': 'resident in region', 'npc': 'rm111', 'region': 'gate'},
    {'test': 'resident has', 'npc': 'rm111', 'what': 'mantle', 'value': 'carter'}, {'test': 'hour between', 'from': 22, 'to': 4},
    {'test': 'calendar is', 'what': 'weekday', 'value': 'Restday'}, {'test': 'weather is', 'weather': 'snow'},
    {'test': 'weather is', 'where': 'grey', 'weather': 'snow'},
    {'test': 'relation is', 'faction': 'greyfen_watch', 'op': '>=', 'stance': 'friendly', 'other': 'road_company'},
    {'test': 'not', 'condition': {'test': 'resident is', 'npc': 'rm119', 'state': 'dead'}},
]
FACTS_QUEST = [
    {'test': 'quest is', 'quest': 'patrol', 'status': 'active'}, {'test': 'quest is', 'quest': 'patrol', 'status': 'none'},
    {'test': 'objective is', 'quest': 'patrol', 'objective': 'walk', 'status': 'done'},
    {'test': 'objective is', 'quest': 'patrol', 'objective': 'door', 'status': 'hidden'},
    {'test': 'quest is', 'quest': 'patrol', 'of': 'story', 'status': 'none'},
]
NIGHT = {'cell': 'grey', 'x': 3, 'y': 4, 'indoors': False, 'town': 'greyfen', 'party': 'p1', 'partySize': 2, 'carries': {'rope': 3},
         'coins': 40, 'standing': {'greyfen_watch': 25}, 'reputation': {'greyfen': 2}, 'downed': False, 'hurt': False, 'wanted': False,
         'held': False}
FACTS_WATCH = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'night-watch', 'title': 'The night watch at Greyfen',
    'audience': {'kind': 'players', 'players': [ASH, BIRCH]}, 'start': 'watch',
    'variables': [{'id': 'wolf', 'type': 'number', 'scope': 'player'}, {'id': 'world', 'type': 'number', 'scope': 'story'},
                  {'id': 'quest', 'type': 'number', 'scope': 'player'}, {'id': 'looped', 'type': 'number', 'scope': 'story'},
                  {'id': 'events', 'type': 'texts', 'scope': 'story'}],
    'regions': [{'id': 'gate', 'cell': 'grey', 'x1': 2, 'y1': 2, 'x2': 6, 'y2': 6}],
    'quests': [{'id': 'patrol', 'title': 'Walk the gate', 'scope': 'player',
                'objectives': [{'id': 'walk', 'label': 'Walk to the gate.', 'on': 'region entered', 'match': {'target': 'gate'}},
                               {'id': 'door', 'label': 'Try the gaol door.', 'hidden': True, 'optional': True, 'on': 'door locked'}]}],
    'phases': [{'id': 'watch', 'journal': 'Keep the night watch.', 'effects': [], 'exits': [
        {'on': 'night falls', 'to': 'dark'}], 'triggers': [
        {'id': 'probe-wolf', 'on': 'conversation', 'audience': 'participant', 'actions': _probe(FACTS_WOLF[:7], 'wolf')},
        {'id': 'probe-wolf-2', 'on': 'conversation', 'audience': 'participant', 'actions': _probe(FACTS_WOLF[7:], 'wolf', 7)},
        {'id': 'probe-world', 'on': 'trade done', 'actions': _probe(FACTS_WORLD, 'world')},
        {'id': 'probe-quest', 'on': 'dialogue node', 'audience': 'participant', 'actions': _probe(FACTS_QUEST, 'quest')},
        {'id': 'start', 'on': 'dialogue opened', 'audience': 'participant', 'match': {'target': 'rm111'}, 'actions': [
            {'do': 'effect', 'effect': {'kind': 'quest.start', 'quest': 'patrol', 'player': '$actor'}}]},
        {'id': 'loop', 'on': 'hour', 'match': {'detail': '23'}, 'actions': [
            {'do': 'each', 'over': 'participants', 'actions': [
                {'do': 'if', 'if': {'test': 'carries', 'who': '$each', 'item': 'rope', 'op': '>=', 'value': 1},
                 'then': [{'do': 'add', 'var': 'looped', 'value': 1}]}]}]},
        {'id': 'log', 'on': 'door opened', 'actions': [{'do': 'list add', 'var': 'events', 'value': 'door opened'},
                                                     {'do': 'wait', 'days': 0.01}, {'do': 'if', 'if': {'test': 'wolf indoors', 'is': True},
                                                                                   'then': [{'do': 'list add', 'var': 'events', 'value': 'indoors after'}]}]},
        *({'id': 'new-' + k.replace(' ', '-'), 'on': k, 'actions': [{'do': 'list add', 'var': 'events', 'value': k}]} for k in (
            'door closed', 'building entered', 'building left', 'region occupied', 'region emptied', 'weather changed', 'day begins',
            'dawn', 'season begins', 'item picked up', 'item dropped', 'dialogue closed', 'crime seen', 'wolf downed', 'wolf revived',
            'party formed', 'party ended')),
    ]}, {'id': 'dark', 'journal': 'Night has fallen.', 'effects': [], 'exits': []}],
    'tests': [
        {'name': 'Facts read as of each event', 'cohort': [ASH, BIRCH],
         'facts': {'wolves': {ASH: NIGHT, BIRCH: {**NIGHT, 'x': 5, 'carries': {}}}, 'world': {'hour': 23, 'weekday': 'Restday', 'season': 'winter', 'period': 'night'},
                   'residents': {'rm111': {'alive': True, 'cell': 'grey', 'x': 4, 'y': 4, 'indoors': False, 'mantle': 'carter'},
                                 'rm119': {'alive': True, 'cell': 'mill'}},
                   'weather': {'grey': 'snow'}, 'relations': {'greyfen_watch>road_company': 'allied'}},
         'events': [
             quest_fact(1, 'conversation', 1, ASH, target='rm111'),
             quest_fact(2, 'trade done', 1.01, ASH, target='rm111', item='rope', detail='bought', quantity=1, coins=4),
             quest_fact(3, 'dialogue opened', 1.02, ASH, target='rm111', detail='resident'),
             quest_fact(4, 'dialogue node', 1.03, ASH, target='rm111', detail='greet', item='resident'),
             quest_fact(5, 'region entered', 1.04, ASH, target='gate', detail='run', cell='grey', x=3, y=4),
             {**quest_fact(6, 'dialogue node', 1.05, ASH, target='rm111', detail='bye'),
              'facts': {'wolves': {ASH: {**NIGHT, 'cell': 'inn', 'indoors': True, 'coins': 5}, 'elm': None}}},
             quest_fact(7, 'conversation', 1.06, ASH, target='rm111'),
             quest_fact(8, 'hour', 1.07, '', detail='23', quantity=23),
             quest_fact(9, 'door opened', 1.08, ASH, target='inn_door', cell='inn', detail='The inn door'),
             {**quest_fact(10, 'trade done', 1.2, BIRCH, target='rm111', item='rope', detail='sold', quantity=1, coins=3),
              'facts': {'world': {'hour': 9, 'weekday': 'Marketday'}, 'weather': {'grey': 'clear'}, 'residents': {'rm111': {'alive': False}},
                        'relations': {'greyfen_watch>road_company': 'tense'}}},
             *(quest_fact(11 + i, k, 1.3 + i / 100, ASH) for i, k in enumerate(('door closed', 'building entered', 'building left',
                                                                                 'region occupied', 'party formed', 'wolf downed', 'wolf revived'))),
             quest_fact(30, 'night falls', 1.5),
         ], 'expect': ['watch', 'dark'],
         'expectVariables': {'player': {ASH: {'wolf': 8104, 'quest': 45}}, 'story': {'world': 3071, 'looped': 1, 'events': [
             'door opened', 'indoors after', 'door closed', 'building entered', 'building left', 'region occupied', 'party formed',
             'wolf downed', 'wolf revived']}}},
        {'name': 'No facts: every world condition unknown', 'cohort': [ASH, BIRCH], 'events': [
            quest_fact(1, 'conversation', 1, ASH, target='rm111'),
            quest_fact(2, 'trade done', 1.1, ASH, target='rm111', item='rope', detail='bought', quantity=1, coins=4),
        ], 'expect': ['watch'], 'expectVariables': {'player': {ASH: {'wolf': 0}}, 'story': {'world': 0}}},
    ],
}


# ----- Doc 86, Phase 1b: a choice for each wolf who comes (J1), residents spared and protected for the campaign (C11), a
# villain named (C9), the realm's memory and one resident's of each wolf (C8), a rumour and its clearing (T7), a value the
# residents are told (N2), and a till that pays (SQ2). Approved as a chapter of a campaign (its holds last the campaign).
TALK = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'v2-pledge', 'title': 'The pledge at the stone',
    'audience': {'kind': 'players', 'players': [ASH, BIRCH, ELM]}, 'start': 'gather',
    'variables': [
        {'id': 'pledged', 'type': 'wolves', 'scope': 'story', 'note': 'Who pledged at the stone.'},
        {'id': 'p_pledge', 'type': 'text', 'scope': 'player', 'talk': True, 'note': 'Told to residents: player.story.p_pledge.'},
    ],
    'phases': [
        {'id': 'gather', 'journal': 'Ivo keeps the old stone: pledge to the town there, or do not.',
         'effects': [
             {'kind': 'npc.spare', 'npc': 'rm111', 'lasts': 'campaign'},
             {'kind': 'npc.protect', 'npc': 'rm112', 'lasts': 'campaign'},
             {'kind': 'figure.set', 'figure': 'nh_villain', 'role': 'villain', 'name': 'The Ledger Keeper', 'title': 'the keeper of the black ledger',
              'known': 'realm', 'about': 'Nobody in Greyfen has seen the keeper; everyone has paid.'},
             {'kind': 'talk.remember', 'scope': 'realm', 'key': 'nh_stage', 'value': 'waking', 'days': 30},
         ],
         'exits': [],
         'triggers': [
             {'id': 'came', 'on': 'arrival', 'audience': 'participant', 'actions': [
                 {'do': 'effect', 'effect': {'kind': 'npc.offer', 'npc': 'rm111', 'id': 'pledge', 'words': 'I pledge to Greyfen.',
                                             'reply': 'Then the stone knows you.', 'player': '$actor', 'once': True}}]},
             {'id': 'pledge', 'on': 'dialogue choice', 'audience': 'participant', 'match': {'detail': 'pledge'}, 'once': 'member', 'actions': [
                 {'do': 'set', 'var': 'p_pledge', 'value': 'greyfen'},
                 {'do': 'list add', 'var': 'pledged', 'value': {'field': 'actor'}},
                 {'do': 'effect', 'effect': {'kind': 'talk.remember', 'scope': 'pair', 'npc': 'rm111', 'player': '$actor', 'key': 'nh_pledge',
                                             'value': 'pledged', 'days': 120, 'keep': True}},
                 {'do': 'effect', 'effect': {'kind': 'character.grant', 'player': '$actor', 'coins': 3, 'purse': 'till:job:rm111',
                                             'reason': 'the stone-keeper\'s penny'}},
                 {'do': 'if', 'if': {'test': 'compare', 'left': {'among': {'list': 'pledged'}, 'of': 'all'}, 'op': '>=', 'right': 2},
                  'then': [{'do': 'enter', 'phase': 'rumour'}]},
             ]},
         ]},
        {'id': 'rumour', 'journal': 'Two have pledged; the town starts to talk.',
         'effects': [
             {'kind': 'belief.plant', 'subject': 'rm112', 'claim': 'was seen at the Customs House at midnight', 'town': 'greyfen',
              'count': 6, 'confidence': 60},
             {'kind': 'belief.plant', 'subject': 'rm113', 'claim': 'took the Customs House silver', 'town': 'greyfen', 'count': 4,
              'confidence': 70, 'claimKind': 'story.cleared'},
             {'kind': 'talk.remember', 'scope': 'town', 'town': 'greyfen', 'key': 'nh_stage', 'value': 'reckoning', 'days': 30},
             {'kind': 'talk.forget', 'scope': 'realm', 'key': 'nh_stage'},
         ],
         'exits': []},
    ],
    'tests': [
        {'name': 'Two wolves pledge; the town talks', 'cohort': [ASH, BIRCH, ELM], 'events': [
            quest_fact(1, 'arrival', 1, ASH),
            quest_fact(2, 'arrival', 1.1, BIRCH),
            quest_fact(3, 'dialogue choice', 1.2, ASH, target='r', item='rm111', detail='pledge'),
            quest_fact(4, 'dialogue choice', 1.3, ASH, target='r', item='rm111', detail='pledge'),
            quest_fact(5, 'dialogue choice', 1.4, BIRCH, target='r', item='rm111', detail='pledge'),
        ], 'expect': ['gather', 'rumour'],
         'expectVariables': {'player': {ASH: {'p_pledge': 'greyfen'}, BIRCH: {'p_pledge': 'greyfen'}}}},
        {'name': 'One wolf alone: the town stays quiet', 'cohort': [ASH, BIRCH, ELM], 'events': [
            quest_fact(1, 'arrival', 1, ELM),
            quest_fact(2, 'dialogue choice', 1.2, ELM, target='r', item='rm111', detail='pledge'),
        ], 'expect': ['gather'], 'expectVariables': {'player': {ELM: {'p_pledge': 'greyfen'}}}},
    ],
}


# ----- Doc 86, Phase 2: values and objectives (tools/test_story_v2.py, test_story_quests.py; both twins, test_story_native.py)
# A story-scope bar whose goal scales with the active wolves, a notice with fills, a deed matched by how it begins, a
# Readiness that waits for the next new moon, a named camp cleared as a plain objective, a wagon objective that credits the
# wolves present, a reward that follows what the purse holds, a tracker of another number, and a mirror of a world number.
VALUES = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'v2-beacons', 'title': 'The beacon road', 'prefix': 'sq',
    'audience': {'kind': 'players', 'players': [ASH, BIRCH, ELM]}, 'start': 'road',
    'regions': [{'id': 'hollow', 'cell': 'grey', 'x1': 2, 'y1': 2, 'x2': 8, 'y2': 8}],
    'variables': [
        {'id': 'bar', 'type': 'number', 'scope': 'story', 'show': 'Beacon oil', 'showOf': 'bar_goal'},
        {'id': 'bar_goal', 'type': 'number', 'scope': 'story', 'note': 'Twelve, or four for each active wolf.'},
        {'id': 'finder', 'type': 'wolf', 'scope': 'story'},
        {'id': 'voice_greyfen', 'type': 'number', 'scope': 'world'},
        {'id': 'voice_coldwater', 'type': 'number', 'scope': 'world'},
        {'id': 'p_town', 'type': 'text', 'scope': 'player'},
        {'id': 'p_voice', 'type': 'number', 'scope': 'player', 'show': 'Your town\'s howl',
         'mirror': {'key': 'p_town', 'names': {'greyfen': 'voice_greyfen', 'coldwater': 'voice_coldwater'}}},
    ],
    'quests': [
        {'id': 'bar', 'title': 'Oil for the beacons', 'summary': 'The beacons want {bar_goal} measures of oil.', 'scope': 'story',
         'objectives': [{'id': 'oil', 'label': 'Bring lamp oil to the beacons.', 'on': 'gift', 'match': {'item': 'lamp_oil'},
                         'measure': 'quantity', 'goal': {'max': [12, {'times': [{'audience': 'active'}, 4]}]}, 'goalLive': True}]},
        {'id': 'camp', 'title': 'The camp in the hollow', 'summary': 'Break the camp the story set in the hollow.',
         'objectives': [{'id': 'break', 'label': 'Break the camp in the hollow.', 'on': 'camp cleared',
                         'match': {'target': 'camp_story_sq_beacon_camp'}}]},
        {'id': 'wagon', 'title': 'Meet the oil wagon', 'summary': 'Be at Greyfen when the oil wagon comes in, {p_town.of}.',
         'join': {'again': 7},
         'objectives': [{'id': 'meet', 'label': 'Be at the market when the wagon arrives.', 'on': 'caravan arrives',
                         'credit': 'present', 'where': {'cells': ['grey']}}],
         'rewards': [{'kind': 'character.grant', 'player': '$actor', 'coins': {'min': [12, {'div': [{'purse': 'town:greyfen'}, 20]}]},
                      'purse': 'town:greyfen', 'reason': 'the carters\' thanks'}]},
    ],
    'phases': [
        {'id': 'road', 'journal': 'Light the beacons on the road, before the new moon.',
         'effects': [{'kind': 'story.area.spawn', 'region': 'hollow', 'count': 2, 'camp': 'sq_beacon_camp'}],
         'ready': {'all': [{'quest': 'bar', 'label': 'The beacons have their oil'}], 'auto': True,
                   'then': {'on': 'moon', 'match': {'detail': 'new'}}},
         'exits': [{'on': 'ready', 'to': 'lit'}],
         'triggers': [
             {'id': 'come', 'on': 'arrival', 'audience': 'participant', 'once': 'member', 'actions': [
                 {'do': 'effect', 'effect': {'kind': 'quest.start', 'quest': 'bar', 'player': '$actor'}},
                 {'do': 'effect', 'effect': {'kind': 'quest.start', 'quest': 'camp', 'player': '$actor'}},
                 {'do': 'effect', 'effect': {'kind': 'quest.start', 'quest': 'wagon', 'player': '$actor'}}]},
             {'id': 'oil', 'on': 'gift', 'audience': 'participant', 'match': {'item': 'lamp_oil'}, 'actions': [
                 {'do': 'add', 'var': 'bar', 'value': {'field': 'quantity'}},
                 {'do': 'set', 'var': 'bar_goal', 'value': {'max': [12, {'times': [{'audience': 'active'}, 4]}]}}]},
             {'id': 'pledge', 'on': 'conversation', 'audience': 'participant', 'match': {'target': {'in': ['rm111', 'rm112']}}, 'actions': [
                 {'do': 'set', 'var': 'p_town', 'value': 'greyfen'},
                 {'do': 'add', 'var': 'voice_greyfen', 'value': 1},
                 {'do': 'effect', 'effect': {'kind': 'character.grant', 'player': '$actor', 'purse': 'town:greyfen', 'reason': 'a pledge',
                                             'coins': {'max': [1, {'times': [{'var': 'p_voice', 'of': '$actor'}, 2]}]}}}]},
             {'id': 'watch', 'on': 'deed', 'audience': 'participant', 'match': {'detail': {'starts': 'Stood watch at the ford'}},
              'if': {'test': 'field', 'field': 'cell', 'op': 'in', 'value': ['grey', 'far']}, 'actions': [
                 {'do': 'remember', 'var': 'finder', 'from': 'actor'},
                 {'do': 'effect', 'effect': {'kind': 'board.post', 'town': 'greyfen', 'days': 3,
                                             'text': '{finder} stood watch at the ford; the beacons have {bar} of {bar_goal} measures.'}},
                 {'do': 'effect', 'effect': {'kind': 'journal.note', 'player': {'field': 'actor'}, 'text': 'You stood watch at the ford ({bar} of {bar_goal}).'}}]},
         ]},
        {'id': 'lit', 'journal': 'The beacons burn along the road.', 'effects': [], 'exits': []},
    ],
    'tests': [
        {'name': 'The beacons lit at the new moon', 'cohort': [ASH, BIRCH, ELM],
         'facts': {'purses': {'town:greyfen': 212},
                   'wolves': {ASH: {'cell': 'grey'}, BIRCH: {'cell': 'grey'}, ELM: {'cell': 'far'}}},
         'events': [
             quest_fact(1, 'arrival', 0.5, ASH),
             quest_fact(2, 'arrival', 0.6, BIRCH),
             quest_fact(3, 'arrival', 0.7, ELM),
             quest_fact(4, 'gift', 1, ASH, target='rm111', item='lamp_oil', quantity=5),
             quest_fact(5, 'conversation', 1.1, ASH, target='rm111'),
             quest_fact(6, 'deed', 1.2, ASH, cell='grey', detail='Stood watch at the ford (d12)'),
             quest_fact(7, 'camp cleared', 1.3, ASH, target='camp_story_sq_beacon_camp', cell='grey', detail='by Ash'),
             quest_fact(8, 'caravan arrives', 1.4, 'cv1', target='greyfen', cell='grey', detail='from coldwater'),
             quest_fact(9, 'gift', 1.5, BIRCH, target='rm111', item='lamp_oil', quantity=7),
             {'check': 'ready', 'met': True},
             quest_fact(10, 'moon', 4, detail='new', quantity=0),
         ], 'expect': ['road', 'lit'],
         'expectQuests': {'bar': {'story': 'done'}, 'camp': {ASH: 'done', BIRCH: 'active', ELM: 'active'},
                          'wagon': {ASH: 'done', BIRCH: 'done', ELM: 'active'}},
         'expectVariables': {'story': {'bar': 12, 'bar_goal': 12, 'finder': ASH}, 'world': {'voice_greyfen': 1},
                             'player': {ASH: {'p_town': 'greyfen'}}},
         'expectEffects': [
             {'phase': 'road', 'kind': 'story.area.spawn', 'region': 'hollow', 'count': 2, 'camp': 'sq_beacon_camp'},
             {'phase': 'road', 'kind': 'character.grant', 'player': ASH, 'purse': 'town:greyfen', 'reason': 'a pledge', 'coins': 2},
             {'phase': 'road', 'kind': 'board.post', 'town': 'greyfen', 'days': 3,
              'text': '{@w:' + ASH + '} stood watch at the ford; the beacons have 5 of 12 measures.'},
             {'phase': 'road', 'kind': 'journal.note', 'player': ASH, 'text': 'You stood watch at the ford (5 of 12).'},
             {'phase': 'road', 'kind': 'character.grant', 'player': ASH, 'coins': 10, 'purse': 'town:greyfen', 'reason': 'the carters\' thanks',
              'reward': 'wagon'},
             {'phase': 'road', 'kind': 'character.grant', 'player': BIRCH, 'coins': 10, 'purse': 'town:greyfen', 'reason': 'the carters\' thanks',
              'reward': 'wagon'}]},
    ],
}


# ----- Doc 86, Phase 3: thanks, titles, ranks and words (tools/test_story_v2.py, test_story_quests.py; both twins,
# test_story_native.py). A story-scope drive for lamp oil that rewards each wolf by their share (and leaves out one who
# brought too little), thanks them in the giver's own voice (npc.greet) and names them (title.give); a tally of what every
# wolf brought; the two who brought most ranked and named for the realm; a title taken back; a rumour put to rest; and the
# phase's line for its wolves with a #symbol# said in the speaker's voice.
THANKS = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'v2-thanks', 'title': 'The oil drive',
    'audience': {'kind': 'players', 'players': [ASH, BIRCH, ELM]}, 'start': 'drive',
    'variables': [
        {'id': 'p_oil', 'type': 'number', 'scope': 'player', 'show': 'Oil you brought'},
        {'id': 'p_backs', 'type': 'text', 'scope': 'player', 'note': 'Whose lamp a wolf brings oil for.'},
        {'id': 'brought', 'type': 'number', 'scope': 'story', 'show': 'Oil brought in all'},
        {'id': 'for_lamp', 'type': 'number', 'scope': 'story', 'note': 'Oil brought by the wolves who back the lamp.'},
        {'id': 'best', 'type': 'wolves', 'scope': 'story', 'note': 'The two who brought most.'},
    ],
    'quests': [
        {'id': 'oil', 'title': 'Oil for the lamps', 'summary': 'The lamps of Greyfen want nine measures of oil.', 'scope': 'story',
         'credit': 'share', 'creditMin': 2,
         'objectives': [{'id': 'bring', 'label': 'Bring lamp oil to the lamplighter.', 'on': 'gift', 'match': {'target': 'rm111', 'item': 'lamp_oil'},
                         'measure': 'quantity', 'goal': 9}],
         'rewards': [
             {'kind': 'character.grant', 'player': '$actor', 'purse': 'town:greyfen', 'reason': 'the lamplighter\'s thanks',
              'coins': {'min': [20, {'times': [{'share': True}, 2]}]}},
             {'kind': 'npc.greet', 'npc': 'rm111', 'player': '$actor', 'text': '#greet# The lamps burn on your oil. I\'ll not forget it.', 'days': 14},
             {'kind': 'title.give', 'player': '$actor', 'title': 'Lamp-bearer of Greyfen', 'known': 'town', 'town': 'greyfen', 'days': 30}]},
    ],
    'phases': [
        {'id': 'drive', 'journal': 'The lamplighter asks for oil before the long nights.',
         'effects': [{'kind': 'npc.line', 'npc': 'rm111', 'text': '#greet# The lamps want oil, and plenty of it.'}],
         'exits': [],
         'triggers': [
             {'id': 'come', 'on': 'arrival', 'audience': 'participant', 'once': 'member', 'actions': [
                 {'do': 'set', 'var': 'p_backs', 'value': 'lamp'},
                 {'do': 'effect', 'effect': {'kind': 'quest.start', 'quest': 'oil', 'player': '$actor'}}]},
             {'id': 'oil', 'on': 'gift', 'audience': 'participant', 'match': {'target': 'rm111', 'item': 'lamp_oil'}, 'actions': [
                 {'do': 'add', 'var': 'p_oil', 'value': {'field': 'quantity'}},
                 {'do': 'set', 'var': 'brought', 'value': {'tally': {'sum': 'p_oil'}}},
                 {'do': 'set', 'var': 'for_lamp', 'value': {'tally': {'sum': 'p_oil', 'where': 'p_backs', 'is': 'lamp'}}}]},
             {'id': 'done', 'on': 'quest completed', 'match': {'detail': 'oil'}, 'actions': [
                 {'do': 'rank', 'var': 'p_oil', 'top': 2, 'min': 2, 'into': 'best'},
                 {'do': 'each', 'over': 'list', 'list': 'best', 'actions': [
                     {'do': 'effect', 'effect': {'kind': 'title.give', 'player': '$each', 'title': 'the Oil-Bringer'}}]},
                 {'do': 'effect', 'effect': {'kind': 'belief.forget', 'subject': 'rm113', 'claimKind': 'story.rumour', 'town': 'greyfen', 'share': 50}},
                 {'do': 'enter', 'phase': 'lit'}]},
         ]},
        {'id': 'lit', 'journal': 'The lamps of Greyfen burn through the long nights.', 'effects': [], 'exits': [],
         'triggers': [
             {'id': 'spilled', 'on': 'deed', 'audience': 'participant', 'match': {'detail': {'starts': 'Spilled the oil'}}, 'actions': [
                 {'do': 'effect', 'effect': {'kind': 'title.take', 'player': '$actor', 'title': 'the Oil-Bringer'}}]}]},
    ],
    'tests': [
        {'name': 'Three bring oil; the two who brought most are named', 'cohort': [ASH, BIRCH, ELM],
         'events': [
             quest_fact(1, 'arrival', 0.5, ASH),
             quest_fact(2, 'arrival', 0.6, BIRCH),
             quest_fact(3, 'arrival', 0.7, ELM),
             quest_fact(4, 'gift', 1, ASH, target='rm111', item='lamp_oil', quantity=5),
             quest_fact(5, 'gift', 1.1, BIRCH, target='rm111', item='lamp_oil', quantity=3),
             quest_fact(6, 'gift', 1.2, ELM, target='rm111', item='lamp_oil', quantity=1),
             quest_fact(7, 'deed', 2, BIRCH, detail='Spilled the oil on the stair (d9)'),
         ], 'expect': ['drive', 'lit'],
         'expectQuests': {'oil': {'story': 'done'}},
         'expectVariables': {'story': {'brought': 9, 'for_lamp': 9, 'best': [ASH, BIRCH]},
                             'player': {ASH: {'p_oil': 5}, BIRCH: {'p_oil': 3}, ELM: {'p_oil': 1}}},
         'expectEffects': [
             {'phase': 'drive', 'kind': 'npc.line', 'npc': 'rm111', 'text': '#greet# The lamps want oil, and plenty of it.'},
             {'phase': 'drive', 'kind': 'character.grant', 'player': ASH, 'purse': 'town:greyfen', 'reason': 'the lamplighter\'s thanks', 'coins': 10,
              'reward': 'oil'},
             {'phase': 'drive', 'kind': 'character.grant', 'player': BIRCH, 'purse': 'town:greyfen', 'reason': 'the lamplighter\'s thanks', 'coins': 6,
              'reward': 'oil'},
             {'phase': 'drive', 'kind': 'npc.greet', 'npc': 'rm111', 'player': ASH, 'text': '#greet# The lamps burn on your oil. I\'ll not forget it.',
              'days': 14, 'reward': 'oil'},
             {'phase': 'drive', 'kind': 'npc.greet', 'npc': 'rm111', 'player': BIRCH, 'text': '#greet# The lamps burn on your oil. I\'ll not forget it.',
              'days': 14, 'reward': 'oil'},
             {'phase': 'drive', 'kind': 'title.give', 'player': ASH, 'title': 'Lamp-bearer of Greyfen', 'known': 'town', 'town': 'greyfen', 'days': 30,
              'reward': 'oil'},
             {'phase': 'drive', 'kind': 'title.give', 'player': BIRCH, 'title': 'Lamp-bearer of Greyfen', 'known': 'town', 'town': 'greyfen', 'days': 30,
              'reward': 'oil'},
             {'phase': 'drive', 'kind': 'title.give', 'player': ASH, 'title': 'the Oil-Bringer'},
             {'phase': 'drive', 'kind': 'title.give', 'player': BIRCH, 'title': 'the Oil-Bringer'},
             {'phase': 'drive', 'kind': 'belief.forget', 'subject': 'rm113', 'claimKind': 'story.rumour', 'town': 'greyfen', 'share': 50},
             {'phase': 'lit', 'kind': 'title.take', 'player': BIRCH, 'title': 'the Oil-Bringer'}]},
    ],
}


# ----- Doc 86, Phase 4: stories that talk to each other. A line of two quests at the lamplighter's, the second only for the
# wolves who did the first (C6 requires) with words while under way (offer.during), closing the line (J3 closure "line");
# the other road taken back from the wolf who chose (L5 quest.withdraw); the news told to the campaign's chapters (C12a
# story.notify) and news heard from a pack (story notified); the realm's tally changed by another story (C13a variable
# changed), a new month and a post changing hands (C13a); a Quickened wolf, and the post a resident holds (J10, C13b).
LINE = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'v2-lamp-line', 'title': 'The lamp line',
    'audience': {'kind': 'players', 'players': [ASH, BIRCH, ELM]}, 'start': 'line',
    'variables': [
        {'id': 'p_voice', 'type': 'number', 'scope': 'player', 'show': 'Your voice in the lamp moot'},
        {'id': 'nh_realm', 'type': 'number', 'scope': 'world', 'note': 'The realm\'s tally, which every story of the campaign keeps.'},
        {'id': 'heard', 'type': 'number', 'scope': 'story', 'note': 'The realm\'s tally as last heard.'},
        {'id': 'month', 'type': 'number', 'scope': 'story', 'note': 'The month that began last.'},
        {'id': 'handovers', 'type': 'number', 'scope': 'story', 'note': 'Posts that changed hands.'},
        {'id': 'gifted', 'type': 'number', 'scope': 'story', 'note': 'Quickened wolves seen at the gate while the Crown was held.'},
    ],
    'quests': [
        {'id': 'lamp', 'title': 'The lamplighter\'s lamp', 'giver': 'rm111', 'handIn': False,
         'offer': {'say': '#greet# My lamp went over the wall in the wind. Would you look for it?'},
         'objectives': [{'id': 'find', 'label': 'Find the lamp and bring it back.', 'on': 'gift', 'match': {'target': 'rm111', 'item': 'lamp'}}]},
        {'id': 'oil', 'title': 'Oil for the lamp', 'giver': 'rm111', 'handIn': False, 'closure': 'line',
         'requires': {'test': 'quest is', 'quest': 'lamp', 'status': 'done'},
         'offer': {'say': 'Now it wants oil. The chandler will have some.', 'during': 'Keep the oil dry on the road, #address#.',
                   'duringAsk': 'About the oil...'},
         'objectives': [{'id': 'fetch', 'label': 'Bring lamp oil from the chandler.', 'on': 'gift', 'match': {'target': 'rm222', 'item': 'lamp_oil'}}]},
        {'id': 'mill', 'title': 'Ask at the mill', 'giver': 'rm113', 'offer': {'say': 'Or you could ask the miller for tallow.'},
         'objectives': [{'id': 'ask', 'label': 'Ask the miller.', 'on': 'conversation', 'match': {'target': 'rm119'}}]},
    ],
    'phases': [
        {'id': 'line', 'journal': 'The lamplighter wants his lamp lit again.', 'effects': [], 'exits': [],
         'triggers': [
             {'id': 'took-oil', 'on': 'quest started', 'audience': 'participant', 'match': {'detail': 'oil'}, 'actions': [
                 {'do': 'effect', 'effect': {'kind': 'quest.withdraw', 'quest': 'mill', 'player': '$actor'}}]},
             {'id': 'lit', 'on': 'quest completed', 'audience': 'participant', 'match': {'detail': 'oil'}, 'actions': [
                 {'do': 'effect', 'effect': {'kind': 'story.notify', 'signal': 'lamp_lit', 'player': '$actor', 'amount': 2}},
                 {'do': 'effect', 'effect': {'kind': 'story.notify', 'signal': 'lamp_lit', 'story': 'nh-ch1'}}]},
             {'id': 'news', 'on': 'story notified', 'audience': 'participant', 'match': {'detail': 'voice'}, 'actions': [
                 {'do': 'add', 'var': 'p_voice', 'value': {'field': 'quantity'}}]},
             {'id': 'realm', 'on': 'variable changed', 'match': {'target': 'nh_realm'}, 'actions': [
                 {'do': 'set', 'var': 'heard', 'value': {'field': 'quantity'}}]},
             {'id': 'month', 'on': 'month begins', 'actions': [{'do': 'set', 'var': 'month', 'value': {'field': 'quantity'}}]},
             {'id': 'handover', 'on': 'succession', 'match': {'detail': {'starts': 'reigning at the Great Palace'}}, 'actions': [
                 {'do': 'add', 'var': 'handovers', 'value': 1}]},
             {'id': 'gate', 'on': 'deed', 'audience': 'participant', 'match': {'detail': {'starts': 'Held the gate'}},
              'if': {'test': 'all', 'conditions': [{'test': 'wolf has', 'what': 'tier', 'value': 'quickened'},
                                                   {'test': 'resident has', 'npc': 'rm112', 'what': 'post', 'value': 'reigning at the Great Palace'}]},
              'actions': [{'do': 'add', 'var': 'gifted', 'value': 1}]},
         ]},
    ],
    'tests': [
        {'name': 'Ash walks the line to its end; the realm and the calendar move', 'cohort': [ASH, BIRCH, ELM],
         'events': [
             quest_fact(1, 'quest accepted', 0.5, ASH, item='rm111', detail='lamp'),
             quest_fact(2, 'gift', 0.6, ASH, target='rm111', item='lamp'),
             quest_fact(3, 'quest accepted', 0.7, BIRCH, item='rm111', detail='oil'),
             quest_fact(4, 'quest accepted', 0.8, ASH, item='rm111', detail='oil'),
             quest_fact(5, 'gift', 1, ASH, target='rm222', item='lamp_oil'),
             quest_fact(6, 'story notified', 1.1, ASH, target='nh-pack', detail='voice', quantity=3),
             quest_fact(7, 'variable changed', 1.2, target='nh_realm', detail='4', quantity=4),
             quest_fact(8, 'month begins', 1.3, detail='2', quantity=2),
             quest_fact(9, 'succession', 1.4, 'rm112', target='rm119', detail='reigning at the Great Palace: a vacancy filled'),
             {**quest_fact(10, 'deed', 1.5, ASH, detail='Held the gate (d2)'),
              'facts': {'wolves': {ASH: {'gift': {'tier': 'quickened', 'family': 'fire'}}},
                        'residents': {'rm112': {'alive': True, 'post': 'reigning at the Great Palace'}}}},
             {**quest_fact(11, 'deed', 1.6, BIRCH, detail='Held the gate (d2)'),
              'facts': {'wolves': {BIRCH: {'gift': {'tier': 'normal', 'family': ''}}},
                        'residents': {'rm112': {'alive': True, 'post': 'reigning at the Great Palace'}}}},
         ], 'expect': ['line'],
         'expectQuests': {'lamp': {ASH: 'done'}, 'oil': {ASH: 'done'}},
         'expectVariables': {'story': {'heard': 4, 'month': 2, 'handovers': 1, 'gifted': 1}, 'player': {ASH: {'p_voice': 3}}},
         'expectEffects': [
             {'phase': 'line', 'kind': 'story.closure', 'player': ASH, 'story': 'v2-lamp-line', 'quest': 'oil', 'closure': 'line',
              'title': 'Oil for the lamp', 'reward': 'oil'}]},
    ],
}


# Doc 86, Phase 5 (J5, J6, L1): howls, crowds and the wild. At nightfall the tarn-keeper howls (npc.howl) and a bear waits for
# the next hunt begun at the tarn (hunt.stage); every wolf's howl is counted (howled); the ropes are manned when three of the
# seven of the crew stand at the tarn (count in region, the residents of a list), not when one of them is dead there; a
# chorus of two does not call the hunt, one of three does (chorus closed, the exit's minQuantity); the bear, killed in the
# hunt begun at the tarn, ends the quest (an ordinary "hunted", target the bear).
TARN = {'id': 'tarn', 'cell': 'grey', 'x1': 2, 'y1': 2, 'x2': 6, 'y2': 6}
CREW = ['rm111', 'rm112', 'rm113', 'rm114', 'rm115', 'rm116', 'rm117']


def crew_facts(inside, dead=()):
    return {'residents': {r: {'alive': r not in dead, 'cell': 'grey', 'x': 3 if r in inside or r in dead else 9, 'y': 3, 'indoors': False}
                          for r in CREW}}


HOWLS = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'v2-old-brown', 'title': 'Old Brown at the tarn',
    'audience': {'kind': 'players', 'players': [ASH, BIRCH, ELM]}, 'start': 'gather', 'regions': [TARN],
    'variables': [
        {'id': 'crew', 'type': 'residents', 'scope': 'story', 'initial': list(CREW), 'note': 'The tarn crew who man the ropes.'},
        {'id': 'howls', 'type': 'number', 'scope': 'story', 'note': 'Howls heard from the story\'s wolves.'},
        {'id': 'ropes', 'type': 'switch', 'scope': 'story', 'note': 'The ropes manned at the tarn.'},
    ],
    'quests': [
        {'id': 'bear', 'title': 'Old Brown', 'giver': 'rm111', 'handIn': False,
         'offer': {'say': '#greet# There is a bear at the tarn, old and brown. Go out after him at dusk, when he comes to drink.'},
         # (Where by the cell, not the region: a hunt's kill is recorded at the killer's tile in its arena, which spans more
         # than the tarn; the stage already ties the bear to the tarn's hunts.)
         'objectives': [{'id': 'kill', 'label': 'Bring down Old Brown at the tarn.', 'on': 'hunted', 'match': {'target': 'bear'},
                         'where': {'cells': ['grey']}}]},
    ],
    'phases': [
        {'id': 'gather', 'journal': 'The tarn crew gather at dusk, and the keeper howls for the hunters.', 'effects': [],
         'exits': [{'on': 'chorus closed', 'to': 'hunt', 'minQuantity': 3}],
         'triggers': [
             {'id': 'dusk', 'on': 'night falls', 'actions': [
                 {'do': 'effect', 'effect': {'kind': 'npc.howl', 'npc': 'rm112'}},
                 {'do': 'effect', 'effect': {'kind': 'hunt.stage', 'region': 'tarn', 'species': 'bear', 'count': 1, 'minutes': 120}}]},
             {'id': 'heard', 'on': 'howled', 'audience': 'participant', 'actions': [{'do': 'add', 'var': 'howls', 'value': 1}]},
             {'id': 'manned', 'on': 'deed', 'audience': 'participant',
              'if': {'test': 'count in region', 'region': 'tarn', 'op': '>=', 'value': 3, 'who': 'residents', 'list': 'crew'},
              'actions': [{'do': 'turn on', 'var': 'ropes'}]},
         ]},
        {'id': 'hunt', 'journal': 'The chorus has called the hunt: Old Brown is at the water.', 'effects': [], 'exits': []},
    ],
    'tests': [
        {'name': 'Dusk at the tarn: the keeper howls, the crew man the ropes, three call the hunt, the bear falls', 'cohort': [ASH, BIRCH, ELM],
         'events': [
             quest_fact(1, 'night falls', 0.5),
             quest_fact(2, 'howled', 0.51, ASH, cell='grey', item='greyfen', detail='howl-1'),
             {**quest_fact(3, 'deed', 0.52, ASH, detail='Held a rope'), 'facts': crew_facts(CREW[:2], dead=CREW[2:3])},
             {**quest_fact(4, 'deed', 0.53, BIRCH, detail='Held a rope'), 'facts': crew_facts(CREW[:3])},
             quest_fact(5, 'chorus closed', 0.54, 'rm112', cell='grey', item='greyfen', detail='howl-1', quantity=2),
             quest_fact(6, 'quest accepted', 0.6, ASH, item='rm111', detail='bear'),
             quest_fact(7, 'hunted', 0.7, ASH, target='bear', cell='grey', x=3, y=4, detail='a clean kill', quantity=3),
             quest_fact(8, 'chorus closed', 0.8, 'rm112', cell='grey', item='greyfen', detail='howl-2', quantity=3),
         ], 'expect': ['gather', 'hunt'],
         'expectQuests': {'bear': {ASH: 'done'}},
         'expectVariables': {'story': {'howls': 1, 'ropes': True}},
         'expectEffects': [
             {'phase': 'gather', 'kind': 'npc.howl', 'npc': 'rm112'},
             {'phase': 'gather', 'kind': 'hunt.stage', 'region': 'tarn', 'species': 'bear', 'count': 1, 'minutes': 120}]},
    ],
}


# Doc 86, Phase 6 (J9): a story's own items. The clerk gives the wolf who asks the watch list, a letter marked watch_list and
# called "The watch list" (taken back when the run ends); a wolf carrying it is counted; only that letter, given to the
# warden, does the quest (a plain letter is not it), and the warden's thanks is a writ the wolf keeps after the story.
WATCH_LIST = {'kind': 'item.give', 'player': '$actor', 'item': 'letter', 'quantity': 1, 'tag': 'watch_list', 'name': 'The watch list'}
THANKS_WRIT = {'kind': 'item.give', 'player': '$actor', 'item': 'writ', 'quantity': 1, 'tag': 'thanks', 'name': "The warden's writ of thanks",
               'keep': True}


def carrying(wolf, n):
    return {'wolves': {wolf: {'cell': 'grey', 'x': 3, 'y': 3, 'indoors': False, 'town': 'greyfen', 'party': '', 'partySize': 1,
                              'carries': {'letter^watch_list': n} if n else {}, 'coins': 0}}}


LETTERS = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'v2-watch-list', 'title': 'The watch list',
    'audience': {'kind': 'players', 'players': [ASH, BIRCH]}, 'start': 'carry',
    'variables': [
        {'id': 'carried', 'type': 'switch', 'scope': 'story', 'note': 'A wolf was seen carrying the watch list.'},
        {'id': 'letters', 'type': 'number', 'scope': 'story', 'note': 'Letters of any kind given to the warden.'},
    ],
    'quests': [
        {'id': 'deliver', 'title': 'The watch list', 'giver': 'rm111', 'handIn': False,
         'offer': {'say': '#greet# Take this list to the warden at the gate, and to no one else.'},
         'objectives': [{'id': 'hand', 'label': 'Give the watch list to the warden.', 'on': 'gift',
                         'match': {'item': 'letter^watch_list', 'target': 'rm112'}}],
         'rewards': [THANKS_WRIT]},
    ],
    'phases': [
        {'id': 'carry', 'journal': 'The clerk has a list for the warden at the gate.', 'effects': [],
         'exits': [{'on': 'quest completed', 'to': 'done', 'match': {'detail': 'deliver'}}],
         'triggers': [
             {'id': 'list', 'on': 'quest accepted', 'audience': 'participant', 'match': {'detail': 'deliver'}, 'actions': [{'do': 'effect', 'effect': WATCH_LIST}]},
             {'id': 'seen', 'on': 'deed', 'audience': 'participant',
              'if': {'test': 'carries', 'item': 'letter^watch_list', 'op': '>=', 'value': 1},
              'actions': [{'do': 'turn on', 'var': 'carried'}]},
             {'id': 'any', 'on': 'gift', 'match': {'item': 'letter', 'target': 'rm112'}, 'actions': [{'do': 'add', 'var': 'letters', 'value': 1}]},
         ]},
        {'id': 'done', 'journal': 'The warden has the watch list.', 'effects': [], 'exits': []},
    ],
    'tests': [
        {'name': 'The list is given, carried, and only it does the quest; the writ of thanks is kept', 'cohort': [ASH, BIRCH],
         'events': [
             quest_fact(1, 'quest accepted', 0.1, ASH, item='rm111', detail='deliver'),
             {**quest_fact(2, 'deed', 0.2, BIRCH, detail='Walked the wall'), 'facts': carrying(BIRCH, 0)},
             {**quest_fact(3, 'deed', 0.3, ASH, detail='Walked the wall'), 'facts': carrying(ASH, 1)},
             quest_fact(4, 'gift', 0.4, ASH, target='rm112', item='letter', quantity=1),
             quest_fact(5, 'gift', 0.5, ASH, target='rm112', item='letter^watch_list', quantity=1),
         ], 'expect': ['carry', 'done'],
         'expectQuests': {'deliver': {ASH: 'done'}},
         'expectVariables': {'story': {'carried': True, 'letters': 2}},
         'expectEffects': [
             {'phase': 'carry', **WATCH_LIST, 'player': ASH},
             {'phase': 'carry', **THANKS_WRIT, 'player': ASH, 'reward': 'deliver'}]},
    ],
}


# Doc 86, Phase 7 (C10, T3): foes and strangers. The Grey Captain, a stranger who looks like rm111, is brought to the parley
# and speaks there in his own panel (a scene cast entry for a visitor); the parley over, he holds the Pass as a champion foe
# with a band that grows with the wolves who are about (min(8, active / 2)), and the turncoat rm113 is made a foe beside
# him for an hour, never killed. Each of the band brought down is counted (foe downed); the Captain beaten (foe defeated,
# the wolves who fought him its quantity) wins the Pass; if he gets away (foe fled), the Pass is lost.
PASS = {'id': 'pass', 'cell': 'grey', 'x1': 2, 'y1': 2, 'x2': 9, 'y2': 9}
PARLEY = {'format': 'ratw-scene', 'version': 1, 'id': 'parley', 'title': 'The parley at the Pass', 'visibility': 'public', 'lockMovement': False,
          'area': {'cell': 'grey', 'x1': 2, 'y1': 2, 'x2': 9, 'y2': 9},
          'cast': [{'id': 'captain', 'visitor': 'grey-captain'}, {'id': 'desmond', 'npc': 'rm112'}], 'start': 'terms',
          'beats': [{'id': 'terms', 'type': 'line', 'speaker': 'captain', 'text': 'Give us the Pass by nightfall and no one hangs.', 'seconds': 8,
                     'next': 'answer'},
                    {'id': 'answer', 'type': 'line', 'speaker': 'desmond', 'text': 'Then you will have to take it from the wolves.', 'seconds': 8}]}
CAPTAIN = {'kind': 'foe.add', 'foe': 'captain', 'name': 'The Grey Captain', 'cell': 'grey', 'x': 5, 'y': 5, 'strength': 'champion',
           'band': {'min': [8, {'div': [{'audience': 'active'}, 2]}]}, 'stance': 'waits', 'minutes': 120, 'like': 'rm111'}
TURNCOAT = {'kind': 'foe.outlaw', 'npc': 'rm113', 'minutes': 60, 'strength': 'tough', 'band': 1}

FOES = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'v2-grey-pass', 'title': 'The Grey Captain at the Pass',
    'audience': {'kind': 'players', 'players': [ASH, BIRCH, ELM]}, 'start': 'parley', 'regions': [PASS], 'scenes': [PARLEY],
    'variables': [
        {'id': 'band-down', 'type': 'number', 'scope': 'story', 'show': "The Captain's band brought down"},
        {'id': 'fought', 'type': 'number', 'scope': 'story', 'note': 'How many wolves fought the Captain.'},
    ],
    'phases': [
        {'id': 'parley', 'journal': 'The Grey Captain comes under a white flag to the Pass.',
         'effects': [{'kind': 'visitor.add', 'visitor': 'grey-captain', 'name': 'The Grey Captain', 'cell': 'grey', 'x': 4, 'y': 4,
                      'minutes': 30, 'like': 'rm111'},
                     {'kind': 'scene.play', 'scene': 'parley', 'cue': 'parley', 'player': 'world'}],
         'exits': [{'on': 'scene finished', 'match': {'detail': 'parley'}, 'to': 'pass'},
                   {'on': 'scene failed', 'match': {'detail': 'parley'}, 'to': 'pass'}],
         # (The Captain and the turncoat come as the parley ends: a trigger's effects may work out a value, the band.)
         'triggers': [{'id': 'muster', 'on': 'scene finished', 'match': {'detail': 'parley'},
                       'actions': [{'do': 'effect', 'effect': CAPTAIN}, {'do': 'effect', 'effect': TURNCOAT}]},
                      {'id': 'muster-anyway', 'on': 'scene failed', 'match': {'detail': 'parley'},
                       'actions': [{'do': 'effect', 'effect': CAPTAIN}, {'do': 'effect', 'effect': TURNCOAT}]}]},
        {'id': 'pass', 'journal': 'The parley is over. The Captain holds the Pass, and the turncoat stands with him.',
         'effects': [{'kind': 'visitor.leave', 'visitor': 'grey-captain'}],
         'exits': [{'on': 'foe defeated', 'match': {'target': 'captain'}, 'to': 'held'},
                   {'on': 'foe fled', 'match': {'target': 'captain'}, 'to': 'lost'}],
         'triggers': [
             {'id': 'downed', 'on': 'foe downed', 'match': {'target': 'captain'}, 'actions': [{'do': 'add', 'var': 'band-down', 'value': 1}]},
             {'id': 'beaten', 'on': 'foe defeated', 'match': {'target': 'captain'},
              'actions': [{'do': 'set', 'var': 'fought', 'value': {'field': 'quantity'}}]},
             {'id': 'turncoat', 'on': 'foe defeated', 'match': {'target': 'rm113'},
              'actions': [{'do': 'note', 'text': 'The turncoat is down, and held.'}]},
         ]},
        {'id': 'held', 'journal': 'The Captain is beaten and his band scattered: the Pass is held.', 'effects': [], 'exits': []},
        {'id': 'lost', 'journal': 'The Captain got away over the Pass.', 'effects': [], 'exits': []},
    ],
    'tests': [
        {'name': 'The parley, then the Pass: two of the band down, the turncoat beaten, the Captain beaten by three', 'cohort': [ASH, BIRCH, ELM],
         'events': [
             quest_fact(6, 'arrival', 0.1, ASH), quest_fact(7, 'arrival', 0.11, BIRCH), quest_fact(8, 'arrival', 0.12, ELM),
             quest_fact(1, 'scene finished', 0.2, '', target='world-run', detail='parley'),
             quest_fact(2, 'foe downed', 0.3, ASH, target='captain', cell='grey', x=5, y=6, detail="one of The Grey Captain's band"),
             quest_fact(3, 'foe downed', 0.31, BIRCH, target='captain', cell='grey', x=6, y=6, detail="one of The Grey Captain's band"),
             quest_fact(4, 'foe defeated', 0.32, ELM, target='rm113', cell='grey', x=4, y=6, detail='the turncoat', quantity=1),
             quest_fact(5, 'foe defeated', 0.35, ASH, target='captain', cell='grey', x=5, y=5, detail='The Grey Captain', quantity=3),
         ], 'expect': ['parley', 'pass', 'held'],
         'expectVariables': {'story': {'band-down': 2, 'fought': 3}},
         'expectEffects': [
             {'phase': 'parley', 'kind': 'visitor.add', 'visitor': 'grey-captain', 'name': 'The Grey Captain', 'cell': 'grey', 'x': 4, 'y': 4,
              'minutes': 30, 'like': 'rm111'},
             {'phase': 'parley', 'kind': 'scene.play', 'scene': 'parley', 'cue': 'parley', 'player': 'world'},
             # (Three wolves about: a band of min(8, 3 / 2), one.)
             {'phase': 'parley', 'kind': 'foe.add', 'foe': 'captain', 'name': 'The Grey Captain', 'cell': 'grey', 'x': 5, 'y': 5,
              'strength': 'champion', 'band': 1, 'stance': 'waits', 'minutes': 120, 'like': 'rm111'},
             {'phase': 'parley', 'kind': 'foe.outlaw', 'npc': 'rm113', 'minutes': 60, 'strength': 'tough', 'band': 1},
             {'phase': 'pass', 'kind': 'visitor.leave', 'visitor': 'grey-captain'}]},
        {'name': 'The Captain gets away: the Pass is lost', 'cohort': [ASH, BIRCH, ELM],
         'events': [
             quest_fact(1, 'scene failed', 0.2, '', target='world-run', detail='parley', item='a stranger is not here'),
             quest_fact(2, 'foe fled', 0.5, '', target='captain', cell='grey', x=5, y=5, item='fled', detail='The Grey Captain'),
         ], 'expect': ['parley', 'pass', 'lost']},
    ],
}



# ----- Doc 86, Phase 8: across acts and the crown (tools/test_story_v2.py; both twins, tools/test_story_native.py). The
# Moot Night: each wolf's voice and pledge come from the acts before (realm variables, J2: a case gives what each wolf
# brings); each wolf who howls at the Moot adds to their own voice; when the chorus closes with three or more, the loudest
# is ranked and the voices pledged to Coldwater are tallied. Enough of them, and the crown changes hands (C14, npc.post:
# rm111 takes the post rm112 held) and the wreckers' lamp-keeper serves a week in the Watch's cells (L4, npc.sentence);
# too few, and the Moot refuses.
CROWN = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'v2-moot-night', 'title': 'The Moot Night',
    'audience': {'kind': 'players', 'players': [ASH, BIRCH, ELM]}, 'start': 'moot',
    'variables': [
        {'id': 'nh_voice', 'type': 'number', 'scope': 'realm', 'show': 'Your voice in the Moot'},
        {'id': 'nh_pledge', 'type': 'text', 'scope': 'realm', 'talk': True},
        {'id': 'voices', 'type': 'number', 'scope': 'story', 'show': 'Voices raised at the Moot'},
        {'id': 'loudest', 'type': 'wolves', 'scope': 'story'},
        {'id': 'for-coldwater', 'type': 'number', 'scope': 'story', 'note': "The voices of the wolves pledged to Coldwater."},
    ],
    'phases': [
        {'id': 'moot', 'journal': 'The Moot sits tonight. Raise your voice with the others.', 'effects': [], 'exits': [],
         'triggers': [
             {'id': 'howl', 'on': 'howled', 'audience': 'participant', 'once': 'player',
              'actions': [{'do': 'add', 'var': 'nh_voice', 'value': 1}, {'do': 'add', 'var': 'voices', 'value': 1}]},
             {'id': 'chorus', 'on': 'chorus closed', 'if': {'test': 'field', 'field': 'quantity', 'op': '>=', 'value': 3}, 'actions': [
                 {'do': 'rank', 'var': 'nh_voice', 'top': 1, 'into': 'loudest'},
                 {'do': 'set', 'var': 'for-coldwater', 'value': {'tally': {'sum': 'nh_voice', 'where': 'nh_pledge', 'is': 'coldwater'}}},
                 {'do': 'if', 'if': {'test': 'compare', 'left': {'var': 'for-coldwater'}, 'op': '>=', 'right': 5},
                  'then': [{'do': 'effect', 'effect': {'kind': 'npc.post', 'npc': 'rm111', 'post': 'job:rm112'}},
                           {'do': 'effect', 'effect': {'kind': 'npc.sentence', 'npc': 'rm113', 'place': 'grey-cells', 'days': 7}},
                           {'do': 'enter', 'phase': 'crowned'}],
                  'else': [{'do': 'enter', 'phase': 'refused'}]}]},
         ]},
        {'id': 'crowned', 'journal': 'The Moot has spoken: the crown passes, and the lamp-keeper goes to the cells for a week.',
         'effects': [], 'exits': []},
        {'id': 'refused', 'journal': 'Too few voices: the Moot will not name a new ruler tonight.', 'effects': [], 'exits': []},
    ],
    'tests': [
        {'name': 'Pledged in the acts before: the crown passes', 'cohort': [ASH, BIRCH, ELM],
         'realm': {ASH: {'nh_voice': 3, 'nh_pledge': 'coldwater'}, BIRCH: {'nh_voice': 1, 'nh_pledge': 'coldwater'},
                   ELM: {'nh_pledge': 'larkfield'}},
         'events': [
             quest_fact(1, 'arrival', 0.1, ASH), quest_fact(2, 'arrival', 0.11, BIRCH), quest_fact(3, 'arrival', 0.12, ELM),
             quest_fact(4, 'howled', 0.2, ASH, cell='grey', item='greyfen', detail='moot'),
             quest_fact(5, 'howled', 0.21, BIRCH, cell='grey', item='greyfen', detail='moot'),
             quest_fact(6, 'howled', 0.22, ELM, cell='grey', item='greyfen', detail='moot'),
             quest_fact(7, 'howled', 0.23, ASH, cell='grey', item='greyfen', detail='moot'),
             quest_fact(8, 'chorus closed', 0.25, ASH, cell='grey', item='greyfen', detail='moot', quantity=3),
         ], 'expect': ['moot', 'crowned'],
         'expectVariables': {'story': {'voices': 3, 'loudest': [ASH], 'for-coldwater': 6},
                             'realm': {ASH: {'nh_voice': 4}, BIRCH: {'nh_voice': 2}, ELM: {'nh_voice': 1}}},
         'expectEffects': [
             {'phase': 'moot', 'kind': 'npc.post', 'npc': 'rm111', 'post': 'job:rm112'},
             {'phase': 'moot', 'kind': 'npc.sentence', 'npc': 'rm113', 'place': 'grey-cells', 'days': 7}]},
        {'name': 'New wolves, pledged to nobody: the Moot refuses', 'cohort': [ASH, BIRCH, ELM],
         'events': [
             quest_fact(1, 'howled', 0.2, ASH, cell='grey', item='greyfen', detail='moot'),
             quest_fact(2, 'howled', 0.21, BIRCH, cell='grey', item='greyfen', detail='moot'),
             quest_fact(3, 'chorus closed', 0.25, ASH, cell='grey', item='greyfen', detail='moot', quantity=3),
         ], 'expect': ['moot', 'refused'],
         'expectVariables': {'story': {'voices': 2, 'for-coldwater': 0}},
         'expectEffects': []},
    ],
}
