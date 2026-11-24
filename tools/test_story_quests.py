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


# -------------------------------------------------------------------------- doc 86, Phase 2: what an objective counts
def one_quest(objective, scope='player', join=None, repeat='once', rewards=None, variables=None, regions=None, players=(ASH, BIRCH, ELM)):
    """A story of one quest a trigger starts for each wolf who arrives: the objective as given."""
    q = {'id': 'q', 'title': 'A thing to do', 'scope': scope, 'repeat': repeat, 'objectives': [{'id': 'o', 'label': 'Do it.', **objective}],
         **({'join': join} if join else {}), **({'rewards': rewards} if rewards else {})}
    p = {'format': 'ratw-story-program', 'version': 2, 'id': 'p2q', 'title': 'Phase 2', 'start': 'a',
         'audience': {'kind': 'players', 'players': list(players)}, 'quests': [q],
         **({'variables': variables} if variables else {}), **({'regions': regions} if regions else {}),
         'phases': [{'id': 'a', 'journal': 'A.', 'effects': [], 'exits': [], 'triggers': [
             {'id': 'start', 'on': 'arrival', 'audience': 'participant', 'actions': [
                 {'do': 'effect', 'effect': {'kind': 'quest.start', 'quest': 'q', 'player': '$actor'}}]}]}],
         'tests': [{'name': 'x', 'cohort': list(players), 'events': [], 'expect': ['a']}]}
    rules.validate(p)
    return p


def statuses(p, events, facts=None, **case):
    report, _ = play(p, events, cohort=p['audience']['players'], **({'facts': facts} if facts else {}), **case)
    return report['quests'].get('q', {}), report


arrive = lambda i, w, day=0.1: fact(i, 'arrival', day, w)


