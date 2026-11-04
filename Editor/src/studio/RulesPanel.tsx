// The loop's rules and the city's services (Docs/Design/68-the-country-and-the-city-loop.md, Progress, "First fixes";
// migration 0058): the rules the DM can tune (comforts a day, the alms, the treasury's cap and its wage support, the
// work bank, fair food, the services' switch and the city's share of them), each the world's own or Data/Items/
// city_loop.json's; and the city's services (a groom and trim at the barber's, coin changed, a cart hired...), each on or
// off with its price and how often a household with money to spare buys it. On Atlas's Economy page and the DM's
// Economy tab alike.
import {useEffect, useState} from 'react';
import {RULES, driftLines, ruleChange, ruleOf, serviceChange, serviceText, serviceValue, wholeOf, type RuleControls,
    type RulesBody, type ServiceListed, type ServicesBody} from '../lib/economy';

type Typed = Record<string, string>;

export function RulesPanel({data, locked, setRules, setServices}: {data: RuleControls; locked: boolean;
    setRules: (body: RulesBody) => void; setServices: (body: ServicesBody) => void}) {
    const [typed, setTyped] = useState<Typed>({});
    useEffect(() => {
        const out: Typed = {};
        for (const r of RULES) out[r.key] = data.rules?.[r.key] === undefined ? '' : String(data.rules[r.key]);
        for (const s of data.serviceList ?? []) {
            const v = serviceValue(data, s);
            out[`${s.id}.price`] = String(v.price);
            out[`${s.id}.every`] = String(v.every);
        }
        setTyped(out);
    }, [data]);
    if (!data.serviceList || !data.ruleDefaults) return null;
    const off = locked || !data.rulesSupported;

    const commitRule = (key: string, max: number) => {
        const value = ruleOf(typed[key] ?? '', max);
        if (value === undefined) { window.alert(`A whole number from 0 to ${max}, or empty for city_loop.json's.`); return; }
        const body = ruleChange(data, key, value);
        if (body) setRules(body);
    };
    const commitService = (s: ServiceListed, key: 'price' | 'every') => {
        const [low, high] = key === 'price' ? [1, 100000] : [1, 3650];
        const value = wholeOf(typed[`${s.id}.${key}`] ?? '', high);
        if (value === null || value < low) { window.alert(`A whole number from ${low} to ${high}.`); return; }
        const body = serviceChange(data, s, key, value);
        if (body) setServices(body);
    };
    const field = (key: string, size: number, label: string, commit: () => void, placeholder = '') =>
        <input value={typed[key] ?? ''} disabled={off} inputMode="numeric" size={size} aria-label={label} placeholder={placeholder}
            onChange={e => setTyped(t => ({...t, [key]: e.target.value}))} onBlur={commit}
            onKeyDown={e => e.key === 'Enter' && commit()} />;

    return <section className="economy-switches rules-panel">
        <h3>The loop's rules</h3>
        <p className="hint">The rules that keep money moving and families fed (doc 68, "First fixes"). Empty: city_loop.json's
            (shown faintly). The treasury's cap pays the shops' short wages only from what it holds over the cap, after its own
            posts. Then what it still holds over the cap and its reserve (the surplus) goes out: a share to public works
            orders from the city's trades, a share to the poorest households up to so much a head (doc 68, "The treasury's
            surplus"). The self-correcting rules (doc 68, "Self-correcting rules") move a shop's prices with its stock, its
            shelves with its sales, a till's wages with what it can pay, and the gate toll and the alms with the treasury's level,
            each gently and within its bounds; where they stand now is below, as the world's last save has it.</p>
        {!data.rulesSupported && <p className="publish-error">{data.rulesProblem}</p>}
        <div className="drift-now"><b>Where the self-correcting rules stand</b>
            {data.drift?.saved && <small className="hint"> (saved {data.drift.saved})</small>}
            {data.drift?.saved
                ? <><ul>{driftLines(data.drift).map(l => <li key={l}>{l}</li>)}</ul>
                    {(data.drift.wages?.moved.length ?? 0) > 0 && <details><summary>Tills whose wages have moved</summary>
                        <ul>{data.drift.wages!.moved.map(w => <li key={w.till}>{w.name}: {w.step > 0 ? '+' : ''}{w.step}p a post</li>)}</ul>
                    </details>}</>
                : <p className="hint">{data.drift?.problem || 'Nothing to show yet.'}</p>}</div>
        {RULES.map(r => r.onOff
            ? <div key={r.key}><label className="toggle-row"><input type="checkbox"
                checked={(data.rules[r.key] ?? data.ruleDefaults[r.key]) === 1} disabled={off}
                onChange={e => { const body = ruleChange(data, r.key, e.target.checked ? 1 : 0); if (body) setRules(body); }} />
                {' '}{r.label}: {r.unit}{data.rules[r.key] === undefined ? ' (city_loop.json\'s)' : ''}</label>
                {data.rules[r.key] !== undefined && <button disabled={off} onClick={() => setRules({set: {[r.key]: null}})}>As city_loop.json</button>}</div>
            : <label key={r.key} className="economy-markup">{r.label}
                {field(r.key, 8, r.label, () => commitRule(r.key, r.max), String(data.ruleDefaults[r.key] ?? ''))}
                <small>{r.unit}</small></label>)}
        <h3>The city's services</h3>
        <p className="hint">Things the barber, the moneychanger, the pawnbroker, the stables, the shipwright and the builders'
            yard do for a household, bought only from money it has to spare (its comforts budget, before comforts), about once
            every so many days; done at once, nothing kept. The city's share of each price goes to the treasury, the rest to
            the shop's till. The armoury's, the shipwright's and the builders' yard's work for their patrons (doc 68, "Trades")
            is paid by the treasury, the Houses' estates or the wharf, for each armed wolf, hull or building about once every
            so many days, all to the shop's till.</p>
        <div className="dm-table-wrap"><table className="dm-table services-table">
            <thead><tr><th>Service</th><th>On</th><th>Price (p)</th><th>Every (days)</th><th></th></tr></thead>
            <tbody>{data.serviceList.map(s => {
                const v = serviceValue(data, s), changed = data.services[s.id] !== undefined;
                return <tr key={s.id} className={changed ? 'changed' : ''}>
                    <td title={serviceText(data, s)}><b>{s.name}</b><br /><small className="hint">{s.business}
                        {s.for === 'patrons' && ', for its patrons: every day count is per unit (an armed wolf, a hull, a building)'}
                        {changed && ` (city_loop.json: ${s.price}p, every ${s.every} days)`}</small></td>
                    <td><input type="checkbox" checked={v.on} disabled={off} aria-label={`${s.name}: on`}
                        onChange={e => { const body = serviceChange(data, s, 'on', e.target.checked); if (body) setServices(body); }} /></td>
                    <td>{field(`${s.id}.price`, 5, `${s.name}: price`, () => commitService(s, 'price'))}</td>
                    <td>{field(`${s.id}.every`, 5, `${s.name}: every`, () => commitService(s, 'every'))}</td>
                    <td>{changed && <button disabled={off} onClick={() => setServices({set: {[s.id]: null}})}>As city_loop.json</button>}</td>
                </tr>;
            })}</tbody>
        </table></div>
    </section>;
}
