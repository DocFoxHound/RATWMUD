// The fight screen (Docs/Design/37-combat-feel.md, phase 1). While this wolf fights or watches a fight the screen is
// about the fight only: the order turns will come in along the top of the map, a card for each fighter on the right,
// what this wolf can do as icon buttons under the map (keys 1–8, Space to end the turn), and the fight told line by
// line above the composer. Everything else on the screen steps aside (hud.ts: the `fight-mode` class).
import {clockLabel, fightTips, meterNow, myTurn, secondsToTurn, termsWords, TurnSeconds, type BattleLine, type BattleView, type FighterView} from '../../game/battle.ts';
import {lawLabel} from '../../game/labels.ts';
import {arr, bool, isObject, num, obj, str, type Json} from '../../game/json.ts';
import type {GameState} from '../../game/state.ts';
import {drawPortrait, type Portraits} from '../portrait.ts';
import {icon} from '../icons.ts';
import {button, el, setClass, setStyle, setText, show} from './dom.ts';
import {noRect} from './story.ts';
import {drawGear, gearDoll} from './gear.ts';

const ageOf = (stage: string) => (stage === 'young' ? 6 : stage === 'adolescent' ? 13 : stage === 'old' ? 65 : 18);

/** How a fight ended, as shown after it (doc 37, phase 4). */
interface Ended {
    title: string;
    tone: 'win' | 'lose' | 'even';
    banner: string;
    dealt: number;
    taken: number;
    notes: string[];
    fallen: Array<{id: string; name: string}>;
    observer: boolean;
    until: number;              // Shown until then (the page's clock), or closed.
    fight: string;                  // The fight's id: its roleplay review comes as its scene settles (doc 33).
}

/** One of this wolf's actions, as the bar shows it. */
interface Action {
    id: string;
    key: string;                // "1"…"8", "Space", "Q", "E", or "" for none.
    icon: string;
    label: string;
    sub: string;                // A number under the word: odds, a cost.
    tip: string;                // What it does, its key, and why it can't be done now.
    enabled: boolean;
    kind: '' | 'go' | 'end' | 'small' | 'warn';
    run: () => void;
    planned?: boolean;          // Planned for one's next turn (doc 37): it plays as the turn comes.
}

/**
 * A wolf's portrait, drawn again only when its look changes. `head` crops to the head and shoulders (the portrait is a
 * side view facing right), for the round faces of the turn order.
 */
class Face {
    readonly canvas: HTMLCanvasElement;
    private drawn = '';
    constructor(parent: HTMLElement, className: string, w: number, h: number, private head = false) {
        this.canvas = el('canvas', className, parent);
        this.canvas.width = w;
        this.canvas.height = h;
    }
    draw(portraits: Portraits, f: FighterView) {
        const key = JSON.stringify([f.appearance, f.lifeStage]);
        if (key === this.drawn) return;
        const c = this.canvas.getContext('2d');
        if (!c) return;
        c.clearRect(0, 0, this.canvas.width, this.canvas.height);
        const w = this.canvas.width, h = this.canvas.height;
        const drawn = this.head ? drawPortrait(c, portraits, f.appearance, ageOf(f.lifeStage), -w * 1.15, -h * 0.12, w * 2.3, h * 1.56)
            : drawPortrait(c, portraits, f.appearance, ageOf(f.lifeStage), -w * 0.12, -h * 0.16, w * 1.24, h * 1.3);
        if (drawn) this.drawn = key;
    }
}

interface Chip {
    root: HTMLElement;
    face: Face;
    when: HTMLElement;
}

interface Card {
    root: HTMLElement;
    face: Face;
    name: HTMLElement;
    marks: HTMLElement;
    marksKey: string;
    health: HTMLElement;
    healthFill: HTMLElement;
    healthText: HTMLElement;
    stamina: HTMLElement;
    staminaFill: HTMLElement;
    hurts: HTMLElement;             // Their injuries, named (doc 38): chips under the name.
    hurtsKey: string;
    init: HTMLElement;              // The initiative bar: a turn's time running out, or the bar filling toward one.
    initFill: HTMLElement;
    initText: HTMLElement;
    mana: HTMLElement;
    manaFill: HTMLElement;
    odds: HTMLElement;
    oddsKey: string;
    clock: HTMLElement;
    lastHealth: number;
    gear: HTMLElement;              // Armour and weapons on a little doll (doc 35), told on hover.
    gearKey: string;
}

export class CombatScreen {
    private s: GameState;
    private portraits: Portraits;
    // Along the top of the map: whose turn comes when.
    private strip: HTMLElement;
    private stripHead: HTMLElement;
    private stripChips: HTMLElement;
    private stripNote: HTMLElement;
    private chips = new Map<string, Chip>();
    // On the right: the fighters.
    private cards: HTMLElement;
    private cardList = new Map<string, Card>();
    private groupHeads: HTMLElement[] = [];
    private cardsKey = '';
    private litCard = '';
    // Under the map: what this wolf can do.
    private bar: HTMLElement;
    private barKey = '';
    private actions: Action[] = [];
    private endFill: HTMLElement | null = null;
    private endText: HTMLElement | null = null;
    // Above the composer: the fight, told.
    private log: HTMLElement;
    private logKey = '';
    // Over the map: the moments (doc 37, phase 4): who faces whom as it starts, one's turn coming, how it ended.
    private versus: HTMLElement;
    private versusUntil = 0;
    private turnFlash: HTMLElement;
    private wasMine = false;
    private lastBattle = '';
    private result: HTMLElement;
    private ended: Ended | null = null;
    private resultKey = '';
    // Over the map: the first fights' tips (doc 37, phase 7), one at a time, each once per character.
    private tip: HTMLElement;
    private tipText: HTMLElement;
    private tipShown = '';
    private tipSince = 0;

    constructor(state: GameState, portraits: Portraits, parts: {map: HTMLElement; side: HTMLElement; center: HTMLElement; before: HTMLElement;
        story: HTMLElement; storyBefore: HTMLElement}) {
        this.s = state;
        this.portraits = portraits;
        this.strip = el('div', 'turn-strip', parts.map);
        this.stripHead = el('div', 'strip-head', this.strip);
        this.stripChips = el('div', 'strip-chips', this.strip);
        this.stripNote = el('div', 'strip-note label muted', this.strip);
        this.cards = el('section', 'panel fight-cards', parts.side);
        this.bar = el('div', 'action-bar', parts.center);
        parts.center.insertBefore(this.bar, parts.before);
        this.log = el('div', 'fight-story', parts.story);
        parts.story.insertBefore(this.log, parts.storyBefore);
        this.versus = el('div', 'versus', parts.map);
        this.turnFlash = el('div', 'turn-flash', parts.map, 'YOUR TURN');
        this.result = el('div', 'result-card', parts.map);
        this.tip = el('div', 'fight-tip', parts.map);
        this.tipText = el('span', '', this.tip);
        button('×', 'fight-tip-close', this.tip, () => this.tipSeen()).title = 'Got it';
        for (const part of [this.strip, this.cards, this.bar, this.log, this.versus, this.turnFlash, this.result, this.tip]) show(part, false);
        state.fightKeys = code => this.key(code);
    }

