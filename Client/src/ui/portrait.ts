// A wolf's portrait wherever the game shows one (Docs/Design/81-painted-wolves.md): the painted wolf (wolfPaint.ts) in
// that wolf's own fur, its face or its profile, or an uploaded picture where the viewer may see one. The appearance as
// the server checks it, and the height at the shoulder. Never used for the map's W token, nor for a hunt's game (its
// glyph).
import {isObject, type Json} from '../game/json.ts';
import {artCache} from './artwork.ts';
import {buildOf, Builds, furOf, readFur, type Build, type Fur} from './fur.ts';
import {PaintedWolves, WolfPaper, type Painting} from './wolfPaint.ts';

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
    // The painted look (doc 81; fur.ts), when chosen: palette ids. `build` is the size it is painted as (fur.ts buildOf).
    fur?: Fur;
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
        if (!['small', 'medium', 'large', 'lean', 'average', 'heavy'].includes(a.build as string)) return null;
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
    if (a.fur !== undefined) {
        const fur = readFur(a.fur);
        if (!fur) return null;
        out.fur = fur;
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

/** Which painting a portrait shows (doc 81): the face (one painting, every wolf's), or the profile of the wolf's size. */
export type PortraitView = 'face' | 'profile';

export interface PortraitOptions {
    view?: PortraitView;        // The profile unless said.
    paper?: boolean;            // On the paintings' paper (rounded), else on nothing: what is behind shows. Off unless said:
                                // the wolves are transparent and stand over the screen (the user's notes, 2026-11-14).
    radius?: number;            // The paper's corners, in pixels (a fraction of the box's shorter side by default).
    pad?: number;               // The margin round the painting, as a share of the box's shorter side (0.07).
    flip?: boolean;             // Facing right (the paintings face left): the left of a face-off.
}

/**
 * The wolves' portraits for one page (doc 81): the paintings (wolfPaint.ts) asked for at once, so they have arrived by
 * the first portrait, and each look painted once at the size it is shown and kept. `version` counts paintings arrived:
 * a place that drew before one did draws again when it changes (or listens).
 */
export class Portraits {
    readonly wolves = new PaintedWolves();
    version = 0;
    private listeners = new Set<() => void>();

    constructor() {
        this.wolves.onReady = () => {
            ++this.version;
            for (const f of this.listeners) f();
        };
        for (const painting of ['face', ...Builds] as const) this.wolves.aspect(painting);
    }

    /** Called as each painting arrives; returns a function that stops it. */
    listen(f: () => void): () => void {
        this.listeners.add(f);
        return () => this.listeners.delete(f);
    }
}

/** Where a wolf's painted look comes from: an appearance as the server sends it (its `fur`, or for a wolf saved before
 *  the painted wolves the look derived from what it had: fur.ts furOf), and the painting of its size. */
export function paintedLook(appearanceJson: unknown): {fur: Fur; build: Build} | null {
    const look = readAppearance(appearanceJson);
    return look ? {fur: furOf(look), build: buildOf(look)} : null;
}

/**
 * Draws a portrait into a box: an uploaded picture the viewer may see (doc 29, phase 9), square and centred; else the
 * painted wolf in its own fur (doc 81), its face or its profile, over what is behind (on its paper only if asked). The profiles share one frame, so a small
 * wolf stands smaller than a large one in boxes of one size. False if there is nothing to draw (yet): no appearance (a
 * hunt's game), or the paintings still on their way.
 */
export function drawPortrait(ctx: CanvasRenderingContext2D, portraits: Portraits, appearanceJson: unknown,
    x: number, y: number, w: number, h: number, artwork?: string, options: PortraitOptions = {}): boolean {
    const picture = artCache.get(artwork);
    if (picture) {
        const side = Math.min(w, h);
        ctx.save();
        ctx.imageSmoothingEnabled = true;
        ctx.drawImage(picture, x + (w - side) / 2, y + (h - side) / 2, side, side);
        ctx.restore();
        return true;
    }
    const look = paintedLook(appearanceJson);
    if (!look) return false;
    const painting: Painting = options.view === 'face' ? 'face' : look.build;
    const wolves = portraits.wolves;
    if (!wolves.aspect(painting)) return false;
    const short = Math.min(w, h), pad = short * (options.pad ?? 0.07);
    ctx.save();
    if (options.paper === true) {
        ctx.fillStyle = WolfPaper;
        ctx.beginPath();
        ctx.roundRect(x, y, w, h, options.radius ?? short * (painting === 'face' ? 0.14 : 0.06));
        ctx.fill();
    }
    if (options.flip) {
        ctx.translate(2 * x + w, 0);
        ctx.scale(-1, 1);
    }
    const drawn = wolves.draw(ctx, painting, look.fur, x + pad, y + pad, w - 2 * pad, h - 2 * pad);
    ctx.restore();
    return drawn;
}
