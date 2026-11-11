// The DM's Server & import tab (Docs/Design/66-worlds-atlas-and-dm.md, section 4; tools/studio.py over
// tools/world_sync.py). The user's flow: start maintenance (players get a minute's warning, then the game server saves
// and stops), import a dev world's changes into the live world (choosing where both changed), and end maintenance
// (the launcher, tools/live.sh server, starts the server again on the live world). Also: switching the live world
// (rarely wise; maintenance only, typed confirmation), and the releases with rollback of the authored content.
import {useCallback, useEffect, useMemo, useState} from 'react';
import {dmApi, DmError, type Me, type Target, type World} from './api';
import {gameUrl} from '../lib/api';
import {buildChoices, defaultPicks, letGoText, prunePicks, replaceMapText, type ImportPlan, type ImportResult, type Picks, type Release,
    type ServerStatus, type SyncWorld} from '../lib/sync';
import {ChangeGroups, ConflictList, Problems, SummaryTable, when} from '../studio/SyncViews';

const LABELS = {dev: 'Import dev\'s', live: 'Keep live\'s'} as const;
type Plan = Awaited<ReturnType<typeof dmApi.importPlan>>;

export function ServerTab({me, target}: {me: Me; target: Target}) {
    const [status, setStatus] = useState<ServerStatus | null>(null);
    const [problem, setProblem] = useState('');
    const canAct = me.role !== 'viewer';
    const loadStatus = useCallback(() => dmApi.serverStatus(target).then(s => { setStatus(s); setProblem(''); return s; })
        .catch(e => { setProblem((e as Error).message); return null; }), [target]);
    useEffect(() => { void loadStatus(); }, [loadStatus]);
    // While the world is closing (maintenance asked, its game server still up), watch for the server to stop.
    // (And while its server is starting.)
    const closing = (status?.server.state === 'maintenance' && status.serverRunning) || (!!status?.starting && !status.serverRunning);
    useEffect(() => { if (!closing) return; const t = setInterval(() => void loadStatus(), 5000); return () => clearInterval(t); }, [closing, loadStatus]);

    if (!status) return <div className="dm-center"><p className="hint">{problem || 'Reading the server\'s state…'}</p></div>;
    const maintenance = status.server.state === 'maintenance';
    return <div className="sync-page">
        <div className="dm-sheet-bar">
            <strong>Server &amp; import · {status.name}</strong>
            <span className="meta hint">{status.key} · release {status.release || 'none yet'}</span>
            <button onClick={() => void loadStatus()}>Refresh</button>
        </div>
        {problem && <p className="error">{problem}</p>}
        <div className="sync-body">
            <ServerPanel status={status} canAct={canAct} onChanged={setStatus} />
            {status.kind === 'dev'
                ? <BuildPanel status={status} canAct={canAct} onBuilt={() => void loadStatus()} />
                : <ImportPanel live={status} maintenance={maintenance} stopped={!status.serverRunning} canAct={canAct} onImported={() => void loadStatus()} />}
            <SwitchPanel status={status} maintenance={maintenance} canAct={canAct} />
            {status.kind !== 'dev' && <ReleasesPanel world={status.key} release={status.release} canAct={canAct} onRolledBack={() => void loadStatus()} />}
        </div>
    </div>;
}

// --------------------------------------------------------------------------- The server

