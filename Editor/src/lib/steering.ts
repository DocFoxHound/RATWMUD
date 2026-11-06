// Steering the economy (Docs/Design/74-steering-the-economy.md, 10): what the DM's Steering panel shows and sends, as
// pure functions (steering.test.ts). The slider "Where the money rests" runs -100 (shared) to +100 (held by the rich) and
// is stored as the rule steerLean, 0 to 200; the table (Data/Items/city_loop.json `steering`) is read as Core reads it.

/** One judging or change of the steering, as the world's save has it (newest first). */
export interface SteerJudging {
    day: number; measure: string; value: number; target: number; band: number; error: number; step: number;
    before: number; after: number; why: string;
}
export interface SteerLever { key: string; label: string; unit: string; own: number; centre: number | null; low: number | null;
    high: number | null; now?: number | null }
export interface SteerSettings { steer: boolean; lean: number; neverHungry: boolean; neverBroke: boolean }
export interface SteerTable {
    levers: Record<string, (number | null)[]>; targets: Record<string, (number | null)[]>; marks: number[];
    treasuryFloor: number; treasuryLow: number;
}
/** The studio's steering block (studio.py Economy.steering). */
export interface SteeringView {
    settings: SteerSettings; words: {left: string; centre: string; right: string}; held: string[];
    own: Record<string, number | undefined>; table: SteerTable; band: [number, number];
    targets: Record<string, number | null>; levers: SteerLever[]; saved: string | null; problem: string;
    changes: {at: number | string; who: string; detail: string}[];
    now?: {on: boolean; push: number; judged: number | null; days: number | null; tollLean: number;
        means: Record<string, number | null>};
    history?: SteerJudging[];
    skim?: {total: number; short: number; days: number; purpose: Record<string, number>; group: Record<string, number>;
        payers: Record<string, number>};
}
export type SteerBody = Partial<SteerSettings>;

/** The slider's -100..+100 and the rule's 0..200. */
export const leanToRule = (lean: number) => lean + 100;
export const ruleToLean = (rule: number) => rule - 100;
/** A slider position as the DM may set it: a whole multiple of 5 within -100..100. */
export const snapLean = (value: number) => Math.max(-100, Math.min(100, Math.round(value / 5) * 5));

/** Core's items::steerAt: a row at `at` (-1..1), straight lines between the five marks; a lever's null is `own`, a
 *  target's (own null) none, as is a place between a mark with none and one. */
export function steerAt(marks: (number | null)[], at: number, own: number | null = null): number | null {
    const x = Math.max(-1, Math.min(1, Number.isFinite(at) ? at : 0));
    const value = (i: number) => marks[i] === null || marks[i] === undefined ? own : marks[i] as number;
    const place = (x + 1) * 2;
    const i = Math.min(3, Math.floor(place));
    const t = place - i;
    if (t <= 1e-12) return value(i);
    if (t >= 1 - 1e-12) return value(i + 1);
    const a = value(i), b = value(i + 1);
    return a === null || b === null ? null : a + (b - a) * t;
}

/** The push's band at a slider position (lean -100..100): steerWidthMid at the centre to steerWidthEnd at the ends. */
export function bandAt(own: Record<string, number | undefined>, lean: number): [number, number] {
    const x = lean / 100, mid = (own.steerWidthMid ?? 15) / 100, end = (own.steerWidthEnd ?? 30) / 100;
    const w = mid + (end - mid) * Math.abs(x);
    return [Math.max(-1, x - w), Math.min(1, x + w)];
}

/** The slider's words at a position: the end's or the centre's, or "leaning" between. */
export function leanWords(v: SteeringView, lean: number): string {
    if (lean <= -100) return v.words.left;
    if (lean >= 100) return v.words.right;
    if (lean === 0) return v.words.centre;
    return lean < 0 ? `Leaning shared (${lean})` : `Leaning to the rich (+${lean})`;
}

/** What a position would mean, before it is saved (the DM dragging the slider): each measure's target and band there, and
 *  each lever's bounds there (its table's value at the band's two ends). */
export function preview(v: SteeringView, lean: number) {
    const x = lean / 100, [lo, hi] = bandAt(v.own, lean);
    const t = (name: string) => v.table.targets[name] ? steerAt(v.table.targets[name], x) : null;
    const levers = v.levers.map(l => {
        const row = v.table.levers[l.key];
        const a = row ? steerAt(row, lo, l.own) : null, b = row ? steerAt(row, hi, l.own) : null;
        return {...l, low: a === null || b === null ? a : Math.min(a, b), high: a === null || b === null ? b : Math.max(a, b)};
    });
    return {band: [lo, hi] as [number, number], gini: t('gini'), giniBand: t('giniBand'), upper: t('upper'), upperBand: t('upperBand'),
        hungerMost: t('hungerMost'), shortMost: t('shortMost'), top10Most: t('top10Most'), top10Least: t('top10Least'), levers};
}

