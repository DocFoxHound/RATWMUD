// Every edit an editor makes must reproduce exactly on everyone else's copy:
// applying diff(a, b) to a copy of a gives b, and the inverse gives a back.
import test from 'node:test';
import assert from 'node:assert/strict';
import * as M from '../model/model.mjs';
import {applyChanges, diff, inverse, stable} from './live.ts';

const world = () => { const p = M.createProject(64, 48, 'Test world'); M.cutGrid(p, 32, 24); return p; };
const glyphAt = (p: M.Project, x: number, y: number) => {
    const c = p.cells.find(c => x >= c.x && y >= c.y && x < c.x + c.width && y < c.y + c.height);
    return c?.terrain[y - c.y][x - c.x];
};

// Edits carry no list order (the database keeps its own), so cells are compared in ID order.
const same = (p: M.Project) => stable({...p, cells: [...p.cells].sort((x, y) => x.id.localeCompare(y.id))});

function roundTrip(a: M.Project, change: (p: M.Project) => void) {
    const b = M.clone(a);
    change(b);
    const ops = diff(a, b);
    const there = M.clone(a); applyChanges(there, ops);
    assert.equal(same(there), same(b), 'applying the edits reproduces the change');
    const back = M.clone(b); applyChanges(back, inverse(ops));
    assert.equal(same(back), same(a), 'the inverse edits undo it');
    return ops;
}

test('terrain edits are single tiles in world coordinates', () => {
    const a = world();
    const ops = roundTrip(a, p => { M.paint(p, 3, 4, '~'); M.setHeight(p, 3, 4, 1); });
    assert.deepEqual(ops.map(o => o.key).sort(), ['height:3,4', 'tile:3,4']);
    assert.equal(ops.find(o => o.key === 'tile:3,4')!.before, null, 'plain ground travels as null');
});

test('re-cutting, splitting and merging cells moves no ground: no tile edits, only cell edits', () => {
    const a = world(); M.paint(a, 10, 10, '#'); M.setHeight(a, 11, 10, 1);
    const cut = roundTrip(a, p => { M.cutGrid(p, 16, 16); });
    assert.ok(cut.length && cut.every(o => o.key.startsWith('cell:') || ['spawn', 'link'].some(k => o.key.startsWith(k))), cut.map(o => o.key).join(' '));
    const b = M.clone(a); applyChanges(b, cut);
    assert.equal(glyphAt(b, 10, 10), '#', 'the other editor rebuilds each cell’s ground from the world');
    roundTrip(b, p => { M.splitCell(p, p.cells[0].id, 'x', 8); });
});

test('cells, interiors and their tiles', () => {
    const a = world();
    roundTrip(a, p => { M.addCell(p, {x: 64, y: 0, width: 32, height: 24}, 'East'); });
    const withRoom = M.clone(a); const room = M.addRoom(withRoom, 8, 6, 'Cellar'); M.paint(withRoom, 2, 2, 'T', 1, room.id);
    const created = roundTrip(a, p => { const r = M.addRoom(p, 8, 6, 'Cellar'); M.paint(p, 2, 2, 'T', 1, r.id); });
    assert.ok(created.some(o => o.key.startsWith('room:')) && created.some(o => o.key === `rtile:${room.id}:2,2`));
    roundTrip(withRoom, p => M.resizeRoom(p, room.id, 12, 4));
    roundTrip(withRoom, p => M.removeRoom(p, room.id));
});

test('a far-away cell and its deletion travel as cell and tile edits', () => {
    const a = world();
    const made = roundTrip(a, p => { const c = M.addCell(p, {x: -9_000_000, y: 40_000, width: 16, height: 12}, 'Far'); M.paint(p, c.x + 2, c.y + 3, '~'); });
    assert.deepEqual(made.map(o => o.key.replace(/^cell:.*/, 'cell')).sort(), ['cell', 'tile:-8999998,40003']);
    const b = M.clone(a); applyChanges(b, made);
    assert.equal(glyphAt(b, -8999998, 40003), '~');
    const far = b.cells.find(c => c.name === 'Far')!;
    const gone = roundTrip(b, p => M.removeCell(p, far.id));
    assert.ok(gone.some(o => o.key === 'tile:-8999998,40003' && o.after === null), 'its ground is cleared in the database too');
});

test('applying edits leaves an older version that shares cells untouched', () => {
    const a = world(), snapshot = stable(a);
    const b = M.clone(a); M.paint(b, 3, 3, '~');
    const c = {...a}; applyChanges(c, diff(a, b));
    assert.equal(glyphAt(c, 3, 3), '~');
    assert.equal(stable(a), snapshot);
});

test('people, routes and the world settings', () => {
    const a = world();
    const person = M.newPerson(a, 'guard', {cell: a.cells[0].id, x: 5, y: 5});
    roundTrip(a, p => { M.upsertPerson(p, person); });
    const b = M.clone(a); M.upsertPerson(b, person);
    roundTrip(b, p => { M.upsertRoute(p, {id: 'watch', name: 'Watch', posts: [{cell: p.cells[0].id, x: 6, y: 6}]}); });
    roundTrip(b, p => { p.name = 'Renamed'; M.setSpawn(p, {cell: p.cells[1].id, x: 3, y: 3}); });
});
