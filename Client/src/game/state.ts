// The game screen's state and rules: SRatwGame (UI/SRatwGame.cpp) without its drawing. The client is a projection of
// observer-filtered server data; no simulation lives here. paint.ts draws it; view.ts connects it to the page.
import {arr, bool, boundedNum, clamp, envNumber, explicitTrue, num, obj, objects, str, wholeCount, type Json} from './json.ts';
import {heightFromChar, type EnvironmentView, type ScentCue} from './labels.ts';
import {MotionBuffer} from './motionBuffer.ts';
import {contains, rect, type Rect} from '../ui/painter.ts';
import {FarAway, type MotionFrame} from '../net/motion.ts';
import type {DoorState, Walker} from './walker.ts';
import {apart, arenaRows, arenaSight, fighterAt, myTurn, octant, readBattle, stepToward, readChallenge, readFights, readGround, type BattleLine, type BattleView,
    type ChallengeView, type FightSquare, type GroundView, type GearView} from './battle.ts';
import {FightEffects} from './fightFx.ts';

/** One entry in the story per fight (Docs/Design/18-combat-presentation.md): the latest, and all of it when expanded. */
export interface EncounterView {
    id: string;
    lines: BattleLine[];
    actions: number;
    latest: string;
    over: boolean;
    expanded: boolean;
    version: number;
}
import {inParty, readParty, type PartyView} from './party.ts';

export interface Post {
    id: string;
    speaker: string;
    to: string[];               // Whom it was meant for, as this player can tell ("you", a name, "someone").
    text: string;
    channel: string;
    kind: string;
    color: number;
    revealed: number;
    postedAt: number;
    system: boolean;
    party: boolean;             // Said in character to the speaker's party, and this player is in it (doc 32).
    chapter?: boolean;          // Said in character to the speaker's Chapter, and this player is in it.
    encounter?: EncounterView;  // The story's one entry for a fight (doc 18), kept up to date in place.
    muffled?: boolean;          // Said around a sword held in the jaws (doc 33).
    faint?: boolean;            // A voice heard, not a word of it made out.
}

export interface Hit {
    rect: Rect;
    action: string;
    target: string;
}

export interface EntityView {
    id: string;
    name: string;
    kind: string;
    state: string;
    actions: string[];
    x: number;
    y: number;
    facing: number;
    motion: MotionBuffer;
    color: number;
    self: boolean;
    typing: boolean;
    speaking: boolean;
    moving: boolean;
    spokenAt: number;
    work: string;               // A resident's trade, as the server gives it ('' for players).
    hostile: boolean;
    rel: string;                // Who they are to this wolf (doc 32): 'party', 'chapter', 'hostile' or ''.
    colour: string;             // A Chapter mate's Chapter colour ("#rrggbb").
    why: string;                // Why hostile: 'bandit', 'fighting you', 'fought your party'...
    appearance: Json | null;
    lifeStage: string;
    artwork: string;            // An uploaded portrait this player may see ('' for none).
    gear: GearView[];           // Armour and weapons they have on, plain to see (doc 35).
    placed?: boolean;           // The own wolf has been drawn once (it then eases instead of jumping).
}

/** The composer, a real text box on the page (or a stand-in in tests). */
export interface Composer {
    text: string;
    focus(): void;
    blur(): void;
    insertNewline(): void;
}

export interface KeyInput {
    code: string;               // KeyboardEvent.code: a physical key, whatever the layout.
    shift?: boolean;
    alt?: boolean;
    ctrl?: boolean;
}

const MovementKeys = ['KeyW', 'KeyA', 'KeyS', 'KeyD'];
const BodyWidth = 0.5;                                  // Two wolves' bodies (Core/RatwStep.h: twice BodyRadius).
const MaxTargets = 4;
const MapScalesForPan = [0.25, 0.5, 1, 2, 4, 8];    // As minimap.ts MapScales (kept here so state has no drawing import).
/** The story column's presets (CSS pixels): balanced, wide, text-first, compact. */
export const StoryWidths = [460, 600, 760, 360];
const KnownWeather = ['overcast', 'rain', 'storm', 'fog', 'snow', 'sandstorm'];
export const DevWeathers = ['clear', 'overcast', 'rain', 'storm', 'fog', 'snow', 'sandstorm'];

function defaultEnvironment(): EnvironmentView {
    return {weather: 'clear', intensity: 1, phase: 'day', lightingTone: 'neutral', lightSource: 'daylight', hour: 12, daylight: 1,
        illumination: 1, artificialLight: 0, daylightAccess: 1, glowStrength: 0, sight: 1, hearing: 1, scent: 1, movement: 1};
}

export class GameState {
    // What the screen is showing.
    snapshot: Json | null = null;
    inspectedCharacter: Json | null = null;
    reputation: string[] = [];          // The last answer to "what's said of me about town" (doc 32, 1.4).
    missionBoard: Json | null = null;   // The last faction mission board asked for (doc 32, 4.5).
    posts: Post[] = [];
    entities = new Map<string, EntityView>();
    motionVisible = new Set<string>();
    motionClock = 0;
    motionOffset = 0;
    latestMotionTime = -1;
    motionClockReady = false;
    cellGeneration = -1;
    scentCues: ScentCue[] = [];
    seenPosts = new Set<string>();
    pendingDrafts = new Map<string, string>();
    failedDraft = '';
    nextRequestId = 0;
    heldKeys = new Set<string>();
    hits: Hit[] = [];
    // Set while drawing: where the map is.
    mapRect: Rect = rect(0, 0, 0, 0);
    mapOrigin: [number, number] = [0, 0];
    tileSize = 23;
    hover: [number, number] = [-1000, -1000];
    mapPan: [number, number] = [0, 0];
    // After a change of cell, the camera starts where the last one left the wolf on screen and slides home.
    cameraShift: [number, number] = [0, 0];
    clock = 0;
    lastTyping = -100;
    lastTypingSent = -100;
    lastMove = -100;
    revealFraction = 0;
    cellWidth = 32;
    cellHeight = 24;
    selectedColor = 0;
    transcriptScroll = 0;
    cellId = '';
    cellName = 'Connecting…';
    sceneDescription = '';
    selfId = '';
    channel = 'ic';
    volume = 'speak';
    modal = '';
    /** The Dev Console (a player marked Dungeon Master; ui/hud/devConsole.ts): open or not, the commands the server
     *  offers ([name, help], as it last said), and each command run with the server's answer, newest last. */
    devConsole = false;
    devCommands: [string, string][] = [];
    devLog: {command: string; ok: boolean; text: string}[] = [];
    private devCommandsAsked = false;
    contextTarget = '';
    /** Combat sound (doc 37, phase 3): 0 off, up to 1; Settings cycles it, and it is kept on this computer. */
    soundVolume = 0.6;
    /** Plays a fight's sound (set by the page: ui/sound.ts). */
    onCue: ((cue: string) => void) | null = null;
    /** Turning one's wolf by dragging from it (doc 37): where the drag began, and the way it points now (−1: not yet). */
    faceDrag: [number, number] | null = null;
    faceDragDir = -1;
    /** The foe the fight screen's actions are aimed at (doc 37): chosen from their card, else the nearest. */
    fightFocus = '';
    /** The fight screen's own keys, while it shows (set by ui/hud/combat.ts): true when one was used. */
    fightKeys: ((code: string) => boolean) | null = null;
    /** "attack|<id>" once Attack has been chosen once on a resident: chosen again, it is done (a crime asks first). */
    armed = '';
    contextName = '';
    contextKind = '';
    contextPoint: [number, number] = [0, 0];
    contextActions: string[] = [];
    tileRows: string[] = [];
    visibilityRows: string[] = [];
    tileHeights: Float32Array = new Float32Array(0);
    private sources: {rows: unknown; visibility: unknown; heights: unknown} = {rows: null, visibility: null, heights: null};
    environment = defaultEnvironment();
    chat = false;
    worldMap = false;
    reducedMotion = false;
    flatWorld = false;
    typingSent = false;
    plainGlyphs = false;
    facingPreview = false;
    navigationFocus = true;
    movementPending = false;
    movementHeard = false;
    outdoors = false;
    windVariable = false;
    windDirection = 0;
    windStrength = 0;
    previewFacing = 0;
    revealSpeed = 64;
    requestedPace = -1;
    travelPage = 0;
    lastPaceRequest = -100;
    travelAtlas = false;
    storyWidth = 460;           // The story column, in CSS pixels (dragged, or one of the presets).
    zoom = 2;                   // Index into paint.ts ZoomTiles.
    contextPage: [number, number] | null = null;   // A menu opened from a panel, at this page point (else on the map).
    highlight = '';             // A wolf pointed at in the In Sight list: ringed on the map.
    hoveredEntity = '';         // The wolf under the pointer on the map: lit in the list.
    hoverTooltips = true;       // Labels beside the pointer (the line under the map always shows).
    perfOverlay = false;        // The latency overlay (Settings, or ?perf): frames, ping, input to motion, traffic.
    /** From a key that sets a standing wolf walking to the first motion frame that shows it moved (ms), the last 30. */
    readonly inputToMotion: number[] = [];
    private inputSentAt = 0;
    private inputFrom: [number, number] | null = null;
    private walking = false;
    private selfPose: [number, number] | null = null;
    // Free movement (Docs/Design/31-responsiveness.md, Phase 3): the page walks its own wolf with the server's own
    // rules (walker.ts) and says where it is; the server checks every pose. The mode is the server's: 0 free, 1 held
    // (the server walks the wolf, around a fight or along a route), 2 fighting.
    walker: Walker | null = null;
    /** Sends a pose in binary (main.ts sets it); without it (tests) a pose goes as a JSON command. */
    poseSender: ((seq: number, x: number, y: number, facing: number, ix: number, iy: number) => void) | null = null;
    movementMode = 1;
    private walkAsked = false;
    private freePose: {x: number; y: number; facing: number} | null = null;
    private poseSeq = 0;
    private poseAckSeen = 0;
    private inputSeq = 0;
    private lastPoseSent = -1;
    private poseUnsent = false;
    private easeUntil = 0;
    /** Poses the server refused (the overlay and the walking smoke look at it). */
    corrections = 0;
    private doorStates: DoorState[] = [];
    private cellVersion = 0;
    private walkerVersion = -1;
    private walkerBlocked = '';
    // Terrain shading worked out here (doc 31, Phase 4.6): which tiles the wolf sees, with the server's own sight
    // rule, on reaching a new tile; the server then leaves its visibility rows out. Which tiles the page knows at all
    // (the glyphs it was sent) stays the server's.
    clientSight = false;
    private sightKey = '';
    talkTargets: string[] = []; // Whom the player is speaking to (up to four), until they leave sight or are let go.
    // The regional weather over the cell (doc 29, phase 7): a letter (kind) and a digit (strength) every `step` tiles.
    weatherField: {cols: number; rows: number; step: number; kinds: string; amounts: string} | null = null;
    worldZoom = -1;             // The World Map's scale (minimap.ts MapScales; -1 fits the known places), and its pan (tiles).
    worldPan: [number, number] = [0, 0];
    miniZoom = 3;               // The minimap's scale (2 pixels a tile).
    inspectedText = '';
    // Fights (Docs/Design/33-combat.md): the arena this wolf fights in or watches, the red squares in sight, a
    // challenge to answer; when the last fight ended (everyone fades back into the world).
    battle: BattleView | null = null;
    walkShown = new Map<string, {x: number; y: number; at: number}>();   // Fighters as drawn, gliding (walkOffset).
    strikeOnArrival: {target: string; verb: string; x: number; y: number; range: number} | null = null;
    fights: FightSquare[] = [];
    challenge: ChallengeView | null = null;
    // The party (doc 32): who is in it and where, an invitation waiting, a party mate's fight calling.
    party: PartyView | null = null;
    fightEndedAt = -10;
    battleOverSeenAt = -10;
    battleAt = 0;               // When the fight as last sent arrived (bars and timers run on from it).
    private lastWalkHint = -10;
    aiming = '';                // Choosing where a spell goes ("flame"), until a tile is clicked or Escape.
    ground: GroundView[] = [];  // Things lying in sight (a sword knocked loose).
    readonly fx = new FightEffects();
    readonly encounters = new Map<string, EncounterView>();
    private plainRows: string[] = [];
    private mergedRows: string[] | null = null;
    private arenaSightKey = '';
    private arenaSightRows: string[] = [];
    toast = '';
    toastUntil = 0;

