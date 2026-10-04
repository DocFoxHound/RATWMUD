// Server Health (Docs/Design/31-responsiveness.md, the health tracker): how the game server has been running, from
// the records it keeps each minute (dm.health). Tick times against the 50 ms budget, players online, their ping as
// their pages report it, the slowest ticks and where their time went, and the players with the worst ping.
import {useCallback, useEffect, useState} from 'react';
import {dmApi, type Health, type HealthWindow, type Target} from './api';

const SPANS: {hours: number; label: string}[] = [{hours: 1, label: 'Hour'}, {hours: 6, label: '6 hours'}, {hours: 24, label: 'Day'}, {hours: 168, label: 'Week'}];
const Budget = 50;                                      // ms: a tick at 20 a second.

export function HealthTab({target}: {target: Target}) {
    const [hours, setHours] = useState(6);
    const [data, setData] = useState<Health | null>(null);
    const [problem, setProblem] = useState('');
    const load = useCallback(() => dmApi.health(target, hours).then(d => { setData(d); setProblem(''); })
        .catch(e => setProblem((e as Error).message)), [target, hours]);
    useEffect(() => { void load(); const t = setInterval(load, 30000); return () => clearInterval(t); }, [load]);

    const windows = data?.windows ?? [];
    const latest = windows.at(-1);
    const over = windows.reduce((n, w) => n + w.over50, 0);
    return <div className="dm-health">
        <div className="dm-sheet-bar">
            <strong>Server Health · {target.toUpperCase()}</strong>
            <span className="meta hint">{latest ? `Last minute: ${latest.clients} players, ticks ${latest.mean.toFixed(1)} ms (p99 ${latest.p99.toFixed(0)}), ` +
                (latest.ping !== null ? `ping ${latest.ping.toFixed(0)} ms` : 'no ping reports') : 'No records yet.'}</span>
            {SPANS.map(s => <button key={s.hours} className={hours === s.hours ? 'on' : ''} onClick={() => setHours(s.hours)}>{s.label}</button>)}
            <button onClick={() => void load()}>Refresh</button>
        </div>
        {problem && <p className="error">{problem}</p>}
        {data?.missing && <p className="hint">The health table isn't in this database yet: run <code>python3 tools/world_db.py migrate</code>.</p>}
        {data && !data.missing && !windows.length && <p className="hint">Nothing recorded in this span. The game server writes a record each minute while it runs.</p>}
        {windows.length > 0 && <div className="dm-health-charts">
            <Chart title={`Tick time (ms) · ${over} ticks over the ${Budget} ms budget`} windows={windows} budget={Budget}
                lines={[{label: 'p99', color: '#e0a35c', get: w => w.p99}, {label: 'mean', color: '#8fc7a0', get: w => w.mean}]} />
            <Chart title="Players online" windows={windows} lines={[{label: 'players', color: '#8fb3e0', get: w => w.clients}]} />
            <Chart title="Ping (ms), as players' pages report it" windows={windows}
                lines={[{label: '95th', color: '#e0a35c', get: w => w.ping95}, {label: 'middle', color: '#8fc7a0', get: w => w.ping}]} />
        </div>}
        {data && (data.spikes.length > 0 || data.players.length > 0) && <div className="dm-health-tables">
            <div>
                <h3>Slowest ticks</h3>
                <table className="dm-table"><thead><tr><th>When</th><th>ms</th><th>Players</th><th>Where the time went</th></tr></thead>
                    <tbody>{data.spikes.map(s => <tr key={s.at + s.ms} title={s.note}>
                        <td>{when(s.at)}</td><td className="num">{s.ms.toFixed(0)}</td><td className="num">{s.clients}</td>
                        <td>{s.parts.map(([name, ms]) => `${name} ${ms.toFixed(0)}`).join(', ')}</td></tr>)}</tbody></table>
            </div>
            <div>
                <h3>Worst ping by player</h3>
                <table className="dm-table"><thead><tr><th>Player</th><th>ms</th><th>When</th></tr></thead>
                    <tbody>{data.players.map(p => <tr key={p.name}><td>{p.name}</td><td className="num">{p.ms.toFixed(0)}</td><td>{when(p.at)}</td></tr>)}</tbody></table>
            </div>
        </div>}
    </div>;
}

function when(at: string): string {
    const d = new Date(at);
    return `${d.toLocaleDateString(undefined, {month: 'short', day: 'numeric'})} ${d.toLocaleTimeString(undefined, {hour: '2-digit', minute: '2-digit'})}`;
}

type Line = {label: string; color: string; get: (w: HealthWindow) => number | null};

/** A line chart over the span, drawn in SVG: one line per series, an optional budget line, time along the bottom. */
function Chart({title, windows, lines, budget}: {title: string; windows: HealthWindow[]; lines: Line[]; budget?: number}) {
    const W = 900, H = 160, L = 44, R = 8, T = 10, B = 22;
    const times = windows.map(w => new Date(w.at).getTime());
    const t0 = times[0], t1 = Math.max(times.at(-1)!, t0 + 1);
    const values = windows.flatMap(w => lines.map(l => l.get(w)).filter((v): v is number => v !== null));
    const top = Math.max(1, budget ?? 0, ...values) * 1.1;
    const x = (t: number) => L + (t - t0) / (t1 - t0) * (W - L - R);
    const y = (v: number) => T + (1 - v / top) * (H - T - B);
    const path = (l: Line) => {
        let d = '', gap = true;
        windows.forEach((w, i) => {
            const v = l.get(w);
            if (v === null || (i > 0 && times[i] - times[i - 1] > 5 * 60000)) { gap = true; if (v === null) return; }
            d += `${gap ? 'M' : 'L'}${x(times[i]).toFixed(1)},${y(v).toFixed(1)}`;
            gap = false;
        });
        return d;
    };
    const ticks = [0, 0.25, 0.5, 0.75, 1].map(f => t0 + f * (t1 - t0));
    return <figure className="dm-chart">
        <figcaption>{title} <span className="legend">{lines.map(l => <span key={l.label} style={{color: l.color}}>■ {l.label} </span>)}</span></figcaption>
        <svg viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none" role="img" aria-label={title}>
            {[0, 0.5, 1].map(f => <g key={f}>
                <line x1={L} x2={W - R} y1={y(top / 1.1 * f)} y2={y(top / 1.1 * f)} stroke="#1f2a26" />
                <text x={L - 6} y={y(top / 1.1 * f) + 4} textAnchor="end" fontSize="10" fill="#7d8c86">{Math.round(top / 1.1 * f)}</text>
            </g>)}
            {budget !== undefined && <line x1={L} x2={W - R} y1={y(budget)} y2={y(budget)} stroke="#a8473f" strokeDasharray="4 4" />}
            {lines.map(l => <path key={l.label} d={path(l)} fill="none" stroke={l.color} strokeWidth="1.6" />)}
            {ticks.map(t => <text key={t} x={x(t)} y={H - 6} textAnchor="middle" fontSize="10" fill="#7d8c86">
                {new Date(t).toLocaleTimeString(undefined, {hour: '2-digit', minute: '2-digit'})}</text>)}
        </svg>
    </figure>;
}
