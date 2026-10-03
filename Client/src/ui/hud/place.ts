// The place panel (Docs/Design/32-parties-chapters-factions.md, 5.2): a place to let where the wolf stands, its terms
// and who holds it; for its Chapter, the lease, the notice board and the Chapter's stores. Hidden elsewhere.
import type {GameState} from '../../game/state.ts';
import {arr, bool, isObject, num, obj, str} from '../../game/json.ts';
import {button, el, setText, show} from './dom.ts';

export class PlacePanel {
    readonly root: HTMLElement;
    private s: GameState;
    private title: HTMLElement;
    private body: HTMLElement;
    private key = '';
    private noticeInput: HTMLInputElement;

    constructor(parent: HTMLElement, state: GameState) {
        this.s = state;
        this.root = el('section', 'panel place', parent);
        const head = el('div', 'panel-head', this.root);
        this.title = el('span', 'label gold', head);
        this.body = el('div', '', this.root);
        this.noticeInput = document.createElement('input');
        this.noticeInput.className = 'name-input';
        this.noticeInput.maxLength = 200;
        this.noticeInput.placeholder = 'Pin a notice (Enter)';
        this.noticeInput.addEventListener('keydown', e => {
            if (e.key !== 'Enter' || !this.noticeInput.value.trim()) return;
            state.sendChapter({verb: 'notice', text: this.noticeInput.value.trim()});
            this.noticeInput.value = '';
        });
    }

    update() {
        const s = this.s, place = obj(obj(s.snapshot, 'self'), 'place');
        show(this.root, !!place);
        if (!place) return;
        const key = JSON.stringify(place);
        if (key === this.key) return;
        this.key = key;
        setText(this.title, str(place, 'name').toUpperCase());
        this.body.replaceChildren();
        const send = (fields: Record<string, unknown>) => s.sendChapter(fields);
        if (!str(place, 'heldBy')) {
            el('div', 'small', this.body, `To let: ${num(place, 'rent')} pennies a week from ${str(place, 'landlord')}` +
                (str(place, 'faction') ? ` (${str(place, 'faction')}'s land)` : '') + '.');
            el('div', 'muted small', this.body, `A Chapter of level ${num(place, 'level')} or more may take it, from its treasury.`);
            button('TAKE THE LEASE', 'small', this.body, () => send({verb: 'lease'}));
            return;
        }
        if (!bool(place, 'mine')) {
            el('div', 'small', this.body, `Rented by ${str(place, 'heldBy')}.`);
            return;
        }
        el('div', 'small', this.body, `Your Chapter's · ${num(place, 'daysPaid')} days paid` +
            (str(place, 'state') === 'grace' ? ' · RENT OVERDUE' : ''));
        el('div', 'label gold', this.body, 'NOTICES');
        arr(place, 'notices').filter(isObject).forEach((n, i) => {
            const row = el('div', 'story-row', this.body);
            el('span', 'small', row, `${str(n, 'text')} — ${str(n, 'by')}`);
            button('×', 'chip-x', row, () => send({verb: 'unnotice', index: i})).title = 'Take it down';
        });
        this.body.append(this.noticeInput);
        el('div', 'label gold', this.body, 'THE STORES');
        const stores = obj(place, 'stores');
        for (const item of ['meal', 'herbs', 'sword']) {
            const row = el('div', 'story-row', this.body);
            el('span', 'small', row, `${item === 'meal' ? 'Meals' : item === 'herbs' ? 'Herbs' : 'Swords'}: ${num(stores, item)}`);
            button('PUT ONE IN', 'small', row, () => send({verb: 'store', item, quantity: 1}));
            if (num(stores, item) > 0) button('TAKE ONE', 'small', row, () => send({verb: 'take', item, quantity: 1}));
        }
        button('GIVE UP THE LEASE', 'small', this.body, () => {
            if (window.confirm('Give up the lease?')) send({verb: 'endlease'});
        });
    }
}
