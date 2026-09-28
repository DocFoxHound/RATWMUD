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
