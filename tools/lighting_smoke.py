#!/usr/bin/env python3
"""Cell atmosphere in the browser client: authored lighting, a gallery, and saved light across a restart."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

import map_editor
from game_run import series


def prepare(root):
    tests = root / 'Saved/Tests'
    tests.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='lighting-', dir=tests))
    script = '''
import * as m from './Editor/src/model/model.mjs';
const p=m.createProject(32,24,'Lighting study');
p.cells[0].terrain=Array.from({length:24},(_,y)=>Array.from({length:32},(_,x)=>
 x===0||y===0||x===31||y===23?'#':x>12&&x<20?'.':(x+y)%5===0?'"':',').join(''));
m.cutGrid(p,32,24);p.cells[0].name='The Night Glade';
p.cells[0].description='A trail runs through the brush toward a sheltered tavern.';
const r=m.addRoom(p,20,14,'The Bent Branch');r.z=0;
r.description='Wooden tables flank a worn central floor. A broad doorway leads to the glade.';
r.lighting={artificial:1,daylightAccess:1,tone:'warm'};
r.terrain=Array.from({length:14},(_,y)=>Array.from({length:20},(_,x)=>
 x===0||y===0||x===19||y===13?'#':(y===4||y===10)&&(x===4||x===5||x===14||x===15)?'T':'.').join(''));
m.addLink(p,{id:'tavern',name:'Tavern door',kind:'door',open:false,
 a:{cell:p.cells[0].id,x:16,y:12},b:{cell:r.id,x:10,y:2}});
p.spawn={cell:r.id,x:10,y:7};console.log(JSON.stringify(p));
'''
    project = json.loads(subprocess.check_output(['node', '--input-type=module', '-e', script], cwd=root, text=True))
    return map_editor.write_export(project, run / 'export'), run / 'world.json'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--headless', action='store_true', help='No browser and no screenshots')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    manifest, save = prepare(root)
    series(('lighting', 'lighting-restore'), save, manifest, headless=args.headless,
           screenshots=('23-tavern-day.png', '24-tavern-warm-night.png', '25-tavern-unlit-night.png', '26-unlit-cellar-day.png',
                        '27-tavern-cool-night.png', '28-outdoor-night-edges.png'))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
