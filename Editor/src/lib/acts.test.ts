import assert from 'node:assert/strict';
import {test} from 'node:test';
import {awaitOutcome, outcomeWords, type ActOutcome} from './acts.ts';

const quick = () => Promise.resolve();

test('an act is waited for until the game server answers it', async () => {
    const answers: ActOutcome[] = [{status: 'queued', result: ''}, {status: 'queued', result: ''}, {status: 'refused', result: 'No such resident.'}];
    let asked = 0;
    const outcome = await awaitOutcome(async () => answers[Math.min(asked++, answers.length - 1)], 10, 0, quick);
    assert.deepEqual(outcome, {status: 'refused', result: 'No such resident.'});
    assert.equal(asked, 3);
});

test('a refusal is never told as done (doc 79, Phase 9\'s second repair)', () => {
    assert.throws(() => outcomeWords('Band for a while', {status: 'refused', result: 'No such resident.'}, 'Done.'),
                  /refused\. No such resident\./);
    assert.throws(() => outcomeWords('Band', {status: 'expired', result: ''}, 'Done.'), /never applied/);
    assert.equal(outcomeWords('Band', {status: 'applied', result: 'x'}, 'Done.'), 'Done.');
    assert.match(outcomeWords('Band', {status: 'queued', result: ''}, 'Done.'), /no game server has picked it up yet/);
});

test('still queued after every try', async () => {
    const outcome = await awaitOutcome(async () => ({status: 'queued', result: ''}), 3, 0, quick);
    assert.equal(outcome.status, 'queued');
});
