// Campaigns made of chapters (Docs/Design/85-story-triggers-and-campaign-control.md, 3.5 "story.start" and Phase 10): the list
// shows each campaign as its chapters in order (done, running, waiting to be started), a chapter opens its run; a run that is
// a chapter shows the strip above its phases, and the chapters it waits to start. "New campaign…" approves several of the
// world's drafts together, in order (each rehearsed and checked as its own approval would be), and publishes the first.
import {useEffect, useState} from 'react';
import {storyProgram, type Target} from '../api';
import {campaignText, chapterMark, chapterReadyText, chapterStartText, chapterStrip, type Campaign, type Readiness, type RunDetail, type RunRow} from '../../lib/campaigns';

/** Doc 86, R3: the running chapter's readiness in one line, and "Advance…" to open its run on the Overview. */
function ChapterReady({ready, chapter, run, open}: {ready?: Readiness; chapter: number; run?: string; open: (run: string) => void}) {
    if (!ready) return null;
    return <small className={`camp-chapter-ready ${ready.met ? 'met' : ''}`}>{chapterReadyText(ready, chapter)}
        {run && <> · <button type="button" className="link-button" onClick={() => open(run)}>Advance…</button></>}</small>;
}

export function CampaignCards({campaigns, open, runs, target, maySteer}: {campaigns: Campaign[]; open: (run: string) => void; runs?: RunRow[];
    /** Doc 86, C11: may let go of a campaign's holds on residents (steering). */
    target?: Target; maySteer?: boolean}) {
    const [said, setSaid] = useState('');
    if (!campaigns.length) return null;
    const release = async (c: Campaign) => {
        if (!target || !window.confirm(`Let go of the residents ${c.title} spares or protects for the campaign? Old age and fights may take them again.`)) return;
        try { await storyProgram(target, 'campaign-release', {campaign: c.id}); setSaid(`${c.title}: its holds are let go.`); }
        catch (e) { setSaid((e as Error).message); }
    };
    const byRun = new Map((runs ?? []).map(r => [r.run, r]));
    return <div className="camp-campaigns" aria-label="Campaigns of chapters">
        {campaigns.map(c => {
            const n = c.chapters.findIndex(ch => ch.runs.some(x => ['running', 'held', 'paused'].includes(x.status)));
            const live = n >= 0 ? c.chapters[n].runs.find(x => ['running', 'held', 'paused'].includes(x.status)) : undefined;
            return <div key={c.id} className={`camp-campaign ${c.status}`}>
                <b>{c.title}</b> <small className="hint">{campaignText(c)} · approved by {c.approvedBy}</small>
                <ChapterChips campaign={c} open={open} />
                {live && <ChapterReady ready={byRun.get(live.run)?.ready} chapter={n + 1} run={live.run} open={open} />}
                {!live && target && maySteer && c.status !== 'approved' && <small> · <button type="button" className="link-button"
                    title="For an act stopped early: the residents its chapters held for the campaign (doc 86)" onClick={() => void release(c)}>Let go of the campaign's holds…</button></small>}
            </div>;
        })}
        {said && <p className="hint">{said}</p>}
    </div>;
}

export function ChapterChips({campaign, open, current}: {campaign: Campaign; open: (run: string) => void; current?: number}) {
    return <ol className="camp-chapters">{chapterStrip(campaign).map((s, n) => <li key={s.story} className={`${s.state}${current === n ? ' now' : ''}`}>
        {s.run ? <button type="button" className="link-button" onClick={() => open(s.run!)} title={`${s.story}: ${s.state}${s.runs > 1 ? ` (${s.runs} runs)` : ''}`}>
            {chapterMark(s.state)} {n + 1}. {s.title}{s.phase && s.state !== 'done' ? <small> · {s.phase}</small> : null}</button>
            : <span title={s.state === 'ready' ? 'Approved: publish it to begin' : 'Started by the chapter before it'}>{chapterMark(s.state)} {n + 1}. {s.title}</span>}
    </li>)}</ol>;
}

/** Above a chapter's phases: its campaign, where it stands among the chapters, and what it is to start. */
export function ChapterBar({detail, names, open}: {detail: RunDetail; names: Record<string, string>; open: (run: string) => void}) {
    const c = detail.campaign;
    if (!c) return null;
    return <div className="camp-chapter-bar">
        <span>Chapter {(c.chapter ?? 0) + 1} of {c.chapters.length} of <b>{c.title}</b>:</span>
        <ChapterChips campaign={c} open={open} current={c.chapter} />
        <ChapterReady ready={detail.ready} chapter={(c.chapter ?? 0) + 1} open={open} />
        {(detail.chapterStarts ?? []).map(s => <small key={s.story} className="hint">Starts {chapterStartText(s, c, names)}.</small>)}
        {(detail.chaptersStarted ?? []).filter(s => s.refused).map(s => <small key={s.story} className="error-text">Did not start {s.story}: {s.refused}</small>)}
    </div>;
}

interface DraftRow {id: number; name: string; title: string; phases: number; triggers: number}
interface Rehearsed {passed: boolean; digest: string; uncovered: string[]}

export function NewCampaign({target, done}: {target: Target; done: (run?: string) => void}) {
    const [drafts, setDrafts] = useState<DraftRow[] | null>(null);
    const [chosen, setChosen] = useState<number[]>([]);
    const [id, setId] = useState(''), [title, setTitle] = useState('');
    const [problem, setProblem] = useState(''), [busy, setBusy] = useState(false);
    const [approved, setApproved] = useState<{campaign: string; ticket?: string; chapters: {story: string; title: string}[]} | null>(null);
    useEffect(() => {
        storyProgram<{drafts: DraftRow[]}>(target, 'draft.list').then(r => setDrafts(r.drafts), e => setProblem((e as Error).message));
    }, [target]);
    const move = (i: number, by: number) => setChosen(c => { const n = [...c]; const [x] = n.splice(i, 1); n.splice(Math.max(0, Math.min(n.length, i + by)), 0, x); return n; });
    const approve = async () => {
        setBusy(true); setProblem('');
        try {
            const chapters = [];
            for (const d of chosen) {
                const opened = await storyProgram<{program: Record<string, unknown>}>(target, 'draft.open', {id: d});
                const r = await storyProgram<Rehearsed>(target, 'rehearse', {program: opened.program});
                const name = drafts?.find(x => x.id === d)?.title ?? String(d);
                if (!r.passed) throw new Error(`"${name}" does not rehearse: open it in Story Creator and fix it first.`);
                if (r.uncovered.length) throw new Error(`"${name}": no rehearsal reaches ${r.uncovered.join(', ')}.`);
                chapters.push({program: opened.program, digest: r.digest});
            }
            setApproved(await storyProgram(target, 'campaign.approve', {campaign: {id: id.trim(), title: title.trim(), chapters}}));
        } catch (e) { setProblem((e as Error).message); } finally { setBusy(false); }
    };
    const publish = async () => {
        if (!approved?.ticket) return;
        setBusy(true); setProblem('');
        try { const r = await storyProgram<{run: string}>(target, 'publish', {ticket: approved.ticket}); done(r.run); }
        catch (e) { setProblem((e as Error).message); } finally { setBusy(false); }
    };
    return <div className="camp-new" role="dialog" aria-label="New campaign">
        <h3>A campaign of chapters</h3>
        <p className="hint">Choose the world's drafts in the order they are told. Each is rehearsed and checked as its own approval would be; a chapter starts the next with
            "Start the next chapter" (story.start), when it has ended (the default) or at once. The first is published here; the others wait for the chapter before them.</p>
        {approved ? <>
            <p>Approved <b>{approved.campaign}</b>: {approved.chapters.map((c, i) => `${i + 1}. ${c.title}`).join(' · ')}.</p>
            {approved.ticket ? <div className="camp-controls"><button className="primary" disabled={busy} onClick={() => void publish()}>Publish chapter 1 now</button>
                <button onClick={() => done()}>Later</button><small className="hint">The ticket is good for a day.</small></div>
                : <p className="hint">Its first chapter has started already: the new approval replaces the chapters still to come.</p>}
        </> : <>
            <div className="wb-head"><label>Campaign id<input value={id} maxLength={80} placeholder="the-long-winter" onChange={e => setId(e.target.value)} /></label>
                <label className="wb-grow">Title<input value={title} maxLength={100} placeholder="The long winter" onChange={e => setTitle(e.target.value)} /></label></div>
            <ol className="camp-chosen">{chosen.map((d, i) => <li key={d}>{drafts?.find(x => x.id === d)?.title ?? d}
                <button type="button" onClick={() => move(i, -1)} disabled={i === 0} aria-label="Earlier">▲</button>
                <button type="button" onClick={() => move(i, 1)} disabled={i === chosen.length - 1} aria-label="Later">▼</button>
                <button type="button" className="wb-x" onClick={() => setChosen(c => c.filter(x => x !== d))} aria-label="Take it out">×</button></li>)}</ol>
            {drafts === null ? <p className="hint">Reading the world's drafts…</p> : <select value="" aria-label="Add a chapter" onChange={e => { const v = Number(e.target.value); if (v) setChosen(c => [...c, v]); }}>
                <option value="">+ add a chapter: a draft…</option>
                {drafts.filter(d => !chosen.includes(d.id)).map(d => <option key={d.id} value={d.id}>{d.title} ({d.name}; {d.phases} phase{d.phases === 1 ? '' : 's'})</option>)}</select>}
            <div className="camp-controls"><button className="primary" disabled={busy || !chosen.length || !id.trim() || !title.trim()} onClick={() => void approve()}>
                {busy ? 'Rehearsing and approving…' : `Approve the ${chosen.length} chapter${chosen.length === 1 ? '' : 's'} together`}</button>
                <button onClick={() => done()}>Cancel</button></div>
        </>}
        {problem && <p className="error-text">{problem}</p>}
    </div>;
}
