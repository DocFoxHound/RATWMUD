// The slider (Docs/Design/79-a-staged-economy.md, 12): the user's one control, -100 hard times to +100 boom in steps of
// 5, with an optional cause the town says of it ("the Crown's war levy") and "now" (the times jump to the mark instead of
// moving there over a few days). The rule `prosperity` and `prosperityCause` in live.economy.rules, through POST
// economy/steer; on Atlas's Economy page and the DM's Economy tab alike.
import {useEffect, useState} from 'react';
import {causeProblem, prosperityText, steerChange, type EconomyData, type SteerBody} from '../lib/economy';

export function SteerPanel({data, locked, setSteer}: {data: EconomyData; locked: boolean; setSteer: (body: SteerBody) => void}) {
    const [value, setValue] = useState(data.prosperity.value);
    const [cause, setCause] = useState(data.prosperity.cause);
    const [now, setNow] = useState(false);
    useEffect(() => { setValue(data.prosperity.value); setCause(data.prosperity.cause); }, [data.prosperity.value, data.prosperity.cause]);
    const off = locked || !data.supported;
    const problem = causeProblem(cause);
    const body = problem ? null : steerChange(data, value, cause, now);
    return <section className="economy-switches steer-panel">
        <h3>Prosperity</h3>
        <p className="hint">Where the times sit: hard (−100) to boom (+100), ordinary at 0. Bands, prices, wagons, ships and
            the street follow it; the town gives its cause. {data.steerNote ?? ''}</p>
        {!data.supported && <p className="publish-error">{data.problem}</p>}
        <label className="economy-markup steer-slider">
            <input type="range" min={data.prosperity.low} max={data.prosperity.high} step={data.prosperity.step} value={value}
                disabled={off} aria-label="Prosperity" onChange={e => setValue(Number(e.target.value))} />
            <b>{prosperityText(value)}</b>
            {value !== data.prosperity.value && <small>(now {prosperityText(data.prosperity.value)})</small>}</label>
        <label className="economy-markup">Its cause
            <input value={cause} disabled={off} maxLength={80} size={40} placeholder="(the world picks one)"
                onChange={e => setCause(e.target.value)} />
            <small>{problem || 'what the town says of it, at most 80 letters'}</small></label>
        <label className="toggle-row"><input type="checkbox" checked={now} disabled={off} onChange={e => setNow(e.target.checked)} />
            {' '}Now: the times jump to the mark (else they move there over a few days)</label>
        <div><button disabled={off || !body} onClick={() => { if (body) { setSteer(body); setNow(false); } }}>Set the times</button>
            {' '}<button disabled={off || (value === 0 && !data.prosperity.value)} onClick={() => setValue(0)}>Ordinary (0)</button></div>
    </section>;
}
