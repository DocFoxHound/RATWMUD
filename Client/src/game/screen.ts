// The screen (Docs/Design/80-the-whole-screen.md): the player is shown every tile on its screen, whatever its wolf's
// line of sight, the light or the weather. "The screen" is a rectangle of tiles round the wolf, the same one the
// server works out (Core/RatwScreen.h, which holds these numbers too: screen.test.ts reads them there): everything in
// it is sent and drawn as seen, and the map never shows ground outside it.

/** Tiles shown each side of the wolf's own tile: 193 across, 113 down. */
export const ScreenHalfWidth = 96, ScreenHalfHeight = 56;
/** How many of those, at each side, the map keeps off its edge: the wolf walks ahead of what the server last sent. */
export const ScreenMargin = 4;

/** Tiles x0..x1 by y0..y1, both ends in, in the cell's own coordinates (below 0 and past its size in the band). */
export interface ScreenRect {
    x0: number;
    y0: number;
    x1: number;
    y1: number;
}

/** Along one axis: `half` tiles each side of the wolf's own, moved to lie within the ground there is (`low`..`high`). */
function span(at: number, half: number, low: number, high: number): [number, number] {
    const first = Math.max(low, Math.min(Math.floor(at) - half, high - 2 * half)) + 0;   // (+ 0: never -0.)
    return [first, Math.min(high, first + 2 * half)];
}

/** The screen of a wolf at (x, y) in a cell `width` x `height`, with `band` tiles of ground shown round the cell. */
export function screenRect(x: number, y: number, width: number, height: number, band: number): ScreenRect {
    const [x0, x1] = span(x, ScreenHalfWidth, -band, width + band - 1);
    const [y0, y1] = span(y, ScreenHalfHeight, -band, height + band - 1);
    return {x0, y0, x1, y1};
}

export function onScreen(r: ScreenRect, x: number, y: number): boolean {
    return x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1;
}

/** The smallest a tile may be drawn, in pixels, on a map `width` x `height`: so it never shows more than the screen. */
export function screenTile(width: number, height: number): number {
    return Math.max(width / (2 * (ScreenHalfWidth - ScreenMargin)), height / (2 * (ScreenHalfHeight - ScreenMargin)));
}

/** A row of glyphs as the map shades it: '2' on the screen (columns `from`..`to` of it), '1' remembered, '0' unknown. */
export function shadeRow(glyphs: string, from: number, to: number): string {
    const dim = (part: string) => part.replace(/[^ ]/g, '1').replace(/ /g, '0');
    const a = Math.max(0, Math.min(glyphs.length, from)), b = Math.max(a, Math.min(glyphs.length, to + 1));
    return dim(glyphs.slice(0, a)) + glyphs.slice(a, b).replace(/[^ ]/g, '2').replace(/ /g, '0') + dim(glyphs.slice(b));
}
