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
}
export interface Field {type: string; label: string; any?: string}
export interface CatEvent {
    id: string; group: string; title: string; sentence: string; help: string; fields: Record<string, Field>; tile: boolean;
    mission?: boolean; measures?: string[]; special?: string; exitOnly?: boolean; notes?: string;
}
export interface CatItem {id: string; title: string; sentence: string; help: string; group?: string | boolean; params?: Param[];
    phaseOnly?: boolean; entryOnly?: boolean; last?: boolean; actsOnWolves?: boolean; needsBuilder?: boolean}
export interface Catalogue {
    groups: {id: string; label: string}[]; actionGroups: {id: string; label: string}[]; types: Record<string, string>;
    trigger: {once: Record<string, string>; audience: Record<string, string>; limits: Record<string, number>};
    events: CatEvent[]; conditions: CatItem[]; actions: CatItem[]; effects: CatItem[];
}

// --------------------------------------------------------------------------------------------------- the program's shape

export type Condition =
    | {test: 'all' | 'any'; conditions: Condition[]}
    | {test: 'not'; condition: Condition}
    | {test: 'mission'; id: string}
    | {test: 'region'; region: string}
    | {test: 'field'; field: string; op: string; value: string | number}
    | {test: 'counter'; scope: string; key: string; op: string; value: number};
export type Effect = Record<string, string | number> & {kind: string};
export type Action =
    | {do: 'emit'; signal: string}
    | {do: 'schedule'; signal: string; days: number; repeat?: boolean; scope?: string; clock?: string; owner?: string}
    | {do: 'cancel'; signal: string; owner?: string}
    | {do: 'enter'; phase: string}
    | {do: 'effect'; effect: Effect}
    | {do: 'add' | 'set'; scope: string; key: string; value: string | number};
export interface Trigger {
    id: string; on: string; signal?: string; match?: Record<string, string>; if?: Condition; audience?: string; once?: string;
    afterDays?: number; actions: Action[]; folder?: string; note?: string;
}
export interface Exit {
    on: string; to: string; match?: Record<string, string>; afterDays?: number; percent?: number; players?: number; participant?: boolean;
    missions?: string[]; minCoins?: number; minQuantity?: number; prerequisites?: unknown[];
}
export interface Objective {label: string; on: string; signal?: string; sameAsFirst?: string; match?: Record<string, string>;
    measure?: 'count' | 'quantity' | 'coins'; goal?: number}
export interface Mission {id: string; title: string; mode: 'individual' | 'group' | 'either'; objectives: Objective[];
    contributors?: number; players?: number; percent?: number}
export interface Phase {id: string; journal: string; effects: Effect[]; exits: Exit[]; triggers?: Trigger[]; missions?: Mission[];
    guestVoices?: string[]; campaignRules?: boolean}
/** One part of a library entry (tools/story_library.components): an opening, an objective, a participation rule, an outcome. */
export interface Component {role: string; label: string; trigger?: Trigger; objective?: Objective; gate?: Pick<Mission, 'mode' | 'percent' | 'contributors'>; action?: Action}
export interface Region {id: string; cell: string; x1: number; y1: number; x2: number; y2: number}
export interface Fact {id: string; kind: string; day: number; elapsedDays?: number; actor?: string; target?: string; item?: string;
    cell?: string; x?: number; y?: number; detail?: string; quantity?: number; coins?: number}
export interface Case {name: string; cohort: string[]; events: Fact[]; expect: string[]; expectEffects?: {phase: string; [key: string]: unknown}[];
    expectMissionVisits?: {phase: string; visit: number; credits: Record<string, string[]>}[]; expectTriggers?: string[]; expectCounters?: Json; expectMissions?: Json}
export interface Program {
    format?: string; version?: number; id: string; title: string; audience: {kind: string; days?: number; players?: string[]};
    start: string; phases: Phase[]; triggers?: Trigger[]; regions?: Region[]; scenes?: Scene[]; tests: Case[]; quietAfterHours?: number;
}

