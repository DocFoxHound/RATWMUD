import {useEffect, useMemo, useRef, useState} from 'react';
import {CELL_SIZES} from '../lib/cellPlacement';
import * as M from '../model/model.mjs';
import {BUILDINGS} from '../model/templates.mjs';
import {COMMANDS, type Command} from '../lib/commands';
import {glyphInfo, glyphLabel, hotkeyGlyph, HEIGHT_CHOICES, ROLE_INFO, type HeightBrush} from '../lib/glyphs';
import {TerrainPalette} from './TerrainPalette';
import {useGlyphRender} from '../lib/glyphFont';
import {formatHeight, tileHeight} from '../lib/elevation';
import {getState, isRosterDirty, openView, selectThing, setState, setWorkspace, TOOL_SPACE, useStore, useTool, type MapSpace, type Tool, type Workspace} from '../lib/store';
import {placeLabel, surfaceFor} from '../lib/surface';

// --------------------------------------------------------------------------- Menu bar

const MENUS = ['File', 'Edit', 'View', 'World', 'Interiors', 'People', 'Tools', 'Help'] as const;

export function MenuBar() {
    const [open, setOpen] = useState<string | null>(null);
    const ref = useRef<HTMLDivElement>(null);
    useStore(s => s.selection); useStore(s => s.past.length); useStore(s => s.overlays); useStore(s => s.tool); useGlyphRender(); // Re-render enabled/checked states.
    useEffect(() => {
        const close = (e: PointerEvent) => { if (!ref.current?.contains(e.target as Node)) setOpen(null); };
        window.addEventListener('pointerdown', close);
        return () => window.removeEventListener('pointerdown', close);
    }, []);
    const run = (c: Command) => { setOpen(null); if (!c.enabled || c.enabled()) c.run(); };
    return (
        <div className="menubar" ref={ref} role="menubar">
            {MENUS.map(menu => {
                const items = COMMANDS.filter(c => c.menu === menu);
                return (
                    <div key={menu} className="menu">
                        <button role="menuitem" aria-haspopup="true" aria-expanded={open === menu} className={open === menu ? 'open' : ''}
                            onClick={() => setOpen(open === menu ? null : menu)} onPointerEnter={() => open && setOpen(menu)}>{menu}</button>
                        {open === menu && <div className="menu-pop" role="menu">
                            {items.map((c, i) => <div key={c.id}>
                                {i > 0 && items[i - 1].group !== c.group && <hr />}
                                <button role="menuitem" disabled={c.enabled ? !c.enabled() : false} onClick={() => run(c)}>
                                    <span className="check">{c.checked?.() ? '✓' : ''}</span>
                                    <span className="label">{c.label}</span>
                                    {c.keys && <kbd>{c.keys}</kbd>}
                                </button>
                            </div>)}
                        </div>}
                    </div>
                );
            })}
        </div>
    );
}

// --------------------------------------------------------------------------- Top bar

export function TopBar() {
    const name = useStore(s => s.project.name);
    const problems = useStore(s => s.project);
    const checks = useMemo(() => M.validate(problems), [problems]);
    return (
        <header className="topbar">
            <div className="brand"><span className="mark">W<i>›</i></span><div><b>ATLAS WORKSHOP</b><small>Runs Against the World</small></div></div>
            <MenuBar />
            <WorkspaceTabs />
            <div className="doc-title">
                <strong>{name}</strong>
                <LiveStatusLine />
            </div>
            <div className="top-actions">
                <EditorsOnline />
                <button className={checks.errors.length ? 'status-chip bad' : 'status-chip ok'} onClick={() => setState({selection: null})}
                    title="Open the Problems panel">{checks.errors.length ? `${checks.errors.length} problem${checks.errors.length > 1 ? 's' : ''}` : 'Ready to play'}</button>
                <button className="publish" onClick={() => setState({dialog: 'publish'})} title="Publish the DEV world to PROD (password needed)">⇪ Push to live</button>
                <button className="play" onClick={() => setState({dialog: 'play'})} title="Ctrl+Enter">▶ Play</button>
            </div>
        </header>
    );
}

