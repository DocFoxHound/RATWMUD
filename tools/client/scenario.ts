// The scripted player: the scenarios the Unreal client ran with -RatwScenario (Testing/RatwScenario.cpp and
// RatwCharacterScenario.cpp), for the browser client (Docs/Design/27-browser-client.md). Each scenario plays one
// character against a running server and writes its evidence, as before, to <capture>/<scenario>-<identity>.json.
//
//   node --experimental-strip-types tools/client/scenario.ts --port 7788 --scenario network --identity ash
//        [--name Ash] [--dev] [--capture DIR] [--browser] [--screenshots]
//
// --dev signs in with a development identity (the server must allow them), as -RatwDevIdentity did. Without
// --browser the scenario runs on the client's own network code in Node, headless; with it, in the real page in
// headless Chromium, and --screenshots then captures the screens the scenario names.
import {mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {Connection} from '../../Client/src/net/connection.ts';
import {Browser} from './browser.mjs';

type J = any;

interface State {
    snapshot: J;
    lobby: J;
    events: J[];
    motionFrames: number;
    snapshotCount: number;
    snapshotBytes: number;
}

interface Driver {
    read(): Promise<State>;
    send(command: J): Promise<void>;
    page(name: string): Promise<void>;
    draft(name: string, age: number, appearance: J): Promise<void>;
    capture(path: string): Promise<void>;
    altHover(x: number, y: number): Promise<void>;
    altRelease(): Promise<void>;
    close(): Promise<void>;
}

const args = new Map<string, string>();
for (let i = 2; i < process.argv.length; ++i) {
    const a = process.argv[i];
    if (!a.startsWith('--')) continue;
    const next = process.argv[i + 1];
    if (next !== undefined && !next.startsWith('--')) args.set(a.slice(2), process.argv[++i]);
    else args.set(a.slice(2), 'true');
}
const port = Number(args.get('port') ?? 7788);
const scenario = args.get('scenario') ?? '';
const role = args.get('identity') ?? 'ash';
const name = args.get('name') ?? role[0].toUpperCase() + role.slice(1);
const dev = args.has('dev');
const output = args.get('capture') ?? 'artifacts/screenshots';
const screenshots = args.has('screenshots');
const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms));

// ------------------------------------------------------------------ Drivers

async function nodeDriver(): Promise<Driver> {
    let lobby: J = null;
    const connection = new Connection(`ws://127.0.0.1:${port}/ws`, {
        lobby: e => { lobby = e; }, enter: () => {}, snapshot: () => {}, motion: () => {}, event: () => {},
    }, open => {
        if (open && dev) connection.submit({type: 'hello', id: role, name});
    });
    const s = connection.session;
    return {
        read: async () => ({snapshot: s.latestSnapshot, lobby, events: s.events, motionFrames: s.motionFrameCount,
            snapshotCount: s.snapshotCount, snapshotBytes: s.snapshotBytes}),
        send: async c => connection.submit(c),
        page: async () => {},
        draft: async () => {},
        capture: async () => {},
        altHover: async () => {},
        altRelease: async () => {},
        close: async () => connection.close(),
    };
}

async function browserDriver(): Promise<Driver> {
    const browser = await Browser.launch();
    const query = dev ? `?identity=${encodeURIComponent(role)}&name=${encodeURIComponent(name)}` : '';
    const page = await browser.open(`http://127.0.0.1:${port}/${query}`);
    await page.waitFor('window.ratw && window.ratw.connection');
    const js = (v: unknown) => JSON.stringify(v);
    // Canvas coordinates to the page's: the 1600×1000 layout is scaled to the window and centred.
    const toPage = (x: number, y: number) => page.evaluate(`(() => {
        const s = Math.min(innerWidth / 1600, innerHeight / 1000);
        return [(innerWidth - 1600 * s) / 2 + ${x} * s, (innerHeight - 1000 * s) / 2 + ${y} * s]; })()`);
    return {
        read: () => page.evaluate(`(() => { const c = window.ratw.connection, s = c.session;
            return {snapshot: s.latestSnapshot, lobby: window.ratw.lobby(), events: s.events, motionFrames: s.motionFrameCount,
                snapshotCount: s.snapshotCount, snapshotBytes: s.snapshotBytes}; })()`),
        send: c => page.evaluate(`window.ratw.connection.submit(${js(c)})`),
        page: n => page.evaluate(`window.ratw.page(${js(n)})`),
        draft: (n, age, a) => page.evaluate(`window.ratw.draft(${js(n)}, ${age}, ${js(a)})`),
        capture: async path => {
            await sleep(250);                  // Let a frame draw what just changed.
            await page.screenshot(path);
        },
        altHover: async (x, y) => {
            const [px, py] = await toPage(x, y);
            await page.keyEvent('keyDown', 'AltLeft');
            await page.send('Input.dispatchMouseEvent', {type: 'mouseMoved', x: px, y: py, modifiers: 1});
        },
        altRelease: () => page.keyEvent('keyUp', 'AltLeft'),
        close: async () => browser.close(),
    };
}

// ------------------------------------------------------------------ Helpers

const str = (o: J, k: string, d = '') => (o && typeof o[k] === 'string' ? o[k] : d);
const num = (o: J, k: string, d = 0) => (o && typeof o[k] === 'number' ? o[k] : d);
const bool = (o: J, k: string) => !!(o && o[k] === true);
const obj = (o: J, k: string) => (o && o[k] && typeof o[k] === 'object' && !Array.isArray(o[k]) ? o[k] : null);
const arr = (o: J, k: string): J[] => (o && Array.isArray(o[k]) ? o[k] : []);
const mentions = (events: J[], text: string) => events.filter(e => JSON.stringify(e).includes(text)).length;
const visibleTiles = (s: J) => arr(s, 'visibility').reduce((n: number, row: string) => n + [...row].filter(c => c === '2').length, 0);

// One of six appearances the character scenario creates (RatwCharacterScenario.cpp's Choice).
function choice(i: number): J {
    const species = ['timber', 'maned', 'arctic', 'red', 'ethiopian', 'timber'];
    return {species: species[Math.min(5, Math.max(0, i))], sex: i % 2 ? 'male' : 'female',
        stature: i === 1 ? 'tall' : i === 3 || i === 5 ? 'short' : 'average',
        baseColor: i === 2 ? 0 : i === 0 ? 7 : i === 5 ? 5 : 6, gradientColor: i === 2 ? 1 : 0, markingColor: 5,
        gradientAmount: 0.45, patternAmount: i === 2 ? 0.15 : 0.6,
        pattern: i === 0 ? 'mantle' : i === 1 ? 'solid' : i === 3 ? 'piebald' : 'saddle'};
}
const Names = ['Lark', 'Sorrel', 'Frost', 'Briar', 'Ember', 'Flint'];
const Ages = [27, 22, 68, 15, 9, 45];

// ------------------------------------------------------------------ The run

const driver = await (args.has('browser') ? browserDriver() : nodeDriver());
mkdirSync(output, {recursive: true});
const started = Date.now() / 1000;
let state: State = {snapshot: null, lobby: null, events: [], motionFrames: 0, snapshotCount: 0, snapshotBytes: 0};
let finished: {passed: boolean; detail: string} | null = null;
let initialX = 0, finalX = 0, maxEntities = 0;
let dialogueEvidence: J = null;
const checks: string[] = [];
let firstId = '';

