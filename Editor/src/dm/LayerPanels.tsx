// NPC layers in NPC Management: patrol routes, painted areas and spawn rules (Docs/Design/21-dungeon-master.md, phase 3).
// Each panel edits a draft; saving writes it to the chosen database and a running game server takes it within seconds.
import type {Person, Project, Route} from '../model/model.mjs';
import {placeLabel} from '../lib/surface';
import {NumberField, Row, SelectField, TextField, Toggle, Hint} from '../components/fields';
import type {AreaKind, NpcArea, SpawnRule, Target} from './api';

export type Layer = 'npcs' | 'routes' | 'areas' | 'spawns';
export const LAYERS: {id: Layer; label: string; icon: string}[] = [
    {id: 'npcs', label: 'NPCs', icon: '☺'}, {id: 'routes', label: 'Routes', icon: '⤳'},
    {id: 'areas', label: 'Areas', icon: '▦'}, {id: 'spawns', label: 'Spawns', icon: '✺'},
];
export const AREA_KINDS: {id: AreaKind; label: string; color: string; blurb: string}[] = [
    {id: 'wander', label: 'Wander', color: '#6fbf8f', blurb: 'NPCs given this area roam its open tiles in their free time.'},
    {id: 'spawn', label: 'Spawn', color: '#d98b5f', blurb: 'Where spawn rules bring new NPCs into the world.'},
    {id: 'plan', label: 'Plan', color: '#7f9fd6', blurb: 'A marked zone for planning; no effect in play yet.'},
];
export const areaColor = (kind: AreaKind) => AREA_KINDS.find(k => k.id === kind)?.color ?? '#aaa';
export type Brush = 'paint' | 'erase';
export type SpawnDraft = Omit<SpawnRule, 'alive' | 'dead'>;

/** A new ID from a name, not clashing with `taken` (IDs never change once saved). Spawn rules keep theirs short: the
 *  NPCs they make are named after them with a suffix. */
export function idFrom(name: string, taken: Iterable<string>, fallback: string, max = 36) {
    const used = new Set(taken);
    const base = (name.toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '').slice(0, max).replace(/_+$/, '') || fallback).replace(/^(?=[^a-z])/, 'n_');
    let id = base;
    for (let n = 2; used.has(id); n++) id = `${base}_${n}`;
    return id;
}

interface Common { target: Target; canAct: boolean; busy: boolean; dirty: boolean; isNew: boolean; onSave: () => void; onRevert: () => void; onDelete: () => void }

function Buttons({target, canAct, busy, dirty, isNew, onSave, onRevert, onDelete, ready = true}: Common & {ready?: boolean}) {
    if (!canAct) return null;
    return <div className="button-grid">
        <button className={target === 'prod' ? 'publish' : 'primary'} disabled={busy || !dirty || !ready} onClick={onSave}>{busy ? 'Saving…' : `Save to ${target.toUpperCase()}`}</button>
        <button disabled={busy || !dirty} onClick={onRevert}>{isNew ? 'Discard' : 'Revert'}</button>
        {!isNew && <button className="danger" disabled={busy} onClick={onDelete}>Delete</button>}
    </div>;
}

export function RouteInspector({route, setRoute, world, users, adding, setAdding, ...common}: Common & {
    route: Route; setRoute: (r: Route) => void; world: Project; users: string[]; adding: boolean; setAdding: (on: boolean) => void;
}) {
    const move = (i: number, by: number) => {
        const posts = [...route.posts];
        [posts[i], posts[i + by]] = [posts[i + by], posts[i]];
        setRoute({...route, posts});
    };
    return <div className="dm-panel">
        <header><div><small>{common.isNew ? 'NEW PATROL ROUTE · NOT SAVED' : 'PATROL ROUTE'}</small><h2>{route.name}</h2></div></header>
        <fieldset disabled={!common.canAct} className="dm-fields">
            <TextField label="Name" value={route.name} max={80} onCommit={v => setRoute({...route, name: v})} />
            <div className="field"><span>Posts, walked in order and back to the first</span>
                <ol className="dm-posts">{route.posts.map((p, i) => <li key={i}><b>{i + 1}</b><span>{placeLabel(world, p)}</span>
                    <button className="icon" title="Earlier" disabled={i === 0} onClick={() => move(i, -1)}>↑</button>
                    <button className="icon" title="Later" disabled={i === route.posts.length - 1} onClick={() => move(i, 1)}>↓</button>
                    <button className="icon" title="Remove this post" onClick={() => setRoute({...route, posts: route.posts.filter((_, j) => j !== i)})}>✕</button></li>)}</ol>
                {!route.posts.length && <p className="hint">No posts yet. Add them on the map.</p>}
                <button className={adding ? 'on' : ''} onClick={() => setAdding(!adding)}>{adding ? 'Done adding posts' : '+ Add posts on the map'}</button>
            </div>
        </fieldset>
        <Hint>{users.length ? `Walked by ${users.join(', ')}.` : 'No one walks this route yet; choose it as a guard’s “Patrol route” or a civilian’s “Travel route”.'}</Hint>
        <Buttons {...common} ready={route.posts.length > 0} />
    </div>;
}

