// Atlas's Economy controls (Docs/Design/67-ridgemere2-city.md, 6; tools/studio.py /api/atlas/economy): the store room,
// a business's craft inputs and resale goods without end at the catalogue's price times a markup, on Atlas's world.
// The world's switches (making, vendors' restocking, the restocking rule) and markups, and each business's own switch:
// on, off, or every business shown at once. Per world: Update from live and Import from dev don't carry the world's
// switches (each world's own lever), only the businesses'. A change reaches a game server running the world at once
// (economy.sync), and any server started on it later; a business's own switch when a server next starts. The seasons and
// the comforts (doc 68, Progress) too, the loop's rules and the city's services (doc 68, "First fixes"), and the Country's
// food farms (doc 71, 4): the DM's Economy tab has the same settings.
import {useCallback, useEffect, useMemo, useState} from 'react';
import {api} from '../lib/api';
import type {Me} from '../dm/api';
import {ANY, bulk, choices, filtered, goodsText, markupOf, percent, switchText, wouldChange, type ComfortsBody,
    type EconomyControls as Controls, type FarmsBody, type Filter, type GoodsBody, type LoopSettings, type RulesBody, type SeasonsBody,
    type ServicesBody, type StoreSwitches} from '../lib/economy';
import {WharfPanel} from './WharfPanel';
import {FarmsPanel} from './FarmsPanel';
import {RulesPanel} from './RulesPanel';
import {SeasonsPanel} from './SeasonsPanel';

const KEPT = 'ratw.economy.filter';
const savedFilter = (): Filter => {
    try { return {business: ANY, district: ANY, ...JSON.parse(localStorage.getItem(KEPT) ?? '{}')}; } catch { return {business: ANY, district: ANY}; }
};

