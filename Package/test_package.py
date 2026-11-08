#!/usr/bin/env python3
"""Tests of the world package's entrypoint and stage.sh (Docs/Design/77, section 8), without Docker or a database:

    python3 Package/test_package.py            (or: python3 -m unittest discover -s Package)

The entrypoint runs here as this user (not root), reading world.toml and the host's files from a temporary folder.
"""
from __future__ import annotations

import contextlib
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
os.environ['RATW_APP_DIR'] = str(ROOT)            # (tools/ beside it, as /opt/ratw/tools is in the image.)
spec = importlib.util.spec_from_file_location('package_entrypoint', HERE / 'entrypoint.py')
E = importlib.util.module_from_spec(spec)
spec.loader.exec_module(E)

TOKEN = 'rh_' + 'a1' * 20
WORLD = 'world-' + '0123456789abcdef' * 2
SETTINGS = f'''[world]
name = "Wolf Hollow"
id = "{WORLD}"
[central]
url = "https://central.example.org"
'''


class Settings(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory(prefix='fix5-bundle-')
        self.dir = Path(self.folder.name)
        self.saved = E.SETTINGS, os.environ.pop('RATW_HOST_TOKEN', None), os.environ.pop('RATW_AI_KEY', None)
        E.SETTINGS = self.dir / 'world.toml'

    def tearDown(self):
        E.SETTINGS = self.saved[0]
        for name, value in (('RATW_HOST_TOKEN', self.saved[1]), ('RATW_AI_KEY', self.saved[2])):
            os.environ.pop(name, None)
            if value is not None:
                os.environ[name] = value
        self.folder.cleanup()

    def write(self, body: str, name: str = 'world.toml', bom: bool = False, mode: int | None = None) -> Path:
        path = self.dir / name
        path.write_bytes((b'\xef\xbb\xbf' if bom else b'') + body.encode('utf-8'))
        if mode is not None:
            os.chmod(path, mode)
        return path

    def refused(self, *texts: str) -> str:
        err = io.StringIO()
        with contextlib.redirect_stderr(err), contextlib.redirect_stdout(io.StringIO()), self.assertRaises(SystemExit):
            E.read_settings()
        for t in texts:
            self.assertIn(t, err.getvalue())
        return err.getvalue()

    def settings(self) -> dict:
        with contextlib.redirect_stdout(io.StringIO()):
            return E.read_settings()

    def test_the_example_settings_need_only_an_id(self):
        body = (HERE / 'world.toml').read_text(encoding='utf-8')
        self.write(body)
        self.refused('needs its id')
        self.write(body.replace('id = ""', f'id = "{WORLD}"'))
        s = self.settings()
        self.assertEqual((s['name'], s['central'], s['token']), ('My Ridgemere', 'https://central.example.org', ''))

    def test_a_world_without_central_is_refused(self):
        """No LAN mode (doc 77, Q9): password mode signs in only from loopback, which a container never sees."""
        self.write(SETTINGS.replace('url = "https://central.example.org"', 'url = ""'))
        self.refused('[central] url is needed', 'isn\'t offered yet')

    def test_a_byte_order_mark_is_allowed(self):
        """Notepad's "UTF-8 with BOM" and PowerShell 5's Set-Content -Encoding UTF8, in world.toml and the key file."""
        self.write(SETTINGS + '[ai]\nkey_file = "ai-key"\n', bom=True)
        self.write('sk-abc123\n', 'ai-key', bom=True)
        self.assertEqual(self.settings()['ai_key'], 'sk-abc123')

    def test_files_that_cant_be_read_say_so(self):
        if E.as_root():
            self.skipTest('root reads any file')
        self.write(SETTINGS, mode=0)
        self.refused('world.toml', 'permission denied', f'uid {os.getuid()}')
        os.chmod(self.dir / 'world.toml', 0o600)
        self.write(SETTINGS + 'token_file = "host-token"\n')
        self.write(TOKEN, 'host-token', mode=0)
        self.refused('host token', 'host-token can\'t be read (permission denied)')

    def test_the_host_token_is_read_and_checked_before_the_agent_starts(self):
        self.write(SETTINGS + 'token_file = "host-token"\n')
        self.refused('host token', 'there is no')
        self.write(f'\n{TOKEN}\n', 'host-token', bom=True)
        self.assertEqual(self.settings()['token'], TOKEN)
        self.write('rh_ two words\n', 'host-token')
        self.refused('spaces or odd characters')
        self.write('\n', 'host-token')
        self.refused('is empty')
        self.write(SETTINGS)
        os.environ['RATW_HOST_TOKEN'] = 'not a token'
        self.refused('RATW_HOST_TOKEN', 'spaces or odd characters')
        os.environ['RATW_HOST_TOKEN'] = f' {TOKEN} '
        self.assertEqual(self.settings()['token'], TOKEN)

    def test_the_token_never_goes_in_clear_to_the_internet(self):
        os.environ['RATW_HOST_TOKEN'] = TOKEN
        self.write(SETTINGS.replace('https://central.example.org', 'http://8.8.8.8'))
        self.refused('http://', 'https://')
        self.write(SETTINGS.replace('https://central.example.org', 'http://127.0.0.1:8790'))
        self.assertEqual(self.settings()['central'], 'http://127.0.0.1:8790')
        del os.environ['RATW_HOST_TOKEN']             # (No token: only the keys, which carry none.)
        self.write(SETTINGS.replace('https://central.example.org', 'http://8.8.8.8'))
        self.assertEqual(self.settings()['token'], '')

    def test_secure_needs_a_host_name(self):
        self.write(SETTINGS + 'secure = true\n')
        self.refused('secure = true needs host')
        self.write(SETTINGS + 'secure = true\nhost = "wolves.example.org"\n')
        self.assertTrue(self.settings()['secure'])

    def test_the_name_is_measured_in_bytes_as_the_server_does(self):
        """Core/RatwGameTickets.cpp refuses a --world-name over 48 bytes: the container would restart for ever."""
        name = 'Wölfe über Grünmoor – Zuflucht für Rudelgefährte'
        self.assertEqual((len(name), len(name.encode('utf-8'))), (48, 55))
        self.write(SETTINGS.replace('Wolf Hollow', name))
        self.refused('55 bytes', 'at most 48')
        self.write(SETTINGS.replace('Wolf Hollow', 'Wölfe über Grünmoor'))
        self.assertEqual(self.settings()['name'], 'Wölfe über Grünmoor')


class Children(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory(prefix='fix5-bundle-')
        self.dir = Path(self.folder.name)
        self.saved = E.STATE, E.LOGS
        E.STATE, E.LOGS = self.dir, self.dir / 'logs'
        E.LOGS.mkdir()
        self.s = {'central': 'https://central.example.org', 'world_id': WORLD, 'name': 'Wolf Hollow',
                  'player_limit': 40, 'arrivals_per_hour': None, 'description': '', 'region': '', 'host': '',
                  'tags': [], 'secure': False, 'token': ''}

    def tearDown(self):
        E.STATE, E.LOGS = self.saved
        self.folder.cleanup()

    def test_the_server_is_always_a_ticket_world(self):
        command = E.server_command(self.s, 7788, self.dir / 'agent', False, 2)
        self.assertIn('--central', command)
        self.assertEqual(command[command.index('--world-name') + 1], 'Wolf Hollow')
        self.assertNotIn('--web', command)

    def test_no_token_keeps_only_the_keys_and_drops_a_tokens_host_and_bans(self):
        agent = self.dir / 'agent'
        agent.mkdir()
        for stale in ('host.json', 'bans.json', 'central-keys.json'):
            (agent / stale).write_text('{}')
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            E.agent_starter(self.s, 7788, agent)
        self.assertIn('host\'s commands', out.getvalue())
        self.assertEqual(sorted(p.name for p in agent.iterdir()), ['central-keys.json'])

    def test_the_agents_lines_reach_the_container_log(self):
        """`docker compose logs world` shows why the agent stopped, or that the world is listed."""
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            E.tee(io.StringIO('world agent: listed on central at 1.2.3.4:7788\n\nworld agent: bye\n'),
                  E.LOGS / 'world-agent.log')
        self.assertEqual(out.getvalue().splitlines(), ['RATW_PACKAGE world agent: listed on central at 1.2.3.4:7788',
                                                       'RATW_PACKAGE world agent: bye'])
        self.assertIn('bye', (E.LOGS / 'world-agent.log').read_text())

    def test_a_childs_environment_holds_only_its_secrets(self):
        saved = dict(os.environ)
        try:
            os.environ.update({k: 'secret' for k in E.SECRET_ENV})
            env = E.child_env('RATW_GAME_PASSWORD', RATW_DATABASE_URL='x')
            self.assertEqual(sorted(k for k in env if k in E.SECRET_ENV), ['RATW_DATABASE_URL', 'RATW_GAME_PASSWORD'])
            self.assertEqual(env['HOME'], str(self.dir))
        finally:
            os.environ.clear()
            os.environ.update(saved)

    def test_children_run_as_their_own_users_under_root(self):
        if E.as_root():
            self.assertEqual(E.run_as('ratw-server'),
                             {'user': 'ratw-server', 'group': 'ratw-server', 'extra_groups': []})
        else:
            self.assertEqual(E.run_as('ratw-server'), {})


class Stage(unittest.TestCase):
    """stage.sh replaces --out: never a directory that isn't one of its contexts."""

    def stage(self, out: str, cwd: Path) -> subprocess.CompletedProcess:
        return subprocess.run(['bash', str(HERE / 'stage.sh'), '--out', out], cwd=cwd, capture_output=True, text=True)

    def test_a_wrong_out_deletes_nothing(self):
        with tempfile.TemporaryDirectory(prefix='fix5-bundle-') as folder:
            victim = Path(folder) / 'victim'
            victim.mkdir()
            (victim / 'precious.txt').write_text('mine')
            for out, cwd, says in (('', victim, 'needs a directory'), ('.', victim, 'isn\'t a staged context'),
                                   (str(victim), ROOT, 'isn\'t a staged context'),
                                   ('Core', ROOT, 'isn\'t a staged context'), ('..', ROOT, 'holds the repository'),
                                   ('/', ROOT, 'holds the repository'), (str(ROOT), victim, 'holds the repository'),
                                   (str(victim / 'precious.txt'), ROOT, 'is a file')):
                result = self.stage(out, cwd)
                self.assertEqual(result.returncode, 2, (out, result.stderr))
                self.assertIn(says, result.stderr, out)
            self.assertEqual((victim / 'precious.txt').read_text(), 'mine')
            self.assertTrue((ROOT / 'Core').is_dir())

    def test_a_context_is_staged_and_staged_again(self):
        with tempfile.TemporaryDirectory(prefix='fix5-bundle-') as folder:
            out = Path(folder) / 'context'
            for _ in range(2):
                result = self.stage(str(out), ROOT)
                self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((out / 'BUILD_ID').is_file() and (out / 'app' / 'Package' / 'entrypoint.py').is_file())
            self.assertFalse((out / 'app' / 'web').exists())
            self.assertFalse((out / 'app' / 'Package' / 'test_package.py').exists())

    def test_a_refused_run_leaves_the_context_as_it_was(self):
        """A --world that isn't an export, or a bad --build-id, is refused before the context is touched."""
        with tempfile.TemporaryDirectory(prefix='fix5-bundle-') as folder:
            out = Path(folder) / 'context'
            for args in (['--world', str(Path(folder) / 'nonexistent')], ['--build-id', 'no spaces']):
                result = subprocess.run(['bash', str(HERE / 'stage.sh'), '--out', str(out), *args], cwd=ROOT,
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertFalse(out.exists(), args)

    def test_a_context_left_halfway_is_staged_again(self):
        """A run that stopped halfway (a failed copy, ^C) left the marker, and the next run replaces what it left."""
        with tempfile.TemporaryDirectory(prefix='fix5-bundle-') as folder:
            out = Path(folder) / 'context'
            (out / 'src').mkdir(parents=True)
            (out / 'src' / 'half.txt').write_text('half')
            (out / '.ratw-context').write_text('')
            result = self.stage(str(out), ROOT)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse((out / 'src' / 'half.txt').exists())
            self.assertTrue((out / 'BUILD_ID').is_file() and (out / '.ratw-context').is_file())


class Compose(unittest.TestCase):
    def test_the_passwords_are_roots_alone(self):
        """The world's entrypoint reads them as root; the Mind and the agent (uid 10001) mustn't."""
        compose = (HERE / 'compose.yaml').read_text()
        start = compose[compose.index('  db-start:'):compose.index('  db-roles:')]
        self.assertNotIn('10001', start)
        self.assertIn('chown 0:0 "$$dir" "$$dir/passwords.env" && chmod 0700 "$$dir" && chmod 0600', start)
        # (On every start, not only the first: outside the `if` that makes them.)
        self.assertGreater(start.index('chown 0:0'), start.index('fi\n'))


if __name__ == '__main__':
    unittest.main()
