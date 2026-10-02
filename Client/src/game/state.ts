// The game screen's state and rules: SRatwGame (UI/SRatwGame.cpp) without its drawing. The client is a projection of
// observer-filtered server data; no simulation lives here. paint.ts draws it; view.ts connects it to the page.
import {arr, bool, boundedNum, clamp, envNumber, explicitTrue, num, obj, objects, str, wholeCount, type Json} from './json.ts';
import {heightFromChar, type EnvironmentView, type ScentCue} from './labels.ts';
import {MotionBuffer} from './motionBuffer.ts';
import {contains, rect, type Rect} from '../ui/painter.ts';
import type {MotionFrame} from '../net/motion.ts';

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
    appearance: Json | null;
    lifeStage: string;
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
    contextTarget = '';
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
    talkTargets: string[] = []; // Whom the player is speaking to (up to four), until they leave sight or are let go.
    // The regional weather over the cell (doc 29, phase 7): a letter (kind) and a digit (strength) every `step` tiles.
    weatherField: {cols: number; rows: number; step: number; kinds: string; amounts: string} | null = null;
    worldZoom = -1;             // The World Map's scale (minimap.ts MapScales; -1 fits the known places), and its pan (tiles).
    worldPan: [number, number] = [0, 0];
    miniZoom = 3;               // The minimap's scale (2 pixels a tile).
    inspectedText = '';
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
        }
        const seenSource = arr(s, 'visibility');
        if (seenSource !== this.sources.visibility) {
            this.sources.visibility = seenSource;
            this.visibilityRows = strings(seenSource);
        }
        const heights = arr(cell, 'heights');
        if (heights !== this.sources.heights || this.tileHeights.length !== this.cellWidth * this.cellHeight) {
            this.sources.heights = heights;
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
                    appearance: null, lifeStage: 'adult'};
                this.entities.set(id, view);
            }
            view.name = str(e, 'name');
            view.kind = str(e, 'kind', bool(e, 'npc') ? 'npc' : 'player');
            view.state = str(e, 'state', str(e, 'posture', 'standing'));
            view.work = str(e, 'work');
            view.hostile = bool(e, 'hostile');
            view.appearance = obj(e, 'appearance');
            view.lifeStage = str(e, 'lifeStage', 'adult');
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
        this.latestMotionTime = frame.time;
        this.observeMotionTime(frame.time);
        this.motionVisible.clear();
        for (const pose of frame.entities) {
            this.motionVisible.add(pose.id);
            // Metadata is observer-filtered too; poses never invent actors.
            const view = this.entities.get(pose.id);
            if (view) {
                this.applyPose(view, pose as unknown as Json, frame.time);
                view.moving = pose.moving;
            }
        }
        for (const id of [...this.entities.keys()]) if (!this.motionVisible.has(id)) this.entities.delete(id);
        this.movementPending = false;
    }

    receiveEvent(e: Json) {
        const type = str(e, 'type', 'system');
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
        if (type === 'talkTarget') {
            this.addTarget(str(e, 'id'));
            return;
        }
        const post: Post = {
            to: arr(e, 'to').filter((v): v is string => typeof v === 'string').slice(0, 6),
            id: eventId, channel: str(e, 'channel', type === 'ooc' ? 'ooc' : 'ic'), kind: type,
            speaker: str(e, 'speaker', str(e, 'name', type === 'system' ? 'THE WORLD' : 'A voice')),
            color: Math.trunc(num(e, 'color')), text: str(e, 'text'), postedAt: this.clock, revealed: 0, system: false,
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
        post.system = type === 'system' || type === 'error';
        post.revealed = post.system || post.channel === 'ooc' ? post.text.length : 0;
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
            // The player's own wolf answers at once: its newest pose, carried a little ahead while it is moving, and
            // eased toward rather than jumped to, so jitter in arrival never shows. A real jump (a door, a correction
            // of more than two tiles) is taken at once.
            const moving = view.moving || this.heldKeys.size > 0;
            const pose = view.motion.at(now - 0.03, moving ? 0.12 : 0.05);
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
        if (!this.chat && this.heldKeys.size && this.clock - this.lastMove > 0.075) this.sendMove();
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

    // ------------------------------------------------------------------ Sending

    sendAction(action: string, target = '') {
        this.send({type: 'action', action, target});
    }

    showToast(text: string) {
        this.toast = text;
        this.toastUntil = this.clock + 5;
    }

    setTyping(active: boolean) {
        active = active && this.channel === 'ic';
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

    submitPost() {
        const text = this.composer.text.trim();
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
        if (this.channel === 'ic') this.setTyping(true);
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
        if (!this.chat && (x !== 0 || y !== 0)) {
            this.movementPending = true;
            this.facingPreview = false;
            this.mapPan = [0, 0];                // Looking around ends when the wolf moves: the map follows it again.
        }
        this.send({type: 'move', x: this.chat ? 0 : x, y: this.chat ? 0 : y});
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
        if (code === 'Escape') {
            this.facingPreview = false;
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
                this.sendAction(this.contextActions[+choice[1] - 1], this.contextTarget);
                this.contextTarget = '';
                return true;
            }
        }
        if (code === 'KeyE') {
            this.targetNearest();
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
    }

    mouseLeave() {
        this.facingPreview = false;
        this.hover = [-1000, -1000];
    }

    /** The wheel: positive is up (away from the player). True when the game took it. */
    wheel(point: [number, number], delta: number, shift: boolean, ctrl: boolean): boolean {
        this.facingPreview = false;
        if (this.modal) return false;
        if (contains(this.mapRect, point[0], point[1]) && !this.worldMap && !this.chat) {
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
        } else if (a === 'ic' || a === 'ooc') {
            this.setTyping(false);
            this.channel = a;
            this.transcriptScroll = 0;
        } else if (a === 'character' || a === 'inventory' || a === 'settings') {
            if (this.chat) this.setChat(false);
            this.heldKeys.clear();
            this.sendMove();
            this.modal = a;
            this.contextTarget = '';
        } else if (a === 'close') this.modal = '';
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
        } else if (a === 'context') {
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
        return obj(this.modal === 'inspect' ? this.inspectedCharacter : obj(this.snapshot, 'self'), 'appearance');
    }

    portraitAge(): number {
        if (this.modal !== 'inspect') return num(obj(this.snapshot, 'self'), 'age', 18);
        // Inspecting another shows an age band, never their exact age.
        const stage = str(this.inspectedCharacter, 'lifeStage', 'adult');
        return stage === 'young' ? 6 : stage === 'adolescent' ? 13 : stage === 'old' ? 65 : 18;
    }
}
