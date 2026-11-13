// RATW Studio: one tool, two suites (Docs/Design/66-worlds-atlas-and-dm.md, section 3; tools/studio.py).
// One sign-in (a Dungeon Master account) and a top bar: the suite switch, the suite's world, who is signed in.
//   Atlas  builds worlds, on dev worlds (its picker switches Atlas's world): the Workshop (the map, interiors, people,
//          characters: App.tsx), its NPC pages (NPCs with their routes, areas, spawn rules and talk; factions), and
//          Update from live (doc 66, section 4: a copy of a live world takes live's changes, conflicts chosen), and the
//          Economy page (doc 79, 12: the slider, the rules, the seasons, and the host's overrides of the data files).
//   DM     runs the live world (the live server's; its Server & import tab switches it, in maintenance): DmSuite.
// Each suite and page is its own page load (?suite=atlas&page=npcs): their maps share one lazy-ground list
// (lib/lazyGround.ts), and Atlas's live session runs for as long as the page does.
import {useEffect, useRef, useState} from 'react';
import {App} from '../App';
import {DmSuite, SignIn} from '../dm/DmApp';
import {NpcTab} from '../dm/NpcTab';
import {FactionsTab} from '../dm/FactionsTab';
import {dmApi, setSuite, setWorlds, signedIn, type Me, type StudioSession, type Target} from '../dm/api';
import {colorFor, saveIdentity, savedIdentity} from '../lib/live';
import {UpdateFromLive} from './UpdateFromLive';
import {ENDED_TEXT, goTo, hosted, reloadApp, RESTARTING_TEXT, sessionEnded, watchRestarting, watchSession, worldRestarting} from '../lib/hosted';
import {EconomyControls} from './EconomyControls';
import {PublishPage} from './PublishPage';
import {BADGE, pageTakes, showFrom} from '../lib/publish';
import {openView, toast, useStore} from '../lib/store';

type Suite = 'atlas' | 'dm';
type AtlasPage = 'workshop' | 'npcs' | 'factions' | 'economy' | 'sync' | 'publish';
const ATLAS_PAGES: {id: AtlasPage; label: string; title: string}[] = [
    {id: 'workshop', label: 'Workshop', title: 'The map, interiors, people and characters'},
    {id: 'npcs', label: 'NPCs & areas', title: 'NPCs with their patrol routes, painted areas, spawn rules, voices, dialects and figures'},
    {id: 'factions', label: 'Factions', title: 'Factions, their territory, members and relations'},
    {id: 'economy', label: 'Economy', title: 'Economy: the slider and its cause, the rules, the seasons, the ships, the bands and the street'},
    {id: 'sync', label: '⇩ Update from live', title: 'Take the live world\'s changes into this copy of it, choosing where both changed'},
    {id: 'publish', label: 'Publish', title: 'Make your map changes the map players are in; go back to the previous map; discard changes'},
];
/** A host's Editor (doc 78): its map editor's pages. No Update from live (one world); Publish instead (M8). */
const HOSTED_PAGES: AtlasPage[] = ['workshop', 'npcs', 'factions', 'economy', 'publish'];
const HOSTED_LABELS: Partial<Record<AtlasPage, string>> = {workshop: 'Map'};

export function Studio() {
    const [me, setMe] = useState<Me | null>(null);
    const [checking, setChecking] = useState(signedIn());
    const [ended, setEnded] = useState(sessionEnded());
    useEffect(() => watchSession(setEnded), []);
    // Hosted: while the world restarts, one quiet ping every few seconds, so the banner goes once it is back even on a
    // tab that doesn't poll.
    const [restarting, setRestarting] = useState(worldRestarting());
    useEffect(() => watchRestarting(setRestarting), []);
    useEffect(() => {
        if (!restarting) return;
        const t = setInterval(() => void dmApi.ping().catch(() => undefined), 5000);
        return () => clearInterval(t);
    }, [restarting]);
    useEffect(() => {
        if (!signedIn()) return;
        // Hosted (doc 78): who is signed in is the Editor session the shell holds; the page never signs in.
        (hosted ? dmApi.studio().then(s => s.me) : dmApi.me()).then(setMe).catch(() => undefined).finally(() => setChecking(false));
    }, [ended, restarting]);
    if (checking) return <div className="dm-center"><p className="hint">Checking your session…</p></div>;
    if (hosted) {
        return <>
            {ended ? <div className="hosted-ended" role="alert">{ENDED_TEXT}</div>
                : restarting && <div className="hosted-ended hosted-restarting" role="status">{RESTARTING_TEXT}</div>}
            {me ? <Shell me={me} onSignOut={() => undefined} />
                : <div className="dm-center"><p className="hint">{restarting ? RESTARTING_TEXT : ENDED_TEXT}</p></div>}
        </>;
    }
    return me ? <Shell me={me} onSignOut={() => setMe(null)} /> : <SignIn onSignedIn={setMe} />;
}

