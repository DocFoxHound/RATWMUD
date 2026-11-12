#!/usr/bin/env python3
"""A throwaway world's Editor port for Desktop/tests/editor.test.mjs (Docs/Design/78, Phase M4's end-to-end run): the
real helper (tools/world_admin.py: its TLS 1.3 port, sign-in, sessions, account routes) on a throwaway database with
every migration, and an admin directory of its own with the world's certificate and first owner.

Until the hosted studio (M3) plugs into Core.app, a stand-in app serves the signed Editor bundle given with --bundle
(page files and bundle.json, only behind a session, as M3 will) and answers /api/studio/session; it records each
request's path, status and the session's account, and the bearer token it saw (only so the test can look for it where
it must never be).

  python3 Desktop/tests/editor_world.py --bundle DIR [--tag s2stage_]

Prints one JSON line when listening: {port, code, owner, fingerprint, admin}. Then reads commands, one a line:
  rotate      ratw-editor new-certificate (signed by the old key), and the helper reloads
  leak        ratw-editor new-certificate --leaked (no signature, every session ended), and the helper reloads
  log         prints {"log": [...]} (the stand-in app's requests so far)
  quit        stops; the database is dropped and the admin directory removed (also on end of input)
"""
from __future__ import annotations

import argparse
import asyncio
import contextlib
import json
from pathlib import Path
import secrets
import shutil
import sys
import tempfile
import threading

TOOLS = Path(__file__).resolve().parents[2] / 'tools'
sys.path.insert(0, str(TOOLS))
import world_admin as A  # noqa: E402
import world_db as W  # noqa: E402
from test_world_db import superuser  # noqa: E402

MIME = {'.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.png': 'image/png',
        '.svg': 'image/svg+xml', '.woff2': 'font/woff2', '.ttf': 'font/ttf', '.ico': 'image/x-icon'}


def stand_in(bundle: Path, log: list):
    def app(session, method, path, query, headers, body):
        log.append({'path': path, 'account': session['account'], 'bearer': headers.get('authorization', '')[7:]})
        if path == '/api/studio/session':
            world = {'key': 'world', 'name': 'Ashfall Vale', 'kind': 'live'}
            data = {'me': {'username': session['username'], 'role': 'admin' if session['role'] == 'owner' else 'dm'}, 'suite': 'dm',
                    'atlas': {'available': False, 'world': world, 'worlds': [world]},
                    'dm': {'world': 'world', 'live': 'world', 'worlds': [world], 'switch': ''}}
            return 200, 'application/json', json.dumps(data).encode()
        if path.startswith('/api/'):
            return 404, 'application/json', b'{"error": "not-found"}'
        relative = 'index.html' if path in ('/', '/index.html') else path.lstrip('/')
        file = (bundle / relative).resolve()
        if bundle.resolve() not in file.parents or not file.is_file():
            return 404, 'application/json', b'{"error": "not-found"}'
        return 200, MIME.get(file.suffix, 'application/octet-stream'), file.read_bytes()
    return app


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
        su.execute(f'GRANT CONNECT ON DATABASE {database} TO ratw_publisher, ratw_game, ratw_dm')
    log: list = []
    try:
        with W.connect('dev', 'owner', dbname=database) as owner:
            W.migrate(owner)
        A.make_certificate(admin)
        (admin / 'settings.json').write_text(json.dumps({'name': 'Ashfall Vale', 'package': 'e2e', 'editor': {}}))
        (admin / 'progress.json').write_text(json.dumps({'code': 'ready', 'percent': 100}))
        connect = lambda: W.connect('dev', 'dm', dbname=database)  # noqa: E731
        with connect() as conn:
            first = A.ensure_first_owner(conn)
        code = A.make_code(A.world_id(admin), A.cert_spki(admin / 'editor-cert.pem'), first['username'], first['password'], first['invite'])
        core = A.Core(connect=connect, folder=admin, peers=A.Peers('real', unidentified=[]))
        core.app = stand_in(args.bundle, log)
        loop = asyncio.new_event_loop()
        server = A.EditorServer(core, '127.0.0.1', 0)
        port = loop.run_until_complete(server.start())
        threading.Thread(target=loop.run_forever, daemon=True).start()
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
            elif command == 'log':
                print(json.dumps({'log': log}), flush=True)
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
