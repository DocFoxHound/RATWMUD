// Drawing the game screen: SRatwGame::OnPaint and its helpers (UI/SRatwGame.cpp), call for call, on a 1600×1000
// canvas. Reads the state; records click targets (state.hits) and where the map is (state.mapRect and friends).
import {css, hexColor, lerp, rgb, scale, transparent, withAlpha, type Color} from '../ui/color.ts';
import {contains, rect, type Painter, type Point, type Rect} from '../ui/painter.ts';
import {Amber, Blue, Ink, Line, Muted, Panel, Paper, Raised, Sage, Scent, speakingColor, font} from '../ui/theme.ts';
import {drawPortrait, type Portraits} from '../ui/portrait.ts';
import {arr, bool, boundedNum, clamp, countText, envNumber, isObject, num, obj, objects, str, wholeCount, wrapCoordinate, type Json} from './json.ts';
import {calendarLabel, dayLabel, elevationLabel, lawLabel, environmentEffectsLabel, environmentLabel, moonLabel, paceLabel, postureLabel, scentLabel,
    windLabel} from './labels.ts';
import {Size as SheetSize, type Art, type Sheets} from './weatherArt.ts';
import type {GameState, Post} from './state.ts';
import {TERRAIN} from './terrain.generated.mjs';

interface TerrainLook {
    kind: string;
    ramp: boolean;
    glyph: string;
    ascii: string;
    fg: Color;
    bg: Color;
}
const Terrain = new Map<string, TerrainLook>(TERRAIN.map(t => [t.code,
    {kind: t.kind, ramp: t.ramp, glyph: t.glyph, ascii: t.ascii, fg: hexColor(t.fg), bg: hexColor(t.bg)}]));
export const terrainInfo = (code: string) => Terrain.get(code);

interface Atmosphere {
    bounds: Rect;
    glowColor: Color;
    weatherColor: Color;
    darkness: number;
    glowStrength: number;
    weatherStrength: number;
    feather: number;
    haloRadius: number;
}
interface WeatherMark {
    x: number;
    y: number;
    alpha: number;
    size: number;
}
interface WeatherLayer {
    art: Art;
    scroll: Point;          // In the sheet's own pixels, along its own axes.
    scale: number;          // Sheet pixels to screen pixels.
    angle: number;
    tint: Color;
}

const upperFirst = (s: string) => (s ? s[0].toUpperCase() + s.slice(1) : s);

export class GamePainter {
    private s: GameState;
    private p: Painter;
    private sheets: Sheets;
    private portraits: Portraits;

    constructor(state: GameState, painter: Painter, sheets: Sheets, portraits: Portraits) {
        this.s = state;
        this.p = painter;
        this.sheets = sheets;
        this.portraits = portraits;
    }

    private button(x: number, y: number, w: number, h: number, label: string, action: string, active = false, target = '') {
        const s = this.s, p = this.p;
        const r = rect(x, y, x + w, y + h);
        const hover = contains(r, s.hover[0], s.hover[1]);
        if (active || hover) p.box(x, y, w, h, active ? rgb(0x303a2e) : Raised);
        if (active) p.box(x, y + h - 2, w, 2, Amber);
        p.text(x + 14, y + 10, label, 12, active ? Amber : Paper, false, active);
        s.hits.push({rect: r, action, target});
    }

    paint() {
        const s = this.s, p = this.p;
        s.hits = [];
        const snapshot = s.snapshot;
        const self = obj(snapshot, 'self');
        const extra = s.storyExtra;
        p.box(0, 0, 1600, 1000, Ink);
        // The restrained frame leaves the typography and spatial glyphs in the foreground.
        p.box(0, 0, 1600, 96, rgb(0x151f20));
        p.box(0, 95, 1600, 1, Line);
        p.lines([[43, 62], [51, 32], [61, 45], [72, 29], [83, 62], [69, 54], [61, 69], [53, 54], [43, 62]], Amber, 1.5);
        p.text(101, 27, 'RUNS AGAINST THE WORLD', 21, Paper, false, true);
        p.text(103, 58, 'A LIVING WORLD.  A STORY OF YOUR OWN.', 9, Muted, true);
        p.text(720, 24, calendarLabel(snapshot), 9, Sage, true);
        p.text(720, 43, environmentLabel(s.environment.hour, s.environment.phase, s.environment.weather, s.outdoors), 11, Muted);
        p.text(720, 64, moonLabel(snapshot), 9, Muted, true);
        const today = dayLabel(snapshot);
        if (today) p.text(900, 64, today.slice(0, 30), 9, Amber, true);
        this.button(1115, 28, 119, 39, 'CHARACTER', 'character');
        this.button(1242, 28, 119, 39, 'INVENTORY', 'inventory');
        this.button(1369, 28, 101, 39, 'SETTINGS', 'settings');
        p.box(1510, 44, 5, 5, snapshot ? Sage : Amber);
        p.text(1525, 38, snapshot ? 'LIVE' : '…', 10, Sage, true);
        p.box(30, 117, 525 + extra, 809, Panel);
        p.frame(30, 117, 525 + extra, 809, Line);
        p.text(54, 140, 'THE STORY', 11, Amber, true);
        this.button(331 + extra, 128, 76, 38, 'IN WORLD', 'ic', s.channel === 'ic');
        this.button(412 + extra, 128, 117, 38, 'LOCAL OOC', 'ooc', s.channel === 'ooc');
        p.box(54, 181, 477 + extra, 1, Line);
        p.text(54, 200, s.cellName, 25, Paper, false, true);
        const sceneHeight = p.paragraph(54, 241, s.sceneDescription ||
            (s.selfId ? 'No scene description has been authored yet.' : 'Connecting to the persistent world…'), 463 + extra, 14, Muted, 1.65);
        const feedTop = Math.min(388, 263 + sceneHeight);
        p.box(54, feedTop, 477 + extra, 1, Line);
        p.text(54, feedTop + 15, s.channel === 'ic' ? 'NEARBY VOICES & ACTIONS' : 'OUT OF CHARACTER · THIS CELL', 9, Muted, true);

        const feed: Array<{post: Post; lines: string[]; height: number}> = [];
        let total = 0, waiting = 0;
        for (const post of s.posts) {
            if (post.channel !== s.channel && !post.system) continue;
            if (post.revealed === 0 && !post.system) {
                ++waiting;
                continue;
            }
            const lines = p.wrap(post.text.slice(0, post.revealed), 439 + extra, 14);
            const height = 38 + lines.length * 23;
            feed.push({post, lines, height});
            total += height;
        }
        const bottom = 744, start = feedTop + 45, available = bottom - start;
        const scroll = clamp(s.transcriptScroll, 0, Math.max(0, total - available));
        let y = total > available ? bottom - total + scroll : start;
        p.clip(rect(49, start, 49 + 480 + extra, start + available), () => {
            if (!feed.length)
                p.paragraph(65, start + 29, 'The scene is yours to enter. Listen to the room, approach someone, or press Enter to begin a conversation.',
                    433 + extra, 15, Muted, 1.85);
            for (const entry of feed) {
                const color = entry.post.system ? Muted : speakingColor(entry.post.color);
                p.box(54, y + 4, 2, entry.height - 16, withAlpha(color, 0.5));
                p.text(68, y, entry.post.speaker.toUpperCase(), 10, color, true);
                let ty = y + 23;
                for (const row of entry.lines) {
                    p.text(68, ty, row, 14, color);
                    ty += 23;
                }
                y += entry.height;
            }
        });
        p.box(54, 756, 477 + extra, 1, Line);
        p.text(55, 772, s.chat ? 'WRITING  /  YOUR DRAFT IS PRIVATE' : 'NAVIGATION  /  ENTER TO WRITE', 9, s.chat ? Sage : Muted, true);
        if (waiting > 0 && extra >= 0) p.text(390 + extra, 772, `${waiting} QUEUED`, 9, Amber, true);
        this.button(54, 792, 95, 28, s.volume.toUpperCase(), 'volume');
        if (!s.failedDraft || extra >= 0)
            p.text(160, 802, s.channel === 'ic' ? '/pose  /me  /sit  /lay  /stand' : 'Visible to this cell only', 11, Muted);
        p.box(54, 822, 466 + extra, 79, Ink);
        p.frame(54, 822, 466 + extra, 79, s.chat ? Sage : Line);
        p.text(55, 909, 'SHIFT + ENTER  newline     ESC  keep draft', 9, Muted, true);
        if (s.failedDraft) this.button(343 + extra, 782, 178, 35, 'RECOVER PRIOR POST', 'recover');

        // The map, physically separate from the transcript.
        p.text(586 + extra, 130, 'YOUR SURROUNDINGS', 10, Amber, true);
        p.text(586 + extra, 154, s.cellName, 22, Paper, false, true);
        p.text(586 + extra, 184, environmentEffectsLabel(s.environment, s.outdoors, s.reducedMotion), 8, Muted, true);
        this.button(1270, 139, 113, 38, 'LOCAL MAP', 'local', !s.worldMap);
        this.button(1390, 139, 143, 38, 'WORLD MAP', 'world', s.worldMap);
        s.mapRect = rect(584 + extra, 199, 1544, 816);
        p.box(584 + extra, 199, 960 - extra, 617, rgb(0x0f1718));
        p.frame(584 + extra, 199, 960 - extra, 617, Line);
        p.clip(rect(585 + extra, 200, 585 + extra + 958 - extra, 815), () => {
            if (s.worldMap) this.drawWorld();
            else this.drawLocal();
        });
        this.drawPace();
        p.text(602 + extra, 890, 'SIGHT · map', 9, Muted);
        p.text(722 + extra, 890, s.movementHeard ? 'HEARING · unseen pawsteps' : 'HEARING · no unseen steps heard', 9, s.movementHeard ? Blue : Muted);
        p.text(942 + extra, 890, scentLabel(s.scentCues), 9, s.scentCues.length ? Scent : Muted);
        p.box(584 + extra, 906, 960 - extra, 1, Line);
        const left = 584 + extra;
        p.text(left, 923, 'ACTIONS', 9, Muted, true);
        this.button(left + 74, 912, 90, 33, 'Listen  L', 'listen');
        this.button(left + 170, 912, 70, 33, 'Look', 'look');
        this.button(left + 246, 912, 72, 33, 'Smell', 'smell');
        this.button(left + 324, 912, 62, 33, 'Wait', 'wait');
        this.button(left + 392, 912, 54, 33, 'Sit', 'sit');
        this.button(left + 452, 912, 106, 33, 'End scene', 'session_end');
        if (extra < 150) {
            p.text(1282, 912, str(self, 'name', 'Connecting'), 12, Paper, false, true);
            p.text(1282, 933, `${postureLabel(self)}  ·  ${str(self, 'state')}`.slice(0, 42), 10, Muted);
        }
        p.box(0, 951, 1600, 49, Panel);
        p.box(0, 951, 1600, 1, Line);
        p.text(38, 969, 'WASD move · CLICK path · ALT+CLICK turn · WHEEL / PgUp PgDn pace · SHIFT/CTRL+WHEEL pan · M map', 10, Muted, true);
        p.text(1185, 969, str(snapshot, 'connection', 'Connecting to the world…'), 10, Sage);

        if (s.contextTarget && !s.modal) {
            const [cx, cy] = s.contextPoint;
            const h = 57 + s.contextActions.length * 36;
            p.box(cx + 5, cy + 6, 194, h, {r: 0, g: 0, b: 0, a: 0.5});
            p.box(cx, cy, 194, h, Panel);
            p.frame(cx, cy, 194, h, withAlpha(Amber, 0.5));
            p.text(cx + 14, cy + 12, s.contextName, 13, Paper, false, true);
            s.contextActions.forEach((action, i) => {
                const ax = cx + 7, ay = cy + 45 + i * 36;
                const r = rect(ax, ay, ax + 180, ay + 33);
                if (contains(r, s.hover[0], s.hover[1])) p.box(ax, ay, 180, 33, Raised);
                p.text(ax + 10, ay + 6, `${i + 1}  ${upperFirst(action)}`, 13, Sage);
                s.hits.push({rect: r, action: 'context', target: action});
            });
        }
        if (s.clock < s.toastUntil) {
            p.box(620 + extra, 765, 880 - extra, 38, Panel);
            p.text(635 + extra, 776, s.toast.slice(0, extra > 150 ? 65 : 105), 12, Amber);
        }
        if (s.modal) this.drawModal();
    }

