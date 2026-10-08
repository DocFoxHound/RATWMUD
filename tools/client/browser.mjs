// Drives the real browser client in headless Chromium over the DevTools protocol, for screenshots and the smokes that
// need the page itself (Docs/Design/27-browser-client.md). No dependencies: Node's own WebSocket and fetch.
//
//   const browser = await Browser.launch({width: 1600, height: 1000});
//   const page = await browser.open('http://127.0.0.1:7788/?identity=ash');
//   await page.waitFor('window.ratw.game()?.state.snapshot');
//   await page.key('KeyW', 500);                 // held for half a second, as a player holds it
//   await page.screenshot('shot.png');
//   await browser.close();
import {spawn} from 'node:child_process';
import {mkdtempSync, rmSync, writeFileSync, existsSync, readdirSync} from 'node:fs';
import {tmpdir, homedir} from 'node:os';
import {join} from 'node:path';

const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));

/** The Chromium Playwright installed, or RATW_CHROME, or one on the PATH. */
export function findChrome() {
    if (process.env.RATW_CHROME) return process.env.RATW_CHROME;
    const cache = join(homedir(), '.cache/ms-playwright');
    if (existsSync(cache))
        for (const dir of readdirSync(cache).filter(d => /^chromium-\d+$/.test(d)).sort().reverse()) {
            const chrome = join(cache, dir, 'chrome-linux64/chrome');
            if (existsSync(chrome)) return chrome;
        }
    for (const name of ['chromium', 'chromium-browser', 'google-chrome'])
        for (const dir of (process.env.PATH ?? '').split(':'))
            if (existsSync(join(dir, name))) return join(dir, name);
    throw new Error('No Chromium found: install one (npx playwright install chromium) or set RATW_CHROME.');
}

const Keys = {
    KeyW: ['w', 87], KeyA: ['a', 65], KeyS: ['s', 83], KeyD: ['d', 68], KeyE: ['e', 69], KeyI: ['i', 73], KeyC: ['c', 67],
    KeyL: ['l', 76], KeyM: ['m', 77], Enter: ['Enter', 13], Escape: ['Escape', 27], PageUp: ['PageUp', 33], PageDown: ['PageDown', 34],
    Digit1: ['1', 49], Digit2: ['2', 50], Digit3: ['3', 51], AltLeft: ['Alt', 18], ShiftLeft: ['Shift', 16], Backquote: ['`', 192],
};

class Page {
    constructor(socket) {
        this.socket = socket;
        this.next = 0;
        this.waiting = new Map();
        this.console = [];
        socket.addEventListener('message', message => {
            const m = JSON.parse(message.data);
            if (m.id && this.waiting.has(m.id)) {
                const {resolve, reject} = this.waiting.get(m.id);
                this.waiting.delete(m.id);
                if (m.error) reject(new Error(`${m.error.message} (${m.error.data ?? ''})`));
                else resolve(m.result);
            } else if (m.method === 'Runtime.consoleAPICalled')
                this.console.push(m.params.args.map(a => a.value ?? a.description).join(' '));
            else if (m.method === 'Runtime.exceptionThrown')
                this.console.push(`EXCEPTION ${m.params.exceptionDetails.exception?.description ?? m.params.exceptionDetails.text}`);
        });
    }

    send(method, params = {}) {
        const id = ++this.next;
        this.socket.send(JSON.stringify({id, method, params}));
        return new Promise((resolve, reject) => this.waiting.set(id, {resolve, reject}));
    }

    /** An expression's value in the page (awaited if it is a promise). */
    async evaluate(expression) {
        const r = await this.send('Runtime.evaluate', {expression, awaitPromise: true, returnByValue: true});
        if (r.exceptionDetails) throw new Error(`In the page: ${r.exceptionDetails.exception?.description ?? r.exceptionDetails.text}`);
        return r.result.value;
    }

    /** Waits until an expression is truthy; returns its value. */
    async waitFor(expression, seconds = 15) {
        const until = Date.now() + seconds * 1000;
        for (;;) {
            const value = await this.evaluate(`(() => { try { return ${expression}; } catch { return undefined; } })()`);
            if (value) return value;
            if (Date.now() > until) throw new Error(`Timed out waiting for: ${expression}`);
            await sleep(100);
        }
    }

