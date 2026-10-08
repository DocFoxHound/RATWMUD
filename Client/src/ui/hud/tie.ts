// Under the map (Docs/Design/52-newcomers.md): a tie offered to a mentor (4), with the newcomer's look, the story
// starter from the mentor's side, the town and the seconds left, Accept and Pass; and an innkeeper's prompt to
// introduce oneself (6). Rebuilt only when what it shows changes.
import {arr, isObject, num, obj, str} from '../../game/json.ts';
import type {GameState} from '../../game/state.ts';
import {button, el, show} from './dom.ts';

export class TiePanel {
    private s: GameState;
    private root: HTMLElement;
    private key = '';

    constructor(parent: HTMLElement, state: GameState) {
        this.s = state;
        this.root = el('section', 'fight-panel tie-panel', parent);
        show(this.root, false);
    }

    update() {
        const s = this.s, o = s.tieOffer, p = s.introducePrompt;
        const hunt = obj(obj(s.snapshot, 'battle'), 'huntPanel');
        const left = o ? Math.ceil(o.until - s.clock) : 0;
        if (o && left <= 0) s.tieOffer = null;
        if (p && p.until <= s.clock) s.introducePrompt = null;
        const key = JSON.stringify([o && left > 0 ? [o.tie, left] : null, s.introducePrompt?.text ?? null, hunt && [arr(hunt, 'asks'), arr(hunt, 'nearby')]]);
        if (key === this.key) return;
        this.key = key;
        this.root.replaceChildren();
        let any = false;
        if (o && left > 0) {
            any = true;
            const r = el('div', 'fight-row', this.root);
            el('span', 'tie-alert', r, `A tie for you: ${o.look}, new${o.town ? ` in ${o.town}` : ''}. ${o.starter} · ${left} s`);
            button('Accept', 'act fight-go', r, () => s.answerTie('accept'));
            button('Pass', 'act', r, () => s.answerTie('pass'));
        }
        // A hunter's panel (doc 53): wolves asking to join (Let in, Not now), and wolves near enough to invite.
        if (hunt) {
            for (const a of arr(hunt, 'asks').filter(isObject)) {
                any = true;
                const r = el('div', 'fight-row hunt-ask', this.root);
                el('span', 'tie-alert', r, `${str(a, 'name')} asks to join your hunt · ${num(a, 'left')} s`);
                button('Let in', 'act fight-go', r, () => s.sendBattle('letIn', {target: str(a, 'id')}));
                button('Not now', 'act', r, () => s.sendBattle('notNow', {target: str(a, 'id')}));
            }
            const near = arr(hunt, 'nearby').filter(isObject);
            if (near.length) {
                any = true;
                const r = el('div', 'fight-row hunt-nearby', this.root);
                el('span', 'label', r, 'Near the hunt:');
                for (const w of near.slice(0, 6))
                    button(`Invite ${str(w, 'name')}`, 'act', r, () => s.sendBattle('invite', {target: str(w, 'id')}));
            }
        }
        if (s.introducePrompt) {
            any = true;
            const r = el('div', 'fight-row introduce-prompt', this.root);
            el('span', 'tie-alert', r, s.introducePrompt.text);
            button('Introduce yourself', 'act fight-go', r, () => s.answerIntroduce(true));
            button('Not now', 'act', r, () => s.answerIntroduce(false));
        }
        show(this.root, any);
    }
}
