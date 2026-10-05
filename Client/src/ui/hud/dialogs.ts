// The sheets over the screen: character, belongings, trade, settings, leaving, and a closer look at someone. HTML
// dialogs (they were drawn on the canvas); built again only when what they show changes.
import {css} from '../color.ts';
import {speakingColor} from '../theme.ts';
import {drawPortrait, type Portraits} from '../portrait.ts';
import {arr, bool, clamp, countText, envNumber, isObject, num, obj, str, wholeCount, type Json} from '../../game/json.ts';
import {postureLabel, restLabel, readLoad, loadLabel, loadCost, weightLabel} from '../../game/labels.ts';
import type {GameState} from '../../game/state.ts';
import {button, el, setClass} from './dom.ts';
import {noRect} from './story.ts';
import {artCache} from '../artwork.ts';

/** What a paper doll shows: names by slot and by fur spot, the portrait, and whether it can be changed (one's own). */
interface Doll {
    worn: Record<string, string>;
    starterSide: string;
    starterName: string;
    jewellery: {spot: string; item: string; name: string}[];
    mouth: string;
    appearance: Json | null;
    age: number;
    artwork: string;
    editable: boolean;
    fight: GameState['battle'];
    self: Json | null;
}

export class Dialogs {
    private s: GameState;
    private portraits: Portraits;
    private overlay: HTMLElement;
    private panel: HTMLElement;
    private key = '';
    private aliasInput: HTMLInputElement;
    private noteInput: HTMLInputElement;
    private chosen = '';                        // The belonging picked out in the status screen,
    private spot = '';                          // and the fur spot.

    constructor(parent: HTMLElement, state: GameState, portraits: Portraits) {
        this.s = state;
        this.noteInput = document.createElement('input');
        this.noteInput.className = 'name-input';
        this.noteInput.maxLength = 500;
        this.noteInput.placeholder = 'Only you see this';
        this.aliasInput = document.createElement('input');
        this.aliasInput.className = 'name-input';
        this.aliasInput.addEventListener('keydown', e => {
            if (e.key !== 'Enter') return;
            this.act('name_add', this.aliasInput.value);
            this.aliasInput.value = '';
        });
        this.portraits = portraits;
        this.overlay = el('div', 'overlay', parent);
        this.panel = el('div', 'sheet', this.overlay);
        this.overlay.addEventListener('mousedown', e => {
            if (e.target === this.overlay) state.activate({rect: noRect, action: 'close', target: ''});
        });
        this.overlay.style.display = 'none';
        portraits.onReady = () => (this.key = '');
    }

    update() {
        const s = this.s, m = s.modal;
        this.overlay.style.display = m ? '' : 'none';
        if (!m) {
            this.key = '';
            return;
        }
        // What the open sheet shows; it is built again only when this changes (a trade's stock, a new colour...).
        const self = obj(s.snapshot, 'self');
        const art = m === 'inspect' || m === 'their_equipment' ? str(s.inspectedCharacter, 'artwork') : m === 'character' ? str(self, 'artwork')
            : m === 'status' ? this.ownArtwork(self) : '';
        const key = JSON.stringify([m, art, !!artCache.get(art), m === 'inspect' ? s.inspectedText : '', m === 'their_equipment' ? [s.inspectedCharacter, this.spot] : '', m === 'character' ? [self, s.reputation] : '',
            m === 'missions' ? s.missionBoard : '',
            m === 'chapter_window' ? [obj(self, 'chapter'), [...s.entities.values()].filter(e => e.kind !== 'npc').map(e => [e.id, e.name])] : '',
            m === 'inventory' || m === 'trade' || m === 'status' ? [arr(s.snapshot, 'inventory'), obj(s.snapshot, 'merchant'), countText(self, 'cash'),
                obj(s.snapshot, 'resource')] : '',
            m === 'status' ? [self, s.battle && !s.battle.observer ? [s.battle.fighters.find(f => f.id === s.selfId), s.battle.acted, s.battle.turn] : null] : '',
            m === 'settings' ? [s.selectedColor, s.revealSpeed, s.reducedMotion, s.flatWorld, s.plainGlyphs, s.perfOverlay, s.storyWidth,
                bool(s.snapshot, 'devTools'), s.environment.phase, s.hoverTooltips, s.soundVolume] : '']);
        if (key === this.key) return;
        this.key = key;
        const typing = document.activeElement === this.aliasInput;      // (Kept, and kept focused, as the sheet is rebuilt.)
        this.panel.replaceChildren();
        setClass(this.panel, 'wide', m === 'status');
        const close = button('×', 'close', this.panel, () => this.act('close'));
        close.title = 'Close (Esc)';
        if (m === 'character') this.character(self);
        else if (m === 'chapter_window') this.chapter(self);
        else if (m === 'missions') this.missions();
        else if (m === 'inventory') this.inventory(self);
        else if (m === 'status') this.status(self);
        else if (m === 'their_equipment') this.theirEquipment();
        else if (m === 'trade') this.trade(self);
        else if (m === 'settings') this.settings();
        else if (m === 'leave_character') this.leave();
        else this.inspect();
        if (typing && this.aliasInput.isConnected) this.aliasInput.focus();
    }

    private act(action: string, target = '') {
        this.s.activate({rect: noRect, action, target});
    }

    private heading(kicker: string, title: string) {
        el('div', 'label gold', this.panel, kicker);
        el('h1', '', this.panel, title);
    }

    private portrait(parent: HTMLElement, appearance: Json | null, age: number, artwork = '') {
        const canvas = el('canvas', 'portrait', parent);
        canvas.width = 500;
        canvas.height = 340;
        const c = canvas.getContext('2d');
        if (c) drawPortrait(c, this.portraits, appearance, age, 0, 0, canvas.width, canvas.height, artwork || undefined);
        return canvas;
    }

    private character(self: Json | null) {
        this.heading('CHARACTER / APPEARANCE', str(self, 'name', 'Your character'));
        el('div', 'label muted', this.panel, `AGE ${wholeCount(self, 'age', 18, 10000)}  ·  A STORY STILL UNFOLDING`);
        const dex = envNumber(self, 'dexterity', 0, 100, 50);
        el('div', 'label sage', this.panel, `STRENGTH ${envNumber(self, 'strength', 0, 100, 50).toFixed(0)}   DEXTERITY ${dex.toFixed(0)} ` +
            `(${envNumber(self, 'effectiveDexterity', 0, 100, dex).toFixed(1)} effective)   WISDOM ${envNumber(self, 'wisdom', 0, 100, 50).toFixed(0)}`);
        const cols = el('div', 'sheet-cols', this.panel);
        const left = el('div', 'sheet-col', cols);
        this.portrait(left, this.s.portraitAppearance(), this.s.portraitAge(), str(self, 'artwork'));
        if (str(self, 'artworkStatus') === 'pending')
            el('div', 'label gold', left, 'YOUR PORTRAIT IS WAITING FOR A DUNGEON MASTER · ONLY YOU SEE IT');
        el('div', 'label muted', left, num(self, 'shoulderHeightCm') > 0
            ? `${str(obj(self, 'appearance'), 'stature', 'average').toUpperCase()} STATURE · ${num(self, 'shoulderHeightCm').toFixed(0)} CM AT SHOULDER`
            : 'YOUR SAVED APPEARANCE');
        const right = el('div', 'sheet-col', cols);
        el('div', 'label gold', right, 'PRESENT STATE');
        el('div', 'big', right, postureLabel(self));
        el('p', 'muted', right, str(self, 'state', 'Set your current state with /me.'));
        el('p', 'sage small', right, '/lay then move to sneak. /stand to walk normally.');
        el('div', 'label gold', right, 'ROLEPLAY PROGRESSION');
        el('div', 'big', right, `Level ${Math.trunc(num(self, 'socialLevel', 1))} · ${str(obj(self, 'social'), 'title', 'Stranger')}`);
        el('div', 'sage', right, `${Math.trunc(num(self, 'socialXp'))} social experience`);
        const bar = el('div', 'bar', right);
        el('div', 'fill', bar).style.width = `${clamp(num(self, 'socialXp') / 100, 0, 1) * 100}%`;
        const skills = el('div', 'skills', right);
        el('span', 'sage', skills, `Sneak ${clamp(Math.trunc(num(self, 'sneakSkill')), 0, 100)} / 100`);
        el('span', 'sage', skills, `Hearing ${clamp(Math.trunc(num(self, 'hearingSkill')), 0, 100)} / 100`);
        el('span', 'scent', skills, `Scent ${clamp(Math.trunc(num(self, 'scentSkill')), 0, 100)} / 100`);
        el('span', 'muted', skills, `Nose ${Math.round(clamp(num(self, 'noseHealth', 1), 0, 1) * 100)}%`);
        this.injuries(right, self);
        this.names(right, self);
        this.stories(right, self);
        this.reputation(right);
        el('div', 'label gold', this.panel, 'DESCRIPTION');
        el('p', '', this.panel, str(self, 'description', 'Your appearance belongs here.'));
        const actions = el('div', 'sheet-actions', this.panel);
        button('CHARACTER SELECTION', 'primary', actions, () => this.act('leave_character'));
    }

