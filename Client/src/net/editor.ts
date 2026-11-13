// The Editor's side of the page (Docs/Design/78-hosts-run-their-own-worlds.md, sections 2 and 6.6): the shell's editor_*
// commands, typed, and every message the Editor's screens show, worded here. A world sends codes and numbers, never
// sentences the client shows (I12): the shell checks its hello and passes on only an error code, minutes and a reason.
//
// The password is typed in the main window and handed to the shell once; the session token never reaches the page.

/** A world's hello as the shell passes it on: checked, its name cleaned and cut to 40. */
export interface EditorHello {
    world: string;
    name: string;
    package: string;
    editorApi: number;
    kind: 'community' | 'official';
    serverState: string;          // running | maintenance | host_only | unknown
    progress: {code: string; percent: number};
    setupPending: boolean;
    /** The game server as the world's entrypoint last saw it (null: it said nothing, e.g. still setting up). */
    server?: ServerUp | null;
    /** The wolves in the world now (the game's public count; null while its server is down or doesn't say), and the
     *  newest backup's age in seconds (null: none yet, or a package without the backup service). Doc 78, section 2. */
    wolves?: number | null;
    lastBackup?: number | null;
}

/** The hello's word on the game server (Package/entrypoint.py's server.json, through the helper and the shell). */
export interface ServerUp {
    up: boolean;
    reason: 'starting' | 'running' | 'restarting' | 'exit' | 'world.toml' | 'closed';
    code: number | null;
    failures: number;
    retryIn: number | null;
}

/** The owner's counts at sign-in (never addresses or names: those are behind the password, in Editor accounts). */
export interface OwnerNotices {failedToday: number; changes: number}

export type Place = 'loopback' | 'home' | 'public';
export type KeyState = 'new' | 'trusted' | 'moved' | 'rotated' | 'changed';

export interface Connected {
    ok: true;
    state: KeyState;
    address: string;
    place: Place;
    fingerprint: string;
    previous: string | null;
    hello: EditorHello;
    username: string;
    code: boolean;                // An Editor code is waiting for this world.
}

export interface Refusal {
    ok: false;
    error: string;
    status?: number;
    minutes?: number;
    reason?: string;
    address?: string;
    place?: Place;
    gameAnswers?: boolean;
}

export interface EditorSession {
    world: string;
    name: string;
    address: string;
    place: Place;
    username: string;
    role: 'viewer' | 'dm' | 'builder' | 'owner';
    mustChange: boolean;
    live: boolean;
    fingerprint: string;
}

export interface RecentWorld {world: string; name: string; address: string; username: string; pin: string}
export interface EditorState {available: boolean; session: EditorSession | null; window: boolean; recent: RecentWorld[]}

export interface EditorAccount {
    username: string; role: string; disabled: boolean; mustChange: boolean; expired: boolean; unclaimed: boolean;
    created: number; lastLogin: number | null; claimedFrom: string | null; lastFrom: string | null; mayActOnWolves: boolean;
    createdBy: string | null; devices: {created: number; lastUsed: number}[];
}
export interface AuditBanner {at: number; action: string; target: string; who: string}

type Reply<T> = Promise<(T & {ok: true}) | Refusal>;

/** The shell's commands for the Editor (Desktop/src-tauri/src/editor.rs). */
export interface EditorBridge {
    state(): Promise<EditorState>;
    connect(address: string): Reply<Connected>;
    useCode(code: string): Reply<{address: string | null; username: string; world: string; fingerprint: string; password: boolean}>;
    trust(address: string, fingerprint: string): Reply<object>;
    forget(address: string, name: string): Reply<object>;
    forgetRecent(pin: string): Reply<object>;
    signIn(address: string, username: string, password: string, code: boolean): Reply<{session: EditorSession; notices?: OwnerNotices | null}>;
    changePassword(old: string, fresh: string): Reply<{session: EditorSession}>;
    stepUp(password: string): Reply<{minutes: number}>;
    accounts(verb: string, body?: Record<string, unknown>): Reply<{accounts?: EditorAccount[]; failedToday?: number; banners?: AuditBanner[]; code?: string}>;
    signOut(): Reply<object>;
    status(): Reply<{hello: EditorHello}>;
    open(suite: 'dm' | 'atlas'): Reply<object>;
    /** The shell's word that the session ended (a 401 from the world, in the Editor window or here). */
    onEnded(listener: (world: string, username: string) => void): void;
}