async function finish(passed: boolean, detail: string) {
    if (finished) return;
    finished = {passed, detail};
    const characters = scenario.startsWith('characters-');
    const result: J = characters
        ? {passed, scenario, role, detail, step: step, firstId, checks, lastSnapshot: state.snapshot ?? undefined, lastLobby: state.lobby ?? undefined}
        : {scenario, role, passed, detail, initialX, finalX, maxVisibleEntities: maxEntities, motionFrames: state.motionFrames,
            fullSnapshots: state.snapshotCount, snapshotBytesReceived: state.snapshotBytes, events: state.events,
            lastSnapshot: state.snapshot ?? undefined};
    if (dialogueEvidence) result.dialogueEvidence = dialogueEvidence;
    writeFileSync(join(output, `${scenario}-${role}.json`), JSON.stringify(result));
    console.log(`RATW_SCENARIO ${passed ? 'PASS' : 'FAIL'} ${detail}`);
}

async function capture(file: string, when = screenshots) {
    if (!when) return;
    const path = join(output, file);
    await driver.capture(path);
    console.log(`RATW_CAPTURE ${path}`);
}

let step = 0, stepAt = 0;
const memory: Record<string, J> = {};           // What a scenario remembers between steps (the C++ statics).

const scenarios: Record<string, (t: Tick) => Promise<void>> = {};
interface Tick {
    s: J;                   // The latest snapshot (whole).
    self: J;
    elapsed: number;
    cell: string;
    x: number;
    y: number;
    events: J[];
    send: (c: J) => Promise<void>;
    next: () => void;
    near: (x: number, y: number) => boolean;
    check: (ok: boolean, detail: string) => Promise<boolean>;
}

scenarios['dm-observer'] = async t => {
    if (t.s.director || t.s.characters || t.s.accounts) return finish(false, 'Private operator data leaked into a player snapshot');
    if (step === 0 && t.elapsed > 1) {
        await t.send({type: 'economy_transfer', from: 'treasury', to: 'player-ash', coins: 500, commandId: 'public-dm-probe'});
        await t.send({type: 'action', action: 'look', target: '', commandId: 'dm-observer-active'});
        t.next();
    }
    if (mentions(t.events, 'DM_SMOKE_DONE')) {
        const good = num(t.self, 'cash') === 23;
        if (!good && t.elapsed - stepAt < 110) return;
        await finish(good, good ? 'Separate operator notice reaches the real client; private finite transfer applies once; public command cannot mint money; player snapshot excludes operator records'
            : 'Operator transfer or public command boundary mismatch');
    }
};

for (const name of ['age-register', 'age-advance', 'age-return'])
    scenarios[name] = async t => {
        if (scenario === 'age-advance' && step === 0 && t.elapsed > 1) {
            if (arr(t.s, 'entities').some(e => str(e, 'id') === 'player-ash')) {
                if (t.elapsed < 12) return;
                return finish(false, 'Logged-out Ash is still present before the calendar advance');
            }
            await t.send({type: 'calendar', value: 'year', commandId: 'offline-aging-year-probe'});
            t.next();
        } else if (t.elapsed > 1 && scenario !== 'age-advance') {
            const notices = mentions(t.events, 'A birthday has passed');
            const returning = scenario === 'age-return';
            const good = num(t.self, 'age') === (returning ? 19 : 18) && num(t.self, 'strength') === (returning ? 51 : 50) &&
                num(t.self, 'cash') === 20 && notices === (returning ? 1 : 0);
            if (!good && t.elapsed < 12) return;
            await finish(good, good ? (returning ? 'Offline character ages once on the running shared server, receives one notice, and keeps the same purse'
                : 'Fresh character registered at age eighteen with a single finite grant') : 'Offline birthday or finite welcome grant mismatch');
        } else if (scenario === 'age-advance' && step === 1 && t.elapsed - stepAt > 1 && (num(t.self, 'age') === 19 || t.elapsed - stepAt > 12)) {
            if (num(t.self, 'age') !== 19) return finish(false, 'Second character could not advance the shared calendar');
            await t.send({type: 'calendar', value: 'year', commandId: 'offline-aging-year-probe'});
            t.next();
        } else if (scenario === 'age-advance' && step === 2 && t.elapsed - stepAt > 1) {
            const notices = mentions(t.events, 'A birthday has passed'), replies = mentions(t.events, 'Shared calendar advanced.');
            const good = num(t.self, 'age') === 19 && notices === 1 && replies === 2;
            if (!good && t.elapsed - stepAt < 12) return;
            await finish(good, good ? 'Calendar advances with Ash absent; a retried command replays its own receipt, not a second birthday or reward'
                : 'Command retry changed age, duplicated a birthday, or lost its original receipt');
        }
    };

scenarios['society-restore'] = async t => {
    if (t.elapsed < 1) return;
    const previous = JSON.parse(readFileSync(join(output, 'society-ash.json'), 'utf8')).lastSnapshot;
    const old = obj(previous, 'self');
    const date = obj(obj(obj(t.s, 'cell'), 'environment'), 'calendar');
    const good = !!old && num(t.self, 'age') === num(old, 'age') && num(t.self, 'cash') === num(old, 'cash') &&
        num(t.self, 'strength') === num(old, 'strength') && num(date, 'year') === 2 && !mentions(t.events, 'A birthday has passed') &&
        bool(t.s, 'persistenceHealthy');
    await finish(good, good ? 'Calendar, birthday reward, finite purse and sent-notice state survive graceful process restart'
        : 'Saved calendar, purse or birthday notice changed on restart');
};

