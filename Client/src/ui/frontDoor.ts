// The front door: signing in, the roster of up to six characters, and the creator (UI/SRatwFrontDoor.cpp), as
// ordinary HTML forms. Account lobby only: credentials are never kept anywhere, and the password box is cleared as soon
// as it has been sent.
import {CoatNames} from './theme.ts';
import {drawPortrait, lifeStage, Portraits, readAppearance, shoulderHeightCm} from './portrait.ts';
import {MaskNames, Masks} from './wolfArt.ts';
import {artCache, sendPortrait, squarePixels} from './artwork.ts';

// Natural coats, pale to dark (doc 29, phase 9); any colour is also allowed.
const CoatSwatches = ['#f4efe6', '#e8e1d3', '#ddd2bd', '#cfc0a2', '#c2ab84', '#b39a72', '#a3865f', '#8e7350', '#7a6142', '#655037',
    '#52412d', '#3d3125', '#2a221b', '#1a1612', '#e1d9c6', '#c8ccca', '#adb3b2', '#939a99', '#777d7b', '#5f6563', '#484d4c', '#303534',
    '#d9b48a', '#c9945e', '#b5733f', '#a26843', '#8f4f2a', '#7a3f22', '#c26b3a', '#d98a52', '#e0a86e', '#bfa27a', '#9c8a6a', '#8e8271',
    '#6b5a48', '#4f4236', '#d6c7a8', '#c4b393', '#a99a7c', '#e6d2b0', '#cfb184', '#b08d5f', '#94704a', '#7b5a3b', '#5e442e', '#45331f',
    '#ece7df', '#bcb4a8'];
const EyeSwatches = ['#d9a441', '#c8902e', '#e3c35a', '#a0702a', '#7a5228', '#5a3a1e', '#8a9a3a', '#6a8a4a', '#5f9fd0', '#8fb8d8',
    '#a9b3b8', '#3a2a1a'];

/** A whole new look at random (keeping the name and age). */
function randomiseAppearance(d: Json) {
    const pick = <T>(list: readonly T[]) => list[Math.floor(Math.random() * list.length)];
    d.species = pick(Choices.species);
    d.sex = pick(Choices.sex);
    d.stature = pick(Choices.stature);
    d.build = pick(['lean', 'average', 'average', 'heavy']);
    d.coat = pick(CoatSwatches);
    d.gradientTint = pick(CoatSwatches);
    d.gradientAmount = Math.round(Math.random() * 10) / 10;
    d.eyes = pick(EyeSwatches);
    const count = Math.floor(Math.random() * 4);
    if (count) d.markings = Array.from({length: count}, () => ({mask: pick(Masks), color: pick(CoatSwatches), opacity: 0.7 + Math.round(Math.random() * 3) / 10}));
    else delete d.markings;
}
import {arr, bool, isObject, num, obj, str, type Json} from '../game/json.ts';
import {newCommandId} from '../net/session.ts';

type Page = 'login' | 'register' | 'roster' | 'creator' | 'review';

const Choices: Record<string, string[]> = {
    species: ['timber', 'maned', 'arctic', 'red', 'ethiopian'],
    sex: ['female', 'male'],
    stature: ['short', 'average', 'tall'],
    pattern: ['solid', 'saddle', 'mantle', 'piebald'],
};
const title = (v: string) => (v ? v[0].toUpperCase() + v.slice(1) : v);
const stageName = (age: number) => (age < 13 ? 'Young' : age < 18 ? 'Adolescent' : age < 65 ? 'Adult' : 'Old');
const utf8Length = (s: string) => new TextEncoder().encode(s).length;

function el<K extends keyof HTMLElementTagNameMap>(tag: K, props: Partial<HTMLElementTagNameMap[K]> & {className?: string} = {},
    ...children: Array<Node | string>): HTMLElementTagNameMap[K] {
    const e = document.createElement(tag);
    Object.assign(e, props);
    e.append(...children);
    return e;
}

const KindNames: Record<string, string> = {work: 'Work', instant: 'Action', gathered: 'Gathered', channelled: 'Channelled', reaction: 'Reaction',
    fightlong: 'Whole fight', passive: 'Passive', shape: 'Shape', twoturn: 'Two turns'};

/** An ability's chips: its kind, and its mana (up front, while held, per painted tile). */
export function abilityChips(a: Json): string[] {
    const chips = [KindNames[str(a, 'kind')] ?? title(str(a, 'kind'))];
    const mana = num(a, 'mana'), perTurn = num(a, 'perTurn'), perTile = num(a, 'perTile');
    const parts = [...(mana > 0 ? [`${mana}`] : []), ...(perTurn > 0 ? [`${perTurn}/turn`] : []), ...(perTile > 0 ? [`${perTile}/tile`] : [])];
    if (parts.length) chips.push(`${parts.join(' + ')} mana`);
    return chips;
}

/** "Gifted · Water", after a separator, for a character with a Gift; nothing for a Normal one. */
export function giftCaption(c: Json | null, gifts: Json | null, before: string): string {
    const family = str(c, 'gift');
    if (!family) return '';
    const f = (Array.isArray(gifts?.families) ? (gifts!.families as Json[]) : []).find(x => str(x, 'id') === family);
    return `${before}${c?.quickened === true ? 'Quickened' : 'Gifted'} · ${str(f, 'name', title(family))}`;
}

export class FrontDoor {
    readonly root: HTMLDivElement;
    stage: 'login' | 'characters' = 'login';
    page: Page = 'login';
    message = '';
    error = false;
    busy = false;
    characters: Json[] = [];
    selectedId = '';
    draftName = '';
    draftAge = 18;
    draftAppearance: Json | null = null;
    draftTier = 'normal';                    // The Gift (doc 43): normal, gifted or quickened, and a family.
    draftFamily = '';
    gifts: Json | null = null;               // The tiers and families, as the server describes them.
    creation: Json | null = null;            // Strengths and weaknesses (doc 49): grades, budget, specialties, presets.
    tiers: Json | null = null;               // Which Gift tiers the account has earned, and what the rest need (doc 49).
    account: Json | null = null;             // The account as a person (doc 50): its handle, asked for when it has none.
    starts: Json[] = [];                     // Where a new wolf may arrive (doc 52): each town, the busiest preselected.
    firstCharacter = false;                  // The account's first wolf: told why it arrives where it does.
    draftStart = '';
    ties: Json[] = [];                       // Story starters for a tie (doc 52), and whether this wolf must take one.
    tieRequired = false;
    draftTie = '';
    draftGrades: Record<string, string> = {};   // An attribute to "weak" or "strong" (plain when absent).
    draftSpecialty = '';
    private showGiftDetails = false;
    private creationRequestId = '';
    private creationFingerprint = '';
    private sentAt = 0;
    private body: HTMLDivElement;
    private status: HTMLParagraphElement;
    private password: HTMLInputElement | null = null;
    private portraits = new Portraits();
    private portraitCanvas: HTMLCanvasElement | null = null;
    private timer: number;
    private send: (command: Json) => void;

