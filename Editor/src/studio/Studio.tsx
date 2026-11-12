// RATW Studio: one tool, two suites (Docs/Design/66-worlds-atlas-and-dm.md, section 3; tools/studio.py).
// One sign-in (a Dungeon Master account) and a top bar: the suite switch, the suite's world, who is signed in.
//   Atlas  builds worlds, on dev worlds (its picker switches Atlas's world): the Workshop (the map, interiors, people,
//          characters: App.tsx), its NPC pages (NPCs with their routes, areas, spawn rules and talk; factions), and
//          Update from live (doc 66, section 4: a copy of a live world takes live's changes, conflicts chosen), and the
//          Economy page (doc 79, 12: the slider, the rules, the seasons, and the host's overrides of the data files).
//   DM     runs the live world (the live server's; its Server & import tab switches it, in maintenance): DmSuite.
// Each suite and page is its own page load (?suite=atlas&page=npcs): their maps share one lazy-ground list
// (lib/lazyGround.ts), and Atlas's live session runs for as long as the page does.
import {useEffect, useState} from 'react';
import {App} from '../App';
import {DmSuite, SignIn} from '../dm/DmApp';
import {NpcTab} from '../dm/NpcTab';
import {FactionsTab} from '../dm/FactionsTab';
import {dmApi, setSuite, setWorlds, signedIn, type Me, type StudioSession, type Target} from '../dm/api';
import {colorFor, saveIdentity, savedIdentity} from '../lib/live';
import {UpdateFromLive} from './UpdateFromLive';
import {ENDED_TEXT, goTo, hosted, reloadApp, sessionEnded, watchSession} from '../lib/hosted';
import {EconomyControls} from './EconomyControls';

type Suite = 'atlas' | 'dm';
type AtlasPage = 'workshop' | 'npcs' | 'factions' | 'economy' | 'sync';
const ATLAS_PAGES: {id: AtlasPage; label: string; title: string}[] = [
    {id: 'workshop', label: 'Workshop', title: 'The map, interiors, people and characters'},
    {id: 'npcs', label: 'NPCs & areas', title: 'NPCs with their patrol routes, painted areas, spawn rules, voices, dialects and figures'},
    {id: 'factions', label: 'Factions', title: 'Factions, their territory, members and relations'},
    {id: 'economy', label: 'Economy', title: 'Economy: the slider and its cause, the rules, the seasons, the ships, the bands and the street'},
    {id: 'sync', label: '⇩ Update from live', title: 'Take the live world\'s changes into this copy of it, choosing where both changed'},
];

export function Studio() {
    const [me, setMe] = useState<Me | null>(null);
    const [checking, setChecking] = useState(signedIn());
    const [ended, setEnded] = useState(sessionEnded());
    useEffect(() => watchSession(setEnded), []);
    useEffect(() => {
        if (!signedIn()) return;
        // Hosted (doc 78): who is signed in is the Editor session the shell holds; the page never signs in.
        (hosted ? dmApi.studio().then(s => s.me) : dmApi.me()).then(setMe).catch(() => undefined).finally(() => setChecking(false));
    }, [ended]);
    if (checking) return <div className="dm-center"><p className="hint">Checking your session…</p></div>;
    if (hosted) {
        return <>
            {ended && <div className="hosted-ended" role="alert">{ENDED_TEXT}</div>}
            {me ? <Shell me={me} onSignOut={() => undefined} />
                : <div className="dm-center"><p className="hint">{ENDED_TEXT}</p></div>}
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
        dmApi.studio().then(s => { setWorlds([...s.dm.worlds, ...s.atlas.worlds]); setSession(s); })
            .catch(e => setProblem((e as Error).message));
    }, []);
    if (!session) return <div className="dm-center"><p className="hint">{problem || 'Loading the studio…'}</p></div>;

    const url = new URLSearchParams(window.location.search);
    const asked = url.get('suite');
    const wanted: Suite = asked === 'atlas' || asked === 'dm' ? asked : session.suite;
    const suite: Suite = wanted === 'atlas' && !session.atlas.available ? 'dm' : wanted;
    const page = ATLAS_PAGES.find(p => p.id === url.get('page'))?.id ?? 'workshop';
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
            <div className="brand"><span className="mark">R<i>›</i></span><div><b>RATW STUDIO</b><small>Runs Against the World</small></div></div>
            <div className="segmented studio-suites" role="radiogroup" aria-label="Suite">
                <button role="radio" aria-checked={suite === 'atlas'} className={suite === 'atlas' ? 'on' : ''} disabled={!session.atlas.available}
                    title={session.atlas.available ? 'Atlas: building worlds (dev worlds)' : 'Atlas is for dm and admin accounts'}
                    onClick={() => suite !== 'atlas' && go('atlas')}>Atlas</button>
                <button role="radio" aria-checked={suite === 'dm'} className={suite === 'dm' ? 'on' : ''} title="Dungeon Master: running the live world"
                    onClick={() => suite !== 'dm' && go('dm')}>DM</button>
            </div>
            {suite === 'atlas' ? <>
                <label className="studio-world" title="Atlas's world: the dev world Atlas edits, and local and scratch servers play">
                    <span>Dev world</span>
                    <select value={atlasKey} disabled={busy || hosted} onChange={e => void switchAtlas(e.target.value)}>
                        {session.atlas.worlds.map(w => <option key={w.key} value={w.key}>{w.name} · {w.key}{w.upstream ? ` (copy of ${w.upstream})` : ''}</option>)}
                    </select>
                </label>
                <div className="segmented" role="tablist" aria-label="Atlas page">{ATLAS_PAGES.map(p =>
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
                <span className="dm-me" title={`Signed in as ${me.username}`}>{me.username}<small>{me.role}</small></span>
                {!hosted && <button onClick={() => { void dmApi.logout().finally(onSignOut); }}>Sign out</button>}
            </div>
        </header>
        {suite === 'dm' ? (dmKey ? <DmSuite me={me} target={dmKey} />
            : <div className="dm-center"><p className="hint">No world is chosen for the DM (python3 tools/world_db.py list).</p></div>)
            : page === 'workshop' ? <Workshop me={me} />
            : page === 'sync' ? <div className="app dm dev"><UpdateFromLive me={me} onWorkshop={() => go('atlas')} /></div>
            : page === 'economy' ? <div className="app dm dev"><EconomyControls me={me} /></div>
            : <div className="app dm dev">{page === 'npcs' ? <NpcTab me={me} target={atlasKey} suite="atlas" />
                : <FactionsTab me={me} target={atlasKey} />}</div>}
    </div>;
}

/** Atlas's Workshop. Its editors are named in each other's view and in the edit log: the DM account's name, unless this
 *  browser was given another. */
function Workshop({me}: {me: Me}) {
    useState(() => { if (!savedIdentity()) saveIdentity({name: me.username, color: colorFor(me.username)}); return 0; });
    return <App />;
}
