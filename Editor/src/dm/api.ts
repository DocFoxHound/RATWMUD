// Calls to the Dungeon Master host (tools/dungeon_master.py). The session token lives in this tab only.
import type {Person, Place, Project, Route} from '../model/model.mjs';
import type {SentGround} from '../lib/lazyGround';
import type {Chronicle} from '../lib/chronicle';

export type Target = 'prod' | 'dev';
export type Role = 'viewer' | 'dm' | 'admin';
export interface Me { username: string; role: Role }
export interface Character {
    id: string; name: string; age: number | null; dead: boolean; cell: string; place: string; indoors: boolean;
    x: number; y: number; worldX: number | null; worldY: number | null; posture: string; activity: string;
    stats: Record<'strength' | 'dexterity' | 'wisdom' | 'stamina', number | null>;
    skills: Record<'sneakSkill' | 'hearingSkill' | 'scentSkill', number | null>;
    senses: Record<'hearing' | 'vision' | 'smell', number | null>;
    gift: string; quickened: boolean;            // A Gift (Docs/Design/33-combat.md): "fire" or "".
    saved: string;
}
export interface Action { id: number; kind: string; target: string; by: string; at: string; status: 'queued' | 'applied' | 'refused' | 'expired'; result: string }
export interface Players { target: Target; world: {id: string; name: string} | null; characters: Character[]; actions: Action[] }

const KEY = 'ratw-dm-session';
let token = (() => { try { return sessionStorage.getItem(KEY) ?? ''; } catch { return ''; } })();
export const signedIn = () => !!token;

export class DmError extends Error { constructor(message: string, readonly status: number) { super(message); } }

async function call<T>(path: string, body?: unknown): Promise<T> {
    const response = await fetch(path, {
        method: body === undefined ? 'GET' : 'POST',
        headers: {...(token ? {Authorization: `Bearer ${token}`} : {}), ...(body === undefined ? {} : {'Content-Type': 'application/json'})},
        body: body === undefined ? undefined : JSON.stringify(body),
    });
    const data = await response.json().catch(() => ({error: `Host replied ${response.status}.`}));
    if (!response.ok) {
        if (response.status === 401) setToken('');
        throw new DmError(data.error ?? `Host replied ${response.status}.`, response.status);
    }
    return data as T;
}
function setToken(value: string) {
    token = value;
    try { if (value) sessionStorage.setItem(KEY, value); else sessionStorage.removeItem(KEY); } catch { /* private window */ }
}

export interface Holder { id: string; name: string; role: string; slot: string; work: Place; home: Place }
export type AreaKind = 'wander' | 'spawn' | 'plan';
/** A painted NPC area: tiles of one cell or interior (cell-local). */
export interface NpcArea { id: string; name: string; kind: AreaKind; cell: string; tiles: [number, number][] }
/** Keeps `count` NPCs made from the template alive in the area; the game server runs it. */
export interface SpawnRule { id: string; name: string; area: string; template: string; count: number; respawnMinutes: number; enabled: boolean; alive: number; dead: number }
export interface Npcs {
    target: Target; world: string; people: Person[]; holders: Holder[]; dead: string[]; routes: Route[]; areas: NpcArea[]; spawns: SpawnRule[];
    /** NPC ID → the area they wander; NPC ID → the rule that spawned them. */
    wanders: Record<string, string>; spawned: Record<string, string>; actions: Action[];
}

export type FactionKind = 'npc' | 'city' | 'guild' | 'clan' | 'other';
export type Stance = 'allied' | 'friendly' | 'neutral' | 'tense' | 'hostile' | 'war';
export interface Faction { id: string; name: string; color: string; kind: FactionKind; description: string }
/** A faction's claim on one cell or interior: painted tiles (place-local), or the whole place when empty. */
export interface Claim { faction: string; area: string; tiles: [number, number][] }
/** How `faction` regards `other`. */
export interface Relation { faction: string; other: string; disposition: number; stance: Stance; reason: string; by: string; at: string }
export interface Member { faction: string; npc: string; rank: string }
export interface Factions {
    target: Target; world: string; factions: Faction[]; claims: Claim[]; relations: Relation[]; members: Member[];
    people: {id: string; name: string; role: string}[]; actions: Action[];
}
export interface RelationChange { disposition: number; stance: Stance; reason: string; by: string; at: string }

/** The world's week and festivals (Phase 9). */
export interface WorldCalendar {
    target: Target;
    today: {day: number; date: string; weekday: string; season: string; hour: number; nextMarket: number; nextRest: number;
        nextFestival: number; festivalDate: string} | null;
    communities: {id: string; residents: number}[];
    actions: {id: number; target: string; payload: {name?: string; inDays?: number}; by: string; at: string; status: Action['status']; result: string}[];
    weekdays: string[];
}

