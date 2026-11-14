"""Tickets and their keys (Docs/Design/77-central-accounts-and-hosted-worlds.md, section 5).

A ticket is a JWT (RFC 7519) signed with the fully specified `Ed25519` algorithm of RFC 9864, the key's id (`kid`) in
its header. Central signs; a world checks it offline with central's public keys (`GET /keys`, a JWK set). `verify()`
here is the reference check, the same rules Phase 2's C++ check follows (and the Phase 0 spike, Desktop/spike/ticket):
the algorithm pinned before anything else is read, repeated keys refused, `aud` the world's own id, `exp` with 2
minutes' leeway, `iat` after the world started and not ahead of the world's clock, a lifetime of at most 10 minutes,
each `jti` once.

The keys live in a directory of their own (mode 0700, each file 0600), never in the database: one JSON file per key,
with its private half, and the secret for the per-world account ids. Rotation: the next key is made and published a
day before it signs anything; when it starts signing, the old one stops, and leaves `/keys` after a grace that covers
any ticket it signed. An emergency rotation (`rotate(now, immediate=True)`) signs with a new key at once and drops every
other key from `/keys`, which revokes every ticket outstanding.
"""
from __future__ import annotations

import base64
import hashlib
import hmac
import json
import os
from pathlib import Path
import re
import secrets
import threading
import time

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey

ALG = 'Ed25519'
LIFETIME = 60            # seconds a ticket lives
LEEWAY = 120             # a world's allowance for a wrong clock (doc 77, 5)
MAX_LIFETIME = 600       # a world refuses a ticket that would live longer (Core/RatwTickets.h, the same)
LEAD = 24 * 3600         # the next key is published this long before it signs
ROTATE_AFTER = 30 * 24 * 3600   # a key signs for this long
GRACE = 3600             # a retired key stays in /keys this long (covers every ticket it signed, and the leeway)
WOLF_ID = re.compile(r'^wolf-[0-9a-f]{32}$')
B64URL = re.compile(r'^[A-Za-z0-9_-]*$')


class TicketError(ValueError):
    """A ticket a world must refuse; the message says why."""


def b64u(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b'=').decode('ascii')


def unb64u(text: str) -> bytes:
    if not B64URL.match(text) or len(text) % 4 == 1:
        raise TicketError('not base64url')
    return base64.urlsafe_b64decode(text + '=' * (-len(text) % 4))


def compact(obj) -> bytes:
    return json.dumps(obj, separators=(',', ':'), ensure_ascii=False).encode('utf-8')


def strict_json(raw: bytes):
    """JSON with no repeated keys and nothing but an object at the top."""
    def pairs(items):
        seen = {}
        for key, value in items:
            if key in seen:
                raise TicketError(f'repeated key {key!r}')
            seen[key] = value
        return seen
    try:
        value = json.loads(raw.decode('utf-8'), object_pairs_hook=pairs,
                           parse_constant=lambda c: (_ for _ in ()).throw(TicketError('not JSON')))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise TicketError('not JSON') from error
    if not isinstance(value, dict):
        raise TicketError('not a JSON object')
    return value


def sign(private: Ed25519PrivateKey, kid: str, claims: dict) -> str:
    head = b64u(compact({'alg': ALG, 'typ': 'JWT', 'kid': kid}))
    body = b64u(compact(claims))
    signing_input = f'{head}.{body}'
    return f'{signing_input}.{b64u(private.sign(signing_input.encode("ascii")))}'


def public_keys(jwks: dict) -> dict[str, bytes]:
    """{kid: raw public key} from a JWK set as /keys serves it; anything but an Ed25519 key is ignored."""
    out = {}
    for key in jwks.get('keys', []):
        if key.get('kty') == 'OKP' and key.get('crv') == 'Ed25519' and key.get('alg') == ALG:
            try:
                raw = unb64u(key['x'])
            except (TicketError, KeyError, TypeError):
                continue
            if len(raw) == 32 and isinstance(key.get('kid'), str):
                out[key['kid']] = raw
    return out