    update() {
        const s = this.s, b = s.battle;
        this.updateMoments(b);
        for (const part of [this.strip, this.cards, this.bar, this.log]) show(part, !!b);
        if (!b) {
            this.barKey = this.cardsKey = this.logKey = '';
            this.actions = [];
            return;
        }
        const since = s.clock - s.battleAt;
        this.updateStrip(b, since);
        this.updateCards(b, since);
        this.updateBar(b, since);
        this.updateLog(b);
        this.updateTip(b);
    }

    // ------------------------------------------------------------------ The first fights' tips

    /**
     * A tip for a new fighter (doc 37, phase 7), when it first fits: one at a time, for about seven seconds (or until
     * closed, or what it is about has passed), and once per character. Settings → Fight tips turns them off, or on and
     * shown again.
     */
    private updateTip(b: BattleView | null) {
        const s = this.s, me = b ? this.me(b) : undefined;
        const tips = !b || b.over || b.observer || !me || !s.fightTips || s.clock < this.versusUntil ? [] : fightTips(b, me, myTurn(b, s.selfId));
        if (this.tipShown) {
            const holds = tips.some(t => t.id === this.tipShown);
            const shown = s.clock - this.tipSince;
            if (shown > 7 || (!holds && shown > 1.5) || !b) this.tipSeen();
            return;
        }
        const seen = s.tipsSeen();
        const next = tips.find(t => !seen.includes(t.id));
        if (!next) return;
        this.tipShown = next.id;
        this.tipSince = s.clock;
        setText(this.tipText, next.text);
        this.tip.className = `fight-tip at-${next.where}`;
        show(this.tip, true);
    }

    private tipSeen() {
        if (this.tipShown) this.s.markTipSeen(this.tipShown);
        this.tipShown = '';
        show(this.tip, false);
    }

    // ------------------------------------------------------------------ Whose turn comes when

    private updateStrip(b: BattleView, since: number) {
        const s = this.s, me = this.me(b);
        const order = b.fighters.filter(f => f.status === 'fighting' || f.status === 'downed')
            .map(f => ({f, wait: secondsToTurn(f, since)}))
            .sort((a, c) => (a.f.acting === c.f.acting ? a.wait - c.wait : a.f.acting ? -1 : 1));
        const present = new Set<string>();
        let before: Element | null = this.stripChips.firstElementChild;
        for (const {f, wait} of order) {
            present.add(f.id);
            let chip = this.chips.get(f.id);
            if (!chip) {
                const root = el('div', 'strip-chip');
                const face = new Face(root, 'strip-face', 64, 64, true);
                const when = el('span', 'strip-when', root);
                root.addEventListener('mouseenter', () => (s.highlight = f.id));
                root.addEventListener('mouseleave', () => {
                    if (s.highlight === f.id) s.highlight = '';
                });
                chip = {root, face, when};
                this.chips.set(f.id, chip);
            }
            if (chip.root !== before) this.stripChips.insertBefore(chip.root, before);
            before = chip.root.nextElementSibling;
            chip.face.draw(this.portraits, f);
            const self = f.id === s.selfId;
            const foe = !!me && f.side !== me.side;
            setClass(chip.root, 'self', self);
            setClass(chip.root, 'foe', foe);
            setClass(chip.root, 'friend', !self && !foe);
            setClass(chip.root, 'acting', f.acting && !b.over);
            setClass(chip.root, 'down', f.status === 'downed');
            setClass(chip.root, 'lit', s.highlight === f.id || s.hoveredEntity === f.id);
            const left = f.acting ? Math.max(0, f.turnLeft - since) : 0;
            // The ring: a turn's time running out, or a bar filling toward one.
            const ring = f.acting ? (f.npc ? 1 : Math.min(1, left / TurnSeconds)) : Math.min(1, Math.max(0, 1 - wait / 15));
            setStyle(chip.root, '--p', ring.toFixed(3));
            setText(chip.when, b.over ? '' : f.acting ? (f.npc ? '' : `${Math.ceil(left)}`) : Number.isFinite(wait) ? `${Math.ceil(wait)}` : '');
            chip.root.title = `${self ? 'You' : f.name}${f.acting ? ' · acting now' : Number.isFinite(wait) ? ` · turn in ${Math.ceil(wait)} s` : ''}`;
        }
        for (const [id, chip] of this.chips)
            if (!present.has(id)) {
                chip.root.remove();
                this.chips.delete(id);
            }
        // The headline: one's own state, in a word or two.
        const mine = myTurn(b, s.selfId);
        const wait = me ? secondsToTurn(me, since) : 0;
        const head = b.over ? 'OVER' : b.observer ? 'WATCHING' : !me ? '' : me.status === 'downed' && !mine ? 'YOU ARE DOWN'
            : mine ? 'YOUR TURN' : Number.isFinite(wait) ? `YOUR TURN IN ${Math.ceil(wait)}${b.planning ? (b.plan ? ' · PLANNED' : ' · PLAN IT') : ''}` : 'WAITING';
        setText(this.stripHead, head);
        setClass(this.stripHead, 'mine', mine);
        // No one deciding, the bars fill faster (doc 37): said, so a quicker turn isn't a surprise.
        setText(this.stripNote, [`ROUND ${Math.max(1, b.round)}`, b.haste > 1 && !b.over ? `» ×${b.haste}` : '',
            b.watching > 0 ? `${b.watching} watching` : ''].filter(Boolean).join(' · '));
        const hasteTip = b.haste > 1 ? 'No one is deciding a turn: every bar fills faster until someone\'s turn comes' : '';
        if (this.stripNote.title !== hasteTip) this.stripNote.title = hasteTip;
    }

    // ------------------------------------------------------------------ The fighters

