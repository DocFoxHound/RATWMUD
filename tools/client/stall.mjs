// Market stalls in the real page (Docs/Design/54-gathering-places.md, Phase 4), on the three-town strip with stalls built
// on each market: on Marketday at noon Ash walks to a stall spot on Upper Accord's square, rents it (3p), and lays out a
// meal at 4p from the panel; Bo comes up, sees it with the keeper there, and buys it. Screenshots go to
// artifacts/screenshots/stall/.
//
//   node tools/client/stall.mjs [OUT]        (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/stall`;
mkdirSync(OUT, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
const tmp = mkdtempSync(join(tmpdir(), 'ratw-stall-'));

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
            y === 5 && [1, 5, 8].includes(i) ? '.....u.u.u......' : '.'.repeat(side)).join('\n') + '\n');   // (Built stalls on each market.)
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
const story = page => page.evaluate(`document.body.innerText`);
try {
    const open = async who => {
        const browser = await Browser.launch({width: 1440, height: 940});
        browsers.push(browser);
        const page = await browser.open(`http://127.0.0.1:${port}/?identity=${who}`);
        await page.waitFor(`${S}.snapshot?.self?.id`, 20);
        return page;
    };
    const ash = await open('ash'), bo = await open('bo');
    // To Marketday, at noon (the dev tools' calendar and time).
    for (let i = 0; i < 8; ++i) {
        const weekday = await ash.evaluate(`${S}.snapshot?.cell?.environment?.calendar?.weekday ?? ''`);
        if (/market/i.test(weekday)) break;
        await ash.evaluate(`${S}.send({type: 'calendar', value: 'day'})`);
        await sleep(600);
    }
    await ash.evaluate(`${S}.send({type: 'time', value: 'day'})`);
    await sleep(600);
    check(/market/i.test(await ash.evaluate(`${S}.snapshot?.cell?.environment?.calendar?.weekday ?? ''`)), 'Marketday');
    // Into Upper Accord's market cell (c_1_0), then to a spot.
    const goTo = async (page, x, y, until) => {
        for (let i = 0; i < 30; ++i) {
            const cell = await page.evaluate(`${S}.snapshot?.cell?.id ?? ''`);
            if (process.env.DEBUG) console.log(i, cell, await page.evaluate(`JSON.stringify([${S}.snapshot?.self?.x, ${S}.snapshot?.self?.y, ${S}.snapshot?.self?.stall ?? null, ${S}.snapshot?.cell?.environment?.calendar?.weekday, ${S}.snapshot?.cell?.environment?.hour, ${S}.snapshot?.cell?.environment?.weather])`));
            if (cell === 'c_1_0') {
                await page.evaluate(`${S}.send({type: 'weather', value: 'clear'})`);
                await page.evaluate(`${S}.send({type: 'path', x: ${x}, y: ${y}})`);
                if (await page.waitFor(until, 6).catch(() => false)) return true;
            } else await page.evaluate(`${S}.send({type: 'path', x: 15.5, y: 6.5})`);
            await sleep(1500);
        }
        return false;
    };
    check(await goTo(ash, 5.5, 6.5, `!!${S}.snapshot?.self?.stall?.offer`), 'Ash stands at a free stall spot: RENT THIS STALL');
    await ash.evaluate(`${S}.send({type: 'weather', value: 'clear'})`);
    await ash.screenshot(`${OUT}/1-a-stall-to-rent.png`);
    await ash.evaluate(`[...document.querySelectorAll('.stall button')].find(b => /RENT THIS STALL/.test(b.textContent)).click()`);
    check(!!await ash.waitFor(`${S}.snapshot?.self?.stall?.mine?.id`, 10), 'Ash rents it');
    await ash.waitFor(`document.querySelector('.stall select')`, 10);
    await ash.evaluate(`(() => { const p = document.querySelector('.stall'); const s = p.querySelector('select');
        s.value = 'meal'; const [q, price] = p.querySelectorAll('input'); q.value = '1'; price.value = '4';
        [...p.querySelectorAll('button')].find(b => b.textContent === 'LAY OUT').click(); })()`);
    check(!!await ash.waitFor(`(${S}.snapshot?.self?.stall?.mine?.wares ?? []).length === 1`, 10), 'a meal laid out at 4p');
    await ash.screenshot(`${OUT}/2-her-stall.png`);
    check(await goTo(bo, 6.5, 6.5, `!!${S}.snapshot?.self?.stall?.here`), 'Bo comes up to her stall');
    const seen = await bo.waitFor(`(document.querySelector('.stall')?.innerText.match(/BUY · 4p/) || [''])[0]`, 10);
    check(!!seen, 'Bo sees the meal at 4p, with her there');
    await bo.screenshot(`${OUT}/3-at-her-stall.png`);
    await bo.evaluate(`[...document.querySelectorAll('.stall button')].find(b => /BUY/.test(b.textContent)).click()`);
    const bought = await bo.waitFor(`(document.body.innerText.match(/You buy 1 [^\\n]*/) || [''])[0]`, 10);
    check(/for 4p/.test(bought), `Bo buys it: ${bought}`);
    const sold = await ash.waitFor(`(document.body.innerText.match(/buys 1 [^\\n]*from your stall[^\\n]*/) || [''])[0]`, 10);
    check(/4p/.test(sold), `Ash is told: ${sold}`);
    for (const p of [ash, bo])
        check(!p.console.some(l => l.startsWith('EXCEPTION')), `page errors: ${p.console.filter(l => l.startsWith('EXCEPTION')).join(' / ') || 'none'}`);
} finally {
    for (const r of results) console.log(r);
    for (const b of browsers) await b.close().catch(() => {});
    server.kill();
    await sleep(300);
    rmSync(tmp, {recursive: true, force: true});
}
