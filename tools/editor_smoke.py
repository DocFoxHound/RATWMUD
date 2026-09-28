#!/usr/bin/env python3
"""Exercise JS edits -> Python export -> C++ importer -> optionally, the world played in the browser client."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

import map_editor
from game_run import series


def fixture(root):
    # Use the actual browser model, not a hand-written equivalent.
    script = '''
import * as m from './Editor/src/model/model.mjs';
const p=m.createProject(64,48,'Atlas integration');
m.paint(p,29,10,'~',6);
m.setHeight(p,43,8,.5);
m.cutGrid(p);
const room=m.addRoom(p,16,12,'The Quiet Annex');
m.addLink(p,{id:'inn_entry',name:'Annex door',kind:'door',open:false,
 a:{cell:p.cells[1].id,x:10,y:10},b:{cell:room.id,x:7,y:10}});
const joined=m.mergeCells(p,[p.cells[0].id,p.cells[1].id]);
m.splitCell(p,joined.id,'x',32);
p.spawn={cell:p.cells.find(c=>c.x===0&&c.y===0).id,x:29,y:18};
const errors=m.validate(p).errors;if(errors.length)throw Error(errors.join('; '));
console.log(JSON.stringify(p));
'''
    raw = subprocess.check_output(['node', '--input-type=module', '-e', script], cwd=root, text=True)
    p = json.loads(raw)
    ground = lambda x, y: next(c['terrain'][y - c['y']][x - c['x']] for c in p['cells']
                               if c['x'] <= x < c['x'] + c['width'] and c['y'] <= y < c['y'] + c['height'])
    assert ''.join(ground(x, 10) for x in range(31, 34)) == '~~~', 'Painting did not cross cut boundary'
    linked = next(c for c in p['cells'] if c['id'] == p['links'][0]['a']['cell'])
    assert linked['x'] + p['links'][0]['a']['x'] == 42, 'Split/merge moved a door anchor'
    return p


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--play', '--engine', dest='play', action='store_true', help='Also play the exported world in the game')
    parser.add_argument('--headless', action='store_true', help='No browser and no screenshots')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    run = Path(tempfile.mkdtemp(prefix='atlas-', dir=root / 'Saved/Tests'))
    p = fixture(root)
    manifest = map_editor.write_export(p, run / 'export')
    probe = subprocess.run([str(root / 'build-core/authoring_tests'), '--probe', str(manifest)],
                           cwd=root, text=True, capture_output=True, timeout=120)
    print(probe.stdout, end='', flush=True)
    if probe.returncode:
        raise RuntimeError('C++ rejected browser-model/Python export: ' + probe.stderr + probe.stdout)
    print('PASS: paint, cut, merge, split and interior link retain their coordinates through real runtime import.', flush=True)
    if not args.play:
        print(f'Fixture: {manifest}', flush=True)
        return 0
    series(('atlas',), run / 'world.json', manifest, headless=args.headless,
           screenshots=('13-authored-cell-runtime.png', '14-authored-interior-runtime.png'), flags=())
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
