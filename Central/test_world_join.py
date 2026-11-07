#!/usr/bin/env python3
"""Joining a world by ticket, the whole way through (Docs/Design/77-central-accounts-and-hosted-worlds.md, Phase 2).

A real central (ratw_central.py serve) on a throwaway database, ratw_central_jointest (made and dropped with the
Postgres superuser from Database/.env, as test_central.py does; no world's database is touched and no role is made or
changed); the world agent (tools/world_agent.py keys) keeping central's keys in a file; and a ratw_server in ticket
mode on a free port, with the built-in demo world saved to a throwaway file (no Postgres for the world). Two wolves of
two accounts get tickets, join, arrive, play a moment, leave and rejoin straight in. Then the save is swept: central's
account ids are there, and nothing of a username or a password. Everything started here is stopped here.

  set -a; . MAIN/Database/.env; set +a        (in a git worktree, which has no Database/.env of its own)
  python3 Central/test_world_join.py          (RATW_SERVER and RATW_IDENTITY name the binaries; else build-*/)
"""
from __future__ import annotations

import base64
import json
import os
from pathlib import Path
import re
import secrets
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest
import urllib.error
import urllib.request
import zlib

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
import central_store  # noqa: E402

TEST_DB = 'ratw_central_jointest'
PASSWORD = 'correct horse battery'
PROTOCOL = 1
LOOK = {'species': 'timber', 'sex': 'female', 'stature': 'average', 'pattern': 'solid', 'baseColor': 3,
        'gradientColor': 1, 'markingColor': 2, 'gradientAmount': 0.4, 'patternAmount': 0.2}
USERNAMES = ('joinwolfalpha', 'joinwolfbravo')


def binary(name: str, env: str) -> Path:
    if os.environ.get(env):
        return Path(os.environ[env])
    for folder in ('build-central', 'build-core', 'Build'):
        if (ROOT / folder / name).is_file():
            return ROOT / folder / name
    raise unittest.SkipTest(f'no {name}: build it, or set {env}')


# --------------------------------------------------------------------------- A WebSocket client, as the page is one

class Socket:
    """Just enough RFC 6455 to play: the handshake, masked binary frames out, the server's frames in."""

    def __init__(self, port: int, origin: str | None = None):
        self.sock = socket.create_connection(('127.0.0.1', port), timeout=10)
        key = base64.b64encode(secrets.token_bytes(16)).decode()
        head = (f'GET /ws HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
                f'Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n')
        if origin:
            head += f'Origin: {origin}\r\n'
        self.sock.sendall((head + '\r\n').encode())
        self.buffer = b''
        while b'\r\n\r\n' not in self.buffer:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError('closed during the handshake')
            self.buffer += chunk
        response, self.buffer = self.buffer.split(b'\r\n\r\n', 1)
        self.status = int(response.split(b' ')[1])
        self.events: list[dict] = []

    def send(self, message: dict):
        payload = b'\x01' + json.dumps(message).encode()
        mask = secrets.token_bytes(4)
        head = bytes([0x82])
        n = len(payload)
        head += bytes([0x80 | n]) if n < 126 else bytes([0x80 | 126]) + struct.pack('>H', n) if n < 65536 else \
            bytes([0x80 | 127]) + struct.pack('>Q', n)
        self.sock.sendall(head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))

    def _take(self, n: int) -> bytes:
        while len(self.buffer) < n:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError('closed')
            self.buffer += chunk
        out, self.buffer = self.buffer[:n], self.buffer[n:]
        return out

    def frame(self) -> tuple[int, bytes]:
        b0, b1 = self._take(2)
        n = b1 & 0x7f
        if n == 126:
            n = struct.unpack('>H', self._take(2))[0]
        elif n == 127:
            n = struct.unpack('>Q', self._take(8))[0]
        return b0 & 0x0f, self._take(n)

    def until(self, test, seconds: float = 15.0) -> dict:
        """The first game event (link::Event, kind 10) that passes `test`, reading for at most `seconds`."""
        end = time.time() + seconds
        while time.time() < end:
            self.sock.settimeout(max(0.1, end - time.time()))
            try:
                op, data = self.frame()
            except socket.timeout:
                break
            if op == 8:
                raise ConnectionError('the server closed the socket')
            if op != 2 or not data or data[0] != 10:
                continue
            length = struct.unpack('<I', data[1:5])[0]
            raw = data[5:] if length & 0x80000000 else zlib.decompress(data[5:])
            event = json.loads(raw)
            self.events.append(event)
            if test(event):
                return event
        raise AssertionError('no such event; the last were: ' + json.dumps(self.events[-4:])[:2000])

    def lobby(self, **want) -> dict:
        return self.until(lambda e: e.get('type') == 'lobby' and all(e.get(k) == v for k, v in want.items()))

    def close(self):
        try:
            self.sock.sendall(bytes([0x88, 0x82]) + b'\0\0\0\0' + b'\x03\xe8')
        except OSError:
            pass
        self.sock.close()


