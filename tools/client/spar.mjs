// Sparring in the real page (Docs/Design/53-hunting-and-working-together.md, Phase 5), on a training yard of its own
// (written here as a world file) with Tam, who trains the recruits: Ash opens Bo's menu, chooses Challenge and then
// Spar (bruises only); Bo accepts; the fight reads "A duel as a spar (bruises only)". Cy, alone, works at the post and
// asks Tam to spar. Screenshots go to artifacts/screenshots/spar/.
//
//   node tools/client/spar.mjs [OUT]       (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/spar`;
mkdirSync(OUT, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
const tmp = mkdtempSync(join(tmpdir(), 'ratw-spar-'));

mkdirSync(join(tmp, 'cells'), {recursive: true});
writeFileSync(join(tmp, 'cells', 'yard.cell'), 'id: training_yard\nname: The Training Yard\ndescription: Packed earth, a post and a rack of blunted blades.\n' +
    'world: 0 0 0\nsize: 40 40\noutdoors: true\nweather: clear\nwind: 0 0.5 1\nlighting: 1 1 warm\ngrid:\n' + `${'.'.repeat(40)}\n`.repeat(40));
writeFileSync(join(tmp, 'world.ratw'), 'RATW_WORLD 2\ncell "training_yard" "cells/yard.cell"\nterritory "training_yard" "yard" "-" 0\n' +
    'spawn "training_yard" 20.5 22.5\neconomy 1000 100 50 10 12\n' +
    'resident "tam" "Tam" "civilian" "trains the recruits" "A trainer." "Hello." 40 "timber" "female" "average" "saddle" ' +
    '3 1 5 1 1 8 17 "-" 40 0 1 "training_yard" 8.5 8.5 "training_yard" 30.5 30.5 "training_yard" 8.5 9.5\n');

const port = 19500 + Math.floor(Math.random() * 400);
const server = spawn(binary, ['--port', String(port), '--web', web, '--world', join(tmp, 'world.ratw'), '--save', `${tmp}/save.json`,
    '--dev-identity', '--dev-tools'], {stdio: ['ignore', 'pipe', 'pipe'], env: {...process.env, RATW_AI: 'off'}});
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
    await ash.evaluate(`${S}.send({type: 'time', value: 'day'})`);
    check(await ash.waitFor(`[...document.querySelectorAll('.actions button')].some(b => b.textContent === 'Practise at the post' && b.offsetParent)`, 10),
        'on a training ground: Practise at the post');
    // Ash challenges Bo to a spar from his menu.
    const [ax, ay] = JSON.parse(await ash.evaluate(`JSON.stringify([${S}.snapshot.self.x, ${S}.snapshot.self.y])`));
    await bo.evaluate(`${S}.send({type: 'path', x: ${ax + 1}, y: ${ay}})`);
    const boId = await ash.waitFor(`[...${S}.entities.values()].find(e => e.kind === 'player' && !e.self && e.actions.includes('challenge') && Math.hypot(e.x - ${ax}, e.y - ${ay}) < 2)?.id`, 20);
    await ash.evaluate(`${S}.openContextAt(${JSON.stringify(boId)}, [700, 420])`);
    const items = await ash.waitFor(`[...document.querySelectorAll('.menu-item')].map(b => b.textContent).join(' | ') || ''`, 10);
    check(/Challenge/.test(items), `Bo's menu: ${items}`);
    await ash.evaluate(`[...document.querySelectorAll('.menu-item')].find(b => /Challenge/.test(b.textContent)).click()`);
    const spar = await ash.waitFor(`[...document.querySelectorAll('.menu-item')].map(b => b.textContent).find(t => /Spar \\(bruises only\\)/.test(t))`, 10);
    check(!!spar, `the challenge's terms offer a spar: ${spar}`);
    await ash.screenshot(`${OUT}/1-spar-bruises-only.png`);
    await ash.evaluate(`[...document.querySelectorAll('.menu-item')].find(b => /Spar \\(bruises only\\)/.test(b.textContent)).click()`);
    const asked = await bo.waitFor(`document.querySelector('.fight-alert')?.textContent`, 10);
    check(/challenges you to a fight as a spar \(bruises only\)/.test(asked), `Bo is asked: ${asked}`);
    await bo.evaluate(`${S}.sendAction('accept', '')`);
    await ash.waitFor(`${S}.battle?.terms === 'spar'`, 10);
    const versus = await ash.waitFor(`document.querySelector('.versus-terms')?.textContent`, 10);
    check(/A duel as a spar \(bruises only\)/.test(versus ?? ''), `the fight says so: ${versus}`);
    await ash.screenshot(`${OUT}/2-sparring.png`);
    // Cy, alone: the post, then Tam.
    const cy = await open('cy');
    await cy.evaluate(`[...document.querySelectorAll('.actions button')].find(b => b.textContent === 'Practise at the post').click()`);
    const post = await cy.waitFor(`(document.body.innerText.match(/You work at the post[^\\n]*/) || [''])[0]`, 10);
    check(/bites, feints and footwork/.test(post), `Cy works at the post: ${post}`);
    await cy.evaluate(`${S}.send({type: 'path', x: 29, y: 30})`);
    const tam = await cy.waitFor(`[...${S}.entities.values()].find(e => e.actions.includes('ask to spar'))?.id`, 40);
    check(!!tam, 'beside Tam: Ask to spar');
    await cy.evaluate(`${S}.openContextAt(${JSON.stringify(tam)}, [700, 420])`);
    await cy.waitFor(`[...document.querySelectorAll('.menu-item')].find(b => /Ask to spar/.test(b.textContent))`, 10);
    await cy.evaluate(`[...document.querySelectorAll('.menu-item')].find(b => /Ask to spar/.test(b.textContent)).click()`);
    await cy.waitFor(`${S}.battle?.terms === 'spar'`, 10);
    check(!(await cy.evaluate(`${S}.battle.crime`)), 'a spar with the trainer, no assault');
    await cy.screenshot(`${OUT}/3-sparring-the-trainer.png`);
    for (const p of [ash, bo, cy])
        check(!p.console.some(l => l.startsWith('EXCEPTION')), `page errors: ${p.console.filter(l => l.startsWith('EXCEPTION')).join(' / ') || 'none'}`);
} finally {
    for (const r of results) console.log(r);
    for (const b of browsers) await b.close().catch(() => {});
    server.kill();
    await sleep(300);
    rmSync(tmp, {recursive: true, force: true});
}
