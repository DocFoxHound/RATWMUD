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

test('social: a scene says what pay still needs, and warns when it goes quiet', async () => {
    const {sceneNeedsLabel, sceneQuietLabel} = await import('./labels.ts');
    assert.equal(sceneNeedsLabel({needTurns: 1, needWords: 15, needReply: true, othersShaped: 1}),
        'To be paid: 1 more line of five words or more · 15 more words · answer someone');
    assert.equal(sceneNeedsLabel({needTurns: 0, needWords: 0, needReply: false, othersShaped: 0}),
        "You've said enough: waiting for another to say as much");
    assert.equal(sceneNeedsLabel({needTurns: 0, needWords: 0, needReply: false, othersShaped: 2}), 'On track to be paid');
    assert.equal(sceneNeedsLabel({fight: true, needTurns: 2, needWords: 35, needReply: true, othersShaped: 0}),
        'To be paid twice: 2 more lines of five words or more · 35 more words · answer someone');
    assert.equal(sceneQuietLabel({quiet: false, endsIn: 1700}), '', 'not quiet: nothing');
    assert.equal(sceneQuietLabel({quiet: true, endsIn: 841}), 'Quiet · ends in 15 min unless someone speaks');
    assert.equal(sceneQuietLabel({quiet: true, endsIn: 0}), 'Quiet · ends in 1 min unless someone speaks', 'never "0 min"');
    assert.equal(sceneQuietLabel({quiet: true, fight: true, endsIn: 100}), '', "a fight's scene ends with the fight");
    const {state: s, commands} = testGame();
    s.sendSocial({verb: 'leave', session: 'scene-7'});
    assert.deepEqual(commands.at(-1), {type: 'social', verb: 'leave', session: 'scene-7'});
});
