// The wolf portrait on the character sheet, the inspection card and the creator (UI/SRatwWolfDoll.cpp): the
// species' painted sheet (Data/Portraits, four life stages in a 2×2 grid) recoloured with the chosen coat. Never used
// for the map's W token.
import {CoatColors} from './theme.ts';
import {isObject, type Json} from '../game/json.ts';

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
}

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
    return {species: a.species as string, sex: a.sex as string, stature: a.stature as string, pattern: a.pattern as string,
        baseColor: a.baseColor, gradientColor: a.gradientColor, markingColor: a.markingColor,
        gradientAmount: a.gradientAmount, patternAmount: a.patternAmount};
}

export type LifeStage = 'young' | 'adolescent' | 'adult' | 'old';
export function lifeStage(age: number): LifeStage {
    return age <= 12 ? 'young' : age <= 17 ? 'adolescent' : age <= 64 ? 'adult' : 'old';
}
const StageFrame: Record<LifeStage, number> = {young: 0, adolescent: 1, adult: 2, old: 3};

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

interface Sheet {
    width: number;
    height: number;
    pixels: Uint8ClampedArray;
}

/** Portraits for one page: sheets loaded once, each look recoloured once. */
export class Portraits {
    private sheets = new Map<string, Sheet | 'loading' | 'failed'>();
    private made = new Map<string, HTMLCanvasElement>();
    onReady: () => void = () => {};

    /** The portrait, or null while its sheet loads (or if it can't). */
    get(appearance: Appearance, age: number): HTMLCanvasElement | null {
        const stage = lifeStage(Number.isFinite(age) ? Math.min(10000, Math.max(0, Math.trunc(age))) : 18);
        const key = JSON.stringify([appearance, stage]);
        const made = this.made.get(key);
        if (made) return made;
        const sheet = this.sheet(appearance.species);
        if (!sheet) return null;
        const w = sheet.width / 2, h = sheet.height / 2, frame = StageFrame[stage];
        const canvas = document.createElement('canvas');
        canvas.width = w;
        canvas.height = h;
        const pixels = recolor(sheet.pixels, sheet.width, (frame % 2) * w, Math.floor(frame / 2) * h, w, h, appearance);
        canvas.getContext('2d')!.putImageData(new ImageData(pixels as Uint8ClampedArray<ArrayBuffer>, w, h), 0, 0);
        if (this.made.size > 32) this.made.clear();
        this.made.set(key, canvas);
        return canvas;
    }

    failed(species: string): boolean {
        return this.sheets.get(species) === 'failed';
    }

    private sheet(species: string): Sheet | null {
        const known = this.sheets.get(species);
        if (known && typeof known === 'object') return known;
        if (known) return null;
        this.sheets.set(species, 'loading');
        const image = new Image();
        image.onload = () => {
            const valid = image.width >= 4 && image.height >= 4 && image.width <= 4096 && image.height <= 4096 &&
                image.width % 2 === 0 && image.height % 2 === 0;
            if (!valid) {
                this.sheets.set(species, 'failed');
                return;
            }
            const canvas = document.createElement('canvas');
            canvas.width = image.width;
            canvas.height = image.height;
            const c = canvas.getContext('2d', {willReadFrequently: true})!;
            c.drawImage(image, 0, 0);
            this.sheets.set(species, {width: image.width, height: image.height, pixels: c.getImageData(0, 0, image.width, image.height).data});
            this.onReady();
        };
        image.onerror = () => this.sheets.set(species, 'failed');
        // Only the five known species, never a path from the server.
        image.src = `./portraits/${species}.png`;
        return null;
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
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(image, x + (w - dw) / 2, y + h * 0.94 - dh * 0.9, dw, dh);
    ctx.restore();
    return true;
}
