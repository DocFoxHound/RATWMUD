// Chapters on the page (Docs/Design/32-parties-chapters-factions.md, Part 3): the Chapter's chats, its colour on a
// mate, and its commands. The rules are the server's (Tests/chapter_tests.cpp).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {testGame} from './testing.ts';
import type {Json} from './json.ts';

const rows = ['##########', '#........#', '#........#', '#........#', '##########'];
const snapshot = (chapter: Json | null, entities: Json[] = []): Json => ({
    cell: {id: 'room', width: 10, height: 5, rows}, visibility: rows.map(r => '2'.repeat(r.length)),
    self: {id: 'self', x: 2.5, y: 2.5, ...(chapter ? {chapter} : {})}, entities: [{id: 'self', x: 2.5, y: 2.5}, ...entities],
});
const noRect = {left: 0, top: 0, right: 0, bottom: 0};

test('chapters: chats only in a Chapter, a mate in its colour, and commands', () => {
    const {state: s, commands} = testGame();
    s.applySnapshot(snapshot(null));
    s.activate({rect: noRect, action: 'chapter', target: ''});
    assert.equal(s.channel, 'ic', 'no Chapter: no Chapter chat');
    s.applySnapshot(snapshot({id: 'chapter-1', name: 'Ashen Lodge', colour: '#5b8bd9'},
        [{id: 'bo', name: 'Bo', x: 4, y: 2, rel: 'chapter', colour: '#5b8bd9'}]));
    assert.ok(s.inChapter());
    assert.equal(s.entities.get('bo')?.rel, 'chapter');
    assert.equal(s.entities.get('bo')?.colour, '#5b8bd9');
    s.activate({rect: noRect, action: 'chapterooc', target: ''});
    assert.equal(s.channel, 'chapterooc');
    s.sendChapter({verb: 'deposit', amount: 5});
    assert.deepEqual(commands.at(-1), {type: 'chapter', verb: 'deposit', amount: 5});
    s.receiveEvent({type: 'roleplay', id: '9', speaker: 'Bo', text: 'To the Lodge.', channel: 'ic', chapter: true});
    assert.ok(s.posts.find(p => p.text === 'To the Lodge.')?.chapter);
    s.applySnapshot(snapshot(null));
    assert.equal(s.channel, 'ic', 'out of the Chapter: back to the world');
});

test('factions: a mission board opens, and a mission is taken', () => {
    const {state: s, commands} = testGame();
    s.receiveEvent({type: 'missions', faction: 'The Greyfen Watch', missions: [{id: 'mission-1', text: 'Bring 2 meals', tier: 1, allowed: true}]});
    assert.equal(s.modal, 'missions');
    assert.equal(s.missionBoard?.faction, 'The Greyfen Watch');
    s.sendFaction({verb: 'take', mission: 'mission-1'});
    assert.deepEqual(commands.at(-1), {type: 'faction', verb: 'take', mission: 'mission-1'});
});
