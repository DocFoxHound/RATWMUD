// Scenes in the real page (Docs/Design/08-social-progression.md): two players in a party talk; the scene line says
// what each still needs to be paid, then that both are on track; a quiet scene's countdown and two scenes at once are
// shown from a snapshot set by hand (fifteen real minutes are too long to wait); Ash steps out with LEAVE and is paid;
// Bo is then in her Known wolves with a recap of the scene (doc 50, Phase 4), and on his card under YOU AND THEM.
// Screenshots go to artifacts/screenshots/scenes/.
//
//   node tools/client/scenes.mjs [OUT]         (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist by default)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const tmp = mkdtempSync(join(tmpdir(), 'ratw-scenes-'));
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/scenes`;
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
mkdirSync(OUT, {recursive: true});
const port = 7900 + Math.floor(Math.random() * 90);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const server = spawn(binary, ['--port', String(port), '--web', web, '--save', `${tmp}/scenes-save.json`,
    '--dev-identity', '--dev-tools'], {stdio: ['ignore', 'ignore', 'pipe']});
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
const Lines = [
    '"The river rose in the night and took the lower bridge with it, so we came the long way round by the mill."',
    '"Then you will have seen the miller\'s dog, the grey one that guards the ford and barks at every passing cart."',
    '"We did, and it followed us halfway to the crossroads before it lost interest and wandered back home again."',
    '"That dog has walked that road longer than I have been alive, and it still thinks the whole valley is its own."',
];
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
    const boId = await bo.evaluate(`${S}.selfId`);
    const act = (page, action, target = '') =>
        page.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: ${JSON.stringify(action)}, target: ${JSON.stringify(target)}})`);
    const say = (page, text) =>
        page.evaluate(`(() => { const s = ${S}; s.setChat(true); s.composer.text = ${JSON.stringify(text)}; s.submitPost(); })()`);
    const bar = page => page.evaluate(`document.querySelector('.scene-bar')?.innerText ?? ''`);
    await sleep(800);
    await ash.evaluate(`${S}.sendAction('invite', ${JSON.stringify(boId)})`);
    await bo.waitFor(`${S}.party?.invite`, 10);
    await act(bo, 'party_verb', 'accept');
    await ash.waitFor(`${S}.party?.members?.length === 2`, 10);
    await act(ash, 'party');
    await act(bo, 'party');
    // The first line opens the party's scene: Ash still needs more.
    await say(ash, Lines[0]);
    await ash.waitFor(`document.querySelector('.scene-row') !== null`, 10);
    const early = await bar(ash);
    check(/PARTY SCENE/.test(early) && /To be paid: 1 more line/.test(early), `Ash sees what she still needs: ${JSON.stringify(early)}`);
    await ash.screenshot(`${OUT}/1-needs-ash.png`);
    // Two lines each (the server takes a line a wolf every two seconds): both on track.
    for (let i = 1; i < 4; ++i) {
        await sleep(2200);
        await say(i % 2 ? bo : ash, Lines[i]);
    }
    await ash.waitFor(`/On track to be paid/.test(document.querySelector('.scene-bar')?.innerText ?? '')`, 10);
    check(true, 'both have said enough: "On track to be paid"');
    await ash.screenshot(`${OUT}/2-on-track-ash.png`);
    // A quiet scene and a second one, as the server would send them, to see the layout.
    await ash.evaluate(`(() => {
        const real = ${S}.snapshot.self.social.scenes[0];
        const scenes = [{...real, quiet: true, endsIn: 240, next: true},
            {id: 'scene-room', party: false, fight: false, with: ['A dun wolf'], turns: 1, needTurns: 1, needWords: 14,
             needReply: true, othersShaped: 0, quiet: false, endsIn: 1790, next: false}];
        // (Kept in place over the snapshots that keep coming, until put back.)
        const state = ${S}, apply = state.applySnapshot;
        const hold = () => { state.snapshot.self.social.scenes = scenes; };
        state.applySnapshot = function (...a) { const r = apply.apply(this, a); hold(); return r; };
        window.sceneHold = () => { state.applySnapshot = apply; };
        hold();
    })()`);
    await sleep(600);
    const two = await bar(ash);
    check(/YOUR WORDS GO HERE/.test(two) && /Quiet · ends in 4 min/.test(two) && /IN A SCENE/.test(two),
        `two scenes, the next marked, the quiet one counting down: ${JSON.stringify(two)}`);
    check(await ash.evaluate(`/gone quiet: it ends in 4 min/.test(document.querySelector('.toast')?.innerText ?? '')`),
        'five minutes left: one toast');
    await ash.screenshot(`${OUT}/3-two-scenes-quiet-ash.png`);
    await ash.evaluate(`window.sceneHold()`);
    // LEAVE: settled for her now, Bo carries on.
    await sleep(2500);                                    // (The next snapshot puts back the true scenes.)
    await ash.evaluate(`window.confirm = () => true`);
    await ash.evaluate(`[...document.querySelectorAll('.scene-row button')].find(b => b.textContent === 'LEAVE')?.click()`);
    await ash.waitFor(`${S}.posts.some(p => /You step out of the scene: \\+20 social/.test(p.text))`, 10);
    check(true, 'Ash steps out and is paid at once');
    await ash.waitFor(`document.querySelector('.scene-row') === null`, 10);
    check(/PARTY SCENE/.test(await bar(bo)), 'Bo is still in the scene');
    await ash.screenshot(`${OUT}/4-left-ash.png`);
    await bo.screenshot(`${OUT}/5-carries-on-bo.png`);
    // Known wolves (doc 50, Phase 4): Bo is on her list, with a recap of the scene from her side (written: no Mind here).
    await act(ash, 'people');
    await ash.waitFor(`document.querySelector('[data-tab="known"]') !== null`, 10);
    await ash.evaluate(`[...document.querySelectorAll('[data-tab="known"]')][0].click()`);
    check(await ash.waitFor(`${S}.knownWolves.some(k => k.id === ${JSON.stringify(boId)} && k.scenes === 1 && k.recaps?.[0]?.text?.startsWith('You shared a scene with'))`, 10)
        .then(() => true).catch(() => false), 'Bo is in her Known wolves, one scene shared, with its recap');
    check(await ash.waitFor(`document.body.innerText.includes('KNOWN WOLVES') && document.body.innerText.includes('You shared a scene with')`, 10)
        .then(() => true).catch(() => false), 'shown in the Known wolves tab');
    await ash.screenshot(`${OUT}/6-known-wolves-ash.png`);
    await act(ash, 'close');
    await ash.evaluate(`${S}.send({type: 'action', action: 'inspect', target: ${JSON.stringify(boId)}})`);
    await ash.waitFor(`document.querySelector('[data-tab="them"]') !== null`, 10);
    await ash.evaluate(`document.querySelector('[data-tab="them"]').click()`);
    check(await ash.waitFor(`document.body.innerText.includes('scene shared') && document.body.innerText.includes('You shared a scene with')`, 10)
        .then(() => true).catch(() => false), 'and on his card, under YOU AND THEM');
    await ash.screenshot(`${OUT}/7-you-and-them-ash.png`);
    const errors = [...ash.console, ...bo.console].filter(l => /EXCEPTION|error/i.test(l));
    check(!errors.length, `page errors: ${errors.length ? errors.join(' | ') : 'none'}`);
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