class Phase2(unittest.TestCase):
    def test_where_an_objective_counts(self):
        p = one_quest({'on': 'hunted', 'match': {'target': 'pheasant'}, 'where': {'cells': ['w_2_0', 'w_2_2']}, 'goal': 2})
        got, _ = statuses(p, [arrive(1, ASH), fact(2, 'hunted', 1, ASH, target='pheasant', cell='w_2_1'),
                              fact(3, 'hunted', 1.1, ASH, target='pheasant', cell='w_2_2'), fact(4, 'hunted', 1.2, ASH, target='pheasant')])
        self.assertEqual(got[ASH], 'active', 'one in 2_1 and one with no place do not count')
        got, _ = statuses(p, [arrive(1, ASH), fact(2, 'hunted', 1, ASH, target='pheasant', cell='w_2_0'),
                              fact(3, 'hunted', 1.1, ASH, target='pheasant', cell='w_2_2')])
        self.assertEqual(got[ASH], 'done')
        region = one_quest({'on': 'hunted', 'where': {'region': 'wood'}}, regions=[{'id': 'wood', 'cell': 'w_2_2', 'x1': 2, 'y1': 2, 'x2': 5, 'y2': 5}])
        self.assertEqual(statuses(region, [arrive(1, ASH), fact(2, 'hunted', 1, ASH, cell='w_2_2', x=9, y=9)])[0][ASH], 'active', 'outside the rectangle')
        self.assertEqual(statuses(region, [arrive(1, ASH), fact(2, 'hunted', 1, ASH, cell='w_2_2', x=3, y=4)])[0][ASH], 'done', 'inside')
        self.assertEqual(statuses(region, [arrive(1, ASH), fact(2, 'hunted', 1, ASH, cell='w_2_2')])[0][ASH], 'done', 'no tile: by its cell')
        for bad, words in [({'cells': []}, '1 to 16 cells'), ({'region': 'nowhere'}, 'region of this story'), ({'cells': ['a'], 'region': 'r'}, 'only in')]:
            with self.assertRaisesRegex(ValueError, words):
                one_quest({'on': 'hunted', 'where': bad})

    def test_who_an_objective_credits(self):
        sale = one_quest({'on': 'stall sale', 'credit': 'target', 'goal': 3})
        got, _ = statuses(sale, [arrive(1, ASH), arrive(2, BIRCH), *(fact(3 + i, 'stall sale', 1 + i / 10, BIRCH, target=ASH, item='bread') for i in range(3))])
        self.assertEqual((got[ASH], got[BIRCH]), ('done', 'active'), 'three things sold at Ash\'s stall: the keeper is credited')
        wagon = one_quest({'on': 'caravan arrives', 'credit': 'present'})
        facts = {'wolves': {ASH: {'cell': 'grey'}, BIRCH: {'cell': 'grey'}, ELM: {'cell': 'far'}}}
        got, _ = statuses(wagon, [arrive(1, ASH), arrive(2, BIRCH), arrive(3, ELM), fact(4, 'caravan arrives', 1, 'cv1', target='greyfen', cell='grey')], facts)
        self.assertEqual(got, {ASH: 'done', BIRCH: 'done', ELM: 'active'}, 'the two in the cell, not the third elsewhere')
        story = one_quest({'on': 'festival', 'credit': 'present', 'goal': 2}, scope='story')
        report, _ = play(story, [arrive(1, ASH), arrive(2, BIRCH), fact(3, 'festival', 1, 'greyfen', cell='grey', detail='Thaw fair')],
                         cohort=(ASH, BIRCH, ELM), facts=facts)
        self.assertEqual(report['quests']['q'], {'story': 'active'}, 'the story\'s one copy gains once from one festival, two wolves there')
        with self.assertRaisesRegex(ValueError, 'Nobody is the actor of a wagon arriving: credit the wolves present'):
            one_quest({'on': 'caravan arrives'})
        with self.assertRaisesRegex(ValueError, 'has a wolf it was done for'):
            one_quest({'on': 'hunted', 'credit': 'target'})
        with self.assertRaisesRegex(ValueError, 'an event with a place'):
            one_quest({'on': 'story signal', 'signal': 's', 'credit': 'present'})
        camp = one_quest({'on': 'camp cleared', 'match': {'target': 'camp_grey'}})
        self.assertEqual(statuses(camp, [arrive(1, ASH), fact(2, 'camp cleared', 1, ASH, target='camp_grey', cell='grey')])[0][ASH], 'done',
                         'a camp cleared by a wolf credits that wolf')
        self.assertEqual(statuses(camp, [arrive(1, ASH), fact(2, 'camp cleared', 1, 'greyfen', target='camp_grey', detail='by the watch')])[0][ASH],
                         'active', '"by the watch" credits nobody')

    def test_the_six_are_mission_events(self):
        for kind in ('camp cleared', 'caravan arrives', 'ship lost', 'ship docks', 'project built', 'festival'):
            self.assertIn(kind, rules.MISSION_EVENTS)
        trade = one_quest({'on': 'trade done', 'match': {'cell': 'city', 'detail': 'sold'}})
        self.assertEqual(statuses(trade, [arrive(1, ASH), fact(2, 'trade done', 1, ASH, target='rm1', cell='frost', detail='sold')])[0][ASH], 'active')
        self.assertEqual(statuses(trade, [arrive(1, ASH), fact(2, 'trade done', 1, ASH, target='rm1', cell='city', detail='sold')])[0][ASH], 'done',
                         'sold in the city (M5: trade done has its cell)')

    def test_a_goal_from_a_value(self):
        variables = [{'id': 'ch_goal', 'type': 'number', 'scope': 'story', 'initial': 3}]
        live = one_quest({'on': 'gift', 'goal': {'var': 'ch_goal'}, 'goalLive': True}, scope='story', variables=variables, players=(ASH,))
        live['phases'][0]['triggers'].append({'id': 'raise', 'on': 'conversation', 'actions': [{'do': 'set', 'var': 'ch_goal', 'value': 5}]})
        live['phases'][0]['triggers'].append({'id': 'lower', 'on': 'dawn', 'actions': [{'do': 'set', 'var': 'ch_goal', 'value': 1}]})
        gifts = [fact(10 + i, 'gift', 1 + i / 10, ASH, target='rm111', item='rope') for i in range(2)]
        state, _ = final_state(live, [arrive(1, ASH), fact(2, 'conversation', 0.5, ASH, target='rm111'), *gifts], cohort=(ASH,))
        c = state['quests']['q']['story']
        self.assertEqual((c['status'], c['objectives']['o']['n'], c['objectives']['o']['goal']), ('active', 2, 5), 'the goal rose to 5 mid-run')
        state, _ = final_state(live, [arrive(1, ASH), *gifts, fact(5, 'dawn', 2)], cohort=(ASH,))
        self.assertEqual(state['quests']['q']['story']['status'], 'done', 'a goal below what is made completes at once')
        kept = one_quest({'on': 'gift', 'goal': {'max': [2, {'times': [{'audience': 'active'}, 1]}]}}, variables=variables)
        state, _ = final_state(kept, [arrive(1, ASH), arrive(2, BIRCH), arrive(3, ELM)], cohort=(ASH, BIRCH, ELM))
        self.assertEqual(state['quests']['q'][ELM]['objectives']['o']['goal'], 3, 'read when the copy starts: three active wolves')
        self.assertEqual(state['quests']['q'][ASH]['objectives']['o']['goal'], 2, 'Ash started when only Ash was active')
        views = quests.views(kept, state, ASH)
        self.assertEqual(views[0]['objectives'][0]['goal'], 2)
        with self.assertRaisesRegex(ValueError, 'for a quest of the whole story'):
            one_quest({'on': 'gift', 'goal': {'var': 'ch_goal'}, 'goalLive': True}, variables=variables)
        with self.assertRaisesRegex(ValueError, 'only when the goal is a value'):
            one_quest({'on': 'gift', 'goalLive': True}, scope='story')
        with self.assertRaisesRegex(ValueError, 'reads no event field'):
            one_quest({'on': 'gift', 'goal': {'field': 'quantity'}}, variables=variables)

    def test_again_after_any_end(self):
        p = one_quest({'on': 'gift'}, join={'again': 7}, repeat='daily', players=(ASH,))
        p['quests'][0]['giver'] = 'rm111'
        p['quests'][0]['handIn'] = False
        accept = lambda i, day: fact(i, 'quest accepted', day, ASH, target='run', item='rm111', detail='q')
        abandon = lambda i, day: fact(i, 'quest abandoned', day, ASH, target='run', detail='q')
        state, _ = final_state(p, [accept(1, 1), abandon(2, 1.5), accept(3, 3)], cohort=(ASH,))
        self.assertEqual(state['quests']['q'][ASH]['status'], 'abandoned', 'a wolf who abandons waits 7 days')
        self.assertEqual(state['toasts'][ASH], 'A thing to do: Not again so soon.')
        state, _ = final_state(p, [accept(1, 1), abandon(2, 1.5), accept(3, 8.6)], cohort=(ASH,))
        self.assertEqual(state['quests']['q'][ASH]['status'], 'active', 'and may take it after')
        three = one_quest({'on': 'gift'}, join={'again': 3}, repeat='daily', players=(ASH,))
        three['quests'][0].update(giver='rm111', handIn=False)
        gift = lambda i, day: fact(i, 'gift', day, ASH, target='rm111', item='rope')
        state, _ = final_state(three, [accept(1, 1), gift(2, 1.2), accept(3, 2.5)], cohort=(ASH,))
        c = state['quests']['q'][ASH]
        self.assertEqual((c['status'], quests.again(three['quests'][0], c)), ('done', 4.2), 'daily with again 3: three days')
        self.assertEqual(quests.views(three, state, ASH)[0]['again'], 4.2)
        with self.assertRaisesRegex(ValueError, 'again after 1 to 365'):
            one_quest({'on': 'gift'}, join={'again': 0})

    def test_a_reward_that_follows_the_purse_and_words_filled_for_each_wolf(self):
        variables = [{'id': 'p_town', 'type': 'text', 'scope': 'player'}, {'id': 'p_n', 'type': 'number', 'scope': 'player', 'initial': 4}]
        p = one_quest({'on': 'gift', 'label': 'Give it, {p_n.of} times.'}, variables=variables, players=(ASH,),
                      rewards=[{'kind': 'character.grant', 'player': '$actor', 'purse': 'town:greyfen', 'reason': 'r',
                                'coins': {'min': [12, {'div': [{'purse': 'town:greyfen'}, 20]}]}}])
        p['quests'][0]['summary'] = 'For {p_town.of}.'
        p['phases'][0]['triggers'][0]['actions'].insert(0, {'do': 'set', 'var': 'p_town', 'value': 'greyfen'})
        report, _ = play(p, [arrive(1, ASH), fact(2, 'gift', 1, ASH, target='rm111')], cohort=(ASH,), facts={'purses': {'town:greyfen': 212}})
        self.assertEqual([e['coins'] for e in report['effects'] if e['kind'] == 'character.grant'], [10], 'min(12, 212 / 20) at 212')
        report, _ = play(p, [arrive(1, ASH), fact(2, 'gift', 1, ASH, target='rm111')], cohort=(ASH,), facts={'purses': {}})
        self.assertEqual([e for e in report['effects'] if e['kind'] == 'character.grant'], [], 'an unknown purse: not given')
        state, _ = final_state(p, [arrive(1, ASH), fact(2, 'gift', 1, ASH, target='rm111')], cohort=(ASH,))
        self.assertTrue(state['rewardNotes'] and state['rewardNotes'][0]['quest'] == 'q' and 'coins has no value' in state['rewardNotes'][0]['why'])
        v = quests.views(p, state, ASH)[0]
        self.assertEqual((v['summary'], v['objectives'][0]['label']), ('For greyfen.', 'Give it, 4 times.'), 'filled for the wolf who reads them')
        p['quests'][0]['offer'] = {'say': 'Help {p_town.of}.'}
        p['quests'][0]['giver'] = 'rm111'
        with self.assertRaisesRegex(ValueError, 'the same for every wolf'):
            rules.validate(p)

    def test_a_goal_not_known_when_the_copy_starts_is_read_later(self):
        # (Review of Phase 2: a goal from a purse, the copy started with no facts, was stuck at 1,000,000 for good.)
        p = one_quest({'on': 'gift', 'goal': {'max': [1, {'div': [{'purse': 'town:greyfen'}, 100]}]}}, players=(ASH,))
        q, state = p['quests'][0], rules.begin(p, [ASH], 0)
        phase, pushed = rules.phase_of(p, state), []
        quests.Run(p, phase, state, 1, 0, pushed.append).start(q, ASH)
        self.assertEqual(state['quests']['q'][ASH]['objectives']['o'], {'goal': None}, 'no purse known: not read')
        facts = {'purses': {'town:greyfen': 200}}
        self.assertEqual(quests.views(p, state, ASH, facts)[0]['objectives'][0]['goal'], 2, 'the journal reads it now')
        self.assertEqual(quests.board(p, state, facts)[0]['objectives'][0]['goal'], 2, 'so does the console\'s board')
        unknown = quests.board(p, state)[0]['objectives'][0]
        self.assertEqual((unknown['goal'], unknown['goalFrom']), (None, q['objectives'][0]['goal']), 'not known: no number, the value')
        self.assertNotEqual(quests.fingerprint(p, state, facts), quests.fingerprint(p, state, {'purses': {'town:greyfen': 400}}),
                            'the journal is sent again when the purse it reads changes')
        gift = lambda i: {**fact(i, 'gift', 1 + i / 10, ASH, target='rm111', item='rope'), 'facts': facts}
        quests.Run(p, phase, state, 1, 0, pushed.append).track(gift(2))
        rec = state['quests']['q'][ASH]['objectives']['o']
        self.assertEqual((rec['goal'], rec['n'], state['quests']['q'][ASH]['status']), (2, 1, 'active'), 'read as it is credited')
        quests.Run(p, phase, state, 1, 0, pushed.append).track(gift(3))
        self.assertEqual(state['quests']['q'][ASH]['status'], 'done')
        # Credited while still not known, then read after a later fact (keep_up): done at once when already reached.
        state = rules.begin(p, [ASH], 0)
        quests.Run(p, phase, state, 1, 0, pushed.append).start(q, ASH)
        quests.Run(p, phase, state, 1, 0, pushed.append).track(fact(2, 'gift', 1, ASH, target='rm111', item='rope'))
        self.assertEqual(state['quests']['q'][ASH]['objectives']['o'], {'goal': None, 'n': 1})
        quests.keep_up(p, phase, state, 1, 0, pushed.append, {'kind': 'dawn', 'actor': '', 'facts': {'purses': {'town:greyfen': 50}}})
        self.assertEqual(state['quests']['q'][ASH]['status'], 'done', 'a goal of 1 already reached')
        self.assertEqual(state['quests']['q'][ASH]['objectives']['o']['goal'], 1)
        # The rehearsal: the same quest with the purse known throughout completes as before.
        got, _ = statuses(p, [arrive(1, ASH), fact(2, 'gift', 1, ASH, target='rm111', item='rope'),
                              fact(3, 'gift', 1.1, ASH, target='rm111', item='rope')], facts)
        self.assertEqual(got[ASH], 'done')

    def test_words_the_same_for_every_wolf_take_no_wolfs_own_value(self):
        variables = [{'id': 'p_n', 'type': 'number', 'scope': 'player', 'initial': 4}]
        p = one_quest({'on': 'gift'}, variables=variables, players=(ASH,))
        p['quests'][0]['summary'] = 'You have given {p_n.of} gifts so far.'
        rules.validate(p)   # (Shown only in each wolf's own journal: filled for them.)
        for key, value in (('giver', 'rm111'), ('board', 'greyfen')):
            offered = copy.deepcopy(p)
            offered['quests'][0][key] = value
            with self.subTest(key), self.assertRaisesRegex(ValueError, 'its summary: the same for every wolf'):
                rules.validate(offered)
        # A reward given once (no wolf named) fills no wolf's own value; one for each credited wolf does.
        with self.assertRaisesRegex(ValueError, 'the same for every wolf'):
            one_quest({'on': 'gift'}, variables=variables, players=(ASH,),
                      rewards=[{'kind': 'board.post', 'town': 'greyfen', 'days': 3, 'text': 'Gifts: {p_n.of}.'}])
        each = one_quest({'on': 'gift'}, variables=variables, players=(ASH,),
                         rewards=[{'kind': 'story.tell', 'to': 'wolf', 'player': '$actor', 'text': 'Gifts: {p_n.of}.'}])
        report, _ = play(each, [arrive(1, ASH), fact(2, 'gift', 1, ASH, target='rm111')], cohort=(ASH,))
        self.assertEqual([e['text'] for e in report['effects'] if e['kind'] == 'story.tell'], ['Gifts: 4.'])


