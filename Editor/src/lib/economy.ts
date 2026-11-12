// The Economy page's and tab's settings (Docs/Design/79-a-staged-economy.md, 12 and 14; tools/studio.py's
// /api/atlas/economy and /api/dm/economy): the slider and its cause, the rules and services, the seasons, and (migration
// 0061) the host's overrides of status.json and market.json and the goods ships never carry. What the panels show and
// send, as pure functions (economy.test.ts). The view's shape is economyContract.ts's.
import type {EconomyReply, EconomyView} from './economyContract';

export type {EconomyReply, EconomyView};
/** A market.json service: its item's name and catalogue price (items.json), its business and how often it is bought. */
export interface ServiceListed { id: string; name: string; business?: string; price: number; every: number; for?: string }
export interface ServiceValues { on: boolean; price: number; every: number }
/** The view as the panels read it: the contract's, with the studio's extras (all optional). */
export type EconomyData = Omit<EconomyView, 'serviceList' | 'seasonGroups'> & {
    world?: string; steerNote?: string; heldRules?: string[]; seasonNames?: string[];
    serviceList: ServiceListed[];
    seasonGroups: (EconomyView['seasonGroups'][number] & {about?: string})[];
};

export type SteerBody = {prosperity: number; cause?: string | null; now?: boolean};
export type NeedsBody = {needsPaused: boolean};
export type RulesBody = {set: Record<string, number | null>};
export type ServicesBody = {set: Record<string, Partial<ServiceValues> | null>};
export type SeasonsBody = {set: Record<string, {price?: number[]; shelf?: number[]} | null>};
export type ImportsBody = {neverImported: string[] | null};
export type StatusBody = {status: Record<string, unknown> | null};
export type MarketBody = {market: Record<string, unknown> | null};

/** A whole number typed in, within low..high, or null. */
export function wholeOf(text: string, high: number, low = 0): number | null {
    const bare = text.trim().replace(/[%p]$/, '').trim();
    const n = bare === '' ? NaN : Number(bare);
    return Number.isInteger(n) && n >= low && n <= high ? n : null;
}

// -- the slider (doc 79, 12: -100 hard times to +100 boom, in steps of 5; clamped, never refused) ----------------------
/** The slider's word: about where the times sit at its mark (section 5.3's words). */
export function timesWord(v: number): 'hard' | 'lean' | 'ordinary' | 'good' | 'boom' {
    return v <= -60 ? 'hard' : v <= -20 ? 'lean' : v < 20 ? 'ordinary' : v < 60 ? 'good' : 'boom';
}
export const prosperityText = (v: number) => `${v > 0 ? '+' : ''}${v} (${timesWord(v)} times)`;
/** The slider as the host clamps it: a number to -100..100 in fives, anything else 0. */
export function clampProsperity(v: unknown): number {
    if (typeof v !== 'number' || !Number.isFinite(v)) return 0;
    return Math.floor(Math.max(-100, Math.min(100, v)) / 5 + .5) * 5;
}
/** The steer's body (null: nothing to change). `cause` "" is the world's own pick; undefined leaves it. */
export function steerChange(d: EconomyData, value: number, cause?: string, now = false): SteerBody | null {
    const v = clampProsperity(value);
    const same = v === d.prosperity.value && (cause === undefined || cause.trim() === d.prosperity.cause);
    if (same && !now) return null;
    return {prosperity: v, ...(cause === undefined ? {} : {cause: cause.trim()}), ...(now ? {now: true} : {})};
}
/** A cause typed in: at most 80 bytes, one line, never the forbidden word; the problem, or '' when it is fine. */
export function causeProblem(text: string): string {
    if (new TextEncoder().encode(text.trim()).length > 80) return 'A cause is at most 80 letters.';
    if (/[\u0000-\u001f\u007f]/.test(text)) return 'A cause is one line.';
    if (/(^|[^a-z])dogs?($|[^a-z])/i.test(text)) return 'Wolves, not that word.';
    return '';
}