    constructor(parent: HTMLElement, send: (command: Json) => void) {
        this.send = send;
        this.body = el('div', {className: 'door-body'});
        this.status = el('p', {className: 'door-status'});
        const stage = el('div', {className: 'door-stage'},
            el('header', {className: 'door-header'},
                el('div', {className: 'door-mark'}, 'W›'),
                el('div', {className: 'door-title'}, el('h1', {}, 'RUNS AGAINST THE WORLD'), el('p', {}, 'A living world. A story of your own.')),
                el('div', {className: 'door-edition'}, 'BROWSER CLIENT  /  EARLY DEVELOPMENT')),
            this.body, this.status,
            el('p', {className: 'door-footnote'},
                'LOCAL TEST ACCOUNTS ONLY · This connection is not encrypted yet. Use a unique test password, never a real one.'));
        this.root = el('div', {className: 'door'}, stage);
        parent.append(this.root);
        this.portraits.onReady = () => this.drawPortrait();
        artCache.onReady = () => this.drawPortrait();
        this.timer = window.setInterval(() => this.tick(), 500);
        window.addEventListener('keydown', this.onKey);
        window.addEventListener('resize', this.fit);
        this.fit();
        this.show('login');
    }

    destroy() {
        clearInterval(this.timer);
        window.removeEventListener('keydown', this.onKey);
        window.removeEventListener('resize', this.fit);
        this.root.remove();
    }

    /** The 1440×940 layout scaled to fit the window, as it was. */
    private fit = () => {
        const s = Math.min(window.innerWidth / 1440, window.innerHeight / 940);
        this.root.style.setProperty('--door-scale', String(s));
    };

    private onKey = (e: KeyboardEvent) => {
        if (e.code !== 'Escape' || this.busy) return;
        const back: Partial<Record<Page, Page>> = {review: 'creator', creator: 'roster', register: 'login'};
        const to = back[this.page];
        if (to) {
            e.preventDefault();
            this.show(to);
        }
    };

    private tick() {
        if (this.busy && performance.now() - this.sentAt > 20000) {
            this.busy = false;
            this.setMessage('No response from the authority yet. Check your connection before retrying; a request may still complete.', true);
            this.show(this.page);
        }
    }

    setMessage(text: string, error = false) {
        this.message = text;
        this.error = error;
        this.status.textContent = text;
        this.status.classList.toggle('error', error);
    }

    private submit(command: Json) {
        if (this.busy) return;
        this.busy = true;
        this.sentAt = performance.now();
        this.root.classList.add('busy');
        this.send(command);
    }

    /** A lobby event from the server. */
    receive(event: Json) {
        if (str(event, 'type') !== 'lobby') return;
        this.busy = false;
        this.root.classList.remove('busy');
        const ok = event.ok === true;
        const next = str(event, 'stage', 'login');
        this.setMessage(str(event, 'message'), !ok);
        if (this.password) this.password.value = '';
        if (next === 'characters') {
            if (isObject(event.gifts)) this.gifts = event.gifts;
            if (isObject(event.creation)) this.creation = event.creation;
            if (isObject(event.tiers)) this.tiers = event.tiers;
            if (isObject(event.account)) this.account = event.account;
            if (Array.isArray(event.starts)) this.starts = event.starts.filter(isObject);
            if (Array.isArray(event.ties)) this.ties = event.ties.filter(isObject);
            this.tieRequired = event.tieRequired === true;
            this.firstCharacter = event.firstCharacter === true;
            this.characters = (Array.isArray(event.characters) ? event.characters : []).filter(isObject).slice(0, 6);
            if (!this.selected() && this.characters.length) this.selectedId = str(this.characters[0], 'id');
            if (ok && this.page === 'review')
                for (const c of this.characters) if (str(c, 'name') === this.draftName) this.selectedId = str(c, 'id');
            const keepDraft = !ok && this.stage === 'characters' && (this.page === 'creator' || this.page === 'review');
            this.stage = 'characters';
            if (!keepDraft) this.show('roster');
            else this.show(this.page);
        } else {
            const wasIn = this.stage === 'characters';
            this.stage = 'login';
            this.characters = [];
            this.selectedId = '';
            if (wasIn || (this.page !== 'login' && this.page !== 'register')) {
                this.draftAppearance = null;
                this.draftName = '';
                this.show('login');
            } else this.show(this.page);
        }
    }

    selected(): Json | null {
        return this.characters.find(c => str(c, 'id') === this.selectedId) ?? null;
    }

    show(page: Page) {
        this.page = page;
        this.portraitCanvas = null;
        this.password = null;
        this.body.replaceChildren(page === 'login' || page === 'register' ? this.loginPage()
            : page === 'roster' ? this.rosterPage() : this.creatorPage(page === 'review'));
        this.root.dataset.page = page;
        this.drawPortrait();
        const first = this.body.querySelector<HTMLElement>('input, button.primary');
        first?.focus({preventScroll: true});
    }

    private button(text: string, action: () => void, primary = false): HTMLButtonElement {
        const b = el('button', {type: 'button', className: primary ? 'primary' : 'secondary', textContent: text});
        b.addEventListener('click', () => { if (!this.busy) action(); });
        return b;
    }

    // ------------------------------------------------------------------ Signing in

