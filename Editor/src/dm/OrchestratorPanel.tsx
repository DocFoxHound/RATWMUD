// The economy's monitor and the Dungeon Master's levers, in the Money tab (Docs/Design/46-economy-orchestrator.md, Part 9;
// Docs/Design/64-economy-migration.md, Phase 12). The monitor measures every day and weighs the week the evening of the
// reckoning: the land's and each town's distress and its kind, hunger, the short and the poor, the idle, the shops' food,
// their takings, coins between towns and players' help; and the land's Gini, the poorer half's share and the residents'.
// It sets nothing. The levers (doc 64, decision 5): the tax rate, the mint on or off, and a one-off relief grant from the
// capital's treasury to a town's, each queued for the game and logged in its ledger.
import {useEffect, useMemo, useState} from 'react';
import {dmApi, type Me, type Money, type Orchestrator, type Role, type Target} from './api';

export const p = (n: number | null | undefined) => n === null || n === undefined ? '—' : `${Math.round(n).toLocaleString()}p`;
export const title = (id: string) => id.replace(/^(house|stores|town):/, '').replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());
const pct = (n: number | null | undefined, digits = 0) => n === null || n === undefined ? '—' : `${(n * 100).toFixed(digits)}%`;
const num = (n: number | null | undefined, digits = 1) => n === null || n === undefined ? '—' : n.toFixed(digits);

