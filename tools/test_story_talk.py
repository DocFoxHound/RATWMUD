"""Doc 86, Phase 1b (Docs/Design/86-quests-and-the-ninth-howl.md; artifacts/quests/design/engine-spec.md C8, C9, C11, T7,
SQ2, V1): what residents remember of a story (talk.remember, talk.forget, with the realm, a town, a resident and a resident
about one wolf), a rumour and its clearing (belief.plant), figures of the realm (figure.set, figure.clear), residents
spared from old age and protected for a while (npc.spare, npc.unspare, npc.protect's `lasts`), and the purses that pay
(till:job:<founder>). Pure: the catalogue's entries, the checks, the orders each becomes, the purse mapping. In a throwaway
migrated database (skipped without the local PostgreSQL): approval's new refusals and warnings (a dead giver, a resident
with no place, a missing account, a works purse, a quest's board and objectives), a figure written and put back, and a
campaign's holds let go. The game applying them is Tests/story_world_tests.cpp and Tests/dialogue_tests.cpp."""

import copy
import json
import unittest

import story_program as rules
import story_quests as quests
import story_runner as runner
import story_v2_samples
import test_story_console as console
from test_world_db import database_available

CAT = rules.CATALOGUE
NEW = ('npc.spare', 'npc.unspare', 'talk.remember', 'talk.forget', 'belief.plant', 'figure.set', 'figure.clear')
ASH = 'player-ash'


def program(actions=(), entry=(), audience='participant'):
    """A version 2 story: one phase whose trigger (on arrival, for the arriving wolf) runs `actions`."""
    return {
        'format': 'ratw-story-program', 'version': 2, 'id': 'ninth-howl', 'title': 'The Ninth Howl',
        'audience': {'kind': 'players', 'players': [ASH, 'player-birch']}, 'start': 'one',
        'phases': [{'id': 'one', 'journal': 'A howl in the hills.', 'effects': list(entry), 'exits': [],
                    'triggers': [{'id': 'came', 'on': 'arrival', 'audience': audience,
                                  'actions': list(actions) or [{'do': 'note', 'text': 'x'}]}]}],
        'tests': [{'name': 'Ash comes', 'cohort': [ASH], 'events': [{'id': 'e1', 'kind': 'arrival', 'actor': ASH, 'day': 1}],
                   'expect': ['one']}],
    }


def fx(kind, **params):
    return {'do': 'effect', 'effect': {'kind': kind, **params}}


REMEMBER = {'scope': 'town', 'town': 'greyfen', 'key': 'nh_stage', 'value': 'reckoning', 'days': 30}
PAIR = {'scope': 'pair', 'npc': 'rm111', 'player': '$actor', 'key': 'nh_stone', 'value': 'brought', 'days': 60, 'keep': True}
RUMOUR = {'subject': 'rm111', 'claim': 'was seen at the Customs House at midnight', 'town': 'greyfen', 'count': 12, 'confidence': 60}
FIGURE = {'figure': 'nh_villain', 'role': 'villain', 'npc': 'rm111', 'title': 'the ledger keeper', 'known': 'realm'}


class Catalogue(unittest.TestCase):
    def test_the_new_effects_are_described(self):
        ids = {e['id']: e for e in CAT['effects']}
        for kind in NEW:
            with self.subTest(kind=kind):
                self.assertIn(kind, ids)
                self.assertIn(ids[kind]['group'], {g['id'] for g in CAT['actionGroups']})
        self.assertEqual({k for k in ('talkkey', 'talkvalue', 'figure') if k in CAT['types']}, {'talkkey', 'talkvalue', 'figure'})
        self.assertTrue(ids['figure.set'].get('needsBuilder') and ids['figure.clear'].get('needsBuilder'), 'figures need a builder')
        lasts = next(p for p in ids['npc.protect']['params'] if p['key'] == 'lasts')
        self.assertEqual((lasts['values'], lasts['default']), (['ever', 'run', 'campaign'], 'ever'))
        kind = next(p for p in ids['belief.plant']['params'] if p['key'] == 'claimKind')
        self.assertEqual(kind['values'], ['story.rumour', 'story.cleared'])
        self.assertIn('Offered again', ids['npc.offer']['help'])
        self.assertNotIn('works, watch or church', next(e for e in CAT['events'] if e['id'] == 'purse crossed')['help'])
        claims = json.loads((runner.DATA / 'Talk/claims.json').read_text())['kinds']
        self.assertEqual({'story.rumour', 'story.cleared'} & set(claims), {'story.rumour', 'story.cleared'})
        self.assertEqual(set(rules.figure_roles()), set(next(p for p in ids['figure.set']['params'] if p['key'] == 'role')['values']))

    def test_the_undone_and_the_orders(self):
        self.assertEqual(runner.WORLD_ORDERS['belief.plant'], 'subject')
        self.assertIsNone(runner.WORLD_ORDERS['talk.remember'])
        self.assertIn('npc.spare', runner.HOLD_ORDERS)


