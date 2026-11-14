// The slash commands and the wheel a right-click opens (game/commands.ts; the user's notes, 2026-11-14): everything the
// old ACTIONS panel held is a command in the box and on the ground's wheel; a wolf's wheel is its actions.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {testGame} from './testing.ts';
import {Commands, parseSlash, slashSuggestions, wheelCommands} from './commands.ts';
import {GroundTarget} from './state.ts';
import {rect} from '../ui/painter.ts';
import type {Json} from './json.ts';
import {gearIconFor, senseLevel, timeOfDay, weatherIcon} from '../ui/hud/hudIcons.ts';

function onTheMap(self: Json = {}) {
    const g = testGame();
    const snapshot: Json = {cell: {id: 'square', width: 32, height: 24}, self: {id: 'me', name: 'Ash', x: 5, y: 5, ...self},
        entities: [{id: 'me', name: 'Ash', x: 5, y: 5}, {id: 'bram', name: 'Bram', x: 8, y: 5, npc: true, actions: ['talk', 'inspect', 'trade']}],
        time: 0, cellGeneration: 1};
    g.state.applySnapshot(snapshot);
    g.state.mapOrigin = [0, 0];
    g.state.mapRect = rect(0, 0, 640, 480);
    g.state.tileSize = 20;
    return {...g, snapshot};
}

const typed = (s: ReturnType<typeof onTheMap>['state'], text: string) => {
    s.setChat(true);
    s.composer.text = text;
    s.composerChanged();
};

test('slash commands: "/" lists them, the list narrows as one types, Tab completes, a bare one runs and is never said', () => {
    const {state: s, commands} = onTheMap();
    const all = slashSuggestions(s, '/').map(c => c.name);
    for (const name of ['listen', 'look', 'smell', 'wait', 'sit', 'lay', 'howl', 'groom', 'pose', 'me', 'character', 'inventory', 'map'])
        assert.ok(all.includes(name), `/${name} is listed`);
    assert.ok(!all.includes('forage') && !all.includes('hunt') && !all.includes('perform') && !all.includes('board'),
        'what this place does not allow is not listed');
    assert.ok(!all.includes('narrate'), "a storyteller's commands only for an approved storyteller");
    assert.deepEqual(slashSuggestions(s, '/l').map(c => c.name), ['listen', 'look', 'lay']);
    assert.deepEqual(slashSuggestions(s, '/listen now'), [], 'no list once it has its words');
    assert.deepEqual(slashSuggestions(s, 'hello'), [], 'nor for speech');
    assert.deepEqual(parseSlash('/Pose stretches'), {name: 'pose', arg: 'stretches'});

    typed(s, '/lo');
    assert.ok(s.composerKey({code: 'Tab'}), 'Tab is taken');
    assert.equal(s.composer.text, '/look', 'and completes the first that fits');
    typed(s, '/l');
    s.composerKey({code: 'ArrowDown'});
    assert.equal(s.slashPick, 1, 'Down chooses the next');
    s.composerKey({code: 'Tab'});
    assert.equal(s.composer.text, '/look');
    typed(s, '/p');
    s.composerKey({code: 'Tab'});
    assert.equal(s.composer.text, '/pose ', 'one that takes words leaves room for them');

    const before = commands.length;
    typed(s, '/listen');
    s.composerKey({code: 'Enter'});
    const sent = commands.slice(before).filter(c => c.type !== 'move' && c.type !== 'typing');
    assert.deepEqual(sent, [{type: 'action', action: 'listen', target: ''}], 'run here: an action, no chat');
    assert.ok(!s.chat && s.composer.text === '', 'the box is cleared and the map has the keys again');

    typed(s, '/sme');
    s.composerKey({code: 'Enter'});
    assert.deepEqual(commands.at(-1), {type: 'action', action: 'smell', target: ''}, 'Enter on a part of one completes and runs it');

    typed(s, '/howl');
    s.composerKey({code: 'Enter'});
    assert.deepEqual(commands.at(-1), {type: 'howl'}, 'the gathering howl, as before');
    typed(s, '/groom');
    s.composerKey({code: 'Enter'});
    assert.deepEqual(commands.at(-1), {type: 'groom', target: 'self'});

    typed(s, '/sit');
    s.composerKey({code: 'Enter'});
    const lastChat = () => commands.filter(c => c.type === 'chat').at(-1)!;
    assert.equal(lastChat().text, '/sit', "the story's own commands are sent as written (the server reads them)");
    assert.ok(!commands.some(c => c.type === 'action' && c.action === 'sit'));
    typed(s, '/pose stretches out');
    s.composerKey({code: 'Enter'});
    assert.equal(lastChat().text, '/pose stretches out');

    typed(s, '/inventory');
    s.composerKey({code: 'Enter'});
    assert.equal(s.modal, 'inventory', 'the pages open too');
});

