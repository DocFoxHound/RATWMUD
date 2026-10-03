/** Pure authoring model. Coordinates are integer tile indices, never runtime positions. */
import {TERRAIN, TERRAIN_CATEGORIES} from './terrain.generated.mjs';

// --- Terrain: one catalog (Data/Terrain/terrain.json, generated into terrain.generated.mjs). Each tile is stored as
// its one-character ASCII code and drawn as its Unicode glyph; solidity, ramps and default heights all come from it.
export {TERRAIN, TERRAIN_CATEGORIES};
/** Catalog tiles by stored code. */
export const TERRAIN_BY_CODE = new Map(TERRAIN.map(t => [t.code, t]));
/** Every stored terrain code, in palette order. */
export const TERRAIN_CODES = TERRAIN.map(t => t.code);
const SOLID = new Set(TERRAIN.filter(t => t.solid).map(t => t.code));
const RAMP = new Set(TERRAIN.filter(t => t.ramp).map(t => t.code));
/** Whether `code` is one catalog terrain code. */
export const isTerrainCode = code => typeof code === 'string' && TERRAIN_BY_CODE.has(code);
/** The catalog tile for a code, or undefined. */
export const terrainTile = code => TERRAIN_BY_CODE.get(code);
/** Ramps (Slope, Stairs) make a full step to or from them walkable. */
export const isRamp = code => RAMP.has(code);
export const WEATHERS = ['clear', 'overcast', 'rain', 'storm', 'fog', 'snow', 'sandstorm'];
const WEATHER = new Set(WEATHERS);
const defaultLighting = () => ({artificial: 1, daylightAccess: 1, tone: 'warm'});
export const defaultTerritory = () => ({region: 'unassigned', claims: [], chapter: ''});
const ID = /^[a-z][a-z0-9_-]{0,47}$/;
/**
 * World cells may sit anywhere within this many tiles of world 0,0: in practice, no limit. Each cell holds its own
 * ground (atlas v3), so a world costs what its cells cost, however far apart they are. Game servers stream cells
 * from the database; only ▶ Play (a local game loading every cell) is limited to GAME_TILES and GAME_AREAS.
 */
export const WORLD_REACH = 1e9;
const LEGACY_CANVAS = 4096;          // Atlas v1/v2 kept one shared canvas of at most this many tiles a side.
const GAME_TILES = 262144, GAME_AREAS = 256;
const MAX_PEOPLE = 16384;                            // Residents the game's society holds (MaxResidents, RatwSociety.h).
/** Cells are bucketed on this grid (their largest size) to find them by tile without scanning them all. */
const BUCKET = 256;
const own = (o, k) => Object.prototype.hasOwnProperty.call(o, k);
const object = value => value !== null && typeof value === 'object' && !Array.isArray(value)
    && (Object.getPrototypeOf(value) === Object.prototype || Object.getPrototypeOf(value) === null);
const integer = (n, lo, hi) => Number.isInteger(n) && n >= lo && n <= hi;
const fail = message => { throw new Error(message); };

// --- Elevation. Tile heights are half steps (0.5) from -16 to 16; the game server and tools/map_editor.py use the
// same rules. Each tile's default height is the catalog's `height` (only `^` Stairs: +0.5). Cardinal moves between
// passable tiles: a height change of at most 0.5 is a free step, exactly 1 needs a ramp (catalog `ramp`: Slope,
// Stairs) on either tile, and more than 1 is a ledge nobody climbs. Solid tiles (cliffs included) are never entered.
export const MIN_HEIGHT = -16, MAX_HEIGHT = 16;
const halfStep = v => typeof v === 'number' && Number.isFinite(v) && v >= MIN_HEIGHT && v <= MAX_HEIGHT && Number.isInteger(v * 2);
/** The height a glyph stands at without an override. */
export const defaultHeight = glyph => TERRAIN_BY_CODE.get(glyph)?.height ?? 0;
/** Rounds a legacy (quarter-step) height to the nearest half step, within -16 to 16. */
export const roundHeight = h => Math.max(MIN_HEIGHT, Math.min(MAX_HEIGHT, Math.round(h * 2) / 2)) + 0;   // + 0: never -0.
/** Solid glyphs are never entered. */
export const isSolid = glyph => SOLID.has(glyph);
/**
 * How the edge between two cardinally adjacent tiles reads: 'flat' (same height), 'step' (a free half step),
 * 'slope' (a full step, walkable because either tile is a ramp: `:` Slope or `^` Stairs), 'ledge' (a change nobody can walk)
 * or 'solid' (either tile is solid: walls, furniture, cliffs). Glyphs may be null (outside the ground): 'solid'.
 */
export function edgeKind(glyphA, heightA, glyphB, heightB) {
    if (glyphA == null || glyphB == null || SOLID.has(glyphA) || SOLID.has(glyphB)) return 'solid';
    const dh = Math.abs(heightA - heightB);
    if (dh < 1e-6) return 'flat';
    if (dh <= 0.5 + 1e-6) return 'step';
    if (dh <= 1 + 1e-6 && (RAMP.has(glyphA) || RAMP.has(glyphB))) return 'slope';
    return 'ledge';
}
/** Whether a walker can move between two cardinally adjacent tiles (either way). */
export const canStep = (glyphA, heightA, glyphB, heightB) => ['flat', 'step', 'slope'].includes(edgeKind(glyphA, heightA, glyphB, heightB));
/** Legacy documents held quarter steps: round them to half steps so they open. Anything else stays for the checks. */
function roundLegacyHeights(value) {
    if (!object(value)) return value;
    const quarter = h => typeof h === 'number' && h >= MIN_HEIGHT && h <= MAX_HEIGHT && Number.isInteger(h * 4) && !Number.isInteger(h * 2);
    const needs = map => object(map) && Object.values(map).some(quarter);
    const rounded = map => Object.fromEntries(Object.entries(map).map(([k, h]) => [k, quarter(h) ? roundHeight(h) : h]));
    // Checked cells hold half steps already (and unchanged ones are skipped), so only a scan of new heights is needed.
    const stale = c => object(c) && !pristine(c) && needs(c.heights);
    const fix = list => Array.isArray(list) && list.some(stale) ? list.map(c => (stale(c) ? {...c, heights: rounded(c.heights)} : c)) : list;
    const cells = fix(value.cells), rooms = fix(value.rooms), canvas = needs(value.heights);
    if (cells === value.cells && rooms === value.rooms && !canvas) return value;
    return {...value, cells, rooms, ...(canvas ? {heights: rounded(value.heights)} : {})};
}

// World content (atlas v2): residents, patrol routes, the town economy and the herb patch.
export const ROLES = ['merchant', 'guard', 'civilian'];
export const SPECIES = ['timber', 'maned', 'arctic', 'red', 'ethiopian'];
export const SEXES = ['female', 'male'];
export const STATURES = ['short', 'average', 'tall'];
export const PATTERNS = ['solid', 'saddle', 'mantle', 'piebald'];
export const COATS = [['ivory', '#E1D9C6'], ['silver', '#ADB3B2'], ['ash', '#777D7B'], ['stone', '#8E8271'],
    ['sable', '#65513F'], ['charcoal', '#303534'], ['rust', '#A26843'], ['sand', '#BEAA84']];
