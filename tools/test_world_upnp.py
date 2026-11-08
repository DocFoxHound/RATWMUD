#!/usr/bin/env python3
"""The world's UPnP attempt (tools/world_upnp.py; Docs/Design/77, section 8) against a fake router on loopback: an SSDP
responder (UDP) and an IGD (HTTP: the description and a WANIPConnection control URL), both on 127.0.0.1. The mapping
made, renewed and removed; a lease the router won't take; carrier-grade NAT and a private internet address; another
computer's forward, the host's own, a leftover of ours and one of ours to this computer's old address; a lost reply to
AddPortMapping; refusals; a redirect, an oversized or entity-laden description (UTF-16 too), a control URL elsewhere,
a slow router and one that trickles its headers; text from the router that would forge a log line; the replies a
search must ignore; the route table's verdict; the agent's hook and the compose service's. Standard library only; never sends a search to the real network.

  python3 tools/test_world_upnp.py
"""
from __future__ import annotations

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import importlib.util
import io
import contextlib
import os
from pathlib import Path
import re
import socket
import sys
import tempfile
import threading
import time
import unittest

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import world_upnp as U  # noqa: E402
import world_agent  # noqa: E402

SERVICE = 'urn:schemas-upnp-org:service:WANIPConnection:1'
U.ANSWER_GRACE = 0.05                           # (One fake router: no need to wait for others.)
NO_ROUTES = Path('/nonexistent/fin5-routes')     # (No route table: as on a computer without /proc/net/route.)


def description(control: str = '/ctl/IPConn', base: str | None = None, extra: str = '') -> bytes:
    return (f'<?xml version="1.0"?>{extra}<root xmlns="urn:schemas-upnp-org:device-1-0">'
            + (f'<URLBase>{base}</URLBase>' if base else '') +
            '<device><deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType><deviceList><device>'
            '<deviceType>urn:schemas-upnp-org:device:WANDevice:1</deviceType><deviceList><device>'
            '<deviceType>urn:schemas-upnp-org:device:WANConnectionDevice:1</deviceType><serviceList><service>'
            f'<serviceType>{SERVICE}</serviceType><serviceId>urn:upnp-org:serviceId:WANIPConn1</serviceId>'
            f'<controlURL>{control}</controlURL><eventSubURL>/evt/IPConn</eventSubURL><SCPDURL>/WANIPCn.xml</SCPDURL>'
            '</service></serviceList></device></deviceList></device></deviceList></device></root>').encode()


def fault(code: int, text: str) -> bytes:
    return ('<?xml version="1.0"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"><s:Body><s:Fault>'
            '<faultcode>s:Client</faultcode><faultstring>UPnPError</faultstring><detail>'
            '<UPnPError xmlns="urn:schemas-upnp-org:control-1-0">'
            f'<errorCode>{code}</errorCode><errorDescription>{text}</errorDescription></UPnPError></detail>'
            '</s:Fault></s:Body></s:Envelope>').encode()


def answer(action: str, args: dict) -> bytes:
    inner = ''.join(f'<{k}>{v}</{k}>' for k, v in args.items())
    return ('<?xml version="1.0"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"><s:Body>'
            f'<u:{action}Response xmlns:u="{SERVICE}">{inner}</u:{action}Response></s:Body></s:Envelope>').encode()


def free_port() -> int:
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


class Trickle:
    """An HTTP server on 127.0.0.1 that sends `head` at once, then one more byte every `every` seconds, for ever: a
    device that never finishes its status line, headers or chunk size."""

    def __init__(self, head: bytes, every: float = 0.1):
        self.sock = socket.socket()
        self.sock.bind(('127.0.0.1', 0))
        self.sock.listen()
        self.port = self.sock.getsockname()[1]
        self.head, self.every, self.done = head, every, threading.Event()
        threading.Thread(target=self.serve, daemon=True).start()

    def serve(self):
        while not self.done.is_set():
            try:
                conn, _ = self.sock.accept()
            except OSError:
                return
            threading.Thread(target=self.drip, args=(conn,), daemon=True).start()

    def drip(self, conn):
        with conn:
            try:
                conn.recv(65536)
                conn.sendall(self.head)
                while not self.done.wait(self.every):
                    conn.sendall(b'a')
            except OSError:
                return

    def close(self):
        self.done.set()
        self.sock.close()


def field(body: str, name: str) -> str:
    m = re.search(f'<{name}>(.*?)</{name}>', body, re.S)
    return m.group(1) if m else ''


