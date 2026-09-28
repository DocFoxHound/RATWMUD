// Editor state: the project plus undo history, selection, tools and view.
// A tiny external store (useSyncExternalStore) keeps this dependency-free.
import {useSyncExternalStore} from 'react';
import * as M from '../model/model.mjs';
import type {Glyph, Place, PlaceRef, Project, Role} from '../model/model.mjs';
import type {Roster, SlotPreview} from './roster';
import type {ElevationMode} from './elevation';
import type {HeightBrush} from './glyphs';
import {applyChanges, diff, inverse, type Batch, type Change, type ConflictNotice, type Editor, type Identity, type LiveStatus} from './live';
import {blockedBy, groundPending, lateFor, noteRemote, settleCell, want, type Ground} from './lazyGround';

export type View = {kind: 'world'} | {kind: 'cell'; id: string};
export type Workspace = 'map' | 'interiors' | 'people' | 'characters';
/** The workspaces that show the map canvas. */
export type MapSpace = Exclude<Workspace, 'characters'>;
export type Tool = 'select' | 'brush' | 'rect' | 'line' | 'fill' | 'pick' | 'height' | 'door' | 'cell'
    | 'person' | 'slot' | 'post' | 'spawn' | 'herb' | 'building' | 'pan';
export type Selection =
    | {kind: 'cells'; ids: string[]}
    | {kind: 'person'; id: string}
    | {kind: 'slot'; id: string}
    | {kind: 'link'; id: string}
    | {kind: 'route'; id: string}
    | {kind: 'spawn'} | {kind: 'herb'}
    | null;
/** A "click the map to choose a place" request from the inspector. */
export type Pending =
    | {kind: 'place'; ref: PlaceRef; label: string}
    | {kind: 'link'; a: Place | null; linkKind: 'door' | 'passage' | 'stairs'}
    | null;
export type Dialog = 'identity' | 'conflict' | 'publish' | 'cut' | 'split' | 'room' | 'politics' | 'play' | 'shortcuts' | 'about' | 'generate' | null;
export interface Toast { id: number; text: string; tone: 'info' | 'error' | 'success' }
export interface Overlays { cuts: boolean; links: boolean; territory: boolean; people: boolean; routes: boolean; grid: boolean }

/** Where someone else just edited, drawn briefly in their color: world tiles, or tiles of one interior. */
export interface Mark { area: 'world' | string; x: number; y: number; w: number; h: number; color: string; at: number }

export interface EditorState {
    /** The one world, as this editor currently sees it (its own edits plus everyone else's as they arrive). */
    project: Project;
    /** This editor's own actions, as the edits that made them; undo sends the inverse. */
    past: Batch[];
    future: Batch[];
    /** False until the world has arrived from the host. */
    loaded: boolean;
    live: LiveStatus;
    identity: Identity | null;
    editors: Editor[];
    conflict: ConflictNotice | null;
    marks: Mark[];
    view: View;
    selection: Selection;
    tool: Tool;
    glyph: Glyph;
    brush: number;
    height: HeightBrush;
    filled: boolean;
    role: Role;
    building: string;
    side: 'S' | 'N';
    /** The New cell tool's size, in tiles. */
    cellSize: {w: number; h: number};
    pending: Pending;
    dialog: Dialog;
    palette: boolean;
    overlays: Overlays;
    /** How the map shows elevation: off, shading (with impassable ledges), or full relief (contours and labels). */
    elevation: ElevationMode;
    toasts: Toast[];
    hover: {x: number; y: number; place: Place | null} | null;
    fitRequest: number;
    /** Map: world terrain and places. Interiors: rooms and buildings' insides. People: who lives and works here. Characters: the shared roster. */
    workspace: Workspace;
    /** What each workspace was showing when you last left it. */
    lastView: Partial<Record<Workspace, View>>;
    roster: Roster | null;
    savedRoster: Roster | null;
    character: string | null;
    profession: string;
    preview: SlotPreview | null;
}