    private loginPage(): HTMLElement {
        const register = this.page === 'register';
        const user = el('input', {type: 'text', autocomplete: 'username', placeholder: '3–32 letters, digits, _ or -', maxLength: 32});
        const pass = el('input', {type: 'password', autocomplete: register ? 'new-password' : 'current-password',
            placeholder: 'Unique test password · 12–128 bytes'});
        this.password = pass;
        const form = el('form', {className: 'door-card door-login'},
            el('h2', {}, register ? 'Create a test account' : 'Welcome back'),
            el('p', {className: 'muted'}, register ? 'An account is private. Character names are what others see in the world.'
                : 'Sign in, then choose whose story you will continue.'),
            el('label', {}, el('span', {}, 'ACCOUNT NAME'), user),
            el('label', {}, el('span', {}, 'TEST PASSWORD'), pass),
            el('button', {type: 'submit', className: 'primary', textContent: register ? 'CREATE ACCOUNT' : 'SIGN IN'}),
            this.button(register ? 'Back to sign in' : 'New here? Create an account', () => {
                pass.value = '';
                this.setMessage('');
                this.show(register ? 'login' : 'register');
            }));
        form.addEventListener('submit', e => {
            e.preventDefault();
            if (this.busy) return;
            const username = user.value.trim(), secret = pass.value;
            if (!username || !secret) return this.setMessage('Enter an account name and a test password.', true);
            if (username.length < 3 || username.length > 32 || utf8Length(secret) < 12 || utf8Length(secret) > 128)
                return this.setMessage('Account names need 3–32 characters; test passwords need 12–128 UTF-8 bytes.', true);
            pass.value = '';
            this.setMessage('Contacting the world authority…');
            this.submit({type: register ? 'auth_register' : 'auth_login', username, password: secret});
        });
        const pitch = el('section', {className: 'door-card door-pitch'},
            el('p', {className: 'gold small'}, 'YOUR STORY BEGINS WITH A WOLF'),
            el('h2', {className: 'big'}, 'A name. A voice.', el('br'), 'A place in the world.'),
            el('p', {className: 'muted large'}, 'Explore through simple glyphs. Express yourself through words. The character sheet holds the ' +
                'portrait; the world leaves room for your imagination.'),
            el('p', {className: 'sage glyphs'}, 'W >   ·   ·   ·   #   +   #', el('br'), el('br'), 'ROLEPLAY  /  EXPLORATION  /  BELONGING'),
            el('p', {className: 'muted'}, 'Your account holds up to six characters. Characters are persistent; choosing another does not erase their story.'));
        return el('div', {className: 'door-row'}, pitch, form);
    }

    // ------------------------------------------------------------------ The roster

    private rosterPage(): HTMLElement {
        const slots = el('div', {className: 'door-slots'}, el('p', {className: 'gold small'}, 'CHOOSE YOUR CHARACTER'));
        for (let i = 0; i < 6; ++i) {
            const c = this.characters[i];
            const id = str(c, 'id');
            const age = num(c, 'age', 18);
            const b = el('button', {type: 'button', className: `slot${id && id === this.selectedId ? ' selected' : ''}`},
                el('strong', {}, c ? str(c, 'name') : '+  Create a character'),
                el('span', {}, c ? `Age ${age.toFixed(0)} · ${stageName(age)}${giftCaption(c, this.gifts, ' · ')}` : `Empty slot ${i + 1} of 6`));
            b.addEventListener('click', () => {
                if (this.busy) return;
                if (!id) this.startCreation();
                else {
                    this.selectedId = id;
                    this.show('roster');
                }
            });
            slots.append(b);
        }
        // The account's handle (doc 50): the name friends and circles see, never the sign-in name.
        if (this.account && !str(this.account, 'handle')) {
            const handle = el('input', {className: 'door-input', placeholder: 'Choose a handle (not your sign-in name)', maxLength: 24});
            handle.dataset.handle = '1';
            slots.append(el('div', {className: 'door-handle'},
                el('p', {className: 'gold small'}, 'YOUR HANDLE'),
                el('p', {className: 'muted small'}, 'Friends and circles will know you by it. It is never shown to strangers, and it can\'t be your sign-in name.'),
                handle, this.button('Save handle', () => {
                    this.setMessage('Saving your handle…');
                    this.submit({type: 'account_handle', handle: handle.value});
                })));
        }
        slots.append(el('div', {className: 'grow'}), this.button('Sign out of account', () => {
            this.setMessage('Signing out…');
            this.submit({type: 'auth_logout'});
        }));
        const c = this.selected();
        const age = num(c, 'age', 18);
        const canvas = el('canvas', {className: 'portrait', width: 480, height: 330});
        this.portraitCanvas = c ? canvas : null;
        const enter = this.button('ENTER THE WORLD', () => this.enter(), true);
        enter.disabled = !c;
        const detail = el('section', {className: 'door-card door-detail'},
            el('h2', {className: 'name'}, str(c, 'name', 'An unwritten story')),
            el('p', {className: 'muted large'}, c ? `${stageName(age)} · age ${age.toFixed(0)} · ${title(str(obj(c, 'appearance'), 'species', 'timber'))} wolf` +
                giftCaption(c, this.gifts, ' · ')
                : 'Choose an empty slot to make your first wolf.'),
            canvas,
            c ? this.portraitControls(c) : el('p', {className: 'muted large'}, ''),
            enter);
        return el('div', {className: 'door-row'}, slots, detail);
    }

    /** Under a character's portrait: their own picture (uploaded, cropped square, approved by a DM before others see it). */
    private portraitControls(c: Json): HTMLElement {
        const status = str(c, 'artworkStatus');
        const note = el('p', {className: 'muted'}, status === 'pending' ? 'Your own portrait is waiting for a Dungeon Master: only you see it until then.'
            : status === 'approved' ? 'Your own portrait is shown to everyone.' : 'Use your own artwork as this character\'s portrait, if you like.');
        const file = el('input', {type: 'file', accept: 'image/png,image/jpeg,image/webp,image/gif'});
        file.style.display = 'none';
        const choose = this.button(status ? 'REPLACE MY PORTRAIT' : 'UPLOAD MY OWN PORTRAIT', () => file.click());
        file.addEventListener('change', async () => {
            const picked = file.files?.[0];
            if (!picked) return;
            try {
                const {pixels} = await squarePixels(picked);
                this.setMessage('Uploading your portrait…');
                sendPortrait(pixels, str(c, 'id'), command => this.submit(command));
            } catch (error) {
                this.setMessage(error instanceof Error ? error.message : 'That picture could not be read.', true);
            }
        });
        return el('div', {className: 'portrait-controls'}, note, choose, file);
    }

    /** Portrait uploads answered (the lobby event that follows refreshes the roster). */
    artworkEvent(e: Json) {
        const type = str(e, 'type');
        if (type === 'artworkError') this.setMessage(str(e, 'text'), true);
        else if (type === 'artworkUploaded') this.setMessage(str(e, 'text'));
        else if (type === 'artwork') this.drawPortrait();
    }

    private enter() {
        if (!this.selected() || this.busy) return;
        this.setMessage('Joining the world…');
        this.submit({type: 'character_enter', id: this.selectedId});
    }

    // ------------------------------------------------------------------ Creating a character

