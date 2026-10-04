// Fights (Docs/Design/33-combat.md) as the page receives them: the arena a fighter or watcher is in, the red squares
// an onlooker sees, a challenge waiting for an answer. The rules are the server's (Core/RatwBattle.cpp); nothing here
// decides a hit, a turn or a death.
import {arr, bool, num, obj, objects, str, type Json} from './json.ts';

export interface FighterView {
    id: string;
    name: string;
    side: number;
    x: number;
    y: number;
    facing: number;             // Eighths of a turn from east (y down).
    status: string;             // "fighting", "downed", "dead".
    npc: boolean;
    away: boolean;
    label: string;              // "Unhurt", "Scratched", "Wounded", "Badly hurt", "Limping", "Downed", "Dead".
    health: number;
    downedLeft: number;         // Seconds, for one's own side's Downed; 0 otherwise.
    mouth: string;              // "sword" when one is held in the jaws.
    burning: number;            // Turns of Burning left.
    casting: boolean;           // Gathering fire (the tell).
    truce: boolean;             // Agreed to the truce on offer.
    meter: number;              // Initiative, 0..100 as last sent; it fills at `rate` a second until full.
    rate: number;
    acting: boolean;            // Taking a turn now (several may be at once: doc 33).
    turnLeft: number;           // Seconds left in it, when acting.
    appearance: Json | null;    // How they look, for the fight screen's portraits (doc 37).
    lifeStage: string;
    stamina: number;            // Everyone's (−1 if not sent).
    mana: number;               // The Gifted's (−1 otherwise), out of manaMax.
    manaMax: number;
    regen: number;              // Stamina back at the start of their next turn (doubled resting).
    fillSeconds: number;        // How long their initiative bar takes to fill from empty.
    resting: boolean;           // Resting this turn (no move): twice the stamina back at the next.
    injuries: InjuryView[];     // What is wrong with them, named (doc 38): on their card, never drawn on them.
    odds: StrikeOdds | null;    // A foe, as this wolf would strike them from where it stands now.
}

/** An injury or condition, named, with what it does. */
export interface InjuryView {
    kind: string;
    name: string;
    does: string;
}

/** A blow from where this wolf stands now: the chance it lands (percent), its usual damage, and whether it reaches. */
export interface StrikeOdds {
    hit: number;
    base: number;               // The chance head on; from the side +10, from behind +20 (doc 33), within 20–95.
    damage: number;
    reach: boolean;
}

export type Tile = [number, number];

/** A player's turn, in seconds (battle::TurnSeconds); typing a line adds as much again, once. */
export const TurnSeconds = 20;

export interface CastView {
    caster: string;
    meter: number;
    quickened: boolean;
    tiles: Tile[];
    left: number;               // Seconds until it goes off (a countdown everyone sees), and how long it gathers.
    of: number;
}

export interface BattleLine {
    seq: number;
    kind: string;
    text: string;
    actor: string;
    target: string;
    tiles: Tile[];
}

export interface BattleView {
    id: string;
    over: boolean;
    banner: string;
    pvp: boolean;
    terms: string;              // "blood", "yield" or "death" (doc 37).
    crime: boolean;             // A resident set on: the watch will hear.
    yieldBy: string;            // Who offers to yield, awaiting an answer.
    arena: {x: number; y: number; w: number; h: number};
    rows: string[];             // The arena's ground, a row a tile, from its top left.
    observer: boolean;
    side: number;
    status: string;
    struggling: boolean;
    canStruggle: boolean;
    turn: string;
    turnLeft: number;
    moved: boolean;
    acted: boolean;
    round: number;
    watching: number;
    fighters: FighterView[];
    reach: Array<[number, number]>;
    log: BattleLine[];
    // This wolf's own means: what is in its jaws, swords carried, a Gift and its mana; burning, gathering fire.
    mouth: string;
    swords: number;
    gift: string;
    quickened: boolean;
    mana: number;
    flame: {length: number; angle: number; mana: number} | null;
    burning: number;
    casting: boolean;
    // This wolf's pace in the fight (the wheel), how far a move goes at it, a tile's stamina, resting this turn, its stats.
    pace: number;
    moveRange: number;
    tileStamina: number;
    resting: boolean;
    stats: {dex: number; baseDex: number; str: number; wis: number} | null;
    agreed: boolean;
    truceBy: string;
    casts: CastView[];
    drops: Array<{x: number; y: number; item: string}>;
    smoke: Tile[];
}

export interface GroundView {
    id: string;
    item: string;
    x: number;
    y: number;
    near: boolean;
}

