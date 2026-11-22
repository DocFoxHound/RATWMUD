// Audit (doc 85, 5.3): the admin's commands on this run, newest first: who, when, what, the reason, the values before and
// after, and the result. Every one is also in the world's dm.audit (5.1, rule 6).
import {ago, beforeAfter, commandLine, type RunDetail} from '../../lib/campaigns';

export function Audit({detail, names, now}: {detail: RunDetail; names: Record<string, string>; now: string}) {
    const commands = detail.commands ?? [];
    if (!commands.length) return <p className="hint">Nobody has steered this run yet. Every command is listed here, and in the world's audit.</p>;
    return <div className="camp-audit"><table className="dm-table"><thead><tr><th>When</th><th>Who</th><th>What</th><th>Why</th><th>Before → after</th><th>Result</th></tr></thead>
        <tbody>{commands.map(c => <tr key={c.id} className={c.status}>
            <td>{c.at ? new Date(c.at).toLocaleString([], {month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit'}) : ''}<small>{ago(c.at, now)} · visit {c.visit}</small></td>
            <td>{c.by.split('#')[0]}</td>
            <td>{commandLine(c.kind, c.payload, names)}</td>
            <td><q>{c.reason}</q></td>
            <td>{beforeAfter(c) || '-'}</td>
            <td><span className={`camp-cmd ${c.status}`}>{c.status === 'queued' ? 'sent, waiting for the runner' : c.status}</span>{c.result?.text ? <small>{c.result.text}</small> : null}</td>
        </tr>)}</tbody></table></div>;
}
