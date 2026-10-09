// Town projects on the page (Docs/Design/57-changing-the-world.md, 4): the progress line a site panel and the board show.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {projectProgress} from '../ui/hud/project.ts';

test('a project\'s progress names its hours and each material, had of needed', () => {
    const line = projectProgress({worked: 8.4, hours: 20, needs: [{name: 'Timber (log)', have: 4, need: 12}, {name: 'Cord (hank)', have: 6, need: 6}]});
    assert.equal(line, '8 of 20 work-hours · Timber (log) 4/12 · Cord (hank) 6/6');
});

test('a project with no materials named shows its hours alone', () => {
    assert.equal(projectProgress({worked: 0, hours: 30}), '0 of 30 work-hours');
});
