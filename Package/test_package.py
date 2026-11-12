#!/usr/bin/env python3
"""Tests of the world package's entrypoint, compose.yaml, the host's folder and stage.sh (Docs/Design/77, section 8;
Docs/Design/78, sections 3, 6 and 12), without Docker or a database:

    python3 Package/test_package.py            (or: python3 -m unittest discover -s Package)

The entrypoint runs here as this user (not root), reading world.toml and the host's files from a temporary folder.
"""
from __future__ import annotations

import base64
import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

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
        self.assertEqual((s['name'], s['central'], s['token'], s['cheats']),
                         ('My Ridgemere', 'https://central.example.org', '', 'off'))

    def test_cheats_are_off_host_or_everyone(self):
        """Doc 78, 8 (Phase M1): world.toml's cheats alone, default off; anything else stops the start."""
        self.write(SETTINGS)
        self.assertEqual(self.settings()['cheats'], 'off')
        for value in ('off', 'host', 'everyone'):
            self.write(SETTINGS.replace('[central]', f'cheats = "{value}"\n[central]'))
            self.assertEqual(self.settings()['cheats'], value)
        self.write(SETTINGS.replace('[central]', 'cheats = "on"\n[central]'))
        self.refused('cheats is "off", "host" or "everyone"')
        self.write(SETTINGS.replace('[central]', 'cheats = true\n[central]'))
        self.refused('cheats must be text')

    def test_the_editor_settings(self):
        """Doc 78, 3.1 and Q23: [editor] enabled (default true), owner (the DM tool's name rule, or left out), new_devices
        open or approve. Never a password."""
        self.write(SETTINGS)
        self.assertEqual(self.settings()['editor'], {'enabled': True, 'owner': '', 'new_devices': 'open'})
        self.write(SETTINGS + '[editor]\nenabled = false\nowner = "wolf_mother"\nnew_devices = "approve"\n')
        self.assertEqual(self.settings()['editor'], {'enabled': False, 'owner': 'wolf_mother', 'new_devices': 'approve'})
        for body, says in (('owner = "Ab"', '[editor] owner is 3 to 32'), ('owner = "9lives"', '[editor] owner'),
                           ('new_devices = "ask"', 'new_devices is "open" or "approve"'),
                           ('enabled = "no"', '[editor] enabled must be true or false')):
            self.write(SETTINGS + f'[editor]\n{body}\n')
            self.refused(says)
        self.write(SETTINGS.replace('[central]', 'editor = 1\n[central]'))
        self.settings()                                       # ([world] editor is just an unknown key.)
        self.write('editor = 1\n' + SETTINGS)
        self.refused('[editor] are tables')

    def test_a_broken_world_toml_on_a_restart_keeps_the_world_waiting(self):
        """Doc 78, 12.4: read again before each server start; a mistake then is said, and doesn't end the container
        (the Editor stays up), so the host can put it right."""
        self.write(SETTINGS)
        self.assertEqual(E.reread_settings()['name'], 'Wolf Hollow')
        self.write(SETTINGS.replace('id = ', 'id = 1 #'))
        err, out = io.StringIO(), io.StringIO()
        with contextlib.redirect_stderr(err), contextlib.redirect_stdout(out):
            self.assertIsNone(E.reread_settings())
        self.assertIn('RATW_PACKAGE error:', err.getvalue())
        self.assertIn('waits until world.toml is put right', out.getvalue())
        self.write(SETTINGS + '[ai]\nkey = "sk bad"\n')         # (Checked with the rest, never later in the loop.)
        self.refused('AI key has a space')

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

    def test_the_server_takes_its_cheats_from_world_toml_and_no_dm_directory(self):
        """Doc 78, 8 and Q18 (Phase M1): off gives no cheat flag, host --host-cheats, everyone --dev-tools; never
        --dm-directory (nothing in the container reads it; the legacy Storykeeper stays unstarted)."""
        for cheats, flags in (('off', []), ('host', ['--host-cheats']), ('everyone', ['--dev-tools'])):
            command = E.server_command({**self.s, 'cheats': cheats}, 7788, self.dir / 'agent', False, 2)
            self.assertEqual([c for c in command if c in ('--host-cheats', '--dev-tools')], flags, cheats)
            self.assertNotIn('--dm-directory', command)
            self.assertNotIn('--official', command)
        self.assertFalse((self.dir / 'dm').exists(), 'and no DM directory is made')

    def test_no_token_keeps_only_the_keys_and_drops_a_tokens_host_and_bans(self):
        agent = self.dir / 'agent'
        agent.mkdir()
        for stale in ('host.json', 'bans.json', 'central-keys.json'):
            (agent / stale).write_text('{}')
        own = self.dir / 'central'
        own.mkdir()
        (own / 'central.json').write_text('{"kind": "community", "host": "acct-x"}')
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            E.agent_starter(self.s, 7788, agent, own)
        self.assertIn('host\'s commands', out.getvalue())
        self.assertEqual(sorted(p.name for p in agent.iterdir()), ['central-keys.json'])
        self.assertFalse((own / 'central.json').exists(), 'nor its central.json (doc 78, 6.7)')

    def test_the_agents_lines_reach_the_container_log(self):
        """`docker compose logs world` shows why the agent stopped, or that the world is listed."""
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            E.tee(io.StringIO('world agent: listed on central at 1.2.3.4:7788\n\nworld agent: bye\n'),
                  E.LOGS / 'world-agent.log')
        self.assertEqual(out.getvalue().splitlines(), ['RATW_AGENT world agent: listed on central at 1.2.3.4:7788',
                                                       'RATW_AGENT world agent: bye'])
        self.assertIn('bye', (E.LOGS / 'world-agent.log').read_text())

    def test_a_childs_lines_cant_pass_for_roots(self):
        """Doc 78's R1 review, security 10: a compromised agent or server forged root's "Editor certificate:" line (the
        agent's lines went out under RATW_PACKAGE, and escapes and breaks passed). Each child has its own prefix, its
        control characters and invisible marks dropped, and root reads no endless line."""
        forged = ('RATW_PACKAGE Editor certificate: 0000 1111\n'
                  'ok\x0bRATW_PACKAGE Editor certificate: 2222\x1b[2K\x1b[1A\u202e\n'
                  + 'x' * 10000 + '\n')
        for run in (lambda s: E.tee(s, E.LOGS / 'world-agent.log'), lambda s: E.pass_lines(s, [])):
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                run(io.StringIO(forged))
            lines = out.getvalue().splitlines()
            self.assertTrue(lines and all(re.match(r'^RATW_(AGENT|SERVER) ', line) for line in lines), lines[:3])
            self.assertFalse(any(re.search('[\x00-\x1f\x7f\u202e]', line) for line in lines))
            self.assertEqual(sum(1 for line in lines if 'xxxx' in line), 3, 'a 10,000-character line in pieces of 4,096')
            self.assertTrue(all(len(line) <= E.LINE_MOST + 12 for line in lines))
        kept = []
        with contextlib.redirect_stdout(io.StringIO()):
            E.pass_lines(io.StringIO('RATW_X \x1b[31mred\n'), kept)
        self.assertEqual(kept, ['RATW_X [31mred'])

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

    def stage(self, out: str, cwd: Path, *more: str) -> subprocess.CompletedProcess:
        # (Without the Editor unless a test asks: building and signing it is EditorBundle's.)
        return subprocess.run(['bash', str(HERE / 'stage.sh'), '--out', out, *(more or ('--no-editor',))], cwd=cwd, capture_output=True,
                              text=True, env={**os.environ, 'RATW_EDITOR_RELEASE_KEY': '/nonexistent/release-key.pem'})

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


