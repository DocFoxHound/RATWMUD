// Tile-shape helpers for the painting tools. All coordinates are whole tiles.
export type Tile = [number, number];

export function square(x: number, y: number, size: number): Tile[] {
    const out: Tile[] = [], start = -Math.floor((size - 1) / 2);
    for (let dy = 0; dy < size; dy++) for (let dx = 0; dx < size; dx++) out.push([x + start + dx, y + start + dy]);
    return out;
}

export function line(x0: number, y0: number, x1: number, y1: number): Tile[] {
    const out: Tile[] = [];
    const dx = Math.abs(x1 - x0), dy = -Math.abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    let err = dx + dy, x = x0, y = y0;
    for (;;) {
        out.push([x, y]);
        if (x === x1 && y === y1) return out;
        const e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

export function rect(x0: number, y0: number, x1: number, y1: number, filled: boolean): Tile[] {
    const out: Tile[] = [];
    const [ax, bx] = [Math.min(x0, x1), Math.max(x0, x1)], [ay, by] = [Math.min(y0, y1), Math.max(y0, y1)];
    for (let y = ay; y <= by; y++) for (let x = ax; x <= bx; x++)
        if (filled || x === ax || x === bx || y === ay || y === by) out.push([x, y]);
    return out;
}

/** Contiguous same-glyph region (4-neighbour), bounded by the surface. */
/** The same-glyph area around x, y (4-connected), up to `limit` tiles; `glyph` is undefined where there is no ground. */
export function flood(glyph: (x: number, y: number) => string | undefined, x: number, y: number, limit = 70000): Tile[] {
    const target = glyph(x, y);
    if (target === undefined) return [];
    const seen = new Set<string>(), out: Tile[] = [], stack: Tile[] = [[x, y]];
    while (stack.length && out.length < limit) {
        const [cx, cy] = stack.pop()!, key = `${cx},${cy}`;
        if (seen.has(key) || glyph(cx, cy) !== target) continue;
        seen.add(key);
        out.push([cx, cy]);
        stack.push([cx + 1, cy], [cx - 1, cy], [cx, cy + 1], [cx, cy - 1]);
    }
    return out;
}