function ServerPanel({status, canAct, onChanged}: {status: ServerStatus; canAct: boolean; onChanged: (s: ServerStatus) => void}) {
    const [note, setNote] = useState(''), [busy, setBusy] = useState(false), [problem, setProblem] = useState('');
    const maintenance = status.server.state === 'maintenance';
    const set = async (on: boolean) => {
        const ask = on
            ? `Start maintenance on ${status.name}?\n\nEveryone playing is told the world closes in 60 seconds (again at 10), then the game server saves and stops. It stays closed until you end maintenance.`
            : `End maintenance on ${status.name}?\n\nThe server is turned back on: its launcher (tools/live.sh server) starts the game on the live world within a few seconds, and players can join.`;
        if (!window.confirm(ask)) return;
        setBusy(true); setProblem('');
        try { onChanged({...status, ...(await dmApi.maintenance(status.key, on, note))}); setNote(''); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };
    const startServer = async () => {
        setBusy(true); setProblem('');
        try {
            const r = await dmApi.startServer(status.key);
            if (r.note) setProblem(r.note);
            onChanged({...status, starting: true});
        } catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };
    return <section className="sync-card">
        <h3>The server</h3>
        <p><b>{status.name}</b> <span className="meta">({status.key}){status.key === status.live ? ' · the live server\'s world' : ''}</span></p>
        <p className={maintenance ? 'sync-state maintenance' : 'sync-state running'}>
            {maintenance ? '⏸ In maintenance' : '▶ Running'}
            {status.server.by && <span className="meta"> · {maintenance ? 'closed' : 'opened'} by {status.server.by} {when(status.server.at)}</span>}
            {status.server.note && <span className="meta"> · “{status.server.note}”</span>}</p>
        <p className="meta">{status.serverRunning
            ? (maintenance ? 'A game server still holds this world: it is telling players and will save and stop within about a minute.'
                : 'A game server holds this world: players can be in it.')
            : (maintenance ? 'No game server holds this world: it is closed, safe to import or switch.'
                : 'No game server holds this world right now (its launcher may be starting it, or it isn\'t running).')}</p>
        {status.play && <p><a className="button play" href={gameUrl(status.play)} target="_blank" rel="noopener">▶ Open its game</a>
            <span className="meta"> {gameUrl(status.play)}</span></p>}
        {!status.serverRunning && !status.starting && <p><button disabled={!canAct || busy} onClick={() => void startServer()}
            title="Starts tools/live.sh server for this world on the studio's computer, on the local network">▶ Start its game server</button></p>}
        {status.starting && !status.serverRunning && <p className="meta">Its game server is starting (building first if Atlas changed it)…</p>}
        {problem && <Problems title={problem} items={[]} />}
        {!maintenance ? <>
            <label className="field"><span>Note for players and the log (optional)</span>
                <input value={note} maxLength={400} placeholder="e.g. Adding houses to Ridgemere; back within the hour." onChange={e => setNote(e.target.value)} /></label>
            <button className="publish" disabled={!canAct || busy} onClick={() => void set(true)}>{busy ? 'Starting…' : '⏸ Start maintenance'}</button>
            <p className="hint">Players get a 60-second warning (and another at 10 seconds); then the game server saves and stops.</p>
        </> : <>
            <button className="primary" disabled={!canAct || busy} onClick={() => void set(false)}>{busy ? 'Turning on…' : '▶ End maintenance (turn the server back on)'}</button>
            <p className="hint">The launcher (<code>tools/live.sh server</code>) waits while the world is in maintenance and starts the server again
                once it is running, on whichever world is live by then.</p>
        </>}
    </section>;
}

// --------------------------------------------------------------------------- A dev world served as live: its builds

/** Instead of Import from dev, when the live server runs a dev world (DEV while PROD is closed: doc 66, Progress):
 *  Atlas edits this very world, and its edits reach the server with a new build. */
function BuildPanel({status, canAct, onBuilt}: {status: ServerStatus; canAct: boolean; onBuilt: () => void}) {
    const [busy, setBusy] = useState(false), [problem, setProblem] = useState(''), [done, setDone] = useState('');
    const build = status.build;
    const run = async () => {
        if (!window.confirm(`Make a new build of ${status.name} from Atlas's edits?\n\nThe game server restarts onto it as soon as nobody is connected (players connected now are told, and it waits for them to leave).`)) return;
        setBusy(true); setProblem(''); setDone('');
        try {
            const r = await dmApi.rebuild(status.key);
            setDone(r.made ? `Build ${r.build} made (${r.cells} cells): the server restarts onto it once nobody is connected.`
                : `Build ${r.build} was already current: announced again, so a server on an older build restarts onto it once nobody is connected.`);
            onBuilt();
        } catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };
    return <section className="sync-card wide">
        <h3>⟳ Atlas's edits to the server</h3>
        <p className="hint">{status.servedDev}</p>
        <p className="meta">NPCs &amp; areas, factions and talk changed in Atlas's NPC pages reach a running server at once. The map, buildings,
            interiors and the Workshop's people need a new build: the server restarts onto it once nobody is connected
            (<code>tools/live.sh server</code> starts it again, and saves, players' progress and the DM's actions carry over).</p>
        {build?.problem ? <Problems title={build.problem} items={[]} />
            : <p className={build?.current ? 'sync-state running' : 'sync-state maintenance'}>
                {!build?.build ? 'No build yet.' : build.current ? `Build ${build.build} is current with Atlas's edits` : `Build ${build.build} is behind Atlas's edits`}
                {build?.build ? <span className="meta"> · made {when(build.at)}{build.by ? ` by ${build.by}` : ''}</span> : null}</p>}
        {problem && <Problems title={problem} items={[]} />}
        {done && <p className="publish-done">{done}</p>}
        <button className="publish" disabled={!canAct || busy} onClick={() => void run()}>{busy ? 'Building…' : '⟳ Rebuild from Atlas\'s edits'}</button>
    </section>;
}

// --------------------------------------------------------------------------- Import from dev

function ImportPanel({live, maintenance, stopped, canAct, onImported}: {live: ServerStatus; maintenance: boolean; stopped: boolean;
        canAct: boolean; onImported: () => void}) {
    const [data, setData] = useState<Plan | null>(null);
    const [dev, setDev] = useState<Target | null>(null);
    const [picks, setPicks] = useState<Picks>({});
    const [note, setNote] = useState(''), [password, setPassword] = useState('');
    const [replace, setReplace] = useState(false);              // The replace-the-map confirmation (two maps only).
    const [busy, setBusy] = useState(false), [loading, setLoading] = useState(false);
    const [problem, setProblem] = useState<{message: string; errors: string[]} | null>(null);
    const [done, setDone] = useState<ImportResult | null>(null);
    const load = useCallback(async (which: Target | null) => {
        setLoading(true);
        try {
            const next = await dmApi.importPlan(live.key, which ?? undefined);
            setData(next); setDev(next.dev);
            if (!next.plan?.replacesMap) setReplace(false);
            setPicks(p => ({...defaultPicks(next.plan?.conflicts ?? []), ...prunePicks(p, next.plan?.conflicts ?? [])}));   // (The game's truth to start.)
        } catch (error) { setProblem({message: (error as Error).message, errors: []}); }
        finally { setLoading(false); }
    }, [live.key]);
    // Compared again whenever the server's state changes (live may have moved; maintenance unblocks it).
    useEffect(() => { void load(dev); }, [load, live.server.state, live.serverRunning, live.release]);   // eslint-disable-line react-hooks/exhaustive-deps
    const plan: ImportPlan | null = data?.plan ?? null;
    const {choices, missing} = useMemo(() => buildChoices(plan?.conflicts ?? [], picks), [plan, picks]);

    const replacing = plan?.replacesMap && plan.maps ? replaceMapText(plan.maps, dev ?? plan.dev, live.key) : null;
    const run = async () => {
        if (!plan || !dev) return;
        const ask = replacing
            ? `Replace ${live.name}'s map with ${worldLabel(data?.candidates, dev)}'s?\n\n${replacing.warning}\n\nThis writes the live world in one go and records a release.`
            : `Import ${worldLabel(data?.candidates, dev)} into ${live.name}?\n\nThis writes the live world in one go and records a release (it can be rolled back).`;
        if (!window.confirm(ask)) return;
        setBusy(true); setProblem(null); setDone(null);
        try {
            const result = await dmApi.importDev({world: live.key, dev, choices, password, note, expect: plan.digest, ...(replacing ? {replaceMap: replace} : {})});
            setDone(result); setPassword(''); setNote(''); setPicks({}); setReplace(false);
            onImported();
            await load(dev);
        } catch (error) {
            const e = error as DmError;
            setProblem({message: e.message, errors: e.status === 409 ? [] : e.errors});
            if (e.status === 401) setPassword('');
            if (e.status === 409 || e.status === 422) await load(dev);       // The worlds moved, or conflicts are left.
        } finally { setBusy(false); }
    };

    const blocked = plan?.blocked ?? [];
    const why = !maintenance ? 'Start maintenance first: an import only happens while the live world is closed.'
        : !stopped ? 'Waiting for the game server to stop (it closes within a minute of maintenance).' : '';
    return <section className="sync-card wide">
        <h3>⇪ Import from dev</h3>
        <p className="hint">Brings a dev world's changes (the map, buildings, NPCs, factions...) into {live.name}, keeps what changed only on live,
            and asks where both changed the same thing. One transaction; a numbered release you can roll back. Players' data and the save are
            never written, and the economy's settings (live.economy) are this world's own: an update never overwrites them. Atlas's Update
            from live first makes this smaller.</p>
        {!data ? <p className="hint">{loading ? 'Comparing…' : ''}</p> : !data.candidates.length
            ? <p className="meta">No dev world is a copy of {live.name} (a dev world whose upstream is {live.key}): nothing to import. Make one with
                <code> python3 tools/world_db.py new KEY --upstream {live.key} --from {live.key}</code>.</p>
            : <>
                <div className="sync-row">
                    <label className="field"><span>Dev world</span>
                        <select value={dev ?? ''} disabled={busy} onChange={e => { setPicks({}); setDone(null); void load(e.target.value); }}>
                            {data.candidates.map(w => <option key={w.key} value={w.key}>{w.name} · {w.key}</option>)}</select></label>
                    <button disabled={loading || busy} onClick={() => void load(dev)}>{loading ? 'Comparing…' : 'Compare again'}</button>
                </div>
                {data.problem && <Problems title={data.problem} items={data.errors} />}
                {why && <p className="sync-blocked">{why}</p>}
                {done && <div className="publish-done">Release {done.release} is live{done.conflicts ? ` (${done.conflicts} conflict${done.conflicts === 1 ? '' : 's'} chosen)` : ''}{done.replacedMap ? `, on the new map (${done.map?.name ?? 'dev\'s'})` : ''}.
                    {done.summary.letGo && letGoText(done.summary.letGo) && <p className="meta">With the old map: {letGoText(done.summary.letGo)}.</p>}
                    <SummaryTable summary={done.summary} />
                    {done.dev.error ? <p className="hint error-text">{done.dev.error}</p>
                        : <p className="meta">The dev world was brought level with live ({done.dev.updated} row{done.dev.updated === 1 ? '' : 's'})
                            {done.dev.editedMeanwhile?.length ? `; ${done.dev.editedMeanwhile.length} edited there meanwhile kept their edits` : ''}.</p>}
                    <p><b>Remember to end maintenance</b> (above) to turn the server back on.</p></div>}
                {problem && <Problems title={problem.message} items={problem.errors} />}
                {plan && <>
                    <div className="sync-counts">
                        <div><b>{plan.counts.dev}</b><span>dev changes to write</span></div>
                        <div><b>{plan.counts.live}</b><span>live changes kept</span></div>
                        <div className={plan.counts.conflicts ? 'warn' : ''}><b>{plan.counts.conflicts}</b><span>conflicts</span></div>
                    </div>
                    <p className="meta">Live release {plan.liveRelease || 'none yet'} · dev revision {plan.devRevision} · base: release {plan.base.release || 0},
                        {' '}{plan.base.how} by {plan.base.by || 'someone'} {when(plan.base.takenAt)}</p>
                    {replacing && <div className="sync-warning">
                        <p><b>A different map.</b> {replacing.warning}</p>
                        {letGoText(plan.letGo) && <p className="meta">Kept, but standing on the old map: {letGoText(plan.letGo)} (below, under Conflicts).</p>}
                        <label><input type="checkbox" checked={replace} disabled={busy} onChange={e => setReplace(e.target.checked)} /> {replacing.confirm}</label>
                    </div>}
                    {plan.errors.length > 0 && <Problems title="Fix these in Atlas before importing:" items={plan.errors} />}
                    {blocked.length > 0 && <Problems title="Not yet:" items={blocked} />}
                    {plan.warnings.length > 0 && <ul className="hint">{plan.warnings.slice(0, 12).map((w, i) => <li key={i}>{w}</li>)}</ul>}
                    <h4>What will be written to live</h4>
                    <SummaryTable summary={plan.summary.dev} empty="Nothing from dev." />
                    <ChangeGroups items={plan.devChanges} side="dev" empty="Dev changed nothing that live didn't." />
                    <h4>Conflicts <span className="meta">changed on both sides since the last pull: choose for each</span></h4>
                    <ConflictList conflicts={plan.conflicts} picks={picks} onPicks={setPicks} labels={LABELS} disabled={busy} areas={plan.areas} />
                    <details className="sync-kept-list"><summary>Live's own changes, kept ({plan.counts.live})</summary>
                        <ChangeGroups items={plan.liveChanges} side="live" empty="None." /></details>
                    <div className="sync-row">
                        <label className="field grow"><span>What changed (for the release history)</span>
                            <input value={note} maxLength={400} placeholder="e.g. Three new houses in Ridgemere." onChange={e => setNote(e.target.value)} /></label>
                        <label className="field"><span>Publish password</span>
                            <input type="password" autoComplete="off" value={password} onChange={e => setPassword(e.target.value)} /></label>
                    </div>
                    <div className="sync-apply">
                        {missing.length > 0 && <span className="hint">{missing.length} conflict{missing.length === 1 ? '' : 's'} still to choose.</span>}
                        <button className="publish" disabled={!canAct || busy || loading || !!why || blocked.length > 0 || missing.length > 0 || !password
                                || (!!replacing && !replace)}
                            title={why || blocked[0] || (missing.length ? 'Choose for every conflict first' : replacing && !replace
                                ? 'Confirm replacing the map first' : !password ? 'Enter the publish password' : '')}
                            onClick={() => void run()}>{busy ? 'Importing…' : replacing ? `⇪ Replace ${live.name}'s map` : `⇪ Import into ${live.name}`}</button>
                    </div>
                </>}
            </>}
    </section>;
}

const worldLabel = (worlds: SyncWorld[] | undefined, key: Target) => worlds?.find(w => w.key === key)?.name ?? key;

// --------------------------------------------------------------------------- The live world switch

function SwitchPanel({status, maintenance, canAct}: {status: ServerStatus; maintenance: boolean; canAct: boolean}) {
    // Every other registered world: the live ones, and dev worlds (served as live: DEV while PROD is closed).
    const others: World[] = status.switchTo ?? [];
    const [chosen, setChosen] = useState(''), [typed, setTyped] = useState('');
    const [busy, setBusy] = useState(false), [problem, setProblem] = useState('');
    const run = async () => {
        setBusy(true); setProblem('');
        try {
            const result = await dmApi.switchLive(chosen, typed);
            window.alert(`The live world is now ${result.name || result.live} (was ${result.was}). It is in maintenance: end maintenance to open it.`);
            window.location.reload();                     // The top bar's live world changed.
        } catch (error) { setProblem((error as Error).message); setBusy(false); }
    };
    return <section className="sync-card">
        <h3>Switch the live server's world</h3>
        <p className="meta">The live server's world is <b>{status.live}</b>: what <code>tools/live.sh server</code> runs when given no world.
            (The DM's own world is chosen in the top bar, apart from it and from Atlas's.)</p>
        <p className="sync-warning">{status.warning || 'Switching the live world is rarely wise: players\' characters and progress belong to each world.'}</p>
        {!others.length ? <p className="meta">There is no other world to switch to (python3 tools/world_db.py new KEY --kind live ...).</p>
            : status.key !== status.live ? <p className="meta">Switched from the live server's own world: choose {status.live} in the DM's world picker.</p>
            : !maintenance ? <p className="meta">Only during maintenance: start maintenance on {status.name} first.</p>
            : <>
                <label className="field"><span>New live world</span>
                    <select value={chosen} disabled={busy} onChange={e => { setChosen(e.target.value); setTyped(''); }}>
                        <option value="">Choose…</option>
                        {others.map(w => <option key={w.key} value={w.key}>{w.name} · {w.key}{w.kind === 'dev' ? ' (a dev world, served as live)' : ''}</option>)}</select></label>
                {chosen && <label className="field"><span>Type <code>{chosen}</code> to confirm</span>
                    <input value={typed} autoComplete="off" onChange={e => setTyped(e.target.value)} /></label>}
                {problem && <Problems title={problem} items={[]} />}
                <button className="publish" disabled={!canAct || busy || !chosen || typed !== chosen} onClick={() => void run()}>
                    {busy ? 'Switching…' : 'Switch the live world'}</button>
                <p className="hint">The new world starts in maintenance; the launcher starts the server on it when you end maintenance there.</p>
            </>}
    </section>;
}

// --------------------------------------------------------------------------- Releases

function ReleasesPanel({world, release, canAct, onRolledBack}: {world: Target; release: number; canAct: boolean; onRolledBack: () => void}) {
    const [releases, setReleases] = useState<Release[] | null>(null);
    const [password, setPassword] = useState(''), [note, setNote] = useState('');
    const [busy, setBusy] = useState(false), [problem, setProblem] = useState(''), [done, setDone] = useState('');
    const load = useCallback(() => dmApi.releases(world).then(r => setReleases(r.releases)).catch(e => setProblem((e as Error).message)), [world]);
    useEffect(() => { void load(); }, [load, release]);
    const back = async (r: Release) => {
        if (!window.confirm(`Put release #${r.number}'s map and authored content back live on ${world}?\n\nLive NPCs and players' data are not changed; the dev worlds are not changed. It records a new release.`)) return;
        setBusy(true); setProblem(''); setDone('');
        try {
            const result = await dmApi.rollback(world, r.number, password, note);
            setDone(`Rolled back to #${r.number}: release ${result.release} is live.`); setPassword(''); setNote('');
            onRolledBack(); await load();
        } catch (error) {
            setProblem((error as Error).message);
            if (error instanceof DmError && error.status === 401) setPassword('');
        } finally { setBusy(false); }
    };
    return <section className="sync-card wide">
        <h3>Releases</h3>
        {problem && <Problems title={problem} items={[]} />}
        {done && <p className="publish-done">{done}</p>}
        {!releases ? <p className="hint">Reading…</p> : !releases.length ? <p className="meta">No releases yet.</p> : <>
            <p className="hint">Rollback puts a release's authored content (the map, interiors, places, doors, chapters) back live; it is a release of its own.</p>
            <div className="sync-row">
                <label className="field grow"><span>Note (optional)</span><input value={note} maxLength={400} onChange={e => setNote(e.target.value)} /></label>
                <label className="field"><span>Publish password</span><input type="password" autoComplete="off" value={password} onChange={e => setPassword(e.target.value)} /></label>
            </div>
            <ol className="releases">{releases.map((r, i) => <li key={r.number}>
                <div><b>#{r.number}</b> {r.kind === 'rollback' ? `rolled back to #${r.restored}` : r.kind} · {r.by || 'someone'} · {when(r.at)}
                    {r.note && <p>{r.note}</p>}<small><SummaryTable summary={r.summary} empty="" /></small></div>
                {i > 0 && <button disabled={!canAct || busy || !password} title={password ? `Put release ${r.number}'s world back live` : 'Enter the publish password first'}
                    onClick={() => void back(r)}>Roll back to this</button>}
            </li>)}</ol>
        </>}
    </section>;
}
