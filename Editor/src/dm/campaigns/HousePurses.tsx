// The world switch for House purses (Docs/Design/86-quests-and-the-ninth-howl.md, engine spec M3, Phase 9): off on every
// world until the user approves its economy check; on, each House's businesses put a share of their tills in its purse
// daily, and a story may pay from house:<House>. Shown as the world was last saved; the owner turns it on or off with a
// reason (audited story.house-purses; the game's order story.house_purses).
import {useCallback, useEffect, useState} from 'react';
import {storyProgram, type Target} from '../api';
import {housePurseLine, type HousePurses} from '../../lib/campaigns';

export function HousePursesPanel({target, mayChange, initial}: {target: Target; mayChange: boolean; initial?: HousePurses}) {
    const [state, setState] = useState<HousePurses | null>(initial ?? null);
    const [asking, setAsking] = useState(false);
    const [reason, setReason] = useState(''), [problem, setProblem] = useState(''), [said, setSaid] = useState(''), [busy, setBusy] = useState(false);
    const read = useCallback(() => storyProgram<HousePurses>(target, 'house-purses-state').then(setState, e => setProblem((e as Error).message)), [target]);
    useEffect(() => { if (!initial) void read(); }, [read]); // eslint-disable-line react-hooks/exhaustive-deps
    const turn = async () => {
        if (!state) return;
        setBusy(true); setProblem('');
        try {
            await storyProgram(target, 'house-purses', {on: !state.on, reason: reason.trim()});
            setSaid(`House purses go ${state.on ? 'off' : 'on'} once the game takes the order (this panel shows the world as last saved).`);
            setAsking(false); setReason(''); await read();
        } catch (e) { setProblem((e as Error).message); } finally { setBusy(false); }
    };
    if (!state) return <p className="hint camp-pad">{problem || 'Reading the House purses…'}</p>;
    return <div className="camp-world-vars" aria-label="House purses">
        <p className="hint">House purses are <b>{state.on ? 'on' : 'off'}</b> (as the world was last saved). On, each House's businesses put a share of
            their tills in its purse once a game day, and a story may pay from <code>house:&lt;House&gt;</code>; off, no money moves and approval refuses
            such a purse. Turn them on only once their economy check is approved.{mayChange ? '' : ' Turning them on or off is the owner\'s.'}</p>
        <ul className="camp-audit-list">{state.houses.map(h => <li key={h.id}>{housePurseLine(h)}</li>)}
            {!state.houses.length && <li className="hint">This world has no Houses.</li>}</ul>
        {mayChange && !asking && <div className="camp-controls"><button disabled={busy} onClick={() => setAsking(true)}>Turn House purses {state.on ? 'off' : 'on'}…</button></div>}
        {asking && <div className="camp-ask" role="dialog" aria-label="Turn House purses on or off">
            <p>{state.on ? 'Turn House purses off? No more money moves to them, and a story\'s house: purse pays nothing.'
                : 'Turn House purses on? From now on each House\'s businesses put a tenth of their tills over 80 in its purse daily.'}</p>
            <label className="field"><span>Why (a few words, for the audit)</span><input value={reason} onChange={e => setReason(e.target.value)} /></label>
            <div className="camp-controls"><button className="primary" disabled={busy || reason.trim().split(/\s+/).length < 2} onClick={() => void turn()}>
                Turn them {state.on ? 'off' : 'on'}</button>
                <button onClick={() => setAsking(false)}>Cancel</button></div>
        </div>}
        {!!state.audit?.length && <ul className="camp-audit-list">{state.audit.map(a => <li key={a.at} className="hint">{a.who}: {a.detail}</li>)}</ul>}
        {said && <p className="hint dm-note">{said}</p>}
        {problem && <p className="error-text">{problem}</p>}
    </div>;
}
