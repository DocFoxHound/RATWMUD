import assert from 'node:assert/strict';
import {test} from 'node:test';
import {niceTicks} from './chart.ts';

test('the health charts\' ticks are whole and never repeat (R1, shot 32: "1, 1, 0")', () => {
    assert.deepEqual(niceTicks(0, 4), [0, 1, 2, 3, 4]);           // No players: the players chart still reads 0 to 4.
    assert.deepEqual(niceTicks(1, 4), [0, 1, 2, 3, 4]);
    assert.deepEqual(niceTicks(7, 4), [0, 2, 4, 6, 8]);
    assert.deepEqual(niceTicks(55), [0, 20, 40, 60]);             // The tick-time chart, its 50 ms budget and a little more.
    assert.deepEqual(niceTicks(0.4, 50), [0, 20, 40, 60]);
    assert.deepEqual(niceTicks(950, 50), [0, 500, 1000]);
    assert.deepEqual(niceTicks(NaN), [0, 1]);
    for (const max of [0, 1, 3, 9, 12, 99, 101, 2345]) {
        const t = niceTicks(max, 1);
        assert.ok(t.at(-1)! >= max && t.length <= 5 && new Set(t).size === t.length && t.every(Number.isInteger), `${max}: ${t}`);
    }
});
