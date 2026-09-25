import test from 'node:test';
import assert from 'node:assert/strict';
import {createProject, paint, setHeight, cutGrid, splitCell, mergeCells, addRoom, addLink,
    removeLink, getCell, cellTerrain, validate, clone, normalizeProject,
    defaultTerritory, upsertFaction, upsertChapter, removeFaction, removeChapter, setTerritory} from './model.mjs';

test('legacy lighting defaults and explicit room lighting survive normalization', () => {
    const p = world(), r = addRoom(p, 8, 8);
    delete p.cells[0].lighting; delete r.lighting;
    const n = normalizeProject(p);
    assert.deepEqual(n.cells[0].lighting, {artificial: 1, daylightAccess: 1, tone: 'warm'});
    assert.deepEqual(n.rooms[0].lighting, n.cells[0].lighting);
    n.rooms[0].lighting = {artificial: 0, daylightAccess: 0, tone: 'neutral'};
    assert.deepEqual(normalizeProject(JSON.parse(JSON.stringify(n))).rooms[0].lighting, n.rooms[0].lighting);
});

test('invalid lighting cannot be normalized or quietly discarded', () => {
    const valid = {artificial: .7, daylightAccess: .3, tone: 'cool'};
    for (const bad of [null, [], true, {}, {...valid, artificial: true}, {...valid, artificial: '1'},
        {...valid, artificial: NaN}, {...valid, artificial: Infinity}, {...valid, daylightAccess: -1},
        {...valid, daylightAccess: 1.1}, {...valid, tone: 'red'}, {...valid, unexpected: 1}]) {
        const p = world(); p.cells[0].lighting = bad;
        assert.throws(() => normalizeProject(p), /lighting/);
        assert.match(validate(p).errors.join(' '), /lighting/);
    }
});

test('cut split and merge preserve lighting without shared mutable profiles', () => {
    const p = world(16, 8, 16, 8), profile = {artificial: .25, daylightAccess: .5, tone: 'cool'};
    p.cells[0].lighting = {...profile};
    const split = splitCell(p, p.cells[0].id, 'x', 8);
    assert.deepEqual(split.map(c => c.lighting), [profile, profile]);
    split[0].lighting.artificial = .1;
    assert.equal(split[1].lighting.artificial, .25);
    unchanged(p, () => mergeCells(p, p.cells.map(c => c.id)), /conflicting lighting/);
    unchanged(p, () => cutGrid(p, 16, 8), /conflicting lighting/);
    p.cells[0].lighting = {...profile};
    mergeCells(p, p.cells.map(c => c.id)); cutGrid(p, 8, 8);
    assert.deepEqual(p.cells.map(c => c.lighting), [profile, profile]);
});

function world(width = 16, height = 8, cutWidth = 8, cutHeight = 8) {
    const project = createProject(width, height);
    cutGrid(project, cutWidth, cutHeight);
    return project;
}
function unchanged(project, operation, pattern) {
    const before = clone(project);
    assert.throws(operation, pattern);
    assert.deepEqual(project, before, 'failed operations must be atomic');
}
function linkTwo(project, overrides = {}) {
    return addLink(project, {name: 'Test door', kind: 'door',
        a: {cell: project.cells[0].id, x: 2, y: 2},
        b: {cell: project.cells[1].id, x: 2, y: 2}, ...overrides});
}
function globalAnchor(project, anchor) {
    const cell = getCell(project, anchor.cell);
    return Object.hasOwn(cell, 'terrain') ? {...anchor} : {x: cell.x + anchor.x, y: cell.y + anchor.y};
}

test('draft creation is structurally valid but export requires cut cells and spawn', () => {
    const p = createProject();
    assert.equal(p.width, 64); assert.equal(p.height, 48);
    assert.equal(p.terrain.length, 48); assert.equal(p.terrain[0], '.'.repeat(64));
    assert.equal(p.cells.length, 0); assert.equal(p.spawn, null);
    assert.deepEqual(normalizeProject(p), p);
    assert.match(validate(p).errors.join(' '), /not assigned.*spawn/s);
    assert.throws(() => createProject(3, 4), /dimensions/);
    assert.throws(() => createProject(4, 257), /dimensions/);
    assert.throws(() => createProject(4.1, 4), /dimensions/);
    assert.throws(() => createProject(4, 4, 'bad\nname'), /control/);
});

test('world brush spans future cell seams without moving or duplicating terrain', () => {
    const p = createProject(16, 8);
    paint(p, 6, 2, ',', 4); setHeight(p, 8, 3, 1.25);
    const terrain = [...p.terrain], heights = {...p.heights};
    cutGrid(p, 8, 8);
    assert.equal(cellTerrain(p, p.cells[0].id)[2], '......,,');
    assert.equal(cellTerrain(p, p.cells[1].id)[2], ',,......');
    assert.deepEqual(p.terrain, terrain); assert.deepEqual(p.heights, heights);
    assert.deepEqual(validate(p).errors, []);
});

