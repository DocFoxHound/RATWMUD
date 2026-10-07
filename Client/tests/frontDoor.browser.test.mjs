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

test('choosing a Gift: the tier, one of eight families, its abilities, and the choice sent (doc 43)', async () => {
    const catalog = JSON.parse(readFileSync(join(import.meta.dirname, '..', '..', 'Data', 'Gifts', 'families.json'), 'utf8'));
    const gifts = {tiers: catalog.tiers, families: catalog.families.filter(f => !f.npcOnly)};
    await door(`(d.busy = false, d.receive({type: 'lobby', stage: 'characters', ok: true, characters: [], gifts: ${JSON.stringify(gifts)}}))`);
    await page.evaluate(`window.ratw.page('creator')`);
    await page.evaluate(`[...document.querySelectorAll('.creator-tabs .tab')].find(b => b.textContent === 'Gift').click()`);
    const count = selector => page.evaluate(`document.querySelectorAll(${JSON.stringify(selector)}).length`);
    assert.equal(await count('.gift-card'), 3, 'Normal, Gifted, Quickened');
    assert.equal(await count('.gift-family'), 0, 'a Normal wolf picks no family');
    await page.evaluate(`document.querySelector('.gift-card[data-tier=gifted]').click()`);
    assert.equal(await count('.gift-family'), 8, 'the eight families, no Death Walker');
    await door(`(d.draftName = 'Rill', d.review())`);
    assert.equal(await door('d.page'), 'creator', 'a Gifted wolf with no family is sent back to the Gift tab');
    await page.evaluate(`document.querySelector('.gift-family[data-family=water]').click()`);
    const water = catalog.families.find(f => f.id === 'water');
    assert.equal(await count('.gift-ability'), water.gifted.abilities.length, 'every Gifted Water ability is listed');
    assert.ok(await page.evaluate(`document.querySelector('.gift-details').textContent.includes('Mend')`), 'Mend among them');
    assert.ok(await page.evaluate(`document.querySelector('.gift-details').textContent.includes('AT WORK')`), 'split into work and fight');
    await page.evaluate(`document.querySelector('.gift-card[data-tier=quickened]').click()`);
    assert.equal(await door('d.draftFamily'), 'water', 'switching tier keeps the family');
    assert.ok(await page.evaluate(`document.querySelector('.gift-details').textContent.includes('Freeze')`), 'and shows its Quickened abilities');
    await page.evaluate(`document.querySelector('.gift-family[data-family=blinker]').click()`);
    assert.ok(await page.evaluate(`document.querySelector('.door-preview').textContent.includes('Quickened · Blinker')`), 'the preview says so');
    await door('(d.review(), d.create())');
    assert.deepEqual((await sent()).at(-1).gift, {tier: 'quickened', family: 'blinker'}, 'the Gift is sent with the creation');
    await door(`(d.busy = false, d.receive({type: 'lobby', stage: 'characters', ok: true, characters: [{id: 'w2', name: 'Rill', age: 18,
        appearance: d.draftAppearance, gift: 'blinker', quickened: true}]}))`);
    assert.ok(await page.evaluate(`document.querySelector('.slot').textContent.includes('Quickened · Blinker')`), 'and the roster shows it');
    await page.evaluate(`window.ratw.page('creator')`);
    await door(`(d.draftName = 'Plain', d.review(), d.create())`);
    assert.equal((await sent()).at(-1).gift, undefined, 'a Normal wolf sends no Gift');
});

