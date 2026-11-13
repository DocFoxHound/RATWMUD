#!/usr/bin/env python3
"""The world container's entrypoint (Docs/Design/77, section 8; Docs/Design/78, sections 3, 6 and 12; Package/README.md).
Standard library only (with the image's psycopg through tools/world_db.py). Run by tini as PID 1's child, as root, and
only to read what is private and start the children; each child runs as a user of its own. One container holds the
whole world:

1. Reads the host's settings, /etc/ratw/host/world.toml (Package/world.toml; their folder is mounted at
   /etc/ratw/host, inside /etc/ratw, which only root may open), the host token and AI key files it names, and the
   database passwords the db service made on its first start (/run/ratw-secrets/passwords.env). Root reads them
   whatever their modes, so a host may keep them 0600.
2. The Editor (doc 78), first, so it answers while the world sets up: the world's own TLS key and certificate, made
   once in the `admin` volume (the helper's own, root and ratw-admin only), their fingerprint in the log and in
   manage/editor-certificate.txt; then the helper, tools/world_admin.py, as ratw-admin (uid 10003, in no other
   group), with only the dm password and its own copies of what it may know: the registry, the AI config, a cleaned
   copy of world.toml (settings.json: no token, no key), central's word on the world (central.json, and
   `ever-official`, never cleared), and the set-up progress (progress.json); and the editor password (slice 2). [editor] enabled = false: no helper, so
   nothing answers on the Editor port. Never on a world central has ever named official (doc 78, 6.7).
3. Waits for the database, then imports the world package (/package/world) into it: `tools/world_package.py import
   /package/world --key world --name NAME`. On the first start that makes the world; on every later one it applies
   the migrations and keeps the world as it is, even when this image carries another package (an update keeps the
   world's own state; `docker compose down -v` starts afresh). Only a real failure stops the start.
   RATW_PACKAGE_IMPORT=skip instead only applies the migrations (an image built without a world export).
   Before the migrations of an update (this image's package or migrations differ from what the world last ran, which
   is recorded in its database as admin.settings' package.ran), the backup service makes a backup first (doc 78, 12.1).
   Then the first Editor account (the owner), when the world has none: its name, one-time password and Editor code
   go to manage/first-editor.txt, never to the log. A start 7 days later with it still unclaimed renews them.
4. With an AI key, writes the NPC Mind's config (tools/npc_bridge.py's format, mode 0600) and starts the Mind on
   127.0.0.1; without one, NPCs speak their authored lines. The Mind's call ledger is kept in logs/mind/, where only
   its user writes (the server's user can write logs/ itself, which is sticky), and its monthly count copied into the
   admin volume for the Editor's AI budget.
5. Ticket mode, always: a world signs players in through central (a world with no central, a LAN world, isn't offered
   yet: doc 77, Q9). The world agent runs with the host token (RATW_HOST_TOKEN, or world.toml's token_file, read
   here and given in its environment, never an argument), keeping central's keys, the heartbeat and the ban list;
   without a token, only central's keys are kept (`world_agent.py keys`), so players can join by address but the
   world isn't listed and has no host. The agent's lines go to the container's log as well as logs/world-agent.log.
6. Runs ratw_server, forwarding SIGTERM and SIGINT to it, and keeps every helper up whatever the server does (doc 78,
   12.4). world.toml is read again before each start (a broken one keeps the server stopped, saying why, until it is
   put right: tried again after 5 s, 30 s, 2 minutes, then every 2 minutes, apart from the crash backoff; a broken
   one at the container's start does the same, with the Editor's helper up from its last good copy); the Mind and the agent are started again when their settings changed, and the Editor's helper when
   [editor] did. Exit 75 (a new release, nobody on) starts it again at once; 76 (closed for maintenance) waits,
   checking the world's state every 5 seconds, until the Editor or the DM opens it; any other code starts it again
   after 5 s, 30 s, 2 minutes, then every 10 minutes (back to 5 s once it has run for 10 minutes). The server's state
   goes to the admin volume (server.json: up, why not, the last exit, the next try) for the Editor's status pill.
   A new map that won't start falls back by itself (doc 78, 12.4; M8): a server that stops twice within two minutes of
   starting on a build it has never run well on has the previous map published again (tools/world_publish.py fallback,
   as the Editor's helper's user and the editor role) and starts on that; fallback.json tells the Editor. And an update
   whose server can't load the map's build format (12.5) checks memory before migrating, rebuilds the map itself when
   nothing is unpublished, and otherwise waits for the Editor's answer (rebuild.json), the helper up meanwhile.
7. Acts on the helper's requests (files in the admin volume's requests/ that name only a kind, never a path or a
   command: doc 78, 6.5 and I7): `restart` (the server stops, saving, and starts again, world.toml read first),
   `backup-by-hand`, `backup-before-publish` and a pin on one backup (`pin-STAMP`, `unpin-STAMP`: passed on to the
   backup service through the `requests` volume),
   `share` ("Share my world", doc 78, 10.5: tools/world_package.py export, players stripped out, as ratw into the
   container's own state, then packed by root into one file in manage/shared/, the folder owner's, the newest 3 kept;
   one at a time, 3 an hour), `first-editor-used` (manage/first-editor.txt emptied: the owner chose a password). A Mind or agent or Editor helper that stops on its own is started again (at most once a minute).
8. UPnP (world.toml's [network] upnp, default true) isn't tried here: this container is on Docker's own network, from
   which the router's search can't be reached. compose.yaml's upnp service tries it from the host's network
   (tools/world_upnp.py hold); this only checks the setting and says where to look.

Each child is given only the secrets it needs, in its environment: the import the superuser's and the owner's
passwords; the Mind the game role's password and the AI key; the agent the host token; the Editor's helper the dm
role's password. The Mind and the agent run as ratw, so they are kept from the passwords file and the host's folder
(root's alone), but not from one another. The server, which faces the internet, is the one kept apart: it runs as
ratw-server with only its database connection (as ratw_game), and can't read the passwords file, the host's folder
(under /etc/ratw), the Mind's config, the admin volume or the other processes' environments. The Editor's helper,
which faces the internet too once the host opens its port, runs as ratw-admin: it can't read the server's or the
Mind's files, and nothing it writes is followed by root (every file root writes is made new and renamed into place;
links and odd files where root expects its own are moved aside). Started as another user than root (a test),
everything runs as that user.
"""
from __future__ import annotations

import collections
import contextlib
import errno
import io
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import signal
import stat
import subprocess
import sys
import threading
import time
import tomllib

APP = Path(os.environ.get('RATW_APP_DIR', '/opt/ratw'))
TOOLS = APP / 'tools'
SERVER = APP / 'bin' / 'ratw_server'
DATA = Path(os.environ.get('RATW_DATA_DIR', APP / 'Data'))
PACKAGE = Path(os.environ.get('RATW_PACKAGE_DIR', '/package'))
SETTINGS = Path(os.environ.get('RATW_WORLD_TOML', '/etc/ratw/host/world.toml'))   # (The host's folder, read-only.)
SECRETS = Path(os.environ.get('RATW_SECRETS_FILE', '/run/ratw-secrets/passwords.env'))
STATE = Path(os.environ.get('RATW_STATE_DIR', '/var/lib/ratw'))
LOGS = STATE / 'logs'
# The Editor's helper's own directory: the `admin` volume (compose.yaml), root and ratw-admin only (doc 78, 6.4-6.5).
ADMIN = Path(os.environ.get('RATW_ADMIN_DIR', STATE / 'admin'))
# The host's world folder's manage/ (compose.yaml: ./manage), where only root inside the container writes.
MANAGE = Path(os.environ.get('RATW_MANAGE_DIR', '/var/ratw/manage'))
# The `requests` volume, which the backup service mounts too (doc 78, 12.1).
BACKUP_REQUESTS = Path(os.environ.get('RATW_BACKUP_REQUESTS', STATE / 'backup-requests'))
KEY = 'world'                                   # The one world's key in this container's registry.
MIND_PORT = 18766
EDITOR_PORT = 17789                             # The helper's port inside the container (compose publishes it).
RELEASE_EXIT, MAINTENANCE_EXIT = 75, 76         # (Core/RatwGame.cpp and RatwGame.h; tools/live.sh loops on them.)
BACKOFF = (5, 30, 120, 600)                     # Seconds before starting a server that stopped on its own (doc 78, 12.4).
STABLE = 600                                    # A server that ran this long starts the backoff afresh.
TOML_RETRY = (5, 30, 120)                       # A broken world.toml read again after these, then every 2 minutes
                                                # (its own count: it never adds to the crash backoff).
SERVER_REASONS = ('starting', 'running', 'restarting', 'exit', 'world.toml', 'closed')   # (server.json's "reason".)
MAINTENANCE_POLL = float(os.environ.get('RATW_MAINTENANCE_POLL', '5'))
STARTED_WELL = 120                              # A server up this long has started on its map (doc 78, 12.4's fallback).
MEMORY_POLL = 30                                # An update's rebuild short of memory looks again this often (12.5).
STOP_WAIT = 80                                  # A restart's SIGTERM, then this long to save (compose gives 90 s).
WORLD_ID = re.compile(r'^world-[0-9a-f]{32}$')
USERNAME = re.compile(r'^[a-z][a-z0-9_-]{2,31}$')     # (The DM tool's rule, 0013_dungeon_master.sql.)
NAME_BYTES = 48                                 # (Core/RatwGameTickets.cpp: the server counts a name's UTF-8 bytes.)
PASSWORDS = ('POSTGRES_PASSWORD', 'RATW_OWNER_PASSWORD', 'RATW_EDITOR_PASSWORD', 'RATW_PUBLISHER_PASSWORD',
             'RATW_GAME_PASSWORD', 'RATW_DM_PASSWORD')
SECRET_ENV = PASSWORDS + ('RATW_HOST_TOKEN', 'RATW_AI_KEY', 'RATW_DATABASE_URL')
DEFAULT_MODELS = {'model': 'gpt-5.4-mini', 'light_model': 'gpt-5.4-nano', 'fallback_model': 'gpt-5.4-mini',
                  'story_model': 'gpt-5.6-luna'}   # (Doc 28: the main voice, the light one, the fallback, stories.)
# The image's users (Package/Dockerfile): ratw 10001 (the import, the Mind, the agent), ratw-server 10002 (the
# server), ratw-admin 10003 (the Editor's helper), each in its own group alone.
HELPER, SERVER_USER, EDITOR_USER = 'ratw', 'ratw-server', 'ratw-admin'
# world.toml's cheats (Docs/Design/78, 8), each but "off" listed "cheats on": the server's flag for each. "host": the
# host's tools may give Gifts and tiers and reward the host's own wolves; "everyone": --dev-tools, every player's
# socket cheats (Gifts and goods from nothing) and the Dev Console.
CHEATS = {'off': [], 'host': ['--host-cheats'], 'everyone': ['--dev-tools']}
# What the helper may ask of root (tools/world_admin.py: REQUEST_KINDS), as `KIND.<12 hex>` files in requests/.
REQUEST = re.compile(r'^(restart|backup-before-publish|backup-by-hand|share|first-editor-used|(?:pin|unpin)-\d{8}T\d{6}Z)'
                     r'\.[0-9a-f]{12}$')
BACKUP_NAME = re.compile(r'^ratw-\d{8}T\d{6}Z-(daily|by-hand|before-publish|before-update|before-restore)$')
REQUESTS_A_TICK = 64
LINE_MOST = 4096                                # A child's output is read in lines of at most this many characters.
# What a child's line may not carry into the container's log: control characters (a terminal's escapes, line and page
# breaks) and the invisible and direction marks (doc 78's R1 review, security 10: a forged "Editor certificate:" line).
LOG_UNSAFE = re.compile('[\x00-\x08\x0a-\x1f\x7f-\x9f\u00ad\u061c\u180e\u200b-\u200f\u2028-\u202e\u2060-\u2069\ufeff]')
EXIT_LINES, EXITS_KEPT = 20, 10                 # exits.json: the server's last exits, each with its last lines.
BACKUPS_WAITING = 8                             # Backup requests left for the backup service, at most.
UPDATE_BACKUP_WAIT, UPDATE_BACKUP_UNCLAIMED = 1800, 120   # An update's backup: waited for this long; given up on if the
                                                # backup service shows no sign of life (an old compose.yaml) for this long.
# "Share my world" (doc 78, 10.5; M9): the world exported (players stripped out, as tools/world_package.py does) and
# packed into one file in manage/shared/: <world>-<date>.ratwpkg. One at a time, at most SHARES_AN_HOUR started an hour
# (the hosted studio refuses before root does), the newest SHARES_KEPT kept. The export needs the room to write the
# package twice over (the work directory in the container's state, then the file) and SHARE_MIN_FREE more on each.
SHARES_KEPT, SHARES_AN_HOUR = 3, 3
SHARE_MIN_FREE = int(os.environ.get('RATW_SHARE_MIN_FREE_MB', '1024')) * 1024 * 1024
SHARE_GUESS = 256 * 1024 * 1024                 # The package's size before the first share has measured it.
SHARE_TIMEOUT = 1800                            # An export still running after this long is stopped (said, failed).
SHARED_FILE = re.compile(r'^[a-z0-9][a-z0-9-]{0,47}-\d{4}-\d{2}-\d{2}(?:-\d{1,2})?\.ratwpkg$')
FIRST_EDITOR_USED = ('Used. Lost your password? Double-click reset-editor-password in this folder\n'
                     '(reset-editor-password.bat on Windows, reset-editor-password.sh elsewhere), or type:\n\n'
                     '    docker compose exec world ratw-editor reset NAME\n\n'
                     '(NAME is your Editor account; docker compose exec world ratw-editor list shows them.)\n')

