// Players' reports of other wolves, for the Dungeon Master (Docs/Design/50-player-card-friends-safety.md, Phase 2):
// open ones first, each with the lines the reporter received as evidence, the reported account's earlier reports and
// how many accounts block it (a count, never who); upheld with a note, a warning or a silence, or dismissed. The game
// server applies the decision and tells the player.
import {useCallback, useEffect, useState} from 'react';
import {dmApi, type Me, type PlayerReport, type Reports, type Target} from './api';

const Categories: Record<string, string> = {harassment: 'Harassment', hateful: 'Hateful content', spam: 'Spam', cheating: 'Cheating', other: 'Other'};

export function ReportsPanel({me, target}: {me: Me; target: Target}) {
    const [data, setData] = useState<Reports | null>(null);
    const [problem, setProblem] = useState(''), [busy, setBusy] = useState('');
    const [choice, setChoice] = useState<Record<string, {outcome: 'note' | 'warning' | 'silence'; hours: number; reason: string}>>({});
    const load = useCallback(() => dmApi.reports(target).then(d => { setData(d); setProblem(''); }).catch(e => setProblem((e as Error).message)), [target]);
    useEffect(() => { void load(); }, [load]);
    const waiting = data?.actions.some(a => a.status === 'queued');
    useEffect(() => { if (!waiting) return; const t = setTimeout(load, 1500); return () => clearTimeout(t); }, [waiting, data, load]);
    const pick = (id: string) => choice[id] ?? {outcome: 'warning' as const, hours: 1, reason: ''};
    const decide = async (r: PlayerReport, decision: 'uphold' | 'dismiss') => {
        const c = pick(r.id);
        const words = decision === 'dismiss' ? 'Dismiss' : `Uphold (${c.outcome}${c.outcome === 'silence' ? `, ${c.hours} h` : ''})`;
        if (target === 'prod' && !window.confirm(`${words} the report against ${r.reportedName} on PROD?`)) return;
        setBusy(r.id); setProblem('');
        try { await dmApi.decideReport(target, r.id, decision, c.outcome, c.hours, c.reason); await load(); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(''); }
    };
    const queued = new Set(data?.actions.filter(a => a.status === 'queued').map(a => String((a.payload as {report?: string})?.report ?? '')) ?? []);
    const canAct = me.role !== 'viewer';
    if (data && !data.ready) return <section className="dm-panel dm-reports"><header><div><small>REPORTS</small><h2>Players' reports</h2></div></header>
        <p className="hint">The {target.toUpperCase()} database has no reports table yet (migration 0036).</p></section>;
    const open = data?.reports.filter(r => r.status === 'open') ?? [], decided = data?.reports.filter(r => r.status !== 'open') ?? [];
    return <section className="dm-panel dm-reports" aria-label="Players' reports">
        <header><div><small>REPORTS</small><h2>Open reports{data ? ` (${open.length})` : ''}</h2></div>
            <button title="Read again" onClick={() => void load()}>↻</button></header>
        {problem && <p className="hint error-text">{problem}</p>}
        {data && !open.length && <p className="hint">Nothing open. A report shows here when a player reports a wolf.</p>}
        <ul className="dm-reports-list">{open.map(r => {
            const c = pick(r.id);
            const set = (patch: Partial<typeof c>) => setChoice(all => ({...all, [r.id]: {...c, ...patch}}));
            return <li key={r.id}>
                <b>{r.reportedName}</b> <span className="meta">({r.reported})</span> · {Categories[r.category] ?? r.category} · {r.kind}
                <p className="meta">Reported by {r.reporterName} ({r.reporter}) · {new Date(r.at).toLocaleString()}
                    {r.blockedBy > 0 ? ` · blocked by ${r.blockedBy} account${r.blockedBy > 1 ? 's' : ''}` : ''}
                    {r.earlier.length ? ` · ${r.earlier.length} earlier report${r.earlier.length > 1 ? 's' : ''} (${r.earlier.filter(e => e.status === 'upheld').length} upheld)` : ''}</p>
                {r.note && <p className="meta">“{r.note}”</p>}
                <ol className="dm-evidence">{r.evidence.map((l, i) => <li key={i}><span className="meta">{l.channel}</span> {l.text}</li>)}</ol>
                {!r.evidence.length && <p className="hint">No lines kept with it.</p>}
                {canAct ? <>
                    <div className="button-grid">
                        <select value={c.outcome} onChange={e => set({outcome: e.target.value as typeof c.outcome})}>
                            <option value="note">Uphold: a note only</option>
                            <option value="warning">Uphold: a warning</option>
                            <option value="silence">Uphold: a silence</option>
                        </select>
                        {c.outcome === 'silence' && <select value={c.hours} onChange={e => set({hours: Number(e.target.value)})}>
                            {[1, 6, 24, 72].map(h => <option key={h} value={h}>{h} hour{h > 1 ? 's' : ''}</option>)}</select>}
                    </div>
                    <input value={c.reason} maxLength={400} placeholder="Reason (for the audit log)" onChange={e => set({reason: e.target.value})} />
                    <div className="button-grid">
                        <button disabled={!!busy || queued.has(r.id)} onClick={() => void decide(r, 'uphold')}>Uphold</button>
                        <button disabled={!!busy || queued.has(r.id)} onClick={() => void decide(r, 'dismiss')}>Dismiss</button>
                    </div>
                    {queued.has(r.id) && <p className="hint">Waiting for the {target.toUpperCase()} game server…</p>}
                </> : null}
            </li>;
        })}</ul>
        {decided.length > 0 && <details><summary>Decided ({decided.length})</summary>
            <ol className="dm-actions">{decided.map(r => <li key={r.id} className={r.status === 'upheld' ? 'applied' : 'refused'}>
                <b>{r.reportedName}</b> {r.status}{r.outcome ? ` · ${r.outcome}${r.outcome === 'silence' ? ` ${r.silenceHours} h` : ''}` : ''}
                {' '}· {r.decidedBy} · {r.decidedAt ? new Date(r.decidedAt).toLocaleString() : ''}</li>)}</ol></details>}
        <p className="hint">Open and dismissed reports are kept 30 days; upheld ones for good, their lines 180 days. The reported player is never told who reported them.</p>
    </section>;
}
