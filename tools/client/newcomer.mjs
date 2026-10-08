// Newcomers in the real page (Docs/Design/52-newcomers.md, Phases 1 and 3): on a strip of three towns named as the
// start towns are (written here as a world export), a new account's creator shows the Arrival tab with Upper Accord
// preselected and why; the first wolf arrives at Upper Accord's spawn. A second new account chooses Ser Ferro and
// arrives at its market. A third arrives beside the first, who sees the newcomer's mark in In Sight and on the card.
// Then ties: Bo, a mentor, is offered Ash's tie under his map and accepts; Ash, entering, is told where to find him,
// with the spot marked on the map. Then matchmaking, with the Mind off: Ash, the tie ended, asks the innkeeper
// something and is pointed at Bo by his look, in a written line; Bo sees the innkeeper nod his way. Then a first evening
// at the inn: Cara arrives, dusk falls, and the innkeeper welcomes her aloud and points out the wolves it knows; she and
// they are prompted to introduce themselves, and Bo does. Screenshots go to artifacts/screenshots/newcomers/.
//
//   node tools/client/newcomer.mjs [OUT]     (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/newcomers`;
mkdirSync(OUT, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
const tmp = mkdtempSync(join(tmpdir(), 'ratw-newcomer-'));

// The strip (as Tests/newcomer_tests.cpp writes it): Upper Accord (with the spawn) in the first two cells, wild
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
    // Upper Accord's innkeeper, working by the spawn (Phase 4: a matchmaker).
    m += resident('ui', 'Wren Tallow', 'civilian', 'serves at the inn', 0, 12.5, 0, 12.5);
    writeFileSync(join(dir, 'world.ratw'), m);
}
writeStrip(join(tmp, 'strip'));

const port = 18900 + Math.floor(Math.random() * 600);
const server = spawn(binary, ['--port', String(port), '--web', web, '--world-export', join(tmp, 'strip'), '--save', `${tmp}/save.json`, '--dev-tools'],
    {stdio: ['ignore', 'pipe', 'pipe'], env: {...process.env, RATW_AI: 'off'}});
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
const D = 'window.ratw.door()', S = 'window.ratw.game().state';
const appearance = {species: 'timber', sex: 'female', stature: 'average', pattern: 'solid', baseColor: 3, gradientColor: 1, markingColor: 5,
    gradientAmount: 0.5, patternAmount: 0.5};
