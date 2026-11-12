// Pull and import with conflicts (Docs/Design/66-worlds-atlas-and-dm.md, section 4; tools/world_sync.py): what the
// engine's plans look like, and the pure logic the screens need (Atlas's Update from live, the DM's Import from dev).
// The central piece is buildChoices: the user's picks in the UI turned into the engine's choices JSON,
// {conflict id: "dev" | "live" | {field: "dev" | "live"}}. Kept free of React so node can test it.
//
// The live world's truth (doc 66, Progress): the live game owns its NPCs' live state (their homes and posts, what they
// carry; a row it made, whole). A conflict over such a field starts on live's (the plan's `default`), and the user can
// choose dev's. A map-replacing import lists the NPCs whose places the new map hasn't (`gone`): a decision each.

import type {HostServer} from './hosted';

export type Side = 'dev' | 'live';
export type RowChange = 'added' | 'changed' | 'removed' | 'unchanged' | 'absent';
/** A value too long to show (terrain glyphs, heights): its size and a digest. */
export interface LongValue { $long: number; digest: string }
/** One field of a row in a plan: its base, dev and live values, and which side changed it (rows on both sides only). */
export interface SyncField {
    field: string; base: unknown; dev: unknown; live: unknown; changed: 'dev' | 'live' | 'same' | 'conflict' | null;
    /** The live game's (its live value is the truth) or Atlas's (authored). */
    owner?: 'game' | 'authored';
    /** The side the field goes with no choice (null: the user chooses). */
    default?: Side | 'same' | null;
}
export type PlaceName = 'home' | 'work' | 'evening';
export interface PlaceRef { cell: string; x: number; y: number }
/** A place (home, work, evening) an NPC would keep that the new map hasn't, and dev's place for it if dev has one. */
export interface GonePlace { place: PlaceName; area: string | null; dev: PlaceRef | null }
/** A decision for a gone place: dev's place, unset, or a new place on the new map. */
export type PlaceChoice = 'dev' | 'unset' | PlaceRef;
/** One of the new map's places, to pick a new home or post from. */
export interface AreaRef { id: string; name: string; kind: 'cell' | 'interior'; width: number | null; height: number | null }
/** One row (by its key) that differs between dev and live. */
export interface SyncItem {
    id: string; table: string; label: string; part: 'world' | 'live'; key: unknown[]; name: string;
    side: Side | 'conflict'; dev: RowChange; live: RowChange; fields: SyncField[];
    /** A conflict on a row both sides have: it can be chosen field by field. */
    fieldChoices: boolean;
    /** The choice taken when none is sent (the live game's truth); null or missing: the user must choose. */
    default?: Choice | null;
    /** A map-replacing import: the places the new map hasn't, a decision each. */
    gone?: GonePlace[];
    placeChoices?: boolean;
    /** A row one side removed with its NPC, that NPC a conflict: the conflict's id; it goes as that ends. */
    follows?: string | null;
}
export interface Counts { added: number; changed: number; removed: number }
/** {part: {table label: counts}}. */
export interface Summary { world: Record<string, Counts>; live: Record<string, Counts> }
export interface SyncBase { upstream: string; world: string; release: number; takenAt: string; by: string; how: string; rows: number }
export interface ServerState { state: 'running' | 'maintenance'; by: string; at: string | null; note: string }
export interface Release { number: number; at: string; by: string; note: string; kind: 'push' | 'import' | 'rollback'; restored: number | null; summary: Summary }
/** A registered world as the studio shows it. */
export interface SyncWorld { key: string; name: string; kind: 'dev' | 'live'; upstream?: string; aliases?: string[] }
/** A world's map identity (doc 66, "The same map"): a stamp made when a map is generated new, kept by every edit. */
export interface MapIdentity { id: string; name: string; made?: string | null; by?: string; how?: string; from?: 'stamp' | 'release' | 'original' }
/** The two worlds' maps, and whether they are one. */
export interface MapPair { dev: MapIdentity; live: MapIdentity; same: boolean }
/** What a map-replacing import lets go of: live's rows standing in places the new map hasn't (never removed with them). */
export interface LetGo { npcs: number; slots: number; posts: number; claims: number; npcAreas: number; names?: string[] }
/** The three-way plan of a dev world against its upstream (world_sync.diff). */
export interface SyncPlan {
    dev: string; live: string; world: string | null; base: SyncBase; liveRelease: number;
    counts: {live: number; dev: number; conflicts: number};
    summary: {live: Summary; dev: Summary};
    liveChanges: SyncItem[]; devChanges: SyncItem[]; conflicts: SyncItem[];
    digest: string; warnings: string[];
    /** The two maps (always on a plan from the engine); `replacesMap` on an import's plan between two maps. */
    maps?: MapPair; replacesMap?: boolean; letGo?: LetGo;
    /** The conflicts' defaults, by id; the new map's places (when some NPC's place is gone); live's economy settings
     *  (live.economy's kept columns: the treasury and stores, needsPaused, the seasons, services and rules with the
     *  host's overrides; doc 79, 14) that differ from dev's: each world's own, never synced or imported. */
    defaults?: Record<string, Choice>; areas?: AreaRef[];
    economy?: {fields: string[]; dev: Record<string, unknown>; live: Record<string, unknown>};
}
/** Atlas's Update from live: the plan, the two worlds, and the live server's state (null when it can't be read). */
export interface PullPlan extends SyncPlan {
    devWorld: SyncWorld; liveWorld: SyncWorld;
    liveServer: {state: ServerState; running: boolean; release: number} | null;
}
export interface PullResult { dev: string; live: string; release: number; applied: Summary; conflicts: number; kept: number; warnings: string[]; defaulted?: string[] }
/** What Import from dev would do (world_sync.plan_import), and what blocks it now. */
export interface ImportPlan extends SyncPlan {
    devRevision: number; errors: string[]; server: ServerState; serverRunning: boolean; blocked: string[]; releases: Release[];
}
export interface ImportResult {
    release: number; summary: Summary & {letGo?: LetGo}; conflicts: number; replacedMap?: boolean; map?: MapIdentity;
    dev: {updated: number; editedMeanwhile?: string[]; error?: string}; warnings?: string[]; defaulted?: string[];
}
export interface ServerStatus {
    key: string; name: string; kind: 'live' | 'dev'; world: string | null; server: ServerState; serverRunning: boolean;
    release: number; live: string; warning: string;
    /** The worlds the live server could switch to (every other registered world: live ones, or a dev world served as live). */
    switchTo?: {key: string; name: string; kind: 'live' | 'dev'; upstream?: string}[];
    /** A dev world served as live (doc 66, Progress): its newest build, and why it has no imports or releases. */
    build?: BuildStatus;
    servedDev?: string;
    /** Where its running game server's game is (its port; its host when on another computer), and whether the studio
     *  is starting one. */
    play?: {port: number; host: string | null; local: boolean} | null;
    starting?: boolean;
    /** A host's Editor (Docs/Design/78, 7): one world; Close and Open only, by its owner. */
    hosted?: boolean;
    /** A host's Editor: the package's own word on its game server (Package/entrypoint.py's server.json), or null. */
    hostServer?: HostServer | null;
}
/** A running game server's game, for a browser on this page's computer: at the host the page came from (the studio
 *  may be reached over the local network), unless the server runs on another computer; the game is plain http. */