const ECONOMY_LIMITS = {treasury: 1000000, storeHerbs: 10000, storeMeals: 10000, dailyHerbs: 1000, dailyMeals: 1000};
export const defaultEconomy = () => ({treasury: 1000, storeHerbs: 100, storeMeals: 50, dailyHerbs: 10, dailyMeals: 12});
const PERSON_KEYS = ['id', 'name', 'role', 'description', 'greeting', 'workLabel', 'age', 'appearance', 'voice',
    'hours', 'route', 'paid', 'purse', 'herbs', 'meals', 'home', 'work', 'evening', 'personality', 'backstory'];
const SLOT_KEYS = ['id', 'name', 'profession', 'workLabel', 'hours', 'route', 'paid', 'purse', 'herbs', 'meals', 'home', 'work', 'evening'];
export const worldIdFor = name => { const s = String(name).toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '').slice(0, 40) || 'world'; return /^[a-z]/.test(s) ? s : `w_${s}`; };
const reservedPersonId = id => id === 'treasury' || id.startsWith('wolf-') || id.startsWith('player-');
const NEIGHBORS8 = [[0, 1], [0, -1], [1, 0], [-1, 0], [1, 1], [-1, 1], [1, -1], [-1, -1]];

// --- Unchanged cells are neither re-checked nor copied, so an edit costs what it changes, not the whole world.
// A canonical cell or room is remembered with what it looked like. While it still looks the same (the same
// fields, lighting, territory, rows array and heights object) the checks and normalization reuse it as it is. Its
// rows and heights, which hold nearly all of a cell's data, are frozen when it is made, so the same array and
// object mean the same ground and comparing them costs nothing however large the cell: replace them to change
// them. Operations never change a cell in place; they replace it with an editable copy (editable()).
const SEEN = new WeakMap();
const FIELDS = ['id', 'name', 'description', 'x', 'y', 'width', 'height', 'z', 'outdoors', 'weather', 'worldX', 'worldY'];
/** What a cell's lighting and territory were, compared field by field (no copies, no JSON). */
const parts = c => {
    const l = object(c.lighting) ? c.lighting : {}, t = object(c.territory) ? c.territory : {};
    return [c.lighting, l.artificial, l.daylightAccess, l.tone, Object.keys(l).length,
        c.territory, t.region, t.chapter, t.claims, Array.isArray(t.claims) ? t.claims.join('\u0000') : null, Object.keys(t).length,
        c.terrain, c.heights];
};
function remember(c) {
    Object.freeze(c.terrain);
    Object.freeze(c.heights);
    SEEN.set(c, {fields: FIELDS.map(k => c[k]), keys: Object.keys(c).length, parts: parts(c)});
    return c;
}
function pristine(c) {
    const seen = object(c) && SEEN.get(c);
    if (!seen || Object.keys(c).length !== seen.keys) return false;
    for (let i = 0; i < FIELDS.length; i++) if (c[FIELDS[i]] !== seen.fields[i]) return false;
    const now = parts(c);
    for (let i = 0; i < now.length; i++) if (now[i] !== seen.parts[i]) return false;
    return true;
}
/** Replaces the cell or room `id` in `next` with a copy that may be changed, and returns it. */
function editable(next, id) {
    for (const list of [next.cells, next.rooms]) {
        const i = list.findIndex(c => c.id === id);
        if (i >= 0) {
            const c = list[i];
            return list[i] = {...c, terrain: [...c.terrain], heights: {...c.heights}, lighting: {...c.lighting}, territory: clone(c.territory)};
        }
    }
    return null;
}

export function clone(value) {
    try { return JSON.parse(JSON.stringify(value)); }
    catch { fail('Project must contain only JSON-serializable data.'); }
}

