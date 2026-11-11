// The goods ships never carry (Docs/Design/79-a-staged-economy.md, 12: the old wharfOff became neverImported; Atlas's
// Wharf panel retargeted): over the catalogue's goods, the world's own list in place of market.json's wharf.never. Kept
// out of every ship's manifest. Migration 0061 only. On Atlas's Economy page and the DM's Economy tab alike.
import {useMemo, useState} from 'react';
import {goodsShown, neverChange, neverNow, type EconomyData, type ImportsBody} from '../lib/economy';

export function ImportsPanel({data, locked, setImports}: {data: EconomyData; locked: boolean; setImports: (body: ImportsBody) => void}) {
    const [search, setSearch] = useState('');
    const never = neverNow(data);
    const own = data.economy.overrides.neverImported !== undefined;
    const shown = useMemo(() => goodsShown(data.goods, search), [data.goods, search]);
    const off = locked || !data.slim;
    return <section className="economy-switches imports-panel">
        <h3>The ships</h3>
        <p className="hint">Goods the ships never carry here, whatever the port wants: kept out of every manifest (a port's
            own makers, a good the realm must make for itself). {own ? 'This world\'s own list.' : `market.json's: ${never.join(', ') || 'none'}.`}</p>
        {!data.slim && <p className="publish-error">This world needs migration 0061 for its own list (python3 tools/world_db.py migrate).</p>}
        <div className="economy-filters">
            <label>Find <input value={search} onChange={e => setSearch(e.target.value)} size={14} /></label>
            <span className="hint">{never.length} never carried · {shown.length} shown</span>
            {own && <button disabled={off} onClick={() => setImports({neverImported: null})}>As market.json</button>}
        </div>
        <div className="dm-table-wrap imports-goods"><table className="dm-table">
            <thead><tr><th>Good</th><th>Never carried</th></tr></thead>
            <tbody>{(search ? shown : [...shown].sort((a, b) => Number(never.includes(b.id)) - Number(never.includes(a.id)))).slice(0, 200)
                .map(g => <tr key={g.id}>
                    <td>{g.name}<br /><small className="hint">{g.id}</small></td>
                    <td><input type="checkbox" checked={never.includes(g.id)} disabled={off} aria-label={`${g.name}: never carried`}
                        onChange={e => { const body = neverChange(data, g.id, e.target.checked); if (body) setImports(body); }} /></td>
                </tr>)}</tbody>
        </table></div>
        {shown.length > 200 && <p className="hint">The first 200 shown: find a good by its name.</p>}
    </section>;
}
