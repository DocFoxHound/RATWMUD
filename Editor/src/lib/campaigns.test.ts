// The Campaigns console's data shaping (doc 85, Phase 1): merging the timeline's polls and pages, what a run waits for
// in words, wolves on the LIVE frame, and the mission grid.
import test from 'node:test';
import assert from 'node:assert/strict';
import {
    ago, alertText, beforeAfter, causeText, changeText, commandLine, dueText, filterRuns, firingCounts, newKey, steerable, typedTitle, wayOf, filterTimeline, listCounts, mergeTimeline, missionGrid, phaseStrip, sinceOf, waitingLines,
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
    assert.deepEqual(sinceOf(earlier, {}, '2026-11-21T10:02:00Z'), {visit: 2, trigger: 8, order: 2, command: 0, version: 0, at: '2026-11-21T10:02:00Z'});
    assert.deepEqual(sinceOf([], {visit: 4, trigger: 9}, 'now'), {visit: 4, trigger: 9, order: 0, command: 0, version: 0, at: 'now'}, 'an empty poll keeps the cursor');
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
    assert.deepEqual(listCounts(runs), {all: 5, running: 1, held: 0, paused: 1, approved: 1, ended: 2, needsYou: 2});
    assert.deepEqual(filterRuns(runs, 'ended').map(r => r.run), ['c', 'd']);
    assert.deepEqual(phaseStrip([{visit: 3, phase: 'deep-freeze', day: 28, cause: 'clock', receipts: []}, {visit: 1, phase: 'enrol', day: 0, cause: 'published', receipts: []},
        {visit: 2, phase: 'ferry-watch', day: 0, cause: 'event 4', receipts: []}],
        [{id: 'deep-freeze', to: ['ferries-reopen', 'limited-crossings']}], 'deep-freeze'),
        {path: ['enrol', 'ferry-watch', 'deep-freeze'], next: ['ferries-reopen', 'limited-crossings']});
});

// ---- Phase 2: steering
const command = (id: number, at: string, visit = 1, extra: Record<string, unknown> = {}): TimelineItem =>
    ({key: `c:${id}`, type: 'command', id, command: 'set', visit, digest: 'd', payload: {key: 'stores', scope: 'story', value: 5},
      reason: 'two coils were handed over', by: 'martin#a1', status: 'done', result: {text: 'stores set 3 → 5.', before: {value: 3}, after: {value: 5}},
      doneAt: at, askedAt: at, at, ...extra} as TimelineItem);

test('a command is the timeline\'s fourth source: after the orders of its moment, filtered as "Admin", in the poll cursor', () => {
    const t = '2026-11-21T10:00:05Z';
    const merged = mergeTimeline([], [visit(2, t), order(5, 'queued', t, 2), command(3, t, 2), firing(9, t, 2)]);
    assert.deepEqual(merged.map(i => i.key), ['c:3', 'o:5', 't:9', 'v:2']);
    assert.deepEqual(filterTimeline(merged, 'command').map(i => i.key), ['c:3']);
    assert.equal(sinceOf(merged, {}, t).command, 3);
    const fired = command(4, t, 2, {command: 'fire', payload: {trigger: 'offer-a-coil', actor: 'player-birch'}});
    assert.deepEqual(filterTimeline([fired, firing(9, t, 2)], 'all', 'player-birch').map(i => i.key), ['c:4']);
    assert.deepEqual(filterTimeline([fired, firing(9, t, 2)], 'all', '', 'offer-a-coil').map(i => i.key), ['c:4']);
});

test('commands, changes and causes in words', () => {
    assert.equal(commandLine('set', {key: 'stores', scope: 'story', value: 5}, names), 'set stores to 5');
    assert.equal(commandLine('fire', {trigger: 'offer-a-coil', actor: 'player-birch', check: false}, names), 'fired offer-a-coil for Birch Oakenfall (unchecked)');
    assert.equal(commandLine('enter', {phase: 'deep-freeze', effects: false}, names), 'went to deep-freeze (without its entry effects)');
    assert.equal(commandLine('tell', {to: 'participants', text: 'The river has closed.'}, names), 'told the story\'s wolves: "The river has closed."');
    assert.equal(commandLine('order', {receipt: 5514, do: 'skip'}, names), 'skipped order #5514');
    assert.equal(commandLine('timer', {do: 'shift', name: 'watch-round', days: 2}, names), 'shift timer watch-round (+2 days)');
    assert.equal(changeText({what: 'counter', scope: 'story', key: 'watches', before: 5, after: 6}), 'watches 5 → 6');
    assert.equal(changeText({what: 'counter', scope: 'player', key: 'hauled', wolf: 'player-birch', before: 0, after: 1}, names), 'hauled (Birch Oakenfall) 0 → 1');
    assert.equal(changeText({what: 'timer', key: 'watch-round', before: 49, after: 56}), 'timer watch-round day 49 → day 56');
    assert.equal(beforeAfter({result: {text: '', before: {status: 'running'}, after: {status: 'held'}}}), 'status running → held');
    assert.equal(causeText('admin martin#a1: the river froze early'), 'martin by hand: "the river froze early"');
});

