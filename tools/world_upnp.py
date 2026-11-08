#!/usr/bin/env python3
"""The world's UPnP attempt (Docs/Design/77-central-accounts-and-hosted-worlds.md, section 8: "UPnP is tried once (it
fails behind carrier-grade NAT), then the call-back says whether it worked"). Standard library only.

Asks the home router, once when the world starts, to forward the game's TCP port to this computer: an SSDP search
(M-SEARCH to 239.255.255.250:1900), the router's description, then its WANIPConnection (or WANPPPConnection) service:
GetExternalIPAddress, GetSpecificPortMappingEntry, AddPortMapping with a lease (an hour, renewed every half hour; a
router that only takes permanent mappings gets one) and a description naming the world, and DeletePortMapping on a
clean stop. It says one plain line: the mapping made, or why there is none and what to do instead. It never retries a
failed attempt (central's call-back, every 10 minutes, is the test that matters); it only renews a mapping it made.

What it trusts, since anything on the network can answer a search:
- only replies from 10/8, 172.16/12 and 192.168/16, whose LOCATION is a plain http:// IPv4 address and the very
  address the reply came from (no names, so no lookups; never another host);
- the description and every SOAP call go to that address only (a control URL elsewhere is refused), by plain
  http.client: a redirect is a failure, never followed; a proxy is never used;
- tight limits: 2.5 s for the search, 3 s per request (its status line, headers and chunk sizes too: every read is
  given only what is left of it, so a router that trickles its answer is cut off in time), 10 s for the whole attempt;
  2 KB per search reply, 16 replies, 256 KB for a description, 32 KB for a SOAP reply; XML with a DOCTYPE or an ENTITY
  is refused (a byte search, then the parser itself, whatever the document's encoding);
- what the router says (its error texts, the address and port of a forward) goes into the log as printable ASCII
  only, so no device on the network can write a line of its own into it;
- a mapping it didn't make (a forward the host set up by hand, or another computer's) is never changed or deleted. A
  mapping named "RATW ..." to this computer and port is taken as its own, from a run that didn't stop cleanly; so is
  one with this world's very description to another address on this network that doesn't answer on the port (this
  computer's address before the router gave it another), which is replaced. If the router's reply to AddPortMapping
  is lost, the router is asked whether it took.

Where it can work: a router on the same network as this computer. A container on Docker's own networks (the bundle's
world), under Docker Desktop (a VM) or under rootless Docker (pasta or slirp4netns, even with network_mode: host)
can't reach the router's search: the search either stays inside, or leaves and its answer never comes back. On Linux
with Docker Engine, a container on the host's network can (compose.yaml's upnp service). Elsewhere the log says to
forward the port by hand.

  python3 tools/world_upnp.py probe
        Discovery only: the search, the router's description and its internet address; changes nothing.
  python3 tools/world_upnp.py hold --port PORT (--name NAME | --settings world.toml)
        Maps PORT, renews it, removes it on SIGTERM/SIGINT; after a failure it says why and waits to be stopped.
        With --settings, the world's name is world.toml's [world] name, and [network] upnp = false stops here.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import http.client
import io
import ipaddress
import json
import os
from pathlib import Path
import re
import signal
import socket
import sys
import threading
import time
import tomllib
import urllib.parse
import xml.etree.ElementTree as ElementTree
from xml.parsers import expat
from xml.sax.saxutils import escape

SSDP = ('239.255.255.250', 1900)
SEARCH_TARGETS = ('urn:schemas-upnp-org:device:InternetGatewayDevice:1',
                  'urn:schemas-upnp-org:device:InternetGatewayDevice:2',
                  'urn:schemas-upnp-org:service:WANIPConnection:1',
                  'urn:schemas-upnp-org:service:WANPPPConnection:1')
SERVICES = ('urn:schemas-upnp-org:service:WANIPConnection:2',     # (In the order they are tried.)
            'urn:schemas-upnp-org:service:WANIPConnection:1',
            'urn:schemas-upnp-org:service:WANPPPConnection:1')
SEARCH_SECONDS = 2.5
ANSWER_GRACE = 0.5                               # After the first router answers, how long to wait for others.
REQUEST_SECONDS = 3.0
BUDGET_SECONDS = 10.0
MAX_DATAGRAM = 2048
MAX_REPLIES = 16
MAX_GATEWAYS = 3
MAX_DESCRIPTION = 256 * 1024
MAX_SOAP = 32 * 1024
LEASE = 3600
DESCRIPTION_CHARS = 60
PRIVATE = tuple(ipaddress.ip_network(n) for n in ('10.0.0.0/8', '172.16.0.0/12', '192.168.0.0/16'))
CGNAT = ipaddress.ip_network('100.64.0.0/10')
ROUTES = Path('/proc/net/route')
OURS = 'RATW '                                   # (Every description this makes starts so.)


class UpnpError(Exception):
    """Why the attempt (or a step of it) failed, as a plain clause; `code` is the router's UPnP error code, if any."""

    def __init__(self, reason: str, code: int | None = None):
        super().__init__(reason)
        self.reason, self.code = reason, code