class Router:
    """A fake IGD on 127.0.0.1: `ssdp` answers searches (LOCATION from `location()`), `http` serves the description and
    the control URL. `mappings` holds {port: {...}}; `calls` every SOAP action, in order; knobs below."""

    def __init__(self):
        self.external = '81.2.69.160'
        self.mappings: dict[int, dict] = {}
        self.calls: list[tuple[str, str]] = []
        self.fetched: list[str] = []
        self.permanent_only = False
        self.refuse: dict[str, tuple[int, str]] = {}   # action -> (errorCode, text)
        self.desc = lambda: description()
        self.desc_status = 200
        self.desc_headers: dict = {}
        self.slow = 0.0
        self.lose_reply: set[str] = set()              # Actions done but never answered (the connection just closes).
        self.location = lambda: f'http://127.0.0.1:{self.port}/rootDesc.xml'
        self.ssdp_replies = None                       # A function giving the datagrams to send, if set.
        router = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def send(self, status, body, headers=None):
                self.send_response(status)
                for k, v in (headers or {}).items():
                    self.send_header(k, v)
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                try:
                    self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError):
                    pass                                # (The client gave up on a too-large answer: as it should.)

            def do_GET(self):
                router.fetched.append(self.path)
                if router.slow:
                    time.sleep(router.slow)
                if self.path == '/rootDesc.xml':
                    self.send(router.desc_status, router.desc(), router.desc_headers)
                else:
                    self.send(404, b'')

            def do_POST(self):
                body = self.rfile.read(int(self.headers.get('Content-Length') or 0)).decode()
                action = (self.headers.get('SOAPAction') or '').strip('"').rsplit('#', 1)[-1]
                router.calls.append((action, body))
                if self.path != '/ctl/IPConn':
                    return self.send(404, b'')
                if action in router.refuse:
                    return self.send(500, fault(*router.refuse[action]))
                port = int(field(body, 'NewExternalPort') or 0)
                if action == 'GetExternalIPAddress':
                    return self.send(200, answer(action, {'NewExternalIPAddress': router.external}))
                if action == 'GetSpecificPortMappingEntry':
                    m = router.mappings.get(port)
                    if not m:
                        return self.send(500, fault(714, 'NoSuchEntryInArray'))
                    return self.send(200, answer(action, {
                        'NewInternalPort': m['inner'], 'NewInternalClient': m['client'], 'NewEnabled': 1,
                        'NewPortMappingDescription': m['description'], 'NewLeaseDuration': m['lease']}))
                if action == 'AddPortMapping':
                    lease = int(field(body, 'NewLeaseDuration') or 0)
                    if router.permanent_only and lease:
                        return self.send(500, fault(725, 'OnlyPermanentLeasesSupported'))
                    router.mappings[port] = {'inner': field(body, 'NewInternalPort'),
                                             'client': field(body, 'NewInternalClient'),
                                             'description': field(body, 'NewPortMappingDescription'), 'lease': lease}
                    if action in router.lose_reply:
                        self.close_connection = True
                        return
                    return self.send(200, answer(action, {}))
                if action == 'DeletePortMapping':
                    if router.mappings.pop(port, None) is None:
                        return self.send(500, fault(714, 'NoSuchEntryInArray'))
                    return self.send(200, answer(action, {}))
                self.send(500, fault(401, 'Invalid Action'))

        self.http = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.port = self.http.server_address[1]
        threading.Thread(target=self.http.serve_forever, daemon=True).start()
        self.udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp.bind(('127.0.0.1', 0))
        self.ssdp = self.udp.getsockname()
        self.searches: list[bytes] = []
        threading.Thread(target=self.answer_searches, daemon=True).start()

    def answer_searches(self):
        while True:
            try:
                data, source = self.udp.recvfrom(4096)
            except OSError:
                return
            self.searches.append(data)
            replies = self.ssdp_replies() if self.ssdp_replies else [
                (f'HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=120\r\nST: {SERVICE}\r\nUSN: uuid:fake::{SERVICE}\r\n'
                 f'LOCATION: {self.location()}\r\nSERVER: Fake/1 UPnP/1.1 MiniUPnPd/2.3\r\n\r\n').encode()]
            for r in replies:
                try:
                    self.udp.sendto(r, source)
                except OSError:
                    pass

    def close(self):
        self.http.shutdown()
        self.http.server_close()
        self.udp.close()

    def actions(self):
        return [a for a, _ in self.calls]


