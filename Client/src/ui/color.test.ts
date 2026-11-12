// Colours in words (ui/color.ts, colourWords): the creator's review never shows a colour code.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {colourWords} from './color.ts';

test('a coat or eye colour is named, never shown as its code', () => {
    assert.equal(colourWords('#f4efe6'), 'snow white');
    assert.equal(colourWords('#303534'), 'charcoal');
    assert.equal(colourWords('#b5733f'), 'rust');
    assert.equal(colourWords('#8a9a3a', 'eyes'), 'olive green');
    assert.equal(colourWords('#5f9fd0', 'eyes'), 'blue');
    assert.equal(colourWords('#D9A441', 'eyes'), 'amber');
    assert.equal(colourWords('amber', 'eyes'), 'amber', 'words stay words');
    // Any colour the picker allows gets a name.
    for (const hex of ['#000000', '#ffffff', '#ff00ff', '#123456']) assert.match(colourWords(hex), /^[a-z -]+$/);
});
