import {useEffect, useState} from 'react';
import {dmApi, worldName, type Me, type Npcs, type Target} from './api';
import {useWorld} from './world';
import {SceneWorkspace} from './SceneWorkspace';
import {StoryProgramPanel} from './StoryProgramPanel';

// The Story Creator (Docs/Design/85, section 4): world stories in the trigger workbench, and scenes. The older "Write
// draft" and "Inspect rehearsal" tabs, a three-trigger draft that lived only in the browser and that no tool read, were
// retired in Phase 4 (4.1 decision 6): "New… → A small story" is what they were for.
export function StoryCreator({me, target, openPersonal, openCampaigns}: {me: Me; target: Target; openPersonal: () => void;
    openCampaigns?: (run: string) => void}) {
    const key = 'ratw.story-creator.v1.' + JSON.stringify([target, me.account || me.username]);
    const [fileError, setFileError] = useState('');
    const [catalogError, setCatalogError] = useState('');
    const [roster, setRoster] = useState<Npcs | null>(null);
    const [mode, setMode] = useState<'program' | 'scenes'>('program');
    const [programKey, setProgramKey] = useState(0);
    const world = useWorld(target, setCatalogError);
    useEffect(() => {
        let active = true;
        dmApi.npcs(target).then(n => { if (active) setRoster(n); }).catch(e => { if (active) setCatalogError(String(e.message)); });
        return () => { active = false; };
    }, [target]);
    const editable = me.role !== 'viewer';
    return <main className="story-workspace">
        <header><div><h2>Story Creator</h2><p className="hint">{editable ? `Drafts are kept on the server for ${worldName(target)}; this browser keeps what is not saved yet.`
            : `Drafts and rehearsals save in this browser for ${worldName(target)}.`}</p></div></header>
        <nav aria-label="Story workspace"><button aria-pressed={mode === 'program'} onClick={() => setMode('program')}>World stories</button>
            <button aria-pressed={mode === 'scenes'} onClick={() => setMode('scenes')}>Scenes</button>
            <button onClick={openPersonal}>Personal storylines and storytellers</button></nav>
        {catalogError && <p className="hint" role="status">The world's map or roster is unavailable: pickers list less. {catalogError}</p>}
        {fileError && <p role="alert">{fileError}</p>}
        {mode === 'scenes' ? <SceneWorkspace libraryKey={key + '.scenes'} world={world} roster={roster} target={target} canDirect={editable}
            openProgram={p => { if (window.confirm('Open this release in World stories and replace the current world-story draft?')) {
                try { localStorage.setItem(key + '.world-program', JSON.stringify(p, null, 2)); localStorage.removeItem(key + '.world-program.draft'); setProgramKey(k => k + 1); setMode('program'); }
                catch { setFileError('Could not save the release draft. Browser storage is unavailable.'); }
            } }}/>
            : <StoryProgramPanel key={key + programKey} sceneLibraryKey={key + '.scenes'} world={world} canDirect={editable} target={target}
                storageKey={key + '.world-program'} openCampaigns={openCampaigns}/>}
    </main>;
}