// -- the rules -------------------------------------------------------------------------------------------------------
/** What the page calls each of Core's rules (Rules[]; the host lists them in ruleList, prosperity apart). A rule the host
 *  lists without words here is shown by its name. */
export const RULE_WORDS: Record<string, {label: string; unit: string}> = {
    neverHungry: {label: 'Never Hungry', unit: 'no household has short days, whatever its band'},
    neverBroke: {label: 'Never Broke', unit: 'no household is destitute'},
    buyAheadDays: {label: 'Buying ahead', unit: 'days of food more a household is given the day before a Restday or festival'},
    halfLifePerishable: {label: 'Half-life, perishable food', unit: 'tenths of a day a good\'s pressure takes to halve'},
    halfLifeKeeping: {label: 'Half-life, keeping food', unit: 'tenths of a day'},
    halfLifeDurable: {label: 'Half-life, the rest', unit: 'tenths of a day'},
    ripple: {label: 'The ripple', unit: '% of a good\'s pressure passed to its inputs and outputs a step, each morning'},
    depthScale: {label: 'Market depth', unit: '% on every market\'s depth (deeper: prices move less)'},
    playerPushMost: {label: 'One holder\'s push, at most', unit: 'hundredths on a good\'s pressure'},
    playerSellMost: {label: 'One holder\'s sales a day, at most', unit: 'p the shops pay one account holder a day'},
    takeAllowance: {label: 'The take allowance', unit: '% of a market\'s depth its shops take of a good a day'},
    shelfScale: {label: 'Shelves', unit: '% on every shelf\'s target'},
    factionDiscountMost: {label: 'The faction discount, at most', unit: 'p a Chapter a week'},
    caravanRate: {label: 'Wagons', unit: 'wagons on the roads, 0-300% of normal'},
    wagonsOnRoad: {label: 'Wagons on the roads, at most', unit: 'at once'},
    cartsOnRoad: {label: 'Farm carts, at most', unit: 'on the farm tracks at once'},
    campStrengthMost: {label: 'A bandit camp\'s strength, at most', unit: 'points'},
    bountyMost: {label: 'A bounty, at most', unit: 'p on a camp'},
    shipsPerWeek: {label: 'Ships a week', unit: 'at a port, one more or fewer by the season and the times; 0 means no ships even with a port'},
    theftCap: {label: 'Thefts a day, at most', unit: 'in each market'},
    errandRunners: {label: 'Errand-runners', unit: 'a household member walks to the shop where a player is near'},
    eventRate: {label: 'Staged events', unit: '% of the markets\' own (a wedding, a fire, a good catch: 1-3 a market a week at 100)'},
    overrideNotices: {label: 'Override notices', unit: 'a notice on the boards for each Times nudge or harvest call'},
    harvestBias: {label: 'Harvest bias', unit: 'hundredths on every area\'s harvest roll, -20 to 20 (+: better harvests)'},
};
export type RuleListed = EconomyView['ruleList'][number];
export const ruleWords = (r: RuleListed) => RULE_WORDS[r.name] ?? {label: r.name, unit: ''};
/** A rule as it stands: the world's own, else the data files'. */
export const ruleValue = (d: EconomyData, r: RuleListed): number => d.economy.rules[r.name] ?? r.fallback;
/** The body that sets a rule (null: back to the data files'); null when it changes nothing. */
export function ruleChange(d: EconomyData, r: RuleListed, value: number | null): RulesBody | null {
    const own = d.economy.rules[r.name];
    if (value === null) return own === undefined ? null : {set: {[r.name]: null}};
    return (own ?? r.fallback) === value ? null : {set: {[r.name]: value}};
}
/** A rule typed in: "" (back to the data files') is null, else a whole number within low..high; undefined if neither. */
export function ruleOf(text: string, high: number, low = 0): number | null | undefined {
    if (text.trim() === '') return null;
    const n = wholeOf(text, high, low);
    return n === null ? undefined : n;
}
/** The rules the world keeps that the game no longer has (kept, written as they are; the game reads them past). */
export const heldRules = (d: EconomyData): string[] => d.heldRules
    ?? Object.keys(d.economy.rules).filter(k => k !== 'prosperity' && !d.ruleList.some(r => r.name === k));

