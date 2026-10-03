// Parties in the real page (Docs/Design/32-parties-chapters-factions.md, Part 2): three players (a browser each). Ash
// invites Bo, Bo accepts; they talk to the party in and out of character; Cy challenges Ash, and Bo, watching, is
// called into the fight on Ash's side. Screenshots go to artifacts/screenshots/party/.
//
//   node tools/client/party.mjs [OUT]          (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist by default)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const tmp = mkdtempSync(join(tmpdir(), 'ratw-party-'));
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/party`;
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
mkdirSync(OUT, {recursive: true});
const port = 7900 + Math.floor(Math.random() * 90);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const server = spawn(binary, ['--port', String(port), '--web', web, '--save', `${tmp}/party-save.json`,
    '--dev-identity', '--dev-tools'], {stdio: ['ignore', 'ignore', 'pipe']});
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
try {
    const open = async who => {
        const browser = await Browser.launch({width: 1600, height: 1000});
        browsers.push(browser);
        const page = await browser.open(`http://127.0.0.1:${port}/?identity=${who}`);
        await page.waitFor('window.ratw.game()?.state.snapshot?.self?.id', 20);
        return page;
    };
    const ash = await open('ash'), bo = await open('bo'), cy = await open('cy');
    const S = 'window.ratw.game().state';
    const id = async p => p.evaluate(`${S}.selfId`);
    const ashId = await id(ash), boId = await id(bo), cyId = await id(cy);
    const act = (page, action, target = '') =>
        page.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: ${JSON.stringify(action)}, target: ${JSON.stringify(target)}})`);
    await sleep(800);
    // The invitation, and the party.
    await ash.evaluate(`${S}.sendAction('invite', ${JSON.stringify(boId)})`);
    await bo.waitFor(`${S}.party?.invite`, 10);
    await bo.screenshot(`${OUT}/1-invited-bo.png`);
    check(await bo.evaluate(`document.querySelector('.party-invite')?.offsetParent !== null`), 'Bo sees the invitation in the Party panel');
    await act(bo, 'party_verb', 'accept');
    await ash.waitFor(`${S}.party?.members?.length === 2`, 10);
    await sleep(500);
    check(await ash.evaluate(`${S}.entities.get(${JSON.stringify(boId)})?.rel === 'party'`), 'Bo is Ash\'s party mate on her map');
    await ash.screenshot(`${OUT}/2-party-ash.png`);
    // The party's two chats.
    await act(ash, 'party');
    await ash.evaluate(`(() => { const s = ${S}; s.setChat(true); s.composer.text = '"We make for the north gate at dusk."'; s.submitPost(); })()`);
    await sleep(800);                                   // (The server wants a moment between posts.)
    await act(ash, 'partyooc');
    await ash.evaluate(`(() => { const s = ${S}; s.setChat(true); s.composer.text = 'back in 5, grabbing tea'; s.submitPost(); })()`);
    await bo.waitFor(`${S}.posts.some(p => p.channel === 'partyooc')`, 10);
    await bo.waitFor(`${S}.posts.some(p => p.party)`, 10);
    check(!(await cy.evaluate(`${S}.posts.some(p => p.channel === 'partyooc')`)), 'Cy, not in the party, gets none of its OOC');
    await act(bo, 'party');
    await sleep(2500);                                  // (In-character lines are revealed as they are read.)
    await bo.screenshot(`${OUT}/3-party-chat-bo.png`);
    await act(ash, 'ic');
    // A fight: Cy challenges Ash, she accepts, Bo is called in.
    await cy.evaluate(`${S}.sendAction('challenge', ${JSON.stringify(ashId)})`);
    await ash.waitFor(`${S}.challenge`, 10);
    await ash.evaluate(`${S}.sendAction('accept')`);
    await bo.waitFor(`${S}.party?.pull`, 10);
    await sleep(300);
    check(await bo.evaluate(`${S}.entities.get(${JSON.stringify(cyId)})?.rel === 'hostile'`), 'Cy shows red to Bo while she fights his party');
    await bo.screenshot(`${OUT}/4-called-in-bo.png`);
    await bo.waitFor(`${S}.battle && !${S}.battle.observer`, 15);
    await sleep(500);
    const sides = await bo.evaluate(`(() => { const b = ${S}.battle; const side = id => b.fighters.find(f => f.id === id)?.side;
        return [side(${JSON.stringify(ashId)}), side(${JSON.stringify(boId)}), side(${JSON.stringify(cyId)})]; })()`);
    check(sides[0] === sides[1] && sides[2] !== sides[0], `Bo fights on Ash's side (${JSON.stringify(sides)})`);
    await bo.screenshot(`${OUT}/5-in-the-fight-bo.png`);
    const errors = [...ash.console, ...bo.console, ...cy.console].filter(l => /EXCEPTION|error/i.test(l));
    check(!errors.length, `page errors: ${errors.length ? errors.join(' | ') : 'none'}`);
} finally {
    for (const b of browsers) await b.close();
    server.kill();
    rmSync(tmp, {recursive: true, force: true});
}
console.log(results.join('\n'));
const tail = log.split('\n').filter(l => /error|warn/i.test(l)).slice(-5);
if (tail.length) console.log('server:', tail.join('\n'));
process.exit(failed ? 1 : 0);
