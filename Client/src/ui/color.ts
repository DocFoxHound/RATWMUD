// Colours as the Unreal client had them: linear-light RGBA (FLinearColor), made from sRGB hex values, so mixing and
// scaling look the same as they did. css() turns one back into sRGB for the canvas.

export interface Color {
    r: number;
    g: number;
    b: number;
    a: number;
}

const toLinear = (c: number) => (c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4));
const toSrgb = (c: number) => (c <= 0.0031308 ? c * 12.92 : 1.055 * Math.pow(c, 1 / 2.4) - 0.055);

/** An sRGB hex colour (0xRRGGBB) with an alpha, in linear light. */
export function rgb(hex: number, alpha = 1): Color {
    return {r: toLinear(((hex >> 16) & 255) / 255), g: toLinear(((hex >> 8) & 255) / 255), b: toLinear((hex & 255) / 255), a: alpha};
}

/** "#rrggbb" as rgb() takes it. */
export function hexColor(text: string, alpha = 1): Color {
    return rgb(parseInt(text.replace('#', ''), 16) || 0, alpha);
}

export const withAlpha = (c: Color, a: number): Color => ({r: c.r, g: c.g, b: c.b, a});
export const lerp = (a: Color, b: Color, t: number): Color =>
    ({r: a.r + (b.r - a.r) * t, g: a.g + (b.g - a.g) * t, b: a.b + (b.b - a.b) * t, a: a.a + (b.a - a.a) * t});
/** Every channel scaled, alpha included, as FLinearColor * float does. */
export const scale = (c: Color, f: number): Color => ({r: c.r * f, g: c.g * f, b: c.b * f, a: c.a * f});
export const transparent: Color = {r: 0, g: 0, b: 0, a: 0};

const byte = (v: number) => Math.round(Math.min(1, Math.max(0, toSrgb(Math.min(1, Math.max(0, v))))) * 255);

export function css(c: Color): string {
    const a = Math.min(1, Math.max(0, c.a));
    return `rgba(${byte(c.r)},${byte(c.g)},${byte(c.b)},${+a.toFixed(4)})`;
}

// Colours in words, for the creator's review (R1: players never see "#f4efe6"): the nearest named colour of a short
// list each for coats and eyes (the creator's own swatches all have one), by distance in sRGB weighted for the eye.
const CoatWords: Array<[string, number]> = [
    ['snow white', 0xf6f3ee], ['cream', 0xece2cc], ['ivory', 0xe1d9c6], ['pale fawn', 0xd6c7a8], ['fawn', 0xcfb184], ['sand', 0xbeaa84],
    ['honey', 0xd9b48a], ['tawny', 0xc9945e], ['ginger', 0xd98a52], ['rust', 0xb5733f], ['russet', 0x8f4f2a], ['light brown', 0xa3865f],
    ['brown', 0x7a6142], ['dark brown', 0x52412d], ['deep brown', 0x2a221b], ['black', 0x161412], ['pale grey', 0xc8ccca],
    ['silver grey', 0xadb3b2], ['grey', 0x8e9493], ['ash grey', 0x777d7b], ['dark grey', 0x5f6563], ['charcoal', 0x303534],
    ['stone', 0x8e8271], ['grey-brown', 0x6b5a48]];
const EyeWords: Array<[string, number]> = [
    ['amber', 0xd9a441], ['copper', 0xc8902e], ['gold', 0xe3c35a], ['hazel', 0xa0702a], ['brown', 0x7a5228], ['dark brown', 0x5a3a1e],
    ['olive green', 0x8a9a3a], ['green', 0x6a8a4a], ['blue', 0x5f9fd0], ['pale blue', 0x8fb8d8], ['grey', 0xa9b3b8], ['near black', 0x3a2a1a]];

/** "#c9945e" as words ("tawny"); text that isn't a colour code is kept as it is (an older draft's "amber"). */
export function colourWords(value: string, kind: 'coat' | 'eyes' = 'coat'): string {
    const m = /^#?([0-9a-f]{6})$/i.exec(value.trim());
    if (!m) return value;
    const hex = parseInt(m[1], 16);
    const [r, g, b] = [(hex >> 16) & 255, (hex >> 8) & 255, hex & 255];
    let best = '', far = Infinity;
    for (const [name, h] of kind === 'eyes' ? EyeWords : CoatWords) {
        const dr = r - ((h >> 16) & 255), dg = g - ((h >> 8) & 255), db = b - (h & 255);
        const d = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
        if (d < far) [best, far] = [name, d];
    }
    return best;
}