scenarios['society'] = async t => {
    const m = memory;
    const count = (id: string) => num(arr(t.s, 'inventory').find(i => str(i, 'id') === id), 'quantity');
    const merchant = obj(t.s, 'merchant');
    const date = obj(obj(obj(t.s, 'cell'), 'environment'), 'calendar');
    const price = (id: string, key: string) => num(arr(merchant, 'items').find(i => str(i, 'id') === id), key);
    if (step === 0 && t.elapsed > 1) {
        if (!await t.check(num(t.self, 'age') === 18 && num(t.self, 'cash') === 20, 'Fresh character did not receive finite welcome allocation and starting age')) return;
        await t.send({type: 'time', value: 'day'});
        await t.send({type: 'path', x: 10.5, y: 6.5});
        t.next();
    } else if (step === 1 && t.near(10.5, 6.5) && merchant) {
        await driver.page('trade');
        Object.assign(m, {cash: num(t.self, 'cash'), meals: count('meal'), herbs: count('herbs'), price: price('meal', 'buyPrice')});
        t.next();
    } else if (step === 2 && t.elapsed - stepAt > 1) {
        await capture('29-finite-merchant.png');
        await t.send({type: 'trade', target: 'npc_keeper', item: 'meal', quantity: 1, buy: true});
        t.next();
    } else if (step === 3 && t.elapsed - stepAt > 1 && (count('meal') === m.meals + 1 || t.elapsed - stepAt > 12)) {
        if (!await t.check(count('meal') === m.meals + 1 && num(t.self, 'cash') === m.cash - m.price, 'Buying did not atomically move real cash and a meal')) return;
        m.cash = num(t.self, 'cash');
        m.price = price('herbs', 'sellPrice');
        await t.send({type: 'trade', target: 'npc_keeper', item: 'herbs', quantity: 1, buy: false});
        t.next();
    } else if (step === 4 && t.elapsed - stepAt > 1 && (count('herbs') === m.herbs - 1 || t.elapsed - stepAt > 12)) {
        if (!await t.check(count('herbs') === m.herbs - 1 && num(t.self, 'cash') === m.cash + m.price, 'Selling did not atomically move real cash and herbs')) return;
        m.cash = num(t.self, 'cash');
        await t.send({type: 'trade', target: 'npc_keeper', item: 'junk', quantity: 1, buy: false});
        await t.send({type: 'eat'});
        t.next();
    } else if (step === 5 && t.elapsed - stepAt > 1 && (count('meal') === m.meals || t.elapsed - stepAt > 12)) {
        if (!await t.check(count('meal') === m.meals && num(t.self, 'cash') === m.cash, 'Eating or refusal of unwanted goods violated inventory/cash')) return;
        m.age = num(t.self, 'age');
        m.strength = num(t.self, 'strength');
        await t.send({type: 'calendar', value: 'year'});
        t.next();
    } else if (step === 6 && t.elapsed - stepAt > 1 && (num(t.self, 'age') === m.age + 1 || t.elapsed - stepAt > 12)) {
        if (!await t.check(num(t.self, 'age') === m.age + 1 && num(t.self, 'strength') === m.strength + 1 && num(date, 'year') === 2 &&
            mentions(t.events, 'A birthday has passed') > 0, 'Annual age, stats, calendar or notification failed')) return;
        await driver.page('character');
        t.next();
    } else if (step === 7 && t.elapsed - stepAt > 1) {
        await capture('30-birthday-character.png');
        t.next();
    } else if (step === 8 && t.elapsed - stepAt > 1) {
        await driver.page('local');
        await capture('32-birthday-notice.png');
        await t.send({type: 'path', x: 20.5, y: 11.5});
        t.next();
    } else if (step === 9 && t.near(20.5, 11.5)) {
        if (!await t.check(!merchant, 'Remote trader inventory leaked after leaving reach')) return;
        await t.send({type: 'trade', target: 'npc_keeper', item: 'meal', quantity: 1, buy: true});
        await t.send({type: 'time', value: 'night'});
        t.next();
    } else if (step === 10 && t.elapsed - stepAt > 8) {
        if (!await t.check(num(t.self, 'cash') === m.cash && num(date, 'moonIllumination') > 0.1 && bool(t.s, 'persistenceHealthy'),
            'Remote trade guard, lunar progression or persistence failed')) return;
        await capture('31-nightly-rest.png');
        t.next();
    } else if (step === 11 && t.elapsed - stepAt > 1)
        await finish(true, 'Real client: finite purchase/sale, refused junk and remote trade, consumed meal, annual reward/notice, moon and saved night');
};

scenarios['lighting-restore'] = async t => {
    if (t.elapsed < 1) return;
    const env = obj(obj(t.s, 'cell'), 'environment');
    const kept = t.cell === 'room_1' && str(env, 'phase') === 'night' && num(env, 'illumination') < 0.1 &&
        num(env, 'artificialLight') === 0 && num(env, 'daylightAccess') === 0;
    await finish(kept, kept ? 'Unlit room, daylight access and night clock survived a separate process restart'
        : 'Restart changed the persisted indoor light profile');
};

scenarios['lighting'] = async t => {
    const env = obj(obj(t.s, 'cell'), 'environment');
    const light = num(env, 'illumination', -1), glow = num(env, 'glowStrength', -1), phase = str(env, 'phase');
    const visible = visibleTiles(t.s);
    const shot = (f: string) => capture(f);
    const m = memory;
    if (step === 0 && t.elapsed > 1) {
        await t.send({type: 'time', value: 'day'});
        t.next();
    } else if (step === 1 && t.elapsed - stepAt > 1.2) {
        if (!await t.check(t.cell === 'room_1' && light === 1 && glow === 0 && visible > 80, 'Authored daylit tavern did not load with clear visibility and no glow')) return;
        m.dayTiles = visible;
        await shot('23-tavern-day.png');
        t.next();
    } else if (step === 2 && t.elapsed - stepAt > 0.7) {
        await t.send({type: 'time', value: 'night'});
        t.next();
    } else if (step === 3 && t.elapsed - stepAt > 1.2) {
        if (!await t.check(phase === 'night' && light === 1 && glow > 0.9 && visible === m.dayTiles && str(env, 'lightingTone') === 'warm',
            'Night tavern must glow warmly without losing daytime visibility')) return;
        await shot('24-tavern-warm-night.png');
        t.next();
    } else if (step === 4 && t.elapsed - stepAt > 0.7) {
        await t.send({type: 'lighting', value: 'unlit'});
        t.next();
    } else if (step === 5 && t.elapsed - stepAt > 1.2) {
        if (!await t.check(light < 0.1 && glow === 0 && visible < m.dayTiles / 4 && num(env, 'hearing') === 1 && num(env, 'scent') === 1,
            'An unlit interior must restrict sight without disabling hearing or smell')) return;
        await shot('25-tavern-unlit-night.png');
        t.next();
    } else if (step === 6 && t.elapsed - stepAt > 0.7) {
        await t.send({type: 'time', value: 'day'});
        t.next();
    } else if (step === 7 && t.elapsed - stepAt > 1.2) {
        if (!await t.check(phase === 'day' && light < 0.1 && glow === 0 && visible < m.dayTiles / 4, 'A sealed unlit interior should remain dark at noon')) return;
        await shot('26-unlit-cellar-day.png');
        t.next();
    } else if (step === 8 && t.elapsed - stepAt > 0.7) {
        await t.send({type: 'lighting', value: 'daylit'});
        t.next();
    } else if (step === 9 && t.elapsed - stepAt > 1.2) {
        if (!await t.check(light === 1 && glow === 0 && visible === m.dayTiles && num(env, 'artificialLight') === 0,
            'Daylight access should relight a windowed room without artificial light')) return;
        await t.send({type: 'time', value: 'night'});
        t.next();
    } else if (step === 10 && t.elapsed - stepAt > 1.2) {
        if (!await t.check(light < 0.1 && visible < m.dayTiles / 4, 'A daylight-only room should darken after sunset')) return;
        await t.send({type: 'lighting', value: 'cool'});
        t.next();
    } else if (step === 11 && t.elapsed - stepAt > 1.2) {
        if (!await t.check(light === 1 && glow > 0.9 && str(env, 'lightingTone') === 'cool', 'Cool artificial lighting should restore sight with a different atmosphere')) return;
        await shot('27-tavern-cool-night.png');
        t.next();
    } else if (step === 12 && t.elapsed - stepAt > 0.7) {
        await t.send({type: 'path', x: 10.5, y: 3.5});
        t.next();
    } else if (step === 13 && t.near(10.5, 3.5)) {
        await t.send({type: 'action', target: 'link_tavern_b', action: 'open'});
        t.next();
    } else if (step === 14 && t.cell === 'cell_1' && t.elapsed - stepAt > 1.2) {
        if (!await t.check(phase === 'night' && light < 0.3 && glow === 0, 'Leaving the lit interior should restore outdoor night conditions')) return;
        await shot('28-outdoor-night-edges.png');
        t.next();
    } else if (step === 15 && t.elapsed - stepAt > 0.7) {
        await t.send({type: 'action', target: 'link_tavern_a', action: 'enter'});
        t.next();
    } else if (step === 16 && t.cell === 'room_1' && t.elapsed - stepAt > 0.7) {
        await t.send({type: 'lighting', value: 'unlit'});
        t.next();
    } else if (step === 17 && t.elapsed - stepAt > 1.5)
        await finish(light < 0.1, 'Daylit/no-glow, warm night, unlit day/night, daylight-only and cool interiors; outdoor night transition verified');
};

