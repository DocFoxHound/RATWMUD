// The client-data hash the page is built with is the one a world computes (tools/client_data_hash.py, CMake).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {join} from 'node:path';
// @ts-ignore (a plain module of the build's, with no types)
import {clientDataHash, protocol} from '../../build-info.mjs';

const root = join(import.meta.dirname, '..', '..', '..');

test('the client-data hash matches the tool the worlds use', () => {
    const tool = execFileSync('python3', [join(root, 'tools', 'client_data_hash.py')], {encoding: 'utf8'}).trim();
    assert.match(tool, /^[0-9a-f]{32}$/);
    assert.equal(clientDataHash(), tool);
});

test('the protocol is the one in Core/RatwBuildInfo.h', () => {
    assert.equal(protocol(), 1);
});
