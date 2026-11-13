// Calls to the studio's DM suite (tools/studio.py: /api/dm/, tools/dungeon_master.py), and to the DM methods Atlas's NPC
// pages use (/api/atlas/layers/). The session token lives in this tab only.
import type {Person, Place, Project, Route} from '../model/model.mjs';
import type {SentGround} from '../lib/lazyGround';
import type {Chronicle} from '../lib/chronicle';
import type {BuildStatus, Choice, ImportPlan, ImportResult, MapPair, Release, ServerStatus, SyncItem, SyncWorld} from '../lib/sync';
import type {EconomyActKind, EconomyReply, EconomyView, ProsperityReport} from '../lib/economyContract';
import {ENDED_TEXT, headingFor, hosted, mayCall, RESTARTING_ACT, restartingReply, setEnded, setRestarting} from '../lib/hosted';
import type {MapSummary, PublishJob, PublishProblem, PublishState, Restart} from '../lib/publish';
export type {EconomyActKind} from '../lib/economyContract';

/** A world's key (Database/worlds.json, doc 66): any registered world, live or dev. */
export type Target = string;
/** A registered world, as GET api/worlds gives it. */
export interface World { key: string; name: string; kind: 'live' | 'dev'; upstream?: string }
export interface Worlds { worlds: World[]; live: Target }
let known: World[] = [{key: 'prod', name: 'PROD', kind: 'live'}];
/** The registry as the host gave it (DmApp sets it once signed in). */
export const setWorlds = (worlds: World[]) => { if (worlds.length) known = worlds; };
/** This page's suite (Studio sets it): the DM suite's calls go to the DM's routes, Atlas's NPC pages' to Atlas's. */
let suite: 'atlas' | 'dm' | null = null;
/** The world the live server runs (doc 66): a live world, or a dev world served as live (DEV while PROD is closed). */
let served: Target | null = null;
export const setSuite = (which: 'atlas' | 'dm', live: Target | null) => { suite = which; served = live; };
/** A world players are in, so changes there are confirmed first: a live world, or in the DM suite the dev world the
 *  live server runs. */
export const isLive = (target: Target) => known.find(w => w.key === target)?.kind === 'live' || (suite === 'dm' && target === served);
/** A world's name, for headings. */
export const worldName = (target: Target) => known.find(w => w.key === target)?.name ?? target.toUpperCase();
/** A world for a heading or a line: in a host's Editor its own name (never its key, "WORLD"); in the dev studio its key
 *  in capitals, as the DM's pages always named it. */
export const worldLabel = (target: Target) => headingFor(target, known.find(w => w.key === target)?.name);
export type Role = 'viewer' | 'dm' | 'builder' | 'owner' | 'admin';
/** Who is signed in. In a host's Editor (Docs/Design/78, 7: `hosted`) the role is the Editor account's (viewer, dm,
 *  builder or owner, never admin), `username` is the account and its session ("pell#3f2a1c"), `mayActOnWolves` says
 *  whether it may kill, injure, move or bring back players' wolves (Q24), and `cheats` is world.toml's setting. */
export interface Me { username: string; role: Role; hosted?: boolean; account?: string; mayActOnWolves?: boolean; cheats?: 'off' | 'host' | 'everyone' }
const RANKS: Record<Role, number> = {viewer: 0, dm: 1, builder: 2, owner: 3, admin: 4};
/** Whether an account's role is this one or above (tools/dungeon_master.py's RANK). */
export const atLeast = (me: Me, role: Role) => (RANKS[me.role] ?? 0) >= RANKS[role];
/** Why a cheat (a Gift given, an account's tiers) is greyed in a host's Editor, or '' when it isn't. */
export const cheatsOff = (me: Me) => (me.hosted && (me.cheats ?? 'off') === 'off'
    ? 'Set cheats in world.toml ("host" or "everyone"); the world will then be listed "cheats on".' : '');