test('brush size clips at map edges and rejects invalid input without mutation', () => {
    const p = createProject(8, 8);
    paint(p, 7, 7, '~', 8);
    assert.equal(p.terrain[7], '.......~');
    for (const [x, y, glyph, size] of [[-1, 0, '.', 1], [8, 0, '.', 1], [0, 8, '.', 1],
        [0.5, 0, '.', 1], [0, 0, ' ', 1], [0, 0, '@', 1], [0, 0, 'WW', 1], [0, 0, '.', 0], [0, 0, '.', 9]])
        unchanged(p, () => paint(p, x, y, glyph, size));
    for (const glyph of '.#,\"T=~:^+') paint(p, 3, 3, glyph);
});

test('sparse quarter-tile heights support explicit zero and removing overrides', () => {
    const p = createProject(8, 8);
    paint(p, 2, 2, '^'); setHeight(p, 2, 2, 0);
    assert.equal(p.heights['2,2'], 0);
    setHeight(p, 2, 2, null); assert.equal(Object.hasOwn(p.heights, '2,2'), false);
    for (const v of [-16, 16, -0.25, 2.75]) { setHeight(p, 0, 0, v); assert.equal(p.heights['0,0'], v); }
    for (const v of [-16.25, 16.25, 0.1, NaN, Infinity, '1', undefined]) unchanged(p, () => setHeight(p, 1, 1, v), /Elevation/);
    unchanged(p, () => setHeight(p, -1, 0, 1), /coordinate/);
});

test('default cut is 32×24, retains exact metadata and IDs on repeated cut', () => {
    const p = createProject(); cutGrid(p);
    assert.equal(p.cells.length, 4);
    assert.deepEqual(p.cells.map(c => [c.x, c.y, c.width, c.height]), [[0, 0, 32, 24], [32, 0, 32, 24], [0, 24, 32, 24], [32, 24, 32, 24]]);
    p.cells[0].name = 'Forest'; p.cells[0].description = 'Old trees\nA winding path';
    const before = clone(p); cutGrid(p);
    assert.deepEqual(p, before);
});

test('custom cuts preserve valid remainders and reject 1–3-tile slivers', () => {
    const p = createProject(36, 28); cutGrid(p);
    assert.deepEqual(p.cells.map(c => [c.width, c.height]), [[32, 24], [4, 24], [32, 4], [4, 4]]);
    for (const size of [33, 34, 35]) {
        const draft = createProject(size, 24); unchanged(draft, () => cutGrid(draft), /sliver/);
    }
    const small = createProject(4, 4); cutGrid(small);
    assert.equal(small.cells.length, 1); assert.equal(small.cells[0].width, 4);
    unchanged(p, () => cutGrid(p, 0, 24), /Cut sizes/);
});

test('first cut finds a passable spawn and all-solid draft remains export-invalid', () => {
    const p = createProject(8, 8); paint(p, 0, 0, '#', 4); cutGrid(p, 8, 8);
    assert.deepEqual(p.spawn, {cell: p.cells[0].id, x: 4, y: 0});
    const solid = createProject(4, 4); paint(solid, 0, 0, '#', 4); cutGrid(solid, 4, 4);
    assert.equal(solid.spawn, null); assert.match(validate(solid).errors.join(' '), /spawn/);
});

test('split partitions terrain and height ownership without losing anchors', () => {
    const p = world(24, 8, 12, 8);
    setHeight(p, 6, 2, 1.5); paint(p, 5, 3, ',', 4);
    p.spawn = {cell: p.cells[0].id, x: 9, y: 5};
    const l = linkTwo(p, {a: {cell: p.cells[0].id, x: 8, y: 2}});
    const terrain = [...p.terrain], heights = {...p.heights};
    const positions = [globalAnchor(p, p.spawn), globalAnchor(p, l.a), globalAnchor(p, l.b)];
    const [a, b] = splitCell(p, p.cells[0].id, 'x', 6);
    assert.equal(a.width, 6); assert.equal(b.x, 6); assert.equal(b.width, 6);
    assert.deepEqual([globalAnchor(p, p.spawn), globalAnchor(p, p.links[0].a), globalAnchor(p, p.links[0].b)], positions);
    assert.equal(p.spawn.cell, b.id); assert.equal(p.links[0].a.cell, b.id);
    assert.deepEqual(p.terrain, terrain); assert.deepEqual(p.heights, heights);
    assert.deepEqual(validate(p).errors, []);
});

test('split supports Y and rejects tiny children, rooms, bad axes, unknown cells', () => {
    const p = world(8, 16, 8, 16), id = p.cells[0].id;
    for (const offset of [0, 3, 13, 16, 4.5]) unchanged(p, () => splitCell(p, id, 'y', offset));
    unchanged(p, () => splitCell(p, id, 'z', 4), /axis/);
    unchanged(p, () => splitCell(p, 'missing', 'y', 4), /world cells/);
    const room = addRoom(p, 8, 8); unchanged(p, () => splitCell(p, room.id, 'x', 4), /world cells/);
    const children = splitCell(p, id, 'y', 8);
    assert.deepEqual(children.map(c => [c.y, c.height]), [[0, 8], [8, 8]]);
});

