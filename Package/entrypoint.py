#!/usr/bin/env python3
"""The world container's entrypoint (Docs/Design/77, section 8; Package/README.md). Standard library only (with the
image's psycopg through tools/world_db.py). Run by tini as PID 1's child, as root, and only to read what is private
and start the children; each child runs as a user of its own. One container holds the whole world:

1. Reads the host's settings, /etc/ratw/host/world.toml (Package/world.toml; their folder is mounted at
   /etc/ratw/host, inside /etc/ratw, which only root may open), the host token and AI key files it names, and the
   database passwords the db service made on its first start (/run/ratw-secrets/passwords.env). Root reads them
   whatever their modes, so a host may keep them 0600.
2. Waits for the database, then imports the world package (/package/world) into it: `tools/world_package.py import
   /package/world --key world --name NAME`. On the first start that makes the world; on every later one it applies
   the migrations and keeps the world as it is, even when this image carries another package (an update keeps the
   world's own state; `docker compose down -v` starts afresh). Only a real failure stops the start.
   RATW_PACKAGE_IMPORT=skip instead only applies the migrations (an image built without a world export).
3. With an AI key, writes the NPC Mind's config (tools/npc_bridge.py's format, mode 0600) and starts the Mind on
   127.0.0.1; without one, NPCs speak their authored lines.
4. Ticket mode, always: a world signs players in through central (a world with no central, a LAN world, isn't offered
   yet: doc 77, Q9). The world agent runs with the host token (RATW_HOST_TOKEN, or world.toml's token_file, read
   here and given in its environment, never an argument), keeping central's keys, the heartbeat and the ban list;
   without a token, only central's keys are kept (`world_agent.py keys`), so players can join by address but the
   world isn't listed and has no host. The agent's lines go to the container's log as well as logs/world-agent.log.
5. Runs ratw_server, forwarding SIGTERM and SIGINT to it. Exit 75 (a new release, nobody on) starts it again; 76
   (closed for maintenance) waits a minute and tries again; any other code stops the Mind and the agent and leaves
   with the server's code. A Mind or agent that stops on its own is started again (at most once a minute).
6. UPnP (world.toml's [network] upnp, default true) isn't tried here: this container is on Docker's own network, from
   which the router's search can't be reached. compose.yaml's upnp service tries it from the host's network
   (tools/world_upnp.py hold); this only checks the setting and says where to look.

Each child is given only the secrets it needs, in its environment: the import the superuser's and the owner's
passwords; the Mind the game role's password and the AI key; the agent the host token. Those three run as ratw, so
they are kept from the passwords file and the host's folder (root's alone), but not from one another: while both
run, the Mind could read the agent's environment and the agent the Mind's (the host token; the game role's password,
the AI key and its config). The server, which faces the internet, is the one kept apart: it runs as ratw-server with
only its database connection (as ratw_game), and can't read the passwords file, the host's folder (under
/etc/ratw), the Mind's config or the other processes' environments (another user's). Started as another user than
root (a test), everything runs as that user.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shutil
import signal
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
KEY = 'world'                                   # The one world's key in this container's registry.
MIND_PORT = 18766
RELEASE_EXIT, MAINTENANCE_EXIT = 75, 76         # (Core/RatwGame.cpp and RatwGame.h; tools/live.sh loops on them.)
WORLD_ID = re.compile(r'^world-[0-9a-f]{32}$')
NAME_BYTES = 48                                 # (Core/RatwGameTickets.cpp: the server counts a name's UTF-8 bytes.)
PASSWORDS = ('POSTGRES_PASSWORD', 'RATW_OWNER_PASSWORD', 'RATW_EDITOR_PASSWORD', 'RATW_PUBLISHER_PASSWORD',
             'RATW_GAME_PASSWORD', 'RATW_DM_PASSWORD')
SECRET_ENV = PASSWORDS + ('RATW_HOST_TOKEN', 'RATW_AI_KEY', 'RATW_DATABASE_URL')
DEFAULT_MODELS = {'model': 'gpt-5.4-mini', 'light_model': 'gpt-5.4-nano', 'fallback_model': 'gpt-5.4-mini',
                  'story_model': 'gpt-5.6-luna'}   # (Doc 28: the main voice, the light one, the fallback, stories.)
HELPER, SERVER_USER = 'ratw', 'ratw-server'     # (The image's users, Package/Dockerfile: uids 10001 and 10002.)
# world.toml's cheats (Docs/Design/78, 8), each but "off" listed "cheats on": the server's flag for each. "host": the
# host's tools may give Gifts and tiers and reward the host's own wolves; "everyone": --dev-tools, every player's
# socket cheats (Gifts and goods from nothing) and the Dev Console.
CHEATS = {'off': [], 'host': ['--host-cheats'], 'everyone': ['--dev-tools']}

stopping = False
server: subprocess.Popen | None = None


def say(text: str) -> None:
    print(f'RATW_PACKAGE {text}', flush=True)


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


def agent_module():
    """tools/world_agent.py, for its token and http:// checks (the same the agent makes, made here so they're seen)."""
    if str(TOOLS) not in sys.path:
        sys.path.insert(0, str(TOOLS))
    import world_agent
    return world_agent