stopping = False
server: subprocess.Popen | None = None
forwarded = False                               # The stop signal reached the server (on_signal, or the loop after it).


def say(text: str) -> None:
    print(f'RATW_PACKAGE {text}', flush=True)


def child_lines(stream, most: int = LINE_MOST):
    """A child's output, a line (of at most `most` characters: a longer one comes in pieces) at a time: root never holds
    an endless line (R1 review)."""
    while True:
        line = stream.readline(most)
        if not line:
            return
        yield line


def child_say(prefix: str, line: str) -> None:
    """A child's line in the container's log, under its own prefix (RATW_SERVER, RATW_AGENT: never root's
    RATW_PACKAGE), its control characters and invisible marks dropped, so it can't pass for root's lines (the
    "Editor certificate:" line among them) or end one and start another (R1 review, security 10)."""
    text = LOG_UNSAFE.sub('', line.rstrip('\n'))
    if text.strip():
        print(f'{prefix} {text}', flush=True)


def fail(text: str, code: int = 2) -> None:
    print(f'RATW_PACKAGE error: {text}', file=sys.stderr, flush=True)
    sys.exit(code)


def as_root() -> bool:
    return os.geteuid() == 0


def unreadable_hint() -> str:
    if as_root():          # (Root reads any file; under rootless Docker, only one of the user running Docker.)
        return 'Make it a file of the user that runs Docker.'
    return (f'This container runs as uid {os.geteuid()}, not root as compose.yaml starts it: the file must be '
            'readable by that uid (chmod 644, or give it that group with chmod 640).')


def tools_module(name: str):
    """A module of tools/ (world_agent for its token and http:// checks, world_db for the database)."""
    if str(TOOLS) not in sys.path:
        sys.path.insert(0, str(TOOLS))
    return __import__(name)


def agent_module():
    return tools_module('world_agent')


# --------------------------------------------------------------------------------------------------- settings

def text(table: dict, name: str, default: str = '', most: int = 4096, where: str = '') -> str:
    value = table.get(name, default)
    if not isinstance(value, str):
        fail(f'world.toml: {where}{name} must be text ("...").')
    value = value.strip()
    if len(value) > most:
        fail(f'world.toml: {where}{name} is {len(value)} characters; at most {most}.')
    return value


def number(table: dict, name: str, default: int | None, low: int, high: int) -> int | None:
    value = table.get(name, default)
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, int) or not low <= value <= high:
        fail(f'world.toml: {name} must be a whole number from {low} to {high}.')
    return value


def flag(table: dict, name: str, default: bool = False, where: str = '') -> bool:
    value = table.get(name, default)
    if not isinstance(value, bool):
        fail(f'world.toml: {where}{name} must be true or false.')
    return value


def host_path(name: str) -> Path:
    """A file world.toml names: absolute, or beside world.toml."""
    return Path(name) if name.startswith('/') else SETTINGS.parent / name


def read_host_file(path: Path, what: str) -> str:
    """A file of the host's, as text (utf-8, a byte-order mark allowed: Notepad and PowerShell 5 write one)."""
    try:
        return path.read_text(encoding='utf-8-sig')
    except FileNotFoundError:
        fail(f'{what}: there is no {path}.')
    except PermissionError:
        fail(f'{what}: {path} can\'t be read (permission denied). {unreadable_hint()}')
    except OSError as error:
        fail(f'{what}: {path} can\'t be read ({error.strerror}).')
    except UnicodeDecodeError:
        fail(f'{what}: {path} is not UTF-8 text.')


def read_settings() -> dict:
    if not SETTINGS.is_file():
        fail(f'no {SETTINGS}: copy world.toml beside compose.yaml (see README.md).')
    try:
        raw = tomllib.loads(read_host_file(SETTINGS, 'world.toml'))
    except tomllib.TOMLDecodeError as error:
        fail(f'world.toml is not valid TOML: {error}')
    tables = ('world', 'central', 'ai', 'server', 'network', 'editor')
    world, central, ai, server_, network, editor = (raw.get(t, {}) for t in tables)
    if not all(isinstance(t, dict) for t in (world, central, ai, server_, network, editor)):
        fail('world.toml: [world], [central], [ai], [server], [network] and [editor] are tables.')
    tags = world.get('tags', [])
    if not isinstance(tags, list) or not all(isinstance(t, str) for t in tags):
        fail('world.toml: tags is a list of words, ["a", "b"].')
    s = {
        'name': text(world, 'name', most=NAME_BYTES),
        'description': text(world, 'description', most=300),
        'region': text(world, 'region', most=32),
        'tags': [t.strip() for t in tags if t.strip()],
        'player_limit': number(world, 'player_limit', 40, 1, 10000),
        'arrivals_per_hour': number(world, 'arrivals_per_hour', None, 1, 100000),
        'private': flag(world, 'private'),
        'cheats': text(world, 'cheats', 'off', 16),
        'world_id': text(world, 'id', most=64),
        'central': text(central, 'url', most=512).rstrip('/'),
        'token_file': text(central, 'token_file', most=1024),
        'host': text(central, 'host', most=253),
        'secure': flag(central, 'secure'),
        'ai_key': text(ai, 'key', most=4096) or os.environ.get('RATW_AI_KEY', '').strip(),
        'ai_key_file': text(ai, 'key_file', most=1024),
        'models': {k: text(ai, k, v, 128) for k, v in DEFAULT_MODELS.items()},
        'cost_mode': text(ai, 'cost_mode', 'balanced', 16),
        # The Editor's AI budget a month, over its tools and the NPC Mind together (doc 78, 7; the hosted studio's
        # default when left out).
        'monthly_tokens': number(ai, 'monthly_tokens', None, 0, 10 ** 12),
        'workers': number(server_, 'workers', None, 0, 16),
        'upnp': flag(network, 'upnp', True),
        # The Editor (doc 78): on unless turned off; the first owner's name if the host picks one; new devices.
        'editor': {'enabled': flag(editor, 'enabled', True, '[editor] '),
                   'owner': text(editor, 'owner', '', 32, '[editor] '),
                   'new_devices': text(editor, 'new_devices', 'open', 16, '[editor] ')},
    }
    if not s['name']:
        fail('world.toml: give the world a name ([world] name = "...").')
    if len(s['name'].encode('utf-8')) > NAME_BYTES:
        fail(f'world.toml: the name is {len(s["name"].encode("utf-8"))} bytes; at most {NAME_BYTES} (an accented '
             'letter takes two or more): make it shorter.')
    if any(ord(c) < 32 or ord(c) == 127 for c in s['name']):
        fail('world.toml: the name has a control character in it.')
    if len(s['tags']) > 5 or any(len(t) > 16 for t in s['tags']):
        fail('world.toml: up to 5 tags, 16 characters each at most.')
    if s['cheats'] not in CHEATS:
        fail('world.toml: cheats is "off", "host" or "everyone".')
    if s['editor']['owner'] and not USERNAME.match(s['editor']['owner']):
        fail('world.toml: [editor] owner is 3 to 32 small letters, digits, - and _, starting with a letter (or leave it '
             'out, and the world makes one up). Never a password.')
    if s['editor']['new_devices'] not in ('open', 'approve'):
        fail('world.toml: [editor] new_devices is "open" or "approve".')
    if s['private']:
        say('warning: private worlds (a password kept on central) aren\'t built yet; the world is listed as public.')
    if not s['central']:
        fail('world.toml: [central] url is needed: a world signs its players in through central. A world without '
             'one (a private or LAN world) isn\'t offered yet.')
    if not re.match(r'^https?://[^/\s]+$', s['central']):
        fail('world.toml: central\'s url is https://name (no path).')
    if not WORLD_ID.match(s['world_id']):
        fail('world.toml: a world on central needs its id ([world] id = "world-..."), shown on the Host page '
             'beside the host token.')
    if s['secure'] and not s['host']:
        fail('world.toml: secure = true needs host = "a.dns.name": the certificate in front of the world is for a '
             'name.')
    if s['ai_key_file'] and not s['ai_key']:
        s['ai_key'] = read_secret_file(s['ai_key_file'], 'the AI key')
    if any(ord(c) <= 32 or ord(c) >= 127 for c in s['ai_key']):
        fail('world.toml: the AI key has a space or an unusual character in it.')
    if s['cost_mode'] not in ('generous', 'balanced', 'frugal'):
        fail('world.toml: cost_mode is generous, balanced or frugal.')
    s['token'] = read_host_token(s)
    if s['token'] and s['central'].startswith('http://'):
        allowed, why = agent_module().plain_http_allowed(s['central'])
        if allowed is False:
            fail(f'world.toml: central\'s url is http:// but {why}: the host token would go in clear. http:// is only '
                 'for a central on this computer or a private network; give its https:// address.')
        if allowed is None:
            say(f'warning: {why}: the world agent sends no heartbeat until it can tell that central is on this '
                'computer or a private network')
    return s


# What the last broken world.toml said, and when (monotonic), so a mistake left for hours isn't said every try.
TOML_SAID = {'text': None, 'at': float('-inf'), 'fresh': False}


def reread_settings(editor_up: bool = True) -> dict | None:
    """world.toml again, before a server start (doc 78, 12.4): None, having said why, if it is broken now. The same
    mistake is said again at most once a minute (TOML_SAID['fresh'] when it was said this time)."""
    said = io.StringIO()
    try:
        with contextlib.redirect_stderr(said):
            s = read_settings()
        TOML_SAID.update(text=None, fresh=False)
        return s
    except SystemExit:
        text, now = said.getvalue(), time.monotonic()
        fresh = text != TOML_SAID['text'] or now - TOML_SAID['at'] >= 60
        TOML_SAID.update(text=text, fresh=fresh, **({'at': now} if fresh else {}))
        if fresh:
            sys.stderr.write(text)
            sys.stderr.flush()
            say('the world waits until world.toml is put right (the line above says what is wrong)' +
                ('; the Editor stays up' if editor_up else ''))
        return None


class TomlRetry:
    """A broken world.toml's own schedule (R1): read again after 5 s, 30 s, 2 minutes, then every 2 minutes, with the
    next try in server.json and in the log (when the mistake was said)."""

    def __init__(self):
        self.tries = 0

    def failed(self, code: int | None = None, failures: int = 0) -> float:
        delay = TOML_RETRY[min(self.tries, len(TOML_RETRY) - 1)]
        self.tries += 1
        server_state(False, 'world.toml', code, failures, delay)
        if TOML_SAID['fresh']:
            say(f'world.toml still has a mistake; next try at '
                f'{time.strftime("%H:%M:%S", time.gmtime(time.time() + delay))} UTC')
        return delay


def read_secret_file(name: str, what: str) -> str:
    return read_host_file(host_path(name), f'world.toml names a file for {what}').strip()


def read_host_token(s: dict) -> str:
    """The host token: world.toml's token_file, or RATW_HOST_TOKEN. '' if there is none; stops at one that can't be
    read, or that isn't a token (the agent would refuse it, out of sight, once a minute)."""
    if s['token_file']:
        token, where = read_secret_file(s['token_file'], 'the host token'), str(host_path(s['token_file']))
    else:
        token, where = os.environ.get('RATW_HOST_TOKEN', '').strip(), 'RATW_HOST_TOKEN'
    if s['token_file'] and not token:
        fail(f'{where} is empty: put the host token from the Host page in it.')
    if token and not agent_module().TOKEN.match(token):
        fail(f'{where}: the host token has spaces or odd characters in it: copy it again from the Host page.')
    return token


def load_passwords() -> None:
    """The passwords the db service generated on its first start (compose.yaml), unless the environment has them."""
    try:
        lines = SECRETS.read_text(encoding='utf-8').splitlines()
    except OSError as error:
        if all(os.environ.get(k) for k in PASSWORDS):
            return
        fail(f'the database passwords can\'t be read from {SECRETS} ({error.strerror}); is the db service running?')
    for line in lines:
        name, _, value = line.partition('=')
        if name in PASSWORDS and value and not os.environ.get(name):
            os.environ[name] = value.strip()
    missing = [k for k in PASSWORDS if not os.environ.get(k)]
    if missing:
        fail(f'{SECRETS} has no {", ".join(missing)}.')


def default_workers() -> int:
    """The game's worker threads: the CPUs this container may use (its cgroup's quota, which the server's own default
    doesn't see), less two, 0-16."""
    cpus = len(os.sched_getaffinity(0))
    try:
        quota, period = Path('/sys/fs/cgroup/cpu.max').read_text().split()
        if quota != 'max':
            cpus = min(cpus, max(1, int(int(quota) / int(period))))
    except (OSError, ValueError):
        pass
    return max(0, min(16, cpus - 2))


def editor_port() -> int:
    """The Editor port as the host's computer sees it (compose.yaml's RATW_EDITOR_PORT): for the files and the log."""
    value = os.environ.get('RATW_EDITOR_PORT', '').strip() or str(EDITOR_PORT)
    if not value.isdigit() or not 1 <= int(value) <= 65535:
        fail('RATW_EDITOR_PORT is a port number (in .env beside compose.yaml).')
    return int(value)


