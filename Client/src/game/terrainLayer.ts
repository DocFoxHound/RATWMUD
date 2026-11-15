// The local map's ground, drawn once into an offscreen canvas and then copied to the screen each frame, instead of
// thousands of boxes, glyphs and edges every frame. The copy covers the tiles on screen and a margin around them, so
// a walking wolf scrolls across it; it is drawn again only when what it shows changes (the ground, what the wolf can
// see, the light, the wolf's own height) or the view walks off its margin.
import {css, hexColor, lerp, rgb, scale, withAlpha, type Color} from '../ui/color.ts';
import type {Painter, Point} from '../ui/painter.ts';
import {font, pixels, Sage} from '../ui/theme.ts';
import {clamp} from './json.ts';
import {TERRAIN} from './terrain.generated.mjs';
import {paintBlockTile, treeCodes, trunkColor} from '../../../Data/Visuals/blockTexture.ts';
import {lampsLit, lightOf, LightSources, nightColor, nightLevel, poolAt, type LightSource} from './lights.ts';
import type {GameState} from './state.ts';

export interface TerrainLook {
    name: string;
    effect: string;
    kind: string;
    ramp: boolean;
    solid: boolean;
    glyph: string;
    ascii: string;
    fg: Color;
    bg: Color;
}
const Terrain = new Map<string, TerrainLook>(TERRAIN.map(t => [t.code,
    {name: t.name, effect: t.effect, kind: t.kind, ramp: t.ramp, solid: t.solid, glyph: t.glyph, ascii: t.ascii, fg: hexColor(t.fg),
        bg: hexColor(t.bg)}]));
export const terrainInfo = (code: string) => Terrain.get(code);

/** The tiles a view covers, inclusive. */
export interface TileWindow {
    x0: number;
    y0: number;
    x1: number;
    y1: number;
}

/** Snapshot decoding may replace arrays without changing any ground in the cached window. */
function sameRows(a: string[] | null, b: string[], w: TileWindow): boolean {
    if (a === b) return true;
    if (!a || a.length !== b.length) return false;
    for (let y = w.y0; y <= w.y1; ++y)
        if ((a[y] ?? '').slice(Math.max(0, w.x0), w.x1 + 1) !== (b[y] ?? '').slice(Math.max(0, w.x0), w.x1 + 1))
            return false;
    return true;
}

function sameHeights(a: Float32Array | null, b: Float32Array, w: TileWindow, width: number): boolean {
    if (a === b) return true;
    if (!a || a.length !== b.length) return false;
    for (let y = Math.max(0, w.y0); y <= w.y1; ++y)
        for (let x = Math.max(0, w.x0); x <= w.x1 && x < width; ++x)
            if (a[y * width + x] !== b[y * width + x]) return false;
    return true;
}

/** The columns of band row `y` (from -band) that lie in `w`, as [from, to) in that row's characters (doc 70's layout:
 *  rows above and below the cell whole, those beside it only their two ends, west then east). */
function bandSlices(y: number, w: TileWindow, band: number, width: number, height: number): Array<[number, number]> {
    if (y < 0 || y >= height) return [[Math.max(0, w.x0 + band), Math.max(0, Math.min(width + 2 * band, w.x1 + band + 1))]];
    const out: Array<[number, number]> = [];
    if (w.x0 < 0) out.push([Math.max(0, w.x0 + band), Math.min(band, w.x1 + band + 1)]);
    if (w.x1 >= width) out.push([band + Math.max(0, w.x0 - width), band + Math.min(band, w.x1 - width + 1)]);
    return out;
}

/**
 * Whether the band round the cell (its rows, what is seen of them, their heights) is the same in the copy's region as
 * before. Only the region counts: the screen's edge moves over the band whenever the wolf walks within the screen's
 * reach of an edge (56 rows above and below, 96 columns to the sides), far outside the copy, and comparing the whole
 * band redrew the whole copy at every tile walked there (the user, 2026-11-15: "moving up-and-down has jerk frames").
 */
function sameFringe(a: unknown[], b: unknown[], w: TileWindow, band: number, width: number, height: number): boolean {
    if (a.length !== b.length) return false;
    if (a.every((v, i) => v === b[i])) return true;
    for (let i = 0; i < a.length; ++i) {
        const old = a[i] as ArrayLike<string | Float32Array> | undefined;
        const now = b[i] as ArrayLike<string | Float32Array> | undefined;
        if (old === now) continue;
        if (!old || !now || old.length !== now.length) return false;
        if (old.length !== height + 2 * band) {
            // (Not the band's layout: every row, whole.)
            for (let row = 0; row < old.length; ++row) if (!sameRow(old[row], now[row], 0, Infinity)) return false;
            continue;
        }
        for (let y = Math.max(-band, w.y0); y <= Math.min(height + band - 1, w.y1); ++y) {
            if (y >= 0 && y < height && w.x0 >= 0 && w.x1 < width) continue;    // (Inside the cell: no band there.)
            for (const [from, to] of bandSlices(y, w, band, width, height))
                if (from < to && !sameRow(old[y + band], now[y + band], from, to)) return false;
        }
    }
    return true;
}

