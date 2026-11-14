// The Editor's screens in the main window (Docs/Design/78-hosts-run-their-own-worlds.md, section 2; Phase M4): the
// user's design, "an "editor" button that prompts for an IP address (to a server) and then, if there is a connection, it
// prompts for a username and password, and then it brings them to another menu for Atlas and DM".
//
//   2   "Edit a world": an Editor code, or the world's address; recent worlds
//   3   connecting, and the key check (still setting up; can't connect; first connection; a changed key's red screen)
//   4   sign in (4b: choose your password)
//   5   the Editor menu: DM and ATLAS, the status pill, the owner's Editor accounts with the password asked again
//
// Everything is drawn as text (textContent, never markup): a world's name, its errors and its account names are a
// world's words. The password goes to the shell once and its box is cleared at once; the shell keeps the session.
import {ask, askText} from './dialog.ts';
import {ago, atlasOffer, bannerText, coBuilderNote, connectRefusal, EDITOR_PORT, firstTimeNote, fromWhere, hostOf, isCode, newPasswordProblem,
    lastBackupText, resetHow, ROLE_LINES, settingUp, signInRefusal, signsInWithCode, pillIsWell, statusPill, type AuditBanner, type Connected, type EditorAccount,
    type EditorBridge, type EditorHello, type EditorSession, type RecentWorld} from '../net/editor.ts';

type Screen = 'address' | 'key' | 'sign-in' | 'password' | 'menu';

/** What the Editor's screens need of the front door around them. */
export interface EditorHost {
    redraw(): void;
    message(text: string, error?: boolean): void;
    leave(): void;
    hostAWorld(): void;
    officialHosts(): string[];
    setBusy(busy: boolean): void;
}

function el<K extends keyof HTMLElementTagNameMap>(tag: K, props: Partial<HTMLElementTagNameMap[K]> & {className?: string} = {},
    ...children: Array<Node | string>): HTMLElementTagNameMap[K] {
    const e = document.createElement(tag);
    Object.assign(e, props);
    e.append(...children);
    return e;
}

export class EditorDoor {
    screen: Screen = 'address';
    address = '';                       // The address as typed or filled ("host:port" once connected).
    connected: Connected | null = null;
    refusal: string[] = [];
    username = '';
    viaCode = false;                    // Signing in with the Editor code the shell holds.
    codePassword = false;               // ...which carries the one-time password.
    session: EditorSession | null = null;
    notice = '';
    hello: EditorHello | null = null;
    accounts: EditorAccount[] | null = null;
    failedToday = 0;
    changes = 0;                        // Account changes since the owner's last visit (the sign-in's count).
    banners: AuditBanner[] = [];
    newCode = '';                       // A co-builder's code, shown this once.
    newCodeFor = '';
    recent: RecentWorld[] = [];
    busy = false;
    private pendingTool: '' | 'dm' = '';    // DUNGEON MASTER on the main menu: the tool to open once the Editor menu is reached.
    private waitTimer = 0;
    private statusTimer = 0;
    private shown: Screen | null = null;    // The screen last drawn: the status line is cleared when it changes...
    private carry = false;                  // ...unless the message was meant for the next screen.
    private password: HTMLInputElement[] = [];
    private readonly bridge: EditorBridge;
    private readonly host: EditorHost;

    constructor(bridge: EditorBridge, host: EditorHost) {
        this.bridge = bridge;
        this.host = host;
        bridge.onEnded((world, username) => {
            if (!this.session || this.session.world !== world) return;
            this.session = {...this.session, live: false};
            this.username = username || this.session.username;
            this.notice = 'Your Editor session ended: sign in again from the Editor menu.';
            this.screen = 'sign-in';
            this.redraw();
        });
    }

    /** The status line under the card. `carry`: the message is for the screen about to be drawn (it survives the change). */
    private say(text: string, error = false, carry = false) {
        this.host.message(text, error);
        this.carry = carry;
    }

    // ------------------------------------------------------------------ Moving between screens

    /** EDITOR on the main menu: the menu if a session is open, otherwise screen 2. */
    async open(address = '') {
        this.stopTimers();
        this.refusal = [];
        this.notice = '';
        this.newCode = '';
        if (!this.carry) this.host.message('');
        const state = await this.bridge.state().catch(() => null);
        this.recent = state?.recent ?? [];
        this.session = state?.session ?? null;
        if (address) {
            this.address = address;
            this.screen = 'address';
        } else if (this.session?.live && !this.session.mustChange) {
            this.screen = 'menu';
        } else if (this.session && !this.session.live) {
            // The session ended (the window may still be open): sign in again as the same account to carry on.
            this.address = this.session.address;
            this.username = this.session.username;
            this.notice = 'Your Editor session ended: sign in again from the Editor menu.';
            this.screen = 'sign-in';
        } else {
            this.screen = 'address';
            this.address = '';
        }
        this.redraw();
    }

