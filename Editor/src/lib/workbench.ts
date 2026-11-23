// The Story Creator's trigger workbench, without React (Docs/Design/85-story-triggers-and-campaign-control.md, section 4
// and Phase 4): sentences from the trigger catalogue (Data/Storylines/trigger-catalogue.json, served by `catalog`), the
// tree of a program, its edits (move, duplicate, copy and paste as a `ratw-triggers` fragment, rename a trigger, a
// signal or a counter, every use of a name), search, undo and redo, and where a problem of the Problems list is. Every
// edit returns a new program and leaves alone what it did not touch, so the workbench never changes a program it did
// not edit (the round trip test of Phase 4, workbench.test.ts and the render smoke).

import type {Scene} from '../dm/scenes';

export type Json = Record<string, unknown>;
export interface Param {
    key: string; type: string; label: string; min?: number; max?: number; values?: string[]; words?: Record<string, string>;
    default?: unknown; nonzero?: boolean; part?: string; or?: string[]; world?: boolean;
    /** Phase 9: a market slot that may be every market ("all"). */
    all?: boolean;
    /** Version 2: a variable's types (`variable`), an optional slot, an operand's kind or the slot it must match. */
    of?: string[]; optional?: boolean; kind?: string; like?: string;
    /** Doc 86, C3a: a `variable` slot's scopes (Rank: a wolf's own number, into a list of the story or the world). */
    scope?: string[];
    /** (The Editor's own: an `objective` slot's quest, filled from the record beside it, so its picker lists that quest's.) */
    quest?: string;
    /** Doc 86, Phase 2: an effect's parameter that may be a value worked out as it fires (C1a), words that may hold fills (C1b). */
    operand?: boolean; fills?: boolean;
    /** (The Editor's own: "…or a value" for an id parameter: its type (wolf, resident, place, item, faction, town).) */
    idKind?: string;
}
export interface Field {type: string; label: string; any?: string}
export interface CatEvent {
    id: string; group: string; title: string; sentence: string; help: string; fields: Record<string, Field>; tile: boolean;
    mission?: boolean; measures?: string[]; special?: string; exitOnly?: boolean; notes?: string;
    /** Phase 6: the story's own fact (sent as it runs, never in the world's log). */
    story?: boolean;
}
export interface CatItem {id: string; title: string; sentence: string; help: string; group?: string | boolean; params?: Param[];
    phaseOnly?: boolean; entryOnly?: boolean; last?: boolean; actsOnWolves?: boolean; needsBuilder?: boolean;
    /** Version 2 (doc 85, Phase 5): only in a story of version 2 (`v2`) or 1 (`v1`); the action's `do` when it is not the id;
     *  the lists of actions it holds (`nest`), and whether it holds a condition. */
    v2?: boolean; v1?: boolean; do?: string; nest?: string[]; condition?: boolean;
    /** Phase 6: kept by the story itself (quest progress, who takes part, a story choice), never an order to the game. */
    story?: boolean;
    /** Phase 8: a condition over the world's facts as of the event (game.story_facts), and which of them it reads. */
    facts?: boolean; reads?: string}
export interface Catalogue {
    groups: {id: string; label: string}[]; actionGroups: {id: string; label: string}[]; types: Record<string, string>;
    trigger: {once: Record<string, string>; audience: Record<string, string>; limits: Record<string, number>; enabled?: Record<string, string>};
    events: CatEvent[]; conditions: CatItem[]; actions: CatItem[]; effects: CatItem[];
    variables?: {types: Record<string, string>; lists: Record<string, string>; scopes: Record<string, string>; help: string};
}

// --------------------------------------------------------------------------------------------------- the program's shape

/** A value of version 2 (doc 85, 3.4): a constant, a variable, the event's field, a list's count, a timer's days, the loop's
 *  value, or a little arithmetic. */
export type Operand = number | boolean | string | {var: string; of?: string} | {field: string} | {count: string; of?: string}
    | {since: string; of?: string} | {each: true} | {plus: [Operand, Operand]} | {minus: [Operand, Operand]} | {min: [Operand, Operand]}
    | {max: [Operand, Operand]}
    // (Doc 86, Phase 1a: the story's audience counted; times and div; the share, or the number, of the wolves who took part.)
    | {audience: 'all' | 'active' | 'online'} | {times: [Operand, Operand]} | {div: [Operand, Operand]}
    | {percent: ShareSource; of?: string} | {among: ShareSource; of?: string}
    // (Doc 86, Phase 2, M2: what a purse holds now.)
    | {purse: string}
    // (Doc 86, Phase 3: a tally over every wolf (J11); a credited wolf's share, in a quest's rewards (C3b).)
    | {tally: Tally} | {share: true};
/** Doc 86, J11: a number each wolf keeps added up (sum), or wolves counted, over the story's wolves or a list, those whose
 *  text or switch (where) is a value (is). */
export interface Tally {sum?: string; where?: string; is?: string | boolean; over?: 'participants' | {list: string}}
/** A filter's value (doc 86, C12b): a text, "begins with", or "one of". */
export type MatchValue = string | {starts: string} | {in: string[]};
/** Who counts (doc 86, R1): the wolves of a story or world list, or those for whom a test is true (the wolf as $each). */
export type ShareSource = {list: string} | {where: Condition};
export type Condition =
    | {test: 'all' | 'any'; conditions: Condition[]}
    | {test: 'not'; condition: Condition}
    | {test: 'mission'; id: string}
    | {test: 'region'; region: string}
    | {test: 'field'; field: string; op: string; value: string | number | string[]}
    | {test: 'counter'; scope: string; key: string; op: string; value: number}
    // (Version 2.)
    | {test: 'compare'; left: Operand; op: string; right: Operand}
    | {test: 'switch'; var: string; is: boolean; of?: string}
    | {test: 'in list'; list: string; value: Operand; of?: string}
    | {test: 'phase is'; phase: string}
    | {test: 'visit is'; op: string; value: number}
    | {test: 'timer'; signal: string; owner?: string; op?: string; days?: number}
    | {test: 'fired'; trigger: string; op: string; value: number; in?: string}
    | {test: 'chance'; percent: number}
    // (Doc 86, R1: enough of the active wolves took part.)
    | {test: 'participation'; source?: 'list' | 'where'; list?: string; where?: Condition; of?: string; percent: number; floor?: number; cap?: number}
    | {test: 'days since'; from: string; op: string; days: number; var?: string; of?: string};
export type Effect = Record<string, string | number | boolean> & {kind: string};   // (Phase 9: a tracker's show is a switch.)
export interface Branch2 {if: Condition; then: Action[]}
export type Action =
    | {do: 'emit'; signal: string}
    | {do: 'schedule'; signal: string; days: number; repeat?: boolean; scope?: string; clock?: string; owner?: string}
    | {do: 'cancel'; signal: string; owner?: string}
    | {do: 'enter'; phase: string}
    | {do: 'effect'; effect: Effect}
    | {do: 'add' | 'set'; scope: string; key: string; value: string | number; var?: undefined}
    // (Version 2: state.)
    | {do: 'add' | 'set' | 'subtract' | 'multiply'; var: string; value: Operand; of?: string; key?: undefined; scope?: undefined}
    | {do: 'clamp'; var: string; min: number; max: number; of?: string}
    | {do: 'turn on' | 'turn off' | 'toggle' | 'list clear' | 'start timer' | 'stop timer'; var: string; of?: string}
    | {do: 'remember'; var: string; from: string; of?: string}
    | {do: 'list add' | 'list remove'; var: string; value: Operand; of?: string}
    | {do: 'pick'; list: string; var: string; of?: string}
    // (Doc 86, C3a: the top wolves by a number each keeps, into a list of wolves.)
    | {do: 'rank'; var: string; top: Operand; into: string; over?: string; list?: string; min?: Operand}
    | {do: 'shift'; signal: string; days: number; owner?: string}
    // (Version 2: flow.)
    | {do: 'if'; if: Condition; then: Action[]; elif?: Branch2[]; else?: Action[]}
    | {do: 'wait'; days: number; clock?: string}
    | {do: 'each'; over: string; list?: string; actions: Action[]}
    | {do: 'random'; choices: {weight: number; actions: Action[]}[]}
    | {do: 'run'; trigger: string; check?: boolean}
    | {do: 'enable' | 'disable'; trigger: string; phase?: string}
    | {do: 'stop'}
    | {do: 'note'; text: string}
    | {do: 'finish'; outcome: string; text: string; who?: string};
export interface Trigger {
    id: string; on: string; signal?: string; match?: Record<string, string>; if?: Condition; audience?: string; once?: string;
    afterDays?: number; actions: Action[]; folder?: string; note?: string;
    /** Version 2: false starts it turned off, until an Enable action or an admin turns it on. */
    enabled?: boolean;
}
/** A declared variable of a version 2 story (doc 85, 3.2). */
export interface Variable {id: string; type: string; scope: string; initial?: unknown; note?: string; show?: string; clock?: string;
    /** Doc 86, N2: a wolf's own value told to the residents they talk to (player.story.<name> in the dialogue window). */
    talk?: boolean;
    /** Doc 86, C1d: shown "of" another number ("This chapter: 34 / 80"); N3: a wolf's number that mirrors the world number
     *  their text `key` names. */
    showOf?: string; mirror?: {key: string; names: Record<string, string>}}
export interface Exit {
    on: string; to: string; match?: Record<string, string>; afterDays?: number; percent?: number; players?: number; participant?: boolean;
    missions?: string[]; minCoins?: number; minQuantity?: number; prerequisites?: unknown[];
}
export interface Objective {label: string; on: string; signal?: string; sameAsFirst?: string; match?: Record<string, string>;
    measure?: 'count' | 'quantity' | 'coins'; goal?: number}
export interface Mission {id: string; title: string; mode: 'individual' | 'group' | 'either'; objectives: Objective[];
    contributors?: number; players?: number; percent?: number}
export interface Phase {id: string; journal: string; effects: Effect[]; exits: Exit[]; triggers?: Trigger[]; missions?: Mission[];
    guestVoices?: string[]; campaignRules?: boolean; ready?: ReadyBlock}
/** A phase's Readiness (doc 86, R2): what it needs before it may move on by its ready exit. */
export type ReadyPartSpec = {label?: string} & ({participation: ShareSource; of?: string; percent: number; floor?: number; cap?: number}
    | {minDays: number; since?: 'phase' | 'chapter'} | {value: Operand; atLeast: Operand} | {test: Condition} | {missions: string[]} | {quest: string}
    | {any: ReadyPartSpec[]});
export interface ReadyBlock {all: ReadyPartSpec[]; auto?: boolean; then?: {on: string; match?: Record<string, string>}; slowAfterDays?: number;
    slowNeedsYou?: boolean; autoAfterDays?: number}
/** A rehearsal's readiness step (doc 86, R2): assert the phase is (not) ready, or press Advance. */
export type ReadyStep = {check: 'ready'; met: boolean} | {advance: 'now' | 'anyway' | 'then'};
export const isStep = (f: unknown): f is ReadyStep => !!f && typeof f === 'object' && !('kind' in f) && ('check' in f || 'advance' in f);
/** One part of a library entry (tools/story_library.components): an opening, an objective, a participation rule, an outcome. */
export interface Component {role: string; label: string; trigger?: Trigger; objective?: Objective; gate?: Pick<Mission, 'mode' | 'percent' | 'contributors'>; action?: Action}
export interface Region {id: string; cell: string; x1: number; y1: number; x2: number; y2: number}
export interface Fact {id: string; kind: string; day: number; elapsedDays?: number; actor?: string; target?: string; item?: string;
    cell?: string; x?: number; y?: number; detail?: string; quantity?: number; coins?: number;
    /** Phase 8: the world's facts from this fact on (as game.story_facts holds them), changed section by section. */
    facts?: WorldFacts}
/** The world's facts as of an event (doc 85, 3.4 and Phase 8): game.story_facts, a rehearsal case's `facts`. */
export interface WorldFacts {wolves?: Record<string, Record<string, unknown> | null>; residents?: Record<string, Record<string, unknown> | null>;
    world?: Record<string, unknown>; weather?: Record<string, string | null>; relations?: Record<string, string | null>}
export interface Case {name: string; cohort: string[]; events: Fact[]; expect: string[]; expectEffects?: {phase: string; [key: string]: unknown}[];
    expectMissionVisits?: {phase: string; visit: number; credits: Record<string, string[]>}[]; expectTriggers?: string[]; expectCounters?: Json; expectMissions?: Json;
    seed?: string; rolls?: number[]; parties?: Record<string, string>; expectVariables?: Json; expectOutcome?: string | null;
    /** Phase 6: each quest's copies as the case ends: {quest: {holder: status}} (a wolf id, "party:<id>" or "story"). */
    expectQuests?: Record<string, Record<string, string>>;
    /** Phase 8: the world's facts as the case begins. */
    facts?: WorldFacts}
/** A quest's objective (doc 85, 3.6): an event of a wolf with filters, a goal, what must come first, optional or hidden. */
export interface QuestObjective {id: string; label: string; on: string; signal?: string; match?: Record<string, string>;
    goal?: number | Operand; measure?: 'count' | 'quantity' | 'coins'; after?: string[]; optional?: boolean; hidden?: boolean;
    /** Doc 86, Phase 2: the goal kept up with as its value changes (C1c); only in some cells or a region (SQ1); who it
     *  credits (SQ5). */
    goalLive?: boolean; where?: {cells: string[]} | {region: string}; credit?: 'actor' | 'target' | 'present'}
/** What fails a quest: an event (with filters), or a deadline after it starts. */
export type FailRule = {on: string; match?: Record<string, string>; signal?: string} | {afterDays: number; clock?: 'game' | 'real'};
export interface QuestOffer {ask?: string; say: string; accept?: string; decline?: string; handIn?: string; thanks?: string}
export interface QuestJoin {who?: 'anyone' | 'participants'; region?: string; cap?: number; limit?: number;
    /** Doc 86, N4: game days before the same wolf may take it again, after any end. */
    again?: number}