class Folders(unittest.TestCase):
    """The entrypoint's state, the admin volume and manage/, in a temporary folder (as this user, not root)."""

    def setUp(self):
        self.folder = tempfile.TemporaryDirectory(prefix='s2stage-m2b-')
        self.dir = Path(self.folder.name)
        names = ('STATE', 'LOGS', 'ADMIN', 'MANAGE', 'BACKUP_REQUESTS', 'PACKAGE')
        self.saved = {n: getattr(E, n) for n in names}
        E.STATE, E.LOGS, E.ADMIN = self.dir / 'state', self.dir / 'state' / 'logs', self.dir / 'admin'
        E.MANAGE, E.BACKUP_REQUESTS, E.PACKAGE = self.dir / 'manage', self.dir / 'backup-requests', self.dir / 'package'
        E.MANAGE.mkdir()
        with contextlib.redirect_stdout(io.StringIO()):
            E.prepare_state()
            E.prepare_admin()

    def tearDown(self):
        for name, value in self.saved.items():
            setattr(E, name, value)
        self.folder.cleanup()

    def quiet(self, work, *args):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            result = work(*args)
        self.output = out.getvalue()
        return result


class Files(Folders):
    def test_logs_are_sticky_and_the_minds_ledger_is_the_minds_alone(self):
        """Doc 78, 6.5 and 7: the server's user can write logs/, but can't rename the others' files there (sticky),
        nor reach the Mind's call ledger, which the AI budget is counted from."""
        self.assertEqual(stat.S_IMODE(E.LOGS.stat().st_mode), 0o1770)
        self.assertEqual(stat.S_IMODE(E.mind_dir().stat().st_mode), 0o700)
        self.assertEqual(E.ledger_path().parent, E.mind_dir())
        self.assertTrue((E.LOGS / 'npc-voices.jsonl').is_file())
        seen = []

        class Popen:
            def __init__(self, args, **kw):
                seen.append(args)
        with mock.patch.object(E.subprocess, 'Popen', Popen):
            E.start_mind(self.dir / 'npc-ai.json')
        args = seen[0]
        self.assertEqual(args[args.index('--ledger') + 1], str(E.LOGS / 'mind' / 'npc-mind-calls.jsonl'))

    def test_root_never_follows_a_link_a_child_left(self):
        """A link where root writes (the admin volume is the helper's; logs/ the server's too) is replaced or moved
        aside, never written through."""
        target = self.dir / 'target'
        target.write_text('precious')
        (E.ADMIN / 'settings.json').symlink_to(target)
        E.write_file(E.ADMIN / 'settings.json', '{}', None, 0o644)
        self.assertEqual(target.read_text(), 'precious')
        self.assertFalse((E.ADMIN / 'settings.json').is_symlink())
        self.assertEqual(stat.S_IMODE((E.ADMIN / 'settings.json').stat().st_mode), 0o644)
        voices = E.LOGS / 'npc-voices.jsonl'
        voices.unlink()
        voices.symlink_to(target)
        (E.LOGS / 'world-agent.log').symlink_to(target)
        (E.LOGS / 'mind').rename(E.LOGS / 'elsewhere')
        (E.LOGS / 'mind').symlink_to(self.dir)
        self.quiet(E.prepare_state)
        with E.append_log(E.LOGS / 'world-agent.log') as log:
            log.write('hello\n')
        self.assertEqual(target.read_text(), 'precious')
        self.assertTrue(voices.is_file() and not voices.is_symlink())
        self.assertTrue(E.mind_dir().is_dir() and not E.mind_dir().is_symlink())
        self.assertEqual(len(list(E.LOGS.glob('*.moved-*'))), 3, list(E.LOGS.iterdir()))
        self.assertIn('moved aside', self.output)

    def test_a_fifo_or_a_link_raced_into_logs_neither_hangs_nor_stops_root(self):
        """R1 review: a FIFO put where a log goes hung root's loop on its open; a link raced in after the look stopped
        the entrypoint."""
        fifo = E.LOGS / 'world-agent.log'
        fifo.unlink(missing_ok=True)
        os.mkfifo(fifo)
        with self.quiet_log() as log:
            log.write('hello\n')
        self.assertTrue(fifo.is_file() and not fifo.is_symlink(), 'a plain file in its place')
        self.assertTrue(any(p.name.startswith('world-agent.log.moved-') for p in E.LOGS.iterdir()))
        target = self.dir / 'precious'
        target.write_text('precious')
        voices = E.LOGS / 'npc-voices.jsonl'
        real_lstat = os.lstat

        raced = []

        def racing(path, *a, **kw):           # The look sees a plain file; a link is there by the open.
            if os.fspath(path) == os.fspath(voices) and not raced:
                raced.append(path)
                os.unlink(voices)
                os.symlink(target, voices)
                return real_lstat(target)
            return real_lstat(path, *a, **kw)
        with mock.patch.object(E.os, 'lstat', side_effect=racing):
            self.quiet(E.own_file, voices, 'ratw', 0o640)
        self.assertEqual(target.read_text(), 'precious')
        self.assertTrue(voices.is_file() and not voices.is_symlink())

    @contextlib.contextmanager
    def quiet_log(self):
        with contextlib.redirect_stdout(io.StringIO()), E.append_log(E.LOGS / 'world-agent.log') as log:
            yield log

    def test_the_helpers_settings_hold_no_secret(self):
        """settings.json (doc 78, 6.5): a cleaned copy of world.toml, without the token, the AI key or file names."""
        s = {'name': 'Wolf Hollow', 'world_id': WORLD, 'description': '', 'region': '', 'tags': ['a'], 'player_limit': 40,
             'arrivals_per_hour': None, 'cheats': 'off', 'central': 'https://c', 'host': '', 'secure': False,
             'token': TOKEN, 'token_file': 'host-token', 'workers': None, 'upnp': True, 'ai_key': 'sk-secret',
             'ai_key_file': 'ai-key', 'models': {'model': 'm'}, 'cost_mode': 'balanced', 'monthly_tokens': 5000,
             'editor': {'enabled': True, 'owner': '', 'new_devices': 'open'}}
        with mock.patch.dict(os.environ, {'RATW_HOST_FOLDER': '/home/host/my-world'}):
            E.write_admin_settings(s, 17790)
        body = (E.ADMIN / 'settings.json').read_text()
        for secret in (TOKEN, 'sk-secret', 'host-token', 'ai-key'):
            self.assertNotIn(secret, body)
        values = json.loads(body)
        self.assertEqual((values['name'], values['editor']['new_devices'], values['editor_port'], values['has_token'],
                          values['ai']['key']), ('Wolf Hollow', 'open', 17790, True, True))
        # (What the hosted studio reads, doc 78 M3: name, package, player_limit, workers, cheats, editor,
        # ai.monthly_tokens, folder.)
        self.assertEqual((values['ai']['monthly_tokens'], values['folder'], values['player_limit'], values['cheats']),
                         (5000, '/home/host/my-world', 40, 'off'))
        self.assertIn('package', values)
        self.assertIn('workers', values)
        with mock.patch.dict(os.environ, {'RATW_HOST_FOLDER': ''}), \
                mock.patch.object(E.Path, 'read_text', return_value=(
                    f'1 2 0:1 /run/desktop/mnt/host/c/RATW/my\\040world {E.SETTINGS.parent} ro - x y z\n')):
            self.assertEqual(E.host_folder(), 'C:\\RATW\\my world')
        self.assertEqual(stat.S_IMODE((E.ADMIN / 'settings.json').stat().st_mode), 0o644)

    def test_the_registry_and_the_ai_config_have_the_helpers_own_copies(self):
        self.quiet(E.write_registry, 'Wolf Hollow')
        for folder in (E.STATE / 'config', E.ADMIN):
            self.assertEqual(json.loads((folder / 'worlds.json').read_text())['worlds'][0]['key'], 'world')
        s = {'ai_key': 'sk-abc', 'models': dict(E.DEFAULT_MODELS), 'cost_mode': 'balanced'}
        E.mind_config(s)
        copy = E.ADMIN / 'npc-ai.json'
        self.assertEqual(json.loads(copy.read_text())['api_key'], 'sk-abc')
        self.assertEqual(stat.S_IMODE(copy.stat().st_mode), 0o600)
        self.assertEqual(E.admin_env(17789)['RATW_AI_CONFIG'], str(copy))
        E.mind_config({**s, 'ai_key': ''})
        self.assertFalse(copy.exists())
        self.assertNotIn('RATW_AI_CONFIG', E.admin_env(17789))

    def test_the_helper_gets_the_dm_password_and_nothing_else_secret(self):
        saved = dict(os.environ)
        try:
            os.environ.update({k: 'secret' for k in E.SECRET_ENV})
            env = E.admin_env(17790)
            self.assertEqual(sorted(k for k in env if k in E.SECRET_ENV), ['RATW_DM_PASSWORD'])
            self.assertEqual((env['RATW_ADMIN_DIR'], env['RATW_WORLDS_FILE'], env['RATW_EDITOR_PORT'], env['HOME']),
                             (str(E.ADMIN), str(E.ADMIN / 'worlds.json'), '17790', str(E.ADMIN)))
        finally:
            os.environ.clear()
            os.environ.update(saved)


