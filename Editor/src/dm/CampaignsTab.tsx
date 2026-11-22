// Campaigns: the running world stories, watched live (Docs/Design/85-story-triggers-and-campaign-control.md, section 5;
// Phase 1, "See it"). A read-only console that refreshes itself every 2 seconds while the page is visible: the list of
// runs (where each is, who is in it, what happened last, what it waits for, its alerts), and one run's timeline,
// variables and timers, missions, wolves on the map and orders. Phase 2 ("Steer it") adds the controls: every one a
// command to the story runner's inbox, asked twice, with a reason, audited; and the runner's heartbeat in the bar.
import {useCallback, useEffect, useRef, useState} from 'react';
import {atLeast, dmApi, maySteer, storyProgram, worldLabel, type Live, type Me, type Target} from './api';
import {newKey, mergeTimeline, sinceOf, type Overview, type RunDetail, type TimelineCursor, type TimelineItem, type TimelinePage, type WorldEvent} from '../lib/campaigns';
import {CampaignList} from './campaigns/CampaignList';
import {CampaignView, type ViewTab} from './campaigns/CampaignView';

export const POLL_MS = 2000;

/** What the render smoke (and a test) hands the tab instead of asking the DM tool. */
export interface CampaignsInitial { overview: Overview; detail?: RunDetail; timeline?: TimelinePage; live?: Live; tab?: ViewTab }

