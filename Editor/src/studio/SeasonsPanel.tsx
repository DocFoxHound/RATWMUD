// The seasons in the Country's supply and the residents' comforts (Docs/Design/68-the-country-and-the-city-loop.md,
// Progress, "Seasons and the DM's rules"): each season group of Data/Items/city_loop.json (grain, fresh fruit, fresh fish,
// ...) with this world's own numbers: by season, how much the Country can send a day, what its goods cost, how full the
// shops keep their shelves of them, and its normal pieces a day; and the comforts' threshold and share. On Atlas's
// Economy page and the DM's Economy tab alike.
import {useEffect, useState} from 'react';
import {comfortOf, comfortsText, percentsOf, percentsText, SEASON_ROWS, seasonChange, wholeOf, type ComfortsBody,
    type SeasonControls, type SeasonGroup, type SeasonRow, type SeasonsBody} from '../lib/economy';

type Typed = Record<string, string>;
const cellKey = (group: string, key: string) => `${group}.${key}`;

function typedOf(groups: SeasonGroup[]): Typed {
    const out: Typed = {};
    for (const g of groups) {
        for (const r of SEASON_ROWS) out[cellKey(g.id, r.key)] = percentsText(g[r.key]);
        out[cellKey(g.id, 'normal')] = String(g.normal);
    }
    return out;
}

export function SeasonsPanel({data, locked, setSeasons, setComforts}: {data: SeasonControls; locked: boolean;
    setSeasons: (body: SeasonsBody) => void; setComforts: (body: ComfortsBody) => void}) {
    const [typed, setTyped] = useState<Typed>({});
    const [comfort, setComfort] = useState({above: '', share: ''});
    useEffect(() => setTyped(typedOf(data.seasons ?? [])), [data.seasons]);
    useEffect(() => setComfort({above: data.comforts?.comfortsAbove === null || data.comforts?.comfortsAbove === undefined ? '' : String(data.comforts.comfortsAbove),
        share: data.comforts?.comfortsShare === null || data.comforts?.comfortsShare === undefined ? '' : String(data.comforts.comfortsShare)}), [data.comforts]);
    if (!data.seasons) return null;
    const off = locked || !data.seasonsSupported;
    const names = data.seasonNames?.length === 4 ? data.seasonNames : ['Spring', 'Summer', 'Autumn', 'Winter'];

    const commitRow = (g: SeasonGroup, key: SeasonRow, low: number, high: number) => {
        const value = percentsOf(typed[cellKey(g.id, key)] ?? '', low, high);
        if (value === null) { window.alert(`Four whole percents (${names.join(', ')}), each from ${low} to ${high}.`); return; }
        const body = seasonChange(g, key, value);
        if (body) setSeasons(body);
    };
    const commitNormal = (g: SeasonGroup) => {
        const value = wholeOf(typed[cellKey(g.id, 'normal')] ?? '', 1000000);
        if (value === null) { window.alert('A whole number of pieces a day (0: no cap).'); return; }
        const body = seasonChange(g, 'normal', value);
        if (body) setSeasons(body);
    };
    const commitComfort = (key: 'above' | 'share') => {
        const max = key === 'above' ? 1000000 : 100;
        const value = comfortOf(comfort[key], max);
        if (value === undefined) { window.alert(`A whole number from 0 to ${max}, or empty for city_loop.json's.`); return; }
        const field = key === 'above' ? 'comfortsAbove' : 'comfortsShare';
        if (value !== data.comforts[field]) setComforts({[field]: value});
    };

    return <section className="economy-switches seasons-panel">
        <h3>The seasons</h3>
        <p className="hint">The Country's supply follows the year: each group's numbers stand at the middle of each season
            ({names.join(', ')}), the days between blending in a straight line. Supply caps what the shops may draw of the group
            a day ({'normal × supply'}; a normal of 0 is no cap); price sets both what the shops pay the store room and what
            residents pay the shops, and residents weigh nourishment for the price, so they buy what is in season; shelf is
            how full the shops keep their shelves of the group's goods (the preserver stocks up in autumn for the winter).
            The groups and their goods are Data/Items/city_loop.json's.</p>
        {!data.seasonsSupported && <p className="publish-error">{data.seasonsProblem}</p>}
        <div className="dm-table-wrap"><table className="dm-table seasons-table">
            <thead><tr><th>Group</th><th></th><th>{names.join(' / ')} (%)</th><th>Normal a day</th><th></th></tr></thead>
            <tbody>{data.seasons.map(g => SEASON_ROWS.map((r, i) => <tr key={`${g.id}.${r.key}`} className={g.changed ? 'changed' : ''}>
                {i === 0 && <td rowSpan={SEASON_ROWS.length} title={g.about}>
                    <b>{g.name}</b><br /><small className="hint">{g.goods.join(', ')}</small>
                    {g.changed && <><br /><small className="hint">changed here (city_loop.json: supply {percentsText(g.file.supply)},
                        price {percentsText(g.file.price)}, shelf {percentsText(g.file.shelf)}, normal {g.file.normal})</small></>}</td>}
                <td title={r.hint}>{r.label}</td>
                <td><input value={typed[cellKey(g.id, r.key)] ?? ''} disabled={off} size={18} aria-label={`${g.name}: ${r.label}`}
                    onChange={e => setTyped(t => ({...t, [cellKey(g.id, r.key)]: e.target.value}))}
                    onBlur={() => commitRow(g, r.key, r.low, r.high)}
                    onKeyDown={e => e.key === 'Enter' && commitRow(g, r.key, r.low, r.high)} /></td>
                {i === 0 && <td rowSpan={SEASON_ROWS.length}><input value={typed[cellKey(g.id, 'normal')] ?? ''} disabled={off} size={7}
                    inputMode="numeric" aria-label={`${g.name}: normal pieces a day`}
                    onChange={e => setTyped(t => ({...t, [cellKey(g.id, 'normal')]: e.target.value}))}
                    onBlur={() => commitNormal(g)} onKeyDown={e => e.key === 'Enter' && commitNormal(g)} />
                    <br /><small className="hint">{g.normal ? 'pieces' : 'no cap'}</small></td>}
                {i === 0 && <td rowSpan={SEASON_ROWS.length}>{g.changed &&
                    <button disabled={off} onClick={() => setSeasons({set: {[g.id]: null}})}>As city_loop.json</button>}</td>}
            </tr>))}</tbody>
        </table></div>
        <h3>Comforts</h3>
        <p className="hint">{data.comforts && comfortsText(data.comforts)} The loop's one rule against hoarding: savings come back
            to the shops. Empty: city_loop.json's.</p>
        <label className="economy-markup">Above
            <input value={comfort.above} disabled={off} inputMode="numeric" size={6} placeholder={String(data.comforts?.fileAbove ?? '')}
                onChange={e => setComfort(c => ({...c, above: e.target.value}))} onBlur={() => commitComfort('above')}
                onKeyDown={e => e.key === 'Enter' && commitComfort('above')} />
            <small>p a head held before a household buys comforts</small></label>
        <label className="economy-markup">Share
            <input value={comfort.share} disabled={off} inputMode="numeric" size={6} placeholder={String(data.comforts?.fileShare ?? '')}
                onChange={e => setComfort(c => ({...c, share: e.target.value}))} onBlur={() => commitComfort('share')}
                onKeyDown={e => e.key === 'Enter' && commitComfort('share')} />
            <small>% of what is over that spent a day</small></label>
    </section>;
}
