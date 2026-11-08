// A local central and a ticket-mode world for the client's end-to-end tests (Docs/Design/77, Phase 3), as
// a host runs them (doc 77, Phase 4): central (ratw_central.py serve --local-worlds, so a world on this computer can be
// listed) on a throwaway database made and dropped with the Postgres superuser from Database/.env (no world's database
// is touched, no role made or changed); a world with a host token; the world agent (`run`: central's keys, and the
// heartbeat that lists the world); and ratw_server in ticket mode on a free port with the built-in demo world and a
// throwaway save file. It returns once central lists the world. Everything started here is stopped by stop().
//
//   const h = await startHarness({cors: 'http://127.0.0.1:PORT'});   // or null when it can't run here (why: h.skip)
//   ... h.central, h.world.id, h.world.port ...
//   await h.stop();
import {spawn, execFileSync} from 'node:child_process';
import {existsSync, mkdtempSync, readFileSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {createInterface} from 'node:readline';
import {createServer} from 'node:net';

export const root = resolve(import.meta.dirname, '..', '..');

/** The Postgres settings: the environment's, else Database/.env here, else the main checkout's (a git worktree has
 *  none of its own). */
export function databaseEnv() {
    if (process.env.POSTGRES_PASSWORD) return {};
    const candidates = [join(root, 'Database', '.env')];
    try {
        const common = execFileSync('git', ['rev-parse', '--git-common-dir'], {cwd: root, encoding: 'utf8'}).trim();
        candidates.push(join(dirname(resolve(root, common)), 'Database', '.env'));
    } catch { /* not a checkout */ }
    for (const file of candidates) {
        if (!existsSync(file)) continue;
        const env = {};
        for (const line of readFileSync(file, 'utf8').split('\n')) {
            const m = /^\s*([A-Z_][A-Z0-9_]*)\s*=\s*(.*?)\s*$/.exec(line);
            if (m) env[m[1]] = m[2].replace(/^(['"])(.*)\1$/, '$2');
        }
        if (env.POSTGRES_PASSWORD) return env;
    }
    return null;
}

export function binary(name, variable) {
    if (process.env[variable]) return process.env[variable];
    for (const folder of ['build-central', 'build-core', 'Build'])
        if (existsSync(join(root, folder, name))) return join(root, folder, name);
    return null;
}

function started(args, env, ready, seconds = 60) {
    const child = spawn(args[0], args.slice(1), {cwd: root, env: {...process.env, ...env}, stdio: ['ignore', 'pipe', 'pipe']});
    const lines = [];
    const found = new Promise((resolveFound, reject) => {
        const timer = setTimeout(() => reject(new Error(`${args[0]} did not start: ${lines.slice(-20).join('\n')}`)), seconds * 1000);
        for (const stream of [child.stdout, child.stderr])
            createInterface({input: stream}).on('line', line => {
                lines.push(line);
                if (lines.length > 400) lines.shift();
                const m = ready && ready.exec(line);
                if (m) {
                    clearTimeout(timer);
                    resolveFound(m[1]);
                }
            });
        child.once('exit', code => {
            clearTimeout(timer);
            if (ready) reject(new Error(`${args[0]} exited (${code}): ${lines.slice(-20).join('\n')}`));
        });
        if (!ready) {
            clearTimeout(timer);
            resolveFound('');
        }
    });
    return {child, found, lines};
}

function python(code, env) {
    return execFileSync('python3', ['-c', code], {cwd: join(root, 'Central'), env: {...process.env, ...env}, encoding: 'utf8'}).trim();
}

const sleep = ms => new Promise(r => setTimeout(r, ms));

function freePort() {
    return new Promise((resolve, reject) => {
        const probe = createServer();
        probe.once('error', reject);
        probe.listen(0, '127.0.0.1', () => {
            const {port} = probe.address();
            probe.close(() => resolve(port));
        });
    });
}

/** Starts central and one world; null with `skip` set when this computer can't (no Postgres settings, no binaries). */
export async function startHarness({cors = [], database = 'ratw_central_clienttest', worldName = 'Client Test Hollow'} = {}) {
    const dbEnv = databaseEnv();
    const server = binary('ratw_server', 'RATW_SERVER'), identity = binary('ratw_identity', 'RATW_IDENTITY');
    if (!dbEnv) return {skip: 'no Postgres settings (Database/.env)'};
    if (!server || !identity) return {skip: 'no ratw_server or ratw_identity built (build-central/)'};
    const env = {...dbEnv, RATW_CENTRAL_DBNAME: database, RATW_IDENTITY: identity};
    const tmp = mkdtempSync(join(tmpdir(), 'ratw-client-e2e-'));
    const children = [];
    // Never left running: if the test process ends or fails before stop(), everything started here goes too.
    process.once('exit', () => { for (const c of children) try { c.kill('SIGKILL'); } catch { /* gone */ } });

    const harness = {
        tmp, env, children, central: '', world: {id: '', port: 0, process: null},
        async stop() {
            for (const c of children.reverse()) if (c.exitCode === null && c.signalCode === null) c.kill('SIGTERM');
            await Promise.all(children.map(c => c.exitCode !== null || c.signalCode !== null ? null
                : Promise.race([new Promise(r => c.once('exit', r)), sleep(20000).then(() => c.kill('SIGKILL'))])));
            try { python(`import central_store; central_store.drop_database(${JSON.stringify(database)})`, env); } catch { /* gone */ }
            rmSync(tmp, {recursive: true, force: true});
        },
        /** Makes an account a host (`ratw_central.py accounts flag USERNAME --host`), as the user does by hand (Q11). */
        flagHost(username) {
            execFileSync('python3', [join(root, 'Central', 'ratw_central.py'), 'accounts', 'flag', username, '--host'],
                {cwd: root, env: {...process.env, ...env}, encoding: 'utf8'});
        },
        /** Stops the world alone (a world going down mid-game). */
        async stopWorld() {
            const c = harness.world.process;
            if (c.exitCode !== null || c.signalCode !== null) return;
            c.kill('SIGTERM');
            await Promise.race([new Promise(r => c.once('exit', r)), sleep(20000)]);
        },
    };
    try {
        const worldSetup = python(`
import secrets, time, central_store, central_service
central_store.drop_database(${JSON.stringify(database)})
central_store.create_database(${JSON.stringify(database)})
s = central_store.Store(${JSON.stringify(database)}); s.migrate()
w = 'world-' + secrets.token_hex(16)
s.add_world(w, ${JSON.stringify(worldName)}, 'community', time.time() - 5)
token = 'rh_' + secrets.token_urlsafe(32)
assert s.set_world_token(w, central_service.token_hash(token), time.time())
s.close(); print(w, token)`, env);
        const [worldId, hostToken] = worldSetup.split(' ');
        const c = started(['python3', join(root, 'Central', 'ratw_central.py'), 'serve', '--port', '0', '--local-worlds', '--keys', join(tmp, 'central-keys'),
            ...[cors].flat().flatMap(o => ['--cors', o])], env, /listening on (http:\/\/[0-9.:]+)\//);
        children.push(c.child);
        harness.central = await c.found;
        const agentDir = join(tmp, 'world'), keys = join(agentDir, 'central-keys.json');
        // The world's port is chosen here (the agent needs it before the world starts): a free one, asked of the OS.
        const port = await freePort();
        const agent = started(['python3', join(root, 'tools', 'world_agent.py'), 'run', '--central', harness.central, '--dir', agentDir,
            '--world-port', String(port), '--every', '7', '--description', 'A test world on this computer.'], {RATW_HOST_TOKEN: hostToken}, null);
        children.push(agent.child);
        harness.agent = agent;
        for (let i = 0; i < 200 && !existsSync(keys); ++i) await sleep(100);
        if (!existsSync(keys)) throw new Error(`the world agent fetched no keys: ${agent.lines.slice(-10).join('\n')}`);
        const w = started([server, '--save', join(agentDir, 'save.json'), '--port', String(port), '--perf-log', '0', '--workers', '0', '--for', '1200',
            '--central', harness.central, '--world-id', worldId, '--central-keys', keys, '--world-name', worldName],
            {}, /listening on port (\d+)/, 120);
        children.push(w.child);
        harness.world = {id: worldId, port: Number(await w.found), process: w.child, lines: w.lines};
        // Listed once a heartbeat finds the world up and central's call-back reaches it.
        for (let i = 0; i < 300; ++i) {
            const listed = await fetch(`${harness.central}/worlds`).then(r => r.json()).catch(() => null);
            if (listed?.worlds?.some(x => x.id === worldId)) break;
            if (i === 299) throw new Error(`central never listed the world: ${agent.lines.slice(-10).join('\n')}`);
            await sleep(100);
        }
        return harness;
    } catch (error) {
        await harness.stop();
        throw error;
    }
}