def package_version() -> str:
    try:
        version = json.loads((PACKAGE / 'world' / 'package.json').read_text(encoding='utf-8')).get('version')
        return str(version)[:40] if version else ''
    except (OSError, ValueError, AttributeError):
        return ''


# ------------------------------------------------------------------------------------------- users and files

def child_env(*keep: str, **extra: str) -> dict:
    """The environment without any secret but those named."""
    env = {k: v for k, v in os.environ.items() if k not in SECRET_ENV or k in keep}
    env['HOME'] = str(STATE)                     # (Not root's: libpq looks there for a .pgpass.)
    env.update(extra)
    return env


def run_as(user: str) -> dict:
    """Popen's arguments that start a child as `user` (its own group, no others), when this runs as root."""
    return {'user': user, 'group': user, 'extra_groups': []} if as_root() else {}


def ids(user: str | None, group: str | None = None) -> tuple[int, int] | None:
    """A user's (and group's) ids, for fchown; None when this doesn't run as root (nothing is chowned then)."""
    if not as_root() or user is None:
        return None
    import grp
    import pwd
    return pwd.getpwnam(user).pw_uid, grp.getgrnam(group or user).gr_gid


def _chown(fd: int, owner: tuple[int, int] | None, best_effort: bool, what: Path) -> None:
    if owner is None:
        return
    try:
        os.fchown(fd, *owner)
    except OSError as error:
        if not best_effort:
            raise
        say(f'note: {what.name}\'s owner couldn\'t be set ({error.strerror}); left as it is')


def write_file(path: Path, body: str, user: str | None = HELPER, mode: int = 0o600, group: str | None = None,
               owner: tuple[int, int] | None = None, best_effort: bool = False) -> None:
    """A file written whole: a new one (never through a link or a file someone else left) renamed into place. Owned by
    `user` (or the ids in `owner`), with `mode`. best_effort: a chown or chmod the folder refuses (a Windows folder
    under Docker Desktop) is said and doesn't stop anything."""
    temp = path.with_name(f'.{path.name}.{secrets.token_hex(6)}')
    fd = os.open(temp, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, mode)
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as out:
            _chown(out.fileno(), owner if owner is not None else ids(user, group), best_effort, path)
            try:
                os.fchmod(out.fileno(), mode)
            except OSError:
                if not best_effort:
                    raise
            out.write(body)
        try:
            os.replace(temp, path)
        except OSError as error:
            # A directory (or anything else rename can't replace) left where the file goes: moved aside, never
            # followed or deleted, and the file put in its place (R1 review: the helper could stop root in a loop so).
            if error.errno not in (errno.EISDIR, errno.ENOTEMPTY, errno.EEXIST, errno.ENOTDIR):
                raise
            move_aside(path, 'not a plain file')
            os.replace(temp, path)
    except BaseException:
        temp.unlink(missing_ok=True)
        raise


def move_aside(path: Path, why: str) -> None:
    """Something root expects to be its own (or a child's) that isn't: renamed, never followed or deleted."""
    aside = path.with_name(f'{path.name}.moved-{int(time.time())}-{secrets.token_hex(3)}')
    os.rename(path, aside)
    say(f'warning: {path} was {why}: moved aside to {aside.name}')


def private_dir(path: Path, user: str | None = HELPER, group: str | None = None, mode: int = 0o700) -> Path:
    """A directory of `user`'s (and `group`'s too, with mode 0o770: both write there; what each writes is 0644). One
    that is a link, or not a directory, is moved aside first; the directory itself is never followed."""
    try:
        found = os.lstat(path)
        if not stat.S_ISDIR(found.st_mode):
            move_aside(path, 'not a directory (a link?)')
    except FileNotFoundError:
        pass
    path.mkdir(mode=mode, parents=True, exist_ok=True)
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        _chown(fd, ids(user, group), False, path)
        os.fchmod(fd, mode)
    finally:
        os.close(fd)
    return path


def _append_fd(path: Path, mode: int) -> int | None:
    """A plain file (one link, never followed) opened to append to, in a folder a child can write: a link, a FIFO or
    anything else found there, or raced in between the look and the open, is moved aside and the open tried once more
    (R1 review: a FIFO hung root's loop, a link stopped the entrypoint). Opened without blocking, checked, then made
    blocking. None, said, if it still isn't one."""
    for last in (False, True):
        try:
            if not stat.S_ISREG(os.lstat(path).st_mode):
                move_aside(path, 'not a plain file (a link?)')
        except FileNotFoundError:
            pass
        try:
            fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_APPEND | os.O_NOFOLLOW | os.O_NONBLOCK, mode)
        except OSError as error:
            if error.errno not in (errno.ELOOP, errno.ENXIO, errno.EISDIR, errno.EEXIST, errno.EWOULDBLOCK):
                raise
            fd = None
        if fd is not None:
            info = os.fstat(fd)
            if stat.S_ISREG(info.st_mode) and info.st_nlink == 1:
                os.set_blocking(fd, True)
                return fd
            os.close(fd)
        with contextlib.suppress(FileNotFoundError):
            if not last:
                move_aside(path, 'not a plain file (a link?)')
    say(f'warning: {path} is still not a plain file: left alone, nothing written to it')
    return None


def own_file(path: Path, user: str, mode: int) -> None:
    """A file a child appends to, made or kept as `user`'s: one that is a link or not a plain file is moved aside."""
    fd = _append_fd(path, mode)
    if fd is None:
        return
    try:
        _chown(fd, ids(user), False, path)
        os.fchmod(fd, mode)
    finally:
        os.close(fd)


def append_log(path: Path):
    """A log file root appends a child's lines to (in logs/, which the server's user can write: never followed)."""
    fd = _append_fd(path, 0o640)
    if fd is None:
        return open(os.devnull, 'a', encoding='utf-8')
    return os.fdopen(fd, 'a', encoding='utf-8')


def read_small(path: Path, most: int = 64 * 1024) -> str | None:
    """A small file a child wrote, read without following a link: None if missing, odd or too big."""
    try:
        fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    except OSError:
        return None
    try:
        info = os.fstat(fd)
        if not stat.S_ISREG(info.st_mode) or info.st_size > most:
            return None
        return os.read(fd, most).decode('utf-8', 'replace')
    except OSError:
        return None
    finally:
        os.close(fd)


def mind_dir() -> Path:
    return LOGS / 'mind'


def ledger_path() -> Path:
    return mind_dir() / 'npc-mind-calls.jsonl'


def prepare_state() -> None:
    """The container's own state, each part its user's: the logs (a volume) shared by the helpers and the server, sticky
    so neither can rename or remove the other's files; the server's voice log its own; the Mind's call ledger in a
    directory of the Mind's user alone (doc 78, 7: the server's user could otherwise rewrite the AI budget's count)."""
    private_dir(STATE, None, mode=0o755)
    private_dir(LOGS, HELPER, SERVER_USER, 0o1770)
    own_file(LOGS / 'npc-voices.jsonl', SERVER_USER, 0o640)
    private_dir(mind_dir(), HELPER)


def prepare_admin() -> None:
    """The Editor's helper's directory (the `admin` volume) and its requests/; the backup service's requests volume
    (root's). Old compose files have neither volume: said, since a key in the container's own layer changes with every
    update (every client would then warn)."""
    private_dir(ADMIN, EDITOR_USER)
    private_dir(ADMIN / 'requests', EDITOR_USER)
    private_dir(BACKUP_REQUESTS, None)
    if as_root() and not os.path.ismount(ADMIN):
        say('warning: this compose.yaml has no admin volume: the Editor\'s certificate and accounts\' devices are lost '
            'whenever the container is made again (an update), and every client then warns. Take the new compose.yaml '
            'from the package (keep your .env and world.toml), then: docker compose up -d')


def manage_ready() -> bool:
    """manage/ is the host's folder (compose.yaml's ./manage mount), not the container's own directory."""
    return MANAGE.is_dir() and (os.path.ismount(MANAGE) or not as_root())


def manage_owner() -> tuple[int, int] | None:
    """What files root writes into manage/ are given: the folder's own owner, so the host can open them (rootful Docker
    on Linux would otherwise leave them root's)."""
    if not as_root():
        return None
    info = os.stat(MANAGE)
    return info.st_uid, info.st_gid


def write_manage(name: str, body: str, mode: int = 0o600) -> bool:
    """A file in the host's manage/ folder, the folder's owner's (0600 unless said). Errors are said, never stop the
    start."""
    if not manage_ready():
        return False
    try:
        if as_root():
            os.chmod(MANAGE, 0o700)        # (Only root inside, and the host outside; a Windows folder ignores it.)
    except OSError:
        pass
    try:
        write_file(MANAGE / name, body, mode=mode, owner=manage_owner(), best_effort=True)
        return True
    except OSError as error:
        say(f'warning: manage/{name} couldn\'t be written ({error.strerror})')
        return False


# ---------------------------------------------------------------------------------------------- the database

# The world's kind in the registry (doc 78, 10.2): a dev world served as live (the server keeps --world-kind live, so
# its dev-only /store console stays off), Atlas's world too; 'live' again after the import for a world started from a
# package without the map's source (slice 1's), which has nothing to edit (world_kind_after_import).
WORLD_KIND = 'dev'


def registry_text(name: str) -> tuple[str, str]:
    database = os.environ.get('RATW_WORLD_DATABASE', 'ratw_world')
    entry = {'key': KEY, 'name': name, 'database': database, 'kind': WORLD_KIND}
    local = {'live': KEY, **({'atlas': KEY} if WORLD_KIND == 'dev' else {})}
    return '{"worlds": [\n  ' + json.dumps(entry) + '\n]}\n', json.dumps(local) + '\n'


def world_kind_after_import(name: str) -> str:
    """The kind the world keeps: dev when its database holds the map's source (cells or interiors), else live, and
    the registry and the helper's copy written again when that changed."""
    global WORLD_KIND
    world_db = tools_module('world_db')
    try:
        with world_db.connect(KEY, 'game') as conn:
            source = conn.execute('SELECT EXISTS (SELECT 1 FROM world.cells) OR EXISTS (SELECT 1 FROM world.interiors)'
                                  ).fetchone()[0]
    except Exception as error:  # noqa: BLE001 - keep the kind as it is; the server's start says what is wrong.
        say(f'note: the world\'s map source couldn\'t be checked ({type(error).__name__}); it stays {WORLD_KIND}')
        return WORLD_KIND
    kind = 'dev' if source else 'live'
    if kind != WORLD_KIND:
        WORLD_KIND = kind
        write_registry(name)
        say('this world was started from a package without its map\'s source: it stays a live world (map editing '
            'needs a world started from a newer package)')
    return kind


def write_registry(name: str) -> None:
    """This container's one-world registry (tools/world_db.py: RATW_WORLDS_FILE), written afresh before every start;
    the Editor's helper gets its own copy in the admin volume."""
    worlds, local = registry_text(name)
    config = private_dir(STATE / 'config')
    write_file(config / 'worlds.json', worlds)
    write_file(config / 'worlds.local.json', local)
    write_file(ADMIN / 'worlds.json', worlds, None, 0o644)
    write_file(ADMIN / 'worlds.local.json', local, None, 0o644)


def wait_for_database(seconds: float = 180) -> None:
    world_db = tools_module('world_db')
    deadline, said = time.monotonic() + seconds, False
    while True:
        try:
            with world_db.superuser() as su:
                su.execute('SELECT 1')
            return
        except world_db.DatabaseError as error:
            # (M6's run: after a refused `up -d` (a port already allocated), Docker can start this container again with
            # no network at all, and the name db never resolves. Only a fresh container mends it.)
            lost = ' This part of the world isn\'t on its network: type docker compose down, then docker compose up -d ' \
                   '(your world is kept).' if 'resolve host' in str(error) else ''
            if time.monotonic() > deadline or stopping:
                fail(f'the database never answered: {error}.{lost}')
            if not said:
                say(f'waiting for the database ({error}).{lost}')
                said = True
            time.sleep(2)


def world_state() -> str:
    """The world's admin.server_state ('running', 'maintenance', ...), or 'unknown' if the database doesn't answer."""
    world_db = tools_module('world_db')
    try:
        with world_db.connect(KEY, 'game') as conn:
            row = conn.execute('SELECT state FROM admin.server_state LIMIT 1').fetchone()
            return str(row[0]) if row else 'running'
    except Exception:
        return 'unknown'


def tool(args: list[str], *keep: str) -> int:
    return subprocess.run([sys.executable, *args], cwd=STATE, env=child_env(*keep), **run_as(HELPER)).returncode


def import_world(name: str) -> None:
    """The world package into the database: made on the first start; on every other, the migrations and the world kept
    as it is (tools/world_package.py import). A non-zero exit is a real failure (a database holding a world no package
    made, an unreadable package, no database): it stops the start."""
    started = time.monotonic()
    if os.environ.get('RATW_PACKAGE_IMPORT', '').lower() == 'skip':
        say('RATW_PACKAGE_IMPORT=skip: the migrations only, no world imported')
        # (Not `world_db.py migrate`: it would write Database/.env, which isn't this image's.)
        code = tool(['-c', 'import sys; sys.path.insert(0, sys.argv[1]); import world_db\n'
                     'with world_db.connect("world", "owner") as conn: print("applied", len(world_db.migrate(conn)), '
                     '"migrations")', str(TOOLS)], 'RATW_OWNER_PASSWORD')
    else:
        if not (PACKAGE / 'world' / 'package.json').is_file():
            fail(f'this image holds no world package ({PACKAGE / "world"} has no package.json).')
        code = tool([str(TOOLS / 'world_package.py'), 'import', str(PACKAGE / 'world'), '--key', KEY, '--name', name],
                    *PASSWORDS)
    if code != 0:
        fail(f'the world package could not be imported (exit {code}); see above.', code if code > 0 else 2)
    say(f'the world database is ready ({time.monotonic() - started:.1f} s)')


