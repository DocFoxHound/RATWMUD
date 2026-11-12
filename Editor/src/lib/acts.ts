/** A DM's act waited for (doc 79, Phase 9's second repair): what the game server made of a queued dm.actions row, asked
 *  `tries` times a `wait` apart. The DM is told only what the server said: applied, refused (with its reason), expired,
 *  or still queued ("no game server has picked it up yet"), never success when the row is merely queued. */
export interface ActOutcome { status: 'applied' | 'refused' | 'expired' | 'queued' | 'failed'; result: string }

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
    throw new Error(`${what}: ${outcome.status === 'refused' ? 'refused' : outcome.status === 'expired' ? 'never applied' : 'failed'}` +
                    (outcome.result ? `. ${outcome.result}` : '.'));
}