test('rectangular merge retains first selected metadata and remaps spawn and outside links', () => {
    const p = world(24, 8, 8, 8);
    const [left, middle, right] = p.cells;
    p.spawn = {cell: middle.id, x: 3, y: 4};
    addLink(p, {a: {cell: middle.id, x: 5, y: 2}, b: {cell: right.id, x: 1, y: 2}});
    const positions = [globalAnchor(p, p.spawn), globalAnchor(p, p.links[0].a)];
    p.cells[1].name = 'Retained name';
    const merged = mergeCells(p, [middle.id, left.id]);
    assert.equal(merged.id, middle.id); assert.equal(merged.name, 'Retained name');
    assert.equal(merged.width, 16); assert.equal(merged.x, 0);
    assert.deepEqual([globalAnchor(p, p.spawn), globalAnchor(p, p.links[0].a)], positions);
    assert.deepEqual(validate(p).errors, []);
});

test('merge rejects L shapes, gaps, duplicate IDs, unknown cells, and detached rooms', () => {
    const p = world(16, 16, 8, 8), ids = p.cells.map(c => c.id);
    unchanged(p, () => mergeCells(p, ids.slice(0, 3)), /rectangle/);
    unchanged(p, () => mergeCells(p, [ids[0], ids[3]]), /rectangle/);
    unchanged(p, () => mergeCells(p, [ids[0], ids[0]]), /distinct/);
    unchanged(p, () => mergeCells(p, [ids[0]]), /at least two/);
    unchanged(p, () => mergeCells(p, [ids[0], 'missing']), /existing/);
    const room = addRoom(p); unchanged(p, () => mergeCells(p, [ids[0], room.id]), /existing/);
});

test('merges and recuts refuse conflicting Z, outdoors, and weather metadata', () => {
    for (const [field, value] of [['z', 1], ['outdoors', false], ['weather', 'snow']]) {
        const p = world(); p.cells[1][field] = value;
        unchanged(p, () => mergeCells(p, p.cells.map(c => c.id)), /conflicting/);
        unchanged(p, () => cutGrid(p, 16, 8), /conflicting/);
    }
});

test('merge and recut never silently delete a same-cell portal', () => {
    const p = world(); linkTwo(p);
    unchanged(p, () => mergeCells(p, p.cells.map(c => c.id)), /same-cell/);
    unchanged(p, () => cutGrid(p, 16, 8), /same-cell/);
    assert.equal(p.links.length, 1);
});

test('recut remaps all world anchors through global positions; room anchors do not move', () => {
    const p = world(32, 16, 16, 16), room = addRoom(p, 8, 8);
    p.spawn = {cell: p.cells[0].id, x: 12, y: 12};
    addLink(p, {kind: 'stairs', a: {cell: p.cells[0].id, x: 10, y: 10}, b: {cell: room.id, x: 2, y: 2}});
    const positions = [globalAnchor(p, p.spawn), globalAnchor(p, p.links[0].a), globalAnchor(p, p.links[0].b)];
    cutGrid(p, 8, 8);
    assert.deepEqual([globalAnchor(p, p.spawn), globalAnchor(p, p.links[0].a), globalAnchor(p, p.links[0].b)], positions);
    assert.equal(p.links[0].open, true); assert.deepEqual(validate(p).errors, []);
});

test('detached rooms have isolated terrain, heights and non-colliding IDs', () => {
    const p = world(), room = addRoom(p, 12, 8, 'Cellar'), other = addRoom(p, 4, 4);
    assert.notEqual(room.id, other.id); assert.equal(room.outdoors, false); assert.equal(room.z, 1);
    paint(p, 2, 2, 'T', 2, room.id); setHeight(p, 2, 2, -1, room.id);
    assert.equal(getCell(p, room.id).terrain[2].slice(2, 4), 'TT');
    assert.equal(getCell(p, room.id).heights['2,2'], -1);
    assert.equal(p.terrain[2][2], '.'); assert.deepEqual(p.heights, {});
    assert.match(validate(p).warnings.join(' '), /detached rooms/);
    unchanged(p, () => paint(p, 0, 0, '#', 1, 'unknown'), /Unknown/);
});

test('cellTerrain returns an independent array and rejects unknown cells', () => {
    const p = world(), r = addRoom(p);
    for (const id of [p.cells[0].id, r.id]) {
        const rows = cellTerrain(p, id); rows[0] = 'BROKEN';
        assert.notEqual(cellTerrain(p, id)[0], 'BROKEN');
    }
    assert.equal(getCell(p, 'missing'), null); assert.throws(() => cellTerrain(p, 'missing'), /Unknown/);
});

test('link creation owns copies, allocates IDs and normalizes stairs/passages open', () => {
    const p = world(), a = {cell: p.cells[0].id, x: 2, y: 2}, b = {cell: p.cells[1].id, x: 2, y: 2};
    const door = addLink(p, {a, b}); a.x = 5;
    assert.equal(door.a.x, 2); assert.equal(door.kind, 'door'); assert.equal(door.open, false);
    removeLink(p, door.id); assert.equal(p.links.length, 0);
    for (const kind of ['stairs', 'passage']) {
        const l = addLink(p, {a, b, kind, open: false}); assert.equal(l.open, true); removeLink(p, l.id);
    }
    unchanged(p, () => removeLink(p, 'unknown'), /Unknown/);
});

