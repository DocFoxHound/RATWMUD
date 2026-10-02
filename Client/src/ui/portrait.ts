// The wolf portrait on the character sheet, the inspection card and the creator (UI/SRatwWolfDoll.cpp): the
// species' painted sheet (Data/Portraits, four life stages in a 2×2 grid) recoloured with the chosen coat. Never used
// for the map's W token.
import {CoatColors} from './theme.ts';
import {isObject, type Json} from '../game/json.ts';
import {drawWolf} from './wolfArt.ts';

export interface Appearance {
    species: string;
    sex: string;
    stature: string;
    pattern: string;
    baseColor: number;
    gradientColor: number;
    markingColor: number;
    gradientAmount: number;
    patternAmount: number;
    // Phase 9 (optional): free colours ("#rrggbb"), eyes, build and layered markings (as the server checks them).
    coat?: string;
    gradientTint?: string;
    markingTint?: string;
    eyes?: string;
    build?: string;
    markings?: Array<{mask: string; color: string; opacity: number}>;
}

const hex = (v: unknown): v is string => typeof v === 'string' && /^#[0-9a-f]{6}$/.test(v);
export const MarkingMasks = ['socks', 'stockings', 'blaze', 'mask', 'cape', 'bib', 'belly', 'tail_tip', 'ear_tips', 'freckles', 'brindle',
    'merle', 'scar', 'eye_patches', 'saddle'];

const Species = ['timber', 'maned', 'arctic', 'red', 'ethiopian'];
const inPalette = (v: unknown): v is number => typeof v === 'number' && Number.isInteger(v) && v >= 0 && v < 8;
const amount = (v: unknown): v is number => typeof v === 'number' && Number.isFinite(v) && v >= 0 && v <= 1;

/** As the server checks it (Core/RatwAppearance.cpp): null for anything invalid. */
export function readAppearance(o: unknown): Appearance | null {
    if (!isObject(o)) return null;
    const a = o as Json;
    if (!Species.includes(a.species as string) || !['female', 'male'].includes(a.sex as string) ||
        !['short', 'average', 'tall'].includes(a.stature as string) ||
        !['solid', 'saddle', 'mantle', 'piebald'].includes(a.pattern as string) ||
        !inPalette(a.baseColor) || !inPalette(a.gradientColor) || !inPalette(a.markingColor) ||
        !amount(a.gradientAmount) || !amount(a.patternAmount)) return null;
    const out: Appearance = {species: a.species as string, sex: a.sex as string, stature: a.stature as string, pattern: a.pattern as string,
        baseColor: a.baseColor, gradientColor: a.gradientColor, markingColor: a.markingColor,
        gradientAmount: a.gradientAmount, patternAmount: a.patternAmount};
    for (const key of ['coat', 'gradientTint', 'markingTint', 'eyes'] as const) {
        if (a[key] === undefined) continue;
        if (!hex(a[key])) return null;
        out[key] = a[key] as string;
    }
    if (a.build !== undefined) {
        if (!['lean', 'average', 'heavy'].includes(a.build as string)) return null;
        out.build = a.build as string;
    }
    if (a.markings !== undefined) {
        if (!Array.isArray(a.markings) || a.markings.length > 6) return null;
        out.markings = [];
        for (const m of a.markings) {
            if (!isObject(m) || !MarkingMasks.includes(m.mask as string) || !hex(m.color) || !amount(m.opacity)) return null;
            out.markings.push({mask: m.mask as string, color: m.color as string, opacity: m.opacity as number});
        }
    }
    return out;
}

export type LifeStage = 'young' | 'adolescent' | 'adult' | 'old';
export function lifeStage(age: number): LifeStage {
    return age <= 12 ? 'young' : age <= 17 ? 'adolescent' : age <= 64 ? 'adult' : 'old';
}

/** Height at the shoulder, for the sheet (Core/RatwAppearance.cpp). */
export function shoulderHeightCm(a: Appearance, age: number): number {
    const baseline = a.species === 'maned' ? 90 : a.species === 'arctic' ? 72 : a.species === 'red' ? 66 : a.species === 'ethiopian' ? 60 : 76;
    const stature = a.stature === 'short' ? 0.88 : a.stature === 'tall' ? 1.12 : 1;
    const stage = lifeStage(age);
    const maturity = stage === 'young' ? 0.65 : stage === 'adolescent' ? 0.87 : stage === 'old' ? 0.96 : 1;
    return baseline * stature * maturity;
}

