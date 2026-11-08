#!/usr/bin/env python3
"""The world container's entrypoint (Docs/Design/77, section 8; Package/README.md). Standard library only (with the
image's psycopg through tools/world_db.py). Run by tini as PID 1's child; one container holds the whole world:

1. Reads /package/host/world.toml (the host's settings, Package/world.toml; their folder is mounted at /package/host)
   and the database passwords the db service made on its first start (/run/ratw-secrets/passwords.env).
2. Waits for the database, then imports the world package (/package/world) into it: `tools/world_package.py import
   /package/world --key world --name NAME`, which does nothing if it was imported before. RATW_PACKAGE_IMPORT=skip
   instead only applies the migrations (an image built without a world export, to try the start-up).
3. With an AI key, writes the NPC Mind's config (tools/npc_bridge.py's format, mode 0600) and starts the Mind on
   127.0.0.1; without one, NPCs speak their authored lines.
4. With a central: ticket mode. The world agent runs with the host token (RATW_HOST_TOKEN, or world.toml's token_file;
   never an argument), keeping central's keys, the heartbeat and the ban list; without a token, only central's keys
   are kept (`world_agent.py keys`), so players can join by address but the world isn't listed. Without a central:
   a private LAN world in password mode (accounts kept by the world itself), serving the bundled page if the image
   has one (README.md).
5. Runs ratw_server, forwarding SIGTERM and SIGINT to it. Exit 75 (a new release, nobody on) starts it again; 76
   (closed for maintenance) waits a minute and tries again; any other code stops the Mind and the agent and leaves
   with the server's code. A Mind or agent that stops on its own is started again (at most once a minute).

Each child gets only the secrets it needs: the server its database connection (as ratw_game), the Mind the game role's
password, the agent the host token, the import the superuser's and the owner's.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time
import tomllib

APP = Path('/opt/ratw')
TOOLS = APP / 'tools'
SERVER = APP / 'bin' / 'ratw_server'
DATA = Path(os.environ.get('RATW_DATA_DIR', APP / 'Data'))
WEB = APP / 'web'
PACKAGE = Path(os.environ.get('RATW_PACKAGE_DIR', '/package'))
SETTINGS = Path(os.environ.get('RATW_WORLD_TOML', PACKAGE / 'host' / 'world.toml'))   # (The host's folder, read-only.)
SECRETS = Path(os.environ.get('RATW_SECRETS_FILE', '/run/ratw-secrets/passwords.env'))
STATE = Path('/var/lib/ratw')
LOGS = STATE / 'logs'
KEY = 'world'                                   # The one world's key in this container's registry.
MIND_PORT = 18766
RELEASE_EXIT, MAINTENANCE_EXIT = 75, 76         # (Core/RatwGame.cpp and RatwGame.h; tools/live.sh loops on them.)
WORLD_ID = re.compile(r'^world-[0-9a-f]{32}$')
PASSWORDS = ('POSTGRES_PASSWORD', 'RATW_OWNER_PASSWORD', 'RATW_EDITOR_PASSWORD', 'RATW_PUBLISHER_PASSWORD',
             'RATW_GAME_PASSWORD', 'RATW_DM_PASSWORD')
SECRET_ENV = PASSWORDS + ('RATW_HOST_TOKEN', 'RATW_AI_KEY', 'RATW_DATABASE_URL')
DEFAULT_MODELS = {'model': 'gpt-5.4-mini', 'light_model': 'gpt-5.4-nano', 'fallback_model': 'gpt-5.4-mini',
                  'story_model': 'gpt-5.6-luna'}   # (Doc 28: the main voice, the light one, the fallback, stories.)

stopping = False
server: subprocess.Popen | None = None


def say(text: str) -> None:
    print(f'RATW_PACKAGE {text}', flush=True)


def fail(text: str, code: int = 2) -> None:
    print(f'RATW_PACKAGE error: {text}', file=sys.stderr, flush=True)
    sys.exit(code)


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


def read_settings() -> dict:
    try:
        raw = tomllib.loads(SETTINGS.read_text(encoding='utf-8'))
    except FileNotFoundError:
        fail(f'no {SETTINGS}: copy world.toml beside compose.yaml (see README.md).')
    except (tomllib.TOMLDecodeError, UnicodeDecodeError) as error:
        fail(f'world.toml is not valid TOML: {error}')
    world, central, ai, server_ = (raw.get(t, {}) for t in ('world', 'central', 'ai', 'server'))
    if not all(isinstance(t, dict) for t in (world, central, ai, server_)):
        fail('world.toml: [world], [central], [ai] and [server] are tables.')
    tags = world.get('tags', [])
    if not isinstance(tags, list) or not all(isinstance(t, str) for t in tags):
        fail('world.toml: tags is a list of words, ["a", "b"].')
    s = {
        'name': text(world, 'name', most=48),
        'description': text(world, 'description', most=300),
        'region': text(world, 'region', most=32),
        'tags': [t.strip() for t in tags if t.strip()],
        'player_limit': number(world, 'player_limit', 40, 1, 10000),
        'arrivals_per_hour': number(world, 'arrivals_per_hour', None, 1, 100000),
        'private': flag(world, 'private'),
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
    }
    if not s['name']:
        fail('world.toml: give the world a name ([world] name = "...").')
    if any(ord(c) < 32 or ord(c) == 127 for c in s['name']):
        fail('world.toml: the name has a control character in it.')
    if len(s['tags']) > 5 or any(len(t) > 16 for t in s['tags']):
        fail('world.toml: up to 5 tags, 16 characters each at most.')
    if s['private']:
        say('warning: private worlds (a password kept on central) aren\'t built yet; the world is listed as public.')
    if s['central']:
        if not re.match(r'^https?://[^/\s]+$', s['central']):
            fail('world.toml: central\'s url is https://name (no path).')
        if not WORLD_ID.match(s['world_id']):
            fail('world.toml: a world on central needs its id ([world] id = "world-..."), shown on the Host page '
                 'beside the host token.')
    if s['ai_key_file'] and not s['ai_key']:
        s['ai_key'] = read_secret_file(s['ai_key_file'], 'the AI key')
    if s['cost_mode'] not in ('generous', 'balanced', 'frugal'):
        fail('world.toml: cost_mode is generous, balanced or frugal.')
    return s


def read_secret_file(name: str, what: str) -> str:
    path = Path(name) if name.startswith('/') else SETTINGS.parent / name
    try:
        return path.read_text(encoding='utf-8').strip()
    except OSError as error:
        fail(f'world.toml names {path} for {what}, but it can\'t be read ({error.strerror}).')


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


def child_env(*keep: str, **extra: str) -> dict:
    """The environment without any secret but those named."""
    env = {k: v for k, v in os.environ.items() if k not in SECRET_ENV or k in keep}
    env.update(extra)
    return env


def private_dir(path: Path) -> Path:
    path.mkdir(parents=True, exist_ok=True)
    os.chmod(path, 0o700)
    return path


def write_private(path: Path, body: str) -> None:
    temp = path.with_name(f'.{path.name}.new')
    fd = os.open(temp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, 'w', encoding='utf-8') as out:
        out.write(body)
    os.replace(temp, path)


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
    return subprocess.run([sys.executable, *args], cwd=STATE, env=child_env(*keep)).returncode


def import_world(name: str) -> None:
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
                            cwd=STATE, stdout=log, stderr=subprocess.STDOUT, env=child_env('RATW_GAME_PASSWORD'))


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


def host_token(s: dict) -> tuple[list[str], dict]:
    """The agent's token: its --token-file, or RATW_HOST_TOKEN in its environment. ([], {}) if there is none."""
    if s['token_file']:
        path = Path(s['token_file']) if s['token_file'].startswith('/') else SETTINGS.parent / s['token_file']
        if not path.is_file():
            fail(f'world.toml names {path} for the host token, but there is no such file.')
        return ['--token-file', str(path)], {}
    token = os.environ.get('RATW_HOST_TOKEN', '').strip()
    return ([], {'RATW_HOST_TOKEN': token}) if token else ([], {})


