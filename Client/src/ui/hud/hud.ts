// The screen around the map (Docs/Design/29-client-polish.md, phase 2; the user's notes, 2026-11-14): the story column
// on the left (story.ts), the map in the middle under its "Your surroundings" bar (the place, the time of day and the
// weather as pictures, the senses), and on the right the pages' buttons, the minimap, who is in sight and how the wolf is
// doing. No top bar, no actions panel, no bottom bar: what one does is a slash command or on the wheel a right-click
// opens (game/commands.ts). Plain HTML, updated in place each frame; the map itself is the canvas (game/paint.ts).
import {css, rgb} from '../color.ts';
import {Amber, Blue, Sage} from '../theme.ts';
import {drawPortrait, type Portraits} from '../portrait.ts';
import {arr, bool, boundedNum, clamp, envNumber, isObject, num, obj, str} from '../../game/json.ts';
import {dayLabel, lawLabel, paceLabel, postureLabel, restLabel, scentLabel, readLoad, loadCost} from '../../game/labels.ts';
import {GroundTarget, type EntityView, type GameState} from '../../game/state.ts';
import {Commands} from '../../game/commands.ts';
import {NavIcons, SenseIcons, TimeIcons, WeatherIcons, senseLevel, timeOfDay, weatherIcon, type NavIcon, type Sense} from './hudIcons.ts';
import './hud.css';
import {describeWolf, lookAt, type Look} from '../../game/look.ts';
import {Dialogs} from './dialogs.ts';
import {MapRenderer, MapScales} from '../../game/minimap.ts';
import {artCache} from '../artwork.ts';
import {drawGear, gearDoll} from './gear.ts';
import {pageSurface} from '../../game/terrainLayer.ts';
import {button, el, setClass, setStyle, setText, show} from './dom.ts';
import {FightPanel} from './fight.ts';
import {TiePanel} from './tie.ts';
import {CombatScreen} from './combat.ts';
import {PartyPanel} from './party.ts';
import {PlacePanel} from './place.ts';
import {LodgingPanel} from './lodging.ts';
import {StallPanel} from './stall.ts';
import {ProjectPanel} from './project.ts';
import {TablePanel} from './table.ts';
import {FestivalPanel} from './festival.ts';
import {CampPanel} from './camp.ts';
import {DevConsole} from './devConsole.ts';
import {noRect, StoryPanel} from './story.ts';

const HostileRed = rgb(0xe0695e);
const upperFirst = (s: string) => (s ? s[0].toUpperCase() + s.slice(1) : s);
const Arrows = ['→', '↘', '↓', '↙', '←', '↖', '↑', '↗'];
/** Menu entries that aren't their own action's name: a challenge's terms (doc 37). */
const MenuWords: Record<string, string> = {'challenge:yield': 'Duel until one yields', 'challenge:blood': 'Duel to first blood',
    'challenge:spar': 'Spar (bruises only)', 'challenge:death': 'Fight until one goes down'};
const MapKeys = 'WASD move · CLICK walk there · RIGHT-CLICK what you can do here, or with someone · ALT+CLICK turn · WHEEL / PgUp PgDn pace · ' +
    'SHIFT/CTRL+WHEEL pan · +/− zoom · M map · E nearest · L listen · C character · I belongings · ENTER write · / commands';
const FightKeys = 'CLICK a tile to move (waiting: to plan) · CLICK a foe to strike · WHEEL pace · 1–9 actions · C stalk · Z aim · G guard · ' +
    'F shove · R rest · SPACE end turn · Q / E turn · ENTER write';

const icons = new WeakMap<HTMLElement, string>();
/** Puts one of the pictures (hudIcons.ts) in an element, only when it changed, with its words for the pointer. */
function setIcon(e: HTMLElement, svg: string, words: string) {
    if (icons.get(e) !== svg) {
        icons.set(e, svg);
        e.innerHTML = svg;
    }
    if (e.title !== words) e.title = words;
    if (e.getAttribute('aria-label') !== words) e.setAttribute('aria-label', words);
}

/** "Tell of Ash" / "Vouch for Ash": the Wardens' menu (doc 53, 4), by the names the wolf knows them by. */
function wardensWords(s: GameState, action: string): string {
    const [verb, id] = action.slice('wardens-'.length).split(':');
    const seen = arr(obj(s.snapshot, 'self'), 'witnessed').filter(isObject).find(w => str(w, 'id') === id);
    const name = seen ? str(seen, 'name') : 'them';
    return verb === 'tell' ? `Tell the Wardens of ${name}` : `Vouch for ${name} to the Wardens`;
}

/** A player's status in words (doc 50): in character needs none. */
export const StatusWords: Record<string, string> = {ooc: 'out of character', lfs: 'looking for a scene', quill: 'storyteller'};

