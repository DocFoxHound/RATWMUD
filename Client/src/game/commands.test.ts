// The slash commands and the wheel a right-click opens (game/commands.ts; the user's notes, 2026-11-14): everything the
// old ACTIONS panel held is a command in the box and on the ground's wheel; a wolf's wheel is its actions.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {testGame} from './testing.ts';
import {Commands, parseSlash, slashSuggestions, wheelCommands} from './commands.ts';
import {actionWords, wheelEntries} from './wheel.ts';
import {basketTotals, shopOf} from './shop.ts';
import {GroundTarget} from './state.ts';
import {rect} from '../ui/painter.ts';
import {str, type Json} from './json.ts';
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
    for (const name of ['listen', 'look', 'smell', 'track', 'wait', 'sit', 'lay', 'howl', 'groom', 'pose', 'me', 'character', 'inventory', 'map'])
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
    typed(s, '/track');
    s.composerKey({code: 'Enter'});
    assert.equal(s.tracking, true);
    assert.deepEqual(commands.at(-1), {type: 'action', action: 'smell', target: ''});

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

    // Its first ring is kinds (game/wheel.ts): a digit opens one, a digit there runs its action.
    s.mouseDown([300, 300], false, false, false);
    assert.deepEqual(wheelEntries(s).map(e => e.words), ['Social', 'Senses', 'Rest'], 'the ground\'s kinds, only those with something in them');
    s.keyDown({code: `Digit${wheelEntries(s).findIndex(e => e.id === 'rest') + 1}`});
    assert.equal(s.contextCategory, 'rest');
    assert.deepEqual(wheelEntries(s).map(e => e.words), ['Wait', 'Sit', 'Lie down', 'Groom']);
    s.keyDown({code: `Digit${wheelEntries(s).findIndex(e => e.id === 'lay') + 1}`});
    assert.deepEqual(commands.at(-1), {type: 'action', action: 'lay', target: ''}, 'its digit runs it');
    s.mouseDown([300, 300], false, false, false);
    s.activate({rect: rect(0, 0, 0, 0), action: 'wheel_group', target: 'senses'});
    s.keyDown({code: 'Escape'});
    assert.ok(s.contextTarget === GroundTarget && (s.contextCategory as string) === '', 'Esc inside a kind goes back to the kinds');
    s.keyDown({code: 'Escape'});
    assert.equal(s.contextTarget, '', 'and then closes it');

    // On a wolf: its own wheel, the actions the server gives.
    s.hits.push({rect: rect(160, 100, 180, 120), action: 'target', target: 'bram'});
    s.mouseDown([170, 110], false, false, false);
    assert.equal(s.contextTarget, 'bram');
    assert.deepEqual(s.contextActions, ['talk', 'inspect', 'trade']);
    assert.deepEqual(s.contextPoint, [170, 110]);
    s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'inspect'});
    assert.deepEqual(commands.at(-1), {type: 'action', action: 'inspect', target: 'bram'});
    s.mouseDown([170, 110], false, false, false);
    assert.deepEqual(wheelEntries(s).map(e => [e.words, e.hint]), [['Trade', 'Buy and sell'], ['Social', 'Talk'], ['Look', 'Inspect']],
        'a keeper\'s kinds, Trade first, each with its one action under it');
    s.keyDown({code: 'Digit2'});
    assert.deepEqual(commands.at(-1), {type: 'action', action: 'talk', target: 'bram'}, 'a kind of one action does it at once, by its digit too');

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

