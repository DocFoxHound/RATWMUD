// The map of the country around (Docs/Design/29-client-polish.md, phase 8): every known place drawn where it truly is
// and as large as it truly is, one pixel a tile in its ground's colour, north up, centred on the wolf. Used twice: the
// minimap in the side panel and the full World Map (M). Each place's picture is made once per change of what the wolf
// remembers of it, and kept.
import {arr, bool, isObject, num, obj, objects, str, type Json} from './json.ts';
import {terrainInfo, type Surface, type SurfaceFactory} from './terrainLayer.ts';
import type {GameState} from './state.ts';
import type {WeatherFieldView} from './ambient.ts';
import {HouseIconScale, IconScale, labelsAt, type RealmMap} from './realm.ts';
import {drawMapIcon} from './mapIcons.ts';
import {storyPings} from './quests.ts';

/** Pixels a tile, closest last. */
export const MapScales = [0.25, 0.5, 1, 2, 4, 8];

/** The scale at which the known places fill a box (the World Map opens fitted). */
export function fitScale(snapshot: Json | null, w: number, h: number): number {
    const places = placesOf(snapshot);
    if (!places.length) return 1;
    const x0 = Math.min(...places.map(p => p.x)), y0 = Math.min(...places.map(p => p.y));
    const x1 = Math.max(...places.map(p => p.x + p.width)), y1 = Math.max(...places.map(p => p.y + p.height));
    return Math.max(0.1, Math.min(8, 0.85 * Math.min(w / Math.max(1, x1 - x0), h / Math.max(1, y1 - y0))));
}

/** The party's colour on the maps (as their names and marks in the world: paint.ts Amber). */
const PartyAmber = '#d9b67b';

/** A party mate's mark on a map, in box pixels: held at the edge (pointing `angle` the way they are) when off it. */
interface MateMark {
    x: number;
    y: number;
    edge: boolean;
    angle: number;
    name: string;
    npc: boolean;
    inside: boolean;
    text: string;
}

interface Place {
    id: string;
    name: string;
    x: number;
    y: number;
    width: number;
    height: number;
    visible: boolean;
    current: boolean;
    glyphs: string;
}

const srgb = (hex: string) => [parseInt(hex.slice(1, 3), 16), parseInt(hex.slice(3, 5), 16), parseInt(hex.slice(5, 7), 16)];
const groundColours = new Map<string, number[]>();
/** A tile's colour on the map: its ground, with a little of its glyph's colour. */
function colourOf(code: string): number[] | null {
    if (code === ' ') return null;
    let c = groundColours.get(code);
    if (!c) {
        const t = TerrainHex.get(code);
        if (!t || !terrainInfo(code)) return null;
        const bg = srgb(t[1]), fg = srgb(t[0]);
        c = bg.map((v, i) => Math.round(v * 0.45 + fg[i] * 0.55));
        groundColours.set(code, c);
    }
    return c;
}
// The terrain catalogue's colours as written (sRGB hex), for pixels.
import {TERRAIN} from './terrain.generated.mjs';
const TerrainHex = new Map<string, [string, string]>(TERRAIN.map(t => [t.code, [t.fg, t.bg]]));

/** The places the snapshot shows (worldMap), at the wolf's height. */
export function placesOf(snapshot: Json | null): Place[] {
    const out: Place[] = [];
    for (const c of objects(snapshot, 'worldMap')) {
        const knowledge = str(c, 'knowledge');
        if (knowledge === 'unknown') continue;
        const width = Math.trunc(num(c, 'width')), height = Math.trunc(num(c, 'height'));
        if (width <= 0 || height <= 0 || width > 1024 || height > 1024) continue;
        out.push({id: str(c, 'id'), name: str(c, 'name'), x: num(c, 'x'), y: num(c, 'y'), width, height, visible: bool(c, 'visible'),
            current: bool(c, 'current'), glyphs: str(c, 'glyphs')});
    }
    return out;
}

/** Where the wolf is in the world, in tiles (outdoors, its own place's corner plus where it stands in it); indoors, where
 *  it last stood outdoors if known, else where its place is said to be. */
export function wolfInWorld(s: GameState): [number, number] {
    const here = placesOf(s.snapshot).find(p => p.current);
    const me = s.entities.get(s.selfId);
    const cell = isObject(s.snapshot?.cell) ? s.snapshot!.cell as Json : null;
    const at: [number, number] = [(here?.x ?? num(cell, 'x')) + (me?.x ?? s.cellWidth / 2), (here?.y ?? num(cell, 'y')) + (me?.y ?? s.cellHeight / 2)];
    if (s.outdoors) return at;
    return s.lastOutdoors ?? at;
}

export class MapRenderer {
    private pictures = new Map<string, {key: string; surface: Surface}>();
    private realmPicture: {realm: RealmMap; surface: Surface} | null = null;
    private current: {rows: unknown; visibility: unknown; fringe: unknown; surface: Surface | null; at: number} =
        {rows: null, visibility: null, fringe: null, surface: null, at: -1};
    private surfaces: SurfaceFactory;

    constructor(surfaces: SurfaceFactory) {
        this.surfaces = surfaces;
    }

