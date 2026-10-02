// Stand-ins for the tests: a composer, and a 2D context that records nothing but answers what painting asks (text
// widths), so the game's drawing can run in Node and be checked by the click targets and map geometry it leaves.
import {GameState, type Composer} from './state.ts';
import {GamePainter} from './paint.ts';
import {Painter} from '../ui/painter.ts';
import type {Sheets} from './weatherArt.ts';
import type {Portraits} from '../ui/portrait.ts';
import type {Json} from './json.ts';

export class FakeComposer implements Composer {
    text = '';
    focused = false;
    focus() { this.focused = true; }
    blur() { this.focused = false; }
    insertNewline() { this.text += '\n'; }
}

export function fakeContext(): CanvasRenderingContext2D {
    let font = '16px sans';
    const noop = () => {};
    const gradient = {addColorStop: noop};
    const context: Record<string, unknown> = {
        get font() { return font; },
        set font(v: string) { font = v; },
        measureText: (text: string) => ({width: text.length * (parseFloat(/([\d.]+)px/.exec(font)?.[1] ?? '16') * 0.55)}),
        createLinearGradient: () => gradient,
        createPattern: () => ({setTransform: noop}),
    };
    for (const name of ['fillRect', 'fillText', 'beginPath', 'moveTo', 'lineTo', 'stroke', 'save', 'restore', 'rect', 'clip', 'roundRect',
        'translate', 'rotate', 'setTransform', 'drawImage', 'clearRect'])
        context[name] = noop;
    return new Proxy(context, {
        get: (target, key) => (key in target ? target[key as string] : undefined),
        set: (target, key, value) => {
            if (key === 'font') font = value;
            else target[key as string] = value;
            return true;
        },
    }) as unknown as CanvasRenderingContext2D;
}

if (typeof (globalThis as Record<string, unknown>).DOMMatrix === 'undefined') {
    (globalThis as Record<string, unknown>).DOMMatrix = class {
        translateSelf() { return this; }
        rotateSelf() { return this; }
        scaleSelf() { return this; }
    };
}

/** A game screen with no page: the state, what it sent, and a painter over a fake context. With `offscreen`, the map's
 * ground is kept in fake offscreen canvases as on a page (counted in `surfaces`). */
export function testGame(offscreen = false) {
    const surfaces: Array<{width: number; height: number}> = [];
    const commands: Json[] = [];
    const composer = new FakeComposer();
    const state = new GameState(c => commands.push(c), composer);
    const sheets = {get: () => ({})} as unknown as Sheets;
    const portraits = {get: () => null, failed: () => false} as unknown as Portraits;
    const factory = offscreen ? (width: number, height: number) => {
        const canvas = {width, height};
        surfaces.push(canvas);
        return {canvas: canvas as unknown as HTMLCanvasElement, ctx: fakeContext()};
    } : undefined;
    const painter = new GamePainter(state, new Painter(fakeContext()), sheets, portraits, factory);
    return {state, commands, composer, painter, surfaces};
}

/** The painter's private drawing steps, for tests that draw one part. */
export function draw(painter: GamePainter, step: 'drawLocal' | 'drawModal' | 'drawTravelAtlas' | 'drawPace' | 'drawScent', ...args: unknown[]) {
    (painter as unknown as Record<string, (...a: unknown[]) => void>)[step](...args);
}
export function paintPart<T>(painter: GamePainter, part: 'atmosphere' | 'weatherLayers' | 'weatherMarks' | 'lightningFlash' | 'cellBounds' |
    'visibleCellBounds'): T {
    return (painter as unknown as Record<string, () => T>)[part]();
}