type Invoke = <T>(command: string, args?: Record<string, unknown>) => Promise<T>;
type Listen = (event: string, handler: (e: {payload: unknown}) => void) => Promise<unknown>;

/** Whether this is a Mac: the Editor stays hidden there until it has been tried on WKWebView (doc 78, Q26). */
export function onMac(nav: {platform?: string; userAgent?: string} = typeof navigator === 'undefined' ? {} : navigator): boolean {
    return /^Mac/i.test(nav.platform ?? '') || /Macintosh|Mac OS X/.test(nav.userAgent ?? '');
}

/** The bridge, in the desktop client only (a browser page can't pin a key), never on a Mac. */
export function shellEditor(invoke: Invoke | null, listen: Listen | null, mac = onMac()): EditorBridge | null {
    if (!invoke || mac) return null;
    const call = <T>(command: string, args?: Record<string, unknown>) =>
        invoke<T>(command, args).catch(error => ({ok: false, error: 'shell', message: String(error)}) as unknown as T);
    // One listener at a time (the newest front door's), on one subscription.
    let ended: ((world: string, username: string) => void) | null = null;
    void listen?.('editor-session', e => {
        const p = (e.payload ?? {}) as {state?: unknown; world?: unknown; username?: unknown};
        if (p.state === 'ended') ended?.(String(p.world ?? ''), String(p.username ?? ''));
    });
    return {
        state: () => invoke<EditorState>('editor_state'),
        connect: address => call('editor_connect', {address}),
        useCode: code => call('editor_use_code', {code}),
        trust: (address, fingerprint) => call('editor_trust', {address, fingerprint}),
        forget: (address, name) => call('editor_forget', {address, name}),
        forgetRecent: pin => call('editor_forget', {address: '', name: '', pin}),
        signIn: (address, username, password, code) => call('editor_sign_in', {address, username, password, code}),
        changePassword: (old, fresh) => call('editor_change_password', {old, new: fresh}),
        stepUp: password => call('editor_step_up', {password}),
        accounts: (verb, body) => call('editor_accounts', {verb, body: body ?? null}),
        signOut: () => call('editor_sign_out'),
        status: () => call('editor_status'),
        open: suite => call('editor_open', {suite}),
        onEnded: listener => { ended = listener; },
    };
}

export const EDITOR_PORT = 17789;
export const CODE_PREFIX = 'ratw-editor:';

/** Whether a pasted string is an Editor code rather than an address. */
export const isCode = (text: string) => text.trim().startsWith(CODE_PREFIX);

/** The host part of "host:port" (or "[v6]:port"), lowercased, for matching against central's list. */
export function hostOf(address: string): string {
    const a = address.trim().toLowerCase();
    if (a.startsWith('[')) return a.slice(1, a.indexOf(']') > 0 ? a.indexOf(']') : undefined);
    return a.split(':').length === 2 ? a.split(':')[0] : a;
}

// ------------------------------------------------------------------------------------------------------- the words

const ANY_PORT = 'Is the Editor port right? It is 17789 unless your .env sets RATW_EDITOR_PORT. The Editor may also be turned off ' +
    '([editor] enabled in world.toml).';
/** Docker Desktop on Windows and macOS; on Linux, Docker itself. */
export function onLinux(nav: {platform?: string; userAgent?: string} = typeof navigator === 'undefined' ? {} : navigator): boolean {
    return /Linux/i.test(nav.platform ?? '') && !/Android/i.test(nav.userAgent ?? '');
}
const docker = (linux: boolean) => `Is ${linux ? 'Docker' : 'Docker Desktop'} running, and is the world started (docker compose up -d in its folder)? ` +
    'If it is, docker compose logs world says what\'s wrong (a line starting RATW_PACKAGE error:).';
const OPEN_IT = 'Your world\'s Editor answers only its own computer until you allow others: add RATW_EDITOR_BIND=0.0.0.0 to .env in the ' +
    'world folder, then run docker compose up -d.';
const ROUTER = 'From outside your home, your router must forward TCP 17789 to the computer that runs the world. The hosting guide shows ' +
    'how. From inside your own home, use the home address instead: some routers can\'t reach their own public address.';

