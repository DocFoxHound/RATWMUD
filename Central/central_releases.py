"""The world releases central knows (Docs/Design/77-central-accounts-and-hosted-worlds.md, 7.1: approved worlds).

Each world image Package/stage.sh builds comes with a manifest of its fingerprints (tools/world_fingerprints.py run
inside the image), signed with the release key (Ed25519, the key stage.sh signs the Editor with):

    {"v": 1, "key": "ratw-<16 hex>", "manifest": base64(the manifest's JSON bytes), "signature": base64(Ed25519)}

The manifest: {"v": 1, "build", "made", "server", "agent", "fingerprints", "rules", "rulesFiles": {...},
"quickened": {"stars", "starGivers"}}. Central takes one only when it is signed by a key in Central/release-keys.json
({"keys": [{"id": "ratw-...", "public": base64 of the raw 32-byte key}]}; the id is "ratw-" and the first 16 hex of
the key's SHA-256, as stage.sh names it); `ratw_central.py releases add FILE --unsigned` takes one by hand without (an
admin on central's machine, for development).
"""
from __future__ import annotations

import base64
import binascii
import hashlib
import json
from pathlib import Path
import re

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey

HERE = Path(__file__).resolve().parent
KEYS_FILE = HERE / 'release-keys.json'
RELEASES_DIR = HERE / 'releases'
SHA256 = re.compile(r'^[0-9a-f]{64}$')
BUILD = re.compile(r'^[A-Za-z0-9._+-]{1,64}$')
SOFTWARE = ('server', 'agent', 'fingerprints')
MOST = 64 * 1024


class ReleaseError(ValueError):
    pass


def key_id(raw: bytes) -> str:
    return 'ratw-' + hashlib.sha256(raw).hexdigest()[:16]


def load_keys(path: Path | None = None) -> dict[str, bytes]:
    """{key id: raw public key} from the release keys file (none if it is missing)."""
    path = path or KEYS_FILE
    try:
        doc = json.loads(path.read_text(encoding='utf-8'))
    except FileNotFoundError:
        return {}
    out = {}
    for entry in doc.get('keys', []) if isinstance(doc, dict) else []:
        try:
            raw = base64.b64decode(entry['public'], validate=True)
        except (KeyError, TypeError, binascii.Error):
            continue
        if len(raw) == 32 and entry.get('id') == key_id(raw):
            out[entry['id']] = raw
    return out


def check_manifest(manifest) -> dict:
    """The manifest's fields, checked; ReleaseError if any is wrong."""
    if not isinstance(manifest, dict) or manifest.get('v') != 1:
        raise ReleaseError('not a version 1 release manifest')
    if not isinstance(manifest.get('build'), str) or not BUILD.match(manifest['build']):
        raise ReleaseError('the manifest has no build id')
    for key in (*SOFTWARE, 'rules'):
        if not isinstance(manifest.get(key), str) or not SHA256.match(manifest[key]):
            raise ReleaseError(f'the manifest has no SHA-256 for {key}')
    quickened = manifest.get('quickened') if isinstance(manifest.get('quickened'), dict) else {}
    for key in ('stars', 'starGivers'):
        value = quickened.get(key)
        if not isinstance(value, int) or isinstance(value, bool) or not 0 < value <= 100000:
            raise ReleaseError(f"the manifest's Quickened thresholds have no {key}")
    return manifest


def read(signed, keys: dict[str, bytes] | None, unsigned: bool = False) -> tuple[dict, str]:
    """(the manifest, the key id that signed it) from a signed file's JSON; with `unsigned`, a bare manifest is taken
    too (key id ''). ReleaseError if it can't be."""
    if not isinstance(signed, dict):
        raise ReleaseError('not a JSON object')
    if 'manifest' not in signed:
        if not unsigned:
            raise ReleaseError('the manifest is not signed (sign it with Package/stage.sh, or give --unsigned)')
        return check_manifest(signed), ''
    try:
        data = base64.b64decode(signed['manifest'], validate=True)
        signature = base64.b64decode(signed['signature'], validate=True)
    except (KeyError, TypeError, binascii.Error) as error:
        raise ReleaseError('the signed file is not base64') from error
    kid = signed.get('key')
    if not keys or kid not in keys:
        raise ReleaseError(f'it is signed with {kid!r}, which is not in Central/release-keys.json')
    try:
        Ed25519PublicKey.from_public_bytes(keys[kid]).verify(signature, data)
    except InvalidSignature as error:
        raise ReleaseError('its signature does not match') from error
    try:
        manifest = json.loads(data.decode('utf-8'))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ReleaseError('the signed manifest is not JSON') from error
    return check_manifest(manifest), kid


def read_file(path: Path, keys: dict[str, bytes] | None, unsigned: bool = False) -> tuple[dict, str]:
    if path.stat().st_size > MOST:
        raise ReleaseError('the file is too large for a manifest')
    try:
        doc = json.loads(path.read_text(encoding='utf-8'))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ReleaseError('the file is not JSON') from error
    return read(doc, keys, unsigned)


def compare(reported: dict, release: dict) -> list[str]:
    """Which of a world's fingerprints differ from a release's ([] if none)."""
    return [k for k in (*SOFTWARE, 'rules') if reported.get(k) != release.get(k)]
