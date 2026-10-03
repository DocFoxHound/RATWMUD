// The browser client: one connection to the server this page came from, the front door until a character enters,
// then the game screen.
//
//   ?identity=ash&name=Ash   a development identity, as -RatwDevIdentity did (the server must allow it)
//   ?server=host:port        another server than the page's own (development: the page served by Vite)
import {Connection, gameUrl} from './net/connection.ts';
import {FrontDoor} from './ui/frontDoor.ts';
import {GameView} from './game/view.ts';
import type {Json} from './game/json.ts';
import {artCache} from './ui/artwork.ts';
import {Walker} from './game/walker.ts';

const app = document.getElementById('app')!;
const params = new URLSearchParams(location.search);
const server = params.get('server');
const url = server ? `ws://${server}/ws` : gameUrl(location);

let door: FrontDoor | null = null;
let game: GameView | null = null;
// The page's own walking (doc 31, Phase 3): the server's movement rules as WebAssembly. Until it has loaded (or where it
// can't run) the server walks the wolf, as before.
let walker: Walker | null = null;
Walker.load(new URL('./wasm/walk.wasm', import.meta.url).href).then(w => {
    walker = w;
    if (game && w) game.state.walker = w;
});
let lastLobby: Json | null = null;

const connection = new Connection(url, {
    lobby(event) {
        lastLobby = event;
        game?.destroy();
        game = null;
        if (!door) door = new FrontDoor(app, command => connection.submit(command));
        door.receive(event);
    },
    enter() {
        door?.destroy();
        door = null;
        if (!game) {
            game = new GameView(app, command => connection.submit(command));
            game.netSample = () => connection.netSample();
            game.state.walker = walker;
            game.state.poseSender = (seq, x, y, facing, ix, iy) => connection.sendPose(seq, x, y, facing, ix, iy);
        }
    },
    snapshot: snapshot => game?.applySnapshot(snapshot),
    motion: frame => game?.applyMotion(frame),
    event: event => game?.receiveEvent(event),
    artwork: event => {
        if (event.type === 'artwork') artCache.receive(event);
        door?.artworkEvent(event);
        game?.artworkEvent(event);
    },
}, open => {
    if (open && params.get('identity'))
        connection.submit({type: 'hello', id: params.get('identity'), name: params.get('name') ?? params.get('identity')});
});

artCache.send = command => connection.submit(command);

// Before the server says anything, the front door says so.
door = new FrontDoor(app, command => connection.submit(command));
door.setMessage('Connecting to the world…');

/** For the tools that drive the page (screenshots, smokes): never used by the game itself. */
declare global {
    interface Window {
        ratw: {
            connection: Connection;
            game: () => GameView | null;
            door: () => FrontDoor | null;
            page: (name: string) => void;
            draft: (name: string, age: number, appearance: Json) => void;
            lobby: () => Json | null;
        };
    }
}
window.ratw = {
    connection,
    game: () => game,
    door: () => door,
    page: name => {
        if (game) game.state.setPresentationPage(name);
        else door?.presentationPage(name);
    },
    draft: (name, age, appearance) => door?.setDraft(name, age, appearance),
    lobby: () => lastLobby,
};
