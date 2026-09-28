#!/usr/bin/env python3
"""Pace and known travel in the browser client, on a disposable authored map and save."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

import map_editor
from game_run import series


def prepare(root):
    run = Path(tempfile.mkdtemp(prefix='travel-', dir=root / 'Saved/Tests'))
    script = '''
import * as m from './Editor/src/model/model.mjs';
const p=m.createProject(72,12,'Travel testing reach');m.cutGrid(p,24,12);
p.cells.forEach((c,i)=>{c.name=['West Meadow','Crossing Fields','Eastern Reach'][i];
c.description='Open travel ground. The familiar route follows the old trail.';});
const r=m.addRoom(p,16,12,'The Wayside Shelter');r.z=0;
r.description='A quiet shelter at the edge of the familiar trail.';
m.addLink(p,{id:'shelter',name:'Shelter door',kind:'door',open:false,
 a:{cell:p.cells[1].id,x:12,y:8},b:{cell:r.id,x:6,y:8}});
p.spawn={cell:p.cells[0].id,x:5,y:6};console.log(JSON.stringify(p));
'''
    project = json.loads(subprocess.check_output(['node', '--input-type=module', '-e', script], cwd=root, text=True))
    manifest = map_editor.write_export(project, run / 'export')
    memories = []
    # Synthetic fixture represents a previously explored route, not a gameplay
    # reveal command. This save is newly created for this test process only.
    for cell in project['cells'] + project['rooms']:
        cell_text = (manifest.parent / 'cells' / f'{cell["id"]}.cell').read_text()
        glyphs = ''.join(cell_text.split('grid:\n', 1)[1].splitlines())
        memories.append(dict(observer='player-ash', id=cell['id'], name=cell['name'], knowledge=2,
                             width=cell['width'], height=cell['height'],
                             x=cell.get('x', cell.get('worldX', 0)),
                             y=cell.get('y', cell.get('worldY', 0)), z=cell['z'],
                             glyphs=glyphs, observed='1' * len(glyphs)))
    state = dict(schema=1, revision=0, sequence=1, time=0,
                 players=[dict(id='player-ash', name='Ash', cell='cell_1', x=5.5, y=6.5,
                               posture='standing', color=0, dexterity=50, stamina=100, pace=0)],
                 mapMemories=memories)
    save = run / 'world.json'
    save.write_text(json.dumps(state))
    return manifest, save


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--headless', action='store_true', help='No browser and no screenshots')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    manifest, save = prepare(root)
    series(('travel',), save, manifest, headless=args.headless, screenshots=('15-travel-pace.png', '16-known-routes.png'))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
