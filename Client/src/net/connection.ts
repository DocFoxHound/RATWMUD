// One WebSocket to the server, feeding a Session. In a browser, messages are inflated and parsed on a worker
// (decodeWorker.ts) so a large snapshot never stalls a frame; they are handed on strictly in the order they arrived (a
// snapshot never overtakes the event before it). Without workers (Node), the same decoding runs here, at once.
import {ackMessage, commandMessage, Kind, pingMessage, pongTime, poseMessage} from './wire.ts';
import {unpack} from './motion.ts';
import {decode, type Decoded} from './decodeWorker.ts';
import {Session, type Json, type SessionView} from './session.ts';

export interface ConnectionStats {
    opened: boolean;
    closed: boolean;
    messages: number;
    bytes: number;
}

/** What the latency overlay shows of the connection (Docs/Design/31-responsiveness.md, Phase 1). */
export interface NetSample {
    ping: number;           // The latest round trip (ms); 0 before the first.
    pingP50: number;        // The median of the last 30.
    bytesPerSecond: number; // Received, over the last few seconds.
}

const PingEvery = 2000;     // ms

/** The game's address on the server this page came from (ws: or wss: to match the page). */
export function gameUrl(location: {protocol: string; host: string}): string {
    return `${location.protocol === 'https:' ? 'wss:' : 'ws:'}//${location.host}/ws`;
}

export class Connection {
    readonly session: Session;
    readonly stats: ConnectionStats = {opened: false, closed: false, messages: 0, bytes: 0};
    private socket: WebSocket;
    private waiting: Uint8Array[] = [];
    private worker: Worker | null = null;
    private sent = 0;
    private handled = 0;
    private early = new Map<number, Decoded>();
    private idle: Array<() => void> = [];
    private pings: number[] = [];
    private pinger: ReturnType<typeof setInterval> | null = null;
    private byteMarks: Array<[number, number]> = [];       // (time, bytes received by then), one a ping apart.

    private onState: (open: boolean) => void;

    constructor(url: string, view: SessionView, onState: (open: boolean) => void = () => {}) {
        this.onState = onState;
        this.session = new Session({
            command: json => { const m = commandMessage(json); if (m) this.send(m); },
            ack: (revision, missing) => this.send(ackMessage(revision, missing)),
        }, view);
        this.worker = Connection.startWorker();
        if (this.worker) this.worker.onmessage = (e: MessageEvent<Decoded>) => this.decoded(e.data);
        this.socket = new WebSocket(url);
        this.socket.binaryType = 'arraybuffer';
        this.socket.addEventListener('open', () => {
            this.stats.opened = true;
            for (const m of this.waiting.splice(0)) this.socket.send(m as Uint8Array<ArrayBuffer>);
            this.onState(true);
            this.ping();
            this.pinger = setInterval(() => this.ping(), PingEvery);
            (this.pinger as unknown as {unref?: () => void}).unref?.();      // Never what keeps Node running.
        });
        this.socket.addEventListener('message', message => {
            if (!(message.data instanceof ArrayBuffer)) return;
            const data = message.data;
            this.stats.bytes += data.byteLength;
            // A Pong is the server's own answer, timed at once: it never joins the game's ordered messages.
            const pong = pongTime(data);
            if (pong !== null) {
                this.pings.push(performance.now() - pong);
                if (this.pings.length > 30) this.pings.shift();
                return;
            }
            ++this.stats.messages;
            this.arrive(data);
        });
        this.socket.addEventListener('close', () => {
            this.stopPinging();
            if (this.stats.closed) return;
            this.stats.closed = true;
            this.session.connectionLost();
            this.onState(false);
        });
    }

    get open(): boolean {
        return this.socket.readyState === WebSocket.OPEN;
    }

    submit(command: Json) {
        this.session.submit(command);
    }

    /** Where the page's own wolf is: a few bytes, twenty times a second (doc 31, Phase 4). */
    sendPose(seq: number, x: number, y: number, facing: number, ix: number, iy: number) {
        this.send(poseMessage(seq, x, y, facing, ix, iy));
    }

    /** The latency overlay's numbers. */
    netSample(): NetSample {
        const sorted = [...this.pings].sort((a, b) => a - b);
        const first = this.byteMarks[0], now = performance.now();
        const seconds = first ? (now - first[0]) / 1000 : 0;
        return {
            ping: this.pings.length ? this.pings[this.pings.length - 1] : 0,
            pingP50: sorted.length ? sorted[Math.floor(sorted.length / 2)] : 0,
            bytesPerSecond: seconds > 0 ? (this.stats.bytes - first[1]) / seconds : 0,
        };
    }

    close() {
        this.stopPinging();
        this.stats.closed = true;
        this.socket.close();
        this.worker?.terminate();
    }

    /** Everything received so far has been handled. */
    settled(): Promise<void> {
        if (this.handled === this.sent) return Promise.resolve();
        return new Promise(resolve => this.idle.push(resolve));
    }

    private ping() {
        this.send(pingMessage(performance.now()));
        this.byteMarks.push([performance.now(), this.stats.bytes]);
        if (this.byteMarks.length > 3) this.byteMarks.shift();
    }

    private stopPinging() {
        if (this.pinger !== null) clearInterval(this.pinger);
        this.pinger = null;
    }

    private static startWorker(): Worker | null {
        if (typeof Worker === 'undefined' || typeof document === 'undefined') return null;
        try {
            return new Worker(new URL('./decodeWorker.ts', import.meta.url), {type: 'module'});
        } catch {
            return null;
        }
    }

    private send(message: Uint8Array) {
        if (this.socket.readyState === WebSocket.CONNECTING) this.waiting.push(message);
        else if (this.socket.readyState === WebSocket.OPEN) this.socket.send(message as Uint8Array<ArrayBuffer>);
    }

    private arrive(data: ArrayBuffer) {
        const seq = this.sent++;
        if (this.worker) this.worker.postMessage({seq, data}, [data]);
        else this.decoded(decode(seq, data));
    }

    /** A decoded message: handed on once every message before it has been. */
    private decoded(message: Decoded) {
        this.early.set(message.seq, message);
        for (let next = this.early.get(this.handled); next; next = this.early.get(this.handled)) {
            this.early.delete(this.handled++);
            this.handle(next);
        }
        if (this.handled === this.sent) for (const resolve of this.idle.splice(0)) resolve();
    }

    private handle(message: Decoded) {
        if (message.kind === Kind.Motion) {
            const frame = unpack(message.raw);
            if (frame) this.session.receiveMotion(frame);
        } else if (message.kind === Kind.Snapshot) this.session.receiveSnapshot(message.value as Json, message.wireBytes);
        else if (message.kind === Kind.Event) this.session.receiveEvent(message.value as Json);
    }
}
