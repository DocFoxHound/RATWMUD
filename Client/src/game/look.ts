// Looking at something with the pointer (Docs/Design/29-client-polish.md, phase 3): what is under it, in words, from
// what the client already holds (never asking the server, so it can't reveal anything the wolf doesn't know).
import {bool, num, objects, str} from './json.ts';
import {terrainInfo} from './terrainLayer.ts';
import type {EntityView, GameState} from './state.ts';

export interface Look {
    key: string;                // What is looked at (the same thing keeps the same key while the pointer moves).
    what: string;               // Its name.
    why: string;                // What it means for a wolf.
}

const upperFirst = (s: string) => (s ? s[0].toUpperCase() + s.slice(1) : s);

/** A wolf in words: name, what they are, what they are doing. Never an exact age. */
export function describeWolf(e: EntityView): Look {
    const what = e.self ? `${e.name || 'You'} (you)` : e.name || 'Someone';
    const kind = e.self ? '' : e.kind === 'npc' ? (e.work ? upperFirst(e.work) : 'A resident') : 'A player';
    const doing = e.state && e.state !== 'standing' ? e.state : e.moving ? 'moving' : 'standing';
    // A player's status and Currently line (doc 50): what they have chosen to show.
    const status = ({ooc: 'out of character', lfs: 'looking for a scene', quill: 'storyteller'} as Record<string, string>)[e.rp ?? ''] ?? '';
    return {key: `wolf:${e.id}`, what, why: [kind, e.rel === 'party' ? 'your party' : '', e.hostile ? (e.why ? `hostile · ${e.why}` : 'hostile') : '', e.lifeStage !== 'adult' ? e.lifeStage : '',
        status, e.walkup ? 'walk-up friendly' : '', e.currently ? `“${e.currently}”` : doing]
        .filter(Boolean).join(' · ')};
}

/** What is under a point on the local map (canvas pixels), or null for nothing. */
export function lookAt(s: GameState, point: [number, number]): Look | null {
    if (s.worldMap || s.tileSize <= 0) return null;
    const m = s.mapRect;
    if (point[0] < m.left || point[0] >= m.right || point[1] < m.top || point[1] >= m.bottom) return null;
    const [ox, oy] = s.mapOrigin, tile = s.tileSize;
    // Wolves first (they stand on the ground), then doors and the herb patch, then the ground itself.
    let nearest: EntityView | null = null, best = 15;
    for (const e of s.entities.values()) {
        const d = Math.hypot(ox + e.x * tile - point[0], oy + e.y * tile - point[1]);
        if (d < best) [best, nearest] = [d, e];
    }
    if (nearest) return describeWolf(nearest);
    for (const door of objects(s.snapshot, 'doors')) {
        const d = Math.hypot(ox + num(door, 'x') * tile - point[0], oy + num(door, 'y') * tile - point[1]);
        if (d < 15) {
            const name = str(door, 'name', 'A door');
            return {key: `door:${str(door, 'id')}`, what: name, why: `${bool(door, 'open') ? 'Open' : 'Closed'} · click for what you can do`};
        }
    }
    const resource = s.visibleResource();
    if (resource && Math.hypot(ox + num(resource, 'x') * tile - point[0], oy + num(resource, 'y') * tile - point[1]) < 15) {
        const left = Math.trunc(num(resource, 'remaining'));
        return {key: 'herb_patch', what: 'Cooking herbs', why: left > 0 ? `${left} bundles to gather` : 'Picked clean for now'};
    }
    const x = Math.floor((point[0] - ox) / tile), y = Math.floor((point[1] - oy) / tile);
    if (x < 0 || y < 0 || x >= s.cellWidth || y >= s.cellHeight) return null;
    const code = s.tileRows[y]?.[x] ?? ' ';
    const seen = s.visibilityRows[y]?.[x] ?? '2';
    if (seen === '0' || code === ' ') return {key: `tile:${x},${y}`, what: 'Unexplored', why: 'You have not seen this ground'};
    const info = terrainInfo(code);
    const name = info?.name ?? 'Something unfamiliar';
    const rise = s.heightAt(x, y) - s.selfHeight();
    const height = info?.kind === 'cliff' ? 'a sheer drop' : rise >= 2 ? 'well above you' : rise >= 0.5 ? 'above you'
        : rise <= -2 ? 'well below you' : rise <= -0.5 ? 'below you' : '';
    const why = [info?.effect ?? '', height].filter(Boolean).join(' · ');
    return seen === '1' ? {key: `tile:${x},${y}`, what: `${name} (remembered)`, why: why || 'As you last saw it'}
        : {key: `tile:${x},${y}`, what: name, why};
}