class Phase3Shares(unittest.TestCase):
    """Doc 86, C3b: a quest of the story (or a party) rewards each wolf by their share of what was made."""

    def shared(self, credit_min=None, scope='story'):
        coins = {'min': [20, {'times': [{'share': True}, 2]}]}
        p = one_quest({'on': 'gift', 'goal': 9, 'measure': 'quantity'}, scope=scope)
        p['quests'][0]['rewards'] = [{'kind': 'character.grant', 'player': '$actor', 'purse': 'town:greyfen', 'reason': 'r', 'coins': coins},
                                     {'kind': 'deed.award', 'player': '$actor', 'detail': 'Carried the lamp oil.'}]
        p['quests'][0]['credit'] = 'share'
        if credit_min:
            p['quests'][0]['creditMin'] = credit_min
        rules.validate(p)
        return p

    def gifts(self):
        return [arrive(1, ASH), arrive(2, BIRCH), arrive(3, ELM), fact(4, 'gift', 1, ASH, target='rm111', quantity=5),
                fact(5, 'gift', 1.1, BIRCH, target='rm111', quantity=3), fact(6, 'gift', 1.2, ELM, target='rm111', quantity=1)]

    def test_three_wolves_paid_by_their_shares(self):
        p = self.shared()
        report, _ = play(p, self.gifts(), cohort=(ASH, BIRCH, ELM))
        paid = {e['player']: e['coins'] for e in report['effects'] if e['kind'] == 'character.grant'}
        self.assertEqual(paid, {ASH: 10, BIRCH: 6, ELM: 2})
        state, _ = final_state(p, self.gifts(), cohort=(ASH, BIRCH, ELM))
        self.assertEqual(state['quests']['q']['story']['shares'], {ASH: 5, BIRCH: 3, ELM: 1})

    def test_the_least_share_leaves_a_wolf_out(self):
        report, _ = play(self.shared(credit_min=2), self.gifts(), cohort=(ASH, BIRCH, ELM))
        paid = {e['player']: e['coins'] for e in report['effects'] if e['kind'] == 'character.grant'}
        self.assertEqual(paid, {ASH: 10, BIRCH: 6})
        self.assertEqual(sorted(e['player'] for e in report['effects'] if e['kind'] == 'deed.award'), sorted([ASH, BIRCH]),
                         'every reward leaves them out')

    def test_a_wolf_who_added_nothing_gets_nothing(self):
        # (No least share named: a share of 0 is no reward, not the least coin and every flat reward. Review of Phase 3.)
        events = [arrive(1, ASH), arrive(2, BIRCH), fact(4, 'gift', 1, ASH, target='rm111', quantity=9)]
        report, _ = play(self.shared(), events, cohort=(ASH, BIRCH, ELM))
        self.assertEqual({e['player']: e['coins'] for e in report['effects'] if e['kind'] == 'character.grant'}, {ASH: 18})
        self.assertEqual([e['player'] for e in report['effects'] if e['kind'] == 'deed.award'], [ASH])

    def test_a_rewards_greeting_is_from_the_giver(self):
        # (C5: a reward's greeting is from the quest's giver unless it names its resident.)
        greet = {'kind': 'npc.greet', 'player': '$actor', 'text': 'You kept the lamps lit.', 'days': 7}
        p = one_quest({'on': 'gift'}, rewards=[{**greet, 'npc': 'rm222', 'text': 'And I heard of it.'}])
        p['quests'][0].update(giver='rm111', handIn=False)
        p['quests'][0]['rewards'].insert(0, greet)
        rules.validate(p)
        report, _ = play(p, [arrive(1, ASH), fact(2, 'gift', 1, ASH, target='rm111')], cohort=(ASH, BIRCH, ELM))
        self.assertEqual([(e['npc'], e['player']) for e in report['effects'] if e['kind'] == 'npc.greet'], [('rm111', ASH), ('rm222', ASH)])
        with self.assertRaisesRegex(ValueError, 'a greeting as a reward is from the giver'):
            one_quest({'on': 'gift'}, rewards=[greet])

    def test_the_checks(self):
        p = self.shared()
        p['quests'][0]['scope'] = 'player'
        self.assertRaisesRegex(ValueError, 'whole story or a party', rules.validate, p)
        p = self.shared()
        p['quests'][0]['credit'] = 'most'
        self.assertRaisesRegex(ValueError, 'credit is "share"', rules.validate, p)
        p = self.shared()
        del p['quests'][0]['credit']
        self.assertRaisesRegex(ValueError, 'only in the rewards of a quest that rewards each wolf by their share', rules.validate, p)
        p = self.shared()
        p['quests'][0]['creditMin'] = 0
        self.assertRaisesRegex(ValueError, 'least share', rules.validate, p)
        # (A share is a reward's: never a trigger's value.)
        p = self.shared()
        p['phases'][0]['triggers'][0]['actions'].append(
            {'do': 'effect', 'effect': {'kind': 'item.give', 'player': '$actor', 'item': 'bread', 'quantity': {'share': True}}})
        self.assertRaisesRegex(ValueError, 'only in the rewards', rules.validate, p)
        self.shared(scope='party')

    def test_a_trigger_s_progress_counts_toward_the_share(self):
        p = self.shared()
        p['phases'][0]['triggers'].append({'id': 'help', 'on': 'deed', 'audience': 'participant', 'actions': [
            {'do': 'effect', 'effect': {'kind': 'objective.progress', 'quest': 'q', 'objective': 'o', 'player': '$actor', 'amount': 4}}]})
        rules.validate(p)
        events = [arrive(1, ASH), arrive(2, BIRCH), fact(3, 'gift', 1, ASH, target='rm111', quantity=5), fact(4, 'deed', 1.1, BIRCH)]
        state, _ = final_state(p, events, cohort=(ASH, BIRCH, ELM))
        self.assertEqual(state['quests']['q']['story']['shares'], {ASH: 5, BIRCH: 4})


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


