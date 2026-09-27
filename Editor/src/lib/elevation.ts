// Elevation made visible: a hypsometric tint with hillshade (light from the north-west), contour strokes on tile
// edges styled by whether a walker can cross them, cliff hatching and height labels. Pure drawing helpers, shared
// by Atlas (MapCanvas) and the Dungeon Master (MapView); only tiles in view are drawn, and each cell's heights are
// unpacked once into a typed grid until the cell changes.
//
// The rules are the game's (see model.mjs edgeKind): a height change of at most ½ is a free step, exactly 1 is
// walkable only with a ramp (catalog `ramp`: `:` Slope, `^` Stairs) on either side, anything more is a ledge. Cliff
// tiles (catalog kind `cliff`, `%`) are solid and drawn with a heavy face and hatching. Default heights come from the catalog.
import * as M from '../model/model.mjs';
import type {AnyCell, EdgeKind} from '../model/model.mjs';
import type {Surface} from './surface';

export interface Camera { s: number; x: number; y: number }

/** Off; shading (tint + hillshade + impassable ledges and cliffs); full (also every contour and height labels). */
export type ElevationMode = 'off' | 'shade' | 'full';
export const ELEVATION_MODES: ElevationMode[] = ['off', 'shade', 'full'];
export const ELEVATION_LABEL: Record<ElevationMode, string> = {off: 'Relief off', shade: 'Relief: shading', full: 'Relief: full'};
export const nextElevation = (m: ElevationMode): ElevationMode => ELEVATION_MODES[(ELEVATION_MODES.indexOf(m) + 1) % ELEVATION_MODES.length];

export {edgeKind, canStep, defaultHeight, roundHeight} from '../model/model.mjs';
export type {EdgeKind};

// Stroke colours, also used by the legend.
export const RELIEF = {
    step: 'rgba(236,228,206,.34)',     // A free ½ step: thin.
    slope: '#d9b67b',                  // A full step made walkable by a Slope or Stairs: medium, dashed.
    ledge: '#ff7a45',                  // Impassable: heavy, on the high side.
    cliff: '#ff7a45',
    wall: 'rgba(236,228,206,.22)',     // Height changes along walls and furniture (solid anyway).
    low: '#4f86c6', high: '#f0cf8e',
};

const CLIFF = new Set(M.TERRAIN.filter(t => t.kind === 'cliff').map(t => t.code));
/** Cliff tiles: solid rock faces, outlined and hatched by the relief. */
export const isCliff = (glyph: string | null | undefined) => glyph != null && CLIFF.has(glyph);

/** A tile's height: its override (rounded to half steps, for legacy data) or its glyph's default. */
export const tileHeight = (glyph: string | null | undefined, override: number | undefined) =>
    override !== undefined ? M.roundHeight(override) : M.defaultHeight(glyph);

/** "1½", "−1", "½" (no sign for positive heights). */
export function formatHeight(h: number): string {
    const r = M.roundHeight(h), a = Math.abs(r), whole = Math.floor(a), half = a - whole >= .5;
    return (r < 0 ? '−' : '') + (whole || !half ? String(whole) : '') + (half ? '½' : '');
}

/** The height at a surface tile (override or glyph default), or undefined off the ground. */
export function surfaceHeight(surface: Surface, x: number, y: number): number | undefined {
    const glyph = surface.glyph(x, y);
    return glyph === undefined ? undefined : tileHeight(glyph, surface.heightAt(x, y));
}

// --------------------------------------------------------------------------- Height grids

interface Grid { terrain: string[]; heights: Record<string, number>; h: Float32Array }
const grids = new WeakMap<AnyCell, Grid>();

/** The cell's effective heights, row-major (width × height), cached until its ground changes. */
export function heightGrid(c: AnyCell): Float32Array {
    const old = grids.get(c);
    if (old && old.terrain === c.terrain && old.heights === c.heights && old.h.length === c.width * c.height) return old.h;
    const h = new Float32Array(c.width * c.height);
    for (let y = 0; y < c.height; y++) {
        const row = c.terrain[y] ?? '';
        for (let x = 0; x < c.width; x++) { const d = M.defaultHeight(row[x]); if (d) h[y * c.width + x] = d; }
    }
    for (const [k, v] of Object.entries(c.heights)) {
        const comma = k.indexOf(','), x = Number(k.slice(0, comma)), y = Number(k.slice(comma + 1));
        if (x >= 0 && y >= 0 && x < c.width && y < c.height && typeof v === 'number' && Number.isFinite(v)) h[y * c.width + x] = M.roundHeight(v);
    }
    grids.set(c, {terrain: c.terrain, heights: c.heights, h});
    return h;
}

// --------------------------------------------------------------------------- Tint and hillshade

const tints = new Map<number, string | null>(), lights = new Map<number, string | null>();

