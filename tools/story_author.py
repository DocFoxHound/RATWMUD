"""JSON-lines authoring bridge. Defaults to local files and deterministic rehearsal."""

import argparse
import json
import sys
from pathlib import Path

import story_library
import story_program as rules
import story_runner as runner
import story_scenes


def request(data, snapshot, world_key=None, read_world=False):
    rules.require(isinstance(data, dict), 'Send a JSON object.')
    op = data.get('op')
    if op in story_scenes.OPERATIONS:
        return story_scenes.request(data)
    if op == 'catalog':
        return runner.catalogue()
    if op == 'components':
        entry = next((e for e in story_library.entries() if e['id'] == data.get('eventType')), None)
        rules.require(entry, 'Choose a known event type.')
        return {'components': story_library.components(entry)}
    if op == 'draft' and data.get('blank') is True:
        return {'program': story_library.blank()}
    if op == 'draft' and 'eventType' in data:
        return {'program': story_library.draft(data['eventType'])}
    if op == 'draft':
        examples = runner.catalogue()['examples']
        name = data.get('example', 'wagon')
        rules.require(isinstance(name, str) and name in examples, 'Choose a catalog example.')
        return {'program': examples[name]}
    if op == 'validate':
        return {'digest': rules.review(data.get('program'))['digest']}
    if op == 'rehearse':
        return rules.review(data.get('program'))
    if op == 'words':
        # (Doc 86, V2: the story's words checked offline: symbols, forbidden words, ids in speech, lengths, face banners.)
        import story_words
        return {'problems': story_words.check(data.get('program'))}
    if op in ('atlas.search', 'npc.context') and not read_world:
        if op == 'atlas.search':
            q = str(data.get('query', '')).casefold()
            return {
                'matches': [r for r in snapshot.get('matches', []) if q in (r['id'] + ' ' + r['name']).casefold()][
                    :100
                ]
            }
        npc = data.get('npc')
        entry = next((r for r in snapshot.get('residents', []) if r['resident']['id'] == npc), None)
        rules.require(entry, 'No context for this resident in the selected snapshot.')
        return entry
    rules.require(
        world_key and op in ('publish', 'signal', 'voice', 'atlas.search', 'npc.context'),
        'Operation unavailable in this bridge.',
    )
    import world_db

    with world_db.connect(world_key, 'dm') as conn, conn.transaction():
        world = runner.world_id(conn)
        if op == 'publish':
            return runner.publish(conn, world, data.get('ticket'))
        if op in ('signal', 'voice'):
            return runner.direct(conn, world, op, data)
        if op == 'atlas.search':
            return runner.atlas(conn, world, data.get('query', ''))
        return runner.context(conn, world, data.get('npc', ''))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot', type=Path, help='Context explicitly exported for this author')
    parser.add_argument('--world', help='Operator-selected world for redeeming scoped tickets')
    parser.add_argument('--read-world', action='store_true', help='Allow curated Atlas and NPC reads in that world')
    args = parser.parse_args()
    if args.read_world and not args.world:
        parser.error('--read-world requires --world')
    service_errors = (OSError,)
    if args.world:
        import world_db

        try:
            import psycopg
        except ImportError:
            parser.error('World operations require psycopg 3.')
        service_errors = (OSError, world_db.DatabaseError, psycopg.Error)
    snapshot = json.loads(args.snapshot.read_text()) if args.snapshot else {}
    for raw in sys.stdin:
        try:
            rules.require(len(raw) <= 1024 * 1024, 'Request exceeds 1 MiB.')
            reply = {'ok': True, 'result': request(json.loads(raw), snapshot, args.world, args.read_world)}
        except (ValueError, KeyError, TypeError) as error:
            reply = {'ok': False, 'error': str(error)}
        except service_errors:
            reply = {'ok': False, 'error': 'World service unavailable. Check the operator connection and migration.'}
        print(json.dumps(reply), flush=True)


if __name__ == '__main__':
    main()