    /** Injuries that outlast a fight (doc 38): acute ones with how bad and how much rest is left, lasting ones with when
     * and how they were got. Each says on hover what it does. */
    private injuries(parent: HTMLElement, self: Json | null) {
        const list = arr(self, 'injuries').filter(isObject);
        el('div', 'label gold', parent, 'INJURIES');
        if (!list.length) {
            el('p', 'muted small', parent, 'None. Fights can leave them; rest heals the ones that heal.');
            return;
        }
        const box = el('div', 'injury-list', parent);
        for (const kind of ['acute', 'lasting']) {
            for (const i of list.filter(x => str(x, 'kind') === kind)) {
                const row = el('div', `injury ${kind}`, box);
                el('span', 'injury-line', row, str(i, 'line'));
                el('span', 'muted small', row, str(i, 'does'));
                row.title = kind === 'acute' ? 'Heals with rest: fastest lying in a bed, slower resting anywhere else, a little while up and about. ' +
                    'Fighting on it sets the healing back.' : 'A mark for life. Others may notice it when they look at you closely.';
            }
        }
    }

    /** Your names (doc 32): the true one, and up to three others you go by. Nobody knows any until you say it. */
    private names(parent: HTMLElement, self: Json | null) {
        const names = obj(self, 'names');
        if (!names || !bool(names, 'hidden')) return;
        el('div', 'label gold', parent, 'YOUR NAMES');
        el('div', 'big', parent, str(names, 'name'));
        const aliases = arr(names, 'aliases').filter((a): a is string => typeof a === 'string');
        const list = el('div', 'name-list', parent);
        for (const alias of aliases) {
            const chip = el('span', 'chip', list, alias);
            button('×', 'chip-x', chip, () => this.act('name_retire', alias)).title =
                `Stop going by ${alias} (those who know you by it still will)`;
        }
        if (aliases.length < 3) {
            const row = el('div', 'name-add', parent);
            row.append(this.aliasInput);
            this.aliasInput.placeholder = 'Another name you go by';
            this.aliasInput.maxLength = 24;
            button('ADD', 'small', row, () => {
                this.act('name_add', this.aliasInput.value);
                this.aliasInput.value = '';
            });
        }
        el('p', 'muted small', parent, 'Wolves know you only by a name you tell them: say "I\'m …" aloud, or Introduce from a wolf\'s menu.');
    }

