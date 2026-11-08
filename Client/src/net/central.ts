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
        '/health': ['GET'],
    };
    if (exact[path]) return exact[path].includes(method);
    return /^\/wolves\/wolf-[0-9a-f]{32}$/.test(path) && (method === 'PATCH' || method === 'DELETE');
}

const TokenFields = ['access_token', 'access_expires', 'refresh_token', 'refresh_expires'];

/** A browser's way to central: fetch, the tokens in memory. */
export class FetchCentral implements CentralTransport {
    readonly where: string;
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
    readonly development: boolean;
    readonly canRemember = true;
    private invoke: <T>(command: string, args?: Record<string, unknown>) => Promise<T>;

    constructor(info: {where: string; development: boolean}, invoke: <T>(command: string, args?: Record<string, unknown>) => Promise<T>) {
        this.where = info.where;
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

/** A world in central's list (doc 77, 3.1; a stub until Phase 4). */
export interface ListedWorld {
    id: string;
    name: string;
    kind: string;            // 'official' or 'community', set by central.
    address: string;         // "host:port"
    secure: boolean;         // wss://
    protocol: number | null;
    description: string;
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
        return (Array.isArray(reply.worlds) ? reply.worlds.filter(isJson) : []).map(w => ({
            id: String(w.id ?? ''), name: String(w.name ?? ''), kind: w.kind === 'official' ? 'official' : 'community',
            address: String(w.address ?? ''), secure: w.secure === true,
            protocol: typeof w.protocol === 'number' ? w.protocol : null, description: typeof w.description === 'string' ? w.description : '',
        })).filter(w => /^world-[0-9a-f]{32}$/.test(w.id) && w.address);
    }
    async ticket(world: string, wolf: string): Promise<string> {
        const reply = await this.ask('POST', '/tickets', {world, wolf});
        return String(reply.ticket ?? '');
    }
}