    /** DUNGEON MASTER on the main menu: the DM tool is the Editor's (doc 78, screen 5), for a world this player hosts or
     *  helps run, after its Editor sign-in. With a session open, the DM tool opens now; otherwise the Editor's screens,
     *  and the DM tool opens once the sign-in reaches the Editor menu. */
    async openDm() {
        this.pendingTool = 'dm';
        await this.open();
        if (this.screen !== 'menu') {
            this.say('The Dungeon Master tool is your world\'s, in the Editor: connect to your world and sign in with its Editor account; ' +
                'the DM tool opens after.', false, true);
            this.redraw();
        }
    }

    /** The Host page's EDITOR on a community row: this computer first, if the world there is that row's world (a host
     *  at home often can't reach their own public address); otherwise screen 2 with the listed host and the Editor port. */
    async openFor(worldId: string, listedAddress: string) {
        await this.open();
        // A world this computer has signed in to before is tried at its own address first (a second world on this
        // computer has its own Editor port); then this computer's usual Editor port.
        const known = this.recent.find(w => w.world === worldId);
        for (const at of [...(known ? [known.address] : []), '127.0.0.1']) {
            this.setBusy(true, 'Looking for the world on this computer…');
            const here = await this.bridge.connect(at);
            this.setBusy(false);
            if (here.ok && here.hello.world === worldId) {
                this.host.message('');
                return this.connectedTo(here);
            }
        }
        this.address = listedAddress ? `${hostOf(listedAddress)}:${EDITOR_PORT}` : '';
        this.screen = 'address';
        this.say(listedAddress ? `If your world's .env sets another RATW_EDITOR_PORT, use that port instead of ${EDITOR_PORT}.` : '');
        this.redraw();
    }

    leave() {
        this.pendingTool = '';
        this.stopTimers();
        this.wipe();
        this.host.message('');
        this.shown = null;
        this.host.leave();
    }

    /** Before the page changes: the password boxes are emptied, the timers stopped. */
    wipe() {
        for (const p of this.password) p.value = '';
        this.password = [];
    }

    private stopTimers() {
        clearTimeout(this.waitTimer);
        clearInterval(this.statusTimer);
        this.waitTimer = this.statusTimer = 0;
    }

    private redraw() {
        this.wipe();
        this.host.redraw();
    }

    private setBusy(busy: boolean, text = '') {
        this.busy = busy;
        this.host.setBusy(busy);
        if (text) this.say(text);
    }

    private button(text: string, action: () => void, kind: 'primary' | 'secondary' | 'danger' = 'secondary'): HTMLButtonElement {
        const b = el('button', {type: 'button', className: kind === 'danger' ? 'secondary danger' : kind, textContent: text});
        b.addEventListener('click', () => { if (!this.busy) action(); });
        return b;
    }

    private passwordBox(placeholder: string, autocomplete: 'current-password' | 'new-password'): HTMLInputElement {
        const p = el('input', {type: 'password', placeholder, autocomplete, maxLength: 1024});
        this.password.push(p);
        return p;
    }

    render(): HTMLElement {
        this.password = [];
        // A message from the screen before doesn't hang on under the next one (R1: the stale status line).
        if (this.screen !== this.shown && !this.carry) this.host.message('');
        this.shown = this.screen;
        this.carry = false;
        const page = this.screen === 'address' ? this.addressScreen() : this.screen === 'key' ? this.keyScreen()
            : this.screen === 'sign-in' ? this.signInScreen() : this.screen === 'password' ? this.passwordScreen() : this.menuScreen();
        page.dataset.editor = this.screen;
        return page;
    }

    /** The world's name and address, drawn by the client (so the host knows where the password is going). */
    private where(): HTMLElement {
        const name = this.session?.name || this.connected?.hello.name || 'Your world';
        const address = this.session?.address || this.connected?.address || this.address;
        return el('div', {className: 'editor-where'}, el('h2', {className: 'name'}, name), el('p', {className: 'muted'}, address));
    }

    // ------------------------------------------------------------------ Screen 2: the address, or an Editor code

