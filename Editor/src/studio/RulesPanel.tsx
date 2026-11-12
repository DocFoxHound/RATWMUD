// The rules, the residents' needs and the city's services (Docs/Design/79-a-staged-economy.md, 12): Core's rules (the
// clamps, the Market Board's, staged trade's, the port's, street life's and the staged events'), each the world's own or
// the data files' default; residents' needs paused (maintenance and testing); and the city's services, each on or off with
// its price and how often a household with money to spare has it done. The slider's rule `prosperity` is its own panel's
// (SteerPanel). On Atlas's Economy page and the DM's Economy tab alike.
import {useEffect, useState} from 'react';
import {heldRules, ruleChange, ruleOf, ruleValue, ruleWords, serviceChange, serviceText, serviceValue, wholeOf,
    type EconomyData, type NeedsBody, type RuleListed, type RulesBody, type ServiceListed, type ServicesBody} from '../lib/economy';

type Typed = Record<string, string>;

export function RulesPanel({data, locked, setRules, setServices, setNeeds}: {data: EconomyData; locked: boolean;
    setRules: (body: RulesBody) => void; setServices: (body: ServicesBody) => void; setNeeds: (body: NeedsBody) => void}) {
    const [typed, setTyped] = useState<Typed>({});
    useEffect(() => {
        const out: Typed = {};
        for (const r of data.ruleList) out[r.name] = data.economy.rules[r.name] === undefined ? '' : String(data.economy.rules[r.name]);
        for (const s of data.serviceList) {
            const v = serviceValue(data, s);
            out[`${s.id}.price`] = String(v.price);
            out[`${s.id}.every`] = String(v.every);
        }
        setTyped(out);
    }, [data]);
    const off = locked || !data.supported;
    const held = heldRules(data);

    const commitRule = (r: RuleListed) => {
        const value = ruleOf(typed[r.name] ?? '', r.high, r.low);
        if (value === undefined) { window.alert(`A whole number from ${r.low} to ${r.high}, or empty for the default.`); return; }
        const body = ruleChange(data, r, value);
        if (body) setRules(body);
    };
    const commitService = (s: ServiceListed, key: 'price' | 'every') => {
        const [low, high] = key === 'price' ? [1, 100000] : [1, 3650];
        const value = wholeOf(typed[`${s.id}.${key}`] ?? '', high, low);
        if (value === null) { window.alert(`A whole number from ${low} to ${high}.`); return; }
        const body = serviceChange(data, s, key, value);
        if (body) setServices(body);
    };
    const field = (key: string, size: number, label: string, commit: () => void, placeholder = '') =>
        <input value={typed[key] ?? ''} disabled={off} inputMode="numeric" size={size} aria-label={label} placeholder={placeholder}
            onChange={e => setTyped(t => ({...t, [key]: e.target.value}))} onBlur={commit}
            onKeyDown={e => e.key === 'Enter' && commit()} />;

    return <section className="economy-switches rules-panel">
        <h3>The rules</h3>
        <p className="hint">The staged economy's rules: the clamps Never Hungry and Never Broke, how the Market Board's prices move
            with what players buy and sell, the wagons, ships, thefts and staged events. Empty: the default (shown faintly).</p>
        {!data.supported && <p className="publish-error">{data.problem}</p>}
        <label className="toggle-row"><input type="checkbox" checked={!data.economy.needsPaused} disabled={off}
            onChange={e => setNeeds({needsPaused: !e.target.checked})} /> Residents' needs: {data.economy.needsPaused
                ? 'paused (nobody grows hungry or buys; for maintenance and testing)' : 'on (they eat and buy)'}</label>
        {data.ruleList.map(r => {
            const words = ruleWords(r), own = data.economy.rules[r.name] !== undefined;
            return r.switch
                ? <div key={r.name}><label className="toggle-row"><input type="checkbox" checked={ruleValue(data, r) === 1} disabled={off}
                    onChange={e => { const body = ruleChange(data, r, e.target.checked ? 1 : 0); if (body) setRules(body); }} />
                    {' '}{words.label}: {words.unit}{own ? '' : ' (the default)'}</label>
                    {own && <button disabled={off} onClick={() => setRules({set: {[r.name]: null}})}>As the default</button>}</div>
                : <label key={r.name} className="economy-markup">{words.label}
                    {field(r.name, 8, words.label, () => commitRule(r), String(r.fallback))}
                    <small>{words.unit}</small></label>;
        })}
        {held.length > 0 && <p className="hint">{held.length} rule{held.length === 1 ? '' : 's'} this world kept from before the
            staged economy ({held.join(', ')}): kept and not listed; the game reads them past (its log names each at load).</p>}
        <h3>The city's services</h3>
        <p className="hint">Things the barber, the moneychanger, the pawnbroker, the stables, the shipwright and the builders'
            yard do for a household or a patron. A household has each done about once every so many days, times its band's (none
            for the destitute, a barber's now and then for the poor, a patron's only for the rich), at a business of its kind in
            its market. Nothing is paid: each counts toward that business's trade (thriving, steady or failing). Off: never
            done. The price is the player's.</p>
        <div className="dm-table-wrap"><table className="dm-table services-table">
            <thead><tr><th>Service</th><th>On</th><th>Price (p)</th><th>Every (days)</th><th></th></tr></thead>
            <tbody>{data.serviceList.map(s => {
                const v = serviceValue(data, s), changed = data.economy.services[s.id] !== undefined;
                return <tr key={s.id} className={changed ? 'changed' : ''}>
                    <td title={serviceText(data, s)}><b>{s.name}</b><br /><small className="hint">{s.business}
                        {s.for === 'patrons' && ', for its patrons: every day count is per unit (an armed wolf, a hull, a building)'}
                        {changed && ` (market.json: ${s.price}p, every ${s.every} days)`}</small></td>
                    <td><input type="checkbox" checked={v.on} disabled={off} aria-label={`${s.name}: on`}
                        onChange={e => { const body = serviceChange(data, s, 'on', e.target.checked); if (body) setServices(body); }} /></td>
                    <td>{field(`${s.id}.price`, 5, `${s.name}: price`, () => commitService(s, 'price'))}</td>
                    <td>{field(`${s.id}.every`, 5, `${s.name}: every`, () => commitService(s, 'every'))}</td>
                    <td>{changed && <button disabled={off} onClick={() => setServices({set: {[s.id]: null}})}>As market.json</button>}</td>
                </tr>;
            })}</tbody>
        </table></div>
    </section>;
}
