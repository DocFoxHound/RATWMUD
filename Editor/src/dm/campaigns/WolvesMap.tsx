// Wolves & map (doc 85, 5.3): the story's wolves on LIVE's map (from the frame LIVE reads: in the world now, or where
// they were saved), the story's regions drawn, the residents it directs marked, and beside it who is where and what
// each last did in the story.
import {useEffect, useMemo, useState} from 'react';
import type {Place} from '../../model/model.mjs';
import {surfaceFor} from '../../lib/surface';
import {NO_MAP_YET, noMapYet} from '../../lib/hosted';
import {MapView, type Marker, type Overlay} from '../MapView';
import {useWorld} from '../world';
import type {Live, Target} from '../api';
import {nameOf, wolvesNow, type FramePerson, type RunDetail, type TimelineItem} from '../../lib/campaigns';

type View = {kind: 'world'} | {kind: 'cell'; id: string};
const HERE = '#7fc8f8', AWAY = '#7d8a85', DIRECTED = '#e6c481', FOCUS = '#e07a6a', REGION = 'rgba(217,182,123,.55)';

export function WolvesMap({target, detail, names, live, items, focus}: {target: Target; detail: RunDetail; names: Record<string, string>;
    live: Live | null; items: TimelineItem[]; focus: {cell: string; x: number; y: number} | null}) {
    const [problem, setProblem] = useState('');
    const world = useWorld(target, setProblem);
    const people = (live?.frame?.people ?? []) as FramePerson[];
    const wolves = useMemo(() => wolvesNow(detail.state?.cohort ?? [], people, names), [detail.state?.cohort, people, names]);
    const busiest = useMemo(() => {
        const counts = new Map<string, number>();
        for (const w of wolves) if (w.here) counts.set(w.cell, (counts.get(w.cell) ?? 0) + 1);
        return [...counts].sort((a, b) => b[1] - a[1])[0]?.[0] ?? detail.regions[0]?.cell ?? '';
    }, [wolves, detail.regions]);
    const [view, setView] = useState<View>(focus ? {kind: 'cell', id: focus.cell} : busiest ? {kind: 'cell', id: busiest} : {kind: 'world'});
    useEffect(() => { if (focus) setView({kind: 'cell', id: focus.cell}); }, [focus]);
    const [selected, setSelected] = useState<string | null>(null);
    const surface = useMemo(() => {
        if (!world) return null;
        const known = view.kind === 'cell' && (world.cells.some(c => c.id === view.id) || world.rooms.some(r => r.id === view.id));
        return surfaceFor(world, (known ? view : {kind: 'world'}) as never);
    }, [world, view]);
    const at = (cell: string, x: number, y: number): [number, number] | null => surface?.fromPlace({cell, x, y} as Place) ?? null;
    const lastBy = useMemo(() => {
        const m = new Map<string, TimelineItem>();
        for (const i of items) if (i.type === 'trigger' && i.actor && !m.has(i.actor)) m.set(i.actor, i);
        return m;
    }, [items]);
    const markers = useMemo((): Marker[] => {
        const out: Marker[] = [];
        const push = (id: string, p: [number, number] | null, color: string, glyph: string, label: string) => {
            if (p) out.push({id, x: p[0] + .5, y: p[1] + .5, color, glyph, label, dead: false, editable: false});
        };
        const directed = new Set(detail.directed);
        for (const p of people) if (directed.has(p[0])) push(`n:${p[0]}`, at(p[3], p[4], p[5]), DIRECTED, '☻', `${p[1]} · in the story's cast`);
        for (const w of wolves) if (w.known) push(`p:${w.id}`, at(w.cell, w.x, w.y), w.here ? HERE : AWAY, '☺', `${w.name}${w.here ? '' : ' · not in the world (where saved)'}`);
        if (focus) push('focus', at(focus.cell, focus.x, focus.y), FOCUS, '◎', 'where it happened');
        return out;
    }, [people, wolves, detail.directed, focus, surface]); // eslint-disable-line react-hooks/exhaustive-deps
    const overlay = useMemo((): Overlay => {
        const out: Overlay = {tiles: [], rects: [], paths: [], circles: []};
        for (const r of detail.regions) {
            const p = at(r.cell, r.x1, r.y1);
            if (p) out.rects.push({x: p[0], y: p[1], w: r.x2 - r.x1 + 1, h: r.y2 - r.y1 + 1, color: REGION, strong: true, label: r.id});
        }
        return out;
    }, [detail.regions, surface]); // eslint-disable-line react-hooks/exhaustive-deps
    const placeName = (cell: string) => world?.cells.find(c => c.id === cell)?.name ?? world?.rooms.find(r => r.id === cell)?.name ?? nameOf(names, cell);
    const here = wolves.filter(w => w.here).length;
    return <div className="camp-map">
        <div className="camp-map-stage">
            <div className="crumbs">
                {view.kind === 'cell' && <button onClick={() => setView({kind: 'world'})}>◂ World</button>}
                <span className="crumb-label">{view.kind === 'world' ? 'World' : placeName(view.id)}</span>
                <span className={live?.frame ? 'live-feed' : 'live-feed stale'}>{live?.frame ? `● positions ${Math.round(live.age ?? 0)} s old` : 'No positions yet: is the game server running?'}</span>
            </div>
            {noMapYet(world) ? <p className="hint">{NO_MAP_YET}</p> : surface && world
                ? <MapView surface={surface} world={world} markers={markers} overlay={overlay} selected={selected} mode="select" cluster={4}
                    onClick={(_p, m) => setSelected(m?.id ?? null)} onPaint={() => undefined} onOpen={id => setView({kind: 'cell', id})} />
                : <p className="hint">{problem || 'Loading the map…'}</p>}
        </div>
        <aside className="camp-map-list">
            <h3>{here} of {wolves.length} here now</h3>
            <ul>{wolves.slice(0, 300).map(w => {
                const last = lastBy.get(w.id);
                return <li key={w.id} className={selected === `p:${w.id}` ? 'on' : ''}>
                    <button className="link-button" disabled={!w.known} onClick={() => { setSelected(`p:${w.id}`); setView({kind: 'cell', id: w.cell}); }}>
                        <span className={w.here ? 'camp-here' : 'camp-away'}>{w.here ? '●' : '○'}</span> {w.name}</button>
                    <small>{w.known ? `${placeName(w.cell)} (${Math.floor(w.x)}, ${Math.floor(w.y)})${w.here && w.doing ? ' · ' + w.doing : ''}` : 'not on the map'}
                        {last && last.type === 'trigger' ? ` · last in the story: ${last.trigger}, day ${last.day.toFixed(1)}` : ''}</small></li>;
            })}</ul>
            {detail.regions.length > 0 && <><h3>Regions</h3><ul>{detail.regions.map(r => <li key={r.id}>
                <button className="link-button" onClick={() => setView({kind: 'cell', id: r.cell})}>{r.id}</button>
                <small>{placeName(r.cell)} ({r.x1}, {r.y1})-({r.x2}, {r.y2})</small></li>)}</ul></>}
            {detail.directed.length > 0 && <><h3>Residents it directs</h3><ul>{detail.directed.map(id => {
                const p = people.find(x => x[0] === id);
                return <li key={id}><span style={{color: DIRECTED}}>☻</span> {nameOf(names, id)}<small>{p ? ` ${placeName(p[3])} (${Math.floor(p[4])}, ${Math.floor(p[5])})` : ' not in the frame'}</small></li>;
            })}</ul></>}
            <p className="hint">Parties are not shown yet: the game does not tell the story runner about them (doc 85, Phase 8).</p>
        </aside>
    </div>;
}