    private startCreation() {
        if (this.busy || this.characters.length >= 6) return;
        this.draftName = '';
        this.draftAge = 18;
        this.draftTier = 'normal';
        this.draftFamily = '';
        this.draftGrades = {};
        this.draftSpecialty = '';
        this.draftStart = str(this.starts.find(t => t.suggested === true) ?? this.starts[0], 'id');
        this.draftTie = this.randomTie('');          // (A suggestion, different for every character: the user.)
        this.creationRequestId = this.creationFingerprint = '';
        this.draftAppearance = {species: 'timber', sex: 'female', stature: 'average', pattern: 'solid', baseColor: 2, gradientColor: 0,
            markingColor: 5, gradientAmount: 0.35, patternAmount: 0.65};
        this.setMessage('Appearance changes are a live preview. Nothing is saved until you confirm creation.');
        this.show('creator');
    }

    /** A page the tools ask for (the Unreal client's SetPresentationPage): only one the account's state allows. */
    presentationPage(page: string) {
        if (page === 'creator') this.startCreation();
        else if (page === 'roster' && this.stage === 'characters') this.show('roster');
        else if (page === 'review' && this.draftAppearance) this.review();
        else if ((page === 'login' || page === 'register') && this.stage === 'login') this.show(page);
    }

    /** A draft from a tool (the Unreal client's SetCharacterDraft): a preview only, never created without review. */
    setDraft(name: string, age: number, appearance: Json) {
        if (this.busy || age < 6 || age > 99 || !readAppearance(appearance)) return;
        this.draftName = name.trim().slice(0, 32);
        this.draftAge = age;
        this.draftAppearance = {...appearance};
        this.creationRequestId = this.creationFingerprint = '';
        this.setMessage('Appearance preview only. Review and confirm to create this character.');
        this.show('creator');
    }

    private describe(): string {
        const a = readAppearance(this.draftAppearance);
        return `${stageName(this.draftAge)} · age ${this.draftAge}\n${title(str(this.draftAppearance, 'stature'))} stature · ` +
            `${a ? shoulderHeightCm(a, this.draftAge).toFixed(0) : '0'} cm at shoulder` +
            giftCaption({gift: this.draftFamily, quickened: this.draftTier === 'quickened'}, this.gifts, '\n');
    }

    private creatorPage(reviewOnly: boolean): HTMLElement {
        const draft = this.draftAppearance ?? {};
        const nameHeading = el('h2', {className: 'name'}, this.draftName || 'Your wolf');
        const summary = el('p', {className: 'sage large pre'}, this.describe());
        const canvas = el('canvas', {className: 'portrait', width: 480, height: 330});
        this.portraitCanvas = canvas;
        const refresh = () => {
            summary.textContent = this.describe();
            this.drawPortrait();
        };
        const fields = el('div', {className: 'door-fields'});
        if (!reviewOnly) fields.append(this.creatorFields(draft, refresh, nameHeading));
        else {
            const colour = (field: string) => CoatNames[Math.min(7, Math.max(0, Math.trunc(num(draft, field))))];
            fields.append(
                el('h2', {}, 'Review your wolf'),
                el('p', {className: 'gold name'}, this.draftName),
                el('p', {className: 'large pre'}, `${title(str(draft, 'species'))} wolf · ${title(str(draft, 'sex'))}\nAge ${this.draftAge} · ` +
                    `${stageName(this.draftAge)}\n${title(str(draft, 'stature'))} stature · ${title(str(draft, 'pattern'))} markings`),
                el('p', {className: 'muted'}, `Coat: ${str(draft, 'coat') || colour('baseColor')}`),
                el('p', {className: 'muted'}, `Gradient: ${str(draft, 'gradientTint') || colour('gradientColor')}`),
                el('p', {className: 'muted'}, Array.isArray(draft.markings) && draft.markings.length
                    ? `Markings: ${(draft.markings as Json[]).map(m => MaskNames[str(m, 'mask')] ?? str(m, 'mask')).join(', ')}`
                    : `Markings: ${title(str(draft, 'pattern'))}, ${colour('markingColor')}`),
                el('p', {className: 'muted'}, `Build: ${title(str(draft, 'build') || 'average')} · eyes ${str(draft, 'eyes') || 'amber'}`),
                ...this.giftReview(),
                ...this.buildReview(),
                ...(this.draftStart ? [el('p', {className: 'muted'}, `Arrives in: ${this.startName(this.draftStart)}`)] : []),
                ...(this.ties.length ? [el('p', {className: 'muted'}, `Tie: ${this.draftTie ? this.tieLine(this.draftTie) : 'none'}`)] : []),
                el('p', {className: 'muted large'}, 'Creation saves this character to your account. You will return to character selection before ' +
                    'entering the world. Appearance does not grant free skill or stat bonuses.'));
        }
        const preview = el('section', {className: 'door-card door-preview'},
            el('p', {className: 'gold small'}, reviewOnly ? 'CREATION REVIEW' : 'LIVE APPEARANCE PREVIEW'),
            nameHeading, summary, canvas,
            el('p', {className: 'muted pre'}, 'Young 6–12 · Adolescent 13–17\nAdult 18–64 · Old 65+'),
            el('p', {className: 'muted'}, 'A static sheet portrait. Map characters remain simple W> glyphs; posture and storytelling stay in your hands.'));
        const actions = el('div', {className: 'door-actions'},
            this.button(reviewOnly ? 'Back to appearance' : 'Cancel', () => this.show(reviewOnly ? 'creator' : 'roster')),
            this.button(reviewOnly ? 'CONFIRM & CREATE' : 'REVIEW CHARACTER', () => (reviewOnly ? this.create() : this.review()), true));
        const form = el('section', {className: 'door-card door-form'}, el('div', {className: 'scroll'}, fields), actions);
        return el('div', {className: 'door-row'}, preview, form);
    }

    private creatorTab = 'body';

    /** The creator's controls, in tabs (doc 29, phase 9): body, coat, markings, eyes, name and age. */
    private creatorFields(draft: Json, refresh: () => void, nameHeading: HTMLElement): HTMLElement {
        const wrap = el('div', {className: 'creator'});
        const tabs = el('div', {className: 'creator-tabs'});
        const panel = el('div', {className: 'creator-panel'});
        const tabNames: Array<[string, string]> = [['body', 'Body'], ['coat', 'Coat'], ['markings', 'Markings'], ['eyes', 'Eyes'], ['gift', 'Gift'],
            ['strengths', 'Strengths'], ...(this.starts.length ? [['arrival', 'Arrival'] as [string, string]] : []),
            ...(this.ties.length ? [['tie', 'Tie'] as [string, string]] : []), ['name', 'Name & age']];
        const render = () => {
            tabs.replaceChildren(...tabNames.map(([id, label]) => {
                const b = el('button', {type: 'button', className: id === this.creatorTab ? 'tab active' : 'tab', textContent: label});
                b.addEventListener('click', () => {
                    this.creatorTab = id;
                    render();
                });
                return b;
            }));
            panel.replaceChildren(...this.creatorPanel(this.creatorTab, draft, () => {
                refresh();
                render();
            }, nameHeading));
        };
        const random = el('button', {type: 'button', className: 'secondary', textContent: 'Randomise'});
        random.addEventListener('click', () => {
            randomiseAppearance(draft);
            refresh();
            render();
        });
        render();
        wrap.append(tabs, panel, random);
        return wrap;
    }

