// Hosted mode (Docs/Design/78, 6.6): the studio served to the desktop client's Editor window, for a host's own world,
// through the shell's `editor` scheme (editor://localhost, or http://editor.localhost on Windows). There:
// - the page never signs in and holds no token: the host signed in on the client's own Editor menu, and the shell adds
//   the session to every request (so no SignIn form, and authHeaders() adds nothing);
// - its world comes from /api/studio/session, never from Atlas's session token (Atlas's writes carry no X-RATW-Editor);
// - a 401 means the Editor session ended: the page says so and keeps what it holds, and signing in again as the same
//   account from the menu lets it carry on;
// - reloads and suite switches happen inside the page (a reload would drop what it holds);
// - the game-opening links are hidden (the Editor window opens no other window).

export const hosted = typeof location !== 'undefined' && (location.protocol === 'editor:' || location.hostname === 'editor.localhost');

let ended = false;
const watchers = new Set<(ended: boolean) => void>();

/** Whether the world said the Editor session is over (the last call was a 401). */
export const sessionEnded = () => ended;

export function setEnded(value: boolean) {
    if (!hosted || value === ended) return;
    ended = value;
    for (const w of watchers) w(ended);
}

export function watchSession(watcher: (ended: boolean) => void): () => void {
    watchers.add(watcher);
    return () => { watchers.delete(watcher); };
}

export const ENDED_TEXT = 'Your Editor session ended: sign in again from the Editor menu. What this page holds is kept.';

const reloads = new Set<() => void>();
/** The page's root registers how to start again inside the page (main.tsx). */
export function onReload(handler: () => void): () => void {
    reloads.add(handler);
    return () => { reloads.delete(handler); };
}

/** A reload: inside the page when hosted, the browser's own otherwise. */
export function reloadApp() {
    if (hosted && reloads.size) for (const r of reloads) r();
    else window.location.reload();
}

/** Opens `?query` (a suite, an Atlas page): inside the page when hosted, as a page load otherwise. */
export function goTo(query: string) {
    if (hosted && reloads.size) {
        window.history.pushState(null, '', `?${query}`);
        reloadApp();
    } else window.location.assign(`?${query}`);
}

// ---------------------------------------------------------------------------------------------------- restarting
// The shell's `editor` scheme answers 503 {"error": "restarting"} when the world doesn't answer (its container
// restarting: doc 78, 2, screen 6). The page keeps what it holds, says so once, and goes on asking; the first answer
// clears it.

let restarting = false;
const restartWatchers = new Set<(restarting: boolean) => void>();

/** LIVE's map on a world whose package holds no map source (slice 1: only its build, which the page can't draw; the
 *  source comes with map editing, doc 78's M7). M6's run found the map simply black. */
export const NO_MAP_YET = 'This world\'s map picture comes with map editing, in a later update: the world itself is all there. ' +
    'Everything else here works now: Find someone (on the left), the panel on the right, and the NPC Life, Players, Prosperity ' +
    'and Economy tabs.';
export const noMapYet = (world: {cells: unknown[]} | null | undefined) => !!world && world.cells.length === 0;

export const RESTARTING_TEXT = 'Your world is restarting: the Editor reconnects by itself.';
export const RESTARTING_ACT = 'Your world is restarting: try again in a moment.';

/** Whether the world is restarting (the last call found nobody there). */
export const worldRestarting = () => restarting;

export function setRestarting(value: boolean) {
    if (!hosted || value === restarting) return;
    restarting = value;
    for (const w of restartWatchers) w(restarting);
}

export function watchRestarting(watcher: (restarting: boolean) => void): () => void {
    restartWatchers.add(watcher);
    return () => { restartWatchers.delete(watcher); };
}

/** Whether a reply (its status and body) says the world isn't there: the shell's 503 "restarting", or a 502 from an
 *  older shell ("The world didn't answer."). */
