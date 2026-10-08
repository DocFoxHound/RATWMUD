// Hunting together in the real page (Docs/Design/53-hunting-and-working-together.md, Phase 2), on a wild cell of its
// own (written here as a world file): Ash goes out after game; Bo sees her hunt under his map (by her look) and joins it with
// Join hunt; Ash closes her hunt to strangers (Settings' Hunting partners), so Cy sees Ask to join and asks; Ash
// lets her in from the prompt under her map. Screenshots go to artifacts/screenshots/hunt/.
//
//   node tools/client/hunt.mjs [OUT]       (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/hunt`;
mkdirSync(OUT, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
const tmp = mkdtempSync(join(tmpdir(), 'ratw-hunt-'));

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
    const ash = await open('ash'), bo = await open('bo'), cy = await open('cy');
    // Ash goes out after game (the count is by chance: a few tries).
    for (let i = 0; i < 30 && !await ash.evaluate(`!!${S}.battle`); ++i) {
        await ash.evaluate(`${S}.send({type: 'hunt'})`);
        await sleep(1000);
    }
    check(await ash.evaluate(`!!${S}.battle`), 'Ash goes out after game');
    // Bo and Cy walk over to where she hunts; Bo sees her hunt under his map: Join hunt, and no button for the game.
    const hunts = `[...document.querySelectorAll('.fight-row')].map(r => r.textContent).find(t => /hunt ·/.test(t))`;
    let row = '';
    for (let i = 0; i < 15 && !row; ++i) {                // (Until she is in sight: woods and scrub hide a wolf.)
        const [ax, ay] = JSON.parse(await ash.evaluate(`JSON.stringify([${S}.snapshot.self.x, ${S}.snapshot.self.y])`));
        await bo.evaluate(`${S}.send({type: 'path', x: ${ax - 1 - (i % 3)}, y: ${ay + 1}})`);
        await cy.evaluate(`${S}.send({type: 'path', x: ${ax - 1 - (i % 3)}, y: ${ay - 1}})`);
        await sleep(2000);
        row = await bo.evaluate(hunts) ?? '';
    }
    check(/'s hunt · 1 hunting · 0 taken/.test(row) && /Join hunt/.test(row) && !/Join A|rabbit|badger|deer|pheasant/i.test(row.replace(/^.*taken/, '')),
        `Bo sees her hunt (by her look: he doesn't know her name), Join hunt, never the game's side: ${row}`);
    await bo.screenshot(`${OUT}/1-a-hunt-in-sight.png`);
    await bo.evaluate(`[...document.querySelectorAll('.fight-row button')].find(b => b.textContent === 'Join hunt').click()`);
    await bo.waitFor(`!!${S}.battle`, 10);
    check(await bo.evaluate(`${S}.battle.fighters.filter(f => f.side === 0).length`) === 2, 'Bo joins her hunt');
    // Ash closes it to strangers (Settings: Hunting partners); Cy may only ask.
    await ash.evaluate(`${S}.send({type: 'partners', kind: 'hunt', on: false})`);
    await ash.waitFor(`${S}.snapshot?.self?.noHuntPartners`, 10);
    const ask = await cy.waitFor(`[...document.querySelectorAll('.fight-row')].map(r => r.textContent).find(t => /Ask to join/.test(t))`, 30);
    check(/Ask to join/.test(ask) && !/Join hunt/.test(ask), `closed: Cy may ask: ${ask}`);
    await cy.evaluate(`[...document.querySelectorAll('.fight-row button')].find(b => b.textContent === 'Ask to join').click()`);
    const prompt = await ash.waitFor(`document.querySelector('.hunt-ask')?.textContent`, 15);
    check(/asks to join your hunt/.test(prompt), `Ash is asked: ${prompt}`);
    await ash.screenshot(`${OUT}/2-asked-to-join.png`);
    await ash.evaluate(`[...document.querySelectorAll('.hunt-ask button')].find(b => b.textContent === 'Let in').click()`);
    await cy.waitFor(`!!${S}.battle`, 10);
    check(await cy.evaluate(`${S}.battle.fighters.filter(f => f.side === 0).length`) >= 3, 'let in: Cy hunts with them');
    await cy.screenshot(`${OUT}/3-three-hunting.png`);
    for (const p of [ash, bo, cy])
        check(!p.console.some(l => l.startsWith('EXCEPTION')), `page errors: ${p.console.filter(l => l.startsWith('EXCEPTION')).join(' / ') || 'none'}`);
} finally {
    for (const r of results) console.log(r);
    for (const b of browsers) await b.close().catch(() => {});
    server.kill();
    await sleep(300);
    rmSync(tmp, {recursive: true, force: true});
}
