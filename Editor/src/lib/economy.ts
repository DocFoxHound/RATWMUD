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
    needsPaused: boolean; country: boolean; countryReserve: number; gateToll: number; carriage: number;
    wageKeeper: number; wageHand: number; wagePublic: number; wageCountry: number; subsidies: Record<string, number>;
}
/** A good of the Country's basket (Data/Items/country.json): its share of the Country's spending and its makers here. */
export interface BasketGood { item: string; name: string; share: number; percent: number; price: number; makers: string[] }
/** A good some business here draws, and whether the Country supplies it (off: its real producers'). */
export interface CountryGood { item: string; name: string; businesses: number; supplied: boolean }
export interface EconomyControls extends StoreSwitches, SeasonControls, RuleControls, FarmControls {
    world: string; supported: boolean; problem: string; note: string; takesEffect: string; businesses: StoreBusiness[];
    worldInfo?: {key: string; name: string; kind: string; upstream?: string};
    loop: LoopSettings; countryOff: string[]; loopSupported: boolean; loopProblem: string; ruleWarning: string;
    market: {id: string; name: string; people: string}; basket: BasketGood[]; goods: CountryGood[];
}

// The seasons in the Country's supply and the residents' comforts (Docs/Design/68-the-country-and-the-city-loop.md,
// Progress, "Seasons and the DM's rules"; migration 0057): country.json's season groups with this world's changes.
/** A group's numbers: percent of normal by season (Spring, Summer, Autumn, Winter), and its normal pieces a day. */
export interface SeasonValues { supply: number[]; price: number[]; shelf: number[]; normal: number }
/** A season group as the world has it (`changed`: away from country.json's, which `file` holds). */
export interface SeasonGroup extends SeasonValues { id: string; name: string; about: string; goods: string[]; changed: boolean; file: SeasonValues }
/** The comforts (null: country.json's, `fileAbove` pennies a head and `fileShare` percent). */
export interface Comforts { comfortsAbove: number | null; comfortsShare: number | null; fileAbove: number; fileShare: number }
export interface SeasonControls {
    seasonsSupported: boolean; seasonsProblem: string; seasonNames: string[]; seasons: SeasonGroup[]; comforts: Comforts;
}
export type SeasonsBody = {set: Record<string, Partial<SeasonValues> | null>};
export type ComfortsBody = Partial<Pick<Comforts, 'comfortsAbove' | 'comfortsShare'>>;
export type SeasonRow = 'supply' | 'price' | 'shelf';
/** The three rows of a group, their limits and what the page calls them. */
export const SEASON_ROWS: {key: SeasonRow; label: string; low: number; high: number; hint: string}[] = [
    {key: 'supply', label: 'Supply', low: 0, high: 1000, hint: '% of its normal pieces a day the Country can send (0: none)'},
    {key: 'price', label: 'Price', low: 10, high: 1000, hint: '% of the catalogue price: store draws and the shops\' prices to residents'},
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
/** How the comforts read: the world's own, or country.json's. */
export function comfortsText(c: Comforts): string {
    const above = c.comfortsAbove ?? c.fileAbove, share = c.comfortsShare ?? c.fileShare;
    const whose = c.comfortsAbove === null && c.comfortsShare === null ? ' (country.json\'s)' : '';
    return `A household holding more than ${above}p a head spends ${share}% of what is over it a day on comforts${whose}.`;
}
/** A comforts value typed in: "" (back to country.json's) is null, else a whole number within 0..max; undefined if neither. */
export function comfortOf(text: string, max: number): number | null | undefined {
    if (text.trim() === '') return null;
    const n = wholeOf(text, max);
    return n === null ? undefined : n;
}
export type GoodsBody = {set: Record<string, boolean>} | {all: boolean};

// The loop's rules and the city's services (Docs/Design/68-the-country-and-the-city-loop.md, Progress, "First fixes";
// migration 0058): this world's changes ({} for none) beside Data/Items/country.json's.
/** A service as a world has it (stored whole once changed). */
export interface ServiceValues { on: boolean; price: number; every: number }
/** A country.json service: its item's name and catalogue price (items.json), its business and how often it is bought. */
export interface ServiceListed { id: string; name: string; business: string; price: number; every: number; for?: string }
export interface RuleControls {
    rulesSupported: boolean; rulesProblem: string; rules: Record<string, number>; services: Record<string, ServiceValues>;
    ruleDefaults: Record<string, number>; serviceList: ServiceListed[]; drift?: DriftView;
}
/** Where the self-correcting rules stand on the world's game, as its last save has them (doc 68, Progress,
 * "Self-correcting rules"): read only; the rules move it, never a setting. */
export interface DriftView {
    saved: string | null; problem: string; tollBase: number; almsBase: number;
    toll?: number; alms?: number; level?: number | null; target?: number;
    prices?: {on: boolean; moved: number; mean: number; low: number; high: number};
    shelves?: {on: boolean; wares: number; mean: number | null};
    wages?: {on: boolean; tills: number; up: number; down: number; mean: number; moved: {till: string; name: string; step: number}[]};
}
/** The drift in a line each, for the page (empty when the world has no save). */
export function driftLines(d: DriftView | undefined): string[] {
    if (!d || !d.saved) return [];
    const off = (on: boolean | undefined) => on ? '' : ' (the rule is off)';
    const out = [`The gate toll ${d.toll?.toFixed(1)}% (the world's ${d.tollBase}%), the alms ${d.alms}p a head (${d.almsBase}p)`
        + `: the treasury's level ${d.level ?? '-'}p against its target ${d.target}p.`];
    if (d.prices) out.push(`Price factors: ${d.prices.moved} wares off the catalogue's price, mean ${d.prices.mean}, from ${d.prices.low} to `
        + `${d.prices.high}${off(d.prices.on)}.`);
    if (d.shelves) out.push(`Shelves from sales: ${d.shelves.wares} wares, ${d.shelves.mean ?? '-'} on a shelf on average${off(d.shelves.on)}.`);
    if (d.wages) out.push(`Wage steps: ${d.wages.up} tills up, ${d.wages.down} down of ${d.wages.tills}, mean ${d.wages.mean}p a post`
        + `${off(d.wages.on)}.`);
    return out;
}
export type RulesBody = {set: Record<string, number | null>};
export type ServicesBody = {set: Record<string, Partial<ServiceValues> | null>};
/** The rules the page lists, their limits, whether each is an on/off switch, and what the page calls them. */
export const RULES: {key: string; label: string; max: number; unit: string; onOff?: boolean}[] = [
    {key: 'comfortsMost', label: 'Comforts a day, at most', max: 100000, unit: 'purchases a household makes a day (it was 20)'},
    {key: 'almsADay', label: 'Alms', max: 1000, unit: 'p a head a household is made up to after the day\'s wages'},
    {key: 'treasuryCap', label: 'Treasury cap', max: 100000000, unit: 'p kept; what is over it pays the shops\' short wages'},
    {key: 'wageSupport', label: 'Wage support', max: 100, unit: '% of a till\'s shortfall the treasury pays (from over the cap)'},
    {key: 'workBankHours', label: 'Work bank', max: 1000, unit: 'hours of the crew\'s work a shop may bank (0: its longest craft\'s)'},
    {key: 'fairFood', label: 'Fair food in a household', max: 1, unit: 'the members with least food buy first', onOff: true},
    {key: 'services', label: 'The city\'s services', max: 1, unit: 'households buy them from their comforts budget', onOff: true},
    {key: 'servicesCityShare', label: 'The city\'s share', max: 100, unit: '% of each service\'s price to the treasury'},
    // Trades and the follow-ups (doc 68, Progress, "Trades").
    {key: 'suppliers', label: 'City suppliers first', max: 1, unit: 'a keeper buys its craft inputs from the city\'s makers before the store room (the cooper\'s casks)', onOff: true},
    {key: 'patrons', label: 'Patrons', max: 1, unit: 'the treasury, the Houses and the wharf pay for their kit, hulls and buildings\' work', onOff: true},
    {key: 'armourEvery', label: 'New kit every', max: 3650, unit: 'days, for each armed wolf (the armoury\'s new pieces)'},
    {key: 'buyAheadDays', label: 'Buy ahead', max: 5, unit: 'days of food more a household buys before a Restday or festival'},
    {key: 'floatDrawDays', label: 'A till\'s float', max: 28, unit: 'days of draws averaged for the float it keeps back (1: yesterday\'s)'},
    {key: 'floatWagesDays', label: 'Wages kept before profit', max: 60, unit: 'days of its posts\' wages a till keeps over its float'},
    {key: 'subsidyToNeed', label: 'Carriage subsidy only to need', max: 1, unit: 'the treasury pays the carters only what they lack of their wages', onOff: true},
    // The treasury's surplus (doc 68, Progress, "The treasury's surplus: works and alms").
    {key: 'surplus', label: 'The treasury\'s surplus', max: 1, unit: 'what it holds over its cap and reserve goes out at 18:00, to public works and the poorest', onOff: true},
    {key: 'surplusKeep', label: 'Reserve over the cap', max: 100000000, unit: 'p the treasury keeps over its cap for the lean seasons before its surplus goes out'},
    {key: 'surplusWorks', label: 'Surplus to public works', max: 100, unit: '% of the surplus a day: orders from the city\'s trades, till to till'},
    {key: 'surplusAlms', label: 'Surplus to the poorest', max: 100, unit: '% of the surplus a day: the poorest households, per head, poorest first'},
    {key: 'surplusAlmsTo', label: 'The poorest raised to', max: 1000, unit: 'p a head, at most, the surplus brings the poorest households up to'},
    // The self-correcting rules (doc 68, Progress, "Self-correcting rules"): each drifts gently within its bounds; where
    // each stands now is the running game's (RuleControls.drift), shown beside them, read only.
    {key: 'stockPrices', label: 'Prices follow stock', max: 1, unit: 'a shop\'s price for a ware rises when it sells out and falls on a glut, the city\'s prices as a whole kept at the catalogue\'s', onOff: true},
    {key: 'stockPriceBand', label: 'Price band', max: 90, unit: '% either side of the catalogue\'s (the season\'s) price, at most'},
    {key: 'stockPriceStep', label: 'Price step', max: 1000, unit: 'thousandths of the price a day (a quarter of it back toward the catalogue\'s on a quiet day)'},
    {key: 'shelfFollow', label: 'Shelves follow sales', max: 1, unit: 'a shelf follows its ware\'s sales a day, smoothed (off: a quarter over yesterday\'s)', onOff: true},
    {key: 'shelfDays', label: 'Sales smoothed over', max: 365, unit: 'days, about'},
    {key: 'shelfCover', label: 'Shelf', max: 1000, unit: '% of the smoothed sales a day on the shelf (never under its starting stock)'},
    {key: 'shelfGrow', label: 'Grows on selling out', max: 500, unit: '% more than its shelf a day it sells out counts as sold'},
    {key: 'wageDrift', label: 'Wages follow cover', max: 1, unit: 'a till\'s posts\' wages rise a penny when it pays them with plenty to spare, fall a penny when it keeps falling short', onOff: true},
    {key: 'wageDriftDays', label: 'Wages judged every', max: 365, unit: 'days'},
    {key: 'wageDriftSpare', label: 'Plenty to spare', max: 1000, unit: '% of its wages its keeper\'s profit is, over those days, for a raise'},
    {key: 'wageDriftMost', label: 'Wage drift, at most', max: 100, unit: 'p either way on a keeper\'s wage'},
    {key: 'wageDriftHandMost', label: 'A hand\'s wage drift, at most', max: 100, unit: 'p either way on a hand\'s wage'},
    {key: 'wageDriftFloor', label: 'Wage floor', max: 1000, unit: 'p a day: no post lowered under it (a day\'s food)'},
    {key: 'treasuryDrift', label: 'Toll and alms follow the treasury', max: 1, unit: 'over its target the toll eases and the alms rise; under it the toll firms and the alms tighten', onOff: true},
    {key: 'treasuryTarget', label: 'Treasury target', max: 100000000, unit: 'p, its level (a slow mean at 18:00) is judged against'},
    {key: 'treasuryBand', label: 'Target band', max: 100, unit: '% either side of the target in which nothing moves'},
    {key: 'treasuryDays', label: 'Level over', max: 3650, unit: 'days, about (the slow mean)'},
    {key: 'treasuryJudgeDays', label: 'Judged every', max: 365, unit: 'days (the toll and the alms move a step at most each time)'},
    {key: 'tollStep', label: 'Toll step', max: 100, unit: 'tenths of a point each time it is judged'},
    {key: 'tollDriftMost', label: 'Toll drift, at most', max: 90, unit: 'points from the world\'s gate toll'},
    {key: 'almsDriftMost', label: 'Alms drift, at most', max: 1000, unit: 'p a head over the alms, at most (a penny each time; never under them)'},
];
/** A rule as it stands: the world's own, else country.json's. */
export const ruleValue = (c: RuleControls, key: string): number => c.rules[key] ?? c.ruleDefaults[key] ?? 0;
/** The body that sets a rule (null: back to country.json's); null when it changes nothing. */
export function ruleChange(c: RuleControls, key: string, value: number | null): RulesBody | null {
    const own = c.rules[key];
    if (value === null) return own === undefined ? null : {set: {[key]: null}};
    return (own ?? c.ruleDefaults[key]) === value ? null : {set: {[key]: value}};
}
/** A rule typed in: "" (back to country.json's) is null, else a whole number within 0..max; undefined if neither. */
export const ruleOf = (text: string, max: number): number | null | undefined => comfortOf(text, max);
/** A service as it stands: the world's own, else country.json's (on, at its catalogue price and frequency). */
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
        + (v.on ? '' : ' (off)') + (c.services[s.id] ? '' : ' (country.json\'s)');
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
/** The farms on the world's game, as its last save has them (read only): each farm's till, its store and its crew's purse. */
export interface FarmsSaved {
    saved: string | null; problem: string;
    farms: Record<string, {till: number; stock: number; goods: Record<string, number>; crewPurse: number}>;
}
export interface FarmControls {
    farmsSupported: boolean; farmsProblem: string; farms: FarmSettings;
    farmDefaults: {start: number; every: number; order: string[]; keeper: number; hand: number; floatDays: number; areas: FarmAreaDefault[]};
    farmsSaved?: FarmsSaved;
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
    {key: 'wageCountry', label: 'The Country\'s posts', max: 1000, unit: 'p a day (dockers, stores, the Houses\' people)'},
    {key: 'gateToll', label: 'Gate toll', max: 90, unit: '% of each draw to the treasury (the city\'s tax)'},
    {key: 'carriage', label: 'Carriage', max: 1000, unit: 'p a wagon load (1,000 weight), in and out, to the carters'},
    {key: 'countryReserve', label: 'The Country\'s reserve', max: 100000, unit: 'p kept back'},
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
export function goodsShown(goods: CountryGood[], search: string): CountryGood[] {
    const q = search.trim().toLowerCase();
    return q ? goods.filter(g => g.item.includes(q) || g.name.toLowerCase().includes(q)) : goods;
}
export function goodsBulk(on: boolean, shown: CountryGood[], all: CountryGood[]): GoodsBody {
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