function checkShape(value) {
    const errors = [];
    const check = (condition, message) => { if (!condition) errors.push(message); };
    if (!object(value)) return ['Project must be an object.'];
    check(value.format === 'ratw-atlas', 'Unsupported project format; expected ratw-atlas.');
    check(value.version === 1 || value.version === 2 || value.version === 3, 'Unsupported atlas version; expected 1 to 3.');
    const legacy = value.version !== 3;                 // v1/v2: one shared canvas; v3: every cell holds its own ground.
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
            check(typeof rows[i] === 'string' && rows[i].length === width && [...rows[i]].every(g => TERRAIN_BY_CODE.has(g)),
                `${label} row ${i} must contain exactly ${width} catalog terrain codes.`);
    };
    const heights = (map, width, height, label) => {
        if (!object(map)) { errors.push(`${label} must be a sparse coordinate object.`); return; }
        for (const [key, v] of Object.entries(map)) {
            const parts = /^(0|[1-9][0-9]*),(0|[1-9][0-9]*)$/.exec(key);
            check(parts && integer(Number(parts[1]), 0, width - 1) && integer(Number(parts[2]), 0, height - 1),
                `${label} coordinate ${key} is outside the terrain or malformed.`);
            check(halfStep(v), `${label} ${key} must be a half-tile height from -16 to 16.`);
        }
    };
    text(value.name, 'Atlas name', 120);
    if (own(value, 'id')) id(value.id, 'World ID');
    if (legacy) {
        check(integer(value.width, 4, LEGACY_CANVAS) && integer(value.height, 4, LEGACY_CANVAS), `World width and height must be 4 to ${LEGACY_CANVAS} tiles.`);
        if (own(value, 'origin')) check(object(value.origin) && Object.keys(value.origin).length === 2
            && integer(value.origin.x, -1e6, 1e6) && integer(value.origin.y, -1e6, 1e6), 'World origin must be {x, y} whole tiles within ±1000000.');
        terrain(value.terrain, value.width, value.height, 'Atlas terrain');
        heights(value.heights, value.width, value.height, 'Atlas heights');
    } else check(!['terrain', 'heights', 'width', 'height', 'origin'].some(k => own(value, k)),
        'An atlas v3 world has no shared canvas: every cell holds its own terrain and heights.');
    for (const list of ['cells', 'rooms', 'links']) check(Array.isArray(value[list]), `${list} must be an array.`);
    if (!Array.isArray(value.cells) || !Array.isArray(value.rooms) || !Array.isArray(value.links)) return errors;
    check(value.cells.length + value.rooms.length <= 65536, 'The world supports at most 65536 cells and interiors.');
    check(value.links.length <= 2048, 'An atlas supports at most 2048 explicit links.');
    for (const [group, items] of [['Cell', value.cells], ['Room', value.rooms]]) {
        for (const [i, c] of items.entries()) {
            const label = `${group} ${i}`;
            if (!legacy && pristine(c)) continue;
            if (!object(c)) { errors.push(`${label} must be an object.`); continue; }
            id(c.id, `${label} ID`); text(c.name, `${label} name`, 120); text(c.description, `${label} description`);
            dimension(c.width, `${label} width`); dimension(c.height, `${label} height`);
            finite(c.z, `${label} z`);
            check(typeof c.outdoors === 'boolean', `${label} outdoors must be boolean.`);
            check(WEATHER.has(c.weather), `${label} weather must be one of ${WEATHERS.join(', ')}.`);
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
            if (own(c, 'letting') && c.letting !== null) {
                // A place to let (Docs/Design/32, 5.2): kind, landlord (a person or "treasury"), weekly rent, level.
                const l = c.letting;
                check(object(l) && Object.keys(l).length === 4 && ['hall', 'warehouse'].includes(l.kind) && typeof l.landlord === 'string' &&
                    integer(l.rent, 1, 100000) && integer(l.level, 2, 5),
                    `${label} a place to let needs kind (hall or warehouse), landlord, rent (1-100000) and level (2-5).`);
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
            if (group === 'Cell' && legacy) {
                check(!own(c, 'terrain') && !own(c, 'heights'), `${label} terrain and heights belong to the shared atlas, not the individual world cell.`);
                check(integer(c.x, 0, value.width - 1) && integer(c.y, 0, value.height - 1), `${label} origin must be inside the atlas.`);
                check(c.x + c.width <= value.width && c.y + c.height <= value.height, `${label} extends outside the atlas.`);
            } else if (group === 'Cell') {
                terrain(c.terrain, c.width, c.height, `${label} terrain`);
                heights(c.heights, c.width, c.height, `${label} heights`);
                check(integer(c.x, -WORLD_REACH, WORLD_REACH) && integer(c.y, -WORLD_REACH, WORLD_REACH),
                    `${label} must sit on whole world tiles within ±${WORLD_REACH}.`);
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
    if (own(value, 'herbPatch') && value.herbPatch !== null) anchor(value.herbPatch, 'Herb patch');
    if (own(value, 'economy')) {
        const e = value.economy;
        check(object(e) && Object.keys(e).length === 5 && Object.entries(ECONOMY_LIMITS).every(([k, max]) => integer(e[k], 0, max)),
            'Economy requires whole-number treasury, storeHerbs, storeMeals, dailyHerbs and dailyMeals within their limits.');
    }
    if (own(value, 'routes')) {
        check(Array.isArray(value.routes) && value.routes.length <= 128, 'routes must be an array of at most 128 patrol routes.');
        for (const [i, r] of (Array.isArray(value.routes) ? value.routes : []).entries()) {
            if (!object(r)) { errors.push(`Route ${i} must be an object.`); continue; }
            id(r.id, `Route ${i} ID`); text(r.name, `Route ${i} name`, 120);
            check(Array.isArray(r.posts) && r.posts.length >= 1 && r.posts.length <= 64, `Route ${i} needs 1 to 64 posts.`);
            for (const [n, post] of (Array.isArray(r.posts) ? r.posts : []).entries()) anchor(post, `Route ${i} post ${n + 1}`);
        }
    }
    if (own(value, 'people')) {
        check(Array.isArray(value.people) && value.people.length <= MAX_PEOPLE, `people must be an array of at most ${MAX_PEOPLE} residents.`);
        for (const [i, p] of (Array.isArray(value.people) ? value.people : []).entries()) {
            const label = `Person ${i}`;
            if (!object(p)) { errors.push(`${label} must be an object.`); continue; }
            id(p.id, `${label} ID`); text(p.name, `${label} name`, 120); text(p.description, `${label} description`);
            check(typeof p.greeting === 'string' && [...p.greeting].length <= 1024 && !/[\u0000-\u001f\u007f]/.test(p.greeting),
                `${label} greeting must be single-line text of at most 1024 characters.`);
            text(p.workLabel, `${label} activity label`, 40);
            if (p.personality !== undefined) check(typeof p.personality === 'string' && [...p.personality].length <= 2000, `${label} personality must be at most 2000 characters.`);
            if (p.backstory !== undefined) check(typeof p.backstory === 'string' && [...p.backstory].length <= 6000, `${label} backstory must be at most 6000 characters.`);
            check(ROLES.includes(p.role), `${label} role must be merchant, guard or civilian.`);
            check(integer(p.age, 0, 200), `${label} age must be a whole number from 0 to 200.`);
            check(integer(p.voice, 0, 31), `${label} voice color must be 0-31.`);
            if (p.joinable !== undefined) check(typeof p.joinable === 'boolean', `${label} joinable must be true or false.`);
            check(typeof p.paid === 'boolean', `${label} paid must be true or false.`);
            check(typeof p.route === 'string' && (p.route === '' || ID.test(p.route)), `${label} route must be a route ID or empty.`);
            for (const [k, max] of [['purse', 100000], ['herbs', 10000], ['meals', 10000]])
                check(integer(p[k], 0, max), `${label} ${k} must be a whole number from 0 to ${max}.`);
            const a = p.appearance;
            check(object(a) && SPECIES.includes(a.species) && SEXES.includes(a.sex) && STATURES.includes(a.stature)
                && PATTERNS.includes(a.pattern) && ['baseColor', 'gradientColor', 'markingColor'].every(k => integer(a[k], 0, 7)),
                `${label} appearance is invalid.`);
            const h = p.hours, hour = v => typeof v === 'number' && Number.isFinite(v) && v >= 0 && v <= 23.75;
            check(object(h) && hour(h.start) && hour(h.end) && h.start !== h.end, `${label} hours need different start and end times from 0 to 23.75.`);
            for (const k of ['home', 'work', 'evening']) anchor(p[k], `${label} ${k}`);
        }
    }
    if (own(value, 'slots')) {
        check(Array.isArray(value.slots) && value.slots.length <= 256, 'slots must be an array of at most 256 profession slots.');
        for (const [i, s] of (Array.isArray(value.slots) ? value.slots : []).entries()) {
            const label = `Profession slot ${i}`;
            if (!object(s)) { errors.push(`${label} must be an object.`); continue; }
            id(s.id, `${label} ID`); text(s.name, `${label} name`, 120); id(s.profession, `${label} profession`);
            check(typeof s.workLabel === 'string' && [...s.workLabel].length <= 40, `${label} activity label is at most 40 characters.`);
            check(typeof s.paid === 'boolean', `${label} paid must be true or false.`);
            check(typeof s.route === 'string' && (s.route === '' || ID.test(s.route)), `${label} route must be a route ID or empty.`);
            for (const [k, max] of [['purse', 100000], ['herbs', 10000], ['meals', 10000]])
                check(integer(s[k], 0, max), `${label} ${k} must be a whole number from 0 to ${max}.`);
            const h = s.hours, hour = v => typeof v === 'number' && Number.isFinite(v) && v >= 0 && v <= 23.75;
            check(object(h) && hour(h.start) && hour(h.end) && h.start !== h.end, `${label} hours need different start and end times.`);
            for (const k of ['home', 'work', 'evening']) anchor(s[k], `${label} ${k}`);
        }
    }
    return errors;
}

function canonical(value) {
    const anchor = a => ({cell: a.cell, x: a.x, y: a.y});
    const metadata = c => ({id: c.id, name: c.name, description: c.description,
        width: c.width, height: c.height, z: c.z, outdoors: c.outdoors, weather: c.weather,
        lighting: {...(c.lighting ?? defaultLighting())},
        territory: {region: c.territory?.region ?? 'unassigned', claims: [...(c.territory?.claims ?? [])].sort(), chapter: c.territory?.chapter ?? ''},
        ...(c.letting ? {letting: {kind: c.letting.kind, landlord: c.letting.landlord, rent: c.letting.rent, level: c.letting.level}} : {})});
    const person = p => ({id: p.id, name: p.name, role: p.role, description: p.description, greeting: p.greeting,
        workLabel: p.workLabel, age: p.age, voice: p.voice, route: p.route, paid: p.paid, purse: p.purse, herbs: p.herbs,
        meals: p.meals, hours: {start: p.hours.start, end: p.hours.end},
        appearance: {species: p.appearance.species, sex: p.appearance.sex, stature: p.appearance.stature,
            pattern: p.appearance.pattern, baseColor: p.appearance.baseColor, gradientColor: p.appearance.gradientColor,
            markingColor: p.appearance.markingColor},
        home: anchor(p.home), work: anchor(p.work), evening: anchor(p.evening),
        personality: p.personality ?? '', backstory: p.backstory ?? '', ...(p.joinable ? {joinable: true} : {})});
    const slot = s => ({id: s.id, name: s.name, profession: s.profession, workLabel: s.workLabel, route: s.route, paid: s.paid,
        purse: s.purse, herbs: s.herbs, meals: s.meals, hours: {start: s.hours.start, end: s.hours.end},
        home: anchor(s.home), work: anchor(s.work), evening: anchor(s.evening)});
    const economy = value.economy ?? defaultEconomy();
    const ox = value.origin?.x ?? 0, oy = value.origin?.y ?? 0;
    const cellGround = value.version === 3
        ? c => ({x: c.x, y: c.y, terrain: [...c.terrain], heights: {...c.heights}})
        : c => ({x: c.x + ox, y: c.y + oy,                 // v1/v2: canvas tiles plus the canvas origin.
            terrain: value.terrain.slice(c.y, c.y + c.height).map(row => row.slice(c.x, c.x + c.width)),
            heights: Object.fromEntries(Object.entries(value.heights).flatMap(([k, h]) => {
                const [hx, hy] = k.split(',').map(Number);
                return hx >= c.x && hx < c.x + c.width && hy >= c.y && hy < c.y + c.height ? [[`${hx - c.x},${hy - c.y}`, h]] : [];
            }))});
    return {format: 'ratw-atlas', version: 3, id: value.id ?? worldIdFor(value.name), name: value.name,
        factions: (value.factions ?? []).map(f => ({id: f.id, name: f.name, color: f.color})),
        chapters: (value.chapters ?? []).map(c => ({id: c.id, name: c.name})),
        cells: value.cells.map(c => value.version === 3 && pristine(c) ? c : remember({...metadata(c), ...cellGround(c)})),
        rooms: value.rooms.map(c => pristine(c) ? c : remember({...metadata(c), terrain: [...c.terrain], heights: {...c.heights}, worldX: c.worldX, worldY: c.worldY})),
        links: value.links.map(l => ({id: l.id, name: l.name, kind: l.kind, a: anchor(l.a), b: anchor(l.b), open: l.open})),
        spawn: value.spawn === null ? null : anchor(value.spawn),
        people: (value.people ?? []).map(person),
        slots: (value.slots ?? []).map(slot),
        routes: (value.routes ?? []).map(r => ({id: r.id, name: r.name, posts: r.posts.map(anchor)})),
        economy: Object.fromEntries(Object.keys(ECONOMY_LIMITS).map(k => [k, economy[k]])),
        herbPatch: value.herbPatch ? anchor(value.herbPatch) : null};
}

/** The world cell with this ground in place of what it holds (ground fetched later for a world loaded lean), as a
 *  canonical cell: the checks and normalization take it as it is. */
export function withGround(c, terrain, heights) {
    return remember({...c, terrain: [...terrain], heights: {...heights}});
}

export function getCell(project, id) {
    return project.cells.find(c => c.id === id) ?? project.rooms.find(c => c.id === id) ?? null;
}

export function cellTerrain(project, id) {
    const c = getCell(project, id);
    if (!c) fail(`Unknown cell: ${id}.`);
    return [...c.terrain];
}

/** Interiors are the cells with an overview position; world cells sit in the world at x, y. */
export const isInterior = c => !!c && own(c, 'worldX');

function glyphAt(project, c, x, y) {
    if (!integer(x, 0, c.width - 1) || !integer(y, 0, c.height - 1)) return null;
    return c.terrain[y][x];
}
function heightAt(project, c, x, y, kind = null) {
    const coordinate = `${x},${y}`;
    if (own(c.heights, coordinate)) return c.heights[coordinate];
    if (kind === 'stairs') return 0.5; // Runtime export stamps stairs over the authored glyph.
    return defaultHeight(glyphAt(project, c, x, y));
}
const keyFor = a => `${a.cell}:${a.x},${a.y}`;

/** Finds world cells by world tile without scanning them all. */
export function cellIndex(cells) {
    const buckets = new Map();
    for (const c of cells)
        for (let by = Math.floor(c.y / BUCKET); by <= Math.floor((c.y + c.height - 1) / BUCKET); by++)
            for (let bx = Math.floor(c.x / BUCKET); bx <= Math.floor((c.x + c.width - 1) / BUCKET); bx++) {
                const key = `${bx},${by}`;
                if (!buckets.has(key)) buckets.set(key, []);
                buckets.get(key).push(c);
            }
    const near = (x, y) => buckets.get(`${Math.floor(x / BUCKET)},${Math.floor(y / BUCKET)}`) ?? [];
    return {
        at: (x, y) => near(x, y).find(c => x >= c.x && y >= c.y && x < c.x + c.width && y < c.y + c.height) ?? null,
        /** Cells that could overlap the rectangle (every cell is filed under each bucket it covers). */
        around: (x, y, w, h) => {
            const out = new Set();
            for (let by = Math.floor(y / BUCKET); by <= Math.floor((y + h - 1) / BUCKET); by++)
                for (let bx = Math.floor(x / BUCKET); bx <= Math.floor((x + w - 1) / BUCKET); bx++)
                    for (const c of buckets.get(`${bx},${by}`) ?? []) out.add(c);
            return [...out];
        },
        /** Every cell that overlaps another: overlapping cells always share a bucket. */
        overlaps: () => {
            const out = new Set();
            for (const list of buckets.values())
                for (let i = 0; i < list.length; i++) for (let j = i + 1; j < list.length; j++)
                    if (overlapping(list[i], list[j])) { out.add(list[i]); out.add(list[j]); }
            return out;
        },
    };
}
const overlapping = (a, b) => a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y;
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
    const overlaps = cellIndex(project.cells).overlaps();
    for (const c of project.cells) if (overlaps.has(c)) errors.push(`Cell ${c.id} overlaps another world cell.`);
    if (requireExport) {
        // What the game can load today; the editor itself allows far more.
        const tiles = [...project.cells, ...project.rooms].reduce((n, c) => n + c.width * c.height, 0);
        // Game servers stream cells from the database (no limit); ▶ Play runs a local game that loads every cell.
        if (tiles > GAME_TILES) warnings.push(`Cells and interiors cover ${tiles} tiles, more than a file export holds (${GAME_TILES}); ▶ Play runs this world from the DEV database instead, as Push to live does.`);
        if (all.length > GAME_AREAS) warnings.push(`The world has ${all.length} cells and interiors, more than a file export holds (${GAME_AREAS}); ▶ Play runs this world from the DEV database instead, as Push to live does.`);
        if (!project.cells.length) errors.push('Add at least one world cell before playing.');
    }
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
    const place = (a, label) => {
        const c = getCell(project, a.cell);
        if (!c) errors.push(`${label} is in unknown cell ${a.cell}.`);
        else if (!walkable(glyphAt(project, c, a.x, a.y))) errors.push(`${label} must be on open ground inside ${c.name}.`);
    };
    if (project.herbPatch) place(project.herbPatch, 'Herb patch');
    const routeIds = new Set();
    for (const r of project.routes) {
        if (routeIds.has(r.id)) errors.push(`Duplicate route ID: ${r.id}.`);
        routeIds.add(r.id);
        r.posts.forEach((post, i) => place(post, `Route ${r.name} post ${i + 1}`));
    }
    const personIds = new Set();
    for (const p of project.people) {
        if (personIds.has(p.id)) errors.push(`Duplicate person ID: ${p.id}.`);
        if (reservedPersonId(p.id)) errors.push(`Person ID ${p.id} is reserved; choose another.`);
        personIds.add(p.id);
        if (p.route && (p.role === 'merchant' || !routeIds.has(p.route))) errors.push(`${p.name}: only guards (on patrol) and civilians (travelling) walk routes, and the route must exist.`);
        for (const k of ['home', 'work', 'evening']) place(p[k], `${p.name}'s ${k}`);
        if (p.role === 'merchant') {
            const c = getCell(project, p.work.cell);
            if (c && !NEIGHBORS8.some(([dx, dy]) => walkable(glyphAt(project, c, p.work.x + dx, p.work.y + dy))))
                errors.push(`${p.name}: customers need an open tile beside the shop counter place.`);
        }
    }
    const slotIds = new Set();
    for (const s of project.slots) {
        if (slotIds.has(s.id)) errors.push(`Duplicate profession slot ID: ${s.id}.`);
        slotIds.add(s.id);
        if (s.route && !routeIds.has(s.route)) errors.push(`${s.name}: the patrol route must exist.`);
        for (const k of ['home', 'work', 'evening']) place(s[k], `${s.name}'s ${k}`);
    }
    if (project.people.length && !project.people.some(p => p.role === 'merchant'))
        warnings.push('No merchant: residents will have nowhere to buy food.');
    if (project.people.length && !project.herbPatch) warnings.push('No herb patch: players cannot gather herbs in this world.');
    if (project.rooms.some(r => !project.links.some(l => l.a.cell === r.id || l.b.cell === r.id)))
        warnings.push('One or more detached rooms have no links and cannot be reached from the atlas.');
    if (project.cells.length > 1) warnings.push('Automatic seams are generated only at compatible traversable edges on the same Z level.');
    return {errors, warnings};
}

/**
 * Checks and canonicalizes a project. `tolerate` is a set of problems that already existed (or true for
 * any): several people edit the world at once, so one edit is refused only for problems it introduces.
 */
export function normalizeProject(value, tolerate = null) {
    value = roundLegacyHeights(value);
    const errors = checkShape(value);
    if (errors.length) fail(errors.join('\n'));
    const safe = canonical(value), checks = relationships(safe);
    const fresh = tolerate === true ? [] : checks.errors.filter(e => !tolerate?.has(e));
    if (fresh.length) fail(fresh.join('\n'));
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
    const known = checkShape(project).length ? null : new Set(relationships(canonical(project)).errors);
    const next = normalizeProject(project, known);
    const result = operation(next);
    const valid = normalizeProject(next, known);
    for (const key of Object.keys(project)) delete project[key];
    Object.assign(project, valid);
    return result;
}

function freshId(project, prefix, used = new Set()) {
    for (const c of [...project.cells, ...project.rooms, ...project.links]) used.add(c.id);
    for (let i = 1; i < 100000; i++) { const id = `${prefix}_${i}`; if (!used.has(id)) { used.add(id); return id; } }
    fail('Could not allocate a unique authoring ID.');
}

/** The rectangle, in world tiles, that just holds every world cell; null without cells. */
export function cellBounds(project) {
    if (!project.cells.length) return null;
    let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    for (const c of project.cells) {
        x0 = Math.min(x0, c.x); y0 = Math.min(y0, c.y); x1 = Math.max(x1, c.x + c.width); y1 = Math.max(y1, c.y + c.height);
    }
    return {x: x0, y: y0, width: x1 - x0, height: y1 - y0};
}

/**
 * The world is the ground its cells cover. Adds a world cell of plain ground anywhere no cell is yet, in world
 * tiles; there is no edge to grow. Ground no cell covers is empty and cannot be painted.
 */
export function addCell(project, {x, y, width, height}, name = 'New cell') {
    let made;
    atomic(project, next => {
        if (!integer(width, 4, 256) || !integer(height, 4, 256)) fail('A cell must be 4 to 256 tiles on each side.');
        if (!integer(x, -WORLD_REACH, WORLD_REACH - width) || !integer(y, -WORLD_REACH, WORLD_REACH - height))
            fail(`A cell sits on whole world tiles within ±${WORLD_REACH}.`);
        const rect = {x, y, width, height};
        const clash = cellIndex(next.cells).around(x, y, width, height).find(c => overlapping(c, rect));
        if (clash) fail(`It would overlap ${clash.name}.`);
        made = freshId(next, 'cell');
        next.cells.push({id: made, name, description: '', x, y, width, height, terrain: Array(height).fill('.'.repeat(width)),
            heights: {}, z: 0, outdoors: true, weather: 'clear', lighting: defaultLighting(), territory: defaultTerritory()});
    });
    return getCell(project, made);
}

/**
 * Deletes a world cell, its ground and every door leading to it. People, posts, the spawn or the herb patch inside
 * must be moved first; the last cell stays.
 */
export function removeCell(project, id) {
    atomic(project, next => {
        if (!next.cells.some(c => c.id === id)) fail(`Unknown world cell: ${id}.`);
        if (next.cells.length === 1) fail('The world needs at least one cell.');
        next.links = next.links.filter(l => l.a.cell !== id && l.b.cell !== id);
        if (anchorsIn(next, id).length) fail('People, patrol posts, the spawn or the herb patch are still inside. Move them first.');
        next.cells = next.cells.filter(c => c.id !== id);
    });
    return project;
}

/** A new world of plain ground, width × height tiles from world 0,0, cut into cells of at most 256 a side. */
export function createProject(width = 64, height = 48, name = 'Untitled atlas') {
    if (!integer(width, 4, LEGACY_CANVAS) || !integer(height, 4, LEGACY_CANVAS)) fail(`A new atlas is 4 to ${LEGACY_CANVAS} tiles a side; add cells to grow it.`);
    const cells = [];
    const spans = n => { const out = []; for (let at = 0; at < n; at += 256) out.push([at, Math.min(256, n - at)]); return out; };
    for (const [y, h] of spans(height)) for (const [x, w] of spans(width))
        cells.push({id: `cell_${cells.length + 1}`, name: `World cell ${cells.length + 1}`, description: '', x, y, width: w, height: h,
            terrain: Array(h).fill('.'.repeat(w)), heights: {}, z: 0, outdoors: true, weather: 'clear',
            lighting: defaultLighting(), territory: defaultTerritory()});
    return normalizeProject({format: 'ratw-atlas', version: 3, name, cells, rooms: [], links: [], spawn: null});
}

/**
 * Changes ground as one operation. `roomId` null means world tiles: each goes to the world cell that holds it, and
 * tiles no cell holds are skipped. `change(x, y)` (in the target's own tiles) returns {glyph} and/or {height} (null
 * clears an elevation override); only the cells touched are replaced.
 */
function editGround(next, roomId, tiles, change) {
    const staged = new Map();                      // Cell or room ID -> {rows: Map<y, glyph[]>, heights: [k, h][]}.
    const stage = (target, x, y, from) => {
        const what = change(from[0], from[1], target, x, y);
        if (!what) return;
        if (!staged.has(target.id)) staged.set(target.id, {target, rows: new Map(), heights: []});
        const s = staged.get(target.id);
        if (what.glyph !== undefined) {
            if (!s.rows.has(y)) s.rows.set(y, [...target.terrain[y]]);
            s.rows.get(y)[x] = what.glyph;
        }
        if (what.height !== undefined) s.heights.push([`${x},${y}`, what.height]);
    };
    if (roomId !== null) {
        const room = next.rooms.find(r => r.id === roomId);
        if (!room) fail(`Unknown detached room: ${roomId}. World cells are painted in world coordinates.`);
        for (const [x, y] of tiles) if (integer(x, 0, room.width - 1) && integer(y, 0, room.height - 1)) stage(room, x, y, [x, y]);
    } else {
        const index = cellIndex(next.cells);
        for (const [wx, wy] of tiles) {
            if (!Number.isInteger(wx) || !Number.isInteger(wy)) continue;
            const c = index.at(wx, wy);
            if (c) stage(c, wx - c.x, wy - c.y, [wx, wy]);
        }
    }
    for (const {target, rows, heights} of staged.values()) {
        const c = editable(next, target.id);
        for (const [y, row] of rows) c.terrain[y] = row.join('');
        for (const [k, h] of heights) { if (h === null) delete c.heights[k]; else c.heights[k] = h; }
    }
}
const checkGlyph = glyph => { if (!isTerrainCode(glyph)) fail('Choose a terrain tile from the catalog.'); };
const checkElevation = value => {
    if (value !== null && !halfStep(value)) fail('Elevation must be null or a half-tile height from -16 to 16.');
};
const square = (x, y, size) => Array.from({length: size * size}, (_, i) => [x + (i % size), y + Math.floor(i / size)]);

/** Paints a size × size brush with its top-left at x, y (world tiles, or the room's tiles). */
export function paint(project, x, y, glyph, size = 1, roomId = null) {
    atomic(project, next => {
        if (!Number.isInteger(x) || !Number.isInteger(y)) fail('Brush origin must be whole tiles.');
        checkGlyph(glyph);
        if (!integer(size, 1, 8)) fail('Brush size must be an integer from 1 to 8.');
        if (roomId !== null) {
            const room = next.rooms.find(r => r.id === roomId);
            if (room && !(integer(x, 0, room.width - 1) && integer(y, 0, room.height - 1))) fail('Brush origin must be an integer tile inside the terrain.');
        } else if (!cellIndex(next.cells).at(x, y)) fail('Brush origin must be a tile inside a world cell.');
        editGround(next, roomId, square(x, y, size), () => ({glyph}));
    });
    return project;
}

export function setHeight(project, x, y, value, roomId = null) {
    atomic(project, next => {
        checkElevation(value);
        const inside = roomId !== null ? (() => { const r = next.rooms.find(room => room.id === roomId); return !r || (integer(x, 0, r.width - 1) && integer(y, 0, r.height - 1)); })()
            : !!cellIndex(next.cells).at(x, y);
        if (!inside) fail('Elevation coordinate must be an integer tile inside the terrain.');
        editGround(next, roomId, [[x, y]], () => ({height: value}));
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

/** Gives cells that are new or changed shape their ground (and heights) from the world as the old cells hold it. */
function regroundCells(oldCells, nextCells) {
    const index = cellIndex(oldCells), byId = new Map(oldCells.map(c => [c.id, c]));
    return nextCells.map(c => {
        const was = byId.get(c.id);
        if (was && was.x === c.x && was.y === c.y && was.width === c.width && was.height === c.height && c.terrain === was.terrain) return c;
        const terrain = [], heights = {};
        for (let y = 0; y < c.height; y++) {
            let row = '';
            for (let x = 0; x < c.width; x++) {
                const from = index.at(c.x + x, c.y + y);
                row += from ? from.terrain[c.y + y - from.y][c.x + x - from.x] : '.';
                const h = from?.heights[`${c.x + x - from.x},${c.y + y - from.y}`];
                if (h !== undefined) heights[`${x},${y}`] = h;
            }
            terrain.push(row);
        }
        return {...c, terrain, heights};
    });
}

function applyPartition(project, nextCells) {
    const oldCells = project.cells;
    nextCells = regroundCells(oldCells, nextCells);
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
    project.spawn = remap(project.spawn); project.links = links;
    project.people = project.people.map(p => ({...p, home: remap(p.home), work: remap(p.work), evening: remap(p.evening)}));
    project.slots = project.slots.map(p => ({...p, home: remap(p.home), work: remap(p.work), evening: remap(p.evening)}));
    project.routes = project.routes.map(r => ({...r, posts: r.posts.map(remap)}));
    project.herbPatch = remap(project.herbPatch);
    project.cells = nextCells;
    if (!project.spawn) {
        const taken = new Set(project.links.flatMap(l => [keyFor(l.a), keyFor(l.b)]));
        outer: for (const c of nextCells) for (let y = 0; y < c.height; y++) for (let x = 0; x < c.width; x++) {
            const a = {cell: c.id, x, y};
            if (walkable(glyphAt(project, c, x, y)) && !taken.has(keyFor(a))) { project.spawn = a; break outer; }
        }
    }
}

/**
 * Re-cuts the rectangle that holds every world cell into a grid of width × height cells (the last row and column
 * take what is left). Ground no cell covered becomes plain ground in the new cells.
 */
export function cutGrid(project, width = 32, height = 24) {
    atomic(project, next => {
        if (!integer(width, 4, 256) || !integer(height, 4, 256)) fail('Cut sizes must be integers from 4 to 256.');
        const box = cellBounds(next);
        if (!box) fail('There are no world cells to cut.');
        for (const [total, size, axis] of [[box.width, width, 'width'], [box.height, height, 'height']])
            if (total % size > 0 && total % size < 4) fail(`The cut leaves a ${total % size}-tile ${axis} sliver. Every cell must be at least 4×4; choose a different cut size.`);
        if (Math.ceil(box.width / width) * Math.ceil(box.height / height) + next.rooms.length > 65536)
            fail('This cut exceeds the 65536 cell and interior limit.');
        const cells = [], reserved = new Set([...next.rooms, ...next.cells].map(c => c.id));
        for (let y = box.y; y < box.y + box.height; y += height) for (let x = box.x; x < box.x + box.width; x += width) {
            const w = Math.min(width, box.x + box.width - x), h = Math.min(height, box.y + box.height - y);
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
                description: source?.description ?? '', x, y, width: w, height: h, terrain: null, heights: {}, z: source?.z ?? 0,
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

/** A place to let (Docs/Design/32, 5.2), or none (null). */
export function setLetting(project, id, letting) {
    atomic(project, next => {
        const cell = editable(next, id);
        if (!cell) fail(`Unknown place: ${id}.`);
        if (letting) cell.letting = clone(letting);
        else delete cell.letting;
    });
    return project;
}

export function setTerritory(project, ids, territory) {
    atomic(project, next => {
        if (!Array.isArray(ids) || !ids.length || new Set(ids).size !== ids.length) fail('Choose one or more distinct cells or rooms for territory settings.');
        for (const id of ids) {
            const cell = editable(next, id);
            if (!cell) fail(`Unknown territory cell: ${id}.`);
            cell.territory = clone(territory);
        }
    });
    return project;
}

// ---------------------------------------------------------------------------
// Shape painting, stamping and room editing.

/** Paints any set of tiles ([[x, y], ...]) as one operation; tiles outside every cell (or the room) are ignored. */
export function paintTiles(project, tiles, glyph, roomId = null) {
    atomic(project, next => {
        checkGlyph(glyph);
        if (!Array.isArray(tiles) || tiles.length > 1048576) fail('Too many tiles in one paint operation.');
        editGround(next, roomId, tiles, () => ({glyph}));
    });
    return project;
}

/** Copies rows of glyphs onto the terrain at x, y. Spaces are transparent; tiles outside every cell are skipped. */
export function stamp(project, x, y, rows, roomId = null) {
    atomic(project, next => {
        if (!Array.isArray(rows) || rows.some(r => typeof r !== 'string' || [...r].some(g => g !== ' ' && !TERRAIN_BY_CODE.has(g))))
            fail('A stamp must be rows of supported glyphs, with spaces for transparent tiles.');
        if (!Number.isInteger(x) || !Number.isInteger(y)) fail('Stamp origin must be whole tiles.');
        const glyphs = new Map();
        rows.forEach((row, dy) => [...row].forEach((g, dx) => { if (g !== ' ') glyphs.set(`${x + dx},${y + dy}`, g); }));
        editGround(next, roomId, [...glyphs.keys()].map(k => k.split(',').map(Number)), (tx, ty) => ({glyph: glyphs.get(`${tx},${ty}`)}));
    });
    return project;
}

const CELL_FIELDS = ['name', 'description', 'z', 'outdoors', 'weather', 'lighting'];
/** Updates metadata of a world cell or room. Rooms also accept worldX/worldY overview positions. */
export function updateCell(project, id, fields) {
    atomic(project, next => {
        const c = editable(next, id);
        if (!c) fail(`Unknown cell: ${id}.`);
        if (!object(fields)) fail('Cell fields must be an object.');
        const allowed = isInterior(c) ? [...CELL_FIELDS, 'worldX', 'worldY'] : CELL_FIELDS;
        for (const [k, v] of Object.entries(fields)) {
            if (!allowed.includes(k)) fail(`Cannot change ${k} here.`);
            c[k] = clone(v);
        }
    });
    return getCell(project, id);
}

const anchorsIn = (project, cellId) => [
    ...project.links.flatMap(l => [l.a, l.b]), ...(project.spawn ? [project.spawn] : []),
    ...[...project.people, ...project.slots].flatMap(p => [p.home, p.work, p.evening]), ...project.routes.flatMap(r => r.posts),
    ...(project.herbPatch ? [project.herbPatch] : [])].filter(a => a.cell === cellId);

/** Resizes a detached room, keeping its top-left terrain. New area is walled floor. */
export function resizeRoom(project, id, width, height) {
    atomic(project, next => {
        const r = next.rooms.some(room => room.id === id) ? editable(next, id) : null;
        if (!r) fail('Only detached interiors can be resized.');
        if (!integer(width, 4, 256) || !integer(height, 4, 256)) fail('Room dimensions must be integers from 4 to 256.');
        if (anchorsIn(next, id).some(a => a.x >= width || a.y >= height))
            fail('Something is placed in the area being removed. Move doors, people or markers first.');
        r.terrain = Array.from({length: height}, (_, y) => Array.from({length: width}, (_, x) =>
            y < r.height && x < r.width ? r.terrain[y][x] : (x === 0 || y === 0 || x === width - 1 || y === height - 1 ? '#' : '.')).join(''));
        r.heights = Object.fromEntries(Object.entries(r.heights).filter(([k]) => {
            const [x, y] = k.split(',').map(Number); return x < width && y < height;
        }));
        r.width = width; r.height = height;
    });
    return getCell(project, id);
}

/** Deletes a detached room and every door leading to it. People or markers inside must be moved first. */
export function removeRoom(project, id) {
    atomic(project, next => {
        if (!next.rooms.some(r => r.id === id)) fail(`Unknown interior: ${id}.`);
        next.links = next.links.filter(l => l.a.cell !== id && l.b.cell !== id);
        if (anchorsIn(next, id).length) fail('People, patrol posts, the spawn or the herb patch are still inside. Move them first.');
        next.rooms = next.rooms.filter(r => r.id !== id);
    });
    return project;
}

export function updateLink(project, id, fields) {
    atomic(project, next => {
        const link = next.links.find(l => l.id === id);
        if (!link) fail(`Unknown link: ${id}.`);
        for (const [k, v] of Object.entries(fields)) {
            if (!['name', 'kind', 'open', 'a', 'b'].includes(k)) fail(`Cannot change ${k} on a link.`);
            link[k] = clone(v);
        }
        if (link.kind !== 'door') link.open = true;
    });
    return project.links.find(l => l.id === id);
}

export function setSpawn(project, place) {
    atomic(project, next => { next.spawn = place ? clone(place) : null; });
    return project;
}

// ---------------------------------------------------------------------------
// People, routes, economy.

const slug = name => (String(name).toLowerCase().normalize('NFKD').replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '') || 'person').replace(/^[^a-z]+/, 'p_').slice(0, 40);
function freshIn(list, base) {
    const used = new Set(list.map(x => x.id));
    if (!used.has(base) && !reservedPersonId(base)) return base;
    for (let i = 2; i < 100000; i++) if (!used.has(`${base}_${i}`)) return `${base}_${i}`;
    fail('Could not allocate a unique ID.');
}

const ROLE_DEFAULTS = {
    merchant: {workLabel: 'keeping shop', hours: {start: 7, end: 19}, paid: false, purse: 120, herbs: 10, meals: 16},
    guard: {workLabel: 'on watch', hours: {start: 6, end: 18}, paid: true, purse: 30, herbs: 0, meals: 1},
    civilian: {workLabel: 'working', hours: {start: 8, end: 17}, paid: true, purse: 30, herbs: 0, meals: 1},
};
/** A new, unsaved person with sensible defaults for a role, placed at one spot. */
export function newPerson(project, role = 'civilian', place = null, name = null) {
    if (!ROLES.includes(role)) fail('Unknown role.');
    const spot = place ?? project.spawn ?? {cell: project.cells[0]?.id ?? '', x: 0, y: 0};
    const title = name ?? {merchant: 'New shopkeeper', guard: 'New guard', civilian: 'New resident'}[role];
    return {id: freshIn(project.people, slug(title)), name: title, role, description: '', greeting: '',
        ...clone(ROLE_DEFAULTS[role]), age: 30, voice: project.people.length % 32, route: '', personality: '', backstory: '',
        appearance: {species: 'timber', sex: 'female', stature: 'average', pattern: 'saddle', baseColor: 3, gradientColor: 1, markingColor: 5},
        home: {...spot}, work: {...spot}, evening: {...spot}};
}

export function upsertPerson(project, person) {
    atomic(project, next => {
        if (!object(person)) fail('Person must be an object.');
        const at = next.people.findIndex(p => p.id === person.id);
        const value = {...clone(person), id: person.id ?? freshIn(next.people, slug(person.name))};
        for (const k of PERSON_KEYS) if (!own(value, k)) fail(`Person is missing ${k}.`);
        if (at < 0) next.people.push(value); else next.people[at] = value;
    });
    return project.people.find(p => p.id === (person.id ?? project.people.at(-1).id));
}

export function removePerson(project, id) {
    atomic(project, next => {
        if (!next.people.some(p => p.id === id)) fail(`Unknown person: ${id}.`);
        next.people = next.people.filter(p => p.id !== id);
    });
    return project;
}

export function upsertRoute(project, route) {
    let id;
    atomic(project, next => {
        if (!object(route)) fail('Route must be an object.');
        id = route.id ?? freshIn(next.routes, slug(route.name ?? 'patrol'));
        const value = {id, name: route.name ?? 'Patrol route', posts: clone(route.posts ?? [])};
        const at = next.routes.findIndex(r => r.id === id);
        if (at < 0) next.routes.push(value); else next.routes[at] = value;
    });
    return project.routes.find(r => r.id === id);
}

/** Deletes a route; guards who walked it hold their work post instead. */
export function removeRoute(project, id) {
    atomic(project, next => {
        if (!next.routes.some(r => r.id === id)) fail(`Unknown route: ${id}.`);
        next.routes = next.routes.filter(r => r.id !== id);
        for (const p of [...next.people, ...next.slots]) if (p.route === id) p.route = '';
    });
    return project;
}

export function setEconomy(project, economy) {
    atomic(project, next => { next.economy = {...next.economy, ...clone(economy)}; });
    return project.economy;
}

export function setHerbPatch(project, place) {
    atomic(project, next => { next.herbPatch = place ? clone(place) : null; });
    return project;
}

/** Moves one placed thing: spawn, herb patch, a person's home/work/evening, or a route post. */
export function movePlace(project, ref, place) {
    atomic(project, next => {
        const spot = clone(place);
        if (ref.kind === 'spawn') next.spawn = spot;
        else if (ref.kind === 'herbPatch') next.herbPatch = spot;
        else if (ref.kind === 'person') {
            const p = next.people.find(x => x.id === ref.id);
            if (!p || !['home', 'work', 'evening'].includes(ref.slot)) fail('Unknown person place.');
            p[ref.slot] = spot;
        } else if (ref.kind === 'slot') {
            const s = next.slots.find(x => x.id === ref.id);
            if (!s || !['home', 'work', 'evening'].includes(ref.slot)) fail('Unknown profession slot place.');
            s[ref.slot] = spot;
        } else if (ref.kind === 'post') {
            const r = next.routes.find(x => x.id === ref.id);
            if (!r || !integer(ref.index, 0, r.posts.length - 1)) fail('Unknown patrol post.');
            r.posts[ref.index] = spot;
        } else fail('Unknown placed thing.');
    });
    return project;
}

// ---------------------------------------------------------------------------
// Building templates: a walled footprint on the world canvas with a door, a
// matching detached interior, and the door link between them.

/** Stamps template on a world cell with its door facing `side` ('S' or 'N'); returns the new pieces. */
export function addBuilding(project, template, cellId, x, y, side = 'S', name = null) {
    let result;
    atomic(project, next => {
        const c = next.cells.some(cell => cell.id === cellId) ? editable(next, cellId) : null;
        if (!c) fail('Buildings are placed on world cells, not inside interiors.');
        if (side !== 'S' && side !== 'N') fail('Door side must be S (south) or N (north).');
        const {footprint: [fw, fh], interior, name: kind} = template;
        if (!integer(x, 0, c.width - fw) || !integer(y, 0, c.height - fh)) fail(`The ${fw}×${fh} footprint must fit inside ${c.name}.`);
        const flip = side === 'N';
        const rows = flip ? [...interior].reverse() : [...interior];
        const doorOut = {cell: cellId, x: x + Math.floor(fw / 2), y: flip ? y : y + fh - 1};
        const front = {x: doorOut.x, y: flip ? doorOut.y - 1 : doorOut.y + 1};
        if (!walkable(glyphAt(next, c, front.x, front.y))) fail('The tile in front of the door must be open ground.');
        c.terrain = c.terrain.map((row, ry) => ry < y || ry >= y + fh ? row
            : row.slice(0, x) + (ry === doorOut.y ? '#'.repeat(doorOut.x - x) + '+' + '#'.repeat(fw - 1 - (doorOut.x - x)) : '#'.repeat(fw)) + row.slice(x + fw));
        const doorRow = flip ? 0 : rows.length - 1;
        const doorX = rows[doorRow].indexOf('+');
        if (doorX < 0) fail('Template interior needs a + door on its street edge.');
        const id = freshId(next, slug(template.id ?? kind));
        const w = rows[0].length, h = rows.length;
        next.rooms.push({id, name: name ?? kind, description: template.description ?? '', width: w, height: h,
            terrain: rows, heights: {}, worldX: c.x + x, worldY: c.y + y + (flip ? fh : -h), z: 0, outdoors: false,
            weather: 'clear', lighting: {...(template.lighting ?? defaultLighting())}, territory: clone(c.territory)});
        const linkId = freshId(next, `${id}_door`);
        next.links.push({id: linkId, name: `${name ?? kind} door`, kind: 'door', a: doorOut,
            b: {cell: id, x: doorX, y: doorRow}, open: !!template.open});
        const mark = ([mx, my]) => ({cell: id, x: mx, y: flip ? h - 1 - my : my});
        result = {roomId: id, linkId, beds: (template.beds ?? []).map(mark), counter: template.counter ? mark(template.counter) : null,
            front: {cell: cellId, ...front}};
    });
    return result;
}

/** Sets (or with null, clears) the elevation override on many tiles as one operation. */
export function setHeights(project, tiles, value, roomId = null) {
    atomic(project, next => {
        checkElevation(value);
        editGround(next, roomId, tiles, () => ({height: value}));
    });
    return project;
}

/** Raises (delta > 0) or lowers every tile by `delta` half steps' worth of height, from its current height (override or
 *  glyph default), clamped to -16 to 16. A tile that lands on its glyph default loses its override. */
export function adjustHeights(project, tiles, delta, roomId = null) {
    atomic(project, next => {
        if (!(typeof delta === 'number' && Number.isInteger(delta * 2) && delta !== 0 && Math.abs(delta) <= 32)) fail('Raise or lower by whole half steps.');
        editGround(next, roomId, tiles, (_x, _y, c, x, y) => {
            const k = `${x},${y}`, glyph = c.terrain[y][x], base = defaultHeight(glyph);
            const h = roundHeight((own(c.heights, k) ? c.heights[k] : base) + delta);
            return {height: h === base ? null : h};
        });
    });
    return project;
}

// ---------------------------------------------------------------------------
// Profession slots: a job placed in the world. At export the shared character
// roster fills each slot, permanently, with its best-suited free character.

/** A new, unsaved slot; `profession` is a roster catalog entry ({id, name, workLabel, hours, paid}). */
export function newSlot(project, profession, place) {
    if (!object(profession) || !ID.test(profession.id ?? '')) fail('Choose a profession from the roster catalog.');
    const spot = place ?? project.spawn ?? {cell: project.cells[0]?.id ?? '', x: 0, y: 0};
    const base = slug(profession.id);
    return {id: freshIn(project.slots, base), name: profession.name, profession: profession.id, workLabel: '',
        hours: clone(profession.hours ?? {start: 8, end: 17}), route: '', paid: profession.paid ?? true,
        purse: 30, herbs: 0, meals: 1, home: {...spot}, work: {...spot}, evening: {...spot}};
}

export function upsertSlot(project, slotValue) {
    atomic(project, next => {
        if (!object(slotValue)) fail('Slot must be an object.');
        for (const k of SLOT_KEYS) if (!own(slotValue, k)) fail(`Slot is missing ${k}.`);
        const at = next.slots.findIndex(s => s.id === slotValue.id);
        if (at < 0) next.slots.push(clone(slotValue)); else next.slots[at] = clone(slotValue);
    });
    return project.slots.find(s => s.id === slotValue.id);
}

/** Removing a slot releases its character at the next export; they keep their profession for life. */
export function removeSlot(project, id) {
    atomic(project, next => {
        if (!next.slots.some(s => s.id === id)) fail(`Unknown profession slot: ${id}.`);
        next.slots = next.slots.filter(s => s.id !== id);
    });
    return project;
}
