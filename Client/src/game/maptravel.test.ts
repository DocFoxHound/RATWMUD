// Map travel and turning to face (the user, 2026-11-20): a click on the minimap or the World Map travels there, all the
// way, by the roads (World::travelToPoint), with the way drawn on both maps; Alt+click turns the wolf anywhere in the
// world, not only in a fight, without moving it.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {fakeContext, testGame} from './testing.ts';
import {MapRenderer, travelLine} from './minimap.ts';
import {Walker} from './walker.ts';
import {rect} from '../ui/painter.ts';
import type {Json} from './json.ts';

const close = (a: number, b: number, e = 1e-6) => Math.abs(a - b) < e;

function outdoors() {
    const g = testGame();
    g.state.applySnapshot({cell: {id: 'field', width: 64, height: 32, outdoors: true, rows: Array(32).fill(','.repeat(64))},
        worldMap: [{id: 'field', name: 'Field', x: 256, y: 512, width: 64, height: 32, knowledge: 'visited', current: true}],
        self: {id: 'self', x: 10.5, y: 8.5}, entities: [{id: 'self', x: 10.5, y: 8.5}], doors: [], time: 0, cellGeneration: 1} as Json);
    g.state.tick(1, 0.016);
    return g;
}

test('a click on the World Map travels there, in world tiles; a drag only pans it', () => {
    const {state: s, commands, painter} = outdoors();
    s.worldMap = true;
    painter.paint(800, 600);
    s.worldScale = 2;
    s.worldPan = [0, 0];
    painter.paint(800, 600);
    // 100 px right and 40 px up of the middle, at two pixels a tile: 50 tiles east and 20 north of the wolf.
    s.mouseDown([500, 260], true, false, false);
    s.mouseUp();
    const sent = commands.filter(c => c.type === 'travel_to');
    assert.equal(sent.length, 1, 'one journey asked for');
    assert.ok(close(Number(sent[0].x), 256 + 10.5 + 50, .051) && close(Number(sent[0].y), 512 + 8.5 - 20, .051),
        `to the spot under the pointer: ${sent[0].x}, ${sent[0].y}`);
    s.mouseDown([500, 260], true, false, false);
    s.mouseMove([540, 300], false);
    s.mouseUp();
    assert.equal(commands.filter(c => c.type === 'travel_to').length, 1, 'a drag pans, and asks for nothing');
    s.modal = 'character';
    s.travelToSpot(300, 500);
    s.modal = '';
    s.battle = {} as typeof s.battle;
    s.travelToSpot(300, 500);
    s.battle = null;
    assert.equal(commands.filter(c => c.type === 'travel_to').length, 1, 'not behind a sheet, nor in a fight');
});

test('the minimap turns a click back into world tiles, and both maps draw the way and where it ends', () => {
    const {state: s} = outdoors();
    const renderer = new MapRenderer(() => null);
    const ctx = fakeContext();
    const strokes: [number, number][] = [];
    (ctx as unknown as Record<string, unknown>).lineTo = (x: number, y: number) => strokes.push([x, y]);
    renderer.draw(ctx, s, {x: 0, y: 0, w: 200, h: 160}, 2);
    const at = renderer.toWorld(150, 30)!;
    assert.ok(close(at[0], 256 + 10.5 + 25) && close(at[1], 512 + 8.5 - 25), `a click 50 px right, 50 up: ${at}`);
    const onWay = ([x, y]: [number, number]) => close(x, 100 + 13.5 * 2) && close(y, 80 - .5 * 2);
    assert.ok(!strokes.some(onWay), 'no journey, no line');
    s.snapshot = {...s.snapshot, travel: {active: true, status: 'Travelling', goal: [300, 520], line: [266.5, 520.5, 280, 520, 300, 520]}} as Json;
    const way = travelLine(s.snapshot);
    assert.deepEqual(way.goal, [300, 520]);
    assert.equal(way.line.length, 3, 'the way, as pairs of world tiles');
    renderer.draw(ctx, s, {x: 0, y: 0, w: 200, h: 160}, 2);
    // From the wolf (the box's middle) along the way: (280, 520) is 13.5 tiles east and half a tile north of the wolf.
    assert.ok(strokes.some(onWay), 'the way is drawn where it lies');
    s.snapshot = {...s.snapshot, travel: {active: false, goal: [300, 520], line: [266.5, 520.5, 300, 520]}} as Json;
    assert.equal(travelLine(s.snapshot).goal, null, 'an ended journey draws nothing');
});