    private updateCards(b: BattleView, since: number) {
        const s = this.s, me = this.me(b);
        const mySide = me ? me.side : 0;
        const shown = b.fighters.filter(f => f.status !== 'fled');
        // In the order turns come: those acting now on top, then the rest as their bars will fill; the dead last.
        const wait = (f: FighterView) => (f.status === 'dead' || f.status === 'yielded' ? Infinity : secondsToTurn(f, since));
        const acting = shown.filter(f => f.acting && !b.over);
        const coming = shown.filter(f => !(f.acting && !b.over)).sort((a, c) => wait(a) - wait(c) || a.side - c.side);
        const groups = [acting, coming];
        const key = JSON.stringify([b.observer, groups.map(g => g.map(f => f.id))]);
        if (key !== this.cardsKey) {
            this.cardsKey = key;
            this.cards.replaceChildren();
            this.groupHeads = [];
            const names = ['ACTING NOW', 'COMING UP'];
            groups.forEach((group, i) => {
                if (!group.length) return;
                this.groupHeads.push(el('div', 'label muted cards-head', this.cards, names[i]));
                for (const f of group) this.cards.append(this.card(f).root);
            });
            for (const id of [...this.cardList.keys()]) if (!shown.some(f => f.id === id)) this.cardList.delete(id);
        }
        const mine = myTurn(b, s.selfId);
        const target = this.s.fightTargetId();
        for (const f of shown) {
            const c = this.card(f);
            c.face.draw(this.portraits, f);
            const self = f.id === s.selfId, foe = !b.observer && f.side !== mySide;
            setText(c.name, self ? 'You' : f.name);
            setClass(c.root, 'foe', foe || (b.observer && f.side !== mySide));
            setClass(c.root, 'self', self);
            setClass(c.root, 'acting', f.acting && !b.over);
            setClass(c.root, 'down', f.status === 'downed' || f.status === 'dead' || f.status === 'yielded');
            setClass(c.root, 'target', foe && f.id === target);
            setClass(c.root, 'lit', s.highlight === f.id || s.hoveredEntity === f.id);
            if (s.hoveredEntity === f.id && this.litCard !== f.id) {
                c.root.scrollIntoView({block: 'nearest'});   // Pointed at on the map: its card in sight.
                this.litCard = f.id;
            } else if (!s.hoveredEntity) this.litCard = '';
            const health = Math.max(0, Math.min(100, f.health));
            setStyle(c.healthFill, 'width', `${health}%`);
            setStyle(c.health.firstElementChild as HTMLElement, 'width', `${health}%`);
            if (c.lastHealth >= 0 && health < c.lastHealth && !s.reducedMotion) {
                // Struck: the card shudders (the animation restarted each blow).
                c.root.classList.remove('struck');
                void c.root.offsetWidth;
                c.root.classList.add('struck');
            }
            c.lastHealth = health;
            setText(c.healthText, f.status === 'dead' ? 'dead' : f.status === 'downed' ? 'down' : `${Math.round(health)}`);
            // What is wrong with them, named: chips under the name, each saying what it does (never drawn on the wolf).
            const hurtsKey = f.injuries.map(i => `${i.kind}|${i.does}`).join();
            if (hurtsKey !== c.hurtsKey) {
                c.hurtsKey = hurtsKey;
                c.hurts.replaceChildren();
                for (const i of f.injuries) el('span', `hurt ${i.kind}`, c.hurts, i.name).title = `${i.name}: ${i.does}`;
            }
            show(c.hurts, f.injuries.length > 0);
            const stats = self ? b.stats : null;
            const hurtNames = f.injuries.map(i => i.name.toLowerCase()).join(', ');
            c.health.title = `Health ${Math.round(health)} of 100 · ${f.label}. Bites take about 12 (more with STR), a sword 20, fire more; ` +
                `at 0 a wolf goes down. Hurt shortens the move (by up to 60%) and slows a wolf in the world; Wounded (under 75) ` +
                `gets ¾ of the stamina back a turn, Badly hurt (under 50) half.${hurtNames ? ` Now: ${hurtNames}.` : ''}`;
            show(c.stamina, f.stamina >= 0);
            if (f.stamina >= 0) {
                setStyle(c.staminaFill, 'width', `${Math.max(0, Math.min(100, f.stamina))}%`);
                c.stamina.title = `Stamina ${Math.round(f.stamina)} of 100 · back ${f.regen} at the start of their next turn ` +
                    `(4 + STR ÷ 10${stats ? `, STR ${stats.str}` : ''}; ¾ Wounded, ½ Badly hurt${f.resting ? '; doubled: resting this turn' : ''}). ` +
                    `Costs: a bite 8, a sword 14, running faster than a trot ${self ? (b.tileStamina > 0 ? `${b.tileStamina.toFixed(1)} a tile at this pace` : 'by the tile (nothing at this pace)') : 'by the tile'}. ` +
                    `At 0 a wolf is winded: it can only walk, and can't bite or swing, until 20.`;
            }
            show(c.mana, f.mana >= 0 && f.manaMax > 0);
            if (f.mana >= 0 && f.manaMax > 0) {
                setStyle(c.manaFill, 'width', `${Math.max(0, Math.min(100, (f.mana / f.manaMax) * 100))}%`);
                c.mana.title = `Mana ${Math.floor(f.mana)} of ${f.manaMax} (20 + WIS × 0.8${stats ? `, WIS ${stats.wis}` : ''}) · +2 at the start ` +
                    `of each turn, and slowly out of a fight. Fire costs 25 (Quickened 40); too little, and it burns its caster twice as much.`;
            }
            // Little marks for what is going on with them.
            const marks: [string, string][] = [];
            if (f.burning > 0) marks.push(['fire', `Burning: ${f.burning} more turn${f.burning === 1 ? '' : 's'}`]);
            if (f.casting) marks.push(['fire', 'Gathering fire']);
            if (f.mouth === 'sword') marks.push(['sword', 'A sword in their jaws']);
            if (f.away) marks.push(['away', 'Away: their turns are skipped']);
            if (f.truce) marks.push(['truce', 'Agreed to the truce']);
            if (f.armour)
                marks.push(['armour', `Armour, where a blow lands: ${f.armour.zones.map(z => `${z.zone}, ${z.piece.toLowerCase()} (${z.thrust} off a bite, ${z.cut} off a cut)`).join(' · ')}. ` +
                    `Blows elsewhere get through whole; at least a quarter always does${f.armour.dex < 0 ? ` · its weight slows their bar as DEX −${-f.armour.dex}` : ''}`]);
            if (f.guarding) marks.push(['guard', 'On guard: harder to hit, and turns to meet a blow, until their next turn']);
            const marksKey = marks.map(m => m[0]).join();
            if (marksKey !== c.marksKey) {
                c.marksKey = marksKey;
                c.marks.replaceChildren();
                for (const [name, tip] of marks) {
                    const m = el('span', `mark ${name}`, c.marks);
                    m.append(icon(name));
                    m.title = tip;
                }
            }
            const gearKey = JSON.stringify(f.gear);
            if (gearKey !== c.gearKey) {
                c.gearKey = gearKey;
                drawGear(c.gear, f.gear);
            }
            // How this wolf would fare against a foe, from where it stands.
            const oddsKey = f.odds && !b.observer && !b.over ? `${f.odds.hit}|${f.odds.damage}|${f.odds.reach}|${b.mouth}` : '';
            if (oddsKey !== c.oddsKey) {
                c.oddsKey = oddsKey;
                c.odds.replaceChildren();
                if (f.odds) {
                    c.odds.append(icon(b.mouth === 'sword' ? 'sword' : 'bite'));
                    el('span', 'odds-hit', c.odds, `${f.odds.hit}%`);
                    el('span', 'odds-dmg', c.odds, `~${f.odds.damage}`);
                    c.odds.title = f.odds.reach ? `In reach: ${f.odds.hit}% to land a blow of about ${f.odds.damage}`
                        : `Out of reach: you would step in first. ${f.odds.hit}% from here`;
                }
            }
            show(c.odds, !!oddsKey);
            setClass(c.odds, 'far', !!f.odds && !f.odds.reach);
            const left = f.acting ? Math.max(0, f.turnLeft - since) : 0;
            // Initiative: acting, the turn's time running out; else the bar filling toward their turn.
            const due = wait(f);
            const fillInit = f.acting && !b.over ? (f.npc ? 1 : Math.min(1, left / TurnSeconds)) : meterNow(f, since);
            setStyle(c.initFill, 'width', `${Math.round(fillInit * 100)}%`);
            setClass(c.init, 'acting', f.acting && !b.over);
            c.init.title = `Initiative: their bar fills in about ${f.fillSeconds} s (6 + DEX ÷ 10${stats ? `, DEX ${stats.dex}` +
                (stats.dex !== stats.baseDex ? ` (${stats.baseDex} before age and injury)` : '') : ''}); full, it is their turn, alongside anyone ` +
                `else's. After a turn it starts at 0, +20 for not moving, +20 for not acting, less a heavy blow's weight (sword 10)` +
                `${f.injuries.some(i => i.kind === 'staggered') ? '; staggered: set back 20' : ''}.`;
            setText(c.initText, b.over ? '' : f.acting ? (f.npc ? 'acting' : `acting · ${Math.ceil(left)} s`)
                : Number.isFinite(due) ? `turn in ${Math.ceil(due)} s` : '');
            const clock = f.status === 'downed' && f.downedLeft > 0 ? `${f.npc ? 'bleeding' : 'up in'} · ${clockLabel(Math.max(0, f.downedLeft - since))}`
                : '';
            setText(c.clock, clock);
            c.root.title = self ? 'You · click for your status, belongings and equipment' : foe ? (mine ? 'Click to aim at them' : 'Click to aim at them on your turn')
                : f.status === 'downed' && mine ? 'Click to tend their wounds' : '';
        }
    }

