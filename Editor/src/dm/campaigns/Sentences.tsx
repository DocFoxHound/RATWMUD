// The residents the world's stories sentenced (Docs/Design/86-quests-and-the-ninth-howl.md, engine spec L4, Phase 8): each
// held at a place indoors for its days, even after the story that sentenced them has ended. Listed as the world was last
// saved, with Release now (a reason, audited story.sentence-release; on a hosted world the owner, or "may steer campaigns").
import {useCallback, useEffect, useState} from 'react';
import {storyProgram, type Target} from '../api';
import {sentenceLine, type Sentence} from '../../lib/campaigns';

export function SentencesPanel({target, mayRelease, initial}: {target: Target; mayRelease: boolean; initial?: Sentence[]}) {
    const [list, setList] = useState<Sentence[] | null>(initial ?? null);
    const [releasing, setReleasing] = useState<Sentence | null>(null);
    const [reason, setReason] = useState(''), [problem, setProblem] = useState(''), [said, setSaid] = useState(''), [busy, setBusy] = useState(false);
    const read = useCallback(() => storyProgram<{sentences: Sentence[]}>(target, 'sentences').then(r => setList(r.sentences), e => setProblem((e as Error).message)), [target]);
    useEffect(() => { if (!initial) void read(); }, [read]); // eslint-disable-line react-hooks/exhaustive-deps
    const release = async () => {
        if (!releasing) return;
        setBusy(true); setProblem('');
        try {
            await storyProgram(target, 'sentence-release', {npc: releasing.npc, reason: reason.trim()});
            setSaid(`${releasing.name} is let go: they walk home once the game takes the order.`); setReleasing(null); setReason(''); await read();
        } catch (e) { setProblem((e as Error).message); } finally { setBusy(false); }
    };
    if (!list) return <p className="hint camp-pad">{problem || 'Reading the sentences…'}</p>;
    return <div className="camp-world-vars" aria-label="Sentences">
        <p className="hint">Residents a story sentenced: held where they are for their days, even after the story has ended (as the world was last saved).
            {mayRelease ? '' : ' Releasing one needs the owner, or "may steer campaigns".'}</p>
        <ul className="camp-audit-list">{list.map(s => <li key={s.npc}>{sentenceLine(s)}
            {mayRelease && <button disabled={busy} onClick={() => setReleasing(s)}>Release now…</button>}</li>)}
            {!list.length && <li className="hint">Nobody is serving a sentence.</li>}</ul>
        {releasing && <div className="camp-ask" role="dialog" aria-label="Release a sentenced resident">
            <p>Let {releasing.name} go now? The story's "sentence served" (released) follows; they walk home.</p>
            <label className="field"><span>Why (a few words, for the audit)</span><input value={reason} onChange={e => setReason(e.target.value)} /></label>
            <div className="camp-controls"><button className="primary" disabled={busy || reason.trim().split(/\s+/).length < 2} onClick={() => void release()}>Release now</button>
                <button onClick={() => setReleasing(null)}>Cancel</button></div>
        </div>}
        {said && <p className="hint dm-note">{said}</p>}
        {problem && <p className="error-text">{problem}</p>}
    </div>;
}