test('which commands ask for the title, which way a jump goes, what may be steered', () => {
    assert.ok(typedTitle('stop', {}));
    assert.ok(typedTitle('order', {do: 'skip'}));
    assert.ok(!typedTitle('order', {do: 'retry'}));
    assert.ok(typedTitle('enter', {}, 'back') && !typedTitle('enter', {}, 'on'));
    const phases = ['enrol', 'ferry-watch', 'deep-freeze'];
    assert.equal(wayOf(phases, 'deep-freeze', 'enrol'), 'back');
    assert.equal(wayOf(phases, 'enrol', 'deep-freeze'), 'on');
    assert.equal(wayOf(phases, 'enrol', 'enrol'), 'again');
    assert.ok(steerable('held') && steerable('paused') && !steerable('stopping') && !steerable('complete'));
    assert.match(newKey(), /^[A-Za-z0-9_-]{8,64}$/);
    assert.notEqual(newKey(), newKey());
    const counts = firingCounts([firing(1, '2026-11-21T09:00:00Z'), firing(2, '2026-11-21T10:00:00Z')]);
    assert.deepEqual(counts['count-defeat'], {count: 2, last: '2026-11-21T10:00:00Z', day: 10});
});

test('held, stopping and the runner in the list and the alerts', () => {
    const rows = [{status: 'running'}, {status: 'held'}, {status: 'stopping'}, {status: 'paused'}] as RunRow[];
    const counts = listCounts(rows);
    assert.deepEqual([counts.running, counts.held, counts.paused], [2, 1, 1]);
    assert.deepEqual(filterRuns(rows, 'held').map(r => r.status), ['held']);
    assert.match(alertText({kind: 'held', needsYou: false, by: 'martin#a1', reason: 'a check', events: 3}, names), /^Held by martin \("a check"\): 3 world events wait/);
    assert.match(alertText({kind: 'runner', needsYou: true, age: 95}, names), /not beaten for 95 s/);
    assert.match(alertText({kind: 'runner', needsYou: true, age: null}, names), /has not run on this world/);
    const lines = waitingLines([{on: 'admin', problem: '', held: {by: 'martin#a1', reason: 'a check'}},
        {on: 'game', stopping: true, order: {id: 9, kind: 'story.stop', target: 'w', status: 'queued', result: '', by: 'm', at: null, doneAt: null}}], names, 10);
    assert.deepEqual(lines.map(l => l.text), ['held by martin ("a check"): resume it to carry on', 'to acknowledge the stop (order #9)']);
});

// ------------------------------------------------------------------------------------------------- Phase 3: Explain it
import {
    condLines, condText, eventText, judgedState, outcomeLines, previewLines, problemList, problemPlace, secondsText, stepText, stuckReasons, summaryLines,
    wouldFireFor, type CondNode, type Judged, type Preview, type Problem,
} from './campaigns.ts';

test('a judged condition in words, true false or unknown, with its values', () => {
    const counter: CondNode = {test: 'counter', key: 'stores', scope: 'story', op: '>=', value: 2, expected: 5, verdict: false};
    assert.equal(condText(counter), 'stores ≥ 5 ✘ (stores is 2)');
    assert.equal(condText({test: 'field', field: 'item', op: '=', expected: 'rope', value: null, verdict: null, needs: 'event'}), 'item = "rope" ? (the event brings it)');
    assert.equal(condText({test: 'counter', key: 'hauled', scope: 'player', op: '>', expected: 1, value: 3, verdict: true, wolf: 'player-ash'}, names),
        'hauled (the wolf\'s) > 1 ✔ (hauled (the wolf\'s) is 3 for Ash Greymantle)');
    assert.equal(condText({test: 'region', region: 'breach', verdict: null, needs: 'place'}), 'it happens inside breach ? (the event brings its place)');
    const tree: CondNode = {test: 'any', verdict: false, conditions: [counter, {test: 'not', verdict: false, condition: {test: 'mission', id: 'm', verdict: true}}]};
    assert.deepEqual(condLines(tree).map(l => [l.depth, l.verdict]), [[0, false], [1, false], [1, false], [2, true]]);
    assert.equal(condLines(undefined).length, 0);
});

