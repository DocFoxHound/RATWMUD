// A hand at a resident's bench in the real page (Docs/Design/53-hunting-and-working-together.md, Phase 7, part A), in a
// bakery of its own (written here as a world file) with Bea, the baker, at work: Ash opens Bea's menu and lends a paw at
// the workshop; the row says ×1.4 beside the maker, who leads; a spell later Ash is paid from the shop's till. The world
// runs ten times as fast (--speed 10), so a spell is half a minute. Screenshots go to artifacts/screenshots/bench/.
//
//   node tools/client/bench.mjs [OUT]       (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/bench`;
mkdirSync(OUT, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
const tmp = mkdtempSync(join(tmpdir(), 'ratw-bench-'));

mkdirSync(join(tmp, 'cells'), {recursive: true});
writeFileSync(join(tmp, 'cells', 'bakery.cell'), 'id: bakery\nname: The Loaf\ndescription: Ovens, a long table and flour on everything.\nworld: 0 0 0\n' +
    'size: 30 30\noutdoors: false\nweather: clear\nwind: 0 0 1\nlighting: 1 1 warm\ngrid:\n' + `${'.'.repeat(30)}\n`.repeat(30));
writeFileSync(join(tmp, 'world.ratw'), 'RATW_WORLD 2\ncell "bakery" "cells/bakery.cell"\nterritory "bakery" "bakery" "-" 0\n' +
    'spawn "bakery" 15.5 17.5\neconomy 1000 100 50 10 12\n' +
    'resident "bea" "Bea" "civilian" "baker at The Loaf" "A baker." "Hello." 40 "timber" "female" "average" "saddle" ' +
    '3 1 5 1 1 8 17 "-" 40 0 1 "bakery" 5.5 5.5 "bakery" 15.5 15.5 "bakery" 5.5 6.5\n');

const port = 19500 + Math.floor(Math.random() * 400);
const server = spawn(binary, ['--port', String(port), '--web', web, '--world', join(tmp, 'world.ratw'), '--save', `${tmp}/save.json`,
    '--dev-identity', '--dev-tools', '--speed', '10'], {stdio: ['ignore', 'pipe', 'pipe'], env: {...process.env, RATW_AI: 'off'}});
let log = '';
server.stdout.on('data', d => { log += d; });
server.stderr.on('data', d => { log += d; });
for (let i = 0; i < 120 && !/listening on port/.test(log); ++i) await sleep(250);
if (!/listening on port/.test(log)) throw new Error('The server did not start:\n' + log.slice(-2000));

const browsers = [];
const results = [];
const check = (ok, what) => {
    results.push(`${ok ? 'ok  ' : 'FAIL'} ${what}`);
    if (!ok) throw new Error(what);
};
const S = 'window.ratw.game().state';
try {
    const browser = await Browser.launch({width: 1440, height: 940});
    browsers.push(browser);
    const ash = await browser.open(`http://127.0.0.1:${port}/?identity=ash`);
    await ash.waitFor(`${S}.snapshot?.self?.id`, 20);
    await ash.evaluate(`${S}.send({type: 'time', value: 'day'})`);
    await ash.evaluate(`${S}.send({type: 'path', x: 16, y: 16})`);
    const bea = await ash.waitFor(`[...${S}.entities.values()].find(e => e.actions.includes('lend a paw at the workshop'))?.id`, 60);
    check(!!bea, 'Bea at her bench: Lend a paw at the workshop');
    await ash.evaluate(`${S}.openContextAt(${JSON.stringify(bea)}, [700, 420])`);
    await ash.waitFor(`[...document.querySelectorAll('.menu-item')].find(b => /Lend a paw at the workshop/.test(b.textContent))`, 10);
    await ash.screenshot(`${OUT}/1-lend-a-paw-at-the-workshop.png`);
    await ash.evaluate(`[...document.querySelectorAll('.menu-item')].find(b => /Lend a paw at the workshop/.test(b.textContent)).click()`);
    const row = await ash.waitFor(`document.querySelector('.work-row')?.textContent`, 10);
    check(/Work at the bench with .* · ×1\.4/.test(row) && /works the craft/.test(row) && /you hold and fetch/.test(row), `the bench row: ${row}`);
    let paid = '';
    for (let i = 0; i < 40 && !paid; ++i) {
        await ash.evaluate(`${S}.send({type: 'typing', active: false})`);
        await sleep(1500);
        paid = await ash.evaluate(`(document.body.innerText.match(/A spell at the bench beside [^\\n]*/) || [''])[0]`);
    }
    check(/the work goes faster for your paw; you are paid \dp/.test(paid), `paid from the shop's till: ${paid}`);
    await ash.screenshot(`${OUT}/2-at-the-bench.png`);
    check(!ash.console.some(l => l.startsWith('EXCEPTION')), `page errors: ${ash.console.filter(l => l.startsWith('EXCEPTION')).join(' / ') || 'none'}`);
} finally {
    for (const r of results) console.log(r);
    for (const b of browsers) await b.close().catch(() => {});
    server.kill();
    await sleep(300);
    rmSync(tmp, {recursive: true, force: true});
}
