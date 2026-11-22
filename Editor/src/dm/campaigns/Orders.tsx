// Orders (doc 85, 5.3): the queue to the game for this run, newest first: what it is waiting on, what was refused or
// expired (with Retry, now for expired orders too), what is still to send, and what was applied.
import {ago, effectLine, orderLine, type RunDetail, type RunRow} from '../../lib/campaigns';

export function Orders({detail, row, names, now, canDirect, busy, retry}: {detail: RunDetail; row: RunRow; names: Record<string, string>; now: string;
    canDirect: boolean; busy: boolean; retry?: () => void}) {
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
                {canDirect && retry && o.id === head && (o.status === 'refused' || o.status === 'expired') &&
                    <button disabled={busy} onClick={retry}>Retry</button>}
            </li>)}</ul>}
        <p className="hint">An order the game refuses or lets expire pauses the story for everyone until it is retried (doc 85, known problem 6).
            Skipping one, retrying it when a wolf is next here, or sending it to someone else come in Phase 2.</p>
    </div>;
}
