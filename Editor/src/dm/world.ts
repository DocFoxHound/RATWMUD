// The world map a Dungeon Master tab draws, loaded lean: cell outlines and previews at once, each cell's ground as it
// comes into view (lib/lazyGround.ts). Only one tab is open at a time, so they share the one waiting list.
import {useEffect, useRef, useState} from 'react';
import type {Project} from '../model/model.mjs';
import {adoptLean, groundPending, resetGround, settleCell, startGround} from '../lib/lazyGround';
import {dmApi, type Target} from './api';

export function useWorld(target: Target, onError?: (message: string) => void): Project | null {
    const [world, setWorld] = useState<Project | null>(null);
    const current = useRef<Project | null>(null);
    useEffect(() => {
        let live = true;
        current.current = null;
        setWorld(null);
        startGround(ids => dmApi.ground(target, ids), (arrived) => {
            const w = current.current;
            if (!w) return [];
            const taken: string[] = [];
            const cells = w.cells.map(c => {
                const g = arrived.get(c.id);
                if (!g || !groundPending(c.id) || g.x !== c.x || g.y !== c.y || g.width !== c.width || g.height !== c.height) return c;
                settleCell(c.id);
                taken.push(c.id);
                return {...c, terrain: g.terrain, heights: g.heights};
            });
            if (taken.length) setWorld(current.current = {...w, cells});
            return taken;
        });
        dmApi.world(target).then(value => {
            if (!live) return;
            setWorld(current.current = adoptLean(value as never) as Project);
        }).catch(e => { if (live) { setWorld(null); onError?.((e as Error).message); } });
        return () => { live = false; resetGround(); };
    }, [target]); // eslint-disable-line react-hooks/exhaustive-deps
    return world;
}