    private addressScreen(): HTMLElement {
        const code = el('input', {type: 'text', placeholder: 'ratw-editor:…', maxLength: 1024, autocomplete: 'off', spellcheck: false});
        code.name = 'editorCode';
        const codeForm = el('form', {className: 'editor-code'},
            el('label', {}, el('span', {}, 'HAVE AN EDITOR CODE? PASTE IT'), code),
            el('button', {type: 'submit', className: 'primary', textContent: 'USE THE CODE'}));
        codeForm.addEventListener('submit', e => {
            e.preventDefault();
            void this.useCode(code.value);
            code.value = '';
        });
        const address = el('input', {type: 'text', placeholder: 'e.g. 127.0.0.1, 192.168.1.20 or myworld.example.net:17789', maxLength: 260,
            value: this.address, spellcheck: false});
        address.name = 'editorAddress';
        const form = el('form', {className: 'world-direct editor-address'},
            el('label', {}, el('span', {}, 'YOUR WORLD\'S ADDRESS'), address),
            el('button', {type: 'submit', className: 'primary', textContent: 'CONNECT'}));
        form.addEventListener('submit', e => {
            e.preventDefault();
            // A code pasted into the address box is a code.
            if (isCode(address.value)) {
                void this.useCode(address.value);
                address.value = '';
            } else void this.connect(address.value);
        });
        const recent = el('div', {className: 'world-list'});
        for (const w of this.recent) {
            const use = this.button('CONNECT', () => void this.connect(w.address), 'primary');
            use.dataset.address = w.address;
            const forget = this.button('FORGET', () => void this.forgetRecent(w));
            recent.append(el('div', {className: 'world-row'},
                el('div', {className: 'world-text'},
                    el('div', {className: 'world-title'}, el('strong', {}, w.name || 'A world')),
                    el('p', {className: 'muted small'}, `${w.address}${w.username ? `  ·  ${w.username}` : ''}`)),
                el('div', {className: 'host-buttons'}, forget, use)));
        }
        const problem = this.refusal.length ? el('div', {className: 'editor-refusal'}, ...this.refusal.map(line => el('p', {className: 'gold'}, line)))
            : el('div');
        const card = el('section', {className: 'door-card door-form editor-card'},
            el('p', {className: 'gold small'}, 'EDIT A WORLD'),
            codeForm,
            el('p', {className: 'muted small'}, 'The code is in manage/first-editor.txt in your world folder, or the world\'s owner gives you one.'),
            form,
            el('p', {className: 'muted small'}, 'On the computer that runs your world, use 127.0.0.1.'),
            problem,
            ...(this.recent.length ? [el('p', {className: 'gold small'}, 'RECENT WORLDS'), el('div', {className: 'scroll'}, recent)] : [el('div', {className: 'grow'})]),
            el('div', {className: 'door-actions'}, this.button('Back to the main menu', () => this.leave()),
                this.button('Don\'t have a world yet? HOST A WORLD', () => { this.stopTimers(); this.host.hostAWorld(); })));
        const side = el('section', {className: 'door-card door-preview'},
            el('p', {className: 'gold small'}, 'THE EDITOR'),
            el('h2', {}, 'Run or build your own world'),
            el('p', {className: 'muted large'}, 'Your world\'s Dungeon Master tool and its map editor, for a world you host: on this computer, ' +
                'at home, or on a server you rent.'),
            el('p', {className: 'muted'}, 'You sign in with your world\'s Editor account, not your RATW account. RATW never asks for your RATW ' +
                'password here or in the Editor window.'));
        return el('div', {className: 'door-row'}, side, card);
    }

    private async useCode(text: string) {
        if (this.busy) return;
        if (!text.trim()) return this.say('Paste the whole Editor code, from ratw-editor: to its end.', true);
        const r = await this.bridge.useCode(text.trim());
        if (!r.ok) {
            this.refusal = connectRefusal({...r, error: 'code'});
            return this.redraw();
        }
        this.viaCode = true;
        this.codePassword = r.password;
        this.username = r.username;
        if (r.address) return this.connect(r.address);
        this.refusal = [];
        this.address = '';
        this.say('The code is ready: now type your world\'s address (the owner tells you it; 127.0.0.1 on the computer that runs it, with the ' +
            'port if it isn\'t 17789).');
        this.redraw();
    }

    private async forgetRecent(w: RecentWorld) {
        if (!await ask(`Forget ${w.name || w.address}? This computer will ask you to check its certificate again next time.`, {ok: 'Forget it'})) return;
        await this.bridge.forgetRecent(w.pin);
        await this.open();
    }

    async connect(given: string) {
        if (this.busy) return;
        const address = given.trim();
        if (!address) return this.say('Type your world\'s address.', true);
        // An address central lists as an official world: refused before connecting at all.
        if (this.host.officialHosts().includes(hostOf(address))) {
            this.stopTimers();
            this.refusal = ['Official worlds can\'t be edited.'];
            this.screen = 'address';                   // (Also from the setting-up screen's recheck: the refusal is shown there.)
            return this.redraw();
        }
        this.address = address;
        this.refusal = [];
        this.setBusy(true, `Connecting to ${address}…`);
        const r = await this.bridge.connect(address);
        this.setBusy(false);
        this.host.message('');
        if (!r.ok) {
            this.refusal = connectRefusal(r);
            this.screen = 'address';
            return this.redraw();
        }
        this.connectedTo(r);
    }

