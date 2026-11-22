// The Campaigns console's data shaping (doc 85, Phase 1): merging the timeline's polls and pages, what a run waits for
// in words, wolves on the LIVE frame, and the mission grid.
import test from 'node:test';
import assert from 'node:assert/strict';
import {
    ago, alertText, dueText, filterRuns, filterTimeline, listCounts, mergeTimeline, missionGrid, phaseStrip, sinceOf, waitingLines,
    waitingOn, wolvesNow, hereCount, type FramePerson, type Mission, type Order, type RunRow, type TimelineItem, type Waiting,
} from './campaigns.ts';

const names = {'player-ash': 'Ash Greymantle', 'player-birch': 'Birch Oakenfall', 'player-elm': 'Elm Sandpaw', rm111: 'Ivo Tarmarsh'};
const order = (id: number, status: Order['status'], at: string, visit = 1, extra: Partial<Order> = {}): TimelineItem =>
    ({key: `o:${id}`, type: 'order', id, kind: 'story.phase', target: 'world-1', status, result: '', by: 'martin', at, doneAt: null, visit, ...extra});
const firing = (id: number, at: string, visit = 1, actor = 'player-ash'): TimelineItem =>
    ({key: `t:${id}`, type: 'trigger', id, visit, phase: 'hold', trigger: 'count-defeat', actor, day: 10, actions: ['add'], at});
const visit = (n: number, at: string, phase = 'hold'): TimelineItem =>
    ({key: `v:${n}`, type: 'phase', visit: n, phase, day: 10, cause: 'published', receipts: [], at});

test('the timeline merges a poll into what it holds: one row per key, the newer copy, newest first', () => {
    const page = [order(2, 'queued', '2026-11-21T10:00:05Z', 2), visit(2, '2026-11-21T10:00:05Z', 'council'),
        firing(7, '2026-11-21T10:00:05Z', 1), order(1, 'applied', '2026-11-21T09:59:00Z'), visit(1, '2026-11-21T09:59:00Z')];
    const merged = mergeTimeline([], [...page].reverse());
    assert.deepEqual(merged.map(i => i.key), ['o:2', 'v:2', 't:7', 'o:1', 'v:1'], 'one tick: the visit it opened is newer than its firing');
    const poll = [order(2, 'applied', '2026-11-21T10:00:05Z', 2), firing(8, '2026-11-21T10:01:00Z', 2, 'player-birch')];
    const after = mergeTimeline(merged, poll);
    assert.deepEqual(after.map(i => i.key), ['t:8', 'o:2', 'v:2', 't:7', 'o:1', 'v:1']);
    assert.equal(after.find(i => i.key === 'o:2')?.type === 'order' && (after.find(i => i.key === 'o:2') as Order).status, 'applied', 'the answer replaces the queued copy');
    // An earlier page joins at the end.
    const earlier = mergeTimeline(after, [firing(3, '2026-11-21T08:00:00Z')]);
    assert.equal(earlier.at(-1)?.key, 't:3');
    assert.deepEqual(sinceOf(earlier, {}, '2026-11-21T10:02:00Z'), {visit: 2, trigger: 8, order: 2, at: '2026-11-21T10:02:00Z'});
    assert.deepEqual(sinceOf([], {visit: 4, trigger: 9}, 'now'), {visit: 4, trigger: 9, order: 0, at: 'now'}, 'an empty poll keeps the cursor');
    assert.deepEqual(filterTimeline(after, 'trigger', 'player-birch').map(i => i.key), ['t:8']);
    assert.deepEqual(filterTimeline(after, 'all', 'player-birch').map(i => i.key), ['t:8', 'v:2', 'v:1'], 'a wolf filter keeps the phases');
});

test('what a run waits for: the admin before the game before the clock; among timers the soonest', () => {
    const stuck: Order = {id: 5514, kind: 'deed.award', target: 'player-ash', status: 'refused', result: 'that wolf is not in the world just now',
        by: 'martin', at: null, doneAt: null, age: 360};
    const waiting: Waiting[] = [
        {on: 'timer', name: 'spring-council', signal: 'spring-council', at: 90, clock: 'game', actor: '', repeat: 0, scope: 'campaign'},
        {on: 'timer', name: 'watch-round', signal: 'watch-round', at: 56, clock: 'game', actor: '', repeat: 7, scope: 'campaign'},
        {on: 'wolves', events: ['gift'], what: [{source: 'trigger', id: 'offer-a-coil', on: 'gift', match: {item: 'rope', target: 'rm111'}}]},
    ];
    assert.deepEqual(waitingOn(waiting, names, 53.9), {who: 'a timer', tone: 'normal', text: 'watch-round day 56.0 (in 2.1 days), every 7 days'});
    assert.deepEqual(waitingOn([{on: 'admin', problem: 'Action 5514 refused', order: stuck}, ...waiting], names, 53.9),
        {who: 'you', tone: 'alert', text: 'deed.award → Ash Greymantle refused: that wolf is not in the world just now'});
    assert.equal(waitingOn([...waiting, {on: 'game', order: {...stuck, status: 'queued', age: 4}}], names, 53.9)?.text,
        'to apply deed.award → Ash Greymantle (order #5514, sent 4 s ago)');
    assert.equal(waitingOn([waiting[2]], names, 53.9)?.text, 'offer-a-coil: when gift (item rope, target Ivo Tarmarsh)', 'residents by name');
    assert.equal(waitingOn([{on: 'publish'}], names, 1)?.who, 'you');
    assert.equal(waitingOn([], names, 1), null);
    assert.deepEqual(waitingLines(waiting, names, 53.9).map(l => l.who), ['a timer', 'a timer', 'wolves'], 'every line, in the order given');
    // A real-clock timer: real days since publication.
    const published = Date.parse('2026-11-20T00:00:00Z') / 1000;
    assert.equal(dueText(2, 'real', 53.9, published, '2026-11-21T00:00:00Z'), 'in 1 days (real time)');
    assert.equal(dueText(1.25, 'real', 53.9, published, '2026-11-21T00:00:00Z'), 'in 6 h (real time)');
    assert.equal(dueText(50, 'game', 53.9), 'day 50.0 (due now)');
    assert.equal(waitingOn([{on: 'timer', name: 'later', signal: 'later', at: 30, clock: 'real', actor: '', repeat: 0, scope: 'visit'},
        {on: 'timer', name: 'soon', signal: 'soon', at: 54, clock: 'game', actor: '', repeat: 0, scope: 'visit'}], names, 53.9, published, '2026-11-21T00:00:00Z')?.text.startsWith('soon'), true);
});

