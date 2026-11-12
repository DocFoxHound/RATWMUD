// Calls to the studio's Atlas suite (tools/studio.py: /api/atlas/, tools/map_editor.py). Every call carries the DM
// account's sign-in (one sign-in for the tool, doc 66, 3) and every write Atlas's own session token; the host only
// listens on loopback.
import type {Project} from '../model/model.mjs';
import type {Character, Roster, SlotPreview} from './roster';
import * as M from '../model/model.mjs';
import {forHost, groundPending, want, type SentGround} from './lazyGround';
import {stable} from './live';
import {authHeaders, studioCall} from '../dm/api';
import {hosted} from './hosted';
import type {Choice, PullPlan, PullResult} from './sync';
import type {EconomyData, EconomyReply, ImportsBody, MarketBody, NeedsBody, RulesBody, SeasonsBody, ServicesBody, StatusBody,
    SteerBody} from './economy';

/** The world Atlas is editing (Database/worlds.json, doc 66), as the host says; null for a host without a database. */
export interface AtlasWorld { key: string; name: string; kind: 'dev' | 'live'; upstream?: string }
let info: Promise<{token: string; world?: AtlasWorld | null}> | null = null;
const ATLAS = 'api/atlas/';
const get = (path: string) => fetch(ATLAS + path, {headers: authHeaders()});
// Hosted (doc 78): no Atlas session token; the world is /api/studio/session's, and the shell's session covers writes.
const sessionInfo = () => (info ??= (hosted
    ? studioCall<{atlas: {world: AtlasWorld}}>('api/studio/session').then(s => ({token: '', world: s.atlas.world}))
    : get('session').then(async r => {
        if (!r.ok) { info = null; throw new Error((await r.json().catch(() => ({}))).error ?? `Host replied ${r.status}.`); }
        return r.json();
    })).catch(error => { info = null; throw error; }));
/** Atlas's own write header: never in hosted mode. */
const editorHeader = async (): Promise<Record<string, string>> => {
    const t = await session();
    return t ? {'X-RATW-Editor': t} : {};
};
const session = () => sessionInfo().then(j => j.token);
export const atlasWorld = () => sessionInfo().then(j => j.world ?? null);

async function post(path: string, body: unknown): Promise<Response> {
    return fetch(ATLAS + path, {
        method: 'POST',
        headers: {...authHeaders(), 'Content-Type': 'application/json', ...await editorHeader()},
        body: JSON.stringify(body),
    });
}
async function json<T>(response: Response): Promise<T> {
    const data = await response.json().catch(() => ({error: `Host replied ${response.status}.`}));
    if (!response.ok && !('errors' in data)) throw new Error(data.error ?? `Host replied ${response.status}.`);
    return data as T;
}

async function economyPost(kind: string, body: unknown): Promise<EconomyReply & EconomyData> {
    return studioCall<EconomyReply & EconomyData>(ATLAS + `economy/${kind}`, body, await editorHeader());
}

export interface HostValidation { valid: boolean; errors: string[]; warnings: string[] }
import type {Change, Editor} from './live';

export interface PlaytestResult { folder: string; manifest: string; command: string }
/** Where the running game server of Atlas's world is (GET api/atlas/play/server): its port, and its host when it runs
 *  on another computer than the studio's (null: the host this page came from); `local`: it serves its own computer only. */
export interface PlayAddress { port: number; host: string | null; local: boolean }
export interface PlayServer {
    world: AtlasWorld; play: PlayAddress | null; why: string; state: 'running' | 'maintenance'; starting: boolean; canStart: boolean;
    build: {build: number | null; current: boolean} | null;
}
export {gameUrl} from './sync';

/** What "who fills each profession slot" depends on: the slots, who is already named, and the ground beside each
 *  slot's work spot (customers need an open tile by a counter). A few kilobytes, however large the world. */
export function slotPlanRequest(project: Project) {
    const ground: Record<string, string> = {};
    for (const slot of project.slots) {
        const c = slot.work && M.getCell(project, slot.work.cell);
        if (!slot.work || !c) continue;                          // (A slot without a work place: doc 66, 2.)
        if (groundPending(c.id)) want(c.id);                     // Placeholder ground until it comes; asked again then.
        for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
            const x = slot.work.x + dx, y = slot.work.y + dy, g = c.terrain[y]?.[x];
            if ((dx || dy) && g !== undefined) ground[`${c.id}|${x}|${y}`] = g;
        }
    }
    return {id: project.id, name: project.name, slots: project.slots, people: project.people.map(p => p.id), ground,
        waiting: project.slots.some(s => !!s.work && groundPending(s.work.cell))};
}
let lastPlan: {key: string; answer: Promise<SlotPreview>} | null = null;
const stamps = new WeakMap<object, number>();
let stamp = 0;
const stampOf = (o: object | null) => { if (!o) return 0; let n = stamps.get(o); if (n === undefined) stamps.set(o, n = ++stamp); return n; };

/** A Chapter's building (Docs/Design/32), in its cell's tiles. */
export interface CampBuilding { cell: string; x: number; y: number; kind: string; built: boolean; condition: number; site: string; chapter: string; colour: string }

