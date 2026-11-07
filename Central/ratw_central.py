#!/usr/bin/env python3
"""Central: accounts, wolves and tickets for every world (Docs/Design/77-central-accounts-and-hosted-worlds.md,
sections 4 and 5; Phase 1). JSON over HTTP; behind Caddy (HTTPS) when it goes on its own server (Phase 7). On this
laptop it runs on loopback for development: worlds and the client reach it with `--central http://127.0.0.1:7790`.

  python3 Central/ratw_central.py db-create          make its database (ratw_central; the Postgres superuser from
                                                     Database/.env, no role made or changed)
  python3 Central/ratw_central.py migrate            apply Central/migrations
  python3 Central/ratw_central.py serve [--port 7790] [--bind 127.0.0.1] [--keys DIR] [--trust-proxy] [--cors ORIGIN]
                                                     run it (migrating first); keys in Saved/Central/keys by default
  python3 Central/ratw_central.py keys list|rotate [--now]
                                                     the ticket keys; rotate publishes the next one a day ahead, --now
                                                     signs with a new one at once and revokes every ticket out
  python3 Central/ratw_central.py worlds add --name N --kind official|community | worlds list
                                                     the world registry by hand (the heartbeat and the list: Phase 4)
  python3 Central/ratw_central.py accounts flag USERNAME [--banned|--unbanned] [--host|--no-host] [--admin|--no-admin]
  python3 Central/ratw_central.py accounts set-password USERNAME   (no email in the closed beta: the user resets by hand)
  python3 Central/ratw_central.py db-drop --yes       drop its database

The calls (JSON bodies; `Authorization: Bearer ACCESS_TOKEN` where an account is needed):
  POST /account/register {username, password, birth_year, birth_month?, remember?}   POST /account/login {username,
  password, remember?}   POST /account/refresh {refresh_token}   POST /account/logout {everywhere?}   GET /account
  DELETE /account {password}   GET /wolves   POST /wolves {name, age, appearance, family?, profile?}
  PATCH /wolves/WOLF_ID {revision, appearance?, profile?}   POST /wolves/name {name}   POST /tickets {world, wolf}
  GET /keys   GET /health
Replies are {"ok": true, ...} or {"ok": false, "error": "...", "code": "..."} with the HTTP status; a rate limit is
429 with Retry-After.

Settings: RATW_CENTRAL_DBNAME, RATW_CENTRAL_ROLE, RATW_CENTRAL_HOST/PORT (Central/central_store.py); RATW_IDENTITY
(the ratw_identity binary, Central/central_identity.py).
"""
from __future__ import annotations

import argparse
import getpass
import ipaddress
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import re
import sys
import threading
import time
from urllib.parse import urlsplit

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import central_identity  # noqa: E402
import central_service  # noqa: E402
import central_store  # noqa: E402
import central_tickets  # noqa: E402

ROOT = HERE.parent
DEFAULT_KEYS = ROOT / 'Saved' / 'Central' / 'keys'
DEFAULT_PORT = 7790
MAX_BODY = 16 * 1024
WOLF_PATH = re.compile(r'^/wolves/(wolf-[0-9a-f]{32})$')


def routes(central: central_service.Central) -> dict:
    return {
        ('POST', '/account/register'): central.register,
        ('POST', '/account/login'): central.login,
        ('POST', '/account/refresh'): central.refresh,
        ('POST', '/account/logout'): central.logout,
        ('DELETE', '/account'): central.delete_account,
        ('GET', '/account'): central.me,
        ('GET', '/wolves'): central.list_wolves,
        ('POST', '/wolves'): central.create_wolf,
        ('POST', '/wolves/name'): central.name_free,
        ('POST', '/tickets'): central.ticket,
        ('GET', '/keys'): central.jwks,
    }


