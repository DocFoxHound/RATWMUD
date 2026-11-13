// Publish (Docs/Design/78, 10.3, 12.3-12.5; Phase M8): a host's map edits made into the map players are in. The page
// says up front how many map changes wait and whether this computer can publish; Publish asks how the world moves onto
// the new map (open only to me, now with a minute's warning, when everyone has left), then shows the job: a backup, the
// checks (each problem in plain words with "Show me"), the build, the restart. After a look alone: "Open to everyone" or
// "Go back to the previous map"; and "Discard unpublished changes". A map that didn't start, and an update that needs
// the map rebuilt, say so here. Builder and up (tools/studio.py's hosted routes; tools/world_publish.py).
import {useCallback, useEffect, useState} from 'react';
import {atLeast, dmApi, type Me} from '../dm/api';
import {goTo} from '../lib/hosted';
import {greyed} from '../lib/acts';
import {FALLBACK_TEXT, jobLine, jobRunning, publishBlocked, rebuildQuestion, RESTART_CHOICES, showQuery, unpublishedLine,
    type PublishProblem, type PublishState, type Restart} from '../lib/publish';

function Problems({items, kind}: {items: PublishProblem[]; kind: 'error' | 'hint'}) {
    if (!items.length) return null;
    return <ul className="publish-problems">{items.map((p, i) => {
        const query = showQuery(p);
        return <li key={i} className={kind}>{p.text}{query && <> <button className="small" onClick={() => goTo(query)}>Show me</button></>}</li>;
    })}</ul>;
}

