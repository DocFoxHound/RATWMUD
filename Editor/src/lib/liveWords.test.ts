import assert from 'node:assert/strict';
import {test} from 'node:test';
import {eventLine, looksLikeId} from './liveWords.ts';

const place = (cell: string) => ({sunfields: 'The Sunfields, Southmeadow'} as Record<string, string>)[cell] ?? cell;

test('the LIVE feed names people, never ids (R1, shot 27)', () => {
    const e = {kind: 'crier', actor: 'tn_lf_rowan_downridge', target: '', cell: 'sunfields', detail: 'Hear ye! Silver in the hills!'};
    assert.equal(eventLine(e, place, id => ({tn_lf_rowan_downridge: 'Rowan Downridge'} as Record<string, string>)[id]),
        'crier (Rowan Downridge) in The Sunfields, Southmeadow: Hear ye! Silver in the hills!');
    // Not known to the page: left out, never the id.
    assert.equal(eventLine(e, place), 'crier in The Sunfields, Southmeadow: Hear ye! Silver in the hills!');
    assert.equal(eventLine({...e, actor: 'tn_a', target: 'Pell'}, place), 'crier (Pell) in The Sunfields, Southmeadow: Hear ye! Silver in the hills!');
    // A place the page can't name isn't shown by its id either.
    assert.equal(eventLine({...e, cell: 'cell_12_4', detail: ''}, place), 'crier');
    assert.ok(looksLikeId('cty_nell_plummeadow') && looksLikeId('3f2a9c105b7e0d41a6c3') && !looksLikeId('Rowan') && !looksLikeId('rowan'));
});
