// The game screen on one connection, for the browser page (main.ts) and the desktop client (client.ts) alike: the view,
// the page's own walking, its poses, and the half-minute line report, which stops with the screen (doc 76, 9: it used
// to be started on every entry and never stopped).
import type {Connection} from './net/connection.ts';
import {GameView} from './game/view.ts';
import {Walker} from './game/walker.ts';

const NetReportEvery = 30000;      // ms: the page's pings told to the server.

// The page's own walking (doc 31, Phase 3): the server's movement rules as WebAssembly. Until it has loaded (or where it
// can't run) the server walks the wolf, as before.
let walker: Walker | null = null;
const games = new Set<GameView>();
let loading: Promise<void> | null = null;

export function loadWalker() {
    loading ??= Walker.load(new URL('./wasm/walk.wasm', import.meta.url).href).then(w => {
        walker = w;
        if (w) for (const g of games) g.state.walker = w;
    });
    return loading;
}

export class Play {
    readonly game: GameView;
    private report: ReturnType<typeof setInterval>;

    constructor(app: HTMLElement, connection: Connection) {
        void loadWalker();
        const game = new GameView(app, command => connection.submit(command));
        this.game = game;
        games.add(game);
        game.netSample = () => connection.netSample();
        game.state.walker = walker;
        game.state.poseSender = (seq, x, y, facing, ix, iy) => connection.sendPose(seq, x, y, facing, ix, iy);
        // The line to the server, every half minute, for its health tracker (Docs/Design/31-responsiveness.md).
        this.report = setInterval(() => {
            const net = connection.netSample();
            if (net.pingP50 > 0)
                connection.submit({type: 'net', p50: Math.round(net.pingP50), p95: Math.round(net.pingP95), max: Math.round(net.pingMax),
                    corrections: game.state.corrections});
        }, NetReportEvery);
    }

    destroy() {
        clearInterval(this.report);
        games.delete(this.game);
        this.game.destroy();
    }
}
