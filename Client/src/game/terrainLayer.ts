// The local map's ground, drawn once into an offscreen canvas and then copied to the screen each frame, instead of
// thousands of boxes, glyphs and edges every frame. The copy covers the tiles on screen and a margin around them, so
// a walking wolf scrolls across it; it is drawn again only when what it shows changes (the ground, what the wolf can
// see, the light, the wolf's own height) or the view walks off its margin.
import {css, hexColor, lerp, rgb, scale, withAlpha, type Color} from '../ui/color.ts';
import type {Painter, Point} from '../ui/painter.ts';
import {font, pixels, Sage} from '../ui/theme.ts';
import {clamp} from './json.ts';
import {TERRAIN} from './terrain.generated.mjs';
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

/** `top` laid over `bottom` (both partly see-through), as one colour. */
function over(top: Color, bottom: Color): Color {
    const a = top.a + bottom.a * (1 - top.a);
    if (a <= 0) return withAlpha(bottom, 0);
    const mix = (t: number, b: number) => (t * top.a + b * bottom.a * (1 - top.a)) / a;
    return {r: mix(top.r, bottom.r), g: mix(top.g, bottom.g), b: mix(top.b, bottom.b), a};
}

/** Draws the ground: tiles then height edges, for the tiles in `w`, with tile (0, 0) at (ox, oy). */
export class GroundPainter {
    private styles = new Map<string, TileStyle>();
    private widths = new Map<string, number>();

    paint(c: CanvasRenderingContext2D, s: GameState, w: TileWindow, ox: number, oy: number, tile: number) {
        const e = s.environment;
        const dark = 1 - e.illumination;
        const darkStep = Math.round(dark * 50);
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
                const key = `${code}|${seen}|${rise}|${facing}|${darkStep}|${s.plainGlyphs ? 1 : 0}`;
                let style = this.styles.get(key);
                if (!style) this.styles.set(key, style = this.style(code, known, rise, facing, dark, s.plainGlyphs));
                const px = ox + x * tile, py = oy + y * tile;
                if (style.floor) {
                    if (currentFill !== style.floor) c.fillStyle = currentFill = style.floor;
                    c.fillRect(px, py, tile, tile);
                }
                const lift = clamp(rise, -2, 2);
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
        this.edges(c, s, w, ox, oy, tile);
    }

    private style(code: string, known: boolean, rise: number, facing: number, dark: number, plain: boolean): TileStyle {
        const info = terrainInfo(code);
        let color = info ? info.fg : Sage;
        if (dark > 0) {
            color = lerp(color, withAlpha(rgb(0x8195ad), color.a), dark * 0.28);
            color = withAlpha(scale(color, 1 - dark * 0.27), color.a);
        }
        // The lie of the land (the slope's facing): a dune's steep face, a valley's far side, in shade, the slopes
        // turned to the light a little brighter, in the glyphs as in the floor, so the shape reads in remembered
        // ground too.
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
        if (known) f = withAlpha(scale(f, 0.75), f.a * 0.7);
        const floor = css(f);
        const shape = !info ? code : plain ? info.ascii : info.glyph;
        // Block and shade characters fill the whole tile, so walls and cliffs read as one mass.
        const fill = plain ? 0 : blocks[shape] ?? 0;
        return {floor, fill, shape, color: css(fill > 0 ? withAlpha(color, color.a * fill) : color), size: smallShapes.has(shape) ? 12 : 15};
    }

    // Where the ground changes height, the edge is drawn by how it can be crossed: a faint contour for a half step, a
    // warm line where a slope or stairs make a full step walkable, a heavy rim with a cast shadow for a ledge or cliff
    // that can't be walked.
    private edges(c: CanvasRenderingContext2D, s: GameState, w: TileWindow, ox: number, oy: number, tile: number) {
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
        const contour = css(rgb(0xc9bf9a, 0.2));
        const contourRemembered = css(rgb(0xc9bf9a, 0.1));
        const ramp = css(rgb(0xd8b877, 0.34));
        const shadow = css(rgb(0x04070a, 0.4));
        const rim = css(rgb(0xe9d2a0, 0.55));
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

export class TerrainLayer {
    rebuilds = 0;                       // For the tests and the frame-time overlay.
    private surface: Surface | null = null;
    private region: TileWindow = {x0: 0, y0: 0, x1: -1, y1: -1};
    private key = '';
    private rows: unknown = null;
    private visibility: unknown = null;
    private heights: unknown = null;
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
        const key = [s.cellId, s.cellGeneration, tile, k, s.plainGlyphs, s.selfHeight(), Math.round(s.environment.illumination * 50),
            s.cellWidth, s.cellHeight, s.fringe].join('|');
        const inside = view.x0 >= this.region.x0 && view.y0 >= this.region.y0 && view.x1 <= this.region.x1 && view.y1 <= this.region.y1;
        const fringe = [s.fringeRows, s.fringeVisibility, s.fringeHeights];
        if (key !== this.key || !inside || this.rows !== s.tileRows || this.visibility !== s.visibilityRows || this.heights !== s.tileHeights ||
            fringe.some((part, i) => part !== this.fringe[i])) {
            if (!this.rebuild(s, view, tile, k)) {
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
        const surface = this.surface!;
        // Whole device pixels only, so glyphs stay crisp however the camera moves.
        const t = typeof c.getTransform === 'function' ? c.getTransform() : null;
        let x = ox + this.region.x0 * tile - Pad / k, y = oy + this.region.y0 * tile - Pad / k;
        if (t && t.a > 0 && t.d > 0) {
            x = (Math.round(t.a * x + t.e) - t.e) / t.a;
            y = (Math.round(t.d * y + t.f) - t.f) / t.d;
        }
        c.drawImage(surface.canvas, x, y, surface.canvas.width / k, surface.canvas.height / k);
    }

    private rebuild(s: GameState, view: TileWindow, tile: number, k: number): boolean {
        const g = s.groundExtent();
        const region: TileWindow = {x0: Math.max(g.x0, view.x0 - Margin), y0: Math.max(g.y0, view.y0 - Margin),
            x1: Math.min(g.x1, view.x1 + Margin), y1: Math.min(g.y1, view.y1 + Margin)};
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
        this.ground.paint(c, s, region, -region.x0 * tile, -region.y0 * tile, tile);
        this.region = region;
        ++this.rebuilds;
        return true;
    }
}
