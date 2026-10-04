// LIVE: the Dungeon Master's main screen (Docs/Design/34-dungeon-master-refresh.md, 1.1). The world map as it is now:
// every player and NPC where they are, with layers of detail switched on as wanted (all off at the start), and
// moving and spawning from the map. Positions come from the game server's watch frames (RatwWatch.h), written only
// while a Dungeon Master is watching, so this screen asks for one every couple of seconds while it is open.
import {useCallback, useEffect, useMemo, useState} from 'react';
import * as M from '../model/model.mjs';
import type {Person, Place, Role} from '../model/model.mjs';
import {ROLE_INFO} from '../lib/glyphs';
import {surfaceFor} from '../lib/surface';
import {Hint} from '../components/fields';
import {useWorld} from './world';
import {CalendarPanel} from './CalendarPanel';
import {MapView, type Marker, type Mode, type Overlay} from './MapView';
import {areaColor, idFrom} from './LayerPanels';
import {dmApi, type Action, type Chapters, type Factions, type Live, type LiveEvent, type LivePerson, type Me, type Npcs, type Target} from './api';

type View = {kind: 'world'} | {kind: 'cell'; id: string};
type LayerId = 'players' | 'npcs' | 'shops' | 'structures' | 'chapters' | 'factions' | 'routes' | 'events';
const LAYERS: {id: LayerId; label: string; icon: string; color: string}[] = [
    {id: 'players', label: 'Players', icon: '☺', color: '#7fc8f8'},
    {id: 'npcs', label: 'NPCs', icon: '☻', color: '#a8c7ad'},
    {id: 'shops', label: 'Shops', icon: '⚖', color: '#e6c481'},
    {id: 'structures', label: 'Structures', icon: '▣', color: '#c9b79c'},
    {id: 'chapters', label: 'Chapters', icon: '♞', color: '#cfa3d9'},
    {id: 'factions', label: 'Factions', icon: '⚑', color: '#e1aba2'},
    {id: 'routes', label: 'Routes & areas', icon: '⤳', color: '#e0b85a'},
    {id: 'events', label: 'Events', icon: '!', color: '#d98b5f'},
];
// Layers whose systems aren't built yet (doc 34): shown, so the DM knows they are coming.
const LATER = ['Stories', 'Quests', 'Rumours', 'Weather'];
const ROAD = '#d98b5f', OFFLINE = '#7d8a85';
const CRIMES = new Set(['theft', 'attempted theft', 'warrant', 'arrest', 'reported', 'raid', 'fine paid', 'released']);
const POLL_MS = 2000;

type Pick = {kind: 'move'; who: LivePerson} | {kind: 'spawn'} | null;
interface SpawnDraft { place: Place; name: string; role: Role; copy: string }