    // ------------------------------------------------------------------ Atmosphere and weather

    private cellBounds(): Rect {
        const s = this.s;
        return rect(s.mapOrigin[0], s.mapOrigin[1], s.mapOrigin[0] + s.cellWidth * s.tileSize, s.mapOrigin[1] + s.cellHeight * s.tileSize);
    }

    private visibleCellBounds(): Rect {
        const cell = this.cellBounds(), map = this.s.mapRect;
        const left = Math.max(cell.left, map.left + 1), top = Math.max(cell.top, map.top + 1);
        return rect(left, top, Math.max(left, Math.min(cell.right, map.right - 1)), Math.max(top, Math.min(cell.bottom, map.bottom - 1)));
    }

    private atmosphere(): Atmosphere {
        const s = this.s, e = s.environment;
        const result: Atmosphere = {bounds: this.cellBounds(), glowColor: transparent, weatherColor: transparent, darkness: 0,
            glowStrength: 0, weatherStrength: 0, feather: 0, haloRadius: 0};
        if (s.worldMap) return result;
        const shortSide = Math.min(s.cellWidth * s.tileSize, s.cellHeight * s.tileSize);
        result.feather = Math.min(shortSide * 0.23, s.tileSize * 3.6);
        result.haloRadius = Math.min(34, shortSide * 0.12);
        result.darkness = 1 - e.illumination;
        result.glowStrength = s.outdoors ? 0 : e.glowStrength;
        result.glowColor = e.lightingTone === 'warm' ? rgb(0xeaa34f) : e.lightingTone === 'cool' ? rgb(0x87b7dd) : rgb(0xd8d4bf);
        if (s.outdoors) {
            const weather: Record<string, [number, number]> = {rain: [0x739bb2, 0.17], snow: [0xc8dce5, 0.24], fog: [0xbbcfca, 0.22],
                overcast: [0x9aa3a8, 0.12], storm: [0x4e6275, 0.24], sandstorm: [0xc79a5c, 0.28]};
            const w = weather[e.weather] ?? (e.phase === 'dawn' || e.phase === 'dusk' ? [0xd59664, 0.1] as [number, number] : null);
            if (w) {
                result.weatherColor = rgb(w[0]);
                result.weatherStrength = w[1];
            }
        }
        return result;
    }

    private weatherMarks(): WeatherMark[] {
        // Falling rain and snow are sheets (weatherLayers); these are the splashes on the ground beneath them.
        const s = this.s, e = s.environment;
        const storm = e.weather === 'storm';
        const marks: WeatherMark[] = [];
        if (!s.outdoors || s.worldMap || (e.weather !== 'rain' && !storm)) return marks;
        const area = this.visibleCellBounds();
        const width = area.right - area.left, height = area.bottom - area.top;
        if (width < 80 || height < 80) return marks;
        const animation = s.reducedMotion ? 0 : s.clock;
        const count = storm ? 56 : 32;
        for (let i = 0; i < count; ++i) {
            const life = wrapCoordinate(animation * (storm ? 1.1 : 0.7) + i * 0.618, 1);
            if (life > 0.62) continue;
            marks.push({x: area.left + 24 + wrapCoordinate(i * 157.3, width - 48), y: area.top + 44 + wrapCoordinate(i * 91.7, height - 68),
                size: 1 + life * 8, alpha: (1 - life / 0.62) * 0.32});
        }
        return marks;
    }

    private weatherLayers(): WeatherLayer[] {
        const s = this.s, e = s.environment;
        const layers: WeatherLayer[] = [];
        if (!s.outdoors || s.worldMap) return layers;
        const t = s.reducedMotion ? 0 : s.clock;
        const wind: Point = [Math.cos(s.windDirection) * s.windStrength, Math.sin(s.windDirection) * s.windStrength];
        // Drift is in screen pixels a second, offset in screen pixels; a layer scrolls by them in its own pixels.
        const add = (art: Art, drift: Point, scaleBy: number, angle: number, tint: Color, offset: Point = [0, 0]) =>
            layers.push({art, scroll: [(drift[0] * t + offset[0]) / scaleBy, (drift[1] * t + offset[1]) / scaleBy], scale: scaleBy, angle, tint});
        // Sunlight: the shadows of passing clouds cross the ground; they vanish with the sun.
        if ((e.weather === 'clear' || e.weather === 'overcast') && e.daylight > 0.05) {
            const overcast = e.weather === 'overcast';
            const drift: Point = [wind[0] * 26 + 7, wind[1] * 26 + 3];
            add('cloud', drift, 2.2, 0, rgb(0x08100c, (overcast ? 0.24 : 0.16) * e.daylight));
            if (overcast) add('cloud', [drift[0] * 1.4, drift[1] * 1.4], 1.35, 0, rgb(0x0c1216, 0.14 * e.daylight));
        }
        if (e.weather === 'rain' || e.weather === 'storm') {
            const storm = e.weather === 'storm';
            // The sheets' streaks run along +Y; turn them to fall where the wind pushes.
            const angle = Math.atan2(-wind[0] * (storm ? 0.9 : 0.5), 1);
            const boost = storm ? 1.2 : 1;
            add('rain', [0, 420], 0.9, angle, rgb(0xa9c4d2, 0.26 * boost));
            add('rain', [0, 700], 1.35, angle, rgb(0xb9d0dc, 0.36 * boost));
            if (storm) add('rain', [0, 980], 1.8, angle, rgb(0xc7d9e2, 0.44));
        } else if (e.weather === 'snow') {
            const scales = [0.8, 1.1, 1.5], fall = [20, 32, 48], alphas = [0.45, 0.62, 0.8];
            // Each depth sways on its own phase, so the flakes never march in step.
            for (let i = 0; i < 3; ++i)
                add('snow', [wind[0] * 60, fall[i] + wind[1] * 40], scales[i], 0, rgb(0xe6eff2, alphas[i]), [Math.sin(t * 0.5 + i * 2.1) * 12, 0]);
        } else if (e.weather === 'fog') {
            add('mist', [wind[0] * 8 + 5, wind[1] * 8 + 1], 2.6, 0, rgb(0xc3d1cf, 0.3));
            add('mist', [wind[0] * 14 - 4, wind[1] * 14 + 2], 1.7, 0, rgb(0xc8d6d3, 0.22));
        } else if (e.weather === 'sandstorm') {
            // Dust streaks run along the sheet's X axis, so the sheet turns to face the wind.
            const speed = 60 + 220 * s.windStrength;
            add('mist', [wind[0] * 30 + 12, wind[1] * 30], 3, 0, rgb(0xb88f58, 0.28));
            add('dust', [speed, 0], 1.9, s.windDirection, rgb(0xc9a26a, 0.42));
            add('dust', [speed * 1.6, 0], 1.2, s.windDirection, rgb(0xdcb886, 0.3));
        }
        return layers;
    }

