#!/usr/bin/env python3
"""Real-client cell atmosphere gallery, authoring import and saved-light restart."""
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
    for scenario in ('lighting', 'lighting-restore'):
        capture = scenario == 'lighting' and not args.headless
        command = base + ['/Engine/Maps/Entry', '-game', '-NoSplash', '-NoSound', '-Unattended',
                          '-noscreenmessages', '-ForceLogFlush', '-RatwIdentity=ash', '-RatwName=Ash', '-RatwDevIdentity',
                          '-RatwDevTools', f'-RatwScenario={scenario}', f'-RatwWorld={manifest}',
                          f'-RatwSave={save}', f'-RatwCaptureDir={evidence}']
        command += (['-windowed', '-ResX=1600', '-ResY=1000', '-ForceRes', '-RenderOffscreen', '-RatwCaptureLighting']
                    if capture else ['-nullrhi'])
        kind = 'packaged' if args.packaged else 'native'
        log = logs / f'{scenario}-{kind}-smoke.log'
        started = time.time_ns()
        child = None
        try:
            with log.open('w') as output:
                child = subprocess.Popen(command, cwd=root, stdout=output, stderr=subprocess.STDOUT)
                code = child.wait(timeout=170)
            result = evidence / f'{scenario}-ash.json'
            if not result.exists() or result.stat().st_mtime_ns < started:
                raise RuntimeError(f'No fresh {scenario} evidence. See {log}')
            report = json.loads(result.read_text())
            if code or not report['passed']:
                raise RuntimeError(f'{scenario} failed: {report["detail"]}. See {log}')
            if capture:
                for name in ('23-tavern-day.png', '24-tavern-warm-night.png', '25-tavern-unlit-night.png',
                             '26-unlit-cellar-day.png', '27-tavern-cool-night.png', '28-outdoor-night-edges.png'):
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
