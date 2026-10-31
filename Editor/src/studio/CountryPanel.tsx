// Atlas's Economy page, the city's loop and the Country (Docs/Design/68-the-country-and-the-city-loop.md, 9): the
// residents' needs, the wages, the gate toll (the city's tax), carriage and the treasury's subsidy of it, the Country's
// switch and reserve, its basket (Data/Items/country.json) and which goods it supplies. Each world's own, in live.economy;
// sent to a server running the world at once (economy.sync), and read by any started later. On Atlas's Economy page and
// the DM's Economy tab alike.
import {useEffect, useMemo, useState} from 'react';
import {basketRows, goodsBulk, goodsShown, LOOP_NUMBERS, wholeOf, type EconomyControls, type GoodsBody,
    type LoopSettings} from '../lib/economy';

type Change = (changes: Partial<LoopSettings>) => void;

export function CountryPanel({data, locked, setLoop, setGoods}: {data: EconomyControls; locked: boolean; setLoop: Change;
    setGoods: (body: GoodsBody) => void}) {
    const loop = data.loop;
    const [typed, setTyped] = useState<Record<string, string>>({});
    const [search, setSearch] = useState('');
    useEffect(() => {
        setTyped({...Object.fromEntries(LOOP_NUMBERS.map(n => [n.key, String(loop[n.key])])),
            subsidy: String(loop.subsidies?.carriage ?? 0)});
    }, [loop]);
    const off = !data.loopSupported || locked;
    const commit = (key: keyof LoopSettings, max: number) => {
        const value = wholeOf(typed[key] ?? '', max);
        if (value === null) { window.alert(`A whole number from 0 to ${max}.`); return; }
        if (value !== loop[key]) setLoop({[key]: value});
    };
    const commitSubsidy = () => {
        const value = wholeOf(typed.subsidy ?? '', 100);
        if (value === null) { window.alert('A share from 0 to 100 percent.'); return; }
        if (value !== (loop.subsidies?.carriage ?? 0)) setLoop({subsidies: {...loop.subsidies, carriage: value}});
    };
    const {rows, unmade} = useMemo(() => basketRows(data.basket), [data.basket]);
    const goods = useMemo(() => goodsShown(data.goods, search), [data.goods, search]);
    const supplied = data.goods.filter(g => g.supplied).length;

    return <section className="economy-switches country-panel">
        <h3>The city's loop and {data.market.name}</h3>
        <p className="hint">Residents earn wages, buy their food and household goods; shops restock from the store room and pay
            {' '}{data.market.name} for it (less the gate toll, the treasury's); {data.market.name} spends what it takes in on the
            city's goods, keeping its reserve; carriage on every delivery goes to the carters, the treasury paying its share.
            Money is only moved, never made. {data.takesEffect}</p>
        {!data.loopSupported && <p className="publish-error">{data.loopProblem}</p>}
        {data.ruleWarning && <p className="publish-error">{data.ruleWarning}</p>}
        <label className="toggle-row"><input type="checkbox" checked={!loop.needsPaused} disabled={off}
            onChange={e => setLoop({needsPaused: !e.target.checked})} /> Residents' needs: {loop.needsPaused
                ? 'paused (nobody grows hungry, buys, or is paid)' : 'on (they eat, buy and are paid)'}</label>
        <label className="toggle-row"><input type="checkbox" checked={loop.country} disabled={off}
            onChange={e => setLoop({country: e.target.checked})} /> {data.market.name} spends back what it takes in:
            {' '}{loop.country ? 'on' : 'off (it only takes in)'}</label>
        {LOOP_NUMBERS.map(n => <label key={n.key} className="economy-markup">{n.label}
            <input value={typed[n.key] ?? ''} disabled={off} inputMode="numeric" size={6}
                onChange={e => setTyped(t => ({...t, [n.key]: e.target.value}))} onBlur={() => commit(n.key, n.max)}
                onKeyDown={e => e.key === 'Enter' && commit(n.key, n.max)} />
            <small>{n.unit}</small></label>)}
        <label className="economy-markup">Carriage subsidy
            <input value={typed.subsidy ?? ''} disabled={off} inputMode="numeric" size={6}
                onChange={e => setTyped(t => ({...t, subsidy: e.target.value}))} onBlur={commitSubsidy}
                onKeyDown={e => e.key === 'Enter' && commitSubsidy()} />
            <small>% of every delivery's carriage the treasury pays the carters (0: off). Only from what it holds over its
                float and tonight's public wages, at most a twentieth of its money a day: the subsidy shrinks before wages do.</small></label>

        <details className="country-basket">
            <summary>{data.market.name}'s basket: {rows.length} goods{unmade ? `, ${unmade} with no maker here` : ''}</summary>
            <p className="hint">What its people buy in the city, as shares of its day's spending (Data/Items/country.json;
                tools/city_loop.py --suggest spreads it over the trades with time to spare). {data.market.people}</p>
            <div className="dm-table-wrap"><table className="dm-table">
                <thead><tr><th>Good</th><th>Share</th><th>Price</th><th>Made here by</th></tr></thead>
                <tbody>{rows.map(b => <tr key={b.item} className={b.makers.length ? '' : 'muted'}>
                    <td>{b.name}<br /><small className="hint">{b.item}</small></td><td className="num">{b.percent}%</td>
                    <td className="num">{b.price}p</td><td>{b.makers.join(', ') || <span className="hint">nobody</span>}</td></tr>)}</tbody>
            </table></div>
        </details>

        <details className="country-goods">
            <summary>Goods {data.market.name} supplies: {supplied} of {data.goods.length}</summary>
            <p className="hint">A good switched off is the city's own producers' (a farm or a mine added later): the store room
                refuses it, for making and resale alike.</p>
            <div className="economy-filters">
                <label>Find <input value={search} onChange={e => setSearch(e.target.value)} size={14} /></label>
                <span className="hint">{goods.length} shown</span>
                <button disabled={off || !goods.length} onClick={() => setGoods(goodsBulk(true, goods, data.goods))}>All on</button>
                <button disabled={off || !goods.length} onClick={() => {
                    if (window.confirm(`Stop ${data.market.name} supplying ${goods.length} good${goods.length === 1 ? '' : 's'}? `
                        + 'Shops that draw them will run out.')) setGoods(goodsBulk(false, goods, data.goods));
                }}>All off</button>
            </div>
            <div className="dm-table-wrap"><table className="dm-table">
                <thead><tr><th>Good</th><th>Businesses drawing it</th><th>Supplied by {data.market.name}</th></tr></thead>
                <tbody>{goods.map(g => <tr key={g.item}>
                    <td>{g.name}<br /><small className="hint">{g.item}</small></td><td className="num">{g.businesses}</td>
                    <td><label className="toggle-row"><input type="checkbox" checked={g.supplied} disabled={off}
                        onChange={e => setGoods({set: {[g.item]: e.target.checked}})} /> {g.supplied ? 'yes' : 'no: its producers\''}</label></td>
                </tr>)}</tbody>
            </table></div>
        </details>
    </section>;
}
