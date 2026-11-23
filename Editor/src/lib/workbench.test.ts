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
    countersOf, takenIds, triggersOf, undo, unique, usesOf, withEvent, type Catalogue, type Condition, type Program, type Ref,
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
    assert.equal(Object.keys(v2).length, 115);   // (Doc 86, Phase 1a: the muster at the ford, a third sample; Phase 1b: the pledge at the stone; Phase 2: the beacon road; Phase 3: the oil drive; Phase 4: the lamp line.)
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
    // (Doc 85, Phase 8: the world's conditions need a region and a quest to name; what only the world can name, the author picks.)
    p.regions = [...(p.regions ?? []), {id: 'gate', cell: 'grey', x1: 1, y1: 1, x2: 4, y2: 4}];
    p.quests = [{id: 'q', title: 'Q', objectives: [{id: 'o', label: 'O', on: 'conversation'}]}];
    const picked = (c: Condition) => Object.fromEntries(Object.entries(c).map(([k, v]) => [k, v === '' ? (k === 'other' && c.test === 'in party with' ? '$actor' : 'picked-' + k) : v])) as Condition;
    p.phases[1].triggers!.push(...tests.map((k, i) => ({id: 'cond-' + i, on: 'arrival', if: picked(newCondition(k, p, 1, cat)), actions: [{do: 'stop'} as const]})));
    const found = JSON.parse(execFileSync('python3', ['-c', "import json, sys\nsys.path.insert(0, 'tools')\nimport story_program as P\n"
        + "print(json.dumps([x for x in P.problems(json.loads(sys.stdin.read())) if x['level'] == 'error']))"], {cwd: repo, input: JSON.stringify(p)}).toString());
    assert.deepEqual(found, []);
});

// ------------------------------------------------------------------------------------------------- quests (doc 85, Phase 6)

import {
    editableQuest, fromTemplate, newEffect, newQuest, renameObjective, renameQuest, smallQuestProgram, withQuests, type Quest,
} from './workbench.ts';
import {questCampaignOf, SMALL_QUEST_INPUT} from './fixtures/inputs.ts';

const smallQuest: Program = JSON.parse(readFileSync(join(dirname(fileURLToPath(import.meta.url)), 'fixtures/small-quest.json'), 'utf8'));
const questCampaign: Program = questCampaignOf(smallQuest);

test('the small quest flow makes the program kept beside the tests: open audience, one open phase, the quest, a rehearsal', () => {
    const p = smallQuestProgram(SMALL_QUEST_INPUT);
    assert.equal(pretty(p) + '\n', readFileSync(join(dirname(fileURLToPath(import.meta.url)), 'fixtures/small-quest.json'), 'utf8'));
    assert.equal(p.version, 2);
    assert.deepEqual(p.audience, {kind: 'open'});
    assert.equal(p.phases.length, 1);
    assert.deepEqual(p.phases[0].exits, []);
    const q = p.quests![0];
    assert.equal(q.giver, 'rm111');
    assert.deepEqual(q.objectives.map(o => [o.id, o.after]), [['step-1', undefined], ['step-2', ['step-1']]]);
    assert.deepEqual(q.rewards, [{kind: 'character.grant', player: '$actor', coins: 12, purse: 'town:greyfen', reason: "the carter's thanks"}]);
    const c = p.tests[0];
    assert.deepEqual(c.cohort, []);
    assert.deepEqual(c.events.map(e => e.kind), ['quest accepted', 'conversation', 'gift', 'quest handed in']);
    assert.equal(c.events[2].quantity, 2, 'a quantity goal is one gift of that many');
    assert.deepEqual(c.expectQuests, {[q.id]: {'player-test': 'done'}});
    // (Nothing but thanks: no rewards key; a count goal of 3 is three facts.)
    const plainer = smallQuestProgram({...SMALL_QUEST_INPUT, reward: {kind: 'none'}, objectives: [{label: 'Talk three times.', on: 'conversation', goal: 3}]});
    assert.equal(plainer.quests![0].rewards, undefined);
    assert.equal(plainer.tests[0].events.filter(e => e.kind === 'conversation').length, 3);
});