export function EconomyControls({me}: {me: Me}) {
    const [data, setData] = useState<Controls | null>(null);
    const [problem, setProblem] = useState(''), [said, setSaid] = useState('');
    const [busy, setBusy] = useState(false);
    const [filter, setFilter] = useState<Filter>(savedFilter);
    const [markups, setMarkups] = useState({storeMarkup: '', resaleMarkup: ''});
    const canChange = me.role === 'dm' || me.role === 'admin';
    const take = useCallback((next: Controls) => {
        setData(next);
        setMarkups({storeMarkup: String(next.storeMarkup), resaleMarkup: String(next.resaleMarkup)});
    }, []);
    const load = useCallback(async () => {
        try { take(await api.economy.read()); setProblem(''); } catch (error) { setProblem((error as Error).message); }
    }, [take]);
    useEffect(() => { void load(); }, [load]);
    useEffect(() => { try { localStorage.setItem(KEPT, JSON.stringify(filter)); } catch { /* private window */ } }, [filter]);
    const shown = useMemo(() => (data ? filtered(data.businesses, filter) : []), [data, filter]);
    const options = useMemo(() => (data ? choices(data.businesses) : {businesses: [], districts: []}), [data]);

    const change = async (what: () => Promise<Controls & {changed?: boolean; detail?: string; warnings?: string[]}>) => {
        setBusy(true); setProblem(''); setSaid('');
        try {
            const next = await what();
            take(next);
            setSaid(next.changed === false ? 'Nothing to change.' : `${next.detail ?? 'Changed.'} ${next.takesEffect}`);
            if (next.warnings?.length) setProblem(next.warnings.join(' '));
        } catch (error) { setProblem((error as Error).message); await load(); }
        finally { setBusy(false); }
    };
    const setStore = (changes: Partial<StoreSwitches>) => change(() => api.economy.setStore(changes));
    const setLoop = (changes: Partial<LoopSettings>) => void change(() => api.economy.setLoop(changes));
    const setGoods = (body: GoodsBody) => void change(() => api.economy.setGoods(body));
    const setSeasons = (body: SeasonsBody) => void change(() => api.economy.setSeasons(body));
    const setComforts = (body: ComfortsBody) => void change(() => api.economy.setComforts(body));
    const setRules = (body: RulesBody) => void change(() => api.economy.setRules(body));
    const setServices = (body: ServicesBody) => void change(() => api.economy.setServices(body));
    const setFarms = (body: FarmsBody) => void change(() => api.economy.setFarms(body));
    const setOne = (npc: string, on: boolean) => change(() => api.economy.setBusinesses({set: {[npc]: on}}));
    const setShown = (on: boolean) => {
        const n = wouldChange(shown, on);
        if (!n) { setSaid(`Every business shown is already ${on ? 'on' : 'off'}.`); return; }
        if (!window.confirm(`Switch the store room ${on ? 'on' : 'off'} for ${n} business${n === 1 ? '' : 'es'} shown?`)) return;
        void change(() => api.economy.setBusinesses(bulk(on, filter, shown)));
    };
    const commitMarkup = (key: 'storeMarkup' | 'resaleMarkup') => {
        if (!data) return;
        const value = markupOf(markups[key]);
        if (value === null) { setProblem('A markup is a whole number from 0 to 1000 (percent of the catalogue price).'); return; }
        if (value !== data[key]) void setStore({[key]: value});
    };

    if (!data) return <div className="dm-center"><p className="hint">{problem || 'Reading the economy controls…'}</p></div>;
    const locked = !canChange || !data.supported || busy;
    const counts = data.businesses.reduce((m, b) => ({...m, [b.effective]: (m[b.effective] ?? 0) + 1}), {} as Record<string, number>);

    return <div className="sync-page economy-page">
        <div className="dm-sheet-bar">
            <strong>Economy · the store room</strong>
            <span className="meta hint">{data.worldInfo?.name ?? data.world} ({data.worldInfo?.key ?? data.world}) · {data.businesses.length} businesses:
                {' '}{Object.entries(counts).map(([k, n]) => `${n} ${k}`).join(', ') || 'none'}</span>
            <button disabled={busy} onClick={() => void load()}>Read again</button>
        </div>
        <div className="sync-body">
            <p className="hint">Every shop keeper, vendor, smith and maker can draw from its store room: the inputs of its own crafts,
                and what it sells, without end. Not a place: the goods go into its till, and what they cost (the catalogue's price times
                the markup, Common quality) goes to the Country (doc 68), less the gate toll, the treasury's. {data.note}</p>
            {!data.supported && <p className="publish-error">{data.problem}</p>}
            {!canChange && <p className="hint">Your account ({me.role}) can look; changing these needs a dm or admin account.</p>}
            <section className="economy-switches">
                <label className="toggle-row"><input type="checkbox" checked={data.internalStore} disabled={locked}
                    onChange={e => void setStore({internalStore: e.target.checked})} /> {switchText('making', data.internalStore)}</label>
                <label className="economy-markup">Making markup
                    <input value={markups.storeMarkup} disabled={locked} inputMode="numeric" size={5}
                        onChange={e => setMarkups(m => ({...m, storeMarkup: e.target.value}))} onBlur={() => commitMarkup('storeMarkup')}
                        onKeyDown={e => e.key === 'Enter' && commitMarkup('storeMarkup')} />
                    <small>{percent(data.storeMarkup)} of the catalogue price (0: free)</small></label>
                <label className="toggle-row"><input type="checkbox" checked={data.storeResale} disabled={locked}
                    onChange={e => void setStore({storeResale: e.target.checked})} /> {switchText('resale', data.storeResale)}</label>
                <label className="economy-markup">Resale markup
                    <input value={markups.resaleMarkup} disabled={locked} inputMode="numeric" size={5}
                        onChange={e => setMarkups(m => ({...m, resaleMarkup: e.target.value}))} onBlur={() => commitMarkup('resaleMarkup')}
                        onKeyDown={e => e.key === 'Enter' && commitMarkup('resaleMarkup')} />
                    <small>{percent(data.resaleMarkup)} (under 100, so a vendor keeps a margin)</small></label>
                <label className="toggle-row"><input type="checkbox" checked={data.storeRule} disabled={locked}
                    onChange={e => {
                        if (e.target.checked && !window.confirm('Turn on the restocking rule? At work, once a game hour, each keeper brings its wares back to their shelves (a quarter more than sold yesterday): a maker makes batches from drawn inputs as its crew\'s hours allow, a vendor draws its goods (doc 68, 7).')) return;
                        void setStore({storeRule: e.target.checked});
                    }} /> {switchText('rule', data.storeRule)}</label>
                <p className="hint">{data.storeRule ? 'Keepers restock from the store room as they work.'
                    : 'With the rule off, no NPC uses the store room: only the Dev Console\'s /store does.'} {data.takesEffect}</p>
            </section>
            <WharfPanel data={data} locked={!canChange || busy} setLoop={setLoop} setGoods={setGoods} />
            <SeasonsPanel data={data} locked={!canChange || busy} setSeasons={setSeasons} setComforts={setComforts} />
            <RulesPanel data={data} locked={!canChange || busy} setRules={setRules} setServices={setServices} />
            <FarmsPanel data={data} locked={!canChange || busy} setFarms={setFarms} />
            <h3>The store room, business by business</h3>
            <div className="economy-filters">
                <label>Business <select value={filter.business} onChange={e => setFilter(f => ({...f, business: e.target.value}))}>
                    <option value={ANY}>every business</option>
                    {options.businesses.map(([b, n]) => <option key={b} value={b}>{b} ({n})</option>)}</select></label>
                <label>District <select value={filter.district} onChange={e => setFilter(f => ({...f, district: e.target.value}))}>
                    <option value={ANY}>every district</option>
                    {options.districts.map(([d, n]) => <option key={d} value={d}>{d || '(none)'} ({n})</option>)}</select></label>
                <span className="hint">{shown.length} shown</span>
                <button disabled={locked || !shown.length} onClick={() => setShown(true)}>All on</button>
                <button disabled={locked || !shown.length} onClick={() => setShown(false)}>All off</button>
            </div>
            {said && <p className="publish-done">{said}</p>}
            {problem && <p className="publish-error">{problem}</p>}
            <div className="dm-table-wrap"><table className="dm-table economy-table">
                <thead><tr><th>Keeper</th><th>Label</th><th>Business</th><th>District</th><th>Draws (each)</th><th>Store room</th></tr></thead>
                <tbody>{shown.map(b => <tr key={b.npc} className={b.effective === 'draws nothing' ? 'muted' : ''} title={b.why || undefined}>
                    <td>{b.name}<br /><small className="hint">{b.npc}</small></td>
                    <td>{b.label}<br /><small className="hint">{b.interior}</small></td>
                    <td>{b.business ?? <span className="hint">none</span>}</td>
                    <td>{b.district || <span className="hint">(none)</span>}</td>
                    <td className="economy-goods">{goodsText(b, data)}</td>
                    <td><label className="toggle-row"><input type="checkbox" checked={b.internalStore}
                        disabled={locked || b.effective === 'draws nothing'} onChange={e => void setOne(b.npc, e.target.checked)} />
                        {' '}{b.effective}</label></td>
                </tr>)}</tbody>
            </table></div>
            {!shown.length && <p className="hint">No placed merchant's business here{filter.business || filter.district ? ' under this filter' : ''}.</p>}
        </div>
    </div>;
}
