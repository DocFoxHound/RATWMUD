// Central from the page (net/central.ts): the calls it may make, what it reads from central's replies, and the Host
// page's calls (Docs/Design/77-central-accounts-and-hosted-worlds.md, 3.2 and 7).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {agoText, allowedCall, baseUrl, Central, CentralError, envLine, readHostWorld, readListedWorld, shellWord, starConfirmation,
    worldTomlLines, ShellCentral, type CentralReply, type CentralTransport} from './central.ts';
import {APPROVED_TITLE, HOST_SEES, kindBadges, parseAddress, readStatus, serverColumns, shownAddress, statusLine,
    UNAPPROVED_TITLE} from './worlds.ts';
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
    assert.ok(allowedCall('POST', '/account/quickened/announced'), 'the Quickened fanfare shown (doc 49, 9.1)');
    assert.ok(!allowedCall('GET', '/account/quickened/announced'));
    assert.ok(allowedCall('POST', '/stars/confirm'), 'a star confirmed by its giver (doc 77, approved worlds)');
    assert.ok(!allowedCall('GET', '/stars/confirm'));
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
    assert.equal(w.quickenedWolves, true, 'nothing said: Quickened wolves may join (doc 49, 9.1)');
    assert.equal(readListedWorld({id: World, address: 'a:1', quickenedWolves: false})?.quickenedWolves, false);
    assert.equal(readListedWorld({id: 'world-nope', address: '1.2.3.4:7788'}), null);
    assert.equal(readListedWorld({id: World}), null, 'no address, not joinable');
    assert.equal(readListedWorld({id: World, address: 'a:1', kind: 'official?'})?.kind, 'community', 'only central says official');
    assert.equal(w.host, '', 'no host name, the address shown');
    assert.equal(statusLine(undefined, w), '3 of 40 wolves · asking… · AI used', 'the listing says the world uses AI');
    assert.equal(statusLine(undefined, null), 'Asking…');
    // "AI used" and "cheats on" from central's listing until the world answers, and from either afterwards (doc 78, 9).
    assert.equal(statusLine(undefined, {...w, voices: false, cheats: true}), '3 of 40 wolves · asking… · cheats on');
    assert.match(statusLine({id: World, name: '', kind: 'community', protocol: 1, clientData: '', players: 5, limit: 40, voices: true,
        cheats: false, private: false, quickenedWolves: true, ms: 12}, w), /^5 of 40 wolves · 12 ms · AI used$/);
    assert.match(statusLine({id: World, name: '', kind: 'community', protocol: 1, clientData: '', players: 5, limit: 40, voices: false,
        cheats: false, private: false, quickenedWolves: true, ms: 12}, {...w, voices: false, cheats: true}), /^5 of 40 wolves · 12 ms · cheats on$/);
    assert.match(HOST_SEES, /read everything you say and do there, and sees your internet address/);
    assert.equal(statusLine(null, w), 'Not answering');
    assert.match(statusLine({id: World, name: '', kind: 'community', protocol: 1, clientData: '', players: 5, limit: 40, voices: false,
        cheats: false, private: false, quickenedWolves: true, ms: 12}, {...w, voices: false}), /^5 of 40 wolves · 12 ms$/);
});

