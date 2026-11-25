#!/usr/bin/env python3
"""Prepare the layered wolf dolls (Data/Wolves/sources/dolls/*.zip) for the client and editor.

Each doll archive contains 48 numbered 1024x1024 RGBA layers stacked 1 (bottom) through 48 (top):
  01      eyes
  02..46  flat-coloured fur and claw layers
  47      shadow (neutral grey multiply shading)
  48      lineart (black ink plus white catchlights)

All six dolls (female/male x small/medium/large) are cropped to one shared ground-aligned frame so
every size keeps its painted relative scale and paws sit on the same ground line. For each doll,
this writes three fully opaque RGB PNGs (so canvas premultiplied-alpha never alters data channels)
into Client/src/assets/wolves/:

  <sex>-<size>-idx.png   R, G, B = top 3 contributing layer indices (1..46; 0 = empty)
  <sex>-<size>-wt.png    R = layer 1 share (0..255), G = layer 2 share (0..255), B = base alpha
                         (layer 3 share is 255 - R - G)
  <sex>-<size>-ink.png   R = shadow alpha (multiply at 131/255), G = lineart alpha, B = lineart luma

Usage:
  python3 tools/doll_prep.py                  generate all six dolls into Client/src/assets/wolves
  python3 tools/doll_prep.py --spots          also print FurSpotsAt coordinates for each doll
  python3 tools/doll_prep.py --sheet FILE     render a visual contact sheet of ready coats
"""
from __future__ import annotations

import argparse
import io
import json
import sys
import zipfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
DOLLS_DIR = ROOT / 'Data/Wolves/sources/dolls'
PALETTE = ROOT / 'Data/Wolves/palette.json'
ASSETS = ROOT / 'Client/src/assets/wolves'
MARGIN = 6
SHADOW_LUMA = 131.0 / 255.0

DOLL_ARCHIVES = (
    ('female-small', 'Female_Small.zip'),
    ('female-medium', 'Female_Average.zip'),
    ('female-large', 'Female_Tall.zip'),
    ('male-small', 'Male_Small.zip'),
    ('male-medium', 'Male_Average.zip'),
    ('male-large', 'Male_Tall.zip'),
)

# The 46 colour layers in exact stack order (1..46). Must match Data/Wolves/palette.json `layers`.
EXPECTED_SUFFIXES = (
    'EYES',
    'TAIL_TIP', 'TAIL_LWR', 'TAIL_MID', 'TAIL_UPPR', 'TAIL_ACCENT', 'TAIL_TOP',
    'BELLY', 'RIBS_LWR', 'RIBS_MID', 'RIBS_UPPR', 'BACK_ACCENT',
    'LEGS_ACCENT_REAR', 'LEGS_REAR', 'HAUNCHES_LWR', 'HAUNCHES_UPPR', 'HAUNCHES_TOP',
    'TOES_REAR', 'SOCKS_REAR', 'CLAWS_REAR',
    'LEGS_ACCENT_FRONT', 'LEGS_FRONT', 'SHOULDER_LWR', 'SHOULDER_UPPR', 'SHOULDER_TOP',
    'TOES_FRONT', 'SOCKS_FRONT', 'CLAWS_FRONT',
    'NECK_UNDER', 'NECK_1', 'NECK_2', 'NECK_3', 'TOP',
    'CHIN_JAW', 'CHEEKS_FRONT', 'CHEEKS_ACCENT', 'CHEEKS', 'UPR_HEAD', 'HEAD_ACCENTS',
    'MUZZLE', 'UPR_EYE', 'LWR_EYE', 'INNR_EAR', 'UPR_EAR', 'LWR_EAR', 'EYEBROW',
)


def _suffix(filename: str) -> str:
    stem = Path(filename).stem
    parts = stem.split('_')
    # Format: 01_AVG_FEMALE_EYES -> strip NN, SIZE, SEX
    return '_'.join(parts[3:])


def load_archive(path: Path) -> list[np.ndarray]:
    """Read all 48 RGBA layers sorted by leading number (01..48)."""
    with zipfile.ZipFile(path) as z:
        names = sorted(n for n in z.namelist() if n.lower().endswith('.png') and not Path(n).name.startswith('.'))
        if len(names) != 48:
            raise SystemExit(f'{path}: expected 48 PNG layers, found {len(names)}')
        for i, expected in enumerate(EXPECTED_SUFFIXES):
            got = _suffix(names[i])
            if got != expected:
                raise SystemExit(f'{path}: layer {i + 1:02d} expected {expected}, got {got} ({names[i]})')
        if _suffix(names[46]) != 'SHADOW' or _suffix(names[47]) != 'LINEART':
            raise SystemExit(f'{path}: layers 47/48 must be SHADOW and LINEART')
        return [np.asarray(Image.open(io.BytesIO(z.read(n))).convert('RGBA')) for n in names]


