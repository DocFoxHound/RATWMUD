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

test('circles: the list and invitations, one chat tab a circle, unread counted, lines sent to it', () => {
    const {state: s, commands, composer} = testGame();
    s.receiveEvent({type: 'circles', circles: [{id: 'c1', name: 'Moot Night', role: 'keeper', members: [], nights: []}],
        invites: [{circle: 'c2', name: 'Pack Night', from: 'Bobbin'}]});
    assert.equal(s.circles.length, 1);
    assert.equal(s.circleInvites.length, 1);
    s.receiveEvent({type: 'ooc', channel: 'circle', circle: 'c1', circleName: 'Moot Night', sequence: 9, speaker: 'Bobbin', text: 'Tonight?'});
    assert.equal(s.posts.at(-1)?.channel, 'circle:c1', 'its own channel');
    assert.equal(s.unreadCircles.c1, 1, 'unread while another tab is open');
    s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'circle:c1', target: ''});
    assert.equal(s.channel, 'circle:c1');
    assert.equal(s.unreadCircles.c1, 0);
    s.keyDown({code: 'Enter'});
    composer.text = 'Yes, at the Wharf.';
    s.composerKey({code: 'Enter'});
    assert.ok(commands.some(c => c.type === 'chat' && c.channel === 'circle' && c.circle === 'c1' && c.text === 'Yes, at the Wharf.'));
    s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'circle:nope', target: ''});
    assert.equal(s.channel, 'circle:c1', 'not a circle one is in');
    s.receiveEvent({type: 'circles', circles: [], invites: []});
    assert.equal(s.channel, 'ic', 'a circle left: back to the world');
});

test('stars: in words, exact for oneself and a friend who sees, in bands for others', async () => {
    const {starsLine} = await import('../ui/hud/dialogs.ts');
    assert.equal(starsLine({exact: true, total: 437, from: 61}), '★ 437 stars from 61 wolves');
    assert.equal(starsLine({exact: true, total: 1, from: 1}), '★ 1 star from 1 wolf');
    assert.equal(starsLine({exact: true, total: 0, from: 0}), '★ No stars yet');
    assert.equal(starsLine({exact: false, band: '250+', fromBand: '30+'}), '★ 250+ stars from 30+ wolves');
    assert.equal(starsLine({exact: false, band: 'a few', fromBand: 'a few'}), '★ A few stars');
});

test('stars: Known for, the tags and the rate in words', async () => {
    const {starsDetail} = await import('../ui/hud/dialogs.ts');
    assert.equal(starsDetail({exact: true, knownFor: ['Storyteller'], tags: [{name: 'Storyteller', count: 30}, {name: 'Packmate', count: 15}],
        rate: 'most wolves who play with them leave a star'}), 'Known for: Storyteller · Storyteller 30, Packmate 15 · Most wolves who play with them leave a star');
    assert.equal(starsDetail({exact: false, knownFor: ['Packmate', 'Good fun'], tags: [{name: 'Packmate', words: 'often'}]}),
        'Known for: Packmate and Good fun · often Packmate');
    assert.equal(starsDetail({exact: false, band: 'a few', tags: []}), '');
});

test('scenes in the log: a line keeps its scene; "my scene only" keeps one\'s own scene, one\'s own words, the world and "→ you"', async () => {
    const {keptByMyScene} = await import('./state.ts');
    const {state: s} = testGame();
    s.receiveEvent({type: 'roleplay', id: 'r1', sequence: 1, speaker: 'A grey wolf', text: 'Mine.', scene: {mine: true, colour: '#5fb3a8'}});
    s.receiveEvent({type: 'roleplay', id: 'r2', sequence: 2, speaker: 'A dun wolf', text: 'Open nearby.', scene: {openness: 'open', place: 'The Wharf', colour: '#9b88c9'}});
    s.receiveEvent({type: 'roleplay', id: 'r3', sequence: 3, speaker: 'A dun wolf', text: 'Private talk.'});
    s.receiveEvent({type: 'roleplay', id: 'r4', sequence: 4, speaker: 'A dun wolf', text: 'To you.', to: ['you']});
    const [mine, open, priv, toYou] = s.posts.slice(-4);
    assert.ok(mine.scene?.mine && mine.scene.colour === '#5fb3a8', 'one\'s own scene, with its colour');
    assert.equal(open.scene?.openness, 'open');
    assert.equal(open.scene?.place, 'The Wharf');
    assert.equal(priv.scene, undefined, 'ordinary talk carries no scene');
    assert.deepEqual([mine, open, priv, toYou].map(p => keptByMyScene(p, 'Ada')), [true, false, false, true]);
    assert.ok(keptByMyScene({...priv, speaker: 'Ada'}, 'Ada'), 'one\'s own words stay');
    s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'my_scene', target: ''});
    assert.ok(s.mySceneOnly, 'the toggle');
});

test('a fight\'s log, blow by blow, kept as asked for', () => {
    const {state: s} = testGame();
    s.receiveEvent({type: 'fightLog', session: 'fight-b1', lines: ['A grey wolf bites a dun wolf (6).', 'A dun wolf stands on guard.']});
    assert.deepEqual(s.fightLogs.get('fight-b1'), ['A grey wolf bites a dun wolf (6).', 'A dun wolf stands on guard.']);
});

