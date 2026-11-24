// The Campaigns list (doc 85, 5.2): every run of the world, and for each live one whether it is alive, where it is,
// who is in it, when anything last happened and what it is waiting for.
import {useState} from 'react';
import {storyProgram, type Live, type Target} from '../api';
import {CampaignCards, NewCampaign} from './Chapters';
import {SentencesPanel} from './Sentences';
import {WorldVarsPanel} from './WorldVars';
import {ago, alertText, audienceText, filterRuns, hereCount, lastText, listCounts, questsText, readySummary, waitingOn, type Alert, type FramePerson, type ListFilter, type Overview} from '../../lib/campaigns';

// (Doc 86, R3: "ready to advance" first among the alerts, then the others needing you, then the rest.)
const alertRank = (a: Alert) => (a.kind === 'ready' ? 0 : a.needsYou ? 1 : 2);

const FILTERS: {id: ListFilter; label: string}[] = [
    {id: 'all', label: 'All'}, {id: 'running', label: 'Running'}, {id: 'held', label: 'Held'}, {id: 'paused', label: 'Paused'}, {id: 'approved', label: 'Approved'},
    {id: 'ended', label: 'Ended'},
];

// Each world's alert rules (doc 85, 5.6: "all adjustable per world"; 5.7: the owner's): the first values, or the owner's.
const RULES: {key: string; label: string; unit: string}[] = [
    {key: 'runnerSeconds', label: 'Runner silent after', unit: 's'}, {key: 'gameSeconds', label: 'Game not answering after', unit: 's'},
    {key: 'behindEvents', label: 'Behind by', unit: 'events'}, {key: 'behindSeconds', label: 'or behind by', unit: 's'},
    {key: 'quietHours', label: 'Quiet after', unit: 'h (0: never)'}, {key: 'overdueSeconds', label: 'Timer overdue after', unit: 's'},
    {key: 'goneDays', label: 'A wolf gone after', unit: 'days'}, {key: 'fullPercent', label: 'Audience nearly full at', unit: '%'},
];

