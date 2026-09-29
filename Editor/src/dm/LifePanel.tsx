// Someone's life, for the Dungeon Master (Docs/Design/26-living-npcs.md, Phase 8): the chronicle compiled from the event
// log, the story written from it on request, and what they carry in mind (memories, bonds, rumours). NPCs and player
// characters alike; read only, except for having a story written.
import {useCallback, useEffect, useState} from 'react';
import {nameOf, regardWords, timeline, type Belief, type Bond, type Chronicle} from '../lib/chronicle';
import {dmApi, type Me, type Target} from './api';

type View = 'life' | 'mind';
const DETAIL: {least: number; rounds: boolean; label: string}[] = [
    {least: 3, rounds: false, label: 'Milestones'},
    {least: 2, rounds: true, label: 'Life and seasons'},
    {least: 1, rounds: true, label: 'Everything'},
];

export function LifePanel({me, target, id, name}: {me: Me; target: Target; id: string; name: string}) {
    const [life, setLife] = useState<Chronicle | null>(null);
    const [problem, setProblem] = useState(''), [busy, setBusy] = useState(false);
    const [view, setView] = useState<View>('life'), [detail, setDetail] = useState(1);
    const load = useCallback(async () => {
        setProblem('');
        try { setLife(await dmApi.chronicle(target, id)); }
        catch (error) { setLife(null); setProblem((error as Error).message); }
    }, [target, id]);
    useEffect(() => { setLife(null); void load(); }, [load]);
    const write = async () => {
        const again = life?.story ? 'Rewrite' : 'Write';
        if (!window.confirm(`${again} ${name}'s life story? This asks the NPC model once (a paid call).`)) return;
        setBusy(true); setProblem('');
        try { setLife(await dmApi.writeStory(target, id)); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };
    const canWrite = me.role !== 'viewer';
    return <section className="dm-life" aria-label={`${name}'s life`}>
        <header><small>LIFE</small>
            <div className="dm-segments" role="tablist">
                {(['life', 'mind'] as const).map(v => <button key={v} role="tab" aria-selected={view === v} className={view === v ? 'on' : ''}
                    onClick={() => setView(v)}>{v === 'life' ? 'Chronicle' : 'In mind'}</button>)}
                <button title="Read again" onClick={() => void load()}>↻</button>
            </div></header>
        {problem && <p className="hint error-text">{problem}</p>}
        {!life ? (!problem && <p className="hint">Reading the event log…</p>)
            : view === 'life' ? <>
                <p className="meta">{life.events ? `${life.events} events, ${life.firstDate} to ${life.lastDate}.` : 'Nothing recorded yet: the event log fills while a game server runs on this database.'}</p>
                <div className="dm-story">
                    {life.story ? <>
                        {life.story.text.split(/\n+/).map((para, i) => <p key={i}>{para}</p>)}
                        <p className="meta">Written {new Date(life.story.at).toLocaleString()} by {life.story.by} ({life.story.model})
                            {life.story.newer > 0 ? ` · ${life.story.newer} new event${life.story.newer === 1 ? '' : 's'} since` : ' · up to date'}</p>
                    </> : <p className="hint">No story written yet.</p>}
                    {canWrite && life.entries.length > 0 && <button disabled={busy} onClick={() => void write()}>
                        {busy ? 'Writing…' : life.story ? 'Rewrite the story' : 'Tell their story'}</button>}
                </div>
                <div className="dm-segments small">{DETAIL.map((d, i) => <button key={d.label} className={detail === i ? 'on' : ''}
                    onClick={() => setDetail(i)}>{d.label}</button>)}</div>
                <ol className="dm-timeline">{timeline(life, DETAIL[detail].least, DETAIL[detail].rounds).map((line, i) => line.kind === 'event'
                    ? <li key={`e${line.entry.id ?? i}-${i}`} className={`i${line.entry.importance}`}>
                        <small>{line.entry.date}{line.entry.place ? ` · ${line.entry.place}` : ''}</small><span>{line.entry.text}</span></li>
                    : <li key={`r${i}`} className="round"><small>{line.round.label}</small><span>{line.round.text}</span></li>)}</ol>
            </> : <MindView life={life} />}
    </section>;
}

function MindView({life}: {life: Chronicle}) {
    const m = life.mind;
    const who = (id: string) => nameOf(m, id);
    const bonds = (list: Bond[], empty: string) => list.length ? <ul className="dm-bonds">{list.map(b =>
        <li key={b.who}><b>{who(b.who)}</b><span>{regardWords(b)}</span>
            <small>affinity {Math.round(b.affinity)} · trust {Math.round(b.trust)} · familiarity {Math.round(b.familiarity)}
                {b.fear ? ` · fear ${Math.round(b.fear)}` : ''}{b.respect ? ` · respect ${Math.round(b.respect)}` : ''}</small></li>)}</ul>
        : <p className="hint">{empty}</p>;
    const rumours = (list: Belief[], told: (b: Belief) => string, empty: string) => list.length
        ? <ul className="dm-bonds">{list.map((b, i) => <li key={i}><span>{told(b)}</span>
            <small>{Math.round(b.confidence * 100)}% sure{b.incident ? ` · ${b.incident}` : ''}</small></li>)}</ul>
        : <p className="hint">{empty}</p>;
    return <div className="dm-mind">
        <h3>Remembers</h3>
        {m.memories.length ? <ul className="dm-bonds">{m.memories.map((x, i) => <li key={i}>
            <b>{x.npc === life.subject ? `Of ${who(x.subject)}` : `${who(x.npc)} remembers`}</b><span>{x.text}</span></li>)}</ul>
            : <p className="hint">No conversations remembered.</p>}
        {m.conversations.length > 0 && <><h3>Talking now</h3>
            {m.conversations.map((c, i) => <details key={i}><summary>{who(c.npc)} and {who(c.subject)} · {c.turns.length} lines</summary>
                <ol className="dm-turns">{c.turns.map((t, j) => <li key={j}><b>{t.who}</b> {t.text}</li>)}</ol></details>)}</>}
        <h3>Their bonds</h3>{bonds(m.bonds, 'Knows nobody yet.')}
        <h3>How others regard them</h3>{bonds(m.regard, 'Nobody knows them yet.')}
        <h3>Has heard</h3>{rumours(m.heard, b => `${who(b.subject)} ${b.claim} (from ${who(b.source)})`, 'No rumours.')}
        <h3>Is said of them</h3>{rumours(m.said, b => `${who(b.holder)} believes they ${b.claim}${b.source === 'saw it' ? ' (saw it)' : ` (from ${who(b.source)})`}`, 'Nothing is said of them.')}
    </div>;
}