export interface FightSquare {
    id: string;
    x0: number;
    y0: number;
    x1: number;
    y1: number;
    standing: [number, number];
    names: [string, string];
    round: number;
    over: boolean;
    canJoin: boolean;
    canObserve: boolean;
    watching: boolean;
    actions: number;
    latest: string;
}

export interface ChallengeView {
    from: string;
    name: string;
    left: number;
    terms: string;
}

/** A fight's terms, in words: "to first blood", "until one yields", "until one goes down" (no player dies: doc 38). */
export function termsWords(terms: string): string {
    return terms === 'blood' ? 'to first blood' : terms === 'yield' ? 'until one yields' : 'until one goes down';
}

export function readBattle(snapshot: Json | null): BattleView | null {
    const b = obj(snapshot, 'battle');
    if (!b) return null;
    const arena = obj(b, 'arena');
    const you = obj(b, 'you');
    const pair = (v: unknown): [number, number] | null =>
        Array.isArray(v) && v.length === 2 && typeof v[0] === 'number' && typeof v[1] === 'number' ? [v[0], v[1]] : null;
    const tiles = (list: unknown[]): Tile[] => list.map(pair).filter((p): p is Tile => p !== null);
    return {
        id: str(b, 'id'),
        over: bool(b, 'over'),
        banner: str(b, 'banner'),
        pvp: bool(b, 'pvp'),
        terms: str(b, 'terms', 'death'),
        crime: bool(b, 'crime'),
        yieldBy: str(b, 'yieldBy'),
        arena: {x: Math.trunc(num(arena, 'x')), y: Math.trunc(num(arena, 'y')), w: Math.trunc(num(arena, 'w')), h: Math.trunc(num(arena, 'h'))},
        rows: arr(b, 'rows').filter((r): r is string => typeof r === 'string'),
        observer: bool(you, 'observer', true),
        side: Math.trunc(num(you, 'side', -1)),
        status: str(you, 'status'),
        struggling: bool(you, 'struggling'),
        canStruggle: bool(you, 'canStruggle'),
        turn: str(b, 'turn'),
        turnLeft: num(b, 'turnLeft'),
        moved: bool(b, 'moved'),
        acted: bool(b, 'acted'),
        round: Math.trunc(num(b, 'round')),
        watching: Math.trunc(num(b, 'watching')),
        fighters: objects(b, 'fighters').map(f => ({
            id: str(f, 'id'), name: str(f, 'name'), side: Math.trunc(num(f, 'side')), x: Math.trunc(num(f, 'x')),
            y: Math.trunc(num(f, 'y')), facing: Math.trunc(num(f, 'facing')), status: str(f, 'status', 'fighting'),
            npc: bool(f, 'npc'), away: bool(f, 'away'), label: str(f, 'label'), health: num(f, 'health'),
            downedLeft: num(f, 'downedLeft'), mouth: str(f, 'mouth'), burning: Math.trunc(num(f, 'burning')),
            casting: bool(f, 'casting'), truce: bool(f, 'truce'), meter: num(f, 'meter'), rate: num(f, 'rate'),
            acting: bool(f, 'acting'), turnLeft: num(f, 'turnLeft'),
            appearance: obj(f, 'appearance'), lifeStage: str(f, 'lifeStage', 'adult'),
            stamina: num(f, 'stamina', -1), mana: num(f, 'mana', -1), manaMax: num(f, 'manaMax', 0),
            regen: num(f, 'regen'), fillSeconds: num(f, 'fillSeconds'), resting: bool(f, 'resting'),
            injuries: objects(f, 'injuries').map(i => ({kind: str(i, 'kind'), name: str(i, 'name'), does: str(i, 'does')})),
            odds: obj(f, 'odds') ? {hit: num(obj(f, 'odds'), 'hit'), base: num(obj(f, 'odds'), 'base', num(obj(f, 'odds'), 'hit')),
                damage: num(obj(f, 'odds'), 'damage'), reach: bool(obj(f, 'odds'), 'reach')} : null,
        })),
        reach: arr(b, 'reach').map(pair).filter((p): p is [number, number] => p !== null),
        log: objects(b, 'log').map(l => ({seq: num(l, 'seq'), kind: str(l, 'kind'), text: str(l, 'text'), actor: str(l, 'actor'),
            target: str(l, 'target'), tiles: tiles(arr(l, 'tiles'))})),
        mouth: str(you, 'mouth'),
        swords: Math.trunc(num(you, 'swords')),
        gift: str(you, 'gift'),
        quickened: bool(you, 'quickened'),
        mana: num(you, 'mana'),
        flame: str(you, 'gift') === 'fire' ? {length: num(you, 'flameLength', 3), angle: num(you, 'flameAngle', 23), mana: num(you, 'flameMana', 25)}
            : null,
        burning: Math.trunc(num(you, 'burning')),
        casting: bool(you, 'casting'),
        pace: Math.trunc(num(you, 'pace')),
        moveRange: Math.trunc(num(you, 'moveRange')),
        tileStamina: num(you, 'tileStamina'),
        resting: bool(you, 'resting'),
        stats: obj(you, 'stats') ? {dex: num(obj(you, 'stats'), 'dex'), baseDex: num(obj(you, 'stats'), 'baseDex'),
            str: num(obj(you, 'stats'), 'str'), wis: num(obj(you, 'stats'), 'wis')} : null,
        agreed: bool(you, 'truce'),
        truceBy: str(b, 'truceBy'),
        casts: objects(b, 'casts').map(c => ({caster: str(c, 'caster'), meter: num(c, 'meter'), quickened: bool(c, 'quickened'),
            tiles: tiles(arr(c, 'tiles')), left: num(c, 'left'), of: num(c, 'of')})),
        drops: objects(b, 'drops').map(d => ({x: Math.trunc(num(d, 'x')), y: Math.trunc(num(d, 'y')), item: str(d, 'item')})),
        smoke: tiles(arr(b, 'smoke')),
    };
}

