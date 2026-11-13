// The painted wolves' colours (Docs/Design/81-painted-wolves.md): the palette (Data/Wolves/palette.json, generated
// into furPalette.generated.ts), a wolf's fur as it is kept (palette ids, checked as Core/RatwAppearance.cpp checks
// them), the ramp that turns a painting's light and dark into the player's colours, the blend between fur zones, and
// the painted look an older wolf (the code-drawn look: a coat colour, a pattern, markings) is shown in.
// No page needed: wolfPaint.ts loads the paintings and draws.
import {EyeColours, FurColours, FurZones, ReadyCoats, type ReadyCoat} from './furPalette.generated.ts';
import {CoatColors} from './theme.ts';
import {isObject} from '../game/json.ts';

export {EyeColours, FurColours, FurZones, ReadyCoats, type ReadyCoat};

/** Three fur palette ids: the painting's dark, mid and light fur. */
export type FurCoat = [string, string, string];
export interface Fur {
    coat: FurCoat;
    eyes: string;                          // An eye palette id.
    zones?: Record<string, FurCoat>;       // A zone's own colours; a zone not here wears the coat.
}
export type Build = 'lean' | 'average' | 'heavy';
export const Builds: readonly Build[] = ['lean', 'average', 'heavy'];
export const BuildNames: Record<Build, string> = {lean: 'Lean', average: 'Average', heavy: 'Heavy'};
export type Rgb = [number, number, number];

const furById = new Map(FurColours.map(c => [c.id, c]));
const eyeById = new Map(EyeColours.map(c => [c.id, c]));
const zoneIndex = new Map(FurZones.map((z, i) => [z.id, i]));

export const isFurColour = (id: unknown): id is string => typeof id === 'string' && furById.has(id);
export const isEyeColour = (id: unknown): id is string => typeof id === 'string' && eyeById.has(id);
export const isFurZone = (id: unknown): id is string => typeof id === 'string' && zoneIndex.has(id);
export const furName = (id: string) => furById.get(id)?.name ?? '';
export const eyeName = (id: string) => eyeById.get(id)?.name ?? '';
export const zoneName = (id: string) => FurZones[zoneIndex.get(id) ?? -1]?.name ?? '';

export function hexRgb(hex: string): Rgb {
    const n = parseInt(hex.slice(1), 16);
    return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}
export const furRgb = (id: string): Rgb => hexRgb(furById.get(id)?.hex ?? '#000000');
export const eyeRgb = (id: string): Rgb => hexRgb(eyeById.get(id)?.hex ?? '#000000');
export const furHex = (id: string) => furById.get(id)?.hex ?? '#000000';
export const eyeHex = (id: string) => eyeById.get(id)?.hex ?? '#000000';

function readCoat(v: unknown): FurCoat | null {
    return Array.isArray(v) && v.length === 3 && v.every(isFurColour) ? [v[0], v[1], v[2]] : null;
}

