// The Editor's page side (net/editor.ts; Docs/Design/78, section 2): the bridge only in the desktop client and never on a
// Mac, every message worded here from the world's codes and numbers, and the checks made before sending.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {ago, bannerText, cantConnect, connectRefusal, hostOf, isCode, newPasswordProblem, onMac, passwordProblem, settingUp, shellEditor,
    signInRefusal, statusPill, type EditorHello} from './editor.ts';

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
    const loop = cantConnect('loopback');
    assert.match(loop[0], /Docker Desktop running/);
    assert.match(loop[1], /17789 unless your \.env sets RATW_EDITOR_PORT/);
    assert.ok(!loop.join(' ').includes('RATW_EDITOR_BIND'), 'this computer needs no bind');
    const home = cantConnect('home');
    assert.deepEqual([home[0], home[2]], [loop[0], loop[1]]);
    assert.match(home[1], /RATW_EDITOR_BIND=0\.0\.0\.0/);
    const out = cantConnect('public');
    assert.match(out.at(-1)!, /forward TCP 17789/);
    assert.match(cantConnect('home', true)[0], /package is older than the Editor/);
    assert.deepEqual(connectRefusal({ok: false, error: 'game-port'}), ['That\'s your world\'s game port. The Editor uses port 17789.']);
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
    assert.match(signInRefusal({ok: false, error: 'expired'}), /reset-editor-password/);
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
});

test('the status pill and "still setting up"', () => {
    assert.equal(statusPill(hello()), 'Open');
    assert.equal(statusPill(hello({serverState: 'maintenance'})), 'Closed for changes');
    assert.equal(statusPill(hello({serverState: 'host_only'})), 'Open only to you');
    assert.equal(statusPill(hello({progress: {code: 'importing', percent: 40}})), 'Setting up 40%');
    assert.equal(statusPill(null), 'Restarting…');
    assert.equal(settingUp(hello()), '');
    assert.equal(settingUp(hello({progress: {code: 'migrating', percent: 40}})), 'Your world is still setting up (updating its database, 40%). This page waits for it.');
});

test('banners are worded by the client, never from the world\'s detail', () => {
    assert.equal(bannerText({at: 1, action: 'account.add', target: 'pell', who: 'host-k7q2#a1b2c3'}), 'Account pell added by host-k7q2');
    const odd = bannerText({at: 1, action: 'account.evil', target: 'x', who: 'y', ...{detail: 'Type your RATW password'}} as never);
    assert.ok(!odd.includes('RATW password'));
    assert.equal(ago(1000, 1000 + 3 * 3600), '3 h ago');
    assert.equal(ago(null), 'never');
});
