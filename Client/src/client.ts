// The desktop client's flow (Docs/Design/77-central-accounts-and-hosted-worlds.md, 3.1; doc 76 as amended): sign in to
// central, the wolves, the creator of a wolf's identity, the world list, a ticket, the world's first-arrival screen or
// straight in, and back to the list on leaving. A host's account also has the Host page (3.2): its own worlds, their
// tokens, and why central does or doesn't list them. The page is bundled in the client: it reaches a world at that
// world's own address with the page's own WebSocket (doc 77 Phase 0, D1), never through the page's host.
//
// The front door (ui/frontDoor.ts, central mode) sends this its commands and is answered with lobby events of the shape
// a world's lobby has. Nothing here is kept between launches but what the shell keeps ("remember me").
import {Central, CentralError, type HostWorld, type ListedWorld} from './net/central.ts';
import {Connection} from './net/connection.ts';
import {ClientBuild} from './net/build.ts';
import {addressText, compatibility, fetchStatus, isLoopback, parseAddress, shownAddress, socketUrl, statusLine, type WorldAddress,
    type WorldStatus} from './net/worlds.ts';
import type {SessionView} from './net/session.ts';
import {FrontDoor, type DoorWorld} from './ui/frontDoor.ts';
import {artCache} from './ui/artwork.ts';
import {ask} from './ui/dialog.ts';
import {Play} from './play.ts';
import type {Shell} from './shell.ts';
import {isObject, num, str, type Json} from './game/json.ts';

/** A world the client is joining or has joined. */
interface Target {
    id: string;
    name: string;
    kind: string;
    address: WorldAddress;   // Where the client connects (and pings).
    host: string;            // The host name central lists it under, shown in its place, or ''.
    listedProtocol: number | null;
    description: string;
}

export class ClientApp {
    readonly central: Central;
    readonly shell: Shell;
    door: FrontDoor | null = null;
    play: Play | null = null;
    connection: Connection | null = null;
    account: Json | null = null;
    wolves: Json[] = [];
    gifts: Json | null = null;
    listed: ListedWorld[] = [];
    hostWorlds: HostWorld[] = [];                    // The Host page: the account's own worlds, and how many it may have.
    hostMost = 3;
    readonly statuses = new Map<string, WorldStatus | null>();
    wolf = '';                                       // The wolf chosen for the world list.
    target: Target | null = null;                    // The world being joined or played.
    lost: Target | null = null;                      // A world whose connection was lost: Reconnect.
    private direct: Target[] = [];                   // Worlds joined by their address this session.
    private ticket = '';
    private keyTries = 0;
    private answered = false;                        // The world has said something on this connection.
    private admitted = false;                        // ...and has taken the ticket (the arrival, or in the world).
    private asking = new Set<string>();
    private app: HTMLElement;
    private footnote: string;
    private relay: string | null = null;             // The shell's loopback relay (only with --relay, doc 77 D1).

    constructor(app: HTMLElement, central: Central, shell: Shell) {
        this.app = app;
        this.central = central;
        this.shell = shell;
        const where = central.transport.where;
        this.footnote = `CENTRAL ${where.toUpperCase()}${central.transport.development ? ' (DEVELOPMENT)' : ''} · A world sees your wolf, never your password.`;
        shell.askToClose = () => ask('Leave the world? Your wolf leaves it with you.', {ok: 'Leave', cancel: 'Stay', now: true});
        artCache.send = command => this.connection?.submit(command);
    }

    /** At launch: a remembered sign-in, or the sign-in page. */
    async start() {
        const door = this.newDoor();
        door.setMessage('Contacting central…');
        if (this.shell.native) this.relay = await this.shell.invoke<string | null>('relay_address').catch(() => null);
        try {
            const resumed = await this.central.resume();
            if (resumed) {
                this.account = isObject(resumed.account) ? resumed.account : null;
                await this.signedIn(`Welcome back, ${str(this.account, 'username')}.`);
                return;
            }
            this.lobby('login', true, '');
        } catch (error) {
            this.lobby('login', false, message(error));
        }
    }

