// Every user action lives here once. Menus, the command palette and keyboard
// shortcuts all read this list, so they never drift apart.
import * as M from '../model/model.mjs';
import {api, download} from './api';
import {ELEVATION_LABEL, ELEVATION_MODES, nextElevation} from './elevation';
import {glyphRender, setPlainAscii} from './glyphFont';
import {allGround, anyPending} from './lazyGround';
import {commit, getState, isRoomView, openView, redo, setState, setWorkspace, toast, undo, useTool, type Tool} from './store';

export interface Command {
    id: string;
    label: string;
    menu: 'File' | 'Edit' | 'View' | 'World' | 'Interiors' | 'People' | 'Tools' | 'Help';
    group?: string;
    keys?: string;          // Display form, e.g. "Ctrl+S".
    match?: (e: KeyboardEvent) => boolean;
    run: () => void;
    enabled?: () => boolean;
    checked?: () => boolean;
}

const ctrl = (key: string, shift = false) => (e: KeyboardEvent) =>
    (e.ctrlKey || e.metaKey) && e.shiftKey === shift && !e.altKey && e.key.toLowerCase() === key;
const plain = (key: string) => (e: KeyboardEvent) => !e.ctrlKey && !e.metaKey && !e.altKey && e.key.toLowerCase() === key;

const safeName = () => getState().project.name.replace(/[^\w-]+/g, '-').replace(/^-|-$/g, '').toLowerCase() || 'atlas';

/** World edits save as they are made; Ctrl+S saves the roster in Characters and otherwise says where things stand. */
export async function saveProject() {
    const s = getState();
    if (s.workspace === 'characters') { const {saveRoster} = await import('../components/Characters'); await saveRoster(); return; }
    toast(s.live.state === 'offline' ? `Not connected: ${s.live.error} Your edits are kept and sent when it is back.`
        : s.live.pending ? 'Saving…' : 'Everything is saved. Edits save as you make them.', s.live.state === 'offline' ? 'error' : 'info');
}

export async function exportZip() {
    const {errors} = M.validate(getState().project);
    if (errors.length) { toast(`Fix ${errors.length} problem${errors.length > 1 ? 's' : ''} before exporting (see Problems).`, 'error'); return; }
    try {
        download(`${safeName()}-world.zip`, await api.exportZip(getState().project));
        toast('Exported the world ZIP. Extract it into a new folder to play.', 'success');
    } catch (error) { toast((error as Error).message, 'error'); }
}

function importCellFile() {
    const input = Object.assign(document.createElement('input'), {type: 'file', accept: '.cell'});
    input.onchange = async () => {
        const file = input.files?.[0];
        if (!file) return;
        if (file.size > 8 * 1024 * 1024) { toast('.cell files are limited to 8 MB.', 'error'); return; }
        importCell(await file.text(), file.name);
    };
    input.click();
}

/** Imports a legacy runtime .cell file as a new detached interior. */
function importCell(text: string, filename: string) {
    const lines = text.replace(/\r/g, '').split('\n');
    const at = lines.indexOf('grid:');
    const header = Object.fromEntries(lines.slice(0, at).map(l => [l.slice(0, l.indexOf(':')), l.slice(l.indexOf(':') + 1).trim()]));
    const rows = lines.slice(at + 1).filter(Boolean);
    if (at < 0 || !rows.length) { toast(`${filename} has no grid: section.`, 'error'); return; }
    commit(`Imported ${filename} as an interior.`, p => {
        const room = M.addRoom(p, rows[0].length, rows.length, header.name || filename.replace(/\.cell$/, ''));
        M.stamp(p, 0, 0, rows, room.id);
        const light = (header.lighting ?? '').split(/\s+/);
        M.updateCell(p, room.id, {
            description: header.description ?? '', outdoors: header.outdoors === 'true',
            weather: ((M.WEATHERS as string[]).includes(header.weather) ? header.weather : 'clear') as M.Weather,
            ...(light.length === 3 ? {lighting: {artificial: +light[0], daylightAccess: +light[1], tone: light[2] as 'warm'}} : {}),
        });
        setTimeout(() => openView({kind: 'cell', id: room.id}));
    });
}

const tool = (id: Tool, label: string, key: string, group: string): Command => ({
    id: `tool.${id}`, label, menu: 'Tools', group, keys: key.toUpperCase(), match: plain(key),
    run: () => useTool(id), checked: () => getState().tool === id,
});

