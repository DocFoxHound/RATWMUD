// The Dev Console on screen (a player marked Dungeon Master; Docs/Design/34-dungeon-master-refresh.md, 1.1b): it drops
// down over the map as a game's console does, the game going on behind it. What it has printed, a line to type into,
// and as one types, the commands that fit, alphabetically: Tab completes, the arrows choose, Enter runs, Esc or ` closes.
// The commands are the server's (Core/RatwGameDev.cpp), asked for when it first opens.
import type {GameState} from '../../game/state.ts';
import {button, el, setClass, show} from './dom.ts';

export class DevConsole {
    private s: GameState;
    private root: HTMLElement;
    private log: HTMLElement;
    private input: HTMLInputElement;
    private list: HTMLElement;
    private logKey = '';
    private listKey = '';
    private selected = -1;          // The suggestion chosen with the arrows, or -1 for what is typed.
    private wasOpen = false;

    constructor(parent: HTMLElement, state: GameState) {
        this.s = state;
        this.root = el('section', 'dev-console', parent);
        this.root.setAttribute('aria-label', 'Dev Console');
        const head = el('div', 'dev-console-head', this.root);
        el('span', 'label gold', head, 'DEV CONSOLE · DUNGEON MASTER');
        el('span', 'muted', head, 'Tab completes · ↑ ↓ choose · Enter runs · Esc or ` closes');
        button('×', 'close', head, () => this.close()).title = 'Close (Esc or `)';
        this.log = el('div', 'dev-console-log', this.root);
        const line = el('div', 'dev-console-line', this.root);
        el('span', 'dev-console-prompt', line, '>');
        this.input = el('input', 'dev-console-input', line);
        this.input.spellcheck = false;
        this.input.autocomplete = 'off';
        this.input.maxLength = 60;
        this.input.placeholder = 'type a command: / for all of them';
        this.list = el('div', 'dev-console-suggest', this.root);
        this.input.addEventListener('input', () => {
            this.selected = -1;
            this.listKey = '';
        });
        this.input.addEventListener('keydown', e => this.key(e));
        show(this.root, false);
    }

    private suggestions() {
        return this.s.devSuggestions(this.input.value);
    }

    private key(e: KeyboardEvent) {
        const fits = this.suggestions();
        if (e.key === 'Escape' || e.key === '`') {
            e.preventDefault();
            this.close();
        } else if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
            e.preventDefault();
            if (!fits.length) return;
            const step = e.key === 'ArrowDown' ? 1 : -1;
            this.selected = this.selected < 0 ? (step > 0 ? 0 : fits.length - 1) : (this.selected + step + fits.length) % fits.length;
            this.listKey = '';
        } else if (e.key === 'Tab') {
            e.preventDefault();
            const pick = fits[Math.max(0, this.selected)];
            if (pick) {
                this.input.value = pick[0];
                this.selected = -1;
                this.listKey = '';
            }
        } else if (e.key === 'Enter') {
            e.preventDefault();
            this.run(this.selected >= 0 && fits[this.selected] ? fits[this.selected][0] : this.input.value);
        }
    }

    private run(command: string) {
        if (!command.trim()) return;
        this.s.runDevCommand(command);
        this.input.value = '';
        this.selected = -1;
        this.listKey = '';
    }

    private close() {
        this.s.toggleDevConsole(false);
        this.input.blur();
    }

    update() {
        const s = this.s, open = s.devConsole;
        show(this.root, open);
        if (open && !this.wasOpen) requestAnimationFrame(() => this.input.focus());
        if (!open && this.wasOpen) this.input.blur();
        this.wasOpen = open;
        if (!open) return;
        // What it has printed: each command, and what came of it.
        const last = s.devLog.at(-1);
        const logKey = `${s.devLog.length}:${last?.command}:${last?.text}`;
        if (logKey !== this.logKey) {
            this.logKey = logKey;
            this.log.replaceChildren();
            if (!s.devLog.length) el('div', 'dev-console-entry muted', this.log, 'Nothing run yet. Type / to see the commands.');
            for (const entry of s.devLog) {
                el('div', 'dev-console-entry command', this.log, `> ${entry.command}`);
                el('div', entry.ok ? 'dev-console-entry ok' : 'dev-console-entry refused', this.log, entry.text);
            }
            this.log.scrollTop = this.log.scrollHeight;
        }
        // The commands that fit what is typed, alphabetically (all of them while nothing is).
        const fits = this.suggestions();
        const listKey = `${this.input.value}|${this.selected}|${s.devCommands.length}`;
        if (listKey !== this.listKey) {
            this.listKey = listKey;
            this.list.replaceChildren();
            if (!s.devCommands.length) el('div', 'dev-console-suggestion muted', this.list, 'Asking the server for its commands…');
            else if (!fits.length) el('div', 'dev-console-suggestion muted', this.list, 'No command begins like that.');
            fits.forEach(([name, help], i) => {
                const row = el('div', 'dev-console-suggestion', this.list);
                setClass(row, 'chosen', i === this.selected);
                el('b', '', row, name);
                el('span', 'muted', row, help);
                row.addEventListener('mousedown', e => {
                    e.preventDefault();         // (The line keeps its focus.)
                    this.run(name);
                });
            });
        }
    }
}