test('a judged trigger: its event, where it stands, the wolves it would fire for', () => {
    const coil: Judged = {source: 'trigger', id: 'offer-a-coil', on: 'gift', match: {item: 'rope', target: 'rm111'}, needsWolves: true, can: true,
        forWolves: ['player-ash', 'player-birch', 'player-elm']};
    assert.equal(eventText(coil, names), 'when gift (item rope, target Ivo Tarmarsh)');
    assert.equal(eventText({on: 'story signal', signal: 'watch-round'}), 'when signal "watch-round"');
    assert.equal(judgedState(coil, names, 10).text, 'waits for its event');
    assert.equal(wouldFireFor(coil, new Set(['player-ash']), names), 'would fire for: Ash Greymantle (here now) and 2 away');
    assert.equal(wouldFireFor(coil, new Set(), names), 'for 3 wolves of the story, none here now');
    assert.equal(judgedState({...coil, off: true}, names, 10).tone, 'alert');
    assert.equal(judgedState({...coil, used: true, once: 'campaign'}, names, 10).text, 'fired: once per campaign');
    assert.equal(judgedState({...coil, cannot: 'gone', can: false}, names, 10).text, 'waits for a wolf of the story: none has been about lately');
    assert.equal(judgedState({source: 'trigger', id: 'c', on: 'clock', due: 12, needsWolves: false, can: true}, names, 10).text, 'due day 12.0 (in 2 days)');
    assert.equal(judgedState({source: 'trigger', id: 's', on: 'story signal', signal: 'x', needsWolves: false, can: true,
        timers: [{timer: 'x', signal: 'x', at: 20, clock: 'game'}], senders: []}, names, 10).text, 'timer x sends it (day 20.0)');
    assert.equal(judgedState({source: 'exit', index: 0, on: 'gift', to: 'b', needsWolves: true, can: true, votes: {have: 1, need: 2, wolves: []}}, names, 10).text,
        '1 of 2 wolves so far');
});

