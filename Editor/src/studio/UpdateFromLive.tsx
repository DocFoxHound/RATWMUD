// Atlas's Update from live (Docs/Design/66-worlds-atlas-and-dm.md, section 4; tools/world_sync.py pull): Atlas's world
// against the live world it is a copy of (its upstream). Every change live made since the last pull or import comes in;
// dev's own changes stay; where both changed a row, the user keeps dev's or takes live's (whole, or field by field).
// Live's state becomes the base. Open Atlas sessions reload (the engine logs a "reload" edit), and the Workshop opens
// on the new world.
import {useCallback, useEffect, useMemo, useState} from 'react';
import {api} from '../lib/api';
import {DmError, type Me} from '../dm/api';
import {buildChoices, prunePicks, summaryLine, summaryTotal, type Picks, type PullPlan, type PullResult} from '../lib/sync';
import {ChangeGroups, ConflictList, Problems, SummaryTable, when} from './SyncViews';

const LABELS = {dev: 'Keep dev\'s', live: 'Take live\'s'} as const;

export function UpdateFromLive({me, onWorkshop}: {me: Me; onWorkshop: () => void}) {
    const [plan, setPlan] = useState<PullPlan | null>(null);
    const [refusal, setRefusal] = useState<{message: string; status: number} | null>(null);
    const [problem, setProblem] = useState<{message: string; errors: string[]} | null>(null);
    const [picks, setPicks] = useState<Picks>({});
    const [busy, setBusy] = useState(false), [loading, setLoading] = useState(true);
    const [done, setDone] = useState<PullResult | null>(null);
    const load = useCallback(async () => {
        setLoading(true);
        try {
            const next = await api.sync.diff();
            setPlan(next); setRefusal(null);
            setPicks(p => prunePicks(p, next.conflicts));
            return next;
        } catch (error) {
            setRefusal({message: (error as Error).message, status: error instanceof DmError ? error.status : 0});
            return null;
        } finally { setLoading(false); }
    }, []);
    useEffect(() => { void load(); }, [load]);
    const {choices, missing} = useMemo(() => buildChoices(plan?.conflicts ?? [], picks), [plan, picks]);

    if (refusal && !plan) return <div className="dm-center"><div className="empty-sheet">
        <h2>⇩ Update from live</h2>
        <p>{refusal.message}</p>
        {refusal.status === 409 && <p>A side project is built on its own; when it is ready, make a world copied from the live world
            and bring the work over there, or ask for it to be given an upstream in <code>Database/worlds.json</code> (then
            <code> python3 tools/world_sync.py init-base KEY</code>).</p>}
        <button onClick={() => void load()}>Try again</button></div></div>;
    if (!plan) return <div className="dm-center"><p className="hint">Comparing Atlas's world with the live world…</p></div>;

    const nothing = !plan.counts.live && !plan.counts.conflicts;
    const running = plan.liveServer ? plan.liveServer.state.state === 'running' || plan.liveServer.running : true;
    const apply = async () => {
        setBusy(true); setProblem(null); setDone(null);
        try {
            const result = await api.sync.pull(choices, plan.digest);
            setDone(result); setPicks({});
            await load();
        } catch (error) {
            const e = error as DmError;
            setProblem({message: e.message, errors: e.status === 409 ? [] : e.errors ?? []});
            // The worlds moved, or conflicts are left: show the plan as it is now.
            if (e.status === 409 || e.status === 422) await load();
        } finally { setBusy(false); }
    };
    const applied = summaryTotal(done?.applied);

    return <div className="sync-page">
        <div className="dm-sheet-bar">
            <strong>⇩ Update {plan.devWorld.name} from {plan.liveWorld.name}</strong>
            <span className="meta hint">{plan.devWorld.key} is a copy of {plan.liveWorld.key} · live release {plan.liveRelease || 'none yet'} ·
                base: release {plan.base.release || 0}, {plan.base.how} by {plan.base.by || 'someone'} {when(plan.base.takenAt)}</span>
            <button disabled={loading || busy} onClick={() => void load()}>{loading ? 'Comparing…' : 'Compare again'}</button>
        </div>
        <div className="sync-body">
            <p className="hint">Brings every change made on the live world since the last update (or import) into Atlas's world, keeps
                the changes made here, and asks where both changed the same thing. Nothing is written to the live world; the
                DM's Import from dev takes your work live, during maintenance. Players' data, the save and the economy are never touched.</p>
            {running && <Problems title={`The live server is ${plan.liveServer?.state.state === 'maintenance' ? 'still running' : 'running'}: live data (NPCs, factions) may still move after this update. That's fine: the DM's import compares again and shows anything new.`} items={[]} />}
            {plan.warnings.filter(w => !w.startsWith('The live server is running')).map(w => <p key={w} className="hint">{w}</p>)}
            <div className="sync-counts">
                <div><b>{plan.counts.live}</b><span>live changes to take</span></div>
                <div><b>{plan.counts.dev}</b><span>dev changes kept</span></div>
                <div className={plan.counts.conflicts ? 'warn' : ''}><b>{plan.counts.conflicts}</b><span>conflicts</span></div>
            </div>
            {done && <p className="publish-done">Updated from {done.live} (release {done.release}): {applied} row{applied === 1 ? '' : 's'} changed in
                Atlas's world, {done.conflicts} conflict{done.conflicts === 1 ? '' : 's'} chosen, {done.kept} dev change{done.kept === 1 ? '' : 's'} kept.
                Open Atlas sessions reload. <button onClick={onWorkshop}>Open the Workshop</button></p>}
            {problem && <Problems title={problem.message} items={problem.errors} />}
            {nothing ? <p className="publish-done">Atlas's world already has everything live has changed{plan.counts.dev ? `; its ${plan.counts.dev} own change${plan.counts.dev === 1 ? '' : 's'} wait for the DM's import` : ''}.</p> : <>
                <h3>Live's changes, to take <span className="meta">({summaryLine(plan.summary.live)})</span></h3>
                <ChangeGroups items={plan.liveChanges} side="live" empty="Live changed nothing that dev didn't." />
                <h3>Conflicts <span className="meta">changed on both sides: choose for each</span></h3>
                <ConflictList conflicts={plan.conflicts} picks={picks} onPicks={setPicks} labels={LABELS} disabled={busy} />
            </>}
            <details className="sync-kept-list"><summary>Dev's own changes, kept ({plan.counts.dev})</summary>
                <SummaryTable summary={plan.summary.dev} empty="None." />
                <ChangeGroups items={plan.devChanges} side="dev" empty="None." /></details>
            {!nothing && <div className="sync-apply">
                {missing.length > 0 && <span className="hint">{missing.length} conflict{missing.length === 1 ? '' : 's'} still to choose.</span>}
                <button className="primary" disabled={busy || loading || missing.length > 0 || me.role === 'viewer'} onClick={() => void apply()}>
                    {busy ? 'Updating…' : `⇩ Update ${plan.devWorld.name}`}</button>
            </div>}
        </div>
    </div>;
}
