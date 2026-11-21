// A crossing between cells changes the screen's colour gradually (ambient.ts, paint.ts; the user, 2026-11-20: "screen
// color changes rather dramatically during a transition from one cell to another"): the frame after a seam is the
// frame before it, and the new cell's look comes in over a few seconds.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {Ambient, seamOffset, type WeatherFieldView} from './ambient.ts';
import {fakeContext, testGame} from './testing.ts';
import {GamePainter} from './paint.ts';
import {Painter} from '../ui/painter.ts';
import type {Sheets} from './weatherArt.ts';
import type {Json} from './json.ts';
import type {EnvironmentView} from './labels.ts';

const sky = (weather: string): EnvironmentView => ({weather, intensity: 1, phase: 'day', lightingTone: 'neutral', lightSource: 'daylight',
    hour: 12, daylight: 1, illumination: 1, artificialLight: 0, daylightAccess: 1, glowStrength: 0, sight: 1, hearing: 1, scent: 1,
    movement: 1});

test('where one cell lies in the next, from their places on the travel map', () => {
    const snapshot: Json = {travelMap: [{id: 'a', x: 256, y: 256, z: 0}, {id: 'b', x: 256, y: 512, z: 0}, {id: 'up', x: 256, y: 512, z: 1}],
        worldMap: [{id: 'c', x: 512, y: 256, z: 0}]};
    assert.deepEqual(seamOffset(snapshot, 'a', 'b'), [0, -256], 'crossing south: the old cell is a cell above');
    assert.deepEqual(seamOffset(snapshot, 'a', 'c'), [-256, 0], 'a cell only glimpsed is placed too');
    assert.equal(seamOffset(snapshot, 'a', 'up'), null, 'not on one level');
    assert.equal(seamOffset(snapshot, 'a', 'gone'), null, 'not known');
});

test('a crossing holds the old field where it lay and hands it over to the new one', () => {
    const a = new Ambient();
    const west: WeatherFieldView = {cols: 3, rows: 1, step: 16, kinds: 'rrf', amounts: '995'};
    const east: WeatherFieldView = {cols: 3, rows: 1, step: 16, kinds: 'rrc', amounts: '990'};
    for (let t = 1; t <= 6; t += 1 / 60) a.update(sky('rain'), west, true, t, 'west');
    const before = a.shares('rain'), fogBefore = a.shares('fog');
    assert.ok(before.now > 0.99 && before.prior === 0 && fogBefore.now > 0.99, 'settled: the field is drawn whole');
    a.update(sky('rain'), east, true, 6 + 1 / 60, 'east', [-48, 0]);
    assert.equal(a.priorField, west);
    assert.deepEqual(a.priorOffset, [-48, 0]);
    const after = a.shares('rain'), fogAfter = a.shares('fog');
    // The frame after the seam draws what the frame before did: the old field all but whole, the new barely begun.
    assert.ok(Math.abs(after.prior - before.now) < 0.01 && after.now < 0.01, `rain ${JSON.stringify(after)}`);
    assert.ok(Math.abs(fogAfter.prior - fogBefore.now) < 0.01 && fogAfter.now === 0, `fog ${JSON.stringify(fogAfter)}`);
    // Half way, both; neither step of the hand-over is large.
    let last = after.prior + after.now, t = 6 + 1 / 60;
    for (; t < 7.5; t += 1 / 60) {
        a.update(sky('rain'), east, true, t, 'east');
        const share = a.shares('rain');
        assert.ok(Math.abs(share.prior + share.now - last) < 0.02, 'the rain never jumps');
        last = share.prior + share.now;
    }
    const mid = a.shares('rain');
    assert.ok(mid.prior > 0.3 && mid.now > 0.3, `both fields are drawn half way (${JSON.stringify(mid)})`);
    for (; t < 11; t += 1 / 60) a.update(sky('rain'), east, true, t, 'east');
    assert.equal(a.priorField, null, 'the old field has gone');
    assert.deepEqual(a.shares('rain'), {now: 1, prior: 0});
    assert.equal(a.sheets.fog, 0, 'the fog that was only in the old field is not then drawn all over the screen');
});

test('without knowing where the old cell lay, the old weather fades as it was, all over', () => {
    const a = new Ambient();
    const field: WeatherFieldView = {cols: 2, rows: 1, step: 16, kinds: 'rr', amounts: '99'};
    for (let t = 1; t <= 6; t += 1 / 60) a.update(sky('rain'), field, true, t, 'west');
    a.update(sky('rain'), field, true, 6.02, 'east');
    assert.equal(a.priorField, null);
    assert.ok(a.priorSheets.rain > 0.99 && a.fieldMix < 0.05);
});