/** Hypsometric tint: low ground cool and dark, high ground warm and light; none at 0. Subtle, so glyphs read. */
export function tintStyle(h: number): string | null {
    const key = Math.round(h * 2);
    let style = tints.get(key);
    if (style === undefined) {
        const t = Math.min(1, Math.sqrt(Math.abs(h) / 8));        // ½ → .25, 2 → .5, 8 → 1.
        style = h === 0 ? null : h > 0
            ? `rgba(${Math.round(200 + 40 * t)},${Math.round(178 + 40 * t)},${Math.round(118 + 55 * t)},${(.08 + .3 * t).toFixed(3)})`
            : `rgba(${Math.round(40 - 20 * t)},${Math.round(80 - 30 * t)},${Math.round(150 - 30 * t)},${(.12 + .38 * t).toFixed(3)})`;
        tints.set(key, style);
    }
    return style;
}

/** Hillshade, lit from the north-west: from the height differences across the tile (west→east, north→south). */
export function hillshade(west: number, east: number, north: number, south: number): number {
    // Ground rising toward the south-east faces the north-west light.
    return Math.max(-1, Math.min(1, ((east - west) + (south - north)) * .35));
}
export function lightStyle(light: number): string | null {
    const key = Math.round(light * 8);
    let style = lights.get(key);
    if (style === undefined) {
        const v = key / 8;
        style = key === 0 ? null : v > 0 ? `rgba(255,244,214,${(.2 * v).toFixed(3)})` : `rgba(0,0,0,${(-.4 * v).toFixed(3)})`;
        lights.set(key, style);
    }
    return style;
}

/** Tint and light styles for tile x, y of a cell (neighbours beyond the cell count as level with it). */
export function shadeAt(c: AnyCell, grid: Float32Array, x: number, y: number): [string | null, string | null] {
    const w = c.width, i = y * w + x, h = grid[i];
    const west = x > 0 ? grid[i - 1] : h, east = x < w - 1 ? grid[i + 1] : h;
    const north = y > 0 ? grid[i - w] : h, south = y < c.height - 1 ? grid[i + w] : h;
    return [tintStyle(h), lightStyle(hillshade(west, east, north, south))];
}

// --------------------------------------------------------------------------- Contours, cliffs and labels

