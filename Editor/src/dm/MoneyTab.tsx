// Money (Docs/Design/42-money-in-circulation.md, Phase 8): where the world's money is, as last saved. Each town's
// treasury, church and buyers; the great houses and their tills; residents' purses by kind of work, the poorest and
// richest tenth and how many are short of a day's food money; money on the road; and the latest reckonings, surplus
// spending, trade caravans and sermons.
import {useCallback, useEffect, useState} from 'react';
import {dmApi, type Money, type Target} from './api';

const p = (n: number | null | undefined) => n === null || n === undefined ? '—' : `${Math.round(n).toLocaleString()}p`;
const title = (id: string) => id.replace(/^(house|stores|town):/, '').replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());

export function MoneyTab({target}: {target: Target}) {
    const [data, setData] = useState<Money | null>(null);
    const [problem, setProblem] = useState('');
    const load = useCallback(() => dmApi.money(target).then(d => { setData(d); setProblem(''); })
        .catch(e => setProblem((e as Error).message)), [target]);
    useEffect(() => { void load(); const t = setInterval(load, 60000); return () => clearInterval(t); }, [load]);
    const r = data?.residents;
    return <div className="dm-health dm-money">
        <div className="dm-sheet-bar">
            <strong>Money · {target.toUpperCase()}</strong>
            <span className="meta hint">{data ? `${p(data.total)} in all, as last saved` + (data.month !== null ? ` · month ${data.month}` : '') : 'Loading…'}</span>
            <button onClick={() => void load()}>Refresh</button>
        </div>
        {problem && <p className="error">{problem}</p>}
        {data && <div className="dm-health-tables">
            <div>
                <h3>Towns</h3>
                <table className="dm-table"><thead><tr><th>Town</th><th>Residents</th><th>Treasury</th><th>Church</th><th>Its buyers</th></tr></thead>
                    <tbody>
                        <tr><td>The capital's treasury</td><td></td><td className="num">{p(data.capital)}</td><td></td><td></td></tr>
                        {Object.entries(data.towns).map(([id, t]) => <tr key={id}>
                            <td>{title(id)}</td><td className="num">{t.residents}</td><td className="num">{p(t.treasury)}</td>
                            <td className="num">{p(t.church)}</td>
                            <td>{Object.entries(t.buyers).map(([b, cash]) => `${b} ${p(cash)}`).join(', ') || '—'}</td></tr>)}
                    </tbody></table>
            </div>
            <div>
                <h3>Residents</h3>
                {r && <table className="dm-table"><tbody>
                    <tr><td>Purses</td><td className="num">{r.count} · {p(r.total)}</td></tr>
                    <tr><td>Middle purse</td><td className="num">{p(r.median)}</td></tr>
                    <tr><td>Poorest tenth, on average</td><td className="num">{p(r.poorestTenth)}</td></tr>
                    <tr><td>Richest tenth, on average</td><td className="num">{p(r.richestTenth)}</td></tr>
                    <tr title="Under 6p: less than a day's food"><td>Short of a day's food money</td><td className="num">{r.shortOfFood}</td></tr>
                    {Object.entries(r.byRole).map(([role, g]) => <tr key={role}><td>{role}s</td>
                        <td className="num">{g.count} · {p(g.total)} · middle {p(g.median)}</td></tr>)}
                </tbody></table>}
                <h3>Great houses</h3>
                <table className="dm-table"><tbody>
                    {data.houses.map(h => <tr key={h.id}><td>{title(h.id)}</td><td className="num">{p(h.cash)}</td></tr>)}
                    <tr><td>Their businesses' tills ({data.tills.count})</td><td className="num">{p(data.tills.total)}</td></tr>
                </tbody></table>
                <h3>On the road and elsewhere</h3>
                <table className="dm-table"><tbody>
                    <tr><td>Caravans</td><td className="num">{p(data.road.caravans)}</td></tr>
                    <tr><td>Contracts' rewards held</td><td className="num">{p(data.road.contracts)}</td></tr>
                    <tr><td>Bandits' hoards</td><td className="num">{p(data.road.bandits)}</td></tr>
                    <tr><td>Players</td><td className="num">{p(data.players)}</td></tr>
                </tbody></table>
            </div>
        </div>}
        {data && data.events.length > 0 && <div>
            <h3>Lately</h3>
            <table className="dm-table"><thead><tr><th>Day</th><th>What</th><th></th></tr></thead>
                <tbody>{data.events.map((e, i) => <tr key={i}><td className="num">{Math.floor(e.day) + 1}</td><td>{e.kind}</td><td>{e.detail}</td></tr>)}</tbody></table>
        </div>}
    </div>;
}
