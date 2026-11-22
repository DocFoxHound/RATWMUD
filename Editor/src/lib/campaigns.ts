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
    | {on: 'admin'; problem: string; order?: Order; held?: {by: string; reason: string; at?: number; cursor?: number}}
    | {on: 'game'; order: Order; stopping?: boolean}
    | {on: 'send'; effects: Effect[]}
    | {on: 'timer'; name: string; signal: string; at: number; clock: 'game' | 'real'; actor: string; repeat: number; scope: string}
    | {on: 'clock'; at: number; what: Listening}
    | {on: 'direction'; signal: string; what: Listening}
    | {on: 'wolves'; events: string[]; what: Listening[]}
    | {on: 'end'};
export interface Alert {
    kind: 'paused' | 'held' | 'runner' | 'game' | 'behind' | 'quiet' | 'stuck' | 'overdue' | 'full'; needsYou: boolean; problem?: string; order?: Order;
    events?: number; seconds?: number; hours?: number; since?: string; wolfSeen?: string; by?: string; reason?: string; age?: number | null;
    beatAt?: string | null; runner?: boolean;
    // (Phase 3: cannot move on, a timer overdue, the audience nearly full.)
    phase?: string; reasons?: Partial<Record<Cannot, number>>; goneDays?: number; timer?: string; signal?: string; at?: number; clock?: string;
    wolves?: number; cap?: number;
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
    behind?: {events: number; seconds: number}; last?: Last | null; alerts?: Alert[]; ending?: boolean; commandsQueued?: number;
}
/** The story runner's heartbeat (doc 85, 5.6; Phase 2). */
export interface Beat { beatAt: string | null; age: number | null; startedAt?: string; pid?: number; version?: string; behind?: number }
export interface Now { day: number | null; newest: number; at: string }
export interface Overview { runs: RunRow[]; now: Now; names: Record<string, string>; thresholds?: Record<string, number>; runner?: Beat | null; speed?: number | null }
export interface Region { id: string; cell: string; x1: number; y1: number; x2: number; y2: number }
export interface Timer { signal?: string; at: number; clock?: 'game' | 'real'; actor?: string; interval?: number; scope?: string }
export interface RunState {
    phase: string; entered: number; cohort: string[]; votes: Record<string, string[]>; visits: number; actor: string; publishedAt?: number;
    counters?: {story?: Record<string, number>; player?: Record<string, Record<string, number>>}; timers?: Record<string, Timer>;
    fired?: Record<string, string[]>; campaignFired?: Record<string, string[]>; triggerEffects?: Effect[]; disabled?: string[];
    held?: {by: string; reason: string; at: number; cursor: number};
}
/** A value a firing or a command changed (the timeline's "watches 5 → 6"). */
export interface Change { what: 'counter' | 'timer'; key: string; scope?: string; wolf?: string; before: number | null; after: number | null }
export type CommandKind = 'hold' | 'resume' | 'stop' | 'signal' | 'fire' | 'enable' | 'disable' | 'enter' | 'set' | 'timer' | 'mission' | 'order' | 'tell' | 'scene';
/** An admin's command (dm.story_commands): what was asked, why, by whom, and once the runner took it, the result. */
export interface Command {
    id: number; key: string; visit: number | null; digest: string; kind: CommandKind; payload: Record<string, unknown>; reason: string; by: string;
    at: string | null; status: 'queued' | 'done' | 'refused'; doneAt: string | null;
    result: {text: string; before?: Record<string, unknown>; after?: Record<string, unknown>; atEvent?: number; visit?: number} | null;
}
/** What the controls may name for the run as it stands (tools/story_runner.controls). */
export interface ControlTrigger { id: string; on: string; signal?: string | null; key: string; off: boolean; scope: 'campaign' | 'phase'; once: string; needsWolf: boolean; hasConditions: boolean; actions: string[] }
export interface Controls {
    triggers: ControlTrigger[]; offElsewhere: string[]; phases: string[]; signals: string[]; counters: {key: string; scope: 'story' | 'player'}[];
    schedules: Record<string, {days: number; repeat?: boolean; scope?: string; clock?: string; owner?: string}>;
    missions: {id: string; title: string; mode: string; objectives: number}[]; scenes: string[]; regions: string[];
}
export interface Visit { visit: number; phase: string; day: number; cause: string; receipts: number[]; missions?: Mission[] | null }
export interface Firing {
    id: number; visit: number; phase: string; trigger: string; actor: string; day: number; actions: string[]; scope?: string; on?: string;
    signal?: string | null; event?: number | string | null; cell?: string; x?: number; y?: number;
    changes?: Change[]; byHand?: boolean; checked?: boolean; command?: number; by?: string;
}
export interface RunDetail extends RunRow {
    state: RunState | null; program?: Record<string, unknown> & {title: string; id: string}; approvedBy: string; cursor: number; now: Now;
    guestVoices: string[]; directions: {npc: string; visit: number; receipt: number; status: string}[]; signals: string[];
    history: Visit[]; trace: Firing[]; actions: {id: number; kind: string; status: string; result: string}[]; orders: Order[];
    missionsNow: Mission[]; directed: string[]; regions: Region[]; phases: {id: string; to: string[]; missions: number}[]; start: string;
    listening: Listening[]; names: Record<string, string>; runner?: Beat | null; commands?: Command[]; controls?: Controls | null;
}
export type TimelineItem =
    | {key: string; type: 'phase'; at: string | null; visit: number; phase: string; day: number; cause: string; receipts: number[]; missions?: Mission[] | null}
    | ({key: string; type: 'trigger'; at: string | null} & Firing)
    | ({key: string; type: 'order'; at: string | null} & Order)
    | ({key: string; type: 'command'; at: string | null; command: CommandKind; askedAt: string | null} & Omit<Command, 'kind' | 'at'>);
