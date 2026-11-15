#!/usr/bin/env python3
"""A world's fingerprints (Docs/Design/77-central-accounts-and-hosted-worlds.md, 7.1: approved worlds): SHA-256 of what
a host runs that decides progression, computed from the files on disk. The world agent sends them with each heartbeat;
the release (Package/stage.sh --build) computes the same ones inside the built world image and signs them as the
release's manifest, which central imports. Central approves a community world while its fingerprints match a release's.

  server        the world server program (ratw_server)
  agent         the world agent (tools/world_agent.py), which reports all of this
  fingerprints  this file, which computes it
  rules         the progression rules, one fingerprint over RULES_FILES under the Data directory (what earns social
                standing, stars and the Gift tiers, and the tiers' thresholds)

Atlas and the Dungeon Master never change these: the map, NPCs, talk and the rest live in the world's database, and
Data/ is read-only in the image (doc 78, 11). The two progression switches a host may set (cheats, --open-tiers) are
not files: the server reports them in its /status, and central checks them there.

Standard library only, as the agent is.

  python3 tools/world_fingerprints.py show [--server PATH] [--data DIR]
        this computer's fingerprints, as the agent would send them
  python3 tools/world_fingerprints.py manifest --build ID [--server PATH] [--data DIR]
        the release manifest (JSON on stdout), for Package/stage.sh to sign
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
VERSION = 1
# The progression rules, relative to the Data directory: social standing's curve, social reasons, daily cap and the
# Gift tiers' thresholds; the creator's tier costs and limits; stars and scene pay; the Gift families and who may have
# them. (Practice and fighting, Data/Progression's other files, grow a wolf, not an account's standing, so they are
# left out; they are read-only in the image all the same.)
RULES_FILES = ('Progression/standing.json', 'Progression/creation.json', 'Social/social.json', 'Gifts/families.json')
IMAGE_SERVER = Path('/opt/ratw/bin/ratw_server')
_cache: dict[str, tuple[tuple, str | None]] = {}


def default_server() -> Path | None:
    """The server program: RATW_SERVER_BINARY, else the world image's, else this checkout's build (development)."""
    given = os.environ.get('RATW_SERVER_BINARY')
    if given:
        return Path(given)
    for candidate in (IMAGE_SERVER, ROOT / 'build-core' / 'ratw_server', ROOT / 'build' / 'ratw_server'):
        if candidate.is_file():
            return candidate
    return None


def default_data() -> Path:
    return Path(os.environ.get('RATW_DATA_DIR') or ROOT / 'Data')


def file_sha256(path: Path | None) -> str | None:
    """The file's SHA-256 (hex), or None if it can't be read. Kept while the file's size and time don't change, so a
    heartbeat a minute doesn't read the server program again."""
    if path is None:
        return None
    try:
        info = os.stat(path)
    except OSError:
        return None
    stamp = (info.st_size, info.st_mtime_ns, info.st_ino)
    known = _cache.get(str(path))
    if known and known[0] == stamp:
        return known[1]
    digest = hashlib.sha256()
    try:
        with open(path, 'rb') as f:
            for block in iter(lambda: f.read(1 << 20), b''):
                digest.update(block)
    except OSError:
        return None
    _cache[str(path)] = (stamp, digest.hexdigest())
    return digest.hexdigest()


def rules_files(data: Path) -> dict[str, str | None]:
    return {name: file_sha256(data / name) for name in RULES_FILES}


def rules_fingerprint(files: dict[str, str | None]) -> str | None:
    """One fingerprint over the rules files: SHA-256 of their names and hashes, in order. None if any is missing."""
    if any(v is None for v in files.values()):
        return None
    text = json.dumps({'v': VERSION, 'files': dict(sorted(files.items()))}, separators=(',', ':'), sort_keys=True)
    return hashlib.sha256(text.encode('utf-8')).hexdigest()


def fingerprints(server: Path | None = None, data: Path | None = None, agent: Path | None = None) -> dict:
    """What the agent sends: {v, server, agent, fingerprints, rules}, each a SHA-256 or None (not found)."""
    server = server if server is not None else default_server()
    data = data if data is not None else default_data()
    return {'v': VERSION, 'server': file_sha256(server), 'agent': file_sha256(agent or HERE / 'world_agent.py'),
            'fingerprints': file_sha256(Path(__file__).resolve()),
            'rules': rules_fingerprint(rules_files(data))}


def quickened_thresholds(data: Path) -> dict:
    """The release's own Quickened thresholds (standing.json's unlocks), which central checks its evidence against."""
    try:
        unlocks = json.loads((data / 'Progression' / 'standing.json').read_text(encoding='utf-8'))['unlocks']
        q = unlocks['quickened']
        return {'stars': int(q['stars']), 'starGivers': int(q['starGivers'])}
    except (OSError, ValueError, KeyError, TypeError):
        return {}


def manifest(build: str, server: Path | None = None, data: Path | None = None) -> dict:
    data = data if data is not None else default_data()
    found = fingerprints(server, data)
    missing = [k for k in ('server', 'agent', 'fingerprints', 'rules') if not found[k]]
    if missing:
        raise SystemExit(f'world_fingerprints: cannot read {", ".join(missing)}: no manifest')
    return {'v': VERSION, 'build': build, 'made': int(time.time()), **{k: found[k] for k in
            ('server', 'agent', 'fingerprints', 'rules')}, 'rulesFiles': rules_files(data),
            'quickened': quickened_thresholds(data)}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = parser.add_subparsers(dest='command', required=True)
    for name in ('show', 'manifest'):
        command = sub.add_parser(name)
        command.add_argument('--server', type=Path, help='the server program (default: the image\'s, or this build)')
        command.add_argument('--data', type=Path, help='the Data directory (default: RATW_DATA_DIR, or Data/)')
        if name == 'manifest':
            command.add_argument('--build', required=True, help='the build id (stage.sh\'s)')
    args = parser.parse_args(argv)
    if args.command == 'show':
        print(json.dumps(fingerprints(args.server, args.data), indent=2))
        return 0
    print(json.dumps(manifest(args.build, args.server, args.data), separators=(',', ':'), sort_keys=True))
    return 0


if __name__ == '__main__':
    sys.exit(main())
