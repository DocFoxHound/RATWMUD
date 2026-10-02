// The game screen on the page: the HTML panels (ui/hud) around the map's canvas, the composer as a real text box in
// the story column, and the keyboard and mouse.
//
// Keys are read by where they are on the keyboard (KeyboardEvent.code), so WASD sits under the left hand on any
// layout. Held keys are let go whenever the page loses focus or is hidden, so a wolf never keeps walking after Alt-Tab.
import {GameState, type Composer, type KeyInput} from './state.ts';
import {GamePainter} from './paint.ts';
import {Sheets} from './weatherArt.ts';
import {Painter} from '../ui/painter.ts';
import {Portraits} from '../ui/portrait.ts';
import {Hud} from '../ui/hud/hud.ts';
import type {Json} from './json.ts';
import type {MotionFrame} from '../net/motion.ts';

const MovementKeys = new Set(['KeyW', 'KeyA', 'KeyS', 'KeyD']);

/** The last few hundred frames: how long apart they came, and how long drawing took (milliseconds). */
export class FrameStats {
    readonly intervals = new Float32Array(600);
    readonly work = new Float32Array(600);
    count = 0;

    add(interval: number, work: number) {
        const i = this.count++ % this.intervals.length;
        this.intervals[i] = interval;
        this.work[i] = work;
    }
    reset() {
        this.count = 0;
    }
    /** p50, p99 and worst of one series over the frames kept. */
    summary(series: 'intervals' | 'work' = 'intervals'): {p50: number; p99: number; worst: number; frames: number} {
        const n = Math.min(this.count, this.intervals.length);
        const sorted = Array.from(this[series].subarray(0, n)).sort((a, b) => a - b);
        const at = (q: number) => (n ? sorted[Math.min(n - 1, Math.floor(q * n))] : 0);
        return {p50: at(0.5), p99: at(0.99), worst: n ? sorted[n - 1] : 0, frames: n};
    }
}

class TextareaComposer implements Composer {
    readonly element: HTMLTextAreaElement;
    private home: HTMLElement;

    constructor(element: HTMLTextAreaElement, home: HTMLElement) {
        this.element = element;
        this.home = home;
    }
    get text() {
        return this.element.value;
    }
    set text(value: string) {
        this.element.value = value;
    }
    focus() {
        this.element.readOnly = false;
        this.element.focus({preventScroll: true});
    }
    blur() {
        this.element.blur();
        this.home.focus({preventScroll: true});
    }
    insertNewline() {
        this.element.setRangeText('\n', this.element.selectionStart, this.element.selectionEnd, 'end');
    }
}

const keyOf = (e: KeyboardEvent): KeyInput => ({code: e.code, shift: e.shiftKey, alt: e.altKey, ctrl: e.ctrlKey});

export class GameView {
    readonly state: GameState;
    readonly root: HTMLDivElement;
    readonly hud: Hud;
    private canvas: HTMLCanvasElement;
    private composer: TextareaComposer;
    private painter: GamePainter;
    private size: [number, number] = [960, 617];
    private frame = 0;
    private last = 0;
    private wheelRest = 0;
    private cleanup: Array<() => void> = [];
    readonly frames = new FrameStats();
    private showPerf = new URLSearchParams(location.search).has('perf');

    constructor(parent: HTMLElement, send: (command: Json) => void) {
        this.root = document.createElement('div');
        this.root.className = 'game-root';
        parent.append(this.root);
        const portraits = new Portraits();
        // The state needs the composer and the panels need the state: the composer's text box is made first.
        let composer: TextareaComposer | null = null;
        const proxy = {
            get text() { return composer!.text; },
            set text(v: string) { composer!.text = v; },
            focus: () => composer!.focus(),
            blur: () => composer!.blur(),
            insertNewline: () => composer!.insertNewline(),
        };
        this.state = new GameState(send, proxy);
        try {
            const width = Number(localStorage.getItem('ratw.storyWidth'));
            if (width >= 300 && width <= 1400) this.state.storyWidth = width;
        } catch { /* No storage here: the default width. */ }
        this.hud = new Hud(this.root, this.state, portraits);
        this.canvas = this.hud.canvas;
        this.composer = composer = new TextareaComposer(this.hud.story.textarea, this.canvas);
        const sheets = new Sheets();
        sheets.warm();
        this.painter = new GamePainter(this.state, new Painter(this.canvas.getContext('2d')!), sheets);
        this.listen();
        // Glyphs measured before the game's fonts arrive would sit off centre: measure again once they have.
        document.fonts?.ready.then(() => this.painter.terrain.invalidate());
        for (const f of ['400 20px "RATW Mono"', '400 20px "RATW Sans"', '500 20px "RATW Sans"']) document.fonts?.load(f).catch(() => {});
        const observer = new ResizeObserver(() => this.resize());
        observer.observe(this.hud.mapWrap);
        this.cleanup.push(() => observer.disconnect());
        this.resize();
        this.canvas.focus({preventScroll: true});
        this.frame = requestAnimationFrame(t => this.draw(t));
    }

    applySnapshot(snapshot: Json) { this.state.applySnapshot(snapshot); }
    /** How many times the map's ground has been drawn offscreen (for the frame smoke). */
    groundRedrawn() { return this.painter.terrain.rebuilds; }
    applyMotion(frame: MotionFrame) { this.state.applyMotion(frame); }
    receiveEvent(event: Json) { this.state.receiveEvent(event); }

    destroy() {
        cancelAnimationFrame(this.frame);
        for (const undo of this.cleanup) undo();
        this.root.remove();
    }

