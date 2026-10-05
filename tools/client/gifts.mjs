// Gifts in a real fight in the real page (Docs/Design/43-gifts.md): two players (a browser each) duel once for each
// family showcased, using their Gift's abilities through the fight screen's Gift row as a player would, with screenshots
// of the arena as each goes off. Screenshots go to artifacts/screenshots/gifts/.
//
//   node tools/client/gifts.mjs [OUT]          (starts its own scratch server of Greyfen: tools/scratch.sh town)
import {spawn} from 'node:child_process';
import {mkdirSync} from 'node:fs';
import {dirname, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/gifts`;
mkdirSync(OUT, {recursive: true});
const sleep = ms => new Promise(r => setTimeout(r, ms));
const server = spawn('bash', [`${root}/tools/scratch.sh`, 'town', '--dev-identity', '--dev-tools', '--for', '900'],
    {stdio: ['ignore', 'pipe', 'pipe'], detached: true, env: {...process.env, RATW_AI: 'off'}});
let log = '';
server.stdout.on('data', d => { log += d; });
server.stderr.on('data', d => { log += d; });
let port = 0;
for (let i = 0; i < 600 && !port; ++i) {
    port = Number(/listening on port (\d+)/.exec(log)?.[1] ?? 0);
    await sleep(500);
}
if (!port) throw new Error('The scratch server did not start:\n' + log.slice(-2000));
const browsers = [];
const results = [];
const errors = [];
const S = 'window.ratw.game().state';
try {
    const open = async who => {
        const browser = await Browser.launch({width: 1600, height: 1000});
        browsers.push(browser);
        const page = await browser.open(`http://127.0.0.1:${port}/?identity=${who}`);
        await page.waitFor(`${S}.snapshot?.self?.id`, 20);
        return page;
    };
    let round = 0;

    // One duel: Ash has `gift` (Quickened or not); `play` uses it on Ash's turns, screenshot by screenshot.
    const duel = async (name, gift, quickened, play) => {
        // Fresh wolves each time (after a fight one is still finding one's feet a while).
        const ash = await open(`ash${++round}`), bo = await open(`bo${round}`);
        const ashId = await ash.evaluate(`${S}.selfId`), boId = await bo.evaluate(`${S}.selfId`);
        await sleep(600);
        await ash.evaluate(`${S}.send({type: 'gift', gift: ${JSON.stringify(gift)}, quickened: ${quickened}})`);
        await sleep(300);
        await ash.evaluate(`${S}.sendAction('challenge', ${JSON.stringify(boId)}, {terms: 'yield'})`);
        try {
            await bo.waitFor(`${S}.challenge`, 10);
        } catch (error) {
            const where = async p => p.evaluate(`JSON.stringify([${S}.snapshot?.self?.x, ${S}.snapshot?.self?.y, ${S}.cellId, ${S}.posts.slice(-4).map(p => p.text)])`);
            throw new Error(`No challenge came: Ash ${await where(ash)}; Bo ${await where(bo)}`);
        }
        await bo.evaluate(`${S}.sendAction('accept')`);
        await ash.waitFor(`${S}.battle`, 10);
        await sleep(400);
        // Take one's ground: both ready.
        for (const p of [ash, bo]) await p.evaluate(`${S}.battle?.placing && ${S}.sendBattle('ready', {on: true})`);
        await ash.waitFor(`${S}.battle && !${S}.battle.placing`, 40);
        const turnOf = async () => {
            for (let i = 0; i < 120; ++i) {
                // Bo only waits: his turns pass.
                await bo.evaluate(`(() => { const b = ${S}.battle; if (b && b.turn === ${JSON.stringify(boId)}) ${S}.sendBattle('wait'); })()`);
                if (await ash.evaluate(`${S}.battle?.turn === ${JSON.stringify(ashId)} && !${S}.battle.casting`)) return true;
                if (await ash.evaluate(`!${S}.battle || ${S}.battle.over`)) return false;
                await sleep(250);
            }
            return false;
        };
        const ctx = {
            ash, bo, ashId, boId, turnOf,
            shot: async file => ash.screenshot(`${OUT}/${name}-${file}.png`),
            // A Gift used as a player would: its button, then a click on the arena.
            use: async (ability, aim) => ash.evaluate(`(() => {
                const s = ${S}, b = s.battle, me = b.fighters.find(f => f.id === s.selfId), foe = b.fighters.find(f => f.side !== me.side);
                const g = b.gifts.find(o => o.id === ${JSON.stringify(ability)});
                if (!g) return 'no such Gift';
                if (!g.ready) return 'not ready: ' + g.why;
                s.useGift(g.id);
                const a = ${JSON.stringify(aim ?? null)};
                if (a === 'foe') s.arenaClick(foe.x, foe.y);
                else if (a === 'self') s.arenaClick(me.x, me.y);
                else if (Array.isArray(a) && Array.isArray(a[0])) { for (const [dx, dy] of a) s.arenaClick(me.x + dx, me.y + dy); s.castShape(); }
                else if (Array.isArray(a)) s.arenaClick(me.x + a[0], me.y + a[1]);
                else if (a === 'towardFoe') s.arenaClick(foe.x, foe.y);
                return 'used ' + g.name;
            })()`),
            hover: async (dx, dy) => ash.evaluate(`(() => { const s = ${S}, b = s.battle, me = b.fighters.find(f => f.id === s.selfId);
                s.hover = [s.mapOrigin[0] + (me.x + ${dx} + 0.5) * s.tileSize, s.mapOrigin[1] + (me.y + ${dy} + 0.5) * s.tileSize]; })()`),
            aimAt: async ability => ash.evaluate(`${S}.useGift(${JSON.stringify(ability)})`),
            end: async () => { await ash.evaluate(`${S}.battle?.turn === ${JSON.stringify(ashId)} && ${S}.sendBattle('wait')`); await sleep(400); },
            last: async () => ash.evaluate(`${S}.battle?.log.slice(-4).map(l => l.text).join(' | ')`),
            // Bo stands a few tiles east of Ash (the server's positioning: he moves there on his turn).
            closeIn: async d => {
                for (let i = 0; i < 80; ++i) {
                    const done = await bo.evaluate(`(() => { const s = ${S}, b = s.battle; if (!b || b.over) return true;
                        const me = b.fighters.find(f => f.id === s.selfId), foe = b.fighters.find(f => f.side !== me.side);
                        if (Math.max(Math.abs(me.x - foe.x), Math.abs(me.y - foe.y)) <= ${d}) return true;
                        if (b.turn !== s.selfId || b.moved) return false;
                        let best = null; const ap = (x, y) => Math.max(Math.abs(x - foe.x), Math.abs(y - foe.y));
                        for (const [x, y] of b.reach) if (ap(x, y) >= ${d} && (!best || ap(x, y) < ap(best[0], best[1]))) best = [x, y];
                        if (best) s.arenaClick(best[0], best[1]); s.sendBattle('wait'); return false; })()`);
                    if (done) return;
                    await ash.evaluate(`(() => { const b = ${S}.battle; if (b && b.turn === ${JSON.stringify(ashId)}) ${S}.sendBattle('wait'); })()`);
                    await sleep(250);
                }
            },
        };
        try {
            await play(ctx);
        } finally {
            await ash.evaluate(`${S}.battle && !${S}.battle.over && ${S}.sendBattle('yield')`);
            await bo.evaluate(`${S}.battle && !${S}.battle.over && ${S}.sendBattle('yield')`);
            for (let i = 0; i < 40 && await ash.evaluate(`!!${S}.battle`); ++i) {
                await bo.evaluate(`${S}.battle?.yieldBy && ${S}.sendBattle('spare')`);
                await ash.evaluate(`${S}.battle?.yieldBy && ${S}.sendBattle('spare')`);
                await sleep(300);
            }
            await sleep(1500);
            for (const p of [ash, bo]) errors.push(...p.console.filter(l => l.startsWith('EXCEPTION')));
            browsers.pop()?.close();
            browsers.pop()?.close();
        }
    };

    await duel('fire', 'fire', true, async c => {
        await c.closeIn(3);
        if (!await c.turnOf()) return;
        await c.shot('1-gift-row');
        await c.aimAt('wall_of_fire');
        for (const [dx, dy] of [[1, -2], [1, -1], [1, 0], [1, 1], [1, 2]]) await c.ash.evaluate(`(() => { const s = ${S}, me = s.battle.fighters.find(f => f.id === s.selfId); s.arenaClick(me.x + ${dx}, me.y + ${dy}); })()`);
        await c.hover(1, 3);
        await sleep(200);
        await c.shot('2-painting-wall');
        await c.ash.evaluate(`${S}.castShape()`);
        await sleep(600);
        await c.shot('3-wall-gathering');
        await sleep(3500);
        await c.shot('4-wall-of-fire');
        results.push(`fire: ${await c.last()}`);
        await c.end();
        if (!await c.turnOf()) return;
        await c.aimAt('heat_lance');
        await c.ash.evaluate(`(() => { const s = ${S}, b = s.battle, foe = b.fighters.find(f => f.id === ${JSON.stringify(c.boId)});
            s.hover = [s.mapOrigin[0] + (foe.x + 0.5) * s.tileSize, s.mapOrigin[1] + (foe.y + 0.5) * s.tileSize]; })()`);
        await sleep(200);
        await c.shot('5-aiming-lance');
        await c.ash.evaluate(`(() => { const s = ${S}, foe = s.battle.fighters.find(f => f.id === ${JSON.stringify(c.boId)}); s.arenaClick(foe.x, foe.y); })()`);
        await sleep(3000);
        await c.shot('6-heat-lance');
        results.push(`fire: ${await c.last()}`);
    });

    await duel('earth', 'earth', true, async c => {
        await c.closeIn(4);
        if (!await c.turnOf()) return;
        results.push(`earth armour: ${await c.use('stone_armor')}`);
        await sleep(400);
        results.push(`earth wall: ${await c.use('stone_wall', [[2, -1], [2, 0], [2, 1]])}`);
        await sleep(3000);
        await c.shot('1-stone-wall');
        results.push(`earth: ${await c.last()}`);
    });

    await duel('water', 'water', true, async c => {
        await c.ash.evaluate(`${S}.send({type: 'grant', item: 'water'})`);   // A skin of water (the Tell).
        await sleep(800);
        await c.closeIn(3);
        if (!await c.turnOf()) return;
        results.push(`water flood: ${await c.use('flood', 'towardFoe')}`);
        await sleep(3500);
        await c.shot('1-flood');
        await c.end();
        if (!await c.turnOf()) return;
        results.push(`water freeze: ${await c.use('freeze', 'foe')}`);
        await sleep(600);
        await c.shot('2-frozen');
        results.push(`water: ${await c.last()}`);
    });

    await duel('gravity', 'gravity', true, async c => {
        await c.closeIn(3);
        if (!await c.turnOf()) return;
        results.push(`gravity well: ${await c.use('well', 'towardFoe')}`);
        await sleep(3500);
        await c.shot('1-well');
        await c.end();
        if (!await c.turnOf()) return;
        results.push(`gravity slam: ${await c.use('slam', 'foe')}`);
        await sleep(600);
        await c.shot('2-slam-held');
        results.push(`gravity: ${await c.last()}`);
    });

    await duel('wind', 'wind', true, async c => {
        await c.closeIn(2);
        if (!await c.turnOf()) return;
        results.push(`wind gust: ${await c.use('battering_gust', 'towardFoe')}`);
        await sleep(500);
        await c.shot('1-gust');
        results.push(`wind: ${await c.last()}`);
    });

    await duel('seer', 'seer', false, async c => {
        await c.closeIn(3);
        if (!await c.turnOf()) return;
        results.push(`seer read: ${await c.use('read_the_line', 'foe')}`);
        await sleep(600);
        await c.shot('1-gifted-seer');
        results.push(`seer: ${await c.last()}`);
    });

    await duel('blinker', 'blinker', true, async c => {
        await c.closeIn(4);
        if (!await c.turnOf()) return;
        results.push(`blink strike: ${await c.use('blink_strike', 'foe')}`);
        await sleep(400);
        await c.shot('1-blink-strike');
        results.push(`blinker: ${await c.last()}`);
    });

    await duel('sound', 'sound', true, async c => {
        await c.closeIn(3);
        if (!await c.turnOf()) return;
        results.push(`shatterhowl: ${await c.use('shatterhowl', 'towardFoe')}`);
        await sleep(600);
        await c.shot('1-howl-gathering');
        await sleep(2500);
        await c.shot('2-shatterhowl');
        results.push(`sound: ${await c.last()}`);
    });

    // At work (doc 43): a Gifted Fire wolf walks to a smith, baker or innkeeper near by and lends them Forge Heat.
    {
        const smith = await open('smithwolf');
        await smith.evaluate(`${S}.send({type: 'gift', gift: 'fire', quickened: false})`);
        await sleep(600);
        const maker = await smith.evaluate(`(() => { const s = ${S};
            const list = [...s.entities.values()].filter(e => /smith|bak|inn|potter|forge|cook|kiln/i.test((e.name || '') + ' ' + (e.label || '') + ' ' + (e.description || '')));
            const e = list[0]; return e ? {id: e.id, x: e.x, y: e.y, name: e.name || e.label} : null; })()`);
        if (maker) {
            await smith.evaluate(`${S}.send({type: 'path', x: ${maker.x + 1}, y: ${maker.y}})`);
            for (let i = 0; i < 60; ++i) {
                const d = await smith.evaluate(`(() => { const s = ${S}, me = s.entities.get(s.selfId), m = s.entities.get(${JSON.stringify(maker.id)}); return me && m ? Math.hypot(me.x - m.x, me.y - m.y) : 99; })()`);
                if (d < 3) break;
                await sleep(500);
            }
            await smith.evaluate(`${S}.send({type: 'giftwork', ability: 'forge_heat'})`);
            await sleep(800);
            results.push(`forge heat at ${maker.name}: ${await smith.evaluate(`${S}.posts.slice(-1).map(p => p.text).join('')`)}`);
        } else results.push('forge heat: no smith, baker or innkeeper in sight');
        await smith.evaluate(`${S}.activate({rect: {left: 0, top: 0, right: 0, bottom: 0}, action: 'status', target: ''})`);
        await sleep(600);
        await smith.screenshot(`${OUT}/work-1-character-sheet.png`);
        errors.push(...smith.console.filter(l => l.startsWith('EXCEPTION')));
    }
    console.log(results.join('\n'));
    console.log(errors.length ? `PAGE ERRORS:\n${errors.join('\n')}` : 'No page errors.');
} finally {
    for (const b of browsers) b.close();
    try { process.kill(-server.pid); } catch { /* gone */ }
}