export function PublishPage({me}: {me: Me}) {
    const [state, setState] = useState<PublishState | null>(null), [problem, setProblem] = useState('');
    const [choice, setChoice] = useState<Restart>('host'), [busy, setBusy] = useState(false), [note, setNote] = useState('');
    const [refused, setRefused] = useState<PublishProblem[]>([]), [confirming, setConfirming] = useState<'' | 'discard' | 'back'>('');
    const [alsoDiscard, setAlsoDiscard] = useState(false);
    const load = useCallback(() => dmApi.mapState().then(s => { setState(s); setProblem(''); return s; })
        .catch(e => { setProblem((e as Error).message); return null; }), []);
    const running = jobRunning(state?.job);
    useEffect(() => {
        void load();
        const t = setInterval(() => void load(), running ? 2000 : 15_000);
        return () => clearInterval(t);
    }, [load, running]);
    if (!state) return <div className="dm-center"><p className="hint">{problem || 'Reading the map\'s state…'}</p></div>;

    const act = async (work: () => Promise<{ok: boolean; warnings?: PublishProblem[]}>, done: string) => {
        setBusy(true); setNote(''); setRefused([]);
        try {
            const out = await work();
            setNote(done);
            setRefused(out.warnings ?? []);
            setConfirming('');
        } catch (error) {
            const e = error as Error & {problems?: PublishProblem[]};
            setNote(e.message);
            setRefused((e.problems ?? []).filter(p => p.text !== e.message));
        } finally { setBusy(false); void load(); }
    };
    const count = state.unpublished.count;
    const builder = atLeast(me, 'builder'), owner = atLeast(me, 'owner');
    const why = greyed([!builder, 'Publishing is for builder and owner accounts.'], [busy, 'Working…'], [!!publishBlocked(state, choice), publishBlocked(state, choice)],
        [!!state.rebuild, 'This update needs the map rebuilt first (above).']);
    const idle = greyed([!builder, 'For builder and owner accounts.'], [busy, 'Working…'], [running, 'The map is being published now.']);
    const job = state.job;
    return <div className="sync-page publish-page">
        <div className="dm-sheet-bar">
            <strong>Publish</strong>
            <span className="meta hint" data-unpublished>{unpublishedLine(count)}{state.unpublished.last ? ` · last by ${state.unpublished.last.by}` : ''}</span>
            <button onClick={() => void load()}>Refresh</button>
        </div>
        {problem && <p className="error">{problem}</p>}
        <div className="sync-body">
            {state.fallback && <section className="sync-card wide" data-fallback><h3>The last map didn't start</h3>
                <p className="error">{FALLBACK_TEXT}</p>
                <p className="hint">The map that failed is kept for a bug report (map {state.fallback.failed ?? '?'}). Put the edit right in the map
                    editor and publish again, or discard the unpublished changes below.</p></section>}
            {state.rebuild && <section className="sync-card wide" data-rebuild><h3>This update needs your map rebuilt</h3>
                <p>{rebuildQuestion(state.rebuild)}</p>
                {!state.memory.ok && <p className="error">{state.memory.words}</p>}
                <p><button className="primary" disabled={!owner || busy || running || !state.memory.ok} title={owner ? undefined : 'Only the world\'s owner can answer this.'}
                    onClick={() => void act(() => dmApi.mapRebuild(false), 'Rebuilding the map…')}>{(state.rebuild.unpublished ?? 0) > 0 ? 'Rebuild with my changes' : 'Rebuild the map'}</button>
                {(state.rebuild.unpublished ?? 0) > 0 && <>{' '}<button disabled={!owner || busy || running || !state.memory.ok}
                    onClick={() => void act(() => dmApi.mapRebuild(true), 'Discarded the changes; rebuilding the map…')}>Discard them, then rebuild</button></>}</p>
            </section>}
            <section className="sync-card wide" data-publish>
                <h3>Publish the map</h3>
                <p data-memory className={state.memory.ok ? 'hint' : 'error'}>{state.memory.words}</p>
                <p>Players see your map changes after Publish. It makes a backup, checks the map, builds it (about a minute or two) and then moves
                    the world onto it. If anything is wrong, nothing is changed and each problem is listed here.</p>
                <div className="publish-choices" role="radiogroup" aria-label="How the world moves onto the new map">
                    {RESTART_CHOICES.map(c => {
                        const off = c.id === 'host' && !state.hostOnly.available;
                        return <label key={c.id} className={off ? 'off' : ''} title={off ? state.hostOnly.reason : undefined}>
                            <input type="radio" name="restart" checked={choice === c.id} disabled={off} onChange={() => setChoice(c.id)} />
                            <span><b>{c.label}</b><small>{off ? state.hostOnly.reason : c.hint}</small></span>
                        </label>;
                    })}
                </div>
                <p><button className="primary" data-action="publish" disabled={!!why} title={why || undefined}
                    onClick={() => void act(() => dmApi.mapPublish(choice), 'Publishing…')}>Publish{count ? ` ${count} ${count === 1 ? 'change' : 'changes'}` : ''}</button>
                    {' '}<span className="hint">At most {state.perHour} an hour for each account.</span></p>
                {job && <div className="my-world-steps" data-job>
                    <p className={job.step === 'refused' || job.step === 'failed' ? 'error' : undefined}><strong>{jobLine(job)}</strong>
                        {' '}<span className="hint">by {job.by}</span></p>
                    <Problems items={job.errors} kind="error" />
                    <Problems items={job.warnings} kind="hint" />
                </div>}
                {note && <p className="hint" data-note>{note}</p>}
                <Problems items={refused} kind="error" />
            </section>
            {state.serverState === 'host_only' && <section className="sync-card wide" data-host-only>
                <h3>The world is open only to you</h3>
                <p>Join it from the game and look at the new map. Players can't come in until you open it.</p>
                <p><button className="primary" disabled={!!idle} title={idle || undefined} data-action="open-all"
                    onClick={() => void act(() => dmApi.mapOpen(), 'The world is open to everyone.')}>Open to everyone</button>
                {' '}<button disabled={!!idle || state.previous === null} title={idle || (state.previous === null ? 'There is no previous map to go back to.' : undefined)}
                    onClick={() => { setConfirming('back'); setAlsoDiscard(false); }}>Go back to the previous map…</button></p>
            </section>}
            <section className="sync-card wide" data-undo>
                <h3>Undo</h3>
                <p><button disabled={!!idle || state.previous === null} title={idle || (state.previous === null ? 'There is no previous map to go back to.' : undefined)}
                    data-action="back" onClick={() => { setConfirming('back'); setAlsoDiscard(false); }}>Go back to the previous map…</button>
                {' '}<button disabled={!!idle || !count} title={idle || (!count ? 'There are no unpublished map changes.' : undefined)}
                    data-action="discard" onClick={() => setConfirming('discard')}>Discard unpublished changes…</button></p>
                {confirming === 'discard' && <div className="my-world-steps" data-confirm-discard>
                    <p>This puts the map editor back to the map as it was last published: your {count} unpublished map {count === 1 ? 'change is' : 'changes are'} undone.
                        Nothing players did is touched. It can't be undone.</p>
                    <button disabled={busy} onClick={() => void act(() => dmApi.mapDiscard(), 'The unpublished map changes are discarded.')}>Discard them</button>
                    {' '}<button onClick={() => setConfirming('')}>Cancel</button>
                </div>}
                {confirming === 'back' && <div className="my-world-steps" data-confirm-back>
                    <p>The world goes back to the map it had before the last Publish: players are given a minute, it closes and starts again on that map.
                        Nothing players did is lost. Your edits stay in the map editor, so the next Publish would bring them back unless you discard them.</p>
                    <label><input type="checkbox" checked={alsoDiscard} onChange={e => setAlsoDiscard(e.target.checked)} /> Also discard my unpublished map changes</label>
                    <p><button disabled={busy} onClick={() => void act(() => dmApi.mapBack(alsoDiscard), 'The world is going back to its previous map.')}>Go back</button>
                    {' '}<button onClick={() => setConfirming('')}>Cancel</button></p>
                </div>}
                <p className="hint">A map that won't start goes back to the previous one by itself. Map edits are undone in the map editor as you work (Ctrl+Z).</p>
            </section>
        </div>
    </div>;
}
