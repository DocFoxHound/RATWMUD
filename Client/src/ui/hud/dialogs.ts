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
        const art = m === 'inspect' ? str(s.inspectedCharacter, 'artwork') : m === 'character' ? str(self, 'artwork') : '';
        const key = JSON.stringify([m, art, !!artCache.get(art), m === 'inspect' ? s.inspectedText : '', m === 'character' ? self : '',
            m === 'inventory' || m === 'trade' ? [arr(s.snapshot, 'inventory'), obj(s.snapshot, 'merchant'), countText(self, 'cash'),
                obj(s.snapshot, 'resource')] : '',
            m === 'settings' ? [s.selectedColor, s.revealSpeed, s.reducedMotion, s.flatWorld, s.plainGlyphs, s.perfOverlay, s.storyWidth,
                bool(s.snapshot, 'devTools'), s.environment.phase, s.hoverTooltips] : '']);
        if (key === this.key) return;
        this.key = key;
        const typing = document.activeElement === this.aliasInput;      // (Kept, and kept focused, as the sheet is rebuilt.)
        this.panel.replaceChildren();
        const close = button('×', 'close', this.panel, () => this.act('close'));
        close.title = 'Close (Esc)';
        if (m === 'character') this.character(self);
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
        el('div', 'big', right, `Level ${Math.trunc(num(self, 'socialLevel', 1))}`);
        el('div', 'sage', right, `${Math.trunc(num(self, 'socialXp'))} social experience`);
        const bar = el('div', 'bar', right);
        el('div', 'fill', bar).style.width = `${clamp(num(self, 'socialXp') / 100, 0, 1) * 100}%`;
        const skills = el('div', 'skills', right);
        el('span', 'sage', skills, `Sneak ${clamp(Math.trunc(num(self, 'sneakSkill')), 0, 100)} / 100`);
        el('span', 'sage', skills, `Hearing ${clamp(Math.trunc(num(self, 'hearingSkill')), 0, 100)} / 100`);
        el('span', 'scent', skills, `Scent ${clamp(Math.trunc(num(self, 'scentSkill')), 0, 100)} / 100`);
        el('span', 'muted', skills, `Nose ${Math.round(clamp(num(self, 'noseHealth', 1), 0, 1) * 100)}%`);
        this.names(right, self);
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
        for (const good of ['herbs', 'meal'] as const) {
            const item = s.tradeItem(good);
            const card = el('div', 'trade', this.panel);
            el('div', 'item-name', card, good === 'herbs' ? 'Cooking herbs' : 'Prepared meal');
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
