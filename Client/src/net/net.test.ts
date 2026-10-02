import {test} from 'node:test';
import assert from 'node:assert/strict';
import {ackMessage, commandMessage, decodeMessage, encodeMessage, Kind, MaxCommand, MaxRaw} from './wire.ts';
import {readFileSync} from 'node:fs';
import {inflate} from './inflate.ts';
import {deflateSync, constants} from 'node:zlib';
import {pack, unpack, type MotionFrame} from './motion.ts';
import {fill, SectionCache, EntriesKept} from './sections.ts';
import {Session, type Json} from './session.ts';

const text = (s: string) => new TextEncoder().encode(s);

test('commands and acknowledgements are framed as the server reads them', () => {
    const command = commandMessage('{"type":"move"}')!;
    assert.equal(command[0], Kind.Command);
    assert.equal(new TextDecoder().decode(command.subarray(1)), '{"type":"move"}');
    assert.equal(commandMessage('x'.repeat(MaxCommand + 1)), null, 'an oversized command is not sent');
    const ack = ackMessage(1234.5, true);
    assert.equal(ack.length, 10);
    assert.equal(ack[0], Kind.Ack);
    assert.equal(new DataView(ack.buffer).getFloat64(1, true), 1234.5);
    assert.equal(ack[9], 1);
});

test('server messages inflate; malformed ones are ignored', async () => {
    const raw = text(JSON.stringify({type: 'lobby', stage: 'login'}));
    const message = await encodeMessage(Kind.Event, raw);
    const arrival = decodeMessage(message);
    assert.equal(arrival?.kind, Kind.Event);
    assert.deepEqual(arrival?.raw, raw);
    assert.equal(arrival?.wireBytes, message.length);
    const wrongLength = message.slice();
    new DataView(wrongLength.buffer).setUint32(1, raw.length + 1, true);
    assert.equal(decodeMessage(wrongLength), null, 'a raw length that does not match');
    const unknownKind = message.slice();
    unknownKind[0] = 3;
    assert.equal(decodeMessage(unknownKind), null, 'an unknown kind');
    assert.equal(decodeMessage(message.slice(0, 5)), null, 'a truncated message');
    const garbage = message.slice();
    garbage.fill(7, 5);
    assert.equal(decodeMessage(garbage), null, 'data that is not zlib');
});

const frame: MotionFrame = {
    motionSession: 'session-1', observer: 'player-ash', cellId: 'exterior', cellGeneration: 3, revision: 42, time: 100.25,
    entities: [{id: 'player-ash', x: 1.5, y: 2.25, facing: 0.5, moving: true}, {id: 'npc_élan', x: 3, y: 4, facing: -1, moving: false}],
};

test('inflate matches zlib at every level and strategy, and refuses damage', () => {
    let seed = 7;
    const random = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff;
    const samples = [
        new Uint8Array(0).length ? new Uint8Array(0) : new Uint8Array([1]),
        text('x'.repeat(100000)),
        text(JSON.stringify({rows: Array.from({length: 80}, (_, i) => '.#~^'.repeat(40).slice(i % 4))})),
        Uint8Array.from({length: 70000}, () => Math.floor(random() * 256)),
        Uint8Array.from({length: 50000}, (_, i) => (i * 7) % 13 + (random() < 0.05 ? 100 : 0)),
    ];
    for (const raw of samples)
        for (const level of [0, 1, 6, 9])
            for (const strategy of [constants.Z_DEFAULT_STRATEGY, constants.Z_FIXED, constants.Z_HUFFMAN_ONLY, constants.Z_RLE]) {
                const packed = new Uint8Array(deflateSync(raw, {level, strategy}));
                assert.deepEqual(inflate(packed, raw.length), raw, `level ${level} strategy ${strategy}, ${raw.length} bytes`);
            }
    const packed = new Uint8Array(deflateSync(samples[2]));
    assert.throws(() => inflate(packed, samples[2].length + 1), /wrong size|out of data/);
    assert.throws(() => inflate(packed, samples[2].length - 1), /too long|bad copy/);
    const damaged = packed.slice();
    damaged[damaged.length - 1] ^= 1;
    assert.throws(() => inflate(damaged, samples[2].length), /checksum/);
    assert.throws(() => inflate(packed.slice(0, packed.length - 10), samples[2].length));
    assert.throws(() => inflate(text('not zlib at all'), 10), /not zlib/);
});

