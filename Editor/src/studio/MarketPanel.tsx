// The staged events and the ships' names (Docs/Design/79-a-staged-economy.md, 8.3-8.5 and 12): this world's own parts of
// Data/Economy/market.json: which staged event kinds it has (a House wedding, a fire at the works, a good catch...), how
// many a market sees a week, the causes the town gives for the times, and the ships' names at a port. Each back to the
// file's on its own. Migration 0061 only. On Atlas's Economy page and the DM's Economy tab alike.
import {asShipped, hosted, needsMigration, shippedOwn} from '../lib/hosted';
import {useEffect, useState} from 'react';
import {kindChange, kindsOn, marketChange, marketOverride, perWeekNow, shipsNow, shipsOf, type EconomyData,
    type MarketBody} from '../lib/economy';

type Json = Record<string, unknown>;

export function MarketPanel({data, locked, setMarket}: {data: EconomyData; locked: boolean; setMarket: (body: MarketBody) => void}) {
    const off = locked || !data.slim;
    const on = kindsOn(data);
    const events = ((marketOverride(data).events ?? {}) as Json);
    const [ships, setShips] = useState('');
    const [causes, setCauses] = useState('');
    useEffect(() => {
        setShips(shipsNow(data).join('\n'));
        setCauses(JSON.stringify(events.causes ?? data.defaults.market.events.causes, null, 1));
    }, [data]);  // eslint-disable-line react-hooks/exhaustive-deps
    const named = shipsOf(ships);
    const own = (k: string) => k in events;
    const ownShips = Array.isArray(((marketOverride(data).wharf ?? {}) as Json).ships);
    const saveCauses = () => {
        let value: unknown;
        try { value = JSON.parse(causes); } catch { window.alert('The causes: not JSON.'); return; }
        setMarket(marketChange(data, 'events', 'causes', value));
    };
    const hostKinds = on.filter(id => !data.defaults.market.events.list.some(k => k.id === id));
    return <section className="economy-switches market-panel">
        <h3>Staged events and ships</h3>
        <p className="hint">{hosted ? 'The parts this world sets for itself; the rest stays the game\'s default.'
            : 'Data/Economy/market.json\'s parts this world sets for itself; the rest stays the file\'s.'}</p>
        {!data.slim && <p className="publish-error">{needsMigration('its own parts')}</p>}
        <label className="economy-markup">Events a week
            <select value={perWeekNow(data)} disabled={off} onChange={e => setMarket(marketChange(data, 'events', 'perWeek', Number(e.target.value)))}>
                {[0, 1, 2, 3].map(n => <option key={n} value={n}>{n}</option>)}</select>
            <small>a market's staged events a week, before the rule eventRate{own('perWeek') ? '' : ` (${shippedOwn('market')})`}</small></label>
        {own('perWeek') && <button disabled={off} onClick={() => setMarket(marketChange(data, 'events', 'perWeek', null))}>{asShipped('market')}</button>}
        <h4>The kinds{own('list') ? ' (this world\'s own list)' : ''}</h4>
        <div className="dm-table-wrap"><table className="dm-table">
            <thead><tr><th>Event</th><th>Where</th><th>On</th></tr></thead>
            <tbody>{data.defaults.market.events.list.map(k => <tr key={k.id} className={on.includes(k.id) ? '' : 'muted'}>
                <td>{k.words}<br /><small className="hint">{k.id}</small></td><td>{k.needs.join(', ') || 'any market'}</td>
                <td><input type="checkbox" checked={on.includes(k.id)} disabled={off} aria-label={`${k.words}: on`}
                    onChange={e => { const body = kindChange(data, k.id, e.target.checked); if (body) setMarket(body); }} /></td>
            </tr>)}</tbody>
        </table></div>
        {hostKinds.length > 0 && <p className="hint">This world's own kinds: {hostKinds.join(', ')}.</p>}
        {own('list') && <button disabled={off} onClick={() => setMarket(marketChange(data, 'events', 'list', null))}>{hosted ? 'Every kind, as the default' : 'Every kind, as market.json'}</button>}
        <details className="market-causes">
            <summary>The causes{own('causes') ? ' (this world\'s own)' : ''}</summary>
            <p className="hint">What the town says of hard and good times: {'{down: [...], up: [...]}'} causes ({'{id, words, notice, cry}'})
                and the notices and cries that carry them ({'{cause}'} in each).</p>
            <textarea value={causes} disabled={off} rows={12} cols={70} spellCheck={false} aria-label="The causes" onChange={e => setCauses(e.target.value)} />
            <div><button disabled={off} onClick={saveCauses}>Save the causes</button>
                {' '}{own('causes') && <button disabled={off} onClick={() => setMarket(marketChange(data, 'events', 'causes', null))}>{asShipped('market')}</button>}</div>
        </details>
        <h4>The ships' names{ownShips ? ' (this world\'s own)' : ''}</h4>
        <textarea value={ships} disabled={off} rows={6} cols={30} spellCheck={false} aria-label="The ships' names" onChange={e => setShips(e.target.value)} />
        {typeof named === 'string' && <p className="publish-error">{named}</p>}
        <div><button disabled={off || typeof named === 'string' || ships === shipsNow(data).join('\n')}
            onClick={() => typeof named !== 'string' && setMarket(marketChange(data, 'wharf', 'ships', named))}>Save the names</button>
            {' '}{ownShips && <button disabled={off} onClick={() => setMarket(marketChange(data, 'wharf', 'ships', null))}>{asShipped('market')}</button>}</div>
    </section>;
}
