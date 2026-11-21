// The look of the sky, eased (the user, 2026-11-15: "Transitioning between cells changes the ambient effect
// immediately, such as rain to snow and the brightness of the screen"). The server sends each cell's light and weather
// as they are; a crossing used to swap them in one frame. What the map draws from is this: the light, the weather's
// sheets and washes, the dawn and dusk tints and an interior's glow, each following what the server says over a few
// seconds, so rain thins as snow comes on and a dark street brightens into a lit one rather than snapping.
import {clamp, num, objects, str, type Json} from './json.ts';
import type {EnvironmentView} from './labels.ts';
import {lerp, rgb, type Color} from '../ui/color.ts';

/** The weather kinds the painter draws, by the server's grid letter (RatwGame.cpp snapshot). */
export const KindOfLetter: Readonly<Record<string, string>> = {r: 'rain', f: 'fog', n: 'snow', o: 'overcast', s: 'storm', d: 'sandstorm',
    c: 'clear'};
export const WeatherKinds = ['clear', 'overcast', 'rain', 'storm', 'fog', 'snow', 'sandstorm'];
/** The tints laid over the outdoor ground: one a kind of weather, and the dawn, dusk and sunlit-day washes. */
export const Washes = [...WeatherKinds.filter(k => k !== 'clear'), 'dawn', 'dusk', 'day'];

/** Seconds for an eased value to close about two thirds of its way (about 95% in three times as long). */
export const AmbientSeconds = 1.2;
/** A pause longer than this (a hidden page, a reconnect) jumps straight to the new look. */
const LongPause = 10;

export interface WeatherFieldView {
    cols: number;
    rows: number;
    step: number;
    kinds: string;
    amounts: string;
}

/** A field's weak samples are left to the tint: only from four ninths up does a kind draw its sheets (paint.ts). */
export const FieldSheetLeast = 4;

/**
 * Where tile (0, 0) of cell `from` lies in cell `to`'s tiles, from the two cells' places in the world (the travel map
 * and the map of what the wolf knows, as the server sends them); null where either is not known or they are not on
 * one level.
 */
export function seamOffset(snapshot: Json | null, from: string, to: string): [number, number] | null {
    const place = (id: string) => [...objects(snapshot, 'travelMap'), ...objects(snapshot, 'worldMap')].find(c => str(c, 'id') === id);
    const a = place(from), b = place(to);
    if (!a || !b || num(a, 'z') !== num(b, 'z')) return null;
    const dx = num(a, 'x', NaN) - num(b, 'x', NaN), dy = num(a, 'y', NaN) - num(b, 'y', NaN);
    return Number.isFinite(dx) && Number.isFinite(dy) ? [dx, dy] : null;
}

const glowOf = (tone: string): Color => tone === 'warm' ? rgb(0xeaa34f) : tone === 'cool' ? rgb(0x87b7dd) : rgb(0xd8d4bf);

export class Ambient {
    /** The environment to draw by: the server's, with its light eased. Its words (weather, phase) are the server's now. */
    env: EnvironmentView = {weather: 'clear', intensity: 1, phase: 'day', lightingTone: 'neutral', lightSource: 'daylight', hour: 12,
        daylight: 1, illumination: 1, artificialLight: 0, daylightAccess: 1, glowStrength: 0, sight: 1, hearing: 1, scent: 1, movement: 1};
    /** How strongly each kind's moving sheets (rain, snow, mist...) are drawn, eased. */
    readonly sheets: Record<string, number> = Object.fromEntries(WeatherKinds.map(k => [k, 0]));
    /** How strongly each wash (a kind of weather's tint, dawn, dusk, the sunlit day) is laid on, eased. */
    readonly washes: Record<string, number> = Object.fromEntries(Washes.map(k => [k, 0]));
    /** The old cell's visible weather, held while the new field fades over it (an old cell with no field: drawn alike all over). */
    readonly priorSheets: Record<string, number> = Object.fromEntries(WeatherKinds.map(k => [k, 0]));
    /**
     * The old cell's field, held while the new one fades in (the user, 2026-11-20: "screen color changes rather
     * dramatically during a transition from one cell to another"): drawn where it lay, `priorOffset` (where the old
     * cell's tile (0, 0) is in the new cell's tiles), each kind as strongly as it was drawn as the wolf crossed
     * (`priorShown`), so the frame after a crossing is the frame before it and the new field comes in over it.
     */
    priorField: WeatherFieldView | null = null;
    priorOffset: [number, number] = [0, 0];
    readonly priorShown: Record<string, number> = Object.fromEntries(WeatherKinds.map(k => [k, 0]));
    /** How far the new cell's field has come in over the old one's (0 at a crossing, 1 once it has). */
    fieldMix = 1;
    private field: WeatherFieldView | null = null;
    /** An interior's glow colour, eased. */
    glow: Color = glowOf('neutral');
    private goalSheets: Record<string, number> = Object.fromEntries(WeatherKinds.map(k => [k, 0]));
    private last = NaN;
    private cell = '';

    /** What the server says the sheets should be now (paint.ts scales a field's masks by sheet / target as one fades in). */
    target(kind: string): number {
        return this.goalSheets[kind] ?? 0;
    }