/** Draws contours, ledges, cliff hatching and height labels for the tiles in view (over the ground). */
export function drawRelief(g: CanvasRenderingContext2D, surface: Surface, cam: Camera, w: number, h: number, mode: ElevationMode) {
    const {s} = cam;
    if (mode === 'off' || s < 3) return;
    const full = mode === 'full';
    const x0v = Math.floor(-cam.x / s), y0v = Math.floor(-cam.y / s), x1v = Math.ceil((w - cam.x) / s), y1v = Math.ceil((h - cam.y) / s);
    const step = new Path2D(), slope = new Path2D(), ledge = new Path2D(), wall = new Path2D(), hatch = new Path2D();
    const lw = Math.max(2, Math.min(5, s * .16)), off = lw / 2;
    const labels: [number, number, number][] = [];
    const showMinor = full && s >= 5, showLabels = full && s >= 22, showHatch = s >= 6;
    /** Adds one tile edge. Vertical edges are at screen x = ex from ey to ey + s; horizontal at y = ey from ex. */
    const edge = (kind: EdgeKind, ga: string, ha: number, gb: string | undefined, hb: number, ex: number, ey: number, vertical: boolean) => {
        const cliffA = CLIFF.has(ga), cliffB = gb !== undefined && CLIFF.has(gb);
        if (gb === undefined && !cliffA) return;          // The ground's own border: only cliffs are outlined.
        if (kind === 'solid') {
            if (cliffA !== cliffB) {                         // A cliff face: heavy, on the cliff's side.
                const d = cliffA ? -off : off;
                if (vertical) { ledge.moveTo(ex + d, ey); ledge.lineTo(ex + d, ey + s); } else { ledge.moveTo(ex, ey + d); ledge.lineTo(ex + s, ey + d); }
            } else if (showMinor && ha !== hb) {
                if (vertical) { wall.moveTo(ex, ey); wall.lineTo(ex, ey + s); } else { wall.moveTo(ex, ey); wall.lineTo(ex + s, ey); }
            }
            return;
        }
        if (kind === 'flat') return;
        if (kind === 'ledge') {
            const d = ha > hb ? -off : off;               // The high side: tile A is left of / above the edge.
            if (vertical) { ledge.moveTo(ex + d, ey); ledge.lineTo(ex + d, ey + s); } else { ledge.moveTo(ex, ey + d); ledge.lineTo(ex + s, ey + d); }
            return;
        }
        if (!showMinor) return;
        const path = kind === 'slope' ? slope : step;
        if (vertical) { path.moveTo(ex, ey); path.lineTo(ex, ey + s); } else { path.moveTo(ex, ey); path.lineTo(ex + s, ey); }
    };
    for (const piece of surface.pieces(x0v, y0v, x1v, y1v)) {
        const c = piece.cell, grid = heightGrid(c), cw = c.width;
        const x0 = Math.max(0, x0v - piece.x), y0 = Math.max(0, y0v - piece.y);
        const x1 = Math.min(cw, x1v - piece.x), y1 = Math.min(c.height, y1v - piece.y);
        const sx = cam.x + piece.x * s, sy = cam.y + piece.y * s;
        for (let y = y0; y < y1; y++) {
            const row = c.terrain[y], below = y + 1 < c.height ? c.terrain[y + 1] : null;
            for (let x = x0; x < x1; x++) {
                const ga = row[x], ha = grid[y * cw + x], px = sx + x * s, py = sy + y * s;
                // East edge.
                let gb: string | undefined, hb = 0;
                if (x + 1 < cw) { gb = row[x + 1]; hb = grid[y * cw + x + 1]; }
                else if (surface.kind === 'world') { gb = surface.glyph(piece.x + x + 1, piece.y + y); if (gb !== undefined) hb = tileHeight(gb, surface.heightAt(piece.x + x + 1, piece.y + y)); }
                edge(M.edgeKind(ga, ha, gb, hb), ga, ha, gb, hb, px + s, py, true);
                // South edge.
                gb = undefined; hb = 0;
                if (below) { gb = below[x]; hb = grid[(y + 1) * cw + x]; }
                else if (surface.kind === 'world') { gb = surface.glyph(piece.x + x, piece.y + y + 1); if (gb !== undefined) hb = tileHeight(gb, surface.heightAt(piece.x + x, piece.y + y + 1)); }
                edge(M.edgeKind(ga, ha, gb, hb), ga, ha, gb, hb, px, py + s, false);
                // A cliff's west and north faces where no ground lies beyond (otherwise that tile draws the edge).
                if (CLIFF.has(ga) && x === 0 && (surface.kind !== 'world' || surface.glyph(piece.x - 1, piece.y + y) === undefined)) {
                    ledge.moveTo(px + off, py); ledge.lineTo(px + off, py + s);
                }
                if (CLIFF.has(ga) && y === 0 && (surface.kind !== 'world' || surface.glyph(piece.x + x, piece.y - 1) === undefined)) {
                    ledge.moveTo(px, py + off); ledge.lineTo(px + s, py + off);
                }
                if (CLIFF.has(ga) && showHatch) {
                    // Diagonal hatching across the cliff tile.
                    for (let k = .25; k < 2; k += .5) {
                        const a = Math.min(k, 1), b = Math.max(0, k - 1);
                        hatch.moveTo(px + a * s, py + b * s); hatch.lineTo(px + b * s, py + a * s);
                    }
                }
                // Labels mark where height changes (every non-zero tile once zoomed right in), so plateaus stay clean.
                if (showLabels && ha !== 0 && (s >= 40 || (x > 0 && grid[y * cw + x - 1] !== ha) || (x + 1 < cw && grid[y * cw + x + 1] !== ha)
                    || (y > 0 && grid[(y - 1) * cw + x] !== ha) || (y + 1 < c.height && grid[(y + 1) * cw + x] !== ha))) labels.push([px, py, ha]);
            }
        }
    }
    g.save();
    g.lineCap = 'butt';
    if (showHatch) { g.strokeStyle = 'rgba(255,122,69,.45)'; g.lineWidth = 1; g.stroke(hatch); }
    if (showMinor) {
        g.setLineDash([]); g.strokeStyle = RELIEF.wall; g.lineWidth = 1; g.stroke(wall);
        g.strokeStyle = RELIEF.step; g.lineWidth = 1; g.stroke(step);
        g.strokeStyle = RELIEF.slope; g.lineWidth = Math.max(1.5, lw * .6); g.setLineDash([Math.max(3, s * .22), Math.max(2, s * .14)]); g.stroke(slope);
        g.setLineDash([]);
    }
    g.strokeStyle = 'rgba(13,20,19,.7)'; g.lineWidth = lw + 2; g.stroke(ledge);     // A dark halo so ledges read on any ground.
    g.strokeStyle = RELIEF.ledge; g.lineWidth = lw; g.stroke(ledge);
    if (labels.length) {
        g.font = `600 ${Math.max(9, Math.round(s * .27))}px Inter, system-ui, sans-serif`; g.textAlign = 'left'; g.textBaseline = 'top';
        g.lineJoin = 'round'; g.lineWidth = 3; g.strokeStyle = 'rgba(13,20,19,.85)';
        for (const [px, py, hv] of labels) {
            const text = formatHeight(hv);
            g.strokeText(text, px + 2, py + 1);
            g.fillStyle = hv > 0 ? '#f3dcaa' : '#a9cbef'; g.fillText(text, px + 2, py + 1);
        }
    }
    g.restore();
}
