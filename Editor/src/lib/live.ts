// Live, shared editing of the one world (host: tools/live_edit.py).
//
// Every action is turned into keyed edits: what changed, the value this editor
// saw before, and the value it wants now. They are sent in order and saved at
// once; a heartbeat every 1.5 s reports where this editor is and brings in
// everyone else's edits. If an edit collides with someone else's, the host
// refuses it whole and names them; the map then shows their version.
import * as M from '../model/model.mjs';
import type {Place, Project} from '../model/model.mjs';

export interface Op { key: string; before: unknown; after: unknown }
export interface Change { seq: number; clientId: string; editor: string; label: string; key: string; after: unknown }
export interface Editor { clientId: string; editor: string; color: string; idle: number; state: Presence }
export interface Presence { workspace?: string; view?: string; place?: Place | null; doing?: string }
export interface Identity { name: string; color: string }
export interface ConflictNotice {
    action: string;
    message: string;
    conflicts: {key: string; label: string; editor: string; at: string | null}[];
}

// --------------------------------------------------------------------------- Keys and values

/** JSON with sorted keys, so values built by the host and by this editor compare equal. */
export function stable(value: unknown): string {
    if (value === undefined) return 'null';
    if (value === null || typeof value !== 'object') return JSON.stringify(value);
    if (Array.isArray(value)) return `[${value.map(stable).join(',')}]`;
    return `{${Object.keys(value).sort().map(k => `${JSON.stringify(k)}:${stable((value as Record<string, unknown>)[k])}`).join(',')}}`;
}

const byId = <T extends {id: string}>(list: T[]) => new Map(list.map(x => [x.id, x]));
/** A cell or interior without its ground: what `cell:` and `room:` edits carry. */
const meta = <T extends {terrain: string[]; heights: Record<string, number>}>(c: T) => { const {terrain: _t, heights: _h, ...rest} = c; return rest; };
const ground = (g: string | undefined) => (g === undefined || g === '.' ? null : g);
const sameShape = (a: M.WorldCell, b: M.WorldCell) => a.x === b.x && a.y === b.y && a.width === b.width && a.height === b.height;

/** World tiles as a project's cells hold them: plain ground and empty ground both read as undefined. */
function worldGround(p: Project) {
    const index = M.cellIndex(p.cells);
    return {
        glyph: (x: number, y: number) => { const c = index.at(x, y); return c ? c.terrain[y - c.y][x - c.x] : undefined; },
        height: (x: number, y: number) => { const c = index.at(x, y); return c ? c.heights[`${x - c.x},${y - c.y}`] : undefined; },
    };
}

/**
 * The edits that turn `a` into `b`. World tiles and cells are in world coordinates. Cells an edit did not touch are
 * the very same objects in both, so a big world costs only what changed.
 */