if __name__ == '__main__':
    unittest.main()


def chain(closure=True, scope='player', during=True):
    """Doc 86, Phase 4: a line of two quests at Ivo's: the second only for the wolves who did the first (C6), and it closes the
    line (J3)."""
    two = {'id': 'two', 'title': 'The oil', 'scope': scope, 'giver': 'rm111', 'handIn': False,
           'offer': {'say': 'Now the oil for it.', **({'during': 'Mind you keep the oil dry.', 'duringAsk': 'About the oil...'} if during else {})},
           'requires': {'test': 'quest is', 'quest': 'one', 'status': 'done'},
           'objectives': [{'id': 'o', 'label': 'Bring the oil.', 'on': 'gift', 'match': {'target': 'rm222'}}]}
    if closure is not None:
        two['closure'] = closure
    p = {'format': 'ratw-story-program', 'version': 2, 'id': 'the-lamp', 'title': 'The lamp', 'start': 'a',
         'audience': {'kind': 'players', 'players': [ASH, BIRCH, ELM]},
         'variables': [{'id': 'open', 'type': 'switch', 'scope': 'story'}],
         'quests': [{'id': 'one', 'title': 'The lamp', 'scope': scope, 'giver': 'rm111', 'handIn': False, 'offer': {'say': 'Find my lamp.'},
                     'objectives': [{'id': 'o', 'label': 'Find the lamp.', 'on': 'gift', 'match': {'target': 'rm111'}}]}, two],
         'phases': [{'id': 'a', 'journal': 'A.', 'effects': [], 'exits': [], 'triggers': [
             {'id': 'open', 'on': 'arrival', 'audience': 'participant', 'actions': [{'do': 'turn on', 'var': 'open'}]}]}],
         'tests': [{'name': 'x', 'cohort': [ASH, BIRCH, ELM], 'events': [], 'expect': ['a']}]}
    rules.validate(p)
    return p


