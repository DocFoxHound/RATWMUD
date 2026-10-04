// How a fight looks and sounds as it happens (doc 37, phase 3): figures, flashes, the shake, a fall, fire, health that
// drains, and the sounds queued, all from the fight's log.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readBattle} from './battle.ts';
import {FightEffects} from './fightFx.ts';
import type {Json} from './json.ts';

const fight = (log: Json[]): Json => ({battle: {
    id: 'fight-1', arena: {x: 0, y: 0, w: 10, h: 10}, you: {observer: false, side: 0, status: 'fighting'},
    fighters: [
        {id: 'ada', name: 'Ada', side: 0, x: 2, y: 2, status: 'fighting', health: 90},
        {id: 'bo', name: 'Bo', side: 1, x: 3, y: 2, status: 'fighting', health: 60},
    ],
    log,
}});

test('blows: a figure rises off the one hit, they flash, a heavy blow shakes the view, and each is heard', () => {
    const fx = new FightEffects();
    fx.selfId = 'ada';
    fx.update(readBattle(fight([{seq: 1, kind: 'start', actor: 'ada', target: 'bo', text: 'Ada goes for Bo.'}])), 0);
    assert.deepEqual(fx.takeCues(), [], 'the first sight of a fight replays nothing');
    fx.update(readBattle(fight([{seq: 2, kind: 'hit', actor: 'ada', target: 'bo', text: 'Ada bites Bo (12).'},
        {seq: 3, kind: 'slash', actor: 'bo', target: 'ada', text: 'Bo cuts Ada (22).'}])), 10);
    const floats = fx.floats(10.4, false);
    assert.deepEqual(floats.map(f => [f.text, f.color]), [['12', 'hit'], ['22', 'heavy']], 'the damage, on the one it struck');
    assert.equal(floats[0].x, 3, 'twelve rises off Bo');
    assert.equal(floats[1].x, 2, 'twenty-two off Ada');
    assert.ok(fx.flash('bo', 10.15) > 0.5, 'Bo flashes at contact');
    assert.equal(fx.flash('bo', 11), 0, 'and not for long');
    const [sx, sy] = fx.shake(10.05, false);
    assert.ok(Math.abs(sx) + Math.abs(sy) > 0, 'a heavy blow on oneself shakes the view');
    assert.deepEqual(fx.shake(10.05, true), [0, 0], 'not in reduced motion');
    assert.deepEqual(fx.shake(12, false), [0, 0], 'and it settles');
    assert.deepEqual(fx.takeCues(), ['bite', 'cut'], 'a bite and a cut, heard in order');
    assert.equal(fx.floats(12, false).length, 0, 'the figures fade');
});

test('a fall bursts and reads DOWN; a miss reads miss; fire fills its tiles, then ash; turns are cued', () => {
    const fx = new FightEffects();
    fx.update(readBattle(fight([])), 0);
    fx.update(readBattle(fight([
        {seq: 1, kind: 'miss', actor: 'ada', target: 'bo', text: 'Ada snaps at Bo and misses.'},
        {seq: 2, kind: 'flame', actor: 'ada', target: '', text: 'Ada breathes a gout of fire!', tiles: [[3, 2], [4, 2]]},
        {seq: 3, kind: 'burnt', actor: 'ada', target: 'bo', text: 'Bo is caught in the fire (15).'},
        {seq: 4, kind: 'down', actor: 'bo', target: 'ada', text: 'Bo goes down.'},
    ])), 5);
    const texts = fx.floats(5.3, false).map(f => f.text);
    assert.ok(texts.includes('miss') && texts.includes('15') && texts.includes('DOWN'), `what happened, in figures: ${texts}`);
    assert.equal(fx.fire(5.5).length, 2, 'fire fills both tiles');
    assert.equal(fx.fire(7).length, 0, 'for a second');
    const b = readBattle(fight([]))!;
    assert.ok(fx.marks(b, 7, false).some(m => m.color === 'ash'), 'then ash where it went');
    assert.equal(fx.rings(5.2, false).length, 1, 'the fall bursts');
    assert.equal(fx.rings(5.2, true).length, 0, 'not in reduced motion');
    assert.deepEqual(fx.takeCues(), ['miss', 'fire', 'down']);
    fx.cue('turn');
    assert.deepEqual(fx.takeCues(), ['turn']);
});

test('health drains after a blow: held a moment, then down to the true figure; it rises at once', () => {
    const fx = new FightEffects();
    assert.equal(fx.lagHealth('bo', 60, 0), 60);
    assert.equal(fx.lagHealth('bo', 40, 1), 60, 'just struck: still drawn at 60');
    assert.equal(fx.lagHealth('bo', 40, 1.2), 60, 'held a moment');
    const draining = fx.lagHealth('bo', 40, 1.45);
    assert.ok(draining < 60 && draining > 40, `then draining: ${draining}`);
    for (let t = 1.5; t < 3; t += 0.05) fx.lagHealth('bo', 40, t);
    assert.equal(fx.lagHealth('bo', 40, 3), 40, 'down to the true figure');
    assert.equal(fx.lagHealth('bo', 60, 3.1), 60, 'tended: up at once');
});