/** Opens a suite (and an Atlas page): as its own page load, or inside the page when hosted. */
function go(suite: Suite, page: AtlasPage = 'workshop') {
    const query = new URLSearchParams({suite});
    if (suite === 'atlas' && page !== 'workshop') query.set('page', page);
    goTo(String(query));
}

function Shell({me, onSignOut}: {me: Me; onSignOut: () => void}) {
    const [session, setSession] = useState<StudioSession | null>(null);
    const [problem, setProblem] = useState('');
    const [busy, setBusy] = useState(false);
    useEffect(() => {
        let on = true, timer = 0;
        // (Hosted, a world restarting: asked again until it answers.)
        const read = () => dmApi.studio().then(s => { if (on) { setWorlds([...s.dm.worlds, ...s.atlas.worlds]); setSession(s); } })
            .catch(e => { if (!on) return; setProblem((e as Error).message); if (hosted && worldRestarting()) timer = window.setTimeout(read, 5000); });
        void read();
        return () => { on = false; clearTimeout(timer); };
    }, []);
    if (!session) return <div className="dm-center"><p className="hint">{problem || 'Loading the studio…'}</p></div>;

    const url = new URLSearchParams(window.location.search);
    const asked = url.get('suite');
    const wanted: Suite = asked === 'atlas' || asked === 'dm' ? asked : session.suite;
    const suite: Suite = wanted === 'atlas' && !session.atlas.available ? 'dm' : wanted;
    const pages = ATLAS_PAGES.filter(p => (session.hosted ? HOSTED_PAGES.includes(p.id) : p.id !== 'publish'));
    const page = pages.find(p => p.id === url.get('page'))?.id ?? 'workshop';
    // Live now, or Waits for Publish (doc 78, 11): what this page's Save buttons do, said beside the page's name.
    const badge = session.hosted ? pageTakes(suite, page) : null;
    const atlasKey = session.atlas.world.key;
    const live = session.dm.live;
    const dmKey = session.dm.world ?? live;          // The DM's world: its own choice, else the live server's.
    // (Where the DM components' calls go; in the DM, its world counts as one players are in.) This page keeps acting on
    // the worlds its header names, whatever another browser picks meanwhile; a reload takes up the new choice.
    setSuite(suite, dmKey);
    const switchDm = async (key: Target) => {
        const name = session.dm.worlds.find(w => w.key === key)?.name ?? key;
        if (key === dmKey || !window.confirm(`Point the DM at ${name}? Every DM page then works on it: players, NPC life, storylines and its server. (This computer's choice: other browsers take it up when they reload.)`)) return;
        setBusy(true);
        try { await dmApi.useDmWorld(key); reloadApp(); }
        catch (error) { setProblem((error as Error).message); setBusy(false); }
    };
    const switchAtlas = async (key: Target) => {
        const name = session.atlas.worlds.find(w => w.key === key)?.name ?? key;
        if (key === atlasKey || !window.confirm(`Switch Atlas to ${name}? Atlas's world changes for everyone using this studio (and for local and scratch servers, as world_db.py use does).`)) return;
        setBusy(true);
        try { await dmApi.useAtlasWorld(key); reloadApp(); }
        catch (error) { setProblem((error as Error).message); setBusy(false); }
    };

    return <div className={`studio studio-${suite}`}>
        <header className="studio-bar">
            <div className="brand"><span className="mark">R<i>›</i></span><div><b>RATW STUDIO</b><small>Run Against the World</small></div></div>
            <div className="segmented studio-suites" role="radiogroup" aria-label="Suite">
                <button role="radio" aria-checked={suite === 'atlas'} className={suite === 'atlas' ? 'on' : ''} disabled={!session.atlas.available}
                    title={session.atlas.available ? 'Atlas: building worlds (dev worlds)' : session.atlas.reason || 'Atlas is for dm and admin accounts'}
                    onClick={() => suite !== 'atlas' && go('atlas')}>Atlas</button>
                <button role="radio" aria-checked={suite === 'dm'} className={suite === 'dm' ? 'on' : ''} title="Dungeon Master: running the live world"
                    onClick={() => suite !== 'dm' && go('dm')}>DM</button>
            </div>
            {session.hosted ? <>
                <span className="studio-world" title="This Editor works on its own world only (Docs/Design/78, 7)">
                    <span>World</span><b>{session.dm.worlds[0]?.name ?? dmKey}</b>
                    {!session.atlas.available && session.atlas.reason && <small>{session.atlas.reason}</small>}
                </span>
                {suite === 'atlas' && <div className="segmented" role="tablist" aria-label="Map editor page">{pages.map(p =>
                    <button key={p.id} role="tab" aria-selected={page === p.id} className={page === p.id ? 'on' : ''} title={p.title}
                        onClick={() => page !== p.id && go('atlas', p.id)}>{HOSTED_LABELS[p.id] ?? p.label}</button>)}</div>}
                {badge && <span className={`takes-badge takes-${badge}`} title={BADGE[badge].title} data-takes={badge}>{BADGE[badge].label}</span>}
            </> : suite === 'atlas' ? <>
                <label className="studio-world" title="Atlas's world: the dev world Atlas edits, and local and scratch servers play">
                    <span>Dev world</span>
                    <select value={atlasKey} disabled={busy || hosted} onChange={e => void switchAtlas(e.target.value)}>
                        {session.atlas.worlds.map(w => <option key={w.key} value={w.key}>{w.name} · {w.key}{w.upstream ? ` (copy of ${w.upstream})` : ''}</option>)}
                    </select>
                </label>
                <div className="segmented" role="tablist" aria-label="Atlas page">{pages.map(p =>
                    <button key={p.id} role="tab" aria-selected={page === p.id} className={page === p.id ? 'on' : ''}
                        title={p.id === 'sync' && !session.atlas.world.upstream ? `${session.atlas.world.name} has no upstream live world: a side project, with nothing to update from` : p.title}
                        onClick={() => page !== p.id && go('atlas', p.id)}>{p.label}</button>)}</div>
            </> : <label className="studio-world" title="The DM's world: every DM page (players, NPC life, storylines, the server) works on it. Chosen apart from Atlas's world.">
                <span>DM world</span>
                <select value={dmKey ?? ''} disabled={busy || hosted || me.role === 'viewer'} onChange={e => void switchDm(e.target.value)}>
                    {session.dm.worlds.map(w => <option key={w.key} value={w.key}>{w.name} · {w.key} ({w.kind}){w.key === live ? ' · the live server\'s' : ''}</option>)}
                </select>
                <small>{dmKey === atlasKey ? 'Atlas edits this world too' : `Atlas: ${session.atlas.world.name}`}</small>
            </label>}
            {problem && <span className="hint error-text">{problem}</span>}
            <div className="top-actions">
                <span className="dm-me" title={`Signed in as ${me.username}`}>{me.hosted ? me.account ?? me.username : me.username}<small>{me.role}</small></span>
                {!hosted && <button onClick={() => { void dmApi.logout().finally(onSignOut); }}>Sign out</button>}
            </div>
        </header>
        {suite === 'dm' ? (dmKey ? <DmSuite me={me} target={dmKey} />
            : <div className="dm-center"><p className="hint">No world is chosen for the DM (python3 tools/world_db.py list).</p></div>)
            : page === 'workshop' ? <Workshop me={me} />
            : page === 'sync' ? <div className="app dm dev"><UpdateFromLive me={me} onWorkshop={() => go('atlas')} /></div>
            : page === 'economy' ? <div className="app dm dev"><EconomyControls me={me} /></div>
            : page === 'publish' ? <div className="app dm dev"><PublishPage me={me} /></div>
            : <div className="app dm dev">{page === 'npcs' ? <NpcTab me={me} target={atlasKey} suite="atlas" />
                : <FactionsTab me={me} target={atlasKey} />}</div>}
    </div>;
}

/** Atlas's Workshop. Its editors are named in each other's view and in the edit log: the DM account's name, unless this
 *  browser was given another. */
function Workshop({me}: {me: Me}) {
    useState(() => { if (!savedIdentity()) saveIdentity({name: me.username, color: colorFor(me.username)}); return 0; });
    // Publish's "Show me" (?show=PLACE,X,Y; doc 78, 10.3): the place opened once the world holding it has loaded.
    const project = useStore(s => s.project), shown = useRef(false);
    useEffect(() => {
        const place = showFrom(window.location.search);
        if (!place || shown.current || ![...project.cells, ...project.rooms].some(c => c.id === place.cell)) return;
        shown.current = true;
        // (The closing run: from the world's overview focusPlace opens nothing for a world cell, so "Show me" landed on
        // the whole map. The place itself is opened, and where in it is said.)
        const found = [...project.cells, ...project.rooms].find(c => c.id === place.cell);
        openView({kind: 'cell', id: place.cell});
        toast(`${found?.name ?? place.cell}${place.x || place.y ? `, at ${place.x}, ${place.y}` : ''}: the problem is here.`);
    }, [project]);
    return <App />;
}
