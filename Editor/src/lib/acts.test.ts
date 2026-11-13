import assert from 'node:assert/strict';
import {test} from 'node:test';
import {awaitOutcome, awaitSync, outcomeWords, type ActOutcome} from './acts.ts';
import {ECONOMY_ACT_KINDS, LIVE_ACT_KINDS, reasonWords, syncWords} from './economyContract.ts';

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

test('the levers\' economy.sync is waited for and told as the server answered it (the last repair)', async () => {
    const reads = [{lastSync: {id: 6, status: 'applied', result: 'old'}}, {lastSync: {id: 7, status: 'queued', result: ''}},
                   {lastSync: {id: 7, status: 'applied', result: 'The economy\'s rules are read again and in force. (2 records skipped)'}}];
    let asked = 0;
    const last = await awaitSync(7, async () => reads[Math.min(asked++, reads.length - 1)], 8, 0, quick);
    assert.equal(asked, 3, 'an older sync and a queued one are read past');
    assert.equal(last?.status, 'applied');
    assert.match(syncWords({...last!, by: 'Ada', at: ''} as never), /#7, by Ada\): applied\. .*2 records skipped/);
    const none = await awaitSync(9, async () => ({lastSync: {id: 9, status: 'queued', result: ''}}), 3, 0, quick);
    assert.equal(none?.status, 'queued');
    assert.match(syncWords({id: 9, status: 'queued', result: '', by: '', at: ''}), /waiting for the game server/);
    // A host's Editor: no row number, and a dev identity is "the DM" (R1, shot 30's "#32, by dm-devtest").
    assert.match(syncWords({id: 32, status: 'applied', result: '', by: 'dm-devtest', at: ''}, () => 'the DM', false),
        /^The last change sent to the game server \(by the DM\): applied\.$/);
    assert.match(syncWords({id: 32, status: 'applied', result: '', by: '', at: ''}, () => 'x', false), /server: applied\.$/);
    assert.match(syncWords({id: 9, status: 'expired', result: 'Not applied within ten minutes.', by: '', at: ''}), /never applied: no game server took it/);
    assert.match(syncWords({id: 9, status: 'refused', result: 'The economy\'s rules could not be read: x', by: '', at: ''}), /refused by the game server\. The economy/);
    assert.equal(syncWords(null), '');
    assert.equal(await awaitSync(9, async () => { throw new Error('down'); }, 2, 0, quick), null);
});

test('the act kinds are one list', () => {
    assert.deepEqual([...LIVE_ACT_KINDS, 'character.grant'], [...ECONOMY_ACT_KINDS]);
});

test('the players\' hover shows goods, not "+0 -0"', () => {
    assert.equal(reasonWords('dm grant: for the bell', [25, 0, 0, 0]), 'dm grant: for the bell: +25p -0p');
    assert.equal(reasonWords('provisions to start with', [0, 0, 12, 0]), 'provisions to start with: goods +12p -0p');
    assert.equal(reasonWords('trade', [0, 4, 1, 0]), 'trade: +0p -4p, goods +1p -0p');
    assert.equal(reasonWords('old save', [3, 1]), 'old save: +3p -1p');
});

test('a confirmation names the wolf and says whether there is an opposite (doc 78, 12.3)', async () => {
    const {sureText, greyed, OPPOSITES} = await import('./acts.ts');
    const kill = sureText('Kill Ada', 'Ashfall', 'character.kill', 'Ada', true);
    assert.match(kill, /^Kill Ada on Ashfall\?/);
    assert.match(kill, /Ada is a player's wolf: its player is told in the game\./);
    assert.match(kill, /Its opposite: Resurrect/);
    const tie = sureText('End Ada\'s tie', 'Ashfall', 'tie.end', 'Ada');
    assert.match(tie, /Ada is a player's wolf\.\n/);
    assert.match(tie, /It has no opposite: it can't be undone from here\./);
    assert.equal(sureText('Do it', 'Ashfall', 'unknown.kind'), 'Do it on Ashfall?', 'an unlisted act says nothing of an opposite');
    // NPC Life's kill, revive and band (M6's run: they asked without the opposite).
    assert.equal(sureText('Kill Ivo Tarmarsh', 'Ashfall', 'npc.kill'), 'Kill Ivo Tarmarsh on Ashfall?\n\nIts opposite: Revive.');
    assert.match(sureText('Make Ivo\'s household poor for 3 days', 'Ashfall', 'band.override'), /Its opposite: Clear the band/);
    assert.ok(Object.values(OPPOSITES).every(o => o === null || (o.length > 3 && !o.endsWith('.'))));
    assert.equal(greyed([false, 'a'], [true, 'b'], [true, 'c']), 'b');
    assert.equal(greyed([false, 'a']), '');
});