    private lightningFlash(): number {
        // Brief, dim and never in reduced motion: a storm cue, not a strobe.
        const s = this.s;
        if (!s.outdoors || s.worldMap || s.reducedMotion || s.environment.weather !== 'storm') return 0;
        const period = 7, slot = Math.floor(s.clock / period);
        const x = Math.sin(slot * 12.9898) * 43758.5453;
        const start = (x - Math.floor(x)) * (period - 1);
        const since = s.clock - slot * period - start;
        if (since < 0 || since > 0.6) return 0;
        const first = since < 0.07 ? 1 : Math.exp(-(since - 0.07) * 10);
        const second = since > 0.18 && since < 0.24 ? 0.7 : 0;
        return 0.2 * Math.max(first, second);
    }

    private drawWeatherLayer(area: Rect, layer: WeatherLayer) {
        if (layer.tint.a <= 0.001 || layer.scale <= 0) return;
        const c = this.p.ctx;
        const sheet = this.sheets.get(layer.art, css(withAlpha(layer.tint, 1)));
        const pattern = c.createPattern(sheet, 'repeat');
        if (!pattern) return;
        const shiftX = wrapCoordinate(layer.scroll[0], SheetSize) * layer.scale, shiftY = wrapCoordinate(layer.scroll[1], SheetSize) * layer.scale;
        const cos = Math.cos(layer.angle), sin = Math.sin(layer.angle);
        const cx = (area.left + area.right) / 2 + cos * shiftX - sin * shiftY;
        const cy = (area.top + area.bottom) / 2 + sin * shiftX + cos * shiftY;
        pattern.setTransform(new DOMMatrix().translateSelf(cx, cy).rotateSelf(layer.angle * 180 / Math.PI).scaleSelf(layer.scale));
        c.save();
        c.globalAlpha = Math.min(1, layer.tint.a);
        c.fillStyle = pattern;
        c.fillRect(area.left, area.top, area.right - area.left, area.bottom - area.top);
        c.restore();
    }

    private drawEnvironment(foreground: boolean) {
        const s = this.s, p = this.p, e = s.environment;
        if (s.worldMap) return;
        const map = s.mapRect;
        if (map.right - map.left <= 2 || map.bottom - map.top <= 2) return;
        // Deliberately local: exterior halos may enter empty map canvas, never roleplay text or controls.
        p.clip(rect(map.left + 1, map.top + 1, map.right - 1, map.bottom - 1), () => {
            const a = this.atmosphere();
            const b = a.bounds;
            const w = b.right - b.left, h = b.bottom - b.top;
            const darkness = a.darkness;
            if (!foreground) {
                if (a.glowStrength > 0.001) p.halo(b, a.haloRadius, withAlpha(a.glowColor, a.glowStrength * 0.42));
                if (a.weatherStrength > 0.001) p.halo(b, a.haloRadius * 0.7, withAlpha(a.weatherColor, a.weatherStrength));
                if (s.outdoors) {
                    const ground = lerp(rgb(0x1e2c22), rgb(0x0a1225), darkness);
                    const horizon = lerp(rgb(0x343629), rgb(0x152339), darkness);
                    p.gradient(b.left, b.top, w, h, horizon, ground, withAlpha(scale(ground, 0.78), 1), false);
                    if (e.phase === 'dawn' || e.phase === 'dusk')
                        p.gradient(b.left, b.top, w, h, rgb(0xd69864, 0.12), rgb(0xc0774b, 0.04), rgb(0x786992, 0.02), e.phase === 'dawn');
                    if (e.weather === 'rain') p.gradient(b.left, b.top, w, h, rgb(0x617a92, 0.08), rgb(0x34495c, 0.07), rgb(0x294859, 0.11), false);
                    else if (e.weather === 'snow') p.gradient(b.left, b.top, w, h, rgb(0xb4c3ce, 0.09), rgb(0x87a0b8, 0.07), rgb(0xb7c9ce, 0.12), false);
                    else if (e.weather === 'fog') p.box(b.left, b.top, w, h, rgb(0x9fafac, 0.11));
                    else if (e.weather === 'overcast') p.box(b.left, b.top, w, h, rgb(0x7d868c, 0.1));
                    else if (e.weather === 'storm') p.gradient(b.left, b.top, w, h, rgb(0x3c4b5c, 0.18), rgb(0x223040, 0.14), rgb(0x1b2836, 0.2), false);
                    else if (e.weather === 'sandstorm') p.gradient(b.left, b.top, w, h, rgb(0xc09a62, 0.2), rgb(0xa57b45, 0.16), rgb(0x8c6a3e, 0.22), true);
                    else if (e.phase === 'day') p.box(b.left, b.top, w, h, rgb(0xffd89a, 0.05 * e.daylight));
                } else if (darkness > 0.01) p.box(b.left, b.top, w, h, rgb(0x020610, darkness * 0.58));
                return;
            }
            // Every fade follows the true room bounds, even beyond this viewport. Atmosphere only: no terrain,
            // identities or click targets.
            if (darkness > 0.01) {
                const edge = rgb(0x02050c, darkness * 0.72);
                p.edgeFade(b, a.feather, edge, true);
                p.edgeFade(b, a.feather, edge, false);
            }
            const glow = withAlpha(a.glowColor, a.glowStrength * 0.2);
            const weatherEdge = withAlpha(a.weatherColor, a.weatherStrength);
            for (const horizontal of [true, false]) {
                p.edgeFade(b, a.feather * 0.75, glow, horizontal);
                p.edgeFade(b, a.feather, weatherEdge, horizontal);
            }
            p.clip(b, () => {
                for (const layer of this.weatherLayers()) this.drawWeatherLayer(b, layer);
                for (const m of this.weatherMarks())
                    p.lines([[m.x - m.size, m.y - 1], [m.x, m.y + m.size * 0.35], [m.x + m.size, m.y - 1]], rgb(0x9dbaca, m.alpha), 0.8);
                // In the dark, the ground beyond a wolf's own sight sinks into night around it.
                const selfView = s.entities.get(s.selfId);
                if (darkness > 0.3 && selfView) {
                    const night = rgb(0x02050c, (darkness - 0.3) / 0.7 * 0.6);
                    const radius = clamp(27 * e.sight, 4, 30) * s.tileSize;
                    const cx = s.mapOrigin[0] + selfView.x * s.tileSize, cy = s.mapOrigin[1] + selfView.y * s.tileSize;
                    const lowX = cx - radius, lowY = cy - radius, highX = cx + radius, highY = cy + radius;
                    const c = p.ctx;
                    c.save();
                    c.globalAlpha = Math.min(1, night.a);
                    c.drawImage(this.sheets.get('pool', css(withAlpha(night, 1))), lowX, lowY, highX - lowX, highY - lowY);
                    c.restore();
                    p.box(b.left, b.top, w, Math.max(0, lowY - b.top), night);
                    p.box(b.left, highY, w, Math.max(0, b.bottom - highY), night);
                    p.box(b.left, lowY, Math.max(0, lowX - b.left), highY - lowY, night);
                    p.box(highX, lowY, Math.max(0, b.right - highX), highY - lowY, night);
                }
                const flash = this.lightningFlash();
                if (flash > 0) p.box(b.left, b.top, w, h, rgb(0xdfe8f5, flash));
            });
        });
    }

    private drawScent(sx: number, sy: number) {
        // Fixed-radius compass hints, not where the scent comes from.
        const s = this.s, p = this.p;
        if (s.worldMap || !contains(s.mapRect, sx, sy)) return;
        for (const cue of s.scentCues) {
            const angle = cue.sector * Math.PI / 4;
            const alpha = (0.26 + cue.strength * 0.13) * 1.2;
            for (let ring = 0; ring < 2; ++ring) {
                const arc: Point[] = [];
                for (let step = 0; step <= 12; ++step) {
                    const theta = angle - Math.PI / 8 + step * (Math.PI / 4) / 12;
                    arc.push([sx + Math.cos(theta) * (38 + ring * 5), sy + Math.sin(theta) * (38 + ring * 5)]);
                }
                p.lines(arc, withAlpha(Scent, alpha * (ring ? 0.45 : 1)), 1.2);
            }
            const x = sx + Math.cos(angle) * 40, y = sy + Math.sin(angle) * 40;
            p.box(x - 9, y - 6, 18, 12, withAlpha(Ink, 0.8));
            p.text(x - 8, y - 8, '~~', 11, withAlpha(Scent, alpha), true);
        }
    }

