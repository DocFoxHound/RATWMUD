// The screen around the map (Docs/Design/29-client-polish.md, phase 2): a slim top bar, the story column on the left
// (story.ts), the map in the middle with its header and actions, and on the right who is in sight and how the wolf is
// doing. Plain HTML, updated in place each frame; the map itself is the canvas (game/paint.ts).
import {css, rgb} from '../color.ts';
import {Amber, Blue, Sage} from '../theme.ts';
import {drawPortrait, type Portraits} from '../portrait.ts';
import {bool, boundedNum, clamp, envNumber, num, obj, str} from '../../game/json.ts';
import {calendarLabel, dayLabel, environmentEffectsLabel, environmentLabel, lawLabel, moonLabel, paceLabel, postureLabel,
    restLabel, scentLabel} from '../../game/labels.ts';
import type {EntityView, GameState} from '../../game/state.ts';
import {describeWolf, lookAt, type Look} from '../../game/look.ts';
import {Dialogs} from './dialogs.ts';
import {MapRenderer, MapScales} from '../../game/minimap.ts';
import {artCache} from '../artwork.ts';
import {pageSurface} from '../../game/terrainLayer.ts';
import {button, el, setClass, setStyle, setText, show} from './dom.ts';
import {FightPanel} from './fight.ts';
import {CombatScreen} from './combat.ts';
import {PartyPanel} from './party.ts';
import {PlacePanel} from './place.ts';
import {CampPanel} from './camp.ts';
import {DevConsole} from './devConsole.ts';
import {noRect, StoryPanel} from './story.ts';

const HostileRed = rgb(0xe0695e);
const upperFirst = (s: string) => (s ? s[0].toUpperCase() + s.slice(1) : s);
const Arrows = ['→', '↘', '↓', '↙', '←', '↖', '↑', '↗'];
/** Menu entries that aren't their own action's name: a challenge's terms (doc 37). */
const MenuWords: Record<string, string> = {'challenge:yield': 'Duel until one yields', 'challenge:blood': 'Duel to first blood',
    'challenge:death': 'Fight until one goes down'};

/** One row of the In Sight list. */
interface SightRow {
    row: HTMLElement;
    portrait: HTMLCanvasElement;
    name: HTMLElement;
    detail: HTMLElement;
    where: HTMLElement;
    drawn: string;              // The look last drawn into the portrait.
}

export class Hud {
    readonly root: HTMLElement;
    readonly story: StoryPanel;
    readonly mapWrap: HTMLElement;
    readonly canvas: HTMLCanvasElement;
    private s: GameState;
    private portraits: Portraits;
    private dialogs: Dialogs;
    // Top bar
    private calendar: HTMLElement;
    private weather: HTMLElement;
    private moon: HTMLElement;
    private day: HTMLElement;
    private live: HTMLElement;
    // Map header and actions
    private mapPlace: HTMLElement;
    private effects: HTMLElement;
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
    private senses: HTMLElement;
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
    private connection: HTMLElement;
    private resizer: HTMLElement;
    private fight: FightPanel;
    private combat: CombatScreen;
    private help: HTMLElement;
    private party: PartyPanel;
    private place: PlacePanel;
    private camp: CampPanel;
    private devConsole: DevConsole;
    private devButton: HTMLButtonElement;