export const gameUrl = (play: {port: number; host: string | null}, here: {hostname: string} = globalThis.location) =>
    `http://${play.host ?? here.hostname}:${play.port}/`;
/** A dev world's newest build (tools/world_build.py): whether it is current with Atlas's edits. */
export interface BuildStatus { build: number | null; at: string | null; by: string; release: number | null; current: boolean; problem?: string }

/** What the user picked for one conflict: the whole row from one side (or, for an NPC whose place the new map hasn't,
 *  unplaced), or (rows both sides have) field by field; and a decision for each gone place. */
export interface Pick { whole?: Side | 'unplace'; byField?: boolean; fields?: Record<string, Side>; places?: Partial<Record<PlaceName, PlaceChoice>> }
export type Picks = Record<string, Pick>;
export type Choice = Side | 'unplace' | Record<string, Side | PlaceChoice>;

/** The fields of a conflicting row in which dev and live differ: the ones a field-by-field choice is about. */
export const differingFields = (item: SyncItem) => item.fields.filter(f => f.changed !== 'same');

/** A field's side in a field-by-field choice: the user's, else the side that alone changed it; undefined while a field
 *  both sides changed is unchosen. */
export function fieldSide(field: SyncField, pick: Pick | undefined): Side | undefined {
    const chosen = pick?.fields?.[field.field];
    if (chosen) return chosen;
    if (field.changed === 'dev' || field.changed === 'live') return field.changed;
    return field.default === 'dev' || field.default === 'live' ? field.default : undefined;    // (The game's: live's.)
}