    /** A character drawn turned, centred on a point (the facing arrows). */
    private turnedText(x: number, y: number, text: string, size: number, color: Color, angle: number) {
        const c = this.p.ctx;
        c.save();
        c.translate(x, y);
        c.rotate(angle);
        c.font = font(size, true);
        c.fillStyle = css(color);
        c.textAlign = 'center';
        c.textBaseline = 'middle';
        c.fillText(text, 0, 0);
        c.restore();
    }

    // ------------------------------------------------------------------ The local map

    private drawLocal() {
        const s = this.s, p = this.p, e = s.environment;
        const extra = s.storyExtra;
        s.tileSize = Math.min(28, Math.min((882 - extra) / Math.min(s.cellWidth, 32), 548 / Math.min(s.cellHeight, 24)));
        // A cell that fits is centred; a larger one follows the wolf, stopping at its edges so no empty canvas
        // shows. Shift/Ctrl + wheel look around (mapPan) until the wolf next moves.
        const viewX = 1064 + extra * 0.5, viewY = 508;
        const me = s.entities.get(s.selfId);
        const axis = (center: number, low: number, high: number, tiles: number, self: number) => {
            const span = tiles * s.tileSize;
            if (span <= high - low || !me) return center - span * 0.5;
            return clamp(center - self * s.tileSize, high - span, low);
        };
        const map = s.mapRect;
        s.mapOrigin = [axis(viewX, map.left, map.right, s.cellWidth, me?.x ?? 0) + s.mapPan[0],
            axis(viewY, map.top, map.bottom, s.cellHeight, me?.y ?? 0) + s.mapPan[1]];
        const [ox, oy] = s.mapOrigin, tile = s.tileSize;
        const cell = obj(s.snapshot, 'cell');
        this.drawEnvironment(false);
        const ground = s.selfHeight();
        // Only the tiles on screen are drawn: a large cell has tens of thousands more.
        const firstX = Math.max(0, Math.floor((map.left - ox) / tile) - 1), firstY = Math.max(0, Math.floor((map.top - oy) / tile) - 1);
        const lastX = Math.ceil((map.right - ox) / tile) + 1;
        const lastY = Math.min(s.tileRows.length - 1, Math.ceil((map.bottom - oy) / tile) + 1);
        const visibility = (x: number, y: number) => s.visibilityRows[y]?.[x] ?? '2';
        const dark = 1 - e.illumination;
        for (let y = firstY; y <= lastY; ++y) {
            const row = s.tileRows[y];
            for (let x = firstX; x <= Math.min(lastX, row.length - 1); ++x) {
                const code = row[x];
                const seen = visibility(x, y);
                if (seen === '0' || code === ' ') continue;
                const px = ox + x * tile, py = oy + y * tile;
                const known = seen === '1';
                const info = terrainInfo(code);
                let color = info ? info.fg : Sage;
                if (e.illumination < 1) {
                    color = lerp(color, withAlpha(rgb(0x8195ad), color.a), dark * 0.28);
                    color = withAlpha(scale(color, 1 - dark * 0.27), color.a);
                }
                if (known) color = withAlpha(color, 0.22);
                // Height reads relative to the wolf: ground above it is lit and warm, ground below sinks into shade,
                // and slopes facing the north-west light are brighter than those turned away.
                const rise = s.heightAt(x, y) - ground;
                if (!known) {
                    const facing = clamp((s.heightAt(x + 1, y) + s.heightAt(x, y + 1) - s.heightAt(x - 1, y) - s.heightAt(x, y - 1)) * 0.5, -2, 2);
                    const base = info ? info.bg : rgb(0x283126);
                    let floor = rise >= 0 ? lerp(base, rgb(0x6b6a4a), Math.min(rise * 0.12, 0.4))
                        : lerp(base, rgb(0x0b1216), Math.min(-rise * 0.14, 0.5));
                    floor = withAlpha(scale(floor, 1 + facing * 0.2), 0.27 + Math.min(Math.abs(rise) * 0.03, 0.12));
                    p.box(px, py, tile - 1, tile - 1, floor);
                }
                const shape = !info ? code : s.plainGlyphs ? info.ascii : info.glyph;
                // Block and shade characters fill the whole tile, so walls and cliffs read as one mass.
                const fill = s.plainGlyphs ? 0 : shape === '█' ? 1 : shape === '▓' ? 0.75 : shape === '▒' ? 0.5 : shape === '░' ? 0.28 : 0;
                const lift = clamp(rise, -2, 2);
                if (fill > 0) p.box(px, py - lift, tile, tile, withAlpha(color, color.a * fill));
                else {
                    const small = shape === '.' || shape === '·' || shape === '∙' || shape === ',';
                    const size = small ? 12 : 15;
                    const [w, h] = p.measure(shape, size, true);
                    p.text(px + (tile - w) * 0.5, py + (tile - h) * 0.5 - lift, shape, size, color, true);
                }
            }
        }
        // Where the ground changes height, the edge is drawn by how it can be crossed: a faint contour for a half
        // step, a warm line where a slope or stairs make a full step walkable, a heavy rim with a cast shadow for a
        // ledge or cliff that can't be walked.
        const seenOpen = (x: number, y: number) => {
            const code = s.tileRows[y]?.[x];
            const info = code ? terrainInfo(code) : undefined;
            return !!info && info.kind !== 'wall' && (s.visibilityRows[y]?.[x] ?? '0') !== '0';
        };
        for (let y = firstY; y <= lastY; ++y)
            for (let x = firstX; x <= Math.min(lastX, s.tileRows[y].length - 1); ++x)
                for (const [dx, dy] of [[1, 0], [0, 1]]) {
                    const nx = x + dx, ny = y + dy;
                    if (!seenOpen(x, y) || !seenOpen(nx, ny)) continue;
                    const infoA = terrainInfo(s.tileRows[y][x])!, infoB = terrainInfo(s.tileRows[ny][nx])!;
                    const ha = s.heightAt(x, y), hb = s.heightAt(nx, ny), drop = Math.abs(ha - hb);
                    const cliff = infoA.kind === 'cliff' || infoB.kind === 'cliff';
                    if (drop < 0.01 && !cliff) continue;
                    const remembered = s.visibilityRows[y][x] === '1' || s.visibilityRows[ny][nx] === '1';
                    const fade = remembered ? 0.45 : 1;
                    const ramp = infoA.ramp || infoB.ramp;
                    const cx = ox + nx * tile, cy = oy + ny * tile;
                    const end: Point = dx ? [cx, cy + tile] : [cx + tile, cy];
                    if (drop <= 0.5 && !cliff) p.lines([[cx, cy], end], rgb(0xc9bf9a, 0.16 * fade), 1);
                    else if (drop <= 1.01 && ramp && !cliff) p.lines([[cx, cy], end], rgb(0xd8b877, 0.34 * fade), 1.3);
                    else {
                        // The shadow falls on the lower side; a level cliff edge shades its open side.
                        const lowAfter = hb < ha || (drop < 0.01 && infoA.kind === 'cliff');
                        const band = Math.min(6, tile * 0.28);
                        const shadowX = lowAfter ? cx : cx - dx * band, shadowY = lowAfter ? cy : cy - dy * band;
                        p.box(shadowX, shadowY, dx ? band : tile, dx ? tile : band, rgb(0x04070a, 0.4 * fade));
                        p.lines([[cx, cy], end], rgb(0xe9d2a0, 0.55 * fade), 2.2);
                    }
                }
        // Actions come only from doors and wolves the server shows.
        for (const door of objects(s.snapshot, 'doors')) {
            const x = ox + num(door, 'x') * tile, y = oy + num(door, 'y') * tile;
            p.box(x - 8, y - 10, 17, 22, Ink);
            p.text(x - 7, y - 10, bool(door, 'open') ? '/' : '+', 17, Amber, true);
            p.frame(x - 11, y - 13, 23, 27, withAlpha(Amber, 0.25));
            s.hits.push({rect: rect(x - 15, y - 16, x + 15, y + 16), action: 'target', target: str(door, 'id')});
        }
        const resource = s.visibleResource();
        if (resource) {
            const x = ox + num(resource, 'x') * tile, y = oy + num(resource, 'y') * tile;
            p.text(x - 8, y - 12, '"', 19, wholeCount(resource, 'remaining') > 0 ? Sage : withAlpha(Muted, 0.5), true);
            s.hits.push({rect: rect(x - 14, y - 14, x + 14, y + 14), action: 'target', target: 'herb_patch'});
        }
        const me2 = s.entities.get(s.selfId);
        if (me2) this.drawScent(ox + me2.x * tile, oy + me2.y * tile);
        for (const view of s.entities.values()) {
            const x = ox + view.x * tile, y = oy + view.y * tile;
            const color = view.self ? Amber : view.kind === 'npc' ? Sage : Blue;
            if (view.self) {
                p.frame(x - 17, y - 17, 34, 34, withAlpha(Amber, 0.22));
                p.box(x - 9, y - 10, 18, 21, Ink);
            }
            const wolfFont = clamp(Math.round(tile * 0.55), 10, 13);
            const [ww, wh] = p.measure('W', wolfFont, true);
            p.text(x - ww * 0.5, y - wh * 0.5, 'W', wolfFont, color, true);
            const reach = Math.min(13, tile * 0.55);
            this.turnedText(x + Math.cos(view.facing) * reach, y + Math.sin(view.facing) * reach, '>', 10, color, view.facing);
            if (view.self && s.facingPreview && s.canFaceAt(s.hover))
                this.turnedText(x + Math.cos(s.previewFacing) * reach, y + Math.sin(s.previewFacing) * reach, '>', 10, withAlpha(color, 0.32),
                    s.previewFacing);
            s.hits.push({rect: rect(x - 14, y - 14, x + 14, y + 14), action: 'target', target: view.id});
            if (view.typing || s.clock - view.spokenAt < 4) {
                const alpha = view.typing ? 1 : clamp((4 - (s.clock - view.spokenAt)) / 1.2, 0, 1);
                const bx = x - 16, by = y - 39;
                p.box(bx, by, 32, 20, withAlpha(Panel, alpha));
                p.frame(bx, by, 32, 20, withAlpha(speakingColor(view.color), alpha * 0.6));
                p.text(bx + 6, by - 1, view.typing ? '...' : "''", 13, withAlpha(speakingColor(view.color), alpha), true);
            }
            if (Math.hypot(x - s.hover[0], y - s.hover[1]) < 20) {
                const label = view.self ? `${view.name} · you` : view.name;
                const [lw, lh] = p.measure(label, 11);
                p.box(x - lw * 0.5 - 6, y + 21, lw + 12, lh + 7, Panel);
                p.text(x - lw * 0.5, y + 23, label, 11, color);
            }
        }
        this.drawEnvironment(true);
        p.text(606 + extra, 218, 'N ^', 10, Muted, true);
        p.text(680 + extra, 218, windLabel(s.outdoors, s.windStrength, s.windDirection, s.windVariable), 9, Muted);
        p.text(606 + extra, 236, elevationLabel(s.selfHeight()), 8, Muted, true);
        p.text(1225, 218, 'W YOU', 8, Amber, true);
        p.text(1305, 218, 'W PLAYER', 8, Blue, true);
        p.text(1410, 218, 'W RESIDENT', 8, Sage, true);
        const travel = obj(s.snapshot, 'travel');
        if (bool(travel, 'active') || bool(travel, 'paused')) {
            p.box(606 + extra, 738, 914 - extra, 37, Panel);
            p.text(616 + extra, 750, `TRAVEL · ${str(travel, 'status', 'Following your route')}`.slice(0, extra > 150 ? 55 : 89), 10,
                bool(travel, 'paused') ? Amber : Sage);
            p.box(1420, 742, 92, 28, Raised);
            p.text(1429, 750, 'STOP · ESC', 9, Paper, true);
            s.hits.push({rect: rect(1420, 742, 1512, 770), action: 'cancel_travel', target: ''});
        }
        if (s.facingPreview && s.canFaceAt(s.hover)) p.text(606 + extra, 782, 'ALT · CLICK TO TURN', 9, Amber, true);
        else p.text(606 + extra, 782, postureLabel(obj(s.snapshot, 'self')), 9, Sage);
        const law = lawLabel(obj(s.snapshot, 'self'));
        if (law) p.text(820 + extra, 782, law.slice(0, Math.max(10, Math.floor((580 - extra) / 6.5))), 9, rgb(0xe1aba2), true);
        p.text(1415, 782, `LOCAL  /  Z ${Math.trunc(num(cell, 'z'))}`, 9, Muted, true);
    }