test('links reject same-cell, reused endpoints, solid terrain, malformed anchors, duplicate IDs', () => {
    const p = world(); linkTwo(p);
    unchanged(p, () => linkTwo(p), /reused/);
    unchanged(p, () => addLink(p, {a: {cell: p.cells[0].id, x: 3, y: 3}, b: {cell: p.cells[0].id, x: 4, y: 4}}), /different cells/);
    unchanged(p, () => addLink(p, {a: {cell: 'unknown', x: 1, y: 1}, b: {cell: p.cells[1].id, x: 3, y: 3}}), /unknown cell/);
    unchanged(p, () => addLink(p, {a: {cell: p.cells[0].id, x: 8, y: 1}, b: {cell: p.cells[1].id, x: 3, y: 3}}), /outside/);
    unchanged(p, () => addLink(p, {a: {cell: p.cells[0].id, x: 1.5, y: 1}, b: {cell: p.cells[1].id, x: 3, y: 3}}), /integers/);
    unchanged(p, () => addLink(p, {id: p.links[0].id, a: {cell: p.cells[0].id, x: 5, y: 5}, b: {cell: p.cells[1].id, x: 5, y: 5}}), /Duplicate/);
    paint(p, 4, 4, '#'); unchanged(p, () => linkTwo(p, {a: {cell: p.cells[0].id, x: 4, y: 4}, b: {cell: p.cells[1].id, x: 4, y: 4}}), /solid/);
});

test('every reciprocal link endpoint needs a free adjacent arrival, not another endpoint', () => {
    const p = world();
    for (const [x, y] of [[2, 1], [3, 2], [2, 3], [1, 2]]) paint(p, x, y, '#');
    unchanged(p, () => linkTwo(p), /neighboring passable arrival/);
    paint(p, 1, 2, '.'); linkTwo(p);
    unchanged(p, () => paint(p, 1, 2, '#'), /neighboring passable arrival/);
    unchanged(p, () => addLink(p, {a: {cell: p.cells[0].id, x: 1, y: 2}, b: {cell: p.cells[1].id, x: 5, y: 5}}), /neighboring passable arrival/);
});

test('painting cannot bury link endpoints or spawn; spawn does not occupy an endpoint', () => {
    const p = world(); linkTwo(p);
    unchanged(p, () => paint(p, 2, 2, '#'), /solid/);
    unchanged(p, () => paint(p, 0, 0, 'T'), /Spawn/);
    unchanged(p, () => addLink(p, {a: {...p.spawn}, b: {cell: p.cells[1].id, x: 5, y: 5}}), /Spawn/);
    paint(p, 2, 2, '~'); assert.deepEqual(validate(p).errors, []);
});

test('validate distinguishes malformed data, structural relations, and export requirements', () => {
    for (const invalid of [null, [], {}, {format: 'other'}, {format: 'ratw-atlas', version: 2}])
        assert.ok(validate(invalid).errors.length);
    const p = world(); p.cells.pop(); p.spawn = null;
    assert.doesNotThrow(() => normalizeProject(p));
    assert.match(validate(p).errors.join(' '), /not assigned.*spawn/s);
    p.cells.push({...p.cells[0], id: 'overlapping'});
    assert.match(validate(p).errors.join(' '), /overlaps/);
    assert.throws(() => normalizeProject(p), /overlaps/);
});

test('normalize returns only schema fields, copies deeply and cannot retain prototype keys', () => {
    const p = world(); p.unknown = {dangerous: true}; p.cells[0].extra = 'discard';
    const input = JSON.parse(JSON.stringify(p).replace('"format":', '"__proto__":{"polluted":true},"format":'));
    const n = normalizeProject(input);
    assert.equal(Object.hasOwn(n, '__proto__'), false); assert.equal(Object.hasOwn(n, 'unknown'), false);
    assert.equal(Object.hasOwn(n.cells[0], 'extra'), false); assert.equal({}.polluted, undefined);
    n.terrain[0] = '#'.repeat(n.width); assert.notEqual(input.terrain[0], n.terrain[0]);
    n.cells[0].name = 'Changed'; assert.notEqual(input.cells[0].name, n.cells[0].name);
});

test('defensive schema rejects malicious IDs, invalid heights, non-finite stats, terrain and types', () => {
    const alterations = [
        p => { p.cells[0].id = '../escape'; }, p => { p.cells[0].id = 'Uppercase'; },
        p => { p.cells[0].id = 'a'.repeat(49); }, p => { p.cells[0].z = NaN; },
        p => { p.cells[0].outdoors = 'false'; }, p => { p.cells[0].weather = 'hail'; },
        p => { p.cells[0].name = 'x\t'; }, p => { p.cells[0].description = 'x\u0000'; },
        p => { p.cells[0].x = -1; }, p => { p.cells[0].width = 256; },
        p => { p.cells[0].terrain = Array(8).fill('.'.repeat(8)); }, p => { p.cells[0].heights = {}; },
        p => { p.heights['01,2'] = 1; }, p => { p.heights['16,0'] = 1; }, p => { p.heights['2,2'] = 0.1; },
        p => { p.heights = JSON.parse('{"__proto__":1}'); }, p => { p.heights['0,0'] = Infinity; },
        p => { p.terrain[0] = ' '.repeat(16); }, p => { p.terrain.pop(); }, p => { p.terrain[0] = 0; },
        p => { p.spawn = undefined; }, p => { p.spawn.x = 0.5; }, p => { p.links = {}; },
        p => { p.version = '1'; }, p => { p.name = 'x'.repeat(121); }, p => { p.cells[0].description = 'x'.repeat(4097); },
    ];
    for (const alter of alterations) {
        const p = world(); alter(p);
        assert.ok(validate(p).errors.length, alter.toString());
        assert.throws(() => normalizeProject(p), undefined, alter.toString());
    }
});

