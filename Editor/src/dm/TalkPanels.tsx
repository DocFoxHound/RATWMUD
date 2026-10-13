// Talk, for the Dungeon Master (Docs/Design/62-talk.md, phase 6): a resident's voice (where they grew up and how broad
// it sounds, their Ridgemere quarter, traits and quirk, the personality written for them, their own words and lines)
// and their family; the regional dialects; the figures residents know of who aren't in the game yet. Each is checked by
// the game's own loader before it is saved (the host runs talk_check), and a running server reads it again at once.
import {useCallback, useEffect, useState} from 'react';
import {dmApi, type Figure, type Me, type Talk, type Target, type Voice} from './api';

// The values Data/Talk/facts.json allows (the host checks them again).
const REGIONS = ['', 'ridgemere', 'frosthollow', 'coldwater', 'oakhaven', 'mirebridge', 'larkfield', 'southmeadow'];
const ACCENTS = ['', 'none', 'light', 'broad'];
const QUARTERS = ['', 'heights', 'commons', 'sump', 'office', 'works', 'wharf', 'docks', 'harbour_hill', 'suburbs', 'estate'];
const TRAITS: Record<string, string[]> = {
    temper: ['calm', 'even', 'nervy', 'short'], talk: ['quiet', 'plain', 'chatty'], warmth: ['cold', 'reserved', 'warm'],
    humour: ['none', 'dry', 'merry'], honesty: ['frank', 'plain', 'guarded', 'sly'], piety: ['devout', 'ordinary'],
    drive: ['content', 'steady', 'restless', 'ambitious'],
};
const ROLES = ['ruler', 'lord', 'captain', 'high_priest', 'hero', 'villain'];
const EMPTY: Voice = {from: '', accent: '', quarter: '', traits: {}, quirk: '', persona: '', grammar: '', rules: ''};

function useRun(target: Target, after: () => void) {
    const [busy, setBusy] = useState(false), [problem, setProblem] = useState(''), [note, setNote] = useState('');
    const run = async (what: string, job: () => Promise<unknown>) => {
        if (target === 'prod' && !window.confirm(`${what} on PROD?`)) return;
        setBusy(true); setProblem(''); setNote('');
        try { await job(); setNote(`${what}: saved; a running game server reads it again within seconds.`); after(); }
        catch (e) { setProblem((e as Error).message); }
        finally { setBusy(false); }
    };
    return {busy, problem, note, run};
}

