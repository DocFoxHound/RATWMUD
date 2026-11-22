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


def all_samples():
    return [copy.deepcopy(LEDGER), copy.deepcopy(FLOW)]


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
