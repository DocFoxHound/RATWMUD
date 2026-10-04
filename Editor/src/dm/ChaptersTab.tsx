// Chapters (Docs/Design/32-parties-chapters-factions.md): the players' clans as the game server last saved them, their
// camps and Holds on the map, and what waits on a Dungeon Master: treaties and requests to become a House. A pending
// one the Dungeon Master leaves alone for a game day is decided by the faction's own rule.
import {useCallback, useEffect, useMemo, useState} from 'react';
import {surfaceFor} from '../lib/surface';
import {Hint} from '../components/fields';
import {useWorld} from './world';
import {dmApi, type CampSite, type Chapter, type Chapters, type HouseRequest, type Me, type Target, type Treaty} from './api';
import {MapView, type Marker, type Overlay} from './MapView';

type View = {kind: 'world'} | {kind: 'cell'; id: string};
const RANKS = ['Head', 'Officer', 'Member', 'Initiate'];
/** Each building's glyph, as the game draws it (Core/RatwCamps.cpp). */
const GLYPHS: Record<string, string> = {
    tent: '^', firepit: '*', leanto: '/', storage: '#', hitching: '|', cookfire: '&', watchpost: 'T', palisade: '=', gate: 'H',
    hall: 'M', workshop: 'w', stable: 's', well: 'o', keep: 'K', wall: '#', tower: 'I', gatehouse: 'G',
};