function sameRow(x: string | Float32Array | undefined, y: string | Float32Array | undefined, from: number, to: number): boolean {
    if (x === y) return true;
    if (typeof x === 'string' && typeof y === 'string') return x.length === y.length && x.slice(from, to) === y.slice(from, to);
    if (!(x instanceof Float32Array) || !(y instanceof Float32Array) || x.length !== y.length) return false;
    for (let j = from; j < Math.min(to, x.length); ++j) if (x[j] !== y[j]) return false;
    return true;
}

export interface Surface {
    canvas: CanvasImageSource & {width: number; height: number};
    ctx: CanvasRenderingContext2D;
}
export type SurfaceFactory = (width: number, height: number) => Surface | null;

/** An offscreen canvas on the page; none in Node (the layer then draws straight onto the screen). */
export const pageSurface: SurfaceFactory = (width, height) => {
    if (typeof document === 'undefined') return null;
    const canvas = document.createElement('canvas');
    canvas.width = width;
    canvas.height = height;
    const ctx = canvas.getContext('2d');
    return ctx ? {canvas, ctx} : null;
};

const Margin = 8;               // Tiles kept beyond the view on every side.
const Pad = 4;                  // Pixels around the copy, for glyphs lifted above their tile.
const MaxSide = 4096;

interface TileStyle {
    floor: string;              // '' when the tile has no lit floor (remembered).
    fill: number;               // How much of the tile a block glyph fills (0: drawn as text).
    shape: string;
    color: string;
    size: number;
    trunk?: string;             // A tree's trunk colour, as dim as its foreground.
}

const blocks: Record<string, number> = {'█': 1, '▓': 0.75, '▒': 0.5, '░': 0.28};
const smallShapes = new Set(['.', '·', '∙', ',']);

// How far each way the ground's slope is read for its shading: wide enough that a dune's long back and steep face, or
// a valley's sides, read as slopes and not as a stair of half steps.
const Reach = 3;

/** The ground's heights over a window and Reach round it, and which of them are known (unknown ground has none). */
class Relief {
    readonly x0: number;
    readonly y0: number;
    readonly width: number;
    readonly heights: Float32Array;
    readonly known: Uint8Array;

    constructor(s: GameState, w: TileWindow) {
        this.x0 = w.x0 - Reach;
        this.y0 = w.y0 - Reach;
        this.width = w.x1 - w.x0 + 1 + 2 * Reach;
        const height = w.y1 - w.y0 + 1 + 2 * Reach;
        this.heights = new Float32Array(this.width * height);
        this.known = new Uint8Array(this.width * height);
        for (let j = 0; j < height; ++j)
            for (let i = 0; i < this.width; ++i) {
                const x = this.x0 + i, y = this.y0 + j, at = j * this.width + i;
                if (s.seenAt(x, y) === '0' || s.glyphAt(x, y) === ' ') continue;
                this.heights[at] = s.heightAt(x, y);
                this.known[at] = 1;
            }
    }

    /**
     * How the ground at (x, y) faces the light from the north-west: its slope in height per tile, read over one, two
     * and three tiles each way and averaged, lit (above 0) where it rises to the south-east, in shade (below 0) where
     * it falls; to a twentieth, so the tile styles keep. Unknown ground counts as level with (x, y), so the edge of
     * what is known never shades as a false slope.
     */
    facing(x: number, y: number): number {
        const here = this.heights[(y - this.y0) * this.width + (x - this.x0)];
        const h = (tx: number, ty: number) => {
            const at = (ty - this.y0) * this.width + (tx - this.x0);
            return this.known[at] ? this.heights[at] : here;
        };
        let gx = 0, gy = 0;
        for (let d = 1; d <= Reach; ++d) {
            gx += (h(x + d, y) - h(x - d, y)) / (2 * d);
            gy += (h(x, y + d) - h(x, y - d)) / (2 * d);
        }
        return Math.round(clamp((gx + gy) / Reach * 2, -0.6, 0.45) * 20) / 20;
    }
}

const luma = (c: Color) => 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;

/** `top` laid over `bottom` (both partly see-through), as one colour. */
function over(top: Color, bottom: Color): Color {
    const a = top.a + bottom.a * (1 - top.a);
    if (a <= 0) return withAlpha(bottom, 0);
    const mix = (t: number, b: number) => (t * top.a + b * bottom.a * (1 - top.a)) / a;
    return {r: mix(top.r, bottom.r), g: mix(top.g, bottom.g), b: mix(top.b, bottom.b), a};
}