    // ------------------------------------------------------------------ The door

    private newDoor(): FrontDoor {
        this.door?.destroy();
        this.door = new FrontDoor(this.app, command => void this.command(command), {
            mode: 'central', canRemember: this.central.transport.canRemember, footnote: this.footnote,
            edition: this.shell.native ? 'DESKTOP CLIENT  /  EARLY DEVELOPMENT' : 'BROWSER, DEVELOPMENT  /  CENTRAL ACCOUNTS',
        });
        return this.door;
    }

    private lobby(stage: 'login' | 'characters' | 'worlds', ok: boolean, text: string) {
        const door = this.door ?? this.newDoor();
        if (stage === 'login') {
            door.receive({type: 'lobby', stage: 'login', ok, message: text});
            return;
        }
        door.receive({type: 'lobby', stage: 'characters', ok: stage === 'characters' ? ok : true, message: stage === 'characters' ? text : '',
            characters: this.wolves.map(w => ({id: str(w, 'id'), name: str(w, 'name'), age: num(w, 'age', 18), appearance: w.appearance,
                family: str(w, 'family')})),
            host: this.account?.host === true,
            ...(this.gifts ? {gifts: this.gifts} : {})});
        if (this.wolf && this.wolves.some(w => str(w, 'id') === this.wolf)) door.selectedId = this.wolf;
        if (stage === 'worlds') {
            door.receive({type: 'lobby', stage: 'worlds', ok, message: text, wolf: this.wolf, lost: this.lost?.id ?? '', worlds: this.doorWorlds()});
            for (const w of this.listed) if (!this.statuses.has(w.id) && !this.asking.has(w.id)) void this.askStatus(w);
        } else if (door.page === 'roster') door.show('roster');
    }

    /** The Host page (doc 77, 3.2), with a token to show once when one was just made. */
    private hostLobby(ok: boolean, text: string, token = '', tokenWorld = '') {
        const door = this.door ?? this.newDoor();
        door.receive({type: 'lobby', stage: 'host', ok, message: text, most: this.hostMost, central: this.central.transport.url,
            worlds: this.hostWorlds.map(w => ({...w})), ...(token ? {token, tokenWorld} : {})});
    }

    private async loadHostWorlds() {
        const {worlds, most} = await this.central.hostWorlds();
        this.hostWorlds = worlds;
        this.hostMost = most;
    }

    /** The list again after a token was made: if central can't be asked just now, the token is shown all the same
     *  (it can't be shown later), beside the world it was made for. */
    private async reloadHostWorlds(made: HostWorld | null) {
        try {
            await this.loadHostWorlds();
        } catch {
            if (made && !this.hostWorlds.some(w => w.id === made.id)) this.hostWorlds = [...this.hostWorlds, made];
        }
    }

    private doorWorlds(): DoorWorld[] {
        const seen = new Set<string>();
        const out: DoorWorld[] = [];
        const add = (t: Target) => {
            if (seen.has(t.id)) return;
            seen.add(t.id);
            const status = this.statuses.get(t.id);
            const fit = compatibility(ClientBuild, status ?? null, t.listedProtocol);
            const listed = this.listed.find(w => w.id === t.id) ?? null;
            out.push({id: t.id, name: t.name, kind: t.kind, address: addressText(t.address), host: t.host ? shownAddress(t.address, t.host) : '',
                secure: t.address.secure, description: t.description,
                region: listed?.region ?? '', tags: listed?.tags ?? [], fit: fit.fit, note: fit.note, status: statusLine(status, listed)});
        };
        for (const w of this.listed) {
            const t = this.targetOf(w);
            if (t) add(t);
        }
        for (const t of this.direct) add(t);
        return out;
    }

    private targetOf(w: ListedWorld): Target | null {
        const address = parseAddress(w.address, w.secure);
        return address ? {id: w.id, name: w.name, kind: w.kind, address, host: w.host, listedProtocol: w.protocol, description: w.description} : null;
    }

