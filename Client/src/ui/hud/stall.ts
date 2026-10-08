// Market stalls (Docs/Design/54-gathering-places.md, 3): renting the spot one stands at on Marketday morning, one's own
// stall (wares and prices, laying out from the pack, taking off, the morning's takings, packing up), and a stall within
// reach (its wares, prices and scent, BUY while the keeper is there). Hidden when there is nothing to show.
import type {GameState} from '../../game/state.ts';
import {arr, bool, isObject, num, obj, objects, str} from '../../game/json.ts';
import {button, el, setText, show} from './dom.ts';

export class StallPanel {
    readonly root: HTMLElement;
    private s: GameState;
    private title: HTMLElement;
    private body: HTMLElement;
    private key = '';

    constructor(parent: HTMLElement, state: GameState) {
        this.s = state;
        this.root = el('section', 'panel stall', parent);
        const head = el('div', 'panel-head', this.root);
        this.title = el('span', 'label gold', head, 'MARKET STALL');
        this.body = el('div', '', this.root);
    }

    update() {
        const s = this.s, stall = obj(obj(s.snapshot, 'self'), 'stall');
        show(this.root, !!stall);
        if (!stall) return;
        const mine = obj(stall, 'mine');
        const key = JSON.stringify([stall, mine && bool(mine, 'here') ? objects(s.snapshot, 'inventory') : null]);
        if (key === this.key) return;
        this.key = key;
        this.body.replaceChildren();
        const send = (fields: Record<string, unknown>) => s.send({type: 'stall', ...fields});
        if (mine) {
            setText(this.title, 'YOUR STALL');
            el('div', 'small', this.body, `Takings this morning: ${num(mine, 'takings')}p · until 2, while you stand by it`);
            const wares = arr(mine, 'wares').filter(isObject);
            if (!wares.length) el('div', 'muted small', this.body, 'Nothing laid out yet.');
            for (const w of wares) {
                const row = el('div', 'story-row', this.body);
                el('span', 'small', row, `${str(w, 'name')} × ${num(w, 'quantity')} · ${num(w, 'price')}p`);
                if (!bool(mine, 'here')) continue;
                button('PRICE', 'small', row, () => {
                    const price = Number(window.prompt(`Price apiece for ${str(w, 'name')} (1 to 999p)`, String(num(w, 'price'))));
                    if (price >= 1) send({verb: 'price', item: str(w, 'id'), price: Math.trunc(price)});
                });
                button('TAKE OFF', 'small', row, () => send({verb: 'unlist', item: str(w, 'id')}));
            }
            if (bool(mine, 'here')) {
                // Laying out from the pack: an item, how many, and a price apiece.
                const row = el('div', 'story-row', this.body);
                const pick = el('select', '', row);
                for (const it of objects(s.snapshot, 'inventory')) {
                    if (num(it, 'quantity') <= 0) continue;
                    const o = el('option', '', pick, `${str(it, 'name', str(it, 'id'))} (${num(it, 'quantity')})`);
                    o.value = str(it, 'id');
                }
                const qty = el('input', '', row);
                qty.type = 'number'; qty.min = '1'; qty.max = '99'; qty.value = '1'; qty.style.width = '3.5em';
                qty.title = 'How many';
                const price = el('input', '', row);
                price.type = 'number'; price.min = '1'; price.max = '999'; price.value = '5'; price.style.width = '4em';
                price.title = 'Price apiece (p)';
                button('LAY OUT', 'small', row, () => {
                    if (pick.value) send({verb: 'list', item: pick.value, quantity: Math.trunc(Number(qty.value)), price: Math.trunc(Number(price.value))});
                });
            } else el('div', 'muted small', this.body, 'Go back to your stall to sell, or to change it.');
            button('PACK UP', 'small', this.body, () => { if (window.confirm('Pack up your stall for the day?')) send({verb: 'close'}); });
            return;
        }
        const here = obj(stall, 'here');
        if (here) {
            setText(this.title, `${str(here, 'keeper').toUpperCase()}'S STALL`);
            const present = bool(here, 'present');
            if (!present) el('div', 'muted small', this.body, 'The keeper isn\'t at the stall: nothing can be bought until they come back.');
            const wares = arr(here, 'wares').filter(isObject);
            if (!wares.length) el('div', 'muted small', this.body, 'Nothing laid out.');
            for (const w of wares) {
                const row = el('div', 'story-row', this.body);
                el('span', 'small', row, `${str(w, 'name')} × ${num(w, 'quantity')} · ${num(w, 'price')}p${str(w, 'scent') ? ` · ${str(w, 'scent')}` : ''}`);
                if (present) button(`BUY · ${num(w, 'price')}p`, 'small', row, () => send({verb: 'buy', id: str(here, 'id'), item: str(w, 'id'), quantity: 1}));
            }
            el('div', 'muted small', this.body, 'Haggle with the keeper in words; they can change a price at any time.');
            return;
        }
        if (obj(stall, 'offer')) {
            setText(this.title, 'MARKET STALL');
            button(`RENT THIS STALL FOR TODAY · ${num(obj(stall, 'offer'), 'fee')}p`, 'small', this.body, () => send({verb: 'rent'}));
            el('div', 'muted small', this.body, 'Sell your own goods face to face until 2. The fee goes to the town.');
        }
    }
}
