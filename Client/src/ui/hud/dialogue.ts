// The dialogue window (Docs/Design/83-dialogue-trees.md; game/dialogue.ts holds its state): a panel at the foot of the
// map, not a modal, so the world goes on. The resident's painted face (a slot built for animation: FaceSlot), their
// name, work and place, the last exchanges dimmed, their line written out, and the choices numbered. Each choice is a
// default line the player may send as it is or rewrite: the one picked out shows its words in a field to edit. Where
// the node allows it, "Say something else..." sends the player's own line (free talk).
import {drawPortrait, type Portraits} from '../portrait.ts';
import type {Json} from '../../game/json.ts';
import type {GameState} from '../../game/state.ts';
import {revealed, wordsFor} from '../../game/dialogue.ts';
import {button, el, setClass, setText, show} from './dom.ts';

/** The face: the resident's painting in their own fur (doc 81), and a state the window sets with each line, so Phase 5's
 *  animation (blinks, a mouth, moods) only has to draw something different. */
export class FaceSlot {
    readonly canvas: HTMLCanvasElement;
    private portraits: Portraits;
    private look: Json | null = null;
    private drawn = '';
    /** What the face is doing (Phase 5 draws it: a mouth while speaking, the mood's expression). */
    state = {speaking: false, mood: ''};

    constructor(parent: HTMLElement, portraits: Portraits, className = 'dialogue-face') {
        this.portraits = portraits;
        this.canvas = el('canvas', className, parent);
        this.canvas.width = 232;
        this.canvas.height = 276;
        portraits.listen(() => { this.drawn = ''; this.draw(); });   // (A painting arrived.)
    }

    setLook(appearance: Json | null) {
        this.look = appearance;
        this.draw();
    }

    setState(state: {speaking: boolean; mood: string}) {
        this.state = state;
        setClass(this.canvas, 'speaking', state.speaking);
        this.canvas.dataset.mood = state.mood;
    }

    private draw() {
        const key = JSON.stringify(this.look) + this.portraits.version;
        if (key === this.drawn) return;
        const c = this.canvas.getContext('2d');
        if (!c) return;
        c.clearRect(0, 0, this.canvas.width, this.canvas.height);
        // On its paper until the painting arrives, then the painted face over it.
        if (drawPortrait(c, this.portraits, this.look, 0, 0, this.canvas.width, this.canvas.height, undefined, {view: 'face', paper: true}))
            this.drawn = key;
    }
}

export class DialogueWindow {
    readonly root: HTMLElement;
    private s: GameState;
    private face: FaceSlot;
    private heading: HTMLElement;
    private past: HTMLElement;
    private line: HTMLElement;
    private note: HTMLElement;
    private list: HTMLElement;
    private freeRow: HTMLElement;
    private freeInput: HTMLInputElement;
    private hint: HTMLElement;
    private key = '';
    private shownLine = '';

    constructor(parent: HTMLElement, state: GameState, portraits: Portraits) {
        this.s = state;
        this.root = el('section', 'dialogue-window', parent);
        this.root.dataset.dialogue = '';
        this.face = new FaceSlot(this.root, portraits);
        const words = el('div', 'dialogue-words', this.root);
        const head = el('div', 'dialogue-head', words);
        this.heading = el('div', 'label gold', head);
        const close = button('×', 'close', head, () => state.closeDialogue(!state.dialogue?.ends));
        close.title = 'Goodbye (Esc)';
        this.past = el('div', 'dialogue-past', words);
        this.line = el('p', 'dialogue-line', words);
        this.line.addEventListener('click', () => { state.dialogueUi.whole = true; });
        this.note = el('div', 'dialogue-note sage small', words);
        this.list = el('ol', 'dialogue-choices', words);
        this.freeRow = el('div', 'dialogue-free', words);
        this.freeInput = el('input', 'dialogue-input', this.freeRow);
        this.freeInput.placeholder = 'Say something else…';
        this.freeInput.maxLength = 300;
        this.freeInput.dataset.field = 'free';
        this.freeInput.addEventListener('input', () => { state.dialogueUi.free = this.freeInput.value; });
        this.freeInput.addEventListener('keydown', e => {
            e.stopPropagation();
            if (e.key === 'Enter') {
                e.preventDefault();
                state.sayDialogue(this.freeInput.value);
                this.freeInput.blur();
            } else if (e.key === 'Escape') {
                e.preventDefault();
                state.dialogueUi.focus = '';
                this.freeInput.blur();
            }
        });
        button('SAY', 'small', this.freeRow, () => state.sayDialogue(this.freeInput.value));
        this.hint = el('div', 'dialogue-hint muted small', words);
        show(this.root, false);
    }

