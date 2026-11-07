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
}

export interface AskTextOptions extends AskOptions {
    value?: string;         // What the box starts with.
    password?: boolean;     // A password box (never kept).
    placeholder?: string;
}

let queue: Promise<unknown> = Promise.resolve();

/** Whether a dialog is open now (the game's keys leave it alone). */
export function dialogOpen(): boolean {
    return typeof document !== 'undefined' && !!document.querySelector('.ratw-dialog');
}

function show<T>(build: (box: HTMLElement, done: (value: T) => void) => HTMLElement | null, cancelled: T): Promise<T> {
    const run = () => new Promise<T>(resolve => {
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
            window.removeEventListener('keydown', keys, true);
            veil.remove();
            previous?.focus?.({preventScroll: true});
            resolve(value);
        };
        // Enter confirms and Escape cancels; no other key reaches the game while the dialog is open (typing still types).
        const keys = (e: KeyboardEvent) => {
            if (e.code === 'Escape') {
                e.preventDefault();
                done(cancelled);
            } else if ((e.code === 'Enter' || e.code === 'NumpadEnter') && confirm) {
                e.preventDefault();
                confirm.click();
            }
            e.stopPropagation();
        };
        const previous = document.activeElement as HTMLElement | null;
        const confirm = build(box, done);
        window.addEventListener('keydown', keys, true);
        document.body.append(veil);
        (box.querySelector('input') ?? confirm ?? box.querySelector('button'))?.focus({preventScroll: true});
    });
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
    }, false);
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
    }, null);
}