const fixed = (v: number | null | undefined, places: number) => v === null || v === undefined ? '-' : v.toFixed(places);
/** The measures against their targets, a line each (the means from the save; the targets at the slider). */
export function measureLines(v: SteeringView, lean = v.settings.lean): {name: string; now: string; target: string; inside: boolean | null}[] {
    const p = preview(v, lean), m = v.now?.means ?? {};
    const banded = (now: number | null | undefined, target: number | null, band: number | null) =>
        now === null || now === undefined || target === null || band === null ? null : Math.abs(now - target) <= band;
    const ceiling = (now: number | null | undefined, most: number | null) => now === null || now === undefined || most === null ? null : now <= most;
    return [
        {name: 'The earners\' Gini (56-day mean)', now: fixed(m.gini, 3),
            target: p.gini === null ? 'watched' : `${fixed(p.gini, 3)} ± ${fixed(p.giniBand, 3)}`, inside: banded(m.gini, p.gini, p.giniBand)},
        {name: 'The upper share: the upper class and the Houses (% of all the money)', now: fixed(m.upper, 1),
            target: p.upper === null ? 'watched' : `${fixed(p.upper, 1)} ± ${fixed(p.upperBand, 1)}`, inside: banded(m.upper, p.upper, p.upperBand)},
        {name: 'The richest tenth\'s share of the households\' purses (%)', now: fixed(m.top10, 1),
            target: p.top10Most !== null ? `at most ${p.top10Most}` : p.top10Least !== null ? `at least ${p.top10Least}` : 'logged',
            inside: m.top10 === null || m.top10 === undefined ? null : (p.top10Most === null || m.top10 <= p.top10Most) && (p.top10Least === null || m.top10 >= p.top10Least)},
        {name: 'Hungry wolves an hour (28-day mean)', now: fixed(m.hungry, 3), target: `at most ${fixed(p.hungerMost, 2)}`, inside: ceiling(m.hungry, p.hungerMost)},
        {name: 'Households short of food a day (28-day mean)', now: fixed(m.short, 2), target: `at most ${fixed(p.shortMost, 1)}`, inside: ceiling(m.short, p.shortMost)},
        {name: 'The treasury at 18:00 (28-day mean)', now: fixed(m.treasury, 0), target: `the toll lean held at 0 under ${v.table.treasuryFloor}p`,
            inside: null},
    ];
}

/** The confirmation before a change is saved. */
export function confirmText(v: SteeringView, body: SteerBody): string {
    if (body.lean !== undefined)
        return `Move where the money rests from ${v.settings.lean} to ${body.lean}? `
            + (v.settings.steer || body.steer ? 'The steering moves its levers toward it over the coming weeks.'
                : 'The steering is off: it takes effect when the steering is turned on.');
    if (body.steer !== undefined)
        return body.steer ? `Turn the steering on, where the money rests at ${v.settings.lean}? Its levers move at once to that place.`
            : 'Turn the steering off? Every rule it moves goes back to the world\'s own at once.';
    if (body.neverHungry !== undefined)
        return body.neverHungry ? 'Turn Never Hungry on? A wolf going hungry is given a day\'s food, bought at the stable price; the richest pay for it.'
            : 'Turn Never Hungry off?';
    if (body.neverBroke !== undefined)
        return body.neverBroke ? 'Turn Never Broke on? A household short of the next day\'s necessities is given just enough; the richest pay for it.'
            : 'Turn Never Broke off?';
    return 'Change the steering?';
}

/** The body to send, or null when it changes nothing (the DM may send only the four settings). */
export function steerChange(v: SteeringView, body: SteerBody): SteerBody | null {
    const out: SteerBody = {};
    if (body.lean !== undefined && snapLean(body.lean) !== v.settings.lean) out.lean = snapLean(body.lean);
    for (const k of ['steer', 'neverHungry', 'neverBroke'] as const)
        if (body[k] !== undefined && body[k] !== v.settings[k]) out[k] = body[k];
    return Object.keys(out).length ? out : null;
}

/** A rule the steering moves, while it is on: read only, with the world's own beside it. */
export function heldText(v: SteeringView | undefined, key: string, yours: number): string | null {
    if (!v || !v.settings.steer || !v.held.includes(key)) return null;
    const lever = v.levers.find(l => l.key === key);
    const now = lever?.now ?? lever?.centre;
    return `steered: ${now === null || now === undefined ? '-' : Math.round(now)}; yours: ${yours}, used again when the steering is off`;
}

/** One log of the DM's changes and the steering's judgings, newest first. */
export function logLines(v: SteeringView): string[] {
    const mine = v.changes.map(c => `${c.who}: ${c.detail}`);
    const judged = (v.history ?? []).map(h => h.why);
    return [...mine.map(l => `The DM, ${l}`), ...judged];
}