/** Uploaded portraits waiting for a decision, and the latest decisions (Docs/Design/29-client-polish.md, phase 9). */
export interface Portrait { id: string; account: string; character: string; name: string; status: 'pending' | 'approved' | 'rejected';
    reason: string; reported: boolean; at: string; png?: string }
export interface Portraits {
    target: Target; ready: boolean; pending: Portrait[]; recent: Portrait[];
    actions: {id: number; target: string; payload: {decision?: string; reason?: string}; by: string; at: string; status: Action['status']; result: string}[];
}

/** Chapters as the game server last saved them (Docs/Design/32): camps and their buildings, treaties, levies, Houses. */
export interface ChapterMember { id: string; name: string; rank: number }
export interface Chapter { id: string; name: string; colour: string; level: number; renown: number; charter: string; members: ChapterMember[];
    hold: string; holdName: string; houseOf: string; toll: number; sworn: number }
export interface CampBuilding { id: string; kind: string; x: number; y: number; built: boolean; condition: number }
/** A camp or Hold; its buildings are in its cell's tiles (cellX + x on the world map). */
export interface CampSite { id: string; chapter: string; name: string; cell: string; place: string; state: string; x: number; y: number;
    cellX: number | null; cellY: number | null; structures: CampBuilding[]; staff: {npc: string; name: string; role: string; wage: number}[] }
export interface Treaty { id: string; faction: string; factionName: string; chapter: string; build: boolean; tithe: number; levy: boolean;
    labour: boolean; weeks: number; state: string; proposed: number; started: number }
export interface HouseRequest { chapter: string; faction: string; factionName: string; state: string; day: number }
export interface Levy { id: string; faction: string; factionName: string; chapter: string; place: string; needed: number; done: number; due: number; state: string }
export interface Chapters {
    target: Target; world: string; chapters: Chapter[]; sites: CampSite[]; treaties: Treaty[]; houses: HouseRequest[]; levies: Levy[];
    actions: (Action & {payload: {approve?: boolean; faction?: string}})[];
}

/** The LIVE map (Docs/Design/34-dungeon-master-refresh.md, 1.1). A frame's people: [id, name, kind, cell, x, y, flags, role,
 *  doing]; kind "p" a player in the world, "o" a character not in it, "n" an NPC, "r" folk of the road, "t" a temporary
 *  visitor; flags 1 dead, 2 downed, 4 off stage, 8 in a fight. Shops: [merchant, name, label, cell, x, y, at a stall].
 *  Weather: [id, kind, x, y (world tiles), reach, strength 0..1]. */
export type LivePerson = [string, string, 'p' | 'o' | 'n' | 'r' | 't', string, number, number, number, string, string];
export type LiveWeather = [string, string, number, number, number, number];
/** A rumour going round: who it is about, what is said, who has heard it, and how sure they are on average. */
export interface Rumour { subject: string; claim: string; holders: string[]; sure: number }
export type LiveShop = [string, string, string, string, number, number, boolean];
export interface LiveEvent { id: number; kind: string; actor: string; target: string; cell: string; day: number; detail: string; at: string }
export interface Live {
    target: Target; frame: {day: number; people: LivePerson[]; shops: LiveShop[]; weather?: LiveWeather[]} | null;
    /** Seconds since the game server wrote the frame; null if it never has. */
    age: number | null; events: LiveEvent[]; actions: Action[];
}

