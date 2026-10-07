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
    practice?: Record<string, {name: string; value: number; cap: number}>;   // Each attribute and skill with its cap (doc 49).
    grades?: Record<string, 'weak' | 'strong'>;  // How it was built (doc 49): grades that aren't plain...
    specialty?: string;                          // ...and its specialty.
    account?: AccountStanding | null;            // Its account's earned Gift tiers (doc 49, Phase 5).
    person?: {handle: string; experience: string; playedHours: number} | null;   // Its account as a person (doc 50).
    profile?: Record<string, unknown> | null;    // Its roleplay profile, as saved (doc 50): read only.
    gift: string; quickened: boolean;            // A Gift (Docs/Design/33-combat.md): "fire" or "".
    injuries?: Injury[];                         // Injuries that outlast a fight (Docs/Design/38-injuries.md).
    dungeonMaster: boolean;                      // Marked a Dungeon Master in the game: they have the Dev Console.
    saved: string;
}
export interface PlayerReport {
    id: string; at: string; reporter: string; reporterName: string; reported: string; reportedName: string; reportedCharacter: string;
    kind: string; category: string; note: string; evidence: {seq: number; at: number; channel: string; text: string}[];
    status: 'open' | 'upheld' | 'dismissed'; decidedBy: string; decidedAt: string | null; outcome: string; silenceHours: number;
    earlier: {id: string; status: string; category: string; outcome: string}[]; blockedBy: number;
}
export interface Reports { target: Target; ready: boolean; reports: PlayerReport[]; actions: (Action & {payload?: unknown})[] }
export interface AccountStanding {
    name: string; socialLevel: number | null; hold: boolean; characters: string[];
    gifted: string | null; quickened: string | null;   // Who opened each tier ("earned", "dm:<name>"), or null.
    measures: {socialLevel?: number; normalScenes?: number; stars?: number; starGivers?: number; closedStories?: number};
}
export interface Injury { id: string; kind: 'acute' | 'lasting'; type: string; side?: string; severity: number; restLeft?: number; restFull?: number; from?: string }
// The injuries a Dungeon Master may give (doc 38, phase 5): acute first, then lasting.
export const InjuryTypes: [string, 'acute' | 'lasting'][] = [
    ['torn_flank', 'acute'], ['bitten_foreleg', 'acute'], ['bitten_hindleg', 'acute'], ['torn_ear_acute', 'acute'], ['wrenched_neck', 'acute'],
    ['deep_gash', 'acute'], ['cut_foreleg', 'acute'], ['cut_muzzle', 'acute'], ['cut_shoulder', 'acute'], ['bruised_ribs', 'acute'],
    ['cracked_rib', 'acute'], ['sprained_foreleg', 'acute'], ['knocked_senseless', 'acute'], ['burned_paws', 'acute'], ['singed_coat', 'acute'],
    ['burned_muzzle', 'acute'], ['torn_ear', 'lasting'], ['bent_tail', 'lasting'], ['scarred_muzzle', 'lasting'], ['scarred_flank', 'lasting'],
    ['burn_scars', 'lasting'], ['notched_nose', 'lasting'], ['clouded_eye', 'lasting'], ['permanent_limp', 'lasting'], ['bad_back', 'lasting'],
    ['stiff_shoulder', 'lasting']];
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

/** The game server's health (Docs/Design/31-responsiveness.md, the health tracker): each minute, the slowest ticks, and the
 *  players with the worst ping. Times in ms; ping is the middle player's as their pages report it (null with none). */
export interface HealthWindow {
    at: string; clients: number; mean: number; p99: number; max: number; over50: number;
    ping: number | null; ping95: number | null; slow: number; outMbps: number; corrections: number;
}
export interface HealthSpike { at: string; ms: number; clients: number; parts: [string, number][]; note: string }
export interface Health {
    target: Target; hours: number; missing: boolean; windows: HealthWindow[]; spikes: HealthSpike[];
    players: {name: string; ms: number; at: string}[];
}

