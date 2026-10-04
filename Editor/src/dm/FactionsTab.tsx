// Factions: every faction (NPC factions, cities, guilds, clans), the territory they claim, who belongs to them,
// and how they regard each other. Factions and claims reach a running game server at once; Atlas sees the factions
// and which places they claim. A claim is not control (design doc 16), and claims may overlap.
import {useCallback, useEffect, useMemo, useRef, useState} from 'react';
import type {Place} from '../model/model.mjs';
import {surfaceFor} from '../lib/surface';
import {Row, SelectField, Slider, TextField, Hint} from '../components/fields';
import {useWorld} from './world';
import {dmApi, type Action, type Claim, type Faction, type FactionKind, type Factions, type Me, type RelationChange, type Stance, type Target} from './api';
import {MapView, type Overlay} from './MapView';
import {idFrom, type Brush} from './LayerPanels';

type View = {kind: 'world'} | {kind: 'cell'; id: string};
type Sub = 'territory' | 'relations';

export const FACTION_KINDS: {id: FactionKind; label: string; icon: string}[] = [
    {id: 'city', label: 'Cities and towns', icon: '♜'}, {id: 'npc', label: 'NPC factions', icon: '⚑'},
    {id: 'guild', label: 'Guilds', icon: '⚒'}, {id: 'clan', label: 'Clans', icon: '♞'}, {id: 'other', label: 'Other', icon: '◇'},
];
export const STANCES: {id: Stance; label: string; color: string; from: number}[] = [
    {id: 'allied', label: 'Allied', color: '#5fb3d9', from: 60}, {id: 'friendly', label: 'Friendly', color: '#7fc27f', from: 20},
    {id: 'neutral', label: 'Neutral', color: '#9aa39a', from: -19}, {id: 'tense', label: 'Tense', color: '#d9c25f', from: -49},
    {id: 'hostile', label: 'Hostile', color: '#d98b5f', from: -79}, {id: 'war', label: 'At war', color: '#e0574f', from: -100},
];
const stanceOf = (id: Stance) => STANCES.find(s => s.id === id)!;
/** The stance a disposition suggests, when the DM has not chosen one. */
const suggested = (d: number): Stance => (STANCES.find(s => d >= s.from) ?? STANCES[STANCES.length - 1]).id;
const COLORS = ['#c9574b', '#d9a441', '#7fc27f', '#5fb3d9', '#9b7fd9', '#d97fb8', '#a8c7ad', '#e0b85a'];