/** A resident's voice and family. */
export function VoicePanel({me, target, npc, name}: {me: Me; target: Target; npc: string; name: string}) {
    const [talk, setTalk] = useState<Talk | null>(null);
    const [voice, setVoice] = useState<Voice>(EMPTY);
    const [kin, setKin] = useState({other: '', kind: 'spouse' as 'spouse' | 'parent', of: true});
    const load = useCallback(() => dmApi.talk(target, npc).then(t => { setTalk(t); setVoice({...EMPTY, ...(t.voice ?? {})}); }).catch(() => setTalk(null)),
        [target, npc]);
    useEffect(() => { void load(); }, [load]);
    const {busy, problem, note, run} = useRun(target, () => void load());
    const canAct = me.role !== 'viewer';
    const set = (fields: Partial<Voice>) => setVoice({...voice, ...fields});
    const select = (label: string, value: string, options: string[], onChange: (v: string) => void) =>
        <label className="field"><span>{label}</span><select value={value} disabled={!canAct} onChange={e => onChange(e.target.value)}>
            {options.map(o => <option key={o} value={o}>{o || '(as worked out)'}</option>)}</select></label>;
    return <div className="dm-panel" aria-label="Voice">
        <header><div><small>VOICE</small><h2>How {name} speaks</h2></div>
            <button title="Read again" onClick={() => void load()}>↻</button></header>
        {problem && <pre className="hint error-text">{problem}</pre>}
        {note && <p className="hint">{note}</p>}
        {voice.writtenBy && <p className="meta">Last written by {voice.writtenBy}.</p>}
        <fieldset disabled={!canAct || busy} className="dm-fields">
            <div className="button-grid">
                {select('Grew up in', voice.from, REGIONS, v => set({from: v}))}
                {select('Accent', voice.accent, ACCENTS, v => set({accent: v}))}
                {select('Ridgemere quarter', voice.quarter, QUARTERS, v => set({quarter: v}))}
            </div>
            <div className="button-grid">{Object.entries(TRAITS).map(([axis, values]) =>
                <span key={axis}>{select(axis, voice.traits[axis] ?? '', ['', ...values], v => {
                    const traits = {...voice.traits};
                    if (v) traits[axis] = v; else delete traits[axis];
                    set({traits});
                })}</span>)}</div>
            <label className="field"><span>Quirk (lower-case words joined by _)</span>
                <input value={voice.quirk} maxLength={60} onChange={e => set({quirk: e.target.value})} /></label>
            <label className="field"><span>Personality, written for them (what the model is given)</span>
                <textarea rows={6} maxLength={4000} value={voice.persona} onChange={e => set({persona: e.target.value})} /></label>
            <label className="field"><span>Their own words (saying! = …, dear! = …, pet!! = …, oath! = …)</span>
                <textarea rows={5} maxLength={8000} className="mono" value={voice.grammar} onChange={e => set({grammar: e.target.value})} /></label>
            <label className="field"><span>Their own lines (rules; only they may say them)</span>
                <textarea rows={8} maxLength={8000} className="mono" value={voice.rules} onChange={e => set({rules: e.target.value})} /></label>
        </fieldset>
        {canAct && <div className="button-grid">
            <button className={target === 'prod' ? 'publish' : 'primary'} disabled={busy}
                onClick={() => void run(`Save ${name}'s voice`, () => dmApi.saveVoice(target, npc, voice))}>{busy ? 'Checking…' : `Save to ${target.toUpperCase()}`}</button>
            <button disabled={busy} onClick={() => setVoice({...EMPTY, ...(talk?.voice ?? {})})}>Revert</button></div>}
        <p className="dm-group">Family</p>
        <ul className="dm-reports-list">{(talk?.family ?? []).map(f => <li key={`${f.npc}-${f.other}-${f.kind}`}>
            <b>{f.npcName}</b> is <b>{f.otherName}</b>'s {f.kind}
            {canAct && <button disabled={busy} onClick={() => void run(`Remove ${f.npcName} as ${f.otherName}'s ${f.kind}`,
                () => dmApi.saveKin(target, f.npc, f.other, f.kind as 'spouse' | 'parent', true))}>Remove</button>}</li>)}</ul>
        {canAct && <div className="button-grid">
            <select value={kin.of ? 'of' : 'is'} onChange={e => setKin({...kin, of: e.target.value === 'of'})}>
                <option value="of">{name} is the</option><option value="is">The other is {name}'s</option></select>
            <select value={kin.kind} onChange={e => setKin({...kin, kind: e.target.value as 'spouse' | 'parent'})}>
                <option value="spouse">spouse</option><option value="parent">parent</option></select>
            <input placeholder="of (a resident's id)" value={kin.other} onChange={e => setKin({...kin, other: e.target.value.trim()})} />
            <button disabled={busy || !kin.other} onClick={() => void run(`Add a ${kin.kind}`, () => kin.of
                ? dmApi.saveKin(target, npc, kin.other, kin.kind) : dmApi.saveKin(target, kin.other, npc, kin.kind))}>Add</button></div>}
        <p className="hint">The society takes a family in at the next day's turn: married, a child's parents.</p>
    </div>;
}

