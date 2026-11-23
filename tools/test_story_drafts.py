"""The Story Creator's workbench on the server (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 4): drafts kept
on the server (migration 0073, tools/story_drafts.py) with their versions and a save over a newer one refused; the round
trip (save, open, compare) of every bundled example and every library draft, key order and all; the pickers' `refs`;
and a rehearsal stepped through with what the phase waits for. In a throwaway migrated database, and through the DM
tool's route with a DM, a builder and a viewer. Skipped without the local PostgreSQL."""

import json
import threading
import unittest
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer

import dungeon_master as D
import story_drafts as drafts
import story_library
import story_program as rules
import story_runner as runner
import world_db as W
import test_story_console as console
from test_world_db import database_available


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class Drafts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        console.ConsoleReads.setUpClass.__func__(cls)
        with console.admin(cls.name) as owner:
            owner.execute('SET ROLE ratw_dm')
            D.create_account(owner, 'wb-dm', 'dm', 'a long dm password')
            D.create_account(owner, 'wb-dm2', 'dm', 'another dm password')
            D.create_account(owner, 'wb-builder', 'builder', 'a long builder password')
            D.create_account(owner, 'wb-viewer', 'viewer', 'a long viewer password')
        dm = D.DungeonMaster(connect=lambda target: W.connect('dev', 'dm', dbname=cls.name), worlds=lambda: ['prod', 'dev'])
        cls.server = ThreadingHTTPServer(('127.0.0.1', 0), D.make_handler(dm, worlds=lambda: ['dev'], default=lambda: 'dev'))
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()
        cls.port = cls.server.server_port

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        console.ConsoleReads.tearDownClass.__func__(cls)

    def setUp(self):
        self.conn = console.admin(self.name)
        self.addCleanup(self.conn.close)
        self.conn.execute('SET LOCAL ROLE ratw_dm')
        self.examples = runner.catalogue()['examples']

    def test_the_round_trip_keeps_every_program_as_it_was(self):
        programs = {**{'example/' + k: v for k, v in self.examples.items()},
                    **{'library/' + e['id']: story_library.draft(e['id']) for e in story_library.entries()}}
        self.assertEqual(len(programs), 108)
        for name, p in programs.items():
            saved = drafts.draft_save(self.conn, self.world, 'martin', {'name': name, 'program': p})
            opened = drafts.draft_open(self.conn, self.world, {'id': saved['id']})
            self.assertEqual(opened['program'], p, name)
            self.assertEqual(json.dumps(opened['program']), json.dumps(p), name + ': key order kept (json, not jsonb)')
            self.assertEqual(rules.digest(opened['program']), rules.digest(p))
        listed = drafts.draft_list(self.conn, self.world)['drafts']
        self.assertEqual(len(listed), 108)
        self.assertNotIn('program', listed[0])
        self.assertTrue(all(d['owner'] == 'martin' and d['version'] == 1 for d in listed))

    def test_versions_a_save_over_a_newer_one_is_refused_and_the_newest_twenty_are_kept(self):
        p = self.examples['reed-ford']
        first = drafts.draft_save(self.conn, self.world, 'martin', {'name': 'Reed Ford', 'program': p})
        second = drafts.draft_save(self.conn, self.world, 'ann', {'id': first['id'], 'version': 1, 'name': 'Reed Ford', 'program': {**p, 'title': 'Reed Ford, again'}})
        self.assertEqual(second['version'], 2)
        with self.assertRaisesRegex(ValueError, r'ann saved this draft at .* \(version 2\) after you opened it'):
            drafts.draft_save(self.conn, self.world, 'martin', {'id': first['id'], 'version': 1, 'name': 'Reed Ford', 'program': p})
        opened = drafts.draft_open(self.conn, self.world, {'id': first['id']})
        self.assertEqual((opened['program']['title'], opened['savedBy'], [v['version'] for v in opened['versions']]), ('Reed Ford, again', 'ann', [2, 1]))
        older = drafts.draft_open(self.conn, self.world, {'id': first['id'], 'version': 1})
        self.assertEqual((older['program']['title'], older['openedVersion']), (p['title'], 1))
        version = 2
        for n in range(25):
            version = drafts.draft_save(self.conn, self.world, 'martin', {'id': first['id'], 'version': version, 'name': 'Reed Ford', 'program': {**p, 'title': f't{n}'}})['version']
        kept = drafts.draft_open(self.conn, self.world, {'id': first['id']})['versions']
        self.assertEqual((len(kept), kept[0]['version'], kept[-1]['version']), (20, 27, 8))
        with self.assertRaisesRegex(ValueError, 'no longer kept'):
            drafts.draft_open(self.conn, self.world, {'id': first['id'], 'version': 1})

    def test_a_broken_program_is_kept_and_what_is_not_a_program_is_refused(self):
        broken = {'format': 'ratw-story-program', 'phases': 'not yet'}
        saved = drafts.draft_save(self.conn, self.world, 'martin', {'name': 'Half written', 'program': broken})
        self.assertEqual(drafts.draft_open(self.conn, self.world, {'id': saved['id']})['program'], broken)
        for bad, words in (({'name': '', 'program': {}}, 'name'), ({'name': 'x', 'program': '{}'}, 'JSON object'),
                           ({'name': 'x', 'program': {'big': 'x' * (1024 * 1024)}}, '1 MiB')):
            with self.assertRaisesRegex(ValueError, words):
                drafts.draft_save(self.conn, self.world, 'martin', bad)
        with self.assertRaisesRegex(ValueError, 'no longer kept'):
            drafts.draft_open(self.conn, self.world, {'id': 999999})

    def test_deleting_is_the_owners_or_a_builders(self):
        saved = drafts.draft_save(self.conn, self.world, 'martin', {'name': 'Mine', 'program': self.examples['wagon']})
        with self.assertRaisesRegex(ValueError, 'Only martin'):
            drafts.draft_delete(self.conn, self.world, 'ann', D.RANK['dm'], {'id': saved['id']})
        drafts.draft_delete(self.conn, self.world, 'bea', D.RANK['builder'], {'id': saved['id']})
        self.assertEqual(self.conn.execute('SELECT count(*) FROM dm.story_draft_versions WHERE draft=%s', (saved['id'],)).fetchone()[0], 0)

    def test_the_drafts_are_closed_to_the_game(self):
        self.conn.execute('SET LOCAL ROLE ratw_game')
        with self.assertRaises(Exception):
            self.conn.execute('SELECT 1 FROM dm.story_drafts')

    def test_refs_list_what_a_picker_may_name(self):
        refs = runner.refs(self.conn, self.world)
        self.assertEqual({r['id'] for r in refs['residents']} >= {'rm111', 'rm119'}, True)
        self.assertIn({'id': 'player-ash', 'name': 'Ash Greymantle'}, refs['wolves'])
        self.assertTrue(any(i['id'] == 'rope' and i['category'] for i in refs['items']))
        self.assertEqual(len(refs['injuries']), 26)
        for key in ('places', 'factions', 'towns', 'markets', 'areas', 'ships', 'templates', 'projects'):
            self.assertIsInstance(refs[key], list, key)
        self.assertNotIn('residents', runner.static_refs())

    def test_a_rehearsal_stepped_through_says_what_the_phase_waits_for(self):
        p = self.examples['reed-ford']
        out = runner.rehearse_steps(p, 0)
        self.assertTrue(out['passed'])
        first, last = out['steps'][0], out['steps'][-1]
        self.assertIsNone(first['fact'])
        self.assertEqual((first['phase'], first['path']), (p['start'], [p['start']]))
        self.assertEqual(last['path'], p['tests'][0]['expect'])
        self.assertEqual(sum(len(s['fired']) for s in out['steps']), len(rules.review(p)['tests'][0]['triggers']))
        self.assertTrue(any(s['listening'] for s in out['steps']))
        self.assertTrue(all({'source', 'can'} <= set(item) for s in out['steps'] for item in s['listening']))
        with self.assertRaisesRegex(ValueError, 'Choose a rehearsal case'):
            runner.rehearse_steps(p, 9)

    # ---------------------------------------------------------------------------------------------- through the route

    def call(self, body, token=''):
        request = urllib.request.Request(f'http://127.0.0.1:{self.port}/api/story-program', json.dumps({'target': 'dev', **body}).encode(), method='POST',
                                         headers={'Content-Type': 'application/json', **({'Authorization': 'Bearer ' + token} if token else {})})
        try:
            with urllib.request.urlopen(request, timeout=20) as reply:
                return reply.status, json.loads(reply.read())
        except urllib.error.HTTPError as error:
            return error.code, json.loads(error.read())

    def token(self, user, password):
        request = urllib.request.Request(f'http://127.0.0.1:{self.port}/api/login', json.dumps({'username': user, 'password': password}).encode(),
                                         method='POST', headers={'Content-Type': 'application/json'})
        try:
            with urllib.request.urlopen(request, timeout=10) as reply:
                return json.loads(reply.read())['token']
        except urllib.error.HTTPError as error:
            raise AssertionError(f'{user}: {error.code} {error.read()!r}') from None

    def test_the_route(self):
        dm, dm2 = self.token('wb-dm', 'a long dm password'), self.token('wb-dm2', 'another dm password')
        builder, viewer = self.token('wb-builder', 'a long builder password'), self.token('wb-viewer', 'a long viewer password')
        p = self.examples['hidden-pup']
        status, saved = self.call({'op': 'draft.save', 'name': 'The pup', 'program': p}, dm)
        self.assertEqual(status, 200, saved)
        status, listed = self.call({'op': 'draft.list'}, dm2)
        self.assertEqual(status, 200)
        self.assertIn(saved['id'], [d['id'] for d in listed['drafts']])
        status, opened = self.call({'op': 'draft.open', 'id': saved['id']}, dm2)
        self.assertEqual((status, opened['program'], opened['owner']), (200, p, 'wb-dm'))
        status, refused = self.call({'op': 'draft.delete', 'id': saved['id']}, dm2)
        self.assertEqual(status, 422, refused)
        status, conflict = self.call({'op': 'draft.save', 'id': saved['id'], 'version': 0, 'name': 'The pup', 'program': p}, dm2)
        self.assertEqual(status, 422, conflict)
        for op in ('draft.list', 'draft.save', 'draft.open', 'draft.delete'):
            status, _ = self.call({'op': op, 'id': saved['id'], 'name': 'x', 'program': p}, viewer)
            self.assertEqual(status, 403, op)
        status, refs = self.call({'op': 'refs'}, dm)
        self.assertEqual(status, 200)
        self.assertIn('wolves', refs)
        status, refs = self.call({'op': 'refs'}, viewer)
        self.assertEqual((status, 'wolves' in refs, 'items' in refs), (200, False, True))
        status, steps = self.call({'op': 'rehearse-step', 'program': p, 'case': 0}, viewer)
        self.assertEqual(status, 200, steps)
        self.assertTrue(steps['steps'])
        status, catalog = self.call({'op': 'catalog'}, viewer)
        self.assertEqual((status, len(catalog['catalogue']['events'])), (200, 208))   # (Phase 8: 24 new events; Phase 10: 10; doc 86: the ready exit.)
        status, gone = self.call({'op': 'draft.delete', 'id': saved['id']}, builder)
        self.assertEqual((status, gone), (200, {'deleted': saved['id']}))


if __name__ == '__main__':
    unittest.main()