test('a program with quests is editable, and its quests, objectives and quest effects read as sentences', () => {
    assert.ok(editable(smallQuest) && editable(questCampaign));
    assert.ok(!editable({...questCampaign, quests: [{id: 'q', title: 'Q', objectives: []}]}), 'a quest needs an objective');
    assert.ok(!editableQuest({id: 'q', title: 'Q', objectives: [{id: 'a', label: 'A'}]}), 'an objective needs its event');
    const ns = namesOf({residents: [{id: 'rm111', name: 'Ivo Tarmarsh'}], items: [{id: 'rope', name: 'Rope (coil)', category: 'tool'}],
        towns: [{id: 'greyfen', name: 'Greyfen'}]}, questCampaign);
    assert.equal(ns.quest.thief, 'Who cut the rope?');
    assert.equal(ns.purse['town:greyfen'], "Greyfen's treasury");
    for (const q of questCampaign.quests!) for (const o of q.objectives) {
        const s = plain(eventParts(cat, o, ns));
        assert.ok(s && !s.includes('{'), s);
    }
    assert.match(plain(eventParts(cat, questCampaign.quests![0].objectives[1], ns)), /Ivo Tarmarsh/);
    const lines = questCampaign.triggers![0].actions.map(a => plain(actionParts(cat, a, ns)));
    for (const l of lines) assert.ok(l && !l.includes('{') && !l.includes('undefined'), l);
    assert.equal(lines[0], 'Offer Who cut the rope? to the acting wolf');
    assert.equal(lines[1], 'Show Name the one who cut it. of Who cut the rope? to the acting wolf', 'the objective by its label, within its quest');
    assert.equal(lines[2], 'Add 1 to Bring him two coils of rope. of Rope for the carter for the acting wolf');
    const reward = plain(actionParts(cat, {do: 'effect', effect: questCampaign.quests![0].rewards![0]}, ns));
    assert.equal(reward, "Pay the acting wolf 12 pennies from Greyfen's treasury: “the carter's thanks”");
    // (The optional parts of npc.offer are left out of a new one, and out of its sentence.)
    const offer = newEffect(cat, 'npc.offer', questCampaign);
    assert.ok(!('player' in offer) && !('once' in offer), JSON.stringify(offer));
    assert.deepEqual(newEffect(cat, 'objective.complete', questCampaign), {kind: 'objective.complete', quest: 'rope-for-the-carter', objective: 'step-1', player: '$actor'});
});

test('quests in uses, search and problems; renaming a quest or an objective follows every use, and back gives the program back', () => {
    const uses = usesOf(questCampaign, {type: 'quest', id: 'thief'});
    assert.ok(uses.some(u => u.ref.kind === 'trigger'), 'the trigger that offers it');
    assert.ok(usesOf(questCampaign, {type: 'resident', id: 'rm111'}).some(u => u.ref.kind === 'quest' && u.where.includes('gives it')));
    assert.ok(search(questCampaign, cat, namesOf(null, questCampaign), 'rope-cutter').some(f => f.ref.kind === 'quest'));
    assert.deepEqual(refOfProblem(questCampaign, {kind: 'quest', index: 1}), {kind: 'quest', index: 1});
    assert.deepEqual(refOfProblem(questCampaign, {kind: 'quest', id: 'thief'}), {kind: 'quest', index: 1});
    assert.deepEqual(refOfProblem(questCampaign, {kind: 'quests'}), {kind: 'quests'});
    const renamed = renameQuest(questCampaign, 'rope-for-the-carter', 'rope');
    assert.equal(renamed.quests![0].id, 'rope');
    assert.equal(renamed.triggers![0].match!.detail, 'rope', 'a trigger on its quest completed follows');
    assert.equal((renamed.quests![1].fail![0] as {match: Record<string, string>}).match.detail, 'rope', 'a fail rule on it follows');
    assert.equal((renamed.triggers![0].actions[2] as {effect: {quest: string}}).effect.quest, 'rope');
    assert.deepEqual(Object.keys(renamed.tests[0].expectQuests!), ['rope']);
    assert.ok(renamed.tests[0].events.filter(e => e.kind.startsWith('quest')).every(e => e.detail === 'rope'));
    assert.equal(pretty(renameQuest(renamed, 'rope', 'rope-for-the-carter')), pretty(questCampaign));
    const moved = renameObjective(questCampaign, 'rope-for-the-carter', 'step-1', 'ask-him');
    assert.deepEqual(moved.quests![0].objectives[1].after, ['ask-him']);
    assert.equal(pretty(renameObjective(moved, 'rope-for-the-carter', 'ask-him', 'step-1')), pretty(questCampaign));
    const reveal = renameObjective(questCampaign, 'thief', 'name', 'accused');
    assert.equal((reveal.triggers![0].actions[1] as {effect: {objective: string}}).effect.objective, 'accused');
    assert.equal((reveal.triggers![0].actions[2] as {effect: {objective: string}}).effect.objective, 'step-2', "another quest's objective is left alone");
    // (A new quest takes a free id; removing the last quest leaves no empty list behind.)
    assert.equal(newQuest(questCampaign).id, 'quest-3');
    assert.ok(!('quests' in withQuests(questCampaign, [])));
    assert.ok(signalsOf({...questCampaign, quests: [{...questCampaign.quests![1], objectives: [{id: 's', label: 'S', on: 'story signal', signal: 'clue'}]} as Quest]}).includes('clue'));
});