    private creatorPanel(tab: string, draft: Json, changed: () => void, nameHeading: HTMLElement): HTMLElement[] {
        const choice = (field: string, caption: string, options: string[], fallback = '') => {
            const select = el('select', {});
            for (const option of options) select.append(el('option', {value: option, textContent: title(option)}));
            select.value = str(draft, field) || fallback;
            select.addEventListener('change', () => {
                draft[field] = select.value;
                changed();
            });
            return el('label', {}, el('span', {}, caption), select);
        };
        const swatches = (field: string, caption: string, colours: string[], allowCustom = true) => {
            const row = el('div', {className: 'swatch-grid'});
            for (const hex of colours) {
                const b = el('button', {type: 'button', title: hex, className: str(draft, field) === hex ? 'swatch chosen' : 'swatch'});
                b.style.background = hex;
                b.addEventListener('click', () => {
                    draft[field] = hex;
                    changed();
                });
                row.append(b);
            }
            const items: Array<Node> = [el('span', {}, caption), row];
            if (allowCustom) {
                const custom = el('input', {type: 'color', value: str(draft, field) || colours[0], title: 'Any colour'});
                custom.addEventListener('change', () => {
                    draft[field] = custom.value.toLowerCase();
                    changed();
                });
                items.push(el('label', {className: 'custom-colour'}, el('span', {}, 'or any colour'), custom));
            }
            return el('div', {className: 'field'}, ...items);
        };
        const amount = (field: string, caption: string) => {
            const label = el('span', {}, `${caption} · ${Math.round(num(draft, field) * 100)}%`);
            const slider = el('input', {type: 'range', min: '0', max: '1', step: '0.05', value: String(num(draft, field))});
            slider.addEventListener('input', () => {
                draft[field] = Number(slider.value);
                label.textContent = `${caption} · ${Math.round(Number(slider.value) * 100)}%`;
                this.drawPortrait();
            });
            return el('label', {}, label, slider);
        };
        if (tab === 'body')
            return [el('div', {className: 'pair'}, choice('species', 'SPECIES', Choices.species), choice('sex', 'SEX', Choices.sex)),
                el('div', {className: 'pair'}, choice('build', 'BUILD', ['lean', 'average', 'heavy'], 'average'),
                    choice('stature', 'STATURE · APPEARANCE ONLY', Choices.stature)),
                el('p', {className: 'muted'}, 'Species and sex shape the frame; build and stature change how it carries itself. None of it changes skills.')];
        if (tab === 'coat')
            return [swatches('coat', 'COAT', CoatSwatches), swatches('gradientTint', 'BELLY AND LEGS', CoatSwatches),
                amount('gradientAmount', 'HOW FAR THE BELLY COLOUR REACHES')];
        if (tab === 'eyes') return [swatches('eyes', 'EYES', EyeSwatches)];
        if (tab === 'gift') return this.giftPanel(changed);
        if (tab === 'strengths') return this.strengthsPanel(changed);
        if (tab === 'arrival') return this.arrivalPanel(changed);
        if (tab === 'tie') return this.tiePanel(changed);
        if (tab === 'name') {
            const name = el('input', {type: 'text', value: this.draftName, placeholder: 'The name others will know', maxLength: 64});
            name.addEventListener('input', () => {
                // A name always starts with a capital (the server makes it so too).
                const capital = name.value.charAt(0).toUpperCase() + name.value.slice(1);
                if (capital !== name.value) {
                    const at = name.selectionStart;
                    name.value = capital;
                    name.setSelectionRange(at, at);
                }
                this.draftName = name.value.slice(0, 64);
                nameHeading.textContent = this.draftName || 'Your wolf';
            });
            const age = el('input', {type: 'number', min: '6', max: '99', value: String(this.draftAge)});
            age.addEventListener('input', () => {
                const v = Math.trunc(Number(age.value));
                if (Number.isFinite(v)) {
                    this.draftAge = Math.min(99, Math.max(6, v));
                    this.drawPortrait();
                }
            });
            return [el('label', {}, el('span', {}, 'CHARACTER NAME'), name), el('label', {}, el('span', {}, 'STARTING AGE · 6–99'), age)];
        }
        // Markings: up to six layers, each a shape, a colour and a strength; the first is painted first.
        const list = Array.isArray(draft.markings) ? (draft.markings as Json[]) : [];
        const rows: HTMLElement[] = [el('p', {className: 'muted'}, list.length ? 'Each marking is painted over the last. Up to six.'
            : `No markings chosen: the ${str(draft, 'pattern')} pattern is shown. Add markings to paint your own.`)];
        list.forEach((m, i) => {
            const mask = el('select', {});
            for (const id of Masks) mask.append(el('option', {value: id, textContent: MaskNames[id]}));
            mask.value = str(m, 'mask');
            mask.addEventListener('change', () => {
                m.mask = mask.value;
                changed();
            });
            const colour = el('input', {type: 'color', value: str(m, 'color')});
            colour.addEventListener('change', () => {
                m.color = colour.value.toLowerCase();
                changed();
            });
            const strength = el('input', {type: 'range', min: '0.1', max: '1', step: '0.05', value: String(num(m, 'opacity', 1))});
            strength.addEventListener('input', () => {
                m.opacity = Number(strength.value);
                this.drawPortrait();
            });
            const remove = el('button', {type: 'button', className: 'small', textContent: '×', title: 'Remove'});
            remove.addEventListener('click', () => {
                list.splice(i, 1);
                if (!list.length) delete draft.markings;
                changed();
            });
            const up = el('button', {type: 'button', className: 'small', textContent: '↑', title: 'Paint earlier', disabled: i === 0});
            up.addEventListener('click', () => {
                [list[i - 1], list[i]] = [list[i], list[i - 1]];
                changed();
            });
            rows.push(el('div', {className: 'marking-row'}, mask, colour, strength, up, remove));
        });
        if (list.length < 6) {
            const add = el('button', {type: 'button', className: 'secondary', textContent: '+ Add a marking'});
            add.addEventListener('click', () => {
                draft.markings = [...list, {mask: 'socks', color: '#f2ede2', opacity: 1}];
                changed();
            });
            rows.push(add);
        }
        rows.push(choice('pattern', 'PATTERN (WITHOUT MARKINGS)', Choices.pattern));
        return rows;
    }

