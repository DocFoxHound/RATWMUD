// The page's own walking (Docs/Design/31-responsiveness.md, Phase 3): the server's rules as WebAssembly
// (Client/src/wasm/walk.wasm, from Core/RatwStep.cpp), and the game state walking its own wolf with them.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {Walker} from './walker.ts';
import {testGame} from './testing.ts';
import type {Json} from './json.ts';

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