# ---------------------------------------------------------------------------------------- the Mind and the agent

class Helper:
    """A background child started again (at most once a minute) if it stops on its own."""

    def __init__(self, name: str, start):
        self.name, self.start, self.process, self.started = name, start, None, 0.0

    def run(self) -> None:
        self.started = time.monotonic()
        self.process = self.start()

    def check(self) -> None:
        if self.process is not None and self.process.poll() is not None and not stopping:
            if time.monotonic() - self.started >= 60:
                say(f'the {self.name} stopped (exit {self.process.returncode}); starting it again')
                self.run()

    def stop(self, wait: float = 15) -> None:
        if self.process is None or self.process.poll() is not None:
            return
        self.process.send_signal(signal.SIGTERM)
        try:
            self.process.wait(wait)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        if getattr(self.process, 'tee', None) is not None:
            self.process.tee.join(2)               # (Its last lines, such as the agent's leave, into the log.)


def mind_settings(s: dict) -> dict | None:
    if not s['ai_key']:
        return None
    return {'enabled': True, 'endpoint': 'https://api.openai.com/v1/chat/completions', 'api_key': s['ai_key'],
            **{k: v for k, v in s['models'].items() if v}, 'cost_mode': s['cost_mode']}


def mind_config(s: dict) -> Path | None:
    """The Mind's config (its user's, 0600), and the Editor's helper's own copy in the admin volume (RATW_AI_CONFIG,
    ratw-admin's, 0600: doc 78, 6.5); both removed without a key."""
    config = mind_settings(s)
    path, copy = private_dir(STATE / 'mind') / 'npc-ai.json', ADMIN / 'npc-ai.json'
    if config is None:
        path.unlink(missing_ok=True)
        copy.unlink(missing_ok=True)
        return None
    body = json.dumps(config, indent=2) + '\n'
    write_file(path, body)
    write_file(copy, body, EDITOR_USER)
    return path


def start_mind(config: Path) -> subprocess.Popen:
    with append_log(LOGS / 'npc-mind.log') as log:
        return subprocess.Popen([sys.executable, str(TOOLS / 'npc_mind.py'), '--config', str(config), '--database',
                                 KEY, '--port', str(MIND_PORT), '--ledger', str(ledger_path())],
                                cwd=STATE, stdout=log, stderr=subprocess.STDOUT, env=child_env('RATW_GAME_PASSWORD'),
                                **run_as(HELPER))


def mind_ready(mind: Helper, seconds: float = 10) -> bool:
    log = LOGS / 'npc-mind.log'
    start = log.stat().st_size if log.exists() else 0
    mind.run()
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        try:
            with log.open('rb') as f:
                f.seek(start)
                if b'"event": "ready"' in f.read():
                    return True
        except OSError:
            pass
        if mind.process.poll() is not None:
            break
        time.sleep(0.2)
    mind.stop()
    return False


def tee(stream, log_path: Path, prefix: str = 'RATW_AGENT') -> None:
    """A child's lines into its log file and the container's log (`docker compose logs world`), under its own prefix."""
    with append_log(log_path) as log:
        for line in child_lines(stream):
            log.write(line if line.endswith('\n') else line + '\n')
            log.flush()
            child_say(prefix, line)


def agent_starter(s: dict, port: int, agent_dir: Path, central_dir: Path | None = None):
    log_path = LOGS / 'world-agent.log'
    env = {'RATW_HOST_TOKEN': s['token']} if s['token'] else {}
    if not s['token']:
        say('warning: no host token (RATW_HOST_TOKEN, or token_file in world.toml): the world keeps central\'s keys, '
            'so players can join it by address, but it isn\'t on the server list, nobody has the host\'s commands '
            '(/kick, /ban, /silence, /pending...), and central\'s ban list doesn\'t reach it.')
        for stale in ('host.json', 'bans.json'):   # (A token's last host and bans: none without one.)
            (agent_dir / stale).unlink(missing_ok=True)
        if central_dir is not None:
            (central_dir / 'central.json').unlink(missing_ok=True)
        command = ['keys', '--central', s['central'], '--out', str(agent_dir / 'central-keys.json')]
    else:
        command = ['run', '--central', s['central'], '--dir', str(agent_dir), '--world-port', str(port)]
        if central_dir is not None:
            command += ['--central-dir', str(central_dir)]
        for name in ('description', 'region', 'host'):
            if s[name]:
                command += [f'--{name}', s[name]]
        if s['tags']:
            command += ['--tags', ','.join(s['tags'])]
        if s['secure']:
            command.append('--secure')

    def start() -> subprocess.Popen:
        process = subprocess.Popen([sys.executable, str(TOOLS / 'world_agent.py'), *command], cwd=STATE,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding='utf-8',
                                   errors='replace', env=child_env(**env), **run_as(HELPER))
        process.tee = threading.Thread(target=tee, args=(process.stdout, log_path), daemon=True)
        process.tee.start()
        return process
    return start


def agent_settings(s: dict) -> tuple:
    """What the agent is started with: when any of it changes, the agent is started again."""
    return tuple(s[k] if not isinstance(s[k], list) else tuple(s[k])
                 for k in ('token', 'central', 'description', 'region', 'host', 'tags', 'secure'))


# --------------------------------------------------------------------------------------------- the Editor

def admin_settings(s: dict, port: int) -> dict:
    """settings.json, the helper's copy of world.toml (doc 78, 6.5): no host token, no AI key, no file names."""
    return {'name': s['name'], 'world_id': s['world_id'], 'description': s['description'], 'region': s['region'],
            'tags': s['tags'], 'player_limit': s['player_limit'], 'arrivals_per_hour': s['arrivals_per_hour'],
            'cheats': s['cheats'], 'central': s['central'], 'host': s['host'], 'secure': s['secure'],
            'has_token': bool(s['token']), 'workers': s['workers'], 'upnp': s['upnp'],
            'ai': {'key': bool(s['ai_key']), 'models': s['models'], 'cost_mode': s['cost_mode'],
                   **({'monthly_tokens': s['monthly_tokens']} if s.get('monthly_tokens') is not None else {})},
            'editor': dict(s['editor']), 'editor_port': port, 'package': package_version(), 'folder': host_folder()}


def host_folder() -> str:
    """The world folder's path on the host's computer, shown to the owner (doc 78, 12.1: so they can check it isn't in a
    synced folder): compose.yaml's RATW_HOST_FOLDER (the shell's PWD), else the folder's bind mount as the kernel
    lists it (/proc/self/mountinfo; Docker Desktop's /run/desktop/mnt/host/c/... read as C:\\...). '' if unknown."""
    folder = os.environ.get('RATW_HOST_FOLDER', '').strip()
    if not folder:
        try:
            for line in Path('/proc/self/mountinfo').read_text(encoding='utf-8', errors='replace').splitlines():
                fields = line.split(' ')
                if len(fields) > 4 and fields[4] == str(SETTINGS.parent):
                    folder = fields[3].replace('\\040', ' ')
                    break
        except OSError:
            folder = ''
        found = re.match(r'^/(?:run/desktop/mnt/host|host_mnt|mnt/host)/([a-z])(/.*)?$', folder)
        if found:
            folder = found.group(1).upper() + ':' + (found.group(2) or '/').replace('/', '\\')
    if len(folder) > 400 or any(ord(c) < 32 or ord(c) == 127 for c in folder):
        return ''
    return folder


def write_admin_settings(s: dict, port: int) -> None:
    write_file(ADMIN / 'settings.json', json.dumps(admin_settings(s, port), indent=2) + '\n', None, 0o644)


def progress(code: str, percent: int = 0) -> None:
    """progress.json: how far the world is set up, for the helper's /editor/hello (doc 78, 6.5)."""
    try:
        write_file(ADMIN / 'progress.json', json.dumps({'code': code, 'percent': percent}) + '\n', None, 0o644)
    except OSError as error:
        say(f'warning: the set-up progress couldn\'t be written ({error.strerror})')


def server_state(up: bool, reason: str, code: int | None = None, failures: int = 0, retry_in: float | None = None) -> None:
    """server.json: the game server's state, for the helper's /editor/hello and the hosted studio (doc 78, section 2's
    pill; R1, H2). reason: starting, running, restarting, exit (a crash: its code, the crashes in a row and when it is
    tried again), world.toml (broken: when it is read again) or closed (for maintenance)."""
    now = time.time()
    body = {'up': bool(up), 'reason': reason if reason in SERVER_REASONS else 'starting', 'code': code,
            'failures': int(failures), 'retryAt': round(now + retry_in, 3) if retry_in is not None else None,
            'at': round(now, 3)}
    try:
        write_file(ADMIN / 'server.json', json.dumps(body) + '\n', None, 0o644)
    except OSError as error:
        say(f'warning: the server\'s state couldn\'t be written ({error.strerror})')


def central_word() -> dict:
    """Central's word on this world, as the agent wrote it (STATE/central, where only ratw writes: doc 78, 6.7)."""
    raw = read_small(STATE / 'central' / 'central.json', 4096)
    try:
        value = json.loads(raw) if raw else {}
    except ValueError:
        return {}
    if not isinstance(value, dict) or value.get('kind') not in ('community', 'official'):
        return {}
    host = value.get('host')
    at = value.get('at')
    return {'kind': value['kind'], 'host': host if isinstance(host, str) and len(host) <= 128 else None,
            'at': at if isinstance(at, (int, float, str)) and not isinstance(at, bool) else None}


def root_mark() -> Path:
    """Root's own `ever-official`, where no child can write (the `requests` volume, root's alone; a dot name, which the
    backup service passes over): the helper owns the admin volume, so it could remove that copy (R1 review)."""
    return BACKUP_REQUESTS / '.ever-official'


def ever_official() -> bool:
    return os.path.lexists(root_mark()) or os.path.lexists(ADMIN / 'ever-official')


def mirror_central(known: dict | None = None) -> dict:
    """Central's word copied into the admin volume (central.json), and `ever-official` written once central has named
    the world official: never cleared, whatever a later central.json says. Returns what was copied."""
    word = central_word()
    try:
        if word != known:
            if word:
                write_file(ADMIN / 'central.json', json.dumps(word) + '\n', None, 0o644)
            elif os.path.lexists(ADMIN / 'central.json'):
                if stat.S_ISDIR(os.lstat(ADMIN / 'central.json').st_mode):
                    move_aside(ADMIN / 'central.json', 'a directory')
                else:
                    (ADMIN / 'central.json').unlink()
        when = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()) + '\n'
        if word.get('kind') == 'official' and not ever_official():
            say('central names this world official: the Editor is off for good on it (Docs/Design/78, 6.7)')
        if word.get('kind') == 'official' or ever_official():
            for mark in (root_mark(), ADMIN / 'ever-official'):     # (Each made again if it went.)
                if not os.path.lexists(mark):
                    write_file(mark, when, None, 0o644 if mark.parent == ADMIN else 0o600)
    except OSError as error:
        say(f'warning: central\'s word couldn\'t be copied for the Editor ({error.strerror})')
    return word


def official() -> bool:
    """Central has ever named this world official. (The shared-progress credential that would also say so (doc 77,
    section 6) isn't built yet: this package holds none.)"""
    return ever_official() or central_word().get('kind') == 'official'


def admin_env(port: int) -> dict:
    """The Editor's helper's environment: the dm and editor passwords and its own copies, nothing else secret (doc 78,
    6.5; the editor's from slice 2, 10.2: Atlas's edits and a dev world's economy go through live_edit as editor)."""
    extra = {'RATW_ADMIN_DIR': str(ADMIN), 'RATW_WORLDS_FILE': str(ADMIN / 'worlds.json'),
             'RATW_WORLDS_LOCAL': str(ADMIN / 'worlds.local.json'), 'RATW_EDITOR_PORT': str(port),
             'RATW_EDITOR_DROPPED': '1', 'HOME': str(ADMIN)}
    if (ADMIN / 'npc-ai.json').is_file():
        extra['RATW_AI_CONFIG'] = str(ADMIN / 'npc-ai.json')
    if package_version():
        extra['RATW_PACKAGE_VERSION'] = package_version()
    return child_env('RATW_DM_PASSWORD', 'RATW_EDITOR_PASSWORD', **extra)


def admin_tool(args: list[str], port: int) -> subprocess.CompletedProcess:
    """tools/world_admin.py as the helper's user (ratw-editor's own commands, never through the port)."""
    return subprocess.run([sys.executable, str(TOOLS / 'world_admin.py'), *args], cwd=ADMIN, env=admin_env(port),
                          capture_output=True, text=True, timeout=120, **run_as(EDITOR_USER))


