import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createProject, cutGrid, splitCell, mergeCells, normalizeProject, validate, newPerson, upsertPerson,
    removePerson, upsertRoute, removeRoute, setEconomy, setHerbPatch, movePlace, addBuilding, paintTiles, stamp,
    resizeRoom, removeRoom, updateCell, addRoom, addLink, updateLink, getCell, cellTerrain} from './model.mjs';
import {BUILDINGS, buildingById} from './templates.mjs';

const greyfen = () => JSON.parse(readFileSync(new URL('../../../Data/Worlds/Greyfen/greyfen.atlas.json', import.meta.url)));
const world = () => { const p = createProject(32, 24, 'Test'); cutGrid(p, 16, 12); return p; };

test('bundled Greyfen is a clean, exportable atlas', () => {
    const p = normalizeProject(greyfen());
    assert.equal(p.people.length, 10);
    assert.equal(p.routes[0].posts.length, 9);
    assert.deepEqual(validate(p).errors, []);
    assert.deepEqual(normalizeProject(JSON.parse(JSON.stringify(p))), p, 'round-trips without loss');
});

test('version 1 atlases upgrade with an empty population and default economy', () => {
    const legacy = greyfen();                      // Bundled Greyfen is an atlas v2 file with one shared canvas.
    legacy.version = 1;
    for (const k of ['people', 'routes', 'economy', 'herbPatch', 'slots']) delete legacy[k];
    const n = normalizeProject(legacy);
    assert.equal(n.version, 3);
    assert.deepEqual(n.people, []);
    assert.equal(n.economy.treasury, 1000);
    assert.equal(n.herbPatch, null);
});

test('people and posts follow their tiles through cut, split and merge', () => {
    const p = world();
    const cell = p.cells[3];                          // bottom-right 16×12 cell at 16,12
    const spot = {cell: cell.id, x: 5, y: 6};         // world tile 21,18
    const person = upsertPerson(p, {...newPerson(p, 'guard', spot), id: 'sentry'});
    upsertRoute(p, {id: 'loop', name: 'Loop', posts: [spot, {cell: p.cells[0].id, x: 1, y: 1}]});
    upsertPerson(p, {...person, route: 'loop'});
    setHerbPatch(p, spot);
    const at = a => { const c = getCell(p, a.cell); return [c.x + a.x, c.y + a.y]; };
    const check = () => {
        const s = p.people[0];
        for (const a of [s.home, s.work, s.evening, p.routes[0].posts[0], p.herbPatch]) assert.deepEqual(at(a), [21, 18]);
        assert.deepEqual(at(p.routes[0].posts[1]), [1, 1]);
    };
    cutGrid(p, 8, 6); check();
    splitCell(p, p.cells.find(c => c.x === 16 && c.y === 12).id, 'x', 4); check();
    mergeCells(p, p.cells.filter(c => c.y >= 12 && c.x >= 16 && c.y < 18).map(c => c.id)); check();
    assert.deepEqual(validate(p).errors, []);
});