    /** A remembered place as a picture (one pixel a tile); null without an offscreen canvas or a picture to make. */
    private picture(p: Place): Surface | null {
        if (!p.glyphs || p.glyphs.length !== p.width * p.height) return null;
        const key = `${p.glyphs.length}:${hashText(p.glyphs)}`;
        const kept = this.pictures.get(p.id);
        if (kept && kept.key === key) return kept.surface;
        const surface = this.surfaces(p.width, p.height);
        if (!surface) return null;
        const image = surface.ctx.createImageData(p.width, p.height);
        for (let i = 0; i < p.glyphs.length; ++i) {
            const c = colourOf(p.glyphs[i]);
            if (!c) continue;
            image.data[i * 4] = Math.round(c[0] * 0.7);
            image.data[i * 4 + 1] = Math.round(c[1] * 0.7);
            image.data[i * 4 + 2] = Math.round(c[2] * 0.7);
            image.data[i * 4 + 3] = 255;
        }
        surface.ctx.putImageData(image, 0, 0);
        if (this.pictures.size > 64) this.pictures.clear();
        this.pictures.set(p.id, {key, surface});
        return surface;
    }

    /** The whole realm's ground as a picture, a pixel a sample, made once per map. */
    private realmSurface(realm: RealmMap): Surface | null {
        if (this.realmPicture?.realm === realm) return this.realmPicture.surface;
        const surface = this.surfaces(realm.cols, realm.rows);
        if (!surface) return null;
        const image = surface.ctx.createImageData(realm.cols, realm.rows);
        const g = realm.ground;
        for (let i = 0; i < g.length; ++i) {
            const c = colourOf(g[i]);
            if (!c) continue;
            image.data[i * 4] = Math.round(c[0] * 0.6);
            image.data[i * 4 + 1] = Math.round(c[1] * 0.6);
            image.data[i * 4 + 2] = Math.round(c[2] * 0.6);
            image.data[i * 4 + 3] = 255;
        }
        surface.ctx.putImageData(image, 0, 0);
        this.realmPicture = {realm, surface};
        return surface;
    }

    /**
     * The wolf's own place, from its rows, with the ground round it past a seam when it has some (the band: doc 70),
     * `band` tiles each way: what it sees bright, what it remembers dim. Remade at most twice a second.
     */
    private currentPicture(s: GameState, clock: number): Surface | null {
        const k = this.current;
        if (k.surface && k.rows === s.tileRows && k.fringe === s.fringeRows && (k.visibility === s.visibilityRows || clock - k.at < 0.5))
            return k.surface;
        const band = s.fringe, w = s.cellWidth + 2 * band, h = s.cellHeight + 2 * band;
        if (!k.surface || k.surface.canvas.width !== w || k.surface.canvas.height !== h) k.surface = this.surfaces(w, h);
        if (!k.surface) return null;
        const image = k.surface.ctx.createImageData(w, h);
        const put = (glyph: string, v: string, px: number, py: number) => {
            if (v === '0') return;
            const c = colourOf(glyph);
            if (!c) return;
            const f = v === '2' ? 1 : 0.55, i = (py * w + px) * 4;
            image.data[i] = Math.round(c[0] * f);
            image.data[i + 1] = Math.round(c[1] * f);
            image.data[i + 2] = Math.round(c[2] * f);
            image.data[i + 3] = 255;
        };
        for (let y = 0; y < s.cellHeight; ++y) {
            const row = s.tileRows[y] ?? '', seen = s.visibilityRows[y] ?? '';
            for (let x = 0; x < s.cellWidth; ++x) put(row[x] ?? ' ', seen[x] ?? '2', x + band, y + band);
        }
        // The band's rows (fringe.ts): whole above and below the cell, else its two ends.
        s.fringeRows.forEach((glyphs, r) => {
            const seen = s.fringeVisibility[r] ?? '', whole = r < band || r >= band + s.cellHeight;
            for (let c = 0; c < glyphs.length; ++c) put(glyphs[c], seen[c] ?? '0', whole || c < band ? c : s.cellWidth + c, r);
        });
        k.surface.ctx.putImageData(image, 0, 0);
        k.rows = s.tileRows;
        k.visibility = s.visibilityRows;
        k.fringe = s.fringeRows;
        k.at = clock;
        return k.surface;
    }

    /**
     * Draws the map into a box of `c` (in its own units): centred on the wolf (plus `pan`, in tiles), `scale` pixels a
     * tile. With `labels`, places are named.
     */
    /** Where the last draw put open scenes' marks (box pixels), and what each says: for the pointer. */
    sceneMarks: {x: number; y: number; text: string}[] = [];

