// The front door: the title screen (signing in), the main menu, the servers, the Character Editor (the roster of up to
// six wolves and the creator) and a world's first arrival, as ordinary HTML forms (UI/SRatwFrontDoor.cpp, redesigned
// from the user's notes of 2026-11-14). Account lobby only: credentials are never kept anywhere, and the password box is
// cleared as soon as it has been sent.
//
// Two modes. 'world' (a browser on a password-mode world, as always): the title screen with the world's own accounts,
// then its roster, its creator and ENTER THE WORLD. 'central' (the desktop client, Docs/Design/77-central-accounts-and-
// hosted-worlds.md, 3.1): the title screen signs in to central; then the MAIN MENU: SERVERS (central's worlds in a
// server browser; a world chosen, then the wolf to play there), CHARACTER EDITOR (the wolves central keeps: their
// identity, and their strengths and tie, which each world takes at the first arrival), EDITOR and DUNGEON MASTER (the
// Editor's screens, editorDoor.ts, Docs/Design/78: a host's way into their own world's tools; the DM tool opens from
// there). A host's account also has the Host page (3.2). In central mode its commands go to the client (client.ts),
// which answers with lobby events of the same shape.
import {lifeStage, Portraits, readAppearance, shoulderHeightCm, drawPortrait} from './portrait.ts';
import {buildOf, BuildNames, Builds, BuildStature, EyeColours, eyeName, FurColours, furHex, furName, furOf, FurZones, olderLook, ReadyCoats, zoneName,
    type Build, type Fur, type FurCoat, type ReadyCoat} from './fur.ts';
import {artCache, sendPortrait, squarePixels} from './artwork.ts';
import {ask, askText} from './dialog.ts';
import {EditorDoor} from './editorDoor.ts';
import {nameProblem, NameMost, resetZone, setZoneColour, tierCards, zoneChanged, zoneCoat} from './creatorRules.ts';
import {hostOf, type EditorBridge} from '../net/editor.ts';
import {HOST_SEES} from '../net/worlds.ts';
import {arr, bool, isObject, num, obj, str, type Json} from '../game/json.ts';
import {newCommandId} from '../net/session.ts';
import {agoText, envLine, worldTomlLines} from '../net/central.ts';

// The painted wolf in the creator (Docs/Design/81-painted-wolves.md): a size (which of the three profiles), the
// coat's three fur colours, the eyes, and any fur zone's own three: all from the palette. Drawn on nothing: the
// paintings are cut out, so the wolves stand over the page (the user, 2026-11-14: no paper card behind them).

/** A draft's fur: what it has chosen, or what its older fields come to (fur.ts); a copy, to change. */
function draftFur(d: Json): Fur {
    const a = readAppearance(d);
    const fur: Fur = a ? furOf(a) : {coat: ReadyCoats[0].coat as FurCoat, eyes: ReadyCoats[0].eyes};
    const zones = Object.entries(fur.zones ?? {}).map(([zone, coat]) => [zone, [...coat]]);
    return {coat: [...fur.coat], eyes: fur.eyes, ...(zones.length ? {zones: Object.fromEntries(zones)} : {})};
}

function draftBuild(d: Json): Build {
    const a = readAppearance(d);
    return a ? buildOf(a) : 'medium';
}

/** "Medium · 76 cm at shoulder": a wolf's size as its painting shows it, and the height that goes with that size. */
function sizeWords(d: Json | null, age: number): string {
    const a = readAppearance(d);
    if (!a) return '';
    const size = buildOf(a);
    return `${BuildNames[size]} · ${shoulderHeightCm({...a, stature: BuildStature[size]}, age).toFixed(0)} cm at shoulder`;
}

/** Puts a painted look on a draft: the size (the build, and the stature that goes with it) and the fur, and the
 *  older fields in step with them (the coat's colour and the eyes in words, the wolf on the sheet: fur.ts olderLook). */
function wear(d: Json, fur: Fur, build: Build) {
    const zones = Object.entries(fur.zones ?? {});
    d.build = build;
    d.stature = BuildStature[build];
    d.fur = {coat: [...fur.coat], eyes: fur.eyes, ...(zones.length ? {zones: Object.fromEntries(zones.map(([zone, coat]) => [zone, [...coat]]))} : {})};
    const older = olderLook(fur);
    Object.assign(d, {coat: older.coat, gradientTint: older.gradientTint, eyes: older.eyes, pattern: 'solid'});
    delete d.markingTint;
    if (older.markings.length) d.markings = older.markings;
    else delete d.markings;
}

const readyFur = (c: ReadyCoat): Fur => ({coat: c.coat as FurCoat, eyes: c.eyes, ...(c.zones ? {zones: c.zones as Record<string, FurCoat>} : {})});

/** "soot, ash and cream" */
const coatWords = (coat: FurCoat) => `${furName(coat[0])}, ${furName(coat[1])} and ${furName(coat[2])}`.toLowerCase();

/** A whole new look at random (keeping the name and age): a size, a natural coat, sometimes a zone or two of another
 *  coat's colours, and eyes. (The species stays as it is: it is no longer chosen, the user 2026-11-14.) */
function randomiseAppearance(d: Json) {
    const pick = <T>(list: readonly T[]) => list[Math.floor(Math.random() * list.length)];
    d.sex = pick(Choices.sex);
    const fur = readyFur(pick(ReadyCoats));
    const zones: Record<string, FurCoat> = {...(fur.zones ?? {})};
    for (let i = Math.floor(Math.random() * 3); i > 0; --i) zones[pick(FurZones).id] = pick(ReadyCoats).coat as FurCoat;
    wear(d, {coat: fur.coat, eyes: pick(EyeColours).id, zones}, pick(Builds));
}

type Page = 'login' | 'register' | 'menu' | 'roster' | 'creator' | 'review' | 'choices' | 'servers' | 'pick' | 'arrive' | 'host' | 'editor';

/** A world in the client's list, as the door shows it (client.ts makes these). */
export interface DoorWorld {
    id: string;              // '' for one reached by its address alone
    name: string;
    kind: string;            // 'official' | 'community'
    address: string;         // "host:port", the one connected to.
    host: string;            // What to show for it (the world's host name and port), or '' to show the address.
    secure: boolean;
    description: string;
    region: string;          // What the world's host says of it: where it is, and a few tags.
    tags: string[];
    fit: string;             // 'ok' | 'update' | 'older' | 'data' | 'unknown'
    note: string;            // Why it can't be joined, or ''.
    status: string;          // "3 of 100 wolves · 12 ms", "Not answering", "" (not asked yet)
    players?: number | null; // The server browser's columns: wolves in it, its limit, the round trip (null: not known).
    limit?: number | null;
    ping?: number | null;
    answer?: string;         // 'asking' | 'yes' | 'no'
    marks?: string[];        // "AI used", "cheats on"
}

/** One of a host's own worlds, on the Host page (client.ts makes these from GET /host/worlds). */
export interface DoorHostWorld {
    id: string;
    name: string;
    kind: string;            // 'official' | 'community'
    listed: boolean;         // In central's list now.
    reachable: boolean;      // Central's call-back last reached it.
    why: string;             // Why it isn't listed, in central's words ('' when it is).
    address: string;         // "ip:port" as central last saw it, or ''.
    players: number;
    lastHeartbeat: number | null;    // Unix seconds.
    tokenCreated: number | null;
}

export interface DoorOptions {
    mode?: 'world' | 'central';
    edition?: string;        // The title screen's corner.
    footnote?: string;       // The line under everything.
    canRemember?: boolean;   // "Remember me" (central mode, the client only).
    editor?: EditorBridge | null;   // The shell's Editor (the desktop client, not on a Mac): EDITOR and DUNGEON MASTER.
}

const Choices: Record<string, string[]> = {
    sex: ['female', 'male'],
};
const title = (v: string) => (v ? v[0].toUpperCase() + v.slice(1) : v);
const stageName = (age: number) => (age < 13 ? 'Young' : age < 18 ? 'Adolescent' : age < 65 ? 'Adult' : 'Old');
const utf8Length = (s: string) => new TextEncoder().encode(s).length;

