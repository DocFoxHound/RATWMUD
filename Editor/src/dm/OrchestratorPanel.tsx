// The economy orchestrator's panel in the Money tab (Docs/Design/46-economy-orchestrator.md, Parts 9 and 10). It measures
// every day and decides once a week, the evening of the reckoning: the latest day's measures (the land's and each town's
// distress, the holders' bands), the latest week's decisions (what the holders over their band spend, where the pot goes by
// town and channel, the prices furthest from the catalogue), and the steers in force. A Dungeon Master steers it (never
// drives it): a steer shows in the next day's measures and acts at the next week's decision, for 1 to 56 days. Phase 8 adds
// the residents' and the poorer half's shares, each channel's reach and learned weight, the wage table, the funds and
// granaries, and scenarios (named bundles of steers, Data/Economy/scenarios.json).
import {useEffect, useMemo, useState} from 'react';
import {dmApi, type EconomyChannel, type Me, type Money, type Orchestrator, type Role, type Scenario, type SteerKind, type Target,
    WageKinds} from './api';

export const p = (n: number | null | undefined) => n === null || n === undefined ? '—' : `${Math.round(n).toLocaleString()}p`;
export const title = (id: string) => id.replace(/^(house|stores|town):/, '').replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());
const pct = (n: number | null | undefined, digits = 0) => n === null || n === undefined ? '—' : `${(n * 100).toFixed(digits)}%`;
const num = (n: number | null | undefined, digits = 1) => n === null || n === undefined ? '—' : n.toFixed(digits);
const times = (n: number | null | undefined) => n === null || n === undefined ? '—' : `×${n.toFixed(2)}`;
/** Channel names in the orchestrator's order, then any others it names. */
const channelOrder = (names: Iterable<string>) => {
    const seen = new Set(names);
    return [...CHANNELS.filter(c => seen.has(c)), ...[...seen].filter(c => !(CHANNELS as string[]).includes(c)).sort()];
};

/** An account's name as a Dungeon Master would say it. */
export function holderName(id: string) {
    if (id === 'treasury') return "The capital's treasury";
    if (id === 'town:all:church') return 'The church';
    if (id.startsWith('stores:')) return `${title(id)} treasury`;
    if (id.startsWith('till:')) return `Till: ${title(id.split(':').slice(2).join(':') || id.slice(5))}`;
    const town = /^town:([^:]+):(.+)$/.exec(id);
    if (town) return `${title(town[1])} ${town[2].replace(/_/g, ' ')}`;
    return title(id);
}

const MostHolders = 20;                     // (Hundreds may be growing: the largest shown.)
const MODES = {shadow: 'Shadow: planning only, not applied yet', on: 'On: its plans are applied', off: 'Off: not planning'};
const CHANNELS: EconomyChannel[] = ['works', 'hires', 'commissions', 'food', 'trade', 'price support', 'wage support', 'rescue', 'opening'];
const KINDS: {id: SteerKind; label: string; min: number; max: number; step: number; start: number; hint: string}[] = [
    {id: 'pressure', label: 'Pressure on the land', min: 0.5, max: 3, step: 0.1, start: 1.5,
        hint: 'How hard the bands squeeze everyone: 0.5 gentle, 1 as now, 3 hard.'},
    {id: 'town', label: 'Favour a town', min: 0, max: 3, step: 0.1, start: 1.5,
        hint: "A weight on the town's distress: above 1 sends it more of the pot, below 1 less, 0 none."},
    {id: 'holder', label: 'Squeeze or spare a holder', min: 0, max: 4, step: 0.1, start: 2,
        hint: '0 spares it (it may hoard for a while); 1.5 to 4 treats it as holding that many times its cash.'},
    {id: 'channel', label: 'Close or favour a channel', min: 0, max: 3, step: 0.1, start: 2,
        hint: '0 closes the channel; 1 as now; 2 doubles it; 3 at most.'},
    {id: 'price', label: 'Price shock', min: 0.5, max: 3, step: 0.1, start: 2,
        hint: 'One good dearer or cheaper: 0.5 a glut, 2 a scare, 3 a blockade. The orchestrator works around it.'},
];
const kindOf = (id: SteerKind) => KINDS.find(k => k.id === id)!;