    /** The Chapter window (doc 32, Part 3): founding one, or the Chapter one is in. */
    private chapter(self: Json | null) {
        const s = this.s, ch = obj(self, 'chapter');
        const send = (fields: Json) => s.sendChapter(fields);
        if (!ch || !str(ch, 'id')) {
            this.heading('CHAPTERS', 'No Chapter yet');
            const proposal = obj(ch, 'proposal');
            if (proposal) {
                el('div', 'label gold', this.panel, `FOUNDING "${str(proposal, 'name')}"`);
                for (const f of arr(proposal, 'founders').filter(isObject))
                    el('div', '', this.panel, `${str(f, 'name')} · ${bool(f, 'agreed') ? 'agreed' : 'waiting'}`);
                const row = el('div', 'sheet-actions', this.panel);
                if (!bool(proposal, 'agreed')) button('AGREE', 'primary', row, () => send({verb: 'agree'}));
                button('CALL IT OFF', 'secondary', row, () => send({verb: 'withdraw'}));
                return;
            }
            if (str(ch, 'invite')) {
                el('p', 'gold', this.panel, `You are invited to join "${str(ch, 'invite')}".`);
                const row = el('div', 'sheet-actions', this.panel);
                button('ACCEPT', 'primary', row, () => send({verb: 'accept'}));
                button('DECLINE', 'secondary', row, () => send({verb: 'decline'}));
                return;
            }
            // Founding: three together, in a scene, each at the social level it takes; two marks from the proposer.
            el('p', 'muted', this.panel, 'A Chapter is founded by three wolves together, in a scene they are all taking part in. ' +
                'Name it, choose its colour, write its charter, and choose the two who found it with you. It costs two marks.');
            const name = el('input', 'name-input', this.panel);
            name.placeholder = 'The Chapter\'s name';
            name.maxLength = 32;
            const charter = el('textarea', 'name-input', this.panel);
            charter.placeholder = 'Its charter: what it stands for (optional)';
            charter.maxLength = 600;
            el('div', 'label gold', this.panel, 'COLOUR');
            const colours = ['#5b8bd9', '#4fb0a5', '#8bbf5a', '#d9b67b', '#b58ad9', '#d98bc4', '#e0e0d0', '#7fa0b0'];
            let colour = colours[0];
            const swatches = el('div', 'name-list', this.panel);
            for (const c of colours) {
                const b = button('', 'swatch', swatches, () => {
                    colour = c;
                    for (const other of swatches.children) (other as HTMLElement).classList.remove('active');
                    b.classList.add('active');
                });
                b.style.background = c;
                if (c === colour) b.classList.add('active');
            }
            el('div', 'label gold', this.panel, 'FOUNDERS WITH YOU (TWO)');
            const chosen = new Set<string>();
            const people = el('div', 'name-list', this.panel);
            for (const e of s.entities.values()) {
                if (e.self || e.kind === 'npc') continue;
                const b = button(e.name, 'small', people, () => {
                    if (chosen.has(e.id)) chosen.delete(e.id);
                    else chosen.add(e.id);
                    b.classList.toggle('active', chosen.has(e.id));
                });
            }
            const row = el('div', 'sheet-actions', this.panel);
            button('PROPOSE THE CHAPTER', 'primary', row, () =>
                send({verb: 'propose', name: name.value.trim(), charter: charter.value.trim(), colour, founders: [...chosen]}));
            return;
        }
        const rank = num(ch, 'rank'), ranks = arr(ch, 'rankNames').filter((r): r is string => typeof r === 'string');
        this.heading(`CHAPTER · ${str(ch, 'levelName').toUpperCase()} (LEVEL ${num(ch, 'level')})`, str(ch, 'name'));
        if (str(ch, 'charter')) el('p', 'muted', this.panel, str(ch, 'charter'));
        // Its Hold, its House, its treaties, the levies on it, and those sworn to it (doc 32, Phase 9).
        const hold = obj(ch, 'hold');
        if (str(hold, 'house')) el('div', 'label gold', this.panel, `RECOGNISED AS ${str(hold, 'house').toUpperCase()}`);
        if (str(hold, 'claims')) {
            const row = el('div', 'story-row', this.panel);
            el('span', 'small', row, `The Chapter's Hold claims ${str(hold, 'claims')} · toll ${num(hold, 'toll')}p for others coming in.`);
            if (rank === 0)
                button('SET TOLL', 'small', row, () => {
                    const toll = window.prompt('Toll in pennies (0 to 5)', String(num(hold, 'toll')));
                    if (toll !== null && /^[0-5]$/.test(toll.trim())) send({verb: 'toll', amount: +toll.trim()});
                });
        }
        const cols = el('div', 'sheet-cols', this.panel);
        const left = el('div', 'sheet-col', cols), right = el('div', 'sheet-col', cols);
        el('div', 'label gold', left, `RENOWN ${num(ch, 'renown')}`);
        const next = obj(ch, 'next');
        if (next)
            el('div', 'small', left, `To ${str(next, 'name')}: ${num(next, 'renown')} renown · ${num(next, 'haveActive')}/${num(next, 'active')} active · ` +
                `${num(next, 'haveStories')}/${num(next, 'stories')} Chapter Stories${str(next, 'ground') ? ` · ${str(next, 'ground')}` : ''}`);
        for (const r of arr(ch, 'renownLog').filter(isObject)) el('div', 'muted small', left, `+${num(r, 'amount')} ${str(r, 'kind')}`);
        el('div', 'label gold', left, 'MEMBERS');
        for (const m of arr(ch, 'members').filter(isObject)) {
            const row = el('div', 'story-row', left);
            el('span', bool(m, 'online') ? '' : 'muted', row, `${str(m, 'name')} · ${ranks[num(m, 'rank')] ?? ''}${bool(m, 'active') ? '' : ' · away'}`);
            const id = str(m, 'id');
            if (id === s.selfId) continue;
            if (rank === 0) {
                if (num(m, 'rank') > 1) button('RAISE', 'small', row, () => send({verb: 'rank', target: id, rank: num(m, 'rank') - 1}));
                if (num(m, 'rank') < 3 && num(m, 'rank') > 0) button('LOWER', 'small', row, () => send({verb: 'rank', target: id, rank: num(m, 'rank') + 1}));
                if (num(m, 'rank') === 1) button('MAKE HEAD', 'small', row, () => send({verb: 'rank', target: id, rank: 0}));
            }
            if (rank === 0 || (rank === 1 && num(m, 'rank') === 3)) button('SEND AWAY', 'small', row, () => send({verb: 'remove', target: id}));
        }
        // How the factions regard the Chapter: bands only, never numbers or whose deeds (doc 32, 4.2b).
        const standings = arr(ch, 'standings').filter(isObject);
        if (standings.length) {
            el('div', 'label gold', left, 'STANDING WITH THE FACTIONS');
            for (const f of standings) {
                el('div', 'small', left, `${str(f, 'name')} · ${str(f, 'band')}${str(f, 'stance') === 'war' ? ' · AT WAR' : ''}`);
                if (str(f, 'weighs')) el('div', 'muted small', left, str(f, 'weighs'));
            }
        }
        const leases = arr(ch, 'leases').filter(isObject);
        if (leases.length) {
            el('div', 'label gold', right, 'RENTED');
            for (const l of leases)
                el('div', 'small', right, `${str(l, 'name')} · ${num(l, 'rent')}p a week · ${num(l, 'daysPaid')} days paid · held ` +
                    `${num(l, 'heldDays')} days${str(l, 'state') === 'grace' ? ' · OVERDUE' : ''}`);
        }
        // Its own ground (doc 32, 5.3): sites, and making camp where the Officer stands.
        el('div', 'label gold', right, 'GROUND');
        const sites = arr(ch, 'sites').filter(isObject);
        if (!sites.length) el('div', 'muted small', right, num(ch, 'level') >= 3 ? 'No camp yet.' : 'A Company (level III) may make camp.');
        for (const site of sites) el('div', 'small', right, `${str(site, 'name')} · ${str(site, 'place')} · ${num(site, 'built')} built`);
        if (rank <= 1 && num(ch, 'level') >= 3)
            button('MAKE CAMP HERE', 'small', right, () => {
                const name = window.prompt('A name for the camp', `${str(ch, 'name')}'s camp`);
                if (name?.trim()) send({verb: 'camp', name: name.trim()});
            });
        const treaties = arr(hold, 'treaties').filter(isObject), levies = arr(hold, 'levies').filter(isObject);
        if (treaties.length || levies.length) {
            el('div', 'label gold', right, 'TREATIES AND LEVIES');
            for (const t of treaties)
                el('div', 'small', right, `${str(t, 'faction')} · ${str(t, 'state') === 'pending' ? 'awaiting its word' : `${num(t, 'weeksLeft')} weeks left`}` +
                    ` · ${bool(t, 'build') ? 'may build · ' : ''}${num(t, 'tithe')}p a week${bool(t, 'levy') ? ' · answers levies' : ''}` +
                    `${bool(t, 'labour') ? ' · its people free to join the Hold' : ''}`);
            for (const l of levies)
                el('div', 'small', right, `LEVY: ${str(l, 'faction')} wants watch kept at ${str(l, 'place')} · ${num(l, 'minutes')} minutes still owed`);
        }
        const sworn = arr(hold, 'sworn').filter((n): n is string => typeof n === 'string');
        if (sworn.length) el('div', 'small', right, `Sworn for life: ${sworn.join(', ')}`);
        // The Hold's folk (doc 32, 5.5): beds and posts from what is built, who works there, and who would come.
        const room = obj(hold, 'room');
        if (room) {
            el('div', 'label gold', right, 'THE HOLD\'S FOLK');
            el('div', 'small', right, `${num(room, 'housed')} of ${num(room, 'beds')} beds taken · ${num(room, 'posts')} posts free`);
            for (const w of arr(room, 'working').filter(isObject))
                el('div', 'muted small', right, `${str(w, 'name')} · ${str(w, 'role')}${bool(w, 'arriving') ? ' · on the way' : ''}`);
            for (const o of arr(room, 'offers').filter(isObject)) {
                const row = el('div', 'story-row', right);
                el('span', 'small', row, `${str(o, 'name')} would come as ${str(o, 'role')} · answer within ${num(o, 'days')} days`);
                if (rank <= 1) {
                    button('WELCOME', 'small', row, () => send({verb: 'welcome', target: str(o, 'id')}));
                    button('TURN AWAY', 'small', row, () => send({verb: 'turnaway', target: str(o, 'id')}));
                }
            }
        }
        el('div', 'label gold', right, 'MEETING PLACE');
        const meeting = obj(ch, 'meeting');
        el('div', '', right, meeting ? str(meeting, 'name') : 'None declared');
        if (rank <= 1) button('MEET HERE', 'small', right, () => send({verb: 'meet'}));
        el('div', 'label gold', right, `TREASURY · ${num(ch, 'treasury')} PENNIES`);
        const amount = el('input', 'name-input', right);
        amount.type = 'number';
        amount.min = '1';
        amount.placeholder = 'Pennies';
        const money = el('div', 'name-list', right);
        button('DEPOSIT', 'small', money, () => send({verb: 'deposit', amount: Math.trunc(+amount.value || 0)}));
        if (rank <= 1) button('DRAW', 'small', money, () => send({verb: 'withdraw_money', amount: Math.trunc(+amount.value || 0)}));
        el('div', 'label gold', right, 'HOSTILE TO THE CHAPTER');
        const hostiles = arr(ch, 'hostiles').filter(isObject);
        if (!hostiles.length) el('div', 'muted small', right, 'No one. Officers mark wolves from their menu.');
        for (const h of hostiles) {
            const row = el('div', 'story-row', right);
            el('span', 'hostile-name', row, `${str(h, 'name')}${str(h, 'reason') ? ` · ${str(h, 'reason')}` : ''}`);
            if (rank <= 1) button('UNMARK', 'small', row, () => send({verb: 'unhostile', target: str(h, 'target')}));
        }
        if (rank === 0 && num(ch, 'level') >= 2) {
            el('div', 'label gold', right, 'RANK NAMES');
            ranks.forEach((r, i) => {
                const b = button(r, 'small', right, () => {
                    const name = window.prompt(`A new name for the rank "${r}"`, r);
                    if (name?.trim()) send({verb: 'rankname', rank: i, name: name.trim()});
                });
                b.title = 'Rename this rank';
            });
        }
        const actions = el('div', 'sheet-actions', this.panel);
        button('LEAVE THE CHAPTER', 'secondary', actions, () => {
            if (window.confirm('Leave the Chapter?')) send({verb: 'leave'});
        });
    }

    /** A faction's mission board (doc 32, 4.5). */
    private missions() {
        const board = this.s.missionBoard;
        this.heading('MISSIONS', str(board, 'faction', 'A faction'));
        const list = arr(board, 'missions').filter(isObject);
        if (!list.length) el('p', 'muted', this.panel, 'Nothing today.');
        for (const m of list) {
            const row = el('div', 'story-row', this.panel);
            el('span', bool(m, 'allowed') ? '' : 'muted', row, str(m, 'text'));
            if (str(m, 'state') === 'taken') el('span', 'label sage', row, 'TAKEN');
            else if (bool(m, 'allowed')) button('TAKE IT ON', 'small', row, () => {
                this.s.sendFaction({verb: 'take', mission: str(m, 'id')});
                this.act('close');
            });
            else el('span', 'muted small', row, num(m, 'tier') === 1 ? 'for a Lodge they know well' : 'for a Company they trust');
        }
        el('p', 'muted small', this.panel, 'Deliveries and letters are handed over from the recipient\'s menu; a watch is kept by staying there.');
    }

    /** Their Stories (doc 32, 1.2): agree to one, tell one, give a Story Star. */
    private stories(parent: HTMLElement, self: Json | null) {
        const stories = arr(obj(self, 'social'), 'stories').filter(isObject);
        if (!stories.length) return;
        el('div', 'label gold', parent, 'STORIES');
        for (const st of stories) {
            const row = el('div', 'story-row', parent);
            const id = str(st, 'id'), state = str(st, 'state');
            el('span', '', row, `"${str(st, 'name')}" · ${state} · ${num(st, 'scenes')} scene${num(st, 'scenes') === 1 ? '' : 's'}` +
                (bool(st, 'chapter') ? ' · a Chapter Story' : ''));
            if (state === 'pending' && !bool(st, 'approved'))
                button('AGREE', 'small', row, () => this.s.sendSocial({verb: 'approve', story: id}));
            if (state === 'active' && bool(st, 'mine') && num(st, 'scenes') >= 2)
                button('TELL IT', 'small', row, () => this.s.sendSocial({verb: 'close', story: id})).title = 'Close the Story and be paid for it';
            for (const t of arr(st, 'starTargets').filter(isObject))
                button(`★ ${str(t, 'name')}`, 'small', row, () => this.s.sendSocial({verb: 'storystar', story: id, target: str(t, 'id')}));
        }
    }

