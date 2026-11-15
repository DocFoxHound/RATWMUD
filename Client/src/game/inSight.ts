// Who is in sight (the In Sight list): the wolves drawn on the map as it is on the page, not everyone in the cell.
//
// The server sends everyone on the player's screen (Docs/Design/80-the-whole-screen.md: 193 x 113 tiles, wider than
// most windows' maps), the cells beside its own included near a seam (World::shownBeside), but never a sneak it hasn't
// made out (World::shown). The list is the part of that the map shows: a wolf is in sight while its token is within
// the map's rectangle on the page. So crossing a seam changes no one in it (the user, 2026-11-15: "'In sight' should
// be whoever is within the view screen, not whoever is in the cell").
import type {EntityView, GameState} from './state.ts';

/** Where the map is on the page, as the painter last drew it (GameState's mapRect, mapOrigin and tileSize). */
export interface MapFrame {
    left: number;
    top: number;
    right: number;
    bottom: number;
    ox: number;                 // Where tile (0, 0)'s corner is on the page...
    oy: number;
    tile: number;               // ...and how many pixels a tile is.
}

export function mapFrame(s: GameState): MapFrame {
    const r = s.mapRect;
    return {left: r.left, top: r.top, right: r.right, bottom: r.bottom, ox: s.mapOrigin[0], oy: s.mapOrigin[1], tile: s.tileSize};
}

/** Whether a wolf at (x, y), in the cell's tiles, is drawn within the map. */
export function onMap(f: MapFrame, x: number, y: number): boolean {
    const px = f.ox + x * f.tile, py = f.oy + y * f.tile;
    return px >= f.left && px <= f.right && py >= f.top && py <= f.bottom;
}

/**
 * Everyone in sight but the player's own wolf, nearest first (by whole tiles, then by id: so two at much the same
 * distance don't trade places every frame). Before the map has been drawn, and in a fight (whose
 * screen shows the arena, not the map), everyone the server sends.
 */
export function inSight(s: GameState): EntityView[] {
    const f = mapFrame(s);
    const framed = !s.battle && f.tile > 0 && f.right > f.left && f.bottom > f.top;
    const me = s.entities.get(s.selfId);
    const out: Array<[number, EntityView]> = [];
    for (const e of s.entities.values()) {
        if (e.self || e.id === s.selfId || (framed && !onMap(f, e.x, e.y))) continue;
        out.push([me ? Math.round(Math.hypot(e.x - me.x, e.y - me.y)) : 0, e]);
    }
    out.sort((a, b) => a[0] - b[0] || (a[1].id < b[1].id ? -1 : 1));
    return out.map(([, e]) => e);
}