    private on<K extends keyof WindowEventMap>(target: Window, type: K, f: (e: WindowEventMap[K]) => void, options?: AddEventListenerOptions): void;
    private on<K extends keyof HTMLElementEventMap>(target: HTMLElement, type: K, f: (e: HTMLElementEventMap[K]) => void, options?: AddEventListenerOptions): void;
    private on(target: EventTarget, type: string, f: (e: never) => void, options?: AddEventListenerOptions) {
        target.addEventListener(type, f as EventListener, options);
        this.cleanup.push(() => target.removeEventListener(type, f as EventListener, options));
    }

    /** A pointer position on the map, in the canvas's own CSS pixels. */
    private point(e: MouseEvent): [number, number] {
        const r = this.canvas.getBoundingClientRect();
        return [e.clientX - r.left, e.clientY - r.top];
    }

    private listen() {
        const s = this.state, textarea = this.composer.element;
        this.on(window, 'keydown', e => {
            if (e.target === textarea) return;
            if (e.repeat && MovementKeys.has(e.code)) {
                e.preventDefault();
                return;
            }
            if (s.keyDown(keyOf(e))) e.preventDefault();
        });
        this.on(window, 'keyup', e => {
            if (e.target === textarea) return;
            if (s.keyUp(keyOf(e))) e.preventDefault();
        });
        // Nothing stays held when attention goes elsewhere.
        this.on(window, 'blur', () => s.focusLost());
        this.on(window, 'focus', () => s.focusGained());
        const hidden = () => {
            if (document.hidden) s.focusLost();
        };
        document.addEventListener('visibilitychange', hidden);
        this.cleanup.push(() => document.removeEventListener('visibilitychange', hidden));
        this.on(this.canvas, 'focus', () => s.focusGained());
        this.on(textarea, 'focus', () => {
            s.heldKeys.clear();
            if (!s.chat) s.setChat(true);
        });
        this.on(textarea, 'input', () => s.composerChanged());
        this.on(textarea, 'keydown', e => {
            if (s.composerKey(keyOf(e))) e.preventDefault();
        });
        this.on(this.canvas, 'mousemove', e => s.mouseMove(this.point(e), e.altKey));
        this.on(this.canvas, 'mouseleave', () => s.mouseLeave());
        this.on(this.canvas, 'mousedown', e => {
            e.preventDefault();
            s.mouseDown(this.point(e), e.button === 0, e.altKey, e.ctrlKey);
            if (document.activeElement === textarea && s.chat) return;
            this.canvas.focus({preventScroll: true});
        });
        // A click anywhere outside an open menu closes it.
        this.on(window, 'mousedown', e => {
            const t = e.target as HTMLElement | null;
            if (s.contextTarget && t !== this.canvas && !t?.closest?.('.menu, .sight-row')) s.contextTarget = '';
        });
        this.on(this.canvas, 'contextmenu', e => e.preventDefault());
        // The wheel over the map changes pace, and pans with Shift or Ctrl; one step per notch, however finely a
        // trackpad reports it.
        this.on(this.canvas, 'wheel', e => {
            const step = e.deltaMode === WheelEvent.DOM_DELTA_LINE ? 3 : e.deltaMode === WheelEvent.DOM_DELTA_PAGE ? 1 : 100;
            this.wheelRest += e.deltaY / step;
            const notches = Math.trunc(this.wheelRest);
            if (!notches) {
                e.preventDefault();
                return;
            }
            this.wheelRest -= notches;
            if (s.wheel(this.point(e), -notches, e.shiftKey, e.ctrlKey)) e.preventDefault();
        }, {passive: false});
    }

    private resize() {
        const dpr = window.devicePixelRatio || 1;
        const r = this.hud.mapWrap.getBoundingClientRect();
        const w = Math.max(1, Math.floor(r.width)), h = Math.max(1, Math.floor(r.height));
        this.size = [w, h];
        this.canvas.width = Math.round(w * dpr);
        this.canvas.height = Math.round(h * dpr);
        this.canvas.style.width = `${w}px`;
        this.canvas.style.height = `${h}px`;
    }

    /** ?perf: frame times in the corner, for finding hitches by eye. */
    private drawPerf(c: CanvasRenderingContext2D, dpr: number) {
        const gaps = this.frames.summary('intervals'), work = this.frames.summary('work');
        c.setTransform(dpr, 0, 0, dpr, 0, 0);
        c.fillStyle = 'rgba(0,0,0,0.7)';
        c.fillRect(8, this.size[1] - 52, 330, 44);
        c.fillStyle = gaps.p99 > 20 ? '#e1aba2' : '#a8c2a6';
        c.font = '12px monospace';
        c.textBaseline = 'top';
        c.fillText(`frame p50 ${gaps.p50.toFixed(1)}  p99 ${gaps.p99.toFixed(1)}  worst ${gaps.worst.toFixed(1)} ms`, 14, this.size[1] - 46);
        c.fillText(`draw  p50 ${work.p50.toFixed(1)}  p99 ${work.p99.toFixed(1)}  ground redrawn ${this.painter.terrain.rebuilds}×`, 14, this.size[1] - 28);
    }

    private draw(time: number) {
        const started = performance.now();
        const interval = this.last ? time - this.last * 1000 : 0;
        const seconds = time / 1000;
        const delta = this.last ? Math.min(0.25, seconds - this.last) : 0;
        this.last = seconds;
        this.state.tick(seconds, delta);
        const c = this.canvas.getContext('2d')!;
        const dpr = window.devicePixelRatio || 1;
        c.setTransform(dpr, 0, 0, dpr, 0, 0);
        this.painter.paint(this.size[0], this.size[1]);
        this.hud.update();
        const work = performance.now() - started;
        if (interval > 0) this.frames.add(interval, work);
        if (this.showPerf) this.drawPerf(c, dpr);
        this.frame = requestAnimationFrame(t => this.draw(t));
    }
}