export const SIGNAL = 'story signal';
export const GATE = 'missions completed';
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
}
/** The kinds a picker of a type lists (a `character` is a wolf or a resident). */
export const REF_KIND: Record<string, keyof Refs> = {
    resident: 'residents', wolf: 'wolves', place: 'places', faction: 'factions', town: 'towns', market: 'markets', area: 'areas',
    ship: 'ships', item: 'items', template: 'templates', project: 'projects',
};
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
    }
    return out;
}

export interface Slot {
    slot: string; keys: string[]; type: string; label: string; value: unknown; shown: string; empty: boolean; param?: Param;
}
export type Part = {text: string} | Slot;
const SLOT = /\{([^}]+)\}/g;

/** A value as words: a name for an id, the catalogue's word for a choice, "the acting wolf" for $actor. */
export function shown(type: string, value: unknown, names: Names, param?: Param): string {
    if (value === undefined || value === null || value === '') return '';
    if (typeof value === 'boolean') return param?.words?.[String(value)] ?? (value ? 'yes' : 'no');
    const text = String(value);
    if (param?.words && text in param.words) return param.words[text];
    if (type === 'wolf' || type === 'character') {
        if (text === '$actor') return 'the acting wolf';
        if (text === 'world' && param?.world) return 'everyone (a public scene)';
    }
    const name = names[type]?.[text];
    if (name && name !== text) return name;
    if (type === 'choice' || type === 'int' || type === 'days' || type === 'number') return text.replaceAll('_', ' ');
    if (type === 'text') return '“' + text + '”';
    return text;
}

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
export const plain = (ps: Part[]) => ps.map(p => 'text' in p ? p.text : (p.shown || p.label)).join('');

export function eventOf(cat: Catalogue, id: string): CatEvent | undefined { return cat.events.find(e => e.id === id); }
export function itemOf(list: CatItem[], id: string): CatItem | undefined { return list.find(e => e.id === id); }

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
    if (c.test === 'field') {
        const amount = (AMOUNTS as readonly string[]).includes(c.field);
        const ps = itemParts(item, c as unknown as Json, names);
        return ps.map(p => 'slot' in p && p.slot === 'value' ? {...p, type: amount ? 'int' : 'text',
            shown: amount ? String(c.value) : shown('text', c.value, names)} : p);
    }
    return itemParts(item, c as unknown as Json, names);
}

