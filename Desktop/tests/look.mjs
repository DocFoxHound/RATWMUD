// A look at the real window (doc 77, Phase 0's open check: does it draw?): the app against a local central and world,
// a screenshot of the sign-in, the world list, and the world itself, written to Desktop/spike/out/look-*.png (KDE's
// spectacle; elsewhere, look at the window). Opens a window for about half a minute.
//
//   cd Desktop && npm run test:app (or: cd Desktop/src-tauri && cargo build --features e2e), then
//   node Desktop/tests/look.mjs [extra app args...]
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {launch, whyNot} from './app.mjs';
import {startHarness} from '../../Client/tests/harness.mjs';
import {on, PASSWORD} from '../../Client/tests/flow.mjs';

const out = join(import.meta.dirname, '..', 'spike', 'out');
mkdirSync(out, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const why = whyNot();
if (why) throw new Error(why);
const h = await startHarness({database: `ratw_central_look_${process.pid}`});
if (h.skip) throw new Error(h.skip);
const data = mkdtempSync(join(tmpdir(), 'ratw-desktop-look-'));
let page;
const shot = async name => {
    await sleep(1500);
    try {
        execFileSync('spectacle', ['-b', '-n', '-a', '-o', join(out, `look-${name}.png`)], {stdio: 'ignore', timeout: 15000});
        console.log(`look-${name}.png`);
    } catch (e) { console.log(`no screenshot (${e.message})`); }
};
try {
    page = await launch(['--central', h.central, '--token-store', 'memory', '--data-dir', data, ...process.argv.slice(2)]);
    const {waitDoor, fill, click, clickText, waitGame} = on(page, h);
    await shot('signin');
    await clickText('New here? Create an account');
    await fill('input[name=username]', 'lookwolf');
    await fill('input[name=password]', PASSWORD);
    await fill('input[name=birthYear]', '1990');
    await page.evaluate(`document.querySelector('form.door-login').requestSubmit()`);
    await waitDoor(`d.page === 'roster' && !d.busy`);
    await page.evaluate(`window.ratw.page('creator')`);
    await shot('creator');
    await click('.creator-tabs [data-tab=name]');
    await fill('.creator-panel input[type=text]', 'lookwolf');
    await clickText('REVIEW CHARACTER');
    await clickText('CONFIRM & CREATE');
    await waitDoor(`d.page === 'roster' && d.characters.length === 1 && !d.busy`);
    await click('button[data-action=enter]');
    await waitDoor(`d.page === 'worlds' && !d.busy`);
    await page.waitFor(`document.querySelector('.world-status')?.textContent.includes('of 100 wolves')`);
    await shot('worlds');
    await click(`button[data-world="${h.world.id}"]`);
    await waitDoor(`d.page === 'arrive'`, 30);
    await clickText('ARRIVE');
    await waitGame();
    await sleep(2000);
    await shot('world');
    // (No frames at all when nothing is drawn: the screen off or locked, the window hidden. Not the app's fault.)
    const fps = await page.evaluate(`new Promise(r => { let n = 0; const t0 = performance.now(); const f = () => { if (++n < 120) requestAnimationFrame(f); else r(Math.round(n * 1000 / (performance.now() - t0))); }; requestAnimationFrame(f); })`, 10)
        .catch(() => null);
    console.log(fps === null ? 'no frames drawn: is the screen off or locked? (the screenshots will be blank too)' : `frames a second in the world: ${fps}`);

    // The window's X in the world asks first (once, however often it is clicked); Stay stays, Leave closes.
    page.close();
    page.close();
    await page.waitFor(`document.querySelector('.ratw-dialog')`, 10);
    await sleep(500);
    assert.equal(await page.evaluate(`document.querySelectorAll('.ratw-dialog').length`), 1, 'one close question');
    await shot('close-question');
    await click('.ratw-dialog .secondary');
    await page.waitFor(`!document.querySelector('.ratw-dialog')`);
    await sleep(500);
    assert.equal(await page.evaluate(`document.querySelectorAll('.ratw-dialog').length`), 0, 'no second question queued');
    assert.equal(await page.evaluate('window.ratw.shell.inWorld'), true, 'Stay stays');
    page.close();
    await page.waitFor(`document.querySelector('.ratw-dialog')`, 10);
    await click('.ratw-dialog .primary');
    const code = await Promise.race([page.exited, sleep(10000).then(() => 'still open')]);
    assert.equal(code, 0, 'Leave closes the window and the app exits');
    console.log('the close question: asked once, Stay stays, Leave exits');
} finally {
    await page?.quit();
    await h.stop();
    rmSync(data, {recursive: true, force: true});
}