    update() {
        const s = this.s, d = s.dialogue;
        const open = !!d && s.modal !== 'trade';
        show(this.root, open);
        if (!d || !open) {
            this.key = '';
            return;
        }
        const ui = s.dialogueUi;
        // The line, written out; the face "speaks" while it is.
        const shown = d.awaiting ? '…' : revealed(d.line, ui, s.clock, s.reducedMotion);
        if (shown !== this.shownLine) {
            this.shownLine = shown;
            setText(this.line, shown ? `“${shown}${shown.length < d.line.length ? '' : '”'}` : '');
        }
        this.face.setState({speaking: d.awaiting || shown.length < d.line.length, mood: d.mood});
        const key = JSON.stringify([d.key, d.note, d.awaiting, ui.selected, ui.focus, s.dialogueVersion]);
        if (key === this.key) return;
        this.key = key;
        this.face.setLook(d.appearance);
        setText(this.heading, [d.name, d.role, d.place].filter(Boolean).join(' · ').toUpperCase());
        this.past.replaceChildren();
        for (const h of d.history) el('div', `dialogue-said ${h.who}`, this.past, h.who === 'you' ? `You: “${h.text}”` : `“${h.text}”`);
        setText(this.note, d.note);
        show(this.note, !!d.note);
        // The choices: numbered, clicked to say; the one picked out with its words to edit.
        const editing = document.activeElement instanceof HTMLInputElement && this.list.contains(document.activeElement);
        this.list.replaceChildren();
        d.choices.forEach((c, i) => {
            const row = el('li', `dialogue-choice${i === ui.selected ? ' selected' : ''}${c.locked ? ' locked' : ''}`, this.list);
            row.dataset.n = String(c.n);
            el('span', 'dialogue-n', row, String(c.n));
            if (i === ui.selected && !c.locked) {
                const field = el('input', 'dialogue-input', row);
                field.value = ui.drafts.get(c.n) ?? c.text;
                field.maxLength = 300;
                field.title = 'Say it as it is, or in your own words (your own words earn roleplay experience)';
                field.dataset.field = 'choice';
                field.addEventListener('input', () => ui.drafts.set(c.n, field.value));
                field.addEventListener('keydown', e => {
                    e.stopPropagation();
                    if (e.key === 'Enter') {
                        e.preventDefault();
                        ui.drafts.set(c.n, field.value);
                        s.chooseDialogue(c.n);
                        field.blur();
                    } else if (e.key === 'Escape') {
                        e.preventDefault();
                        ui.focus = '';
                        field.blur();
                    } else if (e.key === 'ArrowUp' || e.key === 'ArrowDown' || e.key === 'Tab') {
                        e.preventDefault();
                        field.blur();
                        ui.focus = '';
                        if (e.key === 'Tab' && d.free) ui.focus = 'free';
                        else s.dialogueKeyDown(e.key);
                        ++s.dialogueVersion;
                    }
                });
                if (ui.focus === 'edit' || editing) queueMicrotask(() => field.focus());
                const say = button('SAY', 'small', row, () => s.chooseDialogue(c.n));
                say.title = wordsFor(d, ui, c.n) === c.text ? 'Say it' : 'Say it in your words';
            } else {
                el('span', 'dialogue-text', row, c.text);
                if (c.locked) el('span', 'dialogue-why muted small', row, c.why);
            }
            row.addEventListener('click', e => {
                if (e.target instanceof HTMLInputElement || e.target instanceof HTMLButtonElement || c.locked) return;
                if (i === ui.selected) s.chooseDialogue(c.n);
                else {
                    ui.selected = i;
                    ++s.dialogueVersion;
                }
            });
        });
        show(this.list, d.choices.length > 0);
        show(this.freeRow, d.free);
        this.freeInput.value = ui.free;
        this.freeInput.disabled = d.awaiting;
        if (ui.focus === 'free' && d.free) queueMicrotask(() => this.freeInput.focus());
        setText(this.hint, d.ends ? 'Enter or Esc to close'
            : `1–${d.choices.length} say · ↑↓ pick · Tab your own words${d.free ? ' · type below to say something else' : ''} · Esc goodbye`);
    }
}

/** The shop window's strip (doc 83): the keeper's face and their last word (a sale's, the greeting). Drawn straight
 *  onto its canvas: the sheet is built again when a painting arrives (Dialogs listens). */
export function renderKeeperStrip(panel: HTMLElement, s: GameState, portraits: Portraits) {
    const d = s.dialogue;
    if (!d?.inShop) return;
    const strip = el('div', 'shop-keeper', panel);
    const face = el('canvas', 'shop-keeper-face', strip);
    face.width = 232;
    face.height = 276;
    const c = face.getContext('2d');
    if (c) drawPortrait(c, portraits, d.appearance, 0, 0, face.width, face.height, undefined, {view: 'face', paper: true});
    const words = el('div', 'shop-keeper-words', strip);
    el('div', 'label muted', words, d.name.toUpperCase());
    el('div', 'shop-keeper-line', words, d.line ? `“${d.line}”` : '');
}
