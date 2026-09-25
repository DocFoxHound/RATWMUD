/** Pure authoring model. Coordinates are integer tile indices, never runtime positions. */
const GLYPHS = new Set('.#,\"T=~:^+');
const SOLID = new Set(['#', 'T', '=']);
const WEATHER = new Set(['clear', 'rain', 'fog', 'snow']);
const defaultLighting = () => ({artificial: 1, daylightAccess: 1, tone: 'warm'});
export const defaultTerritory = () => ({region: 'unassigned', claims: [], chapter: ''});
const ID = /^[a-z][a-z0-9_-]{0,47}$/;
const own = (o, k) => Object.prototype.hasOwnProperty.call(o, k);
const object = value => value !== null && typeof value === 'object' && !Array.isArray(value)
    && (Object.getPrototypeOf(value) === Object.prototype || Object.getPrototypeOf(value) === null);
const integer = (n, lo, hi) => Number.isInteger(n) && n >= lo && n <= hi;
const fail = message => { throw new Error(message); };

export function clone(value) {
    try { return JSON.parse(JSON.stringify(value)); }
    catch { fail('Project must contain only JSON-serializable data.'); }
}

function checkShape(value) {
    const errors = [];
    const check = (condition, message) => { if (!condition) errors.push(message); };
    if (!object(value)) return ['Project must be an object.'];
    check(value.format === 'ratw-atlas', 'Unsupported project format; expected ratw-atlas.');
    check(value.version === 1, 'Unsupported atlas version; expected 1.');
    const text = (v, label, max = 4096) => check(typeof v === 'string' && [...v].length <= max
        && (max === 4096 || /[^\s\u0085]/u.test(v))
        && [...v].every(character => character.codePointAt(0) < 0xD800 || character.codePointAt(0) > 0xDFFF)
        && !(max === 4096 ? /[\u0000-\u0009\u000b-\u001f\u007f]/ : /[\u0000-\u001f\u007f]/).test(v),
        `${label} must be ${max === 4096 ? '' : 'nonempty '}text of at most ${max} characters without control codes (descriptions may contain newlines).`);
    const dimension = (n, label) => check(integer(n, 4, 256), `${label} must be an integer from 4 to 256.`);
    const id = (v, label) => check(typeof v === 'string' && ID.test(v), `${label} must match ${ID}.`);
    const finite = (v, label) => check(typeof v === 'number' && Number.isFinite(v) && Math.abs(v) <= 1e6,
        `${label} must be a finite number between -1000000 and 1000000.`);
    for (const [key, limit] of [['factions', 64], ['chapters', 128]]) {
        if (!own(value, key)) continue;
        check(Array.isArray(value[key]) && value[key].length <= limit, `${key} must be an array with at most ${limit} entries.`);
        if (!Array.isArray(value[key])) continue;
        const ids = new Set();
        for (const entry of value[key]) {
            if (!object(entry)) { errors.push(`Every ${key} entry must be an object.`); continue; }
            const keys = key === 'factions' ? ['id', 'name', 'color'] : ['id', 'name'];
            check(Object.keys(entry).length === keys.length && keys.every(k => own(entry, k)), `${key} entries require only ${keys.join(', ')}.`);
            id(entry.id, `${key} ID`); text(entry.name, `${key} name`, 120);
            check(!ids.has(entry.id), `Duplicate ${key} ID: ${entry.id}.`); ids.add(entry.id);
            if (key === 'factions') check(typeof entry.color === 'string' && /^#[0-9a-fA-F]{6}$/.test(entry.color), 'Faction color must be a six-digit #RRGGBB color.');
        }
    }
    const terrain = (rows, width, height, label) => {
        check(Array.isArray(rows) && rows.length === height, `${label} must have exactly ${height} rows.`);
        if (!Array.isArray(rows)) return;
        for (let i = 0; i < Math.min(rows.length, 257); i++)
            check(typeof rows[i] === 'string' && rows[i].length === width && [...rows[i]].every(g => GLYPHS.has(g)),
                `${label} row ${i} must contain exactly ${width} supported ASCII terrain glyphs.`);
    };
    const heights = (map, width, height, label) => {
        if (!object(map)) { errors.push(`${label} must be a sparse coordinate object.`); return; }
        for (const [key, v] of Object.entries(map)) {
            const parts = /^(0|[1-9][0-9]*),(0|[1-9][0-9]*)$/.exec(key);
            check(parts && integer(Number(parts[1]), 0, width - 1) && integer(Number(parts[2]), 0, height - 1),
                `${label} coordinate ${key} is outside the terrain or malformed.`);
            check(typeof v === 'number' && Number.isFinite(v) && v >= -16 && v <= 16 && Number.isInteger(v * 4),
                `${label} ${key} must be a quarter-tile height from -16 to 16.`);
        }
    };
    text(value.name, 'Atlas name', 120);
    dimension(value.width, 'Atlas width'); dimension(value.height, 'Atlas height');
    terrain(value.terrain, value.width, value.height, 'Atlas terrain');
    heights(value.heights, value.width, value.height, 'Atlas heights');
    for (const list of ['cells', 'rooms', 'links']) check(Array.isArray(value[list]), `${list} must be an array.`);
    if (!Array.isArray(value.cells) || !Array.isArray(value.rooms) || !Array.isArray(value.links)) return errors;
    check(value.cells.length + value.rooms.length <= 256, 'An atlas supports at most 256 world cells and detached rooms combined.');
    check(value.links.length <= 2048, 'An atlas supports at most 2048 explicit links.');
    const tileCount = value.width * value.height + value.rooms.reduce((n, r) => n + (object(r) ? r.width * r.height : 0), 0);
    check(Number.isFinite(tileCount) && tileCount <= 262144, 'An atlas supports at most 262144 authored tiles including detached rooms.');
    for (const [group, items] of [['Cell', value.cells], ['Room', value.rooms]]) {
        for (const [i, c] of items.entries()) {
            const label = `${group} ${i}`;
            if (!object(c)) { errors.push(`${label} must be an object.`); continue; }
            id(c.id, `${label} ID`); text(c.name, `${label} name`, 120); text(c.description, `${label} description`);
            dimension(c.width, `${label} width`); dimension(c.height, `${label} height`);
            finite(c.z, `${label} z`);
            check(typeof c.outdoors === 'boolean', `${label} outdoors must be boolean.`);
            check(WEATHER.has(c.weather), `${label} weather must be clear, rain, fog, or snow.`);
            if (own(c, 'territory')) {
                const territory = c.territory;
                check(object(territory), `${label} territory must be an object.`);
                if (object(territory)) {
                    check(Object.keys(territory).length === 3 && ['region', 'claims', 'chapter'].every(k => own(territory, k)),
                        `${label} territory requires only region, claims and chapter.`);
                    id(territory.region, `${label} territory region`);
                    check(territory.chapter === '' || typeof territory.chapter === 'string' && ID.test(territory.chapter), `${label} territory chapter must be an ID or empty.`);
                    check(Array.isArray(territory.claims) && territory.claims.length <= 64, `${label} territory claims must be an array of at most 64 faction IDs.`);
                    if (Array.isArray(territory.claims)) {
                        for (const claim of territory.claims) id(claim, `${label} territory claim`);
                        check(new Set(territory.claims).size === territory.claims.length, `${label} territory claims cannot contain duplicates.`);
                    }
                }
            }
            if (own(c, 'lighting')) {
                const light = c.lighting;
                check(object(light), `${label} lighting must be an object.`);
                if (object(light)) {
                    check(Object.keys(light).length === 3 && ['artificial', 'daylightAccess', 'tone'].every(k => own(light, k)),
                        `${label} lighting requires only artificial, daylightAccess and tone.`);
                    for (const key of ['artificial', 'daylightAccess'])
                        check(typeof light[key] === 'number' && Number.isFinite(light[key]) && light[key] >= 0 && light[key] <= 1,
                            `${label} lighting ${key} must be a number from 0 to 1.`);
                    check(['warm', 'neutral', 'cool'].includes(light.tone), `${label} lighting tone must be warm, neutral or cool.`);
                }
            }
            if (group === 'Cell') {
                check(!own(c, 'terrain') && !own(c, 'heights'), `${label} terrain and heights belong to the shared atlas, not the individual world cell.`);
                check(integer(c.x, 0, value.width - 1) && integer(c.y, 0, value.height - 1), `${label} origin must be inside the atlas.`);
                check(c.x + c.width <= value.width && c.y + c.height <= value.height, `${label} extends outside the atlas.`);
            } else {
                terrain(c.terrain, c.width, c.height, `${label} terrain`);
                heights(c.heights, c.width, c.height, `${label} heights`);
                finite(c.worldX, `${label} worldX`); finite(c.worldY, `${label} worldY`);
            }
        }
    }
    const anchor = (a, label) => {
        if (!object(a)) { errors.push(`${label} must be an anchor object.`); return; }
        id(a.cell, `${label} cell`);
        check(integer(a.x, 0, 255) && integer(a.y, 0, 255), `${label} coordinates must be tile integers from 0 to 255.`);
    };
    for (const [i, link] of value.links.entries()) {
        if (!object(link)) { errors.push(`Link ${i} must be an object.`); continue; }
        id(link.id, `Link ${i} ID`); text(link.name, `Link ${i} name`, 120);
        check(['door', 'passage', 'stairs'].includes(link.kind), `Link ${i} kind must be door, passage, or stairs.`);
        check(typeof link.open === 'boolean', `Link ${i} open must be boolean.`);
        check(link.kind === 'door' || link.open === true, `Link ${i}: stairs and passages must always be open.`);
        anchor(link.a, `Link ${i} endpoint A`); anchor(link.b, `Link ${i} endpoint B`);
    }
    if (value.spawn !== null) anchor(value.spawn, 'Spawn');
    return errors;
}

function canonical(value) {
    const anchor = a => ({cell: a.cell, x: a.x, y: a.y});
    const metadata = c => ({id: c.id, name: c.name, description: c.description,
        width: c.width, height: c.height, z: c.z, outdoors: c.outdoors, weather: c.weather,
        lighting: {...(c.lighting ?? defaultLighting())},
        territory: {region: c.territory?.region ?? 'unassigned', claims: [...(c.territory?.claims ?? [])].sort(), chapter: c.territory?.chapter ?? ''}});
    return {format: 'ratw-atlas', version: 1, name: value.name, width: value.width, height: value.height,
        terrain: [...value.terrain], heights: {...value.heights},
        factions: (value.factions ?? []).map(f => ({id: f.id, name: f.name, color: f.color})),
        chapters: (value.chapters ?? []).map(c => ({id: c.id, name: c.name})),
        cells: value.cells.map(c => ({...metadata(c), x: c.x, y: c.y})),
        rooms: value.rooms.map(c => ({...metadata(c), terrain: [...c.terrain], heights: {...c.heights}, worldX: c.worldX, worldY: c.worldY})),
        links: value.links.map(l => ({id: l.id, name: l.name, kind: l.kind, a: anchor(l.a), b: anchor(l.b), open: l.open})),
        spawn: value.spawn === null ? null : anchor(value.spawn)};
}

export function getCell(project, id) {
    return project.cells.find(c => c.id === id) ?? project.rooms.find(c => c.id === id) ?? null;
}

export function cellTerrain(project, id) {
    const c = getCell(project, id);
    if (!c) fail(`Unknown cell: ${id}.`);
    return own(c, 'terrain') ? [...c.terrain]
        : project.terrain.slice(c.y, c.y + c.height).map(row => row.slice(c.x, c.x + c.width));
}

function glyphAt(project, c, x, y) {
    if (!integer(x, 0, c.width - 1) || !integer(y, 0, c.height - 1)) return null;
    return own(c, 'terrain') ? c.terrain[y][x] : project.terrain[y + c.y][x + c.x];
}
function heightAt(project, c, x, y, kind = null) {
    const source = own(c, 'terrain') ? c : project;
    const px = own(c, 'terrain') ? x : c.x + x, py = own(c, 'terrain') ? y : c.y + y;
    const coordinate = `${px},${py}`;
    if (own(source.heights, coordinate)) return source.heights[coordinate];
    if (kind === 'stairs') return 0.5; // Runtime export stamps stairs over the authored glyph.
    const glyph = glyphAt(project, c, x, y);
    return glyph === '^' ? 0.5 : glyph === ':' ? 0.25 : 0;
}
const keyFor = a => `${a.cell}:${a.x},${a.y}`;
const walkable = glyph => glyph !== null && !SOLID.has(glyph);

function relationships(project, requireExport = false) {
    const errors = [], warnings = [];
    const all = [...project.cells, ...project.rooms], ids = new Set();
    const factionIds = new Set(project.factions.map(f => f.id)), chapterIds = new Set(project.chapters.map(c => c.id));
    for (const c of all) {
        if (ids.has(c.id)) errors.push(`Duplicate cell/room ID: ${c.id}.`);
        ids.add(c.id);
        for (const claim of c.territory.claims) if (!factionIds.has(claim)) errors.push(`Cell ${c.id} territory references unknown faction ${claim}.`);
        if (c.territory.chapter && !chapterIds.has(c.territory.chapter)) errors.push(`Cell ${c.id} territory references unknown chapter ${c.territory.chapter}.`);
    }
    const occupied = new Uint8Array(project.width * project.height);
    for (const c of project.cells) {
        let overlap = false;
        for (let y = c.y; y < c.y + c.height; y++) for (let x = c.x; x < c.x + c.width; x++) {
            const i = y * project.width + x;
            if (occupied[i]) overlap = true;
            occupied[i] = 1;
        }
        if (overlap) errors.push(`Cell ${c.id} overlaps another world cell.`);
    }
    const uncovered = occupied.reduce((n, tile) => n + (tile === 0 ? 1 : 0), 0);
    if (uncovered) (requireExport ? errors : warnings).push(`${uncovered} world tiles are not assigned to cells. Cut or complete the world partition before export.`);
    const endpoints = new Set(), linkIds = new Set();
    for (const link of project.links) {
        if (linkIds.has(link.id)) errors.push(`Duplicate link ID: ${link.id}.`);
        linkIds.add(link.id);
        if (link.a.cell === link.b.cell) errors.push(`Link ${link.id} must connect two different cells.`);
        for (const a of [link.a, link.b]) {
            const c = getCell(project, a.cell), key = keyFor(a);
            if (endpoints.has(key)) errors.push(`Link endpoint ${key} is reused.`);
            endpoints.add(key);
            if (!c) errors.push(`Link ${link.id} references unknown cell ${a.cell}.`);
            else if (!walkable(glyphAt(project, c, a.x, a.y))) errors.push(`Link ${link.id} endpoint ${key} is outside its cell or on solid terrain.`);
        }
    }
    for (const link of project.links) for (const a of [link.a, link.b]) {
        const c = getCell(project, a.cell);
        if (!c) continue;
        const sourceHeight = heightAt(project, c, a.x, a.y, link.kind);
        if (![[0, -1], [1, 0], [0, 1], [-1, 0]].some(([dx, dy]) =>
            walkable(glyphAt(project, c, a.x + dx, a.y + dy))
            && !endpoints.has(keyFor({cell: a.cell, x: a.x + dx, y: a.y + dy}))
            && Math.abs(heightAt(project, c, a.x + dx, a.y + dy) - sourceHeight) <= 0.55))
            errors.push(`Link ${link.id} endpoint ${keyFor(a)} needs a neighboring passable arrival tile within 0.55 height, not occupied by another link.`);
    }
    if (project.spawn) {
        const c = getCell(project, project.spawn.cell);
        if (!c) errors.push(`Spawn references unknown cell ${project.spawn.cell}.`);
        else if (!walkable(glyphAt(project, c, project.spawn.x, project.spawn.y))) errors.push('Spawn must be inside its cell on non-solid terrain.');
        if (endpoints.has(keyFor(project.spawn))) errors.push('Spawn cannot occupy a link endpoint.');
    } else if (requireExport) errors.push('A passable player spawn is required before export.');
    if (project.rooms.some(r => !project.links.some(l => l.a.cell === r.id || l.b.cell === r.id)))
        warnings.push('One or more detached rooms have no links and cannot be reached from the atlas.');
    if (project.cells.length > 1) warnings.push('Automatic seams are generated only at compatible traversable edges on the same Z level.');
    return {errors, warnings};
}

export function normalizeProject(value) {
    const errors = checkShape(value);
    if (errors.length) fail(errors.join('\n'));
    const safe = canonical(value), checks = relationships(safe);
    if (checks.errors.length) fail(checks.errors.join('\n'));
    return safe;
}

export function validate(value) {
    try {
        const errors = checkShape(value);
        if (errors.length) return {errors, warnings: []};
        return relationships(canonical(value), true);
    } catch { return {errors: ['Project could not be validated; malformed authoring data.'], warnings: []}; }
}

function atomic(project, operation) {
    const next = normalizeProject(project);
    const result = operation(next);
    const valid = normalizeProject(next);
    for (const key of Object.keys(project)) delete project[key];
    Object.assign(project, valid);
    return result;
}

function freshId(project, prefix, used = new Set()) {
    for (const c of [...project.cells, ...project.rooms, ...project.links]) used.add(c.id);
    for (let i = 1; i < 100000; i++) { const id = `${prefix}_${i}`; if (!used.has(id)) { used.add(id); return id; } }
    fail('Could not allocate a unique authoring ID.');
}

export function createProject(width = 64, height = 48, name = 'Untitled atlas') {
    if (!integer(width, 4, 256) || !integer(height, 4, 256)) fail('Atlas dimensions must be integers from 4 to 256.');
    return normalizeProject({format: 'ratw-atlas', version: 1, name, width, height,
        terrain: Array(height).fill('.'.repeat(width)), heights: {}, cells: [], rooms: [], links: [], spawn: null});
}

function paintTarget(project, roomId) {
    if (roomId === null) return project;
    const room = project.rooms.find(r => r.id === roomId);
    if (!room) fail(`Unknown detached room: ${roomId}. World cells are painted in global atlas coordinates.`);
    return room;
}

export function paint(project, x, y, glyph, size = 1, roomId = null) {
    atomic(project, next => {
        const target = paintTarget(next, roomId);
        if (!integer(x, 0, target.width - 1) || !integer(y, 0, target.height - 1)) fail('Brush origin must be an integer tile inside the terrain.');
        if (typeof glyph !== 'string' || glyph.length !== 1 || !GLYPHS.has(glyph)) fail('Choose a supported ASCII terrain glyph.');
        if (!integer(size, 1, 8)) fail('Brush size must be an integer from 1 to 8.');
        const endX = Math.min(x + size, target.width);
        for (let row = y; row < Math.min(y + size, target.height); row++)
            target.terrain[row] = target.terrain[row].slice(0, x) + glyph.repeat(endX - x) + target.terrain[row].slice(endX);
    });
    return project;
}

export function setHeight(project, x, y, value, roomId = null) {
    atomic(project, next => {
        const target = paintTarget(next, roomId);
        if (!integer(x, 0, target.width - 1) || !integer(y, 0, target.height - 1)) fail('Elevation coordinate must be an integer tile inside the terrain.');
        if (value !== null && !(typeof value === 'number' && Number.isFinite(value) && value >= -16 && value <= 16 && Number.isInteger(value * 4)))
            fail('Elevation must be null or a quarter-tile height from -16 to 16.');
        if (value === null) delete target.heights[`${x},${y}`];
        else target.heights[`${x},${y}`] = value;
    });
    return project;
}

function metadataCompatible(cells) {
    if (cells.some(c => c.z !== cells[0].z || c.outdoors !== cells[0].outdoors || c.weather !== cells[0].weather))
        fail('Cannot combine cells with conflicting Z, outdoors, or weather metadata. Make their settings agree first.');
    if (cells.some(c => ['artificial', 'daylightAccess', 'tone'].some(k => c.lighting[k] !== cells[0].lighting[k])))
        fail('Cannot combine cells with conflicting lighting. Make their lighting settings agree first.');
    if (cells.some(c => JSON.stringify(c.territory) !== JSON.stringify(cells[0].territory)))
        fail('Cannot combine cells with conflicting territory, region, faction claims, or Chapter sites. Reconcile political settings first.');
}

function applyPartition(project, nextCells) {
    const oldCells = project.cells;
    const remap = a => {
        if (!a) return null;
        const previous = oldCells.find(c => c.id === a.cell);
        if (!previous) return {...a}; // Detached room anchor.
        const x = previous.x + a.x, y = previous.y + a.y;
        const next = nextCells.find(c => x >= c.x && y >= c.y && x < c.x + c.width && y < c.y + c.height);
        if (!next) fail(`Partition would orphan an anchor at world tile ${x},${y}.`);
        return {cell: next.id, x: x - next.x, y: y - next.y};
    };
    const links = project.links.map(l => ({...l, a: remap(l.a), b: remap(l.b)}));
    if (links.some(l => l.a.cell === l.b.cell)) fail('Partition would turn an existing cross-cell link into a same-cell portal. Remove or relocate that link first.');
    project.spawn = remap(project.spawn); project.links = links; project.cells = nextCells;
    if (!project.spawn) {
        const taken = new Set(project.links.flatMap(l => [keyFor(l.a), keyFor(l.b)]));
        outer: for (const c of nextCells) for (let y = 0; y < c.height; y++) for (let x = 0; x < c.width; x++) {
            const a = {cell: c.id, x, y};
            if (walkable(glyphAt(project, c, x, y)) && !taken.has(keyFor(a))) { project.spawn = a; break outer; }
        }
    }
}

export function cutGrid(project, width = 32, height = 24) {
    atomic(project, next => {
        if (!integer(width, 4, 256) || !integer(height, 4, 256)) fail('Cut sizes must be integers from 4 to 256.');
        for (const [total, size, axis] of [[next.width, width, 'width'], [next.height, height, 'height']])
            if (total % size > 0 && total % size < 4) fail(`The cut leaves a ${total % size}-tile ${axis} sliver. Every cell must be at least 4×4; choose a different cut size.`);
        if (Math.ceil(next.width / width) * Math.ceil(next.height / height) + next.rooms.length > 256)
            fail('This cut exceeds the 256-cell and room limit.');
        const cells = [], reserved = new Set([...next.rooms, ...next.cells].map(c => c.id));
        for (let y = 0; y < next.height; y += height) for (let x = 0; x < next.width; x += width) {
            const w = Math.min(width, next.width - x), h = Math.min(height, next.height - y);
            const overlaps = next.cells.filter(c => c.x < x + w && c.x + c.width > x && c.y < y + h && c.y + c.height > y);
            if (overlaps.length) metadataCompatible(overlaps);
            const coveredArea = overlaps.reduce((sum, c) => sum + (Math.min(c.x + c.width, x + w) - Math.max(c.x, x))
                * (Math.min(c.y + c.height, y + h) - Math.max(c.y, y)), 0);
            if (coveredArea < w * h && overlaps.some(c => JSON.stringify(c.territory) !== JSON.stringify(defaultTerritory())))
                fail('Cannot extend assigned territory across unassigned canvas during a re-cut. Cut smaller cells first, then explicitly assign their territory.');
            const exact = overlaps.find(c => c.x === x && c.y === y && c.width === w && c.height === h);
            const origin = overlaps.find(c => c.x === x && c.y === y && !cells.some(n => n.id === c.id));
            const source = exact ?? origin ?? overlaps[0];
            const retained = exact ?? origin;
            cells.push({id: retained?.id ?? freshId(next, 'cell', reserved), name: source?.name ?? `World cell ${cells.length + 1}`,
                description: source?.description ?? '', x, y, width: w, height: h, z: source?.z ?? 0,
                outdoors: source?.outdoors ?? true, weather: source?.weather ?? 'clear',
                lighting: {...(source?.lighting ?? defaultLighting())}, territory: clone(source?.territory ?? defaultTerritory())});
        }
        applyPartition(next, cells);
    });
    return project.cells;
}

export function splitCell(project, id, axis, offset) {
    let childIds;
    atomic(project, next => {
        const c = next.cells.find(cell => cell.id === id);
        if (!c) fail('Only world cells can be split; detached rooms are independently sized maps.');
        if (axis !== 'x' && axis !== 'y') fail('Split axis must be x or y.');
        const extent = axis === 'x' ? 'width' : 'height';
        if (!integer(offset, 4, c[extent] - 4)) fail('Both split children must be at least 4 tiles wide and high.');
        const first = {...c, [extent]: offset};
        const second = {...c, id: freshId(next, 'cell'), name: `${[...c.name].slice(0, 112).join('')} (split)`,
            [axis]: c[axis] + offset, [extent]: c[extent] - offset};
        childIds = [first.id, second.id];
        applyPartition(next, next.cells.flatMap(cell => cell.id === id ? [first, second] : [cell]));
    });
    return childIds.map(childId => getCell(project, childId));
}

export function mergeCells(project, ids) {
    let mergedId;
    atomic(project, next => {
        if (!Array.isArray(ids) || ids.length < 2 || new Set(ids).size !== ids.length) fail('Choose at least two distinct world cells to merge.');
        const cells = ids.map(id => next.cells.find(c => c.id === id));
        if (cells.some(c => !c)) fail('Only existing world cells can be merged, not detached rooms.');
        metadataCompatible(cells);
        const x = Math.min(...cells.map(c => c.x)), y = Math.min(...cells.map(c => c.y));
        const width = Math.max(...cells.map(c => c.x + c.width)) - x, height = Math.max(...cells.map(c => c.y + c.height)) - y;
        if (cells.reduce((n, c) => n + c.width * c.height, 0) !== width * height) fail('Selected cells must form one filled rectangle without gaps or L-shaped edges.');
        const merged = {...cells[0], x, y, width, height}; mergedId = merged.id;
        let inserted = false;
        const partition = next.cells.flatMap(c => {
            if (!ids.includes(c.id)) return [c];
            if (inserted) return [];
            inserted = true; return [merged];
        });
        applyPartition(next, partition);
    });
    return getCell(project, mergedId);
}

export function addRoom(project, width = 16, height = 12, name = 'New interior') {
    let id;
    atomic(project, next => {
        if (!integer(width, 4, 256) || !integer(height, 4, 256)) fail('Room dimensions must be integers from 4 to 256.');
        id = freshId(next, 'room');
        next.rooms.push({id, name, description: '', width, height, terrain: Array(height).fill('.'.repeat(width)),
            heights: {}, worldX: 0, worldY: 0, z: 1, outdoors: false, weather: 'clear'});
    });
    return getCell(project, id);
}

export function addLink(project, link) {
    let id;
    atomic(project, next => {
        if (!object(link)) fail('Link must be an object.');
        id = link.id ?? freshId(next, 'link');
        next.links.push({id, name: link.name ?? 'New link', kind: link.kind ?? 'door',
            a: clone(link.a), b: clone(link.b), open: ['stairs', 'passage'].includes(link.kind) ? true : (link.open ?? false)});
    });
    return project.links.find(l => l.id === id);
}

export function removeLink(project, id) {
    atomic(project, next => {
        if (!next.links.some(l => l.id === id)) fail(`Unknown link: ${id}.`);
        next.links = next.links.filter(l => l.id !== id);
    });
    return project;
}

/** Catalog IDs are stable keys: updating a label/color does not rename references. */
export function upsertFaction(project, faction) {
    atomic(project, next => {
        if (!object(faction)) fail('Faction must be an object.');
        const at = next.factions.findIndex(f => f.id === faction.id);
        if (at < 0) next.factions.push(clone(faction)); else next.factions[at] = clone(faction);
    });
    return project.factions.find(f => f.id === faction.id);
}

export function upsertChapter(project, chapter) {
    atomic(project, next => {
        if (!object(chapter)) fail('Chapter must be an object.');
        const at = next.chapters.findIndex(c => c.id === chapter.id);
        if (at < 0) next.chapters.push(clone(chapter)); else next.chapters[at] = clone(chapter);
    });
    return project.chapters.find(c => c.id === chapter.id);
}

export function removeFaction(project, id) {
    atomic(project, next => {
        if (!next.factions.some(f => f.id === id)) fail(`Unknown faction: ${id}.`);
        if ([...next.cells, ...next.rooms].some(c => c.territory.claims.includes(id))) fail('Faction is still claimed on a cell or room. Remove its territory claims before deleting it.');
        next.factions = next.factions.filter(f => f.id !== id);
    });
    return project;
}

export function removeChapter(project, id) {
    atomic(project, next => {
        if (!next.chapters.some(c => c.id === id)) fail(`Unknown chapter: ${id}.`);
        if ([...next.cells, ...next.rooms].some(c => c.territory.chapter === id)) fail('Chapter still has a site in a cell or room. Clear those sites before deleting it.');
        next.chapters = next.chapters.filter(c => c.id !== id);
    });
    return project;
}

export function setTerritory(project, ids, territory) {
    atomic(project, next => {
        if (!Array.isArray(ids) || !ids.length || new Set(ids).size !== ids.length) fail('Choose one or more distinct cells or rooms for territory settings.');
        for (const id of ids) {
            const cell = getCell(next, id);
            if (!cell) fail(`Unknown territory cell: ${id}.`);
            cell.territory = clone(territory);
        }
    });
    return project;
}