class Phase4Quests(unittest.TestCase):
    """Doc 86, Phase 4: C6 `requires` and `offer.during`, L5 `quest.withdraw`, J3 closure quests, C12a's notices."""

    def state_after(self, p, events):
        state, _ = final_state(p, events, cohort=(ASH, BIRCH, ELM))
        return state

    def test_a_chain_quest_is_offered_only_to_the_wolf_who_finished_part_one(self):
        p = chain()
        state = self.state_after(p, [fact(1, 'quest accepted', 0.1, ASH, item='rm111', detail='one'),
                                     fact(2, 'gift', 0.2, ASH, target='rm111')])
        self.assertEqual(state['quests']['one'][ASH]['status'], 'done')
        offers = {o['quest']: o for o in quests.offers(p, state)}
        self.assertNotIn('players', offers['one'], 'part one is for everyone')
        self.assertEqual(offers['two']['players'], [ASH], 'part two only for Ash')
        self.assertEqual((offers['two']['during'], offers['two']['duringAsk']), ('Mind you keep the oil dry.', 'About the oil...'))
        # Birch takes part two at Ivo's anyway (a stale window): not yet.
        report, _ = play(p, [fact(1, 'quest accepted', 0.1, BIRCH, item='rm111', detail='two')], cohort=(ASH, BIRCH, ELM))
        self.assertEqual(report['quests'].get('two', {}), {})
        state = self.state_after(p, [fact(1, 'quest accepted', 0.1, BIRCH, item='rm111', detail='two')])
        self.assertEqual(state.get('toasts', {}).get(BIRCH), 'The oil: Not yet.')
        # Ash takes it: under way, the giver's words while under way go with the offer.
        state = self.state_after(p, [fact(1, 'quest accepted', 0.1, ASH, item='rm111', detail='one'), fact(2, 'gift', 0.2, ASH, target='rm111'),
                                     fact(3, 'quest accepted', 0.3, ASH, item='rm111', detail='two')])
        self.assertEqual(state['quests']['two'][ASH]['status'], 'active')
        self.assertEqual(quests.journal(p, state, ASH)['quests'][1]['status'], 'active')

    def test_a_gate_for_everyone_is_one_offer_or_none(self):
        p = chain(closure=None)
        p['quests'][0]['requires'] = {'test': 'switch', 'var': 'open', 'is': True}
        rules.validate(p)
        self.assertFalse(quests.requires_personal(p['quests'][0], p))
        self.assertTrue(quests.requires_personal(p['quests'][1], p))
        state = rules.begin(p, [ASH, BIRCH, ELM], 0)
        self.assertEqual([(o['quest'], o.get('players')) for o in quests.offers(p, state)], [('one', []), ('two', [])],
                         'part one closed: for nobody (kept for the copies under way); part two for nobody yet')
        before = quests.fingerprint(p, state)
        state = self.state_after(p, [fact(1, 'arrival', 0.1, ASH)])
        offers = quests.offers(p, state)
        self.assertEqual([(o['quest'], 'players' in o) for o in offers], [('one', False), ('two', True)])
        self.assertNotEqual(quests.fingerprint(p, state), before, 'the gate is in the fingerprint')

    def test_withdrawn_from_one_wolf_still_offered_to_another(self):
        p = chain(closure=None)
        state = rules.begin(p, [ASH, BIRCH, ELM], 0)
        run = quests.Run(p, p['phases'][0], state, 1, 0, lambda f: None)
        run.story_effect({'kind': 'quest.offer', 'quest': 'one', 'player': ASH}, ASH)
        run.story_effect({'kind': 'quest.offer', 'quest': 'one', 'player': BIRCH}, BIRCH)
        run.story_effect({'kind': 'quest.withdraw', 'quest': 'one', 'player': ASH}, ASH)
        self.assertEqual(state['questOffers'], {'one': [BIRCH]})
        self.assertEqual(state['withdrawn'], {'one': [ASH]})
        self.assertEqual([v['status'] for v in quests.views(p, state, ASH)], [], 'no mark in the journal')
        offer = next(o for o in quests.offers(p, state) if o['quest'] == 'one')
        self.assertEqual(offer['except'], [ASH])
        self.assertEqual(run.may_start(p['quests'][0], ASH), 'That is not on offer just now.')
        self.assertEqual(run.may_start(p['quests'][0], BIRCH), '')
        run.story_effect({'kind': 'quest.offer', 'quest': 'one', 'player': ASH}, ASH)
        self.assertNotIn('withdrawn', state, 'offered again clears it')
        # A copy already taken is untouched.
        run.start(p['quests'][0], ELM)
        run.story_effect({'kind': 'quest.withdraw', 'quest': 'one', 'player': ELM}, ELM)
        self.assertEqual(state['quests']['one'][ELM]['status'], 'active')

    def test_a_closure_quest_counts_once_for_each_wolf(self):
        p = chain()
        events = [fact(1, 'quest accepted', 0.1, ASH, item='rm111', detail='one'), fact(2, 'gift', 0.2, ASH, target='rm111'),
                  fact(3, 'quest accepted', 0.3, ASH, item='rm111', detail='two'), fact(4, 'gift', 0.4, ASH, target='rm222'),
                  fact(5, 'gift', 0.5, ASH, target='rm222')]
        report, _ = play(p, events, cohort=(ASH, BIRCH, ELM))
        closures = [{k: v for k, v in e.items() if k != 'phase'} for e in report['effects'] if e['kind'] == 'story.closure']
        self.assertEqual(closures, [{'kind': 'story.closure', 'player': ASH, 'story': 'the-lamp', 'quest': 'two', 'closure': 'line',
                                     'title': 'The oil', 'reward': 'two'}], '"true" is read as a line; once, the quest taken once')
        order = runner.order_of(None, 'w', 'run-1', 'martin', {'actor': ASH, 'visits': 1}, closures[0], p['phases'][0])
        self.assertEqual(order, ('story.closure', ASH, {'run': 'run-1', 'story': 'the-lamp', 'quest': 'two', 'kind': 'line', 'title': 'The oil'}))
        # A party's copy: each member one.
        p = chain(scope='party')
        events = [fact(1, 'party joined', 0.05, ASH, target='p1'), fact(2, 'party joined', 0.06, BIRCH, target='p1'),
                  fact(3, 'quest accepted', 0.1, ASH, item='rm111', detail='one'), fact(4, 'gift', 0.2, ASH, target='rm111'),
                  fact(5, 'quest accepted', 0.3, ASH, item='rm111', detail='two'), fact(6, 'gift', 0.4, BIRCH, target='rm222')]
        report, _ = play(p, events, cohort=(ASH, BIRCH, ELM))
        self.assertEqual(sorted(e['player'] for e in report['effects'] if e['kind'] == 'story.closure'), [ASH, BIRCH])

    def test_the_closure_checks(self):
        p = chain()
        p['quests'][1]['closure'] = 'side'
        rules.validate(p)   # (A side story: approval refuses it outside a campaign chapter; test_story_campaigns.)
        p = chain()
        del p['quests'][1]['requires']
        self.assertRaisesRegex(ValueError, 'requires another quest of the story done', rules.validate, p)
        p = chain()
        p['quests'][0]['closure'] = 'line'
        self.assertRaisesRegex(ValueError, 'requires another quest', rules.validate, p)
        p = chain()
        p['quests'].append({**copy.deepcopy(p['quests'][1]), 'id': 'three', 'requires': {'test': 'quest is', 'quest': 'two', 'status': 'done'}})
        self.assertRaisesRegex(ValueError, 'is required by no other quest: three', rules.validate, p)
        p = chain()
        p['quests'][1]['repeat'] = 'daily'
        self.assertRaisesRegex(ValueError, 'taken once', rules.validate, p)
        p = chain()
        p['quests'][1]['scope'] = 'story'
        p['quests'][1].pop('requires')
        p['quests'][1]['closure'] = 'side'
        self.assertRaisesRegex(ValueError, "a wolf's or a party's", rules.validate, p)
        p = chain()
        p['quests'][1]['closure'] = 'yes'
        self.assertRaisesRegex(ValueError, 'closes a quest line', rules.validate, p)
        p = chain()
        for n in range(4):
            p['quests'].append({**copy.deepcopy(p['quests'][1]), 'id': f'side{n}', 'closure': 'side'})
        self.assertRaisesRegex(ValueError, 'At most 4 quests', rules.validate, p)

    def test_the_requires_and_during_checks(self):
        p = chain()
        p['quests'][1]['requires'] = {'test': 'wolf in town', 'town': 'greyfen'}
        self.assertRaisesRegex(ValueError, "not the world's facts", rules.validate, p)
        p = chain()
        p['quests'][1]['requires'] = {'test': 'compare', 'left': {'field': 'quantity'}, 'op': '>', 'right': 1}
        self.assertRaisesRegex(ValueError, 'no event field', rules.validate, p)
        p = chain()
        p['quests'][1]['requires'] = {'test': 'quest is', 'quest': 'nope', 'status': 'done'}
        self.assertRaisesRegex(ValueError, 'Unknown quest', rules.validate, p)
        p = chain()
        p['quests'][1]['offer'].pop('during')
        self.assertRaisesRegex(ValueError, 'needs what they say', rules.validate, p)
        p = chain(closure=None)
        p['quests'][1]['requires'] = {'test': 'all', 'conditions': [{'test': 'quest is', 'quest': 'one', 'status': 'done'},
                                                                    {'test': 'not', 'condition': {'test': 'switch', 'var': 'open', 'is': False}}]}
        rules.validate(p)

    def test_news_for_another_story_is_queued(self):
        p = chain(closure=None)
        state = rules.begin(p, [ASH, BIRCH, ELM], 0)
        run = quests.Run(p, p['phases'][0], state, 1, 0, lambda f: None)
        run.story_effect({'kind': 'story.notify', 'signal': 'lamp_lit', 'player': ASH, 'amount': 3}, ASH)
        run.story_effect({'kind': 'story.notify', 'signal': 'lamp_lit', 'story': 'nh-ch1'}, '')
        self.assertEqual(state['notices'], [{'signal': 'lamp_lit', 'player': ASH, 'amount': 3},
                                            {'signal': 'lamp_lit', 'player': '', 'amount': 0, 'story': 'nh-ch1'}])


