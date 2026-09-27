// The Dungeon Master's map: the world or one place, with painted tiles, whole-place fills, route lines and markers.
// Wheel zooms; drag pans (right-drag while painting); click selects or picks; drag paints in paint mode.
import {useEffect, useRef, useState} from 'react';
import type {Place, Project} from '../model/model.mjs';
import type {Surface} from '../lib/surface';
import {drawGround, visibleTiles} from '../lib/ground';
import {drawRelief, formatHeight, nextElevation, surfaceHeight, type ElevationMode} from '../lib/elevation';
import {ElevationControl} from '../components/Elevation';
import {glyphInfo, glyphLabel} from '../lib/glyphs';
import {useGlyphRender} from '../lib/glyphFont';

export type Mode = 'select' | 'pick' | 'paint';
export interface Marker { id: string; x: number; y: number; color: string; glyph: string; label: string; dead: boolean; editable: boolean }
/** Drawn under the markers, in surface tiles: painted tiles, whole-place fills and lines through numbered points. */
export interface Overlay {
    tiles: {x: number; y: number; color: string; strong: boolean}[];
    rects: {x: number; y: number; w: number; h: number; color: string; strong: boolean}[];
    paths: {points: [number, number][]; color: string; numbered: boolean; loop: boolean}[];
}

/** The world or one place, with area tiles, route lines and NPC spawn-point markers.
 *  Wheel zooms; drag pans (right-drag while painting); click selects or picks; drag paints in paint mode. */