test('why an event did not fire it, in words', () => {
    assert.equal(stepText({verdict: 'filter', field: 'item', expected: 'rope', actual: 'nail'}), 'its item was "nail"; it wants "rope"');
    assert.equal(stepText({verdict: 'outsider', wolf: 'player-elm'}, names), 'Elm Sandpaw is not in the story');
    assert.equal(stepText({verdict: 'used', once: 'player', wolf: 'player-ash'}, names), 'already fired for Ash Greymantle (once per player)');
    assert.match(stepText({verdict: 'conditions', conditions: {test: 'counter', key: 'stores', scope: 'story', op: '>=', value: 2, expected: 5, verdict: false}}),
        /its conditions said no: stores ≥ 5 ✘ \(stores is 2\) \(today's values\)/);
    assert.equal(stepText({verdict: 'fired'}), 'it fired');
});

test('a dry run: what an event or a command would do', () => {
    const fired = {id: 0, visit: 2, phase: 'p', trigger: 'offer-a-coil', actor: 'player-ash', day: 10, actions: ['add']};
    assert.deepEqual(outcomeLines({fired: [fired], changes: [{what: 'counter', key: 'stores', scope: 'story', before: 0, after: 4}], missions: [], moved: true,
        phase: 'deep-freeze', visit: 3, entryEffects: [{kind: 'band.override', npc: 'rm111'}]}, names), [
        'fires offer-a-coil for Ash Greymantle: add', 'sets stores 0 → 4', 'moves to deep-freeze (visit 3)', 'then its entry effects: band.override → Ivo Tarmarsh']);
    const p: Preview = {run: 'w', kind: 'set', payload: {}, outcome: 'done', text: 'stores set 0 → 9.', orders: [], fired: [], moved: false, phase: 'p', visit: 2,
        changes: [{what: 'counter', key: 'stores', scope: 'story', before: 0, after: 9}], missions: []};
    assert.deepEqual(previewLines(p), ['stores set 0 → 9.', 'sets stores 0 → 9']);
    assert.deepEqual(previewLines({...p, outcome: 'refused', text: 'No.'}), ['The runner would refuse it now: No.']);
    assert.deepEqual(previewLines({...p, outcome: 'waits', text: 'It would wait.'}), ['It would wait.']);
});

test('the new alerts, the summary outside the tab, and times', () => {
    assert.equal(alertText({kind: 'stuck', needsYou: true, phase: 'hold', reasons: {used: 2, unsent: 1}, goneDays: 14}, names),
        'Cannot move on from hold: nothing in it can still fire by itself (2 used up, 1 a signal nothing sends). Fire a trigger, set a variable or go to a phase.');
    assert.equal(stuckReasons({reasons: {gone: 1}, goneDays: 30}), '1 waiting for wolves gone 30 days');
    assert.equal(alertText({kind: 'overdue', needsYou: false, signal: 'watch-round', seconds: 400}, names), 'Timer watch-round is overdue by 7 min and has not fired: is the story waiting on the game?');
    assert.equal(alertText({kind: 'full', needsYou: false, wolves: 455, cap: 500}, names), 'Its audience is nearly full: 455 of 500 wolves.');
    assert.equal(secondsText(3000), '≈ 50 min');
    assert.equal(secondsText(45, false), '45 s');
    assert.equal(secondsText(7380), '≈ 2.1 h');
    assert.deepEqual(summaryLines({runner: {beatAt: null, age: null}, runnerDown: true, live: 1, needsYou: 2, at: '', runs: [{run: 'w', title: 'T', status: 'paused',
        kinds: ['paused'], text: 'Story "T" is paused.'}]}), ['The story runner has not run on this world: no story moves until it does.', 'Story "T" is paused.']);
    assert.deepEqual(summaryLines(null), []);
});

test('the Problems list: where each problem is, in order', () => {
    const at = (a: Problem['at']): Problem => ({level: 'error', text: 'x', path: '', at: a});
    assert.equal(problemPlace(at({kind: 'trigger', phase: 1, phaseId: 'ferry-watch', index: 0, id: 'begin-deep-freeze', part: 'actions[1]'})),
        'Phase ferry-watch › trigger begin-deep-freeze › action 2');
    assert.equal(problemPlace(at({kind: 'trigger', phase: null, index: 1, id: 'offer-a-coil', part: 'if'})), 'Campaign triggers › trigger offer-a-coil › conditions');
    assert.equal(problemPlace(at({kind: 'mission', phase: 0, phaseId: 'survey', index: 2, id: 'boats', part: 'objectives[0]'})), 'Phase survey › mission boats › objective 1');
    assert.equal(problemPlace(at({kind: 'case', index: 0, id: 'A shared defence', part: 'events[3]'})), 'rehearsal case A shared defence › fact 4');
    assert.equal(problemPlace(at({kind: 'settings'})), 'Campaign settings');
    const list = problemList([{level: 'warning', text: 'w', path: '', at: {kind: 'settings'}}, {level: 'error', text: 'e', path: '', at: {kind: 'settings'}},
        {level: 'warning', text: 'b', path: '', world: true, blocks: true, at: {kind: 'settings'}}], ['Campaign / open-ledger', 'hold/count-defeat'], ['hold']);
    assert.deepEqual(list.map(p => p.text), ['e', 'b', 'w', 'No rehearsal case reaches this trigger.', 'No rehearsal case reaches this trigger.']);
    assert.deepEqual(list.slice(3).map(p => [p.at.phase, p.at.id]), [[null, 'open-ledger'], [0, 'count-defeat']]);
});

test('version 2: a problem in a nested action says where (doc 85, Phase 5)', async () => {
    const {problemPlace} = await import('./campaigns.ts');
    const at = (part: string) => problemPlace({level: 'error', text: '', path: '', at: {kind: 'trigger', id: 'rope', phase: 0, phaseId: 'gather', part}} as never);
    assert.equal(at('actions[3].elif[0].then[1]'), 'Phase gather › trigger rope › action 4 › else if 1 › 2');
    assert.equal(at('actions[0].actions[1].then[0]'), 'Phase gather › trigger rope › action 1 › for each 2 › then 1');
    assert.equal(at('actions[2].choices[1].actions[0]'), 'Phase gather › trigger rope › action 3 › choice 2 › 1');
});

test('a move onto a new version in words: the report, the diff, the timeline, the command (doc 85, Phase 5b)', async () => {
    const {diffLines, migrationGroups, migrationLine, previewLines, versionLine} = await import('./campaigns.ts');
    assert.equal(migrationLine({what: 'variable', key: 'hauled.player', scope: 'player', wolf: 'player-ash', value: 2, renamed: 'hauled', why: 'renamed from hauled.'}, names),
        'hauled.player (Ash Greymantle) = 2: renamed from hauled.');
    assert.equal(migrationLine({what: 'timer', key: 'watch-round/player-birch', at: 17}, names), 'timer watch-round (Birch Oakenfall), due day 17.0');
    assert.equal(migrationLine({what: 'wait', key: 'watch/ring', wolf: 'player-ash', at: 2}, names), 'the wait in watch/ring for Ash Greymantle (resumes day 2.0)');
    assert.equal(migrationLine({what: 'fired', key: 'offer-a-coil', wolves: ['player-elm']}, names), 'offer-a-coil already fired for Elm Sandpaw');
    const report = {kept: [{what: 'variable' as const, key: 'stores', scope: 'story', value: 5}], reset: [], defaulted: [],
        dropped: [{what: 'timer' as const, key: 'first-freeze', at: 38, why: 'nothing listens.'}], added: [], warnings: ['Counts start now.'], blocked: null};
    assert.deepEqual(migrationGroups(report).map(g => [g.group, g.lines]), [['dropped', ['timer first-freeze, due day 38.0: nothing listens.']], ['kept', ['stores = 5']]]);
    const empty = {added: [], removed: [], changed: []};
    const diff = {same: false, header: ['title'], tests: false, triggers: {...empty, added: ['early-thaw'], changed: [{id: 'offer-a-coil', what: ['note'], wordsOnly: true}]},
        phases: empty, regions: empty, scenes: empty, timers: {added: [], removed: ['first-freeze'], changed: []},
        variables: {added: [], removed: [], retyped: [{id: 'watches', scope: 'story', from: 'number', to: 'switch'}], rescoped: [], changed: []},
        missions: {added: [], removed: [], changed: [{phase: 'survey', id: 'boats', what: ['objectives'], objectives: true}]}};
    assert.deepEqual(diffLines(diff), ['Triggers: 1 added (early-thaw); 1 changed (offer-a-coil: words only)', 'Variables: 1 retyped (watches: number → switch)',
        'Missions: 1 changed (survey/boats: its objectives)', 'Timers: first-freeze never started now', 'Also changed: title']);
    assert.deepEqual(diffLines({...diff, same: true}), ['Nothing the story does has changed (its rehearsals may have).']);
    const item = {key: 'm:2', type: 'version' as const, id: 2, digest: 'b7e1c2d4aa', from: 'a41c9e00ff', status: 'current' as const, approvedBy: 'ann', approvedAt: null,
        movedAt: null, movedBy: 'martin#a1', command: 3, visit: 2, day: 12, order: 9, settledAt: null, problem: '', at: '2026-11-22T10:00:00Z', phase: 'the-watch',
        phaseFrom: 'ferry-watch', warnings: [], counts: {kept: 4, reset: 0, defaulted: 0, dropped: 2, added: 1}};
    assert.equal(versionLine(item), 'moved to version b7e1c2d4 by martin (from a41c9e00), landing in the-watch (ferry-watch is gone): 4 kept, 2 dropped');
    assert.equal(versionLine({...item, status: 'refused'}), 'the game refused version b7e1c2d4: back on a41c9e00 (moved by martin)');
    assert.equal(versionLine({...item, status: 'moving'}).endsWith('waiting for the game'), true);
    // On the timeline: under Admin, and the poll's cursor for it.
    const items = [item, visit(1, '2026-11-21T09:59:00Z')];
    assert.deepEqual(filterTimeline(items, 'command').map(i => i.key), ['m:2']);
    assert.equal(sinceOf(items, {}, 'now').version, 2);
    assert.deepEqual(phaseStrip([{visit: 1, phase: 'gather', day: 1, cause: 'published', receipts: []}], [{id: 'muster', to: ['gate']}], 'muster'),
        {path: ['gather', 'muster'], next: ['gate']}, 'a run that landed in a renamed phase');
    assert.equal(commandLine('migrate', {version: 3, phase: 'the-watch'}, names), 'moved it onto approved version #3, landing in the-watch');
    assert.equal(beforeAfter({result: {text: '', before: {version: 'a41c9e00ff', phase: 'ferry-watch'}, after: {version: 'b7e1c2d4aa', phase: 'ferry-watch'}}}),
        'version a41c9e00 → b7e1c2d4');
    const lines = previewLines({run: 'world-1', kind: 'migrate', payload: {version: 3}, outcome: 'done', text: 'Moving onto version b7e1c2d4.', migration: report, moved: false});
    assert.deepEqual(lines, ['Moving onto version b7e1c2d4.', 'Dropped (the new version has no place for it): timer first-freeze, due day 38.0: nothing listens', 'Counts start now.']);
});
