import assert from 'node:assert/strict';
import {test} from 'node:test';
import {actorName, asShipped, CLOSED_TEXT, ENDED_PROBE_MS, hostServerDown, inTime, mayCall, needsMigration, NO_MAP_YET, noMapYet, restartingReply,
    shippedOwn, type HostServer} from './hosted.ts';

const server = (h: Partial<HostServer>): HostServer => ({up: false, reason: 'exit', code: null, failures: 0, retryIn: null, ...h});

test('the Server tab never says Running while the package says its server is down (R1, H2)', () => {
    assert.equal(hostServerDown(server({code: 11, failures: 3, retryIn: 120})), 'Keeps stopping (exit 11): starts again in 2 min');
    assert.equal(hostServerDown(server({code: 11, failures: 1, retryIn: 5})), 'Stopped (exit 11): starts again in 5 s');
    assert.equal(hostServerDown(server({failures: 1})), 'Stopped: starting again');
    assert.equal(hostServerDown(server({reason: 'world.toml', retryIn: 30})), 'Stopped: world.toml has a mistake (see the log: docker compose logs world)');
    assert.equal(hostServerDown(server({reason: 'restarting'})), 'Restarting…');
    assert.equal(hostServerDown(server({reason: 'restarting', up: true})), 'Restarting…');
    assert.equal(hostServerDown(server({reason: 'starting'})), 'Starting…');
    assert.equal(hostServerDown(server({reason: 'closed'})), CLOSED_TEXT);
    assert.equal(hostServerDown(server({up: true, reason: 'running'})), '');
    assert.equal(hostServerDown(null), '', 'an older package says nothing: the tab reads the database as before');
    assert.deepEqual([0, 5, 59, 60, 119, 600, 7200].map(inTime), ['0 s', '5 s', '59 s', '1 min', '2 min', '10 min', '2 h']);
});

test('a restarting world is the shell\'s 503 "restarting" (or an older shell\'s 502)', () => {
    assert.ok(restartingReply(503, {error: 'restarting'}));
    assert.ok(restartingReply(502, {error: 'The world didn\'t answer.'}));
    assert.ok(!restartingReply(503, {error: 'busy'}) && !restartingReply(401, {error: 'restarting'}) && !restartingReply(200, {}));
});

test('after the session ended, reads go out once a half minute, not every poll (R1, low)', () => {
    const t0 = 1_000_000_000;
    assert.ok(mayCall(t0, false) && mayCall(t0 + 1, false), 'a live session: always');
    assert.ok(mayCall(t0, true), 'the first probe');
    assert.ok(!mayCall(t0 + 2000, true) && !mayCall(t0 + ENDED_PROBE_MS - 1, true));
    assert.ok(mayCall(t0 + ENDED_PROBE_MS, true));
});

test('hosted pages name accounts and defaults in the host\'s words (R1: "by dm-devtest", "market.json")', () => {
    assert.equal(actorName('pell#3f2a1c', true), 'pell');
    assert.equal(actorName('dm-devtest', true), 'the DM');
    assert.equal(actorName('', true), '');
    assert.equal(actorName('dm-devtest', false), 'dm-devtest', 'the dev studio keeps its accounts');
    assert.equal(shippedOwn('market', true), 'the default\'s');
    assert.equal(shippedOwn('market', false), 'market.json\'s');
    assert.equal(asShipped('status', true), 'Back to the default');
    assert.match(needsMigration('its own list', true), /newer package/);
    assert.match(needsMigration('its own list', false), /migration 0061/);
});

test('LIVE says why a world with no map source shows no map, and not a black square (M6\'s run)', () => {
    assert.equal(noMapYet({cells: []}), true);
    assert.equal(noMapYet({cells: [{}]}), false);
    assert.equal(noMapYet(null), false, 'still loading: "Loading the world…" as before');
    assert.match(NO_MAP_YET, /comes with map editing, in a later update/);
});
