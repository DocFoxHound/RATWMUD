// The desktop client's flow, the whole way through (Docs/Design/77, Phase 3), shared by the browser's test
// (client.browser.test.mjs: the built page in headless Chromium with ?central=) and the real app's
// (Desktop/tests/app.test.mjs: the Tauri window, driven through its e2e hook). `page` has evaluate(expression) and
// waitFor(expression, seconds); `h` is tests/harness.mjs's central and world.
//
// Register (the age gate), the main menu, make a wolf with a Gift family, its strengths and its tie (the Character
// Editor), change them, the server browser from central, a server then the wolf, the world's first-arrival screen (the
// tier and the town; the strengths and tie the wolf's own), in the world, leave back to the servers, rejoin straight in
// (by the address box), the world going down (Reconnect), delete the wolf, sign out, sign in, delete the account.
import assert from 'node:assert/strict';

export const PASSWORD = 'a long test password';

/** The steps' helpers, on one page. */
export function on(page, h) {
    const door = expression => page.evaluate(`(() => { const d = window.ratw.door(); return ${expression}; })()`);
    async function waitDoor(expression, seconds = 20) {
        try {
            return await page.waitFor(`(() => { const d = window.ratw.door(); return d && (${expression}); })()`, seconds);
        } catch (error) {
            const state = await page.evaluate(`(() => { const d = window.ratw.door(); return d ? {page: d.page, busy: d.busy, message: d.message} : 'no door'; })()`);
            throw new Error(`${error.message}\n  the door: ${JSON.stringify(state)}\n  the page: ${(page.console ?? []).slice(-10).join('\n')}`);
        }
    }

    async function fill(selector, value) {
        await page.evaluate(`(() => { const e = document.querySelector(${JSON.stringify(selector)}); e.value = ${JSON.stringify(value)};
            e.dispatchEvent(new Event('input', {bubbles: true})); })()`);
    }
    const click = selector => page.evaluate(`(() => { const e = document.querySelector(${JSON.stringify(selector)}); if (!e) throw new Error('no ${selector.replaceAll("'", '')}'); e.click(); })()`);
    const clickText = text => page.evaluate(`(() => { const b = [...document.querySelectorAll('button')].find(b => b.textContent.trim() === ${JSON.stringify(text)});
        if (!b) throw new Error('no button ${text}'); b.click(); })()`);
    const message = () => door('d.message');
    async function waitGame(seconds = 40) {
        try {
            return await page.waitFor('window.ratw.game()?.state.snapshot', seconds);
        } catch (error) {
            const state = await page.evaluate(`(() => { const d = window.ratw.door(); return d ? {page: d.page, busy: d.busy, message: d.message} : 'no door'; })()`);
            throw new Error(`${error.message}\n  the door: ${JSON.stringify(state)}\n  the world: ${h.world.lines.slice(-8).join('\n')}`);
        }
    }
    /** Answers the page's own dialog: types into its box (if any) and confirms. */
    async function answerDialog(value) {
        await page.waitFor(`document.querySelector('.ratw-dialog')`);
        if (value !== undefined) await fill('.ratw-dialog input', value);
        await click('.ratw-dialog .primary');
        await page.waitFor(`!document.querySelector('.ratw-dialog')`);
    }
    return {door, waitDoor, fill, click, clickText, message, waitGame, answerDialog};
}