let state: EditorState = {
    project: M.createProject(64, 48, 'Loading the world…'), past: [], future: [], loaded: false,
    live: {state: 'connecting', pending: 0, error: ''}, identity: null, editors: [], conflict: null, marks: [],
    view: {kind: 'world'}, selection: null, tool: 'select', glyph: '#', brush: 1,
    height: 1, filled: true, role: 'civilian', building: 'house', side: 'S', cellSize: {w: 64, h: 64}, pending: null, dialog: null,
    palette: false, overlays: {cuts: true, links: true, territory: false, people: true, routes: true, grid: false},
    elevation: 'shade', toasts: [], hover: null, fitRequest: 0,
    workspace: 'map', lastView: {}, roster: null, savedRoster: null, character: null, profession: 'guard', preview: null,
};
const listeners = new Set<() => void>();

export function getState() { return state; }
export function setState(patch: Partial<EditorState> | ((s: EditorState) => Partial<EditorState>)) {
    state = {...state, ...(typeof patch === 'function' ? patch(state) : patch)};
    listeners.forEach(l => l());
}
export function useStore<T>(select: (s: EditorState) => T): T {
    return useSyncExternalStore(l => { listeners.add(l); return () => listeners.delete(l); }, () => select(state));
}

let toastId = 1;
export function toast(text: string, tone: Toast['tone'] = 'info') {
    const id = toastId++;
    setState(s => ({toasts: [...s.toasts.slice(-3), {id, text, tone}]}));
    setTimeout(() => setState(s => ({toasts: s.toasts.filter(t => t.id !== id)})), tone === 'error' ? 7000 : 3500);
}

// --------------------------------------------------------------------------- Live editing

let outbox: {push(batch: Batch): void} | null = null;
let batchId = 1;
/** Connects the store to the live session (lib/session.ts); every edit goes out through it. */
export function connectLive(link: {push(batch: Batch): void} | null) { outbox = link; }

function send(label: string, ops: ReturnType<typeof diff>): Batch {
    const batch = {id: batchId++, label, ops};
    outbox?.push(batch);
    return batch;
}

/**
 * Runs one model operation on a copy of the world as a single undo step, and saves it at once.
 * Model operations validate and throw on failure; the world is then unchanged.
 */
export function commit<T>(label: string, operation: (draft: Project) => T, quiet = false): T | undefined {
    if (!state.loaded) { toast('The world is still loading.', 'error'); return undefined; }
    // A shallow copy: model operations replace what they change (cells included) and never edit the rest, so the
    // world before and after share every cell the action did not touch. Diffing and redrawing then cost only that.
    const before = state.project, draft = {...before};
    try {
        const result = operation(draft);
        const ops = diff(before, draft);
        // Ground not yet here (a world loaded lean) holds only placeholder tiles: nothing may change it or rely on it.
        const waiting = blockedBy(before, draft, ops);
        if (waiting.length) {
            waiting.forEach(want);
            toast('The ground there is still arriving; try again in a moment.', 'error');
            return undefined;
        }
        const batch = ops.length ? send(label, ops) : null;
        // When the world's edges move, canvas coordinates shift: forget the stale pointer position.
        const moved = ops.some(o => o.key === 'bounds');
        setState(s => ({project: draft, past: batch ? [...s.past.slice(-99), batch] : s.past, future: batch ? [] : s.future,
            ...(moved ? {hover: null} : {})}));
        repairView();
        if (!quiet) toast(label, 'success');
        return result;
    } catch (error) {
        toast((error as Error).message, 'error');
        return undefined;
    }
}

/** Reverses this editor's own last action. If someone has changed the same thing since, the host refuses it. */
export function undo() {
    const last = state.past[state.past.length - 1];
    if (!last) return;
    const ops = inverse(last.ops), project = {...state.project};
    applyChanges(project, ops);
    const batch = send(`Undo: ${last.label}`, ops);
    setState(s => ({project, past: s.past.slice(0, -1), future: [{...last, id: batch.id}, ...s.future]}));
    repairView();
}
export function redo() {
    const next = state.future[0];
    if (!next) return;
    const project = {...state.project};
    applyChanges(project, next.ops);
    const batch = send(next.label, next.ops);
    setState(s => ({project, future: s.future.slice(1), past: [...s.past, {...next, id: batch.id}]}));
    repairView();
}

