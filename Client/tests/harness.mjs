// A local central and a ticket-mode world for the client's end-to-end tests (Docs/Design/77, Phase 3), as
// Central/test_world_join.py starts them: central (ratw_central.py serve) on a throwaway database made and dropped with
// the Postgres superuser from Database/.env (no world's database is touched, no role made or changed); the world agent
// keeping central's keys in a file; and ratw_server in ticket mode on a free port with the built-in demo world and a
// throwaway save file. Everything started here is stopped by stop().
//
//   const h = await startHarness({cors: 'http://127.0.0.1:PORT'});   // or null when it can't run here (why: h.skip)
//   ... h.central, h.world.id, h.world.port ...
//   await h.stop();
import {spawn, execFileSync} from 'node:child_process';
import {existsSync, mkdtempSync, readFileSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join, resolve} from 'node:path';
import {createInterface} from 'node:readline';

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
        /** Stops the world alone (a world going down mid-game). */
        async stopWorld() {
            const c = harness.world.process;
            if (c.exitCode !== null || c.signalCode !== null) return;
            c.kill('SIGTERM');
            await Promise.race([new Promise(r => c.once('exit', r)), sleep(20000)]);
        },
    };
    try {
        const worldId = python(`
import secrets, time, central_store
central_store.drop_database(${JSON.stringify(database)})
central_store.create_database(${JSON.stringify(database)})
s = central_store.Store(${JSON.stringify(database)}); s.migrate()
w = 'world-' + secrets.token_hex(16)
s.add_world(w, ${JSON.stringify(worldName)}, 'community', time.time() - 5)
s.close(); print(w)`, env);
        const c = started(['python3', join(root, 'Central', 'ratw_central.py'), 'serve', '--port', '0', '--keys', join(tmp, 'central-keys'),
            ...[cors].flat().flatMap(o => ['--cors', o])], env, /listening on (http:\/\/[0-9.:]+)\//);
        children.push(c.child);
        harness.central = await c.found;
        const keys = join(tmp, 'world', 'central-keys.json');
        const agent = started(['python3', join(root, 'tools', 'world_agent.py'), 'keys', '--central', harness.central, '--out', keys], env, null);
        children.push(agent.child);
        for (let i = 0; i < 200 && !existsSync(keys); ++i) await sleep(100);
        if (!existsSync(keys)) throw new Error('the world agent fetched no keys');
        const w = started([server, '--save', join(tmp, 'world', 'save.json'), '--port', '0', '--perf-log', '0', '--workers', '0', '--for', '1200',
            '--central', harness.central, '--world-id', worldId, '--central-keys', keys, '--world-name', worldName],
            {}, /listening on port (\d+)/, 120);
        children.push(w.child);
        harness.world = {id: worldId, port: Number(await w.found), process: w.child, lines: w.lines};
        python(`
import central_store
s = central_store.Store(${JSON.stringify(database)})
assert s.set_world(${JSON.stringify(worldId)}, '127.0.0.1', ${harness.world.port}, {'protocol': 1, 'secure': False, 'description': 'A test world on this computer.'})
s.close()`, env);
        return harness;
    } catch (error) {
        await harness.stop();
        throw error;
    }
}