/** A quest (doc 85, 3.6; Phase 6): a version 2 story's, offered by a resident or given by a trigger. */
export interface Quest {
    id: string; title: string; summary?: string; scope?: 'player' | 'party' | 'story'; contributors?: number; lifetime?: 'campaign' | 'phase';
    phases?: string[]; repeat?: 'once' | 'daily' | 'weekly' | 'always'; giver?: string; board?: string; offer?: QuestOffer; handIn?: boolean; join?: QuestJoin;
    abandon?: boolean; objectives: QuestObjective[]; fail?: FailRule[]; rewards?: Effect[]; bonus?: Effect[];
    /** Doc 86, C3b: each credited wolf rewarded by their share ({"share": true} in the rewards), at least creditMin. */
    credit?: 'share'; creditMin?: number;
}
export interface Audience {kind: string; days?: number; players?: string[]; cap?: number}
export interface Program {
    format?: string; version?: number; id: string; title: string; audience: Audience;
    start: string; phases: Phase[]; triggers?: Trigger[]; regions?: Region[]; scenes?: Scene[]; tests: Case[]; quietAfterHours?: number;
    variables?: Variable[]; quests?: Quest[];
    /** Doc 86, L2: the story's prefix (nh, sq, mq, lq, st): its camps' names begin with it. */
    prefix?: string; activeDays?: number;
}

export const SIGNAL = 'story signal';
export const GATE = 'missions completed';
export const READY = 'ready';   // (Doc 86, R2: the exit a phase's Readiness takes.)
export const FIELDS = ['actor', 'target', 'cell', 'item', 'detail'] as const;
export const AMOUNTS = ['quantity', 'coins'] as const;
const clone = <T,>(v: T): T => structuredClone(v);

// --------------------------------------------------------------------------------------------------- names and sentences

/** What an id is called, by kind (refs from the `refs` op, and the program's own regions, phases, scenes, missions). */
export type Names = Record<string, Record<string, string>>;
export interface Refs {
    residents?: {id: string; name: string; home?: string; work?: string}[]; wolves?: {id: string; name: string}[];
    places?: {id: string; kind: string; name: string; width?: number; height?: number; town?: string}[];
    factions?: {id: string; name: string}[]; towns?: {id: string; name: string}[]; markets?: {id: string; name: string}[];
    areas?: {id: string; name: string}[]; ships?: {id: string; name: string}[]; items?: {id: string; name: string; category: string}[];
    templates?: {id: string; name: string}[]; projects?: {id: string; name: string}[]; injuries?: string[];
    /** Phase 9: the world's doors (by the id of one side; a story locks both) and its mantles. */
    doors?: {id: string; name: string; cell?: string}[]; mantles?: {id: string; name: string}[];
    /** Phase 10: the stories a chapter may start (drafts, releases, campaigns' chapters), and the world's declared variables. */
    stories?: {id: string; name: string; hint?: string}[]; worldVars?: {id: string; name: string; type: string; note?: string}[];
    /** Doc 86, SQ2: the businesses' tills a reward may pay from (till:job:<founder>, as the last checkpoint has them). */
    tills?: {id: string; name: string}[];
}
/** The kinds a picker of a type lists (a `character` is a wolf or a resident). */
export const REF_KIND: Record<string, keyof Refs> = {
    resident: 'residents', wolf: 'wolves', place: 'places', faction: 'factions', town: 'towns', market: 'markets', area: 'areas',
    ship: 'ships', item: 'items', template: 'templates', project: 'projects', door: 'doors', mantle: 'mantles', story: 'stories',
};
/** Doc 86, Phase 1b: the values a program's effects give one parameter (talk memory's keys and values, figures' ids). */
export function effectValuesOf(p: Program, kinds: string[], key: string): string[] {
    const out = new Set<string>();
    const effects = [...p.phases.flatMap(ph => ph.effects ?? []), ...(p.quests ?? []).flatMap(q => [...(q.rewards ?? []), ...(q.bonus ?? [])]),
        ...[...(p.triggers ?? []), ...p.phases.flatMap(ph => ph.triggers ?? [])].flatMap(t => allActions(t.actions).flatMap(({a}) => a.do === 'effect' ? [a.effect] : []))];
    for (const e of effects) if (kinds.includes(e.kind) && typeof e[key] === 'string' && e[key]) out.add(e[key] as string);
    return [...out].sort();
}
export const talkKeysOf = (p: Program) => effectValuesOf(p, ['talk.remember', 'talk.forget'], 'key');
export const talkValuesOf = (p: Program) => effectValuesOf(p, ['talk.remember'], 'value');
export const figuresOf = (p: Program) => effectValuesOf(p, ['figure.set', 'figure.clear'], 'figure');
/** Phase 9: the strangers (visitor.add) a program brings, by the ids it gives them. */
export function visitorsOf(p: Program): string[] {
    const out = new Set<string>();
    const effects = [...p.phases.flatMap(ph => ph.effects ?? []),
        ...[...(p.triggers ?? []), ...p.phases.flatMap(ph => ph.triggers ?? [])].flatMap(t => allActions(t.actions).flatMap(({a}) => a.do === 'effect' ? [a.effect] : []))];
    for (const e of effects) if ((e.kind === 'visitor.add' || e.kind === 'visitor.leave') && typeof e.visitor === 'string' && e.visitor) out.add(e.visitor);
    return [...out].sort();
}
export function namesOf(refs: Refs | null, program?: Program | null): Names {
    const out: Names = {};
    for (const [type, kind] of Object.entries(REF_KIND)) {
        const list = refs?.[kind];
        if (Array.isArray(list)) out[type] = Object.fromEntries((list as {id: string; name: string}[]).map(r => [r.id, r.name]));
    }
    out.character = {...out.resident, ...out.wolf};
    if (program) {
        out.phase = Object.fromEntries(program.phases.map(p => [p.id, p.id]));
        out.region = Object.fromEntries((program.regions ?? []).map(r => [r.id, r.id]));
        out.scene = Object.fromEntries((program.scenes ?? []).map(s => [s.id, s.title || s.id]));
        out.mission = Object.fromEntries(program.phases.flatMap(p => (p.missions ?? []).map(m => [m.id, m.title || m.id])));
        // (Version 2: each variable's scope and type, for its sentence: "the acting wolf's" only before a wolf's own.)
        out['var-scope'] = Object.fromEntries((program.variables ?? []).map(v => [v.id, v.scope]));
        out['var-type'] = Object.fromEntries((program.variables ?? []).map(v => [v.id, v.type]));
        // (Phase 6: quests by title, their objectives by label.)
        out.quest = Object.fromEntries((program.quests ?? []).map(q => [q.id, q.title || q.id]));
        out.objective = Object.fromEntries((program.quests ?? []).flatMap(q => q.objectives.map(o => [o.id, o.label || o.id])));
    }
    out.purse = purseNames(refs);
    return out;
}

export interface Slot {
    slot: string; keys: string[]; type: string; label: string; value: unknown; shown: string; empty: boolean; param?: Param;
    /** Not shown in the sentence (an unused optional part, or "whose" before a variable of the story). */
    hidden?: boolean;
}
export type Part = {text: string} | Slot;
const SLOT = /\{([^}]+)\}/g;

/** A value as words: a name for an id, the catalogue's word for a choice, "the acting wolf" for $actor. */
export function shown(type: string, value: unknown, names: Names, param?: Param): string {
    if (type === 'operand') return operandWords(value as Operand, names);
    if (value === undefined || value === null || value === '') return '';
    // (Doc 86, C1a: a parameter given a value, worked out as it fires; C12b: a filter that begins with or is one of.)
    if (typeof value === 'object') {
        const m = matchWords(value, names, type);
        if (m !== undefined) return m;
        if (isOperand(value)) return operandWords(value as Operand, names);
    }
    if (typeof value === 'boolean') return param?.words?.[String(value)] ?? (value ? 'yes' : 'no');
    const text = String(value);
    if (param?.words && text in param.words) return param.words[text];
    if (text === '$each' && type === 'resident') return 'the looped resident';
    if (type === 'wolf' || type === 'character') {
        if (text === '$actor') return 'the acting wolf';
        if (text === '$each') return 'the looped wolf';
        if (text === 'world' && param?.world) return 'everyone (a public scene)';
    }
    const name = names[type]?.[text];
    if (name && name !== text) return name;
    if (type === 'choice' || type === 'int' || type === 'days' || type === 'number') return text.replaceAll('_', ' ');
    // (Doc 86, C1b: a fill reads as ⟨name⟩, a wolf's own as ⟨name of the wolf⟩; Sentence.tsx shows each as a chip.)
    if (type === 'text') return '“' + fillChips(text) + '”';
    return text;
}
/** A text's fills as the sentence shows them: {bar} as ⟨bar⟩, {p_town.of} as ⟨p_town of the wolf⟩, {{ and }} as braces. */
export const fillChips = (text: string) => text.replace(/\{\{|\}\}|\{([A-Za-z0-9_][A-Za-z0-9_.:-]*?)(\.of)?\}/g,
    (m, name?: string, of?: string) => m === '{{' ? '{' : m === '}}' ? '}' : `⟨${name}${of ? ' of the wolf' : ''}⟩`);

/** A catalogue sentence cut into words and slots; `slot(name)` says what fills each. */
export function parts(template: string, slot: (name: string) => Omit<Slot, 'slot'>): Part[] {
    const out: Part[] = [];
    let last = 0;
    for (const m of template.matchAll(SLOT)) {
        if (m.index! > last) out.push({text: template.slice(last, m.index)});
        out.push({slot: m[1], ...slot(m[1])});
        last = m.index! + m[0].length;
    }
    if (last < template.length) out.push({text: template.slice(last)});
    return out;
}
export const plain = (ps: Part[]) => ps.map(p => 'text' in p ? p.text : p.hidden ? '' : (p.shown || p.label)).join('').replace(/ {2,}/g, ' ').replace(/ ([,:])/g, '$1').trim();

const OPERAND_KEYS = ['var', 'field', 'count', 'since', 'each', 'audience', 'purse', 'percent', 'among', 'plus', 'minus', 'min', 'max', 'times', 'div'];
/** Whether a value is a version 2 value's form ({var}, {field}, arithmetic...), as a parameter marked operand holds one. */
export const isOperand = (v: unknown): boolean => !!v && typeof v === 'object' && !Array.isArray(v) && OPERAND_KEYS.some(k => k in (v as object));

/** A filter's value in words (doc 86, C12b): "begins with “stood watch”", "one of Coldwater, Larkfield"; undefined when it is
 *  not a filter's form. */
export function matchWords(v: unknown, names: Names, type = 'text'): string | undefined {
    if (!v || typeof v !== 'object' || Array.isArray(v)) return undefined;
    const o = v as Record<string, unknown>;
    if (typeof o.starts === 'string' && Object.keys(o).length === 1) return `begins with “${o.starts}”`;
    if (Array.isArray(o.in) && Object.keys(o).length === 1) return 'one of ' + o.in.map(x => shown(type === 'text' ? 'plain' : type, x, names)).join(', ');
    return undefined;
}

/** The fills of a version 2 text (doc 86, C1b): {name} and {name.of}; {{ and }} are braces. */
export function fillsOf(text: string): {name: string; of: boolean}[] {
    const out: {name: string; of: boolean}[] = [];
    for (const m of text.matchAll(/\{\{|\}\}|\{([A-Za-z0-9_][A-Za-z0-9_.:-]*?)(\.of)?\}/g)) if (m[1]) out.push({name: m[1], of: !!m[2]});
    return out;
}
/** The widest a text with fills can be (story_triggers.check_fills): a number 11, a switch 3, a text 200, a wolf's name 24,
 *  another name 40. */
export function widestFilled(text: string, p: Program): number {
    const base = text.replace(/\{\{|\}\}|\{([A-Za-z0-9_][A-Za-z0-9_.:-]*?)(\.of)?\}/g, m => m === '{{' ? '{' : m === '}}' ? '}' : '').length;
    const widest: Record<string, number> = {number: 11, switch: 3, text: 200, wolf: 24};
    return base + fillsOf(text).reduce((n, f) => {
        const v = p.variables?.find(x => x.id === f.name);
        return n + (v?.mirror ? 11 : widest[v?.type ?? ''] ?? 40);
    }, 0);
}

/** The name tokens a fill leaves for the game ({@w:<wolf>}, {@r:...}, {@p:...}, {@i:...}, {@f:...}) as a rehearsal shows them
 *  (doc 86, C1b): «wolf: Bilbo», with the rehearsal's names. */
export function tokenWords(text: string, names: Names): string {
    const kinds: Record<string, [string, string]> = {w: ['wolf', 'wolf'], r: ['resident', 'resident'], p: ['place', 'place'], i: ['item', 'item'], f: ['faction', 'faction']};
    return text.replace(/\{@([wrpif]):([^}]*)\}/g, (_m, k: string, id: string) => `«${kinds[k][0]}: ${names[kinds[k][1]]?.[id] ?? id}»`);
}

const FIELD_WORDS: Record<string, string> = {actor: 'the acting wolf', target: "the event's target", cell: "the event's place", item: "the event's item",
    detail: "the event's detail", quantity: "the event's quantity", coins: "the event's coins"};
