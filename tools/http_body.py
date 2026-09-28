"""Response bodies for the tools' web servers (Atlas, the Dungeon Master): large text and JSON is sent gzipped to a
browser that accepts it, unless it is on this machine, where compressing costs more time than it saves."""
from __future__ import annotations

import gzip
import os

SMALLEST = 16 * 1024
TEXT = ('application/json', 'text/', 'application/javascript', 'image/svg+xml')


def encode(handler, body: bytes, mime: str) -> tuple[bytes, str | None]:
    """The body to send and its Content-Encoding (None: as it is)."""
    if len(body) < SMALLEST or not mime.startswith(TEXT):
        return body, None
    if 'gzip' not in handler.headers.get('Accept-Encoding', ''):
        return body, None
    peer = (getattr(handler, 'client_address', None) or ('',))[0]
    if peer in ('127.0.0.1', '::1', 'localhost') and os.environ.get('RATW_COMPRESS_LOCAL') != '1':
        return body, None
    return gzip.compress(body, compresslevel=3), 'gzip'


def send(handler, body: bytes, mime: str) -> bytes:
    """Sends Content-Length (and Content-Encoding, if compressed) for the body; returns what to write."""
    body, encoding = encode(handler, body, mime)
    if encoding:
        handler.send_header('Content-Encoding', encoding)
    handler.send_header('Vary', 'Accept-Encoding')
    handler.send_header('Content-Length', str(len(body)))
    return body