    /** Their name about town (doc 32, 1.4): what residents who know them think, asked for, never a score. */
    private reputation(parent: HTMLElement) {
        el('div', 'label gold', parent, 'YOUR NAME ABOUT TOWN');
        for (const line of this.s.reputation) el('div', 'small', parent, line);
        button(this.s.reputation.length ? 'ASK AGAIN' : 'HEAR WHAT IS SAID', 'small', parent, () => this.s.sendSocial({verb: 'reputation'}));
    }

    private inventory(self: Json | null) {
        this.heading('BELONGINGS / EQUIPMENT', 'What you carry');
        this.belongings(self);
    }

    /** One's status (doc 33), laid out as an equipment screen: vitals, attributes, senses and skills on the left; the
     * wolf and what it wears (doc 35's slots) in the middle; what is wrong and where one stands on the right; and
     * belongings below, a click on one showing it and what can be done with it. Opened from one's own card in a fight,
     * or one's health and stamina out of one. */
    private status(self: Json | null) {
        const s = this.s, b = s.battle && !s.battle.observer ? s.battle : null;
        const me = b?.fighters.find(f => f.id === s.selfId);
        this.heading(b ? 'STATUS / EQUIPMENT · IN A FIGHT' : 'STATUS / EQUIPMENT', str(self, 'name', 'You'));
        el('div', 'label muted', this.panel, `AGE ${wholeCount(self, 'age', 18, 10000)}  ·  LEVEL ${Math.trunc(num(self, 'socialLevel', 1))} ` +
            `${str(obj(self, 'social'), 'title', 'Stranger').toUpperCase()}  ·  ${postureLabel(self).toUpperCase()}`);
        const screen = el('div', 'rpg', this.panel);
        const left = el('div', 'rpg-col', screen), doll = el('div', 'rpg-doll', screen), right = el('div', 'rpg-col', screen);

        // Vitals: the bars, what drives each on hover.
        const vitals = this.box(left, 'VITALS');
        const meter = (name: string, value: number, max: number, cls: string, why: string) => {
            const row = el('div', 'rpg-meter', vitals);
            row.title = why;
            const head = el('div', 'rpg-meter-head', row);
            el('span', 'label', head, name);
            el('span', 'rpg-num', head, `${Math.round(value)} / ${max}`);
            el('div', 'fill', el('div', `bar ${cls}`, row)).style.width = `${clamp(value / Math.max(1, max), 0, 1) * 100}%`;
        };
        const health = num(self, 'health', 100);
        meter('HEALTH', health, 100, 'health', 'Out of a fight hurt heals, 50 an hour. Hurt shortens a move in a fight (by up to 60%) and slows you in ' +
            'the world. At 0 you go down; you get up after a while (longer for each time since your last full rest in a bed), ' +
            'sooner if someone tends you or you struggle up (once a day).');
        const stamina = me && me.stamina >= 0 ? me.stamina : num(self, 'stamina', 100);
        meter('STAMINA', stamina, 100, '', b && me ? `In a fight it comes back only at the start of each of your turns: ${me.regen} next ` +
            `(4 + STR ÷ 10, less hurt; twice after a turn of rest). A bite costs 8, a sword 14, running faster than a trot ` +
            `${b.tileStamina > 0 ? `${b.tileStamina.toFixed(1)} a tile at your pace` : 'by the tile'}. At 0 you are winded: walking only, no biting.`
            : 'Out of a fight it comes back 5 a second and drains when you run fast (the pace, on the wheel). At 0 you are exhausted ' +
              'and can only walk until it is back to 20.');
        if (num(self, 'manaMax') > 0)
            meter('MANA', num(self, 'mana'), num(self, 'manaMax'), 'mana', 'Most mana is 20 + WIS × 0.8. In a fight +2 at the start of each ' +
                'turn, out of one slowly. Fire costs 25 (Quickened 40); with too little it burns you twice as much.');
        // Worn armour by hit zone (doc 33's "Armour, by hit zone"): a blow lands on one zone, and only the best piece
        // worn there counts; the catalog slot says which zone a piece guards.
        const zones: [string, string][] = [['head', 'HEAD'], ['throat', 'THROAT'], ['body', 'BODY'], ['paws', 'LEGS']];
        const worn = arr(s.snapshot, 'inventory').filter(isObject).filter(i => wholeCount(i, 'worn', 0) > 0 && num(i, 'protect') > 0);
        const guard = zones.map(([slot, name]) => [name, Math.max(0, ...worn.filter(i => str(i, 'slot') === slot).map(i => num(i, 'protect')))] as const);
        if (guard.some(([, v]) => v > 0)) el('div', 'label sage', vitals, `ARMOUR · ${guard.map(([n, v]) => `${n} ${v || '—'}`).join(' · ')}`).title =
            'What your armour takes off a blow where it lands: the best piece on that part of you, plus its extra against ' +
            'cuts or thrusts, less what the weapon pierces. Head on, a blow may land on the face, throat, shoulder or a ' +
            'foreleg; from the side mostly the flank; from behind the back, haunch or a hind leg. At least a quarter of a ' +
            'blow always gets through; fire, burning and bleeding go round armour.';
        el('div', 'muted small', vitals, 'Point at a bar to see what drives it.');

        // Attributes.
        const attributes = el('div', 'rpg-stats', this.box(left, 'ATTRIBUTES'));
        const stat = (name: string, value: string, note = '', why = '') => {
            const cell = el('div', 'rpg-stat', attributes);
            if (why) cell.title = why;
            el('div', 'label muted', cell, name);
            el('div', 'rpg-stat-value', cell, value);
            if (note) el('div', 'muted small', cell, note);
        };
        const dex = envNumber(self, 'dexterity', 0, 100, 50), effectiveDex = envNumber(self, 'effectiveDexterity', 0, 100, dex);
        stat('STRENGTH', envNumber(self, 'strength', 0, 100, 50).toFixed(0), '', 'Stamina back each fight turn, what you can carry, how hard you hit.');
        stat('DEXTERITY', dex.toFixed(0), Math.abs(effectiveDex - dex) >= 0.1 ? `${effectiveDex.toFixed(1)} now` : '',
            'Speed and footing. Age, hurt and load change what it is now.');
        stat('WISDOM', envNumber(self, 'wisdom', 0, 100, 50).toFixed(0), '', 'Mana, for the gifted.');
        stat('FIGHTING', clamp(Math.trunc(num(self, 'fightingSkill', 50)), 0, 100).toFixed(0), '', 'How well you land and turn aside blows.');

        // Senses and skills: each sense with the organ behind it, each skill out of 100.
        const senses = this.box(left, 'SENSES & SKILLS');
        const line = (name: string, value: number, note: string, cls = '') => {
            const row = el('div', 'rpg-line', senses);
            el('span', `label ${cls}`, row, name);
            el('div', 'fill', el('div', 'bar thin', row)).style.width = `${clamp(value, 0, 100)}%`;
            el('span', 'rpg-num', row, note);
        };
        const organ = (key: string) => clamp(num(self, key, 1), 0, 1);
        const pct = (key: string) => Math.round(clamp(num(self, key, 1), 0, 2) * 100);
        line('SIGHT', pct('vision'), `${pct('vision')}%${organ('eyeHealth') < 1 ? ` · eyes ${Math.round(organ('eyeHealth') * 100)}%` : ''}`);
        line('HEARING', pct('hearing'), `${pct('hearing')}%${organ('earHealth') < 1 ? ` · ears ${Math.round(organ('earHealth') * 100)}%` : ''}`);
        line('SCENT', pct('smell'), `${pct('smell')}%${organ('noseHealth') < 1 ? ` · nose ${Math.round(organ('noseHealth') * 100)}%` : ''}`, 'scent');
        for (const [name, key] of [['SNEAK', 'sneakSkill'], ['LISTENING', 'hearingSkill'], ['TRACKING', 'scentSkill']]) {
            const v = clamp(Math.trunc(num(self, key)), 0, 100);
            line(name, v, `${v} / 100`);
        }
        el('div', 'label sage', senses, `PACE ${Math.trunc(num(self, 'pace'))}/10 ${str(self, 'paceName').toUpperCase()}  ·  TOP ${num(self, 'topSpeed').toFixed(1)} t/s`);

        this.paperDoll(doll, this.ownDoll(self, b));

        // What is wrong: in a fight its injuries, named; out of one, what one's state says.
        const list = el('div', 'status-conditions', this.box(right, 'CONDITION'));
        const condition = (name: string, does: string) => {
            const row = el('div', 'status-condition', list);
            el('span', 'hurt', row, name);
            el('span', 'muted small', row, does);
        };
        if (me) for (const i of me.injuries) condition(i.name, i.does);
        else {
            const downedLeft = num(self, 'downedLeft');
            if (downedLeft > 0) condition('Down', `You get up in about ${Math.ceil(downedLeft / 60)} min, sooner if tended or you struggle up.`);
            if (bool(self, 'exhausted')) condition('Exhausted', 'Walking only until your stamina is back to 20.');
            if (health <= 25 && downedLeft <= 0) condition('Limping', 'Badly hurt: slow, and no sprinting.');
            else if (health < 75) condition(health <= 50 ? 'Badly hurt' : 'Wounded', 'Slower, and it shows. Rest and time heal it.');
        }
        const downs = num(self, 'downsSinceRest');
        if (downs > 0) condition(`Down ${downs}× without a full rest`, 'Each time you go down you stay down longer, until you sleep six hours in a bed.');
        const rest = restLabel(self).replace(/^\s*·\s*/, '');
        if (rest) condition('Resting', rest);
        const load = readLoad(self);
        if (load && load.state !== 'comfortable') condition(load.state === 'heavy' ? 'Heavy load' : 'Overloaded', loadCost(load));
        // Injuries that outlast a fight (doc 38): the acute ones, how bad and what they do; the lasting ones on the sheet.
        for (const i of arr(self, 'injuries').filter(isObject).filter(x => str(x, 'kind') === 'acute'))
            condition(str(i, 'name'), `${str(i, 'severity')} · ${str(i, 'does')} About ${Math.max(1, Math.round(num(i, 'daysLeft')))} ` +
                `${Math.round(num(i, 'daysLeft')) === 1 ? 'day' : 'days'} of rest to heal.`);
        if (!list.childElementCount) el('p', 'muted small', list, 'Nothing is wrong with you.');

        const gift = str(self, 'gift');
        if (gift) {
            const box = this.box(right, 'GIFT');
            el('div', 'big', box, gift.charAt(0).toUpperCase() + gift.slice(1));
            el('p', 'muted small', box, bool(self, 'quickened') ? 'Quickened: stronger, and dearer in mana.' : 'Drawn on with mana.');
        }

        const standing = this.box(right, 'STANDING');
        el('div', 'big', standing, `Level ${Math.trunc(num(self, 'socialLevel', 1))} · ${str(obj(self, 'social'), 'title', 'Stranger')}`);
        el('div', 'fill', el('div', 'bar', standing)).style.width = `${clamp(num(self, 'socialXp') / 100, 0, 1) * 100}%`;
        el('div', 'sage small', standing, `${Math.trunc(num(self, 'socialXp'))} social experience`);
        const chapter = obj(self, 'chapter');
        if (str(chapter, 'name')) el('div', 'muted small', standing, `Of ${str(chapter, 'name')}`);
        el('div', 'gold', standing, `PURSE · ${countText(self, 'cash')} silver pennies`);

        this.pack(self, b);
        button('CHARACTER SHEET', '', el('div', 'sheet-actions', this.panel), () => this.act('character'));
    }

