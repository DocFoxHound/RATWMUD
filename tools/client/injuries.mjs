// Injuries in the real page (Docs/Design/38-injuries.md, phases 3 and 4): a wolf's acute and lasting injuries, as the
// server sends them (set by hand here: the rules are the server's, Tests/injury_tests.cpp), on the character sheet and
// among what is wrong in the Status window; and what a closer look at another wolf shows of theirs.
// Screenshots go to artifacts/screenshots/injuries/.
//
//   node tools/client/injuries.mjs [OUT]       (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist by default)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const tmp = mkdtempSync(join(tmpdir(), 'ratw-injuries-'));
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/injuries`;
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
mkdirSync(OUT, {recursive: true});
const port = 7900 + Math.floor(Math.random() * 90);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const server = spawn(binary, ['--port', String(port), '--web', web, '--save', `${tmp}/injuries-save.json`,
    '--dev-identity', '--dev-tools'], {stdio: ['ignore', 'ignore', 'pipe']});
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
    const page = await browser.open(`http://127.0.0.1:${port}/?identity=ash`);
    const S = 'window.ratw.game().state';
    const until = (condition, seconds = 5) => page.waitFor(condition, seconds).then(() => true).catch(() => false);
    await page.waitFor(`${S}.snapshot?.self?.load`, 20);
    check(!(await page.evaluate(`${S}.snapshot.self.injuries`)), 'a new wolf has no injuries');
    const injuries = [
        {id: 'i1', kind: 'acute', name: 'Cracked rib', line: 'Cracked rib: moderate, healing (about 2 more days of rest)',
         does: 'Stamina back 20% slower; attacks cost 2 more breath.', severity: 'moderate', daysLeft: 2.2},
        {id: 'i2', kind: 'acute', name: 'Bitten left foreleg', line: 'Bitten left foreleg: minor, healing (about a day more of rest)',
         does: 'A sprint 5% slower.', severity: 'minor', daysLeft: 1.1},
        {id: 'i3', kind: 'lasting', name: 'Torn left ear', line: 'Torn left ear: in a fight with a bandit, early spring, year 1',
         does: 'Hearing 10% less.'}];
    await page.evaluate(`(() => {
        const s = ${S}, apply = s.applySnapshot;
        const put = () => { s.snapshot.self.injuries = ${JSON.stringify(injuries)}; };
        s.applySnapshot = function (...a) { const r = apply.apply(this, a); put(); return r; };
        put();
    })()`);
    await page.evaluate(`${S}.setPresentationPage('character')`);
    check(await until(`/INJURIES/i.test(document.querySelector('.injury-list')?.parentElement?.innerText ?? '')`),
        'the character sheet has its Injuries');
    const sheet = await page.evaluate(`document.querySelector('.injury-list')?.innerText ?? ''`);
    check(/Cracked rib: moderate, healing \(about 2 more days of rest\)/.test(sheet) && /Torn left ear: in a fight with a bandit/.test(sheet),
        `acute and lasting, each told: ${JSON.stringify(sheet)}`);
    await page.screenshot(`${OUT}/1-character-sheet.png`);
    await page.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'status', target: ''})`);
    check(await until(`[...document.querySelectorAll('.status-condition')].some(r => /cracked rib/i.test(r.innerText) && /2 days of rest/.test(r.innerText))`),
        'the Status window lists the acute ones among what is wrong');
    await page.screenshot(`${OUT}/2-status.png`);
    // A closer look at another wolf: what it shows of their injuries.
    await page.evaluate(`${S}.receiveEvent({type: 'inspect', id: 'npc_x', title: 'A dun wolf', name: 'A dun wolf', description: 'A lean dun wolf.',
        state: '', posture: 'standing', injuries: 'a torn left ear, and limping on a bitten foreleg'})`);
    check(await until(`/You notice a torn left ear, and limping on a bitten foreleg\./.test(document.body.innerText)`),
        'a closer look shows them');
    await page.screenshot(`${OUT}/3-look.png`);
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