/**
 * The light the ground is drawn in: the eased look of the sky (ambient.ts, so a seam never snaps it), in steps of a
 * twenty-fifth, so a fade between cells redraws the cached ground a few times rather than every frame.
 */
export const lightStep = (illumination: number) => Math.round(clamp(illumination, 0, 1) * 25) / 25;
export const groundIllumination = (s: GameState) => lightStep(s.ambient.env.illumination);

/** The light sources shining on a window: how strongly each tile is lit and by which (the strongest) of them. */
class Pools {
    readonly glow: Float32Array;
    readonly by: string[];
    readonly lights: Array<{x: number; y: number; code: string; light: LightSource}> = [];
    private w: TileWindow;

    constructor(s: GameState, w: TileWindow, lit: number) {
        this.w = w;
        const width = w.x1 - w.x0 + 1, height = w.y1 - w.y0 + 1;
        this.glow = new Float32Array(Math.max(0, width * height));
        this.by = new Array(Math.max(0, width * height)).fill('');
        if (lit <= 0) return;
        const reach = Math.ceil(Math.max(...Object.values(LightSources).map(l => l.radius)));
        for (let ly = w.y0 - reach; ly <= w.y1 + reach; ++ly)
            for (let lx = w.x0 - reach; lx <= w.x1 + reach; ++lx) {
                const code = s.glyphAt(lx, ly), light = lightOf(code);
                if (!light || s.seenAt(lx, ly) === '0') continue;
                this.lights.push({x: lx, y: ly, code, light});
                const r = Math.ceil(light.radius);
                for (let y = Math.max(w.y0, ly - r); y <= Math.min(w.y1, ly + r); ++y)
                    for (let x = Math.max(w.x0, lx - r); x <= Math.min(w.x1, lx + r); ++x) {
                        const g = poolAt(Math.hypot(x - lx, y - ly), light.radius) * light.strength * lit, at = (y - w.y0) * width + (x - w.x0);
                        if (g > this.glow[at]) {
                            this.glow[at] = g;
                            this.by[at] = code;
                        }
                    }
            }
    }

    at(x: number, y: number): [number, string] {
        if (x < this.w.x0 || x > this.w.x1 || y < this.w.y0 || y > this.w.y1) return [0, ''];
        const at = (y - this.w.y0) * (this.w.x1 - this.w.x0 + 1) + (x - this.w.x0);
        return [this.glow[at] ?? 0, this.by[at] ?? ''];
    }
}

/** Draws the ground: tiles then height edges, for the tiles in `w`, with tile (0, 0) at (ox, oy). */
export class GroundPainter {
    private styles = new Map<string, TileStyle>();
    private widths = new Map<string, number>();
    private lines = new Map<string, string>();

    /** `illumination` is the light the ground is drawn in (by default the eased sky's, in its steps). */
    paint(c: CanvasRenderingContext2D, s: GameState, w: TileWindow, ox: number, oy: number, tile: number,
          illumination = groundIllumination(s)) {
        const dark = 1 - illumination;
        // The night is one rule for every glyph, texture, floor and edge (lights.ts); only light sources shine through
        // it, and their pools light the ground round them: lit at dusk and night, from the lamps the wolf knows of.
        const lit = lampsLit(dark);
        // (A tile past the window's far sides too: an edge there is coloured by the tile beyond it.)
        const pools = new Pools(s, {x0: w.x0, y0: w.y0, x1: w.x1 + 1, y1: w.y1 + 1}, lit);
        const ground = s.selfHeight();
        if (this.styles.size > 20000) this.styles.clear();
        let currentFont = '', currentFill = '';
        c.textBaseline = 'top';
        // The cell's tiles, and past its edges the ground round it (doc 70), alike.
        const relief = new Relief(s, w);
        for (let y = w.y0; y <= w.y1; ++y) {
            for (let x = w.x0; x <= w.x1; ++x) {
                const code = s.glyphAt(x, y);
                const seen = s.seenAt(x, y);
                if (seen === '0' || code === ' ') continue;
                const known = seen === '1';
                const rise = s.heightAt(x, y) - ground;
                const facing = relief.facing(x, y);
                const [glow, by] = pools.at(x, y);
                const warm = Math.round(glow * 20) / 20;
                // Lamplight takes back the night where it falls (to a fifth of it at the lamp).
                const localDark = dark * (1 - warm * 0.8);
                const darkStep = Math.round(localDark * 50);
                const source = lightOf(code) && lit > 0 ? Math.round(lit * 10) / 10 : 0;
                const key = `${code}|${seen}|${rise}|${facing}|${darkStep}|${warm}|${by}|${source}|${s.plainGlyphs ? 1 : 0}`;
                let style = this.styles.get(key);
                if (!style) this.styles.set(key, style = this.style(code, known, rise, facing, localDark, s.plainGlyphs, warm, by, source));
                const px = ox + x * tile, py = oy + y * tile;
                if (style.floor) {
                    if (currentFill !== style.floor) c.fillStyle = currentFill = style.floor;
                    c.fillRect(px, py, tile, tile);
                }
                const lift = clamp(rise, -2, 2);
                // The colours carry the night already: the texture is drawn at its full alpha.
                if (!s.plainGlyphs && paintBlockTile(c, code, x, y, px, py - lift, tile, style.color, 1, style.trunk)) {
                    currentFill = '';
                    continue;
                }
                if (currentFill !== style.color) c.fillStyle = currentFill = style.color;
                if (style.fill > 0) c.fillRect(px, py - lift, tile, tile);
                else {
                    const f = font(style.size, true);
                    if (currentFont !== f) c.font = currentFont = f;
                    const widthKey = `${style.size}|${style.shape}`;
                    let width = this.widths.get(widthKey);
                    if (width === undefined) this.widths.set(widthKey, width = c.measureText(style.shape).width);
                    const height = pixels(style.size) * 1.2;
                    c.fillText(style.shape, px + (tile - width) * 0.5, py + (tile - height) * 0.5 - lift + pixels(style.size) * 0.12);
                }
            }
        }
        this.edges(c, s, w, ox, oy, tile, dark, pools);
        this.lightPools(c, s, pools, ox, oy, tile, lit);
    }