try {
    const open = async () => {
        const browser = await Browser.launch({width: 1440, height: 940});
        browsers.push(browser);
        const page = await browser.open(`http://127.0.0.1:${port}/`);
        await page.waitFor(`${D}`, 20);
        return page;
    };
    // Registers, opens the creator on the Arrival tab, picks `start` (or leaves the preselected), creates and enters.
    const arrive = async (page, user, name, start, shot, tie = '', tieShot = '') => {
        await page.evaluate(`window.ratw.connection.submit({type: 'auth_register', username: '${user}', password: 'a long enough password'})`);
        await page.waitFor(`window.ratw.lobby()?.stage === 'characters'`, 20);
        await page.evaluate(`window.ratw.page('creator')`);
        await page.evaluate(`window.ratw.draft(${JSON.stringify(name)}, 24, ${JSON.stringify(appearance)})`);
        await page.evaluate(`[...document.querySelectorAll('.creator-tabs button')].find(b => b.textContent === 'Arrival')?.click()`);
        await page.waitFor(`document.querySelector('[data-start]')`, 5);
        if (start) await page.evaluate(`document.querySelector('[data-start="${start}"]').click()`);
        await sleep(200);
        const tab = await page.evaluate(`JSON.stringify({chosen: document.querySelector('.gift-card.chosen')?.dataset.start,
            cards: [...document.querySelectorAll('[data-start]')].map(b => b.textContent), text: document.querySelector('.creator-panel')?.textContent})`);
        if (shot) await page.screenshot(`${OUT}/${shot}`);
        // The Tie tab (Phase 3): a first wolf has one suggested; pick one when asked to.
        await page.evaluate(`[...document.querySelectorAll('.creator-tabs button')].find(b => b.textContent === 'Tie')?.click()`);
        await page.waitFor(`document.querySelector('[data-tie]')`, 5);
        if (tie) await page.evaluate(`document.querySelector('[data-tie="${tie}"]').click()`);
        await sleep(150);
        const tieTab = await page.evaluate(`JSON.stringify({chosen: document.querySelector('.tie-list .gift-card.chosen')?.dataset.tie ?? '',
            current: document.querySelector('.tie-current')?.textContent, none: !!document.querySelector('[data-tie=""]')})`);
        if (tieShot) await page.screenshot(`${OUT}/${tieShot}`);
        await page.evaluate(`window.ratw.page('review')`);
        await sleep(200);
        const review = await page.evaluate(`document.querySelector('.door-form')?.textContent ?? ''`);
        await page.evaluate(`[...document.querySelectorAll('button')].find(b => b.textContent === 'CONFIRM & CREATE')?.click()`);
        await page.waitFor(`(window.ratw.lobby()?.characters ?? []).length === 1`, 20);
        const id = await page.evaluate(`window.ratw.lobby().characters[0].id`);
        await page.evaluate(`window.ratw.connection.submit({type: 'character_enter', id: '${id}'})`);
        await page.waitFor(`window.ratw.game()?.state.snapshot?.self?.id`, 20);
        await sleep(600);
        return {id, tab: JSON.parse(tab), tie: JSON.parse(tieTab), review, cell: await page.evaluate(`${S}.cellId`)};
    };

    const nina = await open();
    const first = await arrive(nina, 'nina', 'Nina', '', '1-arrival-first-wolf.png');
    check(first.tab.chosen === 'upper_accord' && first.tab.cards.length === 3 && /busiest/.test(first.tab.cards[0]),
        `the Arrival tab: three towns, Upper Accord preselected as the busiest (${first.tab.cards.join(' | ')})`);
    check(/Your first wolf arrives in Upper Accord/.test(first.tab.text) && /choose another/.test(first.tab.text), 'a first wolf is told why, and that it may choose');
    check(/Arrives in: Upper Accord/.test(first.review), 'the review says where it arrives');
    check(!!first.tie.current && first.tie.current !== 'No tie' && !first.tie.none && /Tie: /.test(first.review),
        `a first wolf has a tie suggested, and no "No tie" (${first.tie.current})`);
    check(first.cell === 'c_0_0', `Nina arrives at Upper Accord's spawn (${first.cell})`);

    const omar = await open();
    const second = await arrive(omar, 'omar', 'Omar', 'ser_ferro', '2-arrival-chose-ser-ferro.png');
    check(second.tab.chosen === 'ser_ferro' && /Arrives in: Ser Ferro/.test(second.review), 'a first wolf chooses Ser Ferro instead');
    check(second.cell === 'c_5_0', `Omar arrives at Ser Ferro's market (${second.cell})`);
    await omar.screenshot(`${OUT}/3-in-ser-ferro.png`);

    const pia = await open();
    const third = await arrive(pia, 'pia', 'Pia', '', '');
    check(third.cell === 'c_0_0', 'Pia arrives in Upper Accord too');
    await nina.waitFor(`${S}.entities.get('${third.id}')?.nc`, 10);
    const row = await nina.waitFor(`[...document.querySelectorAll('*')].map(n => n.childElementCount === 0 ? n.textContent : '').find(t => /✧/.test(t ?? ''))`, 10);
    check(/✧/.test(row), `Nina sees the newcomer's mark in In Sight: ${row}`);
    await nina.evaluate(`${S}.send({type: 'action', action: 'inspect', target: '${third.id}'})`);
    const card = await nina.waitFor(`[...document.querySelectorAll('.sage.small')].map(n => n.textContent).find(t => /new to these parts/.test(t))`, 10);
    check(!!card, `the card says so: ${card}`);
    await nina.screenshot(`${OUT}/4-a-newcomer-on-the-card.png`);

    // Ties: Bo, a mentor where Ash arrives, is offered Ash's tie and takes it; Ash is told where to find him.
    const bo = await open();
    const boArrival = await arrive(bo, 'bob', 'Bo', '', '');
    await bo.evaluate(`${S}.send({type: 'tie', verb: 'end'})`);          // (His own newcomer's tie: not needed here.)
    await bo.evaluate(`${S}.send({type: 'socialLevel', level: 5})`);
    await sleep(300);
    await bo.evaluate(`${S}.send({type: 'mentor', verb: 'optin'})`);
    await bo.waitFor(`${S}.posts.some(p => /You mentor newcomers now/.test(p.text))`, 10);
    check(boArrival.cell === 'c_0_0', 'Bo stands in Upper Accord');
    const ash = await open();
    const ashArrival = arrive(ash, 'ash', 'Ash', '', '', 'river', '5-the-tie-tab.png');
    await bo.waitFor(`${S}.tieOffer`, 30);
    await bo.waitFor(`document.querySelector('.tie-panel') && getComputedStyle(document.querySelector('.tie-panel')).display !== 'none'`, 10);
    const offer = await bo.evaluate(`document.querySelector('.tie-panel').textContent`);
    check(/A tie for you: .+, new in Upper Accord\. You pulled a wolf out of the river/.test(offer), `Bo's offer under the map: ${offer}`);
    await bo.screenshot(`${OUT}/6-a-mentor-offered-a-tie.png`);
    await bo.evaluate(`[...document.querySelectorAll('.tie-panel button')].find(b => b.textContent === 'Accept').click()`);
    const ashIn = await ashArrival;
    check(ashIn.tie.chosen === 'river', 'Ash picked the river');
    const told = await ash.waitFor(`${S}.posts.map(p => p.text).find(t => /Your tie: They pulled you out of the river/.test(t))`, 15);
    check(/Look for .+ near Stretch 0/.test(told), `Ash is told where to find him: ${told}`);
    await ash.waitFor(`${S}.snapshot?.self?.tie?.marker`, 10);
    const tie = await ash.evaluate(`JSON.stringify(${S}.snapshot.self.tie)`);
    check(/"state":"active"/.test(tie) && /"cell":"c_0_0"/.test(tie), `Ash's tie, with the spot marked: ${tie}`);
    check(await bo.evaluate(`${S}.posts.some(p => /Your tie: You pulled a wolf out of the river/.test(p.text))`), 'Bo is told too');
    await ash.screenshot(`${OUT}/7-told-where-to-find-him.png`);

    // Matchmaking (Phase 4), the Mind off: Ash ends the tie (Bo is free again); the innkeeper knows Bo; Ash asks it.
    await ash.evaluate(`${S}.send({type: 'tie', verb: 'end'})`);
    await bo.evaluate(`${S}.send({type: 'acquaint', npc: 'ui', familiarity: 20})`);
    await bo.evaluate(`${S}.send({type: 'chat', text: '*leans on a post by the inn.*', channel: 'ic', volume: 'speak'})`);
    const boLook = (await ash.evaluate(`${S}.entities.get('${boArrival.id}')?.name`) ?? '').replace(/^A /, 'a ');
    let pointer = '';
    for (let i = 0; i < 16 && !pointer; ++i) {                // (Until the minute's count has Bo about in town.)
        await ash.evaluate(`${S}.send({type: 'chat', text: 'Good day to you. I am new in town.', channel: 'ic', volume: 'speak', targets: ['ui']})`);
        await sleep(5000);
        pointer = await ash.evaluate(`${S}.posts.map(p => p.text).find(t => /^.*Try /.test(t)) ?? ''`);
    }
    check(pointer.includes(`Try ${boLook}`) && /glad to show newcomers about/.test(pointer) && !/\bBo\b/.test(pointer),
        `the innkeeper points Ash at Bo, by his look (${boLook}), without the Mind: ${pointer}`);
    check(await bo.waitFor(`${S}.posts.some(p => /nods your way while talking with/.test(p.text))`, 10), 'Bo sees the innkeeper nod his way');
    await ash.screenshot(`${OUT}/8-the-innkeeper-points-the-way.png`);

    // A first evening at the inn (Phase 5): Cara arrives where the innkeeper works; the innkeeper knows Ash too; dusk.
    await ash.evaluate(`${S}.send({type: 'acquaint', npc: 'ui', familiarity: 20})`);
    const cara = await open();
    const caraIn = await arrive(cara, 'cara', 'Cara', '', '');
    check(caraIn.cell === 'c_0_0', 'Cara arrives in Upper Accord, where the innkeeper works');
    await cara.evaluate(`${S}.send({type: 'time', value: 'dusk'})`);
    const welcome = await cara.waitFor(`${S}.posts.map(p => p.text).find(t => /Welcome, friend/.test(t))`, 20);
    check(!!welcome, `the innkeeper welcomes Cara aloud: ${welcome}`);
    const pointedOut = await cara.waitFor(`${S}.posts.map(p => p.text).filter(t => /^.?That's .+ there: /.test(t)).join(' | ')`, 10);
    check(/That's .+ there: /.test(pointedOut) && !/\bBo\b|\bAsh\b/.test(pointedOut), `and points out the wolves it knows, by look: ${pointedOut}`);
    await cara.waitFor(`document.querySelector('.introduce-prompt')`, 10);
    check(/pointed out the wolves here/.test(await cara.evaluate(`document.querySelector('.introduce-prompt').textContent`)),
        'Cara is asked to introduce herself');
    await cara.screenshot(`${OUT}/9-a-first-evening-at-the-inn.png`);
    await bo.waitFor(`document.querySelector('.introduce-prompt')`, 10);
    check(/pointing you out to a newcomer/.test(await bo.evaluate(`document.querySelector('.introduce-prompt').textContent`)),
        'Bo is asked to introduce himself to a newcomer');
    await bo.evaluate(`[...document.querySelectorAll('.introduce-prompt button')].find(b => b.textContent === 'Introduce yourself').click()`);
    check(!!await cara.waitFor(`${S}.posts.some(p => /I'm Bo\./.test(p.text)) || ${S}.posts.some(p => /Bo/.test(p.text))`, 10) ||
          !!await ash.waitFor(`${S}.posts.some(p => /I'm Bo\./.test(p.text))`, 5), 'Bo introduces himself');
    for (const p of [nina, omar, pia, bo, ash, cara])
        check(!p.console.some(l => l.startsWith('EXCEPTION')), `page errors: ${p.console.filter(l => l.startsWith('EXCEPTION')).join(' / ') || 'none'}`);
} finally {
    for (const r of results) console.log(r);
    for (const b of browsers) await b.close().catch(() => {});
    server.kill();
    await sleep(300);
    rmSync(tmp, {recursive: true, force: true});
}
