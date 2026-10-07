// Mute, block and report in the real page (Docs/Design/50-player-card-friends-safety.md, Phase 2): Bo pesters Ada; she
// mutes him from a line he said (⚑) and hears nothing more while Cy still does; she unmutes, then reports him with a
// block from his card; his words stop for her, and Cy's don't. Screenshots go to artifacts/screenshots/safety/.
//
//   node tools/client/safety.mjs [OUT]      (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist by default)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const tmp = mkdtempSync(join(tmpdir(), 'ratw-safety-'));
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/safety`;
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
mkdirSync(OUT, {recursive: true});
const port = 7900 + Math.floor(Math.random() * 90);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const server = spawn(binary, ['--port', String(port), '--web', web, '--save', `${tmp}/safety-save.json`, '--dev-identity', '--dev-tools'],
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
    const ada = await open('ada'), bo = await open('bo'), cy = await open('cy');
    const until = (page, condition, seconds = 6) => page.waitFor(condition, seconds).then(() => true).catch(() => false);
    const heard = (page, words) => page.evaluate(`${S}.posts.some(p => p.text.includes(${JSON.stringify(words)}))`);
    const say = async (page, text, channel = '') => {
        await page.evaluate(`${S}.send({type: 'chat', text: ${JSON.stringify(text)}, channel: ${JSON.stringify(channel)}})`);
        await sleep(1200);
    };
    const boId = await bo.evaluate(`${S}.selfId`);
    await say(bo, 'Hey. Hey you. Look at me.');
    check(await until(ada, `${S}.posts.some(p => p.text.includes('Look at me') && p.sequence !== undefined)`), 'Ada hears Bo, with the line numbered');
    // Mute from the line's flag.
    await ada.evaluate(`[...document.querySelectorAll('.post')].find(r => r.innerText.includes('Look at me')).querySelector('.line-flag').click()`);
    check(await until(ada, `document.body.innerText.includes('MUTE, BLOCK OR REPORT')`), 'the flag opens the safety menu');
    await ada.screenshot(`${OUT}/1-the-safety-menu.png`);
    await ada.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'MUTE').click()`);
    check(await until(ada, `${S}.safetyMarks.length === 1`), 'muted');
    await say(bo, 'Why are you ignoring me?');
    check(!(await heard(ada, 'ignoring me')), 'Ada no longer hears him');
    check(await until(cy, `${S}.posts.some(p => p.text.includes('ignoring me'))`), 'Cy still does');
    check(!(await heard(bo, 'muted')), 'Bo isn’t told');
    // Unmute, then report from his card with a block.
    await ada.evaluate(`${S}.sendSafety('unmute', {target: ${JSON.stringify(boId)}})`);
    check(await until(ada, `${S}.safetyMarks.length === 0`), 'unmuted');
    await say(bo, 'You are pathetic and nobody likes you.');
    check(await heard(ada, 'pathetic'), 'she hears him again');
    await ada.evaluate(`${S}.send({type: 'action', action: 'inspect', target: ${JSON.stringify(boId)}})`);
    check(await until(ada, `[...document.querySelectorAll('button')].some(b => b.textContent === 'REPORT')`), 'his card has Mute, Block and Report');
    await ada.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'REPORT').click()`);
    await until(ada, `document.body.innerText.includes('REPORT TO A DUNGEON MASTER')`);
    await ada.evaluate(`document.querySelector('.modal select, select').value = 'harassment'`);
    await ada.screenshot(`${OUT}/2-a-report.png`);
    await ada.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'SEND REPORT').click()`);
    check(await until(ada, `${S}.posts.some(p => p.text.includes('Reported. A Dungeon Master will look at it. And blocked.'))`), 'reported, and blocked');
    await say(bo, 'Still here, still talking.');
    await say(cy, 'Evening, both.');
    check(!(await heard(ada, 'Still here')) && await heard(ada, 'Evening, both.'), 'blocked: his words stop for her, Cy’s don’t');
    await ada.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'profile', target: ''})`);
    check(await until(ada, `document.body.innerText.includes('MUTED AND BLOCKED') && /· blocked/.test(document.body.innerText)`), 'her list shows him blocked');
    await ada.screenshot(`${OUT}/3-muted-and-blocked.png`);
    for (const page of [ada, bo, cy]) {
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
