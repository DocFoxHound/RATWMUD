// Lodgings (Docs/Design/54-gathering-places.md, 4): what can be taken where one stands (a bed upstairs at an inn, the
// whole upstairs for a night, a place to let), and one's own lodging: where, how long paid, the chest (put in, take
// out), opening a whole place for the night, giving it up. Hidden when there is nothing to show.
import type {GameState} from '../../game/state.ts';
import {arr, bool, isObject, num, obj, str} from '../../game/json.ts';
import {button, el, setText, show} from './dom.ts';

export class LodgingPanel {
    readonly root: HTMLElement;
    private s: GameState;
    private title: HTMLElement;
    private body: HTMLElement;
    private key = '';

    constructor(parent: HTMLElement, state: GameState) {
        this.s = state;
        this.root = el('section', 'panel lodging', parent);
        const head = el('div', 'panel-head', this.root);
        this.title = el('span', 'label gold', head, 'LODGINGS');
        this.body = el('div', '', this.root);
    }

    update() {
        const s = this.s, lodging = obj(obj(s.snapshot, 'self'), 'lodging');
        show(this.root, !!lodging);
        if (!lodging) return;
        const key = JSON.stringify([lodging, s.inventoryQuantity('meal')]);
        if (key === this.key) return;
        this.key = key;
        this.body.replaceChildren();
        const send = (fields: Record<string, unknown>) => s.send({type: 'lodge', ...fields});
        const mine = obj(lodging, 'mine');
        if (mine) {
            setText(this.title, 'YOUR LODGING');
            el('div', 'small', this.body, `${str(mine, 'where')} · ${str(mine, 'kind') === 'bed' ? 'a bed' : str(mine, 'kind') === 'lodger' ? 'a lodger\'s bed'
                : str(mine, 'kind') === 'night' ? 'the whole upstairs' : 'the whole place'} · paid ${num(mine, 'days').toFixed(1)} days` +
                (num(mine, 'notice') > 0 ? ` · THE LANDLORD WANTS IT BACK in ${num(mine, 'notice').toFixed(1)} days` : ''));
            if (bool(mine, 'here')) {
                el('div', 'label muted', this.body, 'THE CHEST');
                const chest = arr(mine, 'chest').filter(isObject);
                if (!chest.length) el('div', 'muted small', this.body, 'Empty.');
                for (const c of chest) {
                    const row = el('div', 'story-row', this.body);
                    el('span', 'small', row, `${str(c, 'name')} × ${num(c, 'count')}`);
                    button('TAKE ONE', 'small', row, () => send({verb: 'take', item: str(c, 'id'), quantity: 1}));
                }
                if (s.inventoryQuantity('meal') > 0) button('PUT A MEAL IN', 'small', this.body, () => send({verb: 'put', item: 'meal', quantity: 1}));
                if (str(mine, 'kind') === 'night' || str(mine, 'kind') === 'place')
                    button(bool(mine, 'open') ? 'CLOSE THE DOORS' : 'OPEN THE DOORS', 'small', this.body, () => send({verb: bool(mine, 'open') ? 'close' : 'open'}))
                        .title = 'Open: anyone may come in tonight (a player-run night)';
            }
            button('GIVE IT UP', 'small', this.body, () => { if (window.confirm('Give up your lodging?')) send({verb: 'end'}); });
            return;
        }
        setText(this.title, 'TO LET');
        for (const o of arr(lodging, 'offers').filter(isObject))
            button(`${str(o, 'label').toUpperCase()} · ${num(o, 'price')}p`, 'small', this.body, () => send({verb: str(o, 'verb'), period: str(o, 'period')}));
        el('div', 'muted small', this.body, 'A bed you rent rests you fully, and away at an inn your rest builds faster.');
    }
}
