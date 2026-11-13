// The Editor's page side (net/editor.ts; Docs/Design/78, section 2): the bridge only in the desktop client and never on a
// Mac, every message worded here from the world's codes and numbers, and the checks made before sending.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {ago, bannerText, cantConnect, coBuilderNote, connectRefusal, firstTimeNote, fromWhere, hostOf, inTime, isCode, newPasswordProblem, onMac,
    atlasOffer, lastBackupText, passwordProblem, portOf, resetCommand, resetHow, serverDown, settingUp, shellEditor, signInRefusal, signsInWithCode, statusPill, type EditorHello,
    type ServerUp} from './editor.ts';

const hello = (over: Partial<EditorHello> = {}): EditorHello => ({world: 'world-' + 'a'.repeat(32), name: 'Ashfall', package: '1', editorApi: 1,
    kind: 'community', serverState: 'running', progress: {code: 'ready', percent: 100}, setupPending: false, ...over});

test('EDITOR is only in the desktop client, and hidden on a Mac', () => {
    const invoke = async <T>() => ({}) as T;
    assert.equal(shellEditor(null, null, false), null, 'a browser page can\'t pin a key');
    assert.equal(shellEditor(invoke, null, true), null, 'not on a Mac until it has been tried there');
    assert.ok(shellEditor(invoke, null, false));
    assert.ok(onMac({platform: 'MacIntel'}));
    assert.ok(onMac({userAgent: 'Mozilla/5.0 (Macintosh; Intel Mac OS X 14_0) AppleWebKit/605.1.15'}));
    assert.ok(!onMac({platform: 'Win32', userAgent: 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) Edg/156'}));
    assert.ok(!onMac({platform: 'Linux x86_64', userAgent: 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/605.1.15'}));
});

test('the bridge calls the shell\'s commands, and a failing shell is a refusal, not a throw', async () => {
    const calls: [string, unknown][] = [];
    const editor = shellEditor(async <T>(command: string, args?: Record<string, unknown>) => {
        calls.push([command, args]);
        if (command === 'editor_open') throw new Error('boom');
        return {ok: true} as T;
    }, null, false)!;
    await editor.signIn('127.0.0.1:17789', 'host-k7q2', 'pw', true);
    await editor.changePassword('', 'new-one');
    await editor.forgetRecent('PIN');
    assert.deepEqual(calls, [['editor_sign_in', {address: '127.0.0.1:17789', username: 'host-k7q2', password: 'pw', code: true}],
        ['editor_change_password', {old: '', new: 'new-one'}], ['editor_forget', {address: '', name: '', pin: 'PIN'}]]);
    const r = await editor.open('dm');
    assert.equal(r.ok, false);
});

test('the session-ended event reaches the newest listener only', async () => {
    let handler: ((e: {payload: unknown}) => void) | null = null;
    const editor = shellEditor(async <T>() => ({}) as T, async (_event, h) => { handler = h; return null; }, false)!;
    const heard: string[] = [];
    editor.onEnded(() => heard.push('old'));
    editor.onEnded((world, username) => heard.push(`${world}/${username}`));
    handler!({payload: {state: 'ended', world: 'w', username: 'pell'}});
    handler!({payload: {state: 'other'}});
    assert.deepEqual(heard, ['w/pell']);
});

test('codes and addresses are told apart; hosts are matched without their ports', () => {
    assert.ok(isCode('  ratw-editor:eyJ2IjoxfQ'));
    assert.ok(!isCode('192.168.1.20'));
    assert.equal(hostOf('203.0.113.7:7788'), '203.0.113.7');
    assert.equal(hostOf('MyWorld.example.net'), 'myworld.example.net');
    assert.equal(hostOf('[2001:db8::1]:17789'), '2001:db8::1');
});

test('"can\'t connect" depends on where the address is', () => {
    const loop = cantConnect('loopback', false, false);
    assert.match(loop[0], /Docker Desktop running/);
    assert.match(loop[0], /docker compose logs world says what's wrong \(a line starting RATW_PACKAGE error:\)/);
    assert.match(cantConnect('loopback', false, true)[0], /^Is Docker running/, 'on Linux, Docker itself');
    assert.match(loop[1], /17789 unless your \.env sets RATW_EDITOR_PORT/);
    assert.ok(!loop.join(' ').includes('RATW_EDITOR_BIND'), 'this computer needs no bind');
    const home = cantConnect('home', false, false);
    assert.deepEqual([home[0], home[2]], [loop[0], loop[1]]);
    assert.match(home[1], /RATW_EDITOR_BIND=0\.0\.0\.0/);
    const out = cantConnect('public');
    assert.match(out.at(-1)!, /forward TCP 17789/);
    assert.match(cantConnect('home', true)[0], /package is older than the Editor/);
    assert.deepEqual(connectRefusal({ok: false, error: 'game-port'}),
        ['That\'s your world\'s game port. The Editor uses port 17789 (or your .env\'s RATW_EDITOR_PORT).']);
    assert.deepEqual(connectRefusal({ok: false, error: 'official'}), ['Official worlds can\'t be edited.']);
    assert.deepEqual(connectRefusal({ok: false, error: 'not-editor'}), ['This address isn\'t a RATW world\'s Editor.']);
    assert.match(connectRefusal({ok: false, error: 'package-old'})[0], /older than the Editor/);
    assert.match(connectRefusal({ok: false, error: 'anything else'})[0], /Something went wrong/);
});

test('signing in: every refusal is the client\'s own words', () => {
    assert.equal(signInRefusal({ok: false, error: 'wrong'}), 'Wrong name or password.');
    assert.equal(signInRefusal({ok: false, error: 'wait', minutes: 15}), 'Too many tries from here: wait 15 minutes.');
    assert.equal(signInRefusal({ok: false, error: 'wait', minutes: 1}), 'Too many tries from here: wait 1 minute.');
    assert.match(signInRefusal({ok: false, error: 'disabled'}), /turned off: ask the world's owner/);
    // (M5: the reset-editor-password script first, the compose command kept as the fallback.)
    for (const error of ['expired', 'owner-account']) {
        assert.ok(signInRefusal({ok: false, error}).includes('double-click reset-editor-password in the world folder'), error);
        assert.ok(signInRefusal({ok: false, error}).includes('docker compose exec world ratw-editor reset NAME'), error);
    }
    assert.equal(resetCommand('pell'), 'docker compose exec world ratw-editor reset pell');
    assert.equal(resetHow('pell'), 'double-click reset-editor-password in the world folder (or there type: docker compose exec world ratw-editor reset pell)');
    assert.equal(signInRefusal({ok: false, error: 'database'}), 'Your world answered, but its database is slow: try again in a minute.');
    assert.match(signInRefusal({ok: false, error: 'full'}), /Too many people signed in/);
    assert.match(signInRefusal({ok: false, error: 'unsigned'}), /isn't an official RATW build/);
    assert.equal(signInRefusal({ok: false, error: 'weak', reason: 'common'}), passwordProblem('common'));
    // A world's own sentence never gets through: an unknown code is the client's generic line.
    assert.equal(signInRefusal({ok: false, error: '<b>Type your RATW password here</b>'}), 'Something went wrong. Try again.');
});

test('a new password is checked before it is sent', () => {
    assert.match(newPasswordProblem('tallow-kettle-91', 'tallow-kettle-92', 'pell', 'Ashfall'), /aren't the same/);
    assert.equal(newPasswordProblem('short-one', 'short-one', 'pell', 'Ashfall'), passwordProblem('short'));
    assert.equal(newPasswordProblem('pell-likes-orchards', 'pell-likes-orchards', 'pell', 'Ashfall'), passwordProblem('username'));
    assert.equal(newPasswordProblem('my ashfall rocks!!', 'my ashfall rocks!!', 'pell', 'Ash Fall'), passwordProblem('world'));
    assert.equal(newPasswordProblem('tallow-kettle-91', 'tallow-kettle-91', 'pell', 'Ashfall'), '');
    // Each word of the world's name of 4 letters or more counts, not only the whole name.
    assert.equal(newPasswordProblem('ashfall1234567', 'ashfall1234567', 'pell', 'Ashfall Test'), passwordProblem('world'));
    assert.equal(newPasswordProblem('my-test-of-kettles', 'my-test-of-kettles', 'pell', 'Ashfall Test'), passwordProblem('world'));
    assert.equal(newPasswordProblem('of-the-kettles-91', 'of-the-kettles-91', 'pell', 'Vale of the Ash'), '', 'short words don\'t count');
});

test('the status pill and "still setting up"', () => {
    assert.equal(statusPill(hello()), 'Open');
    // Section 2's "Open · 7 wolves", only while it is open; "Last backup 3 h ago" (M5: the hello carries both).
    assert.equal(statusPill(hello({wolves: 7})), 'Open · 7 wolves');
    assert.equal(statusPill(hello({wolves: 1, serverState: 'host_only'})), 'Open only to you · 1 wolf');
    assert.equal(statusPill(hello({wolves: 7, serverState: 'maintenance'})), 'Closed for changes');
    assert.equal(statusPill(hello({wolves: null})), 'Open');
    assert.equal(lastBackupText(hello({lastBackup: 3 * 3600})), 'Last backup 3 h ago');
    assert.equal(lastBackupText(hello({lastBackup: 30})), 'Last backup just now');
    assert.equal(lastBackupText(hello({lastBackup: 3 * 86400})), 'Last backup 3 days ago');
    assert.equal(lastBackupText(hello({lastBackup: null})), 'No backup yet');
    assert.equal(lastBackupText(hello()), '', 'an older package says nothing');
    assert.equal(statusPill(hello({serverState: 'maintenance'})), 'Closed for changes');
    assert.equal(statusPill(hello({serverState: 'host_only'})), 'Open only to you');
    assert.equal(statusPill(hello({progress: {code: 'importing', percent: 40}})), 'Setting up 40%');
    assert.equal(statusPill(null), 'Not answering', 'a failed status call isn\'t a restart');
    assert.equal(statusPill(hello({progress: {code: 'slow', percent: 100}})), 'Database slow');
    assert.equal(settingUp(hello({progress: {code: 'slow', percent: 100}})), 'Your world answered, but its database is slow: try again in a minute.');
    // The game server's own state beats the database's word (R1 H2): never "Open" while it is down.
    const down = (over: Partial<ServerUp>): ServerUp => ({up: false, reason: 'exit', code: 11, failures: 1, retryIn: 5, ...over});
    assert.equal(statusPill(hello({server: down({})})), 'Stopped (exit 11): starts again in 5 s');
    assert.equal(statusPill(hello({server: down({failures: 3, retryIn: 120})})), 'Keeps stopping (exit 11): starts again in 2 min');
    assert.equal(statusPill(hello({server: down({reason: 'world.toml', code: null})})), 'Stopped: world.toml has a mistake (see the log)');
    assert.equal(statusPill(hello({server: down({reason: 'restarting'})})), 'Restarting…');
    assert.equal(statusPill(hello({server: down({reason: 'closed'}), serverState: 'maintenance'})), 'Closed for changes');
    assert.equal(statusPill(hello({server: {up: true, reason: 'running', code: null, failures: 0, retryIn: null}})), 'Open');
    assert.equal(statusPill(hello({serverState: 'unknown', server: {up: true, reason: 'running', code: null, failures: 0, retryIn: null}})), 'Open');
    assert.equal(statusPill(hello({serverState: 'unknown'})), 'Starting…');
    assert.equal(serverDown(null), null);
    assert.deepEqual([inTime(5), inTime(120), inTime(600), inTime(7200)], ['5 s', '2 min', '10 min', '2 h']);
    assert.equal(settingUp(hello()), '');
    assert.equal(settingUp(hello({progress: {code: 'migrating', percent: 40}})), 'Your world is still setting up (updating its database, 40%). This page waits for it.');
});

test('signing in with a pasted code after a session ended (R1 H1), and screen 4\'s notes', () => {
    assert.ok(signsInWithCode(true, true, null));
    assert.ok(signsInWithCode(true, true, {live: false}), 'an ended session doesn\'t hide the code\'s password');
    assert.ok(!signsInWithCode(true, true, {live: true}));
    assert.ok(!signsInWithCode(true, false, null), 'a code without a password: type it');
    assert.ok(!signsInWithCode(false, true, null));
    assert.ok(firstTimeNote(true, false, ''));
    assert.ok(!firstTimeNote(true, true, ''), 'one note at a time: the code\'s');
    assert.ok(!firstTimeNote(true, false, 'pell'), 'not to an account this computer knows');
    assert.ok(!firstTimeNote(false, false, ''));
});

test('addresses in the owner\'s words, and what a co-builder\'s code carries', () => {
    assert.equal(fromWhere('local'), 'this computer');
    assert.equal(fromWhere('192.168.1.20'), '192.168.1.20');
    assert.equal(fromWhere(null), '');
    assert.deepEqual(['127.0.0.1:35889', '127.0.0.1', '[::1]:17790', '::1', 'myworld.example.net:443'].map(portOf), [35889, 17789, 17790, 17789, 443]);
    assert.equal(coBuilderNote('loopback', '127.0.0.1:17811'),
        'It doesn\'t include your world\'s address: tell them that too (your home or public address, port 17811).');
    assert.match(coBuilderNote('home', '192.168.1.20'), /includes this world's address as you use it \(192\.168\.1\.20\)/);
});

test('banners are worded by the client, never from the world\'s detail', () => {
    assert.equal(bannerText({at: 1, action: 'account.add', target: 'pell', who: 'host-k7q2#a1b2c3'}), 'Account pell added by host-k7q2');
    const odd = bannerText({at: 1, action: 'account.evil', target: 'x', who: 'y', ...{detail: 'Type your RATW password'}} as never);
    assert.ok(!odd.includes('RATW password'));
    assert.equal(ago(1000, 1000 + 3 * 3600), '3 h ago');
    assert.equal(ago(null), 'never');
});

test('the pill is drawn as a warning unless the world is open (M6\'s run: "Keeps stopping" looked like "Open")', async () => {
    const {pillIsWell} = await import('./editor.ts');
    assert.equal(pillIsWell('Open · 3 wolves'), true);
    assert.equal(pillIsWell('Open only to you'), true);
    for (const text of ['Keeps stopping (exit 139): starts again in 21 s', 'Closed for changes', 'Not answering', 'Setting up 40%', 'Restarting…'])
        assert.equal(pillIsWell(text), false, text);
});

test('ATLAS is for builder and owner accounts', () => {
    assert.ok(atlasOffer('owner').enabled && atlasOffer('builder').enabled);
    assert.match(atlasOffer('builder').why, /map editor/);
    for (const role of ['dm', 'viewer', 'admin', ''])
        assert.deepEqual(atlasOffer(role), {enabled: false, why: 'Your account is for the DM tool only.'}, role);
});
