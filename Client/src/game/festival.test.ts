// Festivals on the page (Docs/Design/54-gathering-places.md, Phase 6): the programme's lines. The rules are the
// server's (Tests/gathering_tests.cpp).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {programmeLine} from '../ui/hud/festival.ts';

test('programme lines', () => {
    assert.equal(programmeLine({hour: 13, what: 'Races', state: 'open'}), '13:00 Races · open');
    assert.equal(programmeLine({hour: 15, what: 'Howling', state: 'done', winner: 'You'}), '15:00 Howling · won by You');
    assert.equal(programmeLine({hour: 19, what: 'The storytelling contest', state: 'done', winner: 'none'}), '19:00 The storytelling contest · no winner');
    assert.equal(programmeLine({hour: 20, what: 'The crier', state: ''}), '20:00 The crier');
});
