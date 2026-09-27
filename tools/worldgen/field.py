"""Smooth deterministic fields for landforms: fractal value noise, bumps and blends (numpy only)."""
from __future__ import annotations

import numpy as np


def value_noise(width: int, height: int, cell: float, seed: int) -> np.ndarray:
    """Smoothly interpolated random lattice values in [0, 1], one lattice point every `cell` tiles."""
    rng = np.random.default_rng(seed)
    gw, gh = int(width / cell) + 3, int(height / cell) + 3
    lattice = rng.random((gh, gw), dtype=np.float32)
    ys, xs = np.mgrid[0:height, 0:width].astype(np.float32)
    u, v = xs / cell, ys / cell
    x0, y0 = np.floor(u).astype(int), np.floor(v).astype(int)
    fx, fy = u - x0, v - y0
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = lattice[y0, x0], lattice[y0, x0 + 1]
    c, d = lattice[y0 + 1, x0], lattice[y0 + 1, x0 + 1]
    return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy


def fbm(width: int, height: int, cell: float, seed: int, octaves: int = 4) -> np.ndarray:
    total, weight, norm = np.zeros((height, width), dtype=np.float32), 1.0, 0.0
    for o in range(octaves):
        total += weight * value_noise(width, height, cell / 2 ** o, seed + 101 * o)
        norm += weight
        weight *= .5
    return total / norm


def grid(width: int, height: int):
    ys, xs = np.mgrid[0:height, 0:width].astype(np.float32)
    return xs + .5, ys + .5


def bump(xs, ys, cx, cy, sx, sy, angle=0.0, power=2.0):
    """A smooth hill: 1 at (cx, cy), falling off over sx by sy tiles, turned by `angle` radians."""
    c, s = np.cos(angle), np.sin(angle)
    dx, dy = xs - cx, ys - cy
    u, v = (dx * c + dy * s) / sx, (-dx * s + dy * c) / sy
    return np.exp(-(u * u + v * v) ** (power / 2))


def smoothstep(a, b, v):
    t = np.clip((v - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def distance_to_mask(mask: np.ndarray, limit: int = 64) -> np.ndarray:
    """Chebyshev-ish distance (tiles) from each tile to the nearest True tile, capped at `limit`."""
    dist = np.where(mask, 0, limit).astype(np.float32)
    current = mask.copy()
    for d in range(1, limit):
        grown = current.copy()
        grown[1:, :] |= current[:-1, :]
        grown[:-1, :] |= current[1:, :]
        grown[:, 1:] |= current[:, :-1]
        grown[:, :-1] |= current[:, 1:]
        fresh = grown & ~current
        if not fresh.any():
            break
        dist[fresh] = d
        current = grown
    return dist
