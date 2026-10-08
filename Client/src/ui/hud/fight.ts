// The fight panel under the map (Docs/Design/33-combat.md), out of a fight: a challenge to answer, being Downed, and the
// fights in sight to join or watch. Rebuilt only when what it shows changes. In a fight, the fight screen (combat.ts).
import {clockLabel, termsWords} from '../../game/battle.ts';
import {arr, bool, isObject, num, obj, str} from '../../game/json.ts';
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
        const share = obj(self, 'huntShare');
        const work = obj(self, 'work');
        const key = JSON.stringify([s.challenge && [s.challenge.name, Math.ceil(s.challenge.left)], downedLeft > 0 && [Math.ceil(downedLeft),
            bool(self, 'canStruggle'), bool(self, 'struggling')], s.fights, share, work, obj(self, 'giveOffer'), obj(self, 'groomOffer'), obj(self, 'lendOffer')]);
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
        // A grooming asked (doc 55, 7): accept or decline it.
        const ask = obj(self, 'groomOffer');
        if (ask) {
            any = true;
            const r = row();
            el('span', 'fight-alert', r, `${str(ask, 'name')} would groom you · ${Math.ceil(num(ask, 'left'))} s`);
            button('Accept', 'act fight-go', r, () => s.send({type: 'groomAnswer', accept: true}));
            button('Decline', 'act', r, () => s.send({type: 'groomAnswer', accept: false}));
        }
        // A loan offered (doc 55, 8): accept or decline it.
        const loan = obj(self, 'lendOffer');
        if (loan) {
            any = true;
            const r = row();
            el('span', 'fight-alert', r, `${str(loan, 'name')} would lend you ${str(loan, 'what')} for ${num(loan, 'days')} days · ${Math.ceil(num(loan, 'left'))} s`);
            button('Accept', 'act fight-go', r, () => s.send({type: 'lendAnswer', accept: true}));
            button('Decline', 'act', r, () => s.send({type: 'lendAnswer', accept: false}));
        }
        // A gift offered (doc 55): accept or decline it.
        const offer = obj(self, 'giveOffer');
        if (offer) {
            any = true;
            const r = row();
            el('span', 'fight-alert', r, `${str(offer, 'name')} offers you ${str(offer, 'what')} · ${Math.ceil(num(offer, 'left'))} s`);
            button('Accept', 'act fight-go', r, () => s.send({type: 'giveAnswer', accept: true}));
            button('Decline', 'act', r, () => s.send({type: 'giveAnswer', accept: false}));
        }
        // Working together (doc 53): "Foraging with Bo · ×1.8", who does what, and Leave.
        if (work) {
            any = true;
            const r = row();
            r.classList.add('work-row');
            const others = arr(work, 'members').filter(isObject).filter(m => str(m, 'name') !== 'you');
            // Farm work (doc 53, 2.6) also says what it has paid, and when the next spell is counted.
        const farm = str(work, 'farmer') !== '';
        const left = Math.max(0, Math.round(num(work, 'nextBeat')));
        const pay = farm ? ` · ${num(work, 'earned')}p earned · next spell in ${Math.floor(left / 60)}:${String(left % 60).padStart(2, '0')}` : '';
        el('span', 'label', r, `${str(work, 'name')} with ${others.map(m => str(m, 'name')).join(', ')} · ×${num(work, 'rate').toFixed(1)}${pay}`);
            el('span', 'muted small', r, arr(work, 'members').filter(isObject).map(m => `${str(m, 'name')} ${str(m, 'role')}`).join(' · '));
            // Keep watch (doc 53, 4): out in the wild, one may watch over the others instead of working.
        if (str(work, 'kind') === 'forage') {
            const watching = bool(obj(s.snapshot, 'self'), 'keepingWatch');
            button(watching ? 'Back to work' : 'Keep watch', 'act', r, () => s.send({type: 'work', verb: 'watch', on: !watching})).title =
                'Watch over the others: a creeping bandit must get past you too';
        }
        button('Leave', 'act', r, () => s.send({type: 'work', verb: 'leave'}));
        }
        // One's share of the last hunt (doc 53): Give my share to another who hunted with you, to carry.
        if (share && arr(share, 'hunters').length) {
            any = true;
            const r = row();
            el('span', 'label', r, `Your share of the hunt: ${arr(share, 'goods').filter((g): g is string => typeof g === 'string').join(', ')}`);
            for (const h of arr(share, 'hunters').filter(isObject))
                button(`Give my share to ${str(h, 'name')}`, 'act', r, () => s.sendBattle('giveShare', {target: str(h, 'id')}));
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
            if (f.hunt) {
                // A hunt (doc 53): Join hunt, or Ask to join when it is closed; never a button for the game.
                el('span', 'label', r, `${f.starter ? `${f.starter}'s hunt` : 'A hunt'} · ${f.hunters} hunting · ${f.taken} taken`);
                if (f.canJoin) button('Join hunt', 'act', r, () => s.sendBattle('join', {battle: f.id, side: 0}));
                else if (f.canAsk) button('Ask to join', 'act', r, () => s.sendBattle('ask', {battle: f.id})).title =
                    'Its hunters may let you in';
                continue;
            }
            const sides = f.names[0] && f.names[1] ? `${f.names[0]} v ${f.names[1]}` : 'A fight';
            el('span', 'label', r, f.spar ? `${sides} · a spar · round ${f.round} · ${f.watchers} watching`
                : `${sides} · ${f.standing[0]} v ${f.standing[1]} standing · round ${f.round}`);
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