/** Why acting on a player's wolf (kill, injure, move, bring back) is greyed in a host's Editor, or ''. */
export const wolvesOff = (me: Me) => (me.hosted && !me.mayActOnWolves
    ? 'Needs the world\'s owner, or an account the owner marked "may act on players\' wolves".' : '');
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
    person?: {handle: string; experience: string; playedHours: number; newcomer?: boolean;   // Its account as a person (doc 50);
              mentor?: '' | 'available' | 'busy' | 'revoked'; guided?: number} | null;     // newcomer and mentor: doc 52.
    profile?: Record<string, unknown> | null;    // Its roleplay profile, as saved (doc 50): read only.
    circles?: {name: string; role: string; members: string[]}[];   // Its account's circles, members by handle (doc 50).
    stars?: {total: number; from: number; kinds: Record<string, number>} | null;   // Its account's stars (doc 51).
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
    /** A host's Editor (doc 78, 9): the reporter is hidden from the host, and the evidence is the world's own record. */
    reporterHidden?: boolean; evidenceFrom?: 'world';
}
/** Storytellers (doc 58): each account's standing, tales with their steps, the kept log, the visitors' list, templates. */
export interface StorytellerStanding { account: string; state: string; note: string; character: string; name: string; decidedBy: string; reason: string; appliedAt: number }
export interface TaleRow {
    id: string; title: string; premise: string; state: string; author: string; authorName: string; account: string;
    participants: {id: string; name: string; left: boolean}[]; steps: {title: string; objectives: {line: string; kind: string; by: string; byHand: boolean}[]}[];
}
export interface StoryLogRow { id: string; account: string; character: string; name: string; storyline: string; kind: string; target: string; text: string; detail: string; at: number }
export interface Storytellers {
    target: Target; ready: boolean; standing: StorytellerStanding[]; tales: TaleRow[]; log: StoryLogRow[];
    visitors: {id: string; name: string; description: string; enabled: boolean; approvedBy: string}[]; templates: {id: string; title: string; source: string}[];
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
export interface Tie { id: string; state: string; starter: string; town: string; newcomer: string; newcomerName: string;   // Doc 52.
    other: string; otherName: string; resident: boolean; made: number; lapsesAt: number }
export interface Players { target: Target; world: {id: string; name: string} | null; characters: Character[]; actions: Action[]; ties?: Tie[] }

const KEY = 'ratw-dm-session';
// (Hosted, doc 78: the page holds no token at all; the client's shell adds the Editor session to every request.)
let token = hosted ? '' : (() => { try { return sessionStorage.getItem(KEY) ?? ''; } catch { return ''; } })();
export const signedIn = () => hosted || !!token;
/** The DM account's sign-in, for every call to the studio (doc 66, 3: one sign-in for both suites). Nothing, hosted. */
export const authHeaders = (): Record<string, string> => (!hosted && token ? {Authorization: `Bearer ${token}`} : {});
/** Which suite's routes a world's calls go to (tools/studio.py): the DM suite's to the DM's (a live world, or the dev
 *  world the live server runs), Atlas's NPC pages' to Atlas's (the same methods, on Atlas's world only). Without a
 *  suite set: by the world's kind. */
const at = (target: Target) => ((suite ? suite === 'dm' : isLive(target)) ? 'api/dm/' : 'api/atlas/layers/');

/** A refusal from the studio: its message and status, and from the sync engine (tools/world_sync.py) the problems
 *  behind it (`errors`) and the conflicts left to choose (`conflicts`, on a 409). */
export class DmError extends Error {
    readonly status: number;
    readonly errors: string[];
    readonly conflicts: SyncItem[] | null;
    /** Two worlds on different maps (doc 66, "The same map"): both maps, as the engine names them. */
    readonly maps: MapPair | null;
    /** Publish's problems (doc 78, 10.3): each in plain words, with its place for "Show me" when it has one. */
    readonly problems: PublishProblem[];
    constructor(message: string, status: number, data: {errors?: unknown; conflicts?: unknown; maps?: unknown} = {}) {
        super(message);
        this.status = status;
        const listed: unknown[] = Array.isArray(data.errors) ? data.errors : [];
        const told = (e: unknown): e is PublishProblem => !!e && typeof e === 'object' && typeof (e as {text?: unknown}).text === 'string';
        this.problems = listed.filter(told);
        this.errors = listed.map(e => (told(e) ? e.text : String(e)));
        this.conflicts = Array.isArray(data.conflicts) ? data.conflicts as SyncItem[] : null;
        this.maps = data.maps && typeof data.maps === 'object' ? data.maps as MapPair : null;
    }
}

async function call<T>(path: string, body?: unknown, headers: Record<string, string> = {}): Promise<T> {
    // Hosted, once the session ended: a poll would only be refused again (and logged by the shell, all night), so reads
    // go out once a half minute until one finds the session live again (the host signed in again from the menu).
    if (hosted && body === undefined && !mayCall()) throw new DmError(ENDED_TEXT, 401);
    let response: Response;
    try {
        response = await fetch(path, {
            method: body === undefined ? 'GET' : 'POST',
            headers: {...authHeaders(), ...(body === undefined ? {} : {'Content-Type': 'application/json'}), ...headers},
            body: body === undefined ? undefined : JSON.stringify(body),
        });
    } catch (error) {
        // Hosted: nothing answered at all, the world (or its container) restarting.
        if (hosted) { setRestarting(true); throw new DmError(RESTARTING_ACT, 503); }
        throw error;
    }
    const data = await response.json().catch(() => ({error: `Host replied ${response.status}.`}));
    if (hosted && restartingReply(response.status, data)) {
        setRestarting(true);
        throw new DmError(RESTARTING_ACT, 503);
    }
    setRestarting(false);
    // Hosted: a 401 is the Editor session's end (the shell's word, or the world's); any other answer means it is live.
    setEnded(response.status === 401 && /signed-out|session/i.test(String(data.error ?? '')));
    if (!response.ok) {
        // (A wrong publish password is a 401 too, but not the end of the sign-in: the studio says which.)
        if (response.status === 401 && /sign in|session/i.test(String(data.error ?? ''))) setToken('');
        throw new DmError(data.error ?? `Host replied ${response.status}.`, response.status, data);
    }
    return data as T;
}
/** The studio's calls, for other modules' routes (Atlas's Update from live, which also carries Atlas's token). */
export const studioCall = call;
function setToken(value: string) {
    if (hosted) return;
    token = value;
    try { if (value) sessionStorage.setItem(KEY, value); else sessionStorage.removeItem(KEY); } catch { /* private window */ }
}

/** A resident's voice (Docs/Design/62-talk.md, phase 6): where they're from, how they sound, who they are. */
export interface Voice {
    from: string; accent: string; quarter: string; traits: Record<string, string>; quirk: string;
    persona: string;            // The personality written for them (decision 10): what the model is given.
    grammar: string;            // Their own words, in the .grammar syntax (no `grammar` line).
    rules: string;              // Their own lines, in the .talk syntax (rules only; self.id is added).
    writtenBy?: string;
}
export interface Talk { npc: string; voice: Voice | null; family: {npc: string; npcName: string; other: string; otherName: string; kind: string}[] }
export interface Figure { id: string; name: string; title: string; role: string; knownIn: string; about: string; npc: string }
/** A job holder; `work` or `home` null when unset (an unplaced or jobless NPC: doc 66, 2). */
export interface Holder { id: string; name: string; role: string; slot: string; work: Place | null; home: Place | null }
export type AreaKind = 'wander' | 'spawn' | 'plan';
/** A painted NPC area: tiles of one cell or interior (cell-local). */
export interface NpcArea { id: string; name: string; kind: AreaKind; cell: string; tiles: [number, number][] }
/** Keeps `count` NPCs made from the template alive in the area; the game server runs it. */
export interface SpawnRule { id: string; name: string; area: string; template: string; count: number; respawnMinutes: number; enabled: boolean; alive: number; dead: number }
export interface Npcs {
    target: Target; world: string; people: Person[]; holders: Holder[]; dead: string[]; routes: Route[]; areas: NpcArea[]; spawns: SpawnRule[];
    /** NPC ID → the area they wander; NPC ID → the rule that spawned them. */
    wanders: Record<string, string>; spawned: Record<string, string>; actions: Action[];
    /** NPC ID → the DM's band for a while on their household, as last saved: its band and the game day it lapses. */
    bandsHeld?: Record<string, {band: string; until: number; household: string}>;
    /** The NPCs the bands cover (residents the game houses, as last saved; null: no save yet); whether the world holds an
     *  NPC's own band (migration 0061). */
    bandable?: string[] | null; bandsAuthored?: boolean;
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
/** A good deed (doc 56): who did it, who saw it (with the name each knew them by), and how far it has got round each town. */
export interface FameDeed {
    id: string; kind: string; weight: string; phrase: string; doers: string[]; beneficiary: string; town: string; cell: string; day: number;
    names: Record<string, string[]>; witnesses: {id: string; as: Record<string, string>}[]; towns: {town: string; carrier: string; reach: number}[];
}
/** A town project (doc 57): where, how far along, and its top givers by the names they chose. */
export interface TownProject {
    id: string; kind: string; title: string; town: string; cell: string; x: number; y: number; state: string; hours: number; worked: number;
    condition: number; needs: Record<string, number>; namedFor: string; givers: {id: string; shown: string; value: number}[];
}
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

/** The Prosperity tab's read model (Docs/Design/79-a-staged-economy.md, 12; tools/dungeon_master.py prosperity()): Core's
 *  report from the last checkpoint, and what the tool reads beside it from the same row and the database. It lags the
 *  game by up to one checkpoint: `writtenAt`, and `stale` once that is over three minutes old. */
export interface WeekWagon { id: string; from: string; to: string; day: number; state: string; goods: string[] }
export interface WeekShip { id: string; port: string; name: string; day: number; state: 'due' | 'in' | 'sailed' | 'storm' | 'lost'; goods: string[] }
export interface WeekEvent { id: string; market: string; kind: string; day: number; hour: number; state: string; by: string; words: string }
export interface PlayerMoneyDay { day: number; made: number; sunk: number; goodsMade: number; goodsSunk: number }
export interface PlayerMoneyHolder { holder: string; name: string; made: number; sunk: number; 
    /** By the faucet's or the sink's reason: coin made, coin sunk, then goods in and out at the catalogue's price. */
    byReason: Record<string, [number, number, number?, number?]> }
export interface ProsperityView {
    target: Target; writtenAt: string | null; gameDay: number | null; stale: boolean;
    report: ProsperityReport | null;
    /** The towns' treasuries (accounts stores:<community>, D24). */
    towns: {community: string; treasury: number}[];
    /** The realm's own treasury (the account `treasury`), beside the towns' (none: not in the save). */
    treasury?: number | null;
    /** The price board's, the ships' and the wagons' goods by the catalogue's names. */
    goodNames?: Record<string, string>;
    placeNames?: Record<string, string>;     // The Country's farms and works by id (a farm cart's farm).
    week: {wagons: WeekWagon[]; ships: WeekShip[]; events: WeekEvent[]};
    players: {days: PlayerMoneyDay[]; holders: PlayerMoneyHolder[]};
    thefts: number;
    /** The word on the street (12): at most 20. */
    word: {kind: 'cause' | 'harvest' | 'ship' | 'event' | 'times' | 'slider'; text: string}[];
    /** 28 days: the DM's lever writes and acts (an act with its `state`: applied, refused, expired or queued), what the
     *  game made of them (the levers' economy.sync too), and the big moves of a price (over 0.2) with what made each. */
    log: {at: string; kind: string; text: string; state?: 'applied' | 'refused' | 'expired' | 'queued'}[];
}
/** A DM's live act on the staged economy (doc 79, 12): its kind, what it names, and its payload as the tool clamped it. */
export interface Queued { id: number; status: 'queued'; payload: Record<string, unknown> }

export const dmApi = {
    login: async (username: string, password: string) => { const r = await call<Me & {token: string}>('api/dm/login', {username, password}); setToken(r.token); return r as Me; },
    logout: async () => { try { await call('api/dm/logout', {}); } finally { setToken(''); } },
    me: () => call<Me>('api/dm/me'),
    worlds: () => call<Worlds>('api/dm/worlds'),
    players: (target: Target) => call<Players>(`${at(target)}players?target=${target}`),
    health: (target: Target, hours: number) => call<Health>(`${at(target)}health?target=${target}&hours=${hours}`),
    prosperity: (target: Target) => call<ProsperityView>(`${at(target)}prosperity?target=${target}`),
    /** A Times nudge, a harvest called, an event added or cancelled, a ship stormed, prices reset, a household's band for a
     *  while (target an NPC of it), or a player's grant or fine: queued for the game server; poll it with action(). */
    liveAct: (target: Target, kind: EconomyActKind, id: string, payload: Record<string, unknown> = {}, reason = '') =>
        call<Queued>(`${at(target)}prosperity/act`, {target, kind, id, payload, reason}),
    /** Lean: world cells come as outlines and previews; their ground is asked for as they come into view (dm/world.ts). */
    world: (target: Target) => call<Project>(`${at(target)}world?target=${target}&lean=1`),
    ground: (target: Target, ids: string[]) =>
        call<{seq?: number; cells: Record<string, SentGround>}>(`${at(target)}ground?target=${target}&cells=${ids.map(encodeURIComponent).join(',')}`),
    act: (target: Target, kind: string, characterId: string, reason: string, payload?: Record<string, unknown>) =>
        call<{id: number}>(`${at(target)}actions`, {target, kind, characterId, reason, ...(payload ? {payload} : {})}),
    npcs: (target: Target) => call<Npcs>(`${at(target)}npcs?target=${target}`),
    /** Asking keeps this account watching: the game server writes frames only while someone is. */
    live: (target: Target) => call<Live>(`${at(target)}live?target=${target}`),
    move: (target: Target, kind: 'npc.move' | 'character.move', id: string, place: Place, reason = '') =>
        call<{id: number}>(`${at(target)}live/move`, {target, kind, id, cell: place.cell, x: place.x, y: place.y, reason}),
    /** A temporary visitor on a tile for `minutes`, perhaps looking like a resident (`like`). */
    visit: (target: Target, name: string, place: Place, minutes: number, like = '') =>
        call<{id: number; visitor: string}>(`${at(target)}live/visit`, {target, name, cell: place.cell, x: place.x, y: place.y, minutes, like}),
    leave: (target: Target, id: string) => call<{id: number}>(`${at(target)}live/leave`, {target, id}),
    rumours: (target: Target) => call<{target: Target; rumours: Rumour[]}>(`${at(target)}live/rumours?target=${target}`),
    fame: (target: Target) => call<{target: Target; day: number; deeds: FameDeed[]}>(`${at(target)}live/fame?target=${target}`),
    projects: (target: Target) => call<{target: Target; projects: TownProject[]; kinds: {id: string; name: string}[]}>(`${at(target)}live/projects?target=${target}`),
    /** A live action on something other than a character (doc 57: town projects; deeds, nicknames, notices, places to let). */
    liveAction: (target: Target, kind: string, id: string, payload?: Record<string, unknown>, reason = '') =>
        call<{id: number}>(`${at(target)}live/action`, {target, kind, id, reason, ...(payload ? {payload} : {})}),
    /** `before`: the NPC as the page loaded it (null: a new one), so a change the game server made since to what the page
     *  left alone is kept (doc 66, Progress, "The live world's truth"); `written`, the NPC as saved, when it was. */
    saveNpc: (target: Target, person: Person, before?: Person | null) => call<{id: string; action: number; written?: Person}>(
        `${at(target)}npcs/save`, {target, person, ...(before !== undefined ? {before} : {})}),
    deleteNpc: (target: Target, id: string) => call<{id: string; action: number}>(`${at(target)}npcs/delete`, {target, id}),
    npcLife: (target: Target, id: string, dead: boolean) => call<{id: string; action: number}>(`${at(target)}npcs/life`, {target, id, dead}),
    saveRoute: (target: Target, route: Route) => call<{id: string; action: number}>(`${at(target)}routes/save`, {target, route}),
    deleteRoute: (target: Target, id: string) => call<{id: string; action: number}>(`${at(target)}routes/delete`, {target, id}),
    saveArea: (target: Target, area: NpcArea) => call<{id: string; action: number}>(`${at(target)}areas/save`, {target, area}),
    deleteArea: (target: Target, id: string) => call<{id: string; action: number}>(`${at(target)}areas/delete`, {target, id}),
    saveSpawn: (target: Target, rule: Omit<SpawnRule, 'alive' | 'dead'>) => call<{id: string}>(`${at(target)}spawns/save`, {target, rule}),
    deleteSpawn: (target: Target, id: string) => call<{id: string}>(`${at(target)}spawns/delete`, {target, id}),
    setWander: (target: Target, id: string, area: string | null) => call<{id: string; action: number}>(`${at(target)}npcs/wander`, {target, id, area}),
    factions: (target: Target) => call<Factions>(`${at(target)}factions?target=${target}`),
    relationHistory: (target: Target, faction: string, other: string) =>
        call<RelationChange[]>(`${at(target)}factions/history?target=${target}&faction=${encodeURIComponent(faction)}&other=${encodeURIComponent(other)}`),
    saveFaction: (target: Target, faction: Faction) => call<{id: string; action: number}>(`${at(target)}factions/save`, {target, faction}),
    deleteFaction: (target: Target, id: string) => call<{id: string; action: number}>(`${at(target)}factions/delete`, {target, id}),
    claim: (target: Target, faction: string, area: string, tiles: [number, number][]) => call<{action: number}>(`${at(target)}factions/claim`, {target, faction, area, tiles}),
    unclaim: (target: Target, faction: string, area: string) => call<{action: number}>(`${at(target)}factions/unclaim`, {target, faction, area}),
    relate: (target: Target, faction: string, other: string, disposition: number, stance: Stance, reason: string) =>
        call<unknown>(`${at(target)}factions/relation`, {target, faction, other, disposition, stance, reason}),
    member: (target: Target, faction: string, npc: string, rank: string | null) => call<unknown>(`${at(target)}factions/member`, {target, faction, npc, rank}),
    /** Someone's life from the event log, their story if one was written, and what they carry in mind (Phase 8). */
    chronicle: (target: Target, id: string) => call<Chronicle>(`${at(target)}chronicle?target=${target}&id=${encodeURIComponent(id)}`),
    /** Has their life story written from the chronicle (one call to the NPC model) and kept. */
    writeStory: (target: Target, id: string) => call<Chronicle>(`${at(target)}chronicle/story`, {target, id}),
    /** A host's Editor: asks the package to restart the world (doc 78, 6.5: the world saves, stops and starts again). */
    restart: () => call<{ok: boolean; restarting: boolean}>('api/world/restart', {}),
    /** A host's Editor: My world (doc 78, 2 and 7): status, settings, AI use, history, backups; the owner's own views. */
    myWorld: () => call<MyWorld>('api/world'),
    /** A host's Editor: a backup now (the package's backup service, doc 78, 12.1). */
    backupNow: () => call<{ok: boolean; asked: boolean}>('api/world/backup', {}),
    /** A host's Editor: pins one backup (kept 90 days, at most five) or unpins it. */
    pin: (name: string, pinned: boolean) => call<{ok: boolean; name: string; pinned: boolean}>('api/world/pin', {name, pinned}),
    /** A host's Editor: Share my world (doc 78, 10.5): a file of the world without its players, in manage/shared/. */
    shareWorld: () => call<{ok: boolean; asked: boolean}>('api/world/share', {}),
    /** A host's Editor: Publish (doc 78, 10.3, 12.3, 12.5; tools/world_publish.py): the count of unpublished map changes,
     *  the memory line and the job; Publish with how the world moves onto the new map; back to the previous map; discard
     *  the unpublished changes; open to everyone; the update rebuild's answer. */
    mapState: () => call<PublishState>('api/atlas/map'),
    mapPublish: (restart: Restart) => call<{ok: boolean; job: PublishJob}>('api/atlas/map/publish', {restart}),
    mapBack: (discard: boolean) => call<{ok: boolean; build: number; warnings?: PublishProblem[]}>('api/atlas/map/back', {discard}),
    mapDiscard: () => call<{ok: boolean; discarded: number; keys: number}>('api/atlas/map/discard', {}),
    mapOpen: () => call<{ok: boolean}>('api/atlas/map/open', {}),
    mapRebuild: (discard: boolean) => call<{ok: boolean; job: PublishJob}>('api/atlas/map/rebuild', {discard}),
    /** Whether the studio answers (a host's Editor, while its world restarts). */
    ping: () => call<unknown>('api/studio/ping'),
    /** A host's Editor: the AI's state (doc 78, 7). */
    ai: () => call<AiState>('api/studio/ai'),
    calendar: (target: Target) => call<WorldCalendar>(`${at(target)}calendar?target=${target}`),
    callFestival: (target: Target, community: string, name: string, inDays: number) =>
        call<{id: number}>(`${at(target)}festivals/call`, {target, community, name, inDays}),
    artwork: (target: Target) => call<Portraits>(`${at(target)}artwork?target=${target}`),
    reports: (target: Target) => call<Reports>(`${at(target)}reports?target=${target}`),
    storytellers: (target: Target) => call<Storytellers>(`${at(target)}storytellers?target=${target}`),
    // Talk (Docs/Design/62-talk.md, phase 6): a resident's voice and family, the dialects, figures.
    talk: (target: Target, npc: string) => call<Talk>(`${at(target)}talk?target=${target}&npc=${encodeURIComponent(npc)}`),
    saveVoice: (target: Target, npc: string, voice: Voice) => call<{id: number}>(`${at(target)}talk/voice`, {target, npc, voice}),
    saveKin: (target: Target, npc: string, other: string, kind: 'spouse' | 'parent', remove = false) =>
        call<{id: number}>(`${at(target)}talk/kin`, {target, npc, other, kind, remove}),
    dialects: (target: Target) => call<{name: string; grammar: string}[]>(`${at(target)}talk/dialects?target=${target}`),
    saveDialect: (target: Target, name: string, grammar: string) => call<{id: number}>(`${at(target)}talk/dialect`, {target, name, grammar}),
    figures: (target: Target) => call<Figure[]>(`${at(target)}talk/figures?target=${target}`),
    saveFigure: (target: Target, figure: Partial<Figure> & {id: string}, remove = false) => call<{id: number}>(`${at(target)}talk/figure`, {target, figure, remove}),
    saveStoryVisitor: (target: Target, visitor: {id: string; name: string; description: string; like: string; enabled: boolean}) =>
        call<{id: number}>(`${at(target)}storytellers/visitor`, {target, visitor}),
    creditMilestone: (target: Target, story: string, milestone: string, weight: string, rule: Record<string, unknown>, main: string[] = []) =>
        call<{id: number; people: {id: string; lines: string[]}[]}>(`${at(target)}milestone`, {target, story, milestone, weight, rule, main}),
    /** Upholds (with a note, a warning or a silence) or dismisses a player's report (doc 50). */
    decideReport: (target: Target, id: string, decision: 'uphold' | 'dismiss', outcome: string, hours: number, reason: string) =>
        call<{id: number}>(`${at(target)}reports/decide`, {target, id, decision, outcome, hours, reason}),
    reviewArtwork: (target: Target, id: string, decision: 'approve' | 'reject', reason: string) =>
        call<{id: number}>(`${at(target)}artwork/review`, {target, id, decision, reason}),
    chapters: (target: Target) => call<Chapters>(`${at(target)}chapters?target=${target}`),
    /** Approves or refuses a pending treaty (its ID) or House request (the Chapter's ID and the faction). */
    decide: (target: Target, what: 'treaty' | 'house', id: string, approve: boolean, faction = '', reason = '') =>
        call<{id: number}>(`${at(target)}chapters/decide`, {target, what, id, approve, faction, reason}),
    action: (target: Target, id: number) => call<{id: number; status: Action['status']; result: string}>(`${at(target)}actions/${id}?target=${target}`),
    /** The studio (tools/studio.py): who is signed in, the suite to open on, each suite's worlds. */
    studio: () => call<StudioSession>('api/studio/session'),
    /** Switches Atlas's world (a dev world), as `world_db.py use KEY` does. */
    useAtlasWorld: (key: Target) => call<{world: World}>('api/studio/atlas-world', {key}),
    /** Chooses the DM suite's world (any world), apart from Atlas's: as `world_db.py dm KEY` does. */
    useDmWorld: (key: Target) => call<{world: World}>('api/studio/dm-world', {key}),
    /** Starts a world's game server (tools/live.sh server KEY, on the local network) when none runs it. */
    startServer: (world: Target) => call<{started: boolean; starting: boolean; port?: number; note?: string}>('api/dm/server/start', {world}),
    // The server and imports (doc 66, section 4; tools/studio.py over tools/world_sync.py), on live worlds.
    /** A live world's state (running or maintenance), whether a game server holds it, its release; and the live choice. */
    serverStatus: (world?: Target) => call<ServerStatus>(`api/dm/server/status${world ? `?world=${encodeURIComponent(world)}` : ''}`),
    /** Closes a live world for maintenance (its players get a minute) or turns it back on (the launcher restarts it). */
    maintenance: (world: Target, on: boolean, note = '') => call<ServerStatus>('api/dm/server/maintenance', {world, on, note}),
    /** A dev world served as live: a new build of Atlas's edits, announced (its server restarts onto it once nobody is
     *  connected); the newest announced again when nothing changed. */
    rebuild: (world: Target) => call<BuildStatus & {made: boolean; cells: number}>('api/dm/server/rebuild', {world}),
    /** Switches the live server's world: during maintenance, with the world's key typed again as `confirm`. */
    switchLive: (world: Target, confirm: string) =>
        call<{live: Target; was: Target; name: string; server: ServerStatus['server']}>('api/dm/server/switch', {world, confirm}),
    /** The dev worlds copied from the live world, and what importing one (`dev`, else the first) would do. */
    importPlan: (world: Target, dev?: Target) => call<{live: SyncWorld; candidates: SyncWorld[]; dev: Target | null; plan: ImportPlan | null;
        problem: string; errors: string[]}>(`api/dm/import/plan?world=${encodeURIComponent(world)}${dev ? `&dev=${encodeURIComponent(dev)}` : ''}`),
    importDev: (body: {world: Target; dev: Target; choices: Record<string, Choice>; password: string; note: string; expect: string; replaceMap?: boolean}) =>
        call<ImportResult>('api/dm/import', body),
    releases: (world?: Target) => call<{world: Target; releases: Release[]}>(`api/dm/releases${world ? `?world=${encodeURIComponent(world)}` : ''}`),
    rollback: (world: Target, number: number, password: string, note: string) =>
        call<{release: number; summary: Release['summary']}>('api/dm/rollback', {world, number, password, note}),
    /** The economy's levers on the DM's world (Docs/Design/79-a-staged-economy.md, 12; tools/studio.py's /api/dm/economy):
     *  the slider and its cause, the residents' needs, the rules and services, the seasons, and (on a world at migration
     *  0061) the goods never imported and the host's status.json and market.json overrides. Each change is the world's own
     *  (live.economy), audited, and sent to its running game server (economy.sync). */
    economy: {
        get: (world: Target) => call<EconomyView>(`api/dm/economy?world=${encodeURIComponent(world)}`),
        steer: (world: Target, body: object) => call<EconomyReply>('api/dm/economy/steer', {world, ...body}),
        needs: (world: Target, body: object) => call<EconomyReply>('api/dm/economy/needs', {world, ...body}),
        rules: (world: Target, body: object) => call<EconomyReply>('api/dm/economy/rules', {world, ...body}),
        services: (world: Target, body: object) => call<EconomyReply>('api/dm/economy/services', {world, ...body}),
        seasons: (world: Target, body: object) => call<EconomyReply>('api/dm/economy/seasons', {world, ...body}),
        imports: (world: Target, body: object) => call<EconomyReply>('api/dm/economy/imports', {world, ...body}),
        status: (world: Target, body: object) => call<EconomyReply>('api/dm/economy/status', {world, ...body}),
        market: (world: Target, body: object) => call<EconomyReply>('api/dm/economy/market', {world, ...body}),
    },
};

/** One backup, as the package's backup service lists it (doc 78, 12.1): its name (ratw-STAMP-KIND), why it was made,
 *  when (seconds), its size, the package and migration level it came from, and its pin's end. */
export interface BackupItem { name: string; kind: string; at: number; size: number | null; package: string; migration: string; pinnedUntil: number | null }
/** The world's backups: the newest first, the pins and their cap, the backups' disk, the last problem (a refusal). */
export interface Backups {
    at: number | null; last: BackupItem | null; backups: BackupItem[]; pins: number; pinCap: number; working: string | null;
    problem: {code: 'disk' | 'failed' | 'pins' | 'database'; at: number | null; free: number | null} | null;
    disk: {free: number | null; used: number | null; minFree: number | null};
}
/** One shared file in the world folder's manage/shared/ (doc 78, 10.5): its name, size and when it was made. */
export interface SharedFile { name: string; size: number | null; at: number | null }
/** Share my world as the package last said (tools/world_admin.py's read_share): idle, being made, ready (its file) or
 *  failed (why), the files there, and the shares this hour against the most. */
export interface ShareState {
    state: 'idle' | 'working' | 'ready' | 'failed'; at: number | null; started: number | null; file: string | null; size: number | null;
    problem: {code: 'disk' | 'failed' | 'folder'; free: number | null; need: number | null; where: 'folder' | 'docker' | null} | null;
    files: SharedFile[]; kept: number; thisHour: number; perHour: number; waiting: boolean; snapshot: boolean;
}
/** My world (doc 78, 2 and 7; tools/studio.py's HostedStudio.my_world). `folder`, `exits` and addresses in `signIns`:
 *  the owner's alone. */
export interface MyWorld {
    world: string; name: string; package: string; status: ServerStatus; owner: boolean; fingerprint: string; wolves: number | null;
    settings: {player_limit: number | null; workers: number | null; cheats: 'off' | 'host' | 'everyone'; editor: {enabled: boolean}};
    ai: AiState; history: {at: number; action: string; target: string; detail: string; who: string}[];
    signIns: {at: number; account: string; detail: string}[]; backups: Backups | null; share?: ShareState | null;
    /** The map (doc 78, 10.3, "shown before the host starts"): the unpublished count and whether this computer can
     *  publish; null on a world without its map's source. */
    map?: MapSummary | null;
    folder?: string; exits?: {at: number | null; code: number | null; lines: string[]}[];
}
/** The host's tools' AI (doc 78, 7): whether it can be used now and why not, the day's calls against the caps, the
 *  month's tokens against the budget, and a rough cost a story (dollars). */
export interface AiState {
    available: boolean; reason: string; today: {story: number; roster: number}; caps: {story: number; roster: number};
    month: {tools: number; mind: number; budget: number} | null; costPerStory: number | null; model: string;
}
/** What the studio's top bar shows (doc 66, 3). Atlas is for dm and admin accounts (`available`); the DM shows the live
 *  worlds and the one the live server runs (`switch` says where it is switched: the DM's Server & import tab). A host's
 *  Editor (`hosted`, doc 78, 7) has one world and no pickers; Atlas says why it is greyed (`reason`). */
export interface StudioSession {
    me: Me; suite: 'atlas' | 'dm'; hosted?: boolean; myWorld?: boolean; ai?: AiState;
    atlas: {available: boolean; reason?: string; world: World; worlds: World[]};
    /** The DM's world (its own choice; else the live server's), the live server's world, and every world. */
    dm: {world: Target | null; live: Target | null; worlds: World[]; switch: string};
};