test('slash commands: only where they can be done, and a Dungeon Master\'s console keeps its own names', () => {
    const {state: s, commands, snapshot} = onTheMap({nearBoard: true, commonRoom: true});
    snapshot.wild = {hunt: true, forage: true, forageWhat: 'roots'};
    s.applySnapshot(snapshot);
    const names = slashSuggestions(s, '/').map(c => c.name);
    for (const name of ['forage', 'hunt', 'board', 'perform']) assert.ok(names.includes(name), `/${name} here`);
    typed(s, '/forage');
    s.composerKey({code: 'Enter'});
    assert.deepEqual(commands.at(-1), {type: 'forage'});
    typed(s, '/board');
    s.composerKey({code: 'Enter'});
    assert.deepEqual(commands.at(-1), {type: 'board', verb: 'read'});

    (snapshot.self as Json).dungeonMaster = true;
    s.applySnapshot(snapshot);
    s.devCommands = [['/hunt', 'Start a test hunt']];
    typed(s, '/hunt');
    s.composerKey({code: 'Enter'});
    assert.ok(commands.some(c => c.type === 'dev' && c.command === '/hunt'), "the console's /hunt goes to the console");
    assert.ok(!commands.some(c => c.type === 'hunt'), 'not out after game');
    typed(s, '/listen');
    s.composerKey({code: 'Enter'});
    assert.deepEqual(commands.at(-1), {type: 'action', action: 'listen', target: ''}, 'a game command still runs for a Dungeon Master');
});

test('the wheel: a right-click on the ground offers what the actions panel did, on a wolf its actions; a click or a digit runs one', () => {
    const {state: s, commands} = onTheMap();
    s.mouseDown([300, 300], false, false, false);
    assert.equal(s.contextTarget, GroundTarget, 'the ground\'s wheel opens');
    assert.deepEqual(s.contextPoint, [300, 300], 'at the pointer');
    assert.deepEqual(s.contextActions, wheelCommands(s).map(c => c.name));
    for (const name of ['listen', 'look', 'smell', 'wait', 'sit', 'lay', 'howl', 'groom']) assert.ok(s.contextActions.includes(name), `${name} on it`);
    assert.ok(!s.contextActions.includes('stand'), 'standing up only when sitting or lying');
    assert.ok(!commands.some(c => c.type === 'path'), 'a right-click no longer walks');

    s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'smell'});
    assert.deepEqual(commands.at(-1), {type: 'action', action: 'smell', target: ''});
    assert.equal(s.contextTarget, '', 'and closes');

    s.mouseDown([300, 300], false, false, false);
    s.keyDown({code: `Digit${s.contextActions.indexOf('lay') + 1}`});
    assert.deepEqual(commands.at(-1), {type: 'action', action: 'lay', target: ''}, 'its digit runs it');
    s.mouseDown([300, 300], false, false, false);
    s.keyDown({code: 'Escape'});
    assert.equal(s.contextTarget, '', 'Esc closes it');

    // On a wolf: its own wheel, the actions the server gives.
    s.hits.push({rect: rect(160, 100, 180, 120), action: 'target', target: 'bram'});
    s.mouseDown([170, 110], false, false, false);
    assert.equal(s.contextTarget, 'bram');
    assert.deepEqual(s.contextActions, ['talk', 'inspect', 'trade']);
    assert.deepEqual(s.contextPoint, [170, 110]);
    s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'inspect'});
    assert.deepEqual(commands.at(-1), {type: 'action', action: 'inspect', target: 'bram'});
    s.mouseDown([170, 110], false, false, false);
    s.keyDown({code: 'Digit1'});
    assert.deepEqual(commands.at(-1), {type: 'action', action: 'talk', target: 'bram'}, 'its digits too');

    // A left click still walks, and no wheel in a fight or on the World Map.
    s.mouseDown([300, 300], true, false, false);
    assert.equal(commands.at(-1)!.type, 'path');
    s.worldMap = true;
    s.mouseDown([300, 300], false, false, false);
    assert.equal(s.contextTarget, '', 'not on the World Map');
});

test('the wheel and the list agree: every wheel command is a slash command that runs here', () => {
    for (const c of Commands.filter(c => c.wheel)) assert.ok(c.run, `/${c.name} runs from the wheel`);
    const names = Commands.map(c => c.name);
    assert.equal(new Set(names).size, names.length, 'no name twice');
});

