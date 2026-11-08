#!/usr/bin/env python3
"""The world agent (Docs/Design/77-central-accounts-and-hosted-worlds.md, 5.1 and 7): what a world needs from central
over HTTPS, which the C++ server can't do itself (it has no HTTPS client): central's ticket keys, the heartbeat that
keeps the world on central's list, and central's ban list. Standard library only, so it runs in the world's container
as is.

  RATW_HOST_TOKEN=rh_... python3 tools/world_agent.py run --central URL --dir DIR --world-port PORT
        [--token-file FILE] [--description TEXT] [--region TEXT] [--tags a,b] [--host NAME] [--secure] [--every 60]

The world's own agent. DIR is the agent directory: the server runs with --central-keys DIR/central-keys.json, and the
two talk through files there, each written whole and atomically (a temporary file, then a rename):
- DIR/central-keys.json, kept here as `keys` keeps its FILE (below), with DIR/central-keys.json.wanted;
- DIR/online.json, written by the server about every 15 s: {"world", "at", "players", "online": [wolf ids]};
- DIR/bans.json, written here from each heartbeat's reply: {"at", "accounts": [the ticket `sub` of every account
  central has banned, as this world sees it]}. The server refuses a banned account's hello, and sends out a banned
  wolf already in the world.
Every --every seconds it posts POST URL/worlds/heartbeat with the host token (Authorization: Bearer): the listing
fields from the server's own http://127.0.0.1:PORT/status (name, protocol, build, clientData, limit, voices, cheats,
private), this command's description, region, tags, host name and secure flag, the port, and the wolves in
online.json (left out if it is more than a minute old or names another world). Central lists the world at the
address the heartbeat came from (or the host name, if it resolves there) once it has called the world back; the agent
logs when the world goes on or off the list, and why. If central's reply names another world than the server's
/status, the token is for another world: it says so loudly and posts no more. On SIGTERM or SIGINT it posts
POST URL/worlds/leave, so the world drops off the list at once. The host token comes from --token-file FILE or the
RATW_HOST_TOKEN environment variable, never an argument (anyone on the machine can read a process's arguments).

  python3 tools/world_agent.py keys --central URL --out FILE [--once] [--every SECONDS]

Fetches GET URL/keys and writes it to FILE whole and atomically (a temporary file, then a rename), for the server's
--central-keys FILE. It fetches at start, every SECONDS (a day by default: central publishes its next key a day before
it signs with it), and whenever the server asks: a ticket naming a key the world doesn't hold makes the server write
FILE.wanted, and the agent fetches again (at most once a minute) and removes it. The file also records central's clock
against this machine's (`offset`, seconds; GET URL/health), which the server logs. If central can't be reached, or
answers with no usable key, the file is left as it was: a world keeps its last keys (doc 77, 12).

http:// is for a central on this computer (development); anything else should be https://, whose certificate is
checked as usual. The heartbeat and the leave never follow a redirect (the token would go with them).
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import signal
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request

MAX_REPLY = 64 * 1024
KID = re.compile(r'^[A-Za-z0-9_.-]{1,64}$')
X = re.compile(r'^[A-Za-z0-9_-]{43}$')       # 32 bytes in unpadded base64url.
WANTED_EVERY = 60
WOLF = re.compile(r'^wolf-[0-9a-f]{32}$')
ONLINE_FRESH = 60                             # An online.json older than this is a stopped server's: nobody counted.
MOST_ONLINE = 10000
TOKEN = re.compile(r'^[\x21-\x7e]{1,256}$')   # (Printable, no spaces: a token pasted whole.)


class AgentError(RuntimeError):
    pass


def fetch_json(url: str, timeout: float = 10.0) -> dict:
    request = urllib.request.Request(url, headers={'Accept': 'application/json', 'User-Agent': 'ratw-world-agent/1'})
    try:
        with urllib.request.urlopen(request, timeout=timeout) as reply:
            body = reply.read(MAX_REPLY + 1)
    except (urllib.error.URLError, OSError, ValueError) as error:
        raise AgentError(f'{url}: {error}') from error
    if len(body) > MAX_REPLY:
        raise AgentError(f'{url}: the reply is too large')
    try:
        value = json.loads(body.decode('utf-8'))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise AgentError(f'{url}: not JSON') from error
    if not isinstance(value, dict):
        raise AgentError(f'{url}: not a JSON object')
    return value


def usable_keys(jwks: dict) -> list[dict]:
    """The Ed25519 keys of a JWK set, as the server reads them (tickets::readKeys); anything else left out."""
    keys = []
    for key in jwks.get('keys', []) if isinstance(jwks.get('keys'), list) else []:
        if isinstance(key, dict) and key.get('kty') == 'OKP' and key.get('crv') == 'Ed25519' and \
                key.get('alg') == 'Ed25519' and isinstance(key.get('kid'), str) and KID.match(key['kid']) and \
                isinstance(key.get('x'), str) and X.match(key['x']):
            keys.append({'kty': 'OKP', 'crv': 'Ed25519', 'alg': 'Ed25519', 'use': 'sig', 'kid': key['kid'], 'x': key['x']})
    return keys


def write_atomically(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp = tempfile.mkstemp(prefix=f'.{path.name}.', dir=path.parent)
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as out:
            out.write(text)
            out.flush()
            os.fsync(out.fileno())
        os.chmod(temp, 0o644)
        os.replace(temp, path)
    except BaseException:
        Path(temp).unlink(missing_ok=True)
        raise


def fetch_keys(central: str, out: Path) -> str:
    """Fetches central's keys into `out`; returns a line for the log. AgentError if nothing was written."""
    base = central.rstrip('/')
    before = time.time()
    try:
        health = fetch_json(base + '/health')
        after = time.time()
        offset = float(health['time']) - (before + after) / 2 if isinstance(health.get('time'), (int, float)) else 0.0
    except AgentError:
        offset = 0.0
    keys = usable_keys(fetch_json(base + '/keys'))
    if not keys:
        raise AgentError(f'{base}/keys: no usable Ed25519 key')
    write_atomically(out, json.dumps({'keys': keys, 'fetched': time.time(), 'offset': round(offset, 3),
                                      'central': base}, indent=1) + '\n')
    return f'{len(keys)} key(s): {", ".join(k["kid"] for k in keys)}; clock offset {offset:+.1f} s'


