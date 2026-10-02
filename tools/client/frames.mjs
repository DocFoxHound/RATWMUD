// Walks a wolf around in the real browser client and reports how smoothly the frames came (Docs/Design/29).
//
//   node tools/client/frames.mjs http://127.0.0.1:7788/ [seconds] [identity]
//
// Prints one JSON line: the game's work per frame (p50, p99, worst, mean, in milliseconds), long frames as the browser
// reports them, frame intervals, and how often the map's ground was redrawn. Exits 1 when the work's p99 is over 8 ms,
// its worst over 25 ms, or any frame over 100 ms (RATW_FRAMES_LENIENT=1 only reports).
import {Browser} from './browser.mjs';

const [url, seconds = '20', identity = 'frames'] = process.argv.slice(2);
if (!url) {
    console.error('usage: node tools/client/frames.mjs URL [seconds] [identity]');
    process.exit(2);
}
const browser = await Browser.launch({width: 1600, height: 1000});
let code = 0;
try {
    const page = await browser.open(`${url}${url.includes('?') ? '&' : '?'}identity=${identity}&name=${identity}`);
    await page.waitFor('window.ratw && window.ratw.game() && window.ratw.game().state.snapshot', 30);
    await new Promise(resolve => setTimeout(resolve, 1500));          // Fonts, weather sheets, the first ground.
    // Frame intervals measured by the page itself, independent of the game's own counters (so older builds compare).
    // Headless Chromium paints without a GPU and caps its frame rate, so the gaps between frames say little about the
    // game. What does: the page's own work in each frame (every animation-frame callback, timed), and frames the
    // browser itself reports as long (the Long Animation Frames API: everything the main thread did in that frame).
    await page.evaluate(`window.frameGaps = []; window.frameWork = []; window.longFrames = [];
        (() => { let last = 0; const step = t => { if (last) window.frameGaps.push(t - last); last = t; requestAnimationFrame(step); };
        requestAnimationFrame(step);
        const raf = window.requestAnimationFrame.bind(window);
        window.requestAnimationFrame = f => raf(t => { const s = performance.now(); f(t); window.frameWork.push(performance.now() - s); });
        try { new PerformanceObserver(list => { for (const e of list.getEntries()) window.longFrames.push(e.duration); })
            .observe({type: 'long-animation-frame', buffered: false}); } catch {} })()`);
    await page.evaluate('window.ratw.game().frames?.reset()');
    // A walk that turns often, so new ground keeps coming into view: east, south, west, north, and diagonals.
    const legs = [['KeyD'], ['KeyS'], ['KeyA'], ['KeyW'], ['KeyD', 'KeyS'], ['KeyA', 'KeyW']];
    const until = Date.now() + Number(seconds) * 1000;
    for (let i = 0; Date.now() < until; ++i) {
        const keys = legs[i % legs.length];
        for (const k of keys) await page.keyEvent('keyDown', k);
        await new Promise(resolve => setTimeout(resolve, 1800));
        for (const k of keys) await page.keyEvent('keyUp', k);
    }
    const result = await page.evaluate(`(() => { const g = window.ratw.game();
        const gaps = window.frameGaps.slice().sort((a, b) => a - b), n = gaps.length;
        const at = q => (n ? gaps[Math.min(n - 1, Math.floor(q * n))] : 0);
        const work = window.frameWork.slice().sort((a, b) => a - b), w = work.length;
        const wat = q => (w ? work[Math.min(w - 1, Math.floor(q * w))] : 0);
        const long = window.longFrames;
        return {frames: {p50: at(0.5), p99: at(0.99), worst: n ? gaps[n - 1] : 0, frames: n},
                work: {p50: wat(0.5), p99: wat(0.99), worst: w ? work[w - 1] : 0, mean: w ? work.reduce((a, b) => a + b, 0) / w : 0},
                longFrames: {count: long.length, worst: long.length ? Math.max(...long) : 0},
                draw: g.frames?.summary('work'), groundRedrawn: g.groundRedrawn?.(),
                cell: g.state.cellName, entities: g.state.entities.size};
    })()`);
    // The budget: the game's own work stays well inside a 60 Hz frame, and no frame stalls.
    const ok = result.work.p99 <= 8 && result.work.worst <= 25 && result.longFrames.worst <= 100;
    console.log(JSON.stringify({ok, ...result}));
    if (!ok && !process.env.RATW_FRAMES_LENIENT) code = 1;
    if (page.console.some(line => line.startsWith('EXCEPTION'))) {
        console.error(page.console.filter(line => line.startsWith('EXCEPTION')).join('\n'));
        code = 1;
    }
} finally {
    browser.close();
}
process.exit(code);
