// My world (Docs/Design/78, sections 2, 7, 10.5 and 12; Phases M5 and M9): a host's own world in the Editor window. Its
// status with Close and Open and Restart, its backups (a backup now, pins, "Copy restore steps"), Share my world (a file
// of the world without its players, in the world folder's manage/shared/), the settings it runs with, the AI's
// use, its version and certificate, the history of changes, sign-ins; for the owner also the world folder and the lines
// of a server that keeps stopping. Everyone signed in reads it; the actions are the owner's (others see them greyed,
// with why). A restore itself is never done from here: it runs on the world's computer (restore.bat or restore.sh).
import {useCallback, useEffect, useState} from 'react';
import {atLeast, dmApi, type BackupItem, type Me, type MyWorld, type ShareState} from './api';
import {actorName} from '../lib/hosted';
import {greyed} from '../lib/acts';
import {ago, BACKUP_WHY, backupProblem, historyWords, lastBackupLine, restoreSteps, SHARE_HOLDS, SHARE_NEVER, SHARE_NOT_YET, sharedPath,
    shareProblem, size, syncedFolder, windowsFolder} from '../lib/myWorld';
import {HostedServerPanel} from './ServerTab';
import {unpublishedLine} from '../lib/publish';

const at = (seconds: number | null | undefined) => (seconds ? new Date(seconds * 1000).toLocaleString() : '—');
const CHEATS: Record<string, string> = {
    off: 'off',
    host: '"host": your tools may give Gifts and tiers and reward your own wolves (listed "cheats on")',
    everyone: '"everyone": every player has the cheats (listed "cheats on")',
};