export function OrchestratorPanel({money, target, me, onChanged}: {money: Money; target: Target; me?: Me; onChanged: () => void}) {
    const [role, setRole] = useState<Role | null>(me?.role ?? null);
    useEffect(() => { if (!me) dmApi.me().then(m => setRole(m.role)).catch(() => setRole('viewer')); }, [me]);
    const canAct = role !== null && role !== 'viewer';
    return <section className="dm-orchestrator" style={{padding: '0 12px 12px'}}>
        {money.orchestrator ? <Measures o={money.orchestrator} /> : <><h3>The economy's measures</h3><p className="hint">Nothing has been measured yet.</p></>}
        <Levers money={money} target={target} canAct={canAct} onChanged={onChanged} />
    </section>;
}

function Measures({o}: {o: Orchestrator}) {
    const b = o.brief ?? null;                 // The latest day's measures.
    const w = o.decision && o.decision.day >= 0 ? o.decision : null;   // The latest week's.
    const towns = useMemo(() => [...(b?.towns ?? [])].sort((x, y) => y.distress - x.distress), [b]);
    return <>
        <h3>The economy's measures</h3>
        <p className="meta">
            Measured: day {(b?.day ?? o.day) + 1}
            {b && <> · land distress {pct(b.landDistress)} · median purse {p(b.median)} · Gini {num(b.gini, 2)}</>}
            {b?.residentShare !== undefined && <> · <span title="Residents' share of the land's money">residents' share {pct(b.residentShare)}</span></>}
            {b?.bottomShare !== undefined && <> · <span title="The poorer half of households' share of the households' money">poorer half {pct(b.bottomShare)}</span></>}
            {' · '}{w ? <>week weighed: day {w.day + 1} (after the reckoning) · week's distress {pct(w.landDistress)}</> : 'the week is weighed the evening of the weekly reckoning'}
            {b && <> · money in the land {p(b.moneySupply)}</>}
        </p>
        {!b ? <p className="hint">Nothing has been measured yet.</p> : <div className="dm-table-wrap"><table className="dm-table"><thead><tr>
            <th>Town</th><th>People</th><th>Distress</th><th title="A day's food for one, in this town">Food cost</th>
            <th>Hungry</th><th>Starving</th><th title="Households with under a week's food, at home and in purse together">Short</th><th>Poor</th><th title="Without work">Idle</th>
            <th title="Days of food on its shops' shelves">Shop food</th><th title="Its shops' takings against their running">Takings</th>
            <th title="Coins in from other towns less coins out, since the last measure">Net in</th><th title="The week's distress, at the last reckoning">Week</th>
            <th title="Players' help since the last measure (doc 57): coins they spent here, its shops paid them, and goods they brought, at its price">Players in · out · goods</th></tr></thead>
            <tbody>{towns.map(t => <tr key={t.id}>
                <td>{title(t.id)}</td><td className="num">{t.people}</td>
                <td className="num">{pct(t.distress)} <span className="meta">{t.kind || 'well'}</span></td>
                <td className="num">{p(t.foodCost)}</td><td className="num">{pct(t.hungry, 1)}</td><td className="num">{pct(t.starving, 1)}</td>
                <td className="num">{pct(t.short)}</td><td className="num">{pct(t.poor)}</td><td className="num">{t.idle < 0 ? '—' : t.idle}</td>
                <td className="num">{num(t.shopFoodDays)} d</td><td className="num">{pct(t.takingsRatio)}</td><td className="num">{p(t.netInflow)}</td>
                <td className="num">{pct(w?.towns.find(x => x.id === t.id)?.week)}</td>
                <td className="num">{p(t.playerIn ?? 0)} · {p(t.playerOut ?? 0)} · {p(t.playerGoods ?? 0)}</td></tr>)}
            </tbody></table></div>}
    </>;
}

/** The tax rate, the mint, and a relief grant (doc 64, decision 5). */
function Levers({money, target, canAct, onChanged}: {money: Money; target: Target; canAct: boolean; onChanged: () => void}) {
    const levers = money.levers;
    const towns = useMemo(() => Object.keys(money.towns).sort(), [money]);
    const [rate, setRate] = useState(((levers?.taxRate ?? .08) * 100).toFixed(1));
    const [town, setTown] = useState(''), [coins, setCoins] = useState(100);
    const [busy, setBusy] = useState(false), [problem, setProblem] = useState(''), [message, setMessage] = useState('');
    const place = town || towns[0] || '';

    const queue = async (what: string, act: () => Promise<unknown>) => {
        if (target === 'prod' && !window.confirm(`${what} on PROD?`)) return;
        setBusy(true); setProblem(''); setMessage('');
        try { await act(); setMessage(`Queued: ${what.toLowerCase()}. The game logs it in its ledger.`); onChanged(); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };
    const setTax = () => {
        const share = Number(rate) / 100;
        if (!Number.isFinite(share) || share < 0 || share > .5) { setProblem('The tax rate is 0% to 50% of a week\'s income.'); return; }
        void queue(`Set the tax rate to ${Number(rate)}%`, () => dmApi.setTaxRate(target, share));
    };
    const grant = () => {
        if (!place) { setProblem('Choose a town.'); return; }
        if (!Number.isInteger(coins) || coins < 1) { setProblem('A relief grant is a whole number of pennies, 1 or more.'); return; }
        if (money.capital !== null && coins > money.capital) { setProblem(`The capital's treasury holds ${p(money.capital)}.`); return; }
        void queue(`Grant ${p(coins)} to ${title(place)}`, () => dmApi.reliefGrant(target, place, coins));
    };

    return <div>
        <h3>The Dungeon Master's levers</h3>
        <p className="meta">As last saved: tax {levers ? pct(levers.taxRate, 1) : '—'} of a week's income · the mint {levers ? (levers.mint ? 'on' : 'off') : '—'}
            {' · '}the capital's treasury {p(money.capital)}</p>
        <fieldset disabled={!canAct || busy} className="dm-fields">
            <div className="row">
                <label className="field"><span>Tax rate (% of a week's income, from the next reckoning)</span>
                    <input type="number" value={rate} min={0} max={50} step={0.5} onChange={e => setRate(e.target.value)} /></label>
                <button className={target === 'prod' ? 'publish' : 'primary'} onClick={setTax}>Set</button>
            </div>
            <div className="row">
                <span className="hint">The mint strikes when money in use runs short and does harm, and melts what it struck when there's too much.</span>
                <button className={target === 'prod' ? 'publish' : 'primary'} disabled={levers?.mint === true}
                    onClick={() => void queue('Turn the mint on', () => dmApi.setMint(target, true))}>Mint on</button>
                <button className="danger" disabled={levers?.mint === false}
                    onClick={() => void queue('Turn the mint off', () => dmApi.setMint(target, false))}>Mint off</button>
            </div>
            <div className="row">
                <label className="field"><span>Relief grant to</span>
                    <select value={place} onChange={e => setTown(e.target.value)}>
                        {towns.map(t => <option key={t} value={t}>{title(t)}</option>)}</select></label>
                <label className="field"><span>Pennies, from the capital's treasury</span>
                    <input type="number" value={coins} min={1} step={1} onChange={e => setCoins(Math.round(Number(e.target.value)))} /></label>
                <button className={target === 'prod' ? 'publish' : 'primary'} onClick={grant}>Grant</button>
            </div>
        </fieldset>
        {!canAct && <p className="hint">Viewers can't move the levers.</p>}
        {problem && <p className="hint error-text">{problem}</p>}
        {message && <p className="hint">{message}</p>}
    </div>;
}
