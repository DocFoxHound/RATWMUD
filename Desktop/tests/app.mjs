// The real desktop client, launched with its e2e hook (src-tauri/src/e2e.rs), and its page as Client/tests/flow.mjs
// drives a page: evaluate(expression) and waitFor(expression). For tests/app.test.mjs and tests/look.mjs.
//
// Every app launched here is killed when the test process ends, however it ends (a failed launch, Ctrl+C).
import {spawn, spawnSync} from 'node:child_process';
import {existsSync} from 'node:fs';
import {createServer} from 'node:net';
import {createInterface} from 'node:readline';
import {resolve} from 'node:path';

export const app = process.env.RATW_DESKTOP ?? resolve(import.meta.dirname, '..', 'src-tauri', 'target', 'debug', process.platform === 'win32' ? 'ratw-desktop.exe' : 'ratw-desktop');
const sleep = ms => new Promise(r => setTimeout(r, ms));

/** Why the app can't be driven here (no build, a build without the hook), or '' when it can. */
export function whyNot() {
    if (!existsSync(app)) return `no build of the app (${app}): cd Desktop/src-tauri && cargo build --features e2e`;
    const check = spawnSync(app, ['--e2e-check'], {encoding: 'utf8', timeout: 20000});
    if (!String(check.stdout).includes('ratw-desktop e2e'))
        return `the app at ${app} was built without its e2e hook (npm run app builds it so): cd Desktop/src-tauri && cargo build --features e2e`;
    return '';
}

const running = new Set();
const killAll = () => { for (const c of running) try { c.kill('SIGKILL'); } catch { /* gone */ } };
process.once('exit', killAll);
for (const signal of ['SIGINT', 'SIGTERM'])
    process.once(signal, () => { killAll(); process.exit(130); });

/** The app, launched with `args`, and its page. */
export async function launch(args) {
    const server = createServer();
    await new Promise(r => server.listen(0, '127.0.0.1', r));
    const child = spawn(app, args, {env: {...process.env, RATW_E2E: `127.0.0.1:${server.address().port}`}, stdio: ['ignore', 'pipe', 'pipe']});
    running.add(child);
    const console = [];
    for (const stream of [child.stdout, child.stderr]) createInterface({input: stream}).on('line', line => {
        console.push(line);
        if (console.length > 200) console.shift();
    });
    const exited = new Promise(r => child.once('exit', code => { running.delete(child); r(code); }));
    let socket = null, timer = null;
    const waiting = new Map();
    let next = 0;
    const page = {
        console,
        exited,
        evaluate(expression, seconds = 15) {
            const id = ++next;
            return new Promise((resolve, reject) => {
                const t = setTimeout(() => {
                    waiting.delete(id);
                    reject(new Error(`no answer from the page: ${expression.slice(0, 120)}`));
                }, seconds * 1000);
                waiting.set(id, {resolve: v => { clearTimeout(t); resolve(v); }, reject: e => { clearTimeout(t); reject(e); }});
                socket.write(JSON.stringify({id, js: expression}) + '\n');
            });
        },
        /** Until the expression is truthy (a promise is awaited). A step a reloading page drops is asked again. */
        async waitFor(expression, seconds = 15) {
            const until = Date.now() + seconds * 1000;
            while (Date.now() < until) {
                const value = await page.evaluate(`(() => { try { return ${expression}; } catch { return undefined; } })()`, 2).catch(() => undefined);
                if (value) return value;
                await sleep(100);
            }
            throw new Error(`timed out waiting for ${expression}`);
        },
        /** Closes the window as its X does. */
        close() {
            socket.write(JSON.stringify({close: true}) + '\n');
        },
        async quit() {
            if (child.exitCode === null && child.signalCode === null && socket && !socket.destroyed) socket.write(JSON.stringify({quit: 0}) + '\n');
            const code = await Promise.race([exited, sleep(10000).then(() => { child.kill('SIGKILL'); return 'killed'; })]);
            socket?.destroy();
            return code;
        },
    };
    try {
        socket = await Promise.race([
            new Promise(r => server.once('connection', r)),
            exited.then(code => { throw new Error(`the app exited (${code}) before it connected: ${console.join('\n')}`); }),
            new Promise((_, reject) => { timer = setTimeout(() => reject(new Error(`the app never connected: ${console.join('\n')}`)), 60000); }),
        ]);
        clearTimeout(timer);
        server.close();
        createInterface({input: socket}).on('line', line => {
            let reply;
            try { reply = JSON.parse(line); } catch { return; }
            const w = waiting.get(reply.id);
            if (!w) return;
            waiting.delete(reply.id);
            if (reply.ok) w.resolve(reply.value);
            else w.reject(new Error(reply.value));
        });
        await page.waitFor('window.ratw && window.ratw.client && window.ratw.door()', 30);
        // Exceptions, and every reply the shell hands the page, kept from here on for the checks.
        await page.evaluate(`(() => {
            window.__e2eErrors = [];
            window.addEventListener('error', e => window.__e2eErrors.push(String(e.message)));
            window.addEventListener('unhandledrejection', e => window.__e2eErrors.push(String(e.reason)));
            window.__e2eReplies = [];
            // (At the page's shell, which every call to the shell goes through: Tauri's own object is frozen.)
            const shell = window.ratw.shell, invoke = shell.invoke.bind(shell);
            shell.invoke = async (command, args) => {
                const reply = await invoke(command, args);
                window.__e2eReplies.push(JSON.stringify(reply));
                return reply;
            };
            return true;
        })()`);
        return page;
    } catch (error) {
        clearTimeout(timer);
        server.close();
        child.kill('SIGKILL');
        socket?.destroy();
        throw error;
    }
}
