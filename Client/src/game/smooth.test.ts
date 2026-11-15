// Walking smoothly (the user, 2026-11-15: "Moving left-and-right is smooth, moving up-and-down has jerk frames ... I
// seem to run into frame jitter still especially in the market"). What made the jerks, each held to here:
//  - the map's ground copy was drawn again whole at every tile walked within the screen's reach of an edge (56 rows
//    above and below, 96 columns to the sides: the market lies 35-57 rows from its cell's top), because the band round
//    the cell was compared whole, not where the copy is;
//  - walking on past the copy drew all of it again (20-30 ms a city view), every eight tiles;
//  - the walking grid was made again whole (256 x 256 and the band) whenever the band's ground came in;
//  - the camera's origin was where the ground was drawn on whole pixels and everything else between them.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {draw, fakeContext, testGame} from './testing.ts';
import {GamePainter} from './paint.ts';
import {Painter} from '../ui/painter.ts';
import {shadeRow} from './screen.ts';
import {Walker} from './walker.ts';
import type {TileWindow} from './terrainLayer.ts';
import type {Sheets} from './weatherArt.ts';
import type {Json} from './json.ts';

const Side = 256, Band = 48;
const cellRows = Array.from({length: Side}, () => '.'.repeat(Side));
const bandRows = (glyph = ',') => {
    const out: string[] = [];
    for (let y = -Band; y < Side + Band; ++y) out.push(glyph.repeat(y < 0 || y >= Side ? Side + 2 * Band : 2 * Band));
    return out;
};
const lit = (rows: string[]) => rows.map(r => '2'.repeat(r.length));

/** A city cell with the band round it, the wolf at (x, y), on a 400 x 300 map; what the ground painter is asked to draw. */
function city(x: number, y: number) {
    const g = testGame(true);
    const s = g.state;
    s.mapRect = {left: 0, top: 0, right: 400, bottom: 300} as typeof s.mapRect;
    const fringeRows = bandRows();
    const snapshot = (time: number, at: [number, number], fringeVisibility = lit(fringeRows)) => ({cell: {id: 'city', width: Side, height: Side,
        outdoors: true, rows: cellRows, fringe: Band, fringeRows, fringeHeights: fringeRows.map(r => 'P'.repeat(r.length))},
    visibility: lit(cellRows), fringeVisibility, self: {id: 'me', x: at[0], y: at[1]}, entities: [], doors: [], time, cellGeneration: 1} as Json);
    s.applySnapshot(snapshot(0, [x, y]));
    s.tick(0.016, 0.016);
    const painted: TileWindow[] = [];
    const lights: number[] = [];                    // The light each of those was drawn in.
    const ground = (g.painter.terrain as unknown as {ground: {paint: (...a: unknown[]) => void}}).ground;
    const paint = ground.paint.bind(ground);
    ground.paint = (...a: unknown[]) => {
        painted.push({...(a[2] as TileWindow)});
        lights.push(a[6] as number);
        paint(...a);
    };
    const frame = () => draw(g.painter, 'drawLocal');
    const walk = (dx: number, dy: number) => {
        const me = s.entities.get('me')!;
        me.x += dx;
        me.y += dy;
        frame();
    };
    return {...g, s, painted, lights, frame, walk, snapshot, fringeRows};
}

test('walking near an edge, the screen\'s edge moving over the band far off the map never redraws the ground', () => {
    // Forty rows from the top: walking up and down moves the screen's top edge (56 rows up) over the band at -16.
    const c = city(128.5, 40.5);
    c.frame();
    assert.equal(c.painter.terrain.rebuilds, 1);
    const seen = lit(c.fringeRows);
    seen[Band - 16] = seen[Band - 16].replace(/2/g, '1');      // (Band row y -16: off the screen now, remembered.)
    c.s.applySnapshot(c.snapshot(0.1, [128.5, 40.5], seen));
    c.frame();
    assert.equal(c.painter.terrain.rebuilds, 1, 'a band row far above the map changed: the copy is kept');
    // But near the top, where the copy holds the band, a change there redraws it.
    const top = city(128.5, 3.5);
    top.frame();
    const near = lit(top.fringeRows);
    near[Band - 2] = near[Band - 2].replace(/2/g, '1');
    top.s.applySnapshot(top.snapshot(0.1, [128.5, 3.5], near));
    top.frame();
    assert.equal(top.painter.terrain.rebuilds, 2, 'a change in the band the copy holds redraws it');
});