const selectedCells = () => { const s = getState().selection; return s?.kind === 'cells' ? s.ids : []; };

export const COMMANDS: Command[] = [
    {id: 'file.import', label: 'Import a .cell file as an interior…', menu: 'File', group: 'a', keys: 'Ctrl+O', match: ctrl('o'), run: importCellFile},
    {id: 'file.save', label: 'Save status', menu: 'File', group: 'b', keys: 'Ctrl+S', match: ctrl('s'), run: saveProject},
    {id: 'file.saveas', label: 'Download a copy (atlas JSON)', menu: 'File', group: 'b', keys: 'Ctrl+Shift+S', match: ctrl('s', true), run: async () => {
        // A copy is the whole world: first the ground of every cell not yet seen here.
        if (anyPending()) {
            toast('Fetching the rest of the world\'s ground for the copy…');
            try { await allGround(); } catch (error) { toast((error as Error).message, 'error'); return; }
        }
        download(`${safeName()}.atlas.json`, JSON.stringify(getState().project, null, 2) + '\n'); toast('Downloaded a copy of the world.', 'success');
    }},
    {id: 'file.export', label: 'Export world ZIP', menu: 'File', group: 'c', keys: 'Ctrl+E', match: ctrl('e'), run: exportZip},
    {id: 'file.publish', label: 'Push to live…', menu: 'File', group: 'c', run: () => setState({dialog: 'publish'})},
    {id: 'file.play', label: 'Play in game…', menu: 'File', group: 'c', keys: 'Ctrl+Enter', match: e => (e.ctrlKey || e.metaKey) && e.key === 'Enter', run: () => setState({dialog: 'play'})},

    {id: 'edit.undo', label: 'Undo', menu: 'Edit', group: 'a', keys: 'Ctrl+Z', match: ctrl('z'), run: undo, enabled: () => getState().past.length > 0},
    {id: 'edit.redo', label: 'Redo', menu: 'Edit', group: 'a', keys: 'Ctrl+Shift+Z', match: e => ctrl('z', true)(e) || ctrl('y')(e), run: redo, enabled: () => getState().future.length > 0},
    {id: 'edit.delete', label: 'Delete selected', menu: 'Edit', group: 'b', keys: 'Del', match: e => e.key === 'Delete', run: deleteSelection,
        enabled: () => ['person', 'slot', 'link', 'route'].includes(getState().selection?.kind ?? '') || selectedCells().length === 1},
    {id: 'edit.deselect', label: 'Clear selection', menu: 'Edit', group: 'b', keys: 'Esc', run: () => setState({selection: null, pending: null})},
    {id: 'edit.palette', label: 'Command palette…', menu: 'Edit', group: 'c', keys: 'Ctrl+K', match: e => ctrl('k')(e) || ctrl('p', true)(e), run: () => setState({palette: true})},

    {id: 'view.world', label: 'World overview', menu: 'View', group: 'a', keys: 'Home', match: e => e.key === 'Home', run: () => openView({kind: 'world'})},
    {id: 'view.fit', label: 'Fit to window', menu: 'View', group: 'a', keys: 'F', match: plain('f'), run: () => setState(s => ({fitRequest: s.fitRequest + 1}))},
    ...(['cuts', 'grid', 'links', 'people', 'territory', 'camps'] as const).map((k): Command => ({
        id: `view.${k}`, menu: 'View', group: 'b', run: () => setState(s => ({overlays: {...s.overlays, [k]: !s.overlays[k]}})),
        checked: () => getState().overlays[k],
        label: {cuts: 'Cell boundaries', grid: 'Tile grid', links: 'Doors, spawn & herbs', people: 'People', territory: 'Territory claims', camps: "Chapters' camps (from the game)"}[k],
    })),
    {id: 'view.relief', label: 'Cycle elevation relief (off / shading / full)', menu: 'View', group: 'b2', keys: 'E', match: plain('e'),
        run: () => { const next = nextElevation(getState().elevation); setState({elevation: next}); toast(`${ELEVATION_LABEL[next]}.`); }},
    ...ELEVATION_MODES.map((m): Command => ({id: `view.relief.${m}`, menu: 'View', group: 'b2',
        label: {off: 'Elevation: off', shade: 'Elevation: shading & ledges', full: 'Elevation: full relief (contours, labels)'}[m],
        run: () => setState({elevation: m}), checked: () => getState().elevation === m})),
    {id: 'view.ascii', label: 'Plain ASCII glyphs', menu: 'View', group: 'b3', run: () => setPlainAscii(!glyphRender().ascii),
        checked: () => glyphRender().ascii},

    {id: 'world.cell', label: 'Add a cell (tool)', menu: 'World', group: 'a', run: () => useTool('cell')},
    {id: 'world.deleteCell', label: 'Delete selected cell', menu: 'World', group: 'a', run: deleteSelection,
        enabled: () => selectedCells().length === 1 && !isRoomSelected()},
    {id: 'world.cut', label: 'Re-cut the cells…', menu: 'World', group: 'a', run: () => setState({dialog: 'cut'})},
    {id: 'world.merge', label: 'Merge selected cells', menu: 'World', group: 'a', enabled: () => selectedCells().length > 1,
        run: () => commit('Merged cells.', p => M.mergeCells(p, selectedCells()))},
    {id: 'world.split', label: 'Split selected cell…', menu: 'World', group: 'a', enabled: () => selectedCells().length === 1 && getState().project.cells.some(c => c.id === selectedCells()[0]),
        run: () => setState({dialog: 'split'})},
    {id: 'world.building', label: 'Place a building (tool)', menu: 'World', group: 'b', run: () => useTool('building')},
    ...([['map', 'Map workspace', '1'], ['interiors', 'Interiors workspace', '2'], ['people', 'People workspace', '3'], ['characters', 'Characters & professions', '4']] as const).map(([ws, label, key]): Command => ({
        id: `view.${ws}`, label, menu: 'View', group: 'a0', keys: `Alt+${key}`, match: e => e.altKey && !e.ctrlKey && !e.metaKey && e.key === key,
        run: () => setWorkspace(ws), checked: () => getState().workspace === ws})),
    {id: 'world.politics', label: 'Factions & Chapters…', menu: 'World', group: 'd', run: () => setState({dialog: 'politics'})},
    {id: 'world.settings', label: 'World settings', menu: 'World', group: 'd', run: () => { setWorkspace('map'); setState({selection: null}); }},

    {id: 'interiors.new', label: 'New blank interior…', menu: 'Interiors', group: 'a', run: () => setState({dialog: 'room'})},
    {id: 'interiors.building', label: 'Place a building with its interior (tool)', menu: 'Interiors', group: 'a', run: () => useTool('building')},
    {id: 'interiors.delete', label: 'Delete this interior', menu: 'Interiors', group: 'b', run: deleteViewedInterior,
        enabled: () => getState().workspace === 'interiors' && isRoomView(getState().project, getState().view)},

    {id: 'people.person', label: 'Place a named NPC (tool)', menu: 'People', group: 'a', run: () => useTool('person')},
    {id: 'people.slot', label: 'Place profession slots (tool)', menu: 'People', group: 'a', run: () => useTool('slot')},
    {id: 'people.route', label: 'New patrol route', menu: 'People', group: 'b', run: newRoute},
    {id: 'people.post', label: 'Add patrol posts (tool)', menu: 'People', group: 'b', run: () => useTool('post'),
        enabled: () => getState().selection?.kind === 'route'},
    {id: 'view.routes', label: 'Show all patrol routes', menu: 'People', group: 'b', run: () => setState(s => ({overlays: {...s.overlays, routes: !s.overlays.routes}})),
        checked: () => getState().overlays.routes},
    {id: 'people.economy', label: 'Population & economy', menu: 'People', group: 'c', run: () => { setWorkspace('people'); setState({selection: null}); }},

    tool('select', 'Select & move', 'v', 'a'),
    tool('pan', 'Pan', 'm', 'a'),
    tool('brush', 'Brush', 'b', 'b'),
    tool('rect', 'Rectangle', 'r', 'b'),
    tool('line', 'Line', 'l', 'b'),
    tool('fill', 'Fill', 'g', 'b'),
    tool('pick', 'Eyedropper', 'i', 'b'),
    tool('height', 'Elevation', 'h', 'b'),
    tool('cell', 'New cell', 'c', 'c'),
    tool('door', 'Connect (door / stairs)', 'd', 'c'),
    tool('building', 'Building', 'u', 'c'),
    tool('person', 'Named NPC', 'p', 'd'),
    tool('slot', 'Profession slot', 'j', 'd'),
    tool('post', 'Patrol post', 'o', 'd'),
    tool('spawn', 'Player spawn', 'n', 'e'),
    tool('herb', 'Resource: herb patch', 'k', 'e'),

    {id: 'tool.cellTurn', label: 'Turn the new cell (swap width and height)', menu: 'Tools', keys: 'X', match: e => !e.ctrlKey && !e.metaKey && !e.altKey && e.key.toLowerCase() === 'x',
        enabled: () => getState().tool === 'cell', run: () => setState(s => ({cellSize: {w: s.cellSize.h, h: s.cellSize.w}}))},
    {id: 'help.shortcuts', label: 'Keyboard shortcuts', menu: 'Help', keys: '?', match: e => e.key === '?', run: () => setState({dialog: 'shortcuts'})},
    {id: 'help.about', label: 'How worlds reach the game', menu: 'Help', run: () => setState({dialog: 'about'})},
];

