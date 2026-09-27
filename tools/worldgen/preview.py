"""Preview images of a generated region: ground colour by terrain with hillshade, cells outlined and labelled."""
from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

import terrain_catalog as catalog

_FG = np.zeros((128, 3), dtype=np.float32)
_BG = np.zeros((128, 3), dtype=np.float32)
for _t in catalog.TILES:
    _FG[ord(_t['code'])] = [int(_t['fg'][i:i + 2], 16) for i in (1, 3, 5)]
    _BG[ord(_t['code'])] = [int(_t['bg'][i:i + 2], 16) for i in (1, 3, 5)]
FONT = '/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf'


def overview(codes, heights, cells, path: Path, scale: int = 2, labels=(), relief: float = 1.0):
    """One pixel block per tile: the tile's colours mixed, lit from the north-west, with cell borders."""
    rgb = _BG[codes] * .45 + _FG[codes] * .55
    gy, gx = np.gradient(heights.astype(np.float32))
    shade = np.clip(1 + (-gx - gy) * .35 * relief, .55, 1.45)[..., None]
    tint = np.clip((heights[..., None] + 16) / 32, 0, 1)
    rgb = np.clip(rgb * shade * (1 - .25 * relief + .5 * relief * tint), 0, 255).astype(np.uint8)
    image = Image.fromarray(rgb, 'RGB').resize((codes.shape[1] * scale, codes.shape[0] * scale), Image.NEAREST)
    draw = ImageDraw.Draw(image)
    font = ImageFont.truetype(FONT, 11 * max(1, scale // 2))
    for c in cells:
        x, y, w, h = (v * scale for v in (c['x'], c['y'], c['width'], c['height']))
        draw.rectangle([x, y, x + w - 1, y + h - 1], outline=(255, 255, 255))
        draw.text((x + 4, y + 3), c['name'], fill=(255, 255, 255), font=font)
    for (lx, ly), text in labels:
        draw.text((lx * scale, ly * scale), text, fill=(255, 240, 160), font=font)
    image.save(path)


def glyphs(codes, heights, path: Path, x0, y0, w, h, px: int = 14):
    """The region drawn as the game draws it: each tile's Unicode glyph in its colour over its background."""
    image = Image.new('RGB', (w * px, h * px), (12, 16, 17))
    draw = ImageDraw.Draw(image)
    font = ImageFont.truetype(FONT, int(px * .9))
    for y in range(h):
        for x in range(w):
            ch = chr(codes[y0 + y, x0 + x])
            t = catalog.BY_CODE[ch]
            lift = float(np.clip(heights[y0 + y, x0 + x] / 32, -.5, .5))
            bg = tuple(int(np.clip(v * (1 + lift), 0, 255)) for v in _BG[ord(ch)])
            draw.rectangle([x * px, y * px, x * px + px - 1, y * px + px - 1], fill=bg)
            g = t['glyph']
            fill = tuple(int(v) for v in _FG[ord(ch)])
            if g in '█▓▒░':
                alpha = {'█': 1, '▓': .75, '▒': .5, '░': .28}[g]
                blend = tuple(int(b * (1 - alpha) + f * alpha) for b, f in zip(bg, fill))
                draw.rectangle([x * px, y * px, x * px + px - 1, y * px + px - 1], fill=blend)
            else:
                draw.text((x * px + px / 2, y * px + px / 2), g, fill=fill, font=font, anchor='mm')
    image.save(path)