export function actionParts(cat: Catalogue, a: Action, names: Names): Part[] {
    if (a.do === 'effect') return effectParts(cat, a.effect, names);
    const item = itemOf(cat.actions, a.do);
    if (!item) return [{text: String((a as Json).do)}];
    const record = {...Object.fromEntries((item.params ?? []).filter(p => p.default !== undefined).map(p => [p.key, p.default])), ...a};
    return itemParts(item, record as Json, names);
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
    | {kind: 'exit'; phase: number; index: number};
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
            opening: p.start === ph.id, ending: !ph.exits.length && !ph.triggers?.length && !(p.triggers?.length && ph.campaignRules),
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

/** Every signal and counter the program names (for the pickers, and "where is it used"). */
export function signalsOf(p: Program): string[] {
    const out = new Set<string>();
    eachTrigger(p, t => {
        if (t.on === SIGNAL && t.signal) out.add(t.signal);
        for (const a of t.actions) if (a.do === 'emit' || a.do === 'schedule' || a.do === 'cancel') out.add(a.signal);
    });
    for (const ph of p.phases) for (const m of ph.missions ?? []) for (const o of m.objectives) if (o.signal) out.add(o.signal);
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
    eachTrigger(p, t => {
        walk(t.if);
        for (const a of t.actions) if (a.do === 'set' || a.do === 'add') out.set(a.scope + ':' + a.key, {scope: a.scope, key: a.key});
    });
    return [...out.values()].sort((a, b) => a.key.localeCompare(b.key) || a.scope.localeCompare(b.scope));
}
function eachTrigger(p: Program, fn: (t: Trigger, ref: Ref) => void) {
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
        if (type === 'signal') {
            if (t.on === SIGNAL && t.signal === id) out.push({ref, where: label + ': listens for it'});
            t.actions.forEach((a, i) => { if ((a.do === 'emit' || a.do === 'schedule' || a.do === 'cancel') && a.signal === id)
                out.push({ref, where: `${label}: action ${i + 1} (${a.do === 'emit' ? 'sends it' : a.do === 'schedule' ? 'starts its timer' : 'cancels its timer'})`}); });
        } else if (type === 'counter') {
            const scoped = (s: string) => what.scope === undefined || s === what.scope;
            if (conditionsOf(t.if).some(c => c.test === 'counter' && c.key === id && scoped(c.scope))) out.push({ref, where: label + ': reads it'});
            t.actions.forEach((a, i) => { if ((a.do === 'set' || a.do === 'add') && a.key === id && scoped(a.scope)) out.push({ref, where: `${label}: action ${i + 1} (${a.do}s it)`}); });
        } else if (type === 'trigger') {
            if (t.id === id) out.push({ref, where: label});
        } else if (type === 'phase') {
            t.actions.forEach((a, i) => { if (a.do === 'enter' && a.phase === id) out.push({ref, where: `${label}: action ${i + 1} goes there`}); });
        } else if (hasValue(t)) out.push({ref, where: label});
    });
    p.phases.forEach((ph, phase) => {
        if (type === 'phase') {
            ph.exits.forEach((x, index) => { if (x.to === id) out.push({ref: {kind: 'exit', phase, index}, where: `${ph.id}: exit ${index + 1} goes there`}); });
        } else if (type !== 'signal' && type !== 'counter' && type !== 'trigger') {
            ph.exits.forEach((x, index) => { if (hasValue(x.match)) out.push({ref: {kind: 'exit', phase, index}, where: `${ph.id}: exit ${index + 1}`}); });
            if (ph.effects.some(hasValue)) out.push({ref: {kind: 'entry', phase}, where: `${ph.id}: when it opens`});
            if (ph.guestVoices?.includes(id)) out.push({ref: {kind: 'phase', phase}, where: `${ph.id}: a guest voice`});
        }
        if (type === 'signal' && (ph.missions ?? []).some(m => m.objectives.some(o => o.signal === id))) out.push({ref: {kind: 'missions', phase}, where: `${ph.id}: a mission objective`});
        else if (!['signal', 'counter', 'trigger', 'phase'].includes(type) && (ph.missions ?? []).some(m => m.objectives.some(o => hasValue(o.match))))
            out.push({ref: {kind: 'missions', phase}, where: `${ph.id}: a mission objective's filter`});
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
        const touched = t.actions.some(a => (a.do === 'emit' || a.do === 'schedule' || a.do === 'cancel') && a.signal === from);
        if (!signal && !touched) return t;
        return {...t, ...(signal ? {signal: to} : {}), actions: t.actions.map(a =>
            (a.do === 'emit' || a.do === 'schedule' || a.do === 'cancel') && a.signal === from ? {...a, signal: to} : a)};
    });
    return {...out, phases: out.phases.map(ph => !ph.missions?.some(m => m.objectives.some(o => o.signal === from)) ? ph : {...ph,
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

/** A trigger renamed, and the rehearsal cases that expect it by name. */
export function renameTrigger(p: Program, ref: Ref, to: string): Program {
    const t = triggerAt(p, ref);
    if (!t || ref.kind !== 'trigger' || t.id === to) return p;
    const from = t.id;
    const out = withTriggers(p, ref.phase, triggersOf(p, ref.phase).map((x, i) => i === ref.index ? {...x, id: to} : x));
    return {...out, tests: out.tests.map(c => c.expectTriggers?.includes(from) ? {...c, expectTriggers: c.expectTriggers.map(x => x === from ? to : x)} : c)};
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
        const sentence = [plain(eventParts(cat, t, names)), ...(t.if ? conditionsOf(t.if).map(c => plain(conditionParts(cat, c, names))) : []),
            ...t.actions.map(a => plain(actionParts(cat, a, names)))].join(' · ');
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
        case 'source': return null;
        default: return {kind: 'settings'};
    }
}
/** Which action or condition of a trigger a problem's `part` names ("actions[2]", "if", "head"). */
export function partOf(at: At): {action?: number; conditions?: boolean; head?: boolean} {
    const m = /^actions\[(\d+)\]/.exec(at.part ?? '');
    if (m) return {action: Number(m[1])};
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
        if (param.type === 'region') e[param.key] = p.regions?.[0]?.id ?? '';
        else if (param.type === 'choice') e[param.key] = param.values![0];
        else if (param.type === 'int') e[param.key] = param.min! > 1 ? param.min! : Math.max(param.min!, param.nonzero ? 1 : param.key === 'x' || param.key === 'y' ? 0 : 1);
        else if (param.type === 'wolf') e[param.key] = '$actor';
        else if (param.type === 'scene') e[param.key] = String(p.scenes?.[0]?.id ?? '');
        else if (param.type === 'cue') e[param.key] = kind === 'scene.play' ? String(p.scenes?.[0]?.id ?? 'presentation') : 'cue';
        else e[param.key] = '';
    }
    if (kind === 'scene.play' && p.scenes?.[0]?.visibility === 'public') e.player = 'world';
    return e;
}
export function newAction(cat: Catalogue, kind: string, p: Program): Action {
    switch (kind) {
        case 'set': case 'add': return {do: kind, scope: 'story', key: countersOf(p)[0]?.key ?? 'progress', value: 1};
        case 'emit': return {do: 'emit', signal: signalsOf(p)[0] ?? 'next'};
        case 'schedule': return {do: 'schedule', signal: 'deadline', days: 1};
        case 'cancel': return {do: 'cancel', signal: signalsOf(p)[0] ?? 'deadline'};
        case 'enter': return {do: 'enter', phase: p.phases.at(-1)?.id ?? ''};
        default: return {do: 'effect', effect: newEffect(cat, kind.startsWith('effect:') ? kind.slice(7) : 'deed.award', p)};
    }
}
export function newCondition(kind: string, p: Program, phase: number | null): Condition {
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
function editableCondition(v: unknown, depth = 0): boolean {
    if (!record(v) || depth > 5) return false;
    if (v.test === 'all' || v.test === 'any') return Array.isArray(v.conditions) && v.conditions.every(c => editableCondition(c, depth + 1));
    if (v.test === 'not') return editableCondition(v.condition, depth + 1);
    if (v.test === 'region') return typeof v.region === 'string';
    if (v.test === 'mission') return typeof v.id === 'string';
    return (v.test === 'counter' || v.test === 'field') && ['string', 'number'].includes(typeof v.value);
}
export function editableTrigger(v: unknown): boolean {
    return record(v) && typeof v.id === 'string' && typeof v.on === 'string' && (v.if === undefined || editableCondition(v.if))
        && (v.match === undefined || record(v.match)) && Array.isArray(v.actions) && v.actions.every(a => record(a) && (a.do === 'effect'
            ? record(a.effect) && typeof a.effect.kind === 'string' : ['enter', 'emit', 'set', 'add', 'schedule', 'cancel'].includes(String(a.do))));
}
/** Whether the workbench can show a program as it is. A program it cannot is still kept, and opened in Source (risk 6). */
export function editable(v: unknown): v is Program {
    if (!record(v)) return false;
    const p = v as unknown as Program;
    const regionOk = (r: unknown) => record(r) && typeof r.id === 'string' && typeof r.cell === 'string' && ['x1', 'y1', 'x2', 'y2'].every(k => Number.isInteger(r[k]));
    return typeof p.title === 'string' && typeof p.id === 'string' && typeof p.start === 'string' && record(p.audience) && typeof p.audience.kind === 'string'
        && (p.regions === undefined || (Array.isArray(p.regions) && p.regions.every(regionOk)))
        && (p.triggers === undefined || (Array.isArray(p.triggers) && p.triggers.every(editableTrigger)))
        && (p.scenes === undefined || (Array.isArray(p.scenes) && p.scenes.every(s => record(s) && typeof s.title === 'string' && Array.isArray(s.beats))))
        && Array.isArray(p.tests) && p.tests.every(c => record(c) && typeof c.name === 'string' && Array.isArray(c.cohort) && Array.isArray(c.expect)
            && Array.isArray(c.events) && c.events.every(f => record(f) && typeof f.kind === 'string'))
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
