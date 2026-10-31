import {defineConfig} from 'vite';
import react from '@vitejs/plugin-react';

// `npm run dev` serves the UI with hot reload and forwards API calls to the
// studio (python3 tools/studio.py serve). `npm run build` writes dist/, which
// the studio serves directly.
export default defineConfig({
    plugins: [react()],
    base: './',
    // One app (doc 66, section 3): the studio, with its Atlas and DM suites.
    build: {outDir: 'dist', emptyOutDir: true, chunkSizeWarningLimit: 1200,
        rollupOptions: {input: {studio: 'index.html'}}},
    server: {
        port: 5173,
        proxy: {'/api': 'http://127.0.0.1:8765', '/portraits': 'http://127.0.0.1:8765'},
    },
});
