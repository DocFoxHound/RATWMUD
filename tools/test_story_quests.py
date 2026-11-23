"""Quests and a living audience (Docs/Design/85-story-triggers-and-campaign-control.md, 3.6 and Phase 6).

Pure: the checks of a program's quests; rehearsals for each scope (a wolf's own, a party's, the whole story's), objectives
in any order and after one another, optional, hidden and counted ones, fail rules and deadlines, repeats, rewards and the
bonus, offers accepted and declined, abandoning, an open and an everyone audience, the effects a trigger sends to quests;
the journal views; a move onto a new version keeping and resetting quests. In a throwaway, migrated database (skipped
without the local PostgreSQL): a small quest published with an open audience, taken in a resident's window, done and
handed in, the orders the game gets (`story.cohort`, `story.quests`, the rewards, a coin reward from a purse refused and
the story carrying on), each wolf's row in dm.story_player_state, the console's Quests grid and its commands, a party told
to the runner, and a run with quests moved onto a new version and back."""

import copy
import json
import unittest

import story_program as rules
import story_quests as quests
import story_runner as runner
import test_story_console as console
from story_v2_samples import QUEST_CAMPAIGN as CAMPAIGN, QUEST_SMALL as SMALL, quest_fact as fact
import test_story_steering as steering
from test_world_db import database_available

from story_v2_samples import ASH, BIRCH, ELM


def program(base=SMALL, **changes):
    p = copy.deepcopy(base)
    for k, v in changes.items():
        p[k] = v
    return p


def play(p, events, cohort=(), parties=None, **case):
    p = program(p)
    p['tests'] = [{'name': 'a case', 'cohort': list(cohort), 'events': events, 'expect': case.pop('expect', [p['start']]),
                   **({'parties': parties} if parties else {}), **case}]
    return rules._play_case(p, p['tests'][0]), p


def final_state(p, events, cohort=(), parties=None):
    """The state at the end of a case, through the rehearsal's own stepping."""
    p = program(p)
    p['tests'] = [{'name': 'a case', 'cohort': list(cohort), 'events': events, 'expect': [p['start']],
                   **({'parties': parties} if parties else {})}]
    seen = {}
    rules._play_case(p, p['tests'][0], lambda f, state, *_: seen.update(state=json.loads(json.dumps(state))))
    return seen['state'], p


