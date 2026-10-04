#!/usr/bin/env python3
"""How the game server has been running (Docs/Design/31-responsiveness.md, the health tracker).

The server keeps a record each minute, and one for every tick that ran long: in dm.health for a world in the
database (migration 0032), else in a JSON-lines file beside the save (<save>.health.jsonl). This reads them back.

    python3 tools/perf_report.py --database dev              the last 24 hours of DEV's server
    python3 tools/perf_report.py --database dev --hours 2    the last two
    python3 tools/perf_report.py --file Saved/ratw-town.json.health.jsonl

It prints the time in blocks (each block's players, tick mean / p99 / worst, ticks over the 50 ms budget, players'
ping and slow connections), then the slowest ticks with where their time went, and the players with the worst ping.
"""
import argparse
import json
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))


def from_database(target: str, hours: float):
    import world_db
    with world_db.connect(target, 'dm') as conn:
        rows = conn.execute(
            "SELECT at, kind, body FROM dm.health WHERE at > now() - make_interval(secs => %s) ORDER BY at",
            (hours * 3600,)).fetchall()
    return [(at, kind, body if isinstance(body, dict) else json.loads(body)) for at, kind, body in rows]


def from_file(path: Path, hours: float):
    since = datetime.now(timezone.utc) - timedelta(hours=hours)
    records = []
    for line in path.read_text().splitlines():
        try:
            r = json.loads(line)
            at = datetime.strptime(r['at'], '%Y-%m-%dT%H:%M:%SZ').replace(tzinfo=timezone.utc)
        except (ValueError, KeyError):
            continue                                # A torn last line, say.
        if at >= since:
            records.append((at, r['kind'], r['body']))
    return records


def parts_text(parts: dict, at_least: float = 1.0) -> str:
    big = sorted(((ms, name) for name, ms in parts.items() if ms >= at_least), reverse=True)
    return ', '.join(f'{name} {ms:.0f}' for ms, name in big) or 'nothing over 1 ms'


def report(records, hours: float, out=sys.stdout):
    windows = [(at, b) for at, kind, b in records if kind == 'window']
    spikes = [(at, b) for at, kind, b in records if kind == 'spike']
    if not windows and not spikes:
        print(f'No health records in the last {hours:g} hours. Is the server running (and migration 0032 applied)?', file=out)
        return
    first, last = records[0][0], records[-1][0]
    print(f'Server health, {first:%Y-%m-%d %H:%M} to {last:%H:%M} UTC: {len(windows)} windows recorded, {len(spikes)} slow ticks.\n', file=out)
    # Blocks: a quarter hour each over a few hours, an hour over a day or more.
    block = timedelta(minutes=15) if hours <= 6 else timedelta(hours=1)
    print(f'{"from":<6} {"players":>7} {"mean":>6} {"p99":>6} {"worst":>6} {"over 50":>8} {"ping":>6} {"ping 95":>8} {"slow":>5}', file=out)
    start = windows[0][0] if windows else first
    i = 0
    while i < len(windows):
        end = start + block
        group = [b for at, b in windows[i:] if at < end]
        if group:
            n = len(group)
            players = max(b.get('clients', 0) for b in group)
            mean = sum(b['tick']['mean'] for b in group) / n
            p99 = max(b['tick']['p99'] for b in group)
            worst = max(b['tick']['max'] for b in group)
            over = sum(b['tick'].get('over50', 0) for b in group)
            pings = [b['ping']['p50'] for b in group if b.get('ping', {}).get('reporting')]
            highs = [b['ping']['p95'] for b in group if b.get('ping', {}).get('reporting')]
            slow = max(b.get('backlog', {}).get('slow', 0) for b in group)
            ping = f'{sum(pings) / len(pings):.0f}' if pings else '-'
            high = f'{max(highs):.0f}' if highs else '-'
            print(f'{start:%H:%M}  {players:>7.0f} {mean:>6.1f} {p99:>6.1f} {worst:>6.0f} {over:>8.0f} {ping:>6} {high:>8} {slow:>5.0f}', file=out)
        i += len(group)
        start = end
    print('\n(ms; "over 50": ticks over the 50 ms budget; ping: the middle player\'s, as their pages report it; '
          '"slow": connections with more than 256 KB waiting.)\n', file=out)
    if spikes:
        print('Slowest ticks:', file=out)
        for at, b in sorted(spikes, key=lambda s: -s[1]['ms'])[:10]:
            note = f' | world: {b["note"]}' if b.get('note') else ''
            print(f'  {at:%m-%d %H:%M:%S}  {b["ms"]:.0f} ms, {b.get("clients", 0):.0f} players: {parts_text(b["parts"])}{note}', file=out)
        print(file=out)
    worst = {}
    for at, b in windows:
        ping = b.get('ping', {})
        if ping.get('worstWho'):
            who = ping['worstWho']
            if ping['worst'] > worst.get(who, (0, None))[0]:
                worst[who] = (ping['worst'], at)
    if worst:
        print('Worst ping by player:', file=out)
        for who, (ms, at) in sorted(worst.items(), key=lambda w: -w[1][0])[:10]:
            print(f'  {who}: {ms:.0f} ms ({at:%m-%d %H:%M})', file=out)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--database', choices=['dev', 'prod'])
    source.add_argument('--file', type=Path)
    parser.add_argument('--hours', type=float, default=24)
    args = parser.parse_args(argv)
    records = from_database(args.database, args.hours) if args.database else from_file(args.file, args.hours)
    report(records, args.hours)
    return 0


if __name__ == '__main__':
    sys.exit(main())
