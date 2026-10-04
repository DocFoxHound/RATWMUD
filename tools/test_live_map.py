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

SERVER = Path(__file__).resolve().parent.parent / 'build-core' / 'ratw_server'


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


if __name__ == '__main__':
    unittest.main()
