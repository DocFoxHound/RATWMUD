import {useCallback, useEffect, useMemo, useRef, useState} from 'react';
import * as M from '../model/model.mjs';
import type {Place, PlaceRef, Project} from '../model/model.mjs';
import {buildingById} from '../model/templates.mjs';
import {flood, line, rect, square, type Tile} from '../lib/geometry';
import {placeCell} from '../lib/cellPlacement';
import {deleteCell} from '../lib/commands';
import {glyphInfo, glyphLabel, ROLE_INFO} from '../lib/glyphs';
import {useGlyphRender} from '../lib/glyphFont';
import {commit, getState, openView, setState, toast, useStore, useTool, type EditorState, type Mark} from '../lib/store';
import type {Editor} from '../lib/live';
import {surfaceFor, type Surface} from '../lib/surface';
import {drawGlyph, drawGround, glyphFont, swatch, visibleTiles, type Camera} from '../lib/ground';
import {drawRelief} from '../lib/elevation';
import {ElevationControl} from './Elevation';

const MARK_MS = 4000;            // How long someone else's edit stays highlighted.

interface Marker { x: number; y: number; kind: string; ref?: PlaceRef; linkId?: string; select?: () => void; label: string; color: string; glyph: string; r: number; faint?: boolean }
type Gesture =
    | {mode: 'pan'; sx: number; sy: number; cx: number; cy: number}
    | {mode: 'stroke'; tiles: Map<string, Tile>; height: boolean; last: Tile}
    | {mode: 'shape'; x0: number; y0: number; x1: number; y1: number}
    | {mode: 'drag'; marker: Marker; x: number; y: number; moved: boolean}
    | null;

