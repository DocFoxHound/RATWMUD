#!/usr/bin/env python3
"""The weather gallery and a restart, in the browser client, on a disposable Atlas glade."""
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
    run = Path(tempfile.mkdtemp(prefix='weather-', dir=tests))
    script = '''
import * as m from './Editor/src/model/model.mjs';
const p=m.createProject(32,24,'Juniper weather study');
p.cells[0].terrain=Array.from({length:24},(_,y)=>Array.from({length:32},(_,x)=>{
 if(x===0||y===0||x===31||y===23)return '#';
 if(x>23&&x<29&&y>4&&y<11)return '~';
 if((x<8&&y<8)||(x<7&&y>16)||(x>23&&y>16))return ((x+y)%3===0)?'"':',';
 if((y===8&&x>4&&x<12)||(x===9&&y>7&&y<11))return '#';
 if((x===19||x===20)&&y>3&&y<8)return '^';
 return (x>13&&x<19)||(y>10&&y<15)?'.':',';
}).join(''));
m.cutGrid(p,32,24);p.cells[0].name='Juniper Crossing';
p.cells[0].description='A pale trail divides the juniper glade. To the northeast, a spring rests beneath a low rise; old stone walls shelter the western brush.';
const r=m.addRoom(p,16,12,'The Lamplit Shelter');r.z=0;
r.description='Warm lamplight rests over a quiet wooden floor. Beyond the door, the glade is exposed to the changing sky.';
r.terrain=Array.from({length:12},(_,y)=>Array.from({length:16},(_,x)=>
 x===0||y===0||x===15||y===11?'#':(y===3&&x>3&&x<8?'T':'.')).join(''));
m.addLink(p,{id:'shelter',name:'Shelter door',kind:'door',open:false,
 a:{cell:p.cells[0].id,x:17,y:12},b:{cell:r.id,x:8,y:8}});
p.spawn={cell:p.cells[0].id,x:16,y:12};console.log(JSON.stringify(p));
'''
    project = json.loads(subprocess.check_output(['node', '--input-type=module', '-e', script], cwd=root, text=True))
    return map_editor.write_export(project, run / 'export'), run / 'world.json'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--headless', action='store_true', help='No browser and no screenshots')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    manifest, save = prepare(root)
    series(('weather', 'weather-restore'), save, manifest, headless=args.headless,
           screenshots=('17-weather-day.png', '18-weather-rain.png', '19-weather-snow.png', '20-weather-fog.png',
                        '21-weather-night.png', '22-weather-shelter.png'))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