test("doc 58's templates start a small quest: talking is a conversation with the giver, visiting a place is entering it", () => {
    const templates = JSON.parse(readFileSync(join(repo, 'Data/Storylines/templates.json'), 'utf8')).templates;
    const tie = fromTemplate(templates.find((t: {id: string}) => t.id === 'tie-cart'), 'rm111');
    assert.equal(tie.title, 'A tie: the same cart');
    assert.deepEqual(tie.objectives.map(o => o.on), ['conversation', 'enter place', 'conversation']);
    assert.ok(tie.objectives.every(o => !o.label.includes('{') && !o.label.includes('[[')), JSON.stringify(tie.objectives));
    for (const t of templates) {
        const from = fromTemplate(t, 'rm111');
        const p = smallQuestProgram({...SMALL_QUEST_INPUT, ...from, objectives: from.objectives.length ? from.objectives : SMALL_QUEST_INPUT.objectives});
        assert.ok(editable(p), t.id);
    }
});

// ------------------------------------------------------------------------------------- the world's facts (doc 85, Phase 8)

import {isWorldCondition} from './workbench.ts';

test('Phase 8: the night watch opens in the workbench, every world condition and new event in words', () => {
    const night: Program = JSON.parse(execFileSync('python3', ['-c', "import json, sys\nsys.path.insert(0, 'tools')\nimport story_v2_samples as S\n"
        + 'print(json.dumps(S.FACTS_WATCH))'], {cwd: repo}).toString());
    assert.ok(editable(night));
    const names = namesOf(null, night);
    const seen = new Set<string>();
    for (const c of (night.phases[0].triggers ?? []).flatMap(t => allConditions(t)).flatMap(function flat(c: Condition): Condition[] {
        const k = c as {conditions?: Condition[]; condition?: Condition};
        return [c, ...(k.conditions ?? []).flatMap(flat), ...(k.condition ? flat(k.condition) : [])];
    })) {
        const item = cat.conditions.find(x => x.id === c.test);
        assert.ok(item, c.test);
        if (!isWorldCondition(item)) continue;
        seen.add(c.test);
        const words = plain(conditionParts(cat, c, names));
        assert.ok(!words.includes('{') && words.length > 8, c.test + ': ' + words);
        if (c.test === 'carries' && !('who' in c)) assert.equal(words, 'the acting wolf carries at least 2 rope');
        if (c.test === 'weather is' && !('where' in c)) assert.equal(words, 'the weather where the acting wolf is is snow');
    }
    assert.equal(seen.size, 23, [...seen].join(', '));
    for (const t of night.phases[0].triggers ?? []) {
        const words = plain(eventParts(cat, t, names));
        assert.ok(!words.includes('{'), t.on + ': ' + words);
    }
    assert.equal(cat.events.filter(e => (e as {watch?: boolean}).watch).length, 34);   // (Phase 8's 24, Phase 10's 8, doc 86's moon and month begins.)
});

// ------------------------------------------------------------------------------------------ new world effects (doc 85, Phase 9)

import {itemParts, visitorsOf} from './workbench.ts';

