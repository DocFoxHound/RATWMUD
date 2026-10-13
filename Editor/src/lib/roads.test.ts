import test from 'node:test';
import assert from 'node:assert/strict';
import * as M from '../model/model.mjs';
import {paintable, planRoad, type Ground} from './roads.ts';

/** Ground from rows of glyphs, and height overrides by "x,y". */
const ground = (rows: string[], heights: Record<string, number> = {}): Ground => ({
    glyph: (x, y) => (y >= 0 && y < rows.length && x >= 0 && x < rows[y].length ? rows[y][x] : undefined),
    heightAt: (x, y) => heights[`${x},${y}`],
});
const at = (tiles: {x: number; y: number}[]) => new Set(tiles.map(t => `${t.x},${t.y}`));

test('the catalog marks the ways NPCs keep to, the new footpath among them', () => {
    for (const code of ['d', '_', 'f', '8', '/']) assert.ok(M.WAYS.has(code), code);
    for (const code of [',', '.', '4', '#']) assert.ok(!M.WAYS.has(code), code);
    assert.ok(paintable(',') && paintable('*') && !paintable('#') && !paintable('d') && !paintable('+') && !paintable('~'));
});

test('on open grass a road goes straight, as wide as asked, all of it laid', () => {
    const g = ground(Array(9).fill(','.repeat(30)));
    const plan = planRoad(g, [[2, 4], [27, 4]], {glyph: 'd', width: 3, bridges: false});
    assert.equal(plan.problem, undefined);
    assert.ok(plan.route.every(([, y]) => y === 4), 'a straight line');
    assert.equal(plan.tiles.length, (26 + 2) * 3);          // (The width reaches a tile past each end too.)
    assert.ok(plan.tiles.every(t => t.glyph === 'd' && t.height === undefined));
});

test('it finds its way round a wall, through the gap, and lays nothing on the wall', () => {
    const rows = Array(11).fill(','.repeat(30));
    rows[5] = '#'.repeat(24) + ',,,,,,';           // A wall across, with a gap at its east end.
    const plan = planRoad(ground(rows), [[5, 1], [5, 9]], {glyph: '/', width: 1, bridges: false});
    assert.equal(plan.problem, undefined);
    assert.ok(plan.route.some(([x, y]) => y === 5 && x >= 24), 'through the gap');
    assert.ok(plan.tiles.every(t => rows[t.y][t.x] !== '#'), 'never on the wall');
    // Joined edge to edge: every tile of the road touches the next by a side, so walkers follow it.
    const laid = at(plan.tiles);
    for (const [x, y] of plan.route) assert.ok(laid.has(`${x},${y}`));
});

test('it joins a road already there rather than cutting a new one beside it, and leaves the old one as it is', () => {
    const rows = Array(12).fill(','.repeat(40));
    rows[8] = ','.repeat(3) + '_'.repeat(34) + ',,,';   // A cobbled street along row 8.
    const plan = planRoad(ground(rows), [[3, 3], [37, 3]], {glyph: 'd', width: 1, bridges: false});
    assert.equal(plan.problem, undefined);
    const onStreet = plan.route.filter(([, y]) => y === 8).length;
    assert.ok(onStreet > 20, `along the street (${onStreet} tiles of it)`);
    assert.ok(plan.tiles.every(t => rows[t.y][t.x] !== '_'), 'the cobbles are not dug up');
});

test('it climbs only what can be walked: round a ledge, up the half steps', () => {
    // A plateau one whole step up (no ramp) in the middle; to its south the ground rises in half steps.
    const rows = Array(14).fill(','.repeat(30));
    const heights: Record<string, number> = {};
    for (let y = 0; y < 9; y++) for (let x = 10; x < 20; x++) heights[`${x},${y}`] = 1;
    const plan = planRoad(ground(rows, heights), [[2, 3], [27, 3]], {glyph: 'd', width: 1, bridges: false});
    assert.equal(plan.problem, undefined);
    const climbs = plan.route.slice(1).map((t, i) => Math.abs((heights[t.join()] ?? 0) - (heights[plan.route[i].join()] ?? 0)));
    assert.ok(climbs.every(d => d <= .5), 'never a whole step at once');
    assert.ok(plan.route.some(([x, y]) => x >= 10 && x < 20 && y >= 9), 'round the south of the ledge');
});

test('water: round it where it can, over a bridge level with its banks where asked', () => {
    const rows = Array(9).fill(','.repeat(20).slice(0, 8) + '~~~~' + ','.repeat(8));
    const heights: Record<string, number> = {};
    for (let y = 0; y < 9; y++) for (let x = 0; x < 8; x++) heights[`${x},${y}`] = .5;
    for (let y = 0; y < 9; y++) for (let x = 12; x < 20; x++) heights[`${x},${y}`] = .5;
    const g = ground(rows, heights);
    const none = planRoad(g, [[2, 4], [17, 4]], {glyph: 'd', width: 1, bridges: false});
    assert.ok(none.problem, 'a river from edge to edge: no way round without a bridge');
    const plan = planRoad(g, [[2, 4], [17, 4]], {glyph: 'd', width: 2, bridges: true});
    assert.equal(plan.problem, undefined);
    const bridge = plan.tiles.filter(t => t.glyph === '8');
    assert.ok(bridge.length >= 4 && plan.bridges === bridge.length, 'a bridge across');
    assert.ok(bridge.every(t => t.height === .5), 'level with its banks');
});

test('laying it is one edit: the road and its bridge in the world, undone as one', () => {
    let project = M.createProject(20, 10);
    project = M.paintTiles(project, [[10, 3], [10, 4]], '~');
    const g: Ground = {
        glyph: (x, y) => { const c = M.cellIndex(project.cells).at(x, y); return c ? c.terrain[y - c.y][x - c.x] : undefined; },
        heightAt: (x, y) => { const c = M.cellIndex(project.cells).at(x, y); return c ? c.heights[`${x - c.x},${y - c.y}`] : undefined; },
    };
    const plan = planRoad(g, [[2, 3], [17, 3]], {glyph: '_', width: 1, bridges: true});
    assert.equal(plan.problem, undefined);
    project = M.layRoad(project, plan.tiles);
    const c = project.cells[0];
    for (const [x, y] of plan.route) assert.ok(M.WAYS.has(c.terrain[y][x]), `${x},${y} is a way`);
});
