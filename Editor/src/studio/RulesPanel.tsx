// The rules and the city's services (Docs/Design/68-the-country-and-the-city-loop.md, Progress, "First fixes";
// migration 0058): the rules the DM can tune (since Docs/Design/79-a-staged-economy.md, 17, the clamps and the Market
// Board's), each the world's own or the default; and the city's services (a groom and trim at the barber's, coin changed, a cart hired...), each on or
// off with its price and how often a household with money to spare buys it. On Atlas's Economy page and the DM's
// Economy tab alike.
import {useEffect, useState} from 'react';
import {RULES, ruleChange, ruleOf, serviceChange, serviceText, serviceValue, wholeOf, type RuleControls,
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

    const commitRule = (key: string, max: number, low = 0) => {
        const value = ruleOf(typed[key] ?? '', max);
        if (value === undefined || (value !== null && value < low)) { window.alert(`A whole number from ${low} to ${max}, or empty for the default.`); return; }
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
        <h3>The rules</h3>
        <p className="hint">The staged economy's rules (Docs/Design/79-a-staged-economy.md): the clamps Never Hungry and Never
            Broke, and how the Market Board's prices move with what players buy and sell. Empty: the default (shown faintly).
            A rule this world kept from before doc 79 does nothing and is not listed.</p>
        {!data.rulesSupported && <p className="publish-error">{data.rulesProblem}</p>}
        {RULES.filter(r => r.key in data.ruleDefaults).map(r => r.onOff
            ? <div key={r.key}><label className="toggle-row"><input type="checkbox"
                checked={(data.rules[r.key] ?? data.ruleDefaults[r.key]) === 1} disabled={off}
                onChange={e => { const body = ruleChange(data, r.key, e.target.checked ? 1 : 0); if (body) setRules(body); }} />
                {' '}{r.label}: {r.unit}{data.rules[r.key] === undefined ? ' (the default)' : ''}</label>
                {data.rules[r.key] !== undefined && <button disabled={off} onClick={() => setRules({set: {[r.key]: null}})}>As the default</button>}</div>
            : <label key={r.key} className="economy-markup">{r.label}
                {field(r.key, 8, r.label, () => commitRule(r.key, r.max, r.low), String(data.ruleDefaults[r.key] ?? ''))}
                <small>{r.unit}</small></label>)}
        <h3>The city's services</h3>
        <p className="hint">Things the barber, the moneychanger, the pawnbroker, the stables, the shipwright and the builders'
            yard do for a household or a patron, about once every so many days; done at once, nothing kept.</p>
        <p className="publish-error">Not in play yet: since the staged economy (Docs/Design/79-a-staged-economy.md, Phase 3)
            nothing buys these. They are kept, and a change here is saved, for street life (doc 79's Phase 6), which makes
            them staged sales at their frequency by band.</p>
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
