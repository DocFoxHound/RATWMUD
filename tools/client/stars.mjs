// Stars in the real page (Docs/Design/51-scenes-and-stars.md, Phases 1-2): Ash and Bo talk in a party scene and end it;
// Bo gives Ash a Gold Star from the scene's card and tags it Storyteller from the chips that follow; Ash's sheet shows
// "★ 1 star from 1 wolf" and Storyteller; Cy, a stranger, sees "★ A few stars" on Ash's card.
// Screenshots go to artifacts/screenshots/stars/.
//
//   node tools/client/stars.mjs [OUT]          (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist by default)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const tmp = mkdtempSync(join(tmpdir(), 'ratw-stars-'));
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/stars`;
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
mkdirSync(OUT, {recursive: true});
const port = 7900 + Math.floor(Math.random() * 90);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const server = spawn(binary, ['--port', String(port), '--web', web, '--save', `${tmp}/stars-save.json`, '--dev-identity', '--dev-tools'],
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
const Lines = [
    '"The river rose in the night and took the lower bridge with it, so we came the long way round by the mill."',
    '"Then you will have seen the miller\'s dog, the grey one that guards the ford and barks at every passing cart."',
    '"We did, and it followed us halfway to the crossroads before it lost interest and wandered back home again."',
    '"That dog has walked that road longer than I have been alive, and it still thinks the whole valley is its own."',
];
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
    const act = (page, action, target = '') =>
        page.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: ${JSON.stringify(action)}, target: ${JSON.stringify(target)}})`);
    const say = (page, text) => page.evaluate(`(() => { const s = ${S}; s.setChat(true); s.composer.text = ${JSON.stringify(text)}; s.submitPost(); })()`);
    const ashId = await ash.evaluate(`${S}.selfId`), boId = await bo.evaluate(`${S}.selfId`);
    await sleep(800);
    await ash.evaluate(`${S}.sendAction('invite', ${JSON.stringify(boId)})`);
    await bo.waitFor(`${S}.party?.invite`, 10);
    await act(bo, 'party_verb', 'accept');
    await ash.waitFor(`${S}.party?.members?.length === 2`, 10);
    await act(ash, 'party');
    await act(bo, 'party');
    for (let i = 0; i < 4; ++i) {
        if (i) await sleep(2200);
        await say(i % 2 ? bo : ash, Lines[i]);
    }
    await sleep(1500);
    await ash.evaluate(`${S}.sendAction('session_end')`);
    check(await until(bo, `!!${S}.snapshot?.self?.social?.ended?.starTargets?.length`, 10), 'the scene ends: Bo may star Ash');
    // The star, from the scene's card; then the tag chips.
    await bo.evaluate(`[...document.querySelectorAll('.scene-ended button')].find(b => b.textContent.startsWith('★'))?.click()`);
    check(await until(bo, `document.querySelector('.star-tags [data-tag="storyteller"]') !== null`), 'the star is given; tag chips follow');
    await bo.screenshot(`${OUT}/1-tag-the-star.png`);
    await bo.evaluate(`document.querySelector('.star-tags [data-tag="storyteller"]').click()`);
    check(await until(bo, `${S}.posts.some(p => p.text.includes('Tagged: Storyteller.'))`), 'tagged Storyteller');
    check(await until(bo, `document.querySelector('.star-tags') === null`), 'the chips go once it is tagged');
    // Ash's sheet.
    await act(ash, 'character');
    check(await until(ash, `document.body.innerText.includes('★ 1 star from 1 wolf') && document.body.innerText.includes('Storyteller 1')`),
        "Ash's sheet: one star from one wolf, Storyteller");
    await ash.screenshot(`${OUT}/2-her-sheet.png`);
    // Cy, a stranger, on her card.
    await cy.evaluate(`${S}.send({type: 'action', action: 'inspect', target: ${JSON.stringify(ashId)}})`);
    check(await until(cy, `document.body.innerText.includes('★ A few stars')`), 'Cy sees "★ A few stars" on her card');
    await cy.screenshot(`${OUT}/3-a-stranger-sees.png`);
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