test('Phase 9: every new world effect the workbench adds, its slots picked, is one the story checks accept, and reads as words', () => {
    const fresh = ['story.tell', 'journal.note', 'story.ping', 'story.tracker', 'item.take', 'warrant.issue', 'warrant.clear', 'visitor.add',
        'visitor.leave', 'mantle.assign', 'mantle.release', 'npc.kill', 'npc.hold', 'npc.release', 'npc.follow', 'npc.unfollow', 'door.lock',
        'door.unlock', 'door.open', 'door.close', 'item.place', 'board.post', 'weather.set', 'event.add', 'event.cancel', 'prices.reset',
        'project.complete', 'project.cancel', 'deed.revoke', 'storyline.tick', 'scene.stop'];
    assert.deepEqual(fresh.filter(k => !cat.effects.some(e => e.id === k)), []);
    const p = structuredClone(ledger);
    p.variables = [...(p.variables ?? []), {id: 'ropes-shown', type: 'number', scope: 'story', show: 'Ropes'}];
    p.regions = [...(p.regions ?? []), {id: 'gate', cell: 'grey', x1: 1, y1: 1, x2: 4, y2: 4}];
    const filled = (e: Effect) => Object.fromEntries(Object.entries(e).map(([k, v]) => [k, v === '' ? (k === 'text' || k === 'label' || k === 'reason'
        || k === 'words' || k === 'name' || k === 'detail' ? 'Some words.' : k === 'template' ? 'the-debt' : 'picked-' + k) : v])) as Effect;
    const made = fresh.map(k => filled(newEffect(cat, k, p)));
    const tracked = made.find(e => e.kind === 'story.tracker')?.var;
    assert.ok(p.variables?.find(v => v.id === tracked)?.show, 'a tracker starts on the first shown variable: ' + tracked);
    assert.equal(made.find(e => e.kind === 'story.tell')?.to, 'participants');
    made.forEach((e, i) => p.phases[1].triggers!.push({id: 'fx-' + i, on: 'arrival', audience: 'participant', actions: [{do: 'effect', effect: e}]}));
    const found = JSON.parse(execFileSync('python3', ['-c', "import json, sys\nsys.path.insert(0, 'tools')\nimport story_program as P\n"
        + "print(json.dumps([x for x in P.problems(json.loads(sys.stdin.read())) if x['level'] == 'error']))"], {cwd: repo, input: JSON.stringify(p)}).toString());
    assert.deepEqual(found, []);
    const names = namesOf(null, p);
    for (const e of made) {
        const words = plain(itemParts(cat.effects.find(x => x.id === e.kind)!, e, names));
        assert.ok(!words.includes('{') && words.length > 6, e.kind + ': ' + words);
    }
    assert.equal(plain(itemParts(cat.effects.find(x => x.id === 'story.tell')!, {kind: 'story.tell', to: 'party', player: '$actor', text: 'Stay together.'}, names)),
        'Tell the party of the acting wolf: “Stay together.”');
    assert.deepEqual(visitorsOf(p), ['stranger-1']);
});

// ------------------------------------------------------------- chapters, the events Phase 8 left, the library as quests (doc 85, Phase 10)

import {fromLibraryEntry} from './workbench.ts';

test('Phase 10: story.start as the workbench adds it, its chapter picked, passes the checks and reads as words; the new events read as words', () => {
    const p = structuredClone(ledger);
    const e = newEffect(cat, 'story.start', p);
    assert.deepEqual(e, {kind: 'story.start', story: '', audience: 'same'});
    e.story = 'the-next-chapter';
    p.phases[1].triggers!.push({id: 'next', on: 'arrival', audience: 'participant', actions: [{do: 'effect', effect: e}]});
    const found = JSON.parse(execFileSync('python3', ['-c', "import json, sys\nsys.path.insert(0, 'tools')\nimport story_program as P\n"
        + "print(json.dumps([x for x in P.problems(json.loads(sys.stdin.read())) if x['level'] == 'error']))"], {cwd: repo, input: JSON.stringify(p)}).toString());
    assert.deepEqual(found, []);
    const words = plain(itemParts(cat.effects.find(x => x.id === 'story.start')!, {...e, when: 'now', carry: false},
        namesOf({stories: [{id: 'the-next-chapter', name: 'The crossing'}]}, p)));
    assert.ok(words.startsWith('Start the chapter The crossing for this story') && words.includes('now, beside this one'), words);
    for (const id of ['near resident', 'purse crossed', 'standing crossed', 'relation changed', 'mantle bound', 'mantle released', 'skill practised',
        'injury healed', 'story started', 'story ended']) {
        const said = plain(eventParts(cat, {id: 't', on: id, actions: []}, names));
        assert.ok(!said.includes('{') && said.length > 8, id + ': ' + said);
    }
});