test('validation explains bad people, routes and places', () => {
    const p = world();
    const spot = {cell: p.cells[0].id, x: 2, y: 2};
    const bob = upsertPerson(p, {...newPerson(p, 'civilian', spot), id: 'bob', route: ''});
    const bad = (mutate, pattern) => {
        const q = JSON.parse(JSON.stringify(p)); mutate(q);
        assert.match(validate(q).errors.join('\n'), pattern);
    };
    bad(q => { q.people[0].route = 'nowhere'; }, /walk routes, and the route must exist/);
    bad(q => { q.people[0].role = 'wizard'; }, /role must be/);
    bad(q => { q.people[0].id = 'treasury'; }, /reserved/);
    bad(q => { q.people[0].hours = {start: 8, end: 8}; }, /hours need different/);
    bad(q => { for (const c of q.cells) c.terrain[2] = '#'.repeat(c.width); }, /New resident's home must be on open ground/);
    bad(q => { q.people.push({...q.people[0]}); }, /Duplicate person ID/);
    bad(q => { q.people[0].appearance.species = 'fox'; }, /appearance is invalid/);
    bad(q => { q.economy.treasury = -1; }, /Economy requires/);
    assert.throws(() => upsertPerson(p, {id: 'half'}), /missing/);
    assert.equal(bob.name, 'New resident');
});

test('removing a route frees its guards; removing a person is atomic', () => {
    const p = world(), spot = {cell: p.cells[0].id, x: 3, y: 3};
    upsertRoute(p, {id: 'wall', name: 'Wall', posts: [spot]});
    upsertPerson(p, {...newPerson(p, 'guard', spot), id: 'gus', route: 'wall'});
    removeRoute(p, 'wall');
    assert.equal(p.people[0].route, '');
    removePerson(p, 'gus');
    assert.equal(p.people.length, 0);
    assert.throws(() => removePerson(p, 'gus'), /Unknown person/);
});

test('economy, herb patch and movePlace validate atomically', () => {
    const p = world(), spot = {cell: p.cells[0].id, x: 3, y: 3};
    setEconomy(p, {treasury: 500});
    assert.equal(p.economy.treasury, 500);
    assert.throws(() => setEconomy(p, {treasury: -5}));
    assert.equal(p.economy.treasury, 500, 'unchanged after a rejected edit');
    movePlace(p, {kind: 'herbPatch'}, spot);
    assert.deepEqual(p.herbPatch, spot);
    upsertPerson(p, {...newPerson(p, 'civilian', spot), id: 'ann'});
    movePlace(p, {kind: 'person', id: 'ann', slot: 'home'}, {cell: p.cells[1].id, x: 4, y: 4});
    assert.equal(p.people[0].home.cell, p.cells[1].id);
    assert.throws(() => movePlace(p, {kind: 'person', id: 'ann', slot: 'nap'}, spot));
});

test('shape painting and stamps', () => {
    const p = world();
    paintTiles(p, [[5, 0], [6, 0], [99, 99]], '#');
    assert.equal(p.cells[0].terrain[0].slice(4, 8), '.##.');
    assert.throws(() => paintTiles(p, [[p.spawn.x, p.spawn.y]], '#'), /Spawn/, 'never buries the spawn');
    stamp(p, 30, 0, ['~ ~', '~~~']);
    assert.equal(p.cells[1].terrain[0].slice(14), '~.', 'the stamp lands in the cell that holds it');
    assert.equal(p.cells[1].terrain[1].slice(14), '~~');
    const room = addRoom(p, 6, 6);
    paintTiles(p, [[2, 2]], 'T', room.id);
    assert.equal(cellTerrain(p, room.id)[2][2], 'T');
    assert.throws(() => paintTiles(p, [[0, 0]], '@'), /catalog/);   // Not a terrain code ('X' and 'W' are tiles now).
});

test('rooms can be edited, resized and removed with their doors', () => {
    const p = world(), room = addRoom(p, 8, 8, 'Cellar');
    updateCell(p, room.id, {name: 'Wine cellar', worldX: 40});
    assert.equal(getCell(p, room.id).name, 'Wine cellar');
    assert.throws(() => updateCell(p, p.cells[0].id, {worldX: 3}), /Cannot change/);
    const link = addLink(p, {kind: 'door', a: {cell: p.cells[0].id, x: 4, y: 4}, b: {cell: room.id, x: 2, y: 2}});
    updateLink(p, link.id, {name: 'Trapdoor', open: true});
    assert.equal(p.links[0].name, 'Trapdoor');
    resizeRoom(p, room.id, 12, 5);
    assert.equal(getCell(p, room.id).terrain[0].length, 12);
    assert.throws(() => resizeRoom(p, room.id, 4, 2), /dimensions/);
    upsertPerson(p, {...newPerson(p, 'civilian', {cell: room.id, x: 3, y: 3}), id: 'mole'});
    assert.throws(() => removeRoom(p, room.id), /still inside/);
    removePerson(p, 'mole');
    removeRoom(p, room.id);
    assert.equal(p.rooms.length, 0);
    assert.equal(p.links.length, 0, 'its door went with it');
});

test('every building template places a valid, linked building facing either way', () => {
    for (const template of BUILDINGS) for (const side of ['S', 'N']) {
        const p = createProject(48, 40, 'Town');
        cutGrid(p, 48, 40);
        const cell = p.cells[0].id, [fw, fh] = template.footprint;
        const placed = addBuilding(p, template, cell, 4, side === 'S' ? 2 : 40 - fh - 2, side);
        const room = getCell(p, placed.roomId);
        assert.ok(room, `${template.id} ${side}: room`);
        assert.equal(p.links.length, 1);
        for (const bed of placed.beds) assert.ok(!'#T='.includes(cellTerrain(p, room.id)[bed.y][bed.x]), `${template.id} ${side}: bed on floor`);
        if (placed.counter) assert.ok(!'#T='.includes(cellTerrain(p, room.id)[placed.counter.y][placed.counter.x]), 'counter spot on floor');
        assert.deepEqual(validate(p).errors, [], `${template.id} ${side}`);
        assert.equal(p.cells[0].terrain[side === 'S' ? 2 + fh - 1 : 40 - fh - 2][4 + Math.floor(fw / 2)], '+');
    }
    assert.equal(buildingById('shop').name, 'General store');
});

test('buildings refuse to block their own door or leave the cell', () => {
    const p = createProject(32, 24, 'Tight');
    cutGrid(p, 32, 24);
    const cell = p.cells[0].id;
    assert.throws(() => addBuilding(p, buildingById('shop'), cell, 25, 2), /must fit/);
    paintTiles(p, [[4 + 7, 2 + 9]], '#');
    assert.throws(() => addBuilding(p, buildingById('shop'), cell, 4, 2), /in front of the door/);
    assert.equal(p.rooms.length, 0, 'nothing half-built');
});

test('profession slots follow cuts, validate, and free routes when removed', async () => {
    const {newSlot, upsertSlot, removeSlot} = await import('./model.mjs');
    const p = world(), spot = {cell: p.cells[3].id, x: 5, y: 6};
    const guard = {id: 'guard', name: 'Guard', hours: {start: 6, end: 18}, paid: true};
    upsertRoute(p, {id: 'wall', name: 'Wall', posts: [spot]});
    const slot = upsertSlot(p, {...newSlot(p, guard, spot), route: 'wall'});
    assert.equal(slot.id, 'guard');
    assert.equal(upsertSlot(p, newSlot(p, guard, spot)).id, 'guard_2', 'IDs stay unique');
    cutGrid(p, 8, 6);
    const c = getCell(p, p.slots[0].work.cell);
    assert.deepEqual([c.x + p.slots[0].work.x, c.y + p.slots[0].work.y], [21, 18]);
    movePlace(p, {kind: 'slot', id: 'guard', slot: 'home'}, {cell: p.cells[0].id, x: 1, y: 1});
    assert.equal(p.slots[0].home.cell, p.cells[0].id);
    removeRoute(p, 'wall');
    assert.equal(p.slots[0].route, '');
    const bad = JSON.parse(JSON.stringify(p)); bad.slots[0].route = 'nowhere';
    assert.match(validate(bad).errors.join('\n'), /patrol route must exist/);
    removeSlot(p, 'guard');
    assert.deepEqual(p.slots.map(s => s.id), ['guard_2']);
    assert.throws(() => upsertSlot(p, {id: 'x'}), /missing/);
    assert.equal(normalizeProject(p).id, 'test', 'every atlas has a stable world ID');
});
