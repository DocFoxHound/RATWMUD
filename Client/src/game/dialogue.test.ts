// The dialogue window's state (Docs/Design/83-dialogue-trees.md; game/dialogue.ts, game/state.ts): Trade and Talk on a
// wheel open it (walking over first from afar) where the server runs trees, and work as before where it doesn't; the
// choices numbered and said by key or click, each with its default line or the player's own words; a locked choice
// says why; free talk; Esc says goodbye; WASD still walks; the shop window opened from it closes it with the farewell.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {testGame} from './testing.ts';
import {chooseMessage, dialogueKey, moveSelection, newDialogueUi, readDialogue, revealed, sayMessage, wordsFor} from './dialogue.ts';
import {rect} from '../ui/painter.ts';
import type {Json} from './json.ts';

const node = (over: Json = {}): Json => ({
    type: 'dialogue', session: 7, npc: 'bram', name: 'Bram', role: 'baker', place: 'The Bakehouse', appearance: {sex: 'male'},
    tree: 'shop', node: 'greet', line: 'Morning, friend. What can I get you?', mood: 'warm', free: false, ends: false,
    choices: [{n: 1, text: 'Let me see your wares.'}, {n: 2, text: "What's dear today?"},
        {n: 3, text: 'Any work going?', locked: true, why: 'Nothing going.'}, {n: 4, text: 'Goodbye.'}],
    history: [], ...over});

function onTheMap(dialogue = true) {
    const g = testGame();
    g.state.applySnapshot({cell: {id: 'square', width: 32, height: 24}, self: {id: 'me', name: 'Ash', x: 5, y: 5},
        entities: [{id: 'me', name: 'Ash', x: 5, y: 5}, {id: 'bram', name: 'Bram', x: 6, y: 5, npc: true, actions: ['talk', 'trade']},
            {id: 'oda', name: 'Oda', x: 15, y: 5, npc: true, actions: ['talk']}],
        time: 0, cellGeneration: 1, ...(dialogue ? {dialogue: true} : {})});
    g.state.mapOrigin = [0, 0];
    g.state.mapRect = rect(0, 0, 640, 480);
    g.state.tileSize = 20;
    return g;
}

test('the choices: the default line, or the player\'s own words; locked ones refuse; free talk where allowed', () => {
    const d = readDialogue(node());
    const ui = newDialogueUi();
    assert.equal(d.choices.length, 4);
    assert.deepEqual(chooseMessage(d, ui, 1), {type: 'dialogue_choose', session: 7, n: 1}, 'the default: no words sent');
    ui.drafts.set(2, '  Tell me,   what costs the most this week? ');
    assert.equal(wordsFor(d, ui, 2), 'Tell me, what costs the most this week?');
    assert.deepEqual(chooseMessage(d, ui, 2), {type: 'dialogue_choose', session: 7, n: 2, text: 'Tell me, what costs the most this week?'},
        'the player\'s own words');
    ui.drafts.set(1, '   ');
    assert.deepEqual(chooseMessage(d, ui, 1), {type: 'dialogue_choose', session: 7, n: 1}, 'an emptied field says the default');
    assert.equal(chooseMessage(d, ui, 3), null, 'locked');
    assert.equal(chooseMessage(d, ui, 9), null, 'no such choice');
    assert.equal(sayMessage(d, 'Hello there.'), null, 'no free talk on this node');
    const free = readDialogue(node({free: true}));
    assert.deepEqual(sayMessage(free, ' Fine weather   for it. '), {type: 'dialogue_say', session: 7, text: 'Fine weather for it.'});
    assert.equal(sayMessage(readDialogue(node({free: true, awaiting: true})), 'Hm?'), null, 'not while they answer');
    // Up and Down skip the locked one, round the ends.
    ui.selected = 1;
    moveSelection(d, ui, 1);
    assert.equal(ui.selected, 3, 'past the locked third');
    moveSelection(d, ui, 1);
    assert.equal(ui.selected, 0, 'round to the first');
    moveSelection(d, ui, -1);
    assert.equal(ui.selected, 3);
    assert.deepEqual(dialogueKey('Digit2', d), {kind: 'choose', n: 2});
    assert.equal(dialogueKey('Digit7', d), null, 'a number not shown goes elsewhere');
    assert.equal(dialogueKey('KeyW', d), null, 'WASD is the map\'s');
    assert.deepEqual(dialogueKey('Escape', d), {kind: 'bye'});
    // The line is written out, and shown whole on Space.
    ui.lineAt = 0;
    assert.equal(revealed(d.line, ui, 0.1), d.line.slice(0, 6));
    ui.whole = true;
    assert.equal(revealed(d.line, ui, 0.1), d.line);
});