/** The picks a plan starts with: each conflict's default (the live game's truth), so the user sees it chosen and can
 *  change it. Conflicts without a default (authored fields changed on both sides, places the new map hasn't) start
 *  unpicked. */
export function defaultPicks(conflicts: SyncItem[]): Picks {
    const out: Picks = {};
    for (const item of conflicts) {
        const d = item.default;
        if (d === 'dev' || d === 'live') out[item.id] = {whole: d};
        else if (d && typeof d === 'object') {
            const fields = Object.fromEntries(Object.entries(d).filter(([, v]) => v === 'dev' || v === 'live')) as Record<string, Side>;
            out[item.id] = item.fieldChoices ? {byField: true, fields} : {whole: 'live'};
        }
    }
    return out;
}

/** Whether dev has the row (a row it lacks, never had or removed, can't be taken from dev). */
export const devHas = (item: SyncItem) => item.dev !== 'absent' && item.dev !== 'removed';

/** The whole-row sides a conflict offers: both, or for an NPC whose place the new map hasn't, dev's alone (live's place
 *  is gone) and only when dev has the row. */
export const wholeSides = (item: SyncItem): Side[] => (item.gone?.length ? (devHas(item) ? ['dev'] : []) : ['dev', 'live']);

/** "All conflicts" from one side: each conflict that offers it taken whole from it; the rest keep their picks. */
export const allPicks = (conflicts: SyncItem[], picks: Picks, side: Side): Picks =>
    ({...picks, ...Object.fromEntries(conflicts.filter(c => wholeSides(c).includes(side)).map(c => [c.id, {whole: side}]))});

/** The columns of the place a field belongs to (an NPC's or a profession slot's home, work or evening: its area, x and
 *  y), or the field alone: a place is one field, never half dev's and half live's. */
export function placeGroup(item: SyncItem, field: string): string[] {
    const found = /^(home|work|evening)_(area|x|y)$/.exec(field);
    if (!found || (item.table !== 'live.npcs' && item.table !== 'live.profession_slots')) return [field];
    return ['area', 'x', 'y'].map(c => `${found[1]}_${c}`);
}

/** A pick with one field on one side, field by field (a place's column: the whole place). */
export const pickField = (item: SyncItem, pick: Pick | undefined, field: string, side: Side): Pick =>
    ({...pick, byField: true, fields: {...pick?.fields, ...Object.fromEntries(placeGroup(item, field).map(f => [f, side]))}});

/** The columns of a conflict's gone places: their decision, not a side, says where they end. */
export const goneColumns = (item: SyncItem) => new Set((item.gone ?? []).flatMap(g => ['area', 'x', 'y'].map(c => `${g.place}_${c}`)));

/** A conflict's field-by-field part: the fields both sides changed (an unchosen one of the game's goes live's way, an
 *  authored one leaves it incomplete), and a field only one side changed only when taken from the other side. A place
 *  is chosen by any of its columns. */
