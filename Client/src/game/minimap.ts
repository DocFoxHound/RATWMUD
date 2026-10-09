// The map of the country around (Docs/Design/29-client-polish.md, phase 8): every known place drawn where it truly is
// and as large as it truly is, one pixel a tile in its ground's colour, north up, centred on the wolf. Used twice: the
// minimap in the side panel and the full World Map (M). Each place's picture is made once per change of what the wolf
// remembers of it, and kept.
import {arr, bool, isObject, num, obj, objects, str, type Json} from './json.ts';
import {terrainInfo, type Surface, type SurfaceFactory} from './terrainLayer.ts';
import type {GameState} from './state.ts';

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

export class MapRenderer {
    private pictures = new Map<string, {key: string; surface: Surface}>();
    private current: {rows: unknown; visibility: unknown; surface: Surface | null; at: number} = {rows: null, visibility: null, surface: null, at: -1};
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

    /** The wolf's own place, from its rows: what it sees bright, what it remembers dim. Remade at most twice a second. */
    private currentPicture(s: GameState, clock: number): Surface | null {
        const k = this.current;
        if (k.surface && k.rows === s.tileRows && (k.visibility === s.visibilityRows || clock - k.at < 0.5)) return k.surface;
        const w = s.cellWidth, h = s.cellHeight;
        if (!k.surface || k.surface.canvas.width !== w || k.surface.canvas.height !== h) k.surface = this.surfaces(w, h);
        if (!k.surface) return null;
        const image = k.surface.ctx.createImageData(w, h);
        for (let y = 0; y < h; ++y) {
            const row = s.tileRows[y] ?? '', seen = s.visibilityRows[y] ?? '';
            for (let x = 0; x < w; ++x) {
                const v = seen[x] ?? '2';
                if (v === '0') continue;
                const c = colourOf(row[x] ?? ' ');
                if (!c) continue;
                const f = v === '2' ? 1 : 0.55, i = (y * w + x) * 4;
                image.data[i] = Math.round(c[0] * f);
                image.data[i + 1] = Math.round(c[1] * f);
                image.data[i + 2] = Math.round(c[2] * f);
                image.data[i + 3] = 255;
            }
        }
        k.surface.ctx.putImageData(image, 0, 0);
        k.rows = s.tileRows;
        k.visibility = s.visibilityRows;
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
        const cx = (here?.x ?? num(cell, 'x')) + (me?.x ?? s.cellWidth / 2) + pan[0];
        const cy = (here?.y ?? num(cell, 'y')) + (me?.y ?? s.cellHeight / 2) + pan[1];
        const toX = (wx: number) => box.x + box.w / 2 + (wx - cx) * scale;
        const toY = (wy: number) => box.y + box.h / 2 + (wy - cy) * scale;
        c.save();
        c.beginPath();
        c.rect(box.x, box.y, box.w, box.h);
        c.clip();
        c.fillStyle = '#0b1213';
        c.fillRect(box.x, box.y, box.w, box.h);
        c.imageSmoothingEnabled = scale < 1;
        // Remembered places first, then the wolf's own on top.
        for (const p of places) {
            const x = toX(p.x), y = toY(p.y), w = p.width * scale, h = p.height * scale;
            if (x > box.x + box.w || y > box.y + box.h || x + w < box.x || y + h < box.y) continue;
            const picture = p.current ? this.currentPicture(s, s.clock) : this.picture(p);
            if (picture) c.drawImage(picture.canvas, x, y, w, h);
            else {
                c.fillStyle = p.current ? 'rgba(168,194,166,0.18)' : 'rgba(139,155,145,0.12)';
                c.fillRect(x, y, w, h);
            }
            c.strokeStyle = p.current ? 'rgba(217,182,123,0.6)' : p.visible ? 'rgba(168,194,166,0.35)' : 'rgba(139,155,145,0.18)';
            c.lineWidth = 1;
            c.strokeRect(x + 0.5, y + 0.5, w - 1, h - 1);
        }
        // Where the weather is, faintly (the field over the wolf's own place).
        const f = s.weatherField;
        if (f && here) {
            const tints: Record<string, string> = {r: '115,155,178', s: '78,98,117', n: '220,232,238', f: '190,205,202', d: '199,154,92', o: '154,163,168'};
            for (let yy = 0; yy < f.rows; ++yy)
                for (let xx = 0; xx < f.cols; ++xx) {
                    const i = yy * f.cols + xx, tint = tints[f.kinds[i]], a = Number(f.amounts[i]) / 9;
                    if (!tint || a <= 0) continue;
                    c.fillStyle = `rgba(${tint},${(a * 0.28).toFixed(3)})`;
                    c.fillRect(toX(here.x + xx * f.step - f.step / 2), toY(here.y + yy * f.step - f.step / 2), f.step * scale, f.step * scale);
                }
        }
        // Doors of the wolf's place, then the wolves it can see, then the wolf.
        const ox = here?.x ?? num(cell, 'x'), oy = here?.y ?? num(cell, 'y');
        for (const d of objects(s.snapshot, 'doors')) {
            c.fillStyle = '#d9b67b';
            c.fillRect(toX(ox + num(d, 'x')) - 1.5, toY(oy + num(d, 'y')) - 1.5, 3, 3);
        }
        for (const e of s.entities.values()) {
            if (e.self) continue;
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
        // Party mates (doc 32): always shown, even out of sight, in any place this wolf knows.
        for (const m of s.party?.members ?? []) {
            if (m.id === s.selfId || !m.online || s.entities.has(m.id)) continue;
            const place = places.find(p => p.id === m.cell);
            if (!place) continue;
            const x = toX(place.x + m.x), y = toY(place.y + m.y);
            c.fillStyle = '#d9b67b';
            c.beginPath();
            c.arc(x, y, Math.max(2, Math.min(3.5, scale)), 0, Math.PI * 2);
            c.fill();
            c.strokeStyle = '#11191b';
            c.stroke();
        }
        // Open scenes near (doc 51, §7), when the player has turned them on: a small speech mark in the scene's colour
        // with how many wolves, at their middle or at the door into the place they're in.
        this.sceneMarks = [];
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
        if (me) {
            const x = toX(ox + me.x), y = toY(oy + me.y), r = 6;
            c.fillStyle = '#d9b67b';
            c.beginPath();
            c.moveTo(x + Math.cos(me.facing) * r, y + Math.sin(me.facing) * r);
            c.lineTo(x + Math.cos(me.facing + 2.5) * r * 0.8, y + Math.sin(me.facing + 2.5) * r * 0.8);
            c.lineTo(x + Math.cos(me.facing - 2.5) * r * 0.8, y + Math.sin(me.facing - 2.5) * r * 0.8);
            c.closePath();
            c.fill();
        }
        if (labels) {
            c.font = '12px sans-serif';
            c.textBaseline = 'top';
            for (const p of places) {
                const x = toX(p.x), y = toY(p.y);
                if (p.width * scale < 60) continue;
                c.fillStyle = p.current ? '#d9b67b' : '#8b9b91';
                c.fillText(p.name, x + 6, y + 5);
            }
        }
        c.fillStyle = '#8b9b91';
        c.font = '11px monospace';
        c.textBaseline = 'top';
        c.fillText('N ^', box.x + 6, box.y + 5);
        c.restore();
    }
}

function hashText(s: string): number {
    let h = 2166136261;
    for (let i = 0; i < s.length; ++i) h = Math.imul(h ^ s.charCodeAt(i), 16777619);
    return h >>> 0;
}
