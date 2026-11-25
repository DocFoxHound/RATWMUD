// The layered wolf dolls' colours (Docs/Design/81-painted-wolves.md): the palette (Data/Wolves/palette.json,
// generated into furPalette.generated.ts), a wolf's fur as it is kept (palette ids, checked as
// Core/RatwAppearance.cpp checks them), the layered doll compositor (46 flat-coloured layers + multiply shadow +
// lineart), the face portrait ramp, and the derived look an older wolf or NPC is shown in.
// No page needed: wolfPaint.ts loads the assets and draws.
import {DollLayers, EyeColours, FurColours, FurZones, ReadyCoats, type DollLayer, type ReadyCoat} from './furPalette.generated.ts';
import {CoatColors} from './theme.ts';
import {isObject} from '../game/json.ts';

export {DollLayers, EyeColours, FurColours, FurZones, ReadyCoats, type DollLayer, type ReadyCoat};

/** Three fur palette ids (legacy coat / face / legacy zones: dark, mid, light). */
export type FurCoat3 = [string, string, string];
/** Five fur palette ids (layered doll coat: darkest, dark, mid, light, lightest). */
export type FurCoat5 = [string, string, string, string, string];
/** A coat may be 5 shades (layered dolls) or 3 shades (legacy saved wolves). */
export type FurCoat = FurCoat3 | FurCoat5;

export const ShadeNames = ['Darkest', 'Dark', 'Mid', 'Light', 'Lightest'] as const;
/** Body regions for per-layer customisation in the Character Editor (Eyes has its own tab). */
export const DollLayerGroups = ['Head', 'Neck', 'Body', 'Front legs', 'Hind legs', 'Tail', 'Claws'] as const;

export interface Fur {
    coat: FurCoat;
    eyes: string;                          // An eye palette id.
    claws?: string;                        // Optional claw colour (a fur palette id).
    zones?: Record<string, FurCoat3>;      // Legacy 8-zone overrides; kept for older wolves.
    layers?: Record<string, string>;       // Per-layer overrides (doll layer id -> fur palette id).
}

/** Which size a wolf is painted as (the appearance's `build`), smallest first. */
export type Build = 'small' | 'medium' | 'large';
export const Builds: readonly Build[] = ['small', 'medium', 'large'];
export const BuildNames: Record<Build, string> = {small: 'Small', medium: 'Average', large: 'Tall'};
/** The stature that goes with a size: the creator writes both, so the height and the words agree with the doll. */
export const BuildStature: Record<Build, string> = {small: 'short', medium: 'average', large: 'tall'};
/** What `build` held before the sizes: a wolf saved with one keeps the size that took its place. */
const OlderBuilds: Record<string, Build> = {lean: 'large', heavy: 'medium', average: 'small'};

export type Sex = 'female' | 'male';
export const Sexes: readonly Sex[] = ['female', 'male'];
export type DollKey = `${Sex}-${Build}`;
export const AllDolls: readonly DollKey[] = [
    'female-small', 'female-medium', 'female-large',
    'male-small', 'male-medium', 'male-large',
];

export type Rgb = [number, number, number];

/** Default claw palette id when a wolf has no explicit claws colour set. */
export const DefaultClawColour = 'straw';
/** Neutral multiply shadow luminance (47_*_SHADOW.png is neutral grey 131/255). */
export const ShadowLuma = 131 / 255;

const furById = new Map(FurColours.map(c => [c.id, c]));
const eyeById = new Map(EyeColours.map(c => [c.id, c]));
const zoneIndex = new Map(FurZones.map((z, i) => [z.id, i]));
const layerById = new Map(DollLayers.map((l, i) => [l.id, {layer: l, index: i + 1}]));