class Checks(unittest.TestCase):
    def test_the_samples_pass(self):
        for p in (SMALL, CAMPAIGN):
            with self.subTest(p['id']):
                report = rules.review(p)
                self.assertTrue(report['passed'], report['tests'])
                self.assertEqual(rules.problems(p), [])

    def test_what_a_quest_may_not_be(self):
        q = SMALL['quests'][0]
        bad = [
            ({**q, 'scope': 'guild'}, 'scope'),
            ({**q, 'repeat': 'hourly'}, 'once, daily'),
            ({**q, 'giver': 'rm111', 'offer': {'ask': 'Hello'}}, 'say'),
            ({k: v for k, v in q.items() if k not in ('giver',)}, 'offer needs a giver'),
            ({**q, 'objectives': [{**o, 'optional': True} for o in q['objectives']]}, 'at least one objective is required'),
            ({**q, 'objectives': [{**q['objectives'][0], 'after': ['bring']}, q['objectives'][1]]}, 'in a circle'),
            ({**q, 'objectives': [q['objectives'][0], {**q['objectives'][1], 'after': ['nowhere']}]}, 'names objectives'),
            ({**q, 'join': {'region': 'nowhere'}}, 'region'),
            ({**q, 'join': {'cap': 0}}, 'cap'),
            ({**q, 'rewards': [{'kind': 'character.grant', 'player': '$actor', 'coins': 5, 'purse': 'made', 'reason': 'x'}]}, 'purse'),
            ({**q, 'rewards': [{'kind': 'participant.remove', 'player': '$actor'}]}, 'cannot be a reward'),
            ({**q, 'fail': [{'afterDays': 0}]}, 'deadline'),
            ({**q, 'scope': 'story', 'repeat': 'daily'}, 'only once'),
            ({**q, 'phases': ['nowhere']}, 'phases of this story'),
            ({**q, 'bonus': [{'kind': 'deed.award', 'player': '$actor', 'detail': 'x'}], 'objectives': [q['objectives'][0]]}, 'bonus'),
        ]
        for quest, words in bad:
            with self.subTest(words):
                with self.assertRaisesRegex(ValueError, words):
                    rules.validate(program(quests=[quest]))
        with self.assertRaisesRegex(ValueError, 'version 2'):
            rules.validate(program(version=1))
        with self.assertRaisesRegex(ValueError, 'unique'):
            rules.validate(program(quests=[q, q]))
        with self.assertRaisesRegex(ValueError, '"anyone"'):
            rules.validate(program(audience={'kind': 'players', 'players': [ASH]}, quests=[{**q, 'join': {'who': 'anyone'}}]))
        with self.assertRaisesRegex(ValueError, 'audience of 1 to 5000'):
            rules.validate(program(audience={'kind': 'open', 'cap': 5001}))

    def test_effects_name_the_quests_of_the_story(self):
        def with_trigger(effect, audience='participant'):
            return program(phases=[{**SMALL['phases'][0], 'triggers': [
                {'id': 't', 'on': 'conversation', 'audience': audience, 'actions': [{'do': 'effect', 'effect': effect}]}]}])
        rules.validate(with_trigger({'kind': 'objective.complete', 'quest': 'axle', 'objective': 'ask', 'player': '$actor'}))
        with self.assertRaisesRegex(ValueError, 'Unknown quest'):
            rules.validate(with_trigger({'kind': 'quest.start', 'quest': 'nope', 'player': '$actor'}))
        with self.assertRaisesRegex(ValueError, 'no objective'):
            rules.validate(with_trigger({'kind': 'objective.reveal', 'quest': 'axle', 'objective': 'nope', 'player': '$actor'}))
        # Taking a wolf in, offering or giving a quest may name the acting wolf from a trigger of anyone in the world.
        for kind in ('participant.add', 'quest.offer', 'quest.start'):
            effect = {'kind': kind, 'player': '$actor', **({'quest': 'axle'} if kind != 'participant.add' else {})}
            rules.validate(with_trigger(effect, audience='world'))
        with self.assertRaisesRegex(ValueError, 'participant trigger'):
            rules.validate(with_trigger({'kind': 'quest.complete', 'quest': 'axle', 'player': '$actor'}, audience='world'))
        # A phase sends no story effect as it opens; a version 1 story none at all.
        with self.assertRaisesRegex(ValueError, 'goes in a trigger'):
            rules.validate(program(phases=[{**SMALL['phases'][0], 'effects': [{'kind': 'participant.add', 'player': ASH}]}]))
        v1 = runner.catalogue()['examples']['reed-ford']
        v1 = copy.deepcopy(v1)
        v1['phases'][0].setdefault('triggers', []).append(
            {'id': 'nope', 'on': 'conversation', 'actions': [{'do': 'effect', 'effect': {'kind': 'participant.add', 'player': ASH}}]})
        with self.assertRaisesRegex(ValueError, 'version 2'):
            rules.validate(v1)

    def test_a_small_quest_phase_goes_on(self):
        phase = rules.phase_of(SMALL, rules.begin(SMALL, [], 0, 'r'))
        self.assertTrue(rules.ongoing(phase))
        self.assertEqual(phase['_quests'], ['axle'])
        # Its rewards are among what the phase may send (the hosted checks and the world's references read them).
        self.assertIn('character.grant', {e['kind'] for e in rules.possible_effects(phase)})


