// Atlas's Economy controls (Docs/Design/67-ridgemere2-city.md, 6; tools/studio.py's /api/atlas/economy): the store
// room's switches for the world and each business. What the page shows and sends, as pure functions (economy.test.ts).

/** A good a business would draw: its catalogue price and what one costs at the world's markup for its kind. */
export interface StoreGood { item: string; price: number; cost: number }
export type Effective = 'on' | 'off (this business)' | 'off (whole world)' | 'draws nothing';
/** A placed merchant's business (by its keeper, the founder: position job:<npc>). */
export interface StoreBusiness {
    npc: string; name: string; label: string; business: string | null; district: string; interior: string;
    making: StoreGood[]; resale: StoreGood[]; internalStore: boolean; effective: Effective; why: string;
}
export interface StoreSwitches { internalStore: boolean; storeMarkup: number; storeResale: boolean; resaleMarkup: number; storeRule: boolean }
/** The city's loop (doc 68, 9): the residents' needs, the Country, the gate toll, carriage, wages, the subsidies. */
export interface LoopSettings {
    needsPaused: boolean; wharf: boolean; wharfReserve: number; gateToll: number; carriage: number;
    wageKeeper: number; wageHand: number; wagePublic: number; wageWharf: number; subsidies: Record<string, number>;
}
/** A good of the Country's basket (Data/Items/city_loop.json): its share of the Country's spending and its makers here. */
export interface BasketGood { item: string; name: string; share: number; percent: number; price: number; makers: string[] }
/** A good some business here draws, and whether the Country supplies it (off: its real producers'). */
export interface WharfGood { item: string; name: string; businesses: number; supplied: boolean }
export interface EconomyControls extends StoreSwitches, SeasonControls, RuleControls, FarmControls {
    world: string; supported: boolean; problem: string; note: string; takesEffect: string; businesses: StoreBusiness[];
    worldInfo?: {key: string; name: string; kind: string; upstream?: string};
    loop: LoopSettings; wharfOff: string[]; loopSupported: boolean; loopProblem: string; ruleWarning: string;
    market: {id: string; name: string; people: string}; basket: BasketGood[]; goods: WharfGood[];
    /** The Wharf's lists (Data/Items/city_loop.json `wharf`; doc 73, 4.2): imports and takes by good, and its never list. */
    wharfLists?: {imports: Record<string, {most?: number; on?: boolean}>; takes: Record<string, {most?: number; on?: boolean}>; never: string[]};
}

// The seasons in the Country's supply and the residents' comforts (Docs/Design/68-the-country-and-the-city-loop.md,
// Progress, "Seasons and the DM's rules"; migration 0057): market.json's season groups (doc 79, 7.1) with this world's changes.
/** A group's numbers: percent by season (Spring, Summer, Autumn, Winter), and its normal pieces a day. Core reads price
 *  and shelf; supply and normal, the city loop's, are kept in the record unread until doc 79's Phase 9. */
