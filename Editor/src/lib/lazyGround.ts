// Ground that comes as it comes into view (Docs/Design/20-world-database.md, "Massive worlds", step 4). A world loaded
// lean holds each world cell's outline and a preview (every PREVIEW_STEP-th tile of every PREVIEW_STEP-th row); its
// real ground is fetched when the cell is drawn close enough to see it, a few cells at a time. Until then the cell
// holds plain placeholder ground, which nothing may change: an action that would is refused (blockedBy) and the
// ground is fetched. Edits others make there meanwhile are kept and made again once it arrives. Shared by Atlas and
// the Dungeon Master.
import type {Project} from '../model/model.mjs';

export const PREVIEW_STEP = 4;
/** Screen pixels per tile from which a cell in view needs its real ground; below this its preview is drawn. */
export const DETAIL_FROM = 1;
const HEIGHT_CODES = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+-*';   // -16 .. 16 by halves.
const PER_REQUEST = 16, IN_FLIGHT = 3;

/** A cell's ground as the host sends it: heights as rows of HEIGHT_CODES ('.' for none), or as an object. */
export interface SentGround { x: number; y: number; width: number; height: number; terrain: string[]; heightRows?: string[]; heights?: Record<string, number> }
export interface Ground { x: number; y: number; width: number; height: number; terrain: string[]; heights: Record<string, number> }
export type Fetch = (ids: string[]) => Promise<{seq?: number; cells: Record<string, SentGround>}>;
/** Hands arrived ground to the app; returns the IDs it took (a cell cut to another shape meanwhile is asked for again). */
export type Deliver = (cells: Map<string, Ground>, seq: number) => string[];

const previews = new Map<string, string[]>();          // Cells still waiting for their ground, with their preview.
const asked = new Set<string>(), queue = new Set<string>();
let fetcher: Fetch | null = null, deliver: Deliver | null = null, timer: ReturnType<typeof setTimeout> | null = null, inFlight = 0;
let generation = 0;                                    // A new world: answers for the old one are ignored.

export function decodeHeights(rows: string[]): Record<string, number> {
    const out: Record<string, number> = {};
    rows.forEach((row, y) => {
        for (let x = 0; x < row.length; x++) {
            const i = HEIGHT_CODES.indexOf(row[x]);
            if (i >= 0) out[`${x},${y}`] = (i - 32) / 2 + 0;
        }
    });
    return out;
}

const blanks = new Map<number, string>();
function placeholder(width: number, height: number): string[] {
    let row = blanks.get(width);
    if (row === undefined) blanks.set(width, row = '.'.repeat(width));
    return Array(height).fill(row);
}

/**
 * Takes a lean world as the host sent it (in place): each cell without ground gets placeholder ground and waits for
 * its own. Everything waiting from an earlier world is forgotten.
 */
export function adoptLean<T extends {cells: {id: string; width: number; height: number; terrain: unknown; heights: unknown; preview?: string[]}[]}>(value: T): T {
    resetGround();
    for (const c of value.cells) {
        if (c.terrain === null || c.terrain === undefined) {
            previews.set(c.id, Array.isArray(c.preview) ? c.preview : []);
            c.terrain = placeholder(c.width, c.height);
            c.heights = {};
        }
        delete c.preview;
    }
    return value;
}

/** Where to fetch ground from, and what to do with it when it comes. */
export function startGround(fetch: Fetch, onArrive: Deliver) { fetcher = fetch; deliver = onArrive; }
export function resetGround() {
    generation++;
    previews.clear(); asked.clear(); queue.clear(); late.length = 0;
    if (timer) clearTimeout(timer);
    timer = null;
}

export const groundPending = (id: string) => previews.has(id);
export const anyPending = () => previews.size > 0;
export const previewOf = (id: string) => previews.get(id);
/** A cell whose ground is no longer known here (someone re-cut the ground it came from): it is fetched again. */
export function markPending(id: string) { previews.set(id, previews.get(id) ?? []); asked.delete(id); }
export function settleCell(id: string) { previews.delete(id); asked.delete(id); queue.delete(id); }

/** Asks for a waiting cell's ground (batched with others asked for about now). */
export function want(id: string) {
    if (!previews.has(id) || asked.has(id) || queue.has(id) || !fetcher) return;
    queue.add(id);
    timer ??= setTimeout(flush, 30);
}

/** Fetches every waiting cell's ground; resolves once none is waiting (for a whole-world copy, say). */
export function allGround(timeoutMs = 300_000): Promise<void> {
    for (const id of previews.keys()) want(id);
    const started = Date.now();
    return new Promise((resolve, reject) => {
        const check = () => {
            if (!previews.size) resolve();
            else if (Date.now() - started > timeoutMs) reject(new Error('The rest of the world\'s ground did not arrive; try again.'));
            else { for (const id of previews.keys()) want(id); setTimeout(check, 150); }
        };
        check();
    });
}

