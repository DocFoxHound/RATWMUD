// NPC Management: create, place, change, kill, revive and delete NPCs in PROD or DEV, and the layers they live by:
// patrol routes, painted areas (wander, spawn, plan) and spawn rules.
// NPCs are shown at their spawn points (home, work and evening places), never where they happen to be walking;
// that is the LIVE tab's job. Saving writes the row and syncs it into a running game server.
import {useCallback, useEffect, useMemo, useRef, useState} from 'react';
import * as M from '../model/model.mjs';
import type {Person, Place, Project, Role, Route} from '../model/model.mjs';
import {ROLE_INFO, formatHour} from '../lib/glyphs';
import {surfaceFor} from '../lib/surface';
import {NumberField, Row, SelectField, TextField, Toggle, Hint} from '../components/fields';
import {useWorld} from './world';
import {dmApi, type Action, type Me, type NpcArea, type Npcs, type Target} from './api';
import {LifePanel} from './LifePanel';
import {MapView, type Marker, type Mode, type Overlay} from './MapView';
import {AREA_KINDS, AreaInspector, LAYERS, RouteInspector, SpawnInspector, areaColor, idFrom, type Brush, type Layer, type SpawnDraft} from './LayerPanels';

type View = {kind: 'world'} | {kind: 'cell'; id: string};
type Pick = 'new' | 'home' | 'work' | 'evening' | null;

