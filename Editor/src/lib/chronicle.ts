// Chronicles in the Dungeon Master (Docs/Design/26-living-npcs.md, Phase 8): what tools/chronicle.py sends, and how the
// Life panel lays it out. Kept free of React so it can be tested with node.
export interface ChronicleEntry {
    id: number | null; day: number; date: string; kind: string; importance: number; text: string;
    people: string[]; cell: string; place: string;
}
export interface SeasonRound { day: number; label: string; text: string }
export interface Story { text: string; throughEvent: number; entries: number; model: string; by: string; at: string; newer: number }
export interface Bond { who: string; affinity: number; trust: number; familiarity: number; fear: number; respect: number; owed: number; lastContact: number }
export interface Belief { holder: string; subject: string; claim: string; source: string; confidence: number; day: number; incident: string }
export interface Memory { npc: string; subject: string; text: string; started: number | null; consolidated: number | null }
export interface Conversation { npc: string; subject: string; started: number | null; lastActivity: number | null; turns: {who: string; text: string}[] }
export interface Mind {
    memories: Memory[]; conversations: Conversation[]; bonds: Bond[]; regard: Bond[]; heard: Belief[]; said: Belief[];
    names: Record<string, string>;
}
export interface Chronicle {
    subject: string; name: string; entries: ChronicleEntry[]; seasons: SeasonRound[];
    firstDay: number | null; lastDay: number | null; firstDate: string; lastDate: string; events: number; lastEvent: number;
    story: Story | null; mind: Mind;
}

/** A line of the timeline: a life event, or a season's round. */
export type Line = {kind: 'event'; entry: ChronicleEntry} | {kind: 'round'; round: SeasonRound};

/** The life events at or above `least` importance (3: life, 2: notable, 1: first meetings), with each season's round
 *  after that season's events when `rounds` is on; oldest first. */
export function timeline(c: Pick<Chronicle, 'entries' | 'seasons'>, least: number, rounds: boolean): Line[] {
    const lines: {day: number; order: number; line: Line}[] = c.entries
        .filter(e => e.importance >= least)
        .map((entry, i) => ({day: entry.day, order: i, line: {kind: 'event', entry}}));
    if (rounds) {
        // A season's round sits at the end of its season: after everything that happened in it.
        const ends = seasonEnds(c.seasons);
        c.seasons.forEach((round, i) => lines.push({day: ends[i], order: 1e9 + i, line: {kind: 'round', round}}));
    }
    return lines.sort((a, b) => a.day - b.day || a.order - b.order).map(l => l.line);
}

const SEASON_STARTS = [0, 92, 184, 275];
function seasonEnds(seasons: SeasonRound[]): number[] {
    return seasons.map(s => {
        const year = Math.floor(s.day / 365), inYear = s.day - year * 365;
        const next = SEASON_STARTS.find(start => start > inYear);
        return year * 365 + (next ?? 365) - 1e-6;
    });
}

/** How one regards another, in a few words, from a bond's numbers (as Bonds::describe words it in the game). */
export function regardWords(b: Bond): string {
    const words: string[] = [];
    words.push(b.familiarity >= 60 ? 'knows well' : b.familiarity >= 25 ? 'knows' : 'barely knows');
    if (b.affinity >= 40) words.push('fond'); else if (b.affinity >= 10) words.push('likes');
    else if (b.affinity <= -40) words.push('hates'); else if (b.affinity <= -10) words.push('dislikes');
    if (b.trust >= 30) words.push('trusts'); else if (b.trust <= -20) words.push('distrusts');
    if (b.fear >= 30) words.push('afraid');
    if (b.respect >= 30) words.push('respects'); else if (b.respect <= -30) words.push('scorns');
    if (b.owed > 0) words.push(`is owed ${b.owed}p`); else if (b.owed < 0) words.push(`owes ${-b.owed}p`);
    return words.join(' · ');
}

export const nameOf = (mind: Mind, id: string) => mind.names[id] ?? id;