// -- the services ----------------------------------------------------------------------------------------------------
/** A service as it stands: the world's own, else market.json's (on, at its catalogue price and frequency). */
export function serviceValue(d: EconomyData, s: ServiceListed): ServiceValues {
    return d.economy.services[s.id] ?? {on: true, price: s.price, every: s.every};
}
/** The body that changes one of a service's three; null when it is what the service has already. */
export function serviceChange(d: EconomyData, s: ServiceListed, key: keyof ServiceValues, value: boolean | number): ServicesBody | null {
    return serviceValue(d, s)[key] === value ? null : {set: {[s.id]: {[key]: value}}};
}
/** How a service reads: "barber, 1p, about once every 4 days a household with money to spare (off)". */
export function serviceText(d: EconomyData, s: ServiceListed, shipped = 'market.json\'s'): string {
    const v = serviceValue(d, s);
    return `${s.business ?? ''}, ${v.price}p, about once every ${v.every} day${v.every === 1 ? '' : 's'} a household with money to spare`
        + (v.on ? '' : ' (off)') + (d.economy.services[s.id] ? '' : ` (${shipped})`);
}

// -- the seasons (doc 79, 7.1: price and abundance by group, market.json's with the world's changes) -----------------
export type SeasonRow = 'price' | 'shelf';
export const SEASON_ROWS: {key: SeasonRow; label: string; low: number; high: number; hint: string}[] = [
    {key: 'price', label: 'Price', low: 10, high: 1000, hint: '% on the Market Board\'s prices of these goods (doc 79, 7.1)'},
    {key: 'shelf', label: 'Shelf', low: 0, high: 1000, hint: '% of their usual shelf the shops keep of these goods (stocking up)'},
];
/** Four percents typed in ("50 100 150 40", "50/100/150/40", "50, 100, ..."), each within low..high, or null. */
export function percentsOf(text: string, low: number, high: number): number[] | null {
    const parts = text.trim().split(/[\s,/]+/).filter(Boolean).map(p => p.replace(/%$/, ''));
    if (parts.length !== 4) return null;
    const n = parts.map(Number);
    return n.every(v => Number.isInteger(v) && v >= low && v <= high) ? n : null;
}
export const percentsText = (values: number[]) => values.join(' / ');
/** Core's seasonal blend: a group's percent on a day of the year (0 at Spring's first day), each season's value at its
 *  middle day (46, 138, 229.5, 320) and a straight line between, across the year's turn too; as a fraction. */
export function seasonal(values: number[], yearDay: number): number {
    const middle = [46, 138, 229.5, 320];
    const d = ((yearDay % 365) + 365) % 365;
    let a = 3;
    for (let i = 0; i < 4; ++i) if (d >= middle[i]) a = i;
    const b = (a + 1) % 4;
    const from = middle[a], to = b === 0 ? middle[0] + 365 : middle[b];
    const at = d < from ? d + 365 : d;
    const t = Math.min(1, Math.max(0, (at - from) / (to - from)));
    return (values[a] + (values[b] - values[a]) * t) / 100;
}
export type SeasonGroupView = EconomyData['seasonGroups'][number];
/** A group as the world has it: its own price and shelf (a world before 0061 keeps four parts: supply and normal read
 *  past), else market.json's; and whether it is changed here. */
export function seasonNow(d: EconomyData, g: SeasonGroupView): {price: number[]; shelf: number[]; changed: boolean} {
    const own = d.economy.seasons[g.id] as {price?: number[]; shelf?: number[]} | undefined;
    return {price: own?.price ?? g.price, shelf: own?.shelf ?? g.shelf, changed: own !== undefined};
}
/** The body that changes one row of a group; null when it is what the group has already. */
export function seasonChange(d: EconomyData, g: SeasonGroupView, key: SeasonRow, value: number[]): SeasonsBody | null {
    const now = seasonNow(d, g)[key];
    return now.length === value.length && now.every((v, i) => v === value[i]) ? null : {set: {[g.id]: {[key]: value}}};
}

