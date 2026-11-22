#!/usr/bin/env python3
"""How fast the story runner keeps up (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 7): events a second
it reads, how far behind the log it falls, and seconds from an event to its effect applied by the game.

A throwaway, migrated database (ratw_test_storybench_*) gets a world, three wolves, and stories published as the console
would: winter-ferries and reed-ford (bundled examples), a tally that counts every "economy" event (rules on each), and a
reward story whose trigger fires on about one "economy" event in twenty-five (cheese bought) and gives a deed: an effect
for the game. The event stream is a real one: a stretch of Ridgemere 3's own log, read (never written) from its database,
with its daily burst (the day's turn writes 2,700 events in one save), replayed into the throwaway world either as it came
(--mode backlog: all 20,000 at once, as fast as the runner goes: its ceiling), its largest save alone (--mode peak: the
day's turn, 2,681 events at once), or at a steady rate (--mode steady --rate N for --seconds S, written every second as
the game's early event writes do since Phase 7). The game is stood in for: a loop
answering every queued order (a batch with a result for each item), as often as the game looks (--game-poll, 1 s before
Phase 7, 0.25 s after), woken by ratw_dm as the game is.

The runner is a real process, `python3 <root>/tools/story_runner.py --world dev`, its database pointed at the throwaway
one (RATW_DEV_DBNAME): --root picks which build of the runner (a worktree of an older phase for "before"). Nothing is
written anywhere but the throwaway database, which is dropped at the end.

    python3 tools/bench_story_runner.py --root ../r3-camp-p6 --game-poll 1 --mode peak
    python3 tools/bench_story_runner.py --root . --game-poll 0.25 --mode steady --rate 1000 --seconds 20
"""
import argparse
import json
import os
import secrets
import signal
import statistics
import subprocess
import sys
import threading
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import psycopg  # noqa: E402

import story_program as rules  # noqa: E402
import story_runner as runner  # noqa: E402
import world_db as W  # noqa: E402
from test_world_db import superuser  # noqa: E402

WOLVES = ['player-ash', 'player-birch', 'player-elm']
COLUMNS = ('kind', 'actor', 'target', 'cell', 'item', 'quantity', 'coins', 'detail', 'x', 'y')


def admin(name):
    s = W.settings()
    return psycopg.connect(host=s.get('RATW_DB_HOST', '127.0.0.1'), port=s.get('RATW_DB_PORT', '5433'), dbname=name,
                           user=s.get('POSTGRES_USER', 'postgres'), password=s.get('POSTGRES_PASSWORD', ''), autocommit=True)


def real_stream(first, last):
    """A stretch of Ridgemere 3's log, read only: each event's fields, and which save (recorded_at) it came in."""
    with W.connect('ridgemere3', 'dm') as c:
        rows = c.execute(f"""SELECT {','.join(COLUMNS)},game_day,extract(epoch from recorded_at) FROM game.events
                              WHERE id BETWEEN %s AND %s ORDER BY id""", (first, last)).fetchall()
    saves = []
    for r in rows:
        e = dict(zip(COLUMNS, r[:10]))
        e.update(day=r[10], x=e['x'] if e['x'] is not None else None, y=e['y'] if e['y'] is not None else None)
        e = {k: v for k, v in e.items() if v is not None}
        if not saves or abs(saves[-1][0] - r[11]) > 0.5:
            saves.append([r[11], []])
        saves[-1][1].append(e)
    return saves


