// The Campaigns console's data shaping (Docs/Design/85-story-triggers-and-campaign-control.md, section 5, Phase 1): what
// the story runner's reads (tools/story_runner.py: status, timeline) mean in words, without React, so it is tested on
// its own (campaigns.test.ts). Ids are shown by their names wherever the reads gave one.

export type OrderStatus = 'queued' | 'applied' | 'refused' | 'expired';
export interface Order {
    id: number; kind: string; target: string; status: OrderStatus; result: string; by: string; at: string | null;
    doneAt: string | null; age?: number; visit?: number | null;
}
export type Effect = Record<string, unknown> & {kind: string};
/** One thing the phase still reacts to (an exit or a trigger), as the runner lists it. */
export interface Listening {
    source: 'exit' | 'trigger'; on: string; id?: string; index?: number; to?: string; scope?: string; once?: string; audience?: string;
    match?: Record<string, string>; afterDays?: number; missions?: string[]; players?: number; percent?: number; signal?: string;
}
export type Waiting =
    | {on: 'publish'}
    | {on: 'admin'; problem: string; order?: Order}
    | {on: 'game'; order: Order}
    | {on: 'send'; effects: Effect[]}
    | {on: 'timer'; name: string; signal: string; at: number; clock: 'game' | 'real'; actor: string; repeat: number; scope: string}
    | {on: 'clock'; at: number; what: Listening}
    | {on: 'direction'; signal: string; what: Listening}
    | {on: 'wolves'; events: string[]; what: Listening[]}
    | {on: 'end'};
export interface Alert {
    kind: 'paused' | 'game' | 'behind' | 'quiet'; needsYou: boolean; problem?: string; order?: Order; events?: number; seconds?: number;
    hours?: number; since?: string; wolfSeen?: string;
}
export interface MissionSummary { id: string; title: string; mode: 'individual' | 'group' | 'either'; completed: number; required: number; ready: boolean; optional: boolean }
export interface Objective { label: string; on: string; goal?: number; measure?: string; match?: Record<string, string>; signal?: string }
export interface Mission extends MissionSummary {
    objectives: Objective[]; tracks: Record<string, [number, number]>; contributors: string[]; requiredContributors: number;
}
export interface Last { type: 'trigger' | 'phase'; trigger?: string; phase?: string; actor?: string; day: number | null; at: string | null; cause?: string; visit?: number }
export interface RunRow {
    run: string; title: string; status: string; digest: string; approver: string; approvedAt: string | null; phase: string | null; problem: string;
    visit?: number; entered?: number; cohort?: string[]; wolves?: number; missions?: MissionSummary[]; waiting?: Waiting[];
    behind?: {events: number; seconds: number}; last?: Last | null; alerts?: Alert[]; ending?: boolean;
}
export interface Now { day: number | null; newest: number; at: string }
export interface Overview { runs: RunRow[]; now: Now; names: Record<string, string>; thresholds?: Record<string, number> }
export interface Region { id: string; cell: string; x1: number; y1: number; x2: number; y2: number }
export interface Timer { signal?: string; at: number; clock?: 'game' | 'real'; actor?: string; interval?: number; scope?: string }
export interface RunState {
    phase: string; entered: number; cohort: string[]; votes: Record<string, string[]>; visits: number; actor: string; publishedAt?: number;
    counters?: {story?: Record<string, number>; player?: Record<string, Record<string, number>>}; timers?: Record<string, Timer>;
    fired?: Record<string, string[]>; campaignFired?: Record<string, string[]>; triggerEffects?: Effect[];
}
export interface Visit { visit: number; phase: string; day: number; cause: string; receipts: number[]; missions?: Mission[] | null }
export interface Firing {
    id: number; visit: number; phase: string; trigger: string; actor: string; day: number; actions: string[]; scope?: string; on?: string;
    signal?: string | null; event?: number | string | null; cell?: string; x?: number; y?: number;
}
export interface RunDetail extends RunRow {
    state: RunState | null; program?: Record<string, unknown> & {title: string; id: string}; approvedBy: string; cursor: number; now: Now;
    guestVoices: string[]; directions: {npc: string; visit: number; receipt: number; status: string}[]; signals: string[];
    history: Visit[]; trace: Firing[]; actions: {id: number; kind: string; status: string; result: string}[]; orders: Order[];
    missionsNow: Mission[]; directed: string[]; regions: Region[]; phases: {id: string; to: string[]; missions: number}[]; start: string;
    listening: Listening[]; names: Record<string, string>;
}
export type TimelineItem =
    | {key: string; type: 'phase'; at: string | null; visit: number; phase: string; day: number; cause: string; receipts: number[]; missions?: Mission[] | null}
    | ({key: string; type: 'trigger'; at: string | null} & Firing)
    | ({key: string; type: 'order'; at: string | null} & Order);