def handler_class(central: central_service.Central, trust_proxy: bool = False, cors: tuple = ()):
    table = routes(central)

    class Handler(BaseHTTPRequestHandler):
        server_version = 'ratw-central/1'
        protocol_version = 'HTTP/1.1'

        def log_message(self, *_):
            pass        # (Passwords and tokens travel in bodies and headers; nothing of a request is logged.)

        def address(self) -> str:
            peer = self.client_address[0]
            if trust_proxy and ipaddress.ip_address(peer).is_loopback:
                forwarded = [p.strip() for p in ','.join(self.headers.get_all('X-Forwarded-For', [])).split(',')]
                if forwarded and forwarded[-1]:
                    try:
                        return str(ipaddress.ip_address(forwarded[-1]))
                    except ValueError:
                        pass
            return peer

        def send(self, status: int, body: dict, cache: str = 'no-store'):
            data = json.dumps(body, separators=(',', ':'), ensure_ascii=False).encode('utf-8')
            self.send_response(status)
            self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Cache-Control', cache)
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Referrer-Policy', 'no-referrer')
            if status == 429 and body.get('retry_after') is not None:
                self.send_header('Retry-After', str(int(body['retry_after']) + 1))
            origin = self.headers.get('Origin')
            if origin and origin in cors:
                self.send_header('Access-Control-Allow-Origin', origin)
                self.send_header('Vary', 'Origin')
            self.end_headers()
            self.wfile.write(data)

        def body(self) -> dict:
            if self.headers.get('Transfer-Encoding'):
                raise central_service.Reply(400, 'Transfer encoding is not supported.', 'bad_request')
            lengths = self.headers.get_all('Content-Length', [])
            if not lengths:
                return {}
            if len(lengths) != 1 or not lengths[0].isdigit():
                raise central_service.Reply(400, 'A bad Content-Length.', 'bad_request')
            size = int(lengths[0])
            if size > MAX_BODY:
                raise central_service.Reply(413, 'The request is too large.', 'too_large')
            if size == 0:
                return {}
            if self.headers.get('Content-Type', '').split(';')[0].strip().lower() != 'application/json':
                raise central_service.Reply(415, 'Send JSON.', 'bad_request')
            self.connection.settimeout(10)
            raw = self.rfile.read(size)
            try:
                value = json.loads(raw.decode('utf-8'))
            except (UnicodeDecodeError, json.JSONDecodeError):
                raise central_service.Reply(400, 'The body is not JSON.', 'bad_request') from None
            if not isinstance(value, dict):
                raise central_service.Reply(400, 'The body is not a JSON object.', 'bad_request')
            return value

        def bearer(self) -> str | None:
            values = self.headers.get_all('Authorization', [])
            if len(values) == 1 and values[0].startswith('Bearer ') and values[0].isascii():
                return values[0][7:].strip() or None
            return None

        def handle_call(self, method: str):
            try:
                path = urlsplit(self.path).path
                address = self.address()
                wait = central.limiter.hit(('any', address), central.settings.limits.any_per_address)
                if wait:
                    raise central_service.Reply(429, 'Too many requests from here.', 'rate_limited', wait)
                body = self.body()
                if method == 'GET' and path == '/health':
                    self.send(200, {'ok': True, 'time': central.clock()})
                    return
                match = WOLF_PATH.match(path)
                if method == 'PATCH' and match:
                    status, reply = central.call(central.patch_wolf, address, self.bearer(), body, match.group(1))
                elif (method, path) in table:
                    status, reply = central.call(table[(method, path)], address, self.bearer(), body)
                else:
                    known = any(p == path for _, p in table) or bool(match) or path == '/health'
                    raise central_service.Reply(405 if known else 404, 'No such call.', 'not_found')
                self.send(status, reply, 'public, max-age=300' if path == '/keys' and status == 200 else 'no-store')
            except central_service.Reply as reply:
                self.close_connection = True        # (The body may not have been read.)
                body = {'ok': False, 'error': reply.error, 'code': reply.code}
                if reply.retry_after is not None:
                    body['retry_after'] = round(reply.retry_after, 3)
                self.send(reply.status, body)
            except Exception as error:      # (Database down, a bug: never the details to the caller.)
                print(f'central: {method} {urlsplit(self.path).path}: {type(error).__name__}: {error}',
                      file=sys.stderr, flush=True)
                self.send(503, {'ok': False, 'error': 'Central could not do that just now.', 'code': 'unavailable'})

        def do_GET(self):
            self.handle_call('GET')

        def do_POST(self):
            self.handle_call('POST')

        def do_PATCH(self):
            self.handle_call('PATCH')

        def do_DELETE(self):
            self.handle_call('DELETE')

        def do_OPTIONS(self):
            origin = self.headers.get('Origin')
            self.send_response(204 if origin in cors else 403)
            if origin in cors:
                self.send_header('Access-Control-Allow-Origin', origin)
                self.send_header('Access-Control-Allow-Methods', 'GET, POST, PATCH, DELETE')
                self.send_header('Access-Control-Allow-Headers', 'Authorization, Content-Type')
                self.send_header('Access-Control-Max-Age', '600')
                self.send_header('Vary', 'Origin')
            self.send_header('Content-Length', '0')
            self.end_headers()

    return Handler


