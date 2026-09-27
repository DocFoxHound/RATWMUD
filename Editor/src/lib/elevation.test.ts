import test from 'node:test';
import assert from 'node:assert/strict';
import * as M from '../model/model.mjs';
import {formatHeight, heightGrid, hillshade, isCliff, lightStyle, nextElevation, shadeAt, tileHeight, tintStyle} from './elevation.ts';

test('height labels read in half steps', () => {
    assert.deepEqual([0.5, 1, 1.5, -1, -0.5, -1.5, 0, 16, 0.25].map(formatHeight), ['½', '1', '1½', '−1', '−½', '−1½', '0', '16', '½']);
});

test('tile heights: overrides (rounded) win over glyph defaults', () => {
    assert.equal(tileHeight('^', undefined), 0.5);
    assert.equal(tileHeight(':', undefined), 0);
    assert.equal(tileHeight('%', 3), 3);
    assert.equal(tileHeight('.', 0.75), 1);       // Unnormalized legacy data (e.g. the DM's world) still draws.
});

test('height grids unpack overrides and stairs, and are rebuilt when the ground changes', () => {
    const p = M.createProject(8, 8, 'T'); M.cutGrid(p, 8, 8);
    M.paint(p, 1, 1, '^'); M.setHeight(p, 2, 1, 2);
    const c = p.cells[0], grid = heightGrid(c);
    assert.equal(grid[1 * 8 + 1], 0.5); assert.equal(grid[1 * 8 + 2], 2); assert.equal(grid[0], 0);
    assert.equal(heightGrid(c), grid);                       // Cached.
    M.setHeight(p, 2, 1, -1);
    assert.equal(heightGrid(p.cells[0])[1 * 8 + 2], -1);
});

test('shading: high ground warm, low ground cool, lit from the north-west', () => {
    assert.equal(tintStyle(0), null);
    assert.match(tintStyle(2)!, /^rgba\(2\d\d,/);            // Warm and light.
    assert.match(tintStyle(-2)!, /^rgba\(\d{1,2},/);         // Cool and dark.
    assert.ok(hillshade(0, 1, 0, 1) > 0);                    // Rising to the south-east faces the light.
    assert.ok(hillshade(1, 0, 1, 0) < 0);
    assert.equal(hillshade(0, 0, 0, 0), 0);
    assert.equal(lightStyle(0), null);
    const p = M.createProject(8, 8, 'T'); M.cutGrid(p, 8, 8);
    M.setHeight(p, 4, 4, 2);
    const c = p.cells[0], g = heightGrid(c);
    assert.notEqual(shadeAt(c, g, 3, 4)[1], null);           // West of the hill: in its lit slope.
    assert.deepEqual(shadeAt(c, g, 0, 0), [null, null]);     // Flat ground: nothing drawn.
});

test('the relief switch cycles off → shading → full', () => {
    assert.equal(nextElevation('off'), 'shade');
    assert.equal(nextElevation('shade'), 'full');
    assert.equal(nextElevation('full'), 'off');
});

test('catalog defaults: height grids and cliffs come from the terrain catalog', () => {
    const p = M.createProject(8, 8, 'T'); M.cutGrid(p, 8, 8);
    for (const [x, code] of [[0, '^'], [1, ':'], [2, 'k'], [3, 'b'], [4, '%']] as const) M.paint(p, x, 0, code);
    const grid = heightGrid(p.cells[0]);
    assert.deepEqual([...grid.slice(0, 5)], [0.5, 0, 0, 0, 0]);
    assert.equal(isCliff('%'), true); assert.equal(isCliff('#'), false); assert.equal(isCliff(null), false);
    for (const t of M.TERRAIN) assert.equal(tileHeight(t.code, undefined), t.height);
});
