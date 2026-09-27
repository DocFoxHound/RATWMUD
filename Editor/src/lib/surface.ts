// A "surface" is what the canvas is showing: the whole world, one world cell, or one detached interior. It
// translates between surface tiles and model places (cell + local tile).
//
// The world overview's surface tiles are world tiles: there is no canvas, only cells, which may lie anywhere and
// far apart. A cell's or an interior's surface tiles are its own 0-based tiles.
import * as M from '../model/model.mjs';
import type {AnyCell, Place, Project, Room, WorldCell} from '../model/model.mjs';
import type {View} from './store';

/** One cell or interior to draw, with where its tile 0,0 is on the surface. */
export interface Piece { cell: AnyCell; x: number; y: number }

export interface Surface {
    kind: 'world' | 'cell' | 'room';
    title: string;
    /** The surface's extent, in surface tiles: every cell for the world overview, 0,0 to width,height otherwise. */
    left: number;
    top: number;
    width: number;
    height: number;
    /** The glyph at a surface tile, or undefined where there is no ground (outside every cell). */
    glyph: (x: number, y: number) => string | undefined;
    heightAt: (x: number, y: number) => number | undefined;
    /** The cells or the interior with ground in the surface rectangle x0,y0 – x1,y1 (exclusive). */
    pieces: (x0: number, y0: number, x1: number, y1: number) => Piece[];
    /** The world cell under a surface tile (the cell itself on a cell's surface; null in an interior). */
    cellAt: (x: number, y: number) => WorldCell | null;
    /** Model paint target: null paints world tiles. */
    roomId: string | null;
    /** Added to surface tiles, these give the tiles to paint: world tiles, or the interior's own. */
    ox: number;
    oy: number;
    cell: AnyCell | null;
    toPlace: (x: number, y: number) => Place | null;
    fromPlace: (p: Place) => [number, number] | null;
}

export const isRoom = (c: AnyCell | null): c is Room => M.isInterior(c);

function single(c: AnyCell, kind: 'cell' | 'room', ox: number, oy: number, roomId: string | null): Surface {
    const inside = (x: number, y: number) => x >= 0 && y >= 0 && x < c.width && y < c.height;
    return {
        kind, title: c.name, left: 0, top: 0, width: c.width, height: c.height,
        glyph: (x, y) => (inside(x, y) ? c.terrain[y][x] : undefined),
        heightAt: (x, y) => c.heights[`${x},${y}`],
        pieces: () => [{cell: c, x: 0, y: 0}],
        cellAt: (x, y) => (kind === 'cell' && inside(x, y) ? c as WorldCell : null),
        roomId, ox, oy, cell: c,
        toPlace: (x, y) => (inside(x, y) ? {cell: c.id, x, y} : null),
        fromPlace: p => (p.cell === c.id ? [p.x, p.y] : null),
    };
}

export function surfaceFor(project: Project, view: View): Surface {
    if (view.kind === 'cell') {
        const room = project.rooms.find(r => r.id === view.id);
        if (room) return single(room, 'room', 0, 0, room.id);
        const c = project.cells.find(x => x.id === view.id);
        if (c) return single(c, 'cell', c.x, c.y, null);
    }
    const index = M.cellIndex(project.cells), byId = new Map(project.cells.map(c => [c.id, c]));
    const box = M.cellBounds(project) ?? {x: 0, y: 0, width: 16, height: 16};
    return {
        kind: 'world', title: project.name, left: box.x, top: box.y, width: box.width, height: box.height,
        glyph: (x, y) => { const c = index.at(x, y); return c ? c.terrain[y - c.y][x - c.x] : undefined; },
        heightAt: (x, y) => { const c = index.at(x, y); return c ? c.heights[`${x - c.x},${y - c.y}`] : undefined; },
        pieces: (x0, y0, x1, y1) => index.around(x0, y0, x1 - x0, y1 - y0).map(c => ({cell: c, x: c.x, y: c.y})),
        cellAt: (x, y) => index.at(x, y),
        roomId: null, ox: 0, oy: 0, cell: null,
        toPlace: (x, y) => { const c = index.at(x, y); return c ? {cell: c.id, x: x - c.x, y: y - c.y} : null; },
        fromPlace: p => { const c = byId.get(p.cell); return c ? [c.x + p.x, c.y + p.y] : null; },
    };
}

export function placeLabel(project: Project, p: Place | null): string {
    if (!p) return 'Not placed';
    const c = project.cells.find(x => x.id === p.cell) ?? project.rooms.find(x => x.id === p.cell);
    return `${c?.name ?? p.cell} · ${p.x}, ${p.y}`;
}