    /** One's uploaded portrait: the one waiting for a Dungeon Master (only one's self sees it), or the one others see. */
    private ownArtwork(self: Json | null): string {
        return str(self, 'artwork') || this.s.entities.get(this.s.selfId)?.artwork || '';
    }

    private box(parent: HTMLElement, title: string): HTMLElement {
        const box = el('section', 'rpg-box', parent);
        el('div', 'label gold', box, title);
        return box;
    }

    /** One's own doll: what one wears, from one's own state; editable. */
    private ownDoll(self: Json | null, fight: GameState['battle']): Doll {
        const s = this.s;
        const inventory = arr(s.snapshot, 'inventory').filter(isObject);
        const nameOf = (id: string) => str(inventory.find(i => str(i, 'id') === id), 'name', id);
        const worn: Record<string, string> = {};
        for (const [slot, item] of Object.entries(obj(self, 'worn') ?? {})) if (typeof item === 'string') worn[slot] = nameOf(item);
        // The everyday satchel every wolf has sits on a free side of the chest.
        const starter = inventory.find(i => str(i, 'id') === 'starter_satchel');
        const starterSide = !starter ? '' : !worn.chest_left ? 'chest_left' : !worn.chest_right ? 'chest_right' : '';
        const mouth = str(self, 'mouth');
        return {
            worn, starterSide, starterName: str(starter, 'name'),
            jewellery: arr(self, 'jewellery').filter(p => Array.isArray(p) && p.length === 2)
                .map(p => ({spot: String((p as Json[])[0]), item: String((p as Json[])[1]), name: nameOf(String((p as Json[])[1]))})),
            mouth: mouth ? nameOf(mouth) : '',
            appearance: obj(self, 'appearance'), age: num(self, 'age', 18), artwork: this.ownArtwork(self),
            editable: true, fight, self,
        };
    }

    /** Another's doll, from what a closer look showed (the inspect event's equipment): to look at only. */
    private theirDoll(inspected: Json | null): Doll {
        const equipment = obj(inspected, 'equipment');
        const worn: Record<string, string> = {};
        for (const [slot, piece] of Object.entries(obj(equipment, 'worn') ?? {})) if (isObject(piece)) worn[slot] = str(piece, 'name');
        const stage = str(inspected, 'lifeStage', 'adult');
        return {
            worn, starterSide: '', starterName: '',
            jewellery: arr(equipment, 'jewellery').filter(isObject).map(p => ({spot: str(p, 'spot'), item: str(p, 'id'), name: str(p, 'name')})),
            mouth: str(obj(equipment, 'mouth'), 'name'),
            appearance: obj(inspected, 'appearance'), age: stage === 'young' ? 6 : stage === 'adolescent' ? 13 : stage === 'old' ? 65 : 18,
            artwork: str(inspected, 'artwork'), editable: false, fight: null, self: null,
        };
    }

    /** The wolf and what it wears (doc 35, 1.1): its portrait (as on its card: the uploaded picture where there is one,
     * else the wolf as it looks) between the wear slots, the muzzle beneath, and the fur spots where jewellery is
     * clipped. One's own: a click on something worn takes it off. Another's: to look at only. */
    private paperDoll(parent: HTMLElement, doll: Doll) {
        const sides = el('div', 'rpg-doll-grid', parent);
        const leftSlots = el('div', 'rpg-slots', sides);
        const figure = el('div', 'rpg-figure', sides);
        const canvas = el('canvas', 'portrait', figure);
        canvas.width = canvas.height = 420;
        const c = canvas.getContext('2d');
        if (c) drawPortrait(c, this.portraits, doll.appearance, doll.age, 0, 0, canvas.width, canvas.height, doll.artwork || undefined);
        const rightSlots = el('div', 'rpg-slots', sides);
        const slot = (parent: HTMLElement, name: string, holds: string, filled = '', onClick?: () => void, disabled = false, hint = '') => {
            const box = el(onClick ? 'button' : 'div', `rpg-slot${filled ? ' filled' : ''}`, parent) as HTMLElement;
            box.title = `${name}: ${holds}${hint ? `\n${hint}` : ''}`;
            el('div', 'label muted', box, name);
            el('div', filled ? 'rpg-slot-item' : 'rpg-slot-empty', box, filled || 'Empty');
            if (onClick) {
                box.addEventListener('click', onClick);
                (box as HTMLButtonElement).disabled = disabled;
            }
            return box;
        };
        const wear = (parent: HTMLElement, key: string, name: string, holds: string) => {
            const item = doll.worn[key] ?? '';
            if (!item && key === doll.starterSide) return slot(parent, name, holds, doll.starterName, undefined, false, 'The satchel every wolf has.');
            if (!doll.editable) return slot(parent, name, holds, item);
            return slot(parent, name, holds, item, item ? () => this.act('take off', key) : undefined, !!doll.fight,
                item ? (doll.fight ? 'Not in a fight.' : 'Click to take it off.') : 'Choose something below to wear it.');
        };
        wear(leftSlots, 'head', 'HEAD', 'a hat, hood or helm.');
        wear(leftSlots, 'neck', 'NECK', 'a scarf, neckerchief, leather wrap, gorget or neck guard.');
        wear(leftSlots, 'body', 'BODY', 'a vest, coat or barding.');
        wear(leftSlots, 'back', 'BACK', 'a shawl, cape or mantle, over everything.');
        wear(rightSlots, 'harness', 'HARNESS', 'the carrying frame around the chest, under the sides.');
        wear(rightSlots, 'chest_left', 'CHEST · LEFT', 'a satchel, sling bag, water skin or bandolier.');
        wear(rightSlots, 'chest_right', 'CHEST · RIGHT', 'a satchel, sling bag, water skin or bandolier.');
        wear(rightSlots, 'paws', 'PAWS', 'wraps, bindings, boots or claw caps: one set for all four.');
        // The muzzle: what is held, and (one's own) taking up or putting away the sword.
        const holds = 'for holding: a weapon, tool, lantern, basket or letter. Holding something stops Bite and muffles speech.';
        const row = el('div', 'rpg-doll-row', parent);
        const sword = doll.editable ? this.swordAction(doll.self, doll.fight) : null;
        if (!doll.editable) slot(row, 'MUZZLE', holds, doll.mouth);
        else slot(row, 'MUZZLE', holds, doll.mouth, sword?.run, sword?.disabled ?? false,
            sword ? `${sword.label}${sword.why ? ` (${sword.why})` : ''}` : 'You have nothing to hold.');
        this.furSpots(parent, doll);
    }