export function MapCanvas() {
    const project = useStore(s => s.project);
    const view = useStore(s => s.view);
    const tool = useStore(s => s.tool);
    const selection = useStore(s => s.selection);
    const overlays = useStore(s => s.overlays);
    const elevation = useStore(s => s.elevation);
    const pending = useStore(s => s.pending);
    const fitRequest = useStore(s => s.fitRequest);
    const workspace = useStore(s => s.workspace);
    const editors = useStore(s => s.editors);
    const marks = useStore(s => s.marks);
    const glyphs = useGlyphRender();               // Redraw once the glyph font loads, or when Plain ASCII is toggled.
    // Other people's recent edits fade out over a few seconds; keep redrawing while any are fresh.
    const [fade, setFade] = useState(0);
    useEffect(() => {
        if (!marks.some(m => Date.now() - m.at < MARK_MS)) return;
        const t = setTimeout(() => setFade(f => f + 1), 120);
        return () => clearTimeout(t);
    }, [marks, fade]);
    const cellSize = useStore(s => s.cellSize);
    const opts = {brush: useStore(s => s.brush), filled: useStore(s => s.filled), building: useStore(s => s.building), side: useStore(s => s.side)};
    const surface = useMemo(() => surfaceFor(project, view), [project, view]);

    const wrap = useRef<HTMLDivElement>(null);
    const canvas = useRef<HTMLCanvasElement>(null);
    const [size, setSize] = useState({w: 800, h: 600});
    const [camera, setCamera] = useState<Camera>({s: 16, x: 40, y: 40});
    const [hover, setHover] = useState<Tile | null>(null);
    const [aim, setAim] = useState<Tile | null>(null);            // The pointer's tile, also past the world's edge.
    const [gesture, setGesture] = useState<Gesture>(null);
    const [menu, setMenu] = useState<{sx: number; sy: number; tile: Tile} | null>(null);
    const spaceDown = useRef(false);

    // --- Sizing and fitting -------------------------------------------------
    useEffect(() => {
        const el = wrap.current!;
        const observer = new ResizeObserver(() => setSize({w: el.clientWidth, h: el.clientHeight}));
        observer.observe(el);
        return () => observer.disconnect();
    }, []);
    const fit = useCallback(() => {
        const s = Math.max(.05, Math.min(48, Math.min((size.w - 80) / surface.width, (size.h - 80) / surface.height)));
        setCamera({s, x: (size.w - surface.width * s) / 2 - surface.left * s, y: (size.h - surface.height * s) / 2 - surface.top * s});
    }, [size.w, size.h, surface.width, surface.height, surface.left, surface.top]);
    useEffect(fit, [fitRequest, surface.kind, surface.cell?.id, size.w > 0 && size.h > 0]); // eslint-disable-line react-hooks/exhaustive-deps
    useEffect(() => {
        const down = (e: KeyboardEvent) => { if (e.code === 'Space' && (e.target as HTMLElement).tagName !== 'INPUT' && (e.target as HTMLElement).tagName !== 'TEXTAREA') { spaceDown.current = true; } };
        const up = (e: KeyboardEvent) => { if (e.code === 'Space') spaceDown.current = false; };
        window.addEventListener('keydown', down); window.addEventListener('keyup', up);
        return () => { window.removeEventListener('keydown', down); window.removeEventListener('keyup', up); };
    }, []);

    // --- Markers (people, posts, doors, spawn, herbs) -------------------------
    const markers = useMemo(() => buildMarkers(project, surface, selection, overlays, workspace), [project, surface, selection, overlays, workspace]);

    // --- Tool previews ----------------------------------------------------------
    const preview = useMemo((): {tiles: Tile[]; ghost?: {x: number; y: number; w: number; h: number; ok: boolean; door: Tile}} => {
        if (gesture?.mode === 'stroke') return {tiles: [...gesture.tiles.values()]};
        if (gesture?.mode === 'shape') return {tiles: tool === 'line' ? line(gesture.x0, gesture.y0, gesture.x1, gesture.y1)
            : rect(gesture.x0, gesture.y0, gesture.x1, gesture.y1, opts.filled)};
        if (!hover) return {tiles: []};
        if (tool === 'brush' || tool === 'height') return {tiles: square(hover[0], hover[1], opts.brush)};
        if (tool === 'building') {
            const t = buildingById(opts.building);
            if (!t || surface.kind === 'room') return {tiles: []};
            const [fw, fh] = t.footprint, x = hover[0] - Math.floor(fw / 2), y = hover[1] - Math.floor(fh / 2);
            const door: Tile = [x + Math.floor(fw / 2), opts.side === 'N' ? y : y + fh - 1];
            const home = surface.toPlace(x, y), far = surface.toPlace(x + fw - 1, y + fh - 1);
            return {tiles: [], ghost: {x, y, w: fw, h: fh, door, ok: !!home && !!far && home.cell === far.cell}};
        }
        return {tiles: [hover]};
    }, [gesture, hover, tool, opts.brush, opts.filled, opts.building, opts.side, surface]);

    // Where a new cell would go (canvas tiles), snapped to its neighbours.
    const placement = useMemo(() => tool === 'cell' && aim && surface.kind !== 'room'
        ? placeCell(project, aim[0] + surface.ox, aim[1] + surface.oy, cellSize) : null, [tool, aim, surface, project, cellSize]);

    // --- Drawing ----------------------------------------------------------------
    useEffect(() => {
        const c = canvas.current!;
        const dpr = window.devicePixelRatio || 1;
        if (c.width !== size.w * dpr || c.height !== size.h * dpr) { c.width = size.w * dpr; c.height = size.h * dpr; }
        const g = c.getContext('2d')!;
        g.setTransform(dpr, 0, 0, dpr, 0, 0);
        g.fillStyle = '#0d1413'; g.fillRect(0, 0, size.w, size.h);
        const {s, x: ox, y: oy} = camera;
        // Ground, cell by cell and only what is in view. Where no cell is, there is no ground: the background shows.
        drawGround(g, surface, camera, size.w, size.h, elevation !== 'off');
        drawRelief(g, surface, camera, size.w, size.h, elevation);
        if (surface.kind !== 'world') {
            g.strokeStyle = '#3c4a41'; g.lineWidth = 1;
            g.strokeRect(ox - .5, oy - .5, surface.width * s + 1, surface.height * s + 1);
        }
        if (overlays.grid && s >= 8) {
            const view = visibleTiles(camera, size.w, size.h);
            g.strokeStyle = 'rgba(255,255,255,.05)'; g.beginPath();
            for (const {cell: c, x: px, y: py} of surface.pieces(view.x0, view.y0, view.x1, view.y1)) {
                const x0 = ox + px * s, y0 = oy + py * s;
                for (let x = 0; x <= c.width; x++) { g.moveTo(x0 + x * s, y0); g.lineTo(x0 + x * s, y0 + c.height * s); }
                for (let y = 0; y <= c.height; y++) { g.moveTo(x0, y0 + y * s); g.lineTo(x0 + c.width * s, y0 + y * s); }
            }
            g.stroke();
        }
        if (surface.kind === 'world') drawCells(g, surface, camera, size, selection, overlays, project);
        if (workspace === 'people') drawRoutes(g, project, surface, camera, selection, overlays);
        // Tool previews.
        const st = getState();
        for (const [tx, ty] of preview.tiles) {
            if (surface.glyph(tx, ty) === undefined) continue;
            if (tool === 'height') { g.fillStyle = 'rgba(200,183,226,.35)'; g.fillRect(ox + tx * s, oy + ty * s, s, s); continue; }
            if (['brush', 'rect', 'line'].includes(tool)) {
                const info = glyphInfo(st.glyph);
                g.fillStyle = s >= 7 ? info.bg : swatch(info); g.fillRect(ox + tx * s, oy + ty * s, s, s);
                if (s >= 7) { glyphFont(g, s); drawGlyph(g, info, ox + tx * s, oy + ty * s, s); }
            }
            g.strokeStyle = 'rgba(240,215,154,.9)'; g.lineWidth = 1.5; g.strokeRect(ox + tx * s + .75, oy + ty * s + .75, s - 1.5, s - 1.5);
        }
        if (preview.ghost) {
            const {x, y, w, h, ok, door} = preview.ghost;
            g.fillStyle = ok ? 'rgba(207,198,178,.35)' : 'rgba(239,162,150,.35)';
            g.fillRect(ox + x * s, oy + y * s, w * s, h * s);
            g.strokeStyle = ok ? '#e0c486' : '#efa296'; g.lineWidth = 2; g.strokeRect(ox + x * s, oy + y * s, w * s, h * s);
            g.fillStyle = '#f0d79a'; g.fillRect(ox + door[0] * s, oy + door[1] * s, s, s);
        }
        if (placement) {
            const {w, h, ok} = placement, x = placement.x - surface.ox, y = placement.y - surface.oy;
            g.fillStyle = ok ? 'rgba(143,181,111,.22)' : 'rgba(239,162,150,.25)';
            g.fillRect(ox + x * s, oy + y * s, w * s, h * s);
            g.strokeStyle = ok ? '#8fb56f' : '#efa296'; g.lineWidth = 2; g.setLineDash(placement.touching ? [] : [6, 4]);
            g.strokeRect(ox + x * s, oy + y * s, w * s, h * s); g.setLineDash([]);
            g.font = '600 12px Inter, system-ui, sans-serif'; g.textAlign = 'left'; g.textBaseline = 'top'; g.fillStyle = ok ? '#cfe3bf' : '#efa296';
            g.fillText(`${w} × ${h}${ok ? (placement.touching ? ' · snapped to its neighbour' : ' · on the 16-tile grid') : ` · ${placement.problem}`}`,
                ox + x * s + 4, oy + (y + h) * s + 4);
        }
        drawMarkers(g, markers, camera, hover);
        drawOthers(g, surface, camera, editors, marks);
        if (st.pending?.kind === 'link' && st.pending.a) {
            const at = surface.fromPlace(st.pending.a);
            if (at) { g.strokeStyle = '#f0d79a'; g.setLineDash([4, 3]); g.lineWidth = 2; g.strokeRect(ox + at[0] * s - 2, oy + at[1] * s - 2, s + 4, s + 4); g.setLineDash([]); }
        }
    }, [camera, size, surface, project, selection, overlays, elevation, markers, preview, placement, hover, tool, pending, workspace, editors, marks, fade, glyphs]);

    // --- Pointer input ----------------------------------------------------------
    const tileAt = (e: {clientX: number; clientY: number}): Tile => {
        const r = canvas.current!.getBoundingClientRect();
        return [Math.floor((e.clientX - r.left - camera.x) / camera.s), Math.floor((e.clientY - r.top - camera.y) / camera.s)];
    };
    /** Whether a surface tile has ground (is in a cell or the interior). */
    const inside = ([x, y]: Tile) => surface.glyph(x, y) !== undefined;
    const markerAt = (e: {clientX: number; clientY: number}) => {
        const r = canvas.current!.getBoundingClientRect();
        const px = (e.clientX - r.left - camera.x) / camera.s, py = (e.clientY - r.top - camera.y) / camera.s;
        let best: Marker | null = null, dist = Infinity;
        for (const m of markers) {
            if (m.faint) continue;   // Context from the other workspace is shown, not edited.
            const d = Math.hypot(m.x - px, m.y - py), reach = Math.max(m.r, 10 / camera.s);
            if (d < reach && d < dist) { best = m; dist = d; }
        }
        return best;
    };

    const onWheel = (e: React.WheelEvent) => {
        const r = canvas.current!.getBoundingClientRect();
        const mx = e.clientX - r.left, my = e.clientY - r.top;
        setCamera(c => {
            const s = Math.max(MIN_SCALE, Math.min(64, c.s * Math.exp(-e.deltaY * 0.0015)));
            return {s, x: mx - (mx - c.x) * (s / c.s), y: my - (my - c.y) * (s / c.s)};
        });
    };

    const onPointerDown = (e: React.PointerEvent) => {
        setMenu(null);
        canvas.current!.setPointerCapture(e.pointerId);
        const t = tileAt(e);
        const st = getState();
        if (e.button === 1 || spaceDown.current || tool === 'pan') {
            setGesture({mode: 'pan', sx: e.clientX, sy: e.clientY, cx: camera.x, cy: camera.y});
            return;
        }
        if (e.button !== 0) return;
        const place = inside(t) ? surface.toPlace(t[0], t[1]) : null;
        if (st.pending?.kind === 'place') {
            if (!place) { toast('Choose a tile inside a cell.', 'error'); return; }
            const {ref, label} = st.pending;
            if (commit(`Moved ${label}.`, p => M.movePlace(p, ref, place))) setState({pending: null});
            return;
        }
        if (st.pending?.kind === 'link' || tool === 'door') { connectAt(place); return; }
        switch (tool) {
            case 'select': {
                const m = markerAt(e);
                if (m) { m.select?.(); if (m.ref) setGesture({mode: 'drag', marker: m, x: m.x, y: m.y, moved: false}); return; }
                if (surface.kind === 'world') {
                    const c = surface.cellAt(t[0], t[1]);
                    if (!c) { setState({selection: null}); return; }
                    const prev = st.selection?.kind === 'cells' ? st.selection.ids : [];
                    const ids = e.shiftKey ? (prev.includes(c.id) ? prev.filter(i => i !== c.id) : [...prev, c.id]) : [c.id];
                    setState({selection: ids.length ? {kind: 'cells', ids} : null});
                } else setState({selection: surface.cell ? {kind: 'cells', ids: [surface.cell.id]} : null});
                return;
            }
            case 'brush': case 'height': {
                const tiles = new Map<string, Tile>();
                for (const x of square(t[0], t[1], st.brush)) tiles.set(x.join(), x);
                setGesture({mode: 'stroke', tiles, height: tool === 'height', last: t});
                return;
            }
            case 'rect': case 'line': setGesture({mode: 'shape', x0: t[0], y0: t[1], x1: t[0], y1: t[1]}); return;
            case 'cell': {
                if (surface.kind === 'room') { toast('New cells go on the world map; open the world overview (Home).', 'error'); return; }
                const at = placeCell(project, t[0] + surface.ox, t[1] + surface.oy, st.cellSize);
                if (!at.ok) { toast(`A new cell can’t go here: ${at.problem}.`, 'error'); return; }
                const made = commit('Added a cell. Name it in the inspector.', p => M.addCell(p, {x: at.x, y: at.y, width: at.w, height: at.h}));
                if (made) setState({selection: {kind: 'cells', ids: [made.id]}});
                return;
            }
            case 'fill': {
                if (!inside(t)) return;
                const tiles = flood(surface.glyph, t[0], t[1]);
                paint(tiles, `Filled ${tiles.length} tiles.`);
                return;
            }
            case 'pick': { const g = surface.glyph(t[0], t[1]); if (g) { setState({glyph: g, tool: 'brush'}); toast(`Picked ${glyphLabel(glyphInfo(g))}.`); } return; }
            case 'person': {
                if (!place) { toast('Residents must stand inside a cell.', 'error'); return; }
                const person = commit('Added a resident. Set their home, work and evening places in the inspector.', p => M.upsertPerson(p, M.newPerson(p, st.role, place)));
                if (person) setState({selection: {kind: 'person', id: person.id}, tool: 'select'});
                return;
            }
            case 'slot': {
                if (!place) { toast('Profession slots must be inside a cell.', 'error'); return; }
                const profession = st.roster?.professions.find(p => p.id === st.profession);
                if (!profession) { toast('Add a profession in the Characters workspace first.', 'error'); return; }
                // The tool stays active so several slots can be dropped in a row.
                const made = commit(`Placed a ${profession.name.toLowerCase()} slot.`, p => M.upsertSlot(p, M.newSlot(p, profession, place)));
                if (made) setState({selection: {kind: 'slot', id: made.id}});
                return;
            }
            case 'post': {
                if (!place) { toast('Patrol posts go inside a cell or interior.', 'error'); return; }
                const routeId = st.selection?.kind === 'route' ? st.selection.id : null;
                if (!routeId) {
                    // No route selected: this click starts a new one, with this as its first post.
                    const made = commit('Started a patrol route. Keep clicking to add posts; assign guards in the inspector.',
                        p => M.upsertRoute(p, {name: 'Patrol route', posts: [place]} as never));
                    if (made) setState({selection: {kind: 'route', id: made.id}});
                    return;
                }
                commit('Added a patrol post.', p => { const r = p.routes.find(x => x.id === routeId)!; return M.upsertRoute(p, {...r, posts: [...r.posts, place]}); }, true);
                return;
            }
            case 'spawn': if (place) commit('Moved the player spawn.', p => M.setSpawn(p, place)); return;
            case 'herb': if (place) commit('Placed the herb patch.', p => M.setHerbPatch(p, place)); return;
            case 'building': {
                const g = preview.ghost, t2 = buildingById(st.building);
                if (!g || !t2) { if (surface.kind === 'room') toast('Buildings are placed on the world canvas or a world cell, not inside interiors.', 'error'); return; }
                const origin = surface.toPlace(g.x, g.y);
                if (!origin || !g.ok) { toast('The whole footprint must fit inside one world cell.', 'error'); return; }
                const made = commit(`Built a ${t2.name.toLowerCase()} with its interior and door.`, p => M.addBuilding(p, t2, origin.cell, origin.x, origin.y, st.side));
                if (made) {
                    setState({selection: {kind: 'cells', ids: [made.roomId]}});
                    const extra = [made.beds.length && `${made.beds.length} bed${made.beds.length > 1 ? 's' : ''}`, made.counter && 'a shop counter'].filter(Boolean).join(' and ');
                    if (extra) toast(`The interior has ${extra}. Use “Residents here” in the inspector to move people in.`);
                }
                return;
            }
        }
    };

    const onPointerMove = (e: React.PointerEvent) => {
        const t = tileAt(e);
        if (tool === 'cell' && (!aim || aim[0] !== t[0] || aim[1] !== t[1])) setAim(t);
        const inTile = inside(t) ? t : null;
        if (!hover || !inTile || hover[0] !== inTile[0] || hover[1] !== inTile[1]) {
            setHover(inTile);
            setState({hover: inTile ? {x: inTile[0], y: inTile[1], place: surface.toPlace(inTile[0], inTile[1])} : null});
        }
        if (!gesture) return;
        if (gesture.mode === 'pan') setCamera(c => ({...c, x: gesture.cx + e.clientX - gesture.sx, y: gesture.cy + e.clientY - gesture.sy}));
        else if (gesture.mode === 'stroke') {
            // Fill the gap since the last pointer sample, so fast strokes stay continuous.
            if (gesture.last[0] === t[0] && gesture.last[1] === t[1]) return;
            const tiles = new Map(gesture.tiles);
            for (const [lx, ly] of line(gesture.last[0], gesture.last[1], t[0], t[1]))
                for (const x of square(lx, ly, getState().brush)) tiles.set(x.join(), x);
            setGesture({...gesture, tiles, last: t});
        } else if (gesture.mode === 'shape' && (gesture.x1 !== t[0] || gesture.y1 !== t[1])) setGesture({...gesture, x1: t[0], y1: t[1]});
        else if (gesture.mode === 'drag' && inTile && (Math.floor(gesture.x) !== t[0] || Math.floor(gesture.y) !== t[1]))
            setGesture({...gesture, x: t[0] + .5, y: t[1] + .5, moved: true});
    };

    const onPointerUp = () => {
        const g = gesture;
        setGesture(null);
        if (!g) return;
        if (g.mode === 'stroke') {
            const tiles = [...g.tiles.values()];
            if (g.height) heights(tiles);
            else paint(tiles, null);
        } else if (g.mode === 'shape') {
            const tiles = tool === 'line' ? line(g.x0, g.y0, g.x1, g.y1) : rect(g.x0, g.y0, g.x1, g.y1, getState().filled);
            paint(tiles, null);
        } else if (g.mode === 'drag' && g.moved && g.marker.ref) {
            const place = surface.toPlace(Math.floor(g.x), Math.floor(g.y));
            if (place) commit(`Moved ${g.marker.label}.`, p => M.movePlace(p, g.marker.ref!, place));
        }
    };

    const onDoubleClick = (e: React.MouseEvent) => {
        const t = tileAt(e);
        const m = markerAt(e);
        const link = m?.linkId ? project.links.find(l => l.id === m.linkId) : undefined;
        if (link && surface.cell) {
            // Walk through the door: show the other side.
            const other = link.a.cell === surface.cell.id ? link.b : link.a;
            openView({kind: 'cell', id: other.cell});
            return;
        }
        if (surface.kind === 'world') {
            const c = surface.cellAt(t[0], t[1]);
            if (c) openView({kind: 'cell', id: c.id});
        }
    };

    // Model edits, translated from surface tiles to the paint target. The world is the ground its cells hold:
    // tiles outside every cell have no ground and are not painted.
    function paint(tiles: Tile[], label: string | null) {
        const clipped = tiles.filter(inside).map(([x, y]) => [x + surface.ox, y + surface.oy] as Tile);
        if (!clipped.length) return;
        const glyph = getState().glyph;
        commit(label ?? `Painted ${clipped.length} tile${clipped.length > 1 ? 's' : ''}.`, p => M.paintTiles(p, clipped, glyph, surface.roomId), true);
    }
    function heights(tiles: Tile[]) {
        const clipped = tiles.filter(inside).map(([x, y]) => [x + surface.ox, y + surface.oy] as Tile);
        const value = getState().height;
        if (!clipped.length) return;
        if (value === 'raise' || value === 'lower') commit(value === 'raise' ? 'Raised the ground by ½.' : 'Lowered the ground by ½.',
            p => M.adjustHeights(p, clipped, value === 'raise' ? .5 : -.5, surface.roomId), true);
        else commit('Set elevation.', p => M.setHeights(p, clipped, value, surface.roomId), true);
    }
    function connectAt(place: Place | null) {
        const st = getState();
        if (!place) { toast('Choose a tile inside a cell or interior.', 'error'); return; }
        const pend = st.pending?.kind === 'link' ? st.pending : {kind: 'link' as const, a: null, linkKind: 'door' as const};
        if (!pend.a) {
            setState({pending: {...pend, a: place}});
            toast('First end chosen. Now open the other cell or interior and click the second end.');
            return;
        }
        if (pend.a.cell === place.cell) { toast('The two ends must be in different cells or interiors.', 'error'); return; }
        const a = pend.a;
        const link = commit('Connected the two places. Doors show as + in game.', p => M.addLink(p, {kind: pend.linkKind, name: pend.linkKind === 'stairs' ? 'Stairs' : 'Door', a, b: place, open: pend.linkKind !== 'door'}));
        if (link) setState({pending: null, selection: {kind: 'link', id: link.id}, tool: 'select'});
    }

    // --- Context menu -----------------------------------------------------------
    const onContextMenu = (e: React.MouseEvent) => {
        e.preventDefault();
        // Right-click while placing a new cell cancels it.
        if (tool === 'cell') { setAim(null); useTool('select'); toast('New cell cancelled.'); return; }
        const t = tileAt(e);
        if (inside(t)) {
            const r = wrap.current!.getBoundingClientRect();
            setMenu({sx: e.clientX - r.left, sy: e.clientY - r.top, tile: t});
        }
    };
    const menuPlace = menu ? surface.toPlace(menu.tile[0], menu.tile[1]) : null;
    const menuDoor = menuPlace ? project.links.find(l => (l.a.cell === menuPlace.cell && l.a.x === menuPlace.x && l.a.y === menuPlace.y)
        || (l.b.cell === menuPlace.cell && l.b.x === menuPlace.x && l.b.y === menuPlace.y)) : undefined;

    const cursor = gesture?.mode === 'pan' || tool === 'pan' ? 'grabbing'
        : gesture?.mode === 'drag' ? 'move' : tool === 'select' ? 'default' : tool === 'pick' ? 'copy' : 'crosshair';

    return (
        <div className="map-wrap" ref={wrap}>
            <canvas ref={canvas} style={{width: size.w, height: size.h, cursor}} tabIndex={0}
                aria-label={`Map of ${surface.title}. Use the explorer and inspector for keyboard access.`}
                onWheel={onWheel} onPointerDown={onPointerDown} onPointerMove={onPointerMove} onPointerUp={onPointerUp}
                onPointerLeave={() => { setHover(null); setAim(null); setState({hover: null}); }}
                onDoubleClick={onDoubleClick} onContextMenu={onContextMenu} />
            {pending && <div className="map-banner">
                {pending.kind === 'place' ? `Click the map to place ${pending.label}.` : pending.a ? 'Now click the second end (switch cells in the explorer if needed).' : 'Click the first end of the connection.'}
                <button onClick={() => setState({pending: null})}>Cancel</button>
            </div>}
            <div className="zoom-box">
                <button onClick={() => setCamera(c => zoomAt(c, size, 1 / 1.25))} aria-label="Zoom out">−</button>
                <span>{Math.round(camera.s)} px</span>
                <button onClick={() => setCamera(c => zoomAt(c, size, 1.25))} aria-label="Zoom in">+</button>
                <button onClick={fit}>Fit</button>
            </div>
            <ElevationControl mode={elevation} onChange={m => setState({elevation: m})} shortcut="E" />
            {menu && menuPlace && <div className="context-menu" style={{left: menu.sx, top: menu.sy}} onPointerLeave={() => setMenu(null)}>
                <div className="context-title">{menuPlace.cell} · {menuPlace.x}, {menuPlace.y}</div>
                {menuDoor && <button onClick={() => { const other = menuDoor.a.cell === menuPlace.cell ? menuDoor.b : menuDoor.a; openView({kind: 'cell', id: other.cell}); setState({selection: {kind: 'link', id: menuDoor.id}}); setMenu(null); }}>Go through “{menuDoor.name}”</button>}
                {surface.kind === 'world' && <button onClick={() => { openView({kind: 'cell', id: menuPlace.cell}); setMenu(null); }}>Open this cell</button>}
                {workspace === 'map' && surface.kind !== 'room' && <button className="danger-text" onClick={() => { setMenu(null); deleteCell(menuPlace.cell); }}>Delete this cell…</button>}
                {workspace === 'people' && getState().roster?.professions.map(prof => <button key={prof.id} onClick={() => {
                    const made = commit(`Placed a ${prof.name.toLowerCase()} slot.`, p => M.upsertSlot(p, M.newSlot(p, prof, menuPlace)));
                    if (made) setState({selection: {kind: 'slot', id: made.id}});
                    setMenu(null);
                }}>◇ {prof.name} slot here</button>).slice(0, 4)}
                {workspace === 'people' && (['civilian', 'guard', 'merchant'] as const).map(role => <button key={role} onClick={() => {
                    const person = commit(`Added a ${ROLE_INFO[role].label.toLowerCase()}.`, p => M.upsertPerson(p, M.newPerson(p, role, menuPlace)));
                    if (person) setState({selection: {kind: 'person', id: person.id}});
                    setMenu(null);
                }}>Add {ROLE_INFO[role].label.toLowerCase()} here</button>)}
                {workspace === 'map' && <>
                    <button onClick={() => { commit('Moved the player spawn.', p => M.setSpawn(p, menuPlace)); setMenu(null); }}>Set player spawn here</button>
                    <button onClick={() => { commit('Placed the herb patch.', p => M.setHerbPatch(p, menuPlace)); setMenu(null); }}>Put herb patch here</button>
                </>}
                {(workspace === 'map' || workspace === 'interiors') && <>
                    <button onClick={() => { setState({pending: {kind: 'link', a: menuPlace, linkKind: 'door'}}); setMenu(null); toast('Now click the other end of the door.'); }}>Start a door here</button>
                    <button onClick={() => { setState({glyph: surface.glyph(menu.tile[0], menu.tile[1]) ?? getState().glyph}); setMenu(null); }}>Pick this terrain</button>
                </>}
            </div>}
        </div>
    );
}