export function AreaInspector({area, setArea, world, brush, setBrush, wanderers, rules, ...common}: Common & {
    area: NpcArea; setArea: (a: NpcArea) => void; world: Project; brush: Brush; setBrush: (b: Brush) => void; wanderers: string[]; rules: string[];
}) {
    const kind = AREA_KINDS.find(k => k.id === area.kind)!;
    const place = world.cells.find(c => c.id === area.cell) ?? world.rooms.find(r => r.id === area.cell);
    return <div className="dm-panel">
        <header><div><small>{common.isNew ? 'NEW AREA · NOT SAVED' : `${kind.label.toUpperCase()} AREA`}</small><h2>{area.name}</h2></div>
            <span className="role-chip" style={{borderColor: kind.color, color: kind.color}}>▦</span></header>
        <fieldset disabled={!common.canAct} className="dm-fields">
            <TextField label="Name" value={area.name} max={120} onCommit={v => setArea({...area, name: v})} />
            <SelectField label="Kind" value={area.kind} hint={kind.blurb} options={AREA_KINDS.map(k => ({value: k.id, label: k.label}))}
                onChange={v => setArea({...area, kind: v as AreaKind})} />
            <div className="field"><span>Brush</span>
                <div className="segmented">{(['paint', 'erase'] as const).map(b => <button key={b} className={brush === b ? 'on' : ''} onClick={() => setBrush(b)}>
                    {b === 'paint' ? '▦ Paint' : '▢ Erase'}</button>)}</div></div>
            <p className="meta">{area.tiles.length} tiles{place ? ` in ${place.name}` : ''} · drag on the map to {brush}; right-drag pans.
                {area.tiles.length > 0 && <> <button className="link-button" onClick={() => setArea({...area, tiles: []})}>Clear</button></>}</p>
        </fieldset>
        {area.kind === 'wander' && <Hint>{wanderers.length ? `Wandered by ${wanderers.join(', ')}.` : 'Nobody wanders here yet; choose it in an NPC’s “Wanders in”.'}
            {' '}Walls and water inside the area are ignored.</Hint>}
        {rules.length > 0 && <Hint>Spawn rules here: {rules.join(', ')}. Deleting the area deletes them too.</Hint>}
        <Buttons {...common} ready={!!area.cell} />
    </div>;
}

export function SpawnInspector({rule, setRule, status, areas, templates, ...common}: Common & {
    rule: SpawnDraft; setRule: (r: SpawnDraft) => void; status: SpawnRule | null; areas: NpcArea[]; templates: Person[];
}) {
    return <div className="dm-panel">
        <header><div><small>{common.isNew ? 'NEW SPAWN RULE · NOT SAVED' : 'SPAWN RULE'}</small><h2>{rule.name}</h2></div>
            {status && <span className={status.enabled ? 'dm-status' : 'dm-status dead'}>{status.alive}/{status.count} alive</span>}</header>
        <fieldset disabled={!common.canAct} className="dm-fields">
            <TextField label="Name" value={rule.name} max={80} onCommit={v => setRule({...rule, name: v})} />
            <SelectField label="Appear in" value={rule.area} options={[{value: '', label: 'Choose an area…'}, ...areas.map(a => ({value: a.id, label: `${a.name} (${a.kind})`}))]}
                onChange={v => setRule({...rule, area: v})} />
            <SelectField label="Made from" hint="New NPCs copy this NPC: role, looks, schedule hours, personality. They live, work and spend evenings where they appear, and wander the area."
                value={rule.template} options={[{value: '', label: 'Choose a named NPC…'}, ...templates.map(p => ({value: p.id, label: `${p.name} · ${p.workLabel}`}))]}
                onChange={v => setRule({...rule, template: v})} />
            <Row><NumberField label="Keep alive" value={rule.count} min={1} max={50} onCommit={v => setRule({...rule, count: v})} />
                <NumberField label="Respawn after" suffix="min" value={rule.respawnMinutes} min={0} max={10080} onCommit={v => setRule({...rule, respawnMinutes: v})} /></Row>
            <Toggle label="Running" hint="Off: nobody new appears; those already here stay." checked={rule.enabled} onChange={v => setRule({...rule, enabled: v})} />
        </fieldset>
        {status && <Hint>{status.alive} alive and {status.dead} fallen. A fallen NPC is cleared {rule.respawnMinutes} game-server minutes after death, then a new one appears.</Hint>}
        <Hint>The game server checks its rules every ten seconds; no restart needed.</Hint>
        <Buttons {...common} ready={!!rule.area && !!rule.template} />
    </div>;
}
