// The DM's Economy tab (Docs/Design/68-the-country-and-the-city-loop.md, 9 and Progress, "Seasons and the DM's rules"):
// every rule of the city's loop and the store room on the DM's world, the same settings as Atlas's Economy page: the
// store room's switches and markups, the residents' needs, the wages, the gate toll, carriage and its subsidy, the
// Country's switch, reserve and goods, the seasons in its supply and the comforts, the loop's rules and the city's
// services (doc 68, Progress, "First fixes"), the Country's food farms (doc 71, 4). Each change is audited in the world's
// DM audit and sent to its running game server (economy.sync), which applies it at once; a server started later reads it
// too. A business's own store-room switch stays Atlas's (it travels with the world's businesses).
import {useCallback, useEffect, useState} from 'react';
import {dmApi, type Me, type Target} from './api';
import {markupOf, percent, switchText, type ComfortsBody, type EconomyChange, type EconomyControls, type FarmsBody, type GoodsBody,
    type LoopSettings, type RulesBody, type SeasonsBody, type ServicesBody, type StoreSwitches} from '../lib/economy';
import {WharfPanel} from '../studio/WharfPanel';
import {FarmsPanel} from '../studio/FarmsPanel';
import {RulesPanel} from '../studio/RulesPanel';
import {SeasonsPanel} from '../studio/SeasonsPanel';

export function EconomyTab({me, target}: {me: Me; target: Target}) {
    const [data, setData] = useState<EconomyControls | null>(null);
    const [problem, setProblem] = useState(''), [said, setSaid] = useState('');
    const [busy, setBusy] = useState(false);
    const [markups, setMarkups] = useState({storeMarkup: '', resaleMarkup: ''});
    const canChange = me.role === 'dm' || me.role === 'admin';
    const take = useCallback((next: EconomyControls) => {
        setData(next);
        setMarkups({storeMarkup: String(next.storeMarkup), resaleMarkup: String(next.resaleMarkup)});
    }, []);
    const load = useCallback(async () => {
        try { take(await dmApi.economy.read(target)); setProblem(''); } catch (error) { setProblem((error as Error).message); }
    }, [take, target]);
    useEffect(() => { void load(); }, [load]);

    const change = async (what: () => Promise<EconomyChange>) => {
        setBusy(true); setProblem(''); setSaid('');
        try {
            const next = await what();
            take(next);
            setSaid(next.changed === false ? 'Nothing to change.' : `${next.detail ?? 'Changed.'} ${next.takesEffect}`);
            if (next.warnings?.length) setProblem(next.warnings.join(' '));
        } catch (error) { setProblem((error as Error).message); await load(); }
        finally { setBusy(false); }
    };
    const setStore = (changes: Partial<StoreSwitches>) => void change(() => dmApi.economy.setStore(target, changes));
    const setLoop = (changes: Partial<LoopSettings>) => void change(() => dmApi.economy.setLoop(target, changes));
    const setGoods = (body: GoodsBody) => void change(() => dmApi.economy.setGoods(target, body));
    const setSeasons = (body: SeasonsBody) => void change(() => dmApi.economy.setSeasons(target, body));
    const setComforts = (body: ComfortsBody) => void change(() => dmApi.economy.setComforts(target, body));
    const setRules = (body: RulesBody) => void change(() => dmApi.economy.setRules(target, body));
    const setServices = (body: ServicesBody) => void change(() => dmApi.economy.setServices(target, body));
    const setFarms = (body: FarmsBody) => void change(() => dmApi.economy.setFarms(target, body));
    const commitMarkup = (key: 'storeMarkup' | 'resaleMarkup') => {
        if (!data) return;
        const value = markupOf(markups[key]);
        if (value === null) { setProblem('A markup is a whole number from 0 to 1000 (percent of the catalogue price).'); return; }
        if (value !== data[key]) setStore({[key]: value});
    };

    if (!data) return <div className="dm-center"><p className="hint">{problem || 'Reading the economy\'s rules…'}</p></div>;
    const locked = !canChange || busy;
    const storeLocked = locked || !data.supported;
    return <div className="sync-page economy-page dm-economy">
        <div className="dm-sheet-bar">
            <strong>Economy · {data.worldInfo?.name ?? target}</strong>
            <span className="meta hint">the rules of the city's loop and the store room ({data.worldInfo?.key ?? target})</span>
            <button disabled={busy} onClick={() => void load()}>Read again</button>
        </div>
        <div className="sync-body">
            <p className="hint">Each change is the world's own (live.economy), audited, and sent to its running game server, which
                applies it at once. {data.takesEffect} {data.note}</p>
            {!canChange && <p className="hint">Your account ({me.role}) can look; changing these needs a dm or admin account.</p>}
            {said && <p className="publish-done">{said}</p>}
            {problem && <p className="publish-error">{problem}</p>}
            <section className="economy-switches">
                <h3>The store room</h3>
                {!data.supported && <p className="publish-error">{data.problem}</p>}
                <label className="toggle-row"><input type="checkbox" checked={data.internalStore} disabled={storeLocked}
                    onChange={e => setStore({internalStore: e.target.checked})} /> {switchText('making', data.internalStore)}</label>
                <label className="economy-markup">Making markup
                    <input value={markups.storeMarkup} disabled={storeLocked} inputMode="numeric" size={5}
                        onChange={e => setMarkups(m => ({...m, storeMarkup: e.target.value}))} onBlur={() => commitMarkup('storeMarkup')}
                        onKeyDown={e => e.key === 'Enter' && commitMarkup('storeMarkup')} />
                    <small>{percent(data.storeMarkup)} of the catalogue price (0: free)</small></label>
                <label className="toggle-row"><input type="checkbox" checked={data.storeResale} disabled={storeLocked}
                    onChange={e => setStore({storeResale: e.target.checked})} /> {switchText('resale', data.storeResale)}</label>
                <label className="economy-markup">Resale markup
                    <input value={markups.resaleMarkup} disabled={storeLocked} inputMode="numeric" size={5}
                        onChange={e => setMarkups(m => ({...m, resaleMarkup: e.target.value}))} onBlur={() => commitMarkup('resaleMarkup')}
                        onKeyDown={e => e.key === 'Enter' && commitMarkup('resaleMarkup')} />
                    <small>{percent(data.resaleMarkup)} (under 100, so a vendor keeps a margin)</small></label>
                <label className="toggle-row"><input type="checkbox" checked={data.storeRule} disabled={storeLocked}
                    onChange={e => {
                        if (!e.target.checked && !window.confirm('Turn the restocking rule off? Shops then sell out and stay empty.')) return;
                        setStore({storeRule: e.target.checked});
                    }} /> {switchText('rule', data.storeRule)}</label>
                <p className="hint">A business's own switch is Atlas's (its Economy page), with the world's businesses.</p>
            </section>
            <WharfPanel data={data} locked={locked} setLoop={setLoop} setGoods={setGoods} />
            <SeasonsPanel data={data} locked={locked} setSeasons={setSeasons} setComforts={setComforts} />
            <RulesPanel data={data} locked={locked} setRules={setRules} setServices={setServices} />
            <FarmsPanel data={data} locked={locked} setFarms={setFarms} />
        </div>
    </div>;
}
