// The real desktop client end to end (Docs/Design/77, Phase 3): the Tauri app built with its e2e hook, against a
// real central and a ticket-mode world on this computer (Client/tests/harness.mjs). The same flow as the browser's
// test (Client/tests/flow.mjs), then what only the app has: the page can't run eval or leave its origin, central's
// tokens never reach the page, and "remember me" keeps a sign-in across a relaunch (in a throwaway --data-dir, with
// --token-store file, so the test never touches this computer's keychain).
//
//   cd Desktop && npm run test:app        (builds the page and the e2e build, then this)
//
// It opens a real window for about half a minute, and keeps everything (the sign-in, the window's size and place) in a
// throwaway --data-dir and its own central database. Skipped (with the reason) with no display, no e2e build, no
// Postgres or no built ratw_server/ratw_identity.
import {test, before, after} from 'node:test';
import assert from 'node:assert/strict';
import {existsSync, mkdtempSync, rmSync, statSync, readdirSync, readFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {launch, whyNot} from './app.mjs';
import {startHarness} from '../../Client/tests/harness.mjs';
import {clientFlow, on, PASSWORD} from '../../Client/tests/flow.mjs';

let h, skip = '', data = '';
const pages = [];
const sleep = ms => new Promise(r => setTimeout(r, ms));
/** The bundled page's origin: tauri://localhost, or http://tauri.localhost on Windows. */
const bundled = `location.protocol === 'tauri:' || location.origin === 'http://tauri.localhost'`;
const tokenFree = replies => replies.every(r => !/access_token|refresh_token|access_expires|refresh_expires/.test(r));

before(async () => {
    if (process.platform === 'linux' && !process.env.DISPLAY && !process.env.WAYLAND_DISPLAY) return void (skip = 'no display');
    if ((skip = whyNot())) return;
    // Its own database: the browser's test and look.mjs may be running beside it.
    h = await startHarness({database: `ratw_central_apptest_${process.pid}`});
    if (h.skip) return void (skip = h.skip);
    data = mkdtempSync(join(tmpdir(), 'ratw-desktop-e2e-'));
}, {timeout: 180000});

after(async () => {
    for (const p of pages) await p.quit().catch(() => undefined);
    if (h && !h.skip) await h.stop();
    if (data) rmSync(data, {recursive: true, force: true});
});

const args = () => ['--central', h.central, '--token-store', 'file', '--data-dir', data];
const open = async () => {
    const p = await launch(args());
    pages.push(p);
    return p;
};

test('the app: the whole flow, the page kept to itself, and a remembered sign-in', {timeout: 300000}, async t => {
    if (skip) return t.skip(skip);
    let page = await open();
    {
        assert.equal(await page.evaluate(bundled), true, 'the bundled page, not a world\'s');
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
        for (let i = 0; i < 50 && !page.console.some(l => l.includes('refused to leave the page for https://example.net/')); ++i) await sleep(100);
        assert.ok(page.console.some(l => l.includes('refused to leave the page for https://example.net/')), 'the shell refused it');
        assert.equal(await page.evaluate(bundled), true, 'the window stayed on the page');
        const refused = await page.evaluate(`window.__TAURI__.core.invoke('central_call', {method: 'POST', path: '/account/refresh', body: {}})`);
        assert.equal(refused.status, 400, 'refreshing is the shell\'s own');

        // F11, as the key: the guard asks the shell's window for fullscreen, and back.
        const f11 = `(() => { window.dispatchEvent(new KeyboardEvent('keydown', {code: 'F11', key: 'F11', bubbles: true, cancelable: true})); return true; })()`;
        const fullscreen = 'window.__TAURI__.window.getCurrentWindow().isFullscreen()';
        await page.evaluate(f11);
        await page.waitFor(`${fullscreen}.then(f => f || null)`, 10);
        await page.evaluate(f11);
        await page.waitFor(`${fullscreen}.then(f => !f || null)`, 10);

        // "Remember me": register with it ticked; the page sees no token; only a refresh token is kept, for this user only.
        const {waitDoor, fill} = on(page, h);
        await page.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent.trim() === 'New here? Create an account').click()`);
        await fill('input[name=username]', 'rememberedwolf');
        await fill('input[name=password]', PASSWORD);
        await fill('input[name=birthYear]', '1990');
        await page.evaluate(`(() => { document.querySelector('input[name=remember]').checked = true; return true; })()`);
        await page.evaluate(`document.querySelector('form.door-login').requestSubmit()`);
        await waitDoor(`d.page === 'menu' && !d.busy`);
        const kept = readdirSync(data).filter(f => f.endsWith('.token'));
        assert.equal(kept.length, 1, 'one remembered sign-in');
        const token = readFileSync(join(data, kept[0]), 'utf8');
        if (process.platform !== 'win32') assert.equal(statSync(join(data, kept[0])).mode & 0o777, 0o600);
        assert.ok(!token.includes(PASSWORD), 'never the password');
        // Every reply the shell handed the page (recorded before the page's own code saw it): no token in any.
        const replies = await page.evaluate('window.__e2eReplies');
        assert.ok(replies.some(r => r.includes('rememberedwolf')), 'the registration\'s reply was recorded');
        assert.ok(tokenFree(replies), 'no token in any reply to the page');
        const seen = await page.evaluate(`JSON.stringify({local: {...localStorage}, session: {...sessionStorage}})`);
        assert.ok(!seen.includes(token.trim()), 'nor in its storage');
        assert.deepEqual(await page.evaluate('window.__e2eErrors'), [], 'no exceptions in the page');
        assert.equal(await page.quit(), 0);
        assert.ok(existsSync(join(data, 'window-state.json')), 'the window\'s size and place kept in --data-dir, not over the real ones');

        // A new launch: straight back in.
        page = await open();
        const again = on(page, h);
        await again.waitDoor(`d.page === 'menu' && !d.busy`, 20);
        assert.match(await again.message(), /Welcome back, rememberedwolf/);
        assert.equal((await page.evaluate(`window.__TAURI__.core.invoke('central_info')`)).remembered, true);
        const resumed = await page.evaluate(`window.__TAURI__.core.invoke('central_resume')`);
        assert.equal(resumed.status, 200);
        assert.ok(tokenFree([JSON.stringify(resumed)]), 'no token in a resume\'s reply');
        assert.ok(tokenFree(await page.evaluate('window.__e2eReplies')), 'no token in a resume\'s reply');

        // Signing out forgets it; the next launch asks again.
        await page.evaluate(`document.querySelector('button[data-action=sign-out]').click()`);
        await again.waitDoor(`d.page === 'login' && !d.busy`);
        assert.deepEqual(readdirSync(data).filter(f => f.endsWith('.token')), [], 'forgotten');
        assert.deepEqual(await page.evaluate('window.__e2eErrors'), [], 'no exceptions in the page');
        assert.equal(await page.quit(), 0);
        page = await open();
        await on(page, h).waitDoor(`d.page === 'login' && !d.busy`, 20);
        assert.deepEqual(await page.evaluate('window.__e2eErrors'), [], 'no exceptions in the page');

        // The window's X, out of the world: the app closes, and exits on its own.
        page.close();
        const code = await Promise.race([page.exited, sleep(10000).then(() => 'still open')]);
        assert.equal(code, 0, 'closed and exited');
    }
});
