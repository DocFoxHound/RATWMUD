#!/usr/bin/env python3
"""The Unreal save converter, on made-up saves in a temporary folder."""
import json
from pathlib import Path
import sqlite3
import stat
import tempfile
import unittest
from unittest import mock

import convert_saves


def unreal_save(path: Path, payload, version=1):
    path.parent.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(path) as database:
        database.execute('CREATE TABLE world_state (id INTEGER PRIMARY KEY CHECK(id=1), schema_version INTEGER NOT NULL, '
                         'revision INTEGER NOT NULL, payload TEXT NOT NULL)')
        if payload is not None:
            database.execute('INSERT INTO world_state VALUES (1, ?, 3, ?)', (version, json.dumps(payload)))


class Converter(unittest.TestCase):
    def run_in(self, saved: Path, *extra):
        with mock.patch('sys.argv', ['convert_saves.py', '--saved', str(saved), *extra]):
            return convert_saves.main()

    def test_every_kind_of_save_is_converted_privately_and_the_old_one_kept(self):
        with tempfile.TemporaryDirectory() as folder:
            saved = Path(folder)
            unreal_save(saved / 'ratw-world.sqlite', {'schema': 1, 'players': [{'id': 'player-ash'}]})
            unreal_save(saved / 'ratw-town.sqlite', {'schema': 1, 'town': True})
            unreal_save(saved / 'Atlas' / 'abc123' / 'ratw-world.sqlite', {'schema': 1, 'atlas': True})
            self.assertEqual(self.run_in(saved), 0)
            self.assertEqual(json.loads((saved / 'ratw-world.json').read_text())['players'][0]['id'], 'player-ash')
            self.assertTrue(json.loads((saved / 'ratw-town.json').read_text())['town'])
            self.assertTrue(json.loads((saved / 'Atlas' / 'abc123.json').read_text())['atlas'])
            for path in (saved / 'ratw-world.json', saved / 'Atlas' / 'abc123.json'):
                self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600, 'owner-only: account verifiers are inside')
            self.assertTrue((saved / 'ratw-world.sqlite').exists(), 'the old save is left as it was')

    def test_nothing_is_overwritten_and_empty_or_foreign_saves_are_skipped(self):
        with tempfile.TemporaryDirectory() as folder:
            saved = Path(folder)
            unreal_save(saved / 'ratw-world.sqlite', {'schema': 1, 'old': True})
            (saved / 'ratw-world.json').write_text('{"schema":1,"newer":true}')
            unreal_save(saved / 'ratw-town.sqlite', None)
            unreal_save(saved / 'Atlas' / 'v2' / 'ratw-world.sqlite', {'schema': 2}, version=2)
            self.assertEqual(self.run_in(saved), 0)
            self.assertTrue(json.loads((saved / 'ratw-world.json').read_text())['newer'], 'an existing new save wins')
            self.assertFalse((saved / 'ratw-town.json').exists(), 'an empty database makes nothing')
            self.assertFalse((saved / 'Atlas' / 'v2.json').exists(), 'an unknown schema version is not guessed at')

    def test_a_dry_run_writes_nothing(self):
        with tempfile.TemporaryDirectory() as folder:
            saved = Path(folder)
            unreal_save(saved / 'ratw-world.sqlite', {'schema': 1})
            self.assertEqual(self.run_in(saved, '--dry-run'), 0)
            self.assertFalse((saved / 'ratw-world.json').exists())


if __name__ == '__main__':
    unittest.main()