    readonly send: (command: Json) => void;
    readonly composer: Composer;

    constructor(send: (command: Json) => void, composer: Composer) {
        this.send = send;
        this.composer = composer;
    }

    // ------------------------------------------------------------------ Receiving

    applySnapshot(s: Json) {
        this.snapshot = s;
        const cell = obj(s, 'cell') ?? s;
        const newId = str(cell, 'id', str(s, 'cellId', this.cellId));
        const generation = Math.trunc(num(s, 'cellGeneration', -1));
        if (newId !== this.cellId || generation !== this.cellGeneration) {
            this.heldKeys.clear();
            this.mapPan = [0, 0];
            this.contextTarget = '';
            this.facingPreview = false;
            this.entities.clear();
            this.motionVisible.clear();
            this.latestMotionTime = -1;
            this.motionClockReady = false;
            this.freePose = null;
            ++this.cellVersion;
        }
        this.cellGeneration = generation;
        this.cellId = newId;
        const poseTime = num(s, 'time', this.motionClock);
        if (poseTime >= this.latestMotionTime) this.observeMotionTime(poseTime);
        this.cellName = str(cell, 'name', this.cellName);
        this.sceneDescription = str(cell, 'description', this.sceneDescription);
        this.cellWidth = clamp(Math.trunc(num(cell, 'width', 32)), 1, 256);
        this.cellHeight = clamp(Math.trunc(num(cell, 'height', 24)), 1, 256);
        this.outdoors = bool(cell, 'outdoors');
        const env = obj(cell, 'environment');
        const e = this.environment;
        // The weather where the wolf stands (doc 29, phase 7), else the cell's.
        const local = obj(cell, 'localWeather');
        e.weather = str(local, 'kind', str(cell, 'weather', 'clear'));
        if (!KnownWeather.includes(e.weather)) e.weather = 'clear';
        e.intensity = local ? envNumber(local, 'intensity', 0, 1, 1) : 1;
        if (e.weather === 'clear') e.intensity = 0;
        this.weatherField = null;
        const field = obj(cell, 'weatherField');
        if (field) {
            const cols = Math.trunc(num(field, 'cols')), rows = Math.trunc(num(field, 'rows')), step = Math.trunc(num(field, 'step'));
            const kinds = str(field, 'kinds'), amounts = str(field, 'amounts');
            if (cols > 0 && rows > 0 && cols * rows <= 4096 && step > 0 && kinds.length === cols * rows && amounts.length === cols * rows)
                this.weatherField = {cols, rows, step, kinds, amounts};
        }
        e.hour = envNumber(env, 'hour', 0, 24, 12);
        if (e.hour >= 24) e.hour = 0;
        e.phase = str(env, 'phase');
        if (!['day', 'dawn', 'dusk', 'night'].includes(e.phase))
            e.phase = e.hour < 5 || e.hour >= 19 ? 'night' : e.hour < 7 ? 'dawn' : e.hour < 17 ? 'day' : 'dusk';
        e.daylight = envNumber(env, 'daylight', 0, 1, 1);
        e.illumination = envNumber(env, 'illumination', 0, 1, 1);
        e.artificialLight = envNumber(env, 'artificialLight', 0, 1, 0);
        e.daylightAccess = envNumber(env, 'daylightAccess', 0, 1, 1);
        e.glowStrength = envNumber(env, 'glowStrength', 0, 1, 0);
        e.lightingTone = str(env, 'lightingTone', 'neutral');
        if (e.lightingTone !== 'warm' && e.lightingTone !== 'cool') e.lightingTone = 'neutral';
        e.lightSource = str(env, 'lightSource', 'daylight');
        if (!['dark', 'artificial', 'mixed'].includes(e.lightSource)) e.lightSource = 'daylight';
        e.sight = envNumber(env, 'sight', 0, 4, 1);
        e.hearing = envNumber(env, 'hearing', 0, 4, 1);
        e.scent = envNumber(env, 'scent', 0, 4, 1);
        e.movement = envNumber(env, 'movement', 0, 4, 1);
        const wind = obj(cell, 'wind');
        const direction = num(wind, 'direction'), strength = num(wind, 'strength');
        this.windDirection = Number.isFinite(direction) ? Math.atan2(Math.sin(direction), Math.cos(direction)) : 0;
        this.windStrength = this.outdoors && Number.isFinite(strength) ? clamp(strength, 0, 1) : 0;
        this.windVariable = this.outdoors && bool(wind, 'variable');
        const senses = obj(s, 'senses');
        this.movementHeard = bool(senses, 'movementHeard');
        const cues: ScentCue[] = [];
        for (const cue of objects(senses, 'scentCues')) {
            const sector = num(cue, 'sector', -1), intensity = num(cue, 'strength', -1);
            if (!Number.isInteger(sector) || sector < 0 || sector > 7 || !Number.isInteger(intensity) || intensity < 1 || intensity > 3) continue;
            const windborne = this.outdoors && this.windStrength > 0.01 && bool(cue, 'windborne');
            const existing = cues.find(c => c.sector === sector);
            if (existing) {
                existing.strength = Math.max(existing.strength, intensity);
                existing.windborne ||= windborne;
            } else cues.push({sector, strength: intensity, windborne});
        }
        this.scentCues = cues.sort((a, b) => a.sector - b.sector);
        this.selfId = str(s, 'selfId', str(s, 'playerId', this.selfId));
        const self = obj(s, 'self');
        if (self) {
            this.selfId = str(self, 'id', this.selfId);
            this.selectedColor = Math.trunc(num(self, 'color', this.selectedColor));
            const pace = Math.trunc(boundedNum(self, 'pace', 0, 10));
            if (pace === this.requestedPace || this.clock - this.lastPaceRequest > 1.5) this.requestedPace = -1;
        }
        // The big parts come back as the very same arrays while they are unchanged (sections.ts keeps them), so
        // their decoded forms are kept too: the map's offscreen ground is redrawn only when something really changed.
        const strings = (list: unknown[]) => list.filter((v): v is string => typeof v === 'string');
        const rowsSource = arr(cell, 'tiles').length ? arr(cell, 'tiles') : arr(cell, 'rows');
        if (rowsSource !== this.sources.rows || !this.tileRows.length) {
            this.sources.rows = rowsSource;
            this.tileRows = strings(rowsSource);
            ++this.cellVersion;
        }
        // What the wolf sees: from the server, unless this page works it out itself (Phase 4.6: shadeTerrain).
        const seenSource = arr(s, 'visibility');
        if (!this.clientSight && seenSource !== this.sources.visibility) {
            this.sources.visibility = seenSource;
            this.visibilityRows = strings(seenSource);
        }
        const heights = arr(cell, 'heights');
        if (heights !== this.sources.heights || this.tileHeights.length !== this.cellWidth * this.cellHeight) {
            this.sources.heights = heights;
            ++this.cellVersion;
            this.tileHeights = new Float32Array(this.cellWidth * this.cellHeight);
            for (let y = 0; y < heights.length && y < this.cellHeight; ++y) {
                const row = heights[y];
                if (typeof row !== 'string') break;
                for (let x = 0; x < Math.min(row.length, this.cellWidth); ++x) this.tileHeights[y * this.cellWidth + x] = heightFromChar(row[x]);
            }
        }
        if (!this.tileRows.length && objects(cell, 'tiles').length) {
            // Tiles sent one by one, as objects.
            this.tileHeights = new Float32Array(this.cellWidth * this.cellHeight);
            this.sources = {rows: null, visibility: null, heights: null};
            const rows = Array.from({length: this.cellHeight}, () => Array(this.cellWidth).fill(' '));
            const seen = Array.from({length: this.cellHeight}, () => Array(this.cellWidth).fill('0'));
            for (const t of objects(cell, 'tiles')) {
                const x = Math.trunc(num(t, 'x')), y = Math.trunc(num(t, 'y'));
                if (x < 0 || y < 0 || x >= this.cellWidth || y >= this.cellHeight) continue;
                rows[y][x] = str(t, 'glyph', ' ')[0] ?? ' ';
                // A malformed height draws level ground, never a false cliff.
                this.tileHeights[y * this.cellWidth + x] = envNumber(t, 'height', -16, 16, 0);
                seen[y][x] = bool(t, 'visible') ? '2' : bool(t, 'remembered') ? '1' : '0';
            }
            this.tileRows = rows.map(r => r.join(''));
            this.visibilityRows = seen.map(r => r.join(''));
        }
        const doors = objects(s, 'doors').map(d => ({x: num(d, 'x'), y: num(d, 'y'), open: bool(d, 'open')}));
        if (doors.length !== this.doorStates.length || doors.some((d, i) => d.open !== this.doorStates[i].open || d.x !== this.doorStates[i].x)) {
            this.doorStates = doors;
            ++this.cellVersion;
        }
        // A client that can walk its own wolf says so once it is in the world.
        if (self && this.walker && !this.walkAsked) {
            this.walkAsked = true;
            this.clientSight = true;
            this.send({type: 'walking', mode: 'client', sight: 'client'});
        }
        const present = new Set<string>();
        const list = objects(s, 'entities');
        // Exactly one self pose per timestamp: the private row includes queued movement intent; the public duplicate
        // must never win the sample.
        if (self) list.push(self);
        for (const e of list) {
            const id = str(e, 'id');
            if (!id) continue;
            if (self && id === this.selfId && e !== self) continue;
            if (poseTime < this.latestMotionTime && !this.motionVisible.has(id)) continue;
            present.add(id);
            let view = this.entities.get(id);
            if (!view) {
                view = {id, name: '', kind: 'player', state: '', actions: [], x: 0, y: 0, facing: 0, motion: new MotionBuffer(),
                    color: 0, self: false, typing: false, speaking: false, moving: false, spokenAt: -100, work: '', hostile: false,
                    rel: '', why: '', colour: '', appearance: null, lifeStage: 'adult', artwork: '', gear: []};
                this.entities.set(id, view);
            }
            view.name = str(e, 'name');
            view.kind = str(e, 'kind', bool(e, 'npc') ? 'npc' : 'player');
            view.state = str(e, 'state', str(e, 'posture', 'standing'));
            view.work = str(e, 'work');
            view.rel = str(e, 'rel');
            view.why = str(e, 'why');
            view.colour = str(e, 'colour');
            view.hostile = bool(e, 'hostile') || view.rel === 'hostile';
            view.appearance = obj(e, 'appearance');
            view.lifeStage = str(e, 'lifeStage', 'adult');
            view.artwork = str(e, 'artwork');
            view.gear = objects(e, 'gear').map(g => ({place: str(g, 'place'), name: str(g, 'name'), weapon: bool(g, 'weapon'), protect: num(g, 'protect')}));
            view.actions = arr(e, 'actions').filter((a): a is string => typeof a === 'string');
            if (!view.actions.length) view.actions = ['inspect'];
            this.applyPose(view, e, poseTime);
            view.color = Math.trunc(num(e, 'color'));
            view.self = id === this.selfId || bool(e, 'self');
            if (poseTime >= this.latestMotionTime) view.moving = bool(e, 'moving') || num(e, 'postureRemaining') > 0;
            view.typing = bool(e, 'typing');
            const speaking = bool(e, 'speaking');
            if (speaking && !view.speaking) view.spokenAt = this.clock;
            view.speaking = speaking;
        }
        for (const id of [...this.entities.keys()])
            if (!present.has(id) && poseTime >= this.latestMotionTime) this.entities.delete(id);
        this.movementPending = false;
        if (!this.canFaceAt(this.hover)) this.facingPreview = false;
        this.applyFight();
    }