scenarios['weather-restore'] = async t => {
    if (t.elapsed < 1) return;
    const cell = obj(t.s, 'cell'), env = obj(cell, 'environment');
    const restored = t.cell === 'room_1' && str(env, 'phase') === 'night' && str(cell, 'weather') === 'rain' && num(env, 'sight') === 1;
    await finish(restored, restored ? 'Shared night clock and weather survived process restart; indoor shelter remains neutral'
        : 'Weather/clock/shelter did not survive process restart');
};

scenarios['weather'] = async t => {
    const cell = obj(t.s, 'cell'), env = obj(cell, 'environment');
    const weather = str(cell, 'weather'), phase = str(env, 'phase');
    const visible = visibleTiles(t.s);
    const m = memory;
    if (step === 0 && t.elapsed > 1) {
        await t.send({type: 'time', value: 'day'});
        await t.send({type: 'weather', value: 'clear'});
        await t.send({type: 'wind', value: 'east'});
        t.next();
    } else if (step === 1 && t.elapsed - stepAt > 1.5) {
        if (!await t.check(weather === 'clear' && phase === 'day' && num(env, 'sight') === 1 && visible > 100, 'Clear-day environment did not reach client')) return;
        m.dayVisible = visible;
        m.dayHearing = num(env, 'hearing');
        await capture('17-weather-day.png');
        t.next();
    } else if (step === 2 && t.elapsed - stepAt > 0.8) {
        await t.send({type: 'weather', value: 'rain'});
        t.next();
    } else if (step === 3 && t.elapsed - stepAt > 1.5) {
        if (!await t.check(weather === 'rain' && num(env, 'sight') < 1 && num(env, 'hearing') < m.dayHearing && num(env, 'scent') < 1 &&
            num(env, 'movement') < 1, 'Rain did not modify all four environment channels')) return;
        await capture('18-weather-rain.png');
        t.next();
    } else if (step === 4 && t.elapsed - stepAt > 0.8) {
        await t.send({type: 'weather', value: 'snow'});
        t.next();
    } else if (step === 5 && t.elapsed - stepAt > 1.5) {
        if (!await t.check(weather === 'snow' && num(env, 'sight') < 1 && num(env, 'hearing') < m.dayHearing && num(env, 'scent') < 1 &&
            num(env, 'movement') < 0.85, 'Snow did not modify senses and movement')) return;
        await capture('19-weather-snow.png');
        t.next();
    } else if (step === 6 && t.elapsed - stepAt > 0.8) {
        await t.send({type: 'weather', value: 'fog'});
        t.next();
    } else if (step === 7 && t.elapsed - stepAt > 1.5) {
        if (!await t.check(weather === 'fog' && visible < m.dayVisible * 0.7 && num(env, 'sight') <= 0.4, 'Fog did not reduce authoritative visible terrain')) return;
        await capture('20-weather-fog.png');
        t.next();
    } else if (step === 8 && t.elapsed - stepAt > 0.8) {
        await t.send({type: 'weather', value: 'clear'});
        await t.send({type: 'time', value: 'night'});
        t.next();
    } else if (step === 9 && t.elapsed - stepAt > 1.5) {
        if (!await t.check(phase === 'night' && visible < m.dayVisible * 0.5 && num(env, 'illumination') < 0.4 &&
            Math.abs(num(env, 'hearing') - m.dayHearing) < 0.001 && num(env, 'scent') === 1, 'Night must hide distant terrain without silencing hearing or scent')) return;
        await capture('21-weather-night.png');
        t.next();
    } else if (step === 10 && t.elapsed - stepAt > 0.8) {
        await t.send({type: 'action', target: 'link_shelter_a', action: 'open'});
        t.next();
    } else if (step === 11 && t.cell === 'room_1' && t.elapsed - stepAt > 0.8) {
        await t.send({type: 'weather', value: 'rain'});
        t.next();
    } else if (step === 12 && t.elapsed - stepAt > 1.5) {
        const neutral = !bool(cell, 'outdoors') && phase === 'night' && weather === 'rain' &&
            ['illumination', 'sight', 'hearing', 'scent', 'movement'].every(k => num(env, k) === 1);
        if (!await t.check(neutral, 'Indoor shelter inherited outdoor weather/night penalties')) return;
        await capture('22-weather-shelter.png');
        t.next();
    } else if (step === 13 && t.elapsed - stepAt > 1.5)
        await finish(true, 'Day/rain/snow/fog/night changed authoritative conditions and visible terrain; interior stayed sheltered');
};

