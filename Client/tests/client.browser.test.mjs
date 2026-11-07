// The desktop client's flow (tests/flow.mjs), the whole way through, in headless Chromium (Docs/Design/77, Phase 3): the built page with
// ?central= against a real central and a real ticket-mode world on this computer (tests/harness.mjs). Register (the
// age gate), make a wolf with a Gift family (the identity tabs only), the world list from central, join, the world's
// first-arrival screen (tier, strengths, town, tie), in the world, leave back to the list, rejoin straight in (by the
// address box), the world going down (Reconnect), delete the wolf, sign out, sign in, delete the account.
//
//   npm --prefix Client run build && npm --prefix Client run test:browser
// Skipped (with the reason) where there is no Postgres or no built ratw_server/ratw_identity.
import {test, before, after} from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFileSync, existsSync} from 'node:fs';
import {join, extname} from 'node:path';
import {Browser} from '../../tools/client/browser.mjs';
import {startHarness} from './harness.mjs';
import {clientFlow} from './flow.mjs';

const dist = join(import.meta.dirname, '..', 'dist');
const types = {'.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.png': 'image/png', '.ttf': 'font/ttf', '.wasm': 'application/wasm'};
let server, browser, page, h, skip = '';

before(async () => {
    assert.ok(existsSync(join(dist, 'index.html')), 'Build the client first: npm --prefix Client run build');
    server = createServer((request, response) => {
        const wanted = request.url.split('?')[0];
        const path = join(dist, wanted === '/' ? 'index.html' : wanted);
        if (!path.startsWith(dist) || !existsSync(path) || !extname(path)) return response.writeHead(404).end();

        response.writeHead(200, {'Content-Type': types[extname(path)] ?? 'application/octet-stream'}).end(readFileSync(path));
    });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    const origin = `http://127.0.0.1:${server.address().port}`;
    h = await startHarness({cors: origin});
    if (h.skip) {
        skip = h.skip;
        return;
    }
    browser = await Browser.launch({width: 1440, height: 940});
    page = await browser.open(`${origin}/?central=${encodeURIComponent(h.central)}`);
    await page.waitFor('window.ratw && window.ratw.client && window.ratw.door()');
}, {timeout: 180000});

after(async () => {
    await browser?.close();
    server?.close();
    if (h && !h.skip) await h.stop();
});

test('the client: register, a wolf, a world, arrive, leave, rejoin, lose the world, delete', {timeout: 240000}, async t => {
    if (skip) return t.skip(skip);
    await clientFlow(page, h);
    assert.deepEqual(page.console.filter(line => line.startsWith('EXCEPTION')), [], 'no exceptions in the page');
});