function facingSetup() {
    const g = testGame();
    const self: Json = {id: 'player', x: 5, y: 5, facing: Math.PI / 2, moving: false};
    g.state.applySnapshot({self, cell: {id: 'test_cell', width: 32, height: 24, outdoors: true},
        entities: [{id: 'player', x: 5, y: 5, facing: Math.PI / 2, moving: false}]});
    g.state.mapOrigin = [600, 200];
    g.state.mapRect = rect(0, 0, 1600, 900);
    g.state.tileSize = 20;
    return g;
}

test('Alt+click turns toward the ground round the cell too, past a seam', () => {
    const {state: s, commands} = facingSetup();
    s.fringe = 8;
    const west: [number, number] = [600 - 3 * 20, 300];      // Three tiles past the cell's west edge.
    s.mouseMove(west, true);
    assert.ok(s.facingPreview, 'previewed over the ground beyond the edge');
    s.mouseDown(west, true, true, false);
    assert.deepEqual(commands.at(-1), {type: 'face', x: -3, y: 5}, 'and turned toward it');
    const far: [number, number] = [600 - 12 * 20, 300];
    const before = commands.length;
    s.mouseDown(far, true, true, false);
    assert.equal(commands.length, before, 'past the ground shown, nothing');
});

const wasm = readFileSync(new URL('../wasm/walk.wasm', import.meta.url));
const rows = Array(6).fill('............');

test('Alt+click in free movement: the wolf turns at once, where it stands, and the page tells the server by its pose', async () => {
    const {state: s, commands} = testGame();
    s.walker = await Walker.load(wasm);
    const self: Json = {id: 'self', x: 2.5, y: 2.5, posture: 'standing', walkSpeed: 2.6, moveFactor: 1};
    s.applySnapshot({cell: {id: 'room', width: 12, height: 6, rows, outdoors: true}, self, entities: [], doors: [], time: 0, cellGeneration: 1});
    s.applyMotion({motionSession: '', observer: 'self', cellId: 'room', cellGeneration: 1, revision: 0, time: 0.05,
        entities: [{id: 'self', x: 2.5, y: 2.5, facing: 0, moving: false}], mode: 0, inputAck: 0, poseAck: 0});
    s.tick(1, 0.016);
    assert.ok(s.freeWalking(), 'the page walks its own wolf');
    s.mapOrigin = [100, 100];
    s.mapRect = rect(0, 0, 800, 600);
    s.tileSize = 20;
    s.navigationFocus = true;
    const south: [number, number] = [100 + 2.5 * 20, 100 + 5.5 * 20];   // Straight south of the wolf.
    s.mouseMove(south, true);
    s.mouseDown(south, true, true, false);
    assert.ok(!commands.some(c => c.type === 'face' || c.type === 'path'), 'no turn asked of the server, and no walk');
    assert.ok(close(s.entities.get('self')!.facing, Math.PI / 2), 'turned to face south at once');
    s.tick(1.1, 0.016);
    const pose = commands.filter(c => c.type === 'pose').at(-1);
    assert.ok(pose && close(Number(pose.facing), Math.PI / 2) && close(Number(pose.x), 2.5) && close(Number(pose.y), 2.5) && pose.ix === 0,
        'its next pose says so, standing where it was');
    s.tick(1.3, 0.016);
    assert.ok(close(s.entities.get('self')!.facing, Math.PI / 2), 'and it keeps facing there');
});