test('Phase 10: each of the 100 library events starts a small quest whose rehearsal passes', () => {
    const entries = JSON.parse(readFileSync(join(repo, 'Data/Storylines/event-library.json'), 'utf8'));
    const list = (Array.isArray(entries) ? entries : entries.events) as Parameters<typeof fromLibraryEntry>[0][];
    assert.equal(list.length, 100);
    const programs = list.map(entry => {
        const from = fromLibraryEntry(entry);
        assert.ok(from.objectives.length >= 1 && from.title, entry.title);
        return smallQuestProgram({title: from.title, summary: from.summary, giver: from.giver || 'rm111', objectives: from.objectives,
            reward: {kind: 'deed', detail: 'Saw it done.'}, offer: {say: 'There is something you could do.'}});
    });
    const failed = JSON.parse(execFileSync('python3', ['-c', "import json, sys\nsys.path.insert(0, 'tools')\nimport story_program as P\n"
        + "ps = json.loads(sys.stdin.read())\nout = []\nfor p in ps:\n    r = P.review(p)\n    if not r['passed']: out.append([p['title'], [t.get('error') for t in r['tests']]])\n"
        + "print(json.dumps(out))"], {cwd: repo, input: JSON.stringify(programs), maxBuffer: 1 << 26}).toString());
    assert.deepEqual(failed, []);
});

// ------------------------------------------------------------------------------- readiness and participation (doc 86, Phase 1a)
import {conditionVars, isStep, operandWords as words2} from './workbench.ts';

test('doc 86: the audience, times and div, the shares, participation and the ready exit read as sentences', () => {
    const muster = v2['v2-muster'];
    assert.ok(editable(muster), 'a case with readiness steps is shown');
    assert.ok(muster.tests[0].events.some(isStep));
    const ns = namesOf(null, muster);
    assert.equal(words2({audience: 'active'}, ns), 'how many wolves are active');
    assert.equal(words2({times: [{audience: 'active'}, 2]}, ns), '(how many wolves are active × 2)');
    assert.equal(words2({div: [{audience: 'all'}, 2]}, ns), "(how many wolves the story has ÷ 2, whole)");
    assert.equal(words2({percent: {list: 'did'}, of: 'active'}, ns), 'the share (%) of the active wolves in did');
    assert.equal(words2({among: {where: {test: 'switch', var: 'cheered', is: true}}, of: 'all'}, ns), "how many of the story's wolves are meeting a test");
    const cheer = muster.phases[0].triggers![1];
    assert.equal(plain(conditionParts(cat, cheer.if!, ns)),
        "at least 15% of the story's wolves (at least 1; no cap) meet the test (the wolf's hauls >= 2)");
    const listed = plain(conditionParts(cat, {test: 'participation', list: 'did', percent: 35, floor: 3, cap: 150}, ns));
    assert.equal(listed, 'at least 35% of the active wolves (at least 3; 150 always enough) are in the list did');
    assert.equal(plain(exitParts(cat, muster.phases[0].exits[0], ns, [])),
        'the phase is ready (its Readiness is met, or an admin presses Advance) → go to ford');
    const made = newCondition('participation', muster, 0, cat);
    assert.deepEqual(made, {test: 'participation', source: 'list', list: 'did', of: 'active', percent: 35, floor: 3});
    assert.ok(conditionVars(made).includes('did'));
    const renamed = renameVariable(muster, 'did', 'brought');
    assert.ok(!pretty(renamed.phases[0].triggers).includes('"did"'), 'a share\'s list follows a renamed variable');
});

