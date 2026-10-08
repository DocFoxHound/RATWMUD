// Letters on the page (Docs/Design/55-letters-gifts-favours.md, Phase 1): opening the case asks for it; the case as
// sent is kept; a letter arriving is told and refreshes an open case; the commands; letters counted as a reader counts
// them. The rules are the server's (Tests/letters_tests.cpp).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {testGame} from './testing.ts';
import {lettersIn} from '../ui/hud/letters.ts';

test('letters: the case, an arrival, and the commands', () => {
    const {state: s, commands} = testGame();
    s.openLetters();
    assert.equal(s.modal, 'letters');
    assert.deepEqual(commands.at(-1), {type: 'letters'}, 'opening the case asks for it');
    s.receiveEvent({type: 'letters', letters: [{id: 'doc-1', sign: 'Ash', scent: 'It smells of Ash.', read: false}], waiting: [], canWrite: true});
    assert.equal(s.lettersCase?.letters instanceof Array, true, 'the case kept');
    const before = commands.length;
    s.receiveEvent({type: 'letterArrived', id: 'doc-2'});
    assert.deepEqual(commands.slice(before), [{type: 'letters'}], 'an arrival refreshes the open case');
    s.sendLetter('write', {to: 'Bo', text: 'Hello.', sign: ''});
    assert.deepEqual(commands.at(-1), {type: 'letter', verb: 'write', to: 'Bo', text: 'Hello.', sign: ''});
    assert.equal(lettersIn('héllo'), 5, 'letters, not bytes');
});