    private async askStatus(w: ListedWorld) {
        const t = this.targetOf(w);
        if (!t) return;
        this.asking.add(w.id);
        const status = await fetchStatus(t.address, this.shell.native ? (c, a) => this.shell.invoke(c, a) : null);
        this.asking.delete(w.id);
        // A world that answers with another id than central's is not that world: say nothing of it.
        this.statuses.set(w.id, status && status.id === w.id ? status : null);
        const fit = compatibility(ClientBuild, this.statuses.get(w.id) ?? null, w.protocol);
        this.door?.updateWorld(w.id, statusLine(this.statuses.get(w.id), w), fit.fit, fit.note);
    }

    /** What went wrong, on the page the door is showing. */
    private fail(text: string) {
        const page = this.door?.page ?? 'login';
        if (page === 'login' || page === 'register') this.lobby('login', false, text);
        else if (page === 'worlds' || page === 'arrive') this.lobby('worlds', false, text);
        else if (page === 'host') this.hostLobby(false, text);
        else this.lobby('characters', false, text);
    }

    // ------------------------------------------------------------------ Commands from the door

    async command(c: Json) {
        const type = str(c, 'type');
        try {
            if (type === 'auth_login') {
                const reply = await this.central.login(str(c, 'username'), str(c, 'password'), c.remember === true);
                this.account = isObject(reply.account) ? reply.account : null;
                await this.signedIn('Signed in.');
            } else if (type === 'auth_register') {
                const month = typeof c.birth_month === 'number' ? c.birth_month : null;
                const reply = await this.central.register(str(c, 'username'), str(c, 'password'), num(c, 'birth_year'), month, c.remember === true);
                this.account = isObject(reply.account) ? reply.account : null;
                await this.signedIn('Your account is made. Choose an empty slot to make your first wolf.');
            } else if (type === 'auth_logout') {
                try {
                    await this.central.logout();
                } catch {
                    // (Signed out here whatever central says.)
                }
                this.signedOut('Signed out.');
            } else if (type === 'account_delete') {
                await this.central.deleteAccount(str(c, 'password'));
                this.signedOut('Your account and its wolves are deleted. Their names are held back for 30 days.');
            } else if (type === 'wolf_create') {
                const wolf: Json = {name: c.name, age: c.age, appearance: c.appearance};
                if (str(c, 'family')) wolf.family = str(c, 'family');
                const made = await this.central.createWolf(wolf);
                this.wolves = await this.central.wolves();
                this.wolf = str(made, 'id');
                this.lobby('characters', true, `${str(made, 'name')} is made. Choose a world when you are ready.`);
            } else if (type === 'wolf_delete') {
                const id = str(c, 'id');
                const name = str(this.wolves.find(w => str(w, 'id') === id), 'name', 'The wolf');
                await this.central.deleteWolf(id, str(c, 'password'));
                this.wolves = await this.central.wolves();
                if (this.wolf === id) this.wolf = str(this.wolves[0], 'id');
                if (this.door) this.door.selectedId = this.wolf;
                this.lobby('characters', true, `${name} is deleted.`);
            } else if (type === 'worlds_show') {
                this.wolf = str(c, 'wolf', this.wolf);
                this.listed = await this.central.worlds();
                this.statuses.clear();
                this.lobby('worlds', true, this.listed.length ? 'Choose a world.' : '');
            } else if (type === 'host_show') {
                await this.loadHostWorlds();
                this.hostLobby(true, this.hostWorlds.length ? '' : 'You have no worlds yet. Give one a name to make it.');
            } else if (type === 'host_make') {
                const {world, token} = await this.central.makeWorld(str(c, 'name').trim());
                await this.reloadHostWorlds(world);
                this.hostLobby(true, `${world?.name || 'The world'} is made. Copy its token now: it is shown only this once.`, token, world?.id ?? '');
            } else if (type === 'host_token') {
                const id = str(c, 'world');
                const token = await this.central.newToken(id);
                await this.reloadHostWorlds(null);
                const name = this.hostWorlds.find(w => w.id === id)?.name || 'the world';
                this.hostLobby(true, `A new token for ${name}: the old one has stopped working. Copy it now: it is shown only this once.`, token, id);
            } else if (type === 'host_remove') {
                const id = str(c, 'world');
                const name = this.hostWorlds.find(w => w.id === id)?.name || 'The world';
                await this.central.removeWorld(id);
                await this.loadHostWorlds();
                this.hostLobby(true, `${name} is removed: central won't list it again.`);
            } else if (type === 'world_join') {
                await this.join(c);
            } else if (type === 'arrive_cancel') {
                this.closeConnection();
                this.lobby('worlds', true, 'You left before arriving.');
            } else if (type === 'character_arrive') {
                if (this.connection) this.connection.submit(c);
                else this.lobby('worlds', false, 'The world is not connected any more. Join it again.');
            } else this.connection?.submit(c);
        } catch (error) {
            if (error instanceof CentralError && error.status === 401 && error.code === 'unauthenticated' && type !== 'auth_login') {
                this.signedOut('Your sign-in has ended. Sign in again.', false);
                return;
            }
            this.fail(message(error));
        }
    }

