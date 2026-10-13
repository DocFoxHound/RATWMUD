// The Road tool (the user, 2026-10-12): roads, streets, paths and bridges, the catalog's ways, are what NPCs keep to
// when they walk overland. The game reads the catalog's `way` flag by itself (roadTerrain), so a way laid is a way
// preferred: there is nothing more to do. This lays one between the points clicked, finding its own line: round walls,
// trees and buildings, up and down only what can be walked (the game's step rule, M.canStep), gently over the slopes,
// joining the ways already there, avoiding fields and doorways, and bridging water where asked. It paints only open
// ground: buildings, yards' furniture and the ways already there (their cobbles, say) are left as they are.
import * as M from '../model/model.mjs';
import type {RoadTile} from '../model/model.mjs';
import type {Tile} from './geometry';

/** What the router reads of the ground: the glyph (undefined off it) and height override at each tile. */
export interface Ground {
    glyph: (x: number, y: number) => string | undefined;
    heightAt: (x: number, y: number) => number | undefined;
}
export interface RoadOptions {
    glyph: string;          // The way to lay (one of M.WAYS).
    width: number;          // Tiles across, 1 to 5.
    bridges: boolean;       // Cross water on a bridge (else go round it).
}
export interface RoadPlan {
    route: Tile[];          // The middle of the road, from the first point to the last.
    tiles: RoadTile[];      // What to lay (surface tiles): the way, and bridges with their decks' height.
    bridges: number;        // Of those, bridge tiles.
    problem?: string;       // Why no road could be found (route and tiles then empty).
}

const BRIDGE = '8';
const MAX_NODES = 1_500_000;                 // A search looks at most this many tiles (a few seconds at worst).
const MARGIN = 48;                           // How far round the straight line a road may wander, at the least.

const height = (g: Ground, glyph: string, x: number, y: number) => {
    const h = g.heightAt(x, y);
    return h !== undefined ? M.roundHeight(h) : M.defaultHeight(glyph);
};
const info = (glyph: string) => M.TERRAIN_BY_CODE.get(glyph);
const isWater = (glyph: string) => info(glyph)?.kind === 'water';
/** Open ground a road may be laid on: ground and nature, walkable, not water, not a way already. */
export function paintable(glyph: string | undefined): boolean {
    const t = glyph === undefined ? undefined : info(glyph);
    return !!t && !t.solid && t.kind !== 'water' && !t.way && (t.category === 'ground' || t.category === 'nature');
}

/** What a step onto a tile costs the road (per tile): ways already there cheapest, open ground by how rough it is, and
 *  dear what it shouldn't cross (fields, doorways, furniture); null where it can't go. */
function stepCost(glyph: string, bridges: boolean): number | null {
    const t = info(glyph);
    if (!t) return null;
    if (t.way) return .5;
    if (t.kind === 'water') return bridges ? 4 : null;     // A bridge's span: dear, so short crossings are chosen.
    if (t.solid) return null;
    if (glyph === '4') return 3;                         // A crop field: round it if that's not far.
    if (t.category === 'ground' || t.category === 'nature') return t.cost;
    return 3;                                            // A doorway, an archway, stairs: through only if it must.
}

/** The road's middle between two points: A* over the ground, 8-connected (a diagonal only where both tiles beside it
 *  can be stepped on too), in a box round the two. Over water (bridges allowed) the deck carries the bank's height on. */
