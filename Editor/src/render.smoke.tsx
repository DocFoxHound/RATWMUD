// The render smoke (doc 79, Phase 9's first repair): every economy panel of the studio and the DM tool, and the
// Prosperity tab, rendered whole with react-dom/server from views shaped as the tools give them, so a panel that
// throws while rendering (a missing field, a bad map over a list) fails a test, not a host's page. Built for Node by
// scripts/render-smoke.mjs (vite, server side) and run by `npm run smoke`; no browser, no server.
import {renderToString} from 'react-dom/server';
import type {EconomyData} from './lib/economy';
import type {Me, ProsperityView} from './dm/api';
import {SteerPanel} from './studio/SteerPanel';
import {RulesPanel} from './studio/RulesPanel';
import {SeasonsPanel} from './studio/SeasonsPanel';
import {ImportsPanel} from './studio/ImportsPanel';
import {StatusPanel} from './studio/StatusPanel';
import {MarketPanel} from './studio/MarketPanel';
import {ProsperityTab} from './dm/ProsperityTab';
import {MyWorldTab} from './dm/MyWorldTab';
import type {MyWorld} from './dm/api';
import {CampaignsTab, type CampaignsInitial} from './dm/CampaignsTab';
import {GuideTab} from './dm/GuideTab';
import {WorldVarsPanel} from './dm/campaigns/WorldVars';
import type {Campaign} from './lib/campaigns';
import type {Explained, MigrationReport, Order, Preview, Problems, RunDetail, RunRow, TimelineItem, Version} from './lib/campaigns';
import {AskDialog, MovePanel} from './dm/campaigns/Steer';
import {ProblemsList} from './dm/StoryProgramPanel';
import {Workbench, type FullCatalogue} from './dm/triggers/Workbench';
import {RehearsalStrip, type Report, type Steps} from './dm/triggers/Rehearsal';
import {BenchContext} from './dm/triggers/context';
import {EventPicker, ActionPicker, Picker} from './dm/triggers/pickers';
import {buildTree, eventParts, namesOf, pretty, wordingOf, type Program, type Ref, type Refs} from './lib/workbench';
import {WordingContext} from './dm/campaigns/Explain';
import {DmSuite} from './dm/DmApp';
import {NewStory} from './dm/StoryProgramPanel';
import type {QuestBoard} from './lib/campaigns';
import smallQuestText from './lib/fixtures/small-quest.json?raw';
import questCampaignText from './lib/fixtures/quest-campaign.json?raw';

const none = () => {};
const view = (slim: boolean): EconomyData => ({
    supported: true, slim, problem: '', note: 'The economy\'s settings.', takesEffect: 'At once.', worldInfo: null,
    economy: {treasury: 1000, storeHerbs: 100, storeMeals: 50, needsPaused: false,
        seasons: {grain: {price: [120, 100, 80, 130], shelf: [100, 100, 100, 100]}}, rules: {playerSellMost: 300, ripple: 40},
        services: {groom_and_trim: {on: false, price: 2, every: 10}},
        overrides: slim ? {prosperityCause: 'the long frost', neverImported: [], status: {leanDays: 4, caps: {theftADay: 10}},
            market: {events: {perWeek: 1}, wharf: {ships: ['the Gull']}}} : {}},
    prosperity: {value: -45, cause: 'the long frost', low: -100, high: 100, step: 5},
    ruleList: [{name: 'playerSellMost', low: 0, high: 5000, fallback: 400, switch: false},
        {name: 'ripple', low: 0, high: 70, fallback: 50, switch: false},
        {name: 'neverBroke', low: 0, high: 1, fallback: 0, switch: true},
        {name: 'caravanRate', low: 0, high: 300, fallback: 100, switch: false},
        {name: 'harvestBias', low: -20, high: 20, fallback: 0, switch: false}],
    serviceList: [{id: 'groom_and_trim', name: 'A groom and trim', business: 'barber', price: 1, every: 4}],
    seasonGroups: [{id: 'grain', name: 'grain and flour', goods: ['flour', 'bread'], price: [110, 95, 85, 115], shelf: [100, 100, 100, 100]}],
    goods: [{id: 'cheese', name: 'Cheese'}, {id: 'salt', name: 'Salt'}],
    defaults: {status: {shares: [{at: -100, shares: [18, 45, 32, 5]}, {at: 0, shares: [5, 25, 60, 10]}, {at: 100, shares: [2, 12, 70, 16]}],
        caps: {rewardsADay: 200, theftADay: 20, welcomeDays: 7}, leanDays: 3, pay: {keeper: 14}, street: {}},
        market: {events: {perWeek: 2, list: [{id: 'house_wedding', words: 'the House wedding', needs: ['city']},
            {id: 'good_catch', words: 'a good catch', needs: []}], causes: {}},
            wharf: {ships: ['the Gull', 'the Heron'], never: ['cheese']}}},
} as unknown as EconomyData);

