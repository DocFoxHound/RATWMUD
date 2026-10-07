// Friends and private messages in the real page (Docs/Design/50-player-card-friends-safety.md, Phase 3): Ada asks Bo
// to be friends by handle from FRIENDS; he sees the request counted on the menu and accepts; his handle then shows
// under his wolf in Ada's In Sight but never in Cy's; he stops sharing and it goes; she writes to him from MESSAGE on
// the PRIVATE tab and he reads it; he leaves, she writes again, and it reaches him when he comes back. Then a circle:
// Ada makes one, invites him, plans a night, and writes in its tab; he reads it, and Cy never does.
// Screenshots go to artifacts/screenshots/friends/.
//
//   node tools/client/friends.mjs [OUT]      (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist by default)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const tmp = mkdtempSync(join(tmpdir(), 'ratw-friends-'));
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/friends`;
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
mkdirSync(OUT, {recursive: true});
const port = 7900 + Math.floor(Math.random() * 90);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const server = spawn(binary, ['--port', String(port), '--web', web, '--save', `${tmp}/friends-save.json`, '--dev-identity', '--dev-tools'],
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
    const ada = await open('ada'), cy = await open('cy');
    let bo = await open('bo');
    const until = (page, condition, seconds = 6) => page.waitFor(condition, seconds).then(() => true).catch(() => false);
    const clickButton = (page, text) => page.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === ${JSON.stringify(text)}).click()`);
    const sightOf = (page, name) => page.evaluate(`[...document.querySelectorAll('.sight-row, .sight li, [class*=sight] *')]
        .map(r => r.innerText).filter(t => t.includes(${JSON.stringify(name)})).join(' | ')`);
    for (const [page, handle] of [[ada, 'Adder'], [bo, 'Bobbin'], [cy, 'Cypress']]) {
        await page.evaluate(`${S}.sendProfile('handle', {handle: ${JSON.stringify(handle)}})`);
        check(await until(page, `${S}.account?.handle === ${JSON.stringify(handle)}`), `${handle} chosen`);
    }
    // Ada asks by handle, from FRIENDS.
    await clickButton(ada, 'FRIENDS');
    check(await until(ada, `document.body.innerText.includes('ASK TO BE FRIENDS')`), 'FRIENDS opens from the menu');
    await ada.evaluate(`(() => { const i = document.querySelector('[data-field="friend-handle"]'); i.value = 'Bobbin'; })()`);
    await clickButton(ada, 'ASK TO BE FRIENDS');
    check(await until(ada, `${S}.friendRequestsOut.length === 1`), 'the request is waiting');
    check(await until(bo, `[...document.querySelectorAll('button')].some(b => b.textContent === 'FRIENDS · 1')`), 'Bo sees it counted on the menu');
    await clickButton(bo, 'FRIENDS · 1');
    check(await until(bo, `document.body.innerText.includes('ASKING YOU') && document.body.innerText.includes('Adder')`), 'and in FRIENDS');
    await bo.screenshot(`${OUT}/1-a-request.png`);
    await bo.evaluate(`[...document.querySelectorAll('.friend-row button')].find(b => b.textContent === 'ACCEPT').click()`);
    check(await until(ada, `${S}.friends.some(f => f.handle === 'Bobbin' && f.online && f.character)`), 'friends: Ada sees Bobbin here, and his wolf');
    await ada.screenshot(`${OUT}/2-friends.png`);
    await clickButton(ada, '×');
    // His handle under his wolf, for Ada only.
    check(await until(ada, `[...${S}.entities.values()].some(e => e.handle === 'Bobbin')`), 'his handle comes with his wolf for Ada');
    check(await until(ada, `document.body.innerText.includes('friend · Bobbin')`), 'shown in her In Sight');
    await sleep(1000);
    check(!(await cy.evaluate(`document.body.innerText.includes('Bobbin')`)), 'Cy, a stranger, sees no handle');
    await ada.screenshot(`${OUT}/3-in-sight.png`);
    // Bo stops sharing: it goes.
    await bo.evaluate(`${S}.sendFriends('share', {handle: 'Adder', on: false})`);
    check(await until(ada, `![...${S}.entities.values()].some(e => e.handle) && !${S}.friends[0].character`), 'not shared: no handle, no wolf named');
    await bo.evaluate(`${S}.sendFriends('share', {handle: 'Adder', on: true})`);
    await clickButton(bo, '×');
    // A private message from MESSAGE.
    await clickButton(ada, 'FRIENDS');
    await until(ada, `document.querySelector('[data-friend="Bobbin"]')`);
    await ada.evaluate(`[...document.querySelector('[data-friend="Bobbin"]').querySelectorAll('button')].find(b => b.textContent === 'MESSAGE').click()`);
    check(await until(ada, `${S}.channel === 'private' && ${S}.privateTo === 'Bobbin'`), 'MESSAGE opens PRIVATE, to Bobbin');
    await ada.key('Enter');
    await ada.type('Are you coming to the moot tonight?');
    await ada.key('Enter');
    check(await until(bo, `${S}.posts.some(p => p.channel === 'private' && p.text.includes('moot tonight') && p.with === 'Adder')`), 'Bo receives it');
    check(await until(bo, `${S}.unreadPrivate === 1`), 'counted unread on his PRIVATE tab');
    check(!(await cy.evaluate(`${S}.posts.some(p => p.text.includes('moot'))`)), 'and Cy never sees it');
    await ada.screenshot(`${OUT}/4-private.png`);
    // He goes; she writes; he comes back to it.
    await bo.evaluate(`${S}.send({type: 'character_leave'})`);
    await sleep(800);
    check(await until(ada, `${S}.friends.some(f => f.handle === 'Bobbin' && !f.online)`), 'he is away');
    await sleep(600);
    await ada.key('Enter');
    await ada.type('See you at dawn by the ford.');
    await ada.key('Enter');
    check(await until(ada, `${S}.posts.some(p => p.text.includes('reach them when they are next here'))`), 'kept for him, and she is told');
    await browsers[2].close();
    browsers.splice(2, 1);
    bo = await open('bo');
    check(await until(bo, `${S}.posts.some(p => p.channel === 'private' && p.kept && p.text.includes('dawn by the ford'))`), 'it reaches him when he comes back');
    check(await until(bo, `${S}.posts.some(p => p.text.includes('came while you were away'))`), 'and he is told');
    await bo.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'private', target: ''})`);
    await sleep(500);
    await bo.screenshot(`${OUT}/5-kept-while-away.png`);
    // A circle (doc 50, Phase 5): Ada makes one from CIRCLES, invites Bobbin, plans a night and writes in its tab.
    await clickButton(ada, 'FRIENDS');
    await until(ada, `document.querySelector('[data-tab="circles"]')`);
    await ada.evaluate(`document.querySelector('[data-tab="circles"]').click()`);
    await until(ada, `document.querySelector('[data-field="circle-name"]')`);
    await ada.evaluate(`document.querySelector('[data-field="circle-name"]').value = 'Moot Night'`);
    await clickButton(ada, 'MAKE A CIRCLE');
    check(await until(ada, `${S}.circles.some(c => c.name === 'Moot Night' && c.role === 'keeper')`), 'Ada makes the circle Moot Night');
    await until(ada, `document.querySelector('[data-field="circle-invite"]')`);
    await ada.evaluate(`document.querySelector('[data-field="circle-invite"]').value = 'Bobbin'`);
    await clickButton(ada, 'INVITE');
    check(await until(bo, `${S}.circleInvites.some(i => i.name === 'Moot Night' && i.from === 'Adder')`), 'Bo is invited');
    check(await until(bo, `[...document.querySelectorAll('button')].some(b => b.textContent === 'FRIENDS · 1')`), 'counted on his menu');
    await clickButton(bo, 'FRIENDS · 1');
    await until(bo, `document.querySelector('[data-tab="circles"]')`);
    await bo.evaluate(`document.querySelector('[data-tab="circles"]').click()`);
    await until(bo, `[...document.querySelectorAll('button')].some(b => b.textContent === 'JOIN')`);
    await clickButton(bo, 'JOIN');
    check(await until(ada, `${S}.circles[0]?.members?.some(m => m.handle === 'Bobbin' && m.online)`), 'he joins; her roster shows him here');
    const soon = new Date(Date.now() + 2 * 3600 * 1000);
    const local = new Date(soon.getTime() - soon.getTimezoneOffset() * 60000).toISOString().slice(0, 16);
    await until(ada, `document.querySelector('[data-field="night-when"]')`);
    await ada.evaluate(`(() => { document.querySelector('[data-field="night-when"]').value = ${JSON.stringify(local)};
        document.querySelector('[data-field="night-where"]').value = 'the Wharf tavern'; })()`);
    await clickButton(ada, 'PLAN A NIGHT');
    check(await until(bo, `${S}.circles[0]?.nights?.some(n => n.place === 'the Wharf tavern' && Math.abs(n.at - ${Math.floor(soon.getTime() / 60000) * 60}) < 61)`),
        'a night planned, in Ada\'s own time, reaches Bo');
    await bo.screenshot(`${OUT}/6-a-circle.png`);
    await clickButton(ada, 'CHAT');
    check(await until(ada, `${S}.channel.startsWith('circle:')`), 'CHAT opens its tab');
    await ada.key('Enter');
    await ada.type('Who is bringing the cider?');
    await ada.key('Enter');
    check(await until(bo, `${S}.posts.some(p => p.channel.startsWith('circle:') && p.text.includes('bringing the cider') && p.speaker === 'Adder')`),
        'Bo reads her line, by handle');
    check(await until(bo, `Object.values(${S}.unreadCircles).some(n => n === 1) && [...document.querySelectorAll('.circle-tabs .tab')].some(t => t.textContent === 'MOOT NIGHT · 1')`),
        'counted on his Moot Night tab');
    check(!(await cy.evaluate(`${S}.posts.some(p => p.text.includes('cider'))`)), 'Cy, not in it, never does');
    await ada.screenshot(`${OUT}/7-circle-chat.png`);
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