    draw(c: CanvasRenderingContext2D, s: GameState, box: {x: number; y: number; w: number; h: number}, scale: number,
        pan: [number, number] = [0, 0], labels = false) {
        const places = placesOf(s.snapshot);
        const here = places.find(p => p.current);
        const me = s.entities.get(s.selfId);
        const cell = isObject(s.snapshot?.cell) ? s.snapshot!.cell as Json : null;
        if (s.outdoors && here && me) s.lastOutdoors = [here.x + me.x, here.y + me.y];
        // The World Map is the whole realm (labels: it is the World Map): centred where the wolf last stood outdoors,
        // even from indoors. The minimap stays on the wolf's own place.
        const realm = labels ? s.realm : null;
        const centre: [number, number] = realm ? wolfInWorld(s)
            : [(here?.x ?? num(cell, 'x')) + (me?.x ?? s.cellWidth / 2), (here?.y ?? num(cell, 'y')) + (me?.y ?? s.cellHeight / 2)];
        const cx = centre[0] + pan[0], cy = centre[1] + pan[1];
        this.view = {box: {...box}, cx, cy, scale};
        const toX = (wx: number) => box.x + box.w / 2 + (wx - cx) * scale;
        const toY = (wy: number) => box.y + box.h / 2 + (wy - cy) * scale;
        c.save();
        c.beginPath();
        c.rect(box.x, box.y, box.w, box.h);
        c.clip();
        c.fillStyle = '#0b1213';
        c.fillRect(box.x, box.y, box.w, box.h);
        c.imageSmoothingEnabled = scale < 1;
        // The whole realm under it all (the World Map): every place's ground, seen or not (the user, 2026-11-14), and
        // over it what this wolf remembers, in its own detail, and what it sees, bright.
        const ground = realm ? this.realmSurface(realm) : null;
        if (realm && ground) {
            // (Smoothed when magnified: a sample is several tiles, and blocks of them read as noise, not land.)
            c.imageSmoothingEnabled = true;
            c.drawImage(ground.canvas, toX(realm.x0), toY(realm.y0), realm.cols * realm.step * scale, realm.rows * realm.step * scale);
            c.imageSmoothingEnabled = scale < 1;
        }
        // Remembered places first, then the wolf's own on top: with the ground round it past a seam (the band), which
        // carries what it sees in the places beside while it is near them (their own pictures are kept as they were).
        for (const p of [...places.filter(p => !p.current), ...places.filter(p => p.current)]) {
            const x = toX(p.x), y = toY(p.y), w = p.width * scale, h = p.height * scale;
            if (x > box.x + box.w || y > box.y + box.h || x + w < box.x || y + h < box.y) continue;
            const picture = p.current ? this.currentPicture(s, s.clock) : this.picture(p);
            const band = p.current && picture ? s.fringe * scale : 0;
            if (picture) c.drawImage(picture.canvas, x - band, y - band, w + 2 * band, h + 2 * band);
            else {
                c.fillStyle = p.current ? 'rgba(168,194,166,0.18)' : 'rgba(139,155,145,0.12)';
                c.fillRect(x, y, w, h);
            }
            // (Over the whole realm a place's edge is only clutter, the wolf's own too: its box ran across the map
            // zoomed in, and the wolf's mark says where it is.)
            if (realm) continue;
            c.strokeStyle = p.current ? 'rgba(217,182,123,0.6)' : p.visible ? 'rgba(168,194,166,0.35)' : 'rgba(139,155,145,0.18)';
            c.lineWidth = 1;
            c.strokeRect(x + 0.5, y + 0.5, w - 1, h - 1);
        }
        // Where the weather is, faintly (the field over the wolf's own place): for a few seconds after a crossing the
        // old place's too, where it lay, fading as the new one's comes in (ambient.ts), so the minimap's colour does
        // not jump at a seam any more than the map's.
        const tints: Record<string, string> = {r: '115,155,178', s: '78,98,117', n: '220,232,238', f: '190,205,202', d: '199,154,92', o: '154,163,168'};
        const weather = (f: WeatherFieldView, x0: number, y0: number, weight: number) => {
            for (let yy = 0; yy < f.rows; ++yy)
                for (let xx = 0; xx < f.cols; ++xx) {
                    const i = yy * f.cols + xx, tint = tints[f.kinds[i]], a = Number(f.amounts[i]) / 9 * weight;
                    if (!tint || a <= 0.002) continue;
                    c.fillStyle = `rgba(${tint},${(a * 0.28).toFixed(3)})`;
                    c.fillRect(toX(x0 + xx * f.step - f.step / 2), toY(y0 + yy * f.step - f.step / 2), f.step * scale, f.step * scale);
                }
        };
        const mix = s.ambient.fieldMix, prior = mix < 1 ? s.ambient.priorField : null;
        if (here && s.weatherField) weather(s.weatherField, here.x, here.y, prior ? mix : 1);
        if (here && prior) weather(prior, here.x + s.ambient.priorOffset[0], here.y + s.ambient.priorOffset[1], 1 - mix);
        // What each building is (the user, 2026-11-14), an icon over its footprint's middle once close enough to read:
        // on the World Map, and on the minimap outdoors (indoors its picture is the room's, not the street's).
        const buildingsFrom = s.realm && (labels || s.outdoors) ? s.realm : null;
        const iconAt: {x: number; y: number; name: string; kind: string}[] = [];
        if (buildingsFrom && scale >= IconScale) {
            const size = Math.max(12, Math.min(16, scale * 4));
            for (const b of buildingsFrom.buildings) {
                if (!b.kind || (b.kind === 'house' && scale < HouseIconScale)) continue;
                const x = toX(b.x), y = toY(b.y);
                if (x < box.x - size || y < box.y - size || x > box.x + box.w + size || y > box.y + box.h + size) continue;
                if (drawMapIcon(c, b.kind, x, y, size, 0.95)) iconAt.push({x, y, name: b.name, kind: b.kind});
            }
        }
        this.icons = iconAt;
        drawTravelLine(c, s, toX, toY, labels);
        // Doors of the wolf's place, then the wolves it can see, then the wolf.
        const ox = here?.x ?? num(cell, 'x'), oy = here?.y ?? num(cell, 'y');
        for (const d of objects(s.snapshot, 'doors')) {
            c.fillStyle = '#d9b67b';
            c.fillRect(toX(ox + num(d, 'x')) - 1.5, toY(oy + num(d, 'y')) - 1.5, 3, 3);
        }
        for (const e of s.entities.values()) {
            if (e.self) continue;
            // (Party mates are the party's marks, below: drawn wherever they are, not only in sight.)
            if (e.rel === 'party' || (!labels && e.kind === 'npc')) continue;
            const x = toX(ox + e.x), y = toY(oy + e.y);
            c.fillStyle = e.rel === 'party' ? '#d9b67b' : e.hostile ? '#e0695e' : e.rel === 'chapter' && e.colour ? e.colour
                : e.kind === 'npc' ? '#a8c2a6' : '#92bacd';
            c.beginPath();
            c.arc(x, y, Math.max(1.5, Math.min(3, scale)), 0, Math.PI * 2);
            c.fill();
            if (s.talkTargets.includes(e.id)) {
                c.strokeStyle = '#d9b67b';
                c.beginPath();
                c.arc(x, y, 5, 0, Math.PI * 2);
                c.stroke();
            }
        }
        // The Chapter's meeting place (doc 32, 5.1): a ring in its colour, in any place this wolf knows.
        const chapter = obj(obj(s.snapshot, 'self'), 'chapter'), meeting = obj(chapter, 'meeting');
        if (meeting) {
            const place = places.find(p => p.id === str(meeting, 'cell'));
            if (place) {
                c.strokeStyle = str(chapter, 'colour', '#d9b67b');
                c.lineWidth = 2;
                c.beginPath();
                c.arc(toX(place.x + num(meeting, 'x')), toY(place.y + num(meeting, 'y')), 5, 0, Math.PI * 2);
                c.stroke();
                c.lineWidth = 1;
            }
        }
        // The Chapter's own ground (doc 32, 5.3): a square in its colour.
        for (const site of arr(chapter, 'sites').filter(isObject)) {
            const place = places.find(p => p.id === str(site, 'cell'));
            if (!place) continue;
            c.strokeStyle = str(chapter, 'colour', '#d9b67b');
            c.strokeRect(toX(place.x + num(site, 'x')) - 4, toY(place.y + num(site, 'y')) - 4, 8, 8);
        }
        // Party mates (doc 32; the user, 2026-11-20): always shown, wherever they are, drawn over the rest (drawMates).
        const mates = this.partyMarks(s, box, places, here, toX, toY, scale, labels);
        this.sceneMarks = [];
        // Each accepted job gets a numbered marker. The number also identifies its description on hover, even when
        // several jobs point to the same town or wagon. Clamp distant goals to the map edge as the tracked story does.
        const jobs = arr(obj(s.snapshot, 'self'), 'missions').filter(isObject);
        const jobPositions: [number, number][] = [];
        jobs.forEach((job, index) => {
            const place = places.find(p => p.id === str(job, 'cell'));
            const worldX = place?.x ?? num(job, 'worldX', NaN), worldY = place?.y ?? num(job, 'worldY', NaN);
            if (!Number.isFinite(worldX) || !Number.isFinite(worldY)) return;
            const x = Math.max(box.x + 11, Math.min(box.x + box.w - 11, toX(worldX + num(job, 'x'))));
            const originalY = Math.max(box.y + 11, Math.min(box.y + box.h - 24, toY(worldY + num(job, 'y'))));
            let y = originalY;
            for (let step = 1; step <= 8 && jobPositions.some(([px, py]) => Math.hypot(px - x, py - y) < 17); ++step)
                y = Math.max(box.y + 11, Math.min(box.y + box.h - 24,
                    originalY + (step % 2 ? 1 : -1) * Math.ceil(step / 2) * 18));
            jobPositions.push([x, y]);
            const n = String(index + 1);
            c.fillStyle = '#1b2927'; c.strokeStyle = '#f0c581'; c.lineWidth = 2;
            c.beginPath(); c.roundRect(x - 8, y - 8, 16, 16, 3); c.fill(); c.stroke();
            c.fillStyle = '#ffe1a1'; c.font = 'bold 11px monospace'; c.textAlign = 'center'; c.textBaseline = 'middle';
            c.fillText(n, x, y + 0.5); c.textAlign = 'start'; c.lineWidth = 1;
            this.sceneMarks.push({x, y, text: `${n} · ${str(job, 'title')}`});
        });
        // Open scenes near (doc 51, §7), when the player has turned them on: a small speech mark in the scene's colour
        // with how many wolves, at their middle or at the door into the place they're in.
        if (s.mapScenes)
            for (const o of arr(obj(obj(s.snapshot, 'self'), 'social'), 'openNear').filter(isObject)) {
                const place = places.find(p => p.id === str(o, 'cell'));
                if (!place) continue;
                const x = toX(place.x + num(o, 'x')), y = toY(place.y + num(o, 'y')), n = Math.trunc(num(o, 'wolves'));
                c.fillStyle = str(o, 'colour', '#8796a3');
                c.strokeStyle = '#11191b';
                c.beginPath();
                c.roundRect(x - 7, y - 13, 14, 10, 3);
                c.moveTo(x - 2, y - 3.5);
                c.lineTo(x, y);
                c.lineTo(x + 2, y - 3.5);
                c.fill();
                c.stroke();
                c.fillStyle = '#11191b';
                c.font = 'bold 8px sans-serif';
                c.textAlign = 'center';
                c.textBaseline = 'middle';
                c.fillText(String(n), x, y - 8);
                c.textAlign = 'start';
                this.sceneMarks.push({x, y: y - 8, text: `${n} ${n === 1 ? 'wolf' : 'wolves'} at ${str(o, 'place')} · ${str(o, 'openness') === 'open' ? 'open'
                    : 'knock to join (a friend is in it)'}`});
            }
        // Where one's tie was when it was made (doc 52), while its marker lasts: a sage ring.
        const tieMark = obj(obj(obj(s.snapshot, 'self'), 'tie'), 'marker');
        const tiePlace = tieMark ? places.find(p => p.id === str(tieMark, 'cell')) : undefined;
        if (tieMark && tiePlace) {
            const x = toX(tiePlace.x + num(tieMark, 'x')), y = toY(tiePlace.y + num(tieMark, 'y'));
            c.strokeStyle = 'rgba(143,179,154,0.9)';
            c.lineWidth = 1.5;
            c.beginPath();
            c.arc(x, y, 5, 0, Math.PI * 2);
            c.stroke();
            this.sceneMarks.push({x, y, text: `where your tie was${str(tieMark, 'place') ? `, at ${str(tieMark, 'place')}` : ''}`});
        }
        // The tracked storyline's next place (doc 58, 1): a small diamond (at the map's edge when it lies beyond), and its
        // words along the bottom.
        const tracked = obj(obj(s.snapshot, 'self'), 'tracked');
        const trackedPlace = tracked ? places.find(p => p.id === str(tracked, 'cell')) : undefined;
        if (tracked && trackedPlace) {
            const raw = [toX(trackedPlace.x + num(tracked, 'x')), toY(trackedPlace.y + num(tracked, 'y'))];
            const x = Math.max(box.x + 6, Math.min(box.x + box.w - 6, raw[0])), y = Math.max(box.y + 6, Math.min(box.y + box.h - 18, raw[1]));
            c.fillStyle = 'rgba(217,182,123,0.95)';
            c.beginPath();
            c.moveTo(x, y - 5);
            c.lineTo(x + 4, y);
            c.lineTo(x, y + 5);
            c.lineTo(x - 4, y);
            c.closePath();
            c.fill();
            const words = `◆ ${str(tracked, 'title')} · ${str(tracked, 'label')}`;
            c.font = '10px sans-serif';
            c.fillText(words.length > 46 ? words.slice(0, 45) + '…' : words, box.x + 4, box.y + box.h - 4);
            this.sceneMarks.push({x, y, text: words});
        }
        // Howls heard (doc 51, Phase 6), for their minute: a faint arrow at the map's edge, the way the sound came.
        if (s.howlMarks)
            for (const [, h] of s.howls) {
                if (h.until <= s.clock) continue;
                const a = h.bearing * Math.PI / 180, dx = Math.sin(a), dy = -Math.cos(a);
                const half = Math.min(box.w, box.h) / 2 - 10;
                const x = box.x + box.w / 2 + dx * half, y = box.y + box.h / 2 + dy * half;
                c.fillStyle = `rgba(217,182,123,${(0.35 + 0.5 * Math.min(1, (h.until - s.clock) / 20)).toFixed(2)})`;
                c.beginPath();
                c.moveTo(x + dx * 8, y + dy * 8);
                c.lineTo(x - dy * 5, y + dx * 5);
                c.lineTo(x + dy * 5, y - dx * 5);
                c.closePath();
                c.fill();
                this.sceneMarks.push({x, y, text: `a howl${h.wolves > 1 ? ` of ${h.wolves} wolves` : ''}${h.status ? ` · ${h.status}` : ''} · ${h.band}`});
            }
        this.drawMates(c, mates, box, labels);
        this.drawPings(c, s, box, here, toX, toY, scale, labels);
        if (me) {
            const x = toX(ox + me.x), y = toY(oy + me.y), r = labels ? 6 : 8;
            c.fillStyle = '#0b1213';
            c.beginPath(); c.arc(x, y, r + 3, 0, Math.PI * 2); c.fill();
            c.strokeStyle = '#ffe1a1'; c.lineWidth = labels ? 1 : 2;
            c.beginPath(); c.arc(x, y, r + 1, 0, Math.PI * 2); c.stroke(); c.lineWidth = 1;
            c.fillStyle = '#d9b67b';
            c.beginPath();
            c.moveTo(x + Math.cos(me.facing) * r, y + Math.sin(me.facing) * r);
            c.lineTo(x + Math.cos(me.facing + 2.5) * r * 0.8, y + Math.sin(me.facing + 2.5) * r * 0.8);
            c.lineTo(x + Math.cos(me.facing - 2.5) * r * 0.8, y + Math.sin(me.facing - 2.5) * r * 0.8);
            c.closePath();
            c.fill();
        }
        if (labels && realm) this.drawNames(c, box, realm, scale, toX, toY, iconAt);
        else if (labels) {
            c.font = '12px sans-serif';
            c.textBaseline = 'top';
            for (const p of places) {
                const x = toX(p.x), y = toY(p.y);
                if (p.width * scale < 60) continue;
                c.fillStyle = p.current ? '#d9b67b' : '#8b9b91';
                c.fillText(p.name, x + 6, y + 5);
            }
        }
        if (!labels && jobs.length) {
            const shown = jobs.slice(0, 3), more = jobs.length > shown.length;
            const lines = shown.length + Number(more), top = box.y + box.h - lines * 13 - 6;
            c.fillStyle = 'rgba(11,18,19,0.88)';
            c.fillRect(box.x + 3, top - 3, box.w - 6, lines * 13 + 6);
            c.font = '10px monospace'; c.textBaseline = 'top';
            shown.forEach((job, i) => {
                c.fillStyle = '#ffe1a1';
                const title = str(job, 'title');
                c.fillText(`${i + 1} ${title.length > 30 ? title.slice(0, 29) + '…' : title}`, box.x + 7, top + i * 13);
            });
            if (more) c.fillText(`+${jobs.length - shown.length} more jobs`, box.x + 7, top + shown.length * 13);
        }
        c.fillStyle = '#8b9b91';
        c.font = '11px monospace';
        c.textBaseline = 'top';
        c.fillText('N ^', box.x + 6, box.y + 5);
        c.restore();
    }

