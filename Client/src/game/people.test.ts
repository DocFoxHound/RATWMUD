// Friends and private messages on the page (Docs/Design/50-player-card-friends-safety.md, Phase 3).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {testGame} from './testing.ts';

const friends = {type: 'friends', friends: [{handle: 'Bobbin', online: true, character: 'Bo One', shares: true},
    {handle: 'Cypress', online: false, shares: true}], incoming: [{handle: 'Wren', at: 1}], outgoing: []};

test('friends: the list and requests arrive, with a toast', () => {
    const {state: s} = testGame();
    s.receiveEvent({...friends, toast: 'Bobbin is here.'});
    assert.deepEqual(s.friends.map(f => f.handle), ['Bobbin', 'Cypress']);
    assert.equal(s.friendRequestsIn.length, 1);
    assert.equal(s.toast, 'Bobbin is here.');
    s.sendFriends('accept', {handle: 'Wren'});
});

test('private messages: their own tab, a friend chosen, unread counted, replies go back, kept ones marked', () => {
    const {state: s, commands, composer} = testGame();
    s.receiveEvent(friends);
    s.receiveEvent({type: 'ooc', channel: 'private', sequence: 4, text: 'Coming to the moot?', speaker: 'Bobbin', with: 'Bobbin', at: 100});
    assert.equal(s.unreadPrivate, 1, 'counted while another tab is open');
    assert.equal(s.privateTo, 'Bobbin', 'a reply goes back to whoever wrote');
    const post = s.posts.at(-1)!;
    assert.equal(post.channel, 'private');
    assert.equal(post.with, 'Bobbin');
    assert.equal(post.revealed, post.text.length, 'out of character: shown at once');
    s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'private', target: ''});
    assert.equal(s.channel, 'private');
    assert.equal(s.unreadPrivate, 0, 'opening the tab reads them');
    s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'private_to', target: 'Cypress'});
    s.keyDown({code: 'Enter'});
    composer.text = 'See you at dawn.';
    s.composerKey({code: 'Enter'});
    assert.ok(commands.some(c => c.type === 'chat' && c.channel === 'private' && c.to === 'Cypress' && c.text === 'See you at dawn.'),
        'sent to the friend chosen');
    s.receiveEvent({type: 'ooc', channel: 'private', sequence: 5, text: 'See you at dawn.', speaker: 'Adder', with: 'Cypress', to: ['Cypress'],
        outgoing: true, away: true, at: 101});
    const mine = s.posts.at(-1)!;
    assert.ok(mine.outgoing && mine.kept, 'one\'s own copy, kept for a friend away');
    assert.equal(s.unreadPrivate, 0, 'one\'s own lines are never unread');
    s.receiveEvent({type: 'ooc', channel: 'private', sequence: 6, text: 'Missed you.', speaker: 'Cypress', with: 'Cypress', kept: true, at: 50});
    assert.ok(s.posts.at(-1)!.kept && s.posts.at(-1)!.sentAt === 50, 'kept while away, with when it was sent');
});

test('private messages: none goes without a friend chosen, and the draft stays', () => {
    const {state: s, commands, composer} = testGame();
    s.receiveEvent({...friends, friends: [{handle: 'Bobbin', online: true, shares: true}]});
    s.messageFriend('Bobbin');
    s.receiveEvent({...friends, friends: []});
    assert.equal(s.privateTo, '', 'a friend gone from the list is no longer chosen');
    s.keyDown({code: 'Enter'});
    composer.text = 'hello?';
    s.composerKey({code: 'Enter'});
    assert.ok(!commands.some(c => c.type === 'chat'), 'nothing sent');
    assert.equal(composer.text, 'hello?', 'the draft is kept');
    assert.match(s.toast, /Choose a friend/);
});

test('friends: a sharing friend\'s handle on the wolf', () => {
    const {state: s} = testGame();
    s.receiveEvent({type: 'snapshot', revision: 1, time: 1, self: {id: 'me', name: 'Ada'}, entities: [
        {id: 'me', name: 'Ada', x: 1, y: 1}, {id: 'bo', name: 'Bo One', x: 2, y: 1, handle: 'Bobbin'}, {id: 'cy', name: 'Cy', x: 3, y: 1}]});
    assert.equal(s.entities.get('bo')?.handle, 'Bobbin');
    assert.equal(s.entities.get('cy')?.handle, '', 'a stranger shows none');
});

test('known wolves: the list, one entry opened with all its recaps, and onto an open card', () => {
    const {state: s, commands} = testGame();
    s.receiveEvent({type: 'known', wolves: [{id: 'bo', name: 'A russet wolf', scenes: 1, recaps: [{id: 'r2', text: 'Newest.'}], recapCount: 2},
        {id: 'wren', name: 'Wren', resident: true, recaps: [], recapCount: 0}]});
    assert.deepEqual(s.knownWolves.map(k => k.id), ['bo', 'wren']);
    s.inspectedCharacter = {id: 'bo', name: 'A russet wolf'};
    s.receiveEvent({type: 'known', entry: {id: 'bo', name: 'A russet wolf', note: 'Knows the mill.', recaps: [{id: 'r2', text: 'Newest.'}, {id: 'r1', text: 'Older.'}]}});
    assert.equal((s.knownOpen?.recaps as unknown[]).length, 2, 'opened: every recap');
    assert.equal((s.knownWolves[0].recaps as unknown[]).length, 1, 'the list keeps its latest only');
    assert.equal(s.inspectedCharacter?.note, 'Knows the mill.', 'an open card takes the entry');
    assert.ok(s.inspectedCharacter?.known);
    s.sendKnown('tag', {target: 'bo', tag: 'friendly'});
    assert.ok(commands.some(c => c.type === 'known' && c.verb === 'tag' && c.tag === 'friendly'));
});

test('known wolves: noted and unread marks on wolves in sight', () => {
    const {state: s} = testGame();
    s.receiveEvent({type: 'snapshot', revision: 1, time: 1, self: {id: 'me', name: 'Ada'}, entities: [
        {id: 'me', name: 'Ada', x: 1, y: 1}, {id: 'bo', name: 'A russet wolf', x: 2, y: 1, noted: true, unread: true}]});
    assert.ok(s.entities.get('bo')?.noted && s.entities.get('bo')?.unread);
});
