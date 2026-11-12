// The bands and the street (Docs/Design/79-a-staged-economy.md, 6 and 10; 12's "status.json's shares and behaviour table,
// edited in Atlas into live.economy.rules"): this world's own parts of Data/Economy/status.json over the file's: the bands'
// shares along the slider, what each band does (its row), the daily caps on what players are given, the pay words talk
// uses, and the street's numbers. Each part back to the file's on its own. Core checks them as it reads the file (a share
// table that fails its checks falls back to the file's). Migration 0061 only. On Atlas's Economy page and the DM's
// Economy tab alike.
import {hosted, needsMigration} from '../lib/hosted';
import {useEffect, useState} from 'react';
import {sharesOf, sharesText, statusChange, statusOverride, statusPart, wholeOf, type EconomyData, type ShareRow,
    type StatusBody} from '../lib/economy';

type Json = Record<string, unknown>;
const CAP_WORDS: Record<string, string> = {rewardsADay: 'quest and contract rewards a player is paid a day',
    theftADay: 'what one player may lose to thieves a day', stakesADay: 'a day\'s winnings at games of chance',
    courierADay: 'courier jobs a day', grantsADay: 'what a DM may grant one player a day (at most 2,500)'};

function JsonPart({data, name, label, hint, off, setStatus}: {data: EconomyData; name: string; label: string; hint: string;
    off: boolean; setStatus: (body: StatusBody) => void}) {
    const [text, setText] = useState('');
    useEffect(() => setText(JSON.stringify(statusPart(data, name), null, 1)), [data, name]);
    const own = statusOverride(data)[name] !== undefined;
    const save = () => {
        let value: unknown;
        try { value = JSON.parse(text); } catch { window.alert(`${label}: not JSON.`); return; }
        setStatus(statusChange(data, name, value));
    };
    return <details className="status-part">
        <summary>{label}{own ? ' (this world\'s own)' : ''}</summary>
        <p className="hint">{hint}</p>
        <textarea value={text} disabled={off} rows={10} cols={70} spellCheck={false} aria-label={label} onChange={e => setText(e.target.value)} />
        <div><button disabled={off} onClick={save}>Save {label.toLowerCase()}</button>
            {' '}{own && <button disabled={off} onClick={() => setStatus(statusChange(data, name, null))}>Back to the default</button>}</div>
    </details>;
}

function Numbers({data, name, words, unit, extra = [], off, setStatus}: {data: EconomyData; name: string; words: Record<string, string>;
    unit: string; extra?: string[]; off: boolean; setStatus: (body: StatusBody) => void}) {
    const now = (statusPart(data, name) ?? {}) as Json, own = (statusOverride(data)[name] ?? {}) as Json;
    const keys = [...Object.keys(now), ...extra.filter(k => !(k in now))];
    const [typed, setTyped] = useState<Record<string, string>>({});
    useEffect(() => setTyped(Object.fromEntries(keys.map(k => [k, own[k] === undefined ? '' : String(own[k])]))), [data]);  // eslint-disable-line react-hooks/exhaustive-deps
    const commit = (k: string) => {
        const t = (typed[k] ?? '').trim();
        const next: Json = {...own};
        if (t === '') delete next[k];
        else {
            const n = wholeOf(t, 1000000);
            if (n === null) { window.alert('A whole number, or empty for the default.'); return; }
            next[k] = n;
        }
        if (JSON.stringify(next) !== JSON.stringify(own)) setStatus(statusChange(data, name, Object.keys(next).length ? next : null));
    };
    return <>{keys.map(k => <label key={k} className="economy-markup">{k}
        <input value={typed[k] ?? ''} disabled={off} inputMode="numeric" size={7} placeholder={now[k] === undefined ? '' : String(now[k])}
            onChange={e => setTyped(t => ({...t, [k]: e.target.value}))} onBlur={() => commit(k)} onKeyDown={e => e.key === 'Enter' && commit(k)} />
        <small>{words[k] ?? unit}</small></label>)}
        {Object.keys(own).length > 0 && <button disabled={off} onClick={() => setStatus(statusChange(data, name, null))}>Back to the default</button>}</>;
}

export function StatusPanel({data, locked, setStatus}: {data: EconomyData; locked: boolean; setStatus: (body: StatusBody) => void}) {
    const off = locked || !data.slim;
    const shares = (statusPart(data, 'shares') ?? []) as ShareRow[];
    const [text, setText] = useState(sharesText(shares));
    useEffect(() => setText(sharesText((statusPart(data, 'shares') ?? []) as ShareRow[])), [data]);
    const parsed = sharesOf(text);
    const ownShares = statusOverride(data).shares !== undefined;
    return <section className="economy-switches status-panel">
        <h3>The bands and the street</h3>
        <p className="hint">{hosted ? 'The parts this world sets for itself; the rest stays the game\'s default. A share table that ' +
            'fails the game\'s checks is the default\'s again; each cap at most five times the default\'s.'
            : 'Data/Economy/status.json\'s parts this world sets for itself; the rest stays the file\'s. A share table that fails ' +
            'Core\'s checks is the file\'s again; each cap at most five times the file\'s.'}</p>
        {!data.slim && <p className="publish-error">{needsMigration('its own parts')}</p>}
        <h4>The bands' shares{ownShares ? ' (this world\'s own)' : ''}</h4>
        <p className="hint">How a market's households split into destitute, poor, comfortable and rich at points along the slider
            (−100 to 100), one line a point: "at: destitute poor comfortable rich" in percents summing to 100, each at least 2;
            the destitute never more and the rich never fewer as the slider rises.</p>
        <textarea value={text} disabled={off} rows={Math.max(3, shares.length + 1)} cols={40} spellCheck={false}
            aria-label="The bands' shares" onChange={e => setText(e.target.value)} />
        {typeof parsed === 'string' && text !== sharesText(shares) && <p className="publish-error">{parsed}</p>}
        <div><button disabled={off || typeof parsed === 'string' || text === sharesText(shares)}
            onClick={() => typeof parsed !== 'string' && setStatus(statusChange(data, 'shares', parsed))}>Save the shares</button>
            {' '}{ownShares && <button disabled={off} onClick={() => setStatus(statusChange(data, 'shares', null))}>Back to the default</button>}</div>
        <h4>The daily caps</h4>
        <Numbers data={data} name="caps" words={CAP_WORDS} unit="a day" extra={['grantsADay']} off={off} setStatus={setStatus} />
        <h4>Pay words</h4>
        <p className="hint">What talk says a post pays a day, in pennies, times the slider ("eight pennies a day"): nothing pays them.</p>
        <Numbers data={data} name="pay" words={{}} unit="p a day, for talk" off={off} setStatus={setStatus} />
        <JsonPart data={data} name="rows" label="What each band does" off={off} setStatus={setStatus}
            hint={`Each band's row: its meals, food, short days, purse and pocket, budget, theft, goods, shops, luxuries, services and alms. An object merged over ${hosted ? 'the default' : 'the file'}'s, band by band.`} />
        <JsonPart data={data} name="street" label="The street" off={off} setStatus={setStatus}
            hint={`Thefts, begging, rough sleeping and the dole: an object merged over ${hosted ? 'the default' : 'the file'}'s.`} />
    </section>;
}