test('walking on past the ground copy moves it on, drawing only the new strip and a tile each side of the seam', () => {
    const c = city(128.5, 128.5);
    c.frame();
    const first = c.painted.at(-1)!;
    const width = first.x1 - first.x0, height = first.y1 - first.y0;
    assert.equal(c.painter.terrain.rebuilds, 1);
    c.painted.length = 0;
    for (let i = 0; i < 4; ++i) c.walk(1, 0);                   // (Four tiles east: still in the copy's margin.)
    assert.equal(c.painted.length, 0, 'within the copy, nothing is drawn');
    for (let i = 0; i < 6; ++i) c.walk(1, 0);                   // (Ten east: past it.)
    assert.equal(c.painter.terrain.rebuilds, 1, 'never drawn whole again');
    assert.equal(c.painter.terrain.scrolls, 1, 'the copy moved on once');
    assert.equal(c.painted.length, 1, 'one strip, on the east');
    const strip = c.painted[0];
    assert.equal(strip.y1 - strip.y0, height, 'the copy\'s whole height');
    assert.ok(strip.x1 - strip.x0 < width / 2, `a strip, not the copy (${strip.x1 - strip.x0 + 1} of ${width + 1} columns)`);
    c.painted.length = 0;
    for (let i = 0; i < 10; ++i) c.walk(0, -1);                 // North, and then a diagonal.
    assert.equal(c.painted.length, 1, 'north: one strip along the top');
    assert.equal(c.painted[0].x1 - c.painted[0].x0, width, 'the copy\'s whole width');
    c.painted.length = 0;
    for (let i = 0; i < 10; ++i) c.walk(-1, 1);
    assert.equal(c.painted.length, 2, 'diagonally: a strip on each of the two sides');
    assert.equal(c.painter.terrain.rebuilds, 1);
    c.s.zoom = (c.s.zoom + 1) % 4;                               // A new tile size: drawn whole, as ever.
    c.frame();
    assert.ok(c.painter.terrain.rebuilds >= 1);
});

test('a fade into a darker cell draws the ground once, in the light it fades to; strips walked on are in that light', () => {
    // (r3-light drew the copy again at each twenty-fifth of the fade, up to 25 whole redraws a crossing; the fade is
    // now laid over the one copy, and a strip drawn mid-fade is in the copy's light, never a band brighter than it.)
    const c = city(128.5, 128.5);
    c.s.clock = 1;
    c.frame();
    assert.equal(c.painter.terrain.rebuilds, 1);
    c.s.environment = {...c.s.environment, illumination: 0.08};
    c.painted.length = 0;
    c.lights.length = 0;
    for (let i = 0; i < 10; ++i) {
        c.s.clock += 0.1;
        c.walk(1, 0);
    }
    assert.ok(c.s.ambient.env.illumination > 0.3, `still fading (${c.s.ambient.env.illumination.toFixed(2)})`);
    assert.equal(c.painter.terrain.rebuilds, 2, 'drawn whole once, for the light it fades to');
    assert.equal(c.painter.terrain.scrolls, 1, 'and moved on as the wolf walked, mid-fade');
    assert.deepEqual(c.lights, [0.08, 0.08], 'the copy and its new strip both in the night\'s light');
    assert.ok((c.painter.terrain as unknown as {relitCopy: unknown}).relitCopy, 'the fade laid over the copy, in a canvas of its own');
    for (let i = 0; i < 80; ++i) {
        c.s.clock += 0.1;
        c.frame();
    }
    assert.equal(c.s.ambient.env.illumination, 0.08);
    assert.equal(c.painter.terrain.rebuilds, 2, 'never drawn whole again through the fade');
});

test('shading a row of the map cuts the made shadings: the same as shading it afresh', () => {
    const fresh = (glyphs: string, from: number, to: number) => {
        const dim = (part: string) => part.replace(/[^ ]/g, '1').replace(/ /g, '0');
        const a = Math.max(0, Math.min(glyphs.length, from)), b = Math.max(a, Math.min(glyphs.length, to + 1));
        return dim(glyphs.slice(0, a)) + glyphs.slice(a, b).replace(/[^ ]/g, '2').replace(/ /g, '0') + dim(glyphs.slice(b));
    };
    const rows = ['', '#..  ,,', '   ', 'abc', '..#..#..#..'.repeat(20)];
    for (const row of rows)
        for (const [from, to] of [[0, -1], [-5, 3], [2, 4], [0, row.length - 1], [3, 400], [row.length, row.length + 3], [5, 2]])
            assert.equal(shadeRow(row, from, to), fresh(row, from, to), `"${row.slice(0, 12)}" ${from}..${to}`);
});