def programs(examples):
    tally = {
        'format': 'ratw-story-program', 'version': 1, 'id': 'bench-tally', 'title': 'The market tally',
        'audience': {'kind': 'players', 'players': WOLVES}, 'start': 'count',
        'phases': [{'id': 'count', 'journal': 'Every trade is counted.', 'effects': [], 'exits': [], 'triggers': [
            {'id': 'count-trade', 'on': 'economy', 'actions': [{'do': 'add', 'scope': 'story', 'key': 'trades', 'value': 1}]},
            {'id': 'count-bread', 'on': 'economy', 'match': {'item': 'bread'},
             'actions': [{'do': 'add', 'scope': 'story', 'key': 'loaves', 'value': 'quantity'}]}]}],
        'tests': [{'name': 'A loaf sold', 'cohort': WOLVES, 'expect': ['count'], 'events': [
            {'id': 'a', 'kind': 'economy', 'day': 1, 'actor': 'till', 'target': 'x', 'item': 'bread', 'quantity': 1}]}],
    }
    reward = {
        'format': 'ratw-story-program', 'version': 1, 'id': 'bench-cheese', 'title': 'Cheese for the fair',
        'audience': {'kind': 'players', 'players': WOLVES}, 'start': 'fair',
        'phases': [{'id': 'fair', 'journal': 'Ash is owed a deed for every cheese sold.', 'effects': [], 'exits': [], 'triggers': [
            {'id': 'cheese-sold', 'on': 'economy', 'match': {'item': 'cheese'},
             'actions': [{'do': 'effect', 'effect': {'kind': 'deed.award', 'player': 'player-ash', 'detail': 'A cheese sold.'}}]}]}],
        'tests': [{'name': 'A cheese sold', 'cohort': WOLVES, 'expect': ['fair'], 'events': [
            {'id': 'a', 'kind': 'economy', 'day': 1, 'actor': 'till', 'target': 'x', 'item': 'cheese', 'quantity': 1}]}],
    }
    return [examples['winter-ferries'], examples['reed-ford'], tally, reward]


