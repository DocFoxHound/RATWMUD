// What the Unreal player controller did between the wire and the screens (Runtime/RatwPlayerController.cpp): which
// screen shows (the lobby or the world), which snapshots and motion frames belong to the character being played
// (its motion session, its cell and cell generation, never an older revision), the delta sections put back and
// acknowledged, and passwords never sent where the server won't take them. No DOM here: the browser and the headless
// test client both drive it.
import {fill, SectionCache} from './sections.ts';
import type {MotionFrame} from './motion.ts';

export type Json = Record<string, unknown>;

export interface Transport {
    command(json: string): void;
    ack(revision: number, missing: boolean): void;
}

export interface SessionView {
    lobby(event: Json): void;             // The front door: sign in, the roster, creation.
    enter(event: Json): void;             // The world is shown (the "entered" event).
    snapshot(snapshot: Json): void;       // Whole, as if sent whole.
    motion(frame: MotionFrame): void;
    event(event: Json): void;             // Anything else, once in the world.
    artwork?(event: Json): void;          // Uploaded portraits, in the lobby as in the world (doc 29, phase 9).
}

const str = (o: Json | undefined, k: string) => (o && typeof o[k] === 'string' ? (o[k] as string) : '');
const num = (o: Json | undefined, k: string, d = 0) => (o && typeof o[k] === 'number' ? (o[k] as number) : d);

export function lobbyEvent(ok: boolean, message: string): Json {
    return {type: 'lobby', stage: 'login', ok, message, characters: []};
}

export function newCommandId(): string {
    return globalThis.crypto?.randomUUID?.() ?? `c-${Date.now()}-${Math.random().toString(16).slice(2)}`;
}

export class Session {
    enteredWorld = false;
    credentialsAllowed = false;       // As the server's lobby says: passwords only over loopback (or, later, TLS).
    characterId = '';
    motionSession = '';
    cellId = '';
    generation = -1;
    newestGeneration = -1;
    snapshotRevision = -1;
    motionRevision = -1;
    snapshotCount = 0;
    motionFrameCount = 0;
    snapshotBytes = 0;
    latestSnapshot: Json | null = null;
    events: Json[] = [];                  // The newest 256, as the tools and tests read them.
    private sections = new SectionCache();

    private transport: Transport;
    private view: SessionView;

    constructor(transport: Transport, view: SessionView) {
        this.transport = transport;
        this.view = view;
    }

    /** A command from the screens. Credentials go only where the server said it takes them. */
    submit(command: Json) {
        const type = str(command, 'type');
        if ((type === 'auth_login' || type === 'auth_register') && !this.credentialsAllowed) {
            this.showLobby(lobbyEvent(false, 'No credentials were sent. Signing in needs a connection from this computer ' +
                'until the server has encryption.'));
            return;
        }
        const withId = 'commandId' in command ? command : {...command, commandId: newCommandId()};
        this.transport.command(JSON.stringify(withId));
    }

    receiveEvent(event: Json) {
        const type = str(event, 'type');
        if (type === 'lobby') {
            this.showLobby(event);
            return;
        }
        if (type === 'entered') {
            const session = str(event, 'motionSession');
            if (session !== this.motionSession) {
                this.motionSession = session;
                this.cellId = '';
                this.generation = this.newestGeneration = -1;
                this.snapshotRevision = this.motionRevision = -1;
                this.snapshotCount = this.motionFrameCount = 0;
                this.latestSnapshot = null;
                this.sections.reset();
            }
            this.characterId = str(event, 'id') || this.characterId;
            this.enteredWorld = true;
            this.remember(event);
            this.view.enter(event);
            return;
        }
        if (type.startsWith('artwork')) {
            this.view.artwork?.(event);
            return;
        }
        if (!this.enteredWorld) return;
        this.remember(event);
        this.view.event(event);
    }

    receiveSnapshot(snapshot: Json, wireBytes = 0) {
        if (!this.enteredWorld) return;
        const self = snapshot.self as Json | undefined;
        if (!self || str(self, 'id') !== this.characterId) return;
        if (str(snapshot, 'motionSession') !== this.motionSession) return;
        const generation = num(snapshot, 'cellGeneration', -1);
        const revision = num(snapshot, 'revision', -1);
        if (generation < this.newestGeneration || revision <= this.snapshotRevision) return;
        this.snapshotBytes += wireBytes;
        // Put back the parts the server left out because this client holds them; lacking one, ask for everything.
        if (!fill(snapshot, this.sections)) {
            this.transport.ack(revision, true);
            return;
        }
        this.transport.ack(revision, false);
        this.newestGeneration = this.generation = generation;
        this.cellId = str(snapshot.cell as Json | undefined, 'id');
        this.snapshotRevision = revision;
        ++this.snapshotCount;
        this.latestSnapshot = snapshot;
        this.view.snapshot(snapshot);
    }

    receiveMotion(frame: MotionFrame) {
        if (!this.enteredWorld) return;
        if (frame.motionSession !== this.motionSession || frame.observer !== this.characterId) return;
        if (frame.cellGeneration < this.newestGeneration || frame.revision <= this.motionRevision ||
            frame.revision < this.snapshotRevision) return;
        this.newestGeneration = frame.cellGeneration;
        this.motionRevision = frame.revision;
        if (frame.cellGeneration !== this.generation || frame.cellId !== this.cellId) return;
        ++this.motionFrameCount;
        this.view.motion(frame);
    }

    connectionLost() {
        this.showLobby(lobbyEvent(false, 'The connection to the world server was lost.'));
    }

    private showLobby(event: Json) {
        if (typeof event.credentialsAllowed === 'boolean') this.credentialsAllowed = event.credentialsAllowed;
        this.enteredWorld = false;
        this.characterId = '';
        this.motionSession = this.cellId = '';
        this.generation = this.newestGeneration = -1;
        this.snapshotRevision = this.motionRevision = -1;
        this.latestSnapshot = null;
        this.events = [];
        this.view.lobby(event);
    }

    private remember(event: Json) {
        this.events.push(event);
        if (this.events.length > 256) this.events.shift();
    }
}