export function diff(a: Project, b: Project): Op[] {
    const ops: Op[] = [];
    const same = (x: unknown, y: unknown) => x === y || stable(x) === stable(y);
    const put = (key: string, before: unknown, after: unknown) => { if (!same(before, after)) ops.push({key, before: before ?? null, after: after ?? null}); };

    put('name', a.name, b.name);
    put('spawn', a.spawn, b.spawn);
    put('herb', a.herbPatch, b.herbPatch);
    put('economy', a.economy, b.economy);

    const rooms = byId(a.rooms), roomsB = byId(b.rooms);
    for (const id of new Set([...rooms.keys(), ...roomsB.keys()])) {
        const ra = rooms.get(id), rb = roomsB.get(id);
        if (ra === rb) continue;
        put(`room:${id}`, ra && meta(ra), rb && meta(rb));
        // Tiles: only real changes; plain floor is what a new or grown room starts with.
        const w = Math.max(ra?.width ?? 0, rb?.width ?? 0), h = Math.max(ra?.height ?? 0, rb?.height ?? 0);
        for (let y = 0; y < h; y++) {
            const rowA = ra?.terrain[y], rowB = rb?.terrain[y];
            if (rowA !== undefined && rowA === rowB) continue;
            for (let x = 0; x < w; x++) {
                const ga = rowA?.[x], gb = rowB?.[x];
                if (ga === gb || (ga === undefined && gb === '.') || (ga === '.' && gb === undefined)) continue;
                ops.push({key: `rtile:${id}:${x},${y}`, before: ga ?? null, after: gb ?? null});
            }
        }
        for (const k of new Set([...Object.keys(ra?.heights ?? {}), ...Object.keys(rb?.heights ?? {})]))
            put(`rheight:${id}:${k}`, ra?.heights[k], rb?.heights[k]);
    }

    // World ground, tile by tile in world coordinates. A cell that kept its shape is compared row by row; where cells
    // were added, removed or re-cut, the ground is compared across the whole area they cover (it may just have moved
    // from one cell to another, which is no change at all).
    const cellsA = byId(a.cells), cellsB = byId(b.cells);
    let worldA: ReturnType<typeof worldGround> | null = null, worldB: ReturnType<typeof worldGround> | null = null;
    const seen = new Set<string>();
    for (const id of new Set([...cellsA.keys(), ...cellsB.keys()])) {
        const ca = cellsA.get(id), cb = cellsB.get(id);
        if (ca === cb) continue;
        if (ca && cb && sameShape(ca, cb)) {
            for (let y = 0; y < cb.height; y++) {
                const rowA = ca.terrain[y], rowB = cb.terrain[y];
                if (rowA === rowB) continue;
                for (let x = 0; x < cb.width; x++)
                    if (rowA[x] !== rowB[x]) ops.push({key: `tile:${cb.x + x},${cb.y + y}`, before: ground(rowA[x]), after: ground(rowB[x])});
            }
            if (ca.heights !== cb.heights)
                for (const k of new Set([...Object.keys(ca.heights), ...Object.keys(cb.heights)])) {
                    const [x, y] = k.split(',').map(Number);
                    put(`height:${cb.x + x},${cb.y + y}`, ca.heights[k], cb.heights[k]);
                }
            continue;
        }
        worldA ??= worldGround(a); worldB ??= worldGround(b);
        for (const c of [ca, cb]) if (c) for (let y = c.y; y < c.y + c.height; y++) for (let x = c.x; x < c.x + c.width; x++) {
            const at = `${x},${y}`;
            if (seen.has(at)) continue;
            seen.add(at);
            const ga = ground(worldA.glyph(x, y)), gb = ground(worldB.glyph(x, y));
            if (ga !== gb) ops.push({key: `tile:${at}`, before: ga, after: gb});
            put(`height:${at}`, worldA.height(x, y), worldB.height(x, y));
        }
    }

    const lists: [string, (p: Project) => {id: string}[], (p: Project, v: never) => unknown][] = [
        ['cell', p => p.cells, (_, c) => meta(c as M.WorldCell)],
        ['link', p => p.links, (_, v) => v], ['faction', p => p.factions, (_, v) => v], ['chapter', p => p.chapters, (_, v) => v],
        ['person', p => p.people, (_, v) => v], ['slot', p => p.slots, (_, v) => v], ['route', p => p.routes, (_, v) => v],
    ];
    for (const [kind, list, value] of lists) {
        const la = byId(list(a)), lb = byId(list(b));
        for (const id of new Set([...la.keys(), ...lb.keys()])) {
            const va = la.get(id), vb = lb.get(id);
            if (va === vb) continue;
            put(`${kind}:${id}`, va && value(a, va as never), vb && value(b, vb as never));
        }
    }
    return ops;
}

/**
 * Applies edits (already made by someone) to a project in place, without validation. Cells, interiors and lists
 * are replaced rather than changed, so a project that shares them with another (an older version) stays intact.
 */
