// Central, from the page (Docs/Design/77-central-accounts-and-hosted-worlds.md, 3.1 and 4.2): signing in, the wolves,
// the creator's rules, the world list and tickets. JSON over HTTP(S).
//
// The page never holds a token that outlives it, and never stores one:
//   - in the desktop client the shell makes every call (ShellCentral): it keeps the access and refresh tokens itself,
//     the refresh token in the OS's keychain when "remember me" is ticked, and hands the page only the replies, with the
//     tokens taken out. A script slipped into the page by a hostile world can't read them.
//   - in a browser (development and the tests: `?central=URL`, central started with `--cors`), FetchCentral keeps the
//     two tokens in memory only, and "remember me" isn't offered.
// Both make the same calls and take out the same fields, so Central (below) can't tell them apart.
import type {Json} from '../game/json.ts';

export interface CentralReply {
    status: number;          // The HTTP status; 0 when central couldn't be reached.
    body: Json;              // {ok: true, ...} or {ok: false, error, code}.
}

export interface CentralTransport {
    /** Where central is, for the page to say ("127.0.0.1:7790"), and whether it is the built-in one. */
    readonly where: string;
    /** Central's address as a URL ("http://127.0.0.1:7790"), for the world's and the agent's commands on the Host page;
     *  '' when it isn't known. */
    readonly url: string;
    readonly development: boolean;
    /** Whether "remember me" is kept between launches. */
    readonly canRemember: boolean;
    call(method: string, path: string, body?: Json): Promise<CentralReply>;
    /** A remembered sign-in, refreshed: the account, or a 401 when there is none. */
    resume(): Promise<CentralReply>;
}

/** The calls the page may make, and nothing else (the shell has the same list). */
export function allowedCall(method: string, path: string): boolean {
    const exact: Record<string, string[]> = {
        '/account/register': ['POST'], '/account/login': ['POST'], '/account/logout': ['POST'], '/account': ['GET', 'DELETE'],
        '/wolves': ['GET', 'POST'], '/wolves/name': ['POST'], '/tickets': ['POST'], '/worlds': ['GET'], '/rules': ['GET'],
        '/health': ['GET'], '/host/worlds': ['GET', 'POST'],
    };
    if (exact[path]) return exact[path].includes(method);
    if (/^\/wolves\/wolf-[0-9a-f]{32}$/.test(path)) return method === 'PATCH' || method === 'DELETE';
    // The Host page (doc 77, 3.2): a new token for one of the account's worlds, or the world removed. The world's own
    // calls (/worlds/heartbeat, /worlds/leave) are the world agent's, never the client's.
    if (/^\/host\/worlds\/world-[0-9a-f]{32}\/token$/.test(path)) return method === 'POST';
    if (/^\/host\/worlds\/world-[0-9a-f]{32}$/.test(path)) return method === 'DELETE';
    return false;
}

const TokenFields = ['access_token', 'access_expires', 'refresh_token', 'refresh_expires'];

/** A browser's way to central: fetch, the tokens in memory. */
export class FetchCentral implements CentralTransport {
    readonly where: string;
    readonly url: string;
    readonly development = true;
    readonly canRemember = false;
    private access = '';
    private refresh = '';
    private base: string;
    private fetcher: typeof fetch;