export function FactionsTab({me, target}: {me: Me; target: Target}) {
    const [data, setData] = useState<Factions | null>(null);
    const [problem, setProblem] = useState('');
    const world = useWorld(target, setProblem);
    const [view, setView] = useState<View>({kind: 'world'});
    const [sub, setSub] = useState<Sub>('territory');
    const [selected, setSelected] = useState<string | null>(null);
    const [draft, setDraft] = useState<Faction | null>(null);
    const [isNew, setIsNew] = useState(false);
    const [painting, setPainting] = useState<Claim | null>(null);
    const [brush, setBrush] = useState<Brush>('paint');
    const [pair, setPair] = useState<[string, string] | null>(null);
    const [busy, setBusy] = useState(false);
    const [query, setQuery] = useState('');
    const canAct = me.role !== 'viewer';

    const load = useCallback(() => dmApi.factions(target).then(d => { setData(d); setProblem(''); }).catch(e => setProblem((e as Error).message)), [target]);
    useEffect(() => { void load(); }, [load, target]);
    const waiting = data?.actions.some(a => a.status === 'queued');
    useEffect(() => { if (!waiting) return; const t = setTimeout(load, 1500); return () => clearTimeout(t); }, [waiting, data, load]);

    const saved = data?.factions.find(f => f.id === selected) ?? null;
    useEffect(() => { if (!isNew) setDraft(saved ? {...saved} : null); }, [saved, isNew]);
    const dirty = !!draft && (isNew || JSON.stringify(draft) !== JSON.stringify(saved));
    const place = view.kind === 'cell' && world ? world.cells.find(c => c.id === view.id) ?? world.rooms.find(r => r.id === view.id) ?? null : null;
    const claimHere = selected && place ? data?.claims.find(c => c.faction === selected && c.area === place.id) ?? null : null;
    const colorOf = (id: string) => data?.factions.find(f => f.id === id)?.color ?? '#aaa';
    const nameOf = (id: string) => data?.factions.find(f => f.id === id)?.name ?? id;
    const placeName = (id: string) => (world?.cells.find(c => c.id === id) ?? world?.rooms.find(r => r.id === id))?.name ?? id;

    const surface = useMemo(() => (world ? surfaceFor(world, view as never) : null), [world, view]);
    const overlay = useMemo((): Overlay => {
        const out: Overlay = {tiles: [], rects: [], paths: []};
        if (!surface || !world || !data) return out;
        const claims = data.claims.filter(c => !(painting && c.faction === painting.faction && c.area === painting.area)).concat(painting ? [painting] : []);
        // The selected faction's claims are drawn last, on top.
        claims.sort((a, b) => Number(a.faction === selected) - Number(b.faction === selected));
        for (const c of claims) {
            const strong = c.faction === selected, color = colorOf(c.faction);
            if (!c.tiles.length) {
                const cell = world.cells.find(x => x.id === c.area);
                if (surface.kind === 'world' && cell) out.rects.push({x: cell.x, y: cell.y, w: cell.width, h: cell.height, color, strong});
                else if (surface.cell?.id === c.area) out.rects.push({x: 0, y: 0, w: surface.width, h: surface.height, color, strong});
                continue;
            }
            for (const [x, y] of c.tiles) {
                const at = surface.fromPlace({cell: c.area, x, y});
                if (at) out.tiles.push({x: at[0], y: at[1], color, strong});
            }
        }
        return out;
    }, [surface, world, data, selected, painting]); // eslint-disable-line react-hooks/exhaustive-deps

    const finish = async (run: () => Promise<unknown>, done: string) => {
        setBusy(true); setProblem('');
        try { await run(); setIsNew(false); await load(); setProblem(done); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };
    const live = target === 'prod';
    const sure = (what: string) => !live || window.confirm(`${what} on PROD?`);
    const choose = (id: string) => { if (painting && !window.confirm('Discard the unsaved painting?')) return; setPainting(null); setIsNew(false); setSelected(id); setPair(null); };
    const newFaction = () => {
        if (dirty && !window.confirm('Discard the unsaved changes?')) return;
        const used = new Set(data?.factions.map(f => f.color));
        setDraft({id: '', name: 'New faction', color: COLORS.find(c => !used.has(c)) ?? COLORS[0], kind: 'npc', description: ''});
        setIsNew(true); setSelected(null); setPainting(null); setPair(null);
    };
    const save = () => {
        if (!draft || !sure(`Save ${draft.name}`)) return;
        const faction = isNew ? {...draft, id: idFrom(draft.name, data?.factions.map(f => f.id) ?? [], 'faction')} : draft;
        return finish(async () => { await dmApi.saveFaction(target, faction); setSelected(faction.id); }, `Saved ${faction.name}.`);
    };
    const remove = () => draft && window.confirm(`Delete ${draft.name}${live ? ' from PROD' : ''}? Its claims, members and relations go with it.`) &&
        finish(() => dmApi.deleteFaction(target, draft.id).then(() => setSelected(null)), `Deleted ${draft.name}.`);
    const claimWhole = () => selected && place && sure(`${nameOf(selected)} claims all of ${place.name}`) &&
        finish(() => dmApi.claim(target, selected, place.id, []), `${nameOf(selected)} claims all of ${place.name}.`);
    const saveTiles = () => painting && sure(`Save ${nameOf(painting.faction)}'s claim on ${placeName(painting.area)}`) &&
        finish(async () => { await dmApi.claim(target, painting.faction, painting.area, painting.tiles); setPainting(null); }, 'Saved the claim.');
    const unclaim = (area: string) => selected && sure(`${nameOf(selected)} gives up ${placeName(area)}`) &&
        finish(() => dmApi.unclaim(target, selected, area), `${nameOf(selected)} gave up ${placeName(area)}.`);

    const stroke = useRef<Brush>('paint');
    const onPaint = (at: Place | null, start: boolean) => {
        if (!at) return;
        setPainting(c => {
            if (!c || at.cell !== c.area) return c;
            const has = c.tiles.some(([x, y]) => x === at.x && y === at.y);
            if (start) stroke.current = brush;
            if (stroke.current === 'paint') return has || c.tiles.length >= 4096 ? c : {...c, tiles: [...c.tiles, [at.x, at.y]]};
            return has ? {...c, tiles: c.tiles.filter(([x, y]) => x !== at.x || y !== at.y)} : c;
        });
    };

    const q = query.trim().toLowerCase();
    const match = (...t: string[]) => !q || t.some(x => x.toLowerCase().includes(q));
    const places = world ? [...world.cells.map(c => ({id: c.id, name: c.name, room: false})), ...world.rooms.map(r => ({id: r.id, name: r.name, room: true}))] : [];
    const actions = (data?.actions ?? []).filter(a => a.target === selected);
    const mode = painting && place && painting.area === place.id ? 'paint' : 'select';

    return <div className="dm-npcs">
        <nav className="explorer" aria-label="Places and factions">
            <div className="explorer-search"><input value={query} onChange={e => setQuery(e.target.value)} placeholder="Search…" /></div>
            <div className="explorer-scroll">
                <p className="dm-group">Factions {canAct && <button className="icon" title="New faction" onClick={newFaction}>+</button>}</p>
                {FACTION_KINDS.map(k => {
                    const list = (data?.factions ?? []).filter(f => f.kind === k.id && match(f.name, f.id));
                    return list.length ? <div key={k.id}><p className="dm-subgroup">{k.label}</p>
                        {list.map(f => <button key={f.id} className={selected === f.id ? 'tree-item active' : 'tree-item'} onClick={() => choose(f.id)}>
                            <span className="tree-icon" style={{color: f.color}}>{k.icon}</span><span className="tree-label">{f.name}</span>
                            <small>{data?.claims.filter(c => c.faction === f.id).length} places</small></button>)}</div> : null;
                })}
                {data && !data.factions.length && <p className="tree-empty">No factions yet.</p>}
                <p className="dm-group">Places</p>
                <button className={view.kind === 'world' ? 'tree-item active' : 'tree-item'} onClick={() => setView({kind: 'world'})}>
                    <span className="tree-icon">◇</span><span className="tree-label">World overview</span></button>
                {places.filter(p => match(p.name, p.id)).map(p => <button key={p.id} className={view.kind === 'cell' && view.id === p.id ? 'tree-item active' : 'tree-item'}
                    onClick={() => setView({kind: 'cell', id: p.id})}><span className="tree-icon">{p.room ? '▣' : '▦'}</span><span className="tree-label">{p.name}</span>
                    <small>{(data?.claims ?? []).filter(c => c.area === p.id).map(c => <i key={c.faction} className="dm-dot" style={{background: colorOf(c.faction)}} />)}</small></button>)}
            </div>
        </nav>
        <main className="stage">
            <div className="tool-options">
                <div className="crumbs"><div className="segmented dm-layers" role="tablist" aria-label="View">
                    <button role="tab" aria-selected={sub === 'territory'} className={sub === 'territory' ? 'on' : ''} onClick={() => setSub('territory')}>▦ Territory</button>
                    <button role="tab" aria-selected={sub === 'relations'} className={sub === 'relations' ? 'on' : ''} onClick={() => setSub('relations')}>⇄ Relations</button></div>
                    {sub === 'territory' && <span className="crumb-label">{surface?.title ?? 'Loading…'}</span>}</div>
                <div className="options">
                    {sub === 'relations' ? <span className="tool-help">Each row is how that faction regards the others. Click a square to change it.</span>
                        : painting ? <><b className="tool-name">Painting {nameOf(painting.faction)}'s claim</b>
                            <div className="segmented">{(['paint', 'erase'] as const).map(b => <button key={b} className={brush === b ? 'on' : ''} onClick={() => setBrush(b)}>{b === 'paint' ? '▦ Paint' : '▢ Erase'}</button>)}</div>
                            <span className="tool-help">{painting.tiles.length} tiles · drag to {brush} · right-drag pans</span>
                            <button className={live ? 'publish' : 'primary'} disabled={busy || !painting.tiles.length} onClick={saveTiles}>Save claim</button>
                            <button onClick={() => setPainting(null)}>Cancel</button></>
                        : <><span className="tool-help">{selected ? `${nameOf(selected)}'s claims are drawn strongest. Open a place to claim or paint it.` : 'Select a faction to see and change its territory. Claims may overlap; a claim is not control.'}</span>
                            {canAct && <button className="primary" onClick={newFaction}>+ New faction</button>}</>}
                </div>
            </div>
            {sub === 'relations' ? <RelationsMatrix data={data} pair={pair} onPick={(a, b) => { setPair([a, b]); setSelected(a); setIsNew(false); }} />
                : surface && world ? <MapView surface={surface} world={world} markers={[]} overlay={overlay} selected={null} mode={canAct ? mode : 'select'}
                    onClick={() => undefined} onPaint={onPaint} onOpen={id => setView({kind: 'cell', id})} />
                : <div className="dm-center"><p className="hint">{problem || 'Loading the world…'}</p></div>}
        </main>
        <aside className="inspector" aria-label="Inspector">
            {problem && <p className="hint dm-note">{problem}</p>}
            {pair && data && sub === 'relations' ? <RelationEditor key={pair.join('>')} data={data} pair={pair} target={target} canAct={canAct} busy={busy}
                onSave={(both, d, s, r) => finish(async () => {
                    await dmApi.relate(target, pair[0], pair[1], d, s, r);
                    if (both) await dmApi.relate(target, pair[1], pair[0], d, s, r);
                }, 'Saved the relation.')} onClose={() => setPair(null)} />
            : draft ? <div className="dm-panel">
                <header><div><small>{isNew ? 'NEW FACTION · NOT SAVED' : `${FACTION_KINDS.find(k => k.id === draft.kind)?.label.toUpperCase()}`}</small><h2>{draft.name}</h2></div>
                    <span className="role-chip" style={{borderColor: draft.color, color: draft.color}}>{FACTION_KINDS.find(k => k.id === draft.kind)?.icon}</span></header>
                <fieldset disabled={!canAct} className="dm-fields">
                    <TextField label="Name" value={draft.name} max={120} onCommit={v => setDraft({...draft, name: v})} />
                    <Row><SelectField label="Kind" value={draft.kind} options={FACTION_KINDS.map(k => ({value: k.id, label: k.label}))} onChange={v => setDraft({...draft, kind: v as FactionKind})} />
                        <label className="field"><span>Color</span><input type="color" value={draft.color} onChange={e => setDraft({...draft, color: e.target.value})} /></label></Row>
                    <TextField label="Description" multiline value={draft.description} max={4000} onCommit={v => setDraft({...draft, description: v})} />
                </fieldset>
                {canAct && <div className="button-grid">
                    <button className={live ? 'publish' : 'primary'} disabled={busy || !dirty} onClick={save}>{busy ? 'Saving…' : `Save to ${target.toUpperCase()}`}</button>
                    <button disabled={busy || !dirty} onClick={() => { if (isNew) { setIsNew(false); setDraft(null); } else setDraft(saved ? {...saved} : null); }}>{isNew ? 'Discard' : 'Revert'}</button>
                    {!isNew && <button className="danger" disabled={busy} onClick={remove}>Delete</button>}</div>}
                {!isNew && data && world && <>
                    <div className="field"><span>Territory</span>
                        {place && canAct && <div className="dm-claim-here">
                            <div><b>{place.name}</b>: {claimHere ? (claimHere.tiles.length ? `${claimHere.tiles.length} tiles claimed` : 'all of it claimed') : 'not claimed'}</div>
                            <div className="button-grid">
                                <button disabled={busy || (!!claimHere && !claimHere.tiles.length)} onClick={claimWhole}>Claim all of it</button>
                                <button disabled={busy} onClick={() => { setPainting({faction: draft.id, area: place.id, tiles: claimHere?.tiles.length ? claimHere.tiles.map(t => [...t] as [number, number]) : []}); setBrush('paint'); setSub('territory'); }}>Paint tiles</button>
                                {claimHere && <button className="danger" disabled={busy} onClick={() => unclaim(place.id)}>Give up</button>}</div></div>}
                        {!place && <p className="hint">Open a place to claim it or paint its tiles.</p>}
                        <ul className="dm-list">{data.claims.filter(c => c.faction === draft.id).map(c => <li key={c.area}>
                            <button className="link-button" onClick={() => { setView({kind: 'cell', id: c.area}); setSub('territory'); }}>{placeName(c.area)}</button>
                            <small>{c.tiles.length ? `${c.tiles.length} tiles` : 'whole place'}</small></li>)}</ul>
                        {!data.claims.some(c => c.faction === draft.id) && <p className="hint">Claims nothing yet.</p>}</div>
                    <Members data={data} faction={draft.id} canAct={canAct} busy={busy}
                        onSet={(npc, rank) => finish(() => dmApi.member(target, draft.id, npc, rank), rank === null ? 'Removed.' : 'Saved the member.')} />
                    <div className="field"><span>Relations</span>
                        <ul className="dm-list">{data.factions.filter(f => f.id !== draft.id).map(f => {
                            const out = data.relations.find(r => r.faction === draft.id && r.other === f.id), back = data.relations.find(r => r.faction === f.id && r.other === draft.id);
                            return <li key={f.id}><button className="link-button" onClick={() => { setSub('relations'); setPair([draft.id, f.id]); }}>{f.name}</button>
                                <small>{out ? <StanceChip stance={out.stance} disposition={out.disposition} /> : 'neutral'} · they: {back ? <StanceChip stance={back.stance} disposition={back.disposition} /> : 'neutral'}</small></li>;
                        })}</ul></div>
                </>}
                <ActionList actions={actions} target={target} />
            </div>
            : <div className="dm-panel"><header><div><small>FACTIONS</small><h2>{live ? 'PROD: the players’ world' : 'DEV: the rehearsal world'}</h2></div></header>
                <Hint>{data ? `${data.factions.length} factions claiming ${new Set(data.claims.map(c => c.area)).size} places; ${data.relations.length} relations set.` : 'Loading…'}</Hint>
                <Hint>Select a faction, or make one. Factions and their claims reach a running game server at once, and Atlas shows them; members and relations are kept for the Dungeon Master and stories.</Hint></div>}
        </aside>
    </div>;
}

export function StanceChip({stance, disposition}: {stance: Stance; disposition: number}) {
    const s = stanceOf(stance);
    return <span className="dm-stance" style={{color: s.color, borderColor: s.color}}>{s.label} {disposition > 0 ? '+' : ''}{disposition}</span>;
}

function Members({data, faction, canAct, busy, onSet}: {data: Factions; faction: string; canAct: boolean; busy: boolean; onSet: (npc: string, rank: string | null) => void}) {
    const [npc, setNpc] = useState('');
    const members = data.members.filter(m => m.faction === faction);
    const name = (id: string) => data.people.find(p => p.id === id)?.name ?? id;
    return <div className="field"><span>Members</span>
        <ul className="dm-list">{members.map(m => <li key={m.npc}><span>{name(m.npc)}</span>
            {canAct ? <input className="dm-rank" defaultValue={m.rank} placeholder="rank" maxLength={60} disabled={busy}
                onBlur={e => e.target.value.trim() !== m.rank && onSet(m.npc, e.target.value.trim())} /> : <small>{m.rank}</small>}
            {canAct && <button className="icon" title="Remove from the faction" disabled={busy} onClick={() => onSet(m.npc, null)}>✕</button>}</li>)}</ul>
        {!members.length && <p className="hint">No named NPCs belong to it yet.</p>}
        {canAct && <div className="dm-add-row"><select aria-label="Add a member" value={npc} onChange={e => setNpc(e.target.value)}>
            <option value="">Add a named NPC…</option>
            {data.people.filter(p => !members.some(m => m.npc === p.id)).map(p => <option key={p.id} value={p.id}>{p.name} ({p.role})</option>)}</select>
            <button disabled={!npc || busy} onClick={() => { onSet(npc, ''); setNpc(''); }}>Add</button></div>}
    </div>;
}

function RelationsMatrix({data, pair, onPick}: {data: Factions | null; pair: [string, string] | null; onPick: (a: string, b: string) => void}) {
    if (!data) return <div className="dm-center"><p className="hint">Loading…</p></div>;
    if (data.factions.length < 2) return <div className="dm-center"><p className="hint">Relations need at least two factions.</p></div>;
    return <div className="dm-matrix-wrap"><table className="dm-matrix">
        <thead><tr><th>regards →</th>{data.factions.map(f => <th key={f.id} title={f.name}><span style={{color: f.color}}>■</span> {f.name}</th>)}</tr></thead>
        <tbody>{data.factions.map(a => <tr key={a.id}><th><span style={{color: a.color}}>■</span> {a.name}</th>
            {data.factions.map(b => {
                if (a.id === b.id) return <td key={b.id} className="self" />;
                const r = data.relations.find(x => x.faction === a.id && x.other === b.id), s = stanceOf(r?.stance ?? 'neutral');
                const on = pair?.[0] === a.id && pair?.[1] === b.id;
                return <td key={b.id} className={on ? 'on' : ''}><button aria-label={`How ${a.name} regards ${b.name}`} title={r?.reason || 'Not set: neutral'}
                    style={{borderColor: s.color, color: s.color, opacity: r ? 1 : .55}} onClick={() => onPick(a.id, b.id)}>
                    {s.label}<small>{r ? (r.disposition > 0 ? '+' : '') + r.disposition : '—'}</small></button></td>;
            })}</tr>)}</tbody></table></div>;
}

function RelationEditor({data, pair, target, canAct, busy, onSave, onClose}: {data: Factions; pair: [string, string]; target: Target; canAct: boolean; busy: boolean;
    onSave: (both: boolean, disposition: number, stance: Stance, reason: string) => void; onClose: () => void}) {
    const [a, b] = pair;
    const current = data.relations.find(r => r.faction === a && r.other === b);
    const [disposition, setDisposition] = useState(current?.disposition ?? 0);
    const [stance, setStance] = useState<Stance>(current?.stance ?? 'neutral');
    const [chosen, setChosen] = useState(!!current);
    const [reason, setReason] = useState(current?.reason ?? '');
    const [both, setBoth] = useState(false);
    const [history, setHistory] = useState<RelationChange[]>([]);
    useEffect(() => { dmApi.relationHistory(target, a, b).then(setHistory).catch(() => setHistory([])); }, [target, a, b, current?.at]);
    const name = (id: string) => data.factions.find(f => f.id === id)?.name ?? id;
    const changed = !current || current.disposition !== disposition || current.stance !== stance || current.reason !== reason;
    return <div className="dm-panel">
        <header><div><small>RELATION</small><h2>{name(a)} → {name(b)}</h2></div><button className="icon" title="Close" onClick={onClose}>✕</button></header>
        <Hint>How {name(a)} regards {name(b)}. {current ? `Set by ${current.by} ${new Date(current.at).toLocaleString()}.` : 'Not set yet: neutral.'}</Hint>
        <fieldset disabled={!canAct} className="dm-fields">
            <Slider label="Disposition" value={disposition} min={-100} max={100} step={1} format={v => (v > 0 ? '+' : '') + v}
                onCommit={v => { setDisposition(v); if (!chosen) setStance(suggested(v)); }} />
            <SelectField label="Stance" value={stance} hint="Follows the disposition until you choose one." options={STANCES.map(s => ({value: s.id, label: s.label}))}
                onChange={v => { setStance(v as Stance); setChosen(true); }} />
            <label className="field"><span>Reason</span><textarea value={reason} maxLength={1000} rows={3} onChange={e => setReason(e.target.value)}
                placeholder="Why they feel this way (kept in the history)" /></label>
            <label className="toggle-row"><input type="checkbox" checked={both} onChange={e => setBoth(e.target.checked)} /> Also set how {name(b)} regards {name(a)}</label>
        </fieldset>
        {canAct && <div className="button-grid"><button className={target === 'prod' ? 'publish' : 'primary'} disabled={busy || (!changed && !both)}
            onClick={() => (target !== 'prod' || window.confirm('Save this relation on PROD?')) && onSave(both, disposition, stance, reason.trim())}>Save to {target.toUpperCase()}</button></div>}
        {history.length > 0 && <div className="field"><span>History</span><ol className="dm-actions">{history.map((h, i) => <li key={i}>
            <b><StanceChip stance={h.stance as Stance} disposition={h.disposition} /></b> by {h.by} · {new Date(h.at).toLocaleString()}
            {h.reason && <span>{h.reason}</span>}</li>)}</ol></div>}
    </div>;
}

function ActionList({actions, target}: {actions: Action[]; target: Target}) {
    if (!actions.length) return null;
    return <ol className="dm-actions">{actions.slice(0, 6).map(a => <li key={a.id} className={a.status}>
        <b>sync</b> by {a.by} · {new Date(a.at).toLocaleTimeString()}
        <span>{a.status === 'queued' ? `waiting for a ${target.toUpperCase()} game server…` : `${a.status}: ${a.result}`}</span></li>)}</ol>;
}
