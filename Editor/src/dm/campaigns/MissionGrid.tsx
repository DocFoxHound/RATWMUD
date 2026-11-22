// Missions (doc 85, 5.3 "Quests"): for each mission of the current visit, wolves down and objectives across (a wolf's
// own checklist), or one shared row (a group or either mission), with who is in the world now.
import type {Live} from '../api';
import {missionGrid, missionSummary, nameOf, type FramePerson, type RunDetail} from '../../lib/campaigns';

export function MissionGrid({detail, names, live}: {detail: RunDetail; names: Record<string, string>; live: Live | null}) {
    const missions = detail.missionsNow;
    const here = new Set(((live?.frame?.people ?? []) as FramePerson[]).filter(p => p[2] === 'p').map(p => p[0]));
    if (!missions.length) return <p className="hint">This phase has no missions{detail.phases.some(p => p.missions) ? ' (other phases have)' : ''}.</p>;
    const cohort = detail.state?.cohort ?? [];
    return <div className="camp-missions">{missions.map(m => {
        const rows = missionGrid(m, cohort, names);
        return <section key={m.id}>
            <h3>{m.title} <small>({m.mode === 'individual' ? 'each wolf' : m.mode === 'group' ? `together, ${m.requiredContributors} wolves or more` : 'anyone'}{m.optional ? ', optional' : ''})</small></h3>
            <div className="dm-table-wrap"><table className="dm-table camp-grid"><thead><tr><th>Wolf</th>{m.objectives.map((o, i) =>
                <th key={i} title={o.label}>{i + 1}. {o.label.length > 34 ? o.label.slice(0, 32) + '…' : o.label}</th>)}<th></th></tr></thead>
                <tbody>{rows.slice(0, 200).map(r => <tr key={r.who}>
                    <td>{r.who !== 'group' && <span className={here.has(r.who) ? 'camp-here' : 'camp-away'} title={here.has(r.who) ? 'in the world now' : 'not in the world'}>{here.has(r.who) ? '●' : '○'} </span>}{r.name}</td>
                    {r.cells.map((c, i) => <td key={i} className={`camp-cell ${c.state}`}>{c.text}</td>)}
                    <td>{r.done ? 'DONE' : ''}</td></tr>)}</tbody></table></div>
            {m.mode !== 'individual' && m.contributors.length > 0 && <p className="hint">Helped: {m.contributors.map(c => nameOf(names, c)).join(', ')}.</p>}
            <p className="hint">{missionSummary(m, cohort.length)}</p>
        </section>;
    })}</div>;
}