test('alerts and times in words', () => {
    assert.equal(alertText({kind: 'behind', needsYou: false, events: 2400, seconds: 75}, names),
        'Falling behind: 2400 world events unread, the oldest 75 s old. Is the story runner running?');
    assert.match(alertText({kind: 'game', needsYou: false, order: {id: 9, kind: 'story.phase', target: 'world-x', status: 'queued', result: '', by: '', at: null, doneAt: null, age: 180}}, names),
        /story.phase \(#9\) for 3 min/);
    assert.equal(ago('2026-11-21T10:00:00Z', '2026-11-21T13:00:00Z'), '3 h ago');
    assert.equal(ago('2026-11-21T10:00:00Z', '2026-11-21T10:00:10Z'), 'just now');
});

test('the wolves of a story on the LIVE frame: here now first, the others where they were saved, the unknown by name', () => {
    const people: FramePerson[] = [['player-birch', 'Birch Oakenfall', 'o', 'greyfen', 3, 4, 0, '', ''],
        ['player-ash', 'Ash Greymantle', 'p', 'greyfen', 31, 36, 0, '', 'walking'], ['rm111', 'Ivo Tarmarsh', 'n', 'greyfen', 1, 1, 0, 'carter', '']];
    const wolves = wolvesNow(['player-elm', 'player-birch', 'player-ash'], people, names);
    assert.deepEqual(wolves.map(w => [w.name, w.here, w.known]), [['Ash Greymantle', true, true], ['Birch Oakenfall', false, true], ['Elm Sandpaw', false, false]]);
    assert.equal(hereCount(['player-elm', 'player-ash', 'rm111'], people), 1, 'only wolves in the world count');
});

test('a mission as a grid: a row per wolf for their own checklist, one shared row otherwise', () => {
    const passengers: Mission = {id: 'passengers', title: 'Account for the passengers', mode: 'individual', completed: 1, required: 2, ready: false,
        optional: false, requiredContributors: 1, contributors: [],
        objectives: [{label: 'Ask the carter', on: 'conversation'}, {label: 'Bring rope', on: 'gift', goal: 4, measure: 'quantity'}],
        tracks: {'player-ash': [1, 3], 'player-birch': [2, 0]}};
    const grid = missionGrid(passengers, ['player-elm', 'player-ash', 'player-birch'], names);
    assert.deepEqual(grid.map(r => [r.name, r.done, r.cells.map(c => c.text)]), [
        ['Birch Oakenfall', true, ['✔', '✔']], ['Ash Greymantle', false, ['✔', '3 / 4']], ['Elm Sandpaw', false, ['-', '·']]]);
    const line: Mission = {...passengers, id: 'line', mode: 'group', requiredContributors: 2, tracks: {group: [1, 2]}, contributors: ['player-ash']};
    assert.deepEqual(missionGrid(line, ['player-ash'], names).map(r => [r.who, r.cells.map(c => c.text)]), [['group', ['✔', '2 / 4']]], 'one shared row');
    const alone: Mission = {...line, tracks: {group: [1, 4]}};
    assert.deepEqual(missionGrid(alone, ['player-ash'], names)[0].cells[1], {state: 'partial', text: '4 / 4, needs 2 wolves'},
        'met by one wolf: a group mission waits for a second');
});

test('the list: counts by status, what needs the admin, and the phase strip', () => {
    const runs = [{run: 'a', status: 'running', alerts: [{kind: 'quiet', needsYou: false}]}, {run: 'b', status: 'paused', alerts: [{kind: 'paused', needsYou: true}]},
        {run: 'c', status: 'stopped'}, {run: 'd', status: 'complete'}, {run: 'e', status: 'approved'}] as RunRow[];
    assert.deepEqual(listCounts(runs), {all: 5, running: 1, paused: 1, approved: 1, ended: 2, needsYou: 2});
    assert.deepEqual(filterRuns(runs, 'ended').map(r => r.run), ['c', 'd']);
    assert.deepEqual(phaseStrip([{visit: 3, phase: 'deep-freeze', day: 28, cause: 'clock', receipts: []}, {visit: 1, phase: 'enrol', day: 0, cause: 'published', receipts: []},
        {visit: 2, phase: 'ferry-watch', day: 0, cause: 'event 4', receipts: []}],
        [{id: 'deep-freeze', to: ['ferries-reopen', 'limited-crossings']}], 'deep-freeze'),
        {path: ['enrol', 'ferry-watch', 'deep-freeze'], next: ['ferries-reopen', 'limited-crossings']});
});
