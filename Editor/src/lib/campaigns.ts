// The Campaigns console's data shaping (Docs/Design/85-story-triggers-and-campaign-control.md, section 5, Phase 1): what
// the story runner's reads (tools/story_runner.py: status, timeline) mean in words, without React, so it is tested on
// its own (campaigns.test.ts). Ids are shown by their names wherever the reads gave one.

export type OrderStatus = 'queued' | 'applied' | 'refused' | 'expired';
export interface Order {
    id: number; kind: string; target: string; status: OrderStatus; result: string; by: string; at: string | null;
    doneAt: string | null; age?: number; visit?: number | null;
    /** Doc 85, Phase 7: a batch (story.effects), its orders each with its own outcome. */
    items?: OrderItem[];
}
export interface OrderItem {n: number; kind: string; target: string; ifRefused: 'pause' | 'skip' | 'when here'; status: OrderStatus | 'held'; result: string}
/** An effect the game refused that waits for its wolf to come back (doc 85, Phase 7: ifRefused "when here"). */
export interface HeldForWolf {wolf: string; kind: string; until: number; refused: string; receipt: number}
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
    // (Doc 86, R2: the phase moves on when ready: by itself, at its moment, or when an admin presses Advance.)
    | {on: 'ready'; to: string; auto: boolean; then?: {on: string; match?: Record<string, string>} | null; met: boolean;
        waitingFor?: {on: string; match?: Record<string, string>} | null; autoAfterDays?: number | null}
    | {on: 'end'};
export interface Alert {
    kind: 'paused' | 'held' | 'runner' | 'game' | 'behind' | 'quiet' | 'stuck' | 'overdue' | 'full' | 'ready' | 'slow'; needsYou: boolean; problem?: string; order?: Order;
    events?: number; seconds?: number; hours?: number; since?: string; wolfSeen?: string; by?: string; reason?: string; age?: number | null;
    beatAt?: string | null; runner?: boolean;
    // (Phase 3: cannot move on, a timer overdue, the audience nearly full.)
    phase?: string; reasons?: Partial<Record<Cannot, number>>; goneDays?: number; timer?: string; signal?: string; at?: number; clock?: string;
    wolves?: number; cap?: number;
    // (Doc 86, R2: ready to advance (where to, since when, how many parts are met), or slow (after how many days, its furthest part).)
    to?: string; metAt?: number | null; met?: number; parts?: number; autoAfterDays?: number | null; days?: number; slowAfterDays?: number | null;
    part?: ReadyPart | null; words?: string;
}
/** One part of a phase's Readiness as the runner measured it (story_program.readiness): its progress and its need. */
export interface ReadyPart {
    kind: 'participation' | 'minDays' | 'value' | 'test' | 'missions' | 'quest' | 'any'; label: string; met: boolean;
    value: number | null; need: number | null;
    /** participation: out of (active, all, online), how many (|D|), the share now, the share asked, the floor and the cap, the audience. */
    of?: string; count?: number | null; share?: number | null; percent?: number; floor?: number; cap?: number;
    audience?: {all: number; active: number; online: number | null};
    /** minDays: since the phase or the chapter, and the held days not counted. */
    since?: 'phase' | 'chapter'; held?: number; quest?: string;
    any?: ReadyPart[];
}
/** A phase's Readiness (doc 86, R2): met (latched once every part is, or pushed), whether it goes by itself, where to. */
export interface Readiness {
    phase: string; visit: number; met: boolean; auto: boolean; to: string; metAt: number | null;
    pushed?: {by: string; reason: string; day: number; when: 'then'; anyway?: boolean} | null;
    then?: {on: string; match?: Record<string, string>} | null; thenWords?: string | null; waitingFor?: {on: string; match?: Record<string, string>} | null;
    slow?: boolean; slowNeedsYou?: boolean; parts: ReadyPart[]; days?: number; heldDays?: number; checkedAt?: number | null; held?: boolean;
    activeDays?: number; slowAfterDays?: number | null; autoAfterDays?: number | null;
}
export interface ReadyLogEntry { day: number | null; kind: 'ready' | 'slow' | 'pushed' | 'advanced' | 'taken'; text: string; phase: string; visit: number; by?: string; at?: string }
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
    /** The story's own id and format (1 or 2), and whether it is moving onto a new version (doc 85, Phase 5b). */
    storyId?: string; format?: number; moving?: boolean;
    /** Phase 6: who takes part (a living audience: open or everyone, with its cap). */
    audience?: Audience;
    /** Doc 86, R2: the phase's Readiness, when it has one. */
    ready?: Readiness;
}
/** The audience of a run (doc 85, Phase 6): its kind, its cap, how many take part, and whether wolves may still join. */
export interface Audience { kind: string; cap?: number | null; size?: number; joinable?: boolean }
/** The story runner's heartbeat (doc 85, 5.6; Phase 2). */
export interface Beat { beatAt: string | null; age: number | null; startedAt?: string; pid?: number; version?: string; behind?: number }
export interface Now { day: number | null; newest: number; at: string }
export interface Overview { runs: RunRow[]; now: Now; names: Record<string, string>; thresholds?: Record<string, number>; runner?: Beat | null; speed?: number | null;
    /** Phase 10: the campaigns as their chapters. */
    campaigns?: Campaign[] }
/** A campaign (doc 85, Phase 10): stories approved together, in order, each a chapter that the one before it starts (story.start). */
export interface Campaign { id: string; title: string; approvedBy: string; approvedAt: string | null; status: string; chapter?: number;
    chapters: {story: string; title: string; digest: string; starts: string[]; runs: {run: string; status: string; phase: string | null; at: string | null}[]}[] }
/** A chapter a run is to start, waiting (story.start: for now, or for this one's end). */
export interface ChapterStart { story: string; audience: 'same' | 'own' | 'list'; when?: 'ended' | 'now'; carry?: boolean; wolves?: string[]; day?: number }
/** One of the world's variables (Phase 10: declared for the world in the console; Phase 5: written by a story). */
export interface WorldVar { name: string; type: string; value: Value | null; declared?: boolean; initial?: Value | null; note?: string; declaredBy?: string | null;
    updatedBy?: string | null; updatedAt?: string | null; run?: string | null; unset?: boolean;
    stories: {run: string | null; story: string; title: string; type: string; status: string; campaign?: string}[] }
export interface WorldVars { variables: WorldVar[]; declarations: boolean; audit?: {at: number; name: string; detail: string; who: string}[] }
export interface Region { id: string; cell: string; x1: number; y1: number; x2: number; y2: number }
export interface Timer { signal?: string; at: number; clock?: 'game' | 'real'; actor?: string; interval?: number; scope?: string }
export interface RunState {
    phase: string; entered: number; cohort: string[]; votes: Record<string, string[]>; visits: number; actor: string; publishedAt?: number;
    /** The variables (version 1's counters): the story's and the world's, each wolf's and each party's (doc 85, 3.2). */
    counters?: {story?: Record<string, Value>; world?: Record<string, Value>; player?: Record<string, Record<string, Value>>; party?: Record<string, Record<string, Value>>};
    timers?: Record<string, Timer>;
    fired?: Record<string, string[]>; campaignFired?: Record<string, string[]>; triggerEffects?: Effect[]; disabled?: string[];
    held?: {by: string; reason: string; at: number; cursor: number};
    /** Version 2: the actions waiting (Wait), how often each trigger fired, and how the story (or one wolf's part) ended. */
    waiting?: {trigger: string; scope: string; phase: string; at: number; clock: string; actor: string}[];
    counts?: {visit?: Record<string, number>; run?: Record<string, number>};
    outcome?: {outcome: string; text: string; day: number}; outcomes?: Record<string, {outcome: string; text: string; day: number}>;
    parties?: Record<string, string>;
}
/** A variable's value: a number, on or off, a text or id, a list, or (a timer) the day it started. */
export type Value = number | boolean | string | (number | string)[] | null;
/** A variable an admin may set (story_runner.variables): a version 2 story's declared ones, a version 1 story's counters. */
export interface ControlVariable { key: string; scope: 'story' | 'player' | 'party' | 'world'; type?: string; initial?: Value; show?: string; note?: string; clock?: string }
/** A value in words: 3, on, Ash Greymantle, "rope, axle", since day 12.5. */
export function valueText(v: Value | undefined, type: string | undefined, names: Record<string, string> = {}): string {
    if (v === undefined) return '';
    if (v === null) return type === 'timer' ? 'not started' : '—';
    if (typeof v === 'boolean') return v ? 'on' : 'off';
    if (Array.isArray(v)) return v.length ? v.map(x => typeof x === 'string' ? nameOf(names, x) : String(x)).join(', ') : '(empty)';
    if (typeof v === 'number') return type === 'timer' ? 'since day ' + (Number.isInteger(v) ? v : v.toFixed(2)) : Number.isInteger(v) ? String(v) : v.toFixed(2);
    return type && type !== 'text' ? nameOf(names, v) : '“' + v + '”';
}
/** A value a firing or a command changed (the timeline's "watches 5 → 6"). */
export interface Change { what: 'counter' | 'timer'; key: string; scope?: string; wolf?: string; before: Value; after: Value; type?: string }
export type CommandKind = 'hold' | 'resume' | 'stop' | 'signal' | 'fire' | 'enable' | 'disable' | 'enter' | 'set' | 'timer' | 'mission' | 'order' | 'tell' | 'scene' | 'migrate'
    | 'objective' | 'quest' | 'participant' | 'story-event' | 'advance' | 'foe';