    private card(f: FighterView): Card {
        const found = this.cardList.get(f.id);
        if (found) return found;
        const s = this.s;
        const root = el('div', 'fcard');
        const left = el('div', 'fcard-left', root);
        const face = new Face(left, 'fcard-face', 96, 66);
        const gear = gearDoll(left, 'fcard-gear');
        const body = el('div', 'fcard-body', root);
        const top = el('div', 'fcard-top', body);
        const name = el('span', 'fcard-name', top);
        const marks = el('span', 'fcard-marks', top);
        const hurts = el('div', 'fcard-hurts', body);
        const health = el('div', 'meter health', body);
        el('div', 'lag', health);                       // What a blow took, draining after it (styles.css).
        const healthFill = el('div', 'fill', health);
        const healthText = el('span', 'meter-text', health);
        const stamina = el('div', 'meter thin stamina', body);
        const staminaFill = el('div', 'fill', stamina);
        const init = el('div', 'meter init', body);
        const initFill = el('div', 'fill', init);
        const initText = el('span', 'meter-text', init);
        init.title = 'Initiative: when their bar is full, it is their turn';
        const mana = el('div', 'meter thin mana', body);
        const manaFill = el('div', 'fill', mana);
        const foot = el('div', 'fcard-foot', body);
        const odds = el('span', 'odds', foot);
        const clock = el('span', 'fcard-clock', foot);
        root.addEventListener('mouseenter', () => (s.highlight = f.id));
        root.addEventListener('mouseleave', () => {
            if (s.highlight === f.id) s.highlight = '';
        });
        root.addEventListener('mousedown', e => e.preventDefault());
        root.addEventListener('click', () => {
            if (f.id === s.selfId) {
                s.activate({rect: noRect, action: 'status', target: ''});   // One's own card: one's status, belongings, equipment.
                return;
            }
            const b = s.battle, me = b && this.me(b), now = b?.fighters.find(o => o.id === f.id);
            if (!b || !me || !now || b.observer) return;
            if (now.side !== me.side && now.status === 'fighting') s.fightFocus = f.id;
            else if (now.side === me.side && now.status === 'downed' && myTurn(b, s.selfId)) s.fightTarget(f.id);
        });
        const card: Card = {root, face, name, marks, marksKey: '-', health, healthFill, healthText, stamina, staminaFill, hurts, hurtsKey: '-', init, initFill, initText, mana, manaFill, odds,
            oddsKey: '-', clock, lastHealth: -1, gear, gearKey: '-'};
        this.cardList.set(f.id, card);
        return card;
    }

    // ------------------------------------------------------------------ What this wolf can do