test('Trade opens the dialogue window: near at once, from afar on arrival; keys choose, Esc says goodbye, WASD walks', () => {
    const {state: s, commands} = onTheMap();
    s.openContextAt('bram', [120, 100]);
    s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'trade'});
    assert.ok(commands.some(c => c.type === 'shop' && c.target === 'bram'), 'whose list');
    assert.deepEqual(commands.filter(c => c.type === 'dialogue_open').at(-1), {type: 'dialogue_open', npc: 'bram', entry: 'trade'},
        'within reach: the conversation is asked for at once');
    assert.equal(s.modal, '', 'not the shop window yet');
    s.receiveEvent(node());
    assert.ok(s.dialogue && s.dialogue.name === 'Bram' && s.dialogue.choices.length === 4, 'the window shows the keeper\'s greeting');
    assert.equal(s.modal, '', 'the window is no modal: the world goes on');
    // 1: the wares, two clicks from the wheel.
    commands.length = 0;
    assert.ok(s.keyDown({code: 'Digit1'}));
    assert.deepEqual(commands.at(-1), {type: 'dialogue_choose', session: 7, n: 1});
    // Down past the locked one, own words, Enter.
    s.dialogueUi.selected = 1;
    s.keyDown({code: 'ArrowDown'});
    assert.equal(s.dialogueUi.selected, 3, 'past the locked choice');
    s.dialogueUi.drafts.set(4, 'Good day to you, and thanks.');
    s.keyDown({code: 'Enter'});
    assert.deepEqual(commands.at(-1), {type: 'dialogue_choose', session: 7, n: 4, text: 'Good day to you, and thanks.'});
    // A locked number: nothing sent, its reason said.
    commands.length = 0;
    s.keyDown({code: 'Digit3'});
    assert.equal(commands.filter(c => c.type === 'dialogue_choose').length, 0, 'a locked choice sends nothing');
    // WASD still walks.
    assert.ok(s.keyDown({code: 'KeyD'}));
    assert.ok(commands.some(c => c.type === 'move'), 'walking with the window open');
    s.keyUp({code: 'KeyD'});
    // The server's "wares": the shop window, the conversation behind it; closing the window ends it with the farewell.
    commands.length = 0;
    s.receiveEvent({type: 'dialogue_shop', session: 7, npc: 'bram'});
    assert.equal(s.modal, '', 'not at the counter yet: no window');
    assert.ok(commands.some(c => c.type === 'path'), 'a step nearer');
    s.applySnapshot({...s.snapshot!, merchant: {id: 'bram', name: 'Bram', cash: 90, staged: true, buyingToday: 40, items: []}});
    assert.equal(s.modal, 'trade', 'the shop window, as the keeper\'s list arrives');
    assert.ok(s.dialogue?.inShop, 'the conversation waits behind it');
    commands.length = 0;
    s.keyDown({code: 'Escape'});
    s.tick(1, 1 / 60);
    assert.deepEqual(commands.find(c => c.type === 'dialogue_close'), {type: 'dialogue_close', session: 7, why: 'shop'}, 'the farewell');
    assert.equal(s.dialogue, null);
    // Esc in the window: goodbye.
    s.receiveEvent(node({session: 8}));
    commands.length = 0;
    s.keyDown({code: 'Escape'});
    assert.deepEqual(commands.at(-1), {type: 'dialogue_close', session: 8, why: 'bye'});
    assert.equal(s.dialogue, null, 'and it closes');
    // The server ends one: walked away.
    s.receiveEvent(node({session: 9}));
    s.receiveEvent({type: 'dialogue_close', session: 9, npc: 'bram', why: 'away'});
    assert.equal(s.dialogue, null, 'closed by the server');
});

test('Talk opens the hub from afar after walking over; an older server keeps the old ways', () => {
    const {state: s, commands} = onTheMap();
    s.openContextAt('oda', [300, 100]);
    s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'talk'});
    assert.equal(commands.filter(c => c.type === 'dialogue_open').length, 0, 'too far: not yet');
    assert.deepEqual(commands.filter(c => c.type === 'path').at(-1), {type: 'path', x: 15, y: 5}, 'walking over');
    s.applySnapshot({...s.snapshot!, self: {id: 'me', name: 'Ash', x: 13, y: 5},
        entities: [{id: 'me', name: 'Ash', x: 13, y: 5}, {id: 'oda', name: 'Oda', x: 15, y: 5, npc: true, actions: ['talk']}]});
    assert.deepEqual(commands.filter(c => c.type === 'dialogue_open').at(-1), {type: 'dialogue_open', npc: 'oda', entry: 'talk'}, 'arrived: asked');
    s.receiveEvent(node({npc: 'oda', name: 'Oda', tree: 'resident', node: 'hub', free: true, choices: [{n: 1, text: 'What do you do?'}, {n: 2, text: 'Goodbye.'}]}));
    commands.length = 0;
    assert.ok(s.sayDialogue('What brings you out in this weather?'));
    assert.deepEqual(commands.at(-1), {type: 'dialogue_say', session: 7, text: 'What brings you out in this weather?'}, 'free talk');
    // No tree for them on this server: the talk target, as before.
    s.dialogue = null;
    s.openDialogue('oda', 'talk');
    s.receiveEvent({type: 'dialogue_close', npc: 'oda', entry: 'talk', why: 'none'});
    assert.ok(commands.some(c => c.type === 'action' && c.action === 'talk' && c.target === 'oda'), 'the old Talk');

    const old = onTheMap(false);
    old.state.openContextAt('bram', [120, 100]);
    old.state.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'talk'});
    assert.equal(old.commands.filter(c => c.type === 'dialogue_open').length, 0, 'an older server: no dialogue asked for');
    assert.ok(old.commands.some(c => c.type === 'action' && c.action === 'talk'), 'Talk as it was');
});
