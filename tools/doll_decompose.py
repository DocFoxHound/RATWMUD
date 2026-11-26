#!/usr/bin/env python3
"""Decompose flat 6-colour + black-ink 1024x1024 wolf illustrations into exact Porter-Duff 48-layer .zip dolls.

Every output .zip conforms strictly to the 48-layer schema of Data/Wolves/sources/dolls/*.zip and passes
tools/doll_prep.py verification (01_..._EYES.png through 48_..._LINEART.png).

Key mathematical guarantee:
  Given per-pixel target weights w_k (k=1..46) summing to sil_alpha, we convert to exact Porter-Duff
  source-over alphas using the cumulative-weight recursion:
      a_k = (w_k / sum_{j=1..k} w_j)        for interior pixels (sil_alpha == 1)
  so stacking layers 01 -> 46 with standard alpha_composite produces ZERO translucent seams and bit-exact
  colour mixing in Photoshop, Krita, Pillow, and tools/doll_prep.py.

Also provides utilities for:
  - Extracting 6 independent body-region damage PNG overlays from an in-place wounded edit
  - Generating clipped-ear erase+overlay PNG pairs
  - Generating 6 natural colourable fur-marking overlays per pose
"""
from __future__ import annotations

import io
import json
import math
import zipfile
from collections import deque
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
PALETTE_PATH = ROOT / 'Data/Wolves/palette.json'
PALETTE = json.loads(PALETTE_PATH.read_text(encoding='utf-8'))
LID_TO_IDX = {l['id']: i + 1 for i, l in enumerate(PALETTE['layers'])}

SUFFIXES = (
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

ARTIST_SHADE = {
    0: np.array([48, 41, 33], np.uint8),
    1: np.array([102, 87, 71], np.uint8),
    2: np.array([135, 115, 96], np.uint8),
    3: np.array([171, 145, 120], np.uint8),
    4: np.array([242, 227, 212], np.uint8),
    'eyes': np.array([242, 217, 126], np.uint8),
    'claws': np.array([242, 217, 126], np.uint8),
}

SHADES_RGB = np.array([
    [49.0, 41.0, 32.0],
    [99.0, 85.0, 70.0],
    [129.0, 111.0, 92.0],
    [167.0, 142.0, 118.0],
    [241.0, 226.0, 211.0],
], np.float32)
SHADES_LUM = SHADES_RGB @ np.array([0.299, 0.587, 0.114], np.float32)

REGION_SHADE_MAP = {
    'body': ['back_accent', 'ribs_uppr', 'ribs_mid', 'ribs_lwr', 'belly'],
    'tail': ['tail_top', 'tail_uppr', 'tail_lwr', 'tail_mid', 'tail_tip'],
    'hind': ['haunches_top', 'haunches_uppr', 'haunches_lwr', 'hind_legs', 'hind_accent'],
    'front': ['shoulder_top', 'shoulder_uppr', 'shoulder_lwr', 'fore_legs', 'fore_accent'],
    'neck': ['neck_top', 'neck_3', 'neck_2', 'neck_1', 'neck_under'],
    'head': ['head_accents', 'upr_head', 'cheeks_accent', 'cheeks', 'cheeks_front'],
    'ears': ['upr_ear', 'lwr_ear', 'neck_2', 'neck_1', 'inner_ear'],
    'eyebrow': ['head_accents', 'upr_head', 'upr_eye', 'upr_eye', 'eyebrow'],
    'muzzle': ['head_accents', 'upr_head', 'muzzle', 'muzzle', 'chin_jaw'],
}


def poly_mask(pts: list[tuple[int, int]], shape: tuple[int, int] = (1024, 1024)) -> np.ndarray:
    h, w = shape
    im = Image.new('L', (w, h), 0)
    ImageDraw.Draw(im).polygon(pts, fill=255)
    return np.asarray(im) > 0


def feather(mask: np.ndarray, radius: float = 4.0) -> np.ndarray:
    return np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(radius)), np.float32) / 255.0


def dilate(mask: np.ndarray, r: int = 1) -> np.ndarray:
    return np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(2 * r + 1))) > 0


def erode(mask: np.ndarray, r: int = 1) -> np.ndarray:
    return np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(2 * r + 1))) > 0


def largest_component(mask: np.ndarray) -> np.ndarray:
    h, w = mask.shape
    vis = np.zeros((h, w), bool)
    best: list[tuple[int, int]] = []
    ys, xs = np.nonzero(mask)
    for sy, sx in zip(ys.tolist(), xs.tolist()):
        if vis[sy, sx]:
            continue
        q = deque([(sy, sx)])
        vis[sy, sx] = True
        comp = [(sy, sx)]
        while q:
            y, x = q.popleft()
            for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not vis[ny, nx]:
                    vis[ny, nx] = True
                    q.append((ny, nx))
                    comp.append((ny, nx))
        if len(comp) > len(best):
            best = comp
    out = np.zeros((h, w), bool)
    for y, x in best:
        out[y, x] = True
    return out


def extract_silhouette(arr: np.ndarray, white_thresh: float = 242.0) -> tuple[np.ndarray, np.ndarray]:
    """Flood-fill exterior white background from corners; return (inside_bool, smooth_alpha_01)."""
    h, w = arr.shape[:2]
    near_white = (arr[..., 0] > white_thresh) & (arr[..., 1] > white_thresh) & (arr[..., 2] > white_thresh)
    bg = np.zeros((h, w), bool)
    q: deque[tuple[int, int]] = deque()
    for y in (0, h - 1):
        for x in range(w):
            if near_white[y, x] and not bg[y, x]:
                bg[y, x] = True
                q.append((y, x))
    for x in (0, w - 1):
        for y in range(h):
            if near_white[y, x] and not bg[y, x]:
                bg[y, x] = True
                q.append((y, x))
    while q:
        y, x = q.popleft()
        for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
            if 0 <= ny < h and 0 <= nx < w and near_white[ny, nx] and not bg[ny, nx]:
                bg[ny, nx] = True
                q.append((ny, nx))
    inside = largest_component(~bg)
    lum = arr[..., 0] * 0.299 + arr[..., 1] * 0.587 + arr[..., 2] * 0.114
    outer = inside & ~erode(inside, 1)
    sil_alpha = np.where(inside, 1.0, 0.0).astype(np.float32)
    sil_alpha[outer] = np.clip((252.0 - lum[outer]) / 180.0, 0.0, 1.0)
    return inside, sil_alpha


def default_pose_regions(inside: np.ndarray) -> dict:
    """Automatic bounding-box-normalized anatomical regions for any left-facing wolf pose.
    Can be overridden per pose with exact polygons via `spec`."""
    ys, xs = np.nonzero(inside)
    x0, x1 = int(xs.min()), int(xs.max())
    y0, y1 = int(ys.min()), int(ys.max())
    bw, bh = max(1, x1 - x0), max(1, y1 - y0)

    def box(fx0: float, fy0: float, fx1: float, fy1: float) -> list[tuple[int, int]]:
        ax0, ay0 = int(x0 + fx0 * bw), int(y0 + fy0 * bh)
        ax1, ay1 = int(x0 + fx1 * bw), int(y0 + fy1 * bh)
        return [(ax0, ay0), (ax1, ay0), (ax1, ay1), (ax0, ay1)]

    return {
        'head': box(0.0, 0.12, 0.24, 0.42),
        'ears': box(0.05, 0.0, 0.22, 0.20),
        'muzzle': box(0.0, 0.22, 0.13, 0.40),
        'eyebrow': box(0.07, 0.16, 0.14, 0.25),
        'neck': box(0.16, 0.08, 0.42, 0.52),
        'front': box(0.08, 0.36, 0.45, 1.0),
        'body': box(0.40, 0.26, 0.68, 0.72),
        'hind': box(0.60, 0.28, 0.92, 1.0),
        'tail': box(0.76, 0.36, 1.0, 0.92),
        'fore_toes': box(0.08, 0.86, 0.42, 1.0),
        'fore_socks': box(0.10, 0.74, 0.42, 0.88),
        'hind_toes': box(0.55, 0.86, 0.95, 1.0),
        'hind_socks': box(0.58, 0.74, 0.95, 0.88),
        'eye_box': (int(x0 + 0.06 * bw), int(y0 + 0.16 * bh), int(x0 + 0.20 * bw), int(y0 + 0.34 * bh)),
        'fore_claw_box': (int(x0), int(y0 + 0.78 * bh), int(x0 + 0.48 * bw), int(y1 + 2)),
        'rear_claw_box': (int(x0 + 0.48 * bw), int(y0 + 0.76 * bh), int(x1 + 2), int(y1 + 2)),
        'nose_box': (int(x0), int(y0 + 0.22 * bh), int(x0 + 0.10 * bw), int(y0 + 0.38 * bh)),
    }


def weights_to_porter_duff(layer_w: np.ndarray, sil_alpha: np.ndarray) -> np.ndarray:
    """Convert partition-of-unity target weights (47, H, W) to EXACT Porter-Duff source-over alphas (47, H, W).

    When composited from layer 1 up to 46 with standard source-over:
      - Every interior pixel achieves alpha = 1.0 (0 translucent seams!)
      - Final colour equals sum(w_k * C_k) / sum(w_k) identically!
    """
    _, h, w = layer_w.shape
    total_w = layer_w[1:].sum(axis=0)
    safe_tot = np.where(total_w > 1e-6, total_w, 1.0)
    norm_w = np.where(total_w[None, ...] > 1e-6, layer_w / safe_tot[None, ...], 0.0)

    pd_a = np.zeros((47, h, w), np.float32)
    cum = np.zeros((h, w), np.float32)
    for k in range(1, 47):
        wk = norm_w[k]
        if not (wk > 1e-5).any():
            continue
        prev_empty = cum < 1e-6
        cum = cum + wk
        # For the first active layer at a pixel, alpha = sil_alpha; for subsequent layers, alpha = wk / cum.
        ak = np.where(wk > 1e-5, np.where(prev_empty, sil_alpha, wk / np.maximum(cum, 1e-6)), 0.0)
        pd_a[k] = np.clip(ak, 0.0, 1.0)
    return pd_a


