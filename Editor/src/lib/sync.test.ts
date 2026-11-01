import {test} from 'node:test';
import assert from 'node:assert/strict';
import {allFrom, buildChoices, defaultPicks, differentMapText, fieldSide, gameUrl, goneText, groupByLabel, letGoText, middleOf, prunePicks,
    replaceMapText, rowChangeText, sideChangeText,
    summaryLine, summaryRows, summaryTotal, valueText,
    type SyncField, type SyncItem} from './sync.ts';

const field = (name: string, changed: SyncField['changed'], base: unknown = 'b', dev: unknown = 'd', live: unknown = 'l'): SyncField =>
    ({field: name, base, dev, live, changed});
const item = (id: string, fields: SyncField[], over: Partial<SyncItem> = {}): SyncItem => ({
    id, table: 'live.npcs', label: 'NPCs', part: 'live', key: [id.split('/')[1]], name: id, side: 'conflict',
    dev: 'changed', live: 'changed', fields, fieldChoices: true, ...over});

// Wren: both renamed her (a real conflict), dev changed her greeting, live her purse; the two agree on her age.
const wren = item('live.npcs/wren', [field('name', 'conflict'), field('greeting', 'dev'), field('purse', 'live'), field('age', 'same')]);
// Sloe: removed in dev, changed live: only a whole-row choice.
const sloe = item('live.npcs/sloe', [field('greeting', null)], {dev: 'removed', fieldChoices: false});

test('whole-row picks become "dev" or "live"; a conflict without a pick is missing', () => {
    assert.deepEqual(buildChoices([wren, sloe], {}), {choices: {}, missing: ['live.npcs/wren', 'live.npcs/sloe']});
    assert.deepEqual(buildChoices([wren, sloe], {'live.npcs/wren': {whole: 'dev'}, 'live.npcs/sloe': {whole: 'live'}}),
        {choices: {'live.npcs/wren': 'dev', 'live.npcs/sloe': 'live'}, missing: []});
    assert.deepEqual(buildChoices([wren, sloe], allFrom([wren, sloe], 'live')).choices, {'live.npcs/wren': 'live', 'live.npcs/sloe': 'live'});
});

test('field by field names the fields both changed, and a one-sided field only when taken from the other side', () => {
    assert.deepEqual(buildChoices([wren], {'live.npcs/wren': {byField: true}}).missing, ['live.npcs/wren'], 'the name is unchosen');
    assert.deepEqual(buildChoices([wren], {'live.npcs/wren': {byField: true, fields: {name: 'live'}}}),
        {choices: {'live.npcs/wren': {name: 'live'}}, missing: []}, "greeting goes dev's way, purse live's, unchosen");
    assert.deepEqual(buildChoices([wren], {'live.npcs/wren': {byField: true, fields: {name: 'dev', greeting: 'dev', purse: 'dev'}}}).choices,
        {'live.npcs/wren': {name: 'dev', purse: 'dev'}}, 'greeting already goes dev\'s way: not named');
    assert.deepEqual(buildChoices([wren], {'live.npcs/wren': {byField: true, whole: 'live', fields: {name: 'dev'}}}).choices,
        {'live.npcs/wren': {name: 'dev'}}, 'field by field wins over an earlier whole pick');
    // A row added on both sides (no base): every differing field must be chosen.
    const both = item('live.npcs/bo', [field('name', 'conflict', null), field('purse', 'conflict', null)], {dev: 'added', live: 'added'});
    assert.deepEqual(buildChoices([both], {'live.npcs/bo': {byField: true, fields: {name: 'dev'}}}).missing, ['live.npcs/bo']);
});

test('a field-by-field pick on a row removed on one side falls back to its whole pick', () => {
    assert.deepEqual(buildChoices([sloe], {'live.npcs/sloe': {byField: true, fields: {greeting: 'dev'}}}).missing, ['live.npcs/sloe']);
    assert.deepEqual(buildChoices([sloe], {'live.npcs/sloe': {byField: true, whole: 'dev'}}).choices, {'live.npcs/sloe': 'dev'});
});

test('picks for conflicts that went away are dropped when the plan is made again', () => {
    const now = item('live.npcs/wren', [field('name', 'conflict'), field('purse', 'same')]);
    const picks = {'live.npcs/wren': {byField: true, fields: {name: 'dev' as const, greeting: 'dev' as const}}, 'live.npcs/gone': {whole: 'dev' as const}};
    assert.deepEqual(prunePicks(picks, [now]), {'live.npcs/wren': {byField: true, fields: {name: 'dev'}}});
    assert.deepEqual(buildChoices([now], prunePicks(picks, [now])).choices, {'live.npcs/wren': {name: 'dev'}});
    assert.deepEqual(prunePicks({'live.npcs/sloe': {byField: true, whole: 'live'}}, [sloe]), {'live.npcs/sloe': {whole: 'live'}});
});