export function NpcTab({me, target}: {me: Me; target: Target}) {
    const [data, setData] = useState<Npcs | null>(null);
    const [problem, setProblem] = useState('');
    const world = useWorld(target, setProblem);
    const [view, setView] = useState<View>({kind: 'world'});
    const [layer, setLayer] = useState<Layer>('npcs');
    const [selected, setSelected] = useState<string | null>(null);
    const [draft, setDraft] = useState<Person | null>(null);
    const [routeDraft, setRouteDraft] = useState<Route | null>(null);
    const [areaDraft, setAreaDraft] = useState<NpcArea | null>(null);
    const [spawnDraft, setSpawnDraft] = useState<SpawnDraft | null>(null);
    const [isNew, setIsNew] = useState(false);
    const [pick, setPick] = useState<Pick>(null);
    const [addingPosts, setAddingPosts] = useState(false);
    const [brush, setBrush] = useState<Brush>('paint');
    const [role, setRole] = useState<Role>('civilian');
    const [busy, setBusy] = useState(false);
    const [query, setQuery] = useState('');
    const canAct = me.role !== 'viewer';
    useEffect(() => {
        const esc = (e: KeyboardEvent) => { if (e.key === 'Escape') { setPick(null); setAddingPosts(false); } };
        window.addEventListener('keydown', esc);
        return () => window.removeEventListener('keydown', esc);
    }, []);

    const load = useCallback(() => dmApi.npcs(target).then(d => { setData(d); setProblem(''); }).catch(e => setProblem((e as Error).message)), [target]);
    useEffect(() => { void load(); }, [load, target]);
    const waiting = data?.actions.some(a => a.status === 'queued');
    useEffect(() => { if (!waiting) return; const t = setTimeout(load, 1500); return () => clearTimeout(t); }, [waiting, data, load]);
    // Spawn counts change as the server works; refresh them now and then while that layer is open.
    useEffect(() => { if (layer !== 'spawns') return; void load(); const t = setInterval(load, 10000); return () => clearInterval(t); }, [layer, load]);

    const saved = layer === 'npcs' ? data?.people.find(p => p.id === selected) ?? null : null;
    const holder = layer === 'npcs' ? data?.holders.find(h => h.id === selected) ?? null : null;
    const savedRoute = layer === 'routes' ? data?.routes.find(r => r.id === selected) ?? null : null;
    const savedArea = layer === 'areas' ? data?.areas.find(a => a.id === selected) ?? null : null;
    const savedRule = layer === 'spawns' ? data?.spawns.find(r => r.id === selected) ?? null : null;
    const ruleDraftOf = (r: typeof savedRule): SpawnDraft | null => r && {id: r.id, name: r.name, area: r.area, template: r.template, count: r.count, respawnMinutes: r.respawnMinutes, enabled: r.enabled};
    useEffect(() => { if (!isNew) setDraft(saved ? M.clone(saved) : null); }, [saved, isNew]);
    useEffect(() => { if (!isNew) setRouteDraft(savedRoute ? M.clone(savedRoute) : null); }, [savedRoute, isNew]);
    useEffect(() => { if (!isNew) setAreaDraft(savedArea ? M.clone(savedArea) : null); }, [savedArea, isNew]);
    useEffect(() => { if (!isNew) setSpawnDraft(ruleDraftOf(savedRule)); }, [savedRule, isNew]); // eslint-disable-line react-hooks/exhaustive-deps
    const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);
    const dirty = isNew || (layer === 'npcs' ? !!draft && !same(draft, saved) : layer === 'routes' ? !same(routeDraft, savedRoute)
        : layer === 'areas' ? !same(areaDraft, savedArea) : !same(spawnDraft, ruleDraftOf(savedRule)));
    const dead = new Set(data?.dead ?? []);

    const switchLayer = (next: Layer) => {
        if (next === layer) return;
        if (dirty && (draft || routeDraft || areaDraft || spawnDraft) && !window.confirm('Discard the unsaved changes?')) return;
        setLayer(next); setSelected(null); setIsNew(false); setPick(null); setAddingPosts(false);
        setDraft(null); setRouteDraft(null); setAreaDraft(null); setSpawnDraft(null);
    };
    const choose = (id: string, cell?: string) => { setIsNew(false); setSelected(id); setAddingPosts(false); if (cell) setView({kind: 'cell', id: cell}); };

    const surface = useMemo(() => (world ? surfaceFor(world, view as never) : null), [world, view]);
    const markers = useMemo((): Marker[] => {
        if (!surface || !data || (layer !== 'npcs' && layer !== 'spawns')) return [];
        const out: Marker[] = [];
        const add = (id: string, place: Place | undefined, color: string, glyph: string, label: string, editable: boolean) => {
            const at = place && surface.fromPlace(place);
            if (at) out.push({id, x: at[0] + .5, y: at[1] + .5, color, glyph, label, dead: dead.has(id), editable});
        };
        for (const p of data.people) {
            if (layer === 'spawns' && !data.spawned[p.id]) continue;
            const shown = p.id === selected && draft ? draft : p;
            add(p.id, shown.work, ROLE_INFO[shown.role].color, ROLE_INFO[shown.role].icon, `${shown.name} · ${data.spawned[p.id] ? 'spawned here' : 'works here'}`, layer === 'npcs');
        }
        if (layer === 'spawns') return out;
        for (const h of data.holders) add(h.id, h.work, '#b6a3cf', '◇', `${h.name} · ${h.slot} (job holder)`, false);
        if (draft && (isNew || selected === draft.id)) {
            add(draft.id + ':home', draft.home, '#ded5c3', '⌂', `${draft.name}'s home`, false);
            add(draft.id + ':evening', draft.evening, '#ded5c3', '☾', `${draft.name}'s evening place`, false);
            if (isNew) add(draft.id, draft.work, ROLE_INFO[draft.role].color, ROLE_INFO[draft.role].icon, `${draft.name} (unsaved)`, true);
        }
        return out;
    }, [surface, data, selected, draft, isNew, layer]); // eslint-disable-line react-hooks/exhaustive-deps

    const overlay = useMemo((): Overlay => {
        const out: Overlay = {tiles: [], rects: [], paths: []};
        if (!surface || !data) return out;
        const areaTiles = (a: NpcArea, strong: boolean) => {
            for (const [x, y] of a.tiles) {
                const at = surface.fromPlace({cell: a.cell, x, y});
                if (at) out.tiles.push({x: at[0], y: at[1], color: areaColor(a.kind), strong});
            }
        };
        if (layer === 'areas' || layer === 'spawns') {
            const spawnAreas = new Set(data.spawns.map(r => r.area));
            for (const a of data.areas) if (a.id !== areaDraft?.id && (layer === 'areas' || spawnAreas.has(a.id)))
                areaTiles(a, layer === 'spawns' && a.id === spawnDraft?.area);
            if (layer === 'areas' && areaDraft) areaTiles(areaDraft, true);
            if (layer === 'spawns' && spawnDraft) { const a = data.areas.find(x => x.id === spawnDraft.area); if (a && !spawnAreas.has(a.id)) areaTiles(a, true); }
        }
        if (layer === 'npcs' && draft) {                            // The selected NPC's wander area.
            const a = data.areas.find(x => x.id === data.wanders[draft.id]);
            if (a) areaTiles(a, false);
        }
        if (layer === 'routes' || (layer === 'npcs' && draft?.route)) {
            const shown = layer === 'routes' ? data.routes.filter(r => r.id !== routeDraft?.id).concat(routeDraft ? [routeDraft] : [])
                : data.routes.filter(r => r.id === draft?.route);
            for (const r of shown) {
                const on = layer === 'npcs' || r.id === routeDraft?.id;
                const points = r.posts.map(p => surface.fromPlace(p)).filter((p): p is [number, number] => !!p).map(([x, y]) => [x + .5, y + .5] as [number, number]);
                out.paths.push({points, color: on ? '#e0b85a' : 'rgba(224,184,90,.35)', numbered: on, loop: points.length === r.posts.length});
            }
        }
        return out;
    }, [surface, data, layer, areaDraft, routeDraft, spawnDraft, draft]);

    const mode: Mode = !canAct ? 'select' : layer === 'areas' && areaDraft ? 'paint' : pick || addingPosts ? 'pick' : 'select';
    const onMapClick = (place: Place | null, marker: Marker | null) => {
        if (layer === 'routes' && addingPosts && routeDraft) {
            if (!place) { setProblem('Choose open ground inside a cell or interior.'); return; }
            setRouteDraft({...routeDraft, posts: [...routeDraft.posts, place]}); setProblem('');
            return;
        }
        if (pick && canAct) {
            if (!place) { setProblem('Choose open ground inside a cell or interior.'); return; }
            if (pick === 'new' && world) {
                const person = M.newPerson({...world, people: data?.people ?? []}, role, place);
                setDraft(person); setIsNew(true); setSelected(person.id);
            } else if (draft) setDraft({...draft, [pick]: place});
            setPick(null); setProblem('');
            return;
        }
        if (marker) choose(marker.id.split(':')[0]);
    };
    const paintStroke = useRef<Brush>('paint');
    const onPaint = (place: Place | null, start: boolean) => {
        if (!place) return;
        setAreaDraft(a => {
            if (!a) return a;
            if (a.tiles.length && a.cell !== place.cell) { if (start) setProblem('An area stays within one cell or interior.'); return a; }
            const has = a.tiles.some(([x, y]) => x === place.x && y === place.y);
            if (start) paintStroke.current = brush;
            if (paintStroke.current === 'paint') {
                if (has) return a;
                if (a.tiles.length >= 4096) { setProblem('An area has at most 4096 tiles.'); return a; }
                return {...a, cell: place.cell, tiles: [...a.tiles, [place.x, place.y]]};
            }
            return has ? {...a, tiles: a.tiles.filter(([x, y]) => x !== place.x || y !== place.y)} : a;
        });
    };

    const finish = async (run: () => Promise<unknown>, done: string) => {
        setBusy(true); setProblem('');
        try { await run(); setIsNew(false); await load(); setProblem(done); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };
    const live = target === 'prod';
    const sure = (what: string) => !live || window.confirm(`${what} in the LIVE world?`);
    const taken = () => [...(data?.people ?? []).map(p => p.id), ...(data?.holders ?? []).map(h => h.id)];
    const save = () => {
        if (!draft || !sure(`Save ${draft.name}`)) return;
        const person = isNew ? {...draft, id: idFrom(draft.name, taken(), 'npc')} : draft;
        return finish(async () => { await dmApi.saveNpc(target, person); setSelected(person.id); }, `Saved ${person.name}.`);
    };
    const remove = () => draft && window.confirm(`Delete ${draft.name}${live ? ' from the LIVE world' : ''}? They leave the world entirely.`) &&
        (!live || window.confirm(`Really delete ${draft.name}? This cannot be undone.`)) &&
        finish(() => dmApi.deleteNpc(target, draft.id).then(() => setSelected(null)), `Deleted ${draft.name}.`);
    const life = (id: string, name: string, kill: boolean) => sure(`${kill ? 'Kill' : 'Revive'} ${name}`) &&
        finish(() => dmApi.npcLife(target, id, kill), `${kill ? 'Killed' : 'Revived'} ${name}.`);
    const wander = (id: string, name: string, area: string) => sure(area ? `Have ${name} wander ${data?.areas.find(a => a.id === area)?.name}` : `Stop ${name} wandering`) &&
        finish(() => dmApi.setWander(target, id, area || null), area ? `${name} now wanders there.` : `${name} keeps to their schedule.`);

    // Layer drafts: new ones take their ID from the name when first saved.
    const fresh = (make: () => void) => () => { if (!dirty || window.confirm('Discard the unsaved changes?')) make(); };
    const newRoute = fresh(() => { setRouteDraft({id: '', name: 'New patrol route', posts: []}); setIsNew(true); setSelected(null); setAddingPosts(true); });
    const newArea = fresh(() => {
        const cell = view.kind === 'cell' ? view.id : '';
        setAreaDraft({id: '', name: 'New area', kind: 'wander', cell, tiles: []}); setIsNew(true); setSelected(null); setBrush('paint');
    });
    const newRule = fresh(() => {
        const area = data?.areas.find(a => a.kind === 'spawn') ?? data?.areas[0];
        setSpawnDraft({id: '', name: 'New spawn rule', area: area?.id ?? '', template: '', count: 3, respawnMinutes: 30, enabled: true});
        setIsNew(true); setSelected(null);
    });
    const saveRoute = () => {
        if (!routeDraft || !sure(`Save the route ${routeDraft.name}`)) return;
        const route = isNew ? {...routeDraft, id: idFrom(routeDraft.name, data?.routes.map(r => r.id) ?? [], 'route')} : routeDraft;
        return finish(async () => { await dmApi.saveRoute(target, route); setSelected(route.id); setAddingPosts(false); }, `Saved ${route.name}.`);
    };
    const saveArea = () => {
        if (!areaDraft || !sure(`Save the area ${areaDraft.name}`)) return;
        const area = isNew ? {...areaDraft, id: idFrom(areaDraft.name, data?.areas.map(a => a.id) ?? [], 'area')} : areaDraft;
        return finish(async () => { await dmApi.saveArea(target, area); setSelected(area.id); }, `Saved ${area.name}.`);
    };
    const saveRule = () => {
        if (!spawnDraft || !sure(`Save the spawn rule ${spawnDraft.name}`)) return;
        const rule = isNew ? {...spawnDraft, id: idFrom(spawnDraft.name, data?.spawns.map(r => r.id) ?? [], 'spawn', 28)} : spawnDraft;
        return finish(async () => { await dmApi.saveSpawn(target, rule); setSelected(rule.id); }, `Saved ${rule.name}.`);
    };
    const deleteLayer = (what: string, name: string, run: () => Promise<unknown>) =>
        window.confirm(`Delete ${what} ${name}${live ? ' from the LIVE world' : ''}?`) && finish(() => run().then(() => setSelected(null)), `Deleted ${name}.`);
    const revert = () => {
        if (isNew) { setIsNew(false); setSelected(null); setDraft(null); setRouteDraft(null); setAreaDraft(null); setSpawnDraft(null); setAddingPosts(false); return; }
        setDraft(saved ? M.clone(saved) : null); setRouteDraft(savedRoute ? M.clone(savedRoute) : null);
        setAreaDraft(savedArea ? M.clone(savedArea) : null); setSpawnDraft(ruleDraftOf(savedRule));
    };

    const q = query.trim().toLowerCase();
    const match = (...t: string[]) => !q || t.some(x => x.toLowerCase().includes(q));
    const places = world ? [...world.cells.map(c => ({id: c.id, name: c.name, room: false})), ...world.rooms.map(r => ({id: r.id, name: r.name, room: true}))] : [];
    const actions = (data?.actions ?? []).filter(a => a.target === selected);
    const common = {target, canAct, busy, dirty, isNew, onRevert: revert};
    const peopleNamed = (ids: string[]) => ids.map(id => data?.people.find(p => p.id === id)?.name ?? data?.holders.find(h => h.id === id)?.name ?? id);
    const help: Record<Layer, string> = {
        npcs: 'NPCs are shown at their spawn points: where they work, and home and evening for the selected one. Double-click a place to open it.',
        routes: addingPosts ? 'Click open ground to add the next post · Esc when done' : 'Guards walk their route’s posts in order, and back to the first.',
        areas: areaDraft ? `Drag to ${brush} tiles · right-drag pans · wheel zooms` : 'Painted zones: where NPCs wander, where spawns appear, or plans.',
        spawns: 'Spawn rules keep NPCs made from a template alive in an area. Spawned NPCs are shown where they appeared.',
    };

    return <div className="dm-npcs">
        <nav className="explorer" aria-label="Places and NPCs">
            <div className="explorer-search"><input value={query} onChange={e => setQuery(e.target.value)} placeholder="Search…" /></div>
            <div className="explorer-scroll">
                <button className={view.kind === 'world' ? 'tree-item active' : 'tree-item'} onClick={() => setView({kind: 'world'})}>
                    <span className="tree-icon">◇</span><span className="tree-label">World overview</span></button>
                <p className="dm-group">Places</p>
                {places.filter(p => match(p.name, p.id)).map(p => <button key={p.id} className={view.kind === 'cell' && view.id === p.id ? 'tree-item active' : 'tree-item'}
                    onClick={() => setView({kind: 'cell', id: p.id})}><span className="tree-icon">{p.room ? '▣' : '▦'}</span><span className="tree-label">{p.name}</span></button>)}
                {layer === 'npcs' && <>
                    <p className="dm-group">Named NPCs {canAct && <button className="icon" title="Place a new NPC on the map" onClick={() => setPick('new')}>+</button>}</p>
                    {(data?.people ?? []).filter(p => match(p.name, p.id, p.role, p.workLabel)).map(p => <button key={p.id}
                        className={selected === p.id ? 'tree-item active' : 'tree-item'} onClick={() => choose(p.id, p.work.cell)}>
                        <span className="tree-icon" style={{color: ROLE_INFO[p.role].color}}>{dead.has(p.id) ? '✝' : ROLE_INFO[p.role].icon}</span>
                        <span className="tree-label">{p.name}</span>{data?.spawned[p.id] ? <small className="spawned">spawned</small> : <small>{p.workLabel}</small>}</button>)}
                    <p className="dm-group">Job holders</p>
                    {(data?.holders ?? []).filter(h => match(h.name, h.id, h.slot)).map(h => <button key={h.id} className={selected === h.id ? 'tree-item active' : 'tree-item'}
                        onClick={() => choose(h.id, h.work.cell)}>
                        <span className="tree-icon" style={{color: '#b6a3cf'}}>{dead.has(h.id) ? '✝' : '◇'}</span><span className="tree-label">{h.name}</span><small>{h.slot}</small></button>)}
                    {data && !data.holders.length && <p className="tree-empty">Nobody holds a profession slot yet.</p>}
                </>}
                {layer === 'routes' && <>
                    <p className="dm-group">Patrol routes {canAct && <button className="icon" title="New patrol route" onClick={newRoute}>+</button>}</p>
                    {(data?.routes ?? []).filter(r => match(r.name, r.id)).map(r => <button key={r.id} className={selected === r.id ? 'tree-item active' : 'tree-item'}
                        onClick={() => choose(r.id, r.posts[0]?.cell)}><span className="tree-icon" style={{color: '#e0b85a'}}>⤳</span>
                        <span className="tree-label">{r.name}</span><small>{r.posts.length} posts</small></button>)}
                    {data && !data.routes.length && <p className="tree-empty">No patrol routes yet.</p>}
                </>}
                {layer === 'areas' && AREA_KINDS.map(k => <div key={k.id}>
                    <p className="dm-group">{k.label} areas {canAct && k.id === 'wander' && <button className="icon" title="Paint a new area" onClick={newArea}>+</button>}</p>
                    {(data?.areas ?? []).filter(a => a.kind === k.id && match(a.name, a.id)).map(a => <button key={a.id} className={selected === a.id ? 'tree-item active' : 'tree-item'}
                        onClick={() => choose(a.id, a.cell)}><span className="tree-icon" style={{color: k.color}}>▦</span>
                        <span className="tree-label">{a.name}</span><small>{a.tiles.length} tiles</small></button>)}
                </div>)}
                {layer === 'spawns' && <>
                    <p className="dm-group">Spawn rules {canAct && <button className="icon" title="New spawn rule" onClick={newRule}>+</button>}</p>
                    {(data?.spawns ?? []).filter(r => match(r.name, r.id)).map(r => <button key={r.id} className={selected === r.id ? 'tree-item active' : 'tree-item'}
                        onClick={() => choose(r.id, data?.areas.find(a => a.id === r.area)?.cell)}>
                        <span className="tree-icon" style={{color: r.enabled ? '#d98b5f' : 'var(--faint)'}}>✺</span>
                        <span className="tree-label">{r.name}</span><small>{r.alive}/{r.count}</small></button>)}
                    {data && !data.spawns.length && <p className="tree-empty">No spawn rules yet.</p>}
                </>}
            </div>
        </nav>
        <main className="stage">
            <div className="tool-options">
                <div className="crumbs"><div className="segmented dm-layers" role="tablist" aria-label="Layer">{LAYERS.map(l =>
                    <button key={l.id} role="tab" aria-selected={layer === l.id} className={layer === l.id ? 'on' : ''} onClick={() => switchLayer(l.id)}>{l.icon} {l.label}</button>)}</div>
                    <span className="crumb-label">{surface?.title ?? 'Loading…'}</span></div>
                <div className="options">
                    {pick ? <><b className="tool-name">{pick === 'new' ? 'Place a new NPC' : `Pick ${draft?.name}'s ${pick} place`}</b>
                        {pick === 'new' && <div className="segmented">{M.ROLES.map(r => <button key={r} className={role === r ? 'on' : ''} onClick={() => setRole(r as Role)}
                            style={{color: role === r ? ROLE_INFO[r as Role].color : undefined}}>{ROLE_INFO[r as Role].icon} {ROLE_INFO[r as Role].label}</button>)}</div>}
                        <span className="tool-help">Click open ground on the map · Esc to cancel</span>
                        <button onClick={() => setPick(null)}>Cancel</button></>
                        : <><span className="tool-help">{help[layer]}</span>
                            {canAct && layer === 'npcs' && <button className="primary" onClick={() => setPick('new')}>+ Place new NPC</button>}
                            {canAct && layer === 'routes' && <button className="primary" onClick={newRoute}>+ New route</button>}
                            {canAct && layer === 'areas' && <button className="primary" onClick={newArea}>+ Paint new area</button>}
                            {canAct && layer === 'spawns' && <button className="primary" onClick={newRule}>+ New spawn rule</button>}</>}
                </div>
            </div>
            {surface && world ? <MapView surface={surface} world={world} markers={markers} overlay={overlay} selected={selected} mode={mode}
                onClick={onMapClick} onPaint={onPaint} onOpen={id => setView({kind: 'cell', id})} /> : <div className="dm-center"><p className="hint">{problem || 'Loading the world…'}</p></div>}
        </main>
        <aside className="inspector" aria-label="Inspector">
            {problem && <p className="hint dm-note">{problem}</p>}
            {layer === 'npcs' && (draft ? <><NpcInspector draft={draft} setDraft={setDraft} routes={data?.routes ?? []} world={world!} canAct={canAct} busy={busy}
                dirty={dirty} isNew={isNew} dead={dead.has(draft.id)} target={target} actions={actions} areas={data?.areas ?? []}
                wander={data?.wanders[draft.id] ?? ''} spawnedBy={data?.spawns.find(r => r.id === data.spawned[draft.id])?.name ?? (data?.spawned[draft.id] ? 'a deleted rule' : '')}
                onWander={area => wander(draft.id, draft.name, area)}
                onPick={p => setPick(p)} onSave={save} onRevert={revert} onDelete={remove} onLife={kill => life(draft.id, draft.name, kill)} />
                {!isNew && <LifePanel me={me} target={target} id={draft.id} name={draft.name} />}</>
                : holder ? <div className="dm-panel"><header><div><small>JOB HOLDER</small><h2>{holder.name}</h2></div>
                    {dead.has(holder.id) && <span className="dm-status dead">✝ dead</span>}</header>
                    <Hint>Holds the “{holder.slot}” profession slot. Their details come from the character roster and the slot; edit those in Atlas for now.</Hint>
                    {canAct && <div className="button-grid"><button className="danger" disabled={busy || dead.has(holder.id)} onClick={() => life(holder.id, holder.name, true)}>✝ Kill</button>
                        <button disabled={busy || !dead.has(holder.id)} onClick={() => life(holder.id, holder.name, false)}>Revive</button></div>}
                    <ActionList actions={actions} target={target} />
                    <LifePanel me={me} target={target} id={holder.id} name={holder.name} /></div>
                : <Overview title="NPC MANAGEMENT" target={target}>
                    <Hint>{data ? `${data.people.length} named NPCs and ${data.holders.length} job holders; ${data.dead.length} dead.` : 'Loading…'}</Hint>
                    <Hint>Select an NPC on the map or in the list, or place a new one. Saving writes the NPC to the {target.toUpperCase()} database and a running game server takes them over within a second; without a server, the change applies at its next start.</Hint></Overview>)}
            {layer === 'routes' && world && (routeDraft ? <><RouteInspector {...common} route={routeDraft} setRoute={setRouteDraft} world={world}
                users={peopleNamed((data?.people ?? []).filter(p => p.route === routeDraft.id).map(p => p.id))} adding={addingPosts} setAdding={setAddingPosts}
                onSave={saveRoute} onDelete={() => deleteLayer('the route', routeDraft.name, () => dmApi.deleteRoute(target, routeDraft.id))} />
                <ActionList actions={actions} target={target} /></>
                : <Overview title="PATROL ROUTES" target={target}><Hint>{data?.routes.length ?? 0} routes. Select one to change its posts, or make a new one; guards take a changed route at once.</Hint></Overview>)}
            {layer === 'areas' && world && (areaDraft ? <><AreaInspector {...common} area={areaDraft} setArea={setAreaDraft} world={world} brush={brush} setBrush={setBrush}
                wanderers={peopleNamed(Object.entries(data?.wanders ?? {}).filter(([, a]) => a === areaDraft.id).map(([id]) => id))}
                rules={(data?.spawns ?? []).filter(r => r.area === areaDraft.id).map(r => r.name)}
                onSave={saveArea} onDelete={() => deleteLayer('the area', areaDraft.name, () => dmApi.deleteArea(target, areaDraft.id))} />
                <ActionList actions={actions} target={target} /></>
                : <Overview title="NPC AREAS" target={target}><Hint>{data?.areas.length ?? 0} areas. Select one to repaint it, or paint a new one. An area lies within one cell or interior.</Hint></Overview>)}
            {layer === 'spawns' && (spawnDraft ? <SpawnInspector {...common} rule={spawnDraft} setRule={setSpawnDraft} status={savedRule} areas={data?.areas ?? []}
                templates={(data?.people ?? []).filter(p => !data?.spawned[p.id])}
                onSave={saveRule} onDelete={() => deleteLayer('the spawn rule', spawnDraft.name, () => dmApi.deleteSpawn(target, spawnDraft.id))} />
                : <Overview title="SPAWN RULES" target={target}>
                    <Hint>{data ? `${data.spawns.length} rules keeping ${data.spawns.reduce((n, r) => n + r.alive, 0)} NPCs alive.` : 'Loading…'}</Hint>
                    <Hint>A rule needs an area to appear in (paint one on the Areas layer) and a named NPC to copy.</Hint></Overview>)}
        </aside>
    </div>;
}

