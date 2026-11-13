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

// ---------------------------------------------------------------------------------------------- confirmations
// Docs/Design/78, 12.3 (Phase M5): each confirmation names who it acts on (a player's wolf by its name, said to be one)
// and says whether the act has an opposite, so a host knows before the click whether a mistake can be put right.

/** The opposite of each DM act, where one exists (and where on the page it is); null where none does. */
export const OPPOSITES: Record<string, string | null> = {
    'character.kill': 'Resurrect, on the same panel (the time they lose doesn\'t come back)',
    'character.resurrect': 'Kill',
    'character.move': null,
    'injury.add': 'Take away, beside the injury',
    'injury.remove': 'Give injury',
    'gift.give': 'Take Gift away',
    'gift.take': 'Make Gifted or Make Quickened',
    'unlock.grant.gifted': null,
    'unlock.grant.quickened': 'Revoke Quickened',
    'unlock.revoke': 'Grant Quickened',
    'unlock.hold': 'Release unlocks',
    'unlock.release': 'Hold unlocks',
    'mentor.revoke': 'Restore mentoring',
    'mentor.restore': 'Revoke mentoring',
    'tie.end': null,
    'bandits.call': null,
    'npc.kill': 'Revive',
    'npc.revive': 'Kill',
    'npc.leave': null,
    'band.override': 'Clear the band, on the same panel (or let it run out)',
    'visitor.bring': 'Send away',
    'project.remove': null,
    'project.cancel': null,
    'project.complete': null,
    'grant': 'a fine of the same coins',
    'fine': 'a grant of the same coins',
    'prices.reset': null,
    'festival.call': null,
    'report.decide': null,
    'chapter.decide': null,
};

/** A confirmation's words: what, on which world; a player's wolf named as one (and, in a host's Editor, that its player
 *  is told in the game); then the act's opposite, or that it has none. */
export function sureText(what: string, world: string, act: string, wolf?: string | null, told = false): string {
    const opposite = OPPOSITES[act];
    const lines = [`${what} on ${world}?`];
    if (wolf) lines.push(`${wolf} is a player's wolf${told ? ': its player is told in the game' : ''}.`);
    lines.push(opposite === undefined ? '' : opposite ? `Its opposite: ${opposite}.`
        : 'It has no opposite: it can\'t be undone from here.');
    return lines.filter(Boolean).join('\n\n');
}

/** A button's reason for being greyed: the first of `why` whose condition holds, or '' (enabled). For `disabled` and
 *  `title` both, so a greyed button always says why (doc 78, M5). */
export function greyed(...why: [boolean, string][]): string {
    return why.find(([on]) => on)?.[1] ?? '';
}
