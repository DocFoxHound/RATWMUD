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
