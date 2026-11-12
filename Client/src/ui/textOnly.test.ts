// The Editor's screens draw a world's words as text only (Docs/Design/78, 6.6 "Either way", I12): no markup sinks, no
// links or sources from data, no new windows, in the main window's Editor screens and their words. (Editor/src has its
// own, Editor/src/lib/textOnly.test.ts.)
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {join} from 'node:path';

const here = import.meta.dirname;
const BANNED: [RegExp, string][] = [
    [/\.innerHTML\b|\.outerHTML\b|insertAdjacentHTML|document\.write|DOMParser|createContextualFragment/, 'markup'],
    [/window\.open\s*\(|\bopen\s*\(\s*['"`]http/, 'a new window'],
    [/(?<!dataset)\.(href|src|srcdoc|action|formAction)\s*=|setAttribute\(\s*['"](href|src|style|on\w+)['"]/, 'a link, source or handler from data'],
    [/\bhref\s*:|\bsrc\s*:/, 'a link or source set from data'],
    [/\.style\b|cssText/, 'a style from data'],
    [/\beval\s*\(|new Function\s*\(/, 'code from text'],
];

for (const file of ['editorDoor.ts', '../net/editor.ts']) {
    test(`${file} draws text only`, () => {
        const source = readFileSync(join(here, file), 'utf8');
        source.split('\n').forEach((line, i) => {
            if (/^\s*(\/\/|\*)/.test(line)) return;
            for (const [pattern, what] of BANNED) assert.ok(!pattern.test(line), `${file}:${i + 1}: ${what}: ${line.trim()}`);
        });
    });
}

test('the front door has no markup sinks, and its Editor parts are the Editor door\'s', () => {
    const source = readFileSync(join(here, 'frontDoor.ts'), 'utf8');
    for (const [pattern, what] of BANNED.slice(0, 2)) assert.ok(!pattern.test(source), `frontDoor.ts: ${what}`);
    assert.ok(source.includes("new EditorDoor("), 'the Editor screens are drawn by editorDoor.ts');
});
