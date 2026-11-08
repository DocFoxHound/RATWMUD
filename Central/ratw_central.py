#!/usr/bin/env python3
"""Central: accounts, wolves, tickets and the server list for every world (Docs/Design/77-central-accounts-and-hosted-
worlds.md, sections 4, 5 and 7; Phases 1-4). JSON over HTTP; behind Caddy (HTTPS) when it goes on its own server (Phase 7). On this
laptop it runs on loopback for development: worlds and the client reach it with `--central http://127.0.0.1:7790`.

  python3 Central/ratw_central.py db-create          make its database (ratw_central; the Postgres superuser from
                                                     Database/.env, no role made or changed)
  python3 Central/ratw_central.py migrate            apply Central/migrations
  python3 Central/ratw_central.py serve [--port 7790] [--bind 127.0.0.1] [--keys DIR] [--trust-proxy] [--cors ORIGIN]
                                         [--local-worlds]
                                                     run it (migrating first); keys in Saved/Central/keys by default;
                                                     --local-worlds (development) lists worlds on loopback and private
                                                     addresses, which are never listed otherwise
  python3 Central/ratw_central.py keys list|rotate [--now]
                                                     the ticket keys; rotate publishes the next one a day ahead, --now
                                                     signs with a new one at once and revokes every ticket out
  python3 Central/ratw_central.py worlds add --name N --kind official|community [--host USERNAME]
                                                     a world in the registry (prints its id)
  python3 Central/ratw_central.py worlds token WORLD_ID
                                                     makes its host token (prints only the token, on stdout's last
                                                     line; shown this once), revoking the one before
  python3 Central/ratw_central.py worlds revoke WORLD_ID      drops its token: off the list until it has a new one
  python3 Central/ratw_central.py worlds set WORLD_ID --delisted|--listed
  python3 Central/ratw_central.py worlds list        listed or not (and why), last heartbeat, reachable, check note,
                                                     players. A world is listed only by its heartbeat (doc 77, 7): run
                                                     the world agent with its token (tools/world_agent.py run)
  python3 Central/ratw_central.py accounts flag USERNAME [--banned|--unbanned] [--host|--no-host] [--admin|--no-admin]
  python3 Central/ratw_central.py accounts set-password USERNAME   (no email in the closed beta: the user resets by hand)
  python3 Central/ratw_central.py db-drop --yes       drop its database

The calls (JSON bodies; `Authorization: Bearer ACCESS_TOKEN` where an account is needed):
  POST /account/register {username, password, birth_year, birth_month?, remember?}   POST /account/login {username,
  password, remember?}   POST /account/refresh {refresh_token}   POST /account/logout {everywhere?}   GET /account
  DELETE /account {password}   GET /wolves   POST /wolves {name, age, appearance, family?, profile?}
  PATCH /wolves/WOLF_ID {revision, appearance?, profile?}   DELETE /wolves/WOLF_ID {password}   POST /wolves/name {name}   POST /tickets {world, wolf}
  GET /keys   GET /health   GET /rules (the creator's Gift families)   GET /worlds (the list)
The Host page's calls (an account with the host flag): GET /host/worlds   POST /host/worlds {name}
  POST /host/worlds/WORLD_ID/token   DELETE /host/worlds/WORLD_ID
The world agent's (Authorization: Bearer HOST_TOKEN): POST /worlds/heartbeat {port, name, protocol, ...}
  POST /worlds/leave
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
HEARTBEAT_BODY = 512 * 1024         # (Up to 10,000 online wolf ids.)
WOLF_PATH = re.compile(r'^/wolves/(wolf-[0-9a-f]{32})$')
HOST_WORLD_PATH = re.compile(r'^/host/worlds/(world-[0-9a-f]{32})$')
HOST_TOKEN_PATH = re.compile(r'^/host/worlds/(world-[0-9a-f]{32})/token$')


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
        ('GET', '/rules'): central.rules,
        ('GET', '/worlds'): central.list_worlds,
        ('POST', '/worlds/heartbeat'): central.heartbeat,
        ('POST', '/worlds/leave'): central.leave,
        ('GET', '/host/worlds'): central.host_worlds,
        ('POST', '/host/worlds'): central.host_make_world,
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

        def body(self, most: int = MAX_BODY) -> dict:
            if self.headers.get('Transfer-Encoding'):
                raise central_service.Reply(400, 'Transfer encoding is not supported.', 'bad_request')
            lengths = self.headers.get_all('Content-Length', [])
            if not lengths:
                return {}
            if len(lengths) != 1 or not lengths[0].isdigit():
                raise central_service.Reply(400, 'A bad Content-Length.', 'bad_request')
            size = int(lengths[0])
            if size > most:
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
                body = self.body(HEARTBEAT_BODY if path == '/worlds/heartbeat' else MAX_BODY)
                if method == 'GET' and path == '/health':
                    self.send(200, {'ok': True, 'time': central.clock()})
                    return
                match = WOLF_PATH.match(path)
                hosted, token = HOST_WORLD_PATH.match(path), HOST_TOKEN_PATH.match(path)
                if method == 'PATCH' and match:
                    status, reply = central.call(central.patch_wolf, address, self.bearer(), body, match.group(1))
                elif method == 'DELETE' and match:
                    status, reply = central.call(central.delete_wolf, address, self.bearer(), body, match.group(1))
                elif method == 'DELETE' and hosted:
                    status, reply = central.call(central.host_remove_world, address, self.bearer(), body,
                                                 hosted.group(1))
                elif method == 'POST' and token:
                    status, reply = central.call(central.host_new_token, address, self.bearer(), body, token.group(1))
                elif (method, path) in table:
                    status, reply = central.call(table[(method, path)], address, self.bearer(), body)
                else:
                    known = any(p == path for _, p in table) or bool(match or hosted or token) or path == '/health'
                    raise central_service.Reply(405 if known else 404, 'No such call.', 'not_found')
                self.send(status, reply, 'public, max-age=300' if path in ('/keys', '/rules') and status == 200 else 'no-store')
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


def worlds_command(parser, args, store: central_store.Store) -> int:
    """The world registry by hand. A world is listed only by its heartbeat (doc 77, 7), never by an address given
    here: make its host token, and run the world agent with it."""
    now = time.time()
    if args.action == 'add':
        if not args.name:
            parser.error('worlds add needs --name')
        host = None
        if args.host:
            account = store.account_by_username(central_service.normalize_username(args.host) or '')
            if account is None:
                print('no such account', file=sys.stderr)
                return 1
            host = account['id']
        import secrets
        world_id = 'world-' + secrets.token_hex(16)
        store.add_world(world_id, args.name, args.kind, now, host_account=host)
        print(world_id)
        return 0
    if args.action == 'list':
        for w in store.worlds():
            listing = w['listing'] if isinstance(w['listing'], dict) else {}
            why = central_service.Central.why_not_listed(w, now)
            print(json.dumps({'id': w['id'], 'name': listing.get('name') or w['name'], 'kind': w['kind'],
                              'listed': not why, 'why': why, 'last_heartbeat': w['last_heartbeat'],
                              'reachable': w['reachable'], 'check_note': w['check_note'], 'players': w['players'],
                              'address': w['address'], 'port': w['port'], 'host_name': w['host_name'],
                              'delisted': w['delisted'], 'removed': w['removed'], 'token': w['has_token']}))
        return 0
    world = store.world(args.world) if args.world else None
    if world is None:
        print('no such world', file=sys.stderr)
        return 1
    if args.action == 'token':
        if world['removed']:
            print('that world was removed', file=sys.stderr)
            return 1
        token = central_service.new_host_token()
        store.set_world_token(world['id'], central_service.token_hash(token), now)
        print('The host token, shown this once (the one before it no longer works). Give it to the world agent '
              'through --token-file or RATW_HOST_TOKEN:', file=sys.stderr, flush=True)
        print(token)
        return 0
    if args.action == 'revoke':
        store.set_world_token(world['id'], None, now)
        print('token revoked: the world is off the list until it has a new one')
        return 0
    if args.delisted is None:
        parser.error('worlds set needs --delisted or --listed')
    store.set_world(world['id'], delisted=args.delisted)
    print(f'{world["id"]} {"delisted" if args.delisted else "may be listed (while its heartbeat arrives)"}')
    return 0


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
    serve.add_argument('--local-worlds', action='store_true', help='development: list worlds whose heartbeat comes '
                       'from loopback or a private address (never link-local, unspecified or multicast)')
    keys = sub.add_parser('keys')
    keys.add_argument('action', choices=('list', 'rotate'))
    keys.add_argument('--now', action='store_true')
    keys.add_argument('--keys', type=Path, default=DEFAULT_KEYS)
    worlds = sub.add_parser('worlds')
    worlds.add_argument('action', choices=('add', 'token', 'revoke', 'set', 'list'))
    worlds.add_argument('world', nargs='?', help='the world id (token, revoke, set)')
    worlds.add_argument('--name')
    worlds.add_argument('--kind', choices=('official', 'community'), default='community')
    worlds.add_argument('--host', help="the username of the account that hosts it (worlds add)")
    worlds.add_argument('--delisted', dest='delisted', action='store_true', default=None)
    worlds.add_argument('--listed', dest='delisted', action='store_false')
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
            return worlds_command(parser, args, store)
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
                                      central_identity.Identity(),
                                      settings=central_service.Settings(local_worlds=args.local_worlds))
    store.migrate()
    for line in central.maintain():
        print(f'central: {line}', flush=True)
    server = Server(central, args.bind, args.port, args.trust_proxy, args.cors).start()
    print(f'ratw_central listening on http://{args.bind}:{server.port}/ (database {store.info["dbname"]}, '
          f'keys {args.keys}{", local worlds listed" if args.local_worlds else ""})', flush=True)
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
