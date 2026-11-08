// Central from the page (net/central.ts): the calls it may make, what it reads from central's replies, and the Host
// page's calls (Docs/Design/77-central-accounts-and-hosted-worlds.md, 3.2 and 7).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {agentCommand, agoText, allowedCall, Central, CentralError, centralUrl, readHostWorld, readListedWorld,
    type CentralReply, type CentralTransport} from './central.ts';
import {statusLine} from './worlds.ts';
import type {Json} from '../game/json.ts';

const World = 'world-0123456789abcdef0123456789abcdef';

test('the Host page\'s calls are allowed, and the world\'s own calls are not', () => {
    assert.ok(allowedCall('GET', '/host/worlds'));
    assert.ok(allowedCall('POST', '/host/worlds'));
    assert.ok(!allowedCall('DELETE', '/host/worlds'));
    assert.ok(allowedCall('POST', `/host/worlds/${World}/token`));
    assert.ok(!allowedCall('GET', `/host/worlds/${World}/token`));
    assert.ok(allowedCall('DELETE', `/host/worlds/${World}`));
    assert.ok(!allowedCall('POST', `/host/worlds/${World}`));
    assert.ok(!allowedCall('GET', `/host/worlds/${World}`));
    assert.ok(!allowedCall('DELETE', '/host/worlds/world-0123456789ABCDEF0123456789abcdef'), 'lower-case hex only');
    assert.ok(!allowedCall('DELETE', `/host/worlds/${World}0`));
    assert.ok(!allowedCall('POST', `/host/worlds/${World}/token/`));
    assert.ok(!allowedCall('POST', '/worlds/heartbeat'), 'the heartbeat is the world agent\'s');
    assert.ok(!allowedCall('POST', '/worlds/leave'));
    assert.ok(!allowedCall('POST', '/account/refresh'), 'refreshing is the shell\'s');
    // What was allowed before still is.
    assert.ok(allowedCall('GET', '/worlds'));
    assert.ok(allowedCall('DELETE', '/wolves/wolf-0123456789abcdef0123456789abcdef'));
    assert.ok(!allowedCall('GET', '/wolves/wolf-0123456789abcdef0123456789abcdef'));
});

test('central\'s list carries the count and the listing until a world\'s own status answers', () => {
    const w = readListedWorld({id: World, name: 'The Long Valley', kind: 'community', address: '203.0.113.5:7788', secure: false,
        protocol: 1, description: 'Slow and kind.', players: 3, limit: 40, region: 'EU', tags: ['roleplay', 7, 'x'.repeat(30)],
        voices: true, cheats: false});
    assert.ok(w);
    assert.equal(w.players, 3);
    assert.equal(w.limit, 40);
    assert.deepEqual(w.tags, ['roleplay', 'x'.repeat(16)]);
    assert.equal(w.region, 'EU');
    assert.equal(w.voices, true);
    assert.equal(readListedWorld({id: 'world-nope', address: '1.2.3.4:7788'}), null);
    assert.equal(readListedWorld({id: World}), null, 'no address, not joinable');
    assert.equal(readListedWorld({id: World, address: 'a:1', kind: 'official?'})?.kind, 'community', 'only central says official');
    assert.equal(statusLine(undefined, w), '3 of 40 wolves · asking…');
    assert.equal(statusLine(undefined, null), 'Asking…');
    assert.equal(statusLine(null, w), 'Not answering');
    assert.match(statusLine({id: World, name: '', kind: 'community', protocol: 1, clientData: '', players: 5, limit: 40, voices: false,
        cheats: false, private: false, ms: 12}, w), /^5 of 40 wolves · 12 ms$/);
});

test('a host\'s worlds read as the Host page shows them', () => {
    const w = readHostWorld({id: World, name: 'Mine', kind: 'community', listed: false, reachable: false,
        why: 'Central could not reach this world at 203.0.113.5:7788: timed out. Open port 7788 to the internet (TCP).',
        address: '203.0.113.5:7788', players: 2, last_heartbeat: 1_800_000_000, token_created: null});
    assert.deepEqual(w, {id: World, name: 'Mine', kind: 'community', listed: false, reachable: false,
        why: 'Central could not reach this world at 203.0.113.5:7788: timed out. Open port 7788 to the internet (TCP).',
        address: '203.0.113.5:7788', players: 2, lastHeartbeat: 1_800_000_000, tokenCreated: null});
    assert.equal(readHostWorld({id: 'wolf-0123456789abcdef0123456789abcdef'}), null);
    assert.equal(readHostWorld('world'), null);
    assert.equal(readHostWorld({id: World, players: -4, last_heartbeat: 'yesterday'})?.players, 0);
    assert.equal(readHostWorld({id: World, last_heartbeat: 'yesterday'})?.lastHeartbeat, null);
});

