#!/usr/bin/env python3
"""The world agent (Docs/Design/77-central-accounts-and-hosted-worlds.md, 5.1 and 7): what a world needs from central
over HTTPS, which the C++ server can't do itself (it has no HTTPS client): central's ticket keys, the heartbeat that
keeps the world on central's list, and central's ban list. Standard library only, so it runs in the world's container
as is.

  RATW_HOST_TOKEN=rh_... python3 tools/world_agent.py run --central URL --dir DIR --world-port PORT
        [--central-dir CDIR] [--token-file FILE] [--description TEXT] [--region TEXT] [--tags a,b] [--host NAME] [--secure] [--every 60]
        [--upnp-port PORT [--upnp-name NAME]]

The world's own agent. DIR is the agent directory: the server runs with --central-keys DIR/central-keys.json, and the
two talk through files there, each written whole and atomically (a temporary file, then a rename):
- DIR/central-keys.json, kept here as `keys` keeps its FILE (below), with DIR/central-keys.json.wanted;
- DIR/online.json, written by the server about every 15 s: {"world", "at", "players", "online": [wolf ids]};
- DIR/bans.json, written here from each heartbeat's reply: {"at", "accounts": [the ticket `sub` of every account
  central has banned, as this world sees it]}. The server refuses a banned account's hello, and sends out a banned
  wolf already in the world.
- DIR/host.json, written here from each heartbeat's reply: {"at", "host": the ticket `sub` of the world's host account
  here, or null}. The server gives that account's wolves the host commands (doc 77, 3.2). An official world has no
  host; a heartbeat refused because the token is unknown, the world delisted or the host banned writes null, so the
  host's wolves lose them.
- CDIR/central.json, with --central-dir: {"at", "kind": central's kind for the world ("community", "official", or null
  until central has said), "host": as host.json's}, written from each heartbeat's reply as host.json is (a refused
  token, a delisted world or a banned host: the host null, the kind kept). CDIR is a directory only the agent's own
  user may write (Docs/Design/78-hosts-run-their-own-worlds.md, 6.7): the agent directory is the game server's to
  write too, so central's word that the world is official, which locks the host's Editor for good, is never taken
  from there. The server never reads it.
Every --every seconds it posts POST URL/worlds/heartbeat with the host token (Authorization: Bearer): the listing
fields from the server's own http://127.0.0.1:PORT/status (name, protocol, build, clientData, limit, voices, cheats,
private), this command's description, region, tags, host name and secure flag, the port, and the wolves in
online.json (left out if it is more than a minute old or names another world). Central lists the world at the
address the heartbeat came from (or the host name, if it resolves there) once it has called the world back; the agent
logs when the world goes on or off the list, and why. If central's reply names another world than the server's
/status, the token is for another world: it says so loudly and posts no more. On SIGTERM or SIGINT it posts
POST URL/worlds/leave, so the world drops off the list at once. The host token comes from --token-file FILE or the
RATW_HOST_TOKEN environment variable, never an argument (anyone on the machine can read a process's arguments).

  python3 tools/world_agent.py keys --central URL --out FILE [--once] [--every SECONDS] [--upnp-port PORT ...]

Fetches GET URL/keys and writes it to FILE whole and atomically (a temporary file, then a rename), for the server's
--central-keys FILE. It fetches at start, every SECONDS (a day by default: central publishes its next key a day before
it signs with it), and whenever the server asks: a ticket naming a key the world doesn't hold makes the server write
FILE.wanted, and the agent fetches again (at most once a minute) and removes it. The file also records central's clock
against this machine's (`offset`, seconds; GET URL/health), which the server logs. If central can't be reached, or
answers with no usable key, the file is left as it was: a world keeps its last keys (doc 77, 12).

`run` takes http:// only for a central on this computer or on a private network (development, or a PC testing
against a laptop on the same LAN: loopback, 10/8, 172.16/12, 192.168/16, link-local, and the like; a name must resolve
to such addresses only, and is looked at again before each post); a central on the internet is https://, whose
certificate is checked as usual, since the host token goes with every heartbeat. `keys` carries no token and takes
either (it warns about http:// to the internet). The heartbeat and the leave never follow a redirect (the token would
go with them), and the world's own /status is fetched straight from 127.0.0.1, never through an HTTP(S)_PROXY.

With --upnp-port PORT (`run` or `keys`, not --once), the agent first tries UPnP once (tools/world_upnp.py, doc 77
section 8): it asks the router to forward PORT (TCP) to this computer, the mapping's description naming the world
(--upnp-name), before the first heartbeat, so central's first call-back already finds it; it renews the lease while it
runs and removes the mapping when it stops cleanly. One line says what happened. Only where this computer is on the
router's own network: the world package's container isn't, so there compose.yaml's upnp service does it instead.
"""
from __future__ import annotations