    /**
     * The light of the lamps (and braziers, fires) the wolf knows of, at dusk and night: a warm pool on the ground round
     * each, brightest at the lamp, added over the ground already drawn, and a small bright halo at the flame itself.
     */
    private lightPools(c: CanvasRenderingContext2D, s: GameState, pools: Pools, ox: number, oy: number, tile: number, lit: number) {
        if (lit <= 0 || !pools.lights.length || typeof c.createRadialGradient !== 'function') return;
        const ground = s.selfHeight();
        c.save();
        c.globalCompositeOperation = 'lighter';
        for (const {x, y, light} of pools.lights) {
            const lift = clamp(s.heightAt(x, y) - ground, -2, 2);
            const cx = ox + (x + 0.5) * tile, cy = oy + (y + 0.5) * tile - lift, r = light.radius * tile;
            const g = c.createRadialGradient(cx, cy, 0, cx, cy, r);
            const a = light.strength * lit;
            g.addColorStop(0, css(withAlpha(light.color, 0.3 * a)));
            g.addColorStop(0.35, css(withAlpha(light.color, 0.13 * a)));
            g.addColorStop(1, css(withAlpha(light.color, 0)));
            c.fillStyle = g;
            c.fillRect(cx - r, cy - r, r * 2, r * 2);
            const core = c.createRadialGradient(cx, cy - tile * 0.15, 0, cx, cy - tile * 0.15, tile * 0.8);
            core.addColorStop(0, css(withAlpha(light.color, 0.55 * a)));
            core.addColorStop(1, css(withAlpha(light.color, 0)));
            c.fillStyle = core;
            c.fillRect(cx - tile, cy - tile * 1.15, tile * 2, tile * 2);
        }
        c.restore();
    }