export interface SeasonValues { supply: number[]; price: number[]; shelf: number[]; normal: number }
/** A season group as the world has it (`changed`: away from market.json's, which `file` holds). */
export interface SeasonGroup extends SeasonValues { id: string; name: string; about: string; goods: string[]; changed: boolean; file: SeasonValues }
/** The comforts (null: city_loop.json's, `fileAbove` pennies a head and `fileShare` percent). */
export interface Comforts { comfortsAbove: number | null; comfortsShare: number | null; fileAbove: number; fileShare: number }
export interface SeasonControls {
    seasonsSupported: boolean; seasonsProblem: string; seasonNames: string[]; seasons: SeasonGroup[]; comforts: Comforts;
}
export type SeasonsBody = {set: Record<string, Partial<SeasonValues> | null>};
export type ComfortsBody = Partial<Pick<Comforts, 'comfortsAbove' | 'comfortsShare'>>;
export type SeasonRow = 'supply' | 'price' | 'shelf';
/** The three rows of a group, their limits and what the page calls them. */
export const SEASON_ROWS: {key: SeasonRow; label: string; low: number; high: number; hint: string}[] = [
    {key: 'supply', label: 'Supply', low: 0, high: 1000, hint: 'Not read since the staged economy (doc 79): kept in the world\'s record until Phase 9 reshapes it'},
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
/** Core's Society::seasonal: a group's percent on a day of the year (0 at Spring's first day), each season's value at
 *  its middle day (46, 138, 229.5, 320) and a straight line between, across the year's turn too; as a fraction. */
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
/** The body that changes one row (or the normal) of a group; null when it is what the group has already. */
export function seasonChange(group: SeasonGroup, key: SeasonRow | 'normal', value: number[] | number): SeasonsBody | null {
    const now = group[key];
    const same = Array.isArray(value) ? Array.isArray(now) && now.every((v, i) => v === value[i]) : now === value;
    return same ? null : {set: {[group.id]: {[key]: value}}};
}
/** How the comforts read: the world's own, or city_loop.json's. */
export function comfortsText(c: Comforts): string {
    const above = c.comfortsAbove ?? c.fileAbove, share = c.comfortsShare ?? c.fileShare;
    const whose = c.comfortsAbove === null && c.comfortsShare === null ? ' (city_loop.json\'s)' : '';
    return `A household holding more than ${above}p a head spends ${share}% of what is over it a day on comforts${whose}.`;
}
/** A comforts value typed in: "" (back to city_loop.json's) is null, else a whole number within 0..max; undefined if neither. */
export function comfortOf(text: string, max: number): number | null | undefined {
    if (text.trim() === '') return null;
    const n = wholeOf(text, max);
    return n === null ? undefined : n;
}
export type GoodsBody = {set: Record<string, boolean>} | {all: boolean};

// The loop's rules and the city's services (Docs/Design/68-the-country-and-the-city-loop.md, Progress, "First fixes";
// migration 0058): this world's changes ({} for none) beside Data/Items/city_loop.json's.
/** A service as a world has it (stored whole once changed). */
export interface ServiceValues { on: boolean; price: number; every: number }
/** A market.json service: its item's name and catalogue price (items.json), its business and how often it is bought. */
export interface ServiceListed { id: string; name: string; business: string; price: number; every: number; for?: string }
export interface RuleControls {
    rulesSupported: boolean; rulesProblem: string; rules: Record<string, number>; services: Record<string, ServiceValues>;
    ruleDefaults: Record<string, number>; serviceList: ServiceListed[];
}
export type RulesBody = {set: Record<string, number | null>};
export type ServicesBody = {set: Record<string, Partial<ServiceValues> | null>};
/** The rules the page lists (Core's items::Rules since doc 79, 17: the clamps and the Market Board's), their limits,
 *  whether each is an on/off switch, and what the page calls them. A world's rule not listed here is one the game no
 *  longer has: kept and ignored. */
export const RULES: {key: string; label: string; low?: number; max: number; unit: string; onOff?: boolean}[] = [
    // The clamps (doc 79, 6.4).
    {key: 'neverHungry', label: 'Never Hungry', max: 1, unit: 'no household has short days, whatever its band', onOff: true},
    {key: 'neverBroke', label: 'Never Broke', max: 1, unit: 'no household is destitute', onOff: true},
    {key: 'buyAheadDays', label: 'Buying ahead', max: 5, unit: 'days of food more a household is given the day before a Restday or festival'},
    // The Market Board (doc 79, 5 and 12's pressure settings).
    {key: 'halfLifePerishable', label: 'Half-life, perishable food', low: 5, max: 300, unit: 'tenths of a day a good\'s pressure takes to halve'},
    {key: 'halfLifeKeeping', label: 'Half-life, keeping food', low: 5, max: 300, unit: 'tenths of a day'},
    {key: 'halfLifeDurable', label: 'Half-life, the rest', low: 5, max: 300, unit: 'tenths of a day'},
    {key: 'ripple', label: 'The ripple', max: 70, unit: '% of a good\'s pressure passed to its inputs and outputs a step, each morning'},
    {key: 'depthScale', label: 'Market depth', low: 10, max: 1000, unit: '% on every market\'s depth (deeper: prices move less)'},
    {key: 'playerPushMost', label: 'One holder\'s push, at most', max: 50, unit: 'hundredths on a good\'s pressure'},
    {key: 'playerSellMost', label: 'One holder\'s sales a day, at most', max: 5000, unit: 'p the shops pay one account holder a day'},
    {key: 'takeAllowance', label: 'The take allowance', low: 5, max: 100, unit: '% of a market\'s depth its shops take of a good a day'},
    {key: 'shelfScale', label: 'Shelves', low: 10, max: 1000, unit: '% on every shelf\'s target'},
    {key: 'factionDiscountMost', label: 'The faction discount, at most', max: 1000, unit: 'p a Chapter a week'},
    // Staged trade (doc 79, 8.1-8.2 and D14).
    {key: 'caravanRate', label: 'Wagons', max: 300, unit: '% of the roads\' own wagons between markets'},
    {key: 'wagonsOnRoad', label: 'Wagons on the roads, at most', max: 64, unit: 'at once'},
    {key: 'cartsOnRoad', label: 'Farm carts, at most', max: 200, unit: 'on the farm tracks at once'},
    {key: 'campStrengthMost', label: 'A bandit camp\'s strength, at most', low: 2, max: 20, unit: 'points'},
    {key: 'bountyMost', label: 'A bounty, at most', max: 500, unit: 'p on a camp'},
    // The port (doc 79, 8.3; Phase 5).
    {key: 'shipsPerWeek', label: 'Ships a week', max: 6, unit: 'at a port, one more or fewer by the season and the times (0: no ships)'},
    // Street life (doc 79, 10; Phase 6).
    {key: 'theftCap', label: 'Thefts a day, at most', max: 20, unit: 'in each market'},
    {key: 'errandRunners', label: 'Errand-runners', max: 1, unit: 'a household member walks to the shop where a player is near', onOff: true},
    // Staged events and causes (doc 79, 8.4-8.5; Phase 7).
    {key: 'eventRate', label: 'Staged events', max: 300, unit: '% of the markets\' own (a wedding, a fire, a good catch: 1-3 a market a week at 100)'},
    {key: 'overrideNotices', label: 'Override notices', max: 1, unit: 'a Times nudge or a harvest override is posted on the boards', onOff: true},
];
/** A rule as it stands: the world's own, else the data files' (market.json's and status.json's). */
export const ruleValue = (c: RuleControls, key: string): number => c.rules[key] ?? c.ruleDefaults[key] ?? 0;
/** The body that sets a rule (null: back to the data files'); null when it changes nothing. */
export function ruleChange(c: RuleControls, key: string, value: number | null): RulesBody | null {
    const own = c.rules[key];
    if (value === null) return own === undefined ? null : {set: {[key]: null}};
    return (own ?? c.ruleDefaults[key]) === value ? null : {set: {[key]: value}};
}
/** A rule typed in: "" (back to the data files') is null, else a whole number within 0..max; undefined if neither. */
export const ruleOf = (text: string, max: number): number | null | undefined => comfortOf(text, max);
/** A service as it stands: the world's own, else market.json's (on, at its catalogue price and frequency). */
export function serviceValue(c: RuleControls, s: ServiceListed): ServiceValues {
    return c.services[s.id] ?? {on: true, price: s.price, every: s.every};
}
/** The body that changes one of a service's three; null when it is what the service has already. */
export function serviceChange(c: RuleControls, s: ServiceListed, key: keyof ServiceValues, value: boolean | number): ServicesBody | null {
    return serviceValue(c, s)[key] === value ? null : {set: {[s.id]: {[key]: value}}};
}
/** How a service reads: "barber, 1p, about once every 4 days a household (off)". */
export function serviceText(c: RuleControls, s: ServiceListed): string {
    const v = serviceValue(c, s);
    return `${s.business}, ${v.price}p, about once every ${v.every} day${v.every === 1 ? '' : 's'} a household with money to spare`
        + (v.on ? '' : ' (off)') + (c.services[s.id] ? '' : ' (market.json\'s)');
}

// The Country's food farms (Docs/Design/71-the-country-s-industries.md, 4; migration 0060): this world's changes ({} for
// none) beside Data/Economy/country_farms.json's, applied the way Core's Society::buildFarms applies them: a farm's own
// setting, then its area's, then the defaults; the yields times the area's percent and the farm's, the hands likewise
// (rounded half up); the float two days of the wages, the crew's purse by its size.
/** A farm as country_farms.json has it: its crew, wages (null: its area's, then the defaults'), float, crew's purse, the
 *  design's basket a day, and its yields (pieces a day at each season's middle day). */
export interface FarmDefault {
    id: string; kind: string; region: string; keepers: number; hands: number; keeper: number | null; hand: number | null;
    reserve: number; purse: number; basketADay: number; yield: Record<string, number[]>;
    /** A works' House (doc 72), whose estate its rent goes to; '' or absent for a food farm. */
    house?: string;
}
/** An area of farms, or (doc 72) of works: `works` pay their crews and send what is over their floats to their `house`
 *  (its estate) each night instead of buying a basket. */
export interface FarmAreaDefault {
    id: string; name: string; goods: string[]; on: boolean | null; yieldScale: number; crewScale: number;
    keeper: number | null; hand: number | null; price: string; priceFactor: number | null; revenueADay: number[]; farms: FarmDefault[];
    works?: boolean; house?: string;
}
/** What a world may change of an area, of a farm, and of the whole (absent: country_farms.json's). */
export interface FarmAreaOwn { on?: boolean; yieldScale?: number; crewScale?: number; keeper?: number; hand?: number; priceFactor?: number }
export interface FarmOwn { yieldScale?: number; crewScale?: number; keeper?: number; hand?: number }
export interface FarmSettings {
    start?: number; every?: number; order?: string[]; keeper?: number; hand?: number;
    areas?: Record<string, FarmAreaOwn>; farms?: Record<string, FarmOwn>;
}
export interface FarmControls {
    farmsSupported: boolean; farmsProblem: string; farms: FarmSettings;
    farmDefaults: {start: number; every: number; order: string[]; keeper: number; hand: number; floatDays: number; areas: FarmAreaDefault[]};
}
type Unset<T> = {[K in keyof T]?: T[K] | null};
export type FarmsBody = {set: Unset<Omit<FarmSettings, 'areas' | 'farms'>> & {
    areas?: Record<string, Unset<FarmAreaOwn> | null>; farms?: Record<string, Unset<FarmOwn> | null>}};
/** The switch as it stands: the first area on day `start` (-1: none), one every `every` days, in `order`. */
export function farmSchedule(c: FarmControls): {start: number; every: number; order: string[]} {
    const f = c.farms ?? {}, d = c.farmDefaults;
    return {start: f.start ?? d.start, every: f.every ?? d.every, order: f.order ?? d.order};
}
/** The day an area goes on by the schedule (null: never), and whether its own switch overrides it. */
export function areaOnFrom(c: FarmControls, area: string): number | null {
    const {start, every, order} = farmSchedule(c);
    const at = order.indexOf(area);
    return start < 0 || at < 0 ? null : start + at * every;
}
export function areaSwitch(c: FarmControls, a: FarmAreaDefault): 'on' | 'off' | 'schedule' {
    const on = c.farms?.areas?.[a.id]?.on ?? a.on;
    return on === true ? 'on' : on === false ? 'off' : 'schedule';
}
/** A farm as the game runs it: its crew, wages a day, its yields' percent, its float and its crew's purse. */
export function farmNow(c: FarmControls, a: FarmAreaDefault, f: FarmDefault) {
    const own = c.farms ?? {}, area = own.areas?.[a.id] ?? {}, farm = own.farms?.[f.id] ?? {}, d = c.farmDefaults;
    const yieldPercent = (area.yieldScale ?? a.yieldScale) * (farm.yieldScale ?? 100) / 100;
    const hands = Math.floor(f.hands * ((area.crewScale ?? a.crewScale) / 100) * ((farm.crewScale ?? 100) / 100) + .5);
    const keeper = farm.keeper ?? f.keeper ?? area.keeper ?? a.keeper ?? own.keeper ?? d.keeper;
    const hand = farm.hand ?? f.hand ?? area.hand ?? a.hand ?? own.hand ?? d.hand;
    const wages = f.keepers * keeper + hands * hand;
    const design = f.keepers * (f.keeper ?? a.keeper ?? d.keeper) + f.hands * (f.hand ?? a.hand ?? d.hand);
    const lean = Math.max(0, f.reserve - d.floatDays * design);
    const heads = f.keepers + hands, designHeads = f.keepers + f.hands;
    return {keepers: f.keepers, hands, keeper, hand, wages, yieldPercent, reserve: d.floatDays * wages + lean,
        purse: designHeads > 0 ? Math.round(f.purse * heads / designHeads) : 0};
}
/** A farm's yields as the page shows them: each good's four middle-day values times its percent, a decimal each. */
export function yieldText(f: FarmDefault, percent: number): string {
    return Object.entries(f.yield).map(([g, v]) => `${g} ${v.map(x => (x * percent / 100).toFixed(1)).join('/')}`).join('; ');
}
/** The body that sets one of the whole's, an area's or a farm's settings (null: back to country_farms.json's); null when it
 *  changes nothing. */
export function farmChange(c: FarmControls, where: 'all' | 'areas' | 'farms', id: string, key: string, value: number | boolean | string[] | null): FarmsBody | null {
    const own: Record<string, unknown> = where === 'all' ? (c.farms ?? {}) as Record<string, unknown>
        : ((c.farms?.[where] as Record<string, Record<string, unknown>> | undefined)?.[id] ?? {});
    const now = own[key];
    const same = Array.isArray(value) ? Array.isArray(now) && now.length === value.length && now.every((v, i) => v === value[i])
        : value === null ? now === undefined : now === value;
    if (same) return null;
    return where === 'all' ? {set: {[key]: value}} : {set: {[where]: {[id]: {[key]: value}}}};
}
/** The schedule as a sentence. */
export function scheduleText(c: FarmControls): string {
    const {start, every, order} = farmSchedule(c);
    if (start < 0) return 'No schedule: an area goes on only by its own switch.';
    return `The first area (${order[0] ?? '-'}) on day ${start} of the world's calendar, then one every ${every} days: ${order.join(', ')}.`;
}

/** The loop's numbers, with their limits and what the page calls them. */
export const LOOP_NUMBERS: {key: keyof LoopSettings; label: string; max: number; unit: string}[] = [
    {key: 'wageKeeper', label: 'A keeper\'s wage', max: 1000, unit: 'p a day'},
    {key: 'wageHand', label: 'A hand\'s wage', max: 1000, unit: 'p a day'},
    {key: 'wagePublic', label: 'Public posts and the watch', max: 1000, unit: 'p a day (the treasury)'},
    {key: 'wageWharf', label: 'The Country\'s posts', max: 1000, unit: 'p a day (dockers, stores, the Houses\' people)'},
    {key: 'gateToll', label: 'Gate toll', max: 90, unit: '% of each draw to the treasury (the city\'s tax)'},
    {key: 'carriage', label: 'Carriage', max: 1000, unit: 'p a wagon load (1,000 weight), in and out, to the carters'},
    {key: 'wharfReserve', label: 'The Country\'s reserve', max: 100000, unit: 'p kept back'},
];
/** A whole number typed in, within 0..max, or null. */
export function wholeOf(text: string, max: number): number | null {
    const bare = text.trim().replace(/[%p]$/, '').trim();
    const n = bare === '' ? NaN : Number(bare);
    return Number.isInteger(n) && n >= 0 && n <= max ? n : null;
}
/** The basket's goods by share, largest first, and how many have no maker here (the Country can't buy those). */
export function basketRows(basket: BasketGood[]): {rows: BasketGood[]; unmade: number} {
    const rows = [...basket].sort((a, b) => b.share - a.share || a.item.localeCompare(b.item));
    return {rows, unmade: rows.filter(b => !b.makers.length).length};
}
/** The goods the page lists under a search ("" for all), and the body that turns every one shown on or off. */
export function goodsShown(goods: WharfGood[], search: string): WharfGood[] {
    const q = search.trim().toLowerCase();
    return q ? goods.filter(g => g.item.includes(q) || g.name.toLowerCase().includes(q)) : goods;
}
export function goodsBulk(on: boolean, shown: WharfGood[], all: WharfGood[]): GoodsBody {
    return shown.length === all.length ? {all: on} : {set: Object.fromEntries(shown.map(g => [g.item, on]))};
}
export interface EconomyChange extends EconomyControls { changed: boolean; count?: number; detail?: string; warnings?: string[] }
/** POST economy/businesses: per business, or every listed business at once (optionally filtered). */
export type BusinessesBody = {set: Record<string, boolean>} | {all: boolean; business?: string; district?: string};

export interface Filter { business: string; district: string }
export const ANY = '';

/** The businesses the table shows under a filter ('' for any business or district; "none" for a label of no business). */
export function filtered(list: StoreBusiness[], filter: Filter): StoreBusiness[] {
    return list.filter(b => (filter.business === ANY || (b.business ?? 'none') === filter.business)
        && (filter.district === ANY || b.district === filter.district));
}

/** The filters' choices, sorted, with how many of each. */
export function choices(list: StoreBusiness[]): {businesses: [string, number][]; districts: [string, number][]} {
    const count = (values: string[]) => [...values.reduce((m, v) => m.set(v, (m.get(v) ?? 0) + 1), new Map<string, number>())]
        .sort((a, b) => a[0].localeCompare(b[0]));
    return {businesses: count(list.map(b => b.business ?? 'none')), districts: count(list.map(b => b.district))};
}

/** "All on" / "All off" for the rows shown: the filter the host applies itself, so it reaches the same rows. A label of
 *  no business ("none") can't be named to the host, so those rows are sent one by one. */
export function bulk(on: boolean, filter: Filter, shown: StoreBusiness[]): BusinessesBody {
    if (filter.business === 'none')
        return {set: Object.fromEntries(shown.map(b => [b.npc, on]))};
    return {all: on, ...(filter.business ? {business: filter.business} : {}), ...(filter.district ? {district: filter.district} : {})};
}

/** How many of these a bulk change would change. */
export function wouldChange(shown: StoreBusiness[], on: boolean): number {
    return shown.filter(b => b.internalStore !== on).length;
}

/** A markup as the page shows it ("100%"), and the world's switch as a sentence. */
export const percent = (markup: number) => `${markup}%`;
export function switchText(name: 'making' | 'resale' | 'rule', on: boolean): string {
    if (name === 'making') return `Making from the store room: ${on ? 'on' : 'off'} for this world`;
    if (name === 'resale') return `Vendors restock from the store room: ${on ? 'on' : 'off'}`;
    return `Restocking rule: ${on ? 'on' : 'off'} (the user approved it, 2026-10-31; the city's loop needs it)`;
}

/** A business's goods for the table: "iron_bar 9p" each, making first; an empty list says so. */
export function goodsText(b: StoreBusiness, switches: Pick<StoreSwitches, 'internalStore' | 'storeResale'>): string {
    const part = (goods: StoreGood[], on: boolean, mode: string) =>
        goods.length ? `${mode}${on ? '' : ' (off)'}: ${goods.map(g => `${g.item} ${g.cost}p`).join(', ')}` : '';
    return [part(b.making, switches.internalStore, 'making'), part(b.resale, switches.storeResale, 'resale')].filter(Boolean).join(' · ')
        || 'draws nothing';
}

/** A markup typed in: a whole number from 0 to 1000, or null. */
export function markupOf(text: string): number | null {
    const bare = text.trim().replace(/%$/, '').trim();
    const n = bare === '' ? NaN : Number(bare);
    return Number.isInteger(n) && n >= 0 && n <= 1000 ? n : null;
}