    /** The fight, if any: the arena's ground and sight laid over the cell, and back to the world when it ends. */
    private applyFight() {
        const before = this.battle;
        this.battle = readBattle(this.snapshot);
        this.fights = readFights(this.snapshot);
        this.challenge = readChallenge(this.snapshot);
        this.ground = readGround(this.snapshot);
        this.battleAt = this.clock;
        this.strikeIfArrived();
        this.fx.selfId = this.selfId;
        this.fx.update(this.battle, this.clock);
        // One's own turn come round: a soft chime.
        if (this.battle && !this.battle.over && myTurn(this.battle, this.selfId) && !(before && myTurn(before, this.selfId))) this.fx.cue('turn');
        this.updateEncounters(before);
        if (!this.battle || !this.battle.flame || !myTurn(this.battle, this.selfId)) this.aiming = '';
        this.party = readParty(obj(this.snapshot, 'self'));
        if ((this.channel === 'party' || this.channel === 'partyooc') && !inParty(this.party)) this.channel = 'ic';
        if ((this.channel === 'chapter' || this.channel === 'chapterooc') && !this.inChapter()) this.channel = 'ic';
        if (this.tileRows !== this.mergedRows) this.plainRows = this.tileRows;     // Fresh rows from the server.
        const b = this.battle;
        if (b) {
            if (!before) {
                this.heldKeys.clear();
                this.contextTarget = '';
            }
            if (b.over && !before?.over) this.battleOverSeenAt = this.clock;
            const merged = arenaRows(this.plainRows, b);
            const same = this.mergedRows && merged.length === this.mergedRows.length && merged.every((r, i) => r === this.mergedRows![i]);
            if (!same) {
                this.mergedRows = merged;
                ++this.cellVersion;
            }
            this.tileRows = this.mergedRows!;
            const key = `${b.id}|${b.arena.x},${b.arena.y},${b.arena.w},${b.arena.h}|${this.cellWidth}x${this.cellHeight}`;
            if (key !== this.arenaSightKey) {
                this.arenaSightKey = key;
                this.arenaSightRows = arenaSight(this.cellWidth, this.cellHeight, b);
            }
            this.visibilityRows = this.arenaSightRows;
        } else if (before) {
            // Back in the world: its own ground and sight again, and everyone fades in where the fight left them.
            this.fightEndedAt = this.clock;
            this.tileRows = this.plainRows;
            this.mergedRows = null;
            this.arenaSightKey = '';
            this.sightKey = '';
            this.sources.visibility = null;
            const seen = arr(this.snapshot, 'visibility').filter((v): v is string => typeof v === 'string');
            if (!this.clientSight && seen.length) this.visibilityRows = seen;
            ++this.cellVersion;
        }
    }

    /** The story's entry for each fight this wolf is in, watches or can see: kept up to date in place. */
    private updateEncounters(before: BattleView | null) {
        const touch = (id: string, lines: BattleLine[], actions: number, latest: string, over: boolean) => {
            let enc = this.encounters.get(id);
            if (!enc) {
                enc = {id, lines: [], actions: 0, latest: '', over: false, expanded: false, version: 0};
                this.encounters.set(id, enc);
                this.posts.push({id: `encounter-${id}`, speaker: 'Combat', to: [], text: '', channel: 'ic', kind: 'encounter', color: 7,
                    revealed: 1, postedAt: this.clock, system: false, party: false, encounter: enc});
                if (this.posts.length > 300) this.posts.splice(0, this.posts.length - 300);
            }
            const last = enc.lines.at(-1)?.seq ?? 0;
            const fresh = lines.filter(l => l.seq > last);
            if (fresh.length || actions !== enc.actions || latest !== enc.latest || over !== enc.over) {
                enc.lines.push(...fresh);
                enc.actions = Math.max(actions, enc.lines.length);
                enc.latest = latest || enc.lines.at(-1)?.text || '';
                enc.over = over;
                ++enc.version;
                const post = this.posts.find(p => p.encounter === enc);
                if (post) {
                    post.text = `${enc.actions} actions · Latest: ${enc.latest}${over ? ' · ended' : ''}`;
                    post.revealed = post.text.length;
                }
            }
        };
        const b = this.battle;
        if (b) touch(b.id, b.log, b.log.at(-1)?.seq ?? 0, b.log.at(-1)?.text ?? '', b.over);
        for (const f of this.fights) touch(f.id, [], f.actions, f.latest, f.over);
        // A fight gone from view: ended, as far as this wolf can tell.
        if (before && !b) {
            const enc = this.encounters.get(before.id);
            if (enc && !enc.over) touch(before.id, [], enc.actions, enc.latest, true);
        }
        for (const enc of this.encounters.values())
            if (!enc.over && enc.id !== b?.id && !this.fights.some(f => f.id === enc.id)) touch(enc.id, [], enc.actions, enc.latest, true);
    }

    /** Where a fighter is drawn, gliding after its tile: a move is walked a tile at a time (doc 33). Returns the offset
     * from its tile, in tiles. */
    walkOffset(id: string, x: number, y: number, clock: number): [number, number] {
        const shown = this.walkShown.get(id);
        if (!shown || Math.max(Math.abs(shown.x - x), Math.abs(shown.y - y)) > 8 || this.reducedMotion) {
            this.walkShown.set(id, {x, y, at: clock});
            return [0, 0];
        }
        const dt = clamp(clock - shown.at, 0, 0.25), dx = x - shown.x, dy = y - shown.y, gap = Math.hypot(dx, dy);
        const step = Math.max(3, gap * 4) * dt;
        if (gap <= step) shown.x = x, shown.y = y;
        else shown.x += dx / gap * step, shown.y += dy / gap * step;
        shown.at = clock;
        return [shown.x - x, shown.y - y];
    }

    /** Turns to face a tile, on this wolf's turn (free). */
    arenaFace(tx: number, ty: number) {
        const b = this.battle;
        const me = b?.fighters.find(f => f.id === this.selfId);
        if (!b || !me || !myTurn(b, this.selfId) || (tx === me.x && ty === me.y)) return;
        this.sendBattle('face', {dir: octant(tx - me.x, ty - me.y)});
    }

    /** Turns an eighth left (−1) or right (+1), on this wolf's turn. */
    turnInFight(step: number) {
        const b = this.battle;
        const me = b?.fighters.find(f => f.id === this.selfId);
        if (!b || !me || !myTurn(b, this.selfId)) return;
        this.sendBattle('face', {dir: (me.facing + step + 8) % 8});
    }

    /** A fight command: move, bite, tend, flee, struggle, wait, join, observe, leave. */
    sendBattle(verb: string, extra: Json = {}) {
        this.send({type: 'battle', verb, ...extra});
    }

    /** A fighter clicked in the arena: bite an enemy, tend a fallen friend. */
    fightTarget(id: string) {
        const b = this.battle;
        if (!b || b.observer) return;
        const me = b.fighters.find(f => f.id === this.selfId);
        const f = b.fighters.find(o => o.id === id);
        if (!me || !f || f.id === me.id) return;
        if (f.side !== me.side && f.status === 'fighting') {
            // Out of reach on one's turn: step to the tile in reach that is nearest, then strike, in one click.
            const strike = b.mouth === 'sword' ? 'sword' : 'bite', range = strike === 'sword' ? 2 : 1;
            if (apart(me.x, me.y, f.x, f.y) > range && myTurn(b, this.selfId)) {
                // A move is walked (doc 33): the blow is struck on arrival, if the foe is still in reach then.
                const step = stepToward(b, me, f, range);
                if (step) this.sendBattle('move', {x: step.x, y: step.y});
                if (step?.reaches) this.strikeOnArrival = {target: id, verb: strike, x: step.x, y: step.y, range};
                return;
            }
            this.strikeOnArrival = null;
            this.sendBattle(strike, {target: id});
        }
        else if (f.side === me.side && f.status === 'downed') this.sendBattle('tend', {target: id});
    }

    /** A strike waiting on a walk (fightTarget): struck when this wolf gets there, still in its turn and in reach. */
    private strikeIfArrived() {
        const p = this.strikeOnArrival, b = this.battle;
        if (!p) return;
        const me = b?.fighters.find(f => f.id === this.selfId), foe = b?.fighters.find(f => f.id === p.target);
        if (!b || !me || !foe || b.over || !myTurn(b, this.selfId) || b.acted || foe.status !== 'fighting') {
            this.strikeOnArrival = null;
            return;
        }
        if (me.x !== p.x || me.y !== p.y) return;   // (Still walking.)
        this.strikeOnArrival = null;
        if (apart(me.x, me.y, foe.x, foe.y) <= p.range) this.sendBattle(p.verb, {target: p.target});
    }

    /** The foe the fight screen aims at: one pointed at (on the map or a card), else the one chosen, else the nearest. */
    fightTargetId(): string {
        const b = this.battle;
        const me = b && !b.observer ? b.fighters.find(f => f.id === this.selfId) : undefined;
        if (!b || !me) return '';
        const foes = b.fighters.filter(f => f.side !== me.side && f.status === 'fighting');
        for (const id of [this.hoveredEntity, this.highlight, this.fightFocus])
            if (id && foes.some(f => f.id === id)) return id;
        return foes.sort((a, c) => apart(a.x, a.y, me.x, me.y) - apart(c.x, c.y, me.x, me.y))[0]?.id ?? '';
    }

