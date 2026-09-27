"""Placing buildings in a generated world: the block on the outdoor map, its interiors, and the links between."""
from __future__ import annotations

import re

import numpy as np

from .buildings import STYLES, Building, rotate, turn_point
from .canvas import Canvas

# Door side -> quarter turns that carry a bottom-edge door there.
TURNS = {'S': 0, 'W': 1, 'N': 2, 'E': 3}
STEP = {'S': (0, 1), 'N': (0, -1), 'E': (1, 0), 'W': (-1, 0)}
LIGHT = {
    'house': {'artificial': .7, 'daylightAccess': .4, 'tone': 'warm'},
    'shop': {'artificial': .9, 'daylightAccess': .5, 'tone': 'warm'},
    'hall': {'artificial': .9, 'daylightAccess': .6, 'tone': 'neutral'},
    'deep': {'artificial': .35, 'daylightAccess': 0, 'tone': 'cool'},
    'upper': {'artificial': .7, 'daylightAccess': .6, 'tone': 'warm'},
}


def footprint_size(building: Building, facing: str):
    fw, fh = building.footprint
    return (fw, fh) if facing in 'NS' else (fh, fw)


def door_tile(x0, y0, w, h, facing):
    """The door tile on the footprint's edge, and the street tile just outside it."""
    if facing == 'S':
        d = (x0 + w // 2, y0 + h - 1)
    elif facing == 'N':
        d = (x0 + w // 2, y0)
    elif facing == 'E':
        d = (x0 + w - 1, y0 + h // 2)
    else:
        d = (x0, y0 + h // 2)
    sx, sy = STEP[facing]
    return d, (d[0] + sx, d[1] + sy)


class Site:
    def __init__(self, canvas: Canvas, cells):
        self.canvas = canvas
        self.cells = cells                 # [(id, x, y, w, h)]
        self.rooms, self.links, self.manifest = [], [], []
        self.ids = set()

    def cell_at(self, x, y):
        for cid, cx, cy, w, h in self.cells:
            if cx <= x < cx + w and cy <= y < cy + h:
                return cid, x - cx, y - cy
        raise ValueError(f'({x}, {y}) is in no cell')

    def unique(self, name):
        base = re.sub(r'[^a-z0-9]+', '_', name.lower()).strip('_')[:40] or 'building'
        bid, n = base, 2
        while bid in self.ids:
            bid, n = f'{base}_{n}', n + 1
        self.ids.add(bid)
        return bid

    def place(self, b: Building, x0: int, y0: int, facing: str, region: str, open_door=True):
        """Stamp the building's block with its door on `facing`, and add its interiors and links."""
        c = self.canvas
        w, h = footprint_size(b, facing)
        ground = float(c.heights[y0 + h // 2, x0 + w // 2])
        c.rect(x0, y0, w, h, STYLES[b.style]['outer'], ground)
        if w > 2 and h > 2:
            c.rect(x0 + 1, y0 + 1, w - 2, h - 2, b.roof or STYLES[b.style]['roof'][0], ground)
        (dx, dy), outside = door_tile(x0, y0, w, h, facing)
        c.codes[dy, dx] = ord('+')
        bid = self.unique(b.name)
        turns = TURNS[facing]
        record = {'id': bid, 'name': b.name, 'kind': b.kind, 'district': b.district, 'style': b.style, 'trade': b.trade,
                  'door': {'x': dx, 'y': dy}, 'facing': facing, 'rooms': []}
        for i, room in enumerate(b.rooms):
            rid = bid if not room.key else f'{bid}_{room.key}'
            rows = rotate(room.rows, turns)
            rw, rh = room.width, room.height

            def turn(p):
                return turn_point(p[0], p[1], rw, rh, turns)
            light = LIGHT['deep'] if room.z < 0 else LIGHT['upper'] if room.z > 0 else \
                LIGHT['house'] if b.kind == 'house' else LIGHT['hall'] if b.style == 'concord' else LIGHT['shop']
            self.rooms.append({
                'id': rid, 'name': room.name, 'description': room.description,
                'width': len(rows[0]), 'height': len(rows), 'terrain': [''.join(r) for r in rows], 'heights': {},
                'worldX': dx + i * 2, 'worldY': dy, 'z': room.z, 'outdoors': False, 'weather': 'clear',
                'lighting': room.lighting if room.lighting != LIGHT_DEFAULT else light,
                'territory': {'region': region, 'claims': [], 'chapter': ''}})
            record['rooms'].append({'id': rid, 'name': room.name, 'z': room.z,
                                    'beds': [dict(zip('xy', turn(p))) for p in room.beds],
                                    'work': [dict(zip('xy', turn(p))) for p in room.work]})
            if i == 0:
                ix, iy = turn(room.door)
                cid, lx, ly = self.cell_at(dx, dy)
                self.links.append({'id': f'{bid}_door', 'name': f'{b.name} door', 'kind': 'door', 'open': open_door,
                                   'a': {'cell': cid, 'x': lx, 'y': ly}, 'b': {'cell': rid, 'x': ix, 'y': iy}})
        keys = {room.key: (bid if not room.key else f'{bid}_{room.key}', room) for room in b.rooms}
        for n, ((ka, xa, ya), (kb, xb, yb)) in enumerate(b.stairs):
            (ra, room_a), (rb, room_b) = keys[ka], keys[kb]
            pa = turn_point(xa, ya, room_a.width, room_a.height, turns)
            pb = turn_point(xb, yb, room_b.width, room_b.height, turns)
            self.links.append({'id': f'{bid}_stairs_{n + 1}', 'name': f'{b.name} stairs', 'kind': 'stairs',
                               'open': True, 'a': {'cell': ra, 'x': pa[0], 'y': pa[1]},
                               'b': {'cell': rb, 'x': pb[0], 'y': pb[1]}})
        self.manifest.append(record)
        return bid, outside


LIGHT_DEFAULT = {'artificial': .8, 'daylightAccess': .5, 'tone': 'warm'}


class Lots:
    """Where buildings of a given size can go: open ground, clear of other buildings, with a door onto a street."""

    def __init__(self, open_ground: np.ndarray, street: np.ndarray, gap: int = 1):
        self.open = open_ground.copy()
        self.street = street
        self.gap = gap
        self._refresh()

    def _refresh(self):
        a = self.open.astype(np.int32)
        self.sum = np.zeros((a.shape[0] + 1, a.shape[1] + 1), dtype=np.int32)
        self.sum[1:, 1:] = a.cumsum(0).cumsum(1)

    def clear(self, x0, y0, w, h):
        if x0 < 0 or y0 < 0 or y0 + h > self.open.shape[0] or x0 + w > self.open.shape[1]:
            return False
        s = self.sum
        return s[y0 + h, x0 + w] - s[y0, x0 + w] - s[y0 + h, x0] + s[y0, x0] == w * h

    def candidates(self, w_ns, h_ns, near, limit=4000):
        """(x0, y0, facing) for every clear footprint whose door opens onto a street, nearest `near` first."""
        H, W = self.open.shape
        out = []
        for facing in 'SNEW':
            w, h = (w_ns, h_ns) if facing in 'NS' else (h_ns, w_ns)
            sx, sy = STEP[facing]
            ys, xs = np.nonzero(self.street)
            # A street tile just outside a door, so the door tile is one step back from it.
            dx, dy = xs - sx, ys - sy
            if facing == 'S':
                x0, y0 = dx - w // 2, dy - (h - 1)
            elif facing == 'N':
                x0, y0 = dx - w // 2, dy
            elif facing == 'E':
                x0, y0 = dx - (w - 1), dy - h // 2
            else:
                x0, y0 = dx, dy - h // 2
            order = np.argsort(np.hypot(x0 + w / 2 - near[0], y0 + h / 2 - near[1]))[:limit]
            for i in order:
                out.append((float(np.hypot(x0[i] + w / 2 - near[0], y0[i] + h / 2 - near[1])),
                            int(x0[i]), int(y0[i]), facing, w, h))
        out.sort()
        return out

    def take(self, x0, y0, w, h):
        g = self.gap
        self.open[max(0, y0 - g):y0 + h + g, max(0, x0 - g):x0 + w + g] = False
        self._refresh()
