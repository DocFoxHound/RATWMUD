import {StrictMode} from 'react';
import {createRoot} from 'react-dom/client';
import {DmApp} from './DmApp';
import '../styles.css';
import './dm.css';

createRoot(document.getElementById('root')!).render(<StrictMode><DmApp /></StrictMode>);
