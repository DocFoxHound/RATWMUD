#!/usr/bin/env python3
"""The world agent (tools/world_agent.py; Docs/Design/77-central-accounts-and-hosted-worlds.md, 5.1 and 7) against a fake
central and a fake world, both small http.servers on this computer: the heartbeat's body, online.json and bans.json,
the listing's changes in the log, a token for another world, refusals, no redirects, the keys, the leave on
SIGTERM, plain http:// only to this computer or a private network, a large ban list, replies that aren't HTTP, slow
or broken centrals, no proxy for the world's own /status, and a token file with a byte-order mark. Standard library only; starts no game server and touches no database.

  python3 tools/test_world_agent.py
"""
from __future__ import annotations

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import importlib.util
import json
import os
from pathlib import Path
import signal
import socket
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
                try:
                    self.wfile.write(data)
                except (BrokenPipeError, ConnectionResetError):
                    pass                            # (The agent gave up waiting: a test of a slow central.)

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
              'private': False, 'cheats': True, 'quickenedWolves': False}
    status.update(change)
    return status


class Garbage:
    """Something on a port that doesn't speak HTTP: each connection gets a line of nonsense and is closed."""

    def __init__(self):
        self.socket = socket.socket()
        self.socket.bind(('127.0.0.1', 0))
        self.socket.listen()
        self.port = self.socket.getsockname()[1]
        self.url = f'http://127.0.0.1:{self.port}'
        self.thread = threading.Thread(target=self.serve, daemon=True)
        self.thread.start()

    def serve(self):
        while True:
            try:
                connection, _ = self.socket.accept()
            except OSError:
                return
            with connection:
                try:
                    connection.recv(65536)
                    connection.sendall(b'SSH-2.0-OpenSSH_9.6 not a web server\r\n')
                except OSError:
                    pass

    def close(self):
        self.socket.close()


