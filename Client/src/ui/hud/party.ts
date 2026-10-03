// The Party panel (Docs/Design/32-parties-chapters-factions.md, Part 2): an invitation to answer, a party mate's fight
// calling (with Stay out), who is in the party and how they are, and the leader's choices. Hidden when there is none.
import type {GameState} from '../../game/state.ts';
import {inParty} from '../../game/party.ts';
import {button, el, setText, show} from './dom.ts';
import {noRect} from './story.ts';

export class PartyPanel {
    readonly root: HTMLElement;
    private s: GameState;
    private count: HTMLElement;
    private invite: HTMLElement;
    private inviteText: HTMLElement;
    private pull: HTMLElement;
    private pullText: HTMLElement;
    private list: HTMLElement;
    private foot: HTMLElement;
    private goal: HTMLElement;
    private goalInput: HTMLInputElement;
    private listKey = '';

    constructor(parent: HTMLElement, state: GameState) {
        this.s = state;
        const act = (target: string) => state.activate({rect: noRect, action: 'party_verb', target});
        this.root = el('section', 'panel party', parent);
        const head = el('div', 'panel-head', this.root);
        el('span', 'label gold', head, 'PARTY');
        this.count = el('span', 'label muted', head);
        this.pull = el('div', 'party-call', this.root);
        this.pullText = el('span', '', this.pull);
        button('STAY OUT', 'small', this.pull, () => act('stayout'));
        this.invite = el('div', 'party-invite', this.root);
        this.inviteText = el('span', '', this.invite);
        const answers = el('span', 'party-answers', this.invite);
        button('ACCEPT', 'small', answers, () => act('accept'));
        button('DECLINE', 'small', answers, () => act('decline'));
        this.goal = el('div', 'party-goal muted small', this.root);
        this.list = el('div', 'party-list', this.root);
        this.foot = el('div', 'party-foot', this.root);
        // Where the party is bound (the leader sets it; residents travelling with it are told).
        this.goalInput = document.createElement('input');
        this.goalInput.className = 'name-input';
        this.goalInput.placeholder = 'Where are you bound? (Enter)';
        this.goalInput.maxLength = 120;
        this.goalInput.addEventListener('keydown', e => {
            if (e.key !== 'Enter') return;
            state.activate({rect: noRect, action: 'party_verb', target: `goal:${this.goalInput.value}`});
            this.goalInput.value = '';
            this.goalInput.blur();
        });
    }

    update() {
        const s = this.s, p = s.party, grouped = inParty(p);
        show(this.root, !!p && (grouped || !!p.invite || !!p.pull));
        if (!p) return;
        setText(this.count, grouped ? `${p.members.filter(m => m.online).length} / ${p.members.length}` : '');
        show(this.invite, !!p.invite);
        if (p.invite) setText(this.inviteText, `${p.invite.name} invites you to their party · ${Math.max(0, Math.round(p.invite.seconds))}s`);
        show(this.pull, !!p.pull);
        if (p.pull) setText(this.pullText, `Joining ${p.pull.name}'s fight in ${Math.max(0, Math.round(p.pull.seconds))}…`);
        const leading = p.leader === s.selfId;
        setText(this.goal, p.goal ? `Bound for: ${p.goal}` : '');
        const key = JSON.stringify([p.members, leading, p.autoJoin]);
        if (key === this.listKey) return;
        this.listKey = key;
        const act = (target: string) => s.activate({rect: noRect, action: 'party_verb', target});
        this.list.replaceChildren();
        for (const m of grouped ? p.members : []) {
            const row = el('div', m.online ? 'party-row' : 'party-row away', this.list);
            const who = el('div', 'party-who', row);
            el('span', 'party-name', who, (m.leader ? '★ ' : '') + (m.id === s.selfId ? `${m.name} (you)` : m.name));
            const here = m.online && !!m.cell && m.cell === s.cellId;
            const state = !m.online ? 'away' : m.downed ? 'DOWNED' : m.fighting ? 'fighting' : m.health < 100 ? `${m.health}% health` : '';
            const why = m.npc ? (m.reason === 'hired' ? `hired · ${m.wage}p a day` : m.reason === 'story' ? 'with you for now' : 'a friend') : '';
            el('span', 'muted small', who, [why, here ? 'here' : m.place, m.waiting ? 'waiting' : '', state].filter(Boolean).join(' · '));
            if (m.npc && m.mine) {
                // Orders for a resident travelling with the party.
                const tools = el('span', 'party-tools', row);
                const order = (o: string) => () => s.sendAction(o, m.id);
                button(m.waiting ? 'FOLLOW' : 'WAIT', 'small', tools, order(m.waiting ? 'follow me' : 'wait here'));
                button('HOME', 'small', tools, order('go home')).title = `Send ${m.name} home`;
                button('DISMISS', 'small', tools, order('dismiss')).title = `Part ways with ${m.name}`;
            } else if (leading && m.id !== s.selfId && !m.npc) {
                const tools = el('span', 'party-tools', row);
                button('LEAD', 'small', tools, () => act(`lead:${m.id}`)).title = `Make ${m.name} the leader`;
                button('REMOVE', 'small', tools, () => act(`remove:${m.id}`)).title = `Send ${m.name} from the party`;
            }
        }
        this.foot.replaceChildren();
        if (!grouped) return;
        button('LEAVE', 'small', this.foot, () => act('leave'));
        if (leading) {
            button('DISBAND', 'small', this.foot, () => act('disband'));
            this.foot.append(this.goalInput);
        }
        const auto = button(p.autoJoin ? 'JOIN THEIR FIGHTS: ON' : 'JOIN THEIR FIGHTS: OFF', 'small', this.foot,
            () => act(`autojoin:${p.autoJoin ? 'off' : 'on'}`));
        auto.title = 'Whether a party mate\'s fight you can see calls you in (after five seconds, with a chance to stay out)';
    }
}