export function readGround(snapshot: Json | null): GroundView[] {
    return objects(snapshot, 'ground').map(g => ({id: str(g, 'id'), item: str(g, 'item'), x: num(g, 'x'), y: num(g, 'y'), near: bool(g, 'near')}));
}

/** Eighths of a turn from east, for a step (dx, dy), as the server counts them. */
export function octant(dx: number, dy: number): number {
    if (dx === 0 && dy === 0) return 0;
    return ((Math.round(Math.atan2(dy, dx) / (Math.PI / 4)) % 8) + 8) % 8;
}

/** The tiles a flame from (x, y) toward (tx, ty) would take in (the server locks its own; this is the preview). */
export function coneTiles(b: BattleView, x: number, y: number, tx: number, ty: number, length: number, halfAngle: number): Tile[] {
    const out: Tile[] = [];
    if (tx === x && ty === y) return out;
    const aim = Math.atan2(ty - y, tx - x);
    const {x: ax, y: ay, w, h} = b.arena;
    for (let cy = ay; cy < ay + h; ++cy)
        for (let cx = ax; cx < ax + w; ++cx) {
            const dx = cx - x, dy = cy - y, far = Math.hypot(dx, dy);
            if (far < 0.5 || far > length + 0.5) continue;
            let off = Math.abs(Math.atan2(dy, dx) - aim) * 180 / Math.PI;
            if (off > 180) off = 360 - off;
            if (off <= halfAngle + 1e-6) out.push([cx, cy]);
        }
    return out;
}

export function readFights(snapshot: Json | null): FightSquare[] {
    return objects(snapshot, 'fights').map(f => ({
        id: str(f, 'id'), x0: num(f, 'x0'), y0: num(f, 'y0'), x1: num(f, 'x1'), y1: num(f, 'y1'),
        standing: [Math.trunc(num(f, 'standing0')), Math.trunc(num(f, 'standing1'))],
        names: [str(f, 'side0'), str(f, 'side1')],
        round: Math.trunc(num(f, 'round')), over: bool(f, 'over'), canJoin: bool(f, 'canJoin'),
        canObserve: bool(f, 'canObserve'), watching: bool(f, 'watching'), actions: Math.trunc(num(f, 'actions')), latest: str(f, 'latest'),
    }));
}

export function readChallenge(snapshot: Json | null): ChallengeView | null {
    const c = obj(snapshot, 'challenge');
    return c ? {from: str(c, 'from'), name: str(c, 'name'), left: num(c, 'left'), terms: str(c, 'terms', 'yield')} : null;
}

/** Whether a fighter's own turn is now, and they may still do something with it. */
export function myTurn(b: BattleView | null, selfId: string): boolean {
    return !!b && !b.over && !b.observer && b.turn === selfId;
}

/** The fighter standing (or lying) on a tile, if any. */
export function fighterAt(b: BattleView, x: number, y: number): FighterView | undefined {
    return b.fighters.find(f => f.x === x && f.y === y && f.status !== 'fled');
}

/** The cell's rows with the arena's ground laid over them. */
export function arenaRows(rows: string[], b: BattleView): string[] {
    const out = rows.slice();
    const {x, y, w} = b.arena;
    b.rows.forEach((line, i) => {
        const row = out[y + i];
        if (row === undefined) return;
        if (row.length < x + w) return;
        out[y + i] = row.slice(0, x) + line.slice(0, w).padEnd(w, ' ') + row.slice(x + w);
    });
    return out;
}