scenarios['travel'] = async t => {
    const travel = obj(t.s, 'travel');
    const stamina = num(t.self, 'stamina', -1), speed = num(t.self, 'currentSpeed');
    const m = memory;
    m.sawMiddle ||= t.cell === 'cell_2';
    if (step === 0 && t.elapsed > 1) {
        await t.send({type: 'pace', pace: 10});
        await t.send({type: 'travel', target: 'cell_3'});
        t.next();
    } else if (step === 1 && t.elapsed - stepAt > 2.2) {
        if (stamina < 0 || stamina >= 95 || speed <= 2.6 || !bool(travel, 'active'))
            return finish(false, `Sprint route did not move and drain stamina: stamina=${stamina.toFixed(2)} speed=${speed.toFixed(2)}`);
        await capture('15-travel-pace.png');
        await t.send({type: 'typing', active: true});
        await t.send({type: 'chat', text: 'The trail feels familiar.', channel: 'ic'});
        t.next();
    } else if (step === 2 && !bool(travel, 'active') && t.cell !== 'cell_3')
        await finish(false, 'Typing or speaking unexpectedly cancelled the deliberate world journey');
    else if (step === 2 && t.cell === 'cell_3' && !bool(travel, 'active')) {
        if (!m.sawMiddle || arr(t.s, 'travelMap').length !== 4) return finish(false, 'Journey skipped an intermediate cell or lost the visited travel index');
        m.restStamina = stamina;
        await driver.page('travel');
        t.next();
    } else if (step === 3 && t.elapsed - stepAt > 1) {
        if (stamina <= m.restStamina || speed > 0.001) return finish(false, 'Completed travel did not stop and recover stamina');
        await capture('16-known-routes.png');
        t.next();
    } else if (step === 4 && t.elapsed - stepAt > 1) {
        await driver.page('local');
        await t.send({type: 'travel', target: 'cell_1'});
        t.next();
    } else if (step === 5 && t.elapsed - stepAt > 1) {
        await t.send({type: 'stop'});
        t.next();
    } else if (step === 6 && t.elapsed - stepAt > 0.6) {
        Object.assign(m, {stopX: t.x, stopY: t.y, stopCell: t.cell});
        if (bool(travel, 'active')) return finish(false, 'Manual stop failed to cancel the travel intention');
        t.next();
    } else if (step === 7 && t.elapsed - stepAt > 1) {
        if (!t.near(m.stopX, m.stopY) || t.cell !== m.stopCell) return finish(false, 'Cancelled route resumed without another command');
        await t.send({type: 'pace', pace: 5});
        await t.send({type: 'travel', target: 'room_1'});
        t.next();
    } else if (step === 8 && bool(travel, 'paused')) {
        if (t.cell !== 'cell_2' || str(travel, 'nextDoor') !== 'link_shelter_a') return finish(false, 'Remembered shelter route failed to approach its closed doorway');
        t.next();
    } else if (step === 9 && t.elapsed - stepAt > 1) {
        if (t.cell !== 'cell_2' || !bool(travel, 'paused')) return finish(false, 'Travel opened a closed door without an explicit action');
        await t.send({type: 'action', target: 'link_shelter_a', action: 'open'});
        t.next();
    } else if (step === 10 && t.cell === 'room_1' && !bool(travel, 'active'))
        await finish(true, 'Sprint consumed stamina; known travel crossed three local cells and recovered at rest; manual stop cancelled; closed door waited for explicit Open');
};

scenarios['atlas'] = async t => {
    const m = memory;
    if (step === 0 && t.elapsed > 1) {
        m.startingCell = t.cell;
        await t.send({type: 'path', x: 33.5, y: 18.5});
        t.next();
    } else if (step === 1 && t.cell !== m.startingCell) {
        const cell = obj(t.s, 'cell');
        if (!t.near(0.5, 18.5) || num(cell, 'width') !== 32 || num(cell, 'height') !== 24)
            return finish(false, 'Authored seam did not preserve its connection point or separate-cell dimensions');
        t.next();
    } else if (step === 2 && t.elapsed - stepAt > 1) {
        if (!t.near(0.5, 18.5)) return finish(false, 'Movement did not stop after crossing the authored seam');
        await capture('13-authored-cell-runtime.png');
        t.next();
    } else if (step === 3 && t.elapsed - stepAt > 1) {
        await t.send({type: 'path', x: 10.5, y: 11.5});
        t.next();
    } else if (step === 4 && t.near(10.5, 11.5)) {
        if (t.cell === 'room_1') return finish(false, 'Closed authored door auto-opened');
        await t.send({type: 'action', target: 'link_inn_entry_a', action: 'open'});
        t.next();
    } else if (step === 5 && t.cell === 'room_1') {
        if (!t.near(7.5, 9.5)) return finish(false, 'Off-map interior arrival differs from the authored anchor');
        await capture('14-authored-interior-runtime.png');
        t.next();
    } else if (step === 6 && t.elapsed - stepAt > 1)
        await finish(t.near(7.5, 9.5), 'Authored atlas imported; boundary crossed and stopped; explicit door entered a separate off-map interior');
};

scenarios['scent-source'] = async t => {
    // Holds the upwind test player in place until the orchestrator finishes the observer and stops it.
    if (t.elapsed > 100) await finish(true, 'Held the upwind source fixture');
};

scenarios['scent'] = async t => {
    let westCue = false;
    for (const cue of arr(obj(t.s, 'senses'), 'scentCues')) {
        const sector = num(cue, 'sector', -1);
        if (Object.keys(cue).length !== 3 || sector < 0 || sector > 7 || 'id' in cue || 'x' in cue || 'name' in cue)
            return finish(false, 'Scent cue leaked more than the coarse anonymous contract');
        westCue ||= sector === 4;
    }
    if (arr(t.s, 'entities').some(e => str(e, 'id') === 'player-bracken')) return finish(false, 'Unseen scent source was included as an exact map entity');
    if (step === 0 && t.elapsed > 2 && westCue) {
        await t.send({type: 'action', action: 'smell'});
        await t.send({type: 'action', action: 'inspect', target: 'player-bracken'});
        await t.send({type: 'action', action: 'inspect', target: 'player-does-not-exist'});
        t.next();
    } else if (step === 1 && t.elapsed - stepAt > 2) {
        if (t.events.some(e => str(e, 'type') === 'inspect')) return finish(false, 'Scent incorrectly permitted inspection');
        const smelled = t.events.some(e => str(e, 'text').includes('roughly west'));
        const denials = t.events.filter(e => str(e, 'text') === 'You cannot inspect someone you cannot see.').length;
        if (!smelled || denials !== 2) return finish(false, 'Live smell narration or hidden-inspection rejection missing');
        await capture('12-upwind-scent.png');
        t.next();
    } else if (step === 2 && t.elapsed - stepAt > 1) {
        await t.send({type: 'wind', value: 'west'});
        t.next();
    } else if (step === 3 && t.elapsed - stepAt > 2) {
        if (westCue) return finish(false, 'Reversed wind failed to remove the distant westward scent cue');
        t.next();
    } else if (step === 4 && t.elapsed - stepAt > 1) {
        await t.send({type: 'wind', value: 'east'});
        t.next();
    } else if (step === 5 && t.elapsed - stepAt > 2)
        await finish(westCue, 'Two-client upwind scent, anonymous sectors, hidden entity/inspection protection, and wind reversal verified');
};

