#!/usr/bin/env python3
"""A throwaway world's Editor port for Desktop/tests/editor.test.mjs (Docs/Design/78, Phase M4's end-to-end run): the
real helper (tools/world_admin.py: its TLS 1.3 port, sign-in, sessions, account routes) with the real hosted studio
(tools/studio.py's HostedStudio, M3, through world_admin.hosted_studio as `serve` plugs it in) on a throwaway database
with every migration and Greyfen with one player (Ada) in it, and an admin directory of its own with the world's
certificate, first owner, settings copy and one-world registry (`world`), as the entrypoint writes them in the image.

The studio serves the signed Editor bundle given with --bundle as the image's Editor/dist (stage.sh's output:
bundle.json and portraits/ inside), only behind a session. Each request the studio gets is logged with its method,
path, status and the session's account, and the bearer token the helper saw (only so the test can look for it where
it must never be). No AI key: the studio's AI is off, so nothing here calls a model.

  python3 Desktop/tests/editor_world.py --bundle DIR [--tag s2stage_]

Prints one JSON line when listening: {port, code, owner, fingerprint, admin, database}. Then reads commands, one a line:
  rotate      ratw-editor new-certificate (signed by the old key), and the helper reloads
  leak        ratw-editor new-certificate --leaked (no signature, every session ended), and the helper reloads
  code NAME   ratw-editor reset NAME: prints {"code": ...}, a new Editor code for that account (no address: as a code made
              on the world's own computer for someone elsewhere)
  log         prints {"log": [...]} (the studio's requests so far)
  actions     prints {"actions": [...], "audit": [...]} (dm.actions and dm.audit's acts: who asked for what)
  quit        stops; the database is dropped and the admin directory removed (also on end of input)
"""
from __future__ import annotations

import argparse
import asyncio
import contextlib
import json
import os
from pathlib import Path
import secrets
import shutil
import sys
import tempfile
import threading

TOOLS = Path(__file__).resolve().parents[2] / 'tools'
sys.path.insert(0, str(TOOLS))
import map_editor  # noqa: E402
import world_admin as A  # noqa: E402
import world_db as W  # noqa: E402
import world_store as S  # noqa: E402
from test_world_db import superuser  # noqa: E402
from test_world_store import greyfen  # noqa: E402

PLAYER = 'player-ada'


def seed(database: str) -> None:
    """Greyfen and its roster (as the editor writes a world), and a save with one player (as the game server does)."""
    with W.connect('world', 'editor', dbname=database) as editor:
        S.save_roster(editor, S.load_roster(editor))
        S.save_world(editor, greyfen(), None, create=True)
    town = greyfen()['cells'][0]
    save = {'schema': 1, 'players': [{'id': PLAYER, 'name': 'Ada', 'cell': town['id'], 'x': 10.5, 'y': 12.5, 'age': 21,
                                      'strength': 55, 'dexterity': 61, 'wisdom': 33, 'stamina': 90, 'posture': 'standing'}]}
    with W.connect('world', 'game', dbname=database) as game:
        game.execute('SELECT game.save_checkpoint(%s, 1, %s)', ('greyfen', json.dumps(save)))