/** Screen 3's "can't connect", by where the address is (doc 78, section 2). */
export function cantConnect(place: Place | undefined, gameAnswers = false, linux = onLinux()): string[] {
    const lines = gameAnswers ? ['The world\'s game answers at this address, but its Editor doesn\'t. If your world\'s package is older than the ' +
        'Editor, update it (keep world.toml, .env and backups).'] : [];
    if (place === 'loopback') lines.push(docker(linux), ANY_PORT);
    else if (place === 'home') lines.push(docker(linux), OPEN_IT, ANY_PORT);
    else lines.push(OPEN_IT, ANY_PORT, ROUTER);
    return lines;
}

/** Screen 3's other refusals. */
export function connectRefusal(r: Refusal): string[] {
    switch (r.error) {
        case 'unreachable': return [`Can't reach an Editor at ${r.address ?? 'that address'}.`, ...cantConnect(r.place, r.gameAnswers)];
        case 'address': return ['Type your world\'s address: e.g. 127.0.0.1, 192.168.1.20 or myworld.example.net:17789.'];
        case 'unresolved': return ['That name doesn\'t lead to any computer: check it, or use the world\'s IP address.'];
        case 'game-port': return ['That\'s your world\'s game port. The Editor uses port 17789 (or your .env\'s RATW_EDITOR_PORT).'];
        case 'not-editor': return ['This address isn\'t a RATW world\'s Editor.'];
        case 'package-old': return ['Your world\'s package is older than the Editor: update it (keep world.toml, .env and backups).'];
        case 'client-old': return ['This world\'s Editor is newer than your game: update the game.'];
        case 'official': return ['Official worlds can\'t be edited.'];
        case 'code': return ['That isn\'t a whole Editor code: copy it again, from ratw-editor: to its end.'];
        case 'unavailable': return ['The Editor isn\'t in the game on this computer yet.'];
        default: return ['Something went wrong connecting to the world. Try again.'];
    }
}

const SLOW = 'Your world answered, but its database is slow: try again in a minute.';

/** "still setting up (40%)", or '' once it is ready. */
export function settingUp(hello: EditorHello): string {
    if (hello.progress.code === 'ready') return '';
    if (hello.progress.code === 'slow') return SLOW;
    const what: Record<string, string> = {starting: 'starting', database: 'starting its database', importing: 'importing its world',
        migrating: 'updating its database', building: 'building its map', stopped: 'stopped'};
    return `Your world is still setting up (${what[hello.progress.code] ?? 'starting'}, ${Math.round(hello.progress.percent)}%). This page waits for it.`;
}

/** Screen 4's refusals (and the menu's), from the world's code and numbers. */
export function signInRefusal(r: Refusal): string {
    const minutes = (n: number | undefined) => `${n ?? 1} minute${n === 1 ? '' : 's'}`;
    switch (r.error) {
        case 'wrong': return 'Wrong name or password.';
        case 'wait': return `Too many tries from here: wait ${minutes(r.minutes)}.`;
        case 'disabled': return 'This account is turned off: ask the world\'s owner.';
        case 'expired': return `That one-time password has expired: ask the world's owner for a new code, or on the world's computer ${resetHow()}`;
        case 'approve': return 'This world asks for an Editor code to sign in from a new computer: ask its owner for one (or, on the world\'s ' +
            'computer, run ratw-editor invite).';
        case 'full': return 'Too many people signed in to this world\'s Editor: try again later.';
        case 'busy': return 'The world is busy: try again in a minute.';
        case 'setup': return 'Your world is still setting up: signing in opens when it is done.';
        case 'database': return SLOW;
        case 'official': return 'Official worlds can\'t be edited.';
        case 'code': return 'That Editor code is for another world or account: paste it again.';
        case 'connect-first': return 'Connect to the world first.';
        case 'unreachable': return 'The world didn\'t answer. Is it running?';
        case 'ended': case 'signed-out': return 'Your Editor session ended: sign in again.';
        case 'wrong-password': return 'That isn\'t your password.';
        case 'step-up': return 'Type your password again first.';
        case 'owner-only': return 'Only the world\'s owner manages Editor accounts.';
        case 'owner-account': return `An owner's account is reset only on the world's computer: ${resetHow()}`;
        case 'last-owner': return 'The last owner can\'t be removed or turned off.';
        case 'no-account': return 'There is no such Editor account.';
        case 'exists': return 'There is an Editor account by that name already.';
        case 'username': return 'An account name is 3-32 lowercase letters, digits, _ or -, starting with a letter.';
        case 'role': return 'Choose viewer, dm or builder.';
        case 'unsigned': return 'This world\'s Editor isn\'t an official RATW build.';
        case 'must-change': return 'Choose your own password first.';
        case 'weak': return passwordProblem(r.reason ?? '');
        default: return 'Something went wrong. Try again.';
    }
}