# --------------------------------------------------------------------------------------------------- settings

def text(table: dict, name: str, default: str = '', most: int = 4096) -> str:
    value = table.get(name, default)
    if not isinstance(value, str):
        fail(f'world.toml: {name} must be text ("...").')
    value = value.strip()
    if len(value) > most:
        fail(f'world.toml: {name} is {len(value)} characters; at most {most}.')
    return value


def number(table: dict, name: str, default: int | None, low: int, high: int) -> int | None:
    value = table.get(name, default)
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, int) or not low <= value <= high:
        fail(f'world.toml: {name} must be a whole number from {low} to {high}.')
    return value


def flag(table: dict, name: str, default: bool = False) -> bool:
    value = table.get(name, default)
    if not isinstance(value, bool):
        fail(f'world.toml: {name} must be true or false.')
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
    world, central, ai, server_, network = (raw.get(t, {}) for t in ('world', 'central', 'ai', 'server', 'network'))
    if not all(isinstance(t, dict) for t in (world, central, ai, server_, network)):
        fail('world.toml: [world], [central], [ai], [server] and [network] are tables.')
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
        'workers': number(server_, 'workers', None, 0, 16),
        'upnp': flag(network, 'upnp', True),
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


def own(path: Path, user: str | None, group: str | None = None) -> None:
    if as_root() and user:
        shutil.chown(path, user, group or user)


def private_dir(path: Path, user: str | None = HELPER, group: str | None = None, mode: int = 0o700) -> Path:
    """A directory of `user`'s (and `group`'s too, with mode 0o770: both write there; what each writes is 0644)."""
    path.mkdir(parents=True, exist_ok=True)
    own(path, user, group)
    os.chmod(path, mode)
    return path


def write_private(path: Path, body: str, user: str | None = HELPER) -> None:
    temp = path.with_name(f'.{path.name}.new')
    fd = os.open(temp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, 'w', encoding='utf-8') as out:
        out.write(body)
    own(temp, user)
    os.replace(temp, path)


def prepare_state() -> None:
    """The container's own state, each part its user's: the logs (a volume) and the agent's directory shared by the
    helpers and the server, the server's voice log its own."""
    private_dir(STATE, None, mode=0o755)
    private_dir(LOGS, HELPER, SERVER_USER, 0o770)
    voices = LOGS / 'npc-voices.jsonl'
    voices.touch(exist_ok=True)
    own(voices, SERVER_USER)
    if as_root():
        os.chmod(voices, 0o640)


# ---------------------------------------------------------------------------------------------- the database

def write_registry(name: str) -> None:
    """This container's one-world registry (tools/world_db.py: RATW_WORLDS_FILE), written afresh at every start."""
    config = private_dir(STATE / 'config')
    database = os.environ.get('RATW_WORLD_DATABASE', 'ratw_world')
    entry = {'key': KEY, 'name': name, 'database': database, 'kind': 'live'}
    write_private(config / 'worlds.json', '{"worlds": [\n  ' + json.dumps(entry) + '\n]}\n')
    write_private(config / 'worlds.local.json', json.dumps({'live': KEY}) + '\n')


def wait_for_database(seconds: float = 180) -> None:
    sys.path.insert(0, str(TOOLS))
    import world_db
    deadline, said = time.monotonic() + seconds, False
    while True:
        try:
            with world_db.superuser() as su:
                su.execute('SELECT 1')
            return
        except world_db.DatabaseError as error:
            if time.monotonic() > deadline or stopping:
                fail(f'the database never answered: {error}')
            if not said:
                say(f'waiting for the database ({error})')
                said = True
            time.sleep(2)


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


def mind_config(s: dict) -> Path | None:
    if not s['ai_key']:
        return None
    if any(ord(c) <= 32 or ord(c) >= 127 for c in s['ai_key']):
        fail('world.toml: the AI key has a space or an unusual character in it.')
    config = {'enabled': True, 'endpoint': 'https://api.openai.com/v1/chat/completions', 'api_key': s['ai_key'],
              **{k: v for k, v in s['models'].items() if v}, 'cost_mode': s['cost_mode']}
    path = private_dir(STATE / 'mind') / 'npc-ai.json'
    write_private(path, json.dumps(config, indent=2) + '\n')
    return path


