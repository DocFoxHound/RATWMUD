// Money: where the world's money is, as last saved. Each town's treasury, church and buyers; the great houses and the
// shops' tills; residents' purses by kind of work, the poorest and richest tenth and how many are short of a day's food
// money; money on the road. (The Wharf's book with the world beyond and its trade credit went with the Wharf's money:
// Docs/Design/79-a-staged-economy.md, 17.)
import {useCallback, useEffect, useState} from 'react';
import {dmApi, type Me, type Money, type Target} from './api';

const p = (n: number | null | undefined) => n === null || n === undefined ? '—' : `${Math.round(n).toLocaleString()}p`;
const title = (id: string) => id.replace(/^(house|stores|town):/, '').replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());

/** (`me` is taken for the tabs' sake; the money view is the same for every role.) */
// The funds' names (docs 67, 68, 73): the Wharf's purse (the Country's before doc 73), carriage where a world has no
// carters' yard, the old store fund; the Houses' estates (doc 72, Phase 6: estate:<id>; Grayrock's is the Wharf's purse
// while they are split; a world from before doc 72's split has no estate:<id> purse and it is the one purse). Since doc 79
// (Phase 3) these are a pre-79 world's names: Phase 9 makes this tab the Prosperity tab.
const ESTATES: Record<string, string> = {greatly: 'House Greatly', timberland: 'House Timberland', fell: 'House Fell', crown: 'The Crown'};
function fundName(id: string, split = false) {
    if (id === 'outside:wharf' || id === 'outside:country') return split ? 'The Wharf\'s purse (House Grayrock\'s estate)' : 'The Wharf\'s purse';
    if (id.startsWith('estate:')) return `${ESTATES[id.slice(7)] ?? title(id.slice(7))}'s estate`;
    if (id.startsWith('outside:')) return `Outside market: ${id.slice(8)}`;
    if (id === 'fund:carriage') return 'Carriage (no carters\' yard)';
    if (id === 'fund:store') return 'The store room\'s old fund';
    return id;
}
function fundTitle(id: string) {
    if (id.startsWith('estate:')) return 'Its lands\' rents in, its own people\'s wages out; the Crown\'s grants any estate short of its posts (doc 72, Phase 6)';
    if (id.startsWith('outside:')) return 'What the store room\'s draws paid it for the goods it lands, less the gate toll and what it paid beyond the realm; it pays the wharf\'s posts (docs 68, 73)';
    if (id === 'fund:carriage') return 'Carriage paid where the world has no carters\' yard to take it (doc 68, 6)';
    return id;
}

export function MoneyTab({target}: {target: Target; me?: Me}) {
    const [data, setData] = useState<Money | null>(null);
    const [problem, setProblem] = useState('');
    const load = useCallback(() => dmApi.money(target).then(d => { setData(d); setProblem(''); })
        .catch(e => setProblem((e as Error).message)), [target]);
    useEffect(() => { void load(); const t = setInterval(load, 60000); return () => clearInterval(t); }, [load]);
    const r = data?.residents;
    return <div className="dm-health dm-money">
        <div className="dm-sheet-bar">
            <strong>Money · {target.toUpperCase()}</strong>
            <span className="meta hint">{data ? `${p(data.total)} in all, as last saved` : 'Loading…'}</span>
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
                    <tr><td title="Every shop keeps a till of its own: its shelves and its money">Shops' tills, all ({data.tills.count})</td><td className="num">{p(data.tills.total)}</td></tr>
                </tbody></table>
                <h3>On the road and elsewhere</h3>
                <table className="dm-table"><tbody>
                    <tr><td>Caravans</td><td className="num">{p(data.road.caravans)}</td></tr>
                    <tr><td>Contracts' rewards held</td><td className="num">{p(data.road.contracts)}</td></tr>
                    <tr><td>Bandits' hoards</td><td className="num">{p(data.road.bandits)}</td></tr>
                    <tr><td>Players</td><td className="num">{p(data.players)}</td></tr>
                    {(data.funds ?? []).map(f => <tr key={f.id}><td title={fundTitle(f.id)}>{fundName(f.id, (data.funds ?? []).some(g => g.id.startsWith('estate:')))}</td><td className="num">{p(f.cash)}</td></tr>)}
                    {(data.carters ?? []).map(c => <tr key={c.id}><td title={`${c.label}: what carriage paid the yard (doc 68)`}>Carters: {c.name}</td>
                        <td className="num">{p(c.cash)}</td></tr>)}
                </tbody></table>
            </div>
        </div>}
    </div>;
}
