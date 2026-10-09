// Town projects (Docs/Design/57-changing-the-world.md, 4): a project's site panel (what it needs, the work done, the coin
// in its purse, its plaque; GIVE, HAND IN, WORK ON IT) and the same project as the town's board lists it (GIVE only).
// Hidden when there is no site near.
import type {GameState} from '../../game/state.ts';
import {arr, isObject, num, obj, str, type Json} from '../../game/json.ts';
import {button, el, setText, show} from './dom.ts';

/** "8 of 20 work-hours · Timber 4/10 · Stone 4/4". */
export function projectProgress(p: Json): string {
    const needs = arr(p, 'needs').filter(isObject).map(n => `${str(n, 'name')} ${num(n, 'have')}/${num(n, 'need')}`);
    return [`${Math.round(num(p, 'worked'))} of ${Math.round(num(p, 'hours'))} work-hours`, ...needs].join(' · ');
}

/** One project: its progress, purse, plaque, and what this wolf may do (at the site: everything; elsewhere: give). */
export function renderProject(box: HTMLElement, s: GameState, p: Json, here: boolean) {
    const send = (verb: string, extra: Record<string, unknown> = {}) => s.send({type: 'project', verb, project: str(p, 'id'), ...extra});
    const open = str(p, 'state') === 'open';
    el('div', 'small', box, open ? projectProgress(p) : `Standing · ${Math.round(num(p, 'condition'))}% repair`);
    el('div', 'muted small', box, `${num(p, 'coin')}p in its purse${open ? ', buying what it lacks and hiring hands' : ' for its upkeep'}.`);
    const plaque = arr(p, 'plaque').map(String);
    if (plaque.length) el('div', 'sage small', box, `${open ? 'Givers so far' : 'Its plaque'}: ${plaque.join(', ')}.`);
    if (num(p, 'given')) el('div', 'muted small', box, `You have given about ${num(p, 'given')}p's worth${str(p, 'shownAs') ? `, as ${str(p, 'shownAs')}` : ''}.`);
    if (!open) {
        // Standing: coin for its upkeep (hands hired to mend it when worn), and work to mend it.
        if (str(p, 'state') === 'ruin') return;
        const upkeep = el('div', 'story-row', box);
        const coins = el('input', '', upkeep) as HTMLInputElement;
        coins.type = 'number'; coins.min = '1'; coins.max = '100000'; coins.value = '5'; coins.style.width = '5em';
        button('GIVE FOR UPKEEP', 'small', upkeep, () => { const n = Math.trunc(Number(coins.value)); if (n >= 1) send('give', {coins: n}); });
        const working = str(p, 'working');
        if (here && (working || num(p, 'condition') < 100))
            button(working ? `STOP MENDING (${working})` : 'MEND IT', working ? 'secondary' : 'primary', box, () => send('work'));
        return;
    }
    // How a first gift is shown on its plaque: one of one's own names, or none.
    let shown: HTMLSelectElement | null = null;
    const names = arr(p, 'names').map(String);
    if (!str(p, 'shownAs')) {
        const row = el('div', 'story-row', box);
        el('span', 'muted small', row, 'On its plaque, as');
        shown = el('select', '', row) as HTMLSelectElement;
        shown.dataset.field = 'project-shown';
        const friend = el('option', '', shown, 'a friend of the town') as HTMLOptionElement;
        friend.value = '';
        for (const n of names) (el('option', '', shown, n) as HTMLOptionElement).value = n;
    }
    const chosen = () => (shown ? {shown: shown.value} : {});
    const give = el('div', 'story-row', box);
    const coins = el('input', '', give) as HTMLInputElement;
    coins.type = 'number'; coins.min = '1'; coins.max = '100000'; coins.value = '10'; coins.style.width = '5em';
    coins.dataset.field = 'project-coins';
    button('GIVE', 'small', give, () => { const n = Math.trunc(Number(coins.value)); if (n >= 1) send('give', {coins: n, ...chosen()}); });
    if (!here) {
        el('div', 'muted small', box, `Its site: ${str(p, 'place')}. Goods are handed in, and work done, there.`);
        return;
    }
    for (const h of arr(p, 'handable').filter(isObject)) {
        const row = el('div', 'story-row', box);
        const item = str(h, 'item');
        el('span', 'small', row, `${item.replace(/_/g, ' ').replace(/~.*/, '')} × ${num(h, 'count')}`);
        button('HAND IN', 'small', row, () => send('handin', {item, quantity: num(h, 'count'), ...chosen()}));
    }
    const working = str(p, 'working');
    button(working ? `STOP WORK (${working})` : 'WORK ON IT', working ? 'secondary' : 'primary', box, () => send('work', chosen()));
}

export class ProjectPanel {
    readonly root: HTMLElement;
    private s: GameState;
    private title: HTMLElement;
    private body: HTMLElement;
    private key = '';

    constructor(parent: HTMLElement, state: GameState) {
        this.s = state;
        this.root = el('section', 'panel project', parent);
        const head = el('div', 'panel-head', this.root);
        this.title = el('span', 'label gold', head, 'TOWN PROJECT');
        this.body = el('div', '', this.root);
    }

    update() {
        const p = obj(obj(this.s.snapshot, 'self'), 'project');
        show(this.root, !!p);
        if (!p) return;
        const key = JSON.stringify(p);
        if (key === this.key) return;
        this.key = key;
        setText(this.title, str(p, 'title').toUpperCase());
        this.body.replaceChildren();
        renderProject(this.body, this.s, p, true);
    }
}