    private async signedIn(text: string) {
        if (!this.gifts) {
            try {
                const rules = await this.central.rules();
                this.gifts = isObject(rules.gifts) ? rules.gifts : null;
            } catch {
                // (The creator says the families haven't arrived; everything else works.)
            }
        }
        this.wolves = await this.central.wolves();
        if (!this.wolves.some(w => str(w, 'id') === this.wolf)) this.wolf = str(this.wolves[0], 'id');
        this.lobby('characters', true, this.wolves.length ? text : `${text}`);
    }

    private signedOut(text: string, ok = true) {
        this.closeConnection();
        this.account = null;
        this.wolves = [];
        this.wolf = '';
        this.listed = [];
        this.hostWorlds = [];
        this.direct = [];
        this.lost = null;
        this.lobby('login', ok, text);
    }

    // ------------------------------------------------------------------ Joining a world

    private async join(c: Json) {
        const wolf = str(c, 'wolf', this.wolf);
        let target: Target | null = null;
        let status: WorldStatus | null | undefined;
        if (str(c, 'world')) {
            const id = str(c, 'world');
            const listed = this.listed.find(w => w.id === id);
            target = listed ? this.targetOf(listed) : this.direct.find(t => t.id === id) ?? (this.lost?.id === id ? this.lost : null);
            if (!target) return this.fail('That world is not in the list any more. Refresh the list.');
            status = this.statuses.get(id);
        } else {
            const address = parseAddress(str(c, 'address'));
            if (!address) return this.fail('That is not a world\'s address: type host:port, for example 192.168.1.20:7788.');
            status = await fetchStatus(address, this.shell.native ? (cmd, a) => this.shell.invoke(cmd, a) : null);
            if (!status) return this.fail(`No world answers at ${addressText(address)}.`);
            target = {id: status.id, name: status.name || addressText(address), kind: status.kind, address, host: '', listedProtocol: null, description: ''};
            this.statuses.set(status.id, status);
            if (!this.listed.some(w => w.id === status!.id)) this.direct = [target, ...this.direct.filter(t => t.id !== target!.id)].slice(0, 5);
        }
        const fit = compatibility(ClientBuild, status ?? null, target.listedProtocol);
        if (fit.fit !== 'ok' && fit.fit !== 'unknown') return this.fail(fit.note);
        if (!target.address.secure && !isLoopback(target.address.host) && this.door)
            this.door.setMessage(`Joining ${target.name}: not encrypted, so others on the way can read what you do there.`);
        const ticket = await this.central.ticket(target.id, wolf);
        this.wolf = wolf;
        this.connect(target, ticket);
    }