class Purses(unittest.TestCase):
    def test_a_till_by_either_spelling(self):
        for purse, ok in [('till:job:rm_ilse_coalley', True), ('till:rm_ilse_coalley', True), ('till:job:', False), ('till:a:b', False),
                          ('town:coldwater', True), ('town:coldwater:works', True), ('chapter:the-ninth', True), ('treasury', False)]:
            with self.subTest(purse=purse):
                self.assertEqual(quests.check_purse(purse), ok)
        self.assertIn('till:job:<resident>', quests.PURSE_WORDS)

    def test_the_account_a_purse_pays_from(self):
        accounts, towns = {'stores:coldwater', 'treasury', 'till:job:rm_eleanor_saltcot'}, {'coldwater', 'ridgemere'}
        self.assertEqual(runner.purse_account('town:coldwater', accounts, towns), 'stores:coldwater')
        self.assertEqual(runner.purse_account('town:ridgemere', accounts, towns), 'treasury', 'the city pays from the treasury')
        self.assertEqual(runner.purse_account('till:rm_eleanor_saltcot', accounts, towns), 'till:job:rm_eleanor_saltcot')
        self.assertEqual(runner.purse_account('till:job:rm_eleanor_saltcot', accounts, towns), 'till:job:rm_eleanor_saltcot')
        self.assertEqual(runner.purse_account('town:atlantis', accounts, towns), 'stores:atlantis', 'no such town: no such account')

    def test_where_a_program_names_its_purses_and_residents(self):
        p = rules.validate(story_v2_samples.TALK)
        purses = [x[2] for x in runner._purse_sites(p)]
        self.assertIn('till:job:rm111', purses)
        roles = {(rid, role) for _, _, rid, role in runner._resident_sites(p)}
        self.assertIn(('rm111', 'npc'), roles)
        self.assertIn(('rm112', 'subject'), roles)
        self.assertEqual(runner.talk_names(p), ['p_pledge'])


