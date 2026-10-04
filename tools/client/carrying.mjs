// Carrying in the real page (Docs/Design/35-items-crafting-industry.md, 1.2): a new wolf's load comes from the server
// with what each thing weighs; the status panel stays quiet while it is comfortable; a heavy and an overloaded load
// (set by hand: the rules are the server's, Tests/wear_tests.cpp) show the chip, the Status window's bar and condition,
// and the Belongings sheet's bar. Screenshots go to artifacts/screenshots/carrying/.
//
//   node tools/client/carrying.mjs [OUT]       (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist by default)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const tmp = mkdtempSync(join(tmpdir(), 'ratw-carrying-'));
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/carrying`;
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
mkdirSync(OUT, {recursive: true});
const port = 7900 + Math.floor(Math.random() * 90);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const server = spawn(binary, ['--port', String(port), '--web', web, '--save', `${tmp}/carrying-save.json`,
    '--dev-identity', '--dev-tools'], {stdio: ['ignore', 'ignore', 'pipe']});
let log = '';
server.stderr.on('data', d => { log += d; });
await sleep(1500);
let browser;
const results = [];
let failed = false;
const check = (ok, what) => {
    results.push(`${ok ? 'ok  ' : 'FAIL'} ${what}`);
    failed ||= !ok;
};
try {
    browser = await Browser.launch({width: 1600, height: 1000});
    const page = await browser.open(`http://127.0.0.1:${port}/?identity=ash`);
    const S = 'window.ratw.game().state';
    const until = (condition, seconds = 5) => page.waitFor(condition, seconds).then(() => true).catch(() => false);
    await page.waitFor(`${S}.snapshot?.self?.load`, 20);
    const load = await page.evaluate(`${S}.snapshot.self.load`);
    check(load.state === 'comfortable' && load.comfortable === 24.5 && load.carried > 0,
        `the server sends the load: ${JSON.stringify(load)}`);
    check(await page.evaluate(`${S}.snapshot.inventory.some(i => i.id === 'meal' && i.weight === 1.5)`), 'and what a meal weighs');
    check(await until(`getComputedStyle(document.querySelector('.load-chip')).display === 'none'`),
        'comfortable: no chip in the status panel');
    // Heavy, then overloaded, as the server would send them (kept over the snapshots that keep coming).
    const hold = state => page.evaluate(`(() => {
        const s = ${S}, apply = window.loadApply ?? s.applySnapshot;
        window.loadApply = apply;
        const put = () => { s.snapshot.self.load = ${JSON.stringify(state)}; };
        s.applySnapshot = function (...a) { const r = apply.apply(this, a); put(); return r; };
        put();
    })()`);
    await hold({carried: 41.2, comfortable: 24.5, state: 'heavy', pace: 7, drain: 1.34});
    await until(`/HEAVY LOAD/.test(document.querySelector('.load-chip').innerText)`);
    const chip = await page.evaluate(`document.querySelector('.load-chip').innerText`);
    check(/HEAVY LOAD · No faster than a run \(pace 7\)/.test(chip), `heavy: the chip says what it costs: ${JSON.stringify(chip)}`);
    await page.screenshot(`${OUT}/1-heavy-panel.png`);
    await page.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'status', target: ''})`);
    await until(`/HEAVY/.test(document.querySelector('.load-meter')?.innerText ?? '')`);
    const status = await page.evaluate(`document.querySelector('.load-meter')?.innerText ?? ''`);
    check(/LOAD 41\.2 LB OF 24\.5 LB · HEAVY/.test(status), `the Status window's bar: ${JSON.stringify(status)}`);
    check(await page.evaluate(`[...document.querySelectorAll('.status-condition')].some(r => /heavy load/i.test(r.innerText))`),
        'and the heavy load among what is wrong');
    await page.screenshot(`${OUT}/2-heavy-status.png`);
    // Worn armour: what it takes off a blow, on the Status window (a mail coat as the server lists one, worn).
    await page.evaluate(`(() => {
        const s = ${S}, apply = window.loadApply2 ?? s.applySnapshot;
        window.loadApply2 = apply;
        const coat = {id: 'mail_coat', name: 'Mail coat', icon: 'wear', description: '', equipped: true, quantity: 1, worn: 1,
            slot: 'body', places: ['body'], protect: 5, weight: 20};
        const put = () => { if (!s.snapshot.inventory.some(i => i.id === 'mail_coat')) s.snapshot.inventory = [...s.snapshot.inventory, coat]; };
        s.applySnapshot = function (...a) { const r = apply.apply(this, a); put(); return r; };
        put();
    })()`);
    check(await until(`/ARMOUR · HEAD — · THROAT — · BODY 5 · LEGS —/.test(document.querySelector('.rpg')?.innerText ?? '')`),
        'worn armour on the Status window, by hit zone: a mail coat guards the body');
    await hold({carried: 58, comfortable: 24.5, state: 'overloaded', pace: 0, drain: 1});
    await page.evaluate(`${S}.setPresentationPage('inventory')`);
    await until(`/OVERLOADED/.test(document.querySelector('.load-meter')?.innerText ?? '')`);
    const sheet = await page.evaluate(`document.querySelector('.load-meter')?.innerText ?? ''`);
    check(/OVERLOADED/.test(sheet) && /Walking only, and no fighting/.test(sheet), `overloaded, on the Belongings sheet: ${JSON.stringify(sheet)}`);
    check(await until(`[...document.querySelectorAll('.item .label')].some(l => /4\.5 LB/.test(l.innerText))`),
        'each thing with its weight');
    await page.screenshot(`${OUT}/3-overloaded-belongings.png`);
    const errors = page.console.filter(l => /EXCEPTION|error/i.test(l));
    check(!errors.length, `page errors: ${errors.length ? errors.join(' | ') : 'none'}`);
} catch (e) {
    check(false, String(e?.stack ?? e));
} finally {
    await browser?.close();
    server.kill();
    rmSync(tmp, {recursive: true, force: true});
}
console.log(results.join('\n'));
const tail = log.split('\n').filter(l => /error|warn/i.test(l)).slice(-5);
if (tail.length) console.log('server:', tail.join('\n'));
process.exit(failed ? 1 : 0);