def silhouette_bounds(layers: list[np.ndarray]) -> tuple[int, int, int, int]:
    """Tight bounding box of the wolf across all 48 layers, ignoring stray low-alpha specks."""
    mask = np.zeros(layers[0].shape[:2], bool)
    for arr in layers:
        mask |= (arr[..., 3] > 12)
    ys, xs = np.nonzero(mask)
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def pack_doll(layers: list[np.ndarray], bounds: tuple[int, int, int, int], frame: tuple[int, int]) -> tuple[np.ndarray, np.ndarray, np.ndarray, tuple[int, int]]:
    """Pack a doll into (idx_rgb, wt_rgb, ink_rgb) placed inside the shared ground-aligned frame."""
    fh, fw = frame
    x0, y0, x1, y1 = bounds
    cw, ch = x1 - x0 + 1, y1 - y0 + 1
    dx = (fw - cw) // 2
    dy = (fh - MARGIN) - ch

    def place(arr: np.ndarray) -> np.ndarray:
        out = np.zeros((fh, fw) + arr.shape[2:], arr.dtype)
        out[dy:dy + ch, dx:dx + cw] = arr[y0:y1 + 1, x0:x1 + 1]
        return out

    rem = np.ones((fh, fw), np.float32)
    gt_a = np.zeros((fh, fw), np.float32)
    w0 = np.zeros((fh, fw), np.float32)
    w1 = np.zeros((fh, fw), np.float32)
    w2 = np.zeros((fh, fw), np.float32)
    i0 = np.zeros((fh, fw), np.uint8)
    i1 = np.zeros((fh, fw), np.uint8)
    i2 = np.zeros((fh, fw), np.uint8)

    # Porter-Duff source-over from top (46) down to bottom (1)
    for k in range(45, -1, -1):
        arr = place(layers[k])
        a = arr[..., 3].astype(np.float32) / 255.0
        w = a * rem
        rem *= (1.0 - a)
        gt_a += w
        b0 = w > w0
        b1 = (~b0) & (w > w1)
        b2 = (~b0) & (~b1) & (w > w2)
        nw2 = np.where(b0, w1, np.where(b1, w1, np.where(b2, w, w2)))
        ni2 = np.where(b0, i1, np.where(b1, i1, np.where(b2, k + 1, i2)))
        nw1 = np.where(b0, w0, np.where(b1, w, w1))
        ni1 = np.where(b0, i0, np.where(b1, k + 1, i1))
        nw0 = np.where(b0, w, w0)
        ni0 = np.where(b0, k + 1, i0)
        w0, w1, w2, i0, i1, i2 = nw0, nw1, nw2, ni0, ni1, ni2

    wsum = w0 + w1 + w2
    valid = wsum > 1e-6
    safe_sum = np.where(valid, wsum, 1.0)
    q0 = np.where(valid, np.round(w0 / safe_sum * 255.0), 0).clip(0, 255).astype(np.uint8)
    q1 = np.where(valid, np.round(w1 / safe_sum * 255.0), 0).clip(0, 255).astype(np.uint8)
    over = (q0.astype(np.int32) + q1.astype(np.int32)) > 255
    q1 = np.where(over, 255 - q0, q1).astype(np.uint8)
    # Zero out unused secondary/tertiary indices where their quantized weight is 0
    q2 = np.where(valid, 255 - q0.astype(np.int32) - q1.astype(np.int32), 0)
    i1 = np.where(q1 > 0, i1, 0).astype(np.uint8)
    i2 = np.where(q2 > 0, i2, 0).astype(np.uint8)
    base_a = np.round(gt_a * 255.0).clip(0, 255).astype(np.uint8)

    shadow = place(layers[46])
    line = place(layers[47])
    # Mask shadow strictly to inside the wolf silhouette
    shadow_a = np.where(base_a > 0, shadow[..., 3], 0).astype(np.uint8)
    line_a = line[..., 3].astype(np.uint8)
    line_l = np.round(line[..., 0] * 0.299 + line[..., 1] * 0.587 + line[..., 2] * 0.114).clip(0, 255).astype(np.uint8)
    line_l = np.where(line_a > 0, line_l, 0).astype(np.uint8)

    idx_rgb = np.dstack([i0, i1, i2])
    wt_rgb = np.dstack([q0, q1, base_a])
    ink_rgb = np.dstack([shadow_a, line_a, line_l])
    return idx_rgb, wt_rgb, ink_rgb, (dx - x0, dy - y0)


