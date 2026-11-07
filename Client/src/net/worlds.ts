// Worlds as the client sees them (Docs/Design/77-central-accounts-and-hosted-worlds.md, 3.1, 5.1 and 7): an address,
// the socket and status URLs made from it, a world's GET /status, and whether this client can play there (its protocol
// and client-data hash against the page's own).
import type {Json} from '../game/json.ts';

export const DefaultPort = 7788;

export interface WorldAddress {
    host: string;            // A name, an IPv4 address, or an IPv6 address without brackets.
    port: number;
    secure: boolean;         // wss:// and https://
}

/** An address as a player types it, or central lists it: "host:port", "host" (port 7788), "[::1]:7788",
 *  "ws://host:port/ws", "wss://host". Null for anything else. */
export function parseAddress(given: string, secure = false): WorldAddress | null {
    let s = given.trim();
    const scheme = /^(wss?|https?):\/\//i.exec(s);
    if (scheme) {
        secure = /^(wss|https)$/i.test(scheme[1]);
        s = s.slice(scheme[0].length);
    }
    s = s.replace(/\/(ws|status)?\/?$/, '');
    const v6 = /^\[([0-9a-fA-F:.]+)\](?::(\d{1,5}))?$/.exec(s);
    const plain = /^([A-Za-z0-9.-]+)(?::(\d{1,5}))?$/.exec(s);
    const host = v6 ? v6[1] : plain ? plain[1] : '';
    const port = Number((v6 ? v6[2] : plain?.[2]) ?? (scheme && secure ? 443 : DefaultPort));
    if (!host || host.startsWith('.') || host.endsWith('-') || !Number.isInteger(port) || port < 1 || port > 65535) return null;
    return {host: host.toLowerCase(), port, secure};
}

function hostPart(a: WorldAddress): string {
    return a.host.includes(':') ? `[${a.host}]` : a.host;
}

export function addressText(a: WorldAddress): string {
    return `${hostPart(a)}:${a.port}`;
}

export function socketUrl(a: WorldAddress): string {
    return `${a.secure ? 'wss' : 'ws'}://${hostPart(a)}:${a.port}/ws`;
}

export function statusUrl(a: WorldAddress): string {
    return `${a.secure ? 'https' : 'http'}://${hostPart(a)}:${a.port}/status`;
}

/** This computer: plain ws:// to it crosses no network. */
export function isLoopback(host: string): boolean {
    return host === 'localhost' || host === '::1' || /^127\.\d{1,3}\.\d{1,3}\.\d{1,3}$/.test(host);
}

/** A world's GET /status (doc 77, 5.1). */
export interface WorldStatus {
    id: string;
    name: string;
    kind: string;
    protocol: number;
    clientData: string;
    players: number;
    limit: number;
    voices: boolean;
    cheats: boolean;
    private: boolean;
    ms: number;              // The round trip the client measured.
}

export function readStatus(body: unknown, ms: number): WorldStatus | null {
    if (typeof body !== 'object' || body === null || Array.isArray(body)) return null;
    const b = body as Json;
    if (b.ratw !== 'world' || typeof b.id !== 'string' || !/^world-[0-9a-f]{32}$/.test(b.id) || typeof b.protocol !== 'number') return null;
    const text = (v: unknown, most: number) => (typeof v === 'string' ? v.slice(0, most) : '');
    const count = (v: unknown) => (typeof v === 'number' && Number.isFinite(v) ? Math.max(0, Math.trunc(v)) : 0);
    return {id: b.id, name: text(b.name, 48), kind: b.kind === 'official' ? 'official' : 'community', protocol: b.protocol,
        clientData: text(b.clientData, 64), players: count(b.players), limit: count(b.limit), voices: b.voices === true,
        cheats: b.cheats === true, private: b.private === true, ms: Math.max(0, Math.round(ms))};
}

export type Fit = 'ok' | 'update' | 'older' | 'data' | 'unknown';

/** Whether this client can play on a world: its protocol (from /status, else the list) and its client-data hash
 *  (from /status) against the page's own. */
export function compatibility(build: {protocol: number; clientData: string}, status: WorldStatus | null,
    listedProtocol: number | null): {fit: Fit; note: string} {
    const protocol = status ? status.protocol : listedProtocol;
    if (protocol !== null && protocol > build.protocol) return {fit: 'update', note: 'Needs an update: this world is newer than your client.'};
    if (protocol !== null && protocol < build.protocol) return {fit: 'older', note: 'This world is older than your client: its host needs to update it.'};
    if (status && build.clientData && status.clientData && status.clientData !== build.clientData)
        return {fit: 'data', note: 'Needs an update: this world\'s map rules differ from your client\'s.'};
    if (protocol === null) return {fit: 'unknown', note: ''};
    return {fit: 'ok', note: ''};
}

/** A world's status, asked by the shell in the client (no CORS or mixed-content rules there, 2 s, 2 KB), else by the
 *  page's own fetch (GET /status allows any origin). Null when it can't be reached. */
export async function fetchStatus(a: WorldAddress, invoke: (<T>(command: string, args?: Record<string, unknown>) => Promise<T>) | null): Promise<WorldStatus | null> {
    const started = performance.now();
    try {
        if (invoke) {
            const r = await invoke<{status: number; body: unknown; ms: number}>('world_status', {host: a.host, port: a.port, secure: a.secure});
            return r && r.status === 200 ? readStatus(r.body, r.ms) : null;
        }
        const response = await fetch(statusUrl(a), {signal: AbortSignal.timeout(2000), credentials: 'omit', cache: 'no-store', redirect: 'error'});
        if (response.status !== 200) return null;
        const text = await response.text();
        if (text.length > 2048) return null;
        return readStatus(JSON.parse(text), performance.now() - started);
    } catch {
        return null;
    }
}