test('the camera stands on whole device pixels: the ground and every wolf on it move together', () => {
    const {state: s} = testGame();
    const main = fakeContext() as unknown as Record<string, unknown>;
    main.getTransform = () => ({a: 1.25, b: 0, c: 0, d: 1.25, e: 0, f: 0});
    const painter = new GamePainter(s, new Painter(main as unknown as CanvasRenderingContext2D), {get: () => ({})} as unknown as Sheets);
    s.mapRect = {left: 0, top: 0, right: 401, bottom: 299} as typeof s.mapRect;
    s.applySnapshot({cell: {id: 'city', width: Side, height: Side, outdoors: true, rows: cellRows}, visibility: lit(cellRows),
        self: {id: 'me', x: 100.37, y: 80.81}, entities: [{id: 'cy', x: 103.5, y: 82.5, npc: true}], doors: [], time: 0, cellGeneration: 1} as Json);
    s.tick(0.016, 0.016);
    for (const [dx, dy] of [[0, 0], [0.043, 0], [0, 0.043], [0.031, 0.029]]) {
        const me = s.entities.get('me')!;
        me.x += dx;
        me.y += dy;
        draw(painter, 'drawLocal');
        const [ox, oy] = s.mapOrigin;
        assert.ok(Math.abs(ox * 1.25 - Math.round(ox * 1.25)) < 1e-9 && Math.abs(oy * 1.25 - Math.round(oy * 1.25)) < 1e-9,
            `the map's origin on whole device pixels (${ox}, ${oy})`);
        // A resident standing on a tile is drawn on the same pixel grid as the tile under it, however the camera moves.
        const tile = s.tileSize, cy = s.entities.get('cy')!;
        assert.ok(Math.abs((ox + cy.x * tile) - (ox + Math.floor(cy.x) * tile) - 0.5 * tile) < 1e-9);
    }
});

test('the ground round the cell coming in updates the walking grid in place, not the whole of it', async () => {
    const walker = (await Walker.load(readFileSync(new URL('../wasm/walk.wasm', import.meta.url))))!;
    let made = 0;
    const setCell = walker.setCell.bind(walker);
    walker.setCell = (...a: Parameters<Walker['setCell']>) => {
        ++made;
        setCell(...a);
    };
    const {state: s} = testGame();
    s.walker = walker;
    const fringeRows = bandRows();
    const heights = (rise: (r: number, j: number) => string) => fringeRows.map((row, r) => [...row].map((_, j) => rise(r, j)).join(''));
    const snapshot = (time: number, rows: string[], fringeHeights: string[]) => ({cell: {id: 'city', width: Side, height: Side, outdoors: true,
        rows: cellRows, fringe: Band, fringeRows: rows, fringeHeights}, self: {id: 'me', x: 128.5, y: 3.5}, entities: [], doors: [], time,
    cellGeneration: 1} as Json);
    s.applySnapshot(snapshot(0, fringeRows, heights(() => 'P')));
    (s as unknown as {ensureWalkerCell: () => void}).ensureWalkerCell();
    assert.equal(made, 1, 'the grid is made once');
    // New ground in the band (as the screen's edge moves over it): a wall and a rise, three rows above the cell.
    const next = [...fringeRows];
    next[Band - 3] = next[Band - 3].slice(0, Band + 130) + '#' + next[Band - 3].slice(Band + 131);
    s.applySnapshot(snapshot(0.1, next, heights((r, j) => (r === Band - 3 && j === Band + 131 ? 'R' : 'P'))));
    (s as unknown as {ensureWalkerCell: () => void}).ensureWalkerCell();
    assert.equal(made, 1, 'and only updated where the band changed');
    const grid = walker as unknown as {gridBase: number; width: number; wasm: {memory: WebAssembly.Memory}};
    const floats = new Float32Array(grid.wasm.memory.buffer, grid.gridBase);
    const at = (x: number, y: number) => ((y + Band) * grid.width + x + Band) * 5;   // (TileBytes 20: five floats a tile.)
    assert.ok(floats[at(131, -3)] > 0, `the rise is in the grid (${floats[at(131, -3)]})`);
    assert.equal(floats[at(130, -3)], 0, 'beside it, level as before');
});