/** One thing a move onto a new version keeps, resets, defaults, drops or adds (story_program.migration), with why. */
export interface MigrationLine {
    what: 'phase' | 'variable' | 'timer' | 'wait' | 'mission' | 'fired' | 'count' | 'off' | 'vote' | 'steps' | 'seed'; key: string;
    scope?: string; wolf?: string; party?: string; value?: Value; why?: string; renamed?: string; at?: number; wolves?: string[];
}
interface Changes { added: string[]; removed: string[]; changed: {id: string; what: string[]; wordsOnly?: boolean}[] }
/** What changed from one approved program to another (story_program.version_diff). */
export interface VersionDiff {
    triggers: Changes & {newlyOff?: string[]}; phases: Changes; regions: Changes; scenes: Changes;
    variables: {added: {id: string; scope: string; type: string}[]; removed: {id: string; scope: string}[]; retyped: {id: string; scope: string; from: string; to: string}[];
        rescoped: {id: string; from: string; to: string}[]; changed: {id: string; scope: string; what: string[]}[]; renamed?: {id: string; scope: string; to: string}[]};
    missions: {added: {phase: string; id: string}[]; removed: {phase: string; id: string}[]; changed: {phase: string; id: string; what: string[]; objectives: boolean}[]};
    timers: {added: string[]; removed: string[]; changed: string[]}; header: string[]; tests: boolean; same: boolean;
}
export interface MigrationReport {
    kept: MigrationLine[]; reset: MigrationLine[]; defaulted: MigrationLine[]; dropped: MigrationLine[]; added: MigrationLine[]; warnings: string[];
    blocked: string | null; diff?: VersionDiff; phase?: {from: string; to: string | null; choices: string[]; gone: boolean};
    from?: {digest: string; version: number; title: string}; to?: {digest: string; version: number; title: string};
}
/** A version a run has been on, or one approved for it (dm.story_versions, migration 0074). */
export interface Version {
    id: number; digest: string; approvedBy: string; approvedAt: string | null; status: 'approved' | 'moving' | 'current' | 'past' | 'refused' | 'withdrawn';
    from: string | null; movedAt: string | null; movedBy: string | null; command: number | null; visit: number | null; day: number | null;
    report: MigrationReport | null; order: number | null; settledAt: string | null; problem: string; title?: string;
}
/** An admin's command (dm.story_commands): what was asked, why, by whom, and once the runner took it, the result. */
export interface Command {
    id: number; key: string; visit: number | null; digest: string; kind: CommandKind; payload: Record<string, unknown>; reason: string; by: string;
    at: string | null; status: 'queued' | 'done' | 'refused'; doneAt: string | null;
    result: {text: string; before?: Record<string, unknown>; after?: Record<string, unknown>; atEvent?: number; visit?: number} | null;
}
/** What the controls may name for the run as it stands (tools/story_runner.controls). */
export interface ControlTrigger { id: string; on: string; signal?: string | null; key: string; off: boolean; scope: 'campaign' | 'phase'; once: string; needsWolf: boolean; hasConditions: boolean; actions: string[] }
export interface Controls {
    triggers: ControlTrigger[]; offElsewhere: string[]; phases: string[]; signals: string[]; counters: ControlVariable[];
    schedules: Record<string, {days: number; repeat?: boolean; scope?: string; clock?: string; owner?: string}>;
    missions: {id: string; title: string; mode: string; objectives: number}[]; scenes: string[]; regions: string[];
    /** Phase 6: the quests a command may name, with their objectives' ids. */
    quests?: {id: string; title: string; objectives: string[]}[];
    /** Doc 86, R2: the phase's Advance, when it has Readiness. */
    ready?: {to: string; auto: boolean; then: {on: string; match?: Record<string, string>} | null; thenWords: string | null; met: boolean} | null;
}
export interface Visit { visit: number; phase: string; day: number; cause: string; receipts: number[]; missions?: Mission[] | null }
export interface Firing {
    id: number; visit: number; phase: string; trigger: string; actor: string; day: number; actions: string[]; scope?: string; on?: string;
    signal?: string | null; event?: number | string | null; cell?: string; x?: number; y?: number;
    changes?: Change[]; byHand?: boolean; checked?: boolean; command?: number; by?: string;
    /** Version 2: Note actions' lines, and how it fired (by a Run, or resumed after a Wait). */
    notes?: string[]; via?: 'run' | 'wait';
    /** Doc 86, C9: a figure of the realm the story set or cleared, and the row it replaced (null: there was none). */
    figure?: {id: string; previous: Record<string, unknown> | null};
}
/** Doc 86, C10: one of a run's foes now (story_runner.foes_now): a foe it brought, or a resident it made one (outlaw). */
export interface FoeNow { camp: string; foe: string; outlaw: boolean; name: string; strength: string; band: number; left?: number | null;
    fighting: boolean; endsAt?: number | null }