export interface WorldEvent { id: number; kind: string; actor: string; target: string; cell: string; item: string; quantity: number; coins: number; detail: string; day: number; x: number | null; y: number | null; at: string }
export interface TimelineCursor { visit?: number; trigger?: number; order?: number; at?: string }
export interface TimelinePage { run: string; items: TimelineItem[]; events: Record<string, WorldEvent>; names: Record<string, string>; before: TimelineCursor | null; since: TimelineCursor; now: string }

export const nameOf = (names: Record<string, string>, id: string | null | undefined) => (id ? names[id] ?? id : '');

// ------------------------------------------------------------------------------------------------- the timeline

const RANK = {phase: 0, trigger: 1, order: 2} as const;
const seqOf = (i: TimelineItem) => (i.type === 'phase' ? i.visit : i.id);
/** Newest first, as the runner sorts (tools/story_runner.timeline): by time, then visit, then a visit opened before its
 *  firings and its orders, then by id. */
export function newestFirst(a: TimelineItem, b: TimelineItem): number {
    const time = (i: TimelineItem) => (i.at ? Date.parse(i.at) : 0);
    return (time(b) - time(a)) || ((b.visit ?? 0) - (a.visit ?? 0)) || (RANK[b.type] - RANK[a.type]) || (seqOf(b) - seqOf(a));
}
/** What the console has, with a poll's or an earlier page's rows merged in: one row per key (the newer copy wins: an
 *  order answered since), newest first. */
export function mergeTimeline(known: TimelineItem[], fresh: TimelineItem[]): TimelineItem[] {
    const byKey = new Map(known.map(i => [i.key, i]));
    for (const i of fresh) byKey.set(i.key, i);
    return [...byKey.values()].sort(newestFirst);
}
/** The cursor for the next poll: the newest of each kind the console holds, and the time of the last answer. */
export function sinceOf(items: TimelineItem[], previous: TimelineCursor, at: string): TimelineCursor {
    const most = (type: TimelineItem['type'], had?: number) => Math.max(had ?? 0, ...items.filter(i => i.type === type).map(seqOf));
    return {visit: most('phase', previous.visit), trigger: most('trigger', previous.trigger), order: most('order', previous.order), at};
}
export type TimelineFilter = 'all' | 'phase' | 'trigger' | 'order';
export function filterTimeline(items: TimelineItem[], kind: TimelineFilter, wolf = '', trigger = ''): TimelineItem[] {
    return items.filter(i => (kind === 'all' || i.type === kind)
        && (!wolf || (i.type === 'trigger' ? i.actor === wolf : i.type === 'order' ? i.target === wolf : true))
        && (!trigger || (i.type === 'trigger' && i.trigger === trigger)));
}

export function causeText(cause: string, names: Record<string, string> = {}): string {
    if (cause === 'published') return 'published';
    if (cause === 'clock') return 'by the clock (a timer or a delay)';
    if (cause.startsWith('event ')) return `world event #${cause.slice(6)}`;
    if (cause.startsWith('DM signal: ')) return `a Dungeon Master's direction "${cause.slice(11)}"`;
    return nameOf(names, cause);
}

// ------------------------------------------------------------------------------------------------- time