/** Whether this editor's changes have reached the database. */
function LiveStatusLine() {
    const live = useStore(s => s.live);
    const me = useStore(s => s.identity);
    const text = live.state === 'connecting' ? 'Connecting to the DEV database…'
        : live.state === 'offline' ? `Offline, retrying${live.pending ? ` · ${live.pending} change${live.pending > 1 ? 's' : ''} waiting` : ''}`
        : live.state === 'saving' ? `Saving${live.pending > 1 ? ` ${live.pending} changes` : ''}…` : 'All changes saved';
    return <small className={`live-line ${live.state}`} title={live.error || 'Every edit is saved to the DEV database as you make it.'}>
        <i />{text}{me ? ` · editing as ${me.name}` : ''}</small>;
}

/** Everyone else editing right now: a dot in their color, and where they are. */
function EditorsOnline() {
    const editors = useStore(s => s.editors);
    const project = useStore(s => s.project);
    if (!editors.length) return null;
    const where = (e: typeof editors[number]) => {
        const view = e.state.view && e.state.view !== 'world' ? M.getCell(project, e.state.view)?.name ?? e.state.view : 'the world overview';
        return `${e.editor}: ${e.state.workspace ?? 'map'} · ${view}${e.state.doing ? ` · on ${e.state.doing}` : ''}`;
    };
    return <div className="editors-online" aria-label={`${editors.length} other editor${editors.length > 1 ? 's' : ''} online`}>
        {editors.map(e => <button key={e.clientId} className="editor-dot" style={{background: e.color}} title={where(e)}
            onClick={() => { if (e.state.view && e.state.view !== 'world') openView({kind: 'cell', id: e.state.view}); else openView({kind: 'world'}); }}>
            {e.editor.trim().slice(0, 1).toUpperCase()}</button>)}
    </div>;
}

function WorkspaceTabs() {
    const workspace = useStore(s => s.workspace);
    const rosterDirty = useStore(isRosterDirty);
    const count = useStore(s => s.roster?.characters.length ?? 0);
    const people = useStore(s => s.project.people.length + s.project.slots.length);
    const rooms = useStore(s => s.project.rooms.length);
    const tab = (id: Workspace, label: string, badge: string, title: string) =>
        <button role="tab" aria-selected={workspace === id} className={workspace === id ? 'on' : ''} title={title} onClick={() => setWorkspace(id)}>{label}{badge && <em>{badge}</em>}</button>;
    return <div className="workspace-tabs" role="tablist">
        {tab('map', '▦ Map', '', 'World terrain, cells, buildings, doors, spawn and resources (Alt+1)')}
        {tab('interiors', '▣ Interiors', String(rooms), 'The insides of buildings, caves and other detached rooms (Alt+2)')}
        {tab('people', 'W People', String(people), 'Who lives and works in this world: named NPCs, profession slots, patrols, economy (Alt+3)')}
        {tab('characters', '☺ Characters', `${count}${rosterDirty ? ' •' : ''}`, 'The shared character roster and profession catalog (Alt+4)')}
    </div>;
}

// --------------------------------------------------------------------------- Tools

export const TOOLS: {id: Tool; icon: string; label: string; key: string; group: number}[] = [
    {id: 'select', icon: '↖', label: 'Select & move', key: 'V', group: 0},
    {id: 'pan', icon: '✥', label: 'Pan', key: 'M', group: 0},
    {id: 'brush', icon: '✎', label: 'Brush', key: 'B', group: 1},
    {id: 'rect', icon: '▭', label: 'Rectangle', key: 'R', group: 1},
    {id: 'line', icon: '╱', label: 'Line', key: 'L', group: 1},
    {id: 'fill', icon: '◧', label: 'Fill', key: 'G', group: 1},
    {id: 'pick', icon: '⊙', label: 'Eyedropper', key: 'I', group: 1},
    {id: 'height', icon: '≋', label: 'Elevation', key: 'H', group: 1},
    {id: 'building', icon: '⌂', label: 'Building', key: 'U', group: 2},
    {id: 'cell', icon: '▢', label: 'New cell', key: 'C', group: 2},
    {id: 'door', icon: '⇄', label: 'Connect', key: 'D', group: 2},
    {id: 'person', icon: 'W', label: 'Named NPC', key: 'P', group: 3},
    {id: 'slot', icon: '◇', label: 'Profession slot', key: 'J', group: 3},
    {id: 'post', icon: '⟲', label: 'Patrol post', key: 'O', group: 3},
    {id: 'spawn', icon: '★', label: 'Player spawn', key: 'N', group: 4},
    {id: 'herb', icon: '❦', label: 'Resource: herb patch', key: 'K', group: 4},
];