    /**
     * Where each party mate goes on this map (doc 32; the user, 2026-11-20: "When in a party I need to be able to see party
     * members on the minimap and expanded map"): in the wolf's own place where it stands; elsewhere at the spot the
     * server gives in the realm's tiles (indoors, the door into the building from the street), whether this wolf knows
     * the place or not; held at the map's edge, pointing the way, when off it. Each is said on hover (sceneMarks).
     */
    private partyMarks(s: GameState, box: {x: number; y: number; w: number; h: number}, places: Place[], here: Place | undefined,
        toX: (x: number) => number, toY: (y: number) => number, scale: number, labels: boolean): MateMark[] {
        const out: MateMark[] = [];
        // From indoors the minimap is the room's, so a mate elsewhere is placed from where the wolf last stood outside.
        const fromRoom = !labels && !s.outdoors, me = wolfInWorld(s);
        for (const m of s.party?.members ?? []) {
            if (m.id === s.selfId || !m.online || !m.cell) continue;
            let x: number, y: number;
            const seen = s.entities.get(m.id);       // (In sight: where the screen has them, a moment fresher.)
            if (here && m.cell === here.id) x = toX(here.x + (seen?.x ?? m.x)), y = toY(here.y + (seen?.y ?? m.y));
            else {
                const place = places.find(p => p.id === m.cell);
                const wx = Number.isFinite(m.wx) ? m.wx : place ? place.x + m.x : NaN;
                const wy = Number.isFinite(m.wy) ? m.wy : place ? place.y + m.y : NaN;
                if (!Number.isFinite(wx) || !Number.isFinite(wy)) continue;
                if (fromRoom) x = box.x + box.w / 2 + (wx - me[0]) * scale, y = box.y + box.h / 2 + (wy - me[1]) * scale;
                else x = toX(wx), y = toY(wy);
            }
            const pad = 8, inX = Math.max(box.x + pad, Math.min(box.x + box.w - pad, x)), inY = Math.max(box.y + pad, Math.min(box.y + box.h - pad, y));
            const edge = inX !== x || inY !== y;
            const where = m.place ? `${m.inside ? 'in' : 'at'} ${m.place}` : '';
            const state = m.downed ? 'down' : m.fighting ? 'fighting' : '';
            out.push({x: inX, y: inY, edge, angle: Math.atan2(y - inY, x - inX), name: m.name || 'Party mate', npc: m.npc, inside: m.inside,
                text: ['your party', m.name || 'a party mate', where, state].filter(Boolean).join(' · ')});
        }
        return out;
    }