test('doc 86: the variables and missions a Readiness reads are its uses, and follow a rename', () => {
    const muster = v2['v2-muster'];
    for (const id of ['did', 'cheered', 'goal', 'rope'])
        assert.ok(usesOf(muster, {type: 'variable', id}).some(u => u.ref.kind === 'phase' && u.where.includes('Readiness')), id);
    assert.ok(usesOf(muster, {type: 'mission', id: 'scout'}).some(u => u.ref.kind === 'phase' && u.where.includes('Readiness')));
    const ready = (p: typeof muster) => pretty(p.phases.map(ph => ph.ready ?? null));
    for (const [from, to] of [['did', 'brought'], ['cheered', 'roared'], ['goal', 'target'], ['rope', 'cord']]) {
        const renamed = renameVariable(muster, from, to);
        assert.ok(!ready(renamed).includes(`"${from}"`) && ready(renamed).includes(`"${to}"`), `Readiness follows ${from} → ${to}`);
        assert.equal(usesOf(renamed, {type: 'variable', id: to}).length, usesOf(muster, {type: 'variable', id: from}).length);
    }
    // (A wolf's own variable read inside a share's test, and a participation's own test, follow it too.)
    const hauls = renameVariable(muster, 'hauls', 'loads');
    assert.ok(!pretty(hauls.phases[0].triggers).includes('"hauls"'));
    assert.equal(renameVariable(muster, 'nothing', 'else').phases[0].ready, muster.phases[0].ready, 'nothing renamed: the same Readiness');
});

test('doc 86, Phase 1b: talk memory, rumours, figures and holds read as sentences; their keys, values and figures offered back', async () => {
    const {effectValuesOf, figuresOf, purseNames, talkKeysOf, talkValuesOf} = await import('./workbench.ts');
    const ns = namesOf({residents: [{id: 'rm111', name: 'Ivo Tarmarsh'}], towns: [{id: 'greyfen', name: 'Greyfen'}],
        tills: [{id: 'till:job:rm111', name: "Ivo Tarmarsh's till"}]});
    const say = (effect: Record<string, unknown>) => plain(actionParts(cat, {do: 'effect', effect: effect as never}, ns));
    const pair = say({kind: 'talk.remember', scope: 'pair', npc: 'rm111', player: '$actor', key: 'nh_stone', value: 'brought', days: 60, keep: true});
    assert.ok(pair.includes('Ivo Tarmarsh') && pair.includes('nh_stone') && pair.includes('brought') && pair.includes('60') && !pair.includes('{'), pair);
    const rumour = say({kind: 'belief.plant', subject: 'rm111', claim: 'took the silver', town: 'greyfen', count: 4, confidence: 70, claimKind: 'story.cleared'});
    assert.ok(rumour.includes('not true') && rumour.includes('took the silver') && !rumour.includes('{'), rumour);
    for (const e of [{kind: 'npc.spare', npc: 'rm111', lasts: 'campaign'}, {kind: 'npc.unspare', npc: 'rm111'},
        {kind: 'figure.set', figure: 'nh_villain', role: 'villain', name: 'The Ledger Keeper', known: 'realm'}, {kind: 'figure.clear', figure: 'nh_villain'},
        {kind: 'npc.protect', npc: 'rm111', lasts: 'run'}, {kind: 'talk.forget', scope: 'realm', key: 'nh_stage'}])
        assert.ok(!say(e).includes('{'), say(e));
    const p = {format: 'ratw-story-program', version: 2, id: 'x', title: 'X', audience: {kind: 'players', players: []}, start: 'a', tests: [],
        phases: [{id: 'a', journal: 'A.', exits: [], effects: [{kind: 'talk.remember', scope: 'realm', key: 'nh_stage', value: 'waking', days: 3},
            {kind: 'figure.set', figure: 'nh_villain', role: 'villain', name: 'N', known: 'realm'}],
            triggers: [{id: 't', on: 'arrival', actions: [{do: 'effect', effect: {kind: 'talk.forget', scope: 'realm', key: 'nh_debts'}}]}]}]} as unknown as Program;
    assert.deepEqual(talkKeysOf(p), ['nh_debts', 'nh_stage']);
    assert.deepEqual(talkValuesOf(p), ['waking']);
    assert.deepEqual(figuresOf(p), ['nh_villain']);
    assert.deepEqual(effectValuesOf(p, ['npc.spare'], 'npc'), []);
    const purses = purseNames({towns: [{id: 'greyfen', name: 'Greyfen'}], tills: [{id: 'till:job:rm111', name: "Ivo Tarmarsh's till"}]});
    assert.deepEqual(purses, {'town:greyfen': "Greyfen's treasury", 'till:job:rm111': "Ivo Tarmarsh's till"}, 'no works, watch or church purse');
});