    /** A tile clicked in the arena: go there, if it's this wolf's turn and it can. */
    arenaClick(tx: number, ty: number) {
        const b = this.battle;
        if (!b) return;
        if (this.aiming === 'flame') {
            this.aiming = '';
            if (myTurn(b, this.selfId)) this.sendBattle('flame', {x: tx, y: ty});
            return;
        }
        const there = fighterAt(b, tx, ty);
        if (there) {
            this.fightTarget(there.id);
            return;
        }
        if (myTurn(b, this.selfId) && b.reach.some(([x, y]) => x === tx && y === ty)) this.sendBattle('move', {x: tx, y: ty});
    }

    observeMotionTime(serverTime: number) {
        const offset = this.motionClock - serverTime;
        if (this.motionClockReady && offset - this.motionOffset > 0.5) {
            // A suspended window or server may advance wall time farther than its capped simulation step: rebase
            // rather than starve the buffer forever.
            for (const view of this.entities.values()) view.motion.samples = [];
            this.motionClockReady = false;
        }
        this.motionOffset = this.motionClockReady ? Math.min(this.motionOffset, offset) : offset;
        this.motionClockReady = true;
    }

    private applyPose(view: EntityView, pose: Json, time: number) {
        const x = num(pose, 'x'), y = num(pose, 'y'), facing = num(pose, 'facing');
        if (!view.motion.add(time, x, y, facing)) return;
        if (view.motion.samples.length === 1) {
            view.x = x;
            view.y = y;
            view.facing = facing;
        }
    }

    applyMotion(frame: MotionFrame) {
        if (frame.cellId !== this.cellId || frame.cellGeneration !== this.cellGeneration) return;
        if (!(frame.time > this.latestMotionTime)) return;
        if (frame.poseAck !== undefined) this.poseAckSeen = frame.poseAck;
        if (frame.mode !== undefined) {
            this.movementMode = frame.mode;
            if (frame.mode !== 0) this.freePose = null;   // The server walks it now: its poses again.
        }
        this.latestMotionTime = frame.time;
        this.observeMotionTime(frame.time);
        this.motionVisible.clear();
        for (const pose of frame.entities) {
            this.motionVisible.add(pose.id);
            if (pose.id === this.selfId) this.noteSelfPose(pose.x, pose.y);
            // Metadata is observer-filtered too; poses never invent actors.
            const view = this.entities.get(pose.id);
            if (view) {
                this.applyPose(view, pose as unknown as Json, frame.time);
                view.moving = pose.moving;
            }
        }
        // A wolf not in the frame has gone from sight, at once; but a partial frame leaves far wolves out (Phase 4.3):
        // only one near enough that it would have been in it has gone.
        const me = this.entities.get(this.selfId);
        for (const [id, view] of [...this.entities])
            if (!this.motionVisible.has(id) &&
                (!frame.partial || !me || Math.hypot(view.x - me.x, view.y - me.y) <= FarAway - 1)) this.entities.delete(id);
        this.movementPending = false;
    }

    private noteSelfPose(x: number, y: number) {
        this.selfPose = [x, y];
        if (!this.inputFrom || Math.hypot(x - this.inputFrom[0], y - this.inputFrom[1]) < 0.01) return;
        this.inputToMotion.push(performance.now() - this.inputSentAt);
        if (this.inputToMotion.length > 30) this.inputToMotion.shift();
        this.inputFrom = null;
    }

    receiveEvent(e: Json) {
        const type = str(e, 'type', 'system');
        if (type === 'correction') {
            ++this.corrections;
            // A pose the server couldn't accept: back to where the wolf truly is, eased rather than jumped.
            if (str(e, 'cellId') === this.cellId && this.freePose) {
                this.freePose = {x: num(e, 'x'), y: num(e, 'y'), facing: num(e, 'facing')};
                this.easeUntil = this.clock + 0.15;
            }
            return;
        }
        if (type === 'chatAccepted') {
            this.pendingDrafts.delete(str(e, 'requestId'));
            return;
        }
        if (type === 'error' && str(e, 'context') === 'chat') {
            const requestId = str(e, 'requestId');
            const draft = this.pendingDrafts.get(requestId);
            if (draft !== undefined) {
                this.failedDraft = draft;
                this.pendingDrafts.delete(requestId);
                if (!this.composer.text) {
                    this.composer.text = this.failedDraft;
                    this.failedDraft = '';
                }
                this.showToast('Post not sent. Your writing has been preserved.');
            }
        }
        if (type === 'snapshot') {
            this.applySnapshot(e);
            return;
        }
        const eventId = str(e, 'id');
        if (eventId && this.seenPosts.has(eventId)) return;
        if (eventId) this.seenPosts.add(eventId);
        if (type === 'inspect') {
            this.inspectedCharacter = e;
            this.inspectedText = `${str(e, 'title')}\n\n${str(e, 'description', str(e, 'text'))}\n\n${str(e, 'state')}`;
            this.modal = 'inspect';
            this.facingPreview = false;
            return;
        }
        if (type === 'missions') {
            // A faction's mission board (doc 32, 4.5).
            this.missionBoard = e;
            this.modal = 'missions';
            return;
        }
        if (type === 'reputation') {
            // Their name about town (doc 32, 1.4), as the residents who know them would put it.
            this.reputation = arr(e, 'lines').filter((l): l is string => typeof l === 'string');
            return;
        }
        if (type === 'devResult') {
            // The Dev Console's answer (Core/RatwGameDev.cpp): kept in its log, and said in a toast.
            this.devLog.push({command: str(e, 'command'), ok: bool(e, 'ok'), text: str(e, 'text')});
            if (this.devLog.length > 30) this.devLog.splice(0, this.devLog.length - 30);
            if (!this.devConsole) this.showToast(str(e, 'text').split('\n')[0]);
            return;
        }
        if (type === 'devCommands') {
            this.devCommands = arr(e, 'commands')
                .filter((c): c is [string, string] => Array.isArray(c) && typeof c[0] === 'string' && typeof c[1] === 'string')
                .map(c => [c[0], c[1]] as [string, string]);
            return;
        }
        if (type === 'talkTarget') {
            this.addTarget(str(e, 'id'));
            return;
        }
        const post: Post = {
            to: arr(e, 'to').filter((v): v is string => typeof v === 'string').slice(0, 6),
            id: eventId, channel: str(e, 'channel', type === 'ooc' ? 'ooc' : 'ic'), kind: type,
            speaker: str(e, 'speaker', str(e, 'name', type === 'system' ? 'THE WORLD' : 'A voice')),
            color: Math.trunc(num(e, 'color')), text: str(e, 'text'), postedAt: this.clock, revealed: 0, system: false,
            party: bool(e, 'party'), chapter: bool(e, 'chapter'),
        };
        const segments = objects(e, 'segments');
        if (segments.length) {
            const parts = segments.map(part => {
                const body = str(part, 'text');
                return body && str(part, 'kind') === 'speech' ? `"${body}"` : body;
            }).filter(Boolean);
            post.text = parts.join(' ');
        }
        if (!post.text) return;
        // A voice too far off to make out a word of: said so once, not a row of "...".
        if (bool(e, 'anonymous') && /^"?(\.\.\.|…|\s)+"?$/.test(post.text)) {
            const last = this.posts.at(-1);
            if (last?.faint && this.clock - last.postedAt < 20) return;
            post.text = 'Words too far off to make out.';
            post.faint = true;
        }
        if (bool(e, 'muffled')) post.muffled = true;
        post.system = type === 'system' || type === 'error';
        // One's own words are already known: shown at once, not written out again.
        const own = post.speaker === str(obj(this.snapshot, 'self'), 'name') && !bool(e, 'anonymous');
        post.revealed = post.system || own || post.channel !== 'ic' ? post.text.length : 0;
        if (type === 'error') this.showToast(post.text);
        this.posts.push(post);
        if (this.posts.length > 300) this.posts.splice(0, this.posts.length - 300);
        this.transcriptScroll = 0;
        const view = this.entities.get(str(e, 'sourceId', str(e, 'entityId')));
        if (view) view.spokenAt = this.clock;
    }

    // ------------------------------------------------------------------ Time

    /** Once a frame: `time` is seconds since the page opened, `delta` since the last frame. */
    tick(time: number, delta: number) {
        this.clock = time;
        // The fight's sounds (doc 37, phase 3): played by the page's sound (ui/sound.ts), if it has one.
        for (const cue of this.fx.takeCues()) this.onCue?.(cue);
        this.motionClock += Math.max(0, delta);
        const now = this.motionClock - this.motionOffset;
        for (const view of this.entities.values()) {
            if (!view.self) {
                // Others are drawn a tenth of a second in the past, always between two poses the server sent.
                const pose = view.motion.at(now - 0.1);
                view.x = pose.x;
                view.y = pose.y;
                view.facing = pose.facing;
                continue;
            }
            if (this.walkFreely(view, delta)) continue;
            // The player's own wolf answers at once: its newest pose, carried a little ahead while it is moving, and
            // eased toward rather than jumped to, so jitter in arrival never shows. A real jump (a door, a correction
            // of more than two tiles) is taken at once.
            const moving = view.moving || this.heldKeys.size > 0;
            let pose = view.motion.at(now - 0.03, moving ? 0.12 : 0.05);
            // Held (doc 31, Phase 3): the server walks the wolf, and the page predicts it with the same rules: from the
            // server's newest pose, the keys held walked on for the time since, never through a wall.
            const predicted = this.predictHeld(view.motion, now);
            if (predicted) pose = predicted;
            const gap = Math.hypot(pose.x - view.x, pose.y - view.y);
            const ease = gap > 2 || !view.placed ? 1 : 1 - Math.exp(-delta / 0.05);
            view.x += (pose.x - view.x) * ease;
            view.y += (pose.y - view.y) * ease;
            view.facing = pose.facing;
            view.placed = true;
        }
        // A wolf out of sight is no longer spoken to.
        if (this.talkTargets.some(id => !this.entities.has(id))) this.talkTargets = this.talkTargets.filter(id => this.entities.has(id));
        const settle = Math.exp(-delta / 0.12);
        this.cameraShift = [this.cameraShift[0] * settle, this.cameraShift[1] * settle];
        if (Math.abs(this.cameraShift[0]) + Math.abs(this.cameraShift[1]) < 0.5) this.cameraShift = [0, 0];
        if (!this.canFaceAt(this.hover)) this.facingPreview = false;
        if (this.typingSent && this.clock - this.lastTyping > 3) this.setTyping(false);
        if (this.chat && this.clock - this.lastTyping < 3 && (!this.typingSent || this.clock - this.lastTypingSent > 1)) this.setTyping(true);
        if (!this.chat && this.heldKeys.size && this.clock - this.lastMove > 0.075 && !this.freeWalking()) this.sendMove();
        this.shadeTerrain();
        for (const post of this.posts)
            if (post.channel === 'ic' && post.revealed < post.text.length) {
                if (this.revealSpeed === 0 || this.reducedMotion) post.revealed = post.text.length;
                else {
                    this.revealFraction += delta * this.revealSpeed;
                    const n = Math.floor(this.revealFraction);
                    this.revealFraction -= n;
                    post.revealed = Math.min(post.text.length, post.revealed + n);
                }
                break;
            }
    }