    constructor(parent: HTMLElement, state: GameState, portraits: Portraits) {
        this.s = state;
        this.portraits = portraits;
        const act = (action: string, target = '') => state.activate({rect: noRect, action, target});
        this.root = el('div', 'game', parent);

        const top = el('header', 'topbar', this.root);
        const brand = el('div', 'brand', top);
        el('span', 'mark', brand, '◭');
        el('span', 'title', brand, 'RUNS AGAINST THE WORLD');
        const when = el('div', 'when', top);
        this.calendar = el('span', 'label sage', when);
        this.weather = el('span', 'muted', when);
        this.moon = el('span', 'label muted', when);
        this.day = el('span', 'label gold', when);
        const menu = el('nav', 'top-actions', top);
        button('CHARACTER', 'top', menu, () => act('character'));
        button('CHAPTER', 'top', menu, () => act('chapter_window'));
        button('INVENTORY', 'top', menu, () => act('inventory'));
        button('SETTINGS', 'top', menu, () => act('settings'));
        // Only for a player marked Dungeon Master (the Dungeon Master app): the Dev Console, also the ` key.
        this.devButton = button('DEV CONSOLE', 'top dev', menu, () => act('dev_console'));
        this.devButton.title = 'The Dev Console (`): commands for Dungeon Masters';
        show(this.devButton, false);
        this.live = el('span', 'live label', top, '…');

        this.story = new StoryPanel(this.root, state);
        this.resizer = el('div', 'resizer', this.root);
        this.resizer.title = 'Drag to widen the story or the map';
        this.dragToResize();

        const center = el('main', 'center', this.root);
        this.devConsole = new DevConsole(center, state);
        const head = el('div', 'map-head', center);
        const titles = el('div', 'map-titles', head);
        el('span', 'label gold', titles, 'YOUR SURROUNDINGS');
        this.mapPlace = el('span', 'map-place', titles);
        this.effects = el('span', 'label muted effects', titles);
        const views = el('div', 'tabs', head);
        button('−', 'tab zoom', views, () => act('zoom', 'out')).title = 'Zoom out (−)';
        button('+', 'tab zoom', views, () => act('zoom', 'in')).title = 'Zoom in (=)';
        this.localTab = button('LOCAL MAP', 'tab', views, () => act('local'));
        this.worldTab = button('WORLD MAP', 'tab', views, () => act('world'));
        this.mapWrap = el('div', 'map', center);
        this.canvas = el('canvas', '', this.mapWrap);
        this.canvas.tabIndex = 0;
        this.canvas.setAttribute('aria-label', 'The world. WASD to move, Enter to write.');
        this.fight = new FightPanel(center, state);
        this.looking = el('div', 'looking muted', center);
        el('span', 'label muted', this.looking, 'LOOKING AT  ');
        this.lookWhat = el('span', 'what', this.looking);
        this.lookWhy = el('span', '', this.looking);
        const actions = el('div', 'actions', center);
        el('span', 'label muted', actions, 'ACTIONS');
        button('Listen  L', 'act', actions, () => act('listen'));
        button('Look', 'act', actions, () => act('look'));
        button('Smell', 'act', actions, () => act('smell'));
        button('Wait', 'act', actions, () => act('wait'));
        button('Sit', 'act', actions, () => act('sit'));
        button('Lie down', 'act', actions, () => act('lay')).title = 'Rest: six hours lying in a bed is a full rest';
        button('End scene', 'act', actions, () => act('session_end'));

        const side = el('aside', 'side', this.root);
        // The minimap (doc 29, phase 8): the country around; the wheel zooms it, a click opens the World Map.
        const mini = el('section', 'panel minimap', side);
        this.minimap = el('canvas', '', mini);
        this.minimap.title = 'The country around · wheel to zoom · click for the World Map';
        this.minimap.addEventListener('mousedown', e => e.preventDefault());
        this.minimap.addEventListener('click', () => act('world'));
        this.minimap.addEventListener('wheel', e => {
            e.preventDefault();
            state.miniZoom = Math.max(0, Math.min(MapScales.length - 1, state.miniZoom + (e.deltaY < 0 ? 1 : -1)));
        }, {passive: false});
        this.party = new PartyPanel(side, state);
        this.place = new PlacePanel(side, state);
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
        this.senses = el('div', 'senses small', status);

        const help = el('footer', 'helpbar', this.root);
        this.help = el('span', 'label muted', help);
        this.connection = el('span', 'sage small', help);
        this.menu = el('div', 'menu', this.root);
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
        setText(this.calendar, calendarLabel(snapshot));
        setText(this.weather, environmentLabel(s.environment.hour, s.environment.phase, s.environment.weather, s.outdoors, s.environment.intensity));
        setText(this.moon, moonLabel(snapshot));
        setText(this.day, dayLabel(snapshot).slice(0, 30));
        setText(this.live, snapshot ? '● LIVE' : '…');
        setClass(this.live, 'on', !!snapshot);
        setText(this.mapPlace, s.cellName);
        setText(this.effects, environmentEffectsLabel(s.environment, s.outdoors, s.reducedMotion));
        setClass(this.localTab, 'active', !s.worldMap);
        setClass(this.worldTab, 'active', s.worldMap);
        setText(this.connection, str(snapshot, 'connection', 'Connecting to the world…'));
        this.story.update();
        this.updateSight();
        this.updateStatus(self);
        this.updateMenu();
        this.updateLook();
        // In a fight the screen is about the fight: what isn't steps aside (styles.css, .fight-mode).
        const fighting = !!s.battle;
        setClass(this.root, 'fight-mode', fighting);
        setText(this.help, fighting && s.battle?.observer ? 'WATCHING A FIGHT · ENTER write'
            : fighting ? 'CLICK a lit tile to move · CLICK a foe to strike · WHEEL pace · 1–9 actions · R rest · SPACE end turn · Q / E turn · ENTER write'
            : 'WASD move · CLICK path · ALT+CLICK turn · WHEEL / PgUp PgDn pace · SHIFT/CTRL+WHEEL pan · +/− zoom · M map · E nearest');
        this.combat.update();
        this.fight.update();
        this.party.update();
        this.place.update();
        this.camp.update();
        show(this.devButton, s.isDungeonMaster());
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
            setText(r.name, e.name || 'Someone');
            setStyle(r.name, 'color', e.rel === 'chapter' && e.colour ? e.colour
                : css(e.rel === 'party' ? Amber : e.hostile ? HostileRed : e.kind === 'npc' ? Sage : Blue));
            const role = e.kind === 'npc' ? (e.work || 'resident') : 'player';
            setText(r.detail, [upperFirst(role), e.rel === 'party' ? 'your party' : e.rel === 'chapter' ? 'your Chapter' : '', e.hostile ? (e.why ? `hostile · ${e.why}` : 'hostile') : '',
                e.state && e.state !== 'standing' ? e.state : ''].filter(Boolean).join(' · '));
            setClass(r.row, 'hostile', e.hostile);
            setClass(r.row, 'targeted', s.talkTargets.includes(e.id));
            setClass(r.row, 'highlight', s.highlight === e.id || s.hoveredEntity === e.id);
            if (me) {
                const d = distance(e), angle = Math.atan2(e.y - me.y, e.x - me.x);
                const arrow = Arrows[((Math.round(angle / (Math.PI / 4)) % 8) + 8) % 8];
                setText(r.where, d < 1.5 ? 'here' : `${arrow} ${d.toFixed(0)}`);
            }
            const look = JSON.stringify([e.appearance, e.lifeStage, e.artwork, !!artCache.get(e.artwork)]);
            if (r.drawn !== look) {
                const c = r.portrait.getContext('2d');
                if (c) {
                    c.clearRect(0, 0, r.portrait.width, r.portrait.height);
                    const age = e.lifeStage === 'young' ? 6 : e.lifeStage === 'adolescent' ? 13 : e.lifeStage === 'old' ? 65 : 18;
                    // Drawn once its sheet has loaded; until then the row shows its initial.
                    if (drawPortrait(c, this.portraits, e.appearance, age, 0, 0, r.portrait.width, r.portrait.height, e.artwork)) r.drawn = look;
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
        const r: SightRow = {row, portrait, name, detail, where, drawn: ''};
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
                'twice that after a turn of rest). Bites cost 8, a sword 14.';
        } else {
            this.staminaNote.title = '';
            setText(this.staminaNote, rate < -0.01 ? `DRAINING ${(-rate).toFixed(1)}/s · ease pace for distance`
                : rate > 0.01 ? (stamina >= 99.95 ? 'FULL' : `RECOVERING +${rate.toFixed(1)}/s`) : 'STEADY · sustainable travel');
        }
        setText(this.senses, `HEARING · ${s.movementHeard ? 'unseen pawsteps' : 'no unseen steps'}   ·   ${scentLabel(s.scentCues)}`);
    }

    // ------------------------------------------------------------------ The action menu

    private updateMenu() {
        const s = this.s;
        const open = !!s.contextTarget && !s.modal;
        show(this.menu, open);
        if (!open) {
            this.menuKey = '';
            return;
        }
        const key = JSON.stringify([s.contextTarget, s.contextName, s.contextActions, s.contextPage, s.contextPoint, s.armed]);
        if (key === this.menuKey) return;
        this.menuKey = key;
        this.menu.replaceChildren();
        el('div', 'menu-title', this.menu, s.contextName);
        s.contextActions.forEach((action, i) =>
            button(`${i + 1}  ${s.armed === `${action}|${s.contextTarget}` ? 'Attack · sure? (a crime)' : MenuWords[action] ?? upperFirst(action)}`,
                `menu-item${s.armed === `${action}|${s.contextTarget}` ? ' armed' : ''}`, this.menu,
                () => s.activate({rect: noRect, action: 'context', target: action})));
        // On the map, beside what was clicked; from a panel, at the pointer. Kept on screen either way.
        let x: number, y: number;
        if (s.contextPage) [x, y] = s.contextPage;
        else {
            const r = this.canvas.getBoundingClientRect();
            [x, y] = [r.left + s.contextPoint[0], r.top + s.contextPoint[1]];
        }
        const height = 50 + s.contextActions.length * 34;
        setStyle(this.menu, 'left', `${Math.max(8, Math.min(window.innerWidth - 210, x))}px`);
        setStyle(this.menu, 'top', `${Math.max(8, Math.min(window.innerHeight - height - 8, y))}px`);
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
