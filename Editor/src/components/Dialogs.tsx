import {useEffect, useRef, useState, type ReactNode} from 'react';
import * as M from '../model/model.mjs';
import {api, PublishRefusal, type NpcCopy, type PlaytestResult, type PublishPreview, type PublishSummary} from '../lib/api';
import {COMMANDS} from '../lib/commands';
import {EDITOR_COLORS} from '../lib/live';
import {setIdentity} from '../lib/session';
import {commit, getState, openView, setState, toast, useStore} from '../lib/store';
import {Hint, NumberField, Row, TextField, Toggle} from './fields';
import {GenerateDialog} from './Characters';

/** `required`: cannot be dismissed (Esc, backdrop, ×) until its own action closes it. */
function Modal({title, eyebrow, children, wide = false, required = false, onClose}: {title: string; eyebrow: string; children: ReactNode;
        wide?: boolean; required?: boolean; onClose?: () => void}) {
    const ref = useRef<HTMLDialogElement>(null);
    useEffect(() => { ref.current?.showModal(); }, []);
    return (
        <dialog ref={ref} className={wide ? 'modal wide' : 'modal'} onClose={() => { onClose?.(); setState({dialog: null}); }}
            onCancel={e => { if (required) e.preventDefault(); }}
            onPointerDown={e => { if (!required && e.target === ref.current) ref.current?.close(); }}>
            <div className="modal-head"><small>{eyebrow}</small><h2>{title}</h2>
                {!required && <button className="icon" onClick={() => ref.current?.close()} aria-label="Close">×</button>}</div>
            <div className="modal-body">{children}</div>
        </dialog>
    );
}
const done = () => setState({dialog: null});

export function Dialogs() {
    const dialog = useStore(s => s.dialog);
    switch (dialog) {
        case 'identity': return <IdentityDialog />;
        case 'conflict': return <ConflictDialog />;
        case 'publish': return <PublishDialog />;
        case 'cut': return <CutDialog />;
        case 'split': return <SplitDialog />;
        case 'room': return <RoomDialog />;
        case 'politics': return <PoliticsDialog />;
        case 'play': return <PlayDialog />;
        case 'shortcuts': return <ShortcutsDialog />;
        case 'about': return <AboutDialog />;
        case 'generate': return <GenerateDialog />;
        default: return null;
    }
}

function IdentityDialog() {
    const known = getState().identity;
    const [name, setName] = useState(known?.name ?? '');
    const [color, setColor] = useState(known?.color ?? EDITOR_COLORS[0]);
    const ok = name.trim().length > 0;
    return <Modal eyebrow="LIVE EDITING" title="Who is editing?" required={!known}>
        <Hint>Everyone edits the one world together, and every change saves as you make it. Others see your name and color on the map
            where you are working, and your name if an edit of theirs collides with yours.</Hint>
        <TextField label="Your name" value={name} max={60} placeholder="e.g. Martin" onCommit={setName} />
        <div className="field"><span>Your color</span><div className="swatches">
            {EDITOR_COLORS.map(c => <button key={c} className={c === color ? 'swatch on' : 'swatch'} style={{background: c}}
                aria-label={`Color ${c}`} aria-pressed={c === color} onClick={() => setColor(c)} />)}</div></div>
        <div className="modal-actions">
            <button className="primary" disabled={!ok} onClick={() => ok && setIdentity({name: name.trim(), color})}>Start editing</button></div>
    </Modal>;
}

function ConflictDialog() {
    const notice = useStore(s => s.conflict);
    if (!notice) return null;
    const ago = (at: string | null) => {
        if (!at) return '';
        const s = Math.max(0, Math.round((Date.now() - Date.parse(at)) / 1000));
        return s < 60 ? `${s} s ago` : s < 3600 ? `${Math.round(s / 60)} min ago` : new Date(at).toLocaleString();
    };
    const people = [...new Set(notice.conflicts.map(c => c.editor).filter(Boolean))];
    return <Modal eyebrow="EDIT CONFLICT" title={people.length ? `${people.join(' and ')} got there first` : 'Your change was not saved'}
        onClose={() => setState({conflict: null})}>
        <p>Your change <b>“{notice.action}”</b> was not saved{notice.conflicts.length ? ', because it would have overwritten someone else\'s work:' : '.'}</p>
        {notice.conflicts.length ? <ul className="conflicts">
            {notice.conflicts.slice(0, 8).map(c => <li key={c.key}><b>{c.editor || 'Someone'}</b> changed {c.label} {ago(c.at)}</li>)}
            {notice.conflicts.length > 8 && <li>…and {notice.conflicts.length - 8} more</li>}
        </ul> : <p className="hint error-text">{notice.message}</p>}
        <Hint>The map now shows the current version. Make your change again if it still makes sense.</Hint>
        <div className="modal-actions"><button className="primary" onClick={done}>OK</button></div>
    </Modal>;
}