def recorded(app, log: list):
    """The hosted studio, each request it answers logged (method, path, status, the session's account, the bearer)."""
    def answer(session, method, path, query, headers, body):
        status, content_type, data = app(session, method, path, query, headers, body)
        log.append({'method': method, 'path': path, 'status': status, 'account': session['account'],
                    'bearer': headers.get('authorization', '')[7:]})
        return status, content_type, data
    return answer


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--tag', default='s2stage_')
    args = parser.parse_args()
    W.ensure_roles()
    database = f'ratw_test_{args.tag}editor_e2e_{secrets.token_hex(4)}'
    admin = Path(tempfile.mkdtemp(prefix='s2stage-editor-world-'))
    with superuser() as su:
        su.execute(f'CREATE DATABASE {database} OWNER ratw_owner')
        su.execute(f'GRANT CONNECT ON DATABASE {database} TO ratw_editor, ratw_publisher, ratw_game, ratw_dm')
    log: list = []
    # The container's one-world registry in the admin directory (the entrypoint writes it so), the studio's AI config
    # there too (none: no key, no model call), and nothing written beside the repository's own registry.
    (admin / 'worlds.json').write_text(json.dumps({'worlds': [{'key': 'world', 'name': 'Ashfall Vale', 'database': database,
                                                               'kind': 'live'}]}))
    os.environ.update(RATW_WORLDS_FILE=str(admin / 'worlds.json'), RATW_WORLDS_LOCAL=str(admin / 'worlds.local.json'),
                      RATW_AI_CONFIG=str(admin / 'npc-ai.json'), RATW_ADMIN_DIR=str(admin))
    try:
        with W.connect('world', 'owner', dbname=database) as owner:
            W.migrate(owner)
        seed(database)
        A.make_certificate(admin)
        (admin / 'settings.json').write_text(json.dumps({'name': 'Ashfall Vale', 'package': 'e2e', 'cheats': 'off',
                                                         'player_limit': 40, 'workers': 2, 'editor': {}}))
        (admin / 'progress.json').write_text(json.dumps({'code': 'ready', 'percent': 100}))
        connect = lambda: W.connect('world', 'dm', dbname=database)  # noqa: E731
        with connect() as conn:
            first = A.ensure_first_owner(conn)
        core = A.Core(connect=connect, folder=admin, peers=A.Peers('real', unidentified=[]))
        # The image's Editor (/opt/ratw/Editor/dist, with bundle.json and portraits/: stage.sh) is the studio's DIST.
        map_editor.DIST = args.bundle.resolve()
        core.app = recorded(A.hosted_studio(admin), log)
        loop = asyncio.new_event_loop()
        server = A.EditorServer(core, '127.0.0.1', 0)
        port = loop.run_until_complete(server.start())
        threading.Thread(target=loop.run_forever, daemon=True).start()
        # The first owner's code carries this computer's address with the Editor port, as the entrypoint makes it.
        code = A.make_code(A.world_id(admin), A.cert_spki(admin / 'editor-cert.pem'), first['username'], first['password'], first['invite'],
                           f'127.0.0.1:{port}')
        print(json.dumps({'port': port, 'code': code, 'owner': first['username'], 'admin': str(admin), 'database': database,
                          'fingerprint': A.fingerprint(A.cert_spki(admin / 'editor-cert.pem'))}), flush=True)
        for line in sys.stdin:
            command = line.strip()
            if command in ('rotate', 'leak'):
                done = A.new_certificate(admin, leaked=command == 'leak')
                if command == 'leak':
                    with connect() as conn:
                        A.end_all(conn, 'ratw-editor')
                loop.call_soon_threadsafe(server.reload)
                print(json.dumps({'rotated': done}), flush=True)
            elif command.startswith('code '):
                with connect() as conn:
                    password, invite = A.reset_account(conn, command[5:].strip(), 'ratw-editor')
                print(json.dumps({'code': A.make_code(A.world_id(admin), A.cert_spki(admin / 'editor-cert.pem'), command[5:].strip(),
                                                      password, invite)}), flush=True)
            elif command == 'log':
                print(json.dumps({'log': log}), flush=True)
            elif command == 'actions':
                with connect() as conn:
                    actions = [{'kind': r[0], 'target': r[1], 'by': r[2]} for r in conn.execute(
                        'SELECT kind, target_id, requested_by FROM dm.actions ORDER BY id').fetchall()]
                    audit = [{'action': r[0], 'target': r[1], 'who': r[2]} for r in conn.execute(
                        "SELECT action, target, who FROM dm.audit WHERE action NOT LIKE 'login%' AND action NOT LIKE 'editor.%'"
                        ' ORDER BY sequence').fetchall()]
                print(json.dumps({'actions': actions, 'audit': audit}), flush=True)
            elif command == 'quit':
                break
        asyncio.run_coroutine_threadsafe(server.stop(), loop).result(10)
        loop.call_soon_threadsafe(loop.stop)
        core.close()
        return 0
    finally:
        with contextlib.suppress(Exception), superuser() as su:
            su.execute(f'DROP DATABASE IF EXISTS {database} WITH (FORCE)')
        shutil.rmtree(admin, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
