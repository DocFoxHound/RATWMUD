#!/usr/bin/env python3
"""The LIVE map end to end (Docs/Design/34-dungeon-master-refresh.md, 1.1): a game server writes positions while a
Dungeon Master watches, and moves someone when asked. Its own scratch database with Greyfen and its own server (never
DEV's), dropped afterwards; skipped without the local PostgreSQL or a built build-core/ratw_server."""
import os
import secrets
import signal
import socket
import subprocess
import time
import unittest
from pathlib import Path

import dungeon_master as D
import world_build
import world_db as W
import world_store as S
from test_world_db import database_available, superuser
from test_world_store import greyfen

ROOT = Path(__file__).resolve().parent.parent
SERVER = ROOT / 'build-core' / 'ratw_server'


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


@unittest.skipUnless(database_available() and SERVER.exists(), 'local PostgreSQL not running, or ratw_server not built')
class LiveMap(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        W.ensure_roles()
        cls.name = f'ratw_test_live_{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor, ratw_publisher, ratw_game, ratw_dm')
        with W.connect('dev', 'owner', dbname=cls.name) as owner:
            W.migrate(owner)
        with W.connect('dev', 'editor', dbname=cls.name) as editor:
            S.save_roster(editor, S.load_roster(editor))
            S.save_world(editor, greyfen(), None, create=True)
            world_build.build(editor, 'test')
        info = {**W.conninfo('dev', 'game'), 'dbname': cls.name}
        url = ' '.join(f'{k}={v}' for k, v in info.items())
        cls.server = subprocess.Popen([str(SERVER), '--database', 'dev', '--port', str(free_port()), '--for', '120'],
                                      env={**os.environ, 'RATW_DATABASE_URL': url, 'RATW_AI': 'off'},
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in cls.server.stdout:
            if 'listening on port' in line:
                break
        cls.dm = D.DungeonMaster(lambda target: W.connect(target, 'dm', dbname=cls.name))
        cls.master = {'username': 'dm-master', 'role': 'dm'}

    @classmethod
    def tearDownClass(cls):
        cls.server.send_signal(signal.SIGTERM)
        cls.server.wait(timeout=60)
        cls.server.stdout.close()
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    def watch(self, until, seconds=20):
        """Watches, as the map does every couple of seconds, until `until(live)` holds."""
        deadline = time.time() + seconds
        while time.time() < deadline:
            live = self.dm.live(self.master, 'dev')
            if until(live):
                return live
            time.sleep(.5)
        self.fail('the LIVE map never saw it')

    def settled(self, action_id):
        deadline = time.time() + 20
        while time.time() < deadline:
            done = self.dm.action('dev', action_id)
            if done['status'] != 'queued':
                return done
            time.sleep(.3)
        self.fail('the game server never answered')

    def test_positions_appear_while_watched_and_a_move_lands(self):
        live = self.watch(lambda l: l['frame'] is not None)
        npcs = [p for p in live['frame']['people'] if p[2] == 'n']
        self.assertTrue(npcs, "Greyfen's residents are on the map")
        self.assertLess(live['age'], 10, 'and fresh')
        for p in live['frame']['people']:
            self.assertEqual(len(p), 9)
            self.assertIn(p[2], ('p', 'o', 'n', 'r'))

        mover, there = npcs[0], npcs[1]       # Onto the tile another resident stands on: open ground for certain.
        cell, x, y = there[3], int(there[4]), int(there[5])
        done = self.settled(self.dm.move(self.master, 'dev', 'npc.move', mover[0], cell, x, y)['id'])
        self.assertEqual(done['status'], 'applied', done['result'])
        moved = self.watch(lambda l: any(p[0] == mover[0] and p[3] == cell and int(p[4]) == x and int(p[5]) == y
                                         for p in l['frame']['people']), seconds=10)
        self.assertTrue(moved)

        refused = self.settled(self.dm.move(self.master, 'dev', 'character.move', mover[0], cell, x, y)['id'])
        self.assertEqual(refused['status'], 'refused', 'an NPC is not moved as a player character')
        refused = self.settled(self.dm.move(self.master, 'dev', 'npc.move', 'nobody', cell, x, y)['id'])
        self.assertEqual(refused['status'], 'refused')
        self.assertIn('weather', live['frame'], 'the weather systems come with the positions')

    def test_a_visitor_comes_and_is_sent_away(self):
        live = self.watch(lambda l: l['frame'] is not None)
        host = next(p for p in live['frame']['people'] if p[2] == 'n')
        cell, x, y = host[3], int(host[4]), int(host[5])
        queued = self.dm.visit(self.master, 'dev', 'A messenger', cell, x, y, 30, like=host[0])
        self.assertEqual(self.settled(queued['id'])['status'], 'applied')
        seen = self.watch(lambda l: any(p[0] == queued['visitor'] for p in l['frame']['people']), seconds=10)
        visitor = next(p for p in seen['frame']['people'] if p[0] == queued['visitor'])
        self.assertEqual((visitor[1], visitor[2], visitor[3]), ('A messenger', 't', cell), 'a temporary visitor, where put')
        self.assertIn('leaves in 30 min', visitor[8])
        self.assertEqual(self.settled(self.dm.leave(self.master, 'dev', queued['visitor'])['id'])['status'], 'applied')
        self.watch(lambda l: all(p[0] != queued['visitor'] for p in l['frame']['people']), seconds=10)

    def test_the_dead_are_brought_back_where_chosen(self):
        live = self.watch(lambda l: l['frame'] is not None)
        npcs = [p for p in live['frame']['people'] if p[2] == 'n']
        fallen, there = npcs[-1], npcs[-2]
        self.dm.npc_life(self.master, 'dev', fallen[0], True)
        self.watch(lambda l: any(p[0] == fallen[0] and p[6] & 1 for p in l['frame']['people']), seconds=10)
        # As the map does it: revive, then move; the server takes them in order.
        self.dm.npc_life(self.master, 'dev', fallen[0], False)
        moved = self.dm.move(self.master, 'dev', 'npc.move', fallen[0], there[3], int(there[4]), int(there[5]))
        self.assertEqual(self.settled(moved['id'])['status'], 'applied')
        self.watch(lambda l: any(p[0] == fallen[0] and not p[6] & 1 and p[3] == there[3] and int(p[4]) == int(there[4])
                                 for p in l['frame']['people']), seconds=10)


if __name__ == '__main__':
    unittest.main()


@unittest.skipUnless(database_available() and SERVER.exists(), 'local PostgreSQL not running, or ratw_server not built')
class DungeonMasterMark(unittest.TestCase):
    """A player marked Dungeon Master stays one: saved to game.characters (its dungeon_master column, migration 0033),
    through a restart of the game server and its saves after."""

    @classmethod
    def setUpClass(cls):
        W.ensure_roles()
        cls.name = f'ratw_test_dmmark_{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor, ratw_publisher, ratw_game, ratw_dm')
        with W.connect('dev', 'owner', dbname=cls.name) as owner:
            W.migrate(owner)
        with W.connect('dev', 'editor', dbname=cls.name) as editor:
            S.save_roster(editor, S.load_roster(editor))
            S.save_world(editor, greyfen(), None, create=True)
            world_build.build(editor, 'test')
        info = {**W.conninfo('dev', 'game'), 'dbname': cls.name}
        cls.url = ' '.join(f'{k}={v}' for k, v in info.items())
        cls.dm = D.DungeonMaster(lambda target: W.connect(target, 'dm', dbname=cls.name))
        cls.admin = {'username': 'dm-admin', 'role': 'admin'}

    @classmethod
    def tearDownClass(cls):
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    def start(self):
        port = free_port()
        server = subprocess.Popen([str(SERVER), '--database', 'dev', '--port', str(port), '--for', '180', '--dev-identity',
                                   '--web', str(ROOT / 'Client' / 'dist')],
                                  env={**os.environ, 'RATW_DATABASE_URL': self.url, 'RATW_AI': 'off'},
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in server.stdout:
            if 'listening on port' in line:
                break
        return server, port

    def play(self, port, *args):
        """Ash signs in with the real client in headless Chromium (tools/client/devconsole.mjs): what the page saw."""
        import json
        run = subprocess.Popen(['node', str(ROOT / 'tools' / 'client' / 'devconsole.mjs'), str(port), *args],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            out, err = run.communicate(timeout=120)
        except subprocess.TimeoutExpired:
            run.terminate()                         # (A SIGTERM: the script stops its Chromium on the way out.)
            out, err = run.communicate(timeout=30)
        lines = [l for l in out.splitlines() if l.startswith('{')]
        self.assertTrue(lines, out + err)
        seen = json.loads(lines[-1])
        self.assertNotIn('error', seen, seen)
        return seen

    def stop(self, server):
        server.send_signal(signal.SIGTERM)          # A clean stop saves.
        server.wait(timeout=60)
        server.stdout.close()

    def saved(self):
        with W.connect('dev', 'dm', dbname=self.name) as conn:
            return conn.execute("SELECT dungeon_master, data->'dungeonMaster' FROM game.characters WHERE key = 'player-ash'").fetchone()

    def settled(self, action_id):
        deadline = time.time() + 20
        while time.time() < deadline:
            done = self.dm.action('dev', action_id)
            if done['status'] != 'queued':
                return done
            time.sleep(.3)
        self.fail('the game server never answered')

    def test_the_mark_is_saved_and_survives_a_restart(self):
        server, port = self.start()
        try:
            first = self.play(port)
            self.assertFalse(first['dungeonMaster'], 'a new player is no Dungeon Master')
            self.assertFalse(first['button'], 'and has no Dev Console button')
            deadline = time.time() + 60                 # (Saved once they have lingered and left: half a minute.)
            while time.time() < deadline and not self.saved():
                time.sleep(.5)
            self.assertEqual(self.saved(), (False, None), 'a new player is no Dungeon Master, and the column says so')
            done = self.settled(self.dm.request(self.admin, 'dev', 'character.dm', 'player-ash', 'testing', {'dungeonMaster': True})['id'])
            self.assertEqual(done['status'], 'applied', done['result'])
            deadline = time.time() + 20
            while time.time() < deadline and not self.saved()[0]:
                time.sleep(.5)
            self.assertEqual(self.saved(), (True, True), 'saved to the players table, column and record')
            self.assertTrue(self.dm.players('dev')['characters'][0]['dungeonMaster'], 'and the Players tab shows it')
        finally:
            self.stop(server)

        server, port = self.start()                 # A new session of the server: loaded from the database.
        try:
            seen = self.play(port, 'console', str(ROOT / 'artifacts' / 'screenshots' / 'dev-console-fight.png'))
            self.assertTrue(seen['dungeonMaster'], "Ash's page is told: the console is theirs again")
            self.assertTrue(seen['button'], 'a DEV CONSOLE button in the top bar')
            self.assertTrue(seen['console'], 'the ` key opens the Dev Console on screen, ready to type in')
            self.assertEqual(seen['all'], ['/fight-end-myself', '/fight-test-1', '/help'], 'every command, alphabetically')
            self.assertEqual(seen['fits'], ['/fight-end-myself', '/fight-test-1'], 'those that fit what is typed')
            self.assertEqual(seen['completed'], '/fight-test-1', 'Tab completes')
            self.assertTrue(seen['fight'], '/fight-test-1 typed into it starts a fight: ' + str(seen['log']))
            self.assertTrue(seen['ended'], 'and /fight-end-myself ends it: ' + str(seen['log']))
            self.assertEqual([c for c, ok, _ in seen['log']], ['/fight-test-1', '/fight-end-myself'])
            self.assertTrue(all(ok for _, ok, _ in seen['log']), seen['log'])
            self.assertTrue(seen['closed'], 'Esc closes it')
        finally:
            self.stop(server)
        self.assertEqual(self.saved(), (True, True), 'and still saved after the server saved again')

        server, _ = self.start()
        try:
            done = self.settled(self.dm.request(self.admin, 'dev', 'character.dm', 'player-ash', '', {'dungeonMaster': False})['id'])
            self.assertEqual(done['status'], 'applied', done['result'])
            self.assertFalse(self.dm.players('dev')['characters'][0]['dungeonMaster'], 'unmade, shown at once')
        finally:
            self.stop(server)
        self.assertEqual(self.saved()[0], False, 'and saved')