test('duplicate IDs, invalid link open flags, unknown spawn and blocked spawn are rejected', () => {
    const p = world(); linkTwo(p);
    for (const alter of [
        n => { n.cells[1].id = n.cells[0].id; }, n => { n.links[0].open = 1; },
        n => { n.links[0].kind = 'stairs'; }, n => { n.spawn.cell = 'missing'; },
        n => { n.spawn.x = 8; }, n => { n.terrain[0] = '#' + n.terrain[0].slice(1); },
    ]) { const n = clone(p); alter(n); assert.throws(() => normalizeProject(n)); }
});

test('combined cell/room and authored tile limits prevent oversized projects atomically', () => {
    const p = createProject(64, 64); cutGrid(p, 4, 4);
    assert.equal(p.cells.length, 256);
    unchanged(p, () => addRoom(p, 4, 4), /256/);
    const draft = createProject(256, 256);
    unchanged(draft, () => cutGrid(draft, 4, 4), /256/);
    for (let i = 0; i < 3; i++) addRoom(draft, 256, 256);
    unchanged(draft, () => addRoom(draft, 4, 4), /262144/);
});

test('merging four rectangular cells supports later inverse splits with exact terrain', () => {
    const p = world(16, 16, 8, 8);
    for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
        if ((x + y) % 5 === 0 && (x !== 0 || y !== 0)) paint(p, x, y, ',');
        if ((x * 3 + y) % 17 === 0) setHeight(p, x, y, 0.25);
    }
    const before = {terrain: [...p.terrain], heights: {...p.heights}};
    const merged = mergeCells(p, p.cells.map(c => c.id));
    assert.equal(merged.width, 16); assert.equal(merged.height, 16);
    const halves = splitCell(p, merged.id, 'x', 8);
    for (const half of halves) splitCell(p, half.id, 'y', 8);
    assert.equal(p.cells.length, 4); assert.deepEqual(p.terrain, before.terrain); assert.deepEqual(p.heights, before.heights);
    assert.deepEqual(validate(p).errors, []);
});

test('partition property: deterministic randomized recuts preserve every authored tile and anchor', () => {
    let seed = 17; const random = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 2 ** 32; };
    for (let iteration = 0; iteration < 20; iteration++) {
        const p = createProject(32, 24);
        for (let i = 0; i < 30; i++) {
            const x = Math.floor(random() * 32), y = Math.floor(random() * 24);
            paint(p, x, y, [',', '~', ':', '^'][Math.floor(random() * 4)]);
            setHeight(p, x, y, Math.floor(random() * 12) / 4);
        }
        cutGrid(p, 16, 12); const room = addRoom(p, 4, 4);
        p.spawn = {cell: p.cells[0].id, x: 13, y: 10};
        addLink(p, {a: {cell: p.cells[0].id, x: 9, y: 9}, b: {cell: room.id, x: 1, y: 1}});
        const before = clone(p), anchors = [globalAnchor(p, p.spawn), globalAnchor(p, p.links[0].a)];
        cutGrid(p, 8, 8);
        assert.deepEqual(p.terrain, before.terrain); assert.deepEqual(p.heights, before.heights);
        assert.deepEqual([globalAnchor(p, p.spawn), globalAnchor(p, p.links[0].a)], anchors);
        assert.deepEqual(validate(p).errors, []);
    }
});

test('text validation accepts full Unicode code points but rejects unpaired UTF-16 surrogates', () => {
    const p = world(); p.name = '🐺'.repeat(120); p.cells[0].name = '🐺'.repeat(120);
    p.cells[0].description = 'A wolf 🐺 waits.\nSnow falls.';
    assert.doesNotThrow(() => normalizeProject(p));
    assert.deepEqual(validate(p).errors, []);
    const [first, second] = splitCell(p, p.cells[0].id, 'x', 4);
    assert.equal([...first.name].length, 120); assert.equal([...second.name].length, 120);
    assert.doesNotThrow(() => normalizeProject(p));
    for (const bad of ['\ud800', '\udfff', 'prefix\ud83dX', 'X\udc3a', '\ud800\ud800']) {
        const n = clone(p); n.name = bad;
        assert.throws(() => normalizeProject(n), /text/);
        n.name = 'Valid'; n.cells[0].description = bad;
        assert.throws(() => normalizeProject(n), /text/);
    }
    p.name += 'x'; assert.throws(() => normalizeProject(p), /120/);
});

