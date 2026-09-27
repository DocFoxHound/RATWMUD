// Types for the pure authoring model in model.mjs (kept as plain JS so node --test runs it directly).

import type {TerrainTile, TerrainCategory} from './terrain.generated.mjs';
export type {TerrainTile, TerrainCategory, TerrainCategoryId, TerrainKind} from './terrain.generated.mjs';

/** A stored terrain code: one ASCII character from the terrain catalog (validated with isTerrainCode). */
export type Glyph = string;
export const TERRAIN: readonly TerrainTile[];
export const TERRAIN_CATEGORIES: readonly TerrainCategory[];
export const TERRAIN_BY_CODE: ReadonlyMap<string, TerrainTile>;
export const TERRAIN_CODES: readonly string[];
export function isTerrainCode(code: unknown): code is Glyph;
export function terrainTile(code: string | null | undefined): TerrainTile | undefined;
export function isRamp(code: string | null | undefined): boolean;
/** How the edge between two cardinally adjacent tiles reads (see edgeKind). */
export type EdgeKind = 'flat' | 'step' | 'slope' | 'ledge' | 'solid';
export interface Place { cell: string; x: number; y: number }
export interface Lighting { artificial: number; daylightAccess: number; tone: 'warm' | 'neutral' | 'cool' }
export interface Territory { region: string; claims: string[]; chapter: string }
export type Weather = 'clear' | 'overcast' | 'rain' | 'storm' | 'fog' | 'snow' | 'sandstorm';
export const WEATHERS: Weather[];

interface CellBase {
    id: string; name: string; description: string; width: number; height: number; z: number;
    outdoors: boolean; weather: Weather; lighting: Lighting; territory: Territory;
}
/** A world cell sits at x, y in world tiles and holds its own ground (cell-local rows and heights). */
export interface WorldCell extends CellBase { x: number; y: number; terrain: string[]; heights: Record<string, number> }
export interface Room extends CellBase { terrain: string[]; heights: Record<string, number>; worldX: number; worldY: number }
export type AnyCell = WorldCell | Room;

export type LinkKind = 'door' | 'passage' | 'stairs';
export interface Link { id: string; name: string; kind: LinkKind; a: Place; b: Place; open: boolean }

export type Role = 'merchant' | 'guard' | 'civilian';
export interface Appearance {
    species: 'timber' | 'maned' | 'arctic' | 'red' | 'ethiopian'; sex: 'female' | 'male';
    stature: 'short' | 'average' | 'tall'; pattern: 'solid' | 'saddle' | 'mantle' | 'piebald';
    baseColor: number; gradientColor: number; markingColor: number;
}
export interface Person {
    id: string; name: string; role: Role; description: string; greeting: string; workLabel: string;
    age: number; appearance: Appearance; voice: number; hours: {start: number; end: number};
    route: string; paid: boolean; purse: number; herbs: number; meals: number;
    home: Place; work: Place; evening: Place;
    personality: string; backstory: string;
}
/** A job placed in the world; the shared roster fills it at export. */
export interface Slot {
    id: string; name: string; profession: string; workLabel: string; hours: {start: number; end: number};
    route: string; paid: boolean; purse: number; herbs: number; meals: number;
    home: Place; work: Place; evening: Place;
}
export interface Route { id: string; name: string; posts: Place[] }
export interface Economy { treasury: number; storeHerbs: number; storeMeals: number; dailyHerbs: number; dailyMeals: number }
export interface Faction { id: string; name: string; color: string }
export interface Chapter { id: string; name: string }

export interface Project {
    /** Atlas v3: no shared canvas; the world is the ground its cells hold, anywhere within WORLD_REACH of 0,0. */
    format: 'ratw-atlas'; version: 3; id: string; name: string;
    factions: Faction[]; chapters: Chapter[];
    cells: WorldCell[]; rooms: Room[]; links: Link[]; spawn: Place | null;
    people: Person[]; slots: Slot[]; routes: Route[]; economy: Economy; herbPatch: Place | null;
}

export type PlaceRef =
    | {kind: 'spawn'} | {kind: 'herbPatch'}
    | {kind: 'person'; id: string; slot: 'home' | 'work' | 'evening'}
    | {kind: 'slot'; id: string; slot: 'home' | 'work' | 'evening'}
    | {kind: 'post'; id: string; index: number};

export interface BuildingTemplate {
    id: string; name: string; footprint: [number, number]; open: boolean; description: string;
    lighting: Lighting; interior: string[]; beds: [number, number][]; counter?: [number, number];
}
export interface PlacedBuilding { roomId: string; linkId: string; beds: Place[]; counter: Place | null; front: Place }