/** One row of the In Sight list. */
interface SightRow {
    row: HTMLElement;
    portrait: HTMLCanvasElement;
    name: HTMLElement;
    detail: HTMLElement;
    where: HTMLElement;
    drawn: string;              // The look last drawn into the portrait.
    gear: HTMLElement;          // Their armour and weapons on a little doll (doc 35), shown when they have any.
    gearKey: string;
}

export class Hud {
    readonly root: HTMLElement;
    readonly story: StoryPanel;
    readonly mapWrap: HTMLElement;
    readonly canvas: HTMLCanvasElement;
    private s: GameState;
    private portraits: Portraits;
    private dialogs: Dialogs;
    // The surroundings bar: the place, the time of day, the weather, what kind of day, the senses.
    private mapPlace: HTMLElement;
    private timeIcon: HTMLElement;
    private weatherIcon: HTMLElement;
    private day: HTMLElement;
    private senseIcons = new Map<Sense, HTMLElement>();
    private keysHelp: HTMLElement;
    private localTab: HTMLButtonElement;
    private worldTab: HTMLButtonElement;
    readonly looking: HTMLElement;
    // The side
    private sightList: HTMLElement;
    private sightEmpty: HTMLElement;
    private sightCount: HTMLElement;
    private rows = new Map<string, SightRow>();
    private paceLabel: HTMLElement;
    private paceNote: HTMLElement;
    private paceSteps: HTMLElement[] = [];
    private staminaLabel: HTMLElement;
    private staminaNote: HTMLElement;
    private staminaFill: HTMLElement;
    private healthLabel: HTMLElement;
    private healthFill: HTMLElement;
    private manaRow: HTMLElement;
    private manaLabel: HTMLElement;
    private manaFill: HTMLElement;
    private load: HTMLElement;          // A heavy load or too much (doc 35, 1.2): only then.
    private who: HTMLElement;
    private posture: HTMLElement;
    private law: HTMLElement;
    // Floating
    private menu: HTMLElement;
    private menuKey = '';
    private toast: HTMLElement;
    private minimap: HTMLCanvasElement;
    private mapRenderer = new MapRenderer(pageSurface);
    private tooltip: HTMLElement;
    private tipWhat: HTMLElement;
    private tipWhy: HTMLElement;
    private lookWhat: HTMLElement;
    private lookWhy: HTMLElement;
    private lookKey = '';
    private lookSince = 0;
    private resizer: HTMLElement;
    private fight: FightPanel;
    private tie: TiePanel;
    private combat: CombatScreen;
    private party: PartyPanel;
    private place: PlacePanel;
    private lodging!: LodgingPanel;
    private stall!: StallPanel;
    private tellerButton!: HTMLElement;
    private project!: ProjectPanel;
    private table!: TablePanel;
    private festival!: FestivalPanel;
    private camp: CampPanel;
    private devConsole: DevConsole;
    private devButton: HTMLButtonElement;
    private friendsButton: HTMLButtonElement;
    private friendsCount: HTMLElement;