test('values, groups and summaries as the screens show them', () => {
    assert.equal(valueText({$long: 4096, digest: 'abc123def456'}), 'long value (4096 characters, digest abc123def456)');
    assert.equal(valueText(null), '—');
    assert.equal(valueText(''), '""');
    assert.equal(valueText([1, 2]), '[1,2]');
    assert.equal(valueText('Wren'), 'Wren');
    const cell = item('world.cells/c1', [], {label: 'cells', part: 'world', side: 'live'});
    assert.deepEqual(groupByLabel([wren, cell, sloe]).map(g => [g.label, g.items.length]), [['NPCs', 2], ['cells', 1]]);
    assert.deepEqual(summaryRows({world: {cells: {added: 2, changed: 0, removed: 1}}, live: {NPCs: {added: 0, changed: 0, removed: 0}}}),
        [['cells', '2 added, 1 removed']]);
    const summary = {world: {cells: {added: 2, changed: 0, removed: 1}}, live: {NPCs: {added: 0, changed: 4, removed: 0}}};
    assert.equal(summaryTotal(summary), 7);
    assert.equal(summaryLine(summary), '3 cells, 4 NPCs');
    assert.equal(summaryLine(undefined), 'none');
    assert.equal(rowChangeText(sloe), 'removed in dev, changed live');
    assert.equal(sideChangeText(wren, 'live'), 'changed: name, purse');
    assert.equal(sideChangeText(sloe, 'dev'), 'removed');
});

// The same map (doc 66): Update from live between two maps says so, naming both; the DM's import asks to replace the map.
const maps = {dev: {id: 'map-20261031-a', name: 'Ridgemere 2.0: the city'}, live: {id: 'original:upper_accord', name: "Ridgemere's original map"},
    same: false};

test('two maps: Update from live has nothing to do, and says why in plain words', () => {
    assert.equal(differentMapText(maps, 'dev', 'prod'),
        "PROD is on a different map (Ridgemere's original map) from DEV (Ridgemere 2.0: the city): there is nothing to update from " +
        "until DEV's map is imported into PROD (the DM's Import from dev, replacing PROD's map).");
    assert.equal(differentMapText({...maps, same: true}, 'dev', 'prod'), null, 'one map: nothing to say');
    assert.equal(differentMapText(undefined, 'dev', 'prod'), null);
});