    private style(code: string, known: boolean, rise: number, facing: number, dark: number, plain: boolean, warm = 0, by = '',
                  source = 0): TileStyle {
        const info = terrainInfo(code);
        const lamp = lightOf(by);
        // Lamplight on a surface: its colour turned toward the light's, at its own brightness.
        const warmed = (c: Color) => warm > 0 && lamp ? lerp(c, scale(lamp.color, luma(c) / Math.max(1e-4, luma(lamp.color))), warm * 0.45) : c;
        let color = info ? info.fg : Sage;
        // A lit light source is itself bright, whatever the night; everything else is as dim as the night makes it.
        if (source > 0) color = lerp(color, rgb(0xfff1d6), source * 0.25);
        else color = warmed(nightColor(color, dark));
        // The lie of the land (the slope's facing): a dune's steep face, a valley's far side, in shade, the slopes
        // turned to the light a little brighter, in the glyphs as in the floor, so the shape reads in remembered
        // ground too.
        // The slopes' light is the sun's (the light from the north-west): at night a slope turned to it is no brighter
        // than flat ground, and only the shade of one turned away is left, so a long step in the street (the Commons and
        // the Rise, 2026-11-15) never shows as a pale band in the dark.
        if (facing > 0) facing *= 1 - clamp(dark, 0, 1) * 0.85;
        color = withAlpha(scale(color, 1 + facing * 0.45), color.a);
        // Remembered ground stays readable, only softer: a gentle step down from what is in sight, so the edge of sight
        // in broken country (a forest) doesn't cut the map into hard patches.
        if (known) color = withAlpha(scale(color, 0.8), 0.5);
        // Height reads relative to the wolf: ground above it is lit and warm, ground below sinks into shade, and
        // slopes facing the north-west light are brighter than those turned away.
        const base = info ? info.bg : rgb(0x283126);
        let f = rise >= 0 ? lerp(base, rgb(0x6b6a4a), Math.min(rise * 0.12, 0.4)) : lerp(base, rgb(0x0b1216), Math.min(-rise * 0.14, 0.5));
        f = withAlpha(scale(f, 1 + facing * 0.2), 0.27 + Math.min(Math.abs(rise) * 0.03, 0.12));
        // And over the floor a wash of light on a slope turned to it, of shade on one turned away: a dune's steep lee,
        // a valley's far side, read as bands, faint where the ground is gentle.
        f = over(facing >= 0 ? rgb(0xe8dcb4, facing * 0.18) : rgb(0x030507, -facing * 0.45), f);
        // The floor, its height's warmth and its slope's light included, dims with the night like everything else.
        f = warmed(nightColor(f, dark));
        if (known) f = withAlpha(scale(f, 0.75), f.a * 0.7);
        const floor = css(f);
        const shape = !info ? code : plain ? info.ascii : info.glyph;
        // Block and shade characters fill the whole tile, so walls and cliffs read as one mass.
        const fill = plain ? 0 : blocks[shape] ?? 0;
        let trunk: string | undefined;
        if (treeCodes.has(code)) {
            let t = warmed(nightColor(hexColor(trunkColor(code)), dark));
            if (known) t = withAlpha(scale(t, 0.8), 0.5);
            trunk = css(t);
        }
        return {floor, fill, shape, color: css(fill > 0 ? withAlpha(color, color.a * fill) : color), size: smallShapes.has(shape) ? 12 : 15,
            trunk};
    }

    /** An edge's line colour, as dim as the night (and as warm as a lamp near it) makes the ground there. */
    private line(hex: number, alpha: number, dark: number, warm: number, by: string): string {
        const key = `${hex}|${alpha}|${Math.round(dark * 50)}|${warm}|${by}`;
        let v = this.lines.get(key);
        if (v === undefined) {
            let c = nightColor(rgb(hex, alpha), dark);
            const lamp = lightOf(by);
            if (warm > 0 && lamp) c = lerp(c, withAlpha(scale(lamp.color, luma(c) / Math.max(1e-4, luma(lamp.color))), c.a), warm * 0.45);
            if (this.lines.size > 4000) this.lines.clear();
            this.lines.set(key, v = css(c));
        }
        return v;
    }

    // Where the ground changes height, the edge is drawn by how it can be crossed: a faint contour for a half step, a
    // warm line where a slope or stairs make a full step walkable, a heavy rim with a cast shadow for a ledge or cliff
    // that can't be walked.
    private edges(c: CanvasRenderingContext2D, s: GameState, w: TileWindow, ox: number, oy: number, tile: number, dark: number,
                  pools: Pools) {
        const seenOpen = (x: number, y: number) => {
            const info = terrainInfo(s.glyphAt(x, y));
            return !!info && info.kind !== 'wall' && s.seenAt(x, y) !== '0';
        };
        const line = (from: Point, to: Point, color: string, width: number) => {
            c.beginPath();
            c.moveTo(from[0], from[1]);
            c.lineTo(to[0], to[1]);
            c.strokeStyle = color;
            c.lineWidth = width;
            c.lineCap = 'round';
            c.stroke();
        };
        // A ledge's pale rim and a stair's warm line are as dim at night as the ground they edge (the user, 2026-11-14:
        // stairs' edges glowed; 2026-11-15: "a line that also looks like it's glowing" along the Commons and the Rise).
        const shadow = css(rgb(0x04070a, 0.4));
        for (let y = w.y0; y <= w.y1; ++y) {
            for (let x = w.x0; x <= w.x1; ++x)
                for (let d = 0; d < 2; ++d) {
                    const dx = d === 0 ? 1 : 0, dy = 1 - dx;
                    const nx = x + dx, ny = y + dy;
                    if (!seenOpen(x, y) || !seenOpen(nx, ny)) continue;
                    const infoA = terrainInfo(s.glyphAt(x, y))!, infoB = terrainInfo(s.glyphAt(nx, ny))!;
                    const ha = s.heightAt(x, y), hb = s.heightAt(nx, ny), drop = Math.abs(ha - hb);
                    const cliff = infoA.kind === 'cliff' || infoB.kind === 'cliff';
                    if (drop < 0.01 && !cliff) continue;
                    // Remembered ground comes with its heights (its shape is remembered with it), but its edges only as
                    // faint contours: a ledge's rim and shadow are for what is in sight.
                    const remembered = s.seenAt(x, y) === '1' || s.seenAt(nx, ny) === '1';
                    const [glow, by] = pools.at(nx, ny);
                    const warm = Math.round(glow * 20) / 20, here = dark * (1 - warm * 0.8);
                    const contourRemembered = this.line(0xc9bf9a, 0.1, here, warm, by), contour = this.line(0xc9bf9a, 0.2, here, warm, by);
                    // A ramp's warm line and a ledge's pale rim are sunlit highlights: at night most of their light goes.
                    const sun = 1 - here * 0.7;
                    const ramp = this.line(0xd8b877, 0.34 * sun, here, warm, by), rim = this.line(0xe9d2a0, 0.55 * sun, here, warm, by);
                    const cx = ox + nx * tile, cy = oy + ny * tile;
                    const end: Point = dx ? [cx, cy + tile] : [cx + tile, cy];
                    if (remembered) line([cx, cy], end, contourRemembered, 1);
                    else if (drop <= 0.5 && !cliff) line([cx, cy], end, contour, 1);
                    else if (drop <= 1.01 && (infoA.ramp || infoB.ramp) && !cliff) line([cx, cy], end, ramp, 1.3);
                    else {
                        // The shadow falls on the lower side; a level cliff edge shades its open side.
                        const lowAfter = hb < ha || (drop < 0.01 && infoA.kind === 'cliff');
                        const band = Math.min(6, tile * 0.28);
                        c.fillStyle = shadow;
                        c.fillRect(lowAfter ? cx : cx - dx * band, lowAfter ? cy : cy - dy * band, dx ? band : tile, dx ? tile : band);
                        line([cx, cy], end, rim, 2.2);
                    }
                }
        }
    }
}