/** The Foes panel's words for one: who, how strong, the band standing, and the time left (as of the world's last save). */
export function foeLine(f: FoeNow, names: Record<string, string>, nowSeconds: number): string {
    const who = f.outlaw ? `${nameOf(names, f.foe)} (a resident made a foe)` : f.name ? `${f.name} (${f.foe})` : f.foe;
    const band = !f.band ? 'no followers' : f.left === undefined || f.left === null ? `a band of ${f.band}` : `${f.left} of ${f.band} followers standing`;
    const left = f.endsAt ? Math.max(0, Math.round((f.endsAt - nowSeconds) / 60)) : null;
    return `${who}, ${f.strength}, ${band}${left === null ? '' : `, ${left === 0 ? 'its time all but up' : `about ${left} minute${left === 1 ? '' : 's'} left`}`}${f.fighting ? ' · fighting now' : ''}`;
}
export interface RunDetail extends RunRow {
    state: RunState | null; program?: Record<string, unknown> & {title: string; id: string}; approvedBy: string; cursor: number; now: Now;
    guestVoices: string[]; directions: {npc: string; visit: number; receipt: number; status: string}[]; signals: string[];
    history: Visit[]; trace: Firing[]; actions: {id: number; kind: string; status: string; result: string}[]; orders: Order[];
    missionsNow: Mission[]; directed: string[]; regions: Region[]; phases: {id: string; to: string[]; missions: number}[]; start: string;
    listening: Listening[]; names: Record<string, string>; runner?: Beat | null; commands?: Command[]; controls?: Controls | null;
    versions?: Version[];
    /** Phase 6: the story's quests with every copy (each wolf's, each party's, the story's), the audience, and each wolf's party. */
    quests?: QuestBoard[]; audience?: Audience; parties?: Record<string, string>;
    /** Phase 7: effects the game refused, held for their wolf until they are next in the world. */
    whenHere?: HeldForWolf[];
    /** Doc 86, C10: the run's foes in the world, as the game last saved them (the Foes panel). */
    foes?: FoeNow[];
    /** Phase 10: the campaign it is a chapter of; the chapters it waits to start, has started, and the one that started it. */
    campaign?: Campaign | null; chapterStarts?: ChapterStart[]; chaptersStarted?: {story: string; run?: string; refused?: string}[];
    chapterOf?: {campaign: string; from: string; story: string; chapter: number} | null;
    /** Doc 86, C8: what the residents remember of this run, as the game last saved it. */
    memory?: Memory[];
    /** Doc 86, R2: readiness met, slow, pushed, advanced and taken (the last 20). */
    readyLog?: ReadyLogEntry[];
}
export type QuestStatus = 'offered' | 'active' | 'ready' | 'done' | 'failed' | 'abandoned' | 'expired';
/** One copy of a quest (story_runner.status's `quests`): a wolf's own, a party's, or the story's one copy. */
export interface QuestCopy {
    holder: string; kind: 'wolf' | 'party' | 'story'; members: string[]; left?: string[]; status: QuestStatus | string; began?: number | null; ended?: number | null;
    times?: number; objectives: Record<string, {n?: number; done?: boolean; failed?: boolean; shown?: boolean}>;
}
export interface QuestBoard {
    id: string; title: string; summary?: string; scope: 'player' | 'party' | 'story'; lifetime?: string; repeat?: string; giver?: string | null; open?: boolean;
    objectives: {id: string; label: string; optional?: boolean; hidden?: boolean; goal?: number; measure?: string; after?: string[]}[];
    copies: QuestCopy[]; counts?: Partial<Record<QuestStatus, number>>; starts?: number; cap?: number | null; limit?: number | null;
}
export type TimelineItem =
    | {key: string; type: 'phase'; at: string | null; visit: number; phase: string; day: number; cause: string; receipts: number[]; missions?: Mission[] | null}
    | ({key: string; type: 'trigger'; at: string | null} & Firing)
    | ({key: string; type: 'order'; at: string | null} & Order)
    | ({key: string; type: 'command'; at: string | null; command: CommandKind; askedAt: string | null} & Omit<Command, 'kind' | 'at'>)
    | ({key: string; type: 'version'; at: string | null; phase: string | null; phaseFrom: string | null; warnings: string[];
        counts: Record<'kept' | 'reset' | 'defaulted' | 'dropped' | 'added', number>} & Omit<Version, 'report'>)
    // (Doc 86, R2: a readiness entry from the run's own log.)
    | ({key: string; type: 'ready'; at: string | null; readyKind: ReadyLogEntry['kind']} & Omit<ReadyLogEntry, 'at'>);
export interface WorldEvent { id: number; kind: string; actor: string; target: string; cell: string; item: string; quantity: number; coins: number; detail: string; day: number; x: number | null; y: number | null; at: string }
export interface TimelineCursor { visit?: number; trigger?: number; order?: number; command?: number; version?: number; ready?: number; at?: string }
export interface TimelinePage { run: string; items: TimelineItem[]; events: Record<string, WorldEvent>; names: Record<string, string>; before: TimelineCursor | null; since: TimelineCursor; now: string }

export const nameOf = (names: Record<string, string>, id: string | null | undefined) => (id ? names[id] ?? id : '');

// ------------------------------------------------------------------------------------------------- the timeline