export function ToolRail() {
    const tool = useStore(s => s.tool);
    const workspace = useStore(s => s.workspace);
    const tools = TOOLS.filter(t => TOOL_SPACE[t.id].includes(workspace as MapSpace));
    return (
        <div className="toolrail" role="toolbar" aria-label={`${{map: 'Map', interiors: 'Interior', people: 'People', characters: ''}[workspace]} tools`} aria-orientation="vertical">
            {tools.map((t, i) => <div key={t.id}>
                {i > 0 && tools[i - 1].group !== t.group && <hr />}
                <button className={tool === t.id ? 'active' : ''} title={`${t.label} (${t.key})`} aria-label={t.label} aria-pressed={tool === t.id}
                    onClick={() => useTool(t.id)}><span>{t.icon}</span></button>
            </div>)}
        </div>
    );
}

export function ToolOptions() {
    const tool = useStore(s => s.tool);
    const brush = useStore(s => s.brush);
    const height = useStore(s => s.height);
    const filled = useStore(s => s.filled);
    const role = useStore(s => s.role);
    const building = useStore(s => s.building);
    const side = useStore(s => s.side);
    const cellSize = useStore(s => s.cellSize);
    const view = useStore(s => s.view);
    const project = useStore(s => s.project);
    const workspace = useStore(s => s.workspace);
    const surface = surfaceFor(project, view);
    const tip = TOOLS.find(t => t.id === tool)!;
    const palette = <TerrainPalette />;
    const sizes = <label className="inline">Size <select value={brush} onChange={e => setState({brush: Number(e.target.value)})}>
        {[1, 2, 3, 5, 8, 12].map(n => <option key={n} value={n}>{n}×{n}</option>)}</select></label>;
    return (
        <div className="tool-options">
            <div className="crumbs">
                {workspace === 'interiors' ? <span className="crumb-label">Interiors</span>
                    : <button onClick={() => openView({kind: 'world'})} className={view.kind === 'world' ? 'here' : ''}>{project.name}</button>}
                {surface.cell && <><span>›</span><button className="here">{surface.kind === 'room' ? '▣ ' : '▦ '}{surface.cell.name}</button></>}
            </div>
            <div className="options">
                <b className="tool-name">{tip.icon} {tip.label}</b>
                {['brush', 'rect', 'line', 'fill'].includes(tool) && palette}
                {(tool === 'brush' || tool === 'height') && sizes}
                {tool === 'rect' && <label className="inline"><input type="checkbox" checked={filled} onChange={e => setState({filled: e.target.checked})} /> Filled</label>}
                {tool === 'height' && <label className="inline">Height <select value={String(height)} onChange={e => setState({height: parseHeightBrush(e.target.value)})}>
                    {HEIGHT_CHOICES.map(h => <option key={String(h.value)} value={String(h.value)}>{h.label}</option>)}</select></label>}
                {tool === 'person' && <div className="segmented">{(['civilian', 'guard', 'merchant'] as const).map(r =>
                    <button key={r} className={role === r ? 'on' : ''} onClick={() => setState({role: r})} style={{color: role === r ? ROLE_INFO[r].color : undefined}}>{ROLE_INFO[r].icon} {ROLE_INFO[r].label}</button>)}</div>}
                {tool === 'slot' && <SlotProfessionPicker />}
                {tool === 'cell' && <>
                    <select value={`${cellSize.w}x${cellSize.h}`} aria-label="Cell size" onChange={e => { const [w, h] = e.target.value.split('x').map(Number); setState({cellSize: {w, h}}); }}>
                        {CELL_SIZES.map(g => <optgroup key={g.group} label={g.group}>{g.sizes.map(s => <option key={`${s.w}x${s.h}`} value={`${s.w}x${s.h}`}>{s.w} × {s.h}</option>)}</optgroup>)}
                        {!CELL_SIZES.some(g => g.sizes.some(s => s.w === cellSize.w && s.h === cellSize.h)) && <option value={`${cellSize.w}x${cellSize.h}`}>{cellSize.w} × {cellSize.h}</option>}
                    </select>
                    <button title="Swap width and height (X)" onClick={() => setState({cellSize: {w: cellSize.h, h: cellSize.w}})}>↻ Turn</button>
                </>}
                {tool === 'building' && <>
                    <select value={building} onChange={e => setState({building: e.target.value})} aria-label="Building type">
                        {BUILDINGS.map(b => <option key={b.id} value={b.id}>{b.name} · {b.footprint[0]}×{b.footprint[1]}</option>)}</select>
                    <div className="segmented"><button className={side === 'S' ? 'on' : ''} onClick={() => setState({side: 'S'})}>Door ↓ south</button>
                        <button className={side === 'N' ? 'on' : ''} onClick={() => setState({side: 'N'})}>Door ↑ north</button></div>
                </>}
                <span className="tool-help">{workspace === 'interiors' && tool === 'select'
                    ? 'Click to select · drag door markers to move · double-click a door to walk through it · right-click for more' : HELP[tool]}</span>
            </div>
        </div>
    );
}

