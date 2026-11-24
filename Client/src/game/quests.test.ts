// A world story's quests in the journal (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 6).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {noteLines, questButtons, questLines, questMessage, questStatus, storyPings, trackerLines, trackerRows} from './quests.ts';

const axle = {
    id: 'axle', title: 'An axle for the carter', summary: 'The carter at the ford is stuck.', scope: 'player', status: 'active',
    giver: 'rm111', giverName: 'Ivo Tarmarsh', abandon: true, handIn: false, accept: false, hiddenLeft: 1, deadline: 42.5,
    objectives: [
        {id: 'ask', label: 'Ask the carter which part gave way.', done: true, open: true},
        {id: 'bring', label: 'Bring a replacement axle.', amount: 1, goal: 4, open: true},
        {id: 'wheel', label: 'A spare wheel would not go amiss.', optional: true, failed: true, open: true},
        {id: 'later', label: 'Take it to the ford.', open: false},
    ],
};

test('a quest under way: ticked, counted, struck, later, optional, hidden and its deadline', () => {
    assert.equal(questStatus(axle), 'Under way');
    assert.deepEqual(questLines(axle).map(l => [l.tone, l.text]), [
        ['note', 'The carter at the ford is stuck.'],
        ['done', '✓ Ask the carter which part gave way.'],
        ['open', '· Bring a replacement axle. · 1 / 4'],
        ['failed', '✗ A spare wheel would not go amiss. (optional)'],
        ['later', '○ Take it to the ford. · later'],
        ['note', 'And one more thing to find out.'],
        ['note', 'To be done by day 42.'],
    ]);
    assert.deepEqual(questLines(axle, 40).at(-1), {text: 'To be done within 3 days (by day 42).', tone: 'note'});
});

test('the states in words', () => {
    assert.equal(questStatus({...axle, status: 'ready', handIn: true}), 'Ready: return to Ivo Tarmarsh');
    assert.equal(questStatus({...axle, status: 'ready', handIn: false}), 'Ready');
    assert.equal(questStatus({status: 'offered'}), 'Offered to you');
    assert.equal(questStatus({...axle, status: 'offered'}), 'Offered by Ivo Tarmarsh');
    assert.equal(questStatus({status: 'expired'}), 'Ended');
    assert.equal(questStatus({status: 'failed'}), 'Failed');
    assert.equal(questLines({...axle, status: 'done', again: 43.2, summary: '', objectives: []}).at(-1)?.text, 'It may be taken again from day 44.');
    assert.equal(questLines({...axle, status: 'done', again: null, summary: '', objectives: []}).length, 0);
});

test('the buttons and the messages they send', () => {
    assert.deepEqual(questButtons(axle).map(b => b.verb), ['quest_abandon']);
    assert.deepEqual(questButtons({...axle, abandon: false}), []);
    assert.deepEqual(questButtons({...axle, status: 'done'}), []);
    assert.deepEqual(questButtons({...axle, status: 'offered', accept: true}).map(b => [b.label, b.verb]),
        [['ACCEPT', 'quest_accept'], ['DECLINE', 'quest_decline']]);
    assert.deepEqual(questButtons({...axle, status: 'offered', accept: false}), []);
    assert.deepEqual(questMessage('world-abc', 'axle', 'quest_abandon'), {type: 'storyline', verb: 'quest_abandon', storyline: 'world-abc', quest: 'axle'});
});

test('trackers', () => {
    assert.deepEqual(trackerLines({trackers: [{label: 'Ropes delivered', value: 3}, {label: 'Gate open', value: false}, {label: 'Finder', value: 'Ash'}]}),
        ['Ropes delivered: 3', 'Gate open: no', 'Finder: Ash']);
    assert.deepEqual(trackerLines({}), []);
});

test('doc 86, C1d: a number shown of another reads a / b over a thin bar', () => {
    assert.deepEqual(trackerRows({trackers: [{label: 'This chapter', value: 34, goal: 80}, {label: 'Howls', value: 3}]}), [
        {text: 'This chapter: 34 / 80', fill: 34 / 80},
        {text: 'Howls: 3'},
    ]);
    assert.deepEqual(trackerRows({trackers: [{label: 'Past it', value: 90, goal: 80}, {label: 'Nothing asked', value: 0, goal: 0}]}).map(r => r.fill), [1, 0]);
    assert.deepEqual(trackerLines({trackers: [{label: 'This chapter', value: 34, goal: 80}]}), ['This chapter: 34 / 80']);
});

test('doc 85, Phase 9: a world story\'s notes in the journal and its markers on the maps', () => {
    const journal = [
        {id: 'world-a', title: 'The long night', notes: [{text: 'The tracks lead east.', day: 3}], pings: [
            {cell: 'town', x: 4, y: 5, label: 'The tracks', wx: 104.5, wy: 205.5, place: 'Greyfen Crossing'},
            {cell: 'shop', x: 2, y: 2, label: 'Upstairs', wx: 12, wy: 12, inside: true, place: 'Tallow & Twine'},
            {cell: 'nowhere', label: 'Unplaced'}]},
        {id: 'world-b', title: 'Quiet'}];
    assert.deepEqual(noteLines(journal[0]), ['✎ The tracks lead east.']);
    assert.deepEqual(noteLines(journal[1]), []);
    assert.deepEqual(storyPings(journal), [
        {wx: 104.5, wy: 205.5, label: 'The tracks', place: 'Greyfen Crossing', inside: false, story: 'The long night'},
        {wx: 12, wy: 12, label: 'Upstairs', place: 'Tallow & Twine', inside: true, story: 'The long night'}]);
    assert.deepEqual(storyPings(undefined), []);
});