const RANK = {phase: 0, trigger: 1, order: 2, command: 3, version: 4, ready: 5} as const;
const seqOf = (i: TimelineItem) => (i.type === 'phase' ? i.visit : i.type === 'ready' ? Math.round((i.day ?? 0) * 1000) : i.id);
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
    const most = (type: Exclude<TimelineItem['type'], 'ready'>, had?: number) => Math.max(had ?? 0, ...items.filter(i => i.type === type).map(seqOf));
    return {visit: most('phase', previous.visit), trigger: most('trigger', previous.trigger), order: most('order', previous.order),
        command: most('command', previous.command), version: most('version', previous.version), at};
}
export type TimelineFilter = 'all' | 'phase' | 'trigger' | 'order' | 'command';
export function filterTimeline(items: TimelineItem[], kind: TimelineFilter, wolf = '', trigger = ''): TimelineItem[] {
    const wolfOf = (i: Extract<TimelineItem, {type: 'command'}>) => String(i.payload?.actor ?? i.payload?.player ?? '');
    return items.filter(i => (kind === 'all' || i.type === kind || (kind === 'command' && (i.type === 'version' || i.type === 'ready')))
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

export function orderLine(o: Order | OrderItem, names: Record<string, string>): string {
    if ('items' in o && o.items?.length) return `${o.items.length} orders together (${[...new Set(o.items.map(i => i.kind))].join(', ')})`;
    return `${o.kind}${o.target && !o.target.startsWith('world-') ? ' → ' + nameOf(names, o.target) : ''}`;
}
/** A batch's order a paused story is stuck on (the first refused one whose story pauses), if any. */
export function stuckItem(o: Order): OrderItem | undefined {
    return o.items?.find(i => i.status === 'refused' && i.ifRefused === 'pause') ?? o.items?.find(i => i.status === 'held');
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
        else if (w.on === 'ready') out.push(readyWaitLine(w));
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
    const pick = first ?? all.find(w => w.on === 'ready' && w.met && !w.auto && !w.waitingFor) ?? timed ?? all.find(w => w.on === 'direction')
        ?? all.find(w => w.on === 'ready') ?? all.find(w => w.on === 'wolves') ?? all.find(w => w.on === 'end');
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
    // (Doc 86, R2 rules 6 and 7, and decisions §6.2's "Ready to push" for a slow chapter that needs you.)
    if (a.kind === 'ready') return `Ready to advance: ${a.phase} → ${a.to}${a.metAt != null ? ` since day ${dayText(a.metAt)}` : ''} (all ${a.parts ?? 0} parts met)`
        + (a.autoAfterDays ? `; it goes by itself ${a.autoAfterDays} days after` : '');
    if (a.kind === 'slow') return a.needsYou ? `Ready to push: ${a.words || a.part?.label || 'the furthest part'}. Advance anyway, or wait.`
        : `Slow: ${a.part ? partShort(a.part) : 'not ready'} after ${Math.floor(a.days ?? 0)} days; Advance anyway, or wait`;
    return `Quiet for ${Math.round(a.hours ?? 0)} h while its wolves were about: nothing fired and no mission moved.`;
}

// ------------------------------------------------------------------------------------------------- readiness (doc 86, R3)

const dayText = (d: number) => (Number.isInteger(d) ? String(d) : d.toFixed(1));
const OF_WORDS: Record<string, string> = {active: 'active', all: 'story', online: 'online'};
/** A part in short: "21% of the 35% needed", "11 of 10 days", "2 of 3". */
export function partShort(p: ReadyPart): string {
    if (p.kind === 'any') return (p.any ?? []).map(partShort).join(' or ');
    if (p.kind === 'participation') return p.count == null ? 'who is online is not known' : `${p.share ?? 0}% of the ${p.percent}% needed`;
    if (p.kind === 'minDays') return `${dayText(p.value ?? 0)} of ${dayText(p.need ?? 0)} days`;
    if (p.kind === 'test') return p.met ? 'met' : 'not yet';
    return `${p.value ?? '—'} of ${p.need ?? '—'}`;
}
/** The fill (0 to 1) of a part toward its need; 1 when met. */
export function partFill(p: ReadyPart): number {
    if (p.met) return 1;
    if (p.kind === 'any') return Math.max(0, ...(p.any ?? []).map(partFill));
    if (p.value == null || !p.need) return 0;
    return Math.max(0, Math.min(1, p.value / p.need));
}
export interface ReadyBar { label: string; fill: number; mark: number; text: string; met: boolean; kind: ReadyPart['kind']; any?: ReadyBar[] }
/** One bar per part (doc 86, R3): `fill` the progress, `mark` where the need is on it (participation: the share needed of
 *  those counted), and the words: "42 of 118 active wolves (36%); 35% and at least 5 needed", "11.2 of 10 days in the chapter
 *  (1.5 held days not counted)". */
export function readyBars(r: Readiness | null | undefined, names: Record<string, string> = {}): ReadyBar[] {
    void names;
    const bar = (p: ReadyPart): ReadyBar => {
        if (p.kind === 'any') {
            const kids = (p.any ?? []).map(bar);
            return {label: p.label, fill: partFill(p), mark: 1, met: p.met, kind: p.kind, any: kids, text: kids.map(k => k.text).join(' · or · ')};
        }
        if (p.kind === 'participation') {
            const of = OF_WORDS[p.of ?? 'active'] ?? p.of;
            if (p.count == null) return {label: p.label, fill: 0, mark: 1, met: false, kind: p.kind, text: `who is ${of} is not known yet`};
            const mark = p.count ? Math.min(1, (p.need ?? 0) / p.count) : 1;
            const text = `${p.value ?? 0} of ${p.count} ${of} wolves (${p.share ?? 0}%); ${p.percent}% and at least ${p.floor ?? 1} needed`
                + (p.cap ? `, ${p.cap} always enough` : '') + (p.met ? '' : ` (${p.need ?? '—'} wolves)`);
            return {label: p.label, fill: p.count ? Math.min(1, (p.value ?? 0) / p.count) : 0, mark, met: p.met, kind: p.kind, text};
        }
        if (p.kind === 'minDays') {
            const held = p.held ? ` (${dayText(p.held)} held day${p.held === 1 ? '' : 's'} not counted)` : '';
            return {label: p.label, fill: partFill(p), mark: 1, met: p.met, kind: p.kind,
                text: `${dayText(p.value ?? 0)} of ${dayText(p.need ?? 0)} days in the ${p.since === 'chapter' ? 'chapter' : 'phase'}${held}`};
        }
        if (p.kind === 'test') return {label: p.label, fill: p.met ? 1 : 0, mark: 1, met: p.met, kind: p.kind, text: p.met ? 'met' : 'not yet'};
        return {label: p.label, fill: partFill(p), mark: 1, met: p.met, kind: p.kind, text: `${p.value ?? '—'} of ${p.need ?? '—'}`};
    };
    return (r?.parts ?? []).map(bar);
}
/** The Readiness header: "Ready to advance to ch1_end (since day 93)", "Not ready: 2 of 4 met", "Waiting for the next day
 *  (ready since day 93)", "Goes on by itself when ready". */
export function readyHeader(r: Readiness | null | undefined): {text: string; tone: 'ready' | 'waiting' | 'not'} {
    if (!r) return {text: 'Not measured yet', tone: 'not'};
    const since = r.metAt != null ? ` since day ${dayText(r.metAt)}` : '';
    if (r.met && r.waitingFor) return {text: `Waiting for ${r.thenWords ?? r.waitingFor.on} (ready${since})`, tone: 'waiting'};
    if (r.met && r.pushed) return {text: `Pushed by ${r.pushed.by.split('#')[0]}: waiting for ${r.thenWords ?? 'its moment'}`, tone: 'waiting'};
    if (r.met) return {text: `Ready to advance to ${r.to}${since ? ` (${since.trim()})` : ''}`, tone: 'ready'};
    const parts = r.parts ?? [];
    const met = parts.filter(p => p.met).length;
    return {text: r.auto ? `Goes on by itself when ready: ${met} of ${parts.length} met` : `Not ready: ${met} of ${parts.length} met`, tone: 'not'};
}
/** The list's compact readiness: the furthest part not met, and "ready" when met. */
export function readySummary(r: Readiness | null | undefined): {text: string; fill: number; met: boolean} | null {
    if (!r) return null;
    if (r.met) return {text: 'ready', fill: 1, met: true};
    const left = (r.parts ?? []).filter(p => !p.met).sort((a, b) => partFill(b) - partFill(a))[0];
    return left ? {text: `${left.label}: ${partShort(left)}`, fill: partFill(left), met: false} : {text: 'measuring…', fill: 0, met: false};
}
/** The chapters bar's line: "Ch 1 · 36% of 35% · day 11 of 10 · ready". */
export function chapterReadyText(r: Readiness | null | undefined, chapter: number): string {
    if (!r) return '';
    const bits = [`Ch ${chapter}`];
    for (const p of r.parts ?? []) {
        if (p.kind === 'participation' && p.count != null) bits.push(`${p.share ?? 0}% of ${p.percent}%`);
        else if (p.kind === 'minDays') bits.push(`day ${Math.floor(p.value ?? 0)} of ${dayText(p.need ?? 0)}`);
    }
    bits.push(r.met ? 'ready' : 'not ready');
    return bits.join(' · ');
}
function readyWaitLine(w: Extract<Waiting, {on: 'ready'}>): WaitLine {
    if (w.met && w.waitingFor) return {who: 'its moment', tone: 'normal', text: `ready: goes on to ${w.to} at the next ${w.waitingFor.on}`};
    if (w.met && !w.auto) return {who: 'you', tone: 'alert', text: `ready: waits for an admin to advance it to ${w.to}`
        + (w.autoAfterDays ? ` (or goes by itself ${w.autoAfterDays} days after)` : '')};
    if (w.auto) return {who: 'readiness', tone: 'normal', text: `goes on by itself to ${w.to} when ready${w.then ? ', at the next ' + w.then.on : ''}`};
    return {who: 'readiness', tone: 'normal', text: `when ready, waits for an admin to advance it to ${w.to}`};
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
    // (A run moved onto a new version that no longer has its phase carries on, in the same visit, as the phase it landed in:
    // doc 85, Phase 5b. The strip ends there.)
    const path = phasePath(history);
    if (current && path.length && path[path.length - 1] !== current) path.push(current);
    return {path, next: phases.find(p => p.id === current)?.to ?? []};
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
    const n = (v: Value) => (v === null || v === undefined ? '—' : typeof v === 'number' ? (Number.isInteger(v) ? String(v) : v.toFixed(1)) : valueText(v, c.type, names));
    if (c.what === 'timer') return `timer ${c.key.split('/')[0]}${c.key.includes('/') ? ` (${nameOf(names, c.key.split('/')[1])})` : ''} ${c.before === null ? 'started' : 'day ' + n(c.before)} → ${c.after === null ? 'off' : 'day ' + n(c.after)}`;
    return `${c.key}${c.wolf ? ` (${nameOf(names, c.wolf)})` : ''} ${n(c.before)} → ${n(c.after)}`;
}
const PRETTY: Record<CommandKind, string> = {
    hold: 'held it', resume: 'resumed it', stop: 'stopped it', signal: 'sent a signal', fire: 'fired a trigger', enable: 'turned a trigger on',
    disable: 'turned a trigger off', enter: 'went to a phase', set: 'set a variable', timer: 'changed a timer', mission: 'changed a mission',
    order: 'dealt with an order', tell: 'told the wolves', scene: 'started a scene', migrate: 'moved it onto a new version',
    objective: 'changed an objective', quest: 'changed a quest', participant: 'changed who takes part', 'story-event': 'heard of another story',
    advance: 'advanced it (its Readiness)', foe: 'sent a foe away',
};
const OBJ_WORDS: Record<string, string> = {complete: 'completed', progress: 'added to', reveal: 'revealed', fail: 'failed', reset: 'reset'};
const QUEST_WORDS: Record<string, string> = {give: 'gave', take: 'took away', complete: 'completed', fail: 'failed'};
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
        case 'order': return `${p.do === 'skip' ? 'skipped' : p.do === 'when-here' ? 'held for its wolf' : 'retried'} order #${p.receipt}`;
        case 'tell': return `told ${p.to === 'wolf' ? nameOf(names, String(p.player)) : p.to === 'region' ? 'the wolves in ' + p.region : 'the story\'s wolves'}: "${p.text}"`;
        case 'scene': return `started scene ${p.scene} for ${p.player === 'world' ? 'the world' : nameOf(names, String(p.player))}`;
        case 'migrate': return `moved it onto approved version #${p.version}${p.phase ? `, landing in ${p.phase}` : ''}`;
        // (Phase 6: quests and who takes part.)
        case 'objective': return `${OBJ_WORDS[String(p.do)] ?? p.do} ${p.objective} of ${p.quest}${p.do === 'progress' ? ` (+${p.amount ?? 1})` : ''}${p.player === 'story' ? ' (the story\'s copy)' : wolf}`;
        case 'quest': return `${QUEST_WORDS[String(p.do)] ?? p.do} the quest ${p.quest}${p.player === 'story' ? ' (the story\'s copy)' : wolf}`;
        case 'participant': return `${p.do === 'add' ? 'took' : 'took out'} ${nameOf(names, String(p.player))} ${p.do === 'add' ? 'into' : 'of'} the story`;
        // (Phase 10: another story of the world started or ended, told by the story runner itself.)
        case 'story-event': { const e = (p.event ?? {}) as Record<string, string>;
            return `heard that the story ${e.target} ${e.kind === 'story started' ? 'started' : 'ended' + (e.detail ? ` (${e.detail})` : '')}`; }
        // (Doc 86, R2: the phase's ready exit, now, anyway or at its moment; `to` and `notReady` are the console's own words.)
        case 'advance': { const to = p.to ? ` to ${p.to}` : '';
            if (p.when === 'then') return `Advance${to} at ${p.thenWords ?? 'its next moment'}${p.anyway ? ' (anyway)' : ''}`;
            return p.anyway ? `Advance${to} now anyway${p.notReady ? ` (not ready: ${p.notReady})` : ''}` : `Advance${to} now (ready)`; }
        // (Doc 86, C10: the Foes panel's Send away.)
        case 'foe': return `sent ${nameOf(names, String(p.foe))} away`;
        default: return PRETTY[kind] ?? kind;
    }
}
/** The values a command changed, before and after, in words (the Audit tab). */
export function beforeAfter(c: Pick<Command, 'result'>): string {
    const r = c.result;
    if (!r?.before || !r.after) return '';
    const keys = [...new Set([...Object.keys(r.before), ...Object.keys(r.after)])].filter(k => ['status', 'phase', 'value', 'at', 'on', 'step', 'visit', 'version'].includes(k));
    const v = (x: unknown, k: string) => (x === null || x === undefined ? '—' : typeof x === 'boolean' ? (x ? 'on' : 'off')
        : typeof x === 'number' && !Number.isInteger(x) ? x.toFixed(1) : k === 'version' ? String(x).slice(0, 8) : String(x));
    return keys.filter(k => r.before![k] !== r.after![k]).map(k => `${k} ${v(r.before![k], k)} → ${v(r.after![k], k)}`).join(' · ');
}
/** Ask the reason again for these, and the story's title typed (5.8, rule 2). */
export const typedTitle = (kind: CommandKind, payload: Record<string, unknown>, way?: 'back' | 'on' | 'again') =>
    kind === 'stop' || (kind === 'order' && payload.do === 'skip') || (kind === 'enter' && way === 'back') || (kind === 'advance' && payload.anyway === true);
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
    key?: string; scope?: string; field?: string; op?: string; value?: unknown; expected?: unknown; wolf?: string; why?: string;
    needs?: 'event' | 'wolf' | 'place' | 'facts' | 'roll';
    region?: string; id?: string;
    /** Doc 85, Phase 8: what the world's facts (or the story's quests) said, for a world or quest condition. */
    fact?: unknown;
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
    /** Doc 85, Phase 8: what the story asks the game to tell it, and the world's facts as the game last wrote them (its wolves). */
    watch?: StoryWatch; facts?: WorldFactsDoc | null;
}
export interface StoryWatch {facts?: string[]; items?: string[]; factions?: string[]; towns?: string[]; residents?: string[]; cells?: string[];
    events?: string[]; relations?: [string, string][]; weatherWhereWolves?: boolean}