const parseHeightBrush = (v: string): HeightBrush => (v === 'null' ? null : v === 'raise' || v === 'lower' ? v : Number(v));

const HELP: Record<Tool, string> = {
    select: 'Click to select · drag markers to move · double-click a cell to open it · right-click for more',
    pan: 'Drag to pan (or hold Space / middle mouse with any tool)',
    brush: 'Drag to paint · [ and ] change size · 1–0 and - pick terrain (Shift for more) · All terrain lists every tile',
    rect: 'Drag a rectangle', line: 'Drag a line', fill: 'Click to fill a same-terrain area', pick: 'Click a tile to copy its terrain',
    height: 'Drag to set elevation in half steps, or raise / lower by ½; “Glyph default” clears it · E cycles the relief view',
    building: 'Click a world cell to place a building with its interior and door',
    cell: 'Click to place a cell of the chosen size; it snaps beside the cells already there (or to a 16-tile grid) · X turns it · right-click or Esc cancels',
    door: 'Click one end, then the other end in another cell or interior (open it from the explorer)',
    person: 'Click to place a named NPC you design by hand; then set their home and evening places',
    slot: 'Click to place jobs, as many as you like; each is filled from the character roster at export · V or Esc when done',
    post: 'Click to add posts to the selected patrol route, in walking order · with no route selected, a click starts a new route',
    spawn: 'Click where new characters appear',
    herb: 'Click to move the herb patch: the one spot where players Gather herbs (a shared stock that regrows daily)',
};

function SlotProfessionPicker() {
    const roster = useStore(s => s.roster);
    const profession = useStore(s => s.profession);
    if (!roster) return <span className="tool-help">Loading the roster…</span>;
    if (!roster.professions.length) return <button onClick={() => setWorkspace('characters')}>Add professions in Characters first</button>;
    return <select aria-label="Profession" value={profession} onChange={e => setState({profession: e.target.value})}>
        {roster.professions.map(p => <option key={p.id} value={p.id}>{p.name}</option>)}</select>;
}

// --------------------------------------------------------------------------- Status, problems, toasts

