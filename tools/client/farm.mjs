// Farm work in the real page (Docs/Design/53-hunting-and-working-together.md, Phase 4), on a field of its own (written
// here as a world file) with Hale, a farmer, at work in it: winter at midday (the dev calendar and clock); Ash opens
// Hale's menu and helps with the threshing; the work row says ×1.4, what it has paid and when the next spell is
// counted; Bo lends a paw (×2.0 each); Ash, made Gifted (Wind), winnows (×2.2 for the spell); a spell later both are paid
// from Hale's till.
// The world runs ten times as fast (--speed 10), so a spell is half a minute. Screenshots go to artifacts/screenshots/farm/.
//
//   node tools/client/farm.mjs [OUT]       (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/farm`;
mkdirSync(OUT, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
const tmp = mkdtempSync(join(tmpdir(), 'ratw-farm-'));

mkdirSync(join(tmp, 'cells'), {recursive: true});
writeFileSync(join(tmp, 'cells', 'fields.cell'), 'id: fields\nname: The Fields\ndescription: Stubble and a threshing floor.\nworld: 0 0 0\n' +
    'size: 40 40\noutdoors: true\nweather: clear\nwind: 0 0.5 1\nlighting: 1 1 warm\ngrid:\n' + `${','.repeat(40)}\n`.repeat(40));
writeFileSync(join(tmp, 'world.ratw'), 'RATW_WORLD 2\ncell "fields" "cells/fields.cell"\nterritory "fields" "fields" "-" 0\n' +
    'spawn "fields" 20.5 22.5\neconomy 1000 100 50 10 12\n' +
    'resident "hale" "Hale" "civilian" "farms the valley fields" "A farmer." "Hello." 40 "timber" "male" "average" "saddle" ' +
    '3 1 5 1 1 8 17 "-" 40 0 1 "fields" 10.5 10.5 "fields" 20.5 20.5 "fields" 10.5 11.5\n');

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
    const open = async who => {
        const browser = await Browser.launch({width: 1440, height: 940});
        browsers.push(browser);
        const page = await browser.open(`http://127.0.0.1:${port}/?identity=${who}`);
        await page.waitFor(`${S}.snapshot?.self?.id`, 20);
        return page;
    };
    const ash = await open('ash'), bo = await open('bo');
    // Winter, midday: Hale goes out to his threshing.
    for (let i = 0; i < 3; ++i) await ash.evaluate(`${S}.send({type: 'calendar', value: 'season'})`);
    await ash.evaluate(`${S}.send({type: 'time', value: 'day'})`);
    await ash.evaluate(`${S}.send({type: 'path', x: 21, y: 21})`);
    await bo.evaluate(`${S}.send({type: 'path', x: 19, y: 21})`);
    const hale = await ash.waitFor(`[...${S}.entities.values()].find(e => e.actions.includes('help with the threshing'))?.id`, 60);
    check(!!hale, 'winter, Hale at work: Ash may help with the threshing');
    await ash.evaluate(`${S}.openContextAt(${JSON.stringify(hale)}, [700, 420])`);
    await ash.waitFor(`[...document.querySelectorAll('.menu-item')].find(b => /Help with the threshing/.test(b.textContent))`, 10);
    await ash.screenshot(`${OUT}/1-help-with-the-threshing.png`);
    await ash.evaluate(`[...document.querySelectorAll('.menu-item')].find(b => /Help with the threshing/.test(b.textContent)).click()`);
    const row = await ash.waitFor(`document.querySelector('.work-row')?.textContent`, 10);
    check(/Threshing with .* · ×1\.4 · 0p earned · next spell in \d:\d\d/.test(row) && /you hold the flail/.test(row), `Ash's work row: ${row}`);
    // Bo lends a paw: two angles beside Hale's.
    const ashId = await bo.waitFor(`[...${S}.entities.values()].find(e => e.actions.includes('lend a paw'))?.id`, 15);
    await bo.evaluate(`${S}.sendAction('lend a paw', ${JSON.stringify(ashId)})`);
    const boRow = await bo.waitFor(`document.querySelector('.work-row')?.textContent`, 10);
    check(/× ?2\.0/.test(boRow) && /you gather and sort/.test(boRow), `Bo at 2.0: ${boRow}`);
    // Ash is Gifted (Wind): Winnow and Dry on the threshing, an angle of its own, lifts the joint for the spell (doc 53, 4).
    await ash.evaluate(`${S}.send({type: 'gift', gift: 'wind', quickened: false})`);
    await ash.waitFor(`${S}.snapshot?.self?.gift === 'wind'`, 10);
    await ash.evaluate(`${S}.send({type: 'giftwork', ability: 'winnow_and_dry'})`);
    const lifted = await ash.waitFor(`(t => /×2\.2/.test(t) && t)(document.querySelector('.work-row')?.textContent ?? '')`, 10);
    check(/×2\.2/.test(lifted) && /Winnow and Dry/.test(lifted), `a Gifted Wind wolf's lift on the threshing row: ${lifted}`);
    await ash.screenshot(`${OUT}/2a-winnow-and-dry.png`);
    // A spell (half a minute at ten times), both doing something in it.
    let paid = '';
    for (let i = 0; i < 40 && !paid; ++i) {
        for (const p of [ash, bo]) await p.evaluate(`${S}.send({type: 'typing', typing: false})`);
        await sleep(1500);
        paid = await ash.evaluate(`(document.body.innerText.match(/A spell's threshing beside [^\\n]*/) || [''])[0]`);
    }
    check(/A spell's threshing beside .*: .*; you are paid \dp \(\dp so far\)\./.test(paid), `Ash is paid from Hale's till: ${paid}`);
    const earned = await ash.waitFor(`(document.querySelector('.work-row')?.textContent.match(/(\\d+)p earned/) || [])[1]`, 10);
    check(Number(earned) >= 2, `the row says so: ${earned}p earned`);
    await ash.screenshot(`${OUT}/2-threshing-with-hale.png`);
    await bo.screenshot(`${OUT}/3-bo-lends-a-paw.png`);
    for (const p of [ash, bo])
        check(!p.console.some(l => l.startsWith('EXCEPTION')), `page errors: ${p.console.filter(l => l.startsWith('EXCEPTION')).join(' / ') || 'none'}`);
} finally {
    for (const r of results) console.log(r);
    for (const b of browsers) await b.close().catch(() => {});
    server.kill();
    await sleep(300);
    rmSync(tmp, {recursive: true, force: true});
}
