#!/usr/bin/env python3
"""The tools' web servers compress large text for remote browsers that accept it, and nothing else."""
import gzip
import unittest

import http_body


class Handler:
    def __init__(self, peer, accept='gzip, deflate'):
        self.client_address = (peer, 50000)
        self.headers = {'Accept-Encoding': accept}
        self.sent = {}

    def send_header(self, key, value):
        self.sent[key] = value


class BodyTests(unittest.TestCase):
    big = b'{"cells": [' + b'"....~~~~....", ' * 4000 + b'"."]}'

    def test_large_json_to_a_remote_browser_is_gzipped(self):
        handler = Handler('192.168.1.20')
        body = http_body.send(handler, self.big, 'application/json')
        self.assertEqual(handler.sent['Content-Encoding'], 'gzip')
        self.assertEqual(handler.sent['Content-Length'], str(len(body)))
        self.assertLess(len(body), len(self.big) // 10)
        self.assertEqual(gzip.decompress(body), self.big)

    def test_small_local_binary_or_unaccepted_bodies_are_sent_as_they_are(self):
        for handler, body, mime in [(Handler('192.168.1.20'), b'{"ok": true}', 'application/json'),
                                    (Handler('127.0.0.1'), self.big, 'application/json'),
                                    (Handler('192.168.1.20'), self.big, 'application/zip'),
                                    (Handler('192.168.1.20', accept='identity'), self.big, 'application/json')]:
            self.assertEqual(http_body.send(handler, body, mime), body)
            self.assertNotIn('Content-Encoding', handler.sent)
            self.assertEqual(handler.sent['Content-Length'], str(len(body)))


if __name__ == '__main__':
    unittest.main()
