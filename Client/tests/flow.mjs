// The desktop client's flow, the whole way through (Docs/Design/77, Phase 3), shared by the browser's test
// (client.browser.test.mjs: the built page in headless Chromium with ?central=) and the real app's
// (Desktop/tests/app.test.mjs: the Tauri window, driven through its e2e hook). `page` has evaluate(expression) and
// waitFor(expression, seconds); `h` is tests/harness.mjs's central and world.
//
// Register (the age gate), make a wolf with a Gift family (the identity tabs only), the world list from central, join,
// the world's first-arrival screen (tier, strengths, town, tie), in the world, leave back to the list, rejoin straight
// in (by the address box), the world going down (Reconnect), delete the wolf, sign out, sign in, delete the account.
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
    await waitDoor(`d.page === 'roster' && !d.busy`);
    assert.equal(await door('d.characters.length'), 0);
    assert.equal(await page.evaluate(`document.querySelector('input[type=password]')`), null, 'no password box left');

    // The creator: the identity tabs only, and a Gift family.
    await page.evaluate(`window.ratw.page('creator')`);
    assert.equal(await door('d.page'), 'creator');
    assert.deepEqual(await page.evaluate(`[...document.querySelectorAll('.creator-tabs .tab')].map(b => b.dataset.tab)`),
        ['body', 'coat', 'markings', 'eyes', 'gift', 'name'], 'Arrival, Tie and Strengths are each world\'s');
    await click('.creator-tabs [data-tab=gift]');
    assert.equal(await page.evaluate(`document.querySelectorAll('.gift-family').length`), 8, 'the families come from central');
    await click('.gift-family[data-family=fire]');
    assert.equal(await door('d.draftFamily'), 'fire');
    await click('.creator-tabs [data-tab=name]');
    await fill('.creator-panel input[type=text]', 'emberline');
    assert.equal(await door('d.draftName'), 'Emberline');
    await clickText('REVIEW CHARACTER');
    assert.equal(await door('d.page'), 'review');
    assert.match(await page.evaluate(`document.querySelector('.door-fields').textContent`), /Fire family/);
    await clickText('CONFIRM & CREATE');
    await waitDoor(`d.page === 'roster' && d.characters.length === 1 && !d.busy`);
    assert.equal(await door('d.characters[0].name'), 'Emberline');
    assert.match(await page.evaluate(`document.querySelector('.slot.selected').textContent`), /Fire family/);

    // The world list from central: the test world, its status asked of the world itself.
    await click('button[data-action=enter]');
    await waitDoor(`d.page === 'worlds' && !d.busy`);
    assert.equal(await door('d.worlds.length'), 1);
    assert.equal(await door('d.worlds[0].id'), h.world.id);
    await page.waitFor(`document.querySelector('.world-status')?.textContent.includes('of 100 wolves')`);
    assert.equal(await door('d.worlds[0].fit'), 'ok', 'same protocol and client-data hash as the world');

    // Join: a ticket, hello{ticket, protocol}, the world's first-arrival screen.
    await click(`button[data-world="${h.world.id}"]`);
    await waitDoor(`d.page === 'arrive'`, 30);
    const startsHere = await door('d.starts.length');
    assert.deepEqual(await page.evaluate(`[...document.querySelectorAll('.creator-tabs .tab')].map(b => b.dataset.tab)`),
        ['gift', 'strengths', ...(startsHere ? ['arrival'] : []), 'tie'], "the world's own choices (the demo world has no start towns)");

    assert.deepEqual(await door('d.tierChoices'), ['gifted', 'quickened'], 'a wolf with a family arrives Gifted or Quickened');
    assert.equal(await door('d.draftTier'), 'gifted');
    assert.equal(await door('d.arriveWolf.name'), 'Emberline');
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

    // Leave: back to the world list.
    await page.evaluate('window.ratw.game().state.leaveCharacter()');
    await waitDoor(`d.page === 'worlds' && !d.busy`, 20);
    assert.match(await message(), /left the world/);
    assert.equal(await page.evaluate('window.ratw.shell.inWorld'), false);

    // Rejoin, by the address box this time: straight in, no arrival screen.
    await fill('input[name=address]', `127.0.0.1:${h.world.port}`);
    await page.evaluate(`document.querySelector('form.world-direct').requestSubmit()`);
    await waitGame();
    assert.match(await page.evaluate(`JSON.stringify(window.ratw.game().state.snapshot.self)`), /Emberline/);

    // The world goes down: the list again, with Reconnect.
    await h.stopWorld();
    await waitDoor(`d.page === 'worlds'`, 30);
    assert.match(await message(), /connection to Client Test Hollow was lost/);
    assert.equal(await door('d.lostWorld'), h.world.id);
    assert.equal(await page.evaluate(`document.querySelector('button[data-world="${h.world.id}"]').textContent`), 'RECONNECT');
    await click(`button[data-world="${h.world.id}"]`);
    await waitDoor(`!d.busy && d.page === 'worlds'`, 30);
    assert.match(await message(), /Could not reach Client Test Hollow/, 'a new ticket, but nobody answers');

    // Delete the wolf (the password asked again, in the page's own dialog), then sign out and back in.
    await clickText('Back to your wolves');
    await click('button[data-action=delete-wolf]');
    await answerDialog('not my password at all');
    await waitDoor('!d.busy');
    assert.match(await message(), /password/i);
    assert.equal(await door('d.characters.length'), 1);
    await click('button[data-action=delete-wolf]');
    await answerDialog(PASSWORD);
    await waitDoor('!d.busy && d.characters.length === 0');
    assert.match(await message(), /Emberline is deleted/);
    await clickText('Sign out of account');
    await waitDoor(`d.page === 'login' && !d.busy`);
    await fill('input[name=username]', 'clientwolf');
    await fill('input[name=password]', PASSWORD);
    await page.evaluate(`document.querySelector('form.door-login').requestSubmit()`);
    await waitDoor(`d.page === 'roster' && !d.busy`);

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