def map_tool(command: str, port: int, timeout: float = 120) -> dict | None:
    """tools/world_publish.py as the helper's user and the editor role (doc 78, 12.4 and 12.5): `newest` (the newest
    build, and whether this package's server loads its format), `fallback` (the previous map published again), `memory`
    (whether there is room to build), `child` (validate and build). Its last JSON line, or None when it said none."""
    try:
        done = subprocess.run([sys.executable, str(TOOLS / 'world_publish.py'), command, '--world', KEY, '--by', 'update'],
                              cwd=ADMIN, env=admin_env(port), capture_output=True, text=True, timeout=timeout,
                              **run_as(EDITOR_USER))
    except (OSError, subprocess.SubprocessError):
        return None
    for line in reversed(done.stdout.splitlines()):
        try:
            value = json.loads(line)
        except ValueError:
            continue
        if isinstance(value, dict) and 'error' not in value:
            return value.get('result', value) if command == 'child' else value
        break
    return None


def good_build() -> int | None:
    """The build the server last ran well on (kept across container starts)."""
    text_ = read_small(STATE / 'good-build')
    return int(text_.strip()) if text_ and text_.strip().isdigit() else None


def write_rebuild(value: dict | None) -> None:
    """rebuild.json in the admin volume: the update rebuild's question for the Editor (doc 78, 12.5); None removes it."""
    try:
        if value is None:
            (ADMIN / 'rebuild.json').unlink(missing_ok=True)
        else:
            write_file(ADMIN / 'rebuild.json', json.dumps(value) + '\n', None, 0o644)
    except OSError as error:
        say(f'warning: the rebuild\'s state couldn\'t be written ({error.strerror})')


def rebuild_before_update(port: int, sleep=time.sleep) -> bool:
    """Before the migrations (doc 78, 12.5): whether this package's server can't load the newest build's format, so the
    map must be rebuilt. If it must and memory is short, that is said plainly (the log, and rebuild.json for the Editor)
    and looked at again every half minute, rather than migrating and then stopping in a loop. False: no rebuild (or no
    world yet)."""
    state = map_tool('newest', port)
    if not state or state.get('build') is None or state.get('readable'):
        return False
    say(f'this update\'s server can\'t load the map as it was built (build {state["build"]}, format {state.get("format")}): '
        'the map has to be built again')
    said = False
    while not stopping:
        memory = map_tool('memory', port) or {'ok': True}
        if memory.get('ok'):
            return True
        if not said:
            gb = lambda n: '?' if n is None else f'{n / (1 << 30):.1f} GB'
            say(f'the map can\'t be rebuilt yet: building needs about {gb(memory.get("need"))} of memory free, and this '
                f'world has {gb(memory.get("free"))}. Give Docker more memory (the hosting guide: "Memory to Publish"); '
                'the world waits, and nothing has been changed.')
            said = True
        write_rebuild({'needed': True, 'memory': memory, 'unpublished': state.get('unpublished')})
        progress('stopped', 0)
        sleep(MEMORY_POLL)
    return True


def rebuild_after_update(port: int) -> bool:
    """After the migrations: the rebuild itself when nothing is unpublished; with unpublished changes it is the host's to
    answer (they would go live), so rebuild.json asks through the Editor. True: the server waits for a build it loads."""
    state = map_tool('newest', port) or {}
    if state.get('readable'):
        write_rebuild(None)
        return False
    if state.get('unpublished'):
        write_rebuild({'needed': True, 'unpublished': state['unpublished']})
        say(f'this update needs the map rebuilt, and {state["unpublished"]} map change(s) are not published: they would go '
            'live. The Editor asks (the map editor\'s Publish page): rebuild with them, or discard them first. The world '
            'starts once it is rebuilt.')
        return True
    say('rebuilding the map for this update (nothing is unpublished)')
    progress('building', 60)
    result = map_tool('child', port, timeout=900) or {}
    if result.get('ok'):
        say(f'the map is rebuilt (build {result.get("build")})')
        write_rebuild(None)
        return False
    why = '; '.join(e.get('text', '') for e in (result.get('errors') or [])[:3] if isinstance(e, dict)) or 'it did not finish'
    write_rebuild({'needed': True, 'unpublished': 0, 'failed': why[:400]})
    say(f'the map couldn\'t be rebuilt ({why[:400]}). The Editor\'s map editor shows what to put right; the world starts '
        'once it is rebuilt.')
    return True


def certificate(s: dict, port: int) -> str | None:
    """The world's Editor key and certificate, made once (doc 78, 6.4); its fingerprint in the log at every start and
    in manage/editor-certificate.txt. None if it couldn't be made."""
    made = admin_tool(['make-certificate', '--world-id', s['world_id']], port)
    try:
        result = json.loads(made.stdout.strip().splitlines()[-1])
    except (ValueError, IndexError):
        say(f'warning: the Editor\'s certificate couldn\'t be made (exit {made.returncode}): '
            f'{made.stderr.strip()[-300:]}')
        return None
    if result.get('made'):
        say('made the Editor\'s certificate (once: it is kept in the admin volume)')
    say(f'Editor certificate: {result["fingerprint"]}')
    if not manage_ready():
        say('note: this compose.yaml has no manage folder, so manage/editor-certificate.txt isn\'t written; '
            '`docker compose exec world ratw-editor fingerprint` shows it')
    else:
        text_ = admin_tool(['fingerprint'], port)
        if text_.returncode == 0:
            write_manage('editor-certificate.txt', text_.stdout, 0o644)     # (No secret in it.)
        else:
            say(f'warning: manage/editor-certificate.txt couldn\'t be written (exit {text_.returncode})')
    return result['fingerprint']


def first_editor_text(made: dict, port: int, now: float | None = None) -> str:
    """manage/first-editor.txt (doc 78, 3.1): the owner's name, one-time password and Editor code."""
    until = time.strftime('%Y-%m-%d %H:%M UTC', time.gmtime((time.time() if now is None else now) + made.get('days', 7) * 86400))
    address = '127.0.0.1' if port == EDITOR_PORT else f'127.0.0.1:{port}'
    lead = ('A new one-time password: the last one wasn\'t used within 7 days.\n\n' if made.get('renewed') else '')
    return (f'{lead}Your Editor account: {made["username"]}\n'
            f'One-time password: {made["password"]}\n\n'
            'Easiest: open the game, press EDITOR, and paste this Editor code:\n\n'
            f'{made["code"]}\n\n'
            f'Or use the address {address} with the name and password above, on this computer.\n'
            'You\'ll choose your own password next. If you\'ve already chosen your own password, this one no longer '
            'works.\n\n'
            f'The password and the code work until {until}. If they haven\'t been used by then, the world makes new '
            'ones the next time it starts.\n'
            'Keep this file to yourself: whoever has the code can take this account once.\n')


def first_owner(port: int) -> None:
    """The world's first Editor account, if it has none (or a renewed one-time password after 7 days unclaimed), into
    manage/first-editor.txt. The log names the file, never the password."""
    made = admin_tool(['first-owner'], port)
    try:
        result = json.loads(made.stdout.strip().splitlines()[-1]) if made.stdout.strip() else None
    except ValueError:
        result = None
    if made.returncode != 0:
        say(f'warning: the first Editor account couldn\'t be made (exit {made.returncode}): {made.stderr.strip()[-300:]}')
        return
    if not result:
        return
    if write_manage('first-editor.txt', first_editor_text(result, port)):
        say('Your first Editor account is in manage/first-editor.txt' +
            (' (a new one-time password: the last one wasn\'t used within 7 days)' if result.get('renewed') else ''))
    else:
        say(f'Your first Editor account, {result["username"]}, is ready, but this compose.yaml has no manage folder to '
            'put its password in. Take the new compose.yaml from the package, or type: docker compose exec world '
            f'ratw-editor reset {result["username"]}')


def start_editor(port: int) -> subprocess.Popen:
    """The helper (tools/world_admin.py serve) as ratw-admin, listening on the Editor port inside the container."""
    return subprocess.Popen([sys.executable, str(TOOLS / 'world_admin.py'), 'serve', '--port', str(EDITOR_PORT)],
                            cwd=ADMIN, env=admin_env(port), **run_as(EDITOR_USER))


# -------------------------------------------------------------------------------------------------- the ledger

class MindUse:
    """The Mind's calls this month, counted from its ledger (logs/mind/, the Mind's user's alone) into the admin volume
    (mind-month.json: {month, tokens, ...}, which the hosted studio reads), for the Editor's monthly AI budget (doc 78,
    7). Read on from where it stopped."""

    def __init__(self, path: Path | None = None, out: Path | None = None):
        self.path, self.out = path, out
        self.month, self.offset, self.inode, self.counts, self.written = None, 0, None, {}, None

    def _fresh(self, month: str) -> None:
        self.month, self.offset = month, 0
        self.counts = {'month': month, 'tokens': 0, 'calls': 0, 'failed': 0, 'prompt_tokens': 0,
                       'completion_tokens': 0, 'cached_tokens': 0, 'models': {}}

    def _count(self, line: bytes) -> None:
        try:
            entry = json.loads(line)
        except ValueError:
            return
        if not isinstance(entry, dict) or not isinstance(entry.get('t'), (int, float)) or isinstance(entry['t'], bool):
            return
        if time.strftime('%Y-%m', time.gmtime(entry['t'])) != self.month:
            return
        c = self.counts
        c['calls'] += 1
        if entry.get('outcome') != 'success':
            c['failed'] += 1
        model = str(entry.get('model', ''))[:64] or 'default'
        per = c['models'].setdefault(model, {'calls': 0, 'prompt_tokens': 0, 'completion_tokens': 0}) \
            if len(c['models']) < 32 or model in c['models'] else None
        if per is not None:
            per['calls'] += 1
        for k in ('prompt_tokens', 'completion_tokens', 'cached_tokens'):
            v = entry.get(k)
            if type(v) is int and 0 <= v < 10 ** 9:
                c[k] += v
                if per is not None and k in per:
                    per[k] += v
        total = entry.get('total_tokens')
        if not (type(total) is int and 0 <= total < 10 ** 9):
            total = sum(v for k in ('prompt_tokens', 'completion_tokens')
                        if type(v := entry.get(k)) is int and 0 <= v < 10 ** 9)
        c['tokens'] += total

    def update(self, now: float | None = None) -> dict:
        path, out = self.path or ledger_path(), self.out or ADMIN / 'mind-month.json'
        month = time.strftime('%Y-%m', time.gmtime(now or time.time()))
        if month != self.month:
            self._fresh(month)
        try:
            fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        except OSError:
            fd = None
        if fd is not None:
            try:
                info = os.fstat(fd)
                if stat.S_ISREG(info.st_mode):
                    if info.st_ino != self.inode or info.st_size < self.offset:
                        self._fresh(month)
                        self.inode = info.st_ino
                    os.lseek(fd, self.offset, os.SEEK_SET)
                    data = os.read(fd, 16 * 1024 * 1024)
                    end = data.rfind(b'\n') + 1
                    for line in data[:end].splitlines():
                        if len(line) <= 8192:
                            self._count(line)
                    self.offset += end
            finally:
                os.close(fd)
        if self.counts != self.written:
            write_file(out, json.dumps(self.counts) + '\n', None, 0o644)
            self.written = json.loads(json.dumps(self.counts))
        return self.counts


# ------------------------------------------------------------------------------------------------- the server

def server_command(s: dict, port: int, agent_dir: Path, dialogue: bool, workers: int) -> list[str]:
    command = [str(SERVER), '--database', KEY, '--world-kind', 'live', '--port', str(port), '--bind', '0.0.0.0',
               '--workers', str(workers), '--talk-data', str(DATA / 'Talk'), '--voice-data', str(DATA / 'Voice'),
               '--voice-log', str(LOGS / 'npc-voices.jsonl'),
               '--central', s['central'], '--world-id', s['world_id'], '--central-keys',
               str(agent_dir / 'central-keys.json'), '--world-name', s['name'],
               '--player-limit', str(s['player_limit'])]
    # (No --dm-directory: nothing in the container reads what the server would write there, and the legacy
    # Storykeeper stays unstarted: Docs/Design/78, Q18.)
    command += CHEATS[s.get('cheats', 'off')]
    if s['arrivals_per_hour']:
        command += ['--arrivals-per-hour', str(s['arrivals_per_hour'])]
    if dialogue:
        command += ['--dialogue', f'http://127.0.0.1:{MIND_PORT}/dialogue']
    return command


def upnp_note(s: dict, port: int) -> str:
    if not s['upnp']:
        return (f'UPnP is off (world.toml: [network] upnp = false): forward port {port} (TCP) on your router to this '
                'computer by hand')
    return (f'UPnP: the upnp service asks the router to forward port {port} (TCP) here; `docker compose logs upnp` '
            'says whether it could (only on Linux with Docker Engine; otherwise forward it by hand)')


def seconds_words(seconds: float) -> str:
    return f'{int(seconds)} s' if seconds < 60 else f'{int(seconds // 60)} minute{"s" if seconds >= 120 else ""}'


def pass_lines(stream, kept) -> None:
    """The server's lines into the container's log, as they come, under RATW_SERVER, the last few kept for exits.json."""
    for line in child_lines(stream):
        child_say('RATW_SERVER', line)
        if line.strip():
            kept.append(LOG_UNSAFE.sub('', line.rstrip('\n'))[:400])