/** Where the money is (Docs/Design/42-money-in-circulation.md, Phase 8), from the last save. Pennies; null where an
 *  account doesn't exist yet. */
export interface MoneyTown { residents: number; treasury: number | null; church: number | null; buyers: Record<string, number>;
    /** Its buildings' repair, 0 to 100, as the Town Works keeps them; null before its first day. */
    condition: number | null }
export interface MoneyGroup { count: number; total: number; median: number }
export interface Money {
    target: Target; day: number | null; total: number; capital: number | null; month: number | null;
    towns: Record<string, MoneyTown>; houses: {id: string; cash: number}[]; tills: {count: number; total: number};
    residents: {count: number; total: number; median: number | null; poorestTenth: number | null; richestTenth: number | null;
        shortOfFood: number; byRole: Record<string, MoneyGroup>};
    road: {caravans: number; contracts: number; bandits: number}; players: number;
    events: {kind: string; actor: string; target: string; day: number; detail: string; at: string}[];
    /** The economy orchestrator's last plan (Docs/Design/46-economy-orchestrator.md, Part 9); null in saves before it ran. */
    orchestrator?: Orchestrator | null;
    /** The orchestrator's funds (doc 46, Phase 5): each town's by channel, and the land's; absent in older hosts' replies. */
    funds?: Record<string, Partial<Record<string, number>>>;
    landFund?: number | null;
    /** Each town's granary (Phase 7): its cash and how many goods it holds. */
    granaries?: Record<string, {cash: number; goods: number}>;
}

/** The economy orchestrator, as last saved (doc 46). In shadow mode it plans but nothing it plans is applied yet. */
export type SteerKind = 'pressure' | 'town' | 'holder' | 'channel' | 'price';
export type EconomyChannel = 'works' | 'hires' | 'commissions' | 'food' | 'trade' | 'price support' | 'wage support' | 'rescue' | 'opening';
/** What a Dungeon Master asks of it: `target` is "" for pressure, a town, an account, a channel, or a town or "*" for a price. */
export interface SteerRequest { kind: SteerKind; targetId: string; item: string; strength: number; days: number; note: string }
/** A steer in force: from and until are game days; `by` is the Dungeon Master who set it. */
export interface Steer { id: string; kind: SteerKind; target: string; item: string; strength: number; from: number; until: number; note: string; by: string }
/** Why a town is in distress: "" when it is well. */
export type DistressKind = '' | 'empty shelves' | 'empty purses' | 'no work' | 'failing trade' | 'draining';
export interface OrchestratorTown {
    id: string; people: number; distress: number; week?: number; kind: DistressKind; foodCost: number; hungry: number; starving: number; short: number;
    poor: number; idle: number; shopFoodDays: number; takingsRatio: number; netInflow: number; wageFloor: number; share: number;
    /** A day's pay by kind of work (Phase 4): help, guard, labour, clergy, keeper, hand, odd job. */
    wages?: Partial<Record<WageKind, number>>;
}
export type WageKind = 'help' | 'guard' | 'labour' | 'clergy' | 'keeper' | 'hand' | 'odd job';
export const WageKinds: WageKind[] = ['help', 'guard', 'labour', 'clergy', 'keeper', 'hand', 'odd job'];
export type Band = 'warming' | 'growing' | 'lean' | 'comfortable' | 'over' | 'cap' | 'spared' | (string & {});
/** A holder over its band: what it holds, what it needs, and what the orchestrator has it spend. */
export interface OrchestratorHolder { id: string; kind: string; town: string; cash: number; need: number; band: Band; toSpend: number;
    /** What it gained over the week. */
    gain?: number }