    /** The Gift tab (doc 43): Normal, Gifted or Quickened, then one of the eight families, with every ability it gives. */
    private giftPanel(changed: () => void): HTMLElement[] {
        const tiers = obj(this.gifts, 'tiers');
        const families = Array.isArray(this.gifts?.families) ? (this.gifts!.families as Json[]).filter(isObject) : [];
        const out: HTMLElement[] = [];
        const tierRow = el('div', {className: 'gift-tiers'});
        for (const tier of ['normal', 'gifted', 'quickened']) {
            const t = obj(tiers, tier);
            // Earned Gift tiers (doc 49): a locked tier says what it takes; it can still be opened to read its families.
            const locked = tier !== 'normal' && this.tierLocked(tier);
            const b = el('button', {type: 'button', className: `gift-card${this.draftTier === tier ? ' chosen' : ''}${locked ? ' locked' : ''}`},
                el('strong', {}, `${locked ? '🔒 ' : ''}${str(t, 'name', title(tier))}`), el('span', {}, str(t, 'best')),
                ...(locked ? arr(obj(this.tiers, tier), 'progress').filter(isObject).map(p => el('small', {className: 'gift-progress'}, str(p, 'label'))) : []));
            b.dataset.tier = tier;
            b.addEventListener('click', () => {
                this.draftTier = tier;
                if (tier === 'normal') this.draftFamily = '';
                changed();
            });
            tierRow.append(b);
        }
        out.push(el('span', {className: 'gift-heading'}, 'CLASSIFICATION'), tierRow);
        if (this.draftTier === 'normal') {
            out.push(el('p', {className: 'muted'}, 'Most wolves have no Gift. Choose Gifted or Quickened to pick one of the eight Gift families.'));
            return out;
        }
        if (!families.length) {
            out.push(el('p', {className: 'gold'}, 'The Gift families have not arrived from the server yet.'));
            return out;
        }
        const grid = el('div', {className: 'gift-families'});
        for (const f of families) {
            const id = str(f, 'id');
            const b = el('button', {type: 'button', className: `gift-family${this.draftFamily === id ? ' chosen' : ''}`},
                el('strong', {}, str(f, 'name')), el('span', {}, str(obj(f, this.draftTier), 'best')));
            b.dataset.family = id;
            b.style.setProperty('--family', str(f, 'colour', '#d9b67b'));
            b.addEventListener('click', () => {
                this.draftFamily = id;
                this.showGiftDetails = true;
                changed();
            });
            grid.append(b);
        }
        out.push(el('span', {className: 'gift-heading'}, 'GIFT FAMILY'), grid);
        const chosen = families.find(f => str(f, 'id') === this.draftFamily);
        if (chosen) {
            const details = this.giftDetails(chosen);
            out.push(details);
            if (this.showGiftDetails) requestAnimationFrame(() => details.scrollIntoView({block: 'start'}));   // (A family just chosen.)
            this.showGiftDetails = false;
        }
        else out.push(el('p', {className: 'muted'}, 'Choose a family to see what it gives.'));
        return out;
    }

    /** One family at the chosen tier: what it is, how a foe spots it, what it costs, and each ability in a line. */
    private giftDetails(f: Json): HTMLElement {
        const t = obj(f, this.draftTier);
        const quickened = this.draftTier === 'quickened';
        const box = el('div', {className: 'gift-details'});
        box.style.setProperty('--family', str(f, 'colour', '#d9b67b'));
        box.append(el('p', {className: 'gold small'}, `${str(f, 'name').toUpperCase()} · ${quickened ? 'QUICKENED' : 'GIFTED'}`),
            el('p', {className: 'large'}, str(t, 'best')),
            el('p', {className: 'muted'}, str(f, 'domain')),
            el('dl', {className: 'gift-terms'},
                el('dt', {}, 'Tell'), el('dd', {}, str(t, 'tell')),
                el('dt', {}, 'Cost'), el('dd', {}, str(t, 'cost')),
                el('dt', {}, 'Limit'), el('dd', {}, str(t, 'limit')),
                ...(str(t, 'overreach') ? [el('dt', {}, 'Overreach'), el('dd', {}, str(t, 'overreach'))] : [])));
        const abilities = Array.isArray(t?.abilities) ? (t!.abilities as Json[]).filter(isObject) : [];
        const atWork = (a: Json) => str(a, 'kind') === 'work' || a.work === true;
        const list = (heading: string, items: Json[]) => {
            if (!items.length) return;
            box.append(el('p', {className: 'gift-heading'}, heading));
            for (const a of items) {
                const row = el('div', {className: 'gift-ability'},
                    el('strong', {}, str(a, 'name')),
                    el('span', {className: 'gift-chips'}, ...abilityChips(a).map(c => el('span', {className: 'gift-chip'}, c))),
                    el('span', {className: 'muted'}, str(a, 'summary')));
                row.dataset.ability = str(a, 'id');
                box.append(row);
            }
        };
        if (quickened) list('IN A FIGHT', abilities);
        else {
            list('AT WORK', abilities.filter(atWork));
            list('IN A FIGHT', abilities.filter(a => !atWork(a)));
        }
        box.append(el('p', {className: 'muted'}, quickened
            ? 'A Quickened wolf has none of the Gifted abilities: the Gifted stay the best at work. Quickened magic others see draws the Wardens.'
            : 'Gifted abilities never deal damage: they help your side win. Each ability will be added to the game in turn.'));
        return box;
    }

    /** Points a build spends (doc 49): each strength costs, each weakness gives back, and the Gift tier its cost. */
    private buildSpent(grades: Record<string, string> = this.draftGrades): number {
        const c = this.creation;
        let spent = num(obj(c, 'tierCost'), this.draftTier);
        for (const g of Object.values(grades)) spent += g === 'strong' ? num(c, 'strongCost', 1) : g === 'weak' ? -num(c, 'weakRefund', 1) : 0;
        return spent;
    }

    /** Whether a build is within the budget and the limits. */
    private buildFits(grades: Record<string, string>): boolean {
        const c = this.creation, values = Object.values(grades);
        return this.buildSpent(grades) <= num(c, 'budget', 2) && values.filter(g => g === 'strong').length <= num(c, 'mostStrong', 3) &&
            values.filter(g => g === 'weak').length <= num(c, 'mostWeak', 3);
    }

