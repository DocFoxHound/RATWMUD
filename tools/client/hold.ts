// A player who signs in and stays, telling what they see: for tests that act on a player from outside (a Dungeon
// Master's queued action, tools/test_dm_live.py). Prints one JSON line when in the world ({"in": selfId}), then one for
// each system line that arrives ({"said": text}) and each change of the character's injuries ({"injuries": [...]}),
// and leaves after --seconds.
//
//   node --experimental-strip-types tools/client/hold.ts --port 7788 --identity ash [--seconds 30]
import {Connection} from '../../Client/src/net/connection.ts';

const args = new Map<string, string>();
for (let i = 2; i < process.argv.length; i += 2)
    args.set(process.argv[i].replace(/^--/, ''), process.argv[i + 1]);
const port = args.get('port') ?? '7788', identity = args.get('identity') ?? 'ash', seconds = Number(args.get('seconds') ?? 30);
const say = (o: unknown) => process.stdout.write(JSON.stringify(o) + '\n');

let entered = false, injuries = '';
const seen = new Set<unknown>();
const connection = new Connection(`ws://127.0.0.1:${port}/ws`, {
    lobby: () => {}, enter: () => {}, motion: () => {}, artwork: () => {},
    snapshot: () => {},
    event: () => {},
}, open => {
    if (open) connection.submit({type: 'hello', id: identity, name: identity});
});
const s = connection.session;
const timer = setInterval(() => {
    const self = s.latestSnapshot?.self;
    if (!entered && self?.id) {
        entered = true;
        say({in: self.id});
    }
    for (const e of s.events)
        if (e?.type === 'system' && typeof e.text === 'string' && !seen.has(e.sequence)) {
            seen.add(e.sequence);
            say({said: e.text});
        }
    const now = JSON.stringify(self?.injuries ?? []);
    if (entered && now !== injuries) {
        injuries = now;
        say({injuries: self?.injuries ?? []});
    }
}, 100);
setTimeout(() => {
    clearInterval(timer);
    connection.close();
    setTimeout(() => process.exit(0), 300);
}, seconds * 1000);