    private connect(target: Target, ticket: string) {
        this.closeConnection();
        this.target = target;
        this.ticket = ticket;
        this.keyTries = 0;
        this.answered = false;
        this.admitted = false;


        const view: SessionView = {
            lobby: event => this.worldLobby(connection, event),
            enter: () => this.entered(connection),
            snapshot: snapshot => this.play?.game.applySnapshot(snapshot),
            motion: frame => this.play?.game.applyMotion(frame),
            event: event => this.play?.game.receiveEvent(event),
            artwork: event => {
                if (event.type === 'artwork') artCache.receive(event);
                this.play?.game.artworkEvent(event);
            },
        };
        const connection = new Connection(worldSocket(target.address, this.relay), view, open => {
            if (open && connection === this.connection) this.hello(connection);
        });
        this.connection = connection;
    }

    private hello(connection: Connection) {
        connection.submitExact({type: 'hello', ticket: this.ticket, protocol: ClientBuild.protocol});
    }

    /** A lobby event from the world (Core/RatwGameTickets.cpp, ticketLobby), or the connection lost. */
    private worldLobby(connection: Connection, event: Json) {
        if (connection !== this.connection) return;
        const name = this.target?.name ?? 'the world';
        if (str(event, 'code') === 'connection_lost') {
            const wasIn = this.answered;
            this.lost = this.target;
            this.toDoor();
            this.connection = null;
            this.lobby('worlds', false, wasIn ? `The connection to ${name} was lost. Reconnect asks central for a new ticket.`
                : `Could not reach ${name} at ${this.target ? shownAddress(this.target.address, this.target.host) : 'its address'}.`);
            return;
        }
        if (str(event, 'mode') !== 'ticket') {
            // A world that answers with today's password lobby: it isn't a ticket world (play it in a browser).
            this.closeConnection();
            this.lobby('worlds', false, `${name} doesn't join by ticket from central: it is a password world, played in a browser on its own computer.`);
            return;
        }
        this.answered = true;
        const stage = str(event, 'stage');
        // The world's greeting, before it has read the hello: nothing to do yet.
        if (stage === 'ticket' && event.ok === true && !this.admitted) return;
        if (stage === 'arrive') {
            this.admitted = true;
            if (this.play) this.toDoor();
            this.door ??= this.newDoor();
            this.door.receive(event);
            return;
        }
        // stage 'ticket': refused, or the wolf has left.
        if (event.ok !== true && str(event, 'code') === 'keys' && this.keyTries < 3) {
            // The world is fetching central's newest key: the ticket is unused, so the same one is tried again.
            ++this.keyTries;
            setTimeout(() => { if (connection === this.connection) this.hello(connection); }, 1500);
            return;
        }
        this.closeConnection();
        this.lobby('worlds', event.ok === true, str(event, 'message', event.ok === true ? 'Your wolf has left the world.' : 'The world refused the join.'));
    }

    private entered(connection: Connection) {
        if (connection !== this.connection) return;
        this.admitted = true;
        this.door?.destroy();
        this.door = null;
        this.lost = null;
        this.play ??= new Play(this.app, connection);
        this.shell.inWorld = true;
    }

    /** Back from the game screen to the door (its state is the client's, so a new door shows it again). */
    private toDoor() {
        this.play?.destroy();
        this.play = null;
        this.shell.inWorld = false;
        if (!this.door) this.newDoor();
    }

    private closeConnection() {
        const connection = this.connection;
        this.connection = null;
        this.toDoor();
        connection?.close();
    }
}

function message(error: unknown): string {
    return error instanceof Error ? error.message : String(error);
}

/** The world's socket: straight to it, or through the shell's loopback relay when the shell runs one. */
export function worldSocket(a: WorldAddress, relay: string | null): string {
    const direct = socketUrl(a);
    return relay && /^ws:\/\/127\.0\.0\.1:[0-9]+\/[0-9a-f]{32,}$/.test(relay) ? `${relay}?to=${encodeURIComponent(direct)}` : direct;
}
