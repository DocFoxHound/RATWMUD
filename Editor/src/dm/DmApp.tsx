// Dungeon Master: runs the living world (Docs/Design/21-dungeon-master.md, 34-dungeon-master-refresh.md).
// Atlas builds places; this app manages everything alive in them, in PROD (live) or DEV (rehearsal).
import {useCallback, useEffect, useMemo, useRef, useState} from 'react';
import type {Project} from '../model/model.mjs';
import * as M from '../model/model.mjs';
import {surfaceFor} from '../lib/surface';
import {drawGround} from '../lib/ground';
import {useGlyphRender} from '../lib/glyphFont';
import {NpcTab} from './NpcTab';
import {LiveTab} from './LiveTab';
import {useWorld} from './world';
import {FactionsTab} from './FactionsTab';
import {ChaptersTab} from './ChaptersTab';
import {HealthTab} from './HealthTab';
import {MoneyTab} from './MoneyTab';
import {ArtworkPanel} from './ArtworkPanel';
import {ReportsPanel} from './ReportsPanel';
import {LifePanel} from './LifePanel';
import {dmApi, signedIn, InjuryTypes, type Action, type Character, type Injury, type Me, type Players, type Target} from './api';

type Tab = 'npcs' | 'factions' | 'chapters' | 'stories' | 'players' | 'live' | 'health' | 'money';
// LIVE comes first and is where everyone starts: the world as it is now (Docs/Design/34-dungeon-master-refresh.md, 1.1).
const TABS: {id: Tab; label: string; icon: string; ready: boolean; blurb: string}[] = [
    {id: 'live', label: 'LIVE', icon: '●', ready: true, blurb: ''},
    {id: 'npcs', label: 'NPC Management', icon: '☺', ready: true, blurb: ''},
    {id: 'factions', label: 'Factions', icon: '⚑', ready: true,
        blurb: 'NPC factions, cities, and player guilds and clans; paint territory; set how they regard each other, up to war. Coming in phase 4.'},
    {id: 'chapters', label: 'Chapters', icon: '♞', ready: true, blurb: ''},
    {id: 'stories', label: 'Story Creator', icon: '✦', ready: false,
        blurb: 'Build, save and load multi-phase world stories with triggers and actions; test on DEV, run on PROD. Coming in phase 5.'},
    {id: 'players', label: 'Players', icon: '☺', ready: true, blurb: ''},
    {id: 'money', label: 'Money', icon: '¤', ready: true, blurb: ''},
    {id: 'health', label: 'Server Health', icon: '♥', ready: true, blurb: ''},
];

export function DmApp() {
    const [me, setMe] = useState<Me | null>(null);
    const [checking, setChecking] = useState(signedIn());
    useEffect(() => {
        if (!signedIn()) return;
        dmApi.me().then(setMe).catch(() => undefined).finally(() => setChecking(false));
    }, []);
    if (checking) return <div className="dm-center"><p className="hint">Checking your session…</p></div>;
    return me ? <Shell me={me} onSignOut={() => setMe(null)} /> : <SignIn onSignedIn={setMe} />;
}

function SignIn({onSignedIn}: {onSignedIn: (me: Me) => void}) {
    const [username, setUsername] = useState(''), [password, setPassword] = useState('');
    const [problem, setProblem] = useState(''), [busy, setBusy] = useState(false);
    const submit = async () => {
        setBusy(true); setProblem('');
        try { onSignedIn(await dmApi.login(username.trim(), password)); }
        catch (error) { setProblem((error as Error).message); setPassword(''); }
        finally { setBusy(false); }
    };
    return <div className="dm-center">
        <form className="dm-signin" onSubmit={e => { e.preventDefault(); void submit(); }}>
            <div className="brand"><span className="mark">D<i>›</i></span><div><b>DUNGEON MASTER</b><small>Runs Against the World</small></div></div>
            <p className="hint">Sign in with a Dungeon Master account. These are separate from player accounts.</p>
            <label className="field"><span>Account</span><input autoFocus autoComplete="username" value={username} onChange={e => setUsername(e.target.value)} /></label>
            <label className="field"><span>Password</span><input type="password" autoComplete="current-password" value={password} onChange={e => setPassword(e.target.value)} /></label>
            {problem && <p className="hint error-text">{problem}</p>}
            <button className="primary wide" disabled={busy || !username || !password}>{busy ? 'Signing in…' : 'Sign in'}</button>
        </form>
    </div>;
}

