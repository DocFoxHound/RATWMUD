// Practice in the real page (Docs/Design/49-characters-and-earned-gifts.md, Phases 1 and 3): a skill grows by use and
// the player is told ("Your tracking sharpened (31)."); the Status window shows each skill out of its cap, "easing off"
// once today's practice in it reaches its soft limit; and the sheet shows the account's social level, from roleplay
// alone, with no "+N experience" lines. The Dev Console's `practice` message sets the numbers (the rules
// are the server's: Tests/practice_tests.cpp). Screenshots go to artifacts/screenshots/practice/.
//
//   node tools/client/practice.mjs [OUT]       (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist by default)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const tmp = mkdtempSync(join(tmpdir(), 'ratw-practice-'));
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/practice`;
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
mkdirSync(OUT, {recursive: true});
const port = 7900 + Math.floor(Math.random() * 90);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const server = spawn(binary, ['--port', String(port), '--web', web, '--save', `${tmp}/practice-save.json`,
    '--dev-identity', '--dev-tools'], {stdio: ['ignore', 'ignore', 'pipe'], env: {...process.env, RATW_AI: 'off'}});
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
    const page = await browser.open(`http://127.0.0.1:${port}/?identity=ada`);
    const S = 'window.ratw.game().state';
    const until = (condition, seconds = 5) => page.waitFor(condition, seconds).then(() => true).catch(() => false);
    await page.waitFor(`${S}.snapshot?.self?.skills`, 20);
    const skill = id => page.evaluate(`JSON.stringify((${S}.snapshot.self.skills ?? []).find(s => s.id === ${JSON.stringify(id)}) ?? null)`).then(JSON.parse);
    const tracking = await skill('tracking');
    check(tracking && tracking.value === 0 && tracking.cap === 100, `tracking starts at 0 of 100: ${JSON.stringify(tracking)}`);
    check((await page.evaluate(`(${S}.snapshot.self.attributes ?? []).length`)) === 7, 'seven attributes, each with its cap');

    // Just short of 31: nosing about passes it, and the player is told.
    await page.evaluate(`${S}.send({type: 'practice', skill: 'tracking', value: 30.98})`);
    await until(`(${S}.snapshot.self.skills ?? []).find(s => s.id === 'tracking')?.value >= 30.9`);
    await page.evaluate(`${S}.sendAction('smell')`);
    check(await until(`${S}.posts.some(p => /Your tracking sharpened \\(31\\)\\./.test(p.text))`), 'smelling about: "Your tracking sharpened (31)."');
    check(await until(`(${S}.snapshot.self.skills ?? []).find(s => s.id === 'tracking')?.value > 31`), 'and the skill is past 31');

    // Today's practice at the soft limit: the Status window says it is easing off.
    await page.evaluate(`${S}.send({type: 'practice', skill: 'tracking', value: 31.5, today: 2})`);
    check(await until(`(${S}.snapshot.self.skills ?? []).find(s => s.id === 'tracking')?.easing === true`), 'the server says tracking is easing off');
    await page.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'status', target: ''})`);
    check(await until(`[...document.querySelectorAll('.rpg-line')].some(r => /TRACKING/.test(r.innerText) && /31 \\/ 100/.test(r.innerText)) &&
        /EASING OFF TODAY · TRACKING/.test(document.body.innerText)`), 'the Status window: TRACKING 31 / 100, easing off today');
    await page.screenshot(`${OUT}/1-status-easing.png`);
    await page.evaluate(`${S}.setPresentationPage('character')`);
    check(await until(`/Scent 31 \\/ 100/.test(document.body.innerText)`), 'the character sheet: Scent 31 / 100');
    // Social standing (doc 49, Phase 3): the account's, from roleplay alone, with the way to the next level.
    check(await until(`/Social level 1 · Stranger/.test(document.body.innerText) && /0 social experience · 100 to the next level/.test(document.body.innerText)`),
        'the sheet: Social level 1 · Stranger, 100 to the next level');
    await page.screenshot(`${OUT}/2-character-sheet.png`);
    check(!(await page.evaluate(`${S}.posts.some(p => /experience \\(/.test(p.text))`)), 'no "+N experience (…)" lines any more');
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