/** A context that keeps what is drawn as the tests need it: the gradients made, and each image drawn (and how see-through). */
function recording(width: number, height: number) {
    const c = fakeContext() as unknown as Record<string, unknown>;
    const gradients: number[][] = [];
    const images: Array<{w: number; x: number; y: number; alpha: number}> = [];
    let alpha = 1;
    const alphas: number[] = [];
    Object.assign(c, {
        canvas: {width, height},
        getTransform: () => ({a: 1, b: 0, c: 0, d: 1, e: 0, f: 0}),
        createImageData: (w: number, h: number) => ({width: w, height: h, data: new Uint8ClampedArray(w * h * 4)}),
        putImageData: () => {},
        createLinearGradient: (...args: number[]) => {
            gradients.push(args);
            return {addColorStop: () => {}};
        },
        drawImage: (image: {width: number}, x = 0, y = 0) => images.push({w: image?.width ?? 0, x, y, alpha}),
        save: () => alphas.push(alpha),
        restore: () => { alpha = alphas.pop() ?? 1; },
    });
    const ctx = new Proxy(c, {
        get: (target, key) => key === 'globalAlpha' ? alpha : target[key as string],
        set: (target, key, value) => {
            if (key === 'globalAlpha') alpha = value;
            else target[key as string] = value;
            return true;
        },
    }) as unknown as CanvasRenderingContext2D;
    return {ctx, gradients, images};
}

test('the frame after a crossing is drawn as the frame before: the sky laid over the screen, the old field where it was', () => {
    const {state: s} = testGame();
    const main = recording(960, 600);
    const surfaces: Array<ReturnType<typeof recording>> = [];
    const painter = new GamePainter(s, new Painter(main.ctx), {get: () => ({width: 64, height: 64})} as unknown as Sheets,
        (width, height) => {
            const r = recording(width, height);
            surfaces.push(r);
            return {canvas: r.ctx.canvas as unknown as HTMLCanvasElement, ctx: r.ctx};
        });
    // Rain over the west half of each cell: a field with an edge, so it is masked.
    const field = {cols: 17, rows: 17, step: 16, kinds: Array.from({length: 289}, (_, i) => (i % 17 < 9 ? 'r' : 'c')).join(''),
        amounts: Array.from({length: 289}, (_, i) => (i % 17 < 9 ? '9' : '0')).join('')};
    const travelMap = [{id: 'north', x: 0, y: 0, z: 0, width: 256, height: 256}, {id: 'south', x: 0, y: 256, z: 0, width: 256, height: 256}];
    // Outdoor ground with the band round it (doc 70), so the view follows the wolf over the seam as in the world.
    const band = 8, fringeRows = Array.from({length: 256 + 2 * band}, (_, r) => ','.repeat(r < band || r >= band + 256 ? 256 + 2 * band : 2 * band));
    const cell = (id: string, y: number): Json => ({cell: {id, width: 256, height: 256, outdoors: true, weather: 'rain', fringe: band, fringeRows,
        rows: Array(256).fill(','.repeat(256)), localWeather: {kind: 'rain', intensity: 1}, weatherField: field,
        environment: {hour: 12, phase: 'day', daylight: 1, illumination: 1}},
    self: {id: 'self', x: 60.5, y}, entities: [], doors: [], time: 0, cellGeneration: 1, travelMap});
    const frame = () => {
        main.gradients.length = 0;
        for (const r of surfaces) r.images.length = 0;
        main.images.length = 0;
        painter.paint(960, 600);
        // The rain's masks (17 samples wide, and one more each side for the band), where they were laid and how
        // strongly the rain then showed.
        const masks = surfaces.flatMap(r => r.images.filter(i => i.w === 19));
        return {ground: main.gradients[0], masks, composite: main.images.filter(i => i.w === 960).map(i => i.alpha)};
    };
    s.applySnapshot(cell('north', 255.9));
    for (let t = 5; t <= 9; t += 1 / 60) {
        s.clock = t;
        frame();
    }
    s.clock = 9;
    const before = frame();
    s.applySnapshot(cell('south', 0.1));
    s.clock = 9 + 1 / 60;
    const after = frame();
    assert.deepEqual(after.ground, before.ground, 'the ground\'s shade is laid over the screen, not the cell: it does not move');
    assert.equal(before.masks.length, 1);
    const [was] = before.masks;
    const tile = s.tileSize;
    // The old field, still where it lay (to within the wolf's step over the seam), all but as strong as it was.
    const held = after.masks.find(m => Math.abs(m.x - was.x) <= tile * 0.25 && Math.abs(m.y - was.y) <= tile * 0.25);
    assert.ok(held, `the old field is drawn where it was (${JSON.stringify(was)} then ${JSON.stringify(after.masks)})`);
    assert.ok(Math.abs(held!.alpha * Math.max(...after.composite) - was.alpha * Math.max(...before.composite)) < 0.02,
        'and as strongly');
    const fresh = after.masks.find(m => m !== held);
    assert.ok(fresh && fresh.alpha < 0.02, 'the new cell\'s field has barely begun');
});