export function AlertRules({target, thresholds, done}: {target: Target; thresholds: Record<string, number>; done: () => void}) {
    const [values, setValues] = useState<Record<string, string>>(() => Object.fromEntries(RULES.map(r => [r.key, String(thresholds[r.key] ?? '')])));
    const [problem, setProblem] = useState(''), [busy, setBusy] = useState(false);
    const save = async () => {
        setBusy(true); setProblem('');
        const rules = Object.fromEntries(RULES.filter(r => values[r.key] !== String(thresholds[r.key])).map(r => [r.key, values[r.key] === '' ? null : Number(values[r.key])]));
        try { if (Object.keys(rules).length) await storyProgram(target, 'alert-rules', {rules}); done(); } catch (e) { setProblem((e as Error).message); } finally { setBusy(false); }
    };
    return <div className="camp-rules" role="dialog" aria-label="Alert rules">
        {RULES.map(r => <label key={r.key}>{r.label}<span><input type="number" value={values[r.key]} onChange={e => setValues(v => ({...v, [r.key]: e.target.value}))} /> {r.unit}</span></label>)}
        <button className="primary" disabled={busy} onClick={() => void save()}>Save the rules</button><button onClick={done}>Cancel</button>
        <span className="hint">For this world (the owner's to change). Empty: back to the first value. A story may set its own quiet hours.</span>
        {problem && <span className="error-text">{problem}</span>}
    </div>;
}

export function CampaignList({overview, live, names, open, holdAll, target, mayRule, mayApprove, maySteerWorld}: {overview: Overview | null; live: Live | null;
    names: Record<string, string>; open: (run: string) => void; holdAll?: () => void; target?: Target; mayRule?: boolean;
    /** Phase 10: may approve a campaign of chapters; may change the world's variables (hosted: the owner, or "may steer campaigns"). */
    mayApprove?: boolean; maySteerWorld?: boolean}) {
    const [filter, setFilter] = useState<ListFilter>('all');
    const [ruling, setRuling] = useState(false);
    const [panel, setPanel] = useState<'' | 'vars' | 'new' | 'sentences'>('');
    if (!overview) return <p className="hint camp-pad">Loading the world's stories…</p>;
    const counts = listCounts(overview.runs);
    const people = live?.frame?.people as FramePerson[] | undefined;
    const runs = filterRuns(overview.runs, filter);
    return <div className="camp-list">
        <div className="camp-filters">
            {counts.needsYou > 0 ? <span className="camp-needs">⚠ {counts.needsYou} need{counts.needsYou === 1 ? 's' : ''} you</span> : <span className="hint">Nothing needs you.</span>}
            {FILTERS.map(f => <button key={f.id} className={filter === f.id ? 'on' : ''} onClick={() => setFilter(f.id)}>{f.label} {counts[f.id]}</button>)}
            {holdAll && counts.running > 0 && <button className="camp-hold-all" onClick={holdAll}>⏸ Hold all…</button>}
            {target && mayRule && overview.thresholds && <button onClick={() => setRuling(r => !r)}>Alert rules…</button>}
            {target && <button className={panel === 'vars' ? 'on' : ''} onClick={() => setPanel(p => p === 'vars' ? '' : 'vars')}>World variables…</button>}
            {target && <button className={panel === 'sentences' ? 'on' : ''} onClick={() => setPanel(p => p === 'sentences' ? '' : 'sentences')}>Sentences…</button>}
            {target && mayApprove && <button className={panel === 'new' ? 'on' : ''} onClick={() => setPanel(p => p === 'new' ? '' : 'new')}>New campaign…</button>}
            {!live?.frame && <span className="hint">Who is in the world: {live ? 'no positions yet (is the game server running?)' : 'connecting…'}</span>}
        </div>
        {ruling && target && overview.thresholds && <AlertRules target={target} thresholds={overview.thresholds} done={() => setRuling(false)} />}
        {panel === 'vars' && target && <WorldVarsPanel target={target} names={names} mayChange={!!maySteerWorld} />}
        {panel === 'sentences' && target && <SentencesPanel target={target} mayRelease={!!maySteerWorld} />}
        {panel === 'new' && target && <NewCampaign target={target} done={run => { setPanel(''); if (run) open(run); }} />}
        <CampaignCards campaigns={overview.campaigns ?? []} open={open} runs={overview.runs} target={target} maySteer={!!maySteerWorld} />
        {overview.runs.length === 0 ? <p className="hint camp-pad">No world stories in this world yet. Write one in Story Creator, rehearse it, approve and publish it: it shows here.</p>
            : <table className="dm-table camp-table"><thead><tr>
                <th>Story</th><th>Status</th><th>Phase</th><th>Ready</th><th>Wolves</th><th>Missions</th><th>Last happened</th><th>Waiting on</th>
            </tr></thead><tbody>{runs.map(r => {
                const wait = waitingOn(r.waiting, names, overview.now.day, undefined, overview.now.at);
                const alerts = r.alerts ?? [];
                const ready = readySummary(r.ready);
                return <tr key={r.run} onClick={() => open(r.run)} className={alerts.some(a => a.needsYou) ? 'needs' : ''}>
                    <td><b>{r.title}</b><small>{r.approver ? `approved by ${r.approver}` : ''}{r.approvedAt ? ` · ${ago(r.approvedAt, overview.now.at)}` : ''}</small></td>
                    <td><span className={`camp-status ${r.status}`}>{r.status}</span>{r.commandsQueued ? <small className="camp-warn">{r.commandsQueued} command{r.commandsQueued === 1 ? '' : 's'} sent</small> : null}
                        {alerts.map(a => <small key={a.kind} className={a.needsYou ? 'error-text' : 'camp-warn'} title={alertText(a, names)}>⚠ {a.kind}</small>)}</td>
                    <td>{r.phase ?? '-'}{r.visit ? <small>visit {r.visit}{r.entered !== undefined && overview.now.day !== null ? ` · ${(overview.now.day - r.entered).toFixed(1)} days in` : ''}</small> : null}</td>
                    <td>{ready ? <span className={`camp-ready-mini ${ready.met ? 'met' : ''}`} title={ready.text}>
                        <span className="camp-ready-bar"><span className="fill" style={{width: `${Math.round(ready.fill * 100)}%`}} /></span>
                        {ready.met ? <b className="camp-ready-tag">ready</b> : <small>{ready.text}</small>}</span> : '-'}</td>
                    <td>{r.audience ? audienceText(r.audience, r.wolves) : r.wolves ?? '-'}{r.cohort && <small>{hereCount(r.cohort, people)} here now</small>}</td>
                    <td>{questsText(r.missions)}</td>
                    <td>{lastText(r.last, names)}{r.last?.at && <small>{ago(r.last.at, overview.now.at)}{r.last.day != null ? ` · day ${r.last.day.toFixed(1)}` : ''}</small>}</td>
                    <td className={wait ? `camp-wait ${wait.tone}` : ''}>{wait ? <><b>{wait.who}</b><small>{wait.text}</small></> : '-'}</td>
                </tr>;
            })}</tbody></table>}
        {overview.runs.some(r => (r.alerts ?? []).length) && <ul className="camp-alerts">{overview.runs.flatMap(r => (r.alerts ?? []).map(a => ({r, a})))
            .sort((x, y) => alertRank(x.a) - alertRank(y.a)).map(({r, a}) =>
            <li key={r.run + a.kind} className={a.needsYou ? 'needs' : ''}><button className="link-button" onClick={() => open(r.run)}>{r.title}</button> · {alertText(a, names)}</li>)}</ul>}
    </div>;
}
