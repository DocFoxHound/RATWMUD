import {test} from 'node:test';
import assert from 'node:assert/strict';
import {regardWords, timeline, type ChronicleEntry, type SeasonRound} from './chronicle.ts';

const e = (id: number, day: number, importance: number, text: string): ChronicleEntry =>
    ({id, day, date: '', kind: 'x', importance, text, people: [], cell: '', place: ''});
const entries = [e(1, 0.2, 3, 'appeared'), e(2, 2, 1, 'first spoke'), e(3, 3, 2, 'robbed'), e(4, 100, 3, 'married')];
const seasons: SeasonRound[] = [{day: 2, label: 'Spring, Year 1', text: 'spring round'}, {day: 101, label: 'Summer, Year 1', text: 'summer round'}];

test('the timeline keeps what matters, in order, with the round at the end of its season', () => {
    const text = (least: number, rounds: boolean) => timeline({entries, seasons}, least, rounds)
        .map(l => l.kind === 'event' ? l.entry.text : l.round.text);
    assert.deepEqual(text(3, false), ['appeared', 'married']);
    assert.deepEqual(text(2, true), ['appeared', 'robbed', 'spring round', 'married', 'summer round']);
    assert.deepEqual(text(1, false), ['appeared', 'first spoke', 'robbed', 'married']);
});

test('a bond in words', () => {
    const bond = {who: 'ada', affinity: -20, trust: -30, familiarity: 40, fear: 35, respect: 0, owed: 0, lastContact: 3};
    assert.equal(regardWords(bond), 'knows · dislikes · distrusts · afraid');
    assert.equal(regardWords({...bond, affinity: 50, trust: 40, familiarity: 70, fear: 0, owed: -3}), 'knows well · fond · trusts · owes 3p');
});
