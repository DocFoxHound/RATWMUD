// A real fight in the real page (Docs/Design/33-combat.md): three players (a browser each), a challenge, turns played
// by clicking, an onlooker who watches, the end and the fade back. Screenshots go to artifacts/screenshots/fight/.
//
//   node tools/client/fight.mjs [OUT]          (the server from build-core and Client/dist, as tools/play.sh builds them)
import {spawn} from 'node:child_process';
import {mkdirSync, mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {Browser} from './browser.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const tmp = mkdtempSync(join(tmpdir(), 'ratw-fight-'));
const OUT = process.argv[2] ?? `${root}/artifacts/screenshots/fight`;
const binary = process.env.RATW_SERVER ?? `${root}/build-core/ratw_server`;
mkdirSync(OUT, {recursive: true});
const port = 7900 + Math.floor(Math.random() * 90);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const server = spawn(binary, ['--port', String(port), '--web', `${root}/Client/dist`, '--save', `${tmp}/fight-save.json`,
    '--dev-identity', '--dev-tools'], {stdio: ['ignore', 'ignore', 'pipe']});
let log = '';
server.stderr.on('data', d => { log += d; });
await sleep(1500);
const browsers = [];
const results = [];
try {
    const open = async who => {
        const browser = await Browser.launch({width: 1600, height: 1000});
        browsers.push(browser);
        const page = await browser.open(`http://127.0.0.1:${port}/?identity=${who}`);
        await page.waitFor('window.ratw.game()?.state.snapshot?.self?.id', 20);
        return page;
    };
    const ash = await open('ash'), bo = await open('bo'), cy = await open('cy');
    const S = 'window.ratw.game().state';
    const id = async p => p.evaluate(`${S}.selfId`);
    const ashId = await id(ash), boId = await id(bo);
    await sleep(800);
    await ash.evaluate(`${S}.sendAction('challenge', ${JSON.stringify(boId)})`);
    await bo.waitFor(`${S}.challenge`, 10);
    await bo.screenshot(`${OUT}/1-challenge.png`);
    await bo.evaluate(`${S}.sendAction('accept')`);
    await ash.waitFor(`${S}.battle`, 10);
    await cy.waitFor(`${S}.fights.length > 0`, 10);
    await sleep(500);
    await ash.screenshot(`${OUT}/2-arena-ash.png`);
    await cy.screenshot(`${OUT}/3-onlooker-cy.png`);
    results.push(`arena ${JSON.stringify(await ash.evaluate(`${S}.battle.arena`))}, turn ${await ash.evaluate(`${S}.battle.turn`)}`);
    // Cy watches.
    await cy.evaluate(`${S}.sendBattle('observe', {battle: ${S}.fights[0].id})`);
    await cy.waitFor(`${S}.battle && ${S}.battle.observer`, 10);
    // Play: whoever's turn it is clicks toward the foe, then on it, then ends the turn.
    const playOnce = async (page, me) => page.evaluate(`(() => {
        const s = ${S}, b = s.battle;
        if (!b || b.over || b.turn !== ${JSON.stringify(me)}) return 'not mine';
        const self = b.fighters.find(f => f.id === ${JSON.stringify(me)}), foe = b.fighters.find(f => f.side !== self.side && f.status === 'fighting');
        if (!foe) { s.sendBattle('wait'); return 'no foe'; }
        const apart = (x, y) => Math.max(Math.abs(x - foe.x), Math.abs(y - foe.y));
        if (apart(self.x, self.y) > 1 && !b.moved) {
            let best = null;
            for (const [x, y] of b.reach) if (!best || apart(x, y) < apart(best[0], best[1])) best = [x, y];
            if (best) { s.arenaClick(best[0], best[1]); return 'moved'; }
        }
        if (apart(self.x, self.y) === 1 && !b.acted) { s.arenaClick(foe.x, foe.y); return 'bit'; }
        s.sendBattle('wait');
        return 'waited';
    })()`);
    let shot = false;
    for (let i = 0; i < 400; ++i) {
        const over = await ash.evaluate(`!${S}.battle || ${S}.battle.over`);
        if (over) break;
        const a = await playOnce(ash, ashId), b = await playOnce(bo, boId);
        if (!shot && (a === 'bit' || b === 'bit')) {
            await sleep(350);
            await ash.screenshot(`${OUT}/4-exchange-ash.png`);
            await cy.screenshot(`${OUT}/5-watching-cy.png`);
            shot = true;
        }
        await sleep(250);
    }
    await sleep(400);
    await ash.screenshot(`${OUT}/6-over-ash.png`);
    results.push(`banner: ${await ash.evaluate(`${S}.battle?.banner ?? '(gone)'`)}`);
    await ash.waitFor(`!${S}.battle`, 15);
    await sleep(1200);
    await ash.screenshot(`${OUT}/7-back-in-the-world-ash.png`);
    await bo.screenshot(`${OUT}/8-back-in-the-world-bo.png`);
    const self = p => p.evaluate(`JSON.stringify({health: ${S}.snapshot.self.health, downedLeft: ${S}.snapshot.self.downedLeft ?? 0, mode: ${S}.movementMode})`);
    results.push(`ash ${await self(ash)} bo ${await self(bo)} cy watching ${await cy.evaluate(`!!${S}.battle`)}`);
    const errors = [...ash.console, ...bo.console, ...cy.console].filter(l => /EXCEPTION|error/i.test(l));
    results.push(`page errors: ${errors.length ? errors.join(' | ') : 'none'}`);
} finally {
    for (const b of browsers) await b.close();
    server.kill();
    rmSync(tmp, {recursive: true, force: true});
}
console.log(results.join('\n'));
const tail = log.split('\n').filter(l => /error|warn/i.test(l)).slice(-5);
if (tail.length) console.log('server:', tail.join('\n'));
