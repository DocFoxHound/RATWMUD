// A running chapter's readiness (Docs/Design/86-quests-and-the-ninth-howl.md and the engine spec's R3, doc 86 Phase 1a): one
// bar per part of the phase's Readiness (who took part, the days, values, missions, a quest), the header (ready, not ready,
// waiting for its moment, goes on by itself), and Advance…, Advance anyway… and Advance at its moment beside Hold and Resume.
// Every button sends a command through the same AskDialog as the SteerBar's (a reason, a dry run, the typed title for
// "anyway"). Holding stops the chapter's clock: held days do not count toward its days.
import {nameOf, readyBars, readyHeader, type ReadyBar, type RunDetail, type RunRow} from '../../lib/campaigns';
import type {SteerFn} from './Steer';

function Bar({bar}: {bar: ReadyBar}) {
    if (bar.any) return <li className={`camp-ready-any ${bar.met ? 'met' : ''}`}><b>{bar.label}</b> <small>(any one)</small>
        <ul>{bar.any.map((b, i) => <Bar key={i} bar={b} />)}</ul></li>;
    return <li className={bar.met ? 'met' : ''}>
        <div className="camp-ready-label"><b>{bar.label}</b>{bar.met && <span className="camp-ready-tag">met</span>}</div>
        <div className="camp-ready-bar" role="meter" aria-valuemin={0} aria-valuemax={100} aria-valuenow={Math.round(bar.fill * 100)} aria-label={bar.label}>
            <span className="fill" style={{width: `${Math.round(bar.fill * 100)}%`}} />
            {bar.mark < 1 && <span className="mark" style={{left: `${Math.round(bar.mark * 100)}%`}} title="needed" />}
        </div>
        <small>{bar.text}</small>
    </li>;
}

export function Readiness({row, detail, names, steer, busy}: {row: RunRow; detail: RunDetail | null; names: Record<string, string>;
    steer?: SteerFn; busy: boolean}) {
    const r = row.ready;
    if (!r) return null;
    const head = readyHeader(r);
    const bars = readyBars(r, names);
    const parts = r.parts ?? [];   // (A record kept by moving onto a new version has none until it is measured again.)
    const who = parts.find(p => p.kind === 'participation' && p.audience)?.audience;
    const notMet = parts.filter(p => !p.met).length;
    const left = `${parts.length - notMet} of ${parts.length} parts met`;
    const snapshot = bars.map(b => `${b.label}: ${b.text}${b.met ? ' (met)' : ''}`);
    const held = row.status === 'held';
    const canAdvance = steer && ['running', 'held', 'paused'].includes(row.status) && !!detail?.controls?.ready;
    const thenWords = r.thenWords ?? (r.then ? r.then.on : '');
    return <section className="camp-ready" aria-label="Readiness">
        <h3>Readiness <small>{r.auto ? '(goes on by itself when ready)' : '(an admin advances it)'}</small></h3>
        <p className={`camp-ready-head ${head.tone}`}>{head.text}</p>
        {who && <p className="hint">active {who.active} · in the story {who.all}{who.online != null ? ` · online now ${who.online}` : ''} ·
            active = seen in the last {r.activeDays ?? 18} game days ({Math.round(((r.activeDays ?? 18) / 6) * 10) / 10} real days)</p>}
        <ul className="camp-ready-bars">{bars.map((b, i) => <Bar key={i} bar={b} />)}</ul>
        {r.heldDays ? <p className="hint">{r.heldDays} held day{r.heldDays === 1 ? '' : 's'} in this phase are not counted.</p> : null}
        {r.slow && <p className={r.slowNeedsYou ? 'camp-ask-warn' : 'hint'}>Slow: not ready after {r.slowAfterDays} days. Advance anyway, or wait.</p>}
        {canAdvance && <div className="camp-controls" aria-label="Move the chapter on">
            {r.met
                ? <button className="primary" disabled={busy} onClick={() => steer!({kind: 'advance', payload: {when: 'now', to: r.to}, title: `Advance to ${r.to}`,
                    does: [`Takes the ready exit: ${r.phase} → ${r.to}, with ${r.to}'s entry effects.`, ...snapshot]})}>Advance…</button>
                : <button disabled={busy} onClick={() => steer!({kind: 'advance', payload: {when: 'now', anyway: true, to: r.to}, title: `Advance to ${r.to} anyway`,
                    warn: `Not ready: ${left}. The audit keeps what you saw and why.`,
                    does: [`Takes the ready exit now: ${r.phase} → ${r.to}, although it is not ready.`, ...snapshot]})}>Advance anyway…</button>}
            {r.then && !r.waitingFor && <button disabled={busy} onClick={() => steer!({kind: 'advance', payload: {when: 'then', to: r.to, ...(r.met ? {} : {anyway: true})},
                title: `Advance to ${r.to} at ${thenWords}`, ...(r.met ? {} : {warn: `Not ready: ${left}.`}),
                does: [`Counts as ready and waits for ${thenWords}; then it moves on to ${r.to}.`, ...snapshot]})}>Advance at {thenWords}…</button>}
            {held
                ? <button disabled={busy} onClick={() => steer!({kind: 'resume', payload: {catchUp: true}, title: 'Resume',
                    does: ['It reads the world events that happened while it was held, in order.', 'The held days are not counted toward its days.']})}>▶ Resume</button>
                : row.status === 'running' && <button disabled={busy} onClick={() => steer!({kind: 'hold', payload: {}, title: 'Hold',
                    does: ['It reads no events, sends no orders and fires no timers until resumed.', 'Holding stops the chapter\'s clock: held days do not count toward its days.']})}>⏸ Hold</button>}
        </div>}
        <p className="hint">Holding stops the chapter's clock: held days do not count toward its days.{r.pushed ? ` Pushed by ${nameOf(names, r.pushed.by.split('#')[0])}: “${r.pushed.reason}”.` : ''}</p>
    </section>;
}