/** Screen pixels per tile when zoomed all the way out: a whole 2560-tile world fits a laptop screen. */
const MIN_SCALE = .2;

function zoomAt(c: Camera, size: {w: number; h: number}, factor: number): Camera {
    const s = Math.max(MIN_SCALE, Math.min(64, c.s * factor)), mx = size.w / 2, my = size.h / 2;
    return {s, x: mx - (mx - c.x) * (s / c.s), y: my - (my - c.y) * (s / c.s)};
}

/** Everyone else: where their pointer is (if they are looking at this surface) and what they just changed. */
function drawOthers(g: CanvasRenderingContext2D, surface: Surface, cam: Camera, editors: Editor[], marks: Mark[]) {
    const now = Date.now();
    for (const m of marks) {
        const age = now - m.at;
        if (age > MARK_MS) continue;
        let x = m.x, y = m.y;
        if (m.area === 'world') {
            if (surface.kind === 'room') continue;
            x -= surface.ox; y -= surface.oy;
        } else if (surface.kind !== 'room' || surface.cell?.id !== m.area) continue;
        g.globalAlpha = 1 - age / MARK_MS;
        g.strokeStyle = m.color; g.lineWidth = 2;
        g.strokeRect(cam.x + x * cam.s + 1, cam.y + y * cam.s + 1, m.w * cam.s - 2, m.h * cam.s - 2);
    }
    g.globalAlpha = 1;
    for (const e of editors) {
        const place = e.state.place, at = place && e.state.workspace !== 'characters' ? surface.fromPlace(place) : null;
        if (!at) continue;
        const x = cam.x + at[0] * cam.s, y = cam.y + at[1] * cam.s;
        g.strokeStyle = e.color; g.lineWidth = 2.5; g.strokeRect(x - 1, y - 1, cam.s + 2, cam.s + 2);
        g.font = '600 11px Inter, system-ui, sans-serif'; g.textAlign = 'left'; g.textBaseline = 'bottom';
        const label = e.editor, tw = g.measureText(label).width;
        g.fillStyle = e.color; g.fillRect(x, y - 17, tw + 10, 16);
        g.fillStyle = '#11191b'; g.fillText(label, x + 5, y - 3);
    }
}

