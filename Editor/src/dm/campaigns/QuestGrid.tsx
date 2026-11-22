// Quests (doc 85, 5.3 and 5.4; Phase 6): for each quest of the story, its copies down (each wolf's own, each party's, the
// story's one) and its objectives across, with who is in the world now; and every control on them: complete, add to,
// reveal, fail or reset an objective, give the quest, take it away, complete or fail it. Each is a command to the story
// runner (asked first, with a reason, as every steering control); completing by hand gives the quest's rewards as play would
// (5.8, rule 5: no silent rewards).
import {useState} from 'react';
import type {Live} from '../api';
import {giveAsk, nameOf, questGrid, questSummary, rowAsks, withoutQuest, type FramePerson, type RunDetail} from '../../lib/campaigns';
import type {Ask, SteerFn} from './Steer';

const SCOPE: Record<string, string> = {player: 'each wolf', party: 'each party', story: 'one copy for everyone'};

function RowMenu({asks, steer, busy}: {asks: {label: string; ask: Ask}[]; steer: SteerFn; busy: boolean}) {
    const [open, setOpen] = useState(false);
    return <span className="camp-menu"><button type="button" disabled={busy} aria-expanded={open} onClick={() => setOpen(!open)} title="Complete objective… · Set progress… · Reveal · Fail · Reset · Give · Take it away · Complete · Fail">▾</button>
        {open && <span className="camp-menu-list" role="menu">{asks.map(a => <button key={a.label} type="button" role="menuitem" onClick={() => {
            setOpen(false);
            if (a.ask.payload.do !== 'progress') { steer(a.ask); return; }
            // (Set progress: how much more, as play would count it.)
            const amount = Math.round(Number(window.prompt('Add how much towards its goal?', '1')));
            if (Number.isFinite(amount) && amount >= 1) steer({...a.ask, payload: {...a.ask.payload, amount}, does: [`Adds ${amount} towards its goal, as play would.`]});
        }}>{a.label}…</button>)}</span>}</span>;
}

export function QuestGrid({detail, names, live, steer, busy = false}: {detail: RunDetail; names: Record<string, string>; live: Live | null; steer?: SteerFn; busy?: boolean}) {
    const quests = detail.quests ?? [];
    const here = new Set(((live?.frame?.people ?? []) as FramePerson[]).filter(p => p[2] === 'p').map(p => p[0]));
    const cohort = detail.state?.cohort ?? [];
    const [giving, setGiving] = useState<Record<string, string>>({});
    if (!quests.length) return <p className="hint">This story has no quests.</p>;
    return <div className="camp-missions camp-quests">{quests.map(q => {
        const rows = questGrid(q, names);
        const free = withoutQuest(q, cohort);
        const pick = giving[q.id] ?? free[0] ?? '';
        return <section key={q.id}>
            <h3>{q.title} <small>({SCOPE[q.scope] ?? q.scope}{q.repeat && q.repeat !== 'once' ? `, again ${q.repeat}` : ''}{q.lifetime === 'phase' ? ', this phase only' : ''}
                {q.giver ? ` · from ${nameOf(names, q.giver)}` : ''}{q.open === false ? ' · not offered in this phase' : ''})</small></h3>
            {steer && <div className="camp-controls">{free.length > 0 ? <><select value={pick} aria-label={'Give ' + q.title + ' to'} onChange={e => setGiving(g => ({...g, [q.id]: e.target.value}))}>
                {free.map(w => <option key={w} value={w}>{nameOf(names, w)}</option>)}</select>
                <button disabled={busy || !pick} onClick={() => steer(giveAsk(q, pick, names))}>Give the quest…</button></>
                : <small className="hint">Every wolf of the story holds it.</small>}</div>}
            {rows.length ? <div className="dm-table-wrap"><table className="dm-table camp-grid"><thead><tr><th>Who</th>{q.objectives.map(o =>
                <th key={o.id} title={o.label}>{o.label.length > 34 ? o.label.slice(0, 32) + '…' : o.label}{o.optional ? ' (optional)' : ''}{o.hidden ? ' ?hidden' : ''}</th>)}<th></th>{steer && <th></th>}</tr></thead>
                <tbody>{rows.slice(0, 300).map(r => <tr key={r.holder}>
                    <td>{r.kind === 'wolf' && <span className={here.has(r.holder) ? 'camp-here' : 'camp-away'} title={here.has(r.holder) ? 'in the world now' : 'not in the world'}>{here.has(r.holder) ? '●' : '○'} </span>}
                        {r.name}{r.left.length > 0 && <small> (left: {r.left.map(w => nameOf(names, w)).join(', ')})</small>}</td>
                    {r.cells.map((c, i) => <td key={i} className={`camp-cell ${c.state}`}>{c.text}</td>)}
                    <td className={`camp-quest-status ${r.status}`}>{r.word}</td>
                    {steer && <td className="camp-group"><RowMenu asks={rowAsks(q, r)} steer={steer} busy={busy}/></td>}</tr>)}</tbody></table></div>
                : <p className="hint">Nobody has taken it yet.</p>}
            <p className="hint">{questSummary(q)}</p>
        </section>;
    })}</div>;
}
