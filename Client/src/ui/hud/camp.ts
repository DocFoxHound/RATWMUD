// The camp panel (Docs/Design/32-parties-chapters-factions.md, 5.3–5.5): on the Chapter's own ground, what stands and
// what is planned (work on it, or mend it, when close), what can be planned where the wolf stands, and who works there.
import type {GameState} from '../../game/state.ts';
import {arr, bool, isObject, num, obj, str} from '../../game/json.ts';
import {button, el, setText, show} from './dom.ts';

export class CampPanel {
    readonly root: HTMLElement;
    private s: GameState;
    private title: HTMLElement;
    private body: HTMLElement;
    private key = '';

    constructor(parent: HTMLElement, state: GameState) {
        this.s = state;
        this.root = el('section', 'panel camp', parent);
        const head = el('div', 'panel-head', this.root);
        this.title = el('span', 'label gold', head);
        this.body = el('div', '', this.root);
    }

    update() {
        const s = this.s, camp = obj(obj(s.snapshot, 'self'), 'camp');
        show(this.root, !!camp);
        if (!camp) return;
        const key = JSON.stringify(camp);
        if (key === this.key) return;
        this.key = key;
        setText(this.title, str(camp, 'name').toUpperCase());
        this.body.replaceChildren();
        const send = (fields: Record<string, unknown>) => s.sendChapter(fields);
        for (const st of arr(camp, 'structures').filter(isObject)) {
            const row = el('div', 'story-row', this.body);
            const built = bool(st, 'built');
            el('span', 'small', row, `${str(st, 'name')} · ${built ? `${num(st, 'condition')}%` : `building ${num(st, 'progress')}%`}`);
            if (bool(st, 'working')) el('span', 'label sage', row, 'WORKING');
            else if (bool(st, 'near') && (!built || num(st, 'condition') < 100))
                button(built ? 'MEND' : 'WORK ON IT', 'small', row, () => send({verb: 'build', target: str(st, 'id')}));
            if (!built && num(st, 'progress') === 0) button('×', 'chip-x', row, () => send({verb: 'unplan', target: str(st, 'id')})).title = 'Take back the plan';
        }
        el('div', 'label gold', this.body, 'PLAN HERE');
        const kinds = el('div', 'name-list', this.body);
        for (const k of arr(camp, 'kinds').filter(isObject))
            button(`${str(k, 'name')} (${num(k, 'cost')}p)`, 'small', kinds, () => send({verb: 'plan', kind: str(k, 'id')})).title =
                `${num(k, 'hours')} work-hours to build; materials from the treasury`;
        const staff = arr(camp, 'staff').filter(isObject);
        if (staff.length) {
            el('div', 'label gold', this.body, 'WORKING HERE');
            for (const p of staff) {
                const row = el('div', 'story-row', this.body);
                el('span', 'small', row, `${str(p, 'name')} · ${num(p, 'wage')}p a day`);
                button('LET GO', 'small', row, () => send({verb: 'dismissstaff', target: str(p, 'id')}));
            }
        }
    }
}