/** "3 added, 1 changed" for one line of a publish summary; '' when nothing happens to it. */
const countsText = (counts: Record<string, number>) => Object.entries(counts).filter(([, n]) => n).map(([k, n]) => `${n} ${k}`).join(', ');

function SummaryTable({summary}: {summary: PublishSummary}) {
    const rows = [...Object.entries(summary.world), ...Object.entries(summary.live)].map(([what, counts]) => [what, countsText(counts)]).filter(([, t]) => t);
    if (!rows.length) return <p className="meta">Nothing changes.</p>;
    return <table className="summary"><tbody>{rows.map(([what, text]) => <tr key={what}><th>{what}</th><td>{text}</td></tr>)}</tbody></table>;
}

function PublishDialog() {
    const me = useStore(s => s.identity);
    const pending = useStore(s => s.live.pending);
    const [info, setInfo] = useState<PublishPreview | null>(null);
    const [problem, setProblem] = useState<{message: string; errors: string[]} | null>(null);
    const [note, setNote] = useState(''), [password, setPassword] = useState(''), [busy, setBusy] = useState(false);
    const [done_, setDone] = useState('');
    const load = () => api.publish.preview().then(setInfo).catch(e => setProblem({message: (e as Error).message, errors: []}));
    useEffect(() => { void load(); }, [pending === 0]); // eslint-disable-line react-hooks/exhaustive-deps
    const copyText = (c: NpcCopy) => c.error ? `Copying the live NPCs into DEV failed: ${c.error}`
        : `DEV now has the live NPCs (${countsText(c.copied) || 'already up to date'})${c.skipped.length ? `; skipped ${c.skipped.length}: ${c.skipped.join(' ')}` : ''}.`;
    const pull = async () => {
        setBusy(true); setProblem(null); setDone('');
        try { const c = await api.publish.pull({editor}); setDone(copyText(c)); toast('Copied the live NPCs into DEV.', 'success'); }
        catch (error) { setProblem({message: (error as Error).message, errors: error instanceof PublishRefusal ? error.errors : []}); }
        finally { setBusy(false); }
    };
    const run = async (action: () => Promise<{release: number; pulled?: NpcCopy}>, what: string) => {
        setBusy(true); setProblem(null); setDone('');
        try {
            const result = await action();
            setDone(`${what}: release ${result.release} is live.${result.pulled ? ' ' + copyText(result.pulled) : ''}`);
            toast(`Release ${result.release} is live.`, 'success');
            setPassword(''); setNote(''); await load();
        } catch (error) {
            setProblem({message: (error as Error).message, errors: error instanceof PublishRefusal ? error.errors : []});
            if (error instanceof PublishRefusal && error.status === 401) setPassword('');
        } finally { setBusy(false); }
    };
    const editor = me?.name ?? 'Someone';
    const blocked = !info || info.errors.length > 0 || pending > 0;
    const when = (at: string) => new Date(at).toLocaleString();
    return <Modal eyebrow="PUSH TO LIVE" title="Publish the world to PROD" wide>
        <Hint>Copies the world from DEV to PROD, where the live game reads it. Terrain, places and doors become exactly DEV's.
            NPCs already live are never changed by a push (players, events and deaths shape them there): only new NPCs, jobs and routes are added,
            and afterwards DEV copies the live NPCs back so your next edits start from what is really happening. Every push is a numbered release you can roll back to.</Hint>
        {!info && !problem && <p className="hint">Comparing DEV with PROD…</p>}
        {info && <p className="meta">DEV world <b>{info.world}</b> at revision {info.devRevision} · live release {info.release || 'none yet'}</p>}
        {pending > 0 && <p className="hint">Waiting for your last {pending > 1 ? `${pending} changes` : 'change'} to save…</p>}
        {info && info.errors.length > 0 && <div className="publish-errors"><b>Fix these in DEV before publishing:</b>
            <ul>{info.errors.slice(0, 12).map((e, i) => <li key={i}>{e}</li>)}</ul></div>}
        {info && !info.errors.length && (info.changed ? <><b className="summary-title">This push will change:</b><SummaryTable summary={info.summary} /></>
            : <p className="meta">PROD already matches DEV.</p>)}
        {problem && <div className="publish-errors"><b>{problem.message}</b>
            {problem.errors.length > 0 && <ul>{problem.errors.slice(0, 12).map((e, i) => <li key={i}>{e}</li>)}</ul>}</div>}
        {done_ && <p className="publish-done">{done_}</p>}
        <TextField label="What changed (for the release history)" value={note} max={2000} multiline placeholder="e.g. Opened the east fields and the mill." onCommit={setNote} />
        <label className="field"><span>Publish password</span>
            <input type="password" autoComplete="off" value={password} onChange={e => setPassword(e.target.value)}
                onKeyDown={e => { if (e.key === 'Enter' && password && !blocked && info?.changed) void run(() => api.publish.push({password, note, editor}), 'Pushed'); }} /></label>
        <div className="modal-actions"><button onClick={done}>Close</button>
            <button disabled={busy || !info?.release} title="Make DEV's NPCs, their state and the economy match PROD (only DEV changes)"
                onClick={pull}>Copy NPC state from live</button>
            <button className="publish" disabled={busy || blocked || !info?.changed || !password}
                onClick={() => run(() => api.publish.push({password, note, editor}), 'Pushed')}>{busy ? 'Publishing…' : '⇪ Push to live'}</button></div>
        {info && info.releases.length > 0 && <details className="releases" open>
            <summary>Release history ({info.releases.length})</summary>
            <ol>{info.releases.map((r, i) => <li key={r.number}>
                <div><b>#{r.number}</b> {r.kind === 'rollback' ? `rolled back to #${r.restored}` : 'push'} · {r.by || 'someone'} · {when(r.at)}
                    {r.note && <p>{r.note}</p>}<small>{countsText(Object.fromEntries(Object.entries(r.summary.world ?? {}).map(([k, c]) => [k, Object.values(c).reduce((a, b) => a + b, 0)])))}</small></div>
                {i > 0 && <button disabled={busy || !password} title={password ? `Put release ${r.number}'s world back live` : 'Enter the publish password first'}
                    onClick={() => { if (window.confirm(`Put release #${r.number}'s world back live? Live NPCs and the economy are not changed; DEV is not changed.`))
                        void run(() => api.publish.rollback({release: r.number, password, note, editor}), `Rolled back to #${r.number}`); }}>Roll back to this</button>}
            </li>)}</ol>
        </details>}
    </Modal>;
}