/** The world as it arrived from the host; history starts fresh. Existing problems are shown, not refused. */
export function loadProject(value: unknown) {
    try {
        const project = M.normalizeProject(value, true);
        setState({project, loaded: true, past: [], future: [], lastView: {},
            view: state.workspace === 'interiors' ? firstInteriorView(project) : {kind: 'world'},
            selection: null, pending: null, fitRequest: state.fitRequest + 1});
        return true;
    } catch (error) {
        toast(`Could not open the world: ${(error as Error).message}`, 'error');
        return false;
    }
}

/** Someone else's edits: applied as they are, drawn briefly in their color. */
export function applyRemote(changes: Change[], colorOf: (c: Change) => string) {
    const project = {...state.project};
    applyChanges(project, changes);
    noteRemote(state.project, project, changes);
    const now = Date.now(), marks: Mark[] = [];
    for (const c of changes) {
        const [kind, rest] = [c.key.split(':')[0], c.key.slice(c.key.indexOf(':') + 1)];
        if (kind === 'tile' || kind === 'height') {
            const [x, y] = rest.split(',').map(Number);
            marks.push({area: 'world', x, y, w: 1, h: 1, color: colorOf(c), at: now});
        } else if (kind === 'rtile' || kind === 'rheight') {
            const [room, at] = rest.split(':'), [x, y] = at.split(',').map(Number);
            marks.push({area: room, x, y, w: 1, h: 1, color: colorOf(c), at: now});
        } else if (kind === 'cell' && c.after) {
            const cell = c.after as M.WorldCell;
            marks.push({area: 'world', x: cell.x, y: cell.y, w: cell.width, h: cell.height, color: colorOf(c), at: now});
        }
    }
    const moved = changes.some(c => c.key === 'bounds');
    setState(s => ({project, marks: [...s.marks.filter(m => now - m.at < 4000), ...marks].slice(-4000), ...(moved ? {hover: null} : {})}));
    repairView();
}

/**
 * Ground that has arrived for cells still waiting for it (as of edit `seq`), put in place; then edits others made there
 * since are made again. Returns the cells taken: one re-cut meanwhile is asked for again.
 */
export function installGround(arrived: Map<string, Ground>, seq: number): string[] {
    const taken: string[] = [];
    const cells = state.project.cells.map(c => {
        const g = arrived.get(c.id);
        if (!g || !groundPending(c.id) || g.x !== c.x || g.y !== c.y || g.width !== c.width || g.height !== c.height) return c;
        settleCell(c.id);
        taken.push(c.id);
        return M.withGround(c, g.terrain, g.heights);
    });
    if (!taken.length) return taken;
    const project = {...state.project, cells};
    const again = lateFor(cells.filter(c => taken.includes(c.id)), seq);
    if (again.length) applyChanges(project, again);
    setState({project});
    return taken;
}

/** A refused batch: show the true values, forget the action, and tell the editor who got there first. */
export function refuseBatch(batch: Batch, notice: ConflictNotice, current: Record<string, unknown>) {
    const project = {...state.project};
    applyChanges(project, Object.entries(current).map(([key, after]) => ({key, after})));
    setState(s => ({project, conflict: notice, dialog: s.dialog ?? 'conflict',
        past: s.past.filter(b => b.id !== batch.id), future: s.future.filter(b => b.id !== batch.id)}));
    repairView();
}

/** After undo/redo the viewed cell or selected thing may no longer exist. */
function repairView() {
    const {project, view, selection} = state;
    const exists = (id: string) => !!M.getCell(project, id);
    const patch: Partial<EditorState> = {};
    if (view.kind === 'cell' && !exists(view.id)) patch.view = state.workspace === 'interiors' ? firstInteriorView(project) : {kind: 'world'};
    if (patch.view) patch.fitRequest = state.fitRequest + 1;
    if (selection?.kind === 'cells' && !selection.ids.every(exists)) patch.selection = null;
    if (selection?.kind === 'person' && !project.people.some(p => p.id === selection.id)) patch.selection = null;
    if (selection?.kind === 'link' && !project.links.some(l => l.id === selection.id)) patch.selection = null;
    if (selection?.kind === 'route' && !project.routes.some(r => r.id === selection.id)) patch.selection = null;
    if (selection?.kind === 'slot' && !project.slots.some(r => r.id === selection.id)) patch.selection = null;
    if (Object.keys(patch).length) setState(patch);
}

