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

/** What the page shows for a world: its listed host name with the port (doc 77, 7: shown only; a ws:// world is reached
 *  at the address central called back), or the address itself. */
export function shownAddress(a: WorldAddress, host: string): string {
    return host ? `${host}:${a.port}` : addressText(a);
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
    quickenedWolves: boolean;    // Doc 49, 9.1: false when the host keeps Quickened wolves out.
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
        cheats: b.cheats === true, private: b.private === true, quickenedWolves: b.quickenedWolves !== false, ms: Math.max(0, Math.round(ms))};
}

/** What the world list says of a world's status: until the world's own /status answers, central's count. "AI used"
 *  covers the residents' voices and the host's own tools, which use the same key (doc 78, 9); "cheats on" as the
 *  listing says. Either from central's listing or the world's /status. */
export function statusLine(s: WorldStatus | null | undefined, listed?: {players: number; limit: number; voices?: boolean; cheats?: boolean} | null): string {
    const marks = (ai: boolean, cheats: boolean) => `${ai ? ' · AI used' : ''}${cheats ? ' · cheats on' : ''}`;
    if (s === undefined) return listed ? `${listed.players}${listed.limit ? ` of ${listed.limit}` : ''} wolves · asking…${marks(listed.voices === true, listed.cheats === true)}` : 'Asking…';
    if (s === null) return 'Not answering';
    return `${s.players} of ${s.limit} wolves · ${s.ms} ms${marks(s.voices || listed?.voices === true, s.cheats || listed?.cheats === true)}`;
}

/** What a community world's players are told, on the world list and the first-arrival page (doc 78, section 9): the
 *  host owns the database and the connection, and no filter changes that. */
export const HOST_SEES = 'A community world is run by its host. The host can read everything you say and do there, and sees your ' +
    'internet address; co-builders they add see what the host\'s tools show. Your wolf\'s name is the same on every world.';

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

/** What central's listing adds to a world's columns (net/central.ts, ListedWorld). */
export interface ColumnsListing {
    players: number;
    limit: number;
    voices: boolean;
    cheats: boolean;
    quickenedWolves: boolean;
}

/** The server browser's columns for a world: wolves in it and its limit, the round trip, whether it answered (its own
 *  /status, else central's count), and its marks ("AI used", "cheats on", "No Quickened"). */
export function serverColumns(s: WorldStatus | null | undefined, listed: ColumnsListing | null) {
    // Quickened wolves kept out (doc 49, 9.1) by central's listing or the world's own status: "No Quickened".
    const quickenedWolves = s?.quickenedWolves !== false && listed?.quickenedWolves !== false;
    const marks = [...((s?.voices || listed?.voices) ? ['AI used'] : []), ...((s?.cheats || listed?.cheats) ? ['cheats on'] : []),
        ...(quickenedWolves ? [] : ['No Quickened'])];
    if (s) return {players: s.players, limit: s.limit || null, ping: s.ms, answer: 'yes', marks, quickenedWolves};
    return {players: listed ? listed.players : null, limit: listed?.limit || null, ping: null, answer: s === null ? 'no' : 'asking', marks,
        quickenedWolves};
}