function Shell({me, onSignOut}: {me: Me; onSignOut: () => void}) {
    const [tab, setTab] = useState<Tab>('live');
    const [target, setTarget] = useState<Target>('prod');
    const current = TABS.find(t => t.id === tab)!;
    return <div className={target === 'prod' ? 'app dm prod' : 'app dm dev'}>
        <header className="topbar">
            <div className="brand"><span className="mark">D<i>›</i></span><div><b>DUNGEON MASTER</b><small>Runs Against the World</small></div></div>
            <div className="workspace-tabs" role="tablist">
                {TABS.map(t => <button key={t.id} role="tab" aria-selected={tab === t.id} className={tab === t.id ? 'on' : ''}
                    title={t.ready ? t.label : `${t.label}: not built yet`} onClick={() => setTab(t.id)}>{t.icon} {t.label}{!t.ready && <em>soon</em>}</button>)}
            </div>
            <div className="dm-target segmented" role="radiogroup" aria-label="Which world">
                <button role="radio" aria-checked={target === 'prod'} className={target === 'prod' ? 'on prod' : ''} onClick={() => setTarget('prod')}
                    title="The world players are in">PROD · players</button>
                <button role="radio" aria-checked={target === 'dev'} className={target === 'dev' ? 'on' : ''} onClick={() => setTarget('dev')}
                    title="The rehearsal world">DEV · rehearsal</button>
            </div>
            <div className="top-actions">
                <span className="dm-me" title={`Signed in as ${me.username}`}>{me.username}<small>{me.role}</small></span>
                <button onClick={() => { void dmApi.logout().finally(onSignOut); }}>Sign out</button>
            </div>
        </header>
        {tab === 'live' ? <LiveTab me={me} target={target} key={target} />
            : tab === 'npcs' ? <NpcTab me={me} target={target} key={target} />
            : tab === 'factions' ? <FactionsTab me={me} target={target} key={target} />
            : tab === 'chapters' ? <ChaptersTab me={me} target={target} key={target} />
            : tab === 'health' ? <HealthTab target={target} key={target} />
            : tab === 'money' ? <MoneyTab target={target} key={target} me={me} />
            : current.ready ? <PlayersTab me={me} target={target} key={target} />
            : <div className="dm-center"><div className="empty-sheet"><h2>{current.icon} {current.label}</h2><p>{current.blurb}</p></div></div>}
    </div>;
}

// --------------------------------------------------------------------------- Players

type Column = {key: string; label: string; title?: string; get: (c: Character) => number | string | null; fixed?: number};
// An attribute with its grade's mark (doc 49): "62 ▲" strong, "41 ▼" weak, the number alone plain.
const graded = (c: Character, id: string, v: number | null) =>
    v === null || v === undefined ? null : `${v.toFixed(0)}${c.grades?.[id] === 'strong' ? ' ▲' : c.grades?.[id] === 'weak' ? ' ▼' : ''}`;
const COLUMNS: Column[] = [
    {key: 'name', label: 'Name', get: c => c.name},
    {key: 'status', label: 'Status', get: c => (c.dead ? 'dead' : 'alive')},
    {key: 'dm', label: 'DM', title: 'A Dungeon Master in the game (has the Dev Console)', get: c => (c.dungeonMaster ? 'DM' : '')},
    {key: 'age', label: 'Age', get: c => c.age},
    {key: 'place', label: 'Place', get: c => c.place},
    {key: 'x', label: 'X', get: c => c.x, fixed: 1},
    {key: 'y', label: 'Y', get: c => c.y, fixed: 1},
    {key: 'str', label: 'STR', title: 'Strength (▲ strong, ▼ weak: doc 49)', get: c => graded(c, 'strength', c.stats.strength)},
    {key: 'dex', label: 'DEX', title: 'Dexterity (▲ strong, ▼ weak: doc 49)', get: c => graded(c, 'dexterity', c.stats.dexterity)},
    {key: 'wis', label: 'WIS', title: 'Wisdom (▲ strong, ▼ weak: doc 49)', get: c => graded(c, 'wisdom', c.stats.wisdom)},
    {key: 'sta', label: 'STA', title: 'Stamina', get: c => c.stats.stamina, fixed: 0},
    {key: 'account', label: 'Account', title: 'The account the character belongs to (doc 49)', get: c => c.account?.name ?? null},
    {key: 'handle', label: 'Handle', title: 'The account\'s public handle (doc 50): what friends and circles see', get: c => c.person?.handle || null},
    {key: 'hours', label: 'Hours', title: 'Hours played on the account, while at the keys (doc 50)', get: c => c.person?.playedHours ?? null, fixed: 1},
    {key: 'social', label: 'Social', title: 'The account\'s social level (roleplay alone: doc 49)', get: c => c.account?.socialLevel ?? null, fixed: 0},
    {key: 'fight', label: 'Fight', title: 'Fighting skill: grown by fighting (doc 49)', get: c => c.practice?.fighting?.value ?? null, fixed: 0},
    {key: 'sneak', label: 'Sneak', get: c => c.skills.sneakSkill, fixed: 0},
    {key: 'listen', label: 'Hearing', title: 'Hearing skill', get: c => c.skills.hearingSkill, fixed: 0},
    {key: 'scent', label: 'Scent', title: 'Scent skill', get: c => c.skills.scentSkill, fixed: 0},
    {key: 'saved', label: 'Last saved', get: c => c.saved},
];

