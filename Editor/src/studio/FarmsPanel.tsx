// The Country's food farms (Docs/Design/71-the-country-s-industries.md, 4; migration 0060): the switch (an area at a
// time from its first day, so many days apart, in its order), the farms' wages, and each food area's switch, yields' and
// hands' percents, wages and price a year, with its farms' own (their crews, wages, floats, crews' purses and yields as
// the game runs them, and, from the world's last save, their tills, stores and crews' purses). Each the world's own or
// Data/Economy/country_farms.json's. On Atlas's Economy page and the DM's Economy tab alike. The works of doc 72 (the
// woods', the peat's) are areas too, beside the farms, edited the same way: they pay their crews and their rent to their
// House's estate.
import {useEffect, useState} from 'react';
import {areaOnFrom, areaSwitch, farmChange, farmNow, farmSchedule, scheduleText, wholeOf, yieldText, type FarmAreaDefault,
    type FarmControls, type FarmsBody} from '../lib/economy';

type Typed = Record<string, string>;
// A House as the panel names it: "House Fell", but "the Crown" (a food area's land no House holds: doc 72).
const houseName = (h: string) => h.startsWith('the ') ? h : `House ${h}`;

export function FarmsPanel({data, locked, setFarms}: {data: FarmControls; locked: boolean; setFarms: (body: FarmsBody) => void}) {
    const [typed, setTyped] = useState<Typed>({});
    useEffect(() => {
        const f = data.farms ?? {}, out: Typed = {};
        const put = (key: string, v: number | undefined) => { out[key] = v === undefined ? '' : String(v); };
        put('all.start', f.start); put('all.every', f.every); put('all.keeper', f.keeper); put('all.hand', f.hand);
        out['all.order'] = f.order ? f.order.join(', ') : '';
        for (const a of data.farmDefaults?.areas ?? []) {
            const own = f.areas?.[a.id] ?? {};
            for (const k of ['yieldScale', 'crewScale', 'keeper', 'hand', 'priceFactor'] as const) put(`areas.${a.id}.${k}`, own[k]);
            for (const x of a.farms) {
                const mine = f.farms?.[x.id] ?? {};
                for (const k of ['yieldScale', 'crewScale', 'keeper', 'hand'] as const) put(`farms.${x.id}.${k}`, mine[k]);
            }
        }
        setTyped(out);
    }, [data]);
    if (!data.farmDefaults) return null;
    const off = locked || !data.farmsSupported;
    const d = data.farmDefaults;
    const {start} = farmSchedule(data);

    // A number typed in: empty is country_farms.json's (null), else a whole number within low..high.
    const commit = (where: 'all' | 'areas' | 'farms', id: string, key: string, low: number, high: number) => {
        const text = typed[where === 'all' ? `all.${key}` : `${where}.${id}.${key}`] ?? '';
        let value: number | null = null;
        if (text.trim() !== '') {
            const n = text.trim() === '-1' && low < 0 ? -1 : wholeOf(text, high);
            if (n === null || n < low) { window.alert(`A whole number from ${low} to ${high}, or empty for country_farms.json's.`); return; }
            value = n;
        }
        const body = farmChange(data, where, id, key, value);
        if (body) setFarms(body);
    };
    const commitOrder = () => {
        const text = (typed['all.order'] ?? '').trim();
        const order = text ? text.split(/[\s,]+/).filter(Boolean) : null;
        const known = d.areas.map(a => a.id);
        if (order && (order.some(a => !known.includes(a)) || new Set(order).size !== order.length)) {
            window.alert(`Distinct food areas, in the order they go on: ${known.join(', ')}; or empty for country_farms.json's.`);
            return;
        }
        const body = farmChange(data, 'all', '', 'order', order);
        if (body) setFarms(body);
    };
    const field = (key: string, size: number, label: string, done: () => void, placeholder = '') =>
        <input value={typed[key] ?? ''} disabled={off} size={size} aria-label={label} placeholder={placeholder}
            onChange={e => setTyped(t => ({...t, [key]: e.target.value}))} onBlur={done} onKeyDown={e => e.key === 'Enter' && done()} />;
    const setSwitch = (a: FarmAreaDefault, value: string) => {
        const on = value === 'on' ? true : value === 'off' ? false : null;
        const body = farmChange(data, 'areas', a.id, 'on', on);
        if (body) setFarms(body);
    };

    return <section className="economy-switches farms-panel">
        <h3>The Country's farms and works</h3>
        <p className="hint">Modelled farms with virtual crews (no NPC is placed yet: doc 71). An area switched on takes its goods
            from the Country: the shops draw them from its farms' stores (the toll and carriage as before), as much as the stores
            hold, and the Country no longer sends them. Its farms are founded the first time it goes on (their floats and their
            crews' purses made, as the world's founding money was). Each day a farm grows its yields by season, the Country's own
            people eat at its table and what is past keeping spoils; at 18:00 it pays its crew, who shop in the city as a
            household; what it holds over its float and its wages it spends on the city's goods, as the Country does. A works
            (doc 72: the woods, the peat) is the same but for that: what it holds over its float after the wages is its rent,
            sent each night to the House whose land it is on (the Country's purse, booked by House). A food farm pays that House
            a land rent too, a percent of its takings after its wages, from its basket's share (the rules' land rent: doc 72). Empty:
            country_farms.json's (shown faintly).</p>
        {!data.farmsSupported && <p className="publish-error">{data.farmsProblem}</p>}
        <p><b>{scheduleText(data)}</b></p>
        <label className="economy-markup">First area on day
            {field('all.start', 9, 'The first area on day', () => commit('all', '', 'start', -1, 365000000), String(d.start))}
            <small>of the world's calendar (-1: no schedule)</small></label>
        <label className="economy-markup">One every
            {field('all.every', 5, 'Days between areas', () => commit('all', '', 'every', 1, 3650), String(d.every))}
            <small>days</small></label>
        <label className="economy-markup">In the order
            {field('all.order', 40, 'The areas in order', commitOrder, d.order.join(', '))}</label>
        <label className="economy-markup">A keeper's wage
            {field('all.keeper', 5, 'A keeper\'s wage', () => commit('all', '', 'keeper', 0, 1000), String(d.keeper))}
            <small>p a day; a hand's</small>
            {field('all.hand', 5, 'A hand\'s wage', () => commit('all', '', 'hand', 0, 1000), String(d.hand))}
            <small>p (fixed: the basket takes what is over them)</small></label>
        {d.areas.map(a => {
            const sw = areaSwitch(data, a), from = areaOnFrom(data, a.id);
            const own = data.farms?.areas?.[a.id];
            return <details key={a.id} className={own ? 'changed' : ''}>
                <summary><b>{a.name}</b> · {sw === 'schedule' ? (from === null ? 'off (no schedule)' : `on from day ${from}`) : sw}
                    {' '}· {a.farms.length} {a.works ? 'works' : `farm${a.farms.length === 1 ? '' : 's'}`}
                    {a.house ? ` · ${a.works ? 'rent' : 'land rent'} to ${houseName(a.house)}` : ''}</summary>
                <p className="hint">Its goods: {a.goods.join(', ')}. Design takings {a.revenueADay.map(v => Math.round(v)).join(' / ')}p a day
                    (Spring / Summer / Autumn / Winter).</p>
                <label className="economy-markup">Switch <select value={sw} disabled={off} aria-label={`${a.name}: switch`}
                    onChange={e => setSwitch(a, e.target.value)}>
                    <option value="schedule">as the schedule says{start >= 0 && from !== null ? ` (day ${from})` : ''}</option>
                    <option value="on">on</option><option value="off">off</option></select></label>
                <label className="economy-markup">Yields
                    {field(`areas.${a.id}.yieldScale`, 5, `${a.name}: yields`, () => commit('areas', a.id, 'yieldScale', 0, 1000), String(a.yieldScale))}
                    <small>% on all its farms' yields; hands</small>
                    {field(`areas.${a.id}.crewScale`, 5, `${a.name}: hands`, () => commit('areas', a.id, 'crewScale', 0, 1000), String(a.crewScale))}
                    <small>% (rounded half up)</small></label>
                <label className="economy-markup">Wages
                    {field(`areas.${a.id}.keeper`, 5, `${a.name}: keeper's wage`, () => commit('areas', a.id, 'keeper', 0, 1000), String(a.keeper ?? d.keeper))}
                    <small>p a keeper,</small>
                    {field(`areas.${a.id}.hand`, 5, `${a.name}: hand's wage`, () => commit('areas', a.id, 'hand', 0, 1000), String(a.hand ?? d.hand))}
                    <small>p a hand</small></label>
                {a.price === 'year' && <label className="economy-markup">Price a year
                    {field(`areas.${a.id}.priceFactor`, 5, `${a.name}: price a year`, () => commit('areas', a.id, 'priceFactor', 10, 1000), String(a.priceFactor ?? 100))}
                    <small>% of the catalogue's, all year (its store)</small></label>}
                <div className="dm-table-wrap"><table className="dm-table farms-table">
                    <thead><tr><th>{a.works ? 'Works' : 'Farm'}</th><th>Crew, wages</th><th>Yields %</th><th>Hands %</th><th>Keeper p</th><th>Hand p</th>
                        <th>Float, purse</th><th>Yields a day (Sp/Su/Au/Wi)</th></tr></thead>
                    <tbody>{a.farms.map(x => {
                        const now = farmNow(data, a, x), mine = data.farms?.farms?.[x.id];
                        return <tr key={x.id} className={mine ? 'changed' : ''}>
                            <td title={x.region}><b>{x.id}</b><br /><small className="hint">{x.kind}{x.house ? `, ${houseName(x.house)}'s land` : ''}</small></td>
                            <td>{now.keepers} + {now.hands}, {now.wages}p a day</td>
                            <td>{field(`farms.${x.id}.yieldScale`, 4, `${x.id}: yields`, () => commit('farms', x.id, 'yieldScale', 0, 1000), '100')}</td>
                            <td>{field(`farms.${x.id}.crewScale`, 4, `${x.id}: hands`, () => commit('farms', x.id, 'crewScale', 0, 1000), '100')}</td>
                            <td>{field(`farms.${x.id}.keeper`, 4, `${x.id}: keeper's wage`, () => commit('farms', x.id, 'keeper', 0, 1000), String(now.keeper))}</td>
                            <td>{field(`farms.${x.id}.hand`, 4, `${x.id}: hand's wage`, () => commit('farms', x.id, 'hand', 0, 1000), String(now.hand))}</td>
                            <td>{now.reserve}p, {now.purse}p</td>
                            <td><small>{yieldText(x, now.yieldPercent)}</small></td>
                        </tr>;
                    })}</tbody>
                </table></div>
                {own && <button disabled={off} onClick={() => setFarms({set: {areas: {[a.id]: null}}})}>Area as country_farms.json</button>}
            </details>;
        })}
    </section>;
}