class Checks(unittest.TestCase):
    def ok(self, *actions, entry=(), audience='participant'):
        return rules.validate(program(actions, entry, audience))

    def bad(self, words, *actions, entry=(), audience='participant'):
        self.assertRaisesRegex(ValueError, words, rules.validate, program(actions, entry, audience))

    def test_each_is_sound(self):
        self.ok(fx('talk.remember', **REMEMBER), fx('talk.remember', **PAIR), fx('talk.forget', scope='realm', key='nh_stage'),
                fx('belief.plant', **RUMOUR), fx('belief.plant', **RUMOUR, claimKind='story.cleared'), fx('figure.set', **FIGURE),
                fx('figure.clear', figure='nh_villain'), fx('npc.spare', npc='rm111'), fx('npc.spare', npc='rm111', lasts='campaign'),
                fx('npc.unspare', npc='rm111'), fx('npc.protect', npc='rm111', lasts='run'))
        self.ok(entry=[{'kind': 'talk.remember', 'scope': 'realm', 'key': 'nh_stage', 'value': 'waking', 'days': 30},
                       {'kind': 'npc.spare', 'npc': 'rm111', 'lasts': 'campaign'}, {'kind': 'figure.set', **FIGURE}])
        # (The looped wolf, from a trigger of anyone: For each over the story's wolves.)
        self.ok({'do': 'each', 'over': 'participants', 'actions': [fx('talk.remember', **{**PAIR, 'player': '$each'})]}, audience='world')

    def test_the_owners_parts_go_together(self):
        self.bad('names the town', fx('talk.remember', **{**REMEMBER, 'npc': 'rm111'}))
        self.bad('names the town', fx('talk.remember', scope='town', key='k', value='v', days=1))
        self.bad('names no town', fx('talk.remember', scope='realm', town='greyfen', key='k', value='v', days=1))
        self.bad('resident and the wolf', fx('talk.remember', scope='pair', npc='rm111', key='k', value='v', days=1))
        self.bad('a participant trigger', fx('talk.remember', **PAIR), audience='world')
        self.bad('Pair memory', entry=[{'kind': 'talk.remember', **{**PAIR, 'player': 'player-ash'}}])
        self.bad('Pair memory', fx('talk.remember', **{**PAIR, 'player': 'player-ash'}))

    def test_keys_values_and_claims(self):
        for k, v in [('key', 'Nh_stage'), ('key', '1stage'), ('key', 'x' * 41), ('value', 'two words'), ('value', 'Reckoning')]:
            with self.subTest(key=k, value=v):
                self.bad(f'invalid talk.remember.{k}', fx('talk.remember', **{**REMEMBER, k: v}))
        for claim in ('Was seen at the mill', 'was seen with rm112', 'took #x.name#\'s silver', 'stole from wolf-ash', 'met the_innkeeper'):
            with self.subTest(claim=claim):
                self.bad('a phrase that follows a name', fx('belief.plant', **{**RUMOUR, 'claim': claim}))
        self.bad('invalid belief.plant.count', fx('belief.plant', **{**RUMOUR, 'count': 51}))
        self.bad('invalid belief.plant.claimKind', fx('belief.plant', **{**RUMOUR, 'claimKind': 'theft'}))

    def test_a_figure_is_one_of_three_and_known_somewhere(self):
        self.bad('one resident, one wolf, or a name', fx('figure.set', **{**FIGURE, 'name': 'Old Brown'}))
        self.bad('one resident, one wolf, or a name', fx('figure.set', **{k: v for k, v in FIGURE.items() if k != 'npc'}))
        self.ok(fx('figure.set', **{**{k: v for k, v in FIGURE.items() if k != 'npc'}, 'player': '$actor'}))
        self.ok(fx('figure.set', **{**{k: v for k, v in FIGURE.items() if k != 'npc'}, 'name': 'The Captain of the Grey Sails'}))
        self.bad('known in a town', fx('figure.set', **{**FIGURE, 'known': 'region'}))
        self.bad('known in a town', fx('figure.set', **{**FIGURE, 'region': 'greyfen'}))
        self.ok(fx('figure.set', **{**FIGURE, 'known': 'house', 'house': 'house_greatly'}))
        self.bad('invalid figure.set.role', fx('figure.set', **{**FIGURE, 'role': 'emperor'}))
        self.bad('invalid figure.set.figure', fx('figure.set', **{**FIGURE, 'figure': 'NH'}))

    def test_the_holds_lasts(self):
        self.bad('invalid npc.spare.lasts', fx('npc.spare', npc='rm111', lasts='ever'))
        self.bad('invalid npc.protect.lasts', fx('npc.protect', npc='rm111', lasts='forever'))


class FakeConn:
    """Enough of a connection for order_of's campaign lookup (no campaigns table): every query answers nothing."""

    def __init__(self, campaign=''):
        self.campaign = campaign

    def execute(self, sql, args=()):
        answer = (bool(self.campaign),) if 'to_regclass' in sql else (self.campaign,)
        return type('R', (), {'fetchone': lambda _self: answer, 'fetchall': lambda _self: [answer]})()


class Orders(unittest.TestCase):
    def phase(self, p=None):
        return rules.phases_of(rules.validate(p or program([fx('talk.remember', **REMEMBER)])))[0]

    def order(self, e, conn=None):
        return runner.order_of(conn or FakeConn(), 'w', 'world-run', 'martin', {'actor': ASH, 'visits': 3, 'phase': 'one'}, e, self.phase())

    def test_the_game_orders(self):
        self.assertEqual(self.order({'kind': 'talk.remember', **REMEMBER}),
                         ('talk.remember', 'world-run', {**REMEMBER, 'run': 'world-run', 'visit': 3}))
        k, target, payload = self.order({'kind': 'talk.remember', **PAIR})
        self.assertEqual((target, payload['player'], payload['npc'], payload['keep']), ('world-run', ASH, 'rm111', True), 'the acting wolf')
        k, target, payload = self.order({'kind': 'belief.plant', **RUMOUR, 'claimKind': 'story.cleared'})
        self.assertEqual((k, target, payload['claimKind'], payload['run']), ('belief.plant', 'rm111', 'story.cleared', 'world-run'))
        self.assertEqual(self.order({'kind': 'npc.spare', 'npc': 'rm111'}), ('npc.spare', 'rm111', {'run': 'world-run', 'visit': 3}))
        self.assertEqual(self.order({'kind': 'npc.spare', 'npc': 'rm111', 'lasts': 'campaign'}, FakeConn('the-ninth-howl')),
                         ('npc.spare', 'rm111', {'lasts': 'campaign', 'run': 'world-run', 'visit': 3, 'campaign': 'the-ninth-howl'}))
        self.assertEqual(self.order({'kind': 'npc.protect', 'npc': 'rm111'}), ('npc.protect', 'rm111', {}), 'for good: the mark, as ever')
        self.assertEqual(self.order({'kind': 'npc.protect', 'npc': 'rm111', 'lasts': 'run'})[2], {'lasts': 'run', 'run': 'world-run', 'visit': 3})
        self.assertEqual(self.order({'kind': 'npc.unprotect', 'npc': 'rm111'})[2], {'run': 'world-run', 'visit': 3})


