// Drawing the game screen: SRatwGame::OnPaint and its helpers (UI/SRatwGame.cpp), call for call, on a 1600×1000
// canvas. Reads the state; records click targets (state.hits) and where the map is (state.mapRect and friends).
import {css, lerp, rgb, scale, transparent, withAlpha, type Color} from '../ui/color.ts';
import {contains, rect, type Painter, type Point, type Rect} from '../ui/painter.ts';
import {Amber, Blue, Ink, Line, Muted, Panel, Paper, Raised, Sage, Scent, speakingColor, font} from '../ui/theme.ts';
import {arr, bool, boundedNum, clamp, isObject, num, obj, objects, str, wholeCount, wrapCoordinate, type Json} from './json.ts';
import {elevationLabel, windLabel} from './labels.ts';
import {Size as SheetSize, type Art, type Sheets} from './weatherArt.ts';
import type {GameState} from './state.ts';
import {apart, chanceFrom, coneTiles, myTurn, octantGap, pathTo, quarter, stepToward, type BattleView, type FighterView} from './battle.ts';
import type {Mark} from './fightFx.ts';
import {fitScale, MapRenderer, MapScales} from './minimap.ts';

/** How a home's stores are drawn (doc 36). */
const StoreGlyphs: Record<string, string> = {larder: '%', chest: '=', wardrobe: 'H', woodpile: '#'};
import {pageSurface, TerrainLayer, terrainInfo, type Surface, type SurfaceFactory} from './terrainLayer.ts';

// Hostile wolves (doc 32), the same red as a fight's other side.
const HostileRed = rgb(0xe0695e);

/** The server's weather grid letters (RatwGame.cpp snapshot): kind names as the painter knows them. */
const KindOfLetter: Record<string, string> = {r: 'rain', f: 'fog', n: 'snow', o: 'overcast', s: 'storm', d: 'sandstorm', c: 'clear'};

export {terrainInfo};

/** Tile sizes for the zoom steps, closest last. */
export const ZoomTiles = [14, 18, 22, 28];

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


export class GamePainter {
    private s: GameState;
    private p: Painter;
    private sheets: Sheets;
    readonly terrain: TerrainLayer;
    readonly map: MapRenderer;
    private surfaces: SurfaceFactory;
    private patterns = new WeakMap<CanvasImageSource, CanvasPattern | null>();
    private lastCell = '';
    private lastSelfScreen: Point | null = null;

    constructor(state: GameState, painter: Painter, sheets: Sheets, surfaces?: SurfaceFactory) {
        this.s = state;
        this.p = painter;
        this.sheets = sheets;
        this.terrain = new TerrainLayer(surfaces);
        this.surfaces = surfaces ?? pageSurface;
        this.map = new MapRenderer(this.surfaces);
    }


    /**
     * The map: the canvas is the map and nothing else (the panels around it are HTML, ui/hud). Coordinates are the
     * canvas's own CSS pixels; `width` and `height` are its size.
     */
    paint(width = 960, height = 617) {
        const s = this.s, p = this.p;
        s.hits = [];
        s.mapRect = rect(0, 0, width, height);
        p.box(0, 0, width, height, rgb(0x0f1718));
        if (s.worldMap && s.travelAtlas) this.legacyFrame(() => this.drawWorld());
        else if (s.worldMap) this.drawCountry(width, height);
        else this.drawLocal();
    }

    /** The World Map (M): the country around, as the minimap draws it but filling the map, named, zoomed and panned. */
    private drawCountry(width: number, height: number) {
        const s = this.s, p = this.p;
        const scale = s.worldZoom < 0 ? fitScale(s.snapshot, width, height) : MapScales[s.worldZoom] ?? 1;
        this.map.draw(p.ctx, s, {x: 0, y: 0, w: width, h: height}, scale, s.worldPan, true);
        const tab = (x: number, label: string, action: string, active: boolean) => {
            p.box(x, 12, 130, 30, active ? Raised : Panel);
            p.frame(x, 12, 130, 30, active ? Sage : Line);
            p.text(x + 12, 20, label, 9, active ? Sage : Muted, true);
            s.hits.push({rect: rect(x, 12, x + 130, 42), action, target: ''});
        };
        tab(width - 290, 'NEARBY', 'nearby', true);
        tab(width - 150, 'KNOWN ROUTES', 'atlas', false);
        p.text(14, height - 26, 'WHEEL zoom · SHIFT/CTRL+WHEEL pan · BRIGHT seen · DIM remembered · DARK unexplored', 9, Muted, true);
    }

    /**
     * The world map and travel atlas still draw in the old fixed 960×617 frame (phase 8 replaces them): fitted into
     * the map, with the pointer and click targets carried between the two.
     */
    private legacyFrame(draw: () => void) {
        const s = this.s, c = this.p.ctx, map = s.mapRect;
        const old = rect(584, 199, 1544, 816);
        const k = Math.min((map.right - map.left) / (old.right - old.left), (map.bottom - map.top) / (old.bottom - old.top));
        const dx = map.left + ((map.right - map.left) - (old.right - old.left) * k) / 2 - old.left * k;
        const dy = map.top + ((map.bottom - map.top) - (old.bottom - old.top) * k) / 2 - old.top * k;
        const hover = s.hover, from = s.hits.length;
        s.hover = [(hover[0] - dx) / k, (hover[1] - dy) / k];
        const realMap = s.mapRect;
        s.mapRect = old;
        c.save();
        c.translate(dx, dy);
        c.scale(k, k);
        try {
            draw();
        } finally {
            c.restore();
            s.hover = hover;
            s.mapRect = realMap;
        }
        for (const h of s.hits.slice(from))
            h.rect = rect(h.rect.left * k + dx, h.rect.top * k + dy, h.rect.right * k + dx, h.rect.bottom * k + dy);
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
                result.weatherStrength = w[1] * (weather[e.weather] ? Math.max(0.25, e.intensity) : 1);
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
        const count = Math.round((storm ? 56 : 32) * Math.max(0.25, e.intensity));
        for (let i = 0; i < count; ++i) {
            const life = wrapCoordinate(animation * (storm ? 1.1 : 0.7) + i * 0.618, 1);
            if (life > 0.62) continue;
            marks.push({x: area.left + 24 + wrapCoordinate(i * 157.3, width - 48), y: area.top + 44 + wrapCoordinate(i * 91.7, height - 68),
                size: 1 + life * 8, alpha: (1 - life / 0.62) * 0.32});
        }
        return marks;
    }

