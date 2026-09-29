#!/usr/bin/env python3
"""NPC chronicles (chronicle.py): compiled offline from made-up events, and read from a scratch event log."""
from __future__ import annotations

import json
import secrets
import unittest

import chronicle as C

NAMES = {'fennel': 'Fennel', 'wren': 'Wren', 'sorrel': 'Sorrel', 'sloe': 'Sloe', 'player-ada': 'Ada'}


def ev(i, day, kind, actor='', target='', **more):
    return {'id': i, 'day': day, 'kind': kind, 'actor': actor, 'target': target, 'cell': more.pop('cell', ''),
            'item': more.pop('item', ''), 'quantity': more.pop('quantity', 0), 'coins': more.pop('coins', 0),
            'detail': more.pop('detail', ''), **more}


LIFE = [
    ev(1, 0.2, 'spawn', 'fennel', detail='spawn rule bakers'),
    ev(2, 1.5, 'apprenticeship', 'fennel', 'wren', detail='Baker'),
    ev(3, 2.1, 'economy', 'player-ada', 'fennel', item='meal', quantity=1, coins=6, detail='resident food purchase'),
    ev(4, 2.2, 'economy', 'wren', 'fennel', coins=2, detail='service wages'),
    ev(5, 2.3, 'economy', 'fennel', 'fennel', item='meal', quantity=1, detail='cook'),
    ev(6, 2.4, 'conversation', 'player-ada', 'fennel'),
    ev(7, 2.5, 'conversation', 'player-ada', 'fennel'),
    ev(8, 3.0, 'theft', 'player-ada', 'fennel', coins=3, cell='shop', detail='inc-1'),
    ev(9, 3.1, 'stopped by the watch', 'sloe', 'player-ada', detail='theft'),
    ev(10, 40.0, 'death', 'wren'),
    ev(11, 40.0, 'mourning', 'fennel', 'wren', detail='family'),
    ev(12, 41.0, 'succession', 'fennel', 'wren', detail='Baker: the apprentice'),
    ev(13, 100.0, 'marriage', 'fennel', 'sorrel', detail='Sorrel moves in'),
    ev(14, 100.5, 'caravan departs', 'caravan-1', 'fennel'),         # Names them, but it's the road's story.
    ev(15, 101.0, 'economy', 'fennel', 'player-ada', item='herbs', quantity=2, coins=4, detail='resident herb purchase'),
]


class DateTests(unittest.TestCase):
    def test_the_game_calendar(self):
        self.assertEqual('Spring 1, Year 1', C.date_label(0))
        self.assertEqual('Summer 1, Year 1', C.date_label(92))
        self.assertEqual('Autumn 1, Year 1', C.date_label(184))
        self.assertEqual('Winter 90, Year 1', C.date_label(364.9))
        self.assertEqual('Spring 1, Year 2', C.date_label(365))

    def test_ids_nobody_named(self):
        self.assertEqual('Greyfen Road', C.plain_name('camp_greyfen_road'))
        self.assertEqual('someone', C.plain_name(''))