class KeyKeeper:
    """Central's keys in a file: fetched at start, every `every` seconds, and when the server asks (FILE.wanted, at
    most once a minute). A failed fetch is tried again within a minute; the file is left as it was."""

    def __init__(self, central: str, out: Path, every: float, log=print):
        self.central, self.out, self.every, self.log = central, out, every, log
        self.wanted = out.with_name(out.name + '.wanted')
        self.last_try = -1e9
        self.next_due = 0.0

    def step(self, now: float):
        """Fetches if it is due: True if it fetched, False if it tried and couldn't, None if it wasn't due."""
        asked = self.wanted.exists()
        if not (now >= self.next_due or (asked and now - self.last_try >= WANTED_EVERY)):
            return None
        self.last_try = now
        try:
            self.log(f'world agent: {fetch_keys(self.central, self.out)}' + (' (the server asked)' if asked else ''))
            self.wanted.unlink(missing_ok=True)
            self.next_due = now + self.every
            return True
        except (AgentError, OSError) as error:
            self.log(f'world agent: could not fetch central\'s keys: {error}')
            self.next_due = now + min(self.every, 60)
            return False


def keys_loop(central: str, out: Path, every: float, once: bool, log=print) -> int:
    stopping = []
    for sig in (signal.SIGTERM, signal.SIGINT):
        signal.signal(sig, lambda *_: stopping.append(True))
    keeper = KeyKeeper(central, out, every, log)
    while not stopping:
        fetched = keeper.step(time.monotonic())
        if once and fetched is not None:
            return 0 if fetched else 1
        time.sleep(1)
    return 0