/** "3 h ago", "6 min ago", "just now", from two ISO times. */
export function ago(at: string | null | undefined, now: string): string {
    if (!at) return '';
    const s = Math.max(0, (Date.parse(now) - Date.parse(at)) / 1000);
    if (s < 45) return 'just now';
    if (s < 3600) return `${Math.round(s / 60)} min ago`;
    if (s < 86400 * 2) return `${Math.round(s / 3600)} h ago`;
    return `${Math.round(s / 86400)} days ago`;
}
// (A game-day timer's hours are the world's, not the real clock's.)
const days = (n: number, game = true) => (Math.abs(n) < 1 ? `${Math.round(n * 24 * 10) / 10} ${game ? 'game h' : 'h'}` : `${Math.round(n * 10) / 10} days`);
/** When a timer is due: a game-day timer by the world's calendar, a real-clock one in real days since publication. */
export function dueText(at: number, clock: 'game' | 'real' | undefined, day: number | null, publishedAt?: number, nowIso?: string): string {
    if (clock === 'real') {
        if (publishedAt === undefined || !nowIso) return `real day ${at.toFixed(1)} since publication`;
        const left = at - (Date.parse(nowIso) / 1000 - publishedAt) / 86400;
        return left <= 0 ? 'due now (real time)' : `in ${days(left, false)} (real time)`;
    }
    if (day === null) return `day ${at.toFixed(1)}`;
    const left = at - day;
    return left <= 0 ? `day ${at.toFixed(1)} (due now)` : `day ${at.toFixed(1)} (in ${days(left)})`;
}

// ------------------------------------------------------------------------------------------------- what it waits for

export function orderLine(o: Order, names: Record<string, string>): string {
    return `${o.kind}${o.target && !o.target.startsWith('world-') ? ' → ' + nameOf(names, o.target) : ''}`;
}
export function effectLine(e: Effect, names: Record<string, string>): string {
    const who = ['player', 'npc', 'town', 'market', 'faction', 'region', 'scene', 'ship', 'area'].map(k => e[k]).find(v => typeof v === 'string') as string | undefined;
    return `${e.kind}${who ? ' → ' + nameOf(names, who) : ''}`;
}
function listeningLine(l: Listening, names: Record<string, string>): string {
    const filters = Object.entries(l.match ?? {}).map(([k, v]) => `${k} ${nameOf(names, v)}`).join(', ');
    return `${l.source === 'trigger' ? l.id : 'exit to ' + l.to}: when ${l.on}${l.signal ? ' "' + l.signal + '"' : ''}${filters ? ' (' + filters + ')' : ''}`;
}
export type Tone = 'alert' | 'game' | 'normal' | 'done';
export interface WaitLine { who: string; text: string; tone: Tone }
/** Every thing the run waits for, in words, most pressing first (doc 85, 5.2's "Waiting on" and 5.3's "Waiting for"). */
export function waitingLines(waiting: Waiting[] | undefined, names: Record<string, string>, day: number | null, publishedAt?: number, nowIso?: string): WaitLine[] {
    const out: WaitLine[] = [];
    for (const w of waiting ?? []) {
        if (w.on === 'publish') out.push({who: 'you', text: 'approved, waiting to be published', tone: 'alert'});
        else if (w.on === 'admin') out.push({who: 'you', tone: 'alert',
            text: w.order ? `${orderLine(w.order, names)} ${w.order.status}${w.order.result ? ': ' + w.order.result : ''}` : w.problem || 'paused'});
        else if (w.on === 'game') out.push({who: 'the game', tone: 'game',
            text: `to apply ${orderLine(w.order, names)} (order #${w.order.id}, sent ${w.order.age !== undefined ? Math.round(w.order.age) + ' s ago' : 'just now'})`});
        else if (w.on === 'send') out.push({who: 'the runner', tone: 'game',
            text: `${w.effects.length} order${w.effects.length === 1 ? '' : 's'} still to send: ${w.effects.slice(0, 3).map(e => effectLine(e, names)).join(', ')}${w.effects.length > 3 ? '…' : ''}`});
        else if (w.on === 'timer') out.push({who: 'a timer', tone: 'normal',
            text: `${w.signal}${w.actor && w.name.includes('/') ? ' (' + nameOf(names, w.actor) + ')' : ''} ${dueText(w.at, w.clock, day, publishedAt, nowIso)}${w.repeat ? `, every ${w.repeat} days` : ''}`});
        else if (w.on === 'clock') out.push({who: 'the clock', tone: 'normal', text: `${listeningLine(w.what, names)}, ${dueText(w.at, 'game', day)}`});
        else if (w.on === 'direction') out.push({who: 'a direction', tone: 'normal', text: `a Dungeon Master's "${w.signal}"`});
        else if (w.on === 'wolves') out.push({who: 'wolves', tone: 'normal', text: w.what.slice(0, 4).map(l => listeningLine(l, names)).join(' · ') + (w.what.length > 4 ? ' …' : '')});
        else if (w.on === 'end') out.push({who: 'nothing', tone: 'done', text: 'this phase is an ending'});
    }
    return out;
}
/** The one line the list shows: the most pressing thing; among timers and the clock, the soonest. */
export function waitingOn(waiting: Waiting[] | undefined, names: Record<string, string>, day: number | null, publishedAt?: number, nowIso?: string): WaitLine | null {
    const all = waiting ?? [];
    const first = all.find(w => ['publish', 'admin', 'game', 'send'].includes(w.on));
    const timed = all.filter((w): w is Extract<Waiting, {on: 'timer' | 'clock'}> => w.on === 'timer' || w.on === 'clock')
        .map(w => ({w, left: w.on === 'timer' && w.clock === 'real'
            ? (publishedAt !== undefined && nowIso ? w.at - (Date.parse(nowIso) / 1000 - publishedAt) / 86400 : Infinity)
            : w.at - (day ?? 0)}))
        .sort((a, b) => a.left - b.left)[0]?.w;
    const pick = first ?? timed ?? all.find(w => w.on === 'direction') ?? all.find(w => w.on === 'wolves') ?? all.find(w => w.on === 'end');
    return pick ? waitingLines([pick], names, day, publishedAt, nowIso)[0] : null;
}