test('motion frames unpack exactly, including non-ASCII names', () => {
    const got = unpack(pack(frame))!;
    assert.equal(got.motionSession, 'session-1');
    assert.equal(got.cellGeneration, 3);
    assert.equal(got.time, 100.25);
    assert.equal(got.entities[1].id, 'npc_élan');
    assert.equal(got.entities[0].moving, true);
    assert.ok(Math.abs(got.entities[0].facing - 0.5) < 1e-6);
});

test('cut, padded or foreign motion frames are refused whole', () => {
    const bytes = pack(frame);
    assert.equal(unpack(bytes.subarray(0, bytes.length - 1)), null, 'cut');
    const padded = new Uint8Array(bytes.length + 1);
    padded.set(bytes);
    assert.equal(unpack(padded), null, 'padded');
    const foreign = bytes.slice();
    foreign[0] ^= 1;
    assert.equal(unpack(foreign), null, 'foreign magic');
    const huge = bytes.slice();
    new DataView(huge.buffer).setInt32(4, 1 << 20, true);
    assert.equal(unpack(huge), null, 'a string claiming to be enormous');
});

test('an advertised raw length of nothing, or over the limit, is refused before inflating', async () => {
    const message = await encodeMessage(Kind.Snapshot, text('{"revision":1}'));
    assert.equal(MaxRaw, 16 << 20);
    for (const length of [0, MaxRaw + 1, 0xffffffff]) {
        const bomb = message.slice();
        new DataView(bomb.buffer).setUint32(1, length, true);
        assert.equal(decodeMessage(bomb), null, `a raw length of ${length}`);
    }
});

test('motion frames keep their stamps exactly and positions as float32', () => {
    const sent: MotionFrame = {...frame, observer: 'player-bram', cellId: 'ridgemere-gate', revision: 1234567.891,
        entities: [{id: 'player-bram', x: 0.1, y: -123.456, facing: 2.5, moving: false}, {id: 'npc_x', x: 1e-3, y: 4096.7, facing: 0, moving: true}]};
    const got = unpack(pack(sent))!;
    assert.equal(got.observer, 'player-bram');
    assert.equal(got.cellId, 'ridgemere-gate');
    assert.equal(got.revision, 1234567.891);
    for (let i = 0; i < sent.entities.length; ++i) {
        assert.equal(got.entities[i].x, Math.fround(sent.entities[i].x));
        assert.equal(got.entities[i].y, Math.fround(sent.entities[i].y));
    }
});

test('the server\'s packed motion bytes read back as the frame they came from', () => {
    // Tests/server_parts_tests.cpp checks Core's motion::pack makes exactly these bytes from this frame.
    const golden = JSON.parse(readFileSync(new URL('./motion.golden.json', import.meta.url), 'utf8')) as {frame: MotionFrame; bytes: string};
    const bytes = Uint8Array.from(golden.bytes.match(/../g)!, h => parseInt(h, 16));
    assert.deepEqual(unpack(bytes), golden.frame);
    assert.equal(unpack(bytes)!.entities[1].id, 'npc_élan', 'a non-ASCII ID in its UTF-16 form');
    assert.deepEqual(pack(golden.frame), bytes, 'and this client packs them the same');
});