    // ------------------------------------------------------------------ Terrain shading

    /** Works out again what the wolf sees, when it stands on a new tile or the cell, its doors or its sight changed. */
    private shadeTerrain() {
        if (!this.clientSight || !this.walker || this.battle) return;     // (In an arena, the whole arena is seen.)
        const self = obj(this.snapshot, 'self');
        const me = this.entities.get(this.selfId);
        if (!self || !me || !this.tileRows.length) return;
        const x = this.freePose ? this.freePose.x : me.x, y = this.freePose ? this.freePose.y : me.y;
        const range = num(self, 'sightRange', 0);
        const key = `${this.cellId}|${this.cellVersion}|${Math.floor(x)},${Math.floor(y)}|${range}`;
        if (key === this.sightKey) return;
        this.sightKey = key;
        this.ensureWalkerCell();
        const seen = this.walker.sight(x, y, range);
        const rows: string[] = [];
        for (let ty = 0; ty < this.cellHeight; ++ty) {
            const glyphs = this.tileRows[ty] ?? '';
            let row = '';
            for (let tx = 0; tx < this.cellWidth; ++tx) {
                const known = (glyphs[tx] ?? ' ') !== ' ';
                row += !known ? '0' : seen[ty * this.cellWidth + tx] ? '2' : '1';
            }
            rows.push(row);
        }
        this.visibilityRows = rows;
    }

    // ------------------------------------------------------------------ Walking freely

    /** The page walks its own wolf: the server allows it, the walker is here, and the wolf is on its feet. */
    freeWalking(): boolean {
        if (!this.walker || !this.walkAsked || this.movementMode !== 0) return false;
        const posture = str(obj(this.snapshot, 'self'), 'posture', 'standing');
        return posture === 'standing' || posture === 'crouching';
    }

    /** One frame of free walking for the player's own wolf: false when the server walks it instead. */
    private walkFreely(view: {x: number; y: number; facing: number; moving: boolean; placed?: boolean; motion: MotionBuffer}, delta: number): boolean {
        if (!this.freeWalking() || !this.walker) return false;
        if (!this.freePose) {
            const newest = view.motion.samples.at(-1);
            if (!newest) return false;
            this.freePose = {x: newest.x, y: newest.y, facing: newest.facing};
        }
        const held = (code: string) => (this.heldKeys.has(code) ? 1 : 0);
        const ix = this.chat ? 0 : held('KeyD') - held('KeyA'), iy = this.chat ? 0 : held('KeyS') - held('KeyW');
        // Not walking, and every pose of its own answered: where the server has the wolf is where it is (the server
        // may have walked it: a route, a script's keys, a push).
        if (!ix && !iy && !this.poseUnsent && this.poseAckSeen >= this.poseSeq) {
            const newest = view.motion.samples.at(-1);
            if (newest && Math.hypot(newest.x - this.freePose.x, newest.y - this.freePose.y) > 1e-3) {
                this.freePose = {x: newest.x, y: newest.y, facing: newest.facing};
                this.easeUntil = this.clock + 0.15;
            }
        }
        const pose = this.freePose;
        if (ix || iy) {
            this.ensureWalkerCell();
            if (!this.walking) {
                this.walking = true;
                this.inputToMotion.push(0);        // Walked at once, on this very frame.
                if (this.inputToMotion.length > 30) this.inputToMotion.shift();
                this.facingPreview = false;
                this.mapPan = [0, 0];
            }
            const self = obj(this.snapshot, 'self');
            const step = this.walker.step(pose.x, pose.y, ix, iy, num(self, 'walkSpeed', 2.6), str(self, 'posture') === 'crouching',
                num(self, 'moveFactor', 1), Math.min(Math.max(delta, 0), 0.1));
            pose.x = step.x;
            pose.y = step.y;
            pose.facing = Math.atan2(iy, ix);
            this.poseUnsent = true;
        } else if (this.walking) {
            this.walking = false;
            this.poseUnsent = true;                // The last pose, where it stopped.
        }
        if (this.nudgeByBodies(pose)) this.poseUnsent = true;
        // Twenty poses a second while walking (also while pushing into a door: the heading takes it through).
        if (this.poseUnsent && this.clock - this.lastPoseSent >= 0.05) {
            ++this.poseSeq;
            if (this.poseSender) this.poseSender(this.poseSeq, pose.x, pose.y, pose.facing, ix, iy);
            else this.send({type: 'pose', seq: this.poseSeq, x: pose.x, y: pose.y, facing: pose.facing, ix, iy});
            this.lastPoseSent = this.clock;
            this.poseUnsent = !!(ix || iy);
            this.lastMove = this.clock;
        }
        const ease = this.clock < this.easeUntil ? 1 - Math.exp(-delta / 0.04) : 1;
        view.x += (pose.x - view.x) * ease;
        view.y += (pose.y - view.y) * ease;
        view.facing = pose.facing;
        view.moving = !!(ix || iy);
        view.placed = true;
        return true;
    }

    /**
     * Bodies (Core/RatwStep.h, BodyRadius): the page's own wolf never stands on another. Pressed against one, it gives
     * way half the overlap, so walking into someone is slowed by them; the server, which leaves a page-walked wolf to
     * its page, pushes the other the rest. True if it moved.
     */
    private nudgeByBodies(pose: {x: number; y: number}): boolean {
        if (!this.walker) return false;
        let moved = false;
        for (const [id, other] of this.entities) {
            if (id === this.selfId || other.self) continue;
            const dx = pose.x - other.x, dy = pose.y - other.y, d = Math.hypot(dx, dy);
            if (d >= BodyWidth) continue;
            // On the very same spot: apart along a direction fixed by who it is.
            const nx = d > 1e-9 ? dx / d : Math.cos(id.length), ny = d > 1e-9 ? dy / d : Math.sin(id.length);
            const push = (BodyWidth - d) / 2;
            const x = pose.x + nx * push, y = pose.y + ny * push;
            if (!this.walker.passable(x, y, pose.x, pose.y)) continue;   // Not into a wall: the other gives way instead.
            pose.x = x;
            pose.y = y;
            moved = true;
        }
        return moved;
    }

    /** Where the server will have the wolf, held: its newest pose walked on by the keys held (null when not held). */
    private predictHeld(motion: MotionBuffer, now: number): {time: number; x: number; y: number; facing: number} | null {
        if (!this.walker || this.movementMode !== 1 || this.chat) return null;
        const held = (code: string) => (this.heldKeys.has(code) ? 1 : 0);
        const ix = held('KeyD') - held('KeyA'), iy = held('KeyS') - held('KeyW');
        const newest = motion.samples.at(-1);
        if ((!ix && !iy) || !newest) return null;
        const self = obj(this.snapshot, 'self');
        const posture = str(self, 'posture', 'standing');
        if (posture !== 'standing' && posture !== 'crouching') return null;
        this.ensureWalkerCell();
        const ahead = clamp(now - newest.time, 0, 0.25);
        const step = this.walker.step(newest.x, newest.y, ix, iy, num(self, 'walkSpeed', 2.6), posture === 'crouching', num(self, 'moveFactor', 1), ahead);
        return {time: now, x: step.x, y: step.y, facing: Math.atan2(iy, ix)};
    }

    // ------------------------------------------------------------------ Sending

    /** The walker's copy of the cell, again when the cell or the Chapters' built structures in it change (doc 32, 5.7). */
    private ensureWalkerCell() {
        if (!this.walker) return;
        const blocked = objects(this.snapshot, 'structures').filter(st => bool(st, 'blocks')).map(st => `${num(st, 'x')},${num(st, 'y')}`);
        const key = blocked.join(';');
        if (this.walkerVersion === this.cellVersion && this.walkerBlocked === key) return;
        this.walker.setCell(this.cellWidth, this.cellHeight, this.tileRows, this.tileHeights, this.doorStates, new Set(blocked));
        this.walkerVersion = this.cellVersion;
        this.walkerBlocked = key;
    }

    /** A faction command (doc 32, Part 4): taking a mission. */
    sendFaction(fields: Json) {
        this.send({type: 'faction', ...fields});
    }

    /** In a Chapter (doc 32, Part 3). */
    inChapter(): boolean {
        return !!str(obj(obj(this.snapshot, 'self'), 'chapter'), 'id');
    }

    /** A Chapter command (doc 32, Part 3): found, agree, invite, ranks, the treasury, the hostile list... */
    sendChapter(fields: Json) {
        this.send({type: 'chapter', ...fields});
    }

    /** The individual social game (doc 32, Part 1): stars, Stories, notes, a name about town. */
    sendSocial(fields: Json) {
        this.send({type: 'social', ...fields});
        if (fields.verb === 'note' && this.inspectedCharacter && this.inspectedCharacter.id === fields.target)
            this.inspectedCharacter = {...this.inspectedCharacter, note: fields.text};
    }

    sendAction(action: string, target = '', extra: Json = {}) {
        // An introduction is said aloud, so whoever hears it learns the name (doc 32): "I'm Kestrel."
        if (action === 'introduce' || action.startsWith('introduce as ')) {
            const name = action === 'introduce' ? str(obj(obj(this.snapshot, 'self'), 'names'), 'name') : action.slice('introduce as '.length);
            if (!name) return;
            this.send({type: 'chat', requestId: `post_${++this.nextRequestId}`, text: `"I'm ${name}."`, channel: 'ic', volume: 'speak',
                ...(target ? {targets: [target]} : {})});
            return;
        }
        this.send({type: 'action', action, target, ...extra});
    }

    showToast(text: string) {
        this.toast = text;
        this.toastUntil = this.clock + 5;
    }

    setTyping(active: boolean) {
        active = active && (this.channel === 'ic' || this.channel === 'party' || this.channel === 'chapter');
        if (this.typingSent === active && (!active || this.clock - this.lastTypingSent < 1)) return;
        this.typingSent = active;
        this.lastTypingSent = this.clock;
        this.send({type: 'typing', active});
    }

    setChat(active: boolean) {
        this.chat = active;
        this.facingPreview = false;
        this.heldKeys.clear();
        this.sendMove();
        this.contextTarget = '';
        if (active) {
            this.modal = '';
            this.composer.focus();
        } else {
            this.setTyping(false);
            this.composer.blur();
        }
    }

    /** Marked a Dungeon Master by the Dungeon Master app: the Dev Console is theirs. */
    isDungeonMaster() {
        return bool(obj(this.snapshot, 'self'), 'dungeonMaster');
    }

    /** A Dev Console command ("/fight-test-1"), for the server to answer with a devResult. */
    runDevCommand(command: string) {
        const text = command.trim();
        if (!text || !this.isDungeonMaster()) return;
        this.send({type: 'dev', command: text.startsWith('/') ? text : `/${text}`});
    }