function Overview({title, target, children}: {title: string; target: Target; children: React.ReactNode}) {
    return <div className="dm-panel"><header><div><small>{title}</small><h2>{target === 'prod' ? 'The live world' : 'The rehearsal world'}</h2></div></header>{children}</div>;
}

function NpcInspector({draft, setDraft, routes, world, canAct, busy, dirty, isNew, dead, target, actions, areas, wander, spawnedBy, onWander, onPick, onSave, onRevert, onDelete, onLife}: {
    draft: Person; setDraft: (p: Person) => void; routes: {id: string; name: string}[]; world: Project; canAct: boolean; busy: boolean;
    dirty: boolean; isNew: boolean; dead: boolean; target: Target; actions: Action[]; areas: NpcArea[]; wander: string; spawnedBy: string;
    onWander: (area: string) => void; onPick: (p: Pick) => void; onSave: () => void; onRevert: () => void; onDelete: () => void; onLife: (kill: boolean) => void;
}) {
    const set = (fields: Partial<Person>) => setDraft({...draft, ...fields});
    const look = draft.appearance;
    const info = ROLE_INFO[draft.role];
    const placeName = (p: Place) => `${M.getCell(world, p.cell)?.name ?? p.cell} · ${p.x}, ${p.y}`;
    const wanderAreas = areas.filter(a => a.kind === 'wander' || a.id === wander);
    return <div className="dm-panel">
        <header><div><small>{isNew ? 'NEW NPC · NOT SAVED' : info.label.toUpperCase()}</small><h2>{draft.name}</h2></div>
            {dead ? <span className="dm-status dead">✝ dead</span> : <span className="role-chip" style={{borderColor: info.color, color: info.color}}>{info.icon}</span>}</header>
        {spawnedBy && <Hint>Spawned by the rule “{spawnedBy}”. When they fall, the rule clears them and brings someone new.</Hint>}
        <fieldset disabled={!canAct} className="dm-fields">
            <Row><TextField label="Name" value={draft.name} max={120} onCommit={v => set({name: v})} />
                <NumberField label="Age" value={draft.age} min={0} max={200} onCommit={v => set({age: v})} /></Row>
            <SelectField label="Role" value={draft.role} hint={info.blurb} options={M.ROLES.map(r => ({value: r, label: ROLE_INFO[r as Role].label}))}
                onChange={v => set({role: v as Role, route: v === 'guard' ? draft.route : ''})} />
            <TextField label="What they are doing when at work" value={draft.workLabel} max={40} onCommit={v => set({workLabel: v})} />
            <Row><NumberField label="Starts (hour)" value={draft.hours.start} min={0} max={23.75} step={0.25} onCommit={v => set({hours: {...draft.hours, start: v}})} />
                <NumberField label="Ends (hour)" value={draft.hours.end} min={0} max={23.75} step={0.25} onCommit={v => set({hours: {...draft.hours, end: v}})} /></Row>
            <p className="meta">On duty {formatHour(draft.hours.start)}–{formatHour(draft.hours.end)}</p>
            {draft.role === 'guard' && <SelectField label="Patrol route" value={draft.route} onChange={v => set({route: v})}
                options={[{value: '', label: 'Hold the work post'}, ...routes.map(r => ({value: r.id, label: r.name}))]} />}
            <div className="field"><span>Spawn points</span>
                {(['work', 'home', 'evening'] as const).map(k => <div key={k} className="place-row"><div><small>{k}</small><span>{placeName(draft[k])}</span></div>
                    <button onClick={() => onPick(k)}>Pick</button></div>)}</div>
            {!isNew && draft.role === 'civilian' && <SelectField label="Wanders in" value={wander}
                hint={dirty ? 'Save or revert the other changes first.' : 'Roams this area during work hours and evenings instead of standing at one spot. Applies at once.'}
                options={[{value: '', label: 'Keeps to their spots'}, ...wanderAreas.map(a => ({value: a.id, label: a.name}))]}
                onChange={v => { if (!dirty && !busy) onWander(v); }} />}
            <Row><NumberField label="Purse" value={draft.purse} min={0} max={100000} onCommit={v => set({purse: v})} />
                <NumberField label="Meals" value={draft.meals} min={0} max={10000} onCommit={v => set({meals: v})} />
                <NumberField label="Herbs" value={draft.herbs} min={0} max={10000} onCommit={v => set({herbs: v})} /></Row>
            <Toggle label="Paid by the town treasury" checked={draft.paid} onChange={v => set({paid: v})} />
            <TextField label="Description" multiline value={draft.description} max={4000} onCommit={v => set({description: v})} />
            <TextField label="Greeting" value={draft.greeting} max={1024} onCommit={v => set({greeting: v})} />
            <TextField label="Personality" multiline value={draft.personality} max={2000} onCommit={v => set({personality: v})} />
            <TextField label="Backstory" multiline value={draft.backstory} max={6000} onCommit={v => set({backstory: v})} />
            <Row><SelectField label="Species" value={look.species} options={M.SPECIES.map(v => ({value: v, label: v}))} onChange={v => set({appearance: {...look, species: v}})} />
                <SelectField label="Sex" value={look.sex} options={M.SEXES.map(v => ({value: v, label: v}))} onChange={v => set({appearance: {...look, sex: v}})} /></Row>
            <Row><SelectField label="Stature" value={look.stature} options={M.STATURES.map(v => ({value: v, label: v}))} onChange={v => set({appearance: {...look, stature: v}})} />
                <SelectField label="Pattern" value={look.pattern} options={M.PATTERNS.map(v => ({value: v, label: v}))} onChange={v => set({appearance: {...look, pattern: v}})} /></Row>
        </fieldset>
        {canAct && <>
            <div className="button-grid">
                <button className={target === 'prod' ? 'publish' : 'primary'} disabled={busy || !dirty} onClick={onSave}>{busy ? 'Saving…' : `Save to ${target.toUpperCase()}`}</button>
                <button disabled={busy || !dirty} onClick={onRevert}>{isNew ? 'Discard' : 'Revert'}</button>
            </div>
            {!isNew && <div className="button-grid">
                <button className="danger" disabled={busy || dead} onClick={() => onLife(true)}>✝ Kill</button>
                <button disabled={busy || !dead} onClick={() => onLife(false)}>Revive</button>
                <button className="danger" disabled={busy} onClick={onDelete}>Delete</button>
            </div>}
        </>}
        <ActionList actions={actions} target={target} />
    </div>;
}

const ACTION_LABELS: Record<string, string> = {'npc.sync': 'sync', 'npc.kill': 'kill', 'npc.revive': 'revive', 'layers.sync': 'sync layers'};

function ActionList({actions, target}: {actions: Action[]; target: Target}) {
    if (!actions.length) return null;
    return <ol className="dm-actions">{actions.slice(0, 6).map(a => <li key={a.id} className={a.status}>
        <b>{ACTION_LABELS[a.kind] ?? a.kind}</b> by {a.by} · {new Date(a.at).toLocaleTimeString()}
        <span>{a.status === 'queued' ? `waiting for a ${target.toUpperCase()} game server…` : `${a.status}: ${a.result}`}</span></li>)}</ol>;
}