class Phase4AfterReview(unittest.TestCase):
    """Doc 86, Phase 4 after review: a closed gate keeps the words of the copies under way; the gates worked out again for a
    new version; quest.withdraw's quest the program's own; no world's facts in `requires`, the wolf as {"field": "actor"};
    an exit on "variable changed" checked; a line's closure needs a quest done, not one not done."""

    def test_a_closed_gate_keeps_the_words_of_a_copy_under_way(self):
        p = chain(closure=None)
        p['quests'][1]['requires'] = {'test': 'switch', 'var': 'open', 'is': False}
        p['quests'][1]['offer']['thanks'] = 'Dry as a bone.'
        rules.validate(p)
        state = rules.begin(p, [ASH, BIRCH, ELM], 0)
        run = quests.Run(p, p['phases'][0], state, 1, 0, lambda f: None)
        run.start(p['quests'][1], ASH)
        state.setdefault('counters', {}).setdefault('story', {})['open'] = True
        two = next(o for o in quests.offers(p, state) if o['quest'] == 'two')
        self.assertEqual(two['players'], [], 'no one new may take it')
        self.assertEqual((two['during'], two['duringAsk'], two['thanks']), ('Mind you keep the oil dry.', 'About the oil...', 'Dry as a bone.'))
        self.assertEqual(run.may_start(p['quests'][1], BIRCH), quests.NOT_YET)

    def test_the_gates_are_worked_out_again_for_a_new_version(self):
        a = chain(closure=None)
        a['quests'][0]['requires'] = {'test': 'switch', 'var': 'open', 'is': True}
        rules.validate(a)
        b = copy.deepcopy(a)
        b['quests'][0]['requires']['is'] = False
        rules.validate(b)
        state = rules.begin(a, [ASH], 0)
        shut = lambda p: [o['quest'] for o in quests.offers(p, state) if o.get('players') == []]
        self.assertEqual(shut(a), ['one', 'two'])
        self.assertEqual(shut(b), ['two'], 'the same ids and state, other requires: worked out again')
        self.assertNotEqual(quests.fingerprint(a, state), quests.fingerprint(b, state))
        self.assertTrue(all(len(k) == 40 for k in quests._GATES), 'only a digest is kept')

    def test_quest_withdraw_names_a_quest_of_the_story(self):
        p = chain(closure=None)
        p['phases'][0]['triggers'][0]['actions'].append({'do': 'effect', 'effect': {'kind': 'quest.withdraw', 'quest': 'nope', 'player': '$actor'}})
        self.assertRaisesRegex(ValueError, 'Unknown quest "nope"', rules.validate, p)

    def test_requires_reads_no_purse_and_not_who_is_online(self):
        for value in ({'purse': 'town:greyfen:works'}, {'audience': 'online'}, {'percent': {'list': 'open'}, 'of': 'online'}):
            p = chain(closure=None)
            p['quests'][0]['requires'] = {'test': 'compare', 'left': value, 'op': '>=', 'right': 0}
            self.assertRaisesRegex(ValueError, "reads no purse and not who is online", rules.validate, p)
        p = chain(closure=None)
        p['quests'][0]['requires'] = {'test': 'compare', 'left': {'audience': 'active'}, 'op': '>=', 'right': 0}
        rules.validate(p)

    def test_only_for_the_wolves_in_a_list(self):
        p = chain(closure=None)
        p['variables'].append({'id': 'pledged', 'type': 'wolves', 'scope': 'story'})
        p['quests'][0]['requires'] = {'test': 'in list', 'list': 'pledged', 'value': {'field': 'actor'}}
        rules.validate(p)
        self.assertTrue(quests.requires_personal(p['quests'][0], p), 'the wolf tested: worked out for each wolf')
        state = rules.begin(p, [ASH, BIRCH], 0)
        state.setdefault('counters', {}).setdefault('story', {})['pledged'] = [BIRCH]
        self.assertEqual(next(o for o in quests.offers(p, state) if o['quest'] == 'one')['players'], [BIRCH])
        run = quests.Run(p, p['phases'][0], state, 1, 0, lambda f: None)
        self.assertEqual(run.may_start(p['quests'][0], ASH), quests.NOT_YET)
        self.assertEqual(run.may_start(p['quests'][0], BIRCH), '')
        p['quests'][0]['requires']['value'] = {'field': 'target'}
        self.assertRaisesRegex(ValueError, 'no event field', rules.validate, p)

    def test_an_exit_on_variable_changed_names_a_world_variable(self):
        p = chain(closure=None)
        p['phases'][0]['exits'].append({'on': 'variable changed', 'match': {'target': 'nope'}, 'to': 'a'})
        self.assertRaisesRegex(ValueError, 'names a world variable this story declares', rules.validate, p)
        p = chain(closure=None)
        p['variables'].append({'id': 'realm', 'type': 'number', 'scope': 'world'})
        p['phases'][0]['exits'].append({'on': 'variable changed', 'match': {'target': 'realm'}, 'to': 'a'})
        rules.validate(p)

    def test_a_line_needs_a_quest_done_not_one_not_done(self):
        p = chain()
        p['quests'][1]['requires'] = {'test': 'not', 'condition': {'test': 'quest is', 'quest': 'one', 'status': 'done'}}
        self.assertRaisesRegex(ValueError, 'requires another quest of the story done', rules.validate, p)
        p = chain()
        p['quests'][1]['requires'] = {'test': 'any', 'conditions': [{'test': 'quest is', 'quest': 'one', 'status': 'done'},
                                                                    {'test': 'switch', 'var': 'open', 'is': True}]}
        self.assertRaisesRegex(ValueError, 'requires another quest of the story done', rules.validate, p)
        p['quests'][1]['requires']['test'] = 'all'
        rules.validate(p)


