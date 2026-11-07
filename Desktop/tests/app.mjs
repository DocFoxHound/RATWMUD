// The real desktop client, launched with its e2e hook (src-tauri/src/e2e.rs), and its page as Client/tests/flow.mjs
// drives a page: evaluate(expression) and waitFor(expression). For tests/app.test.mjs and tests/look.mjs.
import {spawn} from 'node:child_process';
import {createServer} from 'node:net';
import {createInterface} from 'node:readline';
import {resolve} from 'node:path';

export const app = process.env.RATW_DESKTOP ?? resolve(import.meta.dirname, '..', 'src-tauri', 'target', 'debug', process.platform === 'win32' ? 'ratw-desktop.exe' : 'ratw-desktop');
const sleep = ms => new Promise(r => setTimeout(r, ms));

/** The app, launched with `args`, and its page. */
export async function launch(args) {
    const server = createServer();
    await new Promise(r => server.listen(0, '127.0.0.1', r));
    const child = spawn(app, args, {env: {...process.env, RATW_E2E: `127.0.0.1:${server.address().port}`}, stdio: ['ignore', 'pipe', 'pipe']});
    const console = [];
    for (const stream of [child.stdout, child.stderr]) createInterface({input: stream}).on('line', line => {
        console.push(line);
        if (console.length > 200) console.shift();
    });
    const exited = new Promise(r => child.once('exit', code => r(code)));
    const socket = await Promise.race([
        new Promise(r => server.once('connection', r)),
        exited.then(code => { throw new Error(`the app exited (${code}) before it connected: ${console.join('\n')}`); }),
        sleep(60000).then(() => { throw new Error(`the app never connected: ${console.join('\n')}`); }),
    ]);
    server.close();
    const waiting = new Map();
    let next = 0;
    createInterface({input: socket}).on('line', line => {
        let reply;
        try { reply = JSON.parse(line); } catch { return; }
        const w = waiting.get(reply.id);
        if (!w) return;
        waiting.delete(reply.id);
        if (reply.ok) w.resolve(reply.value);
        else w.reject(new Error(reply.value));
    });
    const page = {
        console,
        exited,
        evaluate(expression, seconds = 15) {
            const id = ++next;
            return new Promise((resolve, reject) => {
                const timer = setTimeout(() => {
                    waiting.delete(id);
                    reject(new Error(`no answer from the page: ${expression.slice(0, 120)}`));
                }, seconds * 1000);
                waiting.set(id, {resolve: v => { clearTimeout(timer); resolve(v); }, reject: e => { clearTimeout(timer); reject(e); }});
                socket.write(JSON.stringify({id, js: expression}) + '\n');
            });
        },
        async waitFor(expression, seconds = 15) {
            const until = Date.now() + seconds * 1000;
            while (Date.now() < until) {
                // (A page still loading drops the step: asked again.)
                const value = await page.evaluate(`(() => { try { return ${expression}; } catch { return undefined; } })()`, 2).catch(() => undefined);
                if (value) return value;
                await sleep(100);
            }
            throw new Error(`timed out waiting for ${expression}`);
        },
        async quit() {
            if (child.exitCode !== null) return child.exitCode;
            socket.write(JSON.stringify({quit: 0}) + '\n');
            const code = await Promise.race([exited, sleep(10000).then(() => { child.kill('SIGKILL'); return 'killed'; })]);
            socket.destroy();
            return code;
        },
    };
    process.once('exit', () => { try { child.kill('SIGKILL'); } catch { /* gone */ } });
    await page.waitFor('window.ratw && window.ratw.client && window.ratw.door()', 30);
    // Exceptions from here on are kept for the end.
    await page.evaluate(`(() => { window.__e2eErrors = []; window.addEventListener('error', e => window.__e2eErrors.push(String(e.message)));
        window.addEventListener('unhandledrejection', e => window.__e2eErrors.push(String(e.reason))); })()`);
    return page;
}