    /** Opens or closes the Dev Console (only ever for a Dungeon Master); the first time, asks the server what it offers. */
    toggleDevConsole(open = !this.devConsole) {
        this.devConsole = open && this.isDungeonMaster();
        if (this.devConsole) {
            this.modal = '';
            this.setChat(false);
            if (!this.devCommandsAsked || !this.devCommands.length) {
                this.devCommandsAsked = true;
                this.send({type: 'devCommands'});
            }
        }
    }

    /** The commands that begin with what is typed (a leading "/" or not), alphabetically: all of them for nothing typed. */
    devSuggestions(typed: string): [string, string][] {
        const [word = '', ...rest] = typed.trim().toLowerCase().replace(/^\/+/, '').split(/\s+/);
        return this.devCommands
            .filter(([name]) => (rest.length ? name.slice(1).toLowerCase() === word : name.slice(1).toLowerCase().startsWith(word)))
            .sort(([x], [y]) => x.localeCompare(y));
    }

    submitPost() {
        const text = this.composer.text.trim();
        if (text.startsWith('/') && this.isDungeonMaster()) {
            // A Dungeon Master's slash command goes to the Dev Console, never into the world as words.
            this.composer.text = '';
            this.runDevCommand(text);
            this.setChat(false);
            this.toggleDevConsole(true);          // Where the answer is.
            return;
        }
        if (text) {
            const id = `post_${++this.nextRequestId}`;
            this.pendingDrafts.set(id, text);
            this.composer.text = '';
            this.send({type: 'chat', requestId: id, text, channel: this.channel, volume: this.volume,
                ...(this.channel === 'ic' && this.talkTargets.length ? {targets: [...this.talkTargets]} : {})});
        }
        this.setChat(false);
    }

    composerChanged() {
        if (!this.chat) return;
        this.lastTyping = this.clock;
        if (this.channel === 'ic' || this.channel === 'party' || this.channel === 'chapter') this.setTyping(true);
    }

    /** A key in the composer; true when the game took it. */
    composerKey(k: KeyInput): boolean {
        if (k.code === 'Escape') {
            this.setChat(false);
            return true;
        }
        const enter = k.code === 'Enter' || k.code === 'NumpadEnter';
        if (enter && k.shift && this.chat) {
            this.composer.insertNewline();
            return true;
        }
        if (enter && !k.shift) {
            if (this.chat) this.submitPost();
            else this.setChat(true);
            return true;
        }
        return false;
    }

    sendMove() {
        const held = (code: string) => (this.heldKeys.has(code) ? 1 : 0);
        const x = held('KeyD') - held('KeyA'), y = held('KeyS') - held('KeyW');
        const travel = obj(this.snapshot, 'travel');
        // Reading, writing and focus changes release WASD, but do not reset an explicit overland route.
        if (x === 0 && y === 0 && (bool(travel, 'active') || bool(travel, 'paused'))) return;
        const walking = !this.chat && (x !== 0 || y !== 0);
        if (walking && !this.walking && this.selfPose) {
            this.inputSentAt = performance.now();
            this.inputFrom = this.selfPose;
        }
        this.walking = walking;
        if (!this.chat && (x !== 0 || y !== 0)) {
            this.movementPending = true;
            this.facingPreview = false;
            this.mapPan = [0, 0];                // Looking around ends when the wolf moves: the map follows it again.
        }
        this.send({type: 'move', x: this.chat ? 0 : x, y: this.chat ? 0 : y, seq: ++this.inputSeq});
        this.lastMove = this.clock;
    }

    displayPace(): number {
        if (this.requestedPace >= 0 && this.clock - this.lastPaceRequest <= 1.5) return this.requestedPace;
        return Math.trunc(boundedNum(obj(this.snapshot, 'self'), 'pace', 0, 10));
    }

    requestPace(pace: number) {
        if (this.chat || this.modal || !this.navigationFocus || !obj(this.snapshot, 'self')) return;
        pace = clamp(pace, 0, 10);
        if (pace === this.displayPace()) return;
        // Rapid wheel and key repeats count from the latest request, not a lagging snapshot.
        this.requestedPace = pace;
        this.lastPaceRequest = this.clock;
        this.send({type: 'pace', pace});
    }

    canTravelTo(id: string): boolean {
        if (!id || id === this.cellId) return false;
        return objects(this.snapshot, 'travelMap').some(c => str(c, 'id') === id && str(c, 'knowledge') === 'visited');
    }

    cancelTravel() {
        const travel = obj(this.snapshot, 'travel');
        if (!bool(travel, 'active') && !bool(travel, 'paused')) return;
        this.send({type: 'cancel_travel'});
    }

    canFaceAt(point: [number, number]): boolean {
        if (this.chat || this.worldMap || this.modal || !this.navigationFocus || this.movementPending || this.heldKeys.size ||
            !contains(this.mapRect, point[0], point[1]) || this.tileSize <= 0) return false;
        const wx = (point[0] - this.mapOrigin[0]) / this.tileSize, wy = (point[1] - this.mapOrigin[1]) / this.tileSize;
        if (wx < 0 || wy < 0 || wx >= this.cellWidth || wy >= this.cellHeight) return false;
        const self = this.entities.get(this.selfId);
        return !!self && !self.moving && (wx - self.x) ** 2 + (wy - self.y) ** 2 > 0.0001;
    }

    updateFacingPreview(point: [number, number], alt: boolean) {
        this.facingPreview = alt && this.canFaceAt(point);
        if (this.facingPreview) {
            const self = this.entities.get(this.selfId)!;
            this.previewFacing = Math.atan2((point[1] - this.mapOrigin[1]) / this.tileSize - self.y,
                (point[0] - this.mapOrigin[0]) / this.tileSize - self.x);
        }
    }

    sendFacing(point: [number, number]) {
        this.send({type: 'face', x: (point[0] - this.mapOrigin[0]) / this.tileSize, y: (point[1] - this.mapOrigin[1]) / this.tileSize});
        this.contextTarget = '';
    }

    // ------------------------------------------------------------------ Input

    /** A key pressed on the map; true when the game took it (the page then keeps it from the browser). */
    keyDown(k: KeyInput): boolean {
        const code = k.code;
        if (code === 'Escape' && this.aiming) {
            this.aiming = '';
            return true;
        }
        if (code === 'Escape') {
            this.facingPreview = false;
            if (this.devConsole) {
                this.toggleDevConsole(false);
                return true;
            }
            if (this.modal) {
                this.modal = '';
                return true;
            }
            if (this.contextTarget) {
                this.contextTarget = '';
                return true;
            }
            if (this.chat) this.setChat(false);
            else if (this.talkTargets.length) this.talkTargets = [];      // Esc on the map lets go of whom you speak to.
            else this.cancelTravel();
            return true;
        }
        if (code === 'Enter' || code === 'NumpadEnter') {
            this.setChat(true);
            return true;
        }
        if (code === 'Backquote' && this.isDungeonMaster() && !this.chat) {
            this.toggleDevConsole();                                         // The Dev Console (a Dungeon Master's).
            return true;
        }
        if (this.modal) return true;
        if (this.chat) return false;
        if (code === 'PageUp' || code === 'PageDown') {
            this.requestPace(this.displayPace() + (code === 'PageUp' ? 1 : -1));
            return true;
        }
        if (code === 'AltLeft' || code === 'AltRight') {
            this.updateFacingPreview(this.hover, true);
            return true;
        }
        if (this.contextTarget) {
            const choice = /^(?:Digit|Numpad)([1-6])$/.exec(code);
            if (choice && this.contextActions[+choice[1] - 1] !== undefined) {
                const picked = this.contextActions[+choice[1] - 1];
                if (picked === 'challenge' || picked.startsWith('challenge:')) {
                    this.activate({rect: rect(0, 0, 0, 0), action: 'context', target: picked});
                    return true;
                }
                if (this.askFirst(picked)) return true;
                this.sendAction(this.contextActions[+choice[1] - 1], this.contextTarget);
                this.contextTarget = '';
                return true;
            }
        }
        // The fight screen's keys (doc 37): 1–8 its actions, Space to end the turn (ui/hud/combat.ts).
        if (this.battle && this.fightKeys?.(code)) return true;
        if (this.battle && (code === 'KeyQ' || code === 'KeyE')) {
            this.turnInFight(code === 'KeyQ' ? -1 : 1);                   // Q / E turn, on one's own turn.
            return true;
        }
        if (code === 'KeyE') {
            this.targetNearest();
            return true;
        }
        if (this.battle && MovementKeys.includes(code)) {
            // No walking in a fight: moving is by clicking a lit tile, on one's turn.
            if (this.clock - this.lastWalkHint > 4) {
                this.lastWalkHint = this.clock;
                this.showToast('In a fight, click a lit tile to move (on your turn). Use the arrows around your wolf to face.');
            }
            return true;
        }
        if (MovementKeys.includes(code)) {
            this.heldKeys.add(code);
            this.sendMove();
            return true;
        }
        if (code === 'KeyM') {
            this.facingPreview = false;
            this.worldMap = !this.worldMap;
            this.contextTarget = '';
            return true;
        }
        if (code === 'KeyI' || code === 'KeyC') {
            const page = code === 'KeyI' ? 'inventory' : 'character';
            this.facingPreview = false;
            this.modal = this.modal === page ? '' : page;
            return true;
        }
        if (code === 'KeyL') {
            this.sendAction('listen');
            return true;
        }
        if (code === 'Equal' || code === 'NumpadAdd' || code === 'Minus' || code === 'NumpadSubtract') {
            this.activate({rect: rect(0, 0, 0, 0), action: 'zoom', target: code === 'Equal' || code === 'NumpadAdd' ? 'in' : 'out'});
            return true;
        }
        return false;
    }

    keyUp(k: KeyInput): boolean {
        if (k.code === 'AltLeft' || k.code === 'AltRight') {
            this.facingPreview = false;
            return true;             // Also keeps a browser from opening its menu bar.
        }
        if (this.heldKeys.delete(k.code)) {
            this.sendMove();
            return true;
        }
        return false;
    }

    receiveArtwork(e: Json) {
        const type = str(e, 'type');
        if (type === 'artworkError') this.showToast(str(e, 'text'));
        else if (type === 'artworkUploaded') this.showToast(str(e, 'text'));
        else if (type === 'artworkReported') this.showToast('Reported. A Dungeon Master will look at it again.');
    }

    reportPortrait(id: string) {
        if (id) this.send({type: 'artwork_report', id, reason: 'reported from a closer look'});
    }

    /** A row of the In Sight list clicked: a resident is chosen (or let go) to speak to; anyone else, their menu. */
    sightClicked(id: string, page: [number, number]) {
        const e = this.entities.get(id);
        if (e && e.kind === 'npc' && e.actions.includes('talk')) this.toggleTarget(id);
        else this.openContextAt(id, page);
    }

    toggleTarget(id: string) {
        if (this.talkTargets.includes(id)) this.talkTargets = this.talkTargets.filter(t => t !== id);
        else this.addTarget(id);
    }