    // ------------------------------------------------------------------ Pace and stamina

    private drawPace() {
        const s = this.s, p = this.p;
        const self = obj(s.snapshot, 'self');
        const left = 602 + s.storyExtra, width = 924 - s.storyExtra, paceWidth = width * 0.51;
        const pace = s.displayPace(), effective = Math.trunc(boundedNum(self, 'effectivePace', 0, 10));
        const exhausted = bool(self, 'exhausted');
        const stamina = boundedNum(self, 'stamina', 0, 100, 100);
        const rate = boundedNum(self, 'staminaRate', -100, 100);
        const paceColor = exhausted ? rgb(0xe1aba2) : pace >= 9 ? Amber : Sage;
        p.text(left, 827, `PACE · ${paceLabel(pace)} ${pace}/10`, 10, paceColor, true);
        const limiter = s.requestedPace >= 0 && s.clock - s.lastPaceRequest <= 1.5 ? 'REQUESTING…'
            : exhausted ? 'EXHAUSTED · walking' : effective < pace ? 'POSTURE-LIMITED' : 'wheel / PgUp PgDn';
        p.text(left + paceWidth - 151, 829, limiter, 8, exhausted ? paceColor : Muted);
        const step = paceWidth / 11;
        for (let i = 0; i <= 10; ++i) {
            const x = left + i * step;
            p.box(x, 851, step - 3, 10, i <= pace ? withAlpha(i >= 9 ? Amber : Sage, i === pace ? 1 : 0.45) : Raised);
            if (i === pace) p.frame(x, 849, step - 3, 14, paceColor);
            s.hits.push({rect: rect(x, 846, x + step - 3, 879), action: 'pace', target: String(i)});
        }
        p.text(left, 867, 'WALK', 8, Muted, true);
        p.text(left + step * 3, 867, 'TROT', 8, Muted, true);
        p.text(left + step * 6, 867, 'RUN', 8, Muted, true);
        p.text(left + step * 9, 867, 'SPRINT', 8, Amber, true);
        const staminaX = left + paceWidth + 23, staminaWidth = width - paceWidth - 23;
        const energy = exhausted ? rgb(0xe1aba2) : rate < -0.01 ? Amber : Sage;
        p.text(staminaX, 827, `STAMINA  ${stamina.toFixed(0)}%`, 10, energy, true);
        p.text(staminaX + staminaWidth - 121, 829, `DEX ${envNumber(self, 'effectiveDexterity', 0, 100, envNumber(self, 'dexterity', 0, 100, 0)).toFixed(0)}` +
            ` · TOP ${boundedNum(self, 'topSpeed', 0, 100).toFixed(1)} t/s`, 8, Muted);
        p.box(staminaX, 851, staminaWidth, 10, Raised);
        p.box(staminaX, 851, staminaWidth * stamina / 100, 10, energy);
        const rateText = rate < -0.01 ? `DRAINING ${(-rate).toFixed(1)}/s · ease pace for distance`
            : rate > 0.01 ? (stamina >= 99.95 ? 'FULL · recovery is always active' : `RECOVERING +${rate.toFixed(1)}/s · ongoing recovery`)
                : 'STEADY · sustainable travel';
        p.text(staminaX, 867, rateText, 8, energy);
    }

    // ------------------------------------------------------------------ The world map and the travel atlas

