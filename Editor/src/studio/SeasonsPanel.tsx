// The seasons (Docs/Design/79-a-staged-economy.md, 7.1 and 12): each season group of Data/Economy/market.json (grain,
// fresh fruit, fresh fish, ...) with this world's own numbers: by season, what its goods cost (on the Market Board's
// prices) and how full the shops keep their shelves of them. A world's own prices are held to the season room (players'
// pressure keeps its room inside the price clamp). On Atlas's Economy page and the DM's Economy tab alike.
import {useEffect, useState} from 'react';
import {percentsOf, percentsText, SEASON_ROWS, seasonChange, seasonNow, type EconomyData, type SeasonRow,
    type SeasonsBody, type SeasonGroupView} from '../lib/economy';

type Typed = Record<string, string>;
const cellKey = (group: string, key: string) => `${group}.${key}`;

export function SeasonsPanel({data, locked, setSeasons}: {data: EconomyData; locked: boolean; setSeasons: (body: SeasonsBody) => void}) {
    const [typed, setTyped] = useState<Typed>({});
    useEffect(() => {
        const out: Typed = {};
        for (const g of data.seasonGroups) {
            const now = seasonNow(data, g);
            for (const r of SEASON_ROWS) out[cellKey(g.id, r.key)] = percentsText(now[r.key]);
        }
        setTyped(out);
    }, [data]);
    const off = locked || !data.supported;
    const names = data.seasonNames?.length === 4 ? data.seasonNames : ['Spring', 'Summer', 'Autumn', 'Winter'];

    const commitRow = (g: SeasonGroupView, key: SeasonRow, low: number, high: number) => {
        const value = percentsOf(typed[cellKey(g.id, key)] ?? '', low, high);
        if (value === null) { window.alert(`Four whole percents (${names.join(', ')}), each from ${low} to ${high}.`); return; }
        const body = seasonChange(data, g, key, value);
        if (body) setSeasons(body);
    };

    return <section className="economy-switches seasons-panel">
        <h3>The seasons</h3>
        <p className="hint">Prices and shelves follow the year: each group's numbers stand at the middle of each season
            ({names.join(', ')}), the days between blending in a straight line. Price moves the Market Board's prices of the
            group's goods, so what is in season is cheap (a wide swing is narrowed to market.json's season span, and the
            prices must leave players' buying and selling room inside the price clamp); shelf is how full the shops keep
            their shelves of them (the preserver stocks up in autumn for the winter). The groups and their goods are
            Data/Economy/market.json's.</p>
        {!data.supported && <p className="publish-error">{data.problem}</p>}
        <div className="dm-table-wrap"><table className="dm-table seasons-table">
            <thead><tr><th>Group</th><th></th><th>{names.join(' / ')} (%)</th><th></th></tr></thead>
            <tbody>{data.seasonGroups.map(g => {
                const now = seasonNow(data, g);
                return SEASON_ROWS.map((r, i) => <tr key={`${g.id}.${r.key}`} className={now.changed ? 'changed' : ''}>
                    {i === 0 && <td rowSpan={SEASON_ROWS.length} title={g.about}>
                        <b>{g.name}</b><br /><small className="hint">{g.goods.join(', ')}</small>
                        {now.changed && <><br /><small className="hint">changed here (market.json: price {percentsText(g.price)},
                            shelf {percentsText(g.shelf)})</small></>}</td>}
                    <td title={r.hint}>{r.label}</td>
                    <td><input value={typed[cellKey(g.id, r.key)] ?? ''} disabled={off} size={18} aria-label={`${g.name}: ${r.label}`}
                        onChange={e => setTyped(t => ({...t, [cellKey(g.id, r.key)]: e.target.value}))}
                        onBlur={() => commitRow(g, r.key, r.low, r.high)}
                        onKeyDown={e => e.key === 'Enter' && commitRow(g, r.key, r.low, r.high)} /></td>
                    {i === 0 && <td rowSpan={SEASON_ROWS.length}>{now.changed &&
                        <button disabled={off} onClick={() => setSeasons({set: {[g.id]: null}})}>As market.json</button>}</td>}
                </tr>);
            })}</tbody>
        </table></div>
    </section>;
}
