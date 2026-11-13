// What the page bakes in, for a world to compare (Docs/Design/77-central-accounts-and-hosted-worlds.md, 5.1): the
// protocol it speaks (Core/RatwBuildInfo.h) and the client-data hash of the repository files it is built from. The
// same hash as tools/client_data_hash.py and CMake's RATW_CLIENT_DATA_FILES: keep the three lists the same.
import {createHash} from 'node:crypto';
import {readFileSync} from 'node:fs';
import {join, resolve} from 'node:path';

export const ClientDataFiles = ['Data/Terrain/terrain.json', 'Core/RatwTerrainCatalog.inc', 'Core/RatwFurPalette.inc', 'Core/RatwStep.h', 'Core/RatwStep.cpp',
    'Client/wasm/walk.cpp', 'Core/RatwPack.h', 'Core/RatwPack.cpp', 'Core/RatwMotionCore.h', 'Core/RatwMotionCore.cpp', 'Core/RatwLink.h'];

const repository = resolve(import.meta.dirname, '..');

export function clientDataHash(root = repository) {
    let manifest = 'ratw-client-data 1\n';
    for (const name of ClientDataFiles) {
        const content = readFileSync(join(root, name)).toString('latin1').replaceAll('\r\n', '\n');
        manifest += `${name} ${createHash('sha256').update(Buffer.from(content, 'latin1')).digest('hex')}\n`;
    }
    return createHash('sha256').update(manifest, 'utf8').digest('hex').slice(0, 32);
}

export function protocol(root = repository) {
    const found = /constexpr int Protocol = (\d+);/.exec(readFileSync(join(root, 'Core/RatwBuildInfo.h'), 'utf8'));
    if (!found) throw new Error('No protocol in Core/RatwBuildInfo.h');
    return Number(found[1]);
}
