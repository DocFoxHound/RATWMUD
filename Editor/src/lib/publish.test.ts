import assert from 'node:assert/strict';
import {test} from 'node:test';
import {readFileSync} from 'node:fs';
import {BADGE, jobLine, jobRunning, pageTakes, publishBlocked, rebuildQuestion, RESTART_CHOICES, showFrom, showQuery, takes, unpublishedLine,
    type PublishJob, type PublishState} from './publish.ts';

const job = (over: Partial<PublishJob> = {}): PublishJob => ({id: 1, by: 'pell', at: 0, step: 'building', done: 0, of: 0, errors: [], warnings: [],
    build: null, restart: 'later', ...over});
const state = (over: Partial<PublishState> = {}): PublishState => ({
    unpublished: {count: 2, build: 7, through: 40, last: null}, memory: {need: 5, free: 8, limit: null, ok: true, words: 'This computer can publish map changes.'},
    job: null, serverState: 'running', hostOnly: {available: true, reason: ''}, previous: 6, build: null, fallback: null, rebuild: null, perHour: 6, ...over});

test('the count of map changes not published', () => {
    assert.equal(unpublishedLine(0), 'Every map change is published');
    assert.equal(unpublishedLine(1), '1 map change not published');
    assert.equal(unpublishedLine(12), '12 map changes not published');
    assert.match(unpublishedLine(null), /counted from the next Publish/);
});

test('the three ways onto a new map, open only to me first', () => {
    assert.deepEqual(RESTART_CHOICES.map(c => c.id), ['host', 'now', 'later']);
    assert.match(RESTART_CHOICES[0].hint, /Recommended for a first Publish/);
});

test('a job in one line', () => {
    assert.equal(jobLine(null), '');
    assert.equal(jobLine(job({step: 'backup'})), 'Making a backup first…');
    assert.equal(jobLine(job({step: 'building', done: 300, of: 1200})), 'Building the map… 25%');
    assert.equal(jobLine(job({step: 'done', build: 9, restart: 'host'})), 'Published (map 9). The world is open only to you, on the new map.');
    assert.match(jobLine(job({step: 'done', build: 9, restart: 'later'})), /once everyone has left/);
    assert.match(jobLine(job({step: 'refused', errors: [{text: 'a'}, {text: 'b'}]})), /2 things need putting right first\. Nothing was changed\./);
    assert.match(jobLine(job({step: 'failed'})), /Nothing was changed/);
    assert.ok(jobRunning(job({step: 'restarting'})) && !jobRunning(job({step: 'done'})) && !jobRunning(null));
});

test('Show me: a problem\'s place, and only a place', () => {
    assert.equal(showQuery({text: 'x', cell: 'old_inn', x: 4.5, y: 7.5}), 'suite=atlas&show=old_inn,4,7');
    assert.equal(showQuery({text: 'x', cell: 'town'}), 'suite=atlas&show=town');
    assert.equal(showQuery({text: 'x'}), null);
    assert.equal(showQuery({text: 'x', cell: 'a&suite=dm'}), null, 'never a query of its own');
    assert.deepEqual(showFrom('?suite=atlas&show=old_inn,4,7'), {cell: 'old_inn', x: 4, y: 7});
    assert.deepEqual(showFrom('?show=town'), {cell: 'town', x: 0, y: 0});
    assert.equal(showFrom('?show=../x'), null);
    assert.equal(showFrom('?suite=atlas'), null);
});

test('Live now or Waits for Publish, by what is edited', () => {
    for (const key of ['tile:3,4', 'height:3,4', 'rtile:shop:1,1', 'room:shop', 'cell:town', 'link:d', 'chapter:c', 'slot:s', 'spawn', 'name', 'herb'])
        assert.equal(takes(key), 'publish', key);
    for (const key of ['person:mara', 'route:watch', 'faction:f', 'economy']) assert.equal(takes(key), 'live', key);
    assert.equal(pageTakes('dm', ''), 'live');
    assert.equal(pageTakes('atlas', 'workshop'), 'publish');
    assert.equal(pageTakes('atlas', 'npcs'), 'live');
    assert.equal(pageTakes('atlas', 'economy'), 'live');
    assert.equal(pageTakes('atlas', 'publish'), null);
    assert.equal(BADGE.live.label, 'Live now');
    assert.equal(BADGE.publish.label, 'Waits for Publish');
    // The same list as the world's own (tools/world_publish.py).
    const py = readFileSync(new URL('../../../tools/world_publish.py', import.meta.url), 'utf8');
    const kinds = /MAP_KINDS = \(([^)]*)\)/.exec(py)![1].match(/'([a-z]+)'/g)!.map(s => s.slice(1, -1));
    for (const kind of kinds) assert.equal(takes(`${kind}:x`), 'publish', kind);
});

test('the update rebuild\'s question, and why Publish is greyed', () => {
    assert.match(rebuildQuestion({needed: true, unpublished: 3}), /Your 3 unpublished changes would go live: rebuild with them, or discard them first\?/);
    assert.match(rebuildQuestion({needed: true, unpublished: 0}), /needs your map rebuilt before the world can start/);
    assert.match(rebuildQuestion({needed: true, failed: 'The starting place is in a wall.'}), /couldn't be: The starting place is in a wall\./);
    assert.equal(publishBlocked(state(), 'host'), '');
    assert.equal(publishBlocked(state({hostOnly: {available: false, reason: 'This needs your host token to be valid.'}}), 'host'), 'This needs your host token to be valid.');
    assert.equal(publishBlocked(state({hostOnly: {available: false, reason: 'x'}}), 'now'), '');
    assert.match(publishBlocked(state({memory: {need: 5, free: 2, limit: null, ok: false, words: 'This computer can\'t publish yet: it needs 5 GB.'}}), 'now'), /can't publish yet/);
    assert.equal(publishBlocked(state({job: job()}), 'now'), 'The map is being published now.');
});
