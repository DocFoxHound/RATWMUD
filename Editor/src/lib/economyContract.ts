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
    lastSync?: LastSync | null;
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
/** How a host is told of the last economy.sync (Phase 9's last repair: no screen showed it). */
export function syncWords(last: LastSync | null | undefined): string {
    if (!last) return '';
    const what = {queued: 'waiting for the game server (none has taken it yet)', applied: 'applied',
                  refused: 'refused by the game server', expired: 'never applied: no game server took it (one started later reads the world\'s row)'}[last.status]
                 ?? last.status;
    // (Core's result names what was skipped; an expiry's is the same words as `what`.)
    const more = last.result && last.status !== 'expired' ? ` ${last.result}` : '';
    return `The last change sent to the game server (#${last.id}${last.by ? `, by ${last.by}` : ''}): ${what}.${more}`;
}

/** The Prosperity report the game writes with its checkpoint (World::prosperityReport). */
export type ProsperityReport = {
    v: 1; day: number; at: number;
    prosperity: {slider: number; target: number; cause: string; by: 'dm' | 'seeded' | ''; neverHungry: boolean; neverBroke: boolean};
    markets: {id: string; name: string; times: {word: 'hard' | 'lean' | 'ordinary' | 'good' | 'boom'; score: number; cause: string};
              households: Record<Band, number>; overridden: number; hungry: number; short: number; thieves: number;
              /** The most beggars out at once on the last begging day (the report's `beggarsDay`): everyone placed, on
               *  Restday the working poor on their day off too, each under its own household's market, so never over that
               *  market's most (status.json's street.beggarsInAMarket). */
              beggars: number; dole: number; nudge: {points: number; until: number; words: string} | null;
              resetting: {until: number} | null}[];
    pressures: {market: string; good: string; P: number; cause: string}[];
    areas: {id: string; name: string; harvest: 'poor' | 'fair' | 'good'; override: {word: string; until: number; words: string} | null}[];
    eventKinds: EventKindListed[]; weekDays: number[];
    /** The last day anyone could beg (a Marketday or a Restday) that the server saw: `beggars` are that day's; -1 or
     *  absent: none seen yet. */
    beggarsDay?: number;
    /** Next calendar week's first day (weekDays from it on are next week's). */
    nextWeek?: number;
    /** The DM's bands for a while in force, by household, with their end and members. */
    bandOverrides?: {household: string; band: Band; until: number; members: string[]}[];
};
/** The DM's acts on the staged economy (dm.actions kinds; Core's Game::applyEconomyCommand): the one list of them on this
 *  side. tools/test_dungeon_master.py holds it to the tool's LIVE_ACTS and to Core's Game::economyCommand (Phase 9's last
 *  repair: the kinds were written out by hand in each place). */
export const ECONOMY_ACT_KINDS = ['times.nudge', 'harvest.override', 'event.add', 'event.cancel', 'ship.storm', 'prices.reset',
    'band.override', 'character.grant'] as const;
export type EconomyActKind = typeof ECONOMY_ACT_KINDS[number];
/** The acts on the world (every one but a player's grant or fine). */
export type LiveActKind = Exclude<EconomyActKind, 'character.grant'>;
export const LIVE_ACT_KINDS = ECONOMY_ACT_KINDS.filter((k): k is LiveActKind => k !== 'character.grant');
/** What the game server made of the last economy.sync (the levers' own act): Core's words, with the records it skipped,
 *  a refusal's reason, or "expired" when no server took it. */
export type LastSync = {id: number; status: 'queued' | 'applied' | 'refused' | 'expired'; result: string; by: string; at: string};

/** A faucet's or a sink's reason in the players' hover: coin made and sunk, and goods in and out at the catalogue's price
 *  (a reason that was all goods read "+0 -0": Phase 9's last repair). */
export const reasonWords = (reason: string, [made = 0, sunk = 0, goodsIn = 0, goodsOut = 0]: (number | undefined)[]): string =>
    `${reason}: ${[made || sunk || !(goodsIn || goodsOut) ? `+${made}p -${sunk}p` : '',
                   goodsIn || goodsOut ? `goods +${goodsIn}p -${goodsOut}p` : ''].filter(Boolean).join(', ')}`;