const prosperity: ProsperityView = {
    target: 'dev', writtenAt: new Date().toISOString(), gameDay: 40.4, stale: false,
    report: {v: 1, day: 40, at: 40.4,
        prosperity: {slider: -10, target: -20, cause: 'the long frost', by: 'dm', neverHungry: false, neverBroke: false},
        markets: [{id: 'grey', name: 'Greyfen', times: {word: 'lean', score: -0.4, cause: 'the long frost'},
            households: {destitute: 1, poor: 3, comfortable: 6, rich: 1}, overridden: 1, hungry: 2, short: 1, thieves: 0.6,
            beggars: 1, dole: 2, nudge: {points: -30, until: 45, words: 'the frost'}, resetting: {until: 41}}],
        pressures: [{market: 'grey', good: 'bread', P: 0.5, cause: 'the long frost'}],
        areas: [{id: 'orchards', name: 'The orchards', harvest: 'poor', override: {word: 'poor', until: 60, words: 'blight'}},
            {id: 'fields', name: 'The fields', harvest: 'fair', override: null}],
        eventKinds: [{id: 'house_wedding', words: 'the House wedding', needs: []}], weekDays: [40, 41, 42, 43, 44, 45, 46, 47, 48],
        nextWeek: 42, bandOverrides: [{household: 'h:cottage', band: 'poor', until: 52, members: ['npc_x']}]},
    towns: [{community: 'greyfen', treasury: 900}], treasury: 5, goodNames: {bread: 'Trencher bread'},
    week: {wagons: [{id: 'c9', from: 'grey', to: 'port', day: 40, state: 'travelling', goods: ['salt']}],
        ships: [{id: 's3', port: 'grey', name: 'the Gull', day: 41, state: 'due', goods: ['salt']}],
        events: [{id: 'e4', market: 'grey', kind: 'house_wedding', day: 42, hour: 11.5, state: 'due', by: 'dm', words: 'the wedding'}]},
    players: {days: [{day: 40, made: 25, sunk: 4, goodsMade: 1, goodsSunk: 0}],
        holders: [{holder: 'acct-ada', name: 'Ada', made: 25, sunk: 4, byReason: {'dm grant: for the bell': [25, 0, 0, 0], 'provisions to start with': [0, 0, 12, 0]}}]},
    thefts: 1, word: [{kind: 'slider', text: 'the long frost'}],
    log: [{at: new Date().toISOString(), kind: 'character.grant', state: 'refused', text: 'dm-user: character.grant Ada: refused by the game (Over the cap)'},
        {at: 'game day 39', kind: 'ripple', text: 'bread at Greyfen dearer by 0.35'}],
};

const now = Math.floor(Date.now() / 1000);
const myWorld = (owner: boolean): MyWorld => ({
    world: 'world', name: 'Ashfall', package: '0.4.2', owner, fingerprint: '3F2A 9C10 5B7E 0D41 A6C3 22F9 8E05 77D1', wolves: 7,
    status: {key: 'world', name: 'Ashfall', kind: 'live', world: null, server: {state: 'running', by: '', at: '', note: ''}, serverRunning: true,
        release: 0, live: 'world', warning: '', hosted: true, hostServer: {up: true, reason: 'running', code: null, failures: 0, retryIn: null}},
    settings: {player_limit: 40, workers: null, cheats: 'off', quickened_wolves: true, editor: {enabled: true}},
    ai: {available: false, reason: 'Needs an AI key in world.toml.', today: {story: 0, roster: 0}, caps: {story: 100, roster: 20}, month: null, costPerStory: null, model: ''},
    history: [{at: now - 60, action: 'server.backup', target: 'world', detail: 'Asked for a backup.', who: 'host-k7q2#3f2a1c'}],
    signIns: [{at: now - 120, account: 'host-k7q2', detail: owner ? 'Signed in from this computer.' : 'Signed in.'}],
    backups: {at: now, last: {name: 'ratw-20261112T140300Z-by-hand', kind: 'by-hand', at: now - 3 * 3600, size: 820000, package: '0.4.2', migration: '0064', pinnedUntil: null},
        backups: [{name: 'ratw-20261112T140300Z-by-hand', kind: 'by-hand', at: now - 3 * 3600, size: 820000, package: '0.4.2', migration: '0064', pinnedUntil: null},
            {name: 'ratw-20261111T090000Z-daily', kind: 'daily', at: now - 30 * 3600, size: 810000, package: '0.4.2', migration: '0064', pinnedUntil: now + 80 * 86400}],
        pins: 1, pinCap: 5, working: null, problem: {code: 'disk', at: now, free: 1024 ** 3}, disk: {free: 1024 ** 3, used: 90 * 1024 ** 2, minFree: 2 * 1024 ** 3}},
    share: owner ? {state: 'ready', at: now - 120, started: now - 200, file: 'ashfall-2026-11-12.ratwpkg', size: 46 * 1024 ** 2, problem: null,
        files: [{name: 'ashfall-2026-11-12.ratwpkg', size: 46 * 1024 ** 2, at: now - 120}], kept: 3, thisHour: 1, perHour: 3, waiting: false, snapshot: true}
        : {state: 'working', at: now, started: now, file: null, size: null, problem: null, files: [], kept: 3, thisHour: 1, perHour: 3, waiting: false, snapshot: false},
    ...(owner ? {folder: 'C:\\Users\\Ann\\OneDrive\\ratw-world', exits: [{at: now - 600, code: 139, lines: ['the world server stopped']}]} : {}),
} as unknown as MyWorld);


// The Campaigns console (doc 85, Phase 1): a running and a paused run as tools/story_runner.py's status and timeline
// give them.
const at = '2026-11-21T10:00:00+00:00', earlier = '2026-11-21T07:00:00+00:00';
const refused: Order = {id: 5514, kind: 'deed.award', target: 'player-ash', status: 'refused', result: 'that wolf is not in the world just now',
    by: 'martin', at: earlier, doneAt: earlier, age: 10800};
const runningRow: RunRow = {run: 'world-9f2c', title: 'Ninety days on the winter ferries', status: 'running', digest: 'a41c9e00', approver: 'martin',
    approvedAt: earlier, phase: 'ferry-watch', problem: '', visit: 2, entered: 10, cohort: ['player-ash', 'player-birch', 'player-elm'], wolves: 3,
    missions: [], behind: {events: 0, seconds: 0}, alerts: [], ending: false,
    last: {type: 'trigger', trigger: 'offer-a-coil', actor: 'player-birch', day: 12.5, at, phase: 'ferry-watch'},
    waiting: [{on: 'timer', name: 'watch-round', signal: 'watch-round', at: 17, clock: 'game', actor: 'player-ash', repeat: 7, scope: 'campaign'},
        {on: 'timer', name: 'first-freeze', signal: 'first-freeze', at: 38, clock: 'game', actor: 'player-ash', repeat: 0, scope: 'campaign'},
        {on: 'wolves', events: ['gift'], what: [{source: 'trigger', id: 'offer-a-coil', on: 'gift', match: {item: 'rope', target: 'rm111'}}]}]};