/** The title screen's wolves: three of the ready coats, one of each size. */
const TitleWolves: Array<[Build, string]> = [['large', 'timber'], ['medium', 'arctic'], ['small', 'russet']];

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
    readonly mode: 'world' | 'central';
    readonly canRemember: boolean;
    stage: 'login' | 'characters' = 'login';
    page: Page = 'login';
    message = '';
    error = false;
    busy = false;
    characters: Json[] = [];
    selectedId = '';
    who = '';                                // Central mode: the signed-in account's name, on the main menu.
    draftName = '';
    draftAge = 18;
    draftAppearance: Json | null = null;
    draftTier = 'normal';                    // The Gift (doc 43): normal, gifted or quickened, and a family.
    draftFamily = '';
    gifts: Json | null = null;               // The tiers and families, as the server describes them.
    creation: Json | null = null;            // Strengths and weaknesses (doc 49): grades, budget, presets.
    tiers: Json | null = null;               // Which Gift tiers the account has earned, and what the rest need (doc 49).
    unlocks: Json | null = null;             // Central mode: what a world's standing asks before each tier opens (doc 49).
    nameMost = NameMost;                     // A new wolf's name, at most (central's rules say; 24).
    account: Json | null = null;             // The account as a person (doc 50): its handle, asked for when it has none.
    starts: Json[] = [];                     // Where a new wolf may arrive (doc 52): each town, the busiest preselected.
    firstCharacter = false;                  // The account's first wolf: told why it arrives where it does.
    draftStart = '';
    ties: Json[] = [];                       // Story starters for a tie (doc 52), and whether this wolf must take one.
    tieRequired = false;
    draftTie = '';
    draftGrades: Record<string, string> = {};   // An attribute to "weak" or "strong" (plain when absent).
    worlds: DoorWorld[] = [];                // Central mode: the server list.
    worldsWolf = '';                         // The wolf last taken to a world (Reconnect takes it again).
    lostWorld = '';                          // The world just lost, offered again (Reconnect).
    serverChoice = '';                       // The server chosen in the list (its id).
    pickWorld: DoorWorld | null = null;      // The server a wolf is being chosen for...
    pickAddress = '';                        // ...or the address typed.
    choicesWolf = '';                        // The wolf whose strengths and tie are being changed.
    arriveWolf: Json | null = null;          // Central mode: the wolf arriving in a world for the first time.
    arriveWorld = '';
    arriveKind = '';                         // The kind of world being joined ('' by address: told as community).
    arriveTieChosen = false;                 // The tie at this arrival was picked for the wolf (it had none, and must).
    tierChoices: string[] = [];
    canHost = false;                         // Central mode: the account is a host's (central sets it), so the Host page.
    hostWorlds: DoorHostWorld[] = [];        // The Host page: the account's worlds, how many it may have, and central's URL.
    hostMost = 3;
    hostCentral = '';
    hostToken = '';                          // A token just made, shown this once (gone when the page is left).
    hostTokenWorld = '';
    private typedUsername = '';
    /** The Editor's screens (doc 78, section 2), when the shell has the Editor. */
    readonly editorDoor: EditorDoor | null = null;
    private readonly edition: string;

    private showGiftDetails = false;
    private worldLines = new Map<string, {row: HTMLElement; players: HTMLElement; ping: HTMLElement; note: HTMLElement}>();
    private serverJoin: HTMLButtonElement | null = null;
    private creationRequestId = '';
    private creationFingerprint = '';
    private sentAt = 0;
    private body: HTMLDivElement;
    private status: HTMLParagraphElement;
    private password: HTMLInputElement | null = null;
    private portraits = new Portraits();
    private wolves = this.portraits.wolves;
    private portraitCanvas: HTMLCanvasElement | null = null;     // The wolf's profile (or a player's own picture).
    private faceCanvas: HTMLCanvasElement | null = null;         // The wolf's face, where a page shows it as its own portrait.
    private portraitWolf: Json | null = null;                    // Whose they are, when not the selected wolf or the draft.
    private pagePainters: Array<() => void> = [];        // The page's other painted wolves (the roster's faces, the title's art).
    private panelPainters: Array<() => void> = [];       // The creator panel's (the builds to choose from).
    private furSlot: Record<string, number> = {};        // Which of a picker's three colours is being chosen.
    private openZone = '';                               // The fur zone whose palette is open.
    private timer: number;
    private send: (command: Json) => void;

    constructor(parent: HTMLElement, send: (command: Json) => void, options: DoorOptions = {}) {
        this.send = send;
        this.mode = options.mode ?? 'world';
        this.canRemember = options.canRemember === true;
        if (options.editor) {
            this.editorDoor = new EditorDoor(options.editor, {
                redraw: () => this.show('editor'),
                message: (text, error) => this.setMessage(text, error === true),
                leave: () => this.show(this.stage === 'characters' ? 'menu' : 'login'),
                hostAWorld: () => this.hostAWorld(),
                officialHosts: () => [...this.worlds, ...this.hostWorlds].filter(w => w.kind === 'official' && w.address).map(w => hostOf(w.address)),
                setBusy: busy => this.root.classList.toggle('busy', busy),
            });
        }
        this.body = el('div', {className: 'door-body'});
        this.status = el('p', {className: 'door-status'});
        const central = this.mode === 'central';
        this.edition = options.edition ?? (central ? 'EARLY DEVELOPMENT' : 'BROWSER CLIENT  /  EARLY DEVELOPMENT');
        const stage = el('div', {className: 'door-stage'},
            el('header', {className: 'door-header'},
                el('div', {className: 'door-title'}, el('h1', {}, 'RUN AGAINST THE WORLD')),
                el('div', {className: 'door-edition'}, this.edition)),
            this.body, this.status,
            el('p', {className: 'door-footnote'}, options.footnote ?? (central ? ''
                : 'LOCAL TEST ACCOUNTS ONLY · This connection is not encrypted yet. Use a unique test password, never a real one.')));
        this.root = el('div', {className: 'door'}, stage);
        this.root.dataset.mode = this.mode;
        parent.append(this.root);
        this.portraits.listen(() => {
            this.drawPortrait();
            for (const paint of [...this.pagePainters, ...this.panelPainters]) paint();
        });
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

    /** Where Back (and Escape) goes from each page. */
    private backFrom(page: Page): Page | null {
        const central = this.mode === 'central';
        const back: Partial<Record<Page, Page>> = {review: 'creator', register: 'login', choices: 'roster',
            creator: central ? 'menu' : 'roster', servers: 'menu', pick: 'servers', host: 'menu'};
        if (central) back.roster = 'menu';
        return back[page] ?? null;
    }

    private onKey = (e: KeyboardEvent) => {
        if (e.code !== 'Escape' || this.busy) return;
        if (document.querySelector('.ratw-dialog')) return;
        if (this.page === 'editor') {
            e.preventDefault();
            this.editorDoor?.leave();
            return;
        }
        const to = this.backFrom(this.page);
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
        if (next === 'worlds') {
            // Central mode: the server list (client.ts).
            this.stage = 'characters';
            this.worlds = arr(event, 'worlds').filter(isObject).map(w => {
                const count = (v: unknown) => (typeof v === 'number' && Number.isFinite(v) ? v : null);
                return {id: str(w, 'id'), name: str(w, 'name'), kind: str(w, 'kind'),
                    address: str(w, 'address'), host: str(w, 'host'), secure: bool(w, 'secure'), description: str(w, 'description'), region: str(w, 'region'),
                    tags: arr(w, 'tags').filter((t): t is string => typeof t === 'string'), fit: str(w, 'fit', 'unknown'),
                    note: str(w, 'note'), status: str(w, 'status'), players: count(w.players), limit: count(w.limit), ping: count(w.ping),
                    answer: str(w, 'answer', 'asking'), marks: arr(w, 'marks').filter((t): t is string => typeof t === 'string')};
            });
            if (str(event, 'wolf')) this.worldsWolf = str(event, 'wolf');
            this.lostWorld = str(event, 'lost');
            if (this.lostWorld) this.serverChoice = this.lostWorld;
            if (!this.worlds.some(w => w.id === this.serverChoice)) this.serverChoice = this.worlds.find(w => this.fits(w))?.id ?? '';
            this.show('servers');
            return;
        }
        if (next === 'host') {
            // Central mode: the Host page (client.ts).
            this.stage = 'characters';
            const time = (v: unknown) => (typeof v === 'number' && Number.isFinite(v) && v > 0 ? v : null);
            this.hostWorlds = arr(event, 'worlds').filter(isObject).map(w => ({id: str(w, 'id'), name: str(w, 'name'), kind: str(w, 'kind'),
                listed: bool(w, 'listed'), reachable: bool(w, 'reachable'), why: str(w, 'why'), address: str(w, 'address'),
                players: num(w, 'players'), lastHeartbeat: time(w.lastHeartbeat), tokenCreated: time(w.tokenCreated)}));
            this.hostMost = num(event, 'most', 3);
            this.hostCentral = str(event, 'central', this.hostCentral);
            if (str(event, 'token')) {
                this.hostToken = str(event, 'token');
                this.hostTokenWorld = str(event, 'tokenWorld');
            }
            // A token whose world is gone is no use to anyone.
            if (this.hostToken && !this.hostWorlds.some(w => w.id === this.hostTokenWorld)) this.hostToken = this.hostTokenWorld = '';
            this.show('host');
            return;
        }
        if (next === 'arrive') {
            // A world's first arrival (doc 77, 5.1; Core/RatwGameTickets.cpp, ticketLobby): its own catalogues, and the
            // wolf from the ticket. The strengths and the tie are the wolf's own, set in the Character Editor.
            if (isObject(event.gifts)) this.gifts = event.gifts;
            if (isObject(event.creation)) this.creation = event.creation;
            this.tiers = isObject(event.tiers) ? event.tiers : null;
            this.starts = arr(event, 'starts').filter(isObject);
            this.ties = arr(event, 'ties').filter(isObject);
            this.tieRequired = event.tieRequired === true;
            this.firstCharacter = event.firstCharacter === true;
            this.tierChoices = arr(event, 'tierChoices').filter((t): t is string => typeof t === 'string');
            const wolf = isObject(event.wolf) ? event.wolf : null;
            const same = this.page === 'arrive' && str(wolf, 'id') === str(this.arriveWolf, 'id');
            this.arriveWolf = wolf;
            this.arriveWorld = str(event, 'worldName');
            if (!same) {
                this.draftName = str(wolf, 'name');
                this.draftAge = num(wolf, 'age', 18);
                this.draftAppearance = obj(wolf, 'appearance') ? {...obj(wolf, 'appearance')} : null;
                this.draftFamily = str(wolf, 'family');
                this.draftTier = this.draftFamily ? (this.tierChoices.includes('gifted') ? 'gifted' : this.tierChoices[0] ?? 'gifted') : 'normal';
                const saved = obj(this.characters.find(c => str(c, 'id') === str(wolf, 'id')) ?? null, 'choices');
                this.draftGrades = gradesOf(obj(saved, 'grades'));
                this.draftTie = this.ties.some(t => str(t, 'id') === str(saved, 'tie')) ? str(saved, 'tie') : '';
                this.arriveTieChosen = false;
                if (this.tieRequired && !this.draftTie) {
                    // A first wolf here must take a tie, and this one has none of its own: one is picked for it.
                    this.draftTie = this.randomTie('');
                    this.arriveTieChosen = !!this.draftTie;
                }
                this.draftStart = str(this.starts.find(t => t.suggested === true) ?? this.starts[0], 'id');
                this.creationRequestId = this.creationFingerprint = '';
                this.creatorTab = this.draftFamily && this.tierChoices.length > 1 ? 'gift' : 'arrival';
            }
            this.stage = 'characters';
            if (ok) this.setMessage(this.arriveWelcome());
            this.show('arrive');
            return;
        }
        if (next === 'characters') {
            if (isObject(event.gifts)) this.gifts = event.gifts;
            if (isObject(event.creation)) this.creation = event.creation;
            if (isObject(event.tiers)) this.tiers = event.tiers;
            if (isObject(event.unlocks)) this.unlocks = event.unlocks;
            if (typeof event.nameMost === 'number' && event.nameMost >= 2) this.nameMost = event.nameMost;
            if (isObject(event.account)) this.account = event.account;
            if (typeof event.who === 'string') this.who = event.who;
            if (Array.isArray(event.starts)) this.starts = event.starts.filter(isObject);
            if (Array.isArray(event.ties)) this.ties = event.ties.filter(isObject);
            this.canHost = this.mode === 'central' && event.host === true;
            if (this.mode === 'world' || event.tieRequired !== undefined) this.tieRequired = event.tieRequired === true;
            this.firstCharacter = event.firstCharacter === true;
            this.characters = (Array.isArray(event.characters) ? event.characters : []).filter(isObject).slice(0, 6);
            if (!this.selected() && this.characters.length) this.selectedId = str(this.characters[0], 'id');
            if (ok && this.page === 'review')
                for (const c of this.characters) if (str(c, 'name') === this.draftName) this.selectedId = str(c, 'id');
            const keep: Page[] = ['creator', 'review', 'choices', 'menu', 'pick', 'roster'];
            const keepPage = !ok && this.stage === 'characters' && keep.includes(this.page);
            this.stage = 'characters';
            if (this.mode === 'central' && event.home === true) this.show('menu');
            else if (keepPage) this.show(this.page);
            else if (this.mode === 'central' && this.page === 'menu' && ok) this.show('menu');
            else this.show('roster');
        } else {
            const wasIn = this.stage === 'characters';
            this.stage = 'login';
            this.characters = [];
            this.selectedId = '';
            this.who = '';
            this.canHost = false;
            this.hostWorlds = [];
            this.hostToken = this.hostTokenWorld = '';
            this.pickWorld = null;
            this.pickAddress = '';
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
        // A host token is shown once: leaving the Host page forgets it.
        if (page !== 'host') this.hostToken = this.hostTokenWorld = '';
        if (page === 'menu' && this.mode !== 'central') page = this.stage === 'characters' ? 'roster' : 'login';
        this.page = page;
        this.portraitCanvas = this.faceCanvas = null;
        this.portraitWolf = null;
        this.pagePainters = [];
        this.panelPainters = [];
        this.password = null;
        this.serverJoin = null;
        this.worldLines.clear();
        // The Editor's password boxes are emptied whenever its screen is left or drawn again.
        this.editorDoor?.wipe();
        if (page === 'editor' && !this.editorDoor) page = this.page = this.stage === 'characters' ? 'menu' : 'login';
        this.body.replaceChildren(page === 'login' || page === 'register' ? this.titlePage()
            : page === 'menu' ? this.menuPage() : page === 'roster' ? this.rosterPage() : page === 'servers' ? this.serversPage()
            : page === 'pick' ? this.pickPage() : page === 'arrive' ? this.arrivePage() : page === 'choices' ? this.choicesPage()
            : page === 'host' ? this.hostPage() : page === 'editor' ? this.editorDoor!.render()
            : this.creatorPage(page === 'review'));

        this.root.dataset.page = page;
        this.drawPortrait();
        const first = this.body.querySelector<HTMLElement>('input, button.primary:not(:disabled)');
        first?.focus({preventScroll: true});
    }

    private button(text: string, action: () => void, primary = false): HTMLButtonElement {
        const b = el('button', {type: 'button', className: primary ? 'primary' : 'secondary', textContent: text});
        b.addEventListener('click', () => { if (!this.busy) action(); });
        return b;
    }

    /** A Back button to wherever Back goes from this page. */
    private backButton(text: string, to: Page, action = 'back'): HTMLButtonElement {
        const b = this.button(text, () => {
            this.setMessage('');
            this.show(to);
        });
        b.dataset.action = action;
        return b;
    }

    // ------------------------------------------------------------------ The title screen (signing in)

    /** The game's title, the painted wolves, and a clean sign-in (the user, 2026-11-14: like a game's title screen). */
    private titlePage(): HTMLElement {
        const register = this.page === 'register';
        const form = this.mode === 'central' ? this.centralLoginForm() : this.worldLoginForm();
        const extra: HTMLElement[] = [];
        // EDITOR before signing in (doc 78, screen 1): the Editor needs no central sign-in, so it works when central is
        // down; on the main menu too.
        if (this.editorDoor && !register) {
            const editor = this.button('EDITOR', () => { this.setMessage(''); void this.editorDoor?.open(); });
            editor.dataset.action = 'editor';
            extra.push(el('div', {className: 'title-editor'}, editor, el('span', {}, 'Run or build your own world · no sign-in needed')));
        }
        return el('div', {className: 'title-screen'},
            this.artCanvas('title-art', 1500, 700),
            el('div', {className: 'title-brand'},
                el('p', {className: 'title-kicker'}, 'A LIVING WORLD  ·  A STORY OF YOUR OWN'),
                el('h1', {className: 'title-name'}, 'Run Against', el('br'), 'the World')),
            el('div', {className: 'title-side'}, form, ...extra),
            el('p', {className: 'title-edition'}, this.edition));
    }

    /** The title's wolves, painted in three ready coats, on nothing. */
    private artCanvas(className: string, width: number, height: number): HTMLCanvasElement {
        const canvas = el('canvas', {className, width, height});
        const paint = () => {
            const g = canvas.getContext('2d');
            if (!g) return;
            g.clearRect(0, 0, width, height);
            const box = {w: width * 0.56, h: height * 0.86};
            TitleWolves.forEach(([build, coat], i) => {
                const ready = ReadyCoats.find(c => c.id === coat) ?? ReadyCoats[i % ReadyCoats.length];
                this.wolves.draw(g, build, readyFur(ready), i * width * 0.22, height - box.h, box.w, box.h);
            });
        };
        this.pagePainters.push(paint);
        paint();
        return canvas;
    }

    private worldLoginForm(): HTMLElement {
        const register = this.page === 'register';
        const user = el('input', {type: 'text', autocomplete: 'username', placeholder: '3–32 letters, digits, _ or -', maxLength: 32});
        const pass = el('input', {type: 'password', autocomplete: register ? 'new-password' : 'current-password',
            placeholder: 'Unique test password · 12–128 bytes'});
        this.password = pass;
        const form = el('form', {className: 'door-card door-login title-card'},
            el('h2', {}, register ? 'Create a test account' : 'Sign in'),
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
        return form;
    }

    /** Signing in to central (doc 77, 3.1): an account for every world; registering asks the year of birth (16 or
     *  older, Q8; the month too only in the year one turns 16); "remember me" keeps a sign-in in the client's shell,
     *  never the password. */
    private centralLoginForm(): HTMLElement {
        const register = this.page === 'register';
        const user = el('input', {type: 'text', autocomplete: 'username', placeholder: '3–32 letters, digits, _ or -', maxLength: 32});
        user.name = 'username';
        user.value = this.typedUsername;                // (Kept when the page is shown again after a refusal; never the password.)
        user.addEventListener('input', () => { this.typedUsername = user.value; });
        const pass = el('input', {type: 'password', autocomplete: register ? 'new-password' : 'current-password',
            placeholder: '12–128 characters'});
        pass.name = 'password';
        this.password = pass;
        const thisYear = new Date().getFullYear();
        const year = el('input', {type: 'number', min: '1900', max: String(thisYear), placeholder: 'e.g. 1990'});
        year.name = 'birthYear';
        const month = el('select', {});
        month.name = 'birthMonth';
        month.append(el('option', {value: '', textContent: 'Month'}),
            ...['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December']
                .map((m, i) => el('option', {value: String(i + 1), textContent: m})));
        const monthLabel = el('label', {}, el('span', {}, 'MONTH OF BIRTH · YOU TURN 16 THIS YEAR'), month);
        const showMonth = () => { monthLabel.style.display = Number(year.value) === thisYear - 16 ? '' : 'none'; };
        year.addEventListener('input', showMonth);
        showMonth();
        const remember = el('input', {type: 'checkbox'});
        remember.name = 'remember';
        const form = el('form', {className: 'door-card door-login title-card'},
            el('h2', {}, register ? 'Create an account' : 'Sign in'),
            el('label', {}, el('span', {}, 'ACCOUNT NAME'), user),
            el('label', {}, el('span', {}, 'PASSWORD'), pass),
            ...(register ? [el('label', {}, el('span', {}, 'YEAR OF BIRTH · YOU MUST BE 16 OR OLDER'), year), monthLabel] : []),
            ...(this.canRemember ? [el('label', {className: 'door-check'}, remember, el('span', {}, 'REMEMBER ME ON THIS COMPUTER'))] : []),
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
            if (!username || !secret) return this.setMessage('Enter an account name and a password.', true);
            if (username.length < 3 || username.length > 32 || utf8Length(secret) < 12 || utf8Length(secret) > 128)
                return this.setMessage('Account names need 3–32 characters; passwords need 12–128 UTF-8 bytes.', true);
            const command: Json = {type: register ? 'auth_register' : 'auth_login', username, password: secret, remember: remember.checked};
            if (register) {
                const born = Math.trunc(Number(year.value));
                if (!year.value || !Number.isFinite(born) || born < 1900 || born > thisYear) return this.setMessage('Give your year of birth.', true);
                command.birth_year = born;
                if (born === thisYear - 16) {
                    if (!month.value) return this.setMessage('You turn 16 this year: give the month you were born, too.', true);
                    command.birth_month = Number(month.value);
                }
            }
            pass.value = '';
            this.setMessage('Contacting central…');
            this.submit(command);
        });
        return form;
    }

    /** The Editor's "Don't have a world yet? HOST A WORLD.": the Host page, for a host's account once signed in. */
    private hostAWorld() {
        if (this.stage === 'characters' && this.canHost) {
            this.setMessage('Asking central for your worlds…');
            this.submit({type: 'host_show'});
            return;
        }
        this.show(this.stage === 'characters' ? 'menu' : 'login');
        this.setMessage(this.stage === 'characters' ? 'Hosting a world needs a host\'s account: the hosting guide says how to ask for one.'
            : 'Sign in first: HOST A WORLD is on the main menu, for a host\'s account.');
    }

    // ------------------------------------------------------------------ The main menu (central mode)

    /** After signing in (the user, 2026-11-14): Servers, the Character Editor, the Editor and the Dungeon Master. */
    private menuPage(): HTMLElement {
        const tile = (action: string, name: string, line: string, run: () => void, why = '') => {
            const b = el('button', {type: 'button', className: 'menu-tile'}, el('strong', {}, name), el('span', {}, why || line));
            b.dataset.action = action;
            b.disabled = !!why;
            b.addEventListener('click', () => { if (!this.busy) run(); });
            return b;
        };
        const noEditor = this.editorDoor ? '' : 'In the desktop client, on Windows and Linux: the Editor runs beside it.';
        const tiles = el('nav', {className: 'menu-tiles'},
            tile('servers', 'SERVERS', 'Find a world and play', () => this.openServers()),
            tile('characters', 'CHARACTER EDITOR', 'Your wolves: make one, change their look, their strengths and their tie',
                () => { this.setMessage(''); this.show('roster'); }),
            tile('editor', 'EDITOR', 'Your own world\'s tools: its map and its settings', () => { this.setMessage(''); void this.editorDoor?.open(); }, noEditor),
            // The Dungeon Master tool is the Editor's (doc 78): for a world you host or help run, after its Editor sign-in.
            tile('dungeon-master', 'DUNGEON MASTER', 'Run the story of a world you host: its people, events and players',
                () => { this.setMessage(''); void this.editorDoor?.openDm(); }, noEditor));
        const actions: HTMLElement[] = [];
        if (this.canHost) {
            const host = this.button('HOST A WORLD', () => {
                this.setMessage('Asking central for your worlds…');
                this.submit({type: 'host_show'});
            });
            host.dataset.action = 'host';
            actions.push(host);
        }
        const signOut = this.button('Sign out', () => {
            this.setMessage('Signing out…');
            this.submit({type: 'auth_logout'});
        });
        signOut.dataset.action = 'sign-out';
        const remove = this.button('Delete my account…', () => void this.deleteAccount());
        remove.dataset.action = 'delete-account';
        actions.push(signOut, remove);
        const wolf = this.selected();
        const count = this.characters.length;
        return el('div', {className: 'menu-screen'},
            this.artCanvas('menu-art', 1500, 700),
            el('div', {className: 'menu-brand'}, el('p', {className: 'title-kicker'}, 'MAIN MENU'), el('h1', {className: 'menu-name'}, 'Run Against the World')),
            tiles,
            el('footer', {className: 'menu-footer'},
                el('p', {className: 'muted'}, `${this.who ? `Signed in as ${this.who}` : 'Signed in'}  ·  ` +
                    (count ? `${count} of 6 wolves${wolf ? `  ·  ${str(wolf, 'name')}` : ''}` : 'No wolves yet: make one in the Character Editor')),
                el('div', {className: 'menu-actions'}, ...actions)));
    }

    private openServers() {
        this.setMessage('Asking central for the servers…');
        this.submit({type: 'worlds_show'});
    }

    // ------------------------------------------------------------------ The roster: the Character Editor

    private rosterPage(): HTMLElement {
        const central = this.mode === 'central';
        const slots = el('div', {className: 'door-slots'}, el('p', {className: 'gold small'}, central ? 'CHARACTER EDITOR' : 'CHOOSE YOUR CHARACTER'));
        for (let i = 0; i < 6; ++i) {
            const c = this.characters[i];
            const id = str(c, 'id');
            const age = num(c, 'age', 18);
            const b = el('button', {type: 'button', className: `slot${id && id === this.selectedId ? ' selected' : ''}`},
                el('strong', {}, c ? str(c, 'name') : '+  Create a character'),
                el('span', {}, c ? `Age ${age.toFixed(0)} · ${stageName(age)}${this.caption(c, ' · ')}` : `Empty slot ${i + 1} of 6`));
            // Each wolf's painted face beside its name (doc 81), on nothing.
            const look = c ? readAppearance(obj(c, 'appearance')) : null;
            if (look) {
                const face = el('canvas', {className: 'slot-face', width: 128, height: 128});
                const paint = () => {
                    const g = face.getContext('2d');
                    if (!g) return;
                    g.clearRect(0, 0, face.width, face.height);
                    this.wolves.draw(g, 'face', furOf(look), 0, 0, face.width, face.height);
                };
                this.pagePainters.push(paint);
                paint();
                b.classList.add('with-face');
                b.replaceChildren(face, el('span', {className: 'slot-text'}, ...Array.from(b.childNodes)));
            }
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
        // The account's handle (doc 50): the name friends and circles see, never the sign-in name. (Per world: in
        // central mode a world asks at the first arrival, doc 77 Q5.)
        if (this.mode === 'world' && this.account && !str(this.account, 'handle')) {
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
        slots.append(el('div', {className: 'grow'}));
        if (central) slots.append(this.backButton('Back to the main menu', 'menu', 'menu'));
        else slots.append(this.button('Sign out of account', () => {
            this.setMessage('Signing out…');
            this.submit({type: 'auth_logout'});
        }));
        const c = this.selected();
        const age = num(c, 'age', 18);
        const canvas = el('canvas', {className: 'portrait painted', width: 960, height: 660});
        this.portraitCanvas = c ? canvas : null;
        const below: HTMLElement[] = [];
        let go: HTMLElement | null = null;
        if (!central) {
            below.push(c ? this.portraitControls(c) : el('p', {className: 'muted large'}, ''));
            const enter = this.button('ENTER THE WORLD', () => this.enter(), true);
            enter.disabled = !c;
            enter.dataset.action = 'enter';
            go = enter;
        } else if (c) {
            below.push(el('p', {className: 'muted roster-choices'}, this.choicesLine(obj(c, 'choices'))));
            const choices = this.button('STRENGTHS & TIE', () => this.editChoices(c), true);
            choices.dataset.action = 'choices';
            const remove = this.button('Delete this wolf…', () => void this.deleteWolf(c));
            remove.dataset.action = 'delete-wolf';
            go = el('div', {className: 'door-actions roster-actions'}, remove, choices);
            below.push(el('p', {className: 'muted small'}, 'Your wolf\'s name and look go with them to every world; what they do in a world stays there. ' +
                'Play from SERVERS on the main menu.'));
        }
        const detail = el('section', {className: 'door-card door-detail'},
            el('h2', {className: 'name'}, str(c, 'name', 'An unwritten story')),
            el('p', {className: 'muted large pre'}, c ? `${stageName(age)} · age ${age.toFixed(0)}` + this.caption(c, ' · ') +
                (sizeWords(obj(c, 'appearance'), age) ? `\n${sizeWords(obj(c, 'appearance'), age)}` : '')
                : 'Choose an empty slot to make your first wolf.'),
            canvas,
            ...below,
            ...(go ? [go] : []));
        return el('div', {className: 'door-row'}, slots, detail);
    }

    /** "Strong: Smell, Hearing · Weak: Wisdom · Tie: a cart on the road", for a wolf's own choices. */
    private choicesLine(choices: Json | null): string {
        const grades = gradesOf(obj(choices, 'grades'));
        const tie = str(choices, 'tie');
        return `${this.gradesWords(grades)} · Tie: ${tie ? this.tieLine(tie) : 'none'}`;
    }

    private gradesWords(grades: Record<string, string>): string {
        const names = new Map(arr(this.creation, 'attributes').filter(isObject).map(a => [str(a, 'id'), str(a, 'name')]));
        const listed = (grade: string) => Object.entries(grades).filter(([, g]) => g === grade).map(([a]) => names.get(a) ?? title(a)).join(', ');
        return `Strong: ${listed('strong') || 'none'} · Weak: ${listed('weak') || 'none'}`;
    }

    /** "Gifted · Water" (a world's character), or "Water family" (a wolf on central, whose tier each world gives). */
    private caption(c: Json | null, before: string): string {
        if (this.mode === 'world') return giftCaption(c, this.gifts, before);
        const family = str(c, 'family');
        if (!family) return `${before}Normal`;
        const f = arr(this.gifts, 'families').filter(isObject).find(x => str(x, 'id') === family);
        return `${before}${str(f, 'name', title(family))} family`;
    }

    /** Central mode: one wolf deleted, the account kept; the password asked again (central does too). */
    private async deleteWolf(c: Json) {
        const password = await askText(`Delete ${str(c, 'name')} for good? Their name is held back for 30 days, and every world is told ` +
            'to forget them. Give your password to confirm.', {password: true, ok: 'Delete the wolf', danger: true, placeholder: 'Your password'});
        if (!password) return;
        this.setMessage(`Deleting ${str(c, 'name')}…`);
        this.submit({type: 'wolf_delete', id: str(c, 'id'), password});
    }

    /** Central mode: the whole account, every wolf with it. */
    private async deleteAccount() {
        const password = await askText('Delete your account and every wolf on it, for good? The names are held back for 30 days. ' +
            'Give your password to confirm.', {password: true, ok: 'Delete my account', danger: true, placeholder: 'Your password'});
        if (!password) return;
        this.setMessage('Deleting your account…');
        this.submit({type: 'account_delete', password});
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

    /** A browser on a password world: the chosen character goes in. */
    enter() {
        if (!this.selected() || this.busy || this.mode !== 'world') return;
        this.setMessage('Joining the world…');
        this.submit({type: 'character_enter', id: this.selectedId});
    }

    // ------------------------------------------------------------------ A wolf's strengths and tie (central mode)

    private editChoices(c: Json) {
        this.choicesWolf = str(c, 'id');
        const saved = obj(c, 'choices');
        this.draftGrades = gradesOf(obj(saved, 'grades'));
        this.draftTie = str(saved, 'tie');
        this.draftFamily = str(c, 'family');
        this.draftTier = this.draftFamily ? 'gifted' : 'normal';
        if (!['strengths', 'tie'].includes(this.creatorTab)) this.creatorTab = 'strengths';
        this.setMessage('');
        this.show('choices');
    }

    /** Strengths and tie for a wolf already made (the user, 2026-11-14: in the character editor, not at each arrival).
     *  Kept with the wolf on central; each world takes them at the wolf's first arrival there. */
    private choicesPage(): HTMLElement {
        const wolf = this.characters.find(c => str(c, 'id') === this.choicesWolf) ?? null;
        if (!wolf) return this.rosterPage();
        this.portraitWolf = wolf;
        const age = num(wolf, 'age', 18);
        const canvas = el('canvas', {className: 'portrait painted', width: 960, height: 660});
        this.portraitCanvas = canvas;
        const preview = el('section', {className: 'door-card door-preview'},
            el('p', {className: 'gold small'}, 'STRENGTHS & TIE'),
            this.wolfId(el('h2', {className: 'name'}, str(wolf, 'name')),
                el('p', {className: 'sage large pre'}, `${stageName(age)} · age ${age.toFixed(0)}${this.caption(wolf, '\n')}`)),
            canvas,
            el('p', {className: 'muted'}, 'Each world your wolf goes to takes these at their first arrival there. A world they have been to keeps what it was given.'));
        const tabs = el('div', {className: 'creator-tabs'});
        const panel = el('div', {className: 'creator-panel'});
        const render = () => {
            tabs.replaceChildren(...([['strengths', 'Strengths'], ['tie', 'Tie']] as Array<[string, string]>).map(([id, label]) => {
                const b = el('button', {type: 'button', className: id === this.creatorTab ? 'tab active' : 'tab', textContent: label});
                b.dataset.tab = id;
                b.addEventListener('click', () => {
                    this.creatorTab = id;
                    render();
                });
                return b;
            }));
            panel.replaceChildren(...(this.creatorTab === 'tie' ? this.tiePanel(render) : this.strengthsPanel(render)));
        };
        if (!['strengths', 'tie'].includes(this.creatorTab)) this.creatorTab = 'strengths';
        render();
        const save = this.button('SAVE', () => this.saveChoices(), true);
        save.dataset.action = 'save-choices';
        const form = el('section', {className: 'door-card door-form'},
            el('div', {className: 'scroll'}, el('div', {className: 'door-fields'}, el('div', {className: 'creator'}, tabs, panel))),
            el('div', {className: 'door-actions'}, this.backButton('Back to your wolves', 'roster'), save));
        return el('div', {className: 'door-row'}, preview, form);
    }

    private saveChoices() {
        if (this.busy || this.page !== 'choices') return;
        if (this.creation && !this.buildFits(this.draftGrades)) return this.setMessage('That build costs more points than there are.', true);
        this.setMessage('Saving…');
        this.submit({type: 'wolf_choices', id: this.choicesWolf, choices: {grades: {...this.draftGrades}, tie: this.draftTie}});
    }

    // ------------------------------------------------------------------ The servers (central mode)

    private fits(w: DoorWorld): boolean {
        return w.fit === 'ok' || w.fit === 'unknown';
    }

    /** "3 / 100", "—" */
    private playersText(w: DoorWorld): string {
        if (typeof w.players !== 'number') return '—';
        return typeof w.limit === 'number' && w.limit > 0 ? `${w.players} / ${w.limit}` : `${w.players}`;
    }

    private pingText(w: DoorWorld): string {
        if (w.answer === 'no') return 'Not answering';
        if (typeof w.ping === 'number') return `${w.ping} ms`;
        return w.answer === 'yes' ? 'Up' : 'Asking…';
    }

    /** A generic server browser in a window over the main menu (the user, 2026-11-14): the worlds central lists, each
     *  one's name, kind, players and limit, region, round trip and tags; refresh; join by address. A server chosen, the
     *  wolf to play there is chosen next. Greyed where this client can't play ("needs an update"). */
    private serversPage(): HTMLElement {
        const table = el('table', {className: 'server-table'});
        table.append(el('thead', {}, el('tr', {}, ...['Name', 'Kind', 'Players', 'Region', 'Ping', 'Tags'].map(h => el('th', {}, h)))));
        const rows = el('tbody', {});
        const join = this.button(this.serverChoice && this.serverChoice === this.lostWorld ? 'RECONNECT' : 'JOIN', () => this.chooseServer(), true);
        join.dataset.action = 'server-join';
        this.serverJoin = join;
        const choose = (id: string) => {
            this.serverChoice = id;
            for (const [rowId, line] of this.worldLines) line.row.classList.toggle('chosen', rowId === id);
            this.updateJoin();
        };
        for (const w of this.worlds) {
            const plain = !w.secure && !/^(127\.|localhost:|\[::1\]:)/.test(w.address);
            const players = el('td', {className: 'server-players'}, this.playersText(w));
            const ping = el('td', {className: 'server-ping'}, this.pingText(w));
            const note = el('p', {className: 'world-note gold'}, w.note);
            const badges = [el('span', {className: `world-kind ${w.kind === 'official' ? 'official' : ''}`}, w.kind === 'official' ? 'OFFICIAL' : 'COMMUNITY')];
            const tags = [...(w.marks ?? []), ...w.tags, ...(plain ? ['not encrypted'] : [])];
            const row = el('tr', {className: `server-row${this.fits(w) ? '' : ' unfit'}${w.id === this.serverChoice ? ' chosen' : ''}`},
                el('td', {className: 'server-name'},
                    el('strong', {}, w.name, ...(w.id === this.lostWorld ? [el('span', {className: 'world-kind lost'}, 'LOST · RECONNECT')] : [])),
                    ...(w.description ? [el('p', {className: 'muted small'}, w.description)] : []),
                    el('p', {className: 'muted small server-address', title: w.host ? w.address : ''}, w.host || w.address),
                    note),
                el('td', {}, ...badges),
                players,
                el('td', {className: 'muted'}, w.region || '—'),
                ping,
                el('td', {}, el('div', {className: 'server-tags'}, ...tags.map(t => el('span', {className: 'server-tag'}, t)))));
            row.dataset.world = w.id;
            row.tabIndex = 0;
            row.addEventListener('click', () => choose(w.id));
            row.addEventListener('dblclick', () => { choose(w.id); this.chooseServer(); });
            row.addEventListener('keydown', e => { if (e.key === 'Enter') { choose(w.id); this.chooseServer(); } });
            this.worldLines.set(w.id, {row, players, ping, note});
            rows.append(row);
        }
        table.append(rows);
        const empty = this.worlds.length ? [] : [el('p', {className: 'muted large server-empty'}, 'Central lists no worlds just now. Refresh in a while, or join one by its address.')];
        const address = el('input', {type: 'text', placeholder: 'host:port, e.g. 192.168.1.20:7788', maxLength: 260});
        address.name = 'address';
        const direct = el('form', {className: 'world-direct'},
            el('label', {}, el('span', {}, 'JOIN BY ADDRESS · ON YOUR NETWORK, OR PRIVATE'), address),
            el('button', {type: 'submit', className: 'secondary', textContent: 'JOIN BY ADDRESS'}));
        direct.addEventListener('submit', e => {
            e.preventDefault();
            if (this.busy) return;
            const typed = address.value.trim();
            if (!typed) return this.setMessage('Type a world\'s address: host:port.', true);
            this.pickWorld = null;
            this.pickAddress = typed;
            this.arriveKind = '';
            this.setMessage('');
            this.show('pick');
        });
        const refresh = this.button('Refresh', () => this.openServers());
        refresh.dataset.action = 'servers-refresh';
        const close = this.backButton('×', 'menu', 'servers-close');
        close.title = 'Back to the main menu';
        close.classList.add('window-close');
        this.updateJoin();
        const window_ = el('section', {className: 'server-window'},
            el('header', {className: 'window-head'},
                el('h2', {}, 'Servers'),
                el('span', {className: 'muted'}, `${this.worlds.length} ${this.worlds.length === 1 ? 'world' : 'worlds'}`),
                el('div', {className: 'grow'}), refresh, close),
            el('div', {className: 'server-scroll'}, table, ...empty),
            el('footer', {className: 'window-foot'}, direct,
                el('div', {className: 'door-actions'}, this.backButton('Back to the main menu', 'menu', 'menu'), join)),
            el('p', {className: 'muted small'}, HOST_SEES));
        return el('div', {className: 'servers-screen'}, window_);
    }

    private updateJoin() {
        const join = this.serverJoin;
        if (!join) return;
        const w = this.worlds.find(x => x.id === this.serverChoice);
        join.disabled = !w || !this.fits(w);
        join.textContent = w && w.id === this.lostWorld ? 'RECONNECT' : 'JOIN';
    }

    /** A world's line, as the client learns more (its status, asked after the list is shown): no page redrawn. */
    updateWorld(id: string, status: string, fit: string, note: string, more: Partial<DoorWorld> = {}) {
        const w = this.worlds.find(x => x.id === id);
        if (w) Object.assign(w, {status, fit, note}, more);
        const line = this.worldLines.get(id);
        if (!line || !w) return;
        line.players.textContent = this.playersText(w);
        line.ping.textContent = this.pingText(w);
        line.note.textContent = note;
        line.row.classList.toggle('unfit', !this.fits(w));
        this.updateJoin();
    }

    /** JOIN on the chosen server: Reconnect takes the same wolf straight back; otherwise the wolf is chosen next. */
    private chooseServer() {
        const w = this.worlds.find(x => x.id === this.serverChoice);
        if (!w || !this.fits(w) || this.busy) return;
        if (w.id === this.lostWorld && this.worldsWolf && this.characters.some(c => str(c, 'id') === this.worldsWolf)) {
            this.selectedId = this.worldsWolf;
            return this.joinWorld(w, this.worldsWolf);
        }
        this.pickWorld = w;
        this.pickAddress = '';
        this.setMessage('');
        this.show('pick');
    }

    /** Which wolf goes to the chosen server. */
    private pickPage(): HTMLElement {
        const w = this.pickWorld;
        const where = w ? w.name : this.pickAddress;
        const grid = el('div', {className: 'pick-grid'});
        for (const c of this.characters) {
            const id = str(c, 'id');
            const face = el('canvas', {className: 'pick-face', width: 320, height: 380});
            const look = readAppearance(obj(c, 'appearance'));
            if (look) {
                const paint = () => {
                    const g = face.getContext('2d');
                    if (!g) return;
                    g.clearRect(0, 0, face.width, face.height);
                    this.wolves.draw(g, 'face', furOf(look), 0, 0, face.width, face.height);
                };
                this.pagePainters.push(paint);
                paint();
            }
            const age = num(c, 'age', 18);
            const card = el('button', {type: 'button', className: `pick-card${id === this.selectedId ? ' chosen' : ''}`},
                face, el('strong', {}, str(c, 'name')), el('span', {}, `${stageName(age)} · age ${age.toFixed(0)}${this.caption(c, ' · ')}`));
            card.dataset.wolf = id;
            card.addEventListener('click', () => {
                this.selectedId = id;
                for (const other of grid.querySelectorAll('.pick-card')) other.classList.toggle('chosen', other === card);
                play.disabled = false;
            });
            card.addEventListener('dblclick', () => { this.selectedId = id; this.play(); });
            grid.append(card);
        }
        const play = this.button('PLAY', () => this.play(), true);
        play.dataset.action = 'play';
        play.disabled = !this.selected();
        const none = this.characters.length ? [] : [el('div', {className: 'pick-none'},
            el('p', {className: 'muted large'}, 'You have no wolves yet. Make one first: their look, their Gift, their strengths and their tie.'),
            this.button('MAKE A WOLF', () => this.startCreation(), true))];
        return el('div', {className: 'servers-screen'},
            el('section', {className: 'server-window pick-window'},
                el('header', {className: 'window-head'}, el('h2', {}, `Who goes to ${where}?`), el('div', {className: 'grow'})),
                el('p', {className: 'muted'}, 'In a world your wolf hasn\'t been to, they take their strengths and their tie from the Character Editor, ' +
                    'and you choose where they arrive. Next time, they go straight in.'),
                el('div', {className: 'server-scroll'}, grid, ...none),
                el('footer', {className: 'window-foot'}, el('div', {className: 'grow'}),
                    el('div', {className: 'door-actions'}, this.backButton('Back to the servers', 'servers', 'pick-back'), play)),
                ...(w && w.kind === 'official' ? [] : [el('p', {className: 'muted small'}, HOST_SEES)])));
    }

    private play() {
        const wolf = this.selected();
        if (!wolf || this.busy) return;
        if (this.pickWorld) return this.joinWorld(this.pickWorld, str(wolf, 'id'));
        if (!this.pickAddress) return;
        this.worldsWolf = str(wolf, 'id');
        this.setMessage(`Asking ${this.pickAddress}…`);
        this.arriveKind = '';
        this.submit({type: 'world_join', address: this.pickAddress, wolf: str(wolf, 'id')});
    }

    private joinWorld(w: DoorWorld, wolf: string) {
        if (this.busy) return;
        this.worldsWolf = wolf;
        this.setMessage(`Joining ${w.name}…`);
        this.arriveKind = w.kind;
        this.submit({type: 'world_join', world: w.id, wolf});
    }

    // ------------------------------------------------------------------ The Host page (central mode)

    /** A host's own worlds (doc 77, 3.2 and 7): each one listed or not and why, where central saw it, its wolves and its
     *  last heartbeat; a new world by name (up to three); a token shown once, with the world agent's command; a new
     *  token, and removing a world, both asked first. Everything central says is shown as text. */
    private hostPage(): HTMLElement {
        const name = el('input', {type: 'text', placeholder: 'e.g. The Long Valley', maxLength: 48});
        name.name = 'worldName';
        const full = this.hostWorlds.length >= this.hostMost;
        const make = el('button', {type: 'submit', className: 'primary', textContent: 'MAKE A WORLD'});
        make.dataset.action = 'host-make';
        make.disabled = full;
        name.disabled = full;
        const form = el('form', {className: 'host-make'},
            el('label', {}, el('span', {}, 'A NEW WORLD · ITS NAME, 1–48 CHARACTERS'), name), make);
        form.addEventListener('submit', e => {
            e.preventDefault();
            if (this.busy || full) return;
            const given = name.value.trim();
            if (!given || given.length > 48) return this.setMessage('Give the world a name of 1–48 characters.', true);
            this.setMessage(`Making ${given}…`);
            this.submit({type: 'host_make', name: given});
        });
        const back = this.backButton('Back to the main menu', 'menu', 'host-back');
        const side = el('section', {className: 'door-card door-preview host-side'},
            el('p', {className: 'gold small'}, 'HOST A WORLD'),
            el('h2', {}, 'Your worlds'),
            el('p', {className: 'muted large'}, 'A world you run on your own computer. Central lists it while it is up and central can ' +
                'reach it from the internet.'),
            el('p', {className: 'muted'}, 'Run it with the world package (docker compose, its README.md says how), with the world\'s token: ' +
                'it tells central the world is up every minute, and this page says why it isn\'t listed if it isn\'t (most often, a port to open).'),
            el('div', {className: 'grow'}),
            el('p', {className: 'muted small'}, full ? `You have ${this.hostWorlds.length} of ${this.hostMost} worlds: remove one to make another.`
                : `${this.hostWorlds.length} of ${this.hostMost} worlds.`),
            form, back);

        const list = el('div', {className: 'world-list'});
        if (!this.hostWorlds.length) list.append(el('p', {className: 'muted large'}, 'No worlds yet. Give one a name to make it and its token.'));
        for (const w of this.hostWorlds) {
            const renew = this.button('NEW TOKEN', () => void this.renewToken(w));
            renew.dataset.action = 'host-token';
            renew.dataset.world = w.id;
            const remove = this.button('REMOVE', () => void this.removeWorld(w));
            remove.dataset.action = 'host-remove';
            remove.dataset.world = w.id;
            remove.classList.add('danger');
            // EDITOR on a community world (never an official one): this computer first, if that world is here.
            const edit: HTMLElement[] = [];
            if (this.editorDoor && w.kind !== 'official') {
                const b = this.button('EDITOR', () => { this.setMessage(''); void this.editorDoor?.openFor(w.id, w.address); });
                b.dataset.action = 'host-editor';
                b.dataset.world = w.id;
                edit.push(b);
            }
            const facts = [w.address || 'Not heard from yet', `${w.players} ${w.players === 1 ? 'wolf' : 'wolves'} in it`,
                `last heartbeat ${agoText(w.lastHeartbeat)}`, ...(w.tokenCreated ? [`token made ${agoText(w.tokenCreated)}`] : ['no token'])];
            const row = el('div', {className: `world-row host-row${w.listed ? ' listed' : ''}`},
                el('div', {className: 'world-text'},
                    el('div', {className: 'world-title'}, el('strong', {}, w.name),
                        el('span', {className: `world-kind ${w.kind === 'official' ? 'official' : ''}`}, w.kind === 'official' ? 'OFFICIAL' : 'COMMUNITY'),
                        el('span', {className: `world-kind ${w.listed ? 'listed' : 'unlisted'}`}, w.listed ? 'LISTED' : 'NOT LISTED')),
                    el('p', {className: 'muted small'}, facts.join('  ·  ')),
                    el('p', {className: 'world-note gold'}, w.listed ? '' : w.why || 'Not listed yet: start the world and its agent.')),
                el('div', {className: 'host-buttons'}, ...edit, renew, remove));
            row.dataset.world = w.id;
            list.append(row);
        }
        const refresh = this.button('Refresh', () => {
            this.setMessage('Asking central for your worlds…');
            this.submit({type: 'host_show'});
        });
        refresh.dataset.action = 'host-refresh';
        // (The token box scrolls with the list, so Refresh and every row's note stay inside the card.)
        const card = el('section', {className: 'door-card door-form door-worlds host-list'},
            el('p', {className: 'gold small'}, 'YOUR WORLDS ON CENTRAL'),
            el('div', {className: 'scroll'}, ...(this.hostToken ? [this.tokenBox()] : []), list),
            el('div', {className: 'door-actions host-actions'}, refresh));
        return el('div', {className: 'door-row'}, side, card);
    }

    /** The token just made, shown this once, and what the world package needs of it (Package/README.md, "Start"): the token
     *  in .env, the world's id and central's address in world.toml, then docker compose up -d (as NEW TOKEN's dialog says). */
    private tokenBox(): HTMLElement {
        const world = this.hostWorlds.find(w => w.id === this.hostTokenWorld);
        const token = el('input', {type: 'text', readOnly: true, value: this.hostToken, className: 'host-token'});
        token.name = 'hostToken';
        token.spellcheck = false;
        token.addEventListener('focus', () => token.select());
        const env = envLine(this.hostToken);
        const toml = worldTomlLines(this.hostTokenWorld, this.hostCentral || 'https://...');
        const shownEnv = el('pre', {className: 'host-command'}, env);
        shownEnv.dataset.command = 'env';
        shownEnv.style.flex = '1';      // (Beside its COPY: styles.css lays out the row for an input.)
        const shownToml = el('pre', {className: 'host-command'}, toml);
        shownToml.dataset.command = 'toml';
        shownToml.style.flex = '1';
        const copyToken = this.button('COPY', () => void this.copy(this.hostToken, 'The token is copied.', token));
        copyToken.dataset.action = 'host-copy';
        const copyEnv = this.button('COPY', () => void this.copy(env, 'The .env line is copied.', null));
        copyEnv.dataset.action = 'host-copy-env';
        const copyToml = this.button('COPY', () => void this.copy(toml, 'The world.toml lines are copied.', null));
        copyToml.dataset.action = 'host-copy-toml';
        return el('div', {className: 'host-token-box'},
            el('p', {className: 'gold small'}, `THE TOKEN FOR ${(world?.name ?? 'YOUR WORLD').toUpperCase()} · SHOWN THIS ONCE`),
            el('div', {className: 'host-token-row'}, token, copyToken),
            el('p', {className: 'muted small'}, 'In your world\'s folder (the world package), put this line in .env, beside compose.yaml:'),
            el('div', {className: 'host-token-row'}, shownEnv, copyEnv),
            el('p', {className: 'muted small'}, 'and these in world.toml (the [world] and [central] parts are already there: fill in id and url):'),
            el('div', {className: 'host-token-row'}, shownToml, copyToml),
            el('p', {className: 'muted small'}, 'Then, in that folder: docker compose up -d. The world goes on the list once central can reach it.'),
            el('p', {className: 'muted small'}, 'Keep the token somewhere safe: it isn\'t shown again. Anyone with it can list a world as this one.'));
    }

    /** Text to the clipboard, the page's way or the old way; or, if neither works, selected for the host to copy. */
    private async copy(text: string, done: string, select: HTMLInputElement | null) {
        try {
            await navigator.clipboard.writeText(text);
            this.setMessage(done);
            return;
        } catch {
            // (Falls back below.)
        }
        const box = el('textarea', {value: text});
        box.style.position = 'fixed';
        box.style.opacity = '0';
        document.body.append(box);
        box.select();
        let copied = false;
        try {
            copied = document.execCommand('copy');
        } catch {
            copied = false;
        }
        box.remove();
        if (copied) return this.setMessage(done);
        select?.focus();
        select?.select();
        this.setMessage('This computer wouldn\'t let the page copy it: select it and press Ctrl+C.', true);
    }

    private async renewToken(w: DoorHostWorld) {
        if (!await ask(`Make a new token for ${w.name}? The old one stops working at once, and the world leaves the list until it ` +
            'restarts with the new one: put it in the world folder\'s .env (RATW_HOST_TOKEN=...), then run docker compose up -d there. ' +
            'Your world\'s Editor and its accounts are unchanged.', {ok: 'Make a new token', danger: true})) return;
        this.setMessage(`Making a new token for ${w.name}…`);
        this.submit({type: 'host_token', world: w.id});
    }

    private async removeWorld(w: DoorHostWorld) {
        if (!await ask(`Remove ${w.name} for good? Its token stops working and central never lists it again. The world's own save, ` +
            'on the computer it runs on, is kept.', {ok: 'Remove the world', danger: true})) return;
        this.setMessage(`Removing ${w.name}…`);
        this.submit({type: 'host_remove', world: w.id});
    }

    // ------------------------------------------------------------------ A first arrival in a world (central mode)

    /** What a world asks when a wolf first comes (doc 77, 3.1 step 5; Q12 (a)): the tier (a wolf with a Gift family
     *  arrives Gifted or Quickened, as this world's standing allows; one without is Normal) and the town. The strengths
     *  and the tie are the wolf's own, from the Character Editor (the user, 2026-11-14). The world opens the purse and
     *  places the wolf. */
    private arrivePage(): HTMLElement {
        const nameHeading = el('h2', {className: 'name'}, this.draftName);
        const summary = el('p', {className: 'sage large pre'}, this.describe());
        const canvas = el('canvas', {className: 'portrait painted', width: 960, height: 660});
        this.portraitCanvas = canvas;
        const tabNames: Array<[string, string]> = [...(this.draftFamily && this.tierChoices.length > 1 ? [['gift', 'Gift'] as [string, string]] : []),
            ['arrival', 'Arrival']];
        if (!tabNames.some(([id]) => id === this.creatorTab)) this.creatorTab = tabNames[0][0];
        const tabs = el('div', {className: 'creator-tabs'});
        const panel = el('div', {className: 'creator-panel'});
        const render = () => {
            summary.textContent = this.describe();
            this.drawPortrait();
            tabs.replaceChildren(...tabNames.map(([id, label]) => {
                const b = el('button', {type: 'button', className: id === this.creatorTab ? 'tab active' : 'tab', textContent: label});
                b.dataset.tab = id;
                b.addEventListener('click', () => {
                    this.creatorTab = id;
                    render();
                });
                return b;
            }));
            const changed = () => render();
            panel.replaceChildren(...(this.creatorTab === 'gift' ? this.arriveGiftPanel(changed) : this.arrivalPanel(changed)));
        };
        render();
        const own = el('div', {className: 'arrive-own'},
            el('span', {className: 'gift-heading'}, 'FROM THE CHARACTER EDITOR'),
            el('p', {className: 'muted'}, this.gradesWords(this.draftGrades)),
            el('p', {className: 'muted'}, `Tie: ${this.draftTie ? this.tieLine(this.draftTie) : 'none'}${this.arriveTieChosen
                ? ' (a first wolf here takes a tie: this one was picked for them)' : ''}`));
        const preview = el('section', {className: 'door-card door-preview'},
            el('p', {className: 'gold small'}, `FIRST TIME IN ${this.arriveWorld.toUpperCase()}`),
            this.wolfId(nameHeading, summary), canvas,
            el('p', {className: 'muted'}, 'What you choose here is this world\'s: another world asks again.'),
            ...(this.arriveKind === 'official' ? [] : [el('p', {className: 'muted small'}, HOST_SEES)]));
        const actions = el('div', {className: 'door-actions'},
            this.button('Leave', () => this.submit({type: 'arrive_cancel'})),
            this.button('ARRIVE', () => this.arrive(), true));
        const form = el('section', {className: 'door-card door-form'},
            el('div', {className: 'scroll'}, el('div', {className: 'door-fields'}, el('div', {className: 'creator'}, tabs, panel, own))), actions);
        return el('div', {className: 'door-row'}, preview, form);
    }

    /** The arrive page's line: what there is to choose in this world, and only that. */
    private arriveWelcome(): string {
        const choices = [...(this.draftFamily && this.tierChoices.length > 1 ? ['how their Gift shows here'] : []),
            ...(this.starts.length > 1 ? ['where they arrive'] : [])];
        const list = choices.length > 1 ? `${choices.slice(0, -1).join(', ')} and ${choices.at(-1)}` : choices[0];
        return list ? `Welcome to ${this.arriveWorld || 'this world'}. Choose ${list}.` : `Welcome to ${this.arriveWorld || 'this world'}.`;
    }

    /** The tier at arrival: the wolf's family is fixed (central's); Gifted or Quickened as the world offers them. */
    private arriveGiftPanel(changed: () => void): HTMLElement[] {
        const tiers = obj(this.gifts, 'tiers');
        const family = arr(this.gifts, 'families').filter(isObject).find(f => str(f, 'id') === this.draftFamily);
        const row = el('div', {className: 'gift-tiers'});
        for (const tier of this.tierChoices) {
            const t = obj(tiers, tier);
            const locked = this.arriveLocked(tier);
            const b = el('button', {type: 'button', className: `gift-card${this.draftTier === tier ? ' chosen' : ''}${locked ? ' locked' : ''}`},
                el('strong', {}, `${locked ? '🔒 ' : ''}${str(t, 'name', title(tier))}`), el('span', {}, str(obj(family, tier), 'best') || str(t, 'best')),
                ...(locked ? arr(obj(this.tiers, tier), 'progress').filter(isObject).map(p => el('small', {className: 'gift-progress'}, str(p, 'label'))) : []));
            b.dataset.tier = tier;
            b.disabled = locked;
            b.addEventListener('click', () => {
                this.draftTier = tier;
                changed();
            });
            row.append(b);
        }
        const out: HTMLElement[] = [el('p', {className: 'muted'}, `Your wolf's Gift is the ${str(family, 'name', title(this.draftFamily))} family. ` +
            'Here they arrive Gifted or Quickened: Quickened opens with standing earned in this world.'),
            el('span', {className: 'gift-heading'}, 'IN THIS WORLD'), row];
        if (family) out.push(this.giftDetails(family));
        return out;
    }

    private arrive() {
        if (this.busy || this.page !== 'arrive') return;
        if (this.creation && !this.buildFits(this.draftGrades)) {
            this.draftGrades = {};
            this.setMessage('Your wolf\'s strengths cost more points than this world allows: they arrive plain here.', true);
        }
        if (this.arriveLocked(this.draftTier)) {
            this.creatorTab = 'gift';
            this.show('arrive');
            return this.setMessage(str(obj(this.tiers, this.draftTier), 'message', `${title(this.draftTier)} isn't open to your account here yet.`), true);
        }
        const command: Json = {type: 'character_arrive', tier: this.draftTier};
        if (this.draftStart && this.starts.some(t => str(t, 'id') === this.draftStart)) command.start = this.draftStart;
        if (this.draftTie) command.tie = this.draftTie;
        if (Object.keys(this.draftGrades).length) command.build = {grades: {...this.draftGrades}};
        const fingerprint = JSON.stringify(command);
        if (!this.creationRequestId || this.creationFingerprint !== fingerprint) {
            this.creationRequestId = newCommandId();
            this.creationFingerprint = fingerprint;
        }
        this.setMessage('Arriving…');
        this.submit({...command, commandId: this.creationRequestId});
    }

    // ------------------------------------------------------------------ Creating a character

    private startCreation() {
        if (this.busy || this.characters.length >= 6) return;
        this.draftName = '';
        this.draftAge = 18;
        this.draftTier = 'normal';
        this.draftFamily = '';
        this.draftGrades = {};
        this.draftStart = str(this.starts.find(t => t.suggested === true) ?? this.starts[0], 'id');
        this.draftTie = this.randomTie('');          // (A suggestion, different for every character: the user.)
        this.creationRequestId = this.creationFingerprint = '';
        this.openZone = '';
        this.draftAppearance = {species: 'timber', sex: 'female', stature: 'average', pattern: 'solid', baseColor: 2, gradientColor: 0,
            markingColor: 5, gradientAmount: 0.35, patternAmount: 0.65};
        // A new wolf starts in the first ready coat, every fur zone in the coat's colours (the user, 2026-11-14).
        wear(this.draftAppearance, readyFur(ReadyCoats[0]), 'medium');
        this.creatorTab = 'body';
        this.setMessage('Appearance changes are a live preview. Nothing is saved until you confirm creation.');
        this.show('creator');
    }

    /** A page the tools ask for (the Unreal client's SetPresentationPage): only one the account's state allows. */
    presentationPage(page: string) {
        if (page === 'creator') this.startCreation();
        else if ((page === 'roster' || page === 'menu') && this.stage === 'characters') this.show(page);
        else if (page === 'review' && this.draftAppearance) this.review();
        else if ((page === 'login' || page === 'register') && this.stage === 'login') this.show(page);
    }

    /** A draft from a tool (the Unreal client's SetCharacterDraft): a preview only, never created without review. */
    setDraft(name: string, age: number, appearance: Json) {
        if (this.busy || age < 6 || age > 99 || !readAppearance(appearance)) return;
        this.draftName = name.trim().slice(0, this.nameMost);
        this.draftAge = age;
        this.draftAppearance = {...appearance};
        this.creationRequestId = this.creationFingerprint = '';
        this.setMessage('Appearance preview only. Review and confirm to create this character.');
        this.show('creator');
    }

    /** A wolf's face as its own portrait, large, beside its name and a line or two (doc 81; the user, 2026-11-14: a
     *  bigger face): the head of a preview card. The profile is the card's other picture, below. */
    private wolfId(...text: HTMLElement[]): HTMLElement {
        const face = el('canvas', {className: 'wolf-face', width: 440, height: 524});
        this.faceCanvas = face;
        return el('div', {className: 'wolf-id'}, face, el('div', {className: 'wolf-id-text'}, ...text));
    }

    private describe(): string {
        return `${stageName(this.draftAge)} · age ${this.draftAge}\n${sizeWords(this.draftAppearance, this.draftAge)}` +
            (this.mode === 'central' && this.page !== 'arrive' ? this.caption({family: this.draftFamily}, '\n')
                : giftCaption({gift: this.draftFamily, quickened: this.draftTier === 'quickened'}, this.gifts, '\n'));
    }

    private creatorPage(reviewOnly: boolean): HTMLElement {
        const draft = this.draftAppearance ?? {};
        const nameHeading = el('h2', {className: 'name'}, this.draftName || 'Your wolf');
        const summary = el('p', {className: 'sage large pre'}, this.describe());
        const canvas = el('canvas', {className: 'portrait painted', width: 960, height: 660});
        this.portraitCanvas = canvas;
        const refresh = () => {
            summary.textContent = this.describe();
            this.drawPortrait();
        };
        const fields = el('div', {className: 'door-fields'});
        // Colours in words, never their codes (R1): the palette's names.
        const fur = draftFur(draft);
        const zones = Object.keys(fur.zones ?? {}).map(zoneName);
        const coatLines = [el('p', {className: 'muted'}, `Coat: ${coatWords(fur.coat)} · ${eyeName(fur.eyes).toLowerCase()} eyes`),
            el('p', {className: 'muted'}, zones.length ? `With their own colours: ${zones.join(', ').toLowerCase()}` : 'One coat all over.')];
        if (!reviewOnly) fields.append(this.creatorFields(draft, refresh, nameHeading));
        else if (this.mode === 'central') {
            const family = arr(this.gifts, 'families').filter(isObject).find(f => str(f, 'id') === this.draftFamily);
            fields.append(
                el('h2', {}, 'Review your wolf'),
                el('p', {className: 'gold name'}, this.draftName),
                el('p', {className: 'large pre'}, `${title(str(draft, 'sex'))}\nAge ${this.draftAge} · ${stageName(this.draftAge)}\n${sizeWords(draft, this.draftAge)}`),
                ...coatLines,
                el('p', {className: family ? 'gold' : 'muted'}, family ? `Gift: the ${str(family, 'name')} family. In each world your wolf arrives Gifted, or Quickened where that world allows.`
                    : 'Gift: none. Your wolf is Normal in every world.'),
                ...this.buildReview(),
                ...(this.ties.length ? [el('p', {className: 'muted'}, `Tie: ${this.draftTie ? this.tieLine(this.draftTie) : 'none'}`)] : []),
                el('p', {className: 'muted large'}, 'Creating saves this wolf to your account on central: their name, age, sex and Gift never change; their ' +
                    'look, strengths and tie can. Each world takes the strengths and the tie at their first arrival there.'));
        } else {
            fields.append(
                el('h2', {}, 'Review your wolf'),
                el('p', {className: 'gold name'}, this.draftName),
                el('p', {className: 'large pre'}, `${title(str(draft, 'sex'))}\nAge ${this.draftAge} · ${stageName(this.draftAge)}\n${sizeWords(draft, this.draftAge)}`),
                ...coatLines,
                ...this.giftReview(),
                ...this.buildReview(),
                ...(this.draftStart ? [el('p', {className: 'muted'}, `Arrives in: ${this.startName(this.draftStart)}`)] : []),
                ...(this.ties.length ? [el('p', {className: 'muted'}, `Tie: ${this.draftTie ? this.tieLine(this.draftTie) : 'none'}`)] : []),
                el('p', {className: 'muted large'}, 'Creation saves this character to your account. You will return to character selection before ' +
                    'entering the world. Appearance does not grant free skill or stat bonuses.'));
        }
        const preview = el('section', {className: 'door-card door-preview'},
            el('p', {className: 'gold small'}, reviewOnly ? 'CREATION REVIEW' : 'LIVE APPEARANCE PREVIEW'),
            this.wolfId(nameHeading, summary), canvas,
            el('p', {className: 'muted pre'}, 'Young 6–12 · Adolescent 13–17 · Adult 18–64 · Old 65+'));
        const back = reviewOnly ? this.backButton('Back to appearance', 'creator')
            : this.mode === 'central' ? this.backButton('Back to the main menu', 'menu', 'menu') : this.backButton('Back', 'roster');
        const actions = el('div', {className: 'door-actions'}, back,
            this.button(reviewOnly ? 'CONFIRM & CREATE' : 'REVIEW CHARACTER', () => (reviewOnly ? this.create() : this.review()), true));
        const form = el('section', {className: 'door-card door-form'}, el('div', {className: 'scroll'}, fields), actions);
        return el('div', {className: 'door-row'}, preview, form);
    }

    private creatorTab = 'body';

    /** The creator's steps, in tabs (doc 29, phase 9; doc 81; the user, 2026-11-14): body, coat, fur zones, eyes, the
     *  Gift, strengths, the tie, name and age (and on a world, where the wolf arrives). */
    private creatorFields(draft: Json, refresh: () => void, nameHeading: HTMLElement): HTMLElement {
        const wrap = el('div', {className: 'creator'});
        const tabs = el('div', {className: 'creator-tabs'});
        const panel = el('div', {className: 'creator-panel'});
        const tabNames: Array<[string, string]> = this.mode === 'central'
            ? [['body', 'Body'], ['coat', 'Coat'], ['markings', 'Fur zones'], ['eyes', 'Eyes'], ['gift', 'Gift'], ['strengths', 'Strengths'],
                ['tie', 'Tie'], ['name', 'Name & age']]
            : [['body', 'Body'], ['coat', 'Coat'], ['markings', 'Fur zones'], ['eyes', 'Eyes'], ['gift', 'Gift'],
                ['strengths', 'Strengths'], ...(this.starts.length ? [['arrival', 'Arrival'] as [string, string]] : []),
                ...(this.ties.length ? [['tie', 'Tie'] as [string, string]] : []), ['name', 'Name & age']];
        if (!tabNames.some(([id]) => id === this.creatorTab)) this.creatorTab = 'body';
        const render = () => {
            tabs.replaceChildren(...tabNames.map(([id, label]) => {
                const b = el('button', {type: 'button', className: id === this.creatorTab ? 'tab active' : 'tab', textContent: label});
                b.dataset.tab = id;
                b.addEventListener('click', () => {
                    this.creatorTab = id;
                    render();
                });
                return b;
            }));
            this.panelPainters = [];
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
        const fur = draftFur(draft), build = draftBuild(draft);
        const wearing = (next: Fur, nextBuild: Build = build) => {
            wear(draft, next, nextBuild);
            changed();
        };
        const swatch = (id: string, name: string, hex: string, chosen: boolean, choose: () => void) => {
            const b = el('button', {type: 'button', title: name, className: chosen ? 'swatch chosen' : 'swatch'});
            b.style.background = hex;
            b.dataset.colour = id;
            b.addEventListener('click', choose);
            return b;
        };
        const dot = (id: string) => {
            const i = el('i', {title: furName(id)});
            i.style.background = furHex(id);
            return i;
        };
        const palette = (chosen: string, choose: (id: string) => void) => {
            const grid = el('div', {className: 'swatch-grid fur-grid'});
            for (const c of FurColours) grid.append(swatch(c.id, c.name, c.hex, chosen === c.id, () => choose(c.id)));
            return grid;
        };
        // Three fur colours (the painting's dark, mid and light fur): a chip each, and the palette for the one in hand.
        const chips = (key: string, coat: FurCoat, active: number | null, pick: (slot: number) => void) => {
            const row = el('div', {className: 'fur-chips'});
            ['Dark', 'Mid', 'Light'].forEach((label, i) => {
                const chip = el('button', {type: 'button', className: i === active ? 'fur-chip active' : 'fur-chip'},
                    dot(coat[i]), el('span', {}, `${label} fur`), el('strong', {}, furName(coat[i])));
                chip.dataset.slot = String(i);
                chip.dataset.key = key;
                chip.addEventListener('click', () => pick(i));
                row.append(chip);
            });
            return row;
        };
        if (tab === 'body') {
            // The three sizes, each in the coat chosen so far, at the sizes they were painted.
            const cards = el('div', {className: 'build-cards'});
            for (const b of Builds) {
                const canvas = el('canvas', {width: 360, height: 230});
                const card = el('button', {type: 'button', className: b === build ? 'build-card chosen' : 'build-card'}, canvas, el('span', {}, BuildNames[b]));
                card.dataset.build = b;
                card.addEventListener('click', () => wearing(fur, b));
                const paint = () => {
                    const g = canvas.getContext('2d');
                    if (!g) return;
                    g.clearRect(0, 0, canvas.width, canvas.height);
                    this.wolves.draw(g, b, fur, 0, 0, canvas.width, canvas.height);
                };
                this.panelPainters.push(paint);
                paint();
                cards.append(card);
            }
            return [el('div', {className: 'field'}, el('span', {}, `SIZE · ${BuildNames[build].toUpperCase()}`), cards),
                el('div', {className: 'pair'}, choice('sex', 'SEX', Choices.sex)),
                el('p', {className: 'muted'}, 'The size is the wolf you see: its painting, its stature and its height at the shoulder. None of it changes skills.')];
        }
        if (tab === 'coat') {
            const ready = el('div', {className: 'ready-coats'});
            for (const c of ReadyCoats) {
                const b = el('button', {type: 'button', className: 'ready-coat'}, el('span', {className: 'fur-dots'}, ...c.coat.map(dot)), el('span', {}, c.name));
                b.dataset.coat = c.id;
                b.addEventListener('click', () => wearing(readyFur(c)));
                ready.append(b);
            }
            const slot = Math.min(2, Math.max(0, this.furSlot.coat ?? 0));
            return [el('div', {className: 'field'}, el('span', {}, 'A COAT TO START FROM'), ready),
                el('div', {className: 'field'}, el('span', {}, 'THE COAT · THREE FUR COLOURS'),
                    el('div', {className: 'fur-picker'},
                        chips('coat', fur.coat, slot, i => { this.furSlot.coat = i; changed(); }),
                        palette(fur.coat[slot], id => {
                            const coat: FurCoat = [...fur.coat];
                            coat[slot] = id;
                            wearing({...fur, coat});
                        }))),
                el('p', {className: 'muted'}, 'The painting keeps its brushwork: its dark, mid and light fur take your three colours. Every fur zone wears ' +
                    'them too, until you change it in Fur zones.')];
        }
        if (tab === 'markings') {                  // (The tab keeps its older id; it is the fur zones now.)
            // Every zone wears the coat's colours; change one of its three right here, and Reset gives it the coat's again
            // (the user, 2026-11-14: no dropdown).
            const rows: HTMLElement[] = [el('p', {className: 'muted'}, 'Every part of the coat starts in the coat\'s colours. Pick a part\'s dark, mid or ' +
                'light fur to change it; Reset gives it the coat\'s again.')];
            for (const z of FurZones) {
                const own = zoneChanged(fur, z.id);
                const coat = zoneCoat(fur, z.id);
                const key = `zone:${z.id}`;
                const open = this.openZone === z.id;
                const slot = Math.min(2, Math.max(0, this.furSlot[key] ?? 0));
                const reset = el('button', {type: 'button', className: 'zone-reset', textContent: 'Reset', title: 'Back to the coat\'s colours'});
                reset.dataset.zone = z.id;
                reset.disabled = !own;
                reset.addEventListener('click', () => {
                    if (this.openZone === z.id) this.openZone = '';
                    wearing(resetZone(fur, z.id));
                });
                const head = el('div', {className: 'zone-head'},
                    el('div', {className: 'zone-title'}, el('strong', {}, z.name), el('small', {className: own ? 'sage' : 'muted'}, own ? 'Its own colours' : 'As the coat')),
                    chips(key, coat, open ? slot : null, i => {
                        const again = open && slot === i;
                        this.openZone = again ? '' : z.id;
                        this.furSlot[key] = i;
                        changed();
                    }),
                    reset);
                const row = el('div', {className: `zone-row${open ? ' open' : ''}${own ? ' own' : ''}`}, head);
                row.dataset.zone = z.id;
                if (open) {
                    row.append(palette(coat[slot], id => wearing(setZoneColour(fur, z.id, slot, id))));
                    queueMicrotask(() => row.scrollIntoView({block: 'nearest'}));      // (Its palette, in sight.)
                }
                rows.push(row);
            }
            return rows;
        }
        if (tab === 'eyes') {
            const grid = el('div', {className: 'swatch-grid eye-grid'});
            for (const c of EyeColours) grid.append(swatch(c.id, c.name, c.hex, fur.eyes === c.id, () => wearing({...fur, eyes: c.id})));
            return [el('div', {className: 'field'}, el('span', {}, `EYES · ${eyeName(fur.eyes).toUpperCase()}`), grid)];
        }
        if (tab === 'gift') return this.giftPanel(changed);
        if (tab === 'strengths') return this.strengthsPanel(changed);
        if (tab === 'arrival') return this.arrivalPanel(changed);
        if (tab === 'tie') return this.tiePanel(changed);
        if (tab === 'name') {
            const most = this.nameMost;
            const name = el('input', {type: 'text', value: this.draftName, placeholder: 'The name others will know', maxLength: most});
            name.name = 'wolfName';
            const count = el('small', {className: 'name-count muted'}, `${this.draftName.length} / ${most}`);
            name.addEventListener('input', () => {
                // A name always starts with a capital (the server makes it so too).
                const capital = name.value.charAt(0).toUpperCase() + name.value.slice(1);
                if (capital !== name.value) {
                    const at = name.selectionStart;
                    name.value = capital;
                    name.setSelectionRange(at, at);
                }
                if (name.value.length > most) name.value = name.value.slice(0, most);
                this.draftName = name.value;
                count.textContent = `${this.draftName.length} / ${most}`;
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
            return [el('label', {}, el('span', {}, `CHARACTER NAME · 2–${most} CHARACTERS`), name, count),
                el('label', {}, el('span', {}, 'STARTING AGE · 6–99'), age)];
        }
        return [];
    }

    /** The Gift step (doc 43; the user, 2026-11-14): three cards, Normal, Gifted and Quickened, each greyed with the
     *  reason where the account hasn't opened it (creatorRules.ts, tierCards: the game's unlock rules); then one of the
     *  eight families, with every ability it gives. */
    private giftPanel(changed: () => void): HTMLElement[] {
        const tiers = obj(this.gifts, 'tiers');
        const families = arr(this.gifts, 'families').filter(isObject);
        const cards = tierCards(this.mode, this.tiers, this.unlocks);
        // A tier chosen before its lock was known (a draft from a tool, a lobby that came later) goes back to Normal.
        if (cards.some(c => c.tier === this.draftTier && !c.open)) {
            this.draftTier = 'normal';
            this.draftFamily = '';
        }
        const tierRow = el('div', {className: 'gift-tiers'});
        for (const card of cards) {
            const t = obj(tiers, card.tier);
            const b = el('button', {type: 'button', className: `gift-card${this.draftTier === card.tier ? ' chosen' : ''}${card.open ? '' : ' locked'}`},
                el('strong', {}, `${card.open ? '' : '🔒 '}${str(t, 'name', title(card.tier))}`),
                el('span', {}, str(t, 'best', card.tier === 'normal' ? 'No Gift: most wolves.' : '')),
                ...(card.open ? [] : [el('small', {className: 'gift-reason'}, card.reason)]),
                ...card.progress.map(p => el('small', {className: 'gift-progress'}, p)));
            b.dataset.tier = card.tier;
            b.disabled = !card.open;
            if (!card.open) b.title = card.reason;
            b.addEventListener('click', () => {
                if (!card.open) return;
                this.draftTier = card.tier;
                if (card.tier === 'normal') this.draftFamily = '';
                changed();
            });
            tierRow.append(b);
        }
        const out: HTMLElement[] = [el('span', {className: 'gift-heading'}, 'CLASSIFICATION'), tierRow];
        if (this.draftTier === 'normal') {
            out.push(el('p', {className: 'muted'}, this.mode === 'central'
                ? 'Most wolves have no Gift: they are Normal in every world. Choose Gifted to give your wolf one of the eight Gift families.'
                : 'Most wolves have no Gift. Choose Gifted or Quickened to pick one of the eight Gift families.'));
            return out;
        }
        if (!families.length) {
            out.push(el('p', {className: 'gold'}, 'The Gift families have not arrived yet.'));
            return out;
        }
        const grid = el('div', {className: 'gift-families'});
        for (const f of families) {
            const id = str(f, 'id');
            const b = el('button', {type: 'button', className: `gift-family${this.draftFamily === id ? ' chosen' : ''}`},
                el('strong', {}, str(f, 'name')), el('span', {}, str(obj(f, this.draftTier), 'best') || str(f, 'domain')));
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
            if (this.showGiftDetails) requestAnimationFrame(() => details.scrollIntoView({block: 'nearest'}));   // (A family just chosen.)
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

    /** The Strengths step (doc 49): a preset or one's own build, each attribute weak, plain or strong (each with where
     *  it starts and how far it can grow), and the points left. (No specialty: it did nothing, the user 2026-11-14.) */
    private strengthsPanel(changed: () => void): HTMLElement[] {
        const c = this.creation;
        if (!c) return [el('p', {className: 'gold'}, 'Strengths and weaknesses have not arrived yet.')];
        const out: HTMLElement[] = [];
        const presets = arr(c, 'presets').filter(isObject);
        const chips = el('div', {className: 'build-presets'});
        const presetGrades = (p: Json) => {
            const g: Record<string, string> = {};
            for (const a of arr(p, 'strong')) if (typeof a === 'string') g[a] = 'strong';
            for (const a of arr(p, 'weak')) if (typeof a === 'string') g[a] = 'weak';
            return g;
        };
        const sameAs = (p: Json) => JSON.stringify(Object.entries(presetGrades(p)).sort()) === JSON.stringify(Object.entries(this.draftGrades).sort());
        for (const p of presets) {
            const b = el('button', {type: 'button', className: `build-chip${sameAs(p) ? ' chosen' : ''}`, textContent: str(p, 'name')});
            b.dataset.preset = str(p, 'id');
            b.addEventListener('click', () => {
                this.draftGrades = presetGrades(p);
                changed();
            });
            chips.append(b);
        }
        const own = el('button', {type: 'button', className: `build-chip${presets.some(sameAs) ? '' : ' chosen'}`, textContent: 'Build my own'});
        own.addEventListener('click', () => {
            this.draftGrades = {};
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
        return out;
    }

    /** The review's lines about strengths and weaknesses. */
    private buildReview(): HTMLElement[] {
        return [el('p', {className: 'muted'}, this.gradesWords(this.draftGrades))];
    }

    /** The Arrival tab (doc 52): the three start towns, the busiest lately preselected; any may be chosen (a first
     *  character too, say to join a friend: the user). */
    private arrivalPanel(changed: () => void): HTMLElement[] {
        const suggested = this.starts.find(t => t.suggested === true);
        const out: HTMLElement[] = [];
        if (!this.starts.length) return [el('p', {className: 'muted'}, 'This world has one place to arrive.')];
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

    /** The Tie step (doc 52): a story starter linking this wolf to a mentor or a resident: one suggested at random,
     *  Reroll, or any from the list; on a world a first wolf must take one (the user), later ones may take none. On
     *  central it is the wolf's own (a world whose first wolf this is picks one if it has none). */
    private tiePanel(changed: () => void): HTMLElement[] {
        const required = this.mode === 'world' && this.tieRequired;
        if (!this.ties.length) return [el('p', {className: 'gold'}, 'The story starters have not arrived yet.')];
        const out: HTMLElement[] = [el('p', {className: 'sage'}, required
            ? 'Your first wolf arrives with a tie: a story that links them to someone here, a mentor if one is free, else someone who lives in town. ' +
              'You are both told, and where to find each other.'
            : 'A tie links your wolf to someone in each world they arrive in: a mentor if one is free, else someone who lives in town. Take one, or none.')];
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
        if (!required) {
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

    /** At a first arrival only Quickened waits on this world's standing: a wolf with a family can't arrive Normal, so
     *  Gifted is always open to it (doc 77, Phase 2; Core/RatwGameTickets.cpp, arrive). */
    private arriveLocked(tier: string): boolean {
        if (tier !== 'quickened') return false;
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

    /** The draft's painted look written down, if it never was (a draft no colour was picked for): a wolf is kept with
     *  the fur it was shown in. */
    private settleLook() {
        const d = this.draftAppearance;
        if (d && !isObject(d.fur) && readAppearance(d)) wear(d, draftFur(d), draftBuild(d));
    }

    /** A step that needs fixing: its tab, and why. */
    private sendBack(tab: string, why: string) {
        this.creatorTab = tab;
        this.show('creator');
        this.setMessage(why, true);
    }

    review() {
        this.draftName = this.draftName.trim();
        const nameWrong = nameProblem(this.draftName, this.nameMost);
        if (nameWrong) return this.sendBack('name', `${nameWrong} The authority validates the final name.`);
        const locked = tierCards(this.mode, this.tiers, this.unlocks).find(c => c.tier === this.draftTier && !c.open);
        if (locked) return this.sendBack('gift', locked.reason);
        if (this.draftTier !== 'normal' && !this.draftFamily)
            return this.sendBack('gift', `A ${title(this.draftTier)} wolf needs a Gift family: choose one of the eight.`);
        if (this.creation && !this.buildFits(this.draftGrades))
            return this.sendBack('strengths', 'That build costs more points than there are (a Gift tier may cost some too).');
        if (this.mode === 'world' && this.tieRequired && !this.draftTie)
            return this.sendBack('tie', 'Your first wolf takes a tie: pick a story starter, or keep the one suggested.');
        this.settleLook();
        this.setMessage(this.mode === 'central' ? 'Review your wolf. Creating saves them to your account; they enter no world yet.'
            : 'Review your choices. Confirming creates a persistent character; it does not enter the world yet.');
        this.show('review');
    }

    create() {
        if (this.busy || !this.draftAppearance || this.page !== 'review') return;
        this.settleLook();
        if (this.mode === 'central') {
            const wolf: Json = {type: 'wolf_create', name: this.draftName, age: this.draftAge, appearance: this.draftAppearance,
                choices: {grades: {...this.draftGrades}, tie: this.draftTie}};
            if (this.draftFamily) wolf.family = this.draftFamily;
            this.setMessage('Creating your wolf…');
            this.submit(wolf);
            return;
        }
        const command: Json = {type: 'character_create', name: this.draftName, age: this.draftAge, appearance: this.draftAppearance};
        if (this.draftTier !== 'normal') command.gift = {tier: this.draftTier, family: this.draftFamily};
        if (this.draftStart && this.starts.some(t => str(t, 'id') === this.draftStart)) command.start = this.draftStart;
        if (this.draftTie) command.tie = this.draftTie;
        if (Object.keys(this.draftGrades).length) command.build = {grades: {...this.draftGrades}};    // (A plain wolf sends no build: doc 49.)
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
        const creating = this.page === 'creator' || this.page === 'review' || this.page === 'arrive';
        const wolf = this.portraitWolf ?? this.selected();
        const appearance = creating ? this.draftAppearance : obj(wolf, 'appearance');
        const age = creating ? this.draftAge : num(wolf, 'age', 18);
        const artwork = creating ? undefined : str(wolf, 'artwork') || undefined;
        // A player's own picture where there is one to show (doc 29, phase 9); else the painted wolf (doc 81): its
        // profile, in its fur, on nothing. (Nothing until the paintings have arrived: they draw this again.)
        const look = readAppearance(appearance);
        if (artwork && artCache.get(artwork)) drawPortrait(c, this.portraits, appearance, 0, 0, canvas.width, canvas.height, artwork);
        else if (!look) {
            c.fillStyle = '#a6a699';
            c.font = '32px sans-serif';
            c.fillText('Portrait unavailable', 24, 48);
        } else this.wolves.draw(c, buildOf(look), furOf(look), 0, 0, canvas.width, canvas.height);
        canvas.dataset.stage = lifeStage(age);
        // The face, where the page shows it as its own portrait.
        const face = this.faceCanvas, f = face?.getContext('2d');
        if (face && f) {
            f.clearRect(0, 0, face.width, face.height);
            if (look) this.wolves.draw(f, 'face', furOf(look), 0, 0, face.width, face.height);
        }
    }
}

/** A wolf's saved grades, only "weak" and "strong" (plain is none). */
function gradesOf(grades: Json | null): Record<string, string> {
    const out: Record<string, string> = {};
    for (const [attribute, grade] of Object.entries(grades ?? {})) if (grade === 'weak' || grade === 'strong') out[attribute] = grade;
    return out;
}