/** Screen 4b: why a new password was refused (the world's reason code). */
export function passwordProblem(reason: string): string {
    switch (reason) {
        case 'short': return 'Your password needs at least 12 characters.';
        case 'long': return 'Your password can be 256 characters at most.';
        case 'username': return 'Your password can\'t contain your account name.';
        case 'world': return 'Your password can\'t contain the world\'s name.';
        case 'common': return 'That is one of the passwords most often found in breaches: choose another.';
        case 'simple': return 'That password is too simple: use more different characters.';
        case 'same': return 'Choose a new password, not the one-time one.';
        default: return 'Choose another password.';
    }
}

/** The command that resets an Editor account, on the world's computer in its folder (doc 78, 3.6). */
export function resetCommand(username = ''): string {
    return `docker compose exec world ratw-editor reset ${username || 'NAME'}`;
}

/** How a lost Editor password is reset, in the host's words: the package's script first (a double-click), the command
 *  as the fallback (doc 78, 3.6; M5). */
export function resetHow(username = ''): string {
    return `double-click reset-editor-password in the world folder (or there type: ${resetCommand(username)})`;
}

/** Checked before sending (the world checks again): length, and the two passwords the same. */
export function newPasswordProblem(fresh: string, again: string, username: string, worldName: string): string {
    if (fresh !== again) return 'The two new passwords aren\'t the same.';
    const squash = (s: string) => s.toLowerCase().replace(/[\s_-]+/g, '');
    if ([...fresh].length < 12) return passwordProblem('short');
    if ([...fresh].length > 256) return passwordProblem('long');
    if (username.length >= 3 && squash(fresh).includes(squash(username))) return passwordProblem('username');
    if (squash(worldName).length >= 3 && squash(fresh).includes(squash(worldName))) return passwordProblem('world');
    // Each word of the world's name of 4 letters or more, too ("ashfall1234567" for "Ashfall Test").
    if (worldName.toLowerCase().split(/[^\p{L}\p{N}]+/u).some(w => [...w].length >= 4 && squash(fresh).includes(w))) return passwordProblem('world');
    return '';
}

/** "5 s", "2 min", "1 h". */
export function inTime(seconds: number): string {
    if (seconds < 90) return `${Math.max(1, Math.round(seconds))} s`;
    if (seconds < 90 * 60) return `${Math.round(seconds / 60)} min`;
    return `${Math.round(seconds / 3600)} h`;
}

/** The game server's state in words, when it isn't up (null when it is, or the world said nothing). */
export function serverDown(server: ServerUp | null | undefined): string | null {
    if (!server || server.up) return null;
    const again = server.retryIn ? `starts again in ${inTime(server.retryIn)}` : 'starting again';
    switch (server.reason) {
        case 'starting': return 'Starting…';
        case 'restarting': return 'Restarting…';
        case 'closed': return 'Closed for changes';
        case 'world.toml': return 'Stopped: world.toml has a mistake (see the log)';
        case 'exit': {
            const exit = server.code === null ? '' : ` (exit ${server.code})`;
            return `${server.failures >= 2 ? 'Keeps stopping' : 'Stopped'}${exit}: ${again}`;
        }
        default: return 'Stopped';
    }
}

/** Screen 5's status pill, from the world's hello (or null when it didn't answer). */
export function statusPill(hello: EditorHello | null): string {
    if (!hello) return 'Not answering';
    if (hello.progress.code === 'slow') return 'Database slow';
    if (hello.progress.code !== 'ready') return `Setting up ${Math.round(hello.progress.percent)}%`;
    const down = serverDown(hello.server);
    if (down) return down;
    const state = ({running: 'Open', maintenance: 'Closed for changes', host_only: 'Open only to you'} as Record<string, string>)[hello.serverState]
        ?? (hello.server?.up ? 'Open' : 'Starting…');
    // "Open · 7 wolves" (section 2's pill): the count only while the world is open and its server says one.
    const wolves = typeof hello.wolves === 'number' && state.startsWith('Open') ? hello.wolves : null;
    return wolves === null ? state : `${state} · ${wolves} ${wolves === 1 ? 'wolf' : 'wolves'}`;
}