def take_requests(directory: Path | None = None) -> list[str]:
    """The helper's requests: kinds, from file names alone (never their contents, never followed), each file removed.
    Anything else in requests/ is removed and ignored; the helper's half-written files (dot names) are left."""
    directory = directory or ADMIN / 'requests'
    kinds, odd = [], 0
    try:
        # (The helper's half-written files, dot names, are passed over before the tick's share is taken: R1 review.)
        entries = sorted((e for e in os.scandir(directory) if not e.name.startswith('.')), key=lambda e: e.name)[:REQUESTS_A_TICK]
    except OSError:
        return []
    for entry in entries:
        match = REQUEST.match(entry.name)
        plain = entry.is_file(follow_symlinks=False)
        try:
            os.unlink(entry.path)
        except OSError:
            continue
        if match and plain:
            if match.group(1) not in kinds:
                kinds.append(match.group(1))
        else:
            odd += 1
    if odd:
        say(f'warning: {odd} odd file(s) in the Editor\'s requests were removed and ignored')
    return kinds


def pass_backup(kind: str, directory: Path | None = None) -> bool:
    """A backup request passed on to the backup service (the `requests` volume): one waiting of each kind at most."""
    directory = directory or BACKUP_REQUESTS
    try:
        waiting = [e.name for e in os.scandir(directory) if not e.name.startswith('.')]
    except OSError as error:
        say(f'warning: a backup was asked for, but the backup requests can\'t be reached ({error.strerror})')
        return False
    if any(name.startswith(kind + '.') for name in waiting) or len(waiting) >= BACKUPS_WAITING:
        say(f'a backup ({kind}) was asked for; one is already waiting for the backup service')
        return False
    write_file(directory / f'{kind}.{secrets.token_hex(6)}', kind + '\n', None, 0o600)
    say(f'{"a pin on a backup" if kind.startswith(("pin-", "unpin-")) else f"a backup ({kind})"} was asked for by the '
        'Editor: passed to the backup service')
    return True


# --------------------------------------------------------------------------------------------- the backups (doc 78, 12.1)

def backups_view(raw: dict) -> dict | None:
    """The backup service's state (.backup-status.json in the requests volume) as the Editor may see it, checked: names
    by their pattern, kinds, whole numbers; anything else dropped. None if it isn't one."""
    if not isinstance(raw, dict) or raw.get('v') != 1:
        return None
    whole = lambda v: v if isinstance(v, int) and not isinstance(v, bool) and v >= 0 else None
    items = []
    for b in (raw.get('backups') or [])[:60]:
        if isinstance(b, dict) and isinstance(b.get('name'), str) and BACKUP_NAME.match(b['name']) and whole(b.get('at')):
            items.append({'name': b['name'], 'kind': BACKUP_NAME.match(b['name']).group(1), 'at': b['at'],
                          'size': whole(b.get('size')), 'pinnedUntil': whole(b.get('pinnedUntil')),
                          'package': re.sub(r'[^A-Za-z0-9._+-]', '', str(b.get('package') or ''))[:40],
                          'migration': re.sub(r'[^0-9]', '', str(b.get('migration') or ''))[:6]})
    problem = raw.get('problem') if isinstance(raw.get('problem'), dict) else None
    if problem is not None:
        problem = {'code': problem.get('code') if problem.get('code') in ('disk', 'failed', 'pins', 'database') else 'failed',
                   'at': whole(problem.get('at')), 'free': whole(problem.get('free'))}
    disk = raw.get('disk') if isinstance(raw.get('disk'), dict) else {}
    return {'v': 1, 'at': whole(raw.get('at')), 'backups': items, 'problem': problem,
            'working': raw.get('working') if raw.get('working') in ('daily', 'by-hand', 'before-publish', 'before-update',
                                                                      'before-restore') else None,
            'disk': {k: whole(disk.get(k)) for k in ('free', 'used', 'minFree')}}


def mirror_backups(known: str | None = None) -> str | None:
    """The backup service's state copied, checked, into the admin volume (backups.json) for the Editor's My world and
    the hello's "Last backup". Returns what was written."""
    view = backups_view(json_or_none(read_small(BACKUP_REQUESTS / '.backup-status.json', 256 * 1024)))
    text_ = json.dumps(view) + '\n' if view is not None else None
    if text_ is not None and text_ != known:
        try:
            write_file(ADMIN / 'backups.json', text_, None, 0o644)
        except OSError as error:
            say(f'warning: the backups\' state couldn\'t be copied for the Editor ({error.strerror})')
            return known
    return text_ if text_ is not None else known


def json_or_none(raw: str | None):
    try:
        return json.loads(raw) if raw else None
    except ValueError:
        return None


def latest_migration() -> str:
    """This image's newest migration (tools/world_db.py's files): '0064'."""
    try:
        return max(v for v, _, _ in tools_module('world_db').migration_files())
    except (ValueError, OSError):
        return ''


def write_world_image(s: dict) -> None:
    """What this image is, for the backup service's restore (its refusal of a dump from a newer package) and its
    manifests (the world's id): .world-image.json in the requests volume (a dot name: not a request)."""
    body = {'package': package_version(), 'migrations': latest_migration(), 'world': s['world_id']}
    try:
        write_file(BACKUP_REQUESTS / '.world-image.json', json.dumps(body) + '\n', None, 0o600)
    except OSError as error:
        say(f'warning: the world image\'s facts couldn\'t be written for the backup service ({error.strerror})')


def last_run() -> dict | None:
    """What the world's database last ran: {'version', 'migrations', 'pending'} (version '' for a world from before this
    was recorded), or None when it holds no world yet (a first start: nothing to back up)."""
    world_db = tools_module('world_db')
    try:
        with world_db.connect(KEY, 'owner') as conn:
            if not conn.execute("SELECT to_regclass('admin.settings') IS NOT NULL").fetchone()[0]:
                return None
            rows = dict(conn.execute("SELECT key, value FROM admin.settings WHERE key IN ('package', 'package.ran')").fetchall())
            if not rows:
                return None
            applied = set(world_db.applied_migrations(conn))
    except Exception as error:  # noqa: BLE001 - (psycopg's own errors too): said; the start goes on.
        say(f'warning: what the world last ran couldn\'t be read ({type(error).__name__}): no backup before migrating')
        return None
    ran = rows.get('package.ran') if isinstance(rows.get('package.ran'), dict) else {}
    pending = [v for v, _, _ in world_db.migration_files() if v not in applied]
    return {'version': str(ran.get('version') or ''), 'migrations': str(ran.get('migrations') or ''), 'pending': pending}


def record_run(s: dict) -> None:
    """admin.settings' package.ran: the package and migrations this world now runs, and its id (doc 78, 12.1: an update
    is told by it, and a restored dump brings back the one it ran)."""
    world_db = tools_module('world_db')
    value = {'version': package_version(), 'migrations': latest_migration(), 'world': s['world_id'],
             'at': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())}
    try:
        with world_db.connect(KEY, 'owner') as conn:
            conn.execute("""INSERT INTO admin.settings (key, value) VALUES ('package.ran', %s::jsonb)
                            ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value, updated_at = now()""", (json.dumps(value),))
    except Exception as error:  # noqa: BLE001 - said; the next start backs up again, which is harmless.
        say(f'warning: the package this world runs couldn\'t be recorded ({type(error).__name__})')


def ask_backup(kind: str, wait: float = UPDATE_BACKUP_WAIT, unclaimed: float = UPDATE_BACKUP_UNCLAIMED, clock=time.monotonic,
               sleep=time.sleep) -> dict | None:
    """A backup asked of the backup service and waited for: its answer ({ok, name} or {ok: false, code}), or None if it
    never answered (no backup service: an old compose.yaml; or it took longer than `wait`)."""
    tag = secrets.token_hex(6)
    request, answer = BACKUP_REQUESTS / f'{kind}.{tag}', BACKUP_REQUESTS / f'.result.{tag}'
    write_file(request, kind + '\n', None, 0o600)
    started = clock()
    while not stopping:
        found = json_or_none(read_small(answer, 4096))
        if isinstance(found, dict):
            with contextlib.suppress(OSError):
                answer.unlink()
            return found
        alive = backups_view(json_or_none(read_small(BACKUP_REQUESTS / '.backup-status.json', 256 * 1024)))
        fresh = alive is not None and alive['at'] is not None and time.time() - alive['at'] < unclaimed
        if clock() - started > wait or (clock() - started > unclaimed and os.path.lexists(request) and not fresh):
            with contextlib.suppress(OSError):
                request.unlink()
            return None
        sleep(1)
    return None


def update_backup(s: dict) -> None:
    """Before an update's migrations: a backup, when this image's package or migrations differ from what the world last
    ran (doc 78, 12.1). Refused (a nearly full disk) or not answered (no backup service): said plainly, and the world
    starts anyway (migrations change schema only; a world that won't start helps nobody)."""
    ran = last_run()
    if ran is None:
        return
    if ran['version'] == package_version() and not ran['pending']:
        return
    say(f'this package ({package_version() or "?"}, {len(ran["pending"])} new migration(s)) differs from what the world '
        f'last ran ({ran["version"] or "an older package"}): a backup first')
    progress('migrating', 10)
    answer = ask_backup('before-update')
    if answer is None:
        say('warning: no backup was made before the update: the backup service didn\'t answer (this compose.yaml may '
            'be older than it: take the new one from the package, keeping .env and world.toml)')
    elif not answer.get('ok'):
        say('warning: no backup was made before the update' + (': the disk is nearly full (see the backup service\'s '
            'log: docker compose logs backup)' if answer.get('code') == 'disk' else ' (docker compose logs backup says why)'))
    else:
        say(f'made a backup before the update: backups/{answer.get("name")}.dump')


KEPT_TABLES = ('admins', 'devices', 'invites', 'locks')


def put_back_accounts() -> None:
    """Today's Editor accounts, kept by a restore of an older backup (Package/ratw-backup.pl: its tables lacked today's
    columns and roles, so they couldn't go straight back), put back over the backup's once the migrations have brought
    it up to date (doc 78, 12.2; M6). One transaction, as the restore does; every Editor session ends. The file is the
    backup service's, in the requests volume only root and it can write; it is removed once used, or set aside as
    .kept-accounts.failed.json (said plainly) if it can't be."""
    path = BACKUP_REQUESTS / '.kept-accounts.json'
    if not os.path.lexists(path):
        return
    kept = json_or_none(read_small(path, 32 * 1024 * 1024))
    tables = kept.get('tables') if isinstance(kept, dict) else None
    admins = tables.get('admins') if isinstance(tables, dict) else None
    try:
        if not isinstance(admins, list) or not all(isinstance(tables.get(t, []), list) for t in KEPT_TABLES):
            raise ValueError('not a list of accounts')
        from psycopg import sql
        world_db = tools_module('world_db')
        with world_db.connect(KEY, 'owner') as conn, conn.transaction():
            conn.execute('DELETE FROM dm.admins')            # (Their sessions, devices, invites and locks go with them.)
            for table in KEPT_TABLES:
                rows = [r for r in tables.get(table) or [] if isinstance(r, dict)]
                if not rows:
                    continue
                columns = [c for (c,) in conn.execute(
                    """SELECT column_name FROM information_schema.columns WHERE table_schema = 'dm' AND table_name = %s
                       ORDER BY ordinal_position""", (table,)).fetchall() if all(c in r for r in rows)]
                names = sql.SQL(', ').join(map(sql.Identifier, columns))
                conn.execute(sql.SQL('INSERT INTO {t} ({c}) SELECT {c} FROM json_populate_recordset(NULL::{t}, %s::json)')
                             .format(t=sql.Identifier('dm', table), c=names), (json.dumps(rows),))
            conn.execute('DELETE FROM dm.sessions')
        path.unlink()
        say(f'put back the {len(admins)} Editor account(s) the restore kept: '
            + ', '.join(sorted(str(a.get('username', '?'))[:32] for a in admins if isinstance(a, dict))))
    except Exception as error:  # noqa: BLE001 - (psycopg's own errors too): said; the world starts with the backup's.
        say(f'warning: the Editor accounts the restore kept couldn\'t be put back ({type(error).__name__}): the world '
            'has the backup\'s accounts. Lost your password? Double-click reset-editor-password in the world folder.')
        with contextlib.suppress(OSError):
            os.replace(path, BACKUP_REQUESTS / '.kept-accounts.failed.json')


def world_slug(name: str) -> str:
    """The world's name as a file name's start: small letters, digits and dashes, at most 40 ("Ashfall Vale" is
    ashfall-vale; a name with none of them is "world")."""
    import unicodedata
    plain = unicodedata.normalize('NFKD', name).encode('ascii', 'ignore').decode('ascii').lower()
    slug = re.sub(r'[^a-z0-9]+', '-', plain).strip('-')[:40].strip('-')
    return slug or 'world'


def shared_dir() -> Path | None:
    """manage/shared/, made if missing, the manage folder owner's (0755: nothing secret goes in it). A link or a file
    there is moved aside, never followed. None when there is no manage folder (an old compose.yaml)."""
    if not manage_ready():
        return None
    path = MANAGE / 'shared'
    try:
        found = os.lstat(path)
        if not stat.S_ISDIR(found.st_mode):
            move_aside(path, 'not a folder (a link?)')
    except FileNotFoundError:
        pass
    path.mkdir(mode=0o755, exist_ok=True)
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        _chown(fd, manage_owner(), True, path)
        with contextlib.suppress(OSError):
            os.fchmod(fd, 0o755)
    finally:
        os.close(fd)
    return path