    /** The Strengths tab (doc 49): a preset or one's own build, each attribute weak, plain or strong (each with where it
     * starts and how far it can grow), the points left, and one specialty. */
    private strengthsPanel(changed: () => void): HTMLElement[] {
        const c = this.creation;
        if (!c) return [el('p', {className: 'gold'}, 'Strengths and weaknesses have not arrived from the server yet.')];
        const out: HTMLElement[] = [];
        const presets = arr(c, 'presets').filter(isObject);
        const chips = el('div', {className: 'build-presets'});
        const gradesOf = (p: Json) => {
            const g: Record<string, string> = {};
            for (const a of arr(p, 'strong')) if (typeof a === 'string') g[a] = 'strong';
            for (const a of arr(p, 'weak')) if (typeof a === 'string') g[a] = 'weak';
            return g;
        };
        const sameAs = (p: Json) => JSON.stringify(Object.entries(gradesOf(p)).sort()) === JSON.stringify(Object.entries(this.draftGrades).sort()) &&
            str(p, 'specialty') === this.draftSpecialty;
        for (const p of presets) {
            const b = el('button', {type: 'button', className: `build-chip${sameAs(p) ? ' chosen' : ''}`, textContent: str(p, 'name')});
            b.dataset.preset = str(p, 'id');
            b.addEventListener('click', () => {
                this.draftGrades = gradesOf(p);
                this.draftSpecialty = str(p, 'specialty');
                changed();
            });
            chips.append(b);
        }
        const own = el('button', {type: 'button', className: `build-chip${presets.some(sameAs) ? '' : ' chosen'}`, textContent: 'Build my own'});
        own.addEventListener('click', () => {
            this.draftGrades = {};
            this.draftSpecialty = '';
            changed();
        });
        chips.append(own);
        out.push(el('span', {className: 'gift-heading'}, 'PRESETS'), chips);
        const left = num(c, 'budget', 2) - this.buildSpent();
        out.push(el('span', {className: 'gift-heading'}, `ATTRIBUTES · POINTS LEFT: ${left}`));
        out.push(el('p', {className: 'muted small'}, 'A strength starts higher and can grow further; a weakness starts lower and stops sooner. ' +
            'Everything grows by practice.'));
        const rows = el('div', {className: 'build-rows'});
        for (const a of arr(c, 'attributes').filter(isObject)) {
            const id = str(a, 'id'), percent = bool(a, 'percent');
            const shown = (v: unknown) => (typeof v === 'number' ? (percent ? `${Math.round(v * 100)}%` : `${Math.round(v)}`) : '—');
            const row = el('div', {className: 'build-row'});
            row.dataset.attribute = id;
            row.append(el('strong', {}, str(a, 'name')));
            const current = this.draftGrades[id] ?? 'plain';
            for (const grade of ['weak', 'plain', 'strong']) {
                const range = arr(a, grade);
                const next = {...this.draftGrades};
                if (grade === 'plain') delete next[id]; else next[id] = grade;
                const fits = grade === current || this.buildFits(next);
                const b = el('button', {type: 'button', className: `build-grade${grade === current ? ' chosen' : ''}`},
                    el('span', {}, title(grade)), el('small', {}, `${shown(range[0])} → ${shown(range[1])}`));
                b.dataset.grade = grade;
                b.disabled = !fits;
                if (!fits) b.title = 'Not enough points left, or too many of that kind.';
                b.addEventListener('click', () => {
                    this.draftGrades = next;
                    changed();
                });
                row.append(b);
            }
            rows.append(row);
        }
        out.push(rows);
        out.push(el('span', {className: 'gift-heading'}, 'SPECIALTY'));
        const specs = el('div', {className: 'build-presets'});
        for (const s of [{id: '', name: 'None'}, ...arr(c, 'specialties').filter(isObject).map(s => ({id: str(s, 'id'), name: str(s, 'name')}))]) {
            const b = el('button', {type: 'button', className: `build-chip${this.draftSpecialty === s.id ? ' chosen' : ''}`, textContent: s.name});
            b.dataset.specialty = s.id;
            b.addEventListener('click', () => {
                this.draftSpecialty = s.id;
                changed();
            });
            specs.append(b);
        }
        out.push(specs, el('p', {className: 'muted small'}, 'Your specialty starts that skill higher.'));
        return out;
    }

    /** The review's lines about strengths and weaknesses. */
    private buildReview(): HTMLElement[] {
        const names = new Map(arr(this.creation, 'attributes').filter(isObject).map(a => [str(a, 'id'), str(a, 'name')]));
        const listed = (grade: string) => Object.entries(this.draftGrades).filter(([, g]) => g === grade).map(([a]) => names.get(a) ?? a).join(', ');
        const spec = arr(this.creation, 'specialties').filter(isObject).find(s => str(s, 'id') === this.draftSpecialty);
        return [el('p', {className: 'muted'}, `Strong: ${listed('strong') || 'none'} · Weak: ${listed('weak') || 'none'} · ` +
            `Specialty: ${spec ? str(spec, 'name') : 'none'}`)];
    }

    /** The Arrival tab (doc 52): the three start towns, the busiest lately preselected; any may be chosen (a first
     *  character too, say to join a friend: the user). */
    private arrivalPanel(changed: () => void): HTMLElement[] {
        const suggested = this.starts.find(t => t.suggested === true);
        const out: HTMLElement[] = [];
        if (this.firstCharacter && suggested)
            out.push(el('p', {className: 'sage'}, `Your first wolf arrives in ${str(suggested, 'name')}: the busiest of the start towns lately, ` +
                'where you are likeliest to meet others. You may choose another, say to join a friend.'));
        else out.push(el('p', {className: 'muted'}, 'Choose where your wolf arrives.'));
        const row = el('div', {className: 'gift-tiers arrival-towns'});
        for (const t of this.starts) {
            const id = str(t, 'id'), wolves = Math.round(num(t, 'wolves'));
            const b = el('button', {type: 'button', className: `gift-card${this.draftStart === id ? ' chosen' : ''}`},
                el('strong', {}, `${str(t, 'name', title(id))}${t.suggested === true ? ' · busiest' : ''}`), el('span', {}, str(t, 'line')),
                el('small', {className: 'gift-progress'}, wolves > 0 ? `Lately: about ${wolves} ${wolves === 1 ? 'wolf' : 'wolves'} about` : 'Lately: quiet'));
            b.dataset.start = id;
            b.addEventListener('click', () => {
                this.draftStart = id;
                changed();
            });
            row.append(b);
        }
        out.push(el('span', {className: 'gift-heading'}, 'WHERE YOU ARRIVE'), row);
        return out;
    }

