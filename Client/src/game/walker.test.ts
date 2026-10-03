// The page's own walking (Docs/Design/31-responsiveness.md, Phase 3): the server's rules as WebAssembly
// (Client/src/wasm/walk.wasm, from Core/RatwStep.cpp), and the game state walking its own wolf with them.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {Walker} from './walker.ts';
import {testGame} from './testing.ts';
import type {Json} from './json.ts';
import {heightFromChar} from './labels.ts';

const wasm = readFileSync(new URL('../wasm/walk.wasm', import.meta.url));
const close = (a: number, b: number, e = 1e-9) => Math.abs(a - b) < e;

// A 12x6 room: floor ('.'), a wall ('#') down column 6, scree ('s', twice as slow... as its catalog cost says),
// a closed door at (9, 1).
const rows = [
    '............',
    '......#.....',
    '......#.....',
    '......#.....',
    '............',
    '............',
];

test('the walker walks as the server does: speed, walls, sliding, closed doors and heavy ground', async () => {
    const walker = await Walker.load(wasm);
    assert.ok(walker, 'the module loads');
    walker.setCell(12, 6, rows, new Float32Array(72), [{x: 9.5, y: 1.5, open: false}]);
    let s = walker.step(2.5, 2.5, 1, 0, 2.6, false, 1, 0.1);
    assert.ok(close(s.x, 2.76) && close(s.y, 2.5) && !s.blocked, 'a tenth of a second at a walk on floor: 0.26 tiles');
    s = walker.step(2.5, 2.5, 1, 0, 2.6, true, 1, 0.1);
    assert.ok(close(s.x, 2.5 + 0.26 * 0.3), 'crouched, three tenths as fast');
    s = walker.step(2.5, 2.5, 1, 0, 2.6, false, 0.5, 0.1);
    assert.ok(close(s.x, 2.63), 'heavy weather halves it');
    s = walker.step(5.8, 2.5, 1, 0, 2.6, false, 1, 0.1);
    assert.ok(s.blocked && s.x < 6 - 0.065 + 1e-9, 'a wall stops it at its footprint');
    s = walker.step(5.8, 2.5, 1, 1, 2.6, false, 1, 0.1);
    assert.ok(s.blocked && close(s.x, 5.8) && s.y > 2.5, 'walking at a wall slantwise slides along it');
    assert.ok(!walker.passable(9.5, 1.5, 9.5, 2.5), 'a closed door is not stood in');
    walker.setCell(12, 6, rows, new Float32Array(72), [{x: 9.5, y: 1.5, open: true}]);
    assert.ok(walker.passable(9.5, 1.5, 9.5, 2.5), 'an open one is');
    const steep = new Float32Array(72);
    steep[2 * 12 + 3] = 2;                     // A ledge two high at (3, 2).
    walker.setCell(12, 6, rows, steep, []);
    assert.ok(!walker.passable(3.5, 2.5, 2.5, 2.5), 'a ledge too high is not climbed');
    const scree = rows.map((r, y) => (y === 4 ? 'ssssssssssss' : r));
    walker.setCell(12, 6, scree, new Float32Array(72), []);
    s = walker.step(2.5, 4.5, 1, 0, 2.6, false, 1, 0.1);
    assert.ok(close(s.x, 2.5 + 0.26 / 1.6, 1e-6), 'scree slows it by its cost (stored as a 32-bit float: a billionth off)');
});

test("free movement: the page walks its own wolf at once, says where it is, and goes back when corrected or held", async () => {
    const {state: s, commands} = testGame();
    const walker = await Walker.load(wasm);
    s.walker = walker;
    const self: Json = {id: 'self', x: 2.5, y: 2.5, posture: 'standing', walkSpeed: 2.6, moveFactor: 1};
    s.applySnapshot({cell: {id: 'room', width: 12, height: 6, rows}, self, entities: [], doors: [], time: 0, cellGeneration: 1});
    assert.ok(commands.some(c => c.type === 'walking' && c.mode === 'client'), 'it asks to walk its own wolf');
    const frame = (time: number, mode: number) => ({motionSession: '', observer: 'self', cellId: 'room', cellGeneration: 1,
        revision: 0, time, entities: [{id: 'self', x: 2.5, y: 2.5, facing: 0, moving: false}], mode, inputAck: 0, poseAck: 0});
    s.applyMotion(frame(0.05, 0));
    s.tick(1, 0.016);
    s.heldKeys.add('KeyD');
    s.tick(1.016, 0.1);
    const view = s.entities.get('self')!;
    assert.ok(close(view.x, 2.76), 'it moves on this very frame, before the server hears of it');
    const poses = commands.filter(c => c.type === 'pose');
    assert.ok(poses.length === 1 && close(poses[0].x as number, 2.76) && poses[0].ix === 1, 'and says where it is, and which way');
    assert.ok(!commands.some(c => c.type === 'move'), 'no keys are sent: the page walks');
    s.tick(1.04, 0.024);
    assert.equal(commands.filter(c => c.type === 'pose').length, 1, 'no more than twenty poses a second');
    s.tick(1.07, 0.03);
    assert.equal(commands.filter(c => c.type === 'pose').length, 2, 'then the next');
    s.receiveEvent({type: 'correction', cellId: 'room', x: 2.5, y: 2.5, facing: 0, seq: 0});
    s.heldKeys.clear();
    for (let t = 1.1; t < 1.5; t += 0.016) s.tick(t, 0.016);
    assert.ok(close(view.x, 2.5, 1e-3), 'corrected, it goes back to where the server says it is');
    // Held (around a fight, or along a route): the server walks it, and the keys go to the server again.
    s.applyMotion(frame(0.1, 1));
    s.heldKeys.add('KeyD');
    const before = commands.filter(c => c.type === 'pose').length;
    s.tick(2, 0.1);
    assert.equal(commands.filter(c => c.type === 'pose').length, before, 'held: no poses');
    assert.ok(commands.some(c => c.type === 'move' && c.x === 1 && typeof c.seq === 'number'), 'the keys, numbered, to the server');
});

