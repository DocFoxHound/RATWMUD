// Drawing ground, cell by cell, for maps of any size. Each cell's ground is cached as an image at a few pixels per
// tile and redrawn only where its rows changed; only the cells in view are drawn. Zoomed in close, glyphs are drawn
// straight onto the canvas instead. Each tile is drawn from the terrain catalog: its Unicode glyph (or plain ASCII
// fallback) in its fg colour over its bg. Shared by Atlas and the Dungeon Master.
import type {AnyCell} from '../model/model.mjs';
import {glyphInfo} from './glyphs';
import {drawnGlyph, glyphRender, GLYPH_STACK} from './glyphFont';
import type {Surface} from './surface';
import {heightGrid, shadeAt, type Camera} from './elevation';
import {DETAIL_FROM, groundPending, PREVIEW_STEP, previewOf, want} from './lazyGround';

export const DIRECT_ABOVE = 15;          // Above this many screen pixels per tile, draw glyphs directly (crisp).
export const FONT = GLYPH_STACK;
const CACHE_PIXELS = 48_000_000;         // Cached images kept, in pixels; the least recently drawn go first.
export type {Camera};

const swatches = new Map<string, string>();
/** A tile too small for its glyph shows the glyph's colour mixed into its background, so the map still reads zoomed out
 *  (backgrounds alone are dark by design: glyphs are meant to sit on them). */
export function swatch(info: {bg: string; fg: string}): string {
    const key = info.bg + info.fg;
    let colour = swatches.get(key);
    if (!colour) {
        const mix = (i: number) => Math.round(parseInt(info.bg.slice(i, i + 2), 16) * .4 + parseInt(info.fg.slice(i, i + 2), 16) * .6);
        colour = `rgb(${mix(1)}, ${mix(3)}, ${mix(5)})`;
        swatches.set(key, colour);
    }
    return colour;
}

/** One tile: background, then (with shading) the elevation tint and hillshade of tile x, y of `cell`, then the glyph when it fits. */
export function drawTile(g: CanvasRenderingContext2D, cell: AnyCell, x: number, y: number, px: number, py: number, s: number, shade: boolean) {
    const glyph = cell.terrain[y][x], info = glyphInfo(glyph);
    const glyphs = s >= 6 && glyphRender().ready;
    g.fillStyle = glyphs ? info.bg : swatch(info); g.fillRect(px, py, s + .5, s + .5);
    if (shade) {
        const [tint, light] = shadeAt(cell, heightGrid(cell), x, y);
        if (tint) { g.fillStyle = tint; g.fillRect(px, py, s + .5, s + .5); }
        if (light) { g.fillStyle = light; g.fillRect(px, py, s + .5, s + .5); }
    }
    if (glyphs) drawGlyph(g, info, px, py, s);
}
/** A catalog tile's glyph centred in the s-pixel tile at px, py (the font set by glyphFont); nothing until the font has loaded. */
export function drawGlyph(g: CanvasRenderingContext2D, info: {glyph: string; ascii: string; fg: string}, px: number, py: number, s: number) {
    const r = glyphRender();
    if (!r.ready) return;
    g.fillStyle = info.fg; g.fillText(drawnGlyph(info, r.ascii), px + s / 2, py + s * .55);
}
/** Sets the glyph font for s-pixel tiles, centred. */
export const glyphFont = (g: CanvasRenderingContext2D, s: number) => { g.font = `${Math.floor(s * .72)}px ${FONT}`; g.textAlign = 'center'; g.textBaseline = 'middle'; };
const font = glyphFont;

interface Cached { terrain: string[]; heights: Record<string, number>; canvas: HTMLCanvasElement; pixels: number }
const cache = new Map<string, Cached>();
let cachedPixels = 0, cachedVersion = -1;

/** The cell's ground as an image at `px` pixels per tile, reusing (and patching) the last one drawn. */
function image(cell: AnyCell, px: number, tint: boolean): HTMLCanvasElement {
    const version = glyphRender().version;
    if (version !== cachedVersion) { cache.clear(); cachedPixels = 0; cachedVersion = version; }   // Font loaded, or ASCII toggled.
    const key = `${cell.id}|${px}|${tint ? 1 : 0}`;
    const old = cache.get(key);
    if (old) cache.delete(key);                                   // Re-inserted below: most recently used.
    if (old && old.terrain === cell.terrain && old.heights === cell.heights) { cache.set(key, old); return old.canvas; }
    let entry: Cached;
    if (old && old.canvas.width === cell.width * px && old.canvas.height === cell.height * px && (!tint || old.heights === cell.heights)) {
        const g = old.canvas.getContext('2d')!; font(g, px);
        // With shading, a changed row also changes the hillshade of the rows beside it (a glyph sets its default height).
        const reach = tint ? 1 : 0;
        const changed = (y: number) => y >= 0 && y < cell.height && old.terrain[y] !== cell.terrain[y];
        for (let y = 0; y < cell.height; y++) {
            let dirty = false;
            for (let d = -reach; d <= reach && !dirty; d++) dirty = changed(y + d);
            if (!dirty) continue;
            for (let x = 0; x < cell.width; x++) drawTile(g, cell, x, y, x * px, y * px, px, tint);
        }
        entry = {...old, terrain: cell.terrain, heights: cell.heights};
    } else {
        if (old) cachedPixels -= old.pixels;
        const canvas = document.createElement('canvas');
        canvas.width = cell.width * px; canvas.height = cell.height * px;
        const g = canvas.getContext('2d')!; font(g, px);
        if (px === 1) pixels(g, cell, tint);                      // Zoomed out: one pixel a tile, written directly.
        else for (let y = 0; y < cell.height; y++) for (let x = 0; x < cell.width; x++)
            drawTile(g, cell, x, y, x * px, y * px, px, tint);
        entry = {terrain: cell.terrain, heights: cell.heights, canvas, pixels: canvas.width * canvas.height};
        cachedPixels += entry.pixels;
    }
    cache.set(key, entry);
    for (const [k, e] of cache) {                                  // Oldest first.
        if (cachedPixels <= CACHE_PIXELS || k === key) break;
        cache.delete(k); cachedPixels -= e.pixels;
    }
    return entry.canvas;
}

