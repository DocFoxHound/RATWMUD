// One WebSocket to the server, feeding a Session: each message is handled as it arrives, so they stay in order (a
// snapshot never overtakes the event before it). Works on the browser's WebSocket and on Node's.
import {ackMessage, commandMessage, decodeMessage, Kind} from './wire.ts';
import {unpack} from './motion.ts';
import {Session, type Json, type SessionView} from './session.ts';

export interface ConnectionStats {
    opened: boolean;
    closed: boolean;
    messages: number;
    bytes: number;
}

const decoder = new TextDecoder();

/** The game's address on the server this page came from (ws: or wss: to match the page). */
export function gameUrl(location: {protocol: string; host: string}): string {
    return `${location.protocol === 'https:' ? 'wss:' : 'ws:'}//${location.host}/ws`;
}

export class Connection {
    readonly session: Session;
    readonly stats: ConnectionStats = {opened: false, closed: false, messages: 0, bytes: 0};
    private socket: WebSocket;
    private waiting: Uint8Array[] = [];

    private onState: (open: boolean) => void;

    constructor(url: string, view: SessionView, onState: (open: boolean) => void = () => {}) {
        this.onState = onState;
        this.session = new Session({
            command: json => { const m = commandMessage(json); if (m) this.send(m); },
            ack: (revision, missing) => this.send(ackMessage(revision, missing)),
        }, view);
        this.socket = new WebSocket(url);
        this.socket.binaryType = 'arraybuffer';
        this.socket.addEventListener('open', () => {
            this.stats.opened = true;
            for (const m of this.waiting.splice(0)) this.socket.send(m as Uint8Array<ArrayBuffer>);
            this.onState(true);
        });
        this.socket.addEventListener('message', message => {
            if (!(message.data instanceof ArrayBuffer)) return;
            const data = message.data;
            ++this.stats.messages;
            this.stats.bytes += data.byteLength;
            this.arrive(data);
        });
        this.socket.addEventListener('close', () => {
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

    close() {
        this.stats.closed = true;
        this.socket.close();
    }

    /** Everything received so far has been handled (it always has: kept for callers that wait). */
    settled(): Promise<void> {
        return Promise.resolve();
    }

    private send(message: Uint8Array) {
        if (this.socket.readyState === WebSocket.CONNECTING) this.waiting.push(message);
        else if (this.socket.readyState === WebSocket.OPEN) this.socket.send(message as Uint8Array<ArrayBuffer>);
    }

    private arrive(data: ArrayBuffer) {
        const arrival = decodeMessage(data);
        if (!arrival) return;
        if (arrival.kind === Kind.Motion) {
            const frame = unpack(arrival.raw);
            if (frame) this.session.receiveMotion(frame);
            return;
        }
        let value: unknown;
        try {
            value = JSON.parse(decoder.decode(arrival.raw));
        } catch {
            return;
        }
        if (typeof value !== 'object' || value === null || Array.isArray(value)) return;
        if (arrival.kind === Kind.Snapshot) this.session.receiveSnapshot(value as Json, arrival.wireBytes);
        else this.session.receiveEvent(value as Json);
    }
}
