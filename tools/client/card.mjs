// The player card in the real page (Docs/Design/50-player-card-friends-safety.md, Phase 1): Ada writes her profile; Bo,
// a stranger, sees her description, Currently and glances but not her title; once she introduces herself he sees her
// title and motto; her OOC tab shows her lines and veils; her status shows in his In Sight list. Screenshots go to
// artifacts/screenshots/card/.
//
//   node tools/client/card.mjs [OUT]        (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist by default)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const tmp = mkdtempSync(join(tmpdir(), 'ratw-card-'));
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/card`;
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
mkdirSync(OUT, {recursive: true});
const port = 7900 + Math.floor(Math.random() * 90);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const server = spawn(binary, ['--port', String(port), '--web', web, '--save', `${tmp}/card-save.json`, '--dev-identity', '--dev-tools'],
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
        await page.waitFor(`${S}.snapshot?.self?.id && ${S}.profileRules`, 20);
        return page;
    };
    const ada = await open('ada'), bo = await open('bo');
    const until = (page, condition, seconds = 6) => page.waitFor(condition, seconds).then(() => true).catch(() => false);
    const adaId = await ada.evaluate(`${S}.selfId`);
    // Ada writes her profile.
    await ada.evaluate(`${S}.sendProfile('set', {fields: {description: 'A lean grey wolf with salt in her fur and a net-mender\\'s calm.',
        currently: 'mending nets by the pier, happy to chat', title: 'the Ferryman', motto: 'Across, and back.', pronouns: 'she/her',
        oocNotes: 'Walk up any time.', consent: {injury: 'ask', death: 'no'},
        glances: [{icon: 'scar', title: 'A fresh scar', line: 'over one eye', sense: 'sight'}, {icon: 'smoke', title: 'Woodsmoke', line: 'clinging to her coat', sense: 'scent'}]}})`);
    check(await until(ada, `${S}.profileOwn?.title === 'the Ferryman'`), 'Ada’s profile is saved');
    // Bo, a stranger: a closer look.
    await until(bo, `${S}.entities.has(${JSON.stringify(adaId)})`, 10);
    await bo.evaluate(`${S}.send({type: 'action', action: 'inspect', target: ${JSON.stringify(adaId)}})`);
    check(await until(bo, `document.querySelector('.profile-card')?.innerText.includes('mending nets by the pier')`), 'Bo sees her Currently');
    check(await bo.evaluate(`document.body.innerText.includes('A lean grey wolf with salt in her fur')`), 'and her description, over the fixed line');
    check(await bo.evaluate(`document.querySelector('.profile-card').innerText.includes('A fresh scar')`), 'and a glance');
    check(!(await bo.evaluate(`document.querySelector('.profile-card').innerText.includes('Ferryman')`)), 'but not her title: he doesn’t know her name');
    await bo.screenshot(`${OUT}/1-a-stranger-looks.png`);
    // Ada introduces herself; Bo looks again.
    await bo.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'close', target: ''})`);
    await ada.evaluate(`${S}.send({type: 'chat', text: "I'm ada."})`);
    await sleep(2500);
    await bo.evaluate(`${S}.send({type: 'action', action: 'inspect', target: ${JSON.stringify(adaId)}})`);
    check(await until(bo, `document.querySelector('.profile-card')?.innerText.includes('the Ferryman')`), 'introduced: her title and motto');
    await bo.screenshot(`${OUT}/2-after-an-introduction.png`);
    await bo.evaluate(`document.querySelector('.creator-tabs .tab[data-tab=ooc]').click()`);
    check(await until(bo, `document.querySelector('.profile-card')?.innerText.includes('Character injury: ask me first')`), 'her OOC tab: lines and veils');
    await bo.screenshot(`${OUT}/3-her-ooc-tab.png`);
    // Her status in Bo's In Sight.
    await bo.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'close', target: ''})`);
    await ada.evaluate(`${S}.sendProfile('status', {value: 'lfs'})`);
    check(await until(bo, `[...document.querySelectorAll('*')].some(n => n.children.length === 0 && /looking for a scene/.test(n.textContent))`),
        'Bo’s In Sight: looking for a scene');
    // Ada's own profile editor.
    await ada.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'profile', target: ''})`);
    check(await until(ada, `document.body.innerText.includes('YOUR PROFILE') && document.querySelector('[data-field=title]')?.value === 'the Ferryman'`),
        'her profile editor, filled in');
    await ada.screenshot(`${OUT}/4-your-profile.png`);
    for (const page of [ada, bo]) {
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