class CompileTests(unittest.TestCase):
    def setUp(self):
        self.c = C.compile_chronicle('fennel', LIFE, NAMES, {'shop': 'Tallow & Twine'})

    def text(self):
        return [e['text'] for e in self.c['entries']]

    def test_a_life_in_order(self):
        self.assertEqual(['Fennel first appeared.', 'Fennel became apprentice to Wren (Baker).',
                          'Fennel first spoke with Ada.', 'Ada stole 3 pennies from Fennel.', 'Fennel mourned Wren, one of the family.',
                          'Fennel took up the post of Baker, after Wren (the apprentice).', 'Fennel married Sorrel.'],
                         self.text())

    def test_only_their_own_events(self):
        self.assertNotIn('death', [e['kind'] for e in self.c['entries']], "Wren's death is Wren's; Fennel mourns")
        self.assertFalse(any('watch' in t for t in self.text()), "The watch stopping Ada is Ada's story")
        self.assertFalse(any('caravan' in e['kind'] for e in self.c['entries']))

    def test_what_is_known_of_it(self):
        theft = next(e for e in self.c['entries'] if e['kind'] == 'theft')
        self.assertEqual(('Spring 4, Year 1', 'Tallow & Twine', ['player-ada'], 2),
                         (theft['date'], theft['place'], theft['people'], theft['importance']))
        self.assertEqual(3, next(e for e in self.c['entries'] if e['kind'] == 'marriage')['importance'])
        self.assertEqual((0.2, 101.0, 15), (self.c['firstDay'], self.c['lastDay'], self.c['lastEvent']))

    def test_the_round_folded_a_season_at_a_time(self):
        self.assertEqual([('Spring, Year 1', 'Earned 8 pennies (2 pennies in wages); sold 1 meal; cooked 1 meal; '
                                             'talked 2 times with Ada.'),
                          ('Summer, Year 1', 'Spent 4 pennies; bought 2 bundles of herbs.')],
                         [(s['label'], s['text']) for s in self.c['seasons']])

    def test_counted_rows_from_the_database(self):
        counted = [ev(1, 5.0, 'economy', 'player-ada', 'fennel', item='meal', quantity=12, coins=72,
                      detail='resident food purchase', count=12),
                   ev(2, 5.0, 'conversation', 'player-ada', 'fennel', count=4)]
        c = C.compile_chronicle('fennel', counted, NAMES)
        self.assertEqual('Earned 72 pennies; sold 12 meals; talked 4 times with Ada.', c['seasons'][0]['text'])
        self.assertEqual(16, c['events'])

    def test_goods_moved_without_money(self):
        rows = [ev(1, 1, 'economy', 'herb patch', 'fennel', item='herbs', quantity=1, detail='gather'),
                ev(2, 1, 'economy', 'fennel', 'consumed', item='meal', quantity=1, detail='eat'),
                ev(3, 1, 'economy', 'wren', 'fennel', item='meal', quantity=3, coins=4, detail='inheritance'),
                ev(4, 1, 'economy', 'fennel', 'player-ada', coins=3, detail='stolen')]
        text = C.compile_chronicle('fennel', rows, NAMES)['seasons'][0]['text']
        self.assertEqual('Earned 4 pennies; gathered 1 bundle of herbs; ate 1 meal; received 3 meals.', text,
                         'A theft is told as a theft, not as spending')

    def test_told_to_the_npc(self):
        c = C.compile_chronicle('fennel', LIFE, NAMES, second_person=True)
        lines = C.milestones(c).splitlines()
        self.assertIn('Spring 2, Year 1: You became apprentice to Wren (Baker).', lines)
        self.assertIn('Summer 9, Year 1: You married Sorrel.', lines)
        self.assertIn('Spring 4, Year 1: Ada stole 3 pennies from you.', lines)
        self.assertEqual(lines, sorted(lines, key=lambda l: [x for x in LIFE if C.date_label(x['day']) == l.split(':')[0]][0]['id']),
                         'Oldest first')
        self.assertLessEqual(len(C.milestones(c, limit=3).splitlines()), 3)
        self.assertEqual('', C.milestones(C.compile_chronicle('nobody', LIFE, NAMES)))

    def test_every_kind_the_game_logs_reads_as_a_sentence(self):
        kinds = ['character created', 'spawn', 'death', 'revival', 'marriage', 'mourning', 'apprenticeship',
                 'apprenticeship completed', 'succession', 'vacancy', 'returned to work', 'estate settled',
                 'newcomer sent for', 'relocation', 'cleared', 'promise', 'promise broken', 'contract posted',
                 'contract taken', 'contract completed', 'theft', 'attempted theft', 'assault', 'beaten down', 'warrant',
                 'reported', 'stopped by the watch', 'fine paid', 'arrest', 'released', 'bandits demand', 'fight', 'robbed',
                 'paid off bandits', 'bandit falls', 'bandits flee', 'camp cleared', 'operator', 'something new']
        for kind in kinds:
            for who in (('fennel', 'wren'), ('wren', 'fennel')):
                c = C.compile_chronicle('fennel', [ev(1, 1, kind, *who, detail='Baker', coins=2)], NAMES)
                text = c['entries'][0]['text']
                self.assertTrue(text[0].isupper() and text.endswith('.') and 'None' not in text, f'{kind}: {text}')
        you = C.compile_chronicle('fennel', [ev(1, 1, 'revival', 'fennel')], NAMES, second_person=True)
        self.assertEqual('You were brought back to life.', you['entries'][0]['text'])

    def test_read_from_a_world_check_export(self):
        import tempfile
        with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as f:
            json.dump({'events': [{'kind': 'marriage', 'actor': 'fennel', 'target': 'sorrel', 'day': 100.0}],
                       'names': {'fennel': 'Fennel', 'sorrel': 'Sorrel'}}, f)
        rows, names = C.from_export(f.name)
        self.assertEqual('Fennel married Sorrel.', C.compile_chronicle('fennel', rows, names)['entries'][0]['text'])

    def test_story_lines(self):
        lines = C.story_lines(self.c)
        self.assertEqual('Spring 1, Year 1: Fennel first appeared.', lines[0])
        self.assertTrue(any('(the round)' in line for line in lines))
        self.assertEqual(3, len(C.story_lines(self.c, limit=3)))


