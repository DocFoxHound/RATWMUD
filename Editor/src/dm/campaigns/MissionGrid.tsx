// Missions (doc 85, 5.3 "Quests"): for each mission of the current visit, wolves down and objectives across (a wolf's
// own checklist), or one shared row (a group or either mission), with who is in the world now; and since Phase 2 a mission
// completed or reset by hand for a wolf (or the shared checklist), which the journal is told of.
import type {Live} from '../api';
import {missionGrid, missionSummary, nameOf, type FramePerson, type RunDetail} from '../../lib/campaigns';
import type {SteerFn} from './Steer';

export function MissionGrid({detail, names, live, steer, busy = false}: {detail: RunDetail; names: Record<string, string>; live: Live | null;
    steer?: SteerFn; busy?: boolean}) {
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
                    <td>{r.done ? 'DONE' : ''}</td>
                    {steer && <td className="camp-group">
                        {!r.done && <button disabled={busy} onClick={() => steer({kind: 'mission', payload: {mission: m.id, do: 'complete', ...(r.who !== 'group' ? {player: r.who} : {})},
                            title: `Complete "${m.title}"${r.who !== 'group' ? ' for ' + r.name : ''}`,
                            does: ['Every objective is marked done, and the journal told.', 'It counts towards moving on at the next thing a wolf of the story does.',
                                'Today\'s missions carry no rewards of their own: a trigger that gives one fires as it would in play.']})}>Complete…</button>}
                        {r.cells.some(c => c.state !== 'open' && c.state !== 'later') && <button disabled={busy} onClick={() => steer({kind: 'mission',
                            payload: {mission: m.id, do: 'reset', ...(r.who !== 'group' ? {player: r.who} : {})}, title: `Reset "${m.title}"${r.who !== 'group' ? ' for ' + r.name : ''}`,
                            does: ['Its progress goes back to the first objective, and the journal is told.'], warn: 'What it already gave stays given.'})}>Reset…</button>}
                    </td>}</tr>)}</tbody></table></div>
            {m.mode !== 'individual' && m.contributors.length > 0 && <p className="hint">Helped: {m.contributors.map(c => nameOf(names, c)).join(', ')}.</p>}
            <p className="hint">{missionSummary(m, cohort.length)}</p>
        </section>;
    })}</div>;
}