function PlayersTab({me, target}: {me: Me; target: Target}) {
    const [data, setData] = useState<Players | null>(null);
    const world = useWorld(target);
    const [problem, setProblem] = useState('');
    const [query, setQuery] = useState(''), [status, setStatus] = useState<'all' | 'alive' | 'dead'>('all');
    const [sort, setSort] = useState<{key: string; up: boolean}>({key: 'name', up: true});
    const [selected, setSelected] = useState<string | null>(null);
    const load = useCallback(() => dmApi.players(target).then(d => { setData(d); setProblem(''); }).catch(e => setProblem((e as Error).message)), [target]);
    useEffect(() => { void load(); }, [load, target]);
    // While anything is queued, keep checking until the game server has answered.
    const waiting = data?.actions.some(a => a.status === 'queued');
    useEffect(() => { if (!waiting) return; const t = setTimeout(load, 1500); return () => clearTimeout(t); }, [waiting, data, load]);

    const rows = useMemo(() => {
        if (!data) return [];
        const q = query.trim().toLowerCase(), column = COLUMNS.find(c => c.key === sort.key)!;
        return data.characters
            .filter(c => (status === 'all' || (status === 'dead') === c.dead) && (!q || `${c.name} ${c.id} ${c.place}`.toLowerCase().includes(q)))
            .sort((a, b) => {
                const x = column.get(a), y = column.get(b);
                const order = x === y ? 0 : x === null ? 1 : y === null ? -1 : x < y ? -1 : 1;
                return sort.up ? order : -order;
            });
    }, [data, query, status, sort]);
    const chosen = data?.characters.find(c => c.id === selected) ?? null;

    return <div className="dm-players">
        <section className="dm-sheet">
            <div className="dm-sheet-bar">
                <input value={query} onChange={e => setQuery(e.target.value)} placeholder="Search characters or places…" aria-label="Search characters" />
                <div className="segmented">{(['all', 'alive', 'dead'] as const).map(s =>
                    <button key={s} className={status === s ? 'on' : ''} onClick={() => setStatus(s)}>{s}</button>)}</div>
                <span className="meta">{data ? `${rows.length} of ${data.characters.length} characters · ${data.world?.name ?? 'no world'}` : 'Loading…'}</span>
                <button onClick={() => void load()} title="Reload from the database">Refresh</button>
            </div>
            {problem && <p className="hint error-text">{problem}</p>}
            <div className="dm-table-wrap">
                <table className="dm-table">
                    <thead><tr>{COLUMNS.map(c => <th key={c.key} title={c.title} aria-sort={sort.key === c.key ? (sort.up ? 'ascending' : 'descending') : 'none'}>
                        <button onClick={() => setSort(s => ({key: c.key, up: s.key === c.key ? !s.up : true}))}>
                            {c.label}{sort.key === c.key ? (sort.up ? ' ▲' : ' ▼') : ''}</button></th>)}</tr></thead>
                    <tbody>{rows.map(c => <tr key={c.id} className={[c.id === selected ? 'on' : '', c.dead ? 'dead' : ''].join(' ')} onClick={() => setSelected(c.id)}>
                        {COLUMNS.map(col => {
                            const v = col.get(c);
                            const text = v === null || v === undefined ? '—' : col.key === 'saved' ? new Date(String(v)).toLocaleString()
                                : typeof v === 'number' && col.fixed !== undefined ? v.toFixed(col.fixed) : String(v);
                            return <td key={col.key} className={typeof v === 'number' ? 'num' : ''}>{text}</td>;
                        })}</tr>)}</tbody>
                </table>
                {data && !data.characters.length && <p className="hint dm-empty">No player characters in {target.toUpperCase()} yet. They appear here once the game server has saved them.</p>}
            </div>
        </section>
        <aside className="dm-side">
            <WorldMap world={world} characters={data?.characters ?? []} selected={selected} onSelect={setSelected} />
            {chosen ? <CharacterPanel me={me} target={target} character={chosen} actions={data!.actions.filter(a => a.target === chosen.id)} onAct={load} />
                : <p className="hint">Select a character in the table or on the map.</p>}
            <ReportsPanel me={me} target={target} />
            <ArtworkPanel me={me} target={target} />
        </aside>
    </div>;
}