    constructor(parent: HTMLElement, state: GameState, portraits: Portraits) {
        this.s = state;
        this.portraits = portraits;
        const act = (action: string, target = '') => state.activate({rect: noRect, action, target});
        this.root = el('div', 'game', parent);

        this.story = new StoryPanel(this.root, state, portraits);
        this.resizer = el('div', 'resizer', this.root);
        this.resizer.title = 'Drag to widen the story or the map';
        this.dragToResize();

        const center = el('main', 'center', this.root);
        this.devConsole = new DevConsole(center, state);
        const head = el('div', 'map-head', center);
        const titles = el('div', 'map-titles', head);
        el('span', 'label gold', titles, 'YOUR SURROUNDINGS');
        this.mapPlace = el('span', 'map-place', titles);
        // The time of day and the weather as pictures, their words on the pointer (no clock, no year, no day count).
        const env = el('div', 'env-icons', head);
        this.timeIcon = el('span', 'env-icon time-icon', env);
        this.weatherIcon = el('span', 'env-icon weather-icon', env);
        this.day = el('span', 'label gold day-kind', env);
        // The senses here (sight, hearing, scent) as an eye, an ear and a nose, open or closed as they work.
        const senses = el('div', 'env-icons senses-icons', head);
        for (const sense of ['sight', 'hearing', 'scent'] as const) this.senseIcons.set(sense, el('span', `env-icon sense-icon ${sense}`, senses));
        const views = el('div', 'tabs', head);
        button('−', 'tab zoom', views, () => act('zoom', 'out')).title = 'Zoom out (−)';
        button('+', 'tab zoom', views, () => act('zoom', 'in')).title = 'Zoom in (=)';
        this.localTab = button('LOCAL MAP', 'tab', views, () => act('local'));
        this.worldTab = button('WORLD MAP', 'tab', views, () => act('world'));
        // The keys, on the pointer (what the bottom bar used to say).
        this.keysHelp = el('span', 'env-icon keys-help', head);
        setIcon(this.keysHelp, NavIcons.help, MapKeys);
        this.mapWrap = el('div', 'map', center);
        this.canvas = el('canvas', '', this.mapWrap);
        this.canvas.tabIndex = 0;
        this.canvas.setAttribute('aria-label', 'The world. WASD to move, Enter to write, right-click for what you can do.');
        this.fight = new FightPanel(center, state);
        this.tie = new TiePanel(center, state);
        this.looking = el('div', 'looking muted', center);
        el('span', 'label muted', this.looking, 'LOOKING AT  ');
        this.lookWhat = el('span', 'what', this.looking);
        this.lookWhy = el('span', '', this.looking);
        const side = el('aside', 'side', this.root);
        // The pages (what the top bar held): small buttons with their pictures, their names on the pointer.
        const nav = el('nav', 'side-nav', side);
        const page = (icon: NavIcon, label: string, tip: string, run: () => void) => {
            const b = button('', `nav-button ${icon}`, nav, run);
            setIcon(b, NavIcons[icon], tip);
            b.dataset.page = label;
            return b;
        };
        page('character', 'CHARACTER', 'Character (C)', () => act('character'));
        page('chapter', 'CHAPTER', 'Chapter', () => act('chapter_window'));
        page('inventory', 'INVENTORY', 'Inventory (I)', () => act('inventory'));
        page('journal', 'JOURNAL', 'Journal: your stories under way, and what you have found out', () => state.send({type: 'journal'}));
        // Friends (doc 50): the count of requests waiting, when there are any.
        this.friendsButton = page('friends', 'FRIENDS', 'Friends', () => act('people'));
        this.friendsCount = el('span', 'nav-count', this.friendsButton);
        page('settings', 'SETTINGS', 'Settings', () => act('settings'));
        // Approved storytellers (doc 58); a player marked Dungeon Master (the Dungeon Master app): the Dev Console, also `.
        this.tellerButton = page('storyteller', 'STORYTELLER', 'Storyteller: your tales', () => act('storyteller'));
        show(this.tellerButton, false);
        this.devButton = page('dev', 'DEV CONSOLE', 'The Dev Console (`): commands for Dungeon Masters', () => act('dev_console'));
        show(this.devButton, false);
        // The minimap (doc 29, phase 8): the country around; the wheel zooms it, a click opens the World Map.
        const mini = el('section', 'panel minimap', side);
        this.minimap = el('canvas', '', mini);
        this.minimap.title = 'The country around · wheel to zoom · click for the World Map';
        this.minimap.addEventListener('mousedown', e => e.preventDefault());
        this.minimap.addEventListener('click', () => act('world'));
        // Pointing at an open scene's mark says what it is (doc 51): "3 wolves at the Wharf tavern · open".
        this.minimap.addEventListener('mousemove', e => {
            const r = this.minimap.getBoundingClientRect();
            const near = this.mapRenderer.sceneMarks.find(m => Math.hypot(m.x - (e.clientX - r.left), m.y - (e.clientY - r.top)) < 9);
            this.minimap.title = near ? near.text : 'The country around · wheel to zoom · click for the World Map';
        });
        this.minimap.addEventListener('wheel', e => {
            e.preventDefault();
            state.miniZoom = Math.max(0, Math.min(MapScales.length - 1, state.miniZoom + (e.deltaY < 0 ? 1 : -1)));
        }, {passive: false});
        this.party = new PartyPanel(side, state);
        this.place = new PlacePanel(side, state);
        this.lodging = new LodgingPanel(side, state);
        this.stall = new StallPanel(side, state);
        this.project = new ProjectPanel(side, state);
        this.table = new TablePanel(side, state);
        this.festival = new FestivalPanel(side, state);
        this.camp = new CampPanel(side, state);
        const sight = el('section', 'panel in-sight', side);
        const sightHead = el('div', 'panel-head', sight);
        el('span', 'label gold', sightHead, 'IN SIGHT');
        this.sightCount = el('span', 'label muted', sightHead);
        this.sightList = el('div', 'sight-list', sight);
        this.sightEmpty = el('p', 'muted small', this.sightList, 'No one in sight.');
        const status = el('section', 'panel status', side);
        this.who = el('div', 'who', status);
        this.posture = el('div', 'muted small', status);
        this.law = el('div', 'law label', status);
        this.load = el('div', 'load-chip small', status);
        const paceHead = el('div', 'meter-head', status);
        this.paceLabel = el('span', 'label', paceHead);
        this.paceNote = el('span', 'note', paceHead);
        const steps = el('div', 'pace-steps', status);
        for (let i = 0; i <= 10; ++i) {
            const step = button('', 'pace-step', steps, () => act('pace', String(i)));
            step.title = `Pace ${i}`;
            this.paceSteps.push(step);
        }
        const gaits = el('div', 'gaits label muted', status);
        for (const g of ['WALK', 'TROT', 'RUN', 'SPRINT']) el('span', '', gaits, g);
        // Health, stamina and mana, always in view (in a fight too): a click opens one's status (doc 33).
        const healthHead = el('div', 'meter-head', status);
        this.healthLabel = el('span', 'label', healthHead);
        const healthBar = el('div', 'bar health', status);
        this.healthFill = el('div', 'fill', healthBar);
        const staminaHead = el('div', 'meter-head', status);
        this.staminaLabel = el('span', 'label', staminaHead);
        const bar = el('div', 'bar', status);
        this.staminaFill = el('div', 'fill', bar);
        this.staminaNote = el('div', 'note', status);
        this.manaRow = el('div', 'mana-row', status);
        this.manaLabel = el('span', 'label', el('div', 'meter-head', this.manaRow));
        this.manaFill = el('div', 'fill', el('div', 'bar mana', this.manaRow));
        for (const part of [healthHead, healthBar, staminaHead, bar, this.manaRow]) {
            part.classList.add('opens-status');
            part.title = 'Your status, belongings and equipment (click)';
            part.addEventListener('click', () => act('status'));
        }

        this.menu = el('div', 'menu radial', this.root);
        this.toast = el('div', 'toast', this.root);
        this.tooltip = el('div', 'tooltip', this.root);
        this.tipWhat = el('div', 'what', this.tooltip);
        this.tipWhy = el('div', 'why', this.tooltip);
        show(this.tooltip, false);
        this.dialogs = new Dialogs(this.root, state, portraits);
        // The fight screen (doc 37): its parts sit in the map, the side, under the map and above the composer.
        this.combat = new CombatScreen(state, portraits, {map: this.mapWrap, side, center, before: this.looking, story: this.story.root,
            storyBefore: this.story.root.querySelector('.composer-bar') as HTMLElement});
        show(this.menu, false);
        show(this.toast, false);
    }