function flush() {
    timer = null;
    while (queue.size && inFlight < IN_FLIGHT && fetcher) {
        const ids = [...queue].slice(0, PER_REQUEST), mine = generation;
        for (const id of ids) { queue.delete(id); asked.add(id); }
        inFlight++;
        fetcher(ids).then(answer => {
            if (mine !== generation) return;
            const arrived = new Map<string, Ground>();
            for (const [id, g] of Object.entries(answer.cells ?? {}))
                arrived.set(id, {x: g.x, y: g.y, width: g.width, height: g.height, terrain: g.terrain,
                    heights: g.heightRows ? decodeHeights(g.heightRows) : g.heights ?? {}});
            const taken = new Set(deliver ? deliver(arrived, answer.seq ?? 0) : []);
            for (const id of ids) if (!taken.has(id)) asked.delete(id);   // Not taken: asked for again when next drawn.
        }).catch(() => {
            if (mine !== generation) return;
            setTimeout(() => { for (const id of ids) { asked.delete(id); want(id); } }, 3000);
        }).finally(() => {
            inFlight--;
            if (queue.size) timer ??= setTimeout(flush, 30);
        });
    }
}

/** The project as the host should see it: cells still waiting for their ground are sent without it (it fills them in). */
export function forHost(project: Project): Project {
    if (!previews.size) return project;
    return {...project, cells: project.cells.map(c => (previews.has(c.id) ? {...c, terrain: null, heights: null} as unknown as typeof c : c))};
}

type Rect = {id: string; x: number; y: number; width: number; height: number};
const overlap = (a: Rect, x: number, y: number, w: number, h: number) => x < a.x + a.width && a.x < x + w && y < a.y + a.height && a.y < y + h;

/**
 * The waiting cells an action (the edits that turn `before` into `after`) would change or rely on: ground in them, a
 * cell re-cut over them, or something placed in them. Empty if it touches none.
 */
export function blockedBy(before: Project, after: Project, ops: {key: string; before?: unknown; after: unknown}[]): string[] {
    if (!previews.size) return [];
    const waiting = [...before.cells, ...after.cells].filter(c => previews.has(c.id));
    const found = new Set<string>();
    const hit = (x: number, y: number, w: number, h: number) => { for (const c of waiting) if (overlap(c, x, y, w, h)) found.add(c.id); };
    for (const op of ops) {
        const colon = op.key.indexOf(':'), kind = colon < 0 ? op.key : op.key.slice(0, colon), rest = op.key.slice(colon + 1);
        if (kind === 'tile' || kind === 'height') {
            const [x, y] = rest.split(',').map(Number);
            hit(x, y, 1, 1);
        } else if (kind === 'cell') {
            const a = before.cells.find(c => c.id === rest), b = after.cells.find(c => c.id === rest);
            const reshaped = !a || !b || a.x !== b.x || a.y !== b.y || a.width !== b.width || a.height !== b.height;
            if (reshaped) for (const c of [a, b]) if (c) hit(c.x, c.y, c.width, c.height);
        } else {
            const text = JSON.stringify(op.after ?? null);       // Where things go (taking them away needs no ground).
            for (const id of previews.keys()) if (text.includes(`"cell":${JSON.stringify(id)}`)) found.add(id);
        }
    }
    return [...found];
}

// Others' ground edits that landed in waiting cells, kept to be made again once the real ground arrives (it may be
// older than they are).
interface Late { seq: number; key: string; after: unknown; x: number; y: number }
const late: Late[] = [];
const LATE_KEPT = 200_000;

/**
 * Others' edits have just been applied to `after` (which was `before`): keeps their ground edits in waiting cells, and
 * marks as waiting any cell now cut over ground that was still waiting (its ground here is only placeholder).
 */
export function noteRemote(before: Project, after: Project, changes: {seq: number; key: string; after: unknown}[]) {
    if (!previews.size) return;
    const waitingBefore = before.cells.filter(c => previews.has(c.id));
    for (const c of changes) {
        const colon = c.key.indexOf(':'), kind = c.key.slice(0, colon), rest = c.key.slice(colon + 1);
        if (kind === 'tile' || kind === 'height') {
            const [x, y] = rest.split(',').map(Number);
            if (waitingBefore.some(w => overlap(w, x, y, 1, 1)) || after.cells.some(w => previews.has(w.id) && overlap(w, x, y, 1, 1)))
                late.push({seq: c.seq, key: c.key, after: c.after, x, y});
        } else if (kind === 'cell') {
            const now = after.cells.find(x => x.id === rest);
            if (!now) { if (previews.has(rest)) settleCell(rest); continue; }
            const was = before.cells.find(x => x.id === rest);
            const same = was && was.x === now.x && was.y === now.y && was.width === now.width && was.height === now.height;
            if (!same && waitingBefore.some(w => overlap(w, now.x, now.y, now.width, now.height))) markPending(now.id);
        }
    }
    if (late.length > LATE_KEPT) late.splice(0, late.length - LATE_KEPT);
}

/** The kept edits to make again on ground that has just arrived as of edit `seq`, in these cells; forgets them. */
export function lateFor(cells: Rect[], seq: number): {key: string; after: unknown}[] {
    const out: {key: string; after: unknown}[] = [];
    for (let i = late.length - 1; i >= 0; i--) {
        const l = late[i];
        if (!cells.some(c => overlap(c, l.x, l.y, 1, 1))) continue;
        if (l.seq > seq) out.push({key: l.key, after: l.after});
        late.splice(i, 1);
    }
    return out.reverse();
}