// -- the host's overrides (migration 0061): the goods ships never carry, status.json's and market.json's parts -------
/** The goods ships never carry here: the world's list, else market.json's wharf.never. */
export const neverNow = (d: EconomyData): string[] => d.economy.overrides.neverImported ?? d.defaults.market.wharf.never;
/** The body that turns one good's "never carried" on or off; null when it changes nothing. */
export function neverChange(d: EconomyData, good: string, never: boolean): ImportsBody | null {
    const now = neverNow(d);
    if (now.includes(good) === never) return null;
    return {neverImported: never ? [...now, good] : now.filter(g => g !== good)};
}
/** The goods the panel lists under a search ("" for all). */
export function goodsShown(goods: EconomyView['goods'], search: string): EconomyView['goods'] {
    const q = search.trim().toLowerCase();
    return q ? goods.filter(g => g.id.includes(q) || g.name.toLowerCase().includes(q)) : goods;
}

type Json = Record<string, unknown>;
const object = (v: unknown): v is Json => typeof v === 'object' && v !== null && !Array.isArray(v);
/** The world's status.json override (empty: the file's). */
export const statusOverride = (d: EconomyData): Json => (object(d.economy.overrides.status) ? d.economy.overrides.status : {}) as Json;
/** A status.json part as it stands: the world's own (an object merged over the file's), else the file's. */
export function statusPart(d: EconomyData, key: string): unknown {
    const own = statusOverride(d)[key], file = (d.defaults.status as Json)[key];
    return own === undefined ? file : object(own) && object(file) ? {...file, ...own} : own;
}
/** The body that sets one part of the status override (null: back to the file's for that part). */
export function statusChange(d: EconomyData, key: string, value: unknown | null): StatusBody {
    const next: Json = {...statusOverride(d)};
    if (value === null || value === undefined) delete next[key];
    else next[key] = value;
    return {status: Object.keys(next).length ? next : null};
}
export type ShareRow = {at: number; shares: number[]};
/** The bands' shares as lines "at: destitute poor comfortable rich" ("0: 5 25 60 10"). */
export const sharesText = (rows: ShareRow[]) => rows.map(r => `${r.at}: ${r.shares.join(' ')}`).join('\n');
/** Lines typed in as share rows, or the problem: two or more, -100 to 100 rising, each four shares of at least 2 summing
 *  to 100, destitute never rising and rich never falling (Core's sharesValid). */
export function sharesOf(text: string): ShareRow[] | string {
    const rows: ShareRow[] = [];
    for (const line of text.split('\n').map(l => l.trim()).filter(Boolean)) {
        const m = line.match(/^(-?\d+)\s*:\s*(.+)$/);
        const shares = m ? m[2].split(/[\s,/]+/).filter(Boolean).map(Number) : [];
        if (!m || shares.length !== 4 || !shares.every(Number.isFinite)) return `"${line}": a line is "at: four shares".`;
        rows.push({at: Number(m[1]), shares});
    }
    if (rows.length < 2 || rows[0].at !== -100 || rows[rows.length - 1].at !== 100) return 'The rows run from -100 to 100 (two at least).';
    for (const [i, r] of rows.entries()) {
        if (r.shares.some(s => s < 2)) return `At ${r.at}: every share is at least 2%.`;
        const sum = r.shares.reduce((a, b) => a + b, 0);
        if (Math.abs(sum - 100) > 1e-6) return `At ${r.at}: the shares sum to ${sum}, not 100.`;
        if (i && r.at <= rows[i - 1].at) return 'The rows run in rising order.';
        if (i && r.shares[0] > rows[i - 1].shares[0]) return `At ${r.at}: the destitute share never rises as the slider rises.`;
        if (i && r.shares[3] < rows[i - 1].shares[3]) return `At ${r.at}: the rich share never falls as the slider rises.`;
    }
    return rows;
}

