#!/usr/bin/env python3
"""The world package (tools/world_package.py; doc 77, section 8 and Progress, "Phase 5's plan"): the strip on small
hand-made saves (money conserved, escrow, maker marks, compound keys, unknown sections refused), the scan (an id
planted in a key, in a nested string, in a row; usernames; money; goods), the journal replay, and end to end: DEV
exported read only (one read-only transaction), scanned, imported into throwaway worlds and loaded by a real
ratw_server there, once as it is and once with players planted in its save before the strip.

The end-to-end tests need the local PostgreSQL and a built build-central (or build-core) ratw_server; they are skipped
without them. The throwaway databases ("pkg" in their names, or RATW_TEST_KEY_PREFIX) are dropped afterwards. DEV is
only read."""
import copy
import json
import os
from pathlib import Path
import re
import secrets
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

import world_db as W
import world_package as P
superuser = W.superuser


def database_available():
    """The local PostgreSQL, from Database/.env or the environment (a worktree has no .env of its own:
    `set -a; . Database/.env; set +a` in the main checkout)."""
    try:
        superuser().close()
        return True
    except Exception:
        return False

ROOT = Path(__file__).resolve().parent.parent
SERVERS = [ROOT / 'build-central' / 'ratw_server', ROOT / 'build-core' / 'ratw_server']
SERVER = next((s for s in SERVERS if s.exists()), None)

# Throwaway worlds' keys start with this (RATW_TEST_KEY_PREFIX: a run's own, to tell its databases from others').
KEY_PREFIX = os.environ.get('RATW_TEST_KEY_PREFIX', 'pkg')

WOLF = 'wolf-' + 'a' * 32
WOLF2 = 'wolf-' + 'b' * 32


def save(**extra):
    """A small save: a treasury, two residents, books that balance."""
    doc = {
        'schema': 1, 'journal': 3, 'revision': 7, 'sequence': 2, 'time': 10.0, 'calendarDays': 4.5,
        'accounts': {'version': 1, 'entries': [{'username': 'ashley', 'characters': [WOLF]}]},
        'director': {'worldId': '16A21C7D-E7BA-E1BD-46CB-9235A404BEAF', 'receipts': []},
        'players': [{'id': WOLF, 'name': 'Ash Tallow', 'npc': False}],
        'npcs': [{'id': 'wren', 'name': 'Wren', 'npc': True, 'witnessed': {WOLF: 3, 'bram': 2}, 'toldWardens': [WOLF],
                  'postureTarget': WOLF, 'scents': [{'item': 'pelt@' + WOLF, 'maker': WOLF, 'count': 1}],
                  'activity': 'working at Ridgemere'},
                 {'id': 'bram', 'name': 'Bram', 'npc': True, 'activity': "working at The Iron Fangs's camp"}],
        'society': {
            'minted': 1067, 'sunk': 100, 'nextEntry': 10, 'budgetDay': 4,
            'accounts': {
                'treasury': {'cash': 500, 'stock': {'meal': 5}},
                'wren': {'cash': 100, 'stock': {'sword@' + WOLF: 2, 'sword': 1, 'pelt@wren': 1}},
                'bram': {'cash': 50, 'stock': {}},
                WOLF: {'cash': 120, 'stock': {'herbs': 4, 'meal@' + WOLF: 2}},
                'chapter:chapter-1': {'cash': 40, 'stock': {'rope': 1}},
                'contract:k1': {'cash': 30, 'stock': {}},          # Escrow of a contract a player posted.
                f'letter:L1@{WOLF}': {'cash': 20, 'stock': {}},     # A player's letter's enclosure.
                'letter:L2@bram': {'cash': 15, 'stock': {}},        # A resident's letter to a player: back to Bram.
                'let:lodge1': {'cash': 0, 'stock': {'candle': 2}},           # The player's lodging's chest.
                'till:job:bram': {'cash': 60, 'stock': {'ale': 4}},          # Its landlord's till: the rent's account.
                'let:lodging-2': {'cash': 0, 'stock': {'soap': 1}},          # A resident's lodging's chest: kept.
                'let:lodging-9': {'cash': 7, 'stock': {}},                   # A chest whose lodging has gone.
                'stall:s1': {'cash': 5, 'stock': {}},
                'table:t1': {'cash': 20, 'stock': {}},
            },
            'ledger': [{'sequence': 4, 'from': 'wren', 'to': 'bram', 'coins': 3, 'day': 1, 'item': '', 'quantity': 0},
                       {'sequence': 5, 'from': WOLF, 'to': 'wren', 'coins': 5, 'day': 2, 'item': '', 'quantity': 0},
                       {'sequence': 6, 'from': 'wren', 'to': 'bram', 'coins': 0, 'day': 2, 'item': 'sword@' + WOLF,
                        'quantity': 1}],
            'residents': {'wren': {'role': 'civilian'}, 'bram': {'role': 'civilian'}},
            'careers': {'day': 4, 'positions': {'job:wren': {'holder': 'wren', 'apprentice': WOLF, 'lastHolder': '',
                                                             'vacantSince': -1},
                                                'job:smith': {'holder': WOLF, 'apprentice': '', 'lastHolder': 'bram',
                                                              'vacantSince': -1}},
                        'skill': {f'{WOLF}|job:smith': 40, 'wren|job:wren': 30},
                        'spouses': {'wren': 'bram', 'bram': 'wren'}, 'mourning': [{'who': 'wren', 'whom': WOLF}]},
        },
        'bonds': [{'holder': 'wren', 'other': WOLF}, {'holder': 'wren', 'other': 'bram'}],
        'beliefs': [{'holder': 'bram', 'subject': 'wren', 'source': WOLF}, {'holder': 'bram', 'subject': 'wren', 'source': ''}],
        'talkMemory': {f'pair:wren|{WOLF}': {'a': ['hello', 1]},
                       'person:wren': {'b': ['hi', 2], 'rumour': ['They say The Iron Fangs paid the watch.', 3]}},
        'regardLedger': {f'wren|{WOLF}': [], 'wren|bram': [{'day': 1, 'source': WOLF}, {'day': 2, 'source': 'bram'}]},
        'acquaintances': {'wren': [{'id': WOLF, 'names': ['Ash']}, {'id': 'bram', 'names': ['Bram']}],
                          WOLF: [{'id': 'wren', 'names': ['Wren']}]},
        'olderMemoryEvents': {f'wren|{WOLF}': [1, 2]},
        'chapters': {'next': 2, 'chapters': [{'id': 'chapter-1', 'name': 'The Iron Fangs', 'members': [{'id': WOLF}]}]},
        # A town's site (a town project stands on it) and a Chapter's camp.
        'camps': {'next': 4,
                  'sites': [{'id': 'site-1', 'chapter': 'town:ridgemere', 'name': 'Ridgemere', 'state': 'standing'},
                            {'id': 'site-2', 'chapter': 'chapter-1', 'name': "The Iron Fangs's camp", 'state': 'standing'}],
                  'structures': [{'id': 'structure-1', 'site': 'site-1', 'kind': 'granary', 'built': True},
                                 {'id': 'structure-2', 'site': 'site-2', 'kind': 'tent'}],
                  'staff': [{'npc': 'wren', 'site': 'site-1', 'role': 'hand'}, {'npc': 'bram', 'site': 'site-2', 'role': 'hand'}],
                  'offers': [{'npc': 'wren', 'site': 'site-2', 'role': 'hand'}],
                  'offered': {'site-1': 3, 'site-2': 4}},
        # Maps keyed by player, and a social scene a player was in (and one only residents were).
        'commandReceipts': {WOLF: ['c1', 'c2']},
        'responseReceipts': {WOLF: {'c1': '{"ok":true}'}},
        'scenesHeard': {WOLF: ['scene-a']},
        'aliases': {WOLF: ['Ashy']},
        'companions': {'wren': WOLF},
        'socialSessions': [{'id': 'scene-1', 'members': [{'actor': WOLF}, {'actor': 'wren'}]},
                           {'id': 'scene-2', 'members': [{'actor': 'wren'}, {'actor': 'bram'}]}],
        'socialAudiences': {f'scene-1|{WOLF}': ['wren'], 'scene-1|wren': [WOLF], 'scene-2|wren': ['bram', WOLF],
                            'scene-2|bram': ['wren']},
        'parties': {'next': 2, 'parties': [{'id': 'party-1', 'leader': WOLF, 'members': [{'id': WOLF}],
                                            'companions': [{'id': 'wren', 'by': WOLF}]}], 'neverAutoJoin': [WOLF]},
        'standing': {'accounts': [{'account': 'ashley', 'characters': [WOLF]}]},
        'factions': {'treaties': [{'id': 't1', 'chapter': 'chapter-1'}], 'missions': [{'id': 1, 'taker': WOLF},
                                                                                     {'id': 2, 'taker': ''}],
                     'standings': [{'key': f'{WOLF}|watch'}, {'key': 'wren|watch'}], 'nextMission': 3},
        'roads': {'contracts': [{'id': 'k1', 'poster': WOLF, 'taker': '', 'status': 'open'},
                                {'id': 'k2', 'poster': 'wren', 'taker': WOLF, 'status': 'taken', 'offeredTo': WOLF}],
                  'caravans': [{'id': 'c1', 'escorts': [WOLF, 'bram'], 'letters': ['L1', 'L9'], 'with': {WOLF: ['x']}}],
                  'nextId': 3},
        'crime': {'incidents': [{'id': 1, 'offender': 'bram', 'victim': WOLF, 'witnesses': []},
                                {'id': 2, 'offender': 'bram', 'victim': 'wren', 'witnesses': [{'id': WOLF}, {'id': 'wren'}]}],
                  'warrants': [{'person': 'bram', 'incidents': [1]}, {'person': 'bram', 'incidents': [2],
                                                                     'restitution': [{'to': WOLF}, {'to': 'wren'}]},
                               {'person': WOLF, 'incidents': [2]}],
                  'custody': [{'person': WOLF}], 'day': 4},
        'people': {'friends': [{'account': 'ashley', 'friend': 'other'}], 'profiles': [{'account': 'ashley'}],
                   # A player's lodging (rent into the landlord's till), and a resident's with a player as a guest.
                   'lodgings': [{'id': 'lodge1', 'holder': WOLF, 'landlord': 'bram', 'account': 'till:job:bram',
                                 'period': 'week', 'guests': []},
                                {'id': 'lodging-2', 'holder': 'bram', 'landlord': 'bram', 'account': 'till:job:bram',
                                 'period': 'night', 'guests': [WOLF, 'wren']}],
                   'stalls': [{'id': 's1', 'keeper': WOLF}],
                   'documents': [{'id': 'L1', 'author': WOLF, 'to': 'wren'}, {'id': 'L2', 'author': 'bram', 'to': WOLF},
                                 {'id': 'L3', 'author': 'bram', 'to': 'wren'}],
                   'projects': [{'id': 'p1', 'kind': 'granary', 'state': 'built', 'site': 'site-1', 'structure': 'structure-1',
                                 'by': WOLF, 'namedFor': WOLF, 'shown': {WOLF: 'the granary'},
                                 'gifts': [{'who': WOLF, 'coins': 3}, {'who': 'wren', 'coins': 2}]}],
                   'storylines': [{'id': 's1', 'owner': WOLF}, {'id': 's2', 'owner': '', 'authorAccount': 'ashley'},
                                  {'id': 's3', 'owner': '', 'participants': [{'id': WOLF}, {'id': 'wren'}]}],
                   'deeds': [{'wolf': WOLF}], 'protectedMarks': {'wren': True}},
        'weather': {'cell': 1}, 'festivals': [],
    }
    doc.update(extra)
    return doc