/** A wolf's fur as the server checks it (wire::readAppearance): null for anything incomplete or off the palette. */
export function readFur(v: unknown): Fur | null {
    if (!isObject(v)) return null;
    const keys = Object.keys(v);
    if (keys.some(k => k !== 'coat' && k !== 'eyes' && k !== 'zones')) return null;
    const coat = readCoat(v.coat);
    if (!coat || !isEyeColour(v.eyes)) return null;
    const out: Fur = {coat, eyes: v.eyes};
    if (v.zones !== undefined) {
        if (!isObject(v.zones) || !Object.keys(v.zones).length) return null;
        out.zones = {};
        for (const [zone, colours] of Object.entries(v.zones)) {
            const own = readCoat(colours);
            if (!isFurZone(zone) || !own) return null;
            out.zones[zone] = own;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------------------------- The ramp

/** Where the ramp's colours lie on the painting's luminance: near black, the dark fur, the mid fur, the light fur,
 *  near white (tools/avatar_prep.py's STOPS: the paintings' own dark, mid and light patches). */
export const RampStops = [0, 38, 118, 212, 255];

/** The painting's luminance (0-255) to a colour: 256 colours, three bytes each. The brushwork is the luminance, so it
 *  is kept; only what colour each tone is changes. */
export function furRamp(dark: Rgb, mid: Rgb, light: Rgb): Uint8Array {
    const stops: Rgb[] = [[dark[0] * 0.45, dark[1] * 0.45, dark[2] * 0.45], dark, mid, light,
        [light[0] + (255 - light[0]) * 0.5, light[1] + (255 - light[1]) * 0.5, light[2] + (255 - light[2]) * 0.5]];
    const out = new Uint8Array(256 * 3);
    let s = 0;
    for (let l = 0; l < 256; ++l) {
        while (s < RampStops.length - 2 && l > RampStops[s + 1]) ++s;
        const t = (l - RampStops[s]) / (RampStops[s + 1] - RampStops[s]);
        for (let c = 0; c < 3; ++c) out[l * 3 + c] = Math.round(stops[s][c] + (stops[s + 1][c] - stops[s][c]) * t);
    }
    return out;
}

/** One ramp per fur zone, in the zone maps' order: the zone's own colours, else the coat's. */
export function zoneRamps(fur: Fur): Uint8Array[] {
    const coat = furRamp(furRgb(fur.coat[0]), furRgb(fur.coat[1]), furRgb(fur.coat[2]));
    return FurZones.map(z => {
        const own = fur.zones?.[z.id];
        return own ? furRamp(furRgb(own[0]), furRgb(own[1]), furRgb(own[2])) : coat;
    });
}

/** A cut-out and its map at one size, as wolfPaint.ts reads them off the page (RGBA, four bytes a pixel). */
export interface FurSource {
    width: number;
    height: number;
    picture: Uint8ClampedArray;     // The painting, cut from its paper.
    map: Uint8ClampedArray;         // Red: two zones (low and high four bits). Green: how much of the second. Blue:
                                    // 0 fur, 1-127 ink (kept as painted), 128-255 the iris.
}

/**
 * Paints a cut-out in a wolf's fur: every pixel's luminance through its zone's ramp, two zones blended where the map
 * feathers them, the iris in the eye colour (shaded as painted), the nose and the eyes' rims left as painted.
 */
export function paintFur(source: FurSource, ramps: Uint8Array[], eye: Rgb, out: Uint8ClampedArray): void {
    const {picture, map} = source;
    const count = source.width * source.height;
    for (let i = 0; i < count; ++i) {
        const p = i * 4;
        const alpha = picture[p + 3];
        if (!alpha) {
            out[p] = out[p + 1] = out[p + 2] = out[p + 3] = 0;
            continue;
        }
        const lum = (picture[p] * 77 + picture[p + 1] * 150 + picture[p + 2] * 29) >> 8;
        const packed = map[p], share = map[p + 1], feature = map[p + 2];
        const a = ramps[packed & 15] ?? ramps[0], l = lum * 3;
        let r = a[l], g = a[l + 1], b = a[l + 2];
        if (share) {
            const other = ramps[packed >> 4] ?? ramps[0], t = share / 255;
            r += (other[l] - r) * t;
            g += (other[l + 1] - g) * t;
            b += (other[l + 2] - b) * t;
        }
        if (feature >= 128) {
            const t = (feature - 128) / 127, shade = Math.min(1.25, Math.max(0.5, lum / 170));
            r += (Math.min(255, eye[0] * shade) - r) * t;
            g += (Math.min(255, eye[1] * shade) - g) * t;
            b += (Math.min(255, eye[2] * shade) - b) * t;
        } else if (feature) {
            const t = feature / 127;
            r += (picture[p] - r) * t;
            g += (picture[p + 1] - g) * t;
            b += (picture[p + 2] - b) * t;
        }
        out[p] = r;
        out[p + 1] = g;
        out[p + 2] = b;
        out[p + 3] = alpha;
    }
}

// ---------------------------------------------------------------------------------------------- An older wolf's fur

/** How far apart two colours look ("redmean": cheap, and good enough to pick a neighbour). */
function apart(a: Rgb, b: Rgb): number {
    const mean = (a[0] + b[0]) / 2, dr = a[0] - b[0], dg = a[1] - b[1], db = a[2] - b[2];
    return (2 + mean / 256) * dr * dr + 4 * dg * dg + (2 + (255 - mean) / 256) * db * db;
}

function nearest<T extends {id: string; hex: string}>(colours: readonly T[], to: Rgb): string {
    let best = colours[0].id, least = Infinity;
    for (const c of colours) {
        const d = apart(hexRgb(c.hex), to);
        if (d < least) [best, least] = [c.id, d];
    }
    return best;
}

/** The fur colour nearest a colour ("#rrggbb" or its parts). */
export const nearestFur = (colour: string | Rgb) => nearest(FurColours, typeof colour === 'string' ? hexRgb(colour) : colour);
export const nearestEye = (colour: string | Rgb) => nearest(EyeColours, typeof colour === 'string' ? hexRgb(colour) : colour);

const mix = (a: Rgb, b: Rgb, t: number): Rgb => [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t];

/** One colour as a painted coat: itself for the mid fur, its shadow and its pale for the dark and the light. */
export function coatOf(colour: Rgb): FurCoat {
    return [nearestFur(mix(colour, [0, 0, 0], 0.5)), nearestFur(colour), nearestFur(mix(colour, [255, 255, 255], 0.6))];
}

/** Which zone an older marking is nearest (only those whose names say so; the rest have no zone). */
export const MarkingZones: Record<string, string> = {socks: 'legs', stockings: 'legs', tail_tip: 'tail_tip', mask: 'head', bib: 'chest',
    belly: 'chest', saddle: 'back', cape: 'back'};

/** What furOf and buildOf read of an appearance (portrait.ts's Appearance has these). */
export interface OlderLook {
    species: string;
    stature: string;
    pattern: string;
    baseColor: number;
    gradientColor: number;
    markingColor: number;
    gradientAmount: number;
    patternAmount: number;
    coat?: string;
    gradientTint?: string;
    markingTint?: string;
    eyes?: string;
    build?: string;
    markings?: Array<{mask: string; color: string; opacity: number}>;
    fur?: Fur;
}

/** Which of the three paintings a wolf is: its build; for an older wolf with none, a tall or slender one is lean. */
export function buildOf(a: OlderLook): Build {
    if (a.build === 'lean' || a.build === 'heavy' || a.build === 'average') return a.build;
    if (a.fur) return 'average';
    return a.stature === 'tall' || a.species === 'maned' || a.species === 'ethiopian' || a.species === 'red' ? 'lean' : 'average';
}

/**
 * A wolf's fur: what it chose, or for an older wolf (no painted look kept) the nearest to what it had. Its coat colour
 * becomes the coat; the belly-and-legs colour, by how far it reached, the chest and the legs; with markings, each whose
 * name is a zone's (socks and stockings the legs, a tail tip, a mask the head, a bib or belly the chest, a saddle or
 * cape the back) that zone, by its strength; without, the pattern (a saddle or a mantle the back, piebald the sides).
 * Nothing is saved: the same older look always gives the same fur.
 */
export function furOf(a: OlderLook): Fur {
    if (a.fur) return a.fur;
    const palette = (i: number): Rgb => hexRgb(`#${(CoatColors[i] ?? CoatColors[3]).toString(16).padStart(6, '0')}`);
    const base = a.coat ? hexRgb(a.coat) : palette(a.baseColor);
    const coat = coatOf(base);
    const zones: Record<string, FurCoat> = {};
    const put = (zone: string, colour: Rgb) => {
        const own = coatOf(colour);
        if (own.join() !== coat.join()) zones[zone] = own;
    };
    if (a.gradientAmount >= 0.15) {
        const belly = mix(base, a.gradientTint ? hexRgb(a.gradientTint) : palette(a.gradientColor), a.gradientAmount);
        put('chest', belly);
        put('legs', belly);
    }
    if (a.markings?.length) {
        for (const m of a.markings) {
            const zone = MarkingZones[m.mask];
            if (zone && m.opacity >= 0.25) put(zone, mix(base, hexRgb(m.color), m.opacity));
        }
    } else if (a.pattern !== 'solid' && a.patternAmount >= 0.15) {
        const marked = mix(base, a.markingTint ? hexRgb(a.markingTint) : palette(a.markingColor), a.patternAmount);
        put(a.pattern === 'piebald' ? 'sides' : 'back', marked);
    }
    const fur: Fur = {coat, eyes: a.eyes ? nearestEye(a.eyes) : 'amber'};
    if (Object.keys(zones).length) fur.zones = zones;
    return fur;
}

/** The zone an older marking stands for, the other way: the marking a zone's own colours are shown as. */
const ZoneMarkings: Record<string, string> = {legs: 'stockings', tail_tip: 'tail_tip', head: 'mask', chest: 'bib', back: 'saddle'};

/** The older fields a painted wolf also keeps, so the places not yet painted (the sheet, the inspection card) and the
 *  words ("a grey wolf") show a wolf of its colours: its coat between dark and mid, its belly the light, its eyes, and
 *  a marking for each zone of its own that an older marking can stand for. */
export function olderLook(fur: Fur): {coat: string; gradientTint: string; eyes: string; markings: Array<{mask: string; color: string; opacity: number}>} {
    const hex = (c: Rgb) => `#${c.map(v => Math.round(v).toString(16).padStart(2, '0')).join('')}`;
    const body = (coat: FurCoat) => hex(mix(furRgb(coat[0]), furRgb(coat[1]), 0.5));
    const markings = Object.entries(fur.zones ?? {}).filter(([zone]) => ZoneMarkings[zone])
        .map(([zone, coat]) => ({mask: ZoneMarkings[zone], color: body(coat), opacity: 1}));
    return {coat: body(fur.coat), gradientTint: furHex(fur.coat[2]), eyes: eyeHex(fur.eyes), markings};
}