test('held: the page predicts the server walking its wolf with the same rules, so it never draws it through a wall', async () => {
    const {state: s} = testGame();
    s.walker = await Walker.load(wasm);
    const self: Json = {id: 'self', x: 5.5, y: 2.5, posture: 'standing', walkSpeed: 2.6, moveFactor: 1};
    s.applySnapshot({cell: {id: 'room', width: 12, height: 6, rows}, self, entities: [], doors: [], time: 0, cellGeneration: 1});
    const frame = (time: number, x: number) => ({motionSession: '', observer: 'self', cellId: 'room', cellGeneration: 1,
        revision: 0, time, entities: [{id: 'self', x, y: 2.5, facing: 0, moving: true}], mode: 1, inputAck: 0, poseAck: 0});
    s.applyMotion(frame(0.05, 5.5));
    s.heldKeys.add('KeyD');
    let t = 1;
    for (let i = 0; i < 30; ++i) {
        s.tick(t += 0.016, 0.016);
        if (i === 10) s.applyMotion(frame(0.1, 5.7));
    }
    const view = s.entities.get('self')!;
    assert.ok(view.x > 5.7 && view.x <= 6 - 0.065 + 1e-6, `ahead of the server's pose, but stopped at the wall: ${view.x}`);
});

test('partial motion frames: a far wolf left out is kept; a near one left out has gone from sight; a full frame decides', () => {
    const {state: s} = testGame();
    s.applySnapshot({cell: {id: 'room', width: 100, height: 20}, self: {id: 'self', x: 2, y: 2},
        entities: [{id: 'near', x: 5, y: 2}, {id: 'far', x: 60, y: 2}], time: 0, cellGeneration: 1});
    const frame = (time: number, partial: boolean, ids: string[]) => ({motionSession: '', observer: 'self', cellId: 'room', cellGeneration: 1,
        revision: 0, time, partial, entities: [{id: 'self', x: 2, y: 2, facing: 0, moving: false},
            ...ids.map(id => ({id, x: id === 'near' ? 5 : 60, y: 2, facing: 0, moving: false}))]});
    s.applyMotion(frame(0.05, false, ['near', 'far']));
    s.tick(1, 0.016);
    s.applyMotion(frame(0.1, true, ['near']));
    assert.ok(s.entities.has('far'), 'a far wolf a partial frame leaves out is kept');
    s.applyMotion(frame(0.15, true, []));
    assert.ok(!s.entities.has('near') && s.entities.has('far'), 'a near wolf left out has gone from sight, at once');
    s.applyMotion(frame(0.2, false, []));
    assert.ok(!s.entities.has('far'), 'and a full frame without the far one says it has gone too');
});

test("the page's sight is the server's rule: walls and closed doors hide what lies beyond, and the range ends it", async () => {
    const walker = (await Walker.load(wasm))!;
    // The room: a wall down column 6 (rows 1 to 3), a closed door at (9, 1).
    walker.setCell(12, 6, rows, new Float32Array(72), [{x: 9.5, y: 1.5, open: false}]);
    const seen = walker.sight(2.5, 2.5, 27);
    const at = (x: number, y: number) => seen[y * 12 + x];
    assert.equal(at(3, 2), 1, 'open floor beside it');
    assert.equal(at(5, 2), 1, 'up to the wall');
    assert.equal(at(6, 2), 1, 'the wall itself');
    assert.equal(at(8, 2), 0, 'not past it');
    assert.equal(at(8, 5), 1, 'around it, where nothing stands between');
    const near = walker.sight(8.5, 2.5, 27);
    assert.equal(near[1 * 12 + 9], 1, 'a closed door is seen');
    assert.equal(near[0 * 12 + 9], 0, 'but not through');
    const short = walker.sight(2.5, 2.5, 2);
    assert.equal(short[2 * 12 + 4], 1, 'within its range');
    assert.equal(short[2 * 12 + 5], 0, 'not beyond it');
});