test('a world says whether Quickened wolves may join, allowed when it says nothing (doc 49, 9.1)', () => {
    const status = {ratw: 'world', id: World, protocol: 1, players: 1, limit: 40};
    assert.equal(readStatus(status, 5)?.quickenedWolves, true, 'an older world: allowed');
    assert.equal(readStatus({...status, quickenedWolves: false}, 5)?.quickenedWolves, false);
    assert.equal(readStatus({...status, quickenedWolves: 'no'}, 5)?.quickenedWolves, true, 'only false keeps them out');
    // The server browser's tag: "No Quickened", from central's listing until the world answers, and from either after.
    const listed = {players: 1, limit: 40, voices: false, cheats: false, quickenedWolves: false};
    assert.deepEqual(serverColumns(undefined, listed).marks, ['No Quickened']);
    assert.equal(serverColumns(undefined, listed).quickenedWolves, false);
    assert.deepEqual(serverColumns(readStatus(status, 5), {...listed, quickenedWolves: true}).marks, [], 'allowed: no tag');
    assert.deepEqual(serverColumns(readStatus({...status, quickenedWolves: false, cheats: true}, 5), {...listed, quickenedWolves: true}).marks,
        ['cheats on', 'No Quickened'], 'the world\'s own word too');
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

test('a world listed under a host name: connected to (and pinged) at its address, shown by its name', () => {
    // ws://: the address is the IP central called back; the name is for showing only (doc 77, 7: no DNS rebinding).
    const plain = readListedWorld({id: World, name: 'Valley', address: '203.0.113.5:7788', host: 'Valley.Example.net', secure: false});
    assert.ok(plain);
    assert.equal(plain.address, '203.0.113.5:7788');
    assert.equal(plain.host, 'valley.example.net');
    const at = parseAddress(plain.address, plain.secure);
    assert.ok(at);
    assert.equal(at.host, '203.0.113.5', 'the client connects to the address, never the name');
    assert.equal(shownAddress(at, plain.host), 'valley.example.net:7788');
    assert.equal(shownAddress(at, ''), '203.0.113.5:7788');
    // wss://: the address is host:port (the certificate binds the name), the IP beside it.
    const secure = readListedWorld({id: World, address: 'valley.example.net:443', host: 'valley.example.net', ip: '203.0.113.5', secure: true});
    assert.equal(secure?.address, 'valley.example.net:443');
    // Anything that isn't a host name is left out, never shown.
    for (const bad of [null, 7, '', 'valley', '<b>x</b>.net', 'a b.net', '-a.net', 'a..net', `${'a'.repeat(64)}.net`])
        assert.equal(readListedWorld({id: World, address: '203.0.113.5:7788', host: bad as Json[string]})?.host, '', String(bad));
});

test('central\'s URL comes from the shell, scheme and all, never guessed from the host', () => {
    const invoke = async <T>() => ({}) as T;
    assert.equal(new ShellCentral({where: '127.0.0.1:7790', url: 'https://127.0.0.1:7790', development: true}, invoke).url, 'https://127.0.0.1:7790');
    assert.equal(new ShellCentral({where: '127.0.0.1:7790', url: 'http://127.0.0.1:7790', development: true}, invoke).url, 'http://127.0.0.1:7790');
    assert.equal(new ShellCentral({where: '[::1]:7790', url: 'http://[::1]:7790', development: true}, invoke).url, 'http://[::1]:7790');
    assert.equal(new ShellCentral({where: 'central.example.net', url: 'https://central.example.net', development: false}, invoke).url,
        'https://central.example.net');
    // No URL from the shell, or not one: '' (the Host page then says URL), never a guess.
    assert.equal(new ShellCentral({where: '127.0.0.1:7790', development: true}, invoke).url, '');
    for (const bad of ['127.0.0.1:7790', 'ftp://a.net', 'https://a.net/x', 'https://a.net; rm -rf ~', 'https://a.net:7790 --x', 7]) assert.equal(baseUrl(bad), '', String(bad));
});

test('the Host page\'s lines for the package: the token in .env, the id and central in world.toml', () => {
    assert.equal(envLine('rh_abc'), 'RATW_HOST_TOKEN=rh_abc');
    assert.equal(worldTomlLines(World, 'https://127.0.0.1:7790'), `[world]\nid = "${World}"\n\n[central]\nurl = "https://127.0.0.1:7790"`);
    assert.equal(worldTomlLines('a"b\\c\n', 'URL'), '[world]\nid = "a\\"b\\\\c"\n\n[central]\nurl = "URL"', 'quoted, whatever it holds');
    // A name is one word to the shell, whatever is in it.
    assert.equal(shellWord("Ash's $(rm -rf ~) `x`"), `'Ash'\\''s $(rm -rf ~) \`x\`'`);
    assert.equal(shellWord(''), "''");
});

test('how long since a heartbeat', () => {
    const now = 1_800_000_000;
    assert.equal(agoText(null, now), 'never');
    assert.equal(agoText(now - 10, now), 'just now');
    assert.equal(agoText(now - 125, now), '2 min ago');
    assert.equal(agoText(now - 3 * 3600, now), '3 h ago');
    assert.equal(agoText(now - 10 * 86400, now), '2027-01-05');
});

test('an approved world reads as approved, and anything but true as not (doc 77, approved worlds)', () => {
    const w = {id: World, name: 'Fen', kind: 'community', address: '203.0.113.5:7788'};
    assert.equal(readListedWorld({...w, approved: true})?.approved, true);
    assert.equal(readListedWorld(w)?.approved, false, 'an older central says nothing: not approved');
    assert.equal(readListedWorld({...w, approved: 'yes'})?.approved, false);
    assert.equal(readListedWorld({...w, kind: 'official', approved: true})?.approved, true);
});

test('the server browser marks an approved world APPROVED, and says where progress stays otherwise', () => {
    assert.deepEqual(kindBadges('community', true).map(b => b.text), ['COMMUNITY', 'APPROVED']);
    assert.equal(kindBadges('community', true)[1].className, 'approved');
    assert.equal(kindBadges('community', true)[1].title, APPROVED_TITLE);
    assert.match(APPROVED_TITLE, /unmodified world software and progression rules.*whole account/);
    assert.deepEqual(kindBadges('community', false).map(b => b.text), ['COMMUNITY']);
    assert.equal(kindBadges('community', false)[0].title, UNAPPROVED_TITLE);
    assert.match(UNAPPROVED_TITLE, /stays in this world/);
    // An official world is always approved: its own badge says so.
    assert.deepEqual(kindBadges('official', true).map(b => [b.text, b.className, b.title]), [['OFFICIAL', 'official', APPROVED_TITLE]]);
});

test('giving a star in a ticket world sends its giver\'s confirmation to central (doc 77, approved worlds)', async () => {
    const giver = 'wolf-0123456789abcdef0123456789abcdef', other = 'wolf-fedcba9876543210fedcba9876543210';
    assert.deepEqual(starConfirmation({type: 'social', verb: 'star', session: 's1', target: other}, World, giver),
        {world: World, wolf: giver, to: other, kind: 'gold'});
    assert.deepEqual(starConfirmation({type: 'social', verb: 'storystar', story: 'st1', target: other}, World, giver),
        {world: World, wolf: giver, to: other, kind: 'story'});
    assert.deepEqual(starConfirmation({type: 'festival', verb: 'star', target: other}, World, giver),
        {world: World, wolf: giver, to: other, kind: 'festival'});
    // Nothing for anything else, a target that isn't a central wolf, or no ticket world or wolf.
    assert.equal(starConfirmation({type: 'social', verb: 'note', target: other}, World, giver), null);
    assert.equal(starConfirmation({type: 'festival', verb: 'cheer', target: other}, World, giver), null);
    assert.equal(starConfirmation({type: 'storyteller', verb: 'star', to: other}, World, giver), null);
    assert.equal(starConfirmation({type: 'social', verb: 'star', target: 'npc_tamsin'}, World, giver), null);
    assert.equal(starConfirmation({type: 'social', verb: 'star', target: giver}, World, giver), null, 'never oneself');
    assert.equal(starConfirmation({type: 'social', verb: 'star', target: other}, '', giver), null, 'no ticket world');
    assert.equal(starConfirmation({type: 'social', verb: 'star', target: other}, World, ''), null, 'no wolf');
    assert.equal(starConfirmation({type: 'social', verb: 'star', target: other}, World, 'dev:ash'), null);
    // Sent as the signed-in giver, through the transport (the shell holds the token).
    const calls: [string, string, Json | undefined][] = [];
    const transport: CentralTransport = {where: 'test', url: 'http://127.0.0.1:7790', development: true, canRemember: false,
        call: async (method, path, body) => { calls.push([method, path, body]); return {status: 200, body: {ok: true}}; },
        resume: async () => ({status: 401, body: {ok: false}})};
    const body = starConfirmation({type: 'social', verb: 'star', target: other}, World, giver)!;
    assert.deepEqual(await new Central(transport).confirmStar(body), {ok: true});
    assert.deepEqual(calls, [['POST', '/stars/confirm', {world: World, wolf: giver, to: other, kind: 'gold'}]]);
});