export function OrchestratorPanel({money, target, me, onChanged}: {money: Money; target: Target; me?: Me; onChanged: () => void}) {
    const [role, setRole] = useState<Role | null>(me?.role ?? null);
    useEffect(() => { if (!me) dmApi.me().then(m => setRole(m.role)).catch(() => setRole('viewer')); }, [me]);
    const o = money.orchestrator;
    if (!o) return <section className="dm-orchestrator" style={{padding: '0 12px'}}>
        <h3>Orchestrator</h3><p className="hint">The orchestrator hasn't run yet.</p></section>;
    return <Plan o={o} money={money} target={target} canAct={role !== null && role !== 'viewer'} onChanged={onChanged} />;
}

function Plan({o, money, target, canAct, onChanged}: {o: Orchestrator; money: Money; target: Target; canAct: boolean; onChanged: () => void}) {
    const b = o.brief ?? null;                 // The latest day's measures.
    const w = o.decision && o.decision.day >= 0 ? o.decision : null;   // The latest week's decisions.
    const today = b?.day ?? o.day;
    const [message, setMessage] = useState(''), [problem, setProblem] = useState(''), [busy, setBusy] = useState(false);
    const towns = useMemo(() => [...(b?.towns ?? [])].sort((x, y) => y.distress - x.distress), [b]);
    const orders = useMemo(() => {
        const by = new Map<string, Map<string, number>>();
        for (const order of w?.orders ?? []) {
            const town = by.get(order.town) ?? new Map<string, number>();
            town.set(order.channel, (town.get(order.channel) ?? 0) + order.coins);
            by.set(order.town, town);
        }
        return [...by.entries()].map(([town, channels]) => ({town, channels: [...channels.entries()].sort((x, y) => y[1] - x[1]),
            total: [...channels.values()].reduce((a, c) => a + c, 0)})).sort((x, y) => y.total - x.total);
    }, [w]);
    const off = (x: {now: number; would: number; catalog: number}) => Math.max(Math.abs(x.now / (x.catalog || 1) - 1), Math.abs(x.would / (x.catalog || 1) - 1));
    const prices = useMemo(() => [...(w?.prices ?? [])].sort((x, y) => off(y) - off(x)).slice(0, 20), [w]);
    const support = prices.some(x => x.support !== undefined);
    const channels = useMemo(() => channelOrder([...Object.keys(w?.channels ?? {}), ...Object.keys(w?.reach ?? {}),
        ...Object.keys((w ?? b)?.learned ?? {})]), [w, b]);
    const wageTowns = useMemo(() => [...(b?.towns ?? [])].filter(t => t.wages && Object.keys(t.wages).length > 0)
        .sort((x, y) => x.id.localeCompare(y.id)), [b]);
    const queued = (text: string) => { setMessage(text); setProblem(''); onChanged(); };

    const end = async (id: string) => {
        if (target === 'prod' && !window.confirm(`End ${id} on PROD?`)) return;
        setBusy(true); setProblem(''); setMessage('');
        try { await dmApi.endSteer(target, id); setMessage("Queued — the steer ends before the next day's measures."); onChanged(); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };

    return <section className="dm-orchestrator" style={{padding: '0 12px 12px'}}>
        <h3>Orchestrator</h3>
        <p className="meta">
            <span title={MODES[o.mode] ?? o.mode} style={{border: '1px solid', borderRadius: 99, padding: '1px 8px', marginRight: 8,
                color: o.mode === 'on' ? '#8fb56f' : o.mode === 'off' ? 'var(--faint)' : 'var(--amber)'}}>{MODES[o.mode] ?? o.mode}</span>
            Measured: day {today + 1}
            {b && <> · land distress {pct(b.landDistress)} · median purse {p(b.median)} · Gini {num(b.gini, 2)}</>}
            {b?.residentShare !== undefined && <> · <span title="Residents' share of the land's money">residents' share {pct(b.residentShare)}</span></>}
            {b?.bottomShare !== undefined && <> · <span title="The poorer half of households' share of residents' money">poorer half {pct(b.bottomShare)}</span></>}
            {' · '}{w ? <>decided: day {w.day + 1} (after the reckoning) · week's distress {pct(w.landDistress)} · pot {p(w.pot)}
                · margin {pct(w.margin)}</> : 'no decision yet: it decides the evening of the weekly reckoning'}
            {(w ?? b)?.autoPressure !== undefined && <> · <span title="Its own pressure on the land, besides any steer">own pressure {times((w ?? b)?.autoPressure)}</span></>}
            {(w ?? b)?.floorLift !== undefined && <> · <span title="How far the living floor is lifted">floor lift {times((w ?? b)?.floorLift)}</span></>}
        </p>
        {problem && <p className="hint error-text">{problem}</p>}
        {message && <p className="hint">{message}</p>}
        {!b ? <p className="hint">Nothing has been measured yet.</p> : <>
            <div className="dm-health-tables" style={{padding: 0}}>
                <div>
                    <h3>Towns, most in distress first</h3>
                    <div className="dm-table-wrap"><table className="dm-table"><thead><tr>
                        <th>Town</th><th>People</th><th>Distress</th><th title="A day's food for one, in this town">Food cost</th>
                        <th>Hungry</th><th>Starving</th><th title="Households with under a week's food, at home and in purse together">Short</th><th>Poor</th><th title="Without work">Idle</th>
                        <th title="Days of food on its shops' shelves">Shop food</th><th>Wage floor</th><th title="Its share of the week's pot, at the last decision">Share</th></tr></thead>
                        <tbody>{towns.map(t => <tr key={t.id}>
                            <td>{title(t.id)}</td><td className="num">{t.people}</td>
                            <td className="num">{pct(t.distress)} <span className="meta">{t.kind || 'well'}</span></td>
                            <td className="num">{p(t.foodCost)}</td><td className="num">{pct(t.hungry, 1)}</td><td className="num">{pct(t.starving, 1)}</td>
                            <td className="num">{pct(t.short)}</td><td className="num">{pct(t.poor)}</td><td className="num">{t.idle < 0 ? "—" : t.idle}</td>
                            <td className="num">{num(t.shopFoodDays)} d</td><td className="num">{p(t.wageFloor)}</td>
                            <td className="num">{p(w?.towns.find(x => x.id === t.id)?.share)}</td></tr>)}
                        </tbody></table></div>
                </div>
                <div>
                    <h3>Holders over their band</h3>
                    <p className="meta">Today: {Object.entries(b.bands ?? {}).map(([k, n]) => `${k} ${n}`).join(' · ')}</p>
                    {!w ? <p className="hint">What they spend is set at the week's decision.</p> : w.holders.length === 0 ? <p className="hint">None at the last decision.</p> : <div className="dm-table-wrap"><table className="dm-table"><thead><tr>
                        <th>Holder</th><th>Kind</th><th>Town</th><th>Cash</th><th title="What it needs to keep going">Need</th><th>Band</th><th title="What it gained over the week">Gain</th><th title="Over the week after the decision">To spend</th></tr></thead>
                        <tbody>{[...w.holders].sort((x, y) => y.toSpend - x.toSpend).slice(0, MostHolders).map(h => <tr key={h.id} title={h.id}>
                            <td>{holderName(h.id)}</td><td>{h.kind}</td><td>{h.town ? title(h.town) : '—'}</td><td className="num">{p(h.cash)}</td>
                            <td className="num">{p(h.need)}</td><td>{h.band}</td><td className="num">{p(h.gain)}</td><td className="num">{p(h.toSpend)}</td></tr>)}</tbody></table>
                        {w.holders.length > MostHolders && <p className="meta">The {MostHolders} sending most of the {w.holders.length} at the last decision ({p(w.holders.slice().sort((x, y) => y.toSpend - x.toSpend).slice(MostHolders).reduce((a, h) => a + h.toSpend, 0))} more from the rest).</p>}</div>}
                </div>
            </div>
            <div className="dm-health-tables" style={{padding: 0}}>
                <div>
                    <h3>{o.mode === 'shadow' ? "Where the pot would go" : "Where the pot went"}</h3>
                    {orders.length === 0 ? <p className="hint">No orders at the last decision.</p> : <table className="dm-table"><thead><tr><th>Town</th><th>By channel</th><th>In all</th></tr></thead>
                        <tbody>{orders.map(t => <tr key={t.town}><td>{t.town === '*' || !t.town ? 'The land' : title(t.town)}</td>
                            <td>{t.channels.map(([c, coins]) => `${c} ${p(coins)}`).join(', ')}</td><td className="num">{p(t.total)}</td></tr>)}</tbody></table>}
                    <h3>Prices furthest from the catalogue</h3>
                    {prices.length === 0 ? <p className="hint">None.</p> : <table className="dm-table"><thead><tr><th>Town</th><th>Good</th><th>Catalogue</th><th>Now</th><th title="Where the orchestrator would set it">Would be</th>{support && <th title="Price support a piece, paid to its shops">Support</th>}</tr></thead>
                        <tbody>{prices.map(x => <tr key={`${x.town}|${x.item}`}><td>{title(x.town)}</td><td>{x.item.replace(/_/g, ' ')}</td>
                            <td className="num">{num(x.catalog, 2)}p</td><td className="num">{num(x.now, 2)}p</td><td className="num">{num(x.would, 2)}p</td>{support && <td className="num">{x.support ? `${num(x.support, 2)}p` : '—'}</td>}</tr>)}</tbody></table>}
                </div>
                <div>
                    <h3>Channels</h3>
                    {channels.length === 0 ? <p className="hint">No channels at the last decision.</p> : <table className="dm-table"><thead><tr>
                        <th>Channel</th><th title="Its share of the week's pot">Pot share</th><th title="How much of what it sent reached the poorer half, this week">Reach</th>
                        <th title="The weight it has learned from its reach (×0.5 to ×2)">Learned</th></tr></thead>
                        <tbody>{channels.map(c => <tr key={c}><td>{c}</td><td className="num">{p(w?.channels?.[c])}</td>
                            <td className="num">{pct(w?.reach?.[c])}</td><td className="num">{times((w ?? b).learned?.[c])}</td></tr>)}</tbody></table>}
                    {b.moneySupply !== undefined && <p className="meta">Money in the land: {p(b.moneySupply)}</p>}
                </div>
            </div>
            <div className="dm-health-tables" style={{padding: 0}}>
                <div>
                    <h3>Wage table</h3>
                    {wageTowns.length === 0 ? <p className="hint">No wage table in the latest measures.</p> : <div className="dm-table-wrap"><table className="dm-table"><thead><tr>
                        <th>Town</th>{WageKinds.map(k => <th key={k}>{k}</th>)}</tr></thead>
                        <tbody>{wageTowns.map(t => <tr key={t.id}><td>{title(t.id)}</td>
                            {WageKinds.map(k => <td key={k} className="num">{t.wages?.[k] === undefined ? '—' : `${num(t.wages[k])}p`}</td>)}</tr>)}</tbody></table></div>}
                    <p className="meta">Pennies a day.</p>
                </div>
                <FundsTable money={money} />
            </div>
        </>}
        <div className="dm-health-tables" style={{padding: 0}}>
            <div>
                <h3>Steers in force</h3>
                {o.steers.length === 0 ? <p className="hint">None: the orchestrator is left to itself.</p> : <table className="dm-table"><thead><tr>
                    <th>Steer</th><th>On</th><th>Strength</th><th>Days left</th><th>By</th><th>Why</th><th></th></tr></thead>
                    <tbody>{o.steers.map(s => <tr key={s.id} title={s.id}>
                        <td>{kindOf(s.kind)?.label ?? s.kind}</td>
                        <td>{s.kind === 'pressure' ? 'The land' : s.kind === 'holder' ? holderName(s.target) : s.kind === 'price'
                            ? `${s.item.replace(/_/g, ' ')} in ${s.target === '*' ? 'every town' : title(s.target)}` : s.kind === 'town' ? title(s.target) : s.target}</td>
                        <td className="num">{s.kind === 'holder' && s.strength === 0 ? 'spared' : `×${s.strength}`}</td>
                        <td className="num">{Math.max(0, s.until - today)}</td><td>{s.by}</td>
                        <td style={{whiteSpace: 'normal'}}>{s.note || '—'}</td>
                        <td>{canAct && <button className="danger" disabled={busy} onClick={() => void end(s.id)}>End</button>}</td></tr>)}</tbody></table>}
            </div>
            <div>
                <SteerForm o={o} money={money} target={target} canAct={canAct} onQueued={queued} />
                <ScenarioForm o={o} money={money} target={target} canAct={canAct} onQueued={queued} />
            </div>
        </div>
    </section>;
}

/** The orchestrator's funds by town and channel, the land's fund, and each town's granary (doc 46, Phases 5 and 7). */
function FundsTable({money}: {money: Money}) {
    const funds = money.funds ?? {}, granaries = money.granaries ?? {};
    const channels = useMemo(() => channelOrder(Object.values(money.funds ?? {}).flatMap(f => Object.keys(f))), [money]);
    const towns = useMemo(() => [...new Set([...Object.keys(money.funds ?? {}), ...Object.keys(money.granaries ?? {})])].sort(), [money]);
    return <div>
        <h3>Funds and granaries</h3>
        {towns.length === 0 && (money.landFund === null || money.landFund === undefined) ? <p className="hint">No funds or granaries in the last save.</p>
            : <div className="dm-table-wrap"><table className="dm-table"><thead><tr>
                <th>Town</th>{channels.map(c => <th key={c}>{c}</th>)}<th title="Its granary's cash">Granary</th><th title="Goods in its granary">Goods</th></tr></thead>
                <tbody>
                    {money.landFund !== null && money.landFund !== undefined && <tr title="fund:land"><td>The land</td>
                        <td className="num" colSpan={Math.max(1, channels.length)}>{p(money.landFund)}</td><td></td><td></td></tr>}
                    {towns.map(t => <tr key={t}><td>{title(t)}</td>
                        {channels.map(c => <td key={c} className="num">{p(funds[t]?.[c])}</td>)}
                        <td className="num">{p(granaries[t]?.cash)}</td><td className="num">{granaries[t] ? granaries[t].goods.toLocaleString() : '—'}</td></tr>)}
                </tbody></table></div>}
    </div>;
}

/** The towns and holders a steer or a scenario may name. */
function useChoices(o: Orchestrator, money: Money) {
    const towns = useMemo(() => [...new Set([...(o.brief?.towns ?? []).map(t => t.id), ...Object.keys(money.towns)])].sort(), [o, money]);
    const holders = useMemo(() => [...new Set([...(o.brief?.holders ?? []).map(h => h.id), ...money.houses.map(h => h.id),
        ...towns.map(t => `stores:${t}`), 'town:all:church', 'treasury'])], [o, money, towns]);
    return {towns, holders};
}

function ScenarioForm({o, money, target, canAct, onQueued}: {o: Orchestrator; money: Money; target: Target; canAct: boolean; onQueued: (text: string) => void}) {
    const {towns, holders} = useChoices(o, money);
    const [scenarios, setScenarios] = useState<Scenario[] | null>(null);
    const [chosen, setChosen] = useState(''), [town, setTown] = useState(''), [holder, setHolder] = useState('');
    const [busy, setBusy] = useState(false), [problem, setProblem] = useState('');
    useEffect(() => { dmApi.scenarios().then(list => { setScenarios(list); setChosen(c => c || list[0]?.id || ''); })
        .catch(error => { setScenarios([]); setProblem((error as Error).message); }); }, []);
    const scenario = scenarios?.find(s => s.id === chosen);
    const needsTown = !!scenario?.needs?.includes('town'), needsHolder = !!scenario?.needs?.includes('holder');
    const place = town || towns[0] || '';

    const submit = async () => {
        if (!scenario) { setProblem('Choose a scenario.'); return; }
        if (needsTown && !place) { setProblem('Choose a town.'); return; }
        if (needsHolder && (!holder.trim() || holder.trim().length > 120 || /\s/.test(holder.trim()))) { setProblem('Name the holder: an account such as house:fell.'); return; }
        const on = [needsTown ? title(place) : '', needsHolder ? holderName(holder.trim()) : ''].filter(Boolean).join(', ');
        if (target === 'prod' && !window.confirm(`Start "${scenario.name}"${on ? ` on ${on}` : ''} on PROD?`)) return;
        setBusy(true); setProblem('');
        try {
            const r = await dmApi.startScenario(target, {scenario: scenario.id, town: needsTown ? place : '', holder: needsHolder ? holder.trim() : ''});
            onQueued(`Queued ${r.queued.length} steer${r.queued.length === 1 ? '' : 's'} — they show in the next day's measures and act at the next week's decision.`);
        } catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };

    return <div>
        <h3>Start a scenario</h3>
        {scenarios === null ? <p className="hint">Loading…</p> : scenarios.length === 0 ? <p className="hint">No scenarios.</p> :
        <fieldset disabled={!canAct || busy} className="dm-fields">
            <label className="field"><span>Scenario</span>
                <select value={chosen} onChange={e => { setChosen(e.target.value); setProblem(''); }}>
                    {scenarios.map(s => <option key={s.id} value={s.id}>{s.name}</option>)}</select></label>
            {scenario && <p className="hint">{scenario.about}</p>}
            {needsTown && <label className="field"><span>Town</span>
                <select value={place} onChange={e => setTown(e.target.value)}>
                    {towns.map(t => <option key={t} value={t}>{title(t)}</option>)}</select></label>}
            {needsHolder && <label className="field"><span>Holder (an account)</span>
                <input value={holder} maxLength={120} list="dm-scenario-holders" placeholder="house:fell" onChange={e => setHolder(e.target.value)} />
                <datalist id="dm-scenario-holders">{holders.map(h => <option key={h} value={h}>{holderName(h)}</option>)}</datalist></label>}
            <button className={target === 'prod' ? 'publish' : 'primary'} disabled={!canAct || busy || !scenario} onClick={() => void submit()}>
                {busy ? 'Queuing…' : 'Start'}</button>
        </fieldset>}
        {!canAct && <p className="hint">Viewers can't start scenarios.</p>}
        {problem && <p className="hint error-text">{problem}</p>}
    </div>;
}

function SteerForm({o, money, target, canAct, onQueued}: {o: Orchestrator; money: Money; target: Target; canAct: boolean; onQueued: (text: string) => void}) {
    const [kind, setKind] = useState<SteerKind>('pressure');
    const spec = kindOf(kind);
    const {towns, holders} = useChoices(o, money);
    const goods = useMemo(() => [...new Set((o.brief?.prices ?? []).map(x => x.item))].sort(), [o]);
    const [town, setTown] = useState(''), [holder, setHolder] = useState(''), [channel, setChannel] = useState<EconomyChannel>('works');
    const [item, setItem] = useState(''), [strength, setStrength] = useState(spec.start), [days, setDays] = useState(7), [note, setNote] = useState('');
    const [busy, setBusy] = useState(false), [problem, setProblem] = useState('');
    const choose = (k: SteerKind) => { setKind(k); setStrength(kindOf(k).start); setProblem(''); };
    const place = kind === 'price' ? town || '*' : town && town !== '*' ? town : towns[0] || '';
    const targetId = kind === 'town' || kind === 'price' ? place : kind === 'holder' ? holder.trim() : kind === 'channel' ? channel : '';

    const problemWith = () => {
        if (!Number.isFinite(strength) || strength < spec.min || strength > spec.max) return `The strength is from ${spec.min} to ${spec.max}.`;
        if (kind === 'holder' && strength !== 0 && strength < 1.5) return 'A holder is spared (0) or squeezed (1.5 to 4).';
        if (!Number.isInteger(days) || days < 1 || days > 56) return 'A steer lasts 1 to 56 days.';
        if (kind === 'holder' && (!targetId || targetId.length > 120 || /\s/.test(targetId))) return 'Name the holder: an account such as house:fell.';
        if (kind === 'price' && (!item.trim() || item.trim().length > 60)) return 'Name the good.';
        if (kind === 'town' && !targetId) return 'Choose a town.';
        return '';
    };
    const submit = async () => {
        const wrong = problemWith();
        if (wrong) { setProblem(wrong); return; }
        if (target === 'prod' && !window.confirm(`Steer the orchestrator on PROD: ${spec.label.toLowerCase()} ×${strength} for ${days} days?`)) return;
        setBusy(true); setProblem('');
        try {
            await dmApi.steerEconomy(target, {kind, targetId, item: kind === 'price' ? item.trim() : '', strength, days, note: note.trim()});
            setNote('');
            onQueued("Queued — it shows in the next day's measures and acts at the next week's decision.");
        } catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };

    return <div>
        <h3>Steer the orchestrator</h3>
        <fieldset disabled={!canAct || busy} className="dm-fields">
            <label className="field"><span>Steer</span>
                <select value={kind} onChange={e => choose(e.target.value as SteerKind)}>
                    {KINDS.map(k => <option key={k.id} value={k.id}>{k.label}</option>)}</select></label>
            {(kind === 'town' || kind === 'price') && <label className="field"><span>Town</span>
                <select value={place} onChange={e => setTown(e.target.value)}>
                    {kind === 'price' && <option value="*">Every town</option>}
                    {towns.map(t => <option key={t} value={t}>{title(t)}</option>)}</select></label>}
            {kind === 'holder' && <label className="field"><span>Holder (an account)</span>
                <input value={holder} maxLength={120} list="dm-steer-holders" placeholder="house:fell" onChange={e => setHolder(e.target.value)} />
                <datalist id="dm-steer-holders">{holders.map(h => <option key={h} value={h}>{holderName(h)}</option>)}</datalist></label>}
            {kind === 'channel' && <label className="field"><span>Channel</span>
                <select value={channel} onChange={e => setChannel(e.target.value as EconomyChannel)}>
                    {CHANNELS.map(c => <option key={c} value={c}>{c}</option>)}</select></label>}
            {kind === 'price' && <label className="field"><span>Good</span>
                <input value={item} maxLength={60} list="dm-steer-goods" placeholder="bread" onChange={e => setItem(e.target.value)} />
                <datalist id="dm-steer-goods">{goods.map(g => <option key={g} value={g} />)}</datalist></label>}
            <div className="row">
                <label className="field"><span>Strength (×{spec.min} to ×{spec.max})</span>
                    <input type="number" value={strength} min={spec.min} max={spec.max} step={spec.step} onChange={e => setStrength(Number(e.target.value))} /></label>
                <label className="field"><span>For how many days</span>
                    <input type="number" value={days} min={1} max={56} step={1} onChange={e => setDays(Math.round(Number(e.target.value)))} /></label>
            </div>
            <p className="hint">{spec.hint}</p>
            <label className="field"><span>Why (a note for the other Dungeon Masters)</span>
                <input value={note} maxLength={200} placeholder="A house saving for a war" onChange={e => setNote(e.target.value)} /></label>
            <button className={target === 'prod' ? 'publish' : 'primary'} disabled={!canAct || busy} onClick={() => void submit()}>
                {busy ? 'Queuing…' : 'Steer'}</button>
        </fieldset>
        {!canAct && <p className="hint">Viewers can't steer the orchestrator.</p>}
        {problem && <p className="hint error-text">{problem}</p>}
    </div>;
}