    private connectedTo(r: Connected) {
        this.connected = r;
        this.address = r.address;
        if (!this.username) this.username = r.username;
        if (r.code) this.viaCode = true;
        if (r.state === 'changed') {
            this.screen = 'key';
            return this.redraw();
        }
        if (r.hello.progress.code !== 'ready') {
            this.screen = 'key';
            this.waitTimer = window.setTimeout(() => { if (this.screen === 'key') void this.connect(r.address); }, 5000);
            return this.redraw();
        }
        if (r.state === 'new') {
            this.screen = 'key';
            return this.redraw();
        }
        this.notice = r.state === 'moved' ? `${r.hello.name || 'Your world'}, now at ${r.address}.`
            : r.state === 'rotated' ? `${r.hello.name || 'Your world'} has a new certificate, signed by its old one.` : '';
        this.screen = 'sign-in';
        this.redraw();
    }

    // ------------------------------------------------------------------ Screen 3: the key check

    private keyScreen(): HTMLElement {
        const c = this.connected!;
        const card = el('section', {className: 'door-card door-form editor-card'});
        const back = this.button('Cancel', () => { this.stopTimers(); this.screen = 'address'; this.redraw(); });
        if (c.state === 'changed') {
            const typed = el('input', {type: 'text', placeholder: 'The world\'s name, as this computer knew it', maxLength: 64});
            typed.name = 'worldName';
            const forget = this.button('FORGET THE OLD CERTIFICATE', () => void this.forget(typed.value), 'danger');
            card.classList.add('editor-red');
            card.append(
                el('p', {className: 'gold small'}, 'STOP: THE CERTIFICATE HAS CHANGED'),
                el('h2', {}, 'Don\'t sign in.'),
                el('p', {className: 'large'}, 'This world\'s certificate has changed since you last connected. Someone may be intercepting your ' +
                    'connection. Don\'t sign in.'),
                el('p', {className: 'muted'}, 'What this computer trusted:'), el('pre', {className: 'editor-print'}, c.previous ?? 'unknown'),
                el('p', {className: 'muted'}, `What ${c.address} shows now:`), el('pre', {className: 'editor-print'}, c.fingerprint),
                el('p', {className: 'muted'}, 'Only if you know why (you started this world afresh, lost its storage, or ran ratw-editor new-certificate ' +
                    '--leaked yourself): compare the new fingerprint with manage/editor-certificate.txt in the world folder, then type the world\'s name and forget the old ' +
                    'certificate. A new Editor code from the world\'s owner also clears this.'),
                el('label', {}, el('span', {}, 'THE WORLD\'S NAME'), typed),
                el('div', {className: 'door-actions'}, back, forget));
            return el('div', {className: 'door-row'}, card);
        }
        if (c.hello.progress.code !== 'ready') {
            const slow = c.hello.progress.code === 'slow';
            const bar = el('progress', {className: 'editor-progress', max: 100, value: Math.max(2, Math.min(100, Math.round(c.hello.progress.percent)))});
            card.append(this.where(), el('p', {className: 'large'}, settingUp(c.hello)),
                ...(slow ? [el('p', {className: 'muted'}, 'This page asks again every few seconds.')] : [bar,
                    el('p', {className: 'muted'}, 'On a first start the world imports its map and sets up its database: that takes a few minutes. ' +
                        'This page asks again every few seconds and goes on by itself.')]),
                el('div', {className: 'grow'}), el('div', {className: 'door-actions'}, back));
            return el('div', {className: 'door-row'}, card);
        }
        const local = c.place === 'loopback';
        const trust = this.button(c.place === 'public' ? 'I COMPARED IT' : 'TRUST AND CONTINUE', () => void this.trust(), 'primary');
        trust.dataset.action = 'editor-trust';
        card.append(
            el('p', {className: 'gold small'}, 'FIRST CONNECTION'),
            el('h2', {}, c.hello.name || 'A world'),
            el('p', {className: 'large'}, `First connection to ${c.hello.name || 'this world'} at ${c.address}. Its certificate fingerprint is:`),
            el('pre', {className: 'editor-print'}, c.fingerprint),
            el('p', {}, 'Compare it with the one in your world folder, manage/editor-certificate.txt. If they match, trust it.'),
            ...(local ? [el('p', {className: 'sage'}, 'This is the computer you\'re using: comparing is up to you.')] : []),
            el('p', {className: 'muted small'}, 'Using an Editor code skips this.'),
            el('div', {className: 'grow'}),
            el('div', {className: 'door-actions'}, back, trust));
        return el('div', {className: 'door-row'}, card);
    }

    private async trust() {
        const c = this.connected!;
        const r = await this.bridge.trust(c.address, c.fingerprint);
        if (!r.ok) return this.connect(c.address);
        this.screen = 'sign-in';
        this.redraw();
    }