    addTarget(id: string) {
        const e = this.entities.get(id);
        if (!e || e.self || this.talkTargets.includes(id)) return;
        if (this.talkTargets.length >= MaxTargets) {
            this.showToast(`You can speak to ${MaxTargets} at once. Let one go first.`);
            return;
        }
        this.talkTargets = [...this.talkTargets, id];
    }

    /** Who will hear the next words as meant for them, for the hint above the composer. */
    speakingTo(): {targets: EntityView[]; nearby: EntityView | null} {
        const targets = this.talkTargets.map(id => this.entities.get(id)).filter((e): e is EntityView => !!e);
        if (targets.length) return {targets, nearby: null};
        // As the server decides: with no one chosen or named, the one resident close by, if there is only one.
        const me = this.entities.get(this.selfId);
        const near = me ? [...this.entities.values()].filter(e => e.kind === 'npc' && !e.self && Math.hypot(e.x - me.x, e.y - me.y) <= 6) : [];
        return {targets, nearby: near.length === 1 ? near[0] : null};
    }

    /** A wolf's menu, opened from a panel (the In Sight list) at a point on the page. */
    openContextAt(id: string, page: [number, number]) {
        const e = this.entities.get(id);
        if (!e || e.self) return;
        this.contextTarget = id;
        this.contextName = e.name;
        this.contextKind = e.kind;
        this.contextActions = e.actions;
        this.contextPage = page;
    }

    /**
     * Attacking a resident is a crime that brings the watch: the first choice of Attack says so and keeps the menu
     * open; choosing it again goes for them. (Bandits and other foes are fought without asking.)
     */
    private askFirst(action: string): boolean {
        const key = `${action}|${this.contextTarget}`;
        const e = this.entities.get(this.contextTarget);
        if (action !== 'attack' || !e || e.kind !== 'npc' || e.hostile || this.armed === key) {
            this.armed = '';
            return false;
        }
        this.armed = key;
        this.showToast('Attacking a resident is a crime: whoever sees it tells the watch, and a guard on duty nearby joins in. Choose Attack again to go for them.');
        return true;
    }

    /** E: the nearest wolf, door or thing, as if clicked. */
    private targetNearest() {
        let sx = 0, sy = 0;
        for (const v of this.entities.values()) if (v.self) [sx, sy] = [v.x, v.y];
        let best = 1e9, target = '', tx = 0, ty = 0;
        for (const [id, v] of this.entities)
            if (!v.self) {
                const d = Math.hypot(v.x - sx, v.y - sy);
                if (d < best) [best, target, tx, ty] = [d, id, v.x, v.y];
            }
        for (const door of objects(this.snapshot, 'doors')) {
            const dx = num(door, 'x'), dy = num(door, 'y');
            const d = Math.hypot(dx - sx, dy - sy);
            if (d < best) [best, target, tx, ty] = [d, str(door, 'id'), dx, dy];
        }
        if (!target) return;
        const px = this.mapOrigin[0] + tx * this.tileSize, py = this.mapOrigin[1] + ty * this.tileSize;
        this.activate({rect: rect(px - 14, py - 14, px + 14, py + 14), action: 'target', target});
    }

    focusGained() {
        this.navigationFocus = true;
        this.facingPreview = false;
    }

    /** The page lost focus (another window, another tab, the composer): nothing stays held. */
    focusLost() {
        this.heldKeys.clear();
        this.navigationFocus = false;
        this.facingPreview = false;
        this.sendMove();
    }

    mouseMove(point: [number, number], alt: boolean) {
        this.hover = point;
        this.updateFacingPreview(point, alt);
        if (this.faceDrag) {
            const c = this.ownTokenCentre();
            this.faceDragDir = c && Math.hypot(point[0] - c[0], point[1] - c[1]) > this.tileSize * 0.55
                ? octant(point[0] - c[0], point[1] - c[1]) : -1;
        }
    }

    /** One's own wolf on the screen, in a fight, on one's own turn (for turning it by dragging). */
    private ownTokenCentre(): [number, number] | null {
        const b = this.battle, me = b?.fighters.find(f => f.id === this.selfId);
        if (!b || !me || !myTurn(b, this.selfId) || me.status !== 'fighting' || this.aiming) return null;
        return [this.mapOrigin[0] + (me.x + 0.5) * this.tileSize, this.mapOrigin[1] + (me.y + 0.5) * this.tileSize];
    }

    /** The button let go: a drag from one's wolf turns it that way; a plain click on its tile is a click as ever. */
    mouseUp() {
        const start = this.faceDrag;
        if (!start) return;
        const dir = this.faceDragDir;
        this.faceDrag = null;
        this.faceDragDir = -1;
        if (dir >= 0) {
            this.sendBattle('face', {dir});
            return;
        }
        for (let i = this.hits.length - 1; i >= 0; --i)
            if (contains(this.hits[i].rect, start[0], start[1])) {
                this.activate(this.hits[i]);
                return;
            }
    }

    mouseLeave() {
        this.facingPreview = false;
        this.hover = [-1000, -1000];
    }

    /** The wheel: positive is up (away from the player). True when the game took it. */
    wheel(point: [number, number], delta: number, shift: boolean, ctrl: boolean): boolean {
        this.facingPreview = false;
        if (this.modal) return false;
        // In a fight the wheel sets pace even while writing: it is how far the next move goes (doc 33).
        if (contains(this.mapRect, point[0], point[1]) && !this.worldMap && (!this.chat || this.battle)) {
            if (ctrl) this.mapPan[0] = clamp(this.mapPan[0] + delta * 60, -1000, 1000);
            else if (shift) this.mapPan[1] = clamp(this.mapPan[1] + delta * 60, -1000, 1000);
            else if (delta) this.requestPace(this.displayPace() + (delta > 0 ? 1 : -1));
            return true;
        }
        if (contains(this.mapRect, point[0], point[1]) && this.worldMap && !this.travelAtlas && !this.chat) {
            if (this.worldZoom < 0) this.worldZoom = 2;
            const step = 200 / (MapScalesForPan[this.worldZoom] ?? 1);
            if (ctrl) this.worldPan[0] = clamp(this.worldPan[0] + delta * step, -3000, 3000);
            else if (shift) this.worldPan[1] = clamp(this.worldPan[1] + delta * step, -3000, 3000);
            else if (delta) this.worldZoom = clamp(this.worldZoom + (delta > 0 ? 1 : -1), 0, MapScalesForPan.length - 1);
            return true;
        }
        if (contains(this.mapRect, point[0], point[1]) && this.worldMap && this.travelAtlas && !this.chat) {
            this.travelPage = clamp(this.travelPage + (delta > 0 ? -1 : 1), 0, this.lastTravelPage());
            return true;
        }
        return false;
    }

    lastTravelPage(): number {
        return Math.max(0, Math.floor((Math.min(256, arr(this.snapshot, 'travelMap').length) - 1) / 8));
    }

    /** A click. Returns where keyboard focus should go: the composer or the map. */
    mouseDown(point: [number, number], left: boolean, alt: boolean, ctrl: boolean): 'composer' | 'map' {
        // Modifier clicks own map input, entities and doors included. An unavailable facing action must never fall
        // through into pathing, inspection or an action menu.
        // In a fight, pressing on one's own wolf on one's turn begins a drag to turn it (let go: mouseUp).
        const own = left && !alt && !ctrl ? this.ownTokenCentre() : null;
        if (own && Math.hypot(point[0] - own[0], point[1] - own[1]) <= this.tileSize * 0.5) {
            this.faceDrag = point;
            this.faceDragDir = -1;
            return 'map';
        }
        if ((alt || ctrl) && this.battle && contains(this.mapRect, point[0], point[1])) {
            // In a fight, Alt/Ctrl+click turns to face that tile (free, on one's turn).
            this.arenaFace(Math.floor((point[0] - this.mapOrigin[0]) / this.tileSize), Math.floor((point[1] - this.mapOrigin[1]) / this.tileSize));
            return 'map';
        }
        if ((alt || ctrl) && contains(this.mapRect, point[0], point[1])) {
            if (left && this.canFaceAt(point)) {
                this.sendFacing(point);
                this.updateFacingPreview(point, alt);
            } else this.facingPreview = false;
            return 'map';
        }
        for (let i = this.hits.length - 1; i >= 0; --i)
            if (contains(this.hits[i].rect, point[0], point[1])) {
                this.activate(this.hits[i]);
                return this.chat && !this.modal ? 'composer' : 'map';
            }
        if (this.modal) return 'map';
        if (!this.worldMap && contains(this.mapRect, point[0], point[1])) {
            this.facingPreview = false;
            this.contextTarget = '';
            if (this.chat) this.setChat(false);
            const wx = (point[0] - this.mapOrigin[0]) / this.tileSize, wy = (point[1] - this.mapOrigin[1]) / this.tileSize;
            if (wx < 0 || wy < 0 || wx >= this.cellWidth || wy >= this.cellHeight) return 'map';
            if (this.battle) {
                this.arenaClick(Math.floor(wx), Math.floor(wy));
                return 'map';
            }
            this.movementPending = true;
            this.send({type: 'path', x: wx, y: wy});
            return 'map';
        }
        this.contextTarget = '';
        return 'map';
    }