@dataclass
class Gateway:
    address: str                                 # The router's LAN address, the one the search reply came from.
    control: str                                 # Its service's control URL (http://address:port/path).
    service: str                                 # The service type (WANIPConnection:1 and the like).
    location: str


def clean(text, most: int) -> str:
    """Text from the network, fit for one log line: printable ASCII only (no newline, no escape), at most `most`."""
    return ''.join(c for c in str(text) if c.isascii() and c.isprintable())[:most]


def in_container() -> bool:
    return Path('/.dockerenv').exists() or Path('/run/.containerenv').exists()


def gateway_address(text: str, loopback_ok: bool = False) -> ipaddress.IPv4Address | None:
    """An IPv4 address a router may have here: private (10/8, 172.16/12, 192.168/16), or loopback in tests."""
    try:
        address = ipaddress.ip_address(text)
    except ValueError:
        return None
    if address.version != 4:
        return None
    if any(address in n for n in PRIVATE) or (loopback_ok and address.is_loopback):
        return address
    return None


def checked_url(url: str, address: str) -> urllib.parse.SplitResult | None:
    """`url` if it is http:// to `address` itself (an IPv4 literal, any port): else None."""
    try:
        parts = urllib.parse.urlsplit(url)
        port = parts.port
    except ValueError:
        return None
    if parts.scheme != 'http' or parts.hostname != address or parts.username or parts.password:
        return None
    if port is not None and not 0 < port < 65536:
        return None
    return parts


# ------------------------------------------------------------------------------------------------------ search

def search_request(target: str) -> bytes:
    return ('M-SEARCH * HTTP/1.1\r\n'
            f'HOST: {SSDP[0]}:{SSDP[1]}\r\n'
            'MAN: "ssdp:discover"\r\n'
            'MX: 2\r\n'
            f'ST: {target}\r\n'
            'USER-AGENT: RATW/1 UPnP/1.1 world/1\r\n\r\n').encode('ascii')


def parse_reply(data: bytes) -> dict | None:
    """A search reply's headers (lower-cased names), or None if it isn't one."""
    try:
        text = data.decode('latin-1')
    except UnicodeDecodeError:
        return None
    lines = text.split('\r\n') if '\r\n' in text else text.split('\n')
    if not re.match(r'^HTTP/1\.[01] 200\b', lines[0]):
        return None
    headers = {}
    for line in lines[1:]:
        name, sep, value = line.partition(':')
        if sep:
            headers[name.strip().lower()] = value.strip()
    return headers