function CutDialog() {
    const project = useStore(s => s.project);
    const box = M.cellBounds(project) ?? {x: 0, y: 0, width: 0, height: 0};
    const [w, setW] = useState(Math.min(32, box.width || 32)), [h, setH] = useState(Math.min(24, box.height || 24));
    const count = Math.ceil(box.width / w) * Math.ceil(box.height / h);
    return <Modal eyebrow="PARTITION" title="Re-cut the cells">
        <Hint>Cells are the game's rooms: players see one at a time and cross edges to reach the next. Re-cutting never moves terrain, people or doors.
            It covers the rectangle around every cell, so empty ground between cells becomes plain ground in new cells.</Hint>
        <Row><NumberField label="Cell width" value={w} min={4} max={256} onCommit={setW} /><NumberField label="Cell height" value={h} min={4} max={256} onCommit={setH} /></Row>
        <p className="meta">Makes {count} cell{count > 1 ? 's' : ''} across the {box.width} × {box.height} tiles around the cells.</p>
        <div className="modal-actions"><button onClick={done}>Cancel</button>
            <button className="primary" onClick={() => { if (commit(`Cut into ${count} cells.`, p => M.cutGrid(p, w, h))) done(); }}>Cut</button></div>
    </Modal>;
}

function SplitDialog() {
    const s = getState().selection;
    const cell = s?.kind === 'cells' ? getState().project.cells.find(c => c.id === s.ids[0]) : undefined;
    const [axis, setAxis] = useState<'x' | 'y'>('x');
    const [offset, setOffset] = useState(cell ? Math.floor(cell.width / 2) : 8);
    if (!cell) return <Modal eyebrow="SPLIT" title="Select a world cell first"><Hint>Select one world cell on the overview, then split it.</Hint></Modal>;
    return <Modal eyebrow="SPLIT" title={`Split ${cell.name}`}>
        <div className="segmented"><button className={axis === 'x' ? 'on' : ''} onClick={() => { setAxis('x'); setOffset(Math.floor(cell.width / 2)); }}>Left | right</button>
            <button className={axis === 'y' ? 'on' : ''} onClick={() => { setAxis('y'); setOffset(Math.floor(cell.height / 2)); }}>Top / bottom</button></div>
        <NumberField label="First part size (tiles)" value={offset} min={4} max={(axis === 'x' ? cell.width : cell.height) - 4} onCommit={setOffset} />
        <div className="modal-actions"><button onClick={done}>Cancel</button>
            <button className="primary" onClick={() => { if (commit('Split the cell.', p => M.splitCell(p, cell.id, axis, offset))) done(); }}>Split</button></div>
    </Modal>;
}

