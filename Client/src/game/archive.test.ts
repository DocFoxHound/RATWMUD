// The archive on the page (Docs/Design/54-gathering-places.md, Phase 7): moving records in the sorting sheet. The order
// is checked by the server (Tests/gathering_tests.cpp).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {moveRecord} from '../ui/hud/archive.ts';

test('moving records', () => {
    assert.deepEqual(moveRecord(['a', 'b', 'c'], 1, -1), ['b', 'a', 'c']);
    assert.deepEqual(moveRecord(['a', 'b', 'c'], 1, 1), ['a', 'c', 'b']);
    assert.deepEqual(moveRecord(['a', 'b', 'c'], 0, -1), ['a', 'b', 'c']);
    assert.deepEqual(moveRecord(['a', 'b', 'c'], 2, 1), ['a', 'b', 'c']);
});
