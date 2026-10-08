// Story books in the real page (Docs/Design/51-scenes-and-stars.md, Phase 7): Ash and Bo's scene ends; Ash puts it in
// a new book from the scene's card (ADD TO A STORY); the book opens with its chapter; Bo's next scene goes into it;
// Ash shares it with friends and proposes finishing it; Bo agrees from the book; it sits finished (gilded) at the top
// of Ash's shelf, and Cy, Ash's friend, finds it under Unaffiliated. Screenshots go to artifacts/screenshots/books/.
//
//   node tools/client/books.mjs [OUT]          (RATW_SERVER: the server binary; RATW_WEB: the built page, Client/dist by default)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const tmp = mkdtempSync(join(tmpdir(), 'ratw-books-'));
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/books`;
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
const web = process.env.RATW_WEB ?? `${root}/Client/dist`;
mkdirSync(OUT, {recursive: true});
const port = 7900 + Math.floor(Math.random() * 90);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const server = spawn(binary, ['--port', String(port), '--web', web, '--save', `${tmp}/books-save.json`, '--dev-identity', '--dev-tools'],
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
        await page.waitFor(`${S}.snapshot?.self?.id`, 20);
        return page;
    };
    const ash = await open('ash'), bo = await open('bo'), cy = await open('cy');
    const until = (page, condition, seconds = 10) => page.waitFor(condition, seconds).then(() => true).catch(() => false);
    const act = (page, action, target = '') =>
        page.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: ${JSON.stringify(action)}, target: ${JSON.stringify(target)}})`);
    const say = (page, text) => page.evaluate(`(() => { const s = ${S}; s.setChat(true); s.composer.text = ${JSON.stringify(text)}; s.submitPost(); })()`);
    const click = (page, selector, text) => page.evaluate(`[...document.querySelectorAll(${JSON.stringify(selector)})].find(b => b.textContent === ${JSON.stringify(text)})?.click()`);
    const boId = await bo.evaluate(`${S}.selfId`);
    await sleep(800);
    await ash.evaluate(`${S}.sendAction('invite', ${JSON.stringify(boId)})`);
    await bo.waitFor(`${S}.party?.invite`, 10);
    await act(bo, 'party_verb', 'accept');
    await ash.waitFor(`${S}.party?.members?.length === 2`, 10);
    await act(ash, 'party');
    await act(bo, 'party');
    const talk = async suffix => {
        for (let i = 0; i < 4; ++i) {
            await sleep(2200);
            await say(i % 2 ? bo : ash, Lines[i].replace(/\."$/, `${suffix}."`));
        }
        await sleep(1500);
        await ash.evaluate(`${S}.sendAction('session_end')`);
    };
    await talk('');
    check(await until(ash, `!!document.querySelector('.scene-ended')`), 'the scene ends: its card');
    // ADD TO A STORY → START A BOOK, from the card.
    await click(ash, '.scene-ended button', 'ADD TO A STORY');
    await until(ash, `!!document.querySelector('.add-to-story input')`);
    await ash.evaluate(`document.querySelector('.add-to-story input').value = 'The Drowned Bell'`);
    await click(ash, '.add-to-story button', 'START A BOOK');
    check(await until(ash, `${S}.posts.some(p => p.text.includes('You begin "The Drowned Bell"'))`), 'a book begun from the scene');
    await act(ash, 'close');
    await ash.evaluate(`${S}.openShelf('shelf', 'all')`);
    check(await until(ash, `[...document.querySelectorAll('.spine')].some(s => s.innerText.includes('The Drowned Bell'))`), 'its spine on Ash\'s shelf');
    await ash.evaluate(`document.querySelector('.spine').click()`);
    check(await until(ash, `${S}.bookOpen?.title === 'The Drowned Bell' && document.querySelectorAll('.book-chapter').length === 1`), 'opened: one chapter');
    await ash.screenshot(`${OUT}/1-a-book-opened-ash.png`);
    // Bo: his next scene goes into it.
    const bookId = await ash.evaluate(`${S}.bookOpen.id`);
    await bo.evaluate(`${S}.sendBook('next', {book: ${JSON.stringify(bookId)}, on: true})`);
    check(await until(bo, `${S}.posts.some(p => p.text.includes('Your next scene goes into'))`), 'Bo flags his next scene');
    await talk(' Truly');
    check(await until(bo, `${S}.posts.some(p => /The scene is in "The Drowned Bell"/.test(p.text))`), 'and it goes in when it ends');
    // Friends with Cy, shared with friends; finishing, agreed by Bo.
    for (const [page, handle] of [[ash, 'Adder'], [cy, 'Cypress']]) await page.evaluate(`${S}.sendProfile('handle', {handle: ${JSON.stringify(handle)}})`);
    await sleep(600);
    await cy.evaluate(`${S}.sendFriends('request', {handle: 'Adder'})`);
    await until(ash, `${S}.friendRequestsIn.length === 1`);
    await ash.evaluate(`${S}.sendFriends('accept', {handle: 'Cypress'})`);
    await ash.evaluate(`${S}.sendBook('share', {book: ${JSON.stringify(bookId)}, sharing: 'friends'})`);
    await ash.evaluate(`${S}.openBook(${JSON.stringify(bookId)})`);
    await until(ash, `[...document.querySelectorAll('button')].some(b => b.textContent === 'FINISH')`);
    await click(ash, 'button', 'FINISH');
    check(await until(ash, `${S}.posts.some(p => p.text.includes('This book has private scenes'))`), 'finishing proposed, with the private-scene warning');
    await bo.evaluate(`${S}.openBook(${JSON.stringify(bookId)})`);
    await until(bo, `[...document.querySelectorAll('button')].some(b => b.textContent === 'AGREE')`);
    await click(bo, 'button', 'AGREE');
    check(await until(bo, `${S}.posts.some(p => p.text.includes('"The Drowned Bell" is finished.'))`), 'Bo agrees: finished');
    await ash.evaluate(`${S}.openShelf('shelf', 'all')`);
    check(await until(ash, `document.querySelector('.spine')?.classList.contains('finished')`), 'gilded, at the top of Ash\'s shelf');
    await ash.screenshot(`${OUT}/2-the-shelf-ash.png`);
    await cy.evaluate(`${S}.openShelf('unaffiliated', 'all')`);
    check(await until(cy, `[...document.querySelectorAll('.spine.kind-friend')].some(s => s.innerText.includes('The Drowned Bell'))`),
        'Cy finds it under Unaffiliated, a friend\'s book');
    await cy.evaluate(`document.querySelector('.spine').click()`);
    check(await until(cy, `document.querySelectorAll('.book-chapter').length === 2 && !document.body.innerText.includes('A private scene: its summary once')`),
        'and reads it, its private scenes no longer sealed');
    await cy.screenshot(`${OUT}/3-a-friends-book-cy.png`);
    for (const page of [ash, bo, cy]) {
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