def discover(seconds: float = SEARCH_SECONDS, target=SSDP, loopback_ok: bool = False) -> list[tuple[str, str]]:
    """The routers that answer a search: [(address, LOCATION)], each address once, at most MAX_GATEWAYS. Replies from
    outside the private ranges, larger than MAX_DATAGRAM, or whose LOCATION isn't http:// to the replying address are
    left out."""
    found: dict[str, str] = {}
    replies = 0
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP) as sock:
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
        sock.bind(('', 0))
        for t in SEARCH_TARGETS:
            try:
                sock.sendto(search_request(t), target)
            except OSError as error:
                raise UpnpError(f'the search could not be sent ({error.strerror or error})') from error
        deadline = time.monotonic() + seconds
        while replies < MAX_REPLIES and len(found) < MAX_GATEWAYS:
            left = deadline - time.monotonic()
            if left <= 0:
                break
            sock.settimeout(left)
            try:
                data, (source, _port) = sock.recvfrom(MAX_DATAGRAM + 1)
            except (socket.timeout, TimeoutError):
                break
            except OSError:
                continue                          # (An ICMP error from one target: the others may still answer.)
            replies += 1
            if len(data) > MAX_DATAGRAM or source in found or gateway_address(source, loopback_ok) is None:
                continue
            headers = parse_reply(data)
            location = headers.get('location', '') if headers else ''
            if location and len(location) <= 512 and checked_url(location, source):
                found[source] = location
                deadline = min(deadline, time.monotonic() + ANSWER_GRACE)   # (Others answer about as fast.)
    return list(found.items())


# ---------------------------------------------------------------------------------------------- HTTP and SOAP

class _DeadlineReader(io.RawIOBase):
    """A socket's reads, each given only what is left before the deadline. (A socket's own timeout is for each recv
    alone, and http.client reads the status line, up to 100 header lines of 64 KB and each chunk size with no limit
    of its own: a router sending a byte a second would never be cut off.)"""

    def __init__(self, owner: '_DeadlineSocket'):
        super().__init__()
        self.owner = owner

    def readable(self) -> bool:
        return True

    def readinto(self, buffer) -> int:
        left = self.owner.deadline - time.monotonic()
        if left <= 0:
            raise TimeoutError('the router answered too slowly')
        self.owner.sock.settimeout(left)
        return self.owner.sock.recv_into(buffer)

    def close(self) -> None:
        if not self.closed:
            self.owner.reader_closed()
        super().close()


class _DeadlineSocket:
    """The connection's socket, whose makefile (what http.client reads the reply through) keeps to the deadline. Like
    a socket's own makefile, the socket stays open until the connection and every reader made from it are closed
    (http.client closes the connection as soon as it has the reply's headers, and reads the body after)."""

    def __init__(self, sock: socket.socket, deadline: float):
        self.sock, self.deadline, self.readers, self.closing = sock, deadline, 0, False

    def makefile(self, *_args, **_kwargs):
        self.readers += 1
        return io.BufferedReader(_DeadlineReader(self))

    def reader_closed(self) -> None:
        self.readers -= 1
        if self.closing and self.readers <= 0:
            self.sock.close()

    def close(self) -> None:
        self.closing = True
        if self.readers <= 0:
            self.sock.close()

    def __getattr__(self, name):
        return getattr(self.sock, name)


class _Connection(http.client.HTTPConnection):
    def __init__(self, host: str, port: int, deadline: float):
        super().__init__(host, port, timeout=max(0.05, deadline - time.monotonic()))
        self.deadline = deadline

    def connect(self) -> None:
        super().connect()
        self.sock = _DeadlineSocket(self.sock, self.deadline)


def request(url: str, address: str, method: str = 'GET', body: bytes | None = None, headers: dict | None = None,
            cap: int = MAX_DESCRIPTION, seconds: float = REQUEST_SECONDS) -> tuple[int, bytes]:
    """One plain HTTP request to `address` (never elsewhere, never a redirect, never a proxy): (status, body),
    reading at most `cap` bytes, the whole exchange (connecting, sending, the status line, the headers, the body)
    within `seconds`."""
    parts = checked_url(url, address)
    if parts is None:
        raise UpnpError(f'the router named an address that isn\'t its own ({clean(url, 80)})')
    deadline = time.monotonic() + seconds
    path = (parts.path or '/') + (f'?{parts.query}' if parts.query else '')
    conn, reply = _Connection(address, parts.port or 80, deadline), None
    try:
        conn.request(method, path, body=body, headers={'User-Agent': 'RATW/1 UPnP/1.1', 'Connection': 'close',
                                                       **(headers or {})})
        reply = conn.getresponse()
        data = bytearray()
        while len(data) <= cap:
            if deadline - time.monotonic() <= 0:
                raise UpnpError('the router answered too slowly')
            chunk = reply.read1(min(16384, cap + 1 - len(data)))
            if not chunk:
                break
            data += chunk
        if len(data) > cap:
            raise UpnpError('the router\'s answer was too large')
        return reply.status, bytes(data)
    except (socket.timeout, TimeoutError) as error:
        raise UpnpError('the router did not answer in time') from error
    except (OSError, http.client.HTTPException) as error:
        raise UpnpError(f'the router could not be asked ({clean(getattr(error, "strerror", None) or repr(error), 120)})') \
            from error
    finally:
        if reply is not None:
            reply.close()
        conn.close()


