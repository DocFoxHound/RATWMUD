// Orders (doc 85, 5.3): the queue to the game for this run, newest first: what it is waiting on, what was refused or
// expired (Retry, or since Phase 2 "Skip it and carry on"), what is still to send, and what was applied.
import {ago, effectLine, orderLine, type RunDetail, type RunRow} from '../../lib/campaigns';
import type {SteerFn} from './Steer';

export function Orders({detail, row, names, now, busy, steer}: {detail: RunDetail; row: RunRow; names: Record<string, string>; now: string;
    busy: boolean; steer?: SteerFn}) {
    const send = (row.waiting ?? []).find(w => w.on === 'send');
    const blocked = (row.waiting ?? []).find(w => w.on === 'admin' || w.on === 'game');
    const head = blocked && 'order' in blocked ? blocked.order?.id : undefined;
    return <div className="camp-orders">
        {send && send.on === 'send' && <><h3>Still to send</h3><ol>{send.effects.map((e, i) =>
            <li key={i} className="waiting">{effectLine(e, names)}<small>{head ? `waiting behind #${head}` : i ? 'after the one before' : 'next'}</small></li>)}</ol></>}
        <h3>Sent</h3>
        {detail.orders.length === 0 ? <p className="hint">No order sent yet.</p> : <ul className="camp-order-list">{detail.orders.map(o =>
            <li key={o.id} className={o.status}>
                <span>#{o.id} <b>{orderLine(o, names)}</b> <span className={`camp-order ${o.status}`}>{o.status.toUpperCase()}</span>
                    {o.result ? ` "${o.result}"` : ''}</span>
                <small>{o.visit ? `visit ${o.visit} · ` : ''}sent {ago(o.at, now)}{o.doneAt ? ` · answered ${ago(o.doneAt, now)}` : o.status === 'queued' ? ' · not answered yet' : ''} · by {o.by}</small>
                {steer && row.status === 'paused' && o.id === head && (o.status === 'refused' || o.status === 'expired') && <span className="camp-group">
                    <button disabled={busy} onClick={() => steer({kind: 'order', payload: {receipt: o.id, do: 'retry'}, title: `Retry order #${o.id}`,
                        does: [`Sends ${orderLine(o, names)} to the game again; the story carries on when it is applied.`]})}>Retry</button>
                    <button disabled={busy} onClick={() => steer({kind: 'order', payload: {receipt: o.id, do: 'skip'}, title: `Skip order #${o.id}`,
                        does: [`${orderLine(o, names)} is not sent again; the story carries on without it.`], warn: 'Whoever it was for does not get it.'})}>Skip it and carry on…</button>
                </span>}
            </li>)}</ul>}
        <p className="hint">An order the game refuses or lets expire pauses the story for everyone until it is retried (doc 85, known problem 6).
            Retry it, or skip it and carry on; retrying when a wolf is next here, or sending it to someone else, come later (doc 85, 3.5).</p>
    </div>;
}