function RoomDialog() {
    const [name, setName] = useState('New interior');
    const [w, setW] = useState(20), [h, setH] = useState(14);
    return <Modal eyebrow="INTERIOR" title="New blank interior">
        <Hint>For a ready-made building with walls, furniture and a street door, use the Building tool (U) instead.</Hint>
        <TextField label="Name" value={name} max={120} onCommit={setName} />
        <Row><NumberField label="Width" value={w} min={4} max={256} onCommit={setW} /><NumberField label="Height" value={h} min={4} max={256} onCommit={setH} /></Row>
        <div className="modal-actions"><button onClick={done}>Cancel</button>
            <button className="primary" onClick={() => {
                const room = commit('Created an interior. Connect it with a door (D).', p => {
                    const r = M.addRoom(p, w, h, name);
                    M.stamp(p, 0, 0, Array.from({length: h}, (_, y) => Array.from({length: w}, (_, x) => x === 0 || y === 0 || x === w - 1 || y === h - 1 ? '#' : '.').join('')), r.id);
                    return r;
                });
                if (room) { done(); openView({kind: 'cell', id: room.id}); setState({selection: {kind: 'cells', ids: [room.id]}}); }
            }}>Create</button></div>
    </Modal>;
}

function PoliticsDialog() {
    const project = useStore(s => s.project);
    const [faction, setFaction] = useState({id: '', name: '', color: '#7799bb'});
    const [chapter, setChapter] = useState({id: '', name: ''});
    return <Modal eyebrow="WORLD POLITICS" title="Factions & Chapters" wide>
        <Hint>IDs are permanent references (lowercase letters, digits, _ and -). Claims and Chapter sites are assigned per cell in the inspector.</Hint>
        <div className="two-col">
            <section><h3>Factions</h3>
                {project.factions.map(f => <div className="catalog-row" key={f.id}><i style={{background: f.color}} /><span>{f.name}<small>{f.id}</small></span>
                    <button className="icon" aria-label={`Delete ${f.name}`} onClick={() => commit(`Deleted ${f.name}.`, p => M.removeFaction(p, f.id))}>×</button></div>)}
                <Row><TextField label="ID" value={faction.id} max={48} onCommit={v => setFaction({...faction, id: v})} /><TextField label="Name" value={faction.name} max={120} onCommit={v => setFaction({...faction, name: v})} /></Row>
                <label className="field"><span>Color</span><input type="color" value={faction.color} onChange={e => setFaction({...faction, color: e.target.value})} /></label>
                <button onClick={() => { if (commit('Saved faction.', p => M.upsertFaction(p, faction))) setFaction({id: '', name: '', color: '#7799bb'}); }}>Add / update faction</button>
            </section>
            <section><h3>Chapters</h3>
                {project.chapters.map(c => <div className="catalog-row" key={c.id}><i /><span>{c.name}<small>{c.id}</small></span>
                    <button className="icon" aria-label={`Delete ${c.name}`} onClick={() => commit(`Deleted ${c.name}.`, p => M.removeChapter(p, c.id))}>×</button></div>)}
                <Row><TextField label="ID" value={chapter.id} max={48} onCommit={v => setChapter({...chapter, id: v})} /><TextField label="Name" value={chapter.name} max={120} onCommit={v => setChapter({...chapter, name: v})} /></Row>
                <button onClick={() => { if (commit('Saved Chapter.', p => M.upsertChapter(p, chapter))) setChapter({id: '', name: ''}); }}>Add / update Chapter</button>
            </section>
        </div>
    </Modal>;
}

