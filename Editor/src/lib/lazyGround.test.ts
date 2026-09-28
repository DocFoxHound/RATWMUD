// A world loaded lean: cells wait for their ground, nothing may change it meanwhile, and others' edits there are
// made again once it arrives, whatever order things come in.
import test from 'node:test';
import assert from 'node:assert/strict';
import * as M from '../model/model.mjs';
import {applyChanges, diff} from './live.ts';
import {adoptLean, allGround, blockedBy, decodeHeights, forHost, groundPending, lateFor, noteRemote, resetGround,
    settleCell, startGround, want, type Ground} from './lazyGround.ts';

// A world of four 32 × 24 cells, as the host would send it lean (cells without ground) and whole.
function whole() {
    const p = M.createProject(64, 48, 'Test world');
    M.cutGrid(p, 32, 24);
    M.paint(p, 3, 4, '~');
    M.setHeight(p, 3, 4, 1.5);
    M.paint(p, 40, 30, '#');
    return p;
}
function lean(p: M.Project) {
    const sent = M.clone(p) as unknown as {cells: {id: string; width: number; height: number; terrain: unknown; heights: unknown; preview?: string[]}[]};
    for (const c of sent.cells) { c.terrain = null; c.heights = null; c.preview = ['..', '..']; }
    return M.normalizeProject(adoptLean(sent), true);
}
const cellAt = (p: M.Project, x: number, y: number) => p.cells.find(c => x >= c.x && y >= c.y && x < c.x + c.width && y < c.y + c.height)!;
const ground = (p: M.Project, id: string): Ground => { const c = p.cells.find(c => c.id === id)!; return {x: c.x, y: c.y, width: c.width, height: c.height, terrain: c.terrain, heights: c.heights}; };
function install(p: M.Project, g: Map<string, Ground>, seq: number) {
    const cells = p.cells.map(c => { const x = g.get(c.id); if (!x) return c; settleCell(c.id); return M.withGround(c, x.terrain, x.heights); });
    const next = {...p, cells};
    applyChanges(next, lateFor(cells.filter(c => g.has(c.id)), seq));
    return next;
}

test('heights travel as rows of one character a tile', () => {
    assert.deepEqual(decodeHeights(['..V', 'W.0', 'g']), {'2,0': -0.5, '0,1': 0, '2,1': -16, '0,2': 5});
    assert.deepEqual(decodeHeights(['*']), {'0,0': 16});
});

test('a lean world waits for its ground, and is sent to the host without it', () => {
    const truth = whole(), p = lean(truth);
    const first = cellAt(p, 3, 4);
    assert.ok(groundPending(first.id));
    assert.equal(first.terrain[4][3], '.', 'placeholder ground until it arrives');
    const sent = forHost(p);
    assert.equal((sent.cells[0] as unknown as {terrain: unknown}).terrain, null, 'the host fills in what never came');
    const next = install(p, new Map([[first.id, ground(truth, first.id)]]), 0);
    assert.ok(!groundPending(first.id));
    assert.equal(cellAt(next, 3, 4).terrain[4][3], '~');
    assert.equal(cellAt(next, 3, 4).heights['3,4'], 1.5);
    resetGround();
});

test('nothing may change or rely on ground that has not arrived', () => {
    const truth = whole(), p = lean(truth), waiting = cellAt(p, 40, 30);
    const tried = (change: (d: M.Project) => void) => { const d = M.clone(p); change(d); return blockedBy(p, d, diff(p, d)); };
    assert.deepEqual(tried(d => M.paint(d, 40, 30, '~')), [waiting.id], 'painting it');
    assert.deepEqual(tried(d => M.setHeight(d, 41, 31, 2)), [waiting.id], 'raising it');
    assert.deepEqual(tried(d => M.setSpawn(d, {cell: waiting.id, x: 2, y: 2})), [waiting.id], 'placing something on it');
    assert.deepEqual(tried(d => M.updateCell(d, waiting.id, {name: 'Renamed'})), [], 'renaming it is fine');
    // Once it is here, anything goes.
    const here = install(p, new Map([[waiting.id, ground(truth, waiting.id)]]), 0);
    const d = M.clone(here); M.paint(d, 40, 30, '~');
    assert.deepEqual(blockedBy(here, d, diff(here, d)), []);
    resetGround();
});

test("others' edits to waiting ground are made again once it arrives, if newer than it", () => {
    const truth = whole(), p = lean(truth), id = cellAt(p, 3, 4).id;
    // Edits 7 and 9 arrive while the cell waits; its ground then comes as of edit 8 (so it has 7 but not 9).
    const changes = [{seq: 7, key: 'tile:3,4', after: 'T'}, {seq: 9, key: 'tile:5,5', after: '#'}];
    const after = {...p}; applyChanges(after, changes); noteRemote(p, after, changes);
    const g = ground(truth, id); g.terrain = g.terrain.map((row, y) => (y === 4 ? row.slice(0, 3) + 'T' + row.slice(4) : row));
    const next = install(after, new Map([[id, g]]), 8);
    assert.equal(cellAt(next, 3, 4).terrain[4][3], 'T', 'edit 7 is in the ground as sent');
    assert.equal(cellAt(next, 5, 5).terrain[5][5], '#', 'edit 9 is made again on it');
    resetGround();
});

test('ground is asked for a few cells at a time, and allGround waits for the last', async () => {
    const truth = whole(), p = lean(truth);
    let current = p;
    const asked: string[][] = [];
    startGround(async ids => {
        asked.push(ids);
        return {seq: 1, cells: Object.fromEntries(ids.map(id => [id, {...ground(truth, id), heights: ground(truth, id).heights}]))};
    }, (arrived, seq) => { current = install(current, arrived, seq); return [...arrived.keys()]; });
    want(p.cells[0].id); want(p.cells[0].id);
    await allGround(5000);
    assert.ok(p.cells.every(c => !groundPending(c.id)));
    assert.equal(asked.flat().filter(id => id === p.cells[0].id).length, 1, 'asked for once');
    assert.equal(cellAt(current, 40, 30).terrain[6][8], '#');
    resetGround();
});
