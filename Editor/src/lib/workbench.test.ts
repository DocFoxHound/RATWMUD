// The trigger workbench's logic (Docs/Design/85, Phase 4): sentences from the catalogue, the tree, moves, copy and paste,
// renames and uses, search, undo, problems' places, over every bundled example and every library draft, and the round
// trip: an edit undone, a move and its move back, a cut and its paste where it was, give the program back unchanged.
import test from 'node:test';
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readFileSync, readdirSync} from 'node:fs';
import {dirname, join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {
    actionParts, buildTree, commit, conditionParts, editable, eventParts, exitParts, fragmentOf, historyOf, insertTriggers, moveTrigger, namesOf,
    pasteFragment, plain, pretty, readFragment, redo, refOfProblem, removeTriggers, renameCounter, renameSignal, renameTrigger, search, signalsOf,
    countersOf, takenIds, triggersOf, undo, unique, usesOf, withEvent, type Catalogue, type Program, type Ref,
} from './workbench.ts';

const repo = join(dirname(fileURLToPath(import.meta.url)), '../../..');
const cat: Catalogue = JSON.parse(readFileSync(join(repo, 'Data/Storylines/trigger-catalogue.json'), 'utf8'));
const examples: Record<string, Program> = Object.fromEntries(readdirSync(join(repo, 'Data/Storylines/programs')).filter(f => f.endsWith('.json'))
    .map(f => [f.replace(/\.json$/, ''), JSON.parse(readFileSync(join(repo, 'Data/Storylines/programs', f), 'utf8'))]));
const drafts: Record<string, Program> = JSON.parse(execFileSync('python3', ['-c',
    "import json, sys\nsys.path.insert(0, 'tools')\nimport story_library as L\nprint(json.dumps({e['id']: L.draft(e['id']) for e in L.entries()}))"],
    {cwd: repo, maxBuffer: 1 << 26}).toString());
const all = {...examples, ...drafts};
const names = namesOf({residents: [{id: 'rm111', name: 'Ivo Tarmarsh'}], items: [{id: 'rope', name: 'Rope (coil)', category: 'tool'}]}, examples['reed-ford']);

test('the 8 examples and the 100 library drafts are all shown by the workbench', () => {
    assert.equal(Object.keys(examples).length, 8);
    assert.equal(Object.keys(drafts).length, 100);
    for (const [name, p] of Object.entries(all)) assert.ok(editable(p), name);
    assert.ok(!editable({...examples['reed-ford'], phases: 'x'}));
    assert.ok(!editable({...examples['reed-ford'], triggers: [{id: 't'}]}));
});

test('every trigger, condition, action, effect and exit of every program reads as a sentence from the catalogue', () => {
    let lines = 0;
    for (const [name, p] of Object.entries(all)) {
        const ns = namesOf(null, p);
        for (const scope of [null, ...p.phases.map((_, i) => i)]) for (const t of triggersOf(p, scope)) {
            const ev = plain(eventParts(cat, t, ns));
            assert.ok(ev && !ev.includes('{'), `${name} ${t.id}: ${ev}`);
            for (const a of t.actions) { const s = plain(actionParts(cat, a, ns)); assert.ok(s && !s.includes('{') && !s.includes('undefined'), `${name} ${t.id}: ${s}`); lines++; }
            if (t.if) assert.ok(plain(conditionParts(cat, t.if.test === 'all' || t.if.test === 'any' ? t.if.conditions[0] : t.if, ns)), name);
        }
        for (const ph of p.phases) {
            for (const x of ph.exits) assert.match(plain(exitParts(cat, x, ns, ph.missions ?? [])), /→ go to /, name);
            for (const e of ph.effects) assert.ok(!plain(actionParts(cat, {do: 'effect', effect: e}, ns)).includes('{'), name);
        }
    }
    assert.ok(lines > 250, String(lines));
});

test('sentences: filters are slots, unfiltered fields say "anyone", names replace ids', () => {
    const gift = {id: 'g', on: 'gift', match: {target: 'rm111', item: 'rope'}, actions: []};
    assert.equal(plain(eventParts(cat, gift, names)), cat.events.find(e => e.id === 'gift')!.sentence
        .replace('{actor}', cat.events.find(e => e.id === 'gift')!.fields.actor.any!).replace('{item}', 'Rope (coil)').replace('{target}', 'Ivo Tarmarsh'));
    assert.equal(plain(eventParts(cat, {on: 'story signal', signal: 'inspect'}, names)), 'the signal inspect arrives');
    assert.equal(plain(eventParts(cat, {on: 'clock', afterDays: 2}, names)), '2 game days after the phase opened');
    assert.equal(plain(actionParts(cat, {do: 'enter', phase: 'crossing'}, names)), 'Go to the phase crossing');
    assert.equal(plain(actionParts(cat, {do: 'add', scope: 'player', key: 'hauled', value: 'quantity'}, names)), "Add the event's quantity to the acting wolf's counter hauled");
    assert.equal(plain(actionParts(cat, {do: 'effect', effect: {kind: 'npc.walk', npc: 'rm111', cell: 'reed', x: 3, y: 4, pace: 'run', cue: 'go'}}, names)),
        'Ivo Tarmarsh walks to reed (3, 4) at a run, cue go');
    assert.equal(plain(actionParts(cat, {do: 'effect', effect: {kind: 'deed.award', player: '$actor', detail: 'Held the ford.'}}, names)),
        'Award the acting wolf the deed “Held the ford.”');
    assert.equal(plain(conditionParts(cat, {test: 'counter', scope: 'story', key: 'rope', op: '>=', value: 4}, names)), "the story's counter rope >= 4");
    // A filter the sentence has no slot for is said after it.
    assert.match(plain(eventParts(cat, {on: 'gift', match: {cell: 'reed'}}, names)), /, cell reed$/);
});

test('the tree: campaign first, folders in first-seen order, triggers numbered as they are tried, exits and missions', () => {
    const p = structuredClone(Object.values(all).find(x => x.phases.some(ph => (ph.triggers?.length ?? 0) >= 2))!);
    const tree = buildTree(p);
    assert.equal(tree[0].scope, null);
    assert.equal(tree.length, p.phases.length + 1);
    assert.ok(tree.some(b => b.opening));
    const ph = p.phases.findIndex(x => (x.triggers?.length ?? 0) >= 2);
    p.phases[ph].triggers![1].folder = 'Muster';
    const b = buildTree(p)[ph + 1];
    assert.deepEqual(b.folders.map(f => f.name), ['Muster']);
    assert.equal(b.folders[0].items[0].order, 2);
    assert.equal(b.loose[0].order, 1);
});

test('round trips: undo, move and back, cut and paste back, rename and back, give the program as it was', () => {
    for (const [name, p] of Object.entries(all)) {
        const text = pretty(p);
        let h = historyOf(text);
        const scope = p.triggers?.length ? null : p.phases.findIndex(ph => ph.triggers?.length);
        if (scope === -1) continue;
        const list = triggersOf(p, scope);
        // A move to the end and back.
        const there = moveTrigger(p, {phase: scope, index: 0}, {phase: scope, index: list.length});
        const back = moveTrigger(there.program, {phase: scope, index: (there.ref as {index: number}).index}, {phase: scope, index: 0});
        assert.equal(pretty(back.program), text, name + ': move and back');
        // A cut and a paste in the same place.
        const frag = fragmentOf(p, [{kind: 'trigger', phase: scope, index: 0}]);
        const cut = removeTriggers(p, [{kind: 'trigger', phase: scope, index: 0}]);
        assert.equal(pretty(pasteFragment(cut, frag, scope, 0, list[0].folder ?? undefined).program), text, name + ': cut and paste back');
        // A rename and back.
        const ref: Ref = {kind: 'trigger', phase: scope, index: 0};
        assert.equal(pretty(renameTrigger(renameTrigger(p, ref, 'renamed-x'), ref, list[0].id)), text, name + ': rename and back');
        for (const s of signalsOf(p).slice(0, 2)) assert.equal(pretty(renameSignal(renameSignal(p, s, 'zz-renamed'), 'zz-renamed', s)), text, name + ': signal and back');
        for (const c of countersOf(p).slice(0, 2)) assert.equal(pretty(renameCounter(renameCounter(p, c.scope, c.key, 'zz-c'), c.scope, 'zz-c', c.key)), text, name + ': counter and back');
        // Edits, then undo all of them.
        h = commit(h, pretty(there.program));
        const added = pretty(insertTriggers(p, scope, 0, [withEvent(list[0], 'gift')]).program);
        h = commit(h, added);
        h = undo(undo(h));
        assert.equal(h.present, text, name + ': undone');
        assert.equal(redo(redo(h)).present, added, name + ': redone');
    }
});

test('history: typing in one field is one step; a new edit clears what could be redone', () => {
    let h = historyOf('a');
    h = commit(h, 'ab', 'title'); h = commit(h, 'abc', 'title'); h = commit(h, 'abcd', 'other');
    assert.deepEqual(h.past, ['a', 'abc']);
    h = undo(h); assert.equal(h.present, 'abc');
    h = commit(h, 'x'); assert.deepEqual(h.future, []);
    assert.equal(commit(h, 'x'), h);
});

test('ids stay unique where triggers are tried together; pasted regions come along', () => {
    const p = structuredClone(examples['siege-at-greyfen']);
    const scope = p.phases.findIndex(ph => ph.triggers?.length);
    const t = p.phases[scope].triggers![0];
    const {program, indexes} = insertTriggers(p, scope, 0, [t, t]);
    const ids = triggersOf(program, scope).map(x => x.id);
    assert.equal(new Set(ids).size, ids.length);
    assert.equal(triggersOf(program, scope)[indexes[1]].id, unique(t.id + '-copy', takenIds(insertTriggers(p, scope, 0, [t]).program, scope)));
    const withRegion = triggersOf(p, null).length ? null : p.phases.flatMap((ph, i) => (ph.triggers ?? []).map((x, j) => ({x, i, j}))).find(({x}) => JSON.stringify(x).includes('"region"'));
    if (withRegion) {
        const f = fragmentOf(p, [{kind: 'trigger', phase: withRegion.i, index: withRegion.j}]);
        assert.ok(f.regions?.length);
        const bare = {...p, regions: []};
        const pasted = pasteFragment(bare, readFragment(JSON.stringify(f)), 0, 0);
        assert.deepEqual(pasted.regions, f.regions!.map(r => r.id));
    }
    assert.throws(() => readFragment('{"format":"ratw-story-program"}'), /not a trigger fragment/);
});

test('uses and search find a name wherever it is', () => {
    const p = examples['reed-ford'];
    const s = signalsOf(p)[0];
    const uses = usesOf(p, {type: 'signal', id: s});
    assert.ok(uses.length >= 2, 'sent and heard');
    assert.ok(uses.every(u => u.ref.kind === 'trigger' || u.ref.kind === 'missions'));
    const found = search(p, cat, namesOf(null, p), 'rope');
    assert.ok(found.some(f => f.ref.kind === 'trigger'));
    assert.ok(usesOf(p, {type: 'phase', id: p.phases[1].id}).length > 0);
});

test("a problem's place is a node of the tree", () => {
    const p = examples['winter-ferries'];
    assert.deepEqual(refOfProblem(p, {kind: 'trigger', phase: null, index: 0}), {kind: 'trigger', phase: null, index: 0});
    assert.deepEqual(refOfProblem(p, {kind: 'trigger', phase: 1, id: p.phases[1].triggers?.[0]?.id}), p.phases[1].triggers?.length ? {kind: 'trigger', phase: 1, index: 0} : {kind: 'phase', phase: 1});
    assert.deepEqual(refOfProblem(p, {kind: 'exit', phase: 0, index: 99}), {kind: 'phase', phase: 0});
    assert.deepEqual(refOfProblem(p, {kind: 'region', index: 0}), {kind: 'regions'});
    assert.equal(refOfProblem(p, {kind: 'source'}), null);
});

test("the console's words come from the catalogue when it has it (condText, eventText)", async () => {
    const {condText, eventText} = await import('./campaigns.ts');
    const {wordingOf} = await import('./workbench.ts');
    const w = wordingOf(cat, {rm111: 'Ivo Tarmarsh'});
    assert.equal(condText({test: 'counter', key: 'stores', scope: 'story', op: '>=', expected: 5, value: 2, verdict: false}, {}, w),
        "the story's counter stores >= 5 ✘ (stores is 2)");
    assert.equal(condText({test: 'counter', key: 'stores', scope: 'story', op: '>=', expected: 5, value: 2, verdict: false}), 'stores ≥ 5 ✘ (stores is 2)');
    assert.equal(eventText({on: 'gift', match: {target: 'rm111'}}, {}, w), 'when ' + plain(eventParts(cat, {on: 'gift', match: {target: 'rm111'}}, namesOf(null)))
        .replace('rm111', 'Ivo Tarmarsh'));
    assert.equal(eventText({on: 'story signal', signal: 'watch-round'}, {}, w), 'when the signal watch-round arrives');
    assert.equal(eventText({on: 'dm signal', match: {detail: 'accord'}}, {}, w), 'when a Dungeon Master directs "accord"');
});

// ------------------------------------------------------------------------------------- version 2 (doc 85, Phase 5)
import {actionItem, allActions, allConditions, mapActions, newAction, newCondition, partOf, renameVariable, variablesOf} from './workbench.ts';

const v2: Record<string, Program> = JSON.parse(execFileSync('python3', ['-c',
    "import json, sys\nsys.path.insert(0, 'tools')\nimport story_v2_samples as S, story_program as P, story_library as L, story_runner as R\n"
    + "out = {p['id']: p for p in S.all_samples()}\n"
    + "out.update({'v2/' + k: P.upgrade(v) for k, v in R.catalogue()['examples'].items()})\n"
    + "out.update({'v2/library/' + e['id']: P.upgrade(L.draft(e['id'])) for e in L.entries()})\nprint(json.dumps(out))"],
    {cwd: repo, maxBuffer: 1 << 26}).toString());
const ledger = v2['v2-ledger'];

test('version 2: the samples and the 108 programs made version 2 are shown by the workbench, every nested action a sentence', () => {
    assert.equal(Object.keys(v2).length, 110);
    let nested = 0;
    for (const [name, p] of Object.entries(v2)) {
        assert.ok(editable(p), name);
        const ns = namesOf(null, p);
        for (const scope of [null, ...p.phases.map((_, i) => i)]) for (const t of triggersOf(p, scope)) {
            for (const {a, part} of allActions(t.actions)) {
                assert.ok(actionItem(cat, a) || a.do === 'effect', `${name} ${t.id} ${part}: ${a.do} is in the catalogue`);
                const s = plain(actionParts(cat, a, ns));
                assert.ok(s && !s.includes('{') && !s.includes('undefined'), `${name} ${t.id} ${part}: ${s}`);
                if (part.includes('.')) nested++;
            }
            for (const c of allConditions(t)) {
                const s = plain(conditionParts(cat, c, ns));
                assert.ok(s && !s.includes('{') && !s.includes('undefined'), `${name} ${t.id}: ${s}`);
            }
        }
    }
    assert.ok(nested >= 12, String(nested));
    const ns = namesOf(null, ledger);
    const rope = ledger.phases[0].triggers![0];
    assert.equal(plain(actionParts(cat, rope.actions[0], ns)), "Add the event's quantity to the acting wolf's hauled");
    assert.equal(plain(actionParts(cat, rope.actions[1], ns)), "Add the event's quantity to ropes", 'no "whose" before a variable of the story');
    assert.equal(plain(conditionParts(cat, (rope.actions[3] as {if: never}).if, ns)), 'ropes >= 5');
    assert.deepEqual(allActions(rope.actions).map(x => x.part).slice(3, 7), ['actions[3]', 'actions[3].then[0]', 'actions[3].then[1]', 'actions[3].then[2]']);
    assert.ok(allActions(rope.actions).some(x => x.part === 'actions[3].elif[0].then[1]'));
});

test('version 2: nested parts are where Problems point; renames and maps give the program back', () => {
    assert.deepEqual(partOf({kind: 'trigger', part: 'actions[3].elif[0].then[1]'}), {action: 3, path: 'actions[3].elif[0].then[1]'});
    const before = pretty(ledger);
    assert.equal(pretty(renameVariable(renameVariable(ledger, 'signed', 'signers'), 'signers', 'signed')), before);
    const renamed = renameVariable(ledger, 'hauled', 'carried');
    assert.ok(!pretty(renamed).includes('"hauled"'), 'every use of hauled follows');
    assert.equal(usesOf(renamed, {type: 'variable', id: 'carried'}).length, usesOf(ledger, {type: 'variable', id: 'hauled'}).length);
    assert.ok(usesOf(ledger, {type: 'variable', id: 'hauled'}).length >= 3);
    const praise: Ref = {kind: 'trigger', phase: 0, index: 2};
    const r = renameTrigger(ledger, praise, 'cheer');
    assert.ok(pretty(r).includes('"trigger": "cheer"'), 'Run and Enable follow a renamed trigger');
    assert.equal(pretty(renameTrigger(r, praise, 'praise')), before);
    for (const p of Object.values(v2)) for (const t of triggersOf(p, 0)) assert.equal(mapActions(t.actions, a => a), t.actions, 'an identity map keeps the list');
    assert.equal(pretty(renameSignal(renameSignal(v2['v2-flow'], 'late', 'later'), 'later', 'late')), pretty(v2['v2-flow']));
    assert.deepEqual(variablesOf(examples['siege-at-greyfen']).map(v => v.id).sort(), ['defeats', 'guard-posts', 'line-held']);
});

test('version 2: every action and condition the workbench adds is one the story checks accept', () => {
    const kinds = cat.actions.filter(a => a.v2).map(a => a.id);
    const tests = cat.conditions.filter(c => c.v2).map(c => c.id);
    const p = structuredClone(ledger);
    p.phases[1].triggers!.push(...kinds.map((k, i) => ({id: 'new-' + i, on: 'arrival', audience: 'participant',
        actions: [newAction(cat, k, p, 'new-' + i, 1)]})));
    p.phases[1].triggers!.push(...tests.map((k, i) => ({id: 'cond-' + i, on: 'arrival', if: newCondition(k, p, 1), actions: [{do: 'stop'} as const]})));
    const found = JSON.parse(execFileSync('python3', ['-c', "import json, sys\nsys.path.insert(0, 'tools')\nimport story_program as P\n"
        + "print(json.dumps([x for x in P.problems(json.loads(sys.stdin.read())) if x['level'] == 'error']))"], {cwd: repo, input: JSON.stringify(p)}).toString());
    assert.deepEqual(found, []);
});