class DatabaseTests(unittest.TestCase):
    """Against a scratch database with the event log (skipped without the local PostgreSQL)."""

    def setUp(self):
        import test_world_db as T
        import world_db as W
        if not T.database_available():
            self.skipTest('local PostgreSQL not running')
        self.name = f'ratw_test_{secrets.token_hex(4)}'
        with T.superuser() as su:
            su.execute(f'CREATE DATABASE {self.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {self.name} TO ratw_game, ratw_dm')
        self.addCleanup(self.drop, T)
        with W.connect('dev', 'owner', dbname=self.name) as owner:
            W.migrate(owner)
            owner.execute("INSERT INTO world.worlds (id, name) VALUES ('w', 'Test world')")
        self.connect = lambda role='dm': W.connect('dev', role, dbname=self.name)
        events = [{'kind': e['kind'], 'actor': e['actor'], 'target': e['target'], 'cell': e['cell'], 'item': e['item'],
                   'quantity': e['quantity'], 'coins': e['coins'], 'detail': e['detail'], 'day': e['day'], 'time': e['day'] * 14400}
                  for e in LIFE]
        events += [{'kind': 'economy', 'actor': 'player-ada', 'target': 'fennel', 'item': 'meal', 'quantity': 1, 'coins': 6,
                    'detail': 'resident food purchase', 'day': 2.9}] * 3
        payload = {'schema': 1, 'players': [{'id': 'player-ada', 'name': 'Ada'}], 'npcs': [{'id': 'fennel', 'name': 'Fennel'}]}
        with self.connect('game') as game:
            game.execute("SELECT game.record_events('w', %s::jsonb)", (json.dumps(events),))
            game.execute('SELECT game.save_checkpoint(%s, 1, %s)', ('w', json.dumps(payload)))

    def drop(self, T):
        with T.superuser() as su:
            su.execute(f'DROP DATABASE IF EXISTS {self.name} WITH (FORCE)')

    def test_read_from_the_log_as_compiled_offline(self):
        with self.connect() as conn:
            c = C.load(conn, 'fennel')
        self.assertEqual('Fennel', c['name'])
        self.assertIn('Ada stole 3 pennies from Fennel.', [e['text'] for e in c['entries']])
        self.assertIn('Wren', c['entries'][1]['text'], 'Someone with no saved body keeps a readable name')
        self.assertEqual('Earned 26 pennies (2 pennies in wages); sold 4 meals; cooked 1 meal; '
                         'talked 2 times with Ada.', c['seasons'][0]['text'], 'Routine counted in the database, a day at a time')
        self.assertEqual(16, c['events'], "Everything with Fennel in it (the watch stopping Ada and Wren's death aren't)")

    def test_the_game_role_reads_life_events_only(self):
        with self.connect('game') as conn:
            c = C.load(conn, 'fennel', second_person=True, routine=False)
        self.assertEqual([], c['seasons'])
        self.assertIn('You married Sorrel.', C.milestones(c))

    def test_someone_with_no_events(self):
        with self.connect() as conn:
            c = C.load(conn, 'nobody')
        self.assertEqual(([], [], 0), (c['entries'], c['seasons'], c['events']))


if __name__ == '__main__':
    unittest.main()