class Editor(Folders):
    def test_first_editor_txt_says_how_to_sign_in_and_is_emptied_on_use(self):
        made = {'username': 'host-k7q2', 'password': 'p' * 20, 'invite': 'i', 'code': 'ratw-editor:abc', 'days': 7,
                'renewed': False}
        body = E.first_editor_text(made, 17789, now=0)
        for words in ('Your Editor account: host-k7q2', 'One-time password: ' + 'p' * 20, 'press EDITOR',
                      'ratw-editor:abc', 'address 127.0.0.1 with', 'until 1970-01-08'):
            self.assertIn(words, body)
        self.assertNotIn('A new one-time password', body)
        renewed = E.first_editor_text({**made, 'renewed': True}, 17790, now=0)
        self.assertIn('A new one-time password', renewed)
        self.assertIn('127.0.0.1:17790', renewed)
        self.assertTrue(E.write_manage('first-editor.txt', body))
        self.assertEqual(stat.S_IMODE((E.MANAGE / 'first-editor.txt').stat().st_mode), 0o600)
        (E.ADMIN / 'requests' / 'first-editor-used.0123456789ab').write_text('first-editor-used\n')
        world = E.Supervisor(7788, 17789, self.dir, self.dir)
        for kind in self.quiet(E.take_requests):
            self.quiet(world.act, kind)
        self.assertTrue((E.MANAGE / 'first-editor.txt').read_text().startswith('Used. Lost your password?'))
        self.assertNotIn('p' * 20, (E.MANAGE / 'first-editor.txt').read_text())

    def test_the_first_owner_is_logged_by_its_file_never_its_password(self):
        made = {'username': 'host-k7q2', 'password': 'secretpassword20char', 'invite': 'i', 'code': 'ratw-editor:x',
                'days': 7, 'renewed': False}
        reply = subprocess.CompletedProcess([], 0, json.dumps(made) + '\n', '')
        with mock.patch.object(E, 'admin_tool', return_value=reply):
            self.quiet(E.first_owner, 17789)
        self.assertIn('manage/first-editor.txt', self.output)
        self.assertNotIn('secretpassword20char', self.output)
        self.assertIn('secretpassword20char', (E.MANAGE / 'first-editor.txt').read_text())
        with mock.patch.object(E, 'admin_tool', return_value=reply), mock.patch.object(E, 'manage_ready',
                                                                                         return_value=False):
            self.quiet(E.first_owner, 17789)                    # (An old compose.yaml: no manage folder.)
        self.assertIn('new compose.yaml', self.output)
        self.assertIn('ratw-editor reset host-k7q2', self.output)
        self.assertNotIn('secretpassword20char', self.output)
        with mock.patch.object(E, 'admin_tool', return_value=subprocess.CompletedProcess([], 0, 'null\n', '')):
            self.quiet(E.first_owner, 17789)                    # (An owner already: nothing written.)
        self.assertEqual(self.output, '')

    def test_requests_are_kinds_from_names_alone(self):
        """Doc 78, 6.5 and I7: a request names a kind, never a path or a command; odd files are removed, links never
        followed, the helper's half-written files left."""
        requests = E.ADMIN / 'requests'
        for name in ('restart.0123456789ab', 'restart.ba9876543210', 'backup-by-hand.0123456789ab',
                     'share.0123456789ab', 'shutdown.0123456789ab', 'restart', 'restart.0123456789ab.sh'):
            (requests / name).write_text('rm -rf /\n')
        (requests / 'first-editor-used.0123456789ab').symlink_to(self.dir / 'nowhere')
        (requests / '.restart.0123456789ab.1234').write_text('restart\n')
        kinds = self.quiet(E.take_requests)
        self.assertEqual(kinds, ['backup-by-hand', 'restart', 'share'])
        self.assertIn('4 odd file(s)', self.output)
        self.assertEqual([p.name for p in requests.iterdir()], ['.restart.0123456789ab.1234'])
        # R1 review: 64 half-written (dot) files took the tick's whole share, and no real request was ever taken.
        for i in range(80):
            (requests / f'.restart.{i:012x}.1234').write_text('')
        (requests / 'restart.0123456789ab').write_text('')
        self.assertEqual(self.quiet(E.take_requests), ['restart'])

    def test_a_backup_request_goes_to_the_backup_service_once(self):
        self.assertTrue(self.quiet(E.pass_backup, 'backup-by-hand'))
        self.assertFalse(self.quiet(E.pass_backup, 'backup-by-hand'))
        self.assertIn('already waiting', self.output)
        self.assertTrue(self.quiet(E.pass_backup, 'backup-before-publish'))
        waiting = sorted(p.name.split('.')[0] for p in E.BACKUP_REQUESTS.iterdir())
        self.assertEqual(waiting, ['backup-before-publish', 'backup-by-hand'])
        self.assertEqual((next(E.BACKUP_REQUESTS.glob('backup-by-hand.*'))).read_text(), 'backup-by-hand\n')

    def test_central_naming_the_world_official_turns_the_editor_off_for_good(self):
        """Doc 78, 6.7: central's word is copied into the admin volume; `ever-official` is never cleared; the helper,
        the certificate and the owner are never made on an official world."""
        central = E.private_dir(E.STATE / 'central')
        self.assertEqual(self.quiet(E.mirror_central), {})
        (central / 'central.json').write_text(json.dumps({'at': 1, 'kind': 'community', 'host': 'acct-1'}))
        self.assertEqual(self.quiet(E.mirror_central)['kind'], 'community')
        self.assertEqual(json.loads((E.ADMIN / 'central.json').read_text())['host'], 'acct-1')
        self.assertFalse(E.official())
        (central / 'central.json').write_text(json.dumps({'at': 2, 'kind': 'official', 'host': None}))
        self.quiet(E.mirror_central)
        self.assertTrue((E.ADMIN / 'ever-official').exists())
        self.assertIn('official', self.output)
        (central / 'central.json').write_text(json.dumps({'at': 3, 'kind': 'community', 'host': 'acct-1'}))
        self.quiet(E.mirror_central)
        self.assertTrue(E.official(), 'never cleared')
        # R1 review: the helper owns the admin volume, so its copy of the mark may go; root's own (in the requests
        # volume, root's alone) keeps the world official, and the helper's is made again.
        (E.ADMIN / 'ever-official').unlink()
        self.assertTrue(E.official(), "root's own mark")
        self.quiet(E.mirror_central)
        self.assertTrue((E.ADMIN / 'ever-official').exists(), 'made again')
        # A directory the helper left where central.json goes: moved aside, never a crash.
        (E.ADMIN / 'central.json').unlink()
        (E.ADMIN / 'central.json').mkdir()
        (central / 'central.json').write_text(json.dumps({'at': 4, 'kind': 'community', 'host': 'acct-2'}))
        self.assertEqual(self.quiet(E.mirror_central, {})['host'], 'acct-2')
        self.assertEqual(json.loads((E.ADMIN / 'central.json').read_text())['host'], 'acct-2')
        (central / 'central.json').unlink()
        (central / 'central.json').symlink_to(E.ADMIN / 'settings.json')
        self.assertEqual(E.central_word(), {}, 'a link is never read')
        made = []
        with mock.patch.object(E, 'certificate', side_effect=lambda *a: made.append(a)), \
                mock.patch.object(E, 'start_editor', side_effect=lambda *a: made.append(a)):
            world = E.Supervisor(7788, 17789, self.dir, central)
            self.quiet(world.apply_editor, {'editor': {'enabled': True, 'owner': '', 'new_devices': 'open'}})
        self.assertEqual(made, [])
        self.assertIsNone(world.editor)
        self.assertIn('official', self.output)

    def test_the_helper_starts_again_when_editor_changes(self):
        """Doc 78, 6.5: the entrypoint restarts the helper when [editor] changes; enabled = false: nothing listens."""
        started = []

        class Process:
            def __init__(self):
                self.code = None
                started.append(self)

            def poll(self):
                return self.code

            def send_signal(self, number):
                self.code = -number

            def wait(self, timeout=None):
                return self.code
        owners = []
        with mock.patch.object(E, 'certificate', return_value='AAAA'), \
                mock.patch.object(E, 'start_editor', side_effect=lambda port: Process()), \
                mock.patch.object(E, 'first_owner', side_effect=owners.append):
            world = E.Supervisor(7788, 17789, self.dir, self.dir)
            editor = {'enabled': True, 'owner': '', 'new_devices': 'open'}
            self.quiet(world.apply_editor, {'editor': editor})
            self.assertEqual((len(started), owners), (1, []), 'the database not ready yet: no owner made')
            world.database_ready = True
            self.quiet(world.apply_editor, {'editor': dict(editor)})
            self.assertEqual(len(started), 1, 'the same settings: left running')
            self.quiet(world.apply_editor, {'editor': {**editor, 'new_devices': 'approve'}})
            self.assertEqual((len(started), started[0].code, owners), (2, -signal.SIGTERM, [17789]))
            self.quiet(world.apply_editor, {'editor': {**editor, 'enabled': False}})
            self.assertEqual((len(started), started[1].code, world.editor), (2, -signal.SIGTERM, None))
            self.assertIn('nothing answers on the Editor port', self.output)
            self.quiet(world.apply_editor, {'editor': {**editor, 'enabled': False}})
            self.assertEqual(self.output, '', 'said once')

    def test_the_minds_calls_are_counted_by_month(self):
        """The Mind's monthly use, from its ledger into the admin volume (doc 78, 7), read on from where it stopped."""
        ledger = E.ledger_path()
        now = 1_800_000_000                        # (2027-01-15.)
        lines = [{'t': now, 'model': 'gpt-5.4-mini', 'outcome': 'success', 'prompt_tokens': 100,
                  'completion_tokens': 20, 'cached_tokens': 50},
                 {'t': now - 40 * 86400, 'model': 'gpt-5.4-mini', 'outcome': 'success', 'prompt_tokens': 999},
                 {'t': now, 'model': 'gpt-5.4-nano', 'outcome': 'provider_timeout'}, 'junk', {'t': True}]
        ledger.write_text(''.join((json.dumps(x) if isinstance(x, dict) else x) + '\n' for x in lines) + '{"t": ')
        use = E.MindUse()
        counts = use.update(now)
        self.assertEqual((counts['month'], counts['calls'], counts['failed'], counts['prompt_tokens'],
                          counts['completion_tokens'], counts['cached_tokens']), ('2027-01', 2, 1, 100, 20, 50))
        self.assertEqual({k: v for k, v in json.loads((E.ADMIN / 'mind-month.json').read_text()).items()
                          if k in ('month', 'tokens', 'calls')}, {'month': '2027-01', 'tokens': 120, 'calls': 2})
        with ledger.open('a') as f:                  # (The half line, finished, and one more.)
            f.write(f'{now}, "model": "gpt-5.4-nano", "outcome": "success", "prompt_tokens": 5}}\n')
        self.assertEqual(use.update(now)['calls'], 3)
        self.assertEqual(use.update(now + 20 * 86400)['calls'], 0, 'a new month starts at 0')