test('left-out sections are put back from what the client holds', () => {
    const cache = new SectionCache();
    const first: Json = {revision: 1, cell: {id: 'a', rows: ['..#'], heights: ['000']}, visibility: ['222'], doors: [],
        sectionKeys: {'cell.rows': 'r1', 'cell.heights': 'h1', visibility: 'v1', doors: 'd1'}};
    assert.ok(fill(first, cache));
    assert.equal('sectionKeys' in first, false, 'the keys are not left for the screens');
    const second: Json = {revision: 2, cell: {id: 'a'}, sectionKeys: {'cell.rows': 'r1', 'cell.heights': 'h1', visibility: 'v1', doors: 'd1'}};
    assert.ok(fill(second, cache));
    assert.deepEqual((second.cell as Json).rows, ['..#']);
    assert.deepEqual(second.visibility, ['222']);
    const unknown: Json = {revision: 3, cell: {id: 'a'}, sectionKeys: {'cell.rows': 'r9'}};
    assert.equal(fill(unknown, cache), false, 'a part not held asks for everything again');
    const whole: Json = {revision: 4, cell: {id: 'a', rows: ['x']}};
    assert.ok(fill(whole, cache), 'a snapshot sent whole needs nothing');
});

test('what the wolf sees can come as row edits against a version held here', () => {
    const cache = new SectionCache();
    const first: Json = {visibility: ['0000', '0220'], sectionKeys: {visibility: 'v1'}};
    assert.ok(fill(first, cache));
    const second: Json = {visibility: {$delta: 'v1', edits: [[0, 1, '22'], [1, 3, '1']]}, sectionKeys: {visibility: 'v2'}};
    assert.ok(fill(second, cache));
    assert.deepEqual(second.visibility, ['0220', '0221']);
    const again: Json = {sectionKeys: {visibility: 'v2'}};
    assert.ok(fill(again, cache));
    assert.deepEqual(again.visibility, ['0220', '0221'], 'and the result is kept under its own key');
    assert.equal(fill({visibility: {$delta: 'v9', edits: []}, sectionKeys: {visibility: 'v3'}}, cache), false, 'an unknown base asks for everything');
    assert.equal(fill({visibility: {$delta: 'v2', edits: [[5, 0, '2']]}, sectionKeys: {visibility: 'v4'}}, cache), false, 'and so do edits that do not fit');
});

test('map entries the client holds come by key alone', () => {
    const cache = new SectionCache();
    const a = {id: 'cell-a', name: 'A'}, b = {id: 'cell-b', name: 'B'};
    const first: Json = {worldMap: [a, b], sectionKeys: {worldMap: 'm1', 'worldMap#cell-a': 'ka', 'worldMap#cell-b': 'kb'}};
    assert.ok(fill(first, cache));
    const revealed = {id: 'cell-b', name: 'B, seen'};
    const second: Json = {worldMap: [{$held: 'ka'}, revealed], sectionKeys: {worldMap: 'm2', 'worldMap#cell-b': 'kb2'}};
    assert.ok(fill(second, cache));
    assert.deepEqual(second.worldMap, [a, revealed]);
    const third: Json = {sectionKeys: {worldMap: 'm2'}};
    assert.ok(fill(third, cache));
    assert.deepEqual(third.worldMap, [a, revealed], 'an unchanged map is put back whole');
    assert.equal(fill({worldMap: [{$held: 'nope'}], sectionKeys: {worldMap: 'm3'}}, cache), false);
    for (let i = 0; i < EntriesKept + 10; ++i) cache.keepEntry(`k${i}`, i);
    assert.equal(cache.entries.size, EntriesKept, 'the oldest entries are let go');
    assert.equal(cache.entries.has('ka'), false);
});

function recorder() {
    const sent: string[] = [], acks: Array<[number, boolean]> = [], shown: string[] = [];
    const snapshots: Json[] = [], motions: MotionFrame[] = [];
    const session = new Session({command: j => sent.push(j), ack: (r, m) => acks.push([r, m])}, {
        lobby: e => shown.push(`lobby:${e.message ?? ''}`), enter: () => shown.push('enter'),
        snapshot: s => snapshots.push(s), motion: f => motions.push(f), event: e => shown.push(`event:${e.type}`),
    });
    return {session, sent, acks, shown, snapshots, motions};
}