/** What a fighter sees: the whole arena, lit; nothing outside it (only the fighters are in it). */
export function arenaSight(width: number, height: number, b: BattleView): string[] {
    const out: string[] = [];
    const {x, y, w, h} = b.arena;
    for (let ty = 0; ty < height; ++ty) {
        if (ty < y || ty >= y + h) {
            out.push('0'.repeat(width));
            continue;
        }
        out.push('0'.repeat(Math.max(0, x)) + '2'.repeat(Math.max(0, Math.min(w, width - x))) + '0'.repeat(Math.max(0, width - x - w)));
    }
    return out;
}

/** How full a fighter's initiative bar is now (0..1), from what was sent and the time since. */
export function meterNow(f: FighterView, since: number): number {
    return Math.max(0, Math.min(100, f.meter + f.rate * Math.max(0, since))) / 100;
}

/** How far apart two facings are, 0..4 eighths. */
export function octantGap(a: number, b: number): number {
    const d = (((a - b) % 8) + 8) % 8;
    return Math.min(d, 8 - d);
}

/** Where a blow from (x, y) comes at a wolf from: 'front', 'side' or 'back' of how it faces. */
export function quarter(foe: FighterView, x: number, y: number): 'front' | 'side' | 'back' {
    const gap = octantGap(foe.facing, octant(x - foe.x, y - foe.y));
    return gap >= 3 ? 'back' : gap === 2 ? 'side' : 'front';
}

/** The chance (percent) of a blow at `foe` from tile (x, y), by the rules the server rolls with. */
export function chanceFrom(foe: FighterView, x: number, y: number): number {
    if (!foe.odds) return 0;
    const q = quarter(foe, x, y);
    return Math.max(20, Math.min(95, foe.odds.base + (q === 'back' ? 20 : q === 'side' ? 10 : 0)));
}

/** Tiles apart, diagonals counting one (as reach is counted). */
export function apart(x: number, y: number, tx: number, ty: number): number {
    return Math.max(Math.abs(x - tx), Math.abs(y - ty));
}

/**
 * Where clicking a foe out of reach would step to: the lit tile nearest them (the nearer to oneself on a tie), and
 * whether a blow reaches from there. Null when one can't move now.
 */
export function stepToward(b: BattleView, me: FighterView, foe: FighterView, range: number): {x: number; y: number; reaches: boolean} | null {
    if (b.moved || !b.reach.length) return null;
    let best: [number, number] | null = null;
    for (const [x, y] of b.reach)
        if (!best || apart(x, y, foe.x, foe.y) < apart(best[0], best[1], foe.x, foe.y) || (apart(x, y, foe.x, foe.y) === apart(best[0], best[1], foe.x, foe.y) &&
            apart(x, y, me.x, me.y) < apart(best[0], best[1], me.x, me.y)))
            best = [x, y];
    return best ? {x: best[0], y: best[1], reaches: apart(best[0], best[1], foe.x, foe.y) <= range} : null;
}

/** A path from one's tile to a lit one, through lit tiles (eight ways), for showing where a move goes. */
export function pathTo(b: BattleView, from: Tile, to: Tile): Tile[] {
    const lit = new Set(b.reach.map(([x, y]) => `${x},${y}`));
    lit.add(`${from[0]},${from[1]}`);
    const back = new Map<string, string>();
    const start = `${from[0]},${from[1]}`, goal = `${to[0]},${to[1]}`;
    const queue = [start];
    back.set(start, '');
    while (queue.length) {
        const at = queue.shift()!;
        if (at === goal) break;
        const [x, y] = at.split(',').map(Number);
        for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1], [1, 1], [1, -1], [-1, 1], [-1, -1]]) {
            const next = `${x + dx},${y + dy}`;
            if (lit.has(next) && !back.has(next)) {
                back.set(next, at);
                queue.push(next);
            }
        }
    }
    if (!back.has(goal)) return [from, to];
    const out: Tile[] = [];
    for (let at = goal; at; at = back.get(at) ?? '') out.unshift(at.split(',').map(Number) as Tile);
    return out;
}

/** Seconds until a fighter's bar is full (0 when it is, or they are acting). */
export function secondsToTurn(f: FighterView, since: number): number {
    if (f.acting) return 0;
    const now = meterNow(f, since) * 100;
    return f.rate > 0 ? Math.max(0, (100 - now) / f.rate) : now >= 100 ? 0 : Infinity;
}

/** Seconds as m:ss. */
export function clockLabel(seconds: number): string {
    const s = Math.max(0, Math.round(seconds));
    return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}
