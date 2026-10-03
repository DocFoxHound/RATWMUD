// Parties on the page (Docs/Design/32-parties-chapters-factions.md, Part 2): reading the party, who shows as a party
// mate or hostile, the party's two chats, and the party's commands. The rules are the server's (Tests/party_tests.cpp).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {testGame} from './testing.ts';
import {inParty, readParty} from './party.ts';
import type {Json} from './json.ts';

const rows = ['##########', '#........#', '#........#', '#........#', '##########'];
const party: Json = {
    id: 'party-1', leader: 'self',
    members: [
        {id: 'self', name: 'Ada', leader: true, online: true, cell: 'room', place: 'The Room', x: 2.5, y: 2.5, health: 100},
        {id: 'bo', name: 'Bo', leader: false, online: true, cell: 'yard', place: 'The Yard', x: 4, y: 1, health: 60, fighting: true},
        {id: 'cy', name: 'Cy', leader: false, online: false},
    ],
};
const snapshot = (self: Json = {}, entities: Json[] = []): Json => ({
    cell: {id: 'room', width: 10, height: 5, rows}, visibility: rows.map(r => '2'.repeat(r.length)),
    self: {id: 'self', x: 2.5, y: 2.5, ...self}, entities: [{id: 'self', x: 2.5, y: 2.5}, ...entities],
});

test('parties: the party, an invitation and a fight calling are read from the own wolf', () => {
    const p = readParty({party})!;
    assert.equal(p.members.length, 3);
    assert.equal(p.members[1].place, 'The Yard');
    assert.ok(p.members[1].fighting && !p.members[2].online);
    assert.ok(inParty(p) && p.autoJoin);
    const invited = readParty({party: {invite: {from: 'bo', name: 'Bo', seconds: 42}, autoJoin: false}})!;
    assert.ok(!inParty(invited) && invited.invite?.name === 'Bo' && !invited.autoJoin, 'invited, but in no party yet');
    assert.equal(readParty({}), null);
});

test('parties: party mates and hostile wolves are marked, and why', () => {
    const {state: s} = testGame();
    s.applySnapshot(snapshot({party}, [
        {id: 'bo', name: 'Bo', x: 4, y: 2, rel: 'party'},
        {id: 'rook', name: 'Rook', x: 6, y: 2, rel: 'hostile', why: 'fighting your party'},
        {id: 'bandit', name: 'Bandit', x: 7, y: 3, hostile: true, rel: 'hostile', why: 'bandit'},
        {id: 'wren', name: 'Wren', x: 8, y: 3},
    ]));
    assert.equal(s.entities.get('bo')?.rel, 'party');
    assert.ok(!s.entities.get('bo')?.hostile);
    assert.ok(s.entities.get('rook')?.hostile && s.entities.get('rook')?.why === 'fighting your party');
    assert.ok(s.entities.get('bandit')?.hostile);
    assert.ok(!s.entities.get('wren')?.hostile && s.entities.get('wren')?.rel === '');
});

test('parties: two chats, in character and out, only while in a party', () => {
    const {state: s, commands, composer} = testGame();
    const lastChat = () => commands.filter(c => c.type === 'chat').at(-1);
    s.applySnapshot(snapshot());
    s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'party', target: ''});
    assert.equal(s.channel, 'ic', 'no party: no party chat');
    s.applySnapshot(snapshot({party}));
    s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'party', target: ''});
    assert.equal(s.channel, 'party');
    s.setChat(true);
    composer.text = '"We leave at dusk."';
    s.submitPost();
    assert.equal(lastChat()?.channel, 'party', 'said to the party, in character');
    s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'partyooc', target: ''});
    s.setChat(true);
    composer.text = 'brb';
    s.submitPost();
    assert.equal(lastChat()?.channel, 'partyooc');
    // A party line is marked, and party OOC is shown at once.
    s.receiveEvent({type: 'roleplay', id: '7', speaker: 'Bo', text: 'Agreed.', channel: 'ic', party: true});
    s.receiveEvent({type: 'ooc', speaker: 'Bo', text: 'ok', channel: 'partyooc', sequence: 8});
    assert.ok(s.posts.find(p => p.text === 'Agreed.')?.party);
    const ooc = s.posts.find(p => p.text === 'ok')!;
    assert.equal(ooc.channel, 'partyooc');
    assert.equal(ooc.revealed, ooc.text.length);
    // Leaving the party puts the composer back in the world.
    s.applySnapshot(snapshot());
    assert.equal(s.channel, 'ic');
});

test('parties: the party commands', () => {
    const {state: s, commands} = testGame();
    s.applySnapshot(snapshot({party}));
    const verb = (target: string) => s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'party_verb', target});
    verb('accept');
    assert.deepEqual(commands.at(-1), {type: 'party', verb: 'accept'});
    verb('remove:bo');
    assert.deepEqual(commands.at(-1), {type: 'party', verb: 'remove', target: 'bo'});
    verb('autojoin:off');
    assert.deepEqual(commands.at(-1), {type: 'party', verb: 'autojoin', on: false});
    const before = commands.length;
    verb('explode');
    assert.equal(commands.length, before, 'nothing unknown is sent');
});

test('parties: residents travelling with the party, and where it is bound', () => {
    const p = readParty({party: {id: 'party-2', leader: 'self', goal: 'Ser Ferro', members: [
        {id: 'self', name: 'Ada', leader: true, online: true},
        {id: 'npc_scout', name: 'Bracken', npc: true, reason: 'hired', wage: 6, waiting: true, mine: true, online: true},
    ]}})!;
    assert.ok(inParty(p), 'a player and a resident are a party');
    assert.equal(p.goal, 'Ser Ferro');
    const b = p.members[1];
    assert.ok(b.npc && b.reason === 'hired' && b.wage === 6 && b.waiting && b.mine);
    const {state: s, commands} = testGame();
    s.applySnapshot(snapshot({party: {id: 'party-2', leader: 'self', members: [{id: 'self'}, {id: 'npc_scout', npc: true}]}}));
    s.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'party_verb', target: 'goal: Ser Ferro: the west gate'});
    assert.deepEqual(commands.at(-1), {type: 'party', verb: 'goal', goal: 'Ser Ferro: the west gate'}, 'a goal may hold a colon');
    s.sendAction('wait here', 'npc_scout');
    assert.deepEqual(commands.at(-1), {type: 'action', action: 'wait here', target: 'npc_scout'});
});
