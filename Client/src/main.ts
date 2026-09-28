// The browser client: one connection to the server this page came from, the front door until a character enters,
// then the game screen.
//
//   ?identity=ash&name=Ash   a development identity, as -RatwDevIdentity did (the server must allow it)
//   ?server=host:port        another server than the page's own (development: the page served by Vite)
import {Connection, gameUrl} from './net/connection.ts';
import {FrontDoor} from './ui/frontDoor.ts';
import {GameView} from './game/view.ts';
import type {Json} from './game/json.ts';

const app = document.getElementById('app')!;
const params = new URLSearchParams(location.search);
const server = params.get('server');
const url = server ? `ws://${server}/ws` : gameUrl(location);

let door: FrontDoor | null = null;
let game: GameView | null = null;

const connection = new Connection(url, {
    lobby(event) {
        game?.destroy();
        game = null;
        if (!door) door = new FrontDoor(app, command => connection.submit(command));
        door.receive(event);
    },
    enter() {
        door?.destroy();
        door = null;
        if (!game) game = new GameView(app, command => connection.submit(command));
    },
    snapshot: snapshot => game?.applySnapshot(snapshot),
    motion: frame => game?.applyMotion(frame),
    event: event => game?.receiveEvent(event),
}, open => {
    if (open && params.get('identity'))
        connection.submit({type: 'hello', id: params.get('identity'), name: params.get('name') ?? params.get('identity')});
});

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
        };
    }
}
window.ratw = {
    connection,
    game: () => game,
    door: () => door,
    page: name => {
        if (game) game.state.setPresentationPage(name);
        else if (door && ['login', 'register', 'roster', 'creator', 'review'].includes(name))
            door.show(name as 'login' | 'register' | 'roster' | 'creator' | 'review');
    },
    draft: (name, age, appearance) => door?.setDraft(name, age, appearance),
};
