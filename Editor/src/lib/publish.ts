// Publish's words (Docs/Design/78, sections 10.3, 11, 12.3-12.5; Phase M8; tools/world_publish.py): the count of map
// changes not published, the memory line, the three ways a world moves onto a new map, a job's progress, each problem's
// "Show me", and which changes are Live now and which wait for Publish. Pure functions, tested in publish.test.ts.

export interface PublishProblem { text: string; cell?: string; x?: number; y?: number }
export type Restart = 'host' | 'now' | 'later';
export interface PublishJob {
    id: number; by: string; at: number; step: string; done: number; of: number; errors: PublishProblem[]; warnings: PublishProblem[];
    build: number | null; cells?: number; restart: Restart; rebuild?: boolean;
}
export interface PublishMemory { need: number; free: number | null; limit: number | null; ok: boolean; words: string }
export interface PublishState {
    unpublished: {count: number | null; build: number | null; through: number | null; last: {at: string; by: string} | null};
    memory: PublishMemory; job: PublishJob | null; serverState: string; hostOnly: {available: boolean; reason: string};
    previous: number | null; build: {id: number; at: string; by: string; copyOf: number | null; failed: boolean; format: number} | null;
    fallback: {at: number; failed: number | null; now: number} | null;
    rebuild: {needed: boolean; unpublished?: number | null; failed?: string; memory?: {need?: number; free?: number | null}} | null;
    perHour: number;
}
/** My world's map lines. */
export interface MapSummary { unpublished: number | null; memory: PublishMemory }

/** "3 map changes not published", "1 map change not published", "Every map change is published". */
export function unpublishedLine(count: number | null | undefined): string {
    if (count === null || count === undefined) return 'Map changes are counted from the next Publish';
    if (count === 0) return 'Every map change is published';
    return `${count} map ${count === 1 ? 'change' : 'changes'} not published`;
}

/** How the world moves onto the new map (doc 78, 10.3, step 6), in the order offered. */
export const RESTART_CHOICES: {id: Restart; label: string; hint: string}[] = [
    {id: 'host', label: 'Open only to me',
        hint: 'Recommended for a first Publish. Players are given a minute to leave, the world starts on the new map and lets in only you. ' +
            'Then you choose: open to everyone, or go back to the previous map.'},
    {id: 'now', label: 'Now, with a minute\'s warning',
        hint: 'Players are told and given a minute; the world closes, starts on the new map and opens again by itself.'},
    {id: 'later', label: 'When everyone has left',
        hint: 'Nobody is interrupted: the world moves onto the new map the next time nobody is in it.'},
];

const STEPS: Record<string, string> = {
    backup: 'Making a backup first…', checking: 'Checking the map…', loading: 'Reading the map…', validating: 'Checking the map…',
    building: 'Building the map…', storing: 'Saving the new map…', restarting: 'Moving the world onto the new map…',
};

export const jobRunning = (job: PublishJob | null | undefined) => !!job && !['done', 'refused', 'failed'].includes(job.step);

/** A job in one line: what it is doing, or how it ended. */
export function jobLine(job: PublishJob | null | undefined): string {
    if (!job) return '';
    if (job.step === 'done') {
        const how = job.restart === 'host' ? 'The world is open only to you, on the new map.'
            : job.restart === 'now' ? 'The world is open again, on the new map.'
            : 'The world moves onto it once everyone has left.';
        return `Published${job.build ? ` (map ${job.build})` : ''}. ${how}`;
    }
    if (job.step === 'refused') return `Not published: ${job.errors.length === 1 ? 'one thing needs' : `${job.errors.length} things need`} putting right first. Nothing was changed.`;
    if (job.step === 'failed') return 'Publishing stopped. Nothing was changed.';
    if (job.step === 'building' && job.of > 0) return `Building the map… ${Math.min(99, Math.round(100 * job.done / job.of))}%`;
    return STEPS[job.step] ?? 'Publishing…';
}

/** A problem's place in the map editor, as the page's query ("Show me"), or null when it names none. */
export function showQuery(p: PublishProblem): string | null {
    if (!p.cell || !/^[a-z0-9_-]{1,80}$/i.test(p.cell)) return null;
    const at = Number.isFinite(p.x) && Number.isFinite(p.y) ? `,${Math.floor(p.x!)},${Math.floor(p.y!)}` : '';
    return `suite=atlas&show=${p.cell}${at}`;
}

/** The place a "Show me" link asked for (?show=CELL or ?show=CELL,X,Y), or null. */
export function showFrom(search: string): {cell: string; x: number; y: number} | null {
    const raw = new URLSearchParams(search).get('show');
    const m = raw ? /^([a-z0-9_-]{1,80})(?:,(-?\d{1,6}),(-?\d{1,6}))?$/i.exec(raw) : null;
    return m ? {cell: m[1], x: m[2] ? Number(m[2]) : 0, y: m[3] ? Number(m[3]) : 0} : null;
}

// ------------------------------------------------------------------------------------------------- Live now / Waits
// Doc 78, section 11: everything a host changes is either Live now (about a second: the DM tool's acts, NPCs, routes,
// areas, factions, the economy, talk) or Waits for Publish (the map: terrain, cells, interiors, doors, places,
// resources and profession-slot fills). Nothing is "restart only".

export type Takes = 'live' | 'publish';
const MAP_KINDS = ['tile', 'height', 'rtile', 'rheight', 'room', 'cell', 'link', 'chapter', 'slot'];
const MAP_KEYS = ['name', 'spawn', 'herb'];

/** When one of Atlas's edits takes effect, by its key (tools/world_publish.py's MAP_KINDS: the same list). */
export const takes = (key: string): Takes => (MAP_KEYS.includes(key) || MAP_KINDS.includes(key.split(':')[0]) ? 'publish' : 'live');

export const BADGE: Record<Takes, {label: string; title: string}> = {
    live: {label: 'Live now', title: 'What you save here reaches the world players are in within about a second.'},
    publish: {label: 'Waits for Publish', title: 'What you save here changes the map\'s source. Players see it after Publish (the Publish page).'},
};

/** The badge a page's Save buttons carry: the Workshop's map waits for Publish; every other page is live. */
export function pageTakes(suite: 'atlas' | 'dm', page: string): Takes | null {
    if (suite === 'dm') return 'live';
    if (page === 'publish') return null;
    return page === 'workshop' ? 'publish' : 'live';
}

export const FALLBACK_TEXT = 'Your last map didn\'t start; the world went back to the previous map. Your edits are kept.';

/** The update rebuild's question (doc 78, 12.5). */
export function rebuildQuestion(r: NonNullable<PublishState['rebuild']>): string {
    if (r.failed) return `This update needs your map rebuilt, and it couldn't be: ${r.failed} Put it right in the map editor, then rebuild.`;
    const n = r.unpublished ?? 0;
    if (n > 0) return `This update needs your map rebuilt. Your ${n} unpublished ${n === 1 ? 'change' : 'changes'} would go live: rebuild with them, or discard them first?`;
    return 'This update needs your map rebuilt before the world can start.';
}

/** Why a Publish can't start, or '' (the button's greyed reason). */
export function publishBlocked(state: PublishState, choice: Restart): string {
    if (jobRunning(state.job)) return 'The map is being published now.';
    if (!state.memory.ok) return state.memory.words;
    if (choice === 'host' && !state.hostOnly.available) return state.hostOnly.reason;
    return '';
}