def build_artist_shadow(
    inside: np.ndarray,
    sil_alpha: np.ndarray,
    layer_w: np.ndarray,
    ink_a: np.ndarray,
) -> np.ndarray:
    """Synthesize crisp, contour-hugging multiply cel/dry-brush shading matching the artist's 47_SHADOW
    across all six source dolls (measured median alpha ~216/255, rgb 131,131,131).

    Follows the artist's exact shadow grammar (zero fuzzy airbrush fog!):
      1. Crisp anatomical underside cel band (14-24px) hugging lower belly, chest-under, under-jaw, and lower tail.
      2. Crisp trailing-edge contour shadow (9-15px) along rear/right edges of legs, thighs, and tail (light from upper-left).
      3. Internal 48_LINEART fur-tuft & crease shadows: crisp directional bands (6-12px down-right) tucked directly
         beneath internal shaggy ruff lines, cheek/jaw creases, shoulder folds, and limb overlaps.
      4. Sharp dry-brush cel threshold (alpha ~214/255, 0.5px anti-alias) so multiply blending stays bold and crisp.
    """
    h, w = inside.shape
    # Fine fur-tuft dry-brush edge grain (crisp high-frequency breakup, no foggy blur)
    rng = np.random.default_rng(73)
    acc = np.zeros((h, w), np.float32)
    amp, norm = 1.0, 0.0
    for sx, sy in ((64, 48), (140, 100), (260, 185)):
        g = rng.random((sy, sx), dtype=np.float32)
        up = np.asarray(Image.fromarray((g * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC), np.float32) / 255.0
        acc += amp * up
        norm += amp
        amp *= 0.55
    fur_g = acc / norm

    def local_underside(mask_bool: np.ndarray, depth_px: float) -> np.ndarray:
        below = np.zeros_like(mask_bool)
        below[:-1, :] = mask_bool[1:, :]
        bottom_edges = mask_bool & (~below)
        dist = np.full((h, w), depth_px * 2.0, np.float32)
        dist[bottom_edges] = 0.0
        for y in range(h - 2, -1, -1):
            m = mask_bool[y]
            if m.any():
                dist[y, m] = np.minimum(dist[y, m], dist[y + 1, m] + 1.0)
        return np.where(mask_bool, np.clip((depth_px - dist) / max(1.0, depth_px), 0.0, 1.0), 0.0)

    def local_rightside(mask_bool: np.ndarray, depth_px: float) -> np.ndarray:
        right = np.zeros_like(mask_bool)
        right[:, :-1] = mask_bool[:, 1:]
        right_edges = mask_bool & (~right)
        dist = np.full((h, w), depth_px * 2.0, np.float32)
        dist[right_edges] = 0.0
        for x in range(w - 2, -1, -1):
            m = mask_bool[:, x]
            if m.any():
                dist[m, x] = np.minimum(dist[m, x], dist[m, x + 1] + 1.0)
        return np.where(mask_bool, np.clip((depth_px - dist) / max(1.0, depth_px), 0.0, 1.0), 0.0)

    # Distance from top-edge (keep dorsal ridge, top of skull, ears, and upper back crisp & lit!)
    above = np.zeros_like(inside)
    above[1:, :] = inside[:-1, :]
    top_edges = inside & (~above)
    top_dist = np.full((h, w), 200.0, np.float32)
    top_dist[top_edges] = 0.0
    for y in range(1, h):
        m = inside[y]
        if m.any():
            top_dist[y, m] = np.minimum(top_dist[y, m], top_dist[y - 1, m] + 1.0)
    lit_crown = np.clip((top_dist - 22.0) / 20.0, 0.0, 1.0)

    # 1. Tight crisp underside cel band (belly, chest underside, under-jaw, lower tail — NOT 118px blurry fog!)
    under_tight = local_underside(inside, 26.0) * lit_crown
    # Deeper crisp crescent under concave belly/groin & under-neck (where layer_w has belly/neck_under)
    belly_m = np.clip(layer_w[LID_TO_IDX['belly']] + layer_w[LID_TO_IDX['neck_under']] * 0.75, 0.0, 1.0)
    under_belly = local_underside(inside, 42.0) * np.clip(belly_m * 1.5, 0.0, 1.0) * lit_crown

    # 2. Crisp trailing-edge shadow along right/rear contours of legs, haunches, and tail (light from upper-left)
    right_tight = local_rightside(inside, 18.0) * lit_crown

    # 3. Internal 48_LINEART fur-tuft & anatomical fold shadows (neck ruff rows, cheek ruff, shoulder/haunch creases!)
    # Exclude outer silhouette border (7px) so only true internal body/fur-tuft lines cast directional crease shadows.
    interior_zone = erode(inside, 7)
    internal_ink = (ink_a > 0.30) & interior_zone
    # Restrict internal fur-tuft shadows away from eyes/muzzle tip/claws
    eye_m = layer_w[LID_TO_IDX['eyes']] > 0.1
    claw_m = (layer_w[LID_TO_IDX['fore_claws']] > 0.1) | (layer_w[LID_TO_IDX['hind_claws']] > 0.1)
    paw_m = (layer_w[LID_TO_IDX['fore_toes']] > 0.25) | (layer_w[LID_TO_IDX['hind_toes']] > 0.25)
    internal_ink &= (~dilate(eye_m, 14)) & (~dilate(claw_m, 10)) & (~dilate(paw_m, 4))

    # Cast directional shadow 2..15px down-and-right (+dy, +dx) away from upper-left light!
    crease_pot = np.zeros((h, w), np.float32)
    ink_f = internal_ink.astype(np.float32)
    for step in range(2, 16):
        dy = int(round(step * 0.80))
        dx = int(round(step * 0.60))
        shifted = np.roll(np.roll(ink_f, dy, axis=0), dx, axis=1)
        w_step = 1.0 - (step - 2) / 15.0
        crease_pot = np.maximum(crease_pot, shifted * w_step)
    # Keep crease shadows inside body & below lit crown, strongest in shaggy neck/shoulder/haunch fur
    crease_pot *= inside.astype(np.float32) * lit_crown
    # Break crease bands subtly with fine dry-brush fur grain so they match the artist's jagged ruff shading
    crease_brushed = np.clip((crease_pot - 0.14 + (fur_g - 0.50) * 0.36) * 4.2, 0.0, 1.0)

    # 4. Crisp hard-edged cel combine (artist median alpha ~216/255 = 0.847 — ZERO blurry grey haze!)
    contour_pot = np.maximum.reduce([
        under_tight * 0.96,
        under_belly * 1.04,
        right_tight * 0.90,
    ])
    # Suppress heavy muddy shadow on top of feet/paws (keep only tiny bottom paw contact edge)
    contour_pot = np.where(paw_m, local_underside(inside, 9.0) * 0.82, contour_pot)

    # Crisp cel threshold with fine dry-brush tooth on upper edge
    cel_contour = np.clip((contour_pot - 0.32 + (fur_g - 0.50) * 0.30) * 5.6, 0.0, 1.0)
    raw = np.maximum(cel_contour * 0.85, crease_brushed * 0.80)

    # Keep eyes and muzzle catchlights completely clean of shadow
    raw[dilate(eye_m, 6)] = 0.0
    raw[dilate(claw_m, 2)] = 0.0

    # Minimal 0.5px anti-alias ONLY (no blurry fog!)
    crisp = np.asarray(Image.fromarray((raw * 255).clip(0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.5)), np.float32) / 255.0
    return np.clip(crisp, 0.0, 0.86) * sil_alpha


def decompose_to_zip(
    src_image: Path,
    out_zip: Path,
    prefix: str = 'AVG_FEMALE',
    spec: dict | None = None,
    ground_y: int = 944,
    keep_teeth_white: bool = True,
) -> dict:
    """Decompose a flat-coloured + black-ink 1024x1024 wolf image into a Porter-Duff 48-layer .zip archive."""
    im = Image.open(src_image).convert('RGB')
    if im.size != (1024, 1024):
        im = im.resize((1024, 1024), Image.LANCZOS)
    arr = np.asarray(im, np.float32)
    h, w = arr.shape[:2]
    inside, sil_alpha = extract_silhouette(arr)
    lum = arr[..., 0] * 0.299 + arr[..., 1] * 0.587 + arr[..., 2] * 0.114
    sat = arr.max(axis=-1) - arr.min(axis=-1)
    yy, xx = np.indices((h, w))

    # Align vertically to ground_y if requested (preserving 1024x1024 bounds)
    ys_in, xs_in = np.nonzero(inside)
    cur_bottom = int(ys_in.max())
    shift_y = 0
    if ground_y is not None:
        desired = ground_y - cur_bottom
        # Never clip top or bottom
        min_shift = -int(ys_in.min()) + 8
        max_shift = (h - 8) - cur_bottom
        shift_y = int(np.clip(desired, min_shift, max_shift))

    regions = default_pose_regions(inside)
    if spec:
        regions.update(spec)

    # 1. Lineart: pure black ink + thin neutral-grey anti-aliased interior ink strokes + eye/nose catchlights (+ optional white bared teeth!)
    ink_core = np.clip((34.0 - lum) / 18.0, 0.0, 1.0)
    ink_thin = np.where(sat < 10.0, np.clip((52.0 - lum) / 24.0, 0.0, 1.0), 0.0)
    ink_a = np.where(inside, np.maximum(ink_core, ink_thin), 0.0).astype(np.float32)
    ex0, ey0, ex1, ey1 = regions['eye_box']
    nx0, ny0, nx1, ny1 = regions['nose_box']
    in_eye_box = (xx >= ex0) & (xx <= ex1) & (yy >= ey0) & (yy <= ey1)
    in_nose_box = (xx >= nx0) & (xx <= nx1) & (yy >= ny0) & (yy <= ny1)
    catchlight = inside & (lum > 234) & (in_eye_box | in_nose_box)
    # Bared teeth / inner mouth on angry/threatening expressions: crisp near-white pixels inside muzzle surrounded by dark ink
    teeth = np.zeros((h, w), bool)
    if keep_teeth_white and 'muzzle' in regions:
        muz_m = poly_mask(regions['muzzle'], (h, w))
        teeth = inside & muz_m & (lum > 236) & (sat < 14) & dilate(ink_a > 0.5, 4)
        # Keep teeth + pink/dark gums in lineart layer so expressions with bared teeth stay crisp regardless of fur colour!
    lineart_a = np.maximum(ink_a, np.where(catchlight | teeth, 1.0, 0.0)) * sil_alpha
    lineart_rgb = np.zeros((h, w, 3), np.float32)
    lineart_rgb[catchlight | teeth] = 250.0

    # 2. Fill fur colour underneath black ink strokes
    non_ink = inside & (ink_a < 0.25) & (~catchlight) & (~teeth) & (lum < 245)
    filled = arr.copy()
    known = non_ink.copy()
    for _ in range(14):
        unknown = inside & ~known
        if not unknown.any():
            break
        acc = np.zeros((h, w, 3), np.float32)
        cnt = np.zeros((h, w), np.float32)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            k_s = np.roll(np.roll(known, dy, axis=0), dx, axis=1)
            f_s = np.roll(np.roll(filled, dy, axis=0), dx, axis=1)
            acc += np.where(k_s[..., None], f_s, 0.0)
            cnt += k_s.astype(np.float32)
        upd = unknown & (cnt > 0)
        filled[upd] = acc[upd] / cnt[upd, None]
        known |= upd

    # 3. Gold (eyes & claws) vs 5 fur shades
    # Gold (#f2d97e) has R-B ~116 and G-B ~91, whereas warm tan (#ab9178) has R-B ~51 and G-B ~25.
    warmth = filled[..., 0] - filled[..., 2]
    green_warmth = filled[..., 1] - filled[..., 2]
    f_lum = filled[..., 0] * 0.299 + filled[..., 1] * 0.587 + filled[..., 2] * 0.114
    is_gold_col = (warmth > 66.0) & (green_warmth > 40.0) & (f_lum > 118.0)
    fcx0, fcy0, fcx1, fcy1 = regions['fore_claw_box']
    rcx0, rcy0, rcx1, rcy1 = regions['rear_claw_box']
    is_eye = inside & is_gold_col & in_eye_box
    is_fore_claw = inside & is_gold_col & (xx >= fcx0) & (xx <= fcx1) & (yy >= fcy0) & (yy <= fcy1)
    is_rear_claw = inside & is_gold_col & (xx >= rcx0) & (xx <= rcx1) & (yy >= rcy0) & (yy <= rcy1) & (~is_fore_claw)
    is_gold = is_eye | is_fore_claw | is_rear_claw

    shade_w = np.zeros((5, h, w), np.float32)
    clamped_l = np.clip(f_lum, SHADES_LUM[0], SHADES_LUM[4])
    for s in range(4):
        lo, hi = SHADES_LUM[s], SHADES_LUM[s + 1]
        in_seg = (clamped_l >= lo) & (clamped_l <= hi) if s < 3 else (clamped_l >= lo)
        t = np.clip((clamped_l - lo) / (hi - lo), 0.0, 1.0)
        shade_w[s] = np.where(in_seg, 1.0 - t, shade_w[s])
        shade_w[s + 1] = np.where(in_seg, t, shade_w[s + 1])
    shade_w *= np.where(inside & ~is_gold, sil_alpha, 0.0)[None, ...]

    # 4. Anatomical region assignment
    region_order = ('body', 'hind', 'tail', 'front', 'neck', 'head', 'ears', 'muzzle', 'eyebrow')
    region_id = np.zeros((h, w), np.int32)
    for idx, rname in enumerate(region_order):
        if rname in regions and regions[rname]:
            region_id[poly_mask(regions[rname], (h, w))] = idx

    def opt_mask(key: str) -> np.ndarray:
        pts = regions.get(key)
        if not pts:
            return np.zeros((h, w), bool)
        if isinstance(pts[0][0], (list, tuple)):
            m = np.zeros((h, w), bool)
            for p in pts:
                m |= poly_mask(p, (h, w))
            return m
        return poly_mask(pts, (h, w))

    ft_w = feather(opt_mask('fore_toes'), 3.0)
    fs_w = np.clip(feather(opt_mask('fore_socks'), 4.5) * (1.0 - ft_w), 0.0, 1.0)
    ht_w = feather(opt_mask('hind_toes'), 3.0)
    hs_w = np.clip(feather(opt_mask('hind_socks'), 4.5) * (1.0 - ht_w), 0.0, 1.0)
    paw_total = np.clip(ft_w + fs_w + ht_w + hs_w, 0.0, 1.0)

    layer_w = np.zeros((47, h, w), np.float32)
    layer_w[LID_TO_IDX['eyes']] = np.where(is_eye, sil_alpha, 0.0)
    layer_w[LID_TO_IDX['fore_claws']] = np.where(is_fore_claw, sil_alpha, 0.0)
    layer_w[LID_TO_IDX['hind_claws']] = np.where(is_rear_claw, sil_alpha, 0.0)

    for idx, rname in enumerate(region_order):
        rm = (region_id == idx).astype(np.float32)
        s_layers = REGION_SHADE_MAP[rname]
        for s in range(5):
            lidx = LID_TO_IDX[s_layers[s]]
            keep_scale = (1.0 - paw_total) if s < 4 else 1.0
            layer_w[lidx] += rm * keep_scale * shade_w[s]

    fur_non_cream = shade_w[0] + shade_w[1] + shade_w[2] + shade_w[3]
    layer_w[LID_TO_IDX['fore_toes']] += ft_w * fur_non_cream
    layer_w[LID_TO_IDX['fore_socks']] += fs_w * fur_non_cream
    layer_w[LID_TO_IDX['hind_toes']] += ht_w * fur_non_cream
    layer_w[LID_TO_IDX['hind_socks']] += hs_w * fur_non_cream

    # Convert to EXACT Porter-Duff source-over alphas!
    pd_a = weights_to_porter_duff(layer_w, sil_alpha)

    # 5. Artist-authentic cel + dry-brush fur shadow (47_SHADOW, multiply rgb(131,131,131))
    # Matches Female_Average standing.zip: bold underside occlusion on belly, inner thighs, far limbs,
    # under-chin/jaw, shaggy ruff creases, and under-tail, feathered upward with directional fur strokes.
    shadow_a = build_artist_shadow(inside, sil_alpha, layer_w, ink_a)

    def apply_shift(a: np.ndarray) -> np.ndarray:
        if shift_y == 0:
            return a
        out_a = np.zeros_like(a)
        if shift_y > 0:
            out_a[shift_y:] = a[:-shift_y]
        else:
            out_a[:shift_y] = a[-shift_y:]
        return out_a

    # 6. Write 48-layer Porter-Duff .zip
    out_zip.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out_zip, 'w', zipfile.ZIP_DEFLATED) as zf:
        for i, suf in enumerate(SUFFIXES):
            a = apply_shift(pd_a[i + 1])
            rgba = np.zeros((h, w, 4), np.uint8)
            m = a > 0.004
            rgba[m, :3] = ARTIST_SHADE[PALETTE['layers'][i]['shade']]
            rgba[m, 3] = np.round(a[m] * 255).clip(0, 255).astype(np.uint8)
            buf = io.BytesIO()
            Image.fromarray(rgba).save(buf, 'PNG', optimize=True)
            zf.writestr(f'{i + 1:02d}_{prefix}_{suf}.png', buf.getvalue())
        sa = apply_shift(shadow_a)
        s_rgba = np.zeros((h, w, 4), np.uint8)
        sm = sa > 0.004
        s_rgba[sm, :3] = 131
        s_rgba[sm, 3] = np.round(sa[sm] * 255).clip(0, 255).astype(np.uint8)
        buf = io.BytesIO()
        Image.fromarray(s_rgba).save(buf, 'PNG', optimize=True)
        zf.writestr(f'47_{prefix}_SHADOW.png', buf.getvalue())

        la = apply_shift(lineart_a)
        lr = apply_shift(lineart_rgb)
        l_rgba = np.zeros((h, w, 4), np.uint8)
        lm = la > 0.004
        l_rgba[lm, :3] = lr[lm].astype(np.uint8)
        l_rgba[lm, 3] = np.round(la[lm] * 255).clip(0, 255).astype(np.uint8)
        buf = io.BytesIO()
        Image.fromarray(l_rgba).save(buf, 'PNG', optimize=True)
        zf.writestr(f'48_{prefix}_LINEART.png', buf.getvalue())

    return {'shift_y': shift_y, 'bbox': [int(xs_in.min()), int(ys_in.min() + shift_y), int(xs_in.max()), int(ys_in.max() + shift_y)]}