export interface WorldEvent { id: number; kind: string; actor: string; target: string; cell: string; item: string; quantity: number; coins: number; detail: string; day: number; x: number | null; y: number | null; at: string }
export interface TimelineCursor { visit?: number; trigger?: number; order?: number; command?: number; at?: string }
export interface TimelinePage { run: string; items: TimelineItem[]; events: Record<string, WorldEvent>; names: Record<string, string>; before: TimelineCursor | null; since: TimelineCursor; now: string }

export const nameOf = (names: Record<string, string>, id: string | null | undefined) => (id ? names[id] ?? id : '');

// ------------------------------------------------------------------------------------------------- the timeline

const RANK = {phase: 0, trigger: 1, order: 2, command: 3} as const;
const seqOf = (i: TimelineItem) => (i.type === 'phase' ? i.visit : i.id);
/** Newest first, as the runner sorts (tools/story_runner.timeline): by time, then visit, then a visit opened before its
 *  firings, its orders and the admin's commands, then by id. */
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
    return {visit: most('phase', previous.visit), trigger: most('trigger', previous.trigger), order: most('order', previous.order),
        command: most('command', previous.command), at};
}
export type TimelineFilter = 'all' | 'phase' | 'trigger' | 'order' | 'command';
export function filterTimeline(items: TimelineItem[], kind: TimelineFilter, wolf = '', trigger = ''): TimelineItem[] {
    const wolfOf = (i: Extract<TimelineItem, {type: 'command'}>) => String(i.payload?.actor ?? i.payload?.player ?? '');
    return items.filter(i => (kind === 'all' || i.type === kind)
        && (!wolf || (i.type === 'trigger' ? i.actor === wolf : i.type === 'order' ? i.target === wolf : i.type === 'command' ? wolfOf(i) === wolf : true))
        && (!trigger || (i.type === 'trigger' && i.trigger === trigger) || (i.type === 'command' && i.payload?.trigger === trigger)));
}

