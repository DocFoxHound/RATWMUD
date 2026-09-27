// Placing new world cells: preset sizes, and snapping to the cells already there so the world grows without gaps.
// Coordinates here are world tiles.
import * as M from '../model/model.mjs';
import type {Project} from '../model/model.mjs';

export interface CellSize { w: number; h: number }
/** Preset sizes: squares, and halves of each (wide and tall). All are multiples of 16, so they tile together. */
export const CELL_SIZES: {group: string; sizes: CellSize[]}[] = [
    {group: 'Square', sizes: [{w: 32, h: 32}, {w: 64, h: 64}, {w: 128, h: 128}, {w: 256, h: 256}]},
    {group: 'Wide', sizes: [{w: 32, h: 16}, {w: 64, h: 32}, {w: 128, h: 64}, {w: 256, h: 128}]},
    {group: 'Tall', sizes: [{w: 16, h: 32}, {w: 32, h: 64}, {w: 64, h: 128}, {w: 128, h: 256}]},
];
/** Without a neighbour to line up with, cells snap to this world grid (world tile 0,0 is on it). */
export const GRID = 16;

export interface Placement { x: number; y: number; w: number; h: number; ok: boolean; touching: boolean; problem: string }
interface Rect { x: number; y: number; width: number; height: number }

const overlaps = (a: Rect, b: Rect) => a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y;
/** Shares a stretch of edge with `b` (not just a corner). */
const touches = (a: Rect, b: Rect) =>
    ((a.x + a.width === b.x || b.x + b.width === a.x) && Math.min(a.y + a.height, b.y + b.height) > Math.max(a.y, b.y)) ||
    ((a.y + a.height === b.y || b.y + b.height === a.y) && Math.min(a.x + a.width, b.x + b.width) > Math.max(a.x, b.x));
const snapGrid = (v: number) => Math.round(v / GRID) * GRID;

/**
 * Where a w×h cell goes when the pointer is at world tile (px, py): centred on the pointer, then pulled against
 * the nearest edge of a nearby cell so the two meet with no gap. Only the axis that closes the gap moves; along the
 * shared edge the cell stays where the pointer puts it (it does not jump to line up with corners). Dropped into a
 * nook, it meets both neighbours. With no neighbour in reach it sits on the 16-tile world grid. Pointing into a
 * cell is refused.
 */
export function placeCell(project: Project, px: number, py: number, size: CellSize): Placement {
    const {w, h} = size;
    const x0 = px - Math.floor(w / 2), y0 = py - Math.floor(h / 2);
    const reach = Math.max(w, h);                                            // How far to look for neighbours.
    const pull = Math.max(4, Math.round(Math.min(w, h) / 3));                // How far an edge pulls the cell.
    const near = M.cellIndex(project.cells).around(x0 - reach, y0 - reach, w + 2 * reach, h + 2 * reach);
    // Positions that put this cell's side against a neighbour's: left or right of it, or above or below it.
    const xs = new Set<number>(), ys = new Set<number>();
    for (const c of near) {
        for (const x of [c.x - w, c.x + c.width]) if (Math.abs(x - x0) <= pull * 2) xs.add(x);
        for (const y of [c.y - h, c.y + c.height]) if (Math.abs(y - y0) <= pull * 2) ys.add(y);
    }
    const candidates: [number, number][] = [...[...xs].map(x => [x, y0] as [number, number]), ...[...ys].map(y => [x0, y] as [number, number])];
    for (const x of xs) for (const y of ys) candidates.push([x, y]);            // Into a nook: against both.
    let best: Placement | null = null, bestScore = Infinity;
    for (const [x, y] of candidates) {
        const r = {x, y, width: w, height: h};
        if (near.some(c => overlaps(r, c))) continue;
        const touching = near.filter(c => touches(r, c)).length;
        if (!touching) continue;
        const score = Math.abs(x - x0) + Math.abs(y - y0) - (touching - 1) * pull;
        if (score < bestScore) { bestScore = score; best = {x, y, w, h, ok: true, touching: true, problem: ''}; }
    }
    if (best) return best;
    const x = snapGrid(x0), y = snapGrid(y0);
    const clash = near.find(c => overlaps({x, y, width: w, height: h}, c));
    return clash ? {x, y, w, h, ok: false, touching: false, problem: `overlaps ${clash.name}`} : {x, y, w, h, ok: true, touching: false, problem: ''};
}