def casting(cast_town='larkfield'):
    """Doc 86, C7: a story that casts a carter of a town for the wolf who comes first, and recasts when the one cast dies; the
    axle's giver is whoever was cast."""
    p = {'format': 'ratw-story-program', 'version': 2, 'id': 'p9cast', 'title': 'The carter wanted', 'start': 'a',
         'audience': {'kind': 'open'},
         'variables': [{'id': 'carter', 'type': 'resident', 'scope': 'story'}, {'id': 'tried', 'type': 'residents', 'scope': 'story'},
                       {'id': 'failed', 'type': 'number', 'scope': 'story'}],
         'quests': [{'id': 'axle', 'title': 'An axle', 'giver': {'var': 'carter'}, 'repeat': 'daily',
                     'offer': {'say': 'My axle split. Could you find me another?'},
                     'objectives': [{'id': 'bring', 'label': 'Bring an axle.', 'on': 'gift', 'match': {'item': 'axle'}}],
                     'rewards': [{'kind': 'npc.greet', 'player': '$actor', 'text': 'You found me an axle!'}]}],
         'phases': [{'id': 'a', 'journal': 'A.', 'effects': [], 'exits': [], 'triggers': [
             {'id': 'cast', 'on': 'arrival', 'once': 'campaign', 'actions': [
                 {'do': 'effect', 'effect': {'kind': 'cast.pick', 'var': 'carter', 'job': 'carter', 'town': cast_town, 'notIn': 'tried',
                                             'near': '$actor'}}]},
             {'id': 'chosen', 'on': 'cast chosen', 'match': {'detail': 'carter'}, 'actions': [
                 {'do': 'list add', 'var': 'tried', 'value': {'var': 'carter'}}]},
             {'id': 'none', 'on': 'cast failed', 'actions': [{'do': 'add', 'var': 'failed', 'value': 1}]},
             {'id': 'died', 'on': 'death', 'if': {'test': 'compare', 'left': {'field': 'actor'}, 'op': '=', 'right': {'var': 'carter'}},
              'actions': [{'do': 'effect', 'effect': {'kind': 'cast.pick', 'var': 'carter', 'job': 'carter', 'town': cast_town,
                                                      'notIn': 'tried'}}]}]}],
         'tests': [{'name': 'x', 'cohort': [], 'events': [], 'expect': ['a']}]}
    rules.validate(p)
    return p


def cast_state(p, events, cast):
    p = program(p)
    p['tests'] = [{'name': 'a case', 'cohort': list(p['audience'].get('players', [])), 'events': events, 'expect': [p['start']],
                   'cast': cast}]
    seen = {}
    report = rules._play_case(p, p['tests'][0], lambda f, state, *_: seen.update(state=json.loads(json.dumps(state))))
    return seen['state'], report


