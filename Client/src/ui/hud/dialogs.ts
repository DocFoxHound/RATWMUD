// The sheets over the screen: character, belongings, trade, settings, leaving, and a closer look at someone. HTML
// dialogs (they were drawn on the canvas); built again only when what they show changes.
import {css} from '../color.ts';
import {speakingColor} from '../theme.ts';
import {drawPortrait, type Portraits} from '../portrait.ts';
import {arr, bool, clamp, countText, envNumber, isObject, num, obj, str, wholeCount, type Json} from '../../game/json.ts';
import {postureLabel} from '../../game/labels.ts';
import type {GameState} from '../../game/state.ts';
import {button, el, setClass} from './dom.ts';
import {noRect} from './story.ts';
import {artCache} from '../artwork.ts';

export class Dialogs {
    private s: GameState;
    private portraits: Portraits;
    private overlay: HTMLElement;
    private panel: HTMLElement;
    private key = '';
    private aliasInput: HTMLInputElement;
    private noteInput: HTMLInputElement;

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
        const art = m === 'inspect' ? str(s.inspectedCharacter, 'artwork') : m === 'character' ? str(self, 'artwork') : '';
        const key = JSON.stringify([m, art, !!artCache.get(art), m === 'inspect' ? s.inspectedText : '', m === 'character' ? [self, s.reputation] : '',
            m === 'missions' ? s.missionBoard : '',
            m === 'chapter_window' ? [obj(self, 'chapter'), [...s.entities.values()].filter(e => e.kind !== 'npc').map(e => [e.id, e.name])] : '',
            m === 'inventory' || m === 'trade' ? [arr(s.snapshot, 'inventory'), obj(s.snapshot, 'merchant'), countText(self, 'cash'),
                obj(s.snapshot, 'resource')] : '',
            m === 'settings' ? [s.selectedColor, s.revealSpeed, s.reducedMotion, s.flatWorld, s.plainGlyphs, s.perfOverlay, s.storyWidth,
                bool(s.snapshot, 'devTools'), s.environment.phase, s.hoverTooltips, s.soundVolume] : '']);
        if (key === this.key) return;
        this.key = key;
        const typing = document.activeElement === this.aliasInput;      // (Kept, and kept focused, as the sheet is rebuilt.)
        this.panel.replaceChildren();
        const close = button('×', 'close', this.panel, () => this.act('close'));
        close.title = 'Close (Esc)';
        if (m === 'character') this.character(self);
        else if (m === 'chapter_window') this.chapter(self);
        else if (m === 'missions') this.missions();
        else if (m === 'inventory') this.inventory(self);
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
        this.names(right, self);
        this.stories(right, self);
        this.reputation(right);
        el('div', 'label gold', this.panel, 'DESCRIPTION');
        el('p', '', this.panel, str(self, 'description', 'Your appearance belongs here.'));
        const actions = el('div', 'sheet-actions', this.panel);
        button('CHARACTER SELECTION', 'primary', actions, () => this.act('leave_character'));
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
        const s = this.s;
        this.heading('BELONGINGS / EQUIPMENT', 'What you carry');
        el('div', 'gold', this.panel, `PURSE · ${countText(self, 'cash')} silver pennies`);
        const items = arr(s.snapshot, 'inventory').filter(isObject);
        const grid = el('div', 'items', this.panel);
        if (!items.length) el('p', 'muted', grid, 'Your pack is empty. Objects you acquire will appear here.');
        for (const item of items) {
            const card = el('div', 'item', grid);
            el('div', `item-icon ${itemIcon(item)}`, card);
            const text = el('div', '', card);
            el('div', 'item-name', text, str(item, 'name'));
            el('div', `label ${bool(item, 'equipped') ? 'sage' : 'muted'}`, text,
                `${bool(item, 'equipped') ? 'EQUIPPED' : 'CARRIED'} · × ${wholeCount(item, 'quantity', 1)}`);
            el('p', 'muted small', text, str(item, 'description'));
        }
        const actions = el('div', 'sheet-actions', this.panel);
        if (s.inventoryQuantity('meal') > 0) button('EAT ONE MEAL', 'primary', actions, () => this.act('eat'));
        if (s.inventoryQuantity('sword') > 0) {
            const held = str(self, 'mouth') === 'sword';
            button(held ? 'PUT THE SWORD AWAY' : 'HOLD THE SWORD IN YOUR JAWS', 'primary', actions, () => this.act(held ? 'stow sword' : 'hold sword'));
        }
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
    return 'thing';
}