function isRoomSelected() {
    const ids = selectedCells();
    return ids.length === 1 && getState().project.rooms.some(r => r.id === ids[0]);
}

/**
 * Starts a new patrol route. A route always has at least one post (the game needs somewhere to walk), so it is
 * created by the first click on the map with the Patrol post tool; later clicks add posts to it.
 */
export function newRoute() {
    setWorkspace('people');
    setState({selection: null, tool: 'post', pending: null});
    toast('New patrol route: click the map where its first post goes, then keep clicking to add posts in walking order.');
}

export function deleteViewedInterior() {
    const view = getState().view;
    const room = view.kind === 'cell' ? getState().project.rooms.find(r => r.id === view.id) : undefined;
    if (!room || !window.confirm(`Delete ${room.name} and every door leading to it?`)) return;
    if (commit('Deleted interior.', p => M.removeRoom(p, room.id))) setState({selection: null});
}

export function deleteSelection() {
    const s = getState().selection;
    if (!s) return;
    if (s.kind === 'person') { if (commit('Removed resident.', p => M.removePerson(p, s.id))) setState({selection: null}); }
    else if (s.kind === 'slot') { if (commit('Removed profession slot; its character is released at the next export.', p => M.removeSlot(p, s.id))) setState({selection: null}); }
    else if (s.kind === 'link') { if (commit('Removed connection.', p => M.removeLink(p, s.id))) setState({selection: null}); }
    else if (s.kind === 'route') { if (commit('Removed route; its guards now hold their work posts.', p => M.removeRoute(p, s.id))) setState({selection: null}); }
    else if (s.kind === 'cells' && isRoomSelected() && window.confirm('Delete this interior and every door leading to it?')) {
        const id = s.ids[0];
        if (commit('Deleted interior.', p => M.removeRoom(p, id))) setState(st => ({selection: null, view: st.view.kind === 'cell' && st.view.id === id ? {kind: 'world'} : st.view}));
    }
    else if (s.kind === 'cells' && s.ids.length === 1) deleteCell(s.ids[0]);
}

