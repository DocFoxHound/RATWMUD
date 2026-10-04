// The Dev Console in the real client (Docs/Design/34-dungeon-master-refresh.md, 1.1b): in headless Chromium, a
// development identity signs in and says whether its page was told it is a Dungeon Master; with "console", it opens the
// console with the ` key, runs /fight-test-1 typed into it, sees the fight, and ends it with /fight-end-myself.
// Prints one line of JSON. Run by tools/test_live_map.py.
//
//   node tools/client/devconsole.mjs PORT [console] [SCREENSHOT.png]
import {Browser} from './browser.mjs';

const [port, mode, shot] = process.argv.slice(2);
const browser = await Browser.launch({width: 1600, height: 1000});
const out = {};
try {
    const page = await browser.open(`http://127.0.0.1:${port}/?identity=ash&name=Ash`);
    await page.waitFor('window.ratw.game()?.state.snapshot?.self');
    out.dungeonMaster = await page.evaluate('!!window.ratw.game().state.snapshot.self.dungeonMaster');
    const button = `getComputedStyle(document.querySelector('.top.dev')).display !== 'none'`;
    await page.waitFor(`document.querySelector('.top.dev') && window.ratw.game().state.clock > 1`, 10);
    if (mode === 'console') {
        out.button = !!(await page.waitFor(button, 5).catch(() => false));
        await page.key('Backquote');
        out.console = await page.waitFor(`window.ratw.game().state.devConsole
            && document.activeElement?.classList.contains('dev-console-input')
            && document.querySelectorAll('.dev-console-suggestion b').length >= 3`, 5).catch(() => false);
        const shown = () => page.evaluate(`[...document.querySelectorAll('.dev-console-suggestion b')].map(b => b.textContent)`);
        out.all = await shown();
        await page.type('/fi');
        await new Promise(r => setTimeout(r, 300));
        out.fits = await shown();
        await page.type('ght-t');
        await page.key('Tab');
        out.completed = await page.evaluate(`document.querySelector('.dev-console-input').value`);
        await page.key('Enter');
        out.fight = !!(await page.waitFor('window.ratw.game().state.battle', 10).catch(() => null));
        await new Promise(r => setTimeout(r, 1500));
        if (shot) await page.screenshot(shot);
        await page.type('/fight-end-myself');
        await page.key('Enter');
        out.ended = await page.waitFor('!window.ratw.game().state.battle', 20).then(() => true).catch(() => false);
        out.log = await page.evaluate('window.ratw.game().state.devLog.map(e => [e.command, e.ok, e.text])');
        await page.key('Escape');
        out.closed = await page.waitFor('!window.ratw.game().state.devConsole', 3).then(() => true).catch(() => false);
    } else
        out.button = await page.evaluate(button);
} catch (error) {
    out.error = String(error);
} finally {
    await browser.close();
}
console.log(JSON.stringify(out));
