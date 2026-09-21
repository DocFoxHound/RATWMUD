#!/usr/bin/env python3
"""Deterministic loopback-only fixture for Unreal's dialogue adapter tests."""
import argparse
import json
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        size = min(int(self.headers.get("Content-Length", "0")), 32768)
        try:
            context = json.loads(self.rfile.read(size))
        except (ValueError, UnicodeDecodeError):
            context = {}
        case = context.get("heard", "")
        if case == "fixture_timeout":
            time.sleep(10)
        code = 503 if case == "fixture_failure" else 200
        if case == "fixture_malformed":
            body = b"this is not JSON"
        elif case == "fixture_empty":
            body = b'{"text":""}'
        elif case == "fixture_oversize":
            body = json.dumps({"text": "x" * 3000}).encode()
        else:
            body = json.dumps({"text": "A grounded reply from the local test provider."}).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, *_):
        pass


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=18765)
    args = parser.parse_args()
    ThreadingHTTPServer(("127.0.0.1", args.port), Handler).serve_forever()
