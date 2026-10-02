// Reading the server's JSON the way the Unreal client did (RatwUI in UI/SRatwGame.cpp): a wrong type reads as the
// default, and numbers that become sizes, strengths or coordinates are checked and bounded.

export type Json = Record<string, unknown>;
export type Maybe = Json | null | undefined;

export const isObject = (v: unknown): v is Json => typeof v === 'object' && v !== null && !Array.isArray(v);

export function str(o: Maybe, k: string, fallback = ''): string {
    const v = o?.[k];
    return typeof v === 'string' ? v : fallback;
}
export function num(o: Maybe, k: string, fallback = 0): number {
    const v = o?.[k];
    return typeof v === 'number' ? v : fallback;
}
export function bool(o: Maybe, k: string, fallback = false): boolean {
    const v = o?.[k];
    return typeof v === 'boolean' ? v : fallback;
}
export function obj(o: Maybe, k: string): Json | null {
    const v = o?.[k];
    return isObject(v) ? v : null;
}
const Empty: readonly unknown[] = Object.freeze([]);
/** The list at `k`, or one shared empty list (the same each time, so "unchanged" can be told by identity). */
export function arr(o: Maybe, k: string): unknown[] {
    const v = o?.[k];
    return Array.isArray(v) ? v : (Empty as unknown[]);
}
export const objects = (o: Maybe, k: string): Json[] => arr(o, k).filter(isObject);

export const clamp = (v: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, v));

export function boundedNum(o: Maybe, k: string, low: number, high: number, fallback = 0): number {
    const v = num(o, k, fallback);
    return Number.isFinite(v) ? clamp(v, low, high) : fallback;
}
/** JSON booleans or strings must never become weather strengths or rendering coordinates. */
export function envNumber(o: Maybe, k: string, low: number, high: number, fallback: number): number {
    const v = o?.[k];
    if (typeof v !== 'number' || !Number.isFinite(v)) return fallback;
    return clamp(v, low, high);
}
export function wholeCount(o: Maybe, k: string, fallback = -1, maximum = 1e9): number {
    const v = o?.[k];
    if (typeof v !== 'number') return fallback;
    return Number.isFinite(v) && v >= 0 && v <= maximum && Math.floor(v) === v ? v : fallback;
}
export function countText(o: Maybe, k: string): string {
    const count = wholeCount(o, k);
    return count < 0 ? '—' : String(count);
}
export const explicitTrue = (o: Maybe, k: string) => o?.[k] === true;
export const wrapCoordinate = (v: number, extent: number) => (extent > 0 ? ((v % extent) + extent) % extent : 0);