function route(g: Ground, from: Tile, to: Tile, bridges: boolean): {path: Tile[]; decks: Map<string, number>} | string {
    const span = Math.max(Math.abs(to[0] - from[0]), Math.abs(to[1] - from[1]));
    const margin = Math.max(MARGIN, Math.ceil(span * .6));
    const x0 = Math.min(from[0], to[0]) - margin, y0 = Math.min(from[1], to[1]) - margin;
    const w = Math.abs(to[0] - from[0]) + 2 * margin + 1, h = Math.abs(to[1] - from[1]) + 2 * margin + 1;
    if (w * h > 16_000_000) return 'Those points are too far apart for one stretch: add a point between them.';
    const n = w * h, index = (x: number, y: number) => (y - y0) * w + (x - x0);
    const inBox = (x: number, y: number) => x >= x0 && y >= y0 && x < x0 + w && y < y0 + h;
    const gScore = new Float32Array(n).fill(Infinity), previous = new Int32Array(n).fill(-1);
    const deck = new Float32Array(n).fill(NaN);          // A water tile's deck height, carried from the bank.
    const closed = new Uint8Array(n);
    // A binary heap of [f, node].
    const heapF: number[] = [], heapN: number[] = [];
    const push = (f: number, node: number) => {
        let i = heapF.length;
        heapF.push(f); heapN.push(node);
        while (i > 0) {
            const p = (i - 1) >> 1;
            if (heapF[p] <= f) break;
            heapF[i] = heapF[p]; heapN[i] = heapN[p]; i = p;
        }
        heapF[i] = f; heapN[i] = node;
    };
    const pop = () => {
        const top = heapN[0], lastF = heapF.pop()!, lastN = heapN.pop()!;
        if (heapF.length) {
            let i = 0;
            for (;;) {
                const l = 2 * i + 1, r = l + 1;
                let m = i, mf = lastF;
                if (l < heapF.length && heapF[l] < mf) { m = l; mf = heapF[l]; }
                if (r < heapF.length && heapF[r] < mf) { m = r; mf = heapF[r]; }
                if (m === i) break;
                heapF[i] = heapF[m]; heapN[i] = heapN[m]; i = m;
            }
            heapF[i] = lastF; heapN[i] = lastN;
        }
        return top;
    };
    const startGlyph = g.glyph(from[0], from[1]), endGlyph = g.glyph(to[0], to[1]);
    if (startGlyph === undefined || endGlyph === undefined) return 'Both ends must be on the ground of a cell.';
    if (stepCost(startGlyph, bridges) === null || stepCost(endGlyph, bridges) === null) return 'A road can’t start or end on a wall, a tree or water.';
    const estimate = (x: number, y: number) => {
        const dx = Math.abs(x - to[0]), dy = Math.abs(y - to[1]);
        return .5 * (Math.max(dx, dy) + (Math.SQRT2 - 1) * Math.min(dx, dy));   // Never more than the cheapest way.
    };
    // Where a walker would stand on a tile, for the step rule: a water tile at its deck's height, as a bridge.
    const standing = (node: number, x: number, y: number, glyph: string): [string, number] =>
        isWater(glyph) ? [BRIDGE, deck[node]] : [glyph, height(g, glyph, x, y)];
    const start = index(from[0], from[1]), end = index(to[0], to[1]);
    gScore[start] = 0;
    if (isWater(startGlyph)) deck[start] = height(g, startGlyph, from[0], from[1]);
    push(estimate(from[0], from[1]), start);
    let looked = 0;
    while (heapF.length) {
        const node = pop();
        if (closed[node]) continue;
        if (node === end) break;
        closed[node] = 1;
        if (++looked > MAX_NODES) return 'No road found within reach: add a point between them, or clear the way.';
        const x = node % w + x0, y = Math.floor(node / w) + y0, glyph = g.glyph(x, y)!;
        const [hereGlyph, hereHeight] = standing(node, x, y, glyph);
        for (let dy = -1; dy <= 1; dy++)
            for (let dx = -1; dx <= 1; dx++) {
                if (!dx && !dy) continue;
                const nx = x + dx, ny = y + dy;
                if (!inBox(nx, ny)) continue;
                const next = index(nx, ny);
                if (closed[next]) continue;
                const nextGlyph = g.glyph(nx, ny);
                if (nextGlyph === undefined) continue;
                const cost = stepCost(nextGlyph, bridges);
                if (cost === null) continue;
                const water = isWater(nextGlyph);
                const nextDeck = water ? (isWater(glyph) ? deck[node] : hereHeight) : NaN;
                const [toGlyph, toHeight] = water ? [BRIDGE, nextDeck] : [nextGlyph, height(g, nextGlyph, nx, ny)];
                if (!M.canStep(hereGlyph, hereHeight, toGlyph, toHeight)) continue;
                if (dx && dy) {
                    // Both tiles beside the diagonal must be walkable from here as well.
                    let ok = true;
                    for (const [sx, sy] of [[nx, y], [x, ny]] as Tile[]) {
                        const sg = g.glyph(sx, sy);
                        if (sg === undefined || stepCost(sg, bridges) === null) { ok = false; break; }
                        const [aside, asideHeight] = isWater(sg) ? [BRIDGE, water ? nextDeck : hereHeight] : [sg, height(g, sg, sx, sy)];
                        if (!M.canStep(hereGlyph, hereHeight, aside, asideHeight)) { ok = false; break; }
                    }
                    if (!ok) continue;
                }
                const through = gScore[node] + (dx && dy ? Math.SQRT2 : 1) * cost + Math.abs(toHeight - hereHeight) * 2;
                if (through < gScore[next]) {
                    gScore[next] = through;
                    previous[next] = node;
                    deck[next] = nextDeck;
                    push(through + estimate(nx, ny), next);
                }
            }
    }
    if (previous[end] < 0 && start !== end) return 'No road can be found between those points: walls, cliffs or water are in the way.';
    const path: Tile[] = [];
    const decks = new Map<string, number>();
    for (let i = end; i !== -1; i = i === start ? -1 : previous[i]) {
        const t: Tile = [i % w + x0, Math.floor(i / w) + y0];
        path.push(t);
        if (!Number.isNaN(deck[i])) decks.set(t.join(), deck[i]);
    }
    path.reverse();
    return {path, decks};
}