/** A version 2 value in words: 3, on, “text”, ropes, the event's item, how many in signed, days since gate, the looped wolf. */
export function operandWords(o: Operand | undefined, names: Names): string {
    if (o === undefined || o === null) return '';
    if (typeof o === 'boolean') return o ? 'on' : 'off';
    if (typeof o === 'number') return String(o);
    if (typeof o === 'string') return '“' + o + '”';
    const whose = (x: {of?: string}) => x.of === '$each' ? "the looped wolf's " : '';
    if ('var' in o) return whose(o) + o.var;
    if ('field' in o) return FIELD_WORDS[o.field] ?? o.field;
    if ('count' in o) return 'how many in ' + whose(o) + o.count;
    if ('since' in o) return 'the days since ' + whose(o) + o.since;
    if ('each' in o) return 'the looped value';
    if ('audience' in o) return AUDIENCE_WORDS[o.audience] ?? o.audience;
    if ('purse' in o) return `what ${names.purse?.[o.purse] ?? o.purse} holds`;
    if ('share' in o) return "the wolf's share";
    if ('tally' in o) return tallyWords(o.tally);
    if ('percent' in o || 'among' in o) {
        const src = ('percent' in o ? o.percent : o.among) as ShareSource;
        const who = OF_WORDS[o.of ?? 'active'] ?? o.of;
        const which = 'list' in src ? `in ${src.list}` : 'who meet a test';
        return 'percent' in o ? `the share (%) of the ${who} wolves ${which}` : `how many of the ${who} wolves are ${which.replace('who meet', 'meeting')}`;
    }
    const [key, pair] = Object.entries(o)[0] as [string, [Operand, Operand]];
    const [a, b] = pair.map(x => operandWords(x, names));
    return key === 'plus' ? `(${a} + ${b})` : key === 'minus' ? `(${a} − ${b})` : key === 'times' ? `(${a} × ${b})` : key === 'div' ? `(${a} ÷ ${b}, whole)`
        : key === 'min' ? `the smaller of ${a} and ${b}` : `the larger of ${a} and ${b}`;
}
/** Doc 86, J11: "the p_voice of every wolf of the story whose p_backs is “lamp”, added up", "how many wolves in pledged". */
export function tallyWords(t: Tally): string {
    const over = t.over && typeof t.over === 'object' ? `in ${t.over.list}` : 'of the story';
    const which = t.where ? ` whose ${t.where} is ${typeof t.is === 'boolean' ? (t.is ? 'on' : 'off') : '“' + (t.is ?? '') + '”'}` : '';
    return t.sum ? `the ${t.sum} of every wolf ${over}${which}, added up` : `how many wolves ${over}${which}`;
}
const AUDIENCE_WORDS: Record<string, string> = {all: "how many wolves the story has", active: 'how many wolves are active', online: 'how many wolves are online'};
const OF_WORDS: Record<string, string> = {active: 'active', all: "story's", online: 'online'};
/** What kind of value an operand is (number, switch or text), as story_triggers.operand_type says; '' when it cannot tell. */
export function operandKind(o: Operand | undefined, p: Program): string {
    if (typeof o === 'number') return 'number';
    if (typeof o === 'boolean') return 'switch';
    if (typeof o === 'string') return 'text';
    if (!o || typeof o !== 'object') return '';
    if ('var' in o) return kindOfType(p.variables?.find(v => v.id === o.var)?.type ?? '');
    if ('field' in o) return (AMOUNTS as readonly string[]).includes(o.field) ? 'number' : 'text';
    if ('each' in o) return 'text';
    return 'number';
}
export const LIST_TYPES: Record<string, string> = {numbers: 'number', texts: 'text', wolves: 'wolf', residents: 'resident'};
/** operandKind from the names alone (the sentence's view of the program). */
export function operandKindIn(o: Operand | undefined, names: Names): string {
    if (o && typeof o === 'object' && 'var' in o) return kindOfType(names['var-type']?.[o.var] ?? '');
    if (o === undefined) return '';
    return operandKind(o, {variables: []} as unknown as Program);
}
export const kindOfType = (t: string) => t === 'number' ? 'number' : t === 'switch' ? 'switch' : t ? 'text' : '';
export const isV2 = (p: Program | null | undefined) => p?.version === 2;

export function eventOf(cat: Catalogue, id: string): CatEvent | undefined { return cat.events.find(e => e.id === id); }
export function itemOf(list: CatItem[], id: string): CatItem | undefined { return list.find(e => e.id === id); }
/** The catalogue's entry for an action: version 2's Set and Add on a declared variable are "set var" and "add var". */
export function actionItem(cat: Catalogue, a: Action | {do: string; var?: unknown}): CatItem | undefined {
    if ((a.do === 'set' || a.do === 'add') && 'var' in a && a.var !== undefined) return itemOf(cat.actions, a.do + ' var');
    return itemOf(cat.actions, a.do);
}

/** The params of an item filled from a record (an action, a condition, an effect): a slot per sentence slot. */
export function itemParts(item: CatItem, record: Record<string, unknown>, names: Names): Part[] {
    const params = item.params ?? [];
    return parts(item.sentence, name => {
        const keys = name.split(',');
        const param = params.find(p => p.key === keys[0]);
        if (keys.length > 1) {   // A tile: cell, x, y as one picker.
            const [cell, x, y] = keys.map(k => record[k]);
            const empty = cell === undefined || cell === '';
            return {keys, type: 'tile', label: 'a tile', value: {cell, x, y}, param,
                shown: empty ? '' : `${shown('place', cell, names)} (${x}, ${y})`, empty};
        }
        const value = record[keys[0]];
        const type = param?.type ?? 'text';
        if (type === 'whose') {   // (Shown only before a variable of each wolf or party.)
            const name = record.var ?? record.list;
            const personal = ['player', 'party'].includes(names['var-scope']?.[String(name)] ?? '');
            const text = personal ? (param?.words?.[String(value ?? '$actor')] ?? "the acting wolf's") : '';
            return {keys, type, label: param?.label ?? keys[0], value, param, shown: text, empty: !personal, hidden: !personal} as Omit<Slot, 'slot'>;
        }
        if (type === 'operand' && param?.like) {   // (A value of the kind of the variable or the value beside it.)
            const other = record[param.like];
            const vt = typeof other === 'string' ? names['var-type']?.[other] ?? '' : '';
            const kind = typeof other === 'string' ? kindOfType(LIST_TYPES[vt] ?? vt) : operandKindIn(other as Operand, names);
            const p2 = kind ? {...param, kind} : param;
            return {keys, type, label: p2.label, value, param: p2, shown: operandWords(value as Operand, names), empty: value === undefined};
        }
        if (param?.optional && (value === undefined || value === '')) {
            // (A list or a timer variable that only some choices need: asked for when "the list" or "the timer" is chosen.)
            const wanted = param.type === 'variable' && (record.over === 'list' || record.from === 'var');
            return {keys, type, label: wanted ? param.label : param.words?.[''] ?? '', value, param, shown: param.words?.[''] ?? '', empty: true,
                hidden: !param.words?.[''] && !wanted} as Omit<Slot, 'slot'>;
        }
        if (type === 'objective' && param) {   // (Phase 6: the objective of the quest beside it.)
            const quest = typeof record.quest === 'string' ? record.quest : '';
            const p2 = {...param, quest};
            return {keys, type, label: p2.label, value, param: p2, shown: shown(type, value, names, p2), empty: value === undefined || value === ''};
        }
        const effective = type === 'value' ? (typeof value === 'number' ? 'int' : 'text') : type;
        const words = param && param.type === 'int' && param.words && typeof value === 'string' ? param.words[value] : undefined;
        const text = words ?? shown(effective, value, names, param);
        return {keys, type, label: param?.label ?? keys[0], value, param, shown: text, empty: text === '' && type !== 'bool'};
    });
}

/** "{actor} gives {item} to {target}" for a trigger, an exit or an objective: each field slot is its filter, or the
 *  catalogue's word for "anyone" when there is none. */
export function eventParts(cat: Catalogue, rule: {on: string; signal?: string; match?: Record<string, string>; afterDays?: number},
    names: Names): Part[] {
    const ev = eventOf(cat, rule.on);
    if (rule.on === SIGNAL) {
        return parts('the signal {signal} arrives', () => ({keys: ['signal'], type: 'signal', label: 'a signal', value: rule.signal,
            shown: rule.signal ?? '', empty: !rule.signal}));
    }
    if (rule.on === 'clock') {
        return parts('{afterDays} game days after the phase opened', () => ({keys: ['afterDays'], type: 'days', label: 'days',
            value: rule.afterDays, shown: rule.afterDays === undefined ? '' : String(rule.afterDays), empty: rule.afterDays === undefined,
            param: {key: 'afterDays', type: 'days', label: 'days', min: 1 / 1440, max: 36500}}));
    }
    const template = ev?.sentence || rule.on;
    const out = parts(template, name => fieldSlot(ev, name, rule.match?.[name], names));
    // A filter on a field the sentence does not show: said after it.
    for (const [key, value] of Object.entries(rule.match ?? {})) {
        if (template.includes('{' + key + '}')) continue;
        out.push({text: `, ${key} `}, {slot: key, ...fieldSlot(ev, key, value, names)});
    }
    return out;
}
function fieldSlot(ev: CatEvent | undefined, key: string, value: string | undefined, names: Names): Omit<Slot, 'slot'> {
    const field = ev?.fields[key];
    const type = field?.type ?? (key === 'item' ? 'item' : key === 'cell' ? 'place' : 'text');
    const text = value === undefined ? '' : shown(type, value, names);
    // (An empty slot shows the catalogue's word for "anyone"; where that is a bare "any", what the field holds reads better.)
    const word = field?.any && field.any !== 'any' ? field.any : field?.label ?? key;
    return {keys: [key], type, label: word, value, shown: text, empty: value === undefined};
}

/** Where a trigger listens and how often, in words: "a wolf of the story", "once per wolf this visit". */
export function headWords(cat: Catalogue, t: Trigger): string {
    const who = cat.trigger.audience[t.audience ?? 'world'] ?? t.audience;
    return `${who} · fires ${cat.trigger.once[t.once ?? 'each'] ?? t.once}`;
}

export function conditionParts(cat: Catalogue, c: Condition, names: Names): Part[] {
    const item = itemOf(cat.conditions, c.test);
    if (!item) return [{text: c.test}];
    if (isWorldCondition(item)) {   // (Doc 85, Phase 8: whose and where left out read as their defaults, the acting wolf.)
        const defaults = Object.fromEntries((item.params ?? []).filter(p => ['who', 'of', 'where'].includes(p.key) && p.default !== undefined).map(p => [p.key, p.default]));
        return itemParts(item, {...defaults, ...c} as unknown as Json, names);
    }
    if (c.test === 'participation') {
        // (Doc 86, R1: who counts as the list or the test, the test in words, the cap only when given.)
        const source = c.source ?? ('where' in c && c.where ? 'where' : 'list');
        const ps = itemParts(item, {...c, source, of: c.of ?? 'active'} as unknown as Json, names);
        return ps.map(p => !('slot' in p) ? p : p.slot === 'where' ? {...p, hidden: source !== 'where', empty: source === 'where' && !c.where,
            shown: c.where ? '(' + plain(conditionParts(cat, c.where, names)).replaceAll("the looped wolf's", "the wolf's") + ')' : ''}
            : p.slot === 'list' ? {...p, hidden: source !== 'list', empty: source === 'list' && !c.list}
            : p.slot === 'cap' ? {...p, shown: c.cap ? `${c.cap} always enough` : 'no cap', empty: false}
            : p.slot === 'floor' ? {...p, shown: String(c.floor ?? 1), empty: false} : p);
    }
    if (c.test === 'field') {
        const amount = (AMOUNTS as readonly string[]).includes(c.field);
        const ps = itemParts(item, c as unknown as Json, names);
        // (Doc 86, C12b: "begins with" a text, "is one of" a list.)
        const value = c.op === 'in' && Array.isArray(c.value) ? (c.value as unknown[]).map(x => '“' + String(x) + '”').join(', ') : undefined;
        return ps.map(p => 'slot' in p && p.slot === 'value' ? {...p, type: amount ? 'int' : 'text',
            shown: amount ? String(c.value) : value ?? shown('text', c.value, names)} : p);
    }
    return itemParts(item, c as unknown as Json, names);
}

export function actionParts(cat: Catalogue, a: Action, names: Names): Part[] {
    if (a.do === 'effect') return effectParts(cat, a.effect, names);
    const item = actionItem(cat, a);
    if (!item) return [{text: String((a as Json).do)}];
    const record = {...Object.fromEntries((item.params ?? []).filter(p => p.default !== undefined).map(p => [p.key, p.default])), ...a};
    return itemParts(item, record as Json, names);
}

/** An effect the game applies, so the game may refuse it (doc 85, Phase 7: its `ifRefused`): not the story's own, not a
 *  phase's line. */
export function refusable(cat: Catalogue, kind: string): boolean {
    const item = itemOf(cat.effects, kind);
    return !!item && !(item as CatItem & {story?: boolean}).story && kind !== 'npc.line';
}

export function effectParts(cat: Catalogue, e: Effect, names: Names): Part[] {
    const item = itemOf(cat.effects, e.kind);
    if (!item) return [{text: 'the world effect ' + e.kind}];
    return itemParts(item, e, names);
}

export function exitParts(cat: Catalogue, x: Exit, names: Names, phaseMissions: Mission[]): Part[] {
    const head: Part[] = [];
    if (x.on === GATE) {
        const titles = (x.missions ?? []).map(id => phaseMissions.find(m => m.id === id)?.title || id);
        head.push({text: 'the missions '}, {slot: 'missions', keys: ['missions'], type: 'missions', label: 'missions', value: x.missions,
            shown: titles.join(', '), empty: !titles.length}, {text: ' are complete'});
    } else if (x.on === READY) {
        head.push({text: 'the phase is ready (its Readiness is met, or an admin presses Advance)'});
    } else if (x.on === 'dm signal') {
        head.push({text: 'a Dungeon Master directs '}, {slot: 'detail', keys: ['detail'], type: 'text', label: 'a direction',
            value: x.match?.detail, shown: x.match?.detail ?? '', empty: !x.match?.detail});
    } else {
        head.push(...eventParts(cat, x, names));
        if (x.participant) head.push({text: ', by a wolf of the story'});
        if (x.players !== undefined) head.push({text: `, once ${x.players} different wolves of the story have`});
        if (x.percent !== undefined) head.push({text: `, once ${x.percent}% of the audience have`});
        if (x.afterDays !== undefined && x.on !== 'clock') head.push({text: ` (after ${x.afterDays} days)`});
    }
    return [...head, {text: ' → go to '}, {slot: 'to', keys: ['to'], type: 'phase', label: 'a phase', value: x.to, shown: x.to, empty: !x.to}];
}

// --------------------------------------------------------------------------------------------------------------- the tree

export type Ref =
    | {kind: 'settings'} | {kind: 'regions'} | {kind: 'scenes'} | {kind: 'tests'}
    | {kind: 'phase'; phase: number} | {kind: 'missions'; phase: number} | {kind: 'entry'; phase: number}
    | {kind: 'trigger'; phase: number | null; index: number}
    | {kind: 'exit'; phase: number; index: number}
    // (Phase 6: the story's quests, and one of them.)
    | {kind: 'quests'} | {kind: 'quest'; index: number};
