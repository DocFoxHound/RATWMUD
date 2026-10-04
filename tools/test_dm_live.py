#!/usr/bin/env python3
"""A Dungeon Master's queued action, end to end (Docs/Design/38-injuries.md, phase 5): a real game server on a scratch
database with Greyfen, a player signed in (tools/client/hold.ts), and dm.actions rows the server applies. An injury
given to the player online reaches their page and is told to them; once they have left, one is taken away and another
given offline; and the saved character holds what is left. Skipped without the local PostgreSQL, a built server or
Node. RATW_SERVER picks the server binary (build-core/ratw_server by default)."""
import json
import os
import secrets
import shutil
import signal
import socket
import subprocess
import time
import unittest
from pathlib import Path

import world_build
import world_db as W
import world_store as S
from test_world_db import database_available, superuser
from test_world_store import greyfen

ROOT = Path(__file__).resolve().parent.parent
SERVER = Path(os.environ.get('RATW_SERVER', ROOT / 'build-core' / 'ratw_server'))
NODE = shutil.which('node')


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


@unittest.skipUnless(database_available() and SERVER.exists() and NODE, 'local PostgreSQL, a built ratw_server or Node missing')
class DungeonMasterLive(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        W.ensure_roles()
        cls.name = f'ratw_test_dmlive_{secrets.token_hex(4)}'
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

    @classmethod
    def tearDownClass(cls):
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    def queue(self, kind, target, payload):
        with W.connect('dev', 'owner', dbname=self.name) as conn:
            action = conn.execute('INSERT INTO dm.actions (kind, target_id, requested_by, payload) VALUES (%s, %s, %s, %s) RETURNING id',
                                  (kind, target, 'test', json.dumps(payload))).fetchone()[0]
            conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action}),))   # (As the DM tool does.)
            return action

    def outcome(self, action, seconds=15):
        """The action's status and result, once the server has dealt with it."""
        deadline = time.time() + seconds
        while time.time() < deadline:
            with W.connect('dev', 'owner', dbname=self.name) as conn:
                status, result = conn.execute('SELECT status, result FROM dm.actions WHERE id = %s', (action,)).fetchone()
            if status != 'queued':
                return status, result
            time.sleep(.25)
        self.fail(f'action {action} was never applied')

    def test_an_injury_given_online_and_offline(self):
        port = free_port()
        server = subprocess.Popen([str(SERVER), '--database', 'dev', '--port', str(port), '--for', '120', '--dev-identity'],
                                  env={**os.environ, 'RATW_DATABASE_URL': self.url, 'RATW_AI': 'off'},
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        player = None
        try:
            for line in server.stdout:
                if 'listening on port' in line:
                    break
            player = subprocess.Popen([NODE, '--experimental-strip-types', '--no-warnings', str(ROOT / 'tools' / 'client' / 'hold.ts'),
                                       '--port', str(port), '--identity', 'ada', '--seconds', '60'],
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=ROOT)
            seen = []

            def until(test, what, seconds=20):
                for earlier in seen:                # (Lines may come in either order.)
                    if test(earlier):
                        return earlier
                deadline = time.time() + seconds
                while time.time() < deadline:
                    line = player.stdout.readline()
                    if not line:
                        break
                    try:
                        seen.append(json.loads(line))
                    except ValueError:
                        continue
                    if test(seen[-1]):
                        return seen[-1]
                self.fail(f'{what} never came: {seen[-6:]}')

            who = until(lambda o: 'in' in o, 'the player in the world')['in']
            # Online: given, told, and on the page.
            given = self.queue('character.injury', who, {'add': 'cracked_rib', 'severity': 3})
            listed = until(lambda o: any(i.get('name') == 'Cracked rib' for i in o.get('injuries', [])), 'the injury on the page')
            rib = next(i for i in listed['injuries'] if i['name'] == 'Cracked rib')
            self.assertEqual((rib['kind'], rib['severity']), ('acute', 'severe'), 'a severe cracked rib, as asked')
            self.assertIn('rest', rib['line'], 'told with the rest it needs: ' + rib['line'])
            until(lambda o: 'You find you have an injury: Cracked rib' in o.get('said', ''), 'the player told')
            status, result = self.outcome(given)
            self.assertEqual(status, 'applied', result)
            self.assertIn('Cracked rib given', result)
            # Offline: the player leaves; the rib is taken away and a bent tail given.
            player.send_signal(signal.SIGTERM)
            player.wait(timeout=10)
            time.sleep(3)
            taken = self.queue('character.injury', who, {'remove': rib['id']})
            status, result = self.outcome(taken)
            self.assertEqual(status, 'applied', result)
            self.assertIn('taken away', result)
            tail = self.queue('character.injury', who, {'add': 'bent_tail'})
            status, result = self.outcome(tail)
            self.assertEqual(status, 'applied', result)
            self.assertIn('Bent tail given', result)
            bad = self.queue('character.injury', who, {'add': 'broken_heart'})
            self.assertEqual(self.outcome(bad)[0], 'refused', 'an unknown injury is refused by the server too')
        finally:
            if player and player.poll() is None:
                player.kill()
            if player:
                player.wait(timeout=10)
                player.stdout.close()
            server.send_signal(signal.SIGTERM)
            out, _ = server.communicate(timeout=60)
        # Saved: the character holds the tail, not the rib.
        with W.connect('dev', 'owner', dbname=self.name) as conn:
            row = conn.execute('SELECT data FROM game.characters WHERE key = %s', (who,)).fetchone()
        self.assertIsNotNone(row, 'the character is saved')
        kept = [i['type'] for i in (row[0].get('injuries') or [])]
        self.assertEqual(kept, ['bent_tail'], 'saved with the bent tail only: ' + json.dumps(row[0].get('injuries')))


if __name__ == '__main__':
    unittest.main()