export const dmApi = {
    login: async (username: string, password: string) => { const r = await call<Me & {token: string}>('api/login', {username, password}); setToken(r.token); return r as Me; },
    logout: async () => { try { await call('api/logout', {}); } finally { setToken(''); } },
    me: () => call<Me>('api/me'),
    players: (target: Target) => call<Players>(`api/players?target=${target}`),
    /** Lean: world cells come as outlines and previews; their ground is asked for as they come into view (dm/world.ts). */
    world: (target: Target) => call<Project>(`api/world?target=${target}&lean=1`),
    ground: (target: Target, ids: string[]) =>
        call<{seq?: number; cells: Record<string, SentGround>}>(`api/ground?target=${target}&cells=${ids.map(encodeURIComponent).join(',')}`),
    act: (target: Target, kind: string, characterId: string, reason: string, payload?: Record<string, unknown>) =>
        call<{id: number}>('api/actions', {target, kind, characterId, reason, ...(payload ? {payload} : {})}),
    npcs: (target: Target) => call<Npcs>(`api/npcs?target=${target}`),
    /** Asking keeps this account watching: the game server writes frames only while someone is. */
    live: (target: Target) => call<Live>(`api/live?target=${target}`),
    move: (target: Target, kind: 'npc.move' | 'character.move', id: string, place: Place, reason = '') =>
        call<{id: number}>('api/live/move', {target, kind, id, cell: place.cell, x: place.x, y: place.y, reason}),
    /** A temporary visitor on a tile for `minutes`, perhaps looking like a resident (`like`). */
    visit: (target: Target, name: string, place: Place, minutes: number, like = '') =>
        call<{id: number; visitor: string}>('api/live/visit', {target, name, cell: place.cell, x: place.x, y: place.y, minutes, like}),
    leave: (target: Target, id: string) => call<{id: number}>('api/live/leave', {target, id}),
    rumours: (target: Target) => call<{target: Target; rumours: Rumour[]}>(`api/live/rumours?target=${target}`),
    saveNpc: (target: Target, person: Person) => call<{id: string; action: number}>('api/npcs/save', {target, person}),
    deleteNpc: (target: Target, id: string) => call<{id: string; action: number}>('api/npcs/delete', {target, id}),
    npcLife: (target: Target, id: string, dead: boolean) => call<{id: string; action: number}>('api/npcs/life', {target, id, dead}),
    saveRoute: (target: Target, route: Route) => call<{id: string; action: number}>('api/routes/save', {target, route}),
    deleteRoute: (target: Target, id: string) => call<{id: string; action: number}>('api/routes/delete', {target, id}),
    saveArea: (target: Target, area: NpcArea) => call<{id: string; action: number}>('api/areas/save', {target, area}),
    deleteArea: (target: Target, id: string) => call<{id: string; action: number}>('api/areas/delete', {target, id}),
    saveSpawn: (target: Target, rule: Omit<SpawnRule, 'alive' | 'dead'>) => call<{id: string}>('api/spawns/save', {target, rule}),
    deleteSpawn: (target: Target, id: string) => call<{id: string}>('api/spawns/delete', {target, id}),
    setWander: (target: Target, id: string, area: string | null) => call<{id: string; action: number}>('api/npcs/wander', {target, id, area}),
    factions: (target: Target) => call<Factions>(`api/factions?target=${target}`),
    relationHistory: (target: Target, faction: string, other: string) =>
        call<RelationChange[]>(`api/factions/history?target=${target}&faction=${encodeURIComponent(faction)}&other=${encodeURIComponent(other)}`),
    saveFaction: (target: Target, faction: Faction) => call<{id: string; action: number}>('api/factions/save', {target, faction}),
    deleteFaction: (target: Target, id: string) => call<{id: string; action: number}>('api/factions/delete', {target, id}),
    claim: (target: Target, faction: string, area: string, tiles: [number, number][]) => call<{action: number}>('api/factions/claim', {target, faction, area, tiles}),
    unclaim: (target: Target, faction: string, area: string) => call<{action: number}>('api/factions/unclaim', {target, faction, area}),
    relate: (target: Target, faction: string, other: string, disposition: number, stance: Stance, reason: string) =>
        call<unknown>('api/factions/relation', {target, faction, other, disposition, stance, reason}),
    member: (target: Target, faction: string, npc: string, rank: string | null) => call<unknown>('api/factions/member', {target, faction, npc, rank}),
    /** Someone's life from the event log, their story if one was written, and what they carry in mind (Phase 8). */
    chronicle: (target: Target, id: string) => call<Chronicle>(`api/chronicle?target=${target}&id=${encodeURIComponent(id)}`),
    /** Has their life story written from the chronicle (one call to the NPC model) and kept. */
    writeStory: (target: Target, id: string) => call<Chronicle>('api/chronicle/story', {target, id}),
    calendar: (target: Target) => call<WorldCalendar>(`api/calendar?target=${target}`),
    callFestival: (target: Target, community: string, name: string, inDays: number) =>
        call<{id: number}>('api/festivals/call', {target, community, name, inDays}),
    artwork: (target: Target) => call<Portraits>(`api/artwork?target=${target}`),
    reviewArtwork: (target: Target, id: string, decision: 'approve' | 'reject', reason: string) =>
        call<{id: number}>('api/artwork/review', {target, id, decision, reason}),
    chapters: (target: Target) => call<Chapters>(`api/chapters?target=${target}`),
    /** Approves or refuses a pending treaty (its ID) or House request (the Chapter's ID and the faction). */
    decide: (target: Target, what: 'treaty' | 'house', id: string, approve: boolean, faction = '', reason = '') =>
        call<{id: number}>('api/chapters/decide', {target, what, id, approve, faction, reason}),
    action: (target: Target, id: number) => call<{id: number; status: Action['status']; result: string}>(`api/actions/${id}?target=${target}`),
};