class Server:
    """Central on a port, with its housekeeping thread (keys, pruning) every `every` seconds."""

    def __init__(self, central: central_service.Central, bind: str = '127.0.0.1', port: int = DEFAULT_PORT,
                 trust_proxy: bool = False, cors: tuple = (), every: float = 300):
        self.central = central
        self.http = ThreadingHTTPServer((bind, port), handler_class(central, trust_proxy, tuple(cors)))
        self.http.daemon_threads = True
        self.port = self.http.server_address[1]
        self.every = every
        self._stop = threading.Event()
        self._threads: list[threading.Thread] = []

    def _housekeeping(self):
        while not self._stop.wait(self.every):
            try:
                for line in self.central.maintain():
                    print(f'central: {line}', flush=True)
            except Exception as error:
                print(f'central: housekeeping failed: {error}', file=sys.stderr, flush=True)

    def start(self):
        for target in (self.http.serve_forever, self._housekeeping):
            t = threading.Thread(target=target, daemon=True)
            t.start()
            self._threads.append(t)
        return self

    def stop(self):
        self._stop.set()
        self.http.shutdown()
        self.http.server_close()


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('db-create')
    drop = sub.add_parser('db-drop')
    drop.add_argument('--yes', action='store_true')
    sub.add_parser('migrate')
    serve = sub.add_parser('serve')
    serve.add_argument('--port', type=int, default=DEFAULT_PORT)
    serve.add_argument('--bind', default='127.0.0.1')
    serve.add_argument('--keys', type=Path, default=DEFAULT_KEYS)
    serve.add_argument('--trust-proxy', action='store_true', help='take the address from X-Forwarded-For when the '
                       'connection comes from loopback (behind Caddy)')
    serve.add_argument('--cors', action='append', default=[], help='an Origin allowed to call it from a page')
    keys = sub.add_parser('keys')
    keys.add_argument('action', choices=('list', 'rotate'))
    keys.add_argument('--now', action='store_true')
    keys.add_argument('--keys', type=Path, default=DEFAULT_KEYS)
    worlds = sub.add_parser('worlds')
    worlds.add_argument('action', choices=('add', 'list'))
    worlds.add_argument('--name')
    worlds.add_argument('--kind', choices=('official', 'community'), default='community')
    accounts = sub.add_parser('accounts')
    accounts.add_argument('action', choices=('flag', 'set-password'))
    accounts.add_argument('username')
    for flag in ('banned', 'host', 'admin'):
        accounts.add_argument(f'--{flag}', dest=flag, action='store_true', default=None)
        accounts.add_argument(f'--{"unbanned" if flag == "banned" else "no-" + flag}', dest=flag,
                              action='store_false')
    args = parser.parse_args(argv)

    if args.command == 'db-create':
        name = central_store.conninfo()['dbname']
        print(f'created {name}' if central_store.create_database(name) else f'{name} exists already')
        return 0
    if args.command == 'db-drop':
        if not args.yes:
            parser.error('db-drop deletes every account: add --yes')
        central_store.drop_database(central_store.conninfo()['dbname'])
        return 0
    if args.command == 'keys':
        ring = central_tickets.KeyRing(args.keys)
        now = time.time()
        if args.action == 'rotate':
            print('new key', ring.rotate(now, immediate=args.now))
        ring.maintain(now)
        for k in ring.describe(now):
            print(json.dumps(k))
        return 0
    store = central_store.Store()
    try:
        if args.command == 'migrate':
            print('\n'.join(store.migrate()) or 'up to date')
            return 0
        store.migrate()
        if args.command == 'worlds':
            if args.action == 'add':
                if not args.name:
                    parser.error('worlds add needs --name')
                import secrets
                world_id = 'world-' + secrets.token_hex(16)
                store.add_world(world_id, args.name, args.kind, time.time())
                print(world_id)
            else:
                for w in store.worlds():
                    print(json.dumps(w))
            return 0
        if args.command == 'accounts':
            account = store.account_by_username(central_service.normalize_username(args.username) or '')
            if account is None:
                print('no such account', file=sys.stderr)
                return 1
            if args.action == 'flag':
                flags = {k: getattr(args, k) for k in ('banned', 'host', 'admin') if getattr(args, k) is not None}
                store.set_flags(account['id'], **flags)
                if flags.get('banned'):
                    store.end_sessions(account['id'])
                print(json.dumps({k: v for k, v in store.account(account['id']).items() if k != 'password'}))
                return 0
            password = getpass.getpass('New password: ')
            if not central_service.valid_password(password):
                print('A password is 12-128 bytes without control characters.', file=sys.stderr)
                return 1
            store.set_password(account['id'], central_service.hash_password(password))
            store.end_sessions(account['id'])
            print('password set; every session of the account ended')
            return 0
    finally:
        store.close()
    # serve
    central = central_service.Central(store := central_store.Store(), central_tickets.KeyRing(args.keys),
                                      central_identity.Identity())
    store.migrate()
    for line in central.maintain():
        print(f'central: {line}', flush=True)
    server = Server(central, args.bind, args.port, args.trust_proxy, args.cors).start()
    print(f'ratw_central listening on http://{args.bind}:{server.port}/ (database {store.info["dbname"]}, '
          f'keys {args.keys})', flush=True)
    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        pass
    finally:
        server.stop()
        central.identity.close()
        store.close()
    return 0


if __name__ == '__main__':
    sys.exit(main())
