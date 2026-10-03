// The fight panel under the map (Docs/Design/33-combat.md): in a fight, whose turn it is, the turn order, what this
// wolf can do and the latest of the fight; out of one, a challenge to answer, being Downed, and the fights in sight
// to join or watch. Rebuilt only when what it shows changes.
import {clockLabel, myTurn} from '../../game/battle.ts';
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
        const s = this.s, b = s.battle, self = obj(s.snapshot, 'self');
        const downedLeft = num(self, 'downedLeft');
        const key = JSON.stringify([b && [b.id, b.over, b.banner, b.turn, Math.ceil(b.turnLeft), b.moved, b.acted, b.observer, b.status,
            b.struggling, b.canStruggle, b.order, b.watching, b.log.at(-1)?.seq, b.fighters.map(f => [f.id, f.label, f.status])],
            s.challenge && [s.challenge.name, Math.ceil(s.challenge.left)], downedLeft > 0 && [Math.ceil(downedLeft),
            bool(self, 'canStruggle'), bool(self, 'struggling')], b ? [] : s.fights]);
        if (key === this.key) return;
        this.key = key;
        this.root.replaceChildren();
        let any = false;
        const row = () => el('div', 'fight-row', this.root);
        if (s.challenge) {
            any = true;
            const r = row();
            el('span', 'fight-alert', r, `${s.challenge.name} challenges you to a fight · ${Math.ceil(s.challenge.left)} s`);
            button('Accept', 'act fight-go', r, () => s.sendAction('accept'));
            button('Decline', 'act', r, () => s.sendAction('decline'));
        }
        if (b) {
            any = true;
            const name = (id: string) => b.fighters.find(f => f.id === id)?.name ?? '?';
            const head = row();
            el('span', 'label gold', head, b.observer ? 'WATCHING A FIGHT' : 'FIGHT');
            if (b.over) el('span', 'fight-alert', head, b.banner || 'The fight is over');
            else if (myTurn(b, s.selfId)) el('span', 'fight-turn', head, `Your turn · ${Math.ceil(b.turnLeft)} s`);
            else if (b.turn) el('span', 'muted', head, `${b.turnName || name(b.turn)}'s turn`);
            if (b.watching > 0) el('span', 'muted small', head, `${b.watching} watching`);
            // The turn order: the next six turns.
            const order = row();
            el('span', 'label muted', order, 'NEXT');
            const me = b.fighters.find(f => f.id === s.selfId);
            b.order.forEach((id, i) => {
                const f = b.fighters.find(o => o.id === id);
                const chip = el('span', 'fight-chip', order, f ? f.name : id);
                if (i === 0) chip.classList.add('now');
                if (f && me && f.side !== me.side) chip.classList.add('foe');
                if (f?.id === s.selfId) chip.classList.add('you');
            });
            // What this wolf can do.
            const acts = row();
            if (b.observer) button('Stop watching', 'act', acts, () => s.sendBattle('leave'));
            else if (!b.over && myTurn(b, s.selfId)) {
                if (b.status === 'downed') {
                    if (b.canStruggle) button('Struggle up', 'act fight-go', acts, () => s.sendBattle('struggle'));
                    el('span', 'muted small', acts, 'You are down. Click a tile to crawl one.');
                } else {
                    el('span', 'muted small', acts, b.moved ? 'Click a foe next to you to bite.' : 'Click a lit tile to move, a foe next to you to bite.');
                    button('Flee', 'act', acts, () => s.sendBattle('flee')).title = 'From the arena\'s edge: out of this fight for good';
                }
                button(b.moved || b.acted ? 'End turn' : 'Wait', 'act', acts, () => s.sendBattle('wait'));
            } else if (!b.over && b.status === 'downed')
                el('span', 'muted small', acts, b.struggling ? 'You are trying to get up.' : 'You are down.');
            // Who is in it, and how they are.
            const who = row();
            for (const f of b.fighters) {
                const tag = el('span', `fight-who${me && f.side !== me.side ? ' foe' : ''}${f.status !== 'fighting' ? ' fallen' : ''}`, who,
                    `${f.id === s.selfId ? 'You' : f.name} · ${f.label}${f.downedLeft > 0 ? ` (${clockLabel(f.downedLeft)})` : ''}${f.away ? ' · away' : ''}`);
                tag.title = `${f.health} health`;
            }
            const log = el('div', 'fight-log', this.root);
            for (const line of b.log.slice(-5)) el('div', `fight-line ${line.kind}`, log, line.text);
        } else {
            if (downedLeft > 0) {
                any = true;
                const r = row();
                el('span', 'fight-alert', r, `You are down · ${clockLabel(downedLeft)} left`);
                if (bool(self, 'struggling')) el('span', 'muted small', r, 'You are struggling to get up…');
                else if (bool(self, 'canStruggle')) button('Struggle up', 'act fight-go', r, () => s.sendAction('struggle'));
                else el('span', 'muted small', r, 'Only someone tending your wounds can get you up.');
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
        }
        show(this.root, any);
    }
}