/** The regional dialects (decision 7): the database's over the game's files. */
export function DialectsPanel({me, target}: {me: Me; target: Target}) {
    const [dialects, setDialects] = useState<{name: string; grammar: string}[]>([]);
    const [name, setName] = useState(''), [text, setText] = useState('');
    const load = useCallback(() => dmApi.dialects(target).then(setDialects).catch(() => setDialects([])), [target]);
    useEffect(() => { void load(); }, [load]);
    const {busy, problem, note, run} = useRun(target, () => void load());
    const canAct = me.role !== 'viewer';
    return <div className="dm-panel" aria-label="Dialects">
        <header><div><small>TALK</small><h2>Dialects ({dialects.length})</h2></div></header>
        {problem && <pre className="hint error-text">{problem}</pre>}
        {note && <p className="hint">{note}</p>}
        <div className="button-grid">{dialects.map(d => <button key={d.name} className={d.name === name ? 'on' : ''}
            onClick={() => { setName(d.name); setText(d.grammar); }}>{d.name}</button>)}</div>
        {name && <>
            <textarea rows={14} className="mono" value={text} disabled={!canAct || busy} onChange={e => setText(e.target.value)} />
            {canAct && <button disabled={busy} onClick={() => void run(`Save the ${name} dialect`, () => dmApi.saveDialect(target, name, text))}>
                {busy ? 'Checking…' : 'Save'}</button>}</>}
        <p className="hint">Every symbol must be in the plain grammar (the game's own file); the game refuses anything else, with its reasons.</p>
    </div>;
}

/** People residents know of who aren't in the game yet (decision 8). */
export function FiguresPanel({me, target}: {me: Me; target: Target}) {
    const [figures, setFigures] = useState<Figure[]>([]);
    const blank: Figure = {id: '', name: '', title: '', role: 'lord', knownIn: 'realm', about: '', npc: ''};
    const [draft, setDraft] = useState<Figure>(blank);
    const load = useCallback(() => dmApi.figures(target).then(setFigures).catch(() => setFigures([])), [target]);
    useEffect(() => { void load(); }, [load]);
    const {busy, problem, note, run} = useRun(target, () => void load());
    const canAct = me.role !== 'viewer';
    return <div className="dm-panel" aria-label="Figures">
        <header><div><small>TALK</small><h2>Figures ({figures.length})</h2></div></header>
        {problem && <pre className="hint error-text">{problem}</pre>}
        {note && <p className="hint">{note}</p>}
        <ul className="dm-reports-list">{figures.map(f => <li key={f.id}>
            <b>{f.name}</b> <span className="meta">{f.title} · {f.role} · known in {f.knownIn}{f.npc ? ` · is ${f.npc}` : ''}</span>
            {f.about && <p className="meta">{f.about}</p>}
            {canAct && <div className="button-grid"><button disabled={busy} onClick={() => setDraft(f)}>Edit</button>
                <button className="danger" disabled={busy} onClick={() => void run(`Remove ${f.name}`, () => dmApi.saveFigure(target, {id: f.id}, true))}>Remove</button></div>}
        </li>)}</ul>
        {canAct && <div className="button-grid">
            <input placeholder="id" value={draft.id} onChange={e => setDraft({...draft, id: e.target.value.trim()})} />
            <input placeholder="Name" value={draft.name} onChange={e => setDraft({...draft, name: e.target.value})} />
            <input placeholder="Title" value={draft.title} onChange={e => setDraft({...draft, title: e.target.value})} />
            <select value={draft.role} onChange={e => setDraft({...draft, role: e.target.value})}>{ROLES.map(r => <option key={r}>{r}</option>)}</select>
            <input placeholder="Known in: realm, region:<name>, house:<name>" value={draft.knownIn} onChange={e => setDraft({...draft, knownIn: e.target.value.trim()})} />
            <input placeholder="Becomes (a resident's id, once in the game)" value={draft.npc} onChange={e => setDraft({...draft, npc: e.target.value.trim()})} />
            <textarea rows={3} maxLength={600} placeholder="About them" value={draft.about} onChange={e => setDraft({...draft, about: e.target.value})} />
            <button disabled={busy || !draft.id || !draft.name} onClick={() => void run(`Save ${draft.name}`, () => dmApi.saveFigure(target, draft))}>Save</button>
            <button disabled={busy} onClick={() => setDraft(blank)}>New</button></div>}
        <p className="hint">Residents speak of a figure by role (#world.figure_ruler#…) where they're known.</p>
    </div>;
}
