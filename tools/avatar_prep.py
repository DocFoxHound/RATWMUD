#!/usr/bin/env python3
"""The painted wolves' art, made ready for the client (Docs/Design/81-painted-wolves.md).

From each source painting (a wolf's profile and its face, in watercolour on paper) this writes, into the client's
assets, for the profile and for the face:

  <build>-<view>.png       the wolf cut from the paper: transparent around it, the paper's smudges gone
  <build>-<view>-map.png   where the colours go, one value per pixel of the cut-out:
                             red    two fur zones, the low four bits and the high four bits (the palette file's `zones`, in order)
                             green  how much of the second zone, 0-255 (the feather between two zones)
                             blue   0 fur; 1-127 ink (the nose, the mouth, the eyes' rims and pupils: kept as
                                    painted), by strength; 128-255 the iris (tinted with the eye colour), by strength

  python3 tools/avatar_prep.py                      the three builds from Data/Wolves/sources
  python3 tools/avatar_prep.py lean=new/lean.png    one build from another painting (the rest from the sources)
  python3 tools/avatar_prep.py --overlay DIR        also the zones laid over each wolf, to check by eye
  python3 tools/avatar_prep.py --grid DIR           also each cut-out under a grid, for placing outlines
  python3 tools/avatar_prep.py --sheet FILE         the palette sheet: every colour by name, each build in six coats

The zones follow the painted fur. The painting is cut into its tone patches (joined dark, mid and light fur); each
patch goes to the zone that the hand-placed outlines (tools/avatar_outlines.json) put most of it in; a patch lying
across two zones is cut along the outline; the edges are feathered. An outline is a polygon in the cut-out's own frame
(hundredths of its width and of its height), so the same outlines fit a larger copy of the same painting; a new painting needs
its outlines placed again (--grid, then --overlay to check).

Needs numpy and Pillow.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
SOURCES = ROOT / 'Data/Wolves/sources'
OUTLINES = ROOT / 'tools/avatar_outlines.json'
PALETTE = ROOT / 'Data/Wolves/palette.json'
ASSETS = ROOT / 'Client/src/assets/wolves'
BUILDS = ('lean', 'average', 'heavy')
VIEWS = ('profile', 'face')
# The zones, in the order the maps number them: the palette file's (the client reads the same list).
ZONES = tuple(z['id'] for z in json.loads(PALETTE.read_text(encoding='utf-8'))['zones'])
# Where the ramp's stops lie on the painting's luminance: black, the dark fur, the mid fur, the light fur, white.
STOPS = (0, 38, 118, 212, 255)
LUMA = np.array([0.299, 0.587, 0.114], np.float32)


# ---------------------------------------------------------------------------------------------- Small image tools

def blur(a: np.ndarray, radius: float) -> np.ndarray:
    """A float image in 0-1, blurred."""
    return np.asarray(Image.fromarray((a.clip(0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(radius))).astype(np.float32) / 255


def grow(mask: np.ndarray, pixels: int) -> np.ndarray:
    size = 2 * abs(pixels) + 1
    f = ImageFilter.MaxFilter(size) if pixels > 0 else ImageFilter.MinFilter(size)
    return np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).filter(f)) > 0


def label(mask: np.ndarray) -> tuple[np.ndarray, int]:
    """Joined parts of a mask (by edges, not corners), numbered from 1; 0 outside. Runs and union-find: no SciPy."""
    h, w = mask.shape
    padded = np.zeros((h, w + 2), bool)
    padded[:, 1:-1] = mask
    edges = np.diff(padded.astype(np.int8), axis=1)
    parent: list[int] = [0]

    def find(i: int) -> int:
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    runs: list[tuple[int, int, int, int]] = []           # y, x0, x1 (exclusive), id
    before: list[tuple[int, int, int]] = []
    for y in range(h):
        starts = np.flatnonzero(edges[y] == 1)
        ends = np.flatnonzero(edges[y] == -1)
        row: list[tuple[int, int, int]] = []
        j = 0
        for x0, x1 in zip(starts.tolist(), ends.tolist()):
            while j < len(before) and before[j][1] <= x0:
                j += 1
            mine = 0
            k = j
            while k < len(before) and before[k][0] < x1:
                other = find(before[k][2])
                if not mine:
                    mine = other
                elif other != mine:
                    parent[max(mine, other)] = min(mine, other)
                    mine = min(mine, other)
                k += 1
            if not mine:
                mine = len(parent)
                parent.append(mine)
            row.append((x0, x1, mine))
            runs.append((y, x0, x1, mine))
        before = row
    names: dict[int, int] = {}
    out = np.zeros((h, w), np.int32)
    for y, x0, x1, i in runs:
        r = find(i)
        out[y, x0:x1] = names.setdefault(r, len(names) + 1)
    return out, len(names)


def polygon_mask(shape: tuple[int, int], points: list[list[float]]) -> np.ndarray:
    h, w = shape
    img = Image.new('L', (w, h), 0)
    ImageDraw.Draw(img).polygon([(x * w / 100, y * h / 100) for x, y in points], fill=255)
    return np.asarray(img) > 0


def font(size: int):
    for path in (ROOT / 'Data/Fonts/Roboto-Regular.ttf', Path('/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf')):
        try:
            return ImageFont.truetype(str(path), size)
        except OSError:
            continue
    return ImageFont.load_default()


# ---------------------------------------------------------------------------------------------- Cutting the wolf out

def cut(source: Path) -> dict[str, tuple[np.ndarray, np.ndarray]]:
    """The profile and the face of a painting, each as (rgb 0-255 float, alpha 0-1), cropped to the wolf with a margin.

    The wolf is what is far from the paper's colour, in the two largest joined parts that keep clear of the picture's
    edge (the paper's smudges are smaller, fainter, or run off the edge). The left one is the profile."""
    im = np.asarray(Image.open(source).convert('RGB')).astype(np.float32)
    h, w, _ = im.shape
    c = max(8, w // 64)
    corners = np.concatenate([im[:c, :c], im[:c, -c:], im[-c:, :c], im[-c:, -c:]]).reshape(-1, 3)
    paper = np.median(corners, axis=0)
    far = np.sqrt(((im - paper) ** 2).sum(-1))
    scale = w / 1536                                        # Sizes below are for a painting 1536 wide.
    solid = np.asarray(Image.fromarray(((far > 13) * 255).astype(np.uint8)).filter(ImageFilter.MedianFilter(5))) > 0
    parts, count = label(solid)
    sizes = np.bincount(parts.ravel(), minlength=count + 1)
    edge = set(np.unique(np.concatenate([parts[0], parts[-1], parts[:, 0], parts[:, -1]])).tolist())
    # A smudge is faint all through; fur has dark in it.
    lum = im @ LUMA
    darkest = np.full(count + 1, 255.0)
    np.minimum.at(darkest, parts.ravel(), blur(lum / 255, 2 * scale).ravel() * 255)
    kept = [i for i in np.argsort(-sizes).tolist() if i and i not in edge and darkest[i] < 90 and sizes[i] > 4000 * scale * scale][:2]
    if len(kept) != 2:
        raise SystemExit(f'{source}: expected a profile and a face, found {len(kept)} wolf-like part(s).')
    out = {}
    boxes = []
    for i in kept:
        ys, xs = np.nonzero(parts == i)
        boxes.append((xs.min(), i))
    for view, (_, i) in zip(VIEWS, sorted(boxes)):
        part = parts == i
        # Fringe: the wash thins into the paper around the part. Alpha by how far from the paper, near the part only.
        near = grow(part, max(1, round(3 * scale)))
        alpha = np.clip((far - 14) / 26, 0, 1) * near
        alpha = np.maximum(alpha, grow(part, -max(1, round(2 * scale))).astype(np.float32))       # Pale fur inside stays whole.
        alpha = blur(alpha, 0.6 * scale)
        # The paper taken back out of the thin edge, so a pale rim doesn't follow the wolf onto a dark page.
        a = np.maximum(alpha, 0.35)[..., None]
        rgb = np.where(alpha[..., None] > 0.02, (im - paper * (1 - a)) / a, im).clip(0, 255)
        ys, xs = np.nonzero(alpha > 0.02)
        m = max(2, round(6 * scale))
        y0, y1, x0, x1 = max(0, ys.min() - m), min(h, ys.max() + 1 + m), max(0, xs.min() - m), min(w, xs.max() + 1 + m)
        out[view] = (rgb[y0:y1, x0:x1], alpha[y0:y1, x0:x1])
    return out


# ---------------------------------------------------------------------------------------------- Zones

def settle(drawn: np.ndarray, lum: np.ndarray, inside: np.ndarray, scale: float) -> np.ndarray:
    """The drawn zones with their boundaries moved onto the painting's own edges.

    Each zone keeps its core (what lies well inside its outline). The band between the cores is given out again from
    the cores outwards, cheapest way first, where stepping across a change of tone is dear and along a patch of one tone
    is nearly free: so two zones meet where a dark patch meets a pale one, if there is such an edge near the outline,
    and at the outline where there is none."""
    import heapq
    h, w = drawn.shape
    reach = max(2, round(22 * scale))
    soft = np.asarray(Image.fromarray(lum.clip(0, 255).astype(np.uint8)).filter(ImageFilter.MedianFilter(2 * max(1, round(scale)) + 1))).astype(np.float32)
    core = np.zeros(drawn.shape, bool)
    for z in range(len(ZONES)):
        parts, count = label(drawn == z)
        for i in range(1, count + 1):
            part = parts == i
            if not (part & inside).any():
                continue
            # A small zone (a muzzle) keeps a core too: less is taken from its edge.
            for r in (reach, round(reach * 0.6), round(reach * 0.35), 2):
                kept = grow(part, -max(1, r))
                if (kept & inside).sum() >= 0.3 * (part & inside).sum() or r == 2:
                    core |= kept
                    break
    zone = np.where(core, drawn, -1).astype(np.int32)
    zone[~inside] = -2                                      # Never entered.
    cost = np.full(drawn.shape, np.inf, np.float32)
    heap: list[tuple[float, int, int]] = []
    ys, xs = np.nonzero(core & inside)
    edge = np.zeros(drawn.shape, bool)                      # Core pixels beside the band: where the giving out starts.
    band = (zone == -1)
    edge[ys, xs] = grow(band, 1)[ys, xs]
    for y, x in zip(*np.nonzero(edge)):
        cost[y, x] = 0
        heap.append((0.0, int(y), int(x)))
    heapq.heapify(heap)
    soft_l = soft.tolist()
    zone_l = zone.tolist()
    cost_l = cost.tolist()
    band_l = band.tolist()
    while heap:
        c, y, x = heapq.heappop(heap)
        if c > cost_l[y][x]:
            continue
        z = zone_l[y][x]
        here = soft_l[y][x]
        for ny, nx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
            if 0 <= ny < h and 0 <= nx < w and band_l[ny][nx]:
                step = c + 0.12 + abs(soft_l[ny][nx] - here)
                if step < cost_l[ny][nx]:
                    cost_l[ny][nx] = step
                    zone_l[ny][nx] = z
                    heapq.heappush(heap, (step, ny, nx))
    out = np.array(zone_l, np.int32)
    out[out < 0] = drawn[out < 0]
    return out


def zone_map(rgb: np.ndarray, alpha: np.ndarray, outlines: dict, view: str, scale: float) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """(first zone, second zone, how much of the second 0-1) for every pixel: the outlines, settled on the fur."""
    shape = alpha.shape
    inside = alpha > 0.02
    base = ZONES.index('sides' if view == 'profile' else 'head')
    drawn = np.full(shape, base, np.int32)
    for entry in outlines.get('zones', []):
        drawn[polygon_mask(shape, entry['outline'])] = ZONES.index(entry['zone'])
    zone = settle(drawn, rgb @ LUMA, inside, scale)
    # Strays: a small island of one zone inside another joins what surrounds it.
    for z in range(len(ZONES)):
        parts, n = label((zone == z) & inside)
        if n < 2:
            continue
        sizes = np.bincount(parts.ravel(), minlength=n + 1)
        for i in np.flatnonzero(sizes[1:] < max(sizes[1:].max() * 0.04, 500 * scale * scale)) + 1:
            island = parts == i
            ring = grow(island, max(1, round(2 * scale))) & ~island & inside
            if ring.any():
                around = np.bincount(zone[ring], minlength=len(ZONES))
                around[z] = 0
                if around.sum():
                    zone[island] = around.argmax()
    # Feathered: each zone's share nearby; the two largest kept.
    weights = np.stack([blur((zone == z).astype(np.float32), 2.2 * scale) for z in range(len(ZONES))])
    order = np.argsort(-weights, axis=0)
    first, second = order[0], order[1]
    wa = np.take_along_axis(weights, first[None], 0)[0]
    wb = np.take_along_axis(weights, second[None], 0)[0]
    return first.astype(np.uint8), second.astype(np.uint8), wb / np.maximum(wa + wb, 1e-6)


def features(rgb: np.ndarray, alpha: np.ndarray, outlines: dict, scale: float) -> np.ndarray:
    """The map's blue: the iris (128-255) where the painting is amber, and ink (1-127) where the outlines' `ink` regions
    are dark (the nose and mouth, the eyes' rims and pupils)."""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    lum = rgb @ LUMA
    iris = blur(((r > 140) & (r - b > 85) & (g > 80) & (alpha > 0.5)).astype(np.float32), 0.8 * scale)
    region = np.zeros(alpha.shape, bool)
    for points in outlines.get('ink', []):
        region |= polygon_mask(alpha.shape, points)
    ink = np.clip((84 - lum) / 34, 0, 1) * blur(region.astype(np.float32), 1.5 * scale) * (1 - np.clip(iris * 3, 0, 1))
    out = np.zeros(alpha.shape, np.uint8)
    out[ink > 0.02] = (1 + ink[ink > 0.02] * 126).astype(np.uint8)
    out[iris > 0.02] = (128 + iris[iris > 0.02] * 127).astype(np.uint8)
    return out


def write_assets(build: str, source: Path, outlines: dict, out: Path) -> dict:
    views = cut(source)
    scale = Image.open(source).width / 1536
    made = {}
    for view, (rgb, alpha) in views.items():
        o = outlines.get(build, {}).get(view, {})
        first, second, share = zone_map(rgb, alpha, o, view, scale)
        blue = features(rgb, alpha, o, scale)
        rgba = np.dstack([rgb, alpha * 255]).round().clip(0, 255).astype(np.uint8)
        rgba[alpha <= 0.02] = 0
        Image.fromarray(rgba).save(out / f'{build}-{view}.png', optimize=True)
        packed = np.dstack([first | (second << 4), (share * 255).round().astype(np.uint8), blue])
        Image.fromarray(packed).save(out / f'{build}-{view}-map.png', optimize=True)
        made[view] = (rgb, alpha, first, second, share, blue)
    return made


# ---------------------------------------------------------------------------------------------- Recolouring (as fur.ts)

def hex_rgb(s: str) -> np.ndarray:
    s = s.lstrip('#')
    return np.array([int(s[i:i + 2], 16) for i in (0, 2, 4)], np.float32)


def ramp(lum: np.ndarray, dark: np.ndarray, mid: np.ndarray, light: np.ndarray) -> np.ndarray:
    """Luminance through the fur's ramp (fur.ts furRamp): near black, dark, mid, light, near white."""
    cols = np.stack([dark * 0.45, dark, mid, light, light + (255 - light) * 0.5])
    return np.stack([np.interp(lum, STOPS, cols[:, c]) for c in range(3)], axis=-1)


def recolour(view: tuple, coat: list[str], eyes: str, zones: dict[str, list[str]] | None = None) -> np.ndarray:
    """An RGBA picture of a cut-out in a coat (three "#rrggbb": dark, mid, light), its eyes, and zones' own coats."""
    rgb, alpha, first, second, share, blue = view
    lum = rgb @ LUMA
    ramps = [ramp(lum, *[hex_rgb(c) for c in (zones or {}).get(z, coat)]) for z in ZONES]
    pick = np.stack(ramps)
    yy, xx = np.indices(lum.shape)
    a, b = pick[first, yy, xx], pick[second, yy, xx]
    out = a + (b - a) * share[..., None]
    ink = np.where(blue < 128, blue / 127.0, 0)[..., None]
    iris = np.where(blue >= 128, (blue - 128) / 127.0, 0)[..., None]
    out = out * (1 - ink) + rgb * ink
    out = out * (1 - iris) + (hex_rgb(eyes) * (lum / 170).clip(0.5, 1.25)[..., None]).clip(0, 255) * iris
    return np.dstack([out, alpha * 255]).round().clip(0, 255).astype(np.uint8)


# ---------------------------------------------------------------------------------------------- Pictures to check by eye

OVERLAY = ('#c8433a', '#3f7fbf', '#e0c040', '#3fa060', '#9050b0', '#f08030', '#30b0b0', '#e070a0')


def overlay(view: tuple) -> Image.Image:
    """The zones laid over the wolf: each zone its own bright colour (the legend's), ink black, iris white."""
    rgb, alpha, first, second, share, blue = view
    lum = (rgb @ LUMA)[..., None] / 255
    cols = np.stack([hex_rgb(c) for c in OVERLAY])
    tint = cols[first] + (cols[second] - cols[first]) * share[..., None]
    out = tint * (0.35 + 0.65 * lum)
    out[(blue > 0) & (blue < 128)] = (0, 0, 0)
    out[blue >= 128] = (255, 255, 255)
    page = np.full(out.shape, 250.0)
    out = out * alpha[..., None] + page * (1 - alpha[..., None])
    return Image.fromarray(out.clip(0, 255).astype(np.uint8))


def grid(view: tuple, zoom: int = 2) -> Image.Image:
    """The cut-out, enlarged, under a grid in hundredths of its width and height (what an outline is written in)."""
    rgb, alpha = view[0], view[1]
    page = rgb * alpha[..., None] + 255 * (1 - alpha[..., None])
    img = Image.fromarray(page.clip(0, 255).astype(np.uint8)).resize((rgb.shape[1] * zoom, rgb.shape[0] * zoom), Image.LANCZOS)
    d = ImageDraw.Draw(img)
    f = font(11)
    for i in range(0, 101, 5):
        x, y = i / 100 * img.width, i / 100 * img.height
        d.line([(x, 0), (x, img.height)], fill=(255, 0, 0) if i % 10 == 0 else (255, 170, 170), width=1)
        d.line([(0, y), (img.width, y)], fill=(0, 0, 255) if i % 10 == 0 else (170, 170, 255), width=1)
        if i % 10 == 0:
            d.text((x + 2, 2), str(i), fill=(200, 0, 0), font=f)
            d.text((2, y + 2), str(i), fill=(0, 0, 200), font=f)
    return img


def sheet(made: dict[str, dict], palette: dict, file: Path) -> None:
    """The palette sheet: every fur and eye colour by name, then each build in six sample coats."""
    fur = {c['id']: c for c in palette['fur']}
    eye = {c['id']: c for c in palette['eyes']}
    coats = palette['coats'][:6]
    paper = (243, 237, 226)
    width, pad = 1800, 44
    cell_w, cell_h, gap = 150, 84, 10
    groups: dict[str, list[dict]] = {}
    for c in palette['fur']:
        groups.setdefault(c['group'], []).append(c)
    names = list(groups)
    group_rows = (len(names) + 1) // 2
    top = pad + 74 + (group_rows + 1) * (cell_h + 30) + 26
    wolf_w = (width - 2 * pad) // 3
    scale_of = {b: (wolf_w - 36) / (made[b]['profile'][0].shape[1] + made[b]['face'][0].shape[1] * 0.8 + 12) for b in BUILDS}
    row_h = int(max(made[b]['profile'][0].shape[0] * scale_of[b] for b in BUILDS)) + 62
    img = Image.new('RGB', (width, top + row_h * len(coats) + pad), paper)
    d = ImageDraw.Draw(img)
    big, mid, small = font(36), font(20), font(15)
    ink, faint = (52, 44, 38), (120, 104, 88)
    d.text((pad, pad - 10), 'Painted wolves: the palette', fill=ink, font=big)
    d.text((pad + 520, pad + 6), f"{len(palette['fur'])} fur colours, {len(palette['eyes'])} eye colours: earthy, natural, muted", fill=faint, font=mid)

    def swatches(title: str, colours: list[dict], x: int, y: int, w: int) -> None:
        d.text((x, y), title.upper(), fill=faint, font=small)
        for i, c in enumerate(colours):
            xx = x + i * (w + gap)
            d.rounded_rectangle([xx, y + 24, xx + w, y + 24 + 42], 6, fill=c['hex'], outline=(190, 180, 164))
            d.text((xx + 2, y + 24 + 46), c['name'], fill=ink, font=small)

    y = pad + 66
    for i, name in enumerate(names):
        swatches(name, groups[name], pad + (i % 2) * ((width - 2 * pad) // 2 + 10), y + (i // 2) * (cell_h + 30), cell_w)
    swatches('Eyes', palette['eyes'], pad, y + group_rows * (cell_h + 30), (width - 2 * pad - gap * (len(palette['eyes']) - 1)) // len(palette['eyes']))
    y = top
    for k, b in enumerate(BUILDS):
        d.text((pad + k * wolf_w, y - 4), b.upper(), fill=faint, font=small)
    y += 26
    for coat in coats:
        hexes = [fur[i]['hex'] for i in coat['coat']]
        zones = {z: [fur[i]['hex'] for i in ids] for z, ids in coat.get('zones', {}).items()}
        words = ', '.join(fur[i]['name'].lower() for i in coat['coat'])
        d.text((pad, y), f"{coat['name']}  ·  {words}; {eye[coat['eyes']]['name'].lower()} eyes" +
               (f"; with their own colours: {', '.join(z.replace('_', ' ') for z in zones)}" if zones else ''), fill=ink, font=mid)
        for k, b in enumerate(BUILDS):
            x = pad + k * wolf_w
            for view, sub in (('profile', 1.0), ('face', 0.8)):
                pic = Image.fromarray(recolour(made[b][view], hexes, eye[coat['eyes']]['hex'], zones))
                pic = pic.resize((max(1, int(pic.width * scale_of[b] * sub)), max(1, int(pic.height * scale_of[b] * sub))), Image.LANCZOS)
                img.paste(pic, (x, y + 34), pic)
                x += pic.width + 12
        y += row_h
    img.save(file)


# ---------------------------------------------------------------------------------------------- Command line

def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('sources', nargs='*', help='build=painting, e.g. lean=art/lean.png (default: Data/Wolves/sources/<build>.*)')
    p.add_argument('--outlines', type=Path, default=OUTLINES)
    p.add_argument('--out', type=Path, default=ASSETS)
    p.add_argument('--overlay', type=Path, help='a folder for the zone overlays')
    p.add_argument('--grid', type=Path, help='a folder for the cut-outs under a grid')
    p.add_argument('--sheet', type=Path, help='write the palette sheet here')
    args = p.parse_args()
    given = dict(s.split('=', 1) for s in args.sources)
    if set(given) - set(BUILDS):
        p.error(f'builds are {", ".join(BUILDS)}')
    outlines = json.loads(args.outlines.read_text(encoding='utf-8')) if args.outlines.exists() else {}
    args.out.mkdir(parents=True, exist_ok=True)
    made = {}
    for build in BUILDS:
        source = Path(given[build]) if build in given else next(iter(sorted(SOURCES.glob(f'{build}.*'))), None)
        if not source or not source.exists():
            p.error(f'no painting for the {build} build')
        made[build] = write_assets(build, source, outlines, args.out)
        for view in VIEWS:
            h, w = made[build][view][1].shape
            print(f'{build}-{view}: {w}x{h}')
            if args.overlay:
                args.overlay.mkdir(parents=True, exist_ok=True)
                overlay(made[build][view]).save(args.overlay / f'{build}-{view}-zones.png')
            if args.grid:
                args.grid.mkdir(parents=True, exist_ok=True)
                grid(made[build][view], 2 if view == 'profile' else 4).save(args.grid / f'{build}-{view}-grid.png')
    if args.sheet:
        sheet(made, json.loads(PALETTE.read_text(encoding='utf-8')), args.sheet)
        print(f'palette sheet: {args.sheet}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