    /** The moving sheets of one kind of weather (by default, the weather where the wolf stands). */
    private weatherLayers(weather = this.s.environment.weather): WeatherLayer[] {
        const s = this.s, e = s.environment;
        const layers: WeatherLayer[] = [];
        if (!s.outdoors || s.worldMap) return layers;
        const t = s.reducedMotion ? 0 : s.clock;
        const wind: Point = [Math.cos(s.windDirection) * s.windStrength, Math.sin(s.windDirection) * s.windStrength];
        // Drift is in screen pixels a second, offset in screen pixels; a layer scrolls by them in its own pixels.
        const add = (art: Art, drift: Point, scaleBy: number, angle: number, tint: Color, offset: Point = [0, 0]) =>
            layers.push({art, scroll: [(drift[0] * t + offset[0]) / scaleBy, (drift[1] * t + offset[1]) / scaleBy], scale: scaleBy, angle, tint});
        // Sunlight: the shadows of passing clouds cross the ground; they vanish with the sun.
        if ((weather === 'clear' || weather === 'overcast') && e.daylight > 0.05) {
            const overcast = weather === 'overcast';
            const drift: Point = [wind[0] * 26 + 7, wind[1] * 26 + 3];
            add('cloud', drift, 2.2, 0, rgb(0x08100c, (overcast ? 0.24 : 0.16) * e.daylight));
            if (overcast) add('cloud', [drift[0] * 1.4, drift[1] * 1.4], 1.35, 0, rgb(0x0c1216, 0.14 * e.daylight));
        }
        if (weather === 'rain' || weather === 'storm') {
            const storm = weather === 'storm';
            // The sheets' streaks run along +Y; turn them to fall where the wind pushes.
            const angle = Math.atan2(-wind[0] * (storm ? 0.9 : 0.5), 1);
            const boost = storm ? 1.2 : 1;
            add('rain', [0, 420], 0.9, angle, rgb(0xa9c4d2, 0.26 * boost));
            add('rain', [0, 700], 1.35, angle, rgb(0xb9d0dc, 0.36 * boost));
            if (storm) add('rain', [0, 980], 1.8, angle, rgb(0xc7d9e2, 0.44));
        } else if (weather === 'snow') {
            const scales = [0.8, 1.1, 1.5], fall = [20, 32, 48], alphas = [0.45, 0.62, 0.8];
            // Each depth sways on its own phase, so the flakes never march in step.
            for (let i = 0; i < 3; ++i)
                add('snow', [wind[0] * 60, fall[i] + wind[1] * 40], scales[i], 0, rgb(0xe6eff2, alphas[i]), [Math.sin(t * 0.5 + i * 2.1) * 12, 0]);
        } else if (weather === 'fog') {
            add('mist', [wind[0] * 8 + 5, wind[1] * 8 + 1], 2.6, 0, rgb(0xc3d1cf, 0.3));
            add('mist', [wind[0] * 14 - 4, wind[1] * 14 + 2], 1.7, 0, rgb(0xc8d6d3, 0.22));
        } else if (weather === 'sandstorm') {
            // Dust streaks run along the sheet's X axis, so the sheet turns to face the wind.
            const speed = 60 + 220 * s.windStrength;
            add('mist', [wind[0] * 30 + 12, wind[1] * 30], 3, 0, rgb(0xb88f58, 0.28));
            add('dust', [speed, 0], 1.9, s.windDirection, rgb(0xc9a26a, 0.42));
            add('dust', [speed * 1.6, 0], 1.2, s.windDirection, rgb(0xdcb886, 0.3));
        }
        return layers;
    }

    private weatherSurface: Surface | null = null;
    private masks = new Map<string, Surface>();

    /**
     * Each kind of weather in the field over this cell, painted on its own and then cut down by a soft mask of how
     * strong it is across the cell (the server's grid, scaled up smoothly), so rain thins toward a front's edge and
     * gives way to fog or snow where two meet. False where it can't be done (no field, or no offscreen canvas).
     */
    private drawFieldWeather(b: Rect): boolean {
        const s = this.s, f = s.weatherField, c = this.p.ctx;
        const t = typeof c.getTransform === 'function' ? c.getTransform() : null;
        if (!f || !t || !c.canvas || s.worldMap) return false;
        const W = c.canvas.width, H = c.canvas.height;
        const letters = new Set<string>();
        for (let i = 0; i < f.kinds.length; ++i) if (f.kinds[i] !== 'c' && f.amounts[i] !== '0') letters.add(f.kinds[i]);
        // Sunlit clear ground keeps its passing cloud shadows.
        if (s.environment.weather === 'clear') for (const layer of this.weatherLayers('clear')) this.drawWeatherLayer(b, layer);
        if (!letters.size) return true;
        if (!this.weatherSurface || this.weatherSurface.canvas.width !== W || this.weatherSurface.canvas.height !== H)
            this.weatherSurface = this.surfaces(W, H);
        const surface = this.weatherSurface;
        if (!surface) return false;
        const oc = surface.ctx, size = f.step * s.tileSize, [ox, oy] = s.mapOrigin;
        for (const letter of letters) {
            const layers = this.weatherLayers(KindOfLetter[letter] ?? 'clear');
            if (!layers.length) continue;
            oc.setTransform(1, 0, 0, 1, 0, 0);
            oc.globalCompositeOperation = 'source-over';
            oc.clearRect(0, 0, W, H);
            oc.setTransform(t);
            oc.save();
            oc.beginPath();
            oc.rect(b.left, b.top, b.right - b.left, b.bottom - b.top);
            oc.clip();
            for (const layer of layers) this.drawWeatherLayer(b, layer, oc);
            oc.restore();
            const mask = this.maskFor(f, letter);
            if (!mask) continue;
            oc.globalCompositeOperation = 'destination-in';
            oc.imageSmoothingEnabled = true;
            oc.drawImage(mask.canvas, ox - size / 2, oy - size / 2, f.cols * size, f.rows * size);
            oc.globalCompositeOperation = 'source-over';
            c.save();
            c.setTransform(1, 0, 0, 1, 0, 0);
            c.drawImage(surface.canvas, 0, 0);
            c.restore();
        }
        return true;
    }

    /** One kind's strength over the cell as a tiny image: one pixel a grid sample, its alpha the strength. */
    private maskFor(f: NonNullable<GameState['weatherField']>, letter: string): Surface | null {
        const key = `${f.cols}x${f.rows}|${f.kinds}|${f.amounts}|${letter}`;
        const kept = this.masks.get(key);
        if (kept) return kept;
        const surface = this.surfaces(f.cols, f.rows);
        if (!surface) return null;
        const image = surface.ctx.createImageData(f.cols, f.rows);
        for (let i = 0; i < f.cols * f.rows; ++i) {
            image.data[i * 4] = image.data[i * 4 + 1] = image.data[i * 4 + 2] = 255;
            image.data[i * 4 + 3] = f.kinds[i] === letter ? Math.round(Number(f.amounts[i]) / 9 * 255) : 0;
        }
        surface.ctx.putImageData(image, 0, 0);
        if (this.masks.size > 16) this.masks.clear();
        this.masks.set(key, surface);
        return surface;
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
        return 0.2 * Math.max(first, second) * Math.max(0.3, s.environment.intensity);
    }

