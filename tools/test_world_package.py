#!/usr/bin/env python3
"""The world package (tools/world_package.py; doc 77, section 8 and Progress, "Phase 5's plan"): the strip on small
hand-made saves (money conserved, escrow, maker marks, compound keys, unknown sections refused), the scan (an id
planted in a key, in a nested string, in a row; usernames; money; goods), the journal replay, and end to end: DEV
exported read only (one read-only transaction), scanned, imported into throwaway worlds and loaded by a real
ratw_server there, once as it is and once with players planted in its save before the strip.

The end-to-end tests need the local PostgreSQL and a built build-central (or build-core) ratw_server; they are skipped
without them. The throwaway databases ("pkg" in their names) are dropped afterwards. DEV is only read."""
import copy
import json
import os
from pathlib import Path
import re
import secrets
import subprocess
import tempfile
import time
import unittest

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
                  'postureTarget': WOLF, 'scents': [{'item': 'pelt@' + WOLF, 'maker': WOLF, 'count': 1}]},
                 {'id': 'bram', 'name': 'Bram', 'npc': True}],
        'society': {
            'minted': 1000, 'sunk': 100, 'nextEntry': 10, 'budgetDay': 4,
            'accounts': {
                'treasury': {'cash': 500, 'stock': {'meal': 5}},
                'wren': {'cash': 100, 'stock': {'sword@' + WOLF: 2, 'sword': 1, 'pelt@wren': 1}},
                'bram': {'cash': 50, 'stock': {}},
                WOLF: {'cash': 120, 'stock': {'herbs': 4, 'meal@' + WOLF: 2}},
                'chapter:chapter-1': {'cash': 40, 'stock': {'rope': 1}},
                'contract:k1': {'cash': 30, 'stock': {}},          # Escrow of a contract a player posted.
                f'letter:L1@{WOLF}': {'cash': 20, 'stock': {}},     # A player's letter's enclosure.
                'letter:L2@bram': {'cash': 15, 'stock': {}},        # A resident's letter to a player: back to Bram.
                'let:lodge1': {'cash': 0, 'stock': {'candle': 2}},
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
        'talkMemory': {f'pair:wren|{WOLF}': {'a': ['hello', 1]}, 'person:wren': {'b': ['hi', 2]}},
        'regardLedger': {f'wren|{WOLF}': [], 'wren|bram': [{'day': 1, 'source': WOLF}, {'day': 2, 'source': 'bram'}]},
        'acquaintances': {'wren': [{'id': WOLF, 'names': ['Ash']}, {'id': 'bram', 'names': ['Bram']}],
                          WOLF: [{'id': 'wren', 'names': ['Wren']}]},
        'olderMemoryEvents': {f'wren|{WOLF}': [1, 2]},
        'chapters': {'next': 2, 'chapters': [{'id': 'chapter-1', 'members': [{'id': WOLF}]}]},
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
                   'lodgings': [{'id': 'lodge1', 'holder': WOLF, 'account': 'let:lodge1'}],
                   'stalls': [{'id': 's1', 'keeper': WOLF}],
                   'documents': [{'id': 'L1', 'author': WOLF, 'to': 'wren'}, {'id': 'L2', 'author': 'bram', 'to': WOLF},
                                 {'id': 'L3', 'author': 'bram', 'to': 'wren'}],
                   'projects': [{'id': 'p1', 'namedFor': WOLF, 'gifts': [{'who': WOLF, 'coins': 3}, {'who': 'wren', 'coins': 2}]}],
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
        self.assertEqual(cash(self.doc), 1000 - 100)
        self.assertEqual(P.money(self.doc), [])
        # Player 120, the Chapter 40, the contract 30, the player's letter 20, the stall 5, the table 20.
        self.assertEqual(self.accounts['treasury']['cash'], 500 + 120 + 40 + 30 + 20 + 5 + 20)
        self.assertEqual(self.accounts['bram']['cash'], 50 + 15, "a resident's letter to a player: back to its writer")

    def test_escrow_accounts_close(self):
        for name in (WOLF, 'chapter:chapter-1', 'contract:k1', f'letter:L1@{WOLF}', 'letter:L2@bram', 'let:lodge1',
                     'stall:s1', 'table:t1'):
            self.assertNotIn(name, self.accounts)
        self.assertEqual(set(self.accounts), {'treasury', 'wren', 'bram'})
        self.assertEqual(self.st.closed, 8)

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

    def test_players_accounts_director_and_journal(self):
        self.assertEqual(self.doc['players'], [])
        self.assertNotIn('accounts', self.doc)
        self.assertNotIn('director', self.doc)
        self.assertEqual(self.doc['journal'], 0)
        self.assertEqual(self.st.usernames, {'ashley'})
        self.assertEqual(self.st.names, {'Ash Tallow'})
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
    """Players in DEV's save before the strip (doc 77's test: the strip's result must still load): a wolf with a
    purse taken from the treasury, a Chapter with its treasury, a maker-marked sword in a resident's purse, a bond, a
    belief, an apprentice, regard and talk memory, a ledger entry, a crime against the wolf."""
    doc = source['snapshot']
    society = doc['society']
    accounts = society['accounts']
    npcs = [e['id'] for e in doc['npcs']]
    a, b = npcs[0], npcs[1]
    accounts['treasury']['cash'] -= 40
    accounts[WOLF] = {'cash': 30, 'stock': {'meal': 2, 'herbs': 3}}
    accounts['chapter:chapter-1'] = {'cash': 10, 'stock': {}}
    accounts[a].setdefault('stock', {})[f'sword@{WOLF}'] = 1
    doc['players'] = [{'id': WOLF, 'name': 'Ash Tallow', 'npc': False, 'cell': doc['npcs'][0]['cell'], 'x': 1, 'y': 1}]
    doc['accounts'] = {'version': 1, 'entries': [{'username': 'ashley', 'characters': [WOLF]}]}
    doc['chapters']['chapters'] = [{'id': 'chapter-1', 'name': 'The Test', 'members': [{'id': WOLF, 'rank': 'head'}]}]
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
    source['planted'] = True


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
        cls.keys = []
        cls.out = base / 'dev'
        cls.planted = base / 'planted'
        cls.manifest = P.export('dev', cls.out, log=lambda *_: None)
        cls.planted_manifest = P.export('dev', cls.planted, log=lambda *_: None, transform=plant)

    @classmethod
    def tearDownClass(cls):
        with superuser() as su:
            for key in cls.keys:
                su.execute(f'DROP DATABASE IF EXISTS ratw_{key} WITH (FORCE)')
        os.environ.clear()
        os.environ.update(cls.saved_env)
        cls.folder.cleanup()
        W.worlds()

    def key(self, n):
        key = f'pkg{self.tag}{n}'
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
        self.assertGreater(strip['accounts_closed'], 1)
        self.assertGreater(sum(strip['removed'].values()), 8)
        self.assertEqual(P.scan_directory(self.planted)[0], [])
        snapshot = P.read_json_gz(self.planted / 'snapshot.json.gz')
        self.assertNotIn(WOLF, json.dumps(snapshot))
        self.assertNotIn('accounts', snapshot)

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

    def check_loaded(self, key, directory):
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
        # Another package into the same world: refused.
        with self.assertRaisesRegex(P.PackageError, 'another package'):
            P.import_package(self.planted, key, log=lambda *_: None)

    def test_the_planted_players_strip_loads_too(self):
        self.check_loaded(self.key('b'), self.planted)

    def test_a_one_world_registry_is_written(self):
        """As in the package's container: RATW_WORLDS_FILE names a registry not there yet."""
        key = self.key('c')
        registry = Path(self.folder.name) / 'one' / 'worlds.json'
        saved = os.environ['RATW_WORLDS_FILE']
        os.environ['RATW_WORLDS_FILE'] = str(registry)
        try:
            self.assertEqual(P.main(['import', str(self.out), '--key', key, '--name', 'Hosted']), 0)
            entries = json.loads(registry.read_text())['worlds']
            self.assertEqual(entries, [{'key': key, 'name': 'Hosted', 'database': f'ratw_{key}', 'kind': 'live'}])
            self.assertEqual(P.main(['import', str(self.out), '--key', key]), 0)
        finally:
            os.environ['RATW_WORLDS_FILE'] = saved
            W.worlds()


if __name__ == '__main__':
    unittest.main()