    /** The Tie tab (doc 52): a story starter linking this wolf to a mentor or a resident: one suggested at random,
     *  Reroll, or any from the list; a first wolf must take one (the user), later ones may take none. */
    private tiePanel(changed: () => void): HTMLElement[] {
        const out: HTMLElement[] = [el('p', {className: 'sage'}, this.tieRequired
            ? 'Your first wolf arrives with a tie: a story that links them to someone here, a mentor if one is free, else someone who lives in town. ' +
              'You are both told, and where to find each other.'
            : 'A tie links your wolf to someone here: a mentor if one is free, else someone who lives in town. Take one, or none.')];
        const current = el('div', {className: 'gift-card chosen tie-current'}, el('strong', {}, this.draftTie ? this.tieLine(this.draftTie) : 'No tie'));
        const reroll = el('button', {type: 'button', className: 'secondary', textContent: 'Reroll'});
        reroll.addEventListener('click', () => {
            this.draftTie = this.randomTie(this.draftTie);
            changed();
        });
        out.push(el('span', {className: 'gift-heading'}, 'YOUR TIE'), current, reroll, el('span', {className: 'gift-heading'}, 'OR PICK ONE'));
        const list = el('div', {className: 'tie-list'});
        for (const t of this.ties) {
            const id = str(t, 'id');
            const b = el('button', {type: 'button', className: `gift-card${this.draftTie === id ? ' chosen' : ''}`}, el('span', {}, str(t, 'line')));
            b.dataset.tie = id;
            b.addEventListener('click', () => {
                this.draftTie = id;
                changed();
            });
            list.append(b);
        }
        if (!this.tieRequired) {
            const none = el('button', {type: 'button', className: `gift-card${this.draftTie ? '' : ' chosen'}`}, el('span', {}, 'No tie'));
            none.dataset.tie = '';
            none.addEventListener('click', () => {
                this.draftTie = '';
                changed();
            });
            list.append(none);
        }
        out.push(list);
        return out;
    }

    private tieLine(id: string): string {
        return str(this.ties.find(t => str(t, 'id') === id), 'line', id);
    }

    private randomTie(not: string): string {
        const pool = this.ties.map(t => str(t, 'id')).filter(id => id && id !== not);
        return pool.length ? pool[Math.floor(Math.random() * pool.length)] : not;
    }

    private startName(id: string): string {
        return str(this.starts.find(t => str(t, 'id') === id), 'name', title(id));
    }

    /** Whether the account hasn't opened a Gift tier yet (doc 49; an older server sends no tiers: all open). */
    private tierLocked(tier: string): boolean {
        const t = obj(this.tiers, tier);
        return !!t && t.open === false;
    }

    /** The review's lines about the Gift. */
    private giftReview(): HTMLElement[] {
        if (this.draftTier === 'normal') return [el('p', {className: 'muted'}, 'Gift: none (Normal)')];
        const f = (Array.isArray(this.gifts?.families) ? (this.gifts!.families as Json[]) : []).find(x => str(x, 'id') === this.draftFamily);
        return [el('p', {className: 'gold'}, `Gift: ${title(this.draftTier)} · ${str(f, 'name', title(this.draftFamily))}`),
            el('p', {className: 'muted'}, str(obj(f, this.draftTier), 'best'))];
    }

    private review() {
        this.draftName = this.draftName.trim();
        if (this.draftName.length < 2 || this.draftName.length > 32)
            return this.setMessage('Choose a character name between 2 and 32 characters. The authority validates the final name.', true);
        if (this.creation && !this.buildFits(this.draftGrades)) {
            this.creatorTab = 'strengths';
            this.show('creator');
            return this.setMessage('That build costs more points than there are (a Gift tier may cost some too).', true);
        }
        if (this.draftTier !== 'normal' && this.tierLocked(this.draftTier)) {
            this.creatorTab = 'gift';
            this.show('creator');
            return this.setMessage(str(obj(this.tiers, this.draftTier), 'message', `${title(this.draftTier)} isn't open to your account yet.`), true);
        }
        if (this.tieRequired && !this.draftTie) {
            this.creatorTab = 'tie';
            this.show('creator');
            return this.setMessage('Your first wolf takes a tie: pick a story starter, or keep the one suggested.', true);
        }
        if (this.draftTier !== 'normal' && !this.draftFamily) {
            this.creatorTab = 'gift';
            this.show('creator');
            return this.setMessage(`A ${title(this.draftTier)} wolf needs a Gift family: choose one of the eight.`, true);
        }
        this.setMessage('Review your choices. Confirming creates a persistent character; it does not enter the world yet.');
        this.show('review');
    }

    private create() {
        if (this.busy || !this.draftAppearance || this.page !== 'review') return;
        const command: Json = {type: 'character_create', name: this.draftName, age: this.draftAge, appearance: this.draftAppearance};
        if (this.draftTier !== 'normal') command.gift = {tier: this.draftTier, family: this.draftFamily};
        if (this.draftStart && this.starts.some(t => str(t, 'id') === this.draftStart)) command.start = this.draftStart;
        if (this.draftTie) command.tie = this.draftTie;
        if (Object.keys(this.draftGrades).length || this.draftSpecialty)     // (A plain wolf sends no build: doc 49.)
            command.build = {grades: {...this.draftGrades}, specialty: this.draftSpecialty};
        // A timeout is an uncertain result, not a new creation: an unchanged draft keeps the same receipt key.
        const fingerprint = JSON.stringify(command);
        if (!this.creationRequestId || this.creationFingerprint !== fingerprint) {
            this.creationRequestId = newCommandId();
            this.creationFingerprint = fingerprint;
        }
        this.setMessage('Creating your character…');
        this.submit({...command, commandId: this.creationRequestId});
    }

    private drawPortrait() {
        const canvas = this.portraitCanvas;
        if (!canvas) return;
        const c = canvas.getContext('2d')!;
        c.clearRect(0, 0, canvas.width, canvas.height);
        const creating = this.page === 'creator' || this.page === 'review';
        const appearance = creating ? this.draftAppearance : obj(this.selected(), 'appearance');
        const age = creating ? this.draftAge : num(this.selected(), 'age', 18);
        const artwork = creating ? undefined : str(this.selected(), 'artwork') || undefined;
        if (!drawPortrait(c, this.portraits, appearance, age, 0, 0, canvas.width, canvas.height, artwork)) {
            const a = readAppearance(appearance);
            if (!a || this.portraits.failed(a.species)) {
                c.fillStyle = '#a6a699';
                c.font = '16px sans-serif';
                c.fillText('Portrait unavailable', 12, 24);
            }
        }
        canvas.dataset.stage = lifeStage(age);
    }
}
