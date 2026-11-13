// The screen (Docs/Design/80-the-whole-screen.md): the page's rectangle is the server's (Core/RatwScreen.h), and
// everything in it is shaded as seen.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {test} from 'node:test';
import {ScreenHalfHeight, ScreenHalfWidth, ScreenMargin, onScreen, screenRect, screenTile, shadeRow} from './screen.ts';
import {ZoomTiles} from './paint.ts';

test('the page and the server agree on the screen', () => {
    const header = readFileSync(new URL('../../../Core/RatwScreen.h', import.meta.url), 'utf8');
    const constant = (name: string) => Number(new RegExp(`constexpr int ${name} = (\\d+);`).exec(header)?.[1]);
    assert.equal(constant('HalfWidth'), ScreenHalfWidth);
    assert.equal(constant('HalfHeight'), ScreenHalfHeight);
});

test('the screen is a rectangle round the wolf, kept on the ground there is', () => {
    assert.deepEqual(screenRect(200.5, 150.5, 400, 300, 0), {x0: 104, y0: 94, x1: 296, y1: 206});
    assert.deepEqual(screenRect(3.5, 2.5, 400, 300, 0), {x0: 0, y0: 0, x1: 192, y1: 112});       // By an edge: as wide as ever.
    assert.deepEqual(screenRect(3.5, 2.5, 400, 300, 48), {x0: -48, y0: -48, x1: 144, y1: 64});   // Over the band round the cell.
    assert.deepEqual(screenRect(398.5, 298.5, 400, 300, 0), {x0: 207, y0: 187, x1: 399, y1: 299});
    assert.deepEqual(screenRect(5.5, 5.5, 32, 24, 0), {x0: 0, y0: 0, x1: 31, y1: 23});           // A small place, whole.
    const r = screenRect(200.5, 150.5, 400, 300, 0);
    assert.ok(onScreen(r, 104, 94) && onScreen(r, 296, 206) && !onScreen(r, 103, 150) && !onScreen(r, 200, 207));
});

test('the map never shows more than the screen', () => {
    // At the widest zoom a map up to 2576 x 1456 pixels shows what the zoom gives; a larger one gets larger tiles.
    const widest = ZoomTiles[0];
    assert.ok(screenTile(2560, 1440) <= widest);
    assert.ok(screenTile(3840, 1440) > widest);
    for (const [w, h] of [[1280, 720], [2560, 1440], [3840, 2160], [5120, 1440]]) {
        const tile = Math.max(widest, screenTile(w, h));
        assert.ok(w / tile <= 2 * (ScreenHalfWidth - ScreenMargin) && h / tile <= 2 * (ScreenHalfHeight - ScreenMargin));
    }
});

test('a row is shaded as seen on the screen, remembered off it, unknown where blank', () => {
    assert.equal(shadeRow('..# ,,..', 2, 5), '112022' + '11');
    assert.equal(shadeRow('..# ,,..', 0, -1), '11101111');       // A row off the screen.
    assert.equal(shadeRow('ab', -5, 40), '22');
    assert.equal(shadeRow('ab', 5, 9), '11');
});