export function LiveTab({me, target}: {me: Me; target: Target}) {
    const [problem, setProblem] = useState('');
    const world = useWorld(target, setProblem);
    const [live, setLive] = useState<Live | null>(null);
    const [view, setView] = useState<View>({kind: 'world'});
    const [on, setOn] = useState<Set<LayerId>>(() => new Set());   // Every layer off at the start.
    const [showOffline, setShowOffline] = useState(true), [showRoad, setShowRoad] = useState(true), [showDead, setShowDead] = useState(true);
    const [npcRole, setNpcRole] = useState('');
    const [query, setQuery] = useState('');
    const [selected, setSelected] = useState<string | null>(null);
    const [pick, setPick] = useState<Pick>(null);
    const [spawn, setSpawn] = useState<SpawnDraft | null>(null);
    const [busy, setBusy] = useState(false);
    const [note, setNote] = useState('');
    const [rail, setRail] = useState(true);
    const [npcs, setNpcs] = useState<Npcs | null>(null);
    const [factions, setFactions] = useState<Factions | null>(null);
    const [chapters, setChapters] = useState<Chapters | null>(null);
    const canAct = me.role !== 'viewer';
    const prod = target === 'prod';

    // Watching: one frame now, then every couple of seconds while the tab is open and the page visible.
    const poll = useCallback(() => dmApi.live(target).then(l => { setLive(l); setProblem(''); }).catch(e => setProblem((e as Error).message)), [target]);
    useEffect(() => {
        void poll();
        const t = setInterval(() => { if (document.visibilityState === 'visible') void poll(); }, POLL_MS);
        return () => clearInterval(t);
    }, [poll]);
    useEffect(() => {
        const esc = (e: KeyboardEvent) => { if (e.key === 'Escape') setPick(null); };
        window.addEventListener('keydown', esc);
        return () => window.removeEventListener('keydown', esc);
    }, []);
    // The slower layers are read when first switched on (and NPC data when spawning, for its copies).
    const loadNpcs = useCallback(() => dmApi.npcs(target).then(setNpcs).catch(e => setProblem((e as Error).message)), [target]);
    useEffect(() => { if ((on.has('routes') || pick?.kind === 'spawn') && !npcs) void loadNpcs(); }, [on, pick, npcs, loadNpcs]);
    useEffect(() => { if (on.has('factions') && !factions) dmApi.factions(target).then(setFactions).catch(e => setProblem((e as Error).message)); }, [on, factions, target]);
    useEffect(() => { if (on.has('chapters') && !chapters) dmApi.chapters(target).then(setChapters).catch(e => setProblem((e as Error).message)); }, [on, chapters, target]);

    const toggle = (id: LayerId) => setOn(s => { const n = new Set(s); if (n.has(id)) n.delete(id); else n.add(id); return n; });
    const surface = useMemo(() => (world ? surfaceFor(world, view as never) : null), [world, view]);
    const rooms = useMemo(() => new Map((world?.rooms ?? []).map(r => [r.id, r])), [world]);
    const placeName = useCallback((cell: string) => world?.cells.find(c => c.id === cell)?.name ?? rooms.get(cell)?.name ?? cell, [world, rooms]);
    /** Where a spot in a place falls on the map being shown: an interior sits at its overview spot on the world map. */
    const at = useCallback((cell: string, x: number, y: number): [number, number] | null => {
        if (!surface) return null;
        const p = surface.fromPlace({cell, x, y} as Place);
        if (p) return p;
        const room = view.kind === 'world' ? rooms.get(cell) : undefined;
        return room ? [room.worldX + .5, room.worldY + .5] : null;
    }, [surface, view, rooms]);
    const centre = useCallback((cell: string): [number, number] | null => {
        const c = world?.cells.find(x => x.id === cell) ?? rooms.get(cell);
        return c ? at(cell, c.width / 2, c.height / 2) : null;
    }, [world, rooms, at]);

    const people = live?.frame?.people ?? [];
    const q = query.trim().toLowerCase();
    const matches = (p: LivePerson) => !q || `${p[1]} ${p[0]} ${p[7]}`.toLowerCase().includes(q);
    const shownPeople = useMemo(() => people.filter(p => {
        if (!showDead && p[6] & 1) return false;
        if (p[2] === 'p' || p[2] === 'o') return on.has('players') && (p[2] === 'p' || showOffline) && matches(p);
        return on.has('npcs') && (p[2] !== 'r' || showRoad) && (!npcRole || (npcRole === 'other' ? !(p[7] in ROLE_INFO) : p[7] === npcRole)) && matches(p);
    }), [people, on, showOffline, showRoad, showDead, npcRole, q]); // eslint-disable-line react-hooks/exhaustive-deps

    const markers = useMemo((): Marker[] => {
        const out: Marker[] = [];
        const push = (id: string, spot: [number, number] | null, color: string, glyph: string, label: string, dead = false, editable = false) => {
            if (spot) out.push({id, x: spot[0], y: spot[1], color, glyph, label, dead, editable});
        };
        if (on.has('structures') && world) {
            if (view.kind === 'world') for (const r of world.rooms) push(`x:${r.id}`, at(r.id, 0, 0), '#c9b79c', '▣', r.name);
            else for (const l of world.links) for (const end of [l.a, l.b]) if (end.cell === view.id)
                push(`l:${l.id}:${end === l.a ? 'a' : 'b'}`, at(end.cell, end.x + .5, end.y + .5), '#c9b79c', l.kind === 'stairs' ? '≡' : l.kind === 'passage' ? '⇢' : '⊓',
                    `${l.name || l.kind} → ${placeName(end === l.a ? l.b.cell : l.a.cell)}`);
        }
        if (on.has('chapters') && chapters) for (const site of chapters.sites) {
            const ch = chapters.chapters.find(c => c.id === site.chapter);
            push(`c:${site.id}`, at(site.cell, site.x + .5, site.y + .5), ch?.colour || '#cfa3d9', '♞', `${site.name} · ${ch?.name ?? site.chapter} (${site.state})`);
        }
        if (on.has('shops')) for (const s of live?.frame?.shops ?? [])
            push(`s:${s[0]}`, at(s[3], s[4], s[5]), '#e6c481', '⚖', `${s[2] || 'Shop'} · ${s[1]}${s[6] ? ' (at a stall)' : ''}`, false, true);
        if (on.has('events')) for (const e of live?.events ?? [])
            push(`e:${e.id}`, centre(e.cell), CRIMES.has(e.kind) ? '#e07a6a' : e.kind.startsWith('caravan') ? '#e0b85a' : '#b8b0a0', '!', eventLine(e, placeName));
        for (const p of shownPeople) {
            const player = p[2] === 'p' || p[2] === 'o';
            const color = p[2] === 'o' ? OFFLINE : player ? '#7fc8f8' : p[2] === 'r' ? ROAD : ROLE_INFO[p[7] as Role]?.color ?? '#a8c7ad';
            const glyph = player ? '☺' : p[2] === 'r' ? '⛟' : ROLE_INFO[p[7] as Role]?.icon ?? '●';
            push(`${player ? 'p' : 'n'}:${p[0]}`, at(p[3], p[4], p[5]), color, glyph, personLine(p), !!(p[6] & 1), p[2] !== 'o');
        }
        return out;
    }, [on, world, view, at, centre, placeName, chapters, live, shownPeople]);

    const overlay = useMemo((): Overlay => {
        const out: Overlay = {tiles: [], rects: [], paths: []};
        if (!surface || !world) return out;
        if (on.has('factions') && factions) for (const c of factions.claims) {
            const color = factions.factions.find(f => f.id === c.faction)?.color || '#e1aba2';
            if (!c.tiles.length) {
                const cell = world.cells.find(x => x.id === c.area);
                if (view.kind === 'world' && cell) out.rects.push({x: cell.x, y: cell.y, w: cell.width, h: cell.height, color, strong: false});
                else if (view.kind === 'cell' && view.id === c.area) out.rects.push({x: 0, y: 0, w: surface.width, h: surface.height, color, strong: false});
            } else for (const [x, y] of c.tiles) { const p = surface.fromPlace({cell: c.area, x, y}); if (p) out.tiles.push({x: p[0], y: p[1], color, strong: false}); }
        }
        if (on.has('routes') && npcs) {
            for (const a of npcs.areas) for (const [x, y] of a.tiles) { const p = surface.fromPlace({cell: a.cell, x, y}); if (p) out.tiles.push({x: p[0], y: p[1], color: areaColor(a.kind), strong: false}); }
            for (const r of npcs.routes) {
                const points = r.posts.map(p => surface.fromPlace(p)).filter((p): p is [number, number] => !!p).map(([x, y]) => [x + .5, y + .5] as [number, number]);
                out.paths.push({points, color: 'rgba(224,184,90,.7)', numbered: false, loop: points.length === r.posts.length});
            }
        }
        return out;
    }, [surface, world, view, on, factions, npcs]);

    // ---- what is selected
    const person = selected && (selected.startsWith('p:') || selected.startsWith('n:')) ? people.find(p => p[0] === selected.slice(2)) ?? null : null;
    const shop = selected?.startsWith('s:') ? live?.frame?.shops.find(s => s[0] === selected.slice(2)) ?? null : null;
    const event = selected?.startsWith('e:') ? live?.events.find(e => `e:${e.id}` === selected) ?? null : null;
    const site = selected?.startsWith('c:') ? chapters?.sites.find(s => `c:${s.id}` === selected) ?? null : null;
    const room = selected?.startsWith('x:') ? rooms.get(selected.slice(2)) ?? null : null;

    const run = async (work: () => Promise<unknown>, done: string) => {
        setBusy(true); setNote('');
        try { await work(); setNote(done); await poll(); }
        catch (error) { setNote((error as Error).message); }
        finally { setBusy(false); }
    };
    const sure = (what: string) => !prod || window.confirm(`${what} on PROD?`);
    const onMapClick = (place: Place | null, marker: Marker | null) => {
        if (pick) {
            if (!place) { setNote('Choose a tile inside a place.'); return; }
            if (pick.kind === 'move') {
                const who = pick.who, kind = who[2] === 'n' || who[2] === 'r' ? 'npc.move' : 'character.move';
                setPick(null);
                if (!sure(`Move ${who[1]} to ${placeName(place.cell)} ${place.x}, ${place.y}`)) return;
                void run(() => dmApi.move(target, kind, who[0], place), `Moving ${who[1]}: waiting for the game server…`);
            } else {
                setPick(null);
                setSpawn({place, name: '', role: 'civilian', copy: ''});
                setSelected(null);
            }
            return;
        }
        setSpawn(null);
        setSelected(marker?.id ?? null);
    };
    const saveSpawn = () => {
        if (!spawn || !world || !npcs) return;
        const taken = [...npcs.people.map(p => p.id), ...npcs.holders.map(h => h.id)];
        const source = npcs.people.find(p => p.id === spawn.copy);
        const name = spawn.name.trim() || (source ? `${source.name} (copy)` : '');
        if (!name) { setNote('Give the newcomer a name.'); return; }
        const base: Person = source ? {...M.clone(source), name, home: spawn.place, work: spawn.place, evening: spawn.place}
            : M.newPerson({...world, people: npcs.people}, spawn.role, spawn.place, name);
        const newcomer = {...base, id: idFrom(name, taken, 'npc')};
        if (!sure(`Spawn ${name} in ${placeName(spawn.place.cell)}`)) return;
        void run(async () => { await dmApi.saveNpc(target, newcomer); setSpawn(null); setNpcs(null); },
            `${name} is written to the ${target.toUpperCase()} world; a running game server brings them in within a second.`);
    };
    const life = (p: LivePerson, kill: boolean) => {
        const npc = p[2] === 'n' || p[2] === 'r';
        if (!sure(`${kill ? 'Kill' : npc ? 'Revive' : 'Resurrect'} ${p[1]}`) || (kill && prod && !window.confirm(`Really kill ${p[1]}?`))) return;
        void run(() => npc ? dmApi.npcLife(target, p[0], kill) : dmApi.act(target, kill ? 'character.kill' : 'character.resurrect', p[0], ''),
            `${kill ? 'Killing' : 'Bringing back'} ${p[1]}: waiting for the game server…`);
    };

    const counts = useMemo(() => {
        const n = {online: 0, offline: 0, npcs: 0, road: 0};
        for (const p of people) { if (p[2] === 'p') n.online++; else if (p[2] === 'o') n.offline++; else if (p[2] === 'r') n.road++; else n.npcs++; }
        return n;
    }, [people]);
    const layerCount: Partial<Record<LayerId, string>> = {
        players: `${counts.online} on · ${counts.offline} off`, npcs: String(counts.npcs + counts.road),
        shops: live?.frame ? String(live.frame.shops.length) : undefined, events: live ? String(live.events.length) : undefined,
    };
    const found = q ? people.filter(matches).slice(0, 40) : [];
    const stale = !live?.frame || live.age === null || live.age > 10;
    // What needs a look: refusals of the last hour (the host sends the day's).
    const alerts = (live?.actions ?? []).filter(a => (a.status === 'refused' || a.status === 'expired') && Date.now() - Date.parse(a.at) < 3600e3).slice(0, 5);
    const mode: Mode = pick ? 'pick' : 'select';
    const titleOf = view.kind === 'world' ? 'World' : placeName(view.id);

    return <div className="dm-npcs dm-live">
        <nav className="explorer" aria-label="Layers">
            <div className="explorer-search"><input value={query} onChange={e => setQuery(e.target.value)} placeholder="Find someone…" aria-label="Find a player or NPC" /></div>
            <div className="explorer-scroll">
                {found.length > 0 && <>
                    <p className="dm-group">Found</p>
                    {found.map(p => <button key={p[0]} className={selected?.slice(2) === p[0] ? 'tree-item active' : 'tree-item'} onClick={() => {
                        const player = p[2] === 'p' || p[2] === 'o';
                        setOn(s => new Set(s).add(player ? 'players' : 'npcs'));
                        setSelected(`${player ? 'p' : 'n'}:${p[0]}`); setView({kind: 'cell', id: p[3]});
                    }}><span className="tree-icon">{p[2] === 'p' || p[2] === 'o' ? '☺' : '☻'}</span><span className="tree-label">{p[1]}</span><small>{placeName(p[3])}</small></button>)}
                </>}
                <p className="dm-group">Layers</p>
                {LAYERS.map(l => <label key={l.id} className="live-layer">
                    <input type="checkbox" checked={on.has(l.id)} onChange={() => toggle(l.id)} />
                    <span className="tree-icon" style={{color: l.color}}>{l.icon}</span><span className="tree-label">{l.label}</span>
                    {layerCount[l.id] && <small>{layerCount[l.id]}</small>}
                </label>)}
                {on.has('players') && <div className="live-filters">
                    <label><input type="checkbox" checked={showOffline} onChange={e => setShowOffline(e.target.checked)} /> Characters not in the world (where they were saved)</label>
                </div>}
                {on.has('npcs') && <div className="live-filters">
                    <label><input type="checkbox" checked={showRoad} onChange={e => setShowRoad(e.target.checked)} /> Folk of the road (caravans, bandits)</label>
                    <label>Role <select value={npcRole} onChange={e => setNpcRole(e.target.value)}>
                        <option value="">all</option>{Object.entries(ROLE_INFO).map(([id, r]) => <option key={id} value={id}>{r.label}</option>)}<option value="other">other</option>
                    </select></label>
                </div>}
                {(on.has('players') || on.has('npcs')) && <div className="live-filters">
                    <label><input type="checkbox" checked={showDead} onChange={e => setShowDead(e.target.checked)} /> The dead</label>
                </div>}
                <p className="dm-group">Coming later</p>
                {LATER.map(l => <span key={l} className="live-layer later">{l}</span>)}
            </div>
        </nav>
        <main className="stage">
            <div className="tool-options">
                <div className="crumbs">
                    {view.kind === 'cell' && <button onClick={() => setView({kind: 'world'})} title="Back to the whole world">◂ World</button>}
                    <span className="crumb-label">{titleOf}</span>
                    <span className={stale ? 'live-feed stale' : 'live-feed'} title="How old the positions are">
                        {!live ? (problem ? `Can't watch: ${problem === 'Not found.' ? 'the DM host is older than this page; restart it' : problem}` : 'Connecting…') : stale ? (live.age === null ? 'No positions yet: is an up-to-date game server running for this world?'
                            : `Positions ${Math.round(live.age!)}s old: has the game server stopped, or is it older than this page?`) : `● live · ${live.age!.toFixed(0)}s`}
                    </span>
                </div>
                <div className="options">
                    {pick ? <><b className="tool-name">{pick.kind === 'move' ? `Move ${pick.who[1]}` : 'Spawn an NPC'}</b>
                        <span className="tool-help">Click a tile on the map · Esc to cancel</span><button onClick={() => setPick(null)}>Cancel</button></>
                        : <><span className="tool-help">Wheel zooms · drag pans · double-click a place to open it</span>
                            {canAct && <button className="primary" onClick={() => { setSelected(null); setPick({kind: 'spawn'}); }}>+ Spawn NPC</button>}
                            <button onClick={() => setRail(r => !r)} title="Show or hide the side panel">{rail ? 'Hide panel ▸' : '◂ Panel'}</button></>}
                </div>
            </div>
            {surface && world ? <MapView surface={surface} world={world} markers={markers} overlay={overlay} selected={selected} mode={mode} cluster={4}
                onClick={onMapClick} onPaint={() => undefined} onOpen={id => setView({kind: 'cell', id})} />
                : <div className="dm-center"><p className="hint">{problem || 'Loading the world…'}</p></div>}
        </main>
        {rail && <aside className="inspector" aria-label="Inspector">
            {(note || problem) && <p className="hint dm-note">{note || problem}</p>}
            {spawn ? <div className="dm-panel"><header><div><small>SPAWN</small><h2>{placeName(spawn.place.cell)} · {spawn.place.x}, {spawn.place.y}</h2></div></header>
                <label className="field"><span>Name</span><input autoFocus value={spawn.name} onChange={e => setSpawn({...spawn, name: e.target.value})} placeholder={spawn.copy ? 'As the copy, if left empty' : 'Their name'} /></label>
                <label className="field"><span>A copy of</span><select value={spawn.copy} onChange={e => setSpawn({...spawn, copy: e.target.value})}>
                    <option value="">Nobody: someone new</option>
                    {(npcs?.people ?? []).slice().sort((a, b) => a.name.localeCompare(b.name)).map(p => <option key={p.id} value={p.id}>{p.name} ({p.workLabel || p.role})</option>)}
                </select></label>
                {!spawn.copy && <div className="segmented">{M.ROLES.map(r => <button key={r} className={spawn.role === r ? 'on' : ''} onClick={() => setSpawn({...spawn, role: r as Role})}
                    style={{color: spawn.role === r ? ROLE_INFO[r as Role].color : undefined}}>{ROLE_INFO[r as Role].icon} {ROLE_INFO[r as Role].label}</button>)}</div>}
                <Hint>They live, work and spend evenings here until changed in NPC Management. A copy takes the other’s role, looks, hours and personality.</Hint>
                <div className="button-grid"><button className="primary" disabled={busy || !npcs} onClick={saveSpawn}>{npcs ? 'Spawn' : 'Loading…'}</button>
                    <button onClick={() => setSpawn(null)}>Cancel</button></div>
            </div>
            : person ? <PersonPanel p={person} place={placeName(person[3])} canAct={canAct} busy={busy} target={target}
                actions={(live?.actions ?? []).filter(a => a.target === person[0])}
                onMove={() => setPick({kind: 'move', who: person})} onLife={kill => life(person, kill)} />
            : shop ? <div className="dm-panel"><header><div><small>SHOP</small><h2>{shop[2] || 'Shop'}</h2></div>{shop[6] && <span className="dm-status">at a stall</span>}</header>
                <Hint>Kept by {shop[1]}, in {placeName(shop[3])} at {Math.floor(shop[4])}, {Math.floor(shop[5])}.</Hint>
                <button onClick={() => { setOn(s => new Set(s).add('npcs')); setSelected(`n:${shop[0]}`); }}>Show {shop[1]}</button></div>
            : event ? <div className="dm-panel"><header><div><small>EVENT · DAY {Math.floor(event.day)}</small><h2>{event.kind}</h2></div></header>
                <Hint>{eventLine(event, placeName)}</Hint><Hint>Recorded {new Date(event.at).toLocaleString()}.</Hint></div>
            : site ? <div className="dm-panel"><header><div><small>CHAPTER SITE</small><h2>{site.name}</h2></div><span className="dm-status">{site.state}</span></header>
                <Hint>{chapters?.chapters.find(c => c.id === site.chapter)?.name ?? site.chapter} · {placeName(site.cell)} · {site.structures.length} buildings, {site.staff.length} staff.</Hint>
                <Hint>Manage it in the Chapters tab.</Hint></div>
            : room ? <div className="dm-panel"><header><div><small>STRUCTURE</small><h2>{room.name}</h2></div></header>
                <Hint>An interior, {room.width} × {room.height}.</Hint><button onClick={() => setView({kind: 'cell', id: room.id})}>Open it</button></div>
            : <div className="dm-panel"><header><div><small>LIVE</small><h2>{prod ? 'PROD: the players’ world' : 'DEV: the rehearsal world'}</h2></div></header>
                <Hint>{live?.frame ? `${counts.online} players in the world, ${counts.offline} away; ${counts.npcs} NPCs and ${counts.road} on the road.`
                    : 'Positions appear once a game server for this world is running; it writes them only while this screen is open.'}</Hint>
                {!on.size && <Hint>Every layer starts switched off. Switch on what you want to see on the left.</Hint>}
                {alerts.length > 0 && <><p className="dm-group">Needs a look</p><ActionList actions={alerts} /></>}
                {(live?.events.length ?? 0) > 0 && <><p className="dm-group">Lately</p>
                    <ol className="dm-actions">{live!.events.slice(0, 8).map(e => <li key={e.id} className="live-event" onClick={() => {
                        setOn(s => new Set(s).add('events')); setSelected(`e:${e.id}`);
                    }}><b>{e.kind}</b> · day {Math.floor(e.day)}<span>{eventLine(e, placeName)}</span></li>)}</ol></>}
                <CalendarPanel me={me} target={target} />
            </div>}
        </aside>}
    </div>;
}

