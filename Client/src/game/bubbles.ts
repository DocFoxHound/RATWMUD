// Speech bubbles over wolves on the map (the user, 2026-11-14: "When they press enter I want to show at least some of
// what they say on-screen, maybe up to a certain character limit that trails off with a '...' if its too much longer").
// Pure helpers: what a bubble says, for how long, and where bubbles go so that several speakers don't cover each other.

/** The most a bubble says before it trails off. */
export const BubbleChars = 80;

/** A said line as its bubble shows it: one line of words, at most `limit` characters, trailing off with "…" (at a
 *  word's end where one is near). */
export function bubbleText(text: string, limit = BubbleChars): string {
    const plain = text.replace(/\s+/g, ' ').trim();
    if (plain.length <= limit) return plain;
    const cut = plain.slice(0, limit - 1);
    const space = cut.lastIndexOf(' ');
    return (space >= limit * 0.6 ? cut.slice(0, space) : cut).replace(/[\s,;:.!?-]+$/, '') + '…';
}

/** How long a bubble stays, in seconds: longer for longer lines (it is read), from 3.5 to 9. */
export function bubbleSeconds(text: string): number {
    return Math.min(9, 3.5 + text.length / 18);
}

/** What a heard line's words are: its spoken parts, else the whole of it (an action alone has no bubble words). */
export function spokenWords(segments: {kind: string; text: string}[], text: string): string {
    if (!segments.length) return text;
    return segments.filter(s => s.kind === 'speech' && s.text.trim()).map(s => s.text.trim()).join(' … ');
}

export interface BubbleBox {
    x: number;                  // Its left and top, and size, in the map's pixels.
    y: number;
    w: number;
    h: number;
}

/**
 * Places bubbles over their wolves without covering each other: each wanted box (over its wolf, in the order given,
 * newest last) moves up past any box already placed that it would overlap. Returns the boxes as placed, in order.
 */
export function stackBubbles(wanted: BubbleBox[], gap = 3): BubbleBox[] {
    const placed: BubbleBox[] = [];
    for (const want of wanted) {
        const box = {...want};
        for (let moved = true, guard = 0; moved && guard < 32; ++guard) {
            moved = false;
            for (const other of placed)
                if (box.x < other.x + other.w && box.x + box.w > other.x && box.y < other.y + other.h && box.y + box.h > other.y) {
                    box.y = other.y - box.h - gap;
                    moved = true;
                }
        }
        placed.push(box);
    }
    return placed;
}

/** The three typing dots' lift (pixels up) at `t` seconds: a wave running left to right, each dot in turn. */
export function dotLift(t: number, dot: number, height = 3): number {
    const phase = (t * 1.6 - dot * 0.18) % 1;
    return phase < 0.35 ? Math.sin((phase / 0.35) * Math.PI) * height : 0;
}