/** How many device pixels one canvas unit is, as the context is now set up (1 where it can't say). */
function deviceScale(c: CanvasRenderingContext2D): number {
    const t = typeof c.getTransform === 'function' ? c.getTransform() : null;
    return t && Number.isFinite(t.a) && t.a > 0 ? t.a : 1;
}

/** (x, y) in the context's units moved to the nearest whole device pixel, as the context is now set up. */
export function snapToDevice(c: CanvasRenderingContext2D, x: number, y: number): [number, number] {
    const t = typeof c.getTransform === 'function' ? c.getTransform() : null;
    if (!t || !(t.a > 0) || !(t.d > 0)) return [x, y];
    return [(Math.round(t.a * x + t.e) - t.e) / t.a, (Math.round(t.d * y + t.f) - t.f) / t.d];
}

export class TerrainLayer {
    rebuilds = 0;                       // For the tests and the frame-time overlay.
    scrolls = 0;                        // How often the copy was moved on with only its new edge drawn (scroll).
    private surface: Surface | null = null;
    private spare: Surface | null = null;   // The other copy, drawn into when the view moves on (scroll).
    private relitCopy: Surface | null = null;   // The copy brightened or dimmed to the sky as it fades (relit).
    private level = 1;                  // The light the copy is drawn in (lightStep).
    private region: TileWindow = {x0: 0, y0: 0, x1: -1, y1: -1};
    private key = '';
    private rows: string[] | null = null;
    private visibility: string[] | null = null;
    private heights: Float32Array | null = null;
    private fringe: unknown[] = [];
    private ground = new GroundPainter();
    private factory: SurfaceFactory;

    constructor(factory: SurfaceFactory = pageSurface) {
        this.factory = factory;
    }

    /** Forgets the copy (and, as fonts have changed, every measured glyph): the next draw makes a new one. */
    invalidate() {
        this.key = '';
        this.ground = new GroundPainter();
    }

