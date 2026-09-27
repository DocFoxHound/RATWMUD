import test from 'node:test';
import assert from 'node:assert/strict';
import * as M from '../model/model.mjs';
import {placeCell} from './cellPlacement.ts';

// One 64×64 cell at world 0,0.
const world = () => { const p = M.createProject(64, 64, 'Test world'); M.cutGrid(p, 64, 64); return p; };

test('a new cell snaps flush against its neighbour, but slides freely along the shared edge', () => {
    const p = world();
    // Pointer a little east of the cell and a little low: pulled flush against its east side, and left where the
    // pointer put it along that side (no jump to line up the corners).
    const at = placeCell(p, 64 + 40, 30, {w: 64, h: 64});
    assert.deepEqual([at.x, at.y, at.ok, at.touching], [64, -2, true, true]);
    const half = placeCell(p, 64 + 20, 50, {w: 32, h: 32});
    assert.deepEqual([half.x, half.y, half.touching], [64, 34, true]);
    // Below: flush against the bottom edge, where the pointer is across.
    const below = placeCell(p, 10, 64 + 12, {w: 64, h: 32});
    assert.deepEqual([below.x, below.y], [-22, 64]);
});

test('dropped into a nook, a cell meets both neighbours', () => {
    const p = world();
    M.addCell(p, {x: 64, y: 0, width: 64, height: 64}, 'East');
    // The nook south of the first cell and west of nothing, under the east cell: aimed near the corner below both.
    const at = placeCell(p, 64 + 30, 64 + 36, {w: 64, h: 64});
    assert.deepEqual([at.y, at.touching], [64, true], 'flush under the cells above');
    const nook = placeCell(p, 64 + 30, 30, {w: 32, h: 32});
    assert.equal(nook.ok, false, 'pointing into a cell is still refused');
});

test('far from other cells, a new cell sits on the 16-tile world grid', () => {
    const p = world();
    const at = placeCell(p, 500, 700, {w: 32, h: 32});
    assert.deepEqual([at.x % 16, at.y % 16, at.ok, at.touching], [0, 0, true, false]);
});

test('a cell never snaps onto another: pointing into a cell is refused', () => {
    const p = world();
    const inside = placeCell(p, 32, 32, {w: 64, h: 64});
    assert.deepEqual([inside.ok, inside.problem], [false, 'overlaps World cell 1']);
    // Just past the east edge it snaps out beside the cell instead.
    const beside = placeCell(p, 64 + 30, 32, {w: 64, h: 64});
    assert.deepEqual([beside.ok, beside.x, beside.y], [true, 64, 0]);
});
