// The sheets over the screen: character, belongings, trade, settings, leaving, and a closer look at someone. HTML
// dialogs (they were drawn on the canvas); built again only when what they show changes.
import {css} from '../color.ts';
import {speakingColor} from '../theme.ts';
import {drawPortrait, type Portraits} from '../portrait.ts';
import {arr, bool, clamp, countText, envNumber, isObject, num, obj, str, wholeCount, type Json} from '../../game/json.ts';
import {postureLabel, restLabel, readLoad, loadLabel, loadCost, weightLabel} from '../../game/labels.ts';
import type {GameState} from '../../game/state.ts';
import {button, el, setClass, show} from './dom.ts';
import {noRect} from './story.ts';
import {artCache} from '../artwork.ts';
import {renderLetters} from './letters.ts';
import {renderBoard} from './board.ts';
import {renderArchive, renderChronicle, renderJournal, renderWelcome} from './archive.ts';
import {nicknameLine, unfinishedLine} from '../../game/fame.ts';

/** Social standing is the account's (doc 49): scenes, stars and Stories across all one's wolves. The bar runs from the
 * level's start to the next (an older server sends neither: then a hundred a level, as it was). */
const SocialWhy = 'Social experience comes from roleplay alone: scenes, Gold Stars and Stories. It is your account\'s, shared by all your wolves. Skills grow by practice instead.';
function socialShare(self: Json | null): number {
    const xp = num(self, 'socialXp'), from = num(self, 'socialXpLevel', -1), to = num(self, 'socialXpNext', -1);
    return from >= 0 && to > from ? clamp((xp - from) / (to - from), 0, 1) : clamp((xp % 100) / 100, 0, 1);
}
/** A player's stars in words (doc 51): "★ 437 stars from 61 wolves", or in bands: "★ 250+ stars from 30+ wolves". */
export function starsLine(stars: Json | null): string {
    if (bool(stars, 'exact')) {
        const total = Math.trunc(num(stars, 'total')), from = Math.trunc(num(stars, 'from'));
        return total ? `★ ${total} star${total === 1 ? '' : 's'} from ${from} ${from === 1 ? 'wolf' : 'wolves'}` : '★ No stars yet';
    }
    const band = str(stars, 'band', 'a few'), from = str(stars, 'fromBand', 'a few');
    return band === 'a few' ? '★ A few stars' : `★ ${band} stars from ${from === 'a few' ? 'a few' : from} wolves`;
}
/** What a player's stars say of them (doc 51): Known for, the tags (counts for oneself, words for others), the rate. */
export function starsDetail(stars: Json | null): string {
    const known = arr(stars, 'knownFor').filter((k): k is string => typeof k === 'string');
    const tags = arr(stars, 'tags').filter(isObject).map(t =>
        bool(stars, 'exact') ? `${str(t, 'name')} ${Math.trunc(num(t, 'count'))}` : `${str(t, 'words')} ${str(t, 'name')}`);
    return [known.length ? `Known for: ${known.join(' and ')}` : '', tags.join(', '), str(stars, 'rate') ? upperFirst(str(stars, 'rate')) : '']
        .filter(Boolean).join(' · ');
}
const upperFirst = (t: string) => (t ? t[0].toUpperCase() + t.slice(1) : t);
const StarsWhy = 'Gold Stars and Story Stars, thanks from other players for roleplay, counted across all of a player\'s wolves. ' +
    'A friend who sees which wolf is theirs sees the exact count; everyone else sees it in bands.';