test('doc 86, Phase 2: values in effects, fills, filters that begin with or are one of, the moon and name tokens read as words', async () => {
    const {fillChips, fillsOf, matchWords, tokenWords, widestFilled, isOperand} = await import('./workbench.ts');
    const ns = namesOf({residents: [{id: 'rm111', name: 'Ivo Tarmarsh'}], towns: [{id: 'greyfen', name: 'Greyfen'}],
        wolves: [{id: 'player-ash', name: 'Ash'}], places: [{id: 'grey', name: 'Greyfen market', kind: 'cell'}]});
    const say = (effect: Record<string, unknown>) => plain(actionParts(cat, {do: 'effect', effect: effect as never}, ns));
    const pay = say({kind: 'character.grant', player: '$actor', purse: 'town:greyfen', reason: 'oil',
        coins: {min: [12, {div: [{purse: 'town:greyfen'}, 20]}]}});
    assert.ok(pay.includes('the smaller of 12 and') && pay.includes('holds') && !pay.includes('{') && !pay.includes('[object'), pay);
    const walk = say({kind: 'npc.walk', npc: {var: 'guide'}, cell: 'grey', x: 3, y: 4, pace: 'walk', cue: 'c'});
    assert.ok(walk.includes('guide') && !walk.includes('[object'), walk);
    const post = say({kind: 'board.post', town: 'greyfen', days: 3, text: '{finder} has {bar} of {bar_goal}; {{not a fill}}, {p_town.of}.'});
    assert.ok(post.includes('⟨finder⟩') && post.includes('⟨p_town of the wolf⟩') && post.includes('{not a fill}'), post);
    assert.equal(fillChips('a {{b}} {c}'), 'a {b} ⟨c⟩');
    assert.deepEqual(fillsOf('{a} {b.of} {{c}}'), [{name: 'a', of: false}, {name: 'b', of: true}]);
    const p = {variables: [{id: 'n', type: 'number', scope: 'story'}, {id: 'w', type: 'wolf', scope: 'story'}, {id: 't', type: 'text', scope: 'story'}]} as unknown as Program;
    assert.equal(widestFilled('x{n}{w}{t}', p), 1 + 11 + 24 + 200);
    assert.equal(matchWords({starts: 'Stood watch'}, ns), 'begins with “Stood watch”');
    assert.equal(matchWords({in: ['rm111', 'rm112']}, ns, 'resident'), 'one of Ivo Tarmarsh, rm112');
    assert.equal(matchWords('rm111', ns), undefined);
    assert.ok(isOperand({purse: 'town:greyfen'}) && !isOperand({starts: 'x'}) && !isOperand({list: 'did'}));
    const deed = plain(eventParts(cat, {on: 'deed', match: {detail: {starts: 'Stood watch at the ford'}} as never}, ns));
    assert.ok(deed.includes('begins with “Stood watch at the ford”') && !deed.includes('[object'), deed);
    const talk = plain(eventParts(cat, {on: 'conversation', match: {target: {in: ['rm111']}} as never}, ns));
    assert.ok(talk.includes('one of Ivo Tarmarsh'), talk);
    const inList = plain(conditionParts(cat, {test: 'field', field: 'cell', op: 'in', value: ['grey', 'far']} as Condition, ns));
    assert.ok(inList.includes('is one of') && inList.includes('“grey”, “far”'), inList);
    const moon = plain(conditionParts(cat, {test: 'calendar is', what: 'moon', value: 'new'} as unknown as Condition, ns));
    assert.ok(moon.includes('the moon') && moon.includes('new'), moon);
    assert.ok(plain(eventParts(cat, {on: 'moon', match: {detail: 'full'}}, ns)).includes('full'));
    assert.equal(tokenWords('{@w:player-ash} met {@r:rm111} at {@p:grey}, {@i:rope}', ns), '«wolf: Ash» met «resident: Ivo Tarmarsh» at «place: Greyfen market», «item: rope»');
    const spawn = say({kind: 'story.area.spawn', region: 'hollow', count: 2, camp: 'sq_beacon_camp'});
    assert.ok(spawn.includes('sq_beacon_camp') && !spawn.includes('{'), spawn);
    const trade = cat.events.find(e => e.id === 'trade done')!;
    assert.ok(trade.fields.cell && !trade.tile, 'trade done has its cell (M5)');
    for (const id of ['camp cleared', 'caravan arrives', 'ship lost', 'ship docks', 'project built', 'festival'])
        assert.ok(cat.events.find(e => e.id === id)?.mission, id + ' is mission evidence (L3)');
    const operands = cat.effects.flatMap(e => (e.params ?? []).filter(x => x.operand).map(x => x.type));
    assert.ok(operands.length > 100 && operands.every(t => ['int', 'resident', 'wolf', 'place', 'item', 'faction', 'town'].includes(t)), String(operands.length));
    assert.ok(!cat.effects.find(e => e.id === 'npc.line')!.params!.some(x => x.operand) && !cat.effects.find(e => e.id === 'story.start')!.params!.some(x => x.operand));
});

