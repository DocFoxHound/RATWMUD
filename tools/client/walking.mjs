// The page walks its own wolf (Docs/Design/31-responsiveness.md, Phase 3): in headless Chromium, the real client is
// opened, a key is held, and the wolf must move on the page at once, its poses accepted by the server (none
// corrected), and the server's own position must follow. Run by tools/walking_smoke.py.
//
//   node tools/client/walking.mjs PORT
import {Browser} from './browser.mjs';

const port = process.argv[2];
const browser = await Browser.launch({width: 1600, height: 1000});
let failed = '';
try {
    const page = await browser.open(`http://127.0.0.1:${port}/?identity=ash&name=Ash`);
    await page.waitFor('window.ratw.game()?.state.snapshot && window.ratw.game().state.walker');
    await page.waitFor('window.ratw.game().state.freeWalking()', 10);
    const where = () => page.evaluate(`(() => { const s = window.ratw.game().state, me = s.entities.get(s.selfId);
        return {x: me.x, y: me.y, server: s.snapshot.self ? s.snapshot.self.x : null, mode: s.movementMode}; })()`);
    const before = await where();
    // Which way is open: try each, keep the one that moves.
    let moved = null;
    for (const key of ['KeyD', 'KeyA', 'KeyS', 'KeyW']) {
        await page.key(key, 700);
        const after = await where();
        if (Math.hypot(after.x - before.x, after.y - before.y) > 0.5) { moved = {key, after}; break; }
    }
    if (!moved) failed = 'the wolf did not move on the page';
    else {
        await page.waitFor('true', 1);
        const settled = await page.evaluate(`new Promise(r => setTimeout(() => { const s = window.ratw.game().state, me = s.entities.get(s.selfId);
            r({x: me.x, y: me.y, sx: s.snapshot.self.x, sy: s.snapshot.self.y, mode: s.movementMode,
               corrections: s.corrections}); }, 800))`);
        const gap = Math.hypot(settled.x - settled.sx, settled.y - settled.sy);
        if (settled.mode !== 0) failed = `not free movement (mode ${settled.mode})`;
        else if (settled.corrections) failed = `${settled.corrections} pose(s) refused by the server`;
        else if (gap > 0.05) failed = `the server's position did not follow the page's (${gap.toFixed(3)} tiles apart)`;
        else console.log(`PASS: walked ${moved.key} on the page; the server followed to within ${gap.toFixed(3)} tiles`);
    }
} catch (error) {
    failed = String(error);
} finally {
    await browser.close();
}
if (failed) {
    console.log(`FAIL: ${failed}`);
    process.exit(1);
}
