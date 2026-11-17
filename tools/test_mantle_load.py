#!/usr/bin/env python3
"""Mantles in a world database (Docs/Design/84-living-conversations.md §17; Phase 9), each case in a throwaway database
made from the migrations (`ratw_test_mantles_<hex>`, dropped after; skipped without PostgreSQL):

  * migration 0066: the tables, the trigger (one revision a transaction, items' revisions, an unchanged save leaves all
    alone, the history), who may write what (the game reads; nobody writes the history), one wearer a mantle, a worn
    mantle never deleted, the game's binding writes in live.game_changes, dm.queue_talk_sync; its rollback, and 0066 again;
  * tools/mantle_load.py over the test fixture (Tests/fixtures/mantles): checked, written, bound, the server told, the files
    written back; the export and the export of a load of it the same bytes (§17.5); loading twice changes nothing;
    `--check` (G6) clean, then finding an edit made as Atlas would make it;
  * refusals that write nothing: a name outside a slot, a stale revision (and --take-theirs, --take-mine), a ready mantle
    under its tier's minimum, a retire while worn;
  * backup and restore.

talk_check must be built (G1 runs it): RATW_TALK_CHECK names it, else the newest build*/talk_check.
"""
import json
import secrets
import shutil
import tempfile
import unittest
from pathlib import Path

import mantle_export as E
import mantle_load as L
import mantle_store as S
import mantle_text as T
import world_db as W
from test_world_db import database_available, superuser

ROOT = Path(__file__).resolve().parent.parent
FIXTURE = ROOT / 'Tests' / 'fixtures' / 'mantles'
ROLLBACK = ROOT / 'Database' / 'rollback' / '0066_mantles_down.sql'


def insert_npc(conn, world, npc, name):
    """A live.npcs row with every column it needs."""
    cols = conn.execute("SELECT column_name, data_type, udt_name FROM information_schema.columns WHERE table_schema = 'live' AND "
                        "table_name = 'npcs' AND is_nullable = 'NO' AND column_default IS NULL").fetchall()
    vals = {'world_id': world, 'id': npc, 'name': name, 'role': 'civilian', 'age': 40, 'voice': 1, 'appearance': '{}',
            'hours_start': 8, 'hours_end': 17}
    for c, t, u in cols:
        if c not in vals:
            vals[c] = '' if t == 'text' or u == 'ratw_id' else 0 if t in ('integer', 'double precision', 'bigint') else '{}' if t == 'jsonb' else False
    conn.execute(f"INSERT INTO live.npcs ({', '.join(vals)}) VALUES ({', '.join(['%s'] * len(vals))})", list(vals.values()))