def claims_of(ticket: str) -> dict:
    body = ticket.split('.')[1]
    return json.loads(base64.urlsafe_b64decode(body + '=' * (-len(body) % 4)))


class JoinByTicket(unittest.TestCase):
    processes: list[subprocess.Popen] = []

    @classmethod
    def start(cls, args, env=None, ready=None, timeout=60.0) -> tuple[subprocess.Popen, str]:
        process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                   env={**os.environ, **(env or {})}, cwd=ROOT)
        cls.processes.append(process)
        if ready is None:
            return process, ''
        end = time.time() + timeout
        seen = []
        while time.time() < end:
            line = process.stdout.readline()
            if not line:
                break
            seen.append(line)
            match = re.search(ready, line)
            if match:
                return process, match.group(1)
        raise AssertionError(f'{args[0]} did not start: ' + ''.join(seen[-20:]))

    @classmethod
    def setUpClass(cls):
        cls.server_binary = binary('ratw_server', 'RATW_SERVER')
        identity = binary('ratw_identity', 'RATW_IDENTITY')
        cls.tmp = tempfile.TemporaryDirectory(prefix='ratw-join-')
        tmp = Path(cls.tmp.name)
        central_store.drop_database(TEST_DB)
        central_store.create_database(TEST_DB)
        store = central_store.Store(TEST_DB)
        store.migrate()
        cls.world = 'world-' + secrets.token_hex(16)
        store.add_world(cls.world, 'Join Test Hollow', 'community', time.time() - 5)
        store.close()
        env = {'RATW_CENTRAL_DBNAME': TEST_DB, 'RATW_IDENTITY': str(identity)}
        _, cls.central = cls.start([sys.executable, str(HERE / 'ratw_central.py'), 'serve', '--port', '0',
                                    '--keys', str(tmp / 'central-keys')], env, r'listening on (http://[0-9.:]+)/')
        cls.keys = tmp / 'world' / 'central-keys.json'
        cls.agent, _ = cls.start([sys.executable, str(ROOT / 'tools' / 'world_agent.py'), 'keys', '--central',
                                  cls.central, '--out', str(cls.keys)])
        end = time.time() + 20
        while not cls.keys.exists() and time.time() < end:
            time.sleep(0.1)
        assert cls.keys.exists(), 'the world agent fetched no keys'
        cls.save = tmp / 'world' / 'save.json'
        cls.server, port = cls.start([str(cls.server_binary), '--save', str(cls.save), '--port', '0', '--perf-log', '0',
                                      '--workers', '0', '--for', '600', '--central', cls.central, '--world-id',
                                      cls.world, '--central-keys', str(cls.keys), '--world-name', 'Join Test Hollow'],
                                     None, r'listening on port (\d+)', timeout=120)
        cls.port = int(port)

    @classmethod
    def tearDownClass(cls):
        for process in reversed(cls.processes):
            if process.poll() is None:
                process.send_signal(signal.SIGTERM)
                try:
                    process.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        central_store.drop_database(TEST_DB)
        cls.tmp.cleanup()

    # ------------------------------------------------------------------ Central over HTTP

    def http(self, method, path, body=None, token=None, base=None):
        data = json.dumps(body).encode() if body is not None else None
        request = urllib.request.Request((base or self.central) + path, data=data, method=method)
        if data is not None:
            request.add_header('Content-Type', 'application/json')
        if token:
            request.add_header('Authorization', f'Bearer {token}')
        try:
            with urllib.request.urlopen(request, timeout=10) as response:
                text = response.read()
                return response.status, json.loads(text) if text[:1] in b'{[' else text, dict(response.headers)
        except urllib.error.HTTPError as error:
            with error:
                return error.code, error.read(), dict(error.headers)

    def account(self, username: str, wolf: str, family: str = '') -> tuple[str, str]:
        status, reply, _ = self.http('POST', '/account/register', {'username': username, 'password': PASSWORD,
                                                                   'birth_year': 1990})
        self.assertEqual(status, 200, reply)
        token = reply['access_token']
        status, made, _ = self.http('POST', '/wolves', {'name': wolf, 'age': 8, 'appearance': LOOK, 'family': family},
                                    token)
        self.assertEqual(status, 200, made)
        return token, made['wolf']['id']

    def ticket(self, token: str, wolf: str) -> str:
        status, reply, _ = self.http('POST', '/tickets', {'world': self.world, 'wolf': wolf}, token)
        self.assertEqual(status, 200, reply)
        return reply['ticket']

    def status(self) -> dict:
        status, body, headers = self.http('GET', '/status', base=f'http://127.0.0.1:{self.port}')
        self.assertEqual(status, 200)
        self.assertEqual(headers.get('Access-Control-Allow-Origin'), '*')
        return body

    def arrive(self, ws: Socket, lobby: dict, command: str, tier: str | None) -> dict:
        arrival = {'type': 'character_arrive', 'commandId': command}
        if tier:
            arrival['tier'] = tier
        if lobby.get('tieRequired'):
            arrival['tie'] = lobby['ties'][0]['id']
        ws.send(arrival)
        return ws.until(lambda e: e.get('type') == 'entered' or (e.get('type') == 'lobby' and not e.get('ok')))

    # ------------------------------------------------------------------ The test

    def test_two_wolves_join_arrive_leave_and_rejoin(self):
        # The world as the list and the client's ping see it (doc 77, 5.1).
        status = self.status()
        self.assertEqual((status['id'], status['name'], status['kind'], status['protocol'], status['players']),
                         (self.world, 'Join Test Hollow', 'community', PROTOCOL, 0))
        self.assertRegex(status['clientData'], r'^[0-9a-f]{32}$')
        hash_now = subprocess.run([sys.executable, str(ROOT / 'tools' / 'client_data_hash.py')], capture_output=True,
                                  text=True, check=True).stdout.strip()
        self.assertEqual(status['clientData'], hash_now, "the server's client-data hash is the tool's")
        self.assertLessEqual(len(json.dumps(status)), 2048)
        self.assertEqual(self.http('GET', '/', base=f'http://127.0.0.1:{self.port}')[0], 404, 'no page is served')
        self.assertEqual(self.http('GET', '/index.html', base=f'http://127.0.0.1:{self.port}')[0], 404)

        alpha_token, alpha_wolf = self.account(USERNAMES[0], 'Alder Join', 'fire')
        bravo_token, bravo_wolf = self.account(USERNAMES[1], 'Bracken Join')

        # Any Origin is accepted (the client's bundled page has its own); a password, even from loopback, is not.
        alpha = Socket(self.port, origin='tauri://localhost')
        self.assertEqual(alpha.status, 101, 'a page of another origin may open the game in ticket mode')
        first = alpha.lobby(stage='ticket', mode='ticket')
        self.assertEqual((first['protocol'], first['world']), (PROTOCOL, self.world))
        alpha.send({'type': 'auth_login', 'username': USERNAMES[0], 'password': PASSWORD})
        self.assertEqual(alpha.lobby(ok=False)['code'], 'ticket_only')

        # Alpha: a ticket, the first arrival (a Gift family: Gifted), in.
        ticket = self.ticket(alpha_token, alpha_wolf)
        alpha.send({'type': 'hello', 'ticket': ticket, 'protocol': PROTOCOL})
        arrive = alpha.lobby(stage='arrive')
        self.assertEqual(arrive['wolf']['name'], 'Alder Join')
        self.assertEqual(arrive['tierChoices'], ['gifted', 'quickened'])
        entered = self.arrive(alpha, arrive, 'alpha-1', 'gifted')
        self.assertEqual(entered.get('id'), alpha_wolf, entered)

        # Bravo: no family, Normal.
        bravo = Socket(self.port)
        bravo.lobby(stage='ticket')
        bravo.send({'type': 'hello', 'ticket': self.ticket(bravo_token, bravo_wolf), 'protocol': PROTOCOL})
        arrive = bravo.lobby(stage='arrive')
        self.assertEqual(arrive['tierChoices'], ['normal'])
        entered = self.arrive(bravo, arrive, 'bravo-1', None)
        self.assertEqual(entered.get('id'), bravo_wolf, entered)

        # Play a moment: a chat both can hear about.
        alpha.send({'type': 'chat', 'text': 'Well met.', 'commandId': 'chat-1'})
        time.sleep(1.5)
        self.assertEqual(self.status()['players'], 2, 'the status counts the wolves in the world')

        # The same ticket twice: refused.
        replay = Socket(self.port)
        replay.lobby(stage='ticket')
        replay.send({'type': 'hello', 'ticket': ticket, 'protocol': PROTOCOL})
        self.assertIn('already used', replay.lobby(ok=False)['message'])
        replay.close()

        # Leave, and come back by a new ticket straight in, no arrival.
        alpha.send({'type': 'character_leave'})
        alpha.lobby(stage='ticket')
        alpha.close()
        bravo.close()
        time.sleep(0.5)
        for token, wolf in ((alpha_token, alpha_wolf), (bravo_token, bravo_wolf)):
            again = Socket(self.port)
            again.lobby(stage='ticket')
            again.send({'type': 'hello', 'ticket': self.ticket(token, wolf), 'protocol': PROTOCOL})
            entered = again.until(lambda e: e.get('type') == 'entered' or
                                  (e.get('type') == 'lobby' and (e.get('stage') == 'arrive' or not e.get('ok'))))
            self.assertEqual((entered['type'], entered.get('id')), ('entered', wolf), 'straight in: ' + json.dumps(entered))
            self.assertFalse([e for e in again.events if e.get('stage') == 'arrive'], 'no second arrival')
            again.send({'type': 'character_leave'})
            again.lobby(stage='ticket')
            again.close()
        subs = {claims_of(self.ticket(alpha_token, alpha_wolf))['sub'], claims_of(self.ticket(bravo_token, bravo_wolf))['sub']}

        # Stop the world (it saves) and sweep its save: central's ids, never a username or a password (doc 77, 12).
        type(self).server.send_signal(signal.SIGTERM)
        self.assertEqual(type(self).server.wait(timeout=60), 0)
        text = self.save.read_text(encoding='utf-8')
        save = json.loads(text)
        accounts = save['accounts']
        self.assertEqual((accounts['version'], accounts['kind']), (2, 'central'))
        self.assertEqual({e['username'] for e in accounts['entries']}, subs, 'the accounts are the tickets\' ids')
        self.assertEqual({w for e in accounts['entries'] for w in e['characters']}, {alpha_wolf, bravo_wolf})
        for username in USERNAMES:
            self.assertNotIn(username, text, 'no username anywhere in the save')
        for word in ('"salt"', '"verifier"', PASSWORD):
            self.assertNotIn(word, text)


    def test_ticket_mode_refuses_what_it_must(self):
        # Doc 77, 5: no development identities; no dev tools or open tiers on an official world; no page (5.1).
        tmp = Path(self.tmp.name)
        base = [str(self.server_binary), '--save', str(tmp / 'refused.json'), '--port', '0', '--perf-log', '0',
                '--workers', '0', '--for', '5', '--central', self.central, '--world-id', self.world, '--central-keys',
                str(self.keys)]
        for extra, why in ((['--dev-identity'], 'dev-identity'), (['--official', '--dev-tools'], 'official'),
                           (['--official', '--open-tiers'], 'official'), (['--web', str(tmp)], 'serves no page')):
            run = subprocess.run(base + extra, capture_output=True, text=True, timeout=120)
            self.assertNotEqual(run.returncode, 0, extra)
            self.assertIn('RATW_WORLD_REJECTED', run.stderr + run.stdout, extra)
            self.assertIn(why, run.stderr + run.stdout, extra)
            self.assertNotIn('listening on port', run.stdout, extra)
        run = subprocess.run([str(self.server_binary), '--save', str(tmp / 'refused.json'), '--world-id', self.world],
                             capture_output=True, text=True, timeout=60)
        self.assertNotEqual(run.returncode, 0, '--world-id without --central')


if __name__ == '__main__':
    unittest.main()