export function MyWorldTab({me, initial}: {me: Me; initial?: MyWorld}) {
    const [world, setWorld] = useState<MyWorld | null>(initial ?? null), [problem, setProblem] = useState('');
    const load = useCallback(() => dmApi.myWorld().then(w => { setWorld(w); setProblem(''); return w; })
        .catch(e => { setProblem((e as Error).message); return null; }), []);
    useEffect(() => {
        void load();
        const t = setInterval(() => void load(), 30_000);
        return () => clearInterval(t);
    }, [load]);
    if (!world) return <div className="dm-center"><p className="hint">{problem || 'Reading your world…'}</p></div>;
    const owner = world.owner && atLeast(me, 'owner');
    const ownerOnly = owner ? '' : 'Only the world\'s owner can do this.';
    return <div className="sync-page my-world">
        <div className="dm-sheet-bar">
            <strong>My world · {world.name}</strong>
            <span className="meta hint" data-my-world-line>{world.wolves === null ? 'Its game server isn\'t up' : `${world.wolves} ${world.wolves === 1 ? 'wolf' : 'wolves'} in the world`}
                {' · '}{lastBackupLine(world.backups?.last ? Date.now() / 1000 - world.backups.last.at : null)}</span>
            <button onClick={() => void load()}>Refresh</button>
        </div>
        {problem && <p className="error">{problem}</p>}
        <div className="sync-body">
            <HostedServerPanel status={world.status} canAct={owner} onChanged={s => setWorld({...world, status: s})}
                reload={() => load().then(w => w?.status ?? null)} />
            {!owner && <p className="hint">Closing, opening and restarting the world, and its backups, are for its owner. You see them as they are.</p>}
            <BackupsCard world={world} owner={owner} ownerOnly={ownerOnly} reload={load} />
            <ShareCard world={world} owner={owner} ownerOnly={ownerOnly} reload={load} />
            {world.map && <section className="sync-card" data-map>
                <h3>The map</h3>
                <p>{unpublishedLine(world.map.unpublished)}.</p>
                <p className={world.map.memory.ok ? 'hint' : 'error'}>{world.map.memory.words}</p>
                <p className="hint">Map changes are made in the map editor (Atlas, at the top) and reach players with Publish, on its Publish page.</p>
            </section>}
            <section className="sync-card">
                <h3>Settings</h3>
                <table className="dm-table"><tbody>
                    <tr><th>Players at once</th><td>{world.settings.player_limit ?? 'the default (40)'}</td></tr>
                    <tr><th>Worker threads</th><td>{world.settings.workers ?? 'chosen by the world (its computer\'s CPUs)'}</td></tr>
                    <tr><th>Cheats</th><td>{CHEATS[world.settings.cheats] ?? world.settings.cheats}</td></tr>
                    <tr><th>The Editor</th><td>{world.settings.editor.enabled ? 'on' : 'off'}</td></tr>
                </tbody></table>
                <p className="hint">These are world.toml's, in the world folder. Change them there, then Restart (above).</p>
            </section>
            <section className="sync-card">
                <h3>AI use</h3>
                {world.ai.available ? <p>On the world's AI key{world.ai.model ? ` (${world.ai.model})` : ''}.</p> : <p className="hint">{world.ai.reason}</p>}
                <p className="meta">Today: {world.ai.today.story} of {world.ai.caps.story} life stories, {world.ai.today.roster} of {world.ai.caps.roster} roster generations.</p>
                {world.ai.month && <p className="meta">This month: {world.ai.month.tools.toLocaleString()} tokens by your tools and {world.ai.month.mind.toLocaleString()} by
                    the NPCs' voices, of {world.ai.month.budget.toLocaleString()} ([ai] monthly_tokens in world.toml).</p>}
            </section>
            <section className="sync-card">
                <h3>Version and certificate</h3>
                <p>Package {world.package || '(unknown)'}</p>
                <p className="meta">The Editor certificate's fingerprint:</p>
                <p><code data-fingerprint>{world.fingerprint || '(none yet)'}</code></p>
                <p className="hint">The same as manage/editor-certificate.txt in the world folder.</p>
            </section>
            {owner && <OwnerCard world={world} />}
            <section className="sync-card wide">
                <h3>History</h3>
                {world.history.length ? <table className="dm-table"><thead><tr><th>When</th><th>Who</th><th>What</th><th>Detail</th></tr></thead>
                    <tbody>{world.history.map((h, i) => <tr key={i}><td>{at(h.at)}</td><td>{actorName(h.who) || '—'}</td>
                        <td>{historyWords(h.action)}{h.target && h.target !== world.world ? ` · ${h.target}` : ''}</td><td>{h.detail}</td></tr>)}</tbody></table>
                    : <p className="hint">Nothing has been changed with the Editor yet.</p>}
            </section>
            <section className="sync-card wide">
                <h3>{owner ? 'Sign-ins' : 'Your sign-ins'}</h3>
                {world.signIns.length ? <table className="dm-table"><thead><tr><th>When</th><th>Account</th><th>What</th></tr></thead>
                    <tbody>{world.signIns.map((s, i) => <tr key={i}><td>{at(s.at)}</td><td>{s.account}</td><td>{s.detail}</td></tr>)}</tbody></table>
                    : <p className="hint">None yet.</p>}
            </section>
            <HelpCard />
        </div>
    </div>;
}

/** The backups: the newest, a backup now, pins, and the steps that put the world back to one (doc 78, 12.1-12.2). */
function BackupsCard({world, owner, ownerOnly, reload}: {world: MyWorld; owner: boolean; ownerOnly: string; reload: () => Promise<MyWorld | null>}) {
    const [note, setNote] = useState(''), [busy, setBusy] = useState(false), [asked, setAsked] = useState<string | null>(null);
    const [steps, setSteps] = useState<BackupItem | null>(null), [copied, setCopied] = useState('');
    const b = world.backups;
    // A backup asked for: read again every 5 s until a newer one appears (two minutes at most).
    useEffect(() => {
        if (asked === null) return;
        if ((b?.last?.name ?? '') !== asked) { setAsked(null); setNote('The backup is made.'); return; }
        const t = setInterval(() => void reload(), 5000), stop = setTimeout(() => setAsked(null), 120_000);
        return () => { clearInterval(t); clearTimeout(stop); };
    }, [asked, b?.last?.name, reload]);
    if (!b) return <section className="sync-card"><h3>Backups</h3>
        <p className="hint">This world has no backup service yet: take the new compose.yaml from the package (keep .env and world.toml),
            then in the world folder: docker compose up -d.</p></section>;
    const now = Date.now() / 1000;
    const nowWhy = greyed([!owner, ownerOnly], [busy, 'Asking…'], [asked !== null || !!b.working, 'A backup is being made now.']);
    const backUp = async () => {
        setBusy(true); setNote('');
        try { await dmApi.backupNow(); setAsked(b.last?.name ?? ''); setNote('Asked: the backup service makes it within a minute.'); }
        catch (error) { setNote((error as Error).message); }
        finally { setBusy(false); }
    };
    const pin = async (item: BackupItem, on: boolean) => {
        if (on && !window.confirm(`Pin the backup of ${at(item.at)}?\n\nA pinned backup is kept for 90 days, whatever newer ones come (at most ${b.pinCap} at once).`)) return;
        setBusy(true); setNote('');
        try { await dmApi.pin(item.name, on); setNote(on ? 'Pinned: the backup service keeps it for 90 days.' : 'Unpinned.'); setTimeout(() => void reload(), 6000); }
        catch (error) { setNote((error as Error).message); }
        finally { setBusy(false); }
    };
    const copy = (text: string, which: string) => {
        void navigator.clipboard?.writeText(text).then(() => setCopied(which), () => setCopied(''));
    };
    const shown = steps ? restoreSteps(world.folder ?? '', steps.name) : null;
    const windowsFirst = windowsFolder(world.folder ?? '');
    return <section className="sync-card wide">
        <h3>Backups</h3>
        <p data-last-backup>{lastBackupLine(b.last ? now - b.last.at : null)}{b.last ? ` (${BACKUP_WHY[b.last.kind] ?? b.last.kind}, ${size(b.last.size)})` : ''}
            {b.working ? ` · making one now (${BACKUP_WHY[b.working] ?? b.working})…` : ''}</p>
        {b.problem && <p className="error" data-backup-problem>{backupProblem(b.problem, b.disk.minFree)}</p>}
        <p className="meta">The world backs itself up when it starts and every day, before an update, and when you ask. They are in the world
            folder's backups: {size(b.disk.used)} in all{b.disk.free !== null ? `, ${size(b.disk.free)} free on that disk` : ''}. Kept: the 7 newest daily
            ones, the 5 newest of the rest, and up to {b.pinCap} you pin ({b.pins} pinned now), each for 90 days.</p>
        <button disabled={!!nowWhy} title={nowWhy || undefined} data-action="backup-now" onClick={() => void backUp()}>⤓ Back up now</button>
        {note && <p className="hint">{note}</p>}
        {b.backups.length > 0 && <table className="dm-table"><thead><tr><th>When</th><th>Why</th><th>Size</th><th>Package</th><th></th><th></th></tr></thead>
            <tbody>{b.backups.map(item => {
                const pinned = !!item.pinnedUntil && item.pinnedUntil > now;
                const pinWhy = greyed([!owner, ownerOnly], [busy, 'Working…'], [!pinned && b.pins >= b.pinCap, `${b.pinCap} backups are pinned already: unpin one first.`]);
                const stepsWhy = greyed([!owner, 'The restore steps are for the world\'s owner, on the world\'s computer.']);
                return <tr key={item.name} data-backup={item.name}>
                    <td title={item.name}>{at(item.at)} <span className="meta">({ago(now - item.at)})</span></td>
                    <td>{BACKUP_WHY[item.kind] ?? item.kind}{pinned ? ` · pinned until ${new Date(item.pinnedUntil! * 1000).toLocaleDateString()}` : ''}</td>
                    <td>{size(item.size)}</td><td>{item.package || '?'}</td>
                    <td><button className="small" disabled={!!pinWhy} title={pinWhy || undefined} onClick={() => void pin(item, !pinned)}>{pinned ? 'Unpin' : 'Pin'}</button></td>
                    <td><button className="small" disabled={!!stepsWhy} title={stepsWhy || undefined} data-action="restore-steps"
                        onClick={() => { setSteps(steps?.name === item.name ? null : item); setCopied(''); }}>Copy restore steps</button></td>
                </tr>;
            })}</tbody></table>}
        {shown && steps && <div className="my-world-steps" data-restore-steps>
            <p>To put the world back to {at(steps.at)}: on the world's computer, open {windowsFirst ? 'PowerShell' : 'a terminal'} and paste these lines.
                restore {windowsFirst ? '.bat' : '.sh'} says what you would lose and asks before it changes anything. Everything players did since then is lost.</p>
            {(windowsFirst ? ['powershell', 'bash'] as const : ['bash', 'powershell'] as const).map(shell => <div key={shell}>
                <p className="meta">{shell === 'powershell' ? 'Windows (PowerShell)' : 'Linux or macOS (a terminal)'}</p>
                <pre>{shown[shell]}</pre>
                <button className="small" onClick={() => copy(shown[shell], shell)}>{copied === shell ? 'Copied' : 'Copy'}</button>
            </div>)}
            {!world.folder && <p className="hint">The world doesn't know its folder's path: start the lines from inside the world folder.</p>}
        </div>}
        <p className="hint">A restore runs only on the world's computer, never from the Editor: in the world folder, double-click restore.bat
            (Windows), or run ./restore.sh. It lists the backups, asks which one, makes a safety copy first and keeps your Editor accounts unless you say.</p>
    </section>;
}

/** Share my world (doc 78, 10.5; M9): a file of the world without its players, made by the world itself into the world
 *  folder's manage/shared/. A confirmation first, saying what goes in the file and what never does; then "Making the
 *  file…" until it is ready (its name and where it is) or failed (why). The owner's; others see it greyed. */
function ShareCard({world, owner, ownerOnly, reload}: {world: MyWorld; owner: boolean; ownerOnly: string; reload: () => Promise<MyWorld | null>}) {
    const [asking, setAsking] = useState(false), [busy, setBusy] = useState(false), [note, setNote] = useState('');
    const [asked, setAsked] = useState<number | null>(null), [copied, setCopied] = useState(false);
    const share: ShareState | null | undefined = world.share;
    const working = !!share && (share.state === 'working' || share.waiting);
    // Asked, or being made: read again every 5 s until it is ready or failed (20 minutes at most). `asked` is the state's
    // own time when the request went (the world's clock, not this computer's): a newer state that isn't working ends it.
    useEffect(() => {
        if (asked === null && !working) return;
        if (asked !== null && share && !working && (share.at ?? 0) !== asked) { setAsked(null); return; }
        const t = setInterval(() => void reload(), 5000), stop = setTimeout(() => setAsked(null), 20 * 60_000);
        return () => { clearInterval(t); clearTimeout(stop); };
    }, [asked, working, share, reload]);
    if (share === undefined || share === null) return <section className="sync-card wide"><h3>Share my world</h3>
        <p className="hint">This world's package is older than Share my world: update it (keep world.toml, .env, manage and backups), then
            docker compose up -d.</p></section>;
    const why = greyed([!owner, ownerOnly], [busy, 'Asking…'], [working || asked !== null, 'Your world\'s file is being made now.'],
        [share.thisHour >= share.perHour, `Your world has been shared ${share.thisHour} times this hour, the most an hour: try again later.`]);
    const make = async () => {
        setBusy(true); setNote('');
        try { await dmApi.shareWorld(); setAsked(share.at ?? 0); setAsking(false); void reload(); }
        catch (error) { setNote((error as Error).message); }
        finally { setBusy(false); }
    };
    const ready = share.state === 'ready' && share.file && !working && asked === null;
    const where = ready ? sharedPath(world.folder ?? '', share.file!) : '';
    return <section className="sync-card wide" data-share>
        <h3>Share my world</h3>
        <p>Makes one file of your world, without its players, for someone else to run or look at. The world makes it itself, into the
            folder <code>manage{windowsFolder(world.folder ?? '') ? '\\' : '/'}shared</code> in your world folder. Never send the world
            folder itself (it holds your host token, your Editor codes and every player's data in its backups).</p>
        {!asking && <button disabled={!!why} title={why || undefined} data-action="share" onClick={() => { setAsking(true); setNote(''); }}>⇪ Share my world…</button>}
        {asking && <div className="my-world-steps" data-share-confirm>
            <p><strong>The file holds:</strong></p>
            <ul>{SHARE_HOLDS.map(line => <li key={line}>{line}</li>)}</ul>
            <p><strong>It never holds:</strong></p>
            <ul>{SHARE_NEVER.map(line => <li key={line}>{line}</li>)}</ul>
            <p className="hint">{SHARE_NOT_YET}</p>
            <p className="hint">Making it takes a minute or two; the world keeps running. The newest {share.kept} files are kept, older ones are removed.</p>
            <button disabled={!!why} title={why || undefined} data-action="share-make" onClick={() => void make()}>Make the file</button>
            {' '}<button onClick={() => setAsking(false)}>Cancel</button>
        </div>}
        {(working || asked !== null) && <p data-share-state="working">Making the file… The world is exported with every player taken out, then
            checked again before it is written. This page says when it is ready.</p>}
        {ready && <div data-share-state="ready">
            <p>Ready: <strong>{share.file}</strong> ({size(share.size)}), made {share.at ? ago(Date.now() / 1000 - share.at) : ''}.</p>
            <p className="meta">It is in your world folder: <code data-share-path>{where}</code>
                {' '}<button className="small" onClick={() => void navigator.clipboard?.writeText(where).then(() => setCopied(true), () => setCopied(false))}>
                    {copied ? 'Copied' : 'Copy the path'}</button></p>
            <p className="hint">Send it like any large file. Whoever gets it can't sign in to your world with it: it holds no account and no key.</p>
        </div>}
        {share.state === 'failed' && !working && asked === null && <p className="error" data-share-state="failed">{shareProblem(share.problem)}</p>}
        {note && <p className="error">{note}</p>}
        {share.files.length > 0 && <table className="dm-table"><thead><tr><th>In manage/shared</th><th>Size</th><th>Made</th></tr></thead>
            <tbody>{share.files.map(f => <tr key={f.name}><td>{f.name}</td><td>{size(f.size)}</td><td>{f.at ? ago(Date.now() / 1000 - f.at) : '—'}</td></tr>)}</tbody></table>}
    </section>;
}

/** The owner's own views: the world folder (is it a synced one?) and a server that keeps stopping. */
function OwnerCard({world}: {world: MyWorld}) {
    const exits = world.exits ?? [];
    const keepsStopping = (world.status.hostServer?.failures ?? 0) >= 2;
    return <section className="sync-card">
        <h3>The world folder</h3>
        <p><code data-folder>{world.folder || '(not known: the world was started without its path)'}</code></p>
        {world.folder && syncedFolder(world.folder) && <p className="error">This looks like a folder a cloud service copies. The backups in it hold players'
            data and your Editor accounts' password hashes: move the world folder out of it (stop the world first, keep every file).</p>}
        <p className="hint">Only you see this. Keep the folder, and its backups, private.</p>
        {exits.length > 0 && <>
            <h3>{keepsStopping ? 'The world keeps stopping' : 'The game server\'s last stops'}</h3>
            {exits.slice().reverse().map((e, i) => <details key={i} open={i === 0 && keepsStopping}>
                <summary>{at(e.at)}: exit {e.code ?? '?'}</summary>
                <pre>{e.lines.join('\n') || '(no lines)'}</pre></details>)}
            <p className="hint">Addresses and anything like a key are left out. In the world folder, docker compose logs world shows the rest.</p>
        </>}
    </section>;
}

/** Doc 78, 12.6: what fixes what, in the host's words. */
function HelpCard() {
    const rows: [string, string, string][] = [
        ['An NPC action was a mistake', 'Its opposite, where one exists: each confirmation says whether there is one', 'Nothing'],
        ['A player\'s wolf was hurt by mistake', 'Resurrect it, or move it back (Players, or LIVE)', 'Their lost time'],
        ['The world keeps stopping', 'Its last lines (above, for the owner); docker compose logs world; a restore if nothing else helps', 'Depends'],
        ['Everything is broken', 'restore.bat or restore.sh in the world folder, with a backup ("Copy restore steps")', 'Everything every player did since that backup'],
        ['A lost password, or "too many tries"', 'Double-click reset-editor-password in the world folder, on the world\'s computer; or the owner resets a co-builder in Editor accounts', 'Nothing'],
        ['Someone you don\'t know signed in', 'Editor accounts: End all Editor sessions, then reset or remove the account', 'Nothing'],
        ['"ports are not available"', 'RATW_EDITOR_PORT=17800 (or another) in .env, then docker compose up -d', 'Nothing'],
    ];
    return <section className="sync-card wide">
        <h3>Something went wrong?</h3>
        <table className="dm-table"><thead><tr><th>What went wrong</th><th>What fixes it</th><th>What it costs</th></tr></thead>
            <tbody>{rows.map(([what, fix, cost]) => <tr key={what}><td>{what}</td><td>{fix}</td><td>{cost}</td></tr>)}</tbody></table>
    </section>;
}