    private drawWeatherLayer(area: Rect, layer: WeatherLayer, on?: CanvasRenderingContext2D, alpha = 1) {
        if (layer.tint.a <= 0.001 || layer.scale <= 0) return;
        const c = on ?? this.p.ctx;
        const sheet = this.sheets.get(layer.art, css(withAlpha(layer.tint, 1)));
        let pattern = this.patterns.get(sheet);
        if (pattern === undefined) {
            pattern = c.createPattern(sheet, 'repeat');
            this.patterns.set(sheet, pattern);
        }
        if (!pattern) return;
        const shiftX = wrapCoordinate(layer.scroll[0], SheetSize) * layer.scale, shiftY = wrapCoordinate(layer.scroll[1], SheetSize) * layer.scale;
        const cos = Math.cos(layer.angle), sin = Math.sin(layer.angle);
        const cx = (area.left + area.right) / 2 + cos * shiftX - sin * shiftY;
        const cy = (area.top + area.bottom) / 2 + sin * shiftX + cos * shiftY;
        pattern.setTransform(new DOMMatrix().translateSelf(cx, cy).rotateSelf(layer.angle * 180 / Math.PI).scaleSelf(layer.scale));
        c.save();
        c.globalAlpha = Math.min(1, layer.tint.a * alpha);
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
                // A halo lies outside the room: nothing of it shows when the room covers the whole map.
                const covered = b.left <= map.left && b.top <= map.top && b.right >= map.right && b.bottom >= map.bottom;
                if (!covered && a.glowStrength > 0.001) p.halo(b, a.haloRadius, withAlpha(a.glowColor, a.glowStrength * 0.42));
                if (!covered && a.weatherStrength > 0.001) p.halo(b, a.haloRadius * 0.7, withAlpha(a.weatherColor, a.weatherStrength));
                if (s.outdoors) {
                    const ground = lerp(rgb(0x1e2c22), rgb(0x0a1225), darkness);
                    const horizon = lerp(rgb(0x343629), rgb(0x152339), darkness);
                    p.gradient(b.left, b.top, w, h, horizon, ground, withAlpha(scale(ground, 0.78), 1), false);
                    if (e.phase === 'dawn' || e.phase === 'dusk')
                        p.gradient(b.left, b.top, w, h, rgb(0xd69864, 0.12), rgb(0xc0774b, 0.04), rgb(0x786992, 0.02), e.phase === 'dawn');
                    p.ctx.save();
                    p.ctx.globalAlpha = e.weather === 'clear' ? 1 : Math.max(0.25, e.intensity);   // As strong as the weather here.
                    if (e.weather === 'rain') p.gradient(b.left, b.top, w, h, rgb(0x617a92, 0.08), rgb(0x34495c, 0.07), rgb(0x294859, 0.11), false);
                    else if (e.weather === 'snow') p.gradient(b.left, b.top, w, h, rgb(0xb4c3ce, 0.09), rgb(0x87a0b8, 0.07), rgb(0xb7c9ce, 0.12), false);
                    else if (e.weather === 'fog') p.box(b.left, b.top, w, h, rgb(0x9fafac, 0.11));
                    else if (e.weather === 'overcast') p.box(b.left, b.top, w, h, rgb(0x7d868c, 0.1));
                    else if (e.weather === 'storm') p.gradient(b.left, b.top, w, h, rgb(0x3c4b5c, 0.18), rgb(0x223040, 0.14), rgb(0x1b2836, 0.2), false);
                    else if (e.weather === 'sandstorm') p.gradient(b.left, b.top, w, h, rgb(0xc09a62, 0.2), rgb(0xa57b45, 0.16), rgb(0x8c6a3e, 0.22), true);
                    else if (e.phase === 'day') p.box(b.left, b.top, w, h, rgb(0xffd89a, 0.05 * e.daylight));
                    p.ctx.restore();
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
                // The regional field (doc 29, phase 7): each kind of weather drawn where it is, as strong as it is there.
                if (!this.drawFieldWeather(b)) {
                    const strength = e.weather === 'clear' ? 1 : Math.max(0.2, e.intensity);
                    for (const layer of this.weatherLayers()) this.drawWeatherLayer(b, layer, undefined, strength);
                }
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

    /** A glyph of what just happened, or of how the fight stands (fx.ts). */
    private drawMark(m: Mark, ox: number, oy: number, tile: number) {
        const colors: Record<Mark['color'], Color> = {hit: rgb(0xf3e3c3), graze: rgb(0xd9c08c), miss: Muted, fire: rgb(0xff8a3c),
            smoke: Muted, burn: rgb(0xff6a2c), charge: rgb(0xffb35c), item: Paper, ash: rgb(0x8a8279)};
        const color = m.glyph === '*' && m.color === 'fire' ? rgb(0xd4483c) : colors[m.color];
        const size = m.color === 'fire' ? Math.max(11, Math.round(this.s.tileSize * 0.7)) : 12;
        this.turnedText(ox + (m.x + 0.5) * tile, oy + (m.y + 0.5) * tile, m.glyph, size, withAlpha(color, m.alpha), 0);
    }

    /** Downed flashes orange; the dead are gray. */
    private fallenColor(state: string): Color {
        if (state === 'dead') return rgb(0x7d8285);
        return withAlpha(rgb(0xff9a3c), 0.45 + 0.55 * Math.abs(Math.sin(this.s.clock * 4)));
    }

    /** The red squares around fights an onlooker can see (their fighters stand frozen in a lineup inside). */
    private drawFights(ox: number, oy: number, tile: number) {
        const p = this.p, red = rgb(0xd4483c);
        // Things lying on the ground: a sword knocked loose in a fight. Close enough, a click picks it up.
        for (const g of this.s.ground) {
            const x = ox + g.x * tile, y = oy + g.y * tile;
            this.turnedText(x, y, g.item === 'sword' ? '†' : '?', 14, Paper, 0);
            if (g.near) this.s.hits.push({rect: rect(x - 10, y - 10, x + 10, y + 10), action: 'ground', target: g.id});
        }
        for (const f of this.s.fights) {
            const x = ox + f.x0 * tile, y = oy + f.y0 * tile, w = (f.x1 - f.x0) * tile, h = (f.y1 - f.y0) * tile;
            p.frame(x, y, w, h, withAlpha(red, f.over ? 0.4 : 0.9));
            p.frame(x + 1, y + 1, w - 2, h - 2, withAlpha(red, f.over ? 0.25 : 0.6));
            p.text(x + 3, y - 14, f.watching ? 'FIGHT · WATCHING' : 'FIGHT', 9, withAlpha(red, 0.95), true);
        }
    }

    /**
     * Inside a fight (doc 37, phase 2): the arena, built to be read. The world outside it is dimmed and its own ground
     * quietened, so the wolves and what they do stand out; each wolf is a round token in its side's colour with a
     * health ring and a wedge where it faces; one's reach is an outline; pointing at a tile shows the way there,
     * pointing at a foe the blow (its chance, from where it would come) and where they are weak.
     */
    private drawArena(b: BattleView, ox: number, oy: number, tile: number) {
        const s = this.s, p = this.p, c = p.ctx;
        const {x: ax, y: ay, w: aw, h: ah} = b.arena;
        const red = rgb(0xd4483c), foeRed = rgb(0xe0695e);
        const map = s.mapRect;
        const left = ox + ax * tile, top = oy + ay * tile, right = left + aw * tile, bottom = top + ah * tile;
        // The world outside the arena, dimmed; the ground inside it, quietened, with a faint grid when there's room.
        c.save();
        c.beginPath();
        c.rect(map.left, map.top, map.right - map.left, map.bottom - map.top);
        c.rect(left, top, aw * tile, ah * tile);
        c.fillStyle = css(withAlpha(Ink, 0.66));
        c.fill('evenodd');
        c.restore();
        p.box(left, top, aw * tile, ah * tile, withAlpha(Ink, 0.6));
        if (tile >= 18) {
            c.save();
            c.beginPath();
            const x0 = Math.max(ax, Math.floor((map.left - ox) / tile)), x1 = Math.min(ax + aw, Math.ceil((map.right - ox) / tile));
            const y0 = Math.max(ay, Math.floor((map.top - oy) / tile)), y1 = Math.min(ay + ah, Math.ceil((map.bottom - oy) / tile));
            for (let x = x0; x <= x1; ++x) {
                c.moveTo(ox + x * tile + 0.5, oy + y0 * tile);
                c.lineTo(ox + x * tile + 0.5, oy + y1 * tile);
            }
            for (let y = y0; y <= y1; ++y) {
                c.moveTo(ox + x0 * tile, oy + y * tile + 0.5);
                c.lineTo(ox + x1 * tile, oy + y * tile + 0.5);
            }
            c.strokeStyle = css(withAlpha(Paper, 0.045));
            c.lineWidth = 1;
            c.stroke();
            c.restore();
        }
        p.frame(left, top, aw * tile, ah * tile, withAlpha(red, 0.8));
        const me = b.observer ? undefined : b.fighters.find(f => f.id === s.selfId);
        const mine = myTurn(b, s.selfId);
        // The edge rows, where one can flee from: a faint band, with arrows out at the middle of each side.
        if (me && !b.over && me.status === 'fighting') {
            const band = withAlpha(red, 0.1);
            p.box(left, top, aw * tile, 2 * tile, band);
            p.box(left, bottom - 2 * tile, aw * tile, 2 * tile, band);
            p.box(left, top + 2 * tile, 2 * tile, (ah - 4) * tile, band);
            p.box(right - 2 * tile, top + 2 * tile, 2 * tile, (ah - 4) * tile, band);
            const out = withAlpha(red, 0.55), size = clamp(Math.round(tile * 0.6), 10, 18);
            for (const [x, y, angle] of [[(left + right) / 2, top + tile, -Math.PI / 2], [(left + right) / 2, bottom - tile, Math.PI / 2],
                [left + tile, (top + bottom) / 2, Math.PI], [right - tile, (top + bottom) / 2, 0]] as const)
                this.turnedText(x, y, '➜', size, out, angle);
        }
        // Spells gathering: their cones, locked, outlined in red for everyone to see (the tell), deepening as the
        // countdown runs out; the seconds left over the cone.
        for (const cast of b.casts) {
            const remaining = Math.max(0, cast.left - (s.clock - s.battleAt));
            const near = cast.of > 0 ? 1 - remaining / cast.of : 0;
            let cx = 0, cy = 0;
            for (const [x, y] of cast.tiles) {
                p.box(ox + x * tile + 1, oy + y * tile + 1, tile - 2, tile - 2, withAlpha(red, 0.12 + 0.35 * near));
                p.frame(ox + x * tile + 1, oy + y * tile + 1, tile - 2, tile - 2, withAlpha(red, 0.75));
                cx += x;
                cy += y;
            }
            if (cast.tiles.length) {
                const words = `${remaining.toFixed(1)}s`;
                const [ww] = p.measure(words, 12, true);
                const tx = ox + (cx / cast.tiles.length + 0.5) * tile, ty = oy + (cy / cast.tiles.length + 0.5) * tile;
                p.box(tx - ww / 2 - 4, ty - 9, ww + 8, 16, withAlpha(Ink, 0.85));
                p.text(tx - ww / 2, ty - 8, words, 12, rgb(0xff9a3c), true);
            }
        }
        // Aiming fire: where it would go.
        if (s.aiming === 'flame' && b.flame && me) {
            const tx = Math.floor((s.hover[0] - ox) / tile), ty = Math.floor((s.hover[1] - oy) / tile);
            for (const [x, y] of coneTiles(b, me.x, me.y, tx, ty, b.flame.length, b.flame.angle)) {
                p.box(ox + x * tile + 1, oy + y * tile + 1, tile - 2, tile - 2, withAlpha(rgb(0xff9a3c), 0.3));
                p.frame(ox + x * tile + 1, oy + y * tile + 1, tile - 2, tile - 2, withAlpha(rgb(0xff9a3c), 0.7));
            }
        }
        const hx = Math.floor((s.hover[0] - ox) / tile), hy = Math.floor((s.hover[1] - oy) / tile);
        const centre = (x: number, y: number): Point => [ox + (x + 0.5) * tile, oy + (y + 0.5) * tile];
        // The wolf under the pointer, worked out first: what is drawn below depends on it.
        const reachOf = Math.max(14, clamp(tile * 0.38, 8, 16) + 4);
        s.hoveredEntity = b.fighters.find(f => f.status !== 'fled' &&
            Math.hypot(centre(f.x, f.y)[0] - s.hover[0], centre(f.x, f.y)[1] - s.hover[1]) < reachOf)?.id ?? '';
        const pointedFoe = me ? b.fighters.find(f => f.id === s.hoveredEntity && f.side !== me.side && f.status === 'fighting') : undefined;
        const range = b.mouth === 'sword' ? 2 : 1;
        // Where this wolf can go: an outline round the lit tiles. The aimed-at foe's strike tiles
        // among them are marked, brighter where the blow is likelier (their side and back).
        // Waiting for one's turn (doc 37): the same, for the next turn, dashed; a click plans it.
        const planning = !!me && !mine && b.planning;
        if (me && (mine || planning) && !b.moved && s.aiming !== 'flame' && b.reach.length) {
            const lit = new Set(b.reach.map(([x, y]) => `${x},${y}`));
            lit.add(`${me.x},${me.y}`);              // One's own tile is inside the outline, not a hole in it.
            const occupied = new Set(b.fighters.filter(f => f.status !== 'fled').map(f => `${f.x},${f.y}`));
            c.save();
            c.beginPath();
            for (const [x, y] of [...b.reach, [me.x, me.y] as [number, number]]) {
                const x0 = ox + x * tile, y0 = oy + y * tile;
                if (!lit.has(`${x},${y - 1}`)) { c.moveTo(x0, y0); c.lineTo(x0 + tile, y0); }
                if (!lit.has(`${x},${y + 1}`)) { c.moveTo(x0, y0 + tile); c.lineTo(x0 + tile, y0 + tile); }
                if (!lit.has(`${x - 1},${y}`)) { c.moveTo(x0, y0); c.lineTo(x0, y0 + tile); }
                if (!lit.has(`${x + 1},${y}`)) { c.moveTo(x0 + tile, y0); c.lineTo(x0 + tile, y0 + tile); }
            }
            c.strokeStyle = css(withAlpha(Amber, planning ? 0.45 : 0.75));
            c.lineWidth = 1.5;
            if (planning) c.setLineDash([3, 4]);
            c.stroke();
            c.restore();
            const aimed = b.fighters.find(f => f.id === s.fightTargetId());
            for (const [x, y] of b.reach) {
                if (aimed && !b.acted && !occupied.has(`${x},${y}`) && apart(x, y, aimed.x, aimed.y) <= range) {
                    const chance = chanceFrom(aimed, x, y);
                    p.box(ox + x * tile + 2, oy + y * tile + 2, tile - 4, tile - 4, withAlpha(foeRed, 0.08 + (chance - 20) / 75 * 0.3));
                }
            }
            // Pointing at a lit tile: the way there, and oneself there (a ghost), with the chance of a blow from it.
            if (lit.has(`${hx},${hy}`) && !(hx === me.x && hy === me.y) && !pointedFoe && !s.faceDrag) {
                p.box(ox + hx * tile + 1, oy + hy * tile + 1, tile - 2, tile - 2, withAlpha(Amber, 0.22));
                const path = pathTo(b, [me.x, me.y], [hx, hy]).map(([x, y]) => centre(x, y));
                c.save();
                c.setLineDash([4, 4]);
                p.lines(path, withAlpha(Amber, 0.8), 2);
                c.restore();
                this.drawToken({...me, x: hx, y: hy}, centre(hx, hy), tile, Amber, 0.4, false);
                if (aimed && !b.acted && apart(hx, hy, aimed.x, aimed.y) <= range)
                    this.chanceBadge(centre(hx, hy), tile, chanceFrom(aimed, hx, hy), quarter(aimed, hx, hy));
            }
        }
        const mySide = me ? me.side : 0;
        const reduced = s.reducedMotion;
        // Names go where they don't cover another's (above, below, then beside).
        const placed: Rect[] = [];
        const overlaps = (r: Rect) => placed.some(o => r.left < o.right && r.right > o.left && r.top < o.bottom && r.bottom > o.top);
        const radius = clamp(tile * 0.38, 8, 16);
        for (const f of b.fighters) {
            const [x, y] = centre(f.x, f.y);
            placed.push(rect(x - radius - 4, y - radius - 4, x + radius + 4, y + radius + 4));
        }
        // Pointing at a foe on one's turn: the blow. From here if they're in reach; else from the tile a click would
        // step to (shown as a ghost), or "out of reach".
        if (me && ((mine && !b.acted) || planning) && pointedFoe && s.aiming !== 'flame') {
            const step = apart(me.x, me.y, pointedFoe.x, pointedFoe.y) <= range ? {x: me.x, y: me.y, reaches: true}
                : stepToward(b, me, pointedFoe, range);
            const foeAt = centre(pointedFoe.x, pointedFoe.y);
            if (step) {
                const from = centre(step.x, step.y);
                if (step.x !== me.x || step.y !== me.y) {
                    c.save();
                    c.setLineDash([4, 4]);
                    p.lines(pathTo(b, [me.x, me.y], [step.x, step.y]).map(([x, y]) => centre(x, y)), withAlpha(Amber, 0.7), 2);
                    c.restore();
                    this.drawToken({...me, x: step.x, y: step.y}, from, tile, Amber, 0.4, false);
                }
                if (step.reaches) {
                    p.lines([from, foeAt], withAlpha(foeRed, 0.8), 2);
                    this.chanceBadge(foeAt, tile, chanceFrom(pointedFoe, step.x, step.y), quarter(pointedFoe, step.x, step.y), placed);
                } else this.chanceBadge(foeAt, tile, -1, 'front', placed);
            } else this.chanceBadge(foeAt, tile, -1, 'front', placed);
        }
        const colorOf = (f: FighterView) => (f.id === s.selfId ? Amber : f.side === mySide ? Blue : foeRed);
        // Others walking (doc 37: a turn shown, not just run): the way they are going, and where it ends.
        for (const f of b.fighters) {
            if (!f.walk.length || f.id === s.selfId || f.status === 'fled') continue;
            const end = f.walk[f.walk.length - 1];
            c.save();
            c.setLineDash([3, 4]);
            p.lines([centre(f.x, f.y), ...f.walk.map(([x, y]) => centre(x, y))], withAlpha(colorOf(f), 0.6), 2);
            c.restore();
            const [ex, ey] = centre(end[0], end[1]);
            c.beginPath();
            c.arc(ex, ey, radius * 0.6, 0, Math.PI * 2);
            c.strokeStyle = css(withAlpha(colorOf(f), 0.6));
            c.lineWidth = 1.5;
            c.stroke();
        }
        // One's plan for the next turn: where it goes (a ghost) and at whom; what it does is said once the names are.
        const planned = me && planning && b.plan ? this.drawPlan(b, me, centre, tile, range) : null;
        // A word or two beside a wolf, where it covers no other wolf or name: `below` first, else above first.
        const say = ([x, y]: Point, text: string, color: Color, alpha: number, below: boolean) => {
            const size = tile >= 24 ? 11 : 10;
            const [w] = p.measure(text, size, true);
            const under: [number, number] = [x - w / 2, y + radius + 6], over: [number, number] = [x - w / 2, y - radius - 22];
            const spots: [number, number][] = [below ? under : over, below ? over : under, [x + radius + 10, y - 7], [x - radius - 10 - w, y - 7],
                [x - w / 2, y + radius + 22], [x - w / 2, y - radius - 38]];
            const spot = spots.find(([lx, ly]) => !overlaps(rect(lx - 4, ly, lx + w + 4, ly + 15))) ?? spots[0];
            placed.push(rect(spot[0] - 4, spot[1], spot[0] + w + 4, spot[1] + 15));
            p.box(spot[0] - 4, spot[1], w + 8, 15, withAlpha(Ink, 0.85 * alpha));
            p.frame(spot[0] - 4, spot[1], w + 8, 15, withAlpha(color, 0.75 * alpha));
            p.text(spot[0], spot[1] + 1, text, size, withAlpha(color, alpha), true);
        };
        // The wolf pointed at (not oneself): where it is weak, as a ring of three: front, sides, back.
        const pointed = b.fighters.find(f => f.id === (s.hoveredEntity || s.highlight) && f.id !== s.selfId && f.status === 'fighting');
        if (pointed) {
            const [x, y] = centre(pointed.x, pointed.y);
            for (let k = 0; k < 8; ++k) {
                const gap = octantGap(pointed.facing, k);
                const color = gap >= 3 ? withAlpha(foeRed, 0.85) : gap === 2 ? withAlpha(Amber, 0.6) : withAlpha(Paper, 0.18);
                c.beginPath();
                c.arc(x, y, radius + 9, (k - 0.5) * Math.PI / 4 + 0.04, (k + 0.5) * Math.PI / 4 - 0.04);
                c.strokeStyle = css(color);
                c.lineWidth = 4;
                c.stroke();
            }
        }
        for (const f of b.fighters) {
            if (f.status === 'fled') continue;
            // A lunge, a recoil, a sidestep, a gathering caster's tremble: offsets for the eye only.
            const [mx, my] = s.fx.offset(f.id, s.clock, reduced);
            const [tx, ty] = s.fx.tremble(f.id, b, s.clock, reduced);
            const [wx, wy] = s.walkOffset(f.id, f.x, f.y, s.clock);   // Walking there, a tile at a time.
            const x = ox + (f.x + 0.5 + mx + tx + wx) * tile, y = oy + (f.y + 0.5 + my + ty + wy) * tile;
            const self = f.id === s.selfId;
            const color = colorOf(f);
            if (f.acting && !b.over) {
                c.beginPath();
                c.arc(x, y, radius + 5, 0, Math.PI * 2);
                c.strokeStyle = css(withAlpha(Amber, 0.35 + 0.45 * Math.abs(Math.sin(s.clock * 3))));
                c.lineWidth = 2;
                c.stroke();
            }
            // Turning by dragging: the way it will face, shown as the drag goes.
            const facing = self && s.faceDrag && s.faceDragDir >= 0 ? s.faceDragDir : f.facing;
            this.drawToken({...f, facing}, [x, y], tile, color, f.status === 'yielded' ? 0.4 : f.away ? 0.55 : 1, true, s.fx.flash(f.id, s.clock),
                s.fx.lagHealth(f.id, f.health, s.clock));
            // Its name (not one's own: the gold token is oneself), where it fits.
            if (!self) {
                const label = f.name;
                const size = tile >= 24 ? 11 : 10;
                const [nw] = p.measure(label, size, true);
                const spots: [number, number][] = [[x - nw / 2, y - radius - 22], [x - nw / 2, y + radius + 8], [x + radius + 10, y - 7],
                    [x - radius - 10 - nw, y - 7], [x - nw / 2, y - radius - 36], [x - nw / 2, y + radius + 22]];
                const spot = spots.find(([lx, ly]) => !overlaps(rect(lx - 3, ly, lx + nw + 3, ly + 14))) ?? spots[0];
                placed.push(rect(spot[0] - 3, spot[1], spot[0] + nw + 3, spot[1] + 14));
                p.box(spot[0] - 3, spot[1], nw + 6, 14, withAlpha(Ink, 0.7));
                p.text(spot[0], spot[1], label, size, withAlpha(color, f.status === 'fighting' ? 0.95 : 0.6), true);
            }
            // Pointed at from the fight screen (a card or the turn order), or the foe its actions aim at: ringed.
            if (s.highlight === f.id || (s.fightFocus === f.id && f.status === 'fighting')) {
                c.beginPath();
                c.arc(x, y, radius + 5, 0, Math.PI * 2);
                c.setLineDash(s.highlight === f.id ? [] : [4, 3]);
                c.strokeStyle = css(withAlpha(s.highlight === f.id ? Paper : color, 0.9));
                c.lineWidth = 2;
                c.stroke();
                c.setLineDash([]);
            }
            const hit = Math.max(14, radius + 4);
            s.hits.push({rect: rect(x - hit, y - hit, x + hit, y + hit), action: 'fighter', target: f.id});
        }
        if (planned) say(planned.at, planned.word, Amber, 1, false);
        // On one's own turn, one's own tile turns the wolf: split three by three, each of its eight outer parts faces
        // that way. Shown when the pointer is on it (a drag from it turns the wolf too); a click on any tile or wolf
        // round it still moves or strikes there. Pushed last, so it wins over one's own wolf.
        if (me && mine && me.status === 'fighting' && s.aiming !== 'flame') {
            const x0 = ox + me.x * tile, y0 = oy + me.y * tile, third = tile / 3;
            const over = hx === me.x && hy === me.y;
            for (let dir = 0; dir < 8; ++dir) {
                const angle = dir * Math.PI / 4;
                const gx = Math.round(Math.cos(angle)), gy = Math.round(Math.sin(angle));
                const r = rect(x0 + (gx + 1) * third, y0 + (gy + 1) * third, x0 + (gx + 2) * third, y0 + (gy + 2) * third);
                if (over && !s.faceDrag) {
                    const near = contains(r, s.hover[0], s.hover[1]);
                    if (near) p.box(r.left, r.top, third, third, withAlpha(Amber, 0.35));
                    this.turnedText((r.left + r.right) / 2, (r.top + r.bottom) / 2, '>', Math.max(8, Math.round(third * 0.8)),
                        withAlpha(dir === me.facing ? Amber : Paper, dir === me.facing || near ? 1 : 0.6), angle);
                }
                s.hits.push({rect: r, action: 'face', target: String(dir)});
            }
        }
        // Fire filling its cone: a glow under the flames.
        for (const t of s.fx.fire(s.clock))
            p.box(ox + t.x * tile + 1, oy + t.y * tile + 1, tile - 2, tile - 2, withAlpha(rgb(0xff7a2c), 0.15 + 0.4 * t.heat));
        for (const m of s.fx.marks(b, s.clock, reduced)) this.drawMark(m, ox, oy, tile);
        // A fall: rings bursting out from where they went down.
        for (const ring of s.fx.rings(s.clock, reduced)) {
            const [rx, ry] = centre(ring.x, ring.y);
            c.beginPath();
            c.arc(rx, ry, ring.radius * tile, 0, Math.PI * 2);
            c.strokeStyle = css(withAlpha(rgb(0xff9a3c), 0.8 * ring.alpha));
            c.lineWidth = 3;
            c.stroke();
        }
        // The figures rising off the wolves: what each blow did.
        const floatColors = {hit: Paper, heavy: rgb(0xfff0c8), graze: rgb(0xb8ad98), miss: Muted, fire: rgb(0xff9a3c), down: rgb(0xff6a4c),
            heal: rgb(0x9fd08f)};
        for (const f of s.fx.floats(s.clock, reduced)) {
            const [fx, fy] = centre(f.x, f.y);
            const size = Math.round(clamp(tile * 0.5, 12, 18) * f.size);
            c.save();
            c.font = font(size, true);
            c.textAlign = 'center';
            c.textBaseline = 'middle';
            c.lineWidth = 3;
            c.strokeStyle = css(withAlpha(Ink, 0.9 * f.alpha));
            c.strokeText(f.text, fx, fy);
            c.fillStyle = css(withAlpha(floatColors[f.color], f.alpha));
            c.fillText(f.text, fx, fy);
            c.restore();
        }
        // What the others just did, a word or two under each: "steps in", "bites".
        for (const cap of s.fx.captions(s.clock)) {
            const f = b.fighters.find(o => o.id === cap.id);
            if (!f || f.status === 'fled') continue;
            const [wx, wy] = s.walkOffset(f.id, f.x, f.y, s.clock);
            say(centre(f.x + wx, f.y + wy), cap.text, colorOf(f), cap.alpha, true);
        }
        // Over: the arena fades out (how it ended is the fight screen's result card: combat.ts).
        if (b.over) {
            const fade = clamp((s.clock - s.battleOverSeenAt - 2) / 0.5, 0, 1);
            if (fade > 0) p.box(map.left, map.top, map.right - map.left, map.bottom - map.top, withAlpha(Ink, fade));
        }
    }

    /**
     * One's plan for the next turn (doc 37, phase 5), drawn while one's bar fills: the way to the tile planned and a
     * ghost there; the action in a word over it; a planned blow's line to its foe (stepping in first, if out of reach),
     * and planned fire's cone.
     */
    private drawPlan(b: BattleView, me: FighterView, centre: (x: number, y: number) => Point, tile: number, range: number): {at: Point; word: string} {
        const p = this.p, c = p.ctx, plan = b.plan!;
        const strike = plan.act === 'bite' || plan.act === 'sword';
        const foe = b.fighters.find(f => f.id === plan.target);
        let at: [number, number] = plan.move ?? [me.x, me.y];
        if (!plan.move && strike && foe && apart(me.x, me.y, foe.x, foe.y) > range) {
            const step = stepToward(b, me, foe, range);
            if (step) at = [step.x, step.y];
        }
        const moved = at[0] !== me.x || at[1] !== me.y;
        if (moved) {
            c.save();
            c.setLineDash([4, 4]);
            p.lines(pathTo(b, [me.x, me.y], at).map(([x, y]) => centre(x, y)), withAlpha(Amber, 0.75), 2);
            c.restore();
            this.drawToken({...me, x: at[0], y: at[1]}, centre(at[0], at[1]), tile, Amber, 0.5, false);
        }
        const from = centre(at[0], at[1]);
        if (strike && foe) {
            c.save();
            c.setLineDash([5, 4]);
            p.lines([from, centre(foe.x, foe.y)], withAlpha(rgb(0xe0695e), 0.75), 2);
            c.restore();
        }
        if (plan.act === 'flame' && b.flame) {
            const [tx, ty] = plan.target.split(',').map(Number);
            for (const [x, y] of coneTiles(b, at[0], at[1], tx, ty, b.flame.length, b.flame.angle)) {
                const [cx, cy] = centre(x, y);
                p.frame(cx - tile / 2 + 2, cy - tile / 2 + 2, tile - 4, tile - 4, withAlpha(rgb(0xff9a3c), 0.6));
            }
        }
        // The action, in a word, said over where it will be done (once the names are placed).
        const words: Record<string, string> = {bite: 'BITE', sword: 'STRIKE', flame: 'FIRE', tend: 'TEND', roll: 'ROLL', rest: 'REST', hold: 'TAKE SWORD',
            stow: 'STOW', pickup: 'PICK UP', flee: 'FLEE'};
        return {at: from, word: plan.act ? `NEXT · ${words[plan.act] ?? plan.act.toUpperCase()}` : 'NEXT'};
    }

    /**
     * A fighter's token: a dark disc rimmed in its side's colour, its `W`, a wedge on the rim where it faces, and a
     * ring round it for its health. Downed, it lies on its side and pulses; dead, grey. `ring` false for a ghost.
     */
    private drawToken(f: FighterView, [x, y]: Point, tile: number, color: Color, alpha: number, ring: boolean, flash = 0, was = f.health) {
        const c = this.p.ctx;
        const r = clamp(tile * 0.38, 8, 16);
        const fallen = f.status === 'downed' || f.status === 'dead';
        const rim = fallen ? this.fallenColor(f.status) : color;
        c.save();
        c.globalAlpha = alpha;
        c.beginPath();
        c.arc(x, y, r, 0, Math.PI * 2);
        c.fillStyle = css(withAlpha(Ink, 0.92));
        c.fill();
        c.strokeStyle = css(rim);
        c.lineWidth = 2;
        c.stroke();
        if (ring && f.status !== 'dead') {
            const share = Math.max(0, Math.min(1, f.health / 100));
            c.beginPath();
            c.arc(x, y, r + 3, -Math.PI / 2, Math.PI * 1.5);
            c.strokeStyle = css(withAlpha(Ink, 0.8));
            c.lineWidth = 3;
            c.stroke();
            // What the last blow took, draining away (light), ahead of what is left.
            const before = Math.max(share, Math.min(1, was / 100));
            if (before > share) {
                c.beginPath();
                c.arc(x, y, r + 3, -Math.PI / 2 + share * Math.PI * 2, -Math.PI / 2 + before * Math.PI * 2);
                c.strokeStyle = css(withAlpha(Paper, 0.85));
                c.stroke();
            }
            if (share > 0) {
                c.beginPath();
                c.arc(x, y, r + 3, -Math.PI / 2, -Math.PI / 2 + share * Math.PI * 2);
                c.strokeStyle = css(share > 0.5 ? withAlpha(rgb(0x9fd08f), 0.9) : share > 0.25 ? withAlpha(Amber, 0.95) : withAlpha(rgb(0xe0695e), 0.95));
                c.stroke();
            }
        }
        if (flash > 0) {
            // Struck: a flash of white over the disc.
            c.beginPath();
            c.arc(x, y, r, 0, Math.PI * 2);
            c.fillStyle = css(withAlpha(rgb(0xfff4dc), 0.85 * flash));
            c.fill();
        }
        if (!fallen) {
            // The wedge: where it faces.
            const a = f.facing * Math.PI / 4;
            c.beginPath();
            c.moveTo(x + Math.cos(a) * (r + 7), y + Math.sin(a) * (r + 7));
            c.lineTo(x + Math.cos(a + 0.42) * (r - 1), y + Math.sin(a + 0.42) * (r - 1));
            c.lineTo(x + Math.cos(a - 0.42) * (r - 1), y + Math.sin(a - 0.42) * (r - 1));
            c.closePath();
            c.fillStyle = css(color);
            c.fill();
        }
        c.restore();
        const size = Math.round(r * 1.15);
        if (fallen) this.turnedText(x, y, 'W', size, rim, Math.PI / 2);
        else {
            this.turnedText(x, y, 'W', size, withAlpha(color, alpha), 0);
            if (f.mouth === 'sword') this.turnedText(x + r * 0.95, y - r * 0.95, '†', Math.max(9, Math.round(r * 0.9)), withAlpha(Paper, alpha), 0.6);
        }
    }

    /** A blow's chance in a badge beside a foe ("81%", and "side" or "behind" when it helps); −1: out of reach. */
    private chanceBadge([x, y]: Point, tile: number, chance: number, from: 'front' | 'side' | 'back', placed?: Rect[]) {
        const p = this.p;
        const words = chance < 0 ? 'out of reach' : `${Math.round(chance)}%${from === 'back' ? ' · behind' : from === 'side' ? ' · side' : ''}`;
        const size = 12;
        const [w] = p.measure(words, size, true);
        const r = clamp(tile * 0.38, 8, 16);
        const bx = x + r + 6, by = y - r - 18;
        p.box(bx - 4, by - 1, w + 8, 17, withAlpha(Ink, 0.92));
        p.frame(bx - 4, by - 1, w + 8, 17, withAlpha(chance < 0 ? Muted : Amber, 0.8));
        p.text(bx, by, words, size, chance < 0 ? Muted : Amber, true);
        placed?.push(rect(bx - 4, by - 1, bx + w + 4, by + 16));
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

    private fightView: {cx: number; cy: number; tile: number} | null = null;

    /**
     * In a fight the view frames the fight, not the wolf's frozen place in the lineup: oneself (where one stands in
     * the arena), everyone within a dozen tiles and at least the nearest foe, with the tiles one can reach, zoomed in
     * as close as that allows. It eases from frame to frame, so a step doesn't jolt it.
     */
    private fightFrame(mapW: number, mapH: number): {cx: number; cy: number; tile: number} | null {
        const s = this.s, b = s.battle;
        if (!b) {
            this.fightView = null;
            return null;
        }
        const standing = b.fighters.filter(f => f.status !== 'fled');
        const me = standing.find(f => f.id === s.selfId);
        let group = standing;
        if (me) {
            const apart = (f: {x: number; y: number}) => Math.max(Math.abs(f.x - me.x), Math.abs(f.y - me.y));
            group = standing.filter(f => apart(f) <= 12);
            const foe = standing.filter(f => f.side !== me.side && f.status === 'fighting').sort((a, c) => apart(a) - apart(c))[0];
            if (foe && !group.includes(foe)) group.push(foe);
        }
        const tiles: [number, number][] = group.map(f => [f.x, f.y]);
        if (me && myTurn(b, s.selfId)) tiles.push(...b.reach);
        if (!tiles.length) tiles.push([b.arena.x + b.arena.w / 2, b.arena.y + b.arena.h / 2]);
        const xs = tiles.map(t => t[0]), ys = tiles.map(t => t[1]);
        const x0 = Math.min(...xs), x1 = Math.max(...xs) + 1, y0 = Math.min(...ys), y1 = Math.max(...ys) + 1;
        // Room round the edge for names and bars, and at the top for the turn banner.
        const tile = clamp(Math.min(mapW * 0.95 / (x1 - x0 + 6), mapH * 0.95 / (y1 - y0 + 8)), 12, 34);
        const want = {cx: (x0 + x1) / 2, cy: (y0 + y1) / 2 - 1, tile};
        const v = this.fightView;
        const ease = (a: number, c: number) => Math.abs(c - a) < 0.01 ? c : a + (c - a) * 0.15;
        this.fightView = !v ? want : {cx: ease(v.cx, want.cx), cy: ease(v.cy, want.cy), tile: ease(v.tile, want.tile)};
        return this.fightView;
    }

    private drawLocal() {
        const s = this.s, p = this.p;
        const map = s.mapRect;
        const mapW = map.right - map.left, mapH = map.bottom - map.top;
        // A small room fills the map (up to 28 pixels a tile); anything larger shows at the chosen zoom.
        const fit = Math.min(mapW * 0.92 / s.cellWidth, mapH * 0.9 / s.cellHeight);
        s.tileSize = Math.max(ZoomTiles[s.zoom] ?? 22, Math.min(28, fit));
        const fight = this.fightFrame(mapW, mapH);
        if (fight) s.tileSize = fight.tile;
        // A cell that fits is centred; a larger one follows the wolf, stopping at its edges so no empty canvas
        // shows. Shift/Ctrl + wheel look around (mapPan) until the wolf next moves.
        const viewX = (map.left + map.right) / 2, viewY = (map.top + map.bottom) / 2;
        const me = s.entities.get(s.selfId);
        const axis = (center: number, low: number, high: number, tiles: number, self: number) => {
            const span = tiles * s.tileSize;
            if (span <= high - low || !me) return center - span * 0.5;
            return clamp(center - self * s.tileSize, high - span, low);
        };
        const [shakeX, shakeY] = fight ? s.fx.shake(s.clock, s.reducedMotion) : [0, 0];
        s.mapOrigin = fight ? [viewX - fight.cx * s.tileSize + s.mapPan[0] + shakeX, viewY - fight.cy * s.tileSize + s.mapPan[1] + shakeY]
            : [axis(viewX, map.left, map.right, s.cellWidth, me?.x ?? 0) + s.mapPan[0],
                axis(viewY, map.top, map.bottom, s.cellHeight, me?.y ?? 0) + s.mapPan[1]];
        // A crossing keeps the wolf where it was on screen for a moment, then the view slides to where it belongs.
        const cellKey = `${s.cellId}|${s.cellGeneration}`;
        if (me) {
            const screen: Point = [s.mapOrigin[0] + me.x * s.tileSize, s.mapOrigin[1] + me.y * s.tileSize];
            if (this.lastCell && this.lastCell !== cellKey && this.lastSelfScreen)
                s.cameraShift = [clamp(this.lastSelfScreen[0] - screen[0], -600, 600), clamp(this.lastSelfScreen[1] - screen[1], -400, 400)];
            this.lastCell = cellKey;
            this.lastSelfScreen = [screen[0] + s.cameraShift[0], screen[1] + s.cameraShift[1]];
        }
        s.mapOrigin = [s.mapOrigin[0] + s.cameraShift[0], s.mapOrigin[1] + s.cameraShift[1]];
        const [ox, oy] = s.mapOrigin, tile = s.tileSize;
        const cell = obj(s.snapshot, 'cell');
        this.drawEnvironment(false);
        // Only the tiles on screen (and a margin, kept offscreen) are drawn: a large cell has tens of thousands more.
        this.terrain.draw(p, s, {x0: Math.max(0, Math.floor((map.left - ox) / tile) - 1), y0: Math.max(0, Math.floor((map.top - oy) / tile) - 1),
            x1: Math.min(s.cellWidth - 1, Math.ceil((map.right - ox) / tile) + 1),
            y1: Math.min(s.tileRows.length - 1, Math.ceil((map.bottom - oy) / tile) + 1)}, ox, oy, tile);
        // Actions come only from doors and wolves the server shows.
        for (const door of objects(s.snapshot, 'doors')) {
            const x = ox + num(door, 'x') * tile, y = oy + num(door, 'y') * tile;
            p.box(x - 8, y - 10, 17, 22, Ink);
            p.text(x - 7, y - 10, bool(door, 'open') ? '/' : '+', 17, Amber, true);
            p.frame(x - 11, y - 13, 23, 27, withAlpha(Amber, 0.25));
            s.hits.push({rect: rect(x - 15, y - 16, x + 15, y + 16), action: 'target', target: str(door, 'id')});
        }
        // A Chapter's camps, Halls and Holds (doc 32, 5.7): its colour; dim while being built; grey in ruin.
        for (const st of objects(s.snapshot, 'structures')) {
            const x = ox + (num(st, 'x') + 0.5) * tile, y = oy + (num(st, 'y') + 0.5) * tile;
            const hex = str(st, 'colour');
            const base = bool(st, 'ruin') ? Muted : /^#[0-9a-f]{6}$/i.test(hex) ? rgb(parseInt(hex.slice(1), 16)) : Amber;
            const alpha = bool(st, 'built') ? 0.35 + 0.65 * Math.max(0, num(st, 'condition')) / 100 : 0.35;
            const size = clamp(Math.round(tile * 0.7), 11, 17);
            const [gw, gh] = p.measure(str(st, 'glyph', '?'), size, true);
            p.text(x - gw / 2, y - gh / 2, str(st, 'glyph', '?'), size, withAlpha(base, alpha), true);
            if (!bool(st, 'built')) p.frame(x - tile * 0.45, y - tile * 0.45, tile * 0.9, tile * 0.9, withAlpha(base, 0.3));
        }
        // A home's stores (doc 36): larder, chest, wardrobe, woodpile; whose and what's in it when the pointer is near.
        for (const st of objects(s.snapshot, 'stores')) {
            const x = ox + num(st, 'x') * tile, y = oy + num(st, 'y') * tile;
            const glyph = StoreGlyphs[str(st, 'kind')] ?? '?';
            const size = clamp(Math.round(tile * 0.6), 10, 15);
            const [gw, gh] = p.measure(glyph, size, true);
            p.text(x - gw / 2, y - gh / 2, glyph, size, withAlpha(Amber, 0.7), true);
            if (Math.hypot(x - s.hover[0], y - s.hover[1]) < 14) {
                const kind = str(st, 'kind');
                const label = `${str(st, 'owner')} ${kind} · ${str(st, 'contents')}`;
                const [lw] = p.measure(label, 9, false);
                p.box(x + 10, y - 9, lw + 10, 18, Panel);
                p.text(x + 15, y - 5, label, 9, Paper);
            }
        }
        const resource = s.visibleResource();
        if (resource) {
            const x = ox + num(resource, 'x') * tile, y = oy + num(resource, 'y') * tile;
            p.text(x - 8, y - 12, '"', 19, wholeCount(resource, 'remaining') > 0 ? Sage : withAlpha(Muted, 0.5), true);
            s.hits.push({rect: rect(x - 14, y - 14, x + 14, y + 14), action: 'target', target: 'herb_patch'});
        }
        const me2 = s.entities.get(s.selfId);
        if (me2) this.drawScent(ox + me2.x * tile, oy + me2.y * tile);
        let hovered = '';
        // Back from a fight, everyone fades in where it left them (doc 33).
        const fadeIn = clamp((s.clock - s.fightEndedAt) / 0.75, 0, 1);
        for (const view of s.battle ? [] : s.entities.values()) {
            const x = ox + view.x * tile, y = oy + view.y * tile;
            // Who they are to this wolf (doc 32): a party mate in the player's own amber, anyone hostile in red.
            const chapterColour = view.rel === 'chapter' && /^#[0-9a-f]{6}$/i.test(view.colour) ? rgb(parseInt(view.colour.slice(1), 16)) : null;
            const color = withAlpha(view.self || view.rel === 'party' ? Amber : view.hostile ? HostileRed : chapterColour ?? (view.kind === 'npc' ? Sage : Blue),
                fadeIn);
            if (view.self) {
                p.frame(x - 17, y - 17, 34, 34, withAlpha(Amber, 0.22 * fadeIn));
                p.box(x - 9, y - 10, 18, 21, Ink);
            }
            const wolfFont = clamp(Math.round(tile * 0.55), 10, 13);
            const reach = Math.min(13, tile * 0.55);
            if (view.state === 'downed' || view.state === 'dead') {
                // Downed: a sideways W flashing orange. Dead: a sideways W in gray.
                this.turnedText(x, y, 'W', wolfFont, withAlpha(this.fallenColor(view.state), fadeIn), Math.PI / 2);
            } else {
                const [ww, wh] = p.measure('W', wolfFont, true);
                p.text(x - ww * 0.5, y - wh * 0.5, 'W', wolfFont, color, true);
                this.turnedText(x + Math.cos(view.facing) * reach, y + Math.sin(view.facing) * reach, '>', 10, color, view.facing);
            }
            // Colour is never the only cue: a dot under a party mate, a "!" over anyone hostile.
            if (view.rel === 'party') p.box(x - 2, y + 11, 4, 3, color);
            else if (view.rel === 'chapter') p.box(x - 6, y + 12, 12, 1.5, color);     // A Chapter mate: underlined.
            else if (view.hostile && !view.self) p.text(x - 3, y - 25, '!', 12, color, true);
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
            // Spoken to: a dashed gold ring that stays while they are chosen.
            if (s.talkTargets.includes(view.id)) {
                const c = p.ctx;
                c.save();
                c.setLineDash?.([4, 3]);
                c.beginPath();
                c.arc(x, y, 15, 0, Math.PI * 2);
                c.strokeStyle = css(withAlpha(Amber, 0.85));
                c.lineWidth = 1.6;
                c.stroke();
                c.restore();
            }
            // Pointed at in the In Sight list: a ring, so a name finds its wolf.
            if (s.highlight === view.id) {
                p.ctx.beginPath();
                p.ctx.arc(x, y, 19, 0, Math.PI * 2);
                p.ctx.strokeStyle = css(withAlpha(color, 0.8));
                p.ctx.lineWidth = 2;
                p.ctx.stroke();
            }
            if (Math.hypot(x - s.hover[0], y - s.hover[1]) < 20) hovered = view.id;
        }
        s.hoveredEntity = hovered;
        if (s.battle) this.drawArena(s.battle, ox, oy, tile);
        else this.drawFights(ox, oy, tile);
        this.drawEnvironment(true);
        // Words on the map, kept to its corners: wind and height top left, travel and turning along the bottom.
        const L = map.left + 14, T = map.top + 12, B = map.bottom;
        p.text(L, T, 'N ^', 10, Muted, true);
        if (!s.battle) {                                            // (In a fight, the turn order has the top.)
            p.text(L + 46, T, windLabel(s.outdoors, s.windStrength, s.windDirection, s.windVariable), 9, Muted);
            p.text(L, T + 18, elevationLabel(s.selfHeight()), 8, Muted, true);
        }
        const travel = obj(s.snapshot, 'travel');
        if (bool(travel, 'active') || bool(travel, 'paused')) {
            const w = mapW - 28;
            p.box(L, B - 84, w, 37, Panel);
            p.text(L + 10, B - 72, `TRAVEL · ${str(travel, 'status', 'Following your route')}`.slice(0, Math.max(20, Math.floor((w - 130) / 6.5))), 10,
                bool(travel, 'paused') ? Amber : Sage);
            const sx = L + w - 100;
            p.box(sx, B - 80, 92, 28, Raised);
            p.text(sx + 9, B - 72, 'STOP · ESC', 9, Paper, true);
            s.hits.push({rect: rect(sx, B - 80, sx + 92, B - 52), action: 'cancel_travel', target: ''});
        }
        if (s.facingPreview && s.canFaceAt(s.hover)) p.text(L, B - 30, 'ALT · CLICK TO TURN', 9, Amber, true);
        p.text(map.right - 110, B - 30, `LOCAL  /  Z ${Math.trunc(num(cell, 'z'))}`, 9, Muted, true);
    }

    // ------------------------------------------------------------------ The world map and the travel atlas

    /** The travel atlas, in the old frame, with its tabs (the nearby map is drawCountry). */
    private drawWorld() {
        const s = this.s, p = this.p;
        const tab = (x: number, label: string, action: string, active: boolean) => {
            p.box(x, 216, 130, 30, active ? Raised : Panel);
            p.frame(x, 216, 130, 30, active ? Sage : Line);
            p.text(x + 12, 224, label, 9, active ? Sage : Muted, true);
            s.hits.push({rect: rect(x, 216, x + 130, 246), action, target: ''});
        };
        tab(1250, 'NEARBY', 'nearby', !s.travelAtlas);
        tab(1390, 'KNOWN ROUTES', 'atlas', s.travelAtlas);
        this.drawTravelAtlas();
    }

    private drawTravelAtlas() {
        const s = this.s, p = this.p, extra = 0;
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
}