export const api = {
    validate: async (project: Project) => json<HostValidation>(await post('validate', forHost(project))),
    exportZip: async (project: Project): Promise<Blob> => {
        const response = await post('export', forHost(project));
        if (!response.ok) {
            const data = await response.json().catch(() => ({}));
            throw new Error((data.errors ?? [data.error ?? 'Export failed.']).join('\n'));
        }
        return response.blob();
    },
    demo: async () => json<Project>(await get('demo')),
    /** The one world, edited live (tools/live_edit.py). */
    live: {
        /** Lean: world cells come as outlines and previews; their ground is asked for as they come into view. */
        load: async () => json<{project: Project; seq: number; gameSeq?: number; bands?: boolean}>(await get('live/world?lean=1')),
        ground: async (ids: string[]) => json<{seq: number; cells: Record<string, SentGround>}>(
            await get(`live/ground?cells=${ids.map(encodeURIComponent).join(',')}`)),
        edit: (body: unknown) => post('live/edit', body),
        /** What the players' Chapters have built, as the DEV game server last saved it (read-only). */
        camps: async () => json<{buildings: CampBuilding[]}>(await get('live/camps')),
        sync: async (body: unknown) => json<{seq: number; reload?: boolean; changes: Change[]; editors: Editor[]; gameSeq?: number}>(await post('live/sync', body)),
    },
    roster: async () => json<Roster>(await get('roster')),
    saveRoster: async (roster: Roster) => {
        const data = await json<Roster & {errors?: string[]}>(await post('roster', roster));
        if (data.errors?.length) throw new Error(data.errors.join('\n'));
        return data as Roster;
    },
    /**
     * Who would fill each profession slot. Only what that depends on is sent (slotPlanRequest), and only when it has
     * changed since the last time (or the roster has): otherwise the last answer stands.
     */
    preview: async (project: Project, options: {roster?: object | null; force?: boolean} = {}) => {
        const request = slotPlanRequest(project), key = stable(request) + '|' + stampOf(options.roster ?? null);
        if (!options.force && lastPlan?.key === key) return lastPlan.answer;
        const answer = (async () => {
            const data = await json<SlotPreview & {errors?: string[]}>(await post('roster/plan', request));
            if (data.errors?.length) throw new Error(data.errors.join('\n'));
            return data as SlotPreview;
        })();
        lastPlan = {key, answer};
        answer.catch(() => { if (lastPlan?.key === key) lastPlan = null; });
        return answer;
    },
    /** Update from live (doc 66, section 4; tools/world_sync.py): the three-way plan of Atlas's world against its
     *  upstream, and the pull (live's changes and the chosen side of each conflict into Atlas's world). Refusals are
     *  DmErrors with the engine's `errors` and, on a 409, the `conflicts` left. */
    sync: {
        diff: () => studioCall<PullPlan>(ATLAS + 'sync/diff'),
        pull: async (choices: Record<string, Choice>, expect: string) =>
            studioCall<PullResult>(ATLAS + 'sync/pull', {choices, expect}, await editorHeader()),
    },
    /** The Economy page (doc 79, 12 and 14; tools/studio.py): the EconomyView of Atlas's world. Reading is anyone's;
     *  each change needs a dm or admin account and Atlas's token, and replies the view with {changed, detail, sync,
     *  warnings}. imports, status and market need migration 0061 (409 before it). */
    economy: {
        read: () => studioCall<EconomyData>(ATLAS + 'economy'),
        /** The slider: {prosperity (clamped, never refused), cause?, now?}. */
        steer: async (body: SteerBody) => economyPost('steer', body),
        needs: async (body: NeedsBody) => economyPost('needs', body),
        rules: async (body: RulesBody) => economyPost('rules', body),
        services: async (body: ServicesBody) => economyPost('services', body),
        seasons: async (body: SeasonsBody) => economyPost('seasons', body),
        imports: async (body: ImportsBody) => economyPost('imports', body),
        status: async (body: StatusBody) => economyPost('status', body),
        market: async (body: MarketBody) => economyPost('market', body),
    },
    ai: async () => json<{available: boolean; model: string; error: string}>(await get('ai')),
    generate: async (count: number, favor: string[], theme: string) => {
        const response = await post('roster/generate', {count, favor, theme});
        const data = await response.json().catch(() => ({}));
        if (!response.ok) throw new Error((data.errors ?? [data.error ?? `Generation failed (${response.status}).`]).join('\n'));
        return data.characters as Character[];
    },
    /** The game server running Atlas's world (tools/studio.py): where to open its game, or why there is none. */
    playServer: async () => json<PlayServer>(await get('play/server')),
    /** Starts the game server of Atlas's world (tools/live.sh server KEY, on the local network). */
    startPlayServer: async () => json<{started: boolean; starting: boolean; port?: number}>(await post('play/server/start', {})),
    playtest: async (project: Project, quickStart: boolean) => {
        const data = await json<PlaytestResult & {errors?: string[]}>(await post('playtest', {project: forHost(project), quickStart}));
        if (data.errors?.length) throw new Error(data.errors.join('\n'));
        return data;
    },
};

export function download(name: string, data: Blob | string, type = 'application/json') {
    const url = URL.createObjectURL(typeof data === 'string' ? new Blob([data], {type}) : data);
    const a = Object.assign(document.createElement('a'), {href: url, download: name});
    document.body.append(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
}
