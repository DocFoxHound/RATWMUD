// The page's own dialogs, in place of window.confirm and window.prompt (Docs/Design/76-the-desktop-client.md, 2 and
// Phase 3): a webview may not support those (WKWebView shows nothing unless the app provides them; then confirm
// returns false and prompt null, and the action behind them silently never happens), and they can't be styled. Only text
// goes in (textContent), never markup. One dialog at a time; a second waits for the first.
//
//   if (await ask('Give up your lodging?')) send(...);
//   const price = await askText('Price apiece', '12');          // null when cancelled

export interface AskOptions {
    ok?: string;            // The confirming button ("OK").
    cancel?: string;        // The other ("Cancel").
    danger?: boolean;       // The confirming button warns (a deletion).
    now?: boolean;          // Shown at once: a dialog already open is cancelled, and the queue waits (the close question).
}

export interface AskTextOptions extends AskOptions {
    value?: string;         // What the box starts with.
    password?: boolean;     // A password box (never kept).
    placeholder?: string;
}

let queue: Promise<unknown> = Promise.resolve();
let gate: Promise<unknown> = Promise.resolve();      // A `now` dialog the queue waits for.
let cancelOpen: (() => void) | null = null;         // Cancels the dialog open now.

/** Whether a dialog is open now (the game's keys leave it alone). */
export function dialogOpen(): boolean {
    return typeof document !== 'undefined' && !!document.querySelector('.ratw-dialog');
}

function show<T>(build: (box: HTMLElement, done: (value: T) => void) => HTMLElement | null, cancelled: T, now = false): Promise<T> {
    const open = () => new Promise<T>(resolve => {
        if (typeof document === 'undefined') return resolve(cancelled);
        const veil = document.createElement('div');
        veil.className = 'ratw-dialog';
        veil.setAttribute('role', 'dialog');
        veil.setAttribute('aria-modal', 'true');
        const box = document.createElement('div');
        box.className = 'ratw-dialog-box';
        veil.append(box);
        let finished = false;
        const done = (value: T) => {
            if (finished) return;
            finished = true;
            if (cancelOpen === cancel) cancelOpen = null;
            window.removeEventListener('keydown', keys, true);
            veil.remove();
            previous?.focus?.({preventScroll: true});
            resolve(value);
        };
        const cancel = () => done(cancelled);
        // Enter confirms (from the text box or the confirming button; on Cancel it cancels, as a button does) and Escape
        // cancels; Tab stays in the dialog; no key reaches the game while it is open (typing still types). An IME's Enter
        // finishes the word, not the dialog.
        const keys = (e: KeyboardEvent) => {
            e.stopPropagation();
            if (e.isComposing || e.keyCode === 229) return;
            if (e.code === 'Escape') {
                e.preventDefault();
                done(cancelled);
            } else if ((e.code === 'Enter' || e.code === 'NumpadEnter') && confirm) {
                const target = e.target as HTMLElement | null;
                if (target instanceof HTMLButtonElement && target !== confirm && box.contains(target)) return;   // (Its own click.)
                e.preventDefault();
                confirm.click();
            } else if (e.code === 'Tab') {
                e.preventDefault();
                const stops = [...box.querySelectorAll<HTMLElement>('input, button')];
                if (!stops.length) return;
                const at = stops.indexOf(document.activeElement as HTMLElement);
                const next = at < 0 ? 0 : (at + (e.shiftKey ? stops.length - 1 : 1)) % stops.length;
                stops[next].focus({preventScroll: true});
            }
        };
        const previous = document.activeElement as HTMLElement | null;
        const confirm = build(box, done);
        window.addEventListener('keydown', keys, true);
        document.body.append(veil);
        cancelOpen = cancel;
        (box.querySelector('input') ?? confirm ?? box.querySelector('button'))?.focus({preventScroll: true});
    });
    if (now) {
        cancelOpen?.();
        const shown = open();
        gate = shown.catch(() => undefined);
        return shown;
    }
    // Queued: after the dialogs before it, and after any `now` dialog (which may have come since).
    const run = async () => {
        for (let seen: Promise<unknown> | null = null; seen !== gate;) {
            seen = gate;
            await seen;
        }
        return open();
    };
    const next = queue.then(run, run);
    queue = next.catch(() => undefined);
    return next;
}

function buttons(box: HTMLElement, options: AskOptions, onOk: () => void, onCancel: () => void): HTMLButtonElement {
    const row = document.createElement('div');
    row.className = 'ratw-dialog-actions';
    const cancel = document.createElement('button');
    cancel.type = 'button';
    cancel.className = 'secondary';
    cancel.textContent = options.cancel ?? 'Cancel';
    cancel.addEventListener('click', onCancel);
    const ok = document.createElement('button');
    ok.type = 'button';
    ok.className = options.danger ? 'primary danger' : 'primary';
    ok.textContent = options.ok ?? 'OK';
    ok.addEventListener('click', onOk);
    row.append(cancel, ok);
    box.append(row);
    return ok;
}

function text(box: HTMLElement, message: string) {
    const p = document.createElement('p');
    p.className = 'ratw-dialog-text';
    p.textContent = message;
    box.append(p);
}

/** A yes-or-no question: true for the confirming button. */
export function ask(message: string, options: AskOptions = {}): Promise<boolean> {
    return show<boolean>((box, done) => {
        text(box, message);
        return buttons(box, options, () => done(true), () => done(false));
    }, false, options.now === true);
}

/** A line of text: what was typed, or null when cancelled. */
export function askText(message: string, options: AskTextOptions = {}): Promise<string | null> {
    return show<string | null>((box, done) => {
        text(box, message);
        const input = document.createElement('input');
        input.type = options.password ? 'password' : 'text';
        input.className = 'ratw-dialog-input';
        input.value = options.value ?? '';
        input.placeholder = options.placeholder ?? '';
        if (options.password) input.autocomplete = 'current-password';
        box.append(input);
        return buttons(box, options, () => {
            const value = input.value;
            input.value = '';
            done(value);
        }, () => {
            input.value = '';
            done(null);
        });
    }, null, options.now === true);
}