export function causeText(cause: string, names: Record<string, string> = {}): string {
    if (cause === 'published') return 'published';
    if (cause === 'clock') return 'by the clock (a timer or a delay)';
    if (cause.startsWith('event ')) return `world event #${cause.slice(6)}`;
    if (cause.startsWith('DM signal: ')) return `a Dungeon Master's direction "${cause.slice(11)}"`;
    if (cause.startsWith('admin ')) {
        const [who, ...why] = cause.slice(6).split(': ');
        return `${who.split('#')[0]} by hand: "${why.join(': ')}"`;
    }
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
        else if (w.on === 'admin' && w.held) out.push({who: 'you', tone: 'alert',
            text: `held by ${w.held.by.split('#')[0]}${w.held.reason ? ` ("${w.held.reason}")` : ''}: resume it to carry on`});
        else if (w.on === 'game' && w.stopping) out.push({who: 'the game', tone: 'game', text: `to acknowledge the stop (order #${w.order.id})`});
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
    if (a.kind === 'held') return `Held by ${(a.by ?? 'a Dungeon Master').split('#')[0]}${a.reason ? ` ("${a.reason}")` : ''}: ${a.events ?? 0} world event${a.events === 1 ? '' : 's'} wait${a.events === 1 ? 's' : ''} for it; nothing is lost.`;
    if (a.kind === 'runner') return a.age == null ? 'The story runner has not run on this world: nothing moves until it does (python3 tools/story_runner.py --world …).'
        : `The story runner has not beaten for ${a.age < 120 ? Math.round(a.age) + ' s' : Math.round(a.age / 60) + ' min'}: is it running?`;
    if (a.kind === 'game') return `The game has not answered ${a.order ? orderLine(a.order, names) + ' (#' + a.order.id + ')' : 'an order'} for ${Math.round((a.order?.age ?? 0) / 60)} min: is the game server running?`;
    if (a.kind === 'behind') return `Falling behind: ${a.events} world event${a.events === 1 ? '' : 's'} unread, the oldest ${Math.round(a.seconds ?? 0)} s old.${a.runner ? '' : ' Is the story runner running?'}`;
    if (a.kind === 'stuck') return `Cannot move on from ${a.phase}: nothing in it can still fire by itself (${stuckReasons(a)}). Fire a trigger, set a variable or go to a phase.`;
    if (a.kind === 'overdue') return `Timer ${a.signal ?? a.timer} is overdue by ${secondsText(a.seconds ?? 0, false)} and has not fired: is the story waiting on the game?`;
    if (a.kind === 'full') return `Its audience is nearly full: ${a.wolves} of ${a.cap} wolves.`;
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

export type ListFilter = 'all' | 'running' | 'held' | 'paused' | 'approved' | 'ended';
export const ended = (status: string) => status === 'complete' || status === 'stopped';
export function listCounts(runs: RunRow[]): Record<ListFilter, number> & {needsYou: number} {
    return {
        all: runs.length, running: runs.filter(r => r.status === 'running' || r.status === 'stopping').length,
        held: runs.filter(r => r.status === 'held').length, paused: runs.filter(r => r.status === 'paused').length,
        approved: runs.filter(r => r.status === 'approved').length, ended: runs.filter(r => ended(r.status)).length,
        needsYou: runs.filter(r => (r.alerts ?? []).some(a => a.needsYou) || r.status === 'approved').length,
    };
}
export function filterRuns(runs: RunRow[], filter: ListFilter): RunRow[] {
    return runs.filter(r => filter === 'all' || (filter === 'ended' ? ended(r.status) : filter === 'running' ? ['running', 'stopping'].includes(r.status) : r.status === filter));
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

// ------------------------------------------------------------------------------------------------- steering (Phase 2)

/** The statuses a command may steer (tools/story_runner.STEERED). */
export const steerable = (status: string) => ['running', 'paused', 'held'].includes(status);
/** A command's own id: sending it twice does it once (5.8, rule 3). */
export function newKey(): string {
    const bytes = new Uint8Array(12);
    crypto.getRandomValues(bytes);
    return 'c' + [...bytes].map(b => b.toString(16).padStart(2, '0')).join('');
}
/** "watches 5 → 6", "stores (Ash) 0 → 2", "timer watch-round day 56.0 → 58.0" */
export function changeText(c: Change, names: Record<string, string> = {}): string {
    const n = (v: number | null) => (v === null || v === undefined ? '—' : Number.isInteger(v) ? String(v) : v.toFixed(1));
    if (c.what === 'timer') return `timer ${c.key.split('/')[0]}${c.key.includes('/') ? ` (${nameOf(names, c.key.split('/')[1])})` : ''} ${c.before === null ? 'started' : 'day ' + n(c.before)} → ${c.after === null ? 'off' : 'day ' + n(c.after)}`;
    return `${c.key}${c.wolf ? ` (${nameOf(names, c.wolf)})` : ''} ${n(c.before)} → ${n(c.after)}`;
}
const PRETTY: Record<CommandKind, string> = {
    hold: 'held it', resume: 'resumed it', stop: 'stopped it', signal: 'sent a signal', fire: 'fired a trigger', enable: 'turned a trigger on',
    disable: 'turned a trigger off', enter: 'went to a phase', set: 'set a variable', timer: 'changed a timer', mission: 'changed a mission',
    order: 'dealt with an order', tell: 'told the wolves', scene: 'started a scene',
};
/** One line for a command: who did what, to what (before the runner's own words come back). */
export function commandLine(kind: CommandKind, payload: Record<string, unknown>, names: Record<string, string>): string {
    const p = payload ?? {};
    const wolf = (p.actor || p.player) ? ` for ${nameOf(names, String(p.actor || p.player))}` : '';
    switch (kind) {
        case 'signal': return `sent signal "${p.name}"${wolf}`;
        case 'fire': return `fired ${p.trigger}${wolf}${p.check === false ? ' (unchecked)' : ''}`;
        case 'enable': case 'disable': return `turned ${p.trigger} ${kind === 'enable' ? 'on' : 'off'}`;
        case 'enter': return `went to ${p.phase}${p.effects === false ? ' (without its entry effects)' : ''}`;
        case 'set': return `set ${p.key}${wolf} to ${p.value}`;
        case 'timer': return `${p.do} timer ${p.name ?? p.signal}${p.days !== undefined ? ` (${Number(p.days) > 0 && p.do === 'shift' ? '+' : ''}${p.days} days)` : ''}`;
        case 'mission': return `${p.do === 'complete' ? 'completed' : 'reset'} ${p.mission}${wolf}`;
        case 'order': return `${p.do === 'skip' ? 'skipped' : 'retried'} order #${p.receipt}`;
        case 'tell': return `told ${p.to === 'wolf' ? nameOf(names, String(p.player)) : p.to === 'region' ? 'the wolves in ' + p.region : 'the story\'s wolves'}: "${p.text}"`;
        case 'scene': return `started scene ${p.scene} for ${p.player === 'world' ? 'the world' : nameOf(names, String(p.player))}`;
        default: return PRETTY[kind] ?? kind;
    }
}
/** The values a command changed, before and after, in words (the Audit tab). */
export function beforeAfter(c: Pick<Command, 'result'>): string {
    const r = c.result;
    if (!r?.before || !r.after) return '';
    const keys = [...new Set([...Object.keys(r.before), ...Object.keys(r.after)])].filter(k => ['status', 'phase', 'value', 'at', 'on', 'step', 'visit'].includes(k));
    const v = (x: unknown) => (x === null || x === undefined ? '—' : typeof x === 'boolean' ? (x ? 'on' : 'off')
        : typeof x === 'number' && !Number.isInteger(x) ? x.toFixed(1) : String(x));
    return keys.filter(k => r.before![k] !== r.after![k]).map(k => `${k} ${v(r.before![k])} → ${v(r.after![k])}`).join(' · ');
}
/** Ask the reason again for these, and the story's title typed (5.8, rule 2). */
export const typedTitle = (kind: CommandKind, payload: Record<string, unknown>, way?: 'back' | 'on' | 'again') =>
    kind === 'stop' || (kind === 'order' && payload.do === 'skip') || (kind === 'enter' && way === 'back');
/** Which way a jump goes: on, back or again (doc 85, 5.1 rule 5: going back gives nothing back). */
export function wayOf(phases: string[], from: string | null, to: string): 'on' | 'back' | 'again' {
    if (from === to) return 'again';
    return phases.indexOf(to) < phases.indexOf(from ?? '') ? 'back' : 'on';
}
/** How many times each trigger fired, and when last, from the timeline the console holds. */
export function firingCounts(items: TimelineItem[]): Record<string, {count: number; last: string | null; day: number | null}> {
    const out: Record<string, {count: number; last: string | null; day: number | null}> = {};
    for (const i of items) {
        if (i.type !== 'trigger') continue;
        const o = (out[i.trigger] ??= {count: 0, last: null, day: null});
        o.count += 1;
        if (!o.last || (i.at && i.at > o.last)) { o.last = i.at; o.day = i.day; }
    }
    return out;
}

// ------------------------------------------------------------------------------------------------- explaining (Phase 3)
// What tools/story_runner.explain answers (doc 85, 5.5): every trigger and exit judged; why one has not fired; what an
// event would do; what a console command would do (a dry run on the runner's own code, rolled back).

/** A condition, judged: true, false or unknown (null), with the values behind it (story_triggers.judge). */
export interface CondNode {
    test: 'all' | 'any' | 'not' | 'counter' | 'field' | 'region' | 'mission'; verdict: boolean | null; conditions?: CondNode[]; condition?: CondNode;
    key?: string; scope?: string; field?: string; op?: string; value?: unknown; expected?: unknown; wolf?: string; why?: string; needs?: 'event' | 'wolf' | 'place';
    region?: string; id?: string;
}
export type Cannot = 'off' | 'used' | 'gone' | 'unsent';
/** One trigger or exit of the phase, judged as the run stands. */
export interface Judged extends Listening {
    key?: string; off?: boolean; used?: boolean; usedFor?: string[]; actions?: string[]; enters?: string | null; conditions?: CondNode;
    due?: number; senders?: {trigger: string; off: boolean}[]; timers?: {timer: string; signal: string; at: number; clock: string}[];
    forWolves?: string[]; needsWolves: boolean; can: boolean; cannot?: Cannot;
    votes?: {have: number; need: number; wolves: string[]}; prerequisites?: {steps: number; progress: Record<string, number>};
    ready?: string[]; missing?: string[]; participant?: boolean; minCoins?: number; minQuantity?: number;
}
export interface TimerAhead { name: string; signal: string; at: number; clock: 'game' | 'real'; actor: string; repeat: number; scope: string; left: number | null; seconds: number | null }
export interface Explained {
    run: string; phase: string; visit: number; status: string; day: number | null; listeners: Judged[]; timers: TimerAhead[]; speed: number | null;
    recent: string[]; goneDays: number; missionEvents?: string[]; stall: (Alert & {what?: {source: string; id?: string; index?: number; on: string; cannot: Cannot}[]}) | null; ending: boolean;
}
export interface Step {
    verdict: 'fired' | 'filter' | 'outsider' | 'off' | 'used' | 'conditions' | 'before' | 'other' | 'would' | 'early';
    field?: string; expected?: unknown; actual?: unknown; wolf?: string; once?: string; conditions?: CondNode; text?: string; due?: number;
}
export interface WhyNot {
    run: string; phase: string; visit: number; listener: Judged; events: (Step & {event: WorldEvent})[]; unread: number; notLogged?: string;
    recent?: {id: number; visit: number; trigger: string; day: number; at: string}[];
}
export interface Outcome {
    fired: Firing[]; changes: Change[]; missions: {mission: string; title: string; who: string; before: [number, number]; after: [number, number]; of: number}[];
    effects?: Effect[]; moved: boolean; phase: string; visit: number; entryEffects?: Effect[]; ending?: boolean; journal?: string;
}
export interface WhatIf extends Outcome { run: string; fact: Record<string, unknown>; day: number; notFired: (Step & {trigger: string})[] }
export interface Preview extends Partial<Outcome> {
    run: string; kind: CommandKind; payload: Record<string, unknown>; outcome: 'done' | 'refused' | 'waits'; text: string;
    before?: Record<string, unknown>; after?: Record<string, unknown>; orders?: {kind: string; target: string; payload: Record<string, unknown>}[];
    next?: Effect[]; status?: string;
}

const OPS: Record<string, string> = {'=': '=', '!=': '≠', '>': '>', '>=': '≥', '<': '<', '<=': '≤'};
const mark = (v: boolean | null) => (v === true ? '✔' : v === false ? '✘' : '?');
const shown = (v: unknown, names: Record<string, string>) => (typeof v === 'string' ? (v ? `"${nameOf(names, v)}"` : 'nothing') : String(v));

/** The catalogue's sentences for events and conditions (Data/Storylines/trigger-catalogue.json, doc 85 Phase 4), when the
 *  page has it: lib/workbench.ts's eventParts and conditionParts (dm/campaigns/Explain.tsx's WordingContext). Without it
 *  the console says the short forms below. */
export interface Wording {
    event?: (rule: {on: string; signal?: string; match?: Record<string, string>; afterDays?: number}) => string;
    condition?: (c: CondNode) => string;
}
/** One judged test in words: "stores ≥ 5 ✘ (stores is 2)", "item = rope ? (the event brings it)"; with the catalogue's
 *  sentence when there is one ("the story's counter stores >= 5 ✘ (stores is 2)"). */
export function condText(c: CondNode, names: Record<string, string> = {}, wording?: Wording): string {
    if (wording?.condition && !['all', 'any', 'not'].includes(c.test)) {
        const head = `${wording.condition(c)} ${mark(c.verdict)}`;
        if (c.test === 'mission') return head;
        if (c.test === 'region') {
            const v = c.value as {cell?: string; x?: number; y?: number} | undefined;
            return head + (c.needs === 'place' ? (v?.cell ? ' (the event has no tile)' : ' (the event brings its place)')
                : v?.cell ? ` (it was at ${nameOf(names, v.cell)} (${v.x}, ${v.y}))` : '');
        }
        const what = c.test === 'counter' ? c.key ?? '' : c.field ?? '';
        if (c.needs === 'event') return `${head} (the event brings it)`;
        if (c.needs === 'wolf') return `${head} (each wolf's own)`;
        if (c.why) return `${head} (${c.wolf ? nameOf(names, c.wolf) + ' is' : 'the wolf is'} ${c.why})`;
        return `${head} (${what} is ${shown(c.value, names)}${c.wolf ? ' for ' + nameOf(names, c.wolf) : ''})`;
    }
    if (c.test === 'all' || c.test === 'any') return `${c.test === 'all' ? 'all' : 'any'} of these ${mark(c.verdict)}`;
    if (c.test === 'not') return `not ${mark(c.verdict)}`;
    if (c.test === 'mission') return `mission ${c.id} is ready ${mark(c.verdict)}`;
    if (c.test === 'region') {
        const v = c.value as {cell?: string; x?: number; y?: number} | undefined;
        return `it happens inside ${c.region} ${mark(c.verdict)}${c.needs === 'place' ? (v?.cell ? ' (the event has no tile)' : ' (the event brings its place)')
            : v?.cell ? ` (it was at ${nameOf(names, v.cell)} (${v.x}, ${v.y}))` : ''}`;
    }
    const what = c.test === 'counter' ? `${c.key}${c.scope === 'player' ? ' (the wolf\'s)' : ''}` : c.field ?? '';
    const head = `${what} ${OPS[c.op ?? '='] ?? c.op} ${shown(c.expected, names)} ${mark(c.verdict)}`;
    if (c.needs === 'event') return `${head} (the event brings it)`;
    if (c.needs === 'wolf') return `${head} (each wolf's own)`;
    if (c.why) return `${head} (${c.wolf ? nameOf(names, c.wolf) + ' is' : 'the wolf is'} ${c.why})`;
    return `${head} (${what} is ${shown(c.value, names)}${c.wolf ? ' for ' + nameOf(names, c.wolf) : ''})`;
}
/** A judged condition tree as indented lines, for the Overview and [Why not?]. */
export function condLines(c: CondNode | undefined, names: Record<string, string> = {}, depth = 0, wording?: Wording): {depth: number; text: string; verdict: boolean | null}[] {
    if (!c) return [];
    const kids = c.conditions ?? (c.condition ? [c.condition] : []);
    return [{depth, text: condText(c, names, wording), verdict: c.verdict}, ...kids.flatMap(k => condLines(k, names, depth + 1, wording))];
}
/** "when a wolf gives (item rope, target Ivo Tarmarsh)", "when signal "watch-round"". */
export function eventText(l: Pick<Listening, 'on' | 'signal' | 'match'> & {missions?: string[]; afterDays?: number}, names: Record<string, string> = {}, wording?: Wording): string {
    if (wording?.event && l.on !== 'dm signal' && l.on !== 'missions completed') return 'when ' + wording.event(l);
    if (l.on === 'story signal') return `when signal "${l.signal}"`;
    if (l.on === 'clock') return `after ${l.afterDays} game day${l.afterDays === 1 ? '' : 's'} in the phase`;
    if (l.on === 'dm signal') return `when a Dungeon Master directs "${l.match?.detail ?? ''}"`;
    if (l.on === 'missions completed') return `when missions ${(l.missions ?? []).join(', ')} are done`;
    const filters = Object.entries(l.match ?? {}).map(([k, v]) => `${k} ${nameOf(names, v)}`).join(', ');
    return `when ${l.on}${filters ? ` (${filters})` : ''}`;
}
/** Where a judged trigger or exit stands, in a few words, and its tone. */
export function judgedState(l: Judged, names: Record<string, string>, day: number | null): {text: string; tone: Tone} {
    if (l.off) return {text: 'turned off', tone: 'alert'};
    if (l.used && l.once === 'visit') return {text: 'fired: once per visit', tone: 'done'};
    if (l.used && l.once === 'campaign') return {text: 'fired: once per campaign', tone: 'done'};
    if (l.used) return {text: `fired for every wolf (once per ${l.once === 'member' ? 'campaign' : 'visit'} each)`, tone: 'done'};
    if (l.cannot === 'gone') return {text: 'waits for a wolf of the story: none has been about lately', tone: 'alert'};
    if (l.cannot === 'unsent') return {text: 'nothing left can send its signal (only the console)', tone: 'alert'};
    if (l.due !== undefined && day !== null) return {text: l.due <= day ? `due now (day ${l.due.toFixed(1)})` : `due day ${l.due.toFixed(1)} (in ${days(l.due - day)})`, tone: 'normal'};
    if (l.on === 'story signal') {
        const t = (l.timers ?? [])[0];
        const by = (l.senders ?? []).filter(s => !s.off).map(s => s.trigger);
        return {text: t ? `timer ${t.timer} sends it (day ${t.at.toFixed(1)})` : by.length ? `sent by ${by.join(', ')}` : 'waits for its signal', tone: 'normal'};
    }
    if (l.votes) return {text: `${l.votes.have} of ${l.votes.need} wolves so far`, tone: 'normal'};
    if (l.missing?.length) return {text: `waits for ${l.missing.join(', ')} (done: ${l.ready?.join(', ') || 'none'})`, tone: 'normal'};
    if (l.usedFor?.length) return {text: `waits for its event · fired for ${l.usedFor.map(w => nameOf(names, w)).join(', ')}`, tone: 'normal'};
    return {text: 'waits for its event', tone: 'normal'};
}
/** "would fire for: Ash, Birch (here now)": the wolves it could fire for, those in the world first. */
export function wouldFireFor(l: Judged, here: Set<string>, names: Record<string, string>): string {
    if (!l.forWolves || l.off || l.used) return '';
    if (!l.forWolves.length) return 'for no wolf of the story now';
    const now = l.forWolves.filter(w => here.has(w)), away = l.forWolves.length - now.length;
    if (!now.length) return `for ${l.forWolves.length} ${l.forWolves.length === 1 ? 'wolf' : 'wolves'} of the story, none here now`;
    return `would fire for: ${now.slice(0, 6).map(w => nameOf(names, w)).join(', ')} (here now)${away ? ` and ${away} away` : ''}`;
}
/** "≈ 50 min" (or "in 50 min" with `about` false). */
export function secondsText(s: number, about = true): string {
    const pre = about ? '≈ ' : '';
    if (s < 120) return `${pre}${Math.round(s)} s`;
    if (s < 5400) return `${pre}${Math.round(s / 60)} min`;
    if (s < 86400 * 2) return `${pre}${Math.round(s / 360) / 10} h`;
    return `${pre}${Math.round(s / 8640) / 10} days`;
}
export function stuckReasons(a: Pick<Alert, 'reasons' | 'goneDays'>): string {
    const words: Record<Cannot, string> = {off: 'turned off', used: 'used up', gone: `waiting for wolves gone ${a.goneDays ?? 14} days`, unsent: 'a signal nothing sends'};
    return Object.entries(a.reasons ?? {}).map(([k, n]) => `${n} ${words[k as Cannot] ?? k}`).join(', ');
}
/** Why one event did not fire a trigger, in words. */
export function stepText(s: Step, names: Record<string, string> = {}): string {
    switch (s.verdict) {
        case 'fired': return 'it fired';
        case 'filter': return s.field === 'kind' ? `another kind of event (${s.actual})` : `its ${s.field} was ${shown(s.actual, names)}; it wants ${shown(s.expected, names)}`;
        case 'outsider': return `${s.wolf ? nameOf(names, s.wolf) : 'nobody'} is not in the story`;
        case 'off': return 'it is turned off';
        case 'used': return `already fired${s.wolf ? ' for ' + nameOf(names, s.wolf) : ''} (once per ${s.once})`;
        case 'conditions': return `its conditions said no: ${condLines(s.conditions, names).filter(l => l.verdict !== true).slice(0, 3).map(l => l.text).join('; ')} (today's values)`;
        case 'before': return 'before this visit of the phase opened';
        case 'early': return `before its delay (due day ${s.due?.toFixed(1)})`;
        case 'other': return s.text ?? 'for another story';
        default: return s.text ?? 'it would fire on this event now';
    }
}
/** A dry run's outcome as lines: the triggers it fires and what they change, missions, effects, the phase it lands in. */
export function outcomeLines(o: Partial<Outcome>, names: Record<string, string> = {}): string[] {
    const out: string[] = [];
    for (const f of o.fired ?? []) out.push(`fires ${f.trigger}${f.actor ? ' for ' + nameOf(names, f.actor) : ''}: ${f.actions.join(' → ')}`);
    for (const c of o.changes ?? []) if (c.before !== c.after) out.push(`sets ${changeText(c, names)}`);
    for (const m of o.missions ?? []) out.push(`${m.title}${m.who !== 'group' ? ' (' + nameOf(names, m.who) + ')' : ''}: step ${m.before[0]} → ${m.after[0]} of ${m.of}`);
    for (const e of o.effects ?? []) out.push(`then sends ${effectLine(e, names)}`);
    if (o.moved) {
        out.push(`moves to ${o.phase} (visit ${o.visit})${o.ending ? ': an ending' : ''}`);
        if (o.entryEffects?.length) out.push(`then its entry effects: ${o.entryEffects.map(e => effectLine(e, names)).join(', ')}`);
    }
    return out;
}
/** "This will do" for a console command, from its dry run (the runner's own code on the run as it stands, rolled back). */
export function previewLines(p: Preview, names: Record<string, string> = {}): string[] {
    if (p.outcome === 'refused') return [`The runner would refuse it now: ${p.text}`];
    if (p.outcome === 'waits') return [p.text];
    const out = [p.text];
    for (const o of p.orders ?? []) out.push(`sends ${o.kind}${o.target && !o.target.startsWith('world-') ? ' → ' + nameOf(names, o.target) : ''} to the game now`);
    out.push(...outcomeLines({...p, effects: [], entryEffects: []}, names).filter(l => !l.startsWith('moves to')));
    if (p.moved) out.push(`it is then in ${p.phase} (visit ${p.visit})${p.ending ? ': an ending' : ''}`);
    if (p.next?.length) out.push(`then it sends, one by one: ${p.next.map(e => effectLine(e, names)).join(', ')}`);
    return out;
}

// ------------------------------------------------------------------------------------------------- outside the tab
/** The op `alerts`: the tab bar's count and LIVE's line (doc 85, 5.6). */
export interface AlertSummary {
    runner: Beat | null; runnerDown: boolean; live: number; needsYou: number; at: string;
    runs: {run: string; title: string; status: string; kinds: string[]; text: string}[];
}
/** LIVE's line: the most pressing first (the runner down, then each story that needs you). */
export function summaryLines(s: AlertSummary | null): string[] {
    if (!s) return [];
    const out: string[] = [];
    if (s.runnerDown) out.push(s.runner?.age == null ? 'The story runner has not run on this world: no story moves until it does.'
        : `The story runner has not beaten for ${secondsText(s.runner.age, false)}: ${s.live} stor${s.live === 1 ? 'y is' : 'ies are'} not moving.`);
    out.push(...s.runs.map(r => r.text));
    return out;
}

// ------------------------------------------------------------------------------------------------- Problems (Story Creator)
/** One problem of a draft (story_program.problems and story_runner.reference_problems). */
export interface Problem {
    level: 'error' | 'warning'; text: string; path: string; world?: boolean; blocks?: boolean;
    at: {kind: string; phase?: number | null; phaseId?: string | null; index?: number; id?: string | null; part?: string};
}
export interface Problems { problems: Problem[]; errors: number; warnings: number; world: boolean; digest: string | null }
/** Where a problem is, in words: "Phase ferry-watch › trigger begin-deep-freeze › action 2". */
export function problemPlace(p: Problem): string {
    const a = p.at ?? {kind: 'program'};
    const parts: string[] = [];
    if (a.phase === null && a.kind === 'trigger') parts.push('Campaign triggers');
    else if (a.phase !== undefined && a.phase !== null) parts.push(`Phase ${a.phaseId ?? '#' + (a.phase + 1)}`);
    const one: Record<string, string> = {trigger: 'trigger', mission: 'mission', exit: 'exit', effect: 'entry effect', region: 'region', scene: 'scene', case: 'rehearsal case'};
    if (one[a.kind]) parts.push(`${one[a.kind]} ${a.id ?? '#' + ((a.index ?? 0) + 1)}`);
    else if (a.kind === 'settings') parts.push('Campaign settings');
    else if (a.kind === 'program') parts.push('The program');
    else if (a.kind === 'tests') parts.push('Rehearsal cases');
    else if (!parts.length) parts.push(a.kind);
    const part = a.part ?? '';
    const n = /\[(\d+)\]/.exec(part);
    if (part === 'if') parts.push('conditions');
    else if (part.startsWith('actions')) parts.push(`action ${Number(n?.[1] ?? 0) + 1}`);
    else if (part.startsWith('objectives')) parts.push(`objective ${Number(n?.[1] ?? 0) + 1}`);
    else if (part.startsWith('events')) parts.push(`fact ${Number(n?.[1] ?? 0) + 1}`);
    else if (part.startsWith('prerequisites')) parts.push(`prerequisite ${Number(n?.[1] ?? 0) + 1}`);
    return parts.join(' › ');
}
/** The Problems list in order: errors, then what would stop approval, then the other warnings; the rehearsal's untried
 *  triggers (from its last report) join the warnings. */
export function problemList(found: Problem[], uncoveredTriggers: string[] = [], phases: string[] = []): Problem[] {
    const untried: Problem[] = uncoveredTriggers.map(name => {
        const [scope, id] = name.startsWith('Campaign / ') ? [null, name.slice(11)] : [name.split('/')[0], name.split('/').slice(1).join('/')];
        const phase = scope === null ? null : phases.indexOf(scope);
        return {level: 'warning', text: 'No rehearsal case reaches this trigger.', path: '', at: {kind: 'trigger', phase: phase === -1 ? undefined : phase,
            phaseId: scope, id}};
    });
    const rank = (p: Problem) => (p.level === 'error' ? 0 : p.blocks ? 1 : 2);
    return [...found, ...untried].map((p, i) => ({p, i})).sort((a, b) => rank(a.p) - rank(b.p) || a.i - b.i).map(x => x.p);
}
