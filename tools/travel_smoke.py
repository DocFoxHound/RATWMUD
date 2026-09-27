#!/usr/bin/env python3
"""Real Unreal pace/travel test using only a disposable authored map and save."""
import argparse
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import tempfile
import time

import map_editor


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
    save = run / 'world.sqlite'
    with sqlite3.connect(save) as database:
        database.execute('CREATE TABLE world_state (id INTEGER PRIMARY KEY CHECK(id=1), '
                         'schema_version INTEGER NOT NULL, revision INTEGER NOT NULL, payload TEXT NOT NULL)')
        database.execute('INSERT INTO world_state VALUES (1, 1, 0, ?)', (json.dumps(state),))
    return manifest, save


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--headless', action='store_true')
    parser.add_argument('--packaged', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    manifest, save = prepare(root)
    evidence = root / 'artifacts' / ('packaged-evidence' if args.packaged else 'screenshots')
    evidence.mkdir(parents=True, exist_ok=True)
    engine = Path(os.environ.get('RATW_UNREAL_ROOT', '/home/martinb/Applications/UnrealEngine/5.8.2'))
    base = ([str(root / 'artifacts/package/Linux/RATWMUD/Binaries/Linux/RATWMUD')]
            if args.packaged else [str(engine / 'Engine/Binaries/Linux/UnrealEditor'), str(root / 'RATWMUD.uproject')])
    command = base + ['/Engine/Maps/Entry', '-game', '-NoSplash', '-NoSound', '-Unattended',
                      '-noscreenmessages', '-ForceLogFlush', '-RatwIdentity=ash', '-RatwName=Ash', '-RatwDevIdentity',
                      '-RatwScenario=travel', f'-RatwWorld={manifest}', f'-RatwSave={save}',
                      f'-RatwCaptureDir={evidence}']
    command += ['-nullrhi'] if args.headless else ['-windowed', '-ResX=1600', '-ResY=1000',
                                                '-ForceRes', '-RenderOffscreen', '-RatwCaptureTravel']
    log = root / 'artifacts/logs' / ('travel-packaged-smoke.log' if args.packaged else 'travel-native-smoke.log')
    started = time.time_ns()
    child = None
    try:
        with log.open('w') as output:
            child = subprocess.Popen(command, cwd=root, stdout=output, stderr=subprocess.STDOUT)
            code = child.wait(timeout=170)
        result = evidence / 'travel-ash.json'
        if not result.exists() or result.stat().st_mtime_ns < started:
            raise RuntimeError(f'No fresh travel scenario evidence. See {log}')
        report = json.loads(result.read_text())
        if code or not report['passed']:
            raise RuntimeError(f'Travel scenario failed: {report["detail"]}. See {log}')
        if not args.headless:
            for name in ('15-travel-pace.png', '16-known-routes.png'):
                capture = evidence / name
                if not capture.exists() or capture.stat().st_mtime_ns < started or capture.stat().st_size < 1024:
                    raise RuntimeError(f'No fresh screenshot: {name}')
        print('PASS: ' + report['detail'])
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