export function MapView({surface, markers, overlay, selected, mode, onClick, onPaint, onOpen}: {surface: Surface; world?: Project; markers: Marker[]; overlay: Overlay;
    selected: string | null; mode: Mode; onClick: (place: Place | null, marker: Marker | null) => void; onPaint: (place: Place | null, start: boolean) => void;
    onOpen: (cell: string) => void}) {
    const wrap = useRef<HTMLDivElement>(null), canvas = useRef<HTMLCanvasElement>(null);
    const [size, setSize] = useState({w: 800, h: 600});
    const [cam, setCam] = useState({s: 12, x: 20, y: 20});
    const drag = useRef<{sx: number; sy: number; cx: number; cy: number; moved: boolean} | null>(null);
    const painting = useRef<string | null>(null);        // The last tile painted in this stroke.
    const [hover, setHover] = useState<Marker | null>(null);
    useGlyphRender();                                    // Redraw once the glyph font loads, or when Plain ASCII is toggled.
    // The hovered tile's readout is written straight into its element: hovering must not redraw the map.
    const readoutRef = useRef<HTMLDivElement>(null), tileHover = useRef('');
    const showTile = (at: [number, number] | null) => {
        const text = at && surface.glyph(at[0], at[1]) !== undefined ? readout(surface, at) : '';
        if (text === tileHover.current || !readoutRef.current) return;
        tileHover.current = text; readoutRef.current.textContent = text; readoutRef.current.hidden = !text;
    };
    // Shading with impassable ledges by default: heights decide what players can see and where they can walk.
    const [elevation, setElevation] = useState<ElevationMode>(() => {
        try { const v = localStorage.getItem('ratw-dm-relief'); return v === 'off' || v === 'full' || v === 'shade' ? v : 'shade'; } catch { return 'shade'; }
    });
    const relief = (m: ElevationMode) => { setElevation(m); try { localStorage.setItem('ratw-dm-relief', m); } catch { /* private window */ } };
    useEffect(() => {
        const key = (e: KeyboardEvent) => {
            const t = e.target as HTMLElement | null;
            if (e.key.toLowerCase() !== 'e' || e.ctrlKey || e.metaKey || e.altKey || t && (t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.tagName === 'SELECT' || t.isContentEditable)) return;
            setElevation(m => { const n = nextElevation(m); try { localStorage.setItem('ratw-dm-relief', n); } catch { /* private window */ } return n; });
        };
        window.addEventListener('keydown', key);
        return () => window.removeEventListener('keydown', key);
    }, []);
    useEffect(() => {
        const el = wrap.current!;
        const o = new ResizeObserver(() => setSize({w: el.clientWidth, h: el.clientHeight}));
        o.observe(el);
        return () => o.disconnect();
    }, []);
    useEffect(() => {                                    // Fit whenever the surface changes.
        const s = Math.max(.05, Math.min(48, Math.min((size.w - 40) / surface.width, (size.h - 40) / surface.height)));
        setCam({s, x: (size.w - surface.width * s) / 2 - surface.left * s, y: (size.h - surface.height * s) / 2 - surface.top * s});
    }, [surface.kind, surface.cell?.id, surface.width, surface.height, size.w, size.h]); // eslint-disable-line react-hooks/exhaustive-deps
    useEffect(() => {
        const c = canvas.current!, g = c.getContext('2d')!, dpr = window.devicePixelRatio || 1;
        c.width = size.w * dpr; c.height = size.h * dpr; g.setTransform(dpr, 0, 0, dpr, 0, 0);
        g.fillStyle = '#0d1413'; g.fillRect(0, 0, size.w, size.h);
        const {s, x: ox, y: oy} = cam;
        drawGround(g, surface, cam, size.w, size.h, elevation !== 'off');
        drawRelief(g, surface, cam, size.w, size.h, elevation);
        if (surface.kind === 'world') {
            g.strokeStyle = 'rgba(200,210,190,.35)'; g.lineWidth = 1;
            const view = visibleTiles(cam, size.w, size.h);
            for (const {cell: c} of surface.pieces(view.x0, view.y0, view.x1, view.y1)) {
                if (!('x' in c)) continue;
                g.strokeRect(ox + c.x * s + .5, oy + c.y * s + .5, c.width * s - 1, c.height * s - 1);
                if (c.width * s > 80) { g.font = '600 12px Inter, system-ui, sans-serif'; g.textAlign = 'left'; g.fillStyle = '#cdd6cc'; g.fillText(c.name, ox + c.x * s + 8, oy + c.y * s + 14); }
            }
        }
        for (const r of overlay.rects) {
            g.globalAlpha = r.strong ? .38 : .22; g.fillStyle = r.color;
            g.fillRect(ox + r.x * s, oy + r.y * s, r.w * s, r.h * s);
            g.globalAlpha = r.strong ? .95 : .5; g.strokeStyle = r.color; g.lineWidth = r.strong ? 2 : 1;
            g.strokeRect(ox + r.x * s + 1, oy + r.y * s + 1, r.w * s - 2, r.h * s - 2);
        }
        for (const t of overlay.tiles) {
            g.globalAlpha = t.strong ? .55 : .28; g.fillStyle = t.color;
            g.fillRect(ox + t.x * s, oy + t.y * s, Math.ceil(s), Math.ceil(s));
            if (t.strong && s >= 6) { g.globalAlpha = .9; g.strokeStyle = t.color; g.lineWidth = 1; g.strokeRect(ox + t.x * s + .5, oy + t.y * s + .5, s - 1, s - 1); }
        }
        g.globalAlpha = 1;
        for (const p of overlay.paths) {
            if (p.points.length > 1) {
                g.strokeStyle = p.color; g.lineWidth = p.numbered ? 2.5 : 1.5; g.setLineDash(p.numbered ? [] : [4, 4]);
                g.beginPath(); p.points.forEach(([x, y], i) => i ? g.lineTo(ox + x * s, oy + y * s) : g.moveTo(ox + x * s, oy + y * s));
                if (p.loop && p.points.length > 2) g.closePath();
                g.stroke(); g.setLineDash([]);
            }
            if (!p.numbered) continue;
            const r = Math.max(7, Math.min(12, s * .45));
            p.points.forEach(([x, y], i) => {
                g.beginPath(); g.arc(ox + x * s, oy + y * s, r, 0, Math.PI * 2); g.fillStyle = p.color; g.fill();
                g.font = `700 ${Math.floor(r * 1.1)}px Inter, system-ui, sans-serif`; g.textAlign = 'center'; g.textBaseline = 'middle';
                g.fillStyle = '#11191b'; g.fillText(String(i + 1), ox + x * s, oy + y * s + 1);
            });
        }
        const r = Math.max(5, Math.min(14, s * .45));
        for (const m of markers) {
            const px = ox + m.x * s, py = oy + m.y * s, on = m.id === selected;
            g.globalAlpha = m.dead ? .45 : 1;
            g.beginPath(); g.arc(px, py, r, 0, Math.PI * 2);
            g.fillStyle = m.editable || m.glyph === '◇' ? m.color : 'rgba(13,20,19,.85)'; g.fill();
            g.lineWidth = on ? 3 : 1.5; g.strokeStyle = on ? '#ffffff' : m.color; g.stroke();
            g.font = `700 ${Math.floor(r * 1.1)}px Inter, system-ui, sans-serif`; g.textAlign = 'center'; g.textBaseline = 'middle';
            g.fillStyle = m.editable || m.glyph === '◇' ? '#11191b' : m.color; g.fillText(m.dead ? '✝' : m.glyph, px, py + 1);
            g.globalAlpha = 1;
            if (on || m === hover) {
                g.font = '600 12px Inter, system-ui, sans-serif'; g.textAlign = 'left';
                const w = g.measureText(m.label).width;
                g.fillStyle = 'rgba(13,20,19,.9)'; g.fillRect(px + r + 4, py - 10, w + 10, 20);
                g.fillStyle = '#ded5c3'; g.fillText(m.label, px + r + 9, py);
            }
        }
    });
    const tile = (e: React.MouseEvent) => {
        const b = canvas.current!.getBoundingClientRect();
        return [(e.clientX - b.left - cam.x) / cam.s, (e.clientY - b.top - cam.y) / cam.s] as const;
    };
    const placeAt = (e: React.MouseEvent) => {
        const [tx, ty] = tile(e), x = Math.floor(tx), y = Math.floor(ty);
        return surface.toPlace(x, y);
    };
    const paint = (e: React.MouseEvent, start: boolean) => {
        const place = placeAt(e), key = place ? `${place.cell}:${place.x},${place.y}` : '';
        if (!start && key === painting.current) return;
        painting.current = key;
        onPaint(place, start);
    };
    const markerAt = (e: React.MouseEvent) => {
        const [tx, ty] = tile(e);
        let best: Marker | null = null, d = Math.max(.7, 10 / cam.s);
        for (const m of markers) { const dist = Math.hypot(m.x - tx, m.y - ty); if (dist < d) { best = m; d = dist; } }
        return best;
    };
    return <div className="map-wrap" ref={wrap}>
        <canvas ref={canvas} style={{width: size.w, height: size.h, cursor: mode === 'select' ? 'default' : 'crosshair'}}
            onContextMenu={e => e.preventDefault()}
            onWheel={e => { const b = canvas.current!.getBoundingClientRect(), mx = e.clientX - b.left, my = e.clientY - b.top;
                setCam(c => { const s = Math.max(.2, Math.min(64, c.s * Math.exp(-e.deltaY * .0015))); return {s, x: mx - (mx - c.x) * (s / c.s), y: my - (my - c.y) * (s / c.s)}; }); }}
            onPointerDown={e => {
                if (mode === 'paint' && e.button === 0) { (e.target as HTMLElement).setPointerCapture?.(e.pointerId); paint(e, true); return; }
                drag.current = {sx: e.clientX, sy: e.clientY, cx: cam.x, cy: cam.y, moved: false};
            }}
            onPointerMove={e => {
                const [tx, ty] = tile(e);
                showTile([Math.floor(tx), Math.floor(ty)]);
                if (painting.current !== null && e.buttons & 1) { paint(e, false); return; }
                const d = drag.current;
                if (d && e.buttons) { if (Math.hypot(e.clientX - d.sx, e.clientY - d.sy) > 4) d.moved = true;
                    if (d.moved) setCam(c => ({...c, x: d.cx + e.clientX - d.sx, y: d.cy + e.clientY - d.sy})); }
                else if (mode !== 'paint') { const m = markerAt(e); if (m !== hover) setHover(m); }
            }}
            onPointerUp={e => {
                if (painting.current !== null) { painting.current = null; return; }
                const d = drag.current; drag.current = null;
                if (d?.moved || e.button !== 0 || mode === 'paint') return;
                onClick(placeAt(e), mode === 'pick' ? null : markerAt(e));
            }}
            onDoubleClick={e => { if (surface.kind !== 'world' || mode === 'paint') return; const [tx, ty] = tile(e);
                const c = surface.cellAt(Math.floor(tx), Math.floor(ty)); if (c) onOpen(c.id); }}
            onPointerLeave={() => showTile(null)} />
        <div className="map-readout" ref={readoutRef} hidden aria-live="off" />
        <ElevationControl mode={elevation} onChange={relief} shortcut="E" />
    </div>;
}

/** "world 12, 40 · Stairs (^) · height ½ (set)" for the hovered tile. */
function readout(surface: Surface, [x, y]: [number, number]): string {
    const glyph = surface.glyph(x, y), h = surfaceHeight(surface, x, y), place = surface.toPlace(x, y);
    const where = surface.kind === 'world' ? `world ${x}, ${y}` : place ? `${place.cell} ${place.x}, ${place.y}` : `${x}, ${y}`;
    return `${where} · ${glyph === undefined ? '—' : glyphLabel(glyphInfo(glyph))} · height ${h === undefined ? '—' : formatHeight(h)}${surface.heightAt(x, y) !== undefined ? ' (set)' : ''}`;
}