    constructor(base: string, fetcher: typeof fetch = (...a) => fetch(...a)) {
        this.base = base.replace(/\/+$/, '');
        this.fetcher = fetcher;
        this.where = this.base.replace(/^https?:\/\//, '');
        this.url = this.base;
    }

    async call(method: string, path: string, body?: Json): Promise<CentralReply> {
        if (!allowedCall(method, path)) return {status: 400, body: {ok: false, error: 'Not a call the client makes.', code: 'bad_request'}};
        let reply = await this.send(method, path, body);
        // Refreshed only when the access token is the problem ('unauthenticated'): a wrong password (bad_sign_in) on a
        // deletion is not tried twice. Signing out is retried too, so an expired token still ends the session.
        if (reply.status === 401 && reply.body.code === 'unauthenticated' && this.refresh && path !== '/account/login' && path !== '/account/register') {
            if ((await this.renew()).status === 200) reply = await this.send(method, path, body);
        }
        if ((path === '/account/login' || path === '/account/register') && reply.status === 200) {
            this.access = String(reply.body.access_token ?? '');
            this.refresh = String(reply.body.refresh_token ?? '');
        }
        if (path === '/account/logout' || (path === '/account' && method === 'DELETE' && reply.status === 200)) this.access = this.refresh = '';
        return {status: reply.status, body: stripTokens(reply.body)};
    }

    async resume(): Promise<CentralReply> {
        if (!this.refresh) return {status: 401, body: {ok: false, error: 'Sign in.', code: 'unauthenticated'}};
        const reply = await this.renew();
        return {status: reply.status, body: stripTokens(reply.body)};
    }

    private async renew(): Promise<CentralReply> {
        const reply = await this.send('POST', '/account/refresh', {refresh_token: this.refresh}, false);
        if (reply.status === 200) {
            this.access = String(reply.body.access_token ?? '');
            this.refresh = String(reply.body.refresh_token ?? '');
        } else if (reply.status === 401 || reply.status === 403) this.access = this.refresh = '';
        return reply;
    }

    private async send(method: string, path: string, body?: Json, auth = true): Promise<CentralReply> {
        const headers: Record<string, string> = {};
        if (body !== undefined) headers['Content-Type'] = 'application/json';
        if (auth && this.access) headers.Authorization = `Bearer ${this.access}`;
        try {
            const response = await this.fetcher(this.base + path, {method, headers, body: body === undefined ? undefined : JSON.stringify(body),
                signal: AbortSignal.timeout(15000), credentials: 'omit', cache: 'no-store', redirect: 'error'});
            let parsed: unknown = null;
            try {
                parsed = await response.json();
            } catch {
                parsed = null;
            }
            return {status: response.status, body: isJson(parsed) ? parsed : {ok: false, error: 'Central sent something unreadable.', code: 'bad_reply'}};
        } catch {
            return {status: 0, body: {ok: false, error: 'Central could not be reached.', code: 'unreachable'}};
        }
    }
}

/** The desktop client's way to central: the shell makes the call (Desktop/src-tauri/src/central.rs). */
export class ShellCentral implements CentralTransport {
    readonly where: string;
    readonly url: string;
    readonly development: boolean;
    readonly canRemember = true;
    private invoke: <T>(command: string, args?: Record<string, unknown>) => Promise<T>;

    /** `info` is the shell's central_info: `url` is the base it was started with, scheme and all (never guessed here:
     *  an https:// central can be on this computer too). */
    constructor(info: {where: string; url?: unknown; development: boolean}, invoke: <T>(command: string, args?: Record<string, unknown>) => Promise<T>) {
        this.where = info.where;
        this.url = baseUrl(info.url);
        this.development = info.development;
        this.invoke = invoke;
    }

    async call(method: string, path: string, body?: Json): Promise<CentralReply> {
        if (!allowedCall(method, path)) return {status: 400, body: {ok: false, error: 'Not a call the client makes.', code: 'bad_request'}};
        return this.reply(this.invoke<CentralReply>('central_call', {method, path, body: body ?? null}));
    }

    resume(): Promise<CentralReply> {
        return this.reply(this.invoke<CentralReply>('central_resume'));
    }