function PlayDialog() {
    const project = useStore(s => s.project);
    const {errors} = M.validate(project);
    const [quick, setQuick] = useState(true);
    const [busy, setBusy] = useState(false);
    const [result, setResult] = useState<PlaytestResult | null>(null);
    const launch = async () => {
        setBusy(true);
        try { setResult(await api.playtest(project, quick)); toast('Launching the game…', 'success'); }
        catch (error) { toast((error as Error).message, 'error'); }
        finally { setBusy(false); }
    };
    return <Modal eyebrow="PLAYTEST" title="Walk around your world">
        {errors.length ? <>
            <Hint>Fix these first:</Hint>
            <ul className="problem-list">{errors.slice(0, 8).map((e, i) => <li key={i}>{e}</li>)}</ul>
        </> : <>
            <Hint>Exports a fresh copy of this world into Saved/Playtests with its own save file, then starts the game on it. Your atlas and other saves are never touched.</Hint>
            <Toggle label="Quick start as a test character" hint="Skips login and character creation" checked={quick} onChange={setQuick} />
            {result && <div className="launch-note"><b>Started.</b> The game opens in your browser in a few seconds.<small>{result.folder}</small><code>{result.command}</code></div>}
        </>}
        <div className="modal-actions"><button onClick={done}>Close</button>
            <button className="play" disabled={!!errors.length || busy} onClick={launch}>{busy ? 'Exporting…' : result ? '▶ Launch again' : '▶ Play'}</button></div>
    </Modal>;
}

function ShortcutsDialog() {
    const rows = COMMANDS.filter(c => c.keys);
    return <Modal eyebrow="HELP" title="Keyboard shortcuts" wide>
        <div className="shortcut-grid">
            {rows.map(c => <div key={c.id}><kbd>{c.keys}</kbd><span>{c.label}</span></div>)}
            <div><kbd>1 – 0, -</kbd><span>Choose terrain while painting (Shift + the same keys: a second set)</span></div>
            <div><kbd>[ ]</kbd><span>Brush size</span></div>
            <div><kbd>Space + drag</kbd><span>Pan with any tool</span></div>
            <div><kbd>Wheel</kbd><span>Zoom at the pointer</span></div>
            <div><kbd>Right-click</kbd><span>Place people and markers, walk through doors</span></div>
            <div><kbd>Esc</kbd><span>Cancel · deselect · back to overview</span></div>
        </div>
    </Modal>;
}

function AboutDialog() {
    return <Modal eyebrow="HELP" title="How worlds reach the game" wide>
        <ol className="steps">
            <li><b>Draw</b> terrain on the continuous canvas. Every glyph is a tile the game walks on.</li>
            <li><b>Cut</b> the canvas into cells: the game shows one cell at a time and crosses open edges automatically.</li>
            <li><b>Build</b>: the Building tool stamps walls and a street door and creates a matching interior, already connected.</li>
            <li><b>Populate</b>: residents need a home (bed), a work place and an evening place. Merchants run shops; guards walk patrol routes; civilians work and socialize, or travel a route. Wages come from the town treasury.</li>
            <li><b>Play</b>: ▶ exports a fresh copy and starts the game on it. <b>Save</b> happens as you edit: every change goes straight to the one world in the DEV database, and everyone else editing sees it within seconds.</li>
        </ol>
        <Hint>The editor, the Python exporter and the game each validate the world independently, so a problem is caught before it can reach a player.</Hint>
    </Modal>;
}
