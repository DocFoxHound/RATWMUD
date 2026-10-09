// Storylines and storytellers in the real page (Docs/Design/58-player-storytellers.md), on the three-town strip: Ash is
// given "The debt" (as the DM's storyline.give would) and finds it in her journal, its first step ticked and its next
// marked on the map; made a storyteller (the dev console), she writes a tale on the STORYTELLER page and begins it, posts
// a call; Bo asks to join from his journal and she admits him; she narrates and rolls from the box where she writes, and
// Bo reads it marked as her story's; she ends it, and Bo stars her as a Storyteller on the end card. Screenshots go to
// artifacts/screenshots/storyteller/.
//
//   node tools/client/storyteller.mjs [OUT]    (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/storyteller`;
mkdirSync(OUT, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
const tmp = mkdtempSync(join(tmpdir(), 'ratw-storyteller-'));

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
const click = (page, text) => page.evaluate(`(() => { const b = [...document.querySelectorAll('button')].find(b => b.textContent.trim() === ${JSON.stringify(text)}); if (b) b.click(); return !!b; })()`);
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
    await ash.evaluate(`window.confirm = () => true`);
    // A storyline given: in her journal, step 1 ticked, the next marked on the map.
    await ash.evaluate(`${S}.send({type: 'storyline', verb: 'give', template: 'the-debt', cast: {resident: 'u1', place: 'c_5_0'}})`);
    check(!!await ash.waitFor(`(${S}.snapshot?.self?.journal ?? []).length > 0`, 10), 'a storyline given');
    check(!!await ash.waitFor(`!!${S}.snapshot?.self?.tracked?.cell`, 10), 'its next step marked');
    await click(ash, 'JOURNAL');
    check(!!await ash.waitFor(`document.body.innerText.includes('UNDER WAY') && document.body.innerText.includes('THE DEBT') && document.body.innerText.includes('✓ Word reaches you')`, 10),
        'the journal shows it, step 1 ticked');
    await ash.screenshot(`${OUT}/1-the-journal.png`);
    await click(ash, '×');
    // A storyteller: a tale written and begun.
    await ash.evaluate(`${S}.send({type: 'storyteller', verb: 'approve'})`);
    check(!!await ash.waitFor(`[...document.querySelectorAll('button')].some(b => b.textContent.trim() === 'STORYTELLER' && b.offsetParent)`, 10), 'the STORYTELLER button');
    await click(ash, 'STORYTELLER');
    check(!!await ash.waitFor(`document.body.innerText.includes('WRITE A NEW TALE')`, 10), 'the STORYTELLER page');
    await ash.evaluate(`(() => {
        document.querySelector('details.letter-sheet').open = true;
        const set = (sel, v) => { const i = document.querySelector(sel); i.value = v; i.dispatchEvent(new Event('change')); };
        set('input[data-field="tale-title"]', 'The Drowned Bell');
        set('input[data-field="step-0-title"]', 'The pier');
        set('input[data-field="step-0-text"]', 'Meet at the pier in the fog.');
    })()`);
    await click(ash, 'SAVE THE DRAFT');
    check(!!await ash.waitFor(`document.body.innerText.includes('THE DROWNED BELL · draft')`, 10), 'the draft saved');
    await click(ash, 'BEGIN');
    check(!!await ash.waitFor(`document.body.innerText.includes('THE DROWNED BELL · running')`, 10), 'begun');
    await click(ash, 'POST A CALL');
    await ash.screenshot(`${OUT}/2-the-storyteller-page.png`);
    // Bo asks from his journal; she admits him.
    check(!!await bo.waitFor(`(${S}.snapshot?.self?.storyteller?.calls ?? []).length > 0`, 10), 'Bo sees the call');
    await click(bo, 'JOURNAL');
    check(!!await bo.waitFor(`document.body.innerText.includes('CALLS IN THIS TOWN')`, 10), 'in his journal');
    await click(bo, 'ASK TO JOIN');
    check(!!await ash.waitFor(`[...document.querySelectorAll('button')].some(b => b.textContent.trim() === 'ADMIT')`, 10), 'Ash sees him asking');
    await click(ash, 'ADMIT');
    check(!!await bo.waitFor(`(${S}.snapshot?.self?.journal ?? []).some(t => t.kind === 'tale' && !t.asked && !t.invited)`, 10), 'Bo is in the tale');
    await click(bo, '×');
    await click(ash, '×');
    // Narration and dice from the box where she writes.
    await ash.evaluate(`${S}.composer.text = '/narrate Fog lies on the water, and somewhere under it a bell is ringing.'; ${S}.submitPost()`);
    check(!!await bo.waitFor(`document.body.innerText.includes('a bell is ringing') && document.body.innerText.includes('✦ STORY')`, 10), 'Bo reads it, marked as her story\'s');
    await sleep(3500);
    await ash.evaluate(`${S}.composer.text = '/roll 2d6 for the crossing'; ${S}.submitPost()`);
    check(!!await bo.waitFor(`/2d6: \\d+ \\+ \\d+ = \\d+ for the crossing/.test(document.body.innerText)`, 10), 'the roll in the open');
    await bo.evaluate(`${S}.modal = ''`);         // (The journal's answer may land after its close.)
    await sleep(300);
    await bo.screenshot(`${OUT}/3-the-story-in-the-log.png`);
    // The end card, and Bo's star.
    await click(ash, 'STORYTELLER');
    await ash.waitFor(`document.body.innerText.includes('END · DONE')`, 10);
    await click(ash, 'END · DONE');
    check(!!await bo.waitFor(`document.body.innerText.includes('THE TALE ENDS')`, 15), 'Bo\'s end card');
    await click(bo, '★ STORYTELLER');
    check(!!await bo.waitFor(`document.body.innerText.includes('★ starred')`, 10), 'Bo stars her as a Storyteller');
    await bo.screenshot(`${OUT}/4-the-end-card.png`);
    check(!ash.console.some(l => l.startsWith('EXCEPTION')) && !bo.console.some(l => l.startsWith('EXCEPTION')),
        `page errors: ${[...ash.console, ...bo.console].filter(l => l.startsWith('EXCEPTION')).join(' / ') || 'none'}`);
} finally {
    for (const r of results) console.log(r);
    for (const b of browsers) await b.close().catch(() => {});
    server.kill();
    await sleep(300);
    rmSync(tmp, {recursive: true, force: true});
}