export const isFurColour = (id: unknown): id is string => typeof id === 'string' && furById.has(id);
export const isEyeColour = (id: unknown): id is string => typeof id === 'string' && eyeById.has(id);
export const isFurZone = (id: unknown): id is string => typeof id === 'string' && zoneIndex.has(id);
export const isFurLayer = (id: unknown): id is string => typeof id === 'string' && id !== 'eyes' && layerById.has(id);
export const furName = (id: string) => furById.get(id)?.name ?? '';
export const eyeName = (id: string) => eyeById.get(id)?.name ?? '';
export const zoneName = (id: string) => FurZones[zoneIndex.get(id) ?? -1]?.name ?? '';
export const layerName = (id: string) => layerById.get(id)?.layer.name ?? '';

export function hexRgb(hex: string): Rgb {
    const n = parseInt(hex.slice(1), 16);
    return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}
export const furRgb = (id: string): Rgb => hexRgb(furById.get(id)?.hex ?? '#000000');
export const eyeRgb = (id: string): Rgb => hexRgb(eyeById.get(id)?.hex ?? '#000000');
export const furHex = (id: string) => furById.get(id)?.hex ?? '#000000';
export const eyeHex = (id: string) => eyeById.get(id)?.hex ?? '#000000';

function readCoat3(v: unknown): FurCoat3 | null {
    return Array.isArray(v) && v.length === 3 && v.every(isFurColour) ? [v[0], v[1], v[2]] : null;
}

function readCoat(v: unknown): FurCoat | null {
    if (!Array.isArray(v) || !v.every(isFurColour)) return null;
    if (v.length === 3) return [v[0], v[1], v[2]];
    if (v.length === 5) return [v[0], v[1], v[2], v[3], v[4]];
    return null;
}

