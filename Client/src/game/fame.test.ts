// Fame on the page (Docs/Design/56-fame-and-memory.md, Phase 3): the sheet's nickname lines. The rules are the server's
// (Tests/fame_tests.cpp).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {nicknameLine, titleLine, unfinishedLine} from './fame.ts';

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

test('title lines (doc 86, C4)', () => {
    assert.equal(titleLine({title: 'the Lamp of the Ford', known: 'realm', days: -1}), 'the Lamp of the Ford · known in the realm');
    assert.equal(titleLine({title: 'Warden of the Weir', known: 'town', town: 'Greyfen', days: 5}), 'Warden of the Weir · known in Greyfen · 5 days left');
    assert.equal(titleLine({title: 'Warden of the Weir', known: 'town', town: 'Greyfen', days: 1}), 'Warden of the Weir · known in Greyfen · 1 day left');
    assert.equal(titleLine({title: 'the Lamp', known: 'realm', days: 0}), 'the Lamp · known in the realm · its last day');
});