def cash(doc):
    return sum(a.get('cash', 0) for a in doc['society']['accounts'].values())


class StripTest(unittest.TestCase):
    def setUp(self):
        self.doc = save()
        self.before = cash(self.doc)
        self.st = P.strip(self.doc)
        self.accounts = self.doc['society']['accounts']

    def test_it_leaves_no_player_and_the_scan_passes(self):
        self.assertFalse(P.mentions(self.doc), json.dumps(self.doc)[:2000])
        errors, _ = P.scan(self.doc, {}, usernames={'ashley'})
        self.assertEqual(errors, [])

    def test_money_is_conserved_into_the_treasury(self):
        self.assertEqual(cash(self.doc), self.before)
        self.assertEqual(cash(self.doc), 1067 - 100)
        self.assertEqual(P.money(self.doc), [])
        # Player 120, the Chapter 40, the contract 30, the player's letter 20, the stall 5, the table 20, the chest of a
        # lodging gone 7.
        self.assertEqual(self.accounts['treasury']['cash'], 500 + 120 + 40 + 30 + 20 + 5 + 20 + 7)
        self.assertEqual(self.accounts['bram']['cash'], 50 + 15, "a resident's letter to a player: back to its writer")

    def test_escrow_accounts_close(self):
        for name in (WOLF, 'chapter:chapter-1', 'contract:k1', f'letter:L1@{WOLF}', 'letter:L2@bram', 'let:lodge1',
                     'stall:s1', 'table:t1', 'let:lodging-9'):
            self.assertNotIn(name, self.accounts)
        self.assertEqual(set(self.accounts), {'treasury', 'wren', 'bram', 'till:job:bram', 'let:lodging-2'})
        self.assertEqual(self.st.closed, 9)

    def test_lodgings_keep_the_landlords_account(self):
        """A lodging's account is its landlord's (the rent's), never closed; the player's chest is."""
        self.assertEqual(self.accounts['till:job:bram'], {'cash': 60, 'stock': {'ale': 4}})
        self.assertEqual(self.accounts['let:lodging-2'], {'cash': 0, 'stock': {'soap': 1}})
        self.assertEqual(self.doc['people']['lodgings'],
                         [{'id': 'lodging-2', 'holder': 'bram', 'landlord': 'bram', 'account': 'till:job:bram',
                           'period': 'night', 'guests': ['wren']}])

    def test_player_keyed_maps_go(self):
        d = self.doc
        for key in ('commandReceipts', 'responseReceipts', 'scenesHeard', 'aliases', 'companions'):
            self.assertEqual(d[key], {}, key)
        self.assertEqual([s['id'] for s in d['socialSessions']], ['scene-2'])
        self.assertEqual(d['socialAudiences'], {'scene-2|wren': ['bram'], 'scene-2|bram': ['wren']})
        self.assertEqual(d['parties'], {'next': 2, 'parties': [], 'neverAutoJoin': []})
        self.assertEqual(d['standing'], {'accounts': []})

    def test_a_towns_camp_stays_and_a_chapters_goes(self):
        camps = self.doc['camps']
        self.assertEqual([s['id'] for s in camps['sites']], ['site-1'])
        self.assertEqual([s['id'] for s in camps['structures']], ['structure-1'])
        self.assertEqual(camps['staff'], [{'npc': 'wren', 'site': 'site-1', 'role': 'hand'}])
        self.assertEqual(camps['offers'], [])
        self.assertEqual(camps['offered'], {'site-1': 3})
        self.assertEqual(camps['next'], 4)
        project = self.doc['people']['projects'][0]
        self.assertEqual((project['site'], project['structure'], project['by'], project['shown']),
                         ('site-1', 'structure-1', '', {}))
        wren, bram = self.doc['npcs']
        self.assertEqual(wren['activity'], 'working at Ridgemere', "a town site's staff keep theirs")
        self.assertEqual(bram['activity'], '', "the camp's name was a player's choice")

    def test_chapter_and_camp_names_are_warned_of(self):
        self.assertTrue({'The Iron Fangs', "The Iron Fangs's camp"} <= self.st.names)
        self.assertNotIn('Ridgemere', self.st.names)
        errors, warnings = P.scan(self.doc, {}, names=self.st.names)
        self.assertEqual(errors, [])
        self.assertTrue(any('talkMemory.person:wren.rumour' in w and 'Iron Fangs' in w for w in warnings), warnings)

    def test_goods_follow_and_maker_marks_come_off(self):
        self.assertEqual(self.accounts['wren']['stock'], {'sword': 3, 'pelt@wren': 1}, 'an NPC mark stays')
        self.assertEqual(self.accounts['treasury']['stock'], {'meal': 7, 'herbs': 4, 'rope': 1, 'candle': 2})
        self.assertEqual(self.doc['npcs'][0]['scents'][0]['item'], 'pelt')

    def test_compound_keys_and_relations(self):
        d = self.doc
        self.assertEqual(list(d['talkMemory']), ['person:wren'])
        self.assertEqual(d['regardLedger'], {'wren|bram': [{'day': 2, 'source': 'bram'}]})
        self.assertEqual(d['acquaintances'], {'wren': [{'id': 'bram', 'names': ['Bram']}]})
        self.assertEqual(d['olderMemoryEvents'], {})
        self.assertEqual(d['bonds'], [{'holder': 'wren', 'other': 'bram'}])
        self.assertEqual(len(d['beliefs']), 1)
        self.assertEqual(d['society']['careers']['skill'], {'wren|job:wren': 30})
        self.assertEqual(d['society']['careers']['mourning'], [])

    def test_careers_fall_vacant(self):
        positions = self.doc['society']['careers']['positions']
        self.assertEqual(positions['job:wren']['apprentice'], '')
        self.assertEqual(positions['job:smith']['holder'], '')
        self.assertEqual(positions['job:smith']['vacantSince'], 4)
        self.assertEqual(positions['job:smith']['lastHolder'], 'bram')

    def test_ledger_keeps_order_without_players(self):
        self.assertEqual([e['sequence'] for e in self.doc['society']['ledger']], [4])

    def test_npc_fields(self):
        wren = self.doc['npcs'][0]
        self.assertEqual(wren['witnessed'], {'bram': 2})
        self.assertEqual(wren['toldWardens'], [])
        self.assertEqual(wren['postureTarget'], '')

    def test_roads_crime_factions_people(self):
        d = self.doc
        self.assertEqual([c['id'] for c in d['roads']['contracts']], ['k2'])
        k2 = d['roads']['contracts'][0]
        self.assertEqual((k2['taker'], k2['status'], 'offeredTo' in k2), ('', 'open', False))
        self.assertEqual(d['roads']['caravans'][0]['escorts'], ['bram'])
        self.assertEqual(d['roads']['caravans'][0]['letters'], ['L9'])
        self.assertEqual(d['roads']['caravans'][0]['with'], {})
        self.assertEqual([i['id'] for i in d['crime']['incidents']], [2])
        self.assertEqual(d['crime']['incidents'][0]['witnesses'], [{'id': 'wren'}])
        self.assertEqual(d['crime']['warrants'], [{'person': 'bram', 'incidents': [2], 'restitution': [{'to': 'wren'}]}])
        self.assertEqual(d['crime']['custody'], [])
        self.assertEqual(d['factions']['treaties'], [])
        self.assertEqual(d['factions']['missions'], [{'id': 2, 'taker': ''}])
        self.assertEqual(d['factions']['standings'], [{'key': 'wren|watch'}])
        people = d['people']
        self.assertEqual(people['friends'], [])
        self.assertEqual([x['id'] for x in people['documents']], ['L3'])
        self.assertEqual(people['projects'][0]['namedFor'], '')
        self.assertEqual(people['projects'][0]['gifts'], [{'who': 'wren', 'coins': 2}])
        self.assertEqual([s['id'] for s in people['storylines']], ['s3'])
        self.assertEqual(people['storylines'][0]['participants'], [{'id': 'wren'}])
        self.assertEqual(people['protectedMarks'], {'wren': True})
        self.assertEqual(d['chapters'], {'next': 2, 'chapters': []})

    def test_a_section_with_fixed_fields_keeps_no_player_in_them(self):
        doc = save(parties={'next': 2, 'parties': [], 'leader': WOLF, f'{WOLF}': 1})
        P.strip(doc)
        self.assertEqual(doc['parties'], {'next': 2, 'parties': []})

    def test_players_accounts_director_and_journal(self):
        self.assertEqual(self.doc['players'], [])
        self.assertNotIn('accounts', self.doc)
        self.assertNotIn('director', self.doc)
        self.assertEqual(self.doc['journal'], 0)
        self.assertEqual(self.st.usernames, {'ashley'})
        self.assertEqual(self.st.names, {'Ash Tallow', 'The Iron Fangs', "The Iron Fangs's camp"})
        self.assertGreater(self.st.found['society'], 0)

    def test_an_unknown_section_stops_it(self):
        with self.assertRaisesRegex(P.PackageError, 'newThing'):
            P.strip(save(newThing={}))
        doc = save()
        doc['people']['newList'] = []
        with self.assertRaisesRegex(P.PackageError, 'newList'):
            P.strip(doc)

    def test_the_treasury_keeps_within_its_goods_limits(self):
        doc = save()
        doc['society']['accounts']['treasury']['stock'] = {f'good{i}': 1 for i in range(63)}
        doc['society']['accounts'][WOLF2] = {'cash': 0, 'stock': {'a': 1, 'b': 2, 'good0': 9999}}
        st = P.strip(doc)
        treasury = doc['society']['accounts']['treasury']['stock']
        self.assertEqual(len(treasury), 64)
        self.assertEqual(treasury['good0'], 10000)
        self.assertGreater(st.discarded, 0)
        self.assertEqual(P.money(doc), [])