class _NoRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None                             # (urllib then raises the 3xx as an HTTPError: never followed.)


_POSTER = urllib.request.build_opener(_NoRedirects)


def post_json(url: str, token: str, body: dict, timeout: float = 10.0) -> tuple[int, dict]:
    """POSTs `body` with the host token: (the HTTP status, the reply's JSON object or {}). AgentError if no reply."""
    request = urllib.request.Request(url, data=json.dumps(body).encode('utf-8'), method='POST', headers={
        'Content-Type': 'application/json', 'Accept': 'application/json', 'Authorization': f'Bearer {token}',
        'User-Agent': 'ratw-world-agent/1'})
    try:
        with _POSTER.open(request, timeout=timeout) as reply:
            status, raw = reply.status, reply.read(MAX_REPLY + 1)
    except urllib.error.HTTPError as error:
        status = error.code
        try:
            raw = error.read(MAX_REPLY + 1)
        except OSError:
            raw = b''
        finally:
            error.close()
    except (urllib.error.URLError, OSError, ValueError) as error:
        raise AgentError(f'{url}: {error}') from error
    try:
        value = json.loads(raw.decode('utf-8')) if len(raw) <= MAX_REPLY else {}
    except (UnicodeDecodeError, json.JSONDecodeError):
        value = {}
    return status, value if isinstance(value, dict) else {}


def read_token(token_file: Path | None) -> str:
    """The host token, from the file or RATW_HOST_TOKEN; AgentError if there is none that looks like one."""
    if token_file is not None:
        try:
            token = token_file.read_text(encoding='utf-8').strip()
        except (OSError, UnicodeDecodeError) as error:
            raise AgentError(f'the host token file can\'t be read: {error}') from error
    else:
        token = os.environ.get('RATW_HOST_TOKEN', '').strip()
    if not token:
        raise AgentError('no host token: give --token-file FILE or set RATW_HOST_TOKEN (make one on the Host page)')
    if not TOKEN.match(token):
        raise AgentError('the host token has spaces or odd characters in it: copy it again from the Host page')
    return token


# What the heartbeat takes from the server's own /status, by type (anything else there is left out).
STATUS_FIELDS = {'name': str, 'protocol': int, 'build': str, 'clientData': str, 'limit': int, 'voices': bool,
                 'cheats': bool, 'private': bool}