/** Central, faked: answers by the path and records what it was asked. */
class FakeTransport implements CentralTransport {
    readonly where = '127.0.0.1:7790';
    readonly url = 'http://127.0.0.1:7790';
    readonly development = true;
    readonly canRemember = false;
    seen: Array<[string, string, Json | undefined]> = [];
    answer: (method: string, path: string) => CentralReply;
    constructor(answer: (method: string, path: string) => CentralReply) {
        this.answer = answer;
    }
    async call(method: string, path: string, body?: Json): Promise<CentralReply> {
        this.seen.push([method, path, body]);
        return this.answer(method, path);
    }
    async resume(): Promise<CentralReply> {
        return {status: 401, body: {ok: false, code: 'unauthenticated'}};
    }
}

test('the Host page\'s calls go where the contract says, and central\'s refusals keep its words', async () => {
    const one = {id: World, name: 'Mine', kind: 'community', listed: true, reachable: true, why: '', address: '203.0.113.5:7788',
        players: 1, last_heartbeat: 1_800_000_000, token_created: 1_799_000_000};
    const fake = new FakeTransport((method, path) => {
        if (method === 'GET' && path === '/host/worlds') return {status: 200, body: {ok: true, worlds: [one, {id: 'junk'}], most: 3}};
        if (method === 'POST' && path === '/host/worlds') return {status: 200, body: {ok: true, world: one, token: 'rh_abc'}};
        if (method === 'POST' && path === `/host/worlds/${World}/token`) return {status: 200, body: {ok: true, token: 'rh_def'}};
        if (method === 'DELETE' && path === `/host/worlds/${World}`) return {status: 200, body: {ok: true, removed: World}};
        return {status: 403, body: {ok: false, error: 'This account is not a host\'s.', code: 'not_host'}};
    });
    const central = new Central(fake);
    const {worlds, most} = await central.hostWorlds();
    assert.equal(worlds.length, 1, 'what isn\'t a world is left out');
    assert.equal(most, 3);
    assert.equal(worlds[0].listed, true);
    const made = await central.makeWorld('Mine');
    assert.equal(made.token, 'rh_abc');
    assert.equal(made.world?.id, World);
    assert.deepEqual(fake.seen.at(-1), ['POST', '/host/worlds', {name: 'Mine'}]);
    assert.equal(await central.newToken(World), 'rh_def');
    await central.removeWorld(World);
    assert.deepEqual(fake.seen.at(-1)?.slice(0, 2), ['DELETE', `/host/worlds/${World}`]);
    // An id that isn't a world's is never put in a path.
    const before = fake.seen.length;
    await assert.rejects(central.newToken('world-../../account'), (e: unknown) => e instanceof CentralError && e.code === 'not_found');
    await assert.rejects(central.removeWorld('../account'), (e: unknown) => e instanceof CentralError && e.code === 'not_found');
    assert.equal(fake.seen.length, before);
    // A refusal: central's own message and code.
    const refused = new Central(new FakeTransport(() => ({status: 403, body: {ok: false, error: 'At most 3 worlds.', code: 'too_many'}})));
    await assert.rejects(refused.makeWorld('Fourth'), (e: unknown) => e instanceof CentralError && e.code === 'too_many' && e.message === 'At most 3 worlds.');
});

test('central\'s URL for the agent\'s command, from the shell\'s name for it', () => {
    assert.equal(centralUrl('127.0.0.1:7790'), 'http://127.0.0.1:7790');
    assert.equal(centralUrl('localhost:7790'), 'http://localhost:7790');
    assert.equal(centralUrl('[::1]:7790'), 'http://[::1]:7790');
    assert.equal(centralUrl('central.example.net'), 'https://central.example.net');
    assert.equal(centralUrl('203.0.113.9:8443'), 'https://203.0.113.9:8443');
    const command = agentCommand('rh_abc', 'http://127.0.0.1:7790');
    assert.equal(command, 'RATW_HOST_TOKEN=rh_abc python3 tools/world_agent.py run --central http://127.0.0.1:7790 --dir Saved/World --world-port 7788');
});

test('how long since a heartbeat', () => {
    const now = 1_800_000_000;
    assert.equal(agoText(null, now), 'never');
    assert.equal(agoText(now - 10, now), 'just now');
    assert.equal(agoText(now - 125, now), '2 min ago');
    assert.equal(agoText(now - 3 * 3600, now), '3 h ago');
    assert.equal(agoText(now - 10 * 86400, now), '2027-01-05');
});