class Central:
    """What the fake central answers, changeable by a test."""

    def __init__(self):
        self.world = WORLD
        self.listed = False
        self.why = "Central could not reach this world at 203.0.113.5:7788: timed out. Open port 7788 to the internet (TCP)."
        self.bans = ['acct-' + 'b' * 32]
        self.host = 'acct-' + 'h' * 32
        self.kind = 'community'
        self.status = 200
        self.code = ''
        self.lock = threading.Lock()

    def heartbeat(self, body, headers):
        with self.lock:
            if headers.get('Authorization') != f'Bearer {TOKEN}':
                return 401, {'ok': False, 'code': 'bad_token', 'error': 'That host token is not known.'}, {}
            if self.status != 200:
                return self.status, {'ok': False, 'code': self.code, 'error': 'Refused.'}, {}
            return 200, {'ok': True, 'world': self.world, 'kind': self.kind, 'listed': self.listed,
                         'reachable': self.listed, 'why': '' if self.listed else self.why,
                         'address': '203.0.113.5:7788', 'players': len(body.get('online', [])), 'bans': self.bans,
                         'host': self.host, 'time': time.time(), 'every': 60}, {}

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
                                'private': False, 'quickenedWolves': False, 'secure': False, 'description': 'A quiet hollow.', 'region': 'EU',
                                'tags': ['rp', 'slow'], 'host': 'hollow.example.net', 'online': [WOLF1, WOLF2]})
        self.assertNotIn('id', body, "the world's id comes from the token, not the body")
        bans = json.loads((self.dir / 'bans.json').read_text())
        self.assertEqual(bans['accounts'], ['acct-' + 'b' * 32])
        self.assertLessEqual(abs(bans['at'] - time.time()), 5)
        self.assertFalse(list(self.dir.glob('.bans.json.*')), 'written atomically: no temporary file left')
        self.assertTrue(any('not listed on central' in line and 'Open port 7788' in line for line in self.lines))

    def test_the_host_is_written_and_cleared_when_central_refuses(self):
        a = self.make()
        self.assertTrue(a.heartbeat())
        host = json.loads((self.dir / 'host.json').read_text())
        self.assertEqual(host['host'], 'acct-' + 'h' * 32)
        self.assertLessEqual(abs(host['at'] - time.time()), 5)
        self.assertFalse(list(self.dir.glob('.host.json.*')), 'written atomically: no temporary file left')
        self.central.host = None                    # (An official world: no host.)
        self.assertTrue(a.heartbeat())
        self.assertIsNone(json.loads((self.dir / 'host.json').read_text())['host'])
        self.central.host = 'x' * 65                # (Not an account's sub: left out.)
        self.assertTrue(a.heartbeat())
        self.assertIsNone(json.loads((self.dir / 'host.json').read_text())['host'])
        for code in ('banned', 'delisted', 'bad_token'):
            self.central.host = 'acct-' + 'h' * 32
            self.central.status, self.central.code = 200, ''
            self.assertTrue(a.heartbeat())
            self.assertIsNotNone(json.loads((self.dir / 'host.json').read_text())['host'])
            self.central.status, self.central.code = 403 if code != 'bad_token' else 401, code
            self.assertFalse(a.heartbeat())
            self.assertIsNone(json.loads((self.dir / 'host.json').read_text())['host'], f'{code}: no host powers')
        self.central.status, self.central.code = 429, 'rate_limited'
        self.central.host = 'acct-' + 'h' * 32
        self.central.status = 200
        a.heartbeat()
        self.central.status = 429
        a.heartbeat()
        self.assertEqual(json.loads((self.dir / 'host.json').read_text())['host'], 'acct-' + 'h' * 32,
                         'a rate limit is no reason to drop the host')

    def test_the_kind_and_the_host_go_where_only_the_agent_writes(self):
        """Doc 78, 6.7 (Phase M1): central.json, in --central-dir (not the agent directory, which the game server's user
        can write), carries central's kind for the world and its host, from each heartbeat; a refusal clears the host
        and keeps the kind; a restarted agent keeps the kind it last wrote; without --central-dir nothing is written."""
        own = self.dir / 'central'
        own.mkdir()
        a = self.make(central_dir=own)
        self.assertTrue(a.heartbeat())
        said = json.loads((own / 'central.json').read_text())
        self.assertEqual((said['kind'], said['host']), ('community', 'acct-' + 'h' * 32))
        self.assertLessEqual(abs(said['at'] - time.time()), 5)
        self.assertFalse(list(own.glob('.central.json.*')), 'written atomically: no temporary file left')
        self.assertFalse((self.dir / 'central.json').exists(), 'never in the shared agent directory')
        self.central.kind, self.central.host = 'official', None
        self.assertTrue(a.heartbeat())
        said = json.loads((own / 'central.json').read_text())
        self.assertEqual((said['kind'], said['host']), ('official', None))
        self.central.kind = 'nonsense'              # (Not a kind: the last one is kept.)
        self.assertTrue(a.heartbeat())
        self.assertEqual(json.loads((own / 'central.json').read_text())['kind'], 'official')
        self.central.status, self.central.code = 403, 'delisted'
        self.assertFalse(a.heartbeat())
        said = json.loads((own / 'central.json').read_text())
        self.assertEqual((said['kind'], said['host']), ('official', None), 'a refusal clears the host, keeps the kind')
        self.central.status, self.central.code, self.central.kind = 401, 'bad_token', 'community'
        again = self.make(central_dir=own)          # (A restarted agent, before any heartbeat is answered.)
        self.assertFalse(again.heartbeat())
        self.assertEqual(json.loads((own / 'central.json').read_text())['kind'], 'official', 'kept across a restart')
        plain = Path(self.temp.name) / 'plain'
        plain.mkdir()
        self.central.status, self.central.code = 200, ''
        b = agent.Agent(self.fake.url, plain, self.world.port, TOKEN, log=self.lines.append)
        self.assertTrue(b.heartbeat())
        self.assertFalse((plain / 'central.json').exists(), 'no --central-dir: no central.json')

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

    def test_an_official_worlds_quickened_accounts_go_with_the_heartbeat(self):
        """Doc 49, 9.1: unlocks.json's wolves (the accounts whose standing in this world opened Quickened) go with each
        heartbeat, for central to give those accounts Quickened in every world; none without the file, from another
        world's, or unreadable; never more than MOST_QUICKENED."""
        a = self.make()
        a.heartbeat()
        self.assertNotIn('quickened', self.fake.calls('/worlds/heartbeat')[-1]['body'], 'no unlocks.json: none sent')
        unlocks = self.dir / 'unlocks.json'
        unlocks.write_text(json.dumps({'world': WORLD, 'at': 1, 'quickened': [WOLF2, 'not-a-wolf', 7, WOLF1]}))
        a.heartbeat()
        self.assertEqual(self.fake.calls('/worlds/heartbeat')[-1]['body']['quickened'], [WOLF2, WOLF1])
        unlocks.write_text(json.dumps({'world': OTHER, 'at': 1, 'quickened': [WOLF1]}))
        a.heartbeat()
        self.assertNotIn('quickened', self.fake.calls('/worlds/heartbeat')[-1]['body'], "another world's: none")
        unlocks.write_text('{not json')
        a.heartbeat()
        self.assertNotIn('quickened', self.fake.calls('/worlds/heartbeat')[-1]['body'], 'unreadable: none')
        unlocks.write_text(json.dumps({'world': WORLD, 'at': 1, 'quickened': [WOLF1] * (agent.MOST_QUICKENED + 5)}))
        a.heartbeat()
        self.assertEqual(len(self.fake.calls('/worlds/heartbeat')[-1]['body']['quickened']), agent.MOST_QUICKENED)

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

    def test_centrals_own_reason_is_not_doubled(self):
        """Central's bad_token reasons already end "Make a new one on the Host page.": no second hint, no '..'."""
        own = ('Central does not know this host token (it was revoked, replaced or never made). Make a new one on '
               'the Host page.')
        self.fake.routes[('POST', '/worlds/heartbeat')] = lambda body, headers: (
            401, {'ok': False, 'code': 'bad_token', 'error': own}, {})
        a = self.make()
        a.heartbeat()
        self.assertEqual(self.lines[-1], f'world agent: central refused the heartbeat (bad_token): {own}')
        self.fake.routes[('POST', '/worlds/heartbeat')] = lambda body, headers: (
            403, {'ok': False, 'code': 'banned', 'error': "This world's host account is banned."}, {})
        a.heartbeat()
        self.assertEqual(self.lines[-1], "world agent: central refused the heartbeat (banned): This world's host "
                                         'account is banned.')
        self.fake.routes[('POST', '/worlds/heartbeat')] = lambda body, headers: (
            403, {'ok': False, 'code': 'delisted'}, {})
        a.heartbeat()
        self.assertEqual(self.lines[-1], 'world agent: central refused the heartbeat (delisted). Central has taken '
                                         'this world off its list.')

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
        token_file.write_bytes(b'\xef\xbb\xbf' + TOKEN.encode() + b'\r\n')   # (Notepad's byte-order mark.)
        self.assertEqual(agent.read_token(token_file), TOKEN)
        token_file.write_text('rh_has a space\n')
        with self.assertRaises(agent.AgentError):
            agent.read_token(token_file)

    def test_plain_http_goes_only_to_this_computer_or_a_private_network(self):
        for url in ('http://127.0.0.1:8080', 'http://localhost:8080', 'http://10.1.2.3', 'http://172.20.0.5:80',
                    'http://192.168.1.20:8787', 'http://169.254.7.7', 'http://[::1]:8080', 'http://[fd00::5]',
                    'https://central.example.net', 'https://8.8.8.8'):
            self.assertEqual(agent.plain_http_allowed(url), (True, ''), url)
        for url in ('http://8.8.8.8', 'http://1.1.1.1:8080', 'http://[2606:4700:4700::1111]', 'http://[::ffff:8.8.8.8]'):
            allowed, why = agent.plain_http_allowed(url)
            self.assertIs(allowed, False, url)
            self.assertIn('on the internet', why)
        self.assertIs(agent.plain_http_allowed('http://no-such-central.invalid')[0], None, "a name that can't be looked up")
        # The agent posts nothing to a public http:// central (the token would go in clear) and says so once.
        a = agent.Agent('http://8.8.8.8:9', self.dir, self.world.port, TOKEN, log=self.lines.append)
        self.assertFalse(a.heartbeat())
        self.assertFalse(a.heartbeat())
        self.assertEqual(sum('in clear' in line and 'https://' in line for line in self.lines), 1, self.lines)
        a.accepted = True
        a.leave()
        self.assertIn('no leave sent', self.lines[-1])
        # And the command refuses one outright, before anything is sent.
        done = subprocess.run([sys.executable, str(Path(agent.__file__)), 'run', '--central', 'http://8.8.8.8:9',
                               '--dir', str(self.dir), '--world-port', str(self.world.port)],
                              env=dict(os.environ, RATW_HOST_TOKEN=TOKEN), capture_output=True, text=True, timeout=10)
        self.assertEqual(done.returncode, 2)
        self.assertIn('https://', done.stderr)
        self.assertNotIn(TOKEN, done.stdout + done.stderr)

    def test_a_large_ban_list_reaches_the_world(self):
        self.central.bans = [f'acct-{n:032x}' for n in range(5000)]   # (~200 KB of reply: well past 64 KB.)
        a = self.make()
        self.assertTrue(a.heartbeat())
        self.assertEqual(len(json.loads((self.dir / 'bans.json').read_text())['accounts']), 5000)
        # A reply past the cap (or not JSON) is its own trouble, not a refusal, and the last bans.json is kept.
        old = agent.MAX_HEARTBEAT_REPLY
        agent.MAX_HEARTBEAT_REPLY = 64 * 1024
        try:
            self.assertFalse(a.heartbeat())
        finally:
            agent.MAX_HEARTBEAT_REPLY = old
        self.assertIn('too large or not JSON', self.lines[-1])
        self.assertNotIn('refused', self.lines[-1])
        self.assertEqual(len(json.loads((self.dir / 'bans.json').read_text())['accounts']), 5000)

    def test_something_that_is_not_http_is_an_error_not_a_crash(self):
        garbage = Garbage()
        try:
            a = agent.Agent(self.fake.url, self.dir, garbage.port, TOKEN, log=self.lines.append)
            self.assertFalse(a.heartbeat())
            self.assertIn('does not answer', self.lines[-1])
            a = agent.Agent(garbage.url, self.dir, self.world.port, TOKEN, log=self.lines.append)
            self.assertFalse(a.heartbeat())
            self.assertIn('could not reach central', self.lines[-1])
            with self.assertRaises(agent.AgentError):
                agent.fetch_keys(garbage.url, self.dir / 'keys.json')
            self.assertFalse(agent.KeyKeeper(garbage.url, self.dir / 'keys.json', 3600, self.lines.append).step(0.0))
        finally:
            garbage.close()

    def test_the_loop_carries_on_after_an_unexpected_error_and_never_logs_the_token(self):
        a = self.make(every=0.1)
        beats = []

        def broken():
            beats.append(1)
            raise RuntimeError(f'something odd with Bearer {TOKEN}')
        a.heartbeat = broken
        thread = threading.Thread(target=a.run)
        thread.start()
        deadline = time.time() + 10
        while len(beats) < 3 and time.time() < deadline:
            time.sleep(0.05)
        a.stop.set()
        thread.join(5)
        self.assertFalse(thread.is_alive())
        self.assertGreaterEqual(len(beats), 3, 'the loop went on beating')
        self.assertEqual(sum('unexpected trouble' in line for line in self.lines), 1, self.lines)
        self.assertFalse([line for line in self.lines if TOKEN in line], 'the token is never logged')

    def test_a_slow_central_is_said_as_such(self):
        def slow(body, headers):
            time.sleep(1.0)
            return self.central.heartbeat(body, headers)
        self.fake.routes[('POST', '/worlds/heartbeat')] = slow
        a = self.make()
        self.assertEqual(a.heartbeat_timeout, 30.0, 'well past central\'s look-up and call-back')
        a.heartbeat_timeout = 0.3
        self.assertFalse(a.heartbeat())
        self.assertIn('did not answer in time', self.lines[-1])
        self.assertFalse((self.dir / 'bans.json').exists())

    def test_a_bans_file_that_cant_be_written_is_said_when_it_starts_and_when_it_mends(self):
        (self.dir / 'bans.json').mkdir()             # (os.replace can't put a file over a directory.)
        a = self.make()
        for _ in range(3):
            self.assertTrue(a.heartbeat())
        self.assertEqual(sum('could not write bans.json' in line for line in self.lines), 1, self.lines)
        self.assertEqual(sum('not listed' in line for line in self.lines), 1, self.lines)
        (self.dir / 'bans.json').rmdir()
        a.heartbeat()
        a.heartbeat()
        self.assertEqual(sum('bans.json is written again' in line for line in self.lines), 1, self.lines)
        self.assertEqual(sum('not listed' in line for line in self.lines), 1, 'the listing is not said again')

    def test_the_worlds_own_status_goes_past_a_proxy_and_no_redirect(self):
        proxy = Fake({})
        old = {k: os.environ.get(k) for k in ('HTTP_PROXY', 'http_proxy', 'NO_PROXY', 'no_proxy')}
        try:
            for k in old:
                os.environ.pop(k, None)
            os.environ['HTTP_PROXY'] = os.environ['http_proxy'] = proxy.url
            spec = importlib.util.spec_from_file_location('world_agent_proxied', agent.__file__)
            proxied = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(proxied)        # (Its openers are built with the proxy in the environment.)
            a = proxied.Agent(self.fake.url, self.dir, self.world.port, TOKEN, log=self.lines.append)
            self.assertEqual(a.world_status()['id'], WORLD)
            self.assertFalse(proxy.seen, 'the local /status did not go to the proxy')
            self.assertEqual(len(self.world.calls('/status')), 1)
        finally:
            for k, v in old.items():
                if v is None:
                    os.environ.pop(k, None)
                else:
                    os.environ[k] = v
            proxy.close()
        self.world.routes[('GET', '/status')] = lambda body, headers: (302, {}, {'Location': self.world.url + '/elsewhere'})
        with self.assertRaises(agent.AgentError):
            self.make().world_status()
        self.assertFalse(self.world.calls('/elsewhere'), 'not followed')

    def test_the_keys_command_is_as_it_was(self):
        out = self.dir / 'keys.json'
        done = subprocess.run([sys.executable, str(Path(agent.__file__)), 'keys', '--central', self.fake.url, '--out',
                               str(out), '--once'], capture_output=True, text=True, timeout=10)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertEqual(json.loads(out.read_text())['keys'], [KEY])
        self.assertFalse(self.fake.calls('/worlds/heartbeat'))


if __name__ == '__main__':
    unittest.main()
