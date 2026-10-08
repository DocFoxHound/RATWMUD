// The archive in the real page (Docs/Design/54-gathering-places.md, Phase 7), on the three-town strip with a records
// clerk at Upper Accord's market: at noon Ash goes to the clerk, asks for archive work, sorts the six records in the
// sheet (▲ to move one up), hands them in, is paid, reads the fragment, and opens the journal. Screenshots go to
// artifacts/screenshots/archive/.
//
//   node tools/client/archive.mjs [OUT]        (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/archive`;
mkdirSync(OUT, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
const tmp = mkdtempSync(join(tmpdir(), 'ratw-archive-'));

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
    m += resident('ua', 'Clerk Ivo', 'civilian', 'copying the city rolls', 0, 6.5, 1, 4.5);   // (A keeper of records: doc 54, 7.)
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
    await ash.evaluate(`${S}.send({type: 'time', value: 'day'})`);
    let there = false;
    for (let i = 0; i < 30 && !there; ++i) {
        const cell = await ash.evaluate(`${S}.snapshot?.cell?.id ?? ''`);
        if (cell === 'c_1_0') {
            await ash.evaluate(`${S}.send({type: 'path', x: 5.5, y: 8.5})`);
            there = !!await ash.waitFor(`!!${S}.snapshot?.self?.archive`, 8).catch(() => false);
        } else await ash.evaluate(`${S}.send({type: 'path', x: 15.5, y: 8.5})`);
        if (!there) await sleep(1500);
    }
    check(there, 'Ash reaches the clerk: ARCHIVE WORK');
    await ash.evaluate(`[...document.querySelectorAll('.actions button')].find(b => b.textContent === 'Archive work').click()`);
    check(!!await ash.waitFor(`document.body.innerText.includes('HAND THEM IN')`, 10), 'six records to sort, in the sheet');
    await ash.screenshot(`${OUT}/1-records-to-sort.png`);
    // Sort them by their clues, moving one up at a time (as a player would).
    const sorted = await ash.evaluate(`(() => {
        const times = ['early spring', 'midspring', 'late spring', 'early summer', 'midsummer', 'late summer', 'early autumn', 'midautumn',
            'late autumn', 'early winter', 'midwinter', 'late winter'];
        const roman = r => { const v = {I: 1, V: 5, X: 10, L: 50}; let t = 0; for (let i = 0; i < r.length; ++i) t += v[r[i]] < (v[r[i + 1]] ?? 0) ? -v[r[i]] : v[r[i]]; return t; };
        const key = text => { let m = text.match(/the (\\d+)(?:st|nd|rd|th) winter/); if (m) return Number(m[1]); m = text.match(/^Roll ([IVXL]+):/); if (m) return roman(m[1]);
            for (let k = 11; k >= 0; --k) if (text.includes('in ' + times[k] + '.')) return k; return 0; };
        const rows = () => [...document.querySelectorAll('.story-row')].filter(r => r.querySelector('button')?.textContent === '▲');
        for (let pass = 0; pass < 12; ++pass) {
            let moved = false;
            const list = rows();
            for (let i = 1; i < list.length; ++i) {
                const a = key(list[i - 1].children[1].textContent), b = key(list[i].children[1].textContent);
                if (b < a) { list[i].querySelector('button').click(); moved = true; break; }
            }
            if (!moved) return rows().map(r => key(r.children[1].textContent));
        }
        return rows().map(r => key(r.children[1].textContent));
    })()`);
    check(sorted.length === 6 && sorted.every((k, i) => i === 0 || sorted[i - 1] <= k), `sorted in the sheet: ${sorted}`);
    await ash.screenshot(`${OUT}/2-sorted.png`);
    await ash.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'HAND THEM IN').click()`);
    const paid = await ash.waitFor(`(document.body.innerText.match(/The records are in order[^\\n]*/) || [''])[0]`, 10);
    check(/pays you 2p/.test(paid), `handed in and paid: ${paid}`);
    check(!!await ash.waitFor(`/Among the records, something on/.test(document.body.innerText)`, 10), 'a fragment of the records');
    await ash.evaluate(`${S}.send({type: 'journal'})`);
    check(!!await ash.waitFor(`document.body.innerText.includes('THE JOURNAL') && document.body.innerText.includes('THE FIRST OATH')`, 10), 'the journal shows the lore read');
    await ash.screenshot(`${OUT}/3-the-journal.png`);
    check(!ash.console.some(l => l.startsWith('EXCEPTION')), `page errors: ${ash.console.filter(l => l.startsWith('EXCEPTION')).join(' / ') || 'none'}`);
} finally {
    for (const r of results) console.log(r);
    for (const b of browsers) await b.close().catch(() => {});
    server.kill();
    await sleep(300);
    rmSync(tmp, {recursive: true, force: true});
}
