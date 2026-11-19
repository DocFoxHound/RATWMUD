// The dialogue window's state (Docs/Design/83-dialogue-trees.md; game/dialogue.ts, game/state.ts): Trade and Talk on a
// wheel open it (walking over first from afar) where the server runs trees, and work as before where it doesn't; the
// choices numbered and said by key or click, each with its default line or the player's own words; a locked choice
// says why; free talk; Esc says goodbye; WASD still walks; the shop window opened from it closes it with the farewell.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {testGame} from './testing.ts';
import {chooseMessage, dialogueKey, moveSelection, newDialogueUi, readDialogue, revealed, roleEntry, sayMessage, wordsFor, workEntry} from './dialogue.ts';
import {rect} from '../ui/painter.ts';
import type {Json} from './json.ts';

const node = (over: Json = {}): Json => ({
    type: 'dialogue', session: 7, npc: 'bram', name: 'Bram', role: 'baker', place: 'The Bakehouse', appearance: {sex: 'male'},
    tree: 'shop', node: 'greet', line: 'Morning, friend. What can I get you?', mood: 'warm', free: false, ends: false,
    choices: [{n: 1, text: 'Let me see your wares.'}, {n: 2, text: "What's dear today?"},
        {n: 3, text: 'Any work going?', locked: true, why: 'Nothing going.'}, {n: 4, text: 'Goodbye.'}],
    history: [], ...over});

function onTheMap(dialogue = true, work = false) {
    const g = testGame();
    g.state.applySnapshot({cell: {id: 'square', width: 32, height: 24}, self: {id: 'me', name: 'Ash', x: 5, y: 5},
        entities: [{id: 'me', name: 'Ash', x: 5, y: 5},
            {id: 'bram', name: 'Bram', x: 6, y: 5, npc: true, actions: ['talk', 'trade', 'ask for work', 'offer k12'],
                words: {'offer k12': 'Ask about: guarding the next wagon from Ridgemere to Oakhaven (12p)'}},
            {id: 'oda', name: 'Oda', x: 15, y: 5, npc: true, actions: ['talk']},
            {id: 'moss', name: 'Moss', x: 5, y: 6, npc: true, actions: ['talk', 'ask for missions', 'deliver mission-3', 'hand over k20'],
                words: {'deliver mission-3': 'Deliver 2 meals', 'hand over k20': 'Hand over the letter'}}],
        time: 0, cellGeneration: 1, ...(dialogue ? {dialogue: true} : {}), ...(work ? {dialogueWork: true} : {})});
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
    // No tree for them on this server: nothing opens, and never the old talk target (doc 84 §13.2, Phase 12).
    s.dialogue = null;
    s.openDialogue('oda', 'talk');
    s.receiveEvent({type: 'dialogue_close', npc: 'oda', entry: 'talk', why: 'none'});
    assert.ok(!commands.some(c => c.type === 'action' && c.action === 'talk'), 'no old Talk');
    assert.match(s.toast, /nothing to say/, 'said so');

    const old = onTheMap(false);
    old.state.openContextAt('bram', [120, 100]);
    old.state.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'talk'});
    assert.equal(old.commands.filter(c => c.type === 'dialogue_open').length, 0, 'an older server: no dialogue asked for');
    assert.ok(old.commands.some(c => c.type === 'action' && c.action === 'talk'), 'Talk as it was');
});