/** Screen 5's "Last backup 3 h ago" (section 2), from the hello's age in seconds; '' when the world says nothing (a
 *  package without the backup service). */
export function lastBackupText(hello: EditorHello | null): string {
    if (!hello || !('lastBackup' in hello) || hello.lastBackup === undefined) return '';
    if (hello.lastBackup === null) return 'No backup yet';
    const n = hello.lastBackup;
    return n < 90 ? 'Last backup just now' : n >= 36 * 3600 ? `Last backup ${Math.round(n / 86400)} days ago` : `Last backup ${inTime(n)} ago`;
}

/** Screen 4: whether SIGN IN uses the one-time password the pasted Editor code carries. A session that has ended (or any
 *  but a live one) doesn't stand in its way: a co-builder's one-time password exists only in the code (R1 H1). */
export function signsInWithCode(viaCode: boolean, codePassword: boolean, session: {live: boolean} | null): boolean {
    return viaCode && codePassword && !session?.live;
}

/** Screen 4's "First time?" note: only while the owner hasn't chosen a password, and never beside an Editor code's note
 *  or to someone this computer already knows by name. */
export function firstTimeNote(setupPending: boolean, fromCode: boolean, knownUsername: string): boolean {
    return setupPending && !fromCode && !knownUsername;
}

/** An address the world recorded, in the owner's words: 'local' (this computer, through Docker) is "this computer". */
export function fromWhere(address: string | null | undefined): string {
    return !address ? '' : address === 'local' ? 'this computer' : address;
}

/** The port of "host:port" (or the Editor's own when none is given). */
export function portOf(address: string): number {
    const m = /:(\d{1,5})$/.exec(address.trim());
    return m && !address.trim().endsWith(']') && (address.split(':').length === 2 || address.includes(']:')) ? Number(m[1]) : EDITOR_PORT;
}

/** Under a co-builder's Editor code: what to tell them besides. A code made on this computer carries no address. */
export function coBuilderNote(place: Place, address: string): string {
    return place === 'loopback' ? `It doesn't include your world's address: tell them that too (your home or public address, port ${portOf(address)}).`
        : `It includes this world's address as you use it (${address}).`;
}

export const ROLE_LINES: Record<string, string> = {
    viewer: 'Can watch the DM tool; can\'t change anything.',
    dm: 'Can run events and NPCs; can\'t touch the map or accounts.',
    builder: 'Can run events and NPCs, and edit and publish the map; can\'t touch accounts or My world.',
    owner: 'Everything, including Editor accounts and My world.',
};

/** An account change, worded by the client (never the world's own sentence). */
export function bannerText(b: AuditBanner): string {
    const who = String(b.who ?? '').split('#')[0] || 'someone';
    const target = String(b.target ?? '');
    const what: Record<string, string> = {
        'account.add': `Account ${target} added by ${who}`,
        'account.reset': `Account ${target} given a new Editor code by ${who}`,
        'account.role': `Account ${target}'s role changed by ${who}`,
        'account.wolves': `Account ${target}'s "may act on players' wolves" changed by ${who}`,
        'account.disable': `Account ${target} turned off by ${who}`,
        'account.enable': `Account ${target} turned on by ${who}`,
        'account.remove': `Account ${target} removed by ${who}`,
        'account.end-sessions': `Every Editor session ended by ${who}`,
        'account.password': `${target} chose a new password`,
    };
    return what[b.action] ?? `An account change by ${who}`;
}

/** "3 h ago", "2 days ago", from Unix seconds. */
export function ago(at: number | null, now = Date.now() / 1000): string {
    if (!at) return 'never';
    const s = Math.max(0, now - at);
    if (s < 90) return 'just now';
    if (s < 3600) return `${Math.round(s / 60)} min ago`;
    if (s < 48 * 3600) return `${Math.round(s / 3600)} h ago`;
    return `${Math.round(s / 86400)} days ago`;
}