def parse_xml(data: bytes, what: str) -> ElementTree.Element:
    """The router's XML, refused if it has a DOCTYPE or an ENTITY: by a byte search first, then by the parser itself
    (a document in UTF-16, say, gets past the bytes)."""
    refused = UpnpError(f'the router\'s {what} holds a DOCTYPE or an ENTITY (refused)')
    lowered = data.lower()
    if b'<!doctype' in lowered or b'<!entity' in lowered:
        raise refused

    def refuse(*_args):
        raise refused
    scan = expat.ParserCreate()
    scan.StartDoctypeDeclHandler = refuse
    scan.EntityDeclHandler = refuse
    try:
        scan.Parse(data, True)
    except expat.ExpatError as error:
        raise UpnpError(f'the router\'s {what} is not XML') from error
    try:
        return ElementTree.fromstring(data)
    except ElementTree.ParseError as error:
        raise UpnpError(f'the router\'s {what} is not XML') from error


def local(tag: str) -> str:
    return tag.rsplit('}', 1)[-1]


def services(location: str, address: str, seconds: float) -> list[Gateway]:
    """The router's WAN connection services, from its description at `location`, best first."""
    status, data = request(location, address, cap=MAX_DESCRIPTION, seconds=seconds)
    if status != 200:
        raise UpnpError(f'the router\'s description could not be read (HTTP {status})')
    root = parse_xml(data, 'description')
    base = location
    for element in root.iter():
        if local(element.tag) == 'URLBase' and (element.text or '').strip():
            base = element.text.strip()
    found = []
    for element in root.iter():
        if local(element.tag) != 'service':
            continue
        fields = {local(child.tag): (child.text or '').strip() for child in element}
        kind, control = fields.get('serviceType', ''), fields.get('controlURL', '')
        if kind in SERVICES and control:
            url = urllib.parse.urljoin(base, control)
            if checked_url(url, address):          # (A control URL on another host is never called.)
                found.append(Gateway(address, url, kind, location))
    found.sort(key=lambda g: SERVICES.index(g.service))
    return found


def soap(gateway: Gateway, action: str, args: dict, seconds: float = REQUEST_SECONDS) -> dict:
    """One SOAP action on the router's service: its reply's arguments. UpnpError (with the router's code) if it fails."""
    inner = ''.join(f'<{k}>{escape(str(v))}</{k}>' for k, v in args.items())
    body = ('<?xml version="1.0"?>\r\n'
            '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" '
            's:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body>'
            f'<u:{action} xmlns:u="{gateway.service}">{inner}</u:{action}>'
            '</s:Body></s:Envelope>\r\n').encode('utf-8')
    status, data = request(gateway.control, gateway.address, 'POST', body, {
        'Content-Type': 'text/xml; charset="utf-8"', 'SOAPAction': f'"{gateway.service}#{action}"'},
        cap=MAX_SOAP, seconds=seconds)
    if status == 200:
        root = parse_xml(data, f'{action} reply')
        for element in root.iter():
            if local(element.tag) == f'{action}Response':
                return {local(child.tag): (child.text or '').strip() for child in element}
        raise UpnpError(f'the router\'s {action} reply has no answer in it')
    code, description = None, ''
    if data:
        try:
            root = parse_xml(data, f'{action} fault')
            for element in root.iter():
                if local(element.tag) == 'errorCode' and (element.text or '').strip().isdigit():
                    code = int(element.text.strip())
                elif local(element.tag) == 'errorDescription':
                    description = clean((element.text or '').strip(), 80)
        except UpnpError:
            pass
    if code is None:
        raise UpnpError(f'the router\'s {action} failed (HTTP {status})')
    raise UpnpError(f'the router refused {action} (error {code}{": " + description if description else ""})', code)


