// Calls to the local Python host (tools/map_editor.py). Every write carries the
// per-launch session token; the host only listens on loopback.
import type {Project} from '../model/model.mjs';
import type {Character, Roster, SlotPreview} from './roster';

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

export const api = {
    validate: async (project: Project) => json<HostValidation>(await post('api/validate', project)),
    exportZip: async (project: Project): Promise<Blob> => {
        const response = await post('api/export', project);
        if (!response.ok) {
            const data = await response.json().catch(() => ({}));
            throw new Error((data.errors ?? [data.error ?? 'Export failed.']).join('\n'));
        }
        return response.blob();
    },
    demo: async () => json<Project>(await fetch('api/demo')),
    /** The one world, edited live (tools/live_edit.py). */
    live: {
        load: async () => json<{project: Project; seq: number}>(await fetch('api/live/world')),
        edit: (body: unknown) => post('api/live/edit', body),
        sync: async (body: unknown) => json<{seq: number; reload?: boolean; changes: Change[]; editors: Editor[]}>(await post('api/live/sync', body)),
    },
    roster: async () => json<Roster>(await fetch('api/roster')),
    saveRoster: async (roster: Roster) => {
        const data = await json<Roster & {errors?: string[]}>(await post('api/roster', roster));
        if (data.errors?.length) throw new Error(data.errors.join('\n'));
        return data as Roster;
    },
    preview: async (project: Project) => {
        const data = await json<SlotPreview & {errors?: string[]}>(await post('api/roster/preview', project));
        if (data.errors?.length) throw new Error(data.errors.join('\n'));
        return data;
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
        const data = await json<PlaytestResult & {errors?: string[]}>(await post('api/playtest', {project, quickStart}));
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