class Loop(Folders):
    """The server's loop (doc 78, 12.4) with a fake server and clock: helpers kept up on any exit, backoff, the 5 s
    maintenance poll, restarts asked for by the Editor, world.toml read before each start."""

    class Server:
        def __init__(self):
            self.code, self.signals = None, []

        def poll(self):
            return self.code

        @property
        def returncode(self):
            return self.code

        def send_signal(self, number):
            self.signals.append(number)

        def kill(self):
            self.signals.append(signal.SIGKILL)

    def setUp(self):
        super().setUp()
        self.now = 1000.0
        self.world = E.Supervisor(7788, 17789, self.dir, self.dir, clock=lambda: self.now)
        self.world.mind_use.update = lambda *a: {}
        self.servers, self.checks, self.state, self.settings = [], [], 'maintenance', {'name': 'Wolf Hollow'}

        class Watched:
            def check(inner):
                self.checks.append(self.now)
        self.world.agent = Watched()

        def start():
            self.world.server = self.Server()
            self.world.started_at = self.now
            self.servers.append(self.world.server)
        self.patches = [mock.patch.object(self.world, 'start_server', side_effect=start),
                        mock.patch.object(self.world, 'apply', side_effect=lambda s: None),
                        mock.patch.object(E, 'reread_settings', side_effect=lambda: self.settings),
                        mock.patch.object(E, 'world_state', side_effect=lambda: self.state),
                        mock.patch.object(E, 'mirror_central', return_value={})]
        for p in self.patches:
            p.start()

    def tearDown(self):
        for p in self.patches:
            p.stop()
        super().tearDown()

    def run_until(self, seconds: float) -> None:
        end, said = self.now + seconds, ''
        while self.now < end:
            self.quiet(self.world.step)
            said += self.output
            self.now += 0.5
        self.output = said

    def test_a_crash_keeps_the_helpers_up_and_backs_off(self):
        self.run_until(1)
        self.assertEqual(len(self.servers), 1)
        waits = []
        for _ in range(5):
            self.servers[-1].code = -11                  # (A crash.)
            stopped, count = self.now, len(self.servers)
            checks = len(self.checks)
            while len(self.servers) == count:
                self.quiet(self.world.step)
                self.now += 0.5
            waits.append(round(self.now - stopped - 0.5))
            self.assertGreater(len(self.checks) - checks, 4, 'the helpers are checked while the server is down')
        self.assertEqual(waits, [5, 30, 120, 600, 600])
        exits = json.loads((E.ADMIN / 'exits.json').read_text())['exits']
        self.assertEqual([e['code'] for e in exits], [-11] * 5, 'each exit kept for the owner (exits.json)')
        self.assertTrue(all(isinstance(e['at'], float) and e['lines'] == [] for e in exits))
        self.now += E.STABLE                             # (A server that ran 10 minutes starts afresh.)
        self.servers[-1].code = 1
        self.quiet(self.world.step)
        self.assertEqual(self.world.next_start - self.now, 5)
        self.assertIn('The Editor stays up', self.output)

    def test_a_release_starts_again_at_once(self):
        self.run_until(1)
        self.servers[-1].code = E.RELEASE_EXIT
        self.run_until(1)
        self.assertEqual(len(self.servers), 2)

    def test_maintenance_polls_every_five_seconds(self):
        self.run_until(1)
        polls = []
        with mock.patch.object(E, 'world_state', side_effect=lambda: polls.append(self.now) or self.state):
            self.servers[-1].code = E.MAINTENANCE_EXIT
            self.run_until(30)
            self.assertEqual(len(self.servers), 1)
            self.assertEqual([round(b - a) for a, b in zip(polls, polls[1:])], [5] * (len(polls) - 1))
            self.assertGreaterEqual(len(polls), 5)
            self.state = 'running'
            self.run_until(6)
        self.assertEqual(len(self.servers), 2)
        self.assertIn('open again', self.output)

    def test_a_restart_asked_for_stops_the_server_and_starts_it_again(self):
        self.run_until(1)
        (E.ADMIN / 'requests' / 'restart.0123456789ab').write_text('restart\n')
        self.run_until(1)
        self.assertEqual(self.servers[-1].signals, [signal.SIGTERM])
        self.servers[-1].code = -signal.SIGTERM
        self.run_until(1)
        self.assertEqual(len(self.servers), 2)
        self.servers[-1].code = 3                           # (A crash, then a restart asked for in the wait.)
        self.run_until(1)
        (E.ADMIN / 'requests' / 'restart.0123456789ab').write_text('restart\n')
        self.run_until(1)
        self.assertEqual(len(self.servers), 3)
        (E.ADMIN / 'requests' / 'restart.0123456789ab').write_text('restart\n')
        self.run_until(1)
        self.run_until(E.STOP_WAIT + 1)                      # (A server that won't stop is stopped hard.)
        self.assertEqual(self.servers[-1].signals, [signal.SIGTERM, signal.SIGKILL])

    def test_world_toml_is_read_before_each_start(self):
        self.settings = None                               # (Broken: no start, the loop waits and reads it again.)
        self.run_until(3)
        self.assertEqual(self.servers, [])
        self.settings = {'name': 'Wolf Hollow'}
        self.run_until(31)
        self.assertEqual(len(self.servers), 1)

    def test_a_stop_ends_the_loop_once_the_server_has_saved(self):
        self.run_until(1)
        try:
            E.stopping = True
            self.assertTrue(self.quiet(self.world.step))
            self.assertEqual(self.servers[-1].signals, [signal.SIGTERM])
            self.servers[-1].code = 0
            self.assertFalse(self.quiet(self.world.step))
        finally:
            E.stopping, E.forwarded = False, False