def verify(token: str, keys: dict[str, bytes], world: str, now: float, world_start: float, seen: dict[str, float],
           leeway: float = LEEWAY) -> dict:
    """The claims of a ticket a world may admit, or TicketError. `seen` is the world's jti memory ({jti: forget at}),
    updated here."""
    if not isinstance(token, str) or not token.isascii():
        raise TicketError('not a ticket')
    parts = token.split('.')
    if len(parts) != 3:
        raise TicketError('a ticket has three parts')
    header = strict_json(unb64u(parts[0]))
    if header.get('alg') != ALG:               # (Before anything else is read: no `none`, no `EdDSA`, no HS256.)
        raise TicketError('wrong algorithm')
    if header.get('typ') not in (None, 'JWT') or set(header) - {'alg', 'typ', 'kid'}:
        raise TicketError('unexpected header')
    kid = header.get('kid')
    if not isinstance(kid, str) or kid not in keys:
        raise TicketError('unknown key')
    signature = unb64u(parts[2])
    try:
        Ed25519PublicKey.from_public_bytes(keys[kid]).verify(signature, f'{parts[0]}.{parts[1]}'.encode('ascii'))
    except InvalidSignature as error:
        raise TicketError('bad signature') from error
    claims = strict_json(unb64u(parts[1]))
    for field, kind in (('aud', str), ('jti', str), ('wolf', str), ('sub', str), ('name', str), ('sex', str),
                        ('kind', str), ('look', dict), ('gift', str), ('age', int), ('iat', int), ('exp', int)):
        if not isinstance(claims.get(field), kind) or isinstance(claims.get(field), bool):
            raise TicketError(f'missing or malformed {field}')
    if 'quickened' in claims and not isinstance(claims['quickened'], bool):
        raise TicketError('missing or malformed quickened')
    if claims['aud'] != world:
        raise TicketError('ticket for another world')
    if now > claims['exp'] + leeway:
        raise TicketError('expired')
    if claims['iat'] > now + leeway:
        raise TicketError('issued in the future')
    if claims['iat'] < world_start - leeway:
        raise TicketError('issued before this world started')
    if not 0 < claims['exp'] - claims['iat'] <= MAX_LIFETIME:
        raise TicketError('wrong lifetime')
    if not WOLF_ID.match(claims['wolf']) or claims['sex'] not in ('female', 'male') or \
            claims['kind'] not in ('official', 'community') or not 16 <= len(claims['jti']) <= 64:
        raise TicketError('malformed identity')
    for jti in [j for j, until in seen.items() if until < now]:
        del seen[jti]
    if claims['jti'] in seen:
        raise TicketError('ticket already used')
    seen[claims['jti']] = claims['exp'] + leeway
    return claims


