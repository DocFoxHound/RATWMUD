// Names on the page (Docs/Design/32-parties-chapters-factions.md, 1.5): introducing oneself is said aloud, and aliases
// are added and retired. Who knows whom is the server's (Tests/names_tests.cpp).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {testGame} from './testing.ts';
import type {Json} from './json.ts';

const rows = ['##########', '#........#', '#........#', '#........#', '##########'];
const snapshot = (entities: Json[] = []): Json => ({
    cell: {id: 'room', width: 10, height: 5, rows}, visibility: rows.map(r => '2'.repeat(r.length)),
    self: {id: 'self', x: 2.5, y: 2.5, names: {name: 'Ash', aliases: ['Kestrel'], hidden: true}},
    entities: [{id: 'self', x: 2.5, y: 2.5}, ...entities],
});
const noRect = {left: 0, top: 0, right: 0, bottom: 0};

test('names: a stranger is shown as they look, and an introduction is said aloud', () => {
    const {state: s, commands} = testGame();
    s.applySnapshot(snapshot([{id: 'bo', name: 'A tall russet wolf with white socks', known: false, x: 4, y: 2,
        actions: ['inspect', 'introduce', 'introduce as Kestrel']}]));
    assert.equal(s.entities.get('bo')?.name, 'A tall russet wolf with white socks');
    s.sendAction('introduce', 'bo');
    assert.deepEqual({...commands.at(-1), requestId: ''},
        {type: 'chat', requestId: '', text: '"I\'m Ash."', channel: 'ic', volume: 'speak', targets: ['bo']});
    s.sendAction('introduce as Kestrel', 'bo');
    assert.equal(commands.at(-1)?.text, '"I\'m Kestrel."', 'by an alias');
});

test('names: aliases are added and retired', () => {
    const {state: s, commands} = testGame();
    s.applySnapshot(snapshot());
    s.activate({rect: noRect, action: 'name_add', target: '  Old Tam '});
    assert.deepEqual(commands.at(-1), {type: 'names', verb: 'add', name: 'Old Tam'});
    s.activate({rect: noRect, action: 'name_retire', target: 'Kestrel'});
    assert.deepEqual(commands.at(-1), {type: 'names', verb: 'retire', name: 'Kestrel'});
    const before = commands.length;
    s.activate({rect: noRect, action: 'name_add', target: '   '});
    assert.equal(commands.length, before, 'nothing blank is sent');
});
