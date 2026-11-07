"""Central's identity check (Docs/Design/77-central-accounts-and-hosted-worlds.md, 4.1): one ratw_identity process
(Server/ratw_identity.cpp), built from Core with the game's own creation rules, kept running and asked one JSON line at
a time. Central never checks a name, a look, a Gift or a profile itself.

The binary: RATW_IDENTITY, else the first of build-core/, build-central/ and Build/ that holds one. Data/ is passed to
it as RATW_DATA_DIR (the repository's, unless RATW_DATA_DIR is set already).
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import selectors
import subprocess
import threading

ROOT = Path(__file__).resolve().parent.parent


class IdentityError(RuntimeError):
    """ratw_identity could not be run or did not answer (not a refusal: that is an answer)."""


def find_binary() -> Path:
    if os.environ.get('RATW_IDENTITY'):
        return Path(os.environ['RATW_IDENTITY'])
    for folder in ('build-core', 'build-central', 'Build'):
        candidate = ROOT / folder / 'ratw_identity'
        if candidate.is_file():
            return candidate
    raise IdentityError('No ratw_identity: build it (cmake --build build-core --target ratw_identity) or set RATW_IDENTITY.')


class Identity:
    def __init__(self, binary: Path | None = None, timeout: float = 5.0):
        self.binary = Path(binary) if binary else find_binary()
        self.timeout = timeout
        self._lock = threading.Lock()
        self._process: subprocess.Popen | None = None

    def _start(self):
        env = dict(os.environ)
        env.setdefault('RATW_DATA_DIR', str(ROOT / 'Data'))
        try:
            self._process = subprocess.Popen([str(self.binary)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                             stderr=subprocess.DEVNULL, env=env, bufsize=0)
        except OSError as error:
            raise IdentityError(f'Cannot start {self.binary}: {error}') from error

    def _stop(self):
        if self._process:
            try:
                self._process.kill()
                self._process.wait(timeout=2)
            except Exception:
                pass
            for pipe in (self._process.stdin, self._process.stdout):
                try:
                    pipe.close()
                except Exception:
                    pass
        self._process = None

    def _ask(self, request: dict) -> dict:
        if self._process is None or self._process.poll() is not None:
            self._stop()
            self._start()
        process = self._process
        line = json.dumps(request, separators=(',', ':'), ensure_ascii=False).encode('utf-8') + b'\n'
        process.stdin.write(line)
        process.stdin.flush()
        reply = b''
        with selectors.DefaultSelector() as selector:
            selector.register(process.stdout, selectors.EVENT_READ)
            while not reply.endswith(b'\n'):
                if not selector.select(self.timeout):
                    raise IdentityError('ratw_identity did not answer in time')
                chunk = os.read(process.stdout.fileno(), 65536)
                if not chunk:
                    raise IdentityError('ratw_identity stopped')
                reply += chunk
        return json.loads(reply.decode('utf-8'))

    def check(self, request: dict) -> dict:
        """The reply: {"ok": true, ...} or {"ok": false, "error": ...}. IdentityError if it can't be asked (one retry on
        a fresh process)."""
        with self._lock:
            for attempt in (1, 2):
                try:
                    return self._ask(request)
                except (IdentityError, OSError, ValueError) as error:
                    self._stop()
                    if attempt == 2:
                        raise IdentityError(str(error)) from error
        raise IdentityError('unreachable')

    def wolf(self, wolf: dict) -> dict:
        return self.check({'check': 'wolf', 'wolf': wolf})

    def look(self, appearance) -> dict:
        return self.check({'check': 'look', 'appearance': appearance})

    def profile(self, saved: dict | None, fields) -> dict:
        return self.check({'check': 'profile', 'profile': saved, 'fields': fields})

    def rules(self) -> dict:
        return self.check({'check': 'rules'})

    def close(self):
        with self._lock:
            if self._process and self._process.stdin:
                try:
                    self._process.stdin.close()
                    self._process.wait(timeout=2)
                except Exception:
                    pass
            self._stop()
