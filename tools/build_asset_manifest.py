#!/usr/bin/env python3
"""Generate Data/Wolves/assets/manifest.json by scanning Data/Wolves/assets/."""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / 'Data/Wolves/assets'
DOLLS = ('female-small', 'female-medium', 'female-large', 'male-small', 'male-medium', 'male-large')


def scan_doll(doll_dir: Path) -> dict:
    poses = sorted(p.stem for p in (doll_dir / 'poses').glob('*.zip')) if (doll_dir / 'poses').exists() else []
    expressions: dict[str, list[str]] = {}
    exp_dir = doll_dir / 'expressions'
    if exp_dir.exists():
        for pose_dir in sorted(d for d in exp_dir.iterdir() if d.is_dir()):
            items = sorted(p.stem for p in pose_dir.glob('*.zip'))
            if items:
                expressions[pose_dir.name] = items
    damage: dict[str, list[str]] = {}
    dmg_dir = doll_dir / 'damage'
    if dmg_dir.exists():
        for pose_dir in sorted(d for d in dmg_dir.iterdir() if d.is_dir()):
            items = sorted(p.stem for p in pose_dir.glob('*.png'))
            if items:
                damage[pose_dir.name] = items
    anatomy: dict[str, list[str]] = {}
    ana_dir = doll_dir / 'anatomy'
    if ana_dir.exists():
        for pose_dir in sorted(d for d in ana_dir.iterdir() if d.is_dir()):
            zips = [p.stem for p in pose_dir.glob('*.zip')]
            clips = sorted({p.stem.removesuffix('_erase').removesuffix('_overlay') for p in pose_dir.glob('clipped_ear_*.png')})
            items = sorted(set(zips + clips))
            if items:
                anatomy[pose_dir.name] = items
    markings: dict[str, list[str]] = {}
    mk_dir = doll_dir / 'markings'
    if mk_dir.exists():
        for pose_dir in sorted(d for d in mk_dir.iterdir() if d.is_dir()):
            items = sorted(p.stem for p in pose_dir.glob('*.png'))
            if items:
                markings[pose_dir.name] = items
    wearables: dict[str, list[str]] = {}
    wr_dir = doll_dir / 'wearables'
    if wr_dir.exists():
        for pose_dir in sorted(d for d in wr_dir.iterdir() if d.is_dir()):
            items = sorted(p.stem for p in pose_dir.glob('*.png'))
            if items:
                wearables[pose_dir.name] = items
    faces = sorted(p.name for p in (doll_dir / 'faces').glob('*') if p.name != '.gitkeep') if (doll_dir / 'faces').exists() else []
    return {
        'poses': poses,
        'expressions': expressions,
        'damage': damage,
        'anatomy': anatomy,
        'markings': markings,
        'wearables': wearables,
        'faces': faces,
    }


def build_manifest() -> dict:
    return {
        'format': 'ratw-wolf-assets',
        'version': 1,
        'canvas': [1024, 1024],
        'layerCount': 48,
        'dolls': {d: scan_doll(ASSETS / d) for d in DOLLS},
        'sharedFaces': sorted(p.name for p in (ASSETS / 'faces').glob('*') if p.name != '.gitkeep') if (ASSETS / 'faces').exists() else [],
    }


def main() -> int:
    manifest = build_manifest()
    out = ASSETS / 'manifest.json'
    out.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'Wrote {out.relative_to(ROOT)}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