def start_mind(config: Path) -> subprocess.Popen:
    log = (LOGS / 'npc-mind.log').open('a', encoding='utf-8')
    return subprocess.Popen([sys.executable, str(TOOLS / 'npc_mind.py'), '--config', str(config), '--database', KEY,
                             '--port', str(MIND_PORT), '--ledger', str(LOGS / 'npc-mind-calls.jsonl')],
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


def tee(stream, log_path: Path) -> None:
    """A child's lines into its log file and the container's log (`docker compose logs world`)."""
    with log_path.open('a', encoding='utf-8') as log:
        for line in stream:
            log.write(line)
            log.flush()
            if line.strip():
                say(line.rstrip('\n'))


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


def on_signal(number, _frame) -> None:
    global stopping
    stopping = True
    if server is not None and server.poll() is None:
        server.send_signal(number)


def main() -> int:
    global server
    for number in (signal.SIGTERM, signal.SIGINT):
        signal.signal(number, on_signal)
    if not as_root():
        say('warning: not started as root: the server runs as the same user as the rest, and could read their '
            'secrets')
    s = read_settings()
    load_passwords()
    os.environ.setdefault('RATW_DB_HOST', 'db')
    os.environ.setdefault('RATW_DB_PORT', '5432')
    os.environ.setdefault('POSTGRES_USER', 'postgres')
    port = int(os.environ.get('RATW_PORT', '7788'))
    if not 1 <= port <= 65535:
        fail('RATW_PORT is a port number.')
    prepare_state()
    workers = s['workers'] if s['workers'] is not None else default_workers()
    say(f'"{s["name"]}": on central {s["central"]} as {s["world_id"]}, port {port}, {workers} worker thread(s)')
    if s['cheats'] != 'off':
        say(f'cheats = "{s["cheats"]}": the world is listed "cheats on"' +
            (' (every player may give themselves Gifts and goods)' if s['cheats'] == 'everyone' else
             ' (the host\'s tools may give Gifts and tiers, and reward the host\'s own wolves)'))
    say(upnp_note(s, port))

    write_registry(s['name'])
    wait_for_database()
    import_world(s['name'])
    if stopping:
        return 0

    sys.path.insert(0, str(TOOLS))
    import world_db
    database_url = world_db.libpq_conninfo(KEY, 'game')

    helpers = []
    dialogue = False
    config = mind_config(s)
    if config:
        mind = Helper('NPC Mind', lambda: start_mind(config))
        dialogue = mind_ready(mind)
        say('the NPC Mind is ready (live model)' if dialogue else
            'warning: the NPC Mind did not start (see logs/npc-mind.log); NPCs use their authored lines')
        if dialogue:
            helpers.append(mind)
    else:
        say('no AI key: NPCs use their authored lines')
    # (The agent and the server talk through files here: both write, each reads the other's.)
    agent_dir = private_dir(STATE / 'agent', HELPER, SERVER_USER, 0o770)
    # (Central's word on this world, its kind and host, where only the agent's user writes: the server's user can write
    # the shared directory, so it could otherwise mark the world official and lock the Editor: Docs/Design/78, 6.7.)
    central_dir = private_dir(STATE / 'central', HELPER)
    agent = Helper('world agent', agent_starter(s, port, agent_dir, central_dir))
    agent.run()
    helpers.append(agent)

    command = server_command(s, port, agent_dir, dialogue, workers)
    code = 0
    while not stopping:
        say('starting the world server')
        server = subprocess.Popen(command, cwd=STATE, env=child_env(RATW_DATABASE_URL=database_url),
                                  **run_as(SERVER_USER))
        while server.poll() is None:
            try:
                server.wait(5)
            except subprocess.TimeoutExpired:
                for h in helpers:
                    h.check()
        code = server.returncode
        if stopping:
            break
        if code == RELEASE_EXIT:
            say('the server stopped for a new release; starting it again')
            continue
        if code == MAINTENANCE_EXIT:
            say('the world is closed for maintenance; trying again in a minute')
            for _ in range(60):
                if stopping:
                    break
                time.sleep(1)
            continue
        say(f'the world server stopped (exit {code})')
        break
    for h in reversed(helpers):                   # (The agent first: its leave takes the world off the list.)
        h.stop()
    say(f'stopped (exit {code})')
    if stopping and code in (0, -signal.SIGTERM, -signal.SIGINT):
        return 0
    return 128 - code if code < 0 else code       # (Killed by a signal: the shell's 128 + its number.)


if __name__ == '__main__':
    sys.exit(main())