import argparse
import http.client
import ipaddress
import json
import os
from pathlib import Path
import re
import signal
import socket
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

MAX_REPLY = 64 * 1024
MAX_HEARTBEAT_REPLY = 8 << 20                 # Every ban rides on it (~41 bytes each): the server's MaxBansFile.
HEARTBEAT_TIMEOUT = 30.0                      # Central may look up a host name and call the world back (5 s) first.
KID = re.compile(r'^[A-Za-z0-9_.-]{1,64}$')
X = re.compile(r'^[A-Za-z0-9_-]{43}$')       # 32 bytes in unpadded base64url.
WANTED_EVERY = 60
WOLF = re.compile(r'^wolf-[0-9a-f]{32}$')
ONLINE_FRESH = 60                             # An online.json older than this is a stopped server's: nobody counted.
MOST_ONLINE = 10000
TOKEN = re.compile(r'^[\x21-\x7e]{1,256}$')   # (Printable, no spaces: a token pasted whole.)


class AgentError(RuntimeError):
    pass


class AgentTimeout(AgentError):
    """Central (or the world) did not answer in time."""


def _timed_out(error: BaseException) -> bool:
    return isinstance(error, TimeoutError) or isinstance(getattr(error, 'reason', None), TimeoutError)


class _NoRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None                             # (urllib then raises the 3xx as an HTTPError: never followed.)


_POSTER = urllib.request.build_opener(_NoRedirects)
# The world's own /status on 127.0.0.1: never through a proxy (HTTP_PROXY with no NO_PROXY would send it there), and
# never redirected elsewhere.
_LOCAL = urllib.request.build_opener(urllib.request.ProxyHandler({}), _NoRedirects)


def fetch_json(url: str, timeout: float = 10.0, opener=None) -> dict:
    request = urllib.request.Request(url, headers={'Accept': 'application/json', 'User-Agent': 'ratw-world-agent/1'})
    try:
        with (opener.open(request, timeout=timeout) if opener else urllib.request.urlopen(request, timeout=timeout)) as reply:
            body = reply.read(MAX_REPLY + 1)
    except (urllib.error.URLError, OSError, ValueError, http.client.HTTPException) as error:
        # (HTTPException: something on the port that doesn't speak HTTP, or a reply cut short.)
        if isinstance(error, urllib.error.HTTPError):
            error.close()
        if _timed_out(error):
            raise AgentTimeout(f'{url}: no answer in {timeout:g} s') from error
        raise AgentError(f'{url}: {error!r}' if isinstance(error, http.client.HTTPException) else f'{url}: {error}') from error
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
        except Exception as error:                # (Anything else: said, and tried again within a minute.)
            self.log(f'world agent: could not fetch central\'s keys: {type(error).__name__}: {error}')
            self.next_due = now + min(self.every, 60)
            return False


class Upnp:
    """The agent's UPnP attempt (tools/world_upnp.py): made once by `open`, renewed by `step`, removed by `close`.
    Nothing at all without a port. `options` are world_upnp.open_mapping's (the tests' fake router)."""

    def __init__(self, port: int | None, name: str, log, **options):
        self.port, self.name, self.options, self.mapping = port, name, options, None
        self.log = lambda line: log(f'world agent: {line}')

    def open(self) -> None:
        if self.port:
            import world_upnp                  # (Beside this file: tools/, or /opt/ratw/tools in the image.)
            self.mapping = world_upnp.open_mapping(self.port, self.name, self.log, **self.options)

    def step(self) -> None:
        if self.mapping is not None:
            try:
                self.mapping.step()
            except Exception as error:          # (Mapping.step says its own; never the heartbeat's loop.)
                self.log(f'UPnP: unexpected trouble renewing the mapping ({type(error).__name__}: {error})')

    def close(self) -> None:
        if self.mapping is not None:
            try:
                self.mapping.close()
            except Exception as error:
                self.log(f'UPnP: unexpected trouble removing the mapping ({type(error).__name__}: {error})')
            self.mapping = None