    private async reply(made: Promise<CentralReply>): Promise<CentralReply> {
        try {
            const r = await made;
            return {status: typeof r?.status === 'number' ? r.status : 0, body: isJson(r?.body) ? stripTokens(r.body) : {ok: false, error: 'No reply.', code: 'bad_reply'}};
        } catch (error) {
            return {status: 0, body: {ok: false, error: String(error), code: 'unreachable'}};
        }
    }
}

/** Central's base URL as the shell gives it (central.rs, parse_central: a scheme, a host and a port, nothing else), or ''
 *  when it isn't one: it goes into commands a host copies, so nothing but that shape is taken. */
export function baseUrl(given: unknown): string {
    return typeof given === 'string' && /^https?:\/\/(\[[0-9a-f:.]+\]|[a-z0-9.-]+)(:\d{1,5})?$/i.test(given) ? given : '';
}

function isJson(v: unknown): v is Json {
    return typeof v === 'object' && v !== null && !Array.isArray(v);
}

function stripTokens(body: Json): Json {
    const out = {...body};
    for (const f of TokenFields) delete out[f];
    return out;
}

/** A refusal from central, in its own words. */
export class CentralError extends Error {
    readonly status: number;
    readonly code: string;
    constructor(status: number, code: string, message: string) {
        super(message);
        this.status = status;
        this.code = code;
    }
}

/** A world in central's list (doc 77, 3.1 and 7): what its heartbeat says of it, the kind and the count central's. */
export interface ListedWorld {
    id: string;
    name: string;
    kind: string;            // 'official' or 'community', set by central.
    address: string;         // Where to connect: "ip:port" central called back (ws://), or "host:port" (wss://).
    host: string;            // A host name to show in its place, or '' (doc 77, 7: shown, never connected to for ws://).
    secure: boolean;         // wss://
    protocol: number | null;
    description: string;
    players: number;         // The honest count (wolves central gave tickets for), until the world's own /status answers.
    limit: number;           // 0 when the world doesn't say.
    region: string;
    tags: string[];
    voices: boolean;
    cheats: boolean;
}

/** One of the account's own worlds, on the Host page (doc 77, 3.2 and 7). */
export interface HostWorld {
    id: string;
    name: string;
    kind: string;
    listed: boolean;
    reachable: boolean;
    why: string;             // Why it isn't listed ('' when it is), in central's words.
    address: string;         // "ip:port" as central last saw it, or ''.
    players: number;
    lastHeartbeat: number | null;    // Unix seconds.
    tokenCreated: number | null;
}

/** "just now", "4 min ago", "3 h ago", or the day: how long since a unix time. */
export function agoText(at: number | null, now = Date.now() / 1000): string {
    if (at === null) return 'never';
    const s = Math.max(0, now - at);
    if (s < 45) return 'just now';
    if (s < 3600) return `${Math.max(1, Math.round(s / 60))} min ago`;
    if (s < 48 * 3600) return `${Math.round(s / 3600)} h ago`;
    return new Date(at * 1000).toISOString().slice(0, 10);
}

/** A word for a POSIX shell: as it is when it is plain, in single quotes otherwise (a world's name can hold anything). */
export function shellWord(word: string): string {
    return /^[A-Za-z0-9_@%+=:,./-]+$/.test(word) ? word : `'${word.replace(/'/g, `'\\''`)}'`;
}

/** A TOML basic string (world.toml's values: a world id and central's address, both checked, but quoted all the same). */
function tomlString(value: string): string {
    return `"${value.replace(/[\\"]/g, c => `\\${c}`).replace(/[\u0000-\u001f\u007f]/g, '')}"`;
}

/** The package's .env line for a world's host token (Package/README.md, "Start"; the NEW TOKEN dialog says the same). */
export function envLine(token: string): string {
    return `RATW_HOST_TOKEN=${token}`;
}

/** The lines world.toml needs for a world on central: its id under [world], central's address under [central]. */
export function worldTomlLines(world: string, central: string): string {
    return `[world]\nid = ${tomlString(world)}\n\n[central]\nurl = ${tomlString(central)}`;
}