class ScanTest(unittest.TestCase):
    def clean(self):
        doc = save()
        P.strip(doc)
        return doc

    def test_a_planted_id_in_a_key(self):
        doc = self.clean()
        doc['talkMemory'][f'person:{WOLF}'] = {}
        errors, _ = P.scan(doc, {})
        self.assertTrue(any('talkMemory.person:wolf-' in e for e in errors), errors)

    def test_a_planted_id_in_a_nested_string(self):
        doc = self.clean()
        doc['npcs'][1]['deep'] = {'a': [{'b': f'told by {WOLF2}'}]}
        errors, _ = P.scan(doc, {})
        self.assertTrue(any('npcs[1].deep.a[0].b' in e for e in errors), errors)
        doc = self.clean()
        doc['weather']['x'] = 'player-ash'
        self.assertTrue(P.scan(doc, {})[0])
        doc = self.clean()
        doc['weather']['x'] = 'acct-' + 'c' * 32
        self.assertTrue(P.scan(doc, {})[0])
        doc = self.clean()
        doc['weather']['x'] = 'dev:wren'
        self.assertTrue(P.scan(doc, {})[0])

    def test_a_planted_id_in_a_row(self):
        rows = {'live.npcs': [{'id': 'wren', 'backstory': f'Wren once met {WOLF}.'}]}
        errors, _ = P.scan(None, rows)
        self.assertTrue(any(e.startswith('live.npcs[0].backstory') for e in errors), errors)

    def test_usernames_and_names(self):
        doc = self.clean()
        doc['weather']['who'] = 'ashley'
        doc['npcs'][1]['description'] = 'Bram remembers Ash Tallow fondly.'
        errors, warnings = P.scan(doc, {}, usernames={'ashley'}, names={'Ash Tallow'})
        self.assertTrue(any('weather.who' in e and 'username' in e for e in errors), errors)
        self.assertTrue(any('npcs[1].description' in w for w in warnings), warnings)
        doc = self.clean()
        doc['weather']['k'] = 'pair:wren|ashley'
        self.assertTrue(P.scan(doc, {}, usernames={'ashley'})[0])
        # A username that is also an item, a resident or a cell: a warning, not a failure.
        errors, warnings = P.scan(self.clean(), {}, usernames={'meal'})
        self.assertEqual(errors, [])
        self.assertTrue(warnings)

    def test_a_username_that_is_a_word_the_game_uses(self):
        """'night' (a lodging's period), 'guard', 'open': words in the save, so warnings; as an account, an error."""
        doc = self.clean()
        self.assertIn('night', P.vocabulary())
        self.assertEqual(doc['people']['lodgings'][0]['period'], 'night')
        errors, warnings = P.scan(doc, {}, usernames={'night'})
        self.assertEqual(errors, [])
        self.assertTrue(any('lodgings[0].period' in w and 'word' in w for w in warnings), warnings)
        doc['weather']['account'] = 'night'
        errors, _ = P.scan(doc, {}, usernames={'night'})
        self.assertEqual(errors, ['snapshot.weather.account: a source username (account)'])
        doc = self.clean()
        doc['weather']['friends'] = {'night': 1}
        self.assertEqual(P.scan(doc, {}, usernames={'night'})[0], [], 'a key of a map that is not of accounts')
        self.assertTrue(P.scan(doc, {}, usernames={'ashley'})[0] == [])
        doc['weather']['accounts'] = {'night': 1}
        self.assertTrue(P.scan(doc, {}, usernames={'night'})[0])
        # Core's other account fields (giverAccount, reportedAccount, otherAccount, ...): accounts too.
        for field in ('giverAccount', 'recipientAccount', 'reporterAccount', 'reportedAccount', 'otherAccount'):
            doc = self.clean()
            doc['weather'][field] = 'night'
            self.assertEqual(P.scan(doc, {}, usernames={'night'})[0],
                             [f'snapshot.weather.{field}: a source username (account)'], field)
        doc = self.clean()
        doc['weather']['circlesPerAccount'] = 4
        doc['weather']['otherAccounts'] = ['night']
        self.assertTrue(P.scan(doc, {}, usernames={'night'})[0])

    def test_dm_tool_accounts(self):
        rows = {'live.npc_voice': [{'npc_id': 'wren', 'written_by': 'dm:martinb'}, {'npc_id': 'bram', 'written_by': 'seed'}],
                'live.story_visitors': [{'id': 'v1', 'approved_by': 'martinb'}, {'id': 'v2', 'approved_by': None}],
                'live.faction_relations': [{'faction_id': 'a', 'updated_by': 'martinb'}]}
        errors, _ = P.scan(None, copy.deepcopy(rows), usernames={'martinb'})
        self.assertTrue(any('npc_voice[0].written_by: names a DM tool account' in e for e in errors), errors)
        self.assertTrue(any('story_visitors[0].approved_by: a source username' in e for e in errors), errors)
        self.assertTrue(any('faction_relations[0].updated_by' in e for e in errors), errors)
        shipped = {t: [P.without_provenance(t, r) for r in copy.deepcopy(rs)] for t, rs in rows.items()}
        self.assertEqual(P.scan(None, shipped, usernames={'martinb'})[0], [])
        self.assertEqual([r['written_by'] for r in shipped['live.npc_voice']], ['dm', 'seed'])
        self.assertEqual([r['approved_by'] for r in shipped['live.story_visitors']], ['package', None],
                         'an approved visitor stays approved')

    def test_money_and_goods(self):
        doc = self.clean()
        doc['society']['accounts']['wren']['cash'] += 1
        self.assertTrue(any(e.startswith('money:') for e in P.scan(doc, {})[0]))
        doc = self.clean()
        doc['society']['accounts']['bram']['stock'] = {f'g{i}': 1 for i in range(65)}
        self.assertTrue(any(e.startswith('goods:') for e in P.scan(doc, {})[0]))
        doc = self.clean()
        doc['society']['accounts']['bram']['stock'] = {'meal': 10001}
        self.assertTrue(any(e.startswith('goods:') for e in P.scan(doc, {})[0]))

    def test_tables_a_package_may_not_hold(self):
        errors, _ = P.scan(None, {'game.accounts': [], 'live.npcs': []}, allowed_tables=P.allowed_tables())
        self.assertEqual(errors, ['game.accounts: not a table a package may hold.'])