export const restartingReply = (status: number, data: {error?: unknown}) =>
    (status === 503 && data.error === 'restarting') || (status === 502 && /didn't answer/i.test(String(data.error ?? '')));

// ---------------------------------------------------------------------------------------------------- after the end
// Once the session has ended, a poll every couple of seconds would only be refused again (and logged by the shell each
// time, all night): the page asks at most once a half minute until it hears the session is live again.

export const ENDED_PROBE_MS = 30_000;
let lastProbe = 0;

/** Whether a call may go out now (always, unless the session ended; then one every ENDED_PROBE_MS). */
export function mayCall(now = Date.now(), isEnded = ended): boolean {
    if (!isEnded) return true;
    if (now - lastProbe < ENDED_PROBE_MS) return false;
    lastProbe = now;
    return true;
}

// ---------------------------------------------------------------------------------------------------- the server
// The package's own word on its game server (Package/entrypoint.py's server.json, through the hosted status): whether
// it is up, and if not why, with the exit code, how many stops in a row and when it starts again.

export type HostServerReason = 'starting' | 'running' | 'restarting' | 'exit' | 'world.toml' | 'closed';
export interface HostServer { up: boolean; reason: HostServerReason; code: number | null; failures: number; retryIn: number | null }

/** "5 s", "2 min", "1 h". */
export function inTime(seconds: number): string {
    const s = Math.max(0, Math.round(seconds));
    if (s < 60) return `${s} s`;
    if (s < 3600) return `${Math.round(s / 60)} min`;
    return `${Math.round(s / 3600)} h`;
}

/** The Server tab's line when the package says its game server isn't up (never "Running"), or '' when it is (or when
 *  the package says nothing: an older one). */
export function hostServerDown(h: HostServer | null | undefined): string {
    if (!h) return '';
    if (h.reason === 'restarting') return 'Restarting…';
    if (h.up) return '';
    switch (h.reason) {
        case 'exit': {
            const what = `${h.failures >= 2 ? 'Keeps stopping' : 'Stopped'}${h.code !== null ? ` (exit ${h.code})` : ''}`;
            return `${what}: ${h.retryIn !== null ? `starts again in ${inTime(h.retryIn)}` : 'starting again'}`;
        }
        case 'world.toml': return 'Stopped: world.toml has a mistake (see the log: docker compose logs world)';
        case 'starting': return 'Starting…';
        case 'closed': return CLOSED_TEXT;
        default: return 'Starting…';
    }
}

export const RUNNING_TEXT = 'Running: players can join.';
export const CLOSED_TEXT = 'Closed: players can\'t join until you open it again.';

// ---------------------------------------------------------------------------------------------------- words
/** The world's name for a heading: hosted, its own display name (`name`, as the session gives it); in the dev studio,
 *  its key in capitals, as before. */
export const headingFor = (key: string, name: string | undefined) => (hosted ? name || key : key.toUpperCase());

/** Who did something, from an audit actor: an Editor account's session ("pell#3f2a1c") is its account's name; anything
 *  else (the DM tool's own accounts, the game) is "the DM" in a host's Editor. */
export function actorName(by: string | null | undefined, isHosted = hosted): string {
    const who = String(by ?? '');
    if (!isHosted) return who;
    if (/^[a-z][a-z0-9_-]{2,31}#[0-9a-f]{4,}$/.test(who)) return who.split('#')[0];
    return who ? 'the DM' : '';
}

/** The economy's shipped files, as a page names them: in a host's Editor "the default" (a host has no Data/ folder to
 *  open); in the dev studio, the file. */
export const shipped = (file: 'market' | 'status', isHosted = hosted) => (isHosted ? 'the default' : `${file}.json`);
export const shippedOwn = (file: 'market' | 'status', isHosted = hosted) => (isHosted ? 'the default\'s' : `${file}.json's`);
export const asShipped = (file: 'market' | 'status', isHosted = hosted) => (isHosted ? 'Back to the default' : `As ${file}.json`);
/** A world before migration 0061: the dev studio's command; a host's package update. */
export const needsMigration = (what: string, isHosted = hosted) => (isHosted
    ? `This world needs a newer package for ${what}: update it (keep world.toml, .env and backups).`
    : `This world needs migration 0061 for ${what} (python3 tools/world_db.py migrate).`);
