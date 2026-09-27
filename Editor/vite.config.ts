import {defineConfig} from 'vite';
import react from '@vitejs/plugin-react';

// `npm run dev` serves the UI with hot reload and forwards API calls to the
// Python host (python3 tools/map_editor.py serve). `npm run build` writes
// dist/, which the Python host serves directly.
export default defineConfig({
    plugins: [react()],
    base: './',
    // Two apps from one codebase: Atlas Workshop (index.html) and Dungeon Master (dm.html).
    build: {outDir: 'dist', emptyOutDir: true, chunkSizeWarningLimit: 1200,
        rollupOptions: {input: {atlas: 'index.html', dm: 'dm.html'}}},
    server: {
        port: 5173,
        proxy: {'/api': 'http://127.0.0.1:8765', '/portraits': 'http://127.0.0.1:8765'},
    },
});
