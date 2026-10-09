// Town projects in the real page (Docs/Design/57-changing-the-world.md, Phase 3), on the three-town strip: a market cover
// is posted at Upper Accord (the dev console, as a Dungeon Master would); Ash and Bo go to its site, where its panel shows
// what it needs; Ash gives coin under her own name, they work on it together in two roles, and it is finished; it stands
// on the map in the town's stone grey with Ash on its plaque. Screenshots go to artifacts/screenshots/projects/.
//
//   node tools/client/projects.mjs [OUT]    (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/projects`;
mkdirSync(OUT, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
const tmp = mkdtempSync(join(tmpdir(), 'ratw-projects-'));

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
try {
    const open = async who => {
        const browser = await Browser.launch({width: 1440, height: 940});
        browsers.push(browser);
        const page = await browser.open(`http://127.0.0.1:${port}/?identity=${who}`);
        await page.waitFor(`${S}.snapshot?.self?.id`, 20);
        return page;
    };
    // Each walks east into the market cell (c_1_0), then to a tile.
    const walkTo = async (page, x, y) => {
        for (let i = 0; i < 40; ++i) {
            const cell = await page.evaluate(`${S}.snapshot?.cell?.id ?? ''`);
            if (cell === 'c_1_0') {
                await page.evaluate(`${S}.send({type: 'path', x: ${x}, y: ${y}})`);
                for (let k = 0; k < 20; ++k) {
                    await sleep(500);
                    const at = await page.evaluate(`[${S}.snapshot?.self?.x ?? 0, ${S}.snapshot?.self?.y ?? 0]`);
                    if (Math.hypot(at[0] - x, at[1] - y) < 1.2) return true;
                }
                return false;
            }
            await page.evaluate(`${S}.send({type: 'path', x: 15.5, y: 8.5})`);
            await sleep(1500);
        }
        return false;
    };
    const ash = await open('ash'), bo = await open('bo');
    await ash.evaluate(`${S}.send({type: 'time', value: 'day'})`);
    check(await walkTo(ash, 9.5, 8.5), 'Ash at Upper Accord\'s market');
    await ash.evaluate(`${S}.send({type: 'project', verb: 'post', kind: 'cover'})`);
    const site = await ash.waitFor(`(${S}.snapshot?.self?.townProjects ?? [])[0] ?? null`, 15);
    check(!!site && site.state === 'open', `a market cover posted: ${JSON.stringify(site)?.slice(0, 160)}`);
    check(await walkTo(ash, site.x + .5, site.y + 1.5), 'Ash at its site');
    check(await walkTo(bo, site.x + 1.5, site.y + .5), 'Bo at its site');
    check(!!await ash.waitFor(`document.body.innerText.includes('THE UPPER ACCORD MARKET COVER') && document.body.innerText.includes('work-hours')`, 10),
        'the site panel shows what it needs');
    await ash.screenshot(`${OUT}/1-the-site.png`);
    // Ash gives 10p under her own name (the first of her names offered).
    const names = await ash.evaluate(`${S}.snapshot.self.project.names ?? []`);
    check(names.length > 0, `her names offered for the plaque: ${names.join(', ')}`);
    await ash.evaluate(`(() => { const s = document.querySelector('select[data-field="project-shown"]'); s.value = ${JSON.stringify(names[0])}; s.dispatchEvent(new Event('change'));
        const c = document.querySelector('input[data-field="project-coins"]'); c.value = '10';
        [...document.querySelectorAll('button')].find(b => b.textContent === 'GIVE').click(); })()`);
    check(!!await ash.waitFor(`(${S}.snapshot?.self?.project?.plaque ?? []).includes(${JSON.stringify(names[0])})`, 10), 'Ash on its givers, by her own name');
    check(await ash.evaluate(`${S}.snapshot.self.project.coin`) > 0, 'the coin in its purse (what it has not yet spent on contracts and hands)');
    // Both to work, in two roles.
    await ash.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'WORK ON IT').click()`);
    await bo.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'WORK ON IT').click()`);
    const roles = [await ash.waitFor(`${S}.snapshot?.self?.project?.working ?? ''`, 10), await bo.waitFor(`${S}.snapshot?.self?.project?.working ?? ''`, 10)];
    check(roles[0] && roles[1] && roles[0] !== roles[1], `two roles: ${roles.join(' and ')}`);
    await sleep(20000);
    const worked = await ash.evaluate(`${S}.snapshot.self.project.worked`);
    check(worked >= .4, `work done together: ${worked} work-hours in 20 s`);
    await bo.screenshot(`${OUT}/2-working-together.png`);
    // Finished (the dev console stands in for the rest of the materials and hours); it stands in the town's grey.
    await ash.evaluate(`${S}.send({type: 'project', verb: 'complete', project: ${JSON.stringify(site.id)}})`);
    const standing = await ash.waitFor(`(${S}.snapshot?.structures ?? []).find(s => s.kind === 'market_cover' && s.built && s.town) ?? null`, 15);
    check(!!standing && standing.colour, `it stands on the map in the town's colour: ${JSON.stringify(standing)}`);
    check(!!await ash.waitFor(`document.body.innerText.includes('Its plaque: ${names[0]}')`, 10), 'its plaque names Ash');
    check(!!await ash.waitFor(`/stands finished/.test(document.body.innerText)`, 5), 'the town is told it stands');
    await walkTo(bo, site.x - 2.5, site.y + 2.5);   // (Out of the way, so the cover shows on the map.)
    await walkTo(ash, site.x - 1.5, site.y + 1.5);
    await ash.screenshot(`${OUT}/3-it-stands.png`);
    check(!ash.console.some(l => l.startsWith('EXCEPTION')) && !bo.console.some(l => l.startsWith('EXCEPTION')),
        `page errors: ${[...ash.console, ...bo.console].filter(l => l.startsWith('EXCEPTION')).join(' / ') || 'none'}`);
} finally {
    for (const r of results) console.log(r);
    for (const b of browsers) await b.close().catch(() => {});
    server.kill();
    await sleep(300);
    rmSync(tmp, {recursive: true, force: true});
}