const pausedRow: RunRow = {run: 'world-77ab', title: 'Two ways across Reed Ford', status: 'paused', digest: 'c0ffee11', approver: 'martin', approvedAt: earlier,
    phase: 'crossing', problem: 'Action 5514 refused: that wolf is not in the world just now', visit: 3, entered: 11, cohort: ['player-ash', 'player-birch'],
    wolves: 2, missions: [{id: 'passengers', title: 'Account for the passengers', mode: 'individual', completed: 1, required: 2, ready: false, optional: false}],
    behind: {events: 0, seconds: 0}, last: {type: 'phase', phase: 'crossing', day: 11, at: earlier, visit: 3, cause: 'event 4102'}, ending: false,
    alerts: [{kind: 'paused', needsYou: true, problem: 'Action 5514 refused: that wolf is not in the world just now'}],
    waiting: [{on: 'admin', problem: 'Action 5514 refused', order: refused}, {on: 'send', effects: [{kind: 'story.practice', player: 'player-ash', skill: 'travel'}]}]};
const names = {'player-ash': 'Ash Greymantle', 'player-birch': 'Birch Oakenfall', 'player-elm': 'Elm Sandpaw', rm111: 'Ivo Tarmarsh', ford: 'Reed Ford'};
const campaignList: CampaignsInitial = {overview: {runs: [runningRow, pausedRow, {run: 'world-old', title: 'The hidden pup', status: 'complete', digest: 'beef',
    approver: 'martin', approvedAt: earlier, phase: null, problem: '', visit: 4, wolves: 9}], now: {day: 12.9, newest: 48211, at}, names,
    runner: {beatAt: at, age: 1.2, behind: 1}},
    live: {target: 'dev', frame: {day: 12.9, people: [['player-ash', 'Ash Greymantle', 'p', 'ford', 31, 36, 0, '', 'walking'],
        ['player-birch', 'Birch Oakenfall', 'o', 'ford', 3, 4, 0, '', '']], shops: []}, age: 2, events: [], actions: []}};
const detailOf = (row: RunRow, extra: Partial<RunDetail>): RunDetail => ({...row, state: {phase: row.phase!, entered: row.entered!, cohort: row.cohort!, votes: {}, visits: row.visit!, actor: '',
    counters: {story: {stores: 5, watches: 1}, player: {'player-birch': {hauled: 2}}},
    timers: {'watch-round': {signal: 'watch-round', at: 17, clock: 'game', actor: 'player-ash', interval: 7, scope: 'campaign'}}},
    program: {id: 'winter-ferries', title: row.title}, approvedBy: 'martin', cursor: 48211, now: {day: 12.9, newest: 48211, at}, guestVoices: [], directions: [], signals: [],
    history: [{visit: 2, phase: row.phase!, day: 10, cause: 'event 47002', receipts: [5512]}, {visit: 1, phase: 'enrol', day: 10, cause: 'published', receipts: [5510]}],
    trace: [], actions: [], orders: [], missionsNow: [], directed: ['rm111'], regions: [{id: 'landing', cell: 'ford', x1: 29, y1: 34, x2: 34, y2: 38}],
    phases: [{id: 'enrol', to: ['ferry-watch'], missions: 0}, {id: row.phase!, to: ['deep-freeze'], missions: 0}], start: 'enrol', listening: [], names,
    // (Doc 85, Phase 2: what the controls may name, the runner's heartbeat and the run's commands.)
    runner: {beatAt: at, age: 1.2, version: 'doc85-phase2', behind: 0},
    controls: {triggers: [{id: 'offer-a-coil', on: 'gift', key: 'offer-a-coil', off: false, scope: 'campaign', once: 'each', needsWolf: true, hasConditions: false, actions: ['add']},
        {id: 'log-watch-round', on: 'story signal', signal: 'watch-round', key: 'log-watch-round', off: true, scope: 'campaign', once: 'each', needsWolf: false,
            hasConditions: false, actions: ['add']}], offElsewhere: [], phases: ['enrol', row.phase!, 'deep-freeze'], signals: ['first-freeze', 'watch-round'],
        counters: [{key: 'stores', scope: 'story'}, {key: 'watches', scope: 'story'}, {key: 'hauled', scope: 'player'}],
        schedules: {'watch-round': {days: 7, repeat: true, scope: 'campaign'}}, missions: [], scenes: [], regions: ['landing']},
    commands: [{id: 12, key: 'cabc12345678', visit: row.visit!, digest: row.digest, kind: 'set', payload: {key: 'stores', scope: 'story', value: 5},
        reason: 'two coils were handed over before the story saw them', by: 'martin#a1', at, status: 'done', doneAt: at,
        result: {text: 'stores set 3 → 5.', before: {value: 3}, after: {value: 5}}}],
    ...extra});
const campaignRunning: CampaignsInitial = {...campaignList, tab: 'overview', detail: detailOf(runningRow, {}), timeline: {run: runningRow.run, names, events: {
    '47002': {id: 47002, kind: 'gift', actor: 'player-birch', target: 'rm111', cell: 'ford', item: 'rope', quantity: 2, coins: 0, detail: '', day: 12.5, x: 31, y: 36, at}},
    before: null, since: {visit: 2, trigger: 9, order: 5513, at}, now: at, items: [
        {key: 'c:12', type: 'command', id: 12, command: 'set', visit: 2, digest: 'a41c9e00', payload: {key: 'stores', scope: 'story', value: 5},
            reason: 'two coils were handed over before the story saw them', by: 'martin#a1', status: 'done', doneAt: at, askedAt: at, at,
            result: {text: 'stores set 3 → 5.', before: {value: 3}, after: {value: 5}}},
        {key: 't:9', type: 'trigger', id: 9, visit: 2, phase: 'ferry-watch', trigger: 'offer-a-coil', actor: 'player-birch', day: 12.5, actions: ['add'], event: 47002, on: 'gift', cell: 'ford', x: 31, y: 36, at,
            changes: [{what: 'counter', scope: 'story', key: 'stores', before: 1, after: 3}]},
        {key: 'o:5513', type: 'order', id: 5513, kind: 'story.phase', target: runningRow.run, status: 'applied', result: '', by: 'martin', at: earlier, doneAt: earlier, visit: 2},
        {key: 'v:2', type: 'phase', visit: 2, phase: 'ferry-watch', day: 10, cause: 'event 47001', receipts: [5513], at: earlier}]}};