class HostFolder(unittest.TestCase):
    def test_the_folder_ships_empty_manage_and_backups(self):
        """Doc 78, 6.3: so Docker doesn't make them itself (root's, on Linux)."""
        spec_ = importlib.util.spec_from_file_location('host_folder', HERE / 'host_folder.py')
        H = importlib.util.module_from_spec(spec_)
        spec_.loader.exec_module(H)
        with tempfile.TemporaryDirectory(prefix='s2stage-m2b-') as folder:
            out = Path(folder) / 'my-world'
            H.make(out)
            self.assertEqual(sorted(p.name for p in out.iterdir()),
                             ['Hosting.md', 'README.md', 'backups', 'compose.yaml', 'manage', 'world.toml'])
            self.assertEqual(list((out / 'manage').iterdir()) + list((out / 'backups').iterdir()), [])
            H.zip_folder(out, Path(folder) / 'my-world.zip')
            with zipfile.ZipFile(Path(folder) / 'my-world.zip') as archive:
                names = archive.namelist()
            self.assertIn('my-world/manage/', names)
            self.assertIn('my-world/backups/', names)
            self.assertIn('my-world/compose.yaml', names)
            with self.assertRaises(SystemExit):
                H.make(out)                                  # (Never into a folder that holds something.)


@unittest.skipUnless(shutil.which('openssl'), 'needs openssl')
class EditorBundle(unittest.TestCase):
    """stage.sh's signed Editor bundle (Docs/Design/78, 6.6, Q6): every file and its SHA-256, signed with the release key
    (Ed25519), which never goes in the repository; refused without a key."""

    def run_stage(self, *args: str, key: str = '/nonexistent/release-key.pem') -> subprocess.CompletedProcess:
        return subprocess.run(['bash', str(HERE / 'stage.sh'), *args], cwd=ROOT, capture_output=True, text=True,
                              env={**os.environ, 'RATW_EDITOR_RELEASE_KEY': key, 'RATW_BUNDLE_VERSION': '1794500000'})

    def test_the_editor_is_staged_signed_file_by_file(self):
        with tempfile.TemporaryDirectory(prefix='s2stage-bundle-') as folder:
            folder = Path(folder)
            made = self.run_stage('--make-release-key', str(folder / 'keys' / 'release.pem'))
            self.assertEqual(made.returncode, 0, made.stderr)
            self.assertRegex(made.stdout, r'^    \("ratw-[0-9a-f]{16}", "[A-Za-z0-9+/]{43}="\),$')
            key = folder / 'keys' / 'release.pem'
            self.assertEqual(key.stat().st_mode & 0o777, 0o600)
            self.assertEqual(self.run_stage('--make-release-key', str(key)).returncode, 2, 'never replaces a release key')
            dist = folder / 'dist'
            (dist / 'assets').mkdir(parents=True)
            (dist / 'index.html').write_text('<!doctype html><script src="assets/a.js"></script>')
            (dist / 'assets' / 'a.js').write_text('run()')
            out = folder / 'context'
            result = self.run_stage('--out', str(out), '--editor-dist', str(dist), key=str(key))
            self.assertEqual(result.returncode, 0, result.stderr)
            staged = out / 'app' / 'Editor' / 'dist'
            bundle = json.loads((staged / 'bundle.json').read_text())
            manifest = base64.b64decode(bundle['manifest'])
            (folder / 'manifest').write_bytes(manifest)
            (folder / 'signature').write_bytes(base64.b64decode(bundle['signature']))
            subprocess.run(['openssl', 'pkey', '-in', str(key), '-pubout', '-out', str(folder / 'public.pem')], check=True)
            verified = subprocess.run(['openssl', 'pkeyutl', '-verify', '-pubin', '-inkey', str(folder / 'public.pem'), '-rawin',
                                       '-in', str(folder / 'manifest'), '-sigfile', str(folder / 'signature')], capture_output=True)
            self.assertEqual(verified.returncode, 0, verified.stderr)
            listed = json.loads(manifest)
            self.assertEqual((listed['v'], listed['version'], listed['editorApi']), (1, 1794500000, 1))
            on_disk = {p.relative_to(staged).as_posix() for p in staged.rglob('*') if p.is_file() and p.name != 'bundle.json'}
            self.assertEqual(set(listed['files']), on_disk, 'every file, and only the files')
            for name, digest in listed['files'].items():
                self.assertEqual(hashlib.sha256((staged / name).read_bytes()).hexdigest(), digest, name)
            portraits = sorted(p.name for p in (ROOT / 'Data' / 'Portraits').glob('*.png'))
            self.assertEqual(sorted(n.split('/')[1] for n in listed['files'] if n.startswith('portraits/')), portraits)
            self.assertEqual(sorted(p.name for p in (out / 'app' / 'Data' / 'Portraits').glob('*.png')), portraits,
                             'beside the studio, where it serves /portraits/')
            self.assertFalse(any(out.rglob('*.pem')), 'no key in the image')

    def test_without_a_key_nothing_is_staged(self):
        with tempfile.TemporaryDirectory(prefix='s2stage-bundle-') as folder:
            out = Path(folder) / 'context'
            result = self.run_stage('--out', str(out))
            self.assertEqual(result.returncode, 2)
            self.assertIn('no Editor release key', result.stderr)
            self.assertFalse(out.exists())

    def test_the_release_key_never_goes_in_the_repository(self):
        inside = ROOT / 'build-package' / 's2stage-key.pem'
        result = self.run_stage('--make-release-key', str(inside))
        self.assertEqual(result.returncode, 2)
        self.assertIn('never goes in the repository', result.stderr)
        self.assertFalse(inside.exists())
        # Nor is one there used (R1 review): staging and signing refuse it.
        with tempfile.TemporaryDirectory(prefix='s2stage-bundle-') as folder:
            made = self.run_stage('--make-release-key', str(Path(folder) / 'release.pem'))
            self.assertEqual(made.returncode, 0, made.stderr)
            made_parent = not inside.parent.exists()
            inside.parent.mkdir(exist_ok=True)
            shutil.copy(Path(folder) / 'release.pem', inside)
            try:
                dist = Path(folder) / 'dist'
                dist.mkdir()
                (dist / 'index.html').write_text('<!doctype html>')
                for args in (('--out', str(Path(folder) / 'context'), '--editor-dist', str(dist)), ('--sign-bundle', str(dist))):
                    result = self.run_stage(*args, key=str(inside))
                    self.assertEqual(result.returncode, 2, args)
                    self.assertIn('never goes in the repository', result.stderr)
                self.assertFalse((Path(folder) / 'context').exists())
                self.assertFalse((dist / 'bundle.json').exists())
            finally:
                inside.unlink(missing_ok=True)
                if made_parent:
                    inside.parent.rmdir()

    def test_a_link_in_the_editor_or_the_world_is_refused(self):
        """R1 review: cp -r kept symbolic links, and the bundle's list hashed what one pointed at on this computer."""
        with tempfile.TemporaryDirectory(prefix='s2stage-bundle-') as folder:
            folder = Path(folder)
            self.assertEqual(self.run_stage('--make-release-key', str(folder / 'release.pem')).returncode, 0)
            dist = folder / 'dist'
            dist.mkdir()
            (dist / 'index.html').write_text('<!doctype html>')
            (dist / 'secrets.txt').symlink_to('/etc/hostname')
            result = self.run_stage('--out', str(folder / 'context'), '--editor-dist', str(dist), key=str(folder / 'release.pem'))
            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertIn('symbolic link', result.stderr)
            self.assertFalse((folder / 'context').exists())
            world = folder / 'world'
            world.mkdir()
            (world / 'package.json').write_text('{}')
            (world / 'x').symlink_to('/etc/hostname')
            result = self.run_stage('--out', str(folder / 'context'), '--world', str(world), '--no-editor')
            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertIn('symbolic link', result.stderr)


