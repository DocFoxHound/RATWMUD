// The ground round the cell (Docs/Design/70-the-realm.md, "Sight across seams"): the band the server sends near an
// edge, read and drawn on past the seam, the page's own sight running on over it, and walking kept to the cell.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {bandFits, bandSpot} from './fringe.ts';
import {Walker} from './walker.ts';
import {testGame, draw} from './testing.ts';
import type {Json} from './json.ts';

const wasm = readFileSync(new URL('../wasm/walk.wasm', import.meta.url));

// A 6 x 4 cell of floor with a band 2 wide round it: grass, a wall ('#') two tiles east of the east edge down rows 0-3,
// and unknown (' ') in the band's top-left corner. Heights in the band: 1 (the server's 'R') east of the cell.
const Width = 6, Height = 4, Band = 2;
const cellRows = ['......', '......', '......', '......'];
function bandRows(): string[] {
    const rows: string[] = [];
    for (let y = -Band; y < Height + Band; ++y) {
        const whole = y < 0 || y >= Height;
        let row = '';
        for (let x = -Band; x < Width + Band; ++x) {
            if (!whole && x >= 0 && x < Width) continue;
            row += y < 0 && x < 0 ? ' ' : x === Width + 1 && y >= 0 && y < Height ? '#' : ',';
        }
        rows.push(row);
    }
    return rows;
}
const heightRows = (rows: string[]) => rows.map(r => [...r].map((_, i) => (i >= r.length - Band && r.length === 2 * Band ? 'R' : 'P')).join(''));

test('the band round the cell: where each tile of it lies in the rows the server sends', () => {
    assert.deepEqual(bandSpot(2, 6, 4, -2, -2), [0, 0], 'the top-left corner starts the first row');
    assert.deepEqual(bandSpot(2, 6, 4, 7, -1), [1, 9], 'the rows above the cell run its width and the band each side');
    assert.deepEqual(bandSpot(2, 6, 4, -1, 0), [2, 1], 'beside the cell, the west end of a row');
    assert.deepEqual(bandSpot(2, 6, 4, 6, 3), [5, 2], 'and its east end');
    assert.deepEqual(bandSpot(2, 6, 4, 0, 4), [6, 2], 'below the cell, whole again');
    assert.equal(bandSpot(2, 6, 4, 3, 2), null, 'the cell itself is not in the band');
    assert.equal(bandSpot(2, 6, 4, 8, 0), null, 'nor anything past it');
    assert.ok(bandFits(Band, Width, Height, bandRows()), 'rows of the right lengths fit');
    assert.ok(!bandFits(Band, Width, Height, bandRows().slice(1)), 'one short does not');
});

test('the map runs on over a seam: the ground round the cell is read, drawn and clicked into', () => {
    const {state: s, commands, painter} = testGame();
    const rows = bandRows();
    s.applySnapshot({cell: {id: 'field', width: Width, height: Height, outdoors: true, rows: cellRows, fringe: Band, fringeRows: rows,
        fringeHeights: heightRows(rows)}, fringeVisibility: rows.map(r => r.replace(/[^ ]/g, '2').replace(/ /g, '0')),
    self: {id: 'self', x: 5.5, y: 1.5}, entities: [], doors: [], time: 0, cellGeneration: 1} as Json);
    assert.equal(s.fringe, Band, 'near an edge the band is held');
    assert.deepEqual(s.groundExtent(), {x0: -2, y0: -2, x1: 7, y1: 5}, 'and the map may show it');
    assert.equal(s.glyphAt(7, 1), '#', 'the wall past the east edge');
    assert.equal(s.glyphAt(6, 0), ',', 'the grass before it');
    assert.equal(s.glyphAt(-1, -1), ' ', 'nothing known in the corner');
    assert.equal(s.seenAt(-1, -1), '0', 'and it is unknown');
    assert.equal(s.seenAt(6, 0), '2', 'what the server sees there');
    assert.equal(s.heightAt(7, 2), 1, 'the band carries its heights');
    assert.equal(s.heightAt(-1, -1), s.heightAt(0, 0), 'unknown ground takes the height of the cell beside it');
    draw(painter, 'drawLocal');
    s.mapOrigin = [0, 0];
    s.tileSize = 10;
    s.mapRect = {left: 0, top: 0, right: 200, bottom: 200} as typeof s.mapRect;
    s.hits = [];
    s.mouseDown([65, 15], true, false, false);
    const path = commands.find(c => c.type === 'path');
    assert.ok(path && (path.x as number) > Width, 'a click past the edge heads there (the server walks it to the seam)');
    // Away from the edge the server sends none: the band is let go.
    s.applySnapshot({cell: {id: 'field', width: Width, height: Height, outdoors: true, rows: cellRows}, self: {id: 'self', x: 3.5, y: 1.5},
        entities: [], doors: [], time: 1, cellGeneration: 1} as Json);
    assert.equal(s.fringe, 0, 'none sent, none held');
    assert.equal(s.glyphAt(7, 1), ' ', 'and nothing is drawn past the edge');
});

test("the page's sight runs on over the band, and its walking stays in the cell", async () => {
    const walker = (await Walker.load(wasm))!;
    const rows = bandRows();
    const heights = rows.map(r => Float32Array.from([...r].map(() => 0)));
    walker.setCell(Width, Height, cellRows, new Float32Array(Width * Height), [], new Set(), {band: Band, rows, heights});
    const seen = walker.sight(4.5, 1.5, 27);
    const at = (x: number, y: number) => seen[(y + Band) * (Width + 2 * Band) + x + Band];
    assert.equal(at(3, 1), 1, 'the cell, as ever');
    assert.equal(at(6, 1), 1, 'the grass past the seam');
    assert.equal(at(7, 1), 1, 'the wall there');
    assert.equal(walker.step(5.5, 1.5, 1, 0, 2.6, false, 1, 0.5).x < Width, true, 'the page walks only to the edge: the server takes it over');
    assert.ok(walker.passable(4.5, 1.5, 3.5, 1.5) && !walker.passable(6.5, 1.5, 5.5, 1.5), 'the band is not stood on');
    // And the game state's own shading of the band (it works out what it sees, the server sending no visibility).
    const {state: s} = testGame();
    s.walker = walker;
    s.applySnapshot({cell: {id: 'field', width: Width, height: Height, outdoors: true, rows: cellRows, fringe: Band, fringeRows: rows,
        fringeHeights: heightRows(rows)}, self: {id: 'self', x: 4.5, y: 1.5, posture: 'standing', sightRange: 27}, entities: [], doors: [],
    time: 0, cellGeneration: 1} as Json);
    s.applyMotion({motionSession: '', observer: 'self', cellId: 'field', cellGeneration: 1, revision: 0, time: 0.05,
        entities: [{id: 'self', x: 4.5, y: 1.5, facing: 0, moving: false}], mode: 0});
    s.tick(1, 0.016);
    assert.equal(s.seenAt(6, 1), '2', 'the grass past the seam is in sight');
    assert.equal(s.seenAt(-1, -1), '0', 'the unknown corner stays unknown');
});