    private updateBar(b: BattleView, since: number) {
        const s = this.s, me = this.me(b), mine = myTurn(b, s.selfId);
        const target = b.fighters.find(f => f.id === this.s.fightTargetId());
        this.actions = this.actionsFor(b, me, mine, target);
        const key = JSON.stringify([b.over, b.banner, this.actions.map(a => [a.id, a.label, a.sub, a.enabled, a.tip, a.kind, a.planned]),
            b.truceBy, b.agreed, b.observer, b.yieldBy, mine, b.moved, b.acted, b.faced, b.resting, s.aiming]);
        if (key !== this.barKey) {
            this.barKey = key;
            this.bar.replaceChildren();
            this.endFill = this.endText = null;
            if (b.over) el('div', 'bar-banner', this.bar, b.banner || 'The fight is over');
            // A truce on the table: answered here, whoever offered it.
            if (!b.over && b.truceBy && !b.observer && me?.status === 'fighting') {
                const t = el('div', 'bar-truce', this.bar);
                t.append(icon('truce'));
                const who = b.truceBy === s.selfId ? 'You offer a truce' : `${b.fighters.find(f => f.id === b.truceBy)?.name ?? 'Someone'} offers a truce`;
                el('span', '', t, who);
                if (!b.agreed) {
                    this.button(t, {id: 'agree', key: '', icon: 'yes', label: 'Agree', sub: '', tip: 'Agree to end the fight here',
                        enabled: true, kind: 'go', run: () => s.sendBattle('agree')});
                    this.button(t, {id: 'refuse', key: '', icon: 'no', label: 'Refuse', sub: '', tip: 'Fight on', enabled: true, kind: '',
                        run: () => s.sendBattle('refuse')});
                } else el('span', 'muted small', t, 'Everyone standing must agree');
            }
            // An offer to yield: the other side answers it here.
            const yielder = b.fighters.find(f => f.id === b.yieldBy);
            if (!b.over && yielder && !b.observer && me) {
                const t = el('div', 'bar-truce', this.bar);
                t.append(icon('yield'));
                if (yielder.id === s.selfId) el('span', '', t, 'You offer to yield · waiting for an answer');
                else if (yielder.side === me.side) el('span', '', t, `${yielder.name} offers to yield`);
                else {
                    el('span', '', t, `${yielder.name} offers to yield`);
                    if (me.status === 'fighting') {
                        this.button(t, {id: 'spare', key: '', icon: 'yes', label: 'Spare them', sub: '', tip: 'Let them out of the fight, on their feet',
                            enabled: true, kind: 'go', run: () => s.sendBattle('spare')});
                        this.button(t, {id: 'press', key: '', icon: 'no', label: 'Press on', sub: '', tip: 'Refuse: the fight goes on', enabled: true, kind: '',
                            run: () => s.sendBattle('press')});
                    }
                }
            }
            const row = el('div', 'bar-actions', this.bar);
            for (const a of this.actions) {
                // The turn's three parts, before End turn: used ones ticked; all three, and the turn ends by itself.
                if (a.kind === 'end' && mine && me?.status === 'fighting') {
                    const parts = el('div', 'turn-parts', row);
                    parts.title = 'A turn is a move, an action (a bite, a strike, fire, tending, an item…) and a facing. ' +
                        'With all three used it ends by itself; or end it sooner. Rest uses the move and the action.';
                    for (const [name, used] of [['MOVE', b.moved], ['ACTION', b.acted], ['FACING', b.faced]] as const)
                        el('span', `turn-part${used ? ' used' : ''}`, parts, `${used ? '✓' : '·'} ${name}`);
                }
                const btn = this.button(row, a);
                if (a.kind === 'end') {
                    this.endFill = el('span', 'end-fill', btn);
                    btn.prepend(this.endFill);
                    this.endText = btn.querySelector('.abtn-sub');
                }
            }
        }
        // The turn's time, running down inside End turn; or one's bar filling toward the next.
        if (this.endFill && me) {
            const left = mine ? Math.max(0, b.turnLeft - since) : 0;
            const wait = secondsToTurn(me, since);
            const share = mine ? Math.min(1, left / TurnSeconds) : Number.isFinite(wait) ? Math.max(0, 1 - wait / 15) : 0;
            setStyle(this.endFill, 'width', `${(share * 100).toFixed(1)}%`);
            if (this.endText) setText(this.endText, mine ? `${Math.ceil(left)} s` : Number.isFinite(wait) ? `in ${Math.ceil(wait)} s` : '');
        }
    }