test('the story\'s faces: from the speaker the line names by id, never by a name; none for the world, a hidden voice or an older server', () => {
    const {state: s, snapshot} = onTheMap();
    const look = {fur: {base: '#808080'}}, other = {fur: {base: '#303030'}};
    const bram = (snapshot.entities as Json[])[1];
    bram.appearance = look;
    // Two in sight by one name: each line still has its own speaker's face.
    (snapshot.entities as Json[]).push({id: 'bram2', name: str(bram, 'name'), x: 9, y: 9, npc: true, appearance: other});
    s.applySnapshot(snapshot);
    const said = (id: string, extra: Json) => s.receiveEvent({type: 'roleplay', id, channel: 'ic', speaker: str(bram, 'name'), text: 'Hm.', ...extra});
    said('r1', {speakerId: str(bram, 'id')});
    assert.equal(s.posts.at(-1)!.speakerId, str(bram, 'id'));
    assert.deepEqual(s.posts.at(-1)!.face, {appearance: look, artwork: ''}, 'by its id, as In Sight has it');
    said('r2', {speakerId: 'bram2'});
    assert.deepEqual(s.posts.at(-1)!.face?.appearance, other, 'the other of the same name: its own face');
    said('r3', {speakerId: 'gone', appearance: {fur: {base: '#101010'}}, artwork: 'art-1'});
    assert.deepEqual(s.posts.at(-1)!.face, {appearance: {fur: {base: '#101010'}}, artwork: 'art-1'}, 'one not in the list: the look the line carries');
    said('r4', {speakerId: 'bram2', appearance: look});
    assert.deepEqual(s.posts.at(-1)!.face?.appearance, look, 'the line\'s look first (as it was when said)');
    said('r5', {});
    assert.equal(s.posts.at(-1)!.face, undefined, 'no id (an older server, or a speaker not seen): no face, no guess by name');
    s.receiveEvent({type: 'roleplay', id: 'r6', channel: 'ic', speaker: 'A voice', anonymous: true, text: 'Hm.', speakerId: 'bram2'});
    assert.equal(s.posts.at(-1)!.face, undefined, 'a voice not put to a wolf');
    s.receiveEvent({type: 'system', id: 'r7', text: 'The bell rings.'});
    assert.equal(s.posts.at(-1)!.face, undefined, "the world's own lines");
});

test('the wheel in categories, in plain words: no ids, nothing twice (the user, 2026-11-15)', () => {
    const {state: s} = onTheMap();
    const keeper = {id: 'ivo', name: 'Ivo Tarmarsh', x: 6, y: 5, npc: true,
        actions: ['inspect', 'talk', 'trade', 'steal', 'attack', 'ask for work', 'take k458', 'take k459', 'take k460', 'apprentice',
            'introduce', 'ask to lodge', 'groom', 'give', 'talk', 'deliver m3', 'hire for 5p a day'],
        words: {'take k458': 'Take on: Clear the bandits from the east road (20p)', 'take k459': 'Take on: Carry a letter to Larkfield (6p)',
            'deliver m3': 'Deliver 3 bread'}};
    s.applySnapshot({...s.snapshot!, entities: [{id: 'me', name: 'Ash', x: 5, y: 5}, keeper]});
    s.openContextAt('ivo', [400, 300]);
    const groups = wheelEntries(s);
    assert.deepEqual(groups.map(g => g.words), ['Trade', 'Social', 'Party', 'Work', 'Items', 'Action', 'Look']);
    const all = groups.flatMap(g => g.actions);
    assert.equal(all.filter(a => a === 'talk').length, 1, 'talk once');
    assert.ok(!all.includes('ask for work'), '"Ask for work" goes beside the work itself');
    const words = all.map(a => actionWords(s, a));
    for (const w of words) assert.doesNotMatch(w, /\b[a-z]{1,4}\d+\b/i, `no raw id in "${w}"`);
    assert.ok(words.includes('Take on: Clear the bandits from the east road (20p)'), 'the server\'s words for the work');
    assert.ok(words.includes('Take on work'), 'work without words from the server: the verb, never "Take k460"');
    assert.ok(words.includes('Deliver 3 bread'));
    // Into Work and back: its own actions, numbered from 1.
    s.activate({rect: rect(0, 0, 0, 0), action: 'wheel_group', target: 'work'});
    assert.deepEqual(wheelEntries(s).map(e => e.id), ['take k458', 'take k459', 'take k460', 'apprentice', 'deliver m3']);
    s.activate({rect: rect(0, 0, 0, 0), action: 'wheel_back', target: ''});
    assert.equal(s.contextCategory, '');
    // A challenge's terms are a flat list of their own.
    s.contextActions = ['challenge'];
    s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'challenge'});
    assert.equal(wheelEntries(s).length, 4);
    assert.equal(wheelEntries(s)[0].words, 'Duel until one yields');
});