class Compose(unittest.TestCase):
    def test_the_editor_is_published_to_this_computer_alone_by_default(self):
        """Doc 78, 3.2 and Q1 (D1: IPv4; 0.0.0.0 opens it); the admin and requests volumes; ./manage; RATW_EDITOR_PORT
        passed in for the files."""
        compose = (HERE / 'compose.yaml').read_text()
        world = compose[compose.index('  world:'):compose.index('  upnp:')]
        self.assertIn('- "${RATW_EDITOR_BIND:-127.0.0.1}:${RATW_EDITOR_PORT:-17789}:17789"', world)
        self.assertIn('RATW_EDITOR_PORT: ${RATW_EDITOR_PORT:-17789}', world)
        for mount in ('- admin:/var/lib/ratw/admin', '- requests:/var/lib/ratw/backup-requests',
                      '- ./manage:/var/ratw/manage'):
            self.assertIn(mount, world)
        self.assertIn('\n  admin:\n  requests:\n', compose[compose.index('\nvolumes:'):])
        self.assertEqual(str(E.ADMIN), '/var/lib/ratw/admin')
        self.assertEqual(str(E.MANAGE), '/var/ratw/manage')
        self.assertEqual(str(E.BACKUP_REQUESTS), '/var/lib/ratw/backup-requests')

    def test_every_service_has_log_limits(self):
        compose = (HERE / 'compose.yaml').read_text()
        self.assertIn('x-logging: &logging\n  driver: json-file\n  options:\n    max-size: 10m\n    max-file: "5"\n',
                      compose)
        services = compose[compose.index('\nservices:'):compose.index('\nvolumes:')]
        names = re.findall(r'^  ([a-z]+):$', services, re.M)
        self.assertEqual(names, ['world', 'upnp', 'db'])
        self.assertEqual(services.count('    logging: *logging\n'), len(names))

    def test_the_image_has_the_editors_user_and_folders(self):
        docker = (HERE / 'Dockerfile').read_text()
        self.assertRegex(docker, r'apt-get install[^&]*\bopenssl\b')
        self.assertIn('useradd --uid 10003 --gid 10003 ', docker)
        self.assertNotRegex(docker, r'useradd[^\n]*(-G|--groups)')          # (Every user in its own group alone.)
        self.assertIn('install -d -o ratw-admin -g ratw-admin -m 0700 /var/lib/ratw/admin', docker)
        self.assertIn('install -d -o ratw -g ratw-server -m 1770 /var/lib/ratw/logs', docker)
        self.assertIn('EXPOSE 7788 17789', docker)

    def test_the_image_ships_the_signed_editor_where_the_hosted_studio_serves_it(self):
        """Doc 78, M4 and M3: stage.sh stages the signed Editor in app/Editor/dist (bundle.json and portraits/ inside)
        and Data/Portraits beside it; the Dockerfile copies app/ to /opt/ratw/, so they are /opt/ratw/Editor/dist and
        /opt/ratw/Data/Portraits, which the hosted studio (in /opt/ratw/tools, the helper's) serves behind a session."""
        docker = (HERE / 'Dockerfile').read_text()
        self.assertIn('\nCOPY app/ /opt/ratw/\n', docker)
        stage = (HERE / 'stage.sh').read_text()
        for staged in ('mkdir -p "$out/app/Editor/dist" "$out/app/Data/Portraits"', 'sign_bundle "$out/app/Editor/dist"',
                       'mkdir -p "$out/app/Editor/dist/portraits"', 'xargs -0 cp -t "$out/app/tools"'):
            self.assertIn(staged, stage)
        studio_files = (ROOT / 'tools' / 'map_editor.py').read_text()     # (tools/ is /opt/ratw/tools: ROOT its parent.)
        for served in ("ROOT = Path(__file__).resolve().parent.parent", "DIST = ROOT / 'Editor' / 'dist'",
                       "PORTRAITS = ROOT / 'Data' / 'Portraits'"):
            self.assertIn(served, studio_files)
        # The helper hands the page's files to the hosted studio only with a session (world_admin.Core.app; M2a).
        self.assertIn("core.app = hosted_studio(folder)", (ROOT / 'tools' / 'world_admin.py').read_text())

    def test_the_readme_says_how_to_reach_the_editor(self):
        readme = (HERE / 'README.md').read_text()
        for words in ('first-editor.txt', 'RATW_EDITOR_BIND=0.0.0.0', 'ports are not available',
                      'RATW_EDITOR_PORT=17800', 'RATW_EDITOR_PORT=17790', 'ufw'):
            self.assertIn(words, readme)

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