def agent_starter(s: dict, port: int, agent_dir: Path):
    args, env = host_token(s)
    log_path = LOGS / 'world-agent.log'
    if not args and not env:
        say('warning: no host token (RATW_HOST_TOKEN, or token_file in world.toml): the world keeps central\'s keys, '
            'so players can join it by address, but it isn\'t on the server list.')
        command = ['keys', '--central', s['central'], '--out', str(agent_dir / 'central-keys.json')]
    else:
        command = ['run', '--central', s['central'], '--dir', str(agent_dir), '--world-port', str(port), *args]
        for name in ('description', 'region', 'host'):
            if s[name]:
                command += [f'--{name}', s[name]]
        if s['tags']:
            command += ['--tags', ','.join(s['tags'])]
        if s['secure']:
            command.append('--secure')

    def start() -> subprocess.Popen:
        log = log_path.open('a', encoding='utf-8')
        return subprocess.Popen([sys.executable, str(TOOLS / 'world_agent.py'), *command], cwd=STATE, stdout=log,
                                stderr=subprocess.STDOUT, env=child_env(**env))
    return start


# ------------------------------------------------------------------------------------------------- the server

def server_command(s: dict, port: int, agent_dir: Path | None, dialogue: bool, workers: int) -> list[str]:
    command = [str(SERVER), '--database', KEY, '--world-kind', 'live', '--port', str(port), '--bind', '0.0.0.0',
               '--workers', str(workers), '--talk-data', str(DATA / 'Talk'), '--voice-data', str(DATA / 'Voice'),
               '--voice-log', str(LOGS / 'npc-voices.jsonl'), '--dm-directory', str(private_dir(STATE / 'dm'))]
    if dialogue:
        command += ['--dialogue', f'http://127.0.0.1:{MIND_PORT}/dialogue']
    if s['central']:
        command += ['--central', s['central'], '--world-id', s['world_id'], '--central-keys',
                    str(agent_dir / 'central-keys.json'), '--world-name', s['name'], '--player-limit',
                    str(s['player_limit'])]
        if s['arrivals_per_hour']:
            command += ['--arrivals-per-hour', str(s['arrivals_per_hour'])]
    elif (WEB / 'index.html').is_file():
        command += ['--web', str(WEB)]
    return command