scenarios['movement'] = async t => {
    const posture = str(t.self, 'posture'), facing = num(t.self, 'facing');
    const m = memory;
    if (step === 0 && t.elapsed > 1) {
        await t.send({type: 'face', x: 0, y: 12.5});
        t.next();
    } else if (step === 1) {
        m.sawPartialTurn ||= Math.abs(facing) > 0.1 && Math.abs(facing) < 2.9;
        if (t.elapsed - stepAt > 1.5) {
            if (!m.sawPartialTurn || Math.abs(Math.abs(facing) - Math.PI) > 0.05)
                return finish(false, 'Stationary facing did not replicate intermediate angles and final target');
            await t.send({type: 'action', action: 'sit'});
            t.next();
        }
    } else if (step === 2 && posture === 'sitting') {
        m.startX = t.x;
        await t.send({type: 'move', x: 1, y: 0});
        t.next();
    } else if (step === 3) {
        await t.send({type: 'move', x: 1, y: 0});
        if (t.elapsed - stepAt > 0.2 && t.elapsed - stepAt < 0.5) m.sawSitPause ||= posture === 'rising' && Math.abs(t.x - m.startX) < 0.01;
        if (t.elapsed - stepAt > 1.4) {
            if (!m.sawSitPause || t.x - m.startX < 1 || posture !== 'standing')
                return finish(false, 'Repeated movement did not preserve sit-rise pause and resume walking');
            await t.send({type: 'stop'});
            await t.send({type: 'chat', channel: 'ic', text: '/lay'});
            t.next();
        }
    } else if (step === 4 && posture === 'lying') {
        m.startX = t.x;
        await t.send({type: 'move', x: -1, y: 0});
        t.next();
    } else if (step === 5) {
        m.sawCrouch ||= posture === 'crouching';
        if (t.elapsed - stepAt > 1.65) {
            const travelled = m.startX - t.x;
            if (!m.sawCrouch || travelled < 0.55 || travelled > 1.4) return finish(false, 'Lying movement did not become a slow crouch');
            await t.send({type: 'stop'});
            t.next();
        }
    } else if (step === 6 && t.elapsed - stepAt > 0.7) {
        await capture('10-crouch-movement.png');
        t.next();
    } else if (step === 7 && t.elapsed - stepAt > 1) {
        if (screenshots) {
            await driver.altHover(1250, 410);
            await capture('11-alt-facing-preview.png');
        }
        t.next();
    } else if (step === 8 && t.elapsed - stepAt > 1) {
        if (screenshots) await driver.altRelease();
        await t.send({type: 'chat', channel: 'ic', text: '/stand'});
        t.next();
    } else if (step === 9 && t.elapsed - stepAt > 1)
        await finish(posture === 'standing', 'Gradual replicated facing, sit-rise delay under refreshed input, lying-to-sneak speed, and slash stand verified');
};

scenarios['gallery'] = async t => {
    const pages: Array<[number, string, string]> = [[7, 'character', '02-character-sheet.png'], [11, 'inventory', '03-inventory.png'],
        [15, 'world', '04-world-map.png'], [19, 'settings', '05-settings.png'], [23, 'text-first', '09-text-first-layout.png']];
    if (step === 0 && t.elapsed > 1) {
        await t.send({type: 'chat', channel: 'ic', volume: 'speak', text: '"The road has been quiet since the rain." /sigh "I could use a warm place by the hearth."'});
        ++step;
    } else if (step === 1 && t.elapsed > 5) {
        await capture('01-tavern-local.png', true);
        ++step;
    } else if (step >= 2 && step < 12) {
        const [at, page, file] = pages[Math.floor((step - 2) / 2)];
        if (step % 2 === 0 && t.elapsed > at) {
            await driver.page(page);
            ++step;
        } else if (step % 2 === 1 && t.elapsed > at + 2) {
            await capture(file, true);
            ++step;
        }
    } else if (step === 12 && t.elapsed > 27) await finish(true, 'Gallery captured from the running browser client');
};

scenarios['network'] = async t => {
    if (step === 0 && t.elapsed > 2) {
        await t.send({type: 'move', x: 1, y: 0});
        ++step;
    } else if (step === 1 && t.elapsed > 2.5) {
        await t.send({type: 'move', x: 0, y: 0});
        ++step;
    } else if (step === 2 && t.elapsed > 4) {
        await t.send({type: 'chat', channel: 'ic', volume: 'speak', text: '"I followed the river all the way here, and now the hearth feels like home." /sigh'});
        ++step;
    } else if (step === 3 && t.elapsed > 6) {
        await t.send({type: 'chat', channel: 'ooc', volume: 'speak', text: 'Network test: both clients connected.'});
        ++step;
    } else if (step === 4 && t.elapsed > 8) {
        await t.send({type: 'face', x: 1, y: 1});
        ++step;
    } else if (step === 5 && t.elapsed > 9) {
        let prose = 'Long-form network proof: ';
        while (prose.length < 16000) prose += 'Rain writes a patient rhythm along the rafters while the wolves share their stories. ';
        await t.send({type: 'chat', channel: 'ic', text: prose.slice(0, 16000)});
        ++step;
    } else if (step === 6 && t.elapsed > 11) {
        await capture(`network-${role}.png`);
        ++step;
    } else if (step === 7 && t.elapsed > 15) {
        const expected = role === 'ash' ? 'Bracken' : 'Ash';
        const from = t.events.filter(e => str(e, 'speaker') === expected);
        const sawOther = from.some(e => str(e, 'channel') === 'ic'), sawOoc = from.some(e => str(e, 'channel') === 'ooc');
        const long = from.some(e => str(e, 'channel') === 'ic' && str(e, 'text').length >= 16000);
        const moved = Math.abs(finalX - initialX) > 0.1;
        const motionRate = state.motionFrames > 100 && state.motionFrames > state.snapshotCount * 3;
        const b = (v: boolean) => (v ? 1 : 0);
        await finish(moved && sawOther && sawOoc && long && motionRate,
            `moved=${b(moved)} otherIC=${b(sawOther)} OOC=${b(sawOoc)} longPost=${b(long)} motion20Hz=${b(motionRate)}`);
    }
};

scenarios['walkthrough'] = async t => {
    if (step === 0 && t.elapsed > 1) {
        await t.send({type: 'path', x: 22.5, y: 6.5});
        t.next();
    } else if (step === 1 && t.near(22.5, 6.5)) {
        await t.send({type: 'action', target: 'door_pantry', action: 'open'});
        t.next();
    } else if (step === 2 && t.elapsed - stepAt > 0.5) {
        await t.send({type: 'path', x: 27.5, y: 5.5});
        t.next();
    } else if (step === 3 && t.near(27.5, 5.5)) {
        await driver.page('world');
        t.next();
    } else if (step === 4 && t.elapsed - stepAt > 2) {
        await capture('06-visible-vertical-world.png', true);
        t.next();
    } else if (step === 5 && t.elapsed - stepAt > 1) {
        await t.send({type: 'action', target: 'stairs_up', action: 'enter'});
        await driver.page('local');
        t.next();
    } else if (step === 6 && t.cell === 'loft' && t.elapsed - stepAt > 2) {
        await capture('07-quiet-loft.png', true);
        t.next();
    } else if (step === 7 && t.elapsed - stepAt > 1) {
        await t.send({type: 'action', target: 'stairs_down', action: 'enter'});
        t.next();
    } else if (step === 8 && t.cell === 'tavern') {
        await t.send({type: 'path', x: 16.5, y: 22.5});
        t.next();
    } else if (step === 9 && t.near(16.5, 22.5)) {
        await t.send({type: 'action', target: 'door_main', action: 'open'});
        t.next();
    } else if (step === 10 && t.cell === 'exterior') {
        await t.send({type: 'path', x: 16.5, y: 9.5});
        t.next();
    } else if (step === 11 && t.near(16.5, 9.5) && t.elapsed - stepAt > 2) {
        await capture('08-rain-in-juniper-yard.png', true);
        t.next();
    } else if (step === 12 && t.elapsed - stepAt > 2)
        await finish(true, 'Pathing, explicit same-cell door, visible vertical map, loft round-trip, yard transition and rain captured');
};