export const ROLES: Role[];
export const SPECIES: Appearance['species'][];
export const SEXES: Appearance['sex'][];
export const STATURES: Appearance['stature'][];
export const PATTERNS: Appearance['pattern'][];
export const COATS: [string, string][];
export function defaultEconomy(): Economy;
export function defaultTerritory(): Territory;

export function clone<T>(value: T): T;
export function createProject(width?: number, height?: number, name?: string): Project;
export function normalizeProject(value: unknown, tolerate?: Set<string> | true | null): Project;
export const WORLD_REACH: number;
export interface Bounds { x: number; y: number; width: number; height: number }
/** The rectangle, in world tiles, that holds every world cell. */
export function cellBounds(project: Project): Bounds | null;
export function isInterior(cell: AnyCell | null): cell is Room;
/** Finds world cells by world tile without scanning them all. */
export function cellIndex(cells: WorldCell[]): {at(x: number, y: number): WorldCell | null; around(x: number, y: number, w: number, h: number): WorldCell[]};
export function removeCell(project: Project, id: string): Project;
export function addCell(project: Project, rect: {x: number; y: number; width: number; height: number}, name?: string): WorldCell;
export function validate(value: unknown): {errors: string[]; warnings: string[]};
export function getCell(project: Project, id: string): AnyCell | null;
export function cellTerrain(project: Project, id: string): string[];

export function paint(project: Project, x: number, y: number, glyph: string, size?: number, roomId?: string | null): Project;
export function paintTiles(project: Project, tiles: [number, number][], glyph: string, roomId?: string | null): Project;
export function setHeight(project: Project, x: number, y: number, value: number | null, roomId?: string | null): Project;
export function setHeights(project: Project, tiles: [number, number][], value: number | null, roomId?: string | null): Project;
export function adjustHeights(project: Project, tiles: [number, number][], delta: number, roomId?: string | null): Project;
export const MIN_HEIGHT: number;
export const MAX_HEIGHT: number;
/** The height a glyph stands at without an override: +0.5 for `^` Stairs, 0 otherwise. */
export function defaultHeight(glyph: string | null | undefined): number;
/** The nearest half step, within -16 to 16. */
export function roundHeight(h: number): number;
export function isSolid(glyph: string): boolean;
export function edgeKind(glyphA: string | null | undefined, heightA: number, glyphB: string | null | undefined, heightB: number): EdgeKind;
export function canStep(glyphA: string | null | undefined, heightA: number, glyphB: string | null | undefined, heightB: number): boolean;
export function stamp(project: Project, x: number, y: number, rows: string[], roomId?: string | null): Project;

export function cutGrid(project: Project, width?: number, height?: number): WorldCell[];
export function splitCell(project: Project, id: string, axis: 'x' | 'y', offset: number): AnyCell[];
export function mergeCells(project: Project, ids: string[]): AnyCell;
export function addRoom(project: Project, width?: number, height?: number, name?: string): Room;
export function updateCell(project: Project, id: string, fields: Partial<Room>): AnyCell;
export function resizeRoom(project: Project, id: string, width: number, height: number): AnyCell;
export function removeRoom(project: Project, id: string): Project;

export function addLink(project: Project, link: Partial<Link> & {a: Place; b: Place}): Link;
export function updateLink(project: Project, id: string, fields: Partial<Link>): Link;
export function removeLink(project: Project, id: string): Project;
export function setSpawn(project: Project, place: Place | null): Project;

export function upsertFaction(project: Project, faction: Faction): Faction;
export function upsertChapter(project: Project, chapter: Chapter): Chapter;
export function removeFaction(project: Project, id: string): Project;
export function removeChapter(project: Project, id: string): Project;
export function setTerritory(project: Project, ids: string[], territory: Territory): Project;

export function newPerson(project: Project, role?: Role, place?: Place | null, name?: string | null): Person;
export function upsertPerson(project: Project, person: Person): Person;
export function removePerson(project: Project, id: string): Project;
export function upsertRoute(project: Project, route: Partial<Route>): Route;
export function removeRoute(project: Project, id: string): Project;
export function setEconomy(project: Project, economy: Partial<Economy>): Economy;
export function setHerbPatch(project: Project, place: Place | null): Project;
export function movePlace(project: Project, ref: PlaceRef, place: Place): Project;
export function worldIdFor(name: string): string;
export function newSlot(project: Project, profession: {id: string; name: string; hours?: {start: number; end: number}; paid?: boolean}, place?: Place | null): Slot;
export function upsertSlot(project: Project, slot: Slot): Slot;
export function removeSlot(project: Project, id: string): Project;
export function addBuilding(project: Project, template: BuildingTemplate, cellId: string, x: number, y: number,
    side?: 'S' | 'N', name?: string | null): PlacedBuilding;