# ------------------------------------------------------------------------------------------------- this computer

def local_address(gateway: str) -> str:
    """This computer's address toward the router (what the router would forward to)."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.connect((gateway, 1900))           # (A UDP connect sends nothing: it only picks the route.)
        return sock.getsockname()[0]


def on_link(gateway: str, routes: Path = ROUTES) -> bool | None:
    """Whether the router is on this computer's own network, not reached through another hop (as from inside a
    container): from Linux's route table. None where there is none to read."""
    try:
        lines = routes.read_text(encoding='ascii').splitlines()[1:]
    except (OSError, UnicodeDecodeError):
        return None
    target = int(ipaddress.IPv4Address(gateway))
    best = None
    for line in lines:
        f = line.split()
        if len(f) < 8:
            continue
        try:                                    # (Each a 32-bit word in the machine's own byte order.)
            dest, via, mask = (int.from_bytes(bytes.fromhex(x), sys.byteorder) for x in (f[1], f[2], f[7]))
            flags = int(f[3], 16)
        except ValueError:
            continue
        if not flags & 1 or target & mask != dest:           # (RTF_UP, and a route that covers the router.)
            continue
        prefix = bin(mask).count('1')
        if best is None or prefix > best[0]:
            best = (prefix, via)
    if best is None:
        return None
    return best[1] == 0


def description_for(name: str, port: int) -> str:
    name = ''.join(c for c in name if c.isprintable()).strip()
    return (OURS + (name or f'world on port {port}'))[:DESCRIPTION_CHARS]


def internet_address(text: str) -> tuple[str | None, str | None]:
    """(the router's internet address if usable, why not otherwise): why is None when nothing is known to be wrong."""
    try:
        address = ipaddress.ip_address(text)
    except ValueError:
        return None, None
    if address.version != 4 or address.is_unspecified:
        return None, None
    if address in CGNAT:
        return None, (f'the router\'s internet address, {address}, is in 100.64.0.0/10, the providers\' shared range: '
                      'this connection is behind carrier-grade NAT, so no port can be opened to it from here, and '
                      'forwarding it by hand won\'t help either: ask your provider for a public IPv4 address, or see '
                      'Hosting.md, section 9')
    if not address.is_global:
        return None, (f'the router\'s own internet address, {address}, is private: it sits behind another router or '
                      'behind carrier-grade NAT. Forward the port on the outer router too if it is yours; if it is '
                      'your provider\'s, see Hosting.md, section 9')
    return str(address), None


# ----------------------------------------------------------------------------------------------- the mapping

class Mapping:
    """A port mapping this made (or adopted): renewed with `step`, removed with `close`."""

    def __init__(self, gateway: Gateway, port: int, client: str, description: str, lease: int, log):
        self.gateway, self.port, self.client, self.description, self.lease, self.log = \
            gateway, port, client, description, lease, log
        self.next_renew = time.monotonic() + lease / 2 if lease else None
        self.failing = False
        self.closed = False

    def add(self, seconds: float = REQUEST_SECONDS) -> None:
        soap(self.gateway, 'AddPortMapping', {
            'NewRemoteHost': '', 'NewExternalPort': self.port, 'NewProtocol': 'TCP', 'NewInternalPort': self.port,
            'NewInternalClient': self.client, 'NewEnabled': 1, 'NewPortMappingDescription': self.description,
            'NewLeaseDuration': self.lease}, seconds)

    def step(self, now: float | None = None) -> None:
        """Renews the lease when half of it has gone; a failure is said once (and when it mends), tried again in five
        minutes."""
        now = time.monotonic() if now is None else now
        if self.closed or self.next_renew is None or now < self.next_renew:
            return
        try:
            self.add()
            self.next_renew = now + self.lease / 2
            if self.failing:
                self.failing = False
                self.log(f'UPnP: the router forwards port {self.port} again')
        except UpnpError as error:
            self.next_renew = now + min(300, self.lease / 2)
            if not self.failing:
                self.failing = True
                self.log(f'UPnP: could not renew the router\'s forwarding of port {self.port}: {error.reason}; '
                         'trying again in five minutes (it lapses within the hour)')

    def close(self) -> None:
        if self.closed:
            return
        self.closed = True
        try:
            soap(self.gateway, 'DeletePortMapping', {'NewRemoteHost': '', 'NewExternalPort': self.port,
                                                     'NewProtocol': 'TCP'})
            self.log(f'UPnP: the router no longer forwards port {self.port}')
        except UpnpError as error:
            if error.code == 714:              # (NoSuchEntryInArray: already gone, its lease run out.)
                self.log(f'UPnP: the router\'s forwarding of port {self.port} had already lapsed')
            else:
                self.log(f'UPnP: could not remove the router\'s forwarding of port {self.port}: {error.reason}' +
                         (' (it lapses within the hour)' if self.lease else ' (remove it on the router)'))