class Sample(unittest.TestCase):
    def test_the_sample_rehearses_clean(self):
        report = rules.review(story_v2_samples.TALK)
        self.assertTrue(report['passed'], report['tests'])
        self.assertEqual((report['uncovered'], report.get('uncoveredReady', [])), ([], []))
        self.assertEqual([x for x in rules.problems(story_v2_samples.TALK) if x['level'] == 'error'], [])


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class InTheWorld(unittest.TestCase):
    """Approval's checks against a throwaway world (the console's fixture), with a checkpoint that has accounts."""
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)

    def setUp(self):
        console.ConsoleReads.setUp(self)
        self.conn.execute('RESET ROLE')
        self.conn.execute("INSERT INTO world.areas(world_id,id,kind) VALUES (%s,'gate','cell')", (self.world,))
        self.conn.execute("""INSERT INTO world.cells(world_id,id,name,x,y,width,height,region)
                             VALUES (%s,'gate','Gate',200,100,8,6,'greyfen')""", (self.world,))
        self.conn.execute("INSERT INTO live.factions(world_id,id,name) VALUES (%s,'house_greatly','House Greatly')", (self.world,))
        society = {'accounts': {'treasury': {'cash': 900}, 'stores:greyfen': {'cash': 200}, 'till:job:rm111': {'cash': 120}},
                   'report': {'markets': [{'id': 'greyfen', 'name': 'Greyfen'}]}}
        self.conn.execute("UPDATE game.checkpoints SET payload=jsonb_set(payload,'{society}',%s::jsonb) WHERE world_id=%s",
                          (json.dumps(society), self.world))
        self.conn.execute("INSERT INTO live.npc_state(world_id,npc_id,alive,state) VALUES (%s,'rm119',false,'{}')", (self.world,))
        self.conn.execute('SET LOCAL ROLE ratw_dm')

    def refs(self, p, chapters=False):
        return runner.references(self.conn, self.world, rules.validate(p), chapters=chapters)

    def test_purses(self):
        grant = {'kind': 'character.grant', 'player': '$actor', 'coins': 5, 'reason': 'thanks'}
        for purse in ('till:job:rm111', 'till:rm111', 'town:greyfen'):
            with self.subTest(purse=purse):
                self.refs(program([{'do': 'effect', 'effect': {**grant, 'purse': purse}}]))
        for purse, words in [('till:job:nobody', 'No purse till:job:nobody'), ('town:greyfen:works', 'no works, watch or church'),
                             ('town:atlantis', 'No town "atlantis"')]:
            with self.subTest(purse=purse):
                self.assertRaisesRegex(ValueError, words, self.refs, program([{'do': 'effect', 'effect': {**grant, 'purse': purse}}]))
        self.conn.execute('RESET ROLE')
        self.conn.execute("UPDATE game.checkpoints SET payload=payload-'society' WHERE world_id=%s", (self.world,))
        self.conn.execute('SET LOCAL ROLE ratw_dm')
        warnings = self.refs(program([{'do': 'effect', 'effect': {**grant, 'purse': 'till:job:nobody'}}]))
        self.assertTrue(any('no checkpoint yet' in w for w in warnings), 'a world without accounts saved: a warning')

    def test_residents_placed_and_alive(self):
        self.assertRaisesRegex(ValueError, 'rm119 has died', self.refs, program([fx('npc.spare', npc='rm119')]))
        p = program()
        p['quests'] = [{'id': 'q', 'title': 'Q', 'giver': 'rm119', 'objectives': [{'id': 'a', 'label': 'Talk.', 'on': 'conversation',
                                                                                   'match': {'target': 'rm111'}}]}]
        self.assertRaisesRegex(ValueError, 'rm119 has died', self.refs, p)
        self.refs(program([fx('npc.kill', npc='rm119')]))   # (A warning: the game refuses it.)
        dead = program()
        dead['phases'][0]['triggers'][0].update(on='death', match={'actor': 'rm119'}, audience='world')
        self.refs(dead)   # (A story may speak of the dead.)
        self.conn.execute('RESET ROLE')
        self.conn.execute("UPDATE live.npcs SET home_area=NULL WHERE world_id=%s AND id='rm113'", (self.world,))
        self.conn.execute('SET LOCAL ROLE ratw_dm')
        self.assertRaisesRegex(ValueError, 'rm113 has no place', self.refs, program([fx('npc.spare', npc='rm113')]))

    def test_quests_boards_and_objectives(self):
        p = program()
        p['quests'] = [{'id': 'q', 'title': 'Q', 'board': 'atlantis', 'objectives': [{'id': 'a', 'label': 'Talk.', 'on': 'conversation',
                                                                                      'match': {'target': 'rm111'}}]}]
        self.assertRaisesRegex(ValueError, 'No notice board', self.refs, p)
        p['quests'][0]['board'] = 'greyfen'
        self.refs(p)
        for o, words in [({'on': 'enter place', 'match': {'target': 'moon'}}, 'Unknown place'),
                         ({'on': 'camp cleared', 'match': {'target': 'camp_nowhere'}}, 'No camp'),
                         ({'on': 'gift', 'match': {'target': 'rm111', 'item': 'dragon_egg'}}, 'No item')]:
            with self.subTest(words=words):
                q = copy.deepcopy(p)
                q['quests'][0]['objectives'] = [{'id': 'a', 'label': 'Do it.', **o}]
                self.assertRaisesRegex(ValueError, words, self.refs, q)

    def test_the_new_effects_towns_and_a_campaigns_hold(self):
        self.assertRaisesRegex(ValueError, 'Unknown town', self.refs, program([fx('talk.remember', **{**REMEMBER, 'town': 'atlantis'})]))
        self.assertRaisesRegex(ValueError, 'Unknown subject', self.refs, program([fx('belief.plant', **{**RUMOUR, 'subject': 'rm999'})]))
        self.assertRaisesRegex(ValueError, 'chapter of a campaign', self.refs, program([fx('npc.spare', npc='rm111', lasts='campaign')]))
        self.refs(program([fx('npc.spare', npc='rm111', lasts='campaign')]), chapters=True)
        self.assertRaisesRegex(ValueError, 'Unknown faction', self.refs,
                               program([fx('figure.set', **{**FIGURE, 'known': 'house', 'house': 'house_nobody'})]))

    def test_a_figure_is_written_and_put_back(self):
        self.conn.execute('RESET ROLE')
        before = runner.write_figure(self.conn, self.world, {'figure': 'nh_villain', 'role': 'villain', 'name': 'Old Brown'})
        self.assertIsNone(before)
        state = {'actor': ASH, 'visits': 1, 'phase': 'one'}
        self.conn.execute('INSERT INTO dm.story_releases(id,world_id,digest,program,approved_by,ticket_hash,status) '
                          "VALUES ('world-fig',%s,'d','{}'::jsonb,'martin','h','running')", (self.world,))
        order = runner.order_of(self.conn, self.world, 'world-fig', 'martin', state, {'kind': 'figure.set', **FIGURE, 'player': None} if False
                                else {'kind': 'figure.set', **{k: v for k, v in FIGURE.items() if k != 'npc'}, 'player': '$actor'},
                                rules.phases_of(rules.validate(program()))[0])
        self.assertEqual(order[:2], ('voice.sync', 'nh_villain'))
        name, npc = self.conn.execute("SELECT name,npc_id FROM live.figures WHERE world_id=%s AND id='nh_villain'", (self.world,)).fetchone()
        self.assertEqual((name, npc), (console.WOLVES[ASH], None), "a wolf's name as it is now")
        hid, entry = self.conn.execute("SELECT id,entry FROM dm.story_trigger_history WHERE run='world-fig' ORDER BY id DESC LIMIT 1").fetchone()
        self.assertEqual(entry['figure']['previous']['name'], 'Old Brown')
        runner.restore_figure(self.conn, self.world, 'martin', hid)
        self.assertEqual(self.conn.execute("SELECT name FROM live.figures WHERE world_id=%s AND id='nh_villain'", (self.world,)).fetchone()[0],
                         'Old Brown')


if __name__ == '__main__':
    unittest.main()