    /** The ground for the tiles in `view`, with tile (0, 0) at (ox, oy) in the target's units. */
    draw(target: Painter, s: GameState, view: TileWindow, ox: number, oy: number, tile: number) {
        const c = target.ctx;
        const k = deviceScale(c);
        // The copy is drawn in the light the sky is fading to (the server's, in its steps), not in each step of the
        // fade: a crossing into a darker or lighter cell draws it once, and the fade itself is laid over it (relit).
        const level = lightStep(s.environment.illumination);
        const key = [s.cellId, s.cellGeneration, tile, k, s.plainGlyphs, s.selfHeight(), level,
            s.cellWidth, s.cellHeight, s.fringe].join('|');
        const inside = view.x0 >= this.region.x0 && view.y0 >= this.region.y0 && view.x1 <= this.region.x1 && view.y1 <= this.region.y1;
        const fringe = [s.fringeRows, s.fringeVisibility, s.fringeHeights];
        const same = key === this.key && sameRows(this.rows, s.tileRows, this.region) &&
            sameRows(this.visibility, s.visibilityRows, this.region) && sameHeights(this.heights, s.tileHeights, this.region, s.cellWidth) &&
            sameFringe(this.fringe, fringe, this.region, s.fringe, s.cellWidth, s.cellHeight);
        if (!same || !inside) {
            // Walked on past the copy, the ground in it as it was: the copy moves on, and only what it hadn't is drawn.
            this.level = level;
            if (!(same && this.scroll(s, view, tile, k)) && !this.rebuild(s, view, tile, k)) {
                this.key = '';
                this.ground.paint(c, s, view, ox, oy, tile);
                return;
            }
            this.key = key;
            this.rows = s.tileRows;
            this.visibility = s.visibilityRows;
            this.heights = s.tileHeights;
            this.fringe = fringe;
        }
        const surface = this.relit(this.surface!, s.ambient.env.illumination);
        // Whole device pixels only, so glyphs stay crisp however the camera moves.
        const [x, y] = snapToDevice(c, ox + this.region.x0 * tile - Pad / k, oy + this.region.y0 * tile - Pad / k);
        c.drawImage(surface.canvas, x, y, surface.canvas.width / k, surface.canvas.height / k);
    }

    /**
     * The copy as bright as the sky is now, while it fades toward the light the copy is drawn in (ambient.ts eases it
     * over a few seconds after a crossing): the night rule keeps `nightLevel` of every colour's brightness as the eye
     * sees it, so the fade is that ratio, laid over the copy's own pixels (dimmed by a see-through black atop them,
     * brightened by the copy added to itself). One copy and two blits a frame, where drawing the copy again at each
     * twenty-fifth of the fade cost a whole redraw (20-30 ms in the city) up to 25 times a crossing; and a strip drawn
     * as the wolf walks on is in the copy's light like the rest, so it is faded with it, never a band of its own.
     * The copy itself when the sky is in its step (or there is no second canvas).
     */
    private relit(surface: Surface, illumination: number): Surface {
        const shown = clamp(illumination, 0, 1);
        if (lightStep(shown) === this.level) return surface;
        const ratio = nightLevel(1 - shown) / nightLevel(1 - this.level);
        const {width, height} = surface.canvas;
        let to = this.relitCopy;
        if (!to || to.canvas.width !== width || to.canvas.height !== height) to = this.relitCopy = this.factory(width, height);
        if (!to) return surface;
        const c = to.ctx;
        c.save();
        c.setTransform(1, 0, 0, 1, 0, 0);
        c.globalCompositeOperation = 'copy';
        c.drawImage(surface.canvas, 0, 0);
        if (ratio < 1) {
            c.globalCompositeOperation = 'source-atop';
            c.fillStyle = `rgba(0, 0, 0, ${(1 - ratio).toFixed(4)})`;
            c.fillRect(0, 0, width, height);
        } else {
            c.globalCompositeOperation = 'lighter';
            for (let more = ratio - 1; more > 0.002; more -= 1) {
                c.globalAlpha = Math.min(1, more);
                c.drawImage(surface.canvas, 0, 0);
            }
        }
        c.restore();
        return to;
    }

    /** The tiles a copy for `view` holds: the view and Margin round it, on the ground there is. */
    private regionFor(s: GameState, view: TileWindow): TileWindow {
        const g = s.groundExtent();
        return {x0: Math.max(g.x0, view.x0 - Margin), y0: Math.max(g.y0, view.y0 - Margin),
            x1: Math.min(g.x1, view.x1 + Margin), y1: Math.min(g.y1, view.y1 + Margin)};
    }

