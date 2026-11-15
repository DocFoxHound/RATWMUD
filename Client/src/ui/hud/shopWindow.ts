// The shop window (the modal 'trade'; game/shop.ts holds its goods and basket): the keeper's goods with their prices to
// buy, one's own goods the shop deals in with what it would pay, a count on each, the purse, and CONFIRM. The server
// prices the basket (its quotes), makes each trade or refuses it, and says which in its receipts, shown at the foot.
import type {GameState} from '../../game/state.ts';
import {basketKey, basketTotals, linePrice, shopOf, type Shop, type ShopRow} from '../../game/shop.ts';
import {button, el, setClass} from './dom.ts';
import {noRect} from './story.ts';

const pennies = (n: number) => `${n}p`;

export function renderShop(panel: HTMLElement, s: GameState) {
    const act = (action: string, target = '') => s.activate({rect: noRect, action, target});
    const shop = shopOf(s);
    el('div', 'label gold', panel, shop?.stall ? 'A MARKET STALL / BUY' : 'SHOP / BUY & SELL');
    el('h1', '', panel, shop ? shop.name : 'The counter is unattended');
    if (!shop) {
        el('p', 'muted', panel, s.shopStall ? 'You have left the stall. Go back to it to buy.'
            : 'The keeper is no longer awake, in sight and within reach. Go back to them to see what they have.');
        receipts(panel, s);
        return;
    }
    const totals = basketTotals(s, shop);
    const purses = el('div', 'shop-purses', panel);
    el('span', 'gold', purses, `YOUR PURSE · ${pennies(totals.purse)}`);
    if (shop.buyingToday >= 0) el('span', 'sage', purses, `${shop.name.toUpperCase()} WILL SPEND TODAY · ${pennies(shop.buyingToday)}`);
    if (!shop.open) el('p', 'gold', panel, shop.closed);
    const cols = el('div', 'shop-cols', panel);
    column(cols, s, shop, true);
    if (!shop.stall) column(cols, s, shop, false);
    else {
        const col = el('div', 'shop-col', cols);
        el('div', 'label muted', col, 'SELL');
        el('div', 'shop-empty', col, 'A stall only sells. To sell your own goods, rent a stall on Marketday, or take them to a shop.');
    }

    // The basket: each line with the server's price for it, then the totals and CONFIRM.
    const basket = el('div', 'shop-basket', panel);
    el('div', 'label gold', basket, 'YOUR BASKET');
    if (!totals.lines.length) el('div', 'shop-empty', basket, 'Nothing chosen yet: use + beside a good.');
    for (const line of totals.lines) {
        const price = linePrice(s, shop, line);
        const row = el('div', 'shop-line', basket);
        el('span', '', row, `${line.buy ? 'Buy' : 'Sell'} ${line.quantity} × ${line.row.name}`);
        if (!price) el('span', 'muted', row, 'asking…');
        else if (!price.ok) el('span', 'refused', row, price.text || 'Refused.');
        else el('span', line.buy ? 'gold' : 'sage', row, `${line.buy ? '−' : '+'}${pennies(price.total)}`);
    }
    const sum = el('div', 'shop-sum', basket);
    el('span', 'gold', sum, `YOU PAY · ${pennies(totals.pay)}`);
    if (!shop.stall) el('span', 'sage', sum, `YOU GET · ${pennies(totals.get)}`);
    el('span', totals.after < 0 ? 'gold' : '', sum, `PURSE AFTER · ${pennies(totals.after)}`);
    const actions = el('div', 'shop-actions', basket);
    const confirm = button('CONFIRM', 'primary', actions, () => act('shop_confirm'));
    confirm.disabled = !totals.ready;
    confirm.dataset.shop = 'confirm';
    if (totals.lines.length) button('CLEAR', 'small', actions, () => act('shop_clear'));
    if (totals.why && totals.lines.length) el('span', 'why', actions, totals.why);
    receipts(panel, s);
    el('p', 'muted small', panel, shop.quoted ? "p = silver penny · Prices follow what's bought and sold here, and what the roads bring. " +
        'The keeper checks every line again as you confirm.' : 'p = silver penny · The keeper checks every line again as you confirm.');
}

function column(parent: HTMLElement, s: GameState, shop: Shop, buy: boolean) {
    const col = el('div', 'shop-col', parent);
    el('div', 'label muted', col, buy ? (shop.stall ? 'LAID OUT' : 'THEIR GOODS · BUY') : 'YOUR GOODS · SELL');
    const rows = el('div', 'shop-rows', col);
    const list = buy ? shop.buy : shop.sell;
    if (!list.length) el('div', 'shop-empty', rows, buy ? 'Nothing for sale.' : 'You carry nothing this shop buys.');
    for (const r of list) goodRow(rows, s, r, buy, shop.open);
}

function goodRow(parent: HTMLElement, s: GameState, r: ShopRow, buy: boolean, open: boolean) {
    const act = (action: string, target = '') => s.activate({rect: noRect, action, target});
    const chosen = s.shopBasket.get(basketKey(buy, r.id)) ?? 0;
    const usable = open && r.ok;
    const row = el('div', `shop-row${chosen ? ' chosen' : ''}${usable ? '' : ' off'}`, parent);
    row.dataset.item = r.id;
    row.dataset.side = buy ? 'buy' : 'sell';
    const name = el('div', 'shop-name', row, r.quality && r.quality !== 'Common' ? `${r.quality} ${r.name.toLowerCase()}` : r.name);
    name.title = name.textContent ?? '';
    if (r.trend) {
        const mark = el('span', r.trend > 0 ? 'gold' : 'sage', name, r.trend > 0 ? ' ▲' : ' ▼');
        mark.title = r.trendCause || (r.trend > 0 ? 'dearer than usual' : 'cheaper than usual');
    }
    const price = r.lot > 1 ? `${r.lot} for ${pennies(r.lotPrice)}` : `${pennies(r.each)} each`;
    const meta = buy ? `${price} · ${r.have} on the shelf` : `They pay ${r.step > 1 ? `${pennies(r.lotPrice)} for ${r.step}` : `${pennies(r.each)} each`} · you carry ${r.have}`;
    el('div', `shop-meta${usable ? '' : ' why'}`, row, usable ? meta : `${meta} · ${r.reason}`);
    const qty = el('div', 'shop-qty', row);
    const set = (n: number) => act('shop_qty', `${buy ? 'buy' : 'sell'}|${r.id}|${n}`);
    const less = button('−', 'small', qty, () => set(chosen - r.step));
    less.disabled = !chosen;
    less.title = 'Fewer';
    const box = el('input', '', qty);
    box.type = 'number';
    box.min = '0';
    box.max = String(r.most);
    box.step = String(r.step);
    box.value = String(chosen);
    box.disabled = !usable;
    box.setAttribute('aria-label', `How many ${r.name} to ${buy ? 'buy' : 'sell'}`);
    box.addEventListener('change', () => set(Math.max(0, Math.trunc(Number(box.value) || 0))));
    box.addEventListener('keydown', e => { if (e.key !== 'Escape') e.stopPropagation(); });   // (Digits: a count here, not the map's.)
    const more = button('+', 'small', qty, () => set(chosen + r.step));
    more.disabled = !usable || chosen + r.step > r.most;
    more.title = 'More';
    setClass(row, 'chosen', chosen > 0);
}

function receipts(panel: HTMLElement, s: GameState) {
    if (!s.shopReceipts.length) return;
    const list = el('div', 'shop-receipts', panel);
    for (const r of s.shopReceipts) el('div', r.ok ? 'ok' : 'no', list, `${r.ok ? '✓' : '✗'} ${r.text}`);
}
