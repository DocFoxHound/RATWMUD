// The Server Health charts' value axis (Docs/Design/31, the health tracker): whole-number ticks a person reads at a
// glance (0, 1, 2; 0, 20, 40, 60), never the same label twice, and never a range so small that a count of 0 or 1 looks
// like a cliff.

/** Ticks from 0 to at least max(`max`, `least`), in steps of 1, 2 or 5 times a power of ten, at most `most` steps. */
export function niceTicks(max: number, least = 1, most = 4): number[] {
    const want = Math.max(Number.isFinite(max) ? max : 0, least, 1);
    let step = 1;
    for (let power = 1; ; power *= 10) {
        const found = [1, 2, 5].map(f => f * power).find(s => Math.ceil(want / s) <= most);
        if (found !== undefined) { step = found; break; }
    }
    const top = Math.ceil(want / step) * step;
    return Array.from({length: top / step + 1}, (_, i) => i * step);
}