def render_zip_coat(zip_path: Path, coat_id: str = 'woodland') -> Image.Image:
    """Render any 48-layer Porter-Duff .zip doll in a palette coat preset (for visual QA)."""
    fur_hex = {c['id']: np.array([int(c['hex'][i:i + 2], 16) for i in (1, 3, 5)], np.float32) for c in PALETTE['fur']}
    eye_hex = {c['id']: np.array([int(c['hex'][i:i + 2], 16) for i in (1, 3, 5)], np.float32) for c in PALETTE['eyes']}
    coat = next(c for c in PALETTE['coats'] if c['id'] == coat_id)
    shades, claw_c, ov = coat['coat'], fur_hex[coat.get('claws', 'straw')], coat.get('layers', {})
    with zipfile.ZipFile(zip_path) as z:
        layers = [np.asarray(Image.open(io.BytesIO(z.read(f))).convert('RGBA')) for f in sorted(z.namelist())]
    h, w = layers[0].shape[:2]
    rem = np.ones((h, w), np.float32)
    rgb = np.zeros((h, w, 3), np.float32)
    tot_a = np.zeros((h, w), np.float32)
    for k in range(45, -1, -1):
        ldef = PALETTE['layers'][k]
        lid = ldef['id']
        col = eye_hex[coat['eyes']] if lid == 'eyes' else fur_hex[ov[lid]] if lid in ov else claw_c if ldef['shade'] == 'claws' else fur_hex[shades[ldef['shade']]]
        a = layers[k][..., 3].astype(np.float32) / 255.0
        wt = a * rem
        rem *= (1.0 - a)
        rgb += wt[..., None] * col
        tot_a += wt
    safe_a = np.where(tot_a > 1e-5, tot_a, 1.0)[..., None]
    rgb = np.where(tot_a[..., None] > 1e-5, rgb / safe_a, 0.0)
    sa = layers[46][..., 3].astype(np.float32) / 255.0
    rgb *= (1.0 - sa[..., None] * (1.0 - 131.0 / 255.0))
    base = Image.fromarray(np.dstack([rgb.clip(0, 255), (tot_a * 255).clip(0, 255)]).astype(np.uint8))
    return Image.alpha_composite(base, Image.fromarray(layers[47]))