export function CampaignsTab({me, target, focus, initial}: {me: Me; target: Target; focus?: string; initial?: CampaignsInitial}) {
    const [overview, setOverview] = useState<Overview | null>(initial?.overview ?? null);
    const [selected, setSelected] = useState<string>(focus ?? initial?.detail?.run ?? '');
    const [detail, setDetail] = useState<RunDetail | null>(initial?.detail ?? null);
    const [items, setItems] = useState<TimelineItem[]>(initial?.timeline?.items ?? []);
    const [events, setEvents] = useState<Record<string, WorldEvent>>(initial?.timeline?.events ?? {});
    const [names, setNames] = useState<Record<string, string>>({...initial?.overview.names, ...initial?.detail?.names, ...initial?.timeline?.names});
    const [earlier, setEarlier] = useState<TimelineCursor | null>(initial?.timeline?.before ?? null);
    const [live, setLive] = useState<Live | null>(initial?.live ?? null);
    const [problem, setProblem] = useState('');
    const [tab, setTab] = useState<ViewTab>(initial?.tab ?? 'overview');
    const program = useRef<RunDetail['program']>(initial?.detail?.program);
    const since = useRef<TimelineCursor | null>(initial?.timeline?.since ?? null);
    const busy = useRef(false);
    const canSee = atLeast(me, 'dm');
    useEffect(() => { if (focus) setSelected(focus); }, [focus]);

    const learn = (more?: Record<string, string>) => { if (more && Object.keys(more).length) setNames(n => ({...n, ...more})); };
    /** One refresh: the list, the frame of who is where, and for the open run only what is new since the last. */
    const poll = useCallback(async (run: string) => {
        if (busy.current) return;
        busy.current = true;
        try {
            const [list, frame] = await Promise.all([storyProgram<Overview>(target, 'status'), dmApi.live(target).catch(() => null)]);
            setOverview(list); learn(list.names); if (frame) setLive(frame);
            if (run) {
                const have = program.current ? (detail?.run === run ? detail.digest : undefined) : undefined;
                const one = await storyProgram<RunDetail>(target, 'status', {run, have, since: {trigger: 1e15}});
                if (one.program) program.current = one.program;
                setDetail({...one, program: one.program ?? program.current});
                learn(one.names);
                const page = await storyProgram<TimelinePage>(target, 'timeline', {run, ...(since.current ? {since: since.current} : {})});
                setItems(known => {
                    const merged = mergeTimeline(since.current ? known : [], page.items);
                    since.current = sinceOf(merged, since.current ?? {}, page.since.at ?? page.now);
                    return merged;
                });
                if (!since.current) setEarlier(page.before);
                setEvents(e => ({...e, ...page.events}));
                learn(page.names);
            }
            setProblem('');
        } catch (e) { setProblem((e as Error).message); }
        finally { busy.current = false; }
    }, [target, detail?.run, detail?.digest]); // eslint-disable-line react-hooks/exhaustive-deps

    // A new run opened: start its timeline afresh.
    useEffect(() => {
        if (initial?.detail?.run === selected) return;
        since.current = null; program.current = undefined;
        setItems([]); setEvents({}); setEarlier(null); setDetail(null);
    }, [selected]); // eslint-disable-line react-hooks/exhaustive-deps
    useEffect(() => {
        if (!canSee || initial) return;
        void poll(selected);
        const t = setInterval(() => { if (document.visibilityState === 'visible') void poll(selected); }, POLL_MS);
        return () => clearInterval(t);
    }, [poll, selected, canSee]); // eslint-disable-line react-hooks/exhaustive-deps

    const more = async () => {
        if (!earlier || !selected) return;
        try {
            const page = await storyProgram<TimelinePage>(target, 'timeline', {run: selected, before: earlier});
            setItems(known => mergeTimeline(known, page.items)); setEvents(e => ({...e, ...page.events})); learn(page.names);
            setEarlier(page.before);
        } catch (e) { setProblem((e as Error).message); }
    };

    /** Doc 34's "pause all": one hold command per running story, one reason (the owner's on a hosted world). */
    const holdAll = () => {
        const reason = window.prompt('Hold every running story? They read no events, send no orders and fire no timers until resumed. Why (a few words, for the audit)?');
        if (!reason || reason.trim().split(/\s+/).length < 2) return;
        void storyProgram<{commands: unknown[]}>(target, 'hold-all', {reason: reason.trim(), key: newKey()})
            .then(r => setProblem(r.commands.length ? '' : 'Nothing was running.'), e => setProblem((e as Error).message));
    };
    if (!canSee) return <div className="dm-center"><div className="empty-sheet"><h2>⚑ Campaigns</h2>
        <p>The Campaigns console names wolves and where they are: it needs a Dungeon Master account.</p></div></div>;
    const run = overview?.runs.find(r => r.run === selected);
    return <div className="dm-health dm-campaigns">
        <div className="dm-sheet-bar">
            <strong>{selected ? <button className="link-button" onClick={() => setSelected('')}>◂ Campaigns</button> : 'Campaigns'} · {worldLabel(target)}</strong>
            <span className="meta hint">{overview ? `game day ${overview.now.day === null ? '—' : overview.now.day.toFixed(1)} · the world's log at #${overview.now.newest}`
                : 'Loading…'}</span>
            {overview?.runner !== undefined && <span className={`camp-beat ${overview.runner && overview.runner.age !== null && overview.runner.age <= 30 ? 'alive' : 'down'}`}
                title="The story runner writes a heartbeat every tick (tools/story_runner.py)">
                runner {overview.runner && overview.runner.age !== null ? `● ${overview.runner.age <= 30 ? 'alive' : 'silent'} ${Math.round(overview.runner.age)} s ago`
                    + ` · ${overview.runner.behind ?? 0} event${overview.runner.behind === 1 ? '' : 's'} behind` : '○ has not run on this world'}</span>}
            <span className="live-feed" title="This page asks again every 2 seconds while it is visible">↻ 2 s</span>
        </div>
        {problem && <p className="error">{problem === 'Not found.' ? 'The DM host is older than this page: restart it.' : problem}</p>}
        {!selected || !run ? <CampaignList overview={overview} live={live} names={names} open={id => { setSelected(id); setTab('overview'); }}
            holdAll={maySteer(me) && (!me.hosted || me.role === 'owner') ? holdAll : undefined} />
            : <CampaignView me={me} target={target} row={run} detail={detail?.run === selected ? detail : null} items={items} events={events}
                names={names} now={overview!.now} live={live} tab={tab} setTab={setTab} earlier={earlier ? more : undefined}
                refresh={() => poll(selected)} />}
    </div>;
}