function fieldMap(item: SyncItem, pick: Pick | undefined, skip = new Set<string>()) {
    const map: Record<string, Side> = {};
    let complete = true;
    for (const field of differingFields(item)) {
        if (skip.has(field.field)) continue;
        const chosen = pick?.fields?.[field.field] ?? placeGroup(item, field.field).map(f => pick?.fields?.[f]).find(Boolean);
        if (field.changed === 'conflict' || field.changed === null) {
            if (chosen) map[field.field] = chosen;
            else if (field.default !== 'dev' && field.default !== 'live') complete = false;    // (The game's: live's.)
        } else if (chosen && chosen !== field.changed) {
            map[field.field] = chosen;
        }
    }
    return {map, complete};
}

/** The engine's choices from the UI's picks, and the conflicts still without a full choice. A field-by-field choice
 *  names the fields both sides changed, and a field only one side changed only when the user took the other side's
 *  (the engine takes the changing side's otherwise). An NPC whose place the new map hasn't needs a decision for each
 *  such place ("unplace": unset, each) and, on a row both sides have, a choice for each authored field both changed,
 *  as any other. Picks for conflicts no longer in the plan are left out. */
export function buildChoices(conflicts: SyncItem[], picks: Picks): {choices: Record<string, Choice>; missing: string[]} {
    const choices: Record<string, Choice> = {}, missing: string[] = [];
    for (const item of conflicts) {
        const pick = picks[item.id];
        const gone = item.gone ?? [];
        if (gone.length) {
            if (pick?.whole === 'dev' && devHas(item)) { choices[item.id] = 'dev'; continue; }
            const places: Partial<Record<PlaceName, PlaceChoice>> = pick?.whole === 'unplace'
                ? Object.fromEntries(gone.map(g => [g.place, 'unset'])) : pick?.places ?? {};
            const fields = item.fieldChoices ? fieldMap(item, pick, goneColumns(item)) : {map: {}, complete: true};
            if (!gone.every(g => places[g.place] !== undefined) || !fields.complete) { missing.push(item.id); continue; }
            choices[item.id] = pick?.whole === 'unplace' && !Object.keys(fields.map).length ? 'unplace' : {...places, ...fields.map};
        } else if (pick?.byField && item.fieldChoices) {
            const {map, complete} = fieldMap(item, pick);
            if (complete) choices[item.id] = map; else missing.push(item.id);
        } else if (pick?.whole === 'dev' || pick?.whole === 'live') {
            choices[item.id] = pick.whole;
        } else if (item.default == null) {
            missing.push(item.id);
        }
        // (No pick on a conflict with a default: the engine takes it, the live game's truth.)
    }
    return {choices, missing};
}

/** "The game's truth": each conflict's fields the live game changed (live alone or both) back on live's, whatever was
 *  picked; the authored fields as the user picked them (a whole-row pick turned field by field); a conflict with no
 *  pick, or a row one side removed, on its default (defaultPicks). Gone places keep their decisions. A place goes
 *  whole: all of its area, x and y, the one the two sides agree on too, so no earlier pick of it is left half. */
export function gameTruthPicks(conflicts: SyncItem[], picks: Picks): Picks {
    const out: Picks = {...picks}, starts = defaultPicks(conflicts);
    for (const item of conflicts) {
        const pick = picks[item.id];
        if (!pick || !item.fieldChoices) {
            if (starts[item.id]) out[item.id] = starts[item.id];
            continue;
        }
        const skip = goneColumns(item);
        const shown = differingFields(item).filter(f => !skip.has(f.field));
        const games = shown.filter(f => f.owner === 'game' && f.changed !== 'dev');
        const whole = !pick.byField && (pick.whole === 'dev' || pick.whole === 'live') ? pick.whole : undefined;
        if (!games.length || whole === 'live') continue;
        const columns = (fields: SyncField[]) => fields.flatMap(f => placeGroup(item, f.field));
        const fields: Record<string, Side> = whole ? Object.fromEntries(columns(shown).map(f => [f, whole])) : {...pick.fields};
        for (const f of columns(games)) fields[f] = 'live';
        const places = whole === 'dev' && item.gone?.length ? Object.fromEntries(item.gone.map(g => [g.place, 'dev'])) : pick.places;
        out[item.id] = {...pick, whole: pick.whole === 'unplace' ? 'unplace' : undefined, byField: true, fields, ...(places ? {places} : {})};
    }
    return out;
}