/** The world's market.json override (empty: the file's). */
export const marketOverride = (d: EconomyData): Json => (object(d.economy.overrides.market) ? d.economy.overrides.market : {}) as Json;
const eventsOf = (d: EconomyData): Json => (object(marketOverride(d).events) ? marketOverride(d).events : {}) as Json;
/** The staged event kinds on in this world (the world's list, else market.json's), by id. */
export function kindsOn(d: EconomyData): string[] {
    const list = eventsOf(d).list;
    if (!Array.isArray(list)) return d.defaults.market.events.list.map(k => k.id);
    return list.map(k => typeof k === 'string' ? k : object(k) && typeof k.id === 'string' ? k.id : '').filter(Boolean);
}
/** The market override with one path set (null: back to the file's for it); empty parts dropped. */
export function marketChange(d: EconomyData, part: 'events' | 'wharf', key: string, value: unknown | null): MarketBody {
    const next: Json = JSON.parse(JSON.stringify(marketOverride(d)));
    const inner: Json = object(next[part]) ? next[part] as Json : {};
    if (value === null || value === undefined) delete inner[key];
    else inner[key] = value;
    if (Object.keys(inner).length) next[part] = inner;
    else delete next[part];
    return {market: Object.keys(next).length ? next : null};
}
/** The body that turns one staged event kind on or off. The kinds stay in market.json's order; a host's own kind (one the
 *  file hasn't) is kept as the world has it. All of the file's on and none of its own: back to the file's list. */
export function kindChange(d: EconomyData, id: string, on: boolean): MarketBody | null {
    const now = kindsOn(d);
    if (now.includes(id) === on) return null;
    const own = (Array.isArray(eventsOf(d).list) ? eventsOf(d).list as unknown[] : [])
        .filter(k => object(k) && !d.defaults.market.events.list.some(f => f.id === k.id));
    const wanted = new Set(on ? [...now, id] : now.filter(k => k !== id));
    const shipped = d.defaults.market.events.list.map(k => k.id).filter(k => wanted.has(k));
    const kept = own.filter(k => wanted.has((k as Json).id as string));
    if (!kept.length && shipped.length === d.defaults.market.events.list.length) return marketChange(d, 'events', 'list', null);
    return marketChange(d, 'events', 'list', [...shipped, ...kept]);
}
/** The ships' names as the world has them (else market.json's). */
export function shipsNow(d: EconomyData): string[] {
    const wharf = marketOverride(d).wharf;
    return object(wharf) && Array.isArray(wharf.ships) ? wharf.ships as string[] : d.defaults.market.wharf.ships;
}
/** Names typed one a line, or the problem: 1-64, distinct, letters, spaces, ' and - (at most 40 each). */
export function shipsOf(text: string): string[] | string {
    const names = text.split('\n').map(l => l.trim()).filter(Boolean);
    if (!names.length || names.length > 64) return 'One to 64 names, one a line.';
    if (new Set(names).size !== names.length) return 'Each name once.';
    const bad = names.find(n => n.length > 40 || !/^[A-Za-z '-]+$/.test(n) || /dog/i.test(n));
    return bad ? `"${bad}": letters, spaces, ' and - only, at most 40.` : names;
}
/** The staged events a market sees a week (the world's, else market.json's). */
export function perWeekNow(d: EconomyData): number {
    const v = eventsOf(d).perWeek;
    return typeof v === 'number' ? v : d.defaults.market.events.perWeek;
}

/** An hour of the game's day with its minutes ("11:30": a seeded event at 11.5 isn't 11:00). The Prosperity tab's. */
export const clock = (hour: number) => {
    const minutes = Math.round((((hour % 24) + 24) % 24) * 60);
    return `${String(Math.floor(minutes / 60) % 24).padStart(2, '0')}:${String(minutes % 60).padStart(2, '0')}`;
};
/** A day the event form offers: today, or day N, with "(next week)" from next week's first day (the report's
 *  nextWeek; weekDays holds this week and the next, so it can't tell them apart). */
export const dayLabel = (day: number, today: number | null, nextWeek: number | null) =>
    (day === today ? 'today' : `day ${day}`) + (nextWeek !== null && day >= nextWeek ? ' (next week)' : '');