// (Doc 85, Phase 5b: a new version approved for the running campaign, its report, the move on the timeline and in Audit.)
const migration: MigrationReport = {blocked: null, warnings: ['Version 1 did not count firings: "fired" conditions count from now.'],
    phase: {from: 'ferry-watch', to: 'ferry-watch', choices: ['enrol', 'ferry-watch', 'deep-freeze'], gone: false},
    from: {digest: 'a41c9e00', version: 1, title: 'Ninety days on the winter ferries'}, to: {digest: 'b7e1c2d4', version: 2, title: 'Ninety days on the winter ferries'},
    kept: [{what: 'phase', key: 'ferry-watch', why: 'Visit 2 of ferry-watch carries on (its entry effects are not sent again).'},
        {what: 'variable', key: 'stores', scope: 'story', value: 5}, {what: 'variable', key: 'hauled.player', scope: 'player', wolf: 'player-birch', value: 2, renamed: 'hauled',
            why: 'renamed from hauled (a version 1 counter named in both scopes).'}, {what: 'timer', key: 'watch-round', at: 17}],
    reset: [{what: 'mission', key: 'ferry-line', why: 'its objectives (or who plays it) changed: its progress starts again.'}],
    defaulted: [{what: 'variable', key: 'watches', scope: 'story', value: 1, why: 'it was a number, it is a switch now: it starts at its first value.'}],
    dropped: [{what: 'timer', key: 'first-freeze', at: 38, why: 'nothing in the new version listens for its signal.'}],
    added: [{what: 'off', key: 'ferry-watch/begin-deep-freeze', why: 'the new version starts it turned off.'}],
    diff: {same: false, header: ['version'], tests: true, triggers: {added: ['early-thaw'], removed: [], newlyOff: ['ferry-watch/begin-deep-freeze'],
        changed: [{id: 'offer-a-coil', what: ['note'], wordsOnly: true}, {id: 'open-winter-ledger', what: ['actions']}]},
        variables: {added: [], removed: [], retyped: [{id: 'watches', scope: 'story', from: 'number', to: 'switch'}], rescoped: [], changed: []},
        phases: {added: [], removed: [], changed: [{id: 'ferry-watch', what: ['journal', 'triggers']}]}, missions: {added: [], removed: [], changed: []},
        timers: {added: [], removed: ['first-freeze'], changed: ['watch-round']}, regions: {added: [], removed: [], changed: []}, scenes: {added: [], removed: [], changed: []}}};
const approvedVersion: Version = {id: 3, digest: 'b7e1c2d4aa', approvedBy: 'ann#c2', approvedAt: at, status: 'approved', from: null, movedAt: null, movedBy: null,
    command: null, visit: null, day: null, report: migration, order: null, settledAt: null, problem: '', title: runningRow.title};
const versions: Version[] = [
    {...approvedVersion, id: 2, digest: 'b7e1c2d4aa', status: 'current', from: 'a41c9e00', movedAt: at, movedBy: 'martin#a1', visit: 2, day: 12.6, order: 5530, settledAt: at},
    {...approvedVersion, id: 1, digest: 'a41c9e00', approvedBy: 'martin', approvedAt: earlier, status: 'past', movedAt: earlier, visit: 1, report: null}];
const movedItem: TimelineItem = {key: 'm:2', type: 'version', id: 2, digest: 'b7e1c2d4aa', from: 'a41c9e00', status: 'current', approvedBy: 'ann#c2', approvedAt: at,
    movedAt: at, movedBy: 'martin#a1', command: 14, visit: 2, day: 12.6, order: 5530, settledAt: at, problem: '', at, phase: 'ferry-watch', phaseFrom: 'ferry-watch',
    warnings: [], counts: {kept: 4, reset: 1, defaulted: 1, dropped: 1, added: 1}};
const campaignMoved: CampaignsInitial = {...campaignRunning, tab: 'audit', detail: {...campaignRunning.detail!, digest: 'b7e1c2d4aa', versions,
    commands: [{id: 14, key: 'cabc12345679', visit: 2, digest: 'a41c9e00', kind: 'migrate', payload: {version: 2}, reason: 'the freeze came early this year',
        by: 'martin#a1', at, status: 'done', doneAt: at, result: {text: 'Moving onto version b7e1c2d4 (from a41c9e00), in ferry-watch, visit 2: 4 kept, 1 reset, 1 defaulted, 1 dropped.',
            before: {version: 'a41c9e00', phase: 'ferry-watch'}, after: {version: 'b7e1c2d4aa', phase: 'ferry-watch'}}}, ...campaignRunning.detail!.commands!]},
    timeline: {...campaignRunning.timeline!, items: [movedItem, {...movedItem, key: 'm:4', id: 4, status: 'refused', digest: 'c9d0e1f2',
        from: 'b7e1c2d4aa', problem: 'The game refused it: Story region does not fit its live cell: pier'}, ...campaignRunning.timeline!.items]}};
const campaignPaused: CampaignsInitial = {...campaignList, tab: 'orders', detail: detailOf(pausedRow, {orders: [refused,
    {...refused, id: 5513, kind: 'story.phase', target: pausedRow.run, status: 'applied', result: ''}]}),
    timeline: {run: pausedRow.run, names, events: {}, before: null, since: {}, now: at, items: []}};

// (Doc 85, Phase 7: a batch stuck at its second order, one sent for the wolf to come back, and an effect waiting for its wolf.)
const batch: Order = {...refused, id: 5520, kind: 'story.effects', target: pausedRow.run, status: 'applied', result: '1 of 3 applied, 1 refused',
    items: [{n: 0, kind: 'npc.speak', target: 'rm111', ifRefused: 'pause', status: 'applied', result: 'ok'},
        {n: 1, kind: 'deed.award', target: 'player-ash', ifRefused: 'pause', status: 'refused', result: 'that wolf is not in the world just now'},
        {n: 2, kind: 'item.give', target: 'player-birch', ifRefused: 'when here', status: 'held', result: 'Not applied: an order before it was refused.'}]};