def keys_loop(central: str, out: Path, every: float, once: bool, log=print, upnp: Upnp | None = None) -> int:
    stopping = []
    for sig in (signal.SIGTERM, signal.SIGINT):
        signal.signal(sig, lambda *_: stopping.append(True))
    keeper = KeyKeeper(central, out, every, log)
    upnp = upnp or Upnp(None, '', log)
    if not once:
        upnp.open()
    while not stopping:
        fetched = keeper.step(time.monotonic())
        if once and fetched is not None:
            return 0 if fetched else 1
        upnp.step()
        time.sleep(1)
    upnp.close()
    return 0


def plain_http_allowed(url: str) -> tuple[bool | None, str]:
    """Whether the host token may go to `url` in clear: https://, or http:// to this computer or a private network
    (every address its name has is loopback, private, link-local or otherwise not on the internet). (allowed, why not):
    allowed is None when the name can't be looked up now (so it can't be told; nothing is sent until it can)."""
    parts = urllib.parse.urlsplit(url)
    if parts.scheme == 'https':
        return True, ''
    host = parts.hostname or ''
    if not host:
        return False, f'{url} names no host'
    try:
        addresses = {ipaddress.ip_address(host)}
    except ValueError:
        try:
            addresses = {ipaddress.ip_address(info[4][0].split('%')[0])
                         for info in socket.getaddrinfo(host, parts.port or 80, proto=socket.IPPROTO_TCP)}
        except (OSError, ValueError, UnicodeError) as error:
            return None, f'{host} can\'t be looked up ({error})'
    public = sorted(str(a) for a in addresses if a.is_global or (a.version == 6 and a.ipv4_mapped and a.ipv4_mapped.is_global))
    if not addresses or public:
        return False, (f'{host} is on the internet ({", ".join(public)})' if public else f'{host} has no address')
    return True, ''


def post_json(url: str, token: str, body: dict, timeout: float = 10.0,
              max_reply: int = MAX_REPLY) -> tuple[int, dict | None]:
    """POSTs `body` with the host token: (the HTTP status, the reply's JSON object, or None if it was larger than
    `max_reply` or not a JSON object). AgentError if no reply (AgentTimeout if none in `timeout` seconds)."""
    request = urllib.request.Request(url, data=json.dumps(body).encode('utf-8'), method='POST', headers={
        'Content-Type': 'application/json', 'Accept': 'application/json', 'Authorization': f'Bearer {token}',
        'User-Agent': 'ratw-world-agent/1'})
    try:
        with _POSTER.open(request, timeout=timeout) as reply:
            status, raw = reply.status, reply.read(max_reply + 1)
    except urllib.error.HTTPError as error:
        status = error.code
        try:
            raw = error.read(max_reply + 1)
        except (OSError, http.client.HTTPException):
            raw = b''
        finally:
            error.close()
    except (urllib.error.URLError, OSError, ValueError, http.client.HTTPException) as error:
        if _timed_out(error):
            raise AgentTimeout(f'{url}: no answer in {timeout:g} s') from error
        raise AgentError(f'{url}: {error!r}' if isinstance(error, http.client.HTTPException) else f'{url}: {error}') from error
    try:
        value = json.loads(raw.decode('utf-8')) if len(raw) <= max_reply else None
    except (UnicodeDecodeError, json.JSONDecodeError):
        value = None
    return status, value if isinstance(value, dict) else None