export function StatusBar() {
    const hover = useStore(s => s.hover);
    const project = useStore(s => s.project);
    const view = useStore(s => s.view);
    const surface = surfaceFor(project, view);
    const glyph = hover ? surface.glyph(hover.x, hover.y) : null;
    return (
        <footer className="statusbar">
            <span title={surface.kind === 'room' ? 'Interior tile' : 'World tile'}>{hover
                ? surface.kind === 'room' ? `${hover.x}, ${hover.y}` : `${hover.x + surface.ox}, ${hover.y + surface.oy}` : '—'}</span>
            <span>{hover?.place ? placeLabel(project, hover.place) : surface.title}</span>
            <span title="Tile height: an explicit override, or the terrain's default">{hover && glyph ? heightReadout(glyph, surface.heightAt(hover.x, hover.y)) : ''}</span>
            <span title="The hovered tile's terrain (catalog name and stored code)">{glyph ? `${glyphLabel(glyphInfo(glyph))} · ${glyphInfo(glyph).effect}` : ''}</span>
            <span className="grow" />
            <span>{project.people.length} residents · {project.cells.length + project.rooms.length} places · {project.links.length} connections</span>
            <button className="link-button" onClick={() => setState({dialog: 'shortcuts'})}>Shortcuts ?</button>
        </footer>
    );
}

/** "Height 1½ (set)" or "Height ½ (default)". */
export const heightReadout = (glyph: string, override: number | undefined) =>
    `Height ${formatHeight(tileHeight(glyph, override))}${override !== undefined ? ' (set)' : ' (default)'}`;

const PROBLEMS_HEIGHT_KEY = 'ratw-problems-height';
const savedProblemsHeight = () => {
    try { const v = Number(localStorage.getItem(PROBLEMS_HEIGHT_KEY)); return Number.isFinite(v) && v >= 60 ? v : 160; } catch { return 160; }
};

export function ProblemsPanel() {
    const project = useStore(s => s.project);
    const {errors, warnings} = useMemo(() => M.validate(project), [project]);
    const [open, setOpen] = useState(true);
    // The list's height when open: drag the top edge to resize it; double-click the edge for compact or tall.
    const [height, setHeight] = useState(savedProblemsHeight);
    const keep = (h: number) => {
        const next = Math.round(Math.max(60, Math.min(window.innerHeight * .7, h)));
        setHeight(next);
        try { localStorage.setItem(PROBLEMS_HEIGHT_KEY, String(next)); } catch { /* private window: not remembered */ }
    };
    const drag = (e: React.PointerEvent<HTMLDivElement>) => {
        e.preventDefault();
        const startY = e.clientY, startHeight = height, handle = e.currentTarget;
        handle.setPointerCapture(e.pointerId);
        const move = (m: PointerEvent) => keep(startHeight - (m.clientY - startY));
        const up = () => { handle.removeEventListener('pointermove', move); handle.removeEventListener('pointerup', up); };
        handle.addEventListener('pointermove', move);
        handle.addEventListener('pointerup', up);
    };
    if (!errors.length && !warnings.length) return null;
    return (
        <div className={open ? 'problems open' : 'problems'} style={open ? {height} : undefined}>
            {open && <div className="problems-resize" role="separator" aria-orientation="horizontal" aria-label="Resize the checks panel"
                title="Drag to resize · double-click for compact or tall" onPointerDown={drag}
                onDoubleClick={() => keep(height > 200 ? 120 : window.innerHeight * .45)} />}
            <button className="problems-head" onClick={() => setOpen(!open)}>
                <b className={errors.length ? 'bad' : 'warn'}>{errors.length ? `${errors.length} must be fixed before export` : 'Checks passed'}</b>
                {warnings.length > 0 && <span>{warnings.length} note{warnings.length > 1 ? 's' : ''}</span>}
                <span className="chev">{open ? '▾' : '▸'}</span>
            </button>
            {open && <ul>
                {errors.map((e, i) => <li key={`e${i}`} className="bad">{e}</li>)}
                {warnings.map((w, i) => <li key={`w${i}`} className="warn">{w}</li>)}
            </ul>}
        </div>
    );
}