    update() {
        const s = this.s, snapshot = s.snapshot, self = obj(snapshot, 'self');
        setStyle(this.root, '--story', `${s.storyWidth}px`);
        setText(this.mapPlace, s.cellName);
        if (this.mapPlace.title !== s.cellName) this.mapPlace.title = s.cellName;   // (The whole name, if it is cut.)
        this.updateSurroundings();
        setClass(this.localTab, 'active', !s.worldMap);
        setClass(this.worldTab, 'active', s.worldMap);
        this.story.update();
        this.updateSight();
        this.updateStatus(self);
        this.updateMenu();
        this.updateLook();
        // In a fight the screen is about the fight: what isn't steps aside (styles.css, .fight-mode).
        const fighting = !!s.battle;
        setClass(this.root, 'fight-mode', fighting);
        setIcon(this.keysHelp, NavIcons.help, fighting && s.battle?.observer ? 'WATCHING A FIGHT · ENTER write' : fighting ? FightKeys : MapKeys);
        this.combat.update();
        this.fight.update();
        this.tie.update();
        this.party.update();
        this.place.update();
        this.lodging.update();
        this.stall.update();
        this.project.update();
        this.table.update();
        this.festival.update();
        this.camp.update();
        show(this.devButton, s.hasConsole());
        show(this.tellerButton, str(obj(obj(s.snapshot, 'self'), 'storyteller'), 'state') === 'approved');
        const waiting = s.friendRequestsIn.length + s.circleInvites.length;   // (Friend requests and circle invitations.)
        setText(this.friendsCount, waiting ? String(waiting) : '');
        this.friendsButton.title = waiting ? `Friends · ${waiting} waiting` : 'Friends';
        setClass(this.friendsButton, 'unread', waiting > 0);
        setClass(this.devButton, 'active', s.devConsole);
        this.devConsole.update();
        this.drawMinimap();
        show(this.toast, s.clock < s.toastUntil);
        setText(this.toast, s.toast);
        this.dialogs.update();
    }