/** A road through the points (at least two), as wide as asked: its middle, and the tiles to lay. */
export function planRoad(g: Ground, points: Tile[], options: RoadOptions): RoadPlan {
    const width = Math.max(1, Math.min(5, Math.round(options.width)));
    if (!M.WAYS.has(options.glyph)) return {route: [], tiles: [], bridges: 0, problem: 'Choose a road, street or path to lay.'};
    if (points.length < 2) return {route: [], tiles: [], bridges: 0};
    const middle: Tile[] = [];
    const decks = new Map<string, number>();
    for (let i = 0; i + 1 < points.length; i++) {
        const found = route(g, points[i], points[i + 1], options.bridges);
        if (typeof found === 'string') return {route: [], tiles: [], bridges: 0, problem: found};
        middle.push(...(i ? found.path.slice(1) : found.path));
        found.decks.forEach((h, k) => decks.set(k, h));
    }
    // A diagonal step gets a tile beside it, so even a one-tile road is joined edge to edge; then the road's width
    // round each tile of its middle.
    const spine: Tile[] = [];
    for (let i = 0; i < middle.length; i++) {
        spine.push(middle[i]);
        const next = middle[i + 1];
        if (next && next[0] !== middle[i][0] && next[1] !== middle[i][1]) {
            const aside: Tile = [next[0], middle[i][1]];
            const g1 = g.glyph(aside[0], aside[1]);
            spine.push(g1 !== undefined && (paintable(g1) || M.WAYS.has(g1) || (options.bridges && isWater(g1))) ? aside : [middle[i][0], next[1]]);
        }
    }
    const lay = new Map<string, RoadTile>();
    let bridges = 0;
    const start = -Math.floor((width - 1) / 2);
    for (const [cx, cy] of spine) {
        const deckHere = decks.get(`${cx},${cy}`);
        for (let dy = 0; dy < width; dy++)
            for (let dx = 0; dx < width; dx++) {
                const x = cx + start + dx, y = cy + start + dy, key = `${x},${y}`;
                if (lay.has(key)) continue;
                const glyph = g.glyph(x, y);
                if (glyph === undefined) continue;
                if (paintable(glyph)) lay.set(key, {x, y, glyph: options.glyph});
                else if (options.bridges && isWater(glyph)) {
                    // The deck: level with the bank the bridge set out from (the nearest of the road's middle).
                    const h = deckHere ?? decks.get(key) ?? nearestDeck(decks, x, y);
                    if (h === undefined) continue;
                    lay.set(key, {x, y, glyph: BRIDGE, height: h});
                    bridges++;
                }
            }
    }
    return {route: middle, tiles: [...lay.values()], bridges};
}

function nearestDeck(decks: Map<string, number>, x: number, y: number): number | undefined {
    let best: number | undefined, dist = Infinity;
    for (const [k, h] of decks) {
        const [dx, dy] = k.split(',').map(Number);
        const d = Math.abs(dx - x) + Math.abs(dy - y);
        if (d < dist) { dist = d; best = h; }
    }
    return dist <= 3 ? best : undefined;
}
