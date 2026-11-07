#!/usr/bin/env python3
"""The world agent (Docs/Design/77-central-accounts-and-hosted-worlds.md, 5.1): what a world needs from central over
HTTPS, which the C++ server can't do itself (it has no HTTPS client). For now (Phase 2) that is central's ticket keys;
the heartbeat and the ban list join it in Phase 4. Standard library only, so it runs in the world's container as is.

  python3 tools/world_agent.py keys --central URL --out FILE [--once] [--every SECONDS]

Fetches GET URL/keys and writes it to FILE whole and atomically (a temporary file, then a rename), for the server's
--central-keys FILE. It fetches at start, every SECONDS (a day by default: central publishes its next key a day before
it signs with it), and whenever the server asks: a ticket naming a key the world doesn't hold makes the server write
FILE.wanted, and the agent fetches again (at most once a minute) and removes it. The file also records central's clock
against this machine's (`offset`, seconds; GET URL/health), which the server logs. If central can't be reached, or
answers with no usable key, the file is left as it was: a world keeps its last keys (doc 77, 12).

http:// is for a central on this computer (development); anything else should be https://, whose certificate is
checked as usual.
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
import time
import urllib.error
import urllib.request

MAX_REPLY = 64 * 1024
KID = re.compile(r'^[A-Za-z0-9_.-]{1,64}$')
X = re.compile(r'^[A-Za-z0-9_-]{43}$')       # 32 bytes in unpadded base64url.
WANTED_EVERY = 60


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


def keys_loop(central: str, out: Path, every: float, once: bool, log=print) -> int:
    stopping = []
    for sig in (signal.SIGTERM, signal.SIGINT):
        signal.signal(sig, lambda *_: stopping.append(True))
    wanted = out.with_name(out.name + '.wanted')
    last_try = -1e9
    next_due = 0.0
    while not stopping:
        now = time.monotonic()
        asked = wanted.exists()
        if now >= next_due or (asked and now - last_try >= WANTED_EVERY):
            last_try = now
            try:
                log(f'world agent: {fetch_keys(central, out)}' + (' (the server asked)' if asked else ''))
                wanted.unlink(missing_ok=True)
                next_due = now + every
            except (AgentError, OSError) as error:
                log(f'world agent: could not fetch central\'s keys: {error}')
                if once:
                    return 1
                next_due = now + min(every, 60)
            if once:
                return 0
        time.sleep(1)
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = parser.add_subparsers(dest='command', required=True)
    keys = sub.add_parser('keys', help="keep central's ticket keys in a file for the server")
    keys.add_argument('--central', required=True, help="central's address, http(s)://...")
    keys.add_argument('--out', required=True, type=Path, help='the file (the server\'s --central-keys)')
    keys.add_argument('--every', type=float, default=86400, help='seconds between fetches (default a day)')
    keys.add_argument('--once', action='store_true', help='fetch once and stop')
    args = parser.parse_args(argv)
    if not re.match(r'^https?://', args.central):
        parser.error('--central is http(s)://...')
    if args.command == 'keys':
        return keys_loop(args.central, args.out, max(60.0, args.every) if not args.once else args.every, args.once,
                         log=lambda line: print(line, flush=True))
    return 2


if __name__ == '__main__':
    sys.exit(main())