/** Cell outlines, names and territory tints on the world overview (cells in view only). */
function drawCells(g: CanvasRenderingContext2D, surface: Surface, cam: Camera, size: {w: number; h: number}, selection: EditorState['selection'],
    overlays: EditorState['overlays'], project: Project) {
    const selected = new Set(selection?.kind === 'cells' ? selection.ids : []);
    const factions = new Map(project.factions.map(f => [f.id, f.color]));
    const view = visibleTiles(cam, size.w, size.h);
    for (const {cell: c, x: cx, y: cy} of surface.pieces(view.x0, view.y0, view.x1, view.y1)) {
        const x = cam.x + cx * cam.s, y = cam.y + cy * cam.s, w = c.width * cam.s, h = c.height * cam.s;
        if (overlays.territory && c.territory.claims.length) {
            g.fillStyle = (factions.get(c.territory.claims[0]) ?? '#888888') + '30'; g.fillRect(x, y, w, h);
        }
        if (overlays.cuts || selected.has(c.id)) {
            g.setLineDash(selected.has(c.id) ? [] : [5, 4]);
            g.strokeStyle = selected.has(c.id) ? '#e6c481' : 'rgba(200,210,190,.35)'; g.lineWidth = selected.has(c.id) ? 2.5 : 1;
            g.strokeRect(x + .5, y + .5, w - 1, h - 1); g.setLineDash([]);
            if (w > 70) {
                g.font = '600 12px Inter, system-ui, sans-serif'; g.textAlign = 'left'; g.textBaseline = 'top';
                const label = c.name, tw = g.measureText(label).width;
                g.fillStyle = 'rgba(13,20,19,.78)'; g.fillRect(x + 6, y + 6, tw + 12, 20);
                g.fillStyle = selected.has(c.id) ? '#f0d79a' : '#cdd6cc'; g.fillText(label, x + 12, y + 10);
            }
        }
    }
}