    /** Jewellery at its fur spots (doc 35): a wolf in outline with a marker at each spot, how many pieces are there, and
     * the chosen spot's pieces (one's own, to unclip). */
    private furSpots(parent: HTMLElement, doll: Doll) {
        const pieces = doll.jewellery;
        const box = this.box(parent, `JEWELLERY · ${pieces.length ? `${pieces.length} PIECE${pieces.length === 1 ? '' : 'S'}` : 'NONE'}`);
        const wrap = el('div', 'rpg-fur', box);
        const ns = 'http://www.w3.org/2000/svg';
        const svg = document.createElementNS(ns, 'svg');
        svg.setAttribute('viewBox', '0 0 300 150');
        svg.setAttribute('class', 'rpg-fur-svg');
        wrap.appendChild(svg);
        const outline = document.createElementNS(ns, 'path');
        // A wolf standing, facing right: tail, back, neck, head and ears, muzzle, chest, legs.
        outline.setAttribute('d', 'M38 62 C22 70 14 92 20 104 C28 96 36 84 52 76 C70 66 96 58 140 58 C170 58 192 54 206 46 ' +
            'L214 26 L222 40 L230 24 L236 44 C248 48 262 56 274 62 L276 70 C262 72 248 72 238 74 C232 86 224 96 214 102 ' +
            'L214 140 L204 140 L202 106 C196 108 188 110 180 110 L178 140 L168 140 L168 108 C140 112 110 112 88 106 ' +
            'L84 140 L74 140 L72 100 C66 96 62 92 60 86 L56 140 L46 140 L48 84 C46 76 44 70 38 62 Z');
        outline.setAttribute('class', 'rpg-fur-wolf');
        svg.appendChild(outline);
        const at: Record<string, [number, number, string]> = {
            ears: [226, 34, 'EARS'], crown: [240, 50, 'CROWN'], ruff: [214, 70, 'RUFF'], chest: [222, 92, 'CHEST'], back: [130, 62, 'BACK'],
            foreleg_left: [208, 128, 'L FORE'], foreleg_right: [174, 128, 'R FORE'], hindleg_left: [80, 128, 'L HIND'],
            hindleg_right: [52, 128, 'R HIND'], tail: [26, 92, 'TAIL'],
        };
        if (!at[this.spot]) this.spot = '';
        for (const [spot, [x, y, label]] of Object.entries(at)) {
            const here = pieces.filter(p => p.spot === spot);
            const g = document.createElementNS(ns, 'g');
            g.setAttribute('class', `rpg-fur-spot${here.length ? ' has' : ''}${spot === this.spot ? ' chosen' : ''}`);
            const dot = document.createElementNS(ns, 'circle');
            dot.setAttribute('cx', String(x));
            dot.setAttribute('cy', String(y));
            dot.setAttribute('r', here.length ? '8' : '5');
            g.appendChild(dot);
            if (here.length) {
                const n = document.createElementNS(ns, 'text');
                n.setAttribute('x', String(x));
                n.setAttribute('y', String(y + 3.5));
                n.textContent = String(here.length);
                g.appendChild(n);
            }
            const title = document.createElementNS(ns, 'title');
            title.textContent = `${label}: ${here.length ? here.map(p => p.name).join(', ') : 'nothing'}`;
            g.appendChild(title);
            g.addEventListener('click', () => {
                this.spot = this.spot === spot ? '' : spot;
                this.key = '';
                this.update();
            });
            svg.appendChild(g);
        }
        const list = el('div', 'rpg-fur-list', wrap);
        if (!this.spot) {
            el('p', 'muted small', list, pieces.length ? 'Click a spot to see what is clipped there.'
                : doll.editable ? 'Nothing clipped to your fur. Jewellery you own can be clipped on from your belongings below.'
                    : 'Nothing clipped to their fur.');
            return;
        }
        el('div', 'label gold', list, at[this.spot][2]);
        const here = pieces.filter(p => p.spot === this.spot);
        if (!here.length) el('p', 'muted small', list, 'Nothing clipped here.');
        for (const p of here) {
            const row = el('div', 'rpg-fur-row', list);
            el('span', '', row, p.name);
            if (!doll.editable) continue;
            const off = button('UNCLIP', 'secondary', row, () => this.act('take off', `${p.spot}@${p.item}`));
            off.disabled = !!doll.fight;
        }
    }

    /** Another's equipment page (doc 35), from a closer look at them: what they wear and where, to look at only. */
    private theirEquipment() {
        const s = this.s, inspected = s.inspectedCharacter;
        this.heading('EQUIPMENT · A CLOSER LOOK', str(inspected, 'title', str(inspected, 'name', 'Someone')));
        el('div', 'label muted', this.panel, `${str(inspected, 'lifeStage', 'adult').toUpperCase()}  ·  ${str(inspected, 'posture', 'standing').toUpperCase()}` +
            '  ·  WHAT YOU CAN SEE OF WHAT THEY WEAR');
        const doll = el('div', 'rpg-doll rpg-doll-alone', el('div', 'rpg rpg-their', this.panel));
        this.paperDoll(doll, this.theirDoll(inspected));
        if (str(inspected, 'wearing')) el('p', 'muted', this.panel, str(inspected, 'wearing'));
        button('BACK TO THE CLOSER LOOK', 'secondary', el('div', 'sheet-actions', this.panel), () => this.act('back_to_inspect'));
    }

    /** Putting on a wearable (doc 35): at its slot, a side of the chest, or a fur spot it clips to; and taking it off. */
    private wearActions(actions: HTMLElement, item: Json, self: Json | null, fight: GameState['battle']) {
        const places = arr(item, 'places').map(p => String(p));
        if (!places.length) return;
        const id = str(item, 'id'), jewel = str(item, 'slot') === 'jewelry';
        const free = wholeCount(item, 'quantity', 1) > wholeCount(item, 'worn', 0, 999);
        const go = (label: string, run: () => void, enabled: boolean, why: string) => {
            const b = button(label, 'primary', actions, run);
            b.disabled = !enabled || !!fight;
            b.title = fight ? 'Not in a fight.' : why;
        };
        const placeLabel = (p: string) => p.replace('_left', ' · left').replace('_right', ' · right').replace('hindleg', 'hind leg')
            .replace('_', ' ').toUpperCase();
        if (jewel) {
            el('div', 'label muted', actions, 'CLIP TO');
            for (const p of places) go(placeLabel(p), () => this.act('wear', `${id}@${p}`), free, free ? '' : 'Every one you have is worn.');
        } else {
            const worn = obj(self, 'worn');
            for (const p of places) {
                const here = str(worn, p) === id;
                if (here) go(`TAKE OFF${places.length > 1 ? ` · ${placeLabel(p.replace('chest_', ''))}` : ''}`, () => this.act('take off', p), true, '');
                else go(places.length > 1 ? `WEAR · ${placeLabel(p.replace('chest_', ''))} SIDE` : `WEAR · ${placeLabel(p)}`,
                    () => this.act('wear', `${id}@${p}`), free, free ? (str(worn, p) ? 'Swaps it for what is there' : '') : 'Every one you have is worn.');
            }
        }
    }

    /** Taking up or putting away the sword: out of a fight at once, in one as the turn's action (doc 33). Null without one. */
    private swordAction(self: Json | null, fight: GameState['battle']) {
        const s = this.s;
        if (s.inventoryQuantity('sword') <= 0) return null;
        const held = str(self, 'mouth') === 'sword';
        const label = held ? 'PUT THE SWORD AWAY' : 'HOLD THE SWORD IN YOUR JAWS';
        if (!fight) return {label, why: '', disabled: false, run: () => this.act(held ? 'stow sword' : 'hold sword')};
        const mine = fight.turn === s.selfId;
        return {label: `${label} · YOUR ACTION`, why: !mine ? 'On your turn' : fight.acted ? 'You have acted this turn' : 'Uses this turn\'s action',
            disabled: !mine || fight.acted, run: () => s.sendBattle(held ? 'stow' : 'hold')};
    }

