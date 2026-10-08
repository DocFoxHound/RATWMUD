import {test} from 'node:test';
import assert from 'node:assert/strict';
import {boardRows, dieFace, knucklebonesLine, nextBid, nextRound, targetsFrom} from './tableGames.ts';

test('knucklebones lines', () => {
    assert.equal(knucklebonesLine(0, 0), 'not begun');
    assert.equal(knucklebonesLine(2, 3), 'through the twos · this turn the threes');
    assert.equal(nextRound(2), 'the threes');
    assert.equal(nextRound(5), '');
});

test('the wolves and deer board', () => {
    const points = '  W.W    ...    ...  .......DDDDDDD  DDD    DDD  ';
    const rows = boardRows(points);
    assert.equal(rows.length, 7);
    assert.deepEqual(rows[0], [' ', ' ', 'W', '.', 'W', ' ', ' ']);
    assert.equal(rows[4].join(''), 'DDDDDDD');
    assert.deepEqual(targetsFrom([[2, 9], [4, 11], [2, 3]], 2), [9, 3]);
});

test('liar\'s bones bids and faces', () => {
    assert.equal(dieFace(3), '⚂');
    assert.equal(dieFace(9), '?');
    assert.deepEqual(nextBid(0, 0, 4, 10), [1, 4]);
    assert.deepEqual(nextBid(2, 3, 5, 10), [2, 5]);
    assert.deepEqual(nextBid(2, 5, 3, 10), [3, 3]);
    assert.equal(nextBid(10, 6, 2, 10), null);
});
