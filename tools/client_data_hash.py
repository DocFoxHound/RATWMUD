#!/usr/bin/env python3
"""The client-data hash (Docs/Design/77-central-accounts-and-hosted-worlds.md, 5.1): one hash of what the game page
bakes in from the repository (the terrain catalogue, the fur palette, the walking rules, the packed formats). A world reports it (its
lobby and GET /status); a client hides a world whose hash differs from its own. CMake makes the same hash at configure
time (CMakeLists.txt, RATW_CLIENT_DATA_FILES); keep the two lists the same.

  python3 tools/client_data_hash.py          print it
"""
from __future__ import annotations

import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FILES = ('Data/Terrain/terrain.json', 'Core/RatwTerrainCatalog.inc', 'Core/RatwFurPalette.inc', 'Core/RatwStep.h',
         'Core/RatwStep.cpp',
         'Client/wasm/walk.cpp', 'Core/RatwPack.h', 'Core/RatwPack.cpp', 'Core/RatwMotionCore.h',
         'Core/RatwMotionCore.cpp', 'Core/RatwLink.h')


def client_data_hash(root: Path = ROOT) -> str:
    manifest = 'ratw-client-data 1\n'
    for name in FILES:
        content = (root / name).read_bytes().replace(b'\r\n', b'\n')
        manifest += f'{name} {hashlib.sha256(content).hexdigest()}\n'
    return hashlib.sha256(manifest.encode('utf-8')).hexdigest()[:32]


if __name__ == '__main__':
    print(client_data_hash())
