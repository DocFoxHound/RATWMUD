// Orders (doc 85, 5.3): the queue to the game for this run, newest first: what it is waiting on, what was refused or
// expired (Retry, or since Phase 2 "Skip it and carry on"), what is still to send, and what was applied. Since Phase 7 a
// run's orders go together (one batch, each order with its own outcome), and one refused for a wolf who is away may wait
// for them ("Retry when … is next here"; the effects held so are listed).
import {ago, effectLine, nameOf, orderLine, stuckItem, type Order, type RunDetail, type RunRow} from '../../lib/campaigns';
import type {SteerFn} from './Steer';

export function Orders({detail, row, names, now, busy, steer}: {detail: RunDetail; row: RunRow; names: Record<string, string>; now: string;
    busy: boolean; steer?: SteerFn}) {
    const send = (row.waiting ?? []).find(w => w.on === 'send');
    const blocked = (row.waiting ?? []).find(w => w.on === 'admin' || w.on === 'game');
    const head = blocked && 'order' in blocked ? blocked.order?.id : undefined;
    const cohort = new Set(detail.state?.cohort ?? []);
    const stuckOn = (o: Order) => o.items ? stuckItem(o) : undefined;
    const canAct = (o: Order) => steer && row.status === 'paused' && o.id === head &&
        (o.status === 'refused' || o.status === 'expired' || (o.status === 'applied' && !!stuckOn(o)));
    return <div className="camp-orders">
        {send && send.on === 'send' && <><h3>Still to send</h3><ol>{send.effects.map((e, i) =>
            <li key={i} className="waiting">{effectLine(e, names)}<small>{head ? `waiting behind #${head}` : i ? 'with the one before' : 'next'}</small></li>)}</ol></>}
        {!!detail.whenHere?.length && <><h3>Waiting for their wolf</h3><ul className="camp-order-list">{detail.whenHere.map((h, i) =>
            <li key={i} className="waiting"><span><b>{h.kind} → {nameOf(names, h.wolf)}</b> refused: "{h.refused}"</span>
                <small>sent again when {nameOf(names, h.wolf)} is next in the world · until {new Date(h.until * 1000).toLocaleString()} · first order #{h.receipt}</small></li>)}</ul></>}
        <h3>Sent</h3>
        {detail.orders.length === 0 ? <p className="hint">No order sent yet.</p> : <ul className="camp-order-list">{detail.orders.map(o => {
            const stuck = stuckOn(o);
            const what = stuck ? orderLine(stuck, names) : orderLine(o, names);
            const wolf = stuck && cohort.has(stuck.target) ? stuck.target : !o.items && cohort.has(o.target) ? o.target : '';
            return <li key={o.id} className={o.status}>
                <span>#{o.id} <b>{orderLine(o, names)}</b> <span className={`camp-order ${o.status}`}>{o.status.toUpperCase()}</span>
                    {o.result ? ` "${o.result}"` : ''}</span>
                <small>{o.visit ? `visit ${o.visit} · ` : ''}sent {ago(o.at, now)}{o.doneAt ? ` · answered ${ago(o.doneAt, now)}` : o.status === 'queued' ? ' · not answered yet' : ''} · by {o.by}</small>
                {o.items && <ol className="camp-batch">{o.items.map(i => <li key={i.n} className={i.status}>
                    {orderLine(i, names)} <span className={`camp-order ${i.status === 'held' ? 'queued' : i.status}`}>{i.status === 'held' ? 'NOT SENT' : i.status.toUpperCase()}</span>
                    {i.result && i.status !== 'applied' ? <small>"{i.result}"</small> : null}
                    {i.ifRefused !== 'pause' && <small>(if refused: {i.ifRefused === 'skip' ? 'carry on' : 'wait for the wolf'})</small>}</li>)}</ol>}
                {canAct(o) && steer && <span className="camp-group">
                    <button disabled={busy} onClick={() => steer({kind: 'order', payload: {receipt: o.id, do: 'retry'}, title: `Retry order #${o.id}`,
                        does: [`Sends ${what} to the game again${o.items ? ', with the orders after it' : ''}; the story carries on when it is applied.`]})}>Retry</button>
                    {wolf && <button disabled={busy} onClick={() => steer({kind: 'order', payload: {receipt: o.id, do: 'when-here'}, title: `Retry when ${nameOf(names, wolf)} is next here`,
                        does: [`${what} waits for ${nameOf(names, wolf)} and is sent again when they next come into the world (for up to 7 days); the story carries on now.`]})}>
                        Retry when {nameOf(names, wolf)} is next here</button>}
                    <button disabled={busy} onClick={() => steer({kind: 'order', payload: {receipt: o.id, do: 'skip'}, title: `Skip order #${o.id}`,
                        does: [`${what} is not sent again; the story carries on without it.`], warn: 'Whoever it was for does not get it.'})}>Skip it and carry on…</button>
                </span>}
            </li>;
        })}</ul>}
        <p className="hint">An order the game refuses pauses the story when its effect says so (the default for world changes), is noted and
            skipped, or waits for its wolf to come back (the default for a quest's reward): each effect's "if refused" in the Story Creator.
            A paused one is retried, held for its wolf, or skipped.</p>
    </div>;
}