    private async forget(typed: string) {
        const c = this.connected!;
        if (!typed.trim()) return this.say('Type the world\'s name to forget its old certificate.', true);
        const r = await this.bridge.forget(c.address, typed);
        if (!r.ok) return this.say(r.error === 'name' ? 'That isn\'t the name this computer knew the world by.' : signInRefusal(r), true);
        this.say('The old certificate is forgotten. Compare the new one before you trust it.', false, true);
        await this.connect(c.address);
    }

    // ------------------------------------------------------------------ Screen 4: sign in

    private signInScreen(): HTMLElement {
        const user = el('input', {type: 'text', autocomplete: 'username', maxLength: 32, value: this.username, spellcheck: false});
        user.name = 'editorUsername';
        const fromCode = signsInWithCode(this.viaCode, this.codePassword, this.session);
        const pass = this.passwordBox(fromCode ? 'From your Editor code' : 'Your Editor password', 'current-password');
        pass.name = 'editorPassword';
        const form = el('form', {className: 'door-login editor-sign-in'},
            el('label', {}, el('span', {}, 'EDITOR ACCOUNT'), user),
            el('label', {}, el('span', {}, 'PASSWORD'), pass),
            el('button', {type: 'submit', className: 'primary', textContent: 'SIGN IN'}));
        form.addEventListener('submit', e => {
            e.preventDefault();
            const secret = pass.value;
            pass.value = '';
            void this.signIn(user.value, secret, fromCode);
        });
        const pending = this.connected?.hello.setupPending;
        const card = el('section', {className: 'door-card door-form editor-card'},
            this.where(),
            ...(this.notice ? [el('p', {className: 'sage editor-notice'}, this.notice)] : []),
            el('p', {className: 'muted'}, 'This is your world\'s Editor account, not your RATW account. Use a password only for this world. RATW ' +
                'never asks for your RATW password here or in the Editor window.'),
            ...(firstTimeNote(!!pending, fromCode, this.username) ? [el('p', {className: 'gold'}, 'First time? Your username and one-time ' +
                'password are in manage/first-editor.txt in your world folder. Pasting the Editor code from that file is easiest.')] : []),
            ...(fromCode ? [el('p', {className: 'sage'}, 'Your Editor code carries the one-time password: just sign in.')] : []),
            form,
            el('p', {className: 'muted small'}, `Lost your password? Ask the world's owner for a new Editor code; or, on the world's computer, ${resetHow(this.username)}.`),
            el('div', {className: 'grow'}),
            el('div', {className: 'door-actions'}, this.button('Back', () => { this.screen = 'address'; this.notice = ''; this.redraw(); })));
        return el('div', {className: 'door-row'}, card);
    }

    private async signIn(username: string, password: string, fromCode: boolean) {
        if (this.busy) return;
        const name = username.trim().toLowerCase();
        if (!name) return this.say('Type your Editor account\'s name.', true);
        if (!password && !fromCode) return this.say('Type your password.', true);
        this.username = name;
        this.setBusy(true, 'Signing in…');
        const r = await this.bridge.signIn(this.address, name, password, this.viaCode);
        this.setBusy(false);
        if (!r.ok) {
            // A code that didn't fit this account: sign in by typing instead.
            if (r.error === 'code') this.viaCode = false;
            this.say(signInRefusal(r), true);
            return this.redraw();
        }
        this.viaCode = false;
        this.codePassword = false;
        this.session = r.session;
        this.failedToday = r.notices?.failedToday ?? 0;
        this.changes = r.notices?.changes ?? 0;
        this.notice = '';
        this.host.message('');
        this.screen = r.session.mustChange ? 'password' : 'menu';
        this.redraw();
    }

    // ------------------------------------------------------------------ Screen 4b: choose your password

    private passwordScreen(): HTMLElement {
        const first = !!this.session?.mustChange;
        const old = first ? null : this.passwordBox('Your password now', 'current-password');
        const fresh = this.passwordBox('At least 12 characters', 'new-password');
        const again = this.passwordBox('The same again', 'new-password');
        const form = el('form', {className: 'door-login editor-password'},
            ...(old ? [el('label', {}, el('span', {}, 'YOUR PASSWORD NOW'), old)] : []),
            el('label', {}, el('span', {}, 'NEW PASSWORD'), fresh),
            el('label', {}, el('span', {}, 'NEW PASSWORD AGAIN'), again),
            el('button', {type: 'submit', className: 'primary', textContent: 'CHOOSE THIS PASSWORD'}));
        form.addEventListener('submit', e => {
            e.preventDefault();
            const values = [old?.value ?? '', fresh.value, again.value];
            for (const p of [old, fresh, again]) if (p) p.value = '';
            void this.choose(values[0], values[1], values[2]);
        });
        const card = el('section', {className: 'door-card door-form editor-card'},
            this.where(),
            el('p', {className: 'gold small'}, first ? 'CHOOSE YOUR PASSWORD' : 'CHANGE YOUR PASSWORD'),
            el('p', {className: 'muted'}, 'At least 12 characters, without your account\'s name or the world\'s name, and not one of the passwords most ' +
                'often found in breaches. Use it only for this world.'),
            ...(first ? [el('p', {className: 'muted'}, 'Your one-time password stops working as soon as you choose your own.')] : []),
            form,
            el('div', {className: 'grow'}),
            el('div', {className: 'door-actions'}, first ? this.button('Sign out', () => void this.signOut())
                : this.button('Back', () => { this.screen = 'menu'; this.redraw(); })));
        return el('div', {className: 'door-row'}, card);
    }

