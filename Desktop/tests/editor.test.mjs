// The Editor end to end (Docs/Design/78, Phase M4): the real desktop client (its e2e build) against a real Editor port
// (tools/world_admin.py, through tests/editor_world.py, on a throwaway database that is dropped afterwards), driven
// through the main window's own screens:
//   EDITOR, the first owner's Editor code, the address, signing in with the code, choosing a password (4b), the menu,
//   the accounts panel behind the password asked again (a wrong one refused), a co-builder added and their code shown
//   once, the DM tool in the Editor window (only signed files; no shell commands there; no token in its page; it can't
//   leave its origin or open windows), the session ending under the window and the same account taking it up again,
//   sign out (the window goes), a signed key rotation moving quietly, a changed key's red screen with no way to sign in
//   until it is forgotten by name, and the pin store and device token files.
//
//   cd Desktop && npm run test:editor     (builds the Editor, the page and the e2e build, then this)
//
// Needs a display, the local PostgreSQL (Database/.env, or its settings in the environment), python3 and openssl.
// Opens real windows for about a minute. Everything is kept in a throwaway --data-dir with --token-store file.
import {test, before, after} from 'node:test';
import assert from 'node:assert/strict';
import {spawn, execFileSync} from 'node:child_process';
import {cpSync, existsSync, mkdtempSync, readFileSync, readdirSync, rmSync, statSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join, resolve} from 'node:path';
import {createInterface} from 'node:readline';
import {launch, whyNot} from './app.mjs';

const root = resolve(import.meta.dirname, '..', '..');
const sleep = ms => new Promise(r => setTimeout(r, ms));
const NEW_PASSWORD = 'tallow-kettle-91-orchard';
let skip = '', scratch = '', data = '', world = null, page = null, lines = null;
const waiting = [];

/** The world's next JSON line (its replies to commands). */
function next() {
    return new Promise((resolve, reject) => {
        const t = setTimeout(() => reject(new Error('the world said nothing')), 60000);
        waiting.push(v => { clearTimeout(t); resolve(v); });
    });
}

async function tell(command) {
    world.stdin.write(command + '\n');
    return next();
}

before(async () => {
    if (process.platform === 'linux' && !process.env.DISPLAY && !process.env.WAYLAND_DISPLAY) return void (skip = 'no display');
    if ((skip = whyNot())) return;
    if (!existsSync(join(root, 'Editor', 'dist', 'index.html'))) return void (skip = 'build the Editor first: npm --prefix Editor run build');
    scratch = mkdtempSync(join(tmpdir(), 's2stage-editor-e2e-'));
    data = join(scratch, 'data');
    // The Editor bundle, signed as stage.sh signs the image's, with a key made for this run (a debug build trusts it
    // through RATW_EDITOR_DEV_KEY; a release build never does).
    const bundle = join(scratch, 'bundle');
    cpSync(join(root, 'Editor', 'dist'), bundle, {recursive: true});
    execFileSync('openssl', ['genpkey', '-algorithm', 'ed25519', '-out', join(scratch, 'key.pem')]);
    execFileSync('bash', [join(root, 'Package', 'stage.sh'), '--sign-bundle', bundle, '--release-key', join(scratch, 'key.pem')], {stdio: 'ignore'});
    const signed = JSON.parse(readFileSync(join(bundle, 'bundle.json'), 'utf8'));
    const der = execFileSync('openssl', ['pkey', '-in', join(scratch, 'key.pem'), '-pubout', '-outform', 'DER']);
    process.env.RATW_EDITOR_DEV_KEY = `${signed.key}:${der.subarray(der.length - 32).toString('base64')}`;
    world = spawn('python3', [join(import.meta.dirname, 'editor_world.py'), '--bundle', bundle], {stdio: ['pipe', 'pipe', 'inherit']});
    lines = createInterface({input: world.stdout});
    lines.on('line', line => { try { const v = JSON.parse(line); waiting.shift()?.(v); } catch { /* the helper's log lines */ } });
    try {
        world.info = await next();
    } catch (error) {
        skip = `the world didn't start (is the local PostgreSQL up?): ${error.message}`;
    }
}, {timeout: 120000});