test('passwords are sent only where the server takes them', () => {
    const r = recorder();
    r.session.receiveEvent({type: 'lobby', stage: 'login', ok: true, credentialsAllowed: false, message: 'hi'});
    r.session.submit({type: 'auth_login', username: 'ash', password: 'a long test password'});
    assert.equal(r.sent.length, 0, 'nothing sent');
    assert.match(r.shown.at(-1)!, /No credentials were sent/);
    r.session.receiveEvent({type: 'lobby', stage: 'login', ok: true, credentialsAllowed: true});
    r.session.submit({type: 'auth_login', username: 'ash', password: 'a long test password'});
    assert.equal(r.sent.length, 1);
    assert.ok(JSON.parse(r.sent[0]).commandId, 'every command carries an ID for its receipt');
    r.session.submit({type: 'move', x: 1, y: 0, commandId: 'mine'});
    assert.equal(JSON.parse(r.sent[1]).commandId, 'mine', 'an ID already given is kept');
});

const snapshotOf = (revision: number, generation = 0, extra: Json = {}): Json => ({
    revision, cellGeneration: generation, motionSession: 'm1', self: {id: 'player-ash'}, cell: {id: 'tavern'}, ...extra,
});

test('only the character being played, its session and newer revisions are shown', () => {
    const r = recorder();
    r.session.receiveSnapshot(snapshotOf(1));
    assert.equal(r.snapshots.length, 0, 'nothing before entering');
    r.session.receiveEvent({type: 'roleplay', text: 'too early'});
    assert.equal(r.shown.length, 0, 'no world events in the lobby');
    r.session.receiveEvent({type: 'entered', id: 'player-ash', motionSession: 'm1'});
    r.session.receiveSnapshot(snapshotOf(2));
    r.session.receiveSnapshot(snapshotOf(2));
    r.session.receiveSnapshot(snapshotOf(1));
    r.session.receiveSnapshot({...snapshotOf(3), self: {id: 'player-other'}});
    r.session.receiveSnapshot({...snapshotOf(4), motionSession: 'old'});
    assert.equal(r.snapshots.length, 1, 'repeats, older revisions, other characters and old sessions are dropped');
    assert.deepEqual(r.acks, [[2, false]]);
    r.session.receiveSnapshot(snapshotOf(5, 0, {sectionKeys: {'cell.rows': 'missing'}}));
    assert.deepEqual(r.acks.at(-1), [5, true], 'a snapshot missing a held part asks for everything');
    assert.equal(r.snapshots.length, 1);
});

test('motion frames follow the snapshot they belong with', () => {
    const r = recorder();
    r.session.receiveEvent({type: 'entered', id: 'player-ash', motionSession: 'm1'});
    r.session.receiveSnapshot(snapshotOf(10, 1));
    const f = (revision: number, generation = 1, cellId = 'tavern'): MotionFrame =>
        ({...frame, motionSession: 'm1', observer: 'player-ash', cellId, cellGeneration: generation, revision});
    r.session.receiveMotion(f(11));
    r.session.receiveMotion(f(11));
    r.session.receiveMotion(f(9));
    r.session.receiveMotion({...f(12), observer: 'player-other'});
    assert.equal(r.motions.length, 1, 'repeats, older frames and other observers are dropped');
    r.session.receiveMotion(f(13, 2, 'exterior'));
    assert.equal(r.motions.length, 1, 'a newer cell waits for its snapshot');
    r.session.receiveSnapshot(snapshotOf(12, 1));
    assert.equal(r.snapshots.length, 1, 'an older generation than one seen in motion is dropped');
    r.session.receiveSnapshot({...snapshotOf(14, 2), cell: {id: 'exterior'}});
    r.session.receiveMotion(f(15, 2, 'exterior'));
    assert.equal(r.motions.length, 2);
});

test('entering again in a new session forgets the old one; losing the connection shows the lobby', () => {
    const r = recorder();
    r.session.receiveEvent({type: 'entered', id: 'player-ash', motionSession: 'm1'});
    r.session.receiveSnapshot(snapshotOf(50));
    r.session.receiveEvent({type: 'entered', id: 'player-ash', motionSession: 'm2'});
    r.session.receiveSnapshot({...snapshotOf(1), motionSession: 'm2'});
    assert.equal(r.snapshots.length, 2, 'revisions start again in a new session');
    r.session.connectionLost();
    assert.equal(r.session.enteredWorld, false);
    assert.match(r.shown.at(-1)!, /connection to the world server was lost/);
});
