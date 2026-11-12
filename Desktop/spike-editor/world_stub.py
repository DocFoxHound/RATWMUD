#!/usr/bin/env python3
"""Doc 78, Phase M0a: a stand-in for the world's Editor port (tools/world_admin.py, not built yet).

TLS 1.3 only with a self-signed P-256 key (make_cert.sh, the doc's openssl command), on --port; and the same key on
--port12 with TLS 1.2 only, which the shell's pinned client must refuse. It serves:
- the built studio app (--dist, Editor/dist) for GET / and its files;
- /api/...: forwarded to a running studio (--studio http://127.0.0.1:N, with Host and Origin set as trusted() wants,
  standing in for hosted mode's replaced check), or a stub 401 without one;
- /spike/hello, /spike/echo (POST: needs Content-Length, refuses chunked; replies with the size and CRC-32),
  /spike/big?mb=N&gz=0|1 (N MB of JSON-like text, gzipped when asked and accepted), /spike/download.json and .exe.
Each request is logged as one JSON line on stdout, with the peer address.
"""
import argparse
import gzip
import http.client
import json
import os
import ssl
import sys
import threading
import time
import urllib.parse
import zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

MIME = {'.html': 'text/html; charset=utf-8', '.js': 'text/javascript', '.css': 'text/css', '.svg': 'image/svg+xml',
        '.png': 'image/png', '.woff2': 'font/woff2', '.woff': 'font/woff', '.json': 'application/json',
        '.ico': 'image/x-icon', '.ttf': 'font/ttf'}
BIG = {}


def big(mb: int) -> bytes:
    if mb not in BIG:
        row = b'{"x":%d,"y":%d,"ground":"meadow","height":%d,"owner":null},\n'
        out, i = bytearray(b'['), 0
        while len(out) < mb * 1024 * 1024:
            out += row % (i % 512, i // 512, (i * 37) % 900)
            i += 1
        BIG[mb] = bytes(out[:mb * 1024 * 1024])
    return BIG[mb]


def log(**fields):
    print(json.dumps(fields), flush=True)


def handler(args):
    dist = Path(args.dist).resolve() if args.dist else None
    studio = urllib.parse.urlsplit(args.studio) if args.studio else None

    class Handler(BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        def log_message(self, *a):
            pass

        def send(self, status, body: bytes, ctype='application/json', extra=None):
            self.send_response(status)
            self.send_header('Content-Type', ctype)
            self.send_header('Content-Length', str(len(body)))
            for k, v in (extra or {}).items():
                self.send_header(k, v)
            self.end_headers()
            self.wfile.write(body)

        def note(self, **more):
            log(peer=self.client_address[0], port=self.server.server_address[1], method=self.command, path=self.path,
                tls=self.connection.version(), auth=bool(self.headers.get('Authorization')),
                origin=self.headers.get('Origin'), cookie=bool(self.headers.get('Cookie')),
                sec_fetch=[h for h in self.headers if h.lower().startswith('sec-fetch')], **more)

        def do_GET(self):
            url = urllib.parse.urlsplit(self.path)
            q = urllib.parse.parse_qs(url.query)
            self.note()
            if url.path == '/spike/hello':
                return self.send(200, json.dumps({'ok': True, 'peer': self.client_address[0]}).encode())
            if url.path == '/spike/big':
                body = big(int(q.get('mb', ['1'])[0]))
                crc = str(zlib.crc32(body))
                if q.get('gz', ['0'])[0] == '1' and 'gzip' in (self.headers.get('Accept-Encoding') or ''):
                    wire = gzip.compress(body, 6)
                    return self.send(200, wire, extra={'Content-Encoding': 'gzip', 'X-Spike-Crc': crc,
                                                       'X-Spike-Wire': str(len(wire))})
                return self.send(200, body, extra={'X-Spike-Crc': crc, 'X-Spike-Wire': str(len(body))})
            if url.path in ('/spike/download.json', '/spike/download.exe'):
                return self.send(200, b'{"spike": "download"}', 'application/octet-stream',
                                 {'Content-Disposition': f'attachment; filename="{url.path.rsplit("/", 1)[1]}"'})
            if url.path.startswith('/api/'):
                return self.forward()
            if dist:
                rel = url.path.lstrip('/') or 'index.html'
                file = (dist / rel).resolve()
                if dist in file.parents and file.is_file() and file.suffix in MIME:
                    return self.send(200, file.read_bytes(), MIME[file.suffix])
            self.send(404, b'{"error":"not found"}')

        def do_POST(self):
            url = urllib.parse.urlsplit(self.path)
            if 'chunked' in (self.headers.get('Transfer-Encoding') or '').lower():
                self.note(refused='chunked')
                self.close_connection = True
                return self.send(411, b'{"error":"chunked bodies are refused"}')
            length = self.headers.get('Content-Length')
            if length is None:
                self.note(refused='no Content-Length')
                self.close_connection = True
                return self.send(411, b'{"error":"Content-Length is required"}')
            n = int(length)
            t = time.perf_counter()
            body = self.rfile.read(n)
            self.note(content_length=n, read=len(body), content_type=self.headers.get('Content-Type'))
            if url.path == '/spike/echo':
                return self.send(200, json.dumps({'bytes': len(body), 'contentLength': n, 'crc': zlib.crc32(body),
                                                  'contentType': self.headers.get('Content-Type'),
                                                  'readMs': round((time.perf_counter() - t) * 1000)}).encode())
            if url.path.startswith('/api/'):
                return self.forward(body)
            self.send(404, b'{"error":"not found"}')

        def forward(self, body=None):
            if not studio:
                return self.send(401, b'{"error":"Sign in to the Editor (spike stub)."}')
            conn = http.client.HTTPConnection(studio.hostname, studio.port, timeout=60)
            here = f'{studio.hostname}:{studio.port}'
            headers = {'Host': here, 'Origin': f'http://{here}', 'Accept-Encoding': self.headers.get('Accept-Encoding', '')}
            if self.headers.get('Content-Type'):
                headers['Content-Type'] = self.headers['Content-Type']
            conn.request(self.command, self.path, body=body, headers=headers)
            answer = conn.getresponse()
            data = answer.read()
            extra = {'Content-Encoding': answer.getheader('Content-Encoding')} if answer.getheader('Content-Encoding') else {}
            self.send(answer.status, data, answer.getheader('Content-Type') or 'application/json', extra)

    return Handler


def serve(args, port, version):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.minimum_version = ctx.maximum_version = version
    ctx.load_cert_chain(args.cert, args.key)
    server = ThreadingHTTPServer((args.bind, port), handler(args))
    server.daemon_threads = True
    server.socket = ctx.wrap_socket(server.socket, server_side=True)
    log(listening=port, tls=str(version))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--cert', required=True)
    p.add_argument('--key', required=True)
    p.add_argument('--bind', default='127.0.0.1')
    p.add_argument('--port', type=int, default=47891)
    p.add_argument('--port12', type=int, default=47892)
    p.add_argument('--dist')
    p.add_argument('--studio')
    p.add_argument('--for', dest='seconds', type=int, default=900)
    args = p.parse_args()
    big(32), big(33)
    serve(args, args.port, ssl.TLSVersion.TLSv1_3)
    serve(args, args.port12, ssl.TLSVersion.TLSv1_2)
    time.sleep(args.seconds)


if __name__ == '__main__':
    main()