export interface WorldFactsDoc {wolves?: Record<string, Record<string, unknown>>; residents?: Record<string, Record<string, unknown>>;
    world?: Record<string, unknown>; weather?: Record<string, string>; relations?: Record<string, string>}
/** The world's facts in lines for the console: "Ash Greymantle · Ridgemere, Northgate (31, 36) · out of doors · carries rope 3 · 40p". */
export function factLines(f: WorldFactsDoc | null | undefined, names: Record<string, string> = {}): string[] {
    if (!f) return [];
    const out: string[] = [];
    const w = f.world;
    if (w) out.push(['the world', w.weekday, w.season, w.hour !== undefined ? `${w.hour}:${String(w.minute ?? 0).padStart(2, '0')}` : '', w.period]
        .filter(Boolean).join(' · '));
    for (const [id, x] of Object.entries(f.wolves ?? {})) {
        const carries = Object.entries((x.carries ?? {}) as Record<string, number>).map(([k, n]) => `${nameOf(names, k)} ${n}`).join(', ');
        const standing = Object.entries((x.standing ?? {}) as Record<string, number>).map(([k, n]) => `${nameOf(names, k)} ${n}`).join(', ');
        const states = ['downed', 'hurt', 'wanted', 'held'].filter(k => x[k] === true);
        out.push([nameOf(names, id), x.cell ? `${nameOf(names, String(x.cell))} (${x.x}, ${x.y})` : '', x.indoors ? 'indoors' : 'out of doors',
            Number(x.partySize ?? 1) > 1 ? `in a party of ${x.partySize}` : '', carries && `carries ${carries}`, x.coins !== undefined ? `${x.coins}p` : '',
            standing && `standing ${standing}`, ...states].filter(Boolean).join(' · '));
    }
    for (const [id, r] of Object.entries(f.residents ?? {}))
        out.push([nameOf(names, id), r.alive === false ? 'dead' : 'alive', r.cell ? `${nameOf(names, String(r.cell))} (${r.x}, ${r.y})` : '',
            r.job ? String(r.job) : '', r.mood ? `mood ${r.mood}` : '', r.mantle ? `mantle ${r.mantle}` : ''].filter(Boolean).join(' · '));
    const weather = Object.entries(f.weather ?? {}).map(([c, v]) => `${nameOf(names, c)} ${v}`).join(', ');
    if (weather) out.push('weather · ' + weather);
    const relations = Object.entries(f.relations ?? {}).map(([k, v]) => `${k.split('>').map(x => nameOf(names, x)).join(' → ')} ${v}`).join(', ');
    if (relations) out.push('relations · ' + relations);
    return out;
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
    /** A move onto a new version (doc 85, Phase 5b): the migration report as the dry run worked it out. */
    migration?: MigrationReport;
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
    if ('fact' in c || c.needs === 'facts') {
        // (Doc 85, Phase 8: a condition over the world's facts as of the event, or the story's quests: what they said.)
        const head = `${wording?.condition ? wording.condition(c) : c.test} ${mark(c.verdict)}`;
        const whose = c.wolf ? ` for ${nameOf(names, c.wolf)}` : '';
        if (c.needs === 'wolf') return `${head} (each wolf's own)`;
        if (c.needs === 'facts') return `${head} (the world's facts do not say${whose}: away, or not yet written)`;
        return `${head} (the world says ${factWords(c.fact, names)}${whose})`;
    }
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
/** A fact the world's facts gave: "3", "yes", "Ridgemere, Northgate (31, 36)", "snow". */
export function factWords(v: unknown, names: Record<string, string> = {}): string {
    if (v === null || v === undefined || v === '') return 'nothing';
    if (typeof v === 'boolean') return v ? 'yes' : 'no';
    if (typeof v === 'object') {
        const o = v as {cell?: string; x?: number; y?: number};
        return o.cell ? `${nameOf(names, o.cell)} (${o.x}, ${o.y})` : 'nowhere';
    }
    return typeof v === 'string' ? nameOf(names, v) : String(v);
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
export function stepText(s: Step, names: Record<string, string> = {}, wording?: Wording): string {
    switch (s.verdict) {
        case 'fired': return 'it fired';
        case 'filter': return s.field === 'kind' ? `another kind of event (${s.actual})` : `its ${s.field} was ${shown(s.actual, names)}; it wants ${shown(s.expected, names)}`;
        case 'outsider': return `${s.wolf ? nameOf(names, s.wolf) : 'nobody'} is not in the story`;
        case 'off': return 'it is turned off';
        case 'used': return `already fired${s.wolf ? ' for ' + nameOf(names, s.wolf) : ''} (once per ${s.once})`;
        case 'conditions': {
            // (Doc 85, Phase 8: a world condition is judged with the world's facts as of the event itself.)
            const facts = JSON.stringify(s.conditions ?? {}).includes('"fact"');
            return `its conditions said no: ${condLines(s.conditions, names, 0, wording).filter(l => l.verdict !== true).slice(0, facts ? 4 : 3).map(l => l.text).join('; ')}`
                + (facts ? ' (the world as it was at this event; the story\'s own values as they are now)' : " (today's values)");
        }
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
    if (p.migration) {
        for (const g of migrationGroups(p.migration, names)) if (g.group !== 'kept') out.push(`${g.title}: ${g.lines.map(l => l.replace(/\.$/, '')).join('; ')}`);
        out.push(...p.migration.warnings);
    }
    return out;
}

// ------------------------------------------------------------------------------------------------- a new version
const short = (digest: string | null | undefined) => (digest ?? '').slice(0, 8);
export {short as shortDigest};
/** One line of a migration report: "stores (the story's) = 3", "hauled (Ash) = 2 — renamed from hauled", "timer watch-round, due day 56.0". */
export function migrationLine(l: MigrationLine, names: Record<string, string> = {}): string {
    const whose = l.wolf ? ` (${nameOf(names, l.wolf)})` : l.party ? ` (party of ${nameOf(names, l.party)})` : l.scope === 'story' ? '' : l.scope ? ` (${l.scope})` : '';
    const value = l.value === undefined ? '' : ` = ${valueText(l.value, typeof l.value === 'number' ? 'number' : undefined, names)}`;
    const head = l.what === 'variable' ? `${l.key}${whose}${value}`
        : l.what === 'timer' ? `timer ${l.key.split('/')[0]}${l.key.includes('/') ? ` (${nameOf(names, l.key.split('/')[1])})` : ''}${l.at !== undefined ? `, due day ${l.at.toFixed(1)}` : ''}`
        : l.what === 'wait' ? `the wait in ${l.key}${l.wolf ? ` for ${nameOf(names, l.wolf)}` : ''}${l.at !== undefined ? ` (resumes day ${l.at.toFixed(1)})` : ''}`
        : l.what === 'mission' ? `mission ${l.key}`
        : l.what === 'fired' ? `${l.key} already fired${l.wolves?.length ? ` for ${l.wolves.map(w => nameOf(names, w)).join(', ')}` : ''}`
        : l.what === 'count' ? `how often ${l.key} fired`
        : l.what === 'off' ? `${l.key} turned off`
        : l.what === 'vote' ? `the votes for ${l.key}`
        : l.what === 'steps' ? `the steps taken towards ${l.key}`
        : l.what === 'phase' ? `phase ${l.key}`
        : l.what === 'seed' ? 'its rolls' : l.key;
    return l.why ? `${head}: ${l.why}` : head;
}
export type MigrationGroup = 'kept' | 'reset' | 'defaulted' | 'dropped' | 'added';
const GROUP_TITLES: Record<MigrationGroup, string> = {
    kept: 'Kept', reset: 'Reset (starts again)', defaulted: 'Back to its first value', dropped: 'Dropped (the new version has no place for it)', added: 'New',
};
/** A migration report as groups of lines, the most telling first: what goes, what starts again, what is new, what stays. */
export function migrationGroups(r: MigrationReport, names: Record<string, string> = {}): {group: MigrationGroup; title: string; lines: string[]}[] {
    return (['dropped', 'reset', 'defaulted', 'added', 'kept'] as MigrationGroup[])
        .filter(g => r[g]?.length).map(g => ({group: g, title: GROUP_TITLES[g], lines: r[g].map(l => migrationLine(l, names))}));
}
/** What changed between the two versions, a line a part: "Triggers: 1 added (early-thaw); 2 changed (open-winter-ledger: actions; offer-a-coil: words only)". */
export function diffLines(d: VersionDiff | undefined): string[] {
    if (!d) return [];
    if (d.same && !d.variables.renamed?.length) return ['Nothing the story does has changed (its rehearsals may have).'];
    const out: string[] = [];
    const part = (title: string, c: Changes) => {
        const bits = [c.added.length && `${c.added.length} added (${c.added.join(', ')})`,
            c.changed.length && `${c.changed.length} changed (${c.changed.map(x => `${x.id}: ${x.wordsOnly ? 'words only' : x.what.join(', ')}`).join('; ')})`,
            c.removed.length && `${c.removed.length} removed (${c.removed.join(', ')})`].filter(Boolean);
        if (bits.length) out.push(`${title}: ${bits.join('; ')}`);
    };
    part('Triggers', d.triggers);
    const v = d.variables, vb = [v.added.length && `${v.added.length} added (${v.added.map(x => `${x.id}, a ${x.type}`).join('; ')})`,
        v.retyped.length && `${v.retyped.length} retyped (${v.retyped.map(x => `${x.id}: ${x.from} → ${x.to}`).join('; ')})`,
        v.rescoped.length && `${v.rescoped.length} moved (${v.rescoped.map(x => `${x.id}: ${x.from}'s → ${x.to}'s`).join('; ')})`,
        v.changed.length && `${v.changed.length} changed (${v.changed.map(x => `${x.id}: ${x.what.join(', ')}`).join('; ')})`,
        v.removed.length && `${v.removed.length} removed (${v.removed.map(x => x.id).join(', ')})`,
        v.renamed?.length && `${v.renamed.length} renamed (${v.renamed.map(x => `${x.scope}'s ${x.id} → ${x.to}`).join('; ')})`].filter(Boolean);
    if (vb.length) out.push(`Variables: ${vb.join('; ')}`);
    part('Phases', d.phases);
    const m = d.missions, mb = [m.added.length && `${m.added.length} added (${m.added.map(x => `${x.phase}/${x.id}`).join(', ')})`,
        m.changed.length && `${m.changed.length} changed (${m.changed.map(x => `${x.phase}/${x.id}${x.objectives ? ': its objectives' : ''}`).join(', ')})`,
        m.removed.length && `${m.removed.length} removed (${m.removed.map(x => `${x.phase}/${x.id}`).join(', ')})`].filter(Boolean);
    if (mb.length) out.push(`Missions: ${mb.join('; ')}`);
    const t = d.timers, tb = [t.added.length && `${t.added.join(', ')} new`, t.changed.length && `${t.changed.join(', ')} started differently`,
        t.removed.length && `${t.removed.join(', ')} never started now`].filter(Boolean);
    if (tb.length) out.push(`Timers: ${tb.join('; ')}`);
    part('Regions', d.regions);
    part('Scenes', d.scenes);
    if (d.header.length) out.push(`Also changed: ${d.header.join(', ')}`);
    return out;
}
/** A move on the timeline: "moved to version b7e1c2d4 by martin (from a41c0f9e): 12 kept, 1 reset, 2 dropped". */
export function versionLine(i: Extract<TimelineItem, {type: 'version'}>): string {
    const c = i.counts;
    const counts = [`${c.kept} kept`, c.reset && `${c.reset} reset`, c.defaulted && `${c.defaulted} back to their first value`, c.dropped && `${c.dropped} dropped`]
        .filter(Boolean).join(', ');
    const landed = i.phase && i.phaseFrom && i.phase !== i.phaseFrom ? `, landing in ${i.phase} (${i.phaseFrom} is gone)` : '';
    const who = (i.movedBy ?? '').split('#')[0];
    if (i.status === 'refused') return `the game refused version ${short(i.digest)}: back on ${short(i.from)} (moved by ${who})`;
    if (i.status === 'moving') return `moving to version ${short(i.digest)} by ${who} (from ${short(i.from)})${landed}: waiting for the game`;
    return `moved to version ${short(i.digest)} by ${who} (from ${short(i.from)})${landed}: ${counts}`;
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
    const one: Record<string, string> = {trigger: 'trigger', mission: 'mission', exit: 'exit', effect: 'entry effect', region: 'region', scene: 'scene', case: 'rehearsal case',
        quest: 'quest'};
    if (one[a.kind]) parts.push(`${one[a.kind]} ${a.id ?? '#' + ((a.index ?? 0) + 1)}`);
    else if (a.kind === 'settings') parts.push('Campaign settings');
    else if (a.kind === 'program') parts.push('The program');
    else if (a.kind === 'tests') parts.push('Rehearsal cases');
    else if (!parts.length) parts.push(a.kind);
    const part = a.part ?? '';
    const n = /\[(\d+)\]/.exec(part);
    if (part === 'if') parts.push('conditions');
    else if (part.startsWith('actions')) {
        // (Version 2: a nested action, "actions[3].elif[0].then[1]": action 4 › else if 1 › 2.)
        parts.push(`action ${Number(n?.[1] ?? 0) + 1}`);
        const steps = [...part.slice(n![0].length + 'actions'.length).matchAll(/\.(\w+)\[(\d+)\]/g)].map(m => [m[1], Number(m[2]) + 1] as const);
        for (let k = 0; k < steps.length; k++) {
            const [key, i] = steps[k];
            if (key === 'elif' || key === 'choices') { parts.push(`${key === 'elif' ? 'else if' : 'choice'} ${i}`, String(steps[k + 1]?.[1] ?? '')); k++; }
            else parts.push(`${key === 'actions' ? 'for each' : key} ${i}`);
        }
    }
    else if (part.startsWith('objectives')) parts.push(`objective ${Number(n?.[1] ?? 0) + 1}`);
    else if (part.startsWith('rewards') || part.startsWith('bonus') || part.startsWith('fail')) parts.push(`${part.startsWith('fail') ? 'fail rule' : part.startsWith('bonus') ? 'bonus' : 'reward'} ${Number(n?.[1] ?? 0) + 1}`);
    else if (part === 'audience') parts.push('the audience');
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

// ------------------------------------------------------------------------------------------------- quests (Phase 6)

export const QUEST_STATUS: Record<string, string> = {offered: 'OFFERED', active: '', ready: 'READY', done: 'DONE', failed: 'FAILED', abandoned: 'ABANDONED', expired: 'ENDED'};
export type QuestCell = {state: 'done' | 'partial' | 'open' | 'later' | 'failed' | 'hidden'; text: string};
export interface QuestRow { holder: string; kind: QuestCopy['kind']; name: string; status: string; word: string; cells: QuestCell[]; members: string[]; left: string[]; player: string }
/** A quest as the console's grid (doc 85, 5.3 "Quests"): a row per copy (each wolf's, each party's, the story's), a column per
 *  objective: ✔ done, ✖ failed, 3 / 4, ? hidden, · not open yet (its "after" not done), - nothing yet. */
export function questGrid(q: QuestBoard, names: Record<string, string>): QuestRow[] {
    const rank: Record<string, number> = {ready: 0, active: 1, offered: 2, done: 3, failed: 4, abandoned: 5, expired: 6};
    return q.copies.map(c => {
        const done = (id: string) => !!c.objectives[id]?.done;
        const cells = q.objectives.map((o): QuestCell => {
            const r = c.objectives[o.id] ?? {};
            const goal = o.goal ?? 1;
            if (r.failed) return {state: 'failed', text: '✖'};
            if (r.done) return {state: 'done', text: '✔'};
            if (o.hidden && !r.shown) return {state: 'hidden', text: '?'};
            if (o.after?.some(a => !done(a))) return {state: 'later', text: '·'};
            if (r.n) return {state: 'partial', text: `${r.n} / ${goal}`};
            return {state: 'open', text: goal > 1 ? `0 / ${goal}` : '-'};
        });
        const name = c.kind === 'story' ? 'Everyone (one copy)' : c.kind === 'party'
            ? `Party of ${c.members.map(m => nameOf(names, m)).join(', ') || nameOf(names, c.holder.replace(/^party:/, ''))}` : nameOf(names, c.holder);
        return {holder: c.holder, kind: c.kind, name, status: c.status, word: QUEST_STATUS[c.status] ?? c.status.toUpperCase(), cells, members: c.members,
            left: c.left ?? [], player: c.kind === 'wolf' ? c.holder : c.kind === 'story' ? 'story' : c.members[0] ?? c.holder.replace(/^party:/, '')};
    }).sort((a, b) => (rank[a.status] ?? 9) - (rank[b.status] ?? 9) || a.name.localeCompare(b.name));
}
/** "9 of 31 done · 3 ready to hand in · 2 failed": a quest's counts in a line. */
export function questSummary(q: QuestBoard): string {
    const counts: Record<string, number> = {};
    for (const c of q.copies) counts[c.status] = (counts[c.status] ?? 0) + 1;
    for (const [k, v] of Object.entries(q.counts ?? {})) counts[k] = v ?? 0;
    const total = Object.values(counts).reduce((a, b) => a + b, 0);
    const out = [`${counts.done ?? 0} of ${total} done`];
    if (counts.ready) out.push(`${counts.ready} ready to hand in`);
    if (counts.active) out.push(`${counts.active} under way`);
    if (counts.offered) out.push(`${counts.offered} offered`);
    for (const k of ['failed', 'abandoned', 'expired']) if (counts[k]) out.push(`${counts[k]} ${k === 'expired' ? 'ended with their phase' : k}`);
    if (q.cap) out.push(`at most ${q.cap} under way at once`);
    if (q.limit) out.push(`${q.starts ?? 0} of ${q.limit} taken`);
    return out.join(' · ');
}
/** The wolves of the story who hold no copy of a quest (whom "Give the quest" may name). */
export function withoutQuest(q: QuestBoard, cohort: string[]): string[] {
    const holding = new Set(q.copies.filter(c => !['done', 'failed', 'abandoned', 'expired'].includes(c.status) || q.repeat === undefined || q.repeat === 'once')
        .flatMap(c => c.kind === 'wolf' ? [c.holder] : c.members));
    return cohort.filter(w => !holding.has(w));
}
/** The audience in words: "open · 12 of 5000", "everyone · 40", "3 named wolves". */
export function audienceText(a: Audience | undefined, wolves?: number): string {
    if (!a) return wolves !== undefined ? String(wolves) : '-';
    const size = a.size ?? wolves ?? 0;
    const word = a.kind === 'open' ? 'open' : a.kind === 'everyone' ? 'everyone' : a.kind === 'active' ? 'active lately' : 'named';
    return `${word} · ${size}${a.cap ? ` of ${a.cap}` : ''}${a.joinable === false ? ' (full)' : ''}`;
}

/** A console command waiting for the admin's second click (Steer.tsx's Ask, by shape). */
export interface QuestAsk { kind: CommandKind; payload: Record<string, unknown>; title: string; does: string[]; warn?: string }
/** The controls of one row, as the asks they send (also what campaigns.test.ts checks). */
export function rowAsks(q: QuestBoard, r: QuestRow): {label: string; ask: QuestAsk}[] {
    const who = r.kind === 'story' ? ' (the story\'s copy)' : ' for ' + r.name;
    const out: {label: string; ask: QuestAsk}[] = [];
    const live = ['active', 'ready', 'offered'].includes(r.status);
    const rewards = 'Its rewards are given as play would give them, to each wolf with credit.';
    for (const [i, o] of q.objectives.entries()) {
        const cell = r.cells[i];
        const obj = (d: string, label: string, does: string[], extra: Record<string, unknown> = {}, warn?: string) => out.push({label,
            ask: {kind: 'objective', payload: {quest: q.id, objective: o.id, player: r.player, do: d, ...extra}, title: `${label}${who}`, does, ...(warn ? {warn} : {})}});
        if (!live) continue;
        if (cell.state !== 'done' && cell.state !== 'failed') obj('complete', `Complete “${o.label}”`, ['The objective is marked done, and the journal told.',
            ...(!o.optional ? ['If it was the last one needed, the quest is ready to hand in (or done, with its rewards).'] : []), rewards]);
        if ((o.goal ?? 1) > 1 && cell.state !== 'done' && cell.state !== 'failed') obj('progress', `Set progress on “${o.label}”`, ['It counts as one more towards its goal.'], {amount: 1});
        if (o.hidden && cell.state === 'hidden') obj('reveal', `Reveal “${o.label}”`, ['The hidden objective appears in the journal.']);
        if (cell.state !== 'failed' && cell.state !== 'done') obj('fail', `Fail “${o.label}”`, [o.optional ? 'The optional objective is struck through.' : 'A required objective failed fails the quest; the journal says so.']);
        if (cell.state === 'done' || cell.state === 'partial' || cell.state === 'failed') obj('reset', `Reset “${o.label}”`, ['Its progress starts again, and the journal is told.'], {},
            'What it already gave stays given.');
    }
    const quest = (d: string, label: string, does: string[], warn?: string) => out.push({label,
        ask: {kind: 'quest', payload: {quest: q.id, player: r.player, do: d}, title: `${label} “${q.title}”${who}`, does, ...(warn ? {warn} : {})}});
    if (live) {
        quest('complete', 'Complete', ['Every required objective is marked done.', rewards]);
        quest('fail', 'Fail', ['The quest fails; the journal says so, and "quest failed" is sent to the story\'s triggers.']);
    }
    quest('take', 'Take it away', ['The copy is removed: it leaves the journal.'], 'What it already gave stays given.');
    return out;
}

export function giveAsk(q: QuestBoard, wolf: string, names: Record<string, string>): QuestAsk {
    return {kind: 'quest', payload: {quest: q.id, player: wolf, do: 'give'}, title: `Give “${q.title}” to ${nameOf(names, wolf)}`,
        does: [q.scope === 'party' ? 'Starts it for the wolf\'s party (its members as they are now).' : q.scope === 'story' ? 'Starts the story\'s one copy, if it has none.'
            : 'Starts the wolf\'s own copy, as accepting it would.', 'The journal is told; "quest started" is sent to the story\'s triggers.']};
}


// ---------------------------------------------------------------------------------- chapters and campaigns (doc 85, Phase 10)
export type ChapterState = 'done' | 'running' | 'held' | 'paused' | 'stopped' | 'waiting' | 'ready';
/** A campaign as its chapters, in order: each one's state (its newest run's), the run to open, and its phase. */
export function chapterStrip(c: Campaign): {story: string; title: string; state: ChapterState; run: string | null; phase: string | null; runs: number}[] {
    return c.chapters.map((ch, n) => {
        const started = ch.runs.filter(r => r.status !== 'approved');
        const last = started[started.length - 1] ?? null;
        const state: ChapterState = !last ? (n === 0 && ch.runs.length ? 'ready' : 'waiting')
            : last.status === 'complete' ? 'done' : last.status === 'stopped' ? 'stopped'
            : last.status === 'held' ? 'held' : last.status === 'paused' ? 'paused' : 'running';
        return {story: ch.story, title: ch.title, state, run: last?.run ?? null, phase: last?.phase ?? null, runs: started.length};
    });
}
const CHAPTER_MARK: Record<ChapterState, string> = {done: '✔', running: '●', held: '⏸', paused: '⚠', stopped: '⏹', waiting: '○', ready: '◌'};
export const chapterMark = (s: ChapterState) => CHAPTER_MARK[s];
/** Words for a campaign: how far it has got. */
export function campaignText(c: Campaign): string {
    const strip = chapterStrip(c);
    const done = strip.filter(s => s.state === 'done').length;
    const now = strip.find(s => ['running', 'held', 'paused'].includes(s.state));
    return `${strip.length} chapter${strip.length === 1 ? '' : 's'} · ${done} done` + (now ? ` · now: ${now.title}${now.phase ? ` (${now.phase})` : ''}`
        : done === strip.length ? ' · ended' : strip[0]?.state === 'ready' ? ' · the first is approved, to publish' : '');
}
/** A chapter waiting to be started by this run, in words. */
export function chapterStartText(s: ChapterStart, c: Campaign | null | undefined, names: Record<string, string> = {}): string {
    const title = c?.chapters.find(ch => ch.story === s.story)?.title ?? s.story;
    const who = s.audience === 'same' ? "this story's wolves" : s.audience === 'own' ? 'its own audience'
        : (s.wolves ?? []).map(w => nameOf(names, w)).join(', ') || 'nobody (the list is empty)';
    return `"${title}" for ${who}` + (s.carry === false ? '' : ', carrying its variables') + (s.when === 'now' ? ', now' : ', when this story has ended');
}
/** A world variable's value in words (by its type). */
export function worldVarText(v: WorldVar, names: Record<string, string> = {}): string {
    if (v.unset || v.value === null || v.value === undefined) return '(not set yet)';
    return valueText(v.value as Value, v.type, names);
}
/** Whether a typed text is a value of a world variable's type: the value, or a reason it is not. */
export function parseWorldValue(type: string, text: string): {value: Value} | {problem: string} {
    const t = text.trim();
    if (type === 'number') { const n = Number(t); return Number.isInteger(n) && t !== '' ? {value: n} : {problem: 'A whole number.'}; }
    if (type === 'switch') return /^(on|true|yes)$/i.test(t) ? {value: true} : /^(off|false|no)$/i.test(t) ? {value: false} : {problem: 'On or off.'};
    if (['numbers', 'texts', 'wolves', 'residents'].includes(type)) {
        const parts = t ? t.split(',').map(x => x.trim()).filter(Boolean) : [];
        if (type === 'numbers') { const ns = parts.map(Number); return ns.every(Number.isInteger) ? {value: ns as unknown as Value} : {problem: 'Whole numbers, with commas.'}; }
        return {value: parts as unknown as Value};
    }
    return t.length <= 200 ? {value: t} : {problem: 'At most 200 characters.'};
}

/** Doc 86, C9: what "Undo this figure" does, in words. */
export function figureUndoText(f: {id: string; previous: Record<string, unknown> | null}): string {
    return f.previous ? `Put the figure ${f.id} back as it was (${String(f.previous.name ?? f.id)}, ${String(f.previous.role ?? '')})`
        : `Take the figure ${f.id} away (there was none before)`;
}

/** Doc 86, C8: one thing residents remember of a story: whose memory (realm, town:<id>, person:<npc>, pair:<a>|<b>). */
export interface Memory {owner: string; key: string; value: string; keep: boolean; daysLeft: number | null}
/** Who remembers, in words: "the realm", "the town of Coldwater", "Ivo Tarmarsh", "Ivo Tarmarsh, about Ash". */
export function memoryOwnerText(owner: string, names: Record<string, string>): string {
    if (owner === 'realm') return 'the realm';
    if (owner.startsWith('town:')) return 'the town of ' + nameOf(names, owner.slice(5));
    if (owner.startsWith('person:')) return nameOf(names, owner.slice(7));
    if (owner.startsWith('pair:')) {
        const [a, b] = owner.slice(5).split('|');
        const wolfFirst = a.startsWith('wolf-') || a.startsWith('player-');
        return `${nameOf(names, wolfFirst ? b : a)}, about ${nameOf(names, wolfFirst ? a : b)}`;
    }
    return owner;
}