    /** The party's marks (partyMarks): a ring in the party's amber round a dark middle, a roof over it for one indoors, an
     *  arrow for one off the map's edge; on the World Map each one named. */
    private drawMates(c: CanvasRenderingContext2D, mates: MateMark[], box: {x: number; y: number; w: number; h: number}, labels: boolean) {
        for (const m of mates) {
            const r = m.npc ? 3.5 : 4.5;
            c.fillStyle = '#11191b';
            c.beginPath(); c.arc(m.x, m.y, r + 2, 0, Math.PI * 2); c.fill();
            c.fillStyle = PartyAmber;
            c.beginPath(); c.arc(m.x, m.y, r, 0, Math.PI * 2); c.fill();
            c.fillStyle = '#11191b';
            c.beginPath(); c.arc(m.x, m.y, r * 0.4, 0, Math.PI * 2); c.fill();
            if (m.inside) {
                // A small roof: they are in the building at this door.
                c.fillStyle = PartyAmber;
                c.beginPath(); c.moveTo(m.x - r - 1, m.y - r - 1); c.lineTo(m.x, m.y - r - 5); c.lineTo(m.x + r + 1, m.y - r - 1); c.closePath(); c.fill();
            }
            if (m.edge) {
                const dx = Math.cos(m.angle), dy = Math.sin(m.angle), tip = r + 7;
                c.fillStyle = PartyAmber;
                c.beginPath();
                c.moveTo(m.x + dx * tip, m.y + dy * tip);
                c.lineTo(m.x + dx * (r + 2) - dy * 3.5, m.y + dy * (r + 2) + dx * 3.5);
                c.lineTo(m.x + dx * (r + 2) + dy * 3.5, m.y + dy * (r + 2) - dx * 3.5);
                c.closePath(); c.fill();
            }
            if (labels) {
                c.font = "600 11px 'RATW Sans', sans-serif";
                c.textBaseline = 'middle';
                c.textAlign = m.x > box.x + box.w - 110 ? 'right' : 'left';
                const tx = m.x + (c.textAlign === 'right' ? -(r + 5) : r + 5);
                c.lineJoin = 'round'; c.strokeStyle = 'rgba(8,12,13,0.9)'; c.lineWidth = 3;
                c.strokeText(m.name, tx, m.y);
                c.fillStyle = PartyAmber;
                c.fillText(m.name, tx, m.y);
                c.textAlign = 'start'; c.lineWidth = 1;
            }
            this.sceneMarks.push({x: m.x, y: m.y, text: m.text});
        }
    }