def on_signal(number, _frame) -> None:
    global stopping
    stopping = True
    if server is not None and server.poll() is None:
        server.send_signal(number)


def main() -> int:
    global server
    for number in (signal.SIGTERM, signal.SIGINT):
        signal.signal(number, on_signal)
    s = read_settings()
    load_passwords()
    os.environ.setdefault('RATW_DB_HOST', 'db')
    os.environ.setdefault('RATW_DB_PORT', '5432')
    os.environ.setdefault('POSTGRES_USER', 'postgres')
    LOGS.mkdir(parents=True, exist_ok=True)
    port = int(os.environ.get('RATW_PORT', '7788'))
    if not 1 <= port <= 65535:
        fail('RATW_PORT is a port number.')
    workers = s['workers'] if s['workers'] is not None else default_workers()
    say(f'"{s["name"]}": ' + (f'on central {s["central"]} as {s["world_id"]}' if s['central'] else
                              'a private LAN world (no central: accounts and passwords kept by this world)') +
        f', port {port}, {workers} worker thread(s)')

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
    agent_dir = None
    if s['central']:
        agent_dir = private_dir(STATE / 'agent')
        agent = Helper('world agent', agent_starter(s, port, agent_dir))
        agent.run()
        helpers.append(agent)
    elif not (WEB / 'index.html').is_file():
        say('warning: a LAN world with no page in this image: players need a client that signs in to it directly')

    command = server_command(s, port, agent_dir, dialogue, workers)
    code = 0
    while not stopping:
        say('starting the world server')
        server = subprocess.Popen(command, cwd=STATE, env=child_env(RATW_DATABASE_URL=database_url))
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