function smooth(a: number, b: number, v: number) {
    const t = Math.min(1, Math.max(0, (v - a) / (b - a)));
    return t * t * (3 - 2 * t);
}
function patch(x: number, y: number, cx: number, cy: number, rx: number, ry: number) {
    const dx = (x - cx) / rx, dy = (y - cy) / ry;
    return 1 - smooth(0.64, 1, dx * dx + dy * dy);
}
const coat = (i: number): [number, number, number] => {
    const c = CoatColors[i];
    return [((c >> 16) & 255) / 255, ((c >> 8) & 255) / 255, (c & 255) / 255];
};

/** Recolours one life stage of a sheet: RGBA in, RGBA out (the painted outlines, eyes and highlights kept). */
export function recolor(source: Uint8ClampedArray, sheetWidth: number, frameX: number, frameY: number, width: number, height: number,
    a: Appearance): Uint8ClampedArray {
    const out = new Uint8ClampedArray(width * height * 4);
    const base = coat(a.baseColor), gradient = coat(a.gradientColor), marking = coat(a.markingColor), highlight = coat(0);
    for (let y = 0; y < height; ++y)
        for (let x = 0; x < width; ++x) {
            const s = ((y + frameY) * sheetWidth + x + frameX) * 4, o = (y * width + x) * 4;
            const u = x / width, v = y / height;
            const l = (source[s] * 0.2126 + source[s + 1] * 0.7152 + source[s + 2] * 0.0722) / 255;
            const g = smooth(0.2, 0.88, v) * a.gradientAmount;
            let color = [0, 1, 2].map(i => base[i] + (gradient[i] - base[i]) * g);
            let mask = 0;
            if (a.pattern === 'saddle') mask = patch(u, v, 0.49, 0.44, 0.27, 0.14);
            else if (a.pattern === 'mantle') mask = Math.max(patch(u, v, 0.48, 0.42, 0.34, 0.18), patch(u, v, 0.27, 0.36, 0.11, 0.2));
            else if (a.pattern === 'piebald')
                mask = Math.max(patch(u, v, 0.4, 0.53, 0.11, 0.16), patch(u, v, 0.66, 0.57, 0.1, 0.19), patch(u, v, 0.22, 0.3, 0.065, 0.12));
            color = color.map((c, i) => c + (marking[i] - c) * mask * a.patternAmount);
            if (l < 0.17) color = [l * 0.7, l * 0.75, l * 0.78];
            else {
                const lift = smooth(0.76, 1, l) * 0.56;
                color = color.map((c, i) => c * (0.3 + l * 1.02) * (1 - lift) + highlight[i] * lift);
            }
            out[o] = color[0] * 255;
            out[o + 1] = color[1] * 255;
            out[o + 2] = color[2] * 255;
            out[o + 3] = source[s + 3];
        }
    return out;
}

/** Portraits for one page: sheets loaded once, each look recoloured once. */
export class Portraits {
    private made = new Map<string, HTMLCanvasElement>();
    onReady: () => void = () => {};

    /** The portrait: the stylised wolf (wolfArt.ts) in this coat and at this age, drawn once and kept. */
    get(appearance: Appearance, age: number): HTMLCanvasElement | null {
        const stage = lifeStage(Number.isFinite(age) ? Math.min(10000, Math.max(0, Math.trunc(age))) : 18);
        const key = JSON.stringify([appearance, stage]);
        const made = this.made.get(key);
        if (made) return made;
        if (typeof document === 'undefined') return null;
        const canvas = document.createElement('canvas');
        canvas.width = 500;
        canvas.height = 340;
        const c = canvas.getContext('2d');
        if (!c) return null;
        drawWolf(c, appearance, stage);
        if (this.made.size > 64) this.made.clear();
        this.made.set(key, canvas);
        return canvas;
    }

    failed(_species: string): boolean {
        return false;                      // Drawn in code: there is no sheet to fail to load.
    }
}

/**
 * Draws a portrait into a box: scaled to fit with room for the tallest stature, so stature genuinely changes height,
 * standing near the bottom. Returns false if there is nothing to draw (yet).
 */
export function drawPortrait(ctx: CanvasRenderingContext2D, portraits: Portraits, appearanceJson: unknown, age: number,
    x: number, y: number, w: number, h: number): boolean {
    const appearance = readAppearance(appearanceJson);
    if (!appearance) return false;
    const image = portraits.get(appearance, age);
    if (!image) return false;
    const stature = appearance.stature === 'short' ? 0.88 : appearance.stature === 'tall' ? 1.12 : 1;
    const s = Math.min(w / image.width, h / (image.height * 1.12));
    const dw = image.width * s, dh = image.height * s * stature;
    ctx.save();
    ctx.imageSmoothingEnabled = true;
    ctx.drawImage(image, x + (w - dw) / 2, y + h * 0.94 - dh * 0.9, dw, dh);
    ctx.restore();
    return true;
}
