// Fame on the page (Docs/Design/56-fame-and-memory.md, Phase 3): the sheet's nickname lines. The rules are the server's
// (Tests/fame_tests.cpp).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {nicknameLine, unfinishedLine} from './fame.ts';

test('nickname lines', () => {
    assert.equal(nicknameLine({text: 'the Lantern', coinedBy: 'The wolf keeping the stall', town: 'Upper Accord', dropped: false}),
        'the Lantern · first said by the wolf keeping the stall, in Upper Accord');
    assert.equal(nicknameLine({text: 'Fleet-Foot', coinedBy: 'Wren', town: 'Ridgemere', dropped: true}), 'Fleet-Foot (you asked folk not to use it)');
});

test('unfinished lines', () => {
    assert.equal(unfinishedLine({text: 'You promised the miller: "honey"', days: 2.2}), 'You promised the miller: "honey" · due in 2 days');
    assert.equal(unfinishedLine({text: 'Work for Wren', days: -0.5}), 'Work for Wren · overdue');
    assert.equal(unfinishedLine({text: '2 letters unread in your case.'}), '2 letters unread in your case.');
});