/** A wolf's fur as the server checks it (wire::readAppearance): null for anything incomplete or off the palette. */
export function readFur(v: unknown): Fur | null {
    if (!isObject(v)) return null;
    const keys = Object.keys(v);
    if (keys.some(k => k !== 'coat' && k !== 'eyes' && k !== 'claws' && k !== 'zones' && k !== 'layers')) return null;
    const coat = readCoat(v.coat);
    if (!coat || !isEyeColour(v.eyes)) return null;
    const out: Fur = {coat, eyes: v.eyes};
    if (v.claws !== undefined) {
        if (!isFurColour(v.claws)) return null;
        out.claws = v.claws;
    }
    if (v.zones !== undefined) {
        if (!isObject(v.zones) || !Object.keys(v.zones).length) return null;
        out.zones = {};
        for (const [zone, colours] of Object.entries(v.zones)) {
            const own = readCoat3(colours);
            if (!isFurZone(zone) || !own) return null;
            out.zones[zone] = own;
        }
    }
    if (v.layers !== undefined) {
        if (!isObject(v.layers) || !Object.keys(v.layers).length) return null;
        out.layers = {};
        for (const [layer, colour] of Object.entries(v.layers)) {
            if (!isFurLayer(layer) || !isFurColour(colour)) return null;
            out.layers[layer] = colour;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------------------------- Coat shade expansion

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

/** Expand any coat (3 or 5 shades) into the 5 doll shades [darkest, dark, mid, light, lightest]. */
export function coatShades(coat: FurCoat): FurCoat5 {
    if (coat.length === 5) return [coat[0], coat[1], coat[2], coat[3], coat[4]];
    const d = furRgb(coat[0]), m = furRgb(coat[1]), l = furRgb(coat[2]);
    return [coat[0], nearestFur(mix(d, m, 0.45)), coat[1], nearestFur(mix(m, l, 0.5)), coat[2]];
}

/** Reduce any coat (3 or 5 shades) to 3 tones [dark, mid, light] for the legacy face ramp. */
export function coatThree(coat: FurCoat): FurCoat3 {
    return coat.length === 5 ? [coat[0], coat[2], coat[4]] : [coat[0], coat[1], coat[2]];
}

/** The claw colour a wolf wears (its own choice, else DefaultClawColour). */
export function clawColour(fur: Fur): string {
    return fur.claws && isFurColour(fur.claws) ? fur.claws : DefaultClawColour;
}

/** The default colour a doll layer inherits from the coat/claws/zones before any per-layer override. */
export function baseLayerColour(fur: Fur, layerId: string): string {
    const entry = layerById.get(layerId);
    if (!entry) return fur.coat[0];
    const l = entry.layer;
    if (l.shade === 'eyes') return fur.eyes;
    if (l.shade === 'claws') return clawColour(fur);
    if (l.zone && fur.zones?.[l.zone]) return coatShades(fur.zones[l.zone])[l.shade];
    return coatShades(fur.coat)[l.shade];
}

/** The resolved colour id for a doll layer (per-layer override if set, else baseLayerColour). */
export function layerColour(fur: Fur, layerId: string): string {
    if (layerId === 'eyes') return fur.eyes;
    return fur.layers?.[layerId] ?? baseLayerColour(fur, layerId);
}

/** A 47-entry RGB lookup table (index 0 unused; 1..46 = DollLayers in stack order) for paintDoll. */
export function dollColours(fur: Fur): Uint8Array {
    const lut = new Uint8Array(47 * 3);
    for (let i = 0; i < DollLayers.length; ++i) {
        const l = DollLayers[i];
        const rgb = l.id === 'eyes' ? eyeRgb(fur.eyes) : furRgb(layerColour(fur, l.id));
        const off = (i + 1) * 3;
        lut[off] = rgb[0];
        lut[off + 1] = rgb[1];
        lut[off + 2] = rgb[2];
    }
    return lut;
}

// ---------------------------------------------------------------------------------------------- Layered doll renderer

/** Packed doll source buffers (all three read from opaque RGB PNGs, tools/doll_prep.py). */
export interface DollSource {
    width: number;
    height: number;
    idx: Uint8ClampedArray;         // R=i0, G=i1, B=i2 (1..46; 0 = empty)
    wt: Uint8ClampedArray;          // R=q0, G=q1, B=baseAlpha (q2 = 255 - q0 - q1)
    ink: Uint8ClampedArray;         // R=shadowAlpha, G=lineAlpha, B=lineLuma
}

/**
 * Composites a packed layered wolf doll into an RGBA pixel buffer: blends the top-3 contributing colour layers per
 * pixel, multiplies the neutral shadow layer, and composites the lineart (+ catchlights) on top.
 */
export function paintDoll(source: DollSource, lut: Uint8Array, out: Uint8ClampedArray): void {
    const {idx, wt, ink} = source;
    const count = source.width * source.height;
    const shadowScale = (1 - ShadowLuma) / 255;
    for (let i = 0; i < count; ++i) {
        const p = i * 4;
        const baseA = wt[p + 2];
        const lineA = ink[p + 1];
        if (!baseA && !lineA) {
            out[p] = out[p + 1] = out[p + 2] = out[p + 3] = 0;
            continue;
        }
        let r = 0, g = 0, b = 0;
        if (baseA) {
            const i0 = idx[p] * 3, i1 = idx[p + 1] * 3, i2 = idx[p + 2] * 3;
            const q0 = wt[p], q1 = wt[p + 1], q2 = 255 - q0 - q1;
            r = (q0 * lut[i0] + q1 * lut[i1] + q2 * lut[i2]) / 255;
            g = (q0 * lut[i0 + 1] + q1 * lut[i1 + 1] + q2 * lut[i2 + 1]) / 255;
            b = (q0 * lut[i0 + 2] + q1 * lut[i1 + 2] + q2 * lut[i2 + 2]) / 255;
            const sa = ink[p];
            if (sa) {
                const mult = 1 - sa * shadowScale;
                r *= mult;
                g *= mult;
                b *= mult;
            }
        }
        if (lineA) {
            const la = lineA / 255;
            const ba = baseA / 255;
            const outA = la + ba * (1 - la);
            const ll = ink[p + 2];
            const k = ba * (1 - la);
            out[p] = (ll * la + r * k) / outA;
            out[p + 1] = (ll * la + g * k) / outA;
            out[p + 2] = (ll * la + b * k) / outA;
            out[p + 3] = Math.round(outA * 255);
        } else {
            out[p] = r;
            out[p + 1] = g;
            out[p + 2] = b;
            out[p + 3] = baseA;
        }
    }
}

// ---------------------------------------------------------------------------------------------- The face ramp (kept until new faces arrive)

/** Where the ramp's colours lie on the face painting's luminance: near black, dark, mid, light, near white. */
export const RampStops = [0, 38, 118, 212, 255];

/** The face painting's luminance (0-255) to a colour: 256 colours, three bytes each. */
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

/** One ramp per legacy fur zone (used by the face portrait): picks up zone or head/muzzle layer overrides. */
export function zoneRamps(fur: Fur): Uint8Array[] {
    const three = coatThree(fur.coat);
    const coat = furRamp(furRgb(three[0]), furRgb(three[1]), furRgb(three[2]));
    return FurZones.map(z => {
        const own = fur.zones?.[z.id];
        if (own) return furRamp(furRgb(own[0]), furRgb(own[1]), furRgb(own[2]));
        if (fur.layers) {
            if (z.id === 'head' && (fur.layers.head_accents || fur.layers.upr_head || fur.layers.inner_ear)) {
                return furRamp(furRgb(layerColour(fur, 'head_accents')), furRgb(layerColour(fur, 'upr_head')), furRgb(layerColour(fur, 'inner_ear')));
            }
            if (z.id === 'muzzle' && (fur.layers.cheeks_accent || fur.layers.muzzle || fur.layers.cheeks_front || fur.layers.chin_jaw)) {
                return furRamp(furRgb(layerColour(fur, 'cheeks_accent')), furRgb(layerColour(fur, 'muzzle')), furRgb(layerColour(fur, 'cheeks_front')));
            }
        }
        return coat;
    });
}

/** A face cut-out and its map at one size (RGBA, four bytes a pixel). */
export interface FurSource {
    width: number;
    height: number;
    picture: Uint8ClampedArray;
    map: Uint8ClampedArray;
}

/** Paints the face cut-out in a wolf's fur (used until new face art replaces face.png). */
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

/** One colour as a 3-shade coat: itself for mid, its shadow and pale for dark and light. */
export function coatOf(colour: Rgb): FurCoat3 {
    return [nearestFur(mix(colour, [0, 0, 0], 0.5)), nearestFur(colour), nearestFur(mix(colour, [255, 255, 255], 0.6))];
}

/** One colour as a 5-shade layered coat: darkest through lightest. */
export function coat5Of(colour: Rgb): FurCoat5 {
    return [
        nearestFur(mix(colour, [0, 0, 0], 0.55)),
        nearestFur(mix(colour, [0, 0, 0], 0.25)),
        nearestFur(colour),
        nearestFur(mix(colour, [255, 255, 255], 0.3)),
        nearestFur(mix(colour, [255, 255, 255], 0.65)),
    ];
}

/** Which zone an older marking is nearest (only those whose names say so; the rest have no zone). */
export const MarkingZones: Record<string, string> = {socks: 'legs', stockings: 'legs', tail_tip: 'tail_tip', mask: 'head', bib: 'chest',
    belly: 'chest', saddle: 'back', cape: 'back'};

/** What furOf, buildOf and sexOf read of an appearance (portrait.ts's Appearance has these). */
export interface OlderLook {
    species: string;
    sex?: string;
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

/** Which sex doll a wolf uses (defaults to 'male' when omitted). */
export function sexOf(a: OlderLook): Sex {
    return a.sex === 'female' ? 'female' : 'male';
}

/** Which size a wolf is painted as: its build (an older build's name as the size that took its place); with
 *  none, its stature (short, average, tall). */
export function buildOf(a: OlderLook): Build {
    if (a.build === 'small' || a.build === 'medium' || a.build === 'large') return a.build;
    if (a.build && OlderBuilds[a.build]) return OlderBuilds[a.build];
    return a.stature === 'tall' ? 'large' : a.stature === 'short' ? 'small' : 'medium';
}

/** The exact doll key (`<sex>-<build>`) for an appearance. */
export function dollOf(a: OlderLook): DollKey {
    return `${sexOf(a)}-${buildOf(a)}`;
}

/**
 * A wolf's fur: what it chose, or for an older wolf / NPC (no `fur` kept) the nearest natural 5-shade look derived
 * from its coat colour, gradient, pattern and markings. Deterministic: the same older look always gives the same fur.
 */
export function furOf(a: OlderLook): Fur {
    if (a.fur) return a.fur;
    const palette = (i: number): Rgb => hexRgb(`#${(CoatColors[i] ?? CoatColors[3]).toString(16).padStart(6, '0')}`);
    const base = a.coat ? hexRgb(a.coat) : palette(a.baseColor);
    const coat = coat5Of(base);
    const coat3 = coatThree(coat);
    const zones: Record<string, FurCoat3> = {};
    const put = (zone: string, colour: Rgb) => {
        const own = coatOf(colour);
        if (own.join() !== coat3.join()) zones[zone] = own;
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
    const fur: Fur = {coat, eyes: a.eyes ? nearestEye(a.eyes) : 'amber', claws: DefaultClawColour};
    if (Object.keys(zones).length) fur.zones = zones;
    return fur;
}

/** The zone an older marking stands for, the other way: the marking a zone's own colours are shown as. */
const ZoneMarkings: Record<string, string> = {legs: 'stockings', tail_tip: 'tail_tip', head: 'mask', chest: 'bib', back: 'saddle'};

/** The older fields a wolf also keeps, so descriptive text ("a grey wolf") matches its chosen colours. */
export function olderLook(fur: Fur): {coat: string; gradientTint: string; eyes: string; markings: Array<{mask: string; color: string; opacity: number}>} {
    const hex = (c: Rgb) => `#${c.map(v => Math.round(v).toString(16).padStart(2, '0')).join('')}`;
    const shades = coatShades(fur.coat);
    const body3 = (coat: FurCoat3) => hex(mix(furRgb(coat[0]), furRgb(coat[1]), 0.5));
    const markings: Array<{mask: string; color: string; opacity: number}> = [];
    for (const [zone, coat] of Object.entries(fur.zones ?? {})) {
        if (ZoneMarkings[zone]) markings.push({mask: ZoneMarkings[zone], color: body3(coat), opacity: 1});
    }
    if (!markings.length && fur.layers) {
        const seen = new Set<string>();
        const add = (mask: string, colour: string) => {
            if (!seen.has(mask)) {
                seen.add(mask);
                markings.push({mask, color: furHex(colour), opacity: 1});
            }
        };
        if (fur.layers.fore_socks || fur.layers.hind_socks) add('socks', fur.layers.fore_socks ?? fur.layers.hind_socks);
        if (fur.layers.tail_tip) add('tail_tip', fur.layers.tail_tip);
        if (fur.layers.back_accent) add('saddle', fur.layers.back_accent);
        if (fur.layers.belly || fur.layers.neck_under) add('belly', fur.layers.belly ?? fur.layers.neck_under);
        if (fur.layers.head_accents || fur.layers.upr_head) add('mask', fur.layers.head_accents ?? fur.layers.upr_head);
    }
    return {
        coat: hex(mix(furRgb(shades[1]), furRgb(shades[2]), 0.5)),
        gradientTint: furHex(shades[4]),
        eyes: eyeHex(fur.eyes),
        markings: markings.slice(0, 6),
    };
}