    private async choose(old: string, fresh: string, again: string) {
        if (this.busy || !this.session) return;
        const problem = newPasswordProblem(fresh, again, this.session.username, this.session.name);
        if (problem) return this.say(problem, true);
        if (!this.session.mustChange && !old) return this.say('Type your password now, then the new one twice.', true);
        this.setBusy(true, 'Saving your password…');
        const r = await this.bridge.changePassword(old, fresh);
        this.setBusy(false);
        if (!r.ok) {
            const ended = r.error === 'ended' || r.error === 'signed-out';
            this.say(signInRefusal(r), true, ended);
            if (ended) this.screen = 'sign-in';
            return this.redraw();
        }
        const first = this.session.mustChange;
        this.session = r.session;
        this.say(first ? 'Your password is saved.' : 'Your password is saved. Your other sessions of this account have ended.', false, true);
        this.screen = 'menu';
        this.redraw();
    }

    // ------------------------------------------------------------------ Screen 5: the Editor menu

    private menuScreen(): HTMLElement {
        const s = this.session!;
        if (this.pendingTool) {
            const tool = this.pendingTool;
            this.pendingTool = '';
            queueMicrotask(() => void this.openTool(tool));
        }
        if (!this.statusTimer) {
            void this.refreshStatus();
            this.statusTimer = window.setInterval(() => { if (this.screen === 'menu') void this.refreshStatus(); }, 15000);
        }
        const pill = el('span', {className: 'world-kind editor-pill'}, statusPill(this.hello));
        pill.classList.toggle('down', !pillIsWell(pill.textContent ?? ''));
        pill.dataset.pill = '1';
        const backup = el('p', {className: 'muted small'}, lastBackupText(this.hello));
        backup.dataset.lastBackup = '1';
        const dm = this.button('DM', () => void this.openTool('dm'), 'primary');
        dm.dataset.action = 'editor-dm';
        const offer = atlasOffer(s.role);
        const atlas = this.button('ATLAS', () => void this.openTool('atlas'), 'primary');
        atlas.dataset.action = 'editor-atlas';
        atlas.disabled = !offer.enabled;
        const atlasWhy = offer.why;
        const tools = el('div', {className: 'editor-tools'},
            el('div', {className: 'editor-tool'}, dm, el('p', {className: 'muted'}, 'The Dungeon Master tool: NPCs, events, the economy, players and their decisions.')),
            el('div', {className: 'editor-tool'}, atlas, el('p', {className: 'muted'}, atlasWhy)));
        const facts = el('p', {className: 'muted small'}, `Signed in as ${s.username} (${s.role})  ·  Certificate checked  ·  ${s.fingerprint}`);
        const owner = s.role === 'owner';
        const link = (text: string, action: string, run: () => void) => {
            const b = this.button(text, run);
            b.dataset.action = action;
            return b;
        };
        const links = el('div', {className: 'door-actions'},
            ...(owner ? [link('EDITOR ACCOUNTS', 'editor-accounts', () => void this.openAccounts())] : []),
            link('Change my password', 'editor-change-password', () => { this.screen = 'password'; this.redraw(); }),
            link('Sign out', 'editor-sign-out', () => void this.signOut()),
            link('Back to the main menu', 'editor-back', () => this.leave()));
        const card = el('section', {className: `door-card door-form editor-card editor-menu${this.accounts ? ' with-accounts' : ''}`},
            el('div', {className: 'world-title'}, el('strong', {}, s.name || 'Your world'), pill),
            el('p', {className: 'muted'}, s.address),
            backup,
            facts,
            ...(this.notice ? [el('p', {className: 'gold editor-notice'}, this.notice)] : []),
            ...(owner && this.failedToday ? [el('p', {className: 'gold'}, `${this.failedToday} failed sign-in${this.failedToday === 1 ? '' : 's'} today`)] : []),
            ...(owner && this.changes && !this.accounts ? [el('p', {className: 'sage editor-banner'},
                `${this.changes} Editor account change${this.changes === 1 ? '' : 's'} since your last visit: EDITOR ACCOUNTS shows ${this.changes === 1 ? 'it' : 'them'}.`)] : []),
            ...(owner ? this.banners.slice(0, 8).map(b => el('p', {className: 'sage editor-banner'}, `${bannerText(b)} · ${ago(b.at)}`)) : []),
            tools,
            ...(this.accounts ? [this.accountsPanel()] : [el('div', {className: 'grow'})]),
            links);
        return el('div', {className: 'door-row'}, card);
    }

