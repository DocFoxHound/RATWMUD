// Taverns on the page (Docs/Design/54-gathering-places.md, Phase 1): the rest label in a common room, with company, and
// on another's bed. The rules are the server's (Tests/gathering_tests.cpp).
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {restLabel} from './labels.ts';

test('taverns: the rest label', () => {
    assert.match(restLabel({rest: {hours: 1, bed: false, room: true, rate: 1.25}}), /resting in the common room · 1\.25$/);
    assert.match(restLabel({rest: {hours: 1, bed: false, room: true, rate: 1.63}}), /common room · 1\.63 with company/);
    assert.match(restLabel({rest: {hours: 1, bed: false, notYours: true}}), /not your bed: a partial rest/);
    assert.match(restLabel({rest: {hours: 2, bed: true, full: 6}}), /resting in a bed · 2\.0 of 6 h/);
});