    private drawWorld() {
        const s = this.s, p = this.p, extra = s.storyExtra;
        const tab = (x: number, label: string, action: string, active: boolean) => {
            p.box(x, 216, 130, 30, active ? Raised : Panel);
            p.frame(x, 216, 130, 30, active ? Sage : Line);
            p.text(x + 12, 224, label, 9, active ? Sage : Muted, true);
            s.hits.push({rect: rect(x, 216, x + 130, 246), action, target: ''});
        };
        tab(1250, 'NEARBY', 'nearby', !s.travelAtlas);
        tab(1390, 'KNOWN ROUTES', 'atlas', s.travelAtlas);
        if (s.travelAtlas) {
            this.drawTravelAtlas();
            return;
        }
        const iso = bool(s.snapshot, 'isometric') && !s.flatWorld;
        p.text(613 + extra, 225, iso ? 'VISIBLE VERTICAL CONNECTION' : 'NEIGHBORHOOD', 10, Sage, true);
        p.text(613 + extra, 251, iso ? 'The visible upper cell lifts into view.' : 'What you can see. What you remember.', 13, Muted);
        const cells = objects(s.snapshot, 'worldMap');
        const current = cells.find(c => bool(c, 'current'));
        const [curX, curY, curZ] = current ? [num(current, 'x'), num(current, 'y'), num(current, 'z')] : [0, 0, 0];
        for (const c of cells) {
            const isCurrent = bool(c, 'current'), visible = bool(c, 'visible');
            const knowledge = str(c, 'knowledge');
            if (!visible && !isCurrent && knowledge === 'unknown') continue;
            const x = num(c, 'x') - curX, y = num(c, 'y') - curY, z = num(c, 'z') - curZ;
            const rx = clamp(x / Math.max(1, s.cellWidth), -1, 1), ry = clamp(y / Math.max(1, s.cellHeight), -1, 1), rz = clamp(z, -1, 1);
            let px = 961 + extra * 0.5 + rx * Math.min(235, (960 - extra - 230) * 0.5), py = 435 + ry * 157 - rz * 120;
            if (iso) {
                px += -ry * 52 - rz * 24;
                py += rx * 37;
            }
            const w = 206, h = 145;
            const color = isCurrent ? Amber : visible ? Sage : withAlpha(Muted, 0.5);
            p.box(px, py, w, h, isCurrent ? rgb(0x242d23) : withAlpha(Panel, visible ? 1 : 0.45));
            p.frame(px, py, w, h, color);
            if (iso) {
                p.lines([[px, py], [px + 32, py - 19], [px + w + 32, py - 19], [px + w, py]], withAlpha(color, 0.5));
                p.lines([[px + w, py], [px + w + 32, py - 19], [px + w + 32, py + h - 19], [px + w, py + h]], withAlpha(color, 0.5));
            }
            if (isCurrent || visible || knowledge === 'visited') {
                const glyphs = str(c, 'glyphs');
                const width = Math.max(1, Math.trunc(num(c, 'width'))), height = Math.max(1, Math.trunc(num(c, 'height')));
                for (let ty = 0; ty < 5; ++ty)
                    for (let tx = 0; tx < 17; ++tx) {
                        const sx = Math.round(tx * (width - 1) / 16), sy = Math.round(ty * (height - 1) / 4);
                        const ch = glyphs[sy * width + sx];
                        if (ch === undefined || ch === ' ' || ch === '\n') continue;
                        const info = terrainInfo(ch);
                        const shape = !info ? ch : s.plainGlyphs ? info.ascii : info.glyph;
                        p.text(px + 10 + tx * 11, py + 34 + ty * 14, shape, 9, withAlpha(color, visible || isCurrent ? 0.4 : 0.17), true);
                    }
            }
            p.text(px + 11, py + 11, str(c, 'name'), 12, color, false, true);
            p.text(px + 11, py + 120, isCurrent ? 'YOU ARE HERE' : visible ? 'IN SIGHT' : knowledge === 'visited' ? 'VISITED · MEMORY' : 'GLIMPSED · OUTLINE',
                8, color, true);
            if (z !== 0) p.text(px + 157, py + 120, z > 0 ? 'ABOVE' : 'BELOW', 8, color, true);
            if (isCurrent) p.text(px + 95, py + 64, 'W>', 15, Amber, true);
        }
        p.text(614 + extra, 742, 'Memories persist. Unseen changes and residents stay hidden.', 12, Muted);
        p.text(614 + extra, 771, 'BRIGHT  currently seen      DIM  remembered      ABSENT  unexplored', 9, Muted, true);
    }

    private drawTravelAtlas() {
        const s = this.s, p = this.p, extra = s.storyExtra;
        p.text(613 + extra, 225, 'YOUR TRAVEL ATLAS', 10, Sage, true);
        p.text(613 + extra, 251, 'Choose a place you have visited. Travel happens on foot, cell by cell.', 12, Muted);
        // Only remembered geography: never live neighbourhood data.
        const cells: Json[] = [];
        const included = new Set<string>();
        for (const c of arr(s.snapshot, 'travelMap')) {
            if (!isObject(c)) continue;
            const id = str(c, 'id');
            if (str(c, 'knowledge') !== 'visited' || !id || included.has(id)) continue;
            if (![num(c, 'x', NaN), num(c, 'y', NaN), num(c, 'z', NaN)].every(Number.isFinite)) continue;
            cells.push(c);
            included.add(id);
            if (cells.length >= 256) break;
        }
        cells.sort((a, b) => (str(a, 'name') < str(b, 'name') ? -1 : str(a, 'name') > str(b, 'name') ? 1 : 0));
        const left = 613 + extra, listX = 1302, mapWidth = listX - left - 25;
        const top = 330, mapHeight = 355;
        p.box(left, top, mapWidth, mapHeight, rgb(0x121b1c));
        p.frame(left, top, mapWidth, mapHeight, Line);
        if (!cells.length) {
            p.paragraph(left + 22, top + 35, 'No visited places have arrived yet. Places seen only in the distance cannot be travel destinations.',
                mapWidth - 44, 13, Muted);
            return;
        }
        let minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
        for (const c of cells) {
            const x = boundedNum(c, 'x', -1e6, 1e6), y = boundedNum(c, 'y', -1e6, 1e6);
            minX = Math.min(minX, x);
            minY = Math.min(minY, y);
            maxX = Math.max(maxX, x + boundedNum(c, 'width', 1, 256, 32));
            maxY = Math.max(maxY, y + boundedNum(c, 'height', 1, 256, 24));
        }
        const k = Math.min((mapWidth - 44) / Math.max(1, maxX - minX), 311 / Math.max(1, maxY - minY));
        const offX = left + (mapWidth - (maxX - minX) * k) * 0.5, offY = top + (mapHeight - (maxY - minY) * k) * 0.5;
        const centers = new Map<string, Point>();
        const travel = obj(s.snapshot, 'travel');
        const destination = str(travel, 'destination');
        let destinationName = 'Known destination';
        const labels: Array<{text: string; x: number; y: number; bounds: Rect; color: Color; places: number}> = [];
        for (const c of cells) {
            const id = str(c, 'id');
            const w = boundedNum(c, 'width', 1, 256, 32), h = boundedNum(c, 'height', 1, 256, 24);
            const cx = offX + (boundedNum(c, 'x', -1e6, 1e6) - minX + w * 0.5) * k, cy = offY + (boundedNum(c, 'y', -1e6, 1e6) - minY + h * 0.5) * k;
            const sw = Math.max(8, w * k - 4), sh = Math.max(8, h * k - 4);
            const x = cx - sw * 0.5, y = cy - sh * 0.5;
            const isCurrent = id === s.cellId, selected = id === destination;
            const color = isCurrent ? Amber : selected ? Sage : withAlpha(Muted, 0.55);
            p.box(x, y, sw, sh, isCurrent ? rgb(0x2e3325) : rgb(0x1c2927));
            p.frame(x, y, sw, sh, color);
            if (sw > 70 && sh > 35) {
                const name = str(c, 'name').slice(0, Math.max(6, Math.trunc(sw / 7) - 2));
                const [lw] = p.measure(name, 9);
                const bounds = rect(x + 7, y + 7, x + 7 + lw, y + 22);
                const overlap = labels.find(l => bounds.left < l.bounds.right && bounds.right > l.bounds.left && bounds.top < l.bounds.bottom &&
                    bounds.bottom > l.bounds.top);
                if (overlap) {
                    overlap.text = `${++overlap.places} places · use list`;
                    overlap.color = Muted;
                    overlap.bounds.right = Math.max(overlap.bounds.right, overlap.x + p.measure(overlap.text, 9)[0]);
                } else labels.push({text: name, x: x + 7, y: y + 7, bounds, color, places: 1});
            }
            if (isCurrent) p.text(cx - 8, cy - 5, 'W>', 12, Amber, true);
            if (selected) destinationName = str(c, 'name', destinationName);
            centers.set(id, [cx, cy]);
            if (!isCurrent) s.hits.push({rect: rect(x, y, x + sw, y + sh), action: 'travel', target: id});
        }
        // Off-map interiors may share an origin with another cell: colliding labels become one hint, drawn after
        // every rectangle so none hides it.
        for (const l of labels) p.text(l.x, l.y, l.text, 9, l.color);
        let previous: Point | null = null;
        for (const step of arr(travel, 'route')) {
            const center = typeof step === 'string' ? centers.get(step) : undefined;
            if (!center) {
                previous = null;
                continue;
            }
            if (previous) p.lines([previous, center], withAlpha(Sage, 0.65), 2);
            previous = center;
        }
        const perPage = 8, lastPage = Math.floor((cells.length - 1) / perPage), page = clamp(s.travelPage, 0, lastPage);
        for (let i = 0; i < perPage && page * perPage + i < cells.length; ++i) {
            const c = cells[page * perPage + i];
            const id = str(c, 'id');
            const isCurrent = id === s.cellId, selected = id === destination;
            const y = 331 + i * 43;
            p.box(listX, y, 216, 39, selected ? Raised : Panel);
            p.text(listX + 9, y + 5, str(c, 'name').slice(0, 26), 10, isCurrent ? Amber : Paper);
            p.text(listX + 9, y + 23, `${isCurrent ? 'YOU ARE HERE' : 'VISITED · TRAVEL >'} · Z ${boundedNum(c, 'z', -1e6, 1e6).toFixed(0)}`, 8,
                isCurrent ? Amber : Muted, true);
            if (!isCurrent) s.hits.push({rect: rect(listX, y, listX + 216, y + 39), action: 'travel', target: id});
        }
        p.text(left, 697, 'DIM = remembered geography · no live remote activity', 9, Muted);
        p.text(listX + 55, 695, `${page + 1} / ${lastPage + 1}`, 9, Muted, true);
        for (const direction of [-1, 1]) {
            const x = listX + (direction < 0 ? 0 : 171);
            p.text(x + 9, 695, direction < 0 ? '<' : '>', 11, Sage, true);
            if ((direction < 0 && page > 0) || (direction > 0 && page < lastPage))
                s.hits.push({rect: rect(x, 687, x + 40, 717), action: 'travel_page', target: String(direction)});
        }
        const active = bool(travel, 'active'), paused = bool(travel, 'paused');
        if (active || paused) {
            p.text(left, 734, `TO ${destinationName}`.slice(0, extra > 150 ? 55 : 85), 12, paused ? Amber : Sage, false, true);
            p.text(left, 759, str(travel, 'status', 'Following route').slice(0, extra > 150 ? 63 : 93), 11, Muted);
            p.box(1390, 738, 128, 35, Raised);
            p.text(1401, 749, 'STOP · ESC', 10, Paper, true);
            s.hits.push({rect: rect(1390, 738, 1518, 773), action: 'cancel_travel', target: ''});
        } else p.text(left, 745, 'No teleporting. Closed doors require an explicit open action.', 11, Muted);
        p.text(left, 785, 'WASD or a local click takes over · choose a comfortable pace below', 9, Muted);
    }