    private async refreshStatus() {
        const r = await this.bridge.status();
        this.hello = r.ok ? r.hello : null;
        const pill = document.querySelector<HTMLElement>('[data-pill]');
        if (pill) {
            pill.textContent = statusPill(this.hello);
            pill.classList.toggle('down', !pillIsWell(pill.textContent));
        }
        const backup = document.querySelector<HTMLElement>('[data-last-backup]');
        if (backup) backup.textContent = lastBackupText(this.hello);
    }

    private async openTool(suite: 'dm' | 'atlas') {
        this.setBusy(true, 'Opening the Editor window…');
        const r = await this.bridge.open(suite);
        this.setBusy(false);
        if (r.ok) return this.say('The Editor window is open. It keeps working while you play.');
        this.say(signInRefusal(r), true, r.error === 'ended');
        if (r.error === 'ended') {
            this.screen = 'sign-in';
            this.redraw();
        }
    }

    private async signOut() {
        this.stopTimers();
        await this.bridge.signOut();
        this.session = null;
        this.accounts = null;
        this.banners = [];
        this.failedToday = this.changes = 0;
        this.say('Signed out of the Editor.', false, true);
        await this.open();
    }

    // ------------------------------------------------------------------ Editor accounts (owner), the password asked again

    /** Every account change asks for the owner's password again, here in the client's own screen (never the Editor
     *  window): a step-up of 5 minutes, then the change. */
    private async stepUp(why: string): Promise<boolean> {
        const password = await askText(`${why}\nType your Editor password to go on.`, {password: true, ok: 'Go on', placeholder: 'Your Editor password'});
        if (!password) return false;
        this.setBusy(true, 'Checking your password…');
        const r = await this.bridge.stepUp(password);
        this.setBusy(false);
        if (!r.ok) {
            this.say(signInRefusal(r), true, r.error === 'ended');
            if (r.error === 'ended') { this.screen = 'sign-in'; this.redraw(); }
            return false;
        }
        this.say('');                   // ("Checking your password…" no longer true: M6's run saw it stay under the list.)
        return true;
    }

    private async openAccounts() {
        if (!await this.stepUp('Editor accounts.')) return;
        await this.loadAccounts();
    }

    private async loadAccounts() {
        const r = await this.bridge.accounts('list');
        if (!r.ok) {
            this.say(signInRefusal(r), true);
            return;
        }
        this.accounts = r.accounts ?? [];
        this.failedToday = r.failedToday ?? 0;
        this.banners = r.banners ?? [];
        this.changes = 0;
        this.redraw();
    }

    private async change(verb: string, body: Record<string, unknown>, why: string) {
        if (!await this.stepUp(why)) return;
        this.setBusy(true, 'Saving…');
        const r = await this.bridge.accounts(verb, body);
        this.setBusy(false);
        if (!r.ok) {
            this.say(signInRefusal(r), true);
            return this.loadAccounts();
        }
        if (r.code) {
            this.newCode = r.code;
            this.newCodeFor = String(body.username ?? '');
        }
        this.say('Done.');
        await this.loadAccounts();
    }

