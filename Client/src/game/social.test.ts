// The individual social game on the page (Docs/Design/32-parties-chapters-factions.md, Part 1): a name about town as
// told, and notes and stars sent. The rules are the server's (Tests/social_game_tests.cpp).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {testGame} from './testing.ts';

test('social: a name about town, a note, a star', () => {
    const {state: s, commands} = testGame();
    s.receiveEvent({type: 'reputation', lines: ['Ridgemere: known to 3 residents; well liked']});
    assert.deepEqual(s.reputation, ['Ridgemere: known to 3 residents; well liked']);
    s.receiveEvent({type: 'inspect', id: 'npc_cook', title: 'The cook', regard: 'know you a little', note: ''});
    s.sendSocial({verb: 'note', target: 'npc_cook', text: 'Kind to strays.'});
    assert.deepEqual(commands.at(-1), {type: 'social', verb: 'note', target: 'npc_cook', text: 'Kind to strays.'});
    assert.equal(s.inspectedCharacter?.note, 'Kind to strays.', 'the open Look shows it at once');
    s.sendSocial({verb: 'star', session: 'scene-7', target: 'bo'});
    assert.deepEqual(commands.at(-1), {type: 'social', verb: 'star', session: 'scene-7', target: 'bo'});
});