test('the surroundings bar\'s pictures: the time of day, the weather, the senses, and what a wolf wears', () => {
    assert.equal(timeOfDay(6, 'dawn').icon, 'morning');
    assert.equal(timeOfDay(8.5, 'day').icon, 'morning');
    assert.equal(timeOfDay(12.5, 'day').icon, 'sun');
    assert.equal(timeOfDay(16, 'day').icon, 'late');
    assert.equal(timeOfDay(18.5, 'dusk').icon, 'late');
    assert.equal(timeOfDay(23, 'night').icon, 'moon');
    assert.deepEqual(weatherIcon('rain', 0.9, true), {icon: 'heavyRain', words: 'Heavy rain'});
    assert.deepEqual(weatherIcon('rain', 0.2, true), {icon: 'rain', words: 'Light rain'});
    assert.equal(weatherIcon('overcast', 0.3, true).icon, 'thinCloud');
    assert.equal(weatherIcon('storm', 1, false).icon, 'sheltered', 'under a roof, sheltered');
    assert.equal(weatherIcon('clear', 0, true).words, 'Clear skies');
    assert.deepEqual(senseLevel('sight', 1), {level: 'keen', words: 'Sight: clear (100%)'});
    assert.equal(senseLevel('hearing', 0.5).level, 'dulled');
    assert.equal(senseLevel('scent', 0.2).words, 'Scent: very poor (20%)');
    assert.equal(gearIconFor('head', 'Wide straw sun hat', false), 'strawHat');
    assert.equal(gearIconFor('head', 'Leather cap with ear slits', false), 'leatherHelm');
    assert.equal(gearIconFor('head', 'Kettle helm', false), 'steelHelm');
    assert.equal(gearIconFor('body', 'Mail coat', false), 'mail');
    assert.equal(gearIconFor('body', 'Boiled-leather barding', false), 'leatherArmour');
    assert.equal(gearIconFor('mouth', 'Steel bit-sword', true), 'sword');
    assert.equal(gearIconFor('mouth', 'Needle rapier', true), 'rapier');
    assert.equal(gearIconFor('mouth', 'Fang knife', true), 'knife');
    assert.equal(gearIconFor('mouth', 'Pitchfork', false), 'pitchfork');
    assert.equal(gearIconFor('paws', 'Steel claw caps', true), 'claws');
    assert.equal(gearIconFor('neck', 'Steel gorget', false), 'collar');
});

test('the story\'s faces: a line from a wolf in sight carries its look; the world\'s, a hidden voice\'s and a shared name\'s none', () => {
    const {state: s, snapshot} = onTheMap();
    const look = {fur: {base: '#808080'}};
    (snapshot.entities as Json[])[1].appearance = look;
    s.applySnapshot(snapshot);
    s.receiveEvent({type: 'roleplay', id: 'r1', channel: 'ic', speaker: 'Bram', text: '"Morning."', segments: [{kind: 'speech', text: 'Morning.'}]});
    assert.deepEqual(s.posts.at(-1)!.face, {appearance: look, artwork: ''}, 'by the name it is known by here');
    s.receiveEvent({type: 'roleplay', id: 'r2', channel: 'ic', speaker: 'A voice', anonymous: true, text: 'Hm.'});
    assert.equal(s.posts.at(-1)!.face, undefined, 'a voice not put to a wolf');
    s.receiveEvent({type: 'system', id: 'r3', text: 'The bell rings.'});
    assert.equal(s.posts.at(-1)!.face, undefined, "the world's own lines");
    (snapshot.entities as Json[]).push({id: 'bram2', name: 'Bram (2)', x: 9, y: 9, npc: true, appearance: {fur: {base: '#303030'}}});
    s.applySnapshot(snapshot);
    s.receiveEvent({type: 'roleplay', id: 'r4', channel: 'ic', speaker: 'Bram (2)', text: 'And again.'});
    assert.deepEqual(s.posts.at(-1)!.face?.appearance, {fur: {base: '#303030'}}, 'strangers alike, numbered as in sight: that one');
    s.receiveEvent({type: 'roleplay', id: 'r5', channel: 'ic', speaker: 'Bram', text: 'Again.'});
    assert.deepEqual(s.posts.at(-1)!.face?.appearance, look, 'and the first');
    (snapshot.entities as Json[]).push({id: 'bram3', name: 'Bram', x: 10, y: 9, npc: true, appearance: look});
    s.applySnapshot(snapshot);
    s.receiveEvent({type: 'roleplay', id: 'r6', channel: 'ic', speaker: 'Bram', text: 'Who?'});
    assert.equal(s.posts.at(-1)!.face, undefined, 'two in sight by one name: no guessing');
});
