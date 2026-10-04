// The fight panel under the map (Docs/Design/33-combat.md), out of a fight: a challenge to answer, being Downed, and the
// fights in sight to join or watch. Rebuilt only when what it shows changes. In a fight, the fight screen (combat.ts).
import {clockLabel, termsWords} from '../../game/battle.ts';
import {bool, num, obj} from '../../game/json.ts';
import type {GameState} from '../../game/state.ts';
import {button, el, show} from './dom.ts';

export class FightPanel {
    private s: GameState;
    private root: HTMLElement;
    private key = '';

    constructor(parent: HTMLElement, state: GameState) {
        this.s = state;
        this.root = el('section', 'fight-panel', parent);
        show(this.root, false);
    }

    update() {
        const s = this.s, self = obj(s.snapshot, 'self');
        if (s.battle) {
            // In a fight, the fight screen shows all of it (combat.ts).
            show(this.root, false);
            this.key = '';
            return;
        }
        const downedLeft = num(self, 'downedLeft');
        const key = JSON.stringify([s.challenge && [s.challenge.name, Math.ceil(s.challenge.left)], downedLeft > 0 && [Math.ceil(downedLeft),
            bool(self, 'canStruggle'), bool(self, 'struggling')], s.fights]);
        if (key === this.key) return;
        this.key = key;
        this.root.replaceChildren();
        let any = false;
        const row = () => el('div', 'fight-row', this.root);
        if (s.challenge) {
            any = true;
            const r = row();
            el('span', 'fight-alert', r, `${s.challenge.name} challenges you to a fight ${termsWords(s.challenge.terms)} · ${Math.ceil(s.challenge.left)} s`);
            button('Accept', 'act fight-go', r, () => s.sendAction('accept'));
            button('Decline', 'act', r, () => s.sendAction('decline'));
        }
        if (downedLeft > 0) {
            any = true;
            const r = row();
            const downs = num(self, 'downsSinceRest');
            el('span', 'fight-alert', r, `You are down · up in ${clockLabel(downedLeft)}`);
            if (downs >= 2) el('span', 'muted small', r, `Down ${downs} times without a full rest: it takes longer each time`);
            if (bool(self, 'struggling')) el('span', 'muted small', r, 'You are struggling to get up…');
            else if (bool(self, 'canStruggle')) button('Struggle up', 'act fight-go', r, () => s.sendAction('struggle'));
            else el('span', 'muted small', r, 'You get up when your time is up, or sooner if someone tends your wounds.');
        }
        for (const f of s.fights) {
            any = true;
            const r = row();
            const sides = f.names[0] && f.names[1] ? `${f.names[0]} v ${f.names[1]}` : 'A fight';
            el('span', 'label', r, `${sides} · ${f.standing[0]} v ${f.standing[1]} standing · round ${f.round}`);
            if (f.canJoin) {
                if (f.names[0]) button(`Join ${f.names[0]}`, 'act', r, () => s.sendBattle('join', {battle: f.id, side: 0}));
                if (f.names[1]) button(`Join ${f.names[1]}`, 'act', r, () => s.sendBattle('join', {battle: f.id, side: 1}));
            }
            if (f.watching) button('Stop watching', 'act', r, () => s.sendBattle('leave'));
            else if (f.canObserve) button('Watch', 'act', r, () => s.sendBattle('observe', {battle: f.id})).title =
                'Watch without a body. Having watched, you can only ever watch this fight.';
        }
        show(this.root, any);
    }
}
