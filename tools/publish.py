#!/usr/bin/env python3
"""Push to live, now Import from dev (tools/world_sync.py; Docs/Design/66-worlds-atlas-and-dm.md, section 4): the old
command line and Atlas's Push to live dialog, kept working over the three-way import.

  python3 tools/publish.py preview                 what an import would change (and what blocks it)
  python3 tools/publish.py push --note "..."       an import (asks for the publish password; needs maintenance)
  python3 tools/publish.py releases                release history
  python3 tools/publish.py rollback N --note "..." put release N's world back live
  python3 tools/publish.py pull                    Update from live: live's changes into the dev world

The two worlds are Push to live's (tools/world_db.py push_worlds: Atlas's world if it has an upstream). A push is an
import: only while the live world is in maintenance, with every conflict chosen (`choices`, as world_sync takes them);
the add-only seeds and the "copy NPCs back" of before are gone (the three-way comparison keeps live's developments).
The release, password and rollback code lives in world_sync and is re-exported here for older callers.
"""
from __future__ import annotations

import argparse
import getpass
import sys

import world_db
import world_store as S
from world_sync import (AUTHORED, LABELS, LOCK_MINUTES, MAX_FAILURES, NOT_CONTENT, NOTIFY_CHANNEL, PUBLISH_LOCK,  # noqa: F401
                        Locked, NotInMaintenance, PublishError, Sync, Unresolved, WrongPassword, apply_world, check_password,
                        delete, dev_content, finish, from_json, latest_release, orphans, plan_world, read, record, releases,
                        rollback, shape, something, the_world, to_json, validate_dev, world_column, write)


class Publisher:
    """Push to live for the editor host: an import from its dev world into that world's upstream (world_sync.Sync),
    every world reached as ratw_publisher."""

    def __init__(self, connect=None, worlds=None):
        self.dev, self.prod = worlds or world_db.push_worlds()
        self.sync = Sync(connect)

    def preview(self):
        info = self.sync.plan_import(self.dev)
        # (The shape Push to live's dialog reads, beside the full plan: what the import changes on live.)
        return {**info, 'release': info['liveRelease'], 'summary': info['summary']['dev'],
                'changed': bool(info['counts']['dev'] or info['counts']['conflicts'])}

    def push(self, data):
        return self.sync.import_(self.dev, data.get('choices') or {}, data.get('password'), str(data.get('editor') or 'Someone'),
                                 str(data.get('note') or ''), data.get('expect'))

    def pull(self, data):
        result = self.sync.pull(self.dev, data.get('choices') or {}, str(data.get('editor') or 'Live copy'), data.get('expect'))
        # (What the old dialog's "Copy NPC state from live" shows: rows copied by kind, none skipped.)
        copied = {label: sum(counts.values()) for part in result['applied'].values() for label, counts in part.items()}
        return {**result, 'copied': copied, 'skipped': []}

    def rollback(self, data):
        number = data.get('release')
        if not isinstance(number, int):
            raise PublishError('Choose a release to roll back to.')
        return self.sync.rollback(self.prod, number, data.get('password'), str(data.get('editor') or 'Someone'),
                                  str(data.get('note') or ''))


def describe(summary):
    lines = []
    for part in ('world', 'live'):
        for what, counts in summary.get(part, {}).items():
            done = ', '.join(f'{n} {k}' for k, n in counts.items() if n)
            if done:
                lines.append(f'  {what}: {done}')
    return '\n'.join(lines) or '  nothing'


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('preview')
    p = sub.add_parser('push'); p.add_argument('--note', default='')
    sub.add_parser('releases')
    sub.add_parser('pull', help="Update from live: live's changes into the dev world")
    r = sub.add_parser('rollback'); r.add_argument('release', type=int); r.add_argument('--note', default='')
    args = parser.parse_args(argv)
    try:
        publisher = Publisher()
        if args.command == 'preview':
            info = publisher.preview()
            print(f'{info["world"]}: {publisher.dev} revision {info["devRevision"]}, live release {info["release"]}')
            print('\n'.join(f'  blocked: {b}' for b in info['blocked']))
            print('\n'.join(f'  problem: {e}' for e in info['errors']) or describe(info['summary']))
            if info['conflicts']:
                print(f'  {len(info["conflicts"])} conflicts: python3 tools/world_sync.py plan-import {publisher.dev}')
        elif args.command == 'pull':
            result = publisher.pull({'editor': getpass.getuser()})
            print(f'Updated {result["dev"]} from {result["live"]}:\n{describe(result["applied"])}')
        elif args.command == 'releases':
            for rel in publisher.sync.releases(publisher.prod):
                print(f'{rel["number"]:>4}  {rel["at"][:19]}  {rel["kind"]:<8} {rel["by"]:<16} {rel["note"]}')
        else:
            password = getpass.getpass('Publish password: ')
            data = {'password': password, 'editor': getpass.getuser(), 'note': args.note, 'release': getattr(args, 'release', None)}
            result = publisher.push(data) if args.command == 'push' else publisher.rollback(data)
            print(f'Release {result["release"]} is live.\n{describe(result["summary"])}')
    except (PublishError, S.StoreError, world_db.DatabaseError) as error:
        print('\n'.join([str(error)] + [f'  {e}' for e in getattr(error, 'errors', [])]), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
