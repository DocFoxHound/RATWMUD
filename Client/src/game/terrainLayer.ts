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
        for (let y = w.y0; y <= w.y1; ++y) {
            const row = s.tileRows[y], seenRow = s.visibilityRows[y];
            if (!row) continue;
            const last = Math.min(w.x1, row.length - 1);
            for (let x = w.x0; x <= last; ++x) {
                const code = row[x];
                const seen = seenRow?.[x] ?? '2';
                if (seen === '0' || code === ' ') continue;
                const known = seen === '1';
                const rise = s.heightAt(x, y) - ground;
                const facing = known ? 0
                    : clamp((s.heightAt(x + 1, y) + s.heightAt(x, y + 1) - s.heightAt(x - 1, y) - s.heightAt(x, y - 1)) * 0.5, -2, 2);
                const key = `${code}|${seen}|${rise}|${facing}|${darkStep}|${s.plainGlyphs ? 1 : 0}`;
                let style = this.styles.get(key);
                if (!style) this.styles.set(key, style = this.style(code, known, rise, facing, dark, s.plainGlyphs));
                const px = ox + x * tile, py = oy + y * tile;
                if (style.floor) {
                    if (currentFill !== style.floor) c.fillStyle = currentFill = style.floor;
                    c.fillRect(px, py, tile - 1, tile - 1);
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
        if (known) color = withAlpha(color, 0.22);
        // Height reads relative to the wolf: ground above it is lit and warm, ground below sinks into shade, and
        // slopes facing the north-west light are brighter than those turned away.
        let floor = '';
        if (!known) {
            const base = info ? info.bg : rgb(0x283126);
            let f = rise >= 0 ? lerp(base, rgb(0x6b6a4a), Math.min(rise * 0.12, 0.4)) : lerp(base, rgb(0x0b1216), Math.min(-rise * 0.14, 0.5));
            f = withAlpha(scale(f, 1 + facing * 0.2), 0.27 + Math.min(Math.abs(rise) * 0.03, 0.12));
            floor = css(f);
        }
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
            const code = s.tileRows[y]?.[x];
            const info = code ? terrainInfo(code) : undefined;
            return !!info && info.kind !== 'wall' && (s.visibilityRows[y]?.[x] ?? '0') !== '0';
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
        const contour = css(rgb(0xc9bf9a, 0.16)), contourOld = css(rgb(0xc9bf9a, 0.16 * 0.45));
        const ramp = css(rgb(0xd8b877, 0.34)), rampOld = css(rgb(0xd8b877, 0.34 * 0.45));
        const shadow = css(rgb(0x04070a, 0.4)), shadowOld = css(rgb(0x04070a, 0.4 * 0.45));
        const rim = css(rgb(0xe9d2a0, 0.55)), rimOld = css(rgb(0xe9d2a0, 0.55 * 0.45));
        for (let y = w.y0; y <= w.y1; ++y) {
            const row = s.tileRows[y];
            if (!row) continue;
            for (let x = w.x0; x <= Math.min(w.x1, row.length - 1); ++x)
                for (let d = 0; d < 2; ++d) {
                    const dx = d === 0 ? 1 : 0, dy = 1 - dx;
                    const nx = x + dx, ny = y + dy;
                    if (!seenOpen(x, y) || !seenOpen(nx, ny)) continue;
                    const infoA = terrainInfo(row[x])!, infoB = terrainInfo(s.tileRows[ny][nx])!;
                    const ha = s.heightAt(x, y), hb = s.heightAt(nx, ny), drop = Math.abs(ha - hb);
                    const cliff = infoA.kind === 'cliff' || infoB.kind === 'cliff';
                    if (drop < 0.01 && !cliff) continue;
                    const remembered = s.visibilityRows[y][x] === '1' || s.visibilityRows[ny][nx] === '1';
                    const cx = ox + nx * tile, cy = oy + ny * tile;
                    const end: Point = dx ? [cx, cy + tile] : [cx + tile, cy];
                    if (drop <= 0.5 && !cliff) line([cx, cy], end, remembered ? contourOld : contour, 1);
                    else if (drop <= 1.01 && (infoA.ramp || infoB.ramp) && !cliff) line([cx, cy], end, remembered ? rampOld : ramp, 1.3);
                    else {
                        // The shadow falls on the lower side; a level cliff edge shades its open side.
                        const lowAfter = hb < ha || (drop < 0.01 && infoA.kind === 'cliff');
                        const band = Math.min(6, tile * 0.28);
                        c.fillStyle = remembered ? shadowOld : shadow;
                        c.fillRect(lowAfter ? cx : cx - dx * band, lowAfter ? cy : cy - dy * band, dx ? band : tile, dx ? tile : band);
                        line([cx, cy], end, remembered ? rimOld : rim, 2.2);
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
            s.cellWidth, s.cellHeight].join('|');
        const inside = view.x0 >= this.region.x0 && view.y0 >= this.region.y0 && view.x1 <= this.region.x1 && view.y1 <= this.region.y1;
        if (key !== this.key || !inside || this.rows !== s.tileRows || this.visibility !== s.visibilityRows || this.heights !== s.tileHeights) {
            if (!this.rebuild(s, view, tile, k)) {
                this.key = '';
                this.ground.paint(c, s, view, ox, oy, tile);
                return;
            }
            this.key = key;
            this.rows = s.tileRows;
            this.visibility = s.visibilityRows;
            this.heights = s.tileHeights;
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
        const region: TileWindow = {x0: Math.max(0, view.x0 - Margin), y0: Math.max(0, view.y0 - Margin),
            x1: Math.min(s.cellWidth - 1, view.x1 + Margin), y1: Math.min(s.cellHeight - 1, view.y1 + Margin)};
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
