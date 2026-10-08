// Fame in the real page (Docs/Design/56-fame-and-memory.md, Phase 3), on the three-town strip: Ash, beside the stall's
// keeper at Upper Accord's market, does a deed through the dev console (the keeper coins its nickname), is greeted by
// the deed, finds the nickname on her character sheet with who first said it, asks folk not to use it, and reads her
// chronicle (Phase 5); she leaves and comes back after a break to the welcome card (Phase 6). Screenshots
// go to artifacts/screenshots/fame/.
//
//   node tools/client/fame.mjs [OUT]        (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/fame`;
mkdirSync(OUT, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
const tmp = mkdtempSync(join(tmpdir(), 'ratw-fame-'));

// The strip (as tools/client/newcomer.mjs writes it, with an inn by the spawn): Upper Accord (with the spawn) in the first two cells, wild
// country, Ser Ferro in cells 4-5, Ridgemere in 7-8; each town's first cell its homes, its second its market.
function writeStrip(dir) {
    const layout = 'UU..SS.RR', side = 16, cells = layout.length;
    const id = i => `c_${i}_0`;
    mkdirSync(join(dir, 'cells'), {recursive: true});
    mkdirSync(join(dir, 'seams'), {recursive: true});
    const seams = {};
    let m = 'RATW_WORLD 3\n', seam = 0;
    for (let i = 0; i < cells; ++i) {
        writeFileSync(join(dir, 'cells', `${id(i)}.cell`), `id: ${id(i)}\nname: Stretch ${i}\ndescription: Open ground.\nworld: ${i * side} 0 0\n` +
            `outdoors: true\nweather: clear\nsize: ${side} ${side}\ngrid:\n` + Array.from({length: side}, (_, y) =>
            '.'.repeat(side)).join('\n') + '\n');
        m += `area "${id(i)}"\n`;
        if (i + 1 < cells)
            for (let k = 0; k < side; ++k, ++seam) {
                const a = `seam_${seam}_a`, b = `seam_${seam}_b`;
                seams[id(i)] = (seams[id(i)] ?? '') + `door "${a}" "Open boundary" "${id(i)}" ${side - .5} ${k + .5} "${id(i + 1)}" .5 ${k + .5} "${b}" 1 0 1 1 "E"\n`;
                seams[id(i + 1)] = (seams[id(i + 1)] ?? '') + `door "${b}" "Open boundary" "${id(i + 1)}" .5 ${k + .5} "${id(i)}" ${side - .5} ${k + .5} "${a}" 1 0 1 1 "W"\n`;
            }
    }
    for (const [cell, text] of Object.entries(seams)) writeFileSync(join(dir, 'seams', cell), text);
    const region = c => ({U: 'upper_accord', S: 'ser_ferro', R: 'ridgemere'})[c] ?? 'wilds';
    for (let i = 0; i < cells; ++i) {
        const exits = [i > 0 ? id(i - 1) : null, i + 1 < cells ? id(i + 1) : null].filter(Boolean);
        m += `exits "${id(i)}" ${exits.length}${exits.map(e => ` "${e}"`).join('')}\n`;
        m += `territory "${id(i)}" "${region(layout[i])}" "-" 0\n`;
    }
    m += `spawn "${id(0)}" 8.5 8.5\neconomy 20000 100 50 10 12\n`;
    const resident = (who, name, role, work, home, hx, at, wx) =>
        `resident "${who}" "${name}" "${role}" "${work}" "Someone." "Hello." 30 "timber" "female" "average" "saddle" 3 1 5 1 1 8 17 "-" 40 0 1 ` +
        `"${id(home)}" ${hx} 4.5 "${id(at)}" ${wx} 8.5 "${id(home)}" ${hx} 5.5\n`;
    for (const [c, tag] of [['U', 'u'], ['S', 's'], ['R', 'r']]) {
        const home = layout.indexOf(c);
        m += resident(`${tag}m`, `Merchant ${tag}`, 'merchant', 'keeping the stall', home, 2.5, home + 1, 8.5);
        for (let n = 1; n <= 5; ++n) m += resident(`${tag}${n}`, `Neighbour ${tag}${n}`, 'civilian', 'working', home, 3.5 + n, home, 3.5 + n);
    }
    // Upper Accord's inn, by the spawn: where letters are written.
    m += resident('ui', 'Wren Tallow', 'merchant', 'keeping the inn', 0, 12.5, 1, 12.5);   // (In the market cell, so the market is there.)
    writeFileSync(join(dir, 'world.ratw'), m);
}
writeStrip(join(tmp, 'strip'));

const port = 18900 + Math.floor(Math.random() * 600);
const server = spawn(binary, ['--port', String(port), '--web', web, '--world-export', join(tmp, 'strip'), '--save', `${tmp}/save.json`,
    '--dev-identity', '--dev-tools', '--voice-data', `${root}/Data/Voice`, '--away-break', '2'], {stdio: ['ignore', 'pipe', 'pipe'], env: {...process.env, RATW_AI: 'off'}});
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
const story = page => page.evaluate(`document.body.innerText`);
try {
    const open = async who => {
        const browser = await Browser.launch({width: 1440, height: 940});
        browsers.push(browser);
        const page = await browser.open(`http://127.0.0.1:${port}/?identity=${who}`);
        await page.waitFor(`${S}.snapshot?.self?.id`, 20);
        return page;
    };
    const ash = await open('ash');
    await ash.evaluate(`${S}.send({type: 'time', value: 'day'})`);
    // To the market cell (c_1_0), beside the stall's keeper (who works at 8.5, 8.5).
    let there = false;
    for (let i = 0; i < 30 && !there; ++i) {
        const cell = await ash.evaluate(`${S}.snapshot?.cell?.id ?? ''`);
        if (cell === 'c_1_0') {
            await ash.evaluate(`${S}.send({type: 'path', x: 9.5, y: 8.5})`);
            await sleep(3000);
            there = true;
        } else await ash.evaluate(`${S}.send({type: 'path', x: 15.5, y: 8.5})`);
        if (!there) await sleep(1500);
    }
    check(there, 'Ash beside the keeper at the market');
    await ash.evaluate(`${S}.send({type: 'deed', kind: 'broke_camp', coin: true})`);
    check(!!await ash.waitFor(`(${S}.snapshot?.self?.nicknames ?? []).length > 0`, 15), 'a deed, and a nickname coined');
    const nick = await ash.evaluate(`${S}.snapshot.self.nicknames[0].text`);
    // The keeper greets her by the deed.
    const keeper = await ash.evaluate(`[...${S}.entities.values()].find(e => /stall/i.test(e.name ?? '') || /stall/i.test(e.activity ?? ''))?.id ?? 'um'`);
    await ash.evaluate(`${S}.send({type: 'chat', text: 'Hello.', channel: 'ic', volume: 'speak', targets: [${JSON.stringify(keeper)}]})`);
    const greeting = await ash.waitFor(`(document.body.innerText.match(/[^\\n]*drove the bandits off[^\\n]*/) || [''])[0]`, 20).catch(() => '');
    check(!!greeting, `the keeper greets her by the deed: ${greeting}`);
    await ash.screenshot(`${OUT}/1-greeted-by-the-deed.png`);
    // The sheet: what folk call her, and who first said it.
    await ash.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'CHARACTER').click()`);
    const row = await ash.waitFor(`(document.body.innerText.match(/WHAT FOLK CALL YOU\\s+[^\\n]*/) || [''])[0]`, 10);
    check(row.includes(nick) && /first said by/.test(row), `the sheet's row: ${row}`);
    await ash.screenshot(`${OUT}/2-what-folk-call-you.png`);
    await ash.evaluate(`window.confirm = () => true; [...document.querySelectorAll('button')].find(b => b.textContent === 'ASK FOLK NOT TO USE IT').click()`);
    check(!!await ash.waitFor(`${S}.snapshot?.self?.nicknames?.[0]?.dropped === true`, 10), 'asked folk not to use it');
    check(!!await ash.waitFor(`/you asked folk not to use it/.test(document.body.innerText)`, 10), 'the sheet says so');
    await ash.screenshot(`${OUT}/3-dropped.png`);
    // The chronicle (Phase 5): her life so far, from what the world remembers.
    await ash.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'CHRONICLE').click()`);
    const page = await ash.waitFor(`document.body.innerText.includes('THE CHRONICLE') && (document.body.innerText.match(/[^\\n]*You drove the bandits off[^\\n]*/) || [''])[0]`, 15);
    check(/Year 1/.test(page || ''), `the chronicle page: ${page}`);
    await ash.screenshot(`${OUT}/4-the-chronicle.png`);
    // Welcome back (Phase 6): she leaves (the "Before you go" page), comes back after a break (two seconds here), and
    // is shown what happened while she was away; it stays on her sheet.
    await ash.evaluate(`${S}.activate({rect: {x: 0, y: 0, w: 0, h: 0}, action: 'leave_character', target: ''})`);
    check(!!await ash.waitFor(`document.body.innerText.includes('Leave this character?')`, 10), 'the leaving page');
    await ash.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'RETURN TO SELECTION').click()`);
    await sleep(3500);
    await ash.close?.();
    browsers.pop()?.close().catch(() => {});
    const back = await open('ash');
    check(!!await back.waitFor(`document.body.innerText.includes('WELCOME BACK')`, 20), 'the welcome card on coming back');
    await back.screenshot(`${OUT}/5-welcome-back.png`);
    await back.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === '×')?.click()`);
    await back.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'CHARACTER').click()`);
    check(!!await back.waitFor(`document.body.innerText.includes('WHILE YOU WERE AWAY')`, 10), 'kept on the sheet');
    check(!ash.console.some(l => l.startsWith('EXCEPTION')), `page errors: ${ash.console.filter(l => l.startsWith('EXCEPTION')).join(' / ') || 'none'}`);
} finally {
    for (const r of results) console.log(r);
    for (const b of browsers) await b.close().catch(() => {});
    server.kill();
    await sleep(300);
    rmSync(tmp, {recursive: true, force: true});
}