export const sameRef = (a: Ref | null | undefined, b: Ref | null | undefined) => JSON.stringify(a) === JSON.stringify(b);

export interface Folder {name: string; items: {ref: Ref; order: number}[]}
export interface Branch {label: string; scope: number | null; folders: Folder[]; loose: {ref: Ref; order: number}[]; exits: Ref[];
    missions: number; entry: number; opening?: boolean; ending?: boolean}

/** Campaign triggers, then each phase (its triggers by folder, in the order they are tried, its Exits, its missions). */
export function buildTree(p: Program): Branch[] {
    const branch = (label: string, scope: number | null, list: Trigger[]): Branch => {
        const folders: Folder[] = [];
        const loose: Branch['loose'] = [];
        list.forEach((t, index) => {
            const item = {ref: {kind: 'trigger', phase: scope, index} as Ref, order: index + 1};
            if (t.folder) {
                let f = folders.find(x => x.name === t.folder);
                if (!f) folders.push(f = {name: t.folder, items: []});
                f.items.push(item);
            } else loose.push(item);
        });
        return {label, scope, folders, loose, exits: [], missions: 0, entry: 0};
    };
    return [
        branch('Campaign (always on)', null, p.triggers ?? []),
        ...p.phases.map((ph, i) => ({
            ...branch(ph.id, i, ph.triggers ?? []),
            exits: ph.exits.map((_, index) => ({kind: 'exit', phase: i, index}) as Ref),
            missions: ph.missions?.length ?? 0, entry: ph.effects.length,
            // (Phase 6: a phase where a quest is offered stays open: a small quest's one phase is not an ending.)
            opening: p.start === ph.id, ending: !ph.exits.length && !ph.triggers?.length && !(p.triggers?.length && ph.campaignRules)
                && !(p.quests ?? []).some(q => !q.phases || q.phases.includes(ph.id)),
        })),
    ];
}

export function triggersOf(p: Program, scope: number | null): Trigger[] {
    return (scope === null ? p.triggers : p.phases[scope]?.triggers) ?? [];
}
export function withTriggers(p: Program, scope: number | null, list: Trigger[]): Program {
    if (scope === null) {
        if (!list.length && p.triggers === undefined) return p;
        return {...p, triggers: list};
    }
    return {...p, phases: p.phases.map((ph, i) => i === scope ? {...ph, triggers: list} : ph)};
}
export function triggerAt(p: Program, ref: Ref): Trigger | undefined {
    return ref.kind === 'trigger' ? triggersOf(p, ref.phase)[ref.index] : undefined;
}
export function exitAt(p: Program, ref: Ref): Exit | undefined {
    return ref.kind === 'exit' ? p.phases[ref.phase]?.exits[ref.index] : undefined;
}

/** The ids a trigger of a scope must not take: a phase's own and the campaign's (they are tried together). */
export function takenIds(p: Program, scope: number | null): Set<string> {
    const ids = new Set((p.triggers ?? []).map(t => t.id));
    const phases = scope === null ? p.phases : [p.phases[scope]];
    for (const ph of phases) for (const t of ph?.triggers ?? []) ids.add(t.id);
    return ids;
}
export function unique(base: string, taken: Set<string>): string {
    const root = base.replace(/-copy(-\d+)?$/, '') || 'trigger';
    if (!taken.has(base)) return base;
    for (let n = 1; ; n++) {
        const id = root + (n === 1 ? '-copy' : '-copy-' + n);
        if (!taken.has(id)) return id;
    }
}

/** Puts triggers into a scope at a place (a folder given: inside it), with ids made unique there. */
export function insertTriggers(p: Program, scope: number | null, at: number, list: Trigger[], folder?: string | null): {program: Program; indexes: number[]} {
    const taken = takenIds(p, scope);
    const added = list.map(t => {
        const copy = clone(t);
        copy.id = unique(copy.id, taken);
        taken.add(copy.id);
        if (folder === null) delete copy.folder;
        else if (folder !== undefined) copy.folder = folder;
        return copy;
    });
    const current = triggersOf(p, scope);
    const where = Math.max(0, Math.min(at, current.length));
    const next = [...current.slice(0, where), ...added, ...current.slice(where)];
    return {program: withTriggers(p, scope, next), indexes: added.map((_, i) => where + i)};
}

export function removeTriggers(p: Program, refs: Ref[]): Program {
    let out = p;
    const byScope = new Map<number | null, Set<number>>();
    for (const r of refs) if (r.kind === 'trigger') {
        if (!byScope.has(r.phase)) byScope.set(r.phase, new Set());
        byScope.get(r.phase)!.add(r.index);
    }
    for (const [scope, gone] of byScope) out = withTriggers(out, scope, triggersOf(out, scope).filter((_, i) => !gone.has(i)));
    return out;
}

/** A trigger moved (dragged) to another place, folder or phase. `to.index` is the position in the target list before the move. */
export function moveTrigger(p: Program, from: {phase: number | null; index: number}, to: {phase: number | null; index: number; folder?: string | null}): {program: Program; ref: Ref} {
    const t = triggersOf(p, from.phase)[from.index];
    if (!t) return {program: p, ref: {kind: 'trigger', ...from}};
    const moved = clone(t);
    if (to.folder === null) delete moved.folder;
    else if (to.folder !== undefined) moved.folder = to.folder;
    let index = to.index;
    if (from.phase === to.phase) {
        const list = [...triggersOf(p, from.phase)];
        list.splice(from.index, 1);
        if (from.index < index) index--;
        index = Math.max(0, Math.min(index, list.length));
        list.splice(index, 0, moved);
        return {program: withTriggers(p, from.phase, list), ref: {kind: 'trigger', phase: from.phase, index}};
    }
    const without = removeTriggers(p, [{kind: 'trigger', ...from}]);
    const taken = takenIds(without, to.phase);
    moved.id = unique(moved.id, taken);
    const list = [...triggersOf(without, to.phase)];
    index = Math.max(0, Math.min(index, list.length));
    list.splice(index, 0, moved);
    return {program: withTriggers(without, to.phase, list), ref: {kind: 'trigger', phase: to.phase, index}};
}

/** The end of a folder in its list (where a trigger dropped on the folder goes, keeping the folder together). */
export function folderEnd(p: Program, scope: number | null, folder: string | null): number {
    const list = triggersOf(p, scope);
    let end = -1;
    list.forEach((t, i) => { if ((t.folder ?? null) === folder) end = i; });
    return end === -1 ? list.length : end + 1;
}

export function renameFolder(p: Program, scope: number | null, from: string, to: string | null): Program {
    return withTriggers(p, scope, triggersOf(p, scope).map(t => {
        if (t.folder !== from) return t;
        const next = {...t};
        if (to) next.folder = to; else delete next.folder;
        return next;
    }));
}

// ------------------------------------------------------------------------------------------------ copy, paste, templates

export interface Fragment {format: 'ratw-triggers'; version: 1; title?: string; triggers: Trigger[]; regions?: Region[]}

/** The regions a list of triggers names (conditions, effects, region events' filters). */
export function regionsUsed(list: unknown, regions: Region[]): Region[] {
    const ids = new Set<string>();
    const visit = (v: unknown) => {
        if (!v || typeof v !== 'object') return;
        if (Array.isArray(v)) { v.forEach(visit); return; }
        const o = v as Json;
        if (typeof o.region === 'string') ids.add(o.region);
        if ((o.on === 'region entered' || o.on === 'region left') && o.match && typeof (o.match as Json).target === 'string') ids.add((o.match as Json).target as string);
        Object.values(o).forEach(visit);
    };
    visit(list);
    return regions.filter(r => ids.has(r.id));
}
export function fragmentOf(p: Program, refs: Ref[], title?: string): Fragment {
    const list = refs.flatMap(r => { const t = triggerAt(p, r); return t ? [clone(t)] : []; });
    const regions = regionsUsed(list, p.regions ?? []);
    return {format: 'ratw-triggers', version: 1, ...(title ? {title} : {}), triggers: list, ...(regions.length ? {regions: clone(regions)} : {})};
}
export function readFragment(text: string): Fragment {
    let v: unknown;
    try { v = JSON.parse(text); } catch { throw Error('This is not a trigger fragment (it is not JSON).'); }
    const f = v as Fragment;
    if (!f || f.format !== 'ratw-triggers' || !Array.isArray(f.triggers) || !f.triggers.every(t => t && typeof t.id === 'string' && typeof t.on === 'string' && Array.isArray(t.actions)))
        throw Error('This is not a trigger fragment: expected {"format": "ratw-triggers", "triggers": [...]}.');
    return f;
}
/** A fragment pasted into a scope at a place: its triggers with ids made unique, the regions it needs that are missing. */
export function pasteFragment(p: Program, f: Fragment, scope: number | null, at: number, folder?: string | null): {program: Program; indexes: number[]; regions: string[]} {
    const have = new Set((p.regions ?? []).map(r => r.id));
    const missing = (f.regions ?? []).filter(r => !have.has(r.id));
    const base = missing.length ? {...p, regions: [...(p.regions ?? []), ...clone(missing)]} : p;
    const {program, indexes} = insertTriggers(base, scope, at, f.triggers, folder);
    return {program, indexes, regions: missing.map(r => r.id)};
}

// ------------------------------------------------------------------------------------------- names: uses and renaming

export interface Use {ref: Ref; where: string}

/** The lists of actions inside one action (version 2's If, For each, Random), each with its part name: then, elif[0].then,
 *  else, actions, choices[1].actions (as story_triggers.part_of names them for the Problems list). */
export function branchesOf(a: Action): {part: string; label: string; list: Action[]}[] {
    if (a.do === 'if') return [{part: 'then', label: 'then', list: a.then}, ...(a.elif ?? []).map((b, k) => ({part: `elif[${k}].then`, label: 'else if', list: b.then})),
        ...(a.else ? [{part: 'else', label: 'else', list: a.else}] : [])];
    if (a.do === 'each') return [{part: 'actions', label: 'do', list: a.actions}];
    if (a.do === 'random') return a.choices.map((c, k) => ({part: `choices[${k}].actions`, label: `weight ${c.weight}`, list: c.actions}));
    return [];
}
/** Every action of a list, nested ones too, with its part ("actions[2].then[0]"). */
export function allActions(list: Action[], base = 'actions'): {a: Action; part: string}[] {
    return list.flatMap((a, i) => {
        const part = `${base}[${i}]`;
        return [{a, part}, ...branchesOf(a).flatMap(b => allActions(b.list, `${part}.${b.part}`))];
    });
}
/** Every condition of a trigger, its own and its If actions', with where. */
export function allConditions(t: Trigger): Condition[] {
    return [...conditionsOf(t.if), ...allActions(t.actions).flatMap(({a}) => a.do === 'if' ? [...conditionsOf(a.if), ...(a.elif ?? []).flatMap(b => conditionsOf(b.if))] : [])];
}
/** A list of actions with every action (nested ones too) passed through `fn`, children first; untouched lists kept as they are. */
export function mapActions(list: Action[], fn: (a: Action) => Action): Action[] {
    let changed = false;
    const out = list.map(a => {
        let next: Action = a;
        if (a.do === 'if') {
            const then = mapActions(a.then, fn), elif = a.elif?.map(b => ({...b, then: mapActions(b.then, fn)})), other = a.else && mapActions(a.else, fn);
            if (then !== a.then || (elif && elif.some((b, k) => b.then !== a.elif![k].then)) || other !== a.else)
                next = {...a, then, ...(a.elif ? {elif: elif!.map((b, k) => b.then === a.elif![k].then ? a.elif![k] : b)} : {}), ...(a.else ? {else: other!} : {})};
        } else if (a.do === 'each') {
            const actions = mapActions(a.actions, fn);
            if (actions !== a.actions) next = {...a, actions};
        } else if (a.do === 'random') {
            const choices = a.choices.map(c => { const actions = mapActions(c.actions, fn); return actions === c.actions ? c : {...c, actions}; });
            if (choices.some((c, k) => c !== a.choices[k])) next = {...a, choices};
        }
        const mapped = fn(next);
        if (mapped !== a) changed = true;
        return mapped;
    });
    return changed ? out : list;
}
/** A condition tree with every node passed through `fn` (leaves and groups), untouched trees kept. */
export function mapCondition(c: Condition, fn: (c: Condition) => Condition): Condition {
    if (c.test === 'all' || c.test === 'any') {
        const conditions = c.conditions.map(x => mapCondition(x, fn));
        return fn(conditions.some((x, i) => x !== c.conditions[i]) ? {...c, conditions} : c);
    }
    if (c.test === 'not') { const child = mapCondition(c.condition, fn); return fn(child !== c.condition ? {...c, condition: child} : c); }
    return fn(c);
}
/** A trigger's conditions (its own and its If actions') passed through `fn`. */
function mapTriggerConditions(t: Trigger, fn: (c: Condition) => Condition): Trigger {
    const own = t.if && mapCondition(t.if, fn);
    const actions = mapActions(t.actions, a => {
        if (a.do !== 'if') return a;
        const cond = mapCondition(a.if, fn), elif = a.elif?.map(b => { const c = mapCondition(b.if, fn); return c === b.if ? b : {...b, if: c}; });
        return cond === a.if && (!elif || elif.every((b, k) => b === a.elif![k])) ? a : {...a, if: cond, ...(elif ? {elif} : {})};
    });
    return own === t.if && actions === t.actions ? t : {...t, ...(own ? {if: own} : {}), actions};
}