function socialLine(self: Json | null): string {
    const xp = Math.trunc(num(self, 'socialXp')), to = num(self, 'socialXpNext', -1);
    return to > xp ? `${xp} social experience · ${Math.trunc(to - xp)} to the next level` : `${xp} social experience`;
}

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
    private chosen = '';                        // The belonging picked out in the status screen,
    private spot = '';                          // and the fur spot.

    constructor(parent: HTMLElement, state: GameState, portraits: Portraits) {
        this.s = state;
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
        const key = JSON.stringify([m, art, !!artCache.get(art), m === 'inspect' ? [s.inspectedText, s.inspectedCharacter, this.inspectTab] : '',
            m === 'profile' ? [s.profileOwn, s.account, s.safetyMarks] : '', m === 'report' ? [s.reportTarget, s.safetyMarks] : '',
            m === 'people' ? [s.friends, s.friendRequestsIn, s.friendRequestsOut, s.account, s.peopleTab, s.knownWolves, s.knownOpen, this.knownFilter,
                s.circles, s.circleInvites] : '',
            m === 'stories' ? [s.shelf] : '', m === 'book' ? [s.bookOpen, s.circles] : '',
            m === 'inspect' ? s.safetyMarks : '', m === 'their_equipment' ? [s.inspectedCharacter, this.spot] : '', m === 'character' ? [self, s.reputation] : '',
            m === 'missions' ? s.missionBoard : '', m === 'letters' ? [s.lettersCase, s.letterDraftVersion, obj(self, 'names'), arr(s.snapshot, 'inventory')] : '',
            m === 'give' || m === 'lend' ? [s.giveTarget, arr(s.snapshot, 'inventory'), countText(self, 'cash')] : '',
            m === 'board' ? s.boardView : '', m === 'archive' ? s.archiveTask : '', m === 'journal' ? s.journalView : '',
            m === 'chronicle' ? s.chronicleView : '', m === 'welcome' ? s.welcomeView : '',
            m === 'chapter_window' ? [obj(self, 'chapter'), [...s.entities.values()].filter(e => e.kind !== 'npc').map(e => [e.id, e.name])] : '',
            m === 'inventory' || m === 'trade' || m === 'status' ? [arr(s.snapshot, 'inventory'), obj(s.snapshot, 'merchant'), countText(self, 'cash'), arr(self, 'loans'),
                obj(s.snapshot, 'resource')] : '',
            m === 'status' ? [self, s.battle && !s.battle.observer ? [s.battle.fighters.find(f => f.id === s.selfId), s.battle.acted, s.battle.turn] : null] : '',
            m === 'settings' ? [s.selectedColor, s.revealSpeed, s.reducedMotion, s.flatWorld, s.plainGlyphs, s.perfOverlay, s.storyWidth,
                bool(s.snapshot, 'devTools'), s.environment.phase, s.hoverTooltips, s.soundVolume, s.mapScenes, s.howlMarks] : '']);
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
        else if (m === 'letters') renderLetters(this.panel, s);
        else if (m === 'give') this.giveSheet();
        else if (m === 'board') renderBoard(this.panel, s);
        else if (m === 'archive') renderArchive(this.panel, s);
        else if (m === 'journal') renderJournal(this.panel, s);
        else if (m === 'chronicle') renderChronicle(this.panel, s);
        else if (m === 'welcome') renderWelcome(this.panel, s);
        else if (m === 'lend') this.giveSheet(true);
        else if (m === 'inventory') this.inventory(self);
        else if (m === 'status') this.status(self);
        else if (m === 'their_equipment') this.theirEquipment();
        else if (m === 'trade') this.trade(self);
        else if (m === 'settings') this.settings();
        else if (m === 'leave_character') this.leave();
        else if (m === 'profile') this.profileEditor();
        else if (m === 'report') this.safetyMenu();
        else if (m === 'people') this.people();
        else if (m === 'stories') this.bookshelf();
        else if (m === 'book') this.book();
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
        el('div', 'big', right, `Social level ${Math.trunc(num(self, 'socialLevel', 1))} · ${str(obj(self, 'social'), 'title', 'Stranger')}`);
        el('div', 'sage', right, socialLine(self)).title = SocialWhy;
        const stars = obj(obj(self, 'social'), 'stars');
        if (stars) el('div', 'gold', right, starsLine(stars)).title = StarsWhy;
        if (stars && starsDetail(stars)) el('div', 'sage small', right, starsDetail(stars));
        const bar = el('div', 'bar', right);
        el('div', 'fill', bar).style.width = `${socialShare(self) * 100}%`;
        const skills = el('div', 'skills', right);
        // Each out of its cap (doc 49), or 100 from an older server.
        const capOf = (id: string) => num(arr(self, 'skills').filter(isObject).find(r => str(r, 'id') === id) ?? null, 'cap', 100);
        el('span', 'sage', skills, `Sneak ${clamp(Math.trunc(num(self, 'sneakSkill')), 0, 100)} / ${Math.trunc(capOf('sneak'))}`);
        el('span', 'sage', skills, `Hearing ${clamp(Math.trunc(num(self, 'hearingSkill')), 0, 100)} / ${Math.trunc(capOf('listening'))}`);
        el('span', 'scent', skills, `Scent ${clamp(Math.trunc(num(self, 'scentSkill')), 0, 100)} / ${Math.trunc(capOf('tracking'))}`);
        el('span', 'muted', skills, `Nose ${Math.round(clamp(num(self, 'noseHealth', 1), 0, 1) * 100)}%`);
        this.injuries(right, self);
        this.names(right, self);
        this.nicknames(right, self);
        this.unfinished(right, self);
        this.welcome(right);
        // Stories are books now (doc 51, Phase 7): the bookshelf.
        button('STORIES', 'secondary', el('div', 'sheet-actions', right), () => this.s.openShelf('shelf', 'all')).title =
            'Your Stories, as books on a shelf: newest first; and your friends\', circles\' and Chapter\'s'
        this.reputation(right);
        el('div', 'label gold', this.panel, 'DESCRIPTION');
        el('p', '', this.panel, str(self, 'description', 'Your appearance belongs here.'));
        const actions = el('div', 'sheet-actions', this.panel);
        button('FRIENDS', 'secondary', actions, () => this.act('people')).title = 'Your friends, requests and private messages (doc 50)';
        const post = obj(self, 'letters');
        button(num(post, 'unread') ? `LETTERS (${num(post, 'unread')})` : 'LETTERS', 'secondary', actions, () => this.s.openLetters()).title =
            num(post, 'waiting') ? `${num(post, 'waiting')} waiting at the inns of ${str(post, 'waitingAt')}` : 'Your letter case (doc 55)';
        button('CHRONICLE', 'secondary', actions, () => this.s.send({type: 'social', verb: 'chronicle'})).title =
            'Your life so far, from what the world remembers (doc 56)';
        button('JOURNAL', 'secondary', actions, () => this.s.send({type: 'journal'})).title =
            'Lore read at archives, beasts brought down, plants found, places been (doc 54)';
        button('YOUR PROFILE', 'secondary', actions, () => this.act('profile')).title =
            'What others see of your wolf (a description, what you are doing, glances), your status, and your OOC notes, lines and veils.';
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

    /** Open threads (doc 56, 9): promises, work taken, a Story waiting for your word, letters unread. Never a nag. */
    private unfinished(parent: HTMLElement, self: Json | null) {
        const open = arr(self, 'unfinished').filter(isObject);
        if (!open.length) return;
        el('div', 'label gold', parent, 'UNFINISHED BUSINESS');
        for (const t of open) el('div', 'small', parent, unfinishedLine(t));
    }

    /** While you were away (doc 56, 10): kept on the sheet until you next leave. */
    private welcome(parent: HTMLElement) {
        const w = this.s.welcomeView;
        if (!w) return;
        el('div', 'label gold', parent, num(w, 'days') >= 1 ? `WHILE YOU WERE AWAY (${num(w, 'days')} ${num(w, 'days') === 1 ? 'DAY' : 'DAYS'})` : 'WHILE YOU WERE AWAY');
        const lines = arr(w, 'lines').map(String);
        if (!lines.length) el('div', 'muted small', parent, 'All much as you left it.');
        for (const l of lines) el('div', 'small', parent, l);
    }

    /** What residents call you for your deeds (doc 56): who first said it, and asking folk not to use one. */
    private nicknames(parent: HTMLElement, self: Json | null) {
        const list = arr(self, 'nicknames').filter(isObject);
        if (!list.length) return;
        el('div', 'label gold', parent, 'WHAT FOLK CALL YOU');
        for (const n of list) {
            const row = el('div', 'story-row', parent);
            el('span', bool(n, 'dropped') ? 'muted small' : 'small', row, nicknameLine(n));
            if (!bool(n, 'dropped'))
                button('ASK FOLK NOT TO USE IT', 'small', row, () => {
                    if (window.confirm(`Ask folk not to call you "${str(n, 'text')}"? They stop at once, and it won't come back.`))
                        this.s.send({type: 'social', verb: 'dropnickname', id: str(n, 'id')});
                }).title = 'Residents stop using it at once, and that deed never earns it again';
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
            el('span', bool(m, 'online') ? '' : 'muted', row, `${str(m, 'name')} · ${ranks[num(m, 'rank')] ?? ''}${bool(m, 'active') ? '' : ' · away'}` +
                `${str(m, 'sponsor') ? ` · brought in by ${str(m, 'sponsor')}` : ''}`);
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
    /** Giving (doc 55, 3): something carried (not worn) and/or coins, to the wolf chosen from its menu. */
    private giveSheet(lend = false) {
        const s = this.s;
        const who = s.entities.get(s.giveTarget);
        this.heading(lend ? 'LEND' : 'GIVE', who ? `To ${who.name}` : 'To whom?');
        const row = el('div', 'sheet-actions', this.panel);
        const item = el('select', 'profile-input', row) as HTMLSelectElement;
        item.dataset.field = 'give-item';
        if (!lend) el('option', '', item, 'Nothing, only coin').setAttribute('value', '');
        for (const i of arr(s.snapshot, 'inventory').filter(isObject).filter(i => str(i, 'id') !== 'token')) {
            const o = el('option', '', item, `${str(i, 'name')} (${wholeCount(i, 'quantity', 1)})`) as HTMLOptionElement;
            o.value = str(i, 'id');
        }
        const quantity = el('input', 'profile-input', row) as HTMLInputElement;
        quantity.type = 'number';
        quantity.min = '1';
        quantity.value = '1';
        quantity.title = 'How many';
        const coins = el('input', 'profile-input', row) as HTMLInputElement;
        coins.type = 'number';
        coins.min = lend ? '1' : '0';
        coins.max = lend ? '7' : '';
        coins.value = lend ? '3' : '0';
        coins.title = lend ? 'For how many game days (1 to 7)' : 'Silver pennies';
        coins.dataset.field = lend ? 'lend-days' : 'give-coins';
        button(lend ? 'LEND' : 'GIVE', 'primary', row, () => {
            const n = Math.max(1, Math.trunc(Number(quantity.value) || 1));
            if (lend) s.send({type: 'lend', target: s.giveTarget, item: item.value, quantity: n, days: Math.max(1, Math.min(7, Math.trunc(Number(coins.value) || 3)))});
            else s.send({type: 'give', target: s.giveTarget, item: item.value, quantity: n, coins: Math.max(0, Math.trunc(Number(coins.value) || 0))});
            this.act('close');
        });
        if (lend) {
            el('p', 'muted small', this.panel, 'They may wear and use it, never sell or give it. They hand it back, or a courier brings it back on the day; ' +
                'if it is gone, they owe you for it.');
            return;
        }
        el('p', 'muted small', this.panel, 'A player may accept or decline; a resident takes it unless it dislikes you. What you wear stays on you. ' +
            'A gift smells of you for a week, unless you are masked.');
    }

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

    /** The bookshelf (doc 51, Phase 7): Stories as books, newest at the top, scrolling down through every one; filtered
     * by shelf; Unaffiliated for those one isn't in that one's friends, circles and Chapter share. */
    private bookshelf() {
        const s = this.s, shelf = s.shelf;
        this.heading('STORIES', 'Your Stories, as books on a shelf');
        const tabs = el('div', 'creator-tabs', this.panel);
        for (const [id, label] of [['shelf', 'YOUR SHELF'], ['unaffiliated', 'UNAFFILIATED']] as const) {
            const b = button(label, shelf.tab === id ? 'tab active' : 'tab', tabs, () => s.openShelf(id, 'all'));
            b.dataset.tab = id;
        }
        const filters = el('div', 'profile-veils', this.panel);
        for (const [id, label] of [['all', 'ALL'], ['world', 'WORLD'], ['chapter', 'CHAPTER'], ['circle', 'CIRCLE'], ['friend', 'FRIEND'],
            shelf.tab === 'unaffiliated' ? ['everyone', 'EVERYONE'] : ['other', 'OTHER']] as const) {
            const b = button(label, shelf.filter === id ? 'small active' : 'small', filters, () => s.openShelf(shelf.tab, id));
            b.dataset.filter = id;
        }
        if (shelf.tab === 'shelf') {
            const start = el('div', 'profile-veils', this.panel);
            const title = el('input', 'profile-input', start);
            title.placeholder = 'A new book\'s title';
            title.maxLength = 80;
            title.dataset.field = 'book-title';
            button('START A BOOK', 'small', start, () => { if (title.value.trim()) s.sendBook('start', {title: title.value.trim()}); s.modal = 'book'; });
        }
        const case_ = el('div', 'bookshelf', this.panel);
        if (!shelf.books.length) el('p', 'muted small', case_, shelf.tab === 'shelf'
            ? 'No books yet. Start one here, or ADD TO A STORY from a scene\'s card.' : 'Nothing your friends, circles or Chapter share that you aren\'t in.');
        // Shelves of eight spines; the newest on the top shelf.
        for (let i = 0; i < shelf.books.length; i += 8) {
            const row = el('div', 'shelf-row', case_);
            for (const b of shelf.books.slice(i, i + 8)) {
                const spine = button('', `spine kind-${str(b, 'kind')}${str(b, 'state') === 'finished' ? ' finished' : ''}`, row, () => s.openBook(str(b, 'id')));
                el('span', 'spine-title', spine, str(b, 'title'));
                if (bool(b, 'official')) el('span', 'spine-mark', spine, '★');
                spine.title = `${str(b, 'title')} · ${num(b, 'chapters')} chapter${num(b, 'chapters') === 1 ? '' : 's'}` +
                    `${str(b, 'state') === 'finished' ? ' · finished' : ''}${str(b, 'flavour') ? `\n${str(b, 'flavour')}` : ''}`;
                spine.dataset.book = str(b, 'id');
            }
        }
        if (shelf.more) button('MORE BOOKS', 'small', case_, () => s.openShelf(shelf.tab, shelf.filter, true));
        case_.addEventListener('scroll', () => {
            if (shelf.more && case_.scrollTop + case_.clientHeight >= case_.scrollHeight - 40) s.openShelf(shelf.tab, shelf.filter, true);
        });
    }

    /** A book opened (doc 51, Phase 7): its summary or flavour, wolves, volume, and its chapters as a timeline; for its
     * wolves, what they may do with it. */
    private book() {
        const s = this.s, b = s.bookOpen;
        if (!b) {
            el('p', 'muted', this.panel, 'Opening the book…');
            return;
        }
        const id = str(b, 'id'), mine = bool(b, 'mine'), keeper = bool(b, 'keeperIsMe'), finished = str(b, 'state') === 'finished';
        button('← STORIES', 'small', this.panel, () => s.openShelf());
        this.heading(finished ? 'A FINISHED BOOK' : 'A BOOK', str(b, 'title'));
        el('p', 'muted small', this.panel, [`Kept by ${str(b, 'keeper')}`, `${num(b, 'chapters')} chapter${num(b, 'chapters') === 1 ? '' : 's'}`,
            bool(b, 'official') ? 'official' : '', str(b, 'storyline') ? `of the world's storyline "${str(b, 'storyline')}"` : '',
            `shared with ${{members: 'its wolves only', friends: 'friends', circle: str(b, 'circleName') || 'a circle', chapter: str(b, 'chapterName') || 'a Chapter', everyone: 'everyone'}[str(b, 'sharing')] ?? 'its wolves'}`]
            .filter(Boolean).join(' · '));
        if (finished && str(b, 'flavour')) el('p', 'gold', this.panel, str(b, 'flavour'));
        if (str(b, 'summary')) el('p', '', this.panel, `${str(b, 'summary')}${str(b, 'summaryBy') ? ` (${str(b, 'summaryBy')})` : ''}`);
        el('p', 'small', this.panel, `Its wolves: ${arr(b, 'wolves').filter((w): w is string => typeof w === 'string').join(', ')}`);
        const volume = arr(b, 'volume').filter(isObject);
        if (volume.length) {
            el('div', 'label gold', this.panel, 'ITS VOLUME');
            for (const v of volume) button(`${str(v, 'kind').toUpperCase()}: ${str(v, 'title')}`, 'small', this.panel, () => s.openBook(str(v, 'id'))).title = str(v, 'flavour');
        }
        if (str(b, 'privateWarning') && !finished) el('p', 'muted small', this.panel, str(b, 'privateWarning'));
        const f = obj(b, 'finishing');
        if (f) {
            const row = el('div', 'profile-veils', this.panel);
            el('span', 'gold small', row, `Finishing: ${num(f, 'agreed')} of ${num(f, 'of')} agree${bool(f, 'objected') ? ' · someone objects' : ` · or done in ${Math.ceil(num(f, 'quietLeft') / 86400)} days unless someone objects`}`);
            if (mine && !bool(f, 'youAgreed')) button('AGREE', 'small', row, () => s.sendBook('agree', {book: id}));
            if (mine) button('OBJECT', 'small', row, () => s.sendBook('object', {book: id}));
        }
        // Official (books are the one kind of Story): agreeing, and Story Stars once told.
        if (str(b, 'storyState') === 'pending' && mine && !bool(b, 'approved'))
            button('AGREE TO MAKE IT OFFICIAL', 'small', this.panel, () => s.sendBook('approve', {book: id}));
        for (const t of arr(b, 'starTargets').filter(isObject))
            button(`★ ${str(t, 'name')}`, 'small', this.panel, () => s.sendSocial({verb: 'storystar', story: str(b, 'story'), target: str(t, 'id')})).title =
                'A Story Star for how they told it';
        el('div', 'label gold', this.panel, 'CHAPTERS');
        const chapters = arr(b, 'chapterList').filter(isObject);
        if (!chapters.length) el('p', 'muted small', this.panel, 'No chapters yet: add a scene below.');
        chapters.forEach((c, i) => {
            const box = el('div', 'book-chapter', this.panel);
            box.dataset.chapter = str(c, 'id');
            el('div', 'gold', box, `${i + 1}. ${str(c, 'title')}`);
            el('div', 'muted small', box, [str(c, 'place'), num(c, 'at') ? new Date(num(c, 'at') * 1000).toLocaleDateString([], {day: 'numeric', month: 'short'}) : '',
                num(c, 'minutes') ? `${num(c, 'minutes')} min` : (num(c, 'ended') ? '' : 'going on'), bool(c, 'private') ? 'a private scene' : '',
                arr(c, 'wolves').filter((w): w is string => typeof w === 'string').join(', ')].filter(Boolean).join(' · '));
            if (bool(c, 'sealed')) el('p', 'muted small', box, 'A private scene: its summary once the book is finished.');
            else if (str(c, 'summary')) storyText(box, str(c, 'summary'), str(c, 'by') ? ` (${str(c, 'by')})` : '');
            if (mine && !finished) {
                const edit = el('div', 'profile-veils', box);
                const t = el('input', 'profile-input', edit);
                t.value = str(c, 'title');
                t.maxLength = 60;
                const text = el('textarea', 'profile-input', edit);
                text.placeholder = 'What happened in it';
                text.maxLength = 7000;
                text.rows = 4;
                text.value = bool(c, 'sealed') ? '' : str(c, 'summary');
                button('SAVE', 'small', edit, () => s.sendBook('chapter', {book: id, chapter: str(c, 'id'), title: t.value, text: text.value}));
                button('FROM MY RECAP', 'small', edit, () => s.sendBook('summarise', {book: id, chapter: str(c, 'id')})).title =
                    'Use your recap of this scene as its summary (with chapters before it, the model tells it on from them)';
                if (i > 0) button('↑', 'small', edit, () => s.sendBook('move', {book: id, chapter: str(c, 'id'), to: i - 1}));
                if (i + 1 < chapters.length) button('↓', 'small', edit, () => s.sendBook('move', {book: id, chapter: str(c, 'id'), to: i + 1}));
                button('TAKE OUT', 'small', edit, () => s.sendBook('remove', {book: id, chapter: str(c, 'id')}));
            }
        });
        if (!mine) return;
        el('div', 'label gold', this.panel, 'YOUR PART IN IT');
        if (!finished) {
            const scenes = arr(b, 'scenes').filter(isObject);
            const add = el('div', 'profile-veils', this.panel);
            const pick = el('select', '', add);
            for (const sc of scenes)
                pick.append(new Option(`${str(sc, 'place')}${bool(sc, 'fight') ? ' (a fight)' : ''}${num(sc, 'ended') ? '' : ' · going on'} · with ${arr(sc, 'with').filter((w): w is string => typeof w === 'string').join(', ') || 'no one'}`, str(sc, 'session')));
            const after = el('select', '', add);
            after.append(new Option('at the end', String(chapters.length - 1)));
            after.append(new Option('at the start', '-1'));
            chapters.forEach((c, i) => after.append(new Option(`after ${i + 1}. ${str(c, 'title')}`, String(i))));
            button('ADD THIS SCENE', 'small', add, () => { if (pick.value) s.sendBook('link', {book: id, session: pick.value, after: Number(after.value)}); });
            if (!scenes.length) el('span', 'muted small', add, 'No scenes of yours from the last 7 days to add.');
            const nextWrap = el('label', 'small', this.panel);
            const next = el('input', '', nextWrap);
            next.type = 'checkbox';
            next.checked = bool(b, 'nextScene');
            next.addEventListener('change', () => s.sendBook('next', {book: id, on: next.checked}));
            nextWrap.append(' My next scene goes into this book');
            const summary = el('div', 'profile-veils', this.panel);
            const text = el('input', 'profile-input', summary);
            text.placeholder = 'The book\'s summary';
            text.maxLength = 1200;
            text.value = str(b, 'summary');
            button('SAVE SUMMARY', 'small', summary, () => s.sendBook('summary', {book: id, text: text.value}));
            button('WRITE IT FOR ME', 'small', summary, () => s.sendBook('summarise', {book: id})).title = 'The model writes it from the chapters';
        }
        const actions = el('div', 'profile-veils', this.panel);
        if (keeper && !finished) {
            const share = el('select', '', actions);
            for (const [v, label] of [['members', 'Its wolves only'], ['friends', 'Friends'], ['chapter', 'My Chapter'], ['everyone', 'Everyone']])
                share.append(new Option(label, v));
            for (const c of s.circles) share.append(new Option(`Circle: ${str(c, 'name')}`, `circle:${str(c, 'id')}`));
            share.value = str(b, 'sharing') === 'circle' ? share.value : str(b, 'sharing');
            share.addEventListener('change', () => share.value.startsWith('circle:')
                ? s.sendBook('share', {book: id, sharing: 'circle', circle: share.value.slice(7)}) : s.sendBook('share', {book: id, sharing: share.value}));
            if (str(b, 'state') === 'open') button('FINISH', 'small', actions, () => s.sendBook('finish', {book: id})).title =
                'Propose finishing it: a majority of its recent wolves, or three days with no objection';
            if (!str(b, 'story')) button('MAKE IT OFFICIAL', 'small', actions, () => s.sendBook('official', {book: id})).title =
                'Official books pay their tellers when finished, as Stories do; two thirds of its wolves must agree';
        }
        const hideWrap = el('label', 'small', actions);
        const hide = el('input', '', hideWrap);
        hide.type = 'checkbox';
        hide.checked = bool(b, 'hidden');
        hide.addEventListener('change', () => s.sendBook('hide', {book: id, on: hide.checked}));
        hideWrap.append(' Hide me from its readers');
        const others = arr(b, 'finishedBooks').filter(isObject);
        if (finished && others.length) {
            const link = el('div', 'profile-veils', this.panel);
            const which = el('select', '', link);
            for (const o of others) which.append(new Option(str(o, 'title'), str(o, 'id')));
            const kind = el('select', '', link);
            for (const k of ['sequel', 'prequel', 'related']) kind.append(new Option(k, k));
            button('LINK INTO A VOLUME', 'small', link, () => s.sendBook('volume', {book: id, other: which.value, kind: kind.value})).title =
                'Readers see the whole volume at once';
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
        el('div', 'label muted', this.panel, `AGE ${wholeCount(self, 'age', 18, 10000)}  ·  SOCIAL LEVEL ${Math.trunc(num(self, 'socialLevel', 1))} ` +
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
            `(4 + STR ÷ 10, less hurt; twice after a turn of rest). A bite costs 8, a sword 9 or 10, running faster than a trot ` +
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
        // Each attribute's grade (doc 49: ▲ strong, ▼ weak) and how far it can grow, from the server's list.
        const attribute = (id: string) => arr(self, 'attributes').filter(isObject).find(a => str(a, 'id') === id) ?? null;
        const mark = (id: string) => ({strong: ' ▲', weak: ' ▼'} as Record<string, string>)[str(attribute(id), 'grade')] ?? '';
        const capNote = (id: string) => (attribute(id) ? `of ${Math.round(num(attribute(id), 'cap'))}` : '');
        stat(`STRENGTH${mark('strength')}`, envNumber(self, 'strength', 0, 100, 50).toFixed(0), capNote('strength'),
            'Stamina back each fight turn, what you can carry, how hard you hit. Grows with shoves and heavy loads.');
        stat(`DEXTERITY${mark('dexterity')}`, dex.toFixed(0), Math.abs(effectiveDex - dex) >= 0.1 ? `${effectiveDex.toFixed(1)} now` : capNote('dexterity'),
            'Speed and footing. Age, hurt and load change what it is now. Grows with dodging and sneaking.');
        stat(`WISDOM${mark('wisdom')}`, envNumber(self, 'wisdom', 0, 100, 50).toFixed(0), capNote('wisdom'), 'Mana, for the gifted. Grows with using a Gift.');
        if (attribute('stamina'))
            stat(`STAMINA${mark('stamina')}`, Math.round(num(attribute('stamina'), 'value', 50)).toString(), capNote('stamina'),
                `How fast your breath comes back (${num(self, 'staminaRecovery', 5).toFixed(1)} a second) and how long you can run. Grows with running far.`);
        const fightingCap = num(arr(self, 'skills').filter(isObject).find(r => str(r, 'id') === 'fighting') ?? null, 'cap', 100);
        stat('FIGHTING', clamp(Math.trunc(num(self, 'fightingSkill', 50)), 0, 100).toFixed(0), `of ${Math.trunc(fightingCap)}`,
            'How well you land and turn aside blows. It grows by fighting: blows landed and fights stood to the end (doc 49).');

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
        line(`SIGHT${mark('vision')}`, pct('vision'), `${pct('vision')}%${organ('eyeHealth') < 1 ? ` · eyes ${Math.round(organ('eyeHealth') * 100)}%` : ''}`);
        line(`HEARING${mark('hearing')}`, pct('hearing'), `${pct('hearing')}%${organ('earHealth') < 1 ? ` · ears ${Math.round(organ('earHealth') * 100)}%` : ''}`);
        line(`SCENT${mark('smell')}`, pct('smell'), `${pct('smell')}%${organ('noseHealth') < 1 ? ` · nose ${Math.round(organ('noseHealth') * 100)}%` : ''}`, 'scent');
        // Skills by practice (doc 49): each with its cap; "easing off" once today's practice in it reaches its soft
        // limit; the trades once begun. (Fighting is with the attributes.) An older server sends the three alone.
        const practised = arr(self, 'skills').filter(isObject).filter(r => str(r, 'id') !== 'fighting' &&
            (['sneak', 'listening', 'tracking'].includes(str(r, 'id')) || num(r, 'value') > 0 || bool(r, 'specialty')));
        if (!practised.length)
            for (const [name, key] of [['SNEAK', 'sneakSkill'], ['LISTENING', 'hearingSkill'], ['TRACKING', 'scentSkill']]) {
                const v = clamp(Math.trunc(num(self, key)), 0, 100);
                line(name, v, `${v} / 100`);
            }
        for (const r of practised) {
            const v = Math.max(0, num(r, 'value')), cap = Math.max(1, num(r, 'cap', 100));
            line(`${str(r, 'name').toUpperCase()}${bool(r, 'specialty') ? ' ★' : ''}`, v / cap * 100, `${Math.trunc(v)} / ${Math.trunc(cap)}`);
            senses.lastElementChild!.setAttribute('title', 'Grows by practice, faster beside someone better, slower near its cap.');
        }
        const easing = practised.filter(r => bool(r, 'easing')).map(r => str(r, 'name'));
        if (easing.length)
            el('div', 'label muted', senses, `EASING OFF TODAY · ${easing.join(', ').toUpperCase()}`).title =
                'Practised a lot today: these come slower until the day comes round.';
        if (num(self, 'restedPractice') > 0)
            el('div', 'label sage', senses, 'RESTED · PRACTICE COUNTS DOUBLE FOR A WHILE').title =
                'Time away fills a pool of rested practice: each gain is doubled from it until it is spent.';
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
        if (num(self, 'fedHours') > 0) condition(`Fed · ${num(self, 'fedHours').toFixed(1)} h`,
            'Stamina comes back a tenth faster and rest heals a little faster. A meal shared with another lasts twice as long.');
        const groomed = obj(self, 'groomed');
        if (groomed) condition(`Well-groomed, by ${str(groomed, 'by')} · ${num(groomed, 'hours').toFixed(1)} h`,
            bool(groomed, 'half') ? 'Self-groomed: half of a grooming\'s good, for two hours.'
                : 'Until a day is out or your next full rest: wounds heal a tenth faster and set into lasting ones less often, residents who ' +
                  'hardly know you warm to you sooner' + (bool(groomed, 'scent') ? ', and for now your scent carries less far.' : '.'));
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
            const family = gift === 'death_walker' ? 'Death Walker' : gift.charAt(0).toUpperCase() + gift.slice(1);
            el('div', 'big', box, `${bool(self, 'quickened') ? 'Quickened' : 'Gifted'} · ${family}`);
            el('p', 'muted small', box, bool(self, 'quickened') ? 'A Gift for fighting, at a frightening scale. Drawn on with mana.'
                : 'A Gift for work and for helping your side. Drawn on with mana.');
            el('div', 'small', box, `MANA · ${Math.floor(num(self, 'mana'))} / ${Math.floor(num(self, 'manaMax'))}`);
            // Its ways of working (doc 43): lent to a workshop near by, or used on oneself (Mend, Shortcut, Lighten Load...).
            for (const w of arr(self, 'giftWork').filter(isObject)) {
                const row = el('div', 'gift-work', box);
                el('span', 'gold', row, str(w, 'name'));
                el('span', 'muted small', row, str(w, 'summary'));
                if (bool(w, 'passive')) el('span', 'small sage', row, 'always');
                else button(`USE · ${Math.round(num(w, 'mana'))}`, 'small', row, () => this.s.send({type: 'giftwork', ability: str(w, 'id')})).title =
                    'Workshop Gifts are lent to the nearest workshop they help (within a few paces); the rest are for you.';
            }
            if (num(self, 'wardenAttention') > 0)
                el('p', 'muted small', box, 'Others have seen your Quickened magic. Somewhere, it is being written down.');
        }

        const standing = this.box(right, 'STANDING');
        el('div', 'big', standing, `Social level ${Math.trunc(num(self, 'socialLevel', 1))} · ${str(obj(self, 'social'), 'title', 'Stranger')}`);
        el('div', 'fill', el('div', 'bar', standing)).style.width = `${socialShare(self) * 100}%`;
        el('div', 'sage small', standing, socialLine(self)).title = SocialWhy;
        if (obj(obj(self, 'social'), 'stars')) el('div', 'gold small', standing, starsLine(obj(obj(self, 'social'), 'stars'))).title = StarsWhy;
        // The next Gift tier the account hasn't opened, and what it still takes (doc 49).
        for (const tier of ['gifted', 'quickened']) {
            const t = obj(obj(self, 'tiers'), tier);
            if (!t || t.open !== false) continue;
            const still = arr(t, 'progress').filter(isObject).filter(p => num(p, 'have') < num(p, 'need')).map(p => str(p, 'label'));
            el('div', 'muted small', standing, `NEXT · ${tier === 'gifted' ? 'GIFTED' : 'QUICKENED'} WOLVES: ${still.join(' · ') || 'Gifted first'}`).title =
                'Your account may make Gifted wolves once it has roleplayed as a Normal wolf a while, and Quickened ones once ' +
                'other wolves have shown they enjoy roleplaying with you.';
            break;
        }
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
        if (!arr(s.snapshot, 'inventory').filter(isObject).some(i => bool(i, 'blade') && wholeCount(i, 'quantity', 0) > 0)) return null;
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
                `${bool(item, 'blade') && bool(item, 'equipped') ? 'IN YOUR JAWS' : bool(item, 'equipped') ? 'WORN' : 'CARRIED'} · × ${wholeCount(item, 'quantity', 1)}` +
                `${str(item, 'tier') ? ` · ${str(item, 'tier').toUpperCase()}` : ''}` +
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
            const sword = bool(item, 'blade') ? this.swordAction(self, fight) : null;
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
            if (str(item, 'scent')) el('div', 'muted small letter-scent', text, str(item, 'scent')[0].toUpperCase() + str(item, 'scent').slice(1));
            el('div', `label ${bool(item, 'equipped') ? 'sage' : 'muted'}`, text,
                `${bool(item, 'equipped') ? 'EQUIPPED' : 'CARRIED'} · × ${wholeCount(item, 'quantity', 1)}` +
                `${num(item, 'weight') > 0 ? ` · ${weightLabel(num(item, 'weight') * wholeCount(item, 'quantity', 1)).toUpperCase()}` : ''}`);
            el('p', 'muted small', text, str(item, 'description'));
        }
        // Loans (doc 55, 8): what is borrowed (RETURN, near its lender) and what is lent out.
        for (const l of arr(self, 'loans').filter(isObject)) {
            const row = el('div', 'story-row', this.panel);
            el('span', bool(l, 'borrowed') ? '' : 'muted', row, bool(l, 'borrowed')
                ? `Borrowed from ${str(l, 'who')}: ${str(l, 'what')} · back in ${num(l, 'days').toFixed(1)} days`
                : `Lent to ${str(l, 'who')}: ${str(l, 'what')} · due in ${num(l, 'days').toFixed(1)} days`);
            if (bool(l, 'borrowed')) button('RETURN', 'small', row, () => s.send({type: 'return', loan: str(l, 'id')})).title = 'Within 2 tiles of them';
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
        toggle(s.howlMarks ? 'Howl marks: On' : 'Howl marks: Off', 'howl_marks').title =
            'Mark the way howls come from on the map for a minute (you still hear them)';
        toggle(s.mapScenes ? 'Scenes on the map: On · open ones near you' : 'Scenes on the map: Off', 'map_scenes').title =
            'Show open scenes near you on the minimap and World Map (and knock-to-join ones with a friend in them)';
        toggle(s.fightTips ? 'Fight tips: On' : 'Fight tips: Off', 'fighttips');
        toggle(bool(obj(s.snapshot, 'self'), 'noPvp') ? 'Fights with players: Auto-decline' : 'Fights with players: Open to challenges', 'nopvp');
        toggle(bool(obj(s.snapshot, 'self'), 'noHuntPartners') ? 'Hunting partners: Only those I invite' : 'Hunting partners: Anyone may join', 'huntpartners').title =
            'Whether anyone may join a hunt you start (your party and Chapter always may; you can invite anyone near)';
        toggle(bool(obj(s.snapshot, 'self'), 'noWorkPartners') ? 'Work partners: Only those I invite' : 'Work partners: Anyone may join', 'workpartners');
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
        // Before you go (doc 56, 9): the open threads, from the snapshot, so leaving never waits. Nothing is asked of you.
        const open = arr(obj(this.s.snapshot, 'self'), 'unfinished').filter(isObject);
        if (open.length) {
            el('div', 'label gold', this.panel, 'BEFORE YOU GO');
            for (const t of open) el('div', 'small', this.panel, unfinishedLine(t));
        }
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
        // How they regard this wolf (doc 32, 1.4).
        const inspected = s.inspectedCharacter, id = str(inspected, 'id');
        const profile = obj(inspected, 'profile');
        if (profile) this.profileCard(profile, id === s.selfId);
        if (str(inspected, 'regard')) el('p', 'sage', this.panel, `They ${str(inspected, 'regard')}.`);
        // Their player's stars (doc 51): exact for a friend who sees which wolf is theirs, in bands for everyone else.
        if (obj(inspected, 'stars') && id !== s.selfId) {
            el('p', 'gold', this.panel, starsLine(obj(inspected, 'stars'))).title = StarsWhy;
            if (starsDetail(obj(inspected, 'stars'))) el('p', 'sage small', this.panel, starsDetail(obj(inspected, 'stars')));
        }
        if (obj(inspected, 'equipment'))
            button(id === s.selfId ? 'YOUR EQUIPMENT' : 'WHAT THEY WEAR', 'primary', el('div', 'sheet-actions', this.panel),
                () => { this.spot = ''; this.act(id === s.selfId ? 'status' : 'their_equipment'); });
        // What this wolf keeps on them (doc 50, 5): a player's under YOU AND THEM; a resident, having no profile tabs, here.
        if (id && id !== s.selfId && !profile) {
            el('div', 'label gold', this.panel, 'YOU AND THEM');
            this.youAndThem(el('div', 'profile-card', this.panel));
        }
        el('p', 'muted small', this.panel, 'Only information your character is allowed to perceive appears here.');
    }

    /** Which tab of a closer look is open: 'look' or 'ooc' (doc 50). */
    private inspectTab = 'look';

    /** A player's roleplay profile on the card (doc 50): what this wolf may see of it, then the OOC tab, players only. */
    private profileCard(p: Json, own: boolean) {
        const tabs = el('div', 'creator-tabs', this.panel);
        for (const [id, label] of [['look', 'LOOK'], ['ooc', 'PROFILE (OOC)'], ...(own ? [] : [['them', 'YOU AND THEM']] as const)] as const) {
            const b = button(label, id === this.inspectTab ? 'tab active' : 'tab', tabs, () => {
                this.inspectTab = id;
                this.key = '';
            });
            b.dataset.tab = id;
        }
        const box = el('div', 'profile-card', this.panel);
        // Mute, block or report this wolf (doc 50).
        if (!own) {
            const id = str(this.s.inspectedCharacter, 'id'), label = str(this.s.inspectedCharacter, 'name');
            const marked = (kind: string) => this.s.safetyMarks.some(m => str(m, 'kind') === kind && str(m, 'character') === id);
            const row = el('div', 'profile-veils', box);
            button(marked('mute') ? 'UNMUTE' : 'MUTE', 'small', row, () => this.s.sendSafety(marked('mute') ? 'unmute' : 'mute', {target: id}))
                .title = 'You stop seeing their words. They are not told.';
            button(marked('block') ? 'UNBLOCK' : 'BLOCK', 'small', row, () => this.s.sendSafety(marked('block') ? 'unblock' : 'block', {target: id}))
                .title = 'You stop seeing their words, on any of their wolves, and they can\'t join your scenes, invite or challenge you. They are not told.';
            button('REPORT', 'small', row, () => this.s.openSafety({target: id, label}));
            // A friend request from the card (doc 50): it shows them your handle.
            button('ADD AS A FRIEND', 'small', row, () => this.s.sendFriends('request', {target: id})).title =
                'Ask to be friends: they see your handle, and if they accept, you see theirs';
        }
        if (this.inspectTab === 'look') {
            if (str(p, 'title')) el('p', 'gold', box, `${str(p, 'title')}${str(p, 'motto') ? ` · “${str(p, 'motto')}”` : ''}`);
            const facts = [bool(p, 'newcomer') ? 'new to these parts' : '',
                str(p, 'mentor') ? `Mentor${str(p, 'mentor') === 'busy' ? ' (busy)' : ''}${num(p, 'guided') ? ` · guided ${num(p, 'guided')} newcomer${num(p, 'guided') === 1 ? '' : 's'}` : ''}` : '',
                StatusLabel[str(p, 'status', 'ic')] ?? '',
                bool(p, 'walkup') ? 'walk-up friendly' : '', str(p, 'pronouns'),
                str(p, 'experience') ? `${ExperienceLabel[str(p, 'experience')] ?? str(p, 'experience')} roleplayer` : ''].filter(Boolean);
            if (facts.length) el('p', 'sage small', box, facts.join(' · '));
            if (str(p, 'currently')) el('p', '', box, `Currently: ${str(p, 'currently')}`);
            if (bool(p, 'folded')) el('p', 'muted', box, 'This profile may have mature content. (Settings: show mature profiles.)');
            for (const g of arr(p, 'glances').filter(isObject)) {
                const row = el('div', 'profile-glance', box);
                el('span', 'glance-icon', row, GlanceIcons[str(g, 'icon')] ?? '•');
                el('strong', '', row, str(g, 'title'));
                if (str(g, 'line')) el('span', 'muted', row, ` ${str(g, 'line')}`);
                if (str(g, 'sense') !== 'sight') el('span', 'label muted', row, str(g, 'sense') === 'scent' ? ' · SMELT' : ' · HEARD');
            }
        } else if (this.inspectTab === 'them' && !own) {
            this.youAndThem(box);
        } else {
            const ooc = obj(p, 'ooc');
            if (!ooc || !Object.keys(ooc).length) el('p', 'muted', box, own ? 'Nothing in your OOC tab yet.' : 'Nothing here.');
            if (str(ooc, 'notes')) el('p', '', box, str(ooc, 'notes'));
            const consent = obj(ooc, 'consent');
            if (consent && Object.keys(consent).length) {
                el('div', 'label gold', box, 'LINES AND VEILS');
                for (const [flag, answer] of Object.entries(consent))
                    el('p', 'small', box, `${ConsentLabel[flag] ?? flag}: ${answer === 'ask' ? 'ask me first' : answer}`);
            }
            if (str(ooc, 'otherLimits')) el('p', 'small', box, `Also: ${str(ooc, 'otherLimits')}`);
            const sliders = obj(ooc, 'sliders');
            if (sliders && Object.keys(sliders).length) {
                el('div', 'label gold', box, 'PERSONALITY');
                for (const [pair, n] of Object.entries(sliders)) {
                    const [lo, hi] = pair.split('/');
                    el('p', 'small', box, `${lo} ${'·'.repeat(Math.max(0, 10 + Math.min(0, Number(n))))}●${'·'.repeat(Math.max(0, 10 - Math.max(0, Number(n))))} ${hi}`);
                }
            }
            if (str(ooc, 'history')) {
                el('div', 'label gold', box, 'HISTORY (OOC: not for acting on unless your character has learned it)');
                el('p', 'pre', box, str(ooc, 'history'));
            }
        }
    }

    /** One's own profile (doc 50): what others see, status and walk-up, the OOC tab, and the account's handle,
     * experience and settings. Nothing is sent until SAVE (status, walk-up and the account's parts save at once). */
    private profileEditor() {
        const s = this.s, own = s.profileOwn ?? {}, account = s.account ?? {}, rules = s.profileRules ?? {};
        const limits = obj(rules, 'limits') ?? {};
        this.heading('YOUR PROFILE', 'What others see of your wolf');
        el('p', 'muted small', this.panel, 'Strangers see your description, what you are doing and your glances. Your title and motto ' +
            'show to wolves who know your name. Your OOC tab is for players only. Residents notice only what they could see.');
        const field = (label: string, key: string, area = false) => {
            const wrap = el('label', 'profile-field', this.panel);
            const most = num(limits, key, 120);
            const head = el('span', 'label gold', wrap, label);
            const input = area ? el('textarea', 'profile-input', wrap) : el('input', 'profile-input', wrap);
            input.value = str(own, key);
            input.maxLength = most;
            input.dataset.field = key;
            const count = el('span', 'label muted', head, ` ${input.value.length}/${most}`);
            input.addEventListener('input', () => (count.textContent = ` ${input.value.length}/${most}`));
            return input;
        };
        const inputs: Record<string, HTMLInputElement | HTMLTextAreaElement> = {};
        inputs.description = field('DESCRIPTION · what anyone sees of your wolf', 'description', true);
        inputs.currently = field('CURRENTLY · what you are doing ("mending nets by the pier, happy to chat")', 'currently');
        inputs.title = field('TITLE · for wolves who know your name', 'title');
        inputs.motto = field('MOTTO · for wolves who know your name', 'motto');
        // Glances: up to five, each an icon, a title, a line and the sense it is caught by.
        el('div', 'label gold', this.panel, 'GLANCES · what others notice at a glance');
        const glances = arr(own, 'glances').filter(isObject);
        const rows: Array<{icon: HTMLSelectElement; title: HTMLInputElement; line: HTMLInputElement; sense: HTMLSelectElement}> = [];
        for (let i = 0; i < num(limits, 'glances', 5); ++i) {
            const g = glances[i] ?? {};
            const row = el('div', 'profile-glance-edit', this.panel);
            const icon = el('select', '', row);
            for (const name of arr(rules, 'glanceIcons').filter((x): x is string => typeof x === 'string'))
                icon.append(new Option(`${GlanceIcons[name] ?? ''} ${name}`, name, false, name === str(g, 'icon', 'scar')));
            const title = el('input', 'profile-input', row);
            title.placeholder = 'A fresh scar';
            title.maxLength = num(limits, 'glanceTitle', 32);
            title.value = str(g, 'title');
            const line = el('input', 'profile-input', row);
            line.placeholder = 'over one eye';
            line.maxLength = num(limits, 'glanceLine', 120);
            line.value = str(g, 'line');
            const sense = el('select', '', row);
            for (const [id, label] of [['sight', 'seen'], ['scent', 'smelt (close)'], ['sound', 'heard']])
                sense.append(new Option(label, id, false, id === str(g, 'sense', 'sight')));
            rows.push({icon, title, line, sense});
        }
        // The OOC tab.
        el('div', 'label gold', this.panel, 'PROFILE (OOC) · players only');
        inputs.oocNotes = field('OOC NOTES', 'oocNotes', true);
        const consent: Record<string, HTMLSelectElement> = {};
        const veils = el('div', 'profile-veils', this.panel);
        for (const c of arr(rules, 'consent').filter(isObject)) {
            const wrap = el('label', '', veils);
            el('span', 'small', wrap, `${str(c, 'name')} `);
            const sel = el('select', '', wrap);
            for (const [id, label] of [['', '—'], ['yes', 'yes'], ['no', 'no'], ['ask', 'ask me first']])
                sel.append(new Option(label, id, false, id === str(obj(own, 'consent'), str(c, 'id'))));
            consent[str(c, 'id')] = sel;
        }
        inputs.otherLimits = field('OTHER LIMITS', 'otherLimits');
        inputs.history = field('HISTORY · optional, OOC', 'history', true);
        const sliders: Record<string, HTMLInputElement> = {};
        const sliderBox = el('div', 'profile-sliders', this.panel);
        for (const pair of arr(rules, 'sliders')) {
            if (!Array.isArray(pair) || pair.length !== 2) continue;
            const key = `${pair[0]}/${pair[1]}`;
            const wrap = el('label', 'profile-slider', sliderBox);
            el('span', 'small', wrap, String(pair[0]));
            const range = el('input', '', wrap);
            range.type = 'range';
            range.min = '-10';
            range.max = '10';
            const set = obj(own, 'sliders');
            range.value = String(set && typeof set[key] === 'number' ? set[key] : 0);
            range.dataset.set = set && typeof set[key] === 'number' ? '1' : '';
            range.addEventListener('input', () => (range.dataset.set = '1'));
            el('span', 'small', wrap, String(pair[1]));
            sliders[key] = range;
        }
        const matureWrap = el('label', 'small', this.panel);
        const mature = el('input', '', matureWrap);
        mature.type = 'checkbox';
        mature.checked = bool(own, 'mature');
        matureWrap.append(' This profile may have mature content');
        const actions = el('div', 'sheet-actions', this.panel);
        button('SAVE PROFILE', 'primary', actions, () => {
            const fields: Json = {};
            for (const [key, input] of Object.entries(inputs)) fields[key] = input.value;
            fields.glances = rows.filter(r => r.title.value.trim()).map(r => ({icon: r.icon.value, title: r.title.value, line: r.line.value, sense: r.sense.value}));
            fields.consent = Object.fromEntries(Object.entries(consent).map(([k, sel]) => [k, sel.value]));
            fields.sliders = Object.fromEntries(Object.entries(sliders).map(([k, r]) => [k, r.dataset.set ? Number(r.value) : null]));
            fields.mature = mature.checked;
            s.sendProfile('set', {fields});
        });
        // Status, walk-up and the account: saved at once.
        el('div', 'label gold', this.panel, 'STATUS');
        const statusRow = el('div', 'profile-veils', this.panel);
        for (const [id, label] of [['ic', 'In character'], ['lfs', 'Looking for a scene'], ['ooc', 'Out of character']])
            button(label, str(own, 'status', 'ic') === id ? 'small active' : 'small', statusRow, () => s.sendProfile('status', {value: id}));
        const walk = button(bool(own, 'walkup') ? 'Walk-up friendly: yes' : 'Walk-up friendly: no', 'small', statusRow,
            () => s.sendProfile('walkup', {on: !bool(own, 'walkup')}));
        walk.title = 'Fine for others to approach you unannounced.';
        el('div', 'label gold', this.panel, `YOUR ACCOUNT · ${num(account, 'playedHours').toFixed(1)} HOURS PLAYED`);
        const handleRow = el('div', 'name-add', this.panel);
        const handle = el('input', 'profile-input', handleRow);
        handle.value = str(account, 'handle');
        handle.placeholder = 'A handle friends and circles see (not your sign-in name)';
        button('SAVE HANDLE', 'small', handleRow, () => s.sendProfile('handle', {handle: handle.value}));
        const xpRow = el('div', 'profile-veils', this.panel);
        for (const x of arr(rules, 'experience').filter(isObject))
            button(str(x, 'name'), str(account, 'experience') === str(x, 'id') ? 'small active' : 'small', xpRow,
                () => s.sendProfile('experience', {value: str(x, 'id')}));
        // Mentoring newcomers (doc 52): opting in (social level 5, no upheld report lately), available or busy.
        const mentor = obj(account, 'mentor') ?? {};
        el('div', 'label gold', this.panel, `MENTORING${num(mentor, 'guided') ? ` · GUIDED ${num(mentor, 'guided')}` : ''}`);
        const mentorRow = el('div', 'profile-veils', this.panel);
        mentorRow.dataset.mentor = bool(mentor, 'on') ? (bool(mentor, 'available') ? 'available' : 'busy') : 'off';
        if (bool(mentor, 'on')) {
            button('Available', bool(mentor, 'available') ? 'small active' : 'small', mentorRow, () => s.send({type: 'mentor', verb: 'available'}));
            button('Busy', !bool(mentor, 'available') ? 'small active' : 'small', mentorRow, () => s.send({type: 'mentor', verb: 'busy'}));
            button('Stop mentoring', 'small', mentorRow, () => s.send({type: 'mentor', verb: 'optout'}));
            if (num(mentor, 'restingFor')) el('span', 'muted small', mentorRow, `resting after a tie, about ${num(mentor, 'restingFor')} h`);
        } else if (bool(mentor, 'may'))
            button('Mentor newcomers', 'small', mentorRow, () => s.send({type: 'mentor', verb: 'optin'})).title =
                'Newcomers see you as a mentor, and when you are available, a mark over you on their map';
        else el('span', 'muted small', mentorRow, str(mentor, 'why', 'Mentors are social level 5 or more.'));
        // One's tie (doc 52): the starter from this side, with whom, how it stands; End (a newcomer) or Release (a mentor).
        const tie = obj(obj(s.snapshot, 'self'), 'tie');
        if (tie) {
            el('div', 'label gold', this.panel, 'YOUR TIE');
            const box = el('div', 'tie-box', this.panel);
            box.dataset.tie = str(tie, 'state');
            el('p', '', box, str(tie, 'line'));
            el('p', 'muted small', box, str(tie, 'state') === 'active'
                ? [`with ${str(tie, 'other')}${bool(tie, 'resident') ? ' (a resident)' : ''}`, `${num(tie, 'scenes')} of 3 scenes together`,
                    `lapses in ${num(tie, 'lapsesInDays')} day${num(tie, 'lapsesInDays') === 1 ? '' : 's'}`,
                    obj(tie, 'marker') ? `the spot is marked on your map: ${str(obj(tie, 'marker'), 'place')}` : ''].filter(Boolean).join(' · ')
                : 'Someone is being found for you: a mentor first, else someone who lives here.');
            if (bool(tie, 'newcomer')) button('End tie', 'small', box, () => s.send({type: 'tie', verb: 'end'})).title =
                'End it early: what you have begun with them goes on as you like';
            else button('Release', 'small', box, () => s.send({type: 'mentor', verb: 'release'})).title =
                'Let it go: they are given a resident instead, and you rest a day';
        }
        const settings = obj(account, 'settings') ?? {};
        const toggle = (label: string, key: string) => {
            const wrap = el('label', 'small', this.panel);
            const box = el('input', '', wrap);
            box.type = 'checkbox';
            box.checked = bool(settings, key);
            box.addEventListener('change', () => s.sendProfile('settings', {settings: {[key]: box.checked}}));
            wrap.append(` ${label}`);
        };
        toggle('Show profiles marked mature', 'showMature');
        toggle('Write me scene recaps (what I perceived is sent to the model to summarise)', 'recaps');
        toggle('Private messages from friends', 'messages');
        toggle('Residents may point others to me, and me to others', 'matchmaking');
        toggle('Tell me when a friend comes into the world', 'toasts');
        // Who one has muted and blocked, by the wolf one pointed at.
        el('div', 'label gold', this.panel, 'MUTED AND BLOCKED');
        if (!s.safetyMarks.length) el('p', 'muted small', this.panel, 'No one. Mute or block a wolf from their card, or from a line they said (⚑).');
        for (const m of s.safetyMarks) {
            const row = el('div', 'profile-veils', this.panel);
            el('span', '', row, `${str(m, 'label')} · ${str(m, 'kind') === 'block' ? 'blocked' : 'muted'}`);
            button(str(m, 'kind') === 'block' ? 'UNBLOCK' : 'UNMUTE', 'small', row,
                () => s.sendSafety(str(m, 'kind') === 'block' ? 'unblock' : 'unmute', {target: str(m, 'character')}));
        }
    }

    /** Friends (doc 50, 4): ask by handle, answer requests, and each friend with whether they're here, the wolf they're
     * playing if they share it, your own share switch, Message and Remove. No places. */
    private people() {
        const s = this.s;
        const tabs = el('div', 'creator-tabs', this.panel);
        for (const [id, label] of [['friends', 'FRIENDS'], ['known', 'KNOWN WOLVES'], ['circles', 'CIRCLES']] as const) {
            const b = button(label, id === s.peopleTab ? 'tab active' : 'tab', tabs, () => {
                s.peopleTab = id;
                if (id === 'known') s.sendKnown('list');
            });
            b.dataset.tab = id;
        }
        if (s.peopleTab === 'known') {
            this.knownWolves();
            return;
        }
        if (s.peopleTab === 'circles') {
            this.circles();
            return;
        }
        this.heading('FRIENDS', 'Your friends, by handle: out of character, across all your wolves');
        if (!str(s.account, 'handle')) el('p', 'muted small', this.panel, 'Choose a handle in YOUR PROFILE first: friends know you by it.');
        const ask = el('div', 'profile-veils', this.panel);
        const handle = el('input', 'profile-input', ask);
        handle.placeholder = 'A friend\'s handle';
        handle.maxLength = 24;
        handle.dataset.field = 'friend-handle';
        button('ASK TO BE FRIENDS', 'small', ask, () => {
            if (handle.value.trim()) s.sendFriends('request', {handle: handle.value.trim()});
        });
        if (s.friendRequestsIn.length) {
            el('div', 'label gold', this.panel, 'ASKING YOU');
            for (const r of s.friendRequestsIn) {
                const row = el('div', 'profile-veils friend-row', this.panel);
                el('span', '', row, `${str(r, 'handle')}${str(r, 'wolf') ? ` · asked as ${str(r, 'wolf')}` : ''}`);
                button('ACCEPT', 'small', row, () => s.sendFriends('accept', {handle: str(r, 'handle')}));
                button('DECLINE', 'small', row, () => s.sendFriends('decline', {handle: str(r, 'handle')})).title = 'They are not told';
            }
        }
        if (s.friendRequestsOut.length) {
            el('div', 'label muted', this.panel, 'WAITING FOR AN ANSWER');
            for (const r of s.friendRequestsOut) {
                const row = el('div', 'profile-veils friend-row', this.panel);
                el('span', 'muted', row, str(r, 'handle'));
                button('WITHDRAW', 'small', row, () => s.sendFriends('cancel', {handle: str(r, 'handle')}));
            }
        }
        el('div', 'label gold', this.panel, `FRIENDS · ${s.friends.length}`);
        if (!s.friends.length) el('p', 'muted small', this.panel, 'No friends yet. Ask by handle above, or ADD AS A FRIEND on a wolf\'s card.');
        for (const f of s.friends) {
            const name = str(f, 'handle'), here = bool(f, 'online');
            const row = el('div', 'profile-veils friend-row', this.panel);
            row.dataset.friend = name;
            el('span', here ? 'sage' : 'muted', row, here ? '●' : '○').title = here ? 'In the world' : 'Away';
            el('span', 'gold', row, name);
            if (str(f, 'was')) el('span', 'muted small', row, `(was ${str(f, 'was')})`);
            if (here) el('span', 'small', row, str(f, 'character') ? `playing ${str(f, 'character')} (OOC)` : 'here');
            button('MESSAGE', 'small', row, () => s.messageFriend(name)).title = here ? `Write to ${name}` : `${name} is away: it will wait for them, 14 days`;
            const wrap = el('label', 'small', row);
            const share = el('input', '', wrap);
            share.type = 'checkbox';
            share.checked = bool(f, 'shares');
            share.addEventListener('change', () => s.sendFriends('share', {handle: name, on: share.checked}));
            wrap.append(' show them my wolf');
            wrap.title = 'When on, they see which wolf you are playing, and your handle under its label';
            button('REMOVE', 'small', row, () => s.sendFriends('remove', {handle: name}));
        }
    }

    /** Circles (doc 50, 6): out-of-character groups. Make one; answer invitations; each circle's roster (a member's wolf
     * only where they share it with the circle), nights in your own time, CHAT, your share switch, and for its keeper
     * and officers: invite, remove, officers, plan nights. */
    private circles() {
        const s = this.s;
        this.heading('CIRCLES', 'Out-of-character groups: a roleplay group, friends, a team');
        const make = el('div', 'profile-veils', this.panel);
        const name = el('input', 'profile-input', make);
        name.placeholder = 'A new circle\'s name';
        name.maxLength = 32;
        name.dataset.field = 'circle-name';
        button('MAKE A CIRCLE', 'small', make, () => { if (name.value.trim()) s.sendCircle('create', {name: name.value.trim()}); });
        if (s.circleInvites.length) {
            el('div', 'label gold', this.panel, 'INVITED');
            for (const i of s.circleInvites) {
                const row = el('div', 'profile-veils friend-row', this.panel);
                el('span', '', row, `${str(i, 'name')} · from ${str(i, 'from')}`);
                button('JOIN', 'small', row, () => s.sendCircle('accept', {circle: str(i, 'circle')}));
                button('DECLINE', 'small', row, () => s.sendCircle('decline', {circle: str(i, 'circle')})).title = 'They are not told';
            }
        }
        if (!s.circles.length) el('p', 'muted small', this.panel, 'You are in no circle yet.');
        for (const c of s.circles) {
            const id = str(c, 'id'), role = str(c, 'role'), officer = role === 'keeper' || role === 'officer';
            const box = el('div', 'known-row', this.panel);
            box.dataset.circle = id;
            const head = el('div', 'profile-veils', box);
            el('span', 'gold', head, str(c, 'name'));
            el('span', 'muted small', head, role);
            button('CHAT', 'small', head, () => s.activate({rect: noRect, action: `circle:${id}`, target: ''}));
            const wrap = el('label', 'small', head);
            const share = el('input', '', wrap);
            share.type = 'checkbox';
            share.checked = bool(c, 'shares');
            share.addEventListener('change', () => s.sendCircle('share', {circle: id, on: share.checked}));
            wrap.append(' show this circle my wolf');
            button('LEAVE', 'small', head, () => s.sendCircle('leave', {circle: id}));
            if (role === 'keeper') button('END IT', 'small', head, () => s.sendCircle('disband', {circle: id})).title = 'End the circle for everyone';
            for (const m of arr(c, 'members').filter(isObject)) {
                const row = el('div', 'profile-veils friend-row', box);
                el('span', bool(m, 'online') ? 'sage' : 'muted', row, bool(m, 'online') ? '●' : '○');
                el('span', '', row, str(m, 'handle'));
                if (str(m, 'role') !== 'member') el('span', 'muted small', row, str(m, 'role'));
                if (str(m, 'character')) el('span', 'small', row, `playing ${str(m, 'character')} (OOC)`);
                if (bool(m, 'you')) continue;
                if (role === 'keeper' && str(m, 'role') !== 'keeper')
                    button(str(m, 'role') === 'officer' ? 'MAKE MEMBER' : 'MAKE OFFICER', 'small', row,
                        () => s.sendCircle('officer', {circle: id, handle: str(m, 'handle'), on: str(m, 'role') !== 'officer'}));
                if ((officer && str(m, 'role') === 'member') || (role === 'keeper' && str(m, 'role') === 'officer'))
                    button('REMOVE', 'small', row, () => s.sendCircle('remove', {circle: id, handle: str(m, 'handle')}));
            }
            if (officer) {
                const ask = el('div', 'profile-veils', box);
                const handle = el('input', 'profile-input', ask);
                handle.placeholder = 'Invite by handle';
                handle.maxLength = 24;
                handle.dataset.field = 'circle-invite';
                button('INVITE', 'small', ask, () => { if (handle.value.trim()) s.sendCircle('invite', {circle: id, handle: handle.value.trim()}); });
                for (const h of arr(c, 'invited').filter((x): x is string => typeof x === 'string')) {
                    const row = el('div', 'profile-veils', box);
                    el('span', 'muted small', row, `${h} · invited`);
                    button('WITHDRAW', 'small', row, () => s.sendCircle('remove', {circle: id, handle: h}));
                }
            }
            el('div', 'label muted', box, 'NIGHTS');
            const nights = arr(c, 'nights').filter(isObject);
            if (!nights.length) el('p', 'muted small', box, 'None planned.');
            for (const n of nights) {
                const row = el('div', 'profile-veils', box);
                el('span', 'small', row, `${new Date(num(n, 'at') * 1000).toLocaleString([], {weekday: 'short', day: 'numeric', month: 'short', hour: '2-digit', minute: '2-digit'})} · ${str(n, 'place')}${str(n, 'line') ? ` · ${str(n, 'line')}` : ''}`);
                el('span', 'muted small', row, `by ${str(n, 'by')}`);
                if (officer) button('TAKE OFF', 'small', row, () => s.sendCircle('unnight', {circle: id, night: str(n, 'id')}));
            }
            if (officer) {
                const plan = el('div', 'profile-veils', box);
                const when = el('input', 'profile-input', plan);
                when.type = 'datetime-local';
                when.dataset.field = 'night-when';
                const where = el('input', 'profile-input', plan);
                where.placeholder = 'Where ("the Wharf tavern")';
                where.maxLength = 80;
                where.dataset.field = 'night-where';
                const line = el('input', 'profile-input', plan);
                line.placeholder = 'A line (optional)';
                line.maxLength = 160;
                button('PLAN A NIGHT', 'small', plan, () => {
                    const at = Date.parse(when.value) / 1000;      // (In this viewer's own time.)
                    if (Number.isFinite(at) && where.value.trim()) s.sendCircle('night', {circle: id, at, place: where.value.trim(), line: line.value.trim()});
                });
            }
        }
    }

    /** What the Known wolves list is filtered by: a name, and a tag. */
    private knownFilter = {text: '', tag: ''};

    /** Known wolves (doc 50, 5): this wolf's list of the wolves it has met, newest first, each as it knows them, with
     * where and when last met, scenes shared, its tag and note, and its latest recap; MORE shows them all. */
    private knownWolves() {
        const s = this.s, f = this.knownFilter;
        this.heading('KNOWN WOLVES', 'The wolves this wolf has met, as it knows them');
        const bar = el('div', 'profile-veils', this.panel);
        const search = el('input', 'profile-input', bar);
        search.placeholder = 'Search by name or note';
        search.value = f.text;
        search.dataset.field = 'known-search';
        search.addEventListener('change', () => { f.text = search.value.trim().toLowerCase(); this.key = ''; });
        const tags = el('select', '', bar);
        tags.append(new Option('Every tag', ''));
        for (const t of arr(obj(s.profileRules, 'known'), 'tags').filter(isObject))
            tags.append(new Option(str(t, 'name'), str(t, 'id')));
        tags.value = f.tag;
        tags.addEventListener('change', () => { f.tag = tags.value; this.key = ''; });
        const shown = s.knownWolves.filter(k => (!f.tag || str(k, 'tag') === f.tag) &&
            (!f.text || `${str(k, 'name')} ${str(k, 'note')} ${str(k, 'tag')}`.toLowerCase().includes(f.text)));
        if (!shown.length) el('p', 'muted small', this.panel, s.knownWolves.length ? 'No one matches.'
            : 'No one yet. Wolves you share a scene with, join a party with or learn the name of come here; a resident, once you note or tag it.');
        for (const k of shown) {
            const id = str(k, 'id'), open = str(s.knownOpen, 'id') === id;
            const row = el('div', 'known-row', this.panel);
            row.dataset.known = id;
            const head = el('div', 'profile-veils', row);
            el('span', 'gold', head, str(k, 'name'));
            if (bool(k, 'resident')) el('span', 'muted small', head, 'resident');
            if (str(k, 'tag')) el('span', 'chip', head, tagName(s, k));
            if (bool(k, 'unread')) el('span', 'sage small', head, 'profile changed').title = 'Their profile has changed since you last looked';
            el('span', 'muted small', head, [str(k, 'place'), ago(num(k, 'lastMet')), num(k, 'scenes') ? `${num(k, 'scenes')} scene${num(k, 'scenes') === 1 ? '' : 's'} shared` : '']
                .filter(Boolean).join(' · '));
            button(open ? 'LESS' : 'MORE', 'small', head, () => { if (open) s.knownOpen = null; else s.sendKnown('get', {target: id}); this.key = ''; });
            button('FORGET', 'small', head, () => s.sendKnown('forget', {target: id})).title = 'Take them off your list, with their recaps';
            if (str(k, 'note')) el('p', 'small', row, `✎ ${str(k, 'note')}`);
            if (str(k, 'tie')) el('p', 'sage small', row, str(k, 'tie'));
            const recaps = arr(open ? s.knownOpen : k, 'recaps').filter(isObject);
            for (const r of open ? recaps : recaps.slice(0, 1)) this.recapLine(row, r, id);
            if (!open && num(k, 'recapCount') > 1) el('p', 'muted small', row, `${num(k, 'recapCount') - 1} more · MORE`);
            if (open) this.youAndThemEdit(row, s.knownOpen ?? k, id);
        }
    }

    /** One recap: when, where, how long, in its words; Delete. */
    private recapLine(parent: HTMLElement, r: Json, target: string) {
        const box = el('div', 'recap', parent);
        el('div', 'muted small', box, [str(r, 'place'), ago(num(r, 'at')), num(r, 'minutes') ? `${num(r, 'minutes')} min` : '',
            bool(r, 'model') ? 'recap' : 'written from the record'].filter(Boolean).join(' · '));
        storyText(box, str(r, 'text'));
        button('DELETE', 'small', box, () => {
            this.s.sendKnown('unrecap', {recap: str(r, 'id')});
            this.s.sendKnown('get', {target});
        });
    }

    /** The tag and note this wolf keeps on another (doc 50, 5). */
    private youAndThemEdit(parent: HTMLElement, k: Json, target: string) {
        const s = this.s;
        const row = el('div', 'profile-veils', parent);
        el('span', 'label muted', row, 'TAG');
        const tag = el('select', '', row);
        tag.append(new Option('None', ''));
        for (const t of arr(obj(s.profileRules, 'known'), 'tags').filter(isObject)) tag.append(new Option(str(t, 'name'), str(t, 'id')));
        tag.append(new Option('Your own…', '*'));
        tag.value = bool(k, 'customTag') ? '*' : str(k, 'tag');
        const own = el('input', 'profile-input', row);
        own.placeholder = 'Your own tag';
        own.maxLength = 24;
        own.value = bool(k, 'customTag') ? str(k, 'tag') : '';
        show(own, tag.value === '*');
        tag.addEventListener('change', () => {
            show(own, tag.value === '*');
            if (tag.value !== '*') s.sendKnown('tag', {target, tag: tag.value});
        });
        own.addEventListener('change', () => s.sendKnown('tag', {target, tag: own.value.trim(), custom: true}));
        const noteRow = el('div', 'profile-veils', parent);
        const note = el('input', 'profile-input', noteRow);
        note.placeholder = 'A private note (only you see it)';
        note.maxLength = 500;
        note.value = str(k, 'note');
        button('SAVE NOTE', 'small', noteRow, () => s.sendKnown('note', {target, text: note.value}));
    }

    /** "You and them" on a card (doc 50, 2 and 5): last met, scenes shared, your tag, and recaps of scenes together. */
    private youAndThem(parent: HTMLElement) {
        const s = this.s, inspected = s.inspectedCharacter, id = str(inspected, 'id'), k = obj(inspected, 'known');
        if (!k) {
            el('p', 'muted small', parent, 'You haven\'t shared a scene yet. Tag them to keep them in Known wolves.');
            this.youAndThemEdit(parent, {}, id);
            return;
        }
        el('p', 'small', parent, [str(k, 'place') ? `Last met at ${str(k, 'place')}` : '', ago(num(k, 'lastMet')),
            num(k, 'scenes') ? `${num(k, 'scenes')} scene${num(k, 'scenes') === 1 ? '' : 's'} shared` : 'no scene shared yet'].filter(Boolean).join(' · '));
        if (str(k, 'tie')) el('p', 'sage small', parent, str(k, 'tie'));
        this.youAndThemEdit(parent, k, id);
        for (const r of arr(k, 'recaps').filter(isObject)) this.recapLine(parent, r, id);
    }

    /** Mute, block or report a wolf or a line's author (doc 50): a report goes to a Dungeon Master with the lines you
     * received from them. */
    private safetyMenu() {
        const s = this.s, about = s.reportTarget;
        if (!about) return;
        this.heading('MUTE, BLOCK OR REPORT', about.label);
        const which = about.line !== undefined ? {line: about.line} : {target: about.target};
        const actions = el('div', 'profile-veils', this.panel);
        button('MUTE', 'secondary', actions, () => { s.sendSafety('mute', which); this.act('close'); }).title = 'You stop seeing their words. They are not told.';
        button('BLOCK', 'secondary', actions, () => { s.sendSafety('block', which); this.act('close'); }).title =
            'You stop seeing their words on any of their wolves; they can\'t join your scenes, invite or challenge you. They are not told.';
        el('div', 'label gold', this.panel, 'REPORT TO A DUNGEON MASTER');
        el('p', 'muted small', this.panel, 'The lines you received from them lately go with it, as evidence. They are never told who reported them.');
        const category = el('select', '', this.panel);
        for (const [id, label] of [['harassment', 'Harassment'], ['hateful', 'Hateful content'], ['spam', 'Spam'], ['cheating', 'Cheating'], ['other', 'Other']])
            category.append(new Option(label, id));
        const kind = el('select', '', this.panel);
        kind.append(new Option('What they said', 'speech'));
        if (about.target) kind.append(new Option('Their profile', 'profile'));
        const note = el('input', 'profile-input', this.panel);
        note.placeholder = 'What happened (optional, 300 letters)';
        note.maxLength = 300;
        const alsoWrap = el('label', 'small', this.panel);
        const also = el('input', '', alsoWrap);
        also.type = 'checkbox';
        also.checked = true;
        alsoWrap.append(' Also block them');
        const send = el('div', 'sheet-actions', this.panel);
        button('SEND REPORT', 'primary', send, () => {
            s.sendSafety('report', {...which, category: category.value, kind: kind.value, note: note.value, block: also.checked});
            this.act('close');
        });
    }
}

/** Status, experience and consent in words; glance icons as signs (doc 50). */
const StatusLabel: Record<string, string> = {ic: 'In character', ooc: 'Out of character', lfs: 'Looking for a scene', storyteller: 'Storyteller'};
const ExperienceLabel: Record<string, string> = {newcomer: 'Newcomer', casual: 'Casual', experienced: 'Experienced', guide: 'Newcomer Guide'};
const ConsentLabel: Record<string, string> = {injury: 'Character injury', death: 'Character death', romance: 'Romance', crime: 'Criminal activity',
    control: 'Loss of control'};
const GlanceIcons: Record<string, string> = {scar: '⚔', eye: '👁', nose: '◉', ear: '◗', paw: '🐾', fang: '▼', coat: '≋', collar: '◯', tail: '〜',
    heart: '♥', star: '★', smoke: '☁', flower: '✿', bone: '⌇', feather: '❦', moon: '☾', drop: '💧', flame: '🔥'};

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

/** How long ago a moment was (Unix seconds), in plain words. */
/** A story's text (a scene told, a chapter): its paragraphs, scrolling past a few; `after` ends the last one. */
function storyText(parent: HTMLElement, text: string, after = '') {
    const box = el('div', 'story-text', parent);
    const paragraphs = text.split(/\n\s*\n|\n/).map((p) => p.trim()).filter(Boolean);
    paragraphs.forEach((p, i) => el('p', '', box, i + 1 === paragraphs.length ? p + after : p));
}

function ago(at: number): string {
    if (!at) return '';
    const minutes = Math.max(0, (Date.now() / 1000 - at) / 60);
    if (minutes < 2) return 'just now';
    if (minutes < 90) return `${Math.round(minutes)} minutes ago`;
    if (minutes < 36 * 60) return `${Math.round(minutes / 60)} hours ago`;
    const days = Math.round(minutes / 1440);
    return `${days} day${days === 1 ? '' : 's'} ago`;
}

/** A known wolf's tag in words: one of the rules' own, or the player's. */
function tagName(s: GameState, k: Json): string {
    if (bool(k, 'customTag')) return str(k, 'tag');
    const t = arr(obj(s.profileRules, 'known'), 'tags').filter(isObject).find(x => str(x, 'id') === str(k, 'tag'));
    return t ? str(t, 'name') : str(k, 'tag');
}