def by_hand(port: int) -> str:
    return f'Forward port {port} (TCP) on your router to this computer by hand (README.md).'


def no_answer(port: int, container: bool) -> str:
    if container:
        return ('UPnP: no router answered the search from inside this container: expected under Docker Desktop and '
                'rootless Docker, which keep it from reaching your router, and on Docker\'s own networks; on Linux '
                'with Docker Engine on the host\'s network (compose.yaml\'s upnp service), UPnP may be off on the '
                f'router. {by_hand(port)}')
    return f'UPnP: no router answered the search (UPnP may be off on the router). {by_hand(port)}'


def open_mapping(port: int, name: str, log=print, *, target=SSDP, loopback_ok: bool = False,
                 budget: float = BUDGET_SECONDS, lease: int = LEASE, container: bool | None = None,
                 routes: Path = ROUTES) -> Mapping | None:
    """The one attempt: forwards `port` (TCP) on the router to this computer and says so in one line, returning the
    Mapping to renew and close; or says why not in one line and returns None. Never raises (a bug included: said)."""
    container = in_container() if container is None else container
    try:
        return _open(port, name, log, target, loopback_ok, time.monotonic() + budget, lease, container, routes)
    except UpnpError as error:
        log(f'UPnP: {error.reason}. {by_hand(port)}')
    except Exception as error:                  # (Never the world's start: said, and the host forwards by hand.)
        log(f'UPnP: the attempt failed unexpectedly ({type(error).__name__}: {error}). {by_hand(port)}')
    return None


def _left(deadline: float) -> float:
    left = deadline - time.monotonic()
    if left < 0.3:
        raise UpnpError(f'the router took too long (more than {BUDGET_SECONDS:g} s in all)')
    return min(REQUEST_SECONDS, left)