    private accountsPanel(): HTMLElement {
        const list = el('div', {className: 'world-list'});
        const owners = (this.accounts ?? []).filter(a => a.role === 'owner' && !a.disabled).length;
        for (const a of this.accounts ?? []) {
            const state = a.disabled ? 'turned off' : a.unclaimed ? 'not yet claimed' : a.mustChange ? 'on a one-time password'
                : `last sign-in ${ago(a.lastLogin)}${a.lastFrom ? ` from ${fromWhere(a.lastFrom)}` : ''}`;
            const buttons: HTMLElement[] = [];
            if (a.role !== 'owner') {
                const role = el('select', {});
                role.append(...['viewer', 'dm', 'builder'].map(r => el('option', {value: r, textContent: r, selected: r === a.role})));
                role.addEventListener('change', () => void this.change('role', {username: a.username, role: role.value}, `Make ${a.username} a ${role.value}?`));
                const wolves = el('input', {type: 'checkbox', checked: a.mayActOnWolves});
                wolves.addEventListener('change', () => void this.change('wolves', {username: a.username, allowed: wolves.checked},
                    wolves.checked ? `Let ${a.username} kill, injure, move or bring back players' wolves?` : `Stop ${a.username} acting on players' wolves?`));
                buttons.push(role, el('label', {className: 'door-check editor-wolves'}, wolves, el('span', {}, 'MAY ACT ON PLAYERS\' WOLVES')),
                    this.button('NEW CODE', () => void this.change('reset', {username: a.username},
                        `Give ${a.username} a new Editor code? Their password, sessions and devices stop working.`)));
            }
            if (a.role === 'owner' && !a.disabled && owners <= 1) {
                // The last owner can't be turned off or removed: no buttons that would only be refused after the password.
                buttons.push(el('span', {className: 'muted small'}, 'The world\'s only owner: it can\'t be turned off or removed.'));
            } else {
                buttons.push(this.button(a.disabled ? 'TURN ON' : 'TURN OFF', () => void this.change(a.disabled ? 'enable' : 'disable', {username: a.username},
                    a.disabled ? `Turn ${a.username} on again?` : `Turn ${a.username} off? Their sessions end at once.`)),
                this.button('REMOVE', () => void this.change('remove', {username: a.username}, `Remove ${a.username} for good?`), 'danger'));
            }
            const row = el('div', {className: 'world-row editor-account'},
                el('div', {className: 'world-text'},
                    el('div', {className: 'world-title'}, el('strong', {}, a.username), el('span', {className: 'world-kind'}, a.role.toUpperCase())),
                    el('p', {className: 'muted small'}, `${state}  ·  ${a.devices.length} device${a.devices.length === 1 ? '' : 's'}` +
                        (a.claimedFrom ? `  ·  claimed from ${fromWhere(a.claimedFrom)}` : ''))),
                el('div', {className: 'host-buttons'}, ...buttons));
            row.dataset.account = a.username;
            list.append(row);
        }
        const name = el('input', {type: 'text', placeholder: 'e.g. pell', maxLength: 32, spellcheck: false});
        name.name = 'coBuilder';
        const role = el('select', {});
        role.append(...['dm', 'builder', 'viewer'].map(r => el('option', {value: r, textContent: r})));
        const line = el('p', {className: 'muted small'}, ROLE_LINES.dm);
        role.addEventListener('change', () => { line.textContent = ROLE_LINES[role.value] ?? ''; });
        const wolves = el('input', {type: 'checkbox'});
        const add = el('form', {className: 'editor-add'},
            el('label', {}, el('span', {}, 'ADD A CO-BUILDER'), name), el('label', {}, el('span', {}, 'ROLE'), role),
            el('label', {className: 'door-check'}, wolves, el('span', {}, 'MAY ACT ON PLAYERS\' WOLVES')),
            el('button', {type: 'submit', className: 'primary', textContent: 'ADD'}));
        add.addEventListener('submit', e => {
            e.preventDefault();
            const who = name.value.trim().toLowerCase();
            if (!who) return this.say('Give the co-builder an account name.', true);
            void this.change('add', {username: who, role: role.value, mayActOnWolves: wolves.checked},
                `Add ${who} with the role ${role.value}? They need the RATW desktop client, and your Editor port open to them (RATW_EDITOR_BIND=0.0.0.0 in .env, ` +
                'and from outside your home a forwarded port).');
        });
        const codeBox: HTMLElement[] = [];
        if (this.newCode) {
            const code = el('input', {type: 'text', readOnly: true, value: this.newCode, className: 'host-token'});
            code.addEventListener('focus', () => code.select());
            codeBox.push(el('div', {className: 'host-token-box'},
                el('p', {className: 'gold small'}, `THE EDITOR CODE FOR ${this.newCodeFor.toUpperCase()} · SHOWN THIS ONCE`),
                el('div', {className: 'host-token-row'}, code, this.button('COPY', () => void this.copy(this.newCode, code))),
                el('p', {className: 'muted small'}, 'Pass it on privately: it works once, for 72 hours. They paste it into EDITOR and choose their own password.'),
                ...(this.session ? [el('p', {className: 'gold small'}, coBuilderNote(this.session.place, this.session.address))] : [])));
        }
        const end = this.button('END ALL EDITOR SESSIONS', () => void this.change('end-sessions', {},
            'End every Editor session, yours too, and forget every device?'), 'danger');
        return el('div', {className: 'editor-accounts'},
            el('p', {className: 'gold small'}, 'EDITOR ACCOUNTS'), ...codeBox,
            list, add, line,
            el('div', {className: 'door-actions'}, end, this.button('Close', () => { this.accounts = null; this.newCode = ''; this.redraw(); })));
    }

    private async copy(text: string, box: HTMLInputElement) {
        try {
            await navigator.clipboard.writeText(text);
            this.say('The code is copied.');
        } catch {
            box.focus();
            box.select();
            this.say('Select the code and press Ctrl+C.', true);
        }
    }
}