class UpnpTests(unittest.TestCase):
    def setUp(self):
        self.router = Router()
        self.lines: list[str] = []
        self.addCleanup(self.router.close)

    def open(self, port=7788, name='Wolf Hollow', **kwargs):
        options = {'target': self.router.ssdp, 'loopback_ok': True, 'container': False, 'budget': 6.0,
                   'routes': NO_ROUTES}
        options.update(kwargs)
        return U.open_mapping(port, name, self.lines.append, **options)

    def said(self, *texts):
        self.assertEqual(len(self.lines), 1, self.lines)
        for t in texts:
            self.assertIn(t, self.lines[0])

    def test_the_port_is_mapped_named_after_the_world_and_removed_on_a_clean_stop(self):
        mapping = self.open()
        self.assertIsNotNone(mapping)
        self.said('UPnP: the router (127.0.0.1) now forwards port 7788 (TCP) to this computer (127.0.0.1)',
                  'for an hour at a time', 'its internet address is 81.2.69.160', 'call-back')
        self.assertEqual(self.router.mappings[7788], {'inner': '7788', 'client': '127.0.0.1',
                                                      'description': 'RATW Wolf Hollow', 'lease': 3600})
        self.assertEqual(self.router.actions(), ['GetExternalIPAddress', 'GetSpecificPortMappingEntry',
                                                 'AddPortMapping'])
        add = self.router.calls[-1][1]
        self.assertIn('<NewProtocol>TCP</NewProtocol>', add)
        self.assertIn('<NewRemoteHost></NewRemoteHost>', add)
        self.assertTrue(all(b'ST: urn:schemas-upnp-org:' in s for s in self.router.searches))
        mapping.close()
        mapping.close()                                  # (Twice: once.)
        self.assertEqual(self.router.mappings, {})
        self.assertEqual(self.router.actions()[-1], 'DeletePortMapping')
        self.assertEqual(self.router.actions().count('DeletePortMapping'), 1)
        self.assertIn('no longer forwards port 7788', self.lines[-1])

    def test_the_lease_is_renewed_at_half_time_and_a_failure_is_said_once(self):
        mapping = self.open()
        mapping.step(time.monotonic())                   # (Not due yet.)
        self.assertEqual(self.router.actions().count('AddPortMapping'), 1)
        mapping.step(time.monotonic() + 1801)
        self.assertEqual(self.router.actions().count('AddPortMapping'), 2)
        self.router.refuse['AddPortMapping'] = (501, 'ActionFailed')
        for later in (3700, 4100, 4500):
            mapping.next_renew = 0
            mapping.step(time.monotonic() + later)
        self.assertEqual(sum('could not renew' in l for l in self.lines), 1)
        del self.router.refuse['AddPortMapping']
        mapping.next_renew = 0
        mapping.step()
        self.assertIn('forwards port 7788 again', self.lines[-1])

    def test_a_router_with_only_permanent_mappings_gets_one(self):
        self.router.permanent_only = True
        mapping = self.open()
        self.assertEqual(mapping.lease, 0)
        self.assertEqual(self.router.mappings[7788]['lease'], 0)
        self.said('until the world stops cleanly')
        mapping.step(time.monotonic() + 10 ** 6)         # (Nothing to renew.)
        self.assertEqual(self.router.actions().count('AddPortMapping'), 2)
        mapping.close()
        self.assertEqual(self.router.mappings, {})

    def test_carrier_grade_nat_and_a_private_internet_address_map_nothing(self):
        for external, says in (('100.64.12.34', 'in 100.64.0.0/10'), ('100.127.255.1', 'in 100.64.0.0/10'),
                                ('192.168.1.254', 'is private'), ('10.20.30.40', 'is private')):
            self.lines.clear()
            self.router.external = external
            self.assertIsNone(self.open())
            self.said('behind carrier-grade NAT', says)
            self.assertNotIn('AddPortMapping', self.router.actions())

    def test_an_unknown_internet_address_still_maps(self):
        self.router.external = ''
        self.assertIsNotNone(self.open())
        self.said('didn\'t say its internet address')

    def test_no_answer_says_to_forward_by_hand_and_why_in_a_container(self):
        silent = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        silent.bind(('127.0.0.1', 0))
        self.addCleanup(silent.close)
        started = time.monotonic()
        self.assertIsNone(self.open(target=silent.getsockname(), container=True))
        self.assertLess(time.monotonic() - started, 4)
        self.said('no router answered the search from inside this container', 'rootless Docker',
                  'Forward port 7788 (TCP) on your router to this computer by hand')
        self.lines.clear()
        self.assertIsNone(self.open(target=silent.getsockname(), container=False))
        self.said('UPnP may be off on the router', 'by hand')

    def test_another_computers_forward_is_never_touched(self):
        self.router.mappings[7788] = {'inner': '7788', 'client': '192.168.0.50', 'description': 'xbox', 'lease': 0}
        self.assertIsNone(self.open())
        self.said('already forwarded on the router to another computer (192.168.0.50)', 'by hand')
        self.assertNotIn('AddPortMapping', self.router.actions())
        self.assertEqual(self.router.mappings[7788]['client'], '192.168.0.50')

    def test_the_hosts_own_forward_is_left_and_never_removed(self):
        self.router.mappings[7788] = {'inner': '7788', 'client': '127.0.0.1', 'description': 'my wolves', 'lease': 0}
        self.assertIsNone(self.open())
        self.said('already forwarded to this computer', '"my wolves"', 'left as it is')
        self.assertNotIn('AddPortMapping', self.router.actions())
        self.assertNotIn('DeletePortMapping', self.router.actions())

    def test_a_leftover_of_ours_is_taken_up_and_removed(self):
        self.router.mappings[7788] = {'inner': '7788', 'client': '127.0.0.1', 'description': 'RATW Old Name',
                                      'lease': 0}
        mapping = self.open()
        self.assertIsNotNone(mapping)
        self.assertEqual(self.router.mappings[7788]['description'], 'RATW Wolf Hollow')
        mapping.close()
        self.assertEqual(self.router.mappings, {})

    def test_our_forward_to_this_computers_old_address_is_replaced_only_if_nothing_answers_there(self):
        port = free_port()                               # (Not 7788: a real world on this computer may have it.)
        self.router.mappings[port] = {'inner': str(port), 'client': '127.0.0.2', 'description': 'RATW Wolf Hollow',
                                      'lease': 0}
        mapping = self.open(port=port)
        self.assertIsNotNone(mapping)
        self.said(f'now forwards port {port} (TCP) to this computer (127.0.0.1) (in place of this world\'s old '
                  'forward to 127.0.0.2, where nothing answers now)')
        self.assertEqual(self.router.mappings[port]['client'], '127.0.0.1')
        self.assertEqual(self.router.actions()[-2:], ['DeletePortMapping', 'AddPortMapping'])
        mapping.close()
        # Something answers there (another computer's world of the same name, say): left alone.
        listener = socket.socket()
        listener.bind(('127.0.0.2', port))
        listener.listen()
        self.addCleanup(listener.close)
        for description in ('RATW Wolf Hollow', 'RATW Another World'):
            self.lines.clear()
            self.router.calls.clear()
            self.router.mappings[port] = {'inner': str(port), 'client': '127.0.0.2', 'description': description,
                                          'lease': 0}
            self.assertIsNone(self.open(port=port))
            self.said('already forwarded on the router to another computer (127.0.0.2)')
            self.assertNotIn('AddPortMapping', self.router.actions())
            self.assertNotIn('DeletePortMapping', self.router.actions())
        # Another name, with nothing answering: still not ours.
        listener.close()
        self.lines.clear()
        self.assertIsNone(self.open(port=port))
        self.assertEqual(self.router.mappings[port]['description'], 'RATW Another World')

    def test_a_lost_reply_to_add_port_mapping_asks_whether_it_took(self):
        self.router.lose_reply = {'AddPortMapping'}
        mapping = self.open()
        self.assertIsNotNone(mapping)                    # (It took: kept, renewed and removed as usual.)
        self.said('now forwards port 7788')
        self.assertEqual(self.router.actions(), ['GetExternalIPAddress', 'GetSpecificPortMappingEntry',
                                                 'AddPortMapping', 'GetSpecificPortMappingEntry'])
        mapping.close()
        self.assertEqual(self.router.mappings, {})

    def test_text_from_the_router_cannot_forge_a_log_line(self):
        forged = 'x\nUPnP: the router\u2028 now forwards port 7788&#9;y'
        self.router.refuse = {'AddPortMapping': (501, forged)}
        self.assertIsNone(self.open())
        self.said('refused AddPortMapping (error 501: xUPnP: the router now forwards port 7788y)')
        self.assertNotIn('\n', self.lines[0])
        self.assertNotIn('\u2028', self.lines[0])
        self.lines.clear()
        self.router.refuse = {}
        self.router.mappings[7788] = {'inner': '7788\r\nUPnP: ok', 'client': '192.168.0.50\nUPnP: fine',
                                      'description': 'x', 'lease': 0}
        self.assertIsNone(self.open())
        self.said('another computer (192.168.0.50UPnP: fine)')
        self.assertEqual(U.clean('a\u2028b\tc\u00e9d', 10), 'abcd')

    def test_refusals_say_why(self):
        for action, code, says in (('AddPortMapping', 718, 'already forwarded on the router to another computer'),
                                   ('AddPortMapping', 606, 'UPnP may be read-only'),
                                   ('AddPortMapping', 501, 'refused AddPortMapping (error 501: Oops)'),
                                   ('GetExternalIPAddress', 501, 'would not say its internet address'),
                                   ('GetSpecificPortMappingEntry', 606, 'refused GetSpecificPortMappingEntry')):
            self.lines.clear()
            self.router.refuse = {action: (code, 'Oops')}
            self.assertIsNone(self.open())
            self.said(says, 'by hand')

    def test_the_name_is_cut_and_escaped(self):
        self.assertEqual(U.description_for('A\x00b\n<c>', 7788), 'RATW Ab<c>')
        self.assertEqual(U.description_for('', 7790), 'RATW world on port 7790')
        self.assertEqual(len(U.description_for('x' * 200, 7788)), U.DESCRIPTION_CHARS)
        self.open(name='Fang & <Claw>')
        add = self.router.calls[-1][1]
        self.assertIn('RATW Fang &amp; &lt;Claw&gt;', add)
        self.assertEqual(self.router.mappings[7788]['description'], 'RATW Fang &amp; &lt;Claw&gt;')   # (As sent.)