export function applyChanges(p: Project, changes: {key: string; after: unknown}[]): void {
    const kindOf = (key: string) => key.slice(0, key.indexOf(':') < 0 ? key.length : key.indexOf(':'));
    const restOf = (key: string) => key.slice(key.indexOf(':') + 1);
    const upsert = <T extends {id: string}>(list: T[], id: string, value: T | null): T[] => {
        const i = list.findIndex(x => x.id === id);
        if (value === null) return i >= 0 ? list.filter((_, j) => j !== i) : list;
        return i >= 0 ? list.map((x, j) => (j === i ? value : x)) : [...list, value];
    };
    // 1. Cells. Their shapes change first; a cell that is new or changed shape takes its ground from the world as it
    //    was (a re-cut moves no ground), and tile edits then land in whichever cell now holds them.
    const cellOps = changes.filter(c => kindOf(c.key) === 'cell');
    if (cellOps.length) {
        const was = worldGround(p);
        let cells = p.cells;
        for (const {key, after} of cellOps) {
            const id = restOf(key), old = cells.find(c => c.id === id), m = after as Omit<M.WorldCell, 'terrain' | 'heights'> | null;
            if (!m) { cells = upsert(cells, id, null); continue; }
            if (old && sameShape(old, m as M.WorldCell)) { cells = upsert(cells, id, {...m, terrain: old.terrain, heights: old.heights} as M.WorldCell); continue; }
            const terrain: string[] = [], heights: Record<string, number> = {};
            for (let y = 0; y < m.height; y++) {
                let row = '';
                for (let x = 0; x < m.width; x++) {
                    row += was.glyph(m.x + x, m.y + y) ?? '.';
                    const h = was.height(m.x + x, m.y + y);
                    if (h !== undefined) heights[`${x},${y}`] = h;
                }
                terrain.push(row);
            }
            cells = upsert(cells, id, {...m, terrain, heights} as M.WorldCell);
        }
        p.cells = cells;
    }
    // 2. Interiors change shape before their tiles are set.
    for (const {key, after} of changes.filter(c => kindOf(c.key) === 'room')) {
        const id = restOf(key), room = after as Omit<M.Room, 'terrain' | 'heights'> | null, old = p.rooms.find(r => r.id === id);
        if (!room) { p.rooms = upsert(p.rooms, id, null); continue; }
        const terrain = Array.from({length: room.height}, (_, y) =>
            Array.from({length: room.width}, (_, x) => old?.terrain[y]?.[x] ?? '.').join(''));
        const heights = Object.fromEntries(Object.entries(old?.heights ?? {}).filter(([k]) => {
            const [x, y] = k.split(',').map(Number); return x < room.width && y < room.height;
        }));
        p.rooms = upsert(p.rooms, id, {...room, terrain, heights} as M.Room);
    }
    // 3. Ground: world tiles go to the cell that holds them now (tiles outside every cell are dropped), interior
    //    tiles to their interior. Each touched cell or interior is copied once.
    const index = M.cellIndex(p.cells), copies = new Map<string, M.WorldCell | M.Room>();
    const copy = (c: M.WorldCell | M.Room) => {
        if (!copies.has(c.id)) copies.set(c.id, {...c, terrain: [...c.terrain], heights: {...c.heights}} as typeof c);
        return copies.get(c.id)!;
    };
    const setTile = (c: M.WorldCell | M.Room, x: number, y: number, glyph: string | null) => {
        if (y < 0 || y >= c.height || x < 0 || x >= c.width) return;
        const t = copy(c);
        t.terrain[y] = t.terrain[y].slice(0, x) + (glyph ?? '.') + t.terrain[y].slice(x + 1);
    };
    const setHeight = (c: M.WorldCell | M.Room, k: string, h: number | null) => { const t = copy(c); if (h === null) delete t.heights[k]; else t.heights[k] = h; };
    for (const {key, after} of changes) {
        const kind = kindOf(key), rest = restOf(key);
        if (kind === 'tile' || kind === 'height') {
            const [wx, wy] = rest.split(',').map(Number), c = index.at(wx, wy);
            if (!c) continue;
            if (kind === 'tile') setTile(c, wx - c.x, wy - c.y, after as string | null);
            else setHeight(c, `${wx - c.x},${wy - c.y}`, after as number | null);
        } else if (kind === 'rtile' || kind === 'rheight') {
            const [id, at] = rest.split(':'), room = p.rooms.find(r => r.id === id);
            if (!room) continue;
            const [x, y] = at.split(',').map(Number);
            if (kind === 'rtile') setTile(room, x, y, after as string | null); else setHeight(room, at, after as number | null);
        }
    }
    if (copies.size) {
        p.cells = p.cells.map(c => (copies.get(c.id) as M.WorldCell | undefined) ?? c);
        p.rooms = p.rooms.map(r => (copies.get(r.id) as M.Room | undefined) ?? r);
    }
    // 4. Everything else.
    for (const {key, after} of changes) {
        const kind = kindOf(key), rest = restOf(key);
        switch (kind) {
            case 'name': p.name = after as string; break;
            case 'spawn': p.spawn = after as Place | null; break;
            case 'herb': p.herbPatch = after as Place | null; break;
            case 'economy': if (after) p.economy = after as M.Economy; break;
            case 'link': p.links = upsert(p.links, rest, after as M.Link | null); break;
            case 'faction': p.factions = upsert(p.factions, rest, after as M.Faction | null); break;
            case 'chapter': p.chapters = upsert(p.chapters, rest, after as M.Chapter | null); break;
            case 'person': p.people = upsert(p.people, rest, after as M.Person | null); break;
            case 'slot': p.slots = upsert(p.slots, rest, after as M.Slot | null); break;
            case 'route': p.routes = upsert(p.routes, rest, after as M.Route | null); break;
        }
    }
}

/** The edits that undo `ops`. */
export const inverse = (ops: Op[]): Op[] => ops.map(o => ({key: o.key, before: o.after, after: o.before})).reverse();

// --------------------------------------------------------------------------- Identity

