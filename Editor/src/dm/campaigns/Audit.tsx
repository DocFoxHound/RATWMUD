// Audit (doc 85, 5.3): the admin's commands on this run, newest first: who, when, what, the reason, the values before and
// after, and the result. Every one is also in the world's dm.audit (5.1, rule 6).
import {ago, beforeAfter, commandLine, shortDigest, type RunDetail, type Version} from '../../lib/campaigns';

const VERSION_WORDS: Record<Version['status'], string> = {
    approved: 'approved, not moved onto yet', moving: 'moving: waiting for the game', current: 'the version it is on', past: 'left',
    refused: 'refused by the game: it went back', withdrawn: 'withdrawn',
};

/** The versions a run has been on, newest first (doc 85, Phase 5b): which, approved by whom, moved by whom and when. */
export function Versions({versions, now}: {versions: Version[]; now: string}) {
    if (!versions.length) return null;
    return <section className="camp-versions"><h3>Versions</h3><table className="dm-table"><thead><tr><th>Version</th><th>Approved</th><th>Moved onto</th><th></th></tr></thead>
        <tbody>{versions.map(v => <tr key={v.id} className={v.status}>
            <td><code>{shortDigest(v.digest)}</code>{v.title ? <small>{v.title}</small> : null}</td>
            <td>{v.approvedBy.split('#')[0]}<small>{ago(v.approvedAt, now)}</small></td>
            <td>{v.movedAt ? <>{v.movedBy ? v.movedBy.split('#')[0] : 'published'}<small>{ago(v.movedAt, now)}{v.visit ? ` · visit ${v.visit}` : ''}{v.from ? ` · from ${shortDigest(v.from)}` : ''}</small></> : '-'}</td>
            <td><span className={`camp-version ${v.status}`}>{VERSION_WORDS[v.status]}</span>{v.problem ? <small>{v.problem}</small> : null}</td>
        </tr>)}</tbody></table></section>;
}

export function Audit({detail, names, now}: {detail: RunDetail; names: Record<string, string>; now: string}) {
    const commands = detail.commands ?? [];
    const versions = <Versions versions={detail.versions ?? []} now={now} />;
    if (!commands.length) return <>{versions}<p className="hint">Nobody has steered this run yet. Every command is listed here, and in the world's audit.</p></>;
    return <div className="camp-audit">{versions}<table className="dm-table"><thead><tr><th>When</th><th>Who</th><th>What</th><th>Why</th><th>Before → after</th><th>Result</th></tr></thead>
        <tbody>{commands.map(c => <tr key={c.id} className={c.status}>
            <td>{c.at ? new Date(c.at).toLocaleString([], {month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit'}) : ''}<small>{ago(c.at, now)} · visit {c.visit}</small></td>
            <td>{c.by.split('#')[0]}</td>
            <td>{commandLine(c.kind, c.payload, names)}</td>
            <td><q>{c.reason}</q></td>
            <td>{beforeAfter(c) || '-'}</td>
            <td><span className={`camp-cmd ${c.status}`}>{c.status === 'queued' ? 'sent, waiting for the runner' : c.status}</span>{c.result?.text ? <small>{c.result.text}</small> : null}</td>
        </tr>)}</tbody></table></div>;
}
