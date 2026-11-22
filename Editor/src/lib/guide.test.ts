// The Story Maker's guide as the Editor shows it (doc 85, Phase 10): Docs/Story-Maker-guide.md read as its Markdown, every
// link of its contents leading to a heading, every event, condition, action and effect of the catalogue a unit of its own
// that a search finds, and the inline parts.
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {dirname, join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {contentsOf, inline, parseGuide, searchGuide, slugOf, unitsOf} from './guide.ts';

const repo = join(dirname(fileURLToPath(import.meta.url)), '../../..');
const text = readFileSync(join(repo, 'Docs/Story-Maker-guide.md'), 'utf8');
const cat = JSON.parse(readFileSync(join(repo, 'Data/Storylines/trigger-catalogue.json'), 'utf8'));
const blocks = parseGuide(text);
const units = unitsOf(blocks);

test('the guide reads as headings, paragraphs, lists, tables and code', () => {
    const kinds = new Set(blocks.map(b => b.type));
    assert.deepEqual([...kinds].sort(), ['code', 'h', 'list', 'p', 'table']);
    const contents = contentsOf(blocks);
    assert.ok(contents.some(c => c.text === '2. A small quest in five minutes'));
    assert.ok(contents.some(c => c.text.startsWith('Reference')));
    // (Every link to a place in the guide leads to a heading or an entry.)
    const ids = new Set([...blocks.flatMap(b => b.type === 'h' ? [b.id] : b.type === 'p' && b.anchor ? [b.anchor] : [])]);
    const links = [...text.matchAll(/\]\(#([^)]+)\)/g)].map(m => m[1]);
    assert.ok(links.length >= 17);
    assert.deepEqual(links.filter(l => !ids.has(l)), []);
    assert.equal(slugOf("8. The world's facts and watched events"), '8-the-worlds-facts-and-watched-events');
});

test('every event, condition, action and effect is a unit of its own that a search finds', () => {
    const anchors = new Set(units.map(u => u.id));
    const slug = (s: string) => s.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
    for (const [kind, list] of [['event', cat.events], ['condition', cat.conditions], ['action', cat.actions], ['effect', cat.effects]] as const)
        for (const item of list as {id: string}[]) assert.ok(anchors.has(`${kind}-${slug(item.id)}`), `${kind} ${item.id}`);
    const found = searchGuide(units, 'story.start');
    assert.ok(found.some(u => u.id === 'effect-story-start') && found.some(u => u.section.startsWith('4. Building a campaign')));
    assert.ok(searchGuide(units, 'purse crossed').some(u => u.id === 'event-purse-crossed'));
    assert.equal(searchGuide(units, '').length, units.length);
    const entry = units.find(u => u.id === 'effect-story-start')!;
    assert.ok(entry.blocks.some(b => b.type === 'code' && b.text.includes('"kind": "story.start"')), 'with its example');
});

test('inline parts', () => {
    assert.deepEqual(inline('A **bold** `code` and *words* [there](#here).'), [
        {t: 'text', s: 'A '}, {t: 'b', s: 'bold'}, {t: 'text', s: ' '}, {t: 'code', s: 'code'}, {t: 'text', s: ' and '}, {t: 'i', s: 'words'},
        {t: 'text', s: ' '}, {t: 'a', s: 'there', href: '#here'}, {t: 'text', s: '.'}]);
});
