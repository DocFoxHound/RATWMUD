// Working together in the real page (Docs/Design/53-hunting-and-working-together.md, Phase 3), on a wild cell of its
// own (written here as a world file): Ash forages; Bo opens her menu and lends her a paw; both see the work row under
// the map (Foraging with … · ×1.8, who digs and who carries and sorts); Ash's next picking is shared, Bo told his share;
// Bo leaves off and the row goes. Screenshots go to artifacts/screenshots/together/.
//
//   node tools/client/together.mjs [OUT]   (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/together`;
mkdirSync(OUT, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
const tmp = mkdtempSync(join(tmpdir(), 'ratw-together-'));

// The hunt tests' wild cell: woodland and grass with scrub and a stream, 80 by 60.
mkdirSync(join(tmp, 'cells'), {recursive: true});
let grid = '';
for (let y = 0; y < 60; ++y) {
    let row = '';
    for (let x = 0; x < 80; ++x) row += (x * 7 + y * 13) % 11 === 0 ? 'Y' : (x * 3 + y * 5) % 17 === 0 ? 'B' : x === 70 ? '~' : ',';
    grid += row + '\n';
}
writeFileSync(join(tmp, 'cells', 'wilds.cell'), 'id: wilds\nname: The Wilds\ndescription: Woods and meadow.\nworld: 0 0 0\nsize: 80 60\n' +
    'outdoors: true\nweather: clear\nwind: 0 0.5 1\nlighting: 1 1 warm\ngrid:\n' + grid);
writeFileSync(join(tmp, 'world.ratw'), 'RATW_WORLD 2\ncell "wilds" "cells/wilds.cell"\nterritory "wilds" "wilds" "-" 0\n' +
    'spawn "wilds" 40.5 30.5\neconomy 1000 100 50 10 12\n');

const port = 19500 + Math.floor(Math.random() * 400);
const server = spawn(binary, ['--port', String(port), '--web', web, '--world', join(tmp, 'world.ratw'), '--save', `${tmp}/save.json`, '--dev-identity'],
    {stdio: ['ignore', 'pipe', 'pipe'], env: {...process.env, RATW_AI: 'off'}});
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
    // Bo comes beside her, in sight.
    const [ax, ay] = JSON.parse(await ash.evaluate(`JSON.stringify([${S}.snapshot.self.x, ${S}.snapshot.self.y])`));
    await bo.evaluate(`${S}.send({type: 'path', x: ${ax + 2}, y: ${ay}})`);
    await sleep(2500);
    // Ash forages (the Forage button); in Bo's menu of her: Lend a paw.
    await ash.evaluate(`[...document.querySelectorAll('.actions button')].find(b => b.textContent === 'Forage').click()`);
    const ashId = await bo.waitFor(`[...${S}.entities.values()].find(e => e.actions.includes('lend a paw'))?.id`, 15);
    check(!!ashId, 'Ash at work: Bo may lend her a paw');
    await bo.evaluate(`${S}.openContextAt(${JSON.stringify(ashId)}, [700, 420])`);
    const item = await bo.waitFor(`[...document.querySelectorAll('.menu-item')].map(b => b.textContent).find(t => /Lend a paw/.test(t))`, 10);
    check(!!item, `the menu offers it: ${item}`);
    await bo.screenshot(`${OUT}/1-lend-a-paw.png`);
    await bo.evaluate(`[...document.querySelectorAll('.menu-item')].find(b => /Lend a paw/.test(b.textContent)).click()`);
    const row = await ash.waitFor(`document.querySelector('.work-row')?.textContent`, 10);
    check(/Foraging with .* · ×1\.8/.test(row) && /you dig\b/.test(row) && /carries and sorts/.test(row) && /Leave/.test(row),
        `Ash's work row: ${row}`);
    const boRow = await bo.waitFor(`document.querySelector('.work-row')?.textContent`, 10);
    check(/Foraging with .* · ×1\.8/.test(boRow) && /you carry and sort/.test(boRow), `and Bo's: ${boRow}`);
    // Ash's next picking is shared.
    let gathered = '';
    for (let i = 0; i < 8 && !gathered; ++i) {
        await sleep(1500);
        await ash.evaluate(`[...document.querySelectorAll('.actions button')].find(b => b.textContent === 'Forage').click()`);
        await sleep(600);
        gathered = await ash.evaluate(`(document.body.innerText.match(/Together you gather[^\\n]*/) || [''])[0]`);
    }
    check(/Together you gather \d+ .*; your share is \d+\./.test(gathered), `a picking shared: ${gathered}`);
    await ash.screenshot(`${OUT}/2-foraging-together.png`);
    await bo.screenshot(`${OUT}/3-his-share.png`);
    // Bo leaves off: the row goes for both.
    await bo.evaluate(`[...document.querySelectorAll('.work-row button')].find(b => b.textContent === 'Leave').click()`);
    check(await ash.waitFor(`!document.querySelector('.work-row')`, 10), 'Bo leaves off: the joint ends');
    for (const p of [ash, bo])
        check(!p.console.some(l => l.startsWith('EXCEPTION')), `page errors: ${p.console.filter(l => l.startsWith('EXCEPTION')).join(' / ') || 'none'}`);
} finally {
    for (const r of results) console.log(r);
    for (const b of browsers) await b.close().catch(() => {});
    server.kill();
    await sleep(300);
    rmSync(tmp, {recursive: true, force: true});
}
