// Steering the economy (Docs/Design/74-steering-the-economy.md, 10.1): the DM's Steering panel at the top of the Economy
// tab. The DM sets only four things: where the money rests (a slider, -100 shared to +100 held by the rich), the
// steering's on/off, and the two floors, Never Hungry and Never Broke; each change is confirmed, audited (economy.steer)
// and sent to the running server. Everything else here is read only: each measure against its target at the slider, the
// push and its band, each lever with its bounds there and its value now, the skim's last 28 days, and the log.
import {useEffect, useRef, useState} from 'react';
import {confirmText, leanWords, logLines, measureLines, preview, steerChange, type SteerBody, type SteeringView} from '../lib/steering';

const fmt = (v: number | null | undefined, places = 1) => v === null || v === undefined ? '-' : (Math.round(v * 10 ** places) / 10 ** places).toString();

export function SteeringPanel({view, locked, setSteer}: {view: SteeringView; locked: boolean; setSteer: (body: SteerBody) => void}) {
    const [lean, setLean] = useState(view.settings.lean);
    // (The slider's position last asked about: letting go fires pointer-up and then, as the confirmation takes the focus,
    // blur, and each would ask and send again.)
    const asked = useRef<number | null>(null);
    useEffect(() => { setLean(view.settings.lean); asked.current = null; }, [view.settings.lean]);
    const send = (body: SteerBody) => {
        const change = steerChange(view, body);
        if (!change) { setLean(view.settings.lean); return; }
        if (!window.confirm(confirmText(view, change))) { setLean(view.settings.lean); return; }
        setSteer(change);
    };
    const release = () => {
        if (lean === view.settings.lean || asked.current === lean) return;
        asked.current = lean;
        send({lean});
    };
    const p = preview(view, lean);
    const off = !view.settings.steer;
    const push = view.now?.on ? view.now.push : 0;
    const at = (v: number) => `${(v + 1) * 50}%`;            // (-1..1 on the scale's width.)
    const skim = view.skim;
    return <section className="economy-switches steering-panel">
        <h3>Steering the economy</h3>
        <p className="hint">Tell the steering where the money should rest. It moves a few of the economy's levers slowly, and
            within bounds, toward that place, and shows below what it is doing and why. The levers can't be set by hand while
            it is on. Players are never skimmed and never given anything.</p>
        <label className="steer-slider"><b>Where the money rests</b>
            <input type="range" min={-100} max={100} step={5} value={lean} disabled={locked} list="steer-marks"
                aria-label="Where the money rests" onChange={e => { asked.current = null; setLean(Number(e.target.value)); }}
                onPointerUp={release} onKeyUp={e => { if (e.key === 'Enter') release(); }} onBlur={release} />
            <datalist id="steer-marks">{[-100, -50, 0, 50, 100].map(m => <option key={m} value={m} />)}</datalist>
            <span className="steer-ends"><small>{view.words.left}</small><small>{view.words.centre}</small><small>{view.words.right}</small></span>
            <span>{lean} — {leanWords(view, lean)}{lean !== view.settings.lean && ' (not saved: let go to save it)'}</span></label>
        {off && <p className="hint">The steering is off: the economy runs on its own rules. Where the money rests may still be
            set; it takes effect when the steering is turned on.</p>}
        <label className="toggle-row"><input type="checkbox" checked={view.settings.steer} disabled={locked}
            onChange={e => send({steer: e.target.checked})} /> The steering: {view.settings.steer ? 'on' : 'off'}</label>
        <label className="toggle-row"><input type="checkbox" checked={view.settings.neverHungry} disabled={locked}
            onChange={e => send({neverHungry: e.target.checked})} /> Never Hungry: a wolf going hungry is given a day's food,
            bought at the stable price; the richest pay for it.</label>
        <label className="toggle-row"><input type="checkbox" checked={view.settings.neverBroke} disabled={locked}
            onChange={e => send({neverBroke: e.target.checked})} /> Never Broke: a household short of the next day's necessities
            is given just enough; the richest pay for it.</label>

        <h4>Where it is against where it is going</h4>
        {view.problem && <p className="hint">{view.problem}</p>}
        {view.saved && <small className="hint">From the world's last save ({view.saved}).</small>}
        <div className="dm-table-wrap"><table className="dm-table">
            <thead><tr><th>Measure</th><th>Now</th><th>Target at {lean}</th><th></th></tr></thead>
            <tbody>{measureLines(view, lean).map(m => <tr key={m.name}><td>{m.name}</td><td>{m.now}</td><td>{m.target}</td>
                <td>{m.inside === null ? '' : m.inside ? 'in its band' : 'outside it'}</td></tr>)}</tbody>
        </table></div>
        <div className="steer-scale" title="The push: where the levers are now, and the band the slider allows it">
            <span className="steer-band" style={{left: at(p.band[0]), width: `${(p.band[1] - p.band[0]) * 50}%`}} />
            <span className="steer-push" style={{left: at(push)}} />
        </div>
        <p className="hint">The push {fmt(push, 2)} (its band at {lean}: {fmt(p.band[0], 2)} to {fmt(p.band[1], 2)}).</p>

        <h4>The levers (read only)</h4>
        <div className="dm-table-wrap"><table className="dm-table">
            <thead><tr><th>Lever</th><th>Now</th><th>At the centre</th><th>Bounds at {lean}</th></tr></thead>
            <tbody>{p.levers.map(l => <tr key={l.key}><td title={l.unit}>{l.label}<br /><small className="hint">{l.unit}</small></td>
                <td>{fmt(l.now ?? (view.now?.on ? null : l.centre))}</td><td>{fmt(l.centre)}</td>
                <td>{fmt(l.low)} to {fmt(l.high)}</td></tr>)}</tbody>
        </table></div>
        {view.history?.[0] && <p className="hint">The last judging: {view.history[0].why}</p>}

        <h4>The skim, the last 28 days</h4>
        {skim && skim.days > 0 ? <ul>
            <li>{skim.total}p taken from the richest at the time, on {skim.days} days{skim.short > 0 ? `; ${skim.short}p it could not find` : ''}.</li>
            <li>For: {Object.entries(skim.purpose).map(([k, v]) => `${k} ${v}p`).join(', ') || 'nothing'}.</li>
            <li>From: {Object.entries(skim.group).sort((a, b) => b[1] - a[1]).map(([k, v]) => `${k} ${v}p`).join(', ')}.</li>
            <li>The largest payers: {Object.entries(skim.payers).map(([k, v]) => `${k} ${v}p`).join(', ')}.</li>
        </ul> : <p className="hint">Nothing skimmed in the last 28 days.</p>}

        <details><summary>The log: the DM's changes and the steering's judgings</summary>
            <ul>{logLines(view).map((l, i) => <li key={i}>{l}</li>)}</ul></details>
    </section>;
}