    private actionsFor(b: BattleView, me: FighterView | undefined, mine: boolean, target: FighterView | undefined): Action[] {
        const s = this.s;
        const out: Action[] = [];
        if (b.over) return out;
        if (b.observer || !me) {
            out.push({id: 'leave', key: '', icon: 'watch', label: 'Stop watching', sub: '', tip: 'Back to the world', enabled: true, kind: '',
                run: () => s.sendBattle('leave')});
            return out;
        }
        if (me.away) {
            out.push({id: 'back', key: 'Space', icon: 'rise', label: "I'm back", sub: '', tip: 'Your turns are being skipped while you are away',
                enabled: true, kind: 'go', run: () => s.sendBattle('back')});
            return out;
        }
        // Waiting for one's turn, it can be planned (doc 37): chosen now, played as the turn comes.
        const planning = !mine && b.planning;
        const plan = b.plan;
        const notYet = mine ? '' : planning ? ' · planned now, played as your turn comes' : ' (on your turn)';
        const end: Action = {id: 'end', key: 'Space', icon: 'end', label: mine ? 'End turn' : plan ? 'Planned' : 'Waiting', sub: '',
            tip: mine ? 'End your turn now (Space): your bar starts filling again at once, sooner if you held back'
                : 'Your bar is filling. Plan your turn meanwhile: click where to go, a foe to strike, or an action',
            enabled: mine, kind: 'end', run: () => s.sendBattle('wait')};
        if (me.status === 'downed') {
            out.push({id: 'struggle', key: '1', icon: 'rise', label: 'Struggle up', sub: b.canStruggle ? 'once a day' : 'spent',
                tip: b.canStruggle ? 'Spend your turn to rise at the start of the next, if nothing hits you (1)'
                    : 'You have no strength left to rise: only someone tending your wounds can get you up',
                enabled: mine && b.canStruggle && !b.struggling, kind: 'go', run: () => s.sendBattle('struggle')});
            out.push(end);
            return out;
        }
        const acted = b.acted, odds = target?.odds;
        const free = (mine && !acted) || planning;     // An action can be taken now, or planned.
        const why = (more: string) => (planning ? 'planned now, played as your turn comes' : !mine ? `Not your turn yet` : acted ? 'You have acted this turn' : more);
        // Now on one's turn; else planned (and, planned already, taken back).
        const doOr = (act: string, now: () => void, target = '') => (mine ? now : () => s.planAction(act, target));
        const isPlanned = (act: string, target = '') => !!plan && plan.act === act && plan.target === target;
        const blow = odds ? `${odds.hit}% · ${odds.damage}` : '';
        const who = target ? ` ${target.name}` : '';
        // 1: the bite, or 2: the sword, at the foe aimed at (stepping in first if they're out of reach).
        if (b.mouth !== 'sword')
            out.push({id: 'bite', key: '1', icon: 'bite', label: 'Bite', sub: blow,
                tip: `Bite${who} (1): 8 breath${odds && !odds.reach ? ', stepping in first' : ''}${!target ? ' · no one to bite' : ''}${!mine || acted ? ` · ${why('')}` : ''}`,
                enabled: free && !!target, kind: '', run: () => target && s.fightTarget(target.id), planned: isPlanned('bite', target?.id)});
        if (b.mouth === 'sword')
            out.push({id: 'sword', key: '2', icon: 'sword', label: 'Sword', sub: blow,
                tip: `Strike${who} with the sword (2): reaches two tiles, 14 breath, slows your next turn${!mine || acted ? ` · ${why('')}` : ''}`,
                enabled: free && !!target, kind: '', run: () => target && s.fightTarget(target.id), planned: isPlanned('sword', target?.id)});
        else if (b.swords > 0)
            out.push({id: 'hold', key: '2', icon: 'sword', label: 'Take sword', sub: 'move',
                tip: `Take a sword in your jaws (2): part of your move, not your action (a tile off the move if taken before it)${b.drew ? ' · done this turn' : ''}${notYet}`,
                enabled: (mine && !b.drew) || planning, kind: '', run: doOr('hold', () => s.sendBattle('hold')), planned: isPlanned('hold')});
        if (b.flame)
            out.push({id: 'fire', key: '3', icon: 'fire', label: 'Fire', sub: `${b.flame.mana} mana`, kind: b.mana < b.flame.mana ? 'warn' : '',
                tip: `Flamethrower (3): aim a cone; it gathers for a few seconds (everyone sees where), costs breath and singes you${b.mana < b.flame.mana ? ' · too little mana: it will burn you twice as much' : ''}${notYet}`,
                enabled: free, run: () => (s.aiming = s.aiming === 'flame' ? '' : 'flame'), planned: plan?.act === 'flame'});
        const fallen = b.fighters.find(f => f.side === me.side && f.id !== me.id && f.status === 'downed');
        if (fallen) {
            const near = Math.max(Math.abs(fallen.x - me.x), Math.abs(fallen.y - me.y)) <= 1;
            out.push({id: 'tend', key: '4', icon: 'tend', label: 'Tend', sub: near ? '10 breath' : 'too far',
                tip: `Tend ${fallen.name}'s wounds (4): they stand at 20 health${near ? '' : ' · get next to them first'}${notYet}`,
                enabled: (mine && !acted && near) || planning, kind: 'go', run: () => s.fightTarget(fallen.id), planned: isPlanned('tend', fallen.id)});
        }
        // Resting: no move and no action this turn, twice the stamina back at the next. Chosen anew each turn (doc 33).
        if (me.status === 'fighting' && !b.casting)
            out.push({id: 'rest', key: 'R', icon: 'rest', label: b.resting ? 'Resting' : 'Rest', sub: `+${(b.resting ? me.regen : me.regen * 2).toFixed(0)} next`,
                tip: b.resting ? 'Resting this turn: twice the stamina back at the start of your next'
                    : `Rest (R): a turn without moving or acting, for twice the stamina back at your next (+${(me.regen * 2).toFixed(0)}). ` +
                      `You can still turn and write. Rest again each turn you mean to${b.moved || acted ? ' · you have already moved or acted' : ''}${notYet}`,
                enabled: (mine && !b.moved && !acted && !b.resting) || planning, kind: b.resting ? 'go' : '', run: doOr('rest', () => s.sendBattle('rest')),
                planned: isPlanned('rest')});
        if (b.burning > 0)
            out.push({id: 'roll', key: '5', icon: 'roll', label: 'Roll', sub: 'put out', tip: `Roll on the ground to put out the flames (5)${notYet}`,
                enabled: free, kind: 'go', run: doOr('roll', () => s.sendBattle('roll')), planned: isPlanned('roll')});
        if (b.drops.length && !b.mouth)
            out.push({id: 'pickup', key: '6', icon: 'pickup', label: 'Pick up', sub: 'sword', tip: `Pick up the sword beside you (6)${notYet}`,
                enabled: free, kind: '', run: doOr('pickup', () => s.sendBattle('pickup')), planned: isPlanned('pickup')});
        if (b.mouth === 'sword')
            out.push({id: 'stow', key: '', icon: 'pickup', label: 'Stow', sub: 'move', tip: `Put the sword away: part of your move, not your action${b.drew ? ' · done this turn' : ''}${notYet}`,
                enabled: (mine && !b.drew) || planning, kind: 'small', run: doOr('stow', () => s.sendBattle('stow')), planned: isPlanned('stow')});
        // Guard (G): no blow, harder to hit and turning to meet one, until one's next turn. Shove (F): the foe aimed at
        // (or anyone next to you) a tile straight back (doc 37).
        if (me.status === 'fighting') {
            out.push({id: 'guard', key: 'G', icon: 'guard', label: me.guarding ? 'On guard' : 'Guard', sub: '−20%',
                tip: `Guard (G): no blow this turn; until your next, blows at you are 20% less likely and you turn to meet them (no side or back to strike)${notYet}`,
                enabled: free && !me.guarding, kind: me.guarding ? 'go' : '', run: doOr('guard', () => s.sendBattle('guard')), planned: isPlanned('guard')});
            const next = target && Math.max(Math.abs(target.x - me.x), Math.abs(target.y - me.y)) <= 1;
            out.push({id: 'shove', key: 'F', icon: 'shove', label: 'Shove', sub: next ? '8 breath' : 'too far',
                tip: `Shove${who} (F): a tile straight back, out of a doorway or toward the edge; strength against strength, harder against one on guard${next ? '' : ' · get next to them first'}${notYet}`,
                enabled: ((mine && !acted && !!next) || planning) && !!target, kind: '',
                run: () => target && (mine ? s.sendBattle('shove', {target: target.id}) : s.planAction('shove', target.id)), planned: isPlanned('shove', target?.id)});
        }
        if (!b.truceBy)
            out.push({id: 'truce', key: '7', icon: 'truce', label: 'Truce', sub: '', tip: 'Offer a truce (7): the fight ends if everyone standing agrees',
                enabled: mine && !acted, kind: '', run: () => s.sendBattle('truce')});
        out.push({id: 'flee', key: '8', icon: 'flee', label: 'Flee', sub: '', kind: '',
            tip: `Flee (8): from the arena's edge only (the red band), out of this fight for good${notYet}`,
            enabled: free, run: doOr('flee', () => s.sendBattle('flee')), planned: isPlanned('flee')});
        if (!b.yieldBy)
            out.push({id: 'yield', key: '9', icon: 'yield', label: 'Yield', sub: '', kind: '',
                tip: 'Yield (9), at any time: you are out of the fight on your feet, if the other side lets you be (residents and the watch do)',
                enabled: true, run: () => s.sendBattle('yield')});
        out.push({id: 'left', key: 'Q', icon: 'left', label: '', sub: '', tip: "Turn left (Q): your turn's facing (turn as often as you like; with the move and the action used, the turn ends a moment after)", enabled: mine, kind: 'small',
            run: () => s.turnInFight(-1)});
        out.push({id: 'right', key: 'E', icon: 'right', label: '', sub: '', tip: "Turn right (E): your turn's facing (turn as often as you like; with the move and the action used, the turn ends a moment after)", enabled: mine, kind: 'small',
            run: () => s.turnInFight(1)});
        if (planning && plan)
            out.push({id: 'unplan', key: '', icon: 'no', label: 'Clear plan', sub: '', tip: 'Take back what you planned for your turn', enabled: true,
                kind: 'small', run: () => s.sendBattle('unplan', {part: ''})});
        out.push(end);
        return out;
    }

