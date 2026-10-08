// Tavern games in the real page (Docs/Design/54-gathering-places.md, Phase 5), on the three-town strip with a table in
// Upper Accord's inn's common room: in the evening Ash sits at the table, sets out Knucklebones, asks the room (a resident
// who lives there takes a seat), begins, and plays it out from the panel against the resident. Screenshots go to
// artifacts/screenshots/tables/.
//
//   node tools/client/tables.mjs [OUT]        (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/tables`;
mkdirSync(OUT, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
const tmp = mkdtempSync(join(tmpdir(), 'ratw-tables-'));

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
            y === 11 && i === 1 ? '............T...' : '.'.repeat(side)).join('\n') + '\n');   // (A table in the inn's common room.)
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
    m += resident('ui', 'Wren Tallow', 'merchant', 'keeping the inn', 0, 12.5, 1, 12.5);   // (The inn: its common room in c_1_0.)
    // A regular who lives by the inn: free in the evening.
    m += `resident "u6" "Pell Barrow" "civilian" "working" "Someone." "Hello." 30 "timber" "male" "average" "saddle" 3 1 5 1 1 8 17 "-" 40 0 1 ` +
        `"${id(1)}" 13.5 12.5 "${id(1)}" 14.5 4.5 "${id(1)}" 13.5 12.5\n`;
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
    const ash = await open('ash');
    await ash.evaluate(`${S}.send({type: 'time', value: 'dusk'})`);
    // Into the inn's common room (c_1_0), to the table.
    let there = false;
    for (let i = 0; i < 30 && !there; ++i) {
        const cell = await ash.evaluate(`${S}.snapshot?.cell?.id ?? ''`);
        if (cell === 'c_1_0') {
            await ash.evaluate(`${S}.send({type: 'path', x: 12.5, y: 12.5})`);
            there = !!await ash.waitFor(`!!${S}.snapshot?.self?.table?.offer`, 8).catch(() => false);
        } else await ash.evaluate(`${S}.send({type: 'path', x: 15.5, y: 12.5})`);
        if (!there) await sleep(1500);
    }
    check(there, 'Ash sits at the table in the common room: A TABLE');
    await ash.screenshot(`${OUT}/1-a-table.png`);
    const click = async (page, label) => page.evaluate(`[...document.querySelectorAll('.table button')].find(b => b.textContent.startsWith(${JSON.stringify(label)}))?.click()`);
    await click(ash, 'KNUCKLEBONES');
    check(!!await ash.waitFor(`${S}.snapshot?.self?.table?.game === 'knucklebones'`, 10), 'Knucklebones set out');
    let seated = false;
    for (let i = 0; i < 20 && !seated; ++i) {
        await ash.waitFor(`[...document.querySelectorAll('.table button')].some(b => b.textContent === 'ASK THE ROOM')`, 10);
        await click(ash, 'ASK THE ROOM');
        seated = !!await ash.waitFor(`(${S}.snapshot?.self?.table?.seats ?? []).length === 2`, 4).catch(() => false);
        if (!seated) await sleep(3000);             // (The regular may be on its way home.)
    }
    check(seated, 'a resident takes a seat');
    await ash.waitFor(`[...document.querySelectorAll('.table button')].some(b => b.textContent === 'BEGIN')`, 10);
    await click(ash, 'BEGIN');
    check(!!await ash.waitFor(`${S}.snapshot?.self?.table?.begun === true`, 10), 'it begins');
    await ash.screenshot(`${OUT}/2-playing.png`);
    // Play: try twice a turn, then bank; the resident plays its own turns.
    let over = '';
    for (let i = 0; i < 200 && !over; ++i) {
        over = await ash.evaluate(`(document.body.innerText.match(/Knucklebones is over[^\\n]*/) || [''])[0]`);
        if (over) break;
        const t = JSON.parse(await ash.evaluate(`JSON.stringify(${S}.snapshot?.self?.table ?? null)`));
        if (t?.begun && t.turn === t.seat) {
            const gained = t.state.at[t.seat] - t.state.banked[t.seat];
            await click(ash, gained >= 2 ? 'BANK' : 'TRY');
        }
        await sleep(700);
    }
    check(/win/.test(over), `played to the end: ${over}`);
    await ash.screenshot(`${OUT}/3-the-end.png`);
    check(!ash.console.some(l => l.startsWith('EXCEPTION')), `page errors: ${ash.console.filter(l => l.startsWith('EXCEPTION')).join(' / ') || 'none'}`);
} finally {
    for (const r of results) console.log(r);
    for (const b of browsers) await b.close().catch(() => {});
    server.kill();
    await sleep(300);
    rmSync(tmp, {recursive: true, force: true});
}