test("two maps: the import warns, and asks to replace live's map by name", () => {
    const {warning, confirm} = replaceMapText(maps, 'dev', 'prod');
    assert.match(warning, /^DEV is on a different map \(Ridgemere 2\.0: the city\) from PROD \(Ridgemere's original map\)\./);
    assert.match(warning, /Players' data and everyone in PROD are kept/);
    assert.equal(confirm, "Replace PROD's map (Ridgemere's original map) with DEV's (Ridgemere 2.0: the city)");
});

test('what a new map lets go of, in a line', () => {
    assert.equal(letGoText(undefined), '');
    assert.equal(letGoText({npcs: 0, slots: 0, posts: 0, claims: 0, npcAreas: 0}), '');
    assert.equal(letGoText({npcs: 2, slots: 1, posts: 3, claims: 1, npcAreas: 0, names: ['Briar', 'Rook']}),
        '2 NPCs and 1 profession slot without a place on the new map (Briar, Rook), a decision each; 3 patrol posts and 1 territory claim let go');
    assert.equal(letGoText({npcs: 0, slots: 0, posts: 0, claims: 2, npcAreas: 1}), '2 territory claims and 1 NPC area let go');
});

test('Play opens the running server at the address the studio was reached by, or at its own computer', () => {
    assert.equal(gameUrl({port: 7788, host: null}, {hostname: '192.168.0.121'}), 'http://192.168.0.121:7788/',
        'from another computer on the network: the studio computer\'s address, not 127.0.0.1');
    assert.equal(gameUrl({port: 7789, host: null}, {hostname: '127.0.0.1'}), 'http://127.0.0.1:7789/');
    assert.equal(gameUrl({port: 7788, host: 'tower'}, {hostname: '192.168.0.121'}), 'http://tower:7788/', 'a server on another computer');
});

// The live world's truth (doc 66, Progress): the game's fields start on live's; a new map's missing places need a decision.
const game = (name: string, changed: SyncField['changed'], dflt: SyncField['default']): SyncField =>
    ({...field(name, changed), owner: 'game', default: dflt});
// Maple: DEV moved her home and changed her words; PROD's game moved her too.
const maple = item('live.npcs/maple', [game('home_area', 'conflict', 'live'), game('home_y', 'conflict', 'live'),
    {...field('greeting', 'dev'), owner: 'authored', default: 'dev'}], {default: {home_area: 'live', home_y: 'live'}});
// Linden: removed in dev, moved by live's game: live's by default, a whole-row choice.
const linden = item('live.npcs/linden', [game('home_area', null, null)], {dev: 'removed', fieldChoices: false, default: 'live'});
// Clover, born live in a cottage the new map hasn't: no default, a decision for each place.
const clover = item('live.npcs/born_c', [], {dev: 'absent', live: 'added', fieldChoices: false, default: null, placeChoices: true,
    gone: [{place: 'home', area: 'cottage', dev: null}, {place: 'work', area: 'cottage', dev: null}]});
// Rook: his evenings in the cottage live; dev has him, with his evenings in the town square.
const rook = item('live.npcs/rook', [], {dev: 'unchanged', live: 'changed', fieldChoices: false, default: null, placeChoices: true,
    gone: [{place: 'evening', area: 'cottage', dev: {cell: 'town', x: 4, y: 5}}]});

test("conflicts over the game's fields start on live's and need no pick", () => {
    assert.deepEqual(defaultPicks([maple, linden, wren, clover]),
        {'live.npcs/maple': {byField: true, fields: {home_area: 'live', home_y: 'live'}}, 'live.npcs/linden': {whole: 'live'}},
        'wren (an authored field both changed) and clover (a place gone) start unpicked');
    assert.deepEqual(buildChoices([maple, linden], {}), {choices: {}, missing: []}, 'no pick: the engine takes the default');
    assert.deepEqual(buildChoices([maple], defaultPicks([maple])).choices, {'live.npcs/maple': {home_area: 'live', home_y: 'live'}});
    assert.deepEqual(buildChoices([maple], {'live.npcs/maple': {byField: true, fields: {home_area: 'dev', home_y: 'dev'}}}).choices,
        {'live.npcs/maple': {home_area: 'dev', home_y: 'dev'}}, "dev's, chosen");
    assert.deepEqual(buildChoices([maple], {'live.npcs/maple': {byField: true}}), {choices: {'live.npcs/maple': {}}, missing: []},
        "an unchosen field of the game's goes live's way");
    assert.equal(fieldSide(maple.fields[0], {byField: true}), 'live');
    assert.equal(fieldSide(wren.fields[0], {byField: true}), undefined, 'an authored field both changed waits for the user');
});

test("a place the new map hasn't needs a decision: unplace, dev's, or a new place", () => {
    assert.deepEqual(buildChoices([clover, rook], {}).missing, ['live.npcs/born_c', 'live.npcs/rook']);
    assert.deepEqual(buildChoices([rook], {'live.npcs/rook': {whole: 'live'}}).missing, ['live.npcs/rook'], "live's place is gone");
    assert.deepEqual(buildChoices([clover], {'live.npcs/born_c': {places: {home: {cell: 'shop', x: 3, y: 3}}}}).missing, ['live.npcs/born_c'],
        'every gone place');
    assert.deepEqual(buildChoices([clover, rook], {'live.npcs/born_c': {places: {home: {cell: 'shop', x: 3, y: 3}, work: 'unset'}},
        'live.npcs/rook': {places: {evening: 'dev'}}}).choices,
        {'live.npcs/born_c': {home: {cell: 'shop', x: 3, y: 3}, work: 'unset'}, 'live.npcs/rook': {evening: 'dev'}});
    assert.deepEqual(buildChoices([clover], {'live.npcs/born_c': {whole: 'unplace'}}).choices, {'live.npcs/born_c': 'unplace'});
    assert.deepEqual(prunePicks({'live.npcs/born_c': {places: {home: 'unset', evening: 'unset'}}}, [clover]),
        {'live.npcs/born_c': {places: {home: 'unset'}}}, 'a place no longer gone is forgotten');
    assert.equal(goneText(clover), 'home (cottage), work (cottage)');
    assert.deepEqual(middleOf({id: 'shop', name: 'Shop', kind: 'interior', width: 20, height: 12}), {cell: 'shop', x: 10, y: 6});
});