def _open(port, name, log, target, loopback_ok, deadline, lease, container, routes) -> Mapping | None:
    answers = discover(min(SEARCH_SECONDS, max(0.5, deadline - time.monotonic())), target, loopback_ok)
    if not answers:
        log(no_answer(port, container))
        return None
    gateways, troubles = [], []
    for address, location in answers:
        try:
            gateways += services(location, address, _left(deadline))
        except UpnpError as error:
            troubles.append(f'{address}: {error.reason}')
    if not gateways:
        raise UpnpError('a router answered but offers no internet connection service' +
                        (f' ({"; ".join(troubles)})' if troubles else ''))
    gateway, external, why = None, None, None
    for candidate in gateways:                 # (The first whose GetExternalIPAddress answers.)
        try:
            reply = soap(candidate, 'GetExternalIPAddress', {}, _left(deadline))
        except UpnpError as error:
            troubles.append(f'{candidate.address}: {error.reason}')
            continue
        gateway = candidate
        external, why = internet_address(reply.get('NewExternalIPAddress', ''))
        break
    if gateway is None:
        raise UpnpError(f'the router would not say its internet address ({"; ".join(troubles)})')
    if why:
        log(f'UPnP: {why}.')
        return None
    client = local_address(gateway.address)
    if on_link(gateway.address, routes) is False or gateway_address(client, loopback_ok) is None:
        hint = (' (this is a container: Docker\'s own networks, Docker Desktop and rootless Docker all put one in '
                'between)' if container else '')
        raise UpnpError(f'this computer ({client}) reaches the router ({gateway.address}) through another hop'
                        f'{hint}, so the router can\'t forward to it')
    description = description_for(name, port)
    entry = _entry(gateway, port, deadline)
    replaced = None
    if entry:
        to, inner = entry.get('NewInternalClient', ''), entry.get('NewInternalPort', '')
        theirs = entry.get('NewPortMappingDescription', '')
        if to != client:
            if theirs != description or inner != str(port) or not _gone(to, port, loopback_ok, deadline):
                raise UpnpError(f'port {port} is already forwarded on the router to another computer ({clean(to, 40)})')
            # This world's own forward, to an address on this network where nothing answers on the port: this
            # computer's, before the router gave it another (a run that didn't stop cleanly). Replaced.
            try:
                soap(gateway, 'DeletePortMapping', {'NewRemoteHost': '', 'NewExternalPort': port, 'NewProtocol': 'TCP'},
                     _left(deadline))
            except UpnpError as error:
                if error.code != 714:
                    raise
            replaced = clean(to, 40)
        elif inner != str(port) or not theirs.startswith(OURS):
            log(f'UPnP: port {port} is already forwarded to this computer on the router ({gateway.address}'
                f'{", " + json.dumps(clean(theirs, 60)) if theirs else ""}'
                f'{", to port " + clean(inner, 8) if inner != str(port) else ""}): '
                'left as it is.' + ('' if inner == str(port) else f' The world listens on {port}: correct it there.'))
            return None
    mapping = Mapping(gateway, port, client, description, lease, log)
    try:
        _add(mapping, deadline)
    except UpnpError as error:
        if error.code == 725 and lease:        # (OnlyPermanentLeasesSupported.)
            mapping = Mapping(gateway, port, client, description, 0, log)
            _add(mapping, deadline)
        elif error.code == 718:                # (ConflictInMappingEntry.)
            raise UpnpError(f'port {port} is already forwarded on the router to another computer') from error
        elif error.code in (606, 401, 402):
            raise UpnpError(f'the router doesn\'t let programs open ports ({error.reason}): UPnP may be read-only '
                            'on it') from error
        else:
            raise
    held = ('for an hour at a time, renewed while the world runs' if mapping.lease else
            'until the world stops cleanly (this router only keeps permanent ones: if the world is killed or the '
            'computer loses power, it stays until the world next starts here, or you remove it on the router)')
    there = f'; its internet address is {external}' if external else '; it didn\'t say its internet address'
    was = f' (in place of this world\'s old forward to {replaced}, where nothing answers now)' if replaced else ''
    log(f'UPnP: the router ({gateway.address}) now forwards port {port} (TCP) to this computer ({client}){was}, '
        f'{held}{there}. Central\'s call-back says whether players can reach it.')
    return mapping


def _entry(gateway: Gateway, port: int, deadline: float) -> dict | None:
    """The router's forward of `port` (TCP), or None if it has none."""
    try:
        return soap(gateway, 'GetSpecificPortMappingEntry', {'NewRemoteHost': '', 'NewExternalPort': port,
                                                             'NewProtocol': 'TCP'}, _left(deadline)) or None
    except UpnpError as error:
        if error.code not in (714, 402, 501, 713):     # (None there: NoSuchEntryInArray and its common variants.)
            raise
        return None


def _gone(address: str, port: int, loopback_ok: bool, deadline: float) -> bool:
    """Whether nothing answers on `port` at `address`, an address on this network (else False: not ours to touch)."""
    if gateway_address(address, loopback_ok) is None:
        return False
    try:
        with socket.create_connection((address, port), timeout=min(1.0, _left(deadline))):
            return False
    except OSError:
        return True


