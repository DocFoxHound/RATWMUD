// Letters in the real page (Docs/Design/55-letters-gifts-favours.md, Phase 1), on a strip of three towns (written here
// as a world export) with an inn by Upper Accord's spawn: Bo introduces himself to Ash; at the inn Ash opens her letter
// case and writes to Bo by name, signed; the courier takes it (a penny, about an hour). An hour later (the world runs at
// thirty times, so twenty seconds) a messenger finds Bo with it; his case shows it sealed, signed Ash, with a scent he
// doesn't know; he breaks the seal and, the signature having introduced her, it smells of Ash. Screenshots go to
// artifacts/screenshots/letters/.
//
//   node tools/client/letters.mjs [OUT]      (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/letters`;
mkdirSync(OUT, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
const tmp = mkdtempSync(join(tmpdir(), 'ratw-letters-'));

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
            `outdoors: true\nweather: clear\nsize: ${side} ${side}\ngrid:\n` + `${'.'.repeat(side)}\n`.repeat(side));
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
    m += resident('ui', 'Wren Tallow', 'merchant', 'keeping the inn', 0, 12.5, 0, 12.5);
    writeFileSync(join(dir, 'world.ratw'), m);
}
writeStrip(join(tmp, 'strip'));

const port = 18900 + Math.floor(Math.random() * 600);
const server = spawn(binary, ['--port', String(port), '--web', web, '--world-export', join(tmp, 'strip'), '--save', `${tmp}/save.json`,
    '--dev-identity', '--dev-tools', '--speed', '30'], {stdio: ['ignore', 'pipe', 'pipe'], env: {...process.env, RATW_AI: 'off'}});
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
    await ash.evaluate(`${S}.send({type: 'time', value: 'day'})`);
    await sleep(1000);
    await bo.evaluate(`${S}.send({type: 'chat', text: '"I\\'m Bo."', commandId: 'intro'})`);
    await sleep(1500);
    // Ash, at the inn, writes to Bo from her letter case.
    await ash.evaluate(`${S}.openLetters()`);
    await ash.waitFor(`document.querySelector('[data-field="letter-text"]')`, 10);
    await ash.evaluate(`(() => { const s = ${S}; s.letterDraft.to = 'Bo'; s.letterDraft.text = 'Meet me by the fountain at dusk. Bring the map.';
        s.letterDraft.sign = 'Ash'; s.letterDraftVersion++; })()`);
    await ash.waitFor(`document.querySelector('[data-field="letter-to"]')?.value === 'Bo'`, 10);
    await ash.screenshot(`${OUT}/1-writing-to-bo.png`);
    await ash.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'SEND').click()`);
    const taken = await ash.waitFor(`(document.body.innerText.match(/The courier takes your letter[^\\n]*/) || [''])[0]`, 10);
    check(/to Bo \(1p\)\. It should reach Upper Accord in about 1 hour\. You signed as Ash\./.test(taken), `the courier takes it: ${taken}`);
    // An hour on (twenty seconds here): a messenger finds Bo.
    const found = await bo.waitFor(`/A messenger finds you with a letter/.test(document.body.innerText)`, 60);
    check(found, 'a messenger finds Bo with it');
    check(await bo.waitFor(`${S}.snapshot?.self?.letters?.unread === 1`, 10), 'one unread');
    await bo.evaluate(`${S}.openLetters()`);
    const sealed = await bo.waitFor(`document.querySelector('.letter')?.innerText`, 10);
    check(/From Ash/.test(sealed) && /A wolf's scent you don't know\./.test(sealed) && /Sealed\./.test(sealed), `sealed, signed, an unknown scent: ${sealed}`);
    await bo.screenshot(`${OUT}/2-a-letter-sealed.png`);
    await bo.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'BREAK THE SEAL').click()`);
    const opened = await bo.waitFor(`(t => /Meet me by the fountain/.test(t) && t)(document.querySelector('.letter')?.innerText ?? '')`, 10);
    check(/It smells of Ash\./.test(opened), `opened: the signature introduced her, so it smells of Ash: ${opened}`);
    await bo.screenshot(`${OUT}/3-it-smells-of-ash.png`);
    // A gift face to face (Phase 2): Ash gives Bo bread from his menu's Give; he accepts; it smells of Ash.
    await ash.evaluate(`${S}.send({type: 'grant', item: 'bread', quantity: 1})`);
    await ash.waitFor(`(${S}.snapshot?.inventory ?? []).some(i => i.id === 'bread')`, 10);
    const boId = await ash.evaluate(`[...${S}.entities.values()].find(e => e.kind === 'player' && !e.self)?.id`);
    await ash.evaluate(`${S}.modal = ''`);
    await ash.evaluate(`${S}.openContextAt(${JSON.stringify(boId)}, [700, 420])`);
    await ash.waitFor(`[...document.querySelectorAll('.menu-item')].find(b => /Give/.test(b.textContent))`, 10);
    await ash.evaluate(`[...document.querySelectorAll('.menu-item')].find(b => /Give/.test(b.textContent)).click()`);
    await ash.waitFor(`document.querySelector('[data-field="give-item"]')`, 10);
    await ash.evaluate(`(() => { const sel = document.querySelector('[data-field="give-item"]'); sel.value = 'bread'; })()`);
    await ash.screenshot(`${OUT}/4-give.png`);
    await ash.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'GIVE').click()`);
    const offered = await bo.waitFor(`[...document.querySelectorAll('.fight-row')].map(r => r.textContent).find(t => /offers you/.test(t))`, 10);
    check(/offers you 1 /.test(offered ?? ''), `Bo is offered it: ${offered}`);
    await bo.screenshot(`${OUT}/5-offered.png`);
    await bo.evaluate(`[...document.querySelectorAll('.fight-row button')].find(b => b.textContent === 'Accept').click()`);
    await bo.waitFor(`(${S}.snapshot?.inventory ?? []).some(i => i.id === 'bread')`, 10);
    const smell = await bo.evaluate(`(${S}.snapshot.inventory.find(i => i.id === 'bread') || {}).scent`);
    check(smell === 'it smells of Ash', `his bread smells of Ash: ${smell}`);
    await bo.evaluate(`${S}.modal = 'inventory'`);
    await bo.waitFor(`/It smells of Ash/.test(document.body.innerText)`, 10);
    await bo.screenshot(`${OUT}/6-it-smells-of-ash.png`);
    // Grooming (Phase 3): Ash asks from Bo's menu; he accepts; fifteen seconds (half a second at thirty times); he is
    // Well-groomed, by Ash, and looks freshly groomed to her.
    await bo.evaluate(`${S}.modal = ''`);
    await ash.evaluate(`${S}.openContextAt(${JSON.stringify(boId)}, [700, 420])`);
    await ash.waitFor(`[...document.querySelectorAll('.menu-item')].find(b => /Groom/.test(b.textContent))`, 10);
    await ash.evaluate(`[...document.querySelectorAll('.menu-item')].find(b => /Groom/.test(b.textContent)).click()`);
    const asked = await bo.waitFor(`[...document.querySelectorAll('.fight-row')].map(r => r.textContent).find(t => /would groom you/.test(t))`, 10);
    check(/Ash would groom you/.test(asked ?? ''), `Bo is asked: ${asked}`);
    await bo.evaluate(`[...document.querySelectorAll('.fight-row button')].find(b => b.textContent === 'Accept').click()`);
    check(await bo.waitFor(`${S}.snapshot?.self?.groomed?.by === 'Ash'`, 20), 'Bo is Well-groomed, by Ash');
    check(await ash.waitFor(`[...${S}.entities.values()].find(e => e.id === ${JSON.stringify(boId)})?.groomed === true`, 10), 'he looks freshly groomed to her');
    const groomLine = await bo.waitFor(`document.body.innerText.split('\\n').find(l => /ruff, slow and careful/.test(l)) ?? ''`, 10).catch(() => '') ?? '';
    check(/ruff, slow and careful/.test(groomLine), `the grooming is in the story as her action: ${groomLine}`);
    await bo.evaluate(`${S}.modal = 'status'`);
    await bo.waitFor(`/Well-groomed, by Ash/.test(document.body.innerText)`, 10);
    await bo.screenshot(`${OUT}/7-well-groomed.png`);
    for (const p of [ash, bo])
        check(!p.console.some(l => l.startsWith('EXCEPTION')), `page errors: ${p.console.filter(l => l.startsWith('EXCEPTION')).join(' / ') || 'none'}`);
} finally {
    for (const r of results) console.log(r);
    for (const b of browsers) await b.close().catch(() => {});
    server.kill();
    await sleep(300);
    rmSync(tmp, {recursive: true, force: true});
}
