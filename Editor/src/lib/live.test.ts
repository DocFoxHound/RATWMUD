// Every edit an editor makes must reproduce exactly on everyone else's copy:
// applying diff(a, b) to a copy of a gives b, and the inverse gives a back.
import test, {mock} from 'node:test';
import assert from 'node:assert/strict';
import * as M from '../model/model.mjs';
import {againWithChoices, applyChanges, diff, inverse, queueRefusal, stable, startLive, type Batch, type Change, type ConflictNotice} from './live.ts';

const world = () => { const p = M.createProject(64, 48, 'Test world'); M.cutGrid(p, 32, 24); return p; };
const glyphAt = (p: M.Project, x: number, y: number) => {
    const c = p.cells.find(c => x >= c.x && y >= c.y && x < c.x + c.width && y < c.y + c.height);
    return c?.terrain[y - c.y][x - c.x];
};

// Edits carry no list order (the database keeps its own), so cells are compared in ID order.
const same = (p: M.Project) => stable({...p, cells: [...p.cells].sort((x, y) => x.id.localeCompare(y.id))});

function roundTrip(a: M.Project, change: (p: M.Project) => void) {
    const b = M.clone(a);
    change(b);
    const ops = diff(a, b);
    const there = M.clone(a); applyChanges(there, ops);
    assert.equal(same(there), same(b), 'applying the edits reproduces the change');
    const back = M.clone(b); applyChanges(back, inverse(ops));
    assert.equal(same(back), same(a), 'the inverse edits undo it');
    return ops;
}

test('terrain edits are single tiles in world coordinates', () => {
    const a = world();
    const ops = roundTrip(a, p => { M.paint(p, 3, 4, '~'); M.setHeight(p, 3, 4, 1); });
    assert.deepEqual(ops.map(o => o.key).sort(), ['height:3,4', 'tile:3,4']);
    assert.equal(ops.find(o => o.key === 'tile:3,4')!.before, null, 'plain ground travels as null');
});

test('re-cutting, splitting and merging cells moves no ground: no tile edits, only cell edits', () => {
    const a = world(); M.paint(a, 10, 10, '#'); M.setHeight(a, 11, 10, 1);
    const cut = roundTrip(a, p => { M.cutGrid(p, 16, 16); });
    assert.ok(cut.length && cut.every(o => o.key.startsWith('cell:') || ['spawn', 'link'].some(k => o.key.startsWith(k))), cut.map(o => o.key).join(' '));
    const b = M.clone(a); applyChanges(b, cut);
    assert.equal(glyphAt(b, 10, 10), '#', 'the other editor rebuilds each cell’s ground from the world');
    roundTrip(b, p => { M.splitCell(p, p.cells[0].id, 'x', 8); });
});

test('cells, interiors and their tiles', () => {
    const a = world();
    roundTrip(a, p => { M.addCell(p, {x: 64, y: 0, width: 32, height: 24}, 'East'); });
    const withRoom = M.clone(a); const room = M.addRoom(withRoom, 8, 6, 'Cellar'); M.paint(withRoom, 2, 2, 'T', 1, room.id);
    const created = roundTrip(a, p => { const r = M.addRoom(p, 8, 6, 'Cellar'); M.paint(p, 2, 2, 'T', 1, r.id); });
    assert.ok(created.some(o => o.key.startsWith('room:')) && created.some(o => o.key === `rtile:${room.id}:2,2`));
    roundTrip(withRoom, p => M.resizeRoom(p, room.id, 12, 4));
    roundTrip(withRoom, p => M.removeRoom(p, room.id));
});

test('a far-away cell and its deletion travel as cell and tile edits', () => {
    const a = world();
    const made = roundTrip(a, p => { const c = M.addCell(p, {x: -9_000_000, y: 40_000, width: 16, height: 12}, 'Far'); M.paint(p, c.x + 2, c.y + 3, '~'); });
    assert.deepEqual(made.map(o => o.key.replace(/^cell:.*/, 'cell')).sort(), ['cell', 'tile:-8999998,40003']);
    const b = M.clone(a); applyChanges(b, made);
    assert.equal(glyphAt(b, -8999998, 40003), '~');
    const far = b.cells.find(c => c.name === 'Far')!;
    const gone = roundTrip(b, p => M.removeCell(p, far.id));
    assert.ok(gone.some(o => o.key === 'tile:-8999998,40003' && o.after === null), 'its ground is cleared in the database too');
});