test('link arrival requires height compatibility and height edits preserve a safe exit atomically', () => {
    const p = world();
    for (const [x, y] of [[2, 1], [3, 2], [2, 3]]) paint(p, x, y, '#');
    linkTwo(p);
    unchanged(p, () => setHeight(p, 2, 2, 0.75), /0.55 height/);
    setHeight(p, 1, 2, 0.5); setHeight(p, 2, 2, 1);
    assert.deepEqual(validate(p).errors, []);
    unchanged(p, () => setHeight(p, 1, 2, null), /0.55 height/);
    unchanged(p, () => setHeight(p, 1, 2, 0.25), /0.55 height/);
    paint(p, 1, 2, '^'); setHeight(p, 1, 2, null);
    assert.deepEqual(validate(p).errors, []);
    unchanged(p, () => paint(p, 1, 2, '.'), /0.55 height/);
    unchanged(p, () => paint(p, 1, 2, ':'), /0.55 height/);
});

test('painting an endpoint glyph cannot invalidate its only height-compatible neighbor', () => {
    const p = world();
    for (const [x, y] of [[2, 1], [3, 2], [2, 3]]) paint(p, x, y, '#');
    setHeight(p, 1, 2, -0.5); linkTwo(p);
    unchanged(p, () => paint(p, 2, 2, '^'), /0.55 height/);
    unchanged(p, () => paint(p, 2, 2, ':'), /0.55 height/);
    setHeight(p, 2, 2, -0.25); paint(p, 2, 2, '^');
    assert.deepEqual(validate(p).errors, []); // Explicit override takes precedence over glyph defaults.
});

test('stairs use stamped .5 elevation unless explicitly overridden', () => {
    const p = world();
    for (const [x, y] of [[2, 1], [3, 2], [2, 3]]) paint(p, x, y, '#');
    setHeight(p, 1, 2, -0.25);
    unchanged(p, () => linkTwo(p, {kind: 'stairs'}), /0.55 height/);
    const door = linkTwo(p); removeLink(p, door.id);
    setHeight(p, 2, 2, 0); linkTwo(p, {kind: 'stairs'});
    unchanged(p, () => setHeight(p, 2, 2, null), /0.55 height/);
    setHeight(p, 1, 2, 0); setHeight(p, 2, 2, null);
    assert.deepEqual(validate(p).errors, []); // .5 stair to level floor is a compatible step.
});

test('height validation resolves world-cell offsets and isolated room overrides', () => {
    const p = world(), room = addRoom(p, 8, 8);
    const a = {cell: p.cells[1].id, x: 2, y: 2}, b = {cell: room.id, x: 2, y: 2};
    for (const [x, y] of [[2, 1], [3, 2], [2, 3]]) {
        paint(p, x + 8, y, '#'); paint(p, x, y, '#', 1, room.id);
    }
    addLink(p, {a, b});
    unchanged(p, () => setHeight(p, 10, 2, 1), /0.55 height/);
    unchanged(p, () => setHeight(p, 2, 2, 1, room.id), /0.55 height/);
    setHeight(p, 9, 2, 0.5); setHeight(p, 10, 2, 1);
    setHeight(p, 1, 2, -0.5, room.id); setHeight(p, 2, 2, -1, room.id);
    assert.deepEqual(validate(p).errors, []);
});

test('explicit link cap matches authoritative exporter at 2048', () => {
    const p = world(256, 256, 128, 256);
    for (let i = 0; i < 2048; i++) {
        const x = 2 + (i % 63) * 2, y = 2 + Math.floor(i / 63) * 2;
        p.links.push({id: `test_${i}`, name: 'Door', kind: 'door', open: false,
            a: {cell: p.cells[0].id, x, y}, b: {cell: p.cells[1].id, x, y}});
    }
    assert.deepEqual(validate(p).errors, []);
    unchanged(p, () => addLink(p, {a: {cell: p.cells[0].id, x: 1, y: 100},
        b: {cell: p.cells[1].id, x: 1, y: 100}}), /2048/);
});

test('atlas, cell, room and link names require non-whitespace text; descriptions may be empty', () => {
    for (const bad of ['', ' ', '\u00a0', '\u0085', '\u2003']) {
        assert.throws(() => createProject(8, 8, bad), /nonempty/);
        const p = world();
        unchanged(p, () => addRoom(p, 8, 8, bad), /nonempty/);
        unchanged(p, () => linkTwo(p, {name: bad}), /nonempty/);
        p.cells[0].name = bad; assert.throws(() => normalizeProject(p), /nonempty/);
    }
    const p = world(); p.cells[0].description = '';
    const room = addRoom(p); room.description = '';
    assert.deepEqual(validate(p).errors, []);
});

function politicalWorld() {
    const p = world(16, 8, 8, 8);
    upsertFaction(p, {id: 'north', name: 'North Wardens', color: '#6688AA'});
    upsertFaction(p, {id: 'south', name: 'South Wardens', color: '#CC9988'});
    upsertChapter(p, {id: 'hearth', name: 'Hearth Chapter'});
    return p;
}