const IDENTITY_KEY = 'ratw-editor-identity';
export const EDITOR_COLORS = ['#e6c481', '#a8ceda', '#ceaee0', '#e1aba2', '#becf91', '#8bd2c6', '#d5afc6', '#aec4e4', '#d9bc76', '#95c3a0'];

export function savedIdentity(): Identity | null {
    try {
        const v = JSON.parse(localStorage.getItem(IDENTITY_KEY) ?? 'null');
        return v && typeof v.name === 'string' && v.name.trim() && /^#[0-9a-f]{6}$/i.test(v.color) ? v : null;
    } catch { return null; }
}
export function saveIdentity(identity: Identity) {
    try { localStorage.setItem(IDENTITY_KEY, JSON.stringify(identity)); } catch { /* private window: asked again next time */ }
}
export function colorFor(name: string) {
    let h = 0;
    for (const ch of name) h = (h * 31 + ch.charCodeAt(0)) >>> 0;
    return EDITOR_COLORS[h % EDITOR_COLORS.length];
}

// --------------------------------------------------------------------------- Connection

/** One browser tab: its edits are recognisable in the shared feed, so it does not apply them twice. */
export const clientId = `${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 10)}`;

export interface Host {
    load(): Promise<{project: Project; seq: number}>;
    edit(body: unknown): Promise<Response>;
    sync(body: unknown): Promise<{seq: number; reload?: boolean; changes: Change[]; editors: Editor[]}>;
}

export interface Batch { id: number; label: string; ops: Op[] }

export interface LiveCallbacks {
    identity(): Identity;
    presence(): Presence;
    /** Someone else's edits arrived. */
    remote(changes: Change[]): void;
    /** A batch was refused; `current` holds the true values of its keys. */
    refused(batch: Batch, notice: ConflictNotice, current: Record<string, unknown>): void;
    status(status: LiveStatus): void;
    editors(editors: Editor[]): void;
    reload(): void;
    /** The ID of the world this tab holds; the host asks for a reload if DEV now holds another. */
    world?(): string | undefined;
}
export interface LiveStatus { state: 'connecting' | 'saved' | 'saving' | 'offline'; pending: number; error: string }

export function startLive(host: Host, seq: number, cb: LiveCallbacks) {
    const outbox: Batch[] = [];
    // The edit number of this tab's own latest saved change per key: an older change to the same key
    // arriving later (e.g. after a refusal showed the current value) must not replace a newer one.
    const own = new Map<string, number>();
    let since = seq, sending = false, stopped = false, offline = '';
    const report = () => cb.status({state: offline ? 'offline' : outbox.length ? 'saving' : 'saved', pending: outbox.length, error: offline});

    async function pump() {
        if (sending || stopped || !outbox.length) return;
        sending = true;
        const batch = outbox[0];
        try {
            const me = cb.identity();
            const response = await host.edit({clientId, editor: me.name, label: batch.label, ops: batch.ops});
            const data = await response.json().catch(() => ({error: `Host replied ${response.status}.`}));
            if (response.ok) {
                outbox.shift(); offline = '';
                for (const op of batch.ops) own.set(op.key, data.seq);
            }
            else if (response.status === 409 || response.status === 422) {
                outbox.shift(); offline = '';
                cb.refused(batch, {action: batch.label, message: data.error ?? 'Refused.', conflicts: data.conflicts ?? []}, data.current ?? {});
            } else throw new Error(data.error ?? `Host replied ${response.status}.`);
        } catch (error) {
            offline = (error as Error).message || 'The editor host is not reachable.';
            report();
            sending = false;
            setTimeout(pump, 3000);
            return;
        }
        sending = false;
        report();
        void pump();
    }

    async function heartbeat() {
        if (stopped) return;
        try {
            const me = cb.identity();
            const data = await host.sync({clientId, editor: me.name, color: me.color, since, state: cb.presence(), world: cb.world?.()});
            if (data.reload) { stopped = true; cb.reload(); return; }
            const theirs = data.changes.filter(c => c.clientId !== clientId && !((own.get(c.key) ?? 0) > c.seq));
            since = data.seq;
            if (theirs.length) cb.remote(theirs);
            cb.editors(data.editors);
            if (offline && !outbox.length) offline = '';
        } catch (error) {
            offline = (error as Error).message || 'The editor host is not reachable.';
        }
        report();
        setTimeout(heartbeat, 1500);
    }
    report();
    void heartbeat();
    return {
        push(batch: Batch) { outbox.push(batch); report(); void pump(); },
        pending: () => outbox.length,
        stop() { stopped = true; },
    };
}