class Rehearsals(unittest.TestCase):
    def test_any_order_after_optional_hidden_and_the_bonus(self):
        events = [
            fact(1, 'quest accepted', 1, ASH, target='r', item='rm111', detail='axle'),
            fact(2, 'gift', 1.05, ASH, target='rm111', item='cart_wheel', quantity=1),       # optional, open from the start
            fact(3, 'gift', 1.1, ASH, target='rm111', item='axle', quantity=5),              # before "ask": not counted
            fact(4, 'conversation', 1.2, ASH, target='rm111'),
            fact(5, 'conversation', 1.25, ASH, target='rm119'),                             # hidden: done, so shown
            fact(6, 'gift', 1.3, ASH, target='rm111', item='axle', quantity=1),
        ]
        state, p = final_state(SMALL, events)
        c = state['quests']['axle'][ASH]
        self.assertEqual(c['status'], 'active')
        self.assertEqual(c['objectives']['bring'], {'n': 1})
        self.assertTrue(c['objectives']['wheel']['done'] and c['objectives']['thief']['shown'])
        events.append(fact(7, 'gift', 1.4, ASH, target='rm111', item='axle', quantity=1))
        state, p = final_state(SMALL, events)
        self.assertEqual(state['quests']['axle'][ASH]['status'], 'ready', 'with a giver: handed in to finish')
        state, p = final_state(SMALL, [*events[:1], *events[2:], fact(8, 'gift', 1.45, ASH, target='rm111', item='cart_wheel', quantity=1)])
        self.assertTrue(state['quests']['axle'][ASH]['objectives']['wheel']['done'], 'an optional one still counts while ready')
        events.append(fact(8, 'quest handed in', 1.5, ASH, target='r', item='rm111', detail='axle'))
        report, _ = play(SMALL, events, expectQuests={'axle': {ASH: 'done'}})
        self.assertTrue(report['passed'])
        self.assertEqual([e['kind'] for e in report['effects']], ['deed.award', 'character.grant', 'item.give'],
                         'the bonus: every optional objective was done too')
        self.assertTrue(all(e['player'] == ASH and e['reward'] == 'axle' for e in report['effects']))

    def test_a_hidden_objective_is_not_in_the_journal_until_shown(self):
        state, p = final_state(SMALL, [fact(1, 'quest accepted', 1, ASH, target='r', item='rm111', detail='axle')])
        view = quests.views(p, state, ASH)[0]
        self.assertEqual([o['id'] for o in view['objectives']], ['ask', 'bring', 'wheel'])
        self.assertEqual(view['hiddenLeft'], 1)
        self.assertEqual([o['open'] for o in view['objectives']], [True, False, True], '"bring" comes after "ask"')
        self.assertEqual((view['status'], view['giver'], view['abandon'], view['accept']), ('active', 'rm111', True, False))

    def test_repeat_cap_and_who_may_take_it(self):
        accept = lambda i, wolf, day: fact(i, 'quest accepted', day, wolf, target='r', item='rm111', detail='axle')
        done = [accept(1, ASH, 1), fact(2, 'conversation', 1.1, ASH, target='rm111'),
                fact(3, 'gift', 1.2, ASH, target='rm111', item='axle', quantity=2), fact(4, 'quest handed in', 1.3, ASH, target='r', item='rm111', detail='axle')]
        state, p = final_state(SMALL, [*done, accept(5, ASH, 1.5)])
        self.assertEqual(state['quests']['axle'][ASH]['status'], 'done', 'daily: not again the same day')
        self.assertIn('Not again so soon', state['toasts'][ASH])
        state, p = final_state(SMALL, [*done, accept(5, ASH, 2.4)])
        self.assertEqual((state['quests']['axle'][ASH]['status'], state['quests']['axle'][ASH]['times']), ('active', 1))
        # The cap: two at once.
        state, p = final_state(SMALL, [accept(1, ASH, 1), accept(2, BIRCH, 1), accept(3, ELM, 1)])
        self.assertEqual(sorted(state['quests']['axle']), [ASH, BIRCH])
        self.assertEqual(state['cohort'], [ASH, BIRCH], 'Elm was not taken in')
        # A wolf asking another resident, or from the journal without an offer, is ignored.
        state, p = final_state(SMALL, [fact(1, 'quest accepted', 1, ASH, target='r', item='rm999', detail='axle'),
                                       fact(2, 'quest accepted', 1, BIRCH, target='r', item='', detail='axle')])
        self.assertNotIn('quests', state)
        # Only the story's wolves: a fixed audience.
        fixed = program(audience={'kind': 'players', 'players': [ASH]}, quests=[{**SMALL['quests'][0], 'join': {'who': 'participants'}}])
        fixed['tests'][0]['cohort'] = [ASH]
        state, p = final_state(fixed, [accept(1, BIRCH, 1), accept(2, ASH, 1)], cohort=[ASH])
        self.assertEqual(list(state['quests']['axle']), [ASH])

    def test_fail_rules_deadline_and_abandon(self):
        q = {**SMALL['quests'][0], 'fail': [{'on': 'death', 'match': {'actor': 'rm111'}}, {'afterDays': 2}]}
        p = program(quests=[q])
        accept = fact(1, 'quest accepted', 1, ASH, target='r', item='rm111', detail='axle')
        state, _ = final_state(p, [accept, fact(2, 'death', 1.5, 'rm111')])
        self.assertEqual(state['quests']['axle'][ASH]['status'], 'failed')
        state, _ = final_state(p, [accept, fact(2, 'clock', 3.1)])
        self.assertEqual(state['quests']['axle'][ASH]['status'], 'failed', 'its deadline passed')
        view = quests.views(p, state, ASH)[0]
        self.assertEqual((view['status'], view['again']), ('failed', 3.1), 'a failed quest may be taken again')
        state, _ = final_state(p, [accept, fact(2, 'quest abandoned', 1.2, ASH, target='r', detail='axle')])
        self.assertEqual(state['quests']['axle'][ASH]['status'], 'abandoned')
        fixed = program(p, quests=[{**q, 'abandon': False}])
        state, _ = final_state(fixed, [accept, fact(2, 'quest abandoned', 1.2, ASH, target='r', detail='axle')])
        self.assertEqual(state['quests']['axle'][ASH]['status'], 'active', 'this one cannot be abandoned')

    def test_party_story_scope_offers_and_the_everyone_audience(self):
        report = rules.review(CAMPAIGN)['tests'][0]
        self.assertTrue(report['passed'], report)
        self.assertEqual(report['cohort'], [ASH, BIRCH, ELM], 'Elm signed in: everyone')
        self.assertEqual([(e['kind'], e['player']) for e in report['effects']],
                         [('standing.adjust', ASH), ('standing.adjust', BIRCH)], 'each wolf of the party with credit')
        # The story's quest needs two contributors; started by an admin or a trigger.
        state, p = final_state(CAMPAIGN, [fact(1, 'arrival', 1, ELM)], cohort=[ASH, BIRCH])
        self.assertEqual(state['questOffers'], {'scout': [ELM]}, 'the arrival trigger offered Elm the scout')
        self.assertEqual(quests.views(p, state, ELM)[0]['status'], 'offered')
        self.assertTrue(quests.views(p, state, ELM)[0]['accept'], 'no giver: accepted from the journal')
        run = quests.Run(p, rules.phase_of(p, state), state, 1, 0, lambda f: None)
        run.story_effect({'kind': 'quest.start', 'quest': 'watch', 'player': ASH}, ASH)
        for i, wolf in enumerate((ASH, ASH, BIRCH)):
            run.track({'kind': 'region entered', 'actor': wolf, 'target': 'ford', 'detail': 'r'})
            if i == 1:
                self.assertEqual(state['quests']['watch']['story']['status'], 'active', 'one wolf twice is not two')
        self.assertEqual(state['quests']['watch']['story']['status'], 'done')
        self.assertEqual(sorted(e['player'] for e in state['triggerEffects']), [ASH, BIRCH])
        # Declining withdraws an offer; a party left keeps its credit.
        state, p = final_state(CAMPAIGN, [fact(1, 'arrival', 1, ELM), fact(2, 'quest declined', 1.1, ELM, target='r', detail='scout')],
                               cohort=[ASH, BIRCH])
        self.assertNotIn('questOffers', state)
        state, p = final_state(CAMPAIGN, [
            fact(1, 'quest accepted', 1.1, ASH, target='r', item='rm112', detail='ropes'),
            fact(2, 'gift', 1.2, BIRCH, target='rm112', item='rope', quantity=1),
            fact(3, 'party left', 1.3, BIRCH, target='p1'),
            fact(4, 'gift', 1.4, BIRCH, target='rm112', item='rope', quantity=2),     # no more credit after leaving
        ], cohort=[ASH, BIRCH], parties={ASH: 'p1', BIRCH: 'p1'})
        c = state['quests']['ropes']['party:p1']
        self.assertEqual((c['objectives']['rope']['n'], c['left'], c['credit']), (1, [BIRCH], [BIRCH]))
        self.assertEqual(state['parties'], {ASH: 'p1'})

    def test_phase_lifetime_npc_offer_and_trackers(self):
        state, p = final_state(CAMPAIGN, [
            fact(1, 'arrival', 1, ELM), fact(2, 'quest accepted', 1, ELM, target='r', detail='scout'),
            fact(3, 'conversation', 1.1, ELM, target='rm119'),
            fact(4, 'dialogue choice', 1.2, ELM, target='r', item='rm119', detail='hale-news'),
        ], cohort=[ASH, BIRCH])
        self.assertEqual(quests.choices(state), [{'id': 'hale-news', 'npc': 'rm119', 'words': 'Any news of the ice?',
                                                  'reply': 'Thin at the edges.', 'players': None, 'once': True, 'picked': [ELM]}])
        self.assertEqual(quests.trackers(p, state, ELM), [{'label': 'Ropes laid', 'value': 0}])
        offers = quests.offers(p, state)
        self.assertEqual([(o['quest'], o['npc'], o['who'], o['full']) for o in offers], [('ropes', 'rm112', 'anyone', False)])
        self.assertEqual(offers[0]['ask'], 'Ask about: Rope the ford')

    def test_participants_in_and_out(self):
        state, p = final_state(SMALL, [fact(1, 'quest accepted', 1, ASH, target='r', item='rm111', detail='axle')])
        run = quests.Run(p, rules.phase_of(p, state), state, 2, 0, lambda f: None)
        run.story_effect({'kind': 'participant.add', 'player': BIRCH}, '')
        self.assertEqual(state['cohort'], [ASH, BIRCH])
        run.story_effect({'kind': 'participant.remove', 'player': ASH}, '')
        self.assertEqual((state['cohort'], state['quests']['axle'][ASH]['status']), ([BIRCH], 'abandoned'))
        # Leaving the story from the journal (an open audience lets the wolf go).
        run.track({'kind': 'storyline abandoned', 'actor': BIRCH, 'target': 'r'})
        self.assertEqual(state['cohort'], [])
        # The cap of the audience.
        tiny = program(audience={'kind': 'open', 'cap': 1})
        state, p = final_state(tiny, [fact(1, 'quest accepted', 1, ASH, target='r', item='rm111', detail='axle'),
                                      fact(2, 'quest accepted', 1, BIRCH, target='r', item='rm111', detail='axle')])
        self.assertEqual((state['cohort'], state['toasts'][BIRCH]), ([ASH], 'An axle for the carter: The story has all the wolves it can take.'))