class Scratch:
    def __init__(self):
        self.name = f'ratw_test_mantles_{secrets.token_hex(4)}'

    def __enter__(self):
        with superuser() as su:
            su.execute(f'CREATE DATABASE {self.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {self.name} TO ratw_editor, ratw_game, ratw_dm, ratw_publisher')
        self.owner = W.connect('dev', 'owner', dbname=self.name)
        W.migrate(self.owner)
        self.owner.execute("INSERT INTO world.worlds (id, name) VALUES ('w', 'Mantle world')")
        # Everyone the fixture's mantles are written for, and a Hollin to be named where only slots may name.
        people = set()
        for f in FIXTURE.rglob('*.mantle'):
            m = T.read(f.read_text(), str(f))
            if m.written_for:
                people.add(m.written_for)
        for npc in sorted(people) + ['tt_hollin']:
            insert_npc(self.owner, 'w', npc, 'Hollin Ashby' if npc == 'tt_hollin' else npc.replace('_', ' ').title())
            self.owner.execute("INSERT INTO live.npc_voice (world_id, npc_id) VALUES ('w', %s)", (npc,))
        self.owner.execute("INSERT INTO game.checkpoints (world_id, schema_version, revision, payload) VALUES "
                           "('w', 1, 1, '{\"time\": 144000.0, \"x\": 1}')")
        self.editor = W.connect('dev', 'editor', dbname=self.name)
        self.tmp = Path(tempfile.mkdtemp(prefix='ratw-mantles-'))
        self.drafts = self.tmp / 'drafts'
        shutil.copytree(FIXTURE, self.drafts)
        return self

    def __exit__(self, *_):
        self.editor.close()
        self.owner.close()
        shutil.rmtree(self.tmp, ignore_errors=True)
        with superuser() as su:
            su.execute(f'DROP DATABASE {self.name} WITH (FORCE)')

    def load(self, paths=None, **kw):
        return L.load_into(self.editor, 'w', [str(p) for p in (paths or [self.drafts])], kw.pop('batch', 'test'), **kw)

    def count(self, sql, *args):
        return self.owner.execute(sql, args).fetchone()[0]


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class MantleRows(unittest.TestCase):
    def test_migration(self):
        with Scratch() as s:
            o, e = s.owner, s.editor
            with e.transaction():
                e.execute("SET LOCAL ratw.edit_source = 'test'")
                e.execute("INSERT INTO live.mantles (world_id, id, dialect, tier, accents) VALUES ('w', 'cw001', 'coldwater', "
                          "'standard', '{light}')")
                for i in range(3):
                    e.execute("INSERT INTO live.mantle_lines (world_id, mantle_id, key, section, kind, move, position, body) VALUES "
                              "('w', 'cw001', %s, 'grown', 'line', 'greet', %s, 'line greet : Hello.')", (f'grown.greet.{i}', i))
            self.assertEqual(s.count("SELECT revision FROM live.mantles"), 1, 'a new mantle and its items: revision 1')
            with e.transaction():
                e.execute("UPDATE live.mantle_lines SET body = 'line greet : Hi.' WHERE key = 'grown.greet.1'")
                e.execute("UPDATE live.mantle_lines SET body = 'line greet : Hello.' WHERE key = 'grown.greet.0'")
                e.execute("UPDATE live.mantles SET title = 'x'")
            self.assertEqual(s.count("SELECT revision FROM live.mantles"), 2, 'one edit, one revision')
            self.assertEqual(dict(o.execute("SELECT key, revision FROM live.mantle_lines").fetchall()),
                             {'grown.greet.0': 1, 'grown.greet.1': 2, 'grown.greet.2': 1}, 'only the changed item moves')
            hist = o.execute("SELECT key, revision, action, source FROM live.mantle_history ORDER BY seq").fetchall()
            self.assertEqual(hist[0], ('', 1, 'insert', 'test'))
            self.assertIn(('grown.greet.1', 2, 'update', ''), hist)
            self.assertEqual(len(hist), 6, 'no history for an unchanged save')
            with self.assertRaises(Exception):
                e.execute("INSERT INTO live.mantle_history (world_id, mantle_id, revision, action) VALUES ('w', 'cw001', 1, 'insert')")
            g = W.connect('dev', 'game', dbname=s.name)
            with self.assertRaises(Exception):
                g.execute("UPDATE live.mantles SET title = 'y'")
            self.assertEqual(g.execute('SELECT count(*) FROM live.mantle_lines').fetchone()[0], 3, 'the game reads them')
            o.execute('GRANT UPDATE ON live.npc_voice TO ratw_game')
            g.execute("UPDATE live.npc_voice SET mantle_id = 'cw001', bound_how = 'authored' WHERE npc_id = 'tt_co_keeper'")
            self.assertEqual(s.count("SELECT count(*) FROM live.game_changes WHERE table_name = 'live.npc_voice'"), 1,
                             "the game's binding is in live.game_changes")
            g.close()
            with self.assertRaises(Exception):
                o.execute("UPDATE live.npc_voice SET mantle_id = 'cw001' WHERE npc_id = 'tt_hollin'")
            with self.assertRaises(Exception):
                e.execute("DELETE FROM live.mantles WHERE id = 'cw001'")
            self.assertTrue(e.execute("SELECT dm.queue_talk_sync('me', 'mantle.sync', 'cw001')").fetchone()[0])
            with self.assertRaises(Exception):
                e.execute("SELECT dm.queue_talk_sync('me', 'economy.sync', 'x')")
            # The rollback, after a backup, and 0066 again.
            backup = s.tmp / 'backup.jsonl'
            L.dump(o, 'w', None, None, backup)
            self.assertIn('live.mantle_history', backup.read_text())
            o.execute(ROLLBACK.read_text())
            self.assertEqual(s.count("SELECT count(*) FROM information_schema.tables WHERE table_schema = 'live' AND "
                                     "table_name LIKE 'mantle%%'"), 0)
            self.assertEqual(s.count("SELECT count(*) FROM information_schema.columns WHERE table_name = 'npc_voice' AND "
                                     "column_name = 'mantle_id'"), 0)
            self.assertEqual(W.migrate(o), ['0066_mantles'])

    def test_load_export_round_trip(self):
        with Scratch() as s:
            report = s.load(bind=True)
            self.assertEqual(report['problems'], [])
            self.assertEqual(len(report['loaded']), 16)
            self.assertIn('cw017 -> tt_co_keeper', report['bound'])
            self.assertEqual(s.count("SELECT count(*) FROM live.npc_voice WHERE mantle_id IS NOT NULL"), 13,
                             'each bound to the one it was written for (the spares to nobody)')
            self.assertEqual(s.count("SELECT bound_day FROM live.npc_voice WHERE npc_id = 'tt_co_keeper'"), 10.0)
            self.assertEqual(s.count("SELECT count(*) FROM dm.actions WHERE kind = 'voice.sync'"), 1, 'the server told once')
            self.assertEqual(s.count("SELECT count(DISTINCT source) FROM live.mantle_history"), 1)
            self.assertTrue(Path(report['backup']).exists())
            self.assertEqual(s.count("SELECT written_by FROM live.mantles WHERE id = 'cw017'"), 'claude-test')
            # The files were written back from the rows: named, numbered, with their revision.
            back = (s.drafts / 'coldwater' / 'cw017.mantle').read_text()
            self.assertIn('revision 1\n', back)
            self.assertIn('line @1 insist', back)
            # The export, and the export of a load of it: the same bytes.
            out1, out2 = s.tmp / 'export1', s.tmp / 'export2'
            E.export(s.editor, 'w', out1)
            self.assertEqual((out1 / 'coldwater' / 'cw017.mantle').read_text(), back)
            again = s.load([out1], batch='again')
            self.assertEqual(again['problems'], [])
            for mid, done in again['loaded'].items():
                self.assertEqual((done['inserted'], done['updated'], done['deleted']), (0, 0, 0), f'{mid} unchanged')
            self.assertEqual(s.count("SELECT max(revision) FROM live.mantles"), 1, 'no revision moved')
            E.export(s.editor, 'w', out2)
            for f in out1.rglob('*'):
                if f.is_file():
                    self.assertEqual(f.read_bytes(), (out2 / f.relative_to(out1)).read_bytes(), str(f.relative_to(out1)))
            self.assertEqual(E.differences(s.editor, 'w', out1), [], 'G6: rows and export agree')
            # The game's reading of the rows (Game::loadMantles assembles the text from them) gives the keys they have.
            m = T.read(back)
            rows = dict(s.owner.execute("SELECT key, body FROM live.mantle_lines WHERE mantle_id = 'cw017'").fetchall())
            self.assertEqual({i.key for i in m.items}, set(rows))
            # An edit as Atlas makes it: one item, from the revision it was read at.
            m.items[[i.key for i in m.items].index('grown.insist.1')].body = 'line @1 insist [conv.did=swim] : Every year, and the lake hasn\'t had me yet.'
            with s.editor.transaction():
                done = S.save(s.editor, 'w', m, base_revision=1, source='atlas', account='tester')
            self.assertEqual((done['updated'], done['revision']), (1, 2))
            self.assertEqual(s.count("SELECT account FROM live.mantle_history WHERE key = 'grown.insist.1' AND action = 'update'"), 'tester')
            found = E.differences(s.editor, 'w', out1)
            self.assertIn('cw017 grown.insist.1: differs', found)
            self.assertIn('cw017: revision 2 in the rows, 1 in the file', found)
            # The old file is now stale.
            stale = s.load([out1 / 'coldwater' / 'cw017.mantle'], batch='stale', export=False)
            self.assertTrue(any('moved on to revision 2' in p for p in stale['problems']), stale['problems'])
            self.assertEqual(stale['loaded'], {})
            theirs = s.load([out1 / 'coldwater' / 'cw017.mantle'], batch='stale', export=False, take_theirs={'cw017'})
            self.assertEqual(theirs['skipped'], ['cw017'])
            self.assertEqual(s.count("SELECT revision FROM live.mantles WHERE id = 'cw017'"), 2)
            mine = s.load([out1 / 'coldwater' / 'cw017.mantle'], batch='mine', export=False, take_mine={'cw017'})
            self.assertEqual(mine['loaded']['cw017']['updated'], 1)
            self.assertEqual(s.count("SELECT revision FROM live.mantles WHERE id = 'cw017'"), 3)
            # A new unnamed item never takes a key the rows have, though the file lacks it.
            draft = T.write(T.read(back)).replace('line @1 preclose', 'line preclose')
            path = s.tmp / 'cw017-draft.mantle'
            path.write_text(draft.replace('revision 1', 'revision 3'))
            again = s.load([path], batch='draft', export=False)
            self.assertEqual(again['problems'], [])
            self.assertEqual(again['loaded']['cw017']['inserted'], 1)
            self.assertIn('grown.preclose.2', {r[0] for r in s.owner.execute("SELECT key FROM live.mantle_lines WHERE mantle_id = 'cw017'")})
            # check: the export is stale now, and says so.
            self.assertTrue(any('differs' in f or 'revision' in f for f in L.check_conn(s.editor, 'w', out1)))

    def test_refusals_write_nothing(self):
        with Scratch() as s:
            self.assertEqual(s.load(bind=True)['problems'], [])
            before = s.count('SELECT count(*) FROM live.mantle_history')
            base = (s.drafts / 'saltwell' / 'sw001.mantle').read_text()
            cases = {
                'named': (base.replace('line @1 answer.how : ', 'line @1 answer.how : Hollin says hello. '), 'Hollin'),
                'gods': (base.replace('line @1 answer.how : ', 'line @1 answer.how : By the gods. '), 'gods'),
                'dog': (base.replace('line @1 answer.how : ', 'line @1 answer.how : A dog barked. '), 'dog'),
                'stop': (base.replace('oath! = By the pans', 'oath! = By the pans.'), 'ends its own sentence'),
                'loader': (base.replace('line @1 answer.how', 'line @1 flarp'), "'flarp' is not a move"),
                'truth': (base.replace('line @1 answer.how : ', 'line @1 answer.how : My nets are torn. '), 'fisher'),
                'minimum': (T.write(T.read(base)).split('\nany\n')[0] + '\n', 'minimum'),
            }
            for name, (text, why) in cases.items():
                p = s.tmp / f'{name}.mantle'
                p.write_text(text)
                r = s.load([p], batch=name, export=False)
                self.assertTrue(any(why in x for x in r['problems']), f'{name}: {r["problems"]}')
                self.assertEqual(r['loaded'], {}, name)
            self.assertEqual(s.count('SELECT count(*) FROM live.mantle_history'), before, 'nothing written')
            # A draft under its tier's minimum may be saved; a retire while worn may not.
            m = T.read('mantle sw002\nstate draft\ndialect saltwell\naccent light\ntier spare\n\nyoung\n'
                       'line tell.boast : I found a blue stone by the pans!\n')
            with s.editor.transaction():
                S.save(s.editor, 'w', m, base_revision=None, source='atlas')
            worn = T.read(base)
            worn.state = 'retired'
            with self.assertRaises(S.StoreError) as e:
                with s.editor.transaction():
                    S.save(s.editor, 'w', worn, base_revision=1, source='atlas')
            self.assertIn('while tt_sa_storekeeper wears it', str(e.exception))

    def test_backup_and_restore(self):
        with Scratch() as s:
            s.load(bind=True)
            path = s.tmp / 'backup.jsonl'
            n = L.dump(s.editor, 'w', None, None, path)
            self.assertGreater(n, 200)
            s.owner.execute("DELETE FROM live.mantle_lines WHERE mantle_id = 'cw017' AND key = 'grown.insist.1'")
            s.owner.execute("UPDATE live.npc_voice SET mantle_id = NULL WHERE npc_id = 'tt_co_keeper'")
            self.assertGreater(L.restore_conn(s.editor, str(path)), 200)
            self.assertEqual(s.count("SELECT mantle_id FROM live.npc_voice WHERE npc_id = 'tt_co_keeper'"), 'cw017')
            self.assertEqual(s.count("SELECT source FROM live.mantle_history ORDER BY seq DESC LIMIT 1"), 'restore')
            self.assertEqual(s.count("SELECT count(*) FROM live.mantle_lines WHERE key = 'grown.insist.1' AND mantle_id = 'cw017'"), 1)


if __name__ == '__main__':
    unittest.main()
