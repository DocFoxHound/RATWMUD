// The front door in a real browser (headless Chromium), as the Unreal client's RATW.UI.FrontDoorAccountAndCreation
// checked it: nothing sent that shouldn't be, the password box cleared at once, no double submissions, an explicit
// review before creation, a creation retried under its own receipt, and nothing private kept after signing out.
//
//   npm --prefix Client run build && npm --prefix Client run test:browser
import {test, before, after} from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFileSync, existsSync} from 'node:fs';
import {join, extname} from 'node:path';
import {Browser} from '../../tools/client/browser.mjs';

const dist = join(import.meta.dirname, '..', 'dist');
const types = {'.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.png': 'image/png', '.ttf': 'font/ttf'};
let server, browser, page;

before(async () => {
    assert.ok(existsSync(join(dist, 'index.html')), 'Build the client first: npm --prefix Client run build');
    // The page's files only: with no game server behind it, the connection fails and the front door stays.
    server = createServer((request, response) => {
        const path = join(dist, request.url === '/' ? 'index.html' : request.url.split('?')[0]);
        if (!path.startsWith(dist) || !existsSync(path)) return response.writeHead(404).end();
        response.writeHead(200, {'Content-Type': types[extname(path)] ?? 'application/octet-stream'}).end(readFileSync(path));
    });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    browser = await Browser.launch({width: 1440, height: 940});
    page = await browser.open(`http://127.0.0.1:${server.address().port}/`);
    await page.waitFor('window.ratw && window.ratw.door()');
    // Every command the front door sends is recorded here instead.
    await page.evaluate(`window.sent = []; window.ratw.connection.session.submit = c => window.sent.push(c)`);
});

after(() => {
    browser?.close();
    server?.close();
});

const sent = () => page.evaluate('window.sent');
const door = expression => page.evaluate(`(() => { const d = window.ratw.door(); return ${expression}; })()`);
async function fill(selector, value) {
    await page.evaluate(`(() => { const e = document.querySelector(${JSON.stringify(selector)}); e.value = ${JSON.stringify(value)};
        e.dispatchEvent(new Event('input', {bubbles: true})); })()`);
}
const submitLogin = () => page.evaluate(`document.querySelector('form.door-login').requestSubmit()`);

test('signing in: checked before sending, sent once, the password cleared at once', async () => {
    assert.equal(await door('d.page'), 'login');
    await fill('input[type=text]', 'sample_account');
    await fill('input[type=password]', 'short');
    await submitLogin();
    assert.deepEqual(await sent(), [], 'a short password is refused before sending');
    await fill('input[type=password]', 'test-only-secret-123');
    await submitLogin();
    const commands = await sent();
    assert.equal(commands.length, 1);
    assert.equal(commands[0].type, 'auth_login');
    assert.equal(commands[0].username, 'sample_account');
    assert.equal(await page.evaluate(`document.querySelector('input[type=password]').value`), '', 'the password box is cleared');
    assert.ok(await door('d.busy'));
    await fill('input[type=password]', 'test-only-secret-123');
    await submitLogin();
    assert.equal((await sent()).length, 1, 'a second click cannot duplicate a request in flight');
});

test('creating a character: an explicit review, one request, retried under its own receipt', async () => {
    await door(`d.receive({type: 'lobby', stage: 'characters', ok: true, characters: []})`);
    assert.equal(await door('d.page'), 'roster');
    assert.equal(await door('d.characters.length'), 0, 'no sample character is made up');
    await page.evaluate(`window.ratw.page('creator')`);
    assert.equal(await door('d.page'), 'creator');
    assert.equal(await door('d.draftAge'), 18);
    assert.equal(await door('Object.keys(d.draftAppearance).length'), 9, 'all nine appearance fields');
    await door(`(d.draftName = '  Briar  ', d.draftAge = 12, Object.assign(d.draftAppearance, {species: 'maned', pattern: 'saddle', baseColor: 6}))`);
    await door('d.review()');
    assert.equal(await door('d.draftName'), 'Briar', 'the name is trimmed');
    assert.equal(await door('d.page'), 'review');
    assert.equal((await sent()).length, 1, 'previewing and reviewing send nothing');
    await page.key('Escape');
    assert.equal(await door('d.page'), 'creator', 'Escape goes back to appearance');
    assert.equal(await door('d.draftAppearance.species'), 'maned', 'keeping the draft');
    await door('(d.review(), d.create())');
    let commands = await sent();
    assert.equal(commands.length, 2);
    const create = commands[1];
    assert.equal(create.type, 'character_create', 'creation, not entry');
    assert.equal(create.age, 12);
    assert.equal(create.appearance.species, 'maned');
    assert.ok(create.commandId);
    await door('(d.busy = false, d.create())');
    assert.equal((await sent()).at(-1).commandId, create.commandId, 'a retry of the same draft keeps its receipt');
    await door('(d.busy = false, d.draftAppearance.baseColor = 7, d.create())');
    assert.notEqual((await sent()).at(-1).commandId, create.commandId, 'a changed draft is a new request');
    await door(`d.receive({type: 'lobby', stage: 'characters', ok: false, message: 'Try again.', characters: []})`);
    assert.equal(await door('d.page'), 'review', 'a failed creation keeps the review');
    assert.equal(await door('d.draftName'), 'Briar');
    await door(`d.receive({type: 'lobby', stage: 'characters', ok: true, characters: [{id: 'owned_character_1', name: 'Briar', age: 12,
        appearance: d.draftAppearance}]})`);
    assert.equal(await door('d.page'), 'roster', 'back to selection, not straight into the world');
    assert.equal(await door('d.selectedId'), 'owned_character_1', 'the new character is selected');
    await door('d.enter()');
    commands = await sent();
    assert.deepEqual([commands.at(-1).type, commands.at(-1).id], ['character_enter', 'owned_character_1']);
});

test('signing out forgets the roster and any draft', async () => {
    await door(`(d.busy = false, d.receive({type: 'lobby', stage: 'login', ok: true, characters: []}))`);
    assert.equal(await door('d.characters.length'), 0);
    assert.equal(await door('d.draftAppearance'), null);
    assert.equal(await door('d.page'), 'login');
});

test('the page shows no errors', async () => {
    assert.deepEqual(page.console.filter(line => line.startsWith('EXCEPTION')), []);
});