for (const name of ['dialogue-live', 'dialogue-recall'])
    scenarios[name] = async t => {
        // One ordinary IC post: never a direct call into the NPC, an injected reply, or a way around perception.
        const m = memory;
        if (step === 0 && t.elapsed > 1) {
            if (t.cell !== 'tavern') return finish(false, 'Live dialogue fixture must start in the tavern');
            const inventory = t.s.inventory;
            if (!Array.isArray(inventory) || typeof t.self.socialXp !== 'number' || typeof t.self.socialLevel !== 'number')
                return finish(false, 'Live dialogue fixture lacks authoritative inventory/XP fields');
            dialogueEvidence = {inventoryBefore: inventory, socialXpBefore: t.self.socialXp, socialLevelBefore: t.self.socialLevel,
                activeTurnsBefore: num(obj(t.s, 'memory'), 'activeTurns'), providerLabel: str(t.s, 'dialogueProvider')};
            m.inventory = JSON.stringify(inventory);
            await t.send({type: 'path', x: 11.5, y: 6.5});
            t.next();
        } else if (step === 1 && t.near(11.5, 6.5)) {
            // Rowan must be within three tiles of the hearth fixture, well inside clear hearing, before the post.
            const rowan = arr(t.s, 'entities').some(e => str(e, 'id') === 'npc_keeper' && (num(e, 'x') - t.x) ** 2 + (num(e, 'y') - t.y) ** 2 <= 9);
            if (!rowan) {
                if (t.elapsed - stepAt > 20) return finish(false, 'Rowan was not at the clear-hearing hearth fixture');
                return;
            }
            m.baseline = Math.max(0, ...t.events.map(e => num(e, 'sequence')));
            const prompt = scenario === 'dialogue-live'
                ? 'Rowan, remember that my sister is called Willow and I promised to bring her blue river stones.'
                : 'Rowan, what is my sister called, and what did I promise to bring her?';
            Object.assign(dialogueEvidence, {prompt, postsSent: 1, baselineSequence: m.baseline});
            await t.send({type: 'move', x: 0, y: 0});
            await t.send({type: 'chat', channel: 'ic', volume: 'speak', requestId: scenario, text: prompt});
            t.next();
        } else if (step === 2) {
            for (const e of t.events) {
                if (str(e, 'type') === 'error' && str(e, 'requestId') === scenario) return finish(false, 'Live dialogue post rejected by the server');
                if (str(e, 'type') === 'chatAccepted' && str(e, 'requestId') === scenario) dialogueEvidence.postAccepted = true;
                if (step === 2 && str(e, 'type') === 'roleplay' && str(e, 'speaker') === 'Rowan' && str(e, 'channel') === 'ic' &&
                    num(e, 'sequence') > m.baseline && str(e, 'text')) {
                    // The external bridge's audit can hash this exact text to tell generation from the authored fallback.
                    Object.assign(dialogueEvidence, {replyText: str(e, 'text'), replySequence: num(e, 'sequence'), replySeconds: t.elapsed - stepAt});
                    t.next();
                }
            }
            if (step === 2 && t.elapsed - stepAt > 15) return finish(false, 'No Rowan speech event arrived within fifteen seconds');
        } else if (step === 3 && t.elapsed - stepAt > 0.5) {
            const xp = num(t.self, 'socialXp', -1), level = num(t.self, 'socialLevel', -1), turns = num(obj(t.s, 'memory'), 'activeTurns');
            const inventoryUnchanged = JSON.stringify(t.s.inventory) === m.inventory;
            const xpUnchanged = xp === dialogueEvidence.socialXpBefore && level === dialogueEvidence.socialLevelBefore;
            const memoryAdvanced = turns >= dialogueEvidence.activeTurnsBefore + 2;
            Object.assign(dialogueEvidence, {inventoryUnchanged, xpUnchanged, socialXpAfter: xp, socialLevelAfter: level, activeTurnsAfter: turns,
                memoryAdvanced, inventoryAfter: t.s.inventory});
            if (!memoryAdvanced && t.elapsed - stepAt < 5) return;
            const accepted = !!dialogueEvidence.postAccepted;
            const b = (v: boolean) => (v ? 1 : 0);
            await finish(accepted && inventoryUnchanged && xpUnchanged && memoryAdvanced,
                `Rowan reply received; accepted=${b(accepted)} inventoryUnchanged=${b(inventoryUnchanged)} xpUnchanged=${b(xpUnchanged)} ` +
                `memoryAdvanced=${b(memoryAdvanced)}. External provider audit must verify generated provenance.`);
        }
    };

scenarios['persist-write'] = async t => {
    if (step === 0 && t.elapsed > 1) {
        await t.send({type: 'color', index: 21});
        await t.send({type: 'chat', channel: 'ic', text: '/me watches the rain with patient curiosity'});
        await t.send({type: 'path', x: 18.5, y: 12.5});
        t.next();
    } else if (step === 1 && t.near(18.5, 12.5) && t.elapsed - stepAt > 1) {
        await t.send({type: 'action', target: 'npc_keeper', action: 'talk'});
        t.next();
    } else if (step === 2 && t.elapsed - stepAt > 6) await finish(true, 'Authored character state and NPC conversation persisted');
};

for (const name of ['persist-read', 'persist-aged'])
    scenarios[name] = async t => {
        if (t.elapsed <= 2) return;
        const mem = obj(t.s, 'memory');
        const active = num(mem, 'activeTurns'), summaries = num(mem, 'summaries');
        const memoryOk = scenario === 'persist-aged' ? summaries >= 1 && active === 0 : active >= 2;
        const state = str(t.self, 'state'), color = num(t.self, 'color');
        const pass = state === 'watches the rain with patient curiosity' && color === 21 && t.near(18.5, 12.5) && memoryOk;
        await finish(pass, `color=${color} position=${t.x.toFixed(2)},${t.y.toFixed(2)} active=${active} summaries=${summaries} state=${state}`);
    };

// ------------------------------------------------------------------ Characters (the front door)