class Phase9Casting(unittest.TestCase):
    """Doc 86, Phase 9: C7 cast.pick and a giver from a variable; SQ4 rewardLimit."""

    def test_a_carter_cast_in_larkfield_gives_the_quest_and_is_recast_when_they_die(self):
        p = casting()
        cast = {'carter': ['rm201', 'rm202']}
        state, report = cast_state(p, [fact(1, 'arrival', 0.1, ASH)], cast)
        self.assertEqual(state['counters']['story']['carter'], 'rm201')
        self.assertEqual(state['counters']['story']['tried'], ['rm201'])
        self.assertEqual([o['npc'] for o in quests.offers(p, state)], ['rm201'], 'the offer is at the carter cast')
        events = [fact(1, 'arrival', 0.1, ASH), fact(2, 'quest accepted', 0.2, ASH, target='run', item='rm201', detail='axle'),
                  fact(3, 'death', 0.3, 'rm201'), fact(4, 'quest accepted', 0.4, BIRCH, target='run', item='rm201', detail='axle'),
                  fact(5, 'quest accepted', 0.5, BIRCH, target='run', item='rm202', detail='axle')]
        state, report = cast_state(p, events, cast)
        self.assertEqual(state['counters']['story']['carter'], 'rm202', 'recast when the carter died, never the one tried')
        self.assertEqual(state['quests']['axle'][ASH]['giver'], 'rm201', "Ash's copy keeps the giver it began with")
        self.assertEqual(state['quests']['axle'][BIRCH]['giver'], 'rm202', 'Birch took it at the new carter, not the old')
        offers = sorted((o['npc'], o.get('players')) for o in quests.offers(p, state))
        self.assertEqual(offers, [('rm201', []), ('rm202', None)], 'offered at the new carter; the old keeps the copy under way')
        self.assertEqual(quests.journal(p, state, ASH)['quests'][0]['giver'], 'rm201')
        events += [fact(6, 'gift', 0.6, ASH, target='rm201', item='axle'), fact(7, 'quest handed in', 0.7, ASH, target='run', item='rm202', detail='axle'),
                   fact(8, 'quest handed in', 0.8, ASH, target='run', item='rm201', detail='axle')]
        state, report = cast_state(p, events, cast)
        self.assertEqual(state['quests']['axle'][ASH]['status'], 'done', 'handed in to the giver it began with')
        self.assertEqual(report['quests']['axle'][ASH], 'done')
        greet = [e for e in report['effects'] if e['kind'] == 'npc.greet']
        self.assertEqual([e['npc'] for e in greet], ['rm201'], "the reward's greeting from the copy's giver")

    def test_nobody_fits(self):
        p = casting()
        state, _ = cast_state(p, [fact(1, 'arrival', 0.1, ASH)], {})
        self.assertEqual(state['counters']['story'].get('failed'), 1)
        self.assertNotIn('carter', state['counters']['story'], 'the variable keeps what it held')
        self.assertEqual(quests.offers(p, state), [], 'nobody cast: no offer yet')
        state, _ = cast_state(p, [fact(1, 'arrival', 0.1, ASH)], {'carter': ['rm201']})
        self.assertEqual(state['castLog'][-1]['chosen'], 'rm201')

    def test_every_casting_has_its_own_seed_past_the_log_of_twenty(self):
        """End of slice (review): the seed counted the casting log, which keeps the last 20, so from the 21st casting on
        every one asked the same seed and got the same resident."""
        p = casting()
        p['phases'][0]['triggers'] = [{'id': 'again', 'on': 'deed', 'actions': [
            {'do': 'effect', 'effect': {'kind': 'cast.pick', 'var': 'carter', 'job': 'carter', 'town': 'larkfield'}}]}]
        rules.validate(p)
        state = rules.begin(p, [], 0)
        seeds = []
        token = quests.CASTER.set(lambda r: seeds.append(r['seed']) or 'rm201')
        try:
            for i in range(30):
                rules.advance(p, state, fact(i + 1, 'deed', 0.1 + i / 100, ASH), 0.1 + i / 100)
        finally:
            quests.CASTER.reset(token)
        self.assertEqual(len(seeds), 30)
        self.assertEqual(len(set(seeds)), 30, 'thirty castings, thirty seeds')
        self.assertEqual((len(state['castLog']), state['castCount']), (20, 30))

    def test_a_casting_asked_where_nobody_answers_waits(self):
        p = casting()
        state = rules.begin(p, [], 0)
        rules.advance(p, state, fact(1, 'arrival', 0.1, ASH), 0.1)
        self.assertEqual(len(state['castRequests']), 1, 'no caster: the request waits')
        req = state['castRequests'][0]
        self.assertEqual((req['var'], req['job'], req['town'], req['near'], req['notIn']), ('carter', 'carter', 'larkfield', ASH, []))
        token = quests.CASTER.set(lambda r: 'rm202')
        try:
            rules.advance(p, state, fact(2, 'deed', 0.2, ASH), 0.2)
        finally:
            quests.CASTER.reset(token)
        self.assertNotIn('castRequests', state)
        self.assertEqual(state['counters']['story']['carter'], 'rm202', 'answered before the next fact')

    def test_the_casting_checks(self):
        p = casting()
        for change, words in (({'job': None, 'town': None}, 'at least one'), ({'var': 'tried'}, 'resident variable of the story'),
                              ({'notIn': 'carter'}, 'list of residents')):
            q = copy.deepcopy(p)
            e = q['phases'][0]['triggers'][0]['actions'][0]['effect']
            for k, v in change.items():
                if v is None:
                    e.pop(k)
                else:
                    e[k] = v
            with self.subTest(change=change):
                self.assertRaisesRegex(ValueError, words, rules.validate, q)
        q = copy.deepcopy(p)
        q['quests'][0]['giver'] = {'var': 'nobody'}
        self.assertRaisesRegex(ValueError, 'giver from a variable', rules.validate, q)
        q['quests'][0]['giver'] = {'var': 'tried'}
        self.assertRaisesRegex(ValueError, 'giver from a variable', rules.validate, q)
        q['variables'].append({'id': 'mine', 'type': 'resident', 'scope': 'player'})
        q['quests'][0]['giver'] = {'var': 'mine'}
        self.assertRaisesRegex(ValueError, 'giver from a variable', rules.validate, q)

    def limited(self, per_day=2, scope='quest', ok=True):
        grant = {'kind': 'character.grant', 'player': '$actor', 'coins': 3, 'purse': 'town:larkfield', 'reason': 'for the axle'}
        p = one_quest({'on': 'gift', 'match': {'item': 'axle'}}, rewards=[grant], repeat='daily',
                      variables=[{'id': 'coin_larkfield_day', 'type': 'number', 'scope': 'world'},
                                 {'id': 'coin_larkfield_ok', 'type': 'switch', 'scope': 'world', 'initial': ok}],
                      players=(ASH, BIRCH, ELM, 'player-fen'))
        p['quests'][0]['rewardLimit'] = {'perDay': per_day, 'scope': scope}
        rules.validate(p)
        return p

    def test_a_reward_limit_pays_twice_a_day_and_writes_the_counters(self):
        p = self.limited()
        arrivals = [arrive(10 + i, w, 0.05) for i, w in enumerate((ASH, BIRCH, ELM, 'player-fen'))]
        gifts = [fact(i, 'gift', 0.1 * i, w, target='rm111', item='axle') for i, w in enumerate((ASH, BIRCH, ELM), 1)]
        state, report = cast_state(p, arrivals + gifts, {})
        grants = [e for e in report['effects'] if e['kind'] == 'character.grant']
        self.assertEqual([e['player'] for e in grants], [ASH, BIRCH], 'paid twice, then the reward without its coin')
        self.assertEqual(state['counters']['world']['coin_larkfield_day'], 6, 'the counter nh-purses resets: 3 + 3')
        self.assertEqual(state['rewardNotes'][-1]['wolf'], ELM)
        self.assertIn('coin rewards a day already', state['rewardNotes'][-1]['why'])
        state, report = cast_state(p, arrivals + gifts + [fact(9, 'gift', 1.2, 'player-fen', target='rm111', item='axle')], {})
        self.assertEqual([e['player'] for e in report['effects'] if e['kind'] == 'character.grant'], [ASH, BIRCH, 'player-fen'],
                         'a new game day pays again')
        # The floor switch off (decisions 3.2): no coin at all.
        state, report = cast_state(self.limited(ok=False), arrivals + gifts, {})
        self.assertFalse([e for e in report['effects'] if e['kind'] == 'character.grant'])
        self.assertIn('thin', state['rewardNotes'][-1]['why'])
        self.assertEqual(quests.purse_counter('town:ridgemere'), 'ridgemere')
        self.assertEqual(quests.purse_counter('till:job:rm_eleanor_saltcot'), 'till_eleanor_saltcot', "decisions 3.2's names")
        self.assertEqual(quests.purse_counter('house:greatly'), 'house_greatly')

    def test_a_reward_limit_counts_hand_ins_not_grants(self):
        # (Review of Phase 9: one hand-in's rewards and its bonus are one under the limit, and so is each wolf of a party.)
        p = self.limited(per_day=1)
        q = p['quests'][0]
        q['objectives'].append({'id': 'extra', 'label': 'Bring a spoke too.', 'on': 'gift', 'match': {'item': 'spoke'}, 'optional': True})
        q['bonus'] = [{'kind': 'character.grant', 'player': '$actor', 'coins': 2, 'purse': 'town:larkfield', 'reason': 'for the spoke'}]
        rules.validate(p)
        events = [arrive(1, ASH), fact(2, 'gift', 0.1, ASH, target='rm111', item='spoke'), fact(3, 'gift', 0.2, ASH, target='rm111', item='axle')]
        state, report = cast_state(p, events, {})
        self.assertEqual([e['coins'] for e in report['effects'] if e['kind'] == 'character.grant'], [3, 2],
                         'the reward and its bonus, one hand-in')
        self.assertEqual(state['counters']['world']['coin_larkfield_day'], 5, 'each grant still adds its coins')
        self.assertEqual(state['rewardLimits']['q']['n'], 1)
        self.assertFalse(state.get('rewardNotes'))
        events += [arrive(4, BIRCH, 0.25), fact(5, 'gift', 0.3, BIRCH, target='rm111', item='axle')]
        state, report = cast_state(p, events, {})
        self.assertEqual([e['player'] for e in report['effects'] if e['kind'] == 'character.grant'], [ASH, ASH],
                         "Birch's hand-in the same day is the second: no coin")
        # A party: every wolf of the hand-in paid, and it counts once.
        p = self.limited(per_day=1)
        p['quests'][0]['scope'] = 'party'
        rules.validate(p)
        events = [fact(1, 'party joined', 0.05, ASH, target='p1'), fact(2, 'party joined', 0.06, BIRCH, target='p1'), arrive(3, ASH),
                  fact(4, 'gift', 0.2, BIRCH, target='rm111', item='axle')]
        state, report = cast_state(p, events, {})
        self.assertEqual(sorted(e['player'] for e in report['effects'] if e['kind'] == 'character.grant'), sorted([ASH, BIRCH]),
                         "each wolf of the party paid for the one hand-in")
        self.assertEqual(state['rewardLimits']['q']['n'], 1)

    def test_the_reward_limit_checks(self):
        p = self.limited()
        for limit, words in (({'perDay': 0}, '1 to 1,000'), ({'perDay': 2, 'scope': 'town'}, 'for the quest or for its purse')):
            q = copy.deepcopy(p)
            q['quests'][0]['rewardLimit'] = limit
            with self.subTest(limit=limit):
                self.assertRaisesRegex(ValueError, words, rules.validate, q)
        q = copy.deepcopy(p)
        q['quests'][0]['rewards'] = [{'kind': 'deed.award', 'player': '$actor', 'detail': 'Found an axle.'}]
        self.assertRaisesRegex(ValueError, 'pays coin', rules.validate, q)
