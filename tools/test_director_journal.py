#!/usr/bin/env python3
"""The Storykeeper's grant through a failing journal (Docs/Design/79-a-staged-economy.md, Phase 9's second repair): a real
game server on a file save with the operator bridge (--dm-directory), a player signed in (tools/client/hold.ts).

The save's folder made read only and its journal 0400, a player_grant is written to the bridge's outbox: the checkpoint
fails and the journal can't write the grant's record. Before the repair the bridge answered at once "it stands" and took
the request out of the outbox, and the server killed then lost the grant. Now it is held: no answer, the request left in
the outbox, the channel halted. The server killed and the folder writable again, the next server applies it once and
answers it.

Needs a built server (RATW_SERVER, build-core/ratw_server by default) and Node; no database. Its files under a temporary
folder, removed after.
"""
import json
import os
import shutil
import signal
import socket
import stat
import subprocess
import tempfile
import time
import unittest
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SERVER = Path(os.environ.get('RATW_SERVER', ROOT / 'build-core' / 'ratw_server'))
NODE = shutil.which('node')


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


@unittest.skipUnless(SERVER.exists() and NODE and os.geteuid() != 0, 'a built ratw_server or Node missing (or run as root)')
class DirectorGrantThroughAFailingJournal(unittest.TestCase):
    def setUp(self):
        self.run = Path(tempfile.mkdtemp(prefix='ratw-s2stage-director-'))
        self.folder = self.run / 'save'
        self.folder.mkdir()
        self.save = self.folder / 'world.json'
        self.bridge = self.run / 'bridge'

    def tearDown(self):
        for p in (self.folder, *self.folder.iterdir()):
            try:
                p.chmod(0o700 if p.is_dir() else 0o600)
            except OSError:
                pass
        shutil.rmtree(self.run, ignore_errors=True)

    def start(self):
        port = free_port()
        server = subprocess.Popen([str(SERVER), '--save', str(self.save), '--port', str(port), '--for', '120', '--dev-identity',
                                   '--dm-directory', str(self.bridge)],
                                  env={**os.environ, 'RATW_AI': 'off'}, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in server.stdout:
            if 'listening on port' in line:
                return server, port
        server.wait(timeout=30)
        self.fail('the server never listened')

    def until(self, test, what, seconds=30):
        deadline = time.time() + seconds
        while time.time() < deadline:
            found = test()
            if found:
                return found
            time.sleep(.25)
        self.fail(what)

    def saved_cash(self, pc):
        try:
            data = json.loads(self.save.read_text())
        except (OSError, ValueError):
            return None
        account = ((data.get('society') or {}).get('accounts') or {}).get(pc) or {}
        return account.get('cash')

    def request(self, pc):
        world = json.loads((self.bridge / 'snapshot.json').read_text())['worldId']
        now = int(time.time())
        rid = uuid.uuid4().hex
        body = {'version': 1, 'id': rid, 'worldId': world, 'createdAtUnix': now, 'expiresAtUnix': now + 280,
                'kind': 'player_grant', 'payload': {'player': pc, 'coins': 23, 'reason': 'for the lost ewe'}}
        path = self.bridge / 'outbox' / f'{rid}.json'
        temp = path.with_suffix('.tmp')
        temp.write_text(json.dumps(body))
        temp.rename(path)
        return rid, path, self.bridge / 'inbox' / f'{rid}.json'

    def test_a_grant_the_journal_has_not_written_is_held_not_told(self):
        server, port = self.start()
        player = None
        try:
            player = subprocess.Popen([NODE, '--experimental-strip-types', '--no-warnings', str(ROOT / 'tools' / 'client' / 'hold.ts'),
                                       '--port', str(port), '--identity', 'ash', '--seconds', '60'],
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=ROOT)
            pc = None
            for line in player.stdout:
                try:
                    pc = json.loads(line).get('in') or pc
                except ValueError:
                    continue
                if pc:
                    break
            self.assertTrue(pc, 'the player came in')
            had = self.until(lambda: self.saved_cash(pc), 'the player\'s purse in a save', 60)
            journal = Path(str(self.save) + '.journal')
            if not journal.exists():
                journal.touch()
            journal.chmod(stat.S_IRUSR)
            self.folder.chmod(stat.S_IRUSR | stat.S_IXUSR)
            rid, outbox, inbox = self.request(pc)
            time.sleep(8)
            self.assertFalse(inbox.exists(), 'never told it stands while its record isn\'t written: ' +
                             (inbox.read_text() if inbox.exists() else ''))
            self.assertTrue(outbox.exists(), 'the request left for the next server')
            server.kill()
            server.communicate(timeout=60)
        finally:
            if player and player.poll() is None:
                player.kill()
            if player:
                player.wait(timeout=10)
                player.stdout.close()
            if server.poll() is None:
                server.kill()
                server.communicate(timeout=60)
            self.folder.chmod(stat.S_IRWXU)
            for p in self.folder.iterdir():
                p.chmod(stat.S_IRUSR | stat.S_IWUSR)
        self.assertEqual(self.saved_cash(pc), had, 'the failed checkpoint kept nothing')
        server, _ = self.start()
        try:
            self.until(lambda: inbox.exists(), 'the next server answers the request', 30)
            reply = json.loads(inbox.read_text())
            self.assertTrue(reply['ok'], reply)
            self.assertIn('Granted 23p', reply['detail'])
            self.until(lambda: self.saved_cash(pc) == had + 23, 'the grant saved once', 60)
        finally:
            server.send_signal(signal.SIGTERM)
            server.communicate(timeout=60)
        self.assertEqual(self.saved_cash(pc), had + 23, 'paid once')


if __name__ == '__main__':
    unittest.main()