/** The picks that still apply after the plan was made again (the worlds moved): those for conflicts still there,
 *  without field picks for fields no longer differing. */
export function prunePicks(picks: Picks, conflicts: SyncItem[]): Picks {
    const out: Picks = {};
    for (const item of conflicts) {
        const pick = picks[item.id];
        if (!pick) continue;
        if (pick.byField && !item.fieldChoices) { if (pick.whole) out[item.id] = {whole: pick.whole}; continue; }
        const names = new Set(differingFields(item).map(f => f.field));
        const fields = Object.fromEntries(Object.entries(pick.fields ?? {}).filter(([name]) => names.has(name)));
        const gone = new Set((item.gone ?? []).map(g => g.place));
        const places = Object.fromEntries(Object.entries(pick.places ?? {}).filter(([name]) => gone.has(name as PlaceName)));
        out[item.id] = {...pick, ...(pick.fields ? {fields} : {}), ...(pick.places ? {places} : {})};
        if (pick.whole === 'unplace' && !gone.size) delete out[item.id].whole;
    }
    return out;
}

/** Every conflict taken whole from one side. */
export const allFrom = (conflicts: SyncItem[], side: Side): Picks => Object.fromEntries(conflicts.map(c => [c.id, {whole: side}]));

/** "home (cottage), evening (cottage)": a conflict's gone places, for the screens. */
export const goneText = (item: SyncItem) => (item.gone ?? []).map(g => `${g.place} (${g.area ?? '?'})`).join(', ');

/** A place as the screens show it: "shop 3, 4". */
export const placeText = (place: PlaceRef | null | undefined) => (place ? `${place.cell} ${place.x}, ${place.y}` : 'none');

/** The middle tile of one of the new map's places: where a new place picked in it starts. */
export const middleOf = (area: AreaRef): PlaceRef =>
    ({cell: area.id, x: Math.max(1, Math.floor((area.width ?? 2) / 2)), y: Math.max(1, Math.floor((area.height ?? 2) / 2))});

export const isLong = (value: unknown): value is LongValue =>
    typeof value === 'object' && value !== null && '$long' in value && typeof (value as LongValue).$long === 'number';

/** A value for a cell of the side-by-side view. */
export function valueText(value: unknown): string {
    if (value === undefined || value === null) return '—';
    if (isLong(value)) return `long value (${value.$long} characters, digest ${value.digest})`;
    if (typeof value === 'string') return value === '' ? '""' : value;
    return JSON.stringify(value);
}

/** Items by their table's label, in the plan's order. */
export function groupByLabel(items: SyncItem[]): {label: string; part: 'world' | 'live'; items: SyncItem[]}[] {
    const groups = new Map<string, {label: string; part: 'world' | 'live'; items: SyncItem[]}>();
    for (const item of items) {
        const group = groups.get(item.label) ?? {label: item.label, part: item.part, items: []};
        group.items.push(item);
        groups.set(item.label, group);
    }
    return [...groups.values()];
}

/** "2 added, 1 changed" ('' when nothing). */
export const countsText = (counts: Partial<Counts>) =>
    (['added', 'changed', 'removed'] as const).filter(k => counts[k]).map(k => `${counts[k]} ${k}`).join(', ');

/** A summary's lines, [label, "2 added, 1 changed"], world then live, the empty ones left out. */
export const summaryRows = (summary: Summary | undefined): [string, string][] =>
    summary ? [...Object.entries(summary.world ?? {}), ...Object.entries(summary.live ?? {})]
        .map(([label, counts]): [string, string] => [label, countsText(counts)]).filter(([, text]) => text) : [];

