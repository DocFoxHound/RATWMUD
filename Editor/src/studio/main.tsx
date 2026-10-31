import {StrictMode} from 'react';
import {createRoot} from 'react-dom/client';
import {Studio} from './Studio';
import '../styles.css';
import '../dm/dm.css';
import './studio.css';

createRoot(document.getElementById('root')!).render(<StrictMode><Studio /></StrictMode>);