def _add(mapping: Mapping, deadline: float) -> None:
    """AddPortMapping. If its reply is lost (a timeout, a cut connection: no error code from the router), the router
    is asked whether it took, so a forward it holds is never left behind unknown."""
    try:
        mapping.add(_left(deadline))
    except UpnpError as error:
        if error.code is not None:
            raise
        try:
            entry = _entry(mapping.gateway, mapping.port, deadline)
        except UpnpError:
            entry = None
        if not (entry and entry.get('NewInternalClient') == mapping.client and
                entry.get('NewPortMappingDescription') == mapping.description):
            raise


# ---------------------------------------------------------------------------------------------- the commands

def probe(target=SSDP, loopback_ok: bool = False) -> dict:
    """Discovery only (the search, the descriptions, GetExternalIPAddress): what a mapping would use. Changes nothing."""
    out: dict = {'routers': []}
    for address, location in discover(target=target, loopback_ok=loopback_ok):
        entry: dict = {'address': address, 'location': location}
        try:
            found = services(location, address, REQUEST_SECONDS)
            entry['services'] = [g.service.rsplit(':', 2)[-2] + ':' + g.service.rsplit(':', 1)[-1] for g in found]
            for g in found:
                try:
                    entry['internet_address'] = soap(g, 'GetExternalIPAddress', {}).get('NewExternalIPAddress', '')
                    usable, why = internet_address(entry['internet_address'])
                    entry['verdict'] = why or ('usable' if usable else 'unknown')
                    break
                except UpnpError as error:
                    entry['error'] = error.reason
            entry['this_computer'] = local_address(address)
            entry['on_link'] = on_link(address)
        except UpnpError as error:
            entry['error'] = error.reason
        out['routers'].append(entry)
    out['container'] = in_container()
    return out


def settings_name(path: Path) -> tuple[str, bool]:
    """world.toml's [world] name and [network] upnp (true unless it says false)."""
    raw = tomllib.loads(path.read_text(encoding='utf-8-sig'))
    world, network = raw.get('world', {}), raw.get('network', {})
    name = world.get('name', '') if isinstance(world, dict) else ''
    upnp = network.get('upnp', True) if isinstance(network, dict) else True
    if not isinstance(upnp, bool):
        raise ValueError('[network] upnp must be true or false')
    return (name if isinstance(name, str) else ''), upnp


def hold(port: int, name: str, log, stop: threading.Event, **kwargs) -> int:
    mapping = open_mapping(port, name, log, **kwargs)
    while not stop.wait(5):
        if mapping is not None:
            mapping.step()
    if mapping is not None:
        mapping.close()
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('probe', help='discovery only: the router, its services and internet address; changes nothing')
    h = sub.add_parser('hold', help='map a port, renew it, remove it on SIGTERM')
    h.add_argument('--port', type=int, required=True, help='the game port (the same on the router and here)')
    h.add_argument('--name', help='the world\'s name, for the mapping\'s description')
    h.add_argument('--settings', type=Path, help='world.toml: its [world] name, and [network] upnp')
    args = parser.parse_args(argv)
    log = lambda line: print(line, flush=True)   # noqa: E731
    if args.command == 'probe':
        print(json.dumps(probe(), indent=1))
        return 0
    if not 0 < args.port < 65536:
        parser.error('--port is 1-65535')
    stop = threading.Event()
    for sig in (signal.SIGTERM, signal.SIGINT):
        signal.signal(sig, lambda *_: stop.set())
    name, wanted = args.name or '', True
    if args.settings:
        try:
            name, wanted = settings_name(args.settings)
        except (OSError, UnicodeDecodeError, tomllib.TOMLDecodeError, ValueError) as error:
            log(f'UPnP: world.toml can\'t be read here ({error}): not tried. {by_hand(args.port)}')
            wanted = False
        else:
            if not wanted:
                log(f'UPnP: off (world.toml: [network] upnp = false). {by_hand(args.port)}')
    if not wanted:
        stop.wait()                              # (Waits to be stopped, so `restart: unless-stopped` doesn't loop.)
        return 0
    return hold(args.port, name, log, stop)


if __name__ == '__main__':
    sys.exit(main())