    // ------------------------------------------------------------------ Sheets and dialogs

    private drawModal() {
        const s = this.s, p = this.p;
        p.box(0, 97, 1600, 854, rgb(0x080e10, 0.94));
        p.box(298, 164, 1024, 697, {r: 0, g: 0, b: 0, a: 0.45});
        p.box(288, 151, 1024, 697, Panel);
        p.frame(288, 151, 1024, 697, Line);
        const button = (x: number, y: number, w: number, h: number, label: string, action: string, target = '', active = false) => {
            const r = rect(x, y, x + w, y + h);
            p.box(x, y, w, h, active ? rgb(0x35402d) : Raised);
            if (contains(r, s.hover[0], s.hover[1])) p.frame(x, y, w, h, Sage);
            p.text(x + 12, y + 11, label, 12, active ? Amber : Paper);
            s.hits.push({rect: r, action, target});
        };
        button(1240, 173, 47, 40, '×', 'close');
        const self = obj(s.snapshot, 'self');
        const m = s.modal;
        if (m === 'character') {
            p.text(324, 185, 'CHARACTER / APPEARANCE', 10, Amber, true);
            p.text(324, 221, str(self, 'name', 'Your character'), 35, Paper, false, true);
            p.text(325, 271, `AGE ${wholeCount(self, 'age', 18, 10000)}  ·  NORMAL  ·  A STORY STILL UNFOLDING`, 10, Muted, true);
            const dex = envNumber(self, 'dexterity', 0, 100, 50);
            p.text(325, 293, `STRENGTH ${envNumber(self, 'strength', 0, 100, 50).toFixed(0)}   DEXTERITY ${dex.toFixed(0)} ` +
                `(${envNumber(self, 'effectiveDexterity', 0, 100, dex).toFixed(1)} effective)   WISDOM ${envNumber(self, 'wisdom', 0, 100, 50).toFixed(0)}`,
                10, Sage, true);
            p.box(324, 318, 509, 355, Ink);
            p.frame(324, 318, 509, 355, Line);
            drawPortrait(p.ctx, this.portraits, s.portraitAppearance(), s.portraitAge(), 324, 318, 509, 300);
            p.text(342, 643, num(self, 'shoulderHeightCm') > 0
                ? `${str(obj(self, 'appearance'), 'stature', 'average').toUpperCase()} STATURE · ${num(self, 'shoulderHeightCm').toFixed(0)} CM AT SHOULDER · SAVED PROFILE`
                : 'STATIC PROFILE · YOUR SAVED APPEARANCE', 9, Muted, true);
            p.text(866, 328, 'PRESENT STATE', 10, Amber, true);
            p.text(866, 357, postureLabel(self), 18, Paper);
            p.paragraph(866, 397, str(self, 'state', 'Set your current state with /me.'), 365, 14, Muted);
            p.paragraph(866, 444, '/lay then move to sneak. /stand to walk normally.', 365, 11, Sage, 1.45);
            p.text(866, 478, 'ROLEPLAY PROGRESSION', 10, Amber, true);
            p.text(866, 508, `Level ${Math.trunc(num(self, 'socialLevel', 1))}`, 24, Paper);
            p.text(866, 550, `${Math.trunc(num(self, 'socialXp'))} social experience`, 13, Sage);
            p.box(866, 582, 354, 4, Line);
            p.box(866, 582, clamp(num(self, 'socialXp') / 100, 0, 1) * 354, 4, Sage);
            p.text(866, 613, `Sneak ${clamp(Math.trunc(num(self, 'sneakSkill')), 0, 100)} / 100`, 12, Sage);
            p.text(1043, 613, `Hearing ${clamp(Math.trunc(num(self, 'hearingSkill')), 0, 100)} / 100`, 12, Sage);
            p.text(866, 637, `Scent ${clamp(Math.trunc(num(self, 'scentSkill')), 0, 100)} / 100`, 12, Scent);
            p.text(1043, 637, `Nose ${Math.round(clamp(num(self, 'noseHealth', 1), 0, 1) * 100)}%`, 12, Muted);
            p.text(866, 663, 'SKILLS · TRAINING NOT IMPLEMENTED', 9, Muted, true);
            p.text(326, 706, 'DESCRIPTION', 10, Amber, true);
            p.paragraph(326, 736, str(self, 'description',
                'Your appearance belongs here. Map tokens remain simple, leaving actions and expression to the imagination.'), 690, 14, Paper);
            button(1044, 789, 240, 40, 'CHARACTER SELECTION', 'leave_character');
        } else if (m === 'inventory') {
            p.text(324, 185, 'BELONGINGS / EQUIPMENT', 10, Amber, true);
            p.text(324, 221, 'What you carry', 35, Paper, false, true);
            p.text(325, 271, 'Each object has a place in the story.', 14, Muted);
            p.text(856, 272, `PURSE · ${countText(self, 'cash')} silver pennies`, 13, Amber);
            const items = arr(s.snapshot, 'inventory');
            if (!items.length)
                p.paragraph(326, 337, 'Your pack is empty. Objects you acquire will appear here, each with its own icon and description.', 700, 16, Muted);
            items.slice(0, 6).forEach((value, i) => {
                if (!isObject(value)) return;
                const item = value;
                const x = 325 + (i % 2) * 478, y = 327 + Math.floor(i / 2) * 142;
                p.box(x, y, 455, 124, Ink);
                p.frame(x, y, 455, 124, Line);
                p.box(x + 14, y + 16, 81, 89, Raised);
                this.itemIcon(x + 27, y + 28, item);
                p.text(x + 113, y + 20, str(item, 'name'), 17, Paper, false, true);
                p.text(x + 114, y + 50, `${bool(item, 'equipped') ? 'EQUIPPED' : 'CARRIED'} · × ${wholeCount(item, 'quantity', 1)}`, 9,
                    bool(item, 'equipped') ? Sage : Muted, true);
                p.paragraph(x + 114, y + 72, str(item, 'description'), 316, 12, Muted, 1.4);
            });
            if (s.inventoryQuantity('meal') > 0) button(326, 765, 160, 39, 'EAT ONE MEAL', 'eat');
            const resource = s.visibleResource();
            if (s.canGather()) button(501, 765, 165, 39, 'GATHER HERBS', 'gather');
            else if (resource)
                p.text(505, 779, wholeCount(resource, 'remaining') > 0 ? 'Approach the herb patch to gather.' : 'The visible herb patch is depleted.', 11, Muted);
            const merchantId = str(obj(s.snapshot, 'merchant'), 'id');
            if (merchantId)
                button(949, 765, 285, 39, merchantId === 'npc_keeper' ? 'TRADE WITH THE KEEPER' : 'TRADE WITH THE SHOPKEEPER', 'trade_open', merchantId);
            p.text(326, 823, 'Equipment appears on your sheet. Your map presence remains W>.', 12, Muted);
        } else if (m === 'trade') {
            const merchant = obj(s.snapshot, 'merchant');
            const available = !!str(merchant, 'id');
            p.text(324, 185, 'LOCAL TRADE / REAL GOODS & REAL PURSES', 10, Amber, true);
            p.text(324, 221, available ? str(merchant, 'name', 'The keeper').slice(0, 38) : 'The counter is unattended', 31, Paper, false, true);
            if (!available)
                p.paragraph(326, 320, 'The keeper is no longer awake, visible, and within reach. Return to them to see current stock and offers. ' +
                    'Old quotes are not retained.', 860, 17, Muted, 1.7);
            else {
                p.text(326, 273, 'One item per exchange. The authority rechecks every offer.', 13, Muted);
                p.text(326, 307, `YOUR PURSE · ${countText(self, 'cash')} p`, 12, Amber, true);
                p.text(804, 307, `KEEPER'S PURSE · ${countText(merchant, 'cash')} p`, 12, Sage, true);
                (['herbs', 'meal'] as const).forEach((good, i) => {
                    const item = s.tradeItem(good);
                    const x = 326, y = 347 + i * 204;
                    p.box(x, y, 908, 186, Ink);
                    p.frame(x, y, 908, 186, Line);
                    p.text(x + 20, y + 16, i === 0 ? 'Cooking herbs' : 'Prepared meal', 21, Paper, false, true);
                    p.text(x + 21, y + 53, `KEEPER STOCK ${countText(item, 'stock')} · YOU CARRY ${countText(item, 'owned')}`, 10, Muted, true);
                    for (const buy of [true, false]) {
                        const bx = x + (buy ? 21 : 465), by = y + 83;
                        const enabled = s.canTradeItem(good, buy);
                        const label = `${buy ? 'BUY 1 · ' : 'SELL 1 · '}${countText(item, buy ? 'buyPrice' : 'sellPrice')} p`;
                        if (enabled) button(bx, by, 214, 39, label, buy ? 'trade_buy' : 'trade_sell', good);
                        else {
                            p.box(bx, by, 214, 39, Panel);
                            p.text(bx + 12, by + 11, label, 12, Muted);
                        }
                        p.paragraph(bx, by + 51, enabled
                            ? (buy ? "Your purse pays for one item from the keeper's stock." : 'The keeper pays for one item from your pack.')
                            : str(item, buy ? 'buyReason' : 'sellReason', 'This offer is currently unavailable.').slice(0, 102),
                        403, 12, enabled ? Muted : Amber, 1.4);
                    }
                });
                p.text(326, 786, 'p = silver penny · stock, demand, and cash are finite', 12, Muted);
                p.text(326, 811, 'Prices can change as residents gather, cook, buy, and eat.', 12, Muted);
            }
        } else if (m === 'settings') {
            p.text(324, 185, 'PREFERENCES / READING & PRESENCE', 10, Amber, true);
            p.text(324, 221, 'Make yourself heard', 35, Paper, false, true);
            p.text(326, 291, 'YOUR SPEAKING COLOR', 10, Amber, true);
            p.paragraph(326, 319, 'One color follows your words, typing ellipsis, and speaking marker. Your map identity keeps its own color.',
                504, 14, Muted);
            for (let i = 0; i < 32; ++i) {
                const x = 326 + (i % 8) * 60, y = 395 + Math.floor(i / 8) * 55;
                p.box(x, y, 43, 36, speakingColor(i));
                if (i === s.selectedColor) p.frame(x - 4, y - 4, 51, 44, Paper);
                s.hits.push({rect: rect(x, y, x + 43, y + 36), action: 'color', target: String(i)});
            }
            p.text(326, 644, '"There is always another story beyond the door."', 16, speakingColor(s.selectedColor));
            p.text(888, 291, 'STORY FLOW', 10, Amber, true);
            button(886, 323, 347, 45, `Reveal speed: ${s.revealSpeed === 0 ? 'Instant' : `${s.revealSpeed} characters / second`}`, 'speed');
            p.paragraph(886, 382, 'One post unfolds at a time. Later voices wait their turn without changing when events happen.', 342, 13, Muted);
            button(886, 474, 347, 45, s.reducedMotion ? 'Reduced motion: On · static weather' : 'Reduced motion: Off', 'motion');
            button(886, 535, 170, 45, s.flatWorld ? 'World: Always flat' : 'World: Automatic', 'projection');
            button(1063, 535, 170, 45, s.plainGlyphs ? 'Map: Plain ASCII' : 'Map: Unicode', 'glyphs');
            const split = s.storyExtra < 0 ? 'Compact narrative' : s.storyExtra === 0 ? 'Balanced' : s.storyExtra === 150 ? 'Wide narrative' : 'Text-first';
            button(886, 596, 347, 45, `Pane balance: ${split}`, 'split');
            if (bool(s.snapshot, 'devTools')) {
                p.text(326, 675, 'WORLD CLOCK · DEVELOPMENT ONLY', 9, Muted, true);
                ['dawn', 'day', 'dusk', 'night'].forEach((t, i) => button(326 + i * 122, 692, 112, 34, t, 'time', t, s.environment.phase === t));
                p.text(326, 738, 'ROOM LIGHTING · DEVELOPMENT ONLY', 9, Muted, true);
                ['warm', 'unlit', 'daylit', 'cool'].forEach((t, i) => button(326 + i * 122, 755, 112, 34, t, 'lighting', t));
                p.text(886, 663, 'DEVELOPMENT WEATHER', 9, Muted, true);
                const short = ['clear', 'cloud', 'rain', 'storm', 'fog', 'snow', 'sand'];
                ['clear', 'overcast', 'rain', 'storm', 'fog', 'snow', 'sandstorm'].forEach((w, i) => button(886 + i * 50, 686, 47, 39, short[i], 'weather', w));
                p.text(886, 737, 'WIND FLOW · DEVELOPMENT ONLY', 9, Muted, true);
                ['east', 'west', 'north', 'calm', 'live'].forEach((w, i) => button(886 + i * 70, 757, 66, 34, w, 'wind', w));
                button(886, 800, 105, 32, 'Day +1', 'calendar', 'day');
                button(1005, 800, 105, 32, 'Year +1', 'calendar', 'year');
                button(1124, 800, 110, 32, 'Seasonal', 'weather', 'seasonal');
            }
            p.text(326, 798, 'ENTER  write / send     SHIFT + ENTER  newline', 10, Muted, true);
            p.text(326, 814, 'ESC  preserve draft', 10, Muted, true);
            p.text(326, 835, 'ALT + mouse previews facing; click to turn. /lay + move sneaks; /stand rises.', 12, Muted);
        } else if (m === 'leave_character') {
            p.text(324, 185, 'RETURN TO YOUR CHARACTERS', 10, Amber, true);
            p.text(324, 249, 'Leave this character?', 33, Paper);
            p.paragraph(326, 322, "Your character remains saved. Returning to selection ends this play session. Unsent drafts and this session's " +
                'local transcript are not kept when switching characters.', 876, 19, Muted, 1.7);
            button(326, 496, 284, 48, 'RETURN TO SELECTION', 'leave_confirm');
            button(622, 496, 284, 48, 'KEEP PLAYING', 'leave_cancel');
        } else {
            p.text(324, 185, 'A CLOSER LOOK', 10, Amber, true);
            if (s.portraitAppearance()) {
                p.box(324, 285, 449, 355, Ink);
                p.frame(324, 285, 449, 355, Line);
                drawPortrait(p.ctx, this.portraits, s.portraitAppearance(), s.portraitAge(), 324, 285, 449, 355);
                const inspected = s.inspectedCharacter;
                p.text(326, 670, str(inspected, 'lifeStage', 'adult').toUpperCase() + (num(inspected, 'shoulderHeightCm') > 0
                    ? ` · ${num(inspected, 'shoulderHeightCm').toFixed(0)} CM AT SHOULDER` : ' · STATIC CHARACTER PROFILE'), 10, Sage, true);
                p.paragraph(805, 254, s.inspectedText, 426, 16, Paper, 1.6);
            } else p.paragraph(325, 254, s.inspectedText, 876, 19, Paper, 1.7);
            p.text(326, 787, 'Only information your character is allowed to perceive appears here.', 12, Muted);
        }
    }