test('legacy projects receive independent neutral territory and empty catalogs', () => {
    const p = world(), room = addRoom(p, 8, 8);
    delete p.factions; delete p.chapters;
    for (const c of [...p.cells, ...p.rooms]) delete c.territory;
    const n = normalizeProject(p);
    assert.deepEqual(n.factions, []); assert.deepEqual(n.chapters, []);
    for (const c of [...n.cells, ...n.rooms]) assert.deepEqual(c.territory, defaultTerritory());
    n.cells[0].territory.region = 'changed'; n.cells[0].territory.claims.push('local_only');
    assert.deepEqual(n.cells[1].territory, defaultTerritory());
    assert.deepEqual(getCell(n, room.id).territory, defaultTerritory());
    assert.equal(p.cells[0].territory, undefined);
});

test('political catalogs update labels and colors without renaming or aliasing IDs', () => {
    const p = politicalWorld(), faction = {id: 'north', name: 'Northern Council', color: '#abcdef'};
    setTerritory(p, [p.cells[0].id], {region: 'reach', claims: ['south', 'north'], chapter: 'hearth'});
    upsertFaction(p, faction); faction.name = 'caller mutation';
    upsertChapter(p, {id: 'hearth', name: 'The New Hearth'});
    assert.equal(p.factions.length, 2); assert.equal(p.factions[0].name, 'Northern Council');
    assert.equal(p.chapters.length, 1); assert.equal(p.chapters[0].name, 'The New Hearth');
    assert.deepEqual(p.cells[0].territory, {region: 'reach', claims: ['north', 'south'], chapter: 'hearth'});
    assert.deepEqual(normalizeProject(clone(p)), p);
});

test('political catalogs reject malformed arrays, entries, names, IDs, colors and duplicates', () => {
    const faction = {id: 'north', name: 'North', color: '#112233'}, chapter = {id: 'hearth', name: 'Hearth'};
    for (const bad of [null, {}, true, 'x', [null], [true], [{}], [{...faction, id: '../bad'}],
        [{...faction, id: 'a'.repeat(49)}], [{...faction, name: ' '}], [{...faction, name: '\ud800'}],
        [{...faction, name: 'bad\nline'}], [{...faction, color: '#123'}], [{...faction, color: 'red'}],
        [{...faction, color: '#abcdef00'}], [{...faction, color: true}], [{...faction, color: '#gggggg'}],
        [{...faction, extra: 1}], [faction, faction]]) {
        const p = world(); p.factions = bad; assert.throws(() => normalizeProject(p)); assert.ok(validate(p).errors.length);
    }
    for (const bad of [null, {}, true, [null], [{}], [{...chapter, name: ''}], [{...chapter, id: 4}],
        [{...chapter, color: '#ffffff'}], [chapter, chapter]]) {
        const p = world(); p.chapters = bad; assert.throws(() => normalizeProject(p));
    }
});

test('catalog and claim limits accept their exact bounds and refuse overflow atomically', () => {
    const p = world();
    p.factions = Array.from({length: 64}, (_, i) => ({id: `f_${i}`, name: `Faction ${i}`, color: '#112233'}));
    p.chapters = Array.from({length: 128}, (_, i) => ({id: `c_${i}`, name: `Chapter ${i}`}));
    setTerritory(p, [p.cells[0].id], {region: 'reach', claims: p.factions.map(f => f.id), chapter: 'c_127'});
    assert.deepEqual(validate(p).errors, []);
    unchanged(p, () => upsertFaction(p, {id: 'overflow', name: 'Overflow', color: '#aabbcc'}), /64/);
    unchanged(p, () => upsertChapter(p, {id: 'overflow', name: 'Overflow'}), /128/);
    upsertFaction(p, {id: 'f_63', name: 'Rename at capacity', color: '#aabbcc'});
    upsertChapter(p, {id: 'c_127', name: 'Rename at capacity'});
    assert.equal(p.factions.length, 64); assert.equal(p.chapters.length, 128);
});

test('territory shape and references are strict, including booleans and duplicate claims', () => {
    const valid = {region: 'reach', claims: ['north'], chapter: 'hearth'};
    for (const bad of [null, [], true, {}, {...valid, region: ''}, {...valid, region: '../reach'},
        {...valid, region: true}, {...valid, region: 'a'.repeat(49)}, {...valid, claims: null},
        {...valid, claims: 'north'}, {...valid, claims: ['north', 'north']}, {...valid, claims: [false]},
        {...valid, claims: ['absent']}, {...valid, chapter: true}, {...valid, chapter: 'absent'},
        {...valid, chapter: null}, {...valid, extra: 1}, {region: 'reach', claims: []}]) {
        const p = politicalWorld();
        unchanged(p, () => setTerritory(p, [p.cells[0].id], bad));
        p.cells[0].territory = bad; assert.throws(() => normalizeProject(p));
    }
    const p = politicalWorld();
    setTerritory(p, [p.cells[0].id], {region: 'a'.repeat(48), claims: [], chapter: ''});
    assert.deepEqual(validate(p).errors, []);
});