export const isRoomView = (project: Project, view: View) => view.kind === 'cell' && project.rooms.some(r => r.id === view.id);
const firstInteriorView = (project: Project): View => project.rooms.length ? {kind: 'cell', id: project.rooms[0].id} : {kind: 'world'};

/** Shows a surface. Interiors open in the Interiors workspace and world cells in Map; People can show either. */
export function openView(view: View) {
    const room = isRoomView(state.project, view);
    if (room && state.workspace === 'map') setWorkspace('interiors');
    else if (!room && state.workspace === 'interiors') setWorkspace('map');
    setState(s => ({view, fitRequest: s.fitRequest + 1, hover: null}));
}
/** Selects a placed thing and shows the surface it lives on. */
export function focusPlace(place: Place | null) {
    if (!place) return;
    const inRoom = state.project.rooms.some(r => r.id === place.cell);
    const view = state.view;
    if (inRoom || (view.kind === 'cell' && view.id !== place.cell)) openView({kind: 'cell', id: place.cell});
}

/** Roster edits are kept in memory until Save roster; there is one shared file for every world. */
export function editRoster(change: (r: Roster) => void) {
    const current = state.roster;
    if (!current) return;
    const next = M.clone(current);
    change(next);
    setState({roster: next});
}
export const isRosterDirty = (s: EditorState) => s.roster !== s.savedRoster;

/** Which map workspaces offer each tool; the first is where picking it from elsewhere takes you. */
const TERRAIN: MapSpace[] = ['map', 'interiors'];
export const TOOL_SPACE: Record<Tool, MapSpace[]> = {
    select: ['map', 'interiors', 'people'], pan: ['map', 'interiors', 'people'],
    brush: TERRAIN, rect: TERRAIN, line: TERRAIN, fill: TERRAIN, pick: TERRAIN, height: TERRAIN, door: TERRAIN,
    building: ['map'], cell: ['map'], spawn: ['map'], herb: ['map'], person: ['people'], slot: ['people'], post: ['people'],
};
/** Which map workspaces edit each kind of selection. */
export function selectionSpace(project: Project, selection: Selection): MapSpace[] {
    if (!selection) return [];
    if (selection.kind === 'cells') return selection.ids.every(id => project.rooms.some(r => r.id === id)) ? ['interiors'] : ['map'];
    if (selection.kind === 'link') return TERRAIN;
    if (selection.kind === 'spawn' || selection.kind === 'herb') return ['map'];
    return ['people'];
}

/** Switches workspace, dropping a tool or selection that belongs to another one and restoring what this one last showed. */
export function setWorkspace(workspace: Workspace) {
    setState(s => {
        const patch: Partial<EditorState> = {workspace, pending: null, lastView: {...s.lastView, [s.workspace]: s.view}};
        if (workspace !== 'characters') {
            if (!TOOL_SPACE[s.tool].includes(workspace)) patch.tool = 'select';
            if (s.selection && !selectionSpace(s.project, s.selection).includes(workspace)) patch.selection = null;
            const remembered = s.lastView[workspace];
            const valid = remembered && (remembered.kind === 'world' || M.getCell(s.project, remembered.id));
            if (workspace === 'interiors' && !isRoomView(s.project, s.view))
                patch.view = valid && isRoomView(s.project, remembered) ? remembered : firstInteriorView(s.project);
            else if (workspace === 'map' && isRoomView(s.project, s.view))
                patch.view = valid && !isRoomView(s.project, remembered) ? remembered : {kind: 'world'};
            if (patch.view) patch.fitRequest = s.fitRequest + 1;
        }
        return patch;
    });
}
/** Picks a tool, moving to the workspace it belongs to. */
export function useTool(tool: Tool) {
    const spaces = TOOL_SPACE[tool];
    if (!spaces.includes(state.workspace as MapSpace)) setWorkspace(spaces[0]);
    setState({tool, pending: null});
}
/** Selects something, moving to the workspace that edits it. */
export function selectThing(selection: Selection) {
    const spaces = selectionSpace(state.project, selection);
    if (spaces.length && !spaces.includes(state.workspace as MapSpace)) setWorkspace(spaces[0]);
    setState({selection});
}