function drawRoutes(g: CanvasRenderingContext2D, project: Project, surface: Surface, cam: Camera, selection: EditorState['selection'], overlays: EditorState['overlays']) {
    for (const r of project.routes) {
        const active = selection?.kind === 'route' && selection.id === r.id;
        if (!overlays.routes && !active) continue;
        const pts = r.posts.map(p => surface.fromPlace(p));
        g.strokeStyle = active ? 'rgba(168,206,218,.95)' : 'rgba(168,206,218,.35)'; g.lineWidth = active ? 2 : 1.25;
        g.setLineDash([6, 4]); g.beginPath();
        let first = true;
        for (const [i, pt] of pts.entries()) {
            const next = pts[(i + 1) % pts.length];
            if (!pt || !next || pts.length < 2) { first = true; continue; }
            if (first) g.moveTo(cam.x + (pt[0] + .5) * cam.s, cam.y + (pt[1] + .5) * cam.s);
            g.lineTo(cam.x + (next[0] + .5) * cam.s, cam.y + (next[1] + .5) * cam.s);
            first = false;
        }
        g.stroke(); g.setLineDash([]);
    }
}

function buildMarkers(project: Project, surface: Surface, selection: EditorState['selection'], overlays: EditorState['overlays'], workspace: EditorState['workspace']): Marker[] {
    const out: Marker[] = [];
    // Each workspace edits its own markers and shows the others' faintly for context.
    const owners: Record<string, EditorState['workspace'][]> = {link: ['map', 'interiors'], spawn: ['map'], herb: ['map']};
    const add = (p: Place | null, m: Omit<Marker, 'x' | 'y'>) => {
        const at = p && surface.fromPlace(p);
        if (at) out.push({...m, x: at[0] + .5, y: at[1] + .5, faint: !(owners[m.kind] ?? ['people']).includes(workspace)});
    };
    if (overlays.links || selection?.kind === 'link') for (const l of project.links) for (const [end, other] of [[l.a, l.b], [l.b, l.a]] as const)
        add(end, {kind: 'link', linkId: l.id, label: `${l.name} → ${other.cell}`, color: '#f0d79a', glyph: l.kind === 'stairs' ? '^' : l.kind === 'passage' ? '≡' : '+', r: .6,
            select: () => setState({selection: {kind: 'link', id: l.id}})});
    if (overlays.links || selection?.kind === 'spawn') add(project.spawn, {kind: 'spawn', ref: {kind: 'spawn'}, label: 'player spawn', color: '#e6c481', glyph: '★', r: .6, select: () => setState({selection: {kind: 'spawn'}})});
    if (overlays.links || selection?.kind === 'herb') add(project.herbPatch, {kind: 'herb', ref: {kind: 'herbPatch'}, label: 'herb patch', color: '#8fb56f', glyph: '❦', r: .6, select: () => setState({selection: {kind: 'herb'}})});
    for (const r of project.routes) {
        const active = selection?.kind === 'route' && selection.id === r.id;
        if ((!overlays.routes && !active) || workspace !== 'people') continue;
        r.posts.forEach((post, index) => add(post, {kind: 'post', ref: {kind: 'post', id: r.id, index}, label: `${r.name} post ${index + 1}`, color: active ? '#a8ceda' : '#6f95a3', glyph: String(index + 1), r: .5,
            select: () => setState({selection: {kind: 'route', id: r.id}})}));
    }
    if (overlays.people || selection?.kind === 'slot') for (const s of project.slots) {
        const active = selection?.kind === 'slot' && selection.id === s.id, select = () => setState({selection: {kind: 'slot', id: s.id}});
        if (active) {
            add(s.home, {kind: 'home', ref: {kind: 'slot', id: s.id, slot: 'home'}, label: `${s.name}: home`, color: '#b6a3cf', glyph: '⌂', r: .55, select});
            add(s.evening, {kind: 'evening', ref: {kind: 'slot', id: s.id, slot: 'evening'}, label: `${s.name}: evening`, color: '#b6a3cf', glyph: '☾', r: .55, select});
        }
        add(s.work, {kind: active ? 'slot-active' : 'slot', ref: {kind: 'slot', id: s.id, slot: 'work'}, label: `${s.name} (profession slot)`, color: '#b6a3cf', glyph: '◇', r: .7, select});
    }
    if (overlays.people || selection?.kind === 'person') for (const p of project.people) {
        const active = selection?.kind === 'person' && selection.id === p.id;
        const color = ROLE_INFO[p.role].color, select = () => setState({selection: {kind: 'person', id: p.id}});
        if (active) {
            add(p.home, {kind: 'home', ref: {kind: 'person', id: p.id, slot: 'home'}, label: `${p.name}'s home`, color, glyph: '⌂', r: .55, select});
            add(p.evening, {kind: 'evening', ref: {kind: 'person', id: p.id, slot: 'evening'}, label: `${p.name}'s evening place`, color, glyph: '☾', r: .55, select});
        }
        add(p.work, {kind: active ? 'person-active' : 'person', ref: {kind: 'person', id: p.id, slot: 'work'}, label: `${p.name}'s work place`, color, glyph: 'W', r: .7, select});
    }
    return out;
}

