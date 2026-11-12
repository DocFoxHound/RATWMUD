/** The staged economy's DM and Atlas contract (Docs/Design/79-a-staged-economy.md, 12 and 14; Phase 9), as the studio's
 *  /api/economy (and /api/dm/economy) routes reply and the DM tool's /api/prosperity: tools/economy_contract.py's
 *  constants and the shapes of world_store's economy entity. */

export const BANDS = ['destitute', 'poor', 'comfortable', 'rich'] as const;
export type Band = typeof BANDS[number];
export const PROSPERITY = {low: -100, high: 100, step: 5} as const;
export const CAUSE_MOST = 80;
export const HARVEST_WORDS = ['poor', 'fair', 'good'] as const;
export const GRANT_MOST = 2500;

export type EconomyOverrides = {prosperityCause?: string; neverImported?: string[]; status?: object; market?: object};
export type EconomyEntity = {
    treasury: number; storeHerbs: number; storeMeals: number; needsPaused: boolean;
    seasons: Record<string, {price: number[]; shelf: number[]}>; rules: Record<string, number>;
    services: Record<string, {on: boolean; price: number; every: number}>;
    overrides: EconomyOverrides;
};
export type EventKindListed = {id: string; words: string; needs: string[]};
export type EconomyView = {
    supported: boolean; slim: boolean; problem: string; note: string; takesEffect: string; worldInfo: unknown;
    economy: EconomyEntity;
    prosperity: {value: number; cause: string; low: -100; high: 100; step: 5};
    ruleList: {name: string; low: number; high: number; fallback: number; switch: boolean}[];   // RULES less prosperity
    serviceList: {id: string; name: string; price: number; every: number}[];
    seasonGroups: {id: string; name: string; goods: string[]; price: number[]; shelf: number[]}[];  // market.json's
    goods: {id: string; name: string}[];                      // market.json's base goods (for neverImported)
    defaults: {status: object; market: {events: {perWeek: number; list: EventKindListed[]; causes: object};
                                        wharf: {ships: string[]; never: string[]}}};
};
/** What every POST under /api/economy replies. */
export type EconomyReply = EconomyView & {changed: boolean; detail: string; sync: number | null; warnings: string[]};

/** The Prosperity report the game writes with its checkpoint (World::prosperityReport). */
export type ProsperityReport = {
    v: 1; day: number; at: number;
    prosperity: {slider: number; target: number; cause: string; by: 'dm' | 'seeded' | ''; neverHungry: boolean; neverBroke: boolean};
    markets: {id: string; name: string; times: {word: 'hard' | 'lean' | 'ordinary' | 'good' | 'boom'; score: number; cause: string};
              households: Record<Band, number>; overridden: number; hungry: number; short: number; thieves: number;
              beggars: number; dole: number; nudge: {points: number; until: number; words: string} | null;
              resetting: {until: number} | null}[];
    pressures: {market: string; good: string; P: number; cause: string}[];
    areas: {id: string; name: string; harvest: 'poor' | 'fair' | 'good'; override: {word: string; until: number; words: string} | null}[];
    eventKinds: EventKindListed[]; weekDays: number[];
    /** Next calendar week's first day (weekDays from it on are next week's). */
    nextWeek?: number;
    /** The DM's bands for a while in force, by household, with their end and members. */
    bandOverrides?: {household: string; band: Band; until: number; members: string[]}[];
};
/** The DM's live acts (dm.actions kinds; Core's Game::applyEconomyCommand). */
export const LIVE_ACT_KINDS = ['times.nudge', 'harvest.override', 'event.add', 'event.cancel', 'ship.storm', 'prices.reset',
    'band.override'] as const;
export type LiveActKind = typeof LIVE_ACT_KINDS[number];