export function Toasts() {
    const toasts = useStore(s => s.toasts);
    return <div className="toasts" role="status" aria-live="polite">{toasts.map(t => <div key={t.id} className={`toast ${t.tone}`}>{t.text}</div>)}</div>;
}

// --------------------------------------------------------------------------- Command palette

export function CommandPalette() {
    const open = useStore(s => s.palette);
    const project = useStore(s => s.project);
    const [query, setQuery] = useState('');
    const [index, setIndex] = useState(0);
    useEffect(() => { if (open) { setQuery(''); setIndex(0); } }, [open]);
    const results = useMemo(() => {
        const q = query.trim().toLowerCase();
        const entries: {label: string; detail: string; run: () => void}[] = [
            ...COMMANDS.filter(c => !c.enabled || c.enabled()).map(c => ({label: c.label, detail: `${c.menu}${c.keys ? ' · ' + c.keys : ''}`, run: c.run})),
            ...[...project.cells, ...project.rooms].map(c => ({label: `Go to ${c.name}`, detail: 'Place', run: () => openView({kind: 'cell', id: c.id})})),
            ...project.people.map(p => ({label: p.name, detail: `${ROLE_INFO[p.role].label} · ${p.workLabel}`, run: () => selectThing({kind: 'person', id: p.id})})),
            ...project.routes.map(r => ({label: r.name, detail: 'Patrol route', run: () => selectThing({kind: 'route', id: r.id})})),
        ];
        return (q ? entries.filter(e => (e.label + ' ' + e.detail).toLowerCase().includes(q)) : entries).slice(0, 40);
    }, [query, project]);
    if (!open) return null;
    const close = () => setState({palette: false});
    const go = (i: number) => { const r = results[i]; close(); r?.run(); };
    return (
        <div className="overlay" onPointerDown={e => { if (e.target === e.currentTarget) close(); }}>
            <div className="palette" role="dialog" aria-label="Command palette">
                <input autoFocus value={query} placeholder="Type a command, place or person…" onChange={e => { setQuery(e.target.value); setIndex(0); }}
                    onKeyDown={e => {
                        if (e.key === 'Escape') close();
                        else if (e.key === 'ArrowDown') { e.preventDefault(); setIndex(i => Math.min(results.length - 1, i + 1)); }
                        else if (e.key === 'ArrowUp') { e.preventDefault(); setIndex(i => Math.max(0, i - 1)); }
                        else if (e.key === 'Enter') go(index);
                    }} />
                <div className="palette-list">
                    {results.map((r, i) => <button key={i} className={i === index ? 'on' : ''} onPointerEnter={() => setIndex(i)} onClick={() => go(i)}>
                        <span>{r.label}</span><small>{r.detail}</small></button>)}
                    {!results.length && <p className="hint">Nothing matches.</p>}
                </div>
            </div>
        </div>
    );
}

export function useGlobalKeys(handler: (e: KeyboardEvent) => boolean) {
    useEffect(() => {
        const onKey = (e: KeyboardEvent) => {
            if (handler(e)) return;
            const t = e.target as HTMLElement;
            if (t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.tagName === 'SELECT' || e.ctrlKey || e.metaKey || e.altKey) return;
            const g = hotkeyGlyph(e);
            if (g && ['brush', 'rect', 'line', 'fill'].includes(getState().tool)) { setState({glyph: g.code}); e.preventDefault(); }
            if (e.key === '[') setState(s => ({brush: Math.max(1, s.brush - 1)}));
            if (e.key === ']') setState(s => ({brush: Math.min(12, s.brush + 1)}));
        };
        const warn = (e: BeforeUnloadEvent) => { if (getState().live.pending || isRosterDirty(getState())) e.preventDefault(); };
        window.addEventListener('keydown', onKey);
        window.addEventListener('beforeunload', warn);
        return () => { window.removeEventListener('keydown', onKey); window.removeEventListener('beforeunload', warn); };
    }, [handler]);
}