function drawMarkers(g: CanvasRenderingContext2D, markers: Marker[], cam: Camera, hover: Tile | null) {
    const r = Math.max(5, Math.min(16, cam.s * .45));
    for (const m of [...markers].sort((a, b) => Number(b.faint ?? false) - Number(a.faint ?? false))) {
        g.globalAlpha = m.faint ? .32 : 1;
        const x = cam.x + m.x * cam.s, y = cam.y + m.y * cam.s;
        g.beginPath();
        if (m.kind === 'link') { g.moveTo(x, y - r); g.lineTo(x + r, y); g.lineTo(x, y + r); g.lineTo(x - r, y); g.closePath(); }
        else g.arc(x, y, m.kind === 'post' ? r * .8 : r, 0, Math.PI * 2);
        g.fillStyle = m.kind === 'link' || m.kind === 'post' ? 'rgba(13,20,19,.85)' : m.color;
        g.fill();
        const active = m.kind === 'person-active' || m.kind === 'slot-active';
        if (m.kind.startsWith('slot')) g.setLineDash([3, 2]);
        g.lineWidth = active ? 3 : 1.5; g.strokeStyle = active ? '#ffffff' : m.color; g.stroke(); g.setLineDash([]);
        if (r >= 6) {
            g.font = `700 ${Math.floor(r * 1.15)}px Inter, system-ui, sans-serif`; g.textAlign = 'center'; g.textBaseline = 'middle';
            g.fillStyle = m.kind === 'link' || m.kind === 'post' ? m.color : '#11191b'; g.fillText(m.glyph, x, y + 1);
        }
        const hovered = !m.faint && hover && Math.floor(m.x) === hover[0] && Math.floor(m.y) === hover[1];
        if (hovered || m.kind === 'person-active' || m.kind === 'slot-active') {
            g.font = '600 12px Inter, system-ui, sans-serif'; g.textAlign = 'left'; g.textBaseline = 'middle';
            const tw = g.measureText(m.label).width;
            g.fillStyle = 'rgba(13,20,19,.9)'; g.fillRect(x + r + 4, y - 10, tw + 10, 20);
            g.fillStyle = '#ded5c3'; g.fillText(m.label, x + r + 9, y);
        }
    }
    g.globalAlpha = 1;
}