test('a giver\'s work: the wheel\'s work opens their dialogue at the job, the board or the hand-in', () => {
    assert.equal(workEntry('ask for work'), 'work');
    assert.equal(workEntry('offer k12'), 'job:k12');
    assert.equal(workEntry('ask for missions'), 'missions');
    assert.equal(workEntry('deliver mission-3'), 'deliver:mission-3');
    assert.equal(workEntry('hand over k20'), 'deliver:k20');
    assert.equal(workEntry('trade'), null);
    assert.equal(workEntry('take k12'), null, 'an older take is sent as it was');
    const {state: s, commands} = onTheMap(true, true);
    for (const [who, action, entry] of [['bram', 'offer k12', 'job:k12'], ['bram', 'ask for work', 'work'], ['moss', 'ask for missions', 'missions'],
        ['moss', 'deliver mission-3', 'deliver:mission-3'], ['moss', 'hand over k20', 'deliver:k20']] as const) {
        commands.length = 0;
        s.openContextAt(who, [120, 100]);
        s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: action});
        assert.deepEqual(commands.filter(c => c.type === 'dialogue_open').at(-1), {type: 'dialogue_open', npc: who, entry}, `${action}: ${entry}`);
        assert.equal(commands.filter(c => c.type === 'action').length, 0, `${action}: no old command`);
        s.pendingDialogue = null;
    }
    // The work, each job in plain words; one the server wouldn't allow, greyed with why.
    s.receiveEvent(node({tree: 'work', node: 'list', line: 'There\'s work, if you want it.', choices: [
        {n: 1, text: 'Ask about: guarding the next wagon from Ridgemere to Oakhaven (12p)'},
        {n: 2, text: 'Ask about: a letter for Oda (5p)', locked: true, why: 'That\'s promised to another for now.'},
        {n: 3, text: 'Goodbye.'}]}));
    assert.ok(s.dialogue && s.dialogue.tree === 'work' && s.dialogue.choices[1].locked && s.dialogue.choices[1].why.includes('promised'),
        'the job list, a locked one with why');
    assert.equal(s.modal, '', 'no offer window: the dialogue');
    commands.length = 0;
    s.keyDown({code: 'Digit2'});
    assert.equal(commands.filter(c => c.type === 'dialogue_choose').length, 0, 'the locked job sends nothing');
    s.keyDown({code: 'Digit1'});
    assert.deepEqual(commands.at(-1), {type: 'dialogue_choose', session: 7, n: 1});
    // Its details, then take it on (Accept), or not (Decline).
    s.receiveEvent(node({tree: 'work', node: 'job', line: 'Meet the wagon at the Ridgemere market. It waits for its guards before setting out.',
        choices: [{n: 1, text: 'I\'ll take it on.'}, {n: 2, text: 'What does it pay, and by when?'}, {n: 3, text: 'Is there any danger in it?'},
            {n: 4, text: 'Not for me, thanks.'}]}));
    assert.ok(s.dialogue?.line.startsWith('Meet the wagon'), 'the details');
    assert.equal(s.dialogueUi.selected, 0, 'a new node: the first choice picked out');
    commands.length = 0;
    s.keyDown({code: 'Enter'});
    assert.deepEqual(commands.at(-1), {type: 'dialogue_choose', session: 7, n: 1}, 'accepted');
    // Coming back: how it stands, and giving it back.
    s.receiveEvent(node({tree: 'work', node: 'progress', line: 'The wagon waits at the Ridgemere market for its guards.',
        choices: [{n: 1, text: 'Remind me what it is.'}, {n: 2, text: 'I can\'t do it after all.'}, {n: 3, text: 'I\'ll get on with it.'}]}));
    s.dialogueUi.drafts.set(2, 'I am sorry, but I cannot guard that wagon after all.');
    commands.length = 0;
    s.keyDown({code: 'Digit2'});
    assert.deepEqual(commands.at(-1), {type: 'dialogue_choose', session: 7, n: 2, text: 'I am sorry, but I cannot guard that wagon after all.'},
        'given back, in their own words');
    s.receiveEvent(node({tree: 'work', node: 'given', line: 'No harm done. I\'ll find another.', note: 'Given back: no harm done.',
        choices: [{n: 1, text: 'Goodbye.'}]}));
    assert.equal(s.dialogue?.note, 'Given back: no harm done.');
    // Taken meanwhile: said in words, nothing opens.
    s.dialogue = null;
    s.openDialogue('bram', 'job:k12');
    s.receiveEvent({type: 'dialogue_close', npc: 'bram', entry: 'job:k12', why: 'gone', text: 'That work has been taken, or isn\'t to be had here.'});
    assert.equal(s.dialogue, null);
    assert.equal(s.pendingDialogue, null, 'no walk over, no retry');
    assert.equal(commands.filter(c => c.type === 'action' && c.action === 'talk').length, 0, 'not the old Talk');

    // A server whose givers don't speak yet (Phase 1): the work on the wheel as it was.
    const old = onTheMap(true, false);
    old.state.openContextAt('bram', [120, 100]);
    old.state.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'offer k12'});
    assert.ok(old.commands.some(c => c.type === 'action' && c.action === 'offer k12'), 'the offer window\'s way');
    assert.equal(old.commands.filter(c => c.type === 'dialogue_open').length, 0);
});