def compute_fur_spots(layers: list[np.ndarray], offset: tuple[int, int]) -> dict[str, list[int]]:
    """Compute jewellery fur spot coordinates in shared-frame pixels from actual layer masks."""
    ox, oy = offset

    def centroid(indices: tuple[int, ...], x_pct: float | None = None, y_pct: float | None = None) -> list[int]:
        mask = np.zeros(layers[0].shape[:2], bool)
        for idx in indices:
            mask |= (layers[idx - 1][..., 3] > 128)
        ys, xs = np.nonzero(mask)
        if not len(xs):
            return [0, 0]
        cx = float(np.percentile(xs, x_pct)) if x_pct is not None else float(np.mean(xs))
        cy = float(np.percentile(ys, y_pct)) if y_pct is not None else float(np.mean(ys))
        return [round(cx + ox), round(cy + oy)]

    # Paintings face left: left = near leg (further right in front pair? wait: let us place left/right on the two legs)
    # Inspect front legs (22_LEGS_FRONT) and rear legs (14_LEGS_REAR) x-distribution for the two legs:
    def leg_pair(idx: int, y_band: tuple[float, float]) -> tuple[list[int], list[int]]:
        mask = layers[idx - 1][..., 3] > 128
        ys, xs = np.nonzero(mask)
        y_lo, y_hi = np.percentile(ys, y_band[0]), np.percentile(ys, y_band[1])
        band = (ys >= y_lo) & (ys <= y_hi)
        bxs, bys = xs[band], ys[band]
        y_mid = float(np.mean(bys))
        # Near (left) leg vs far (right) leg: on these wolves, front near leg is rightmost of front pair; hind near leg is rightmost of hind pair
        left_x = float(np.percentile(bxs, 82))
        right_x = float(np.percentile(bxs, 18))
        return [round(left_x + ox), round(y_mid + oy)], [round(right_x + ox), round(y_mid + oy)]

    fore_left, fore_right = leg_pair(22, (45, 75))
    hind_left, hind_right = leg_pair(14, (55, 85))
    return {
        'ears': centroid((43, 44), 50, 40),
        'crown': centroid((38,), 50, 35),
        'ruff': centroid((29,), 45, 45),
        'chest': centroid((8, 9), 20, 65),
        'back': centroid((12,), 50, 50),
        'foreleg_left': fore_left,
        'foreleg_right': fore_right,
        'hindleg_left': hind_left,
        'hindleg_right': hind_right,
        'tail': centroid((4, 5), 55, 50),
    }


def render_packed(idx_rgb: np.ndarray, wt_rgb: np.ndarray, ink_rgb: np.ndarray, colours: np.ndarray) -> np.ndarray:
    """Render a packed doll given a (47, 3) float32 colour lookup table (index 0 unused)."""
    i0, i1, i2 = idx_rgb[..., 0], idx_rgb[..., 1], idx_rgb[..., 2]
    q0 = wt_rgb[..., 0].astype(np.float32) / 255.0
    q1 = wt_rgb[..., 1].astype(np.float32) / 255.0
    q2 = np.maximum(0.0, 1.0 - q0 - q1)
    base_a = wt_rgb[..., 2].astype(np.float32) / 255.0

    rgb = q0[..., None] * colours[i0] + q1[..., None] * colours[i1] + q2[..., None] * colours[i2]
    # Multiply shadow
    sa = ink_rgb[..., 0].astype(np.float32) / 255.0
    rgb = rgb * (1.0 - sa[..., None] * (1.0 - SHADOW_LUMA))
    # Composite lineart (source-over in straight alpha)
    la = ink_rgb[..., 1].astype(np.float32) / 255.0
    ll = ink_rgb[..., 2].astype(np.float32)[..., None]
    out_a = la + base_a * (1.0 - la)
    safe_a = np.where(out_a > 1e-6, out_a, 1.0)[..., None]
    out_rgb = (ll * la[..., None] + rgb * (base_a * (1.0 - la))[..., None]) / safe_a
    out_rgb = np.where(out_a[..., None] > 1e-6, out_rgb, 0.0)
    return np.dstack([out_rgb.clip(0, 255), (out_a * 255.0).clip(0, 255)]).round().astype(np.uint8)


def hex_rgb(s: str) -> np.ndarray:
    s = s.lstrip('#')
    return np.array([int(s[i:i + 2], 16) for i in (0, 2, 4)], np.float32)