    /**
     * How strongly a kind of the field is drawn now: `now`, the new cell's field (as far as the kind has come in, times
     * the field's share), and `prior`, the old cell's held after a crossing (as it was drawn then, times what is left
     * of its share). Across the crossing frame `prior` takes up just what `now` was, so nothing jumps.
     */
    shares(kind: string): {now: number; prior: number} {
        const goal = this.goalSheets[kind] ?? 0;
        const fade = goal > 0 ? Math.min(1, (this.sheets[kind] ?? 0) / goal) : 0;
        return {now: fade * this.fieldMix, prior: this.priorField && this.fieldMix < 1 ? (this.priorShown[kind] ?? 0) * (1 - this.fieldMix) : 0};
    }

    /** Snaps everything to what the server says, on the next update. */
    reset() {
        this.last = NaN;
        this.cell = '';
        this.fieldMix = 1;
        this.priorField = null;
    }

    /**
     * Once a frame, before drawing: eases toward `e` (and the weather field over the cell) by the time since the last.
     * The first cell the page is shown (`cell`, its id) is drawn as it is, not faded into from the page's default day.
     * `offset`, at a crossing, is where the old cell's tile (0, 0) lies in the new cell's tiles (seamOffset): with it
     * the old cell's field stays where it was while the new one comes in.
     */
    update(e: EnvironmentView, field: WeatherFieldView | null, outdoors: boolean, clock: number, cell = '',
           offset: [number, number] | null = null) {
        const dt = clock - this.last;
        const snap = !(dt >= 0 && dt <= LongPause) || (!this.cell && !!cell);
        if (!snap && cell && this.cell && cell !== this.cell) {
            // The old field as it was drawn: each kind at its fade, times its own share if it was still coming in.
            const held = this.field && offset && outdoors ? this.field : null;
            for (const kind of WeatherKinds) {
                const goal = this.goalSheets[kind];
                this.priorShown[kind] = held && goal > 0 ? Math.min(1, this.sheets[kind] / goal) * this.fieldMix : 0;
                this.priorSheets[kind] = held ? 0 : this.sheets[kind];
            }
            this.priorField = held;
            if (held) this.priorOffset = [offset![0], offset![1]];
            this.fieldMix = 0;
        }
        this.last = clock;
        if (cell) this.cell = cell;
        this.field = field;
        const k = snap ? 1 : 1 - Math.exp(-dt / AmbientSeconds);
        const mixing = this.fieldMix < 1;
        this.fieldMix = snap ? 1 : Math.min(1, this.fieldMix + dt / (AmbientSeconds * 2.7));
        const ease = (from: number, to: number) => {
            const v = from + (to - from) * k;
            return Math.abs(v - to) < 0.002 ? to : v;
        };
        // The sheets each kind draws.
        for (const kind of WeatherKinds) this.goalSheets[kind] = 0;
        if (outdoors) {
            if (field) {
                for (let i = 0; i < field.kinds.length; ++i) {
                    const kind = KindOfLetter[field.kinds[i]], amount = Number(field.amounts[i]);
                    if (kind && kind !== 'clear' && amount >= FieldSheetLeast)
                        this.goalSheets[kind] = Math.max(this.goalSheets[kind], amount / 9);
                }
                if (e.weather === 'clear') this.goalSheets.clear = 1;      // The sunlit clear ground's cloud shadows.
            } else this.goalSheets[e.weather] = e.weather === 'clear' ? 1 : Math.max(0.2, e.intensity);
        }
        for (const kind of WeatherKinds) this.sheets[kind] = ease(this.sheets[kind], this.goalSheets[kind]);
        if (this.fieldMix >= 1 && (snap || mixing)) {
            // The old field has gone: a kind it had and the new cell has not went with it (it is not then drawn
            // over the whole screen, as a kind fading out of the field where the wolf stands is).
            if (this.priorField)
                for (const kind of WeatherKinds) if (kind !== 'clear' && this.goalSheets[kind] <= 0) this.sheets[kind] = 0;
            this.priorField = null;
        }
        // The washes over the outdoor ground.
        for (const wash of Washes) {
            let goal = 0;
            if (outdoors) {
                if (wash === 'dawn' || wash === 'dusk') goal = e.phase === wash ? 1 : 0;
                else if (wash === 'day') goal = e.weather === 'clear' && e.phase === 'day' ? e.daylight : 0;
                else goal = e.weather === wash ? Math.max(0.25, e.intensity) : 0;
            }
            this.washes[wash] = ease(this.washes[wash], goal);
        }
        const was = this.env;
        this.env = {...e,
            illumination: ease(was.illumination, clamp(e.illumination, 0, 1)),
            daylight: ease(was.daylight, clamp(e.daylight, 0, 1)),
            glowStrength: ease(was.glowStrength, outdoors ? 0 : clamp(e.glowStrength, 0, 1)),
            intensity: ease(was.intensity, e.intensity)};
        this.glow = snap ? glowOf(e.lightingTone) : lerp(this.glow, glowOf(e.lightingTone), k);
    }
}
