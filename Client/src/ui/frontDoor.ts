// The front door: signing in, the roster of up to six characters, and the creator (UI/SRatwFrontDoor.cpp), as
// ordinary HTML forms. Account lobby only: credentials are never kept anywhere, and the password box is cleared as soon
// as it has been sent.
import {CoatColors, CoatNames} from './theme.ts';
import {drawPortrait, lifeStage, Portraits, readAppearance, shoulderHeightCm} from './portrait.ts';
import {isObject, num, obj, str, type Json} from '../game/json.ts';
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
                el('span', {}, c ? `Age ${age.toFixed(0)} · ${stageName(age)}` : `Empty slot ${i + 1} of 6`));
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
            el('p', {className: 'muted large'}, c ? `${stageName(age)} · age ${age.toFixed(0)} · ${title(str(obj(c, 'appearance'), 'species', 'timber'))} wolf`
                : 'Choose an empty slot to make your first wolf.'),
            canvas,
            el('p', {className: 'muted large'}, 'The portrait belongs to the sheet. On the map, your presence stays W> — simple, expressive, and yours to imagine.'),
            enter);
        return el('div', {className: 'door-row'}, slots, detail);
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
        this.creationRequestId = this.creationFingerprint = '';
        this.draftAppearance = {species: 'timber', sex: 'female', stature: 'average', pattern: 'solid', baseColor: 2, gradientColor: 0,
            markingColor: 5, gradientAmount: 0.35, patternAmount: 0.65};
        this.setMessage('Appearance changes are a live preview. Nothing is saved until you confirm creation.');
        this.show('creator');
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
            `${a ? shoulderHeightCm(a, this.draftAge).toFixed(0) : '0'} cm at shoulder`;
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
        if (!reviewOnly) {
            const name = el('input', {type: 'text', value: this.draftName, placeholder: 'The name others will know', maxLength: 64});
            name.addEventListener('input', () => {
                this.draftName = name.value.slice(0, 64);
                nameHeading.textContent = this.draftName || 'Your wolf';
            });
            const choice = (field: string, caption: string) => {
                const select = el('select', {});
                for (const option of Choices[field]) select.append(el('option', {value: option, textContent: title(option)}));
                select.value = str(draft, field);
                select.addEventListener('change', () => {
                    draft[field] = select.value;
                    refresh();
                });
                return el('label', {}, el('span', {}, caption), select);
            };
            const age = el('input', {type: 'number', min: '6', max: '99', value: String(this.draftAge)});
            age.addEventListener('input', () => {
                const v = Math.trunc(Number(age.value));
                if (Number.isFinite(v)) {
                    this.draftAge = Math.min(99, Math.max(6, v));
                    refresh();
                }
            });
            const palette = (field: string, caption: string) => {
                const row = el('div', {className: 'palette'});
                CoatColors.forEach((hex, i) => {
                    const b = el('button', {type: 'button', title: `${caption}: ${CoatNames[i]}`,
                        textContent: `${num(draft, field) === i ? '✓ ' : ''}${CoatNames[i]}`});
                    b.style.background = `#${hex.toString(16).padStart(6, '0')}`;
                    b.style.color = i === 4 || i === 5 ? '#fff' : '#000';
                    b.addEventListener('click', () => {
                        draft[field] = i;
                        for (const [j, other] of [...row.children].entries()) other.textContent = `${j === i ? '✓ ' : ''}${CoatNames[j]}`;
                        refresh();
                    });
                    row.append(b);
                });
                return el('div', {className: 'field'}, el('span', {}, caption), row);
            };
            const amount = (field: string, caption: string) => {
                const label = el('span', {}, `${caption} · ${Math.round(num(draft, field) * 100)}%`);
                const slider = el('input', {type: 'range', min: '0', max: '1', step: '0.05', value: String(num(draft, field))});
                slider.addEventListener('input', () => {
                    draft[field] = Number(slider.value);
                    label.textContent = `${caption} · ${Math.round(Number(slider.value) * 100)}%`;
                    refresh();
                });
                return el('label', {}, label, slider);
            };
            fields.append(
                el('label', {}, el('span', {}, 'CHARACTER NAME'), name),
                el('div', {className: 'pair'}, choice('species', 'SPECIES'), choice('sex', 'SEX')),
                el('div', {className: 'pair'}, el('label', {}, el('span', {}, 'STARTING AGE · 6–99'), age), choice('stature', 'STATURE · APPEARANCE ONLY')),
                palette('baseColor', 'BASE COAT'), palette('gradientColor', 'GRADIENT COLOR'), amount('gradientAmount', 'GRADIENT STRENGTH'),
                choice('pattern', 'MARKING PATTERN'), palette('markingColor', 'MARKING COLOR'), amount('patternAmount', 'MARKING STRENGTH'));
        } else {
            const colour = (field: string) => CoatNames[Math.min(7, Math.max(0, Math.trunc(num(draft, field))))];
            fields.append(
                el('h2', {}, 'Review your wolf'),
                el('p', {className: 'gold name'}, this.draftName),
                el('p', {className: 'large pre'}, `${title(str(draft, 'species'))} wolf · ${title(str(draft, 'sex'))}\nAge ${this.draftAge} · ` +
                    `${stageName(this.draftAge)}\n${title(str(draft, 'stature'))} stature · ${title(str(draft, 'pattern'))} markings`),
                el('p', {className: 'muted'}, `Base coat: ${colour('baseColor')}`),
                el('p', {className: 'muted'}, `Gradient: ${colour('gradientColor')}`),
                el('p', {className: 'muted'}, `Markings: ${colour('markingColor')}`),
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

    private review() {
        this.draftName = this.draftName.trim();
        if (this.draftName.length < 2 || this.draftName.length > 32)
            return this.setMessage('Choose a character name between 2 and 32 characters. The authority validates the final name.', true);
        this.setMessage('Review your choices. Confirming creates a persistent character; it does not enter the world yet.');
        this.show('review');
    }

    private create() {
        if (this.busy || !this.draftAppearance || this.page !== 'review') return;
        const command: Json = {type: 'character_create', name: this.draftName, age: this.draftAge, appearance: this.draftAppearance};
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
        if (!drawPortrait(c, this.portraits, appearance, age, 0, 0, canvas.width, canvas.height)) {
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