const campaignBatch: CampaignsInitial = {...campaignPaused, detail: {...campaignPaused.detail!, orders: [batch, ...campaignPaused.detail!.orders],
    whenHere: [{wolf: 'player-birch', kind: 'character.grant', until: 1795400000, refused: 'The purse is short (3p).', receipt: 5511}]},
    overview: {...campaignPaused.overview, runs: campaignPaused.overview.runs.map(r => r.run === pausedRow.run
        ? {...r, waiting: [{on: 'admin', problem: 'Story action 5520 (deed.award for player-ash) refused', order: batch}]} : r)}};

// (Doc 85, Phase 6: the console's quests, each wolf's copy, a party's, the living audience and the wolves' parties.)
const ropeQuest: QuestBoard = {id: 'rope', title: 'Rope for the carter', scope: 'player', repeat: 'weekly', giver: 'rm111', open: true,
    objectives: [{id: 'ask', label: 'Ask the carter what he needs.'}, {id: 'bring', label: 'Bring him two coils of rope.', goal: 2, measure: 'quantity', after: ['ask']},
        {id: 'spare', label: 'A spare wheel.', optional: true}, {id: 'thief', label: 'Find who cut it.', hidden: true}],
    copies: [{holder: 'player-ash', kind: 'wolf', members: ['player-ash'], status: 'active', began: 10, objectives: {ask: {n: 1, done: true}, bring: {n: 1}}},
        {holder: 'player-birch', kind: 'wolf', members: ['player-birch'], status: 'ready', began: 9, objectives: {ask: {n: 1, done: true}, bring: {n: 2, done: true}, spare: {failed: true}}}],
    starts: 2, cap: 10};
const partyQuest: QuestBoard = {id: 'thief', title: 'Who cut the rope?', scope: 'party', giver: null, open: true,
    objectives: [{id: 'ask', label: 'Ask about the rope.'}, {id: 'name', label: 'Name the one who cut it.', hidden: true}],
    copies: [{holder: 'party:7', kind: 'party', members: ['player-ash', 'player-birch'], left: ['player-elm'], status: 'active', objectives: {ask: {n: 1, done: true}}}]};
const campaignQuests: CampaignsInitial = {...campaignRunning, tab: 'quests', detail: {...campaignRunning.detail!, quests: [ropeQuest, partyQuest],
    audience: {kind: 'open', cap: 5000, size: 3, joinable: true}, parties: {'player-ash': 'party:7', 'player-birch': 'party:7'},
    controls: {...campaignRunning.detail!.controls!, quests: [{id: 'rope', title: ropeQuest.title, objectives: ['ask', 'bring', 'spare', 'thief']}]}}};

// (Doc 85, Phase 3: what the run waits for, explained; a dry run's "this will do"; a stuck run; the Problems list.)
const explained: Explained = {run: runningRow.run, phase: 'ferry-watch', visit: 2, status: 'running', day: 12.9, speed: 1 / 1800, recent: ['player-ash'], goneDays: 14,
    stall: null, ending: false,
    listeners: [
        {source: 'trigger', id: 'offer-a-coil', on: 'gift', match: {item: 'rope', target: 'rm111'}, scope: 'campaign', once: 'each', audience: 'participant',
            off: false, used: false, actions: ['add'], enters: null, forWolves: ['player-ash', 'player-birch', 'player-elm'], needsWolves: true, can: true},
        {source: 'trigger', id: 'stocked-at-thaw', on: 'story signal', signal: 'spring-council', scope: 'campaign', once: 'each', audience: 'world', off: false,
            used: false, actions: ['enter'], enters: 'ferries-reopen', needsWolves: false, can: true, senders: [],
            timers: [{timer: 'spring-council', signal: 'spring-council', at: 90, clock: 'game'}],
            conditions: {test: 'counter', key: 'stores', scope: 'story', op: '>=', value: 2, expected: 5, verdict: false}},
        {source: 'trigger', id: 'open-winter-ledger', on: 'arrival', scope: 'campaign', once: 'campaign', audience: 'participant', off: false, used: true,
            actions: ['schedule', 'enter'], needsWolves: true, can: false, cannot: 'used'}],
    timers: [{name: 'watch-round', signal: 'watch-round', at: 17, clock: 'game', actor: 'player-ash', repeat: 7, scope: 'campaign', left: 4.1, seconds: 7380}]};
const dryRun: Preview = {run: runningRow.run, kind: 'enter', payload: {phase: 'deep-freeze', effects: true}, outcome: 'done',
    text: 'Entered deep-freeze (moved on), with its entry effects.', orders: [{kind: 'story.phase', target: runningRow.run, payload: {}}], fired: [], changes: [],
    missions: [], moved: true, phase: 'deep-freeze', visit: 3, next: [{kind: 'band.override', npc: 'rm111', band: 'poor', days: 28}]};
const problems: Problems = {errors: 2, warnings: 2, world: false, digest: null, problems: [
    {level: 'error', text: 'Choose a counter name and scope.', path: 'phases[1].triggers[0].actions[1]',
        at: {kind: 'trigger', phase: 1, phaseId: 'ferry-watch', index: 0, id: 'begin-deep-freeze', part: 'actions[1]'}},
    {level: 'error', text: 'Each phase needs a unique ID and journal entry.', path: 'phases[3]', at: {kind: 'phase', phase: 3, phaseId: 'ferries-reopen'}},
    {level: 'warning', text: 'Signal "releif" is sent but nothing listens for it.', path: 'triggers[1].actions[1]',
        at: {kind: 'trigger', phase: null, index: 1, id: 'offer-a-coil', part: 'actions[1]'}},
    {level: 'warning', world: true, blocks: true, text: 'Unknown npc in this world: rm404', path: 'phases[2].effects[1]',
        at: {kind: 'effect', phase: 2, phaseId: 'deep-freeze', index: 1}}]};