    /** A world story's markers (doc 85, Phase 9: story.ping): a gold diamond at the spot, held at the edge with an arrow when off the
     *  map, labelled on the World Map, said on hover. */
    private drawPings(c: CanvasRenderingContext2D, s: GameState, box: {x: number; y: number; w: number; h: number}, here: Place | undefined,
        toX: (x: number) => number, toY: (y: number) => number, scale: number, labels: boolean) {
        const pings = storyPings(obj(s.snapshot, 'self')?.journal);
        if (!pings.length) return;
        const fromRoom = !labels && !s.outdoors, me = wolfInWorld(s);
        void here;
        for (const p of pings) {
            const x = fromRoom ? box.x + box.w / 2 + (p.wx - me[0]) * scale : toX(p.wx);
            const y = fromRoom ? box.y + box.h / 2 + (p.wy - me[1]) * scale : toY(p.wy);
            const pad = 9, inX = Math.max(box.x + pad, Math.min(box.x + box.w - pad, x)), inY = Math.max(box.y + pad, Math.min(box.y + box.h - pad, y));
            const edge = inX !== x || inY !== y, r = labels ? 6 : 5;
            c.fillStyle = '#11191b';
            c.beginPath(); c.moveTo(inX, inY - r - 2); c.lineTo(inX + r + 2, inY); c.lineTo(inX, inY + r + 2); c.lineTo(inX - r - 2, inY); c.closePath(); c.fill();
            c.fillStyle = '#ffd36b';
            c.beginPath(); c.moveTo(inX, inY - r); c.lineTo(inX + r, inY); c.lineTo(inX, inY + r); c.lineTo(inX - r, inY); c.closePath(); c.fill();
            if (edge) {
                const a = Math.atan2(y - inY, x - inX), dx = Math.cos(a), dy = Math.sin(a), tip = r + 8;
                c.beginPath();
                c.moveTo(inX + dx * tip, inY + dy * tip);
                c.lineTo(inX + dx * (r + 2) - dy * 4, inY + dy * (r + 2) + dx * 4);
                c.lineTo(inX + dx * (r + 2) + dy * 4, inY + dy * (r + 2) - dx * 4);
                c.closePath(); c.fill();
            }
            if (labels && p.label) {
                c.font = "600 11px 'RATW Sans', sans-serif";
                c.textBaseline = 'middle';
                c.textAlign = inX > box.x + box.w - 120 ? 'right' : 'left';
                const tx = inX + (c.textAlign === 'right' ? -(r + 5) : r + 5);
                c.lineJoin = 'round'; c.strokeStyle = 'rgba(8,12,13,0.9)'; c.lineWidth = 3;
                c.strokeText(p.label, tx, inY);
                c.fillStyle = '#ffd36b';
                c.fillText(p.label, tx, inY);
                c.textAlign = 'start'; c.lineWidth = 1;
            }
            this.sceneMarks.push({x: inX, y: inY, text: [p.story, p.label, p.place ? `${p.inside ? 'in' : 'at'} ${p.place}` : ''].filter(Boolean).join(' · ')});
        }
    }

