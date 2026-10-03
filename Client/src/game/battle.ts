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
    label: string;              // "Scratched", "Wounded", "Badly hurt", "Limping", "Downed", "Dead".
    health: number;
    downedLeft: number;         // Seconds, for one's own side's Downed; 0 otherwise.
}

export interface BattleLine {
    seq: number;
    kind: string;
    text: string;
}

export interface BattleView {
    id: string;
    over: boolean;
    banner: string;
    pvp: boolean;
    arena: {x: number; y: number; w: number; h: number};
    rows: string[];             // The arena's ground, a row a tile, from its top left.
    observer: boolean;
    side: number;
    status: string;
    struggling: boolean;
    canStruggle: boolean;
    turn: string;
    turnName: string;
    turnLeft: number;
    moved: boolean;
    acted: boolean;
    round: number;
    watching: number;
    fighters: FighterView[];
    order: string[];
    reach: Array<[number, number]>;
    log: BattleLine[];
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
}

export interface ChallengeView {
    from: string;
    name: string;
    left: number;
}

export function readBattle(snapshot: Json | null): BattleView | null {
    const b = obj(snapshot, 'battle');
    if (!b) return null;
    const arena = obj(b, 'arena');
    const you = obj(b, 'you');
    const pair = (v: unknown): [number, number] | null =>
        Array.isArray(v) && v.length === 2 && typeof v[0] === 'number' && typeof v[1] === 'number' ? [v[0], v[1]] : null;
    return {
        id: str(b, 'id'),
        over: bool(b, 'over'),
        banner: str(b, 'banner'),
        pvp: bool(b, 'pvp'),
        arena: {x: Math.trunc(num(arena, 'x')), y: Math.trunc(num(arena, 'y')), w: Math.trunc(num(arena, 'w')), h: Math.trunc(num(arena, 'h'))},
        rows: arr(b, 'rows').filter((r): r is string => typeof r === 'string'),
        observer: bool(you, 'observer', true),
        side: Math.trunc(num(you, 'side', -1)),
        status: str(you, 'status'),
        struggling: bool(you, 'struggling'),
        canStruggle: bool(you, 'canStruggle'),
        turn: str(b, 'turn'),
        turnName: str(b, 'turnName'),
        turnLeft: num(b, 'turnLeft'),
        moved: bool(b, 'moved'),
        acted: bool(b, 'acted'),
        round: Math.trunc(num(b, 'round')),
        watching: Math.trunc(num(b, 'watching')),
        fighters: objects(b, 'fighters').map(f => ({
            id: str(f, 'id'), name: str(f, 'name'), side: Math.trunc(num(f, 'side')), x: Math.trunc(num(f, 'x')),
            y: Math.trunc(num(f, 'y')), facing: Math.trunc(num(f, 'facing')), status: str(f, 'status', 'fighting'),
            npc: bool(f, 'npc'), away: bool(f, 'away'), label: str(f, 'label'), health: num(f, 'health'),
            downedLeft: num(f, 'downedLeft'),
        })),
        order: arr(b, 'order').filter((v): v is string => typeof v === 'string'),
        reach: arr(b, 'reach').map(pair).filter((p): p is [number, number] => p !== null),
        log: objects(b, 'log').map(l => ({seq: num(l, 'seq'), kind: str(l, 'kind'), text: str(l, 'text')})),
    };
}

export function readFights(snapshot: Json | null): FightSquare[] {
    return objects(snapshot, 'fights').map(f => ({
        id: str(f, 'id'), x0: num(f, 'x0'), y0: num(f, 'y0'), x1: num(f, 'x1'), y1: num(f, 'y1'),
        standing: [Math.trunc(num(f, 'standing0')), Math.trunc(num(f, 'standing1'))],
        names: [str(f, 'side0'), str(f, 'side1')],
        round: Math.trunc(num(f, 'round')), over: bool(f, 'over'), canJoin: bool(f, 'canJoin'),
        canObserve: bool(f, 'canObserve'), watching: bool(f, 'watching'),
    }));
}

export function readChallenge(snapshot: Json | null): ChallengeView | null {
    const c = obj(snapshot, 'challenge');
    return c ? {from: str(c, 'from'), name: str(c, 'name'), left: num(c, 'left')} : null;
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

/** Seconds as m:ss. */
export function clockLabel(seconds: number): string {
    const s = Math.max(0, Math.round(seconds));
    return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}