def fake_game(name, poll, stop, answered):
    """The game, stood in for: every `poll` seconds once notified (ratw_dm), each queued order applied; a batch answered
    with a result for each of its items."""
    conn = admin(name)
    conn.execute('LISTEN ratw_dm')
    conn.execute('SET ROLE ratw_game')
    notified = True
    last = 0.0
    while not stop.is_set():
        for _ in conn.notifies(timeout=0.02, stop_after=1):
            notified = True
        if not notified or time.monotonic() - last < poll:
            continue
        last, notified = time.monotonic(), False
        with conn.transaction():
            rows = conn.execute("SELECT id,kind,payload FROM dm.actions WHERE status='queued' ORDER BY id LIMIT 50").fetchall()
            for rid, kind, payload in rows:
                result = json.dumps([{'ok': True, 'text': 'ok'} for _ in payload['items']]) if kind == 'story.effects' else 'ok'
                conn.execute("UPDATE dm.actions SET status='applied',result=%s,done_at=clock_timestamp() WHERE id=%s", (result, rid))
                answered.append(rid)
    conn.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--root', default=str(HERE.parent), help='The worktree whose tools/story_runner.py runs.')
    ap.add_argument('--game-poll', type=float, default=0.25)
    ap.add_argument('--mode', choices=('backlog', 'peak', 'steady'), default='peak')
    ap.add_argument('--rate', type=int, default=1000, help='steady: events a second')
    ap.add_argument('--seconds', type=int, default=20, help='steady: for how long')
    ap.add_argument('--first', type=int, default=352000, help='The stretch of Ridgemere 3\'s log replayed (event ids).')
    ap.add_argument('--last', type=int, default=372000)
    ap.add_argument('--timeout', type=float, default=240)
    ap.add_argument('--json', help='Write the numbers here too.')
    args = ap.parse_args()
    root = Path(args.root).resolve()
    library = os.environ.get('RATW_STORY_LIBRARY') or str(root / 'build-core' / 'libratw_story.so')

    stream = real_stream(args.first, args.last)
    events = [e for _, batch in stream for e in batch]
    print(f'stream: {len(events)} events of Ridgemere 3 in {len(stream)} saves, the largest {max(len(b) for _, b in stream)}', flush=True)

    W.ensure_roles()
    name = 'ratw_test_storybench_' + secrets.token_hex(3)
    with superuser() as su:
        su.execute(f'CREATE DATABASE {name} OWNER ratw_owner')
        su.execute(f'GRANT CONNECT ON DATABASE {name} TO ratw_editor, ratw_publisher, ratw_game, ratw_dm')
    proc = None
    stop = threading.Event()
    try:
        with W.connect('dev', 'owner', dbname=name) as owner:
            W.migrate(owner)
            world = 'bench'
            owner.execute('INSERT INTO world.worlds(id,name) VALUES (%s,%s)', (world, 'Bench world'))
            owner.execute('INSERT INTO game.checkpoints(world_id,schema_version,revision,payload) VALUES (%s,1,1,%s)',
                          (world, json.dumps({'calendarDays': 10})))
            for npc in ('rm111', 'rm112', 'rm113', 'rm119'):
                owner.execute("""INSERT INTO live.npcs(world_id,id,name,role,age,voice,appearance,hours_start,hours_end,
                    home_x,home_y,work_x,work_y,evening_x,evening_y) VALUES (%s,%s,%s,'civilian',4,1,'{}',8,12,0,0,0,0,0,0)""",
                              (world, npc, npc))
            for faction in ('city_watch', 'road_company'):
                owner.execute('INSERT INTO live.factions(world_id,id,name) VALUES (%s,%s,%s)', (world, faction, faction))
            for wolf in WOLVES:
                owner.execute("INSERT INTO game.characters(world_id,key,position,data) VALUES (%s,%s,1,'{}')", (world, wolf))
        conn = admin(name)
        conn.execute('SET ROLE ratw_game')
        conn.execute('SELECT game.record_events(%s,%s::jsonb)', (world, json.dumps(
            [{'kind': 'arrival', 'actor': w, 'day': 10} for w in WOLVES])))
        conn.execute('SET ROLE ratw_dm')
        runs = []
        for p in programs(runner.catalogue()['examples']):
            with conn.transaction():
                ticket = runner.approve(conn, world, 'bench', p, rules.digest(p))['ticket']
                runs.append(runner.publish(conn, world, ticket)['run'])
        answered = []
        game = threading.Thread(target=fake_game, args=(name, args.game_poll, stop, answered), daemon=True)
        game.start()
        env = {**os.environ, **W.settings(), 'RATW_DEV_DBNAME': name, 'RATW_STORY_LIBRARY': library, 'PYTHONUNBUFFERED': '1'}
        proc = subprocess.Popen([sys.executable, str(root / 'tools' / 'story_runner.py'), '--world', 'dev'], env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        # (Settled: each run's phase answered, the runner idle, before the stream starts.)
        t0 = time.time()
        while time.time() - t0 < 30:
            if conn.execute("SELECT count(*) FROM dm.actions WHERE status='queued'").fetchone()[0] == 0 and answered and \
                    conn.execute("SELECT count(*) FROM dm.story_releases WHERE world_id=%s AND status='running'", (world,)).fetchone()[0] == len(runs):
                break
            time.sleep(0.2)
        time.sleep(3)
        start_id = conn.execute('SELECT max(id) FROM game.events').fetchone()[0]

        def write(batch):
            conn.execute('SET ROLE ratw_game')
            conn.execute('SELECT game.record_events(%s,%s::jsonb)', (world, json.dumps(batch)))
            conn.execute("SELECT pg_notify('ratw_events',%s)", (world,))
            conn.execute('SET ROLE ratw_dm')

        behind_samples = []
        began = time.time()
        if args.mode == 'backlog':
            for _, batch in stream:
                write(batch)
        elif args.mode == 'peak':
            write(max((b for _, b in stream), key=len))
        else:
            n = 0
            for s in range(args.seconds):
                tick_at = began + s
                while time.time() < tick_at:
                    time.sleep(0.005)
                batch = [events[(n + i) % len(events)] for i in range(args.rate)]
                n += args.rate
                write(batch)
                newest = conn.execute('SELECT max(id) FROM game.events').fetchone()[0]
                slowest = conn.execute("SELECT min(cursor) FROM dm.story_releases WHERE world_id=%s AND status='running'",
                                       (world,)).fetchone()[0]
                behind_samples.append(newest - slowest)
        written_at = time.time()
        last_id = conn.execute('SELECT max(id) FROM game.events').fetchone()[0]
        total = last_id - start_id
        caught = None
        while time.time() - began < args.timeout:
            slowest = conn.execute("SELECT min(cursor) FROM dm.story_releases WHERE world_id=%s AND status='running'", (world,)).fetchone()[0]
            if slowest is not None and slowest >= last_id:
                caught = time.time()
                break
            time.sleep(0.05)
        # Seconds from an event to its effect applied: each firing of the reward story, from the event's commit to the
        # done_at of the first order sent at or after the firing.
        reward_run = runs[-1]
        lags = [float(r[0]) for r in conn.execute(
            """SELECT extract(epoch from (SELECT a.done_at FROM dm.actions a WHERE a.requested_at >= h.recorded_at
                                           AND a.kind IN ('deed.award','story.effects') AND a.done_at IS NOT NULL ORDER BY a.id LIMIT 1)
                                         - e.recorded_at)
               FROM dm.story_trigger_history h JOIN game.events e ON e.id = (h.entry->>'event')::bigint
               WHERE h.run=%s AND e.id > %s""", (reward_run, start_id)) if r[0] is not None]
        firings = conn.execute('SELECT count(*) FROM dm.story_trigger_history h WHERE run=%s AND (entry->>\'event\')::bigint > %s',
                               (reward_run, start_id)).fetchone()[0]
        deeds = conn.execute("""SELECT count(*) FILTER (WHERE kind='deed.award'),
                                       coalesce(sum(jsonb_array_length(payload->'items')) FILTER (WHERE kind='story.effects'),0)
                                FROM dm.actions WHERE id >= (SELECT min(id) FROM dm.actions WHERE requested_at >= to_timestamp(%s))""",
                             (began,)).fetchone()
        states = conn.execute("SELECT program->>'id',status,problem FROM dm.story_releases WHERE world_id=%s", (world,)).fetchall()
        elapsed = (caught or time.time()) - began
        out = {
            'root': str(root), 'perRun': round(total * len(runs) / elapsed, 1), 'stories': len(runs), 'mode': args.mode, 'gamePoll': args.game_poll, 'events': total,
            'caughtUp': caught is not None, 'seconds': round(elapsed, 2), 'eventsPerSecond': round(total / elapsed, 1),
            'afterWrite': round((caught or time.time()) - written_at, 2),
            'behindMax': max(behind_samples) if behind_samples else None,
            'effectLag': {'n': len(lags), 'firings': firings, 'median': round(statistics.median(lags), 2) if lags else None,
                          'p95': round(sorted(lags)[int(len(lags) * .95) - 1], 2) if lags else None, 'max': round(max(lags), 2) if lags else None},
            'ordersSent': {'single': deeds[0], 'inBatches': deeds[1]},
            'runs': [list(s) for s in states],
        }
        if args.mode == 'steady':
            out['rate'] = args.rate
        print(json.dumps(out, indent=1), flush=True)
        if args.json:
            Path(args.json).write_text(json.dumps(out, indent=1))
    finally:
        stop.set()
        if proc and proc.poll() is None:
            proc.send_signal(signal.SIGTERM)
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
        if proc and proc.stdout:
            tail = proc.stdout.read()[-2000:]
            if tail.strip():
                print('runner said:', tail)
        time.sleep(0.5)
        with superuser() as su:
            su.execute(f'DROP DATABASE IF EXISTS {name} WITH (FORCE)')


if __name__ == '__main__':
    main()