let createIndex = 0, capturedStep = -1, at = 0;
async function characters(now: number) {
    const lobby = state.lobby, snapshot = state.snapshot, self = obj(snapshot, 'self');
    const roster: J[] | null = lobby && Array.isArray(lobby.characters) ? lobby.characters : null;
    const send = (c: J) => driver.send(c);
    const auth = (register: boolean, user: string, correct = true) =>
        // A synthetic, public fixture credential: never anyone's password.
        send({type: register ? 'auth_register' : 'auth_login', username: user, password: correct ? 'RATW-fixture-only-2026!' : 'Incorrect-fixture-2026!'});
    const enter = (id: string) => send({type: 'character_enter', id});
    const create = (i: number, invalid = false) => send({type: 'character_create', name: Names[i], age: invalid ? 100 : Ages[i],
        commandId: `fixture-create-${role}-${i}-${invalid ? 'invalid' : 'valid'}`, appearance: choice(i)});
    const next = (check = '') => {
        if (check) checks.push(check);
        ++step;
        at = now;
    };
    const shotBefore = async (file: string) => {
        if (!screenshots || capturedStep === step) return false;
        await capture(file);
        capturedStep = step;
        at = now;
        return true;
    };
    const ok = bool(lobby, 'ok');
    if (now - started > 100) return finish(false, 'Character flow timed out');
    if (now - at < 0.8) return;
    if (scenario !== 'characters-create') {
        if (step === 0) {
            await auth(false, role === 'birch' ? 'portrait_b' : 'portrait_a');
            next();
        } else if (step === 1 && roster && roster.length > 0 && ok) {
            firstId = str(roster[0], 'id');
            await enter(firstId);
            next('Saved owned roster restored after process restart');
        } else if (step === 2 && scenario === 'characters-duplicate') {
            if (self || ok) return finish(false, 'Duplicate active ownership was accepted');
            return finish(true, 'A second session cannot occupy an already-connected character');
        } else if (step === 2 && self) {
            if (str(obj(self, 'appearance'), 'species') !== (role === 'birch' ? 'arctic' : 'timber')) return finish(false, 'Owned appearance was lost on restart');
            next('Persisted avatar restored in authoritative self snapshot');
        } else if (step === 3 && scenario === 'characters-hold') {
            if (now - at > 48) return finish(true, 'Persisted account character held for independent peer inspections');
        } else if (step === 3) {
            const other = arr(snapshot, 'entities').find(e => !bool(e, 'npc') && str(e, 'id') !== str(self, 'id'));
            if (other) {
                await send({type: 'action', action: 'inspect', target: str(other, 'id')});
                next("Independent client sees the other player's public appearance");
            }
        } else if (step === 4) {
            const inspect = state.events.find(e => str(e, 'type') === 'inspect');
            if (inspect) {
                if (!obj(inspect, 'appearance') || 'age' in inspect) return finish(false, 'Inspection appearance missing or private exact age leaked');
                await capture('39-other-player-inspection.png');
                next('Inspection delivers appearance + public life stage without exact age');
            }
        } else if (step === 5) return finish(true, 'Network login, persisted avatar, and other-player inspection passed');
        return;
    }
    if (step === 0) {
        if (await shotBefore('33-character-login.png')) return;
        await send({type: 'hello', identity: 'ash'});
        await send({type: 'move'});
        next();
    } else if (step === 1) {
        if (self) return finish(false, 'Unauthenticated command obtained a world entity');
        await auth(true, 'portrait_a');
        next('Default login rejects unauthenticated development identity and movement');
    } else if (step === 2 && roster && roster.length === 0 && ok) {
        await driver.draft('Lark', Ages[0], choice(0));
        next('Local account registration persisted before empty roster acknowledgement');
    } else if (step === 3) {
        if (await shotBefore('34-character-creator.png')) return;
        await create(0, true);
        next();
    } else if (step === 4) {
        if (!roster || roster.length || ok) return finish(false, 'Invalid creation was accepted');
        await create(createIndex);
        next('Invalid age is rejected without creating a character');
    } else if (step === 5 && roster && roster.length === createIndex + 1 && ok) {
        if (createIndex === 0) firstId = str(roster[0], 'id');
        ++createIndex;
        if (createIndex < 6) {
            await create(createIndex);
            at = now;
        } else {
            await capture('35-character-roster.png');
            await create(0);
            next('All five species and all four age stages saved across six owned slots');
        }
    } else if (step === 6) {
        if (!roster || roster.length !== 6 || !ok) return finish(false, 'Idempotent creation did not replay safely');
        await send({type: 'character_create', name: 'Overflow', age: 18, appearance: choice(0)});
        next('Retrying a creation request does not duplicate the character');
    } else if (step === 7) {
        if (!roster || roster.length !== 6 || ok) return finish(false, 'Slot limit not enforced');
        await enter(firstId);
        next('Server rejects a seventh character');
    } else if (step === 8 && self) {
        if (str(obj(self, 'appearance'), 'species') !== 'timber' || num(self, 'age') !== Ages[0])
            return finish(false, 'Selected character mismatched authoritative appearance/age');
        await driver.page('character');
        next('Character selection enters the correct owned character');
    } else if (step === 9) {
        if (await shotBefore('36-player-character-card.png')) return;
        await send({type: 'character_leave'});
        next();
    } else if (step === 10) {
        if (self || !roster || roster.length !== 6) return finish(false, 'Leaving did not clear gameplay and restore roster');
        await driver.draft('Frost', Ages[2], choice(2));
        next('Leave returns to owned roster and clears prior gameplay snapshot');
    } else if (step === 11) {
        if (await shotBefore('37-old-arctic-preview.png')) return;
        await send({type: 'auth_logout'});
        next();
    } else if (step === 12) {
        await auth(true, 'portrait_b');
        next();
    } else if (step === 13 && ok) {
        await enter(firstId);
        next();
    } else if (step === 14) {
        if (self || ok) return finish(false, 'Other-account character ownership bypass');
        await create(2);
        next('Another account cannot enter a character it does not own');
    } else if (step === 15 && roster && roster.length === 1) {
        await send({type: 'auth_logout'});
        next();
    } else if (step === 16) {
        await auth(false, 'portrait_a', false);
        next();
    } else if (step === 17) {
        if (ok || str(lobby, 'stage') !== 'login') return finish(false, 'Wrong password accepted');
        await auth(false, 'portrait_a');
        next('Wrong password rejected without revealing an owned roster');
    } else if (step === 18 && roster && roster.length === 6 && ok) {
        await driver.draft('Sorrel', Ages[1], choice(1));
        next('Correct password restores the same six owned characters');
    } else if (step === 19) {
        await capture('38-maned-wolf-preview.png');
        next();
    } else if (step === 20) return finish(true, 'Browser login, registration, live creator, six-slot roster, ownership, replay safety, and character-card flow passed');
}

// ------------------------------------------------------------------ The loop

let begun = -1;
try {
    if (!scenarios[scenario] && !scenario.startsWith('characters-')) await finish(false, 'Unknown scenario');
    while (!finished) {
        await sleep(args.has('browser') ? 100 : 50);
        state = await driver.read();
        const now = Date.now() / 1000;
        if (scenario.startsWith('characters-')) {
            if (at === 0) at = now;
            await characters(now);
            continue;
        }
        const self = obj(state.snapshot, 'self');
        if (!self) {
            if (now - started > 100) await finish(false, 'No authoritative snapshot');
            continue;
        }
        if (begun < 0) {
            begun = now;
            initialX = num(self, 'x');
            console.log(`RATW_SCENARIO_READY ${role}`);
        }
        finalX = num(self, 'x');
        maxEntities = Math.max(maxEntities, arr(state.snapshot, 'entities').length);
        const elapsed = now - begun;
        if (elapsed > 120) {
            await finish(false, `Timed out at step ${step}`);
            break;
        }
        const x = finalX, y = num(self, 'y');
        const tick: Tick = {
            s: state.snapshot, self, elapsed, cell: str(self, 'cell'), x, y, events: state.events,
            send: c => driver.send(c),
            next: () => {
                ++step;
                stepAt = elapsed;
            },
            near: (tx, ty) => Math.abs(x - tx) < 0.3 && Math.abs(y - ty) < 0.3,
            check: async (ok, detail) => {
                if (!ok) await finish(false, detail);
                return ok;
            },
        };
        await scenarios[scenario](tick);
    }
} catch (error) {
    await finish(false, `Scenario error: ${(error as Error).stack ?? error}`);
} finally {
    await driver.close();
}
process.exit(finished?.passed ? 0 : 1);
