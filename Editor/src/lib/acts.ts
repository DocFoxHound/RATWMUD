/** A DM's act waited for (doc 79, Phase 9's second repair): what the game server made of a queued dm.actions row, asked
 *  `tries` times a `wait` apart. The DM is told only what the server said: applied, refused (with its reason), expired,
 *  or still queued ("no game server has picked it up yet"), never success when the row is merely queued. */
export interface ActOutcome { status: 'applied' | 'refused' | 'expired' | 'queued'; result: string }   // (dm.actions' four.)

export async function awaitOutcome(ask: () => Promise<ActOutcome>, tries = 10, wait = 1000,
                                   sleep: (ms: number) => Promise<void> = ms => new Promise(r => setTimeout(r, ms))): Promise<ActOutcome> {
    let last: ActOutcome = {status: 'queued', result: ''};
    for (let i = 0; i < tries; i++) {
        await sleep(wait);
        last = await ask();
        if (last.status !== 'queued') return last;
    }
    return last;
}

/** The words for an outcome: `done` when applied, else what the server said (a refusal throws, for a form's problem line). */
export function outcomeWords(what: string, outcome: ActOutcome, done: string): string {
    if (outcome.status === 'applied') return done;
    if (outcome.status === 'queued') return `${what}: still queued; no game server has picked it up yet.`;
    throw new Error(`${what}: ${outcome.status === 'refused' ? 'refused' : 'never applied'}` +
                    (outcome.result ? `. ${outcome.result}` : '.'));
}

/** What the game server made of economy.sync #`id` (Phase 9's last repair: no screen showed it): the view read again,
 *  `tries` times a `wait` apart, until its `lastSync` is that sync (or a later one) and no longer queued. The last one
 *  read (still queued when no server is running: the page then says so), or null if none could be read. */
export async function awaitSync<S extends {id: number; status: string}>(
    id: number, read: () => Promise<{lastSync?: S | null}>, tries = 8, wait = 1000,
    sleep: (ms: number) => Promise<void> = ms => new Promise(r => setTimeout(r, ms))): Promise<S | null> {
    let last: S | null = null;
    for (let i = 0; i < tries; i++) {
        await sleep(wait);
        try { last = (await read()).lastSync ?? last; } catch { continue; }
        if (last && last.id >= id && last.status !== 'queued') return last;
    }
    return last;
}