class JournalTest(unittest.TestCase):
    def test_replay_after_the_documents_own(self):
        doc = {'journal': 2, 'society': {'accounts': {'a': {'cash': 1}}}, 'players': [{'id': 'x', 'v': 1}]}
        records = [(2, '[{"set": ["society", "accounts", "a"], "value": {"cash": 99}}]'),
                   (3, '[{"set": ["society", "accounts", "a"], "value": {"cash": 5}}, {"erase": ["nothing", "here"]}]'),
                   (4, '[{"upsert": "players", "key": "x", "value": {"id": "x", "v": 2}},'
                       ' {"upsert": "players", "key": "y", "value": {"id": "y"}}, {"set": ["new", "k"], "value": 1}]')]
        self.assertEqual(P.journal_replay(doc, records), 4)
        self.assertEqual(doc['society']['accounts']['a'], {'cash': 5})
        self.assertEqual(doc['players'], [{'id': 'x', 'v': 2}, {'id': 'y'}])
        self.assertEqual(doc['new'], {'k': 1})

    def test_a_bad_record_refuses(self):
        for bad in ('not json', '{}', '[{"what": 1}]', '[{"set": []}]', '[{"upsert": "players", "key": "", "value": {}}]'):
            with self.assertRaises(P.PackageError):
                P.journal_replay({'journal': 0}, [(1, bad)])


