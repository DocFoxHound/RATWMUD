// Everything a world sends is shown as text (Docs/Design/78, 6.6 "Either way"; I12): in hosted builds the studio is a
// world's page in the client's Editor window, and the world's own strings (names, notices, health reports) must never
// become markup, links, sources or windows.
//
// - Never: innerHTML, dangerouslySetInnerHTML, outerHTML, insertAdjacentHTML, document.write, eval, new Function.
// - window.open and a link or source made from data only where hosted mode hides it (the line says `hosted`), or on the
//   short list below with its reason.
// - Styles: React sets them through the CSSOM (a value can't add a property), and the shell's CSP (default-src 'self',
//   img-src 'self' blob: data:) refuses every load from elsewhere, so data-derived colours stay allowed; style markup and
//   cssText do not.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readdirSync, readFileSync, statSync} from 'node:fs';
import {join, relative} from 'node:path';

const SRC = join(import.meta.dirname, '..');
const NEVER = /\.innerHTML\b|dangerouslySetInnerHTML|\.outerHTML\b|insertAdjacentHTML|document\.write|\beval\s*\(|new Function\s*\(|cssText|style="/;
const GUARDED = /window\.open\s*\(|\bhref=\{|\bsrc=\{|\.href\s*=|\.src\s*=|location\.(assign|replace|href\s*=)/;
// file:line text that may stay, and why.
const ALLOWED: [string, RegExp, string][] = [
    ['dm/ArtworkPanel.tsx', /src=\{`data:image\/png;base64,/, 'a data: PNG (a portrait to review): an image, never script'],
    ['lib/hosted.ts', /window\.location\.assign\(`\?\$\{query\}`\)/, 'the studio\'s own suite switch, outside hosted mode only'],
];

function files(dir: string): string[] {
    return readdirSync(dir).flatMap(name => {
        const path = join(dir, name);
        if (statSync(path).isDirectory()) return files(path);
        return /\.(tsx?|mts)$/.test(name) && !/\.test\./.test(name) ? [path] : [];
    });
}

test('the studio draws a world\'s words as text only', () => {
    const problems: string[] = [];
    for (const path of files(SRC)) {
        const name = relative(SRC, path).replaceAll('\\', '/');
        readFileSync(path, 'utf8').split('\n').forEach((line, i) => {
            if (/^\s*(\/\/|\*)/.test(line)) return;
            if (NEVER.test(line)) problems.push(`${name}:${i + 1}: never: ${line.trim()}`);
            if (GUARDED.test(line) && !/\bhosted\b/.test(line) && !ALLOWED.some(([f, p]) => f === name && p.test(line)))
                problems.push(`${name}:${i + 1}: a link, source or window from data, not hidden in hosted builds: ${line.trim()}`);
        });
    }
    assert.deepEqual(problems, []);
});

test('hosted mode is the editor scheme\'s origin, and nothing else', async () => {
    const at = async (href: string) => {
        const url = new URL(href);
        (globalThis as {location?: unknown}).location = {protocol: url.protocol, hostname: url.hostname};
        const m = await import(`./hosted.ts?${encodeURIComponent(href)}`);
        return m.hosted as boolean;
    };
    assert.equal(await at('editor://localhost/?suite=dm'), true);
    assert.equal(await at('http://editor.localhost/?suite=dm'), true);
    assert.equal(await at('http://127.0.0.1:8765/'), false);
    assert.equal(await at('http://localhost:5173/'), false);
    delete (globalThis as {location?: unknown}).location;
});