    /**
     * Moves the copy on to the region round `view` without drawing it all again: what it shares with the old one is
     * copied across, pixel for pixel, and only the ground new to it is drawn, with a tile of the old each side of the
     * seam between them (a glyph or a ledge's line reaches a little over its tile), cut to that strip. A full redraw
     * of a city view was 20-30 ms, every eight tiles walked (the user, 2026-11-15: "frame jitter, especially in the
     * market"); a strip is a fraction of it. False when it can't (a tile not a whole number of device pixels, so the
     * copy wouldn't land on its pixels; too little in common; no second canvas): then the whole copy is drawn.
     */
    private scroll(s: GameState, view: TileWindow, tile: number, k: number): boolean {
        const old = this.region, from = this.surface, step = tile * k;
        if (!from || !Number.isInteger(step)) return false;
        // The same size of copy, moved on to hold the view and Margin past it on the side it left by (so the second
        // canvas is reused), on the ground there is.
        const g = s.groundExtent();
        const shift = (lo: number, hi: number, viewLo: number, viewHi: number, groundLo: number, groundHi: number) =>
            Math.max(groundLo - lo, Math.min(groundHi - hi, viewLo < lo ? viewLo - Margin - lo : viewHi > hi ? viewHi + Margin - hi : 0));
        const dx = shift(old.x0, old.x1, view.x0, view.x1, g.x0, g.x1), dy = shift(old.y0, old.y1, view.y0, view.y1, g.y0, g.y1);
        const region: TileWindow = {x0: old.x0 + dx, x1: old.x1 + dx, y0: old.y0 + dy, y1: old.y1 + dy};
        if (view.x0 < region.x0 || view.x1 > region.x1 || view.y0 < region.y0 || view.y1 > region.y1 ||
            region.x0 < g.x0 || region.x1 > g.x1 || region.y0 < g.y0 || region.y1 > g.y1) return false;
        const ix0 = Math.max(old.x0, region.x0), ix1 = Math.min(old.x1, region.x1);
        const iy0 = Math.max(old.y0, region.y0), iy1 = Math.min(old.y1, region.y1);
        if (ix1 - ix0 < 4 || iy1 - iy0 < 4) return false;
        const width = (region.x1 - region.x0 + 1) * step + Pad * 2, height = (region.y1 - region.y0 + 1) * step + Pad * 2;
        if (width > MaxSide || height > MaxSide) return false;
        let to = this.spare;
        if (!to || to.canvas.width !== width || to.canvas.height !== height) to = this.factory(width, height);
        if (!to) return false;
        const c = to.ctx;
        c.setTransform(1, 0, 0, 1, 0, 0);
        c.clearRect(0, 0, width, height);
        c.drawImage(from.canvas, (old.x0 - region.x0) * step, (old.y0 - region.y0) * step);
        // Device pixels of the edge of tile column x (row y) in the new copy.
        const px = (x: number) => Pad + (x - region.x0) * step, py = (y: number) => Pad + (y - region.y0) * step;
        // Where the copy shrank, the old ground past its new edge goes (it would show in the edge's padding).
        if (region.x0 > old.x0) c.clearRect(0, 0, Pad, height);
        if (region.x1 < old.x1) c.clearRect(px(region.x1 + 1), 0, width, height);
        if (region.y0 > old.y0) c.clearRect(0, 0, width, Pad);
        if (region.y1 < old.y1) c.clearRect(0, py(region.y1 + 1), width, height);
        const strips: Array<{clip: [number, number, number, number]; tiles: TileWindow}> = [];
        const all = {x0: region.x0, x1: region.x1, y0: region.y0, y1: region.y1};
        if (region.x0 < ix0) strips.push({clip: [0, 0, px(ix0 + 1), height], tiles: {...all, x1: Math.min(region.x1, ix0 + 2)}});
        if (region.x1 > ix1) strips.push({clip: [px(ix1), 0, width, height], tiles: {...all, x0: Math.max(region.x0, ix1 - 1)}});
        if (region.y0 < iy0) strips.push({clip: [0, 0, width, py(iy0 + 1)], tiles: {...all, y1: Math.min(region.y1, iy0 + 2)}});
        if (region.y1 > iy1) strips.push({clip: [0, py(iy1), width, height], tiles: {...all, y0: Math.max(region.y0, iy1 - 1)}});
        for (const {clip: [x0, y0, x1, y1], tiles} of strips) {
            c.setTransform(1, 0, 0, 1, 0, 0);
            c.clearRect(x0, y0, x1 - x0, y1 - y0);
            c.save();
            c.beginPath();
            c.rect(x0, y0, x1 - x0, y1 - y0);
            c.clip();
            c.setTransform(k, 0, 0, k, Pad, Pad);
            this.ground.paint(c, s, tiles, -region.x0 * tile, -region.y0 * tile, tile, this.level);
            c.restore();
        }
        this.spare = from;
        this.surface = to;
        this.region = region;
        ++this.scrolls;
        return true;
    }

    private rebuild(s: GameState, view: TileWindow, tile: number, k: number): boolean {
        const region = this.regionFor(s, view);
        const width = Math.ceil((region.x1 - region.x0 + 1) * tile * k) + Pad * 2;
        const height = Math.ceil((region.y1 - region.y0 + 1) * tile * k) + Pad * 2;
        if (width <= 0 || height <= 0 || width > MaxSide || height > MaxSide) return false;
        if (!this.surface || this.surface.canvas.width !== width || this.surface.canvas.height !== height) {
            this.surface = this.factory(width, height);
            if (!this.surface) return false;
        }
        const c = this.surface.ctx;
        c.setTransform(1, 0, 0, 1, 0, 0);
        c.clearRect(0, 0, width, height);
        c.setTransform(k, 0, 0, k, Pad, Pad);
        this.ground.paint(c, s, region, -region.x0 * tile, -region.y0 * tile, tile, this.level);
        this.region = region;
        ++this.rebuilds;
        return true;
    }
}
