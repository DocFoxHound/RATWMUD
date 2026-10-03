// The player's own wolf, walked by the page (Docs/Design/31-responsiveness.md, Phase 3): the server's own walking
// rules (Core/RatwStep.h), compiled to WebAssembly (Client/wasm/walk.cpp, built by tools/build_wasm.sh), run on the
// tiles this client holds. The server checks every pose it is sent and corrects any it can't accept.
import {TERRAIN, type TerrainTile} from './terrain.generated.mjs';

const ByCode = new Map<string, TerrainTile>(TERRAIN.map(t => [t.code, t]));
const terrainByCode = (code: string) => ByCode.get(code);

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

const TileBytes = 16;

export class Walker {
    private readonly wasm: Exports;
    private width = 0;
    private height = 0;

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

    /** The cell this client holds, as walking sees it (the caller says when it changed: GameState's cell version). */
    setCell(width: number, height: number, rows: string[], heights: Float32Array, doors: DoorState[]) {
        const closed = doors.filter(d => !d.open).map(d => `${Math.floor(d.x)},${Math.floor(d.y)}`);
        this.width = width;
        this.height = height;
        const base = this.wasm.walk_grid(width, height);
        if (!base && width * height > 0) return;
        const view = new DataView(this.wasm.memory.buffer, base, width * height * TileBytes);
        const doorAt = new Set(closed);
        for (let y = 0; y < height; ++y) {
            const row = rows[y] ?? '';
            for (let x = 0; x < width; ++x) {
                const at = (y * width + x) * TileBytes;
                const terrain = terrainByCode(row[x] ?? ' ');
                view.setFloat32(at, heights[y * width + x] ?? 0, true);
                view.setFloat32(at + 4, terrain ? terrain.cost : 1, true);
                view.setFloat32(at + 8, terrain ? terrain.stature : 0, true);
                view.setUint8(at + 12, terrain?.solid ? 1 : 0);
                view.setUint8(at + 13, terrain?.ramp ? 1 : 0);
                view.setUint8(at + 14, doorAt.has(`${x},${y}`) ? 1 : 0);
                view.setUint8(at + 15, terrain?.opaque ? 1 : 0);
            }
        }
    }

    /** One step of `dt` seconds heading (ix, iy), as the server's walking takes it. */
    step(x: number, y: number, ix: number, iy: number, flatSpeed: number, crouching: boolean, environment: number, dt: number): WalkStep {
        this.wasm.walk_step(x, y, ix, iy, flatSpeed, crouching ? 1 : 0, environment, dt);
        const out = new Float64Array(this.wasm.memory.buffer, this.wasm.walk_result(), 3);
        return {x: out[0], y: out[1], blocked: out[2] !== 0};
    }

    passable(x: number, y: number, fromX: number, fromY: number): boolean {
        return this.wasm.walk_passable(x, y, fromX, fromY) !== 0;
    }

    /** What a wolf at (x, y) with this sight range sees of the cell: 1 a tile it sees, as the server works it out. */
    sight(x: number, y: number, range: number): Uint8Array {
        const base = this.wasm.walk_sight(x, y, range);
        return new Uint8Array(this.wasm.memory.buffer, base, this.width * this.height).slice();
    }
}