/** Every signal and counter the program names (for the pickers, and "where is it used"). */
export function signalsOf(p: Program): string[] {
    const out = new Set<string>();
    eachTrigger(p, t => {
        if (t.on === SIGNAL && t.signal) out.add(t.signal);
        for (const {a} of allActions(t.actions)) if (a.do === 'emit' || a.do === 'schedule' || a.do === 'cancel' || a.do === 'shift') out.add(a.signal);
        for (const c of allConditions(t)) if (c.test === 'timer') out.add(c.signal);
    });
    for (const ph of p.phases) for (const m of ph.missions ?? []) for (const o of m.objectives) if (o.signal) out.add(o.signal);
    for (const q of p.quests ?? []) for (const o of [...q.objectives, ...(q.fail ?? [])]) if ('signal' in o && o.signal) out.add(o.signal);
    return [...out].sort();
}
export function countersOf(p: Program): {scope: string; key: string}[] {
    const out = new Map<string, {scope: string; key: string}>();
    const walk = (c: Condition | undefined) => {
        if (!c) return;
        if (c.test === 'all' || c.test === 'any') c.conditions.forEach(walk);
        else if (c.test === 'not') walk(c.condition);
        else if (c.test === 'counter') out.set(c.scope + ':' + c.key, {scope: c.scope, key: c.key});
    };
    if (isV2(p)) return (p.variables ?? []).map(v => ({scope: v.scope, key: v.id})).sort((a, b) => a.key.localeCompare(b.key));
    eachTrigger(p, t => {
        walk(t.if);
        for (const a of t.actions) if ((a.do === 'set' || a.do === 'add') && a.key !== undefined) out.set(a.scope + ':' + a.key, {scope: a.scope, key: a.key});
    });
    return [...out.values()].sort((a, b) => a.key.localeCompare(b.key) || a.scope.localeCompare(b.scope));
}
/** The variables of a story with their types (doc 85, 3.2): a version 2 story's declared ones; a version 1 story's counters
 *  as it reads and writes them, as numbers (the Variables panel, the pickers, the console). */
export function variablesOf(p: Program): Variable[] {
    if (isV2(p)) return p.variables ?? [];
    return countersOf(p).map(c => ({id: c.key, type: 'number', scope: c.scope}));
}
/** The variable names an operand, a condition or an action reads or writes. */
export function operandVars(o: unknown): string[] {
    if (!o || typeof o !== 'object') return [];
    const x = o as Json;
    const own = ['var', 'count', 'since'].flatMap(k => typeof x[k] === 'string' ? [x[k] as string] : []);
    const shares = ['percent', 'among'].flatMap(k => record(x[k]) ? [...(typeof (x[k] as Json).list === 'string' ? [(x[k] as Json).list as string] : []),
        ...(record((x[k] as Json).where) ? conditionsOf((x[k] as Json).where as Condition).flatMap(conditionVars) : [])] : []);
    // (Doc 86, J11: a tally reads what it adds up, the variable it picks wolves by, and the list it is over.)
    const t = record(x.tally) ? x.tally as Json : null;
    const tally = t ? [t.sum, t.where, record(t.over) ? (t.over as Json).list : undefined].filter((v): v is string => typeof v === 'string') : [];
    return [...own, ...shares, ...tally, ...['plus', 'minus', 'min', 'max', 'times', 'div'].flatMap(k => Array.isArray(x[k]) ? (x[k] as unknown[]).flatMap(operandVars) : [])];
}
export function conditionVars(c: Condition): string[] {
    const x = c as unknown as Json;
    return [...(['switch', 'in list', 'days since', 'participation'].includes(c.test) ? [x.var, x.list].filter((v): v is string => typeof v === 'string') : []),
        ...(c.test === 'participation' && c.where ? conditionsOf(c.where).flatMap(conditionVars) : []),
        ...operandVars(x.left), ...operandVars(x.right), ...operandVars(x.value)];
}
export function actionVars(a: Action): {reads: string[]; writes: string[]} {
    const x = a as unknown as Json;
    if (a.do === 'each') return {reads: a.list ? [a.list] : [], writes: []};
    if (a.do === 'pick') return {reads: [a.list], writes: [a.var]};
    if (a.do === 'rank') return {reads: [a.var, ...(a.list ? [a.list] : []), ...operandVars(a.top), ...operandVars(a.min)], writes: [a.into]};
    return {reads: operandVars(x.value), writes: typeof x.var === 'string' && a.do !== 'if' ? [x.var] : []};
}
/** Doc 86, R2: the variables a phase's Readiness reads (its participation parts' lists and tests, its values and tests,
 *  inside an `any` group too). */
export function readyVars(ph: Phase): string[] {
    const part = (x: ReadyPartSpec): string[] => {
        if ('any' in x) return x.any.flatMap(part);
        if ('participation' in x) return 'list' in x.participation ? [x.participation.list] : conditionsOf(x.participation.where).flatMap(conditionVars);
        if ('value' in x) return [...operandVars(x.value), ...operandVars(x.atLeast)];
        if ('test' in x) return conditionsOf(x.test).flatMap(conditionVars);
        return [];
    };
    return (ph.ready?.all ?? []).flatMap(part);
}
export function eachTrigger(p: Program, fn: (t: Trigger, ref: Ref) => void) {
    (p.triggers ?? []).forEach((t, index) => fn(t, {kind: 'trigger', phase: null, index}));
    p.phases.forEach((ph, phase) => (ph.triggers ?? []).forEach((t, index) => fn(t, {kind: 'trigger', phase, index})));
}
function conditionsOf(c: Condition | undefined): Condition[] {
    if (!c) return [];
    if (c.test === 'all' || c.test === 'any') return [c, ...c.conditions.flatMap(conditionsOf)];
    if (c.test === 'not') return [c, ...conditionsOf(c.condition)];
    return [c];
}

/** Where a name is used: a signal, a counter, a region, a phase, or any id (a resident, an item, a place...). */
export function usesOf(p: Program, what: {type: string; id: string; scope?: string}): Use[] {
    const out: Use[] = [];
    const {type, id} = what;
    const hasValue = (v: unknown): boolean => {
        if (v === id) return true;
        if (!v || typeof v !== 'object') return false;
        return Array.isArray(v) ? v.some(hasValue) : Object.values(v).some(hasValue);
    };
    eachTrigger(p, (t, ref) => {
        const label = (ref.kind === 'trigger' && ref.phase !== null ? p.phases[ref.phase].id : 'Campaign') + ' / ' + t.id;
        const numbered = allActions(t.actions).map(({a, part}) => ({a, n: part.replace(/^actions\[(\d+)\]/, (_, i) => String(Number(i) + 1)).replaceAll('actions', '').replace(/[[\]]/g, ' ').replace(/\s+/g, ' ').trim()}));
        if (type === 'signal') {
            if (t.on === SIGNAL && t.signal === id) out.push({ref, where: label + ': listens for it'});
            numbered.forEach(({a, n}) => { if ((a.do === 'emit' || a.do === 'schedule' || a.do === 'cancel' || a.do === 'shift') && a.signal === id)
                out.push({ref, where: `${label}: action ${n} (${a.do === 'emit' ? 'sends it' : a.do === 'schedule' ? 'starts its timer' : a.do === 'shift' ? 'moves its timer' : 'cancels its timer'})`}); });
            if (allConditions(t).some(c => c.test === 'timer' && c.signal === id)) out.push({ref, where: label + ': asks about its timer'});
        } else if (type === 'counter' || type === 'variable') {
            const scoped = (s: string) => what.scope === undefined || s === what.scope;
            if (conditionsOf(t.if).some(c => c.test === 'counter' && c.key === id && scoped(c.scope))) out.push({ref, where: label + ': reads it'});
            numbered.forEach(({a, n}) => { if ((a.do === 'set' || a.do === 'add') && a.key === id && scoped(a.scope ?? '')) out.push({ref, where: `${label}: action ${n} (${a.do}s it)`}); });
            if (allConditions(t).some(c => conditionVars(c).includes(id))) out.push({ref, where: label + ': tests it'});
            numbered.forEach(({a, n}) => { const v = actionVars(a);
                if (v.writes.includes(id)) out.push({ref, where: `${label}: action ${n} (${a.do} it)`});
                else if (v.reads.includes(id)) out.push({ref, where: `${label}: action ${n} reads it`}); });
        } else if (type === 'trigger') {
            if (t.id === id) out.push({ref, where: label});
            numbered.forEach(({a, n}) => { if ((a.do === 'run' || a.do === 'enable' || a.do === 'disable') && a.trigger === id) out.push({ref, where: `${label}: action ${n} (${a.do})`}); });
        } else if (type === 'phase') {
            numbered.forEach(({a, n}) => { if (a.do === 'enter' && a.phase === id) out.push({ref, where: `${label}: action ${n} goes there`}); });
        } else if (hasValue(t)) out.push({ref, where: label});
    });
    p.phases.forEach((ph, phase) => {
        if (type === 'phase') {
            ph.exits.forEach((x, index) => { if (x.to === id) out.push({ref: {kind: 'exit', phase, index}, where: `${ph.id}: exit ${index + 1} goes there`}); });
        } else if (type === 'variable') {
            if (readyVars(ph).includes(id)) out.push({ref: {kind: 'phase', phase}, where: `${ph.id}: its Readiness reads it`});
        } else if (type !== 'signal' && type !== 'counter' && type !== 'trigger') {
            // (Doc 86, R2: a Readiness part names this phase's missions and the story's quests.)
            if (hasValue(ph.ready)) out.push({ref: {kind: 'phase', phase}, where: `${ph.id}: its Readiness`});
            ph.exits.forEach((x, index) => { if (hasValue(x.match)) out.push({ref: {kind: 'exit', phase, index}, where: `${ph.id}: exit ${index + 1}`}); });
            if (ph.effects.some(hasValue)) out.push({ref: {kind: 'entry', phase}, where: `${ph.id}: when it opens`});
            if (ph.guestVoices?.includes(id)) out.push({ref: {kind: 'phase', phase}, where: `${ph.id}: a guest voice`});
        }
        if (type === 'signal' && (ph.missions ?? []).some(m => m.objectives.some(o => o.signal === id))) out.push({ref: {kind: 'missions', phase}, where: `${ph.id}: a mission objective`});
        else if (!['signal', 'counter', 'variable', 'trigger', 'phase'].includes(type) && (ph.missions ?? []).some(m => m.objectives.some(o => hasValue(o.match))))
            out.push({ref: {kind: 'missions', phase}, where: `${ph.id}: a mission objective's filter`});
    });
    // (Phase 6: the quests: their objectives' and fail rules' signals and filters, the giver, the region, the rewards; a quest
    // named by the quest effects of triggers is found above, as any id.)
    (p.quests ?? []).forEach((q, index) => {
        const ref: Ref = {kind: 'quest', index}, label = 'Quest ' + q.id;
        if (type === 'signal') {
            q.objectives.forEach(o => { if (o.signal === id) out.push({ref, where: `${label}: objective ${o.id} waits for it`}); });
            if ((q.fail ?? []).some(f => 'signal' in f && f.signal === id)) out.push({ref, where: `${label}: fails on it`});
        } else if (type === 'quest') {
            if (q.id === id) out.push({ref, where: label});
            else if (hasValue(q.objectives) || hasValue(q.fail) || hasValue(q.rewards) || hasValue(q.bonus)) out.push({ref, where: label});
        } else if (type === 'phase') {
            if (q.phases?.includes(id)) out.push({ref, where: `${label}: offered in it`});
        } else if (!['counter', 'variable', 'trigger'].includes(type)) {
            if (q.giver === id) out.push({ref, where: `${label}: gives it`});
            else if (q.join?.region === id) out.push({ref, where: `${label}: taken inside it`});
            else if (hasValue(q.objectives) || hasValue(q.fail) || hasValue(q.rewards) || hasValue(q.bonus)) out.push({ref, where: label});
        }
    });
    return out;
}

const rewriteTriggers = (p: Program, fn: (t: Trigger) => Trigger): Program => ({
    ...p, ...(p.triggers ? {triggers: p.triggers.map(fn)} : {}),
    phases: p.phases.map(ph => ph.triggers ? {...ph, triggers: ph.triggers.map(fn)} : ph),
});

/** A signal renamed everywhere: listeners, senders, timers and journal objectives. */
export function renameSignal(p: Program, from: string, to: string): Program {
    const out = rewriteTriggers(p, t => {
        const signal = t.on === SIGNAL && t.signal === from;
        const actions = mapActions(t.actions, a => (a.do === 'emit' || a.do === 'schedule' || a.do === 'cancel' || a.do === 'shift') && a.signal === from ? {...a, signal: to} : a);
        const next = mapTriggerConditions({...t, actions}, c => c.test === 'timer' && c.signal === from ? {...c, signal: to} : c);
        if (!signal && next.actions === t.actions && next.if === t.if) return t;
        return {...next, ...(signal ? {signal: to} : {})};
    });
    const quests = out.quests?.map(q => !q.objectives.some(o => o.signal === from) && !(q.fail ?? []).some(f => 'signal' in f && f.signal === from) ? q : {...q,
        objectives: q.objectives.map(o => o.signal === from ? {...o, signal: to} : o),
        ...(q.fail ? {fail: q.fail.map(f => 'signal' in f && f.signal === from ? {...f, signal: to} : f)} : {})});
    return {...out, ...(quests ? {quests} : {}), phases: out.phases.map(ph => !ph.missions?.some(m => m.objectives.some(o => o.signal === from)) ? ph : {...ph,
        missions: ph.missions!.map(m => ({...m, objectives: m.objectives.map(o => o.signal === from ? {...o, signal: to} : o)}))})};
}

/** A counter renamed everywhere it is read or written (of one scope), and in the rehearsal cases' expected counters. */
export function renameCounter(p: Program, scope: string, from: string, to: string): Program {
    const cond = (c: Condition): Condition => {
        if (c.test === 'all' || c.test === 'any') return {...c, conditions: c.conditions.map(cond)};
        if (c.test === 'not') return {...c, condition: cond(c.condition)};
        if (c.test === 'counter' && c.scope === scope && c.key === from) return {...c, key: to};
        return c;
    };
    const out = rewriteTriggers(p, t => {
        const reads = conditionsOf(t.if).some(c => c.test === 'counter' && c.scope === scope && c.key === from);
        const writes = t.actions.some(a => (a.do === 'set' || a.do === 'add') && a.scope === scope && a.key === from);
        if (!reads && !writes) return t;
        return {...t, ...(reads && t.if ? {if: cond(t.if)} : {}), actions: t.actions.map(a =>
            (a.do === 'set' || a.do === 'add') && a.scope === scope && a.key === from ? {...a, key: to} : a)};
    });
    const tests = out.tests.map(c => {
        const counters = c.expectCounters as Record<string, Record<string, unknown>> | undefined;
        if (!counters) return c;
        const rename = (o: Record<string, unknown>) => from in o ? Object.fromEntries(Object.entries(o).map(([k, v]) => [k === from ? to : k, v])) : o;
        if (scope === 'story' && counters.story && from in counters.story) return {...c, expectCounters: {...counters, story: rename(counters.story)}};
        if (scope === 'player' && counters.player) return {...c, expectCounters: {...counters,
            player: Object.fromEntries(Object.entries(counters.player).map(([w, o]) => [w, rename(o as Record<string, unknown>)]))}};
        return c;
    });
    return {...out, tests};
}

