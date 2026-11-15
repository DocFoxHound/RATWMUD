// Night and the lights in it (the user, 2026-11-15: "Shallow Water tiles appear to be glowing ... a recurring problem").
//
// The ground is drawn three ways: block-texture characters (grass, cobbles, roofs, walls: Data/Visuals/blockTexture.ts),
// whole glyphs (water, stairs, boulders, slopes, doors, features), and lines and washes (height edges, the floor's
// shading). Each used to be darkened at night its own way: the textures by their alpha (to 55%), the glyphs only by a
// scale of their linear-light colour (to 73% linear, which is still about 87% as bright to the eye) and a pull toward a
// light moon blue that lifted dark colours; the edge lines barely, the floor's light washes not at all. As the night sky
// behind them went dark, everything drawn as a glyph or a line stayed nearly as bright as by day and read as a light.
//
// Here is the one rule for all of them: under a sky of darkness `dark` (1 - illumination) every colour keeps
// `nightLevel(dark)` of its brightness as the eye sees it (sRGB), cooled toward moonlight without brightening. Only the
// light sources below are exempt, and they light the ground round them.
import {clamp} from './json.ts';
import {rgb, type Color} from '../ui/color.ts';

/** How bright a colour stays, to the eye, under a full night sky (darkness 1). */
export const NightFloor = 0.4;

/** What is left of a colour's brightness (as seen, 0..1) under a sky of darkness `dark`. */
export const nightLevel = (dark: number) => 1 - clamp(dark, 0, 1) * (1 - NightFloor);

const luma = (c: Color) => 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
const Moon = rgb(0x8195ad);
const MoonLuma = luma(Moon);

/**
 * A colour under a sky of darkness `dark`: cooled toward a moonlit blue-grey of its own brightness (so the blue shift
 * never lifts it), then dimmed to nightLevel(dark) as the eye sees it. Colours are linear light (ui/color.ts), so the
 * seen level is raised to 2.2 for the channels; alpha is kept.
 */
export function nightColor(c: Color, dark: number): Color {
    const d = clamp(dark, 0, 1);
    if (d <= 0) return c;
    const y = luma(c), k = y / MoonLuma, t = d * 0.3;
    const f = Math.pow(nightLevel(d), 2.2);
    return {r: (c.r + (Moon.r * k - c.r) * t) * f, g: (c.g + (Moon.g * k - c.g) * t) * f, b: (c.b + (Moon.b * k - c.b) * t) * f, a: c.a};
}

/** A thing on the map that gives light: how far its pool reaches (tiles), its colour, and how strongly it lights. */
export interface LightSource {
    radius: number;
    color: Color;
    strength: number;
}

/**
 * The terrain that gives light, by code (Data/Terrain/terrain.json). Nothing else may be brighter at night than the
 * night allows (lights.test.ts walks every terrain kind to hold to that).
 */
export const LightSources: Readonly<Record<string, LightSource>> = {
    "'": {radius: 4.5, color: rgb(0xffc47a), strength: 1},       // Street lamp (doc 59; Ridgemere 3's streets).
    i: {radius: 3.5, color: rgb(0xff9f4f), strength: 0.9},        // Brazier.
    '9': {radius: 3.5, color: rgb(0xff9446), strength: 0.85},     // Campfire.
    h: {radius: 2.5, color: rgb(0xff9a52), strength: 0.7},        // Hearth.
    '{': {radius: 2.5, color: rgb(0xff8a46), strength: 0.7},      // Furnace.
};
export const lightOf = (code: string): LightSource | undefined => LightSources[code];

/**
 * How lit the lamps are under a sky of darkness `dark`: out by day, coming on through dusk, full at night. (The server
 * sends no lit or unlit state for a lamp: when it does, a lamplighter's round, it belongs here.)
 */
export function lampsLit(dark: number): number {
    const t = clamp((dark - 0.3) / 0.3, 0, 1);
    return t * t * (3 - 2 * t);
}

/** How strongly a light's pool lights a tile `distance` tiles from it (1 at the light, 0 at its reach), smoothly. */
export function poolAt(distance: number, radius: number): number {
    if (distance >= radius) return 0;
    const u = 1 - (distance / radius) * (distance / radius);
    return u * u;
}