test('Trade opens the shop window: from afar it walks over first; a basket priced by the server, confirmed as trades', () => {
    const {state: s, commands} = onTheMap({cash: 30});
    // Out of reach (no keeper's list in the snapshot): walk over, and no window yet.
    s.openContextAt('bram', [400, 300]);
    s.activate({rect: rect(0, 0, 0, 0), action: 'context', target: 'trade'});
    assert.ok(commands.some(c => c.type === 'shop' && c.target === 'bram'), 'the server told whose list');
    assert.deepEqual(commands.filter(c => c.type === 'path').at(-1), {type: 'path', x: 8, y: 5}, 'walking over to the keeper');
    assert.equal(s.modal, '', 'no window until within reach');
    // Arrived: the keeper's list comes, and the window opens on it.
    const bread = {id: 'bread', name: 'Bread', quality: 'Common', stock: 30, owned: 4, buyPrice: 2, sellPrice: 1, canBuy: true, canSell: true,
        listCount: 1, listPrice: 2, buyCount: 1, buyLotPrice: 2, canBuyLot: true, buyReason: 'Available', sellLot: 2, sellLotPrice: 1,
        canSellLot: true, sellLotReason: 'Available', trend: 0};
    const oil = {id: 'masking_oil', name: 'Scent-masking oil', stock: 0, owned: 0, buyPrice: 38, sellPrice: 5, canBuy: false, canSell: false,
        buyReason: 'There is not enough stock.', sellLot: 1, canSellLot: false, sellLotReason: 'There is not enough stock.'};
    const merchant = {id: 'bram', name: 'Bram', cash: 90, staged: true, buyingToday: 40, items: [bread, oil]};
    s.applySnapshot({...s.snapshot!, merchant});
    assert.equal(s.modal, 'trade', 'the shop window');
    const shop = shopOf(s)!;
    assert.deepEqual(shop.buy.map(r => [r.id, r.ok]), [['bread', true], ['masking_oil', false]], 'their goods; the empty shelf off');
    assert.equal(shop.buy[1].reason, 'Out of stock.');
    assert.deepEqual(shop.sell.map(r => [r.id, r.step]), [['bread', 2]], 'what one carries that they buy, by their lot');
    // A basket: three loaves bought, two sold; each line quoted by the server.
    commands.length = 0;
    s.activate({rect: rect(0, 0, 0, 0), action: 'shop_qty', target: 'buy|bread|3'});
    s.activate({rect: rect(0, 0, 0, 0), action: 'shop_qty', target: 'sell|bread|3'});
    assert.deepEqual(commands.filter(c => c.type === 'trade_quote'), [
        {type: 'trade_quote', target: 'bram', item: 'bread', quantity: 3, buy: true},
        {type: 'trade_quote', target: 'bram', item: 'bread', quantity: 2, buy: false}], 'priced by the server (sold by the pair)');
    assert.equal(basketTotals(s, shopOf(s)!).ready, false, 'not before the quotes');
    s.receiveEvent({type: 'tradeQuote', target: 'bram', item: 'bread', buy: true, quantity: 3, ok: true, total: 7, text: 'Available'});
    s.receiveEvent({type: 'tradeQuote', target: 'bram', item: 'bread', buy: false, quantity: 2, ok: true, total: 1, text: 'Available'});
    const t = basketTotals(s, shopOf(s)!);
    assert.deepEqual([t.pay, t.get, t.after, t.ready], [7, 1, 24, true]);
    commands.length = 0;
    s.activate({rect: rect(0, 0, 0, 0), action: 'shop_confirm', target: ''});
    assert.deepEqual(commands, [{type: 'trade', target: 'bram', item: 'bread', quantity: 2, buy: false},
        {type: 'trade', target: 'bram', item: 'bread', quantity: 3, buy: true}], 'sold first, then bought');
    assert.equal(s.shopBasket.size, 0);
    // Refusals come back plainly, as receipts.
    s.receiveEvent({type: 'tradeResult', target: 'bram', item: 'bread', buy: true, quantity: 3, ok: false, text: 'You cannot afford that purchase.'});
    assert.deepEqual(s.shopReceipts.at(-1), {ok: false, text: 'You cannot afford that purchase.'});
    // Too dear for the purse: no confirming.
    s.activate({rect: rect(0, 0, 0, 0), action: 'shop_qty', target: 'buy|bread|20'});
    s.receiveEvent({type: 'tradeQuote', target: 'bram', item: 'bread', buy: true, quantity: 20, ok: false, total: 44, text: 'You cannot afford that purchase.'});
    const dear = basketTotals(s, shopOf(s)!);
    assert.ok(!dear.ready && /cannot afford/.test(dear.why), dear.why);
    // The keeper gone (asleep, away): the window says the counter is unattended.
    s.applySnapshot({...s.snapshot!, merchant: undefined});
    assert.equal(shopOf(s), null);
});
