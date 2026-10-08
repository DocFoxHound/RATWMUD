// The gathering howl in the real page (Docs/Design/51-scenes-and-stars.md, Phase 6): Ash howls; Bo, near, hears it
// (a sound, a line, a mark at the minimap's edge) and joins with JOIN THE HOWL; Cy sees one mark for the chorus, of two
// wolves; Ash's Howl button rests. Screenshots go to artifacts/screenshots/howl/.
//
//   node tools/client/howl.mjs [OUT]           (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist by default)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const tmp = mkdtempSync(join(tmpdir(), 'ratw-howl-'));
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/howl`;
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
mkdirSync(OUT, {recursive: true});
const port = 7900 + Math.floor(Math.random() * 90);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const server = spawn(binary, ['--port', String(port), '--web', web, '--save', `${tmp}/howl-save.json`, '--dev-identity', '--dev-tools'],
    {stdio: ['ignore', 'ignore', 'pipe'], env: {...process.env, RATW_AI: 'off'}});
let log = '';
server.stderr.on('data', d => { log += d; });
await sleep(1500);
const browsers = [];
const results = [];
let failed = false;
const check = (ok, what) => {
    results.push(`${ok ? 'ok  ' : 'FAIL'} ${what}`);
    failed ||= !ok;
};
const S = 'window.ratw.game().state';
try {
    const open = async who => {
        const browser = await Browser.launch({width: 1600, height: 1000});
        browsers.push(browser);
        const page = await browser.open(`http://127.0.0.1:${port}/?identity=${who}`);
        await page.waitFor(`${S}.snapshot?.self?.id`, 20);
        return page;
    };
    const ash = await open('ash'), bo = await open('bo'), cy = await open('cy');
    const until = (page, condition, seconds = 8) => page.waitFor(condition, seconds).then(() => true).catch(() => false);
    await sleep(800);
    await ash.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'Howl').click()`);
    check(await until(ash, `${S}.posts.some(p => p.text === 'You howl.')`), 'Ash howls from the Howl button');
    check(await until(bo, `${S}.howls.size === 1 && [...${S}.howls.values()][0].canJoin`), 'Bo hears it, near enough to join');
    check(await until(bo, `${S}.posts.some(p => /A howl rises to the/.test(p.text))`), 'with a line where he writes');
    check(await until(bo, `/A HOWL TO THE [\\s\\S]*JOIN THE HOWL/.test(document.querySelector('.scene-bar')?.innerText ?? '')`), 'and JOIN THE HOWL');
    await bo.screenshot(`${OUT}/1-a-howl-heard-bo.png`);
    await bo.evaluate(`[...document.querySelectorAll('.howl-call button')].find(b => b.textContent === 'JOIN THE HOWL').click()`);
    check(await until(bo, `${S}.posts.some(p => p.text === 'You join the howl.')`), 'Bo joins the howl');
    check(await until(cy, `${S}.howls.size === 1 && [...${S}.howls.values()][0].wolves === 2`), 'Cy: one mark for the chorus, of two wolves');
    check(await until(ash, `[...document.querySelectorAll('button')].find(b => b.textContent === 'Howl')?.disabled === true`), "Ash's voice rests");
    await cy.screenshot(`${OUT}/2-the-chorus-cy.png`);
    for (const page of [ash, bo, cy]) {
        const errors = page.console.filter(l => /EXCEPTION|error/i.test(l));
        check(!errors.length, `page errors: ${errors.length ? errors.join(' | ') : 'none'}`);
    }
} catch (e) {
    check(false, String(e?.stack ?? e));
} finally {
    for (const b of browsers) await b.close();
    server.kill();
    rmSync(tmp, {recursive: true, force: true});
}
console.log(results.join('\n'));
const tail = log.split('\n').filter(l => /error|warn/i.test(l)).slice(-5);
if (tail.length) console.log('server:', tail.join('\n'));
process.exit(failed ? 1 : 0);