    /** Belongings as a grid of tiles; the chosen one shown beside, with what can be done with it. */
    private pack(self: Json | null, fight: GameState['battle']) {
        const s = this.s;
        const items = arr(s.snapshot, 'inventory').filter(isObject);
        const section = this.box(this.panel, 'BELONGINGS');
        this.loadMeter(section, self);
        const body = el('div', 'rpg-pack', section);
        const grid = el('div', 'rpg-items', body);
        if (!items.length) el('p', 'muted', grid, 'Your pack is empty. Objects you acquire will appear here.');
        if (!items.some(i => str(i, 'id') === this.chosen)) this.chosen = '';
        for (const item of items) {
            const id = str(item, 'id');
            const tile = el('button', `rpg-item${id === this.chosen ? ' chosen' : ''}${bool(item, 'equipped') ? ' equipped' : ''}`, grid);
            tile.title = str(item, 'name');
            el('div', `item-icon ${itemIcon(item)}`, tile);
            el('div', 'rpg-item-name', tile, str(item, 'name'));
            const quantity = wholeCount(item, 'quantity', 1);
            if (quantity > 1) el('span', 'rpg-count', tile, `×${quantity}`);
            if (bool(item, 'equipped')) el('span', 'rpg-worn', tile, 'E');
            tile.addEventListener('click', () => {
                this.chosen = this.chosen === id ? '' : id;
                this.key = '';
                this.update();
            });
        }
        const detail = el('div', 'rpg-detail', body);
        const item = items.find(i => str(i, 'id') === this.chosen);
        if (!item) el('p', 'muted small', detail, 'Choose something to look at it.');
        else {
            el('div', 'item-name', detail, str(item, 'name'));
            el('div', `label ${bool(item, 'equipped') ? 'sage' : 'muted'}`, detail,
                `${str(item, 'id').split('~')[0] === 'sword' && bool(item, 'equipped') ? 'IN YOUR JAWS' : bool(item, 'equipped') ? 'WORN' : 'CARRIED'} · × ${wholeCount(item, 'quantity', 1)}` +
                `${item && 'condition' in item ? ` · CONDITION ${num(item, 'condition')}%` : ''}` +
                `${num(item, 'warmth') > 0 ? ` · WARMTH ${num(item, 'warmth')}` : ''}${num(item, 'protect') > 0 ? ` · PROTECTION ${num(item, 'protect')}` : ''}` +
                `${num(item, 'status') > 0 ? ` · FINERY ${num(item, 'status')}` : ''}` +
                `${num(item, 'weight') > 0 ? ` · ${weightLabel(num(item, 'weight')).toUpperCase()}${wholeCount(item, 'quantity', 1) > 1 ? ' EACH' : ''}` : ''}`);
            el('p', 'muted small', detail, str(item, 'description'));
            const actions = el('div', 'rpg-detail-actions', detail);
            const id = str(item, 'id');
            if (id === 'meal') {
                const eat = button('EAT ONE', 'primary', actions, () => this.act('eat'));
                eat.disabled = !!fight;
                if (fight) eat.title = 'Not in a fight';
            }
            // Masking oil (doc 35): one's scent, and that of what one carries, hidden for a few hours.
            if (id.split('~')[0] === 'masking_oil') {
                const masked = num(obj(this.s.snapshot, 'self'), 'scentMasked');
                const use = button(masked > 0 ? 'USE · MORE' : 'USE', 'primary', actions, () => this.s.send({type: 'mask'}));
                use.title = masked > 0 ? `Masked for about ${Math.ceil(masked / 600)} more hours` : 'Hide your scent for a few hours';
            }
            this.wearActions(actions, item, self, fight);
            const sword = id.split('~')[0] === 'sword' ? this.swordAction(self, fight) : null;
            if (sword) {
                const go = button(sword.label, 'primary', actions, sword.run);
                go.disabled = sword.disabled;
                go.title = sword.why;
            }
            // Worn gear mended at a shop that deals in it (doc 35): the fee to the shop.
            if (str(item, 'repairBy')) {
                const mend = button(`REPAIR · ${num(item, 'repairCost')}p`, 'small', actions,
                    () => this.s.send({type: 'repair', target: str(item, 'repairBy'), item: id}));
                mend.disabled = !!fight;
                mend.title = fight ? 'Not in a fight' : 'Have the shop beside you mend it';
            }
        }
        if (fight) return;                          // (Gathering and trading wait for the fight to end.)
        const actions = el('div', 'sheet-actions', section);
        const resource = s.visibleResource();
        if (s.canGather()) button('GATHER HERBS', 'primary', actions, () => this.act('gather'));
        else if (resource)
            el('span', 'muted', actions, wholeCount(resource, 'remaining') > 0 ? 'Approach the herb patch to gather.' : 'The visible herb patch is depleted.');
        const merchantId = str(obj(s.snapshot, 'merchant'), 'id');
        if (merchantId)
            button(merchantId === 'npc_keeper' ? 'TRADE WITH THE KEEPER' : 'TRADE WITH THE SHOPKEEPER', 'primary', actions, () => this.act('trade_open', merchantId));
    }

    /** The load (doc 35, 1.2): a bar to twice what is comfortable, marked at comfortable, and what it costs. */
    private loadMeter(parent: HTMLElement, self: Json | null) {
        const load = readLoad(self);
        if (!load) return;
        const row = el('div', `load-meter ${load.state}`, parent);
        row.title = `Comfortable up to ${weightLabel(load.comfortable)} (12 + STR ÷ 4). Up to twice that is heavy: slower, and ` +
            'running tires you more. Beyond it you can only walk, and can\'t fight. Everything you hold weighs, worn or not.';
        el('span', 'label', row, loadLabel(load).toUpperCase());
        const bar = el('div', 'bar thin load-bar', row);
        el('div', 'fill', bar).style.width = `${clamp(load.carried / (2 * load.comfortable), 0, 1) * 100}%`;
        el('div', 'load-mark', bar);
        const cost = loadCost(load);
        if (cost) el('span', 'small load-cost', row, cost);
    }

    /** What one carries, and what can be done with it: the purse, the items, equipping and using them. */
    private belongings(self: Json | null) {
        const s = this.s;
        const fight = s.battle && !s.battle.observer ? s.battle : null;
        el('div', 'gold', this.panel, `PURSE · ${countText(self, 'cash')} silver pennies`);
        this.loadMeter(this.panel, self);
        const items = arr(s.snapshot, 'inventory').filter(isObject);
        const grid = el('div', 'items', this.panel);
        if (!items.length) el('p', 'muted', grid, 'Your pack is empty. Objects you acquire will appear here.');
        for (const item of items) {
            const card = el('div', 'item', grid);
            el('div', `item-icon ${itemIcon(item)}`, card);
            const text = el('div', '', card);
            el('div', 'item-name', text, str(item, 'name'));
            el('div', `label ${bool(item, 'equipped') ? 'sage' : 'muted'}`, text,
                `${bool(item, 'equipped') ? 'EQUIPPED' : 'CARRIED'} · × ${wholeCount(item, 'quantity', 1)}` +
                `${num(item, 'weight') > 0 ? ` · ${weightLabel(num(item, 'weight') * wholeCount(item, 'quantity', 1)).toUpperCase()}` : ''}`);
            el('p', 'muted small', text, str(item, 'description'));
        }
        const actions = el('div', 'sheet-actions', this.panel);
        if (s.inventoryQuantity('meal') > 0 && !fight) button('EAT ONE MEAL', 'primary', actions, () => this.act('eat'));
        if (s.inventoryQuantity('sword') > 0) {
            const held = str(self, 'mouth') === 'sword';
            const label = held ? 'PUT THE SWORD AWAY' : 'HOLD THE SWORD IN YOUR JAWS';
            if (fight) {
                // In a fight it is the turn's action (doc 33).
                const mine = fight.turn === s.selfId;
                const go = button(`${label} · YOUR ACTION`, 'primary', actions, () => s.sendBattle(held ? 'stow' : 'hold'));
                go.disabled = !mine || fight.acted;
                go.title = !mine ? 'On your turn' : fight.acted ? 'You have acted this turn' : 'Uses this turn\'s action';
            } else button(label, 'primary', actions, () => this.act(held ? 'stow sword' : 'hold sword'));
        }
        if (fight) return;                          // (Gathering and trading wait for the fight to end.)
        const resource = s.visibleResource();
        if (s.canGather()) button('GATHER HERBS', 'primary', actions, () => this.act('gather'));
        else if (resource)
            el('span', 'muted', actions, wholeCount(resource, 'remaining') > 0 ? 'Approach the herb patch to gather.' : 'The visible herb patch is depleted.');
        const merchantId = str(obj(s.snapshot, 'merchant'), 'id');
        if (merchantId)
            button(merchantId === 'npc_keeper' ? 'TRADE WITH THE KEEPER' : 'TRADE WITH THE SHOPKEEPER', 'primary', actions, () => this.act('trade_open', merchantId));
    }