const rgba = new Map<string, number[]>();
/** A CSS colour as drawn here (#rrggbb, rgb() or rgba()) as [r, g, b, alpha]. */
function parse(css: string): number[] {
    let v = rgba.get(css);
    if (!v) {
        v = css.startsWith('#') ? [1, 3, 5].map(i => parseInt(css.slice(i, i + 2), 16)).concat(1)
            : css.slice(css.indexOf('(') + 1, -1).split(',').map(Number);
        if (v.length === 3) v.push(1);
        rgba.set(css, v);
    }
    return v;
}
/** The whole cell at one pixel a tile, the same colours drawTile gives a tile too small for its glyph, as one
 *  ImageData: a world of millions of tiles draws zoomed right out in moments rather than one rectangle a tile. */
function pixels(g: CanvasRenderingContext2D, cell: AnyCell, tint: boolean) {
    const out = g.createImageData(cell.width, cell.height), d = out.data, grid = tint ? heightGrid(cell) : null;
    for (let y = 0; y < cell.height; y++) {
        const row = cell.terrain[y];
        for (let x = 0; x < cell.width; x++) {
            let [r, gr, b] = parse(swatch(glyphInfo(row[x])));
            if (grid) for (const style of shadeAt(cell, grid, x, y)) {
                if (!style) continue;
                const [sr, sg, sb, a] = parse(style);
                r += (sr - r) * a; gr += (sg - gr) * a; b += (sb - b) * a;
            }
            const i = (y * cell.width + x) * 4;
            d[i] = r; d[i + 1] = gr; d[i + 2] = b; d[i + 3] = 255;
        }
    }
    g.putImageData(out, 0, 0);
}

const previewImages = new WeakMap<string[], HTMLCanvasElement>();
/** A cell still waiting for its ground: its preview (one pixel for each PREVIEW_STEP² tiles), lightly hatched. */
function drawPreview(g: CanvasRenderingContext2D, cell: AnyCell, sx: number, sy: number, s: number) {
    const rows = previewOf(cell.id) ?? [];
    let image = previewImages.get(rows);
    if (!image && rows.length) {
        image = document.createElement('canvas');
        image.width = Math.max(1, rows[0].length); image.height = rows.length;
        const p = image.getContext('2d')!, out = p.createImageData(image.width, image.height), d = out.data;
        rows.forEach((row, y) => { for (let x = 0; x < row.length; x++) {
            const [r, gr, b] = parse(swatch(glyphInfo(row[x]))), i = (y * image!.width + x) * 4;
            d[i] = r; d[i + 1] = gr; d[i + 2] = b; d[i + 3] = 255;
        } });
        p.putImageData(out, 0, 0);
        previewImages.set(rows, image);
    }
    const w = cell.width * s, h = cell.height * s;
    if (image) {
        g.imageSmoothingEnabled = false;
        g.drawImage(image, sx, sy, image.width * PREVIEW_STEP * s, image.height * PREVIEW_STEP * s);
    } else { g.fillStyle = '#1b1f1c'; g.fillRect(sx, sy, w, h); }
    if (s >= DETAIL_FROM) {                                        // Its ground is on its way: say so.
        g.save(); g.beginPath(); g.rect(sx, sy, w, h); g.clip();
        g.strokeStyle = 'rgba(255, 255, 255, .08)'; g.lineWidth = 1;
        for (let d = -h; d < w; d += 24) { g.beginPath(); g.moveTo(sx + d, sy + h); g.lineTo(sx + d + h, sy); g.stroke(); }
        g.restore();
    }
}

/** The surface tiles visible on a `w` × `h` canvas. */
export function visibleTiles(cam: Camera, w: number, h: number) {
    return {x0: Math.floor(-cam.x / cam.s), y0: Math.floor(-cam.y / cam.s), x1: Math.ceil((w - cam.x) / cam.s), y1: Math.ceil((h - cam.y) / cam.s)};
}

/** Draws the ground of every cell (or the interior) in view; `tint` adds the elevation tint and hillshade. */
export function drawGround(g: CanvasRenderingContext2D, surface: Surface, cam: Camera, w: number, h: number, tint: boolean) {
    const {s} = cam, view = visibleTiles(cam, w, h);
    const px = s < 3 ? 1 : s < 7 ? 4 : 12;
    font(g, s);
    for (const piece of surface.pieces(view.x0, view.y0, view.x1, view.y1)) {
        const c = piece.cell, sx = cam.x + piece.x * s, sy = cam.y + piece.y * s;
        if (sx > w || sy > h || sx + c.width * s < 0 || sy + c.height * s < 0) continue;
        if (groundPending(c.id)) {
            drawPreview(g, c, sx, sy, s);
            if (s >= DETAIL_FROM) want(c.id);
            continue;
        }
        if (s > DIRECT_ABOVE) {
            const x0 = Math.max(0, view.x0 - piece.x), y0 = Math.max(0, view.y0 - piece.y);
            const x1 = Math.min(c.width, view.x1 - piece.x), y1 = Math.min(c.height, view.y1 - piece.y);
            for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++)
                drawTile(g, c, x, y, sx + x * s, sy + y * s, s, tint);
        } else {
            g.imageSmoothingEnabled = s < px;
            g.drawImage(image(c, px, tint), sx, sy, c.width * s, c.height * s);
        }
    }
}