class TrustTests(unittest.TestCase):
    """What a search, a description and a control URL may point at."""

    def setUp(self):
        self.router = Router()
        self.lines: list[str] = []
        self.addCleanup(self.router.close)

    def open(self, **kwargs):
        options = {'target': self.router.ssdp, 'loopback_ok': True, 'container': False, 'budget': 6.0,
                   'routes': NO_ROUTES}
        options.update(kwargs)
        return U.open_mapping(7788, 'Wolf Hollow', self.lines.append, **options)

    def test_only_private_gateways_are_answered(self):
        for text, ok in (('192.168.0.1', True), ('10.0.0.138', True), ('172.16.0.1', True), ('172.31.255.1', True),
                         ('172.32.0.1', False), ('100.64.0.1', False), ('8.8.8.8', False), ('169.254.1.1', False),
                         ('127.0.0.1', False), ('::1', False), ('fe80::1', False), ('bogus', False)):
            self.assertEqual(U.gateway_address(text) is not None, ok, text)
        self.assertIsNotNone(U.gateway_address('127.0.0.1', loopback_ok=True))
        # Loopback (this fake) isn't answered outside the tests.
        self.assertEqual(U.discover(1.0, self.router.ssdp, loopback_ok=False), [])

    def test_a_location_elsewhere_or_by_name_is_ignored(self):
        for location in ('http://127.0.0.2:{p}/rootDesc.xml', 'http://localhost:{p}/rootDesc.xml',
                         'https://127.0.0.1:{p}/rootDesc.xml', 'http://user:pw@127.0.0.1:{p}/rootDesc.xml',
                         'ftp://127.0.0.1/x', 'http://127.0.0.1:99999/x', ''):
            self.router.location = lambda location=location: location.format(p=self.router.port)
            self.assertEqual(U.discover(0.4, self.router.ssdp, loopback_ok=True), [], location)
        self.assertEqual(self.router.fetched, [])

    def test_odd_search_replies_are_ignored(self):
        good = (f'HTTP/1.1 200 OK\r\nLOCATION: http://127.0.0.1:{self.router.port}/rootDesc.xml\r\n\r\n').encode()
        self.router.ssdp_replies = lambda: [b'NOTIFY * HTTP/1.1\r\nLOCATION: http://127.0.0.1/\r\n\r\n',
                                            b'HTTP/1.1 404 Not Found\r\n\r\n', b'\xff' * 3000, b'', good]
        found = U.discover(1.0, self.router.ssdp, loopback_ok=True)
        self.assertEqual(found, [('127.0.0.1', f'http://127.0.0.1:{self.router.port}/rootDesc.xml')])

    def test_a_redirect_is_never_followed(self):
        self.router.desc_status = 302
        self.router.desc_headers = {'Location': f'http://127.0.0.1:{self.router.port}/elsewhere'}
        self.assertIsNone(self.open())
        self.assertEqual(self.router.fetched, ['/rootDesc.xml'])
        self.assertIn('HTTP 302', self.lines[0])

    def test_a_control_url_elsewhere_is_never_called(self):
        for desc in (description(control='http://10.9.9.9:5000/ctl/IPConn'),
                     description(control='/ctl/IPConn', base='http://192.168.77.1:5000/'),
                     description(control='//127.0.0.2/ctl/IPConn')):
            self.lines.clear()
            self.router.desc = lambda desc=desc: desc
            self.assertIsNone(self.open())
            self.assertIn('offers no internet connection service', self.lines[0])
        self.assertEqual(self.router.calls, [])

    def test_large_or_entity_laden_descriptions_are_refused(self):
        utf16 = '<?xml version="1.0" encoding="UTF-16"?><!DOCTYPE r [<!ENTITY e "injected">]><r>&e;</r>'.encode('utf-16')
        with self.assertRaisesRegex(U.UpnpError, 'DOCTYPE or an ENTITY'):
            U.parse_xml(utf16, 'description')            # (The byte search can't see it: the parser does.)
        with self.assertRaisesRegex(U.UpnpError, 'DOCTYPE or an ENTITY'):
            U.parse_xml('<?xml version="1.0" encoding="UTF-16"?><!DOCTYPE r><r/>'.encode('utf-16'), 'description')
        self.assertEqual(U.parse_xml('<r>ok</r>'.encode('utf-16'), 'description').text, 'ok')
        for desc, says in ((description(extra='<!-- ' + 'x' * (U.MAX_DESCRIPTION + 10) + ' -->'), 'too large'),
                           (description(extra='<!DOCTYPE r [<!ENTITY a "aaaa">]>'), 'DOCTYPE or an ENTITY'),
                           (utf16, 'DOCTYPE or an ENTITY'),
                           (b'<root><not closed', 'not XML')):
            self.lines.clear()
            self.router.desc = lambda desc=desc: desc
            self.assertIsNone(self.open())
            self.assertIn(says, self.lines[0])
        self.assertEqual(self.router.calls, [])

    def test_a_slow_router_is_given_up_on_in_time(self):
        self.router.slow = 5
        started = time.monotonic()
        self.assertIsNone(self.open())
        self.assertLess(time.monotonic() - started, 6)
        self.assertIn('did not answer in time', self.lines[0])

    def test_a_router_trickling_its_headers_is_cut_off_in_time(self):
        """A socket timeout is for each recv alone: a byte every 0.1 s would keep http.client reading for ever."""
        for head in (b'HTTP/1.1 200 OK\r\nX: ', b'HTTP/1.1 2',
                     b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1'):
            trickle = Trickle(head)
            self.addCleanup(trickle.close)
            url = f'http://127.0.0.1:{trickle.port}/rootDesc.xml'
            started = time.monotonic()
            with self.assertRaisesRegex(U.UpnpError, 'in time|too slowly'):
                U.request(url, '127.0.0.1', seconds=0.5)
            self.assertLess(time.monotonic() - started, 1.5, head)
            # The whole attempt, the description at such a device: within its budget.
            self.lines.clear()
            self.router.location = lambda url=url: url
            started = time.monotonic()
            self.assertIsNone(self.open(budget=2.0))
            self.assertLess(time.monotonic() - started, 3.5, head)
            self.assertIn('offers no internet connection service', self.lines[0])
            self.assertIn('in time', self.lines[0])

    def test_a_bug_never_stops_the_world(self):
        original = U.services
        U.services = lambda *a: 1 / 0
        try:
            self.assertIsNone(self.open())
        finally:
            U.services = original
        self.assertIn('failed unexpectedly (ZeroDivisionError', self.lines[0])

    def test_the_route_table_tells_a_container_from_the_host(self):
        with tempfile.TemporaryDirectory(prefix='fin5-upnp-') as folder:
            routes = Path(folder) / 'route'
            head = 'Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n'
            le = lambda a: bytes(reversed(socket.inet_aton(a))).hex().upper() if sys.byteorder == 'little' else \
                socket.inet_aton(a).hex().upper()   # noqa: E731
            host = head + (f'wlo1\t00000000\t{le("192.168.0.1")}\t0003\t0\t0\t600\t00000000\t0\t0\t0\n'
                           f'wlo1\t{le("192.168.0.0")}\t00000000\t0001\t0\t0\t600\t{le("255.255.255.0")}\t0\t0\t0\n')
            routes.write_text(host)
            self.assertIs(U.on_link('192.168.0.1', routes), True)
            self.assertIs(U.on_link('8.8.8.8', routes), False)
            bridge = head + (f'eth0\t00000000\t{le("172.17.0.1")}\t0003\t0\t0\t0\t00000000\t0\t0\t0\n'
                             f'eth0\t{le("172.17.0.0")}\t00000000\t0001\t0\t0\t0\t{le("255.255.0.0")}\t0\t0\t0\n')
            routes.write_text(bridge)
            self.assertIs(U.on_link('192.168.0.1', routes), False)
            self.assertIsNone(U.on_link('192.168.0.1', Path(folder) / 'none'))

    def test_a_computer_behind_another_hop_maps_nothing(self):
        """As inside a container: the route table reaches the router through a gateway, not on this network."""
        with tempfile.TemporaryDirectory(prefix='fin5-upnp-') as folder:
            routes = Path(folder) / 'route'
            routes.write_text('Iface\tDestination\tGateway\tFlags\tRefCnt\tUse\tMetric\tMask\tMTU\tWindow\tIRTT\n'
                              'eth0\t00000000\t010011AC\t0003\t0\t0\t0\t00000000\t0\t0\t0\n')
            self.assertIsNone(self.open(routes=routes, container=True))
        self.assertIn('reaches the router (127.0.0.1) through another hop (this is a container', self.lines[0])
        self.assertIn('by hand', self.lines[0])
        self.assertNotIn('AddPortMapping', self.router.actions())


class HookTests(unittest.TestCase):
    """The agent's hook (world_agent.Upnp) and the bundle's (compose.yaml's upnp service, world.toml, entrypoint)."""

    def setUp(self):
        self.router = Router()
        self.addCleanup(self.router.close)

    def test_the_agent_maps_before_its_first_heartbeat_and_removes_after_its_leave(self):
        lines = []
        with tempfile.TemporaryDirectory(prefix='fin5-upnp-') as folder:
            a = world_agent.Agent('http://127.0.0.1:9', Path(folder), free_port(), 'rh_x', every=0.2, log=lines.append,
                                  upnp_port=7788, upnp_name='Wolf Hollow')
            a.upnp.options = {'target': self.router.ssdp, 'loopback_ok': True, 'container': False, 'budget': 6.0,
                              'routes': NO_ROUTES}
            order = []
            beat = a.heartbeat
            a.heartbeat = lambda: (order.append(dict(self.router.mappings)), beat())[1]
            a.keys.step = lambda now: None
            thread = threading.Thread(target=a.run)
            thread.start()
            deadline = time.monotonic() + 8
            while not order and time.monotonic() < deadline:
                time.sleep(0.05)
            a.stop.set()
            thread.join(10)
        self.assertTrue(order and 7788 in order[0], order)        # (Mapped before the first heartbeat.)
        self.assertEqual(self.router.mappings, {})
        self.assertTrue(any(l.startswith('world agent: UPnP: the router (127.0.0.1) now forwards port 7788') for l in lines))
        self.assertTrue(any('world agent: UPnP: the router no longer forwards port 7788' in l for l in lines))

    def test_a_trickling_device_holds_back_the_first_heartbeat_no_longer_than_the_budget(self):
        trickle = Trickle(b'HTTP/1.1 200 OK\r\nX: ')
        self.addCleanup(trickle.close)
        self.router.location = lambda: f'http://127.0.0.1:{trickle.port}/rootDesc.xml'
        lines, beats = [], []
        with tempfile.TemporaryDirectory(prefix='fin5-upnp-') as folder:
            a = world_agent.Agent('http://127.0.0.1:9', Path(folder), free_port(), 'rh_x', every=0.2, log=lines.append,
                                  upnp_port=7788, upnp_name='Wolf Hollow')
            a.upnp.options = {'target': self.router.ssdp, 'loopback_ok': True, 'container': False, 'budget': 2.0,
                              'routes': NO_ROUTES}
            a.heartbeat = lambda: beats.append(time.monotonic())
            a.keys.step = lambda now: None
            started = time.monotonic()
            thread = threading.Thread(target=a.run)
            thread.start()
            while not beats and time.monotonic() < started + 8:
                time.sleep(0.05)
            a.stop.set()
            thread.join(10)
        self.assertTrue(beats, lines)
        self.assertLess(beats[0] - started, 3.5)
        self.assertTrue(any('world agent: UPnP: a router answered but offers no internet connection service' in l
                            for l in lines), lines)

    def test_no_port_no_attempt(self):
        u = world_agent.Upnp(None, 'x', print)
        u.open()
        u.step()
        u.close()
        self.assertEqual(self.router.searches, [])

    def test_hold_with_upnp_off_waits_and_says_so(self):
        with tempfile.TemporaryDirectory(prefix='fin5-upnp-') as folder:
            settings = Path(folder) / 'world.toml'
            settings.write_bytes(b'\xef\xbb\xbf[world]\nname = "Wolf Hollow"\n[network]\nupnp = false\n')
            self.assertEqual(U.settings_name(settings), ('Wolf Hollow', False))
            settings.write_text('[world]\nname = "Wolf Hollow"\n')
            self.assertEqual(U.settings_name(settings), ('Wolf Hollow', True))
            settings.write_text('[network]\nupnp = "yes"\n')
            with self.assertRaises(ValueError):
                U.settings_name(settings)

    def test_hold_maps_and_removes_on_stop(self):
        stop, lines = threading.Event(), []
        thread = threading.Thread(target=U.hold, args=(7788, 'Wolf Hollow', lines.append, stop),
                                  kwargs={'target': self.router.ssdp, 'loopback_ok': True, 'container': False,
                                          'routes': NO_ROUTES})
        thread.start()
        deadline = time.monotonic() + 8
        while 7788 not in self.router.mappings and time.monotonic() < deadline:
            time.sleep(0.05)
        self.assertIn(7788, self.router.mappings)
        stop.set()
        thread.join(10)
        self.assertEqual(self.router.mappings, {})

    def test_the_bundle_runs_it_from_the_hosts_network_with_world_toml_alone(self):
        compose = (HERE.parent / 'Package' / 'compose.yaml').read_text()
        service = compose[compose.index('\n  upnp:\n'):compose.index('\n  db:\n')]
        self.assertIn('network_mode: host', service)
        self.assertIn('"/opt/ratw/tools/world_upnp.py", "hold", "--port", "${RATW_PORT:-7788}"', service)
        self.assertIn('        source: ./world.toml\n        target: /etc/ratw-upnp/world.toml\n        read_only: true\n'
                      '        bind:\n          create_host_path: false\n', service)   # (No folder made for a missing one.)
        self.assertIn('cap_drop: [ALL]', service)
        self.assertIn('cap_add: [DAC_OVERRIDE]', service)          # (Never DAC_READ_SEARCH: open_by_handle_at.)
        self.assertNotIn('DAC_READ_SEARCH]', service)
        self.assertNotIn('secrets', service)           # (No passwords, token or key.)
        self.assertNotIn('ports:', service)
        self.assertNotIn('- ./:', service)
        toml = (HERE.parent / 'Package' / 'world.toml').read_text()
        self.assertIn('[network]\n', toml)
        self.assertIn('\nupnp = true\n', toml)

    def test_the_entrypoint_checks_the_setting_and_says_where_to_look(self):
        os.environ.setdefault('RATW_APP_DIR', str(HERE.parent))
        spec = importlib.util.spec_from_file_location('package_entrypoint_upnp', HERE.parent / 'Package' / 'entrypoint.py')
        E = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(E)
        with tempfile.TemporaryDirectory(prefix='fin5-upnp-') as folder:
            E.SETTINGS = Path(folder) / 'world.toml'
            base = ('[world]\nname = "Wolf Hollow"\nid = "world-' + '0123456789abcdef' * 2 + '"\n'
                    '[central]\nurl = "https://central.example.org"\n')
            saved = os.environ.pop('RATW_HOST_TOKEN', None)
            try:
                for extra, upnp in (('', True), ('[network]\nupnp = false\n', False), ('[network]\nupnp = true\n', True)):
                    E.SETTINGS.write_text(base + extra)
                    with contextlib.redirect_stdout(io.StringIO()):
                        s = E.read_settings()
                    self.assertIs(s['upnp'], upnp)
                    self.assertIn('docker compose logs upnp' if upnp else 'upnp = false', E.upnp_note(s, 7788))
                E.SETTINGS.write_text(base + '[network]\nupnp = 1\n')
                err = io.StringIO()
                with contextlib.redirect_stderr(err), contextlib.redirect_stdout(io.StringIO()), \
                        self.assertRaises(SystemExit):
                    E.read_settings()
                self.assertIn('upnp must be true or false', err.getvalue())
                # The example world.toml still reads (with an id).
                body = (HERE.parent / 'Package' / 'world.toml').read_text()
                E.SETTINGS.write_text(body.replace('id = ""', 'id = "world-' + 'ab' * 16 + '"'))
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertIs(E.read_settings()['upnp'], True)
            finally:
                if saved is not None:
                    os.environ['RATW_HOST_TOKEN'] = saved


if __name__ == '__main__':
    unittest.main()