import {actionVars, operandVars} from './workbench.ts';

test('doc 86, Phase 3: rank, a tally and a share in words; a rank reads and writes its lists', () => {
    const p = v2['v2-thanks'];
    const ns = namesOf(null, p);
    const ts = triggersOf(p, 0);
    const done = ts.find(t => t.id === 'done')!;
    assert.equal(plain(actionParts(cat, done.actions[0], ns)), 'Put the top 2 wolves of the wolves of the story by p_oil (at least 2) into best');
    const oil = ts.find(t => t.id === 'oil')!;
    assert.match(plain(actionParts(cat, oil.actions[2], ns)), /the p_oil of every wolf of the story whose p_backs is “lamp”, added up/);
    assert.equal(words2({tally: {}}, {}), 'how many wolves of the story');
    assert.equal(words2({tally: {sum: 'p_oil', over: {list: 'best'}}}, {}), 'the p_oil of every wolf in best, added up');
    assert.equal(words2({min: [20, {times: [{share: true}, 2]}]}, {}), "the smaller of 20 and (the wolf's share × 2)");
    assert.deepEqual(actionVars(done.actions[0]), {reads: ['p_oil'], writes: ['best']});
    assert.ok(operandVars({tally: {sum: 'p_oil', where: 'p_backs', is: 'lamp', over: {list: 'best'}}}).sort().join() === 'best,p_backs,p_oil');
    assert.equal(p.quests![0].credit, 'share');
});

import {REQUIRES_TESTS} from './workbench.ts';

test('doc 86, Phase 4: the lamp line in words; a quest\'s "Only for wolves who…" the story checks accept', () => {
    const p = v2['v2-lamp-line'];
    assert.ok(editable(p));
    const ns = namesOf(null, p);
    const ts = triggersOf(p, 0);
    for (const t of ts) assert.ok(!plain(eventParts(cat, t, ns)).includes('{'), t.on);
    assert.equal(plain(eventParts(cat, ts.find(t => t.id === 'news')!, ns)), plain(eventParts(cat, {on: 'story notified', match: {detail: 'voice'}}, ns)));
    const gate = ts.find(t => t.id === 'gate')!;
    const words = allConditions(gate).filter(c => c.test === 'wolf has' || c.test === 'resident has').map(c => plain(conditionParts(cat, c, ns)));
    assert.ok(words.some(w => /Gift's tier is “quickened”/.test(w)) && words.some(w => /post is “reigning at the Great Palace”/.test(w)), words.join(' | '));
    const notify = ts.find(t => t.id === 'lit')!;
    assert.match(plain(actionParts(cat, notify.actions[1], ns)), /^Tell nh-ch1 the news lamp_lit about nobody in particular/);
    assert.match(plain(actionParts(cat, ts.find(t => t.id === 'took-oil')!.actions[0], ns)), /is no longer offered to the acting wolf/);
    const oil = p.quests!.find(q => q.id === 'oil')!;
    assert.equal(oil.closure, 'line');
    assert.equal(oil.offer?.duringAsk, 'About the oil...');
    // (The tests the workbench offers for "Only for wolves who…" are the story checks' own.)
    const python = JSON.parse(execFileSync('python3', ['-c', "import json, sys\nsys.path.insert(0, 'tools')\nimport story_quests as Q\nprint(json.dumps(Q.REQUIRES_TESTS))"],
        {cwd: repo}).toString());
    assert.deepEqual([...REQUIRES_TESTS].sort(), ['all', 'any', 'not', ...python].sort());
});