export function renderAll(): Record<string, string> {
    const me = {username: 'dm-user', role: 'dm'} as unknown as Me;
    const out: Record<string, string> = {};
    for (const slim of [true, false]) {
        const d = view(slim), at = slim ? '' : ' (before 0061)';
        out['Steer' + at] = renderToString(<SteerPanel data={d} locked={false} setSteer={none} />);
        out['Rules' + at] = renderToString(<RulesPanel data={d} locked={false} setRules={none} setServices={none} setNeeds={none} />);
        out['Seasons' + at] = renderToString(<SeasonsPanel data={d} locked={false} setSeasons={none} />);
        out['Imports' + at] = renderToString(<ImportsPanel data={d} locked={false} setImports={none} />);
        out['Status' + at] = renderToString(<StatusPanel data={d} locked={false} setStatus={none} />);
        out['Market' + at] = renderToString(<MarketPanel data={d} locked={false} setMarket={none} />);
    }
    out['Prosperity'] = renderToString(<ProsperityTab me={me} target="dev" initial={prosperity} />);
    // (A world with no town purses of its own, as Ridgemere 2.0: the realm's treasury, and a word why the table is empty.)
    out['Prosperity (no town purses)'] = renderToString(<ProsperityTab me={me} target="dev" initial={{...prosperity, towns: [], treasury: 1200}} />);
    out['My world (owner)'] = renderToString(<MyWorldTab me={{username: 'host-k7q2#3f2a1c', role: 'owner', hosted: true} as Me} initial={myWorld(true)} />);
    out['My world (dm)'] = renderToString(<MyWorldTab me={{username: 'pell#3f2a1c', role: 'dm', hosted: true} as Me} initial={myWorld(false)} />);
    // (Doc 78, M9: Share my world refused on a nearly full disk.)
    out['My world (share failed)'] = renderToString(<MyWorldTab me={{username: 'host-k7q2#3f2a1c', role: 'owner', hosted: true} as Me}
        initial={{...myWorld(true), share: {state: 'failed', at: now, started: now, file: null, size: null, kept: 3, thisHour: 1, perHour: 3,
            waiting: false, snapshot: false, files: [], problem: {code: 'disk', free: 300 * 1024 ** 2, need: 1280 * 1024 ** 2, where: 'folder'}}}} />);
    out['Campaigns'] = renderToString(<CampaignsTab me={me} target="dev" initial={campaignList} />);
    out['Campaigns (running)'] = renderToString(<CampaignsTab me={me} target="dev" initial={campaignRunning} />);
    out['Campaigns (paused)'] = renderToString(<CampaignsTab me={me} target="dev" initial={campaignPaused} />);
    out['Campaigns (a batch stuck)'] = renderToString(<CampaignsTab me={me} target="dev" initial={campaignBatch} />);
    out['Campaigns (triggers)'] = renderToString(<CampaignsTab me={me} target="dev" initial={{...campaignRunning, tab: 'triggers'}} />);
    out['Campaigns (audit)'] = renderToString(<CampaignsTab me={me} target="dev" initial={{...campaignRunning, tab: 'audit'}} />);
    out['Campaigns (held)'] = renderToString(<CampaignsTab me={me} target="dev" initial={{...campaignRunning,
        overview: {...campaignList.overview, runs: [{...runningRow, status: 'held', alerts: [{kind: 'held', needsYou: false, by: 'martin#a1', reason: 'a quick check', events: 4}],
            waiting: [{on: 'admin', problem: '', held: {by: 'martin#a1', reason: 'a quick check'}}]}]},
        detail: {...campaignRunning.detail!, status: 'held'}}} />);
    out['Campaigns (hosted dm)'] = renderToString(<CampaignsTab me={{username: 'pell#3f2a1c', role: 'dm', hosted: true} as Me} target="dev" initial={campaignRunning} />);
    out['Campaigns (viewer)'] = renderToString(<CampaignsTab me={{username: 'v', role: 'viewer'} as Me} target="dev" initial={campaignList} />);
    // (Doc 85, Phase 3.)
    out['Campaigns (explained)'] = renderToString(<CampaignsTab me={me} target="dev" initial={{...campaignRunning, explained}} />);
    // (Doc 85, Phase 8: a condition over the world's facts, judged, and the world as the game last told the story.)
    out['Campaigns (world facts)'] = renderToString(<CampaignsTab me={me} target="dev" initial={{...campaignRunning, explained: {...explained,
        listeners: [{...explained.listeners[0], conditions: {test: 'all', verdict: false, conditions: [
            {test: 'carries' as never, verdict: false, fact: 0, expected: 1, wolf: 'player-ash'},
            {test: 'hour between' as never, verdict: true, fact: 14}, {test: 'wolf in region' as never, verdict: null, needs: 'facts'}]}}],
        watch: {facts: ['wolves', 'world'], items: ['rope'], events: ['region occupied', 'trade done']},
        facts: {world: {hour: 14, minute: 5, weekday: 'Marketday', season: 'winter', period: 'day'},
            wolves: {'player-ash': {cell: 'grey', x: 31, y: 36, indoors: false, partySize: 2, carries: {}, coins: 40}}}}}} />);
    out['Campaigns (stuck)'] = renderToString(<CampaignsTab me={me} target="dev" initial={{...campaignList, overview: {...campaignList.overview,
        runs: [{...runningRow, alerts: [{kind: 'stuck', needsYou: true, phase: 'ferry-watch', reasons: {used: 1, gone: 1, unsent: 2}, goneDays: 14},
            {kind: 'overdue', needsYou: false, timer: 'watch-round', signal: 'watch-round', seconds: 600}, {kind: 'full', needsYou: false, wolves: 460, cap: 500}]}]}}} />);
    out['Campaigns (dry run)'] = renderToString(<AskDialog ask={{kind: 'enter', payload: {phase: 'deep-freeze', effects: true}, title: 'Go to deep-freeze',
        does: ['what the program says']}} row={runningRow} names={names} busy={false} send={none} cancel={none} target="dev" dryRun={dryRun} />);
    out['Campaigns (refused dry run)'] = renderToString(<AskDialog ask={{kind: 'fire', payload: {trigger: 'stocked-at-thaw'}, title: 'Fire stocked-at-thaw now', does: []}}
        row={runningRow} names={names} busy={false} send={none} cancel={none} target="dev"
        dryRun={{run: runningRow.run, kind: 'fire', payload: {trigger: 'stocked-at-thaw'}, outcome: 'refused', text: 'Its conditions are false just now.'}} />);
    out['Campaigns (new version)'] = renderToString(<CampaignsTab me={me} target="dev" initial={{...campaignRunning,
        detail: {...campaignRunning.detail!, versions: [approvedVersion, ...versions.slice(1).map(v => ({...v, status: 'current' as const}))]}}} />);
    out['Campaigns (move)'] = renderToString(<MovePanel row={runningRow} detail={campaignRunning.detail!} version={approvedVersion} names={names} busy={false}
        steer={none} close={none} />);
    out['Campaigns (move, phase gone)'] = renderToString(<MovePanel row={runningRow} detail={campaignRunning.detail!} names={names} busy={false} steer={none} close={none}
        version={{...approvedVersion, report: {...migration, blocked: 'Phase ferry-watch is not in the new version: name the phase to land in.',
            phase: {from: 'ferry-watch', to: null, choices: ['enrol', 'the-watch', 'deep-freeze'], gone: true}}}} />);
    out['Campaigns (moved)'] = renderToString(<CampaignsTab me={me} target="dev" initial={campaignMoved} />);
    out['Campaigns (moved, timeline)'] = renderToString(<CampaignsTab me={me} target="dev" initial={{...campaignMoved, tab: 'timeline'}} />);
    out['Campaigns (moving)'] = renderToString(<CampaignsTab me={me} target="dev" initial={{...campaignRunning, overview: {...campaignList.overview,
        runs: [{...runningRow, moving: true}]}}} />);
    out['Campaigns (move dry run)'] = renderToString(<AskDialog ask={{kind: 'migrate', payload: {version: 3}, title: 'Move onto version b7e1c2d4', does: []}}
        row={runningRow} names={names} busy={false} send={none} cancel={none} target="dev"
        dryRun={{run: runningRow.run, kind: 'migrate', payload: {version: 3}, outcome: 'done', migration, moved: false, phase: 'ferry-watch', visit: 2,
            text: 'Moving onto version b7e1c2d4 (from a41c9e00), in ferry-watch, visit 2: 4 kept, 1 reset, 1 defaulted, 1 dropped.',
            orders: [{kind: 'story.phase', target: runningRow.run, payload: {revise: true}}]}} />);
    out['Campaigns (quests)'] = renderToString(<CampaignsTab me={me} target="dev" initial={campaignQuests} />);
    out['Campaigns (wolves, parties)'] = renderToString(<CampaignsTab me={me} target="dev" initial={{...campaignQuests, tab: 'wolves'}} />);
    out['Campaigns (open audience)'] = renderToString(<CampaignsTab me={me} target="dev" initial={{...campaignList, overview: {...campaignList.overview,
        runs: [{...runningRow, audience: {kind: 'open', cap: 5000, size: 3}}]}}} />);
    // (Doc 85, Phase 10: a campaign as its chapters, in the list and on a chapter's page; the world's variables; the guide.)
    const ford: Campaign = {id: 'ford', title: 'The long winter', approvedBy: 'martin', approvedAt: at, status: 'running', chapters: [
        {story: 'the-muster', title: 'The muster', digest: 'a', starts: ['winter-ferries'], runs: [{run: 'world-old', status: 'complete', phase: 'gather', at}]},
        {story: 'winter-ferries', title: 'Ninety days on the winter ferries', digest: 'b', starts: [], runs: [{run: runningRow.run, status: 'running', phase: 'ferry-watch', at}]},
        {story: 'spring-council', title: 'The spring council', digest: 'c', starts: [], runs: []}]};
    out['Campaigns (chapters)'] = renderToString(<CampaignsTab me={me} target="dev" initial={{...campaignList, overview: {...campaignList.overview, campaigns: [ford]}}} />);
    out['Campaigns (a chapter)'] = renderToString(<CampaignsTab me={me} target="dev" initial={{...campaignRunning, detail: {...campaignRunning.detail!,
        campaign: {...ford, chapter: 1}, chapterStarts: [{story: 'spring-council', audience: 'same', when: 'ended', carry: true}]}}} />);
    out['World variables'] = renderToString(<WorldVarsPanel target="dev" names={names} mayChange initial={{declarations: true, variables: [
        {name: 'ferries-run', type: 'number', value: 4, declared: true, note: 'ferries that crossed this winter', declaredBy: 'martin', updatedBy: 'martin',
         stories: [{run: runningRow.run, story: 'winter-ferries', title: 'Ninety days on the winter ferries', type: 'number', status: 'running'}]},
        {name: 'bells', type: 'switch', value: true, declared: false, run: 'world-old', stories: []}],
        audit: [{at: 1763800000, name: 'ferries-run', detail: 'ferries-run: 3 → 4. Reason: "one more crossed at dawn"', who: 'martin'}]}} />);
    out['Guide'] = renderToString(<GuideTab />);
    out['Guide (search)'] = renderToString(<GuideTab initialQuery="purse crossed" />);
    out['Story problems'] = renderToString(<ProblemsList found={problems} untried={['ferry-watch/begin-deep-freeze']} phases={['enrol', 'ferry-watch']} go={none} asking={false} />);
    out['DM tab bar'] = renderToString(<DmSuite me={me} target="dev" alertsInitial={{runner: {beatAt: earlier, age: 95}, runnerDown: true, live: 2, needsYou: 2, at,
        runs: [{run: 'world-77ab', title: 'Two ways across Reed Ford', status: 'paused', kinds: ['paused'], text: 'Story "Two ways across Reed Ford" is paused.'}]}} />);
    return out;
}