export function alertText(a: Alert, names: Record<string, string>): string {
    if (a.kind === 'paused') return `Paused: ${a.problem || 'the game refused an order'}`;
    if (a.kind === 'game') return `The game has not answered ${a.order ? orderLine(a.order, names) + ' (#' + a.order.id + ')' : 'an order'} for ${Math.round((a.order?.age ?? 0) / 60)} min: is the game server running?`;
    if (a.kind === 'behind') return `Falling behind: ${a.events} world event${a.events === 1 ? '' : 's'} unread, the oldest ${Math.round(a.seconds ?? 0)} s old. Is the story runner running?`;
    return `Quiet for ${Math.round(a.hours ?? 0)} h while its wolves were about: nothing fired and no mission moved.`;
}

// ------------------------------------------------------------------------------------------------- wolves

/** The LIVE frame's person: [id, name, kind ("p" in the world, "o" not), cell, x, y, flags, role, doing] (dm/api.ts). */
export type FramePerson = [string, string, string, string, number, number, number, string, string];
export interface WolfNow { id: string; name: string; here: boolean; known: boolean; cell: string; x: number; y: number; doing: string }
/** Each wolf of the story as the LIVE frame has them: in the world now, or where they were saved. */
export function wolvesNow(cohort: string[], people: FramePerson[] | undefined, names: Record<string, string>): WolfNow[] {
    const frame = new Map((people ?? []).filter(p => p[2] === 'p' || p[2] === 'o').map(p => [p[0], p]));
    return cohort.map(id => {
        const p = frame.get(id);
        return p ? {id, name: p[1] || nameOf(names, id), here: p[2] === 'p', known: true, cell: p[3], x: p[4], y: p[5], doing: p[8] ?? ''}
            : {id, name: nameOf(names, id), here: false, known: false, cell: '', x: 0, y: 0, doing: ''};
    }).sort((a, b) => Number(b.here) - Number(a.here) || a.name.localeCompare(b.name));
}
export const hereCount = (cohort: string[] | undefined, people: FramePerson[] | undefined) => {
    const here = new Set((people ?? []).filter(p => p[2] === 'p').map(p => p[0]));
    return (cohort ?? []).filter(id => here.has(id)).length;
};