test('terrain shading on the page: seen, remembered and unknown worked out here, the server sending no visibility', async () => {
    const {state: s, commands} = testGame();
    s.walker = await Walker.load(wasm);
    const known = rows.map((r, y) => (y === 5 ? '            ' : r));    // The bottom row unknown: blank glyphs.
    s.applySnapshot({cell: {id: 'room', width: 12, height: 6, rows: known}, self: {id: 'self', x: 2.5, y: 2.5, posture: 'standing', sightRange: 27},
        entities: [], doors: [], time: 0, cellGeneration: 1});
    assert.ok(commands.some(c => c.type === 'walking' && c.sight === 'client'), 'it asks to shade the terrain itself');
    s.applyMotion({motionSession: '', observer: 'self', cellId: 'room', cellGeneration: 1, revision: 0, time: 0.05,
        entities: [{id: 'self', x: 2.5, y: 2.5, facing: 0, moving: false}], mode: 0});
    s.tick(1, 0.016);
    assert.equal(s.visibilityRows[2][3], '2', 'floor in sight: seen');
    assert.equal(s.visibilityRows[2][8], '1', 'past the wall: remembered');
    assert.equal(s.visibilityRows[5][3], '0', 'never sent: unknown');
});

test("the page sees exactly what the server sees: the demo world's exterior (Client/src/game/sight.golden.json)", async () => {
    const golden = JSON.parse(readFileSync(new URL('./sight.golden.json', import.meta.url), 'utf8')) as {width: number; height: number;
        x: number; y: number; range: number; rows: string[]; heights: string[]; doors: {x: number; y: number; open: boolean}[]; seen: string[]};
    const walker = (await Walker.load(wasm))!;
    const heights = new Float32Array(golden.width * golden.height);
    golden.heights.forEach((row, y) => [...row].forEach((c, x) => { heights[y * golden.width + x] = heightFromChar(c); }));
    walker.setCell(golden.width, golden.height, golden.rows, heights, golden.doors);
    const seen = walker.sight(golden.x, golden.y, golden.range);
    let differ = 0;
    golden.seen.forEach((row, y) => [...row].forEach((c, x) => { if ((seen[y * golden.width + x] ? '1' : '0') !== c) ++differ; }));
    assert.equal(differ, 0, `${differ} tiles seen differently from the server`);
});

test("the walker: a Chapter's built structure stands in the way, as on the server (doc 32, 5.7)", async () => {
    const walker = (await Walker.load(wasm))!;
    const rows = ['..........', '..........', '..........'];
    walker.setCell(10, 3, rows, new Float32Array(30), []);
    assert.ok(walker.passable(5.5, 1.5, 4.5, 1.5), 'open ground');
    walker.setCell(10, 3, rows, new Float32Array(30), [], new Set(['5,1']));
    assert.ok(!walker.passable(5.5, 1.5, 4.5, 1.5), 'a tent there: not passable');
});

test("bodies: the page's own wolf walks into another and is held off by it, never passing through", async () => {
    const {state: s} = testGame();
    s.walker = await Walker.load(wasm);
    s.applySnapshot({cell: {id: 'room', width: 12, height: 6, rows}, self: {id: 'self', x: 1.5, y: 4.5, posture: 'standing', walkSpeed: 2.6, moveFactor: 1},
        entities: [{id: 'other', x: 3.5, y: 4.5}], doors: [], time: 0, cellGeneration: 1});
    const frame = (time: number) => ({motionSession: '', observer: 'self', cellId: 'room', cellGeneration: 1, revision: 0, time,
        entities: [{id: 'self', x: 1.5, y: 4.5, facing: 0, moving: false}, {id: 'other', x: 3.5, y: 4.5, facing: 0, moving: false}], mode: 0, inputAck: 0, poseAck: 0});
    s.applyMotion(frame(0.05));
    s.tick(1, 0.016);
    s.heldKeys.add('KeyD');
    let closest = 9;
    for (let t = 1.016; t < 3; t += 0.016) {
        s.tick(t, 0.016);
        closest = Math.min(closest, s.entities.get('other')!.x - s.entities.get('self')!.x);
    }
    assert.ok(closest > 0.4 && closest < 0.5, `pressed up against it, a body apart: ${closest}`);
});
