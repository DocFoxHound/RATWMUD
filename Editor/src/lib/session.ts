// Starts live editing once the editor has said who they are: loads the one world,
// then keeps it in step with everyone else (lib/live.ts).
import * as M from '../model/model.mjs';
import {api} from './api';
import {colorFor, saveIdentity, startLive, type Identity, type Presence} from './live';
import {adoptLean, startGround} from './lazyGround';
import {applyRemote, connectLive, getState, installGround, loadProject, refuseBatch, setState, toast} from './store';

let starting = false;

export async function startSession() {
    const identity = getState().identity;
    if (!identity) { setState({dialog: 'identity'}); return; }
    if (starting) return;
    starting = true;
    let seq: number;
    try {
        // Cell outlines and previews now; each cell's ground as it comes into view (lib/lazyGround.ts).
        const world = await api.live.load();
        startGround(api.live.ground, installGround);
        if (!loadProject(adoptLean(world.project as never))) throw new Error('The world from the database could not be read.');
        seq = world.seq;
    } catch (error) {
        starting = false;
        setState({live: {state: 'offline', pending: 0, error: (error as Error).message}});
        setTimeout(startSession, 4000);
        return;
    }
    const link = startLive(api.live, seq, {
        identity: () => getState().identity!,
        world: () => getState().project.id,
        presence,
        remote: changes => applyRemote(changes, c => getState().editors.find(e => e.clientId === c.clientId)?.color ?? colorFor(c.editor)),
        refused: refuseBatch,
        status: live => setState({live}),
        editors: editors => setState({editors}),
        reload: () => {
            connectLive(null);
            starting = false;
            toast('A lot changed while this editor was away; reloading the world.');
            void startSession();
        },
    });
    connectLive(link);
}

export function setIdentity(identity: Identity) {
    saveIdentity(identity);
    setState({identity, dialog: null});
    void startSession();
}

/** Where this editor is and what they have in hand, for everyone else's view. */
function presence(): Presence {
    const s = getState(), p = s.project, sel = s.selection;
    const named = (list: {id: string; name: string}[], id: string) => list.find(x => x.id === id)?.name ?? id;
    const doing = !sel ? '' : sel.kind === 'cells' ? sel.ids.map(id => M.getCell(p, id)?.name ?? id).join(', ')
        : sel.kind === 'person' ? named(p.people, sel.id) : sel.kind === 'slot' ? named(p.slots, sel.id)
        : sel.kind === 'route' ? named(p.routes, sel.id) : sel.kind === 'link' ? named(p.links, sel.id)
        : sel.kind === 'spawn' ? 'the player spawn' : 'the herb patch';
    return {workspace: s.workspace, view: s.view.kind === 'cell' ? s.view.id : 'world', place: s.hover?.place ?? null, doing};
}