export async function clientFlow(page, h, {native = false} = {}) {
    const {door, waitDoor, fill, click, clickText, message, waitGame, answerDialog} = on(page, h);
    // Signing in to central: the bundled page's own sign-in, never a world's.
    assert.equal(await door('d.mode'), 'central');
    assert.equal(await door('d.page'), 'login');
    if (native) assert.notEqual(await page.evaluate(`document.querySelector('input[name=remember]')`), null, '"remember me" in the client');
    else assert.equal(await page.evaluate(`document.querySelector('input[name=remember]')`), null, 'no "remember me" in a browser');
    await clickText('New here? Create an account');
    assert.equal(await door('d.page'), 'register');
    await fill('input[name=username]', 'clientwolf');
    await fill('input[name=password]', PASSWORD);
    await fill('input[name=birthYear]', String(new Date().getFullYear() - 10));
    await page.evaluate(`document.querySelector('form.door-login').requestSubmit()`);
    await waitDoor('!d.busy');
    assert.match(await message(), /16 or older/, 'the age gate is central\'s');
    assert.equal(await door('d.page'), 'register');
    await fill('input[name=password]', PASSWORD);
    await fill('input[name=birthYear]', '1990');
    await page.evaluate(`document.querySelector('form.door-login').requestSubmit()`);
    await waitDoor(`d.page === 'menu' && !d.busy`);
    assert.equal(await door('d.characters.length'), 0);
    assert.equal(await page.evaluate(`document.querySelector('input[type=password]')`), null, 'no password box left');
    // The main menu (the user, 2026-11-14): Servers, the Character Editor, the Editor and the Dungeon Master.
    assert.deepEqual(await page.evaluate(`[...document.querySelectorAll('.menu-tile')].map(b => b.dataset.action)`),
        ['servers', 'characters', 'editor', 'dungeon-master']);
    assert.equal(await page.evaluate(`document.querySelector('.menu-tile[data-action=editor]').disabled`), !native, 'the Editor is the desktop client\'s');

    // The Character Editor's creator: the wolf's identity, its Gift, its strengths and its tie.
    await click('button[data-action=characters]');
    await waitDoor(`d.page === 'roster'`);
    assert.equal(await page.evaluate(`document.querySelector('button[data-action=editor]')`), null, 'no Editor button here');
    await page.evaluate(`document.querySelector('.slot').click()`);
    assert.equal(await door('d.page'), 'creator');
    assert.deepEqual(await page.evaluate(`[...document.querySelectorAll('.creator-tabs .tab')].map(b => b.dataset.tab)`),
        ['body', 'coat', 'markings', 'eyes', 'gift', 'strengths', 'tie', 'name'], 'the strengths and the tie are the wolf\'s own; the town is each world\'s');
    assert.deepEqual(await page.evaluate(`[...document.querySelectorAll('.door-actions button')].map(b => b.textContent)`),
        ['Back to the main menu', 'REVIEW CHARACTER'], 'no Choose a World, no Editor');
    await click('.creator-tabs [data-tab=gift]');
    assert.deepEqual(await page.evaluate(`[...document.querySelectorAll('.gift-card')].map(b => [b.dataset.tier, b.disabled])`),
        [['normal', false], ['gifted', false], ['quickened', true]], 'Quickened greyed: the account hasn\'t opened it');
    assert.match(await page.evaluate(`document.querySelector('.gift-card[data-tier=quickened]').textContent`), /social level 8/);
    // Once an official world opens Quickened for the account (doc 49, 9.1), central says so and it opens here.
    await door(`(d.accountQuickened = true, d.show('creator'), 1)`);
    assert.equal(await page.evaluate(`document.querySelector('.gift-card[data-tier=quickened]').disabled`), false,
        'the account has Quickened: open for every world');
    await door(`(d.accountQuickened = false, d.show('creator'), 1)`);
    await click('.gift-card[data-tier=gifted]');
    assert.equal(await page.evaluate(`document.querySelectorAll('.gift-family').length`), 8, 'the families come from central');
    await click('.gift-family[data-family=fire]');
    assert.equal(await door('d.draftFamily'), 'fire');
    await click('.creator-tabs [data-tab=strengths]');
    assert.equal(await page.evaluate(`document.querySelectorAll('.build-row').length`), 7, 'the attributes come from central');
    await click('.build-chip[data-preset=hunter]');
    await click('.creator-tabs [data-tab=tie]');
    const tie = await page.evaluate(`document.querySelectorAll('.tie-list [data-tie]')[1].dataset.tie`);
    await click(`.tie-list [data-tie="${tie}"]`);
    await click('.creator-tabs [data-tab=name]');
    assert.equal(await page.evaluate(`document.querySelector('input[name=wolfName]').maxLength`), 24, 'a name is at most 24 characters');
    await fill('input[name=wolfName]', 'emberline');
    assert.equal(await door('d.draftName'), 'Emberline');
    await clickText('REVIEW CHARACTER');
    assert.equal(await door('d.page'), 'review');
    assert.match(await page.evaluate(`document.querySelector('.door-fields').textContent`), /Fire family/);
    await clickText('CONFIRM & CREATE');
    await waitDoor(`d.page === 'roster' && d.characters.length === 1 && !d.busy`);
    assert.equal(await door('d.characters[0].name'), 'Emberline');
    assert.match(await page.evaluate(`document.querySelector('.slot.selected').textContent`), /Fire family/);
    assert.deepEqual(await door('d.characters[0].choices'), {grades: {smell: 'strong', hearing: 'strong', dexterity: 'strong', wisdom: 'weak'}, tie},
        'the strengths and the tie are kept with the wolf on central');
    // ...and changed later, from the selected wolf.
    await click('button[data-action=choices]');
    await waitDoor(`d.page === 'choices'`);
    await click('.build-row[data-attribute=strength] .build-grade[data-grade=weak]');
    await click('button[data-action=save-choices]');
    await waitDoor(`d.page === 'roster' && !d.busy`);
    assert.equal(await door('d.characters[0].choices.grades.strength'), 'weak');

    // The servers from central: the test world, its status asked of the world itself.
    await click('button[data-action=menu]');
    await waitDoor(`d.page === 'menu'`);
    await click('button[data-action=servers]');
    await waitDoor(`d.page === 'servers' && !d.busy`);
    assert.equal(await door('d.worlds.length'), 1);
    assert.equal(await door('d.worlds[0].id'), h.world.id);
    await page.waitFor(`document.querySelector('.server-ping')?.textContent.includes('ms')`);
    assert.equal(await page.evaluate(`document.querySelector('.server-players').textContent`), '0 / 100');
    assert.equal(await door('d.worlds[0].fit'), 'ok', 'same protocol and client-data hash as the world');

    // Join: the server, then the wolf; a ticket, hello{ticket, protocol}, the world's first-arrival screen.
    await click(`tr[data-world="${h.world.id}"]`);
    await click('button[data-action=server-join]');
    await waitDoor(`d.page === 'pick'`);
    await click('.pick-card');
    await click('button[data-action=play]');
    await waitDoor(`d.page === 'arrive'`, 30);
    assert.deepEqual(await page.evaluate(`[...document.querySelectorAll('.creator-tabs .tab')].map(b => b.dataset.tab)`),
        ['gift', 'arrival'], 'the world asks the tier and the town only');
    assert.deepEqual(await door('d.tierChoices'), ['gifted', 'quickened'], 'a wolf with a family arrives Gifted or Quickened');
    assert.equal(await door('d.draftTier'), 'gifted');
    assert.equal(await door('d.arriveWolf.name'), 'Emberline');
    assert.equal(await door('d.draftGrades.strength'), 'weak', 'the wolf\'s own strengths go with it');
    assert.equal(await door('d.draftTie'), tie, 'and its tie');
    await clickText('ARRIVE');
    await waitGame();
    assert.equal(await page.evaluate('window.ratw.shell.inWorld'), true);
    assert.equal(await page.evaluate('window.ratw.door()'), null);
    assert.match(await page.evaluate(`JSON.stringify(window.ratw.game().state.snapshot.self)`), /Emberline/);

    // In the world, the guard keeps reload off; Ctrl+C reaches the page, not the character sheet.
    const reload = await page.evaluate(`(() => { const e = new KeyboardEvent('keydown', {code: 'F5', key: 'F5', bubbles: true, cancelable: true});
        window.dispatchEvent(e); return e.defaultPrevented; })()`);
    assert.equal(reload, true, 'F5 is blocked in the world');
    await page.evaluate(`window.dispatchEvent(new KeyboardEvent('keydown', {code: 'KeyC', key: 'c', ctrlKey: true, bubbles: true}))`);
    assert.equal(await page.evaluate('window.ratw.game().state.modal'), '', 'Ctrl+C leaves the character sheet shut');

    // Leave: back to the servers.
    await page.evaluate('window.ratw.game().state.leaveCharacter()');
    await waitDoor(`d.page === 'servers' && !d.busy`, 20);
    assert.match(await message(), /left the world/);
    assert.equal(await page.evaluate('window.ratw.shell.inWorld'), false);

    // Rejoin, by the address box this time: straight in, no arrival screen.
    await fill('input[name=address]', `127.0.0.1:${h.world.port}`);
    await page.evaluate(`document.querySelector('form.world-direct').requestSubmit()`);
    await waitDoor(`d.page === 'pick'`);
    await click('.pick-card');
    await click('button[data-action=play]');
    await waitGame();
    assert.match(await page.evaluate(`JSON.stringify(window.ratw.game().state.snapshot.self)`), /Emberline/);

    // The world goes down: the list again, with Reconnect.
    await h.stopWorld();
    await waitDoor(`d.page === 'servers'`, 30);
    assert.match(await message(), /connection to Client Test Hollow was lost/);
    assert.equal(await door('d.lostWorld'), h.world.id);
    assert.equal(await page.evaluate(`document.querySelector('button[data-action=server-join]').textContent`), 'RECONNECT');
    await click('button[data-action=server-join]');
    await waitDoor(`!d.busy && d.page === 'servers'`, 30);
    assert.match(await message(), /Could not reach Client Test Hollow/, 'a new ticket, but nobody answers');

    // Delete the wolf (the password asked again, in the page's own dialog), then sign out and back in.
    await click('button[data-action=menu]');
    await click('button[data-action=characters]');
    await waitDoor(`d.page === 'roster'`);
    // The page's own dialog: Tab stays in it, and Enter on a focused Cancel doesn't confirm.
    await click('button[data-action=delete-wolf]');
    await page.waitFor(`document.querySelector('.ratw-dialog')`);
    const key = code => page.evaluate(`(() => { (document.activeElement || document.body).dispatchEvent(new KeyboardEvent('keydown',
        {code: '${code}', key: '${code}', bubbles: true, cancelable: true})); return true; })()`);
    for (let i = 0; i < 5; ++i) await key('Tab');
    assert.equal(await page.evaluate(`document.querySelector('.ratw-dialog').contains(document.activeElement)`), true, 'Tab stays in the dialog');
    await page.evaluate(`(() => { document.querySelector('.ratw-dialog .secondary').focus(); return true; })()`);
    await key('Enter');
    assert.equal(await page.evaluate(`!!document.querySelector('.ratw-dialog')`), true, 'Enter on Cancel did not confirm');
    await click('.ratw-dialog .secondary');
    await page.waitFor(`!document.querySelector('.ratw-dialog')`);
    assert.equal(await door('d.characters.length'), 1);
    await click('button[data-action=delete-wolf]');
    await answerDialog('not my password at all');
    await waitDoor('!d.busy');
    assert.match(await message(), /password/i);
    assert.equal(await door('d.characters.length'), 1);
    await click('button[data-action=delete-wolf]');
    await answerDialog(PASSWORD);
    await waitDoor('!d.busy && d.characters.length === 0');
    assert.match(await message(), /Emberline is deleted/);
    // The user makes this account a host (Q11: approved hosts); it shows from the next sign-in.
    await click('button[data-action=menu]');
    await waitDoor(`d.page === 'menu'`);
    assert.equal(await page.evaluate(`document.querySelector('button[data-action=host]')`), null, 'no Host page before');
    h.flagHost('clientwolf');
    await click('button[data-action=sign-out]');
    await waitDoor(`d.page === 'login' && !d.busy`);
    await fill('input[name=username]', 'clientwolf');
    await fill('input[name=password]', PASSWORD);
    await page.evaluate(`document.querySelector('form.door-login').requestSubmit()`);
    await waitDoor(`d.page === 'menu' && !d.busy`);

    // The Host page (doc 77, 3.2 and Phase 4): make a world, its token shown once; a refused name; a new token; remove it.
    await click('button[data-action=host]');
    await waitDoor(`d.page === 'host' && !d.busy`);
    assert.equal(await door('d.hostWorlds.length'), 0);
    await fill('input[name=worldName]', 'The Official Hollow');
    await click('button[data-action=host-make]');
    await waitDoor('!d.busy');
    assert.match(await message(), /official|name/i, 'a community world can\'t call itself official');
    assert.equal(await door('d.hostWorlds.length'), 0);
    await fill('input[name=worldName]', 'Laptop Hollow');
    await click('button[data-action=host-make]');
    await waitDoor(`!d.busy && d.hostWorlds.length === 1`);
    const firstToken = await door('d.hostToken');
    assert.match(firstToken, /^rh_[A-Za-z0-9_-]{40,}$/, 'the token, shown once');
    assert.equal(await page.evaluate(`document.querySelector('input[name=hostToken]').value`), firstToken);
    assert.equal(await door('d.hostWorlds[0].listed'), false, 'not listed until its agent sends a heartbeat');
    const made = await door('d.hostWorlds[0].id');
    await click(`button[data-action=host-token][data-world="${made}"]`);
    await answerDialog();
    await waitDoor(`!d.busy && d.hostToken && d.hostToken !== ${JSON.stringify(firstToken)}`);
    await click(`button[data-action=host-remove][data-world="${made}"]`);
    await answerDialog();
    await waitDoor(`!d.busy && d.hostWorlds.length === 0`);
    assert.equal(await door('d.hostToken'), '', 'its token gone with it');
    await click('button[data-action=host-back]');
    await waitDoor(`d.page === 'menu'`);

    // Delete the account.
    await click('button[data-action=delete-account]');
    await answerDialog(PASSWORD);
    await waitDoor(`d.page === 'login' && !d.busy`);
    assert.match(await message(), /account and its wolves are deleted/);
    await fill('input[name=username]', 'clientwolf');
    await fill('input[name=password]', PASSWORD);
    await page.evaluate(`document.querySelector('form.door-login').requestSubmit()`);
    await waitDoor('!d.busy');
    assert.equal(await door('d.page'), 'login', 'the account is gone');
}
