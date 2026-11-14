// The realm's map, whole (Core/RatwRealmMap.h; the user, 2026-11-14: "I think we should just have the full worldmap
// and not just where a player has been/seen", and "when the player zooms in close enough they should see the names of
// the areas and towns"). Sent once by the server when asked (`realmMap`), never in a snapshot: every outdoor cell's
// ground sampled down, the names of its regions, towns and districts by rank, and its buildings with what they are.
import {arr, isObject, num, str, type Json} from './json.ts';

export interface RealmLabel {
    name: string;
    kind: string;               // "region", "city", "town" or "district".
    rank: number;               // 0 shows on the whole map; 1 a little closer; 2 close in.
    x: number;
    y: number;
}

export interface RealmBuilding {
    name: string;
    kind: string;               // What it is, for its icon (mapIcons.ts): "inn", "smith"... or "" for nothing to say.
    x: number;                  // The middle of its footprint, world tiles.
    y: number;
}

export interface RealmMap {
    build: string;
    x0: number;                 // The ground's top-left, world tiles, and how many tiles each sample is.
    y0: number;
    step: number;
    cols: number;
    rows: number;
    ground: string;             // cols x rows glyph codes, ' ' where there is none.
    labels: RealmLabel[];
    buildings: RealmBuilding[];
}

/** The map from the server's event, or null if it is not one (or is still being made: `pending`). */
export function readRealm(e: Json): RealmMap | null {
    const cols = Math.trunc(num(e, 'cols')), rows = Math.trunc(num(e, 'rows')), step = Math.trunc(num(e, 'step', 1));
    const ground = str(e, 'ground');
    if (cols <= 0 || rows <= 0 || cols > 4096 || rows > 4096 || step < 1 || ground.length !== cols * rows) return null;
    const labels: RealmLabel[] = arr(e, 'labels').filter(isObject).map(l => ({name: str(l, 'n'), kind: str(l, 'k'),
        rank: Math.max(0, Math.min(3, Math.trunc(num(l, 'r', 2)))), x: num(l, 'x'), y: num(l, 'y')})).filter(l => l.name);
    const buildings: RealmBuilding[] = arr(e, 'buildings').filter(isObject).map(b => ({name: str(b, 'n'), kind: str(b, 'k'),
        x: num(b, 'x'), y: num(b, 'y')}));
    return {build: str(e, 'build'), x0: num(e, 'x0'), y0: num(e, 'y0'), step, cols, rows, ground, labels, buildings};
}

/** The scale (pixels a tile) from which a label of this rank is shown on the World Map. */
export function labelScale(rank: number): number {
    return rank <= 0 ? 0.15 : rank === 1 ? 0.35 : rank === 2 ? 0.6 : 4;
}

/** The scale from which buildings' icons show (houses, the most of them, only closer still). */
export const IconScale = 2;
export const HouseIconScale = 4;

/** The labels to draw at this scale, larger places first. */
export function labelsAt(realm: RealmMap | null, scale: number): RealmLabel[] {
    if (!realm) return [];
    return realm.labels.filter(l => scale >= labelScale(l.rank)).sort((a, b) => a.rank - b.rank);
}

/** The scale at which the whole realm fills a box (the World Map opens fitted), or 0 without one. */
export function realmFit(realm: RealmMap | null, w: number, h: number): number {
    if (!realm) return 0;
    const width = realm.cols * realm.step, height = realm.rows * realm.step;
    return Math.max(0.02, Math.min(8, 0.94 * Math.min(w / Math.max(1, width), h / Math.max(1, height))));
}
