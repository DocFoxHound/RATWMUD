// Portraits players upload, for the Dungeon Master (Docs/Design/29-client-polish.md, phase 9): each new or reported one
// with its image, approved or rejected with a reason. The game server applies the decision and tells the owner.
import {useCallback, useEffect, useState} from 'react';
import {dmApi, type Me, type Portrait, type Portraits, type Target} from './api';

export function ArtworkPanel({me, target}: {me: Me; target: Target}) {
    const [data, setData] = useState<Portraits | null>(null);
    const [problem, setProblem] = useState(''), [busy, setBusy] = useState('');
    const [reasons, setReasons] = useState<Record<string, string>>({});
    const load = useCallback(() => dmApi.artwork(target).then(d => { setData(d); setProblem(''); }).catch(e => setProblem((e as Error).message)), [target]);
    useEffect(() => { void load(); }, [load]);
    // While a decision is queued, keep checking until the game server has applied it.
    const waiting = data?.actions.some(a => a.status === 'queued');
    useEffect(() => { if (!waiting) return; const t = setTimeout(load, 1500); return () => clearTimeout(t); }, [waiting, data, load]);
    const decide = async (p: Portrait, decision: 'approve' | 'reject') => {
        const reason = (reasons[p.id] ?? '').trim();
        if (decision === 'reject' && !reason && !window.confirm(`Reject ${p.name}'s portrait without a reason?`)) return;
        if (target === 'prod' && !window.confirm(`${decision === 'approve' ? 'Approve' : 'Reject'} ${p.name}'s portrait in the LIVE world?`)) return;
        setBusy(p.id); setProblem('');
        try { await dmApi.reviewArtwork(target, p.id, decision, reason); await load(); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(''); }
    };
    const queued = new Set(data?.actions.filter(a => a.status === 'queued').map(a => a.target) ?? []);
    const canAct = me.role !== 'viewer';
    if (data && !data.ready) return <section className="dm-panel dm-artwork"><header><div><small>PORTRAITS</small><h2>Uploaded portraits</h2></div></header>
        <p className="hint">The {target.toUpperCase()} database has no portrait table yet (migration 0027).</p></section>;
    return <section className="dm-panel dm-artwork" aria-label="Uploaded portraits">
        <header><div><small>PORTRAITS</small><h2>Waiting for review{data ? ` (${data.pending.length})` : ''}</h2></div>
            <button title="Read again" onClick={() => void load()}>↻</button></header>
        {problem && <p className="hint error-text">{problem}</p>}
        {data && !data.pending.length && <p className="hint">Nothing waiting. A portrait shows here when a player uploads one, or reports one.</p>}
        <ul className="dm-portraits">{data?.pending.map(p => <li key={p.id}>
            {p.png && <img src={`data:image/png;base64,${p.png}`} width={128} height={128} alt={`${p.name}'s portrait`} />}
            <div>
                <b>{p.name}</b>{p.reported && <span className="dm-status dead">reported</span>}
                <p className="meta">{p.account} · {new Date(p.at).toLocaleString()}</p>
                {p.reason && <p className="meta">{p.reason}</p>}
                {canAct ? <>
                    <input value={reasons[p.id] ?? ''} maxLength={400} placeholder="Reason (the owner sees it)"
                        onChange={e => setReasons(r => ({...r, [p.id]: e.target.value}))} />
                    <div className="button-grid">
                        <button disabled={!!busy || queued.has(p.id)} onClick={() => void decide(p, 'approve')}>Approve</button>
                        <button className="danger" disabled={!!busy || queued.has(p.id)} onClick={() => void decide(p, 'reject')}>Reject</button>
                    </div>
                    {queued.has(p.id) && <p className="hint">Waiting for the {target.toUpperCase()} game server…</p>}
                </> : null}
            </div></li>)}</ul>
        {data && data.recent.length > 0 && <details><summary>Recent decisions</summary>
            <ol className="dm-actions">{data.recent.map(p => <li key={p.id} className={p.status === 'approved' ? 'applied' : 'refused'}>
                <b>{p.name}</b> {p.status} · {new Date(p.at).toLocaleString()}{p.reason && <span>{p.reason}</span>}</li>)}</ol></details>}
        <p className="hint">Players see their own portrait at once; others see it only once approved. Decisions reach the game within a second while a {target.toUpperCase()} server runs.</p>
    </section>;
}
