// The Quickened fanfare in the world (Docs/Design/49-characters-and-earned-gifts.md, 9.1): the world's "fanfare" event
// is kept as a banner for the middle of the screen until it is closed (ui/hud/hud.ts); the server's side is
// Tests/ticket_tests.cpp (aWolfsQuickenedComesFromItsTicket).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {testGame} from './testing.ts';

test('a fanfare is kept until it is closed', () => {
    const {state: s} = testGame();
    assert.equal(s.fanfare, null);
    s.receiveEvent({type: 'fanfare', kind: 'quickened', kicker: 'QUICKENED IS OPEN', title: 'Your name runs ahead of you.',
        text: 'Quickened is open to your whole account, in every world.', everywhere: true});
    assert.deepEqual(s.fanfare, {kind: 'quickened', kicker: 'QUICKENED IS OPEN', title: 'Your name runs ahead of you.',
        text: 'Quickened is open to your whole account, in every world.'});
    s.clock += 600;
    assert.ok(s.fanfare, 'it stays: a banner, not a toast');
    s.fanfare = null;                               // (Its button: hud.ts.)
    s.receiveEvent({type: 'fanfare', kind: 'quickened'});
    const shown = () => s.fanfare;                  // (Read again: TypeScript took it for null after the line above.)
    assert.equal(shown()?.title, 'Your name runs ahead of you.', 'with words of its own when the world sends none');
});