    private button(parent: HTMLElement, a: Action): HTMLButtonElement {
        const b = button('', `abtn${a.kind ? ` ${a.kind}` : ''}${this.s.aiming === 'flame' && a.id === 'fire' ? ' armed' : ''}${a.planned ? ' planned' : ''}`, parent, () => {
            if (a.enabled) a.run();
        });
        b.disabled = !a.enabled;
        b.title = a.tip;
        if (a.key && a.key !== 'Q' && a.key !== 'E') el('span', 'abtn-key', b, a.key === 'Space' ? '␣' : a.key);
        b.append(icon(a.icon, 'abtn-icon'));
        if (a.label) el('span', 'abtn-label', b, a.label);
        if (a.sub || a.kind === 'end') el('span', 'abtn-sub', b, a.sub);
        return b;
    }

    /** A key on the map while the fight screen shows: true when it was one of its actions. */
    private key(code: string): boolean {
        if (!this.s.battle) return false;
        const pressed = code === 'Space' ? 'Space' : code === 'KeyR' ? 'R' : code === 'KeyG' ? 'G' : code === 'KeyF' ? 'F'
            : /^(?:Digit|Numpad)([1-9])$/.exec(code)?.[1];
        if (!pressed) return false;
        const a = this.actions.find(x => x.key === pressed);
        if (a?.enabled) a.run();
        return true;
    }

    // ------------------------------------------------------------------ The moments

    private updateMoments(b: BattleView | null) {
        const s = this.s;
        // As it starts: who faces whom, and on what terms; for a moment, while the arena takes the view.
        if (b && b.id !== this.lastBattle) {
            this.lastBattle = b.id;
            this.ended = null;
            this.resultKey = '';
            show(this.result, false);
            if (!b.over) {
                this.buildVersus(b);
                this.versusUntil = s.clock + 2.2;
            }
        }
        show(this.versus, !!b && s.clock < this.versusUntil);
        // One's own turn come round: a word in the middle, briefly (and a chime: fightFx.ts).
        const mine = !!b && myTurn(b, s.selfId);
        if (mine && !this.wasMine && !b!.over && s.clock >= this.versusUntil - 0.6) {
            show(this.turnFlash, true);
            this.turnFlash.classList.remove('go');
            void this.turnFlash.offsetWidth;
            this.turnFlash.classList.add('go');
        }
        if (!mine) show(this.turnFlash, false);
        this.wasMine = mine;
        // As it ends: how it went, what it cost, what follows, and what one might do now.
        if (b && b.over && !this.ended) this.ended = this.summarise(b);
        const ended = this.ended;
        const showing = !!ended && s.clock < ended.until;
        show(this.result, showing);
        if (!showing || !ended) return;
        const key = JSON.stringify([ended.until, !!b, ended.fallen.length, this.review(ended)]);
        if (key === this.resultKey) return;
        this.resultKey = key;
        this.buildResult(ended, !b);
    }

    private buildVersus(b: BattleView) {
        const s = this.s, me = this.me(b);
        const mySide = me ? me.side : 0;
        this.versus.replaceChildren();
        const card = el('div', 'versus-card', this.versus);
        const side = (fighters: FighterView[], cls: string) => {
            const col = el('div', `versus-side ${cls}`, card);
            for (const f of fighters.slice(0, 3)) {
                const who = el('div', 'versus-who', col);
                new Face(who, 'versus-face', 120, 82).draw(this.portraits, f);
                el('div', 'versus-name', who, f.id === s.selfId ? 'You' : f.name);
            }
            if (fighters.length > 3) el('div', 'muted small', col, `and ${fighters.length - 3} more`);
        };
        side(b.fighters.filter(f => f.side === mySide), b.observer ? 'friend' : 'self');
        el('div', 'versus-vs', card, 'VS');
        side(b.fighters.filter(f => f.side !== mySide), 'foe');
        const terms = b.pvp ? `A duel ${termsWords(b.terms)}` : b.crime ? 'An assault: the watch will hear of it' : `A fight ${termsWords(b.terms)}`;
        el('div', 'versus-terms', this.versus, b.observer ? `Watching · ${terms}` : terms);
    }

    /** How the fight went, from this wolf's side: a word for it, what it dealt and took, and what follows. */
    private summarise(b: BattleView): Ended {
        const s = this.s, me = this.me(b);
        const lines = s.encounters.get(b.id)?.lines ?? b.log;
        let dealt = 0, taken = 0;
        for (const line of lines) {
            const n = Number(/\((\d+)\)\.?$/.exec(line.text)?.[1] ?? 0);
            if (!n) continue;
            const blow = ['hit', 'graze', 'slash', 'burnt'].includes(line.kind);
            if (blow && line.actor === s.selfId) dealt += n;
            if ((blow && line.target === s.selfId) || (line.kind === 'burn' && line.actor === s.selfId)) taken += n;
        }
        const even = /truce|lapses/i.test(b.banner);
        const won = !!me && b.fighters.some(f => f.side === me.side && f.status === 'fighting');
        const title = !me ? 'The fight is over' : even ? (/lapses/i.test(b.banner) ? 'It lapses' : 'Truce')
            : won ? (me.status === 'fighting' ? 'You stand' : 'Your side stands')
            : me.status === 'yielded' ? 'You yield' : me.status === 'downed' ? 'You are down' : me.status === 'dead' ? 'You die' : 'Beaten';
        const notes: string[] = [];
        if (me?.status === 'downed') notes.push('Down: you get up when your time is up, sooner if you struggle up or someone tends you.');
        const law = lawLabel(obj(s.snapshot, 'self'));
        if (law) notes.push(law.charAt(0) + law.slice(1).toLowerCase());
        else if (b.crime && me) notes.push('The watch will hear of this.');
        const fallen = me?.status === 'fighting' || won
            ? b.fighters.filter(f => f.id !== s.selfId && f.status === 'downed').map(f => ({id: f.id, name: f.name})) : [];
        return {title, tone: !me || even ? 'even' : won ? 'win' : 'lose', banner: b.banner, dealt, taken, notes, fallen, observer: !me,
            until: s.clock + 16, fight: b.id};
    }