class Moving(unittest.TestCase):
    def test_quests_kept_reset_and_dropped(self):
        events = [fact(1, 'quest accepted', 1, ASH, target='r', item='rm111', detail='axle'), fact(2, 'conversation', 1.1, ASH, target='rm111')]
        state, p = final_state(SMALL, events)
        words = program(quests=[{**SMALL['quests'][0], 'title': 'An axle, please'}])
        moved, report = rules.migration(p, words, state, run='r')
        self.assertEqual(moved['quests'], state['quests'])
        self.assertIn({'what': 'quest', 'key': 'axle', 'why': 'Its objectives are unchanged.'}, report['kept'])
        changed = program(quests=[{**SMALL['quests'][0], 'objectives': SMALL['quests'][0]['objectives'][:3]}])
        moved, report = rules.migration(p, changed, state, run='r')
        self.assertEqual((moved['quests']['axle'][ASH]['objectives'], moved['quests']['axle'][ASH]['status']), ({}, 'active'))
        self.assertEqual(report['reset'][0]['key'], 'axle')
        gone = program(quests=[{**SMALL['quests'][0], 'id': 'wheel'}])
        moved, report = rules.migration(p, gone, state, run='r')
        self.assertNotIn('axle', moved.get('quests', {}))
        self.assertEqual([x['key'] for x in report['dropped'] if x['what'] == 'quest'], ['axle'])
        self.assertEqual([x['key'] for x in report['added'] if x['what'] == 'quest'], ['wheel'])
        self.assertEqual(report['diff']['quests'], {'added': ['wheel'], 'removed': ['axle'], 'changed': []})
        self.assertEqual(moved['cohort'], state['cohort'], 'a move keeps the audience as it is')


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class QuestRuns(unittest.TestCase):
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    fact, launch, answer, tick, rewrite, row = (console.ConsoleReads.fact, console.ConsoleReads.launch, console.ConsoleReads.answer,
                                                console.ConsoleReads.tick, console.ConsoleReads.rewrite, console.ConsoleReads.row)
    one, send, taken, steer, answer_all, settle = (steering.Steering.one, steering.Steering.send, steering.Steering.taken,
                                                   steering.Steering.steer, steering.Steering.answer_all, steering.Steering.settle)

    def setUp(self):
        console.ConsoleReads.setUp(self)
        small = program()
        small['quests'][0]['join'] = {'cap': 3}
        campaign = program(CAMPAIGN)
        campaign['regions'] = []
        campaign['quests'] = [q for q in campaign['quests'] if q['id'] == 'ropes']
        campaign['phases'][0]['triggers'] = [t for t in campaign['phases'][0]['triggers'] if t['id'] != 'greet']
        campaign['tests'][0]['events'] = [e for e in campaign['tests'][0]['events'] if e['kind'] != 'arrival' and e.get('detail') != 'scout']
        campaign['tests'][0]['expectQuests'] = {'ropes': {'party:p1': 'done'}}
        self.examples = {**self.examples, 'carter-axle': small, 'ford-campaign': campaign}

    def orders(self, kind=None):
        """The orders sent, each order of a batch (doc 85, Phase 7: story.effects) as one of its own with the batch's id."""
        rows = self.conn.execute('SELECT id,kind,target_id,payload,status FROM dm.actions ORDER BY id').fetchall()
        out = []
        for r in rows:
            if r[1] == 'story.effects':
                out.extend({'id': r[0], 'kind': i['kind'], 'target': i['target'], 'payload': i['payload'], 'status': r[4], 'n': n}
                           for n, i in enumerate(r[3]['items']))
            else:
                out.append(dict(zip(('id', 'kind', 'target', 'payload', 'status'), r)))
        return [o for o in out if kind is None or o['kind'] == kind]

    def refuse(self, order, text):
        """The game refusing one order (in a batch: that item, the rest applied)."""
        self.conn.execute('SET LOCAL ROLE ratw_game')
        if 'n' in order:
            items = self.conn.execute('SELECT payload FROM dm.actions WHERE id=%s', (order['id'],)).fetchone()[0]['items']
            result = [{'ok': n != order['n'], 'text': text if n == order['n'] else 'ok'} for n in range(len(items))]
            self.conn.execute("UPDATE dm.actions SET status='applied',result=%s,done_at=now() WHERE id=%s", (json.dumps(result), order['id']))
        else:
            self.conn.execute("UPDATE dm.actions SET status='refused',result=%s,done_at=now() WHERE id=%s", (text, order['id']))
        self.conn.execute('SET LOCAL ROLE ratw_dm')

    def rows(self):
        return dict(self.conn.execute('SELECT player,state FROM dm.story_player_state WHERE run=%s', (self.run_id,)).fetchall())

    def blob(self):
        return self.conn.execute('SELECT state FROM dm.story_releases WHERE id=%s', (self.run_id,)).fetchone()[0]

    def test_a_small_quest_taken_done_handed_in(self):
        self.launch('carter-axle')
        phase = self.orders('story.phase')[-1]
        self.assertEqual(phase['payload']['players'], [], 'an open audience: nobody at first')
        self.settle(2)
        offers = self.orders('story.quests')[-1]['payload']
        self.assertEqual(offers['offers'][0]['quest'], 'axle')
        self.assertEqual((offers['offers'][0]['npc'], offers['offers'][0]['accept'], offers['offers'][0]['full']),
                         ('rm111', 'I will find you one.', False))
        self.fact('quest accepted', console.ASH if hasattr(console, 'ASH') else 'player-ash', self.run_id, item='rm111', detail='axle')
        self.fact('quest accepted', 'player-ash', 'another-run', item='rm111', detail='axle')     # another story's: not read
        self.tick()
        cohort = self.orders('story.cohort')
        self.assertEqual([o['payload']['add'] for o in cohort], [['player-ash']])
        journal = self.orders('story.quests')[-1]['payload']
        view = journal['wolves']['player-ash']['quests'][0]
        self.assertEqual((view['id'], view['status'], view['giver'], view['hiddenLeft']), ('axle', 'active', 'rm111', 1))
        self.assertNotIn('offers', journal, 'the offers did not change')
        # Each wolf's own state is in their row, not the release's JSON.
        self.assertEqual(self.rows()['player-ash']['quests']['axle']['status'], 'active')
        self.assertNotIn('quests', self.blob())
        self.assertEqual(self.one()['state']['quests']['axle']['player-ash']['status'], 'active', 'the console sees one state')
        self.settle(2)
        self.fact('conversation', 'player-ash', 'rm111')
        self.fact('gift', 'player-ash', 'rm111', item='axle', quantity=2)
        self.fact('quest handed in', 'player-ash', self.run_id, item='rm111', detail='axle')
        self.tick()
        for _ in range(6):
            if self.orders('character.grant'):
                break
            self.answer_all()
            self.tick()
        grants = self.orders('character.grant')
        self.assertEqual([(g['target'], g['payload']) for g in grants],
                         [('player-ash', {'coins': 12, 'purse': 'town:greyfen', 'reason': 'the carter thanks you'})])
        deeds = self.orders('deed.award')
        self.assertEqual(deeds[0]['target'], 'player-ash')
        # The purse was short: the game refused; the story notes it and carries on.
        self.refuse(grants[0], 'The purse is short (3p).')
        self.settle(3)
        one = self.one()
        self.assertEqual(one['status'], 'running')
        self.assertEqual(one['rewardsRefused'][0]['result'], 'The purse is short (3p).')
        notes = [n for item in runner.timeline(self.conn, self.world, self.run_id)['items'] for n in item.get('notes', [])]
        self.assertTrue(any('was not given' in n for n in notes), notes)
        self.assertEqual(one['quests'][0]['copies'][0]['status'], 'done')
        last = self.orders('story.quests')[-1]['payload']['wolves']['player-ash']['quests'][0]
        self.assertEqual((last['status'], last['again']), ('done', last['again']))
        self.assertIsNotNone(last['again'], 'daily: again tomorrow')

    def test_the_console_gives_completes_and_takes_wolves_in_and_out(self):
        self.launch('carter-axle')
        self.settle(2)
        status, result = self.steer('participant', {'player': 'player-birch', 'do': 'add'})
        self.assertEqual(status, 'done', result)
        self.settle(2)
        self.assertEqual(self.orders('story.cohort')[-1]['payload']['add'], ['player-birch'])
        status, result = self.steer('quest', {'quest': 'axle', 'player': 'player-birch', 'do': 'give'})
        self.assertEqual(status, 'done', result)
        self.settle(2)
        status, result = self.steer('objective', {'quest': 'axle', 'objective': 'bring', 'player': 'player-birch', 'do': 'progress', 'amount': 1})
        self.assertEqual(status, 'done', result)
        self.settle(1)   # (Doc 85, Phase 7: the journal's orders are waited for as every order is, before the next flow command.)
        status, result = self.steer('objective', {'quest': 'axle', 'objective': 'thief', 'player': 'player-birch', 'do': 'reveal'})
        self.assertEqual(status, 'done', result)
        self.settle(2)
        view = self.orders('story.quests')[-1]['payload']['wolves']['player-birch']['quests'][0]
        self.assertEqual(([o['id'] for o in view['objectives']], view['hiddenLeft']), (['ask', 'bring', 'wheel', 'thief'], 0))
        # The dry run: what completing would do (the rewards, as play gives them), nothing sent.
        before = len(self.orders())
        dry = runner.explain(self.conn, self.world, self.run_id, {'command': {'kind': 'quest', 'payload': {
            'quest': 'axle', 'player': 'player-birch', 'do': 'complete'}}}, 'martin')
        self.assertEqual(dry['outcome'], 'done', dry)
        self.assertEqual([e['kind'] for e in dry['next']], ['deed.award', 'character.grant'])
        self.assertEqual(len(self.orders()), before)
        status, result = self.steer('quest', {'quest': 'axle', 'player': 'player-birch', 'do': 'complete'})
        self.assertEqual(status, 'done', result)
        self.settle(4)
        self.assertEqual([o['target'] for o in self.orders('deed.award')], ['player-birch'])
        grid = self.one()['quests'][0]
        self.assertEqual([(c['holder'], c['status']) for c in grid['copies']], [('player-birch', 'done')])
        self.assertEqual(grid['counts']['done'], 1)
        status, result = self.steer('participant', {'player': 'player-birch', 'do': 'remove'})
        self.assertEqual(status, 'done', result)
        self.settle(2)
        self.assertEqual(self.orders('story.cohort')[-1]['payload']['remove'], ['player-birch'])
        self.assertEqual(self.orders('story.quests')[-1]['payload']['wolves']['player-birch'], {'quests': [], 'trackers': []})
        self.assertEqual(self.rows()['player-birch']['quests']['axle']['status'], 'done', 'what they did is kept')
        self.assertNotIn('player-birch', self.one()['state']['cohort'])
        # Refused: a quest the story has not, a wolf not in it.
        status, result = self.steer('quest', {'quest': 'axle', 'player': 'player-elm', 'do': 'complete'})
        self.assertEqual(status, 'refused')

    def test_a_party_quest_with_parties_told_to_the_runner(self):
        self.rewrite(f"""UPDATE game.checkpoints SET payload='{json.dumps({"calendarDays": 10, "parties": {"next": 2, "parties": [
            {"id": "p1", "leader": "player-ash", "members": [{"id": "player-ash"}, {"id": "player-birch"}]}]}})}' WHERE world_id='{self.world}'""")
        self.launch('ford-campaign')
        self.settle(2)
        self.assertEqual(self.one()['parties'], {'player-ash': 'p1', 'player-birch': 'p1'})
        self.fact('quest accepted', 'player-ash', self.run_id, item='rm112', detail='ropes')
        self.tick()
        self.settle(2)
        grid = self.one()['quests'][0]
        self.assertEqual([(c['holder'], c['members']) for c in grid['copies']], [('party:p1', ['player-ash', 'player-birch'])])
        journal = self.orders('story.quests')[-1]['payload']['wolves']
        self.assertEqual(set(journal), {'player-ash', 'player-birch'}, 'both see the party\'s copy')
        self.fact('party left', 'player-birch', 'p1')
        self.fact('gift', 'player-birch', 'rm112', item='rope', quantity=3)
        self.tick()
        self.settle(2)
        c = self.one()['quests'][0]['copies'][0]
        self.assertEqual((c['left'], c['objectives']), (['player-birch'], {}), 'no credit after leaving the party')
        self.assertNotIn('quests', self.rows().get('player-birch', {}), 'a party\'s copy is the run\'s, not a wolf\'s')
        self.assertIn('party:p1', self.blob()['quests']['ropes'])

    def test_moved_onto_a_new_version_and_back(self):
        self.launch('carter-axle')
        self.settle(2)
        self.fact('quest accepted', 'player-ash', self.run_id, item='rm111', detail='axle')
        self.fact('conversation', 'player-ash', 'rm111')
        self.tick()
        self.settle(2)
        new = program(quests=[{**SMALL['quests'][0], 'join': {'cap': 3}, 'objectives': SMALL['quests'][0]['objectives'][:3]}])
        report = runner.approve(self.conn, self.world, 'martin', new, rules.digest(new), for_run=self.run_id)
        self.assertEqual(report['report']['reset'][0]['key'], 'axle')
        version = runner.versions_of(self.conn, self.run_id)[0]['id']
        status, result = self.steer('migrate', {'version': version})
        self.assertEqual(status, 'done', result)
        self.assertEqual(self.rows()['player-ash']['quests']['axle']['objectives'], {}, 'reset in the wolf\'s row')
        # The game refuses: the run goes back, with the wolf's row as it was.
        move = self.orders('story.phase')[-1]
        self.conn.execute('SET LOCAL ROLE ratw_game')
        self.conn.execute("UPDATE dm.actions SET status='refused',result='no',done_at=now() WHERE id=%s", (move['id'],))
        self.conn.execute('SET LOCAL ROLE ratw_dm')
        self.tick()
        self.assertEqual(self.rows()['player-ash']['quests']['axle']['objectives'], {'ask': {'n': 1, 'done': True, 'shown': True}})
        self.assertEqual(self.one()['digest'], rules.digest(self.examples['carter-axle']))