    private trade(self: Json | null) {
        const s = this.s, merchant = obj(s.snapshot, 'merchant');
        const available = !!str(merchant, 'id');
        this.heading('LOCAL TRADE / REAL GOODS & REAL PURSES', available ? str(merchant, 'name', 'The keeper').slice(0, 38) : 'The counter is unattended');
        if (!available) {
            el('p', 'muted', this.panel, 'The keeper is no longer awake, visible, and within reach. Return to them to see current stock and offers.');
            return;
        }
        el('p', 'muted', this.panel, 'One item per exchange. The authority rechecks every offer.');
        const purses = el('div', 'purses', this.panel);
        el('span', 'gold', purses, `YOUR PURSE · ${countText(self, 'cash')} p`);
        el('span', 'sage', purses, `KEEPER'S PURSE · ${countText(merchant, 'cash')} p`);
        // What this trader deals in, as the server lists it (a shopkeeper: herbs and meals; a smith: swords).
        for (const good of arr(merchant, 'items').filter(isObject).map(i => str(i, 'id')).filter(Boolean)) {
            const item = s.tradeItem(good);
            const card = el('div', 'trade', this.panel);
            el('div', 'item-name', card, str(item, 'name', good));
            el('div', 'label muted', card, `KEEPER STOCK ${countText(item, 'stock')} · YOU CARRY ${countText(item, 'owned')}`);
            const row = el('div', 'trade-row', card);
            for (const buy of [true, false]) {
                const side = el('div', '', row);
                const enabled = s.canTradeItem(good, buy);
                const b = button(`${buy ? 'BUY 1 · ' : 'SELL 1 · '}${countText(item, buy ? 'buyPrice' : 'sellPrice')} p`, 'primary', side,
                    () => this.act(buy ? 'trade_buy' : 'trade_sell', good));
                b.disabled = !enabled;
                el('p', enabled ? 'muted small' : 'gold small', side, enabled
                    ? (buy ? "Your purse pays for one item from the keeper's stock." : 'The keeper pays for one item from your pack.')
                    : str(item, buy ? 'buyReason' : 'sellReason', 'This offer is currently unavailable.').slice(0, 102));
            }
        }
        el('p', 'muted small', this.panel, 'p = silver penny · stock, demand, and cash are finite. Prices change as residents gather, cook, buy, and eat.');
    }

    private settings() {
        const s = this.s;
        this.heading('PREFERENCES / READING & PRESENCE', 'Make yourself heard');
        const cols = el('div', 'sheet-cols', this.panel);
        const left = el('div', 'sheet-col', cols);
        el('div', 'label gold', left, 'YOUR SPEAKING COLOR');
        el('p', 'muted small', left, 'One color follows your words, typing ellipsis, and speaking marker.');
        const swatches = el('div', 'swatches', left);
        for (let i = 0; i < 32; ++i) {
            const b = button('', 'swatch', swatches, () => this.act('color', String(i)));
            b.style.background = css(speakingColor(i));
            setClass(b, 'chosen', i === s.selectedColor);
        }
        el('p', '', left, '"There is always another story beyond the door."').style.color = css(speakingColor(s.selectedColor));
        const right = el('div', 'sheet-col', cols);
        el('div', 'label gold', right, 'STORY FLOW & THE MAP');
        const toggle = (label: string, action: string) => button(label, 'setting', right, () => this.act(action));
        toggle(`Reveal speed: ${s.revealSpeed === 0 ? 'Instant' : `${s.revealSpeed} characters / second`}`, 'speed');
        toggle(s.reducedMotion ? 'Reduced motion: On · static weather' : 'Reduced motion: Off', 'motion');
        toggle(s.flatWorld ? 'World: Always flat' : 'World: Automatic', 'projection');
        toggle(s.plainGlyphs ? 'Map: Plain ASCII' : 'Map: Unicode', 'glyphs');
        toggle(s.hoverTooltips ? 'Pointer labels: On' : 'Pointer labels: Off', 'tooltips');
        toggle(s.fightTips ? 'Fight tips: On' : 'Fight tips: Off', 'fighttips');
        toggle(`Combat sound: ${s.soundVolume === 0 ? 'Off' : s.soundVolume < 0.45 ? 'Quiet' : s.soundVolume < 0.8 ? 'Normal' : 'Loud'}`, 'sound');
        toggle(s.perfOverlay ? 'Performance overlay: On' : 'Performance overlay: Off', 'perf');
        const width = s.storyWidth <= 360 ? 'Compact' : s.storyWidth <= 460 ? 'Balanced' : s.storyWidth <= 600 ? 'Wide' : 'Text-first';
        toggle(`Story column: ${width} (or drag its edge)`, 'split');
        if (bool(s.snapshot, 'devTools')) {
            const dev = el('div', 'dev', this.panel);
            const group = (label: string, values: string[], action: string, names = values, active = '') => {
                el('div', 'label muted', dev, label);
                const row = el('div', 'dev-row', dev);
                values.forEach((v, i) => setClass(button(names[i], 'small', row, () => this.act(action, v)), 'active', v === active));
            };
            group('WORLD CLOCK · DEVELOPMENT ONLY', ['dawn', 'day', 'dusk', 'night'], 'time', undefined, s.environment.phase);
            group('ROOM LIGHTING · DEVELOPMENT ONLY', ['warm', 'unlit', 'daylit', 'cool'], 'lighting');
            group('DEVELOPMENT WEATHER', ['clear', 'overcast', 'rain', 'storm', 'fog', 'snow', 'sandstorm', 'seasonal'], 'weather',
                ['clear', 'cloud', 'rain', 'storm', 'fog', 'snow', 'sand', 'seasonal']);
            group('WIND FLOW · DEVELOPMENT ONLY', ['east', 'west', 'north', 'calm', 'live'], 'wind');
            group('CALL UP A FRONT HERE · DEVELOPMENT ONLY', ['rain', 'storm', 'fog', 'snow', 'sandstorm'], 'front');
            group('CALENDAR · DEVELOPMENT ONLY', ['day', 'year'], 'calendar', ['Day +1', 'Year +1']);
        }
        el('p', 'muted small', this.panel, 'ENTER write / send · SHIFT + ENTER newline · ESC preserve draft · ALT + mouse previews facing; click to turn.');
    }

    private leave() {
        this.heading('RETURN TO YOUR CHARACTERS', 'Leave this character?');
        el('p', 'muted', this.panel, "Your character remains saved. Returning to selection ends this play session. Unsent drafts and this session's " +
            'local transcript are not kept when switching characters.');
        const actions = el('div', 'sheet-actions', this.panel);
        button('RETURN TO SELECTION', 'primary', actions, () => this.act('leave_confirm'));
        button('KEEP PLAYING', 'secondary', actions, () => this.act('leave_cancel'));
    }

    private inspect() {
        const s = this.s;
        el('div', 'label gold', this.panel, 'A CLOSER LOOK');
        if (s.portraitAppearance()) {
            const cols = el('div', 'sheet-cols', this.panel);
            const left = el('div', 'sheet-col', cols);
            const inspected = s.inspectedCharacter;
            this.portrait(left, s.portraitAppearance(), s.portraitAge(), str(inspected, 'artwork'));
            if (str(inspected, 'artwork')) {
                const report = button('Report this portrait', 'secondary', left, () => s.reportPortrait(str(inspected, 'artwork')));
                report.title = 'If a picture is not fit to be seen, a Dungeon Master will look at it again.';
            }
            el('div', 'label sage', left, str(inspected, 'lifeStage', 'adult').toUpperCase() + (num(inspected, 'shoulderHeightCm') > 0
                ? ` · ${num(inspected, 'shoulderHeightCm').toFixed(0)} CM AT SHOULDER` : ''));
            el('p', 'pre', el('div', 'sheet-col', cols), s.inspectedText);
        } else el('p', 'pre', this.panel, s.inspectedText);
        // How they regard this wolf, and this wolf's own note on them (doc 32, 1.4).
        const inspected = s.inspectedCharacter, id = str(inspected, 'id');
        if (str(inspected, 'regard')) el('p', 'sage', this.panel, `They ${str(inspected, 'regard')}.`);
        if (obj(inspected, 'equipment'))
            button(id === s.selfId ? 'YOUR EQUIPMENT' : 'WHAT THEY WEAR', 'primary', el('div', 'sheet-actions', this.panel),
                () => { this.spot = ''; this.act(id === s.selfId ? 'status' : 'their_equipment'); });
        if (id && id !== s.selfId) {
            el('div', 'label gold', this.panel, 'YOUR NOTE');
            const row = el('div', 'name-add', this.panel);
            this.noteInput.value = str(inspected, 'note');
            row.append(this.noteInput);
            button('SAVE', 'small', row, () => s.sendSocial({verb: 'note', target: id, text: this.noteInput.value}));
        }
        el('p', 'muted small', this.panel, 'Only information your character is allowed to perceive appears here.');
    }
}

/** A plain icon for an item, by its kind (drawn in CSS). */
function itemIcon(item: Json): string {
    const kind = str(item, 'icon', 'bag');
    if (kind.includes('bag') || kind.includes('satchel') || kind.includes('pack')) return 'bag';
    if (str(item, 'id') === 'herbs') return 'herbs';
    if (kind.includes('bowl') || kind.includes('food')) return 'bowl';
    if (kind.includes('knife') || kind.includes('weapon')) return 'knife';
    if (kind === 'jewel') return 'jewel';
    if (kind === 'wear') return 'wear';
    return 'thing';
}