    /** The fight's settled scene, as this wolf sees it: its pay and whom it may star (doc 33's roleplay review). */
    private review(ended: Ended): Json | null {
        const done = obj(obj(obj(this.s.snapshot, 'self'), 'social'), 'ended');
        return done && bool(done, 'fight') && str(done, 'id') === `fight-${ended.fight}` ? done : null;
    }

    private buildResult(ended: Ended, back: boolean) {
        const s = this.s;
        this.result.replaceChildren();
        this.result.className = `result-card ${ended.tone}`;
        el('div', 'result-title', this.result, ended.title);
        el('div', 'result-banner', this.result, ended.banner);
        if (!ended.observer) {
            const stats = el('div', 'result-stats', this.result);
            const stat = (iconName: string, n: number, words: string) => {
                const st = el('span', 'result-stat', stats);
                st.append(icon(iconName));
                el('b', '', st, String(n));
                el('span', 'muted', st, words);
            };
            stat('bite', ended.dealt, 'dealt');
            stat('down', ended.taken, 'taken');
        }
        for (const note of ended.notes) el('div', 'result-note', this.result, note);
        // The roleplay review: a Gold Star to each who played it well, one each (doc 33).
        const review = this.review(ended);
        if (review) {
            const box = el('div', 'result-review', this.result);
            el('div', 'label gold', box, `ROLEPLAY REVIEW · +${num(review, 'xp')} SOCIAL`);
            el('div', 'muted small', box, bool(review, 'talked') ? 'For the fight, and twice for roleplaying it.'
                : 'For the fight. Talk it through: roleplay in a fight pays twice.');
            const targets = arr(review, 'starTargets').filter(isObject), starred = arr(review, 'starred').filter(isObject);
            const row = el('div', 'result-stars', box);
            for (const t of targets)
                button(`★ ${str(t, 'name')}`, 'small', row, () => s.sendSocial({verb: 'star', session: str(review, 'id'), target: str(t, 'id')})).title =
                    `Give ${str(t, 'name')} a Gold Star for how they roleplayed this fight`;
            for (const t of starred) {
                const given = button(`★ ${str(t, 'name')} ✓`, 'small given', row, () => undefined);
                given.disabled = true;
            }
            if (!targets.length && !starred.length) el('span', 'muted small', row, 'No one else to star.');
        }
        const next = el('div', 'result-next', this.result);
        // Back in the world: the fallen may be tended (their timer runs), and the scene written.
        for (const f of ended.fallen)
            this.button(next, {id: `tend-${f.id}`, key: '', icon: 'tend', label: `Tend ${f.name}`, sub: back ? '' : 'in a moment',
                tip: 'Tend their wounds: walk to them; it takes ten seconds', enabled: back, kind: 'go', run: () => s.sendAction('tend', f.id)});
        this.button(next, {id: 'write', key: '', icon: 'start', label: 'Write', sub: '', tip: 'Write what happens next (Enter)', enabled: true, kind: '',
            run: () => s.setChat(true)});
        this.button(next, {id: 'close', key: '', icon: 'no', label: 'Close', sub: '', tip: 'Close this', enabled: true, kind: 'small',
            run: () => {
                if (this.ended) this.ended.until = 0;
            }});
    }

    // ------------------------------------------------------------------ The fight, told

    private updateLog(b: BattleView) {
        const s = this.s, me = this.me(b);
        const lines = s.encounters.get(b.id)?.lines ?? b.log;
        const shown = lines.slice(-40);
        const key = `${b.id}|${lines.length}|${shown.at(-1)?.seq ?? 0}`;
        if (key === this.logKey) return;
        this.logKey = key;
        this.log.replaceChildren();
        el('div', 'label gold fight-story-head', this.log, 'THE FIGHT');
        const list = el('div', 'fight-lines', this.log);
        const sideOf = (id: string) => b.fighters.find(f => f.id === id)?.side;
        for (const line of shown) {
            const row = el('div', `fline ${line.kind}`, list);
            const who = line.actor === s.selfId ? 'self' : me && sideOf(line.actor) !== undefined && sideOf(line.actor) !== me.side ? 'foe'
                : sideOf(line.actor) !== undefined ? 'friend' : '';
            if (who) row.classList.add(who);
            const blade = / (nicks|swings at) /.test(line.text);      // A sword's graze or miss, not a bite's.
            row.append(icon(blade ? 'sword' : LineIcons[line.kind] ?? 'start', 'fline-icon'));
            this.lineText(row, line);
        }
        list.scrollTop = list.scrollHeight;
    }

    /** A line's words, with its number ("(12)") drawn out as a figure. */
    private lineText(row: HTMLElement, line: BattleLine) {
        const text = el('span', 'fline-text', row);
        const m = /^(.*?)\s*\((\d+)\)\.?$/.exec(line.text);
        if (!m) {
            text.textContent = line.text;
            return;
        }
        text.textContent = m[1];
        el('span', 'fline-num', row, m[2]);
    }

    // ------------------------------------------------------------------ Helpers

    private me(b: BattleView): FighterView | undefined {
        return b.observer ? undefined : b.fighters.find(f => f.id === this.s.selfId);
    }
}

const LineIcons: Record<string, string> = {
    start: 'start', join: 'start', hit: 'bite', graze: 'bite', miss: 'miss', slash: 'sword', charge: 'fire', flame: 'fire', burn: 'fire',
    burnt: 'fire', roll: 'roll', down: 'down', death: 'down', rise: 'rise', struggle: 'rise', tend: 'tend', wait: 'wait', timeout: 'wait',
    flee: 'flee', truce: 'truce', over: 'truce', yield: 'yield', refuse: 'no', hold: 'sword', drop: 'sword', pickup: 'pickup', break: 'no',
    guard: 'guard', shove: 'shove', stow: 'sword',
};