/** Version 2: a declared variable renamed everywhere (its declaration, every test and action that names it, the rehearsal
 *  cases' expected variables). */
export function renameVariable(p: Program, from: string, to: string): Program {
    const op = (o: Operand): Operand => {
        if (!o || typeof o !== 'object') return o;
        const x = o as Json;
        for (const k of ['var', 'count', 'since']) if (x[k] === from) return {...x, [k]: to} as Operand;
        for (const k of ['percent', 'among']) if (record(x[k])) { const s = share(x[k] as ShareSource); if (s !== x[k]) return {...x, [k]: s} as Operand; }
        for (const k of ['plus', 'minus', 'min', 'max', 'times', 'div']) if (Array.isArray(x[k])) {
            const pair = (x[k] as Operand[]).map(op);
            return pair.some((v, i) => v !== (x[k] as Operand[])[i]) ? {[k]: pair} as Operand : o;
        }
        return o;
    };
    const named = (x: Json) => {
        let next = x;
        for (const k of ['var', 'list']) if (next[k] === from) next = {...next, [k]: to};
        for (const k of ['left', 'right', 'value']) if (k in next) { const v = op(next[k] as Operand); if (v !== next[k]) next = {...next, [k]: v}; }
        if (record(next.where)) { const w = cond(next.where as Condition); if (w !== next.where) next = {...next, where: w}; }   // (A participation's test.)
        return next;
    };
    const one = (c: Condition) => ['compare', 'switch', 'in list', 'days since', 'participation'].includes(c.test) ? named(c as unknown as Json) as unknown as Condition : c;
    const cond = (c: Condition): Condition => mapCondition(c, one);
    const share = (s: ShareSource): ShareSource => 'list' in s ? (s.list === from ? {list: to} : s) : (cond(s.where) === s.where ? s : {where: cond(s.where)});
    // (Doc 86, R2: a phase's Readiness reads variables in its participation parts, its values and its tests.)
    const readyPart = (x: ReadyPartSpec): ReadyPartSpec => {
        if ('any' in x) { const any = x.any.map(readyPart); return any.some((y, i) => y !== x.any[i]) ? {...x, any} : x; }
        if ('participation' in x) { const s = share(x.participation); return s === x.participation ? x : {...x, participation: s}; }
        if ('value' in x) { const v = op(x.value), a = op(x.atLeast); return v === x.value && a === x.atLeast ? x : {...x, value: v, atLeast: a}; }
        if ('test' in x) { const t = cond(x.test); return t === x.test ? x : {...x, test: t}; }
        return x;
    };
    const renamed = rewriteTriggers(p, t => mapTriggerConditions({...t, actions: mapActions(t.actions, a => named(a as unknown as Json) as unknown as Action)}, one));
    const out = {...renamed, phases: renamed.phases.map(ph => {
        if (!ph.ready) return ph;
        const all = ph.ready.all.map(readyPart);
        return all.some((x, i) => x !== ph.ready!.all[i]) ? {...ph, ready: {...ph.ready, all}} : ph;
    })};
    const tests = out.tests.map(c => {
        const v = c.expectVariables as Record<string, Json> | undefined;
        if (!v) return c;
        const rename = (o: Json) => from in o ? Object.fromEntries(Object.entries(o).map(([k, x]) => [k === from ? to : k, x])) : o;
        return {...c, expectVariables: Object.fromEntries(Object.entries(v).map(([scope, o]) => [scope,
            scope === 'player' || scope === 'party' ? Object.fromEntries(Object.entries(o).map(([w, x]) => [w, rename(x as Json)])) : rename(o)]))};
    });
    return {...out, tests, variables: (out.variables ?? []).map(v => v.id === from ? {...v, id: to} : v)};
}

/** A trigger renamed, and the rehearsal cases that expect it by name. */
export function renameTrigger(p: Program, ref: Ref, to: string): Program {
    const t = triggerAt(p, ref);
    if (!t || ref.kind !== 'trigger' || t.id === to) return p;
    const from = t.id;
    let out = withTriggers(p, ref.phase, triggersOf(p, ref.phase).map((x, i) => i === ref.index ? {...x, id: to} : x));
    // (Version 2: Run, Enable, Disable and "has fired" name triggers; those naming this one follow it.)
    const phaseId = ref.phase === null ? null : p.phases[ref.phase].id;
    const sees = (scope: number | null) => ref.phase === null || scope === ref.phase;
    out = {...out, ...(out.triggers ? {triggers: out.triggers.map(x => renameRefs(x, from, to, phaseId, sees(null)))} : {}),
        phases: out.phases.map((ph, i) => ph.triggers ? {...ph, triggers: ph.triggers.map(x => renameRefs(x, from, to, phaseId, sees(i)))} : ph)};
    return {...out, tests: out.tests.map(c => c.expectTriggers?.includes(from) ? {...c, expectTriggers: c.expectTriggers.map(x => x === from ? to : x)} : c)};
}

function renameRefs(t: Trigger, from: string, to: string, phaseId: string | null, sees: boolean): Trigger {
    const actions = mapActions(t.actions, a => (a.do === 'run' || a.do === 'enable' || a.do === 'disable') && a.trigger === from
        && ('phase' in a && a.phase !== undefined ? a.phase === phaseId : sees) ? {...a, trigger: to} : a);
    return mapTriggerConditions(actions === t.actions ? t : {...t, actions}, c => c.test === 'fired' && c.trigger === from && sees ? {...c, trigger: to} : c);
}

// --------------------------------------------------------------------------------------------------------------- search

export interface Found {ref: Ref; text: string; label: string}
/** Every trigger, exit and phase whose id, folder, note, journal or sentences have the words (all of them). */
export function search(p: Program, cat: Catalogue, names: Names, query: string): Found[] {
    const words = query.toLowerCase().split(/\s+/).filter(Boolean);
    if (!words.length) return [];
    const hit = (text: string) => { const t = text.toLowerCase(); return words.every(w => t.includes(w)); };
    const out: Found[] = [];
    eachTrigger(p, (t, ref) => {
        const scope = ref.kind === 'trigger' && ref.phase !== null ? p.phases[ref.phase].id : 'Campaign';
        const sentence = [plain(eventParts(cat, t, names)), ...allConditions(t).map(c => plain(conditionParts(cat, c, names))),
            ...allActions(t.actions).map(({a}) => plain(actionParts(cat, a, names)))].join(' · ');
        const text = [t.id, t.folder ?? '', t.note ?? '', sentence, JSON.stringify(t)].join(' ');
        if (hit(text)) out.push({ref, label: scope + ' / ' + t.id, text: sentence});
    });
    p.phases.forEach((ph, phase) => {
        if (hit(ph.id + ' ' + ph.journal)) out.push({ref: {kind: 'phase', phase}, label: 'Phase ' + ph.id, text: ph.journal});
        ph.exits.forEach((x, index) => {
            const sentence = plain(exitParts(cat, x, names, ph.missions ?? []));
            if (hit(sentence + ' ' + JSON.stringify(x))) out.push({ref: {kind: 'exit', phase, index}, label: ph.id + ' / exit ' + (index + 1), text: sentence});
        });
    });
    (p.quests ?? []).forEach((q, index) => {
        const sentence = q.objectives.map(o => o.label + ' (' + plain(eventParts(cat, o, names)) + ')').join(' · ');
        if (hit([q.id, q.title, q.summary ?? '', q.offer?.say ?? '', sentence, JSON.stringify(q)].join(' ')))
            out.push({ref: {kind: 'quest', index}, label: 'Quest ' + q.id, text: q.title + ': ' + sentence});
    });
    return out;
}

// ------------------------------------------------------------------------------------------------------- undo and redo

/** The draft's history: each edit pushes the program before it; typing in one field coalesces into one step. */
export interface History {past: string[]; present: string; future: string[]; mark?: string}
export const LIMIT = 200;
export function historyOf(text: string): History { return {past: [], present: text, future: []}; }
export function commit(h: History, next: string, mark?: string): History {
    if (next === h.present) return h;
    if (mark && mark === h.mark) return {...h, present: next, future: []};
    return {past: [...h.past, h.present].slice(-LIMIT), present: next, future: [], mark};
}
export function undo(h: History): History {
    if (!h.past.length) return h;
    return {past: h.past.slice(0, -1), present: h.past[h.past.length - 1], future: [h.present, ...h.future]};
}
export function redo(h: History): History {
    if (!h.future.length) return h;
    return {past: [...h.past, h.present], present: h.future[0], future: h.future.slice(1)};
}

// --------------------------------------------------------------------------------------------------- problems and traces

export interface At {kind: string; phase?: number | null; phaseId?: string | null; index?: number; id?: string | null; part?: string}
/** The node of the tree a problem's place (story_program.problems' `at`) belongs to. */
export function refOfProblem(p: Program, at: At): Ref | null {
    const phase = typeof at.phase === 'number' && at.phase < p.phases.length ? at.phase : null;
    switch (at.kind) {
        case 'trigger': {
            const list = triggersOf(p, phase);
            const index = at.index ?? list.findIndex(t => t.id === at.id);
            return index >= 0 && index < list.length ? {kind: 'trigger', phase, index} : phase === null ? {kind: 'settings'} : {kind: 'phase', phase};
        }
        case 'exit': return phase !== null && at.index !== undefined && at.index < p.phases[phase].exits.length ? {kind: 'exit', phase, index: at.index} : phase !== null ? {kind: 'phase', phase} : null;
        case 'exits': case 'phase': return phase !== null ? {kind: 'phase', phase} : {kind: 'settings'};
        case 'effect': return phase !== null ? {kind: 'entry', phase} : null;
        case 'mission': case 'missions': return phase !== null ? {kind: 'missions', phase} : null;
        case 'region': case 'regions': return {kind: 'regions'};
        case 'case': case 'tests': return {kind: 'tests'};
        case 'scene': case 'scenes': return {kind: 'scenes'};
        // (Phase 6: a quest by its index or id, else the list.)
        case 'quest': case 'quests': {
            const list = p.quests ?? [];
            const index = at.index !== undefined && at.index < list.length ? at.index : list.findIndex(q => q.id === at.id);
            return index >= 0 ? {kind: 'quest', index} : {kind: 'quests'};
        }
        case 'audience': return {kind: 'settings'};
        case 'source': return null;
        default: return {kind: 'settings'};
    }
}
/** Which action or condition of a trigger a problem's `part` names ("actions[2]", "actions[2].then[0]", "if", "head"). */
export function partOf(at: At): {action?: number; path?: string; conditions?: boolean; head?: boolean} {
    const m = /^actions\[(\d+)\]/.exec(at.part ?? '');
    if (m) return {action: Number(m[1]), path: (at.part ?? '').replace(/\.if$/, '')};
    if (at.part === 'if' || at.part?.startsWith('if')) return {conditions: true};
    return {head: true};
}

/** How often each trigger fired in a rehearsal's traces, keyed "campaign/<id>" or "<phase>/<id>". */
export function traceCounts(tests: {triggers?: {scope?: string; phase: string; trigger: string}[]}[]): Record<string, number> {
    const out: Record<string, number> = {};
    for (const t of tests) for (const f of t.triggers ?? []) {
        const key = (f.scope === 'campaign' ? 'campaign' : f.phase) + '/' + f.trigger;
        out[key] = (out[key] ?? 0) + 1;
    }
    return out;
}
export const traceKey = (p: Program, ref: Ref) => {
    const t = triggerAt(p, ref);
    return t && ref.kind === 'trigger' ? (ref.phase === null ? 'campaign' : p.phases[ref.phase].id) + '/' + t.id : '';
};

// ------------------------------------------------------------------------------------------------- new things, by kind