test('bulk territory edits are atomic and do not alias cells or input objects', () => {
    const p = politicalWorld(), ids = p.cells.map(c => c.id), room = addRoom(p, 8, 8);
    const metadata = {region: 'reach', claims: ['north'], chapter: 'hearth'};
    setTerritory(p, [...ids, room.id], metadata); metadata.claims.push('south');
    assert.deepEqual(p.cells[0].territory.claims, ['north']);
    p.cells[0].territory.claims.push('south');
    assert.deepEqual(p.cells[1].territory.claims, ['north']);
    assert.deepEqual(getCell(p, room.id).territory.claims, ['north']);
    for (const bad of [null, [], [ids[0], ids[0]], [ids[0], 'unknown']]) unchanged(p, () => setTerritory(p, bad, defaultTerritory()));
});

test('catalog deletion refuses all world and detached-room references without erasing them', () => {
    const p = politicalWorld(), room = addRoom(p, 8, 8);
    setTerritory(p, [p.cells[0].id, room.id], {region: 'reach', claims: ['north'], chapter: 'hearth'});
    unchanged(p, () => removeFaction(p, 'north'), /still claimed/);
    unchanged(p, () => removeChapter(p, 'hearth'), /still has a site/);
    setTerritory(p, [p.cells[0].id], defaultTerritory());
    unchanged(p, () => removeFaction(p, 'north'), /still claimed/);
    unchanged(p, () => removeChapter(p, 'hearth'), /still has a site/);
    setTerritory(p, [room.id], defaultTerritory()); removeFaction(p, 'north'); removeChapter(p, 'hearth');
    assert.equal(p.factions.length, 1); assert.deepEqual(p.chapters, []);
    unchanged(p, () => removeFaction(p, 'absent'), /Unknown/);
    unchanged(p, () => removeChapter(p, 'absent'), /Unknown/);
});

test('split and smaller re-cuts independently inherit territory with exact terrain and spawn', () => {
    const p = politicalWorld();
    setTerritory(p, p.cells.map(c => c.id), {region: 'reach', claims: ['north', 'south'], chapter: 'hearth'});
    const terrain = clone(p.terrain), spawn = globalAnchor(p, p.spawn);
    splitCell(p, p.cells[0].id, 'x', 4);
    assert.deepEqual(p.cells[0].territory, p.cells[1].territory);
    assert.notEqual(p.cells[0].territory, p.cells[1].territory);
    assert.notEqual(p.cells[0].territory.claims, p.cells[1].territory.claims);
    cutGrid(p, 4, 4);
    for (const c of p.cells) assert.deepEqual(c.territory, {region: 'reach', claims: ['north', 'south'], chapter: 'hearth'});
    assert.deepEqual(p.terrain, terrain); assert.deepEqual(globalAnchor(p, p.spawn), spawn);
    p.cells[0].territory.claims.pop(); assert.equal(p.cells[1].territory.claims.length, 2);
});

test('merge and larger re-cuts reject every political mismatch with no silent loss', () => {
    for (const other of [{region: 'different', claims: ['north'], chapter: 'hearth'},
        {region: 'reach', claims: ['south'], chapter: 'hearth'},
        {region: 'reach', claims: ['north', 'south'], chapter: 'hearth'},
        {region: 'reach', claims: ['north'], chapter: ''}]) {
        const p = politicalWorld();
        setTerritory(p, [p.cells[0].id], {region: 'reach', claims: ['north'], chapter: 'hearth'});
        setTerritory(p, [p.cells[1].id], other);
        unchanged(p, () => mergeCells(p, p.cells.map(c => c.id)), /conflicting territory/);
        unchanged(p, () => cutGrid(p, 16, 8), /conflicting territory/);
    }
});

test('equal claims with different input ordering and object key order can merge', () => {
    const p = politicalWorld();
    p.cells[0].territory = {chapter: 'hearth', claims: ['south', 'north'], region: 'reach'};
    p.cells[1].territory = {region: 'reach', claims: ['north', 'south'], chapter: 'hearth'};
    mergeCells(p, p.cells.map(c => c.id));
    assert.equal(p.cells.length, 1); assert.deepEqual(p.cells[0].territory.claims, ['north', 'south']);
});

test('re-cuts cannot expand political territory silently into uncovered canvas', () => {
    const p = politicalWorld(); p.cells.pop();
    setTerritory(p, [p.cells[0].id], {region: 'reach', claims: ['north'], chapter: ''});
    unchanged(p, () => cutGrid(p, 16, 8), /unassigned canvas/);
    cutGrid(p, 8, 8);
    assert.deepEqual(p.cells[1].territory, defaultTerritory());
    assert.deepEqual(p.cells[0].territory.claims, ['north']);
});

test('faction, Chapter and cell IDs have independent namespaces', () => {
    const p = politicalWorld(), id = p.cells[0].id;
    upsertFaction(p, {id, name: 'Faction with cell ID', color: '#123456'});
    upsertChapter(p, {id, name: 'Chapter with cell ID'});
    setTerritory(p, [id], {region: id, claims: [id], chapter: id});
    assert.deepEqual(validate(p).errors, []);
});
