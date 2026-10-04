// The world's week and its festivals, for the Dungeon Master (Docs/Design/26-living-npcs.md, Phase 9): today as the
// game last saved it, when the next market, rest day and festival fall, and calling a festival for a town.
import {useCallback, useEffect, useState} from 'react';
import {dmApi, type Me, type Target, type WorldCalendar} from './api';

const days = (n: number) => n === 0 ? 'today' : n === 1 ? 'tomorrow' : `in ${n} days`;

export function CalendarPanel({me, target}: {me: Me; target: Target}) {
    const [cal, setCal] = useState<WorldCalendar | null>(null);
    const [problem, setProblem] = useState(''), [busy, setBusy] = useState(false);
    const [town, setTown] = useState(''), [name, setName] = useState(''), [when, setWhen] = useState(0);
    const load = useCallback(async () => {
        try { const c = await dmApi.calendar(target); setCal(c); setTown(t => t || c.communities[0]?.id || ''); setProblem(''); }
        catch (error) { setProblem((error as Error).message); }
    }, [target]);
    useEffect(() => { void load(); }, [load]);
    const call = async () => {
        if (target === 'prod' && !window.confirm(`Call a festival in ${town} on PROD, ${days(when)}?`)) return;
        setBusy(true); setProblem('');
        try { await dmApi.callFestival(target, town, name.trim(), when); setName(''); await load(); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };
    const t = cal?.today;
    return <section className="dm-life" aria-label="Calendar">
        <header><small>CALENDAR</small><button title="Read again" onClick={() => void load()}>↻</button></header>
        {problem && <p className="hint error-text">{problem}</p>}
        {!cal ? <p className="hint">Reading the calendar…</p> : <>
            {t ? <p className="meta">{t.weekday}, {t.date} · {String(Math.floor(t.hour)).padStart(2, '0')}:{String(Math.round((t.hour % 1) * 60) % 60).padStart(2, '0')} (as last saved)</p>
                : <p className="hint">No game server has saved this world yet.</p>}
            {t && <ul className="dm-bonds">
                <li><b>Marketday</b><span>{days(t.nextMarket)}: stalls at every town's market in the morning.</span></li>
                <li><b>Restday</b><span>{days(t.nextRest)}: no work but the watch; shops open the morning.</span></li>
                <li><b>{t.season} festival</b><span>{days(t.nextFestival)} ({t.festivalDate}): each town keeps it from noon.</span></li>
            </ul>}
            {me.role !== 'viewer' && cal.communities.length > 0 && <div className="dm-fields">
                <label className="field"><span>Call a festival in</span>
                    <select value={town} onChange={e => setTown(e.target.value)}>
                        {cal.communities.map(c => <option key={c.id} value={c.id}>{c.id} ({c.residents} residents)</option>)}</select></label>
                <label className="field"><span>Its name (empty: the season's own)</span>
                    <input value={name} maxLength={60} placeholder="The Lantern Night" onChange={e => setName(e.target.value)} /></label>
                <label className="field"><span>When</span>
                    <select value={when} onChange={e => setWhen(Number(e.target.value))}>
                        {[0, 1, 2, 3, 7, 14].map(n => <option key={n} value={n}>{days(n)}{n === 0 ? ' (from noon)' : ''}</option>)}</select></label>
                <button className={target === 'prod' ? 'publish' : 'primary'} disabled={busy || !town} onClick={() => void call()}>
                    {busy ? 'Calling…' : 'Call the festival'}</button>
            </div>}
            {cal.actions.length > 0 && <ol className="dm-actions">{cal.actions.slice(0, 5).map(a => <li key={a.id} className={a.status}>
                <b>{a.payload.name || 'festival'}</b> in {a.target}, {days(a.payload.inDays ?? 0)} · by {a.by}
                <span>{a.status === 'queued' ? `waiting for a ${target.toUpperCase()} game server…` : `${a.status}: ${a.result}`}</span></li>)}</ol>}
        </>}
    </section>;
}
