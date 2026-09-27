import test from 'node:test';
import assert from 'node:assert/strict';
import {TERRAIN} from '../model/model.mjs';
import {GLYPHS, GLYPH_GROUPS, HOTKEYS, QUICK_GLYPHS, glyphInfo, glyphLabel, hotkeyGlyph, matchesGlyph} from './glyphs.ts';
import {drawnGlyph, textPresentation} from './glyphFont.ts';

test('the palette is the catalog, grouped by category in catalog order', () => {
    assert.equal(GLYPHS.length, TERRAIN.length);
    assert.deepEqual(GLYPH_GROUPS.flatMap(g => g.tiles.map(t => t.code)).sort(), TERRAIN.map(t => t.code).sort());
    assert.deepEqual(GLYPH_GROUPS.map(g => g.id), ['ground', 'nature', 'elevation', 'structure', 'furniture', 'civic']);
    for (const g of GLYPH_GROUPS) assert.ok(g.tiles.length > 0, g.id);
    for (const code of Object.keys(HOTKEYS)) assert.ok(TERRAIN.some(t => t.code === code), `hotkey for unknown code ${code}`);
    assert.equal(new Set(Object.values(HOTKEYS)).size, Object.keys(HOTKEYS).length);
});

test('hotkeys: 1–0 and - keep the original eleven tiles; Shift picks a second set', () => {
    const key = (code: string, shiftKey = false) => hotkeyGlyph({code, shiftKey})?.code;
    assert.deepEqual(['Digit1', 'Digit2', 'Digit3', 'Digit4', 'Digit5', 'Digit6', 'Digit7', 'Digit8', 'Digit9', 'Digit0', 'Minus'].map(c => key(c)),
        ['.', ',', '"', '#', 'T', '=', '~', ':', '^', '+', '%']);
    assert.equal(key('Numpad4'), '#');
    assert.equal(key('Digit1', true), 'd');
    assert.equal(key('KeyA'), undefined);
    assert.deepEqual(QUICK_GLYPHS.map(g => g.code).join(''), '.,"#T=~:^+%');
});

test('tile lookup, labels and the palette filter', () => {
    assert.equal(glyphInfo('k').name, 'Bookshelf');
    assert.equal(glyphInfo('?').code, TERRAIN[0].code);           // Unknown codes read as the first tile.
    assert.equal(glyphLabel(glyphInfo('^')), 'Stairs (^)');
    const names = (q: string) => GLYPHS.filter(g => matchesGlyph(g, q)).map(g => g.code);
    assert.deepEqual(names('bookshelf'), ['k']);
    assert.ok(names('wall').includes('#') && names('wall').includes('H'));
    assert.ok(names('sleep').includes('b'));                     // Matches the effect.
    assert.ok(names('Furniture').includes('T'));                 // Matches the category.
    assert.deepEqual(names('%'), ['%']);                           // Matches the code exactly.
    assert.equal(names('').length, GLYPHS.length);
});

test('drawn glyphs: Unicode with text presentation for symbols, or plain ASCII', () => {
    const hearth = glyphInfo('h'), wall = glyphInfo('#');
    assert.equal(drawnGlyph(hearth, false), '♨︎');
    assert.equal(drawnGlyph(hearth, true), '&');
    assert.equal(drawnGlyph(wall, false), '▓');
    assert.equal(drawnGlyph(wall, true), '#');
    assert.equal(textPresentation('≈'), '≈');
    for (const t of TERRAIN) assert.equal(drawnGlyph(t, true), t.ascii);
});
