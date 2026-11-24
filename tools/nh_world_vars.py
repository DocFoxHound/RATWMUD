"""The Ninth Howl's world variables (Docs/Design/86 10.1 step 2): check the declarations file, print the console steps, or
declare them for the DM.

  python3 tools/nh_world_vars.py                      check the file (names, types, first values, notes) and that every
                                                      program under Data/Storylines/ridgemere3 declares its world
                                                      variables with the file's type
  python3 tools/nh_world_vars.py --world ridgemere3   also compare with what the world already declares (read only)
  python3 tools/nh_world_vars.py --steps              the same steps by hand, for Campaigns > World variables
  python3 tools/nh_world_vars.py --apply --world ridgemere3 --who NAME --reason "..."
                                                      declare every variable (op world-var, do: declare; audited
                                                      story.world-var), in one transaction. The DM runs this; a variable
                                                      already declared with the same type keeps its value.

The file is Data/Storylines/ridgemere3/ninth-howl/world-vars.json (or --file). Nothing is written without --apply.
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
import story_runner  # noqa: E402
import world_db  # noqa: E402

FILE = ROOT / 'Data' / 'Storylines' / 'ridgemere3' / 'ninth-howl' / 'world-vars.json'
PROGRAMS = ROOT / 'Data' / 'Storylines' / 'ridgemere3'


def credentials():
    """A worktree has no Database/.env of its own: borrow the main checkout's (into this process's environment only)."""
    if world_db.ENV_FILE.is_file():
        return
    try:
        common = subprocess.run(['git', '-C', str(ROOT), 'rev-parse', '--git-common-dir'], capture_output=True, text=True,
                                check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return
    env = (ROOT / common).resolve().parent / 'Database' / '.env'
    for k, v in world_db.read_env(env).items():
        if k.startswith(('RATW_', 'POSTGRES_')):
            os.environ.setdefault(k, v)


def load(path):
    doc = json.loads(Path(path).read_text())
    if doc.get('format') != 'ratw-world-vars' or doc.get('version') != 1 or not isinstance(doc.get('variables'), list):
        raise ValueError(f'{path}: not a ratw-world-vars file (version 1).')
    return doc


def check(doc):
    """Problems with the declarations themselves, as the console's declare would refuse them."""
    out, seen = [], set()
    for d in doc['variables']:
        name, kind = d.get('name'), d.get('type')
        if not isinstance(name, str) or not story_runner.WORLD_VAR_NAME.match(name):
            out.append(f'{name!r}: not a world variable name')
            continue
        if name in seen:
            out.append(f'{name}: declared twice')
        seen.add(name)
        if kind not in story_runner.WORLD_VAR_TYPES:
            out.append(f'{name}: unknown type {kind!r}')
        elif not story_runner._world_var_ok(kind, d.get('value')):
            out.append(f'{name}: {d.get("value")!r} is not a first value for {kind}')
        if not isinstance(d.get('note', ''), str) or len(d.get('note', '')) > 300:
            out.append(f'{name}: a note of up to 300 characters')
    return out


def programs(root=PROGRAMS):
    for path in sorted(Path(root).rglob('*.json')):
        if path.name == 'world-vars.json':
            continue
        try:
            p = json.loads(path.read_text())
        except ValueError:
            continue
        if isinstance(p, dict) and p.get('format') == 'ratw-story-program':
            yield path, p


def against_programs(doc, root=PROGRAMS):
    """Every world variable a program declares must be in the file with the same type (else the DM declares it as
    something else, or not at all). A different first value is only a warning."""
    by = {d['name']: d for d in doc['variables']}
    errors, warnings = [], []
    for path, p in programs(root):
        for v in p.get('variables', []):
            if v.get('scope') != 'world':
                continue
            rel = path.relative_to(ROOT)
            d = by.get(v['id'])
            if d is None:
                errors.append(f'{rel}: world variable {v["id"]} is not in the declarations file')
            elif d['type'] != v['type']:
                errors.append(f'{rel}: {v["id"]} is a {v["type"]} here, a {d["type"]} in the declarations file')
            elif 'initial' in v and v['initial'] != d['value']:
                warnings.append(f'{rel}: {v["id"]} starts at {v["initial"]!r} here, {d["value"]!r} in the declarations file')
    return errors, warnings


def readonly(key):
    conn = world_db.connect(key, 'dm', autocommit=False)
    conn.execute('SET TRANSACTION READ ONLY')
    return conn


def against_world(doc, key):
    """What the world already declares (read only): a clash of type is an error, a different value a note."""
    errors, notes = [], []
    with readonly(key) as conn:
        world = conn.execute('SELECT id FROM world.worlds').fetchone()[0]
        have = {v['name']: v for v in story_runner.world_vars(conn, world)['variables']}
        conn.rollback()
    for d in doc['variables']:
        h = have.get(d['name'])
        if h is None or h.get('unset'):
            notes.append(f'{d["name"]}: not declared yet')
        elif h['type'] != d['type']:
            errors.append(f'{d["name"]}: the world has it as {h["type"]}, the file as {d["type"]}')
        elif h.get('value') != d['value']:
            notes.append(f'{d["name"]}: declared; holds {json.dumps(h.get("value"))} (first value {json.dumps(d["value"])}); kept')
    return errors, notes


def steps(doc):
    lines = ['In the Dungeon Master suite: Campaigns > World variables..., then for each row "Declare":',
             '(name | type | first value | note); the reason: "The Ninth Howl, doc 86 10.1".', '']
    group = None
    for d in doc['variables']:
        if d.get('group') != group:
            group = d.get('group')
            lines.append(f'## {group}')
        lines.append(f'{d["name"]} | {d["type"]} | {json.dumps(d["value"])} | {d.get("note", "")}')
    return '\n'.join(lines)


def apply(doc, key, who, reason):
    with world_db.connect(key, 'dm', autocommit=False) as conn:
        world = conn.execute('SELECT id FROM world.worlds').fetchone()[0]
        done = []
        for d in doc['variables']:
            r = story_runner.world_var(conn, world, who, {'do': 'declare', 'name': d['name'], 'type': d['type'],
                                                          'value': d['value'], 'note': d.get('note', ''), 'reason': reason})
            done.append(r['text'])
        conn.commit()
    return done


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--file', default=str(FILE))
    ap.add_argument('--world', default='', help='a world key (ridgemere3): compare with what it declares, read only')
    ap.add_argument('--steps', action='store_true', help='print the console steps')
    ap.add_argument('--apply', action='store_true', help='declare every variable in --world (the DM)')
    ap.add_argument('--who', default='', help='the DM account declaring them (with --apply)')
    ap.add_argument('--reason', default='', help='the reason kept in the audit (with --apply)')
    a = ap.parse_args(argv)
    doc = load(a.file)
    if a.steps:
        print(steps(doc))
        return 0
    bad = check(doc)
    errors, warnings = against_programs(doc)
    bad += errors
    for w in warnings:
        print('warning:', w)
    if a.world:
        credentials()
        key = world_db.resolve(a.world)
        if a.apply:
            if bad:
                print('\n'.join(['refused:'] + bad))
                return 1
            if not a.who or not a.reason:
                print('--apply needs --who and --reason')
                return 2
            for line in apply(doc, key, a.who, a.reason):
                print(line)
            return 0
        errors, notes = against_world(doc, key)
        bad += errors
        for n in notes:
            print('  ', n)
    elif a.apply:
        print('--apply needs --world')
        return 2
    print(f'{len(doc["variables"])} world variables: ' + ('PASS' if not bad else 'FAIL'))
    for b in bad:
        print('  error:', b)
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