function PersonPanel({p, place, canAct, busy, target, actions, onMove, onLife}: {p: LivePerson; place: string; canAct: boolean; busy: boolean; target: Target;
    actions: Action[]; onMove: () => void; onLife: (kill: boolean) => void}) {
    const player = p[2] === 'p' || p[2] === 'o', dead = !!(p[6] & 1);
    const kind = p[2] === 'p' ? 'PLAYER · IN THE WORLD' : p[2] === 'o' ? 'PLAYER · AWAY' : p[2] === 'r' ? 'FOLK OF THE ROAD' : 'NPC';
    const states = [dead && 'dead', p[6] & 2 && 'downed', p[6] & 8 && 'in a fight', p[6] & 4 && 'off stage'].filter(Boolean) as string[];
    return <div className="dm-panel">
        <header><div><small>{kind}</small><h2>{p[1]}</h2></div>{dead ? <span className="dm-status dead">✝ dead</span> : states.length > 0 && <span className="dm-status">{states[0]}</span>}</header>
        <Hint>{place} · {Math.floor(p[4])}, {Math.floor(p[5])}{!player && p[7] ? ` · ${ROLE_INFO[p[7] as Role]?.label ?? p[7]}` : ''}</Hint>
        {p[8] && <Hint>Now: {p[8]}</Hint>}
        {states.length > 1 && <Hint>Also: {states.slice(1).join(', ')}.</Hint>}
        {p[2] === 'r' && <Hint>Made by the roads as needed and never saved: moving them lasts only until the road takes them back.</Hint>}
        {canAct && <div className="button-grid">
            <button className="primary" disabled={busy} onClick={onMove} title={p[2] === 'o' ? 'They wake there' : 'Put them on a tile'}>Move…</button>
            {dead ? <button disabled={busy} onClick={() => onLife(false)}>{player ? 'Resurrect' : 'Revive'}</button>
                : <button className="danger" disabled={busy || p[2] === 'r'} onClick={() => onLife(true)}>✝ Kill</button>}
        </div>}
        <ActionList actions={actions} target={target} />
    </div>;
}

const LABELS: Record<string, string> = {'npc.move': 'Move', 'character.move': 'Move', 'npc.sync': 'NPC saved'};
function ActionList({actions, target}: {actions: Action[]; target?: Target}) {
    if (!actions.length) return null;
    return <ol className="dm-actions">{actions.slice(0, 6).map(a => <li key={a.id} className={a.status}>
        <b>{LABELS[a.kind] ?? a.kind}</b> {a.target} by {a.by} · {new Date(a.at).toLocaleTimeString()}
        <span>{a.status === 'queued' ? `waiting for a ${(target ?? 'game').toUpperCase()} game server…` : `${a.status}: ${a.result}`}</span></li>)}</ol>;
}

function personLine(p: LivePerson) {
    const what = p[2] === 'o' ? 'away' : p[2] === 'r' ? 'on the road' : p[8];
    return `${p[1]}${what ? ` · ${what}` : ''}`;
}

function eventLine(e: LiveEvent, placeName: (cell: string) => string) {
    const who = [e.actor, e.target].filter(Boolean).join(' → ');
    return `${e.kind}${who ? ` (${who})` : ''} in ${placeName(e.cell)}${e.detail ? `: ${e.detail}` : ''}`;
}