/** Deletes a world cell: its ground is cleared and the world shrinks to the cells that remain. */
export function deleteCell(id: string) {
    const cell = getState().project.cells.find(c => c.id === id);
    if (!cell || !window.confirm(`Delete ${cell.name}? Its ground is cleared, every door leading to it goes, and the world shrinks to the cells that remain.`)) return;
    if (commit(`Deleted ${cell.name}.`, p => M.removeCell(p, id))) setState(st => ({selection: null, view: st.view.kind === 'cell' && st.view.id === id ? {kind: 'world'} : st.view}));
}

export function handleShortcut(e: KeyboardEvent): boolean {
    const target = e.target as HTMLElement | null;
    const typing = target && (target.tagName === 'INPUT' || target.tagName === 'TEXTAREA' || target.tagName === 'SELECT' || target.isContentEditable);
    if (e.key === 'Escape') {
        const s = getState();
        if (s.palette || s.dialog) return false;
        if (s.tool === 'cell') useTool('select');           // Cancels placing a new cell.
        else if (s.pending) setState({pending: null});
        else if (s.selection) setState({selection: null});
        else if (s.view.kind === 'cell' && s.workspace !== 'interiors') openView({kind: 'world'});
        return true;
    }
    for (const c of COMMANDS) {
        if (!c.match?.(e)) continue;
        const global = e.ctrlKey || e.metaKey;
        if (typing && !global) return false;
        if (c.enabled && !c.enabled()) return false;
        e.preventDefault();
        c.run();
        return true;
    }
    return false;
}
