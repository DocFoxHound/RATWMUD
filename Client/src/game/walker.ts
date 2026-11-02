// The player's own wolf, walked by the page (Docs/Design/31-responsiveness.md, Phase 3): the server's own walking
// rules (Core/RatwStep.h), compiled to WebAssembly (Client/wasm/walk.cpp, built by tools/build_wasm.sh), run on the
// tiles this client holds. The server checks every pose it is sent and corrects any it can't accept.
import {TERRAIN} from './terrain.generated.mjs';
import {bandSpot} from './fringe.ts';

// The catalog by character code (all ASCII), for filling the grid: a cell and its fringe are a hundred thousand tiles.
const Unknown = {cost: 1, stature: 0, hides: 1, solid: false, ramp: false, opaque: false};
const ByCharCode = Array.from({length: 128}, (_, code) => TERRAIN.find(t => t.code.charCodeAt(0) === code) ?? Unknown);

export interface WalkStep {
    x: number;
    y: number;
    blocked: boolean;
}

interface Exports {
    memory: WebAssembly.Memory;
    _initialize: () => void;
    walk_grid: (w: number, h: number) => number;
    walk_result: () => number;
    walk_step: (x: number, y: number, ix: number, iy: number, flat: number, crouching: number, environment: number, dt: number) => void;
    walk_passable: (x: number, y: number, fx: number, fy: number) => number;
    walk_sight: (x: number, y: number, range: number) => number;
}

/** A door as the snapshot lists it. */
export interface DoorState {
    x: number;
    y: number;
    open: boolean;
}

/** The ground round the cell (doc 70; fringe.ts): its band's width, rows of glyphs and heights. */
export interface FringeGround {
    band: number;
    rows: string[];
    heights: Float32Array[];
}

const TileBytes = 20;

export class Walker {
    private readonly wasm: Exports;
    private width = 0;
    private height = 0;
    /** The fringe's width: the grid is the cell with that much of the ground round it, the cell's (0, 0) at (band, band). */
    band = 0;

    private constructor(wasm: Exports) {
        this.wasm = wasm;
    }

    /** The module, from its bytes (Node) or its URL (the page). Null if WebAssembly can't run it here. */
    static async load(source: ArrayBuffer | Uint8Array | string): Promise<Walker | null> {
        try {
            const bytes = typeof source === 'string' ? await (await fetch(source)).arrayBuffer() : source;
            // The few system calls the standard library might make (only to report a fatal error) do nothing here.
            const quiet = () => 0;
            const imports = {
                env: {emscripten_notify_memory_growth: () => {}},
                wasi_snapshot_preview1: {fd_close: quiet, fd_write: quiet, fd_seek: quiet},
            };
            const {instance} = await WebAssembly.instantiate(bytes as BufferSource, imports);
            const wasm = instance.exports as unknown as Exports;
            wasm._initialize();
            return new Walker(wasm);
        } catch {
            return null;
        }
    }

    /**
     * The cell this client holds, as walking and sight see it (the caller says when it changed: GameState's cell
     * version), and the ground round it (doc 70): sight runs on over it, but the page walks only in the cell (the
     * server takes the wolf over a seam), so to walking it is solid.
     */
    setCell(width: number, height: number, rows: string[], heights: Float32Array, doors: DoorState[], blocked: Set<string> = new Set(),
        fringe: FringeGround | null = null) {
        const band = fringe && fringe.band > 0 ? fringe.band : 0;
        const gridWidth = width + 2 * band, gridHeight = height + 2 * band;
        this.width = gridWidth;
        this.height = gridHeight;
        this.band = band;
        const base = this.wasm.walk_grid(gridWidth, gridHeight);
        if (!base && gridWidth * gridHeight > 0) return;
        const floats = new Float32Array(this.wasm.memory.buffer, base, gridWidth * gridHeight * TileBytes / 4);
        const bytes = new Uint8Array(this.wasm.memory.buffer, base, gridWidth * gridHeight * TileBytes);
        const doorAt = new Set(doors.filter(d => !d.open).map(d => `${Math.floor(d.x)},${Math.floor(d.y)}`));
        for (let gy = 0; gy < gridHeight; ++gy) {
            const y = gy - band;
            const inRows = y >= 0 && y < height;
            const row = inRows ? rows[y] ?? '' : '';
            for (let gx = 0; gx < gridWidth; ++gx) {
                const x = gx - band;
                const inCell = inRows && x >= 0 && x < width;
                let code = 32, rise = 0;
                if (inCell) {
                    code = row.charCodeAt(x) || 32;
                    rise = heights[y * width + x] ?? 0;
                } else if (fringe) {
                    const at = bandSpot(band, width, height, x, y);
                    if (at) {
                        code = fringe.rows[at[0]]?.charCodeAt(at[1]) || 32;
                        rise = fringe.heights[at[0]]?.[at[1]] ?? 0;
                    }
                }
                const terrain = ByCharCode[code & 127];
                const i = gy * gridWidth + gx, f = i * TileBytes / 4, b = i * TileBytes;
                floats[f] = rise;
                floats[f + 1] = terrain.cost;
                floats[f + 2] = terrain.stature;
                floats[f + 3] = terrain.hides;
                // (A Chapter's structure stands in the way: doc 32, 5.7. Past the edge, the server walks the wolf.)
                bytes[b + 16] = terrain.solid || !inCell || blocked.has(`${x},${y}`) ? 1 : 0;
                bytes[b + 17] = terrain.ramp ? 1 : 0;
                bytes[b + 18] = inCell && doorAt.has(`${x},${y}`) ? 1 : 0;
                bytes[b + 19] = terrain.opaque ? 1 : 0;
            }
        }
    }

    /** One step of `dt` seconds heading (ix, iy), as the server's walking takes it. */
    step(x: number, y: number, ix: number, iy: number, flatSpeed: number, crouching: boolean, environment: number, dt: number): WalkStep {
        const band = this.band;
        this.wasm.walk_step(x + band, y + band, ix, iy, flatSpeed, crouching ? 1 : 0, environment, dt);
        const out = new Float64Array(this.wasm.memory.buffer, this.wasm.walk_result(), 3);
        return {x: out[0] - band, y: out[1] - band, blocked: out[2] !== 0};
    }

    passable(x: number, y: number, fromX: number, fromY: number): boolean {
        const band = this.band;
        return this.wasm.walk_passable(x + band, y + band, fromX + band, fromY + band) !== 0;
    }

    /**
     * What a wolf at (x, y) with this sight range sees: 1 a tile it sees, as the server works it out, over the grid
     * (the cell and its fringe: the cell's (0, 0) is at (band, band)).
     */
    sight(x: number, y: number, range: number): Uint8Array {
        const base = this.wasm.walk_sight(x + this.band, y + this.band, range);
        return new Uint8Array(this.wasm.memory.buffer, base, this.width * this.height).slice();
    }
}
