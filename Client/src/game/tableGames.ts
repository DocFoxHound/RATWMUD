// Tavern games (Docs/Design/54-gathering-places.md, 5): what the table panel draws, as pure functions of what the
// server sent. The server keeps the rules; these only lay them out.

export const RoundNames = ['ones', 'twos', 'threes', 'fours', 'fives'];
const Dice = ['⚀', '⚁', '⚂', '⚃', '⚄', '⚅'];

/** A bone's face as a die (1..6); '?' for anything else. */
export function dieFace(n: number): string {
    return n >= 1 && n <= 6 ? Dice[Math.trunc(n) - 1] : '?';
}

/** Knucklebones: how far a seat has come ("through the twos · this turn the threes"). */
export function knucklebonesLine(banked: number, at: number): string {
    const through = banked > 0 ? `through the ${RoundNames[Math.min(5, banked) - 1]}` : 'not begun';
    return at > banked ? `${through} · this turn the ${RoundNames[Math.min(5, at) - 1]}` : through;
}

/** The round a seat tries next ("the threes"), or '' when it is through. */
export function nextRound(at: number): string {
    return at < 5 ? `the ${RoundNames[Math.max(0, at)]}` : '';
}

/** Wolves and Deer: the 49 points as 7 rows ('W', 'D', '.' or ' ' off the board). */
export function boardRows(points: string): string[][] {
    const rows: string[][] = [];
    for (let y = 0; y < 7; ++y) rows.push([...points.slice(y * 7, y * 7 + 7).padEnd(7, ' ')]);
    return rows;
}

/** Where a piece at `from` may go, from the server's list of [from, to] moves. */
export function targetsFrom(moves: readonly (readonly number[])[], from: number): number[] {
    return moves.filter(m => m[0] === from).map(m => m[1]);
}

/** Liar's Bones: the smallest bid above the standing one, on `face` if it can be (count 0: none yet). */
export function nextBid(count: number, face: number, wantFace: number, total: number): [number, number] | null {
    const f = Math.min(6, Math.max(1, Math.trunc(wantFace)));
    const c = count === 0 ? 1 : f > face ? count : count + 1;
    return c <= total ? [c, f] : null;
}