export interface OrchestratorBrief {
    day: number; decided?: boolean; landDistress: number; moneySupply: number; pot: number; margin: number; median: number; gini: number;
    /** Residents' share of the land's money, 0..1 (Phase 6). */
    residentShare?: number;
    /** Its own pressure, 1 or more (Phase 5). */
    autoPressure?: number;
    /** The poorer half of households' share of residents' money, 0..1 (Phase 6). */
    bottomShare?: number;
    /** The living floor's multiplier, 1 or more (Phase 6). */
    floorLift?: number;
    /** Each channel's reach this week, 0..1 (Phase 7; on a decision's day). */
    reach?: Partial<Record<string, number>>;
    /** Each channel's learned weight, 0.5..2 (Phase 7). */
    learned?: Partial<Record<string, number>>;
    towns: OrchestratorTown[]; holders: OrchestratorHolder[]; bands: Partial<Record<string, number>>;
    orders: {from: string; town: string; channel: string; coins: number}[];
    channels: Partial<Record<string, number>>;
    prices: {town: string; item: string; catalog: number; now: number; would: number; support?: number}[];
}
/** A named bundle of steers a Dungeon Master starts with one click (Data/Economy/scenarios.json): `needs` says whether
 *  it asks for a town or a holder. */
export interface ScenarioSteer { kind: SteerKind; target: string; item?: string; strength: number; days: number }
export interface Scenario { id: string; name: string; about: string; needs: ('town' | 'holder')[]; steers: ScenarioSteer[] }
/** It measures every day (`brief`, the latest day's) and decides once a week, the evening of the reckoning (`decision`). */
export interface Orchestrator {
    mode: 'shadow' | 'on' | 'off'; day: number; steers: Steer[]; memory?: unknown;
    brief?: OrchestratorBrief | null; decision?: OrchestratorBrief | null;
}

export const dmApi = {
    login: async (username: string, password: string) => { const r = await call<Me & {token: string}>('api/login', {username, password}); setToken(r.token); return r as Me; },
    logout: async () => { try { await call('api/logout', {}); } finally { setToken(''); } },
    me: () => call<Me>('api/me'),
    players: (target: Target) => call<Players>(`api/players?target=${target}`),
    health: (target: Target, hours: number) => call<Health>(`api/health?target=${target}&hours=${hours}`),
    money: (target: Target) => call<Money>(`api/money?target=${target}`),
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
    /** Queues a steer for the economy orchestrator (doc 46, Part 10); the game names it steer-<id> at the next day's plan. */
    steerEconomy: (target: Target, steer: SteerRequest) => call<{id: number; steer: string}>('api/economy/steer', {target, ...steer}),
    /** Ends a steer before its time. */
    endSteer: (target: Target, id: string) => call<{id: number}>('api/economy/unsteer', {target, id}),
    scenarios: () => call<{scenarios: Scenario[]}>('api/economy/scenarios').then(r => r.scenarios),
    /** Starts a scenario: its steers expanded and queued, all or none. */
    startScenario: (target: Target, start: {scenario: string; town?: string; holder?: string}) =>
        call<{scenario: string; queued: {id: number; steer: string}[]}>('api/economy/scenario', {target, ...start}),
    artwork: (target: Target) => call<Portraits>(`api/artwork?target=${target}`),
    reports: (target: Target) => call<Reports>(`api/reports?target=${target}`),
    /** Upholds (with a note, a warning or a silence) or dismisses a player's report (doc 50). */
    decideReport: (target: Target, id: string, decision: 'uphold' | 'dismiss', outcome: string, hours: number, reason: string) =>
        call<{id: number}>('api/reports/decide', {target, id, decision, outcome, hours, reason}),
    reviewArtwork: (target: Target, id: string, decision: 'approve' | 'reject', reason: string) =>
        call<{id: number}>('api/artwork/review', {target, id, decision, reason}),
    chapters: (target: Target) => call<Chapters>(`api/chapters?target=${target}`),
    /** Approves or refuses a pending treaty (its ID) or House request (the Chapter's ID and the faction). */
    decide: (target: Target, what: 'treaty' | 'house', id: string, approve: boolean, faction = '', reason = '') =>
        call<{id: number}>('api/chapters/decide', {target, what, id, approve, faction, reason}),
    action: (target: Target, id: number) => call<{id: number; status: Action['status']; result: string}>(`api/actions/${id}?target=${target}`),
};
