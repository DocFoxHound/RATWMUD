// Names in the real page (Docs/Design/32-parties-chapters-factions.md, 1.5): two players (a browser each). Each sees
// the other as they look; Bo takes an alias on his character sheet and introduces himself to Ash by it from his menu
// on her; Ash then knows him by that name. Screenshots go to artifacts/screenshots/names/.
//
//   node tools/client/names.mjs [OUT]          (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist by default)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const tmp = mkdtempSync(join(tmpdir(), 'ratw-names-'));
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/names`;
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
mkdirSync(OUT, {recursive: true});
const port = 7900 + Math.floor(Math.random() * 90);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const server = spawn(binary, ['--port', String(port), '--web', web, '--save', `${tmp}/names-save.json`, '--dev-identity', '--dev-tools'],
    {stdio: ['ignore', 'ignore', 'pipe']});
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
    const ash = await open('ash'), bo = await open('bo');
    const S = 'window.ratw.game().state';
    const ashId = await ash.evaluate(`${S}.selfId`), boId = await bo.evaluate(`${S}.selfId`);
    const calls = (page, id) => page.evaluate(`${S}.entities.get(${JSON.stringify(id)})?.name ?? '(not seen)'`);
    await sleep(800);
    const look = await calls(ash, boId);
    check(look !== 'bo' && /wolf/.test(look), `Ash sees Bo as he looks: "${look}"`);
    await ash.screenshot(`${OUT}/1-a-stranger-ash.png`);
    // Bo takes an alias on his character sheet.
    await bo.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'character', target: ''})`);
    await bo.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'name_add', target: 'Kestrel'})`);
    await bo.waitFor(`${S}.snapshot.self.names.aliases.includes('Kestrel')`, 10);
    await sleep(400);
    await bo.screenshot(`${OUT}/2-names-on-the-sheet-bo.png`);
    await bo.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'close', target: ''})`);
    const offered = await bo.evaluate(`${S}.entities.get(${JSON.stringify(ashId)})?.actions ?? []`);
    check(offered.includes('introduce as Kestrel'), `Bo's menu on Ash offers "introduce as Kestrel" (${offered.join(', ')})`);
    await bo.evaluate(`${S}.sendAction('introduce as Kestrel', ${JSON.stringify(ashId)})`);
    await ash.waitFor(`${S}.entities.get(${JSON.stringify(boId)})?.name === 'Kestrel'`, 10);
    check(true, 'Ash now knows Bo as Kestrel');
    check(await bo.evaluate(`${S}.posts.some(p => p.text.includes('You introduced yourself as Kestrel'))`), 'Bo gets the receipt');
    await sleep(2500);
    await ash.screenshot(`${OUT}/3-introduced-ash.png`);
    const errors = [...ash.console, ...bo.console].filter(l => /EXCEPTION|error/i.test(l));
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
