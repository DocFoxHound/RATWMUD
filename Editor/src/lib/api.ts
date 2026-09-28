// Calls to the local Python host (tools/map_editor.py). Every write carries the
// per-launch session token; the host only listens on loopback.
import type {Project} from '../model/model.mjs';
import type {Character, Roster, SlotPreview} from './roster';
import * as M from '../model/model.mjs';
import {forHost, groundPending, want, type SentGround} from './lazyGround';
import {stable} from './live';

let token: Promise<string> | null = null;
const session = () => (token ??= fetch('api/session').then(r => r.json()).then(j => j.token as string));

async function post(path: string, body: unknown): Promise<Response> {
    return fetch(path, {
        method: 'POST',
        headers: {'Content-Type': 'application/json', 'X-RATW-Editor': await session()},
        body: JSON.stringify(body),
    });
}
async function json<T>(response: Response): Promise<T> {
    const data = await response.json().catch(() => ({error: `Host replied ${response.status}.`}));
    if (!response.ok && !('errors' in data)) throw new Error(data.error ?? `Host replied ${response.status}.`);
    return data as T;
}

export interface HostValidation { valid: boolean; errors: string[]; warnings: string[] }
import type {Change, Editor} from './live';

export type Counts = Record<string, number>;
export interface PublishSummary { world: Record<string, Counts>; live: Record<string, Counts> }
export interface Release { number: number; at: string; by: string; note: string; kind: 'push' | 'rollback'; restored: number | null; summary: PublishSummary }
export interface NpcCopy { copied: Record<string, number>; skipped: string[]; error?: string }
export interface PublishPreview {
    world: string; devRevision: number; release: number; errors: string[]; warnings: string[]; changed: boolean;
    summary: PublishSummary; releases: Release[];
}
/** A refused push or rollback: the message, and the problems behind it (e.g. what blocks publishing). */
export class PublishRefusal extends Error {
    constructor(message: string, readonly errors: string[], readonly status: number) { super(message); }
}
export interface PlaytestResult { folder: string; manifest: string; command: string }

/** What "who fills each profession slot" depends on: the slots, who is already named, and the ground beside each
 *  slot's work spot (customers need an open tile by a counter). A few kilobytes, however large the world. */
export function slotPlanRequest(project: Project) {
    const ground: Record<string, string> = {};
    for (const slot of project.slots) {
        const c = M.getCell(project, slot.work.cell);
        if (!c) continue;
        if (groundPending(c.id)) want(c.id);                     // Placeholder ground until it comes; asked again then.
        for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
            const x = slot.work.x + dx, y = slot.work.y + dy, g = c.terrain[y]?.[x];
            if ((dx || dy) && g !== undefined) ground[`${c.id}|${x}|${y}`] = g;
        }
    }
    return {id: project.id, name: project.name, slots: project.slots, people: project.people.map(p => p.id), ground,
        waiting: project.slots.some(s => groundPending(s.work.cell))};
}
let lastPlan: {key: string; answer: Promise<SlotPreview>} | null = null;
const stamps = new WeakMap<object, number>();
let stamp = 0;
const stampOf = (o: object | null) => { if (!o) return 0; let n = stamps.get(o); if (n === undefined) stamps.set(o, n = ++stamp); return n; };

export const api = {
    validate: async (project: Project) => json<HostValidation>(await post('api/validate', forHost(project))),
    exportZip: async (project: Project): Promise<Blob> => {
        const response = await post('api/export', forHost(project));
        if (!response.ok) {
            const data = await response.json().catch(() => ({}));
            throw new Error((data.errors ?? [data.error ?? 'Export failed.']).join('\n'));
        }
        return response.blob();
    },
    demo: async () => json<Project>(await fetch('api/demo')),
    /** The one world, edited live (tools/live_edit.py). */
    live: {
        /** Lean: world cells come as outlines and previews; their ground is asked for as they come into view. */
        load: async () => json<{project: Project; seq: number}>(await fetch('api/live/world?lean=1')),
        ground: async (ids: string[]) => json<{seq: number; cells: Record<string, SentGround>}>(
            await fetch(`api/live/ground?cells=${ids.map(encodeURIComponent).join(',')}`)),
        edit: (body: unknown) => post('api/live/edit', body),
        sync: async (body: unknown) => json<{seq: number; reload?: boolean; changes: Change[]; editors: Editor[]}>(await post('api/live/sync', body)),
    },
    roster: async () => json<Roster>(await fetch('api/roster')),
    saveRoster: async (roster: Roster) => {
        const data = await json<Roster & {errors?: string[]}>(await post('api/roster', roster));
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
            const data = await json<SlotPreview & {errors?: string[]}>(await post('api/roster/plan', request));
            if (data.errors?.length) throw new Error(data.errors.join('\n'));
            return data as SlotPreview;
        })();
        lastPlan = {key, answer};
        answer.catch(() => { if (lastPlan?.key === key) lastPlan = null; });
        return answer;
    },
    /** Push to live (tools/publish.py). */
    publish: {
        preview: async () => json<PublishPreview>(await fetch('api/publish')),
        push: async (body: {password: string; note: string; editor: string}) => published(await post('api/publish/push', body)),
        rollback: async (body: {release: number; password: string; note: string; editor: string}) => published(await post('api/publish/rollback', body)),
        /** Copies PROD's NPCs and their running state into DEV (no password: it only writes DEV). */
        pull: async (body: {editor: string}) => {
            const response = await post('api/publish/pull', body);
            const data = await response.json().catch(() => ({error: `Host replied ${response.status}.`}));
            if (!response.ok) throw new PublishRefusal(data.error ?? 'Refused.', data.errors ?? [], response.status);
            return data as NpcCopy;
        },
    },
    ai: async () => json<{available: boolean; model: string; error: string}>(await fetch('api/ai')),
    generate: async (count: number, favor: string[], theme: string) => {
        const response = await post('api/roster/generate', {count, favor, theme});
        const data = await response.json().catch(() => ({}));
        if (!response.ok) throw new Error((data.errors ?? [data.error ?? `Generation failed (${response.status}).`]).join('\n'));
        return data.characters as Character[];
    },
    playtest: async (project: Project, quickStart: boolean) => {
        const data = await json<PlaytestResult & {errors?: string[]}>(await post('api/playtest', {project: forHost(project), quickStart}));
        if (data.errors?.length) throw new Error(data.errors.join('\n'));
        return data;
    },
};

async function published(response: Response): Promise<{release: number; summary: PublishSummary; pulled?: NpcCopy}> {
    const data = await response.json().catch(() => ({error: `Host replied ${response.status}.`}));
    if (!response.ok) throw new PublishRefusal(data.error ?? 'Refused.', data.errors ?? [], response.status);
    return data;
}

export function download(name: string, data: Blob | string, type = 'application/json') {
    const url = URL.createObjectURL(typeof data === 'string' ? new Blob([data], {type}) : data);
    const a = Object.assign(document.createElement('a'), {href: url, download: name});
    document.body.append(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
}