// The families a player may have (Data/Gifts/families.json; Death Walkers are NPCs only).
const GIFT_FAMILIES = ['fire', 'earth', 'water', 'wind', 'sound', 'blinker', 'gravity', 'seer'];

function CharacterPanel({me, target, character, actions, onAct}: {me: Me; target: Target; character: Character; actions: Action[]; onAct: () => void}) {
    const [reason, setReason] = useState(''), [busy, setBusy] = useState(false), [problem, setProblem] = useState('');
    const canAct = me.role !== 'viewer';
    const act = async (kind: 'character.kill' | 'character.resurrect') => {
        const verb = kind === 'character.kill' ? 'Kill' : 'Resurrect';
        if (target === 'prod' && !window.confirm(`${verb} ${character.name} on PROD?`)) return;
        setBusy(true); setProblem('');
        try { await dmApi.act(target, kind, character.id, reason); setReason(''); onAct(); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };
    // A Gift (docs 33, 43): chosen at creation, and here the Dungeon Master gives one, changes it, or takes it away.
    const [family, setFamily] = useState(character.gift || 'fire');
    const gift = async (kind: string, quickened: boolean) => {
        const words = kind ? (quickened ? `Make Quickened (${kind})` : `Give the Gift (${kind}) to`) : 'Take the Gift from';
        if (target === 'prod' && !window.confirm(`${words} ${character.name} on PROD?`)) return;
        setBusy(true); setProblem('');
        try { await dmApi.act(target, 'character.gift', character.id, reason, {gift: kind, quickened}); setReason(''); onAct(); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };
    // An account's earned Gift tiers (doc 49, Phase 5): grant or revoke a tier, or hold new unlocks (in place of reports).
    const unlock = async (op: 'grant' | 'revoke' | 'hold' | 'release', tier: '' | 'gifted' | 'quickened', words: string) => {
        if (target === 'prod' && !window.confirm(`${words} on PROD?`)) return;
        setBusy(true); setProblem('');
        try { await dmApi.act(target, 'account.unlock', character.id, reason, {op, tier}); setReason(''); onAct(); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };
    // A Dungeon Master in the game: the player has the Dev Console (the ` key in the game), for trying things out.
    const markDm = async (on: boolean) => {
        if (target === 'prod' && !window.confirm(`${on ? 'Make' : 'Unmake'} ${character.name} a Dungeon Master in the game on PROD?`)) return;
        setBusy(true); setProblem('');
        try { await dmApi.act(target, 'character.dm', character.id, reason, {dungeonMaster: on}); setReason(''); onAct(); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };
    // Bandits called up near them (doc 33): a small camp a few strides off, for a fight that isn't with townsfolk.
    const callBandits = async (count: number) => {
        if (target === 'prod' && !window.confirm(`Call ${count} bandit${count > 1 ? 's' : ''} near ${character.name} on PROD?`)) return;
        setBusy(true); setProblem('');
        try { await dmApi.act(target, 'bandits.call', character.id, reason, {count}); setReason(''); onAct(); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };
    // Injuries (doc 38, phase 5): a correction, or a storyline's wound; given or taken away, online or offline.
    const [injuryType, setInjuryType] = useState('cracked_rib'), [injurySeverity, setInjurySeverity] = useState(2), [injurySide, setInjurySide] = useState('');
    const injure = async (payload: {add: string; severity: number; side: string} | {remove: string}, words: string) => {
        if (target === 'prod' && !window.confirm(`${words} on PROD?`)) return;
        setBusy(true); setProblem('');
        try { await dmApi.act(target, 'character.injury', character.id, reason, payload); setReason(''); onAct(); }
        catch (error) { setProblem((error as Error).message); }
        finally { setBusy(false); }
    };
    const injuryName = (i: Injury) => `${i.type.replace(/_acute$/, '').replace(/_/g, ' ')}${i.side ? ` (${i.side})` : ''}`;
    const pending = actions.some(a => a.status === 'queued');
    const stat = (label: string, v: number | null, digits = 0) => <div><b>{v === null || v === undefined ? '—' : v.toFixed(digits)}</b><span>{label}</span></div>;
    return <div className="dm-panel">
        <header><div><small>{character.dead ? 'DEAD' : 'PLAYER CHARACTER'}</small><h2>{character.name}</h2></div>
            <span className={character.dead ? 'dm-status dead' : 'dm-status'}>{character.dead ? '✝ dead' : 'alive'}</span></header>
        <p className="meta">{character.place} · {character.x.toFixed(1)}, {character.y.toFixed(1)}{character.indoors ? ' (indoors)' : ''} · age {character.age ?? '—'} · {character.posture}{character.activity ? ` · ${character.activity}` : ''}</p>
        <div className="stats">{stat('strength', character.stats.strength)}{stat('dexterity', character.stats.dexterity)}{stat('wisdom', character.stats.wisdom)}{stat('stamina', character.stats.stamina)}</div>
        {character.account ? <div className="meta" title="Earned Gift tiers (doc 49): opened by roleplay, or by a Dungeon Master.">
            Account <b>{character.account.name}</b> · social level {character.account.socialLevel ?? '—'}
            {' '}· Gifted {character.account.gifted ? `open (${character.account.gifted})` : 'locked'}
            {' '}· Quickened {character.account.quickened ? `open (${character.account.quickened})` : 'locked'}
            {character.account.hold ? ' · unlocks held' : ''}
            {' '}· {character.account.measures.normalScenes ?? 0} Normal scenes, {character.account.measures.stars ?? 0} stars from
            {' '}{character.account.measures.starGivers ?? 0} wolves, {character.account.measures.closedStories ?? 0} Stories
            {canAct && <div className="button-grid">
                <button disabled={busy || pending || !!character.account.gifted} onClick={() => void unlock('grant', 'gifted', `Open Gifted to ${character.account!.name}`)}>Grant Gifted</button>
                <button disabled={busy || pending || !!character.account.quickened} onClick={() => void unlock('grant', 'quickened', `Open Quickened to ${character.account!.name}`)}>Grant Quickened</button>
                <button title="Stands while the account's unlocks are held: released, an earned tier opens again." disabled={busy || pending || !character.account.quickened} onClick={() => void unlock('revoke', 'quickened', `Take Quickened from ${character.account!.name}`)}>Revoke Quickened</button>
                <button disabled={busy || pending} onClick={() => void unlock(character.account!.hold ? 'release' : 'hold', '', `${character.account!.hold ? 'Release' : 'Hold'} ${character.account!.name}'s unlocks`)}>
                    {character.account.hold ? 'Release unlocks' : 'Hold unlocks'}</button>
            </div>}
        </div> : <p className="meta">Account: unknown (a development identity, or not saved since doc 49's migration 0034)</p>}
        {character.profile && <details className="meta"><summary>Profile (read only, doc 50){character.person ? ` · ${character.person.handle || 'no handle'}, ${character.person.experience}, ${character.person.playedHours} h played` : ''}</summary>
            {(['currently', 'description', 'pronouns', 'title', 'motto', 'oocNotes', 'history', 'otherLimits'] as const).map(k =>
                typeof character.profile?.[k] === 'string' && character.profile[k] ? <p key={k}><b>{k}</b>: {String(character.profile[k])}</p> : null)}
            {Array.isArray(character.profile.glances) && <p><b>glances</b>: {(character.profile.glances as Array<{title: string; line: string; sense: string}>)
                .map(g => `${g.title}${g.line ? ` (${g.line})` : ''}${g.sense !== 'sight' ? ` [${g.sense}]` : ''}`).join('; ')}</p>}
            <p><b>status</b>: {String(character.profile.status ?? 'ic')}{character.profile.walkup ? ' · walk-up friendly' : ''}{character.profile.mature ? ' · mature' : ''}</p>
        </details>}
        <p className="meta">Gift: {character.gift ? `${character.gift}${character.quickened ? ' · Quickened' : ' · Gifted'}` : 'none'}</p>
        <div className="meta">Injuries: {(character.injuries ?? []).length ? <ul className="dm-injuries">{(character.injuries ?? []).map(i =>
            <li key={i.id}><b>{injuryName(i)}</b> · {i.kind === 'acute' ? `${['', 'minor', 'moderate', 'severe'][i.severity] ?? ''}, ${((i.restLeft ?? 0) / 24).toFixed(1)} days of rest left` : 'lasting'}
                {i.from ? ` · from ${i.from}` : ''}
                {canAct && <button className="small" disabled={busy || pending} onClick={() => void injure({remove: i.id}, `Take ${injuryName(i)} from ${character.name}`)}>Take away</button>}</li>)}</ul> : 'none'}</div>
        <p className="meta">Dungeon Master in the game: {character.dungeonMaster ? <b>yes</b> : 'no'}
            {character.dungeonMaster ? ' · has the Dev Console (the ` key in the game)' : ''}
            {pending && actions.some(a => a.kind === 'character.dm' && a.status === 'queued') ? ' · changing: waiting for the game server…' : ''}</p>
        <div className="stats">{stat('sneak', character.skills.sneakSkill)}{stat('hearing skill', character.skills.hearingSkill)}{stat('scent skill', character.skills.scentSkill)}
            {stat('vision', character.senses.vision, 2)}</div>
        <p className="meta">Build: {Object.entries(character.grades ?? {}).map(([a, g]) => `${g} ${a}`).join(', ') || 'plain'}
            {character.specialty ? ` · specialty ${character.specialty}` : ''}</p>
        {Object.keys(character.practice ?? {}).length > 0 && <p className="meta" title="Grown by practice (doc 49): each value out of its cap.">Practice: {
            Object.values(character.practice ?? {}).map(p => `${p.name} ${p.value.toFixed(p.cap < 5 ? 2 : 0)} / ${p.cap < 5 ? p.cap.toFixed(2) : p.cap}`).join(' · ')}</p>}
        {canAct ? <>
            <label className="field"><span>Reason (for the audit log)</span><input value={reason} maxLength={500} onChange={e => setReason(e.target.value)} placeholder="Optional" /></label>
            <div className="button-grid">
                <button className="danger" disabled={busy || pending || character.dead} onClick={() => void act('character.kill')}>✝ Kill</button>
                <button disabled={busy || pending || !character.dead} onClick={() => void act('character.resurrect')}>Resurrect</button>
            </div>
            <div className="button-grid">
                <label className="field"><span>Gift family</span>
                    <select value={family} onChange={e => setFamily(e.target.value)}>
                        {GIFT_FAMILIES.map(f => <option key={f} value={f}>{f}</option>)}</select></label>
                <button disabled={busy || pending || (character.gift === family && !character.quickened)} onClick={() => void gift(family, false)}>Make Gifted</button>
                <button disabled={busy || pending || (character.gift === family && character.quickened)} onClick={() => void gift(family, true)}>Make Quickened</button>
                <button disabled={busy || pending || !character.gift} onClick={() => void gift('', false)}>Take Gift away</button>
            </div>
            {me.role === 'admin' && <div className="button-grid">
                <button disabled={busy || pending} onClick={() => void markDm(!character.dungeonMaster)}
                    title="A Dungeon Master in the game has the Dev Console (the ` key) for trying things out">
                    {character.dungeonMaster ? 'Unmake Dungeon Master' : 'Make Dungeon Master in game'}</button>
            </div>}
            <div className="button-grid">
                <select value={injuryType} onChange={e => setInjuryType(e.target.value)} aria-label="Injury">
                    {InjuryTypes.map(([t, k]) => <option key={t} value={t}>{t.replace(/_acute$/, '').replace(/_/g, ' ')} ({k})</option>)}</select>
                <select value={injurySeverity} onChange={e => setInjurySeverity(Number(e.target.value))} aria-label="Severity"
                    disabled={InjuryTypes.find(([t]) => t === injuryType)?.[1] === 'lasting'}>
                    <option value={1}>minor</option><option value={2}>moderate</option><option value={3}>severe</option></select>
                <select value={injurySide} onChange={e => setInjurySide(e.target.value)} aria-label="Side">
                    <option value="">either side</option><option value="left">left</option><option value="right">right</option></select>
                <button disabled={busy || pending} onClick={() => void injure({add: injuryType, severity: injurySeverity, side: injurySide},
                    `Give ${character.name} a ${injuryType.replace(/_acute$/, '').replace(/_/g, ' ')}`)}>Give injury</button>
            </div>
            <div className="button-grid">
                <button disabled={busy || pending || character.dead} onClick={() => void callBandits(1)}>Call a bandit near them</button>
                <button disabled={busy || pending || character.dead} onClick={() => void callBandits(3)}>Call three bandits</button>
            </div>
            <p className="hint">The game server applies it within a second, online or offline. Nothing happens until a {target.toUpperCase()} server is running; unapplied actions expire after ten minutes.</p>
        </> : <p className="hint">Your account can view but not change the world.</p>}
        {problem && <p className="hint error-text">{problem}</p>}
        {actions.length > 0 && <ol className="dm-actions">{actions.slice(0, 6).map(a =>
            <li key={a.id} className={a.status}><b>{a.kind.replace('character.', '')}</b> by {a.by} · {new Date(a.at).toLocaleTimeString()}
                <span>{a.status === 'queued' ? 'waiting for the game server…' : `${a.status}: ${a.result}`}</span></li>)}</ol>}
        <LifePanel me={me} target={target} id={character.id} name={character.name} />
    </div>;
}

/** The world, with each character plotted: interiors at their overview position. Click a dot to select. */
function WorldMap({world, characters, selected, onSelect}: {world: Project | null; characters: Character[]; selected: string | null; onSelect: (id: string) => void}) {
    const canvas = useRef<HTMLCanvasElement>(null);
    const size = {w: 460, h: 320};
    useGlyphRender();                                    // Redraw once the glyph font loads.
    const layout = useMemo(() => {
        if (!world) return null;
        const surface = surfaceFor(world, {kind: 'world'}), box = M.cellBounds(world) ?? {x: 0, y: 0, width: 16, height: 16};
        const s = Math.min(size.w / box.width, size.h / box.height);
        return {surface, cam: {s, x: (size.w - box.width * s) / 2 - box.x * s, y: (size.h - box.height * s) / 2 - box.y * s}};
    }, [world]); // eslint-disable-line react-hooks/exhaustive-deps
    const at = (c: Character) => layout && c.worldX !== null && c.worldY !== null
        ? [layout.cam.x + c.worldX * layout.cam.s, layout.cam.y + c.worldY * layout.cam.s] : null;
    useEffect(() => {
        const g = canvas.current?.getContext('2d');
        if (!g) return;
        g.fillStyle = '#0d1413'; g.fillRect(0, 0, size.w, size.h);
        if (!world || !layout) return;
        const {s, x: ox, y: oy} = layout.cam;
        drawGround(g, layout.surface, layout.cam, size.w, size.h, true);   // Elevation shading.
        g.strokeStyle = 'rgba(200,210,190,.35)';
        for (const c of world.cells) g.strokeRect(ox + c.x * s + .5, oy + c.y * s + .5, c.width * s - 1, c.height * s - 1);
        for (const c of characters) {
            const p = at(c);
            if (!p) continue;
            const on = c.id === selected;
            g.beginPath(); g.arc(p[0], p[1], on ? 6 : 4, 0, Math.PI * 2);
            g.fillStyle = c.dead ? '#8b8b8b' : on ? '#f0d79a' : '#a8ceda'; g.fill();
            g.lineWidth = c.indoors ? 2 : 1; g.strokeStyle = '#0d1413'; g.stroke();
            if (on) { g.font = '600 12px Inter, system-ui, sans-serif'; g.fillStyle = '#f0d79a'; g.fillText(c.name, p[0] + 8, p[1] - 6); }
        }
    }); // Redraw on every render: the map is small.
    const click = (e: React.MouseEvent<HTMLCanvasElement>) => {
        const r = e.currentTarget.getBoundingClientRect(), mx = e.clientX - r.left, my = e.clientY - r.top;
        let best: Character | null = null, dist = 12;
        for (const c of characters) {
            const p = at(c);
            if (p && Math.hypot(p[0] - mx, p[1] - my) < dist) { best = c; dist = Math.hypot(p[0] - mx, p[1] - my); }
        }
        if (best) onSelect(best.id);
    };
    return <div className="dm-map"><canvas ref={canvas} width={size.w} height={size.h} onClick={click}
        aria-label="World map with each player character's last saved position" />
        {!world && <p className="hint">The world map is not available.</p>}</div>;
}

