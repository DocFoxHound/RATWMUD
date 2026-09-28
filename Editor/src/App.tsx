import {useCallback, useEffect} from 'react';
import {Explorer} from './components/Explorer';
import {Inspector} from './components/Inspector';
import {MapCanvas} from './components/MapCanvas';
import {CommandPalette, ProblemsPanel, StatusBar, Toasts, ToolOptions, ToolRail, TopBar, useGlobalKeys} from './components/Chrome';
import {Dialogs} from './components/Dialogs';
import {handleShortcut} from './lib/commands';
import {isRoomView, setState, useStore, useTool} from './lib/store';
import {savedIdentity} from './lib/live';
import {startSession} from './lib/session';
import {api} from './lib/api';
import {CharactersWorkspace, loadRoster} from './components/Characters';

export function App() {
    useGlobalKeys(useCallback(handleShortcut, []));
    const workspace = useStore(s => s.workspace);
    const view = useStore(s => s.view);
    // Keep "who fills each profession slot" current; a cheap, read-only local call.
    const project = useStore(s => s.project), roster = useStore(s => s.savedRoster);
    useEffect(() => {
        if (!project.slots.length) { setState({preview: null}); return; }
        const timer = setTimeout(() => { api.preview(project, {roster}).then(preview => setState({preview})).catch(() => undefined); }, 700);
        return () => clearTimeout(timer);
    }, [project, roster]);
    useEffect(() => {
        void loadRoster();
        const identity = savedIdentity();
        if (identity) setState({identity});
        void startSession();          // Asks who you are first if this browser does not know yet.
    }, []);
    return (
        <div className="app">
            <TopBar />
            {workspace === 'characters' ? <CharactersWorkspace /> : <div className="workspace">
                <ToolRail />
                <Explorer />
                <main className="stage">
                    <ToolOptions />
                    {workspace === 'interiors' && !isRoomView(project, view) ? <NoInteriors /> : <MapCanvas />}
                    <ProblemsPanel />
                </main>
                <Inspector />
            </div>}
            <StatusBar />
            <Dialogs />
            <CommandPalette />
            <Toasts />
        </div>
    );
}

function NoInteriors() {
    return <div className="map-wrap"><div className="empty-sheet">
        <h2>No interiors yet</h2>
        <p>Interiors are the insides of places: a shop, a house, a cave. Each is its own map, joined to the world by doors, passages or stairs.</p>
        <p><button className="primary" onClick={() => setState({dialog: 'room'})}>+ New blank interior…</button>{' '}
            <button onClick={() => useTool('building')}>⌂ Place a building on the Map…</button></p>
    </div></div>;
}