class Agent:
    """One world's heartbeat, ban list and keys (see `run` in the notes above)."""

    def __init__(self, central: str, directory: Path, world_port: int, token: str, *, description: str | None = None,
                 region: str | None = None, tags: list[str] | None = None, host: str | None = None,
                 secure: bool = False, every: float = 60, keys_every: float = 86400, log=print):
        self.central = central.rstrip('/')
        self.dir = directory
        self.world_port = world_port
        self.token = token
        self.extra: dict = {'secure': bool(secure)}
        if description is not None:
            self.extra['description'] = description
        if region is not None:
            self.extra['region'] = region
        if tags:
            self.extra['tags'] = tags
        if host:
            self.extra['host'] = host
        self.every = every
        self.log = log
        self.keys = KeyKeeper(self.central, directory / 'central-keys.json', keys_every, log)
        self.stop = threading.Event()
        self.halted = False                     # The token is another world's: no more heartbeats, and no leave.
        self.accepted = False                   # Central has answered a heartbeat (so a leave means something).
        self.state = None                       # What was last said about the listing, so each change is said once.
        self.online_note = None
        self.clock_said = False

    def say(self, state, line: str) -> None:
        if state != self.state:
            self.state = state
            self.log(f'world agent: {line}')

    def world_status(self) -> dict:
        status = fetch_json(f'http://127.0.0.1:{self.world_port}/status', timeout=5.0)
        if status.get('ratw') != 'world' or not isinstance(status.get('id'), str):
            raise AgentError(f'port {self.world_port} does not answer as a ticket-mode world')
        return status

    def online(self, world_id: str) -> list[str]:
        """The wolves in the world now, from the server's online.json; none if it is stale or another world's."""
        why = None
        try:
            value = json.loads((self.dir / 'online.json').read_text(encoding='utf-8'))
        except FileNotFoundError:
            value, why = None, 'no online.json yet'
        except (OSError, UnicodeDecodeError, json.JSONDecodeError):
            value, why = None, 'online.json can\'t be read'
        if why is None and (not isinstance(value, dict) or value.get('world') != world_id):
            why = 'online.json names another world (is --dir the server\'s?)'
        if why is None and (not isinstance(value.get('at'), (int, float)) or isinstance(value.get('at'), bool) or
                            time.time() - value['at'] > ONLINE_FRESH):
            why = 'online.json is more than a minute old'
        if why is not None:
            if why != self.online_note:
                self.online_note = why
                self.log(f'world agent: {why}: nobody is counted as playing')
            return []
        self.online_note = None
        wolves = value.get('online') if isinstance(value.get('online'), list) else []
        return [w for w in wolves if isinstance(w, str) and WOLF.match(w)][:MOST_ONLINE]

    def body(self, status: dict) -> dict:
        out: dict = {'port': self.world_port}
        for field, kind in STATUS_FIELDS.items():
            value = status.get(field)
            if isinstance(value, kind) and (kind is bool or not isinstance(value, bool)):
                out[field] = value
        out.update(self.extra)
        out['online'] = self.online(status['id'])
        return out

    def write_bans(self, bans) -> None:
        accounts = sorted({b for b in bans if isinstance(b, str) and 0 < len(b) <= 64}) if isinstance(bans, list) else []
        write_atomically(self.dir / 'bans.json', json.dumps({'at': int(time.time()), 'accounts': accounts}) + '\n')

    def heartbeat(self) -> bool:
        """One heartbeat: True if central answered it with this world's listing."""
        if self.halted:
            return False
        try:
            status = self.world_status()
        except AgentError as error:
            self.say(('world',), f'the world on port {self.world_port} does not answer ({error}): no heartbeat until it does')
            return False
        try:
            code, reply = post_json(self.central + '/worlds/heartbeat', self.token, self.body(status))
        except AgentError as error:
            self.say(('central',), f'could not reach central: {error}')
            return False
        if code != 200 or reply.get('ok') is not True:
            what = reply.get('code') if isinstance(reply.get('code'), str) else f'HTTP {code}'
            error = reply.get('error') if isinstance(reply.get('error'), str) else ''
            hint = {'bad_token': ' Make a new token on the Host page.',
                    'delisted': ' Central has taken this world off its list.',
                    'banned': ' The host account is banned.'}.get(what, '')
            self.say(('refused', what), f'central refused the heartbeat ({what}){": " + error if error else ""}.{hint}')
            return False
        if reply.get('world') != status['id']:
            self.halted = True
            for line in ('!' * 72,
                         f'THE HOST TOKEN IS FOR WORLD {reply.get("world")!r}, BUT THE SERVER ON PORT {self.world_port} '
                         f'IS WORLD {status["id"]!r}.',
                         'NO MORE HEARTBEATS: give this agent the token made for this world, or start the server with '
                         'the world id the token is for.',
                         '!' * 72):
                self.log(f'world agent: {line}')
            return False
        self.accepted = True
        try:
            self.write_bans(reply.get('bans'))
        except OSError as error:
            self.say(('bans',), f'could not write bans.json: {error}')
        listed = reply.get('listed') is True
        why = reply.get('why') if isinstance(reply.get('why'), str) else ''
        address = reply.get('address') if isinstance(reply.get('address'), str) else '?'
        if listed:
            self.say(('listed', address), f'listed on central at {address}')
        else:
            self.say(('not listed', why), f'not listed on central: {why or "(no reason given)"}')
        clock_off = isinstance(reply.get('time'), (int, float)) and abs(reply['time'] - time.time()) >= 60
        if clock_off and not self.clock_said:
            self.log(f'world agent: this machine\'s clock is {time.time() - reply["time"]:+.0f} s from central\'s: '
                     'tickets allow two minutes; set the clock right')
        self.clock_said = clock_off
        return True

    def leave(self) -> None:
        try:
            code, _ = post_json(self.central + '/worlds/leave', self.token, {}, timeout=5.0)
            self.log('world agent: left central\'s list' if code == 200 else f'world agent: the leave was refused (HTTP {code})')
        except AgentError as error:
            self.log(f'world agent: could not tell central the world is leaving: {error}')

    def run(self) -> int:
        self.log(f'world agent: heartbeats every {self.every:g} s for the world on port {self.world_port}, to '
                 f'{self.central} (agent directory {self.dir})')
        next_beat = 0.0
        while not self.stop.is_set():
            now = time.monotonic()
            self.keys.step(now)
            if not self.halted and now >= next_beat:
                next_beat = now + self.every
                self.heartbeat()
            self.stop.wait(min(0.5, self.every))
        if self.accepted and not self.halted:
            self.leave()
        return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = parser.add_subparsers(dest='command', required=True)
    run = sub.add_parser('run', help='the heartbeat, the ban list and the keys, for one world')
    run.add_argument('--central', required=True, help="central's address, http(s)://...")
    run.add_argument('--dir', required=True, type=Path,
                     help='the agent directory (the server runs with --central-keys DIR/central-keys.json)')
    run.add_argument('--world-port', required=True, type=int, help='the world server\'s port on this machine')
    run.add_argument('--token-file', type=Path, help='a file holding the host token (or set RATW_HOST_TOKEN)')
    run.add_argument('--description', help='the listing\'s description (300 characters at most)')
    run.add_argument('--region', help='the listing\'s region (32 characters at most)')
    run.add_argument('--tags', help='up to 5 tags, comma-separated (16 characters each at most)')
    run.add_argument('--host', help='a DNS name that resolves to this machine\'s address, listed in its place')
    run.add_argument('--secure', action='store_true',
                     help='players connect with wss:// (needs --host, and a certificate for it in front of the world)')
    run.add_argument('--every', type=float, default=60, help='seconds between heartbeats (default 60)')
    keys = sub.add_parser('keys', help="keep central's ticket keys in a file for the server")
    keys.add_argument('--central', required=True, help="central's address, http(s)://...")
    keys.add_argument('--out', required=True, type=Path, help='the file (the server\'s --central-keys)')
    keys.add_argument('--every', type=float, default=86400, help='seconds between fetches (default a day)')
    keys.add_argument('--once', action='store_true', help='fetch once and stop')
    args = parser.parse_args(argv)
    if not re.match(r'^https?://', args.central):
        parser.error('--central is http(s)://...')
    log = lambda line: print(line, flush=True)   # noqa: E731
    if args.command == 'run':
        if not 0 < args.world_port < 65536:
            parser.error('--world-port is 1-65535')
        if args.every <= 0:
            parser.error('--every is a number of seconds')
        if args.secure and not args.host:
            parser.error('--secure needs --host: a certificate is for a name')
        try:
            token = read_token(args.token_file)
        except AgentError as error:
            print(f'world agent: {error}', file=sys.stderr)
            return 2
        tags = [t.strip() for t in args.tags.split(',') if t.strip()] if args.tags else None
        agent = Agent(args.central, args.dir, args.world_port, token, description=args.description, region=args.region,
                      tags=tags, host=args.host, secure=args.secure, every=args.every, log=log)
        for sig in (signal.SIGTERM, signal.SIGINT):
            signal.signal(sig, lambda *_: agent.stop.set())
        return agent.run()
    if args.command == 'keys':
        return keys_loop(args.central, args.out, max(60.0, args.every) if not args.once else args.every, args.once, log=log)
    return 2


if __name__ == '__main__':
    sys.exit(main())