if __name__ == '__main__':
    unittest.main()


class Phase1b(unittest.TestCase):
    """Doc 86, Phase 1b: a story choice kept for a list of wolves (J1), and the values a story tells residents of each wolf
    (N2: the `talk` map beside the journal's views)."""

    def run_of(self, p=None, state=None):
        p = rules.validate(program(p or SMALL))
        phase = rules.phases_of(p)[0]
        state = state if state is not None else {'phase': phase['id'], 'cohort': [ASH, BIRCH], 'counters': {}}
        return p, state, quests.Run(p, phase, state, 1, 0, lambda *a: None)

    def test_a_choice_offered_to_one_wolf_then_another_is_both_of_theirs(self):
        p, state, run = self.run_of()
        offer = {'kind': 'npc.offer', 'npc': 'rm111', 'id': 'pledge', 'words': 'I stand with you.', 'reply': 'Then stand.',
                 'player': '$actor', 'once': True}
        run.story_effect(offer, ASH)
        run.story_effect({**offer, 'reply': 'Stand, then.'}, BIRCH)
        [c] = quests.choices(state)
        self.assertEqual((c['players'], c['reply']), ([ASH, BIRCH], 'Stand, then.'), 'both see it; the words are the latest')
        run.game_event({'kind': 'dialogue choice', 'actor': ASH, 'detail': 'pledge', 'item': 'rm111'})
        run.story_effect(offer, ASH)
        [c] = quests.choices(state)
        self.assertEqual((c['players'], c['picked']), ([ASH, BIRCH], [ASH]), 'Ash picked it (once); Birch still sees it')
        run.story_effect({k: v for k, v in offer.items() if k != 'player'}, ASH)
        [c] = quests.choices(state)
        self.assertEqual((c['players'], c['picked']), (None, []), 'offered to every wolf: everyone\'s, as before')

    def test_an_older_state_reads_its_one_wolf_as_a_list(self):
        state = {'offers': {'old': {'npc': 'rm111', 'words': 'x', 'reply': 'y', 'player': ASH, 'once': False, 'picked': []}}}
        self.assertEqual(quests.choices(state)[0]['players'], [ASH])
        p, state2, run = self.run_of(state={'phase': 'open', 'cohort': [ASH, BIRCH], 'counters': {}, **state})
        run.story_effect({'kind': 'npc.offer', 'npc': 'rm111', 'id': 'old', 'words': 'x', 'reply': 'y', 'player': '$actor'}, BIRCH)
        self.assertEqual(quests.choices(state2)[0]['players'], [ASH, BIRCH])

    def test_the_views_carry_the_talk_map(self):
        base = copy.deepcopy(SMALL)
        base['variables'] = [*base.get('variables', []),
                             {'id': 'p_howl', 'type': 'text', 'scope': 'player', 'talk': True},
                             {'id': 'p_backs', 'type': 'number', 'scope': 'player', 'talk': True},
                             {'id': 'p_advice', 'type': 'switch', 'scope': 'player', 'talk': True},
                             {'id': 'p_quiet', 'type': 'number', 'scope': 'player'}]
        p = rules.validate(base)
        state = {'phase': p['start'], 'cohort': [ASH, BIRCH],
                 'counters': {'player': {ASH: {'p_howl': 'coldwater', 'p_backs': 3, 'p_advice': True, 'p_quiet': 9},
                                         BIRCH: {'p_howl': 'Cold Water'}}}}
        self.assertEqual(quests.journal(p, state, ASH)['talk'], {'p_howl': 'coldwater', 'p_backs': '3', 'p_advice': 'yes'})
        self.assertNotIn('talk', quests.journal(p, state, BIRCH), 'words no line could test are kept, not told')
        self.assertEqual(quests.talk_unsaid(p, state, BIRCH), ['p_howl'])
        self.assertNotIn('talk', quests.journal(p, state, ELM), 'only wolves with a stored value')
        self.assertTrue(quests.relevant(p))
        for bad, words in [({'id': 'x', 'type': 'wolf', 'scope': 'player', 'talk': True}, 'number, switch or text'),
                           ({'id': 'x', 'type': 'number', 'scope': 'story', 'talk': True}, 'number, switch or text'),
                           ({'id': 'x', 'type': 'text', 'scope': 'player', 'talk': True, 'initial': 'Two words'}, 'small letters')]:
            with self.subTest(words=words):
                q = copy.deepcopy(base)
                q['variables'].append(bad)
                self.assertRaisesRegex(ValueError, words, rules.validate, q)
        q = copy.deepcopy(base)
        q['variables'] += [{'id': f't{i}', 'type': 'number', 'scope': 'player', 'talk': True} for i in range(6)]
        self.assertRaisesRegex(ValueError, 'up to 8', rules.validate, q)
