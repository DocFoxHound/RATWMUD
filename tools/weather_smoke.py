#!/usr/bin/env python3
"""Native weather gallery and restart regression with a disposable Atlas glade."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time

import map_editor


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
    return map_editor.write_export(project, run / 'export'), run / 'world.sqlite'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--headless', action='store_true')
    parser.add_argument('--packaged', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    manifest, save = prepare(root)
    evidence = root / 'artifacts' / ('packaged-evidence' if args.packaged else 'screenshots')
    evidence.mkdir(parents=True, exist_ok=True)
    logs = root / 'artifacts/logs'
    logs.mkdir(parents=True, exist_ok=True)
    engine = Path(os.environ.get('RATW_UNREAL_ROOT', '/home/martinb/Applications/UnrealEngine/5.8.2'))
    base = ([str(root / 'artifacts/package/Linux/RATWMUD/Binaries/Linux/RATWMUD')]
            if args.packaged else [str(engine / 'Engine/Binaries/Linux/UnrealEditor'), str(root / 'RATWMUD.uproject')])
    for scenario in ('weather', 'weather-restore'):
        capture = scenario == 'weather' and not args.headless
        command = base + ['/Engine/Maps/Entry', '-game', '-NoSplash', '-NoSound', '-Unattended',
                          '-noscreenmessages', '-ForceLogFlush', '-RatwIdentity=ash', '-RatwName=Ash', '-RatwDevIdentity',
                          '-RatwDevTools', f'-RatwScenario={scenario}', f'-RatwWorld={manifest}',
                          f'-RatwSave={save}', f'-RatwCaptureDir={evidence}']
        command += (['-windowed', '-ResX=1600', '-ResY=1000', '-ForceRes', '-RenderOffscreen', '-RatwCaptureWeather']
                    if capture else ['-nullrhi'])
        kind = 'packaged' if args.packaged else 'native'
        log = logs / f'{scenario}-{kind}-smoke.log'
        started = time.time_ns()
        child = None
        try:
            with log.open('w') as output:
                child = subprocess.Popen(command, cwd=root, stdout=output, stderr=subprocess.STDOUT)
                code = child.wait(timeout=160)
            result = evidence / f'{scenario}-ash.json'
            if not result.exists() or result.stat().st_mtime_ns < started:
                raise RuntimeError(f'No fresh {scenario} evidence. See {log}')
            report = json.loads(result.read_text())
            if code or not report['passed']:
                raise RuntimeError(f'{scenario} failed: {report["detail"]}. See {log}')
            if capture:
                for name in ('17-weather-day.png', '18-weather-rain.png', '19-weather-snow.png',
                             '20-weather-fog.png', '21-weather-night.png', '22-weather-shelter.png'):
                    shot = evidence / name
                    if not shot.exists() or shot.stat().st_mtime_ns < started or shot.stat().st_size < 1024:
                        raise RuntimeError(f'No fresh screenshot: {name}')
            print('PASS: ' + report['detail'], flush=True)
        finally:
            if child and child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait(timeout=10)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
