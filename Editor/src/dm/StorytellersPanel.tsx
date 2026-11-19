// Storytellers, for the Dungeon Master (Docs/Design/58-player-storytellers.md, 11): applications with their notes
// (approve or refuse, with a reason), the approved (revoke), each storyteller's tales with who did what, their kept words
// (narration and story characters' lines, 30 days), the approved visitors, and the DM's own hand: give a character a
// storyline, credit a world story's milestone (its lines built from the ledger by the DM host).
import {useCallback, useEffect, useRef, useState} from 'react';
import {awaitOutcome, type ActOutcome} from '../lib/acts';
import type {Project} from '../model/model.mjs';
import {StoryPicker} from './StoryPicker';
import {type Npcs, type Players, isLive, worldName, dmApi, type Me, type Storytellers, type Target, worldLabel} from './api';

export function StorytellersPanel({me, target, world, players, refresh, openAssignment}: {openAssignment: boolean; me: Me; target: Target; world: Project | null; players: Players | null; refresh: () => Promise<void>}) {
    const [data, setData] = useState<Storytellers | null>(null);
    const [problem, setProblem] = useState(''), [note, setNote] = useState(''), [busy, setBusy] = useState(false);
    const [reason, setReason] = useState('');
    const [visitor, setVisitor] = useState({id: '', name: '', description: '', like: '', enabled: true});
    const [give, setGive] = useState({character: '', template: '', resident: '', place: ''});
    const [showGive, setShowGive] = useState(openAssignment);
    const heading = useRef<HTMLElement>(null);
    useEffect(() => { if (openAssignment) heading.current?.focus(); }, [openAssignment]);
    const [roster, setRoster] = useState<Npcs | null>(null);
    const [sent, setSent] = useState<(ActOutcome & {id: number}) | null>(null);
    const observed = players?.actions.find(a => a.id === sent?.id);
    const receipt = sent?.status === 'queued' ? observed ?? sent : sent;
    useEffect(() => {
        if (!showGive) return;
        let active = true;
        dmApi.npcs(target).then(n => { if (active) setRoster(n); }).catch(e => { if (active) setProblem(e.message); });
        return () => { active = false; };
    }, [showGive, target]);
    const [credit, setCredit] = useState({story: '', milestone: '', weight: 'great', kind: 'storyline', storyline: '', cell: '', from: 0, to: 0});
    const load = useCallback(() => dmApi.storytellers(target).then(d => { setData(d); setProblem(''); }).catch(e => setProblem((e as Error).message)), [target]);
    useEffect(() => { void load(); }, [load]);
    const canAct = me.role !== 'viewer';
    const run = async (what: string, job: () => Promise<unknown>) => {
        if (isLive(target) && !window.confirm(`${what} on ${worldName(target)}?`)) return;
        setBusy(true); setProblem('');
        try { await job(); setNote(`${what}: sent; the game applies it within seconds.`); setTimeout(() => void load(), 1500); }
        catch (e) { setProblem((e as Error).message); }
        finally { setBusy(false); }
    };
    const act = (kind: string, id: string, payload?: Record<string, unknown>) => run(`${kind} ${id}`, () => dmApi.liveAction(target, kind, id, payload));
    if (data && !data.ready) return <section className="dm-panel"><header><div><small>STORYTELLERS</small><h2>Storytellers</h2></div></header>
        <p className="hint">The {worldLabel(target)} database has no storytellers table yet (migration 0049).</p></section>;
    const applied = data?.standing.filter(s => s.state === 'applied') ?? [], approved = data?.standing.filter(s => s.state === 'approved') ?? [];
    return <section className="dm-panel" aria-label="Storytellers" ref={heading} tabIndex={-1}>
        <header><div><small>STORYTELLERS</small><h2>Applications{data ? ` (${applied.length})` : ''}</h2></div>
            <button title="Read again" onClick={() => void load()}>↻</button></header>
        {problem && <p className="hint error-text">{problem}</p>}
        {note && <p className="hint">{note}</p>}
        {canAct && <input value={reason} maxLength={400} placeholder="Reason (told to the player, and audited)" onChange={e => setReason(e.target.value)} />}
        {data && !applied.length && <p className="hint">No applications. Wolves of social level 5 apply from their character sheet.</p>}
        <ul className="dm-reports-list">{applied.map(s => <li key={s.account}>
            <b>{s.name || s.character}</b> <span className="meta">({s.account})</span>
            {s.note && <p className="meta">“{s.note}”</p>}
            {canAct && <div className="button-grid">
                <button disabled={busy} onClick={() => void act('storyteller.decide', s.account, {approve: true, reason})}>Approve</button>
                <button disabled={busy} onClick={() => void act('storyteller.decide', s.account, {approve: false, reason})}>Refuse</button></div>}
        </li>)}</ul>
        <p className="dm-group">Storytellers ({approved.length})</p>
        <ul className="dm-reports-list">{approved.map(s => <li key={s.account}><b>{s.name || s.character}</b> <span className="meta">({s.account}) · approved by {s.decidedBy}</span>
            {canAct && <button disabled={busy} onClick={() => void act('storyteller.revoke', s.account, {reason})}>Revoke</button>}</li>)}</ul>
        <p className="dm-group">Tales ({data?.tales.length ?? 0})</p>
        <ul className="dm-reports-list">{(data?.tales ?? []).map(t => <li key={t.id}>
            <b>{t.title}</b> <span className="meta">· {t.state} · by {t.authorName} · {t.participants.filter(p => !p.left).map(p => p.name).join(', ') || 'no one yet'}</span>
            {t.premise && <p className="meta">{t.premise}</p>}
            <ol className="dm-evidence">{t.steps.map((s, i) => <li key={i}>{s.title}: {s.objectives.map(o => `${o.line || o.kind}${o.by ? ` (${o.by}${o.byHand ? ', by hand' : ''})` : ''}`).join('; ')}</li>)}</ol>
            {canAct && <div className="button-grid">
                {t.state === 'running' && <button disabled={busy} onClick={() => void act('tale.pause', t.id)}>Pause</button>}
                {t.state === 'paused' && <button disabled={busy} onClick={() => void act('tale.resume', t.id)}>Resume</button>}
                {(t.state === 'running' || t.state === 'paused') && <button disabled={busy} onClick={() => void act('tale.stop', t.id)}>Stop</button>}</div>}
            <details><summary>Their words</summary><ol className="dm-evidence">{(data?.log ?? []).filter(e => e.storyline === t.id).slice(0, 60).map(e =>
                <li key={e.id}><span className="meta">{new Date(e.at * 1000).toLocaleString()} · {e.kind}{e.target ? ` · ${e.target}` : ''}</span> {e.text || e.detail}</li>)}</ol></details>
        </li>)}</ul>
        <p className="dm-group">Story visitors</p>
        <ul className="dm-reports-list">{(data?.visitors ?? []).map(v => <li key={v.id}><b>{v.name}</b> <span className="meta">({v.id}){v.enabled ? '' : ' · off'} · {v.description}</span></li>)}</ul>
        {canAct && <div className="button-grid">
            <input placeholder="id" value={visitor.id} onChange={e => setVisitor({...visitor, id: e.target.value})} />
            <input placeholder="Name" value={visitor.name} onChange={e => setVisitor({...visitor, name: e.target.value})} />
            <input placeholder="Looks like (a resident's id)" value={visitor.like} onChange={e => setVisitor({...visitor, like: e.target.value})} />
            <input placeholder="Description" value={visitor.description} onChange={e => setVisitor({...visitor, description: e.target.value})} />
            <button disabled={busy || !visitor.id || !visitor.name} onClick={() => void run(`Save visitor ${visitor.id}`, () => dmApi.saveStoryVisitor(target, visitor))}>Save visitor</button></div>}
        {canAct && <details open={showGive} onToggle={e => setShowGive(e.currentTarget.open)}><summary>Give a personal storyline</summary>
            <label>Player character<select aria-label="Player character" value={give.character} onChange={e => setGive({...give, character:e.target.value})}>
                <option value="">Choose a player</option>{players?.characters.map(c => <option key={c.id} value={c.id}>{c.name} · {c.id}</option>)}</select></label>
            <label>Story template<select aria-label="Story template" value={give.template} onChange={e => setGive({...give, template:e.target.value})}>
                <option value="">Choose a template</option>{data?.templates.filter(t => t.source === 'dm').map(t => <option key={t.id} value={t.id}>{t.title}</option>)}</select></label>
            <StoryPicker world={world} roster={roster} resident={give.resident} place={give.place}
                onResident={resident => setGive({...give,resident})} onPlace={place => setGive({...give,place})} />
            <p className="hint">An empty location uses the player’s current cell.</p>
            <button disabled={busy || receipt?.status === 'queued' || !give.character || !give.template ||
                (data?.templates.find(t => t.id === give.template)?.residentRequired && !give.resident) ||
                (!!give.resident && (!roster || roster.dead.includes(give.resident) ||
                    ![...roster.people, ...roster.holders].find(p => p.id === give.resident)?.home))}
                onClick={async () => {
                    if (isLive(target) && !window.confirm('Give this personal storyline on ' + worldName(target) + '?')) return;
                    setBusy(true); setProblem('');
                    try {
                        const sent = await dmApi.liveAction(target, 'storyline.give', give.character, {template:give.template,
                            cast:Object.fromEntries([['resident',give.resident],['place',give.place]].filter(([,v]) => v))});
                        setSent({id:sent.id, status:'queued', result:''});
                        const done = await awaitOutcome(() => dmApi.action(target, sent.id));
                        setSent({id:sent.id, ...done});
                        await refresh();
                    } catch (e) { setProblem((e as Error).message); } finally { setBusy(false); }
                }}>Give storyline</button>
            {receipt && <p role="status">Storyline assignment {receipt.status}. {receipt.result} <small>Action {receipt.id}</small></p>}
            {receipt?.status === 'queued' && !busy && <button onClick={() => dmApi.action(target, receipt.id)
                .then(setSent).catch(e => setProblem(e.message))}>Check assignment status</button>}
        </details>}
        {canAct && <><p className="dm-group">Credit a milestone</p><div className="button-grid">
            <input placeholder="The story" value={credit.story} onChange={e => setCredit({...credit, story: e.target.value})} />
            <input placeholder="The milestone" value={credit.milestone} onChange={e => setCredit({...credit, milestone: e.target.value})} />
            <select value={credit.weight} onChange={e => setCredit({...credit, weight: e.target.value})}><option value="great">Great</option><option value="legendary">Legendary</option></select>
            <select value={credit.kind} onChange={e => setCredit({...credit, kind: e.target.value})}><option value="storyline">A storyline's wolves</option><option value="place">Everyone at a place, in a window</option></select>
            {credit.kind === 'storyline' ? <input placeholder="Storyline id" value={credit.storyline} onChange={e => setCredit({...credit, storyline: e.target.value})} />
                : <><input placeholder="Cell id" value={credit.cell} onChange={e => setCredit({...credit, cell: e.target.value})} />
                    <input type="number" placeholder="From day" value={credit.from} onChange={e => setCredit({...credit, from: Number(e.target.value)})} />
                    <input type="number" placeholder="To day" value={credit.to} onChange={e => setCredit({...credit, to: Number(e.target.value)})} /></>}
            <button disabled={busy || !credit.milestone} onClick={() => void run(`Credit ${credit.milestone}`, () => dmApi.creditMilestone(target, credit.story, credit.milestone, credit.weight,
                credit.kind === 'storyline' ? {kind: 'storyline', storyline: credit.storyline} : {kind: 'place', cell: credit.cell, from: credit.from, to: credit.to}))}>Credit</button></div>
            <p className="hint">Each wolf's lines come from the ledger only; each may give up to 3 stars on the screen.</p></>}
    </section>;
}