/** A new effect of a kind, each parameter at its first value (a region: the first of the story). */
export function newEffect(cat: Catalogue, kind: string, p: Program): Effect {
    const e: Effect = {kind};
    for (const param of itemOf(cat.effects, kind)?.params ?? []) {
        // (Phase 6: an optional parameter starts left out.)
        if (param.optional) continue;
        if (param.type === 'region') e[param.key] = p.regions?.[0]?.id ?? '';
        else if (param.type === 'choice') e[param.key] = param.values![0];
        else if (param.type === 'int') e[param.key] = param.min! > 1 ? param.min! : Math.max(param.min!, param.nonzero ? 1 : param.key === 'x' || param.key === 'y' ? 0 : 1);
        else if (param.type === 'wolf') e[param.key] = '$actor';
        else if (param.type === 'scene') e[param.key] = String(p.scenes?.[0]?.id ?? '');
        else if (param.type === 'cue') e[param.key] = kind === 'scene.play' ? String(p.scenes?.[0]?.id ?? 'presentation') : 'cue';
        // (A quest: the first of the story's, its first objective.)
        else if (param.type === 'quest') e[param.key] = p.quests?.[0]?.id ?? '';
        else if (param.type === 'objective') e[param.key] = p.quests?.find(q => q.id === e.quest)?.objectives[0]?.id ?? '';
        else if (param.type === 'offer') e[param.key] = 'choice';
        // (Phase 9: show a tracker, the first shown variable it may be; a stranger's id of the story's own.)
        else if (param.type === 'bool') e[param.key] = true;
        else if (param.type === 'variable') e[param.key] = (p.variables ?? []).find(v => v.show && (param.of ?? []).includes(v.type))?.id ?? '';
        else if (param.type === 'visitor') e[param.key] = 'stranger-' + ((visitorsOf(p).length) + 1);
        else e[param.key] = '';
    }
    if (kind === 'scene.play' && p.scenes?.[0]?.visibility === 'public') e.player = 'world';
    if (kind === 'weather.set') e.place = '';   // (Phase 9: a place, or else a town's streets: one of them, to pick.)
    return e;
}
/** The first declared variable of some types (version 2), or a name to start with. */
const firstVar = (p: Program, types: string[], fallback = 'progress') => (p.variables ?? []).find(v => types.includes(v.type))?.id ?? fallback;
const NUMBERS = ['number'], SWITCHES = ['switch'], LISTS = Object.keys(LIST_TYPES), TIMERS = ['timer'];
export function newAction(cat: Catalogue, kind: string, p: Program, trigger?: string, scope: number | null = null): Action {
    if (isV2(p)) switch (kind) {
        case 'set var': { const v = (p.variables ?? []).find(x => !LISTS.includes(x.type) && x.type !== 'timer');
            return {do: 'set', var: v?.id ?? 'progress', value: v?.type === 'switch' ? true : v?.type === 'number' || !v ? 1 : ''}; }
        case 'add var': return {do: 'add', var: firstVar(p, NUMBERS), value: 1};
        case 'subtract': case 'multiply': return {do: kind, var: firstVar(p, NUMBERS), value: kind === 'multiply' ? 2 : 1};
        case 'clamp': return {do: 'clamp', var: firstVar(p, NUMBERS), min: 0, max: 10};
        case 'turn on': case 'turn off': case 'toggle': return {do: kind, var: firstVar(p, SWITCHES, 'done')};
        case 'remember': return {do: 'remember', var: firstVar(p, ['wolf', 'resident', 'text', 'item', 'place', 'faction', 'region'], 'who'), from: 'actor'};
        case 'list add': case 'list remove': return {do: kind, var: firstVar(p, LISTS, 'names'), value: {field: 'actor'}};
        case 'list clear': return {do: 'list clear', var: firstVar(p, LISTS, 'names')};
        case 'pick': {
            const list = (p.variables ?? []).find(v => LISTS.includes(v.type));
            const element = kindOfType(LIST_TYPES[list?.type ?? 'texts']);
            const into = (p.variables ?? []).find(v => !LISTS.includes(v.type) && v.type !== 'timer' && kindOfType(v.type) === element);
            return {do: 'pick', list: list?.id ?? 'names', var: into?.id ?? 'chosen'};
        }
        case 'start timer': case 'stop timer': return {do: kind, var: firstVar(p, TIMERS, 'since')};
        case 'rank': {
            // (Doc 86, C3a: by a number each wolf keeps, into a list of wolves of the story or the world.)
            const by = (p.variables ?? []).find(v => v.type === 'number' && v.scope === 'player' && !v.mirror)?.id ?? 'progress';
            const into = (p.variables ?? []).find(v => v.type === 'wolves' && (v.scope === 'story' || v.scope === 'world'))?.id ?? 'best';
            return {do: 'rank', var: by, top: 3, into};
        }
        case 'shift': return {do: 'shift', signal: signalsOf(p)[0] ?? 'deadline', days: 1};
        case 'if': return {do: 'if', if: newCondition('compare', p, null), then: [{do: 'note', text: 'Then…'}]};
        case 'wait': return {do: 'wait', days: 1};
        case 'each': return {do: 'each', over: 'participants', actions: [{do: 'note', text: 'For each wolf…'}]};
        case 'random': return {do: 'random', choices: [{weight: 1, actions: [{do: 'note', text: 'One way…'}]}, {weight: 1, actions: [{do: 'note', text: 'Or another…'}]}]};
        case 'run': case 'enable': case 'disable': {
            // (One this trigger can see: of its own phase, or the campaign's.)
            const other = [...(scope === null ? [] : triggersOf(p, scope)), ...(p.triggers ?? [])].find(t => t.id !== trigger)?.id ?? '';
            return kind === 'run' ? {do: 'run', trigger: other} : {do: kind, trigger: other};
        }
        case 'stop': return {do: 'stop'};
        case 'note': return {do: 'note', text: 'A note for the DM.'};
        case 'finish': return {do: 'finish', outcome: 'success', text: 'The story is told.'};
    }
    switch (kind) {
        case 'set': case 'add': return {do: kind, scope: 'story', key: countersOf(p)[0]?.key ?? 'progress', value: 1};
        case 'emit': return {do: 'emit', signal: signalsOf(p)[0] ?? 'next'};
        case 'schedule': return {do: 'schedule', signal: 'deadline', days: 1};
        case 'cancel': return {do: 'cancel', signal: signalsOf(p)[0] ?? 'deadline'};
        case 'enter': return {do: 'enter', phase: p.phases.at(-1)?.id ?? ''};
        default: return {do: 'effect', effect: newEffect(cat, kind.startsWith('effect:') ? kind.slice(7) : 'deed.award', p)};
    }
}
/** Doc 85, Phase 8: a condition over the world's facts as of the event (where a wolf is, what they carry, the hour, the
 *  weather, standing...), or over the story's quests. */
export function isWorldCondition(item: CatItem | undefined): boolean {
    return !!item && (!!item.facts || item.id === 'quest is' || item.id === 'objective is');
}
/** A new world or quest condition: the catalogue's defaults, the story's first region and quest; what only the world can
 *  name (a place, an item, a faction, a resident) left to be picked (its slot shows red until it is). */
export function newWorldCondition(item: CatItem, p: Program): Condition {
    const c: Record<string, unknown> = {test: item.id};
    for (const param of item.params ?? []) {
        if (['who', 'of', 'where'].includes(param.key)) continue;   // (The acting wolf unless another is chosen.)
        if (param.type === 'region') c[param.key] = p.regions?.[0]?.id ?? '';
        else if (param.type === 'quest') c[param.key] = p.quests?.[0]?.id ?? '';
        else if (param.type === 'objective') c[param.key] = p.quests?.find(q => q.id === c.quest)?.objectives[0]?.id ?? '';
        else if (param.default !== undefined) c[param.key] = param.default;
        else if (param.type === 'int') c[param.key] = param.min ?? 0;
        else c[param.key] = '';
    }
    return c as unknown as Condition;
}
export function newCondition(kind: string, p: Program, phase: number | null, cat?: Catalogue): Condition {
    const world = cat ? itemOf(cat.conditions, kind) : undefined;
    if (world && isWorldCondition(world)) return newWorldCondition(world, p);
    if (isV2(p)) switch (kind) {
        case 'all': case 'any': return {test: kind, conditions: [newCondition('compare', p, phase)]};
        case 'compare': { const v = (p.variables ?? []).find(x => !LISTS.includes(x.type) && x.type !== 'timer');
            return {test: 'compare', left: {var: v?.id ?? 'progress'}, op: v && v.type !== 'number' ? '=' : '>=', right: v?.type === 'switch' ? true : v && v.type !== 'number' ? '' : 1}; }
        case 'switch': return {test: 'switch', var: firstVar(p, SWITCHES, 'done'), is: true};
        case 'in list': return {test: 'in list', list: firstVar(p, LISTS, 'names'), value: {field: 'actor'}};
        case 'phase is': return {test: 'phase is', phase: p.phases[phase ?? 0]?.id ?? p.start};
        case 'visit is': return {test: 'visit is', op: '>=', value: 2};
        case 'timer': return {test: 'timer', signal: signalsOf(p)[0] ?? 'deadline'};
        case 'fired': return {test: 'fired', trigger: triggersOf(p, phase)[0]?.id ?? (p.triggers ?? [])[0]?.id ?? '', op: '>=', value: 1};
        case 'chance': return {test: 'chance', percent: 50};
        case 'days since': return {test: 'days since', from: 'phase', op: '>=', days: 1};
        case 'participation': { const list = (p.variables ?? []).find(v => v.type === 'wolves' && ['story', 'world'].includes(v.scope))?.id ?? 'did';
            return {test: 'participation', source: 'list', list, of: 'active', percent: 35, floor: 3}; }
        case 'counter': return newCondition('compare', p, phase);
    }
    switch (kind) {
        case 'all': case 'any': return {test: kind, conditions: [{test: 'counter', scope: 'story', key: countersOf(p)[0]?.key ?? 'progress', op: '>=', value: 1}]};
        case 'not': return {test: 'not', condition: {test: 'field', field: 'item', op: '=', value: ''}};
        case 'region': return {test: 'region', region: p.regions?.[0]?.id ?? ''};
        case 'mission': return {test: 'mission', id: phase === null ? '' : p.phases[phase]?.missions?.[0]?.id ?? ''};
        case 'field': return {test: 'field', field: 'actor', op: '=', value: ''};
        default: return {test: 'counter', scope: 'story', key: countersOf(p)[0]?.key ?? 'progress', op: '>=', value: 1};
    }
}
/** An event chosen for a trigger: its filters cleared, and what the event needs (a delay, a signal, the scene rules). */
export function withEvent(t: Trigger, on: string): Trigger {
    const {signal, afterDays, match, ...rest} = t;
    void signal; void afterDays; void match;
    const next: Trigger = {...rest, on};
    if (on === SIGNAL) next.signal = 'next';
    if (on === 'clock') { next.afterDays = 1; next.once = 'visit'; next.audience = 'world'; }
    if (on.startsWith('scene ') && ['arrived', 'spoken', 'posed', 'blocked'].some(w => on.endsWith(w))) {
        next.audience = 'world';
        if (next.once === 'player' || next.once === 'member') next.once = 'each';
    }
    return next;
}

// --------------------------------------------------------------------------------------------------------- the forms' gate

const record = (v: unknown): v is Json => typeof v === 'object' && v !== null && !Array.isArray(v);
const V2_TESTS = ['compare', 'switch', 'in list', 'phase is', 'visit is', 'timer', 'fired', 'chance', 'days since',
    // (Phase 8: over the world's facts as of the event, and the story's quests.)
    'wolf here', 'wolf in region', 'wolf in place', 'wolf in town', 'wolf indoors', 'count in region', 'carries', 'coins', 'standing', 'reputation',
    'wolf is', 'party size', 'in party with', 'resident is', 'resident in place', 'resident in region', 'resident has', 'hour between', 'calendar is',
    'weather is', 'relation is', 'quest is', 'objective is', 'participation'];
function editableCondition(v: unknown, depth = 0): boolean {
    if (!record(v) || depth > 5) return false;
    if (v.test === 'all' || v.test === 'any') return Array.isArray(v.conditions) && v.conditions.every(c => editableCondition(c, depth + 1));
    if (v.test === 'not') return editableCondition(v.condition, depth + 1);
    if (v.test === 'region') return typeof v.region === 'string';
    if (v.test === 'mission') return typeof v.id === 'string';
    if (V2_TESTS.includes(String(v.test))) return true;   // (Version 2: shown as its sentence; the check says what is wrong.)
    // (Doc 86, C12b: the field's "is one of" takes a list of texts.)
    return (v.test === 'counter' || v.test === 'field') && (['string', 'number'].includes(typeof v.value)
        || (v.test === 'field' && v.op === 'in' && Array.isArray(v.value) && v.value.every(x => typeof x === 'string')));
}
const ACTIONS = ['enter', 'emit', 'set', 'add', 'schedule', 'cancel', 'subtract', 'multiply', 'clamp', 'turn on', 'turn off', 'toggle', 'remember',
    'list add', 'list remove', 'list clear', 'pick', 'start timer', 'stop timer', 'shift', 'wait', 'run', 'enable', 'disable', 'stop', 'note', 'finish',
    'rank'];
function editableActions(v: unknown, depth = 0): boolean {
    return Array.isArray(v) && depth <= 4 && v.every(a => record(a) && (a.do === 'effect' ? record(a.effect) && typeof a.effect.kind === 'string'
        : a.do === 'if' ? editableCondition(a.if) && editableActions(a.then, depth + 1) && (a.else === undefined || editableActions(a.else, depth + 1))
            && (a.elif === undefined || (Array.isArray(a.elif) && a.elif.every(b => record(b) && editableCondition(b.if) && editableActions(b.then, depth + 1))))
        : a.do === 'each' ? editableActions(a.actions, depth + 1)
        : a.do === 'random' ? Array.isArray(a.choices) && a.choices.every(c => record(c) && typeof c.weight === 'number' && editableActions(c.actions, depth + 1))
        : ACTIONS.includes(String(a.do))));
}
export function editableTrigger(v: unknown): boolean {
    return record(v) && typeof v.id === 'string' && typeof v.on === 'string' && (v.if === undefined || editableCondition(v.if))
        && (v.match === undefined || record(v.match)) && (v.enabled === undefined || typeof v.enabled === 'boolean') && editableActions(v.actions);
}
/** A quest the Quest editor can show: its id, title and objectives, and lists where lists go (Phase 6). */
export function editableQuest(v: unknown): v is Quest {
    const list = (x: unknown, each: (y: Json) => boolean) => x === undefined || (Array.isArray(x) && x.every(y => record(y) && each(y)));
    return record(v) && typeof v.id === 'string' && typeof v.title === 'string' && Array.isArray(v.objectives) && v.objectives.length > 0
        && v.objectives.every(o => record(o) && typeof o.id === 'string' && typeof o.label === 'string' && typeof o.on === 'string'
            && (o.match === undefined || record(o.match)) && (o.after === undefined || Array.isArray(o.after)))
        && list(v.fail, f => typeof f.on === 'string' || typeof f.afterDays === 'number') && list(v.rewards, e => typeof e.kind === 'string')
        && list(v.bonus, e => typeof e.kind === 'string') && (v.phases === undefined || Array.isArray(v.phases))
        && (v.offer === undefined || record(v.offer)) && (v.join === undefined || record(v.join));
}
/** Whether the workbench can show a program as it is. A program it cannot is still kept, and opened in Source (risk 6). */
export function editable(v: unknown): v is Program {
    if (!record(v)) return false;
    const p = v as unknown as Program;
    const regionOk = (r: unknown) => record(r) && typeof r.id === 'string' && typeof r.cell === 'string' && ['x1', 'y1', 'x2', 'y2'].every(k => Number.isInteger(r[k]));
    return typeof p.title === 'string' && typeof p.id === 'string' && typeof p.start === 'string' && record(p.audience) && typeof p.audience.kind === 'string'
        && (p.variables === undefined || (Array.isArray(p.variables) && p.variables.every(d => record(d) && typeof d.id === 'string' && typeof d.type === 'string' && typeof d.scope === 'string')))
        && (p.regions === undefined || (Array.isArray(p.regions) && p.regions.every(regionOk)))
        && (p.triggers === undefined || (Array.isArray(p.triggers) && p.triggers.every(editableTrigger)))
        && (p.scenes === undefined || (Array.isArray(p.scenes) && p.scenes.every(s => record(s) && typeof s.title === 'string' && Array.isArray(s.beats))))
        && (p.quests === undefined || (Array.isArray(p.quests) && p.quests.every(editableQuest)))
        && Array.isArray(p.tests) && p.tests.every(c => record(c) && typeof c.name === 'string' && Array.isArray(c.cohort) && Array.isArray(c.expect)
            && Array.isArray(c.events) && c.events.every(f => record(f) && (typeof f.kind === 'string' || isStep(f))))
        && Array.isArray(p.phases) && p.phases.length > 0 && p.phases.every(ph => record(ph) && typeof ph.id === 'string' && typeof ph.journal === 'string'
            && (ph.guestVoices === undefined || Array.isArray(ph.guestVoices))
            && (ph.triggers === undefined || (Array.isArray(ph.triggers) && ph.triggers.every(editableTrigger)))
            && (ph.missions === undefined || (Array.isArray(ph.missions) && ph.missions.every(m => record(m) && typeof m.id === 'string' && typeof m.title === 'string'
                && ['individual', 'group', 'either'].includes(String(m.mode)) && Array.isArray(m.objectives) && m.objectives.every(o => record(o) && typeof o.label === 'string' && typeof o.on === 'string'))))
            && Array.isArray(ph.exits) && ph.exits.every(x => record(x) && typeof x.on === 'string' && typeof x.to === 'string' && (x.missions === undefined || Array.isArray(x.missions)))
            && Array.isArray(ph.effects) && ph.effects.every(e => record(e) && typeof e.kind === 'string'));
}