    /** Where the last draw put buildings' icons (box pixels), and what each is: for the pointer. */
    icons: {x: number; y: number; name: string; kind: string}[] = [];

    /** Where the last draw was centred (world tiles) and at what scale, in its box: for turning a click back into tiles. */
    view: {box: {x: number; y: number; w: number; h: number}; cx: number; cy: number; scale: number} | null = null;

    /** A point in the last draw's box (its own units) as world tiles: where a click on the map means (map travel). */
    toWorld(px: number, py: number): [number, number] | null {
        const v = this.view;
        if (!v || v.scale <= 0) return null;
        return [v.cx + (px - v.box.x - v.box.w / 2) / v.scale, v.cy + (py - v.box.y - v.box.h / 2) / v.scale];
    }

    /**
     * The realm's names on the World Map (the user, 2026-11-14: "when the player zooms in close enough they should see
     * the names of the areas and towns"): larger places first and from further out, each where it doesn't cover one
     * already written; close in, the buildings' names under their icons.
     */
    private drawNames(c: CanvasRenderingContext2D, box: {x: number; y: number; w: number; h: number}, realm: RealmMap, scale: number,
        toX: (x: number) => number, toY: (y: number) => number, icons: {x: number; y: number; name: string; kind: string}[]) {
        const taken: {l: number; t: number; r: number; b: number}[] = [];
        const free = (l: number, t: number, r: number, b: number) => !taken.some(o => l < o.r && r > o.l && t < o.b && b > o.t);
        // A name stays inside the map: moved in from an edge it would cross (a place near the edge keeps its name), and
        // left out if it doesn't fit, or its place is further off than half its width (it isn't on the map).
        const inside = (x: number, y: number, w: number, h: number): number | null => {
            const pad = 6, half = w / 2;
            if (w + 2 * pad > box.w || y - h / 2 < box.y + 2 || y + h / 2 > box.y + box.h - 2) return null;
            if (x < box.x - half || x > box.x + box.w + half) return null;
            return Math.max(box.x + pad + half, Math.min(box.x + box.w - pad - half, x));
        };
        c.save();
        c.textAlign = 'center';
        c.textBaseline = 'middle';
        c.lineJoin = 'round';
        const styles = [
            {font: "600 15px 'RATW Sans', sans-serif", fill: '#e7dcc4', spacing: true},
            {font: "600 13px 'RATW Sans', sans-serif", fill: '#d9b67b', spacing: false},
            {font: "12px 'RATW Sans', sans-serif", fill: '#c4cfc6', spacing: false},
            {font: "11px 'RATW Sans', sans-serif", fill: '#e7dcc4', spacing: false},
        ];
        for (const l of labelsAt(realm, scale)) {
            const style = styles[Math.min(3, l.rank)];
            const words = style.spacing ? l.name.toUpperCase() : l.name;
            c.font = style.font;
            const w = c.measureText(words).width + (style.spacing ? words.length * 1.5 : 0), h = l.rank === 0 ? 18 : 15;
            const y = toY(l.y), x = inside(toX(l.x), y, w, h);
            if (x === null || !free(x - w / 2 - 4, y - h / 2, x + w / 2 + 4, y + h / 2)) continue;
            taken.push({l: x - w / 2 - 4, t: y - h / 2, r: x + w / 2 + 4, b: y + h / 2});
            if ('letterSpacing' in c) (c as unknown as {letterSpacing: string}).letterSpacing = style.spacing ? '1.5px' : '0px';
            c.strokeStyle = 'rgba(8,12,13,0.85)';
            c.lineWidth = 3.5;
            c.strokeText(words, x, y);
            c.fillStyle = style.fill;
            c.fillText(words, x, y);
        }
        if ('letterSpacing' in c) (c as unknown as {letterSpacing: string}).letterSpacing = '0px';
        if (scale >= 4) {
            c.font = "11px 'RATW Sans', sans-serif";
            for (const i of icons) {
                if (i.kind === 'house') continue;
                const w = c.measureText(i.name).width, y = i.y + 14, x = inside(i.x, y, w, 14);
                if (x === null || !free(x - w / 2 - 2, y - 7, x + w / 2 + 2, y + 7)) continue;
                taken.push({l: x - w / 2 - 2, t: y - 7, r: x + w / 2 + 2, b: y + 7});
                c.strokeStyle = 'rgba(8,12,13,0.85)';
                c.lineWidth = 3;
                c.strokeText(i.name, x, y);
                c.fillStyle = '#e7dcc4';
                c.fillText(i.name, x, y);
            }
        }
        c.restore();
    }
}