// The trigger workbench (doc 85, Phase 4) over every bundled example and every library draft: each rendered whole with each
// of its triggers and exits selected in turn, so a program that makes it throw fails here; and none of them edited by
// being shown (`edit` is never called while rendering: the round trip guard, with workbench.test.ts's).
export const WB_REFS: Refs = {
    residents: [{id: 'rm111', name: 'Ivo Tarmarsh', work: 'carter'}, {id: 'rm112', name: 'Wren the innkeeper'}],
    wolves: [{id: 'player-ash', name: 'Ash Greymantle'}, {id: 'player-birch', name: 'Birch Oakenfall'}],
    places: [{id: 'reed_ford', kind: 'cell', name: 'Reed Ford', width: 64, height: 64, town: 'greyfen'}],
    items: [{id: 'rope', name: 'Rope (coil)', category: 'tool'}], factions: [{id: 'ridgemere_watch', name: 'The Ridgemere Watch'}],
    towns: [{id: 'greyfen', name: 'greyfen'}], markets: [], areas: [], ships: [], templates: [], projects: [],
};
export function renderWorkbenches(catalog: FullCatalogue, programs: Record<string, Program>): {html: Record<string, string>; edits: number; views: number} {
    const html: Record<string, string> = {};
    let edits = 0, views = 0;
    const edit = () => { edits++; };
    for (const [name, program] of Object.entries(programs)) {
        const before = pretty(program);
        const refs: Ref[] = [];
        for (const b of buildTree(program)) {
            for (const f of b.folders) refs.push(...f.items.map(i => i.ref));
            refs.push(...b.loose.map(i => i.ref), ...b.exits);
            if (b.scope !== null) refs.push({kind: 'phase', phase: b.scope}, {kind: 'missions', phase: b.scope});
        }
        refs.push({kind: 'settings'}, {kind: 'tests'}, {kind: 'scenes'});
        // (Phase 6: the quests, and each quest in the Quest editor.)
        if (program.quests) refs.push({kind: 'quests'}, ...program.quests.map((_, index) => ({kind: 'quest', index}) as Ref));
        let first = '';
        for (const ref of refs) {
            const out = renderToString(<Workbench program={program} edit={edit} catalog={catalog} refs={WB_REFS} world={null} target="dev" sceneLibraryKey="s"
                templatesKey="t" problems={null} checking={false} chosenCase={null} stepping={null} startFromLibrary={none} initial={ref} />);
            views++;
            if (!first) first = out;
        }
        if (pretty(program) !== before) throw Error(name + ': the workbench changed the program it showed');
        html[name] = first;
    }
    return {html, edits, views};
}
export function renderWorkbenchParts(catalog: FullCatalogue, program: Program, report: Report, steps: Steps): Record<string, string> {
    const out: Record<string, string> = {};
    const cat = catalog.catalogue;
    const bench = {cat, refs: WB_REFS, names: namesOf(WB_REFS, program), program, world: null, edit: none, problems: {}};
    out['Workbench rehearsal'] = renderToString(<BenchContext.Provider value={bench}><WordingContext.Provider value={wordingOf(cat, {})}>
        <RehearsalStrip report={report} program={program} target="dev" busy={false}
        rehearse={none} chosen={0} choose={none} stepping={{steps, at: Math.min(2, steps.steps.length - 1)}} setStepping={none} /></WordingContext.Provider></BenchContext.Provider>);
    // (Doc 85, Phase 6: the Quest editor, the small quest flow in New…, and a quest effect's pickers.)
    const quest = JSON.parse(questCampaignText) as Program;
    const questBench = {...bench, names: namesOf(WB_REFS, quest), program: quest};
    out['Quest editor'] = renderToString(<Workbench program={quest} edit={none} catalog={catalog} refs={WB_REFS} world={null} target="dev" sceneLibraryKey="s"
        templatesKey="t" problems={null} checking={false} chosenCase={null} stepping={null} startFromLibrary={none} initial={{kind: 'quest', index: 0}} />);
    out['Quest editor (party, hidden, fail rules)'] = renderToString(<Workbench program={quest} edit={none} catalog={catalog} refs={WB_REFS} world={null} target="dev" sceneLibraryKey="s"
        templatesKey="t" problems={{errors: 1, warnings: 0, world: false, digest: null, problems: [{level: 'error', text: 'The purse must be a town, a till or a Chapter.',
            path: 'quests[1].rewards[0]', at: {kind: 'quest', index: 1, id: 'thief', part: 'rewards[0]'}}]}} checking={false} chosenCase={null} stepping={null}
        startFromLibrary={none} initial={{kind: 'quest', index: 1}} />);
    out['Quest effects'] = renderToString(<Workbench program={quest} edit={none} catalog={catalog} refs={WB_REFS} world={null} target="dev" sceneLibraryKey="s"
        templatesKey="t" problems={null} checking={false} chosenCase={null} stepping={null} startFromLibrary={none} initial={{kind: 'trigger', phase: null, index: 0}} />);
    out['Quest pickers'] = renderToString(<BenchContext.Provider value={questBench}>
        <Picker slot={{slot: 'objective', keys: ['objective'], type: 'objective', label: 'the objective', value: 'name', shown: '', empty: false,
            param: {key: 'objective', type: 'objective', label: 'the objective', quest: 'thief'}}} pick={none} close={none} />
        <Picker slot={{slot: 'purse', keys: ['purse'], type: 'purse', label: 'who pays', value: '', shown: '', empty: true}} pick={none} close={none} />
        <ActionPicker rewards close={none} pick={none} /></BenchContext.Provider>);
    out['New small quest'] = renderToString(<NewStory catalog={catalog as never} refs={WB_REFS} close={none} start={none} pick={none} questing />);
    void smallQuestText;
    out['Workbench pickers'] = renderToString(<BenchContext.Provider value={bench}><EventPicker value="gift" close={none} pick={none} />
        <ActionPicker close={none} pick={none} />
        <Picker slot={eventParts(cat, {on: 'gift', match: {target: 'rm111'}}, bench.names).find(p => 'slot' in p && p.slot === 'target') as never} pick={none} close={none} clearable />
        <Picker slot={{slot: 'item', keys: ['item'], type: 'item', label: 'the item', value: 'rope', shown: 'Rope (coil)', empty: false}} pick={none} close={none} />
        <Picker slot={{slot: 'x', keys: ['cell', 'x', 'y'], type: 'tile', label: 'a tile', value: {}, shown: '', empty: true}} pick={none} close={none} />
        </BenchContext.Provider>);
    return out;
}

