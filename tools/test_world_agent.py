#!/usr/bin/env python3
"""The world agent (tools/world_agent.py; Docs/Design/77-central-accounts-and-hosted-worlds.md, 5.1 and 7) against a fake
central and a fake world, both small http.servers on this computer: the heartbeat's body, online.json and bans.json,
the listing's changes in the log, a token for another world, refusals, no redirects, the keys, and the leave on
SIGTERM. Standard library only; starts no game server and touches no database.

  python3 tools/test_world_agent.py
"""
from __future__ import annotations

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import threading
import time
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import world_agent as agent  # noqa: E402

WORLD = 'world-0123456789abcdef0123456789abcdef'
OTHER = 'world-fedcba9876543210fedcba9876543210'
WOLF1 = 'wolf-' + '1' * 32
WOLF2 = 'wolf-' + '2' * 32
TOKEN = 'rh_test-token_0123456789'
KEY = {'kty': 'OKP', 'crv': 'Ed25519', 'alg': 'Ed25519', 'use': 'sig', 'kid': 'k1', 'x': 'A' * 43}


class Fake:
    """A small JSON server on a free port: `routes` maps (method, path) to a function of the request's JSON body and
    headers giving (status, reply, extra headers); every request is kept in `seen`."""

    def __init__(self, routes):
        self.routes = routes
        self.seen: list[dict] = []
        fake = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def answer(self, method):
                length = int(self.headers.get('Content-Length') or 0)
                raw = self.rfile.read(length) if length else b''
                try:
                    body = json.loads(raw) if raw else None
                except json.JSONDecodeError:
                    body = None
                fake.seen.append({'method': method, 'path': self.path, 'body': body, 'headers': dict(self.headers)})
                route = fake.routes.get((method, self.path))
                status, reply, extra = route(body, self.headers) if route else (404, {'ok': False, 'code': 'not_found'}, {})
                data = json.dumps(reply).encode()
                self.send_response(status)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(data)))
                for name, value in extra.items():
                    self.send_header(name, value)
                self.end_headers()
                self.wfile.write(data)

            def do_GET(self):
                self.answer('GET')

            def do_POST(self):
                self.answer('POST')

        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.port = self.server.server_address[1]
        self.url = f'http://127.0.0.1:{self.port}'
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def calls(self, path):
        return [r for r in self.seen if r['path'] == path]

    def close(self):
        self.server.shutdown()
        self.server.server_close()


def world_status(world_id=WORLD, **change):
    status = {'ratw': 'world', 'id': world_id, 'name': 'Test Hollow', 'kind': 'community', 'protocol': 1,
              'build': 'abc1234', 'clientData': '0' * 32, 'players': 2, 'limit': 40, 'voices': False,
              'private': False, 'cheats': True}
    status.update(change)
    return status


class Central:
    """What the fake central answers, changeable by a test."""

    def __init__(self):
        self.world = WORLD
        self.listed = False
        self.why = "Central could not reach this world at 203.0.113.5:7788: timed out. Open port 7788 to the internet (TCP)."
        self.bans = ['acct-' + 'b' * 32]
        self.status = 200
        self.code = ''
        self.lock = threading.Lock()

    def heartbeat(self, body, headers):
        with self.lock:
            if headers.get('Authorization') != f'Bearer {TOKEN}':
                return 401, {'ok': False, 'code': 'bad_token', 'error': 'That host token is not known.'}, {}
            if self.status != 200:
                return self.status, {'ok': False, 'code': self.code, 'error': 'Refused.'}, {}
            return 200, {'ok': True, 'world': self.world, 'kind': 'community', 'listed': self.listed,
                         'reachable': self.listed, 'why': '' if self.listed else self.why,
                         'address': '203.0.113.5:7788', 'players': len(body.get('online', [])), 'bans': self.bans,
                         'time': time.time(), 'every': 60}, {}

    def routes(self):
        return {('POST', '/worlds/heartbeat'): self.heartbeat,
                ('POST', '/worlds/leave'): lambda body, headers: (200, {'ok': True}, {}),
                ('GET', '/keys'): lambda body, headers: (200, {'keys': [KEY]}, {}),
                ('GET', '/health'): lambda body, headers: (200, {'ok': True, 'time': time.time()}, {})}


class AgentTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='ratw-agent-test-')
        self.dir = Path(self.temp.name)
        self.central = Central()
        self.fake = Fake(self.central.routes())
        self.status = world_status()
        self.world = Fake({('GET', '/status'): lambda body, headers: (200, self.status, {})})
        self.lines: list[str] = []

    def tearDown(self):
        self.fake.close()
        self.world.close()
        self.temp.cleanup()

    def make(self, **options) -> agent.Agent:
        options.setdefault('log', self.lines.append)
        return agent.Agent(self.fake.url, self.dir, self.world.port, TOKEN, **options)

    def write_online(self, wolves, *, at=None, world=WORLD):
        (self.dir / 'online.json').write_text(json.dumps({'world': world, 'at': time.time() if at is None else at,
                                                          'players': len(wolves), 'online': wolves}))

    def test_the_heartbeat_carries_the_listing_and_the_wolves_and_writes_the_bans(self):
        self.write_online([WOLF1, 'not-a-wolf', WOLF2])
        a = self.make(description='A quiet hollow.', region='EU', tags=['rp', 'slow'], host='hollow.example.net')
        self.assertTrue(a.heartbeat())
        call = self.fake.calls('/worlds/heartbeat')[-1]
        self.assertEqual(call['headers'].get('Authorization'), f'Bearer {TOKEN}')
        body = call['body']
        self.assertEqual(body, {'port': self.world.port, 'name': 'Test Hollow', 'protocol': 1, 'build': 'abc1234',
                                'clientData': '0' * 32, 'limit': 40, 'voices': False, 'cheats': True,
                                'private': False, 'secure': False, 'description': 'A quiet hollow.', 'region': 'EU',
                                'tags': ['rp', 'slow'], 'host': 'hollow.example.net', 'online': [WOLF1, WOLF2]})
        self.assertNotIn('id', body, "the world's id comes from the token, not the body")
        bans = json.loads((self.dir / 'bans.json').read_text())
        self.assertEqual(bans['accounts'], ['acct-' + 'b' * 32])
        self.assertLessEqual(abs(bans['at'] - time.time()), 5)
        self.assertFalse(list(self.dir.glob('.bans.json.*')), 'written atomically: no temporary file left')
        self.assertTrue(any('not listed on central' in line and 'Open port 7788' in line for line in self.lines))

    def test_a_stale_or_foreign_online_file_counts_nobody(self):
        a = self.make()
        self.write_online([WOLF1], at=time.time() - 120)
        a.heartbeat()
        self.assertEqual(self.fake.calls('/worlds/heartbeat')[-1]['body']['online'], [])
        self.write_online([WOLF1], world=OTHER)
        a.heartbeat()
        self.assertEqual(self.fake.calls('/worlds/heartbeat')[-1]['body']['online'], [])
        (self.dir / 'online.json').unlink()
        a.heartbeat()
        self.assertEqual(self.fake.calls('/worlds/heartbeat')[-1]['body']['online'], [])
        self.write_online([WOLF2])
        a.heartbeat()
        self.assertEqual(self.fake.calls('/worlds/heartbeat')[-1]['body']['online'], [WOLF2])
        self.assertTrue(any('more than a minute old' in line for line in self.lines))
        self.assertTrue(any('another world' in line for line in self.lines))

    def test_the_listing_is_logged_when_it_changes_only(self):
        a = self.make()
        for _ in range(3):
            a.heartbeat()
        self.assertEqual(sum('not listed' in line for line in self.lines), 1)
        self.central.listed = True
        a.heartbeat()
        a.heartbeat()
        self.assertEqual(sum('listed on central at 203.0.113.5:7788' in line for line in self.lines), 1)
        self.central.listed = False
        self.central.why = "This world's address (IP) is not on the internet."
        a.heartbeat()
        self.assertIn('not on the internet', self.lines[-1])

    def test_a_token_for_another_world_stops_the_heartbeats(self):
        self.central.world = OTHER
        a = self.make()
        self.assertFalse(a.heartbeat())
        self.assertTrue(a.halted)
        self.assertTrue(any('THE HOST TOKEN IS FOR WORLD' in line and OTHER in line and WORLD in line for line in self.lines))
        self.assertFalse((self.dir / 'bans.json').exists(), "another world's bans aren't written here")
        before = len(self.fake.calls('/worlds/heartbeat'))
        self.assertFalse(a.heartbeat())
        self.assertEqual(len(self.fake.calls('/worlds/heartbeat')), before, 'no more heartbeats are posted')

    def test_refusals_and_a_silent_world_are_said_once(self):
        a = self.make()
        a.token = 'rh_wrong'
        a.heartbeat()
        a.heartbeat()
        self.assertEqual(sum('bad_token' in line for line in self.lines), 1)
        self.assertIn('Make a new token', self.lines[-1])
        self.assertFalse((self.dir / 'bans.json').exists())
        a.token = TOKEN
        self.central.status, self.central.code = 403, 'delisted'
        a.heartbeat()
        self.assertIn('delisted', self.lines[-1])
        self.central.status, self.central.code = 429, 'rate_limited'
        a.heartbeat()
        self.assertIn('rate_limited', self.lines[-1])
        self.status = {'ok': True}                  # (Not a ticket-mode world on that port.)
        posted = len(self.fake.calls('/worlds/heartbeat'))
        self.assertFalse(a.heartbeat())
        self.assertEqual(len(self.fake.calls('/worlds/heartbeat')), posted, 'no heartbeat without the world')
        self.assertIn('does not answer', self.lines[-1])

    def test_the_heartbeat_never_follows_a_redirect(self):
        self.fake.routes[('POST', '/worlds/heartbeat')] = lambda body, headers: (
            303, {"ok": False}, {"Location": self.fake.url + "/elsewhere"})
        a = self.make()
        self.assertFalse(a.heartbeat())
        self.assertFalse(self.fake.calls('/elsewhere'), 'the token is not carried to where a redirect points')
        self.assertIn("HTTP 303", self.lines[-1])

    def test_the_loop_keeps_the_keys_beats_and_leaves(self):
        self.write_online([WOLF1])
        a = self.make(every=0.2)
        thread = threading.Thread(target=a.run)
        thread.start()
        deadline = time.time() + 10
        while len(self.fake.calls('/worlds/heartbeat')) < 3 and time.time() < deadline:
            time.sleep(0.05)
        (self.dir / 'central-keys.json.wanted').write_text('1\n')
        a.keys.last_try = -1e9                      # (A minute has passed since the last fetch.)
        while (self.dir / 'central-keys.json.wanted').exists() and time.time() < deadline:
            time.sleep(0.05)
        a.stop.set()
        thread.join(5)
        self.assertFalse(thread.is_alive())
        self.assertGreaterEqual(len(self.fake.calls('/worlds/heartbeat')), 3)
        self.assertEqual(len(self.fake.calls('/keys')), 2, 'the keys at start, and again when the server asked')
        self.assertEqual(json.loads((self.dir / 'central-keys.json').read_text())['keys'], [KEY])
        leave = self.fake.calls('/worlds/leave')
        self.assertEqual(len(leave), 1)
        self.assertEqual(leave[0]['headers'].get('Authorization'), f'Bearer {TOKEN}')

    def test_the_command_takes_its_token_from_the_environment_and_leaves_on_sigterm(self):
        self.write_online([WOLF1])
        env = dict(os.environ, RATW_HOST_TOKEN=TOKEN)
        process = subprocess.Popen(
            [sys.executable, str(Path(agent.__file__)), 'run', '--central', self.fake.url, '--dir', str(self.dir),
             '--world-port', str(self.world.port), '--every', '0.3', '--tags', 'rp, slow,'],
            env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            deadline = time.time() + 10
            while not self.fake.calls('/worlds/heartbeat') and time.time() < deadline:
                time.sleep(0.05)
            self.assertTrue(self.fake.calls('/worlds/heartbeat'), 'a heartbeat is posted')
            self.assertEqual(self.fake.calls('/worlds/heartbeat')[0]['body']['tags'], ['rp', 'slow'])
            process.send_signal(signal.SIGTERM)
            output, _ = process.communicate(timeout=10)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
        self.assertEqual(process.returncode, 0, output)
        self.assertEqual(len(self.fake.calls('/worlds/leave')), 1, output)
        self.assertNotIn(TOKEN, output, 'the token is never printed')
        self.assertIn("left central's list", output)

    def test_the_command_needs_a_token_and_reads_a_token_file(self):
        env = {k: v for k, v in os.environ.items() if k != 'RATW_HOST_TOKEN'}
        command = [sys.executable, str(Path(agent.__file__)), 'run', '--central', self.fake.url, '--dir', str(self.dir),
                   '--world-port', str(self.world.port)]
        done = subprocess.run(command, env=env, capture_output=True, text=True, timeout=10)
        self.assertEqual(done.returncode, 2)
        self.assertIn('no host token', done.stderr)
        self.assertFalse(self.fake.seen, 'nothing is sent without a token')
        done = subprocess.run(command + ['--secure'], env=dict(env, RATW_HOST_TOKEN=TOKEN), capture_output=True,
                              text=True, timeout=10)
        self.assertNotEqual(done.returncode, 0)
        self.assertIn('--secure needs --host', done.stderr)
        token_file = self.dir / 'token'
        token_file.write_text(TOKEN + '\n')
        self.assertEqual(agent.read_token(token_file), TOKEN)
        token_file.write_text('rh_has a space\n')
        with self.assertRaises(agent.AgentError):
            agent.read_token(token_file)

    def test_the_keys_command_is_as_it_was(self):
        out = self.dir / 'keys.json'
        done = subprocess.run([sys.executable, str(Path(agent.__file__)), 'keys', '--central', self.fake.url, '--out',
                               str(out), '--once'], capture_output=True, text=True, timeout=10)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertEqual(json.loads(out.read_text())['keys'], [KEY])
        self.assertFalse(self.fake.calls('/worlds/heartbeat'))


if __name__ == '__main__':
    unittest.main()