def render_zip_flat_white(zip_path: Path) -> Image.Image:
    """Render any 48-layer .zip as unshaded flat colours + lineart on pure white (for expression/damage edits)."""
    with zipfile.ZipFile(zip_path) as z:
        names = sorted(z.namelist())
        comp = Image.new('RGBA', (1024, 1024), (255, 255, 255, 255))
        for f in names[:46]:
            comp = Image.alpha_composite(comp, Image.open(io.BytesIO(z.read(f))).convert('RGBA'))
        comp = Image.alpha_composite(comp, Image.open(io.BytesIO(z.read(names[47]))).convert('RGBA'))
    return comp.convert('RGB')


def generate_markings_for_zip(zip_path: Path, out_dir: Path, seed: int = 42) -> list[str]:
    """Generate 6 colourable hand-brushed fur-marking overlays (white + alpha, clipped strictly to fur silhouette)."""
    with zipfile.ZipFile(zip_path) as z:
        names = sorted(z.namelist())
        layers = [np.asarray(Image.open(io.BytesIO(z.read(f))).convert('RGBA')) for f in names]
    h, w = 1024, 1024
    # Fur silhouette = union of layers 02..46 minus claws (20, 28) & eyes (01)
    fur_a = np.zeros((h, w), np.float32)
    back_a = np.zeros((h, w), np.float32)
    leg_head_a = np.zeros((h, w), np.float32)
    for k in range(1, 46):
        lid = PALETTE['layers'][k]['id']
        if lid in ('fore_claws', 'hind_claws'):
            continue
        a = layers[k][..., 3].astype(np.float32) / 255.0
        fur_a = np.maximum(fur_a, a)
        if PALETTE['layers'][k]['shade'] in (0, 1):
            back_a = np.maximum(back_a, a)
        if PALETTE['layers'][k]['group'] in ('Front legs', 'Hind legs', 'Head'):
            leg_head_a = np.maximum(leg_head_a, a)

    inside = fur_a > 0.05
    ys, xs = np.nonzero(inside)
    if not len(xs):
        return []
    x0, x1 = int(xs.min()), int(xs.max())
    y0, y1 = int(ys.min()), int(ys.max())
    bw, bh = max(1, x1 - x0), max(1, y1 - y0)
    yy, xx = np.indices((h, w), dtype=np.float32)
    nx = (xx - x0) / bw
    ny = (yy - y0) / bh

    rng = np.random.default_rng(seed)

    def fbm(scale_x: float, scale_y: float, octaves: int = 3) -> np.ndarray:
        acc = np.zeros((h, w), np.float32)
        amp = 1.0
        norm = 0.0
        for o in range(octaves):
            gh = max(4, int(scale_y * (2 ** o)))
            gw = max(4, int(scale_x * (2 ** o)))
            g = rng.random((gh, gw), dtype=np.float32)
            up = np.asarray(Image.fromarray((g * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC), np.float32) / 255.0
            acc += amp * up
            norm += amp
            amp *= 0.5
        return acc / norm

    # Fine fur-tuft directional jitter so every marking edge looks jagged & brushed like wolf fur!
    fur_grain = fbm(70, 45, 2) - 0.5
    out_dir.mkdir(parents=True, exist_ok=True)
    written: list[str] = []

    def save_mask(name: str, alpha_01: np.ndarray) -> None:
        a = np.clip(alpha_01, 0.0, 1.0) * fur_a
        rgba = np.zeros((h, w, 4), np.uint8)
        m = a > 0.01
        rgba[m, :3] = 255
        rgba[m, 3] = np.round(a[m] * 255).clip(0, 255).astype(np.uint8)
        Image.fromarray(rgba).save(out_dir / f'{name}.png', optimize=True)
        written.append(name)

    # 1. Brindle: curved organic tiger stripes across neck, ribs, haunches & legs
    warp = (fbm(10, 8, 3) - 0.5) * 0.22 + fur_grain * 0.08
    stripe_wave = np.sin((nx * 15.0 + ny * 4.5 + warp * 8.0) * math.pi)
    stripe_var = fbm(8, 6, 2)
    brindle = np.clip((stripe_wave - 0.25 + (stripe_var - 0.5) * 0.6) * 2.6, 0.0, 1.0)
    # Keep muzzle tip slightly lighter
    brindle *= np.clip((nx - 0.06) * 8.0, 0.2, 1.0)
    save_mask('brindle', brindle * 0.92)

    # 2. Merle: mottled irregular dapple spots of varying sizes with ragged fur edges
    n_big = fbm(12, 10, 3)
    n_med = fbm(24, 20, 2)
    merle = np.clip(((n_big - 0.56) * 5.0 + (n_med - 0.5) * 2.2 + fur_grain * 1.4), 0.0, 1.0)
    save_mask('merle', merle * 0.95)

    # 3. Piebald: bold asymmetric irregular pinto patches across body/shoulders/flanks/head
    p_field = fbm(5, 4, 3) + fur_grain * 0.14
    piebald = np.clip((p_field - 0.52) * 9.0, 0.0, 1.0)
    save_mask('piebald', piebald * 0.98)

    # 4. Ticking: fine organic freckled speckles concentrated along legs, muzzle, chest & lower flanks
    speck = fbm(58, 48, 2)
    clump = fbm(14, 12, 2)
    ticking = np.clip((speck - 0.64 + (clump - 0.5) * 0.22) * 8.5, 0.0, 1.0)
    tick_zone = np.clip(0.35 + 0.65 * np.maximum(leg_head_a, np.clip((ny - 0.42) * 2.5, 0.0, 1.0)), 0.0, 1.0)
    save_mask('ticking', ticking * tick_zone * 0.92)

    # 5. Irish saddle / mantle: sweeping dorsal blanket + chest blaze + face mask
    dorsal = np.clip((0.56 - ny + (fbm(7, 5, 2) - 0.5) * 0.18 + fur_grain * 0.12) * 5.5, 0.0, 1.0)
    mid_span = np.clip((nx - 0.14) * 7.0, 0.0, 1.0) * np.clip((0.88 - nx) * 7.0, 0.0, 1.0)
    irish = np.maximum(dorsal * mid_span, back_a * 0.85)
    save_mask('irish_saddle', irish * 0.95)

    # 6. Frosting: soft guard-hair silvered/tipped streaks along upper brow, shoulders, spine & tail ridge
    streak = fbm(36, 14, 3)
    top_band = np.clip((0.62 - ny + fur_grain * 0.16) * 3.0, 0.0, 1.0)
    frosting = np.clip((streak - 0.48) * 3.4, 0.0, 1.0) * top_band
    save_mask('frosting', frosting * 0.88)

    return written


def generate_clipped_ears_for_zip(zip_path: Path, out_dir: Path) -> list[str]:
    """Generate clipped_ear_{left,right,both}_{erase,overlay}.png for any 48-layer pose .zip.

    For left-facing wolf poses:
      - 'right' = wolf's anatomical right ear (far ear, peeking to the left/front)
      - 'left'  = wolf's anatomical left ear (near ear, closer to viewer on the right)
      - 'both'  = union of both clipped ear tips
    Each variant writes:
      - `<name>_erase.png`:   1024x1024 RGBA white mask for destination-out erasing of the clipped ear tip
      - `<name>_overlay.png`: 1024x1024 RGBA hand-inked healed edge closing the cut cleanly on the lineart
    """
    with zipfile.ZipFile(zip_path) as z:
        names = sorted(z.namelist())
        layers = [np.asarray(Image.open(io.BytesIO(z.read(f))).convert('RGBA')) for f in names]
    h, w = 1024, 1024
    ear_fur = np.zeros((h, w), bool)
    for idx in (42, 43, 44):  # INNR_EAR, UPR_EAR, LWR_EAR
        ear_fur |= layers[idx][..., 3] > 20
    line_a = layers[47][..., 3] > 30
    sil = np.zeros((h, w), bool)
    for k in range(46):
        sil |= layers[k][..., 3] > 20
    sil |= line_a

    ys, xs = np.nonzero(ear_fur)
    if not len(xs):
        return []
    x0, x1, y0, y1 = int(xs.min()), int(xs.max()), int(ys.min()), int(ys.max())
    # Include the ear outline pixels directly adjacent to ear_fur
    ear_region = (ear_fur | (line_a & dilate(ear_fur, 4)))

    # Find top-most y of ear_region per x column to locate the two ear tips
    col_top = np.full(x1 - x0 + 1, 1024)
    for x in range(x0, x1 + 1):
        cy = np.flatnonzero(ear_region[:, x])
        if len(cy):
            col_top[x - x0] = cy.min()

    # Locate the split between far (right) ear (left side) and near (left) ear (right side).
    # Use inner ear cream (layer 42) median X to anchor the near ear, then find the valley to its left.
    iy, ix = np.nonzero(layers[42][..., 3] > 40)
    span_w = max(1, x1 - x0)
    if len(ix):
        near_cx = int(np.median(ix))
        # Ensure at least 22% of ear span sits left of split for the far ear
        raw_split = max(x0 + int(span_w * 0.24), min(x0 + int(span_w * 0.62), near_cx - 12))
    else:
        raw_split = x0 + int(span_w * 0.45)

    # Refine split_x to the local valley (maximum y) within [raw_split - 12, raw_split + 12]
    s_lo = max(int(span_w * 0.18), raw_split - 12 - x0)
    s_hi = min(int(span_w * 0.72), raw_split + 12 - x0)
    if s_hi > s_lo:
        valid_sub = np.where(col_top[s_lo:s_hi] < 1000, col_top[s_lo:s_hi], -1)
        if valid_sub.max() > 0:
            split_x = x0 + s_lo + int(np.argmax(valid_sub))
        else:
            split_x = raw_split
    else:
        split_x = raw_split

    far_cols = col_top[:max(1, split_x - x0)]
    near_cols = col_top[max(0, split_x - x0):]
    far_tip_x = x0 + int(np.argmin(far_cols))
    far_tip_y = int(far_cols.min())
    near_tip_x = split_x + int(np.argmin(near_cols))
    near_tip_y = int(near_cols.min())

    # If the far ear is heavily tucked behind the near ear (e.g. sleeping), anchor far_tip to left 28% ear edge
    if far_tip_y >= 1000:
        valid_x = np.flatnonzero(col_top < 1000)
        if len(valid_x):
            far_rel = int(valid_x[min(len(valid_x) - 1, max(0, len(valid_x) // 6))])
            far_tip_x = x0 + far_rel
            far_tip_y = int(col_top[far_rel])
            split_x = max(far_tip_x + 8, split_x)

    yy, xx = np.indices((h, w))

    def build_clip(tip_x: int, tip_y: int, is_near: bool) -> tuple[np.ndarray, np.ndarray]:
        """Build (erase_mask_bool, edge_ink_rgba) clipping the upper ~32% of one ear tip cleanly."""
        if tip_y >= 1000:
            return np.zeros((h, w), bool), np.zeros((h, w, 4), np.uint8)
        ear_span_h = max(36, min(95, y1 - tip_y))
        clip_depth = max(18, int(ear_span_h * 0.34))
        cut_y = tip_y + clip_depth
        slope = -0.18 if is_near else 0.16
        if is_near:
            side_ok = (xx >= split_x - 2) & (xx <= min(w - 1, tip_x + 52))
        else:
            side_ok = (xx <= split_x + 2) & (xx >= max(0, tip_x - 48))
        cut_line = cut_y + slope * (xx - tip_x)
        notch = 3.0 * np.exp(-((xx - tip_x) ** 2) / (2.0 * 7.0 ** 2))
        above_cut = yy < (cut_line + notch)
        ear_tip_zone = side_ok & (yy >= max(0, tip_y - 10)) & (yy <= cut_y + 22) & ear_region
        erase_raw = ear_tip_zone & above_cut
        erase_mask = dilate(erase_raw, 6) & above_cut & side_ok
        remaining_ear = ear_region & (~erase_mask)
        near_cut = (np.abs(yy - (cut_line + notch)) <= 2.2) & side_ok & dilate(remaining_ear, 3) & dilate(erase_raw, 5)
        if not near_cut.any():
            # Fallback: stroke along bottom edge of erase_raw where it touches ear_region
            near_cut = (np.abs(yy - (cut_line + notch)) <= 2.6) & side_ok & dilate(ear_region, 3) & dilate(erase_mask, 4)
        ink = np.zeros((h, w, 4), np.uint8)
        ink[near_cut, :3] = 14
        ink[near_cut, 3] = 250
        ink_im = Image.fromarray(ink).filter(ImageFilter.GaussianBlur(0.4))
        return erase_mask, np.asarray(ink_im)

    far_er, far_ink = build_clip(far_tip_x, far_tip_y, is_near=False)
    near_er, near_ink = build_clip(near_tip_x, near_tip_y, is_near=True)
    # Prevent near ear erase from ever encroaching on far ear fur, and vice versa;
    # for 'both', bridge the 2px split column between them so no 1px whisker remains.
    both_er = near_er | far_er
    both_er |= dilate(near_er, 2) & dilate(far_er, 2)
    near_er &= (xx >= split_x + 1)
    far_er &= (xx <= split_x - 1)

    out_dir.mkdir(parents=True, exist_ok=True)
    written: list[str] = []
    variants = [
        ('clipped_ear_left', near_er, near_ink),
        ('clipped_ear_right', far_er, far_ink),
        ('clipped_ear_both', both_er, np.maximum(near_ink, far_ink)),
    ]
    for name, er_m, ink_rgba in variants:
        er_rgba = np.zeros((h, w, 4), np.uint8)
        er_rgba[er_m, :3] = 255
        er_rgba[er_m, 3] = 255
        Image.fromarray(er_rgba).save(out_dir / f'{name}_erase.png', optimize=True)
        Image.fromarray(ink_rgba).save(out_dir / f'{name}_overlay.png', optimize=True)
        written.append(name)
    return written


def extract_damage_overlays(
    base_zip: Path,
    wounded_img: Path,
    out_dir: Path,
    diff_thresh: float = 24.0,
) -> dict[str, int]:
    """Diff-extract 6 independent transparent damage overlays (head, neck, forelegs, hindlegs, torso, tail)
    from a single in-place wounded edit of `render_zip_flat_white(base_zip)`.

    Returns dict mapping region_name -> non-transparent pixel count.
    """
    base_rgb = np.asarray(render_zip_flat_white(base_zip), np.float32)
    w_im = Image.open(wounded_img).convert('RGB')
    if w_im.size != (1024, 1024):
        w_im = w_im.resize((1024, 1024), Image.LANCZOS)
    w_rgb = np.asarray(w_im, np.float32)

    with zipfile.ZipFile(base_zip) as z:
        names = sorted(z.namelist())
        layers = [np.asarray(Image.open(io.BytesIO(z.read(f))).convert('RGBA')) for f in names]

    h, w = 1024, 1024
    # Build exact per-region masks directly from the pose's own 48 layers!
    group_map = {
        'Head': 'head',
        'Neck': 'neck',
        'Front legs': 'forelegs',
        'Hind legs': 'hindlegs',
        'Body': 'torso',
        'Tail': 'tail',
    }
    reg_a: dict[str, np.ndarray] = {v: np.zeros((h, w), np.float32) for v in group_map.values()}
    sil_a = np.zeros((h, w), np.float32)
    for k in range(46):
        a = layers[k][..., 3].astype(np.float32) / 255.0
        sil_a = np.maximum(sil_a, a)
        grp = PALETTE['layers'][k]['group']
        if grp in group_map:
            rkey = group_map[grp]
            reg_a[rkey] = np.maximum(reg_a[rkey], a)
    sil_a = np.maximum(sil_a, layers[47][..., 3].astype(np.float32) / 255.0)
    sil_dil = dilate(sil_a > 0.15, 6)

    # Pixel-wise colour difference + redness/dark-wound boost
    diff = np.linalg.norm(w_rgb - base_rgb, axis=-1)
    # Ignore pure white background noise
    not_bg = ~((w_rgb[..., 0] > 240) & (w_rgb[..., 1] > 240) & (w_rgb[..., 2] > 240) & (sil_a < 0.1))
    # Wound pixels: significant colour change (crimson blood, dark scabs, pale bandages/scars) inside/near wolf
    redness = w_rgb[..., 0] - np.maximum(w_rgb[..., 1], w_rgb[..., 2])
    base_red = base_rgb[..., 0] - np.maximum(base_rgb[..., 1], base_rgb[..., 2])
    red_gain = np.maximum(0.0, redness - base_red)
    score = diff + red_gain * 0.9
    raw_alpha = np.clip((score - diff_thresh) / 30.0, 0.0, 1.0)
    raw_alpha = np.where(sil_dil & not_bg, raw_alpha, 0.0).astype(np.float32)
    # Mild despeckle: remove isolated 1px JPEG compression jitter
    eroded = np.asarray(Image.fromarray((raw_alpha * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(3)), np.float32) / 255.0
    kept_core = dilate(eroded > 0.12, 3)
    clean_alpha = np.where(kept_core, raw_alpha, 0.0) * np.clip(feather(sil_a > 0.1, 2.0) * 1.4, 0.0, 1.0)

    # Assign each pixel to its dominant anatomical region (dilated slightly for lineart/edge wounds)
    reg_names = ('head', 'neck', 'forelegs', 'hindlegs', 'torso', 'tail')
    reg_stack = np.stack([feather(reg_a[r] > 0.12, 6.0) for r in reg_names], axis=0)
    reg_sum = reg_stack.sum(axis=0)
    best_idx = np.argmax(reg_stack, axis=0)

    out_dir.mkdir(parents=True, exist_ok=True)
    counts: dict[str, int] = {}
    for i, rname in enumerate(reg_names):
        rmask = (best_idx == i) & (reg_sum > 0.02)
        ra = clean_alpha * feather(rmask, 2.0)
        rgba = np.zeros((h, w, 4), np.uint8)
        m = ra > 0.03
        rgba[m, :3] = w_rgb[m].clip(0, 255).astype(np.uint8)
        rgba[m, 3] = np.round(ra[m] * 255).clip(0, 255).astype(np.uint8)
        Image.fromarray(rgba).save(out_dir / f'{rname}.png', optimize=True)
        counts[rname] = int(m.sum())
    return counts


def generate_tail_variants_for_zip(
    zip_path: Path,
    out_dir: Path,
    root_xy: tuple[float, float],
    tip_xy: tuple[float, float],
    limp_angle_deg: float = 55.0,
    dock_frac: float = 0.36,
    hinge_frac: float = 0.44,
    radial_dock_px: float | None = None,
) -> list[str]:
    """Generate standalone 48-layer `docked_tail.zip` and `half_limp_tail.zip` directly from a pose .zip.

    Operates strictly on tail layers (02..07) + tail-only pixels of 47_SHADOW and 48_LINEART so all
    other 40 body/head/leg layers remain 100% bit-identical to the base pose.
    """
    with zipfile.ZipFile(zip_path) as z:
        names = sorted(z.namelist())
        raw_layers = [np.asarray(Image.open(io.BytesIO(z.read(f))).convert('RGBA')) for f in names]

    h, w = 1024, 1024
    # Tail fur mask = union of layers 02..07 (indices 1..6)
    tail_fur = np.zeros((h, w), bool)
    for k in range(1, 7):
        tail_fur |= raw_layers[k][..., 3] > 12
    # Non-tail body/leg mask = union of layers 01 and 08..46
    body_fur = raw_layers[0][..., 3] > 12
    for k in range(7, 46):
        body_fur |= raw_layers[k][..., 3] > 12

    # Tail-exclusive pixel mask (including tail outline strokes in layer 48 adjacent to tail_fur, outside body)
    line_a = raw_layers[47][..., 3] > 10
    tail_only_zone = (tail_fur | (line_a & dilate(tail_fur, 5))) & (~dilate(body_fur, 2))

    rx, ry = float(root_xy[0]), float(root_xy[1])
    tx, ty = float(tip_xy[0]), float(tip_xy[1])
    dx, dy = tx - rx, ty - ry
    length = max(1.0, math.hypot(dx, dy))
    ux, uy = dx / length, dy / length
    vx, vy = -uy, ux  # perpendicular unit vector

    yy, xx = np.indices((h, w), dtype=np.float32)
    px = xx - rx
    py = yy - ry
    s = (px * ux + py * uy) / length  # 0 at root, ~1 at tip
    t = (px * vx + py * vy) / length  # signed perpendicular offset (normalized)

    # Compute geodesic distance strictly through tail_only_zone from root_xy so curled tails
    # (sitting/lying/sleeping) dock cleanly along their true curved arc length without short-circuiting through body!
    tail_walk = tail_only_zone.copy()
    geo = np.full((h, w), -1.0, np.float32)
    # Seed BFS from tail pixels closest to root_xy
    t_ys, t_xs = np.nonzero(tail_walk)
    if len(t_xs):
        d2 = (t_xs - rx) ** 2 + (t_ys - ry) ** 2
        seed_mask = d2 <= max(400.0, float(np.percentile(d2, 3.0)))
        q: deque[tuple[int, int]] = deque()
        for sy, sx in zip(t_ys[seed_mask].tolist(), t_xs[seed_mask].tolist()):
            geo[sy, sx] = 0.0
            q.append((sy, sx))
        while q:
            cy, cx = q.popleft()
            cd = geo[cy, cx] + 1.0
            for ny, nx in ((cy - 1, cx), (cy + 1, cx), (cy, cx - 1), (cy, cx + 1)):
                if 0 <= ny < h and 0 <= nx < w and tail_walk[ny, nx] and geo[ny, nx] < 0:
                    geo[ny, nx] = cd
                    q.append((ny, nx))
    max_geo = float(geo.max()) if (geo >= 0).any() else length
    s_geo = np.where(geo >= 0, geo / max(1.0, max_geo), np.clip(s, 0.0, 2.0))

    # Estimate tail half-width around the dock cut for natural rounded taper
    near_dock = tail_fur & (s_geo >= dock_frac - 0.12) & (s_geo <= dock_frac + 0.12)
    half_w = float(np.percentile(np.abs(t[near_dock]), 92)) if near_dock.any() else 0.18
    half_w = max(0.08, half_w)

    # ---------- 1. DOCKED TAIL ----------
    dist_r = np.hypot(xx - rx, yy - ry)
    if radial_dock_px is not None:
        # For curled/ground tails (sitting, lying, sleeping): keep ONLY the short rounded fluffy bob
        # exiting behind the rump (geodesic + radial distance from tail root) so the curled lower loop
        # disappears cleanly with ZERO diagonal wedge or leftover tip!
        ang_r = np.arctan2(yy - ry, xx - rx)
        r_limit = radial_dock_px + 4.5 * np.sin(ang_r * 9.0 + 0.5) + 2.5 * np.cos(ang_r * 17.0)
        d_eff = np.where(geo >= 0, np.maximum(geo * 0.72, dist_r), dist_r)
        keep_raw = tail_only_zone & (d_eff <= r_limit)
        # Keep only the component directly attached to the tail root
        keep_stub = largest_component(keep_raw) if keep_raw.any() else keep_raw
        remove_dock = tail_only_zone & (~keep_stub)
        keep_dock = ~remove_dock
        # Crisp black ink stroke closing the rounded stub end where it faces the removed tail
        stub_edge = (
            tail_only_zone
            & dilate(keep_stub & tail_fur, 2)
            & dilate(remove_dock, 3)
        )
    else:
        # Smooth organic rounded fur-tuft contour at s_geo ~ dock_frac (no jagged sawtooth)
        tuft_wave = 0.032 * np.sin((t / half_w) * 4.5 + 0.6) + 0.014 * np.cos((t / half_w) * 9.0)
        cut_s = dock_frac - 0.09 * np.clip((t / half_w) ** 2, 0.0, 1.5) + tuft_wave
        s_eff = np.where(geo >= 0, s_geo, dist_r / max(1.0, length))
        remove_dock = tail_only_zone & (s_eff > cut_s)
        keep_dock = ~remove_dock
        # Crisp black ink stroke along the docked stub's rounded furry end
        stub_edge = tail_only_zone & (np.abs(s_eff - cut_s) * max_geo <= 2.4) & dilate(tail_fur & keep_dock, 2)
    dock_layers = [lay.copy() for lay in raw_layers]
    for k in range(1, 7):
        dock_layers[k][remove_dock, 3] = 0
    dock_layers[46][remove_dock, 3] = 0
    dock_layers[47][remove_dock, 3] = 0
    # Paint dark ink outline closing the docked tail tip cleanly
    dock_layers[47][stub_edge, :3] = 10
    dock_layers[47][stub_edge, 3] = np.maximum(dock_layers[47][stub_edge, 3], 248)

    out_dir.mkdir(parents=True, exist_ok=True)
    dock_zip = out_dir / 'docked_tail.zip'
    with zipfile.ZipFile(dock_zip, 'w', zipfile.ZIP_DEFLATED) as zf:
        for fname, lay in zip(names, dock_layers):
            buf = io.BytesIO()
            Image.fromarray(lay).save(buf, 'PNG', optimize=True)
            zf.writestr(fname, buf.getvalue())

    # ---------- 2. HALF-LIMP TAIL ----------
    # Smoothly bend the distal half of the tail (s > hinge_frac) downward around hinge_xy!
    hx = rx + hinge_frac * dx
    hy = ry + hinge_frac * dy
    rad = math.radians(limp_angle_deg)
    # Use smooth inverse rotation centered at hinge: dst -> src
    # For pixels in the distal half, rotate by -rad around hinge to sample original tail.
    # Determine rotation sign so the tip always moves MORE DOWNWARD (+y) and stays inside canvas!
    ca, sa_pos = math.cos(rad), math.sin(rad)
    # Test forward rotation of (tip - hinge) with +rad vs -rad; pick whichever increases y more!
    tdx, tdy = tx - hx, ty - hy
    y_plus = hy + (tdx * sa_pos + tdy * ca)
    y_minus = hy + (-tdx * sa_pos + tdy * ca)
    sign = 1.0 if y_plus >= y_minus else -1.0
    ang = sign * rad

    # Forward bend map on a dense grid, inverted cleanly via smooth blending near hinge:
    # Build inverse map: for every destination pixel (xx, yy), compute angle around hinge.
    # Wait: even cleaner — compute the bent distal tail via exact inverse rotation (-ang) around (hx, hy)
    # for the distal piece (s_src > hinge_frac - 0.05), and composite it cleanly over the proximal piece!
    inv_c, inv_s = math.cos(-ang), math.sin(-ang)
    dxh = xx - hx
    dyh = yy - hy
    src_x = hx + dxh * inv_c - dyh * inv_s
    src_y = hy + dxh * inv_s + dyh * inv_c
    # Project (src_x, src_y) onto original tail axis
    src_s = ((src_x - rx) * ux + (src_y - ry) * uy) / length

    # Bilinear sampler for RGBA arrays
    def sample_rgba(img_rgba: np.ndarray, sx: np.ndarray, sy: np.ndarray) -> np.ndarray:
        x0 = np.floor(sx).astype(np.int32)
        y0 = np.floor(sy).astype(np.int32)
        fx = (sx - x0)[..., None]
        fy = (sy - y0)[..., None]
        valid = (x0 >= 0) & (x0 < w - 1) & (y0 >= 0) & (y0 < h - 1)
        xc = np.clip(x0, 0, w - 2)
        yc = np.clip(y0, 0, h - 2)
        p00 = img_rgba[yc, xc].astype(np.float32)
        p10 = img_rgba[yc, xc + 1].astype(np.float32)
        p01 = img_rgba[yc + 1, xc].astype(np.float32)
        p11 = img_rgba[yc + 1, xc + 1].astype(np.float32)
        out = (p00 * (1 - fx) * (1 - fy) + p10 * fx * (1 - fy) + p01 * (1 - fx) * fy + p11 * fx * fy)
        out[~valid] = 0.0
        return out

    # Proximal keep weight: 1 for s <= hinge_frac, smoothly fading to 0 by hinge_frac + 0.06
    prox_keep = np.clip((hinge_frac + 0.05 - s) / 0.07, 0.0, 1.0)
    # Distal sample weight: 1 for src_s >= hinge_frac, smoothly fading to 0 at hinge_frac - 0.06
    dist_keep = np.clip((src_s - (hinge_frac - 0.05)) / 0.07, 0.0, 1.0)
    # Make sure warped distal piece never overwrites the wolf's main body/hindlegs
    not_body = (~dilate(body_fur, 1)).astype(np.float32)

    limp_layers = [lay.copy() for lay in raw_layers]
    # Extract tail-only shadow and lineart before erasing distal half
    tail_shadow = np.where(tail_only_zone[..., None], raw_layers[46], 0)
    tail_line = np.where(tail_only_zone[..., None], raw_layers[47], 0)

    def blend_tail_layer(base_lay: np.ndarray, src_tail_only: np.ndarray, is_line: bool = False) -> np.ndarray:
        out = base_lay.astype(np.float32).copy()
        # Erase distal part of original tail-only region
        erase_amt = np.where(tail_only_zone, 1.0 - prox_keep, 0.0)
        out[..., 3] *= (1.0 - erase_amt)
        # Warp distal part from src_tail_only
        warped = sample_rgba(src_tail_only, src_x, src_y)
        warped[..., 3] *= dist_keep * not_body
        if is_line:
            # Keep ink crisp black on warped lineart
            w_a = np.clip(warped[..., 3] * 1.15, 0.0, 255.0)
            warped[..., 3] = w_a
            # Composite warped over out (source-over)
            sa = warped[..., 3:4] / 255.0
            da = out[..., 3:4] / 255.0
            oa = sa + da * (1.0 - sa)
            safe = np.where(oa > 1e-5, oa, 1.0)
            out_rgb = (warped[..., :3] * sa + out[..., :3] * da * (1.0 - sa)) / safe
            out = np.concatenate([out_rgb, oa * 255.0], axis=-1)
        else:
            # Keep exact flat layer RGB, combine alpha via union/source-over
            sa = warped[..., 3] / 255.0
            da = out[..., 3] / 255.0
            oa = np.clip(sa + da * (1.0 - sa), 0.0, 1.0)
            # Fill RGB where newly painted from warped
            m_new = (sa > 0.01) & (da <= 0.01)
            out[m_new, :3] = warped[m_new, :3]
            out[..., 3] = oa * 255.0
        return out.clip(0, 255).astype(np.uint8)

    for k in range(1, 7):
        limp_layers[k] = blend_tail_layer(raw_layers[k], raw_layers[k], is_line=False)
    limp_layers[46] = blend_tail_layer(raw_layers[46], tail_shadow, is_line=False)
    limp_layers[47] = blend_tail_layer(raw_layers[47], tail_line, is_line=True)

    limp_zip = out_dir / 'half_limp_tail.zip'
    with zipfile.ZipFile(limp_zip, 'w', zipfile.ZIP_DEFLATED) as zf:
        for fname, lay in zip(names, limp_layers):
            buf = io.BytesIO()
            Image.fromarray(lay).save(buf, 'PNG', optimize=True)
            zf.writestr(fname, buf.getvalue())

    return ['docked_tail', 'half_limp_tail']


def _fur_grain_field(h: int, w: int, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed)
    acc = np.zeros((h, w), np.float32)
    amp, norm = 1.0, 0.0
    for sx, sy in ((40, 30), (95, 70), (190, 140)):
        g = rng.random((sy, sx), dtype=np.float32)
        up = np.asarray(Image.fromarray((g * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC), np.float32) / 255.0
        acc += amp * up
        norm += amp
        amp *= 0.55
    return acc / norm


def _paint_ink_wound(
    canvas: Image.Image,
    pts: list[tuple[float, float]],
    width: float,
    fur_n: np.ndarray,
    rng: np.random.Generator,
    stain_len: float = 24.0,
    style: str = 'gash',
) -> Image.Image:
    """Paint one hand-inked comic-style wolf wound with ragged matted-fur crimson stain and tapered ink parting."""
    h, w = canvas.size[1], canvas.size[0]
    dense: list[tuple[float, float]] = []
    for i in range(len(pts) - 1):
        x0, y0 = pts[i]
        x1, y1 = pts[i + 1]
        steps = max(10, int(math.hypot(x1 - x0, y1 - y0) / 1.5))
        for s in range(steps):
            t = s / steps
            wob = math.sin(t * math.pi * 2.5 + float(rng.uniform(0, 6))) * 1.3
            dense.append((x0 + (x1 - x0) * t + wob, y0 + (y1 - y0) * t + float(rng.normal(0, 0.35))))
    dense.append(pts[-1])
    n = len(dense)

    stain_m = Image.new('L', (w, h), 0)
    sd = ImageDraw.Draw(stain_m)
    for i, (x, y) in enumerate(dense):
        u = i / max(1, n - 1)
        taper = (math.sin(u * math.pi) ** 0.55) if style != 'bite' else 0.9
        rad = width * (2.5 if style == 'graze' else 1.85) * taper + float(rng.uniform(0, 2.0))
        sd.ellipse([x - rad, y - rad, x + rad, y + rad], fill=185)
    num_streaks = 5 if style != 'bite' else 4
    for _ in range(num_streaks):
        idx = int(rng.integers(max(0, n // 5), max(1, 4 * n // 5)))
        bx, by = dense[idx]
        sl = float(rng.uniform(stain_len * 0.45, stain_len * 1.15))
        ang = float(rng.uniform(1.15, 1.85))
        for s in range(int(sl)):
            f = s / sl
            rx = bx + math.cos(ang) * s + float(rng.normal(0, 0.65))
            ry = by + math.sin(ang) * s
            rr = max(0.8, (1.0 - f * 0.8) * width * 0.9)
            sd.ellipse([rx - rr, ry - rr, rx + rr, ry + rr], fill=int(210 * (1.0 - f * 0.5)))
    stain_arr = np.asarray(stain_m.filter(ImageFilter.GaussianBlur(2.0)), np.float32) / 255.0
    clump = np.clip((stain_arr * 1.35 + (fur_n - 0.48) * 0.78) * 1.5, 0.0, 1.0) * (stain_arr > 0.06)
    stain_rgba = np.zeros((h, w, 4), np.uint8)
    stain_rgba[..., 0] = np.clip(76 + (1.0 - fur_n) * 34, 0, 255).astype(np.uint8)
    stain_rgba[..., 1] = np.clip(15 + (1.0 - fur_n) * 10, 0, 255).astype(np.uint8)
    stain_rgba[..., 2] = np.clip(17 + (1.0 - fur_n) * 10, 0, 255).astype(np.uint8)
    stain_rgba[..., 3] = np.round(clump * 195).clip(0, 255).astype(np.uint8)
    out = Image.alpha_composite(canvas, Image.fromarray(stain_rgba))

    ink = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    idraw = ImageDraw.Draw(ink)
    if style == 'bite':
        # Irregular teardrop jagged canine tears (not uniform circles)
        for bx, by in pts:
            ang = float(rng.uniform(1.1, 1.9))
            tear_len = width * 2.2
            for s in range(10):
                u = s / 9.0
                tx = bx + math.cos(ang) * tear_len * u
                ty = by + math.sin(ang) * tear_len * u
                r = max(0.6, width * (1.0 - u * 0.72))
                idraw.ellipse([tx - r - 1.3, ty - r - 1.3, tx + r + 1.3, ty + r + 1.3], fill=(18, 8, 8, 240))
            for s in range(10):
                u = s / 9.0
                tx = bx + math.cos(ang) * tear_len * u
                ty = by + math.sin(ang) * tear_len * u
                r = max(0.5, width * 0.75 * (1.0 - u * 0.72))
                idraw.ellipse([tx - r, ty - r, tx + r, ty + r], fill=(104, 15, 17, 248))
    else:
        for i, (x, y) in enumerate(dense):
            u = i / max(1, n - 1)
            taper = math.sin(u * math.pi) ** 0.72
            press = 0.72 + 0.45 * math.sin(u * 13.0 + 1.2)
            r = max(0.5, width * 0.56 * taper * press)
            idraw.ellipse([x - r - 1.4, y - r - 1.4, x + r + 1.4, y + r + 1.4], fill=(18, 8, 8, int(235 * min(1.0, taper * 2.2))))
        for i, (x, y) in enumerate(dense):
            u = i / max(1, n - 1)
            taper = math.sin(u * math.pi) ** 0.85
            press = 0.72 + 0.45 * math.sin(u * 13.0 + 1.2)
            r = max(0.4, width * 0.40 * taper * press)
            idraw.ellipse([x - r, y - r, x + r, y + r], fill=(108, 16, 18, int(245 * min(1.0, taper * 2.2))))
            if r > 0.9:
                idraw.ellipse([x - r * 0.45, y - r * 0.45, x + r * 0.45, y + r * 0.45], fill=(145, 26, 26, 210))
    return Image.alpha_composite(out, ink)


def generate_damage_for_zip(zip_path: Path, out_dir: Path, seed: int = 31) -> dict[str, int]:
    """Generate 6 independent, hand-inked transparent damage overlays (head, neck, forelegs, hindlegs, torso, tail)
    tracked strictly inside ink-bounded interior fur segments of each pose's 48 layers.

    Guarantees:
      - Wounds anchor on the deepest interior fur of each anatomical region (never on forepaws/toes or seams).
      - Limb/tail wounds align with the local principal axis of that ink-bounded fur segment.
      - Wounds are clipped cleanly to the active fur segment and masked beneath dark 48_LINEART contours so
        no wound ever paints over inked lines between body parts.
    """
    with zipfile.ZipFile(zip_path) as z:
        names = sorted(z.namelist())
        layers = [np.asarray(Image.open(io.BytesIO(z.read(f))).convert('RGBA')) for f in names]

    h, w = 1024, 1024
    group_map = {
        'Head': 'head',
        'Neck': 'neck',
        'Front legs': 'forelegs',
        'Hind legs': 'hindlegs',
        'Body': 'torso',
        'Tail': 'tail',
    }
    reg_a: dict[str, np.ndarray] = {v: np.zeros((h, w), np.float32) for v in group_map.values()}
    sil_a = np.zeros((h, w), np.float32)
    eye_a = layers[0][..., 3].astype(np.float32) / 255.0
    paw_a = np.zeros((h, w), np.float32)
    ear_a = np.zeros((h, w), np.float32)
    # Dark black ink contours from 48_LINEART (exclude white eye/nose/teeth catchlights)
    line_rgba = layers[47]
    line_lum = line_rgba[..., 0] * 0.299 + line_rgba[..., 1] * 0.587 + line_rgba[..., 2] * 0.114
    ink_a = np.where(line_lum < 90.0, line_rgba[..., 3].astype(np.float32) / 255.0, 0.0)

    for k in range(1, 46):
        a = layers[k][..., 3].astype(np.float32) / 255.0
        lid = PALETTE['layers'][k]['id']
        sil_a = np.maximum(sil_a, a)
        if lid in ('fore_toes', 'hind_toes', 'fore_claws', 'hind_claws'):
            paw_a = np.maximum(paw_a, a)
            continue
        if lid in ('inner_ear', 'upr_ear', 'lwr_ear'):
            ear_a = np.maximum(ear_a, a)
        grp = PALETTE['layers'][k]['group']
        if grp in group_map:
            rkey = group_map[grp]
            reg_a[rkey] = np.maximum(reg_a[rkey], a)

    # Distance from dark ink contours / silhouette boundary (pure Pillow MinFilter, fast & exact)
    ink_barrier = dilate(ink_a > 0.28, 2)
    fur_n = _fur_grain_field(h, w, seed)
    rng = np.random.default_rng(seed + 17)
    out_dir.mkdir(parents=True, exist_ok=True)
    counts: dict[str, int] = {}

    def connected_component_at(mask: np.ndarray, sy: int, sx: int) -> np.ndarray:
        out = np.zeros((h, w), bool)
        if not (0 <= sy < h and 0 <= sx < w and mask[sy, sx]):
            return out
        q = deque([(sy, sx)])
        out[sy, sx] = True
        while q:
            y, x = q.popleft()
            for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not out[ny, nx]:
                    out[ny, nx] = True
                    q.append((ny, nx))
        return out

    def find_safe_pocket(rname: str, prefer_qy: float = 0.45, prefer_qx: float = 0.50) -> tuple[float, float, float, float, float, np.ndarray]:
        """Return (cx, cy, depth_px, ux, uy, comp_mask) for the best ink-free interior fur pocket in rname."""
        raw = reg_a[rname] > 0.30
        if rname == 'head':
            raw = raw & (~dilate(eye_a > 0.1, 12)) & (~dilate(ear_a > 0.2, 4))
        elif rname in ('forelegs', 'hindlegs'):
            raw = raw & (~dilate(paw_a > 0.15, 14))
        safe = raw & (~ink_barrier)
        if safe.sum() < 40:
            # Fallback to broader region (e.g. knocked_out pose with hidden paws -> shoulder/rump area)
            safe = (reg_a[rname] > 0.20) & (~ink_barrier)
        if safe.sum() < 25:
            safe = (sil_a > 0.30) & (~ink_barrier)

        # Iterative erosion to measure distance to nearest ink line or region edge (up to 26px)
        depth = np.zeros((h, w), np.float32)
        cur = safe.copy()
        for step in range(1, 27):
            if not cur.any():
                break
            depth[cur] = float(step)
            cur = erode(cur, 1)

        ys, xs = np.nonzero(safe)
        x0, x1 = float(np.percentile(xs, 10)), float(np.percentile(xs, 90))
        y0, y1 = float(np.percentile(ys, 10)), float(np.percentile(ys, 90))
        bw, bh = max(20.0, x1 - x0), max(20.0, y1 - y0)
        tx, ty = x0 + prefer_qx * bw, y0 + prefer_qy * bh
        # Score pixels: strongly prefer deep interior (far from ink lines!) with gentle preference toward anatomical sweet-spot
        d_vals = depth[ys, xs]
        dist_pref = np.hypot((xs - tx) / bw, (ys - ty) / bh)
        score = d_vals - dist_pref * 3.5
        best = int(np.argmax(score))
        cx, cy = float(xs[best]), float(ys[best])
        max_d = float(d_vals[best])

        comp = connected_component_at(safe, int(cy), int(cx))
        # Compute local principal axis (ux, uy) of pixels in this ink-bounded pocket near (cx, cy)
        cys, cxs = np.nonzero(comp)
        near = ((cxs - cx) ** 2 + (cys - cy) ** 2) <= (68.0 ** 2)
        if near.sum() >= 15:
            pts = np.stack([cxs[near] - cx, cys[near] - cy], axis=1).astype(np.float32)
            cov = (pts.T @ pts) / len(pts)
            evals, evecs = np.linalg.eigh(cov)
            ux, uy = float(evecs[0, 1]), float(evecs[1, 1])
            # Normalize direction to point generally downward-right for natural gravity/fur flow
            if uy < 0 or (abs(uy) < 0.2 and ux < 0):
                ux, uy = -ux, -uy
        else:
            ux, uy = 0.6, 0.8
        return cx, cy, max_d, ux, uy, comp

    for rname in ('head', 'neck', 'forelegs', 'hindlegs', 'torso', 'tail'):
        ov = Image.new('RGBA', (w, h), (0, 0, 0, 0))
        if rname == 'head':
            cx, cy, dep, ux, uy, comp = find_safe_pocket('head', prefer_qy=0.62, prefer_qx=0.56)
            span = min(22.0, max(10.0, dep * 1.45))
            ov = _paint_ink_wound(
                ov,
                [(cx - ux * span, cy - uy * span), (cx, cy), (cx + ux * span * 0.9, cy + uy * span * 0.9)],
                2.4, fur_n, rng, min(14.0, dep * 0.95), 'gash',
            )
            px, py = -uy, ux
            ov = _paint_ink_wound(
                ov,
                [(cx + px * 7 - ux * span * 0.55, cy + py * 7 - uy * span * 0.55),
                 (cx + px * 7 + ux * span * 0.55, cy + py * 7 + uy * span * 0.55)],
                1.9, fur_n, rng, min(10.0, dep * 0.7), 'graze',
            )
            valid_comp = comp
        elif rname == 'neck':
            cx, cy, dep, ux, uy, comp = find_safe_pocket('neck', prefer_qy=0.48, prefer_qx=0.48)
            span = min(28.0, max(12.0, dep * 1.4))
            px, py = -uy, ux
            ov = _paint_ink_wound(
                ov,
                [(cx - ux * span * 0.45 + px * 6, cy - uy * span * 0.45 + py * 6),
                 (cx + px * 2, cy + py * 2),
                 (cx + ux * span * 0.45 - px * 6, cy + uy * span * 0.45 - py * 6)],
                3.0, fur_n, rng, min(22.0, dep * 1.1), 'bite',
            )
            ov = _paint_ink_wound(
                ov,
                [(cx - ux * span * 0.85 - px * 9, cy - uy * span * 0.85 - py * 9),
                 (cx - px * 9, cy - py * 9),
                 (cx + ux * span * 0.85 - px * 9, cy + uy * span * 0.85 - py * 9)],
                2.6, fur_n, rng, min(18.0, dep * 0.95), 'gash',
            )
            valid_comp = comp
        elif rname == 'torso':
            cx, cy, dep, ux, uy, comp = find_safe_pocket('torso', prefer_qy=0.48, prefer_qx=0.48)
            # Three asymmetric diagonal claw rakes strictly inside ribs/flank interior
            span = min(44.0, max(18.0, dep * 1.85))
            # Diagonal claw direction (~55 deg down-right)
            dx, dy = 0.56, 0.83
            px, py = -dy, dx
            ov = _paint_ink_wound(ov, [(cx - dx * span - px * 11, cy - dy * span - py * 11), (cx - px * 11, cy - py * 11), (cx + dx * span - px * 11, cy + dy * span - py * 11)], 3.7, fur_n, rng, min(28.0, dep * 1.15), 'gash')
            ov = _paint_ink_wound(ov, [(cx - dx * span * 0.82 + px * 5, cy - dy * span * 0.82 + py * 5), (cx + px * 5, cy + py * 5), (cx + dx * span * 0.82 + px * 5, cy + dy * span * 0.82 + py * 5)], 3.0, fur_n, rng, min(22.0, dep * 1.0), 'gash')
            ov = _paint_ink_wound(ov, [(cx - dx * span * 0.55 + px * 19, cy - dy * span * 0.55 + py * 19), (cx + dx * span * 0.55 + px * 19, cy + dy * span * 0.55 + py * 19)], 2.2, fur_n, rng, min(14.0, dep * 0.8), 'graze')
            valid_comp = comp
        elif rname == 'forelegs':
            # Anchor strictly in the broad upper shoulder/forearm interior (NEVER on forepaws or leg-gap ink lines!)
            cx, cy, dep, ux, uy, comp = find_safe_pocket('forelegs', prefer_qy=0.28, prefer_qx=0.52)
            span = min(28.0, max(10.0, dep * 1.55))
            px, py = -uy, ux
            ov = _paint_ink_wound(
                ov,
                [(cx - ux * span, cy - uy * span), (cx, cy), (cx + ux * span, cy + uy * span)],
                2.8, fur_n, rng, min(18.0, dep * 1.0), 'gash',
            )
            if dep >= 11.0:
                ov = _paint_ink_wound(
                    ov,
                    [(cx + px * 7 - ux * span * 0.6, cy + py * 7 - uy * span * 0.6),
                     (cx + px * 7 + ux * span * 0.6, cy + py * 7 + uy * span * 0.6)],
                    2.2, fur_n, rng, min(13.0, dep * 0.8), 'graze',
                )
            valid_comp = comp
        elif rname == 'hindlegs':
            # Anchor strictly in the muscular upper haunch/thigh interior (away from hock & leg-gap ink lines)
            cx, cy, dep, ux, uy, comp = find_safe_pocket('hindlegs', prefer_qy=0.32, prefer_qx=0.46)
            span = min(36.0, max(14.0, dep * 1.65))
            px, py = -uy, ux
            ov = _paint_ink_wound(ov, [(cx - ux * span, cy - uy * span), (cx, cy), (cx + ux * span, cy + uy * span)], 3.4, fur_n, rng, min(24.0, dep * 1.1), 'gash')
            ov = _paint_ink_wound(ov, [(cx + px * 10 - ux * span * 0.72, cy + py * 10 - uy * span * 0.72), (cx + px * 10 + ux * span * 0.72, cy + py * 10 + uy * span * 0.72)], 2.4, fur_n, rng, min(16.0, dep * 0.85), 'gash')
            valid_comp = comp
        elif rname == 'tail':
            cx, cy, dep, ux, uy, comp = find_safe_pocket('tail', prefer_qy=0.50, prefer_qx=0.52)
            span = min(24.0, max(10.0, dep * 1.4))
            px, py = -uy, ux
            ov = _paint_ink_wound(ov, [(cx - ux * span, cy - uy * span), (cx, cy), (cx + ux * span, cy + uy * span)], 2.7, fur_n, rng, min(15.0, dep * 0.9), 'gash')
            ov = _paint_ink_wound(ov, [(cx + px * 5 - ux * span * 0.35, cy + py * 5 - uy * span * 0.35), (cx + px * 5 + ux * span * 0.35, cy + py * 5 + uy * span * 0.35)], 2.2, fur_n, rng, min(12.0, dep * 0.75), 'bite')
            valid_comp = comp

        # Strictly clip to this ink-bounded fur component AND mask out dark 48_LINEART strokes so
        # wounds NEVER paint over inked lines separating limbs or body parts!
        comp_soft = np.clip(feather(dilate(valid_comp, 1), 1.8) * 1.35, 0.0, 1.0)
        reg_soft = np.clip(feather(reg_a[rname] > 0.15, 1.5) * 1.4, 0.0, 1.0)
        ink_keepout = 1.0 - np.clip(ink_a * 1.45, 0.0, 1.0)
        clip_m = comp_soft * reg_soft * np.clip(sil_a * 1.4, 0.0, 1.0) * ink_keepout
        if rname == 'head':
            clip_m *= (1.0 - np.clip(feather(eye_a > 0.1, 2.0) * 1.6, 0.0, 1.0))
        oarr = np.asarray(ov, np.float32)
        oarr[..., 3] *= clip_m
        rgba = oarr.clip(0, 255).astype(np.uint8)
        Image.fromarray(rgba).save(out_dir / f'{rname}.png', optimize=True)
        counts[rname] = int((rgba[..., 3] > 8).sum())

    return counts
