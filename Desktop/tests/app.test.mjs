// The real desktop client end to end (Docs/Design/77, Phase 3): the Tauri app built with its e2e hook, against a
// real central and a ticket-mode world on this computer (Client/tests/harness.mjs). The same flow as the browser's
// test (Client/tests/flow.mjs), then what only the app has: the page can't run eval or leave its origin, central's
// tokens never reach the page, and "remember me" keeps a sign-in across a relaunch (in a throwaway --data-dir, with
// --token-store file, so the test never touches this computer's keychain).
//
//   npm --prefix Client run build && (cd Desktop/src-tauri && cargo build --features e2e) && node --test Desktop/tests/app.test.mjs
//
// It opens a real window for about half a minute. Skipped (with the reason) with no display, no e2e build, no
// Postgres or no built ratw_server/ratw_identity.
import {test, before, after} from 'node:test';
import assert from 'node:assert/strict';
import {existsSync, mkdtempSync, rmSync, statSync, readdirSync, readFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {app, launch} from './app.mjs';
import {startHarness} from '../../Client/tests/harness.mjs';
import {clientFlow, on, PASSWORD} from '../../Client/tests/flow.mjs';

let h, skip = '', data = '';
const sleep = ms => new Promise(r => setTimeout(r, ms));

before(async () => {
    if (!existsSync(app)) return void (skip = `no e2e build of the app (${app}): cd Desktop/src-tauri && cargo build --features e2e`);
    if (process.platform === 'linux' && !process.env.DISPLAY && !process.env.WAYLAND_DISPLAY) return void (skip = 'no display');
    h = await startHarness({});
    if (h.skip) return void (skip = h.skip);
    data = mkdtempSync(join(tmpdir(), 'ratw-desktop-e2e-'));
}, {timeout: 180000});

after(async () => {
    if (h && !h.skip) await h.stop();
    if (data) rmSync(data, {recursive: true, force: true});
});

const args = () => ['--central', h.central, '--token-store', 'file', '--data-dir', data];

test('the app: the whole flow, the page kept to itself, and a remembered sign-in', {timeout: 300000}, async t => {
    if (skip) return t.skip(skip);
    let page = await launch(args());
    try {
        assert.equal(await page.evaluate('location.protocol'), 'tauri:', 'the bundled page, not a world\'s');
        assert.equal(await page.evaluate('window.ratw.shell.native'), true);
        const info = await page.evaluate(`window.__TAURI__.core.invoke('central_info')`);
        assert.equal(info.store, 'file');
        assert.equal(info.remembered, false);

        // The flow, as in a browser.
        await clientFlow(page, h, {native: true});

        // The page is kept to itself: no eval (the CSP), no other site, no call the client doesn't make.
        assert.equal(await page.evaluate(`(() => { try { eval('1'); return 'ran'; } catch { return 'refused'; } })()`), 'refused');
        assert.equal(await page.evaluate(`(() => { try { new Function('return 1')(); return 'ran'; } catch { return 'refused'; } })()`), 'refused');
        await page.evaluate(`(() => { location.href = 'https://example.net/'; return true; })()`);
        await sleep(1500);
        assert.equal(await page.evaluate('location.protocol'), 'tauri:', 'the window stayed on the page');
        const refused = await page.evaluate(`window.__TAURI__.core.invoke('central_call', {method: 'POST', path: '/account/refresh', body: {}})`);
        assert.equal(refused.status, 400, 'refreshing is the shell\'s own');

        // F11's way: the page asks the shell's window for fullscreen, and back.
        const fullscreen = 'window.__TAURI__.window.getCurrentWindow().isFullscreen()';
        await page.evaluate('window.ratw.shell.toggleFullscreen()');
        await page.waitFor(`window.__TAURI__.window.getCurrentWindow().isFullscreen().then(f => f || null)`, 10);
        await page.evaluate('window.ratw.shell.toggleFullscreen()');
        await sleep(500);
        assert.equal(await page.evaluate(fullscreen), false);

        // "Remember me": register with it ticked; the page sees no token; only a refresh token is kept, for this user only.
        const {waitDoor, fill} = on(page, h);
        await page.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent.trim() === 'New here? Create an account').click()`);
        await fill('input[name=username]', 'rememberedwolf');
        await fill('input[name=password]', PASSWORD);
        await fill('input[name=birthYear]', '1990');
        await page.evaluate(`(() => { document.querySelector('input[name=remember]').checked = true; return true; })()`);
        await page.evaluate(`document.querySelector('form.door-login').requestSubmit()`);
        await waitDoor(`d.page === 'roster' && !d.busy`);
        const kept = readdirSync(data).filter(f => f.endsWith('.token'));
        assert.equal(kept.length, 1, 'one remembered sign-in');
        const token = readFileSync(join(data, kept[0]), 'utf8');
        if (process.platform !== 'win32') assert.equal(statSync(join(data, kept[0])).mode & 0o777, 0o600);
        assert.ok(!token.includes(PASSWORD), 'never the password');
        const seen = await page.evaluate(`JSON.stringify({local: {...localStorage}, session: {...sessionStorage}, ratw: Object.keys(window.ratw)})`);
        assert.ok(!seen.includes(token.trim()), 'the page never holds the token');
        assert.deepEqual(await page.evaluate('window.__e2eErrors'), [], 'no exceptions in the page');
        assert.equal(await page.quit(), 0);

        // A new launch: straight back in.
        page = await launch(args());
        const again = on(page, h);
        await again.waitDoor(`d.page === 'roster' && !d.busy`, 20);
        assert.match(await again.message(), /Welcome back, rememberedwolf/);
        assert.equal((await page.evaluate(`window.__TAURI__.core.invoke('central_info')`)).remembered, true);

        // Signing out forgets it; the next launch asks again.
        await page.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent.trim() === 'Sign out of account').click()`);
        await again.waitDoor(`d.page === 'login' && !d.busy`);
        assert.deepEqual(readdirSync(data).filter(f => f.endsWith('.token')), [], 'forgotten');
        assert.equal(await page.quit(), 0);
        page = await launch(args());
        await on(page, h).waitDoor(`d.page === 'login' && !d.busy`, 20);
        assert.deepEqual(await page.evaluate('window.__e2eErrors'), [], 'no exceptions in the page');
    } finally {
        await page.quit();
    }
});