after(async () => {
    if (page) await page.quit().catch(() => undefined);
    if (world && world.exitCode === null) {
        world.stdin.write('quit\n');
        await Promise.race([new Promise(r => world.once('exit', r)), sleep(20000)]);
        if (world.exitCode === null) world.kill('SIGKILL');
    }
    if (scratch) rmSync(scratch, {recursive: true, force: true});
});

const js = s => JSON.stringify(s);
const click = selector => page.evaluate(`(() => { const b = document.querySelector(${js(selector)}); if (!b) throw new Error('no ' + ${js(selector)}); b.click(); return true; })()`);
const fill = (selector, value) => page.evaluate(`(() => { const e = document.querySelector(${js(selector)}); if (!e) throw new Error('no ' + ${js(selector)});
    e.value = ${js(value)}; e.dispatchEvent(new Event('input', {bubbles: true})); return true; })()`);
const submit = selector => page.evaluate(`(() => { document.querySelector(${js(selector)}).requestSubmit(); return true; })()`);
const screen = () => page.evaluate(`document.querySelector('[data-editor]')?.dataset.editor ?? ''`);
const status = () => page.evaluate(`document.querySelector('.door-status')?.textContent ?? ''`);
const onScreen = (name, seconds = 20) => page.waitFor(`document.querySelector('[data-editor]')?.dataset.editor === ${js(name)} && !window.ratw.door().editorDoor.busy`, seconds);
/** The page's own dialog (askText): typed into and confirmed. */
async function answer(text) {
    await page.waitFor(`!!document.querySelector('.ratw-dialog input')`);
    await fill('.ratw-dialog input', text);
    await click('.ratw-dialog button.primary');
    await page.waitFor(`!document.querySelector('.ratw-dialog')`);
}
/** In the Editor window (the e2e hook's way; the window itself has no IPC). */
const inEditor = (expression, seconds = 15) => page.evaluate(expression, seconds, 'editor');
async function editorReady() {
    let last = '';
    for (let i = 0; i < 100; i++) {
        const ok = await inEditor(`document.readyState === 'complete' && !!document.querySelector('.studio, .dm-center')`, 3).catch(e => { last = String(e); return false; });
        if (ok) return true;
        await sleep(200);
    }
    throw new Error(`the Editor window never showed the studio (${last}); the app said:\n${page.console.slice(-30).join('\n')}`);
}

