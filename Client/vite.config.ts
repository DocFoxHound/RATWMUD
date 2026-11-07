import {defineConfig, type Plugin} from 'vite';
import {copyFileSync, mkdirSync, readFileSync} from 'node:fs';
import {join, resolve} from 'node:path';
import {clientDataHash, protocol} from './build-info.mjs';

const data = resolve(import.meta.dirname, '../Data');

// The fonts live in Data/ (the server and tools read them too); the build copies them in. (The portrait sheets are no
// longer copied: the page draws its wolves in code, ui/wolfArt.ts, and never loaded them.)
function gameData(): Plugin {
    const files = () => [
        ...['DejaVuSansMono.ttf', 'DejaVuSansMono-LICENSE.txt', 'Roboto-Regular.ttf', 'Roboto-Medium.ttf', 'Roboto-LICENSE.txt']
            .map(f => [join(data, 'Fonts', f), `fonts/${f}`]),
    ];
    let out = '';
    return {
        name: 'ratw-game-data',
        configResolved(config) { out = resolve(config.root, config.build.outDir); },
        // In development, the same files are served from Data/.
        configureServer(server) {
            server.middlewares.use((req, res, next) => {
                const found = files().find(([, url]) => req.url === `/${url}`);
                if (!found) return next();
                res.setHeader('Content-Type', found[1].endsWith('.png') ? 'image/png' : found[1].endsWith('.ttf') ? 'font/ttf' : 'text/plain');
                res.end(readFileSync(found[0]));
            });
        },
        closeBundle() {
            for (const [from, to] of files()) {
                mkdirSync(join(out, to, '..'), {recursive: true});
                copyFileSync(from, join(out, to));
            }
        },
    };
}

// `npm run dev` serves the client with hot reload and forwards the game's WebSocket to a running server
// (bash tools/server.sh); `npm run build` writes dist/, which ratw_server serves itself (--web).
export default defineConfig({
    plugins: [gameData()],
    base: './',
    // The protocol and client-data hash this page speaks (doc 77, 5.1: net/build.ts).
    define: {__RATW_BUILD__: JSON.stringify({protocol: protocol(), clientData: clientDataHash()})},
    build: {outDir: 'dist', emptyOutDir: true},
    server: {port: 5174, proxy: {'/ws': {target: 'ws://127.0.0.1:7788', ws: true}}},
});