test('the other roles: Talk asks the server, which answers with the role\'s tree; Apprentice, Report and Pay fine open theirs', () => {
    assert.equal(roleEntry('apprentice'), 'learn');
    assert.equal(roleEntry('report'), 'report');
    assert.equal(roleEntry('pay fine'), 'fine');
    assert.equal(roleEntry('talk'), null, 'Talk is talk, whoever it is');
    assert.equal(roleEntry('steal'), null);
    const roles = (roles = true) => {
        const g = testGame();
        g.state.applySnapshot({cell: {id: 'square', width: 32, height: 24}, self: {id: 'me', name: 'Ash', x: 5, y: 5},
            entities: [{id: 'me', name: 'Ash', x: 5, y: 5},
                {id: 'gil', name: 'Gil', x: 6, y: 5, npc: true, work: 'Sergeant of the watch', actions: ['talk', 'report', 'pay fine']},
                {id: 'nell', name: 'Nell', x: 5, y: 6, npc: true, work: 'keeping the inn', actions: ['talk', 'trade']},
                {id: 'hob', name: 'Hob', x: 4, y: 5, npc: true, work: 'smithing', actions: ['talk', 'apprentice']}],
            time: 0, cellGeneration: 1, dialogue: true, dialogueWork: true, ...(roles ? {dialogueRoles: true} : {})});
        g.state.mapOrigin = [0, 0];
        g.state.mapRect = rect(0, 0, 640, 480);
        g.state.tileSize = 20;
        return g;
    };
    const {state: s, commands} = roles();
    // Talk with each: the same request, the role's tree in reply (the server picks it by their work).
    for (const [who, tree, line] of [['gil', 'watch', 'The watch. What is it?'], ['nell', 'inn', 'Bed, board or a drink?'],
        ['hob', 'resident', 'What can I do for you?']] as const) {
        commands.length = 0;
        s.dialogue = null;
        s.openContextAt(who, [120, 100]);
        s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'talk'});
        assert.deepEqual(commands.filter(c => c.type === 'dialogue_open').at(-1), {type: 'dialogue_open', npc: who, entry: 'talk'}, `Talk with ${who}`);
        s.receiveEvent(node({npc: who, tree, node: 'hub', line, free: true, choices: [{n: 1, text: 'Goodbye.'}]}));
        const shown = s.dialogue as ReturnType<typeof readDialogue> | null;   // (Set by the event, past the null above.)
        assert.ok(shown && shown.tree === tree && shown.npc === who && shown.free, `${who}: the ${tree} tree, free talk in it`);
        s.pendingDialogue = null;
    }
    // The roles' own actions on the wheel: their dialogue at its entry, not the old command.
    for (const [who, action, entry] of [['gil', 'report', 'report'], ['gil', 'pay fine', 'fine'], ['hob', 'apprentice', 'learn']] as const) {
        commands.length = 0;
        s.dialogue = null;
        s.openContextAt(who, [120, 100]);
        s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: action});
        assert.deepEqual(commands.filter(c => c.type === 'dialogue_open').at(-1), {type: 'dialogue_open', npc: who, entry}, `${action}: ${entry}`);
        assert.equal(commands.filter(c => c.type === 'action').length, 0, `${action}: no old command`);
        s.pendingDialogue = null;
    }
    // Refused (not the watch after all): said in words, nothing opens, nothing retried.
    s.openDialogue('hob', 'report');
    s.receiveEvent({type: 'dialogue_close', npc: 'hob', entry: 'report', why: 'none', text: 'Hob isn\'t of the watch.'});
    assert.equal(s.dialogue, null);
    assert.equal(s.pendingDialogue, null);
    // A server whose roles don't speak yet (Phase 2): the wheel's actions as they were.
    const old = roles(false);
    old.state.openContextAt('gil', [120, 100]);
    old.state.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'pay fine'});
    assert.ok(old.commands.some(c => c.type === 'action' && c.action === 'pay fine'), 'Pay fine as it was');
    assert.equal(old.commands.filter(c => c.type === 'dialogue_open').length, 0);
});
