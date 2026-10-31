import {test} from 'node:test';
import assert from 'node:assert/strict';
import {allFrom, buildChoices, groupByLabel, prunePicks, rowChangeText, sideChangeText, summaryLine, summaryRows, summaryTotal, valueText,
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
