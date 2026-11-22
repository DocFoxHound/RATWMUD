// Builds src/render.smoke.tsx for Node (vite, server side, React bundled in) into a temporary folder, renders every
// economy panel and the Prosperity tab, and checks each said what it shows. `npm run smoke`.
import {build} from 'vite';
import {mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join, dirname} from 'node:path';
import {fileURLToPath, pathToFileURL} from 'node:url';
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const out = mkdtempSync(join(tmpdir(), 'ratw-s2stage-render-'));
try {
    await build({root, configFile: join(root, 'vite.config.ts'), logLevel: 'error',
        build: {ssr: 'src/render.smoke.tsx', outDir: out, emptyOutDir: true, rollupOptions: {input: 'src/render.smoke.tsx'}},
        ssr: {noExternal: true}});
    const {renderAll, renderWorkbenches, renderWorkbenchParts} = await import(pathToFileURL(join(out, 'render.smoke.js')).href);
    const html = renderAll();
    // (Doc 85, Phase 4: the workbench over the 8 examples and the 100 library drafts, as tools/story_runner.py serves them.)
    const served = JSON.parse(execFileSync('python3', ['-c', `import json, sys
sys.path.insert(0, 'tools')
import story_library as L, story_runner as R
c = R.catalogue()
drafts = {'library/' + e['id']: L.draft(e['id']) for e in L.entries()}
import story_v2_samples as S
p = c['examples']['reed-ford']
v2 = {'v2/' + x['id']: x for x in S.all_samples()}
v2.update({'v2/example/' + k: R.rules.upgrade(v) for k, v in c['examples'].items()})
v2['v2/night-watch'] = S.FACTS_WATCH
print(json.dumps({'catalog': c, 'drafts': drafts, 'report': R.rules.review(p), 'steps': R.rehearse_steps(p, 0), 'v2': v2}))`], {cwd: join(root, '..'), maxBuffer: 1 << 28}).toString());
    const programs = {...Object.fromEntries(Object.entries(served.catalog.examples).map(([k, v]) => ['example/' + k, v])), ...served.drafts};
    assert.equal(Object.keys(programs).length, 108, 'the 8 examples and the 100 library drafts');
    const benches = renderWorkbenches(served.catalog, programs);
    assert.equal(benches.edits, 0, 'showing a program never edits it');
    for (const [name, text] of Object.entries(benches.html)) {
        assert.ok(text.includes(programs[name].phases[0].id), `${name}: the workbench shows its phases`);
        assert.ok(text.includes('Problems'), `${name}: the workbench shows the Problems list`);
    }
    // (Doc 85, Phase 5: the version 2 samples, with nested If, For each and Random, and the 8 examples made version 2.)
    const second = renderWorkbenches(served.catalog, served.v2);
    assert.equal(second.edits, 0, 'showing a version 2 program never edits it');
    for (const [name, text] of Object.entries(second.html)) {
        assert.ok(text.includes('Problems'), `${name}: the workbench shows the Problems list`);
        assert.ok(text.includes('Variables ·'), `${name}: the Variables panel`);
    }
    console.log(`workbench smoke, version 2: ${Object.keys(served.v2).length} programs, ${second.views} views, none edited`);
    html['Workbench (version 2)'] = second.html['v2/v2-ledger'];
    html['Workbench (world facts)'] = second.html['v2/night-watch'];   // (Doc 85, Phase 8.)
    // (Doc 85, Phase 6: the small quest the flow makes, and a campaign with quests and quest effects: each quest in the Quest editor.)
    const {readFileSync} = await import('node:fs');
    const questPrograms = Object.fromEntries(['small-quest', 'quest-campaign'].map(f => [f, JSON.parse(readFileSync(join(root, 'src/lib/fixtures', f + '.json'), 'utf8'))]));
    const third = renderWorkbenches(served.catalog, questPrograms);
    assert.equal(third.edits, 0, 'showing a program with quests never edits it');
    for (const [name, text] of Object.entries(third.html)) assert.ok(text.includes('Quests ('), `${name}: the tree shows its quests`);
    console.log(`workbench smoke, quests: ${Object.keys(questPrograms).length} programs, ${third.views} views, none edited`);
    Object.assign(html, renderWorkbenchParts(served.catalog, served.catalog.examples['reed-ford'], served.report, served.steps));
    html['Workbench (reed-ford)'] = benches.html['example/reed-ford'];
    html['Workbench (library)'] = benches.html['library/' + Object.keys(served.drafts)[0].slice(8)];
    console.log(`workbench smoke: ${Object.keys(programs).length} programs, ${benches.views} views, none edited`);
    const expect = {
        'Steer': ['-45 (lean times)', 'Set the times'], 'Rules': ['wagons on the roads, 0-300% of normal', 'Harvest bias'],
        'Seasons': ['grain and flour'], 'Imports': ['Cheese', '0 never carried'], 'Status': ['The daily caps'],
        'Market': ['the House wedding'],
        'Imports (before 0061)': ['0061'], 'Status (before 0061)': ['0061'], 'Market (before 0061)': ['0061'],
        'Prosperity': ['Greyfen', '11:30', 'refused by the game', 'the Gull', 'The orchards', 'Trencher bread', "The realm's treasury"],
        'Prosperity (no town purses)': ["No town keeps a purse of its own in this world: the realm's treasury is the only one"],
        // (Doc 78, M5: My world, the owner's with the folder and the stops, a co-builder's without.)
        'My world (owner)': ['7 wolves in the world', 'Last backup 3 h ago', 'Back up now', 'Copy restore steps', 'nearly full',
            'The world folder', 'a cloud service copies', 'The world keeps stopping', '3F2A 9C10', 'Asked for a backup', 'Something went wrong?',
            // (M9: Share my world, ready: the file, its size and where it is.)
            'Share my world', 'Ready: ashfall-2026-11-12.ratwpkg (46 MB)', 'C:\\Users\\Ann\\OneDrive\\ratw-world\\manage\\shared\\ashfall-2026-11-12.ratwpkg'],
        'My world (dm)': ['Your sign-ins', 'are for its owner', 'Only the world&#x27;s owner can do this.', 'Making the file…'],
        // (Doc 85, Phase 1: the Campaigns console, a running and a paused run.)
        'Campaigns': ['Ninety days on the winter ferries', '1 here now', '1 needs you', 'watch-round day 17.0 (in 4.1 days), every 7 days',
            'offer-a-coil · Birch Oakenfall', 'Paused: Action 5514 refused', 'Ended 1', 'runner ● alive 1 s ago · 1 event behind', 'Hold all…'],
        'Campaigns (running)': ['Waiting for', 'enrol', '[ferry-watch]', 'next: deep-freeze', 'offer-a-coil', 'Birch Oakenfall', 'Reed Ford (31, 36)',
            'offer-a-coil: when gift (item rope, target Ivo Tarmarsh)', 'Stop…',
            // (Phase 2: the steering row, the heartbeat, a command and a firing's values in the timeline.)
            '⏸ Hold', 'Send signal', 'Go to phase…', 'Tell the wolves…', 'set stores to 5', 'two coils were handed over',
            'stores 1 → 3'],
        'Campaigns (paused)': ['Retry', 'Skip it and carry on…', 'deed.award → Ash Greymantle', 'REFUSED', 'waiting behind #5514', 'story.practice → Ash Greymantle'],
        'Campaigns (triggers)': ['offer-a-coil', 'choose a wolf…', 'Fire…', 'log-watch-round', '>off<'],
        'Campaigns (audit)': ['Before → after', 'value 3 → 5', 'two coils were handed over before the story saw them', 'stores set 3 → 5.'],
        'Campaigns (held)': ['▶ Resume', 'Resume from now', 'Held by martin', '4 world events wait'],
        'Campaigns (hosted dm)': ['needs its owner, or the owner&#x27;s'],
        'Campaigns (viewer)': ['it needs a Dungeon Master account'],
        // (Doc 85, Phase 3: "Waiting for" explained, the stuck alert, a dry run's "this will do", the Problems list, the tab bar.)
        'Campaigns (explained)': ['offer-a-coil', 'would fire for: Ash Greymantle (here now) and 2 away', 'stores ≥ 5 ✘ (stores is 2)',
            'timer spring-council sends it (day 90.0)', 'then enters ferries-reopen', '1 turned off or used up', 'Why not?', 'Next by itself',
            'watch-round day 17.0 (in 4.1 days) · ≈ 2.1 h · every 7 days', 'What if…', 'What would happen?', 'Fire…'],
        // (Doc 85, Phase 8: world conditions judged with the facts, and the world as the game last told the story.)
        'Campaigns (world facts)': ['The world, as the game last told the story', 'Listening for: region occupied, trade done',
            'Marketday · winter · 14:05 · day', 'out of doors', 'in a party of 2', '40p', 'the world says 0 for Ash Greymantle',
            'the world says 14', 'do not say'],
        'Workbench (world facts)': ['carries at least 2', 'stands in'],
        'Campaigns (stuck)': ['Cannot move on from ferry-watch', '1 used up, 1 waiting for wolves gone 14 days, 2 a signal nothing sends',
            'Timer watch-round is overdue by 10 min', 'nearly full: 460 of 500'],
        'Campaigns (dry run)': ['This will do', 'Entered deep-freeze (moved on), with its entry effects.', 'sends story.phase to the game now',
            'it is then in deep-freeze (visit 3)', 'then it sends, one by one: band.override → Ivo Tarmarsh', 'A dry run on the story as it stands now'],
        'Campaigns (refused dry run)': ['The runner would refuse it now: Its conditions are false just now.'],
        // (Doc 85, Phase 5b: a new version approved for a running campaign, and moving the run onto it.)
        'Campaigns (new version)': ['⇪ Move onto a new version…'],
        'Campaigns (move)': ['Version b7e1c2d4', 'approved by ann', 'version 1 → 2 of the format', 'What changed', 'Triggers: 1 added (early-thaw)',
            'offer-a-coil: words only', 'Variables: 1 retyped (watches: number → switch)', 'Timers: watch-round started differently; first-freeze never started now',
            'Dropped (the new version has no place for it) · 1', 'timer first-freeze, due day 38.0: nothing in the new version listens for its signal.',
            'Reset (starts again) · 1', 'mission ferry-line', 'hauled.player (Birch Oakenfall) = 2: renamed from hauled', 'Move onto this version…'],
        'Campaigns (move, phase gone)': ['Phase ferry-watch is not in the new version. Land in:', 'the-watch', 'name the phase to land in'],
        'Campaigns (moved)': ['Versions', 'b7e1c2d4', 'the version it is on', 'a41c9e00', '>left<', 'moved it onto approved version #2',
            'version a41c9e00 → b7e1c2d4', 'the freeze came early this year'],
        'Campaigns (moved, timeline)': ['moved to version b7e1c2d4 by martin (from a41c9e00): 4 kept, 1 reset, 1 back to their first value, 1 dropped',
            'the game refused version c9d0e1f2: back on b7e1c2d4', 'does not fit its live cell'],
        'Campaigns (moving)': ['Moving onto a new version: the game is being told its words for this visit'],
        'Campaigns (move dry run)': ['This will do', 'Moving onto version b7e1c2d4', 'sends story.phase to the game now',
            'Dropped (the new version has no place for it): timer first-freeze', 'Reset (starts again): mission ferry-line',
            'Version 1 did not count firings'],
        'Story problems': ['2 errors', '3 warnings', 'Phase ferry-watch › trigger begin-deep-freeze › action 2', 'Phase ferries-reopen',
            'Campaign triggers › trigger offer-a-coil › action 2', 'from the world: approval refuses this', 'No rehearsal case reaches this trigger.', 'Go'],
        'DM tab bar': ['⚑ Campaigns', '⚠ 2', 'The story runner has not beaten for 95 s: 2 stories are not moving.', 'Open Campaigns'],
        // (Doc 85, Phase 4: the workbench, its pickers, the rehearsal strip stepping.)
        'Workbench (version 2)': ['Variables ·', 'Enabled at the start', 'Else if', 'the acting wolf&#x27;s', '&gt;=',
            '+ New variable', 'Ropes delivered'],
        'Workbench (reed-ford)': ['Campaign (always on)', 'Phase: ', 'Exits', 'Events', 'Conditions', 'Actions', 'Library…', 'Signals', 'Counters'],
        'Workbench pickers': ['Choose an event', 'A gift is given', 'Gifts, items and loans', 'Add an action', 'Award a deed', 'Ivo Tarmarsh', 'Rope (coil)', 'Any (no filter)', 'Reed Ford'],
        'Workbench rehearsal': ['Rehearse ▶', 'every case passes', 'Step through', 'step 2 of', 'Waiting for', 'Counters'],
        // (Doc 85, Phase 6: quests in the workbench, the small quest flow, the console's Quests tab and the living audience.)
        'Quest editor': ['Quests (2)', 'Rope for the carter', 'Who gives it', 'Ivo Tarmarsh', 'The resident offers it', 'Could you find me some?', 'handed in to the resident',
            'Who may take it', 'any wolf who asks', 'What to do', 'Ask the carter what he needs.', 'After:', 'Rewards', 'Pay the acting wolf 12 pennies from greyfen', 'Bonus',
            'after a week'],
        'Quest editor (party, hidden, fail rules)': ['one copy for the wolf&#x27;s party', 'hidden until revealed or done', 'optional', 'What fails it',
            'When it has run for', 'The purse must be a town, a till or a Chapter.', 'given by a trigger'],
        'Quest effects': ['Offer Who cut the rope? to the acting wolf', 'Show Name the one who cut it. of Who cut the rope? to the acting wolf',
            'Ivo Tarmarsh offers the choice accuse', 'Take the acting wolf into the story'],
        'Quest pickers': ['Name the one who cut it.', 'greyfen&#x27;s treasury', 'greyfen&#x27;s Town Works', 'Add a reward', 'Pay a wolf from a purse', 'Give a wolf an item'],
        'New small quest': ['A small quest', 'Start from a personal-quest template', 'A tie: the same cart', 'Who gives it', 'What to do', 'The reward', 'coins, from a purse',
            'Make the small quest', 'Choose who gives it.'],
        'Campaigns (quests)': ['Quests', 'Rope for the carter', 'each wolf', 'from Ivo Tarmarsh', 'READY', '1 / 2', '✖', 'Give the quest…', 'Elm Sandpaw',
            '0 of 2 done · 1 ready to hand in · 1 under way · at most 10 under way at once', 'Who cut the rope?', 'Party of Ash Greymantle, Birch Oakenfall', 'left: Elm Sandpaw'],
        'Campaigns (wolves, parties)': ['Audience: open · 3 of 5000', 'Add a wolf…', 'Remove…', 'party with Birch Oakenfall'],
        'Campaigns (open audience)': ['open · 3 of 5000'],
        'My world (share failed)': ['the disk that holds the world folder is nearly full (300 MB free; making the file needs about 1.3 GB)'],
    };
    const absent = {'My world (dm)': ['The world folder', 'OneDrive', 'exit 139'], 'Campaigns (viewer)': ['Ninety days'],
        'Campaigns (hosted dm)': ['⏸ Hold', 'Go to phase…'], 'Campaigns (held)': ['⏸ Hold'], 'Campaigns (dry run)': ['what the program says'],
        'Campaigns (running)': ['Move onto a new version'], 'Campaigns (move)': ['Land in:'],
        'Quest pickers': ['Start a quest', 'Take a wolf into the story'], 'Quest editor': ['Quests need a version 2 story']};
    if (process.env.RENDER_DUMP) for (const [name, text] of Object.entries(html)) console.log(`== ${name}\n${text.replace(/<[^>]+>/g, " ").replace(/\s+/g, " ").slice(0, 700)}`);
    for (const [name, text] of Object.entries(html)) {
        assert.ok(text.length > 100, `${name}: rendered`);
        const plain = text.replace(/<!--.*?-->/g, '').replace(/<[^>]+>/g, ' ').replace(/&#x27;/g, "'").replace(/\s+/g, ' ');
        for (const words of expect[name] ?? [])
            assert.ok(plain.includes(words) || text.includes(words), `${name}: shows "${words}"`);
        for (const words of absent[name] ?? [])
            assert.ok(!plain.includes(words), `${name}: doesn't show "${words}"`);
    }
    console.log(`render smoke: ${Object.keys(html).length} panels rendered: ${Object.keys(html).join(', ')}`);
} finally {
    rmSync(out, {recursive: true, force: true});
}