def plant(source):
    """Players in DEV's save before the strip (doc 77's test: the strip's result must still load), in every part of the
    save the strip has a path for: a wolf with a purse and an account; a Chapter with its treasury, a camp (staffed,
    an offer out) and a lease; a town's own camp site with the granary a town project built on it (it must stay); a
    contract the wolf posted (escrow) and one a resident posted that the wolf took; a letter from the wolf and one to
    it (both with enclosures); the wolf's lodging (rent into a landlord's till, its chest let:) and stall (stall:);
    stakes on a table (table:); gifts to the project; storylines; a treaty, a mission and standings; a party with a
    companion; aliases, scenes heard, command and response receipts, a social scene and its audiences, a nickname and
    a deed; and the bonds, beliefs, apprentice, regard, talk memory, ledger entry and crime of the first version. Every
    coin planted comes out of the treasury, so the books still balance."""
    doc = source['snapshot']
    society = doc['society']
    accounts = society['accounts']
    residents = [e['id'] for e in doc['npcs'] if e['id'] in accounts]
    a, b, c = residents[0], residents[1], residents[2]
    cell = doc['npcs'][0]['cell']
    town = 'ridgemere'
    faction = source['live']['live.factions'][0]['id']
    till = next(k for k in accounts if k.startswith('till:'))
    planted = {'till': till, 'till_cash': accounts[till]['cash'], 'resident': a, 'resident_cash': accounts[a]['cash']}

    def account(name, coins, stock=None):
        accounts[name] = {'cash': coins, 'stock': dict(stock or {})}
        accounts['treasury']['cash'] -= coins

    account(WOLF, 30, {'meal': 2, 'herbs': 3})
    account('chapter:chapter-1', 10)
    account('contract:k-wolf', 25)                         # Posted by the wolf: goes.
    account('contract:k-res', 15)                          # Posted by a resident, taken by the wolf: stays, open.
    account(f'letter:doc-wolf@{WOLF}', 12)                 # The wolf's letter's enclosure: to the treasury.
    account(f'letter:doc-res@{a}', 8)                      # A resident's letter to the wolf: back to its writer.
    account('let:lodging-1', 3, {'candle_tallow': 2})      # The wolf's chest at its lodging.
    account('stall:st1', 4, {'herbs': 1})
    account('table:t1', 6)
    accounts[a].setdefault('stock', {})[f'sword@{WOLF}'] = 1
    doc['players'] = [{'id': WOLF, 'name': 'Ash Tallow', 'npc': False, 'cell': cell, 'x': 1, 'y': 1}]
    doc['accounts'] = {'version': 1, 'entries': [{'username': 'ashley', 'characters': [WOLF]}]}
    doc['standing'] = {'accounts': [{'account': 'ashley', 'characters': [WOLF], 'giftedAt': 1, 'giftedBy': 'earned'}]}
    doc['chapters']['chapters'] = [{'id': 'chapter-1', 'name': 'The Test', 'members': [{'id': WOLF, 'rank': 'head'}]}]
    # Camps: the town's (stays) and the Chapter's (goes, with its staff's "working at The Test's camp").
    doc['camps'] = {'next': 5,
                    'sites': [{'id': 'site-1', 'chapter': f'town:{town}', 'cell': cell, 'name': 'Ridgemere', 'x': 2, 'y': 2,
                               'founded': 0, 'visited': 0, 'state': 'standing'},
                              {'id': 'site-2', 'chapter': 'chapter-1', 'cell': cell, 'name': "The Test's camp", 'x': 6, 'y': 6,
                               'founded': 0, 'visited': 0, 'state': 'standing'}],
                    'structures': [{'id': 'structure-1', 'site': 'site-1', 'kind': 'granary', 'x': 2, 'y': 2, 'work': 40,
                                    'condition': 100, 'built': True, 'trade': ''},
                                   {'id': 'structure-2', 'site': 'site-2', 'kind': 'tent', 'x': 6, 'y': 6, 'work': 2,
                                    'condition': 100, 'built': True, 'trade': ''}],
                    'staff': [{'npc': c, 'site': 'site-2', 'role': 'hand', 'wage': 2, 'paidTo': 0, 'arriving': False, 'since': 0}],
                    'offers': [{'npc': b, 'site': 'site-2', 'role': 'hand', 'expires': 99}],
                    'offered': {'site-2': 0}}
    next(e for e in doc['npcs'] if e['id'] == c)['activity'] = "working at The Test's camp"
    people = doc['people']
    people['projects'] = [{'id': 'proj-1', 'kind': 'granary', 'town': town, 'cell': cell, 'title': 'the Ridgemere granary',
                           'state': 'built', 'by': f'town:{town}', 'site': 'site-1', 'structure': 'structure-1',
                           'namedFor': WOLF, 'x': 2, 'y': 2, 'hours': 40, 'worked': 40, 'posted': 0, 'finished': 0,
                           'condition': 100, 'needs': {}, 'shown': {WOLF: 'the granary'},
                           'gifts': [{'who': WOLF, 'kind': 'coins', 'item': '', 'amount': 3, 'value': 3, 'day': 0},
                                     {'who': a, 'kind': 'coins', 'item': '', 'amount': 2, 'value': 2, 'day': 0}]}]
    doc['roads']['contracts'] += [
        {'id': 'k-wolf', 'kind': 'courier', 'poster': WOLF, 'town': town, 'target': a, 'taker': '', 'status': 'open',
         'reward': 25, 'created': 0, 'due': 30, 'detail': ''},
        {'id': 'k-res', 'kind': 'courier', 'poster': a, 'town': town, 'target': b, 'taker': WOLF, 'status': 'taken',
         'reward': 15, 'created': 0, 'due': 30, 'detail': '', 'offeredTo': WOLF, 'offeredUntil': 5}]
    people['documents'] += [
        {'id': 'doc-wolf', 'kind': 'letter', 'author': WOLF, 'to': a, 'text': 'Dear friend', 'fromTown': town,
         'postTown': town, 'state': 'posted', 'written': 0, 'deliverAt': 1},
        {'id': 'doc-res', 'kind': 'letter', 'author': a, 'to': WOLF, 'text': 'Dear Ash', 'fromTown': town,
         'postTown': town, 'state': 'posted', 'written': 0, 'deliverAt': 1}]
    landlord = till.split(':')[-1]
    people['lodgings'] += [{'id': 'lodging-1', 'holder': WOLF, 'cell': cell, 'kind': 'place', 'landlord': landlord,
                            'account': till, 'period': 'week', 'town': town, 'x': 3, 'y': 3, 'rent': 5, 'paidTo': 7,
                            'noticeUntil': -1, 'open': False, 'guests': [b]}]
    people['stalls'] += [{'id': 'st1', 'keeper': WOLF, 'community': town, 'cell': cell, 'x': 4, 'y': 4, 'day': 0, 'fee': 2,
                          'takings': 0, 'closed': False, 'prices': {'herbs': 3}}]
    people['storylines'] += [
        {'id': 'story-1', 'kind': 'personal', 'owner': WOLF, 'authorAccount': 'ashley', 'title': "Ash's road",
         'state': 'live', 'steps': [], 'participants': {WOLF: {'joined': 0, 'left': -1}}},
        {'id': 'tale-2', 'kind': 'tale', 'owner': '', 'title': 'The market tale', 'state': 'ended', 'steps': [],
         'participants': {WOLF: {'joined': 0, 'left': -1}, a: {'joined': 0, 'left': -1}}, 'tracking': [WOLF]}]
    people['nicknames'] += [{'id': 'nick-1', 'wolf': WOLF, 'text': 'Ash the Quick', 'family': 'deed', 'deed': 'deed-1',
                             'coinedBy': a, 'town': town, 'day': 0, 'dropped': False}]
    people['deeds'] += [{'id': 'deed-1', 'kind': 'rescue', 'weight': 1, 'doers': [WOLF], 'beneficiary': a, 'town': town,
                         'cell': cell, 'place': '', 'detail': '', 'source': 'seen', 'x': 1, 'y': 1, 'day': 0}]
    people['friends'] = [{'account': 'ashley', 'friend': 'bramble'}]
    factions = doc['factions']
    factions['treaties'] += [{'id': 'treaty-1', 'faction': faction, 'chapter': 'chapter-1', 'build': True, 'tithe': 1,
                              'levy': 0, 'labour': 0, 'weeks': 4, 'state': 'active', 'proposed': 0, 'started': 0, 'paidTo': 0}]
    factions['missions'] += [{'id': 'mission-1', 'faction': faction, 'kind': 'deliver', 'tier': 1, 'item': 'meal',
                              'quantity': 1, 'official': a, 'to': b, 'cell': cell, 'place': '', 'seconds': 0, 'held': 0,
                              'coins': 4, 'standing': 1, 'renown': 0, 'chapter': '', 'taker': WOLF, 'state': 'taken',
                              'expires': 99}]
    factions['standings'] += [{'key': f'{WOLF}|{faction}', 'earned': 5, 'stance': 'friendly', 'week': 0, 'weekly': {},
                               'log': []},
                              {'key': f'chapter-1|{faction}', 'earned': 5, 'stance': 'friendly', 'week': 0, 'weekly': {},
                               'log': []}]
    doc['estates']['leases'] += [{'property': cell, 'chapter': 'chapter-1', 'landlord': 'treasury', 'rent': 5, 'started': 0,
                                  'paidTo': 7, 'state': 'active', 'guests': [WOLF], 'notices': []}]
    doc['parties'] = {'next': 2, 'parties': [{'id': 'party-1', 'leader': WOLF, 'members': [{'id': WOLF}],
                                              'companions': [{'id': b, 'by': WOLF, 'reason': 'friend', 'wage': 0,
                                                              'joined': 0, 'paidThrough': 0, 'waiting': False}]}],
                      'neverAutoJoin': [WOLF]}
    doc['companions'] = {b: WOLF}
    doc['aliases'] = {WOLF: ['Ashy']}
    doc['scenesHeard'] = {WOLF: ['scene-x']}
    doc['commandReceipts'] = {WOLF: ['cmd-1', 'cmd-2']}
    doc['responseReceipts'] = {WOLF: {'cmd-1': '{"ok":true}'}}
    doc['socialSessions'].append({'id': 'sess-wolf', 'cell': cell, 'started': 0, 'last': 0, 'ended': 0, 'openness': 'open',
                                  'opennessAt': 0, 'members': [{'actor': WOLF, 'turns': 1, 'words': 3, 'replies': 0,
                                                                'last': 0, 'joined': 0},
                                                               {'actor': a, 'turns': 1, 'words': 3, 'replies': 0,
                                                                'last': 0, 'joined': 0}]})
    doc['socialAudiences'] = {**doc.get('socialAudiences', {}), f'sess-wolf|{WOLF}': [a], f'sess-wolf|{a}': [WOLF]}
    doc['bonds'].append({'holder': a, 'other': WOLF, 'affinity': 0.5, 'trust': 0.1, 'familiarity': 1, 'fear': 0,
                         'respect': 0, 'owed': 0, 'lastContact': 0.5})
    doc['beliefs'].append({'holder': b, 'subject': WOLF, 'claim': 'thief', 'source': a, 'confidence': 0.5, 'day': 0})
    doc['regardLedger'][f'{a}|{WOLF}'] = [{'day': 0, 'source': 'talk', 'reason': 'x', 'change': [0, 0, 0, 0, 0]}]
    doc['talkMemory'][f'person:{WOLF}'] = {'greet': ['hello', 0]}
    doc['acquaintances'][a] = [{'id': WOLF, 'names': ['Ash'], 'how': 'introduced', 'day': 0}]
    doc['npcs'][0]['witnessed'] = {WOLF: 0}
    doc['npcs'][0]['toldWardens'] = [WOLF]
    position = next(iter(society['careers']['positions'].values()))
    position['apprentice'] = WOLF
    society['ledger'].append({'sequence': society['nextEntry'], 'from': WOLF, 'to': a, 'coins': 1, 'day': 0,
                              'item': '', 'quantity': 0, 'kind': 'a sale'})
    society['nextEntry'] += 1
    society['ledger'] = society['ledger'][-128:]
    doc['crime']['incidents'].append({'id': 1, 'kind': 'theft', 'offender': b, 'victim': WOLF, 'cell': '', 'town': '',
                                      'time': 0, 'day': 0, 'item': '', 'quantity': 0, 'coins': 0, 'status': 'open',
                                      'witnesses': []})
    assert P.money(doc) == [], P.money(doc)
    source['planted'] = planted