test('the Editor: a code, a password, the menu, accounts, the window, a rotation and a changed key', {timeout: 400000}, async t => {
    if (skip) return t.skip(skip);
    const {port, code, owner, fingerprint} = world.info;
    const address = `127.0.0.1:${port}`;
    page = await launch(['--central', 'http://127.0.0.1:9', '--token-store', 'file', '--data-dir', data]);

    // Screen 1: EDITOR on the sign-in page, with central down.
    await page.waitFor(`!!document.querySelector('button[data-action=editor]')`, 30);
    await click('button[data-action=editor]');
    await onScreen('address');

    // Screen 2: the Editor code, then the address (the first owner's code carries none).
    await fill('input[name=editorCode]', code);
    await submit('form.editor-code');
    await page.waitFor(`document.querySelector('.door-status').textContent.includes('now type your world')`);
    assert.ok(!(await page.evaluate('JSON.stringify(window.__e2eReplies)')).includes(code.slice(20, 60)), 'the code\'s secrets stay in the shell');
    await fill('input[name=editorAddress]', address);
    await submit('form.editor-address');
    // Screen 3 is skipped (the code pinned the key); screen 4 has the account and the code's password.
    await onScreen('sign-in');
    assert.equal(await page.evaluate(`document.querySelector('input[name=editorUsername]').value`), owner);
    assert.equal(await page.evaluate(`document.querySelector('.editor-notice')`), null, 'no "now at" notice for a code\'s first connection');
    await submit('form.editor-sign-in');
    // Screen 4b: the one-time password must be changed; a weak one is refused before sending.
    await onScreen('password');
    const boxes = `document.querySelectorAll('form.editor-password input[type=password]')`;
    assert.equal(await page.evaluate(`${boxes}.length`), 2, 'only the new password, twice: the shell holds the one-time one');
    await page.evaluate(`(() => { const b = ${boxes}; b[0].value = 'short'; b[1].value = 'short'; return true; })()`);
    await submit('form.editor-password');
    assert.match(await status(), /at least 12 characters/);
    await page.evaluate(`(() => { const b = ${boxes}; b[0].value = 'password1234'; b[1].value = 'password1234'; return true; })()`);
    await submit('form.editor-password');
    await page.waitFor(`document.querySelector('.door-status').textContent.includes('breaches')`);
    await page.evaluate(`(() => { const b = ${boxes}; b[0].value = ${js(NEW_PASSWORD)}; b[1].value = ${js(NEW_PASSWORD)}; return true; })()`);
    await submit('form.editor-password');
    await onScreen('menu');
    assert.equal(await page.evaluate(`[...document.querySelectorAll('input[type=password]')].every(p => p.value === '')`), true);
    assert.match(await page.evaluate(`document.querySelector('.editor-menu').textContent`), new RegExp(`Signed in as ${owner} \\(owner\\)`));
    await page.waitFor(`document.querySelector('[data-pill]').textContent === 'Open'`);

    // The pin store: by key, the address an alias; no secret in it. The device token: a file of its own, 0600.
    const pins = readFileSync(join(data, 'editor-worlds.json'), 'utf8');
    const store = JSON.parse(pins);
    assert.equal(store.keys.length, 1);
    assert.deepEqual(store.keys[0].addresses, [address]);
    assert.equal(store.keys[0].username, owner);
    const device = readdirSync(data).find(f => f.startsWith('editor-device-'));
    assert.ok(device, 'the device token is kept');
    const deviceToken = readFileSync(join(data, device), 'utf8').trim();
    if (process.platform !== 'win32') assert.equal(statSync(join(data, device)).mode & 0o777, 0o600);
    for (const secret of [deviceToken, NEW_PASSWORD, 'ratw-editor:']) assert.ok(!pins.includes(secret), 'no secret in editor-worlds.json');

    // The accounts panel, behind the password asked again: a wrong one refused, then the list and a co-builder.
    await click('button[data-action=editor-accounts]');
    await answer('not-my-password-at-all');
    await page.waitFor(`document.querySelector('.door-status').textContent === "That isn't your password."`);
    assert.equal(await page.evaluate(`document.querySelector('.editor-accounts')`), null);
    await click('button[data-action=editor-accounts]');
    await answer(NEW_PASSWORD);
    await page.waitFor(`!!document.querySelector('.editor-accounts [data-account=${js(owner)}]')`);
    await fill('input[name=coBuilder]', 'pell');
    await submit('form.editor-add');
    await answer(NEW_PASSWORD);
    await page.waitFor(`!!document.querySelector('.editor-accounts input.host-token') && !!document.querySelector('[data-account=pell]')`);
    const pellCode = await page.evaluate(`document.querySelector('.editor-accounts input.host-token').value`);
    assert.ok(pellCode.startsWith('ratw-editor:'));
    const record = JSON.parse(Buffer.from(pellCode.slice('ratw-editor:'.length), 'base64url').toString());
    assert.equal(record.u, 'pell');
    assert.equal(record.a, undefined, 'this computer\'s address is no use to a co-builder');
    assert.match(await page.evaluate(`document.querySelector('.editor-menu').textContent`), /Account pell added by/);

    // The DM tool in the Editor window: the signed files only, the session added by the shell.
    await click('button[data-action=editor-dm]');
    await editorReady();
    assert.match(await inEditor('location.href'), /^(editor:\/\/localhost|http:\/\/editor\.localhost)\/\?suite=dm$/);
    const log = (await tell('log')).log;
    const paths = log.map(l => l.path);
    for (const p of ['/bundle.json', '/', '/api/studio/session']) assert.ok(paths.includes(p), `${p} was asked for: ${paths.join(' ')}`);
    assert.ok(log.every(l => l.account === owner));
    const bearer = log[0].bearer;
    // No shell command answers the Editor window (no capability names it), and nothing secret is in its page.
    for (const [command, args] of [['editor_state', {}], ['central_call', {method: 'GET', path: '/worlds'}], ['plugin:window|set_title', {label: 'editor', value: 'x'}],
        ['plugin:event|emit', {event: 'editor-session', payload: {state: 'ended'}}], ['plugin:window|set_fullscreen', {label: 'editor', value: true}],
        ['plugin:window-state|save_window_state', {flags: 0}], ['world_status', {host: '127.0.0.1', port: 1, secure: false}]]) {
        const r = await inEditor(`(async () => { try { await window.__TAURI__.core.invoke(${js(command)}, ${js(args)}); return 'ANSWERED'; }
            catch (e) { return 'refused'; } })()`);
        assert.equal(r, 'refused', `${command} from the Editor window`);
    }
    const everything = await inEditor(`document.documentElement.outerHTML + JSON.stringify({...localStorage}) + JSON.stringify({...sessionStorage}) +
        document.cookie + Object.keys(window).join(',')`);
    for (const secret of [bearer, deviceToken, NEW_PASSWORD]) assert.ok(secret && !everything.includes(secret), 'no token or password in the Editor\'s page');
    // It can't read a file off the signed list, leave its origin, or open a window.
    assert.equal(await inEditor(`fetch('/editor/accounts').then(r => r.status)`), 403);
    assert.equal(await inEditor(`fetch('/not-signed.js').then(r => r.status)`), 404);
    assert.equal(await inEditor(`String(window.open('https://example.net/'))`), 'null');
    await inEditor(`(location.href = 'https://example.net/', true)`).catch(() => undefined);
    await sleep(1500);
    assert.match(await inEditor('location.origin'), /^(editor:\/\/localhost|http:\/\/editor\.localhost)$/, 'still on its own origin');

    // The session ends under the window (`ratw-editor sign-out-all` would do the same): its next request is a 401, and the
    // menu says so with the account filled in. Signing in again as the same account hands the window the new session.
    const pg = await import('node:child_process');
    pg.execFileSync('python3', ['-c', `import sys; sys.path.insert(0, ${js(join(root, 'tools'))}); import world_db as W, world_admin as A
c = W.connect('dev', 'dm', dbname=${js(world.info.database)}); A.end_all(c, 'test'); c.close()`]);
    assert.equal(await inEditor(`fetch('/api/studio/session').then(r => r.status)`), 401);
    await onScreen('sign-in');
    assert.match(await page.evaluate(`document.querySelector('.editor-notice').textContent`), /session ended/);
    assert.equal(await page.evaluate(`document.querySelector('input[name=editorUsername]').value`), owner);
    await fill('input[name=editorPassword]', NEW_PASSWORD);
    await submit('form.editor-sign-in');
    await onScreen('menu');
    assert.equal(await inEditor(`fetch('/api/studio/session').then(r => r.status)`), 200, 'the same window, the new session');

    // Sign out: the window goes, with its data directory.
    const windows = join(data, 'editor-windows');
    assert.equal(readdirSync(windows).length, 1, 'one data directory, the window\'s own');
    await click('button[data-action=editor-sign-out]');
    await onScreen('address');
    await sleep(1500);
    await assert.rejects(inEditor('1', 3), 'the Editor window is gone');
    for (let i = 0; i < 50 && readdirSync(windows).length; i++) await sleep(200);
    assert.deepEqual(readdirSync(windows), [], `its data directory went with it: ${page.console.slice(-12).join('\n')}`);
    await page.waitFor(`!document.querySelector('.ratw-dialog')`);

    // A signed rotation: the pin moves over quietly ("signed by its old one"), at the same address.
    const rotated = (await tell('rotate')).rotated;
    assert.equal(rotated.signed, true);
    await click(`button[data-address=${js(address)}]`);                           // Recent worlds' CONNECT
    await onScreen('sign-in');
    assert.match(await page.evaluate(`document.querySelector('.editor-notice').textContent`), /new certificate, signed by its old one/);
    const moved = JSON.parse(readFileSync(join(data, 'editor-worlds.json'), 'utf8'));
    assert.equal(moved.keys.length, 1);
    assert.notEqual(rotated.fingerprint, fingerprint);
    await fill('input[name=editorPassword]', NEW_PASSWORD);
    await submit('form.editor-sign-in');
    await onScreen('menu');
    assert.match(await page.evaluate(`document.querySelector('.editor-menu').textContent`), new RegExp(rotated.fingerprint));
    await click('button[data-action=editor-sign-out]');
    await onScreen('address');

    // A changed key (`new-certificate --leaked`): the red screen, both fingerprints, no sign-in from it; forgetting needs
    // the world's name; then a first connection again, trusted, and signing in works.
    const leaked = (await tell('leak')).rotated;
    assert.equal(leaked.signed, false);
    await click(`button[data-address=${js(address)}]`);
    await onScreen('key');
    const red = await page.evaluate(`document.querySelector('.editor-red').textContent`);
    assert.match(red, /Don't sign in/);
    assert.ok(red.includes(rotated.fingerprint) && red.includes(leaked.fingerprint), 'both fingerprints');
    assert.equal(await page.evaluate(`document.querySelector('form.editor-sign-in, input[type=password]')`), null, 'no way to sign in from it');
    const pinned = await page.evaluate(`window.__TAURI__.core.invoke('editor_sign_in', {address: ${js(address)}, username: ${js(owner)}, password: ${js(NEW_PASSWORD)}, code: false})`);
    assert.equal(pinned.ok, false, 'the shell refuses to sign in to the changed key too');
    await fill('input[name=worldName]', 'Ashfal');
    await click('.editor-red button.danger');
    await page.waitFor(`document.querySelector('.door-status').textContent.includes("isn't the name")`);
    await fill('input[name=worldName]', 'ashfall vale');
    await click('.editor-red button.danger');
    await page.waitFor(`!!document.querySelector('button[data-action=editor-trust]')`);
    const first = await page.evaluate(`document.querySelector('[data-editor=key]').textContent`);
    assert.ok(first.includes(leaked.fingerprint) && first.includes('This is the computer you\'re using'));
    await click('button[data-action=editor-trust]');
    await onScreen('sign-in');
    await fill('input[name=editorPassword]', NEW_PASSWORD);
    await submit('form.editor-sign-in');
    await onScreen('menu');
    await click('button[data-action=editor-sign-out]');
    await onScreen('address');

    // Nothing left behind: no Editor window's data directory, and no page error along the way.
    assert.deepEqual(await page.evaluate('window.__e2eErrors'), []);
    const replies = await page.evaluate('JSON.stringify(window.__e2eReplies)');
    for (const secret of [bearer, deviceToken]) assert.ok(!replies.includes(secret), 'no token in any reply the page got');
    await page.quit();
    page = null;
    await sleep(1000);
    assert.ok(!existsSync(windows) || readdirSync(windows).length === 0, `the Editor windows' data directories are gone: ${existsSync(windows) ? readdirSync(windows) : ''}`);
});