def read_token(token_file: Path | None) -> str:
    """The host token, from the file or RATW_HOST_TOKEN; AgentError if there is none that looks like one."""
    if token_file is not None:
        try:
            token = token_file.read_text(encoding='utf-8-sig').strip()   # (-sig: Notepad's byte-order mark.)
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
                 secure: bool = False, every: float = 60, keys_every: float = 86400, log=print,
                 upnp_port: int | None = None, upnp_name: str = '', central_dir: Path | None = None):
        self.central = central.rstrip('/')
        self.dir = directory
        self.central_dir = central_dir          # (central.json: the world's kind and host, where only this user writes.)
        self.kind = None                        # Central's kind for the world, as last said (kept across restarts).
        if central_dir is not None:
            try:
                kept = json.loads((central_dir / 'central.json').read_text(encoding='utf-8'))
                if isinstance(kept, dict) and kept.get('kind') in ('community', 'official'):
                    self.kind = kept['kind']
            except (OSError, UnicodeDecodeError, ValueError):
                pass
        self.central_failing = False            # central.json can't be written: said when it starts and when it mends.
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
        self.bans_failing = False               # bans.json can't be written: said when it starts and when it mends.
        self.host_failing = False               # Likewise host.json.
        self.trouble = None                     # The last unexpected error in the loop, said once.
        self.heartbeat_timeout = HEARTBEAT_TIMEOUT
        self.plain = urllib.parse.urlsplit(self.central).scheme != 'https'
        self.online_note = None
        self.clock_said = False
        self.upnp = Upnp(upnp_port, upnp_name, log)

    def say(self, state, line: str) -> None:
        if state != self.state:
            self.state = state
            self.log(f'world agent: {line}')

    def world_status(self) -> dict:
        status = fetch_json(f'http://127.0.0.1:{self.world_port}/status', timeout=5.0, opener=_LOCAL)
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

    def write_host(self, host) -> None:
        """host.json: the host account's ticket sub here, or null (none, or no longer the host). Said once when it
        can't be written, and once when it mends."""
        host = host if isinstance(host, str) and 0 < len(host) <= 64 else None
        self.write_central(host)
        try:
            write_atomically(self.dir / 'host.json', json.dumps({'at': int(time.time()), 'host': host}) + '\n')
            if self.host_failing:
                self.host_failing = False
                self.log('world agent: host.json is written again')
        except OSError as error:
            if not self.host_failing:
                self.host_failing = True
                self.log(f'world agent: could not write host.json: {error} (the world keeps its last host)')

    def write_central(self, host) -> None:
        """central.json in the agent's own directory (--central-dir): central's kind for the world and its host, as the
        last heartbeat's reply said them (doc 78, 6.7). Said once when it can't be written, and once when it mends."""
        if self.central_dir is None:
            return
        try:
            write_atomically(self.central_dir / 'central.json',
                             json.dumps({'at': int(time.time()), 'kind': self.kind, 'host': host}) + '\n')
            if self.central_failing:
                self.central_failing = False
                self.log('world agent: central.json is written again')
        except OSError as error:
            if not self.central_failing:
                self.central_failing = True
                self.log(f'world agent: could not write central.json: {error}')

    def heartbeat(self) -> bool:
        """One heartbeat: True if central answered it with this world's listing."""
        if self.halted:
            return False
        try:
            status = self.world_status()
        except AgentError as error:
            self.say(('world',), f'the world on port {self.world_port} does not answer ({error}): no heartbeat until it does')
            return False
        if self.plain:
            allowed, why = plain_http_allowed(self.central)
            if allowed is not True:             # (Looked at again each time: a name may come to resolve elsewhere.)
                self.say(('plain http', why), f'no heartbeat: {why}, and the host token would go in clear over http:// ' +
                         ('until it is known to be this computer or a private network' if allowed is None else
                          'to the internet: give central\'s https:// address'))
                return False
        try:
            code, reply = post_json(self.central + '/worlds/heartbeat', self.token, self.body(status),
                                    timeout=self.heartbeat_timeout, max_reply=MAX_HEARTBEAT_REPLY)
        except AgentTimeout:
            self.say(('timeout',), f'central did not answer in time ({self.heartbeat_timeout:g} s): the heartbeat may '
                                   'have counted, but the ban list is not updated until one is answered')
            return False
        except AgentError as error:
            self.say(('central',), f'could not reach central: {error}')
            return False
        if code == 200 and reply is None:
            self.say(('bad reply',), 'central answered the heartbeat (HTTP 200) with a reply that is too large or not '
                                     f'JSON ({MAX_HEARTBEAT_REPLY >> 20} MB at most): the ban list is not updated')
            return False
        reply = reply or {}
        if code != 200 or reply.get('ok') is not True:
            what = reply.get('code') if isinstance(reply.get('code'), str) else f'HTTP {code}'
            error = reply.get('error').strip().rstrip('.') if isinstance(reply.get('error'), str) else ''
            # (A hint of its own only where central's reason doesn't already say it: its bad_token reasons end
            # "Make a new one on the Host page.")
            hint = {'bad_token': ' Make a new token on the Host page.',
                    'delisted': ' Central has taken this world off its list.',
                    'banned': ' The host account is banned.'}.get(what, '')
            if error and (what != 'bad_token' or 'Host page' in error):
                hint = ''
            self.say(('refused', what), f'central refused the heartbeat ({what}){": " + error if error else ""}.{hint}')
            if what in ('bad_token', 'delisted', 'banned') and (self.dir / 'host.json').exists():
                self.write_host(None)           # (A revoked token, a delisted world or a banned host: no host powers.)
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
        if reply.get('kind') in ('community', 'official'):
            self.kind = reply['kind']
        try:
            self.write_bans(reply.get('bans'))
            if self.bans_failing:
                self.bans_failing = False
                self.log('world agent: bans.json is written again')
        except OSError as error:
            if not self.bans_failing:
                self.bans_failing = True
                self.log(f'world agent: could not write bans.json: {error} (the world keeps its last ban list)')
        self.write_host(reply.get('host'))
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
        if self.plain:
            allowed, why = plain_http_allowed(self.central)
            if allowed is not True:
                self.log(f'world agent: no leave sent: {why}, and the host token would go in clear over http://')
                return
        try:
            code, _ = post_json(self.central + '/worlds/leave', self.token, {}, timeout=5.0)
            self.log('world agent: left central\'s list' if code == 200 else f'world agent: the leave was refused (HTTP {code})')
        except AgentError as error:
            self.log(f'world agent: could not tell central the world is leaving: {error}')

    def unexpected(self, what: str, error: BaseException) -> None:
        """An error nothing above expected: said once (without the token), and the loop carries on."""
        line = f'{what}: {type(error).__name__}: {error}'.replace(self.token, '<host token>')
        if line != self.trouble:
            self.trouble = line
            self.log(f'world agent: unexpected trouble in {line} (carrying on)')

    def run(self) -> int:
        self.log(f'world agent: heartbeats every {self.every:g} s for the world on port {self.world_port}, to '
                 f'{self.central} (agent directory {self.dir})')
        self.upnp.open()
        next_beat = 0.0
        while not self.stop.is_set():
            now = time.monotonic()
            self.upnp.step()
            try:
                self.keys.step(now)
            except Exception as error:          # (KeyKeeper.step says its own; this is a last resort.)
                self.keys.next_due = now + 60
                self.unexpected('the keys', error)
            if not self.halted and now >= next_beat:
                next_beat = now + self.every
                try:
                    self.heartbeat()
                except Exception as error:
                    self.unexpected('the heartbeat', error)
            self.stop.wait(min(0.5, self.every))
        if self.accepted and not self.halted:
            self.leave()
        self.upnp.close()
        return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = parser.add_subparsers(dest='command', required=True)
    run = sub.add_parser('run', help='the heartbeat, the ban list and the keys, for one world')
    run.add_argument('--central', required=True, help="central's address, http(s)://...")
    run.add_argument('--dir', required=True, type=Path,
                     help='the agent directory (the server runs with --central-keys DIR/central-keys.json)')
    run.add_argument('--world-port', required=True, type=int, help='the world server\'s port on this machine')
    run.add_argument('--central-dir', type=Path,
                     help='a directory only this user writes, for central.json (the world\'s kind and host)')
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
    for command in (run, keys):
        command.add_argument('--upnp-port', type=int,
                             help='try UPnP once: ask the router to forward this TCP port here (tools/world_upnp.py)')
        command.add_argument('--upnp-name', default='', help='the world\'s name, in the mapping\'s description')
    args = parser.parse_args(argv)
    if args.upnp_port is not None and not 0 < args.upnp_port < 65536:
        parser.error('--upnp-port is 1-65535')
    if not re.match(r'^https?://', args.central):
        parser.error('--central is http(s)://...')
    log = lambda line: print(line, flush=True)   # noqa: E731
    if args.command == 'run':
        allowed, why = plain_http_allowed(args.central)
        if allowed is None:                     # (Not fatal: each heartbeat looks again, and sends nothing till then.)
            log(f'world agent: warning: {why}: no heartbeat until it can be, and is this computer or a private network')
        elif not allowed:
            parser.error(f'--central: {why}, and the host token would go in clear: http:// is only for a central on '
                         'this computer or a private network; give its https:// address')
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
                      tags=tags, host=args.host, secure=args.secure, every=args.every, log=log,
                      upnp_port=args.upnp_port, upnp_name=args.upnp_name, central_dir=args.central_dir)
        for sig in (signal.SIGTERM, signal.SIGINT):
            signal.signal(sig, lambda *_: agent.stop.set())
        return agent.run()
    if args.command == 'keys':
        if plain_http_allowed(args.central)[0] is False:
            log(f'world agent: warning: {args.central} is http:// to the internet: keys fetched in clear could be '
                'swapped on the way; use https://')
        return keys_loop(args.central, args.out, max(60.0, args.every) if not args.once else args.every, args.once, log=log,
                         upnp=Upnp(args.upnp_port, args.upnp_name, log))
    return 2


if __name__ == '__main__':
    sys.exit(main())