export const pretty = (v: unknown) => JSON.stringify(v, null, 2);

/** The console's words from the catalogue (lib/campaigns.ts's Wording): a judged condition's expected value in the sentence. */
export function wordingOf(cat: Catalogue, flat: Record<string, string>) {
    const names: Names = new Proxy({}, {get: () => flat}) as Names;
    return {
        event: (rule: {on: string; signal?: string; match?: Record<string, string>; afterDays?: number}) => plain(eventParts(cat, rule, names)),
        condition: (c: {test: string; field?: string; op?: string; expected?: unknown; key?: string; scope?: string; region?: string; id?: string}) =>
            plain(conditionParts(cat, {...c, value: c.expected} as unknown as Condition, names)),
    };
}

// ------------------------------------------------------------------------------------------------- quests (Phase 6)

/** The quest effects' kinds that name a quest (and an objective): renaming one follows them. */
export const QUEST_EFFECTS = ['quest.offer', 'quest.start', 'quest.complete', 'quest.fail', 'objective.complete', 'objective.fail', 'objective.reveal',
    'objective.progress'];
/** The events a quest's objectives may wait for: the catalogue's mission events, and a story signal. */
export function objectiveEvents(cat: Catalogue): CatEvent[] {
    return cat.events.filter(e => e.mission || e.id === SIGNAL);
}
/** Who pays a coin reward (doc 85, question 4: never minted): each town's treasury, works, watch and church, from the refs. */
export function purseNames(refs: Refs | null): Record<string, string> {
    const out: Record<string, string> = {};
    for (const t of refs?.towns ?? []) out['town:' + t.id] = `${t.name}'s treasury`;
    // (Doc 86, SQ2: a business's till pays as till:job:<founder>; no world has a town's works, watch or church purse.)
    for (const t of refs?.tills ?? []) out[t.id] = t.name;
    return out;
}
export const PURSE = /^(town:[A-Za-z0-9_.-]{1,60}(:(works|watch|church))?|till:(job:)?[A-Za-z0-9_.-]{1,70}|chapter:[A-Za-z0-9_.:-]{1,70})$/;
const ID = /^[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}$/;
export const isId = (v: string) => ID.test(v);

/** A new quest of a program: one objective (talk to someone), offered by nobody yet. */
export function newQuest(p: Program): Quest {
    const taken = new Set((p.quests ?? []).map(q => q.id));
    let n = (p.quests?.length ?? 0) + 1;
    while (taken.has('quest-' + n)) n++;
    return {id: 'quest-' + n, title: 'A new quest', scope: 'player', objectives: [{id: 'talk', label: 'Speak with them.', on: 'conversation'}]};
}
export function withQuests(p: Program, quests: Quest[]): Program {
    if (!quests.length) { const {quests: _q, ...rest} = p; void _q; return rest; }
    return {...p, quests};
}
/** Every effect of the program (triggers' actions, nested ones too, phases' entry effects, quests' rewards) passed through `fn`. */
function mapEffects(p: Program, fn: (e: Effect) => Effect): Program {
    const actions = (list: Action[]) => mapActions(list, a => {
        if (a.do !== 'effect') return a;
        const e = fn(a.effect);
        return e === a.effect ? a : {...a, effect: e};
    });
    const trig = (t: Trigger) => { const next = actions(t.actions); return next === t.actions ? t : {...t, actions: next}; };
    const list = (l: Effect[] | undefined) => { if (!l) return l; const next = l.map(fn); return next.some((e, i) => e !== l[i]) ? next : l; };
    return {...p, ...(p.triggers ? {triggers: p.triggers.map(trig)} : {}),
        phases: p.phases.map(ph => {
            const effects = list(ph.effects)!;
            return {...ph, effects, ...(ph.triggers ? {triggers: ph.triggers.map(trig)} : {})};
        }),
        ...(p.quests ? {quests: p.quests.map(q => ({...q, ...(q.rewards ? {rewards: list(q.rewards)} : {}), ...(q.bonus ? {bonus: list(q.bonus)} : {})}))} : {})};
}
const QUEST_FACTS = ['quest accepted', 'quest declined', 'quest handed in', 'quest abandoned', 'quest started', 'quest completed', 'quest failed', 'objective completed'];
/** A quest renamed everywhere: its id, the quest effects naming it, objectives and fail rules waiting for its events, and the
 *  rehearsal cases (their facts and expectQuests). */
export function renameQuest(p: Program, from: string, to: string): Program {
    if (from === to) return p;
    let out = mapEffects(p, e => QUEST_EFFECTS.includes(e.kind) && e.quest === from ? {...e, quest: to} : e);
    const rule = <T extends {on?: string; match?: Record<string, string>}>(o: T): T =>
        o.on && QUEST_FACTS.includes(o.on) && o.match?.detail === from ? {...o, match: {...o.match, detail: to}} : o;
    out = rewriteTriggers(out, t => rule(t));
    out = {...out, quests: (out.quests ?? []).map(q => ({...(q.id === from ? {...q, id: to} : q), objectives: q.objectives.map(rule),
        ...(q.fail ? {fail: q.fail.map(f => 'on' in f ? rule(f) : f)} : {})}))};
    out = {...out, tests: out.tests.map(c => ({...c, events: c.events.map(f => QUEST_FACTS.includes(f.kind) && f.detail === from ? {...f, detail: to} : f),
        ...(c.expectQuests && from in c.expectQuests ? {expectQuests: Object.fromEntries(Object.entries(c.expectQuests).map(([k, v]) => [k === from ? to : k, v]))} : {})}))};
    if (!p.quests) { const {quests: _q, ...rest} = out; void _q; return rest; }
    return out;
}
/** An objective renamed: its id, the others' `after` naming it, and the objective effects naming it for its quest. */
export function renameObjective(p: Program, quest: string, from: string, to: string): Program {
    if (from === to) return p;
    const out = mapEffects(p, e => QUEST_EFFECTS.includes(e.kind) && e.quest === quest && e.objective === from ? {...e, objective: to} : e);
    return {...out, quests: (out.quests ?? []).map(q => q.id !== quest ? q : {...q, objectives: q.objectives.map(o => ({...(o.id === from ? {...o, id: to} : o),
        ...(o.after ? {after: o.after.map(a => a === from ? to : a)} : {})}))})};
}

/** What the "New small quest" flow asks (doc 85, 4.4): who gives it, what to do, the reward, the offer's words. */
export interface SmallQuestInput {
    title: string; summary: string; giver: string; objectives: Omit<QuestObjective, 'id'>[];
    reward: {kind: 'deed'; detail: string} | {kind: 'coins'; coins: number; purse: string; reason: string} | {kind: 'item'; item: string; quantity: number}
        | {kind: 'standing'; faction: string; points: number; reason: string} | {kind: 'none'};
    offer: QuestOffer; repeat?: Quest['repeat'];
}
export const slug = (text: string, fallback = 'quest') => text.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '').slice(0, 40) || fallback;
export const REHEARSAL_WOLF = 'player-test';
/** A small quest as a whole program: version 2, an open audience (anyone who asks may take it), one phase that stays open,
 *  the quest with its giver, offer and hand-in, and a rehearsal case: a wolf takes it, does each objective and hands it in. */
export function smallQuestProgram(input: SmallQuestInput): Program {
    const id = slug(input.title);
    const objectives: QuestObjective[] = input.objectives.map((o, i) => ({id: 'step-' + (i + 1), ...o, ...(i > 0 && !o.optional ? {after: ['step-' + i]} : {})}));
    // (Each step after the one before: the order the wolf is told. The author can free them in the Quest editor.)
    for (const o of objectives) if (o.after && objectives.find(x => x.id === o.after![0])?.optional) delete o.after;
    const r = input.reward;
    const rewards: Effect[] = r.kind === 'deed' ? [{kind: 'deed.award', player: '$actor', detail: r.detail}]
        : r.kind === 'coins' ? [{kind: 'character.grant', player: '$actor', coins: r.coins, purse: r.purse, reason: r.reason}]
        : r.kind === 'item' ? [{kind: 'item.give', player: '$actor', item: r.item, quantity: r.quantity}]
        : r.kind === 'standing' ? [{kind: 'standing.adjust', player: '$actor', faction: r.faction, points: r.points, reason: r.reason}] : [];
    const offer = Object.fromEntries(Object.entries(input.offer).filter(([, v]) => typeof v === 'string' && v.trim())) as unknown as QuestOffer;
    const quest: Quest = {id, title: input.title, ...(input.summary ? {summary: input.summary} : {}), scope: 'player', repeat: input.repeat ?? 'once',
        giver: input.giver, offer: {...offer, say: offer.say || 'There is something you could do for me.'}, handIn: true, objectives,
        ...(rewards.length ? {rewards} : {})};
    const wolf = REHEARSAL_WOLF;
    const events: Fact[] = [{id: 'take', kind: 'quest accepted', day: 0, actor: wolf, item: input.giver, detail: id}];
    objectives.filter(o => !o.optional).forEach((o, i) => {
        if (o.on === SIGNAL) return;   // (A signal is the story's own: a case cannot state one.)
        const fact: Fact = {id: o.id, kind: o.on, day: 0, actor: wolf, ...(o.match ?? {})};
        const goal = typeof o.goal === 'number' ? o.goal : 1;
        if (o.measure === 'quantity') fact.quantity = goal;
        else if (o.measure === 'coins') fact.coins = goal;
        for (let k = 0; k < (o.measure && o.measure !== 'count' ? 1 : goal); k++) events.push({...fact, id: goal > 1 && (!o.measure || o.measure === 'count') ? `${o.id}-${k + 1}` : o.id, day: 0.01 * (i + 1)});
    });
    events.push({id: 'hand-in', kind: 'quest handed in', day: 0.5, actor: wolf, item: input.giver, detail: id});
    return {format: 'ratw-story-program', version: 2, id, title: input.title, audience: {kind: 'open'}, start: 'open',
        phases: [{id: 'open', journal: input.summary || input.title, effects: [], exits: []}], quests: [quest],
        tests: [{name: 'A wolf takes it, does it and hands it in', cohort: [], events, expect: ['open'], expectQuests: {[id]: {[wolf]: 'done'}}}]};
}

/** One of doc 58's personal-quest templates (Data/Storylines/templates.json), as the small quest's starting point. */
export interface StorylineTemplate {id: string; title: string; source?: string; steps: {title: string; text: string; objectives: {kind: string; target?: string; line: string}[]}[]}
const words = (t: string) => t.replace(/\[\[\{?([^}\]]+)\}?\]\]/g, (_, n) => n === 'tie' ? 'them' : n).replace(/\{([^}]+)\}/g, (_, n) => n).trim();
/** A template's title, summary and objectives, as far as they map: talking to someone is a conversation (with the giver when it
 *  names the template's resident), visiting a place is entering it; being told, scenes and contracts have no event of their own. */
export function fromTemplate(t: StorylineTemplate, giver: string): {title: string; summary: string; objectives: Omit<QuestObjective, 'id'>[]} {
    const objectives: Omit<QuestObjective, 'id'>[] = [];
    for (const s of t.steps) for (const o of s.objectives) {
        if (o.kind === 'talk') objectives.push({label: words(o.line).slice(0, 180), on: 'conversation', ...(giver ? {match: {target: giver}} : {})});
        else if (o.kind === 'place') objectives.push({label: words(o.line).slice(0, 180), on: 'enter place'});
    }
    return {title: words(t.title).slice(0, 120), summary: words(t.steps[0]?.text ?? '').slice(0, 300), objectives: objectives.slice(0, 12)};
}

/** One of the 100 library events (Data/Storylines/event-library.json) as a small quest's starting point (doc 85, Phase 10): its
 *  objectives are the steps (the mission events and filters it names), its giver the resident its first step talks to or gives
 *  to, and its title. */
export function fromLibraryEntry(e: {title: string; objectives: {label: string; on: string; match?: Record<string, string>; goal?: number; measure?: string}[];
    opening?: {journal: string}}): {title: string; summary: string; giver: string; objectives: Omit<QuestObjective, 'id'>[]} {
    const objectives = e.objectives.slice(0, 12).map(o => ({label: o.label.slice(0, 180), on: o.on, ...(o.match ? {match: {...o.match}} : {}),
        ...(o.goal && o.goal > 1 ? {goal: o.goal} : {}), ...(o.measure && o.measure !== 'count' ? {measure: o.measure} : {})}) as Omit<QuestObjective, 'id'>);
    const first = e.objectives.find(o => o.match?.target && ['conversation', 'gift', 'gift again'].includes(o.on));
    return {title: e.title.slice(0, 120), summary: (e.opening?.journal ?? '').slice(0, 300), giver: first?.match?.target ?? '', objectives};
}