def shared_files(directory: Path | None) -> list[dict]:
    """The shared files in manage/shared/, newest first: {name, size, at}. Only plain files with a share's name."""
    found = []
    if directory is None:
        return found
    try:
        for entry in os.scandir(directory):
            if SHARED_FILE.match(entry.name) and entry.is_file(follow_symlinks=False):
                info = entry.stat(follow_symlinks=False)
                found.append({'name': entry.name, 'size': info.st_size, 'at': int(info.st_mtime)})
    except OSError:
        return []
    return sorted(found, key=lambda f: (f['at'], f['name']), reverse=True)


class Share:
    """"Share my world" (doc 78, 10.5; M9). The helper asks with a `share` request (it holds no owner password:
    world_package export connects as the database's owner); this runs tools/world_package.py export as ratw (with the
    owner's password alone) into the container's own state, where only ratw writes, then packs the package into one file
    in the host's manage/shared/, the folder owner's: what the export holds and nothing else (players, the Editor's
    accounts, dm.*, admin.*, the host token and the AI key are never in it: the strip and the scan are the export's).
    Its state goes to the admin volume (share.json) for My world: idle, working, ready (the file) or failed (disk,
    failed, folder), with the files there."""

    def __init__(self, clock=time.monotonic, wall=time.time):
        self.clock, self.wall = clock, wall
        self.process: subprocess.Popen | None = None
        self.work: Path | None = None
        self.started, self.started_wall = 0.0, 0.0
        self.recent: list[float] = []               # Wall-clock starts this hour (root's own count: the studio's too).
        self.lines: collections.deque = collections.deque(maxlen=EXIT_LINES)
        self.name = ''
        self.last_size = 0
        self.state: dict = {'v': 1, 'state': 'idle'}

    def root(self) -> Path:
        return STATE / 'share'

    def tidy(self) -> None:
        """At the container's start: an export's leftovers (a stop part way) and half-written files removed; the state
        written afresh, with the files there."""
        with contextlib.suppress(OSError):
            if os.path.lexists(self.root()):
                if os.path.isdir(self.root()) and not os.path.islink(self.root()):
                    shutil.rmtree(self.root(), ignore_errors=True)
                else:
                    os.unlink(self.root())
        directory = shared_dir() if manage_ready() else None
        if directory is not None:
            with contextlib.suppress(OSError):
                for entry in os.scandir(directory):
                    if entry.name.startswith('.') and '.partial-' in entry.name and entry.is_file(follow_symlinks=False):
                        os.unlink(entry.path)
        self.write('idle')

    def write(self, state: str, **extra) -> None:
        directory = MANAGE / 'shared' if manage_ready() else None
        self.state = {'v': 1, 'state': state, 'at': int(self.wall()), 'kept': SHARES_KEPT,
                      'recent': [int(t) for t in self.recent], 'files': shared_files(directory)[:SHARES_KEPT + 2], **extra}
        try:
            write_file(ADMIN / 'share.json', json.dumps(self.state) + '\n', None, 0o644)
        except OSError as error:
            say(f'warning: the share\'s state couldn\'t be written for the Editor ({error.strerror})')

    def failed(self, code: str, said: str, **extra) -> None:
        say(f'Share my world: {said}')
        self.write('failed', problem={'code': code, 'at': int(self.wall()), **extra}, started=int(self.started_wall))

    def ask(self, s: dict | None) -> None:
        """A `share` request: one at a time, SHARES_AN_HOUR an hour; the folder and the disks checked first."""
        now = self.wall()
        self.recent = [t for t in self.recent if now - t < 3600]
        if self.process is not None:
            say('Share my world was asked for again while one is being made: one at a time')
            return
        if len(self.recent) >= SHARES_AN_HOUR:
            say(f'Share my world was asked for more than {SHARES_AN_HOUR} times this hour: refused')
            return
        self.recent.append(now)
        self.started_wall = now
        if s is None or shared_dir() is None:
            self.failed('folder', 'there is no manage folder in the world folder (an older compose.yaml): take the new '
                        'compose.yaml from the package (keep .env and world.toml), make an empty folder called manage '
                        'beside it, then docker compose up -d')
            return
        need = max(self.last_size, SHARE_GUESS) + SHARE_MIN_FREE
        for where, what in ((MANAGE, 'the world folder'), (STATE, 'Docker\'s own storage')):
            try:
                free = shutil.disk_usage(where).free
            except OSError:
                continue
            if free < need:
                self.failed('disk', f'refused: {what}\'s disk has {free / 2**20:.0f} MB free, and the file needs about '
                            f'{need / 2**20:.0f} MB to be made (with {SHARE_MIN_FREE / 2**20:.0f} MB left free)',
                            free=int(free), need=int(need), where='folder' if where == MANAGE else 'docker')
                return
        made = time.gmtime(now)
        self.name = f'{world_slug(s.get("name") or "")}-{time.strftime("%Y-%m-%d", made)}'
        version = f'{world_slug(s.get("name") or "")}-shared-{time.strftime("%Y%m%d%H%M", made)}'
        root = private_dir(self.root(), HELPER)
        self.work = root / f'package-{secrets.token_hex(6)}'
        self.lines.clear()
        say('Share my world was asked for by the Editor: exporting the world, with every player taken out')
        self.process = subprocess.Popen(
            [sys.executable, str(TOOLS / 'world_package.py'), 'export', '--world', KEY, '--out', str(self.work),
             '--version', version], cwd=STATE, env=child_env('RATW_OWNER_PASSWORD'), stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='replace', **run_as(HELPER))
        self.process.tee = threading.Thread(target=self._lines, args=(self.process.stdout,), daemon=True)
        self.process.tee.start()
        self.started = self.clock()
        self.write('working', started=int(now))

    def _lines(self, stream) -> None:
        for line in child_lines(stream):
            child_say('RATW_SHARE', line)
            if line.strip():
                self.lines.append(LOG_UNSAFE.sub('', line.rstrip('\n'))[:400])

    def check(self) -> None:
        """Every tick: an export that ended is packed into its file (or said to have failed); one that runs too long is
        stopped."""
        if self.process is None:
            return
        code = self.process.poll()
        if code is None:
            if self.clock() - self.started > SHARE_TIMEOUT:
                say(f'Share my world: the export ran over {SHARE_TIMEOUT // 60} minutes: stopping it')
                self.process.kill()
            return
        self.process.tee.join(2)
        with contextlib.suppress(OSError):
            self.process.stdout.close()
        self.process = None
        try:
            if code != 0:
                disk = any('No space left' in line for line in self.lines)
                self.failed('disk' if disk else 'failed', f'the export failed (exit {code})'
                            + ('; the disk is full' if disk else ': its lines are above, under RATW_SHARE'))
                return
            self.finish()
        finally:
            if self.work is not None:
                shutil.rmtree(self.work, ignore_errors=True)
                self.work = None

    def finish(self) -> None:
        """The export's package packed into manage/shared/NAME.ratwpkg (a new file, renamed into place, the folder
        owner's, 0644), the oldest beyond SHARES_KEPT removed."""
        directory = shared_dir()
        if directory is None:
            self.failed('folder', 'the manage folder went away while the file was made')
            return
        world_package = tools_module('world_package')
        name, n = f'{self.name}.ratwpkg', 2
        while os.path.lexists(directory / name):
            name, n = f'{self.name}-{n}.ratwpkg', n + 1
        temp = directory / f'.{name}.partial-{secrets.token_hex(4)}'
        try:
            manifest = json_or_none(read_small(self.work / 'package.json', 256 * 1024)) or {}
            fd = os.open(temp, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o644)
            with os.fdopen(fd, 'wb') as out:
                _chown(out.fileno(), manage_owner(), True, temp)
                with contextlib.suppress(OSError):
                    os.fchmod(out.fileno(), 0o644)
                size = world_package.pack(self.work, out)
                out.flush()
                os.fsync(out.fileno())
            os.replace(temp, directory / name)
        except (OSError, world_package.PackageError) as error:
            with contextlib.suppress(OSError):
                temp.unlink()
            disk = isinstance(error, OSError) and error.errno in (errno.ENOSPC, errno.EDQUOT)
            numbers = {}
            if disk:
                with contextlib.suppress(OSError):
                    need = sum(e.stat(follow_symlinks=False).st_size for e in os.scandir(self.work))
                    numbers = {'free': int(shutil.disk_usage(directory).free), 'need': int(need), 'where': 'folder'}
            self.failed('disk' if disk else 'failed', f'the file couldn\'t be written into manage/shared ({error})', **numbers)
            return
        self.last_size = size
        for old in shared_files(directory)[SHARES_KEPT:]:
            with contextlib.suppress(OSError):
                os.unlink(directory / old['name'])
                say(f'Share my world: removed the oldest shared file, manage/shared/{old["name"]} (the newest '
                    f'{SHARES_KEPT} are kept)')
        say(f'Share my world: manage/shared/{name} is ready ({size / 1e6:.1f} MB): the world without its players')
        self.write('ready', file=name, size=size, started=int(self.started_wall),
                   contents={'snapshot': manifest.get('snapshot') is True,
                             'build': (manifest.get('build') or {}).get('id') if isinstance(manifest.get('build'), dict) else None})

    def stop(self) -> None:
        if self.process is not None:
            self.process.kill()
            with contextlib.suppress(subprocess.TimeoutExpired):
                self.process.wait(5)
            with contextlib.suppress(OSError):
                self.process.stdout.close()
            self.process = None
        if self.work is not None:
            shutil.rmtree(self.work, ignore_errors=True)
            self.work = None


