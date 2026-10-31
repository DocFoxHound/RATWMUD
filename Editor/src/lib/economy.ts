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
export interface EconomyControls extends StoreSwitches {
    world: string; supported: boolean; problem: string; note: string; takesEffect: string; businesses: StoreBusiness[];
    worldInfo?: {key: string; name: string; kind: string; upstream?: string};
    loop: LoopSettings; countryOff: string[]; loopSupported: boolean; loopProblem: string; ruleWarning: string;
    market: {id: string; name: string; people: string}; basket: BasketGood[]; goods: CountryGood[];
}
export type GoodsBody = {set: Record<string, boolean>} | {all: boolean};

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