export function ChaptersTab({me, target}: {me: Me; target: Target}) {
    const [data, setData] = useState<Chapters | null>(null);
    const [problem, setProblem] = useState('');
    const world = useWorld(target, setProblem);
    const [view, setView] = useState<View>({kind: 'world'});
    const [selected, setSelected] = useState<string | null>(null);
    const [busy, setBusy] = useState(false);
    const [reason, setReason] = useState('');
    const [query, setQuery] = useState('');
    const canAct = me.role !== 'viewer';

    const load = useCallback(() => dmApi.chapters(target).then(d => { setData(d); setProblem(''); }).catch(e => setProblem((e as Error).message)), [target]);
    useEffect(() => { void load(); }, [load, target]);
    const waiting = data?.actions.some(a => a.status === 'queued');
    useEffect(() => { if (!waiting) return; const t = setTimeout(load, 1500); return () => clearTimeout(t); }, [waiting, data, load]);

    const surface = useMemo(() => (world ? surfaceFor(world, view as never) : null), [world, view]);
    const colourOf = (id: string) => data?.chapters.find(c => c.id === id)?.colour || '#92bacd';
    const nameOf = (id: string) => data?.chapters.find(c => c.id === id)?.name ?? id;
    const {markers, overlay} = useMemo(() => {
        const out: Overlay = {tiles: [], rects: [], paths: []}, marks: Marker[] = [];
        if (!surface || !data) return {markers: marks, overlay: out};
        for (const site of data.sites) {
            const colour = colourOf(site.chapter), strong = site.chapter === selected;
            for (const b of site.structures) {
                const at = surface.fromPlace({cell: site.cell, x: b.x, y: b.y});
                if (!at) continue;
                out.tiles.push({x: at[0], y: at[1], color: colour, strong});
                if (surface.kind !== 'world')
                    marks.push({id: `${site.id}/${b.id}`, x: at[0] + .5, y: at[1] + .5, color: colour, glyph: GLYPHS[b.kind] ?? '?',
                        label: `${b.kind}${b.built ? '' : ' (being built)'} · ${Math.round(b.condition)}%`, dead: !b.built, editable: false});
            }
            const at = surface.fromPlace({cell: site.cell, x: site.x, y: site.y});
            if (at && surface.kind === 'world')
                marks.push({id: site.id, x: at[0] + .5, y: at[1] + .5, color: colour, glyph: '♞', label: `${site.name || 'Camp'} · ${nameOf(site.chapter)}`,
                    dead: site.state !== 'standing', editable: false});
        }
        return {markers: marks, overlay: out};
    }, [surface, data, selected]); // eslint-disable-line react-hooks/exhaustive-deps

    const decide = async (what: 'treaty' | 'house', id: string, approve: boolean, faction = '', words = '') => {
        if (target === 'prod' && !window.confirm(`${approve ? 'Approve' : 'Refuse'} ${words} on PROD?`)) return;
        setBusy(true); setProblem('');
        try { await dmApi.decide(target, what, id, approve, faction, reason.trim()); setReason(''); await load(); setProblem('Sent to the game server.'); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };

    const q = query.trim().toLowerCase();
    const pendingTreaties = (data?.treaties ?? []).filter(t => t.state === 'pending');
    const pendingHouses = (data?.houses ?? []).filter(h => h.state === 'pending');
    const chapter = data?.chapters.find(c => c.id === selected) ?? null;
    const choose = (c: Chapter) => {
        setSelected(c.id);
        const site = data?.sites.find(s => s.chapter === c.id);
        if (site) setView({kind: 'cell', id: site.cell});
    };

    return <div className="dm-npcs">
        <nav className="explorer" aria-label="Chapters">
            <div className="explorer-search"><input value={query} onChange={e => setQuery(e.target.value)} placeholder="Search Chapters…" /></div>
            <div className="explorer-scroll">
                <p className="dm-group">Waiting on you</p>
                {pendingTreaties.map(t => <button key={t.id} className={selected === t.chapter ? 'tree-item active' : 'tree-item'} onClick={() => setSelected(t.chapter)}>
                    <span className="tree-icon">✉</span><span className="tree-label">Treaty: {nameOf(t.chapter)}</span><small>{t.factionName}</small></button>)}
                {pendingHouses.map(h => <button key={`${h.chapter}|${h.faction}`} className={selected === h.chapter ? 'tree-item active' : 'tree-item'} onClick={() => setSelected(h.chapter)}>
                    <span className="tree-icon">♛</span><span className="tree-label">House: {nameOf(h.chapter)}</span><small>{h.factionName}</small></button>)}
                {data && !pendingTreaties.length && !pendingHouses.length && <p className="tree-empty">Nothing pending.</p>}
                <p className="dm-group">Chapters</p>
                {(data?.chapters ?? []).filter(c => !q || `${c.name} ${c.id}`.toLowerCase().includes(q)).map(c =>
                    <button key={c.id} className={selected === c.id ? 'tree-item active' : 'tree-item'} onClick={() => choose(c)}>
                        <span className="tree-icon" style={{color: c.colour}}>♞</span><span className="tree-label">{c.name}</span><small>level {c.level}</small></button>)}
                {data && !data.chapters.length && <p className="tree-empty">No Chapters yet.</p>}
                <p className="dm-group">Map</p>
                <button className={view.kind === 'world' ? 'tree-item active' : 'tree-item'} onClick={() => setView({kind: 'world'})}>
                    <span className="tree-icon">◇</span><span className="tree-label">World overview</span></button>
                {(data?.sites ?? []).map(s => <button key={s.id} className={view.kind === 'cell' && view.id === s.cell ? 'tree-item active' : 'tree-item'}
                    onClick={() => setView({kind: 'cell', id: s.cell})}><span className="tree-icon" style={{color: colourOf(s.chapter)}}>▦</span>
                    <span className="tree-label">{s.name || s.place}</span><small>{s.structures.filter(b => b.built).length} built</small></button>)}
            </div>
        </nav>
        <main className="stage">
            <div className="tool-options">
                <div className="crumbs"><span className="crumb-label">{surface?.title ?? 'Loading…'}</span></div>
                <div className="options"><span className="tool-help">Camps and Holds as last saved; buildings being raised are drawn faint. Open a camp to see each building.</span>
                    <button onClick={() => void load()} title="Reload from the database">Refresh</button></div>
            </div>
            {surface && world ? <MapView surface={surface} world={world} markers={markers} overlay={overlay} selected={null} mode="select"
                onClick={(_, marker) => { const site = marker && data?.sites.find(s => s.id === marker.id.split('/')[0]); if (site) setSelected(site.chapter); }}
                onPaint={() => undefined} onOpen={id => setView({kind: 'cell', id})} />
                : <div className="dm-center"><p className="hint">{problem || 'Loading the world…'}</p></div>}
        </main>
        <aside className="inspector" aria-label="Inspector">
            {problem && <p className="hint dm-note">{problem}</p>}
            {data && (pendingTreaties.length > 0 || pendingHouses.length > 0) && <div className="dm-panel">
                <header><div><small>WAITING ON A DUNGEON MASTER</small><h2>Pending</h2></div></header>
                <Hint>Left alone for a game day, the faction decides by its own rule: a treaty if the Chapter is Trusted, a House if it is level 5 and Sworn.</Hint>
                {canAct && <label className="field"><span>Reason (for the audit log)</span>
                    <input value={reason} maxLength={400} onChange={e => setReason(e.target.value)} placeholder="Optional" /></label>}
                {pendingTreaties.map(t => <TreatyCard key={t.id} t={t} chapter={nameOf(t.chapter)} canAct={canAct} busy={busy}
                    onDecide={ok => decide('treaty', t.id, ok, '', `the treaty between ${nameOf(t.chapter)} and ${t.factionName}`)} />)}
                {pendingHouses.map(h => <HouseCard key={`${h.chapter}|${h.faction}`} h={h} chapter={nameOf(h.chapter)} canAct={canAct} busy={busy}
                    onDecide={ok => decide('house', h.chapter, ok, h.faction, `${nameOf(h.chapter)} as a House of ${h.factionName}`)} />)}
                {!canAct && <p className="hint">Your account can view but not decide.</p>}
            </div>}
            {chapter ? <ChapterPanel chapter={chapter} data={data!} /> : <div className="dm-panel">
                <header><div><small>CHAPTERS</small><h2>{target === 'prod' ? 'PROD: the players’ world' : 'DEV: the rehearsal world'}</h2></div></header>
                <Hint>{data ? `${data.chapters.length} Chapters, ${data.sites.length} camps, ${data.treaties.filter(t => t.state === 'active').length} treaties in force.` : 'Loading…'}</Hint>
                <Hint>Chapters are made and run by players in the game; this shows them as the game server last saved them (every few seconds).</Hint></div>}
            {data && data.actions.length > 0 && <ol className="dm-actions">{data.actions.slice(0, 8).map(a => <li key={a.id} className={a.status}>
                <b>{a.kind === 'treaty.decide' ? 'treaty' : 'House'} {a.payload.approve ? 'approved' : 'refused'}</b> by {a.by} · {new Date(a.at).toLocaleTimeString()}
                <span>{a.status === 'queued' ? `waiting for a ${target.toUpperCase()} game server…` : `${a.status}: ${a.result}`}</span></li>)}</ol>}
        </aside>
    </div>;
}

function TreatyCard({t, chapter, canAct, busy, onDecide}: {t: Treaty; chapter: string; canAct: boolean; busy: boolean; onDecide: (approve: boolean) => void}) {
    const terms = [t.build ? 'may build a Hold' : 'no building', t.tithe ? `tithe ${t.tithe} a week` : 'no tithe', t.levy ? 'answers levies' : 'no levies',
        t.labour ? 'may take on its residents' : 'keeps its residents', `${t.weeks} weeks`];
    return <div className="field"><span>Treaty · {chapter} with {t.factionName}</span>
        <p className="meta">{terms.join(' · ')}</p>
        {canAct && <div className="button-grid"><button className="primary" disabled={busy} onClick={() => onDecide(true)}>Approve</button>
            <button className="danger" disabled={busy} onClick={() => onDecide(false)}>Refuse</button></div>}</div>;
}

function HouseCard({h, chapter, canAct, busy, onDecide}: {h: HouseRequest; chapter: string; canAct: boolean; busy: boolean; onDecide: (approve: boolean) => void}) {
    return <div className="field"><span>House · {chapter} asks to serve {h.factionName}</span>
        <p className="meta">Asked on day {Math.floor(h.day)}.</p>
        {canAct && <div className="button-grid"><button className="primary" disabled={busy} onClick={() => onDecide(true)}>Approve</button>
            <button className="danger" disabled={busy} onClick={() => onDecide(false)}>Refuse</button></div>}</div>;
}

function ChapterPanel({chapter, data}: {chapter: Chapter; data: Chapters}) {
    const sites: CampSite[] = data.sites.filter(s => s.chapter === chapter.id);
    const treaties = data.treaties.filter(t => t.chapter === chapter.id && t.state !== 'pending');
    const levies = data.levies.filter(l => l.chapter === chapter.id && l.state === 'called');
    return <div className="dm-panel">
        <header><div><small>CHAPTER · LEVEL {chapter.level}</small><h2>{chapter.name}</h2></div>
            <span className="role-chip" style={{borderColor: chapter.colour, color: chapter.colour}}>♞</span></header>
        <p className="meta">{chapter.renown} renown · {chapter.members.length} members{chapter.holdName ? ` · holds ${chapter.holdName}` : ''}
            {chapter.houseOf ? ` · a House of ${data.treaties.find(t => t.faction === chapter.houseOf)?.factionName ?? chapter.houseOf}` : ''}
            {chapter.sworn ? ` · ${chapter.sworn} sworn residents` : ''}{chapter.toll ? ` · toll ${chapter.toll}` : ''}</p>
        {chapter.charter && <Hint>{chapter.charter}</Hint>}
        <div className="field"><span>Members</span><ul className="dm-list">{chapter.members.map(m => <li key={m.id}><span>{m.name}</span><small>{RANKS[m.rank] ?? m.rank}</small></li>)}</ul></div>
        {sites.map(s => <div key={s.id} className="field"><span>{s.name || 'Camp'} · {s.place}{s.state !== 'standing' ? ` (${s.state})` : ''}</span>
            <ul className="dm-list">{s.structures.map(b => <li key={b.id}><span>{GLYPHS[b.kind] ?? '?'} {b.kind}</span>
                <small>{b.built ? `${Math.round(b.condition)}%` : 'being built'} · {b.x}, {b.y}</small></li>)}</ul>
            {s.staff.length > 0 && <p className="meta">Staff: {s.staff.map(h => `${h.name} (${h.role}, ${h.wage}/wk)`).join(', ')}</p>}</div>)}
        {treaties.length > 0 && <div className="field"><span>Treaties</span><ul className="dm-list">{treaties.map(t => <li key={t.id}>
            <span>{t.factionName}</span><small>{t.state}{t.tithe ? ` · tithe ${t.tithe}` : ''}</small></li>)}</ul></div>}
        {levies.length > 0 && <div className="field"><span>Levies called</span><ul className="dm-list">{levies.map(l => <li key={l.id}>
            <span>{l.factionName}: {l.place}</span><small>{Math.round(100 * l.done / Math.max(1, l.needed))}%</small></li>)}</ul></div>}
    </div>;
}