@unittest.skipUnless(database_available() and SERVER, 'local PostgreSQL not running, or ratw_server not built')
class EndToEndTest(unittest.TestCase):
    """DEV exported read only, imported into throwaway worlds, loaded by a real server."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory(prefix='pkg-test-')
        base = Path(cls.folder.name)
        registry = base / 'worlds.json'
        registry.write_text(W.WORLDS_FILE.read_text(encoding='utf-8'), encoding='utf-8')
        cls.saved_env = dict(os.environ)
        os.environ.update({'RATW_WORLDS_FILE': str(registry), 'RATW_WORLDS_LOCAL': str(base / 'local.json')})
        cls.tag = secrets.token_hex(3)
        cls.planting = {}
        cls.keys = []
        cls.out = base / 'dev'
        cls.planted = base / 'planted'
        cls.manifest = P.export('dev', cls.out, log=lambda *_: None)
        cls.planted_manifest = P.export('dev', cls.planted, log=lambda *_: None,
                                        transform=lambda source: (plant(source), cls.planting.update(source['planted'])))

    @classmethod
    def tearDownClass(cls):
        from psycopg import sql
        with superuser() as su:
            for key in cls.keys:
                su.execute(sql.SQL('DROP DATABASE IF EXISTS {} WITH (FORCE)').format(sql.Identifier(P.database_of(key))))
        os.environ.clear()
        os.environ.update(cls.saved_env)
        cls.folder.cleanup()
        W.worlds()

    def key(self, n):
        key = f'{KEY_PREFIX}{self.tag}{n}'
        type(self).keys.append(key)
        return key

    def run_server(self, key, seconds=12):
        env = dict(os.environ, RATW_DATABASE_URL=W.libpq_conninfo(key, 'game'), RATW_AI='off')
        done = subprocess.run([str(SERVER), '--database', key, '--world-kind', 'live', '--bind', '127.0.0.1', '--port', '0',
                               '--for', str(seconds), '--talk-data', str(ROOT / 'Data' / 'Talk')],
                              env=env, capture_output=True, text=True, timeout=seconds + 120)
        return done.returncode, done.stdout + done.stderr

    def stored_save(self, key):
        with W.connect(key, 'owner') as conn:
            world_id = conn.execute('SELECT id FROM world.worlds').fetchone()[0]
            return json.loads(conn.execute('SELECT game.load_checkpoint(%s)', (world_id,)).fetchone()[0])

    def test_the_export(self):
        m = self.manifest
        self.assertEqual(m['format'], 1)
        self.assertEqual(m['source'], 'dev')
        self.assertTrue(m['snapshot'])
        self.assertEqual(m['protocol'], 1)
        self.assertTrue(re.fullmatch(r'[0-9a-f]{32}', m['client_data'] or ''))
        for name in P.PARTS + (P.MANIFEST, P.SUMS):
            self.assertTrue((self.out / name).is_file(), name)
        self.assertEqual(P.scan_directory(self.out)[0], [])
        listed = {line.split('  ')[1] for line in (self.out / P.SUMS).read_text().splitlines()}
        self.assertEqual(listed, set(P.PARTS) | {P.MANIFEST})
        self.assertFalse(any(p.name.startswith('.') for p in Path(self.folder.name).iterdir()), 'no partial left')
        self.assertEqual(P.main(['scan', str(self.out)]), 0)

    def test_the_planted_players_are_stripped(self):
        strip = self.planted_manifest['strip']
        # The wolf's purse, the Chapter's, an escrow of each kind (contract, two letters, the chest, the stall, the table).
        self.assertEqual(strip['accounts_closed'], 8)
        for section in ('camps', 'chapters', 'commandReceipts', 'responseReceipts', 'scenesHeard', 'aliases', 'companions',
                        'socialAudiences', 'socialSessions', 'parties', 'standing', 'factions', 'estates', 'roads', 'people',
                        'npcs', 'society', 'players'):
            self.assertIn(section, strip['removed'], section)
        self.assertEqual(P.scan_directory(self.planted)[0], [])
        snapshot = P.read_json_gz(self.planted / 'snapshot.json.gz')
        self.assertNotIn(WOLF, json.dumps(snapshot))
        self.assertNotIn('chapter-1', json.dumps(snapshot))
        self.assertNotIn('accounts', snapshot)
        accounts = snapshot['society']['accounts']
        for gone in (WOLF, 'chapter:chapter-1', 'contract:k-wolf', f'letter:doc-wolf@{WOLF}', 'let:lodging-1', 'stall:st1',
                     'table:t1'):
            self.assertNotIn(gone, accounts)
        self.assertIn('contract:k-res', accounts, "a resident's contract keeps its escrow")
        self.assertFalse([k for k in accounts if k.startswith('letter:doc-')])
        resident = self.planting['resident']
        self.assertEqual(accounts[resident]['cash'], self.planting['resident_cash'] + 8, "the resident's enclosure back")
        till = self.planting['till']
        self.assertEqual(accounts[till]['cash'], self.planting['till_cash'], "the landlord's till is left alone")
        contracts = [k for k in snapshot['roads']['contracts'] if k['id'] in ('k-wolf', 'k-res')]
        self.assertEqual([(k['id'], k['poster'], k['taker'], k['status'], 'offeredTo' in k) for k in contracts],
                         [('k-res', resident, '', 'open', False)])
        camps = snapshot['camps']
        self.assertEqual([s['id'] for s in camps['sites']], ['site-1'])
        self.assertEqual([s['id'] for s in camps['structures']], ['structure-1'])
        self.assertEqual((camps['staff'], camps['offers'], camps['offered']), ([], [], {}))
        self.assertEqual([p['id'] for p in snapshot['people']['projects']], ['proj-1'])
        self.assertEqual([s['id'] for s in snapshot['people']['storylines'] if s['id'] in ('story-1', 'tale-2')], ['tale-2'])
        for key in ('commandReceipts', 'responseReceipts', 'scenesHeard', 'aliases', 'companions'):
            self.assertEqual(snapshot[key], {}, key)
        self.assertFalse([e for e in snapshot['npcs'] if e.get('activity') == "working at The Test's camp"])

    def test_a_damaged_package_is_refused(self):
        damaged = Path(self.folder.name) / 'damaged'
        damaged.mkdir()
        for p in self.out.iterdir():
            (damaged / p.name).write_bytes(p.read_bytes())
        with (damaged / 'live.json.gz').open('ab') as f:
            f.write(b'x')
        with self.assertRaisesRegex(P.PackageError, 'checksum'):
            P.verify(damaged)
        self.assertEqual(P.main(['scan', str(damaged)]), 1)

    def test_an_export_that_fails_its_scan_leaves_nothing(self):
        target = Path(self.folder.name) / 'refused'

        def leak(source):
            source['live']['live.npcs'][0]['backstory'] = f'Met {WOLF2} once.'
        with self.assertRaisesRegex(P.PackageError, 'scan failed'):
            P.export('dev', target, log=lambda *_: None, transform=leak)
        self.assertFalse(target.exists())
        self.assertFalse(any(p.name.startswith('.refused') for p in Path(self.folder.name).iterdir()))
        with self.assertRaisesRegex(P.PackageError, 'isn\'t empty'):
            P.export('dev', self.out, log=lambda *_: None)

    def check_loaded(self, key, directory, planted=False):
        logs = []
        self.assertEqual(P.import_package(directory, key, log=logs.append), 'imported', logs)
        self.assertEqual(P.import_package(directory, key, log=logs.append), 'already')
        before = self.stored_save(key)
        self.assertEqual(P.money(before), [])
        with W.connect(key, 'owner') as conn:        # (Before the server plays: it writes events and NPC states.)
            for table in ('game.journal', 'game.events', 'game.artwork', 'game.reports', 'game.accounts',
                          'game.characters', 'live.npc_state', 'live.game_changes', 'dm.stories', 'dm.admins',
                          'admin.releases'):
                self.assertEqual(conn.execute(f'SELECT count(*) FROM {table}').fetchone()[0], 0, table)
        status, log = self.run_server(key)
        self.assertEqual(status, 0, log[-3000:])
        self.assertIn('RATW_DATABASE_WORLD_LOADED', log)
        self.assertIn('RATW_RESTORE characters=0', log, log[-3000:])
        self.assertNotIn('rejected', log)
        self.assertNotIn('autosave disabled', log)
        after = self.stored_save(key)                 # (Saved again by the server as it stopped.)
        self.assertEqual(P.money(after), [], 'money conserved after the server played it')
        society = after['society']
        self.assertEqual(sum(a['cash'] for a in society['accounts'].values()), society['minted'] - society['sunk'])
        self.assertEqual(P.scan(after, {})[0], [])
        if planted:
            # The town's site and the granary its project built are still on the ground after the server played it.
            sites = {s['id']: s for s in after['camps']['sites']}
            self.assertEqual(sites.get('site-1', {}).get('chapter'), 'town:ridgemere', after['camps'])
            self.assertIn('structure-1', {s['id'] for s in after['camps']['structures'] if s['site'] == 'site-1'})
            project = next(p for p in after['people']['projects'] if p['id'] == 'proj-1')
            self.assertEqual((project['site'], project['structure']), ('site-1', 'structure-1'))
            self.assertIn(self.planting['till'], after['society']['accounts'])
        return before, log

    def test_imported_and_loaded_by_a_real_server(self):
        key = self.key('a')
        before, _ = self.check_loaded(key, self.out)
        with W.connect(key, 'owner') as conn:
            self.assertEqual(conn.execute("SELECT value->>'id' FROM admin.settings WHERE key = 'package'").fetchone()[0],
                             self.manifest['id'])
            self.assertEqual(conn.execute('SELECT count(*) FROM world.build_cells').fetchone()[0],
                             self.manifest['build']['cells'])
            self.assertEqual(conn.execute('SELECT count(*) FROM live.npcs').fetchone()[0],
                             self.manifest['rows']['live.npcs'])
        # An update's image carries another package: the world keeps its own state (doc 77, 8; exit 0, the server
        # starts on it).
        logs = []
        self.assertEqual(P.import_package(self.planted, key, log=logs.append), 'kept')
        self.assertTrue(any('keeps its own state' in line and 'docker compose down -v' in line
                            and self.planted_manifest['version'] in line for line in logs), logs)
        self.assertEqual(P.main(['import', str(self.planted), '--key', key]), 0)
        with W.connect(key, 'owner') as conn:
            self.assertEqual(conn.execute("SELECT value->>'id' FROM admin.settings WHERE key = 'package'").fetchone()[0],
                             self.manifest['id'], 'the package it was started from')
        after = self.stored_save(key)
        self.assertNotIn('sess-wolf', json.dumps(after['socialSessions']), "the new package's save was not applied")

    def test_the_planted_players_strip_loads_too(self):
        self.check_loaded(self.key('b'), self.planted, planted=True)

    def test_a_world_no_package_made_is_refused(self):
        key = self.key('d')
        self.assertEqual(P.import_package(self.out, key, log=lambda *_: None), 'imported')
        with W.connect(key, 'owner') as conn:
            conn.execute("DELETE FROM admin.settings WHERE key = 'package'")
        with self.assertRaisesRegex(P.PackageError, 'no package made'):
            P.import_package(self.out, key, log=lambda *_: None)
        self.assertEqual(P.main(['import', str(self.out), '--key', key]), 1)

    def test_an_import_stopped_part_way_completes_on_a_rerun(self):
        """A key the registry lacks, the process killed (no cleanup runs) after some of the migrations: the rerun finds
        the key registered and the database there, and finishes."""
        key = self.key('e')
        code = ('import os, sys\n'
                f'sys.path.insert(0, {str(ROOT / "tools")!r})\n'
                'from pathlib import Path\n'
                'import world_db as W, world_package as P\n'
                'real = W.migrate\n'
                'def partial(conn, files=None):\n'
                '    real(conn, W.migration_files()[:20])\n'
                '    os._exit(3)\n'
                'W.migrate = partial\n'
                'P.import_package(Path(sys.argv[1]), sys.argv[2], log=lambda *_: None)\n')
        done = subprocess.run([sys.executable, '-c', code, str(self.out), key], capture_output=True, text=True, timeout=300)
        self.assertEqual(done.returncode, 3, done.stdout + done.stderr)
        W.worlds()
        self.assertIn(key, W.keys(), 'registered before its database was made')
        self.assertTrue(P._database_exists(W.world(key)['database']))
        self.assertEqual(P.import_package(self.out, key, log=lambda *_: None), 'imported')
        with W.connect(key, 'owner') as conn:
            self.assertEqual(len(W.applied_migrations(conn)), len(W.migration_files()))
            self.assertEqual(conn.execute("SELECT value->>'id' FROM admin.settings WHERE key = 'package'").fetchone()[0],
                             self.manifest['id'])

    def test_an_unregistered_key_whose_database_holds_a_world_is_refused_untouched(self):
        """A database ratw_<key> left by another registry or checkout: refused before anything is done to it (no
        migration, no grant), and the key not left registered."""
        key = self.key('f')
        self.assertEqual(P.import_package(self.out, key, log=lambda *_: None), 'imported')
        W.unregister(key)
        before = W.registry_file().read_text()
        with mock.patch.object(W, 'migrate', side_effect=AssertionError('migrated')), \
                mock.patch.object(W, 'ensure_roles', side_effect=AssertionError('granted')):
            with self.assertRaisesRegex(P.PackageError, 'already exists and holds package'):
                P.import_package(self.out, key, log=lambda *_: None)
            with superuser(P.database_of(key)) as su:        # (By its database's name: the key is not registered.)
                su.execute("DELETE FROM admin.settings WHERE key = 'package'")
            with self.assertRaisesRegex(P.PackageError, 'already exists and holds world'):
                P.import_package(self.out, key, log=lambda *_: None)
        self.assertEqual(W.registry_file().read_text(), before, 'the registry as it was')
        self.assertNotIn(key, W.keys())
        self.assertEqual(P.main(['import', str(self.out), '--key', key]), 1)
        self.assertNotIn(key, W.keys())

    def test_a_failed_import_of_a_new_key_leaves_nothing(self):
        """A key the registry lacked, the import failing after its database was made: unregistered and dropped again
        (as new_world registers only on success). An empty database already there (an import stopped part way, then
        unregistered) is completed."""
        key = self.key('g')
        before = W.worlds()
        with mock.patch.object(P, 'import_build', side_effect=P.PackageError('a build that fails')):
            with self.assertRaisesRegex(P.PackageError, 'a build that fails'):
                P.import_package(self.out, key, log=lambda *_: None)
        self.assertEqual(W.worlds(), before, 'the registry as it was')
        self.assertFalse(P._database_exists(P.database_of(key)))
        from psycopg import sql
        with superuser() as su:
            su.execute(sql.SQL('CREATE DATABASE {} OWNER ratw_owner').format(sql.Identifier(P.database_of(key))))
        logs = []
        self.assertEqual(P.import_package(self.out, key, log=logs.append), 'imported', logs)
        self.assertTrue(any('stopped part way' in line for line in logs), logs)
        self.assertIn(key, W.keys())

    def test_a_one_world_registry_is_written(self):
        """As in the package's container: RATW_WORLDS_FILE names a registry not there yet."""
        key = self.key('c')
        registry = Path(self.folder.name) / 'one' / 'worlds.json'
        saved = os.environ['RATW_WORLDS_FILE']
        os.environ['RATW_WORLDS_FILE'] = str(registry)
        try:
            self.assertEqual(P.main(['import', str(self.out), '--key', key, '--name', 'Hosted']), 0)
            entries = json.loads(registry.read_text())['worlds']
            self.assertEqual(entries, [{'key': key, 'name': 'Hosted', 'database': P.database_of(key), 'kind': 'live'}])
            self.assertEqual(P.main(['import', str(self.out), '--key', key]), 0)
        finally:
            os.environ['RATW_WORLDS_FILE'] = saved
            W.worlds()


if __name__ == '__main__':
    unittest.main()
