// Screenshots of a story scene's speaker panels in the real page (the user, 2026-11-20: "in scenes when anyone speaks
// it's in their own panel with their face"): Ash watches a ceremony in the browser, types a reply in the scene, and a
// second player's reply and the resident's answer join in their own panels. Driven by tools/test_story_scene_live.py
// (RATW_SCENE_SHOTS=DIR), which runs the world, the story and the other player; this file is the browser side.
//
//   node tools/client/scene-panels-shot.mjs PORT OUT     prints "asked", "replied" and "done" as it goes
import {mkdirSync} from 'node:fs';
import {Browser} from './browser.mjs';

const [port, out] = process.argv.slice(2);
mkdirSync(out, {recursive: true});
const say = word => process.stdout.write(word + '\n');
const browser = await Browser.launch({width: 1440, height: 900});
try {
    const page = await browser.open(`http://127.0.0.1:${port}/?identity=ash&name=Ash`);
    const scene = 'window.ratw.game()?.state.cinematic';
    await page.waitFor(`${scene}?.current?.beat === 'ask' && ${scene}.speeches().length >= 1`, 60);
    await new Promise(r => setTimeout(r, 900));
    await page.screenshot(`${out}/scene-1-the-resident-asks.png`);
    say('asked');
    await page.evaluate(`(() => {
        const input = document.querySelector('.cinematic-response input');
        input.value = 'I will keep it, with Fern beside me.';
        input.dispatchEvent(new Event('input'));
        document.querySelector('.cinematic-response').requestSubmit();
        return true;
    })()`);
    await page.waitFor(`${scene}.speeches().some(l => l.self)`, 20);
    say('replied');
    await page.waitFor(`${scene}?.current?.beat === 'close' && ${scene}.speeches().length >= 4`, 60);
    await new Promise(r => setTimeout(r, 900));
    await page.screenshot(`${out}/scene-2-three-speakers.png`);
    const shown = await page.evaluate(`JSON.stringify(${scene}.speeches().map(l => [l.kind, l.name, l.self, !!l.appearance, l.text]))`);
    process.stdout.write('panels ' + shown + '\n');
    say('done');
} finally {
    await browser.close();
}
