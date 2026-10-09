// Storytellers' commands in the box where one writes (Docs/Design/58-player-storytellers.md, 5): never said aloud.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {storyCommand, storytellerNote} from '../ui/hud/storyteller.ts';

test('the slash commands become the storyteller\'s verbs', () => {
    assert.deepEqual(storyCommand('/narrate The bell tolls under the water.'), {type: 'storyteller', verb: 'narrate', text: 'The bell tolls under the water.'});
    assert.deepEqual(storyCommand('/npc the ferryman: "Two pennies, or swim."'), {type: 'storyteller', verb: 'npc', name: 'the ferryman', text: 'Two pennies, or swim.'});
    assert.deepEqual(storyCommand('/roll 2d6+1 for the crossing'), {type: 'storyteller', verb: 'roll', dice: '2d6+1', for: 'the crossing'});
    assert.deepEqual(storyCommand('/tick 2.1'), {type: 'storyteller', verb: 'tick', step: 1, objective: 0});
    assert.equal(storyCommand('/me sits'), null);
    assert.equal(storyCommand('narrate this'), null);
});

test('the sheet says where one stands', () => {
    assert.equal(storytellerNote({state: 'applied'}), 'Your application to tell stories is with the Dungeon Masters.');
    assert.equal(storytellerNote({state: 'refused', reason: 'Not yet'}), 'Your application was refused: Not yet');
    assert.equal(storytellerNote(null), '');
});