test('applying edits leaves an older version that shares cells untouched', () => {
    const a = world(), snapshot = stable(a);
    const b = M.clone(a); M.paint(b, 3, 3, '~');
    const c = {...a}; applyChanges(c, diff(a, b));
    assert.equal(glyphAt(c, 3, 3), '~');
    assert.equal(stable(a), snapshot);
});

test('people, routes and the world settings', () => {
    const a = world();
    const person = M.newPerson(a, 'guard', {cell: a.cells[0].id, x: 5, y: 5});
    roundTrip(a, p => { M.upsertPerson(p, person); });
    const b = M.clone(a); M.upsertPerson(b, person);
    roundTrip(b, p => { M.upsertRoute(p, {id: 'watch', name: 'Watch', posts: [{cell: p.cells[0].id, x: 6, y: 6}]}); });
    roundTrip(b, p => { p.name = 'Renamed'; M.setSpawn(p, {cell: p.cells[1].id, x: 3, y: 3}); });
});

test("a batch the game's changes refused is made again on the game's values, with the editor's choices", () => {
    const seen = {id: 'maple', name: 'Maple', greeting: 'Hi.', home: {cell: 'longhouse', x: 17, y: 9}};
    const now = {...seen, home: {cell: 'cottage', x: 3, y: 3}};
    const after = {...seen, greeting: 'Hello there.', home: {cell: 'shop', x: 1, y: 1}};
    const notice = {action: 'Move Maple', message: '', ops: [{key: 'person:maple', before: seen, after}, {key: 'name', before: 'W', after: 'V'}],
        current: {'person:maple': now, name: 'W'},
        conflicts: [{key: 'person:maple', label: 'NPC Maple', editor: 'The game', at: null, game: true,
            fields: [{field: 'home', base: seen.home, mine: after.home, game: now.home}]}]};
    assert.deepEqual(againWithChoices(notice, {}), [{key: 'person:maple', before: now, after: {...now, greeting: 'Hello there.'}},
        {key: 'name', before: 'W', after: 'V'}], "the game's home kept, the rest of the edit made");
    assert.deepEqual(againWithChoices(notice, {'person:maple': {home: true}})?.[0].after, {...now, greeting: 'Hello there.', home: after.home});
    assert.equal(againWithChoices({...notice, conflicts: [{...notice.conflicts[0], game: false}]}, {}), null, "another editor's: not made again");
});

test("a key the host merged with the game's change is made again on the game's row, not the editor's stale one", () => {
    // Rook's home both changed (the conflict); Maple, in the same batch, the game moved and the host merged.
    const maple = {id: 'maple', greeting: 'Hi.', home: {cell: 'longhouse', x: 17, y: 9}}, rook = {id: 'rook', greeting: 'Yo.', home: null};
    const mapleNow = {...maple, home: {cell: 'cottage', x: 3, y: 3}}, rookNow = {...rook, home: {cell: 'shop', x: 1, y: 1}};
    const notice: ConflictNotice = {action: 'Greet', message: '', current: {'person:maple': mapleNow, 'person:rook': rookNow},
        ops: [{key: 'person:maple', before: maple, after: {...maple, greeting: 'Hello.'}},
              {key: 'person:rook', before: rook, after: {...rook, home: {cell: 'barracks', x: 2, y: 2}}}],
        conflicts: [{key: 'person:rook', label: 'NPC Rook', editor: 'The game', at: null, game: true,
            fields: [{field: 'home', base: null, mine: {cell: 'barracks', x: 2, y: 2}, game: rookNow.home}]}]};
    for (const mine of [{}, {'person:rook': {home: true}}]) {
        const again = againWithChoices(notice, mine)!;
        assert.deepEqual(again[0], {key: 'person:maple', before: mapleNow, after: {...mapleNow, greeting: 'Hello.'}}, "the game's home kept");
    }
});

test('a refusal while another is open waits its turn', () => {
    const one = {action: 'One', message: '', conflicts: []}, two = {action: 'Two', message: '', conflicts: []};
    const first = queueRefusal(null, [], one);
    assert.deepEqual(first, {shown: one, waiting: []});
    assert.deepEqual(queueRefusal(first.shown, first.waiting, two), {shown: one, waiting: [two]});
});