/** How many rows a summary adds, changes and removes in all. */
export const summaryTotal = (summary: Summary | undefined) =>
    [...Object.values(summary?.world ?? {}), ...Object.values(summary?.live ?? {})].reduce((n, c) => n + c.added + c.changed + c.removed, 0);

/** "12 NPCs, 3 cells": a summary's rows by label, in a line ('none' when empty). */
export const summaryLine = (summary: Summary | undefined) =>
    [...Object.entries(summary?.world ?? {}), ...Object.entries(summary?.live ?? {})]
        .map(([label, c]) => [label, c.added + c.changed + c.removed] as const).filter(([, n]) => n)
        .map(([label, n]) => `${n} ${label}`).join(', ') || 'none';

/** How a row changed on each side: "changed in dev, removed live". */
export function rowChangeText(item: SyncItem): string {
    const part = (what: RowChange, where: string) => (what === 'unchanged' || what === 'absent' ? '' : `${what} ${where}`);
    return [part(item.dev, 'in dev'), part(item.live, 'live')].filter(Boolean).join(', ') || 'differs';
}

/** The change one side made to a row ("added", "changed: name, purse", "removed"). */
export function sideChangeText(item: SyncItem, side: Side): string {
    const what = item[side];
    if (what !== 'changed') return what;
    const fields = item.fields.filter(f => f.changed === side || f.changed === 'conflict' || f.changed === null).map(f => f.field);
    return fields.length ? `changed: ${fields.join(', ')}` : 'changed';
}

/** Why Update from live has nothing to do between two maps (the engine refuses the same way): "PROD is on a different
 *  map (Ridgemere's original map) from DEV (Ridgemere 2.0: the city): ...". Null on one map. */
export function differentMapText(maps: MapPair | null | undefined, dev: string, live: string): string | null {
    if (!maps || maps.same) return null;
    const D = dev.toUpperCase(), L = live.toUpperCase();
    return `${L} is on a different map (${maps.live.name}) from ${D} (${maps.dev.name}): there is nothing to update from until ` +
        `${D}'s map is imported into ${L} (the DM's Import from dev, replacing ${L}'s map).`;
}

/** What the DM's import says before replacing live's map, and the confirmation it asks for. */
export function replaceMapText(maps: MapPair, dev: string, live: string): {warning: string; confirm: string} {
    const D = dev.toUpperCase(), L = live.toUpperCase();
    return {
        warning: `${D} is on a different map (${maps.dev.name}) from ${L} (${maps.live.name}). Importing replaces ${L}'s map with ` +
            `${D}'s whole: its cells, interiors, doors and places. Players' data and everyone in ${L} are kept; afterwards both share ` +
            `one map and Update from live works again.`,
        confirm: `Replace ${L}'s map (${maps.live.name}) with ${D}'s (${maps.dev.name})`,
    };
}

const plural = (n: number, one: string, many = `${one}s`) => `${n} ${n === 1 ? one : many}`;

/** "2 NPCs unplaced (Briar, Rook); 3 patrol posts and 1 territory claim let go" ('' when nothing). */
export function letGoText(letGo: LetGo | undefined): string {
    if (!letGo) return '';
    const unplaced = [letGo.npcs ? plural(letGo.npcs, 'NPC') : '', letGo.slots ? plural(letGo.slots, 'profession slot') : '']
        .filter(Boolean).join(' and ');
    const names = letGo.names?.length ? ` (${letGo.names.slice(0, 8).join(', ')}${letGo.names.length > 8 ? ', …' : ''})` : '';
    const gone = [letGo.posts ? plural(letGo.posts, 'patrol post') : '', letGo.claims ? plural(letGo.claims, 'territory claim') : '',
        letGo.npcAreas ? plural(letGo.npcAreas, 'NPC area') : ''].filter(Boolean);
    const goneText = gone.length > 1 ? `${gone.slice(0, -1).join(', ')} and ${gone[gone.length - 1]}` : gone[0] ?? '';
    return [unplaced ? `${unplaced} without a place on the new map${names}, a decision each` : '', goneText ? `${goneText} let go` : ''].filter(Boolean).join('; ');
}
