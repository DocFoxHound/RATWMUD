"""Running the game for the smokes (Docs/Design/27-browser-client.md): the server (build-core/ratw_server, serving the
built browser client) and scripted players (tools/client/scenario.ts), each its own process.

    with GameServer(save=run / 'world.json', flags=['--dev-tools'], log=logs / 'server.log') as server:
        result = run_scenario(server.port, 'network', 'ash', capture=evidence, log=logs / 'ash.log')
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import socket
import subprocess
import time

ROOT = Path(__file__).resolve().parent.parent
SERVER = Path(os.environ.get('RATW_SERVER_BINARY', ROOT / 'build-core' / 'ratw_server'))
CLIENT = ROOT / 'Client'
NODE = ['node', '--experimental-strip-types', '--no-warnings']


def ensure_client() -> Path:
    """The built browser client, built first if its sources are newer than the build."""
    dist = CLIENT / 'dist'
    index = dist / 'index.html'
    sources = [p for p in (CLIENT / 'src').rglob('*') if p.is_file()] + [CLIENT / 'index.html', CLIENT / 'vite.config.ts']
    if not index.exists() or any(p.stat().st_mtime > index.stat().st_mtime for p in sources):
        if not (CLIENT / 'node_modules').exists():
            subprocess.run(['npm', '--prefix', str(CLIENT), 'install', '--no-audit', '--no-fund'], check=True)
        subprocess.run(['npm', '--prefix', str(CLIENT), 'run', 'build', '--silent'], check=True)
    return dist


def ensure_server() -> Path:
    if not SERVER.exists():
        raise RuntimeError(f'No server at {SERVER}: build it with cmake --build build-core --target ratw_server')
    return SERVER


def free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


class GameServer:
    """ratw_server on a free port until the block ends (stopped with SIGTERM, which saves)."""

    def __init__(self, save: Path | None = None, world: Path | None = None, database: str | None = None,
                 flags: list[str] | None = None, log: Path | None = None, dev_identity: bool = True, env: dict | None = None):
        self.port = free_port()
        command = [str(ensure_server()), '--port', str(self.port), '--web', str(ensure_client())]
        if database:
            command += ['--database', database]
        else:
            command += ['--save', str(save)]
            if world:
                command += ['--world', str(world)]
        if dev_identity:
            command += ['--dev-identity']
        command += flags or []
        self.log = log or ROOT / 'artifacts' / 'logs' / 'server.log'
        self.log.parent.mkdir(parents=True, exist_ok=True)
        self.handle = self.log.open('w')
        self.process = subprocess.Popen(command, cwd=ROOT, stdout=self.handle, stderr=subprocess.STDOUT,
                                        env={**os.environ, **(env or {})})
        deadline = time.monotonic() + 60
        while f'listening on port {self.port}' not in self.log.read_text(errors='replace'):
            if self.process.poll() is not None or time.monotonic() > deadline:
                self.stop()
                raise RuntimeError(f'The server did not start; inspect {self.log}')
            time.sleep(0.05)

    @property
    def url(self) -> str:
        return f'http://127.0.0.1:{self.port}/'

    def stop(self) -> int | None:
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=10)
        self.handle.close()
        return self.process.returncode

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.stop()


def start_scenario(port: int, scenario: str, identity: str = 'ash', name: str | None = None, capture: Path | None = None,
                   dev: bool = True, browser: bool = False, screenshots: bool = False, log: Path | None = None) -> subprocess.Popen:
    """A scripted player, running (see tools/client/scenario.ts)."""
    command = NODE + [str(ROOT / 'tools' / 'client' / 'scenario.ts'), '--port', str(port), '--scenario', scenario,
                      '--identity', identity, '--name', name or identity.title()]
    if capture:
        command += ['--capture', str(capture)]
    if dev:
        command += ['--dev']
    if browser or screenshots:
        command += ['--browser']
    if screenshots:
        command += ['--screenshots']
    log = log or ROOT / 'artifacts' / 'logs' / f'{scenario}-{identity}.log'
    log.parent.mkdir(parents=True, exist_ok=True)
    return subprocess.Popen(command, cwd=ROOT, stdout=log.open('w'), stderr=subprocess.STDOUT)


def evidence(capture: Path, scenario: str, identity: str = 'ash', since_ns: int = 0) -> dict:
    path = capture / f'{scenario}-{identity}.json'
    if not path.exists() or path.stat().st_mtime_ns < since_ns:
        raise RuntimeError(f'No fresh evidence from {scenario} ({identity}) at {path}')
    return json.loads(path.read_text())


def run_scenario(port: int, scenario: str, identity: str = 'ash', capture: Path | None = None, timeout: float = 180,
                 **options) -> dict:
    """Runs a scripted player to the end and returns its evidence (raising if it failed)."""
    capture = capture or ROOT / 'artifacts' / 'screenshots'
    started = time.time_ns()
    child = start_scenario(port, scenario, identity, capture=capture, **options)
    try:
        code = child.wait(timeout=timeout)
    finally:
        if child.poll() is None:
            child.kill()
            child.wait()
    result = evidence(capture, scenario, identity, started)
    if code or not result.get('passed'):
        raise RuntimeError(f'{scenario} ({identity}) failed: {result.get("detail")}')
    return result


def fresh_screenshots(capture: Path, names, since_ns: int):
    for name in names:
        shot = capture / name
        if not shot.exists() or shot.stat().st_mtime_ns < since_ns or shot.stat().st_size < 1024:
            raise RuntimeError(f'No fresh screenshot: {name}')


def series(scenarios, save: Path, world: Path | None = None, screenshots=(), headless: bool = False,
           capture: Path | None = None, flags=('--dev-tools',), identity: str = 'ash'):
    """Runs scenarios one after another, each against a new server process on the same save (so a '-restore' scenario
    checks what survived a restart). The first takes the screenshots named, unless headless."""
    capture = capture or ROOT / 'artifacts' / 'screenshots'
    logs = ROOT / 'artifacts' / 'logs'
    capture.mkdir(parents=True, exist_ok=True)
    results = []
    for index, scenario in enumerate(scenarios):
        shoot = bool(screenshots) and index == 0 and not headless
        started = time.time_ns()
        with GameServer(save=save, world=world, flags=list(flags), log=logs / f'{scenario}-server.log') as server:
            result = run_scenario(server.port, scenario, identity, capture=capture, screenshots=shoot, log=logs / f'{scenario}.log')
        if shoot:
            fresh_screenshots(capture, screenshots, started)
        print('PASS: ' + result['detail'], flush=True)
        results.append(result)
    return results