test('the howl: heard once with a sound, one mark per chorus updated, /howl sends it', () => {
    const {state: s, commands, composer} = testGame();
    const cues: string[] = [];
    s.onCue = c => cues.push(c);
    s.receiveEvent({type: 'howl', id: 'howl-1', bearing: 45, band: 'far', status: 'Looking for a scene', wolves: 1, canJoin: false, until: 60});
    s.receiveEvent({type: 'howl', id: 'howl-1', bearing: 47, band: 'far', wolves: 2, canJoin: false, until: 60});
    assert.equal(s.howls.size, 1, 'one mark for the chorus');
    assert.equal(s.howls.get('howl-1')?.wolves, 2, 'its count updated');
    assert.deepEqual(cues, ['howlFar'], 'heard once, faint from far off');
    s.keyDown({code: 'Enter'});
    composer.text = '/howl';
    s.composerKey({code: 'Enter'});
    assert.ok(commands.some(c => c.type === 'howl') && !commands.some(c => c.type === 'chat'), '/howl howls; it is not said');
});

test('Story books: the shelf (forty at a time, appended), a book opened, and the commands', () => {
    const {state: s, commands} = testGame();
    s.openShelf('shelf', 'all');
    assert.equal(s.modal, 'stories');
    assert.ok(commands.some(c => c.type === 'book' && c.verb === 'shelf' && c.tab === 'shelf' && c.offset === 0));
    s.receiveEvent({type: 'shelf', tab: 'shelf', filter: 'all', offset: 0, books: [{id: 'book-1', title: 'The Drowned Bell'}], more: true,
        open: [{id: 'book-1', title: 'The Drowned Bell'}]});
    s.receiveEvent({type: 'shelf', tab: 'shelf', filter: 'all', offset: 40, books: [{id: 'book-2', title: 'Older'}], more: false, open: []});
    assert.deepEqual(s.shelf.books.map(b => b.id), ['book-1', 'book-2'], 'the next forty appended');
    s.openBook('book-1');
    s.receiveEvent({type: 'book', book: {id: 'book-1', title: 'The Drowned Bell', chapterList: []}});
    assert.equal(s.modal, 'book');
    assert.equal(s.bookOpen?.title, 'The Drowned Bell');
});

test('newcomers and mentors: the marks on wolves in sight, in words, and the mentor commands', async () => {
    const {describeWolf} = await import('./look.ts');
    const {state: s, commands} = testGame();
    s.receiveEvent({type: 'snapshot', revision: 1, time: 1, self: {id: 'me', name: 'Ada'}, entities: [
        {id: 'me', name: 'Ada', x: 1, y: 1}, {id: 'nia', name: 'A dun wolf', x: 2, y: 1, nc: true},
        {id: 'mo', name: 'A grey wolf', x: 3, y: 1, mentor: true, mentorFree: true}, {id: 'bu', name: 'A red wolf', x: 4, y: 1, mentor: true}]});
    const nia = s.entities.get('nia')!, mo = s.entities.get('mo')!, bu = s.entities.get('bu')!;
    assert.ok(nia.nc && !nia.mentor, 'a newcomer');
    assert.ok(mo.mentor && mo.mentorFree && bu.mentor && !bu.mentorFree, 'a mentor free to take one, and one who isn\'t');
    assert.match(describeWolf(nia).why, /new to these parts/);
    assert.match(describeWolf(mo).why, /a mentor, free to show you around/);
    assert.match(describeWolf(bu).why, /a mentor/);
    s.send({type: 'mentor', verb: 'optin'});
    assert.ok(commands.some(c => c.type === 'mentor' && c.verb === 'optin'));
});

test('ties: an offer to a mentor, with its seconds, answered once', () => {
    const {state: s, commands} = testGame();
    s.receiveEvent({type: 'tieOffer', tie: 'tie-1', look: 'a dun wolf', starter: 'You came into town on the same cart as a newcomer.', town: 'Upper Accord', seconds: 180});
    assert.equal(s.tieOffer?.tie, 'tie-1');
    assert.ok(s.tieOffer!.until > s.clock + 170, 'three minutes to answer');
    s.answerTie('accept');
    assert.ok(commands.some(c => c.type === 'mentor' && c.verb === 'accept' && c.tie === 'tie-1'), 'accepted');
    assert.equal(s.tieOffer, null, 'answered: gone');
    s.answerTie('pass');
    assert.equal(commands.filter(c => c.type === 'mentor').length, 1, 'nothing more to answer');
});

test('first evenings and vouching: the innkeeper\'s prompt, and vouching for a wolf in sight', () => {
    const {state: s, commands} = testGame();
    s.receiveEvent({type: 'snapshot', revision: 1, time: 1, self: {id: 'me', name: 'Ada', names: {name: 'Ada'}}, entities: [
        {id: 'me', name: 'Ada', x: 1, y: 1}, {id: 'nia', name: 'A dun wolf', x: 2, y: 1},
        {id: 'inn', name: 'The wolf who serves at the inn', kind: 'npc', x: 3, y: 1, actions: ['talk', 'vouch']}]});
    s.receiveEvent({type: 'introducePrompt', target: 'nia', text: 'The innkeeper is pointing you out to a newcomer. Introduce yourself?', seconds: 120});
    assert.equal(s.introducePrompt?.target, 'nia');
    s.answerIntroduce(true);
    assert.ok(commands.some(c => c.type === 'chat' && c.text === '"I\'m Ada."' && JSON.stringify(c.targets) === '["nia"]'), 'introduced to the newcomer');
    assert.equal(s.introducePrompt, null);
    s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'target', target: 'inn'});
    s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'context', target: 'vouch'});
    assert.deepEqual(s.contextActions, ['vouch:nia'], 'a wolf in sight to vouch for');
    s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'context', target: 'vouch:nia'});
    assert.ok(commands.some(c => c.type === 'vouch' && c.resident === 'inn' && c.for === 'nia'), 'vouched for, to the innkeeper');
});
