// Who is in sight (inSight.ts): the wolves drawn within the map, wherever the server places them, the cells beside
// included; so crossing a seam changes no one in it (the user, 2026-11-15).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {inSight, onMap} from './inSight.ts';
import {draw, testGame} from './testing.ts';
import type {Json} from './json.ts';

const Side = 256;
const rows = (width: number, height: number) => Array.from({length: height}, () => '.'.repeat(width));
const ids = (s: ReturnType<typeof testGame>['state']) => inSight(s).map(e => e.id);

/** A city cell, the wolf at (x, y), the others as given; the map 400 x 300 pixels on the page, drawn once. */
function seen(entities: Json[], x = 128.5, y = 128.5, cell = 'here') {
    const g = testGame();
    const s = g.state;
    s.mapRect = {left: 0, top: 0, right: 400, bottom: 300} as typeof s.mapRect;
    s.applySnapshot({cell: {id: cell, width: Side, height: Side, outdoors: true, rows: rows(Side, Side)}, self: {id: 'me', x, y},
        entities, doors: [], time: 0, cellGeneration: 1} as Json);
    s.tick(0.016, 0.016);
    draw(g.painter, 'drawLocal');
    return g;
}

test('in sight: whoever is drawn within the map, nearest first, never the wolf itself', () => {
    const {state: s} = seen([{id: 'near', x: 130.5, y: 128.5, npc: true}, {id: 'far', x: 135.5, y: 131.5, npc: true},
        {id: 'offmap', x: 160.5, y: 128.5, npc: true}, {id: 'below', x: 128.5, y: 150.5}]);
    assert.equal(s.tileSize > 0, true);
    const f = {left: 0, top: 0, right: 400, bottom: 300, ox: s.mapOrigin[0], oy: s.mapOrigin[1], tile: s.tileSize};
    assert.ok(onMap(f, 128.5, 128.5), 'the wolf is in the middle of the map');
    assert.ok(!onMap(f, 160.5, 128.5) && !onMap(f, 128.5, 150.5), 'past its right and bottom edges are off it');
    assert.equal(s.entities.size, 5, 'the server sent all four (its screen is wider than this map)');
    assert.deepEqual(ids(s), ['near', 'far'], 'only those on the map are in sight, nearest first');
});

test('in sight across a seam: one past it is listed while drawn, and the same wolves stay listed over the crossing', () => {
    // Standing by the north edge, with a wolf four tiles past it (the server places it in this cell's tiles,
    // y -3) and one in the cell behind; the band is drawn round the cell, so the map shows the ground past the seam.
    const band = 48;
    const fringeRows: string[] = [];
    for (let y = -band; y < Side + band; ++y) fringeRows.push(','.repeat(y < 0 || y >= Side ? Side + 2 * band : 2 * band));
    const g = testGame();
    const s = g.state;
    s.mapRect = {left: 0, top: 0, right: 400, bottom: 300} as typeof s.mapRect;
    const north = (time: number, x: number, y: number, entities: Json[], cell: string, generation: number) => {
        s.applySnapshot({cell: {id: cell, width: Side, height: Side, outdoors: true, rows: rows(Side, Side), fringe: band, fringeRows,
            fringeHeights: fringeRows.map(r => 'P'.repeat(r.length))},
            self: {id: 'me', x, y}, entities, doors: [], time, cellGeneration: generation} as Json);
        s.tick(time, 0.016);
        draw(g.painter, 'drawLocal');
    };
    north(0.1, 100.5, 0.5, [{id: 'bo', cell: 'north', x: 100.5, y: -2.5}, {id: 'cy', cell: 'here', x: 102.5, y: 5.5, npc: true}], 'here', 1);
    assert.ok(s.entities.get('bo')!.beyond && !s.entities.get('cy')!.beyond, 'Bo stands past the seam, Cy in this cell');
    assert.deepEqual(ids(s), ['bo', 'cy'], 'both are in sight, nearest first');
    // Over the seam: the server now has the wolf in the cell north, Bo in its own, Cy past the seam behind.
    north(0.2, 100.5, Side - 0.5, [{id: 'bo', cell: 'north', x: 100.5, y: Side - 2.5}, {id: 'cy', cell: 'here', x: 102.5, y: Side + 5.5, npc: true}],
        'north', 2);
    assert.deepEqual(ids(s), ['bo', 'cy'], 'the same two, in the same order, the moment the wolf is over');
    assert.ok(s.entities.get('cy')!.beyond && !s.entities.get('bo')!.beyond, 'now Cy is the one past the seam');
    // A click on a row opens its menu (one past the seam, or any resident: doc 84 §13.2).
    s.entities.get('cy')!.actions = ['inspect', 'talk'];
    s.sightClicked('cy', [10, 10]);
    assert.deepEqual(s.talkTargets, [], 'not chosen to speak to');
    assert.equal(s.contextTarget, 'cy', 'its menu instead');
});

test('in sight: before the map is drawn, everyone the server sends', () => {
    const {state: s} = testGame();
    s.applySnapshot({cell: {id: 'room', width: 32, height: 24, rows: rows(32, 24)}, self: {id: 'me', x: 5.5, y: 5.5},
        entities: [{id: 'a', x: 30.5, y: 20.5}, {id: 'b', x: 6.5, y: 5.5}], doors: [], time: 0, cellGeneration: 1} as Json);
    assert.deepEqual(ids(s), ['b', 'a'], 'no map yet: all, nearest first');
});