    private itemIcon(x: number, y: number, item: Json) {
        const p = this.p;
        const kind = str(item, 'icon', 'bag');
        const at = (dx: number, dy: number): Point => [x + dx, y + dy];
        if (kind.includes('bag') || kind.includes('satchel') || kind.includes('pack')) {
            p.frame(x + 5, y + 18, 45, 39, Amber);
            p.lines([at(11, 18), at(14, 6), at(43, 6), at(48, 18)], Amber, 2);
            p.lines([at(6, 20), at(28, 34), at(49, 20)], Amber);
        } else if (str(item, 'id') === 'herbs') {
            p.lines([at(28, 59), at(26, 7)], Sage, 2);
            for (let leaf = 0; leaf < 3; ++leaf) {
                const ly = 16 + leaf * 13;
                p.lines([at(27, ly + 8), at(8, ly - 3), at(17, ly + 10), at(27, ly + 8), at(47, ly - 4), at(38, ly + 11)], Sage, 1.6);
            }
        } else if (kind.includes('bowl') || kind.includes('food')) {
            p.lines([at(3, 25), at(11, 51), at(46, 51), at(54, 25), at(3, 25)], Amber, 2);
            p.lines([at(18, 13), at(15, 4), at(18, -1)], Muted);
        } else if (kind.includes('knife') || kind.includes('weapon')) {
            p.lines([at(10, 57), at(41, 8), at(48, 3), at(42, 26), at(21, 47)], Paper, 2);
            p.lines([at(10, 38), at(30, 51)], Amber, 3);
        } else {
            p.lines([at(13, 15), at(39, 15), at(46, 53), at(9, 53), at(13, 15)], Sage, 2);
            p.frame(x + 17, y + 5, 19, 10, Sage);
        }
    }
}
