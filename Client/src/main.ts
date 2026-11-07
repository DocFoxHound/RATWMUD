// The game page. Two ways in:
//   - a browser on a password-mode world (DEV, scratch servers, this laptop): one connection to the server this page
//     came from, the front door until a character enters, then the game screen, as always;
//   - the desktop client (Docs/Design/77-central-accounts-and-hosted-worlds.md, Phase 3), which bundles this page:
//     sign in to central, the wolves, the world list, and a world reached at its own address (client.ts).
//
//   ?identity=ash&name=Ash   a development identity, as -RatwDevIdentity did (the server must allow it)
//   ?server=host:port        another server than the page's own (development: the page served by Vite)
//   ?central=URL             the client's flow in a browser, against that central (development and tests; central
//                            started with --cors for this page's origin); the desktop client needs none of these
import {Connection, gameUrl} from './net/connection.ts';
import {Central, FetchCentral, ShellCentral} from './net/central.ts';
import {FrontDoor} from './ui/frontDoor.ts';
import type {GameView} from './game/view.ts';
import type {Json} from './game/json.ts';
import {artCache} from './ui/artwork.ts';
import {ask} from './ui/dialog.ts';
import {loadWalker, Play} from './play.ts';
import {Shell} from './shell.ts';
import {ClientApp} from './client.ts';

const app = document.getElementById('app')!;
const params = new URLSearchParams(location.search);
const shell = new Shell();
shell.install();
void loadWalker();

/** For the tools that drive the page (screenshots, smokes, tests): never used by the game itself. */
declare global {
    interface Window {
        ratw: {
            connection: Connection | null;
            game: () => GameView | null;
            door: () => FrontDoor | null;
            page: (name: string) => void;
            draft: (name: string, age: number, appearance: Json) => void;
            lobby: () => Json | null;
            client?: ClientApp;
            shell: Shell;
        };
    }
}

const centralParam = params.get('central');
if (shell.native || centralParam) void startClient();
else startWorldPage();

/** The desktop client, or its flow in a browser against `?central=`. */
async function startClient() {
    let transport;
    if (shell.native) {
        const info = await shell.invoke<{where: string; development: boolean}>('central_info');
        transport = new ShellCentral(info, (command, args) => shell.invoke(command, args));
    } else transport = new FetchCentral(centralParam!);
    const client = new ClientApp(app, new Central(transport), shell);
    window.ratw = {
        get connection() { return client.connection; },
        game: () => client.play?.game ?? null,
        door: () => client.door,
        page: name => {
            if (client.play) client.play.game.state.setPresentationPage(name);
            else client.door?.presentationPage(name);
        },
        draft: (name, age, appearance) => client.door?.setDraft(name, age, appearance),
        lobby: () => null,
        client,
        shell,
    };
    await client.start();
}

/** A browser on a password-mode world: the page as it has always been. */
function startWorldPage() {
    const server = params.get('server');
    const url = server ? `ws://${server}/ws` : gameUrl(location);
    let door: FrontDoor | null = null;
    let play: Play | null = null;
    let lastLobby: Json | null = null;
    shell.askToClose = () => ask('Leave the world? Your wolf leaves it with you.', {ok: 'Leave', cancel: 'Stay'});

    const connection: Connection = new Connection(url, {
        lobby(event) {
            lastLobby = event;
            play?.destroy();
            play = null;
            shell.inWorld = false;
            if (!door) door = new FrontDoor(app, command => connection.submit(command));
            door.receive(event);
            if (event.code === 'connection_lost') offerReconnect(door);
        },
        enter() {
            door?.destroy();
            door = null;
            play ??= new Play(app, connection);
            shell.inWorld = true;
        },
        snapshot: snapshot => play?.game.applySnapshot(snapshot),
        motion: frame => play?.game.applyMotion(frame),
        event: event => play?.game.receiveEvent(event),
        artwork: event => {
            if (event.type === 'artwork') artCache.receive(event);
            door?.artworkEvent(event);
            play?.game.artworkEvent(event);
        },
    }, open => {
        if (open && params.get('identity'))
            connection.submit({type: 'hello', id: params.get('identity'), name: params.get('name') ?? params.get('identity')});
    });

    artCache.send = command => connection.submit(command);

    // Before the server says anything, the front door says so.
    door = new FrontDoor(app, command => connection.submit(command));
    door.setMessage('Connecting to the world…');

    window.ratw = {
        connection,
        game: () => play?.game ?? null,
        door: () => door,
        page: name => {
            if (play) play.game.state.setPresentationPage(name);
            else door?.presentationPage(name);
        },
        draft: (name, age, appearance) => door?.setDraft(name, age, appearance),
        lobby: () => lastLobby,
        shell,
    };
}

/** The connection lost (doc 76, 3.1 point 4): a Reconnect button that waits for the server to answer again (same
 *  origin, every 5 s ±50%) and then loads the page anew. */
function offerReconnect(door: FrontDoor) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'secondary door-lost';
    button.textContent = 'RECONNECT';
    button.dataset.action = 'reconnect';
    let waiting = false;
    button.addEventListener('click', () => {
        if (waiting) return;
        waiting = true;
        button.disabled = true;
        door.setMessage('Waiting for the world server to answer…');
        const poll = async () => {
            try {
                const answer = await fetch(location.href, {cache: 'no-store', signal: AbortSignal.timeout(4000)});
                if (answer.ok) return location.reload();
            } catch {
                // (Not up yet.)
            }
            setTimeout(poll, 5000 * (0.5 + Math.random()));
        };
        void poll();
    });
    door.root.querySelector('.door-login, .door-detail')?.append(button);
}