class KeyRing:
    """Central's ticket keys, in a directory of their own. Thread-safe."""

    def __init__(self, directory: Path, lead: float = LEAD, rotate_after: float = ROTATE_AFTER, grace: float = GRACE):
        self.directory = Path(directory)
        self.lead, self.rotate_after, self.grace = lead, rotate_after, grace
        self._lock = threading.RLock()
        self.directory.mkdir(parents=True, exist_ok=True)
        os.chmod(self.directory, 0o700)
        self._keys: dict[str, dict] = {}
        self._private: dict[str, Ed25519PrivateKey] = {}
        self._load()

    # ---------------------------------------------------------------- Files

    def _load(self):
        for path in sorted(self.directory.glob('key-*.json')):
            record = json.loads(path.read_text(encoding='utf-8'))
            self._keys[record['kid']] = record
            self._private[record['kid']] = Ed25519PrivateKey.from_private_bytes(unb64u(record['private']))

    def _write(self, name: str, text: str):
        path = self.directory / name
        temp = self.directory / f'.{name}.{secrets.token_hex(4)}'
        fd = os.open(temp, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, 'w', encoding='utf-8') as out:
            out.write(text)
            out.flush()
            os.fsync(out.fileno())
        os.replace(temp, path)

    def _save(self, record: dict):
        self._write(f'key-{record["kid"]}.json', json.dumps(record, indent=1))

    def _new(self, now: float, activates: float) -> dict:
        private = Ed25519PrivateKey.generate()
        raw = private.private_bytes(serialization.Encoding.Raw, serialization.PrivateFormat.Raw,
                                    serialization.NoEncryption())
        public = private.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
        kid = time.strftime('%Y%m%d', time.gmtime(activates)) + '-' + secrets.token_hex(3)
        record = {'kid': kid, 'created': now, 'activates': activates, 'retires': None, 'removed': None,
                  'x': b64u(public), 'private': b64u(raw)}
        self._keys[kid], self._private[kid] = record, private
        self._save(record)
        return record

    def pairwise_secret(self) -> bytes:
        """The secret for per-world account ids on community worlds (doc 77, 5), made once."""
        with self._lock:
            path = self.directory / 'pairwise.secret'
            if not path.exists():
                self._write('pairwise.secret', secrets.token_hex(32))
            return bytes.fromhex(path.read_text(encoding='ascii').strip())

    # ---------------------------------------------------------------- Rotation

    def _signing(self, now: float) -> dict | None:
        live = [k for k in self._keys.values()
                if k['activates'] <= now and (k['retires'] is None or k['retires'] > now)]
        return max(live, key=lambda k: k['activates'], default=None)

    def maintain(self, now: float) -> list[str]:
        """Keeps the ring turning: a key to sign with, the next one published a day ahead of its turn, old ones gone
        once their grace is over. Returns what it did (for the log). Call at start and every few minutes."""
        done = []
        with self._lock:
            for kid, k in list(self._keys.items()):
                if k['removed'] is not None and k['removed'] <= now:
                    (self.directory / f'key-{kid}.json').unlink(missing_ok=True)
                    del self._keys[kid], self._private[kid]
                    done.append(f'removed {kid}')
            current = self._signing(now)
            if current is None:
                current = self._new(now, now)
                done.append(f'made {current["kid"]} (signing now)')
            pending = [k for k in self._keys.values() if k['activates'] > now]
            if not pending and now >= current['activates'] + self.rotate_after - self.lead:
                self._schedule(current, self._new(now, now + self.lead))
                done.append(f'made {self._latest()["kid"]} (signing from {self._latest()["activates"]:.0f})')
        return done

    def _latest(self) -> dict:
        return max(self._keys.values(), key=lambda k: k['activates'])

    def _schedule(self, old: dict, new: dict):
        old['retires'] = new['activates']
        old['removed'] = new['activates'] + self.grace
        self._save(old)

    def rotate(self, now: float, immediate: bool = False) -> str:
        """A new key: published now and signing in a day, or (immediate) signing now with every other key withdrawn,
        which revokes every ticket outstanding. Returns its kid."""
        with self._lock:
            if immediate:
                for k in self._keys.values():
                    k['retires'] = k['removed'] = now
                    self._save(k)
                new = self._new(now, now)
                self.maintain(now)
                return new['kid']
            pending = [k for k in self._keys.values() if k['activates'] > now]
            if pending:
                return pending[0]['kid']
            current = self._signing(now)
            new = self._new(now, now + self.lead)
            if current is not None:
                self._schedule(current, new)
            return new['kid']

    # ---------------------------------------------------------------- Use

    def jwks(self, now: float) -> dict:
        """The public set: the signing key, the next one, and retired ones still in their grace."""
        with self._lock:
            keys = [k for k in self._keys.values() if k['removed'] is None or k['removed'] > now]
            return {'keys': [{'kty': 'OKP', 'crv': 'Ed25519', 'alg': ALG, 'use': 'sig', 'kid': k['kid'], 'x': k['x']}
                             for k in sorted(keys, key=lambda k: k['activates'])]}

    def sign(self, claims: dict, now: float) -> tuple[str, str]:
        """(ticket, kid) signed with the key whose turn it is."""
        with self._lock:
            current = self._signing(now)
            if current is None:
                self.maintain(now)
                current = self._signing(now)
            return sign(self._private[current['kid']], current['kid'], claims), current['kid']

    def describe(self, now: float) -> list[dict]:
        with self._lock:
            current = self._signing(now)
            return [{'kid': k['kid'], 'activates': k['activates'], 'retires': k['retires'], 'removed': k['removed'],
                     'signing': current is not None and k['kid'] == current['kid']}
                    for k in sorted(self._keys.values(), key=lambda k: k['activates'])]


def pairwise_id(secret: bytes, account_id: str, world_id: str) -> str:
    """A community world's id for an account (OpenID's pairwise ids): the same in one world, unlinkable across worlds,
    in the account id's own shape."""
    return 'acct-' + hmac.new(secret, f'{account_id}|{world_id}'.encode(), hashlib.sha256).hexdigest()[:32]