/** The way a journey to a spot on the map takes (the snapshot's travel line, World::travelToPoint), in world tiles. */
export function travelLine(snapshot: Json | null): {line: [number, number][]; goal: [number, number] | null} {
    const travel = obj(snapshot, 'travel');
    const flat = arr(travel, 'line'), goal = arr(travel, 'goal');
    if (!travel || !(bool(travel, 'active') || bool(travel, 'paused'))) return {line: [], goal: null};
    const line: [number, number][] = [];
    for (let i = 0; i + 1 < flat.length && line.length < 400; i += 2) {
        const x = Number(flat[i]), y = Number(flat[i + 1]);
        if (Number.isFinite(x) && Number.isFinite(y)) line.push([x, y]);
    }
    const gx = Number(goal[0]), gy = Number(goal[1]);
    return {line, goal: goal.length === 2 && Number.isFinite(gx) && Number.isFinite(gy) ? [gx, gy] : null};
}

/**
 * The way to a spot clicked on the map, drawn over the ground on both maps: from the wolf, a dashed amber line along the
 * way it will go, and a ring with a cross where it ends.
 */
function drawTravelLine(c: CanvasRenderingContext2D, s: GameState, toX: (x: number) => number, toY: (y: number) => number, large: boolean) {
    const {line, goal} = travelLine(s.snapshot);
    if (!goal) return;
    const here = wolfInWorld(s);
    const points: [number, number][] = [[toX(here[0]), toY(here[1])], ...line.slice(1).map(([x, y]) => [toX(x), toY(y)] as [number, number])];
    c.save();
    c.lineJoin = 'round';
    c.lineCap = 'round';
    for (const [width, colour, dash] of [[large ? 5 : 4, 'rgba(11,18,19,0.8)', [] as number[]], [large ? 2.5 : 2, 'rgba(255,214,140,0.95)', [6, 4]]] as const) {
        c.lineWidth = width;
        c.strokeStyle = colour;
        c.setLineDash([...dash]);
        c.beginPath();
        points.forEach(([x, y], i) => (i ? c.lineTo(x, y) : c.moveTo(x, y)));
        c.stroke();
    }
    c.setLineDash([]);
    const x = toX(goal[0]), y = toY(goal[1]), r = large ? 7 : 5;
    c.lineWidth = 3;
    c.strokeStyle = 'rgba(11,18,19,0.85)';
    c.beginPath(); c.arc(x, y, r, 0, Math.PI * 2); c.stroke();
    c.lineWidth = 1.5;
    c.strokeStyle = '#ffd68c';
    c.beginPath(); c.arc(x, y, r, 0, Math.PI * 2); c.stroke();
    c.beginPath(); c.moveTo(x - r * 0.6, y - r * 0.6); c.lineTo(x + r * 0.6, y + r * 0.6);
    c.moveTo(x + r * 0.6, y - r * 0.6); c.lineTo(x - r * 0.6, y + r * 0.6); c.stroke();
    c.restore();
}

function hashText(s: string): number {
    let h = 2166136261;
    for (let i = 0; i < s.length; ++i) h = Math.imul(h ^ s.charCodeAt(i), 16777619);
    return h >>> 0;
}