// ------------------------------------------------------------------------------------------------- missions

export type Cell = {state: 'done' | 'partial' | 'open' | 'later'; text: string};
export interface MissionRow { who: string; name: string; cells: Cell[]; done: boolean; contributed?: boolean }
/** A mission as a grid (doc 85, 5.3 "Quests"): a row per wolf of the story (each wolf's own checklist), or one shared
 *  row for a group or either mission; a column per objective, in order. */
export function missionGrid(m: Mission, cohort: string[], names: Record<string, string>): MissionRow[] {
    const row = (who: string, name: string): MissionRow => {
        const [step, amount] = m.tracks[who] ?? [0, 0];
        return {who, name, done: step >= m.objectives.length, cells: m.objectives.map((o, i) => {
            const goal = o.goal ?? 1;
            if (i < step) return {state: 'done', text: '✔'};
            // (A group mission's last objective can be met with too few wolves: it waits for another to help.)
            if (i === step && amount >= goal && m.mode === 'group') return {state: 'partial', text: `${amount} / ${goal}, needs ${m.requiredContributors} wolves`};
            if (i === step && amount > 0) return {state: 'partial', text: `${amount} / ${goal}`};
            if (i === step) return {state: 'open', text: goal > 1 ? `0 / ${goal}` : '-'};
            return {state: 'later', text: '·'};
        })};
    };
    if (m.mode === 'individual') return cohort.map(id => row(id, nameOf(names, id)))
        .sort((a, b) => Number(b.done) - Number(a.done) || progress(b) - progress(a) || a.name.localeCompare(b.name));
    return [row('group', m.mode === 'group' ? 'Shared (together)' : 'Shared (anyone)')];
}
const progress = (r: MissionRow) => r.cells.filter(c => c.state === 'done').length + r.cells.filter(c => c.state === 'partial').length / 2;
export function missionSummary(m: MissionSummary | Mission, wolves?: number): string {
    if (m.mode === 'individual') return `${m.completed} of ${wolves ?? '?'} done · needs ${m.required} to count${m.ready ? ' ✔' : ''}`;
    return m.ready ? 'done ✔' : 'not done yet';
}

// ------------------------------------------------------------------------------------------------- the list

export type ListFilter = 'all' | 'running' | 'paused' | 'approved' | 'ended';
export const ended = (status: string) => status === 'complete' || status === 'stopped';
export function listCounts(runs: RunRow[]): Record<ListFilter, number> & {needsYou: number} {
    return {
        all: runs.length, running: runs.filter(r => r.status === 'running').length, paused: runs.filter(r => r.status === 'paused').length,
        approved: runs.filter(r => r.status === 'approved').length, ended: runs.filter(r => ended(r.status)).length,
        needsYou: runs.filter(r => (r.alerts ?? []).some(a => a.needsYou) || r.status === 'approved').length,
    };
}
export function filterRuns(runs: RunRow[], filter: ListFilter): RunRow[] {
    return runs.filter(r => filter === 'all' || (filter === 'ended' ? ended(r.status) : r.status === filter));
}
/** Missions in a row of the list: "2 open · 1 done". */
export function questsText(missions: MissionSummary[] | undefined): string {
    if (!missions?.length) return '-';
    const done = missions.filter(m => m.ready).length;
    return `${missions.length - done} open · ${done} done`;
}
export function lastText(last: Last | null | undefined, names: Record<string, string>): string {
    if (!last) return '-';
    if (last.type === 'trigger') return `${last.trigger}${last.actor ? ' · ' + nameOf(names, last.actor) : ''}`;
    return `entered ${last.phase}`;
}
/** The phases visited in order, oldest first, from the run history (which the runner gives newest first). */
export function phasePath(history: Visit[]): string[] {
    return [...history].sort((a, b) => a.visit - b.visit).map(v => v.phase);
}
/** The phase strip: the phases this run has been through (in order) and where it can go from the current one. */
export function phaseStrip(history: Visit[], phases: {id: string; to: string[]}[], current: string | null): {path: string[]; next: string[]} {
    return {path: phasePath(history), next: phases.find(p => p.id === current)?.to ?? []};
}
