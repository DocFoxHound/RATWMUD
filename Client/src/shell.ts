// The page's side of the desktop client's shell (Docs/Design/76-the-desktop-client.md, 4.4-4.5, as amended by doc 77:
// the page is bundled in the client). In a browser most of it still helps (the in-world key guard, the question before
// leaving a tab, the context menu); the window's own fullscreen and the close question need the shell.
//
// The shell is Tauri 2 (Desktop/src-tauri), reached through window.__TAURI__ (withGlobalTauri). Nothing here assumes
// it: in a browser `tauri()` is null and those parts do nothing.

/** The parts of Tauri's global API the page uses. */
export interface TauriApi {
    core: {invoke<T = unknown>(command: string, args?: Record<string, unknown>): Promise<T>};
    window: {getCurrentWindow(): TauriWindow};
}
export interface TauriWindow {
    setFullscreen(on: boolean): Promise<void>;
    isFullscreen(): Promise<boolean>;
    destroy(): Promise<void>;
    onCloseRequested(handler: (event: {preventDefault(): void}) => void | Promise<void>): Promise<() => void>;
}

export function tauri(): TauriApi | null {
    const t = (globalThis as {__TAURI__?: TauriApi}).__TAURI__;
    return t && t.core && typeof t.core.invoke === 'function' ? t : null;
}

/** A key's facts, as KeyboardEvent has them (a plain object in the tests). */
export interface KeyFacts {
    code: string;
    key?: string;
    ctrlKey?: boolean;
    metaKey?: boolean;
    altKey?: boolean;
    shiftKey?: boolean;
}

/** What the guard does with a key before the game sees it: block it (the browser's reload, find, print, close),
 *  toggle the window's fullscreen, or nothing. */
export function guardKey(k: KeyFacts, inWorld: boolean, native: boolean): 'block' | 'fullscreen' | null {
    const mod = !!(k.ctrlKey || k.metaKey);
    if (native && (k.code === 'F11' || (k.altKey && (k.code === 'Enter' || k.code === 'NumpadEnter')))) return 'fullscreen';
    const reload = k.code === 'F5' || (mod && k.code === 'KeyR');
    // In the client a reload is never wanted (it would drop the sign-in); in a browser only in the world.
    if (reload && (native || inWorld)) return 'block';
    if (inWorld && mod && (k.code === 'KeyF' || k.code === 'KeyP' || k.code === 'KeyW' || k.code === 'KeyG')) return 'block';
    if (native && (k.code === 'F3' || k.code === 'F7' || (mod && (k.code === 'KeyU' || k.code === 'KeyJ')))) return 'block';
    return null;
}

/** Whether a keydown reaches the game's keys: Ctrl and Cmd leave the page's copy, paste and the like alone, so Ctrl+C
 *  on a story copies instead of opening the character sheet. Keyup always reaches it (a held W released while Ctrl is
 *  down still stops the wolf). */
export function gameTakesKeyDown(k: KeyFacts): boolean {
    return !(k.ctrlKey || k.metaKey);
}

/** Whether Meta going up or down should let go of every held key: macOS WebKit sends no keyup for other keys while Cmd
 *  is held. */
export function metaKey(k: KeyFacts): boolean {
    return k.code === 'MetaLeft' || k.code === 'MetaRight' || k.key === 'Meta';
}

/** Whether a right-click keeps the webview's own menu: only in text fields (cut, copy, paste). */
export function keepsContextMenu(target: EventTarget | null): boolean {
    const t = target as {tagName?: string; isContentEditable?: boolean} | null;
    const tag = t?.tagName?.toUpperCase();
    return tag === 'INPUT' || tag === 'TEXTAREA' || t?.isContentEditable === true;
}

export class Shell {
    readonly api: TauriApi | null;
    inWorld = false;
    /** Asked before the window closes in the world (the page's own dialog); true leaves. */
    askToClose: () => Promise<boolean> = async () => true;
    private installed = false;

    constructor(api: TauriApi | null = tauri()) {
        this.api = api;
    }

    get native(): boolean {
        return this.api !== null;
    }

    invoke<T>(command: string, args?: Record<string, unknown>): Promise<T> {
        if (!this.api) return Promise.reject(new Error('Not in the desktop client.'));
        return this.api.core.invoke<T>(command, args);
    }

    /** The guard, the context menu, and the close question; once. */
    install(target: Window = window) {
        if (this.installed) return;
        this.installed = true;
        // Capture phase, ahead of the game, so an open panel (which swallows every key) can't swallow these.
        target.addEventListener('keydown', e => {
            const what = guardKey(e, this.inWorld, this.native);
            if (!what) return;
            e.preventDefault();
            e.stopPropagation();
            if (what === 'fullscreen' && !e.repeat) void this.toggleFullscreen();
        }, {capture: true});
        target.addEventListener('contextmenu', e => {
            if (!keepsContextMenu(e.target)) e.preventDefault();
        });
        // A browser tab: closing it, or Ctrl+W, asks first while in the world (a page can't stop it, only ask).
        target.addEventListener('beforeunload', e => {
            if (!this.native && this.inWorld) {
                e.preventDefault();
                e.returnValue = '';
            }
        });
        if (this.api) {
            const window = this.api.window.getCurrentWindow();
            void window.onCloseRequested(async event => {
                if (!this.inWorld) return;
                event.preventDefault();
                if (await this.askToClose()) await window.destroy();
            });
        }
    }

    async toggleFullscreen() {
        if (!this.api) return;
        const window = this.api.window.getCurrentWindow();
        await window.setFullscreen(!(await window.isFullscreen()));
    }
}