def write_sheet(packed: dict[str, tuple[np.ndarray, np.ndarray, np.ndarray]], palette: dict, out_path: Path) -> None:
    """Render a visual sheet showing every ready coat on female-medium and male-large."""
    fur_hex = {c['id']: hex_rgb(c['hex']) for c in palette['fur']}
    eye_hex = {c['id']: hex_rgb(c['hex']) for c in palette['eyes']}
    layer_defs = palette['layers']
    coats = palette['coats']

    cell_w, cell_h, pad = 340, 280, 24
    cols = 2
    rows = len(coats)
    w = pad * 2 + cols * cell_w + 260
    h = pad * 2 + rows * cell_h + 40
    sheet = Image.new('RGBA', (w, h), (230, 220, 203, 255))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype(str(ROOT / 'Data/Fonts/Roboto-Regular.ttf'), 18)
    except OSError:
        font = ImageFont.load_default()

    for r, coat in enumerate(coats):
        lut = np.zeros((47, 3), np.float32)
        shades = coat['coat']
        claw_col = fur_hex[coat.get('claws', 'straw')]
        overrides = coat.get('layers', {})
        for idx, ldef in enumerate(layer_defs, start=1):
            lid = ldef['id']
            if lid == 'eyes':
                lut[idx] = eye_hex[coat['eyes']]
            elif lid in overrides:
                lut[idx] = fur_hex[overrides[lid]]
            elif ldef['shade'] == 'claws':
                lut[idx] = claw_col
            else:
                lut[idx] = fur_hex[shades[ldef['shade']]]
        y = pad + 30 + r * cell_h
        draw.text((pad, y + 20), coat['name'], fill=(40, 34, 28, 255), font=font)
        draw.text((pad, y + 48), ', '.join(shades), fill=(95, 82, 68, 255), font=font)
        draw.text((pad, y + 72), f"eyes: {coat['eyes']}  claws: {coat.get('claws', 'straw')}", fill=(95, 82, 68, 255), font=font)
        for c_idx, doll in enumerate(('female-medium', 'male-large')):
            rgba = render_packed(*packed[doll], lut)
            im = Image.fromarray(rgba)
            im.thumbnail((cell_w - 16, cell_h - 16), Image.LANCZOS)
            sheet.alpha_composite(im, (pad + 260 + c_idx * cell_w, y + (cell_h - im.height) // 2))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.convert('RGB').save(out_path)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('--sources', type=Path, default=DOLLS_DIR)
    p.add_argument('--out', type=Path, default=ASSETS)
    p.add_argument('--spots', action='store_true', help='print FurSpotsAt coordinates for each doll')
    p.add_argument('--sheet', type=Path, help='render a contact sheet of all ready coats here')
    args = p.parse_args()

    raw: dict[str, list[np.ndarray]] = {}
    bounds: dict[str, tuple[int, int, int, int]] = {}
    for key, archive in DOLL_ARCHIVES:
        path = args.sources / archive
        if not path.exists():
            raise SystemExit(f'Missing doll archive: {path}')
        raw[key] = load_archive(path)
        bounds[key] = silhouette_bounds(raw[key])

    max_w = max(b[2] - b[0] + 1 for b in bounds.values())
    max_h = max(b[3] - b[1] + 1 for b in bounds.values())
    frame = (max_h + 2 * MARGIN, max_w + 2 * MARGIN)
    print(f'Shared ground-aligned frame: {frame[1]}x{frame[0]}')

    args.out.mkdir(parents=True, exist_ok=True)
    packed: dict[str, tuple[np.ndarray, np.ndarray, np.ndarray]] = {}
    spots: dict[str, dict[str, list[int]]] = {}
    total = 0
    for key, _ in DOLL_ARCHIVES:
        idx_rgb, wt_rgb, ink_rgb, offset = pack_doll(raw[key], bounds[key], frame)
        packed[key] = (idx_rgb, wt_rgb, ink_rgb)
        spots[key] = compute_fur_spots(raw[key], offset)
        p_idx = args.out / f'{key}-idx.png'
        p_wt = args.out / f'{key}-wt.png'
        p_ink = args.out / f'{key}-ink.png'
        Image.fromarray(idx_rgb).save(p_idx, optimize=True)
        Image.fromarray(wt_rgb).save(p_wt, optimize=True)
        Image.fromarray(ink_rgb).save(p_ink, optimize=True)
        sz = p_idx.stat().st_size + p_wt.stat().st_size + p_ink.stat().st_size
        total += sz
        b = bounds[key]
        print(f'  {key:14s}: {b[2] - b[0] + 1}x{b[3] - b[1] + 1} in {frame[1]}x{frame[0]} ({sz // 1024} KB)')
    print(f'Total packed doll assets: {total // 1024} KB ({total / (1024 * 1024):.2f} MB)')

    if args.spots:
        print(json.dumps(spots, indent=2))
    if args.sheet:
        write_sheet(packed, json.loads(PALETTE.read_text(encoding='utf-8')), args.sheet)
        print(f'Sheet written to {args.sheet}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