    async goto(url) {
        await this.send('Page.navigate', {url});
        await this.waitFor('document.readyState === "complete"');
    }

    async keyEvent(type, code) {
        const [key, keyCode] = Keys[code] ?? [code, 0];
        await this.send('Input.dispatchKeyEvent', {type, code, key, windowsVirtualKeyCode: keyCode, nativeVirtualKeyCode: keyCode,
            text: type === 'keyDown' && key.length === 1 ? key : undefined});
    }

    /** Presses a key and holds it this long, as a player would. */
    async key(code, holdMs = 60) {
        await this.keyEvent('keyDown', code);
        await sleep(holdMs);
        await this.keyEvent('keyUp', code);
    }

    async type(text) {
        await this.send('Input.insertText', {text});
    }

    /** A click at a point of the page in CSS pixels. */
    async click(x, y, modifiers = 0) {
        for (const type of ['mousePressed', 'mouseReleased'])
            await this.send('Input.dispatchMouseEvent', {type, x, y, button: 'left', clickCount: 1, modifiers});
    }

    /** The pointer moved to a point of the page in CSS pixels (for :hover). */
    async hover(x, y) {
        await this.send('Input.dispatchMouseEvent', {type: 'mouseMoved', x, y});
    }

    async screenshot(path) {
        const {data} = await this.send('Page.captureScreenshot', {format: 'png'});
        writeFileSync(path, Buffer.from(data, 'base64'));
    }
}

export class Browser {
    static async launch({width = 1600, height = 1000, port = 9300 + Math.floor(Math.random() * 500)} = {}) {
        const profile = mkdtempSync(join(tmpdir(), 'ratw-browser-'));
        const chrome = spawn(findChrome(), ['--headless=new', `--remote-debugging-port=${port}`, `--user-data-dir=${profile}`,
            `--window-size=${width},${height}`, '--no-first-run', '--no-default-browser-check', '--disable-gpu',
            '--hide-scrollbars', '--force-device-scale-factor=1', 'about:blank'], {stdio: 'ignore'});
        const browser = new Browser(chrome, port, profile, width, height);
        // Never left running: if this script ends, is stopped (Ctrl-C, a timeout's SIGTERM) or fails, Chromium goes too.
        const gone = () => browser.kill();
        process.once('exit', gone);
        for (const signal of ['SIGINT', 'SIGTERM', 'SIGHUP'])
            process.once(signal, () => { gone(); process.exit(130); });
        process.once('uncaughtException', error => { gone(); console.error(error); process.exit(1); });
        for (let i = 0; i < 100; ++i) {
            await sleep(100);
            try {
                const targets = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
                if (targets.some(t => t.type === 'page')) return browser;
            } catch { /* starting */ }
        }
        await browser.close();
        throw new Error('Chromium did not start');
    }

    constructor(process, port, profile, width, height) {
        Object.assign(this, {process, port, profile, width, height});
    }

    async open(url) {
        const targets = await (await fetch(`http://127.0.0.1:${this.port}/json/list`)).json();
        const target = targets.find(t => t.type === 'page');
        const socket = new WebSocket(target.webSocketDebuggerUrl);
        await new Promise((resolve, reject) => {
            socket.addEventListener('open', resolve);
            socket.addEventListener('error', reject);
        });
        const page = new Page(socket);
        await page.send('Runtime.enable');
        await page.send('Page.enable');
        await page.send('Emulation.setDeviceMetricsOverride', {width: this.width, height: this.height, deviceScaleFactor: 1, mobile: false});
        await page.goto(url);
        return page;
    }

    /** Stops Chromium at once and removes its profile (what can be removed while it is still going). */
    kill() {
        try { this.process.kill('SIGKILL'); } catch { /* gone */ }
        try { rmSync(this.profile, {recursive: true, force: true}); } catch { /* busy */ }
    }

    /** Stops Chromium, waits for it to be gone, then removes its profile. */
    async close() {
        const exited = this.process.exitCode !== null || this.process.signalCode !== null ? Promise.resolve()
            : new Promise(resolve => this.process.once('exit', resolve));
        try { this.process.kill('SIGKILL'); } catch { /* gone */ }
        await Promise.race([exited, sleep(5000)]);
        try { rmSync(this.profile, {recursive: true, force: true, maxRetries: 3, retryDelay: 100}); } catch { /* busy */ }
    }
}
