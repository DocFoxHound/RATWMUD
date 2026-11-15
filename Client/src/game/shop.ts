// The shop window's goods (ui/hud/dialogs.ts draws it; the user's notes of 2026-11-15: "a generic shop window for buy
// and sell"). One shape for any shop: a keeper's (the snapshot's `merchant`: a resident who keeps a shop, a stall, an
// inn; the staged economy's list, doc 79, 7.1) and a player's stall (self.stall.here: doc 54, 3, buying only). The
// server is the authority on every line: a keeper's basket is priced by its quotes (the ladder: a dozen is not twelve
// times one), each line is a real trade it may refuse, and its receipts say what happened in its words.
import type {GameState} from './state.ts';
import {arr, bool, isObject, num, obj, str, type Json} from './json.ts';

export interface ShopRow {
    id: string;
    name: string;
    quality: string;            // "Common", "Fine"... ('' for none).
    each: number;               // A piece's price now (what the keeper asks, or pays).
    lot: number;                // The list's lot ("12 for 5p": 12; else 1) and its price.
    lotPrice: number;
    have: number;               // On the shelf (buying) or in one's pack (selling).
    step: number;               // The least that may be traded (a sell lot: "I buy those by the pair").
    most: number;               // The most a basket line may hold.
    ok: boolean;                // Whether the keeper deals in it now (a piece, or the least lot).
    reason: string;             // Why not, in the keeper's words.
    trend: number;              // +1 dearer than usual, -1 cheaper (with its cause), 0.
    trendCause: string;
}

export interface Shop {
    stall: boolean;             // A player's stall (buying only, at its keeper's prices).
    quoted: boolean;            // Its lines priced by the server's quotes (a keeper since doc 79); else a piece's price × count.
    id: string;                 // The keeper's id, or the stall's.
    name: string;
    open: boolean;
    closed: string;             // Why it is shut, when it is.
    buyingToday: number;        // What the keeper will still spend today (-1: not said).
    buy: ShopRow[];
    sell: ShopRow[];
}

export interface ShopLine {
    key: string;
    buy: boolean;
    row: ShopRow;
    quantity: number;
}

export const basketKey = (buy: boolean, item: string) => `${buy ? 'buy' : 'sell'}|${item}`;
export const quoteKey = (buy: boolean, item: string, quantity: number) => `${basketKey(buy, item)}|${quantity}`;

const whole = (v: number) => Number.isFinite(v) ? Math.max(0, Math.trunc(v)) : 0;

function keeperRow(item: Json, buy: boolean): ShopRow {
    const have = whole(num(item, buy ? 'stock' : 'owned'));
    const step = buy ? 1 : Math.max(1, whole(num(item, 'sellLot', 1)));
    // Buying goes by the piece; selling by the least lot worth a penny (the keeper's "by the pair").
    const ok = have >= step && bool(item, buy ? 'canBuy' : item.canSellLot === undefined ? 'canSell' : 'canSellLot');
    return {
        id: str(item, 'id'), name: str(item, 'name', str(item, 'id')), quality: str(item, 'quality'),
        each: whole(num(item, buy ? 'buyPrice' : 'sellPrice')),
        lot: buy ? Math.max(1, whole(num(item, 'listCount', 1))) : step,
        lotPrice: whole(num(item, buy ? 'listPrice' : 'sellLotPrice', num(item, buy ? 'buyPrice' : 'sellPrice'))),
        have, step, most: Math.min(99, have), ok,
        reason: ok ? '' : have < step ? (buy ? 'Out of stock.' : `You need ${step} to sell.`)
            : str(item, buy ? 'buyReason' : 'sellLotReason', 'Not now.').slice(0, 102),
        trend: Math.sign(num(item, 'trend')), trendCause: str(item, 'trendCause').slice(0, 80),
    };
}

/** The shop the window shows, or null when there is none to show (the keeper gone, asleep, out of reach). */
export function shopOf(s: GameState): Shop | null {
    if (s.shopStall) {
        const here = obj(obj(obj(s.snapshot, 'self'), 'stall'), 'here');
        if (!here) return null;
        const present = bool(here, 'present');
        const buy = arr(here, 'wares').filter(isObject).map((w): ShopRow => {
            const have = whole(num(w, 'quantity')), price = whole(num(w, 'price'));
            return {id: str(w, 'id'), name: str(w, 'name', str(w, 'id')), quality: '', each: price, lot: 1, lotPrice: price, have, step: 1,
                most: Math.min(99, have), ok: present && have > 0, reason: present ? (have > 0 ? '' : 'Out of stock.') : 'The keeper is away.',
                trend: 0, trendCause: str(w, 'scent')};
        });
        return {stall: true, quoted: false, id: str(here, 'id'), name: `${str(here, 'keeper', 'A')}'s stall`, open: present,
            closed: present ? '' : "The keeper isn't at the stall: nothing can be bought until they come back.", buyingToday: -1, buy, sell: []};
    }
    const m = obj(s.snapshot, 'merchant');
    if (!m || !str(m, 'id')) return null;
    const items = arr(m, 'items').filter(isObject);
    return {stall: false, quoted: bool(m, 'staged'), id: str(m, 'id'), name: str(m, 'name', 'The keeper').slice(0, 38), open: true, closed: '',
        buyingToday: bool(m, 'staged') ? whole(num(m, 'buyingToday')) : whole(num(m, 'cash')),
        buy: items.map(i => keeperRow(i, true)), sell: items.map(i => keeperRow(i, false)).filter(r => r.have > 0)};
}

/** The basket's lines, as the shop lists them now (a line whose good has gone is dropped). */
export function basketLines(s: GameState, shop: Shop): ShopLine[] {
    const lines: ShopLine[] = [];
    for (const [key, quantity] of s.shopBasket) {
        const [side, item] = [key.slice(0, key.indexOf('|')), key.slice(key.indexOf('|') + 1)];
        const buy = side === 'buy';
        const row = (buy ? shop.buy : shop.sell).find(r => r.id === item);
        if (row && quantity > 0) lines.push({key, buy, row, quantity: Math.min(quantity, row.most)});
    }
    return lines;
}

/** A line's price: the server's quote for a keeper's (null until it comes), a stall's price times the count. */
export function linePrice(s: GameState, shop: Shop, line: ShopLine): {ok: boolean; total: number; text: string} | null {
    if (!shop.quoted) return {ok: line.row.ok, total: line.row.each * line.quantity, text: line.row.reason};
    return s.shopQuotes.get(quoteKey(line.buy, line.row.id, line.quantity)) ?? null;
}

/** The basket summed: paid out, taken in, the purse after; whether it can be confirmed, and if not why. */
export function basketTotals(s: GameState, shop: Shop) {
    const purse = whole(num(obj(s.snapshot, 'self'), 'cash'));
    let pay = 0, get = 0, waiting = false, refused = '';
    const lines = basketLines(s, shop);
    for (const line of lines) {
        const price = linePrice(s, shop, line);
        if (!price) waiting = true;
        else if (!price.ok) refused ||= `${line.row.name}: ${price.text}`;
        else if (line.buy) pay += price.total;
        else get += price.total;
    }
    const after = purse - pay + get;
    const why = !lines.length ? 'Choose what to buy or sell.' : !shop.open ? shop.closed : refused || (waiting ? 'Asking the price…' : '')
        || (after < 0 ? `You haven't ${pay}p.` : '');
    return {lines, purse, pay, get, after, ready: !why, why};
}
