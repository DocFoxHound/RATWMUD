// Night, lights and the eased sky (lights.ts, ambient.ts): nothing on the ground glows at night but a light source.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {TERRAIN} from './terrain.generated.mjs';
import {GroundPainter} from './terrainLayer.ts';
import {lampsLit, LightSources, NightFloor, nightLevel} from './lights.ts';
import {Ambient} from './ambient.ts';
import {fakeContext, settleAmbient, testGame} from './testing.ts';
import {GamePainter} from './paint.ts';
import {Painter} from '../ui/painter.ts';
import type {GameState} from './state.ts';
import type {Sheets} from './weatherArt.ts';
import type {Json} from './json.ts';

interface Op {
    kind: 'text' | 'rect' | 'line';
    light: number;              // How bright it is drawn, as seen: the sRGB luma of its colour times every alpha on it.
    text?: string;
    pool: boolean;              // A gradient laid on with 'lighter': a light's pool.
}

/** How bright a canvas colour is to the eye (0..1), its alpha included; NaN for a gradient. */
function brightness(style: unknown): number {
    if (typeof style !== 'string') return NaN;
    let r = 0, g = 0, b = 0, a = 1;
    const m = /rgba?\(([\d.]+),\s*([\d.]+),\s*([\d.]+)(?:,\s*([\d.]+))?\)/.exec(style);
    if (m) [r, g, b, a] = [Number(m[1]), Number(m[2]), Number(m[3]), m[4] === undefined ? 1 : Number(m[4])];
    else if (/^#[0-9a-f]{6}$/i.test(style)) {
        const v = parseInt(style.slice(1), 16);
        [r, g, b] = [(v >> 16) & 255, (v >> 8) & 255, v & 255];
    } else return NaN;
    return (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255 * a;
}

/** A 2D context that records what is drawn and how brightly, with save and restore as a canvas keeps them. */
function recorder() {
    const c = fakeContext() as unknown as Record<string, unknown>;
    const ops: Op[] = [];
    const stack: Array<Record<string, unknown>> = [];
    const kept = ['globalAlpha', 'fillStyle', 'strokeStyle', 'globalCompositeOperation'];
    Object.assign(c, {globalAlpha: 1, fillStyle: '#000000', strokeStyle: '#000000', globalCompositeOperation: 'source-over'});
    c.save = () => stack.push(Object.fromEntries(kept.map(k => [k, c[k]])));
    c.restore = () => Object.assign(c, stack.pop() ?? {});
    const record = (kind: Op['kind'], style: unknown, text?: string) => ops.push({kind, text,
        light: brightness(style) * (c.globalAlpha as number), pool: typeof style !== 'string' && c.globalCompositeOperation === 'lighter'});
    c.fillText = (text: string) => record('text', c.fillStyle, text);
    c.fillRect = () => record('rect', c.fillStyle);
    c.stroke = () => record('line', c.strokeStyle);
    c.strokeRect = () => record('line', c.strokeStyle);
    return {ctx: c as unknown as CanvasRenderingContext2D, ops};
}

/** Ground for GroundPainter alone: `glyph` and `height` by tile, all in sight unless `seen` says otherwise. */
function ground(glyph: (x: number, y: number) => string, height: (x: number, y: number) => number, illumination: number, seen = '2'): GameState {
    return {glyphAt: glyph, seenAt: () => seen, heightAt: height, selfHeight: () => 0, plainGlyphs: false,
        ambient: {env: {illumination}}} as unknown as GameState;
}

const Night = 0.08, Day = 1;                // The server's outdoor light under a clear night sky, and at noon.
const darkAt = (illumination: number) => 1 - illumination;
const brightest = (ops: Op[], keep: (o: Op) => boolean = () => true) =>
    Math.max(0, ...ops.filter(o => keep(o) && Number.isFinite(o.light)).map(o => o.light));

function paintGround(s: GameState, plain = false) {
    const {ctx, ops} = recorder();
    (s as unknown as {plainGlyphs: boolean}).plainGlyphs = plain;
    new GroundPainter().paint(ctx, s, {x0: 0, y0: 0, x1: 4, y1: 4}, 0, 0, 22);
    return ops;
}

test('at night every kind of ground is dimmed as the night says, and only a light source shines', () => {
    // Every kind in the terrain catalogue, in sight and remembered, as glyphs and as plain letters: the brightest thing
    // drawn for it at night is no brighter than the night leaves of the brightest by day. (Shallow water's ≈ was once
    // drawn at nearly its full day brightness under a black sky, and read as glowing: the user, 2026-11-15.)
    const allowed = nightLevel(darkAt(Night)) + 0.06;
    assert.ok(allowed < 0.6 && NightFloor < 0.5, 'the night is a real dimming');
    const failures: string[] = [];
    for (const t of TERRAIN)
        for (const seen of ['2', '1'])
            for (const plain of [false, true]) {
                const at = (illumination: number) => paintGround(ground(() => t.code, () => 0, illumination, seen), plain);
                const day = at(Day), night = at(Night);
                const lit = brightest(day), dim = brightest(night);
                const name = `${t.name} (${JSON.stringify(t.code)}, ${seen === '2' ? 'in sight' : 'remembered'}${plain ? ', plain' : ''})`;
                if (!(lit > 0)) failures.push(`${name}: nothing drawn by day`);
                else if (LightSources[t.code]) {
                    if (dim < lit * 0.95) failures.push(`${name}: a light source dimmed at night (${dim.toFixed(3)} of ${lit.toFixed(3)})`);
                    if (!night.some(o => o.pool) || day.some(o => o.pool)) failures.push(`${name}: its pool is not lit at night only`);
                } else if (dim > lit * allowed)
                    failures.push(`${name}: ${(dim / lit).toFixed(2)} of its day brightness at night (at most ${allowed.toFixed(2)})`);
                else if (night.some(o => o.pool)) failures.push(`${name}: a pool of light with no light source`);
            }
    assert.deepEqual(failures, []);
});

test('height edges, ledges, cliffs, stairs, slopes and raised ground are as dim at night as the ground', () => {
    const allowed = nightLevel(darkAt(Night)) + 0.06;
    // Two columns of ground: the west one at 0, the east one at `east`; each case draws one kind of edge.
    const cases: Array<[string, string, string, number]> = [
        ['a half-step contour', '.', '.', 0.5],
        ['a walkable step by a slope', ':', '.', 1],
        ['a walkable step by stairs', '^', '_', 1],
        ['a ledge', '.', '.', 1.5],
        ['a cliff\'s rim', '.', '%', 0],
        ['high ground above the wolf (the Rise)', '_', '_', 3],
        ['a long slope turned to the light', ',', ',', -1],
    ];
    for (const [name, west, east, rise] of cases) {
        // The slope case climbs a tile at a time eastward, so its tiles' facing (and their light wash) is strong.
        const height = (x: number) => rise < 0 ? x : x >= 2 ? rise : 0;
        const at = (illumination: number) => paintGround(ground(x => x >= 2 ? east : west, height, illumination));
        const day = at(Day), night = at(Night);
        for (const kind of ['line', 'rect', 'text'] as const) {
            const lit = brightest(day, o => o.kind === kind), dim = brightest(night, o => o.kind === kind);
            if (lit > 0) assert.ok(dim <= lit * allowed, `${name}: its ${kind}s keep ${(dim / lit).toFixed(2)} of their day brightness`);
        }
        if (name.includes('contour') || name.includes('step') || name.includes('ledge') || name.includes('rim'))
            assert.ok(brightest(day, o => o.kind === 'line') > 0, `${name}: an edge is drawn`);
    }
});

/** A small outdoor cell drawn whole at `illumination`: its doors, trees and roofs as the map draws them over the ground. */
function paintCell(illumination: number) {
    const {state: s} = testGame();
    const {ctx, ops} = recorder();
    const painter = new GamePainter(s, new Painter(ctx), {get: () => ({})} as unknown as Sheets);
    const rows = ['..........', '.P..ZZZ...', '....ZZZ...', '..^.......', '..........', '..........'];
    s.applySnapshot({cell: {id: 'yard', width: 10, height: 6, outdoors: true, rows,
        environment: {hour: illumination > 0.5 ? 12 : 23, phase: illumination > 0.5 ? 'day' : 'night', daylight: illumination > 0.5 ? 1 : 0,
            illumination}},
    self: {id: 'self', x: 1.5, y: 4.5}, entities: [], visibility: rows.map(r => '2'.repeat(r.length)),
    doors: [{id: 'door', x: 5.5, y: 3, name: 'Door', open: false}, {id: 'locked', x: 7.5, y: 3, name: 'Shop', locked: true},
        {id: 'stairs', x: 2.5, y: 3.5, name: 'Stairs up'}], time: 0, cellGeneration: 1} as Json);
    settleAmbient(painter);
    painter.paint(960, 600);
    return ops;
}

test('doors, stair markers, tree crowns and eaves are dimmed at night too', () => {
    const day = paintCell(Day), night = paintCell(Night);
    const groups: Array<[string, (o: Op) => boolean, number]> = [
        ['a door\'s marker', o => o.text === '+', 0.7],
        ['a locked door\'s marker', o => o.text === '×', 0.7],
        ['a stair\'s marker', o => o.kind === 'line' && o.text === undefined, 0.75],
        ['a tree\'s crown', o => o.text === '^^^^^^^', nightLevel(darkAt(Night)) + 0.06],
        ['a roof\'s eave', o => o.text === '////', nightLevel(darkAt(Night)) + 0.06],
    ];
    for (const [name, keep, most] of groups) {
        const lit = brightest(day, keep), dim = brightest(night, keep);
        assert.ok(lit > 0, `${name} is drawn`);
        assert.ok(dim <= lit * most, `${name} keeps ${(dim / lit).toFixed(2)} of its day brightness at night`);
    }
});

test('street lamps glow at night: a warm pool round them, none by day', () => {
    // A cobbled street with one lamp in the middle of it.
    const glyph = (x: number, y: number) => x === 2 && y === 2 ? "'" : '_';
    const tileLight = (illumination: number) => {
        const {ctx, ops} = recorder();
        const seen: Array<[number, number, number]> = [];
        // Which tile each cobble's characters fall in, by where they are drawn.
        (ctx as unknown as Record<string, unknown>).fillText = (text: string, x: number, y: number) => {
            const c = ctx as unknown as Record<string, unknown>;
            seen.push([Math.floor(x / 22), Math.floor(y / 22), brightness(c.fillStyle) * (c.globalAlpha as number)]);
            ops.push({kind: 'text', text, light: 0, pool: false});
        };
        new GroundPainter().paint(ctx, ground(glyph, () => 0, illumination), {x0: 0, y0: 0, x1: 8, y1: 4}, 0, 0, 22);
        return {seen, ops};
    };
    const near = (r: Array<[number, number, number]>) => Math.max(...r.filter(([x, y]) => Math.hypot(x - 2, y - 2) === 1).map(v => v[2]));
    const far = (r: Array<[number, number, number]>) => Math.max(...r.filter(([x]) => x >= 7).map(v => v[2]));
    const night = tileLight(Night), day = tileLight(Day);
    assert.ok(near(night.seen) > far(night.seen) * 1.4, `the ground by the lamp is lit (${near(night.seen)} by it, ${far(night.seen)} away)`);
    assert.ok(Math.abs(near(day.seen) - far(day.seen)) < 1e-6, 'by day the lamp lights nothing');
    const pools = (illumination: number) => {
        const {ctx, ops} = recorder();
        new GroundPainter().paint(ctx, ground(glyph, () => 0, illumination), {x0: 0, y0: 0, x1: 8, y1: 4}, 0, 0, 22);
        return ops.filter(o => o.pool).length;
    };
    assert.ok(pools(Night) > 0 && pools(Day) === 0, 'its pool is laid at night only');
    assert.ok(lampsLit(darkAt(0.5)) > 0 && lampsLit(darkAt(0.5)) < 1 && lampsLit(darkAt(0.8)) === 0, 'lamps come on through dusk');
    assert.ok(pools(0.7) === 0, 'and are out in daylight');
});

test('crossing a seam eases the sky: light, weather and tints fade over seconds, never in one frame', () => {
    const a = new Ambient();
    const env = (weather: string, illumination: number, intensity = 1) => ({weather, intensity, phase: 'night', lightingTone: 'neutral',
        lightSource: 'night sky', hour: 23, daylight: 0, illumination, artificialLight: 0, daylightAccess: 1, glowStrength: 0, sight: 1,
        hearing: 1, scent: 1, movement: 1});
    a.update(env('rain', 0.1), null, true, 10);
    assert.ok(a.env.illumination === 0.1, 'the first look is the server\'s');
    assert.ok(a.sheets.rain > 0.9 && a.washes.rain > 0.9 && a.sheets.snow === 0);
    // Into a brighter, snowy cell: a frame later, both kinds show, and the light is between the two.
    a.update(env('snow', 0.5), null, true, 10.05);
    assert.ok(a.env.illumination > 0.1 && a.env.illumination < 0.15, `the light eases (${a.env.illumination})`);
    assert.ok(a.sheets.rain > 0.8 && a.sheets.snow > 0 && a.sheets.snow < 0.1, 'rain thins while snow comes on');
    assert.ok(a.washes.rain > 0.8 && a.washes.snow < 0.1);
    for (let t = 10.1; t <= 11.5; t += 1 / 60) a.update(env('snow', 0.5), null, true, t);
    assert.ok(a.env.illumination > 0.3 && a.env.illumination < 0.5, 'still on its way after a second and a half');
    assert.ok(a.sheets.rain > 0.05 && a.sheets.snow > 0.5);
    for (let t = 11.5; t <= 16; t += 1 / 60) a.update(env('snow', 0.5), null, true, t);
    assert.ok(Math.abs(a.env.illumination - 0.5) < 0.01 && a.sheets.rain < 0.01 && a.sheets.snow > 0.99, 'and there within a few seconds');
    // A weather field fades in the same way: its masks are scaled by how far each kind has come.
    a.update(env('fog', 0.5), {cols: 2, rows: 1, step: 16, kinds: 'ff', amounts: '99'}, true, 16.05);
    assert.equal(a.target('fog'), 1);
    assert.ok(a.sheets.fog > 0 && a.sheets.fog < 0.1 && a.sheets.snow > 0.9);
    // A long pause (a hidden page) jumps to the new look.
    a.update(env('clear', 1), null, true, 100);
    assert.ok((a.env as {illumination: number}).illumination === 1 && a.sheets.fog === 0 && a.sheets.clear === 1);

    // The page's first cell is drawn as it is, not faded into from the page's default noon.
    const page = new Ambient();
    page.update(env('clear', 1), null, true, 1);
    page.update(env('rain', 0.1), null, true, 1.02, 'lanes');
    assert.ok(page.env.illumination === 0.1 && page.sheets.rain > 0.9, 'the first cell is not faded into');
});

test('the map eases across a cell crossing: the frame after it is drawn between the two cells\' light', () => {
    const {state: s} = testGame();
    const painter = new GamePainter(s, new Painter(fakeContext()), {get: () => ({})} as unknown as Sheets);
    const cell = (id: string, illumination: number, weather: string): Json => ({cell: {id, width: 8, height: 6, outdoors: true, weather,
        rows: Array(6).fill(','.repeat(8)), environment: {hour: 23, phase: 'night', daylight: 0, illumination}},
    self: {id: 'self', x: 3.5, y: 3.5}, entities: [], doors: [], time: 0, cellGeneration: 1});
    s.applySnapshot(cell('lanes', 0.08, 'rain'));
    s.clock = 5;
    painter.paint(960, 600);
    assert.equal(s.ambient.env.illumination, 0.08);
    s.applySnapshot(cell('square', 0.6, 'snow'));
    s.clock = 5 + 1 / 60;
    painter.paint(960, 600);
    const after = s.ambient.env.illumination;
    assert.ok(after > 0.08 && after < 0.12, `one frame after the crossing the light has barely moved (${after})`);
    assert.ok(s.ambient.sheets.rain > 0.9 && s.ambient.sheets.snow < 0.05, 'the rain is still falling as the snow begins');
});