test('strengths and weaknesses: a preset, the budget, a specialty, and the build sent (doc 49)', async () => {
    // The creator's copy as the server builds it (practice::creationCatalog) from Data/Progression.
    const data = name => JSON.parse(readFileSync(join(import.meta.dirname, '..', '..', 'Data', 'Progression', name), 'utf8'));
    const creationFile = data('creation.json'), skills = data('skills.json');
    const creation = {...creationFile, attributes: skills.attributes.map(a => ({id: a.id, name: a.name, short: a.short,
        ...(a.format === 'percent' ? {percent: true} : {}), ...creationFile.grades[a.id]}))};
    await door(`(d.busy = false, d.receive({type: 'lobby', stage: 'characters', ok: true, characters: [], creation: ${JSON.stringify(creation)}}))`);
    await page.evaluate(`window.ratw.page('creator')`);
    await page.evaluate(`[...document.querySelectorAll('.creator-tabs .tab')].find(b => b.textContent === 'Strengths').click()`);
    const count = selector => page.evaluate(`document.querySelectorAll(${JSON.stringify(selector)}).length`);
    assert.equal(await count('.build-row'), 7, 'seven attributes, each weak, plain or strong');
    assert.ok(await page.evaluate(`document.body.innerText.includes('POINTS LEFT: 2')`), 'two points to spend');
    await page.evaluate(`document.querySelector('.build-chip[data-preset=hunter]').click()`);
    assert.deepEqual(await door('d.draftGrades'), {smell: 'strong', hearing: 'strong', dexterity: 'strong', wisdom: 'weak'}, 'the Hunter preset fills it');
    assert.equal(await door('d.draftSpecialty'), 'tracker', 'with its specialty');
    assert.ok(await page.evaluate(`document.body.innerText.includes('POINTS LEFT: 0')`), 'and spends the budget');
    assert.ok(await page.evaluate(`document.querySelector('.build-row[data-attribute=strength] .build-grade[data-grade=strong]').disabled`),
        'a fourth strength is out of reach');
    assert.ok(await page.evaluate(`document.querySelector('.build-row[data-attribute=smell] .build-grade[data-grade=strong]').textContent.includes('115% → 160%')`),
        'each grade says where it starts and how far it grows');
    await page.evaluate(`document.querySelector('.build-row[data-attribute=strength] .build-grade[data-grade=weak]').click()`);
    assert.ok(await page.evaluate(`document.body.innerText.includes('POINTS LEFT: 1')`), 'a weakness gives a point back');
    await page.evaluate(`document.querySelector('.build-chip[data-specialty=fighter]').click()`);
    await door(`(d.draftName = 'Brindle', d.review())`);
    assert.ok(await page.evaluate(`document.body.innerText.includes('Specialty: Fighter')`), 'the review lists the build');
    await door('d.create()');
    const sentBuild = (await sent()).at(-1).build;
    assert.equal(sentBuild.specialty, 'fighter', 'the build is sent with the creation');
    assert.equal(sentBuild.grades.strength, 'weak');
    await door(`(d.busy = false, d.receive({type: 'lobby', stage: 'characters', ok: true, characters: []}))`);
    await page.evaluate(`window.ratw.page('creator')`);
    await door(`(d.draftName = 'Plain', d.review(), d.create())`);
    assert.equal((await sent()).at(-1).build, undefined, 'a plain wolf sends no build');
});

test('earned Gift tiers: locked cards say what they take, and a locked choice is refused at review (doc 49)', async () => {
    const catalog = JSON.parse(readFileSync(join(import.meta.dirname, '..', '..', 'Data', 'Gifts', 'families.json'), 'utf8'));
    const gifts = {tiers: catalog.tiers, families: catalog.families.filter(f => !f.npcOnly)};
    const tiers = {gifted: {open: false, progress: [{measure: 'socialLevel', have: 1, need: 3, label: 'Social level 1 of 3'},
        {measure: 'normalScenes', have: 4, need: 10, label: 'Scenes as a Normal wolf: 4 of 10'}],
        message: "Gifted isn't open to your account yet: Social level 1 of 3; Scenes as a Normal wolf: 4 of 10."},
        quickened: {open: false, progress: [{measure: 'stars', have: 62, need: 100, label: 'Stars: 62 of 100'}],
            message: "Gifted isn't open to your account yet: Social level 1 of 3."}};
    await door(`(d.busy = false, d.receive({type: 'lobby', stage: 'characters', ok: true, characters: [], gifts: ${JSON.stringify(gifts)},
        tiers: ${JSON.stringify(tiers)}}))`);
    await page.evaluate(`window.ratw.page('creator')`);
    await page.evaluate(`[...document.querySelectorAll('.creator-tabs .tab')].find(b => b.textContent === 'Gift').click()`);
    assert.equal(await page.evaluate(`document.querySelectorAll('.gift-card.locked').length`), 2, 'Gifted and Quickened locked');
    assert.ok(await page.evaluate(`document.querySelector('.gift-card[data-tier=quickened]').textContent.includes('Stars: 62 of 100')`),
        'a locked card says what it takes');
    await page.evaluate(`document.querySelector('.gift-card[data-tier=gifted]').click()`);
    assert.equal(await page.evaluate(`document.querySelectorAll('.gift-family').length`), 8, 'its families can still be read');
    await page.evaluate(`document.querySelector('.gift-family[data-family=water]').click()`);
    await door(`(d.draftName = 'Hopeful', d.review())`);
    assert.equal(await door('d.page'), 'creator', 'a locked tier is refused at review');
    assert.ok((await door('d.message')).startsWith("Gifted isn't open to your account yet"), 'and says why');
    await door(`(d.tiers = null, d.draftTier = 'normal', d.draftFamily = '')`);
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
