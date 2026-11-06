import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {bandAt, confirmText, heldText, leanToRule, leanWords, measureLines, preview, ruleToLean, snapLean, steerAt, steerChange,
    type SteeringView} from './steering.ts';

// Steering the economy (Docs/Design/74-steering-the-economy.md, 10): what the DM's Steering panel reads and sends.
const file = JSON.parse(readFileSync(new URL('../../../Data/Items/city_loop.json', import.meta.url), 'utf8'));
const view: SteeringView = {
    settings: {steer: true, lean: 0, neverHungry: false, neverBroke: false},
    words: {left: 'Shared: money and food equal across all wolves', centre: 'Today\'s economy', right: 'Held by the rich: the upper class and the Houses'},
    held: ['surplusKeep', 'treasuryCap', 'surplusAlmsTo'], own: {steerWidthMid: 15, steerWidthEnd: 30},
    table: {...file.steering, marks: [-1, -.5, 0, .5, 1]}, band: [-.15, .15], targets: {},
    levers: [{key: 'surplusKeep', label: 'The treasury\'s keep', unit: 'p', own: 4000, centre: 4000, low: 2950, high: 4000, now: 4000},
        {key: 'groundRent', label: 'The ground rent', unit: '%', own: 0, centre: 0, low: 0, high: 6}],
    saved: '2026-11-06', problem: '', changes: [],
    now: {on: true, push: 0, judged: 56, days: 60, tollLean: 0, means: {gini: .49, upper: 13.1, top10: 34, hungry: .01, short: .4, treasury: 1690}},
};

test('the slider: -100..+100 on the tab, 0..200 in the rule, in fives', () => {
    assert.equal(leanToRule(-40), 60);
    assert.equal(ruleToLean(200), 100);
    assert.equal(snapLean(-42), -40);
    assert.equal(snapLean(130), 100);
    assert.equal(leanWords(view, -100), 'Shared: money and food equal across all wolves');
    assert.equal(leanWords(view, 0), 'Today\'s economy');
    assert.equal(leanWords(view, 100), 'Held by the rich: the upper class and the Houses');
    assert.equal(leanWords(view, 35), 'Leaning to the rich (+35)');
});

test('the table read as Core reads it (the shared cases)', () => {
    const cases = JSON.parse(readFileSync(new URL('../../../Tests/fixtures/steer_table.json', import.meta.url), 'utf8')).cases;
    for (const c of cases) {
        const got = steerAt(file.steering[c.kind][c.name], c.at, c.own);
        if (c.want === null) assert.equal(got, null, JSON.stringify(c));
        else assert.ok(Math.abs((got as number) - c.want) < 1e-9, JSON.stringify(c));
    }
    assert.deepEqual(bandAt(view.own, 0), [-.15, .15]);
    assert.deepEqual(bandAt(view.own, -100), [-1, -.7]);
});

test('dragging shows what a place would mean, before it is saved', () => {
    const p = preview(view, -50);
    assert.equal(p.gini, .41);
    assert.equal(p.upper, null, 'the upper share watched left of the centre');
    const keep = p.levers.find(l => l.key === 'surplusKeep')!;
    assert.deepEqual([keep.low, keep.high], [550, 2350], 'the keep\'s bounds at -50: the table at the band\'s two ends');
    const right = preview(view, 100).levers.find(l => l.key === 'groundRent')!;
    assert.deepEqual([right.low, right.high], [21, 30]);
});

test('the measures against their targets', () => {
    const lines = measureLines(view);
    assert.equal(lines[0].target, '0.487 ± 0.035');
    assert.equal(lines[0].inside, true);
    assert.equal(lines[1].target, '13.0 ± 2.0');
    assert.equal(lines[3].inside, true, 'hunger under its ceiling');
});

test('only the four settings are sent, and only when they change something', () => {
    assert.equal(steerChange(view, {lean: 0}), null);
    assert.deepEqual(steerChange(view, {lean: -42}), {lean: -40});
    assert.deepEqual(steerChange(view, {neverBroke: true}), {neverBroke: true});
    assert.equal(steerChange(view, {steer: true}), null);
    assert.match(confirmText(view, {lean: -40}), /from 0 to -40\? The steering moves its levers toward it/);
    assert.match(confirmText(view, {steer: false}), /back to the world's own/);
});

test('a rule the steering holds is read only while it is on', () => {
    assert.equal(heldText(view, 'surplusKeep', 4000), 'steered: 4000; yours: 4000, used again when the steering is off');
    assert.equal(heldText(view, 'almsADay', 10), null);
    assert.equal(heldText({...view, settings: {...view.settings, steer: false}}, 'surplusKeep', 4000), null);
});