    private drawMinimap() {
        const canvas = this.minimap, dpr = window.devicePixelRatio || 1;
        const w = Math.max(1, Math.floor(canvas.clientWidth)), h = Math.max(1, Math.floor(canvas.clientHeight));
        if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) {
            canvas.width = Math.round(w * dpr);
            canvas.height = Math.round(h * dpr);
        }
        const c = canvas.getContext('2d');
        if (!c) return;
        c.setTransform(dpr, 0, 0, dpr, 0, 0);
        this.mapRenderer.draw(c, this.s, {x: 0, y: 0, w, h}, MapScales[this.s.miniZoom] ?? 1);
    }

    // ------------------------------------------------------------------ Looking with the pointer

    private updateLook() {
        const s = this.s;
        const pointed = s.highlight ? s.entities.get(s.highlight) : undefined;
        const look: Look | null = pointed ? describeWolf(pointed) : lookAt(s, s.hover);
        if ((look?.key ?? '') !== this.lookKey) {
            this.lookKey = look?.key ?? '';
            this.lookSince = s.clock;
        }
        setText(this.lookWhat, look ? look.what : '');
        setText(this.lookWhy, look ? (look.why ? `  —  ${look.why}` : '') : 'Point at anything on the map to see what it is.');
        // Beside the pointer too, after a moment's rest, unless a menu or sheet is open or the player turned it off.
        const tip = !!look && !pointed && !s.battle && s.hoverTooltips && !s.modal && !s.contextTarget && s.clock - this.lookSince >= 0.15;
        show(this.tooltip, tip);
        if (!tip || !look) return;
        setText(this.tipWhat, look.what);
        setText(this.tipWhy, look.why);
        const r = this.canvas.getBoundingClientRect();
        const x = r.left + s.hover[0] + 16, y = r.top + s.hover[1] + 18;
        setStyle(this.tooltip, 'left', `${Math.min(window.innerWidth - 290, x)}px`);
        setStyle(this.tooltip, 'top', `${Math.min(window.innerHeight - 70, y)}px`);
    }

    // ------------------------------------------------------------------ Who is in sight

    private updateSight() {
        const s = this.s, me = s.entities.get(s.selfId);
        const others = [...s.entities.values()].filter(e => !e.self);
        const distance = (e: EntityView) => (me ? Math.hypot(e.x - me.x, e.y - me.y) : 0);
        others.sort((a, b) => distance(a) - distance(b));
        setText(this.sightCount, others.length ? String(others.length) : '');
        show(this.sightEmpty, !others.length);
        const present = new Set<string>();
        let before: Element | null = this.sightEmpty.nextElementSibling;
        for (const e of others) {
            present.add(e.id);
            let r = this.rows.get(e.id);
            if (!r) r = this.makeRow(e);
            // Keep the list in order of distance without rebuilding it.
            if (r.row !== before) this.sightList.insertBefore(r.row, before);
            before = r.row.nextElementSibling;
            // Known wolves (doc 50): ✎ for a note kept on them; • for a profile changed since one last looked.
            setText(r.name, `${e.name || 'Someone'}${e.nc ? ' ✧' : ''}${e.noted ? ' ✎' : ''}${e.unread ? ' •' : ''}`);
            r.name.title = [e.nc ? 'New to these parts' : '', e.noted ? 'You keep a note on them' : '',
                e.unread ? 'Their profile has changed since you last looked' : ''].filter(Boolean).join(' · ');
            setStyle(r.name, 'color', e.rel === 'chapter' && e.colour ? e.colour
                : css(e.rel === 'party' ? Amber : e.hostile ? HostileRed : e.kind === 'npc' ? Sage : Blue));
            const role = e.kind === 'npc' ? (e.work || 'resident') : 'player';
            // A friend who shares their wolf with you: their handle, subdued, under the name (doc 50).
            setText(r.detail, [upperFirst(role), e.handle ? `friend · ${e.handle}` : '', e.rel === 'party' ? 'your party' : e.rel === 'chapter' ? 'your Chapter' : '', e.hostile ? (e.why ? `hostile · ${e.why}` : 'hostile') : '',
                e.mentor ? (e.mentorFree ? 'mentor · free' : 'mentor') : '', StatusWords[e.rp] ?? '', e.walkup ? 'walk-up friendly' : '',
                e.state && e.state !== 'standing' ? e.state : ''].filter(Boolean).join(' · '));
            r.row.title = e.currently ? `Currently: ${e.currently}` : '';
            setClass(r.row, 'hostile', e.hostile);
            setClass(r.row, 'targeted', s.talkTargets.includes(e.id));
            setClass(r.row, 'highlight', s.highlight === e.id || s.hoveredEntity === e.id);
            if (me) {
                const d = distance(e), angle = Math.atan2(e.y - me.y, e.x - me.x);
                const arrow = Arrows[((Math.round(angle / (Math.PI / 4)) % 8) + 8) % 8];
                setText(r.where, d < 1.5 ? 'here' : `${arrow} ${d.toFixed(0)}`);
            }
            const gearKey = JSON.stringify(e.gear);
            if (gearKey !== r.gearKey) {
                r.gearKey = gearKey;
                drawGear(r.gear, e.gear);
                show(r.gear, e.gear.length > 0);
            }
            // The wolf's painted face in its own fur (doc 81), or its uploaded picture: painted again only when the look
            // changes (each look at this size is kept by the painted wolves, so dozens in sight cost one paint each).
            const look = JSON.stringify([e.appearance, e.artwork, !!artCache.get(e.artwork), this.portraits.version]);
            if (r.drawn !== look) {
                const c = r.portrait.getContext('2d');
                if (c) {
                    c.clearRect(0, 0, r.portrait.width, r.portrait.height);
                    // Nothing for a hunt's game (no appearance) or until the paintings have arrived (the version changes).
                    drawPortrait(c, this.portraits, e.appearance, 0, 0, r.portrait.width, r.portrait.height, e.artwork, {view: 'face', pad: 0.04});
                    r.drawn = look;
                }
            }
        }
        for (const [id, r] of this.rows)
            if (!present.has(id)) {
                r.row.remove();
                this.rows.delete(id);
            }
    }

    private makeRow(e: EntityView): SightRow {
        const s = this.s;
        const row = el('div', 'sight-row');
        const portrait = el('canvas', 'thumb', row);
        portrait.width = portrait.height = 88;
        const text = el('div', 'sight-text', row);
        const name = el('div', 'sight-name', text);
        const detail = el('div', 'sight-detail', text);
        const gear = gearDoll(row, 'sight-gear');
        const where = el('div', 'sight-where', row);
        row.addEventListener('mouseenter', () => (s.highlight = e.id));
        row.addEventListener('mouseleave', () => {
            if (s.highlight === e.id) s.highlight = '';
        });
        row.addEventListener('mousedown', ev => ev.preventDefault());
        row.title = e.kind === 'npc' ? 'Click to speak to them (or stop); right-click for more' : 'Click for what you can do';
        row.addEventListener('click', ev => s.sightClicked(e.id, [ev.clientX, ev.clientY]));
        row.addEventListener('contextmenu', ev => {
            ev.preventDefault();
            s.openContextAt(e.id, [ev.clientX, ev.clientY]);
        });
        const r: SightRow = {row, portrait, name, detail, where, drawn: '', gear, gearKey: '-'};
        this.rows.set(e.id, r);
        return r;
    }

    // ------------------------------------------------------------------ How the wolf is doing

    private updateStatus(self: ReturnType<typeof obj>) {
        const s = this.s;
        setText(this.who, str(self, 'name', 'Connecting'));
        setText(this.posture, `${postureLabel(self)}${str(self, 'state') ? `  ·  ${str(self, 'state')}` : ''}${restLabel(self)}`);
        const law = lawLabel(self);
        setText(this.law, law);
        show(this.law, !!law);
        const load = readLoad(self);
        const laden = !!load && load.state !== 'comfortable';
        setText(this.load, laden ? `${load.state === 'heavy' ? 'HEAVY LOAD' : 'OVERLOADED'} · ${loadCost(load)}` : '');
        setClass(this.load, 'overloaded', load?.state === 'overloaded');
        this.load.title = laden ? `Carrying ${load.carried.toFixed(1)} of a comfortable ${load.comfortable.toFixed(1)} lb. Open Belongings (I) to see what weighs.` : '';
        show(this.load, laden);
        const pace = s.displayPace(), effective = Math.trunc(boundedNum(self, 'effectivePace', 0, 10));
        const exhausted = bool(self, 'exhausted');
        const stamina = boundedNum(self, 'stamina', 0, 100, 100), rate = boundedNum(self, 'staminaRate', -100, 100);
        const health = boundedNum(self, 'health', 0, 100, 100), downedLeft = num(self, 'downedLeft');
        setText(this.healthLabel, `HEALTH ${Math.round(health)}  ·  ${downedLeft > 0 ? 'DOWN' : health >= 100 ? 'UNHURT' : health > 75 ? 'SCRATCHED'
            : health > 50 ? 'WOUNDED' : health > 25 ? 'BADLY HURT' : 'LIMPING'}`);
        setStyle(this.healthFill, 'width', `${health}%`);
        setClass(this.healthFill, 'low', health <= 25);
        const manaMax = num(self, 'manaMax');
        show(this.manaRow, manaMax > 0);
        if (manaMax > 0) {
            setText(this.manaLabel, `MANA ${Math.floor(num(self, 'mana'))} / ${manaMax}`);
            setStyle(this.manaFill, 'width', `${clamp(num(self, 'mana') / manaMax, 0, 1) * 100}%`);
        }
        setText(this.paceLabel, `PACE · ${paceLabel(pace)} ${pace}/10`);
        setClass(this.paceLabel, 'tired', exhausted);
        setClass(this.paceLabel, 'gold', !exhausted && pace >= 9);
        setText(this.paceNote, s.requestedPace >= 0 && s.clock - s.lastPaceRequest <= 1.5 ? 'REQUESTING…'
            : exhausted ? 'EXHAUSTED · walking' : effective < pace ? 'POSTURE-LIMITED' : 'wheel / PgUp PgDn');
        this.paceSteps.forEach((step, i) => {
            setClass(step, 'on', i <= pace);
            setClass(step, 'current', i === pace);
            setClass(step, 'fast', i >= 9);
        });
        setText(this.staminaLabel, `STAMINA ${stamina.toFixed(0)}%  ·  DEX ${envNumber(self, 'effectiveDexterity', 0, 100,
            envNumber(self, 'dexterity', 0, 100, 0)).toFixed(0)}  ·  TOP ${boundedNum(self, 'topSpeed', 0, 100).toFixed(1)} t/s`);
        setStyle(this.staminaFill, 'width', `${stamina}%`);
        setClass(this.staminaFill, 'draining', rate < -0.01);
        setClass(this.staminaFill, 'tired', exhausted);
        const b = s.battle, me = b && !b.observer ? b.fighters.find(f => f.id === s.selfId) : undefined;
        if (b && me) {
            // In a fight (doc 33): how far a move goes at this pace, what it costs, what comes back next turn.
            setText(this.staminaLabel, `STAMINA ${Math.round(me.stamina >= 0 ? me.stamina : stamina)}  ·  STR ${b.stats?.str ?? '?'}  ·  DEX ${b.stats?.dex ?? '?'}`);
            setText(this.staminaNote, `MOVE ${b.moveRange} TILES AT THIS PACE  ·  ${b.tileStamina > 0 ? `${b.tileStamina.toFixed(1)} STAMINA A TILE` : 'WALKING IS FREE'}` +
                `  ·  +${me.regen} NEXT TURN${b.resting ? ' (RESTING)' : ''}`);
            this.staminaNote.title = 'Pace (the wheel) sets how far a move goes: half at a walk, as far as DEX allows at a trot, half again at ' +
                'a sprint. Faster than a trot costs stamina a tile. Stamina comes back only at the start of each turn (4 + STR ÷ 10, less hurt; ' +
                'twice that after a turn of rest). Bites cost 8, a sword 9 or 10.';
        } else {
            this.staminaNote.title = '';
            setText(this.staminaNote, rate < -0.01 ? `DRAINING ${(-rate).toFixed(1)}/s · ease pace for distance`
                : rate > 0.01 ? (stamina >= 99.95 ? 'FULL' : `RECOVERING +${rate.toFixed(1)}/s`) : 'STEADY · sustainable travel');
        }
    }

    // ------------------------------------------------------------------ The surroundings bar

    /** The time of day and the weather as pictures (their words on the pointer), the kind of day, and the senses. */
    private updateSurroundings() {
        const s = this.s, e = s.environment;
        const calendar = obj(obj(obj(s.snapshot, 'cell'), 'environment'), 'calendar');
        const time = timeOfDay(e.hour, e.phase);
        const moon = str(calendar, 'moonName').toLowerCase();
        const when = [time.words, str(calendar, 'weekday'), str(calendar, 'season').toLowerCase(),
            e.phase === 'night' && moon ? `a ${moon}` : ''].filter(Boolean).join(' · ');
        setIcon(this.timeIcon, TimeIcons[time.icon], when);
        const weather = weatherIcon(e.weather, e.intensity, s.outdoors);
        const light = s.outdoors ? '' : e.illumination < 0.25 ? 'Unlit' : e.glowStrength > 0.05 ? `${upperFirst(e.lightingTone)} light` : '';
        setIcon(this.weatherIcon, WeatherIcons[weather.icon], [weather.words, light].filter(Boolean).join(' · '));
        const day = dayLabel(s.snapshot);
        setText(this.day, day);
        show(this.day, !!day);
        const sense = (which: Sense, value: number, extra = '', alert = false) => {
            const icon = this.senseIcons.get(which)!, level = senseLevel(which, value);
            setIcon(icon, SenseIcons[which][level.level], extra ? `${level.words}\n${extra}` : level.words);
            setClass(icon, 'alert', alert);
        };
        sense('sight', e.sight);
        sense('hearing', e.hearing, s.movementHeard ? 'Unseen pawsteps nearby' : 'No unseen steps', s.movementHeard);
        sense('scent', e.scent, upperFirst(scentLabel(s.scentCues).replace(/^SCENT · /, '')), s.scentCues.length > 0);
    }

    // ------------------------------------------------------------------ The wheel (a right-click, or a row of In Sight)

    private updateMenu() {
        const s = this.s;
        const open = !!s.contextTarget && !s.modal;
        show(this.menu, open);
        if (!open) {
            this.menuKey = '';
            return;
        }
        const ground = s.contextTarget === GroundTarget;
        const howlIn = num(obj(obj(s.snapshot, 'self'), 'social'), 'howlIn');
        const key = JSON.stringify([s.contextTarget, s.contextName, s.contextActions, s.contextPage, s.contextPoint, s.armed, ground && howlIn > 0,
            window.innerWidth, window.innerHeight]);
        if (key === this.menuKey) return;
        this.menuKey = key;
        this.menu.replaceChildren();
        const words = (action: string) => s.armed === `${action}|${s.contextTarget}` ? 'Attack · sure? (a crime)'
            : ground ? Commands.find(c => c.name === action)?.wheel ?? upperFirst(action)
            : action.startsWith('vouch:') ? `Vouch for ${s.entities.get(action.slice(6))?.name ?? 'them'}`
            : action.startsWith('wardens-') ? wardensWords(s, action)
            : action.startsWith('trouble:') ? action.slice(action.indexOf('|') + 1)   // (A resident's trouble, as the server words it: doc 57.)
            : action === 'vouch' ? 'Vouch for…' : MenuWords[action] ?? upperFirst(action);
        // Round the middle, clockwise from the top: half down the right, half up the left, each name reading outward
        // from the ring, so long ones never cross. The digits 1–9 choose too.
        const n = s.contextActions.length, right = Math.ceil(n / 2), most = Math.max(right, n - right);
        const radius = Math.max(72, most * 19);
        const ring = el('div', 'radial-ring', this.menu);
        setStyle(ring, 'width', `${radius * 2}px`);
        setStyle(ring, 'height', `${radius * 2}px`);
        // The name in the middle, at most three lines (smaller for a long one, an ellipsis past that); the whole of it
        // in the tooltip.
        const name = s.contextName || '…';
        const centre = button('', `radial-centre${name.length > 34 ? ' longest' : name.length > 18 ? ' long' : ''}`, this.menu,
            () => (s.contextTarget = ''));
        el('span', 'radial-name', centre, name);
        centre.title = `${name}\nClose (Esc)`;
        s.contextActions.forEach((action, i) => {
            const onRight = i < right, k = onRight ? right : n - right, j = onRight ? i : n - 1 - i;
            const spread = Math.min(72, 14 * k), angle = k > 1 ? (-spread + (2 * spread * j) / (k - 1)) * Math.PI / 180 : 0;
            const x = Math.cos(angle) * radius * (onRight ? 1 : -1), y = Math.sin(angle) * radius;
            const armed = s.armed === `${action}|${s.contextTarget}`;
            const item = button('', `menu-item radial-item ${onRight ? 'east' : 'west'}${armed ? ' armed' : ''}`, this.menu,
                () => s.activate({rect: noRect, action: 'context', target: action}));
            if (i < 9) el('span', 'radial-key', item, String(i + 1));
            el('span', 'radial-word', item, words(action));
            item.dataset.action = action;
            setStyle(item, 'left', `${x.toFixed(1)}px`);
            setStyle(item, 'top', `${y.toFixed(1)}px`);
            const command = ground ? Commands.find(c => c.name === action) : undefined;
            if (command) item.title = `/${command.name}${command.key ? ` · ${command.key}` : ''} · ${command.help}`;
            if (ground && action === 'howl' && howlIn > 0) {
                item.disabled = true;
                item.title = `Your voice needs a rest: ${Math.ceil(howlIn / 60)} min`;
            }
        });
        // At the pointer (on the map) or where the row was clicked; the whole wheel kept on screen.
        let x: number, y: number;
        if (s.contextPage) [x, y] = s.contextPage;
        else {
            const r = this.canvas.getBoundingClientRect();
            [x, y] = [r.left + s.contextPoint[0], r.top + s.contextPoint[1]];
        }
        const reach = radius + 170;
        setStyle(this.menu, 'left', `${Math.max(reach, Math.min(window.innerWidth - reach, x))}px`);
        setStyle(this.menu, 'top', `${Math.max(radius + 24, Math.min(window.innerHeight - radius - 24, y))}px`);
    }

    // ------------------------------------------------------------------ Widening the story or the map

    private dragToResize() {
        this.resizer.addEventListener('mousedown', down => {
            down.preventDefault();
            const start = down.clientX, width = this.s.storyWidth;
            const move = (e: MouseEvent) => {
                this.s.storyWidth = Math.round(Math.max(300, Math.min(window.innerWidth * 0.6, width + e.clientX - start)));
            };
            const up = () => {
                window.removeEventListener('mousemove', move);
                window.removeEventListener('mouseup', up);
                try {
                    localStorage.setItem('ratw.storyWidth', String(this.s.storyWidth));
                } catch { /* A private window: the width lasts this visit. */ }
            };
            window.addEventListener('mousemove', move);
            window.addEventListener('mouseup', up);
        });
    }
}