const WorldId = /^world-[0-9a-f]{32}$/;
const text = (v: unknown, most: number) => (typeof v === 'string' ? v.slice(0, most) : '');
const count = (v: unknown) => (typeof v === 'number' && Number.isFinite(v) ? Math.max(0, Math.trunc(v)) : 0);
const HostName = /^(?=.{1,253}$)[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?(\.[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?)+$/i;
const when = (v: unknown) => (typeof v === 'number' && Number.isFinite(v) && v > 0 ? v : null);

/** A world from central's public list, or null when it isn't one. */
export function readListedWorld(w: Json): ListedWorld | null {
    const id = String(w.id ?? '');
    const address = text(w.address, 260);
    if (!WorldId.test(id) || !address) return null;
    const host = typeof w.host === 'string' && HostName.test(w.host) ? w.host.toLowerCase() : '';
    return {id, name: text(w.name, 48), kind: w.kind === 'official' ? 'official' : 'community', address, host, secure: w.secure === true,
        protocol: typeof w.protocol === 'number' ? w.protocol : null, description: text(w.description, 300),
        players: count(w.players), limit: count(w.limit), region: text(w.region, 32),
        tags: (Array.isArray(w.tags) ? w.tags : []).filter((t): t is string => typeof t === 'string').slice(0, 5).map(t => t.slice(0, 16)),
        voices: w.voices === true, cheats: w.cheats === true};
}

/** One of the account's worlds from GET /host/worlds, or null when it isn't one. */
export function readHostWorld(w: unknown): HostWorld | null {
    if (!isJson(w) || typeof w.id !== 'string' || !WorldId.test(w.id)) return null;
    return {id: w.id, name: text(w.name, 48), kind: w.kind === 'official' ? 'official' : 'community', listed: w.listed === true,
        reachable: w.reachable === true, why: text(w.why, 400), address: text(w.address, 260), players: count(w.players),
        lastHeartbeat: when(w.last_heartbeat), tokenCreated: when(w.token_created)};
}

/** The calls, with central's refusals turned into errors carrying its own message. */
export class Central {
    readonly transport: CentralTransport;
    constructor(transport: CentralTransport) {
        this.transport = transport;
    }

    private async ask(method: string, path: string, body?: Json): Promise<Json> {
        const {status, body: reply} = await this.transport.call(method, path, body);
        if (status === 200 && reply.ok === true) return reply;
        const message = typeof reply.error === 'string' && reply.error ? reply.error
            : status === 0 ? `Central (${this.transport.where}) could not be reached. Check your connection and try again.`
            : 'Central could not do that just now.';
        throw new CentralError(status, typeof reply.code === 'string' ? reply.code : '', message);
    }

    register(username: string, password: string, birthYear: number, birthMonth: number | null, remember: boolean) {
        const body: Json = {username, password, birth_year: birthYear, remember};
        if (birthMonth !== null) body.birth_month = birthMonth;
        return this.ask('POST', '/account/register', body);
    }
    login(username: string, password: string, remember: boolean) {
        return this.ask('POST', '/account/login', {username, password, remember});
    }
    async resume(): Promise<Json | null> {
        const {status, body} = await this.transport.resume();
        if (status === 200 && body.ok === true) return body;
        if (status === 0) throw new CentralError(0, 'unreachable', `Central (${this.transport.where}) could not be reached.`);
        return null;
    }
    logout() {
        return this.ask('POST', '/account/logout', {});
    }
    deleteAccount(password: string) {
        return this.ask('DELETE', '/account', {password});
    }
    async wolves(): Promise<Json[]> {
        const reply = await this.ask('GET', '/wolves');
        return Array.isArray(reply.wolves) ? reply.wolves.filter(isJson) : [];
    }
    async createWolf(wolf: Json): Promise<Json> {
        const reply = await this.ask('POST', '/wolves', wolf);
        return isJson(reply.wolf) ? reply.wolf : {};
    }
    deleteWolf(id: string, password: string) {
        return this.ask('DELETE', `/wolves/${id}`, {password});
    }
    rules() {
        return this.ask('GET', '/rules');
    }
    async worlds(): Promise<ListedWorld[]> {
        const reply = await this.ask('GET', '/worlds');
        return (Array.isArray(reply.worlds) ? reply.worlds.filter(isJson) : []).map(readListedWorld).filter((w): w is ListedWorld => w !== null);
    }
    async ticket(world: string, wolf: string): Promise<string> {
        const reply = await this.ask('POST', '/tickets', {world, wolf});
        return String(reply.ticket ?? '');
    }

    // The Host page (doc 77, 3.2): the account's worlds, a new one, a new token, one removed. Only for an account
    // central has made a host ('not_host' otherwise).
    async hostWorlds(): Promise<{worlds: HostWorld[]; most: number}> {
        const reply = await this.ask('GET', '/host/worlds');
        const worlds = (Array.isArray(reply.worlds) ? reply.worlds : []).map(readHostWorld).filter((w): w is HostWorld => w !== null);
        return {worlds, most: count(reply.most) || 3};
    }
    async makeWorld(name: string): Promise<{world: HostWorld | null; token: string}> {
        const reply = await this.ask('POST', '/host/worlds', {name});
        return {world: readHostWorld(reply.world), token: text(reply.token, 200)};
    }
    async newToken(world: string): Promise<string> {
        if (!WorldId.test(world)) throw new CentralError(404, 'not_found', 'That is not one of your worlds.');
        const reply = await this.ask('POST', `/host/worlds/${world}/token`, {});
        return text(reply.token, 200);
    }
    removeWorld(world: string) {
        if (!WorldId.test(world)) return Promise.reject(new CentralError(404, 'not_found', 'That is not one of your worlds.'));
        return this.ask('DELETE', `/host/worlds/${world}`, {});
    }
}