class Supervisor:
    """The server and its helpers, kept up whatever the server does (doc 78, 12.4 and 6.5)."""

    def __init__(self, port: int, editor_port: int, agent_dir: Path, central_dir: Path, clock=time.monotonic):
        self.port, self.editor_port, self.agent_dir, self.central_dir = port, editor_port, agent_dir, central_dir
        self.clock = clock
        self.s: dict | None = None
        self.server: subprocess.Popen | None = None
        self.started_at = 0.0
        self.failures = 0
        self.next_start = 0.0
        self.closed = False
        self.next_poll = 0.0
        self.restart = False
        self.stop_by: float | None = None
        self.mind: Helper | None = None
        self.mind_text: str | None = None
        self.dialogue = False
        self.agent: Helper | None = None
        self.agent_key: tuple | None = None
        self.editor: Helper | None = None
        self.editor_key: dict | None = None
        self.database_ready = False
        self.central: dict | None = None
        self.mind_use = MindUse()
        self.exits: list = []
        self.next_count = 0.0
        self.code = 0
        self.toml = TomlRetry()
        self.backups: str | None = None
        self.share = Share(clock)
        # The map (doc 78, 12.4 and 12.5): the newest build when the server last started, the one it last ran well on,
        # its failed starts in a row on a build it never ran well on, and whether it waits for a rebuild.
        self.build: int | None = None
        self.good = good_build()
        self.build_failures = 0
        self.rebuild_wait = False

    # -- the helpers -------------------------------------------------------------------------------------------
    def apply_editor(self, s: dict) -> None:
        """The Editor's helper, started (or started again) when [editor] changed; never on an official world."""
        wanted = s['editor']['enabled'] and not official()
        running = self.editor is not None
        if running == wanted and s['editor'] == self.editor_key:
            return
        if running:
            say('the Editor\'s settings changed: ' + ('starting its helper again' if wanted else 'stopping its helper'))
            self.editor.stop()
            self.editor = None
        self.editor_key = dict(s['editor'])
        if not wanted:
            say('the Editor is off on an official world (Docs/Design/78, 6.7)' if s['editor']['enabled'] else
                'the Editor is off (world.toml: [editor] enabled = false): nothing answers on the Editor port')
            return
        if certificate(s, self.editor_port) is None:
            return
        self.editor = Helper('Editor\'s helper', lambda: start_editor(self.editor_port))
        self.editor.run()
        if self.database_ready:
            first_owner(self.editor_port)

    def apply(self, s: dict) -> None:
        """world.toml's settings, before a server start: the registry and the helper's copy; the Mind, the agent and the
        Editor's helper started again when theirs changed."""
        self.s = s
        write_registry(s['name'])
        write_admin_settings(s, self.editor_port)
        write_world_image(s)
        text_ = json.dumps(mind_settings(s), sort_keys=True)
        if text_ != self.mind_text:
            if self.mind is not None:
                say('the AI settings changed: starting the NPC Mind again')
                self.mind.stop()
                self.mind = None
            self.mind_text, self.dialogue = text_, False
            config = mind_config(s)
            if config:
                mind = Helper('NPC Mind', lambda: start_mind(config))
                self.dialogue = mind_ready(mind)
                say('the NPC Mind is ready (live model)' if self.dialogue else
                    'warning: the NPC Mind did not start (see logs/npc-mind.log); NPCs use their authored lines')
                self.mind = mind if self.dialogue else None
            else:
                say('no AI key: NPCs use their authored lines')
        key = agent_settings(s)
        if key != self.agent_key:
            if self.agent is not None:
                say('the world\'s central settings changed: starting the world agent again')
                self.agent.stop()
            self.agent_key = key
            self.agent = Helper('world agent', agent_starter(s, self.port, self.agent_dir, self.central_dir))
            self.agent.run()
        self.apply_editor(s)

    def helpers(self) -> list[Helper]:
        return [h for h in (self.agent, self.mind, self.editor) if h is not None]

    def tick(self) -> None:
        """Every half second: helpers up, the Editor's requests, central's word, the Mind's monthly count."""
        for h in self.helpers():
            h.check()
        for kind in take_requests():
            self.act(kind)
        self.share.check()
        self.central = mirror_central(self.central)
        self.backups = mirror_backups(self.backups)
        if self.editor is not None and official():
            say('stopping the Editor\'s helper: this world is official')
            self.editor.stop()
            self.editor = None
        now = self.clock()
        if self.server is not None and self.build != self.good and now - self.started_at >= STARTED_WELL:
            self.ran_well()
        if now >= self.next_count:
            self.next_count = now + 60
            try:
                self.mind_use.update()
            except OSError as error:
                say(f'warning: the NPC Mind\'s calls couldn\'t be counted ({error.strerror})')

    def act(self, kind: str) -> None:
        if kind == 'restart':
            say('a restart was asked for by the Editor: the world saves, stops and starts again')
            self.restart = True
            server_state(False, 'restarting', self.code)
        elif kind in ('backup-by-hand', 'backup-before-publish') or kind.startswith(('pin-', 'unpin-')):
            pass_backup(kind)
        elif kind == 'share':
            self.share.ask(self.s)
        elif kind == 'first-editor-used':
            if write_manage('first-editor.txt', FIRST_EDITOR_USED):
                say('the first Editor account has its own password now: manage/first-editor.txt is emptied')

    # -- the map ---------------------------------------------------------------------------------------------------
    def ran_well(self) -> None:
        """The server started on its build and stayed up: that build is the one to trust."""
        self.build_failures = 0
        if self.build is None or self.build == self.good:
            return
        self.good = self.build
        try:
            write_file(STATE / 'good-build', f'{self.build}\n', None, 0o644)
        except OSError as error:
            say(f'warning: the map that started couldn\'t be noted ({error.strerror})')

    def fall_back(self) -> bool:
        """A new map that didn't start twice (doc 78, 12.4): the previous map published again under a new id (as the
        helper's user and the editor role); fallback.json tells the Editor. False: there is no previous map."""
        self.build_failures = 0
        out = map_tool('fallback', self.editor_port, 300)
        if not out or not out.get('now'):
            say(f'the world\'s map (build {self.build}) didn\'t start, and there is no previous map to go back to'
                + ('' if out else ' (the database didn\'t answer)'))
            return False
        try:
            write_file(ADMIN / 'fallback.json', json.dumps({'at': round(time.time(), 3), 'failed': out.get('failed'),
                                                            'now': out['now']}) + '\n', None, 0o644)
        except OSError as error:
            say(f'warning: the fallback couldn\'t be noted for the Editor ({error.strerror})')
        say(f'the new map (build {out.get("failed")}) didn\'t start twice: the world goes back to the previous map '
            f'(published again as build {out["now"]}). Your edits are kept; the failing build stays for the bug report.')
        return True

    # -- the server --------------------------------------------------------------------------------------------
    def start_server(self) -> None:
        global server
        s = self.s
        workers = s['workers'] if s['workers'] is not None else default_workers()
        say('starting the world server')
        self.server = server = subprocess.Popen(
            server_command(s, self.port, self.agent_dir, self.dialogue, workers), cwd=STATE,
            env=child_env(RATW_DATABASE_URL=self.database_url), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, encoding='utf-8', errors='replace', **run_as(SERVER_USER))
        self.server.lines = collections.deque(maxlen=EXIT_LINES)
        self.server.tee = threading.Thread(target=pass_lines, args=(self.server.stdout, self.server.lines), daemon=True)
        self.server.tee.start()
        self.started_at = self.clock()
        self.stop_by = None
        server_state(True, 'running', self.code, self.failures)
        self.build = (map_tool('newest', self.editor_port, 30) or {}).get('build')

    def record_exit(self, process, code: int) -> None:
        """exits.json in the admin volume: the server's last exits, each with its last lines, for the owner's "the world
        keeps stopping" (doc 78, 12.4; the hosted studio scrubs addresses and tokens before showing them)."""
        tee = getattr(process, 'tee', None)
        if tee is not None:
            tee.join(2)
        lines = list(getattr(process, 'lines', ()))
        self.exits = (self.exits + [{'at': round(time.time(), 3), 'code': code, 'lines': lines}])[-EXITS_KEPT:]
        try:
            write_file(ADMIN / 'exits.json', json.dumps({'exits': self.exits}) + '\n', None, 0o644)
        except OSError as error:
            say(f'warning: the server\'s exits couldn\'t be written ({error.strerror})')

    def exited(self, code: int) -> None:
        global server
        now = self.clock()
        ran = now - self.started_at
        process = self.server
        self.server = server = None
        self.code = code
        self.stop_by = None
        if stopping:
            return
        self.record_exit(process, code)
        if self.restart:
            self.restart = False
            self.failures, self.next_start = 0, now
            server_state(False, 'restarting', code)
            return
        if code in (RELEASE_EXIT, MAINTENANCE_EXIT):
            self.ran_well()                           # (It left by itself, saved: it had started.)
        if code == RELEASE_EXIT:
            say('the server stopped for a new release; starting it again')
            self.failures, self.next_start = 0, now
            server_state(False, 'restarting', code)
            return
        if code == MAINTENANCE_EXIT:
            say(f'the world is closed for maintenance: it starts again once it is opened (the Editor, or the DM); '
                f'checking every {MAINTENANCE_POLL:g} s')
            self.closed, self.next_poll = True, now + MAINTENANCE_POLL
            server_state(False, 'closed', code)
            return
        if ran >= STABLE:
            self.failures = 0
        if self.build is not None and self.build != self.good and ran < STARTED_WELL:
            self.build_failures += 1
            if self.build_failures >= 2 and self.fall_back():
                self.failures, self.next_start = 0, now
                server_state(False, 'restarting', code)
                return
        else:
            self.build_failures = 0
        delay = BACKOFF[min(self.failures, len(BACKOFF) - 1)]
        self.failures += 1
        self.next_start = now + delay
        server_state(False, 'exit', code, self.failures, delay)
        say(f'the world server stopped (exit {code}); starting it again in {seconds_words(delay)}. The Editor stays up.')

    def due(self) -> bool:
        """Whether a stopped server should start now."""
        now = self.clock()
        if self.rebuild_wait:
            # An update's rebuild the Editor is asked about (doc 78, 12.5): the server starts once a build it loads is there.
            if now < self.next_poll:
                return False
            self.next_poll = now + MAINTENANCE_POLL
            if not (map_tool('newest', self.editor_port, 30) or {}).get('readable'):
                return False
            say('the map is rebuilt: starting the world server')
            self.rebuild_wait = False
            write_rebuild(None)
        if self.restart:
            self.restart, self.closed = False, False
            return True
        if self.closed:
            if now < self.next_poll:
                return False
            self.next_poll = now + MAINTENANCE_POLL
            if world_state() in ('running', 'host_only'):
                say('the world is open again: starting its server')
                self.closed = False
                return True
            return False
        return now >= self.next_start

    def step(self) -> bool:
        """One turn of the loop. False when it is time to leave."""
        global forwarded
        if self.server is not None and self.server.poll() is not None:
            self.exited(self.server.returncode)
        if stopping:
            if self.server is not None and not forwarded:   # (Started just as the signal came.)
                self.server.send_signal(signal.SIGTERM)
                forwarded = True
            return self.server is not None           # (Wait for it to save and stop.)
        self.tick()
        if self.server is None:
            if self.due():
                s = reread_settings()
                if s is None:
                    # world.toml's own schedule: a mistake there never adds to the crash backoff (R1).
                    self.next_start = self.clock() + self.toml.failed(self.code or None, self.failures)
                else:
                    self.toml.tries = 0
                    self.apply(s)
                    self.start_server()
        elif self.restart and self.stop_by is None:
            self.server.send_signal(signal.SIGTERM)
            self.stop_by = self.clock() + STOP_WAIT
        elif self.stop_by is not None and self.clock() > self.stop_by:
            say('the world server didn\'t stop in time: stopping it hard')
            self.server.kill()
            self.stop_by = float('inf')               # (Nothing more to do but wait for it.)
        return True

    def run(self, database_url: str) -> int:
        self.database_url = database_url
        self.next_start = self.clock()
        while self.step():
            time.sleep(0.5)
        self.share.stop()
        for h in (self.agent, self.mind, self.editor):   # (The agent first: its leave takes the world off the list.)
            if h is not None:
                h.stop()
        say(f'stopped (the server\'s last exit: {self.code})')
        return 0                                      # (Only a stop asked for ends the loop.)


def first_settings(port: int, clock=time.monotonic, sleep=time.sleep) -> dict | None:
    """world.toml at the container's start (R1, M-f). A mistake no longer ends the container (which Docker would only
    start again, saying the same): it is said, and read again on world.toml's own schedule until it is right. Meanwhile
    the Editor's helper runs from its last good copy (settings.json and the certificate in the admin volume, from an
    earlier start), so the Editor still answers, saying world.toml has a mistake. None: stopped while waiting."""
    kept = _read_admin_settings()
    can_help = bool(kept) and (ADMIN / 'editor-cert.pem').is_file() and kept.get('editor', {}).get('enabled', True) \
        is not False and not official()
    s = reread_settings(can_help)
    if s is not None:
        return s
    helper = None
    if can_help:
        say('the Editor\'s helper starts from the last good world.toml, so the Editor answers while you put it right')
        helper = Helper('Editor\'s helper', lambda: start_editor(port))
        helper.run()
    retry = TomlRetry()
    try:
        while True:
            next_try = clock() + retry.failed()
            while clock() < next_try:
                if stopping:
                    return None
                if helper is not None:
                    helper.check()
                sleep(0.5)
            s = reread_settings(can_help)
            if s is not None:
                say('world.toml is right now: the world starts')
                return s
    finally:
        if helper is not None:
            helper.stop()                             # (The start proper starts it again, with the new settings.)


def _read_admin_settings() -> dict:
    try:
        value = json.loads((ADMIN / 'settings.json').read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return {}
    return value if isinstance(value, dict) and isinstance(value.get('editor', {}), dict) else {}


def on_signal(number, _frame) -> None:
    global stopping, forwarded
    stopping = True
    if server is not None and server.poll() is None:
        server.send_signal(number)
        forwarded = True


def main() -> int:
    for number in (signal.SIGTERM, signal.SIGINT):
        signal.signal(number, on_signal)
    if not as_root():
        say('warning: not started as root: the server runs as the same user as the rest, and could read their '
            'secrets')
    load_passwords()
    os.environ.setdefault('RATW_DB_HOST', 'db')
    os.environ.setdefault('RATW_DB_PORT', '5432')
    os.environ.setdefault('POSTGRES_USER', 'postgres')
    port = int(os.environ.get('RATW_PORT', '7788'))
    if not 1 <= port <= 65535:
        fail('RATW_PORT is a port number.')
    port_e = editor_port()
    prepare_state()
    prepare_admin()
    server_state(False, 'starting')
    s = first_settings(port_e)
    if s is None:
        return 0                                      # (Stopped while world.toml was still broken.)
    workers = s['workers'] if s['workers'] is not None else default_workers()
    say(f'"{s["name"]}": on central {s["central"]} as {s["world_id"]}, port {port}, {workers} worker thread(s)')
    if s['cheats'] != 'off':
        say(f'cheats = "{s["cheats"]}": the world is listed "cheats on"' +
            (' (every player may give themselves Gifts and goods)' if s['cheats'] == 'everyone' else
             ' (the host\'s tools may give Gifts and tiers, and reward the host\'s own wolves)'))
    say(upnp_note(s, port))

    # (The agent and the server talk through files here: both write, each reads the other's.)
    agent_dir = private_dir(STATE / 'agent', HELPER, SERVER_USER, 0o770)
    # (Central's word on this world, its kind and host, where only the agent's user writes: the server's user can write
    # the shared directory, so it could otherwise mark the world official and lock the Editor: Docs/Design/78, 6.7.)
    central_dir = private_dir(STATE / 'central', HELPER)
    world = Supervisor(port, port_e, agent_dir, central_dir)

    # The Editor first, so it says how far the world is while it sets up (doc 78, 6.5).
    progress('starting', 0)
    write_registry(s['name'])
    write_admin_settings(s, port_e)
    world.central = mirror_central()
    world.apply_editor(s)
    write_world_image(s)
    world.backups = mirror_backups()
    progress('database', 5)
    wait_for_database()
    rebuild = rebuild_before_update(port_e)           # (Memory first, before anything is migrated: doc 78, 12.5.)
    update_backup(s)
    progress('importing', 20)
    import_world(s['name'])
    world_kind_after_import(s['name'])
    if rebuild and not stopping:
        world.rebuild_wait = rebuild_after_update(port_e)
    record_run(s)
    put_back_accounts()
    if stopping:
        if world.editor is not None:
            world.editor.stop()
        return 0
    progress('ready', 100)
    world.database_ready = True
    if world.editor is not None:
        first_owner(port_e)

    world_db = tools_module('world_db')
    world.apply(s)
    world.share.tidy()
    return world.run(world_db.libpq_conninfo(KEY, 'game'))


if __name__ == '__main__':
    sys.exit(main())
