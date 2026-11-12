import {StrictMode, useEffect, useState} from 'react';
import {createRoot} from 'react-dom/client';
import {Studio} from './Studio';
import {onReload} from '../lib/hosted';
import '../styles.css';
import '../dm/dm.css';
import './studio.css';

/** The studio, started again inside the page by reloadApp() (hosted mode, doc 78) instead of a page load. */
function Root() {
    const [round, setRound] = useState(0);
    useEffect(() => onReload(() => setRound(r => r + 1)), []);
    return <Studio key={round} />;
}

createRoot(document.getElementById('root')!).render(<StrictMode><Root /></StrictMode>);