test("the game's change to a key with an unsaved batch waits for it, and the newer of the two stands", async () => {
    mock.timers.enable({apis: ['setTimeout']});
    try {
        const flush = () => new Promise(resolve => setImmediate(resolve));
        for (const [reply, applied] of [[{seq: 9, merged: {'person:maple': {id: 'maple', greeting: 'Hello.', home: 'game'}}}, false],
                                        [{seq: 3}, true]] as const) {
            const beats: {seq: number; changes: Change[]; editors: never[]; gameSeq: number}[] = [
                {seq: 0, changes: [], editors: [], gameSeq: 0},
                {seq: 0, changes: [{seq: 5, clientId: 'game', editor: 'The game', label: 'Maple placed', key: 'person:maple',
                                    after: {id: 'maple', greeting: 'Hi.', home: 'game'}}], editors: [], gameSeq: 1}];
            let answer: (value: Response) => void = () => {};
            const remote: Change[][] = [], written: unknown[] = [];
            const host = {load: async () => ({project: {} as never, seq: 0}), sync: async () => beats.shift() ?? {seq: 0, changes: [], editors: []},
                edit: () => new Promise<Response>(resolve => { answer = resolve; })};
            const link = startLive(host, 0, {identity: () => ({name: 'Ada', color: '#aabbcc'}), presence: () => ({}), remote: c => remote.push(c),
                refused: () => {}, status: () => {}, editors: () => {}, reload: () => {}, written: v => written.push(v)}, 0);
            await flush();
            const batch: Batch = {id: 1, label: 'Greet', ops: [{key: 'person:maple', before: {id: 'maple', greeting: 'Hi.', home: null},
                after: {id: 'maple', greeting: 'Hello.', home: null}}]};
            link.push(batch);
            mock.timers.tick(1500);                      // (The heartbeat with the game's move, while the batch is unsaved.)
            await flush();
            assert.deepEqual(remote, [], "held: the editor's own values stand while its batch is out");
            answer(new Response(JSON.stringify(reply), {status: 200}));
            await flush(); await flush();
            assert.equal(remote.length, applied ? 1 : 0, applied ? 'newer than the batch: applied after it' : 'the merged reply is newer');
            assert.equal(written.length, applied ? 0 : 1);
            link.stop();
        }
    } finally {
        mock.timers.reset();
    }
});

test("a change older than what the tab holds of its key never replaces it: a held game change, a late reply, an edit in the game's", async () => {
    mock.timers.enable({apis: ['setTimeout']});
    try {
        const flush = () => new Promise(resolve => setImmediate(resolve));
        const change = (seq: number, clientId: string, greeting: string): Change =>
            ({seq, clientId, editor: clientId, label: 'Maple', key: 'person:maple', after: {id: 'maple', greeting}});
        // The batch (saved as edit 5) is out; the game's change, tagged 5, waits for it; another tab's edit 7 comes; the
        // batch's reply comes last. Then a game change tagged 9 and, a beat later, edit 9 it already holds.
        const beats = [[], [change(5, 'game', 'Game.')], [change(7, 'bo-tab', 'Bo.')], [change(9, 'game', 'Game, with Bo.')],
            [change(9, 'bo-tab', 'Bo again.'), change(10, 'bo-tab', 'Bo, newest.')]]
            .map((changes, i) => ({seq: i, changes, editors: [] as never[], gameSeq: i}));
        let answer: (value: Response) => void = () => {};
        const remote: string[] = [], written: unknown[] = [];
        const host = {load: async () => ({project: {} as never, seq: 0}), sync: async () => beats.shift() ?? {seq: 0, changes: [], editors: []},
            edit: () => new Promise<Response>(resolve => { answer = resolve; })};
        const link = startLive(host, 0, {identity: () => ({name: 'Ada', color: '#aabbcc'}), presence: () => ({}),
            remote: c => remote.push(...c.map(x => (x.after as {greeting: string}).greeting)),
            refused: () => {}, status: () => {}, editors: () => {}, reload: () => {}, written: v => written.push(v)}, 0);
        await flush();
        link.push({id: 1, label: 'Greet', ops: [{key: 'person:maple', before: {id: 'maple', greeting: 'Hi.'}, after: {id: 'maple', greeting: 'Ada.'}}]});
        for (let beat = 0; beat < 2; beat++) { mock.timers.tick(1500); await flush(); }
        assert.deepEqual(remote, ['Bo.'], "the game's waits; Bo's newer edit is taken");
        answer(new Response(JSON.stringify({seq: 5, merged: {'person:maple': {id: 'maple', greeting: 'Ada.'}}}), {status: 200}));
        await flush(); await flush();
        assert.deepEqual([remote, written], [['Bo.'], []], "neither the batch's rows nor the game's change, older, replace Bo's");
        for (let beat = 0; beat < 2; beat++) { mock.timers.tick(1500); await flush(); }
        assert.deepEqual(remote, ['Bo.', 'Game, with Bo.', 'Bo, newest.'], "edit 9 is in the game's change already");
        link.stop();
    } finally {
        mock.timers.reset();
    }
});