    activate(h: Hit) {
        this.facingPreview = false;
        const a = h.action;
        if (a === 'leave_character') this.modal = 'leave_character';
        else if (a === 'leave_confirm') this.leaveCharacter();
        else if (a === 'leave_cancel') this.modal = 'character';
        else if (a === 'local') {
            this.worldMap = false;
            this.contextTarget = '';
        } else if (a === 'world') {
            this.worldMap = true;
            this.contextTarget = '';
        } else if (a === 'nearby' || a === 'atlas') {
            this.worldMap = true;
            this.travelAtlas = a === 'atlas';
            this.contextTarget = '';
        } else if (a === 'pace') {
            if (/^\d+$/.test(h.target)) this.requestPace(+h.target);
        } else if (a === 'travel_page') {
            this.travelPage = clamp(this.travelPage + clamp(Math.trunc(+h.target || 0), -1, 1), 0, this.lastTravelPage());
        } else if (a === 'travel') {
            if (this.chat || this.modal || !this.worldMap || !this.travelAtlas || !this.canTravelTo(h.target)) return;
            this.heldKeys.clear();
            this.send({type: 'travel', target: h.target});
            this.showToast('Finding a route through places you have visited…');
        } else if (a === 'cancel_travel') {
            if (!this.chat && !this.modal) this.cancelTravel();
        } else if (a === 'ic' || a === 'ooc' || ((a === 'party' || a === 'partyooc') && inParty(this.party)) ||
            ((a === 'chapter' || a === 'chapterooc') && this.inChapter())) {
            this.setTyping(false);
            this.channel = a;
            this.transcriptScroll = 0;
        } else if (a === 'name_add' || a === 'name_retire') {
            // Aliases (doc 32): names this wolf also goes by.
            const name = h.target.trim();
            if (name) this.send({type: 'names', verb: a === 'name_add' ? 'add' : 'retire', name});
        } else if (a === 'party_verb') {
            // The party's commands (doc 32): "accept", "decline", "leave", "disband", "stayout", "autojoin:on|off",
            // "remove:<id>", "lead:<id>".
            const [verb, rest] = h.target.split(':', 2);
            if (verb === 'autojoin') this.send({type: 'party', verb, on: rest !== 'off'});
            else if (verb === 'goal') this.send({type: 'party', verb, goal: h.target.slice('goal:'.length).trim()});
            else if (['accept', 'decline', 'leave', 'disband', 'stayout', 'remove', 'lead'].includes(verb))
                this.send({type: 'party', verb, ...(rest ? {target: rest} : {})});
        } else if (a === 'character' || a === 'inventory' || a === 'settings' || a === 'chapter_window' || a === 'status' ||
            ((a === 'their_equipment' || a === 'back_to_inspect') && this.inspectedCharacter)) {
            if (this.chat) this.setChat(false);
            this.heldKeys.clear();
            this.sendMove();
            this.modal = a === 'back_to_inspect' ? 'inspect' : a;
            this.contextTarget = '';
        } else if (a === 'close') this.modal = '';
        else if (a === 'dev_console') this.toggleDevConsole();
        else if (a === 'volume') this.volume = this.volume === 'speak' ? 'whisper' : this.volume === 'whisper' ? 'yell' : 'speak';
        else if (a === 'send') {
            if (this.chat) this.submitPost();
            else this.setChat(true);
        } else if (a === 'recover') {
            const draft = this.composer.text;
            this.composer.text = draft + (draft ? '\n\n' : '') + this.failedDraft;
            this.failedDraft = '';
            this.setChat(true);
        } else if (a === 'speed') this.revealSpeed = this.revealSpeed === 64 ? 120 : this.revealSpeed === 120 ? 0 : 64;
        else if (a === 'motion') this.reducedMotion = !this.reducedMotion;
        else if (a === 'projection') this.flatWorld = !this.flatWorld;
        else if (a === 'glyphs') this.plainGlyphs = !this.plainGlyphs;
        else if (a === 'tooltips') this.hoverTooltips = !this.hoverTooltips;
        else if (a === 'sound') {
            const steps = [0, 0.3, 0.6, 1];
            this.soundVolume = steps[(steps.findIndex(v => Math.abs(v - this.soundVolume) < 0.05) + 1) % steps.length];
            try {
                localStorage.setItem('ratw.sound', String(this.soundVolume));
            } catch { /* No storage here: for this visit only. */ }
            if (this.soundVolume > 0) this.onCue?.('turn');            // A sample at the new level.
        }
        else if (a === 'perf') {
            this.perfOverlay = !this.perfOverlay;
            try {
                localStorage.setItem('ratw.perfOverlay', this.perfOverlay ? '1' : '');
            } catch { /* No storage here: for this visit only. */ }
        }
        else if (a === 'split') {
            const presets = StoryWidths, at = presets.indexOf(this.storyWidth);
            this.storyWidth = presets[(at + 1) % presets.length];
            this.mapPan = [0, 0];
            this.contextTarget = '';
        } else if (a === 'color') {
            this.selectedColor = Math.trunc(+h.target || 0);
            this.send({type: 'color', index: this.selectedColor});
        } else if (a === 'trade_open') this.openTrade(h.target);
        else if (a === 'trade_buy' || a === 'trade_sell') {
            const buy = a === 'trade_buy';
            if (this.modal !== 'trade' || !this.canTradeItem(h.target, buy)) {
                this.showToast('That offer is no longer available. Check the current stock and purses.');
                return;
            }
            this.send({type: 'trade', target: str(obj(this.snapshot, 'merchant'), 'id'), item: h.target, quantity: 1, buy});
        } else if (a === 'eat' || a === 'gather') {
            if ((a === 'eat' && this.inventoryQuantity('meal') <= 0) || (a === 'gather' && !this.canGather())) {
                this.showToast(a === 'eat' ? 'You have no prepared meal to eat.' : 'Approach a visible herb patch with bundles remaining.');
                return;
            }
            this.send({type: a});
        } else if (a === 'zoom') {
            this.zoom = clamp(this.zoom + (h.target === 'in' ? 1 : -1), 0, 3);
            this.mapPan = [0, 0];
        } else if (a === 'target') {
            this.contextTarget = h.target;
            this.contextPage = null;
            this.contextPoint = [Math.max(this.mapRect.left, Math.min(this.mapRect.right - 200, h.rect.right + 12)),
                Math.max(this.mapRect.top, Math.min(this.mapRect.bottom - 260, h.rect.top))];
            this.contextName = h.target;
            this.contextActions = ['inspect'];
            this.contextKind = 'object';
            const e = this.entities.get(h.target);
            if (e) {
                this.contextName = e.name;
                this.contextKind = e.kind;
                this.contextActions = e.actions;
            }
            for (const door of objects(this.snapshot, 'doors'))
                if (str(door, 'id') === h.target) {
                    this.contextName = str(door, 'name');
                    this.contextKind = 'door';
                    this.contextActions = arr(door, 'actions').filter((v): v is string => typeof v === 'string');
                    if (!this.contextActions.length) this.contextActions = ['inspect', 'open', 'knock'];
                    break;
                }
            const resource = this.visibleResource();
            if (resource && str(resource, 'id') === h.target) {
                this.contextName = 'Cooking herbs';
                this.contextKind = 'resource';
                this.contextActions = ['gather'];
            }
        } else if (a === 'fighter') {
            if (this.aiming === 'flame') {
                const f = this.battle?.fighters.find(o => o.id === h.target);
                if (f) this.arenaClick(f.x, f.y);
            } else this.fightTarget(h.target);
        } else if (a === 'face') {
            this.sendBattle('face', {dir: Number(h.target)});
        } else if (a === 'ground') {
            this.sendAction('take', h.target);
        } else if (a === 'context') {
            if (this.askFirst(h.target)) return;
            // A challenge names its terms (doc 37): the menu offers them, the default first.
            if (h.target === 'challenge') {
                this.contextActions = ['challenge:yield', 'challenge:blood', 'challenge:death'];
                return;
            }
            if (h.target.startsWith('challenge:')) {
                this.sendAction('challenge', this.contextTarget, {terms: h.target.slice('challenge:'.length)});
                this.contextTarget = '';
                return;
            }
            if (h.target === 'trade') this.openTrade(this.contextTarget);
            else if (h.target === 'gather') this.activate({rect: rect(0, 0, 0, 0), action: 'gather', target: ''});
            else this.sendAction(h.target, this.contextTarget);
            this.contextTarget = '';
        } else if (['weather', 'wind', 'time', 'lighting', 'calendar', 'front'].includes(a)) {
            if (!bool(this.snapshot, 'devTools')) return;
            if (a === 'calendar' && h.target !== 'day' && h.target !== 'year') return;
            this.send({type: a, value: h.target});
        } else this.sendAction(a, h.target);
    }

    leaveCharacter() {
        this.setTyping(false);
        this.heldKeys.clear();
        this.sendMove();
        this.send({type: 'character_leave'});
        this.showToast('Returning to character selection…');
    }

    /** Pages the tools and screenshots ask for (the Unreal client's SetPresentationPage). */
    setPresentationPage(page: string) {
        this.facingPreview = false;
        if (page === 'text-first') this.storyWidth = 760;
        if (page === 'balanced') this.storyWidth = 460;
        this.travelAtlas = page === 'travel';
        this.worldMap = page === 'world' || this.travelAtlas;
        this.modal = ['character', 'inventory', 'settings', 'trade'].includes(page) ? page : '';
        this.contextTarget = '';
    }

    // ------------------------------------------------------------------ What the snapshot allows

    inventoryQuantity(id: string): number {
        const item = objects(this.snapshot, 'inventory').find(i => str(i, 'id') === id);
        return item ? wholeCount(item, 'quantity', 0) : 0;
    }

    tradeItem(id: string): Json | null {
        if (id !== 'herbs' && id !== 'meal') return null;
        const merchant = obj(this.snapshot, 'merchant');
        if (!str(merchant, 'id')) return null;
        return objects(merchant, 'items').find(i => str(i, 'id') === id) ?? null;
    }

    canTradeItem(id: string, buy: boolean): boolean {
        const item = this.tradeItem(id), merchant = obj(this.snapshot, 'merchant');
        const price = wholeCount(item, buy ? 'buyPrice' : 'sellPrice');
        return !!item && explicitTrue(item, buy ? 'canBuy' : 'canSell') && price > 0 &&
            wholeCount(item, buy ? 'stock' : 'owned') > 0 && wholeCount(buy ? obj(this.snapshot, 'self') : merchant, 'cash') >= price;
    }

    visibleResource(): Json | null {
        const resource = obj(this.snapshot, 'resource');
        if (!this.outdoors || str(resource, 'id') !== 'herb_patch' || wholeCount(resource, 'remaining') < 0) return null;
        for (const [key, limit] of [['x', this.cellWidth], ['y', this.cellHeight]] as const) {
            const v = resource![key];
            if (typeof v !== 'number' || !Number.isFinite(v) || v < 0 || v >= limit) return null;
        }
        return resource;
    }

    canGather(): boolean {
        const resource = this.visibleResource(), self = obj(this.snapshot, 'self');
        return !!resource && wholeCount(resource, 'remaining') > 0 && !!self &&
            Math.hypot(num(resource, 'x') - num(self, 'x', -1000), num(resource, 'y') - num(self, 'y', -1000)) <= 1.7;
    }

    openTrade(target: string) {
        const merchant = obj(this.snapshot, 'merchant');
        if (!target || str(merchant, 'id') !== target) {
            this.showToast('Trading requires the trader to be awake, visible, and nearby.');
            return;
        }
        if (this.chat) this.setChat(false);
        this.heldKeys.clear();
        this.sendMove();
        this.modal = 'trade';
        this.contextTarget = '';
    }

    heightAt(x: number, y: number): number {
        // Edges repeat their neighbour, so the rim of the cell never shades as a false slope.
        x = clamp(x, 0, this.cellWidth - 1);
        y = clamp(y, 0, this.cellHeight - 1);
        return this.tileHeights[y * this.cellWidth + x] ?? 0;
    }

    selfHeight(): number {
        const self = this.entities.get(this.selfId);
        return self ? this.heightAt(Math.floor(self.x), Math.floor(self.y)) : 0;
    }

    portraitAppearance(): Json | null {
        return obj(this.modal === 'inspect' || this.modal === 'their_equipment' ? this.inspectedCharacter : obj(this.snapshot, 'self'), 'appearance');
    }

    portraitAge(): number {
        if (this.modal !== 'inspect' && this.modal !== 'their_equipment') return num(obj(this.snapshot, 'self'), 'age', 18);
        // Inspecting another shows an age band, never their exact age.
        const stage = str(this.inspectedCharacter, 'lifeStage', 'adult');
        return stage === 'young' ? 6 : stage === 'adolescent' ? 13 : stage === 'old' ? 65 : 18;
    }
}
